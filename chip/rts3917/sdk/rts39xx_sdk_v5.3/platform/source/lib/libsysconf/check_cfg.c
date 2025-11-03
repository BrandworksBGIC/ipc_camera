/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <errno.h>
#include "sysconf.h"

int main(int argc, char **argv)
{
	int ret = 0;

	if (argc < 2)
		return 0;

	ret = rts_conf_scanf(argv[1], "%k");

	if (-EIO == ret) {
		return -1;
	} else {
		return 0;
	}
}
