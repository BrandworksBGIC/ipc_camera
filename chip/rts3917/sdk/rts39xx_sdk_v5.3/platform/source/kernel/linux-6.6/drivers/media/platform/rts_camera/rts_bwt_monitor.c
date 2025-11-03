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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include "rts_camera.h"
#include "linux/rts_bwt_monitor.h"

#define RTS_BWT_DRV_NAME		"rts_bwt"
#define RTS_BWT_DEV_NAME		"rtsbwt"

#define READ_BASE			0
#define WRITE_BASE			0x00000040

#define ADDR_CONFIG		0
#define JUMP_CONFIG		0x00000004
#define PACKET_CONFIG		0x00000008
#define TRANSFER_NUM		0x0000000c
#define START_CONTROL		0x00000010
#define STOP_CONTROL		0x00000014
#define CYCLE_COUNTER		0x00000018
#define DATA_COUNTER		0x0000001c
#define TRANSFER_COUNTER	0x00000020
#define CUR_ADDR		0x00000024
#define ADDR_CONFIG_2		0x00000028
#define RANDOM_SEED		0x0000002c

#define RTS_BWT_INTERRUPT_EN		0x00000080
#define RTS_BWT_INTERRUPT_FLAG		0x00000084

#define GE_AXI_USE			0x00000088

struct rtscam_bwt_monitor {
	struct device *dev;

	struct rtscam_ge_device *bwt_monitor;

	struct mutex lock;
	atomic_t open_cnt;

	unsigned long status;
	wait_queue_head_t alarm_wq;

	struct clk *clk;
	void __iomem *hwregs;
};

static int rtscam_bwt_read_reg(struct rtscam_bwt_monitor *rtsbwt, off_t reg)
{
	return le32_to_cpu(ioread32(rtsbwt->hwregs + reg));
}

static void rtscam_bwt_write_reg(struct rtscam_bwt_monitor *rtsbwt,
				u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), rtsbwt->hwregs + reg);
}

static int rtscam_bwt_enable_ctrl(struct rtscam_bwt_monitor *rtsbwt, int enable)
{
	u32 val = 0;

	if (!rtsbwt)
		return -EINVAL;

	if (enable)
		val = 1;

	rtscam_bwt_write_reg(rtsbwt, val, GE_AXI_USE);

	return 0;
}

