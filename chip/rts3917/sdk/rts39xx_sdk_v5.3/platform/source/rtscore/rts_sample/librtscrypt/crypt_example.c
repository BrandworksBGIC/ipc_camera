/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rts_crypto.h"

#define PRINTF_HELP() \
	do {\
		printf("crtpy_example <input file> <output file> <direction> [padding] [<alg name> <keysize>] [golden file]\n");\
		printf("direction:	0: encrypt	1: decrypt\n");\
		printf("padding:	0: none		1: pkcs#7\n");\
		printf("keysize:	0: efuse key mode\n");\
	} while (0)

#define CRYPT_KEY_MAX	32
#define CRYPT_IV_MAX	16
#define CRYPT_BUF_MAX_BYTES	(RTS_CIPHER_ENCRYPT_LEN_MAX + 256)

static char key[CRYPT_KEY_MAX] = {
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
			0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
			0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
			0x07, 0x00, 0x00, 0x00, 0x08};

static char iv[CRYPT_IV_MAX] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

static int crypt_test(const char *in_file, const char *out_file,
			int direction, const char *alg_name,
			uint32_t keysize,
			const char *golden_file,
			enum RTS_CIPHER_PADDING_MODE padding_mode)
{
	int ret;
	struct rts_cipher_handle *handle = NULL;
	char algname[60];
	uint32_t blocksize, ivsize;
	char input_text[CRYPT_BUF_MAX_BYTES] = {0};
	char *output_text = NULL;
	int inlen, outlen;
	FILE *in_fd, *out_fd, *golden_fd;
	char charval;
	int i;

	/* split alg name */
	sprintf(algname, "%s", alg_name);
	printf("cipher alg name: %s,%s\n", alg_name, algname);
	/* int rts cipher*/
	ret = rts_cipher_init(&handle, algname);
	if (ret || !handle) {
		printf("rts cipher handle init fail, ret = %d\n", ret);
		return ret;
	}
	printf("rts cipher handle init ok\n");

	/* padding mode*/
	ret = rts_cipher_set_padding(handle, padding_mode);
	if (ret) {
		printf("rts cipher set padding mode fail, ret = %d\n", ret);
		goto err;
	}
	printf("rts cipher set padding mode = %d\n", padding_mode);

	/* get blocksize */
	ret = rts_cipher_get_blocksize(handle, &blocksize);
	if (ret) {
		printf("rts cipher get blocksize fail, ret = %d\n", ret);
		goto err;
	}
	printf("rts cipher get blocksize = %d\n", blocksize);

	/* get ivsize */
	ret = rts_cipher_get_ivsize(handle, &ivsize);
	if (ret) {
		printf("rts cipher get ivsize fail, ret = %d\n", ret);
		goto err;
	}
	printf("rts cipher get ivsize = %d\n", ivsize);

	if (ivsize > CRYPT_IV_MAX) {
		printf("ivsize > CRYPT_IV_MAX, ivsize = %d\n", ivsize);
		goto err;
	}

	/* set key */
	if (keysize) {
		ret = rts_cipher_set_keymode(RTS_CIPHER_KEYMODE_NORMAL);
		if (ret) {
			printf("rts cipher set keymode normal fail, ret = %d\n",
				ret);
			goto err;
		}
		printf("rts cipher set keymode normal ok\n");

		ret = rts_cipher_setkey(handle, key, keysize);
		if (ret) {
			printf("rts cipher set key fail, ret = %d\n",
				ret);
			goto err;
		}
		printf("rts cipher set key ok\n");
	} else {
		ret = rts_cipher_set_keymode(RTS_CIPHER_KEYMODE_EFUSE);
		if (ret) {
			printf("rts cipher set keymode efuse fail, ret = %d\n",
				ret);
			goto err;
		}
		printf("rts cipher set keymode efuse ok\n");
	}

	/* set iv */
	if (ivsize) {
		ret = rts_cipher_setiv(handle, iv, ivsize);
		if (ret) {
			printf("rts cipher set iv fail, ret = %d\n", ret);
			goto err;
		}
		printf("rts cipher set iv ok\n");
	}

