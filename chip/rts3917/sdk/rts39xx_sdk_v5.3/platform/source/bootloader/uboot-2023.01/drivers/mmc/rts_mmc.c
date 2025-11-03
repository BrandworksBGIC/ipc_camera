// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Realtek ipcam card reader
 *
 * Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include <common.h>
#include <dm.h>
#include <mmc.h>
#include <linux/errno.h>
#include <asm/unaligned.h>
#include "rts_mmc.h"
#include "rts_mmcregs.h"
#include <linux/bug.h>
#include <linux/delay.h>
#include <cpu_func.h>

#define clear_bit(addr, val)	writel((readl(addr) & ~(val)), addr)
#define set_bit(addr, val)	writel((readl(addr) | (val)), addr)

#define FORCE_BUS_SD_RESET		0x80
#define FORCE_BUS_SD1_RESET		0x200
#define FORCE_SD_CLK_ASYNC_RESET	0x020
#define FORCE_SD1_CLK_ASYNC_RESET	0x8000

#define RTS_FRR_SET(addr, mask)				\
do {							\
	u32 val;					\
	val = readl((addr));				\
	val |= (mask);					\
	writel(val, (addr));				\
} while (0)

#define RTS_FRR_CLR(addr, mask)				\
do {							\
	u32 val;					\
	val = readl((addr));				\
	val &= ~(mask);					\
	writel(val, (addr));				\
} while (0)

#define RTS_FORCE_RESET_AUTO(addr, mask)	RTS_FRR_SET(addr, mask)

#define RTS_FORCE_RESET(addr, mask)			\
do {							\
	RTS_FRR_SET(addr, mask);			\
	RTS_FRR_CLR(addr, mask);			\
} while (0)

#define rtsmmc_control_table(addr, val) (((u32)(addr) << 16) | (u8)(val))

#define RTSMMC_CMD_TIMEOUT_MS		50
#define RTSMMC_DAT_TIMEOUT_MS		10000

static const u32 rtsmmc_pull_ctl_enable_tbl[] = {
	rtsmmc_control_table(SDDAT_L_PULL_CTL, 0xAA),
	rtsmmc_control_table(SDCMD_PULL_CTL, 0xA9),
	0,
};

static const u32 rtsmmc_pull_ctl_disable_tbl[] = {
	rtsmmc_control_table(SDDAT_L_PULL_CTL, 0x55),
	rtsmmc_control_table(SDCMD_PULL_CTL, 0x95),
	0,
};

int rtsmmc_reg_read(struct rts_mmc_host *rtsmmc, u16 addr, u8 *data)
{
	u32 val = HAIMR_START_READ | ((u32)(addr & 0x3FFF) << 16);
	int i;

	rtsmmc_writel(rtsmmc, HAIMR, val);

	for (i = 0; i < REG_RW_RETRY_CNT; i++) {
		val = rtsmmc_readl(rtsmmc, HAIMR);
		if (!(val & HAIMR_START)) {
			if (data)
				*data = val & 0xFF;
			return 0;
		}
	}

	debug("read register 0x%02X failed\n", addr);
	return -ETIMEDOUT;
}

static int rtsmmc_reg_write(struct rts_mmc_host *rtsmmc, u16 addr,
			    u8 mask, u8 data)
{
	int i;
	u32 val = HAIMR_START_WRITE |
		((u32)(addr & 0x3FFF) << 16 | (u32)mask << 8 | (u32)data);

	rtsmmc_writel(rtsmmc, HAIMR, val);

	for (i = 0; i < REG_RW_RETRY_CNT; i++) {
		val = rtsmmc_readl(rtsmmc, HAIMR);
		if (!(val & HAIMR_START)) {
			if (data != (u8)val)
				return -EIO;
			return 0;
		}
	}

	debug("write register 0x%02X failed\n", addr);
	return -ETIMEDOUT;
}

void __rtsmmc_add_cmd(struct rts_mmc_host *rtsmmc, u8 cmd_type, u16 reg_addr,
		      u8 mask, u8 data)
{
	u32 val = 0;
	u32 *ptr = (u32 *)(rtsmmc->cmd_ptr) + rtsmmc->cmd_idx;

	WARN_ON(rtsmmc->cmd_idx >= CMD_NUM_MAX);

	val |= (u32)(cmd_type & 0x03) << 30;
	val |= (u32)(reg_addr & 0x3FFF) << 16 | (u32)mask << 8 | (u32)data;

	put_unaligned_le32(val, ptr);
	ptr++;
	rtsmmc->cmd_idx++;
}

static inline void rtsmmc_read(struct rts_mmc_host *rtsmmc, u16 reg_addr)
{
	__rtsmmc_add_cmd(rtsmmc, 0, reg_addr, 0, 0);
}

static inline void rtsmmc_write(struct rts_mmc_host *rtsmmc, u16 reg_addr,
				u8 mask, u8 data)
{
	__rtsmmc_add_cmd(rtsmmc, 1, reg_addr, mask, data);
}

static inline void rtsmmc_check(struct rts_mmc_host *rtsmmc, u16 reg_addr,
				u8 mask, u8 data)
{
	__rtsmmc_add_cmd(rtsmmc, 2, reg_addr, mask, data);
}

static int rts_mmc_get_cd(struct udevice *dev)
{
	struct rts_mmc_host *rtsmmc = dev_get_priv(dev);
	u32 val;

	val = rtsmmc_readl(rtsmmc, BIPR) & SD_EXIST;

	return val;
}

static inline void rtsmmc_init_cmd(struct rts_mmc_host *rtsmmc)
{
	rtsmmc->cmd_idx = 0;
}

static inline void rtsmmc_clear_error(struct rts_mmc_host *rtsmmc)
{
	rtsmmc_reg_write(rtsmmc, CARD_STOP, 0xff,
			 SD_STOP | MS_STOP | SD_CLEAR_ERROR | MS_CLEAR_ERROR);
	rtsmmc_reg_write(rtsmmc, SD_CFG1, SD_ASYNC_FIFO_CTL, SD_ASYNC_FIFO_RST);
}

static inline void rtsmmc_write_be32(struct rts_mmc_host *rtsmmc, u16 reg_addr,
				     u32 data)
{
	rtsmmc_write(rtsmmc, reg_addr,     0xFF, data >> 24);
	rtsmmc_write(rtsmmc, reg_addr + 1, 0xFF, data >> 16);
	rtsmmc_write(rtsmmc, reg_addr + 2, 0xFF, data >> 8);
	rtsmmc_write(rtsmmc, reg_addr + 3, 0xFF, data);
}

