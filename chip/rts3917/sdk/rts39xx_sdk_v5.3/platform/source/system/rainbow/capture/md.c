/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <libubus.h>
#include <rtscamkit.h>
#include <rtsavapi.h>

#include "rf_msg.h"
#include "md.h"

int rf_control_md(
	int request, void *arg)
{
	int ret = RF_ERR_OK;
	struct rf_md_attr *attr = arg;

	switch (request) {
	case RF_MD_GET_ATTR:

		break;
	case RF_MD_SET_ATTR:

		break;
	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}

int rf_init_md(void)
{
	int ret = RF_ERR_OK;

	return ret;
}

void rf_release_md(void)
{

}
