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

#ifndef _RTS_ISP_GEOM_H_INC_
#define _RTS_ISP_GEOM_H_INC_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum rts_isp_geom_type {
	RTS_ISP_GRID,
	RTS_ISP_RECT,
};

typedef struct {
	int32_t x;
	int32_t y;
} rts_isp_point_t;

typedef struct {
	int32_t w;
	int32_t h;
} rts_isp_size_t;

typedef struct {
	union {
		struct {
			int32_t x;
			int32_t y;
			int32_t w;
			int32_t h;
		};
		struct {
			rts_isp_point_t start;
			rts_isp_size_t size;
		};
	};
} rts_isp_rect_t;

typedef struct {
	union {
		struct {
			int32_t left;
			int32_t top;
			int32_t right;
			int32_t bottom;
		};
		struct {
			rts_isp_point_t start;
			rts_isp_point_t end;
		};
	};
} rts_isp_prect_t;

typedef struct {
	int32_t cols;
	int32_t rows;
} rts_isp_grid_size_t;

typedef struct {
	rts_isp_point_t start;
	rts_isp_size_t cell;
	rts_isp_grid_size_t size;
} rts_isp_grid_t;

typedef struct {
	rts_isp_grid_size_t size;
	rts_isp_size_t cell_align;
} rts_isp_grid_info_t;

typedef struct {
	/* a * x + b * y + c = 0 */
	int32_t a;
	int32_t b;
	int32_t c;
} rts_isp_line_t;

typedef struct {
	float x;
	float y;
} rts_isp_pointf_t;

typedef struct {
	float w;
	float h;
} rts_isp_sizef_t;

typedef struct {
	union {
		struct {
			float x;
			float y;
			float w;
			float h;
		};
		struct {
			rts_isp_pointf_t start;
			rts_isp_sizef_t size;
		};
	};
} rts_isp_rectf_t;

typedef struct {
	union {
		struct {
			float left;
			float top;
			float right;
			float bottom;
		};
		struct {
			rts_isp_pointf_t start;
			rts_isp_pointf_t end;
		};
	};
} rts_isp_prectf_t;

typedef struct {
	/* a * x + b * y + c = 0 */
	float a;
	float b;
	float c;
} rts_isp_linef_t;

int rts_isp_get_grid_from_rect(rts_isp_grid_t *grid,
			       const rts_isp_rect_t *rect,
			       const rts_isp_grid_info_t *info);
int rts_isp_get_rect_from_grid(rts_isp_rect_t *rect,
			       const rts_isp_grid_t *grid);
int rts_isp_get_grid_from_prect(rts_isp_grid_t *grid,
				const rts_isp_prect_t *prect,
				const rts_isp_grid_info_t *info);
int rts_isp_get_prect_from_grid(rts_isp_prect_t *prect,
				const rts_isp_grid_t *grid);
int rts_isp_get_rect_from_prect(rts_isp_rect_t *rect,
				const rts_isp_prect_t *prect);
int rts_isp_get_prect_from_rect(rts_isp_prect_t *prect,
				const rts_isp_rect_t *rect);
#ifdef __cplusplus
}
#endif

#endif /* _RTS_ISP_GEOM_H_INC_ */
