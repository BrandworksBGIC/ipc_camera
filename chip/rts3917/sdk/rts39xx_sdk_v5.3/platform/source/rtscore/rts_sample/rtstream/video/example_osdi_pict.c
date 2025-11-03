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
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <rtscolor.h>
#include <rtscamkit.h>
#include <getopt.h>

/* code flow

+---------+         +---------+
|         |         |         |
|  vin_ch |========>|encode_ch|========>receive_block=====>rts_av_set_osdi_single ===> osd_out.h265
|         |         |         |
+---------+         +---------+

*/

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"pict", required_argument, NULL, 'p'},
	{"enc", required_argument, NULL, 'e'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{0, 0, 0, 0}
};

struct strm {
	int vin;
	int enc;
};

struct osdi_info {
	struct strm stream;
	char filename[128];
	struct rts_osdi_attr *attr;
	enum RTS_OSDI_BLK_FMT mode;
	uint32_t width;
	uint32_t height;
	uint8_t *buf;
	uint32_t len;
	uint32_t isp_width;
	uint32_t isp_height;
};

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int g_exit;

static int rts_enc_type;
char rts_enc_name[10];
static int rts_num;
static char *save_dir;

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tan example to for example_osdi_pict\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_osdi_pict [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-p, --pict\t<picname width height mode>\n");
	fprintf(stdout, "\t<mode> can take a value of 0~5:\n");
	fprintf(stdout, "\t\t0: RTS_OSD2_BLK_FMT_1BPP\n");
	fprintf(stdout, "\t\t1: RTS_OSD2_BLK_FMT_RGBA1111\n");
	fprintf(stdout, "\t\t2: RTS_OSD2_BLK_FMT_RGBA2222\n");
	fprintf(stdout, "\t\t3: RTS_OSD2_BLK_FMT_RGBA4444\n");
	fprintf(stdout, "\t\t4: RTS_OSD2_BLK_FMT_RGBA5551\n");
	fprintf(stdout, "\t\t5: RTS_OSD2_BLK_FMT_RGBA8888\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_osdi_pict "
			"-p 1bpp.pict 640 480 0 -e h265 -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

static void Termination(int sign)
{
	g_exit = 1;
}

enum RTS_OSDI_BLK_FMT __get_mode_val(int mode)
{
	switch (mode) {
	case 0:
		return RTS_OSDI_BLK_FMT_1BPP;
	case 1:
		return RTS_OSDI_BLK_FMT_RGBA1111;
	case 2:
		return RTS_OSDI_BLK_FMT_RGBA2222;
	case 3:
		return RTS_OSDI_BLK_FMT_RGBA4444;
	case 4:
		return RTS_OSDI_BLK_FMT_RGBA5551;
	case 5:
		return RTS_OSDI_BLK_FMT_RGBA8888;
	default:
		return -1;
	}
}

int __set_osd_attr(struct osdi_info *osdi)
{
	uint32_t mode;
	struct rts_osdi_attr *attr;

	if (!osdi)
		return RTS_RETURN(RTS_E_NULL_POINT);

	attr = osdi->attr;

	mode = __get_mode_val(osdi->mode);
	if (mode < 0) {
		RTS_ERR("block color mode is invalid\n");
		return mode;
	}
	attr->blocks->enable = RTS_TRUE;
	attr->blocks->picture.pixel_fmt = mode;
	attr->blocks->picture.pdata = osdi->buf;
	attr->blocks->picture.length = osdi->len;
	attr->blocks->rect.right = osdi->width;
	attr->blocks->rect.bottom = osdi->height;
	attr->blocks->rect.left = 0;
	attr->blocks->rect.top = 0;

	return rts_av_set_osdi_single(attr, 0);
}

int enable_osd(struct osdi_info *osdi)
{
	FILE *fp = NULL;
	int ret = -1;

	if (!osdi)
		return RTS_RETURN(RTS_E_NULL_POINT);

	fp = fopen(osdi->filename, "rb");
	if (!fp) {
		RTS_ERR("open failed\n");
		return RTS_RETURN(RTS_E_OPEN_FAIL);
	}

	ret = fseek(fp, 0l, SEEK_END);
	if (ret) {
		RTS_ERR("seek failed\n");
		goto exit;
	}

	osdi->len = ftell(fp);
	if (osdi->len < 0) {
		RTS_ERR("ftell failed\n");
		ret = osdi->len;
		goto exit;
	}

	ret = fseek(fp, 0l, SEEK_SET);
	if (ret) {
		RTS_ERR("seek failed\n");
		goto exit;
	}

	osdi->buf = (uint8_t *)calloc(1, osdi->len);
	if (!osdi->buf) {
		RTS_ERR("malloc block pdata fail\n");
		ret = RTS_RETURN(RTS_E_NULL_POINT);
		goto exit;
	}

	if (fread(osdi->buf, sizeof(char), osdi->len, fp) != osdi->len) {
		RTS_ERR("read failed\n");
		ret = RTS_RETURN(RTS_E_READ_FAIL);
		goto exit;
	}

	ret = __set_osd_attr(osdi);
exit:
	RTS_SAFE_RELEASE(osdi->buf, free);
	RTS_SAFE_RELEASE(fp, fclose);

	return ret;
}

int create_h264_encode_chn(void)
{
	struct rts_h264_attr h264_attr = {0};

	h264_attr.level = H264_LEVEL_4;
	h264_attr.rotation = RTS_AV_ROTATION_0;
	h264_attr.mirror = RTS_AV_MIRROR_NO;

	return rts_av_create_h264_chn(&h264_attr);
}

int create_h265_encode_chn(void)
{
	struct rts_h265_attr h265_attr = {0};

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	h265_attr.mirror = RTS_AV_MIRROR_NO;

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

int test_stream(struct osdi_info *osdi)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	pthread_t tid;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int ret;
	char outfile[100];

	vin_attr.vin_id = 0;
	vin_attr.vin_buf_num = 1;
	vin_attr.vin_mode = RTS_AV_VIN_RING_MODE;
	osdi->stream.vin = rts_av_create_vin_chn(&vin_attr);
	if (osdi->stream.vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", osdi->stream.vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", osdi->stream.vin);

	osdi->stream.enc = create_encode_chn();
	if (osdi->stream.enc < 0) {
		RTS_ERR("fail to create %s chn, ret = %d\n",
				rts_enc_name, osdi->stream.enc);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("enc chn : %d\n", osdi->stream.enc);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = osdi->isp_width;
	profile.video.height = osdi->isp_height;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(osdi->stream.vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(osdi->stream.vin, osdi->stream.enc);
	if (ret) {
		RTS_ERR("fail to bind vin and encode, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/osdi_out.%s",
					save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file osdi_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	ret = rts_av_query_osdi(osdi->stream.vin, &osdi->attr);
	if (ret < 0) {
		RTS_ERR("query osdi attr fail\n");
		goto exit;
	}

	ret = enable_osd(osdi);
	if (ret < 0) {
		RTS_ERR("enable osd fail, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(osdi->stream.vin);
	rts_av_enable_chn(osdi->stream.enc);
	rts_av_start_recv(osdi->stream.enc);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(osdi->stream.enc, &buffer, 100))
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

	rts_av_stop_recv(osdi->stream.enc);
	rts_av_disable_chn(osdi->stream.vin);
	rts_av_disable_chn(osdi->stream.enc);
	rts_av_unbind(osdi->stream.vin, osdi->stream.enc);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	RTS_SAFE_RELEASE(osdi->attr, rts_av_release_osdi);

	if (osdi->stream.vin >= 0) {
		rts_av_destroy_chn(osdi->stream.vin);
		osdi->stream.vin = -1;
	}
	if (osdi->stream.enc >= 0) {
		rts_av_destroy_chn(osdi->stream.enc);
		osdi->stream.enc = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}


int main(int argc, char *argv[])
{
	int ret = -1;
	int c;
	struct osdi_info osdi;

	if (argc < 2) {
		RTS_ERR("need more parameters\n");
		RTS_ERR("use -h to get help info\n");
		return -1;
	}

	memset(&osdi, 0, sizeof(osdi));
	osdi.isp_width = 1280;
	osdi.isp_height = 720;
	while ((c = getopt_long(argc, argv,
			":hp:e:s:n:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'p':
			if (argc < optind + 3) {
				RTS_ERR("need more parameters\n");
				RTS_ERR("use -h to get help info\n");
				return -1;
			}
			strcpy(osdi.filename, optarg);
			osdi.width = (uint32_t)strtol(argv[optind++], NULL, 0);
			osdi.height = (uint32_t)strtol(argv[optind++], NULL, 0);
			osdi.mode = (int)strtol(argv[optind], NULL, 0);
			if ((osdi.width > osdi.isp_width) ||
					(osdi.height > osdi.isp_height)) {
				printf("isp res: %dx%d, but osdi res: %dx%d\n",
					osdi.isp_width, osdi.isp_height,
					osdi.width, osdi.height);
				return -1;
			}
			break;
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

	if (!osdi.filename || !osdi.width || !osdi.height) {
		RTS_INFO("please assign pict file\n");
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

	ret = test_stream(&osdi);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
