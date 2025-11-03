/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __CUNIT_SUIT_H__
#define __CUNIT_SUIT_H__
#define static

struct test_item {
	const char *name;
	CU_TestFunc func;
};

struct test_suite {
	const char *name;
	CU_InitializeFunc init;
	CU_CleanupFunc cleanup;
	struct test_item *items;
};

#endif