static inline void rtsmmc_reg_set_sd_cmd(struct rts_mmc_host *rtsmmc,
					 struct mmc_cmd *cmd)
{
	if (!cmd)
		return;

	rtsmmc_write(rtsmmc, SD_CMD0, 0xFF, 0x40 | cmd->cmdidx);
	rtsmmc_write_be32(rtsmmc, SD_CMD1, cmd->cmdarg);
}

static inline void rtsmmc_reg_get_sd_resp(struct rts_mmc_host *rtsmmc)
{
	int i;

	for (i = SD_CMD0; i < SD_CMD0 + 5; i++)
		rtsmmc_read(rtsmmc, i);
}

static inline void rtsmmc_reg_get_pp_resp(struct rts_mmc_host *rtsmmc)
{
	int i;

	for (i = PINGPONG_BUF; i < PINGPONG_BUF + 16; i++)
		rtsmmc_read(rtsmmc, i);
}

static inline void rtsmmc_reg_get_resp(struct rts_mmc_host *rtsmmc,
				       int resp_type)
{
	if (resp_type == SD_RSP_TYPE_R2)
		rtsmmc_reg_get_pp_resp(rtsmmc);
	else if (resp_type != SD_RSP_TYPE_R0)
		rtsmmc_reg_get_sd_resp(rtsmmc);
}

void rtsmmc_exec_cmd(struct rts_mmc_host *rtsmmc)
{
	u32 val = CMD_START | (rtsmmc->cmd_idx * 4 & CMD_LEN_MASK);

	rtsmmc_writel(rtsmmc, HCBAR, rtsmmc->cmd_addr);
	rtsmmc_writel(rtsmmc, HCBCTLR, val);
}

static inline void rtsmmc_stop_cmd(struct rts_mmc_host *rtsmmc)
{
	rtsmmc_writel(rtsmmc, HCBCTLR, CMD_STOP);
	rtsmmc_writel(rtsmmc, HDBCTLR, DATA_STOP);
}

static int rtsmmc_wait_transfer(struct rts_mmc_host *rtsmmc,
				int timeout, int cmdtype)
{
	int err = 0;
	u32 intflag;

	timeout *= 1000;

	do {
		intflag = rtsmmc_readl(rtsmmc, BIPR);

		if (cmdtype & CMD_TYPE_DATA_SREAD)
			if (intflag & CMD_DONE_INT &&
			    intflag & SDHOST_END_INT)
				cmdtype &= ~CMD_TYPE_DATA_SREAD;

		if (cmdtype & CMD_TYPE_DATA_SWRITE)
			if (intflag & CMD_DONE_INT &&
			    intflag & SDHOST_END_INT)
				cmdtype &= ~CMD_TYPE_DATA_SWRITE;

		if (cmdtype & CMD_TYPE_CMD_BUFF)
			if (intflag & CMD_DONE_INT)
				cmdtype &= ~CMD_TYPE_CMD_BUFF;

		if (cmdtype & CMD_TYPE_DATA_READ)
			if ((intflag & SDHOST_END_INT) &&
			    (intflag & DATA_DONE_INT))
				cmdtype &= ~CMD_TYPE_DATA_READ;

		if (cmdtype & CMD_TYPE_DATA_WRITE)
			if ((intflag & DATA_DONE_INT) &&
			    (intflag & SDHOST_END_INT))
				cmdtype &= ~CMD_TYPE_DATA_WRITE;

		if (cmdtype == 0)
			break;

		if (timeout == 0) {
			printf("timeout:%x\n", intflag);
			err = -ETIMEDOUT;
			break;
		}
		udelay(1);
		timeout--;

	} while (intflag & SD_EXIST);

	if ((intflag & SD_EXIST) == 0) {
		printf("nodev:%x\n", intflag);
		err = -ENODEV;
	} else {
		if (intflag & CARD_ERR_INT) {
			printf("card err:%x\n", intflag);
			err = -EIO;
		}

		if (intflag & TRANS_FAIL) {
			printf("trans fail:%x\n", intflag);
			err = -EIO;
		}
	}

	if (err < 0) {
		rtsmmc_stop_cmd(rtsmmc);
		// rtsmmc_dump_all_regs();
		// rtsmmc_dump_dma_table(rtsmmc);
	}

	rtsmmc_writel(rtsmmc, BIPR, intflag);

	return err;
}

static inline int rtsmmc_transfer_cmd_timeout(struct rts_mmc_host *rtsmmc,
					      int timeout, int cmdtype)
{
	rtsmmc_exec_cmd(rtsmmc);
	return rtsmmc_wait_transfer(rtsmmc, timeout, cmdtype);
}

static void *rtsmmc_get_data(struct rts_mmc_host *rtsmmc)
{
	return rtsmmc->cmd_ptr;
}

static void rtsmmc_get_resp(struct rts_mmc_host *rtsmmc, struct mmc_cmd *cmd,
			    u8 *buf, int resp_type)
{
	int i;

	if (resp_type == SD_RSP_TYPE_R2) {
		for (i = 0; i < 4; i++)
			cmd->response[i] = __get_unaligned_be32(buf + 4 * i);
		debug("response: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		      cmd->response[0], cmd->response[1],
		      cmd->response[2], cmd->response[3]);
	} else {
		cmd->response[0] = __get_unaligned_be32(buf);
		debug("response: 0x%08x\n", cmd->response[0]);
	}
}

static int rtsmmc_resp_status_index(int resp_type)
{
	if ((resp_type & SD_RSP_LEN_MASK) == SD_RSP_LEN_6)
		return 5;
	else if ((resp_type & SD_RSP_LEN_MASK) == SD_RSP_LEN_17)
		return 16;
	else
		return 0;
}

static int rtsmmc_check_resp(struct rts_mmc_host *rtsmmc, u8 *buf,
			     int resp_type)
{
	if ((buf[0] & 0xC0) != 0) {
		debug("invalid response bit: 0x%2x\n", buf[0]);
		return -EILSEQ;
	}

	if (!(resp_type & SD_NO_CHECK_CRC7)) {
		int stat_idx = rtsmmc_resp_status_index(resp_type);

		if (buf[stat_idx] & SD_ERR_CRC7) {
			debug("CRC7 error: 0x%02x\n", buf[stat_idx]);
			return -EILSEQ;
		}
	}

	return 0;
}