	/* read input text*/
	in_fd = fopen(in_file, "r");
	if (!in_fd) {
		printf("open input file %s fail\n", in_file);
		goto err;
	}

	inlen = fread(input_text, 1, CRYPT_BUF_MAX_BYTES, in_fd);
	fclose(in_fd);

	/* alloc output buffer*/
	if (!direction) /* encrypt */
		outlen = (inlen / blocksize + 1) * blocksize;
	else /* decrypt */
		outlen = inlen;
	printf("outlen = %d\n", outlen);

	output_text = calloc(1, outlen);
	if (!output_text) {
		printf("calloc output_text fail\n");
		goto err;
	}

	/* encrypt */
	if (!direction) {
		printf("start encrypt inlen = %d\n", inlen);
		ret = rts_cipher_encrypt(handle, input_text, inlen,
					output_text, (uint32_t *)&outlen);
		if (ret) {
			printf("rts cipher encrypt fail, ret = %d\n", ret);
			goto err;
		}
	/* decrypt */
	} else {
		printf("start decrypt inlen = %d\n", inlen);
		ret = rts_cipher_decrypt(handle, input_text, inlen,
					output_text, (uint32_t *)&outlen);
		if (ret) {
			printf("rts cipher decrypt fail, ret = %d\n", ret);
			goto err;
		}
	}
	printf("actual outlen = %d\n", outlen);

	/* save output text*/
	out_fd = fopen(out_file, "wb+");
	if (!out_fd) {
		printf("open output file %s fail\n", out_file);
		goto err;
	}

	for (i = 0; i < outlen; i++) {
		charval = output_text[i];
		fputc(charval, out_fd);
	}
	fclose(out_fd);
	printf("saved output file\n");

	/* compare with golden*/
	if (golden_file) {
		/* read golden text*/
		golden_fd = fopen(golden_file, "r");
		if (!golden_fd) {
			printf("open golden file %s fail\n", golden_file);
			goto err;
		}

		memset(input_text, 0, sizeof(input_text));
		inlen = fread(input_text, 1, CRYPT_BUF_MAX_BYTES, golden_fd);
		fclose(golden_fd);

		if (inlen != outlen) {
			printf("inlen = %d, != outlen\n", inlen);
			goto err;
		}

		ret = strcmp(output_text, input_text);
		if (ret) {
			printf("compare with golden fail, ret = %d\n", ret);
			goto err;
		} else {
			printf("compare with golden pass\n");
		}
	}

	if (output_text) {
		free(output_text);
		output_text = NULL;
	}

	rts_cipher_destroy(handle);
	return 0;
err:
	if (output_text) {
		free(output_text);
		output_text = NULL;
	}

	rts_cipher_destroy(handle);
	return -1;
}

int main(int argc, char **argv)
{
	int ret;
	const char *in_file, *out_file, *alg_name;
	int direction;
	uint32_t keysize;
	char alg_df[10] = "cbc(aes)";
	const char *golden_file = NULL;
	enum RTS_CIPHER_PADDING_MODE padding_mode;

	/* parse arg */
	if (argc == 4 || argc == 5) {
		printf("default alg name: cbc(aes)\n");
		alg_name = alg_df;
		keysize = RTS_CIPHER_AES_KEYSIZE_128;
		padding_mode = RTS_CIPHER_PADDING_NONE;

		if (argc == 5)
			padding_mode =
				(enum RTS_CIPHER_PADDING_MODE)atoi(argv[4]);

	} else if (argc >= 7 && argc <= 8) {
		padding_mode = (enum RTS_CIPHER_PADDING_MODE)atoi(argv[4]);
		alg_name = argv[5];
		if (argc >= 7)
			keysize = (uint32_t)atoi(argv[6]);

		if (argc == 8)
			golden_file = argv[7];

	} else {
		PRINTF_HELP();
		return -1;
	}

	in_file = argv[1];
	out_file = argv[2];
	direction = atoi(argv[3]);

	/* crypt test */
	printf("start crypt test\n");
	ret = crypt_test(in_file, out_file, direction,
				alg_name, keysize, golden_file,
				padding_mode);
	if (ret) {
		printf("crypt test fail, ret = %d\n", ret);
		return -1;
	}

	printf("crypt test pass\n");
	return 0;
}
