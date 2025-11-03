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
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <fcntl.h>
#include <errno.h>

/* code flow
                +----------+
                |          |
input.yuv  ===> | h265_ch  | ===> test.h265
                |          |
                +----------+
*/

enum dbg_bitrate_mode {
	DBG_BITRATE_MODE_CBR,
	DBG_BITRATE_MODE_C_VBR,
	DBG_BITRATE_MODE_VBR,
};

struct __dbg_option {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	int intraPeriod;
	enum dbg_bitrate_mode bitrate_mode;
	uint32_t EncBitrate;
	uint32_t max_bitrate;
	uint32_t min_bitrate;

	char *filename;
	uint32_t number;
	char *input_file;
};

#define RTS_DBG_RC_MODE                   0x1
#define RTS_DBG_MIN_BITRATE               0x2
#define RTS_DBG_MAX_BITRATE               0x3

struct option longopts[] = {
	{"size", required_argument, NULL, 'V'},
	{"save", required_argument, NULL, 's'},
	{"number", required_argument, NULL, 'n'},
	{"input", required_argument, NULL, 'i'},
	{"fps", required_argument, NULL, 'U'},
	{"intraPeriod", required_argument, NULL, '0'},
	{"rc_mode", required_argument, NULL, RTS_DBG_RC_MODE},
	{"min_bitrate", required_argument, NULL, RTS_DBG_MIN_BITRATE},
	{"max_bitrate", required_argument, NULL, RTS_DBG_MAX_BITRATE},
	{"bps", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
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
	fprintf(stdout, "\texample for input_yuv\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_h265_yuv");
	fprintf(stdout, " [-i file] [-s file] [--size size] [-n frame]\n");
	fprintf(stdout, "\t--help | -h                    print example_h265_yuv usage\n");
	fprintf(stdout, "\t--input | -i <file>            input file is encoded\n");
	fprintf(stdout, "\t--size <wxh>                   input frame resolution\n");
	fprintf(stdout, "\t--save | -s <file>             output file\n");
	fprintf(stdout, "\t--fps  <value>                 set frame rate\n");
	fprintf(stdout, "\t--intraPeriod  <value>         set interval between I frames\n");
	fprintf(stdout, "\t--rc_mode<0.cbr 1.cvbr 2.vbr>  set bitrate mode\n");
	fprintf(stdout, "\t--bps  <value>                 set encode bitrate for cbr\n");
	fprintf(stdout, "\t--min_bitrate  <value>         set encode bitrate for cvbr\n");
	fprintf(stdout, "\t--max_bitrate  <value>         set encode bitrate for cvbr\n");
	fprintf(stdout, "\t--number | -n <num>            number of encode for file\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_h265_yuv -i input.yuv --size 1920x1080 -s test.h265 -n 300\n");
}

static int __load_frame(FILE *fp, struct rts_av_buffer *buffer)
{
	int ret;

	if (!fp)
		return RTS_RETURN(RTS_E_NULL_POINT);

	ret = fread(buffer->vm_addr, 1, buffer->length, fp);
	if (ret == 0 && feof(fp)) {
		RTS_INFO("End of file!\n");
		return RTS_RETURN(RTS_E_EMPTY);
	} else if (ret < buffer->length) {
		return RTS_RETURN(RTS_E_INVALID_DATA);
	}

	buffer->bytesused = buffer->length;
	return RTS_OK;
}

int test_stream(struct __dbg_option *option)
{
	struct rts_h265_attr h265_attr;
	struct rts_av_profile profile;
	struct rts_av_callback cb;
	struct rts_h265_ctrl *pctrl = NULL;
	struct rts_av_buffer *input_buffer = NULL;
	int outfd = 0;
	FILE *input_pfile = NULL;
	FILE *output_pfile = NULL;
	uint32_t number = 0;
	int h265 = -1;
	int ret;

	h265_attr.level = H265_LEVEL_5;
	h265_attr.tier = 0;
	h265_attr.rotation = RTS_AV_ROTATION_0;
	h265_attr.mirror = RTS_AV_MIRROR_NO;
	h265 = rts_av_create_h265_chn(&h265_attr);
	if (h265 < 0) {
		RTS_ERR("fail to create h265 chn, ret = %d\n", h265);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("h265 chnno:%d\n", h265);

	profile.fmt = RTS_V_FMT_YUV420PLANAR;
	profile.video.width = option->width;
	profile.video.height = option->height;
	profile.video.numerator = 1;
	profile.video.denominator = option->fps;
	ret = rts_av_set_profile(h265, &profile);
	if (ret) {
		RTS_ERR("set isp profile fail, ret = %d\n", ret);
		goto exit;
	}

	if (!option->input_file) {
		RTS_ERR("no input file\n");
		goto exit;
	}

	input_pfile = fopen(option->input_file, "r");
	if (input_pfile)
		RTS_INFO("open %s to encode\n", option->input_file);

	input_buffer = rts_av_new_buffer(0);
	if (!input_buffer) {
		RTS_ERR("fail to alloc input buffer\n");
		goto exit;
	}

	input_buffer->length = option->width * option->height * 3 / 2;

	ret = rts_av_sys_vrm_alloc(input_buffer);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("buffer init fail!\n");
		goto exit;
	}

	ret = rts_av_set_buffer_profile(input_buffer, &profile);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("set buffer profile fail, ret = %d\n", ret);
		return ret;
	}

