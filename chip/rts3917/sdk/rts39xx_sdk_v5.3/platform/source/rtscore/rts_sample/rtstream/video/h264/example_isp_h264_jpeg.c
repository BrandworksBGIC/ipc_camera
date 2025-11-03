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
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

/* code flow

               +---------+
+---------+    |         |recv
|         |===>| h264_ch |==========>test.h264
|         |    |         |
|  vin_ch |    +---------+
|         |    +---------+
|         |===>|         |callback
+---------+    | jpeg_ch |==========>xxx.jpeg
               |         |
               +---------+
*/

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

static char *save_dir;
static int rts_num;

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"num", required_argument, NULL, 'n'},
	{"save", required_argument, NULL, 's'},
	{0, 0, 0, 0}
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\texample for h264 and jpeg\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_isp_h264_jpeg [option]...");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_isp_h264_jpeg -s /mnt -n 10\n");
	fprintf(stdout, "\n");
}

static int __init_sys_vmem(void)
{
	int ret = 0;
	struct rts_sys_vmem_cfg cfg = {0};
	int status = 0;

	status = rts_av_sys_vmem_status();

	if (status == RTS_SYS_VMEM_STATUS_ON)
		goto out;

	/* 1-channel */
	cfg.stream[0].enable = 1;
	cfg.stream[0].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
	cfg.stream[0].width = 0;
	cfg.stream[0].height = 0;

	/* vin */
	cfg.stream[0].module[0].type = RTS_AV_ID_VIN;
	cfg.stream[0].module[0].cnt = 1;
	cfg.stream[0].module[0].mode = 1;

	/* h26x */
	cfg.stream[0].module[1].type = RTS_AV_ID_H264;
	cfg.stream[0].module[1].cnt = 1;
	cfg.stream[0].module[1].outbuf.setted = 1;
	cfg.stream[0].module[1].outbuf.shared = 0;
	cfg.stream[0].module[1].outbuf.num = 1;
	cfg.stream[0].module[1].outbuf.size = 0; // default size

	/* mjpeg */
	cfg.stream[0].module[2].type = RTS_AV_ID_MJPGENC;
	cfg.stream[0].module[2].cnt = 1;
	cfg.stream[0].module[2].outbuf.setted = 1;
	cfg.stream[0].module[2].outbuf.shared = 0;
	cfg.stream[0].module[2].outbuf.num = 1;
	cfg.stream[0].module[2].outbuf.size = 0; // default size

	ret = rts_av_sys_vmem_set_conf(&cfg);
	if (ret) {
		RTS_ERR("failed to set sysmem cfg, ret:%d\n", ret);
		return ret;
	}

	ret = rts_av_sys_vmem_init();
	if (ret) {
		RTS_ERR("failed to init sysmem cfg, ret:%d\n", ret);
		return ret;
	}

out:
	return ret;
}

static void __release_sys_vmem(void)
{
	int status = 0;

	status = rts_av_sys_vmem_status();

	if (status == RTS_SYS_VMEM_STATUS_OFF)
		return;

	rts_av_sys_vmem_release();
}

void save_mjpeg(void *priv, struct rts_av_profile *profile,
		struct rts_av_buffer *buffer)
{
	static uint64_t index;
	char outfile[64];
	FILE *pfile = NULL;

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/%lld.jpg",
				save_dir, index++);

		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open %s fail\n", outfile);
			return;
		}

		fwrite(buffer->vm_addr, 1, buffer->bytesused, pfile);

		RTS_SAFE_RELEASE(pfile, fclose);
	}
}

int test_stream(void)
{
	struct rts_h264_attr h264_attr = {0};
	struct rts_vin_attr vin_attr = {0};
	struct rts_jpgenc_attr jpg_attr = {0};
	struct rts_av_profile profile;
	struct rts_av_callback cb;

	FILE *pfile = NULL;
	uint32_t number = 0;
	int vin = -1;
	int h264 = -1;
	int jpg = -1;
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

	h264_attr.level = H264_LEVEL_4;
	h264_attr.rotation = 0;
	h264 = rts_av_create_h264_chn(&h264_attr);
	if (h264 < 0) {
		RTS_ERR("fail to create h264 chn, ret = %d\n", h264);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h264 chn : %d\n", vin);

	jpg_attr.rotation = 0;
	jpg = rts_av_create_mjpeg_chn(&jpg_attr);
	if (jpg < 0) {
		RTS_ERR("fail to create jpg chn, ret = %d\n", jpg);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("jpg chn : %d\n", jpg);

	ret = rts_av_bind(vin, h264);
	if (ret) {
		RTS_ERR("fail to bind vin and h264, ret %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(vin, jpg);
	if (ret) {
		RTS_ERR("fail to bind vin and mjpeg, ret %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/out.h264",
					save_dir);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open encode file out.h264 fail\n");
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	cb.func = save_mjpeg;
	cb.start = 0;
	cb.times = -1;
	cb.interval = 5;
	cb.type = RTS_AV_CB_TYPE_SYNC;
	cb.priv = NULL;
	ret = rts_av_set_callback(jpg, &cb, 0);
	if (ret) {
		RTS_ERR("fail to set mjpeg callback, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(vin);
	rts_av_enable_chn(h264);
	rts_av_enable_chn(jpg);
	rts_av_start_recv(h264);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(h264, &buffer, 100))
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

	rts_av_stop_recv(h264);
	rts_av_disable_chn(vin);
	rts_av_disable_chn(h264);
	rts_av_disable_chn(jpg);
	rts_av_unbind(vin, h264);
	rts_av_unbind(vin, jpg);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (vin >= 0) {
		rts_av_destroy_chn(vin);
		vin = -1;
	}
	if (h264 >= 0) {
		rts_av_destroy_chn(h264);
		h264 = -1;
	}
	if (jpg >= 0) {
		rts_av_destroy_chn(jpg);
		jpg = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret;
	int c;

	while ((c = getopt_long(argc, argv,
				":hn:s:", longopts, NULL)) != -1) {
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

	__init_sys_vmem();

	ret = test_stream();

	__release_sys_vmem();

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return RTS_OK;
}