static int rtsmmc_resp_type(struct rts_mmc_host *rtsmmc,
			    struct mmc_cmd *cmd)
{
	switch (cmd->resp_type) {
	case MMC_RSP_NONE:
		return SD_RSP_TYPE_R0;
	case MMC_RSP_R1:
		return SD_RSP_TYPE_R1;
	case MMC_RSP_R1b:
		return SD_RSP_TYPE_R1b;
	case MMC_RSP_R2:
		return SD_RSP_TYPE_R2;
	case MMC_RSP_R3:
		return SD_RSP_TYPE_R3;
	default:
		debug("unknown cmd->flag\n");
		return -EINVAL;
	}
}

static int rtsmmc_resp_timeout(struct rts_mmc_host *rtsmmc, int resp_type)
{
	if (resp_type == SD_RSP_TYPE_R1b)
		return 10 * rtsmmc->cmd_timeout_ms;

	return rtsmmc->cmd_timeout_ms;
}

static int rtsmmc_send_cmd_get_resp(struct rts_mmc_host *rtsmmc,
				    struct mmc_cmd *cmd)
{
	int err = 0;
	u8 *ptr;
	int resp_type = rtsmmc_resp_type(rtsmmc, cmd);
	int timeout = rtsmmc_resp_timeout(rtsmmc, resp_type);

	if (resp_type < 0)
		return -EINVAL;

	debug("SD/MMC CMD %d, arg = 0x%08x resp_type %d\n", cmd->cmdidx,
	      cmd->cmdarg, resp_type);

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_reg_set_sd_cmd(rtsmmc, cmd);
	rtsmmc_write(rtsmmc, SD_CFG2, 0xFF, resp_type);
	rtsmmc_write(rtsmmc, CARD_DATA_SOURCE, 0x01, SRC_PINGPONG_BUF);
	rtsmmc_write(rtsmmc, SD_TRANSFER, 0xFF,
		     SD_TRANSFER_START | SD_TM_CMD_RSP);
	rtsmmc_check(rtsmmc, SD_TRANSFER, SD_TRANSFER_END | SD_STAT_IDLE,
		     SD_TRANSFER_END | SD_STAT_IDLE);
	rtsmmc_reg_get_resp(rtsmmc, resp_type);
	rtsmmc_read(rtsmmc, SD_STAT1);
	err = rtsmmc_transfer_cmd_timeout(rtsmmc, timeout, CMD_TYPE_CMD_BUFF);
	if (err)
		return err;

	if (resp_type == SD_RSP_TYPE_R0)
		return err;

	ptr = (u8 *)rtsmmc_get_data(rtsmmc) + 1; /* skip for check command */
	rtsmmc_get_resp(rtsmmc, cmd, ptr + 1, resp_type); /* skip opcode */

	return rtsmmc_check_resp(rtsmmc, ptr, resp_type);
}

static inline int rtsmmc_transfer_cmd(struct rts_mmc_host *rtsmmc, int cmdtype)
{
	return rtsmmc_transfer_cmd_timeout(rtsmmc,
		rtsmmc->cmd_timeout_ms, cmdtype);
}

static inline void rtsmmc_change_clock(void)
{
	u32 clk_rate_set = CLOCK_SELECT_DIV2 | SRC_USB_PLL_5;

	writel(SD0_CHANGE_CLK, CLK_CHANGE_REG);
	writel(CLK_ENABLE | clk_rate_set, SD0_CRC_CLK_CFG_REG);
	writel(CLK_ENABLE | clk_rate_set, SD0_SAMPLE_CLK_CFG_REG);
	writel(CLK_ENABLE | clk_rate_set, SD0_PUSH_CLK_CFG_REG);
	writel(0, CLK_CHANGE_REG);

	writel(SD1_CHANGE_CLK, CLK_CHANGE_REG);
	writel(CLK_ENABLE | clk_rate_set, SD1_CRC_CLK_CFG_REG);
	writel(CLK_ENABLE | clk_rate_set, SD1_SAMPLE_CLK_CFG_REG);
	writel(CLK_ENABLE | clk_rate_set, SD1_PUSH_CLK_CFG_REG);
	writel(0, CLK_CHANGE_REG);
}

static int rtsmmc_set_clock_asic(struct rts_mmc_host *rtsmmc,
				 unsigned int clock)
{
	int err = 0;
	unsigned int final_clock;
	u8 cfg_div = SD_CLK_DIVIDE_0;

	if (clock == 0) {
		err = rtsmmc_reg_write(rtsmmc, SD_BUS_STAT, 0xFF,
				       SD_CLK_TOGGLE_STOP);
		return err;
	}

	if (clock > rtsmmc->max_clock)
		clock = rtsmmc->max_clock;

	final_clock = clock;
	if (rtsmmc->initial_mode) {
		final_clock *= 128;
		cfg_div = SD_CLK_DIVIDE_128;
	} else {
		final_clock *= 2;
	}
	//set clock 48M
	rtsmmc_change_clock();

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_write(rtsmmc, SD_BUS_STAT, SD_CLK_TOGGLE_STOP, 0);
	rtsmmc_write(rtsmmc, CARD_CLK_EN, SD_CLK_EN, SD_CLK_EN);
	rtsmmc_write(rtsmmc, SD_CFG1, SD_CLK_DIVIDE_MASK, cfg_div);
	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);

	udelay(10);			/* wait clock stable */

	debug("clock %d final clock %d, cfg_div %d\n",
	      clock, final_clock, cfg_div);
	return err;
}

int rtsmmc_set_clock(struct rts_mmc_host *rtsmmc, unsigned int clock)
{
	int err = 0;

	if (clock && clock < rtsmmc->min_clock) {
		debug("set clock to %u\n", clock);
		return -EINVAL;
	}

	rtsmmc->initial_mode = (bool)(clock < CLK_1MHz);

	debug("set clock to %u %d\n", clock, rtsmmc->initial_mode);

#ifdef CONFIG_TARGET_FPGA
	err |= rtsmmc_set_clock_fpga(rtsmmc, clock);
#else
	err |= rtsmmc_set_clock_asic(rtsmmc, clock);
#endif
	if (!err)
		rtsmmc->clock = clock;

	return err;
}

static inline void rtsmmc_reg_set_data_len(struct rts_mmc_host *rtsmmc,
					   u16 blocks, u16 blksz)
{
	if (blocks == 0 || blksz == 0)
		return;

	rtsmmc_write(rtsmmc, SD_BLOCK_CNT_L, 0xFF, blocks);
	rtsmmc_write(rtsmmc, SD_BLOCK_CNT_H, 0xFF, blocks >> 8);
	rtsmmc_write(rtsmmc, SD_BYTE_CNT_L, 0xFF, blksz);
	rtsmmc_write(rtsmmc, SD_BYTE_CNT_H, 0xFF, blksz >> 8);
}

