/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RB_AV_FMT {
	RB_AV_FMT_UNDEFINED = 0x0,
	RB_V_FMT_YUYV,
	RB_V_FMT_YUV420PLANAR,
	RB_V_FMT_YUV420SEMIPLANAR,	//V4L2_PIX_FMT_NV12
	RB_V_FMT_YUV422SEMIPLANAR,
	RB_V_FMT_MJPEG,
	RB_V_FMT_H264,
	RB_V_FMT_H265,

	RB_A_FMT_AUDIO = 0x100,
	RB_A_FMT_MP3,
	RB_A_FMT_ULAW,
	RB_A_FMT_ALAW,
	RB_A_FMT_G726,
	RB_A_FMT_AMRNB,
	RB_A_FMT_AAC,
	RB_A_FMT_SBC,
	RB_A_FMT_OPUS,
};

typedef struct {
	enum RB_AV_FMT fmt;

	union {
		struct {
			uint32_t width;
			uint32_t height;
			uint32_t numerator;
			uint32_t denominator;

			uint32_t timebase_numerator;
			uint32_t timebase_denominator;

			int32_t qp;
			uint32_t gop;
			uint32_t bitrate;
		} video;

		struct {
			uint32_t samplerate;
			uint32_t bitfmt;
			uint32_t channels;
		} audio;
	};
} stream_format;

typedef struct {
	void *vm_addr;
	uint32_t length;
	uint64_t timestamp;
	uint32_t flags;
	uint32_t type;
	uint32_t index;
} packet_t;


/*
 * size = 0: open a existing ringbuffer
 * size > 0: create a ringbuffer
 */
void *rts_ringbuffer_init(char *filename, uint32_t size);
void rts_ringbuffer_release(void *handle);
/*
 * return 0: success
 * return < 0: fail
 */
int rts_ringbuffer_write_packet(void *handle, packet_t *b);

/*
 * return 0: success
 * return < 0: fail
 * return -EAGIN: try again
 * return 1: write process has already done
 */
int rts_ringbuffer_read_packet(void *handle, packet_t **b);
/*
 * free every packet you read
 */
void rts_ringbuffer_free_packet(packet_t *b);
int rts_ringbuffer_set_stream_format(void *handle, stream_format *fmt);
int rts_ringbuffer_get_stream_format(void *handle, stream_format *fmt);
/*
 * seek a timestamp early than now
 */
int rts_ringbuffer_seek_timestamp(void *handle, uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif
