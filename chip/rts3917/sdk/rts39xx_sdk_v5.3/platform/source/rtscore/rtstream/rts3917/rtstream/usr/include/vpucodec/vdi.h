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

#ifndef _VDI_H_
#define _VDI_H_

#include "mm.h"
#include "vpuconfig.h"
#include "vputypes.h"

/************************************************************************/
/* COMMON REGISTERS                                                     */
/************************************************************************/
#define VPU_PRODUCT_NAME_REGISTER                 0x1040
#define VPU_PRODUCT_CODE_REGISTER                 0x1044

#define SUPPORT_MULTI_CORE_IN_ONE_DRIVER
#define MAX_VPU_CORE_NUM MAX_NUM_VPU_CORE

#define MAX_VPU_BUFFER_POOL 1000

#define VpuWriteReg( CORE, ADDR, DATA )                 vdi_write_register( CORE, ADDR, DATA )					// system register write
#define VpuReadReg( CORE, ADDR )                        vdi_read_register( CORE, ADDR )							// system register read
#define VpuWriteMem( CORE, ADDR, DATA, LEN, ENDIAN )    vdi_write_memory( CORE, ADDR, DATA, LEN, ENDIAN )		// system memory write
#define VpuReadMem( CORE, ADDR, DATA, LEN, ENDIAN )     vdi_read_memory( CORE, ADDR, DATA, LEN, ENDIAN )		// system memory read

typedef struct vpu_buffer_t {
    char name[32];
    Uint32 size;
    unsigned long phys_addr;
    unsigned long   base;
    unsigned long   virt_addr;
    unsigned int buf_io;
} vpu_buffer_t;

typedef enum {
    VDI_LITTLE_ENDIAN = 0,      /* 64bit LE */
    VDI_BIG_ENDIAN,             /* 64bit BE */
    VDI_32BIT_LITTLE_ENDIAN,
    VDI_32BIT_BIG_ENDIAN,
    /* WAVE PRODUCTS */
    VDI_128BIT_LITTLE_ENDIAN    = 16,
    VDI_128BIT_LE_BYTE_SWAP,
    VDI_128BIT_LE_WORD_SWAP,
    VDI_128BIT_LE_WORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_SWAP,
    VDI_128BIT_LE_DWORD_BYTE_SWAP,
    VDI_128BIT_LE_DWORD_WORD_SWAP,
    VDI_128BIT_LE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_WORD_SWAP,
    VDI_128BIT_BE_DWORD_BYTE_SWAP,
    VDI_128BIT_BE_DWORD_SWAP,
    VDI_128BIT_BE_WORD_BYTE_SWAP,
    VDI_128BIT_BE_WORD_SWAP,
    VDI_128BIT_BE_BYTE_SWAP,
    VDI_128BIT_BIG_ENDIAN        = 31,
    VDI_ENDIAN_MAX
} EndianMode;

#define VDI_128BIT_ENDIAN_MASK 0xf
typedef enum {
    DEC_TASK, DEC_WORK, DEC_FBC, DEC_FBCY_TBL, DEC_FBCC_TBL, DEC_BS, DEC_FB_LINEAR, DEC_MV, DEC_ETC,
    ENC_TASK, ENC_WORK, ENC_FBC, ENC_FBCY_TBL, ENC_FBCC_TBL, ENC_BS, ENC_SRC, ENC_MV, ENC_SUBSAMBUF, ENC_ETC
} MemTypes;

#ifdef SUPPORT_MULTI_INST_INTR_IN_API
typedef struct vpu_pending_intr_t {
    int int_reason[COMMAND_QUEUE_DEPTH];
    int count;
} vpu_pending_intr_t;
#endif
typedef struct vpu_instance_pool_t {
    unsigned char   codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];  // Since VDI don't know the size of CodecInst structure, VDI should have the enough space not to overflow.
    vpu_buffer_t    vpu_common_buffer;
    int             vpu_instance_num;
    int             instance_pool_inited;
    void*           pendingInst;
    int             pendingInstIdxPlus1;
    Uint32          lastPerformanceCycles;
    video_mm_t      vmem;
#ifdef SUPPORT_MULTI_INST_INTR_IN_API
    vpu_pending_intr_t pending_intr_list[MAX_NUM_INSTANCE];
#endif
} vpu_instance_pool_t;


#if defined (__cplusplus)
extern "C" {
#endif
    int vdi_probe(unsigned long core_idx);
    int vdi_init(unsigned long core_idx);
    int vdi_release(unsigned long core_idx);	//this function may be called only at system off.

    vpu_instance_pool_t * vdi_get_instance_pool(unsigned long core_idx);
    int vdi_allocate_common_memory(unsigned long core_idx);
    int vdi_get_common_memory(unsigned long core_idx, vpu_buffer_t *vb);
	Uint32 vdi_get_tempbuf_size(unsigned long core_idx);
    int vdi_allocate_dma_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex);
    int vdi_attach_dma_memory(unsigned long core_idx, vpu_buffer_t *vb);
    void vdi_free_dma_memory(unsigned long core_idx, vpu_buffer_t *vb, int memTypes, int instIndex);
    int vdi_get_sram_memory(unsigned long core_idx, vpu_buffer_t *vb);
    int vdi_dettach_dma_memory(unsigned long core_idx, vpu_buffer_t *vb);

