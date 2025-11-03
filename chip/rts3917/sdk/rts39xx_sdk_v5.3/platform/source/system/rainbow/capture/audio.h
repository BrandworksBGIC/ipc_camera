/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <rts_queue.h>
#include <rtsavapi.h>

#define BUF_LEN		    (4 * 1024)
#define TWOWAY_BUFF     (4 * 1024)
#define AUDIO_LISTEN_PORT (8000)
#define AUDIO_SHM_BUFF  (256 * 1024)

struct audio_context {
	int decode_ch;
	int d2m_resample_ch;
	int mixer_ch;
	int playback_ch;
	int capture_ch;
	int a2e_resample_ch;
	int encode_ch;
	uint8_t bitfmt;
	uint8_t channels;
	uint16_t samplerate;
	int quit;
	int need_config;
	Queue idles;
	void *ringbuf;
	pthread_t tid;
	pthread_t tw_tid;
	pthread_mutex_t mutex;
	struct rts_audio_attr cfg_p;
	struct rts_audio_attr cfg_c;
	struct rts_av_buffer *buffers[3];
};

struct payload_header {
	uint32_t length;
	uint8_t type;
	uint8_t channels;
	uint8_t bitfmt;
	uint32_t samplerate;
	uint64_t timestamp;
	uint8_t data[0];
} __attribute__ ((__packed__));

enum {
	TWOWAY_AUDIO_PCM = 0,
	TWOWAY_AUDIO_ULAW,
	TWOWAY_AUDIO_ALAW,
	TWOWAY_AUDIO_G726,
};

int rf_control_audio_capture(
	int request, void *arg);
int rf_start_audio_capture(void);
void rf_stop_audio_capture(void);

#endif
