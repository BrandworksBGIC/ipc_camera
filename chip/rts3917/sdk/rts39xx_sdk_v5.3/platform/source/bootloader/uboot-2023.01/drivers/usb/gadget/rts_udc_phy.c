// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 */

#include <common.h>
#include <malloc.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/errno.h>
#include <usb.h>
#include <linux/io.h>
#include "rts_udc_pri.h"

struct rts_usb_dev_phy_regs {
	u32 dphy_cfg;

	u32 dphy_cfg1;

	u32 dphy_cfg2;

	u32 dphy_sts;
#define VBUS_DEGLITCH_TIME_SHIFT 3
#define VBUS_INT_EN 0x04
#define VBUS_DETECT_ON 0x02
#define VBUS_DETECT_DOWN 0x01
};

struct rts_usb_phy_regs {
	u32 phy_mdio;

	u32 aphy_cfg1;
#define UPHY_HST_SEN_HST_SHIFT 27
#define UPHY_HST_SEN_HST_MASK 0x0F
#define UPHY_HST_SEN_SHIFT 23
#define UPHY_HST_SEN_MASK 0x0F

	u32 aphy_cfg2;
#define UPHY_HST_HS_CKSEL 0x08
#define UPHY_HST_CAL (1 << 7)
#define UPHY_HST_SRC_MASK 0x07

	u32 aphy_cfg3;
#define UPHY_DEV_SEN_SHIFT 23
#define UPHY_DEV_SEN_MASK 0x0F

	u32 aphy_cfg4;
#define UPHY_DEV_CAL (1 << 7)
#define UPHY_DEV_SRC_MASK 0x07

	u32 aphy_cfg5;
#define UPHY_ANA_AUTO_K (1 << 16)
#define UPHY_Z0_CODE_MASK 0x0F
#define UPHY_Z0_CODE_SHIFT 8

	u32 aphy_cfg6;
#define UPHY_U2_APHY_RST_N (1 << 20)

	u32 dphy_cfg1;

	u32 dphy_cfg2;
#define UPHY_U2_DPHY_CHECK_CHIRPK 0x100

	u32 host_cfg;
#define UPHY_PORT_OC_EN 0x01
#define UPHY_HST_FORCE_PORT_PWR_EN 0x02
#define UPHY_HST_AUTO_PPD_ON_OC 0x08
#define UPHY_PORT_PWR_POLARITY_SEL_HI 0x20000
#define UPHY_PORT_OC_POLARITY_SEL_HI 0x10000
};

#define RTS3901_HOST_DISCONN_THD 0x0C

#define RTS_Z0_CODE 9
#define RTS_DEV_SEN 2
#define RTS_HST_SEN 2
#define RTS_SRC_VAL 4

#define UPHY_DEV_PORT_VBUS_INT_MSK 0x03
#define UPHY_DEV_PORT_VBUS_ON_INT 0x02
#define UPHY_DEV_PORT_VBUS_DOWN_INT 0x01

static uint32_t rts_usb_read_phy_int(struct rts_usb_phy *rts)
{
	uint32_t int_val;

	int_val = readl(&rts->dev_regs->dphy_cfg2);
	return int_val;
}

static void rts_usb_clear_phy_int(struct rts_usb_phy *rts, uint32_t int_val,
				  uint32_t clr_msk)
{
	clr_msk = ~clr_msk;
	writel(int_val & clr_msk, &rts->dev_regs->dphy_cfg2);
}

#define DEV_PORT 0
#define HST_PORT 1

#define PORT_CFG0(port) ((port) << 4)
#define PORT_CFG1(port) (((port) << 4) | 0x01)
#define PORT_CFG2(port) (((port) << 4) | 0x02)
#define PORT_CFG3(port) (((port) << 4) | 0x03)

#define USB2_CLK_CFG0 0x100
#define USB2_CLK_CFG1 0x101
#define USB2_CLK_CFG2 0x102
#define USB2_CLK_CFG3 0x103

#define APHY_RPDEN 0x40

#define APHY_CALEN_DEV 0x10
#define APHY_NSQ_DEV 0x20
#define APHY_CALEN_HST 0x400
#define APHY_NSQ_HST 0x800
#define APHY_SD_CAL_DEV_SHIFT 0
#define APHY_SD_CAL_DEV_MASK 0x0F
#define APHY_SD_CAL_DEV_INIT (0x08 << APHY_SD_CAL_DEV_SHIFT)
#define APHY_SD_CAL_HST_SHIFT 6
#define APHY_SD_CAL_HST_MASK 0x3C0
#define APHY_SD_CAL_HST_INIT (0x08 << APHY_SD_CAL_HST_SHIFT)
#define APHY_CALEN_INIT (APHY_SD_CAL_HST_INIT | APHY_SD_CAL_DEV_INIT)

/*
 *    Bit 3:  MDO (MISO)
 *    Bit 2:  MDI OE (MOSI OE)
 *    Bit 1:  MDI (MOSI)
 *    Bit 0:  MDC
 */

