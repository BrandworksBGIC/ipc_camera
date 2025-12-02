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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/reset.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>

#include "linux/wave521mp/vpuconfig.h"
#include "linux/wave521mp/vpu.h"
#include "rts_camera.h"

//#define ENABLE_DEBUG_MSG
#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...)		printk(KERN_INFO args);
#else
#define DPRINTK(args...)
#endif

#ifndef CONFIG_PM
#define CONFIG_PM
#endif

#define RTS_IPU_REG_DROP_CNT	0x24
#define RTS_IPU_REG_IMAGE_ERR	0x28
#define RTS_IPU_REG_START_LINE	0x2c

#define RTS_VPU_LOW_LATENCY_STREAM_IDX 		0
#define RTS_VPU_LOW_LATENCY_BUF_ERROR_FLAG	0x10

#define RTS_VPU_SUPPORT_MAX_WIDTH	3840
#define RTS_VPU_SUPPORT_MAX_HEIGHT	2160
#define RTS_VPU_MAX_COMMON_SIZE		0x10E000

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* #define VPU_SUPPORT_CLOCK_CONTROL */

/* if the driver want to use interrupt service from kernel ISR */
#ifdef SUPPORT_INTERRUPT
#define VPU_SUPPORT_ISR
#else
#endif

#ifdef VPU_SUPPORT_ISR
/* if the driver want to disable and enable IRQ whenever interrupt asserted. */
//#define VPU_IRQ_CONTROL
#endif

/* if the platform driver knows the name of this driver */
/* VPU_PLATFORM_DEVICE_NAME */
#define VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
#include <linux/of.h>
#endif

/* if this driver knows the dedicated video memory address */
/* #define VPU_SUPPORT_RESERVED_VIDEO_MEMORY */
#define VPU_SUPPORT_RESERVED_RTS_MEMORY

#define VPU_PLATFORM_DEVICE_NAME "vdec"
/* #define VPU_CLK_NAME "vcodec" */
#define VPU_CLK_NAME "h265_ck"
#define VPU_ACLK_NAME "h265_aclk_ck"
#define VPU_BCLK_NAME "h265_bclk_ck"
#define VPU_CCLK_NAME "h265_cclk_ck"
#define VPU_DEV_NAME "vpu"

/* if the platform driver knows this driver */
/* the definition of VPU_REG_BASE_ADDR and VPU_REG_SIZE are not meaningful */

#define VPU_REG_BASE_ADDR 0x75000000
#define VPU_REG_SIZE (0x4000*MAX_NUM_VPU_CORE)

#ifdef VPU_SUPPORT_ISR
#define VPU_IRQ_NUM (23+32)
#endif

/* this definition is only for chipsnmedia FPGA board env */
/* so for SOC env of customers can be ignored */

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

typedef struct vpu_drv_context_t {
	struct fasync_struct *async_queue;
#ifdef SUPPORT_MULTI_INST_INTR
	unsigned long interrupt_reason[MAX_NUM_INSTANCE];
#else
	unsigned long interrupt_reason;
#endif
	u32 open_count;					 /*!<< device reference count. Not instance count */
#ifdef DBG_ENCODE_TIME
	struct timespec64 start;
	struct timespec64 end;
	unsigned long cnt;
	int wid;
	struct ring_buffer *timebuf;
#endif
} vpu_drv_context_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
	struct list_head list;
	struct vpudrv_buffer_t vb;
	struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
	struct list_head list;
	unsigned long inst_idx;
	unsigned long core_idx;

	unsigned long long timestamp;
	struct file *filp;
} vpudrv_instanace_list_t;

/* To reserve streaminfo when vpu instance closed*/
typedef struct vpudrv_streaminfo_t {
	int lowlatency_mode[MAX_NUM_INSTANCE];
	unsigned long frame_count;
	unsigned int drop_count;
	unsigned long error_count;
	unsigned long overflow_count;
} vpudrv_streaminfo_t;

typedef struct vpudrv_instance_pool_t {
	unsigned char codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
} vpudrv_instance_pool_t;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#	define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE (62*1024*1024)
#	define VPU_DRAM_PHYSICAL_BASE 0x86C00000
#include "vmm.h"
static video_mm_t s_vmem;
static vpudrv_buffer_t s_video_memory = {0};
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
#include "rts_isp_mem.h"
static struct rtscam_mem_info *rtsmem;
#endif /*VPU_SUPPORT_RESERVED_VIDEO_MEMORY*/

static int vpu_hw_reset(void);
static int vpu_hw_assert(void);
static void vpu_clk_disable(struct clk *clk);
static int vpu_clk_enable(struct clk *clk);
static struct clk *vpu_clk_get(struct device *dev, const char *clk_name);
static void vpu_clk_put(struct clk *clk);

/* end customer definition */
static vpudrv_buffer_t s_instance_pool = {0};
static vpudrv_buffer_t s_common_memory = {0};
static vpu_drv_context_t s_vpu_drv_context;
static vpudrv_streaminfo_t s_streaminfo;

#define VPU_SUPPORT_RTS_DEV_REGISTER
#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
struct rtscam_ge_device *s_vpu_gdev;
#else
static int s_vpu_major;
static struct cdev s_vpu_cdev;
static struct device s_vpu_dev;
#endif

static struct clk *s_vpu_clk;
static atomic_t s_vpu_clk_cnt;
static u32 s_bclk_rate;

static int s_vpu_open_ref_count;
#ifdef VPU_SUPPORT_ISR
static int s_vpu_irq = VPU_IRQ_NUM;
#endif

static vpudrv_buffer_t s_vpu_register = {0};
static vpudrv_buffer_t s_vpu_register2 = {0};
static struct {
	struct reset_control *axi;
	struct reset_control *bpu;
	struct reset_control *core;
	struct reset_control *sysmem;
} s_vpu_rst;

#ifdef SUPPORT_MULTI_INST_INTR
static int s_interrupt_flag[MAX_NUM_INSTANCE];
static wait_queue_head_t s_interrupt_wait_q[MAX_NUM_INSTANCE];
typedef struct kfifo kfifo_t;
static kfifo_t s_interrupt_pending_q[MAX_NUM_INSTANCE];
static spinlock_t s_kfifo_lock = __SPIN_LOCK_UNLOCKED(s_kfifo_lock);
#else
static int s_interrupt_flag;
static wait_queue_head_t s_interrupt_wait_q;
#endif

static spinlock_t s_vpu_lock = __SPIN_LOCK_UNLOCKED(s_vpu_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static DECLARE_MUTEX(s_vpu_sem);
#else
static DEFINE_SEMAPHORE(s_vpu_sem, 1);
#endif
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);

static vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

#ifdef CONFIG_PM
/* implement to power management functions */
#define BIT_BASE			0x0000
#define BIT_CODE_RUN				(BIT_BASE + 0x000)
#define BIT_CODE_DOWN			   (BIT_BASE + 0x004)
#define BIT_INT_CLEAR			   (BIT_BASE + 0x00C)
#define BIT_INT_STS				 (BIT_BASE + 0x010)
#define BIT_CODE_RESET			(BIT_BASE + 0x014)
#define BIT_INT_REASON			  (BIT_BASE + 0x174)
#define BIT_BUSY_FLAG			   (BIT_BASE + 0x160)
#define BIT_RUN_COMMAND			 (BIT_BASE + 0x164)
#define BIT_RUN_INDEX			   (BIT_BASE + 0x168)
#define BIT_RUN_COD_STD			 (BIT_BASE + 0x16C)

/* WAVE5 registers */
#define W5_REG_BASE					 0x0000
#define W5_VPU_BUSY_STATUS          (W5_REG_BASE + 0x0070)
#define W5_VPU_INT_REASON_CLEAR     (W5_REG_BASE + 0x0034)
#define W5_VPU_VINT_CLEAR           (W5_REG_BASE + 0x003C)
#define W5_VPU_VPU_INT_STS          (W5_REG_BASE + 0x0044)
#define W5_VPU_INT_REASON           (W5_REG_BASE + 0x004c)
#define W5_RET_FAIL_REASON          (W5_REG_BASE + 0x010C)

#ifdef SUPPORT_MULTI_INST_INTR
#define W5_RET_BS_EMPTY_INST            (W5_REG_BASE + 0x01E4)
#define W5_RET_QUEUE_CMD_DONE_INST      (W5_REG_BASE + 0x01E8)
#define W5_RET_SEQ_DONE_INSTANCE_INFO   (W5_REG_BASE + 0x01FC)
#endif
typedef enum {
	INT_WAVE5_INIT_VPU          = 0,
	INT_WAVE5_WAKEUP_VPU        = 1,
	INT_WAVE5_SLEEP_VPU         = 2,
	INT_WAVE5_CREATE_INSTANCE   = 3,
	INT_WAVE5_FLUSH_INSTANCE    = 4,
	INT_WAVE5_DESTORY_INSTANCE  = 5,
	INT_WAVE5_INIT_SEQ          = 6,
	INT_WAVE5_SET_FRAMEBUF      = 7,
	INT_WAVE5_DEC_PIC           = 8,
	INT_WAVE5_ENC_PIC           = 8,
	INT_WAVE5_ENC_SET_PARAM     = 9,
#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	INT_WAVE5_ENC_SRC_RELEASE   = 10,
#endif
	INT_WAVE5_ENC_LOW_LATENCY   = 13,
	INT_WAVE5_DEC_QUERY         = 14,
	INT_WAVE5_BSBUF_EMPTY       = 15,
	INT_WAVE5_BSBUF_FULL        = 15,
	RTS_INT_WAVE5_BUF_STATUS    = 16,
} Wave5InterruptBit;

/* WAVE5 INIT, WAKEUP */
#define W5_PO_CONF                  (W5_REG_BASE + 0x0000)
#define W5_VPU_VINT_ENABLE          (W5_REG_BASE + 0x0048)
#define W5_VPU_RESET_REQ            (W5_REG_BASE + 0x0050)
#define W5_VPU_RESET_STATUS         (W5_REG_BASE + 0x0054)
#define W5_VPU_REMAP_CTRL           (W5_REG_BASE + 0x0060)
#define W5_VPU_REMAP_VADDR          (W5_REG_BASE + 0x0064)
#define W5_VPU_REMAP_PADDR          (W5_REG_BASE + 0x0068)
#define W5_VPU_REMAP_CORE_START     (W5_REG_BASE + 0x006C)
#define W5_RST_BLOCK_ALL            (0x3fffffff)
#define W5_REMAP_CODE_INDEX			0

