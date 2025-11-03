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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clocksource.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define XB2_RTC_CFG_EN 0x0000
#define XB2_RTC_CNT_RST 0x0004
#define XB2_RTC_CNT_SET 0x0008
#define XB2_RTC_CLK32K_SEL 0x000C
#define XB2_RTC_XTAL_CFG 0x0010
#define XB2_RTC_XTAL_STATE 0x0014
#define XB2_RTC_C_CNT 0x0018
#define XB2_RTC_C_CNT_SYNC 0x001C
#define XB2_RTC_S_CNT 0x0020
#define XB2_RTC_SAMPLE_CTRL 0x0024
#define XB2_RTC_ALARM_EN 0x0040
#define XB2_RTC_ALARM0_SECOND 0x0044
#define XB2_RTC_ALARM1_SECOND 0x0048
#define XB2_RTC_ALARM2_SECOND 0x004C
#define XB2_RTC_ALARM3_SECOND 0x0050
#define XB2_RTC_ALARM_INT_EN 0x0054
#define XB2_RTC_ALARM_INT_FLAG 0x0058

#define ALARM0_ENABLE 0
#define ALARM1_ENABLE 1
#define ALARM2_ENABLE 2
#define ALARM3_ENABLE 3

#define SEC_VALUE(x) (x & 0x3f)
#define MIN_VALUE(x) ((x & 0x3f00) >> 8)
#define HOUR_VALUE(x) ((x & 0x1f0000) >> 16)
#define WEEK_VALUE(x) ((x & 0x7000000) >> 24)

#define DAY_VALUE(x) (x & 0x1f)
#define MON_VALUE(x) ((x & 0xf00) >> 8)
#define YEAR_VALUE(x) ((x & 0x7f0000) >> 16)
#define CEN_VALUE(x) ((x & 0x7f000000) >> 24)

#define TIME_REG(weekday, hour, min, sec)                   \
	(((weekday & 0x7f) << 24) | ((hour & 0x1f) << 16) | \
	 ((min & 0x3f) << 8) | (sec & 0x3f))

#define DATE_REG(cen, year, mon, day)                                        \
	(((cen & 0x7f) << 24) | ((year & 0x7f) << 16) | ((mon & 0xf) << 8) | \
	 (day & 0x1f))

enum {
	TYPE_RTS3917 = 1,

	TYPE_FPGA = (1 << 16),
};

#define RTS_SOC_CAM_HW_ID(type) ((type) & 0xff)

struct rts_rtc {
	struct resource *mem;
	void __iomem *base;
	struct rtc_device *rtc;
	int irq;
	spinlock_t lock;
	int devtype;
	int xtal_flag;
};

int rts_set_time_xtal(struct rts_rtc *rtc, u32 sec)
{
	iowrite32(1, rtc->base + XB2_RTC_CFG_EN);
	msleep(500);

	iowrite32(1, rtc->base + XB2_RTC_CLK32K_SEL);
	iowrite32(1, rtc->base + XB2_RTC_CNT_RST);
	iowrite32(0, rtc->base + XB2_RTC_CNT_RST);

	iowrite32(sec, rtc->base + XB2_RTC_CNT_SET);
	iowrite32(0, rtc->base + XB2_RTC_CNT_SET);
	iowrite32(0, rtc->base + XB2_RTC_CLK32K_SEL);
	iowrite32(0, rtc->base + XB2_RTC_CFG_EN);

	return 0;
}

int rts_set_time_nonxtal(struct rts_rtc *rtc, u32 sec)
{
	iowrite32(1, rtc->base + XB2_RTC_CFG_EN);
	msleep(500);

	iowrite32(0, rtc->base + XB2_RTC_CLK32K_SEL);
	iowrite32(1, rtc->base + XB2_RTC_CNT_RST);
	iowrite32(0, rtc->base + XB2_RTC_CNT_RST);

	iowrite32(sec, rtc->base + XB2_RTC_CNT_SET);
	iowrite32(0, rtc->base + XB2_RTC_CNT_SET);
	iowrite32(1, rtc->base + XB2_RTC_CLK32K_SEL);
	iowrite32(0, rtc->base + XB2_RTC_CFG_EN);

	return 0;
}

