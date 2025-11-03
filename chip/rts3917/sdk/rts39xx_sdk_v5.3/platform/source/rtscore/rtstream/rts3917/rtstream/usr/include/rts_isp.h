/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

#ifndef _RTS_ISP_H_INC_
#define _RTS_ISP_H_INC_

#include <stdint.h>
#include <linux/videodev2.h>
#include <rtsavisp.h>
#include <rts_isp_sensor.h>
#include <rts_isp_ae.h>
#include <rts_isp_awb.h>
#include <rts_isp_af.h>

#ifdef __cplusplus
extern "C"
{
#endif

int rts_isp_v4l2_query_ctrl(uint32_t isp_id, struct v4l2_queryctrl *qc);
int rts_isp_v4l2_query_menu(uint32_t isp_id, struct v4l2_querymenu *qm);
int rts_isp_v4l2_g_ctrl(uint32_t isp_id, struct v4l2_control *control);
int rts_isp_v4l2_s_ctrl(uint32_t isp_id, struct v4l2_control *control);
int rts_isp_v4l2_query_ext_ctrl(uint32_t isp_id,
				struct v4l2_query_ext_ctrl *qec);
int rts_isp_v4l2_g_ext_ctrls(uint32_t isp_id, struct v4l2_ext_controls *ec);
int rts_isp_v4l2_s_ext_ctrls(uint32_t isp_id, struct v4l2_ext_controls *ec);
int rts_isp_v4l2_try_ext_ctrls(uint32_t isp_id, struct v4l2_ext_controls *ec);

#define RTS_V4L2_CID_BASE (V4L2_CID_USER_BASE | 0xf000)
#define RTS_V4L2_CID_GREEN_BALANCE (RTS_V4L2_CID_BASE + 0)
#define RTS_V4L2_CID_IQ_TABLE (RTS_V4L2_CID_BASE + 1)
#define RTS_V4L2_CID_DAY_NIGHT (RTS_V4L2_CID_BASE + 2)
enum rts_isp_day_night {
	RTS_ISP_DAY,
	RTS_ISP_NIGHT,
};
#define RTS_V4L2_CID_DYNAMIC_FPS (RTS_V4L2_CID_BASE + 3)
#define RTS_V4L2_CID_DYNAMIC_IQ (RTS_V4L2_CID_BASE + 4)
#define RTS_V4L2_CID_TEMPORAL_DENOISE (RTS_V4L2_CID_BASE + 5)
#define RTS_V4L2_CID_DEHAZE (RTS_V4L2_CID_BASE + 6)
#define RTS_V4L2_CID_DEHAZE_LEVEL (RTS_V4L2_CID_BASE + 7)
#define RTS_V4L2_CID_LDC (RTS_V4L2_CID_BASE + 8)
#define RTS_V4L2_CID_GRAY (RTS_V4L2_CID_BASE + 9)
#define RTS_V4L2_CID_AUTO_CCM (RTS_V4L2_CID_BASE + 10)
#define RTS_V4L2_CID_CCM_MATRIX (RTS_V4L2_CID_BASE + 11)
#define RTS_V4L2_CID_WDR_MODE (RTS_V4L2_CID_BASE + 12)
enum rts_isp_wdr_mode {
	RTS_ISP_WDR_INNER_DISABLE,
	RTS_ISP_WDR_INNER_MANUAL,
	RTS_ISP_WDR_INNER_AUTO,
};
#define RTS_V4L2_CID_WDR_LEVEL (RTS_V4L2_CID_BASE + 13)
#define RTS_V4L2_CID_AUTO_GAMMA (RTS_V4L2_CID_BASE + 14)
#define RTS_V4L2_CID_GAMMA_CURVE (RTS_V4L2_CID_BASE + 15)
#define RTS_V4L2_CID_AUTO_YGAMMA (RTS_V4L2_CID_BASE + 16)
#define RTS_V4L2_CID_YGAMMA_CURVE (RTS_V4L2_CID_BASE + 17)
#define RTS_V4L2_CID_OUTPUT_POSITION (RTS_V4L2_CID_BASE + 18)
#define RTS_V4L2_CID_COLOR_RANGE (RTS_V4L2_CID_BASE + 19)
enum rts_isp_color_range {
	RTS_ISP_COLOR_RANGE_BT601_STUDIO_SWING,
	RTS_ISP_COLOR_RANGE_BT601_FULL_SWING,
	RTS_ISP_COLOR_RANGE_BT709_STUDIO_SWING,
	RTS_ISP_COLOR_RANGE_BT709_FULL_SWING,
};
#define RTS_V4L2_CID_NR_LEVEL (RTS_V4L2_CID_BASE + 20)
#define RTS_V4L2_CID_DE_LEVEL (RTS_V4L2_CID_BASE + 21)
#define RTS_V4L2_CID_SNR_CROP_X (RTS_V4L2_CID_BASE + 22)
#define RTS_V4L2_CID_SNR_CROP_Y (RTS_V4L2_CID_BASE + 23)
#define RTS_V4L2_CID_Y_GAIN (RTS_V4L2_CID_BASE + 24)
#define RTS_V4L2_CID_SMART_IR_MODE (RTS_V4L2_CID_BASE + 25)
#define RTS_V4L2_CID_SMART_IR_MANUAL_LEVEL (RTS_V4L2_CID_BASE + 26)
#define RTS_V4L2_CID_DAYNIGHT_DETECTION (RTS_V4L2_CID_BASE + 27)
#define RTS_V4L2_CID_AUTO_HDR_RATIO (RTS_V4L2_CID_BASE + 30)
#define RTS_V4L2_CID_HDR_RATIO (RTS_V4L2_CID_BASE + 31)
#define RTS_V4L2_CID_MIRROR_FLIP (RTS_V4L2_CID_BASE + 32)
#define RTS_V4L2_CID_BV (RTS_V4L2_CID_BASE + 33)
#define RTS_V4L2_CID_AUTO_EXPOSURE_BIAS (RTS_V4L2_CID_BASE + 34)
#define RTS_V4L2_CID_HIGHLIGHT_ANTI_BANDING_LEVEL (RTS_V4L2_CID_BASE + 35)
#define RTS_V4L2_CID_SNR_LEVEL (RTS_V4L2_CID_BASE + 36)
#define RTS_V4L2_CID_TNR_LEVEL (RTS_V4L2_CID_BASE + 37)
#define RTS_V4L2_CID_DAY_NIGHT_DETECTION_SENSITIVITY (RTS_V4L2_CID_BASE + 38)

struct rts_isp_mask_entry {
	const enum rts_isp_geom_type type;

	int enable;
	union {
		struct {
			const uint32_t max_cols;
			const uint32_t max_rows;
			rts_isp_grid_t grid;
			void *bitmap;
		};
		rts_isp_prect_t rect;
	};
};

struct rts_isp_mask {
	uint32_t color; /* rgb888 */
	const int number;
	struct rts_isp_mask_entry *entries;
};

int rts_isp_query_mask(uint32_t isp_id, struct rts_isp_mask **mask);
int rts_isp_get_mask(uint32_t isp_id, struct rts_isp_mask *mask);
int rts_isp_set_mask(uint32_t isp_id, struct rts_isp_mask *mask);
void rts_isp_release_mask(uint32_t isp_id, struct rts_isp_mask *mask);

enum rts_isp_ae_mode {
	RTS_ISP_AE_MANUAL_MODE,
	RTS_ISP_AE_AUTO_MODE,
};

struct rts_isp_ae_auto_setting {
	const uint32_t window_num;
	uint8_t * const win_weights;
	int target_delta;
	float exposure_min;
	float exposure_max;
	uint16_t gain_min;
	uint16_t gain_max;
	float min_fps;
};

struct rts_isp_ae_manual_setting {
	uint32_t exposure_time;
	uint16_t total_gain;
	struct {
		uint16_t analog_gain;
		uint16_t digital_gain;
		uint16_t isp_gain;
	} gain;
};

struct rts_isp_ae_setting {
	enum rts_isp_ae_mode mode;
	struct rts_isp_ae_auto_setting _auto;
	struct rts_isp_ae_manual_setting _manual;
};

int rts_isp_query_ae_setting(uint32_t isp_id,
			     struct rts_isp_ae_setting **ae_setting);
int rts_isp_get_ae_setting(uint32_t isp_id,
			   struct rts_isp_ae_setting *ae_setting);
int rts_isp_set_ae_setting(uint32_t isp_id,
			   struct rts_isp_ae_setting *ae_setting);
void rts_isp_release_ae_setting(uint32_t isp_id,
				struct rts_isp_ae_setting *ae_setting);

int rts_isp_get_ae_statis_info(uint32_t isp_id,
			       struct rts_isp_ae_stat_info *info);
int rts_isp_query_ae_statis(uint32_t isp_id,
			    const struct rts_isp_ae_statis **ae_statis);
int rts_isp_get_ae_statis(uint32_t isp_id,
			  const struct rts_isp_ae_statis *ae_statis);
int rts_isp_put_ae_statis(uint32_t isp_id,
			  const struct rts_isp_ae_statis *ae_statis);
void rts_isp_release_ae_statis(uint32_t isp_id,
			       const struct rts_isp_ae_statis *ae_statis);

enum rts_isp_awb_mode {
	RTS_ISP_AWB_TEMPERATURE_MODE,
	RTS_ISP_AWB_AUTO_MODE,
	RTS_ISP_AWB_COMPONENT_MODE,
};

struct rts_isp_awb_setting {
	enum rts_isp_awb_mode mode;

	struct {
		uint32_t temperature;
	} _temperature;

	struct {
		uint16_t r_gain_adj;
		uint16_t b_gain_adj;
	} _auto;

	struct {
		uint16_t r_gain;
		uint16_t g_gain;
		uint16_t b_gain;
	} _component;
};

int rts_isp_query_awb_setting(uint32_t isp_id,
			      struct rts_isp_awb_setting **awb_setting);
int rts_isp_get_awb_setting(uint32_t isp_id,
			    struct rts_isp_awb_setting *awb_setting);
int rts_isp_set_awb_setting(uint32_t isp_id,
			    struct rts_isp_awb_setting *awb_setting);
void rts_isp_release_awb_setting(uint32_t isp_id,
				 struct rts_isp_awb_setting *awb_setting);

int rts_isp_get_awb_statis_info(uint32_t isp_id,
				struct rts_isp_awb_stat_info *info);
int rts_isp_query_awb_statis(uint32_t isp_id,
			     const struct rts_isp_awb_statis **statis);
int rts_isp_get_awb_statis(uint32_t isp_id,
			   const struct rts_isp_awb_statis *statis);
int rts_isp_put_awb_statis(uint32_t isp_id,
			   const struct rts_isp_awb_statis *statis);
void rts_isp_release_awb_statis(uint32_t isp_id,
				const struct rts_isp_awb_statis *statis);

int rts_isp_get_af_statis_info(uint32_t isp_id,
			       struct rts_isp_af_stat_info *info);
int rts_isp_query_af_statis(uint32_t isp_id,
			    const struct rts_isp_af_statis **af_statis);
int rts_isp_get_af_statis(uint32_t isp_id,
			  const struct rts_isp_af_statis *af_statis);
int rts_isp_put_af_statis(uint32_t isp_id,
			  const struct rts_isp_af_statis *af_statis);
void rts_isp_release_af_statis(uint32_t isp_id,
			       const struct rts_isp_af_statis *af_statis);

struct rts_isp_image_size {
	enum rts_isp_sensor_bit_depth sensor_bit_depth;
	rts_isp_size_t sensor_size;
	rts_isp_size_t isp_middle_size;
	rts_isp_size_t isp_final_size;
};

int rts_isp_get_image_size(uint32_t isp_id, struct rts_isp_image_size *size);

struct rts_isp_bypass {
	union {
		uint32_t all;
		struct {
			uint32_t bypass_blc_0:1;
			uint32_t bypass_blc_1:1;
			uint32_t bypass_dpc_0:1;
			uint32_t bypass_dpc_1:1;
			uint32_t bypass_rnr_0:1;
			uint32_t bypass_rnr_1:1;
			uint32_t bypass_nlsc:1;
			uint32_t bypass_mlsc:1;
			uint32_t bypass_awb:1;
			uint32_t bypass_ae_0:1;
			uint32_t bypass_ae_1:1;
			uint32_t bypass_ae:1;
			uint32_t bypass_ccm:1;
			uint32_t bypass_gamma:1;
			uint32_t bypass_wdr:1;
			uint32_t bypass_ygc:1;
			uint32_t bypass_uvtune:1;
			uint32_t bypass_ldc:1;
			uint32_t bypass_eeh:1;
			uint32_t bypass_spe:1;
			uint32_t bypass_ygamma:1;
			uint32_t bypass_hue:1;
		};
	};
};

int rts_isp_get_bypass(uint32_t isp_id, struct rts_isp_bypass *bypass);
int rts_isp_set_bypass(uint32_t isp_id, struct rts_isp_bypass *bypass);

enum rts_isp_blc_position {
	BLC_AFTER_RNR = 0,
	BLC_BEFORE_RNR = 1,
};

int rts_isp_get_blc_position(uint32_t isp_id);

const char *rts_isp_get_bound_sensor_name(uint32_t isp_id);

int rts_isp_set_time_tracker(uint32_t isp_id, int on);


#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_H_INC_ */
