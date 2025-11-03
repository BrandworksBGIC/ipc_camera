/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _SYSCONF_H
#define _SYSCONF_H

#include <json-c/json.h>
#include <stdarg.h>

#define CFG_DOMAIN_SYSTEM	"system"
#define CFG_DOMAIN_MULTIMEDIA	"peacock"
#define CFG_DOMAIN_ONVIF	"onvif"
#define CFG_DOMAIN_NETWORK	"network"
#define CFG_DOMAIN_ISP		"isp"
#define CFG_DOMAIN_MTD_MASK		"mtd_mask"
#define CFG_DOMAIN_OSD		"osd"
#define CFG_DOMAIN_TEST		"test"
#define CFG_DOMAIN_BLUETOOTH	"bluetooth"
#define CFG_DOMAIN_BLESS	"bless"
#define CFG_DOMAIN_BLESS_CONFIG "bless_conf"
#define CFG_DOMAIN_AEFRAMING	"ae_framing"

#define TYPE_INT	0
#define TYPE_STRING	1

enum {
	E_DOMAIN_INVALID	= 0x70000001,
	E_FS_INVALID		= 0x70000002,
	E_KEY_NOT_FOUND		= 0x70000005,
	E_INVALID_OBJ_TYPE	= 0x70000006,
};

enum {
	SHARED_ACCESS = 0,
	EXCLUSIVE_ACCESS,


	SYSCONF_FILE_MODIFY		= 1,
	SYSCONF_FILE_DELETE		= 2,
};


int rts_conf_scanf(char *domain, const char *fmt, ...);
int rts_conf_printf(char *domain, const char *fmt, ...);
int rts_conf_printf_vl(char *domain, const char *fmt, va_list vl);
int rts_conf_reset(char *domain);
void *rts_conf_get_metadata(const char *domain, int flag);
int rts_conf_scanf_ex(void *metadata, const char *fmt, ...);
int rts_conf_printf_ex(void *metadata, const char *fmt, ...);
int rts_conf_put_metadata(const char *domain, void *metadata);
void rts_conf_free_metadata(void *metadata);

/* rts_conf_check_storage_change - check whether underlying file had been changed.
 * @metadata - input param
 *
 * return value
 * 0 - underlying file had not been changed.
 * 1 - underlying file had been changed
 */
int rts_conf_check_storage_change(void *metadata);

#endif