static void rtsmmc_add_sg_tbl(struct rts_mmc_host *rtsmmc, dma_addr_t addr,
			      unsigned int len, int end)
{
	u64 *ptr = (u64 *)(rtsmmc->sg_tbl_ptr) + rtsmmc->sg_tbl_idx;
	u64 val;
	u8 option = SG_VALID | SG_TRANS_DATA | (end ? SG_END : 0);

	WARN_ON(len > SG_LEN_MAX);

	if (len == SG_LEN_MAX)
		len = 0;		/* len = 0 means SG_LEN_MAX */

	val = ((u64)addr << 32) | ((u64)len << 16) | option;

	debug("Add adma item at %08X: %08x, %04x, %04x\n",
	      (unsigned int)ptr, (u32)(val >> 32), (u16)(val >> 16), (u16)val);

	put_unaligned_le64(val, ptr);
	rtsmmc->sg_tbl_idx++;
}

static void rtsmmc_dump_dma_table(struct rts_mmc_host *rtsmmc)
{
	u64 *ptr = (u64 *)(rtsmmc->sg_tbl_ptr);
	int i;

	for (i = 0; i < rtsmmc->sg_tbl_idx; i++, ptr++) {
		u64 val = get_unaligned_le64(ptr);

		debug("ADMA table at %08X: %08x, %04x, %04x\n",
		      rtsmmc->sg_tbl_addr + 8 * i,
		      (u32)(val >> 32), (u16)(val >> 16), (u16)val);
	}
}

int rtsmmc_transfer_data(struct rts_mmc_host *rtsmmc,
			 struct mmc_data *data, int write)
{
	int timeout = data->blocks >= 1 ? rtsmmc->data_timeout_ms :
		rtsmmc->cmd_timeout_ms;
	int err = 0;
	unsigned int cnt, blk_cnt, cnt_o;
	ulong start_addr, addr_o = 0;
	u32 dir = write ? DATA_WRITE : DATA_READ;
	u32 val = DATA_START | dir;

	blk_cnt = data->blocks;
	cnt_o = cnt = data->blocksize * blk_cnt;

	if (data->flags == MMC_DATA_READ) {
		start_addr = (unsigned int)data->dest;
		addr_o = start_addr;
	} else {
		start_addr = (unsigned int)data->src;
	}

	flush_cache((ulong)start_addr, cnt);

	rtsmmc->sg_tbl_idx = 0;
	do {
		if (blk_cnt <= 128) {
			cnt = data->blocksize * blk_cnt;
			rtsmmc_add_sg_tbl(rtsmmc, start_addr, cnt, 1);
			start_addr += cnt;
			break;
		}

		cnt = data->blocksize * 128;
		rtsmmc_add_sg_tbl(rtsmmc, start_addr, cnt, 0);
		start_addr += cnt;
		blk_cnt -= 128;

	} while (1);

	if (write)
		rtsmmc_reg_write(rtsmmc, SD_TRANSFER, 0xFF,
				 SD_TRANSFER_START | SD_TM_AUTO_WRITE_3);
	else
		rtsmmc_reg_write(rtsmmc, SD_TRANSFER, 0xFF,
				 SD_TRANSFER_START | SD_TM_AUTO_READ_2);

	rtsmmc_writel(rtsmmc, HDBAR, rtsmmc->sg_tbl_addr);
	rtsmmc_writel(rtsmmc, HDBCTLR, val);

	if (write)
		err = rtsmmc_wait_transfer(rtsmmc, timeout,
					   CMD_TYPE_DATA_WRITE);
	else
		err = rtsmmc_wait_transfer(rtsmmc, timeout, CMD_TYPE_DATA_READ);

	if (write == 0)
		invalidate_dcache_range((ulong)addr_o, addr_o + cnt_o);

	if (err < 0) {
		debug("\n%d: HDBAR = %08X -> %08X, HDBCTLR = %08X\n",
		      err, rtsmmc->sg_tbl_addr, rtsmmc_readl(rtsmmc, HDBAR),
		      rtsmmc_readl(rtsmmc, HDBCTLR));
		debug("error SDMAR = %08x, SDMACTLR = %08x\n",
		      rtsmmc_readl(rtsmmc, SDMAR),
		      rtsmmc_readl(rtsmmc, SDMACTLR));
		rtsmmc_dump_dma_table(rtsmmc);
		if (rtsmmc->data_errors++)
			rtsmmc->mmc->has_init = 0;
	} else if (rtsmmc->data_errors) {
		rtsmmc->data_errors = 0;
	}

	return err;
}

static int rtsmmc_write_long_data(struct rts_mmc_host *rtsmmc,
				  struct mmc_cmd *cmd, struct mmc_data *data)
{
	struct mmc *mmc = rtsmmc->mmc;
	int err = 0;
	u8 cfg2 = 0;

	err = rtsmmc_send_cmd_get_resp(rtsmmc, cmd);
	if (err < 0)
		return err;

	if (mmc->has_init == 0)
		cfg2 |= SD_NO_CHECK_CRC_TIMEOUT;

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_reg_set_data_len(rtsmmc, data->blocks, data->blocksize);
	rtsmmc_write(rtsmmc, SD_CFG2, 0xFF, cfg2 | SD_RSP_TYPE_R1_DATA);
	rtsmmc_write(rtsmmc, CARD_DATA_SOURCE, 0x01, SRC_RING_BUF);
	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);
	if (err) {
		rtsmmc_clear_error(rtsmmc);
		return err;
	}

	err = rtsmmc_transfer_data(rtsmmc, data, 1);
	if (err) {
		rtsmmc_clear_error(rtsmmc);
		return err;
	}

	return err;
}

static int rtsmmc_read_long_data(struct rts_mmc_host *rtsmmc,
				 struct mmc_cmd *cmd, struct mmc_data *data)
{
	struct mmc *mmc = rtsmmc->mmc;
	int resp_type = rtsmmc_resp_type(rtsmmc, cmd);
	int err = 0;
	u8 cfg2 = 0;

	if (resp_type < 0)
		return -EINVAL;

