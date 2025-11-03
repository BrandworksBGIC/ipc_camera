/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <CUnit/Basic.h>

#include "cunit_suit.h"

static int __init_()
{

	return 0;
}

static int __cleanup_()
{

	return 0;
}

#define FILE_SRC	"s.test"
#define FILE_DST	"d.test"

static void get_random(uint8_t *buf, int len)
{
	int i = 0;

	srandom(time(NULL));

	for (i = 0; i < len; i++)
		buf[i] = (uint8_t) random();
}

static void generate_source_file(const char *file, uint8_t *buf, int len)
{
	FILE *fp = fopen(file, "w+");

	CU_ASSERT(NULL != fp);
	if (!fp)
		return;

	fwrite(buf, sizeof(buf[0]), len, fp);
	fflush(fp);
	fclose(fp);
}

static void test_sync_file()
{
	const char *file_s = FILE_SRC;
	const char *file_d = FILE_DST;
	FILE *fp_d = NULL;
	uint8_t buf_s[128] = {0};
	uint8_t buf_d[128] = {0};

	get_random(buf_s, sizeof(buf_s));
	generate_source_file(file_s, buf_s, sizeof(buf_s));

	sync_file(file_d, file_s, 0);

	fp_d = fopen(file_d, "r");
	CU_ASSERT(NULL != fp_d);
	if (!fp_d)
		return;
	fread(buf_d, sizeof(buf_d[0]), sizeof(buf_d), fp_d);
	fclose(fp_d);
	CU_ASSERT(0 == memcmp(buf_s, buf_d, sizeof(buf_d)));
}

#define FILE_TEST "test.conf"

static void test_sync_conf_file()
{
	struct conf_file cf = {{NULL, NULL}, FILE_TEST, {0, 0}, 0, 0};
	FILE *fp_d = NULL;
	uint8_t buf_s[128] = {0};
	uint8_t buf_d[128] = {0};

	system("mkdir -p /var/conf");
	get_random(buf_s, sizeof(buf_s));
	generate_source_file(CONF_DIR_RAMFS"/"FILE_TEST, buf_s, sizeof(buf_s));

	sync_conf_file(&cf);

	fp_d = fopen(CONF_DIR_JFFS2"/"FILE_TEST, "r");
	CU_ASSERT(NULL != fp_d);
	if (!fp_d)
		return;
	fread(buf_d, sizeof(buf_d[0]), sizeof(buf_d), fp_d);
	fclose(fp_d);
	CU_ASSERT(0 == memcmp(buf_s, buf_d, sizeof(buf_d)));

}

static void test_delete_conf_file()
{
	struct conf_file cf = {{NULL, NULL}, FILE_TEST, {0, 0}, 0, 0};

	system("mkdir -p /var/conf");
	system("touch " CONF_DIR_JFFS2 "/" FILE_TEST);
	delete_conf_file(&cf);
	CU_ASSERT(0 != access(CONF_DIR_JFFS2 "/" FILE_TEST, F_OK));

}

static struct test_item test_entropy[] = {
	{"test sync file", test_sync_file},
	{"test sync conf file", test_sync_conf_file},
	{"test delete conf file", test_delete_conf_file},
	{NULL, NULL},
};

struct test_suite test_suite_entropy = {
	.name = "entropy",
	.init = __init_,
	.cleanup = __cleanup_,
	.items = test_entropy,
};
