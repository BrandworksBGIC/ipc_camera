// SPDX-License-Identifier: GPL-2.0-only
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

#define TAG	"OSDI"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "linux/rts_camera_osdi.h"
#include "rts_camera.h"

#define odd(a)		((a) % 2)
//#define RTS_OSDI_DEBUG			1
//#define RTS_OSDI_POLL_ENABLE		1
#define RTS_OSDI_DEBUG_SRAM	1

#define RTS_OSDI_DRV_NAME		"rts_osdi"
#define RTS_OSDI_DEV_NAME		"rtsosdi"

#define RTS_OSDI_NUM			3
#define RTS_OSDI_BLK_NUM		6
#define RTS_OSDI_BASE_INTERVAL		0x1000
#define RTS_OSDI_COLOR_TABLE_BUF_LEN	190

#define OSDI_FRAME_DONE_BIT		0
#define	OSDI_IS_RUNNING_BIT		1


#define RTS_REG_OSDI_CTRL			0
#define	RTS_REG_OSDI_STATUS			4
#define	RTS_REG_OSDI_INT_ENABLE			8
#define	RTS_REG_OSDI_INT_FLAG			0xc
#define	RTS_REG_OSDI_AXI_TRANSFER_LENGTH	0x10
#define	RTS_REG_OSDI_SET_CONFIG			0x14
#define	RTS_REG_OSDI_BLK_ENABLE			0x18
#define	RTS_REG_OSDI_BLK_STATE			0x1c

#define	RTS_REG_OSDI_BLK0_DDR_ADDR		0x100
#define	RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG1	0x104
#define	RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG2	0x108
#define	RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG3	0x10c
#define	RTS_REG_OSDI_BLK0_MEM_CONFIG		0x110
#define	RTS_REG_OSDI_BLK0_TOTLAL_PIX		0x114
#define	RTS_REG_OSDI_BLK_OFFSET			0x100

#define	RTS_REG_OSDI_ISP_FRAME_INFO		0x618
#define	RTS_REG_OSDI_ISP_FIFO_STATUS		0x61c
#define	RTS_REG_OSDI_0_ENABLE			0x630

#define	RTS_REG_OSDI_LUT_START			0xa00
#define	RTS_REG_OSDI_LUT_END			0xab8

/* This lock is added for set color table */
struct mutex m_ioc_lock;

enum osdi_color_type {
	OSDI_COLOR_TYPE_A,
	OSDI_COLOR_TYPE_R,
	OSDI_COLOR_TYPE_G,
	OSDI_COLOR_TYPE_B,
	OSDI_COLOR_TYPE_RESERVED,
};

enum {
	OSDI_START,
	OSDI_STOP,
	OSDI_FRAME_STOP,
	OSDI_RESET,
	OSDI_PURE_COLOR,
	OSDI_BLK0_WIDTH,
	OSDI_BLK0_HEIGHT,
	OSDI_BLK0_START_X,
	OSDI_BLK0_START_Y,
	OSDI_BLK0_FMT,
	OSDI_BLK0_DDR_ADDR,
	OSDI_BLK0_TOTAL_PIXEL,
	OSDI_FRAME_WIDTH,
	OSDI_FRAME_HEIGHT,
	OSDI_BYTE_TRANSFER,
	OSDI_FRAME_STOP_INT_FLAG,
	OSDI_FRAME_END_INT_FLAG,
	OSDI_BUSY_STATUS,
	OSDI_AXI_BUSY_STATUS
};

struct osdi_color_field_t {
	int name;
	uint8_t type;
	uint8_t base;
	uint8_t	size;
};

static const struct osdi_color_field_t ColorTableDesc[] = {
	{RTS_OSDI_RGBA_1111,
			OSDI_COLOR_TYPE_A,		186,	2},
	{RTS_OSDI_RGBA_1111,
			OSDI_COLOR_TYPE_B,		184,	2},
	{RTS_OSDI_RGBA_1111,
			OSDI_COLOR_TYPE_G,		182,	2},
	{RTS_OSDI_RGBA_1111,
			OSDI_COLOR_TYPE_R,		180,	2},
	{RTS_OSDI_RGBA_2222,
			OSDI_COLOR_TYPE_A,		176,	4},
	{RTS_OSDI_RGBA_2222,
			OSDI_COLOR_TYPE_B,		172,	4},
	{RTS_OSDI_RGBA_2222,
			OSDI_COLOR_TYPE_G,		168,	4},
	{RTS_OSDI_RGBA_2222,
			OSDI_COLOR_TYPE_R,		164,	4},
	{RTS_OSDI_RGBA_4444,
			OSDI_COLOR_TYPE_A,		148,	16},
	{RTS_OSDI_RGBA_4444,
			OSDI_COLOR_TYPE_B,		132,	16},
	{RTS_OSDI_RGBA_4444,
			OSDI_COLOR_TYPE_G,		116,	16},
	{RTS_OSDI_RGBA_4444,
			OSDI_COLOR_TYPE_R,		100,	16},
	{RTS_OSDI_PURE_COLOR,
			OSDI_COLOR_TYPE_RESERVED,	98,	2},
	{RTS_OSDI_RGBA_5551,
			OSDI_COLOR_TYPE_A,		96,	2},
	{RTS_OSDI_RGBA_5551,
			OSDI_COLOR_TYPE_B,		64,	32},
	{RTS_OSDI_RGBA_5551,
			OSDI_COLOR_TYPE_G,		32,	32},
	{RTS_OSDI_RGBA_5551,
			OSDI_COLOR_TYPE_R,		0,	32},
};

