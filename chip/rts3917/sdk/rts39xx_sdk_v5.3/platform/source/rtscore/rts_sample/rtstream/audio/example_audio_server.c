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
#include <rts_queue.h>
#include <rtscamkit.h>
#include <rtsavapi.h>
#include <rtsaudio.h>
#include <getopt.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define RECORD_TIME    20
#define COMPOSE_ID(a, b, c, d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define WAV_RIFF		COMPOSE_ID('R', 'I', 'F', 'F')
#define WAV_WAVE		COMPOSE_ID('W', 'A', 'V', 'E')
#define WAV_FMT			COMPOSE_ID('f', 'm', 't', ' ')
#define WAV_DATA		COMPOSE_ID('d', 'a', 't', 'a')
#define WAV_FACT		COMPOSE_ID('f', 'a', 'c', 't')

struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

struct xlaw_header {
	unsigned int riff;		/* 'RIFF' */
	unsigned int filelen;		/* filelen */
	unsigned int type;		/* 'WAVE' */

	unsigned int fmt;
	unsigned int fmtlen;
	short int formattag;
	short int channels;
	unsigned int rate;
	unsigned int byte_p_sec;
	short int byte_p_spl;
	short int format;
	short int ext;

	unsigned int fact;		/* 'fact' */
	unsigned int factlen;
	unsigned int factsize;

	unsigned int data;		/* 'data' */
	unsigned int length;
} __packed;

struct __server_info {
	int capture_ch;
	int encode_ch;
	int playback_ch;
	int mixer_ch;
	int decode_ch;
	int dtom_resample_ch;
	int atoe_resample_ch;
	FILE *fp;
	FILE *fout;
	uint32_t chunk_bytes;
	struct rts_audio_attr cfg_p;
	struct rts_audio_attr cfg_c;
	struct rts_av_buffer *buffers[2];
	Queue idles;
	uint32_t encode_type;
	struct xlaw_header xlawheader;
	int finish;
};

/* code flow

                           +---------+    +---------+    +---------+    +---------+
                           |         |    |         |    |         |    |         |
                           |         |    |dtom_resa|    |         |    | playback|
play.ulaw (g711 header)===>|decode_ch|===>| mple_ch |===>|mixer_ch |===>|  _ch    |===>speaker
                           |         |    |(option) |    |         |    |         |
                           +---------+    +---------+    +---------+    +---------+

       +---------+    +---------+    +---------+
       |         |    |atoe_res |    |         |
mic===>|capture  |===>| ample_ch|===>|encode_ch|===>record.ulaw (g711 header)
       |   _ch   |    |(option) |    |         |
       +---------+    +---------+    +---------+
*/


static int g_exit;
static void Termination(int sign)
{
	g_exit = 1;
}

void print_help_info(void)
{
	fprintf(stdout, "DESCRIPTION:\n");
	fprintf(stdout, "\taudio server: aplayback & capture audio\n");
	fprintf(stdout, "\tTo test the effect of the echo cancellation.\n");
	fprintf(stdout, "\tuse ctrt + c to quit capture\n");
	fprintf(stdout, "USAGE:\n");
	fprintf(stdout, "\texample_audio_server [FILE1]...[FILE2]...\n");
	fprintf(stdout, "\t[FILE1] is the input file.\n");
	fprintf(stdout, "\t[FILE2] is the output file.\n");
	fprintf(stdout, "EXAMPLE:\n");
	fprintf(stdout, "\texample_audio_server play.ulaw record.ulaw\n");
}