	if (mmc->has_init == 0)
		cfg2 |= SD_NO_CHECK_CRC_TIMEOUT;

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_reg_set_sd_cmd(rtsmmc, cmd);
	rtsmmc_reg_set_data_len(rtsmmc, data->blocks, data->blocksize);
	rtsmmc_write(rtsmmc, SD_CFG2, 0xFF, cfg2 | resp_type);
	rtsmmc_write(rtsmmc, CARD_DATA_SOURCE, 0x01, SRC_RING_BUF);
	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);
	if (err) {
		rtsmmc_clear_error(rtsmmc);
		return err;
	}

	err = rtsmmc_transfer_data(rtsmmc, data, 0);
	if (err) {
		rtsmmc_clear_error(rtsmmc);
		return err;
	}

	return 0;
}

static int rtsmmc_long_data_xfer(struct rts_mmc_host *rtsmmc,
				 struct mmc_cmd *cmd, struct mmc_data *data)
{
	int err = 0;
	int initial_mode = rtsmmc->initial_mode;
	u32 clock = rtsmmc->clock;

	debug("SD/MMC CMD %d, arg = 0x%08x blocks %d blocksize %d\n",
	      cmd->cmdidx, cmd->cmdarg, data->blocks, data->blocksize);

	if (initial_mode)
		rtsmmc_set_clock(rtsmmc, DATA_MIN_CLK);

	if (data->flags & MMC_DATA_WRITE)
		err = rtsmmc_write_long_data(rtsmmc, cmd, data);
	else
		err = rtsmmc_read_long_data(rtsmmc, cmd, data);

	if (initial_mode)
		rtsmmc_set_clock(rtsmmc, clock);

	return err;
}

static int __rtsmmc_do_pp_rw(struct rts_mmc_host *rtsmmc, u16 offset,
			     u8 *buf, int len, int read)
{
	int i, err = 0;
	u8 *ptr = buf;
	u16 reg = PINGPONG_BUF + offset;

	WARN_ON(len > CMD_NUM_MAX);

	rtsmmc_init_cmd(rtsmmc);
	if (read)
		for (i = 0; i < len; ++i)
			rtsmmc_read(rtsmmc, reg++);
	else
		for (i = 0; i < len; ++i)
			rtsmmc_write(rtsmmc, reg++, 0xFF, *ptr++);

	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);
	if (err < 0)
		return err;

	if (read)
		memcpy(ptr, rtsmmc_get_data(rtsmmc), len);

	return err;
}

static int __rtsmmc_pp_rw(struct rts_mmc_host *rtsmmc, u8 *buf, int len,
			  int read)
{
	int err = 0, i;
	int step = CMD_NUM_MAX;
	int q_len = len / step;
	int r_len = len % step;
	u8 *ptr = buf;

	if (!buf || len <= 0)
		return 0;

	WARN_ON(len > 512);

	for (i = 0; i < q_len; i++, ptr += step) {
		err = __rtsmmc_do_pp_rw(rtsmmc, ptr - buf, ptr, step, read);
		if (err)
			return err;
	}

	if (r_len) {
		err = __rtsmmc_do_pp_rw(rtsmmc, ptr - buf, ptr, r_len, read);
		if (err)
			return err;
	}

	return err;
}

int rtsmmc_pp_read(struct rts_mmc_host *rtsmmc, u8 *buf, int len)
{
	return __rtsmmc_pp_rw(rtsmmc, buf, len, true);
}

static int rtsmmc_read_short_data(struct rts_mmc_host *rtsmmc,
				  struct mmc_cmd *cmd, u8 *buf, int len)
{
	int err = 0;
	u8 trans_mode = SD_TM_NORMAL_READ;

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_reg_set_sd_cmd(rtsmmc, cmd);
	rtsmmc_reg_set_data_len(rtsmmc, 1, len);
	rtsmmc_write(rtsmmc, SD_CFG2, 0xFF, SD_RSP_TYPE_R1);
	if (trans_mode != SD_TM_AUTO_TUNING)
		rtsmmc_write(rtsmmc, CARD_DATA_SOURCE, 0x01, SRC_PINGPONG_BUF);
	rtsmmc_write(rtsmmc, SD_TRANSFER, 0xFF, SD_TRANSFER_START | trans_mode);
	rtsmmc_check(rtsmmc, SD_TRANSFER, SD_TRANSFER_END, SD_TRANSFER_END);
	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_DATA_SREAD);
	if (err)
		return err;

	return rtsmmc_pp_read(rtsmmc, buf, len);
}

int rtsmmc_pp_write(struct rts_mmc_host *rtsmmc, u8 *buf, int len)
{
	return __rtsmmc_pp_rw(rtsmmc, buf, len, false);
}

static int rtsmmc_write_short_data(struct rts_mmc_host *rtsmmc,
				   struct mmc_cmd *cmd, u8 *buf, int len)
{
	int err = 0;

	err = rtsmmc_send_cmd_get_resp(rtsmmc, cmd);
	if (err)
		return err;

	err = rtsmmc_pp_write(rtsmmc, buf, len);
	if (err)
		return err;

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_reg_set_sd_cmd(rtsmmc, cmd);
	rtsmmc_reg_set_data_len(rtsmmc, 1, len);
	rtsmmc_write(rtsmmc, SD_CFG2, 0xFF, SD_RSP_TYPE_R1_DATA);
	rtsmmc_write(rtsmmc, SD_TRANSFER, 0xFF,
		     SD_TRANSFER_START | SD_TM_AUTO_WRITE_3);
	rtsmmc_check(rtsmmc, SD_TRANSFER, SD_TRANSFER_END, SD_TRANSFER_END);
	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_DATA_SWRITE);
	if (err)
		return err;

	return err;
}

static int rtsmmc_short_data_xfer(struct rts_mmc_host *rtsmmc,
				  struct mmc_cmd *cmd, struct mmc_data *data)
{
	int err = 0;

	debug("SD/MMC CMD %d, arg = 0x%08x blocks %d blocksize %d\n",
	      cmd->cmdidx, cmd->cmdarg, data->blocks, data->blocksize);
	int initial_mode = rtsmmc->initial_mode;
	u32 clock = rtsmmc->clock;

	if (initial_mode)
		rtsmmc_set_clock(rtsmmc, DATA_MIN_CLK);

	if (data->flags & MMC_DATA_READ)
		err = rtsmmc_read_short_data(rtsmmc, cmd, (u8 *)data->dest,
					     data->blocksize);
	else
		err = rtsmmc_write_short_data(rtsmmc, cmd, (u8 *)data->src,
					      data->blocksize);

	if (initial_mode)
		rtsmmc_set_clock(rtsmmc, clock);
	return err;
}

/*
 * Sends a command out on the bus.  Takes the mmc pointer,
 * a command pointer, and an optional data pointer.
 */