struct osdi_color_table {
	u8 *data;
	u32 length;
};

struct rtscam_osdi {
	struct device *dev;
	void __iomem *hwregs;

	struct rtscam_ge_device *jdev;

	struct completion stop_completion;

	atomic_t use_count;
	u8 index;

	struct osdi_all_blk_info all_blk_info;
	struct osdi_frame_info frame_info;
	struct osdi_color_table *color_table;
};

typedef struct {
	off_t offset;
	int lsb;
	int bits;
} reg_desc_t;

static reg_desc_t rosdi_regs[] = {
	[OSDI_START] = {RTS_REG_OSDI_CTRL, 0, 1},
	[OSDI_STOP] = {RTS_REG_OSDI_CTRL, 2, 1},
	[OSDI_FRAME_STOP] = {RTS_REG_OSDI_CTRL, 1, 1},
	[OSDI_RESET] = {RTS_REG_OSDI_CTRL, 3, 1},
	[OSDI_PURE_COLOR] = {RTS_REG_OSDI_AXI_TRANSFER_LENGTH, 8, 24},
	[OSDI_BLK0_WIDTH] = {RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG1, 0, 13},
	[OSDI_BLK0_HEIGHT] = {RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG1, 16, 13},
	[OSDI_BLK0_START_X] = {RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG2, 0, 13},
	[OSDI_BLK0_START_Y] = {RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG2, 16, 13},
	[OSDI_BLK0_FMT] = {RTS_REG_OSDI_BLK0_GRAPHIC_CONFIG3, 0, 3},
	[OSDI_BLK0_DDR_ADDR] = {RTS_REG_OSDI_BLK0_DDR_ADDR, 0, 32},
	[OSDI_BLK0_TOTAL_PIXEL] = {RTS_REG_OSDI_BLK0_TOTLAL_PIX, 0, 26},
	[OSDI_FRAME_WIDTH] = {RTS_REG_OSDI_ISP_FRAME_INFO, 0, 13},
	[OSDI_FRAME_HEIGHT] = {RTS_REG_OSDI_ISP_FRAME_INFO, 16, 13},
	[OSDI_BYTE_TRANSFER] = {RTS_REG_OSDI_AXI_TRANSFER_LENGTH, 0, 2},
	[OSDI_FRAME_STOP_INT_FLAG] = {RTS_REG_OSDI_INT_FLAG, 3, 1},
	[OSDI_FRAME_END_INT_FLAG] = {RTS_REG_OSDI_INT_FLAG, 2, 1},
	[OSDI_BUSY_STATUS] = {RTS_REG_OSDI_STATUS, 1, 1},
	[OSDI_AXI_BUSY_STATUS] = {RTS_REG_OSDI_STATUS, 0, 1},
};

static const struct osdi_color_field_t *__find_color_field(
			int name, int type)
{
	int i = 0;
	int len = ARRAY_SIZE(ColorTableDesc);

	for (i = 0; i < len; i++) {
		if (ColorTableDesc[i].name == name &&
				ColorTableDesc[i].type == type)
			return &ColorTableDesc[i];
	}

	return NULL;
}

static int rtscam_osdi_read_reg(struct rtscam_osdi *osdi, off_t reg)
{
	u32 val;

	val = le32_to_cpu(ioread32(osdi->hwregs +
			osdi->index * RTS_OSDI_BASE_INTERVAL + reg));

	return val;
}

static void rtscam_osdi_write_reg(struct rtscam_osdi *osdi,
				  u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value),
		osdi->hwregs + osdi->index * RTS_OSDI_BASE_INTERVAL + reg);

#ifdef RTS_OSDI_DEBUG
	if (reg >= RTS_REG_OSDI_LUT_START && reg <= RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x1000 + RTS_REG_OSDI_LUT_START && reg <= 0x1000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x2000 + RTS_REG_OSDI_LUT_START && reg <= 0x2000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x3000 + RTS_REG_OSDI_LUT_START && reg <= 0x3000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg != 0xc && reg != 0x100c && reg != 0x200c && reg != 0x300c)
		printk("++++++write: val:0x%08x\treg:0x%08x\n", value,
			(u32)(osdi->index * RTS_OSDI_BASE_INTERVAL + reg));
#endif
}

static void rtscam_osdi_write_color_reg(struct rtscam_osdi *osdi,
				  u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), osdi->hwregs + reg);

#ifdef RTS_OSDI_DEBUG
	if (reg >= RTS_REG_OSDI_LUT_START && reg <= RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x1000 + RTS_REG_OSDI_LUT_START && reg <= 0x1000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x2000 + RTS_REG_OSDI_LUT_START && reg <= 0x2000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg >= 0x3000 + RTS_REG_OSDI_LUT_START && reg <= 0x3000 +
		RTS_REG_OSDI_LUT_END)
		return;
	if (reg != 0xc && reg != 0x100c && reg != 0x200c && reg != 0x300c)
		printk("++++++write: val:0x%08x\treg:0x%08x\n", value,
			(u32)reg);
#endif
}

static void __osdi_write_id(struct rtscam_osdi *rosdi,
				int id, unsigned int val, u32 offset)
{
	unsigned int v;
	unsigned int mask;
	reg_desc_t *des = &rosdi_regs[id];

	mask = (unsigned int)(((1LL << des->bits) - 1) << des->lsb);
	v = rtscam_osdi_read_reg(rosdi, des->offset + offset);
	v &= (~mask);

	v |= ((val << des->lsb) & mask);
	rtscam_osdi_write_reg(rosdi, v, des->offset + offset);
}

