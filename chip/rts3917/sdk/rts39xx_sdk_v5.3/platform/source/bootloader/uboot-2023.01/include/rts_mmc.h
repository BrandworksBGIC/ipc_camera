/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Jaehoon Chung <jh80.chung@samsung.com>
 *
 */

#ifndef __DWMMC_HW_H
#define __DWMMC_HW_H

#include <asm/io.h>
#include <mmc.h>

#define RTS_MMC_DUMMY_F_MIN     250000
#define RTS_MMC_DUMMY_F_MAX     208000000
#define RTS_MMC_MAX_BLOCK_LEN   0x80

#define RTSX_SD_DEFAULT_MODE		0
#define RTSX_SD_HS_MODE			1
#define RTSX_SD_SDR_MODE		2
#define RTSX_SD_DDR_MODE		3
#define RTSX_ICR_STATE_IDLE		0
#define RTSX_ICR_STATE_RUN		1
#define RTSX_SD_POWER_OFF		0
#define RTSX_SD_POWER_ON		1

#define RTS_MMC_GPIO_DIR 0x18800000
#define RTS_MMC_GPIO_OUT 0x18800004
#define RTS_MMC_BASEADDR 0x18300000
#define SD0_PULLCTRL 0x188000c8
#define SD0_PULLCTRL_SETTING 0xa6aa

#define MMC_BUS_WIDTH_1	1
#define MMC_BUS_WIDTH_4	4
#define MMC_BUS_WIDTH_8	8

#define CMD_TYPE_CMD_BUFF	0x1
#define CMD_TYPE_DATA_READ	0x2
#define CMD_TYPE_DATA_WRITE	0x4
#define CMD_TYPE_DATA_SREAD	0x8
#define CMD_TYPE_DATA_SWRITE	0x10

#define CLK_ENABLE		0x1000000
#define CLK_BASE_ADDR	0x188d0000
#define SYS_CLK_REG(x)	((x) + CLK_BASE_ADDR)
#define CLK_CHANGE_REG		SYS_CLK_REG(0x00)
#define SD1_CHANGE_CLK		0x200
#define SD0_CHANGE_CLK		0x100
#define SD0_CRC_CLK_CFG_REG	SYS_CLK_REG(0xe0)
#define SD0_SAMPLE_CLK_CFG_REG	SYS_CLK_REG(0xe4)
#define SD0_PUSH_CLK_CFG_REG	SYS_CLK_REG(0xe8)
#define SD0_DDR_CLK_CFG_REG	SYS_CLK_REG(0xec)

#define SD1_CRC_CLK_CFG_REG	SYS_CLK_REG(0xf0)
#define SD1_SAMPLE_CLK_CFG_REG	SYS_CLK_REG(0xf4)
#define SD1_PUSH_CLK_CFG_REG	SYS_CLK_REG(0xf8)
#define SD1_DDR_CLK_CFG_REG	SYS_CLK_REG(0xfc)

#define CLOCK_SELECT_DIV2	(0x1 << 2)
#define SRC_USB_PLL_5	0
#define SRC_GPLL0_3	0x1
#define SRC_GPLL0_5	0x2
#define SRC_USB_PLL_3	0x3

struct rts_mmc_plat {
	u32 base_addr;
	struct mmc_config cfg;
	struct mmc mmc;
};

struct rts_mmc_host {
	struct mmc_config	cfg;
	int			id;
	u32			buswidth;
	u32			regs;

	void			*resv_buf;

	u32			cmd_idx;
	dma_addr_t		cmd_addr;
	void			*cmd_ptr;

	u32			sg_tbl_idx;
	dma_addr_t		sg_tbl_addr;
	void			*sg_tbl_ptr;

	u32			bier;
	u32			card_status;

	bool			removed;
	bool			short_dma_mode;

	const u32		*sd_pull_ctl_enable_tbl;
	const u32		*sd_pull_ctl_disable_tbl;

	u8			state;

	u32			cmd_timeout_ms;
	u32			data_timeout_ms;

	struct mmc		*mmc;

	bool			initial_mode;
	unsigned int		clock;
	unsigned int		max_clock;
	unsigned int		min_clock;
	u8			sd_mode;

	u32			data_errors;
};

static inline void rtsmmc_writel(struct rts_mmc_host *rtsmmc, int reg, u32 val)
{
	writel(val, rtsmmc->regs + reg);
}

static inline u32 rtsmmc_readl(struct rts_mmc_host *rtsmmc, int reg)
{
	return readl(rtsmmc->regs + reg);
}

#endif
