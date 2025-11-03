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

#ifndef _RTS_ISP_AF_H_INC_
#define _RTS_ISP_AF_H_INC_

#include <stdint.h>
#include <rts_isp_errno.h>
#include <rts_isp_geom.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define AF_API_VERSION_MAGIC ((uint32_t)'a' << 8 | (uint32_t)'f')
#define AF_API_MAJOR_VERSION 1
#define AF_API_MINOR_VERSION 1
#define AF_API_VERSION (AF_API_VERSION_MAGIC << 16 | \
			AF_API_MAJOR_VERSION << 8 | AF_API_MINOR_VERSION)
#define AF_VERSION_MASK ((1 << 16) - 1)

struct rts_isp_af_stat_info {
	rts_isp_grid_size_t win;
};

struct rts_isp_af_iq_info {
	uint32_t magic;
	uint32_t version;
	const void *iq;
	uint8_t resv[8];
};

struct rts_isp_af_param_info {
	uint32_t magic;
	uint32_t version;
	void *algo_param;
	uint8_t resv[8];
};

struct rts_isp_af_info {
	struct rts_isp_af_stat_info stat_info;
	const void *iq;
	void *algo_param; /* only used for internal */
	uint8_t resv[8];
};

struct rts_isp_af_statis {
	uint32_t frame_count;
	const uint16_t *y_mean;
	const uint16_t *high_illum_cnt;
	const uint32_t *gr_focus_value;
	const uint32_t *gb_focus_value;
	uint8_t resv[8];
};

struct rts_isp_af_position {
	uint32_t need_update;
	uint32_t position;
};

struct rts_isp_af_statis_config {
	uint32_t need_update;
	uint8_t resv[32];
};

struct rts_isp_af_result {
	struct rts_isp_af_position result;
	struct rts_isp_af_statis_config config;
	uint8_t resv[8];
};

/* pointers for ctrl are only valid in ctrl callback */
enum rts_isp_af_ctrls {
	SET_AE_GAIN, /* uint32_t */
	SET_AE_ALGO_STATUS, /* enum rts_isp_ae_algo_status */
};

struct rts_isp_af_algo {
	uint32_t api_version;
	int (*init)(uint32_t isp_id, const struct rts_isp_af_info *info);
	int (*run)(uint32_t isp_id, const struct rts_isp_af_statis *statis,
		   struct rts_isp_af_result *result);
	int (*ctrl)(uint32_t isp_id, enum rts_isp_af_ctrls id, ...);
	int (*cleanup)(uint32_t isp_id);
};

#define RTS_ISP_DEFINE_AF_ALGO_PLUGIN(af_algo_ops)                             \
	const struct rts_isp_af_algo *rts_isp_get_af_algo(void)                \
	{                                                                      \
		return &af_algo_ops;                                           \
	}

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_AF_H_INC_ */
