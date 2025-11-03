/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OSD_H__
#define __OSD_H__

#include <librtsosdi.h>
#include "video.h"

#define CUSTOM_TEXT "通道1 星期1"
#define TIME_BLK_ID 0
#define TEXT_BLK_ID 2

struct osd_info {
	int quit;
	int isp_ch;
	int img_width;
	int img_height;
	uint32_t char_h;
	uint32_t text_rotate;
	uint32_t time_rotate;
	int refresh_rate;
	pthread_t tid;
	struct rts_osdi_attr *osdi_attr;
};

static struct osd_info *g_osd_ctx[MAX_STREAM_NUM];

int rf_control_osdi(
	int request, void *args);
int rf_init_osdi(void);
void rf_release_osdi(void);

#endif
