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

/* code flow

+---------+      +---------+
|         |      |         |
|  vin_ch | ===> | h265_ch | ===> disable fov ===> rts_av_set_isp_crop ===> isp_crop.h265
|         |      |         |
+---------+      +---------+

*/

static char *save_dir;
static int rts_num;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{"start_x", required_argument, NULL, 'x'},
	{"start_y", required_argument, NULL, 'y'},
	{"end_x", required_argument, NULL, 'X'},
	{"end_y", required_argument, NULL, 'Y'},
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
	fprintf(stdout, "\texample for isp crop\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_isp_crop [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-x, --start_x\tstart x\n");
	fprintf(stdout, "-y, --start_y\tstart y\n");
	fprintf(stdout, "-X, --end_x\tend x\n");
	fprintf(stdout, "-Y, --end_y\tend y\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_isp_crop -x 0 -y 0 -X 640 -Y 480 -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

int set_isp_crop(int vin, struct rts_video_rect *pcrop)
{
	int ret = RTS_OK;

	ret = rts_av_set_isp_fov_mode(vin, 1);
	if (ret) {
		RTS_ERR("Fail to set fov mode in channel %d, ret = %d\n",
			vin, ret);
		goto exit;
	}

	ret = rts_av_set_isp_crop(vin, pcrop);
	if (ret) {
		RTS_ERR("Fail to set isp crop in channel %d, ret = %d\n",
			vin, ret);
		goto exit;
	}

	printf("isp crop:start_x = %d,start_y = %d,end_x = %d,end_y = %d\n",
		pcrop->start.x, pcrop->start.y,
		pcrop->end.x, pcrop->end.y);
exit:
	return ret;
}

int test_stream(struct rts_video_rect *pcrop)
{
	struct rts_h265_attr h265_attr = {0};
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int enc_chn = -1;
	int ret = RTS_OK;
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

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	h265_attr.mirror = RTS_AV_MIRROR_NO;
	enc_chn = rts_av_create_h265_chn(&h265_attr);
	if (enc_chn < 0) {
		RTS_ERR("fail to create h265 chn, ret = %d\n", enc_chn);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h265 chn : %d\n", enc_chn);

	ret = rts_av_bind(vin, enc_chn);
	if (ret) {
		RTS_ERR("fail to bind vin and h265, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/isp_crop.h265",
					save_dir);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file isp_crop.h265 fail\n");
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	ret = set_isp_crop(vin, pcrop);
	if (ret)
		goto exit;

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc_chn);
	rts_av_start_recv(enc_chn);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(enc_chn, &buffer, 100))
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

	rts_av_stop_recv(enc_chn);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(enc_chn);
	rts_av_unbind(vin, enc_chn);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (enc_chn >= 0) {
		rts_av_destroy_chn(enc_chn);
		enc_chn = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret;
	int c;
	struct rts_video_rect crop;

	crop.start.x = 0;
	crop.start.y = 0;
	crop.end.x = 640;
	crop.end.y = 480;

	while ((c = getopt_long(argc, argv,
				":hs:n:x:y:X:Y:", longopts, NULL)) != -1) {
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
		case 'x':
			crop.start.x = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'y':
			crop.start.y = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'X':
			crop.end.x = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'Y':
			crop.end.y = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
		default:
			break;
		}
	}

	if ((crop.start.x >= crop.end.x) ||
			(crop.start.y >= crop.end.y)) {
		printf("error parameters\n");
		return -1;
	}

	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	ret = rts_av_init();
	if (ret) {
		RTS_ERR("rts_av_init fail\n");
		return ret;
	}

	ret = test_stream(&crop);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
