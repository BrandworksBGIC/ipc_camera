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

#define TAG "MD"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "rts_camera.h"
#include "linux/rts_camera_md.h"

#define md_err(...) rtsprintk(RTS_TRACE_ERROR, __VA_ARGS__)
#define md_info(...) rtsprintk(RTS_TRACE_INFO, __VA_ARGS__)
#define md_debug(...) rtsprintk(RTS_TRACE_DEBUG, __VA_ARGS__)

#define RTS_ISP_MD_DEV_NAME "rtsmd"
#define RTS_ISP_MD_DRV_NAME "rts_md"

#define MD_FRAME_DONE_BIT 0
//#define MD_DEBUG			1

/* registers */
#define MD_REG_BIN_CTRL			0x003c
#define MD_REG_INT_EN			0x005c
#define MD_REG_INT_CLR			0x0060
#define MD_REG_IC_HISTBUF		0x0050
#define MD_REG_IC_RESULTBUF		0x006c

#define MD_BIN_CTRL_MASK_PAUSE		0x8
#define MD_BIN_CTRL_MASK_EN		0x10
#define MD_BIN_CTRL_MASK_RESET		0x20

#define MD_INT_EN_MASK_HIST		0x1
#define MD_INT_EN_MASK_RESULT		0x2

#define MD_INT_MASK_HIST		0x1
#define MD_INT_MASK_RESULT		0x2
#define MD_INT_MASK_IC_HISTBUF_OL	0x4
#define MD_INT_MASK_IC_RESULTBUF_OL	0x8
#define MD_INT_MASK_HISTDDR_OL		0x10
#define MD_INT_MASK_RESULTDDR_OL	0x20

#define MD_INT_MASK_MD_UL		0x40
#define MD_INT_MASK_HIST_OL		0x80
#define MD_INT_MASK_RESULT_OL		0x100

#define MD_INT_MASK_OL			0x3fc

enum {
	MD_IN_WIDTH = 0,
	MD_IN_HEIGHT,
	MD_IN_SCALE_X,
	MD_IN_SCALE_Y,
	MD_IN_START_X,
	MD_IN_START_Y,
	MD_OUT_WIDTH,
	MD_OUT_HEIGHT,
	MD_OUT_SCALE_X,
	MD_OUT_SCALE_Y,
	MD_OUT_START_X,
	MD_OUT_START_Y,
	MD_OUT_DS_THD,
	MD_ROI_START_X,
	MD_ROI_START_Y,
	MD_ROI_END_X,
	MD_ROI_END_Y,
	MD_NR_BINS,
	MD_BIN_BITS,
	MD_TRAIN,
	MD_RESET_BUF,
	//MD_DATA_REQ,
	MD_ENABLE_SKIP,
	MD_SKIP_FRAMES,
	MD_BACK_THD,
	MD_LEARN_THD,
	MD_FORGET_THD,
	MD_MOTION_THD,
	MD_TRAIN_FRAMES,
	MD_HIST_ADDR,
	MD_HIST_LEN,
	MD_HIST_AXI_ADDR,
	MD_HIST_AXI_LEN,
	MD_HIST_PIX_CNT,
	MD_INT_FLAG,
	MD_BURST_LEN_SEL_WR0,
	MD_RESULT_ADDR,
	MD_RESULT_LEN,
	MD_RESULT_AXI_ADDR,
	MD_RESULT_AXI_LEN,
	MD_RESULT_PIX_CNT,
	MD_BURST_LEN_SEL_WR1,
	MD_BURST_LEN_SEL_RD,
	MD_FLAG,
	MD_MOTION_PIX_CNT,
};

typedef struct {
	off_t offset;
	int lsb;
	int bits;
} reg_desc_t;

