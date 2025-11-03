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
|  vin_ch | ===> | h265_ch | ===> rts_av_set_h265_ctrl ===> h265ctrl_out.h265
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
	fprintf(stdout, "\texample for h265 ctrl\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_h265_ctrl [option]...");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_h265_ctrl -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

int test_stream(void)
{
	struct rts_h265_attr h265_attr = {0};
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int h265 = -1;
	int ret;
	char outfile[100];

	struct rts_h265_ctrl *ctrl = NULL;

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

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	h265_attr.mirror = RTS_AV_MIRROR_NO;
	h265 = rts_av_create_h265_chn(&h265_attr);
	if (h265 < 0) {
		RTS_ERR("fail to create h265 chn, ret = %d\n", h265);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h265 chn : %d\n", vin);

	ret = rts_av_bind(vin, h265);
	if (ret) {
		RTS_ERR("fail to bind vin and h265, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/ctrl_out.h265",
					save_dir);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file ctrl_out.h265 fail\n");
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	ret = rts_av_query_h265_ctrl(h265, &ctrl);
	if (ret) {
		RTS_ERR("Failed to query ctrl\n");
		goto exit;
	}

	ctrl->bitrate_mode = RTS_BITRATE_MODE_CBR;
	ctrl->bitrate = 1024 * 1024;
	ret = rts_av_set_h265_ctrl(ctrl);
	if (ret) {
		RTS_ERR("set h265 ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_get_h265_ctrl(ctrl);
	if (ret) {
		RTS_ERR("get h265 ctrl fail, ret = %d\n", ret);
		goto exit;
	}
	RTS_INFO("ctrl:bitrate mode = 0x%x, bitrate = %d\n",
		ctrl->bitrate_mode, ctrl->bitrate);

	rts_av_enable_chn(vin);
	rts_av_enable_chn(h265);
	rts_av_start_recv(h265);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(h265, &buffer, 100))
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

	rts_av_stop_recv(h265);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(h265);
	rts_av_unbind(vin, h265);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	RTS_SAFE_RELEASE(ctrl, rts_av_release_h265_ctrl);
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (h265 >= 0) {
		rts_av_destroy_chn(h265);
		h265 = -1;
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
