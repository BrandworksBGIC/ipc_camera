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

+---------+         +---------+         +---------+
|         |         |         |         |         |
|  vin_ch |========>|  osd_ch |========>|encode_ch|========>receive_block=====>rts_av_set_osd2_single ===> osd_out.h265
|         |         |         |         |         |
+---------+         +---------+         +---------+

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
	int osd;
};

struct osd2_info {
	struct strm stream;
	char filename[128];
	struct rts_osd2_attr *attr;
	enum RTS_OSD2_BLK_FMT mode;
	uint32_t width;
	uint32_t height;
	uint8_t *buf;
	uint32_t len;
	int isp_width;
	int isp_height;
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
	fprintf(stdout, "\tan example to for example_osd2_pict\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_osd2_pict [option]...\n");
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
	fprintf(stdout, "\texample_osd2_pict "
			"-p 1bpp.pict 640 480 0 -e h265 -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

static void Termination(int sign)
{
	g_exit = 1;
}

enum RTS_OSD2_BLK_FMT __get_mode_val(int mode)
{
	switch (mode) {
	case 0:
				return RTS_OSD2_BLK_FMT_1BPP;
	case 1:
		return RTS_OSD2_BLK_FMT_RGBA1111;
	case 2:
		return RTS_OSD2_BLK_FMT_RGBA2222;
	case 3:
		return RTS_OSD2_BLK_FMT_RGBA4444;
	case 4:
		return RTS_OSD2_BLK_FMT_RGBA5551;
	case 5:
		return RTS_OSD2_BLK_FMT_RGBA8888;
	default:
		return -1;
	}
}

int __set_osd_attr(struct osd2_info *osd2)
{
	int mode;
	struct rts_osd2_attr *attr;

	if (!osd2)
		return RTS_RETURN(RTS_E_NULL_POINT);

	attr = osd2->attr;

	mode = __get_mode_val(osd2->mode);
	if (mode < 0) {
		RTS_ERR("block color mode is invalid\n");
		return mode;
	}

	attr->blocks->picture.length = osd2->len;
	attr->blocks->picture.pdata = osd2->buf;
	attr->blocks->picture.pixel_fmt = mode;

	attr->blocks->rect.left = 0;
	attr->blocks->rect.top = 0;
	attr->blocks->rect.right = osd2->width;
	attr->blocks->rect.bottom = osd2->height;

	attr->blocks->enable = RTS_TRUE;

	return rts_av_set_osd2_single(attr, 0);
}

int enable_osd(struct osd2_info *osd2)
{
	FILE *fp = NULL;
	int ret = -1;

	if (!osd2)
		return RTS_RETURN(RTS_E_NULL_POINT);

	fp = fopen(osd2->filename, "rb");
	if (!fp) {
		RTS_ERR("open failed\n");
		return RTS_RETURN(RTS_E_OPEN_FAIL);
	}

	ret = fseek(fp, 0l, SEEK_END);
	if (ret) {
		RTS_ERR("seek failed\n");
		goto exit;
	}

	osd2->len = ftell(fp);
	if (osd2->len < 0) {
		RTS_ERR("ftell failed\n");
		ret = osd2->len;
		goto exit;
	}

	ret = fseek(fp, 0l, SEEK_SET);
	if (ret) {
		RTS_ERR("seek failed\n");
		goto exit;
	}

	osd2->buf = (uint8_t *)calloc(1, osd2->len);
	if (!osd2->buf) {
		RTS_ERR("malloc block pdata fail\n");
		ret = RTS_RETURN(RTS_E_NULL_POINT);
		goto exit;
	}

	if (fread(osd2->buf, sizeof(char), osd2->len, fp) != osd2->len) {
		RTS_ERR("read failed\n");
		ret = RTS_RETURN(RTS_E_READ_FAIL);
		goto exit;
	}

	ret = __set_osd_attr(osd2);
exit:
	RTS_SAFE_RELEASE(osd2->buf, free);
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

int test_stream(struct osd2_info *osd2)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	pthread_t tid;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int ret;
	char outfile[100];

	vin_attr.vin_id = 1;
	vin_attr.vin_buf_num = 2;
	vin_attr.vin_mode = RTS_AV_VIN_FRAME_MODE;
	osd2->stream.vin = rts_av_create_vin_chn(&vin_attr);
	if (osd2->stream.vin < 0) {
		RTS_ERR("fail to create vin chn, ret = %d\n", osd2->stream.vin);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("vin chn : %d\n", osd2->stream.vin);

	osd2->stream.osd = rts_av_create_osd_chn();
	if (osd2->stream.osd < 0) {
		RTS_ERR("fail to create osd chn, ret = %d\n", osd2->stream.osd);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("osd chn : %d\n", osd2->stream.osd);

	osd2->stream.enc = create_encode_chn();
	if (osd2->stream.enc < 0) {
		RTS_ERR("fail to create %s chn, ret = %d\n",
				rts_enc_name, osd2->stream.enc);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("enc chn : %d\n", osd2->stream.enc);

	profile.fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	profile.video.width = osd2->isp_width;
	profile.video.height = osd2->isp_height;
	profile.video.numerator = 1;
	profile.video.denominator = 15;
	ret = rts_av_set_profile(osd2->stream.vin, &profile);
	if (ret) {
		RTS_ERR("set vin profile fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(osd2->stream.vin, osd2->stream.osd);
	if (ret) {
		RTS_ERR("fail to bind vin and osd, ret %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(osd2->stream.osd, osd2->stream.enc);
	if (ret) {
		RTS_ERR("fail to bind osd and encode, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/osd2_out.%s",
					save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file osd2_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	ret = rts_av_query_osd2(osd2->stream.osd, &osd2->attr);
	if (ret < 0) {
		RTS_ERR("query osd2 attr fail\n");
		goto exit;
	}

	ret = enable_osd(osd2);
	if (ret < 0) {
		RTS_ERR("enable osd fail, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(osd2->stream.vin);
	rts_av_enable_chn(osd2->stream.enc);
	rts_av_enable_chn(osd2->stream.osd);

	rts_av_start_recv(osd2->stream.enc);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(osd2->stream.enc, &buffer, 100))
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

	rts_av_stop_recv(osd2->stream.enc);
	rts_av_disable_chn(osd2->stream.vin);
	rts_av_disable_chn(osd2->stream.enc);
	rts_av_disable_chn(osd2->stream.osd);
	rts_av_unbind(osd2->stream.vin, osd2->stream.osd);
	rts_av_unbind(osd2->stream.osd, osd2->stream.enc);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	RTS_SAFE_RELEASE(osd2->attr, rts_av_release_osd2);

	if (osd2->stream.vin >= 0) {
		rts_av_destroy_chn(osd2->stream.vin);
		osd2->stream.vin = -1;
	}
	if (osd2->stream.enc >= 0) {
		rts_av_destroy_chn(osd2->stream.enc);
		osd2->stream.enc = -1;
	}
	if (osd2->stream.osd >= 0) {
		rts_av_destroy_chn(osd2->stream.osd);
		osd2->stream.osd = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	int c;
	struct osd2_info osd2;

	if (argc < 2) {
		RTS_ERR("need more parameters\n");
		RTS_ERR("use -h to get help info\n");
		return -1;
	}

	memset(&osd2, 0, sizeof(osd2));
	osd2.isp_width = 640;
	osd2.isp_height = 360;
	while ((c = getopt_long(argc, argv,
			":hp:e:s:n:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'p':
			if (argc < optind + 3) {
				printf("need more parameters\n");
				printf("use -h to get help info\n");
				return -1;
			}
			strcpy(osd2.filename, optarg);
			osd2.width = (uint32_t)strtol(argv[optind++], NULL, 0);
			osd2.height = (uint32_t)strtol(argv[optind++], NULL, 0);
			osd2.mode = (int)strtol(argv[optind], NULL, 0);
			if ((osd2.width > osd2.isp_width) ||
					(osd2.height > osd2.isp_height)) {
				printf("isp res: %dx%d, but osd2 res: %dx%d\n",
					osd2.isp_width, osd2.isp_height,
					osd2.width, osd2.height);
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

	if (!osd2.filename || !osd2.width || !osd2.height) {
		printf("please assign pict file\n");
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

	ret = test_stream(&osd2);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
