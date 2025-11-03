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
#include <rtsaudio.h>
#include <getopt.h>

/* code flow

        +------------+    +------------+
        |            |    |            |
mic ===>| capture_ch |===>| encode_ch  |===>g711_raw_file (g711 no header)
        |            |    |            |
        +------------+    +------------+

*/

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"save", required_argument, NULL, 's'},
	{"num", required_argument, NULL, 'n'},
	{0, 0, 0, 0}
};

static uint32_t rts_num;
static char *save_dir;

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tcapture audio and encode to ulaw\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_audio_capture_encode [option]...\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "-h, --help\thelp\n");
	fprintf(stdout, "-s, --save\tsave frame at <dir>\n");
	fprintf(stdout, "-n, --num\tframe number\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "\texample_audio_capture_encode -s /mnt -n 10\n");
}

int test_stream(void)
{
	struct rts_audio_attr attr;
	int capture = -1;
	int encode = -1;
	uint32_t number = 0;
	int ret;
	FILE *pfile = NULL;
	char outfile[100];

	memset(&attr, 0, sizeof(attr));
	snprintf(attr.dev_node, sizeof(attr.dev_node), "hw:0,1");
	attr.rate = 8000;
	attr.format = 16;
	attr.channels = 1;

	capture = rts_av_create_audio_capture_chn(&attr);
	if (capture < 0) {
		RTS_ERR("fail to create capture chn, ret = %d\n", capture);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("capture chn : %d\n", capture);

	encode = rts_av_create_audio_encode_chn(RTS_AUDIO_TYPE_ID_ULAW, 0);
	if (encode < 0) {
		RTS_ERR("fail to create encode chn, ret = %d\n", capture);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	RTS_INFO("encode chn : %d\n", encode);

	ret = rts_av_bind(capture, encode);
	if (ret) {
		RTS_ERR("fail to bind capture and encode, ret = %d\n", ret);
		goto exit;
	}

	if (save_dir) {
		snprintf(outfile, sizeof(outfile), "%s/audio_out.ulaw",
					save_dir);
		RTS_INFO("save to %s\n", outfile);
		pfile = fopen(outfile, "wb");
		if (!pfile) {
			RTS_ERR("open audio file audio_out.ulaw fail\n");
			ret = RTS_RETURN(RTS_E_OPEN_FAIL);
			goto exit;
		}
	}

	rts_av_enable_chn(capture);
	rts_av_enable_chn(encode);
	rts_av_start_recv(encode);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (rts_av_recv_block(encode, &buffer, 100))
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

	rts_av_stop_recv(encode);
	rts_av_disable_chn(encode);
	rts_av_disable_chn(capture);
	rts_av_unbind(encode, capture);

	RTS_INFO("\n");
	RTS_INFO("get %d frames\n", number);
exit:
	if (encode >= 0) {
		rts_av_destroy_chn(encode);
		encode = -1;
	}
	if (capture >= 0) {
		rts_av_destroy_chn(capture);
		capture = -1;
	}
	RTS_SAFE_RELEASE(pfile, fclose);

	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	int c;

	while ((c = getopt_long(argc, argv,
				":hn:s:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'n':
			rts_num = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 's':
			save_dir = optarg;
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
