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
|  vin_ch |========>|encode_ch|========>receive_block=====>ae_out.h265
|  (YUV)  |         |         |
+---------+         +---------+             +----------------+
                                            |2. total_gain   |
+---------+          +----------+Manual mode|3. manual_gain  |
|         |          |          |==========>|4. exposure_time|
| thread- |          |          |            +---------------+
| ctrl_ae |=========>|rts_av_   |
|         |          |set_isp_ae|Auto mode  +---------------+
+---------+          |          |==========>|1. set auto    |
                     +----------+           |5. target_delta|
                                            |6. gain_max    |
                                            |7. min_fps     |
                                            |8. win_weights |
                                            +---------------+
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
	fprintf(stdout, "\texample to auto exposure\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_ae [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-c, --cmd\tae command, id list:\n");
	fprintf(stdout, "\t\t1--set_ae_auto\n");
	fprintf(stdout, "\t\t2--set_total_ae_gain\n");
	fprintf(stdout, "\t\t3--set_ae_gain\n");
	fprintf(stdout, "\t\t4--set_ae_exposure_time\n");
	fprintf(stdout, "\t\t5--set_ae_target_delta\n");
	fprintf(stdout, "\t\t6--set_ae_gain_max\n");
	fprintf(stdout, "\t\t7--set_ae_min_fps\n");
	fprintf(stdout, "\t\t8--set_ae_weight\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_ae -e h265/h264 -s /mnt -n 10 -c 1\n");
	fprintf(stdout, "\n");
}

int set_ae_auto(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_AUTO;

	return rts_av_set_isp_ae(ae);
}

int set_total_ae_gain(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.total_gain = 256;

	return rts_av_set_isp_ae(ae);
}

int set_ae_gain(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.gain.analog = 256;
	ae->_manual.gain.digital = 256;
	ae->_manual.gain.isp_digital = 256;

	return rts_av_set_isp_ae(ae);
}

int set_ae_exposure_time(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_MANUAL;
	ae->_manual.exposure_time = 10000;

	return rts_av_set_isp_ae(ae);
}

int set_ae_target_delta(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.target_delta = 0;

	return rts_av_set_isp_ae(ae);
}

int set_ae_gain_max(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.gain_max = 4096;

	return rts_av_set_isp_ae(ae);
}

int set_ae_min_fps(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.min_fps = 0;

	return rts_av_set_isp_ae(ae);
}

int set_ae_weight(struct rts_isp_ae_ctrl *ae)
{
	ae->mode = RTS_ISP_AE_AUTO;
	ae->_auto.win_weights[0] = 2;

	return rts_av_set_isp_ae(ae);
}

void print_ae_ctrl(struct rts_isp_ae_ctrl *ae)
{
	int i;

	RTS_INFO("--------\n");
	RTS_INFO("(%d x %d) %d %d\n",
		 ae->window_size.width, ae->window_size.height,
		 ae->window_num, ae->histogram_num);
	RTS_INFO("mode : %s\n",
		 ae->mode == RTS_ISP_AE_AUTO ? "auto" : "manual");
	RTS_INFO("total_gain : %d\n", ae->_manual.total_gain);
	RTS_INFO("gain analog : %d\n", ae->_manual.gain.analog);
	RTS_INFO("gain digital : %d\n", ae->_manual.gain.digital);
	RTS_INFO("gain isp_digital : %d\n", ae->_manual.gain.isp_digital);
	RTS_INFO("exposure_time : %d\n", ae->_manual.exposure_time);
	RTS_INFO("weights :\n");
	for (i = 0; i < ae->window_num; i++) {
		uint8_t *pdata = ae->_auto.win_weights + i;

		RTS_OPT("%3d", *pdata);
		if (i % 16 != 15 && i != ae->window_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
	}
	RTS_INFO("target_delta : %d\n", ae->_auto.target_delta);
	RTS_INFO("gain_max : %d\n", ae->_auto.gain_max);
	RTS_INFO("min_fps : %d\n", ae->_auto.min_fps);
	RTS_INFO("statis:\n");
	RTS_INFO("y_mean : %d\n", ae->statis.y_mean);
	RTS_INFO("window y_means:\n");
	for (i = 0; i < ae->window_num; i++) {
		uint16_t *pdata = ae->statis.win_y_means + i;

		RTS_OPT("%3d", *pdata);
		if (i % 16 != 15 && i != ae->window_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
	}
	RTS_INFO("histogram:\n");
	for (i = 0; i < ae->histogram_num; i++) {
		uint32_t *pdata = ae->statis.histogram_info + i;

		RTS_OPT("%5d", *pdata);
		if (i % 16 != 15 && i != ae->histogram_num - 1)
			RTS_OPT(" ");
		else
			RTS_OPT("\n");
	}
}

static void *ctrl_ae(void *arg)
{
	struct rts_isp_ae_ctrl *ae;
	int ret;

	/*wait 100ms after enable isp channel to query ae*/
	usleep(100 * 1000);

	ret = rts_av_query_isp_ae(&ae);
	if (ret) {
		RTS_ERR("query isp ae ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	switch(rts_cmd) {
	case 1:
		RTS_INFO("set ae auto\n");
		ret = set_ae_auto(ae);
		break;
	case 2:
		RTS_INFO("set total ae gain\n");
		ret = set_total_ae_gain(ae);
		break;
	case 3:
		RTS_INFO("set ae gain\n");
		ret = set_ae_gain(ae);
		break;
	case 4:
		RTS_INFO("set ae exposure time\n");
		ret = set_ae_exposure_time(ae);
		break;
	case 5:
		RTS_INFO("set ae target delta\n");
		ret = set_ae_target_delta(ae);
		break;
	case 6:
		RTS_INFO("set ae gain max\n");
		ret = set_ae_gain_max(ae);
		break;
	case 7:
		RTS_INFO("set ae min fps\n");
		ret = set_ae_min_fps(ae);
		break;
	case 8:
		RTS_INFO("set ae weight\n");
		ret = set_ae_weight(ae);
		break;
	default:
		RTS_INFO("set ae auto\n");
		ret = set_ae_auto(ae);
		break;
	}
	if (ret) {
		RTS_ERR("ae ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_get_isp_ae(ae);
	if (ret) {
		RTS_ERR("get isp ae ctrl fail, ret = %d\n", ret);
		goto exit;
	}

	print_ae_ctrl(ae);

	while (!g_exit)
		usleep(1000);
exit:
	RTS_SAFE_RELEASE(ae, rts_av_release_isp_ae);
	RTS_INFO("quit ae control thread\n");
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
		snprintf(outfile, sizeof(outfile), "%s/ae_out.%s",
					save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file ae_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc_chn);
	rts_av_start_recv(enc_chn);

	pthread_create(&tid, NULL, ctrl_ae, NULL);

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
