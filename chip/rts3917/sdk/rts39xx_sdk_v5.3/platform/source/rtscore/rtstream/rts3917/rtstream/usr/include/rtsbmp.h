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

#ifndef _LIBRTSBMP_INCLUDE_RTSBMP_H
#define _LIBRTSBMP_INCLUDE_RTSBMP_H

#include <stdint.h>
#include <rtscolor.h>

enum {
	RTS_BMP_BITS_DATA_CONTINUOUS = 0,
	RTS_BMP_BITS_DATA_BYTE_ALIGN,
	RTS_BMP_BITS_DATA_LINE_ALIGN
};

struct rts_bmp_encin {
	uint8_t *psrc;
	int length;
	int fmt;
	int width;
	int height;
	int align;
};

int rts_bmp_encode(struct rts_bmp_encin *encin, uint8_t *pdst, int dst_len);
int rts_bmp_save(struct rts_bmp_encin *encin, const char *filename);

#endif
