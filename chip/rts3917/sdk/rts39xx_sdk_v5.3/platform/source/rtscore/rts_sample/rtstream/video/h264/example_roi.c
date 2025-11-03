/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <getopt.h>

struct rts_h264_stream_cfg {
	struct {
		int vin_id;
		enum RTS_AV_FMT format;
		uint32_t width;
		uint32_t height;
		uint32_t numerator;
		uint32_t denominator;
		int vin_buf_num;
	} vin_cfg;

	enum RTS_H264_LEVEL level;

	int qp;
	unsigned int bps;
	unsigned int gop;
	enum RTS_AV_ROTATION rotation;
	int videostab;
};

static int g_exit;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\ta example for roi\n");
	fprintf(stdout, "\tthe region of interest is (0.0)-(320.240)\n");
	fprintf(stdout, "\toutput out.264 file under program directory\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_roi\n");
}

int test_roi(int chnno)
{
	struct rts_h264_roi *roi;
	int ret;

	ret = rts_av_query_h264_roi(chnno, &roi);
	if (ret)
		return ret;

	if (roi->count <= 0)
		return RTS_RETURN(RTS_E_GET_FAIL);

	roi->roi->enable = 1;
	roi->roi->area.start.x = 0;
	roi->roi->area.start.y = 0;
	roi->roi->area.end.x = 320;
	roi->roi->area.end.y = 240;
	roi->roi->value = -10;

	ret = rts_av_set_h264_roi(roi);
	if (ret)
		goto exit;

	ret = rts_av_get_h264_roi(roi);
	RTS_INFO("ret = %d, roi.0 [%d,%d][%d,%d] %d\n", ret,
		 roi->roi->area.start.x,
		 roi->roi->area.start.y,
		 roi->roi->area.end.x,
		 roi->roi->area.end.y,
		 roi->roi->value);

exit:
	RTS_SAFE_RELEASE(roi, rts_av_release_h264_roi);
	return ret;
}

int cancel_roi(int chnno)
{
	struct rts_h264_roi *roi;
	int ret;

	ret = rts_av_query_h264_roi(chnno, &roi);
	if (ret)
		return ret;

	if (roi->count <= 0)
		return RTS_RETURN(RTS_E_GET_FAIL);

	roi->roi->enable = 0;
	roi->roi->value = 0;

	ret = rts_av_set_h264_roi(roi);

	RTS_SAFE_RELEASE(roi, rts_av_release_h264_roi);
	return ret;
}