/* WAVE5 registers */
#define W5_ADDR_CODE_BASE           (W5_REG_BASE + 0x0110)
#define W5_CODE_SIZE				(W5_REG_BASE + 0x0114)
#define W5_CODE_PARAM				(W5_REG_BASE + 0x0118)
#define W5_INIT_VPU_TIME_OUT_CNT	(W5_REG_BASE + 0x0130)
#define W5_HW_OPTION				(W5_REG_BASE + 0x012C)
#define W5_RET_SUCCESS              (W5_REG_BASE + 0x0108)
#define W5_COMMAND			        (W5_REG_BASE + 0x0100)
#define W5_VPU_HOST_INT_REQ	        (W5_REG_BASE + 0x0038)

/* Product register */
#define VPU_PRODUCT_CODE_REGISTER   (BIT_BASE + 0x1044)
#if defined(VPU_SUPPORT_PLATFORM_DRIVER_REGISTER) && defined(CONFIG_PM)
//static u32	s_vpu_reg_store[MAX_NUM_VPU_CORE][64];
#endif
#endif

#define	ReadVpuRegister(addr)		*(volatile unsigned int *)(s_vpu_register.virt_addr + s_bit_firmware_info[core].reg_base_offset + addr)
#define	WriteVpuRegister(addr, val)	*(volatile unsigned int *)(s_vpu_register.virt_addr + s_bit_firmware_info[core].reg_base_offset + addr) = (unsigned int)val
#define	WriteVpu(addr, val)			*(volatile unsigned int *)(addr) = (unsigned int)val
#define	ReadVpu(addr)           *(volatile unsigned int *)(addr)

static int s_vpu_error_num = 0;
static int s_vpu_error_state = 0;
static int s_vpu_error_reset_num = 0;

