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

#ifndef _RTS_ISP_AWB_H_INC_
#define _RTS_ISP_AWB_H_INC_

#include <stdint.h>
#include <rts_isp_errno.h>
#include <rts_isp_geom.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define AWB_API_VERSION_MAGIC ((uint32_t)'w' << 8 | (uint32_t)'b')
#define AWB_API_MAJOR_VERSION 1
#define AWB_API_MINOR_VERSION 1
#define AWB_API_VERSION (AWB_API_VERSION_MAGIC << 16 | \
			 AWB_API_MAJOR_VERSION << 8 | AWB_API_MINOR_VERSION)
#define AWB_VERSION_MASK ((1 << 16) - 1)
#define AWB_WEIGHT_TABLE_COUNT_MAX 16

struct rts_isp_awb_stat_info {
	rts_isp_grid_size_t win;
	uint16_t pixels_per_cell;
	uint32_t fine_pixels;
};

struct rts_isp_awb_iq_info {
	uint32_t magic;
	uint32_t version;
	const void *iq;
	uint8_t resv[8];
};

struct rts_isp_awb_param_info {
	uint32_t magic;
	uint32_t version;
	void *algo_param;
	uint8_t resv[8];
};

struct rts_isp_awb_info {
	struct rts_isp_awb_stat_info stat_info;
	const void *iq; /* only used for internal*/
	void *algo_param; /* only used for internal*/
	uint8_t resv[8];
};

struct rts_isp_awb_statis {
	uint32_t frame_count;
	const uint16_t *r_mean; /* 16.0 bit precision */
	const uint16_t *g_mean; /* 16.0 bit precision */
	const uint16_t *b_mean; /* 16.0 bit precision */
	const uint16_t *y_mean; /* 16.0 bit precision */
	const uint16_t *rg; /* 3.8 bit precision */
	const uint16_t *bg; /* 3.8 bit precision */
	const uint16_t *pixels;
	uint32_t illum_white_pixels[6];
	uint32_t fine_r_sum;
	uint32_t fine_g_sum;
	uint32_t fine_b_sum;
	uint32_t fine_white_pixels;
	uint8_t resv[8];
};

struct rts_isp_awb_gain {
	uint16_t r_gain; /* 3.8 bit precision */
	uint16_t gr_gain; /* 3.8 bit precision */
	uint16_t gb_gain; /* 3.8 bit precision */
	uint16_t b_gain; /* 3.8 bit precision */
};

struct rts_isp_awb_gain_result {
	uint32_t need_update;
	uint32_t color_temperature;
	struct rts_isp_awb_gain gain;
};

struct rts_isp_awb_rough_gain {
	uint32_t need_update;
	uint16_t r_gain; /* 3.8 bit precision */
	uint16_t b_gain; /* 3.8 bit precision */
};

struct rts_isp_awb_rough_limit {
	uint32_t need_update;
	uint32_t y_min; /* 16.3 bit precision */
	uint32_t y_max; /* 16.3 bit precision */
	uint32_t r_min; /* 16.3 bit precision */
	uint32_t r_max; /* 16.3 bit precision */
	uint32_t g_min; /* 16.3 bit precision */
	uint32_t g_max; /* 16.3 bit precision */
	uint32_t b_min; /* 16.3 bit precision */
	uint32_t b_max; /* 16.3 bit precision */
	uint16_t rg_min;  /* 3.8 bit precision */
	uint16_t rg_max;  /* 3.8 bit precision */
	uint16_t bg_min;  /* 3.8 bit precision */
	uint16_t bg_max;  /* 3.8 bit precision */
	uint16_t bgrg_k1; /* 3.8 bit precision */
	uint16_t bgrg_b1; /* 8.8 bit precision */
	uint16_t bgrg_k2; /* 3.8 bit precision */
	uint16_t bgrg_b2; /* 8.8 bit precision */
};

struct rts_isp_awb_fine_limit {
	uint32_t need_update;
	uint32_t y_min; /* 16.3 bit precision */
	uint32_t y_max; /* 16.3 bit precision */
	uint16_t rg_min;  /* 3.8 bit precision */
	uint16_t rg_max;  /* 3.8 bit precision */
	uint16_t bg_min;  /* 3.8 bit precision */
	uint16_t bg_max;  /* 3.8 bit precision */
};

struct rts_isp_awb_illums {
	uint32_t need_update;
	uint16_t r_gain[6]; /* 3.8 bit precision */
	uint16_t b_gain[6]; /* 3.8 bit precision */
	uint16_t rg_min; /* 3.8 bit precision */
	uint16_t rg_max; /* 3.8 bit precision */
	uint16_t bg_min; /* 3.8 bit precision */
	uint16_t bg_max; /* 3.8 bit precision */
	uint32_t y_min; /* 16.3 bit precision */
	uint32_t y_max; /* 16.3 bit precision */
};

struct rts_isp_awb_stat_config {
	struct rts_isp_awb_rough_gain rough_gain;
	struct rts_isp_awb_rough_limit rough_limit;
	struct rts_isp_awb_fine_limit fine_limit;
	struct rts_isp_awb_illums illums;
};

struct rts_isp_awb_result {
	struct rts_isp_awb_gain_result result;
	struct rts_isp_awb_stat_config config;
	uint8_t resv[8];
};

struct rts_isp_awb_rb_gain_adj {
	uint16_t r_gain_adj;
	uint16_t b_gain_adj;
};

struct rts_isp_awb_weight_table {
	uint32_t w_table_count;
	uint8_t w_table[AWB_WEIGHT_TABLE_COUNT_MAX];
	uint32_t g_table_count;
	uint8_t g_table[AWB_WEIGHT_TABLE_COUNT_MAX];
};

/* pointers for ctrl are only valid in ctrl callback */
enum rts_isp_awb_ctrls {
	COLOR_TEMP_TO_GAIN, /* struct rts_isp_awb_gain_result (pointer) */
	GAIN_TO_COLOR_TEMP, /* struct rts_isp_awb_gain_result (pointer) */
	SET_GAIN_ADJ, /* struct rts_isp_awb_rb_gain_adj */
	SET_AE_RATIO, /* uint32_t */
	SET_WEIGHT_TABLE, /* const struct rts_isp_awb_weight_table (pointer) */
};

struct rts_isp_awb_algo {
	uint32_t api_version;
	int (*init)(uint32_t isp_id, const struct rts_isp_awb_info *info);
	int (*run)(uint32_t isp_id, const struct rts_isp_awb_statis *statis,
		   struct rts_isp_awb_result *result);
	int (*ctrl)(uint32_t isp_id, enum rts_isp_awb_ctrls id, ...);
	int (*cleanup)(uint32_t isp_id);
};

#define RTS_ISP_DEFINE_AWB_ALGO_PLUGIN(awb_algo_ops)                           \
	const struct rts_isp_awb_algo *rts_isp_get_awb_algo(void)              \
	{                                                                      \
		return &awb_algo_ops;                                          \
	}

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_AWB_H_INC_ */
