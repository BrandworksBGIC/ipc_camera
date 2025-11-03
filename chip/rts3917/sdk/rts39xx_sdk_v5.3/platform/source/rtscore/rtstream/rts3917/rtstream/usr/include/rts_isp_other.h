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

#ifndef _RTS_ISP_OTHER_H_INC_
#define _RTS_ISP_OTHER_H_INC_

#include <stdint.h>
#include <rts_isp_ae.h>
#include <rts_isp_awb.h>
#include <rts_isp_af.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum rts_isp_ae_algo_status {
	AE_STABLE,
	AE_UNSTABLE,
};

struct rts_isp_flick_iq_info {
	uint32_t magic;
	uint32_t version;
	const void *iq;
	uint8_t resv[8];
};

struct rts_isp_flick_param_info {
	uint32_t magic;
	uint32_t version;
	void *algo_param;
	uint8_t resv[8];
};

struct rts_isp_flick_info_ex {
	struct rts_isp_flick_iq_info iq_info;
	struct rts_isp_flick_param_info param_info;
	uint8_t resv[16];
};

struct rts_isp_flick_info {
	struct rts_isp_ae_stat_info stat_info;
	enum rts_isp_power_line_freq cur_power_line_freq;
	const void *iq; /* only used for internal */
	void *algo_param; /* only used for internal*/
	const void *info_ex;
	uint8_t resv[4];
};

struct rts_isp_flick_statis {
	uint32_t frame_count;
	uint32_t fft_sum_2_9;
	uint32_t fft_sum_2_127;
	uint32_t valid;
	const uint16_t *ae_y_mean;
	uint8_t resv[8];
};

struct rts_isp_flick_ae_config {
	uint32_t need_update;
	uint8_t insert_dummy_line;
};

struct rts_isp_flick_result {
	struct {
		uint32_t need_update;
		enum rts_isp_power_line_freq power_line_freq;
	};
	struct rts_isp_flick_ae_config config;
	uint8_t resv[8];
};

/* pointers for ctrl are only valid in ctrl callback */
enum rts_isp_flick_ctrls {
	SET_AE_EXPOSURE, /* double */
	SET_AE_STATUS, /* enum rts_isp_mod_ae_status */
};

struct rts_isp_flick_algo {
	int (*init)(uint32_t isp_id, const struct rts_isp_flick_info *info);
	int (*run)(uint32_t isp_id, const struct rts_isp_flick_statis *statis,
		   struct rts_isp_flick_result *result);
	int (*ctrl)(uint32_t isp_id, enum rts_isp_flick_ctrls id, ...);
	int (*cleanup)(uint32_t isp_id);
};

/* pointers for ctrl are only valid in ctrl callback */
enum rts_isp_wdr_ctrls {
	SET_DAY_NIGHT, /* enum rts_isp_day_night */
	SET_AE_TARGET, /* uint32_t */
	SET_WDR_LEVEL_MANUAL, /* int */
};

struct rts_isp_wdr_iq_info {
	uint32_t magic;
	uint32_t version;
	const void *iq;
	uint8_t resv[8];
};

struct rts_isp_wdr_param_info {
	uint32_t magic;
	uint32_t version;
	void *algo_param;
	uint8_t resv[8];
};

struct rts_isp_wdr_info_ex {
	struct rts_isp_wdr_iq_info iq_info;
	struct rts_isp_wdr_param_info param_info;
	uint8_t resv[16];
};

struct rts_isp_wdr_info {
	struct rts_isp_ae_stat_info stat_info;
	const void *iq;
	void *algo_param; /* only used for internal*/
	const void *info_ex;
	uint8_t resv[4];
};

struct rts_isp_wdr_statis {
	const struct rts_isp_ae_statis *ae_statis;
	uint8_t resv[8];
};

struct rts_isp_wdr_result {
	uint32_t need_update;
	uint16_t level;
	uint16_t step;
	uint8_t resv[8];
};

struct rts_isp_wdr_algo {
	int (*init)(uint32_t isp_id, const struct rts_isp_wdr_info *info);
	int (*run)(uint32_t isp_id, const struct rts_isp_wdr_statis *statis,
		   struct rts_isp_wdr_result *result);
	int (*ctrl)(uint32_t isp_id, enum rts_isp_wdr_ctrls id, ...);
	int (*cleanup)(uint32_t isp_id);
};

struct rts_isp_raw_stat_info {
	rts_isp_grid_size_t win;
	uint16_t pixels_per_cell;
	uint32_t fine_pixels;
};

struct rts_isp_raw_statis {
	uint32_t frame_count;
	const uint16_t *r_mean; /* 12.0 bit precision */
	const uint16_t *g_mean; /* 12.0 bit precision */
	const uint16_t *b_mean; /* 12.0 bit precision */
	const uint16_t *y_mean; /* 12.0 bit precision */
	const uint16_t *rg; /* 3.8 bit precision */
	const uint16_t *bg; /* 3.8 bit precision */
	const uint16_t *pixels;
	uint8_t resv[8];
};

struct rts_isp_y_stat_info {
	rts_isp_grid_size_t win;
	uint16_t hist_bins;
};

struct rts_isp_y_statis {
	uint32_t frame_count;
	const uint16_t *y_mean;
	const uint32_t *hist;
	uint8_t resv[8];
};

struct rts_isp_other_algo {
	struct rts_isp_flick_algo flick;
	struct rts_isp_wdr_algo wdr;
};

#define RTS_ISP_DEFINE_OTHER_ALGO_PLUGIN(other_algo_ops)                       \
	const struct rts_isp_other_algo *rts_isp_get_other_algo(void)          \
	{                                                                      \
		return &other_algo_ops;                                        \
	}

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_OTHER_H_INC_ */
