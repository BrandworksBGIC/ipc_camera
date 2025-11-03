/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RF_MSG_H__
#define __RF_MSG_H__
#include <stdint.h>
#include <libubus.h>

#include "rf_error.h"

#define CAPTURE_MSGD "capture_msgd"
#define CAPTURE_MSGD_HANDLE "capture_msgd_handle"
#define CAPTURE_NOTIFY "capture_notify"

enum rf_msg_notify_type {
	RF_MSG_MD   = 0x0001,
};

enum rf_msg_type {
	RF_MSG_DOMAIN_VIDEO   = 0x0001,
	RF_MSG_DOMAIN_AUDIO   = 0x0002,
	RF_MSG_DOMAIN_ISPCTRL = 0x0004,
	RF_MSG_DOMAIN_MD      = 0x0008,
	RF_MSG_DOMAIN_MASK    = 0x0010,
	RF_MSG_DOMAIN_OSD     = 0x0020,
};

#define RF_MSG_BASE(domain) (domain << 16)
#define RF_MSG_DOMAIN(request) (request >> 16)

#define RF_MIN_GOP (1)
#define RF_MAX_GOP_NORMAL (150)
#define RF_MAX_GOP_BSMART (1500)

typedef enum rf_msg_request {
	RF_VIDEO_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_VIDEO) | 0x000001,
	RF_VIDEO_CTRL_PROBE = RF_VIDEO_MSG_BASE,
	RF_VIDEO_CTRL_PAUSE,
	RF_VIDEO_CTRL_RESUME,
	RF_VIDEO_CTRL_KEY_FRAME,
	RF_VIDEO_SNAPSHOT,
	RF_VIDEO_GET_ENCODER_ABILITY,
	RF_VIDEO_GET_ENCODER_ATTR,
	RF_VIDEO_SET_ENCODER_ATTR,
	RF_VIDEO_GET_ISP_ABILITY,
	RF_VIDEO_GET_ISP_ATTR,
	RF_VIDEO_SET_ISP_ATTR,
	RF_VIDEO_MSG_MAX,
	RF_VIDEO_MSG_NUM = RF_VIDEO_MSG_MAX - RF_VIDEO_MSG_BASE,

	RF_ISPCTRL_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_ISPCTRL) | 0x000001,
	RF_ISPCTRL_GET_ALL = RF_ISPCTRL_MSG_BASE,
	RF_ISPCTRL_SET_ALL,
	RF_ISPCTRL_GET_CTRL,
	RF_ISPCTRL_SET_CTRL,
	RF_ISPCTRL_MSG_MAX,
	RF_ISPCTRL_MSG_NUM = RF_ISPCTRL_MSG_MAX - RF_ISPCTRL_MSG_BASE,

	RF_MASK_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_MASK) | 0x000001,
	RF_MASK_GET_ATTR = RF_MASK_MSG_BASE,
	RF_MASK_SET_ATTR,
	RF_MASK_MSG_MAX,
	RF_MASK_MSG_NUM = RF_MASK_MSG_MAX - RF_MASK_MSG_BASE,

	RF_MD_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_MD) | 0x000001,
	RF_MD_GET_ATTR = RF_MD_MSG_BASE,
	RF_MD_SET_ATTR,
	RF_MD_MSG_MAX,
	RF_MD_MSG_NUM = RF_MD_MSG_MAX - RF_MD_MSG_BASE,

	RF_OSD_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_OSD) | 0x000001,
	RF_OSD_GET_ATTR = RF_OSD_MSG_BASE,
	RF_OSD_SET_ATTR,
	RF_OSD_MSG_MAX,
	RF_OSD_MSG_NUM = RF_OSD_MSG_MAX - RF_OSD_MSG_BASE,

	RF_AUDIO_MSG_BASE = RF_MSG_BASE(RF_MSG_DOMAIN_AUDIO) | 0x000001,
	RF_AUDIO_GET_ATTR = RF_AUDIO_MSG_BASE,
	RF_AUDIO_SET_ATTR,
	RF_AUDIO_GET_VOLUME,
	RF_AUDIO_SET_VOLUME,
	RF_AUDIO_GET_AEC,
	RF_AUDIO_SET_AEC,
	RF_AUDIO_MSG_MAX,
	RF_AUDIO_MSG_NUM = RF_AUDIO_MSG_MAX - RF_AUDIO_MSG_BASE,
} RF_MSG_REQUEST;

