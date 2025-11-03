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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/property.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>

#include "spi-dw.h"

#define DRIVER_NAME "dw_spi_rts"
#define RTS_SPI_AUTOSUSPEND_DELAY 1000

/*Register for ssi spi*/
#define SSI_START 0x8000
#define SSI_STOP 0x8004
#define SSI_RD_ADDR 0x8008
#define SSI_WR_ADDR 0x800c
#define SSI_DATA_LEN 0x8010
#define SSI_CONTROL 0x8014
#define SSI_IRQ_ENABLE 0x8018
#define SSI_IRQ_STATUS 0x801c

//SPI_SSI_START
#define SPI_SSI_START 0x1

//SPI_SSI_STOP
#define SPI_SSI_STOP 0x1

//SPI_SSI_IRQ
#define DONE_INT_EN 0x1
#define DONE_INT_DIS 0x0

//SSI_CONTROL
#define SSI_AUTO_CS 18

//SPI_SSI_IRQ_STATUS
#define SSI_DONE_INT 0x1

/*Register for ssi pad offsets*/
/*0x1887_0070 ~ 0x1887_008c*/
#define DW_SSI_PAD_SEL 0x0
#define DW_SSI_PAD_PU 0x4
#define DW_SSI_PAD_PD 0x8
#define DW_SSI_PAD_SR 0xc
#define DW_SSI_PAD_OE2 0x10
#define DW_SSI_CS_FW 0x14
#define DW_SSI_CS_SEL 0x18
#define DW_SSI_CS_CTRL_SEL 0x1c

struct dw_spi_rts {
	struct dw_spi dws;
	struct device *dev;
	void __iomem *reg_pad; // TODO: move pad config to pinctrl subsystem
	struct clk *clk;
};

static inline u32 rts_spi_readl_pad(struct dw_spi_rts *dwsrts, u32 offset)
{
	return readl(dwsrts->reg_pad + offset);
}

static inline void rts_spi_writel_pad(struct dw_spi_rts *dwsrts, u32 offset,
				      u32 val)
{
	writel(val, dwsrts->reg_pad + offset);
}

static void rts_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct dw_spi *dws = spi_controller_get_devdata(spi->controller);
	struct dw_spi_rts *dwsrts = container_of(dws, struct dw_spi_rts, dws);
	bool cs_high = !!(spi->mode & SPI_CS_HIGH);

	if (cs_high == enable) {
		rts_spi_writel_pad(dwsrts, DW_SSI_CS_FW, 1);
		dw_writel(dws, DW_SPI_SER, BIT(0));
		rts_spi_writel_pad(dwsrts, DW_SSI_CS_SEL,
				   spi_get_chipselect(spi, 0));
		clear_bit(SSI_AUTO_CS, dws->regs + SSI_CONTROL);

	} else {
		rts_spi_writel_pad(dwsrts, DW_SSI_CS_FW, 0);
		dw_writel(dws, DW_SPI_SER, 0);
		set_bit(SSI_AUTO_CS, dws->regs + SSI_CONTROL);
	}
}

static int rts_spi_dma_init(struct device *dev, struct dw_spi *dws)
{
	init_completion(&dws->dma_completion);

	return 0;
}

static void rts_spi_dma_exit(struct dw_spi *dws)
{
}

static irqreturn_t rts_spi_dma_transfer_handler(struct dw_spi *dws)
{
	struct dw_spi_rts *dwsrts = container_of(dws, struct dw_spi_rts, dws);

	dev_err(dwsrts->dev, "dma interrupt should go to rts_spi_dma_irq\n");

	return IRQ_NONE;
}

static int rts_spi_dma_setup(struct dw_spi *dws, struct spi_transfer *xfer)
{
	reinit_completion(&dws->dma_completion);

	dws->transfer_handler = rts_spi_dma_transfer_handler;

	return 0;
}

static bool rts_spi_can_dma(struct spi_controller *host, struct spi_device *spi,
			    struct spi_transfer *xfer)
{
	struct dw_spi *dws = spi_controller_get_devdata(host);

	return xfer->len > dws->fifo_len;
}