int rts_set_time(struct rts_rtc *rtc, u32 sec, int xtal)
{
	int ret;

	if (xtal)
		ret = rts_set_time_xtal(rtc, sec);
	else
		ret = rts_set_time_nonxtal(rtc, sec);

	return ret;
}

static inline uint32_t rts_rtc_reg_read(struct rts_rtc *rtc, size_t reg)
{
	return ioread32(rtc->base + reg);
}

static inline void rts_rtc_reg_write(struct rts_rtc *rtc, size_t reg,
				     uint32_t val)
{
	iowrite32(val, rtc->base + reg);
}

static inline void rts_rtc_reg_setbit(struct rts_rtc *rtc, size_t reg,
				      uint32_t bit)
{
	u32 v;

	v = ioread32(rtc->base + reg) | (1 << bit);
	iowrite32(v, rtc->base + reg);
}

static inline void rts_rtc_reg_clearbit(struct rts_rtc *rtc, size_t reg,
					uint32_t bit)
{
	u32 v;

	v = ioread32(rtc->base + reg) & ~(1 << bit);
	iowrite32(v, rtc->base + reg);
}

static int rts_rtc_read_time(struct device *dev, struct rtc_time *rtc_tm)
{
	uint32_t time;

	struct platform_device *pdev = to_platform_device(dev);
	struct rts_rtc *pdata = platform_get_drvdata(pdev);

	time = ioread32(pdata->base + XB2_RTC_C_CNT);
	rtc_time64_to_tm(time, rtc_tm);

	dev_dbg(dev, "read time %04d.%02d.%02d %02d:%02d:%02d\n",
		1900 + rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	return rtc_valid_tm(rtc_tm);
}

static int rts_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_rtc *pdata = platform_get_drvdata(pdev);
	ulong sec;

	sec = rtc_tm_to_time64(tm);
#ifdef CONFIG_RTC_EXTERN_XTAL
	if (pdata->xtal_flag == 1) /*external xtal*/
		rts_set_time(pdata, sec, 1);
	else /*internal xtal*/
		rts_set_time(pdata, sec, 0);
#else
	rts_set_time(pdata, sec, 0);
#endif
	return 0;
}

static int rts_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_rtc *pdata = platform_get_drvdata(pdev);
	struct rtc_time *rtc_tm = &(alrm->time);
	ulong sec;

	sec = rtc_tm_to_time64(rtc_tm);
	rts_rtc_reg_write(pdata, XB2_RTC_ALARM3_SECOND, sec);

	return 0;
}

static int rts_rtc_get_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_rtc *pdata = platform_get_drvdata(pdev);
	uint32_t time;
	struct rtc_time *rtc_tm = &(alrm->time);

	time = ioread32(pdata->base + XB2_RTC_ALARM3_SECOND);
	alrm->enabled = rts_rtc_reg_read(pdata, XB2_RTC_ALARM_EN) &
			(1 << ALARM3_ENABLE);
	rtc_time64_to_tm(time, rtc_tm);

	dev_dbg(dev, "get alarm %04d.%02d.%02d %02d:%02d:%02d\n",
		1900 + rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	alrm->pending = 0;

	return 0;
}

static irqreturn_t rts_rtc_interrupt(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct rts_rtc *pdata = platform_get_drvdata(pdev);
	unsigned long events = 0;

	if ((rts_rtc_reg_read(pdata, XB2_RTC_ALARM_INT_FLAG) &
	     BIT(ALARM3_ENABLE)) == 0)
		return IRQ_NONE;

	rts_rtc_reg_setbit(pdata, XB2_RTC_ALARM_INT_FLAG, ALARM3_ENABLE);

	events = RTC_IRQF | RTC_AF;
	if (likely(pdata->rtc))
		rtc_update_irq(pdata->rtc, 1, events);

	return events ? IRQ_HANDLED : IRQ_NONE;
}