enum ispctrl_id {
	ISPCTRL_ID_BRIGHTNESS = 1,
	ISPCTRL_ID_CONTRAST,
	ISPCTRL_ID_HUE,
	ISPCTRL_ID_SATURATION,
	ISPCTRL_ID_SHARPNESS,
	ISPCTRL_ID_GAMMA,
	ISPCTRL_ID_AWB_CTRL,
	ISPCTRL_ID_WB_TEMPERATURE,
	ISPCTRL_ID_BLC,
	ISPCTRL_ID_GAIN,
	ISPCTRL_ID_PWR_FREQUENCY,
	ISPCTRL_ID_EXPOSURE_MODE,
	ISPCTRL_ID_EXPOSURE_PRIORITY,
	ISPCTRL_ID_EXPOSURE_TIME,
	ISPCTRL_ID_AF,
	ISPCTRL_ID_FOCUS,
	ISPCTRL_ID_ZOOM,
	ISPCTRL_ID_PAN,
	ISPCTRL_ID_TILT,
	ISPCTRL_ID_ROLL,
	ISPCTRL_ID_FLIP,
	ISPCTRL_ID_MIRROR,
	ISPCTRL_ID_ROTATE,
	ISPCTRL_ID_ISP_SPECIAL_EFFECT,
	ISPCTRL_ID_EV_COMPENSATE,
	ISPCTRL_ID_COLOR_TEMPERATURE_ESTIMATION,
	ISPCTRL_ID_AE_LOCK,
	ISPCTRL_ID_AWB_LOCK,
	ISPCTRL_ID_AF_LOCK,
	ISPCTRL_ID_LED_TOUCH_MODE,
	ISPCTRL_ID_LED_FLASH_MODE,
	ISPCTRL_ID_ISO,
	ISPCTRL_ID_SCENE_MODE,
	ISPCTRL_ID_ROI_MODE,
	ISPCTRL_ID_3A_STATUS,
	ISPCTRL_ID_IDEA_EYE_SENSITIVITY,
	ISPCTRL_ID_IDEA_EYE_STATUS,
	ISPCTRL_ID_IEDA_EYE_MODE,
	ISPCTRL_ID_GRAY_MODE,
	ISPCTRL_ID_WDR_MODE,
	ISPCTRL_ID_WDR_LEVEL,
	ISPCTRL_ID_GREEN_BALANCE,
	ISPCTRL_ID_RED_BALANCE,
	ISPCTRL_ID_BLUE_BALANCE,
	ISPCTRL_ID_AE_GAIN,
	ISPCTRL_ID_3DNR,
	ISPCTRL_ID_DEHAZE,
	ISPCTRL_ID_IR_MODE,
	ISPCTRL_ID_SMART_IR_MODE,
	ISPCTRL_ID_SMART_IR_MANUAL_LEVEL,
	ISPCTRL_ID_IN_OUT_DOOR_MODE,
	ISPCTRL_ID_NOISE_REDUCTION,
	ISPCTRL_ID_DETAIL_ENHANCEMENT,
	ISPCTRL_ID_LDC,
	ISPCTRL_ID_MAX,
};

typedef struct rf_ispctrl_ctrl {
	/*for 3916*/
	uint32_t	chn;

	uint32_t	id;
	uint32_t	type;
	uint32_t	supported;	/* is this ctrl supported */
	char		name[32];	/* Whatever */
	int32_t		minimum;	/* Note signedness */
	int32_t		maximum;
	int32_t		step;
	int32_t		default_val;
	int32_t		current;
	uint32_t	flags;
	uint32_t	reserved[2];
} rf_ispctrl_ctrl;

