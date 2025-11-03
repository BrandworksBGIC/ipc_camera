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
#include <getopt.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

/* code flow

+---------+      +---------+
|         |      |         |
|  vin_ch | ===> | h264_ch | ===> rts_av_set_h264_ctrl ===> h264ctrl_out.h264
|         |      |         |
+---------+      +---------+

*/

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

static char *save_dir;
static int rts_num;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"num", required_argument, NULL, 'n'},
	{"save", required_argument, NULL, 's'},
	{0, 0, 0, 0}
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\texample for h264 ctrl\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_h264_ctrl [option]...");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_h264_ctrl -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

int test_stream(void)
{
	struct rts_h264_attr h264_attr = {0};
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int h264 = -1;
	int ret;
	char outfile[100];

	struct rts_h264_ctrl *ctrl = NULL;

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

	h264_attr.level = H264_LEVEL_4;
	h264_attr.rotation = 0;
	h264 = rts_av_create_h264_chn(&h264_attr);
	if (h264 < 0) {
		RTS_ERR("fail to create h264 chn, ret = %d\n", h264);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h264 chn : %d\n", vin);

	ret = rts_av_bind(vin, h264);
	if (ret) {
		RTS_ERR("fail to bind vin and h264, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/ctrl_out.h264",
					save_dir);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file ctrl_out.h264 fail\n");
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	ret = rts_av_query_h264_ctrl(h264, &ctrl);
	if (ret) {
		RTS_ERR("Failed to query ctrl\n");
		goto exit;
	}

	ctrl->bitrate_mode = RTS_BITRATE_MODE_CBR;
	ctrl->bitrate = 1024 * 1024;
	ret = rts_av_set_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h264 ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_get_h264_ctrl(ctrl);
	if (ret) {
		RTS_ERR("get h264 ctrl fail, ret = %d\n", ret);
		goto exit;
	}
	RTS_INFO("ctrl:bitrate mode = 0x%x, bitrate = %d\n",
		ctrl->bitrate_mode, ctrl->bitrate);

	rts_av_enable_chn(vin);
	rts_av_enable_chn(h264);
	rts_av_start_recv(h264);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(h264, &buffer, 100))
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

	rts_av_stop_recv(h264);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(h264);
	rts_av_unbind(vin, h264);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	RTS_SAFE_RELEASE(ctrl, rts_av_release_h264_ctrl);
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (h264 >= 0) {
		rts_av_destroy_chn(h264);
		h264 = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret;
	int c;

	while ((c = getopt_long(argc, argv,
				":hn:s:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 's':
			save_dir = optarg;
			break;
		case 'n':
			rts_num = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
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

	ret = test_stream();

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return RTS_OK;
}