static int __osdi_read_id(struct rtscam_osdi *rosdi, int id, u32 offset)
{
	unsigned int v;
	reg_desc_t *des = &rosdi_regs[id];

	v = rtscam_osdi_read_reg(rosdi, des->offset + offset);

	return ((v >> des->lsb) & (unsigned int)((1LL << des->bits) - 1));
}

static int rtscam_osdi_enable_clk(struct rtscam_osdi *rosdi, int enable)
{
	return 0;
}

static int rtscam_osdi_enable_interrupt(struct rtscam_osdi *osdi, int enable)
{
	u32 int_f = 0xffffffff;
#ifndef RTS_OSDI_POLL_ENABLE
	u32 int_en;

	if (enable)
		int_en = 0xf;
	else
		int_en = 0;

	rtscam_osdi_write_reg(osdi, int_en, RTS_REG_OSDI_INT_ENABLE);
#endif
	rtscam_osdi_write_reg(osdi, int_f, RTS_REG_OSDI_INT_FLAG);
	return 0;
}

#ifndef RTS_OSDI_POLL_ENABLE
static irqreturn_t rtscam_osdi_irq(int irq, void *data)
{

	struct rtscam_osdi *rosdi = data;
	u32 status;
	u32 mask;
	off_t reg;

	reg = RTS_REG_OSDI_INT_FLAG;
	status = rtscam_osdi_read_reg(rosdi, reg);
	if ((status & 0x1) != 1)
		return IRQ_NONE;

	if (rosdi->index >= RTS_OSDI_NUM)
		return IRQ_NONE;

	mask = 0x2;
	if (status & mask) {
		rtscam_osdi_write_reg(rosdi, mask, reg);
		rtscam_osdi_write_reg(rosdi, 0xd, RTS_REG_OSDI_INT_ENABLE);
		rtsprintk(RTS_TRACE_WARNING, "osdi underflow!\n");
		return IRQ_HANDLED;
	}

	mask = 0x4;
	if (status & mask) {
		rtscam_osdi_write_reg(rosdi, mask, reg);
		rtscam_osdi_write_reg(rosdi, 0xf, RTS_REG_OSDI_INT_ENABLE);
		/*no extra handle*/
		return IRQ_HANDLED;
	}

	mask = 0x8;
	if (status & mask) {
		rtscam_osdi_write_reg(rosdi, mask, reg);
		if (!completion_done(&rosdi->stop_completion))
			complete(&rosdi->stop_completion);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
#endif

static int __set_osdi_blk_parameter(struct rtscam_osdi *rosdi, int index)
{
	uint32_t val;

	if (!rosdi)
		return -EINVAL;

	if (index < 0 || index >= RTSOSDI_MAX_BLK_NUM)
		return -EINVAL;

	val = rtscam_osdi_read_reg(rosdi, RTS_REG_OSDI_SET_CONFIG);
	val |= (1 << index);
	rtscam_osdi_write_reg(rosdi, val, RTS_REG_OSDI_SET_CONFIG);

	return 0;
}

static int __set_osdi_blk_enable(struct rtscam_osdi *rosdi, int idx, int value)
{
	uint32_t val;

	if (!rosdi)
		return -EINVAL;

	if (idx < 0 || idx >= RTSOSDI_MAX_BLK_NUM)
		return -EINVAL;

	val = rtscam_osdi_read_reg(rosdi, RTS_REG_OSDI_BLK_ENABLE);

	if (value)
		val |= (1 << idx);
	else
		val &= (~(1 << idx));

	rtscam_osdi_write_reg(rosdi, val, RTS_REG_OSDI_BLK_ENABLE);

	return 0;
}

static uint8_t __get_osdi_blk_enable(struct rtscam_osdi *rosdi, int idx)
{
	uint32_t val;

	if (!rosdi)
		return -EINVAL;

	if (idx < 0 || idx >= RTSOSDI_MAX_BLK_NUM)
		return -EINVAL;

	val = rtscam_osdi_read_reg(rosdi, RTS_REG_OSDI_BLK_ENABLE);

	val = (val >> idx) & 1;

	return val;
}

static int __set_osdi_enable(struct rtscam_osdi *rosdi)
{
	uint32_t val;

	if (!rosdi)
		return -EINVAL;

	val = rtscam_osdi_read_reg(rosdi, RTS_REG_OSDI_0_ENABLE);
	val |= 1;
	rtscam_osdi_write_reg(rosdi, val, RTS_REG_OSDI_0_ENABLE);
	return 0;
}

static void __config_osdi_blk_mem(struct rtscam_osdi *rosdi)
{
	__osdi_write_id(rosdi, OSDI_BYTE_TRANSFER, 2, 0);
	if (rosdi->index == 0 || rosdi->index == 1) {
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 0),
			RTS_REG_OSDI_BLK0_MEM_CONFIG);
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 2),
			RTS_REG_OSDI_BLK0_MEM_CONFIG +
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 4),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 2 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 6),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 3 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 8),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 4 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | 10),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 5 *
			RTS_REG_OSDI_BLK_OFFSET);
	} else {
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 0),
			RTS_REG_OSDI_BLK0_MEM_CONFIG);
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 1),
			RTS_REG_OSDI_BLK0_MEM_CONFIG +
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 2),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 2 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 3),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 3 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 4),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 4 *
			RTS_REG_OSDI_BLK_OFFSET);
		rtscam_osdi_write_reg(rosdi, ((0 << 8) | 5),
			RTS_REG_OSDI_BLK0_MEM_CONFIG + 5 *
			RTS_REG_OSDI_BLK_OFFSET);
	}
}
#ifdef RTS_OSDI_DEBUG_SRAM
static int __change_osdi_blk_mem(struct rtscam_osdi *rosdi,
				struct osdi_blk_info *info)
{
	if (rosdi->index == 0 || rosdi->index == 1) {
		rtscam_osdi_write_reg(rosdi, ((1 << 8) | info->mem_ddr),
			RTS_REG_OSDI_BLK0_MEM_CONFIG);
	} else {
		rtscam_osdi_write_reg(rosdi, info->mem_ddr,
			RTS_REG_OSDI_BLK0_MEM_CONFIG);
	}
	return 0;
}
#endif

