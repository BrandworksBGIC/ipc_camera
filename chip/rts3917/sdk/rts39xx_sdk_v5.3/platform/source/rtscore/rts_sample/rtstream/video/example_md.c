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
#include <getopt.h>
#include <rtscamkit.h>
#include <rtsbmp.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <pthread.h>

/* code flow

+---------+         +---------+
|         |         |         |
|  vin_ch |========>|encode_ch|========>recv_block
|         |         |         |
+---------+         +---------+         +----------+
                                        |          |
+---------+         +---------+callback |received  |
|         |         |         |========>|(md_event)|
| thread- |         |         |         |          |
| set_md  |========>|enable_md|         +----------+
|         |         |         |callback +-------------+
+---------+         |         |========>|             |
                    +---------+         |process_data |
                                        |(md_bmp_data)|
                                        |             |
                                        +-------------+
*/

static int g_exit;

#define _RES_W		1280
#define _RES_H		720
static int GRID_R = 72;
static int GRID_C = 128;

struct __bmp_data {
	uint8_t *vm_addr;
	uint32_t length;
	uint32_t width;
	uint32_t height;
};

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type = -1;
static char rts_enc_name[10];
static char *save_dir;

struct option longopts[] = {
	{"poll", no_argument, NULL, 'p'},
	{"trig", no_argument, NULL, 't'},
	{"data", required_argument, NULL, 'd'},
	{"enc", required_argument, NULL, 'e'},
	{"save", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

struct md_option {
	int polling;
	int trig;
	uint32_t data_mode_mask;
};

static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout,
		"\texample for motion detect, only for RTS3903/RTS3913\n");
	fprintf(stdout, "\tsave the piture as bmp file");
	fprintf(stdout, " when IPC detected motion\n");
	fprintf(stdout, "\tuse ctrt + c to quit\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_md [OPTION]...[VALUE]...\n");
	fprintf(stdout, "\tp--enable polling\n");
	fprintf(stdout, "\tt-- enable trig\n");
	fprintf(stdout, "\td--set data_mode_mask\n");
	fprintf(stdout, "\te--set encode type (h264 h265)\n");
	fprintf(stdout, "\ts--save frame at <dir>\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_md -e h265/h264 -d 1 -s /mnt\n");
	fprintf(stdout, "\texample_md -e h265/h264 -p\n");
	fprintf(stdout, "\texample_md -e h265/h264 -t\n");
}

int motioned(int idx, void *priv)
{
	RTS_INFO("motion detected\n");

	return RTS_OK;
}

int print_data(struct rts_md_type_data *md_data)
{
	struct rts_md_data *data;
	uint8_t *ptr;
	int i;
	int j;

	RTS_ASSERT(md_data);

	if (!md_data->data)
		return RTS_OK;

	data = md_data->data;
	ptr = data->vm_addr;

	for (j = 0; j < GRID_R; j++) {
		RTS_OPT("[%4d]", j);
		for (i = 0; i < GRID_C; i++) {
			int x;
			int y;
			int index = j * GRID_C + i;
			uint8_t mask = 0;
			int bpp = data->bpp;
			int val;

			switch (bpp) {
			case 1:
				mask = 0x1;
				break;
			case 2:
				mask = 0x3;
				break;
			case 4:
				mask = 0xf;
				break;
			case 8:
				mask = 0xff;
				break;
			}
			y = (index * bpp) % 8;
			x = (index * bpp - y) / 8;

			val = (ptr[x] >> y) & mask;


			RTS_OPT("%d", val);
		}
		RTS_OPT("\n");
	}

	return RTS_OK;
}

int copy_data(uint8_t *psrc, int bpp, int w, int h, uint32_t bpl, uint8_t *pdst)
{
	int i;
	int j;
	uint8_t *ptr = pdst;

	if (bpp == 8) {
		for (j = 0; j < h; j++) {
			memcpy(ptr, psrc + w * j, w);
			ptr += bpl;
		}
		return RTS_OK;
	}

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			uint8_t val = 0;
			uint8_t mask = 0;
			int x;
			int y;
			int index = w * j + i;
			int coef = 0;

			y = (index * bpp) % 8;
			x = (index * bpp - y) / 8;

			switch (bpp) {
			case 1:
				mask = 0x1;
				coef = 255;
				break;
			case 2:
				mask = 0x3;
				coef = 85;
				break;
			case 4:
				mask = 0xf;
				coef = 17;
				break;
			}
			val = (psrc[x] >> y) & mask;
			ptr[i] = val * coef;

		}
		ptr += bpl;
	}

	return RTS_OK;
}