static int rts_spi_dma_wait(struct dw_spi *dws, unsigned int len, u32 speed)
{
	unsigned long long ms;

	ms = (u64)len * MSEC_PER_SEC * BITS_PER_BYTE;
	do_div(ms, speed);
	ms += ms + 200;

	if (ms > UINT_MAX)
		ms = UINT_MAX;

	ms = wait_for_completion_timeout(&dws->dma_completion,
					 msecs_to_jiffies(ms));

	if (ms == 0) {
		dev_err(&dws->host->cur_msg->spi->dev,
			"DMA transaction timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int rts_spi_dma_transfer(struct dw_spi *dws, struct spi_transfer *xfer)
{
	unsigned int base, len;
	unsigned int tx_len = 0, rx_len = 0;
	unsigned int tx_dma = 0, rx_dma = 0;
	struct scatterlist *tx_sg = NULL, *rx_sg = NULL;
	int ret;

	for (base = 0, len = 0; base < xfer->len; base += len) {
		/* Fetch next Tx DMA data chunk */
		if (!tx_len && xfer->tx_buf) {
			tx_sg = !tx_sg ? &xfer->tx_sg.sgl[0] : sg_next(tx_sg);
			tx_dma = sg_dma_address(tx_sg);
			tx_len = sg_dma_len(tx_sg);
		}

		/* Fetch next Rx DMA data chunk */
		if (!rx_len && xfer->rx_buf) {
			rx_sg = !rx_sg ? &xfer->rx_sg.sgl[0] : sg_next(rx_sg);
			rx_dma = sg_dma_address(rx_sg);
			rx_len = sg_dma_len(rx_sg);
		}

		if (xfer->tx_buf && xfer->rx_buf)
			len = min(tx_len, rx_len);
		else if (xfer->tx_buf)
			len = tx_len;
		else if (xfer->rx_buf)
			len = rx_len;

		dw_writel(dws, SSI_RD_ADDR, tx_dma);
		dw_writel(dws, SSI_WR_ADDR, rx_dma);

		dw_writel(dws, SSI_DATA_LEN, len);
		/* Set the interrupt mask */
		dw_writel(dws, SSI_IRQ_STATUS, SSI_DONE_INT);
		dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_EN);
		/* start */
		dw_writel(dws, SSI_START, SPI_SSI_START);

		ret = rts_spi_dma_wait(dws, len, xfer->effective_speed_hz);
		if (ret)
			break;

		dw_writel(dws, SSI_STOP, SPI_SSI_STOP);

		reinit_completion(&dws->dma_completion);

		tx_dma += xfer->tx_buf ? len : 0;
		rx_dma += xfer->rx_buf ? len : 0;
		tx_len -= xfer->tx_buf ? len : 0;
		rx_len -= xfer->rx_buf ? len : 0;
	}

	return 0;
}

static void rts_spi_dma_stop(struct dw_spi *dws)
{
	/* stop */
	dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_DIS);

	dw_writel(dws, SSI_STOP, SPI_SSI_STOP);
}

static const struct dw_spi_dma_ops rts_dma_ops = {
	.dma_init = rts_spi_dma_init,
	.dma_exit = rts_spi_dma_exit,
	.dma_setup = rts_spi_dma_setup,
	.can_dma = rts_spi_can_dma,
	.dma_transfer = rts_spi_dma_transfer,
	.dma_stop = rts_spi_dma_stop,
};

static irqreturn_t rts_spi_dma_irq(int irq, void *data)
{
	struct dw_spi *dws = data;
	struct spi_controller *host = dws->host;
	u16 irq_dma_status = dw_readl(dws, SSI_IRQ_STATUS) & SSI_DONE_INT;

	if (!irq_dma_status)
		return IRQ_NONE;

	if (!host->cur_msg) {
		dw_writel(dws, SSI_IRQ_ENABLE, DONE_INT_DIS);
		return IRQ_HANDLED;
	}

	dw_writel(dws, SSI_IRQ_STATUS, SSI_DONE_INT);

	complete(&dws->dma_completion);

	return IRQ_HANDLED;
}

static int dw_spi_rts_probe(struct platform_device *pdev)
{
	struct dw_spi_rts *dwsrts;
	struct dw_spi *dws;
	struct resource *mem;
	int ret;
	int num_cs;

	dwsrts =
		devm_kzalloc(&pdev->dev, sizeof(struct dw_spi_rts), GFP_KERNEL);
	if (!dwsrts)
		return -ENOMEM;

	dwsrts->dev = &pdev->dev;
	dws = &dwsrts->dws;

	platform_set_drvdata(pdev, dwsrts);

	/* Get basic io resource and map it */
	dws->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &mem);
	if (IS_ERR(dws->regs))
		return PTR_ERR(dws->regs);

	dws->paddr = mem->start;

	dwsrts->reg_pad = devm_platform_get_and_ioremap_resource(pdev, 1, NULL);
	if (IS_ERR(dwsrts->reg_pad))
		return PTR_ERR(dwsrts->reg_pad);

	dws->irq = platform_get_irq(pdev, 0);
	if (dws->irq < 0)
		return dws->irq; /* -ENXIO */

	ret = devm_request_irq(&pdev->dev, dws->irq, rts_spi_dma_irq,
			       IRQF_SHARED, dev_name(&pdev->dev), dws);
	if (ret) {
		dev_err(&pdev->dev, "can not get IRQ\n");
		return ret;
	}

	dwsrts->clk = devm_clk_get(&pdev->dev, "ssi_ck");
	if (IS_ERR(dwsrts->clk))
		return PTR_ERR(dwsrts->clk);

	dws->bus_num = pdev->id;

	dws->max_freq = clk_get_rate(dwsrts->clk);

	if (device_property_read_u32(&pdev->dev, "reg-io-width",
				     &dws->reg_io_width))
		dws->reg_io_width = 4;

	num_cs = 4;

	device_property_read_u32(&pdev->dev, "num-cs", &num_cs);

	dws->num_cs = num_cs;

	dws->dma_ops = &rts_dma_ops;

	dws->set_cs = rts_spi_set_cs;

	pm_runtime_set_autosuspend_delay(&pdev->dev, RTS_SPI_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to resume ssi device.\n");
		goto out;
	}

	/*enable ssi pad select*/
	rts_spi_writel_pad(dwsrts, DW_SSI_PAD_SEL, 1);
	rts_spi_writel_pad(dwsrts, DW_SSI_CS_CTRL_SEL, 1);

	ret = dw_spi_add_host(&pdev->dev, dws);
	if (ret)
		goto out;

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;

out:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);

	return ret;
}

