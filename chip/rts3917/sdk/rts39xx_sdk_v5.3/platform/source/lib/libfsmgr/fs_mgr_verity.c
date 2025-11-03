/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>

#include <kcapi.h>

#include "ext4_utils/ext4_sb.h"
#include "squashfs_utils/squashfs_utils.h"

#include "fs_mgr_priv.h"
#include "fs_mgr_priv_verity.h"
#include "fs_mgr.h"

#define VERITY_METADATA_SIZE 32768

#define RSA_BITS 2048
#define RSA_BYTES (RSA_BITS >> 3)
#define KEYFILE_MAX_BYTES 1024
#define HASH_BYTES 32

#define UBIFS_AUTH_KEY_MAX_BYTES 32

static int load_keyfile(const char *path, unsigned char *key)
{
	FILE *fp;
	int ret = 0;

	if (!path || !key) {
		ERROR("path or key point is NULL\n");
		return -1;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		ERROR("fail to open keyfile: %s\n", path);
		return -1;
	}

	ret = fread(key, 1, KEYFILE_MAX_BYTES, fp);
	if (ret <= 0) {
		ERROR("Invalid keyfile length %d bytes\n", ret);
        	fclose(fp);
		return -1;
	}

	fclose(fp);
	return ret;
}

static int verify_table(char *signature, char *table, int table_length, const char *key_path)
{
	unsigned char *hash = NULL;
	unsigned char key[KEYFILE_MAX_BYTES];
	struct kcapi_handle *handle = NULL;
        int ret = -1;

	if (!signature || !table || ! key_path) {
		ERROR("null pointer!\n");
		return -1;
	}

	hash = (unsigned char *)signature + RSA_BYTES;
	ret = kcapi_md_sha256((uint8_t *)table, table_length,
				hash, HASH_BYTES);
	if (ret != HASH_BYTES) {
                ERROR("calculate hash of table is failed!\n");
                return -1;
        }

	ret = kcapi_akcipher_init(&handle, "pkcs1pad(rsa,sha256)", 0);
        if (ret != 0) {
                ERROR("kcapi akcipher init fail\n");
                return -1;
        }

        // Now get the public key from the keyfile (DER format)
        ret = load_keyfile(key_path, key);
        if (ret <= 0) {
                ERROR("Couldn't load verity keys\n");
                return -1;
        }

	ret = kcapi_akcipher_setpubkey(handle, key, ret);
        if (ret < 0) {
                ERROR("kcapi akcipher setpubkey fail, ret=%d\n", ret);
		ret = -1;
                goto out;
        }

	ret = kcapi_akcipher_verify(handle,
				(uint8_t *)signature, RSA_BYTES + HASH_BYTES,
				(uint8_t *)signature, HASH_BYTES,
				KCAPI_ACCESS_SENDMSG);
        if (ret < 0) {
                ERROR("kcapi akcipher verify fail,ret=%d\n", ret);
		ret = -1;
                goto out;
        }

        ret = 0;
out:
	kcapi_akcipher_destroy(handle);
        return ret;
}

static inline int load_table(const char *table)
{
	return execle_cmd(table);
}

static int squashfs_get_target_device_size(char *blk_device, uint64_t *device_size)
{
        struct squashfs_info sq_info;

        if (squashfs_parse_sb(blk_device, &sq_info) >= 0) {
                *device_size = sq_info.bytes_used_4K_padded;
                return 0;
        } else {
                return -1;
        }
}

static int ext4_get_target_device_size(char *blk_device, uint64_t *device_size)
{
        int data_device;
        struct ext4_super_block sb;
        struct fs_info info;

        info.len = 0;  /* Only len is set to 0 to ask the device for real size. */
        data_device = TEMP_FAILURE_RETRY(open(blk_device, O_RDONLY | O_CLOEXEC));
        if (data_device == -1) {
                ERROR("Error opening block device (%s)", strerror(errno));
                return -1;
        }

        if (TEMP_FAILURE_RETRY(lseek64(data_device, 1024, SEEK_SET)) < 0) {
                ERROR("Error seeking to superblock");
                close(data_device);
                return -1;
        }

        if (TEMP_FAILURE_RETRY(read(data_device, &sb, sizeof(sb))) != sizeof(sb)) {
                ERROR("Error reading superblock");
                close(data_device);
                return -1;
        }

        ext4_parse_sb(&sb, &info);
        *device_size = info.len;

        close(data_device);
        return 0;
}

