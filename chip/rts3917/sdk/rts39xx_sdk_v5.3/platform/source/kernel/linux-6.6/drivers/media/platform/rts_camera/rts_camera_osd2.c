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

#define TAG	"OSD2"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "linux/rts_camera_osd2.h"
#include "rts_camera.h"
#include "rts_hw_id.h"

#define RTS_OSD2_DRV_NAME		"rts_osd2"
#define RTS_OSD2_DEV_NAME		"rtsosd2"

#define OSD2_FRAME_DONE_BIT		0

#define RTS_REG_INT_EN_OSD2_TO_HOST		0x0000003c
#define RTS_REG_INT_FLAG_OSD2_TO_HOST		0x00000040
#define RTS_REG_OSD2_ISP_BUF_CONFIG		0x0000005c
#define RTS_REG_OSD2_BUSIF_ENABLE		0x00000064
#define RTS_REG_OSD2_BUSIF_RST			0x00000068

struct rtscam_osd2 {
	struct device *dev;
	void __iomem *hwregs;
	unsigned long iobaseaddr;
	unsigned int iosize;

	int irq_enable;

	struct rtscam_ge_device *jdev;
	atomic_t use_count;

	struct mutex lock;

	struct reset_control *reset;

	unsigned long status;
	wait_queue_head_t alarm_wq;

	unsigned int ic_type;
	struct rtscam_region config;
};

static int rtscam_osd2_enable_clk(struct rtscam_osd2 *rosd2, int enable)
{
	return 0;
}

static int rtscam_osd2_read_reg(struct rtscam_osd2 *rosd2, off_t reg)
{
	return le32_to_cpu(ioread32(rosd2->hwregs + reg));
}

static void rtscam_osd2_write_reg(struct rtscam_osd2 *rosd2,
				  u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), rosd2->hwregs + reg);
}

static int rtscam_osd2_config_isp_buffer(struct rtscam_osd2 *rosd2)
{
	rtscam_osd2_write_reg(rosd2, rosd2->config.base >> 4,
			      RTS_REG_OSD2_ISP_BUF_CONFIG);

	return 0;
}

static int rtscam_osd2_enable_bus(struct rtscam_osd2 *rosd2)
{
	if (rosd2->ic_type < TYPE_RTS3915)
		return 0;

	rtscam_osd2_write_reg(rosd2, 0, RTS_REG_OSD2_BUSIF_RST);
	rtscam_osd2_write_reg(rosd2, 0x1, RTS_REG_OSD2_BUSIF_RST);
	rtscam_osd2_write_reg(rosd2, 0, RTS_REG_OSD2_BUSIF_RST);
	rtscam_osd2_write_reg(rosd2, 0x1, RTS_REG_OSD2_BUSIF_ENABLE);

	return 0;
}

static int rtscam_osd2_disable_bus(struct rtscam_osd2 *rosd2)
{
	if (rosd2->ic_type < TYPE_RTS3915)
		return 0;

	rtscam_osd2_write_reg(rosd2, 0, RTS_REG_OSD2_BUSIF_ENABLE);

	return 0;
}

static int rtscam_osd2_enable_interrupt(struct rtscam_osd2 *rosd2,
					int enable)
{
	u32 int_en;
	u32 int_f = 0xffffffff;

	if (enable)
		int_en = 0xffffff03;
	else
		int_en = 0;

	rtscam_osd2_write_reg(rosd2, int_en, RTS_REG_INT_EN_OSD2_TO_HOST);
	rtscam_osd2_write_reg(rosd2, int_f, RTS_REG_INT_FLAG_OSD2_TO_HOST);
	return 0;
}

static int rtscam_osd2_reset(struct rtscam_osd2 *rosd2)
{
	int ret;

	if (!rosd2->reset)
		return 0;

	ret = reset_control_reset(rosd2->reset);
	if (ret)
		return ret;

	udelay(1);
	return 0;
}

static irqreturn_t rtscam_osd2_irq(int irq, void *data)
{

	struct rtscam_osd2 *rosd2 = data;
	u32 status;
	u32 mask;

	const off_t reg = RTS_REG_INT_FLAG_OSD2_TO_HOST;

	status = rtscam_osd2_read_reg(rosd2, reg);

	if (!status)
		return IRQ_NONE;

	/*encode finish*/
	mask = 0x1;
	if (status & mask) {
		rtscam_osd2_write_reg(rosd2, mask, reg);
		set_bit(OSD2_FRAME_DONE_BIT, &rosd2->status);
		wake_up_interruptible(&rosd2->alarm_wq);
		rtscam_ge_kill_fasync(rosd2->jdev, SIGIO, POLL_IN);
		return IRQ_HANDLED;
	}

	/*run done a cmd*/
	mask = 0x2;
	if (status & mask) {
		rtscam_osd2_write_reg(rosd2, mask, reg);
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}


static int rtscam_osd2_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_osd2 *rosd2 = rtscam_ge_get_drvdata(gdev);

	filp->private_data = rosd2;

	return 0;
}