static int vpu_alloc_dma_buffer(vpudrv_buffer_t *vb)
{
#ifdef VPU_SUPPORT_RESERVED_RTS_MEMORY
	dma_addr_t phy_addr;
#endif

	if (!vb)
		return -1;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vb->phys_addr = (unsigned long)vmem_alloc(&s_vmem, vb->size, 0);
	if ((unsigned long)vb->phys_addr  == (unsigned long)-1) {
		printk(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
		return -1;
	}

	vb->base = (unsigned long)(s_video_memory.base + (vb->phys_addr - s_video_memory.phys_addr));
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
	phy_addr = vb->phys_addr;
	vb->virt_addr = (unsigned long)rtscam_mem_realloc(rtsmem, vb->size,
					&phy_addr, vb->buf_io, vb->name);
	if ((void *)vb->virt_addr == NULL)
		vb->virt_addr = (unsigned long)rtscam_mem_alloc(rtsmem,
					vb->size, &phy_addr,
					vb->buf_io, vb->dir, vb->name);
	if ((void *)vb->virt_addr == NULL) {
		printk(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
		return -ENOMEM;
	}
	vb->phys_addr = phy_addr;
	vb->base = vb->virt_addr;
#else
	vb->base = (unsigned long)dma_alloc_coherent(NULL, PAGE_ALIGN(vb->size), (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);
	if ((void *)(vb->base) == NULL)	{
		printk(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
		return -1;
	}
#endif
	return 0;
}


static void vpu_free_dma_buffer(vpudrv_buffer_t *vb)
{
	if (!vb)
		return;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (vb->base)
		vmem_free(&s_vmem, vb->phys_addr, 0);
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
	if (vb->base)
		rtscam_mem_free_V2(rtsmem, vb->phys_addr);
#else
	if (vb->base)
		dma_free_coherent(0, PAGE_ALIGN(vb->size), (void *)vb->base, vb->phys_addr);
#endif
}

static int vpu_free_instances(struct file *filp)
{
	vpudrv_instanace_list_t *vil, *n;
	vpudrv_instance_pool_t *vip;
	void *vip_base;
	int instance_pool_size_per_core;
	void *vdi_mutexes_base;
	const int PTHREAD_MUTEX_T_DESTROY_VALUE = 0xdead10cc;

	DPRINTK("[VPUDRV] vpu_free_instances\n");

	instance_pool_size_per_core = (s_instance_pool.size/MAX_NUM_VPU_CORE); /* s_instance_pool.size  assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */

	list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
	{
		if (vil->filp == filp) {
			vip_base = (void *)(s_instance_pool.base + (instance_pool_size_per_core*vil->core_idx));
			DPRINTK("[VPUDRV] vpu_free_instances detect instance crash instIdx=%d, coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n", (int)vil->inst_idx, (int)vil->core_idx, vip_base, (int)instance_pool_size_per_core);
			vip = (vpudrv_instance_pool_t *)vip_base;
			if (vip) {
				memset(&vip->codecInstPool[vil->inst_idx], 0x00, 4);	/* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
				vdi_mutexes_base = (vip_base + (instance_pool_size_per_core - PTHREAD_MUTEX_T_HANDLE_SIZE*4));
				DPRINTK("[VPUDRV] vpu_free_instances : force to destroy vdi_mutexes_base=%p in userspace \n", vdi_mutexes_base);
				if (vdi_mutexes_base) {
					int i;
					for (i = 0; i < 4; i++) {
						memcpy(vdi_mutexes_base, &PTHREAD_MUTEX_T_DESTROY_VALUE, PTHREAD_MUTEX_T_HANDLE_SIZE);
						vdi_mutexes_base += PTHREAD_MUTEX_T_HANDLE_SIZE;
					}
				}
			}
			s_vpu_open_ref_count--;
			list_del(&vil->list);
			kfree(vil);
		}
	}
	return 1;
}

static int vpu_free_buffers(struct file *filp)
{
	vpudrv_buffer_pool_t *pool, *n;
	vpudrv_buffer_t vb;

	DPRINTK("[VPUDRV] vpu_free_buffers\n");

	list_for_each_entry_safe(pool, n, &s_vbp_head, list)
	{
		if (pool->filp == filp) {
			vb = pool->vb;
			if (vb.base) {
				vpu_free_dma_buffer(&vb);
				list_del(&pool->list);
				kfree(pool);
			}
		}
	}

	return 0;
}

#ifdef SUPPORT_MULTI_INST_INTR
static inline u32 get_inst_idx(u32 reg_val)
{
	u32 inst_idx;
	int i;
	for (i=0; i < MAX_NUM_INSTANCE; i++)
	{
		if(((reg_val >> i)&0x01) == 1)
			break;
	}
	inst_idx = i;
	return inst_idx;
}

static s32 get_vpu_inst_idx(vpu_drv_context_t *dev, u32 *reason, u32 empty_inst, u32 done_inst, u32 seq_inst)
{
	s32 inst_idx;
	u32 reg_val;
	u32 int_reason;

	int_reason = *reason;
	DPRINTK("[VPUDRV][+]%s, int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n", __func__, int_reason, empty_inst, done_inst);
	//printk(KERN_ERR "[VPUDRV][+]%s, int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n", __func__, int_reason, empty_inst, done_inst);

	if (int_reason & (1 << INT_WAVE5_BSBUF_EMPTY))
	{
		reg_val = (empty_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason = (1 << INT_WAVE5_BSBUF_EMPTY);
		DPRINTK("[VPUDRV]	%s, W5_RET_BS_EMPTY_INST reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_INIT_SEQ))
	{
		reg_val = (seq_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason  = (1 << INT_WAVE5_INIT_SEQ);
		DPRINTK("[VPUDRV]	%s, RET_SEQ_DONE_INSTANCE_INFO INIT_SEQ reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_DEC_PIC))
	{
		reg_val = (done_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason  = (1 << INT_WAVE5_DEC_PIC);
		DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);

		if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY))
		{
			u32 ll_inst_idx;
			reg_val = (done_inst >> 16);
			ll_inst_idx = get_inst_idx(reg_val);
			if (ll_inst_idx == inst_idx)
				*reason = ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY));
			DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC and ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d, ll_inst_idx=%d\n", __func__, reg_val, inst_idx, ll_inst_idx);
		}
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_ENC_SET_PARAM))
	{
		reg_val = (seq_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason  = (1 << INT_WAVE5_ENC_SET_PARAM);
		DPRINTK("[VPUDRV]	%s, RET_SEQ_DONE_INSTANCE_INFO ENC_SET_PARAM reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
	if (int_reason & (1 << INT_WAVE5_ENC_SRC_RELEASE))
	{
		reg_val = (done_inst & 0xffff);
		inst_idx = get_inst_idx(reg_val);
		*reason  = (1 << INT_WAVE5_ENC_SRC_RELEASE);
		DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST ENC_SET_PARAM reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}
#endif

	if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY))
	{
		reg_val = (done_inst >> 16);
		inst_idx = get_inst_idx(reg_val);
		*reason  = (1 << INT_WAVE5_ENC_LOW_LATENCY);
		DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d\n", __func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	inst_idx = -1;
	*reason  = 0;
	DPRINTK("[VPUDRV]	%s, UNKNOWN INTERRUPT REASON: %08x\n", __func__, int_reason);

GET_VPU_INST_IDX_HANDLED:

	DPRINTK("[VPUDRV][-]%s, inst_idx=%d. *reason=0x%x\n", __func__, inst_idx, *reason);

	return inst_idx;
}
#endif

#ifdef VPU_SUPPORT_ISR
static irqreturn_t vpu_irq_handler(int irq, void *dev_id)
{
	vpu_drv_context_t *dev = (vpu_drv_context_t *)dev_id;

	/* this can be removed. it also work in VPU_WaitInterrupt of API function */
	int core;
	int product_code;
#ifdef SUPPORT_MULTI_INST_INTR
	u32 intr_reason;
	u32 intr_reason_final;
	s32 intr_inst_index;
	int ringbuf_overflow;
#endif
	DPRINTK("[VPUDRV][+]%s\n", __func__);

#ifdef VPU_IRQ_CONTROL
	disable_irq_nosync(s_vpu_irq);
#endif

	ringbuf_overflow = rtscam_check_pixel_cnt();

#ifdef SUPPORT_MULTI_INST_INTR
	intr_inst_index = 0;
	intr_reason = 0;
#endif
	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
		if (s_bit_firmware_info[core].size == 0) {/* it means that we didn't get an information the current core from API layer. No core activated.*/
			DPRINTK("[VPUDRV] :  s_bit_firmware_info[core].size is zero\n");
			continue;
		}
		product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);

		if (PRODUCT_CODE_W_SERIES(product_code)) {
			if (ReadVpuRegister(W5_VPU_VPU_INT_STS)) {
#ifdef SUPPORT_MULTI_INST_INTR
				u32 empty_inst;
				u32 done_inst;
				u32 seq_inst;
				u32 i, reason, reason_clr;

				reason     = ReadVpuRegister(W5_VPU_INT_REASON);
				empty_inst = ReadVpuRegister(W5_RET_BS_EMPTY_INST);
				done_inst  = ReadVpuRegister(W5_RET_QUEUE_CMD_DONE_INST);
				seq_inst   = ReadVpuRegister(W5_RET_SEQ_DONE_INSTANCE_INFO);
				reason_clr = reason;

				DPRINTK("[VPUDRV] vpu_irq_handler reason=0x%x, empty_inst=0x%x, done_inst=0x%x, seq_inst=0x%x \n", reason, empty_inst, done_inst, seq_inst);
				for (i=0; i<MAX_NUM_INSTANCE; i++) {
					if (0 == empty_inst && 0 == done_inst && 0 == seq_inst) break;
					intr_reason = reason;
					intr_inst_index = get_vpu_inst_idx(dev, &intr_reason, empty_inst, done_inst, seq_inst);
					DPRINTK("[VPUDRV]     > instance_index: %d, intr_reason: %08x empty_inst: %08x done_inst: %08x seq_inst: %08x\n", intr_inst_index, intr_reason, empty_inst, done_inst, seq_inst);
                    if (intr_inst_index >= 0 && intr_inst_index < MAX_NUM_INSTANCE) {
						if (intr_reason == (1 << INT_WAVE5_BSBUF_EMPTY)) {
							empty_inst = empty_inst & ~(1 << intr_inst_index);
							WriteVpuRegister(W5_RET_BS_EMPTY_INST, empty_inst);
							if (0 == empty_inst) {
								reason &= ~(1<<INT_WAVE5_BSBUF_EMPTY);
							}
							DPRINTK("[VPUDRV]	%s, W5_RET_BS_EMPTY_INST Clear empty_inst=0x%x, intr_inst_index=%d\n", __func__, empty_inst, intr_inst_index);
						}
						if (intr_reason == (1 << INT_WAVE5_DEC_PIC))
						{
							done_inst = done_inst & ~(1 << intr_inst_index);
							WriteVpuRegister(W5_RET_QUEUE_CMD_DONE_INST, done_inst);
							if (0 == done_inst) {
								reason &= ~(1<<INT_WAVE5_DEC_PIC);
							}
							DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST Clear done_inst=0x%x, intr_inst_index=%d\n", __func__, done_inst, intr_inst_index);
						}
						if ((intr_reason == (1 << INT_WAVE5_INIT_SEQ)) || (intr_reason == (1 << INT_WAVE5_ENC_SET_PARAM)))
						{
							seq_inst = seq_inst & ~(1 << intr_inst_index);
							WriteVpuRegister(W5_RET_SEQ_DONE_INSTANCE_INFO, seq_inst);
							if (0 == seq_inst) {
								reason &= ~(1<<INT_WAVE5_INIT_SEQ | 1<<INT_WAVE5_ENC_SET_PARAM);
							}
							DPRINTK("[VPUDRV]	%s, W5_RET_SEQ_DONE_INSTANCE_INFO Clear done_inst=0x%x, intr_inst_index=%d\n", __func__, done_inst, intr_inst_index);
						}
						if ((intr_reason == (1 << INT_WAVE5_ENC_LOW_LATENCY)))
						{
							done_inst = (done_inst >> 16);
							done_inst = done_inst & ~(1 << intr_inst_index);
							done_inst = (done_inst << 16);
							WriteVpuRegister(W5_RET_QUEUE_CMD_DONE_INST, done_inst);
							if (0 == done_inst) {
								reason &= ~(1 << INT_WAVE5_ENC_LOW_LATENCY);
							}
							DPRINTK("[VPUDRV]	%s, W5_RET_QUEUE_CMD_DONE_INST INT_WAVE5_ENC_LOW_LATENCY Clear done_inst=0x%x, intr_inst_index=%d\n", __func__, done_inst, intr_inst_index);
						}
						if (!kfifo_is_full(&s_interrupt_pending_q[intr_inst_index])) {
							vpudrv_instanace_list_t *vil, *n;
							int found = 0;

							if (intr_reason == ((1 << INT_WAVE5_ENC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY)))
								intr_reason_final = (1 << INT_WAVE5_ENC_PIC);
							else
								intr_reason_final = intr_reason;

							spin_lock(&s_vpu_lock);
							list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
								if (vil->inst_idx == intr_inst_index) {
									found = 1;
									break;
								}
							}
							spin_unlock(&s_vpu_lock);

							if (!found) {
								printk(KERN_ERR "vpu irq instance(%d) not found\n", intr_inst_index);
								continue;
							}

							/*check vin buf status, if error occured, notify user space to drop the frame*/
							if (s_streaminfo.lowlatency_mode[intr_inst_index] && (intr_reason_final == (1 << INT_WAVE5_ENC_PIC))) {
								u32 buf_status;

								rtscam_get_video_in_buf_status(RTS_VPU_LOW_LATENCY_STREAM_IDX, &buf_status);
								if (buf_status & RTS_VPU_LOW_LATENCY_BUF_ERROR_FLAG) {
									intr_reason_final |= (1 << RTS_INT_WAVE5_BUF_STATUS);
									rtscam_set_video_in_buf_status(RTS_VPU_LOW_LATENCY_STREAM_IDX, buf_status);
									s_streaminfo.error_count++;
								} else if (ringbuf_overflow) {
									intr_reason_final |= (1 << RTS_INT_WAVE5_BUF_STATUS);
									s_streaminfo.overflow_count++;
								} else {
									s_streaminfo.frame_count++;
								}
							}

							kfifo_in_spinlocked(&s_interrupt_pending_q[intr_inst_index], &intr_reason_final, sizeof(u32), &s_kfifo_lock);
							if (s_streaminfo.lowlatency_mode[intr_inst_index])
								vil->timestamp = ktime_get_raw_ns();
						}
						else {
							printk(KERN_ERR "[VPUDRV] :  kfifo_is_full kfifo_count=%d \n", kfifo_len(&s_interrupt_pending_q[intr_inst_index]));
						}
					}
					else {
						DPRINTK("[VPUDRV] :  intr_inst_index is wrong intr_inst_index=%d \n", intr_inst_index);
					}
				}

				if (0 != reason)
					printk(KERN_ERR "INTERRUPT REASON REMAINED: %08x\n", reason);
				WriteVpuRegister(W5_VPU_INT_REASON_CLEAR, reason_clr);
#else
				dev->interrupt_reason = ReadVpuRegister(W5_VPU_INT_REASON);
				WriteVpuRegister(W5_VPU_INT_REASON_CLEAR, dev->interrupt_reason);
#endif

				WriteVpuRegister(W5_VPU_VINT_CLEAR, 0x1);
			}
		}
		else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
			if (ReadVpuRegister(BIT_INT_STS)) {
#ifdef SUPPORT_MULTI_INST_INTR
				intr_reason = ReadVpuRegister(BIT_INT_REASON);
				intr_inst_index = 0; // in case of coda seriese. treats intr_inst_index is already 0
				kfifo_in_spinlocked(&s_interrupt_pending_q[intr_inst_index], &intr_reason, sizeof(u32), &s_kfifo_lock);
#else
				dev->interrupt_reason = ReadVpuRegister(BIT_INT_REASON);
#endif
				WriteVpuRegister(BIT_INT_CLEAR, 0x1);
			}
		}
		else {
			DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
			continue;
		}
#ifdef SUPPORT_MULTI_INST_INTR
		DPRINTK("[VPUDRV] product: 0x%08x intr_reason: 0x%08x\n\n", product_code, intr_reason);
#else
		DPRINTK("[VPUDRV] product: 0x%08x intr_reason: 0x%08lx\n", product_code, dev->interrupt_reason);
#endif
	}

#ifdef DBG_ENCODE_TIME
	ktime_get_raw_ts64(&dev->end);
#endif

	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);	/* notify the interrupt to user space */

#ifdef SUPPORT_MULTI_INST_INTR
	for (intr_inst_index = 0; intr_inst_index < MAX_NUM_INSTANCE; intr_inst_index++) {
		if (kfifo_len(&s_interrupt_pending_q[intr_inst_index])) {
			s_interrupt_flag[intr_inst_index] = 1;
			wake_up_interruptible(&s_interrupt_wait_q[intr_inst_index]);
		}
	}
#else
	s_interrupt_flag = 1;

	wake_up_interruptible(&s_interrupt_wait_q);
#endif
	DPRINTK("[VPUDRV][-]%s\n", __func__);

	return IRQ_HANDLED;
}
#endif

#ifdef DBG_ENCODE_TIME

static int __init_timebuf(void)
{
	if (rtscam_init_ring_buf(s_vpu_drv_context.timebuf) < 0)
		return -EFAULT;

	if (!s_vpu_drv_context.open_count)
		s_vpu_drv_context.cnt = 0;

	return 0;
}

static void __set_start_encode_time(struct vpu_drv_context_t *dev, int width)
{
	if (dev->start.tv_sec) {
		long delta;

		delta = (dev->end.tv_nsec - dev->start.tv_nsec) / 1000 / 1000 +
				(dev->end.tv_sec - dev->start.tv_sec) * 1000;
		rtscam_write_ring_buf(dev->timebuf, dev->wid,
						delta, dev->cnt++);
	}
	dev->wid = width;
	ktime_get_raw_ts64(&dev->start);
}

static int __alloc_time_buf(void)
{
	s_vpu_drv_context.timebuf = kmalloc(sizeof(*s_vpu_drv_context.timebuf),
						GFP_KERNEL);
	if (!s_vpu_drv_context.timebuf)
		return -EFAULT;

	return 0;
}

static void __put_time_buf(void)
{
	DPRINTK("[VPUDRV] free timebuf memory\n");
	kfree(s_vpu_drv_context.timebuf);
}

#else

static int __init_timebuf(void)
{
	return 0;
}

static void __set_start_encode_time(struct vpu_drv_context_t *dev, int width)
{
}

static int __alloc_time_buf(void)
{
	return 0;
}

static void __put_time_buf(void)
{
}
#endif

static void __reset_streaminfo(void)
{
	WriteVpu(s_vpu_register2.virt_addr + RTS_IPU_REG_DROP_CNT, 1);

	s_streaminfo.frame_count = 0;
	s_streaminfo.drop_count = 0;
	s_streaminfo.overflow_count = 0;
	s_streaminfo.error_count = 0;
}


#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
static int vpu_open(struct file *filp)
#else
static int vpu_open(struct inode *inode, struct file *filp)
#endif
{
	DPRINTK("[VPUDRV][+] %s\n", __func__);
	spin_lock_irq(&s_vpu_lock);

	__init_timebuf();
	s_vpu_drv_context.open_count++;

	filp->private_data = (void *)(&s_vpu_drv_context);
	spin_unlock_irq(&s_vpu_lock);

	DPRINTK("[VPUDRV][-] %s\n", __func__);

	return 0;
}

static void vpu_ioc_reset(void)
{
	if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)s_instance_pool.base);
#else
		vpu_free_dma_buffer(&s_instance_pool);
#endif
		s_instance_pool.base = 0;
	}

	if (s_common_memory.base) {
		vpu_free_dma_buffer(&s_common_memory);
		s_common_memory.base = 0;
	}

	reset_control_assert(s_vpu_rst.sysmem);
	vpu_hw_assert();
	vpu_clk_disable(s_vpu_clk);
	vpu_hw_reset();
	reset_control_deassert(s_vpu_rst.sysmem);
	vpu_clk_enable(s_vpu_clk);
}