static void dw_spi_rts_remove(struct platform_device *pdev)
{
	struct dw_spi_rts *dwsrts = platform_get_drvdata(pdev);

	dw_spi_remove_host(&dwsrts->dws);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);

	clk_disable_unprepare(dwsrts->clk);
}

static const struct of_device_id dw_spi_rts_of_match[] = {
	{
		.compatible = "realtek,dw-apb-ssi",
	},
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, dw_spi_rts_of_match);

static int dw_spi_rts_runtime_suspend(struct device *dev)
{
	struct dw_spi_rts *dwsrts = dev_get_drvdata(dev);

	clk_disable_unprepare(dwsrts->clk);

	return 0;
}

static int dw_spi_rts_runtime_resume(struct device *dev)
{
	struct dw_spi_rts *dwsrts = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(dwsrts->clk);
	if (ret) {
		dev_err(dev, "Cannot enable ssi device clock.\n");
		clk_disable_unprepare(dwsrts->clk);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dw_spi_rts_suspend(struct device *dev)
{
	struct dw_spi_rts *dwsrts = dev_get_drvdata(dev);

	return dw_spi_suspend_host(&dwsrts->dws);
}

static int dw_spi_rts_resume(struct device *dev)
{
	struct dw_spi_rts *dwsrts = dev_get_drvdata(dev);

	return dw_spi_resume_host(&dwsrts->dws);
}
#endif

static const struct dev_pm_ops dw_spi_rts_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_spi_rts_suspend, dw_spi_rts_resume)
		SET_RUNTIME_PM_OPS(dw_spi_rts_runtime_suspend,
				   dw_spi_rts_runtime_resume, NULL)
};

static struct platform_driver dw_spi_rts_driver = {
	.probe		= dw_spi_rts_probe,
	.remove_new	= dw_spi_rts_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.pm     = &dw_spi_rts_pm_ops,
		.of_match_table = dw_spi_rts_of_match,
	},
};
module_platform_driver(dw_spi_rts_driver);

MODULE_AUTHOR("Keent <keent_zhuo@realsil.com.cn>");
MODULE_DESCRIPTION("SSI interface driver for DW SPI Core");
MODULE_LICENSE("GPL v2");
