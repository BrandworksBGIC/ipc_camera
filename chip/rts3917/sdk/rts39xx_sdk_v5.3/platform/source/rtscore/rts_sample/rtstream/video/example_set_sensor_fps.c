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
#include <rts_pthreadpool.h>

/* code flow

+---------+    +---------+
|         |    |         |===> set_isp_ctrl(RTS_ISP_AE_PRIORITY_MANUAL) ==> set_isp_dynamic_fps =====> receive_block
|  vin_ch |===>|encode_ch|
|         |    |         |===> set_isp_ctrl(RTS_ISP_AE_PRIORITY_AUTO)
+---------+    +---------+

*/

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type;
static char rts_enc_name[10];
static char *save_dir;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"fps", required_argument, NULL, 'f'},
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
	fprintf(stdout, "\tprovide a example to set sensor fps, only for RTS3903/RTS3913\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_set_sensor_fps [OPTION]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-f, --fps\tfps\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_ae -e h265/h264 -f 25 -s /mnt\n");
	fprintf(stdout, "\n");
}

void set_sensor_fps(uint8_t fps)
{
	uint32_t id = RTS_ISP_CTRL_ID_EXPOSURE_PRIORITY;
	struct rts_isp_control ctrl;

	rts_av_get_isp_ctrl(id, &ctrl);
	if (fps) {
		uint8_t tmp;

		ctrl.current_value = RTS_ISP_AE_PRIORITY_MANUAL;
		rts_av_set_isp_ctrl(id, &ctrl);

		tmp = rts_av_get_isp_dynamic_fps();
		rts_av_set_isp_dynamic_fps(fps);

		RTS_INFO("[sensor fps]%d -> %d\n",
			 tmp, rts_av_get_isp_dynamic_fps());
	} else {
		ctrl.current_value = RTS_ISP_AE_PRIORITY_AUTO;
		rts_av_set_isp_ctrl(id, &ctrl);

		RTS_INFO("[sensor fps] : %d\n", rts_av_get_isp_dynamic_fps());
	}
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

int test_stream(uint8_t fps)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int enc = -1;
	int ret;
	char outfile[100];

	vin_attr.vin_id = 0;
	vin_attr.vin_buf_num = 2;
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
		RTS_ERR("fail to bind vin and encode, ret %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc);

	set_sensor_fps(fps);

	rts_av_start_recv(enc);



	if (save_dir) {
		snprintf(outfile, sizeof(outfile),
			"%s/sensor_fps_out.%s", save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file sensor_fps_out.%s fail\n",
					rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

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
	}

	rts_av_disable_chn(vin);
	rts_av_disable_chn(enc);

	RTS_SAFE_RELEASE(pfile, fclose);

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

	return ret;
}

int main(int argc, char *argv[])
{
	int c;
	int ret;
	uint8_t fps = 25;

	if (argc < 2) {
		printf("need more parameters\n");
		printf("use -h to get help info\n");
		return RTS_OK;
	}
	while ((c = getopt_long(argc, argv,
				":he:f:s:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return RTS_OK;
		case 'e':
			if (strcmp(optarg, "h264") == 0) {
				rts_enc_type = RTS_ENC_H264;
			} else if (strcmp(optarg, "h265") == 0) {
				rts_enc_type = RTS_ENC_H265;
			} else {
				fprintf(stdout, "error encode type: %s\n",
						optarg);
				return RTS_OK;
			}
			snprintf(rts_enc_name, sizeof(rts_enc_name),
				"%s", optarg);
			break;
		case 'f':
			fps = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 's':
			save_dir = optarg;
			break;
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

	test_stream(fps);

	rts_av_release();

	return 0;
}
