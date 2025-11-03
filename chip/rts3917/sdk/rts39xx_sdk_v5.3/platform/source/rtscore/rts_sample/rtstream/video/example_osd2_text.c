/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtscamkit.h>
#include <getopt.h>
#include "example_osdi_text.h"

#define TIME_OSD_INTERVAL	(995 * 1000)	// a little greater than 1s - OSD cost time (according to OSD size)

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

struct stream_info {
	int vin;
	int enc;
	int osd;
};

static int g_exit;
static int rts_enc_type;
static char rts_enc_name[10];
static int rts_num;
static char *save_dir;
static struct rts_osd2_attr *osd2_attr;
static pthread_t tid_refresh;
static int rts_ret;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{0, 0, 0, 0}
};

static void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tan example for example_osd2_text\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_osd2_text [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_osd2_text -e h265 -s /mnt\n");
}

static void Termination(int sign)
{
	g_exit = 1;
}

static void *update_osd2_timedate(void *arg)
{
	char timedate[20];
	struct timeval tv;
	struct tm timeinfo;
	struct rts_osd_text_cfg textcfg;
	struct rts_osd2_block *block;
	int ret = 0;

	while (!g_exit) {
		gettimeofday(&tv, NULL);
		localtime_r(&tv.tv_sec, &timeinfo);
		strftime(timedate, sizeof(timedate), "%Y-%m-%d %H:%M:%S", &timeinfo);

		textcfg.text = timedate;
		textcfg.textlen = strlen(timedate);
		textcfg.target_height = HEIGHT;
		textcfg.rotate = RTS_AV_ROTATION_0;

		block = &osd2_attr->blocks[0];
		block->rect.left = 0;
		block->rect.top = 0;
		block->enable = 1;
		ret = rts_av_osd2_text_set(osd2_attr, 0, &textcfg);
		if (ret < 0) {
			RTS_ERR("set osd2 text fail, ret = %d\n", ret);
			break;
		}

		if (tv.tv_usec < TIME_OSD_INTERVAL)
			usleep(TIME_OSD_INTERVAL - tv.tv_usec);
	}
	rts_ret = ret;
	g_exit = 1;

	return NULL;
}

static int show_custom_text(void)
{
	struct rts_osd_text_cfg textcfg;
	struct rts_osd2_block *block;
	int ret;

	textcfg.text = "通道 1";
	textcfg.textlen = strlen(textcfg.text);
	textcfg.target_height = 32;
	textcfg.rotate = RTS_AV_ROTATION_0;

	block = &osd2_attr->blocks[2];
	block->rect.left = 1180;
	block->rect.top = 0;
	block->enable = 1;
	ret = rts_av_osd2_text_set(osd2_attr, 2, &textcfg);
	if (ret < 0)
		RTS_ERR("set osd2 text fail, ret = %d\n", ret);

	return ret;
}

static int osd2_run(struct stream_info strm_info)
{
	int ret;
	struct rts_osd_text_attr text_attr = {0};

	ret = rts_av_query_osd2(strm_info.osd, &osd2_attr);
	if (ret) {
		RTS_ERR("query osd2 attr fail\n");
		return ret;
	}

	/* configure ASCII font lib attribute */
	text_attr.tagcode_asc = (uint16_t *)fonttag_asc;
	text_attr.taglen_asc = taglength_asc;
	text_attr.font_asc = (uint8_t *)fontlib_asc;
	text_attr.height = HEIGHT;
	text_attr.width_asc = WIDTH;
	text_attr.fmt = RTS_OSD2_BLK_FMT_RGBA2222;

	/* configure chinese font lib attribute */
	text_attr.tagcode_chi = (uint16_t *)fonttag_chi;
	text_attr.taglen_chi = taglength_chi;
	text_attr.font_chi = (uint8_t *)fontlib_chi;
	text_attr.width_chi = WIDTH_CH;

	ret = rts_av_osd2_text_config(osd2_attr, &text_attr);
	if (ret) {
		RTS_ERR("config osd2 text fail\n");
		return ret;
	}

	ret = show_custom_text();
	if (ret)
		return ret;
	pthread_create(&tid_refresh, NULL, update_osd2_timedate, NULL);

	return ret;
}

