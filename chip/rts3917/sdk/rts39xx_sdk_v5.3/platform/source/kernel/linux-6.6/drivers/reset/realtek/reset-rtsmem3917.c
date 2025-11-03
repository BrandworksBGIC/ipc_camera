/*
 * Realtek Semiconductor Corp.
 *
 * Memory power control driver
 *
 * Copyright (C) 2017      Wei WANG (wei_wang@realsil.com.cn)
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <dt-bindings/reset/rts-sysmem.h>

struct rts_sysmem_regs {
	u32 resvd[136];
	u32 sys_mem_ls;
	u32 sys_mem_ds;
	u32 sys_mem_sd;
#define NN_MEM_SD		BIT(22)
#define JPEG_MEM_SD		BIT(21)
#define MIPITX_MEM_SD		BIT(20)
#define LCDC_MEM_SD		BIT(19)
#define NAND_MEM_SD		(BIT(17) | BIT(18))
#define ONCHIP_MEM_SD		BIT(16)
#define H265_MEM_SD		BIT(15)
#define DRAM_MEM_SD		BIT(14)
#define CPU_MEM_SD		BIT(13)
#define VIDEO_MEM_SD		(BIT(10) | BIT(11) | BIT(12))
#define ISP_DMA_MEM_SD		BIT(9)
#define ISP_MEM_SD		BIT(8)
#define SDIO1_MEM_SD		BIT(7)
#define SDIO0_MEM_SD		BIT(6)
#define RSA_MEM_SD		BIT(5)
#define GE_MEM_SD		BIT(4)
#define ETH_MEM_SD		BIT(3)
#define CIPHER_MEM_SD		BIT(2)
#define AUDIO_MEM_SD		BIT(1)
#define U2DEV_MEM_SD		BIT(0)
};

struct rts_sysmem_data {
	struct mutex				lock;
	struct rts_sysmem_regs __iomem	*regs;
	struct reset_controller_dev	rcdev;
};

#define SYS_ISP_MEM_ALL_MASK	0xFFFFFFFF
#define SYS_VIDEO_MEM_ALL_MASK	0xFFFFFFFF

#define RTS_REG_SET(addr, mask)				\
do {							\
	u32 val;					\
	val = readl((addr));				\
	val |= (mask);					\
	writel(val, (addr));				\
} while (0)

#define RTS_REG_CLR(addr, mask)				\
do {							\
	u32 val;					\
	val = readl((addr));				\
	val &= ~(mask);					\
	writel(val, (addr));				\
} while (0)

static int rts_sysmem_deassert(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	struct rts_sysmem_data *rdata = container_of(rcdev,
						     struct rts_sysmem_data,
						     rcdev);

	struct rts_sysmem_regs *regs = rdata->regs;

	mutex_lock(&rdata->lock);

	switch (id) {
	case SYS_ISP_MEM:
		RTS_REG_CLR(&regs->sys_mem_sd, ISP_MEM_SD);
		RTS_REG_CLR(&regs->sys_mem_sd, ISP_DMA_MEM_SD);
		break;

	case SYS_VIDEO_MEM:
		RTS_REG_CLR(&regs->sys_mem_sd, VIDEO_MEM_SD);
		break;

	case SYS_MEM_SD_NAND_SPIC:
		RTS_REG_CLR(&regs->sys_mem_sd, NAND_MEM_SD);
		break;

	case SYS_MEM_SD_ETH:
		RTS_REG_CLR(&regs->sys_mem_sd, ETH_MEM_SD);
		break;

	case SYS_MEM_SD_CIPHER:
		RTS_REG_CLR(&regs->sys_mem_sd, CIPHER_MEM_SD);
		break;

	case SYS_MEM_SD_AUDIO:
		RTS_REG_CLR(&regs->sys_mem_sd, AUDIO_MEM_SD);
		break;

	case SYS_MEM_SD_H265:
		RTS_REG_CLR(&regs->sys_mem_sd, H265_MEM_SD);
		break;

	case SYS_MEM_SD_U2DEV:
		RTS_REG_CLR(&regs->sys_mem_sd, U2DEV_MEM_SD);
		break;

	case SYS_MEM_SD_SDIO0:
		RTS_REG_CLR(&regs->sys_mem_sd, SDIO0_MEM_SD);
		break;

	case SYS_MEM_SD_SDIO1:
		RTS_REG_CLR(&regs->sys_mem_sd, SDIO1_MEM_SD);
		break;

	case SYS_MEM_SD_GE:
		RTS_REG_CLR(&regs->sys_mem_sd, GE_MEM_SD);
		break;

	case SYS_MEM_SD_RSA:
		RTS_REG_CLR(&regs->sys_mem_sd, RSA_MEM_SD);
		break;

	case SYS_MEM_SD_LCDC:
		RTS_REG_CLR(&regs->sys_mem_sd, LCDC_MEM_SD);
		break;

	case SYS_MEM_SD_MIPITX:
		RTS_REG_CLR(&regs->sys_mem_sd, MIPITX_MEM_SD);
		break;

	case SYS_MEM_SD_JPEG:
		RTS_REG_CLR(&regs->sys_mem_sd, JPEG_MEM_SD);
		break;

	case SYS_MEM_SD_NN:
		RTS_REG_CLR(&regs->sys_mem_sd, NN_MEM_SD);
		break;

	default:
		pr_info("ERROR: invalid sys mem id %ld\n", id);
		break;
	}

	mutex_unlock(&rdata->lock);

	return 0;
}

static int rts_sysmem_assert(struct reset_controller_dev *rcdev,
					unsigned long id)
{
	struct rts_sysmem_data *rdata = container_of(rcdev,
						     struct rts_sysmem_data,
						     rcdev);

	struct rts_sysmem_regs *regs = rdata->regs;

	mutex_lock(&rdata->lock);

	switch (id) {
	case SYS_ISP_MEM:
		RTS_REG_SET(&regs->sys_mem_sd, ISP_MEM_SD);
		RTS_REG_SET(&regs->sys_mem_sd, ISP_DMA_MEM_SD);
		break;

	case SYS_VIDEO_MEM:
		RTS_REG_SET(&regs->sys_mem_sd, VIDEO_MEM_SD);
		break;

	case SYS_MEM_SD_NAND_SPIC:
		RTS_REG_SET(&regs->sys_mem_sd, NAND_MEM_SD);
		break;

	case SYS_MEM_SD_ETH:
		RTS_REG_SET(&regs->sys_mem_sd, ETH_MEM_SD);
		break;

	case SYS_MEM_SD_CIPHER:
		RTS_REG_SET(&regs->sys_mem_sd, CIPHER_MEM_SD);
		break;

	case SYS_MEM_SD_AUDIO:
		RTS_REG_SET(&regs->sys_mem_sd, AUDIO_MEM_SD);
		break;

	case SYS_MEM_SD_H265:
		RTS_REG_SET(&regs->sys_mem_sd, H265_MEM_SD);
		break;

	case SYS_MEM_SD_U2DEV:
		RTS_REG_SET(&regs->sys_mem_sd, U2DEV_MEM_SD);
		break;

	case SYS_MEM_SD_SDIO0:
		RTS_REG_SET(&regs->sys_mem_sd, SDIO0_MEM_SD);
		break;

	case SYS_MEM_SD_SDIO1:
		RTS_REG_SET(&regs->sys_mem_sd, SDIO1_MEM_SD);
		break;

	case SYS_MEM_SD_GE:
		RTS_REG_SET(&regs->sys_mem_sd, GE_MEM_SD);
		break;

	case SYS_MEM_SD_RSA:
		RTS_REG_SET(&regs->sys_mem_sd, RSA_MEM_SD);
		break;

	case SYS_MEM_SD_LCDC:
		RTS_REG_SET(&regs->sys_mem_sd, LCDC_MEM_SD);
		break;

	case SYS_MEM_SD_MIPITX:
		RTS_REG_SET(&regs->sys_mem_sd, MIPITX_MEM_SD);
		break;

	case SYS_MEM_SD_JPEG:
		RTS_REG_SET(&regs->sys_mem_sd, JPEG_MEM_SD);
		break;

	case SYS_MEM_SD_NN:
		RTS_REG_SET(&regs->sys_mem_sd, NN_MEM_SD);
		break;

	default:
		pr_info("ERROR: invalid sys mem id %ld\n", id);
		break;
	}

	mutex_unlock(&rdata->lock);

	return 0;
}

static const struct reset_control_ops rlx_sysmem_ops = {
	.assert		= rts_sysmem_assert,
	.deassert	= rts_sysmem_deassert,
};

static int rts_sysmem_probe(struct platform_device *pdev)
{
	struct rts_sysmem_data *rdata;
	struct resource *res;

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata), GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	mutex_init(&rdata->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rdata->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rdata->regs))
		return PTR_ERR(rdata->regs);

	rdata->rcdev.owner = THIS_MODULE;
	rdata->rcdev.nr_resets = SYS_MEM_MAX;
	rdata->rcdev.ops = &rlx_sysmem_ops;
	rdata->rcdev.of_node = pdev->dev.of_node;

	writel(0, &rdata->regs->sys_mem_ls);
	writel(0, &rdata->regs->sys_mem_ds);

	return devm_reset_controller_register(&pdev->dev, &rdata->rcdev);
}

static const struct of_device_id rts_sysmem_dt_ids[] = {
	 { .compatible = "realtek,rts3917-sysmem", },
	 { /* sentinel */ },
};

static struct platform_driver rts_sysmem_driver = {
	.probe	= rts_sysmem_probe,
	.driver = {
		.name		= "rts-sysmem",
		.of_match_table	= of_match_ptr(rts_sysmem_dt_ids),
	},
};

static int __init rts_sysmem_init(void)
{
	return platform_driver_register(&rts_sysmem_driver);
}
postcore_initcall(rts_sysmem_init);

static void __exit rts_sysmem_exit(void)
{
	platform_driver_unregister(&rts_sysmem_driver);
}
module_exit(rts_sysmem_exit);
