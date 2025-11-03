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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>

struct option longopts[] = {
	{"exec", required_argument, NULL, 'c'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\texample for pre-alloc rtstream video memory\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_pre_alloc [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-c, --exec\t0-alloc, 1-free\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_pre_alloc -c 0 : pre-alloc memory\n");
	fprintf(stdout, "\texample_pre_alloc -c 1 : pre-free memory\n");
	fprintf(stdout, "\n");
}

/*
 * 2304x1296 yuv420sp vin osd h265 mjpeg
 * 640x480 yuv420sp vin osd h264
 */
int main(int argc, char *argv[])
{
	int ret = 0, c;
	struct rts_sys_vmem_cfg cfg = {0};
	int state = 0;
	int status = 0;

	while ((c = getopt_long(argc, argv,
				"hc:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'c':
			state = (uint32_t)strtol(optarg, NULL, 0);
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

	status = rts_av_sys_vmem_status();

	if (state == 0) {
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

		/* h265 */
		cfg.stream[0].module[2].type = RTS_AV_ID_H265;
		cfg.stream[0].module[2].cnt = 1;
		cfg.stream[0].module[2].outbuf.setted = 1;
		cfg.stream[0].module[2].outbuf.shared = 0;
		cfg.stream[0].module[2].outbuf.num = 1;
		cfg.stream[0].module[2].outbuf.size = 0; // default size

		/* mjpeg */
		cfg.stream[0].module[3].type = RTS_AV_ID_MJPGENC;
		cfg.stream[0].module[3].cnt = 1;
		cfg.stream[0].module[3].outbuf.setted = 1;
		cfg.stream[0].module[3].outbuf.shared = 0;
		cfg.stream[0].module[3].outbuf.num = 1;
		cfg.stream[0].module[3].outbuf.size = 0; // default size

		/* 2-channel */
		cfg.stream[1].enable = 1;
		cfg.stream[1].fmt = RTS_V_FMT_YUV420SEMIPLANAR;
		cfg.stream[1].width = 0;
		cfg.stream[1].height = 0;

		/* vin */
		cfg.stream[1].module[0].type = RTS_AV_ID_VIN;
		cfg.stream[1].module[0].cnt = 1;
		cfg.stream[1].module[0].mode = 0;
		cfg.stream[1].module[0].outbuf.num = 2;
		cfg.stream[1].module[0].outbuf.setted = 1;

		/* h265 */
		cfg.stream[1].module[2].type = RTS_AV_ID_H264;
		cfg.stream[1].module[2].cnt = 1;
		cfg.stream[1].module[2].outbuf.setted = 1;
		cfg.stream[1].module[2].outbuf.shared = 0;
		cfg.stream[1].module[2].outbuf.num = 1;
		cfg.stream[1].module[2].outbuf.size = 0; // default size

		char *dev = "/dev/video61";

		if (access(dev, 0) == 0) {
			/* 3-channel */
			cfg.stream[2].enable = 1;
			cfg.stream[2].fmt = RTS_V_FMT_RGB;
			cfg.stream[2].width = 0;
			cfg.stream[2].height = 0;

			/* vin */
			cfg.stream[2].module[0].type = RTS_AV_ID_VIN;
			cfg.stream[2].module[0].cnt = 1;
			cfg.stream[2].module[0].mode = 0;
			cfg.stream[2].module[0].outbuf.num = 2;
			cfg.stream[2].module[0].outbuf.setted = 1;
		}

		ret = rts_av_sys_vmem_set_conf(&cfg);
		if (ret) {
			RTS_ERR("failed to set sysmem cfg, ret:%d\n", ret);
			goto out;
		}

		ret = rts_av_sys_vmem_init();
		if (ret) {
			RTS_ERR("failed to init sysmem cfg, ret:%d\n", ret);
			goto out;
		}
	} else {
		if (status == RTS_SYS_VMEM_STATUS_OFF)
			goto out;

		rts_av_sys_vmem_release();
	}
out:
	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