static int rts_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rts_rtc *pdata = platform_get_drvdata(pdev);

	if (pdata->irq <= 0)
		return -EINVAL;

	rts_rtc_reg_setbit(pdata, XB2_RTC_ALARM_EN, ALARM3_ENABLE);

	return 0;
}

static const struct rtc_class_ops rts_rtc_ops = {
	.read_time = rts_rtc_read_time,
	.set_time = rts_rtc_set_time,
	.read_alarm = rts_rtc_get_alarm,
	.set_alarm = rts_rtc_set_alarm,
	.alarm_irq_enable = rts_rtc_alarm_irq_enable,
};

static struct rts_rtc *rtc_data;
static uint64_t rtc_cs_read(struct clocksource *cs)
{
	return ioread32(rtc_data->base + XB2_RTC_C_CNT);
}

static struct clocksource clocksource_rtc = {
	.name = "RTC-RTS",
	.read = rtc_cs_read,
	.mask = CLOCKSOURCE_MASK(32),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static const struct of_device_id rlx_rtc_match[] = {
	{
		.compatible = "realtek,rts3917-fpga-rtc",
		.data = (void *)(TYPE_RTS3917 | TYPE_FPGA),
	},
	{
		.compatible = "realtek,rts3917-rtc",
		.data = (void *)(TYPE_RTS3917),
	},
	{}
};
MODULE_DEVICE_TABLE(of, rlx_rtc_match);

static int rts_rtc_probe(struct platform_device *pdev)
{
	int ret;
	struct rts_rtc *rtc;
	const struct of_device_id *of_id;
	struct rtc_time tm;
	u32 sec;
	u32 counter_o, counter_t;

	of_id = of_match_device(rlx_rtc_match, &pdev->dev);

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;
	rtc->devtype = (int)of_id->data;

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0) {
		dev_err(&pdev->dev, "Failed to get platform irq\n");
		ret = -ENOENT;
	}

	rtc->base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(rtc->base)) {
		dev_err(&pdev->dev, "Failed to get platform mmio memory\n");
		return PTR_ERR(rtc->base);
	}

	spin_lock_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);

	device_init_wakeup(&pdev->dev, 1);
	ret = devm_request_irq(&pdev->dev, rtc->irq, rts_rtc_interrupt,
			       IRQF_SHARED, pdev->name, pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request rtc irq\n");
		return ret;
	}

	//enable irq
	rts_rtc_reg_setbit(rtc, XB2_RTC_ALARM_INT_EN, ALARM3_ENABLE);

	rtc->rtc = devm_rtc_device_register(&pdev->dev, pdev->name,
					    &rts_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc)) {
		dev_err(&pdev->dev, "Failed to register rtc device\n");
		return PTR_ERR(rtc->rtc);
	}

	counter_o = ioread32(rtc->base + XB2_RTC_S_CNT);
	msleep(20);
	counter_t = ioread32(rtc->base + XB2_RTC_S_CNT);
	if (counter_o != counter_t)
		rtc->xtal_flag = 1;
	else
		rtc->xtal_flag = 0;
	sec = ioread32(rtc->base + XB2_RTC_C_CNT);
	rtc_time64_to_tm(sec, &tm);
	rts_rtc_set_time(&pdev->dev, &tm);

	/* register clocksource */
	rtc_data = rtc;
	clocksource_rtc.rating = 100;
	clocksource_register_hz(&clocksource_rtc, 1);

	return 0;
}

static void rts_rtc_remove(struct platform_device *pdev)
{
}

static struct platform_driver rts_rtc_driver = {
	.probe	 = rts_rtc_probe,
	.remove_new = rts_rtc_remove,
	.driver	 = {
		.name  = "rts-rtc",
		.of_match_table = rlx_rtc_match,
	},
};

module_platform_driver(rts_rtc_driver);