static int rtscam_osd2_close(struct file *filp)
{
	struct rtscam_osd2 *rosd2 = filp->private_data;

	filp->private_data = NULL;

	if (!rosd2)
		return -EINVAL;

	return 0;
}

static long rtscam_osd2_do_ioctl(struct file *filp, unsigned int cmd,
				 void *arg)
{
	struct rtscam_osd2 *rosd2 = filp->private_data;
	int err = 0;

	if (_IOC_TYPE(cmd) != RTSOSD2_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RTSOSD2_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RTSOSD2_IOCGHWOFFSET:
		*(unsigned long *)arg = rosd2->iobaseaddr;
		break;
	case RTSOSD2_IOCGHWIOSIZE:
		*(unsigned int *)arg = rosd2->iosize;
		break;
	case RTSOSD2_IOCDONE:
		clear_bit(OSD2_FRAME_DONE_BIT, &rosd2->status);
		break;
	case RTSOSD2_IOC_ENABLE:
		mutex_lock(&rosd2->lock);
		if (atomic_inc_return(&rosd2->use_count) == 1) {
			rtscam_osd2_reset(rosd2);
			rtscam_osd2_enable_bus(rosd2);
			rtscam_osd2_enable_clk(rosd2, 1);
			rtscam_osd2_config_isp_buffer(rosd2);
			rtscam_osd2_enable_interrupt(rosd2, rosd2->irq_enable);
		}
		mutex_unlock(&rosd2->lock);
		break;
	case RTSOSD2_IOC_DISABLE:
		mutex_lock(&rosd2->lock);
		if (atomic_dec_return(&rosd2->use_count) == 0) {
			rtscam_osd2_enable_clk(rosd2, 0);
			rtscam_osd2_disable_bus(rosd2);
		}
		mutex_unlock(&rosd2->lock);
		break;
	case RTSOSD2_IOC_WAIT_INTERRUPT:
		mutex_lock(&rosd2->lock);
		err = wait_event_interruptible_timeout(rosd2->alarm_wq,
			test_bit(OSD2_FRAME_DONE_BIT, &rosd2->status) != 0,
			msecs_to_jiffies(30));
		if (!err) {
			err = -ETIME;
			mutex_unlock(&rosd2->lock);
			break;
		}

		if (signal_pending(current)) {
			err = -ERESTARTSYS;
			mutex_unlock(&rosd2->lock);
			break;
		}
		err = 0;
		mutex_unlock(&rosd2->lock);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "unknown[rtsosd2] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		err = -ENOTTY;
		break;
	}

	return err;
}

static long rtscam_osd2_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_osd2_do_ioctl);
}

static unsigned int rtscam_osd2_poll(struct file *filp,
				     struct poll_table_struct *wait)
{
	struct rtscam_osd2 *rosd2 = filp->private_data;
	unsigned int mask = 0;
	unsigned long req_events = poll_requested_events(wait);

	if (!(req_events & (POLLIN | POLLRDNORM)))
		return mask;

	if (!test_bit(OSD2_FRAME_DONE_BIT, &rosd2->status))
		poll_wait(filp, &rosd2->alarm_wq, wait);

	if (test_bit(OSD2_FRAME_DONE_BIT, &rosd2->status))
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static int rtscam_osd2_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct rtscam_osd2 *rosd2 = filp->private_data;
	unsigned long addr, size, start, end;

	addr = vm->vm_pgoff << PAGE_SHIFT;
	size = vm->vm_end - vm->vm_start;
	start = rosd2->iobaseaddr;
	end = start + PAGE_ALIGN(rosd2->iosize);

	if (addr < start || (addr + size) > end)
		return -EINVAL;

	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);

	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff,
			size, vm->vm_page_prot) ? -EAGAIN : 0;
}

static struct rtscam_ge_file_operations rtscam_osd2_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_osd2_open,
	.release	= rtscam_osd2_close,
	.ioctl		= rtscam_osd2_ioctl,
	.poll		= rtscam_osd2_poll,
	.mmap		= rtscam_osd2_mmap,
};