static int rts_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			    struct mmc_data *data)
{
	struct rts_mmc_host *rtsmmc = dev_get_priv(dev);
	int err = 0;
	int data_size = 0;

	if (!rts_mmc_get_cd(dev)) {
		err = -NO_CARD_ERR;
		goto finish;
	}

	if (data)
		data_size = data->blocks * data->blocksize;

	if (!data) {
		err = rtsmmc_send_cmd_get_resp(rtsmmc, cmd);
	} else if (data_size >= 512) {
		err = rtsmmc_long_data_xfer(rtsmmc, cmd, data);
	} else {
		if (rtsmmc->short_dma_mode)
			err = rtsmmc_long_data_xfer(rtsmmc, cmd, data);

		if (!rtsmmc->short_dma_mode || err) {
			err = rtsmmc_short_data_xfer(rtsmmc, cmd, data);
			if (rtsmmc->short_dma_mode && !err) {
				rtsmmc->short_dma_mode = 0;
				debug("disable short dma mode\n");
			}
		}
	}

finish:

	return err;
}

int rtsmmc_set_bus_width(struct rts_mmc_host *rtsmmc,
			 unsigned char bus_width)
{
	int err = 0;
	u8 width[] = {
		[MMC_BUS_WIDTH_1] = SD_BUS_WIDTH_1BIT,
		[MMC_BUS_WIDTH_4] = SD_BUS_WIDTH_4BIT,
		[MMC_BUS_WIDTH_8] = SD_BUS_WIDTH_8BIT,
	};

	if (bus_width <= MMC_BUS_WIDTH_8)
		err = rtsmmc_reg_write(rtsmmc, SD_CFG1, 0x03, width[bus_width]);
	else
		err = -EINVAL;

	return err;
}

static int rts_mmc_set_ios(struct udevice *dev)
{
	struct rts_mmc_host *rtsmmc = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	int err = 0;

	if (rtsmmc_set_bus_width(rtsmmc, mmc->bus_width))
		debug("set bus width to %u\n", mmc->bus_width);

	/* Set the clock speed */
	if (mmc->clock) {
		err |= rtsmmc_set_clock(rtsmmc, mmc->clock);
		debug("set clock to %u err %d\n", mmc->clock, err);
	}

	return 0;
}

static int rts_mmc_get_ro(struct udevice *dev)
{
	struct rts_mmc_host *rtsmmc = dev_get_priv(dev);
	int ro = 0;
	u32 val;

	if (rtsmmc->removed)
		return -EINVAL;

	val = rtsmmc_readl(rtsmmc, BIPR);

	debug("BIPR = 0x%08x\n", val);

	if (val & SD_WRITE_PROTECT)
		ro = 1;

	return ro;
}

static const struct dm_mmc_ops rts_mmc_ops = {
	.send_cmd	= rts_mmc_send_cmd,
	.set_ios	= rts_mmc_set_ios,
	.get_cd		= rts_mmc_get_cd,
	.get_wp		= rts_mmc_get_ro,
};

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int rts_mmc_ofdata_to_platdata(struct udevice *dev)
{
	struct rts_mmc_plat *plat = dev_get_plat(dev);
	struct mmc_config *cfg;
	int ret;

	plat->base_addr = dev_read_addr(dev);
	cfg = &plat->cfg;
	cfg->name = "RTS MMC";
	cfg->host_caps = MMC_MODE_HC;
	ret = mmc_of_parse(dev, cfg);
	if (ret < 0) {
		debug("failed to parse host caps\n");
		return ret;
	}

	cfg->f_max = RTS_MMC_DUMMY_F_MAX;
	cfg->f_min = RTS_MMC_DUMMY_F_MIN;
	cfg->b_max = RTS_MMC_MAX_BLOCK_LEN;
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;

	return 0;
}
#endif

static void rts_gpio_direction_output(unsigned int offset, int value)
{
	set_bit(RTS_MMC_GPIO_DIR, offset);

	if (value)
		set_bit(RTS_MMC_GPIO_OUT, offset);
	else
		clear_bit(RTS_MMC_GPIO_OUT, offset);
}

static void rtsmmc_pull_ctl_enable(struct rts_mmc_host *rtsmmc)
{
	const u32 *tbl = rtsmmc->sd_pull_ctl_enable_tbl;

	while (*tbl & 0xFFFF0000) {
		rtsmmc_write(rtsmmc, *tbl >> 16, 0xFF, *tbl);
		tbl++;
	}
}

#ifdef CONFIG_TARGET_FPGA

static inline int rtsmmc_switch_voltage_fpga(u8 voltage)
{
	rts_gpio_direction_output(SD_VOTAGE_SWITCH_CTRL_C2, voltage);
	mdelay(50);
	return 0;
}

static int rtsmmc_power_on_fpga(struct rtsmmc_host *rtsmmc)
{
	int err;

	rtsmmc_init_cmd(rtsmmc);
	rtsmmc_write(rtsmmc, SD_AUTO_RESET_FIFO, AUTO_RESET_FIFO_EN,
			AUTO_RESET_FIFO_EN);

	rtsmmc_pull_ctl_enable(rtsmmc);

	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);
	if (err < 0)
		return err;

	rts_gpio_direction_output(SD_PULL_CTRL_C2, 1);
	rts_gpio_direction_output(SD_PWR_CTRL_C2, 1);

	mdelay(20);

	err = rtsmmc_reg_write(CARD_OE, SD_OUTPUT_EN, SD_OUTPUT_EN);
	if (err < 0)
		return err;

	return 0;
}

#else

static int rtsmmc_wait_voltage_stable_1(struct rts_mmc_host *rtsmmc)
{
	int err = 0;
	u8 stat = 0;

	mdelay(1);

	err = rtsmmc_reg_read(rtsmmc, SD_BUS_STAT, &stat);
	if (err < 0)
		return err;

	if (stat & SD_CMD_DATA_STATUS_MASK)
		return -EINVAL;

	err = rtsmmc_reg_write(rtsmmc, SD_BUS_STAT, 0xFF, SD_CLK_TOGGLE_STOP);

	return err;
}