struct rf_ispctrl_all {
	rf_ispctrl_ctrl brightness;
	rf_ispctrl_ctrl contrast;
	rf_ispctrl_ctrl hue;
	rf_ispctrl_ctrl saturation;
	rf_ispctrl_ctrl sharpness;
	rf_ispctrl_ctrl gamma;
	rf_ispctrl_ctrl white_balance;
	rf_ispctrl_ctrl white_balance_temperature;
	rf_ispctrl_ctrl backlight_compensate;
	rf_ispctrl_ctrl gain;
	rf_ispctrl_ctrl power_line_frequency;
	rf_ispctrl_ctrl exposure_mode;
	rf_ispctrl_ctrl exposure_priority;
	rf_ispctrl_ctrl exposure_time;
	rf_ispctrl_ctrl auto_focus;
	rf_ispctrl_ctrl focus;
	rf_ispctrl_ctrl zoom;
	rf_ispctrl_ctrl pan;
	rf_ispctrl_ctrl tilt;
	rf_ispctrl_ctrl roll;
	rf_ispctrl_ctrl flip;
	rf_ispctrl_ctrl mirror;
	rf_ispctrl_ctrl rotate;
	rf_ispctrl_ctrl isp_special_effect;
	rf_ispctrl_ctrl ev_compensate;
	rf_ispctrl_ctrl color_temperature_estimation;
	rf_ispctrl_ctrl ae_lock;
	rf_ispctrl_ctrl awb_lock;
	rf_ispctrl_ctrl af_lock;
	rf_ispctrl_ctrl led_touch_mode;
	rf_ispctrl_ctrl led_flash_mode;
	rf_ispctrl_ctrl iso;
	rf_ispctrl_ctrl scene_mode;
	rf_ispctrl_ctrl roi_mode;
	rf_ispctrl_ctrl three_a_status;
	rf_ispctrl_ctrl idea_eye_sensitivity;
	rf_ispctrl_ctrl idea_eye_status;
	rf_ispctrl_ctrl idea_eye_mode;
	rf_ispctrl_ctrl gray_mode;
	rf_ispctrl_ctrl wdr_mode;
	rf_ispctrl_ctrl wdr_level;
	rf_ispctrl_ctrl green_mdoe;
	rf_ispctrl_ctrl red_mode;
	rf_ispctrl_ctrl blue_mode;
	rf_ispctrl_ctrl ae_gain;
	rf_ispctrl_ctrl three_dnr;
	rf_ispctrl_ctrl dehaze;
	rf_ispctrl_ctrl ir_mode;
	rf_ispctrl_ctrl smart_ir_mode;
	rf_ispctrl_ctrl smart_ir_level;
	rf_ispctrl_ctrl in_out_door_mode;
	rf_ispctrl_ctrl noise_reduction;
	rf_ispctrl_ctrl detail_enhancement;
	rf_ispctrl_ctrl ldc;
};

struct rf_ispctrl_req {
	union {
		struct rf_ispctrl_all all;
		struct rf_ispctrl_ctrl ctrl;
	};
};

enum {
	MD_MAX_BITMAP_SIZE = 150,
	MD_BLOCK_NUM = 5,
	MASK_GRID_NUM = 1,
	MASK_RECT_NUM = 4,
	OSD_BUFFER_SIZE = 256,
	OSD_BLOCK_NUM = 6,
};

struct rf_rect {
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
};

struct rf_md_block {
	int32_t enable;
	uint8_t sensitivity;
	uint8_t percentage;

	struct rf_rect rect;
	uint32_t res_width;
	uint32_t res_height;
};

struct rf_mask_rect_block {
	int32_t enable;

	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
};

struct rf_grid_bitmap {
	uint8_t *vm_addr;
	uint32_t length;
};

struct rf_mask_grid_block {
	int32_t enable;
	uint32_t supported_grid_num;

	int32_t start_x;
	int32_t start_y;
	uint32_t cell_width;
	uint32_t cell_height;
	uint32_t grid_rows;
	uint32_t grid_columns;

	struct rf_grid_bitmap bitmap;
};

struct rf_mask_attr {
	uint32_t color;
	struct rf_mask_grid_block grids[MASK_GRID_NUM];
	struct rf_mask_rect_block rects[MASK_RECT_NUM];

	uint32_t reserved[4];
};

struct rf_mask_req {
	union {
		struct rf_mask_attr attr;
	};
};

struct rf_md_attr {
	uint32_t attr_id;
	int32_t number;	/* default 5 */
	struct rf_md_block blocks[MD_BLOCK_NUM];

	uint32_t reserved[4];
};

struct rf_md_req {
	union {
		struct rf_md_attr attr;
	};
};

enum {
	OSD_SINGLE_CHAR = 0,
	OSD_DOUBLE_CHAR,
	OSD_BITMAP,
	OSD_OTHERS,
	OSD_ROTATE_0 = 0,
	OSD_ROTATE_90,
	OSD_ROTATE_180,
	OSD_ROTATE_270,
};

enum rf_osdi_blk_fmt {
	RF_OSDI_BLK_FMT_1BPP = 0x100,
	RF_OSDI_BLK_FMT_RGBA1111 = 0x240,
	RF_OSDI_BLK_FMT_RGBA2222,
	RF_OSDI_BLK_FMT_RGBA5551,
	RF_OSDI_BLK_FMT_RGBA4444,
	RF_OSDI_BLK_FMT_RGBA8888,
};