static void __read_osdi_blk(struct rtscam_osdi *rosdi,
				struct osdi_blk_info *info)
{
	u32 offset;

	offset = info->index * RTS_REG_OSDI_BLK_OFFSET;

	info->fmt = __osdi_read_id(rosdi, OSDI_BLK0_FMT, offset);
	info->phy_addr = __osdi_read_id(rosdi, OSDI_BLK0_DDR_ADDR, offset);
	info->width = __osdi_read_id(rosdi, OSDI_BLK0_WIDTH, offset);
	info->height = __osdi_read_id(rosdi, OSDI_BLK0_HEIGHT, offset);
	info->start_x = __osdi_read_id(rosdi, OSDI_BLK0_START_X, offset);
	info->start_y = __osdi_read_id(rosdi, OSDI_BLK0_START_Y, offset);
	info->enable = __get_osdi_blk_enable(rosdi, info->index);
	info->transfer_length = __osdi_read_id(rosdi, OSDI_BYTE_TRANSFER, 0);
}

static void __write_osdi_blk(struct rtscam_osdi *rosdi,
				struct osdi_blk_info *info)
{
	u32 offset;

	offset = info->index * RTS_REG_OSDI_BLK_OFFSET;

	if (!info->enable) {
		__set_osdi_blk_enable(rosdi,
				info->index, info->enable);
		return;
	}

	__osdi_write_id(rosdi, OSDI_BLK0_FMT, info->fmt, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_DDR_ADDR, info->phy_addr, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_WIDTH, info->width, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_HEIGHT, info->height, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_START_X, info->start_x, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_START_Y, info->start_y, offset);
	__osdi_write_id(rosdi, OSDI_BLK0_TOTAL_PIXEL,
				info->width * info->height, offset);
	__set_osdi_blk_enable(rosdi, info->index, info->enable);
}

static void __clear_osdi_all_blk_enable(struct rtscam_osdi *rosdi,
				struct osdi_all_blk_info *info)
{
	int i;

	for (i = 0; i < RTS_OSDI_BLK_NUM; i++) {
		__set_osdi_blk_enable(rosdi, info->infos[i].index, 0);
		__set_osdi_blk_parameter(rosdi, info->infos[i].index);
	}
}

static void __write_osdi_all_blk_enable(struct rtscam_osdi *rosdi,
				struct osdi_all_blk_info *info)
{
	int i;

	for (i = 0; i < RTS_OSDI_BLK_NUM; i++) {
		__set_osdi_blk_enable(rosdi, info->infos[i].index,
			info->infos[i].enable);
		__set_osdi_blk_parameter(rosdi, info->infos[i].index);
	}
}

static void __write_osdi_frame(struct rtscam_osdi *rosdi)
{
	__osdi_write_id(rosdi, OSDI_FRAME_WIDTH, rosdi->frame_info.width, 0);
	__osdi_write_id(rosdi, OSDI_FRAME_HEIGHT, rosdi->frame_info.height, 0);
}

static void __write_osdi_color_table(struct rtscam_osdi *rosdi)
{
	int i, cnt;
	u32 val, reg;
	u32 start = RTS_REG_OSDI_LUT_START;
	u32 end = RTS_REG_OSDI_LUT_END;
	u32 *pval;

	if (!rosdi->color_table || !rosdi->color_table->data)
		return;

	pval = (u32 *)rosdi->color_table->data;
	cnt = (end - start) / 4 + 1;
	for (i = 0; i < cnt; i++) {
		val = *(pval + i);
		reg = start + i * 4;
		rtscam_osdi_write_color_reg(rosdi, val, reg);
	}
}

static int __check_osdi_para(struct rtscam_osdi *rosdi)
{
	if (!rosdi->frame_info.width || !rosdi->frame_info.height)
		return -EINVAL;

	return 0;
}