#ifdef SUPPORT_MULTI_INST_INTR
    int vdi_wait_interrupt(unsigned long coreIdx, unsigned int instIdx, int timeout);
#else
    int vdi_wait_interrupt(unsigned long core_idx, int timeout);
#endif

    int vdi_wait_vpu_busy(unsigned long core_idx, int timeout, unsigned int addr_bit_busy_flag);
    int vdi_wait_vcpu_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag);
    int vdi_wait_bus_busy(unsigned long core_idx, int timeout, unsigned int gdi_busy_flag);
    int vdi_hw_reset(unsigned long core_idx);

    int vdi_set_clock_gate(unsigned long core_idx, int enable);
    int vdi_get_clock_gate(unsigned long core_idx);
    /**
     * @brief       make clock stable before changing clock frequency
     * @detail      Before inoking vdi_set_clock_freg() caller MUST invoke vdi_ready_change_clock() function.
     *              after changing clock frequency caller also invoke vdi_done_change_clock() function.
     * @return  0   failure
     *          1   success
     */
    int vdi_ready_change_clock(unsigned long core_idx);
    int vdi_set_change_clock(unsigned long core_idx, unsigned long clock_mask);
    int vdi_done_change_clock(unsigned long core_idx);

    /*rts added*/
    int vdi_get_bclk_rate(unsigned long core_idx, Uint32 *rate);

    int  vdi_get_instance_num(unsigned long core_idx);

    void vdi_write_register(unsigned long core_idx, unsigned int addr, unsigned int data);
    unsigned int vdi_read_register(unsigned long core_idx, unsigned int addr);
    void vdi_fio_write_register(unsigned long core_idx, unsigned int addr, unsigned int data);
    unsigned int vdi_fio_read_register(unsigned long core_idx, unsigned int addr);
    int vdi_clear_memory(unsigned long core_idx, PhysicalAddress addr, int len, int endian);
    int vdi_set_memory(unsigned long core_idx, PhysicalAddress addr, int len, int endian, Uint32 data);
    int vdi_write_memory(unsigned long core_idx, PhysicalAddress addr, unsigned char *data, int len, int endian);
    int vdi_read_memory(unsigned long core_idx, PhysicalAddress addr, unsigned char *data, int len, int endian);

	void *vdi_get_ptr(unsigned long core_idx, unsigned int addr);
    int vdi_lock(unsigned long core_idx);
    void vdi_unlock(unsigned long core_idx);
    int vdi_disp_lock(unsigned long core_idx);
    void vdi_disp_unlock(unsigned long core_idx);
    int vdi_open_instance(unsigned long core_idx, unsigned long inst_idx);
    int vdi_close_instance(unsigned long core_idx, unsigned long inst_idx);
    int vdi_set_bit_firmware_to_pm(unsigned long core_idx, const unsigned short *code);
    int vdi_get_system_endian(unsigned long core_idx);
    int vdi_convert_endian(unsigned long core_idx, unsigned int endian);
    void vdi_get_start_timestamp(unsigned long core_idx, int size);

    /*rts add*/
    int vdi_set_ipu_frame_mode(unsigned long core_idx, int mode);
    int vdi_set_ipu_frame_sync(unsigned long core_idx);
    int vdi_set_ipu_frame_sync_thd(unsigned long core_idx, int thd);
    int vdi_enable_ipu_frame(unsigned long core_idx);
    int vdi_disable_ipu_frame(unsigned long core_idx);
    int vdi_set_ipu_height(unsigned long core_idx, uint32_t height);
    int vdi_set_ipu_sync_method(unsigned long core_idx, unsigned int val);
    int vdi_start_ipu_encode(unsigned long core_idx);
    int vdi_get_ipu_drop_cnt(unsigned long core_idx);
    int vdi_clear_ipu_drop_cnt(unsigned long core_idx);
    int vdi_check_ipu_ringbuffer_status(unsigned long core_idx);
    int vdi_clear_ipu_ringbuffer_status(unsigned long core_idx);
    int vdi_get_ipu_start_line(unsigned long core_idx);
    /*rts add*/
    int vdi_set_low_latency_mode(unsigned long core_idx, int inst_idx);
    unsigned long long vdi_get_timestamp(unsigned long core_idx, int inst_idx);

#if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    int vdi_get_task_num(unsigned long core_idx);
#endif
#if defined (__cplusplus)
}
#endif

#endif //#ifndef _VDI_H_

