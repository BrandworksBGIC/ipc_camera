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

#ifndef _LINUX_RTS_ISP_MEM_H
#define _LINUX_RTS_ISP_MEM_H

enum {
	RTSCAM_ISP_BUF_COHERENT = 0,
	RTSCAM_ISP_BUF_FROM_DEVICE = 1,
	RTSCAM_ISP_BUF_TO_DEVICE = 2,
	RTSCAM_ISP_BUF_BIDIRECTIONAL = 3,
};

struct rtscam_isp_dma_buf {
	unsigned long size;
	off_t offset;
	unsigned long dma_addr;
	unsigned long vaddr;
	uint32_t buf_io;
	uint32_t direction;
	char name[32];
};

struct rtscam_adma_cp_stru {
	unsigned long dst;
	unsigned long src;
	unsigned long size;
};

struct rtscam_isp_dma_info {
	char info[32];
	unsigned long size;
	unsigned long dma_addr;
	uint32_t buf_io;
	int32_t index;
};

struct rtscam_isp_dma_ext_infos {
	struct rtscam_isp_dma_info *infos;
	uint32_t number;
};

#define RTSOCIOC_ALLOC_DMA  _IOWR('s', 0x38, struct rtscam_isp_dma_buf)
#define RTSOCIOC_FREE_DMA   _IOW('s', 0x39, unsigned long)
#define RTSOCIOC_REALLOC_DMA _IOWR('s', 0x40, struct rtscam_isp_dma_buf)
#define RTSOCIOC_SET_DMA_INFO	_IOW('s', 0x41, struct rtscam_isp_dma_info)
#define RTSOCIOC_GET_DMA_INFO	_IOWR('s', 0x42, struct rtscam_isp_dma_info)
#define RTSOCIOC_PRE_ALLOC_MEM	_IOWR('s', 0x43, struct rtscam_isp_dma_info)
#define RTSOCIOC_SET_PRE_ALLOC_STATUS	_IOWR('s', 0x44, int)
#define RTSOCIOC_GET_PRE_ALLOC_STATUS	_IOWR('s', 0x45, int)
#define RTSOCIOC_REMOVE_CMA_MEM		_IO('s', 0x46)
#define RTSOCIOC_SYNC_FOR_DEVICE _IOWR('s', 0x60, struct rtscam_isp_dma_buf)
#define RTSOCIOC_SYNC_FOR_CPU _IOWR('s', 0x61, struct rtscam_isp_dma_buf)
#define RTSOCIOC_GET_VIN_RING_HEIGHT	_IOWR('s', 0x62, uint32_t)
#define RTSOCIOC_ADMA_COPY	_IOW('s', 0x80, struct rtscam_adma_cp_stru)
#define RTSOCIOC_MEM_IS_IO	_IOWR('s', 0x81, unsigned long)
#endif
