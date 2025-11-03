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
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <getopt.h>
#include <pthread.h>

/* code flow

+---------+         +---------+
|         |         |         |
|  vin_ch |========>|encode_ch|========>receive_block=====>mask_out.h265
|         |         |         |
+---------+         +---------+

+----------------+          +-------------------+
|                |          |                   |
| thread-set_mask|=========>|  rts_av_set_mask  |
|                |          |                   |
+----------------+          +-------------------+

*/

enum {
	RTS_ENC_H264 = 0,
	RTS_ENC_H265,
};

static int rts_enc_type;
static char rts_enc_name[10];
static char *save_dir;
static int rts_num;
static int rts_ret;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"enc", required_argument, NULL, 'e'},
	{"idx", required_argument, NULL, 'i'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
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
	fprintf(stdout, "\texample to video mask\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_mask [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-i, --idx\tidx [0, 4]\n");
	fprintf(stdout, "-e, --enc\tencode type (h264 h265)\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_mask -e h265/h264 -i 1 -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

int set_grid(struct rts_video_grid *grid)
{
	struct rts_video_grid_bitmap *bitmap;
	int grid_num;
	int length;

	RTS_ASSERT(grid);

	RTS_INFO("%d\n", sizeof(*grid));

	grid->start.x = 0;
	grid->start.y = 0;
	grid->cell.width = 32;
	grid->cell.height = 24;
	grid->size.rows = 30;
	grid->size.columns = 40;

	grid_num = grid->size.rows * grid->size.columns;
	length = RTS_DIV_ROUND_UP(grid_num, 8);

	bitmap = &grid->bitmap;
	memset(bitmap->vm_addr, 0xf, length);

	RTS_INFO("%d %d %d %d %d %d\n", grid->start.x, grid->start.y,
		 grid->cell.width, grid->cell.height,
		 grid->size.rows, grid->size.columns);

	return RTS_OK;
}

int set_rect(struct rts_video_rect *rect, int idx)
{
	RTS_ASSERT(rect);

	rect->start.x = 340 * ((idx - 1) % 2);
	rect->start.y = 260 * ((idx - 1) / 2);
	rect->end.x = rect->start.x + 320;
	rect->end.y = rect->start.y + 240;

	return RTS_OK;
}

int enable_mask(struct rts_mask_attr *attr, int idx, int enable)
{
	struct rts_mask_block *block;
	int ret;

	RTS_ASSERT(attr);

	if (attr->number == 0)
		return -1;

	if (idx >= attr->number || idx < 0)
		return -1;

	ret = rts_av_get_mask(attr);
	if (ret) {
		RTS_ERR("get mask attr fail, ret = %d\n");
		return ret;
	}

	block = attr->blocks + idx;

	RTS_INFO("%d %d\n", block->type, block->supported_grid_num);

	block->enable = 0;
	if (enable) {
		switch (block->type) {
		case RTS_BLK_TYPE_GRID:
			set_grid(&block->area);
			block->enable = 1;
			break;
		case RTS_BLK_TYPE_RECT:
			set_rect(&block->rect, idx);
			block->enable = 1;
			break;
		}
	}

	return rts_av_set_mask(attr);
}
static void *set_mask(void *arg)
{
	int idx = 1;
	int ret;
	struct rts_mask_attr *attr = NULL;

	idx = *((int *)arg);

	ret = rts_av_query_mask(&attr, 1280, 720);
	if (ret) {
		RTS_ERR("query isp mask attr fail, ret = %d\n", ret);
		goto exit;
	}

	RTS_INFO("mask block number : %d\n", attr->number);

	ret = enable_mask(attr, idx, 1);
	if (ret) {
		RTS_ERR("enable mask fail, ret = %d\n", ret);
		goto exit;
	}

	while (!g_exit)
		usleep(1000);

	ret = enable_mask(attr, idx, 0);
	if (ret) {
		RTS_ERR("disable mask fail, ret = %d\n", ret);
		goto exit;
	}
exit:
	RTS_SAFE_RELEASE(attr, rts_av_release_mask);
	RTS_INFO("quit mask thread\n");
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

int test_stream(int idx)
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

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/mask_out.%s",
					save_dir, rts_enc_name);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file mask_out.%s fail\n",
				rts_enc_name);
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(enc);
	rts_av_start_recv(enc);

	pthread_create(&tid, NULL, set_mask, &idx);

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

		if (rts_num > 0 && number >= rts_num)
			break;
	}

	g_exit = 1;
	pthread_join(tid, NULL);
	if (rts_ret)
		ret = rts_ret;

	rts_av_stop_recv(enc);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(enc);
	rts_av_unbind(vin, enc);

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
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int c;
	int ret;
	int idx = 1;

	if (argc < 2) {
		printf("need more parameters\n");
		printf("use -h to get help info\n");
		return -1;
	}

	while ((c = getopt_long(argc, argv,
				":he:i:s:n:", longopts, NULL)) != -1) {
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
		case 'i':
			idx = (int)strtol(optarg, NULL, 0);
			break;
		case 's':
			save_dir = optarg;
			break;
		case 'n':
			rts_num = (uint32_t)strtol(optarg, NULL, 0);
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

	ret = test_stream(idx);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
