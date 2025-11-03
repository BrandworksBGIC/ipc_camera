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
#include <sys/time.h>
#include "header.h"


/* code flow
                       +--------------+
                       |              |
in.wav (PCM header)===>| resample_ch  |===>out.wav (PCM no_header)
                       |              |
                       +--------------+
*/

struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"rate", required_argument, NULL, 'r'},
		{"format", required_argument, NULL, 'f'},
		{"channels", required_argument, NULL, 'c'},
		{"input", required_argument, NULL, 'i'},
		{"output", required_argument, NULL, 'o'},
		{0, 0, 0, 0}
};

struct resample_info {
	char *f_in;
	char *f_out;
	struct rts_audio_attr attr_out;
	struct rts_audio_attr attr_in;
};

static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\tresample wav audio file\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_audio_resample");
	fprintf(stdout, " [-i FILE] [-o FILE] [-r rate]");
	fprintf(stdout, " [-c channel] [-f format]\n");
	fprintf(stdout, "\ti--input file name\n");
	fprintf(stdout, "\to--output file name\n");
	fprintf(stdout, "\tr--output rate\n");
	fprintf(stdout, "\tc--output channel\n");
	fprintf(stdout, "\tf--output format\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_audio_resample -i in.wav -o out.wav");
	fprintf(stdout, " -r 44100 -c 2 -f 16\n");
}


void recycle_buffer(void *master, struct rts_av_buffer *buffer)
{
	if (g_exit)
		return;

	buffer->bytesused = 0;
	*((int *)master) = 0;
}

int test_stream(struct resample_info *info)
{
	int ret;
	int finish = 0;
	int resample = -1;
	struct rts_av_profile profile;
	uint32_t chunk_bytes;
	FILE *fin = NULL;
	FILE *fout = NULL;
	struct rts_av_buffer *inbuf = NULL;
	int buf_state = 0;
	struct subchunk_fmt fmt = {0};
	int data_size = 0;
	int head_length = 0;
	int len;

	fin = fopen(info->f_in, "rb");
	if (!fin) {
		RTS_ERR("fail to open %s: %s\n",
			info->f_in, strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	fout = fopen(info->f_out, "wb");
	if (!fout) {
		RTS_ERR("fail to open %s: %s\n",
			info->f_out, strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	ret = analyze_audio_header(fin, &fmt, &head_length, &data_size);
	if (ret) {
		RTS_ERR("analyze audio header fail, ret = %d\n", ret);
		goto exit;
	}
	info->attr_in.rate = fmt.sample_rate;
	info->attr_in.format = fmt.num_bits;
	info->attr_in.channels = fmt.num_channels;
	RTS_INFO("rate = %d, format bit = %d, channels = %d\n",
		info->attr_in.rate, info->attr_in.format,
		info->attr_in.channels);

	chunk_bytes = info->attr_in.rate / 32 * info->attr_in.channels *
			info->attr_in.format / 8;
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

	resample = rts_av_create_audio_resample_chn(info->attr_out.rate,
			info->attr_out.format, info->attr_out.channels);
	if (resample < 0) {
		RTS_ERR("fail to create resample chn, ret = %d\n", resample);
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	profile.fmt = RTS_A_FMT_AUDIO;
	profile.audio.samplerate = info->attr_in.rate;
	profile.audio.bitfmt = info->attr_in.format;
	profile.audio.channels = info->attr_in.channels;
	ret = rts_av_set_profile(resample, &profile);
	if (ret) {
		RTS_ERR("set profile fail, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(resample);
	rts_av_start_send(resample);
	rts_av_start_recv(resample);

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

		if (feof(fin) && rts_av_is_idle(resample))
			finish++;
		else
			finish = 0;

		if (finish >= 30) {
			usleep(1000 * 1000 * 1);
			RTS_INFO("finish\n");
			break;
		}

		if (rts_av_recv_block(resample, &buffer, 100))
			continue;

		if (buffer) {
			if (fout)
				fwrite(buffer->vm_addr, 1,
					buffer->bytesused, fout);
			rts_av_put_buffer(buffer);
		}
	}

	rts_av_stop_send(resample);
	rts_av_stop_recv(resample);
	rts_av_disable_chn(resample);
exit:
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
	struct resample_info info;
	int ret;
	int c;

	if (argc < 2) {
		printf("use -h to get help info\n");
		return -1;
	}

	memset(&info, 0, sizeof(info));
	info.attr_out.rate = 44100;
	info.attr_out.channels = 2;
	info.attr_out.format = 16;

	while ((c = getopt_long(argc, argv,
				"hr:f:c:i:o:", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_help_info();
			return 0;
		case 'r':
			info.attr_out.rate = (int)strtol(optarg, NULL, 0);
			break;
		case 'f':
			info.attr_out.format = (int)strtol(optarg, NULL, 0);
			break;
		case 'c':
			info.attr_out.channels = (int)strtol(optarg, NULL, 0);
			break;
		case 'i':
			info.f_in = optarg;
			break;
		case 'o':
			info.f_out = optarg;
			break;
		}
	}

	if (!info.f_in || !info.f_out) {
		printf("please assign in file and out file\n");
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

	ret = test_stream(&info);

	rts_av_release();

	if (ret)
		printf("Fail\n");
	else
		printf("Success\n");

	return ret;
}