static reg_desc_t m_md_regs[] = {
	[MD_IN_WIDTH]		=	{0x0,	0,	10},
	[MD_IN_HEIGHT]		=	{0x4,	0,	9},
	[MD_IN_SCALE_X]		=	{0x8,	0,	4},
	[MD_IN_SCALE_Y]		=	{0x8,	8,	4},
	[MD_IN_START_X]		=	{0x0c,	0,	10},
	[MD_IN_START_Y]		=	{0x10,	0,	9},
	[MD_OUT_WIDTH]		=	{0x14,	0,	10},
	[MD_OUT_HEIGHT]		=	{0x18,	0,	9},
	[MD_OUT_SCALE_X]	=	{0x1c,	0,	3},
	[MD_OUT_SCALE_Y]	=	{0x1c,	8,	3},
	[MD_OUT_START_X]	=	{0x20,	0,	10},
	[MD_OUT_START_Y]	=	{0x24,	0,	9},
	[MD_OUT_DS_THD]		=	{0x28,	0,	4},
	[MD_ROI_START_X]	=	{0x2c,	0,	10},
	[MD_ROI_START_Y]	=	{0x30,	0,	9},
	[MD_ROI_END_X]		=	{0x34,	0,	10},
	[MD_ROI_END_Y]		=	{0x38,	0,	9},
	[MD_NR_BINS]		=	{0x3c,	0,	1},
	[MD_BIN_BITS]		=	{0x3c,	1,	1},
	[MD_TRAIN]		=	{0x84,	0,	1},
	[MD_RESET_BUF]		=	{0x3c,	5,	1},
	//[MD_DATA_REQ]		=	{0x3c,	6,	1},
	[MD_ENABLE_SKIP]	=	{0x3c,	7,	1},
	[MD_SKIP_FRAMES]	=	{0x3c,	8,	7},
	[MD_BACK_THD]		=	{0x40,	0,	4},
	[MD_LEARN_THD]		=	{0x40,	8,	8},
	[MD_FORGET_THD]		=	{0x40,	16,	8},
	[MD_MOTION_THD]		=	{0x44,	0,	17},
	[MD_TRAIN_FRAMES]	=	{0x48,	0,	5},
	[MD_HIST_ADDR]		=	{0x4c,	0,	32},
	[MD_HIST_LEN]		=	{0x54,	0,	24},
	[MD_HIST_AXI_ADDR]	=	{0x50,	0,	14},
	[MD_HIST_AXI_LEN]	=	{0x50,	16,	16},
	[MD_HIST_PIX_CNT]	=	{0x58,	0,	23},
	[MD_INT_FLAG]		=	{0x60,	0,	10},
	[MD_BURST_LEN_SEL_WR0]	=	{0x64,	0,	1},
	[MD_RESULT_ADDR]	=	{0x68,	0,	32},
	[MD_RESULT_LEN]		=	{0x70,	0,	24},
	[MD_RESULT_AXI_ADDR]	=	{0x6c,	0,	14},
	[MD_RESULT_AXI_LEN]	=	{0x6c,	16,	16},
	[MD_RESULT_PIX_CNT]	=	{0x74,	0,	23},
	[MD_BURST_LEN_SEL_WR1]	=	{0x78,	0,	1},
	[MD_BURST_LEN_SEL_RD]	=	{0x7C,	0,	1},
	[MD_FLAG]		=	{0x80,	0,	1},
	[MD_MOTION_PIX_CNT]	=	{0x80,	1,	17},
};

#define MD_OPS(func, args...)		\
	do {\
		if (func)\
			func(args);\
	} while (0)

struct rtscam_md_ops {
	int (*enable)(void *, int enable);
	int (*enable_interrupt)(void *, int enable);
	int (*enable_clk)(void *, int enable);
	int (*enable_pwr)(void *, int enable);
	int (*reset)(void *);
	void (*set_icfg)(void *);
	irqreturn_t (*irq)(int irq, void *data);
};

struct rts_hw_buffer_cfg {
	u32 start;
	u32 size;
};

