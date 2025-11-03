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
#include "linux/rts_pp.h"


#define RTS_PP_DRV_NAME		"rts_pp"
#define RTS_PP_DEV_NAME		"rtspp"
#define RTS_PP_CLK_NAME         "pp_clk"

#define RTS_REG_PP_CTRL                0x400
#define RTS_REG_PP_WORK_MODE           0x404
#define RTS_REG_PP_DDR_ADDR            0x408
#define RTS_REG_PP_IMAGE_SIZE          0x40c
#define RTS_REG_PP_INT_ENABLE          0x18
#define RTS_REG_PP_INT_STATUS          0x44
#define RTS_REG_PP_INT_FLAG            0x414
#define RTS_REG_PP_RESULT_MV_H         0x418
#define RTS_REG_PP_RESULT_MV_L         0x41C
#define RTS_REG_PP_RESULT_SSD_H        0x420
#define RTS_REG_PP_RESULT_SSD_L        0x424
#define RTS_REG_PP_RESULT_INTRA        0x428
#define RTS_REG_PP_RESULT_VAR_H        0x42C
#define RTS_REG_PP_RESULT_VAR_L        0x430

#define RTS_PP_POLLING       1

struct rtscam_pp {
	struct device *dev;

	struct rtscam_ge_device *jdev;

	void __iomem *hwregs;

	struct clk *clk;

	struct completion pp_completion;
};

static u32 rtscam_pp_read_reg(struct rtscam_pp *rtspp, off_t reg)
{
	return le32_to_cpu(ioread32(rtspp->hwregs + reg));
}

static void rtscam_pp_write_reg(struct rtscam_pp *rtspp, u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), rtspp->hwregs + reg);
}

static int __pp_is_running(struct rtscam_pp *rtspp)
{
	__u32 val;

	val = rtscam_pp_read_reg(rtspp, RTS_REG_PP_CTRL);
	if (val & 0x4)
		return 1;
	else
		return 0;
}

static int __pp_check_info(struct rtspp_info *info)
{
	if (!info->pp_addr || !info->width || !info->height)
		return -EINVAL;

	if (info->work_mode >= RTS_PP_WORK_MODE_RESERVED)
		return -EINVAL;

	return 0;
}

#if RTS_PP_POLLING
static int rtscam_pp_disable_interrupt(struct rtscam_pp *rtspp)
{
	if (!rtspp)
		return -EINVAL;

	rtscam_pp_write_reg(rtspp, 0x0000, RTS_REG_PP_INT_FLAG);
	rtscam_pp_write_reg(rtspp, 0x0000, RTS_REG_PP_INT_ENABLE);
	return 0;
}
#else
static int rtscam_pp_enable_interrupt(struct rtscam_pp *rtspp)
{
	if (!rtspp)
		return -EINVAL;

	rtscam_pp_write_reg(rtspp, 0xffff, RTS_REG_PP_INT_FLAG);
	rtscam_pp_write_reg(rtspp, 0xffff, RTS_REG_PP_INT_ENABLE);
	return 0;
}
#endif
static irqreturn_t rtscam_pp_irq(int irq, void *data)
{
	struct rtscam_pp *rtspp = data;
	u32 status;
	u32 mask;

	const off_t reg = RTS_REG_PP_INT_FLAG;
	const off_t reg_clear = RTS_REG_PP_INT_STATUS;

	status = rtscam_pp_read_reg(rtspp, reg);
	if (!status) {
		return IRQ_NONE;
	}

	mask = 0x1;
	if (status & mask) {
		rtscam_pp_write_reg(rtspp, 0xff, reg_clear);
#if RTS_PP_POLLING == 0
		complete(&rtspp->pp_completion);
#endif
		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

static int __pp_get_info(struct rtscam_pp *rtspp, struct rtspp_info *info)
{
	if (!rtspp || !info)
		return -EINVAL;

	info->mv = ((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_MV_H) << 32) |
		((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_MV_L));
	info->ssd = ((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_SSD_H) << 32) |
		((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_SSD_L));
	info->var = ((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_VAR_H) << 32) |
		((u64)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_VAR_L));
	info->intra = (u32)rtscam_pp_read_reg(rtspp, RTS_REG_PP_RESULT_INTRA);

	return 0;
}

static int __pp_set_info(struct rtscam_pp *rtspp, struct rtspp_info *info)
{
	if (__pp_check_info(info)) {
		rtsprintk(RTS_TRACE_ERROR, "invalid rtspp info, set fail\n");
		return -EINVAL;
	}

	rtscam_pp_write_reg(rtspp, (info->work_mode & 0x01),
				RTS_REG_PP_WORK_MODE);
	rtscam_pp_write_reg(rtspp, info->pp_addr, RTS_REG_PP_DDR_ADDR);
	rtscam_pp_write_reg(rtspp, (((__u32)info->width) << 16) |
				((__u32)info->height),
				RTS_REG_PP_IMAGE_SIZE);

	return 0;
}

static int rtscam_pp_exec(struct rtscam_pp *rtspp, struct rtspp_info *info)
{
	int ret;
#if RTS_PP_POLLING
	u32 status;
	u32 mask;

	const off_t reg = RTS_REG_PP_INT_FLAG;
	const off_t reg_clear = RTS_REG_PP_INT_STATUS;
#endif
	if (!rtspp || !info)
		return -EINVAL;

	if (__pp_is_running(rtspp)) {
		rtsprintk(RTS_TRACE_ERROR,
			"rtspp is running now, please wait\n");
		return -EPERM;
	}

	ret = __pp_set_info(rtspp, info);
	if (ret)
		return ret;
#if RTS_PP_POLLING == 0
	init_completion(&rtspp->pp_completion);
	rtscam_pp_write_reg(rtspp, 1, RTS_REG_PP_CTRL);

	ret = wait_for_completion_timeout(
			&rtspp->pp_completion, 3000 * HZ / 1000);
	if (ret <= 0) {
		rtsprintk(RTS_TRACE_ERROR, "rtspp wait for complete fail\n");
		return -EINVAL;
	}
#else
	rtscam_pp_write_reg(rtspp, 1, RTS_REG_PP_CTRL);

	do {
		status = rtscam_pp_read_reg(rtspp, reg);
	} while (!status);
	mask = 0x01;
	if (status & mask)
		rtscam_pp_write_reg(rtspp, 0xff, reg_clear);
#endif
	ret = __pp_get_info(rtspp, info);
	if (ret)
		return ret;

	return 0;
}

static long rtscam_pp_do_ioctl(struct file *filp, unsigned int cmd,
				 void *arg)
{
	struct rtscam_pp *rtspp = filp->private_data;
	int err = 0;

	if (_IOC_TYPE(cmd) != RTSPP_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RTSPP_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RTSPP_IOC_EXEC:
		err = rtscam_pp_exec(rtspp, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "unknown[rtspp] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		err = -ENOTTY;
		break;
	}

	return err;
}

static long rtscam_pp_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_pp_do_ioctl);
}