#define MDI_H 0x02
#define MDI_L 0x00
#define MDO_H 0x08
#define MDO_L 0x00
#define MDC_H 0x01
#define MDC_L 0x00
#define MD_OE 0x04

#define MDC_MASK 0x01
#define MDI_MASK 0x02
#define MDO_MASK 0x08

/* Push data at falling edge */
#ifdef CONFIG_TARGET_FPGA
#define MDI_CLK_W(rts, init, bit)                     \
	do {                                          \
		(init) |= MD_OE;                      \
		(init) |= MDC_H;                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(1);                            \
		(init) &= ~(MDC_MASK | MDI_MASK);     \
		(init) |= (bit);                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(5);                            \
	} while (0)

/* Push data at rising edge */
#define MDI_CLK_W_RISING(rts, init, bit)              \
	do {                                          \
		(init) |= MD_OE;                      \
		(init) |= MDC_H;                      \
		writel(init, &(rts)->regs->phy_mdio); \
		(init) &= ~MDI_MASK;                  \
		(init) |= (bit);                      \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(1);                            \
		(init) &= ~MDC_MASK;                  \
		writel(init, &(rts)->regs->phy_mdio); \
		udelay(5);                            \
	} while (0)

/* Sample data at falling edge */
#define MDO_CLK_R(rts, init, bit)                         \
	do {                                              \
		u32 _tmp_val;                             \
		(init) &= ~MD_OE;                         \
		(init) |= MDC_H;                          \
		writel(init, &(rts)->regs->phy_mdio);     \
		udelay(1);                                \
		(init) &= ~(MDC_MASK | MDI_MASK);         \
		writel(init, &(rts)->regs->phy_mdio);     \
		_tmp_val = readl(&(rts)->regs->phy_mdio); \
		(bit) = (_tmp_val & MDO_H) ? 1 : 0;       \
		udelay(5);                                \
	} while (0)

