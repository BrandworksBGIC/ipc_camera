/*
*  Copyright (C) 2019 Realtek Semiconductor Corp.
*  All Rights Reserved
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtscamkit.h>
#include <getopt.h>

#include "rf_msg.h"
#include "video.h"
#include "osd_font.h"
#include "osd.h"

static void *update_osdi_timedate(void *arg)
{
	struct osd_info *info = arg;
	int ret = 0;
	char timedate[20] = {0};
	time_t now;
	struct tm tm = {0};
	struct rts_osdi_text_cfg timecfg;
	struct rts_osdi_block *block;

	timecfg.target_height = info->char_h;
	timecfg.rotate = info->time_rotate;

	block = &info->osdi_attr->blocks[TIME_BLK_ID];
	block->rect.left = info->char_h / 2;
	block->rect.top = 0;
	block->enable = 1;

	while (!info->quit) {
		now = time(NULL);
		localtime_r(&now, &tm);

		sprintf(timedate, "%04d-%02d-%02d %02d:%02d:%02d",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

		timecfg.text = timedate;
		timecfg.textlen = strlen(timedate);
		ret = rts_av_osdi_text_set(info->osdi_attr, TIME_BLK_ID, &timecfg);
		if (ret < 0) {
			RTS_ERR("set timestamp osdi text fail, ret = %d\n", ret);
			break;
		}
		usleep(1000*1000 / info->refresh_rate);
	}

	info->quit = 1;

	return NULL;
}

static int show_custom_text(struct osd_info *info)
{
	int ret;
	char text[16] = {0};
	struct rts_osdi_text_cfg textcfg;
	struct rts_osdi_block *block;

	textcfg.text = CUSTOM_TEXT;
	textcfg.textlen = strlen(textcfg.text);
	textcfg.target_height = info->char_h;
	textcfg.rotate = info->text_rotate;

	block = &info->osdi_attr->blocks[TEXT_BLK_ID];
	block->rect.left = info->img_width - info->char_h;
	block->rect.top = info->char_h / 2;
	block->enable = 1;

	ret = rts_av_osdi_text_set(info->osdi_attr, TEXT_BLK_ID, &textcfg);
	if (ret < 0)
		RTS_ERR("set custom osdi text fail, ret = %d\n", ret);

	return ret;
}

static int enable_osd(struct osd_info *info)
{
	int ret;
	struct rts_osdi_text_attr text_attr = {0};

	ret = rts_av_query_osdi(info->isp_ch, &info->osdi_attr);
	if (ret) {
		RTS_ERR("query osdi attr fail\n");
		return ret;
	}

	/* configure ASCII font lib attribute */
	text_attr.tagcode_asc = (uint16_t *)fonttag_asc;
	text_attr.taglen_asc = taglength_asc;
	text_attr.font_asc = (uint8_t *)fontlib_asc;
	text_attr.height = FONT_HEIGHT;
	text_attr.width_asc = FONT_WIDTH;
	text_attr.fmt = RTS_OSDI_BLK_FMT_RGBA2222;

	/* configure chinese font lib attribute */
	text_attr.tagcode_chi = (uint16_t *)fonttag_chi;
	text_attr.taglen_chi = taglength_chi;
	text_attr.font_chi = (uint8_t *)fontlib_chi;
	text_attr.width_chi = FONT_WIDTH_CH;

	rts_av_osdi_text_config(info->osdi_attr, &text_attr);

	pthread_create(&info->tid, NULL, update_osdi_timedate, info);

	ret = show_custom_text(info);
	if (ret)
		RTS_ERR("show custom text fail\n");

	return ret;
}

static int disable_osd(struct osd_info *info)
{
	int i, ret = 0;
	struct rts_osdi_attr *attr = info->osdi_attr;

	ret = rts_av_query_osdi(info->isp_ch, &attr);
	if (!ret) {
		for (i = 0; i < attr->number; i++)
			attr->blocks[i].enable = 0;
		ret = rts_av_set_osdi(attr);
	}

	info->quit = 1;

	if (info->tid != 0)
		pthread_join(info->tid, NULL);
	info->tid = 0;

	RTS_SAFE_RELEASE(attr, rts_av_release_osdi);
	attr = NULL;

	return ret;
}

