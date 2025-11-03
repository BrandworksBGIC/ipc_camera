/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VIDEO_H__
#define __VIDEO_H__

#define MAX_STREAM_NUM 2
#define VIDEO_SHM_BUFF (1024 * 1024)

struct configs {
	int cur_status;
	int priv_status;
	int refresh_rb;
	int rebuild_rb;
};

struct video_context {
	int init_ok;
	int isp_id;
	int isp_buf_num;
	int isp_ch;
	int h264_ch;
	int h265_ch;
	int mjpeg_ch;
	pthread_mutex_t mutex;
	void *ringbuf;
	char shm_video[64];
	struct configs config;
	pthread_t tid;
	int status;
};

extern struct video_context *g_video_ctx[MAX_STREAM_NUM];

struct isp_config {
	int isp_id;
	int isp_buf_num;
	int vin_mode;
	int width;
	int height;
	int framerate;
};

struct h264_config {
	int level;
	int qp;
	unsigned int bitrate;
	unsigned int gop;
	int rotation;
};

struct h265_config {
	int level;
	int tier;
	unsigned int bitrate;
	unsigned int gop;
	int rotation;
	int mirror;
};

struct video_config {
	int codec_type;
	struct isp_config isp_config;
	struct h264_config h264_config;
	struct h265_config h265_config;
};

extern struct video_config g_config[MAX_STREAM_NUM];

int rf_control_video_capture(
	int request, void *args);
int rf_start_video_capture(void);
void rf_stop_video_capture(void);

#endif