int set_xlaw_header(struct __server_info *info)
{
	info->xlawheader.riff = WAV_RIFF;
	info->xlawheader.type = WAV_WAVE;
	info->xlawheader.fmt = WAV_FMT;
	switch (info->encode_type) {
	case RTS_AUDIO_TYPE_ID_ULAW:
		info->xlawheader.fmtlen = 0x12;
		info->xlawheader.formattag = 0x07;
		break;
	case RTS_AUDIO_TYPE_ID_ALAW:
		info->xlawheader.fmtlen = 0x12;
		info->xlawheader.formattag = 0x06;
		break;
	default:
		return -1;
	}
	info->xlawheader.channels = 1;
	info->xlawheader.rate = 8000;
	info->xlawheader.format = 8;
	info->xlawheader.ext = 0;
	info->xlawheader.fact = WAV_FACT;
	info->xlawheader.factlen = 4;
	info->xlawheader.factsize = RECORD_TIME * info->xlawheader.rate *
		info->xlawheader.channels * info->xlawheader.format / 8;
	info->xlawheader.filelen = RECORD_TIME * info->xlawheader.rate *
		info->xlawheader.channels * info->xlawheader.format / 8 + 50;
	info->xlawheader.byte_p_sec = info->xlawheader.rate *
		info->xlawheader.channels * info->xlawheader.format / 8;
	info->xlawheader.byte_p_spl = info->xlawheader.channels *
		info->xlawheader.format / 8;
	info->xlawheader.data = WAV_DATA;
	info->xlawheader.length = RECORD_TIME * info->xlawheader.rate *
		info->xlawheader.channels * info->xlawheader.format / 8;

	return RTS_OK;
}

void recycle_buffer(void *master, struct rts_av_buffer *buffer)
{
	struct __server_info *info = master;
	int ret;

	if (g_exit)
		return;

	if (!info || !info->fp)
		return;

	buffer->bytesused = 0;
	ret = fread(buffer->vm_addr, 1, info->chunk_bytes, info->fp);
	if (ret == 0) {
		fseek(info->fp, 0x3a, SEEK_SET);
	} else {
		buffer->bytesused = ret;
		buffer->timestamp = 0;
		rts_queue_push_back(info->idles, rts_av_get_buffer(buffer));
	}
}