/*static int vpu_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg) // for kernel 2.6.9 of C&M*/
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)filp->private_data;

	switch (cmd) {
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
		{
			vpudrv_buffer_pool_t *vbp;

			DPRINTK("[VPUDRV][+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");

			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
				vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
				if (!vbp) {
					up(&s_vpu_sem);
					return -ENOMEM;
				}

				ret = copy_from_user(&(vbp->vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					up(&s_vpu_sem);
					return -EFAULT;
				}

				ret = vpu_alloc_dma_buffer(&(vbp->vb));
				if (ret == -1) {
					ret = -ENOMEM;
					kfree(vbp);
					up(&s_vpu_sem);
					break;
				}
				ret = copy_to_user((void __user *)arg, &(vbp->vb), sizeof(vpudrv_buffer_t));
				if (ret) {
					kfree(vbp);
					ret = -EFAULT;
					up(&s_vpu_sem);
					break;
				}

				vbp->filp = filp;
				spin_lock_irq(&s_vpu_lock);
				list_add(&vbp->list, &s_vbp_head);
				spin_unlock_irq(&s_vpu_lock);

				up(&s_vpu_sem);
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
		}
		break;
	case VDI_IOCTL_FREE_PHYSICALMEMORY:
		{
			vpudrv_buffer_pool_t *vbp, *n;
			vpudrv_buffer_t vb;
			DPRINTK("[VPUDRV][+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {

				ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret) {
					up(&s_vpu_sem);
					return -EACCES;
				}

				if (vb.base)
					vpu_free_dma_buffer(&vb);

				spin_lock_irq(&s_vpu_lock);
				list_for_each_entry_safe(vbp, n, &s_vbp_head, list)
				{
					if (vbp->vb.base == vb.base) {
						list_del(&vbp->list);
						kfree(vbp);
						break;
					}
				}
				spin_unlock_irq(&s_vpu_lock);

				up(&s_vpu_sem);
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");

		}
		break;
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
		{
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
			if (s_video_memory.base != 0) {
				ret = copy_to_user((void __user *)arg, &s_video_memory, sizeof(vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
#endif
		}
		break;
	case VDI_IOCTL_SET_MODE:
		{
			int inst_idx;

			ret = copy_from_user(&inst_idx, (int *)arg, sizeof(int));
			if (ret != 0)
				return -EFAULT;


			s_streaminfo.lowlatency_mode[inst_idx] = 1;
			__reset_streaminfo();
		}
		break;
	case VDI_IOCTL_GET_TIMESTAMP:
		{
			rts_vpudrv_timestamp ts;
			vpudrv_instanace_list_t *vil, *n;

			ret = copy_from_user(&ts, (rts_vpudrv_timestamp *)arg,
					sizeof(rts_vpudrv_timestamp));
			if (ret != 0)
				return -EFAULT;

			spin_lock_irq(&s_vpu_lock);
			list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
				if (vil->inst_idx == ts.inst_idx) {
					ts.time = vil->timestamp;
					break;
				}
			}
			spin_unlock_irq(&s_vpu_lock);

			ret = copy_to_user((void __user *)arg, &ts, sizeof(rts_vpudrv_timestamp));
		}
		break;
	case VDI_IOCTL_WAIT_INTERRUPT:
		{
			vpudrv_intr_info_t info;
#ifdef SUPPORT_MULTI_INST_INTR
			u32 intr_inst_index;
			u32 intr_reason_in_q;
			u32 interrupt_flag_in_q;
#endif
			DPRINTK("[VPUDRV][+]VDI_IOCTL_WAIT_INTERRUPT\n");

			ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg, sizeof(vpudrv_intr_info_t));
			if (ret != 0)
			{
				return -EFAULT;
			}
#ifdef SUPPORT_MULTI_INST_INTR
			intr_inst_index = info.intr_inst_index;

			intr_reason_in_q = 0;
			interrupt_flag_in_q = kfifo_out_spinlocked(&s_interrupt_pending_q[intr_inst_index], &intr_reason_in_q, sizeof(u32), &s_kfifo_lock);
			if (interrupt_flag_in_q > 0)
			{
				dev->interrupt_reason[intr_inst_index] = intr_reason_in_q;
				DPRINTK("[VPUDRV] Interrupt Remain : intr_inst_index=%d, intr_reason_in_q=0x%x, interrupt_flag_in_q=%d\n", intr_inst_index, intr_reason_in_q, interrupt_flag_in_q);
				goto INTERRUPT_REMAIN_IN_QUEUE;
			}
#endif
#ifdef SUPPORT_MULTI_INST_INTR
#ifdef SUPPORT_TIMEOUT_RESOLUTION
			kt =  ktime_set(0, info.timeout*1000*1000);
			ret = wait_event_interruptible_hrtimeout(s_interrupt_wait_q[intr_inst_index], s_interrupt_flag[intr_inst_index] != 0, kt);
#else
			ret = wait_event_interruptible_timeout(s_interrupt_wait_q[intr_inst_index], s_interrupt_flag[intr_inst_index] != 0, msecs_to_jiffies(info.timeout));
#endif
#else
			ret = wait_event_interruptible_timeout(s_interrupt_wait_q, s_interrupt_flag != 0, msecs_to_jiffies(info.timeout));
#endif
#ifdef SUPPORT_TIMEOUT_RESOLUTION
			if (ret == -ETIME) {
				//DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d \n", info.timeout);
				break;
			}
#endif
			if (!ret) {
				ret = -ETIME;
				break;
			}

			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}

#ifdef SUPPORT_MULTI_INST_INTR
			intr_reason_in_q = 0;
			interrupt_flag_in_q = kfifo_out_spinlocked(&s_interrupt_pending_q[intr_inst_index], &intr_reason_in_q, sizeof(u32), &s_kfifo_lock);
			if (interrupt_flag_in_q > 0) {
				dev->interrupt_reason[intr_inst_index] = intr_reason_in_q;
			}
			else {
				dev->interrupt_reason[intr_inst_index] = 0;
			}
#endif
#ifdef SUPPORT_MULTI_INST_INTR
			DPRINTK("[VPUDRV] inst_index(%d), s_interrupt_flag(%d), reason(0x%08lx)\n", intr_inst_index, s_interrupt_flag[intr_inst_index], dev->interrupt_reason[intr_inst_index]);
#else
			DPRINTK("[VPUDRV]	s_interrupt_flag(%d), reason(0x%08lx)\n", s_interrupt_flag, dev->interrupt_reason);
#endif

#ifdef SUPPORT_MULTI_INST_INTR
INTERRUPT_REMAIN_IN_QUEUE:
			info.intr_reason = dev->interrupt_reason[intr_inst_index];
			s_interrupt_flag[intr_inst_index] = 0;
			dev->interrupt_reason[intr_inst_index] = 0;
#else
			info.intr_reason = dev->interrupt_reason;
			s_interrupt_flag = 0;
			dev->interrupt_reason = 0;
#endif
#ifdef VPU_IRQ_CONTROL
			enable_irq(s_vpu_irq);
#endif
			ret = copy_to_user((void __user *)arg, &info, sizeof(vpudrv_intr_info_t));
			DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT\n");
			if (ret != 0)
			{
				return -EFAULT;
			}
		}
		break;
	case VDI_IOCTL_SET_CLOCK_GATE:
		{
			u32 clkgate;

			DPRINTK("[VPUDRV][+]VDI_IOCTL_SET_CLOCK_GATE\n");
			if (get_user(clkgate, (u32 __user *) arg))
				return -EFAULT;
#ifdef VPU_SUPPORT_CLOCK_CONTROL
			if (clkgate)
				vpu_clk_enable(s_vpu_clk);
			else
				vpu_clk_disable(s_vpu_clk);
#endif
			DPRINTK("[VPUDRV][-]VDI_IOCTL_SET_CLOCK_GATE\n");

		}
		break;
	case VDI_IOCTL_GET_INSTANCE_POOL:
		{
			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_POOL\n");
			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
				if (s_instance_pool.base != 0) {
					ret = copy_to_user((void __user *)arg, &s_instance_pool, sizeof(vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else {
					ret = copy_from_user(&s_instance_pool, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret == 0) {
	#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
					s_instance_pool.size = PAGE_ALIGN(s_instance_pool.size);
					s_instance_pool.base = (unsigned long)vmalloc(s_instance_pool.size);
					s_instance_pool.phys_addr = s_instance_pool.base;

					if (s_instance_pool.base != 0)
	#else
						if (vpu_alloc_dma_buffer(&s_instance_pool) != -1)
	#endif
						{
							memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size); /*clearing memory*/
							ret = copy_to_user((void __user *)arg, &s_instance_pool, sizeof(vpudrv_buffer_t));
							if (ret == 0) {
								/* success to get memory for instance pool */
								up(&s_vpu_sem);
								break;
							}
						}
					}
					ret = -EFAULT;
				}
				up(&s_vpu_sem);
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_INSTANCE_POOL\n");
		}
		break;
	case VDI_IOCTL_GET_COMMON_MEMORY:
		{
			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_COMMON_MEMORY\n");
			if (s_common_memory.base != 0) {
				ret = copy_to_user((void __user *)arg, &s_common_memory, sizeof(vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;
			} else {
				ret = copy_from_user(&s_common_memory, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (s_common_memory.size == 0)
					s_common_memory.size = RTS_VPU_MAX_COMMON_SIZE;
				strncpy(s_common_memory.name, "H26x Common",
					sizeof(s_common_memory.name));
				s_common_memory.dir = RTSMEM_ALLOC_END;
				s_common_memory.buf_io = 0;
				s_common_memory.phys_addr = 0x0;
				if (vpu_alloc_dma_buffer(&s_common_memory) != -1) {
					rtscam_mem_add_property(rtsmem, s_common_memory.phys_addr, RTSMEM_PROBE_ALLOCATED);
					ret = copy_to_user((void __user *)arg, &s_common_memory, sizeof(vpudrv_buffer_t));
					if (ret == 0) {
						/* success to get memory for common memory */
						break;
					}
				}

				ret = -EFAULT;
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_COMMON_MEMORY\n");
		}
		break;
	case VDI_IOCTL_OPEN_INSTANCE:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;

			vil = kzalloc(sizeof(*vil), GFP_KERNEL);
			if (!vil)
				return -ENOMEM;
            if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t))) {
                kfree(vil);
                return -EFAULT;
            }

			vil->inst_idx = inst_info.inst_idx;
			vil->core_idx = inst_info.core_idx;
			vil->filp = filp;
			s_streaminfo.lowlatency_mode[inst_info.inst_idx] = 0;

			spin_lock_irq(&s_vpu_lock);
			list_add(&vil->list, &s_inst_list_head);

            inst_info.inst_open_count = 0; /* counting the current open instance number */
			list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
			{
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
#ifdef SUPPORT_MULTI_INST_INTR
			kfifo_reset(&s_interrupt_pending_q[inst_info.inst_idx]);
#endif
			spin_unlock_irq(&s_vpu_lock);

			s_vpu_open_ref_count++; /* flag just for that vpu is in opened or closed */

			if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t))) {
                kfree(vil);
                return -EFAULT;
            }

			DPRINTK("[VPUDRV] VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n", (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_open_ref_count, inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_CLOSE_INSTANCE:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;
			u32 found = 0;

			DPRINTK("[VPUDRV][+]VDI_IOCTL_CLOSE_INSTANCE\n");
			if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t)))
				return -EFAULT;

			spin_lock_irq(&s_vpu_lock);
			list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
			{
				if (vil->inst_idx == inst_info.inst_idx && vil->core_idx == inst_info.core_idx) {
					list_del(&vil->list);
					kfree(vil);
					found = 1;
					break;
				}
			}

			if (0 == found) {
				spin_unlock_irq(&s_vpu_lock);
				return -EINVAL;
			}

			inst_info.inst_open_count = 0; /* counting the current open instance number */
			list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
			{
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
#ifdef SUPPORT_MULTI_INST_INTR
			kfifo_reset(&s_interrupt_pending_q[inst_info.inst_idx]);
#endif
			spin_unlock_irq(&s_vpu_lock);

			s_vpu_open_ref_count--; /* flag just for that vpu is in opened or closed */

			if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t)))
				return -EFAULT;

			DPRINTK("[VPUDRV] VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n", (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_open_ref_count, inst_info.inst_open_count);
		}
		break;
	case VDI_IOCTL_GET_INSTANCE_NUM:
		{
			vpudrv_inst_info_t inst_info;
			vpudrv_instanace_list_t *vil, *n;
			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_NUM\n");

			ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t));
			if (ret != 0)
				break;

			spin_lock_irq(&s_vpu_lock);
			inst_info.inst_open_count = 0;
			list_for_each_entry_safe(vil, n, &s_inst_list_head, list)
			{
				if (vil->core_idx == inst_info.core_idx)
					inst_info.inst_open_count++;
			}
			spin_unlock_irq(&s_vpu_lock);

			ret = copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t));

			DPRINTK("[VPUDRV] VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n", (int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);

		}
		break;
	case VDI_IOCTL_RESET:
		{
			vpu_hw_reset();
		}
		break;
	case VDI_IOCTL_GET_REGISTER_INFO:
		{
			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_REGISTER_INFO\n");
			ret = copy_to_user((void __user *)arg, &s_vpu_register, sizeof(vpudrv_buffer_t));
			if (ret != 0)
				ret = -EFAULT;
			DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_REGISTER_INFO s_vpu_register.phys_addr==0x%lx, s_vpu_register.virt_addr=0x%lx, s_vpu_register.size=%d\n", s_vpu_register.phys_addr , s_vpu_register.virt_addr, s_vpu_register.size);
		}
		break;
	case VDI_IOCTL_SET_START_TIMESTAMP:
		{
			__set_start_encode_time(dev, (int)arg);
		}
		break;
	case VDI_IOCTL_GENCODE_TIMEBUF:
		{
#ifdef DBG_ENCODE_TIME
			ret = rtscam_read_ring_buf(dev->timebuf,
							(void __user *)arg);
#else
			ret = -EFAULT;
#endif
		}
		break;
	case VDI_IOCTL_IPU_RW_REG:
		{
			vpudrv_ipu_reg_t reg;

			ret = copy_from_user(&reg, (vpudrv_ipu_reg_t *)arg, sizeof(vpudrv_ipu_reg_t));
			if (ret != 0)
				break;

			if (reg.rw == 0) {
				reg.value = ReadVpu(s_vpu_register2.virt_addr
						+ reg.addr);
			} else if (reg.rw == 1) {
				WriteVpu(s_vpu_register2.virt_addr + reg.addr,
						reg.value);
			} else {
				ret = -EFAULT;
				break;
			}

			ret = copy_to_user((void __user *)arg, &reg, sizeof(vpudrv_ipu_reg_t));
		}
		break;
	case VDI_IOCTL_LOCK:
		{
			ret = down_interruptible(&s_vpu_sem);
		}
		break;
	case VDI_IOCTL_UNLOCK:
		{
			up(&s_vpu_sem);
		}
		break;
	case VDI_IOCTL_QUERY_BCLK:
		{
			ret = copy_to_user((void __user *)arg,
					&s_bclk_rate, sizeof(u32));
		}
		break;
	case VDI_IOCTL_ERROR_RESET:
		{
			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
				s_vpu_error_state = 1;
				up(&s_vpu_sem);
			}
		}
		break;
	case VDI_IOCTL_ERROR_NUM:
		{
			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
				int add;
				ret = copy_from_user(&add, (int *)arg, sizeof(int));
				if (ret != 0) {
					up(&s_vpu_sem);
					break;
				}

				if (add) {
					s_vpu_error_num++;
				} else {
					if (s_vpu_error_num > 0) {
						s_vpu_error_num--;
						if ((s_vpu_error_num == 0) && (s_vpu_error_state == 1)) {
							vpu_ioc_reset();
							s_vpu_error_state = 0;
							s_vpu_error_reset_num++;
						}
					}
				}
				up(&s_vpu_sem);
			}
		}
		break;
	case VDI_IOCTL_ERROR_STATE:
		{
			if ((ret = down_interruptible(&s_vpu_sem)) == 0) {
				ret = copy_to_user((void __user *)arg, &s_vpu_error_state, sizeof(int));
				up(&s_vpu_sem);
			}
		}
		break;
	default:
		{
			printk(KERN_ERR "[VPUDRV] No such IOCTL, cmd is %d\n", cmd);
		}
		break;
	}
	return ret;
}

