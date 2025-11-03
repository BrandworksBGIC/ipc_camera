#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

#include "fs_mgr_priv.h"
#include "fs_mgr_priv_crypt.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define DMSETUP_BIN		"/usr/sbin/dmsetup"
#define START_SECTOR		0
#define DEV_SECTOR_SIZE(s)		((s) / 512)
#define TARGET_NAME		"crypt"
#define CIPHER_MODE_IV		"aes-cbc-plain64"
#define IV_OFFSET		0
#define ENCRYPTED_OFFSET	0
#define OPTIONAL_PARAMS_NUMS	2
#define OPTIONAL_PARAMS		"sector_size:4096 iv_large_sectors"

#define AES_KEY_MAX_BYTES 32

int fs_mgr_setup_crypt(struct fstab_rec *fsrec)
{
	int ret = FS_MGR_SETUP_CRYPT_SUCCESS;
	char *cmd = NULL, *crypt_blk_name = NULL;
	char *mount_point = basename(fsrec->mount_point);
	FILE *fp = NULL;
	int len = AES_KEY_MAX_BYTES;
	char key[2 * AES_KEY_MAX_BYTES + 1], *tmp = NULL;
	int ch;
	uint64_t size;

	if (!fsrec->key_loc)
		return FS_MGR_SETUP_CRYPT_FAIL;

	fp = fopen(fsrec->key_loc, "rb");
	if (!fp) {
		ERROR("fail to open keyfile: %s\n", fsrec->key_loc);
		return FS_MGR_SETUP_CRYPT_FAIL;
	}

	tmp = key;
	while ((ch = fgetc(fp)) != EOF) {
		if (!len) {
			ERROR("AES key length exceeds maximum : %d\n",
						AES_KEY_MAX_BYTES);
			return FS_MGR_SETUP_CRYPT_FAIL;
		}
		--len;

		sprintf(tmp, "%02X", ch);
		tmp += 2;
	}
	tmp = '\0';
	DEBUG("key=0x%s\n", key);

	ret = asprintf(&crypt_blk_name, "/dev/mapper/dm-crypt-%s",
				!strcmp(mount_point, "/") ?
				"root" : mount_point);
	if (ret < 0) {
		ERROR("Error getting crypt block device name (%s)",
					strerror(errno));
		ret = FS_MGR_SETUP_CRYPT_FAIL;
		goto out;
	}

	ret = fs_mgr_get_blk_size(fsrec->blk_device, &size);
	if (ret < 0) {
		ret = FS_MGR_SETUP_CRYPT_FAIL;
		goto out;
	}

	ret = asprintf(&cmd,
			"%s create dm-crypt-%s --table \"%d %llu %s %s %s %d %s %d %d %s\"",
			DMSETUP_BIN, mount_point,
			START_SECTOR, DEV_SECTOR_SIZE(size), TARGET_NAME,
			CIPHER_MODE_IV, key, IV_OFFSET,
			fsrec->blk_device, ENCRYPTED_OFFSET,
			OPTIONAL_PARAMS_NUMS, OPTIONAL_PARAMS);
	DEBUG("cmd: %s\n", cmd);
	if (ret < 0) {
		ERROR("Error getting crypt cmd (%s)", strerror(errno));
		ret = FS_MGR_SETUP_CRYPT_FAIL;
		goto out;
	}

	ret = execle_cmd(cmd);
	if (ret != 0) {
		ERROR("execle cmd: '%s' failed, status=%d\n", cmd, ret);
		ret = FS_MGR_SETUP_CRYPT_FAIL;
		goto out;
	}

	INFO("Enabling dm-crypt for %s\n",  fsrec->mount_point);

	// assign the new crypt block device as the block device
	SAFE_FREE(fsrec->blk_device);
	fsrec->blk_device = crypt_blk_name;
	crypt_blk_name = NULL;

	ret = FS_MGR_SETUP_CRYPT_SUCCESS;
out:
	SAFE_FREE(crypt_blk_name);
	SAFE_FREE(cmd);

	return ret;
}