static void osd2_finish(void)
{
	if (tid_refresh)
		pthread_join(tid_refresh, NULL);

	RTS_SAFE_RELEASE(osd2_attr, rts_av_release_osd2);
}

static int create_h264_encode_chn(void)
{
	struct rts_h264_attr h264_attr = {0};

	h264_attr.level = H264_LEVEL_4;
	h264_attr.rotation = RTS_AV_ROTATION_0;
	h264_attr.mirror = RTS_AV_MIRROR_NO;

	return rts_av_create_h264_chn(&h264_attr);
}

static int create_h265_encode_chn(void)
{
	struct rts_h265_attr h265_attr = {0};

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	h265_attr.mirror = RTS_AV_MIRROR_NO;

	return rts_av_create_h265_chn(&h265_attr);
}

static int create_encode_chn(void)
{
	if (rts_enc_type == RTS_ENC_H264)
		return create_h264_encode_chn();
	else if (rts_enc_type == RTS_ENC_H265)
		return create_h265_encode_chn();
}

static int test_stream(int isp_id)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	struct stream_info strm_info;
	pthread_t tid;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int ret;
	char outfile[100];

	vin_attr.vin_id = isp_id;
	vin_attr.vin_buf_num = 2;
	vin_attr.vin_mode = RTS_AV_VIN_FRAME_MODE;
	strm_info.vin = rts_av_create_vin_chn(&vin_attr);
	if (strm_info.vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", strm_info.vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", strm_info.vin);

	strm_info.osd = rts_av_create_osd_chn();
	if (strm_info.osd < 0) {
		RTS_ERR("fail to create osd chn, ret = %d\n", strm_info.osd);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	strm_info.enc = create_encode_chn();
	if (strm_info.enc < 0) {
		RTS_ERR("fail to create %s chn, ret = %d\n",
				rts_enc_name, strm_info.enc);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("enc chn : %d\n", strm_info.enc);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = 1280;
	profile.video.height = 720;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(strm_info.vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(strm_info.vin, strm_info.osd);
	if (ret) {
		RTS_ERR("fail to bind vin and osd, ret %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(strm_info.osd, strm_info.enc);
	if (ret) {
		RTS_ERR("fail to bind osd and encode, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/osd2.%s",
					save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file osd2.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(strm_info.vin);
	rts_av_enable_chn(strm_info.osd);
	rts_av_enable_chn(strm_info.enc);
	rts_av_start_recv(strm_info.enc);

	ret = osd2_run(strm_info);
	if (ret) {
		RTS_ERR("osd2 run fail, ret = %d\n", ret);
		goto exit;
	}

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(strm_info.enc, &buffer, 100))
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
	osd2_finish();
	ret = rts_ret;

	rts_av_stop_recv(strm_info.enc);
	rts_av_disable_chn(strm_info.vin);
	rts_av_disable_chn(strm_info.osd);
	rts_av_disable_chn(strm_info.enc);
	rts_av_unbind(strm_info.vin, strm_info.osd);
	rts_av_unbind(strm_info.osd, strm_info.enc);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (strm_info.vin >= 0) {
		rts_av_destroy_chn(strm_info.vin);
		strm_info.vin = -1;
	}
	if (strm_info.osd >= 0) {
		rts_av_destroy_chn(strm_info.osd);
		strm_info.osd = -1;
	}
	if (strm_info.enc >= 0) {
		rts_av_destroy_chn(strm_info.enc);
		strm_info.enc = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	int isp_id;
	int c;

	while ((c = getopt_long(argc, argv,
				":he:s:n:", longopts, NULL)) != -1) {
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
		case ':':
			printf("required argument : -%c\n", optopt);
			return -1;
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
	isp_id = 0;
	ret = test_stream(isp_id);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
