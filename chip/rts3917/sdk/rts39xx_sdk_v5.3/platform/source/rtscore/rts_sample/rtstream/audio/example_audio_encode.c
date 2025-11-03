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
#include <rts_queue.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <getopt.h>
#include "header.h"

/* code flow
                             +------------+
                             |            |
souce.wav (PCM header)  ===> | encode_ch  |===>target.ulaw (g711 no_header)
                             |            |
                             +------------+
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
	fprintf(stdout, "\tencode wav file to ulaw file\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_audio_encode <in-file> <out-file>\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_audio_decode source.wav target.ulaw\n");
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
	int encode = -1;
	int resample = -1;
	struct rts_av_profile profile;
	uint32_t chunk_bytes;
	FILE *fin = NULL;
	FILE *fout = NULL;
	struct rts_av_buffer *inbuf = NULL;
	int buf_state = 0;
	struct rts_audio_attr in_attr = {0};
	struct subchunk_fmt fmt = {0};
	int data_size = 0;
	int head_length = 0;
	int len;

	fin = fopen(argv[1], "rb");
	if (!fin) {
		RTS_ERR("fail to open %s: %s\n",
			argv[1], strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	fout = fopen(argv[2], "wb");
	if (!fout) {
		RTS_ERR("fail to open %s: %s\n",
			argv[2], strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	ret = analyze_audio_header(fin, &fmt, &head_length, &data_size);
	if (ret) {
		RTS_ERR("analyze audio header fail, ret = %d\n", ret);
		goto exit;
	}
	in_attr.rate = fmt.sample_rate;
	in_attr.format = fmt.num_bits;
	in_attr.channels = fmt.num_channels;
	RTS_INFO("rate = %d, format bit = %d, channels = %d\n",
		in_attr.rate, in_attr.format, in_attr.channels);

	chunk_bytes = in_attr.rate / 32 * in_attr.channels *
			in_attr.format / 8;
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

	encode = rts_av_create_audio_encode_chn(RTS_AUDIO_TYPE_ID_ULAW, 0);
	if (encode < 0) {
		RTS_ERR("fail to create encode chn, ret = %d\n", encode);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	resample = rts_av_create_audio_resample_chn(8000, 16, 1);
	if (resample < 0) {
		RTS_ERR("fail to create resample chn, ret = %d\n", resample);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	profile.fmt = RTS_A_FMT_AUDIO;
	profile.audio.samplerate = in_attr.rate;
	profile.audio.bitfmt = in_attr.format;
	profile.audio.channels = in_attr.channels;
	ret = rts_av_set_profile(resample, &profile);
	if (ret) {
		RTS_ERR("set profile fail, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(resample, encode);
	if (ret) {
		RTS_ERR("fail to bind resample and encode, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(encode);
	rts_av_enable_chn(resample);
	rts_av_start_send(resample);
	rts_av_start_recv(encode);

	while (!g_exit) {
		struct rts_av_buffer *buffer = NULL;

		if (!buf_state && !feof(fin)) {
			len = fread(inbuf->vm_addr, 1,
					chunk_bytes, fin);
			if (len > 0) {
				inbuf->bytesused = len;
				rts_av_set_buffer_profile(inbuf, &profile);
				buf_state = 1;
				rts_av_send(resample, inbuf);
			}
		}

		if (feof(fin) && rts_av_is_idle(encode))
			finish++;
		else
			finish = 0;

		if (finish >= 30) {
			usleep(1000 * 1000 * 1);
			RTS_INFO("finish\n");
			break;
		}

		if (rts_av_recv_block(encode, &buffer, 100))
			continue;

		if (buffer) {
			if (fout)
				fwrite(buffer->vm_addr, 1,
					buffer->bytesused, fout);
			rts_av_put_buffer(buffer);
		}
	}

	rts_av_stop_send(resample);
	rts_av_stop_recv(encode);
	rts_av_disable_chn(encode);
	rts_av_disable_chn(resample);
	rts_av_unbind(resample, encode);
exit:
	if (encode >= 0) {
		rts_av_destroy_chn(encode);
		encode = -1;
	}
	if (resample >= 0) {
		rts_av_destroy_chn(resample);
		resample = -1;
	}
	if (inbuf) {
		rts_av_set_buffer_callback(inbuf, NULL, NULL);
		rts_av_delete_buffer(inbuf);
	}
	RTS_SAFE_RELEASE(fout, fclose);
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

	if (argc != 3) {
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
