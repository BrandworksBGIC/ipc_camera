//-----------------------------------------------------------------------------
// COPYRIGHT (C) 2020   CHIPS&MEDIA INC. ALL RIGHTS RESERVED
//
// This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
// SPDX License Identifier: BSD-3-Clause
// SPDX License Identifier: LGPL-2.1-only
//
// The entire notice above must be reproduced on all authorized copies.
//
// Description  :
//-----------------------------------------------------------------------------

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>
#include "vpuconfig.h"

#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VDI_IOCTL_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY	_IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY		_IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT			_IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE			_IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET						_IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL			_IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY			_IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE				_IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE			_IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM			_IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO			_IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_GET_FREE_MEM_SIZE			_IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_SET_START_TIMESTAMP			_IO(VDI_IOCTL_MAGIC, 14)
#define VDI_IOCTL_GENCODE_TIMEBUF			_IO(VDI_IOCTL_MAGIC, 15)
#define VDI_IOCTL_LOCK					_IO(VDI_IOCTL_MAGIC, 16)
#define VDI_IOCTL_UNLOCK				_IO(VDI_IOCTL_MAGIC, 17)
#define VDI_IOCTL_QUERY_BCLK			_IO(VDI_IOCTL_MAGIC, 18)
/*rts add*/
#define VDI_IOCTL_ERROR_RESET				_IO(VDI_IOCTL_MAGIC, 19)
#define VDI_IOCTL_ERROR_NUM				_IO(VDI_IOCTL_MAGIC, 20)
#define VDI_IOCTL_ERROR_STATE				_IO(VDI_IOCTL_MAGIC, 21)

#define VDI_IOCTL_IPU_RW_REG				_IO(VDI_IOCTL_MAGIC, 22)
#define VDI_IOCTL_SET_MODE				_IO(VDI_IOCTL_MAGIC, 23)
#define VDI_IOCTL_GET_TIMESTAMP				_IO(VDI_IOCTL_MAGIC, 24)

typedef struct vpudrv_ipu_reg {
	unsigned int rw;        /* 0 r, 1 w */
	unsigned int addr;
	unsigned int value;
} vpudrv_ipu_reg_t;

typedef struct vpudrv_buffer_t {
	char name[32];
	unsigned int size;
	unsigned long phys_addr;
	unsigned long base;							/* kernel logical address in use kernel */
	unsigned long virt_addr;				/* virtual user space address */
	unsigned int buf_io;
	unsigned int dir;
} vpudrv_buffer_t;

typedef struct rts_vpudrv_timestamp {
	unsigned long long time;
	char inst_idx;
} rts_vpudrv_timestamp;

typedef struct vpu_bit_firmware_info_t {
	unsigned int size;						/* size of this structure*/
	unsigned int core_idx;
	unsigned long reg_base_offset;
	unsigned short bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_inst_info_t {
	unsigned int core_idx;
	unsigned int inst_idx;
	int inst_open_count;	/* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
	unsigned int timeout;
	int			intr_reason;
#ifdef SUPPORT_MULTI_INST_INTR
	int			intr_inst_index;
#endif
} vpudrv_intr_info_t;
#endif

