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

#ifndef _RTS_ISP_ISP_DEFINE_H_INC_
#define _RTS_ISP_ISP_DEFINE_H_INC_

#include <rts_isp_geom.h>

/* IMPORTANT: DO NOT define hardware-dependent symbol here */

/**
 * @defgroup isp ISP of Realtek IPCam
 * @brief Structure and API for setting of Realtek IPCam ISP.
 * ISP must be configured before ISP video stream.
 * @ingroup total
 * @{
 */
#define RTS_ISP_NAME_LEN 32
#define RTS_ISP_PATH_LEN 256

/**
 * @brief ISP ID
 */
enum isp_id {
	ISP0, /**< the first ISP.Only one ISP is supported temporarily. */
	_ISP_ID_NUM,
};

#define RTS_ISP_HDR_CHAN_MAX 4

/**
 * @brief hdr mode of ISP
 */
enum rts_isp_hdr_mode {
	RTS_ISP_HDR_NONE = 0, /**< linear mode */
	RTS_ISP_HDR_LINE_2TO1 = 0x10, /**< stagger/dol hdr mode */
};

static inline int hdr_chan_num(enum rts_isp_hdr_mode hdr)
{
	if (hdr == RTS_ISP_HDR_LINE_2TO1)
		return 2;
	return 1;
}

#define RTS_ISP_ROI_AREA_MAX 8

/**
 * @brief mode of sensor
 */
struct rts_isp_sensor_mode {
	enum rts_isp_hdr_mode hdr; /**< sensor hdr mode */
	rts_isp_size_t size; /**< sensor size after isp crop */
	float fps; /**< sensor base fps */
};

/**
 * @brief modes list of sensor
 */
struct rts_isp_sensor_modes {
	uint32_t num; /**< modes num */
	struct rts_isp_sensor_mode mode[8]; /**< modes list array */
};

/*!@}*/

#endif /* _RTS_ISP_ISP_DEFINE_H_INC_ */
