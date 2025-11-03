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

#ifndef _INCLUDE_RTSCAM_HWVER_H
#define _INCLUDE_RTSCAM_HWVER_H

#include <stdint.h>

enum {
	RTSC_HW_ID_VER_UNKNOWN = 0,
	RTSC_HW_ID_VER_RLE0745,
	RTSC_HW_ID_VER_RTS3901,
	RTSC_HW_ID_VER_RTS3903,
	RTSC_HW_ID_VER_RTS3913,
	RTSC_HW_ID_VER_RTS3915,
	RTSC_HW_ID_VER_RTS3917,
};

struct rtsc_hw_id {
	uint16_t hw_ver;
	uint32_t hw_id;
};

struct rtsc_hw_id rtsc_get_hw_id(void);

#endif