struct rts_md_resource {
	unsigned long io_start;
	unsigned int io_size;
	void __iomem *reg_base;
	int irq;
};

struct rtscam_md {
	struct rtscam_ge_device *gdev;
	struct device *dev;

	struct rtscam_md_ops *ops;

	struct rts_md_resource res;

	struct rts_hw_buffer_cfg icfg[2];

	struct mutex lock;
	wait_queue_head_t wq;
	atomic_t use_count;

	unsigned long status;
	unsigned int intr_flag;

	struct completion skip_completion;
	u8 skip_completion_flag;
	u8 skip_completion_needed;
};

static inline u32 __md_read_reg(struct rtscam_md *md, off_t reg)
{
	return le32_to_cpu(ioread32(md->res.reg_base + reg));
}

static inline void __md_write_reg(struct rtscam_md *md, off_t reg, u32 val)
{
	iowrite32(cpu_to_le32(val), md->res.reg_base + reg);

#ifdef MD_DEBUG
	u32 read_val;

	read_val = __md_read_reg(md, reg);
	if (read_val != val)
		printk("**************************************\n");
	printk("++++++write: val:0x%08x\tread: val:0x%08x\treg:0x%08x\n",
		val, read_val, (u32)(md->res.reg_base + reg));
#endif
}

static int md_enable_interrupt(void *h, int enable)
{
	struct rtscam_md *md = h;

	if (!md)
		return 0;

	md_debug("enable intr %d\n", enable);

	if (enable)
		__md_write_reg(md, MD_REG_INT_EN, 0x3ff);
	else
		__md_write_reg(md, MD_REG_INT_EN, 0);

	md_debug("read intr 0x%08x\n", __md_read_reg(md, MD_REG_INT_EN));

	return 0;
}

static int md_enable(void *h, int enable)
{
	struct rtscam_md *md = h;
	u32 b;

	b = __md_read_reg(md, MD_REG_BIN_CTRL);

	if (enable) {
		b = (b | MD_BIN_CTRL_MASK_EN) & (~MD_BIN_CTRL_MASK_RESET);
		__md_write_reg(md, MD_REG_BIN_CTRL, b);
	} else {
		if (b & MD_BIN_CTRL_MASK_EN) {
			b = __md_read_reg(md, MD_REG_BIN_CTRL);
			__md_write_reg(md, MD_REG_BIN_CTRL,
					(b & (~MD_BIN_CTRL_MASK_EN)));
		}
	}

	return 0;
}

static void md_set_icfg(void *h)
{
	struct rtscam_md *md = h;

	if (!md)
		return;

	/*set icbuf addr*/
	__md_write_reg(md, MD_REG_IC_HISTBUF, ((md->icfg[0].size << 16) |
			md->icfg[0].start));
	__md_write_reg(md, MD_REG_IC_RESULTBUF, ((md->icfg[1].size << 16) |
			md->icfg[1].start));
}

static int md_enable_clk(void *h, int enable)
{
	return 0;
}

static int __check_md_busy(u32 val)
{
	if (val & MD_BIN_CTRL_MASK_PAUSE)
		return 1;
	else
		return 0;
}

static void __disable_md_busy(struct rtscam_md *md)
{
	u32 val;

	val = __md_read_reg(md, MD_REG_BIN_CTRL);

	if (!__check_md_busy(val))
		return;

	__md_write_reg(md, MD_REG_BIN_CTRL, val & (~MD_BIN_CTRL_MASK_PAUSE));
}

static void __enable_md_busy(struct rtscam_md *md)
{
	u32 val;

	val = __md_read_reg(md, MD_REG_BIN_CTRL);

	if (__check_md_busy(val))
		return;

	__md_write_reg(md, MD_REG_BIN_CTRL, (val | MD_BIN_CTRL_MASK_PAUSE));
}

