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

#ifndef _INCLUDE_RTS_RTSTREAM_H
#define _INCLUDE_RTS_RTSTREAM_H

#include <linux/types.h>
#include "rtstream_mod.h"

struct rtstream_cmd_t {
	__u32 status;
	__u32 target;
	__u32 cmdcode;
	__s32 errcode;
	__u32 args[4];
	__u32 reserved[8];
	__u8 data[1024];
};

struct rtstream_sys_t {
	struct rtstream_cmd_t info;
	__u8 sn_bitmap[64];
	__u8 reserved[64];
	__u32 vreg[1024];
};

struct rtstream_cfg_info {
	void *cfg;
	__u32 cfg_size;
	char name[32];
};

enum RTSTREAM_SIG_OP {
	RTSTREAM_SIG_OP_W_PARAM = 0,
	RTSTREAM_SIG_OP_R_PARAM,
};

struct rtstream_signal_info {
	int chnno;
	int pid;
};

enum RTSTREAM_STREAM_OP {
	RTSTREAM_STREAM_OP_CREATE = 0,
	RTSTREAM_STREAM_OP_RELEASE,
};

struct rtstream_stream_info {
	uint32_t stream_pid;
	uint32_t stream_op;
};

enum RTSTREAM_CHN_OP {
	RTSTREAM_CHN_OP_CREATE = 0,
	RTSTREAM_CHN_OP_START,
	RTSTREAM_CHN_OP_STOP,
	RTSTREAM_CHN_OP_RELEASE,
};

struct rtstream_chn_info {
	uint32_t chn_id;
	uint32_t chn_no;
	uint32_t chn_pid;
	uint32_t chn_op;
};

enum RTSTREAM_PIPE_OP {
	RTSTREAM_PIPE_OP_BIND = 0,
	RTSTREAM_PIPE_OP_UNBIND,
};

struct rtstream_chn_pipe {
	uint32_t pipe_op;
	uint32_t src_id;
	uint32_t src_no;
	uint32_t dst_id;
	uint32_t dst_no;
	uint32_t pipe_pid;
};

#define RTSTREAM_UNIT_OUTBUF_MAX	3

struct rtstream_statis {
	char name[10];
	uint8_t unit_state; /* 0 disable, 1 enable, 2 pause */
	unsigned long poll_times;
	unsigned long run_times;
	unsigned long do_run_times;
	unsigned long run_success_times;
	unsigned long run_fail_times;
	uint8_t buf_state[RTSTREAM_UNIT_OUTBUF_MAX]; /* 0 unused, 1 idle, 2 out */
};

struct rtstream_mem {
	struct rtstream_statis statis;
};

struct rtstream_mem_info {
	uint8_t index;
	uint32_t chnno;
	uint32_t pid;
};

#define RTSTREAM_BITMAP_NUM		3
#define RTSTREAM_MEM_STEPSIZE		1024
#define RTSTREAM_MEM_STEPNUM		(RTSTREAM_BITMAP_NUM * 8)
#define RTSTREAM_MEM_TOTAL_SIZE	(RTSTREAM_MEM_STEPNUM * RTSTREAM_MEM_STEPSIZE)

#define RTS_RTSTREAM_IOC_MAGIC		'R'

#define RTSTREAM_IOC_G_MEM		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x0, struct rtstream_mem_info)
#define RTSTREAM_IOC_P_MEM		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x1, struct rtstream_mem_info)
#define RTSTREAM_IOC_LOCK		_IO(RTS_RTSTREAM_IOC_MAGIC, 0x10)
#define RTSTREAM_IOC_UNLOCK		_IO(RTS_RTSTREAM_IOC_MAGIC, 0x11)
#define RTSTREAM_IOC_G_CFG		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x20, struct rtstream_cfg_info)
#define RTSTREAM_IOC_S_CFG		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x21, struct rtstream_cfg_info)
#define RTSTREAM_IOC_C_CFG		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x22, struct rtstream_cfg_info)
#define RTSTREAM_IOC_STREAM_INFO	_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x30, struct rtstream_stream_info)
#define RTSTREAM_IOC_CHN_INFO		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x31, struct rtstream_chn_info)
#define RTSTREAM_IOC_CHN_PIPE		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x32, struct rtstream_chn_pipe)
#define RTSTREAM_IOC_GCFG_SHLR		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x33, bool)
#define RTSTREAM_IOC_SIGNAL_INFO	_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x40, struct rtstream_signal_info)
/* h26x debug */
#define RTSTREAM_IOC_H26X_SIG		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x41, int)
#define RTSTREAM_IOC_H26X_OP		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x42, struct rtstream_h26x_op)
/* h26x debug */
#define RTSTREAM_IOC_AEC_SIG		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x43, int)
#define RTSTREAM_IOC_AEC_OP		_IOWR(RTS_RTSTREAM_IOC_MAGIC, 0x44, struct rtstream_aec_op)

#endif