static int rtscam_pp_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_pp *rtspp = rtscam_ge_get_drvdata(gdev);
#if RTS_PP_POLLING == 0
	rtscam_pp_enable_interrupt(rtspp);
#else
	rtscam_pp_disable_interrupt(rtspp);
#endif
	filp->private_data = rtspp;

	return 0;
}

static int rtscam_pp_close(struct file *filp)
{
	filp->private_data = NULL;

	return 0;
}

static struct rtscam_ge_file_operations rtscam_pp_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_pp_open,
	.release	= rtscam_pp_close,
	.ioctl		= rtscam_pp_ioctl,
};

static int __create_device(struct rtscam_pp *rtspp)
{

	struct rtscam_ge_device *gdev;
	int ret;

	if (rtspp->jdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_PP_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(rtspp->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_pp_fops;

	rtscam_ge_set_drvdata(gdev, rtspp);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rtspp->jdev = gdev;

	return 0;
}

static int rtscam_pp_probe(struct platform_device *pdev)
{
	struct rtscam_pp *rtspp;
	struct resource *res;
	void __iomem *base;
	int err = 0;
	int irq;
	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (res == NULL || irq < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}

	rtspp = devm_kzalloc(&pdev->dev, sizeof(*rtspp), GFP_KERNEL);
	if (!rtspp) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rtspp object\n");
		return -ENOMEM;
	}
	rtspp->dev = get_device(&pdev->dev);

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap rtspp resource\n");
		err = PTR_ERR(base);
		goto error;
	}
	rtspp->hwregs = base;

	if (irq > 0) {
		err = devm_request_irq(rtspp->dev, irq, rtscam_pp_irq,
				       IRQF_SHARED, RTS_PP_DRV_NAME, rtspp);
		if (err) {
			rtsprintk(RTS_TRACE_ERROR, "rtspp: request irq fail\n");
			goto error;
		}
	}

	err = __create_device(rtspp);
	if (err) {
		rtsprintk(RTS_TRACE_ERROR, "create rtspp device fail\n");
		goto error;
	}

	rtspp->clk = devm_clk_get(rtspp->dev, RTS_PP_CLK_NAME);
	if (IS_ERR(rtspp->clk)) {
		rtsprintk(RTS_TRACE_ERROR, "get pp clk fail\n");
		goto error;
	}

	clk_prepare_enable(rtspp->clk);

	platform_set_drvdata(pdev, rtspp);
	return 0;
error:
	if (rtspp && rtspp->dev) {
		put_device(rtspp->dev);
		rtspp->dev = NULL;
	}
	return err;
}

static void __remove_device(struct rtscam_pp *rtspp)
{
	struct rtscam_ge_device *gdev;

	if (!rtspp->jdev)
		return;

	gdev = rtspp->jdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
}

static int rtscam_pp_remove(struct platform_device *pdev)
{
	struct rtscam_pp *rtspp = platform_get_drvdata(pdev);

	__remove_device(rtspp);
	put_device(rtspp->dev);
	clk_disable_unprepare(rtspp->clk);
	devm_clk_put(rtspp->dev, rtspp->clk);
	rtspp->dev = NULL;

	return 0;
}

static const struct of_device_id rtscam_pp_ids[] = {
	{ .compatible = "realtek,rts3917-pp", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_pp_driver = {
	.driver		= {
		.name	= RTS_PP_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_pp_ids),
	},
	.probe		= rtscam_pp_probe,
	.remove		= rtscam_pp_remove,
};

module_platform_driver(rtscam_pp_driver);

MODULE_DESCRIPTION("Realsil pp device driver");
MODULE_AUTHOR("Bruce Sun <bruce_sun@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.1");
MODULE_ALIAS("platform:" RTS_PP_DRV_NAME);
