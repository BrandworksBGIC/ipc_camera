/*
 * Realtek Semiconductor Corp.
 *
 * libs/include/rtsjpeg.h
 *
 * Copyright (C) 2014      Ming Qian<ming_qian@realsil.com.cn>
 */
#ifndef _RTSJPGENC_H
#define _RTSJPGENC_H
#include <stdint.h>
#include "rts_camera_jpgenc.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RTSJPGENC_MAX_OUTNUM	4

typedef const void *RtsJpgEncInst;

enum rtsjpgenc_pic_type {
	RTSJPGENC_YUV420_SEMIPLANAR = 0,
	RTSJPGENC_YUV422_SEMIPLANAR
};

enum rtsjpgenc_enc_mode {
	RTSJPGENC_NORMAL = 0,
	RTSJPGENC_TRIG,
	RTSJPGENC_STREAM,
};

enum rtsjpgenc_rotation_mirror {
	RTSJPGENC_RM_NO = 0,
	RTSJPGENC_RM_HOR = 1,
	RTSJPGENC_RM_VER = 2,
	RTSJPGENC_RM_180 = 3,
	RTSJPGENC_RM_90L_HOR = 4,
	RTSJPGENC_RM_90R = 5,
	RTSJPGENC_RM_90L = 6,
	RTSJPGENC_RM_90L_VER = 7,
};

enum rtsjpgenc_status {
	RTSJPGENC_STATUS_DONE = 0x1,
	RTSJPGENC_STATUS_BUF_NUM_OVERFLOW = (0x1 << 1),
	RTSJPGENC_STATUS_BUF_LEN_OVERFLOW = (0x1 << 2),
	RTSJPGENC_STATUS_LINE_BUF_OVERFLOW = (0x1 << 3),
	RTSJPGENC_STATUS_VIN_ERROR = (0x1 << 4),
	RTSJPGENC_STATUS_NO_DATA = (0x1 << 5),
};

struct rtsjpgenc_buf {
	/* in */
	uint32_t y;
	uint32_t uv;
	/* out */
	uint8_t idx; // input
	uint32_t phy; // input
	uint32_t size; // input
	uint32_t used; // output
	uint32_t status; // output
	uint64_t time_stamp; // output
};

struct rtsjpgenc_config {
	enum rtsjpgenc_pic_type in_type; // 0 yuv420, 1 yuv422
	enum rtsjpgenc_rotation_mirror rotation; // rotation
	enum rtsjpgenc_enc_mode mode; // 0 normal, 1 trig, 2 stream
	uint8_t l2f; // 0~1, limit to full
	uint8_t vin_chn; // 0~1, vin chn
	uint16_t width; // input width
	uint16_t height; // input height
	uint8_t quality; // quality, 1~100
	uint8_t buf_num; // out buffer number
};

int rtsjpgenc_init(RtsJpgEncInst *pinst);
int rtsjpgenc_release(RtsJpgEncInst inst);
int rtsjpgenc_set_config(RtsJpgEncInst inst, struct rtsjpgenc_config *pcfg);
int rtsjpgenc_normal(RtsJpgEncInst inst, struct rtsjpgenc_buf *buf);
int rtsjpgenc_trig(RtsJpgEncInst inst, struct rtsjpgenc_buf *buf);
int rtsjpgenc_stream_start(RtsJpgEncInst inst,
		struct rtsjpgenc_buf *buf, int num);
int rtsjpgenc_stream_stop(RtsJpgEncInst inst);
int rtsjpgenc_poll(RtsJpgEncInst inst, int timeout);
int rtsjpgenc_get_buffer(RtsJpgEncInst inst, struct rtsjpgenc_buf *buf);
int rtsjpgenc_put_buffer(RtsJpgEncInst inst, struct rtsjpgenc_buf *buf);

#ifdef __cplusplus
}
#endif
#endif