static int __init_osdi_color_table(struct osdi_color_table **pcolor_table)
{
	u8 *pdata;
	const struct osdi_color_field_t *field;
	struct osdi_color_table *color_table;
	int i, j, c;

	if (!pcolor_table) {
		rtsprintk(RTS_TRACE_ERROR, "osdi pcolor table is null\n");
		return -EINVAL;
	}

	color_table = kzalloc(sizeof(*color_table), GFP_KERNEL);
	if (!color_table)
		return -EINVAL;
	pdata = kzalloc(
			RTS_OSDI_COLOR_TABLE_BUF_LEN, GFP_KERNEL);
	if (!pdata)
		return -EINVAL;

	color_table->length = RTS_OSDI_COLOR_TABLE_BUF_LEN;
	color_table->data = pdata;

	for (j = RTS_OSDI_RGBA_4444; j >= RTS_OSDI_RGBA_1111; j--) {
		for (i = (OSDI_COLOR_TYPE_RESERVED - 1); i >= 0; i--) {
			field = __find_color_field(j, i);
			if (!field)
				return -EINVAL;
			for (c = 0; c < field->size - 1; c++)
				*(pdata + field->base + c) =
					c * (0xff / (field->size - 1));
			*(pdata + field->base + field->size - 1) = 0xff;
		}
	}

	field = __find_color_field(RTS_OSDI_PURE_COLOR,
		OSDI_COLOR_TYPE_RESERVED);
	*(pdata + field->base) = 0;
	*(pdata + field->base + 1) = 0xff;

	*pcolor_table = color_table;

	return 0;
}

void __release_osdi_color_table(struct rtscam_osdi *rosdi)
{
	if (!rosdi->color_table || !rosdi->color_table->data ||
			!rosdi->color_table->length)
		return;

	kfree(rosdi->color_table->data);
	rosdi->color_table->data = NULL;
	rosdi->color_table->length = 0;
	kfree(rosdi->color_table);
}

static int rtscam_osdi_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_osdi *rosdi = rtscam_ge_get_drvdata(gdev);

	if (atomic_inc_return(&rosdi->use_count) == 1) {
		__set_osdi_enable(rosdi);
		rtscam_osdi_enable_clk(rosdi, 1);
		__config_osdi_blk_mem(rosdi);
	}

	filp->private_data = rosdi;

	return 0;
}

static int rtscam_osdi_close(struct file *filp)
{
	struct rtscam_osdi *rosdi = filp->private_data;

	if (atomic_dec_return(&rosdi->use_count) == 0) {
		rtscam_osdi_enable_interrupt(rosdi, 0);
		rtscam_osdi_enable_clk(rosdi, 0);
	}
	filp->private_data = NULL;
	return 0;
}

static int __start_osdi(struct rtscam_osdi *rosdi)
{
	int ret;
	struct osdi_all_blk_info *info = NULL;

	if (!rosdi)
		return -EINVAL;

	ret = __check_osdi_para(rosdi);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			"osdi frame is not valid(index = %d)\n", rosdi->index);
		return -EINVAL;
	}

	info = &rosdi->all_blk_info;
	__clear_osdi_all_blk_enable(rosdi, info);
	__osdi_write_id(rosdi, OSDI_START, 1, 0);
	__write_osdi_all_blk_enable(rosdi, info);
	rtscam_osdi_enable_interrupt(rosdi, 1);

	return 0;
}

static int __frame_stop_osdi(struct rtscam_osdi *rosdi)
{
	int ret = 0;
#ifdef RTS_OSDI_POLL_ENABLE
	int i, val;
	int cnt = 10000;
#endif

	if (!rosdi)
		return -EINVAL;
#ifndef RTS_OSDI_POLL_ENABLE
	init_completion(&rosdi->stop_completion);
#endif
	__osdi_write_id(rosdi, OSDI_FRAME_STOP, 1, 0);

#ifndef RTS_OSDI_POLL_ENABLE
	ret = wait_for_completion_timeout(
			&rosdi->stop_completion, 3000 * HZ / 1000);
	if (ret <= 0)
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for stop finish fail(index = %d)\n",
			rosdi->index);
	ret = ret > 0 ? 0 : -EINVAL;
#else
	for (i = 0; i < cnt; i++) {
		val = __osdi_read_id(rosdi, OSDI_FRAME_STOP_INT_FLAG, 0);
		if (val) {
			__osdi_write_id(rosdi, OSDI_FRAME_STOP_INT_FLAG, 1, 0);
			break;
		}
		udelay(1000);
	}
	if (i == cnt)
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for stop(next frame)inish fail by polling\n");
	else
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for stop(next frame)inish success by polling\n");
#endif

	__osdi_write_id(rosdi, OSDI_RESET, 1, 0);

	return ret;
}

static int __immediate_stop_osdi(struct rtscam_osdi *rosdi)
{
	int ret = 0;
#ifdef RTS_OSDI_POLL_ENABLE
	int i, val;
	int cnt = 10000;
#endif

	if (!rosdi)
		return -EINVAL;
#ifndef RTS_OSDI_POLL_ENABLE
	init_completion(&rosdi->stop_completion);
#endif
	__osdi_write_id(rosdi, OSDI_STOP, 1, 0);
	__osdi_write_id(rosdi, OSDI_RESET, 1, 0);

#ifndef RTS_OSDI_POLL_ENABLE
	ret = wait_for_completion_timeout(
			&rosdi->stop_completion, 3000 * HZ / 1000);
	if (ret <= 0)
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for stop finish fail(index = %d)\n",
			rosdi->index);
	ret = ret > 0 ? 0 : -EINVAL;
#else
	for (i = 0; i < cnt; i++) {
		val = __osdi_read_id(rosdi, OSDI_FRAME_STOP_INT_FLAG, 0);
		if (val) {
			__osdi_write_id(rosdi, OSDI_FRAME_STOP_INT_FLAG, 1, 0);
			break;
		}
		udelay(1000);
	}
	if (i == cnt)
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for immediate stop finish fail by poll\n");
	else
		rtsprintk(RTS_TRACE_ERROR,
			"osd wait for immediate stop finish success by poll\n");
