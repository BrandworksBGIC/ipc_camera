/*
 * Realtek Semiconductor Corp.
 *
 * rtstream/osdi/include/librtsosdi.h
 *
 * Copyright (C) 2020      Luis Han<luis_han@realsil.com.cn>
 */

#ifndef _LIB_RTSOSDI_H_
#define _LIB_RTSOSDI_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <rtsc.h>

#define RTS_OSDI_MAX_BLK_NUM		6
#define RTS_OSDI_PICT_NUM			2
#define CMP(a, b, c) (((a) <= (b)) && ((b) <= (c)))

typedef const void *RtsOsdiInst;

struct rts_osdi_blk {
	uint8_t index;
	uint8_t enable;
	uint32_t fmt;
	void *pdata;
	uint32_t length;
	uint16_t width;
	uint16_t height;
	uint16_t start_x;
	uint16_t start_y;
	uint16_t sram;
	uint8_t transfer_length;
	uint32_t fixed_buf_len;
};

struct rts_osdi_frame_info {
	uint32_t fmt;
	uint16_t width;
	uint16_t height;
};

int rts_osdi_init(RtsOsdiInst *posd, int num);
int rts_osdi_release(RtsOsdiInst posd);
int rts_osdi_set_blk(RtsOsdiInst posd, int blkidx);
int rts_osdi_get_blk(RtsOsdiInst posd, int blkidx);
int rts_osdi_config_frame(RtsOsdiInst posd,
			struct rts_osdi_frame_info *frame_info);
int rts_osdi_config_block(RtsOsdiInst posd,
			int blkidx, struct rts_osdi_blk *block);
int rts_osdi_disable_block(RtsOsdiInst posd, int blkidx);
int rts_osdi_enable_block(RtsOsdiInst posd, int blkidx);
int rts_osdi_start(RtsOsdiInst posd);
int rts_osdi_frame_stop(RtsOsdiInst posd);
int rts_osdi_immediate_stop(RtsOsdiInst posd);
int rts_osdi_query_block(RtsOsdiInst posd, int blkidx,
			struct rts_osdi_blk *block);
int rts_osdi_set_color_table(int fmt, uint32_t val,
				uint8_t red, uint8_t green, uint8_t blue,
				uint8_t alpha);
int rts_osdi_get_color_table(int fmt, uint32_t *val,
				uint8_t red, uint8_t green,
				uint8_t blue, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif

