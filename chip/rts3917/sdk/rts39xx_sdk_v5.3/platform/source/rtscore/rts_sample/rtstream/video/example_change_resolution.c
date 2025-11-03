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
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <getopt.h>
#include <string.h>

/* code flow

+---------+         +---------+
|         |         |         |========>receive_block=====>res_out.h265
|  vin_ch |========>|encode_ch|
|  (YUV)  |         |         |========>change_resolution (when recv number=50)
+---------+         +---------+

*/

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type;
char rts_enc_name[10];
static char *save_dir;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"save", required_argument, NULL, 's'},
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
	fprintf(stdout, "\tprovide a example to change resolution\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_change_resolution [OPTION]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_change_resolution -e h265/h264 -s /mnt\n");
}

static int change_resolution(int chn, int w, int h)
{
	struct rts_av_profile profile;
	uint32_t tmp_w, tmp_h;
	int ret;

	ret = rts_av_get_profile(chn, &profile);
	if (ret) {
		RTS_ERR("get profile fail, ret = %d\n", ret);
		return ret;
	}
	tmp_w = profile.video.width;
	tmp_h = profile.video.height;

	profile.video.width = w;
	profile.video.height = h;
	ret = rts_av_set_profile(chn, &profile);
	if (ret) {
		RTS_ERR("set profile fail, ret = %d\n", ret);
		return ret;
	}

	/**
	 * get check whether the new value is set or not,
	 * no need in actual use
	 */
	ret = rts_av_get_profile(chn, &profile);
	if (ret) {
		RTS_ERR("get profile fail, ret = %d\n", ret);
		return ret;
	}
	RTS_INFO("[change resolution]%dx%d->%dx%d\n", tmp_w, tmp_h,
			profile.video.width, profile.video.height);

	return RTS_OK;
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

int test_stream(void)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int enc_chn = -1;
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

	enc_chn = create_encode_chn();
	if (enc_chn < 0) {
		RTS_ERR("fail to create %s chn, ret = %d\n",
				rts_enc_name, enc_chn);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("%s chn : %d\n", rts_enc_name, enc_chn);

	ret = rts_av_bind(vin, enc_chn);
	if (ret) {
		RTS_ERR("fail to bind vin and encode, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/res_out.%s",
				save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file res_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc_chn);
	rts_av_start_recv(enc_chn);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(enc_chn, &buffer, 100))
			continue;

		if (buffer) {
			if (save_dir)
				fwrite(buffer->vm_addr, 1,
				       buffer->bytesused, pfile);
			number++;
			rts_av_put_buffer(buffer);
			if (number == 50)
				change_resolution(vin, 640, 480);
			if (number >= 100)
				break;
		}
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
	int c;
	int ret;

	if (argc < 2) {
		printf("need more parameters\n");
		printf("use -h to get help info\n");
		return -1;
	}

	while ((c = getopt_long(argc, argv,
				":he:s:", longopts, NULL)) != -1) {
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
		case 's':
			save_dir = optarg;
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

	ret = test_stream();

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