#endif

	return ret;
}

static int __config_osdi_frame(struct rtscam_osdi *rosdi,
				struct osdi_frame_info *info)
{
	if (!rosdi || !info)
		return -EINVAL;

	if (!info->width || !info->height)
		return -EINVAL;

	rosdi->frame_info.width = info->width;
	rosdi->frame_info.height = info->height;

	__write_osdi_frame(rosdi);

	return 0;
}

static int __check_blk_info(struct osdi_blk_info *info)
{
	if (odd(info->start_x) || odd(info->start_y)) {
		rtsprintk(RTS_TRACE_ERROR,
			"blk start_x or start_y should be even\n");
		return -EINVAL;
	}

	if (odd(info->width)) {
		rtsprintk(RTS_TRACE_ERROR,
			"blk width should be even\n");
		return -EINVAL;
	}

	return 0;
}

static int __get_osdi_blk(struct rtscam_osdi *rosdi,
				struct osdi_blk_info *info)
{
	if (!rosdi || !info)
		return -EINVAL;

	__read_osdi_blk(rosdi, info);

	return 0;
}

static int __get_osdi_all_blk(struct rtscam_osdi *rosdi,
				struct osdi_all_blk_info *info)
{
	int i;
	int ret;

	if (!rosdi || !info)
		return -EINVAL;