static int rtsmmc_wait_voltage_stable_2(struct rts_mmc_host *rtsmmc)
{
	int err = 0;
	u8 stat = 0;
	u8 mask = SD_CMD_DATA_STATUS_MASK;

	mdelay(50);			/* wait 1.8V output stable */

	err = rtsmmc_reg_write(rtsmmc, SD_BUS_STAT, 0xFF, SD_CLK_TOGGLE_EN);
	if (err < 0)
		return err;

	mdelay(20);			/* wait card drive SD_DAT[3:0] high */

	err = rtsmmc_reg_read(rtsmmc, SD_BUS_STAT, &stat);
	if (err < 0)
		return err;

	if ((stat & mask) != mask) {
		debug("SD_BUS_STAT = 0x%x\n", stat);
		rtsmmc_reg_write(rtsmmc, SD_BUS_STAT,
				 SD_CLK_TOGGLE_EN | SD_CLK_TOGGLE_STOP, 0);
		rtsmmc_reg_write(rtsmmc, CARD_CLK_EN, 0xFF, 0);
		return -EINVAL;
	}

	return err;
}

static int rtsmmc_switch_voltage_asic(struct rts_mmc_host *rtsmmc, u8 voltage)
{
	int err = 0;

	if (voltage == VOLTAGE_OUTPUT_1V8) {
		err = rtsmmc_wait_voltage_stable_1(rtsmmc);
		if (err < 0)
			goto out;
	}

	if (voltage == VOLTAGE_OUTPUT_1V8)
		rtsmmc_reg_write(rtsmmc, H_SD_LDO_CFG0, 0xFF, SD_LDO_TUNE18);
	else
		rtsmmc_reg_write(rtsmmc, H_SD_LDO_CFG0, 0xFF, SD_LDO_TUNE33);

	if (voltage == VOLTAGE_OUTPUT_1V8) {
		err = rtsmmc_wait_voltage_stable_2(rtsmmc);
		if (err < 0)
			goto out;
	}

	err = rtsmmc_reg_write(rtsmmc, SD_BUS_STAT,
			       SD_CLK_TOGGLE_EN | SD_CLK_TOGGLE_STOP, 0);
out:
	return err;
}

static int rtsmmc_power_on_asic(struct rts_mmc_host *rtsmmc)
{
	int err = 0;
	u8 val, count = 0;

	rts_gpio_direction_output(SD_PWR_CTRL_C2, 1);

	rtsmmc_reg_write(rtsmmc, H_SD_LDO_CFG0, 0xFF, SD_LDO_TUNE33);
//	rtsmmc_reg_write(rtsmmc, H_SD_LDO_CTRL, 0xFF, SD_LDO_OCP3318_EN);
	rtsmmc_reg_write(rtsmmc, H_SD_LDO_CTRL, 0xFF,
			 SD_LDO_SSC_LDO_EN | SD_LDO_SSC_BIAS_EN
			 | SD_LDO_RESERVED_BIT1 | SD_LDO_LDO3318_POW);

	rtsmmc_reg_write(rtsmmc, H_SSC_PLL_POW, 0xFF, U_SSC_PLL_POW);

	rtsmmc_reg_read(rtsmmc, H_SD_CKUSABLE, &val);
	while (!(val & H_SD_CKUSABLE_CKUSABLE)) {
		udelay(500);
		rtsmmc_reg_read(rtsmmc, H_SD_CKUSABLE, &val);
		count++;

		if (count > 10) {
			printf("wait for PLL stable fail\n");
			return -EBUSY;
		}
	}

	rtsmmc_init_cmd(rtsmmc);

	rtsmmc_write(rtsmmc, SD_AUTO_RESET_FIFO, AUTO_RESET_FIFO_EN,
		     AUTO_RESET_FIFO_EN);
	rtsmmc_write(rtsmmc, CARD_CLK_EN, SD_CLK_EN, SD_CLK_EN);
	rtsmmc_pull_ctl_enable(rtsmmc);
	// REG32(SD0_PULLCTRL) = SD0_PULLCTRL_SETTING;
	writel(SD0_PULLCTRL_SETTING, SD0_PULLCTRL);

	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);
	if (err < 0)
		return err;

	udelay(150);

	err = rtsmmc_reg_write(rtsmmc, CARD_OE, SD_OUTPUT_EN, SD_OUTPUT_EN);
	if (err < 0)
		return err;

	return err;
}
#endif

static inline int rtsmmc_clk_enable(void)
{
	writel(CLK_ENABLE | readl(SD0_CRC_CLK_CFG_REG), SD0_CRC_CLK_CFG_REG);
	writel(CLK_ENABLE | readl(SD0_SAMPLE_CLK_CFG_REG),
	       SD0_SAMPLE_CLK_CFG_REG);
	writel(CLK_ENABLE | readl(SD0_PUSH_CLK_CFG_REG), SD0_PUSH_CLK_CFG_REG);

	writel(CLK_ENABLE | readl(SD1_CRC_CLK_CFG_REG), SD1_CRC_CLK_CFG_REG);
	writel(CLK_ENABLE | readl(SD1_SAMPLE_CLK_CFG_REG),
	       SD1_SAMPLE_CLK_CFG_REG);
	writel(CLK_ENABLE | readl(SD1_PUSH_CLK_CFG_REG), SD1_PUSH_CLK_CFG_REG);

	return 0;
}

#ifdef CONFIG_TARGET_FPGA
static int rtsmmc_set_timing_fpga(struct rts_mmc_host *rtsmmc,
				  unsigned char timing)
{
	int err = 0;

	rtsmmc_init_cmd(rtsmmc);

	switch (timing) {
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		rtsmmc->sd_mode = RTSX_SD_HS_MODE;
		rtsmmc_write(rtsmmc, SD_CFG1, SD_MODE_SEL_MASK, SD_20_MODE);
		rtsmmc_write(rtsmmc, SD_PUSH_POINT_CTL, TX_SD20_SEL_MASK,
			     TX_SD20_SEL_AHEAD_1_4);
		rtsmmc_write(rtsmmc, SD_SAMPLE_POINT_CTL, RX_SD20_SEL_MASK,
			     RX_SD20_SEL_DELAY_1_4);
		break;

	default:
		rtsmmc->sd_mode = RTSX_SD_DEFAULT_MODE;
		rtsmmc_write(rtsmmc, SD_CFG1, SD_MODE_SEL_MASK, SD_20_MODE);
		rtsmmc_write(rtsmmc, SD_PUSH_POINT_CTL, 0xFF, 0);
		rtsmmc_write(rtsmmc, SD_SAMPLE_POINT_CTL, RX_SD20_SEL_MASK,
			     RX_SD20_SEL_RISING);
		break;
	}

	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);

	return err;
}
#else
static int rtsmmc_set_timing_asic(struct rts_mmc_host *rtsmmc,
				  unsigned char timing)
{
	int err = 0;

	rtsmmc_init_cmd(rtsmmc);

	switch (timing) {
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		rtsmmc->sd_mode = RTSX_SD_HS_MODE;
		rtsmmc_write(rtsmmc, SD_CFG1, SD_MODE_SEL_MASK, SD_20_MODE);
		rtsmmc_write(rtsmmc, SD_PUSH_POINT_CTL, TX_SD20_SEL_MASK,
			     TX_SD20_SEL_AHEAD_1_4);
		rtsmmc_write(rtsmmc, SD_SAMPLE_POINT_CTL, RX_SD20_SEL_MASK,
			     RX_SD20_SEL_DELAY_1_4);
		break;

	default:
		rtsmmc->sd_mode = RTSX_SD_DEFAULT_MODE;
		rtsmmc_write(rtsmmc, SD_CFG1, SD_MODE_SEL_MASK, SD_20_MODE);
		rtsmmc_write(rtsmmc, SD_PUSH_POINT_CTL, 0xFF, 0);
		rtsmmc_write(rtsmmc, SD_SAMPLE_POINT_CTL, RX_SD20_SEL_MASK,
			     RX_SD20_SEL_RISING);
		break;
	}

	err = rtsmmc_transfer_cmd(rtsmmc, CMD_TYPE_CMD_BUFF);

	return err;
}
#endif

