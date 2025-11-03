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
#include <pthread.h>
#include <errno.h>
#include <libubus.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

#include "rf_msg.h"
#include "mjpeg.h"
#include "video.h"

#define COMPRESS_RATE 4
struct rts_mjpeg_ctrl *g_mjpeg_ctrl;
static int snapshot_num = 1;

void rf_release_mjpeg_ctrl(void)
{
	rts_av_release_mjpeg_ctrl(g_mjpeg_ctrl);
}

int rf_set_mjpeg_ctrl(void)
{
	int ret = RF_ERR_OK;

	g_mjpeg_ctrl->normal_compress_rate = COMPRESS_RATE;
	ret = rts_av_set_mjpeg_ctrl(g_mjpeg_ctrl);
	if (ret != RF_ERR_OK) {
		RTS_ERR("rts_av_set_mjpeg_ctrl failed, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_get_mjpeg_ctrl(g_mjpeg_ctrl);
	if (ret != RF_ERR_OK) {
		RTS_ERR("rts_av_get_mjpeg_ctrl failed, ret = %d\n", ret);
		goto exit;
	}

	return RF_ERR_OK;

exit:
	rf_release_mjpeg_ctrl();
	return RF_ERR_CTRL_MJPEG;
}

int rf_init_mjpeg(void)
{
	int ret = RF_ERR_OK;
	int chn_num = 1;
	int mjpeg_chn;

	mjpeg_chn = g_video_ctx[chn_num]->mjpeg_ch;
		if (mjpeg_chn < 0) {
			RTS_ERR("cant init mjpeg chn\n");
			return ret;
			goto exit;
		}

	ret = rts_av_query_mjpeg_ctrl(mjpeg_chn, &g_mjpeg_ctrl);
	if (ret != RF_ERR_OK) {
		RTS_ERR("query mjpeg fail, ret = %d\n", ret);
		goto exit;
	}

	return ret;

exit:
	rf_release_mjpeg_ctrl();
	return RF_ERR_QUERY_MJPEG;
}

int save_mjpeg(uint8_t *pdata, uint32_t length)
{
	static uint64_t index;
	char filename[64];
	FILE *pfile = NULL;

	snprintf(filename, sizeof(filename), "%lld.jpg", index++);

	pfile = fopen(filename, "wb");
	if (!pfile) {
		RTS_ERR("open %s fail\n", filename);
		return RF_ERR_OPENFILE;
	}

	fwrite(pdata, 1, length, pfile);

	printf("save pic %s\n", filename);

	RTS_SAFE_RELEASE(pfile, fclose);

	return RTS_OK;
}

void mjpeg_cb(void *priv, struct rts_av_profile *profile,
	      struct rts_av_buffer *buffer)
{
	save_mjpeg(buffer->vm_addr, buffer->bytesused);
	snapshot_num--;
}

static void *flag_monitor(void)
{
	while (snapshot_num != 0)
		usleep(10);
	rts_av_disable_chn(g_video_ctx[0]->mjpeg_ch);

	return NULL;
}

int rf_set_mjpeg_callback(uint32_t start, uint32_t pic_num, uint32_t interval)
{
	int ret;
	int mjpeg_chn;
	pthread_t tid;
	struct rts_av_callback cb;

	mjpeg_chn = g_video_ctx[1]->mjpeg_ch;
	cb.func = mjpeg_cb;
	cb.start = start;
	cb.times = pic_num;
	cb.interval = interval;
	cb.type = RTS_AV_CB_TYPE_SYNC;
	cb.priv = NULL;

	snapshot_num = pic_num;

	ret = rts_av_set_callback(mjpeg_chn, &cb, 0);
	if (ret) {
		RTS_ERR("set mjpeg callback fail, ret = %d\n", ret);
		return RF_ERR_SNAPSHOT;
	}
	pthread_create(&tid, NULL, (void *)flag_monitor, 0);
	return RF_ERR_OK;
}

