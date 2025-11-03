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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/cdev.h>
#include <linux/thermal.h>

/* DDR controller register offsets */
#define DRR 0x0010

/*DDR phy register offset*/
#define SSC2 0x24
#define SSC3 0x28

/* DRR bits */
#define tRFC 0
#define tREF 8

#define REF_PLL 25000000

enum {
	DDR_STATUS_NORMAL = 0,
	DDR_STATUS_HOT,
};

struct rts_ddrc {
	struct device *dev;
	void __iomem *cregs; /*ddr controller register*/
	void __iomem *pregs; /*ddr phy register*/
	u32 tref;
	int cur_state;
};

static ssize_t ddrc_tref_show(struct device *_dev,
			      struct device_attribute *attr, char *buf)
{
	u32 tref = 0;

	struct rts_ddrc *ddrc = dev_get_drvdata(_dev);

	if (ddrc->dev == _dev)
		tref = ddrc->tref;

	return sprintf(buf, "%d\n", tref);
}

DEVICE_ATTR(ddrc_tref, 0644, ddrc_tref_show, NULL);

static const struct attribute *ddrc_attrs[] = {
	&dev_attr_ddrc_tref.attr,
	NULL,
};

static const struct attribute_group ddrc_attr_group = {
	.attrs = (struct attribute **)ddrc_attrs,
};

static inline u32 rts_readl_c(struct rts_ddrc *rddr, u32 reg)
{
	return readl(rddr->cregs + reg);
}

static inline void rts_writel_c(struct rts_ddrc *rddr, u32 reg, u32 val)
{
	writel(val, rddr->cregs + reg);
}

static inline u32 rts_readl_p(struct rts_ddrc *rddr, u32 reg)
{
	return readl(rddr->pregs + reg);
}

static inline void rts_writel_p(struct rts_ddrc *rddr, u32 reg, u32 val)
{
	writel(val, rddr->pregs + reg);
}

static int get_dram_clock(struct rts_ddrc *rddr)
{
	u64 dpi_n_code, dpi_f_code;
	u64 clock;

	dpi_f_code = rts_readl_p(rddr, SSC2) & 0x7ff;
	dpi_n_code = rts_readl_p(rddr, SSC3) & 0xff;

	clock = div_u64(REF_PLL * dpi_f_code * 2, 2048);

	if (of_device_is_compatible(rddr->dev->of_node, "realtek,rts3917-ddrc"))
		clock = div_u64(
			(clock + REF_PLL * (dpi_n_code + 3) * 2 + 500000),
			1000000);

	return (int)clock;
}

static int rts_ddrc_adjust_tref(struct rts_ddrc *rddr, int state)
{
	int ddr_clock;
	int pctl_period_ps = 2500;
	u32 drr_val_temp;
	u32 drr_tref_val_temp;

	/*get ddr clock*/
	ddr_clock = get_dram_clock(rddr);

	switch (ddr_clock) {
	case 800:
		pctl_period_ps = 5000;
		break;
	case 1066:
		pctl_period_ps = 3750;
		break;
	case 1333:
		pctl_period_ps = 3000;
		break;
	case 1600:
		pctl_period_ps = 2500;
		break;
	case 1866:
		pctl_period_ps = 2143;
		break;
	case 2133:
		pctl_period_ps = 1875;
		break;
	default:
		dev_err(rddr->dev, "invalid ddr clock\n");
		return -EINVAL;
	}

	drr_val_temp = rts_readl_c(rddr, DRR);
	drr_tref_val_temp = ((u16)(drr_val_temp >> 8) + 0x100) * pctl_period_ps;

	rddr->tref = drr_tref_val_temp;

	if ((state == DDR_STATUS_NORMAL) && (rddr->tref < 59900000)) {
		dev_info(rddr->dev, "switch to DDR normal state parameters\n");
		rddr->tref = 60000000;
		drr_val_temp = (drr_val_temp & 0xff0000ff) |
			       ((rddr->tref / pctl_period_ps - 0x100) << 8);
		rts_writel_c(rddr, DRR, drr_val_temp);
	}

	if ((state == DDR_STATUS_HOT) && (rddr->tref > 59900000)) {
		dev_info(rddr->dev, "switch to DDR hot state parameters\n");
		rddr->tref = 30000000;
		drr_val_temp = (drr_val_temp & 0xff0000ff) |
			       ((rddr->tref / pctl_period_ps - 0x100) << 8);
		rts_writel_c(rddr, DRR, drr_val_temp);
	}

	return 0;
}

static int ddrc_get_max_state(struct thermal_cooling_device *cdev,
			      unsigned long *state)
{
	*state = DDR_STATUS_HOT;

	return 0;
}

static int ddrc_get_cur_state(struct thermal_cooling_device *cdev,
			      unsigned long *state)
{
	struct rts_ddrc *rddr = cdev->devdata;

	*state = rddr->cur_state;

	return 0;
}

static int ddrc_set_cur_state(struct thermal_cooling_device *cdev,
			      unsigned long state)
{
	struct rts_ddrc *rddr = cdev->devdata;
	int ret;

	ret = rts_ddrc_adjust_tref(rddr, state);
	if (ret) {
		dev_err(rddr->dev, "failed to adjust tref\n");
		return ret;
	}

	rddr->cur_state = state;

	return 0;
}

static struct thermal_cooling_device_ops ddrc_cooling_ops = {
	.get_max_state = ddrc_get_max_state,
	.get_cur_state = ddrc_get_cur_state,
	.set_cur_state = ddrc_set_cur_state,
};

static const struct of_device_id rts_ddrc_dt_ids[] = {
	{
		.compatible = "realtek,rts3917-ddrc",
	},
	{}
};

MODULE_DEVICE_TABLE(of, rts_ddrc_dt_ids);

static int rts_ddrc_probe(struct platform_device *pdev)
{
	struct rts_ddrc *rddr;
	struct thermal_cooling_device *cdev;
	int err;

	rddr = devm_kzalloc(&pdev->dev, sizeof(*rddr), GFP_KERNEL);
	if (!rddr)
		return -ENOMEM;

	rddr->dev = &pdev->dev;

	rddr->cregs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(rddr->cregs)) {
		dev_err(&pdev->dev, "get and ioremap memory resource 0 failed");
		return PTR_ERR(rddr->cregs);
	}

	rddr->pregs = devm_platform_get_and_ioremap_resource(pdev, 1, NULL);
	if (IS_ERR(rddr->pregs)) {
		dev_err(&pdev->dev, "no memory resource provided");
		return PTR_ERR(rddr->pregs);
	}

	platform_set_drvdata(pdev, rddr);

	err = sysfs_create_group(&rddr->dev->kobj, &ddrc_attr_group);
	if (err < 0)
		return err;

	cdev = devm_thermal_of_cooling_device_register(
		&pdev->dev, pdev->dev.of_node, "ddrc", rddr, &ddrc_cooling_ops);
	if (IS_ERR(cdev)) {
		dev_err(&pdev->dev,
			"failed to register thermal cooling device");
		return PTR_ERR(cdev);
	}

	return 0;
}

static void rts_ddrc_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &ddrc_attr_group);
}

static struct platform_driver rts_ddrc_driver = {
	.driver = {
		.name = "ddrc_platform",
		.of_match_table = rts_ddrc_dt_ids,
	},
	.probe = rts_ddrc_probe,
	.remove_new = rts_ddrc_remove,
};
module_platform_driver(rts_ddrc_driver);

MODULE_LICENSE("GPL v2");