static ssize_t vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{

	return -1;
}

static ssize_t vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{

	/* DPRINTK("[VPUDRV] vpu_write len=%d\n", (int)len); */
	if (!buf) {
		printk(KERN_ERR "[VPUDRV] vpu_write buf = NULL error \n");
		return -EFAULT;
	}

	if (len == sizeof(vpu_bit_firmware_info_t))	{
		vpu_bit_firmware_info_t *bit_firmware_info;

		bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t), GFP_KERNEL);
		if (!bit_firmware_info) {
			printk(KERN_ERR "[VPUDRV] vpu_write  bit_firmware_info allocation error \n");
			return -EFAULT;
		}

		if (copy_from_user(bit_firmware_info, buf, len)) {
			printk(KERN_ERR "[VPUDRV] vpu_write copy_from_user error for bit_firmware_info\n");
            kfree(bit_firmware_info);
			return -EFAULT;
		}

		if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
			DPRINTK("[VPUDRV] vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
			bit_firmware_info->core_idx, (int)bit_firmware_info->reg_base_offset, bit_firmware_info->size, bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx > MAX_NUM_VPU_CORE) {
				printk(KERN_ERR "[VPUDRV] vpu_write coreIdx[%d] is exceeded than MAX_NUM_VPU_CORE[%d]\n", bit_firmware_info->core_idx, MAX_NUM_VPU_CORE);
				return -ENODEV;
			}

			memcpy(&s_bit_firmware_info[bit_firmware_info->core_idx], bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
			kfree(bit_firmware_info);

			return len;
		}

		kfree(bit_firmware_info);
	}

	return -1;
}

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
static int vpu_release(struct file *filp)
#else
static int vpu_release(struct inode *inode, struct file *filp)
#endif
{
	int ret = 0;
	u32 open_count;
#ifdef SUPPORT_MULTI_INST_INTR
	int i;
#endif
	DPRINTK("[VPUDRV] vpu_release\n");

	if ((ret = down_interruptible(&s_vpu_sem)) == 0) {

		/* found and free the not handled buffer by user applications */
		vpu_free_buffers(filp);

		/* found and free the not closed instance by user applications */
		vpu_free_instances(filp);
		spin_lock_irq(&s_vpu_lock);
		s_vpu_drv_context.open_count--;
		open_count = s_vpu_drv_context.open_count;
		spin_unlock_irq(&s_vpu_lock);
		if (open_count == 0) {
#ifdef SUPPORT_MULTI_INST_INTR
			for (i=0; i<MAX_NUM_INSTANCE; i++) {
				kfifo_reset(&s_interrupt_pending_q[i]);
			}
#endif
			if (s_instance_pool.base) {
				DPRINTK("[VPUDRV] free instance pool\n");
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
				vfree((const void *)s_instance_pool.base);
#else
				vpu_free_dma_buffer(&s_instance_pool);
#endif
				s_instance_pool.base = 0;
			}

		}
		up(&s_vpu_sem);
	}

	return 0;
}

static int vpu_fasync(int fd, struct file *filp, int mode)
{
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}