static irqreturn_t md_irq(int irq, void *data)
{
	struct rtscam_md *md = data;
	u32 s;
	int flag_done = 0;

	if (!md)
		return 0;

	s = __md_read_reg(md, MD_REG_INT_CLR);
	if (!s)
		return IRQ_NONE;

	__md_write_reg(md, MD_REG_INT_CLR, s);

	/*do not handle hist(mtd0) frame end irq*/

	if (s & MD_INT_MASK_RESULT)
		flag_done++;

	if (s & MD_INT_MASK_OL) {
		md_err("buffer overflow [0x%x].\n", s);
		flag_done++;
	}

	if (flag_done) {
		/*pause the md module*/
		__enable_md_busy(md);

		md->intr_flag = s;
		set_bit(MD_FRAME_DONE_BIT, &md->status);
		wake_up_interruptible(&md->wq);
		rtscam_ge_kill_fasync(md->gdev, SIGIO, POLLIN);
		if (md->skip_completion_needed) {
			if (md->skip_completion_flag &&
				!completion_done(&md->skip_completion)) {
				md->skip_completion_flag = 0;
				complete(&md->skip_completion);
			}
		}
	}
	return IRQ_HANDLED;
}

static struct rtscam_md_ops m_md_ops = {
	.enable = md_enable,
	.enable_interrupt = md_enable_interrupt,
	.irq = md_irq,
	.set_icfg = md_set_icfg,
	.enable_clk = md_enable_clk,
};

static int rtscam_isp_md_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_md *md = rtscam_ge_get_drvdata(gdev);

	if (mutex_lock_interruptible(&md->lock))
		return -ERESTARTSYS;
	if (atomic_inc_return(&md->use_count) == 1) {
		MD_OPS(md->ops->enable_clk, md, 1);
		MD_OPS(md->ops->enable_pwr, md, 1);
		MD_OPS(md->ops->set_icfg, md);
		MD_OPS(md->ops->enable_interrupt, md, 1);
		MD_OPS(md->ops->enable_clk, md, 0);
	}
	mutex_unlock(&md->lock);

	filp->private_data = md;
	return 0;
}

static int rtscam_isp_md_close(struct file *filp)
{
	struct rtscam_md *md = filp->private_data;

	if (mutex_lock_interruptible(&md->lock))
		return -ERESTARTSYS;

	if (atomic_dec_return(&md->use_count) == 0) {
		MD_OPS(md->ops->enable_clk, md, 1);
		MD_OPS(md->ops->enable_pwr, md, 0);
		MD_OPS(md->ops->enable_interrupt, md, 0);
		MD_OPS(md->ops->enable_clk, md, 0);
	}

	mutex_unlock(&md->lock);

	filp->private_data = NULL;

	return 0;
}

static void rtscam_md_write(struct rtscam_md *md, int id, unsigned int val)
{
	unsigned int v;
	unsigned int mask;
	reg_desc_t *des = &m_md_regs[id];

	mask = (unsigned int)(((1LL << des->bits) - 1) << des->lsb);
	v = __md_read_reg(md, des->offset);
	v &= (~mask);

	v |= ((val << des->lsb) & mask);
	__md_write_reg(md, des->offset, v);
}

static int rtscam_md_read(struct rtscam_md *md, int id)
{
	unsigned int v;
	reg_desc_t *des = &m_md_regs[id];

	v = __md_read_reg(md, des->offset);
	return ((v >> des->lsb) & (unsigned int)((1LL << des->bits) - 1));
}

static void __md_set_extra_attr(struct rtscam_md *md,
			struct rtscam_md_extra_attr *attr)
{
	if (!md || !attr)
		return;

	rtscam_md_write(md, MD_BURST_LEN_SEL_RD,
				attr->burst_length.isp_read);
	rtscam_md_write(md, MD_BURST_LEN_SEL_WR0,
				attr->burst_length.hist_write);
	rtscam_md_write(md, MD_BURST_LEN_SEL_WR1,
				attr->burst_length.res_write);