int get_fs_size(char *fs_type, char *blk_device, uint64_t *device_size)
{
        if (!strcmp(fs_type, "ext4")) {
                if (ext4_get_target_device_size(blk_device, device_size) < 0) {
                        ERROR("Failed to get ext4 fs size on %s.", blk_device);
                        return -1;
                }
        } else if (!strcmp(fs_type, "squashfs")) {
                if (squashfs_get_target_device_size(blk_device, device_size) < 0) {
                        ERROR("Failed to get squashfs fs size on %s.", blk_device);
                        return -1;
                }
        } else {
                ERROR("%s: Unsupported filesystem for verity.", fs_type);
                return -1;
        }
        return 0;
}

static int read_verity_metadata(uint64_t device_size, char *block_device, char **signature,
                char **table)
{
        unsigned magic_number;
        unsigned table_length;
        int protocol_version;
        int device;
        int retval = FS_MGR_SETUP_VERITY_FAIL;

        *signature = NULL;

        if (table) {
                *table = NULL;
        }

        device = TEMP_FAILURE_RETRY(open(block_device, O_RDONLY | O_CLOEXEC));
        if (device == -1) {
                ERROR("Could not open block device %s (%s).\n", block_device, strerror(errno));
                goto out;
        }

        if (TEMP_FAILURE_RETRY(lseek64(device, device_size, SEEK_SET)) < 0) {
                ERROR("Could not seek to start of verity metadata block.\n");
                goto out;
        }

        // check the magic number
        if (TEMP_FAILURE_RETRY(read(device, &magic_number, sizeof(magic_number))) !=
                        sizeof(magic_number)) {
                ERROR("Couldn't read magic number!\n");
                goto out;
        }

        if (magic_number != VERITY_METADATA_MAGIC_NUMBER) {
                ERROR("Couldn't find verity metadata at offset %"PRIu64"!\n", device_size);
                goto out;
        }

        // check the protocol version
        if (TEMP_FAILURE_RETRY(read(device, &protocol_version,
                        sizeof(protocol_version))) != sizeof(protocol_version)) {
                ERROR("Couldn't read verity metadata protocol version!\n");
                goto out;
        }
        if (protocol_version != 0) {
                ERROR("Got unknown verity metadata protocol version %d!\n", protocol_version);
                goto out;
        }

        // get the signature
        *signature = (char*) malloc(RSA_BYTES + HASH_BYTES);
        if (!*signature) {
                ERROR("Couldn't allocate memory for signature!\n");
                goto out;
        }
        if (TEMP_FAILURE_RETRY(read(device, *signature, RSA_BYTES)) != RSA_BYTES) {
                ERROR("Couldn't read signature from verity metadata!\n");
                goto out;
        }

        if (!table) {
                retval = FS_MGR_SETUP_VERITY_SUCCESS;
                goto out;
        }

        // get the size of the table
        if (TEMP_FAILURE_RETRY(read(device, &table_length, sizeof(table_length))) !=
                        sizeof(table_length)) {
                ERROR("Couldn't get the size of the verity table from metadata!\n");
                goto out;
        }

        // get the table + null terminator
        *table = malloc(table_length + 1);
        if (!*table) {
                ERROR("Couldn't allocate memory for verity table!\n");
                goto out;
        }
        if (TEMP_FAILURE_RETRY(read(device, *table, table_length)) !=
                        (ssize_t)table_length) {
                ERROR("Couldn't read the verity table from metadata!\n");
                goto out;
        }

        (*table)[table_length] = 0;
        retval = FS_MGR_SETUP_VERITY_SUCCESS;

out:
        if (device != -1)
                close(device);

        if (retval != FS_MGR_SETUP_VERITY_SUCCESS) {
                free(*signature);
                *signature = NULL;

                if (table) {
                        free(*table);
                        *table = NULL;
                }
        }

        return retval;
}


static int test_access(char *device) {
        int tries = 25;
        while (tries--) {
                if (!access(device, F_OK) || errno != ENOENT) {
                        return 0;
                }
                usleep(40 * 1000);
        }
        return -1;
}

