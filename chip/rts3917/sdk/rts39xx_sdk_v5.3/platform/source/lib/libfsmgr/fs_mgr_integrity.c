#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fs_mgr_priv.h"
#include "fs_mgr_priv_integrity.h"

#define INTEGRITYSETUP_BIN	"/usr/sbin/integritysetup"
#define MKFS_EXT4_BIN		"/usr/sbin/mkfs.ext4"

#define SB_MAGIC	"integrt"
#define SB_VERSION_1	1
#define SB_VERSION_2	2
#define SB_VERSION_3	3
#define SB_VERSION_4	4
#define SB_VERSION_5	5

struct integrity_sb {
	uint8_t magic[8];               /* "integrt" */
	uint8_t version;                /* superblock version, 1,2,3,4 or 5 */
	int8_t log2_interleave_sectors; /* interleave sectors */
	uint16_t integrity_tag_size;    /* tag size per-sector */
	uint32_t journal_sections;      /* size of journal */
	uint64_t provided_data_sectors; /* available size for data */
	uint32_t flags;                 /* flags */
	uint8_t log2_sectors_per_block; /* presented block (sector) size */
	uint8_t log2_blocks_per_bitmap_bit; /* blocks per bitmap, V3 only */
	uint8_t pad[2];                 /* (padding) */
	uint64_t recalc_sector;         /* recalculate sector, V2 only */
} __attribute__ ((packed));

static int get_integrity_superblock(char *blk_device, struct integrity_sb *sb)
{
	int data_device;

	data_device = open(blk_device, O_RDONLY | O_CLOEXEC);
	if (data_device == -1) {
		ERROR("Error opening block device (%s)", strerror(errno));
		return -1;
	}

	if (read(data_device, sb, sizeof(*sb)) != sizeof(*sb)) {
		ERROR("Error reading superblock");
		close(data_device);
		return -1;
	}

	close(data_device);

	return 0;
}

static int is_integrity_formatted(char *device)
{
	int ret = 0;
	struct integrity_sb sb;

	ret = get_integrity_superblock(device, &sb);
	if (ret < 0) {
		ret = 0;
		goto out;
	}

	if (!memcmp(sb.magic, SB_MAGIC, sizeof(sb.magic)) &&
		sb.version >= SB_VERSION_1 && sb.version <= SB_VERSION_5) {
		ret = 1;
	}
out:
	return ret;
}

static int is_integrity_opened(char *device)
{
	int tries = 1;

	while (tries--) {
		if (!access(device, F_OK) || errno != ENOENT)
			return 1;

		usleep(40 * 1000);
	}
	return 0;
}

int fs_mgr_setup_integrity(struct fstab_rec *fsrec)
{
	int ret = FS_MGR_SETUP_INTEGRITY_SUCCESS;
	char *cmd = NULL;
	char *integrity_blk_name = NULL;
	char *mount_point = basename(fsrec->mount_point);
	int is_format = 0;
	int is_open = 0;

	/* get dm device */
	ret = asprintf(&integrity_blk_name, "/dev/mapper/dm-integrity-%s",
				!strcmp(mount_point, "/") ?
				"root" : mount_point);
	if (ret < 0) {
		ERROR("Error getting device name (%s)", strerror(errno));
		ret = FS_MGR_SETUP_INTEGRITY_FAIL;
		goto out;
	}

	is_format = is_integrity_formatted(fsrec->blk_device);

	if (is_format == 0) {
		/* integritysetup format */
		ret = asprintf(&cmd, "%s format -q %s",
				INTEGRITYSETUP_BIN, fsrec->blk_device);
		INFO("cmd: %s\n", cmd);
		if (ret < 0) {
			ERROR("Error getting cmd (%s)", strerror(errno));
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		ret = execle_cmd(cmd);
		if (ret != 0) {
			ERROR("execle cmd: '%s' failed %d\n", cmd, ret);
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		SAFE_FREE(cmd);
	}

	is_open = is_integrity_opened(integrity_blk_name);

	if (is_open == 0) {
		/* integritysetup open */
		ret = asprintf(&cmd, "%s open %s %s",
				INTEGRITYSETUP_BIN, fsrec->blk_device,
				basename(integrity_blk_name));
		INFO("cmd: %s\n", cmd);
		if (ret < 0) {
			ERROR("Error getting cmd (%s)", strerror(errno));
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		ret = execle_cmd(cmd);
		if (ret != 0) {
			ERROR("execle cmd: '%s' failed %d\n", cmd, ret);
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		SAFE_FREE(cmd);
	}

	if (is_format == 0) {
		/* mkfs */
		ret = asprintf(&cmd, "%s -F -q %s",
				MKFS_EXT4_BIN, integrity_blk_name);
		INFO("cmd: %s\n", cmd);
		if (ret < 0) {
			ERROR("Error getting cmd (%s)", strerror(errno));
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		ret = execle_cmd(cmd);
		if (ret != 0) {
			ERROR("execle cmd: '%s' failed %d\n", cmd, ret);
			ret = FS_MGR_SETUP_INTEGRITY_FAIL;
			goto out;
		}
		SAFE_FREE(cmd);
	}

	INFO("Enabling dm-integrity for %s\n",  mount_point);

	/* assign the new integrity block device as the block device */
	SAFE_FREE(fsrec->blk_device);
	fsrec->blk_device = integrity_blk_name;
	integrity_blk_name = NULL;

	ret = FS_MGR_SETUP_INTEGRITY_SUCCESS;
out:
	SAFE_FREE(integrity_blk_name);
	SAFE_FREE(cmd);

	return ret;
}