int rtsmmc_set_timing(struct rts_mmc_host *rtsmmc, int timing)
{
	int err = 0;
#ifdef CONFIG_TARGET_FPGA
		err = rtsmmc_set_timing_fpga(rtsmmc, timing);
#else
		err = rtsmmc_set_timing_asic(rtsmmc, timing);
#endif

	return err;
}

static int rtsmmc_power_on(struct rts_mmc_host *rtsmmc)
{
	int err;

	rtsmmc_clk_enable();
#ifdef CONFIG_TARGET_FPGA
		err = rtsmmc_power_on_fpga(rtsmmc);
		rtsmmc_set_timing(rtsmmc, 0);
		rtsmmc_switch_voltage_fpga(VOLTAGE_OUTPUT_3V3);
#else
		err = rtsmmc_power_on_asic(rtsmmc);
		rtsmmc_set_timing(rtsmmc, 0);
		rtsmmc_switch_voltage_asic(rtsmmc, VOLTAGE_OUTPUT_3V3);
#endif

	if (err)
		return err;

	return 0;
}

static int rts_mmc_init(struct mmc *mmc)
{
	int err = 0;
	struct rts_mmc_host *rtsmmc = mmc->priv;

	rtsmmc->sd_pull_ctl_enable_tbl = rtsmmc_pull_ctl_enable_tbl;
	rtsmmc->sd_pull_ctl_disable_tbl = rtsmmc_pull_ctl_disable_tbl;

	rtsmmc_writel(rtsmmc, OCP_WRAPPER_EN, OCP_WRAPPER_ENABLE);
	rtsmmc_writel(rtsmmc, HCBAR, rtsmmc->cmd_addr);

	rtsmmc->card_status = rtsmmc_readl(rtsmmc, BIPR) & SD_EXIST;

	if (rtsmmc->card_status & SD_EXIST) {
		err = rtsmmc_power_on(rtsmmc);
		if (err < 0) {
			printf("rtsmmc_power_on err\n");
			return err;
		}
	} else {
		printf("no card exist!\n");
		return -1;
	}

	err = rtsmmc_reg_write(rtsmmc, SDCMD_DRV_SEL, 0xFF,
			       SD_CMD_DRV_SEL | SD_CLK_DRV_SEL);
	if (err < 0)
		return err;

	err = rtsmmc_reg_write(rtsmmc, SDDAT_L_DRV_SEL, 0xFF,
			       SD_DAT0_DRV_SEL | SD_DAT1_DRV_SEL |
			       SD_DAT2_DRV_SEL | SD_DAT3_DRV_SEL);
	if (err < 0)
		return err;

	return mmc_init(mmc);
}

static int rts_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct rts_mmc_host *rtsmmc = dev_get_priv(dev);
	struct rts_mmc_plat *plat = dev_get_plat(dev);

	clear_bit(SYS_MEM_SD, SDIO_MEM_SD);
	mdelay(5);

	RTS_FORCE_RESET_AUTO(SYS_RESET_BASE, FORCE_BUS_SD_RESET);
	RTS_FORCE_RESET(FORCE_REG_ASYNC_RST, FORCE_SD_CLK_ASYNC_RESET);
	udelay(2);

	// clear_bit(SYS_MEM_SD, SDIO2_MEM_SD);
	// mdelay(5);

	// RTS_FORCE_RESET_AUTO(SYS_RESET_BASE, FORCE_BUS_SD1_RESET);
	// RTS_FORCE_RESET(FORCE_REG_ASYNC_RST, FORCE_SD1_CLK_ASYNC_RESET);
	// udelay(2);

	plat->mmc.priv = rtsmmc;
	upriv->mmc = &plat->mmc;
	rtsmmc->regs = plat->base_addr;
	rtsmmc->cfg = plat->cfg;
	rtsmmc->resv_buf = (void *)0x19010000;
	if (!rtsmmc->resv_buf)
		return -ENOMEM;
	rtsmmc->cmd_addr = (u32)rtsmmc->resv_buf;
	rtsmmc->cmd_ptr = (void *)(rtsmmc->cmd_addr);
	rtsmmc->sg_tbl_ptr = rtsmmc->cmd_ptr + CMD_BUF_LEN;
	rtsmmc->sg_tbl_addr = rtsmmc->cmd_addr + CMD_BUF_LEN;

	rtsmmc->cmd_timeout_ms = RTSMMC_CMD_TIMEOUT_MS;
	rtsmmc->data_timeout_ms = RTSMMC_DAT_TIMEOUT_MS;

	return rts_mmc_init(&plat->mmc);
}

static int rts_mmc_bind(struct udevice *dev)
{
	struct rts_mmc_plat *plat = dev_get_plat(dev);

	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static const struct udevice_id rts_mmc_ids[] = {
	{.compatible = "realtek,rts3917-sdhc"},
	{/* sentinel */}
};
#endif

U_BOOT_DRIVER(rts_mmc_drv) = {
	.name = "rts_mmc",
	.id = UCLASS_MMC,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = rts_mmc_ids,
	.of_to_plat = rts_mmc_ofdata_to_platdata,
	.plat_auto = sizeof(struct rts_mmc_plat),
#endif
	.bind = rts_mmc_bind,
	.probe = rts_mmc_probe,
	.ops = &rts_mmc_ops,
	.priv_auto = sizeof(struct rts_mmc_host),
};
