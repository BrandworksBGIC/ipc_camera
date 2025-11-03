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

#ifndef _INCLUDE_RTSC_H
#define _INCLUDE_RTSC_H

#include <stdint.h>
#include <rtstream.h>

enum {
	RTSC_BUF_COHERENT = 0,
	RTSC_BUF_FROM_DEVICE = 1,
	RTSC_BUF_TO_DEVICE = 2,
	RTSC_BUF_BIDIRECTIONAL = 3,
};

enum {
	RTSC_DMA_BUFFER_DEFAULT,
	RTSC_DMA_BUFFER_FROM_HEAD,
	RTSC_DMA_BUFFER_FROM_END,
	RTSC_CMA_BUFFER_DEFAULT = (0x1 << 8),
};

struct rtsc_dma_buffer {
	void *vm_addr;
	uint32_t length;
	uint32_t phy_addr;
	uint32_t buf_io;
	uint32_t direction;
	char name[32];
};

int rtsc_v4l2_open(int isp_id);
int rtsc_v4l2_close(int fd);
unsigned long rtsc_get_video_phy_addr(int fd, unsigned long vm_addr, int index);

int rtsc_zoom_open(void);
int rtsc_zoom_close(int fd);

int rtsc_ctrl_open(void);
int rtsc_ctrl_close(int fd);
int rtsc_exec_vendor_cmd(int fd, unsigned int cmdcode, void *arg);
int rtsc_get_streamid(int fd);

int rtsc_soc_open(int oflag);
int rtsc_soc_close(int fd);

int rtsc_mem_open(int oflag);
void rtsc_mem_close(int memfd);
int rtsc_mem_alloc(int memfd, struct rtsc_dma_buffer *buffer);
int rtsc_mem_free(int memfd, struct rtsc_dma_buffer *buffer);
int rtsc_mem_realloc(int memfd,
		struct rtsc_dma_buffer *buffer, uint32_t new_length);
int rtsc_mem_set_info(int memfd, uint32_t phy_addr, const char *info);
int rtsc_alloc_dma(struct rtsc_dma_buffer *buffer);
int rtsc_free_dma(struct rtsc_dma_buffer *buffer);
int rtsc_realloc_dma(struct rtsc_dma_buffer *buffer,
			uint32_t new_length);
int rtsc_set_dma_info(uint32_t phy_addr, const char *info);
int rtsc_mem_sync_device(unsigned long dma_addr, unsigned long size);
int rtsc_mem_sync_cpu(unsigned long dma_addr, unsigned long size);
int rtsc_dma_memcpy(void *dst, const void *src, size_t size);
void *rtsc_memcpy(void *dst, const void *src, size_t size);
int rtsc_mem_get_vin_ring_height(uint32_t *rh);

typedef const void *RtsRegInst;
int rtsc_reg_close(RtsRegInst reginst);
int rtsc_reg_isp_open(RtsRegInst *reginst);
int rtsc_reg_isp_close(RtsRegInst ispreg);
int rtsc_reg_h264_open(RtsRegInst *reginst);
int rtsc_reg_h264_close(RtsRegInst h264reg);
int rtsc_reg_osd2_open(RtsRegInst *reginst);
int rtsc_reg_osd2_close(RtsRegInst osd2reg);
uint32_t rtsc_reg_read_reg(RtsRegInst reginst, unsigned int offset);
void rtsc_reg_write_reg(RtsRegInst reginst,
		       unsigned int offset, uint32_t val);
void rtsc_reg_set_reg_bit(RtsRegInst reginst,
			 unsigned int offset, uint8_t bit_idx);
void rtsc_reg_clr_reg_bit(RtsRegInst reginst,
			 unsigned int offset, uint8_t bit_idx);
int rtsc_reg_get_handle(RtsRegInst reginst);
unsigned int rtsc_reg_get_size(RtsRegInst reginst);
int rtsc_reg_handle_sigio(RtsRegInst reginst);

void rtsc_reg_h264_done(RtsRegInst reginst);
void rtsc_reg_osd2_done(RtsRegInst reginst);

int rtsc_reg_osd2_wait_interrupt(RtsRegInst reginst);

int rtsc_reg_osd2_set_enabled(RtsRegInst reginst, int enable);
int rtsc_reg_h264_set_enabled(RtsRegInst reginst, int enable);

int rtsc_wq_lock_allocation(unsigned long key);
int rtsc_wq_lock_deallocate(unsigned long key);
int rtsc_wq_lock_init(unsigned long key, int num);
int rtsc_wq_lock_set_name(unsigned long key, const char *name);
int rtsc_wq_lock_wait(unsigned long key);
int rtsc_wq_lock_post(unsigned long key);

int rtsc_resume(void);
int rtsc_pause(void);

int rtsc_get_fov_mode(int fd);
int rtsc_set_fov_mode(int fd, int val);

int rtsc_soc_attach(void);
int rtsc_soc_detach(void);

int rtsc_set_rtstream_cfg(void *cfg, char *name, uint32_t cfg_size);
int rtsc_get_rtstream_cfg(void *cfg, char *name, uint32_t cfg_size);
int rtsc_clear_rtstream_cfg(char *name);
int rtsc_rtstream_lock(void);
int rtsc_rtstream_unlock(void);

int rtsc_rtstream_open(void);
int rtsc_rtstream_close(int fd);
int rtsc_sys_stream_create(void);
int rtsc_sys_stream_release(void);
int rtsc_sys_chn_create(uint32_t chn_id, uint32_t chn_no);
int rtsc_sys_chn_release(uint32_t chn_id, uint32_t chn_no);
int rtsc_sys_chn_bind(uint32_t src_id, uint32_t src_no,
			uint32_t dst_id, uint32_t dst_no);
int rtsc_sys_chn_unbind(uint32_t src_id, uint32_t src_no,
			uint32_t dst_id, uint32_t dst_no);

void *rtsc_sys_mmap_mem(void);
int rtsc_sys_munmap_mem(void *p);
int rtsc_sys_get_mem_index(int chnno);
int rtsc_sys_put_mem_index(int idx, int chnno);
void *rtsc_sys_get_mem_by_index(void *p, int idx);

int rtsc_sys_support_h26x_long_ref(void);
int rtsc_sys_signal_info(int *chnno, int *pid);

int rtsc_sys_h26x_signal(int *signal);
int rtsc_sys_h26x_op(struct rtstream_h26x_op *h26x_op);
int rtsc_sys_aec_signal(int *signal);
int rtsc_sys_aec_op(struct rtstream_aec_op *aec_op);

// For wave521 mv&var accelerate --------------------
struct rtsc_hpp_info {
	uint64_t motion_extent;
	uint64_t ssd_extent;
	uint64_t var;
	uint32_t report_addr;
	uint32_t intra_num;
	uint16_t width;
	uint16_t height;
	uint8_t work_mode;
};

int rtsc_h26x_pp_open(void);
int rtsc_h26x_pp_get_info(int fd, struct rtsc_hpp_info *info);
int rtsc_h26x_pp_close(int fd);
// ---------------------------------------------------

#endif