	rtscam_md_write(md, MD_ENABLE_SKIP, attr->skip.enable);
	rtscam_md_write(md, MD_SKIP_FRAMES, attr->skip.frames);
}

static void __md_get_extra_attr(struct rtscam_md *md,
			struct rtscam_md_extra_attr *attr)
{
	if (!md || !attr)
		return;

	attr->burst_length.isp_read =
		rtscam_md_read(md, MD_BURST_LEN_SEL_RD);
	attr->burst_length.hist_write =
		rtscam_md_read(md, MD_BURST_LEN_SEL_WR0);
	attr->burst_length.res_write =
		rtscam_md_read(md, MD_BURST_LEN_SEL_WR1);

	attr->skip.enable = rtscam_md_read(md, MD_ENABLE_SKIP);
	attr->skip.frames = rtscam_md_read(md, MD_SKIP_FRAMES);
}

static void __md_set_attr(struct rtscam_md *md,
			struct rtscam_md_attr *attr)
{
	if (!md || !attr)
		return;

	rtscam_md_write(md, MD_IN_WIDTH, attr->md_in.w);
	rtscam_md_write(md, MD_IN_HEIGHT, attr->md_in.h);
	rtscam_md_write(md, MD_IN_START_X, attr->md_in.x);
	rtscam_md_write(md, MD_IN_START_Y, attr->md_in.y);
	rtscam_md_write(md, MD_IN_SCALE_X, attr->md_in.scale_x);
	rtscam_md_write(md, MD_IN_SCALE_Y, attr->md_in.scale_y);

	rtscam_md_write(md, MD_OUT_WIDTH, attr->md_out.w);
	rtscam_md_write(md, MD_OUT_HEIGHT, attr->md_out.h);
	rtscam_md_write(md, MD_OUT_START_X, attr->md_out.x);
	rtscam_md_write(md, MD_OUT_START_Y, attr->md_out.y);
	rtscam_md_write(md, MD_OUT_SCALE_X, attr->md_out.scale_x);
	rtscam_md_write(md, MD_OUT_SCALE_Y, attr->md_out.scale_y);

	rtscam_md_write(md, MD_ROI_END_X, attr->roi.x + attr->roi.w);
	rtscam_md_write(md, MD_ROI_END_Y, attr->roi.y + attr->roi.h);
	rtscam_md_write(md, MD_ROI_START_X, attr->roi.x);
	rtscam_md_write(md, MD_ROI_START_Y, attr->roi.y);

	rtscam_md_write(md, MD_NR_BINS, attr->nr_bins);
	rtscam_md_write(md, MD_BIN_BITS, attr->bin_bits);
	rtscam_md_write(md, MD_TRAIN_FRAMES, attr->train_frames);

	rtscam_md_write(md, MD_BACK_THD, attr->thd.back);
	rtscam_md_write(md, MD_LEARN_THD, attr->thd.learn);
	rtscam_md_write(md, MD_FORGET_THD, attr->thd.forget);
	rtscam_md_write(md, MD_OUT_DS_THD, attr->thd.ds);
	rtscam_md_write(md, MD_MOTION_THD, attr->thd.motion);
}

static void __md_get_attr(struct rtscam_md *md,
			struct rtscam_md_attr *attr)
{
	if (!md || !attr)
		return;

	attr->md_in.w = rtscam_md_read(md, MD_IN_WIDTH);
	attr->md_in.h = rtscam_md_read(md, MD_IN_HEIGHT);
	attr->md_in.x = rtscam_md_read(md, MD_IN_START_X);
	attr->md_in.y = rtscam_md_read(md, MD_IN_START_Y);
	attr->md_in.scale_x = rtscam_md_read(md, MD_IN_SCALE_X);
	attr->md_in.scale_y = rtscam_md_read(md, MD_IN_SCALE_Y);