static irqreturn_t rtscam_bwt_irq(int irq, void *data)
{
	struct rtscam_bwt_monitor *rtsbwt = data;
	u32 status;
	u32 mask;
	const off_t reg = RTS_BWT_INTERRUPT_FLAG;

	status = rtscam_bwt_read_reg(rtsbwt, reg);
	if (!status)
		return IRQ_NONE;

	mask = 0x18;
	if (status & mask) {
		rtscam_bwt_write_reg(rtsbwt, mask, reg);
		rtsprintk(RTS_TRACE_ERROR,
			"bwt: counter overflow(status:0x%x)\n",
			status);
		rtsbwt->status = 2;
		wake_up_interruptible(&rtsbwt->alarm_wq);
		return IRQ_HANDLED;
	}

	mask = 0x20;
	if (status & mask) {
		rtscam_bwt_write_reg(rtsbwt, mask, reg);
		rtsprintk(RTS_TRACE_DEBUG,
				"bwt:transfer finish\n");
		rtsbwt->status = 1;
		wake_up_interruptible(&rtsbwt->alarm_wq);
		return IRQ_HANDLED;
	}

	mask = 0xffffffc7;
	if (status & mask) {
		rtscam_bwt_write_reg(rtsbwt, mask, reg);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

static int __get_counter(struct rtscam_bwt_monitor *rtsbwt,
			struct rtsbwt_counter *cnt)
{
	off_t reg_base = READ_BASE;

	if (!rtsbwt || !cnt)
		return -EINVAL;

	if (cnt->write)
		reg_base = WRITE_BASE;

	cnt->cycle = rtscam_bwt_read_reg(rtsbwt, reg_base + CYCLE_COUNTER);
	cnt->data = rtscam_bwt_read_reg(rtsbwt, reg_base + DATA_COUNTER);
	cnt->transfer = rtscam_bwt_read_reg(rtsbwt,
					reg_base + TRANSFER_COUNTER);

	return 0;
}

static int __set_cfg(struct rtscam_bwt_monitor *rtsbwt,
			struct rtsbwt_cfg *cfg)
{
	off_t reg_base = READ_BASE;
	u32 val;

	if (!rtsbwt || !cfg)
		return -EINVAL;

	if (cfg->write)
		reg_base = WRITE_BASE;

	rtscam_bwt_write_reg(rtsbwt, cfg->start_addr,
				reg_base + ADDR_CONFIG);
	rtscam_bwt_write_reg(rtsbwt, cfg->jump_addr,
					reg_base + JUMP_CONFIG);

	val = cfg->packet_num[0] | (cfg->packet_num[1] << 8) |
		(cfg->packet_num[2] << 16) | (cfg->packet_num[3] << 24);

	rtscam_bwt_write_reg(rtsbwt, val, reg_base + PACKET_CONFIG);
	rtscam_bwt_write_reg(rtsbwt, cfg->trans_num, reg_base + TRANSFER_NUM);
	rtscam_bwt_write_reg(rtsbwt, cfg->end_addr,
				reg_base + ADDR_CONFIG_2);
	rtscam_bwt_write_reg(rtsbwt, 0xf, reg_base + RANDOM_SEED);
	return 0;
}

static int __stop_transfer(struct rtscam_bwt_monitor *rtsbwt,
			u8 type, u8 write)
{
	off_t reg_base = READ_BASE;

	if (type > 2)
		return -EINVAL;

	if (write)
		reg_base = WRITE_BASE;

	rtscam_bwt_write_reg(rtsbwt, 1 << type, reg_base + STOP_CONTROL);

	return 0;
}

static int __start_transfer(struct rtscam_bwt_monitor *rtsbwt, u8 write)
{
	off_t reg_base = READ_BASE;

	if (write)
		reg_base = WRITE_BASE;

	rtscam_bwt_write_reg(rtsbwt, 1, reg_base + START_CONTROL);

	return 0;
}

static int __wait_transfer_done(struct rtscam_bwt_monitor *rtsbwt, u8 write,
				unsigned long timeout)
{
	off_t reg_base = READ_BASE;
	u32 val;
	unsigned long loop = timeout / 10;

	if (write)
		reg_base = WRITE_BASE;

	while (loop--) {
		val = rtscam_bwt_read_reg(rtsbwt, reg_base + START_CONTROL);
		if (!(val & 0x1))
			break;
		usleep_range(9900, 10000);
	}

	if (val & 0x1)
		return -EBUSY;

	return 0;
}

static long rtscam_bwt_do_ioctl(struct file *filp, unsigned int cmd,
				 void *arg)
{
	struct rtscam_bwt_monitor *rtsbwt = filp->private_data;
	int err = 0;

	if (_IOC_TYPE(cmd) != RTSBWT_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RTSBWT_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RTSBWT_IOC_GET_COUNTER:
		mutex_lock(&rtsbwt->lock);
		err = __get_counter(rtsbwt, arg);
		mutex_unlock(&rtsbwt->lock);
		break;
	case RTSBWT_IOC_SET_CFG:
		mutex_lock(&rtsbwt->lock);
		err = __set_cfg(rtsbwt, arg);
		mutex_unlock(&rtsbwt->lock);
		break;
	case RTSBWT_IOC_STOP_WRITE:
		mutex_lock(&rtsbwt->lock);
		err = __stop_transfer(rtsbwt, *(u8 *)arg, 1);
		mutex_unlock(&rtsbwt->lock);
		break;
	case RTSBWT_IOC_STOP_READ:
		mutex_lock(&rtsbwt->lock);
		err = __stop_transfer(rtsbwt, *(u8 *)arg, 0);
		mutex_unlock(&rtsbwt->lock);
		break;
	case RTSBWT_IOC_START:
		mutex_lock(&rtsbwt->lock);
		err = __start_transfer(rtsbwt, *(u8 *)arg);
		mutex_unlock(&rtsbwt->lock);
		break;
	case RTSBWT_IOC_WAIT_WRITE_DONE:
		err = __wait_transfer_done(rtsbwt, 1, *(unsigned long *)arg);
		break;
	case RTSBWT_IOC_WAIT_READ_DONE:
		err = __wait_transfer_done(rtsbwt, 0, *(unsigned long *)arg);
		break;
	case RTSBWT_IOC_WAIT_INTERRUPT:
		{
			unsigned long timeout = *(unsigned long *)arg;

			err = wait_event_interruptible_timeout(
				rtsbwt->alarm_wq,
				rtsbwt->status != 0,
				msecs_to_jiffies(timeout));
			if (!err) {
				err = -ETIME;
				break;
			}
			err = rtsbwt->status;
			rtsbwt->status = 0;
			break;
		}
	case RTSBWT_IOC_DRAM_CLK_RATE:
		{
			struct clk *clk;

			clk = clk_get(NULL, "dram_ck");
			if (IS_ERR(clk)) {
				err = -EINVAL;
				break;
			}

			*(unsigned long *)arg = clk_get_rate(clk);
			clk_put(clk);
			break;
		}
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "unknown[rtsbwt] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		err = -ENOTTY;
		break;
	}

	return err;
}

static long rtscam_bwt_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_bwt_do_ioctl);
}