	ret = rts_av_query_h265_ctrl(h265, &pctrl);
	if (RTS_IS_ERR(ret))
		goto exit;

	if (option->bitrate_mode == DBG_BITRATE_MODE_VBR) {
		pctrl->bitrate_mode = RTS_BITRATE_MODE_VBR;
	} else if (option->bitrate_mode == DBG_BITRATE_MODE_CBR) {
		pctrl->bitrate_mode = RTS_BITRATE_MODE_CBR;
		pctrl->bitrate = option->EncBitrate;
	} else {
		pctrl->bitrate_mode = RTS_BITRATE_MODE_C_VBR;
		pctrl->min_bitrate = option->min_bitrate;
		pctrl->max_bitrate = option->max_bitrate;
	}

	pctrl->qp = 30;
	pctrl->gop = option->intraPeriod;
	ret = rts_av_set_h265_ctrl(pctrl);
	if (RTS_IS_ERR(ret))
		goto exit;

	ret = rts_av_start_recv(h265);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("start recv h265 fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_start_send(h265);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("start send h265 fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_enable_chn(h265);
	if (RTS_IS_ERR(ret)) {
		RTS_ERR("enable h265 fail, ret = %d\n", ret);
		goto exit;
	}

	if (option->filename) {
		RTS_INFO("save to %s\n", option->filename);
		outfd = open(option->filename,
			O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (outfd < 0) {
			RTS_ERR("open error!, errno = %d, %s\n",
					errno, strerror(errno));
			ret = outfd;
			goto exit;
		}
	}

	while (!g_exit) {
		struct rts_av_buffer *output_buffer = NULL;

		ret = __load_frame(input_pfile, input_buffer);
		if (ret == RTS_RETURN(RTS_E_EMPTY)) {
			RTS_INFO("read input file to end\n");
			break;
		} else if (ret) {
			RTS_ERR("read input file error!, ret = %d\n", ret);
			break;
		}

		ret = rts_av_sys_vrm_flush_cache(input_buffer);
		if (ret)
			RTS_ERR("flush cahce fail\n");

		ret = rts_av_send(h265, input_buffer);
		if (ret) {
			RTS_ERR("rts_av_send error!, ret = %d\n", ret);
			break;
		}

		usleep(1000);

		ret = rts_av_recv_block(h265, &output_buffer, 1000);
		if (ret != RTS_OK)
			continue;

		if (output_buffer) {
			ret = write(outfd, output_buffer->vm_addr,
				output_buffer->bytesused);
			if (ret != output_buffer->bytesused) {
				RTS_ERR("write error!, ret = %d\n", ret);
				break;
			}

			ret = fsync(outfd);
			if (ret != 0) {
				RTS_ERR("fsync error!, ret = %d\n", ret);
				break;
			}

			RTS_INFO("index: %d, frame size %d\n",
				number, output_buffer->bytesused);
			number++;

			RTS_SAFE_RELEASE(output_buffer, rts_av_put_buffer);
		}

		if (option->number && number >= option->number)
			break;
	}

	rts_av_stop_send(h265);
	rts_av_stop_recv(h265);
	rts_av_disable_chn(h265);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);

exit:
	if (h265 >= 0) {
		rts_av_destroy_chn(h265);
		h265 = -1;
	}

	if (input_buffer) {
		if (input_buffer->vm_addr)
			rts_av_sys_vrm_free(input_buffer);
		rts_av_delete_buffer(input_buffer);
	}

	RTS_SAFE_RELEASE(input_pfile, fclose);
	RTS_SAFE_CLOSE(outfd, close);
	RTS_SAFE_RELEASE(pctrl, rts_av_release_h265_ctrl);

	return ret;
}

int main(int argc, char *argv[])
{
	struct __dbg_option option;
	int c;
	int ret;

	if (argc < 2) {
		printf("too few parameter\n");
		printf("use -h to get help info\n");
		return -1;
	}

	option.width = 1280;
	option.height = 720;
	option.fps = 15;
	option.filename = NULL;
	option.number = 0;
	option.intraPeriod = 30;
	option.bitrate_mode = DBG_BITRATE_MODE_CBR;
	option.EncBitrate = 1024 * 1024;
	option.min_bitrate = 512 * 1024;
	option.max_bitrate = 2 * 1024 * 1024;

	while ((c = getopt_long(argc, argv,
				":s:hn:i:V:U:0:d:", longopts, NULL)) != -1) {
		switch (c) {
		case 's':
			option.filename = optarg;
			break;
		case 'n':
			option.number = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'h':
			print_help_info();
			return 0;
		case 'i':
			option.input_file = optarg;
			break;
		case 'V':
			sscanf(optarg, "%dx%d", &option.width,
					&option.height);
			break;
		case 'U':
			option.fps = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case '0':
			option.intraPeriod = (int)strtol(optarg, NULL, 0);
			break;
		case RTS_DBG_RC_MODE:
			option.bitrate_mode = (int)strtol(optarg, NULL, 0);
			if (option.bitrate_mode < 0 ||
					option.bitrate_mode > 2) {
				printf("rc mode (%d) is invalid\n",
						option.bitrate_mode);
				return -1;
			}
			break;
		case 'd':
			option.EncBitrate = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case RTS_DBG_MIN_BITRATE:
			option.min_bitrate = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case RTS_DBG_MAX_BITRATE:
			option.max_bitrate = (uint32_t)strtol(optarg, NULL, 0);
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

	ret = test_stream(&option);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return RTS_OK;
}
