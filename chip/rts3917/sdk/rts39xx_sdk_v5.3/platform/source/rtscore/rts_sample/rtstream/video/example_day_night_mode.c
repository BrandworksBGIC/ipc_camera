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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <errno.h>
#include <pthread.h>
#include "rts_io_gpio.h"
#include "rts_errno.h"

/* code flow

+---------+         +---------+
|         |         |         |
|  vin_ch |========>|encode_ch|========>receive_block=====>daynight_out.h265
|         |         |         |
+---------+         +---------+

+------------+
|            |      +------------+      +------------+      +------------+
| thread-    |      |   isp      |      |    isp     |      |            |
| test_day   |=====>| gray mode  |=====>| ir mode    |=====>| gpio irct  |
| _night_mode|      |            |      |            |      |            |
+------------+      +------------+      +------------+      +------------+

*/

#define DAY_MODE 0
#define NIGHT_MODE 1

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type;
static char rts_enc_name[10];
static char *save_dir;
static int rts_num;
static int rts_ret;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"mode", required_argument, NULL, 'm'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{0, 0, 0, 0}
};

static int g_exit;

static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tan example for setting day/night mode\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_day_night [OPTION]\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-m, --mode\t0-day mode, 1-night mode\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_day_night_mode -e h265/h264 -m 0 -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

static int get_valid_value(int id, int value, struct rts_isp_control *ctrl)
{
	int tvalue = value;

	if (value < ctrl->minimum)
		tvalue = ctrl->minimum;
	if (value > ctrl->maximum)
		tvalue = ctrl->maximum;
	if ((value - ctrl->minimum) % ctrl->step)
		tvalue = value - (value - ctrl->minimum) % ctrl->step;

	return tvalue;
}

static int test_set_attr(uint32_t id, int value)
{
	struct rts_isp_control ctrl;
	int ret;

	ret = rts_av_get_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}
	value = get_valid_value(id, value, &ctrl);

	RTS_INFO("before setting: ");

	if (!ctrl.current_value)
		RTS_INFO("day mode\n");
	else
		RTS_INFO("night mode\n");

	ctrl.current_value = value;
	ret = rts_av_set_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("set isp attr fail, ret = %d\n", ret);
		return ret;
	}

	/**
	 * get check whether the new value is set or not,
	 * no need in actual use
	 */
	ret = rts_av_get_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}

	return RTS_OK;
}

static int test_get_attr(uint32_t id)
{
	struct rts_isp_control ctrl;
	int ret;

	ret = rts_av_get_isp_ctrl(id, &ctrl);
	if (ret) {
		RTS_ERR("get isp attr fail, ret = %d\n", ret);
		return ret;
	}
	RTS_INFO("after setting : ");
	if (!ctrl.current_value)
		RTS_INFO("day mode\n");
	else
		RTS_INFO("night mode\n");

	return RTS_OK;
}

static void *test_day_night_mode(void *arg)
{
	int value = 0;
	struct rts_gpio *rts_gpio;
	int ret;

	value = *((int *)arg);

	RTS_INFO("--------GRAY_MODE--------\n");
	ret = test_set_attr(RTS_ISP_CTRL_ID_GRAY_MODE, value);
	if (ret)
		goto exit;

	ret = test_get_attr(RTS_ISP_CTRL_ID_GRAY_MODE);
	if (ret)
		goto exit;

	RTS_INFO("---------IR_MODE---------\n");
	ret = test_set_attr(RTS_ISP_CTRL_ID_IR_MODE, value);
	if (ret)
		goto exit;

	ret = test_get_attr(RTS_ISP_CTRL_ID_IR_MODE);
	if (ret)
		goto exit;

exit:
	RTS_INFO("quit test_day_night_mode thread\n");
	rts_ret = ret;

	return NULL;
}

int create_h264_encode_chn(void)
{
	struct rts_h264_attr h264_attr = {0};

	h264_attr.level = H264_LEVEL_4;
	h264_attr.rotation = RTS_AV_ROTATION_0;

	return rts_av_create_h264_chn(&h264_attr);
}

int create_h265_encode_chn(void)
{
	struct rts_h265_attr h265_attr = {0};

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;

	return rts_av_create_h265_chn(&h265_attr);
}

int create_encode_chn(void)
{
	if (rts_enc_type == RTS_ENC_H264)
		return create_h264_encode_chn();
	else if (rts_enc_type == RTS_ENC_H265)
		return create_h265_encode_chn();

	return -1;
}

int test_stream(int value)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	pthread_t tid;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int enc = -1;
	int ret;
	char outfile[100];

	vin_attr.vin_id = 0;
	vin_attr.vin_buf_num = 1;
	vin_attr.vin_mode = RTS_AV_VIN_RING_MODE;
	vin = rts_av_create_vin_chn(&vin_attr);
	if (vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", vin);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = 1280;
	profile.video.height = 720;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	enc = create_encode_chn();
	if (enc < 0) {
		RTS_ERR("fail to create %s chn, ret = %d\n",
				rts_enc_name, enc);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("%s chn : %d\n", rts_enc_name, enc);

	ret = rts_av_bind(vin, enc);
	if (ret) {
		RTS_ERR("fail to bind vin and %s, ret %d\n",
				rts_enc_name, ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile),
			"%s/daynight_out.%s", save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file daynight_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc);
	rts_av_start_recv(enc);

	pthread_create(&tid, NULL, test_day_night_mode, &value);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(enc, &buffer, 100))
			continue;

		if (buffer) {
			if (pfile)
				fwrite(buffer->vm_addr, 1,
					buffer->bytesused, pfile);
			number++;
			rts_av_put_buffer(buffer);
		}

		if (rts_num > 0 && number >= rts_num)
			break;
	}

	pthread_join(tid, NULL);
	if (rts_ret)
		ret = rts_ret;

	rts_av_stop_recv(enc);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(enc);
	rts_av_unbind(vin, enc);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (enc >= 0) {
		rts_av_destroy_chn(enc);
		enc = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int value = DAY_MODE;

	if (argc < 2) {
		printf("need more parameters\n");
		printf("use -h to get help info\n");
		return -1;
	}

	while ((c = getopt_long(argc, argv,
				":he:m:s:n:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'e':
			if (strcmp(optarg, "h264") == 0) {
				rts_enc_type = RTS_ENC_H264;
			} else if (strcmp(optarg, "h265") == 0) {
				rts_enc_type = RTS_ENC_H265;
			} else {
				printf("error encode type: %s\n", optarg);
				return -1;
			}
			snprintf(rts_enc_name, sizeof(rts_enc_name),
				"%s", optarg);
			break;
		case 'm':
			value = strtol(optarg, NULL, 0);
			if (value < 0 || value > 1) {
				printf("error mode: %d\n", value);
				return -1;
			}
			break;
		case 's':
			save_dir = optarg;
			break;
		case 'n':
			rts_num = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
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

	ret = test_stream(value);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