	attr->md_out.w = rtscam_md_read(md, MD_OUT_WIDTH);
	attr->md_out.h = rtscam_md_read(md, MD_OUT_HEIGHT);
	attr->md_out.x = rtscam_md_read(md, MD_OUT_START_X);
	attr->md_out.y = rtscam_md_read(md, MD_OUT_START_Y);
	attr->md_out.scale_x = rtscam_md_read(md, MD_OUT_SCALE_X);
	attr->md_out.scale_y = rtscam_md_read(md, MD_OUT_SCALE_Y);

	attr->roi.x = rtscam_md_read(md, MD_ROI_START_X);
	attr->roi.y = rtscam_md_read(md, MD_ROI_START_Y);
	attr->roi.w = rtscam_md_read(md, MD_ROI_END_X) - attr->roi.x;
	attr->roi.h  = rtscam_md_read(md, MD_ROI_END_Y) - attr->roi.y;

	attr->nr_bins = rtscam_md_read(md, MD_NR_BINS);
	attr->bin_bits = rtscam_md_read(md, MD_BIN_BITS);
	attr->train_frames = rtscam_md_read(md, MD_TRAIN_FRAMES);

	attr->thd.back = rtscam_md_read(md, MD_BACK_THD);
	attr->thd.learn = rtscam_md_read(md, MD_LEARN_THD);
	attr->thd.forget = rtscam_md_read(md, MD_FORGET_THD);
	attr->thd.ds = rtscam_md_read(md, MD_OUT_DS_THD);
	attr->thd.motion = rtscam_md_read(md, MD_MOTION_THD);
}

static void __md_trigger_train(struct rtscam_md *md)
{
	rtscam_md_write(md, MD_TRAIN, 1);
}

static void __md_set_dma_buffer(struct rtscam_md *md,
				struct rtscam_md_buffer *p)
{
	if (!md || !p)
		return;

	rtscam_md_write(md, MD_HIST_ADDR, p->hist_addr);
	rtscam_md_write(md, MD_HIST_LEN, p->hist_length);

	rtscam_md_write(md, MD_RESULT_ADDR, p->res_addr);
	rtscam_md_write(md, MD_RESULT_LEN, p->res_length);

	md_debug("md set dma buffer: 0x%x %d, res 0x%x %d\n",
		p->hist_addr, p->hist_length, p->res_addr, p->res_length);
}

static void __md_set_axi_buffer(struct rtscam_md *md,
				struct rtscam_md_buffer *p)
{
	if (!md || !p)
		return;

	rtscam_md_write(md, MD_HIST_AXI_ADDR, p->hist_addr);
	rtscam_md_write(md, MD_HIST_AXI_LEN, p->hist_length);

	rtscam_md_write(md, MD_RESULT_AXI_ADDR, p->res_addr);
	rtscam_md_write(md, MD_RESULT_AXI_LEN, p->res_length);

	md_debug("md set axi buffer: 0x%x %d, res 0x%x %d\n",
		p->hist_addr, p->hist_length, p->res_addr, p->res_length);
}

static void __md_get_result(struct rtscam_md *md,
				struct rtscam_md_result *p)
{
	if (!md || !p)
		return;

	p->motion_flag = rtscam_md_read(md, MD_FLAG);
	p->motion_count = rtscam_md_read(md, MD_MOTION_PIX_CNT);
}