int save_data(struct __bmp_data *bmp)
{
	struct rts_bmp_encin encin;
	char filename[64];
	static int index;

	if (!bmp || !bmp->vm_addr)
		return RTS_OK;

	encin.psrc = bmp->vm_addr;
	encin.length = bmp->length;
	encin.fmt = RTS_PIX_FMT_GRAY_8;
	encin.width = bmp->width;
	encin.height = bmp->height;
	encin.align = RTS_BMP_BITS_DATA_CONTINUOUS;

	snprintf(filename, sizeof(filename), "%d.bmp", index++);
	rts_bmp_save(&encin, filename);

	return RTS_OK;
}

int process_data(struct rts_md_result *result,
		 struct __bmp_data *bmp)
{
	unsigned int i;
	uint32_t bytesused = 0;

	if (!result)
		return RTS_OK;

	if (!bmp)
		return RTS_OK;

	if (!bmp->vm_addr)
		return RTS_OK;

	memset(bmp->vm_addr, 0x0, bmp->length);

	RTS_INFO("----count = %d----\n", result->count);

	for (i = 0; i < result->count; i++) {
		struct rts_md_type_data *md_data = result->results + i;
		struct rts_md_data *data;

		if (!md_data->data)
			continue;

		data = md_data->data;
		if (bytesused + GRID_C * GRID_R > bmp->length)
			return RTS_OK;

		copy_data(data->vm_addr, data->bpp, GRID_C, GRID_R, bmp->width,
			  bmp->vm_addr + bytesused);

		bytesused += GRID_C * (GRID_R + 1);
	}

	save_data(bmp);

	return RTS_OK;
}

int received(int idx, struct rts_md_result *result, void *priv)
{
	if (!result)
		return RTS_RETURN(RTS_E_NULL_POINT);

	RTS_INFO("motion data received\n");

	process_data(result, priv);

	return RTS_OK;
}

int enable_md(struct rts_md_attr *attr, int polling, int trig,
	      uint32_t data_mode_mask,
	      struct __bmp_data *bmp)
{
	int i;
	int enable = 0;
	int ret;

	RTS_ASSERT(attr);

	if (!attr->number)
		return RTS_RETURN(RTS_FAIL);

	for (i = 0; i < attr->number; i++) {
		struct rts_md_block *block = attr->blocks + i;
		uint32_t detect_mode;
		int len;

		if (trig)
			detect_mode = RTS_MD_DETECT_USER_TRIG;
		else
			detect_mode = RTS_MD_DETECT_HW;

		block->enable = 0;
		if (i > 0)
			continue;

		RTS_INFO("%x %x %d\n",
			 block->supported_data_mode,
			 block->supported_detect_mode,
			 block->supported_grid_num);

		data_mode_mask &= block->supported_data_mode;
		if (!RTS_CHECK_BIT(block->supported_detect_mode, detect_mode)) {
			RTS_ERR("detect mode %x is not support\n", detect_mode);
			continue;
		}
		if (GRID_R * GRID_C > block->supported_grid_num) {
			RTS_ERR("grid size (%d,%d) is out of range\n",
				GRID_R, GRID_C);
			continue;
		}

		len = RTS_DIV_ROUND_UP(GRID_R * GRID_C, 8);

		block->data_mode_mask = data_mode_mask;
		block->detect_mode = detect_mode;
		block->area.start.x = 0;
		block->area.start.y = 0;
		block->area.cell.width = _RES_W / GRID_C;
		block->area.cell.height = _RES_H / GRID_R;
		block->area.size.rows = GRID_R;
		block->area.size.columns = GRID_C;

		memset(block->area.bitmap.vm_addr, 0xff, len);

		block->sensitivity = 80;
		block->percentage = 30;
		block->frame_interval = 5;

		if (bmp) {
			uint32_t length = 0;
			unsigned int count;

			count = rts_memweight((uint8_t *)&data_mode_mask,
					      sizeof(data_mode_mask));

			length = GRID_C * (GRID_R + 1) * count;
			bmp->vm_addr = rts_calloc(1, length);
			if (bmp->vm_addr) {
				bmp->length = length;
				bmp->width = GRID_C;
				bmp->height = (GRID_R + 1) * count;
			}
		}

		if (!polling) {
			block->ops.motion_detected = motioned;
			block->ops.motion_received = received;
			block->ops.priv = bmp;
		}

		block->enable = 1;
		enable++;
	}

	ret = rts_av_set_md(attr);
	if (ret)
		return ret;

	if (!enable)
		return RTS_RETURN(RTS_FAIL);

