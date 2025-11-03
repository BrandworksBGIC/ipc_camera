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
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <getopt.h>
#include "header.h"


/* code flow
                           +-------------+
                           |             |
souce.wav (PCM header)===> | playback_ch |===>speaker
                           |             |
                           +-------------+
*/

struct option longopts[] = {
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
	fprintf(stdout, "\tplay wav file\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_audio_playback <in-file>\n");
	fprintf(stdout, "\t<in_file> must be an audio file in wav format.\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_audio_playbcak source.wav\n");
}

void recycle_buffer(void *master, struct rts_av_buffer *buffer)
{
	if (g_exit)
		return;

	buffer->bytesused = 0;
	*((int *)master) = 0;
}

int test_stream(char **argv)
{
	int ret;
	int finish = 0;
	int playback = -1;
	struct rts_av_profile profile;
	uint32_t chunk_bytes;
	FILE *fin = NULL;
	struct rts_av_buffer *inbuf = NULL;
	int buf_state = 0;
	struct rts_audio_attr attr = {0};
	struct subchunk_fmt fmt = {0};
	int head_length = 0;
	int data_size = 0;
	int len;

	fin = fopen(argv[1], "rb");
	if (!fin) {
		RTS_ERR("fail to open %s: %s\n",
			argv[1], strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	ret = analyze_audio_header(fin, &fmt, &head_length, &data_size);
	if (ret) {
		RTS_ERR("analyze audio header fail, ret = %d\n", ret);
		goto exit;
	}
	attr.rate = fmt.sample_rate;
	attr.format = fmt.num_bits;
	attr.channels = fmt.num_channels;
	snprintf(attr.dev_node, sizeof(attr.dev_node), "hw:0,1");
	fseek(fin, head_length, SEEK_SET);

	RTS_INFO("rate = %d, format bit = %d, channels = %d\n",
		 attr.rate, attr.format, attr.channels);
	chunk_bytes = attr.rate / 32 * attr.channels * attr.format / 8;

	inbuf = rts_av_new_buffer(chunk_bytes);
	if (!inbuf) {
		RTS_ERR("fail to alloc buffer\n");
		ret = RTS_RETURN(RTS_E_NO_MEMORY);
		goto exit;
	}

	ret = rts_av_set_buffer_callback(inbuf, &buf_state, recycle_buffer);
	if (ret) {
		RTS_ERR("fail to set buffer callback\n");
		goto exit;
	}
	buf_state = 0;

	playback = rts_av_create_audio_playback_chn(&attr);
	if (playback < 0) {
		RTS_ERR("fail to create playback chn, ret = %d\n", playback);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	rts_av_enable_chn(playback);
	rts_av_start_send(playback);

	while (!g_exit) {
		if (!buf_state && !feof(fin)) {
			len = fread(inbuf->vm_addr, 1,
					chunk_bytes, fin);
			if (len > 0) {
				inbuf->bytesused = len;
				buf_state = 1;
				rts_av_send(playback, inbuf);
			}
		}

		if (feof(fin) && rts_av_is_idle(playback))
			finish++;
		else
			finish = 0;

		if (finish >= 30) {
			usleep(1000 * 1000 * 1);
			RTS_INFO("finish\n");
			break;
		}

		usleep(1000);
	}

	rts_av_stop_send(playback);
	rts_av_disable_chn(playback);
exit:
	if (playback >= 0) {
		rts_av_destroy_chn(playback);
		playback = -1;
	}
	if (inbuf) {
		rts_av_set_buffer_callback(inbuf, NULL, NULL);
		rts_av_delete_buffer(inbuf);
	}
	RTS_SAFE_RELEASE(fin, fclose);

	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	int c;

	while ((c = getopt_long(argc, argv,
				":h", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case '?':
			printf("invalid param: -%c\n", optopt);
			return -1;
		default:
			break;
		}
	}

	if (argc != 2) {
		printf("use -h to get help info\n");
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

	ret = test_stream(argv);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