static long rtscam_isp_md_do_ioctl(struct file *filp, unsigned int cmd,
				void *arg)
{
	struct rtscam_md *md = filp->private_data;
	int ret = 0;

	if (!md)
		return -EINVAL;

	md_debug("ioctl cmd 0x%08x, '%c' %d.\n",
			cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != RTSMD_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) > RTSMD_IOC_MAXNR)
		return -ENOTTY;

	if (mutex_lock_interruptible(&md->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case RTSMD_IOC_ENABLE:
		MD_OPS(md->ops->enable_clk, md, 1);
		MD_OPS(md->ops->enable, md, 1);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_DISABLE:

		MD_OPS(md->ops->enable_clk, md, 1);

		if (md->skip_completion_needed) {
			if (rtscam_md_read(md, MD_ENABLE_SKIP)) {
				__disable_md_busy(md);
				rtscam_md_write(md, MD_ENABLE_SKIP, 0);
				init_completion(&md->skip_completion);
				md->skip_completion_flag = 1;
				ret = wait_for_completion_timeout(
					&md->skip_completion, 3000 * HZ / 1000);
				if (ret <= 0)
					md_err("md2 wait for skip fail\n");
				ret = ret > 0 ? 0 : -EINVAL;
			}
		}

		MD_OPS(md->ops->enable, md, 0);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_DONE:
		{
			MD_OPS(md->ops->enable_clk, md, 1);
			md->intr_flag = 0;
			clear_bit(MD_FRAME_DONE_BIT, &md->status);
			__disable_md_busy(md);
			MD_OPS(md->ops->enable_clk, md, 0);
		}
		break;
	case RTSMD_IOC_STATUS:
		*(unsigned int *) arg = md->intr_flag;
		break;
	case RTSMD_IOC_SET_EXTRA_ATTR:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_set_extra_attr(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_GET_EXTRA_ATTR:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_get_extra_attr(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_SET_ATTR:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_set_attr(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_GET_ATTR:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_get_attr(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_TRIGGER_TRAIN:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_trigger_train(md);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_SET_DMA_ADDR:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_set_dma_buffer(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_SET_AXI_BUFFER:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_set_axi_buffer(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	case RTSMD_IOC_GET_RESULT:
		MD_OPS(md->ops->enable_clk, md, 1);
		__md_get_result(md, arg);
		MD_OPS(md->ops->enable_clk, md, 0);
		break;
	default:
		md_err("unrecognized cmd 0x%08x, '%c' %d\n",
				cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&md->lock);

	return ret;
}


static long rtscam_isp_md_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_isp_md_do_ioctl);
}

static unsigned int rtscam_isp_md_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	struct rtscam_md *md = filp->private_data;
	u32 mask = 0;
	unsigned long req_events = poll_requested_events(wait);

	if (!(req_events & (POLLIN | POLLRDNORM)))
		return mask;

	if (!test_bit(MD_FRAME_DONE_BIT, &md->status))
		poll_wait(filp, &md->wq, wait);
	else
		goto out;

	if (test_bit(MD_FRAME_DONE_BIT, &md->status))
		goto out;
	else
		return 0;

out:
	if (md->intr_flag & MD_INT_MASK_OL)
		mask |= POLLERR;
	if (md->intr_flag & MD_INT_MASK_RESULT)
		mask |= (POLLIN | POLLRDNORM);
	return mask;
}

static struct rtscam_ge_file_operations m_ge_ops = {
	.owner = THIS_MODULE,
	.open = rtscam_isp_md_open,
	.release = rtscam_isp_md_close,
	.ioctl = rtscam_isp_md_ioctl,
	.poll = rtscam_isp_md_poll,
};

static int __create_device(struct rtscam_md *md)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (md->gdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_ISP_MD_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(md->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &m_ge_ops;

	rtscam_ge_set_drvdata(gdev, md);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	md->gdev = gdev;
	return 0;
}

static void __remove_device(struct rtscam_md *md)
{
	struct rtscam_ge_device *gdev;

	if (!md || !md->gdev)
		return;

	gdev = md->gdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
}

static int __parse_of_cfg(struct device_node *pp,
			struct rts_hw_buffer_cfg cfg[2])
{
	int ret;
	struct device_node *pn0, *pn1;
	u32 arr0[2];
	u32 arr1[2];

	pn0 = of_parse_phandle(pp, "md_cfg", 0);
	pn1 = of_parse_phandle(pp, "md_cfg", 1);
	if (!pn0 || !pn1) {
		md_err("failed to get handle md_cfg.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(pn0, "reg", arr0, 2);
	ret |= of_property_read_u32_array(pn1, "reg", arr1, 2);
	if (!ret) {
		cfg[0].start = (arr0[0]);
		cfg[0].size = (arr0[1]);
		cfg[1].start = (arr1[0]);
		cfg[1].size = (arr1[1]);
	}

	md_debug("md icbuf 1 <0x%x 0x%x>\n", cfg[0].start, cfg[0].size);
	md_debug("md icbuf 2 <0x%x 0x%x>\n", cfg[1].start, cfg[1].size);

	return ret;
}


static int rtscam_isp_md_probe(struct platform_device *pdev)
{
	struct rtscam_md *md = NULL;
	int ret = 0;
	struct resource *res;
	int irq;

	md_info("%s\n", __func__);


	md = devm_kzalloc(&pdev->dev, sizeof(*md), GFP_KERNEL);
	if (!md) {
		md_err("alloc mem failed.\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || irq < 0) {
		md_err("get resource failed");
		if (irq < 0)
			md_err(" irq[%d]", irq);
		md_err("\n");
		ret = -ENODEV;
		goto failed;
	}

	ret = __parse_of_cfg(pdev->dev.of_node, md->icfg);
	if (ret) {
		md_err("parse err [%d].\n", ret);
		goto failed;
	}
	md->res.io_start = res->start;
	md->res.io_size = resource_size(res);
	md->res.reg_base = devm_ioremap(&pdev->dev, md->res.io_start,
					md->res.io_size);
	md->res.irq = irq;

	if (!md->res.reg_base) {
		md_err("ioremap failed for resource 0x%lx\n", md->res.io_start);
		ret = -ENOMEM;
	}

	md_debug("irq %d\n", md->res.irq);
	md_debug("io_start 0x%lx\n", md->res.io_start);
	md_debug("io_size 0x%x\n", md->res.io_size);
	md_debug("reg_base 0x%px\n", md->res.reg_base);

	if (of_device_is_compatible(pdev->dev.of_node, "realtek,rts3917-md"))
		md->skip_completion_needed = 1;

	md->dev = get_device(&pdev->dev);
	md->ops = &m_md_ops;
	atomic_set(&md->use_count, 0);
	mutex_init(&md->lock);
	init_waitqueue_head(&md->wq);

	ret = devm_request_irq(md->dev, md->res.irq, md->ops->irq,
			IRQF_SHARED, RTS_ISP_MD_DEV_NAME, md);
	if (ret) {
		md_err("request irq failed [%d].\n", ret);
		goto failed;
	}

	ret = __create_device(md);
	if (ret) {
		md_err("create device failed [%d].\n", ret);
		goto failed;
	}

	platform_set_drvdata(pdev, md);

	md_debug("md init success\n");
	return 0;

failed:
	if (md && md->dev) {
		put_device(md->dev);
		md->dev = NULL;
	}
	return ret;
}

static int rtscam_isp_md_remove(struct platform_device *pdev)
{
	struct rtscam_md *md = platform_get_drvdata(pdev);

	__remove_device(md);
	put_device(md->dev);
	md->dev = NULL;

	return 0;
}

static const struct of_device_id rtscam_isp_md_ids[] = {
	{ .compatible = "realtek,rts3917-md", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_isp_md_driver = {
	.driver = {
		.name = RTS_ISP_MD_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_isp_md_ids),
	},

	.probe = rtscam_isp_md_probe,
	.remove = rtscam_isp_md_remove,
};


module_platform_driver(rtscam_isp_md_driver);

MODULE_DESCRIPTION("Realsil isp md device driver");
MODULE_AUTHOR("Anakin Wang <anakin_wang@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.0");
MODULE_ALIAS("platform:" RTS_ISP_MD_DRV_NAME);
