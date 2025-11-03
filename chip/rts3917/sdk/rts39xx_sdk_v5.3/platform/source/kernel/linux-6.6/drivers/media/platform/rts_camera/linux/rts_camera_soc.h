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

#ifndef _UAPI_RTS_CAMERA_SOC_H
#define _UAPI_RTS_CAMERA_SOC_H

#include <linux/types.h>

struct rtscam_soc_cmd_stru {
	__u16 cmdcode;
	__u8 index;
	__u8 length;
	__u16 param;
	__u16 addr;
	__u8 *buf;
	__u32 reserved[1];
};

struct rtscam_soc_ext_cmds {
	__u32 count;
	struct rtscam_soc_cmd_stru *cmds;
};

struct rtscam_soc_ldc_stru {
	unsigned int length;
	__u8 *ptable;
};

struct rtscam_soc_hw_ver {
	unsigned int hw_ver;
	unsigned int hw_id;
};

struct rtscam_soc_ive_ctrl {
	int enable;
	struct {
		__u8 r;
		__u8 g;
		__u8 b;
	} normal_mean;
	__u16 normal_scale;
	int8_t quant_len;
	__u32 asym_inv_scale;
	__u8 asym_zero_point;
};

#define RTSOCIOC_G_STREAMID	_IOR('s', 0x30, int)
#define RTSOCIOC_G_HWOFFSET	_IOR('s', 0x31, unsigned long)
#define RTSOCIOC_G_HWIOSIZE	_IOR('s', 0x32, unsigned int)

#define RTSOCIOC_PAUSE		_IO('s', 0x60)
#define RTSOCIOC_RESUME		_IO('s', 0x61)

#define RTSOCIOC_S_IVE_CTRL	_IOW('s', 0x70, struct rtscam_soc_ive_ctrl)
#define RTSOCIOC_G_IVE_CTRL	_IOR('s', 0x71, struct rtscam_soc_ive_ctrl)

#define RTSOCIOC_CAMERA_DETACH _IO('s', 0x50)
#define RTSOCIOC_CAMERA_ATTACH _IO('s', 0x51)

struct rts_zoom_color_range_cfg {
	int stream_id;
	int limit_enable;
};

#define RTSZOOMIOC_SET_SUBDEV	_IOW('z', 0x01, int)
#define RTSZOOMIOC_GET_SUBDEV	_IOR('z', 0x02, int)
#define RTSZOOMIOC_SET_LIMIT_COLOR_RANGE \
	_IOW('z', 0x03, struct rts_zoom_color_range_cfg)
#define RTSZOOMIOC_GET_LIMIT_COLOR_RANGE \
	_IOWR('z', 0x04, struct rts_zoom_color_range_cfg)
#define RTSZOOMIOC_SET_DYNAMIC_FPS \
	_IOW('z', 0x05, struct v4l2_fract)

#endif