int start_server(struct __server_info *info)
{
	struct rts_av_profile profile;
	struct rts_audio_capture_vqe c_vqe;
	int ret;
	int i;
	int codec_samplerate = 8000;
	int codec_format = 16;
	int codec_channels = 1;

	RTS_ASSERT(info);

	info->decode_ch = rts_av_create_audio_decode_chn();
	if (info->decode_ch < 0) {
		ret = info->decode_ch;
		return ret;
	}
	RTS_INFO("audio decode chn : %d\n", info->decode_ch);

	rts_av_get_profile(info->decode_ch, &profile);
	profile.fmt = RTS_A_FMT_ULAW;
	ret = rts_av_set_profile(info->decode_ch, &profile);
	if (ret) {
		RTS_ERR("set decode fail, ret = %d\n", ret);
		goto exit;
	}

	info->dtom_resample_ch = rts_av_create_audio_resample_chn(
				info->cfg_p.rate, info->cfg_p.format,
				info->cfg_p.channels);
	if (info->dtom_resample_ch < 0) {
		ret = info->dtom_resample_ch;
		goto exit;
	}
	RTS_INFO("audio resample chn : %d\n", info->dtom_resample_ch);

	info->mixer_ch = rts_av_create_audio_mixer_chn();
	if (info->mixer_ch < 0) {
		ret = info->mixer_ch;
		goto exit;
	}
	RTS_INFO("audio mixer chn : %d\n", info->mixer_ch);

	info->playback_ch = rts_av_create_audio_playback_chn(&info->cfg_p);
	if (info->playback_ch < 0) {
		ret = info->playback_ch;
		goto exit;
	}
	RTS_INFO("audio playback chn : %d\n", info->playback_ch);

	info->capture_ch = rts_av_create_audio_capture_chn(&info->cfg_c);
	if (info->capture_ch < 0) {
		ret = info->capture_ch;
		goto exit;
	}
	RTS_INFO("audio capture chn : %d\n", info->capture_ch);

	info->atoe_resample_ch = rts_av_create_audio_resample_chn(
			codec_samplerate, codec_format,
			codec_channels);
	if (info->atoe_resample_ch < 0) {
		ret = info->atoe_resample_ch;
		goto exit;
	}
	RTS_INFO("audio resample chn : %d\n", info->atoe_resample_ch);

	info->encode_ch = rts_av_create_audio_encode_chn(info->encode_type, 0);
	if (info->encode_ch < 0) {
		ret = info->encode_ch;
		goto exit;
	}
	RTS_INFO("encode chn : %d\n", info->encode_ch);

	ret = rts_av_bind(info->decode_ch, info->dtom_resample_ch);
	if (ret) {
		RTS_ERR("fail to bind decode and resample, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(info->dtom_resample_ch, info->mixer_ch);
	if (ret) {
		RTS_ERR("fail to bind resample and mixer, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(info->mixer_ch, info->playback_ch);
	if (ret) {
		RTS_ERR("fail to bind mixer and playback, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(info->capture_ch, info->atoe_resample_ch);
	if (ret) {
		RTS_ERR("fail to bind aec and resample, ret = %d\n", ret);
		goto exit;
	}

	ret = rts_av_bind(info->atoe_resample_ch, info->encode_ch);
	if (ret) {
		RTS_ERR("fail to bind resample and encode, ret = %d\n", ret);
		goto exit;
	}

	rts_av_enable_chn(info->decode_ch);
	rts_av_enable_chn(info->dtom_resample_ch);
	rts_av_enable_chn(info->mixer_ch);
	rts_av_enable_chn(info->playback_ch);
	rts_av_enable_chn(info->capture_ch);
	rts_av_enable_chn(info->atoe_resample_ch);
	rts_av_enable_chn(info->encode_ch);

	memset(&c_vqe, 0, sizeof(c_vqe));
	c_vqe.aecns_enable = 1;
	c_vqe.aecns_attr.aec_enable = 1;
	c_vqe.aecns_attr.ns_enable = 1;

	rts_av_set_audio_capture_vqe(info->capture_ch, &c_vqe);

	rts_av_start_send(info->decode_ch);
	rts_av_start_recv(info->encode_ch);

	for (i = 0; i  < RTS_ARRAY_SIZE(info->buffers); i++) {
		struct rts_av_buffer *buffer =
				rts_av_get_buffer(info->buffers[i]);
		rts_av_set_buffer_callback(buffer, info, recycle_buffer);
		RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);
	}

	return RTS_OK;
exit:
	RTS_SAFE_CLOSE(info->decode_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->dtom_resample_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->mixer_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->playback_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->capture_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->atoe_resample_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->encode_ch, rts_av_destroy_chn);
	return ret;
}

void stop_server(struct __server_info *info)
{
	int i;

	rts_av_stop_send(info->decode_ch);
	rts_av_stop_recv(info->encode_ch);

	rts_av_disable_chn(info->decode_ch);
	rts_av_disable_chn(info->dtom_resample_ch);
	rts_av_disable_chn(info->mixer_ch);
	rts_av_disable_chn(info->playback_ch);
	rts_av_disable_chn(info->capture_ch);
	rts_av_disable_chn(info->atoe_resample_ch);
	rts_av_disable_chn(info->encode_ch);

	rts_av_unbind(info->decode_ch, info->dtom_resample_ch);
	rts_av_unbind(info->dtom_resample_ch, info->mixer_ch);
	rts_av_unbind(info->mixer_ch, info->playback_ch);
	rts_av_unbind(info->capture_ch, info->atoe_resample_ch);
	rts_av_unbind(info->atoe_resample_ch, info->encode_ch);

	RTS_SAFE_CLOSE(info->decode_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->dtom_resample_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->mixer_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->playback_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->capture_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->atoe_resample_ch, rts_av_destroy_chn);
	RTS_SAFE_CLOSE(info->encode_ch, rts_av_destroy_chn);

	for (i = 0; i < RTS_ARRAY_SIZE(info->buffers); i++)
		rts_av_set_buffer_callback(info->buffers[i], NULL, NULL);
}

int test_server(struct __server_info *info)
{
	struct rts_av_buffer *buffer = NULL;
	int ret;

	RTS_ASSERT(info);

	if (!rts_queue_empty(info->idles)) {
		buffer = rts_queue_pop(info->idles);
		rts_av_send(info->decode_ch, buffer);
		RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);
	}

	ret = rts_av_recv_block(info->encode_ch, &buffer, 100);
	if (ret)
		return ret;

	if (!buffer)
		return RTS_RETURN(RTS_E_GET_FAIL);

	if (info->fout) {
		ret = fwrite(buffer->vm_addr, 1,
				buffer->bytesused, info->fout);
		info->finish -= ret;
		if (info->finish <= 0) {
			RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);
			g_exit = 1;
		}
	}

	RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

	return RTS_OK;
}

int clear_server_info(struct __server_info *info)
{
	int i;

	RTS_ASSERT(info);

	for (i = 0; i < RTS_ARRAY_SIZE(info->buffers); i++)
		rts_av_set_buffer_callback(info->buffers[i], NULL, NULL);

	if (info->idles) {
		rts_queue_clear(info->idles,
				(cleanup_item_func)rts_av_put_buffer);
		RTS_SAFE_RELEASE(info->idles, rts_queue_destroy);
	}

	for (i = 0; i < RTS_ARRAY_SIZE(info->buffers); i++)
		RTS_SAFE_RELEASE(info->buffers[i], rts_av_delete_buffer);

	return RTS_OK;
}

int init_server_info(struct __server_info *info)
{
	int ret;
	int i;

	RTS_ASSERT(info);

	info->capture_ch = -1;
	info->atoe_resample_ch = -1;
	info->decode_ch = -1;
	info->dtom_resample_ch = -1;
	info->encode_ch = -1;
	info->mixer_ch = -1;
	info->playback_ch = -1;
	snprintf(info->cfg_p.dev_node, sizeof(info->cfg_p.dev_node), "hw:0,1");
	info->cfg_p.rate = 16000;
	info->cfg_p.format = 16;
	info->cfg_p.channels = 1;

	snprintf(info->cfg_c.dev_node, sizeof(info->cfg_c.dev_node), "hw:0,1");
	info->cfg_c.rate = 16000;
	info->cfg_c.format = 16;
	info->cfg_c.channels = 1;

	info->chunk_bytes = 512;

	info->encode_type = RTS_AUDIO_TYPE_ID_ULAW;

	info->finish = 0;

	info->idles = rts_queue_init();
	if (!info->idles)
		return RTS_RETURN(RTS_E_NO_MEMORY);

	for (i = 0; i < RTS_ARRAY_SIZE(info->buffers); i++) {
		info->buffers[i] = rts_av_new_buffer(info->chunk_bytes);
		if (!info->buffers[i]) {
			ret = RTS_RETURN(RTS_E_NO_MEMORY);
			goto exit;
		}
	}
	return RTS_OK;
exit:
	clear_server_info(info);
	return ret;
}

int test_stream(char **argv)
{
	struct __server_info info;
	int ret = 0;

	memset(&info, 0, sizeof(info));
	ret = init_server_info(&info);
	if (ret) {
		RTS_ERR("init info fail, ret = %d\n", ret);
		goto exit;
	}

	info.fp = fopen(argv[1], "rb");
	if (!info.fp) {
		RTS_ERR("fail to open %s: %s\n", argv[1], strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}
	fseek(info.fp, 0x3a, SEEK_SET);

	info.fout = fopen(argv[2], "wb");
	if (!info.fout) {
		RTS_ERR("fail to open %s: %s\n", argv[2], strerror(errno));
		ret = RTS_RETURN(RTS_E_OPEN_FAIL);
		goto exit;
	}

	ret = set_xlaw_header(&info);
	if (ret) {
		RTS_ERR("fail to set audio header\n");
		goto exit;
	}
	fwrite(&info.xlawheader, sizeof(info.xlawheader), 1, info.fout);

	ret = start_server(&info);
	if (ret) {
		RTS_ERR("start server fail, ret = %d\n", ret);
		goto exit;
	}

	info.finish = info.xlawheader.length;
	while (!g_exit) {
		test_server(&info);
	}

	stop_server(&info);
exit:
	RTS_SAFE_RELEASE(info.fout, fclose);
	RTS_SAFE_RELEASE(info.fp, fclose);
	clear_server_info(&info);

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