static int __create_device(struct rtscam_osd2 *rosd2)
{

	struct rtscam_ge_device *gdev;
	int ret;

	if (rosd2->jdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_OSD2_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(rosd2->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_osd2_fops;

	rtscam_ge_set_drvdata(gdev, rosd2);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rosd2->jdev = gdev;

	return 0;
}

static void __remove_device(struct rtscam_osd2 *rosd2)
{
	struct rtscam_ge_device *gdev;

	if (!rosd2->jdev)
		return;

	gdev = rosd2->jdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
}

static int rtscam_osd2_parse_buffer_config(struct rtscam_region *region,
					   struct device_node *np)
{
	struct device_node *node;
	int ret;

	if (!region || !np)
		return -EINVAL;

	node = of_parse_phandle(np, "osd-config", 0);
	if (!node) {
		rtsprintk(RTS_TRACE_ERROR, "there is no osd-config node\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(node, "reg", 0, &region->base);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reg address\n");
		goto exit;
	}
	ret = of_property_read_u32_index(node, "reg", 1, &region->size);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reg size\n");
		goto exit;
	}

	rtsprintk(RTS_TRACE_DEBUG, "osd buffer config: <0x%x 0x%x>\n",
		  region->base, region->size);
	ret = 0;

exit:
	of_node_put(node);
	node = NULL;

	return ret;
}

static int rtscam_osd2_probe(struct platform_device *pdev)
{
	struct rtscam_osd2 *rosd2;
	struct resource *res;
	void __iomem *base;
	int irq;
	int err = 0;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (res == NULL) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}
	rosd2 = devm_kzalloc(&pdev->dev, sizeof(*rosd2), GFP_KERNEL);
	if (rosd2 == NULL) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera osd object\n");
		return -ENOMEM;
	}
	rosd2->dev = get_device(&pdev->dev);
	atomic_set(&rosd2->use_count, 0);
	mutex_init(&rosd2->lock);
	init_waitqueue_head(&rosd2->alarm_wq);

	if (of_device_is_compatible(pdev->dev.of_node,
					"realtek,rts3903-osd2")) {
		rosd2->ic_type = TYPE_RTS3903;
	} else if (of_device_is_compatible(pdev->dev.of_node,
					"realtek,rts3915-osd2")) {
		rosd2->ic_type = TYPE_RTS3915;
	} else if (of_device_is_compatible(pdev->dev.of_node,
					"realtek,rts3917-osd2")){
		rosd2->ic_type = TYPE_RTS3917;
	} else {
		rtsprintk(RTS_TRACE_ERROR, "unknown device type\n");
		err = -EINVAL;
		goto error;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap resource\n");
		err = PTR_ERR(base);
		goto error;
	}
	rosd2->hwregs = base;
	rosd2->iobaseaddr = res->start;
	rosd2->iosize = resource_size(res);

	if (irq > 0) {
		err = devm_request_irq(rosd2->dev, irq, rtscam_osd2_irq,
				       IRQF_SHARED, RTS_OSD2_DRV_NAME, rosd2);
		if (err) {
			rtsprintk(RTS_TRACE_ERROR,
				  "osd:request irq fail\n");
			goto error;
		}
		rosd2->irq_enable = 1;
		rtscam_osd2_enable_interrupt(rosd2, rosd2->irq_enable);
	}
	err = rtscam_osd2_parse_buffer_config(&rosd2->config,
					      pdev->dev.of_node);
	if (err)
		goto error;

	if (rosd2->ic_type >= TYPE_RTS3917) {
		rosd2->reset = devm_reset_control_get(&pdev->dev, "osd_reset");
		if (IS_ERR(rosd2->reset)) {
			rtsprintk(RTS_TRACE_ERROR, "osd get reset fail\n");
			return -EINVAL;
		}
	}

	__create_device(rosd2);
	platform_set_drvdata(pdev, rosd2);

	return 0;
error:
	if (rosd2 && rosd2->dev) {
		put_device(rosd2->dev);
		rosd2->dev = NULL;
	}
	return err;
}

static int rtscam_osd2_remove(struct platform_device *pdev)
{
	struct rtscam_osd2 *rosd2 = platform_get_drvdata(pdev);

	__remove_device(rosd2);
	put_device(rosd2->dev);
	rosd2->dev = NULL;

	return 0;
}

static const struct of_device_id rtscam_osd2_ids[] = {
	{ .compatible = "realtek,rts3903-osd2", },
	{ .compatible = "realtek,rts3915-osd2", },
	{ .compatible = "realtek,rts3917-osd2", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_osd2_driver = {
	.driver		= {
		.name	= RTS_OSD2_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_osd2_ids),
	},
	.probe		= rtscam_osd2_probe,
	.remove		= rtscam_osd2_remove,
};

module_platform_driver(rtscam_osd2_driver);

MODULE_DESCRIPTION("Realsil Osd device driver");
MODULE_AUTHOR("Wil Shi <wil_shi@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.1");
MODULE_ALIAS("platform:" RTS_OSD2_DRV_NAME);
