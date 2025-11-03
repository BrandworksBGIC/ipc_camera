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

#ifndef _RTS_ISP_TUNING_H_INC_
#define _RTS_ISP_TUNING_H_INC_

#include <stdint.h>
#include <rts_isp.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum rts_isp_tuning_statis_type {
	RTS_ISP_TUNING_STATIS_ALL = 1,
	RTS_ISP_TUNING_STATIS_AE = 2,
	RTS_ISP_TUNING_STATIS_AWB = 3,
	RTS_ISP_TUNING_STATIS_AF = 4,
	RTS_ISP_TUNING_STATIS_FLICK = 5,
	RTS_ISP_TUNING_STATIS_RAW = 6,
	RTS_ISP_TUNING_STATIS_Y = 7,
	__RTS_ISP_TUNING_STATIS_BOUND,
};

struct rts_isp_tuning_statis {
	enum rts_isp_tuning_statis_type type;
	const void * const buf;
	const uint32_t len;
};

int rts_isp_tuning_query_statis(uint32_t isp_id,
				struct rts_isp_tuning_statis **statis);
int rts_isp_tuning_get_statis(uint32_t isp_id,
			      struct rts_isp_tuning_statis *statis);
void rts_isp_tuning_release_statis(uint32_t isp_id,
				   struct rts_isp_tuning_statis *statis);

enum rts_isp_tuning_param_type {
	RTS_ISP_TUNING_PARAM_ALL = 1,
	RTS_ISP_TUNING_PARAM_BLC = 2,
	RTS_ISP_TUNING_PARAM_NLSC = 3,
	RTS_ISP_TUNING_PARAM_MLSC = 4,
	RTS_ISP_TUNING_PARAM_CCM = 5,
	RTS_ISP_TUNING_PARAM_GAMMA = 6,
	RTS_ISP_TUNING_PARAM_YGC = 7,
	RTS_ISP_TUNING_PARAM_UVTUNE = 8,
	RTS_ISP_TUNING_PARAM_SPE = 9,
	RTS_ISP_TUNING_PARAM_YGAMMA = 10,
	RTS_ISP_TUNING_PARAM_TEXTURE = 11,
	RTS_ISP_TUNING_PARAM_AE = 32,
	RTS_ISP_TUNING_PARAM_AWB = 33,
	RTS_ISP_TUNING_PARAM_AF = 34,
	RTS_ISP_TUNING_PARAM_FLICK = 35,
	RTS_ISP_TUNING_PARAM_WDR = 36,
	RTS_ISP_TUNING_PARAM_DAYNIGHT = 37,
	RTS_ISP_TUNING_PARAM_HIGH_TEMP = 38,
	__RTS_ISP_TUNING_PARAM_BOUND,
};

struct rts_isp_tuning_param {
	enum rts_isp_tuning_param_type type;
	uint8_t *buf;
	uint32_t len;
	const uint32_t max_len;
};

int rts_isp_tuning_query_param(uint32_t isp_id,
			       struct rts_isp_tuning_param **param);
int rts_isp_tuning_get_param(uint32_t isp_id,
			     struct rts_isp_tuning_param *param);
int rts_isp_tuning_set_param(uint32_t isp_id,
			     struct rts_isp_tuning_param *param);
void rts_isp_tuning_release_param(uint32_t isp_id,
				  struct rts_isp_tuning_param *param);
enum rts_isp_tuning_iq_type {
	RTS_ISP_TUNING_IQ_TABLE_ALL = 1,
	RTS_ISP_TUNING_IQ_TABLE_BLC = 2,
	RTS_ISP_TUNING_IQ_TABLE_NLSC = 3,
	RTS_ISP_TUNING_IQ_TABLE_MLSC = 4,
	RTS_ISP_TUNING_IQ_TABLE_CCM = 5,
	RTS_ISP_TUNING_IQ_TABLE_GAMMA = 6,
	RTS_ISP_TUNING_IQ_TABLE_YGC = 7,
	RTS_ISP_TUNING_IQ_TABLE_UVTUNE = 8,
	RTS_ISP_TUNING_IQ_TABLE_SPE = 9,
	RTS_ISP_TUNING_IQ_TABLE_YGAMMA = 10,
	RTS_ISP_TUNING_IQ_TABLE_TEXTURE = 11,
	RTS_ISP_TUNING_IQ_TABLE_AE = 32,
	RTS_ISP_TUNING_IQ_TABLE_AWB = 33,
	RTS_ISP_TUNING_IQ_TABLE_AF = 34,
	RTS_ISP_TUNING_IQ_TABLE_FLICK = 35,
	RTS_ISP_TUNING_IQ_TABLE_WDR = 36,
	RTS_ISP_TUNING_IQ_TABLE_DAYNIGHT = 37,
	RTS_ISP_TUNING_IQ_TABLE_HIGH_TEMP = 38,
	__RTS_ISP_TUNING_IQ_TABLE_BOUND
};

struct rts_isp_tuning_iq {
	enum rts_isp_tuning_iq_type type;
	void *buf;
	uint32_t len;
	const uint32_t max_len;
};

struct rts_isp_tuning_reg {
	uint32_t offset;
	uint32_t value;
	uint32_t mask; /* 0 means no mask */
};

int rts_isp_tuning_query_iq(uint32_t isp_id,
			    struct rts_isp_tuning_iq **iq);
int rts_isp_tuning_get_iq(uint32_t isp_id,
			  struct rts_isp_tuning_iq *iq);
int rts_isp_tuning_set_iq(uint32_t isp_id,
			  struct rts_isp_tuning_iq *iq);
void rts_isp_tuning_release_iq(uint32_t isp_id,
			       struct rts_isp_tuning_iq *iq);
int rts_isp_tuning_read_regs(uint32_t isp_id,
			     struct rts_isp_tuning_reg *regs, uint32_t num);
int rts_isp_tuning_write_regs(uint32_t isp_id,
			      struct rts_isp_tuning_reg *regs, uint32_t num);
const char *rts_isp_tuning_packed_iq_path(uint32_t isp_id);
int rts_isp_tuning_reload_packed_iq(uint32_t isp_id);

struct rts_isp_tuning_packed_iq_info {
	uint32_t version;
	uint8_t cus_version;
	uint64_t timestamp;
};

int rts_isp_tuning_get_packed_iq_info(
	uint32_t isp_id, struct rts_isp_tuning_packed_iq_info *info);

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_TUNING_H_INC_ */