	return RTS_OK;
}

int disable_md(struct rts_md_attr *attr)
{
	int i;

	RTS_ASSERT(attr);

	for (i = 0; i < attr->number; i++) {
		struct rts_md_block *block = attr->blocks + i;

		block->enable = 0;
	}

	return rts_av_set_md(attr);
}

static void *set_md(void *arg)
{
	struct rts_md_attr *attr = NULL;
	struct rts_md_result result;
	struct __bmp_data bmp;
	struct md_option mdopt;

	int status = 0;
	int ret;

	memset(&bmp, 0, sizeof(bmp));
	mdopt = *((struct md_option *)arg);

	ret = rts_av_query_md(&attr, _RES_W, _RES_H);
	if (ret) {
		RTS_ERR("query isp md attr fail, ret = %d\n", ret);
		goto exit;
	}

	ret = enable_md(attr, mdopt.polling,
			mdopt.trig, mdopt.data_mode_mask, &bmp);
	if (ret) {
		RTS_ERR("enable md fail\n");
		goto exit;
	}

	if (mdopt.polling) {
		int i;
		uint32_t mask = attr->blocks->data_mode_mask;

		rts_av_init_md_result(&result, mask);
		RTS_INFO("%d\n", result.count);
		for (i = 0; i < result.count; i++) {
			struct rts_md_type_data *pdata;

			pdata = result.results + i;
			RTS_INFO("0x%x\n", pdata->type);
		}
	}


	while (!g_exit) {
		usleep(10);

		if (!status) {
			if (mdopt.trig) {
				ret = rts_av_trig_md(attr, 0);
				if (ret)
					continue;
				if (mdopt.polling)
					status = 1;
			} else if (mdopt.polling) {
				status = rts_av_check_md_status(attr, 0);
			}
			continue;
		}

		ret = rts_av_get_md_result(attr, 0, &result);
		if (ret)
			continue;
		RTS_INFO("get data\n");
		process_data(&result, &bmp);
		status = 0;
	}

	if (mdopt.polling)
		rts_av_uninit_md_result(&result);

	disable_md(attr);
exit:
	RTS_INFO("quit md thread\n");
	RTS_SAFE_RELEASE(attr, rts_av_release_md);
	RTS_SAFE_DELETE(bmp.vm_addr);
	bmp.length = 0;
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

int test_stream(struct md_option mdopt)
{
	struct rts_vin_attr vin_attr = {0};
	struct rts_av_profile profile;
	pthread_t tid;

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

	pthread_create(&tid, NULL, set_md, &mdopt);

	rts_av_start_recv(enc);

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/md_out.%s",
				save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file md_out.%s fail\n",
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

	pthread_join(tid, NULL);

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
	struct md_option mdopt = {0};
	int c;
	int ret;

	mdopt.data_mode_mask = RTS_MD_DATA_TYPE_RLTCUR;
	rts_set_log_mask(RTS_LOG_MASK_CONS);

	signal(SIGINT, Termination);
	signal(SIGTERM, Termination);

	if (argc > 1)
		mdopt.polling = (int)strtol(argv[1], NULL, 0);

	mdopt.data_mode_mask = RTS_MD_DATA_TYPE_AVGY |
				RTS_MD_DATA_TYPE_RLTPRE |
				RTS_MD_DATA_TYPE_RLTCUR |
				RTS_MD_DATA_TYPE_BACKY |
				RTS_MD_DATA_TYPE_BACKF |
				RTS_MD_DATA_TYPE_BACKC;

	while ((c = getopt_long(argc, argv,
				":phtd:e:s:", longopts, NULL)) != -1) {
		switch (c) {
		case 'p':
			mdopt.polling = 1;
			break;
		case 't':
			mdopt.trig = 1;
			break;
		case 'd':
			mdopt.data_mode_mask =
					(uint32_t)strtol(optarg, NULL, 0);
			break;
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
		case 's':
			save_dir = optarg;
			break;
		case 'h':
			print_help_info();
			return RTS_OK;
		case ':':
			RTS_ERR("required argument : -%c\n", optopt);
			break;
		case '?':
			RTS_ERR("invalid param: -%c\n", optopt);
			break;
		}
	}

	if (rts_enc_type < 0) {
		RTS_INFO("please assign encode type(-e h264/h265)\n");
		return RTS_OK;
	}

	ret = rts_av_init();
	if (ret) {
		RTS_ERR("rts_av_init fail\n");
		return ret;
	}

	test_stream(mdopt);

	rts_av_release();

	return ret;
}