static u16 mdio_read(struct rts_usb_phy *rts, u16 addr)
{
	u32 init = readl(&rts->regs->phy_mdio);
	u16 data = 0;
	int i;

	/* PRE */
	for (i = 0; i < 40; i++)
		MDI_CLK_W(rts, init, MDI_H);

	/* ST */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* OP */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Address */
	for (i = 9; i >= 0; i--) {
		int hi = (addr >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* TA */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Data */
	for (i = 15; i >= 0; i--) {
		u8 bit;

		MDO_CLK_R(rts, init, bit);
		data |= bit << i;
	}

	/* Idle */
	for (i = 0; i < 10; i++)
		MDI_CLK_W_RISING(rts, init, MDI_H);

	return data;
}

static void mdio_write(struct rts_usb_phy *rts, u16 addr, u16 val)
{
	u32 init = readl(&rts->regs->phy_mdio);
	int i;

	/* PRE */
	for (i = 0; i < 40; i++)
		MDI_CLK_W(rts, init, MDI_H);

	/* ST */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* OP */
	MDI_CLK_W(rts, init, MDI_L);
	MDI_CLK_W(rts, init, MDI_H);

	/* Address */
	for (i = 9; i >= 0; i--) {
		int hi = (addr >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* TA */
	MDI_CLK_W(rts, init, MDI_H);
	MDI_CLK_W(rts, init, MDI_L);

	/* Data */
	for (i = 15; i >= 0; i--) {
		int hi = (val >> i) & 0x01;

		if (hi)
			MDI_CLK_W(rts, init, MDI_H);
		else
			MDI_CLK_W(rts, init, MDI_L);
	}

	/* Idle */
	for (i = 0; i < 10; i++)
		MDI_CLK_W(rts, init, MDI_H);
}

/*
 * We will calibration from bit 3 to bit 0
 * idx stands for the bit index
 */
static void rts_usb_phy_calen_dev(struct rts_usb_phy *rts, int port, int idx)
{
	u32 val;
	u16 mask = 0x01 << idx;
	u8 nsq_dev;

	val = mdio_read(rts, PORT_CFG3(port));
	nsq_dev = !!(val & APHY_NSQ_DEV);
	val &= ~mask;
	val |= ((nsq_dev << idx) | (mask >> 1));
	mdio_write(rts, PORT_CFG3(port), val);
}

static void rts_usb_phy_calen_hst(struct rts_usb_phy *rts, int port, int idx)
{
	u32 val;
	u16 mask = 0x40 << idx;
	u8 nsq_hst;

	val = mdio_read(rts, PORT_CFG3(port));
	nsq_hst = !!(val & APHY_NSQ_HST);
	val &= ~mask;
	val |= ((nsq_hst << (idx + APHY_SD_CAL_HST_SHIFT)) |
		((mask >> 1) & APHY_SD_CAL_HST_MASK));
	mdio_write(rts, PORT_CFG3(port), val);
}

static void rts_usb_reset_aphy(struct rts_usb_phy *rts)
{
	u32 cfg, mask;

	cfg = readl(&rts->regs->aphy_cfg6);
	mask = UPHY_U2_APHY_RST_N;

	cfg &= ~mask;
	writel(cfg, &rts->regs->aphy_cfg6);
	mdelay(1);
	cfg |= mask;
	writel(cfg, &rts->regs->aphy_cfg6);
}

static void rts_usb_pd15k(struct rts_usb_phy *rts, int port)
{
	u16 val;

	val = mdio_read(rts, PORT_CFG0(port));
	val |= APHY_RPDEN;
	mdio_write(rts, PORT_CFG0(port), val);
	mdio_read(rts, PORT_CFG0(port));
}

static void rts_usb_disable_cali(struct rts_usb_phy *rts, int port, u16 calen)
{
	u16 val;

	val = mdio_read(rts, PORT_CFG3(port));
	val &= ~calen;
	mdio_write(rts, PORT_CFG3(port), val);
	mdio_read(rts, PORT_CFG3(port));
}

static int rts_init_fpga_phy(struct rts_usb_phy *rts)
{
	int i, port;
	u16 val;

	mutex_lock(&rts->mutex);

	/* Reset APHY */
	rts_usb_reset_aphy(rts);
	mdelay(100);

	/* Device calibration */
	for (port = 0; port < 2; port++) {
		mdio_write(rts, PORT_CFG3(port), APHY_CALEN_INIT);
		mdio_write(rts, PORT_CFG3(port),
			   APHY_CALEN_INIT | APHY_CALEN_DEV);

		for (i = 3; i >= 0; i--)
			rts_usb_phy_calen_dev(rts, port, i);

		rts_usb_disable_cali(rts, port, APHY_CALEN_DEV);
	}

	/* Pull down 15K resistor for downstream ports */
	rts_usb_pd15k(rts, HST_PORT);

	/* Host calibration */
	mdio_write(rts, PORT_CFG3(HST_PORT), APHY_CALEN_INIT);
	mdio_write(rts, PORT_CFG3(HST_PORT), APHY_CALEN_INIT | APHY_CALEN_HST);

	for (i = 3; i >= 0; i--)
		rts_usb_phy_calen_hst(rts, HST_PORT, i);

	rts_usb_disable_cali(rts, HST_PORT, APHY_CALEN_HST);

	/* fixed RLE0485 Z0 value 0b'1010 without Auto-Calibration */
	mdio_write(rts, USB2_CLK_CFG2, 0x128);

	/* Improve the RX sensitivity for downstream port */
	val = mdio_read(rts, PORT_CFG3(HST_PORT));
	val = ((val & 0x0F) < 0x0E) ? (val + 2) : (val | 0x0F);
	mdio_write(rts, PORT_CFG3(HST_PORT), val);

	mutex_unlock(&rts->mutex);
	return 0;
}
#endif

static int rts_usb_phy_handle_irq(struct rts_usb_phy *rts)
{
	u32 int_val;

	RTS_PHY_DEBUG("%s()\n", __func__);

	int_val = rts_usb_read_phy_int(rts);
	switch (int_val & UPHY_DEV_PORT_VBUS_INT_MSK) {
	case UPHY_DEV_PORT_VBUS_ON_INT:
		RTS_PHY_DEBUG(
			"clear usb phy power on interrupt 0x1824001c=0x%-8x\n",
			int_val);
		rts_usb_clear_phy_int(rts, int_val, 0x01);
		rts_phy_event(USB_EVENT_VBUS);
		break;
	case UPHY_DEV_PORT_VBUS_DOWN_INT:
		RTS_PHY_DEBUG(
			"clear usb phy power down interrupt 0x1824001c=0x%-8x\n",
			int_val);
		rts_usb_clear_phy_int(rts, int_val, 0x02);
		rts_phy_event(USB_EVENT_NONE);
		break;
	case UPHY_DEV_PORT_VBUS_INT_MSK:
		RTS_PHY_DEBUG(
			"usb phy power on/down interrupt, change VBUS deglich time!\n");
		rts_usb_clear_phy_int(rts, int_val, 0x00);
		rts_phy_event(USB_EVENT_NONE);
		break;
	}

	return 0;
}

int rts_usb_phy_init(u32 usb_base)
{
	struct rts_usb_phy rts_phy;

	RTS_PHY_DEBUG("%s()\n", __func__);

	mutex_init(&rts->mutex);

	rts_phy.regs = (struct rts_usb_phy_regs __iomem *)0x18140000;

	rts_phy.dev_regs =
		(struct rts_usb_dev_phy_regs __iomem *)(usb_base + 0x800);

#ifdef CONFIG_TARGET_FPGA
	rts_init_fpga_phy(&rts_phy);
#endif
	rts_usb_phy_handle_irq(&rts_phy);

	return 0;
}
