/*
 * Realtek Semiconductor Corp.
 *
 * rtstream/md/include/rts_isp_md.h
 *
 * Copyright (C) 2019      Anakin Wang<anakin_wang@realsil.com.cn>
 */

#ifndef _LIB_RTSMD_H_
#define _LIB_RTSMD_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <rtsc.h>

enum MD_BIN_BITS {
	MD_BIN_BITS_2 = 0,
	MD_BIN_BITS_4,
	MD_BIN_BITS_MAX,
};

enum MD_NR_BINS {
	MD_NR_BINS_16 = 0,
	MD_NR_BINS_32,
	MD_NR_BINS_MAX,
};

struct rts_isp_md_attr {
	/* scale is not useful for roi */
	struct {
		uint32_t x, y;
		uint32_t w, h;
		uint32_t scale_x, scale_y;
	} md_in, md_out, roi;

	enum MD_BIN_BITS bin_bits;
	enum MD_NR_BINS nr_bins;
	uint32_t train_frames;

	struct {
		uint8_t back;
		uint8_t learn;
		uint8_t forget;
		uint8_t ds;
		uint32_t motion;
	} thd;
};


struct rts_isp_md_extra_attr {
	struct {
		//0: 128 bytes, 1:256 bytes
		int hist_write;
		int res_write;
		int isp_read;
	} burst_length;

	struct {
		int enable;
		int frames; // 0 - 100
	} skip;
};


struct rts_isp_md_result {
	int motion_flag;
	uint32_t motion_count;
};

int rts_isp_md_init(void **md);
void rts_isp_md_release(void *md);

int rts_isp_md_set_attr(void *md, struct rts_isp_md_attr *attr);
int rts_isp_md_get_attr(void *md, struct rts_isp_md_attr *attr);
int rts_isp_md_get_fd(void *md);

/*
 *extra can be set optionally, it has a default val
 */
int rts_isp_md_set_extra_attr(void *md, struct rts_isp_md_extra_attr *attr);
int rts_isp_md_get_extra_attr(void *md, struct rts_isp_md_extra_attr *attr);

int rts_isp_md_trigger_train(void *md);

int rts_isp_md_set_dma_addr(void *md,
			uint32_t hist_addr, uint32_t hist_sz,
			uint32_t res_addr, uint32_t res_sz);
int rts_isp_md_calc_dma_size(struct rts_isp_md_attr *attr,
			uint32_t *hist_sz,
			uint32_t *res_sz);

/*
 * set buffer->length & alloc
 * it will generate phy_addr, vm_addr
 */
int rts_isp_md_alloc_dma(struct rtsc_dma_buffer *buffer);
void rts_isp_md_free_dma(struct rtsc_dma_buffer *buffer);

int rts_isp_md_set_axi_buffer(void *md,
		uint32_t hist_addr, uint32_t hist_len,
		uint32_t result_addr, uint32_t result_len);

int rts_isp_md_enable(void *md, int enable);

/* return RTS_TRUE if POLLIN */
int rts_isp_md_poll(void *md, int timeout_ms);
int rts_isp_md_done(void *md);

#define RTSMD_FL_AFIFO_OL	(1<<9)
#define RTSMD_FL_RESULT_OL	(1<<8)
#define RTSMD_FL_HISTO_OL	(1<<7)
#define RTSMD_FL_MTD_UL		(1<<6)
#define RTSMD_FL_RESULT_DDR_OL	(1<<5)
#define RTSMD_FL_HISTO_DDR_OL	(1<<4)
#define RTSMD_FL_RESULT_AXI_OL	(1<<3)
#define RTSMD_FL_HISTO_AXI_OL	(1<<2)

#define RTSMD_OL_MASK		(0x3fc)

/*
 *return RTS_TRUE if events, status will be set if POLLERR
 *return RTS_FALSE if no events
 */
int rts_isp_md_poll2(void *md, int timeout_ms, uint32_t *status);

int rts_isp_md_get_result(void *md, struct rts_isp_md_result *res);
void rts_isp_md_dump_regs(void *md);


#ifdef __cplusplus
}
#endif

#endif
