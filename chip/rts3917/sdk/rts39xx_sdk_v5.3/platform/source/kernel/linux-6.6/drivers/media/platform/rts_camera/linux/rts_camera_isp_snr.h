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

#ifndef _U_RTS_CAMERA_ISP_SNR_H
#define _U_RTS_CAMERA_ISP_SNR_H

#include <linux/types.h>

enum rts_isp_snr_pwr_type {
	SNR_RST_GPIO,
	SNR_PWDN_GPIO,
	SNR_HCLK,
	SNR_IO_POWER,
	SNR_ANALOG_POWER,
	SNR_CORE_POWER,
	_MAX_SNR_POWER_TYPE,
};

struct rts_isp_snr_pwr_item {
	enum rts_isp_snr_pwr_type type;
	__u32 value;
	__u32 delay; /* us */
};

struct rts_isp_snr_pwr {
	__u32 num;
	struct rts_isp_snr_pwr_item items[16];
};

struct rts_isp_i2c_info {
	__u8 i2c_id;
	__u8 addr_len;
	__u8 data_len;
};

struct rts_isp_i2c_reg {
	__u16 addr;
	__u16 data;
};

struct rts_isp_i2c {
	struct rts_isp_i2c_info info;
	__u32 num;
	struct rts_isp_i2c_reg regs[16];
};

struct rts_isp_i2c_reg_mask {
	__u16 addr;
	__u16 data;
	__u16 mask;
};

struct rts_isp_reg_mask {
	__u32 addr;
	__u32 data;
	__u32 mask;
};

enum rts_isp_interrupt {
	RTS_ISP_INT_NONE,
	RTS_ISP_INT_DATA_START,
	RTS_ISP_INT_FRAME_END,
	_MAX_RTS_ISP_INT,
};

enum rts_isp_sync_type {
	RTS_ISP_SYNC_TYPE_INFO,
	RTS_ISP_SYNC_TYPE_I2C,
	RTS_ISP_SYNC_TYPE_REG,
	_MAX_RTS_ISP_SYNC_TYPE,
};

struct rts_isp_sync_info {
	__u32 delay_frames; /* total max 4 */
	enum rts_isp_interrupt interrupt;
};

struct rts_isp_sync_reg {
	enum rts_isp_sync_type type;
	union {
		struct rts_isp_i2c_reg_mask i2c;
		struct rts_isp_reg_mask reg;
		struct rts_isp_sync_info info;
	};
};

struct rts_isp_sync_regs {
	__u32 num;
	struct rts_isp_sync_reg *reg; /* max 32 regs for sensor */

	/* private fields, do not set in sensor model */
	struct rts_isp_i2c_info i2c_info;
	__u32 split_index;
};

#endif /* U_RTS_CAMERA_ISP_SNR_H */