static int vpu_map_to_register(struct file *fp, struct vm_area_struct *vm)
{
	unsigned long pfn;

	vm_flags_set(vm, VM_IO | VM_RESERVED);
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = s_vpu_register.phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
	unsigned long size;
	dma_addr_t phy_addr;
	struct rtscam_mem_item *mem = NULL;
	int mask = RTSCAM_ISP_BUF_FROM_DEVICE | RTSCAM_ISP_BUF_TO_DEVICE;

	size = vm->vm_end-vm->vm_start;
	phy_addr = vm->vm_pgoff << PAGE_SHIFT;

	if (rtscam_mem_check(rtsmem, phy_addr, (size_t)size, &mem) || !mem) {
		printk(KERN_ERR "[VPUDRV] invalid 0x%08lx : 0x%08lx for mmap\n",
				(unsigned long)phy_addr, size);
		return -EINVAL;
	}

	vm_flags_set(vm, VM_IO | VM_RESERVED);
	if ((mem->buf_io & mask) == 0)
		vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);

	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, size,
			vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_instance_pool_memory(struct file *fp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	int ret;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	char *vmalloc_area_ptr = (char *)s_instance_pool.base;
	unsigned long pfn;

	vm_flags_set(vm, VM_RESERVED);

	/* loop over all pages, map it page individually */
	while (length > 0)
	{
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
				return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}


	return 0;
#else
	vm->vm_flags |= VM_RESERVED;
	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
#endif
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
static int vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	if (vm->vm_pgoff == 0)
		return vpu_map_to_instance_pool_memory(fp, vm);

	if (vm->vm_pgoff == (s_vpu_register.phys_addr>>PAGE_SHIFT))
		return vpu_map_to_register(fp, vm);

	return vpu_map_to_physical_memory(fp, vm);
#else
	if (vm->vm_pgoff) {
		if (vm->vm_pgoff == (s_instance_pool.phys_addr>>PAGE_SHIFT))
			return vpu_map_to_instance_pool_memory(fp, vm);

		return vpu_map_to_physical_memory(fp, vm);
	} else {
		return vpu_map_to_register(fp, vm);
	}
#endif
}

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
static struct rtscam_ge_file_operations vpu_fops = {
#else
static struct file_operations vpu_fops = {
#endif
	.owner = THIS_MODULE,
	.open = vpu_open,
	.read = vpu_read,
	.write = vpu_write,
	/*.ioctl = vpu_ioctl, // for kernel 2.6.9 of C&M*/
	.unlocked_ioctl = vpu_ioctl,
	.release = vpu_release,
	.fasync = vpu_fasync,
	.mmap = vpu_mmap,
};

static int vpu_init_reset(struct device *dev)
{
	s_vpu_rst.axi = devm_reset_control_get(dev, "axi");
	if (IS_ERR(s_vpu_rst.axi)) {
		printk(KERN_ERR"fail to get reset : axi\n");
		return PTR_ERR(s_vpu_rst.axi);
	}
	s_vpu_rst.bpu = devm_reset_control_get(dev, "bpu");
	if (IS_ERR(s_vpu_rst.bpu)) {
		printk(KERN_ERR"fail to get reset : bpu\n");
		return PTR_ERR(s_vpu_rst.bpu);
	}
	s_vpu_rst.core = devm_reset_control_get(dev, "core");
	if (IS_ERR(s_vpu_rst.core)) {
		printk(KERN_ERR"fail to get reset : core\n");
		return PTR_ERR(s_vpu_rst.core);
	}
	s_vpu_rst.sysmem = devm_reset_control_get(dev, "h265-sysmem-up");
	if (IS_ERR(s_vpu_rst.sysmem)) {
		printk(KERN_ERR"fail to get reset : h265-sysmem-up\n");
		return PTR_ERR(s_vpu_rst.sysmem);
	}

	reset_control_reset(s_vpu_rst.axi);
	reset_control_reset(s_vpu_rst.bpu);
	reset_control_reset(s_vpu_rst.core);
	reset_control_deassert(s_vpu_rst.sysmem);

	return 0;
}

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
static int vpu_create_device(struct device *dev)
{
	int ret;

	s_vpu_gdev = rtscam_ge_device_alloc();
	if (!s_vpu_gdev)
		return -ENOMEM;

	strlcpy(s_vpu_gdev->name, VPU_DEV_NAME, sizeof(s_vpu_gdev->name));
	s_vpu_gdev->parent = get_device(dev);
	s_vpu_gdev->release = rtscam_ge_device_release;
	s_vpu_gdev->fops = &vpu_fops;

	ret = rtscam_ge_register_device(s_vpu_gdev);
	if (ret) {
		rtscam_ge_device_release(s_vpu_gdev);
		return ret;
	}

	return ret;
}

static void vpu_remove_device(void)
{
	if (!s_vpu_gdev)
		return;

	put_device(s_vpu_gdev->parent);
	rtscam_ge_unregister_device(s_vpu_gdev);
	s_vpu_gdev = NULL;
}
#else
static void vpu_dev_release(struct device *cd)
{
	cdev_del(&s_vpu_cdev);
	put_device(cd->parent);
}

static int vpu_create_cdev_node(struct device *dev)
{
	s_vpu_dev.devt = s_vpu_major;
	dev_set_name(&s_vpu_dev, "vpu");
	s_vpu_dev.parent = get_device(dev);
	s_vpu_dev.release = vpu_dev_release;

	return device_register(&s_vpu_dev);
}
#endif

#if 0
#define RTS_M_CEIL(A, B)	(((A - 1) / B + 1) * B)

static u32 rts_vpu_calc_common_bufsize(u32 w, u32 h)
{
	u32 tbuf_s_h265, tbuf_s_h264;
	u32 rdo_s, lf_s;

	if (w > RTS_VPU_SUPPORT_MAX_WIDTH || h > RTS_VPU_SUPPORT_MAX_HEIGHT)
		return 0;

	rdo_s = RTS_M_CEIL(w, 64) / 64 * 18 * 16;
	lf_s = RTS_M_CEIL(w, 32) * 5 + RTS_M_CEIL(w, 32) * 3;
	tbuf_s_h265 = lf_s + rdo_s + 4096;

	rdo_s = RTS_M_CEIL(w, 64) / 64 * 24 * 16;
	lf_s = RTS_M_CEIL(w, 16) * 4 + RTS_M_CEIL(w, 16) * 3;
	tbuf_s_h264 = lf_s + rdo_s + 4096;

	s_tempbuf_size = tbuf_s_h265 > tbuf_s_h264 ? tbuf_s_h265 : tbuf_s_h264;

	return SIZE_CODE_BUFFER + s_tempbuf_size;
}

static int rts_vpu_init_common_buffer_size(void)
{
	int i = 0;
	int ret;
	u32 w = 0;
	u32 h = 0;

	do {
		struct device_node *node;
		u32 cur_w, cur_h;

		node = of_parse_phandle(s_vpu_dev.parent->of_node, "streams", i);
		if (!node) {
			DPRINTK("[VPUDRV] parse %d streams device node ok\n", i);
			break;
		}

		ret = of_property_read_u32(node, "width", &cur_w);
		if (ret) {
			of_node_put(node);
			break;
		}
		ret = of_property_read_u32(node, "height", &cur_h);
		if (ret) {
			of_node_put(node);
			break;
		}

		if (cur_w > w)
			w = cur_w;
		if (cur_h > h)
			h = cur_h;

		of_node_put(node);
		i++;
	} while (i < 100);

	if (ret) {
		DPRINTK("[VPUDRV] parse stream info fail\n");
		return ret;
	}

	if (w > RTS_VPU_SUPPORT_MAX_WIDTH || h > RTS_VPU_SUPPORT_MAX_HEIGHT) {
		printk(KERN_ERR "[VPUDRV] too big resolution, max support 3840x2160\n");
		return -1;
	}

	if (i == 0) {
		DPRINTK("[VPUDRV] no stream info at vpu dtb\n");
		return -1;
	}
	s_common_memory.size = MAX_NUM_VPU_CORE *
			rts_vpu_calc_common_bufsize(w, h);
	return 0;
}
#endif


static ssize_t show_streaminfo(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int num = 0;
	int found = 0;
	int i;

	for (i = 0; i < MAX_NUM_INSTANCE; i++) {
		if (s_streaminfo.lowlatency_mode[i]) {
			found = 1;
			break;
		}
	}

	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "stream mode\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "frame count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "drop count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "overflow count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			 "error count\n");

	if (found) {
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%-10s\t%-10ld\t%-10d\t%-10ld\t%-10ld\n",
				 "ring", s_streaminfo.frame_count, s_streaminfo.drop_count,
				 s_streaminfo.overflow_count, s_streaminfo.error_count);
	} else {
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%-10s\t%-10d\t%-10d\t%-10d\t%-10d\n",
				 "ring", 0, 0, 0, 0);
	}
	return num;
}

static ssize_t clr_streaminfo(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int found = 0;
	int i;

	for (i = 0; i < MAX_NUM_INSTANCE; i++) {
		if (s_streaminfo.lowlatency_mode[i]) {
			found = 1;
			break;
		}
	}

	if (!found)
		return count;

	__reset_streaminfo();

	return count;
}
static DEVICE_ATTR(streaminfo, 0664, show_streaminfo, clr_streaminfo);

static ssize_t show_start_line(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int num = 0;
	unsigned int start_line;

	start_line = ReadVpu(s_vpu_register2.virt_addr +
			RTS_IPU_REG_START_LINE);

	num += scnprintf(buf + num, PAGE_SIZE, "%d\n", start_line);

	return num;
}
static DEVICE_ATTR(start_line, 0444, show_start_line, NULL);

static ssize_t show_resetnum(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "reset number\t%d\n", s_vpu_error_reset_num);
}

static DEVICE_ATTR(resetnum, 0444, show_resetnum, NULL);

static void __set_default_clk_freq(struct platform_device *pdev)
{
	struct clk *vpu_bclk;
	struct clk *vpu_cclk;
	int err = 0;
	u32 brate, crate;

	if (pdev) {
		vpu_bclk = vpu_clk_get(&pdev->dev, VPU_BCLK_NAME);
		vpu_cclk = vpu_clk_get(&pdev->dev, VPU_CCLK_NAME);
	} else {
		vpu_bclk = vpu_clk_get(NULL, VPU_BCLK_NAME);
		vpu_cclk = vpu_clk_get(NULL, VPU_CCLK_NAME);
	}

	if (!vpu_bclk || !vpu_cclk) {
		pr_err("[VPUDRV] : not support bclk/cclk controller.\n");
		goto exit;
	} else {
		DPRINTK("[VPUDRV] : get clk controller bclk=%p,cclk=%p\n",
				vpu_bclk, vpu_cclk);
	}

	err = of_property_read_u32(pdev->dev.of_node, "h265-bclk-frequency",
					&brate);
	if (!err && brate) {
		clk_set_rate(vpu_bclk, brate);
		s_bclk_rate = brate;
		DPRINTK("[VPUDRV] : h265 bclk default frequency is %d\n",
				brate);
	}

	err = of_property_read_u32(pdev->dev.of_node, "h265-cclk-frequency",
					&crate);
	if (!err && crate) {
		clk_set_rate(vpu_cclk, crate);
		DPRINTK("[VPUDRV] : h265 cclk default frequency is %d\n",
				crate);
	}
exit:
	vpu_clk_put(vpu_bclk);
	vpu_clk_put(vpu_cclk);
}

