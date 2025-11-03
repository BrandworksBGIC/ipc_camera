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
#include <rtsvideo.h>

#include "rf_msg.h"
#include "mask.h"
#include "video.h"

struct rts_mask_attr *g_mask_attr = NULL;

static int rf_get_mask_attr(struct rf_mask_attr *mask_attr)
{
	int ret = RF_ERR_OK;
	struct rf_mask_attr *rf_attr = mask_attr;
	struct rts_mask_block *block;

	if(!rf_attr)
		return RF_ERR_GET_MASK;

	ret = rts_av_get_mask(g_mask_attr);
	if (ret != RF_ERR_OK) {
		RTS_ERR("get mask attr fail, ret = %d\n");
		return RF_ERR_GET_MASK;
	}

	rf_attr->color = g_mask_attr->color;
	block = g_mask_attr->blocks;
	for (int i = 0; i < MASK_GRID_NUM; i++) {
		rf_attr->grids[i].enable = block->enable;
		rf_attr->grids[i].supported_grid_num =
					block->supported_grid_num;

		rf_attr->grids[i].start_x = block->area.start.x;
		rf_attr->grids[i].start_y = block->area.start.y;
		rf_attr->grids[i].cell_width = block->area.cell.width;
		rf_attr->grids[i].cell_height = block->area.cell.height;
		rf_attr->grids[i].grid_rows = block->area.size.rows;
		rf_attr->grids[i].grid_columns = block->area.size.columns;
		rf_attr->grids[i].bitmap.vm_addr = block->area.bitmap.vm_addr;
		rf_attr->grids[i].bitmap.length = block->area.bitmap.length;
		block++;
	}

	for (int i = 0; i < MASK_RECT_NUM; i++) {
		rf_attr->rects[i].enable = block->enable;

		rf_attr->rects[i].left = block->rect.left;
		rf_attr->rects[i].right = block->rect.right;
		rf_attr->rects[i].top = block->rect.top;
		rf_attr->rects[i].bottom = block->rect.bottom;
		block++;
	}

	return RF_ERR_OK;
}

static void rf_reset_grid_bitmap(struct rf_grid_bitmap *bitmap)
{
	free(bitmap->vm_addr);
	bitmap->length = 0;
}

static int rf_set_mask_attr(struct rf_mask_attr *mask_attr)
{
	int ret = RF_ERR_OK;
	struct rf_mask_attr *rf_attr = mask_attr;
	struct rts_mask_block *block;
	int grid_num;

	if(!rf_attr)
		return RF_ERR_SET_MASK;

	ret = rf_get_mask_attr(rf_attr);
	if (!ret)
		return ret;

	g_mask_attr->color = rf_attr->color;

	block = g_mask_attr->blocks;
	for (int i = 0; i < MASK_GRID_NUM; i++) {
		block->enable = rf_attr->grids[i].enable;
		if  (block->enable) {
			block->area.start.x = rf_attr->grids[i].start_x;
			block->area.start.y = rf_attr->grids[i].start_y;
			block->area.cell.width = rf_attr->grids[i].cell_width;
			block->area.cell.height = rf_attr->grids[i].cell_height;
			block->area.size.rows = rf_attr->grids[i].grid_rows;
			block->area.size.columns =
					rf_attr->grids[i].grid_columns;
			block->area.bitmap.vm_addr =
				rf_attr->grids[i].bitmap.vm_addr;
			block->area.bitmap.length =
				rf_attr->grids[i].bitmap.length;

			grid_num = rf_attr->grids[i].grid_rows
				* rf_attr->grids[i].grid_columns;
			if (grid_num > g_mask_attr->blocks->supported_grid_num)
				return RF_ERR_GRID_NUM;
		} else {
			rf_reset_grid_bitmap(&rf_attr->grids[i].bitmap);
		}
		block++;
	}

	for (int i = 0; i < MASK_RECT_NUM; i++) {
		block->enable = rf_attr->rects[i].enable;

		block->rect.left = rf_attr->rects[i].left;
		block->rect.right = rf_attr->rects[i].right;
		block->rect.top = rf_attr->rects[i].top;
		block->rect.bottom = rf_attr->rects[i].bottom;
		block++;
	}

	ret = rts_av_set_mask(g_mask_attr);
	if (ret != RF_ERR_OK) {
		RTS_ERR("set mask attr fail, ret = %d\n");
		return RF_ERR_SET_MASK;
	}

	return RF_ERR_OK;
}

int rf_control_mask(
	int request, void *arg)
{
	int ret = RF_ERR_OK;
	struct rf_mask_attr *attr = (struct rf_mask_attr *)arg;

	switch (request) {
	case RF_MASK_GET_ATTR:
		rf_get_mask_attr(attr);

		break;
	case RF_MASK_SET_ATTR:
		rf_set_mask_attr(attr);

		break;
	default:
		ret = RF_ERR_REQUEST_NOT_SUPPORT;
		break;
	}

	return ret;
}

int rf_init_mask(void)
{
	int ret = RF_ERR_OK;
	int chn;
	struct rts_av_profile isp_profile;

	chn = g_video_ctx[0]->isp_ch;
	ret = rts_av_get_profile(chn, &isp_profile);

	ret = rts_av_query_mask(&g_mask_attr,
			isp_profile.video.width,
			isp_profile.video.height);
	if (ret != RF_ERR_OK) {
		RTS_ERR("query mask attr fail, ret = %d\n", ret);
		goto exit;
	}

	return RF_ERR_OK;

exit:
	rf_release_mask();
	return RF_ERR_QUERY_MASK;
}

void rf_release_mask(void)
{
	rts_av_release_mask(g_mask_attr);
}