static int setup_ubifs_auth(struct fstab_rec *fsrec)
{
	int ret = FS_MGR_SETUP_VERITY_SUCCESS;
	char *cmd = NULL;
	FILE *fp = NULL;
	int len = UBIFS_AUTH_KEY_MAX_BYTES;
	char key[2 * UBIFS_AUTH_KEY_MAX_BYTES + 1], *tmp = NULL;
	int ch;

	if (!fsrec->verity_loc)
		goto success;

	fp = fopen(fsrec->verity_loc, "rb");
	if (!fp) {
		ERROR("fail to open keyfile: %s\n", fsrec->verity_loc);
		return FS_MGR_SETUP_VERITY_FAIL;
	}

	tmp = key;
	while ((ch = fgetc(fp)) != EOF) {
		if (!len) {
			ERROR("ubifs auth key length exceeds maximum : %d\n",
						UBIFS_AUTH_KEY_MAX_BYTES);
			ret = FS_MGR_SETUP_VERITY_FAIL;
			goto out;
		}
		--len;

		sprintf(tmp, "%02X", ch);
		tmp += 2;
	}
	tmp = '\0';
	DEBUG("key=0x%s\n", key);

	ret = asprintf(&cmd,
			"%s add -x logon ubifs:foo %s @us",
			"keyctl", key);
	DEBUG("cmd: %s\n", cmd);
	if (ret < 0) {
		ERROR("Error getting ubifs_auth cmd (%s)", strerror(errno));
		ret = FS_MGR_SETUP_VERITY_FAIL;
		goto out;
	}

	ret = execle_cmd(cmd);
	if (ret != 0) {
		ERROR("execle cmd: '%s' failed, status=%d\n", cmd, ret);
		ret = FS_MGR_SETUP_VERITY_FAIL;
		goto out;
	}
success:
	INFO("Enabling ubifs_auth for %s\n",  fsrec->mount_point);

	ret = FS_MGR_SETUP_VERITY_SUCCESS;
out:
	SAFE_FREE(cmd);
	if (fp)
		fclose(fp);

	return ret;
}

int fs_mgr_setup_verity(struct fstab_rec *fsrec) {

        int retval = FS_MGR_SETUP_VERITY_FAIL;

        char *verity_blk_name = 0;
        char *verity_table = 0;
        char *verity_table_signature = 0;
        int verity_table_length = 0;
        uint64_t device_size = 0;
        char *mount_point = basename(fsrec->mount_point);

	if (fsrec->fs_mgr_flags & MF_UBIFS_AUTH)
		return setup_ubifs_auth(fsrec);

        // get verity filesystem size
        if (get_fs_size(fsrec->fs_type, fsrec->blk_device, &device_size) < 0) {
                return retval;
        }
        DEBUG("device_size: %" PRId64 "\n", device_size);

        // read the verity block at the end of the block device
        // send error code up the chain so we can detect attempts to disable verity
        retval = read_verity_metadata(device_size,
                                                                  fsrec->blk_device,
                                                                  &verity_table_signature,
                                                                  &verity_table);
        if (retval < 0) {
                goto out;
        }

        retval = FS_MGR_SETUP_VERITY_FAIL;
        verity_table_length = strlen(verity_table);

        // get the name of the device file
        if (asprintf(&verity_blk_name, "/dev/mapper/dm-verity-%s",
				!strcmp(mount_point, "/") ? "root" : mount_point) < 0) {
                ERROR("Error getting verity block device name (%s)", strerror(errno));
		goto out;
        }

        // verify the signature on the table
        DEBUG("verity table: %s\n",  verity_table);
        DEBUG("verity table length: %d\n",  verity_table_length);
/*
	printf("verity signature: %d\n", RSA_BYTES);
	int i = 0;
	unsigned char * test = (unsigned char *)verity_table_signature;
	for (i = 0; i < RSA_BYTES; i++)
        	printf("%02x",  test[i]);
	printf("\n");
*/
        if (verify_table(verity_table_signature,
                                          verity_table,
                                          verity_table_length,
					  fsrec->verity_loc) < 0) {
                        goto out;
        }

        INFO("Enabling dm-verity for %s\n",  mount_point);

        // load the verity mapping table
	retval = load_table(verity_table);
	if (retval != 0) {
		ERROR("load table failed, status=%d\n", retval);
		retval = FS_MGR_SETUP_VERITY_FAIL;
		goto out;
	}

        // assign the new verity block device as the block device
        free(fsrec->blk_device);
        fsrec->blk_device = verity_blk_name;
        verity_blk_name = 0;

        // make sure we've set everything up properly
        if (test_access(fsrec->blk_device) < 0) {
                goto out;
        }

        retval = FS_MGR_SETUP_VERITY_SUCCESS;

out:
        free(verity_table);
        free(verity_table_signature);
        free(verity_blk_name);
	verity_table = NULL;
	verity_table_signature = NULL;
	verity_blk_name = NULL;

        return retval;
}
