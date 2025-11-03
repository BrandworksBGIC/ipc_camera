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
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <getopt.h>

/* code flow

+---------+         +---------+
|         |         |         |
|  vin_ch |========>|encode_ch|========>receive_block=====>awb_out.h265
|         |         |         |
+---------+         +---------+

+----------+          +-------------------------+
|          |          |1. get_isp_awb           |
| thread-  |          |2. awb_auto              |
| ctrl_awb |=========>|3. awb_temperature       |
|          |          |4. awb_component         |
+----------+          |5. awb_ct_gain           |
                      |6. refresh_isp_awb_statis|
                      +-------------------------+
*/

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type;
static char rts_enc_name[10];
static char *save_dir;
static int rts_num;
static int rts_cmd;
static int rts_ret;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{"cmd", required_argument, NULL, 'c'},
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
	fprintf(stdout, "\ta example to change awb parameter\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_awb [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-c, --cmd\tawb command, id list:\n");
	fprintf(stdout, "\t\t1--set_awb_auto\n");
	fprintf(stdout, "\t\t2--set_awb_temperature\n");
	fprintf(stdout, "\t\t3--set_awb_component\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_awb -e h265/h264 -s /mnt -n 10 -c 1\n");
	fprintf(stdout, "\n");
}

int set_awb_auto(struct rts_isp_awb_ctrl *awb)
{
	awb->mode = RTS_ISP_AWB_AUTO;
	awb->_auto.r_gain = 256;
	awb->_auto.b_gain = 256;

	return rts_av_set_isp_awb(awb);
}

int set_awb_temperature(struct rts_isp_awb_ctrl *awb)
{
	awb->mode = RTS_ISP_AWB_TEMPERATURE;
	awb->_manual.temperature = 5000;

	return rts_av_set_isp_awb(awb);
}

int set_awb_component(struct rts_isp_awb_ctrl *awb)
{
	awb->mode = RTS_ISP_AWB_COMPONENT;
	awb->_component.red = 515;
	awb->_component.green = 256;
	awb->_component.blue = 445;

	return rts_av_set_isp_awb(awb);
}

void print_awb_ctrl(struct rts_isp_awb_ctrl *awb)
{
	char *mode_str;
	uint16_t *pdata;
	int i;

	switch (awb->mode) {
	case RTS_ISP_AWB_AUTO:
		mode_str = "auto";
		break;
	case RTS_ISP_AWB_TEMPERATURE:
		mode_str = "temperature";
		break;
	case RTS_ISP_AWB_COMPONENT:
		mode_str = "component";
		break;
	default:
		mode_str = "unknown";
		break;
	}

	RTS_INFO("--------\n");
	RTS_INFO("(%d x %d) %d\n",
		 awb->window_size.width, awb->window_size.height,
		 awb->window_num);
	RTS_INFO("mode : %s\n", mode_str);
	RTS_INFO("auto r gain:%d\n", awb->_auto.r_gain);
	RTS_INFO("auto b gain:%d\n", awb->_auto.b_gain);
	RTS_INFO("temperature:%d\n", awb->_manual.temperature);
	RTS_INFO("component r:%d\n", awb->_component.red);
	RTS_INFO("component g:%d\n", awb->_component.green);
	RTS_INFO("component b:%d\n", awb->_component.blue);
	RTS_INFO("r_means:\n");
	pdata = awb->statis.r_means;
	for (i = 0; i < awb->window_num; i++) {
		RTS_OPT("%3d", *pdata);
		if (i % 16 != 15 && i != awb->window_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
		pdata++;
	}
	RTS_INFO("g_means:\n");
	pdata = awb->statis.g_means;
	for (i = 0; i < awb->window_num; i++) {
		RTS_OPT("%3d", *pdata);
		if (i % 16 != 15 && i != awb->window_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
		pdata++;
	}
	RTS_INFO("b_means:\n");
	pdata = awb->statis.b_means;
	for (i = 0; i < awb->window_num; i++) {
		RTS_OPT("%3d", *pdata);
		if (i % 16 != 15 && i != awb->window_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
		pdata++;
	}
}

static void *ctrl_awb(void *arg)
{
	struct rts_isp_awb_ctrl *awb;
	int ret;

	/*wait 100ms after enable isp channel to query awb*/
	usleep(100 * 1000);

	ret = rts_av_query_isp_awb(&awb);
	if (ret) {
		RTS_ERR("query isp af ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	switch(rts_cmd) {
	case 1:
		RTS_INFO("set awb auto\n");
		ret = set_awb_auto(awb);
		break;
	case 2:
		RTS_INFO("set awb temperature\n");
		ret = set_awb_temperature(awb);
		break;
	case 3:
		RTS_INFO("set awb component\n");
		ret = set_awb_component(awb);
		break;
	default:
		RTS_INFO("set awb auto\n");
		ret = set_awb_auto(awb);
		break;
	}
	if (ret) {
		RTS_ERR("awb ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_get_isp_awb(awb);
	if (ret) {
		RTS_ERR("get isp awb ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	while (!g_exit) {
		print_awb_ctrl(awb);

		usleep(1 * 1000 * 1000);
		ret = rts_av_refresh_isp_awb_statis(awb);
		if (ret) {
			RTS_ERR("refresh isp awb statis fail, ret = %d\n", ret);
			goto exit;
		}
	}
exit:
	RTS_SAFE_RELEASE(awb, rts_av_release_isp_awb);
	RTS_INFO("quit awb control thread\n");
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

int test_stream(void)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	pthread_t tid;

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
		snprintf(outfile, sizeof(outfile), "%s/awb_out.%s",
				save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file awb_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc_chn);
	rts_av_start_recv(enc_chn);

	pthread_create(&tid, NULL, ctrl_awb, NULL);

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

	g_exit = 1;
	pthread_join(tid, NULL);
	if (rts_ret)
		ret = rts_ret;

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
				":he:s:n:c:", longopts, NULL)) != -1) {
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
		case 'n':
			rts_num = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'c':
			rts_cmd = (uint32_t)strtol(optarg, NULL, 0);
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