static int vpu_probe(struct platform_device *pdev)
{
	int err = 0;
	struct resource *res = NULL;

	DPRINTK("[VPUDRV] vpu_probe\n");
	if (pdev)
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {/* if platform driver is implemented */
		s_vpu_register.phys_addr = res->start;
		s_vpu_register.virt_addr = (unsigned long)ioremap(res->start, res->end - res->start);
		s_vpu_register.size = res->end - res->start;
		DPRINTK("[VPUDRV] : vpu base address get from platform driver physical base addr==0x%lx, virtual base=0x%lx\n", s_vpu_register.phys_addr , s_vpu_register.virt_addr);
	} else {
		s_vpu_register.phys_addr = VPU_REG_BASE_ADDR;
		s_vpu_register.virt_addr = (unsigned long)ioremap(s_vpu_register.phys_addr, VPU_REG_SIZE);
		s_vpu_register.size = VPU_REG_SIZE;
		DPRINTK("[VPUDRV] : vpu base address get from defined value physical base addr==0x%lx, virtual base=0x%lx\n", s_vpu_register.phys_addr, s_vpu_register.virt_addr);

	}

	/* wave521 IPU RS */
	if (pdev)
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		s_vpu_register2.phys_addr = res->start;
		s_vpu_register2.virt_addr = (unsigned long)ioremap(res->start, res->end - res->start);
		s_vpu_register2.size = res->end - res->start;
		DPRINTK("[VPUDRV]: physical base addr == 0x%lx, virtual base = 0x%lx\n", s_vpu_register2.phys_addr, s_vpu_register2.virt_addr);
	}

	err = vpu_init_reset(&pdev->dev);
	if (err) {
		printk(KERN_ERR "init vpu reset fail\n");
		goto ERROR_PROVE_DEVICE;
	}

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
	err = vpu_create_device(&pdev->dev);
	if (err) {
		printk(KERN_ERR "could not create chrdev node\n");
		goto ERROR_PROVE_DEVICE;
	}
#else
	/* get the major number of the character device */
	if ((alloc_chrdev_region(&s_vpu_major, 0, 1, VPU_DEV_NAME)) < 0) {
		err = -EBUSY;
		printk(KERN_ERR "could not allocate major number\n");
		goto ERROR_PROVE_DEVICE;
	}
	printk(KERN_DEBUG "SUCCESS alloc_chrdev_region\n");

	/* initialize the device structure and register the device with the kernel */
	cdev_init(&s_vpu_cdev, &vpu_fops);
	if ((cdev_add(&s_vpu_cdev, s_vpu_major, 1)) < 0) {
		err = -EBUSY;
		printk(KERN_ERR "could not allocate chrdev\n");

		goto ERROR_PROVE_DEVICE;
	}

	err = vpu_create_cdev_node(&pdev->dev);
	if (err) {
		printk(KERN_ERR "could not create chrdev node\n");
		goto ERROR_PROVE_DEVICE;
	}
#endif

	__set_default_clk_freq(pdev);
	atomic_set(&s_vpu_clk_cnt, 0);

	if (pdev)
		s_vpu_clk = vpu_clk_get(&pdev->dev, VPU_CLK_NAME);
	else
		s_vpu_clk = vpu_clk_get(NULL, VPU_CLK_NAME);

	if (!s_vpu_clk)
		printk(KERN_ERR "[VPUDRV] : not support clock controller.\n");
	else
		DPRINTK("[VPUDRV] : get clock controller s_vpu_clk=%p\n", s_vpu_clk);

#ifdef VPU_SUPPORT_CLOCK_CONTROL
#else
	vpu_clk_enable(s_vpu_clk);
#endif

#ifdef VPU_SUPPORT_ISR
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
	if (pdev)
		s_vpu_irq = platform_get_irq(pdev, 0);
	DPRINTK("[VPUDRV] : vpu irq number = %x\n", s_vpu_irq);
#else
	DPRINTK("[VPUDRV] : vpu irq number get from defined value irq=0x%x\n", s_vpu_irq);
#endif

	err = devm_request_irq(&pdev->dev, s_vpu_irq, vpu_irq_handler,
		IRQF_SHARED, "VPU_CODEC_IRQ", (void *)(&s_vpu_drv_context));
	if (err) {
		printk(KERN_ERR "[VPUDRV] :  fail to register interrupt handler\n");
		goto ERROR_PROVE_DEVICE;
	}
#endif


#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
	s_video_memory.phys_addr = VPU_DRAM_PHYSICAL_BASE;
	s_video_memory.base = (unsigned long)ioremap(s_video_memory.phys_addr, PAGE_ALIGN(s_video_memory.size));
	if (!s_video_memory.base) {
		printk(KERN_ERR "[VPUDRV] :  fail to remap video memory physical phys_addr==0x%lx, base==0x%lx, size=%d\n", s_video_memory.phys_addr, s_video_memory.base, (int)s_video_memory.size);
		goto ERROR_PROVE_DEVICE;
	}

	if (vmem_init(&s_vmem, s_video_memory.phys_addr, s_video_memory.size) < 0) {
		printk(KERN_ERR "[VPUDRV] :  fail to init vmem system\n");
		goto ERROR_PROVE_DEVICE;
	}
	DPRINTK("[VPUDRV] success to probe vpu device with reserved video memory phys_addr==0x%lx, base = =0x%lx\n", s_video_memory.phys_addr, s_video_memory.base);
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
	rtsmem = rts_get_mem_info();
	if (!rtsmem) {
		printk(KERN_ERR "[VPUDRV] : fail to get rts reserved memory\n");
		goto ERROR_PROVE_DEVICE;
	}
	DPRINTK("[VPUDRV] success to probe vpu device with rts reserved video memory\n");
#else
	DPRINTK("[VPUDRV] success to probe vpu device with non reserved video memory\n");
#endif
#if 0
	if (!s_common_memory.base) {
		err = rts_vpu_init_common_buffer_size();
		if (err) {
			printk(KERN_ERR
				"[VPUDRV] : fail to init common buffer size\n");
			goto ERROR_PROVE_DEVICE;
		}
		if (vpu_alloc_dma_buffer(&s_common_memory) != 0) {
			printk(KERN_ERR
				"[VPUDRV] : fail to alloc common memory\n");
			goto ERROR_PROVE_DEVICE;
		}
		rtscam_mem_add_property(rtsmem,
				s_common_memory.phys_addr,
				RTSMEM_PROBE_ALLOCATED);
		rtscam_mem_set_info(rtsmem,
				    s_common_memory.phys_addr,
				    "H26x Common");
	}
#endif
	if (__alloc_time_buf() < 0) {
		pr_err("[VPUDRV] : fail to alloc mem for encode time\n");
		goto ERROR_PROVE_DEVICE;
	}

	device_create_file(&pdev->dev, &dev_attr_streaminfo);
	device_create_file(&pdev->dev, &dev_attr_start_line);
	device_create_file(&pdev->dev, &dev_attr_resetnum);

	return 0;

ERROR_PROVE_DEVICE:

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
	vpu_remove_device();
#else
	if (s_vpu_major)
		unregister_chrdev_region(s_vpu_major, 1);
#endif

	if (s_vpu_register.virt_addr)
		iounmap((void *)s_vpu_register.virt_addr);

	return err;
}
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static int vpu_remove(struct platform_device *pdev)
{
	DPRINTK("[VPUDRV] vpu_remove\n");

	if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)s_instance_pool.base);
#else
		vpu_free_dma_buffer(&s_instance_pool);
#endif
		s_instance_pool.base = 0;
	}

	if (s_common_memory.base) {
		vpu_free_dma_buffer(&s_common_memory);
		s_common_memory.base = 0;
	}

	__put_time_buf();
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base) {
		iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;
		vmem_exit(&s_vmem);
	}
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
	if (rtsmem)
		rts_put_mem_info(rtsmem);
	rtsmem = NULL;
#endif

	reset_control_assert(s_vpu_rst.sysmem);
#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
	vpu_remove_device();
#else
	if (s_vpu_major > 0) {
		cdev_del(&s_vpu_cdev);
		device_unregister(&s_vpu_dev);
		unregister_chrdev_region(s_vpu_major, 1);
		s_vpu_major = 0;
	}
#endif

	if (s_vpu_register.virt_addr)
		iounmap((void *)s_vpu_register.virt_addr);

	vpu_hw_assert();
	vpu_clk_disable(s_vpu_clk);
	vpu_clk_put(s_vpu_clk);
	device_remove_file(&pdev->dev, &dev_attr_streaminfo);
	device_remove_file(&pdev->dev, &dev_attr_start_line);
	device_remove_file(&pdev->dev, &dev_attr_resetnum);

	return 0;
}
#endif /*VPU_SUPPORT_PLATFORM_DRIVER_REGISTER*/

#if defined(VPU_SUPPORT_PLATFORM_DRIVER_REGISTER) && defined(CONFIG_PM)
#define W5_MAX_CODE_BUF_SIZE    (512*1024)
#define W5_CMD_INIT_VPU         (0x0001)
#define W5_CMD_SLEEP_VPU        (0x0004)
#define W5_CMD_WAKEUP_VPU       (0x0002)
/*
static void Wave5BitIssueCommand(int core, u32 cmd)
{
	WriteVpuRegister(W5_VPU_BUSY_STATUS, 1);
	WriteVpuRegister(W5_COMMAND, cmd);
	WriteVpuRegister(W5_VPU_HOST_INT_REQ, 1);

	return;
}
*/

static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
#if 0
	int i;
	int core;
	unsigned long timeout = jiffies + HZ;	/* vpu wait timeout to 1sec */
	int product_code;

	DPRINTK("[VPUDRV] vpu_suspend\n");

	vpu_clk_enable(s_vpu_clk);

	if (s_vpu_open_ref_count > 0) {
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);
			if (PRODUCT_CODE_W_SERIES(product_code)) {
				while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
					if (time_after(jiffies, timeout)) {
						DPRINTK("SLEEP_VPU BUSY timeout");
						goto DONE_SUSPEND;
					}
				}

				Wave5BitIssueCommand(core, W5_CMD_SLEEP_VPU);

				while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
					if (time_after(jiffies, timeout)) {
						DPRINTK("SLEEP_VPU BUSY timeout");
						goto DONE_SUSPEND;
					}
				}
				if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
					DPRINTK("SLEEP_VPU failed [0x%x]", ReadVpuRegister(W5_RET_FAIL_REASON));
					goto DONE_SUSPEND;
				}
			}
			else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
				while (ReadVpuRegister(BIT_BUSY_FLAG)) {
					if (time_after(jiffies, timeout))
						goto DONE_SUSPEND;
				}

				for (i = 0; i < 64; i++)
					s_vpu_reg_store[core][i] = ReadVpuRegister(BIT_BASE+(0x100+(i * 4)));
			}
			else {
				DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
				goto DONE_SUSPEND;
			}
		}
	}

	vpu_clk_disable(s_vpu_clk);
	return 0;

DONE_SUSPEND:

	vpu_clk_disable(s_vpu_clk);

	return -EAGAIN;

	return 0;
#else
	reset_control_assert(s_vpu_rst.sysmem);
	vpu_hw_assert();
	vpu_clk_disable(s_vpu_clk);

	return 0;
