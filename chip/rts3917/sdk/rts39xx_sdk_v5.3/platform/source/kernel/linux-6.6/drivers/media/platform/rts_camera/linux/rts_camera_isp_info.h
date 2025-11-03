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

#ifndef _U_RTS_CAMERA_ISP_INFO_H
#define _U_RTS_CAMERA_ISP_INFO_H

#include <linux/types.h>
#include <linux/videodev2.h>

/* MUST be same as struct isp_message */
struct rts_isp_msg_hdr {
	__u32 sequence; /* set by internal */
	__u32 msg_len;
	__u32 ret_len;
	__u32 isp_id;
	__u32 mod_id;
	__u32 action;
	__s32 ret_val;
	__u16 reloc_pos;
	__u16 reloc_num;
};

struct rts_isp_info {
	__u16 width;
	__u16 height;
	struct v4l2_fract fps_max;
	struct v4l2_fract fps_min;
};

struct rts_isp_preview_info {
	struct v4l2_fract fps;
};

struct rts_isp_statis_info {
	__u32 phy_addr;
	__u32 size;
	__u32 num;
};

struct rts_isp_statis_awb_reg {
	__u32 illum_white_pixels[6];
	__u32 fine_r_sum;
	__u32 fine_g_sum;
	__u32 fine_b_sum;
	__u32 fine_white_pixels;
};

struct rts_isp_statis_flick_reg {
	__u32 fft_sum2_9;
	__u32 fft_sum2_127;
	__u32 valid;
};

struct rts_isp_statis_data {
	__u32 buf_id;

	__u32 frame_count;
	struct rts_isp_statis_awb_reg awb_reg;
	struct rts_isp_statis_flick_reg flick_reg;
};

enum rts_isp_mem_sync_type {
	RTS_ISP_SYNC_FOR_DEVICE,
	RTS_ISP_SYNC_FOR_CPU,
};

enum rts_isp_mem_dma_dir {
	RTS_ISP_DMA_BIDIRECTIONAL,
	RTS_ISP_DMA_TO_DEVICE,
	RTS_ISP_DMA_FROM_DEVICE,
};


#define _DRIVER_ACTC(dir, type, nr, size) \
	(__u32)((dir) << 30 | (size) << 16 | (type) << 8 | (nr) << 0)
#define _DRIVER_ACT(type, nr) _DRIVER_ACTC(0, type, nr, 0)
#define _DRIVER_ACTR(type, nr, size) _DRIVER_ACTC(1, type, nr, sizeof(size))
#define _DRIVER_ACTW(type, nr, size) _DRIVER_ACTC(2, type, nr, sizeof(size))
#define _DRIVER_ACTWR(type, nr, size) _DRIVER_ACTC(3, type, nr, sizeof(size))

#define RTS_ISP_EXEC_TYPE 'E'

enum rts_isp_driver_action {
	RTS_ISP_SET_FPS = _DRIVER_ACTW(RTS_ISP_EXEC_TYPE, 0,
				       struct rts_isp_preview_info),
	RTS_ISP_STATIS_DONE = _DRIVER_ACTW(RTS_ISP_EXEC_TYPE, 1,
					   struct rts_isp_statis_data),
};

#endif /* _U_RTS_CAMERA_ISP_INFO_H */