struct rf_picture {
	enum rf_osdi_blk_fmt pixel_fmt;
	void *pdata;
	uint32_t length;
};

struct rf_osd_block {
	uint8_t enable;
	struct rf_picture picture;
	uint32_t left;
	uint32_t top;
	uint32_t right;
	uint32_t bottom;
};

struct rf_osd_attr {
	struct rf_osd_block blocks[OSD_BLOCK_NUM];
};

struct rf_osd_req {
	union {
		struct rf_osd_attr attr;
	};
};

struct rf_audio_node {
	char dev_node[256];
};

struct rf_audio_volume {
	int32_t capture_volume;
	int32_t play_volume;
};

struct rf_audio_attr {
	int32_t playback_sample_rate;
	int32_t playback_channels;
	int32_t playback_format;
	int32_t capture_sample_rate;
	int32_t capture_channels;
	int32_t capture_format;
	int32_t encode_sample_rate;
	int32_t encode_channels;
	int32_t encode_format;
	int32_t encode_type;
	int32_t encode_bitrate;
};

struct rf_audio_aec {
	uint8_t aecns_enable;
	int32_t aec_en;
	int32_t ns_en;
	int32_t aec_thr;
	int32_t aec_scale;
};

struct rf_audio_req {
	union {
		struct rf_audio_node node;
		struct rf_audio_attr attr;
		struct rf_audio_volume volume;
		struct rf_audio_aec aec;
	};
};

struct rf_encode_attr {
	uint32_t codec_type;
	uint32_t bitrate_mode;

	uint32_t bitrate;
	uint32_t max_bitrate;
	uint32_t min_bitrate;

	uint32_t qp;
	uint32_t max_qp;
	uint32_t min_qp;

	uint32_t gop;
	uint32_t gop_mode;
	uint32_t super_p_period;
};

enum {
	H264_BASELINE_PROFILE = 1,
	H264_MAIN_PROFILE,
	H264_HIGH_PROFILE,
};

struct param_range {
	int32_t		minimum;	/* Note signedness */
	int32_t		maximum;
	int32_t		step;
};

struct rf_encode_ability {
	struct param_range profile;
	struct param_range level;
	struct param_range rotate;
	struct param_range gop_mode;
	struct param_range bitrate_mode;
	struct param_range gop[2];
	struct param_range super_p_period[2];
	struct param_range bitrate;

};

struct rf_isp_attr {
	int32_t width;
	int32_t height;
	int32_t framerate;
};

#define MAX_FMT_NUM_STREAM 16
#define MAX_FRMIVAL_NUM_STREAM 32

struct video_resolution_stepwise {
	uint32_t min_width;	/* Minimum frame width [pixel] */
	uint32_t max_width;	/* Maximum frame width [pixel] */
	uint32_t step_width;	/* Frame width step size [pixel] */
	uint32_t min_height;	/* Minimum frame height [pixel] */
	uint32_t max_height;	/* Maximum frame height [pixel] */
	uint32_t step_height;	/* Frame height step size [pixel] */
};

struct fract_number {
	uint32_t numerator;
	uint32_t denominator;
};

struct rf_isp_ability {
	int32_t fmt_number;
	int32_t formats[MAX_FMT_NUM_STREAM];
	struct video_resolution_stepwise resolution;
	int32_t frmival_type;
	int32_t frmival_num;
	struct fract_number frmivals[MAX_FRMIVAL_NUM_STREAM];
};

struct rf_snapshot {
	int32_t number;
	int32_t interval;
};

struct rf_video_req {
	int32_t index;
	union {
		struct rf_encode_ability enc_ability;
		struct rf_encode_attr enc_attr;
		struct rf_isp_attr isp_attr;
		struct rf_isp_ability isp_ability;
		struct rf_snapshot snapshot;
	};
};

struct rf_notify_st {
	unsigned int	id;
	time_t		time;
};

typedef int (*notify_cb_t)(struct rf_notify_st *msg);

int rf_connect_ubus(void);
void rf_disconnect_ubus(void);
int rf_send_msg(
		RF_MSG_REQUEST request,
		void *arg,
		int len);
int rf_send_event(
		enum rf_msg_notify_type event_id);
void *rf_register_event_handler(
		char *event_notify,
		notify_cb_t cb);
void rf_unregister_event_handler(
		void *handler);
#endif
