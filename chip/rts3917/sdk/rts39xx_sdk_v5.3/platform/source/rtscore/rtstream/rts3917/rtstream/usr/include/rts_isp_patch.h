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

#ifndef _RTS_ISP_PATCH_H_INC_
#define _RTS_ISP_PATCH_H_INC_

#include <rts_isp_errno.h>
#include <rts_isp_dynamic.h>
#include <isp_iq_table.pb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PATCH_API_VERSION_MAGIC ((uint32_t)'p' << 8 | (uint32_t)'h')
#define PATCH_API_MAJOR_VERSION 1
#define PATCH_API_MINOR_VERSION 0
#define PATCH_API_VERSION                                               \
	(PATCH_API_VERSION_MAGIC << 16 | PATCH_API_MAJOR_VERSION << 8 | \
	 PATCH_API_MINOR_VERSION)
#define PATCH_VERSION_MASK ((1 << 16) - 1)

struct rts_isp_sensor_patch_ops {
	uint32_t api_version;
	int (*iq_change)(uint32_t isp_id, int iq_sel, int night);
	int (*dynamic)(uint32_t isp_id, const struct isp_notify_dynamic *dyn);
	/* optional */
	int (*init)(uint32_t isp_id);
	int (*cleanup)(uint32_t isp_id);
	int (*preview_start)(uint32_t isp_id);
	int (*preview_stop)(uint32_t isp_id);
};

/* these APIs can only be used in sensor patch */
int rts_isp_patch_set_ccm_patch_mode(uint32_t isp_id, int32_t enable);
int rts_isp_patch_set_ccm_v1_dyn(uint32_t isp_id,
				 isp_iq_ccm_v1_dyn_item_t *item);
int rts_isp_patch_set_blc_patch_mode(uint32_t isp_id, int32_t enable);
int rts_isp_patch_set_blc_v1_dyn(uint32_t isp_id,
				 isp_iq_blc_v1_dyn_item_t *item);
int rts_isp_patch_set_gamma_patch_mode(uint32_t isp_id, int32_t enable);
int rts_isp_patch_set_gamma_v1_dyn(uint32_t isp_id,
				   isp_iq_gamma_v1_dyn_item_t *item);
int rts_isp_patch_set_ygc_patch_mode(uint32_t isp_id, int32_t enable);
int rts_isp_patch_set_ygc_v1_dyn(uint32_t isp_id,
				 isp_iq_ygc_v1_dyn_item_t *item);
int rts_isp_patch_set_ygamma_patch_mode(uint32_t isp_id, int32_t enable);
int rts_isp_patch_set_ygamma_v1_dyn(uint32_t isp_id,
				    isp_iq_ygamma_v1_dyn_item_t *item);
uint32_t rts_isp_patch_read_reg(uint32_t isp_id, uint32_t offset);
void rts_isp_patch_write_reg(uint32_t isp_id, uint32_t value, uint32_t offset);
void rts_isp_patch_write_reg_mask(uint32_t isp_id, uint32_t value,
				  uint32_t offset, uint32_t mask);

#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_PATCH_H_INC_ */