static int rtscam_bwt_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_bwt_monitor *rtsbwt = rtscam_ge_get_drvdata(gdev);

	mutex_lock(&rtsbwt->lock);
	if (atomic_inc_return(&rtsbwt->open_cnt) == 1)
		rtscam_bwt_enable_ctrl(rtsbwt, 1);
	mutex_unlock(&rtsbwt->lock);

	filp->private_data = rtsbwt;

	return 0;
}

static int rtscam_bwt_close(struct file *filp)
{
	struct rtscam_bwt_monitor *rtsbwt = filp->private_data;

	mutex_lock(&rtsbwt->lock);
	if (atomic_dec_return(&rtsbwt->open_cnt) == 0)
		rtscam_bwt_enable_ctrl(rtsbwt, 0);
	mutex_unlock(&rtsbwt->lock);

	filp->private_data = NULL;

	return 0;
}

static struct rtscam_ge_file_operations rtscam_bwt_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_bwt_open,
	.release	= rtscam_bwt_close,
	.ioctl		= rtscam_bwt_ioctl,
};

static int __create_device(struct rtscam_bwt_monitor *rtsbwt)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (rtsbwt->bwt_monitor)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_BWT_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(rtsbwt->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_bwt_fops;

	rtscam_ge_set_drvdata(gdev, rtsbwt);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rtsbwt->bwt_monitor = gdev;

	return 0;
}

static int rtscam_bwt_probe(struct platform_device *pdev)
{
	struct rtscam_bwt_monitor *rtsbwt;
	struct resource *res;
	void __iomem *base;
	int err = 0;
	int irq = 0;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	rtsbwt = devm_kzalloc(&pdev->dev, sizeof(*rtsbwt), GFP_KERNEL);
	if (!rtsbwt) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rtsbwt object\n");
		return -ENOMEM;
	}
	rtsbwt->dev = get_device(&pdev->dev);

	atomic_set(&rtsbwt->open_cnt, 0);
	mutex_init(&rtsbwt->lock);
	init_waitqueue_head(&rtsbwt->alarm_wq);
	rtsbwt->status = 0;

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR,
			"Couldn't ioremap rtsbwt resource\n");
		err = PTR_ERR(base);
		goto error;
	}
	rtsbwt->hwregs = base;

	if (irq > 0) {
		err = devm_request_irq(rtsbwt->dev, irq, rtscam_bwt_irq,
				       IRQF_SHARED, RTS_BWT_DRV_NAME, rtsbwt);
		if (err) {
			rtsprintk(RTS_TRACE_ERROR,
				"rtsbwt: request irq fail\n");
			goto error;
		}
	}

	err = __create_device(rtsbwt);
	if (err) {
		rtsprintk(RTS_TRACE_ERROR, "create rtsbwt device fail\n");
		goto error;
	}

	rtsbwt->clk = devm_clk_get(rtsbwt->dev, "bwt_ck");
	if (IS_ERR(rtsbwt->clk)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get bwt clk\n");
		err = PTR_ERR(rtsbwt->clk);
		goto error;
	}
	clk_prepare_enable(rtsbwt->clk);
	platform_set_drvdata(pdev, rtsbwt);
	return 0;
error:
	if (rtsbwt && rtsbwt->dev) {
		put_device(rtsbwt->dev);
		rtsbwt->dev = NULL;
	}
	return err;
}

static void __remove_device(struct rtscam_bwt_monitor *rtsbwt)
{
	struct rtscam_ge_device *gdev;

	if (!rtsbwt->bwt_monitor)
		return;

	gdev = rtsbwt->bwt_monitor;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
	rtsbwt->bwt_monitor = NULL;
}

static int rtscam_bwt_remove(struct platform_device *pdev)
{
	struct rtscam_bwt_monitor *rtsbwt = platform_get_drvdata(pdev);

	__remove_device(rtsbwt);
	clk_disable_unprepare(rtsbwt->clk);
	put_device(rtsbwt->dev);
	rtsbwt->dev = NULL;

	return 0;
}

static const struct of_device_id rtscam_bwt_ids[] = {
	{ .compatible = "realtek,rts3917-bwt", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_bwt_driver = {
	.driver		= {
		.name	= RTS_BWT_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_bwt_ids),
	},
	.probe		= rtscam_bwt_probe,
	.remove		= rtscam_bwt_remove,
};

module_platform_driver(rtscam_bwt_driver);

MODULE_DESCRIPTION("Realsil bwt device driver");
MODULE_AUTHOR("Mona Mao <mona_mao@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.1");
MODULE_ALIAS("platform:" RTS_BWT_DRV_NAME);
