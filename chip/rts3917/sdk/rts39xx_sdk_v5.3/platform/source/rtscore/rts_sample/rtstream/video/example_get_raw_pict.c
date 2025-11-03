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
#include <rtscamkit.h>
#include <getopt.h>
#include <rts_raw.h>
#include <rtsavisp.h>

/* code flow

+------------------------+    +--------------------------+    +-------------------------+
|                        |    |                          |    |                         |
| rts_init_raw_streaming |===>| rts_config_raw_streaming |===>| rts_start_raw_streaming |===>
|                        |    |                          |    |                         |
+------------------------+    +--------------------------+    +-------------------------+

+------------------------+    +------------------------+    +---------------------------+
|                        |    |                        |    |                           |
| rts_get_raw_streaming  |===>| rts_stop_raw_streaming |===>| rts_release_raw_streaming |
|                        |    |                        |    |                           |
+------------------------+    +------------------------+    +---------------------------+

*/
#define MAX_PATH_LEN	(128)

static char output[MAX_PATH_LEN];

struct option longopts[] = {
	{"fmt", required_argument, NULL, 'f'},
	{"save", no_argument, NULL, 's'},
	{"output", required_argument, 0, 'o'},
	{0, 0, 0, 0},
};

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n\n");
	fprintf(stdout, "\tan example to get raw pict\n");
	fprintf(stdout, "\texample_get_raw_pict -f fmt -o output [-s]\n");
	fprintf(stdout, "--save | -s		save raw frame as test.raw\n");
	fprintf(stdout, "--fmt | -f <fmt>	set raw format\n");
	fprintf(stdout, "--output | -o <output>	output file\n");
	fprintf(stdout, "\t\t\t0: final output;\n");
	fprintf(stdout, "\t\t\t1: after crop for short exp;\n");
	fprintf(stdout, "\t\t\t2: after crop for long exp;\n");
	fprintf(stdout, "\t\t\t3: before BLC for short exp\n");
	fprintf(stdout, "\t\t\t4: before BLC for long exp\n");
	fprintf(stdout, "\t\t\t5: after HDR fusion\n");
	fprintf(stdout, "\t\t\t6: after global tone mapping\n");
	fprintf(stdout, "\t\t\t7: after local tone mapping\n");
	fprintf(stdout, "\t\t\t8: before CCM\n");
	fprintf(stdout, "\t\t\t9: before UV tune\n");
	fprintf(stdout, "\t\t\t10: before fusion for long exp\n");
	fprintf(stdout, "\t\t\t11: before fusion for short exp\n");
	fprintf(stdout, "\t\t\t12: before fusion\n");
	fprintf(stdout, "\t\t\t13: after ae gain\n");
	fprintf(stdout, "\t\t\t14: after WDR\n");
	fprintf(stdout, "\t\t\t15: before yuv444to422\n");
	fprintf(stdout, "\t\t\t16: before mcrop\n");
	fprintf(stdout, "\t\t\t17: after mask\n");
	fprintf(stdout, "\t\t\t18: before awb gain\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_get_raw_pict -f 2 -s -o /tmp/test.raw\n");
}

static void __save_raw_data(struct rts_raw_t *praw)
{
	FILE *pfile = NULL;
	int save = 0;

	if (!praw)
		return;

	save = *(int *)praw->master;
	if (save) {
		pfile = fopen(output, "wb+");
		if (!pfile) {
			printf("Failed to open %s\n", output);
			printf("please make sure it's writeable\n");
			return;
		}

		fwrite(praw->header, 1, RTS_RAW_HEADER_LENGTH, pfile);
		fwrite(praw->pdata, 1, praw->length, pfile);
	}
	RTS_SAFE_RELEASE(pfile, fclose);
}

static int __get_raw_frame(int fmt, int save)
{
	struct rts_raw_t *praw = NULL;
	int ret = RTS_OK;

	ret = rts_init_raw_streaming(&praw, __save_raw_data, &save);
	if (RTS_IS_ERR(ret)) {
		printf("Failed to init raw,ret =%d\n", ret);
		return ret;
	}

	ret = rts_config_raw_streaming(fmt, praw);
	if (RTS_IS_ERR(ret)) {
		printf("Failed to config raw frame,ret = %d\n", ret);
		goto exit;
	}

	ret = rts_start_raw_streaming(praw);
	if (RTS_IS_ERR(ret)) {
		printf("Failed to start raw frame,ret = %d\n", ret);
		goto exit;
	}

	ret = rts_get_raw_streaming(praw);
	if (RTS_IS_ERR(ret))
		printf("Failed to get raw frame,ret = %d\n", ret);

	rts_stop_raw_streaming(praw);
exit:
	RTS_SAFE_RELEASE(praw, rts_release_raw_streaming);
	return ret;
}

static const char *get_hdr_str(enum rts_isp_hdr_mode hdr)
{
	switch (hdr) {
	case RTS_ISP_HDR_NONE:
		return "linear";
	case RTS_ISP_HDR_LINE_2TO1:
		return "hdr line 2to1";
	default:
		return "unknown mode";
	}
}

static int set_sensor_mode_hdr(int mode_id)
{
	struct rts_isp_sensor_modes modes;
	struct rts_isp_sensor_mode mode;
	int ret;

	ret = rts_av_isp_enum_sensor_modes(ISP0, &modes);
	if (ret) {
		printf("enum sensor modes fail [%d]\n", ret);
		return ret;
	}

	if (mode_id < 0 || mode_id >= modes.num) {
		printf("not support sensor mode: %d\n", mode_id);
		return RTS_RETURN(RTS_E_INVALID_TYPE);
	}

	ret = rts_av_isp_set_sensor_mode(ISP0,
				&modes.mode[mode_id]);
	if (ret) {
		printf("set sensor mode fail [%d]\n", ret);
		return ret;
	}

	ret = rts_av_isp_get_sensor_mode(ISP0, &mode);
	if (ret) {
		printf("get sensor mode fail [%d]\n", ret);
		return ret;
	}

	printf("current sensor mode:\n");
	printf("  %s -> %dx%d@%.3ffps\n",
			get_hdr_str(mode.hdr),
			mode.size.w, mode.size.h,
			mode.fps);

	return 0;
}

int main(int argc, char *argv[])
{
	int c;
	int fmt = 0;
	int save = 0;
	int n = 0;
	int ret;

	memset(output, 0, sizeof(output));

	while ((c = getopt_long(argc, argv,
				":hf:so:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'f':
			fmt = (int)strtol(optarg, NULL, 0);
			break;
		case 'o':
			n = snprintf(output, sizeof(output),
					"%s", strdup(optarg));
			if (n > MAX_PATH_LEN) {
				printf("the path of output is too long\n");
				return -1;
			}
			break;
		case 's':
			save = 1;
			break;
		case ':':
			printf("required argument : -%c\n", optopt);
			return -1;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
		default:
			print_help_info();
			break;
		}
	}

	if (output[0] == '\0') {
		print_help_info();
		return -1;
	}

	/* set sensor_mode to hdr */
	ret = set_sensor_mode_hdr(4);
	if (ret) {
		printf("set sensor mode to hdr failed\n");
		return ret;
	}

	ret = __get_raw_frame(fmt, save);
	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