int main(int argc, char *argv[])
{
	struct rts_av_buffer *buffer = NULL;
	struct rts_h264_stream_cfg cfg;
	int ret;
	int number = 0;
	int h264 = -1;
	int vin = -1;
	struct timeval begin, end;
	unsigned long delta;
	struct rts_av_profile profile;
	struct rts_h264_attr h264_attr = {0};
	struct rts_vin_attr vin_attr = {0};
	FILE *pfile = NULL;
	int c;

	while ((c = getopt_long(argc, argv,
				":h", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return RTS_OK;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return RTS_RETURN(RTS_E_INVALID_ARG);
		default:
			break;
		}
	}
	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = rts_av_init();
	if (ret) {
		RTS_ERR("rts_av_init fail\n");
		return ret;
	}

	cfg.vin_cfg.vin_id = 0;
	cfg.vin_cfg.format = RTS_V_FMT_YUV420SEMIPLANAR;
	cfg.vin_cfg.width = 1280;
	cfg.vin_cfg.height = 720;
	cfg.vin_cfg.numerator = 1;
	cfg.vin_cfg.denominator = 15;
	cfg.vin_cfg.vin_buf_num = 2;
	cfg.level = H264_LEVEL_4;
	cfg.rotation = RTS_AV_ROTATION_0;
	cfg.videostab = 0;

	vin_attr.vin_id = cfg.vin_cfg.vin_id;
	vin_attr.vin_buf_num = cfg.vin_cfg.vin_buf_num;
	vin_attr.vin_mode = RTS_AV_VIN_RING_MODE;
	if (vin_attr.vin_id != 0 && vin_attr.vin_mode == 1) {
		RTS_ERR("vin ring mode only work in vin_id 0\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}
	if (vin_attr.vin_id > 1 && vin_attr.vin_mode == 2) {
		RTS_ERR("vin direct mode only work in vin_id 0/1\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}
	if (vin_attr.vin_mode < 0 || vin_attr.vin_mode > 2) {
		RTS_ERR("vin mode range [0~2]\n");
		return RTS_RETURN(RTS_E_INVALID_ARG);
	}
	vin = rts_av_create_vin_chn(&vin_attr);
	if (vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = cfg.vin_cfg.width;
	profile.video.height = cfg.vin_cfg.height;
	profile.video.numerator = cfg.vin_cfg.numerator;
	profile.video.denominator = cfg.vin_cfg.denominator;
	ret = rts_av_set_profile(vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	h264_attr.level = cfg.level;
	h264_attr.rotation = cfg.rotation;
	h264 = rts_av_create_h264_chn(&h264_attr);
	if (!h264) {
		RTS_ERR("fail to create h264 chn, ret = %d\n", h264);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	ret = rts_av_bind(vin, h264);
	if (ret) {
		RTS_ERR("fail to bind vin and h264, ret %d\n", ret);
		goto exit;
	}

	RTS_INFO("fmt = %d, size = %dx%d, fps = %d/%d, level = %d\n",
		 cfg.vin_cfg.format, cfg.vin_cfg.width, cfg.vin_cfg.height,
		 cfg.vin_cfg.numerator, cfg.vin_cfg.denominator,
		 cfg.level);

	rts_av_get_profile(h264, &profile);
	RTS_INFO("fmt = %d, %dx%d, %d/%d\n",
		 profile.fmt, profile.video.width, profile.video.height,
		 profile.video.numerator, profile.video.denominator);

	ret = rts_av_enable_chn(vin);
	if (ret) {
		RTS_ERR("rts_av_enable_chn fail, ret = %d\n", ret);
		goto exit;
	}
	ret = rts_av_enable_chn(h264);
	if (ret) {
		RTS_ERR("rts_av_enable_chn fail, ret = %d\n", ret);
		goto exit;
	}

	ret = test_roi(h264);
	if (ret) {
		RTS_ERR("test roi fail, ret = %d\n", ret);
		goto exit;
	}

	rts_av_start_recv(h264);

	pfile = fopen("out.h264", "wb");
	gettimeofday(&begin, NULL);
	while (!g_exit) {
		gettimeofday(&end, NULL);
		delta = (end.tv_sec - begin.tv_sec) * 1000 +
			end.tv_usec / 1000 - begin.tv_usec / 1000;
		if (delta >= 10000)
			break;

		if (rts_av_recv_block(h264, &buffer, 100))
			continue;

		if (buffer) {
			number++;
			if (buffer->flags & RTSTREAM_PKT_FLAG_KEY)
				RTS_INFO("Get I Frame\n");
			RTS_INFO("frame size %d\n", buffer->bytesused);
			if (pfile)
				fwrite(buffer->vm_addr, 1,
				       buffer->bytesused, pfile);
			rts_av_put_buffer(buffer);
			buffer = NULL;
		}
		if (number == 50) {
			ret = cancel_roi(h264);
			if (ret) {
				RTS_ERR("cancel roi fail, ret = %d\n", ret);
				break;
			}
		}
	}

	rts_av_disable_chn(h264);
	RTS_SAFE_RELEASE(pfile, fclose);

	RTS_INFO("get %d frames\n", number);
exit:
	if (h264 >= 0) {
		rts_av_destroy_chn(h264);
		h264 = -1;
	}

	rts_av_release();

	return ret;
}
