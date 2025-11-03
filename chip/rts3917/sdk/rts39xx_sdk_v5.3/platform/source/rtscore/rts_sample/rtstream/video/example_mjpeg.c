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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtstream.h>
#include <rtsvideo.h>

#define WIDTH		1280
#define HEIGHT		720

#define QUALITY_MIN	1
#define QUALITY_MAX	100

struct __dbg_setting {
	uint32_t width;
	uint32_t height;
	int fmt;
	int fps;
	int mirror;
	int rotate;
	int vin_id;
	int vin_mode;
	int jpg_mode;
	int quality;
	int number;
};

static int g_exit;


/* code flow

+---------+         +---------+
|         | callback|         |
|  vin_ch |========>| jpeg_ch |
|         |         |         |
+---------+         +---------+

*/

static void Termination(int sign)
{
	g_exit = 1;
}

struct option longopts[] = {
	{"quality", required_argument, NULL, 'q'},
	{"save", required_argument, NULL, 's'},
	{"number", required_argument, NULL, 'n'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\texample for mjpeg\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texampl_mjpeg [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-s, --save\tsave dir\n");
	fprintf(stdout, "-q, --quality\tquality=[1, 100]\n");
	fprintf(stdout, "-n, --num\tencode number\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_mjpeg -q 50 -n 10 -s /mnt\n");
	fprintf(stdout, "\n");
}

static uint32_t __get_outbuf_length(uint32_t w, uint32_t h, int quality)
{
	uint32_t length = 0;

	if (quality > 80)
		length = w * h;
	else if (quality > 60)
		length = w * h * 2 / 3;
	else
		length = w * h / 2;

	return length;
}

static char *save_dir;
int save_mjpeg(uint8_t *pdata, uint32_t length)
{
	static uint64_t index;
	char filename[64];
	FILE *pfile = NULL;

	if (!save_dir)
		return 0;

	snprintf(filename, sizeof(filename), "%s/%lld.jpg", save_dir, index++);

	pfile = fopen(filename, "wb");
	if (!pfile) {
		RTS_ERR("open %s fail\n", filename);
		return RTS_RETURN(RTS_E_OPEN_FAIL);
	}

	fwrite(pdata, 1, length, pfile);

	RTS_SAFE_RELEASE(pfile, fclose);

	return RTS_OK;
}

int check_cfg(struct __dbg_setting *setting)
{
	if (setting->vin_id > 1 && setting->vin_mode == 2) {
		printf("vin direct mode only work in vin_id 0/1\n");
		return -1;
	}
	if (setting->jpg_mode < 0 || setting->jpg_mode > 1) {
		printf("jpeg stream mode range [0~1]\n");
		return -1;
	}
	if (setting->quality > QUALITY_MAX || setting->quality < QUALITY_MIN) {
		printf("quality %d out of range!\n", setting->quality);
		return -1;
	}

	return 0;
}

int test_mjpeg(struct __dbg_setting *setting)
{
	int ret = -1;
	static int done;
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile av_profile_vin;
	struct rts_jpgenc_attr jpg_attr = {0};
	int vin = -1;
	int jpg = -1;

	vin_attr.vin_id = setting->vin_id;
	vin_attr.vin_buf_num = 2;
	vin_attr.vin_mode = setting->vin_mode;
	vin = rts_av_create_vin_chn(&vin_attr);
	if (vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", vin);

	av_profile_vin.fmt = setting->fmt;
	av_profile_vin.video.width = setting->width;
	av_profile_vin.video.height = setting->height;
	av_profile_vin.video.numerator = 1;
	av_profile_vin.video.denominator = setting->fps;

	ret = rts_av_set_profile(vin, &av_profile_vin);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	jpg_attr.mirror = setting->mirror;
	jpg_attr.rotation = setting->rotate;
	jpg_attr.stream_mode = setting->jpg_mode;
	jpg = rts_av_create_mjpeg_chn(&jpg_attr);
	if (jpg < 0) {
		RTS_ERR("fail to create jpg chn, ret = %d\n", jpg);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("jpg chn : %d\n", jpg);

	ret = rts_av_bind(vin, jpg);
	if (ret) {
		RTS_ERR("fail to bind vin and jpg, ret %d\n", ret);
		goto exit;
	}

	ret = rts_av_set_mjpeg_quality(jpg, setting->quality);
	if (ret) {
		RTS_ERR("rts_av_set_mjpeg_quality failed, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_enable_chn(vin);
	ret |= rts_av_enable_chn(jpg);
	if (ret) {
		RTS_ERR("rts_av_enable_chn failed, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_start_recv(jpg);
	if (ret) {
		RTS_ERR("rts_av_start_recv failed, ret = %d\n", ret);
		goto exit;
	}

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(jpg, &buffer, 100))
			continue;
		if (buffer) {
			save_mjpeg(buffer->vm_addr, buffer->bytesused);
			done++;
			rts_av_put_buffer(buffer);
		}

		if (setting->number && setting->number == done)
			break;
	}

exit:
	RTS_INFO("callback %d times\n", done);
	if (jpg >= 0) {
		rts_av_stop_recv(jpg);
		rts_av_disable_chn(jpg);
		rts_av_unbind(vin, jpg);
		rts_av_destroy_chn(jpg);
		jpg = -1;
	}
	if (vin >= 0) {
		rts_av_disable_chn(vin);
		rts_av_destroy_chn(vin);
		vin = -1;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = RTS_OK;
	int c;
	struct __dbg_setting setting;
	uint32_t len;

	setting.quality = 60;
	setting.mirror = 0;
	setting.rotate = 0;
	setting.width = WIDTH;
	setting.height = HEIGHT;
	setting.fps = 15;
	setting.fmt = RTS_V_FMT_YUV422SEMIPLANAR;
	setting.vin_mode = RTS_AV_VIN_DIRECT_MODE;
	setting.vin_id = 0;
	setting.jpg_mode = RTS_AV_JPG_NO_TRIGGER;
	setting.number = 0;

	while ((c = getopt_long(argc, argv,
				":hq:s:n:", longopts, NULL))
				!= -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return RTS_OK;
		case 'q':
			setting.quality = (uint16_t)strtol(optarg, NULL, 0);
			break;
		case 's':
			save_dir = optarg;
			break;
		case 'n':
			setting.number = (int)strtol(optarg, NULL, 0);
			break;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
		default:
			break;
		}
	}

	ret = check_cfg(&setting);
	if (ret) {
		printf("fail to check cfg, ret %d\n", ret);
		return -1;;
	}

	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = rts_av_init();
	if (ret) {
		RTS_INFO("rts_av_init failed, ret = %d\n", ret);
		return ret;
	}

	len = __get_outbuf_length(setting.width, setting.height,
			setting.quality);

	RTS_INFO("test %dx%d, mirror=%d, rotate=%d, quality=%d, fmt=%d, fps=%d\n",
		setting.width, setting.height, setting.mirror, setting.rotate,
		setting.quality, setting.fmt, setting.fps);

	ret = test_mjpeg(&setting);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
