/*
 * Copyright 2017 Google Inc.
 * Author: Joe Richey (joerichey@google.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "sha512.h"
#include "fs_mgr_priv.h"
#include "fs_mgr_priv_fscrypt.h"

// Some of the necessary structures, constants, and functions are declared in
// <linux/fs.h> or <keyutils.h> but are either incomplete (depending on your
// kernel version) or require an external library. So to simplify things, they
// are just redeclared here.

/* Begin <linux/fs.h> */
#define FS_MAX_KEY_SIZE 64

struct fscrypt_key {
	uint32_t mode;
	uint8_t raw[FS_MAX_KEY_SIZE];
	uint32_t size;
} __attribute__((packed));

#define FS_KEY_DESCRIPTOR_SIZE 8
#define FS_KEY_DESCRIPTOR_HEX_SIZE ((2 * FS_KEY_DESCRIPTOR_SIZE) + 1)


// Service prefixes for encryption keys
#define FS_KEY_DESC_PREFIX "fscrypt:"
#define MAX_KEY_DESC_PREFIX_SIZE 8
/* End <linux/fs.h> */

/* Begin <keyutils.h> */
typedef int32_t key_serial_t;
#define KEYCTL_GET_KEYRING_ID 0     /* ask for a keyring's ID */
#define KEY_SPEC_SESSION_KEYRING -3 /* current session keyring */

static key_serial_t add_key(const char *type, const char *description,
			const void *payload, size_t plen, key_serial_t ringid)
{
	return syscall(__NR_add_key, type, description, payload, plen, ringid);
}

static key_serial_t keyctl_get_keyring_ID(key_serial_t id, int create)
{
	return syscall(__NR_keyctl, KEYCTL_GET_KEYRING_ID, id, create);
}
/* End <keyutils.h> */


// Takes an input key descriptor as a byte array and outputs a hex string.
static void key_descriptor_to_hex(const uint8_t bytes[FS_KEY_DESCRIPTOR_SIZE],
			char hex[FS_KEY_DESCRIPTOR_HEX_SIZE])
{
	int i;

	for (i = 0; i < FS_KEY_DESCRIPTOR_SIZE; ++i)
		sprintf(hex + 2 * i, "%02x", bytes[i]);
}

// Reads key data from file into the provided data buffer. Return 0 on success.
static int read_key(uint8_t key[FS_MAX_KEY_SIZE], FILE *fp)
{
	size_t rc = fread(key, 1, FS_MAX_KEY_SIZE, fp);
	int end = fgetc(fp);

	// We should read exactly FS_MAX_KEY_SIZE bytes, then hit EOF
	if (rc == FS_MAX_KEY_SIZE && end == EOF && feof(fp))
		return 0;

	ERROR("error: input key must be %d bytes\n", FS_MAX_KEY_SIZE);
	return -1;
}

// The descriptor is just the first 8 bytes of a double application of SHA512
// formatted as hex (so 16 characters).
static void compute_descriptor(const uint8_t key[FS_MAX_KEY_SIZE],
			char descriptor[FS_KEY_DESCRIPTOR_HEX_SIZE])
{
	uint8_t digest1[SHA512_DIGEST_LENGTH];
	uint8_t digest2[SHA512_DIGEST_LENGTH];

	SHA512(key, FS_MAX_KEY_SIZE, digest1);
	SHA512(digest1, SHA512_DIGEST_LENGTH, digest2);

	key_descriptor_to_hex(digest2, descriptor);
	secure_wipe(digest1, SHA512_DIGEST_LENGTH);
	secure_wipe(digest2, SHA512_DIGEST_LENGTH);
}

// Inserts the key into the current session keyring with type logon and the
// service specified by service_prefix.
static int insert_logon_key(const uint8_t key_data[FS_MAX_KEY_SIZE],
			const char descriptor[FS_KEY_DESCRIPTOR_HEX_SIZE],
			const char *service_prefix)
{
	// We cannot add directly to KEY_SPEC_SESSION_KEYRING,
	// as that will make a new session keyring if one does not exist,
	// rather than adding it to the user session keyring.
	char description[MAX_KEY_DESC_PREFIX_SIZE + FS_KEY_DESCRIPTOR_HEX_SIZE];
	int keyring_id = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 0);
	struct fscrypt_key key = {.mode = 0, .size = FS_MAX_KEY_SIZE};
	int ret;

	if (keyring_id < 0)
		return -1;

	sprintf(description, "%s%s", service_prefix, descriptor);

	memcpy(key.raw, key_data, FS_MAX_KEY_SIZE);

	ret = add_key("logon", description, &key, sizeof(key), keyring_id)
		< 0 ? -1 : 0;

	secure_wipe(key.raw, FS_MAX_KEY_SIZE);
	return ret;
}


int fs_mgr_setup_fscrypt(struct fstab_rec *fsrec)
{
	int ret = FS_MGR_SETUP_FSCRYPT_SUCCESS;
	const char *service_prefix = FS_KEY_DESC_PREFIX;
	FILE *fp = NULL;
	uint8_t key[FS_MAX_KEY_SIZE];
	char descriptor[FS_KEY_DESCRIPTOR_HEX_SIZE];

	if (!fsrec->key_loc)
		goto success;

	fp = fopen(fsrec->key_loc, "rb");
	if (!fp) {
		ERROR("fail to open keyfile: %s\n", fsrec->key_loc);
		return FS_MGR_SETUP_FSCRYPT_FAIL;
	}

	if (read_key(key, fp)) {
		ret = FS_MGR_SETUP_FSCRYPT_FAIL;
		goto out;
	}

	compute_descriptor(key, descriptor);
	if (insert_logon_key(key, descriptor, service_prefix)) {
		ERROR("error: inserting key: %s\n", strerror(errno));
		ret = FS_MGR_SETUP_FSCRYPT_FAIL;
		goto out;
	}
	puts(descriptor);

success:
	INFO("Enabling fscrypt for %s\n", fsrec->mount_point);

	ret = FS_MGR_SETUP_FSCRYPT_SUCCESS;
out:
	secure_wipe(key, FS_MAX_KEY_SIZE);
	return ret;
}