static struct osd_info *create_info(int index)
{
	struct osd_info *info = NULL;

	info = calloc(1, sizeof(struct osd_info));

	info->osdi_attr = NULL;
	info->quit = 0;
	info->text_rotate = RTS_AV_ROTATION_90R;
	info->time_rotate = RTS_AV_ROTATION_0;
	info->refresh_rate = 1;
	info->isp_ch = g_config[index].isp_config.isp_id;
	info->img_width = g_config[index].isp_config.width;
	info->img_height = g_config[index].isp_config.height;
	info->char_h = info->isp_ch > 0 ? 32 : 64;

	return info;
}

static void release_info(struct osd_info *info)
{
	if (info)
		free(info);
}

int rf_init_osdi(void)
{
	int i, ret = 0;
	struct osd_info *info = NULL;

	for (i = 0; i < MAX_STREAM_NUM; i++) {
		info = create_info(i);
		ret = enable_osd(info);
		g_osd_ctx[i] = info;
	}

	return ret;
}

void rf_release_osdi(void)
{
	int i, ret = 0;
	struct osd_info *info = NULL;

	for (i = 0; i < MAX_STREAM_NUM; i++) {
		info = g_osd_ctx[i];
		if (info) {
			ret = disable_osd(info);
			release_info(info);
			g_osd_ctx[i] = NULL;
		}
	}
}

static int rf_get_osd_attr(struct rf_osd_attr *osd_attr)
{
	int ret = RF_ERR_OK;
	struct rf_osd_attr *rf_attr = osd_attr;
	struct rts_osdi_block *block;

	ret = rts_av_get_osdi(g_osd_ctx[0]->osdi_attr);
	if (ret != RF_ERR_OK)
		return RF_ERR_GET_OSD_ATTR;

	block = g_osd_ctx[0]->osdi_attr->blocks;

	rf_attr->blocks[0].enable = block->enable;
	rf_attr->blocks[0].left = block->rect.left;
	rf_attr->blocks[0].top = block->rect.top;
	rf_attr->blocks[0].right = block->rect.right;
	rf_attr->blocks[0].bottom = block->rect.bottom;
	rf_attr->blocks[0].picture.pixel_fmt = block->picture.pixel_fmt;
	rf_attr->blocks[0].picture.pdata = block->picture.pdata;
	rf_attr->blocks[0].picture.length = block->picture.length;

	return ret;
}

static int rf_set_osd_attr(struct rf_osd_attr *osd_attr)
{
	int ret = RF_ERR_OK;
	struct rf_osd_attr *rf_attr = osd_attr;
	struct rts_osdi_block *block;

	ret = rts_av_get_osdi(g_osd_ctx[0]->osdi_attr);
	if (ret != RF_ERR_OK)
		return RF_ERR_GET_OSD_ATTR;

	block = g_osd_ctx[0]->osdi_attr->blocks;

	block->enable = rf_attr->blocks[0].enable;

	if (block->enable) {
		block->rect.left = rf_attr->blocks[0].left;
		block->rect.top = rf_attr->blocks[0].top;
		block->rect.right = rf_attr->blocks[0].right;
		block->rect.bottom = rf_attr->blocks[0].bottom;

		int width = block->rect.right - block->rect.left;
		int height = block->rect.bottom - block->rect.top;
		char data[width][height];

		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++)
				data[x][y] = 0xc3;
		}
		block->picture.pdata = data;
		block->picture.pixel_fmt =
			rf_attr->blocks[0].picture.pixel_fmt;
		block->picture.length = width * height;

		if (block->picture.pdata) {
			ret = rts_av_set_osdi_single(
				g_osd_ctx[0]->osdi_attr, 0);
			if (ret)
				return RF_ERR_SET_OSD_ATTR;
		}
	} else {
		block->picture.pdata = NULL;
		block->picture.length = 0;
	}

	return ret;
}

int rf_control_osdi(
	int request, void *arg)
{
	int ret = RF_ERR_OK;
	struct rf_osd_attr *attr = arg;

	switch (request) {
	case RF_OSD_GET_ATTR:
		rf_get_osd_attr(attr);
		break;
	case RF_OSD_SET_ATTR:
		rf_set_osd_attr(attr);
		break;
	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}