	for (i = 0; i < RTS_OSDI_BLK_NUM; i++) {
		ret = __get_osdi_blk(rosdi, &info->infos[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static int __get_osdi_blk_sram_len(struct rtscam_osdi *rosdi)
{
	int sram_len;
	int enable_num = 0;
	int i;

	for (i = 0; i < RTS_OSDI_BLK_NUM; i++) {
		if (rosdi->all_blk_info.infos[i].enable)
			enable_num++;
	}

	if (rosdi->index == 0 || rosdi->index == 1) {
		if (enable_num > 0 && enable_num <= 4)
			sram_len = 12 / enable_num;
		else
			sram_len = 2;
	} else {
		if (enable_num > 0 && enable_num <= 3)
			sram_len = 6 / enable_num;
		else
			sram_len = 1;
	}

	return sram_len;
}

static void __config_osdi_blk_sram(struct rtscam_osdi *rosdi, int sram_len)
{
	int i, j;
	int sram_addr;
	u32 offset;
	struct osdi_all_blk_info *info = NULL;

	for (i = 0, j = 0; i < RTS_OSDI_BLK_NUM; i++) {
		info = &rosdi->all_blk_info;
		if (info->infos[i].enable) {
			offset = info->infos[i].index *
					RTS_REG_OSDI_BLK_OFFSET;
			sram_addr = j * sram_len;
			rtscam_osdi_write_reg(rosdi,
					(((sram_len - 1) << 8) | sram_addr),
					RTS_REG_OSDI_BLK0_MEM_CONFIG + offset);
			j++;
		}
	}
	rtscam_osdi_write_reg(rosdi, 0x3f, RTS_REG_OSDI_SET_CONFIG);
}

static int __config_osdi_blk(struct rtscam_osdi *rosdi,
				struct osdi_blk_info *info)
{
	int ret;
	int sram_len;

	if (!rosdi || !info)
		return -EINVAL;

	ret = __check_blk_info(info);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "check blk info fail\n");
		return -EINVAL;
	}

	memcpy(&rosdi->all_blk_info.infos[info->index], info, sizeof(*info));
	__write_osdi_blk(rosdi, info);
	sram_len = __get_osdi_blk_sram_len(rosdi);
	__config_osdi_blk_sram(rosdi, sram_len);

	return 0;
}

static int __config_osdi_all_blk(struct rtscam_osdi *rosdi,
				struct osdi_all_blk_info *info)
{
	int i;
	int ret;

	if (!rosdi || !info)
		return -EINVAL;

	for (i = 0; i < RTS_OSDI_BLK_NUM; i++) {
		ret = __config_osdi_blk(rosdi, &info->infos[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static int __check_all_osdi_status(struct rtscam_osdi *rosdi)
{
	struct rtscam_osdi *first_osdi = NULL;
	struct rtscam_osdi *p = NULL;
	int ret = 0;
	int i = 0;

	if (!rosdi)
		return -EINVAL;

	first_osdi = rosdi - rosdi->index;
	for (i = 0; i < RTS_OSDI_NUM; i++) {
		p = first_osdi + i;
		if (p->all_blk_info.start_flag)
			return -EINVAL;
	}

	return ret;
}

static int __set_osdi_color_table(struct rtscam_osdi *rosdi,
				struct osdi_color_table_info *info)
{
	int i;
	int ret;
	u8 *pdata;
	u8 value[4];
	u8 len[4] = {info->alpha, info->red, info->green, info->blue};
	const struct osdi_color_field_t *field;

	if (!rosdi || !info)
		return -EINVAL;

	if (!rosdi->color_table || !rosdi->color_table->data ||
			!rosdi->color_table->length)
		return -EINVAL;

	ret = __check_all_osdi_status(rosdi);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			"set color table fail, please stop all osdi\n");
		return ret;
	}

	pdata = rosdi->color_table->data;
	if (info->fmt == RTS_OSDI_PURE_COLOR) {
		field = __find_color_field(info->fmt,
			OSDI_COLOR_TYPE_RESERVED);
		if (!field) {
			rtsprintk(RTS_TRACE_ERROR,
				"Can't find color(%d)\n", info->fmt);
			return -EINVAL;
		}
		if (info->alpha >= field->size) {
			rtsprintk(RTS_TRACE_ERROR,
				"invalid alpha index\n");
			return -EINVAL;
		}
		value[0] = (u8)(info->val & 0xff);
		*(pdata + field->base + info->alpha) = value[0];
		__osdi_write_id(rosdi, OSDI_PURE_COLOR, info->val >> 8, 0);
		goto exit;
	}

	value[0] = (u8)(info->val & 0xff);
	value[1] = (u8)((info->val >> 24) & 0xff);
	value[2] = (u8)((info->val >> 16) & 0xff);
	value[3] = (u8)((info->val >> 8) & 0xff);

	for (i = 0; i < OSDI_COLOR_TYPE_RESERVED; i++) {
		field = __find_color_field(info->fmt, i);
		if (!field) {
			rtsprintk(RTS_TRACE_ERROR,
				"Can't find color(%d)\n", info->fmt);
			return -EINVAL;
		}
		if (len[i] >= field->size) {
			rtsprintk(RTS_TRACE_ERROR,
				"invalid rgba index\n");
			return -EINVAL;
		}
		*(pdata + field->base + len[i]) = value[i];
	}

exit:
	__write_osdi_color_table(rosdi);

	return 0;
}

static int __get_osdi_color_table(struct rtscam_osdi *rosdi,
				struct osdi_color_table_info *info)
{
	int i;
	u8 *pdata;
	u8 value[4];
	u8 len[4] = {info->alpha, info->red, info->green, info->blue};
	const struct osdi_color_field_t *field;

	if (!rosdi || !info)
		return -EINVAL;

	if (!rosdi->color_table || !rosdi->color_table->data ||
			!rosdi->color_table->length)
		return -EINVAL;

	pdata = rosdi->color_table->data;
	if (info->fmt == RTS_OSDI_PURE_COLOR) {
		field = __find_color_field(info->fmt,
			OSDI_COLOR_TYPE_RESERVED);
		if (!field) {
			rtsprintk(RTS_TRACE_ERROR,
				"Can't find color(%d)\n", info->fmt);
			return -EINVAL;
		}
		value[0] = *(pdata + field->base + len[0]);
		info->val = (__osdi_read_id(rosdi, OSDI_PURE_COLOR, 0) << 8) | value[0];
		return 0;
	}

	for (i = 0; i < OSDI_COLOR_TYPE_RESERVED; i++) {
		field = __find_color_field(info->fmt, i);
		if (!field) {
			rtsprintk(RTS_TRACE_ERROR,
				"Can't find color(%d)\n", info->fmt);
			return -EINVAL;
		}
		if (len[i] >= field->size) {
			rtsprintk(RTS_TRACE_ERROR,
				"invalid rgba index\n");
			return -EINVAL;
		}
		value[i] = *(pdata + field->base + len[i]);
	}
	info->val = (u32)((value[1] << 24)|
			(value[2] << 16) | (value[3] << 8) | value[0]);
	return 0;
}

static long rtscam_osdi_do_ioctl(struct file *filp, unsigned int cmd,
				 void *arg)
{
	struct rtscam_osdi *rosdi = filp->private_data;
	int err = 0;

	if (_IOC_TYPE(cmd) != RTSOSDI_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RTSOSDI_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RTSOSDI_IOC_START:
		mutex_lock(&m_ioc_lock);
		if (!rosdi->all_blk_info.start_flag) {
			err = __start_osdi(rosdi);
			if (!err)
				rosdi->all_blk_info.start_flag = 1;
		}
		mutex_unlock(&m_ioc_lock);
		break;
	case RTSOSDI_IOC_FRAME_STOP:
		mutex_lock(&m_ioc_lock);
		if (rosdi->all_blk_info.start_flag) {
			err = __frame_stop_osdi(rosdi);
			rosdi->all_blk_info.start_flag = 0;
		}
		mutex_unlock(&m_ioc_lock);
		break;
	case RTSOSDI_IOC_IMMEDIATE_STOP:
		mutex_lock(&m_ioc_lock);
		if (rosdi->all_blk_info.start_flag) {
			err = __immediate_stop_osdi(rosdi);
			rosdi->all_blk_info.start_flag = 0;
		}
		mutex_unlock(&m_ioc_lock);
		break;
	case RTSOSDI_IOC_CONFIG_FRAME:
		err = __config_osdi_frame(rosdi, arg);
		break;
	case RTSOSDI_IOC_SET_BLK:
		err = __config_osdi_blk(rosdi, arg);
		break;
	case RTSOSDI_IOC_SET_ALL_BLK:
		err = __config_osdi_all_blk(rosdi, arg);
		break;
	case RTSOSDI_IOC_GET_BLK:
		err = __get_osdi_blk(rosdi, arg);
		break;
	case RTSOSDI_IOC_GET_ALL_BLK:
		err = __get_osdi_all_blk(rosdi, arg);
		break;
	case RTSOSDI_IOC_SET_COLOR_TABLE:
		mutex_lock(&m_ioc_lock);
		err = __set_osdi_color_table(rosdi, arg);
		mutex_unlock(&m_ioc_lock);
		break;
	case RTSOSDI_IOC_GET_COLOR_TABLE:
		mutex_lock(&m_ioc_lock);
		err = __get_osdi_color_table(rosdi, arg);
		mutex_unlock(&m_ioc_lock);
		break;
#ifdef RTS_OSDI_DEBUG_SRAM
	case RTSOSDI_IOC_SET_MEM:
		err = __change_osdi_blk_mem(rosdi, arg);
		break;
#endif
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "unknown[rtsosdi] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		err = -ENOTTY;
		break;
	}


	return err;
}

static long rtscam_osdi_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_osdi_do_ioctl);
}

static struct rtscam_ge_file_operations rtscam_osdi_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_osdi_open,
	.release	= rtscam_osdi_close,
	.ioctl		= rtscam_osdi_ioctl,
};

static int __create_device(struct rtscam_osdi *prosdi)
{

	struct rtscam_ge_device *gdev;
	struct rtscam_osdi *rosdi = NULL;
	char name[128];
	int ret;
	int i;

	if (!prosdi)
		return -EINVAL;

	for (i = 0; i < RTS_OSDI_NUM; i++) {
		rosdi = prosdi + i;
		if (rosdi->jdev)
			return 0;

		gdev = rtscam_ge_device_alloc();
		if (!gdev)
			return -ENOMEM;
		rosdi->jdev = gdev;

		sprintf(name, "%s%d", RTS_OSDI_DEV_NAME, i + 1);
		strlcpy(gdev->name, name, sizeof(gdev->name));
		gdev->parent = get_device(rosdi->dev);
		gdev->release = rtscam_ge_device_release;
		gdev->fops = &rtscam_osdi_fops;

		rtscam_ge_set_drvdata(gdev, rosdi);
		ret = rtscam_ge_register_device(gdev);
		if (ret)
			goto error;
	}

	return 0;
error:
	for (i = 0; i < RTS_OSDI_NUM; i++) {
		rosdi = prosdi + i;
		rtscam_ge_device_release(rosdi->jdev);
		rosdi->jdev = NULL;
	}
	return ret;
}

static void __remove_device(struct rtscam_osdi *prosdi)
{
	struct rtscam_ge_device *gdev;
	struct rtscam_osdi *rosdi = NULL;
	int i;

	for (i = 0; i < RTS_OSDI_NUM; i++) {
		rosdi = prosdi + i;
		if (!rosdi->jdev)
			return;

		gdev = rosdi->jdev;
		put_device(gdev->parent);
		rtscam_ge_unregister_device(gdev);
	}
}

static int rtscam_osdi_probe(struct platform_device *pdev)
{
	struct rtscam_osdi *rosdi;
	struct osdi_color_table *color_table = NULL;
	struct resource *res;
	void __iomem *base;
	int irq;
	int err = 0;
	int i, j;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}
	rosdi = devm_kzalloc(&pdev->dev,
		sizeof(*rosdi) * RTS_OSDI_NUM, GFP_KERNEL);
	if (rosdi == NULL) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera osd object\n");
		return -ENOMEM;
	}

	mutex_init(&m_ioc_lock);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap resource\n");
		err = PTR_ERR(base);
		return err;
	}

	err = __init_osdi_color_table(&color_table);
	if (err) {
		rtsprintk(RTS_TRACE_ERROR, "osdi color table init fail\n");
		return err;
	}

	for (i = 0; i < RTS_OSDI_NUM; i++) {
		struct rtscam_osdi *p = rosdi + i;

		atomic_set(&p->use_count, 0);
		irq = platform_get_irq(pdev, i);
		p->dev = get_device(&pdev->dev);
		p->hwregs = base;
		p->index = i;
		p->color_table = color_table;
		for (j = 0; j < RTS_OSDI_BLK_NUM; j++)
			p->all_blk_info.infos[j].index = j;
#ifndef RTS_OSDI_POLL_ENABLE
		if (irq > 0) {
			err = devm_request_irq(&pdev->dev, irq,
				rtscam_osdi_irq, IRQF_SHARED,
				RTS_OSDI_DRV_NAME, p);
			if (err) {
				rtsprintk(RTS_TRACE_ERROR,
					"osdi request irq fail\n");
				return err;
			}
		}
#endif
	}

	__create_device(rosdi);
	for (i = 0; i < RTS_OSDI_NUM; i++)
		__osdi_write_id(rosdi + i, OSDI_RESET, 1, 0);
	__write_osdi_color_table(rosdi);
	platform_set_drvdata(pdev, rosdi);

	return 0;
}

static int rtscam_osdi_remove(struct platform_device *pdev)
{
	int i;
	struct rtscam_osdi *rosdi = platform_get_drvdata(pdev);

	__release_osdi_color_table(rosdi);
	__remove_device(rosdi);

	for (i = 0; i < RTS_OSDI_NUM; i++) {
		struct rtscam_osdi *p = rosdi;

		p = rosdi + i;
		p->color_table = NULL;
		put_device(p->dev);
		p->dev = NULL;
	}

	return 0;
}

static const struct of_device_id rtscam_osdi_ids[] = {
	{ .compatible = "realtek,rts3917-osdi", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_osdi_driver = {
	.driver		= {
		.name	= RTS_OSDI_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_osdi_ids),
	},
	.probe		= rtscam_osdi_probe,
	.remove		= rtscam_osdi_remove,
};

module_platform_driver(rtscam_osdi_driver);

MODULE_DESCRIPTION("Realsil Osdi device driver");
MODULE_AUTHOR("Wil Shi <wil_shi@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.1");
MODULE_ALIAS("platform:" RTS_OSDI_DRV_NAME);
