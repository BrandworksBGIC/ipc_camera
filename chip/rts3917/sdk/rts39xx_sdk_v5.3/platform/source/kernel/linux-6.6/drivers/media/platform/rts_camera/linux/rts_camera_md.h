/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _RTS_CAMERA_MD_H_
#define _RTS_CAMERA_MD_H_

#include <linux/types.h>

#define RTSMD_IOC_MAGIC 'm'

struct rtscam_md_extra_attr {
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

enum RTSMD_BIN_BITS {
	RTSMD_BIN_BITS_2 = 0,
	RTSMD_BIN_BITS_4,
	RTSMD_BIN_BITS_MAX,
};

enum RTSMD_NR_BINS {
	RTSMD_NR_BINS_16 = 0,
	RTSMD_NR_BINS_32,
	RTSMD_NR_BINS_MAX,
};

struct rtscam_md_attr {
	/* scale is not useful for roi */
	struct {
		__u32 x, y;
		__u32 w, h;
		__u32 scale_x, scale_y;
	} md_in, md_out, roi;

	enum RTSMD_BIN_BITS bin_bits;
	enum RTSMD_NR_BINS nr_bins;
	__u32 train_frames;

	struct {
		__u8 back;
		__u8 learn;
		__u8 forget;
		__u8 ds;
		__u32 motion;
	} thd;
};

struct rtscam_md_buffer {
	__u32 hist_addr;
	__u32 hist_length;
	__u32 res_addr;
	__u32 res_length;
};

struct rtscam_md_result {
	int motion_flag;
	__u32 motion_count;
};

#define RTSMD_IOC_ENABLE	_IO(RTSMD_IOC_MAGIC, 1)
#define RTSMD_IOC_DISABLE	_IO(RTSMD_IOC_MAGIC, 2)
#define RTSMD_IOC_DONE		_IO(RTSMD_IOC_MAGIC, 3)
#define RTSMD_IOC_STATUS	_IOR(RTSMD_IOC_MAGIC, 4, unsigned int)

#define RTSMD_IOC_SET_EXTRA_ATTR	_IOW(RTSMD_IOC_MAGIC, 5,	\
						struct rtscam_md_extra_attr)
#define RTSMD_IOC_SET_ATTR		_IOW(RTSMD_IOC_MAGIC, 6,	\
						struct rtscam_md_attr)
#define RTSMD_IOC_GET_ATTR		_IOR(RTSMD_IOC_MAGIC, 7,	\
						struct rtscam_md_attr)
#define RTSMD_IOC_TRIGGER_TRAIN		_IO(RTSMD_IOC_MAGIC, 8)
#define RTSMD_IOC_SET_DMA_ADDR		_IOW(RTSMD_IOC_MAGIC, 9,	\
						struct rtscam_md_buffer)
#define RTSMD_IOC_SET_AXI_BUFFER	_IOW(RTSMD_IOC_MAGIC, 10,	\
						struct rtscam_md_buffer)
#define RTSMD_IOC_GET_RESULT		_IOR(RTSMD_IOC_MAGIC, 11,	\
						struct rtscam_md_result)
#define RTSMD_IOC_GET_EXTRA_ATTR	_IOR(RTSMD_IOC_MAGIC, 12,	\
						struct rtscam_md_extra_attr)

#define RTSMD_IOC_MAXNR   12

#endif