#endif
}
static int vpu_resume(struct platform_device *pdev)
{
#if 0
	int i;
	int core;
	u32 val;
	unsigned long timeout = jiffies + HZ;	/* vpu wait timeout to 1sec */
	int product_code;

	unsigned long   code_base;
	u32		  	code_size;
	u32		  	remap_size;
	int			 regVal;
	u32		  	hwOption  = 0;


	DPRINTK("[VPUDRV] vpu_resume\n");

	vpu_clk_enable(s_vpu_clk);

	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {

		if (s_bit_firmware_info[core].size == 0) {
			continue;
		}

		product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER);
		if (PRODUCT_CODE_W_SERIES(product_code)) {
			code_base = s_common_memory.phys_addr;
			/* ALIGN TO 4KB */
			code_size = (W5_MAX_CODE_BUF_SIZE&~0xfff);
			if (code_size < s_bit_firmware_info[core].size*2) {
				goto DONE_WAKEUP;
			}

			regVal = 0;
			WriteVpuRegister(W5_PO_CONF, regVal);

			/* Reset All blocks */
            regVal = W5_RST_BLOCK_ALL;
			WriteVpuRegister(W5_VPU_RESET_REQ, regVal);	/*Reset All blocks*/

			/* Waiting reset done */
			while (ReadVpuRegister(W5_VPU_RESET_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

			WriteVpuRegister(W5_VPU_RESET_REQ, 0);

			/* remap page size */
			remap_size = (code_size >> 12) & 0x1ff;
			regVal = 0x80000000 | (W5_REMAP_CODE_INDEX<<12) | (0 << 16) | (1<<11) | remap_size;
			WriteVpuRegister(W5_VPU_REMAP_CTRL, regVal);
			WriteVpuRegister(W5_VPU_REMAP_VADDR,0x00000000);	/* DO NOT CHANGE! */
			WriteVpuRegister(W5_VPU_REMAP_PADDR,code_base);
			WriteVpuRegister(W5_ADDR_CODE_BASE, code_base);
			WriteVpuRegister(W5_CODE_SIZE,      code_size);
			WriteVpuRegister(W5_CODE_PARAM,		0);
			WriteVpuRegister(W5_INIT_VPU_TIME_OUT_CNT,   timeout);

			WriteVpuRegister(W5_HW_OPTION, hwOption);

			/* Interrupt */
			if (product_code == WAVE521_CODE || product_code == WAVE521C_CODE ) {
				regVal  = (1<<INT_WAVE5_ENC_SET_PARAM);
				regVal |= (1<<INT_WAVE5_ENC_PIC);
				regVal |= (1<<INT_WAVE5_INIT_SEQ);
				regVal |= (1<<INT_WAVE5_DEC_PIC);
				regVal |= (1<<INT_WAVE5_BSBUF_EMPTY);
			}
			else {
				// decoder
				regVal  = (1<<INT_WAVE5_INIT_SEQ);
				regVal |= (1<<INT_WAVE5_DEC_PIC);
				regVal |= (1<<INT_WAVE5_BSBUF_EMPTY);
			}

			WriteVpuRegister(W5_VPU_VINT_ENABLE,  regVal);

			Wave5BitIssueCommand(core, W5_CMD_INIT_VPU);
			WriteVpuRegister(W5_VPU_REMAP_CORE_START, 1);

			while (ReadVpuRegister(W5_VPU_BUSY_STATUS)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}

			if (ReadVpuRegister(W5_RET_SUCCESS) == 0) {
				DPRINTK("WAKEUP_VPU failed [0x%x]", ReadVpuRegister(W5_RET_FAIL_REASON));
				goto DONE_WAKEUP;
			}
		}
		else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {

			WriteVpuRegister(BIT_CODE_RUN, 0);

			/*---- LOAD BOOT CODE*/
			for (i = 0; i < 512; i++) {
				val = s_bit_firmware_info[core].bit_code[i];
				WriteVpuRegister(BIT_CODE_DOWN, ((i << 16) | val));
			}

			for (i = 0 ; i < 64 ; i++)
				WriteVpuRegister(BIT_BASE+(0x100+(i * 4)), s_vpu_reg_store[core][i]);

			WriteVpuRegister(BIT_BUSY_FLAG, 1);
			WriteVpuRegister(BIT_CODE_RESET, 1);
			WriteVpuRegister(BIT_CODE_RUN, 1);

			while (ReadVpuRegister(BIT_BUSY_FLAG)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}
		}
		else {
			DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
			goto DONE_WAKEUP;
		}

	}

	if (s_vpu_open_ref_count == 0)
		vpu_clk_disable(s_vpu_clk);

DONE_WAKEUP:

	if (s_vpu_open_ref_count > 0)
		vpu_clk_enable(s_vpu_clk);

	return 0;
#else
	if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)s_instance_pool.base);
#else
		vpu_free_dma_buffer(&s_instance_pool);
#endif
		s_instance_pool.base = 0;
	}

	if (s_common_memory.base) {
		vpu_free_dma_buffer(&s_common_memory);
		s_common_memory.base = 0;
	}

	vpu_hw_reset();
	reset_control_deassert(s_vpu_rst.sysmem);
	vpu_clk_enable(s_vpu_clk);

	return 0;
#endif
}
#else
#define	vpu_suspend	NULL
#define	vpu_resume	NULL
#endif				/* !CONFIG_PM */

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
static const struct of_device_id vpu_dt_ids[] = {
	{ .compatible = "realtek,rts3915-fpga-vpucodec", },
	{ .compatible = "realtek,rts3915-vpucodec", },
	{ .compatible = "realtek,rts3917-vpucodec", },
	{ /* sentinel */ },
};

static struct platform_driver vpu_driver = {
	.driver = {
		   .name = VPU_PLATFORM_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(vpu_dt_ids),
		   },
	.probe = vpu_probe,
	.remove = vpu_remove,
	.suspend = vpu_suspend,
	.resume = vpu_resume,
};
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

static int __init vpu_init(void)
{
	int res;
#ifdef SUPPORT_MULTI_INST_INTR
	int i;
#endif
	DPRINTK("[VPUDRV] begin vpu_init\n");
#ifdef SUPPORT_MULTI_INST_INTR
	for (i=0; i<MAX_NUM_INSTANCE; i++) {
		init_waitqueue_head(&s_interrupt_wait_q[i]);
	}

	for (i=0; i<MAX_NUM_INSTANCE; i++) {
#define MAX_INTERRUPT_QUEUE (16*MAX_NUM_INSTANCE)
		res = kfifo_alloc(&s_interrupt_pending_q[i], MAX_INTERRUPT_QUEUE*sizeof(u32), GFP_KERNEL);
		if (res) {
			DPRINTK("[VPUDRV] kfifo_alloc failed 0x%x\n", res);
		}
	}
#else
	init_waitqueue_head(&s_interrupt_wait_q);
#endif
	s_common_memory.base = 0;
	s_instance_pool.base = 0;
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
	res = platform_driver_register(&vpu_driver);
#else
	res = vpu_probe(NULL);
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

	DPRINTK("[VPUDRV] end vpu_init result=0x%x\n", res);
	return res;
}

static void __exit vpu_exit(void)
{
#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
	DPRINTK("[VPUDRV] vpu_exit\n");

	platform_driver_unregister(&vpu_driver);

#else /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

#ifdef VPU_SUPPORT_CLOCK_CONTROL
#else
	vpu_clk_disable(s_vpu_clk);
#endif
	vpu_clk_put(s_vpu_clk);

	if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)s_instance_pool.base);
#else
		vpu_free_dma_buffer(&s_instance_pool);
#endif
		s_instance_pool.base = 0;
	}

	if (s_common_memory.base) {
		vpu_free_dma_buffer(&s_common_memory);
		s_common_memory.base = 0;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base) {
		iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;

		vmem_exit(&s_vmem);
	}
#elif defined VPU_SUPPORT_RESERVED_RTS_MEMORY
	if (rtsmem)
		rts_put_mem_info(rtsmem);
	rtsmem = NULL;
#endif

#ifdef VPU_SUPPORT_RTS_DEV_REGISTER
	vpu_remove_device();
#else
	if (s_vpu_major > 0) {
		cdev_del(&s_vpu_cdev);
		unregister_chrdev_region(s_vpu_major, 1);
		s_vpu_major = 0;
	}
#endif

#ifdef SUPPORT_MULTI_INST_INTR
	{
		int i;
		for (i=0; i<MAX_NUM_INSTANCE; i++) {
			kfifo_free(&s_interrupt_pending_q[i]);
		}
	}
#endif

	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		s_vpu_register.virt_addr = 0x00;
	}

#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

	return;
}

MODULE_AUTHOR("A customer using C&M VPU, Inc.");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("GPL v2");

module_init(vpu_init);
module_exit(vpu_exit);

static int vpu_hw_reset(void)
{
	DPRINTK("[VPUDRV] request vpu reset from application. \n");

	reset_control_reset(s_vpu_rst.axi);
	reset_control_reset(s_vpu_rst.bpu);
	reset_control_reset(s_vpu_rst.core);

	return 0;
}

static int vpu_hw_assert(void)
{
	DPRINTK("[VPUDRV] request vpu assert from application. \n");

	reset_control_assert(s_vpu_rst.axi);
	reset_control_assert(s_vpu_rst.bpu);
	reset_control_assert(s_vpu_rst.core);

	return 0;
}

static struct clk *vpu_clk_get(struct device *dev, const char *clk_name)
{
	return clk_get(dev, clk_name);
}
static void vpu_clk_put(struct clk *clk)
{
	if (!(clk == NULL || IS_ERR(clk)))
		clk_put(clk);
}
static int vpu_clk_enable(struct clk *clk)
{
		if (!(clk == NULL || IS_ERR(clk))) {
		/* the bellow is for C&M EVB.*/
		/*
		{
			struct clk *s_vpuext_clk = NULL;
			s_vpuext_clk = clk_get(NULL, "vcore");
			if (s_vpuext_clk)
			{
				DPRINTK("[VPUDRV] vcore clk=%p\n", s_vpuext_clk);
				clk_enable(s_vpuext_clk);
			}

			DPRINTK("[VPUDRV] vbus clk=%p\n", s_vpuext_clk);
			if (s_vpuext_clk)
			{
				s_vpuext_clk = clk_get(NULL, "vbus");
				clk_enable(s_vpuext_clk);
			}
		}
		*/
		/* for C&M EVB. */

		DPRINTK("[VPUDRV] vpu_clk_enable\n");
		//customers needs implementation to turn on clock like clk_enable(clk)
		if (atomic_inc_return(&s_vpu_clk_cnt) == 1) {
			reset_control_reset(s_vpu_rst.bpu);
			udelay(1);
			return clk_prepare_enable(clk);
		}
	}

	return 0;
}

static void vpu_clk_disable(struct clk *clk)
{
	if (!(clk == NULL || IS_ERR(clk))) {
		DPRINTK("[VPUDRV] vpu_clk_disable\n");
		//customers needs implementation to turn off clock like clk_disable(clk)
		if (atomic_dec_return(&s_vpu_clk_cnt) == 0) {
			clk_disable_unprepare(clk);
			reset_control_assert(s_vpu_rst.bpu);
		}
	}
}


