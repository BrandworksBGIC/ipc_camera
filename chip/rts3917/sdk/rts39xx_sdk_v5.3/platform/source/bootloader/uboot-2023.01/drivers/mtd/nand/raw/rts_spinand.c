// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2003-2013 Broadcom Corporation
 *
 * Copyright (c) 2009-2010 Micron Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/rawnand.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <malloc.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <nand.h>
#include "rts_spinand.h"

#include <configs/bspchip.h>

#define BUFSIZE (10 * 64 * 2048)
#define CACHE_BUF 2112

static u8 swdie_id;

#define CONFIG_MTD_SPINAND_RTSECC
// #define CONFIG_MTD_SPINAND_RTSECC_TEST
#ifdef CONFIG_MTD_SPINAND_RTSECC
static struct nand_ecclayout spinand_oob_64 = {
	.eccbytes = 23,
	.eccpos = {
		41, 42, 43, 44, 45, 46,
		47, 48, 49, 50, 51, 52,
		53, 54, 55, 56, 57, 58,
		59, 60, 61, 62, 63},
	.oobavail = 39,
	.oobfree = {
		{.offset = 2,
			.length = 39}
	}
};
#endif

static inline struct spinand_state *mtd_to_state(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct rts_spinand_info *info = (struct rts_spinand_info *)chip->priv;
	struct spinand_state *state =
	(struct spinand_state *)info->spinand_state;

	return state;
}

int sfnc_wait_ready(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;
	int res = 0;

	controller_map = info->base_addr;

	while (1) {
		if (DW_BIT_GET_UNSHIFTED(controller_map->sr, SPIC_READY))
			break;
	}

	return res;
}

/*
 * spinand_cmd - to process a command to send to the SPI Nand
 * Description:
 *    Set up the command buffer to send to the SPI controller.
 *    The command buffer has to initialized to 0.
 */
static int spinand_cmd_process(struct rts_spinand_info *info,
			       struct spinand_cmd *cmd)
{
	struct spi_nandflash_portmap *controller_map;
	int res;
	unsigned short i, read_data;
	unsigned int tmp;

	controller_map = info->base_addr;

	controller_map->ssienr = 0;

	controller_map->ucmd = cmd->cmd;

	controller_map->addr_len = cmd->n_addr;

	controller_map->uaddr = cmd->addr[0] |
	(cmd->addr[1] << 8) | (cmd->addr[2] << 16);

	read_data = controller_map->read_dummy_len;

	switch (cmd->cmd_type) {
		/*When command type is CMD_TYPE_1CH_RX,
		 *default read dummy is 0,
		 *but when spic frequency is high,
		 *we need add internal dummy 1 or 2,
		 *or read id error.
		 */
		/*When command type is CMD_TYPE_READ,
		 *default read dummy is 8,
		 *internal dummy range has a relationship
		 *with baudr.When baudr is large,range is large.
		 *internal read dummy central point is 3 - baudr.
		 */
	case CMD_TYPE_1CH_RX:
			controller_map->ctrlr0 = DATA_SINGLE_CH |
			ADDR_SINGLE_CH | RECEIVE_MODE;
#if (defined CONFIG_BOARD_RTS3917)
#ifdef CONFIG_TARGET_FPGA
			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2 + 1;
#else
			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2 +
			4 + controller_map->baudr;
#endif /* CONFIG_TARGET_FPGA */
#else
			// controller_map->read_dummy_len =
			// cmd->n_dummy * controller_map->baudr * 2 + 1;

			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2;
#endif
			break;

	case CMD_TYPE_1CH_TX:
			controller_map->ctrlr0 = DATA_SINGLE_CH |
			ADDR_SINGLE_CH | TRANSMIT_MODE;
			controller_map->program_dummy_len = cmd->n_dummy *
			controller_map->baudr * 2;
			break;

	case CMD_TYPE_READ:
			controller_map->ctrlr0 = info->spi_dev.read_type |
			RECEIVE_MODE;
#if (defined CONFIG_BOARD_RTS3917)
#ifdef CONFIG_TARGET_FPGA
			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2 +
			controller_map->baudr;
#else
			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2 +
			4 + controller_map->baudr;
#endif /* CONFIG_TARGET_FPGA */
#else
			controller_map->read_dummy_len =
			cmd->n_dummy * controller_map->baudr * 2 + 15;

#endif
			break;

	case CMD_TYPE_WRITE:
			controller_map->ctrlr0 = info->spi_dev.write_type |
			TRANSMIT_MODE;

			controller_map->program_dummy_len = cmd->n_dummy *
			controller_map->baudr * 2;

			break;
	}

	if (cmd->rx_buf)
		controller_map->data_len = cmd->n_rx;
	else if (cmd->tx_buf)
		controller_map->data_len = cmd->n_tx;
	else
		controller_map->data_len = 0;

	controller_map->da = 0;
	if (cmd->tx_buf) {
		for (i = 0; i < cmd->n_tx / 4; i++) {
			controller_map->dv = cmd->tx_buf[4 * i] |
				(cmd->tx_buf[4 * i + 1] << 8) |
				(cmd->tx_buf[4 * i + 2] << 16) |
				(cmd->tx_buf[4 * i + 3] << 24);
		}
		tmp = 0;
		if (cmd->n_tx % 4) {
			for (i = 0; i < cmd->n_tx % 4; i++)
				tmp = ((cmd->tx_buf[(cmd->n_tx / 4) * 4 + i] <<
				(i * 8)) & (0xff << (i * 8))) | tmp;
			controller_map->dv = tmp;
		}
	}

	controller_map->ssienr = 1;
	res = sfnc_wait_ready(info);

/*	printf("n_rx is %x\n", cmd->n_rx);*/
	controller_map->ssienr = 0;

	if (cmd->rx_buf)
		memcpy(cmd->rx_buf, (void *)controller_map->data_fifo, cmd->n_rx);

	controller_map->read_dummy_len = read_data;
	return res;
}

/*
 * spinand_read_id- Read SPI Nand ID
 * Description:
 * Read ID: read two ID bytes from the SPI Nand device
 */
static int spinand_read_id(struct rts_spinand_info *info, u8 *id)
{
	int retval;

	u8 nand_id[3];

	struct spinand_cmd cmd = {0};

	/*
	 *When reading id, Gigadevice need address,
	 *macronix and winbond do not need address.
	 */
	cmd.cmd = CMD_READ_ID;

	cmd.n_addr = 1;

	cmd.addr[0] = 0;

	cmd.n_rx = 3;

	cmd.rx_buf = &nand_id[0];

	cmd.tx_buf = NULL;

	cmd.cmd_type = CMD_TYPE_1CH_RX;

	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);

	if (retval < 0) {
		printf("spi nand flash error %d reading id\n", retval);

		return retval;
	}

	id[0] = nand_id[0];

	id[1] = nand_id[1];

	id[2] = nand_id[2];

	return retval;
}

/*
 * spinand_read_status- send command 0xf to the SPI Nand status register
 * Description:
 *    After read, write, or erase, the Nand device is expected to set the
 *    busy status.
 *    This function is to allow reading the status of the command: read,
 *    write, and erase.
 *    Once the status turns to be ready, the other status bits also are
 *    valid status bits.
 */
static int spinand_read_status(struct rts_spinand_info *info, uint8_t *status)
{
	struct spinand_cmd cmd = {0};
	int ret;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_STATUS;
	cmd.n_rx = 1;
	cmd.rx_buf = status;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_RX;
	cmd.n_dummy = 0;

	ret = spinand_cmd_process(info, &cmd);
	if (ret < 0)
		printf("err: %d read status register\n", ret);

	return ret;
}

#define MAX_WAIT_JIFFIES  (40 * HZ)
static int spinand_wait_ready(struct rts_spinand_info *info)
{
	int retval;
	u8 stat = 0;
	unsigned long ts;

	ts = get_timer(0);
	do {
		retval = spinand_read_status(info, &stat);
		if (retval < 0)
			return -1;
	} while ((stat & 0x1) && get_timer(ts) < 1000);

	if ((stat & 0x1) == 0)
		return 0;

	printf("get spi nand flash status no ready in 1 second\n");
	return -1;
}

/*ECC_EN of xt26g02b is different from other devices.
 *spinand_get_ecc_config and spinand_set_ecc_config is to config
 *ecc only for xt26g02b.
 */
static int spinand_get_ecc_config(struct rts_spinand_info *info, u8 *ecc)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_ECC;
	cmd.n_rx = 1;
	cmd.rx_buf = ecc;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_RX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d get ecc\n", retval);
	return retval;
}

static int spinand_set_ecc_config(struct rts_spinand_info *info, u8 *ecc)
{
	int retval;
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_WRITE_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_ECC;
	cmd.n_tx = 1;
	cmd.tx_buf = ecc;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d set ecc\n", retval);

	return retval;
}

/**
 * spinand_get_otp- send command 0xf to read the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_get_otp(struct rts_spinand_info *info, u8 *otp)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_OTP;
	cmd.n_rx = 1;
	cmd.rx_buf = otp;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_RX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d get otp\n", retval);
	return retval;
}

/**
 * spinand_set_otp- send command 0x1f to write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
static int spinand_set_otp(struct rts_spinand_info *info, u8 *otp)
{
	int retval;
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_WRITE_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_OTP;
	cmd.n_tx = 1;
	cmd.tx_buf = otp;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d set otp\n", retval);

	return retval;
}

static int spinand_get_status(struct rts_spinand_info *info, u8 *otp)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_STATUS;
	cmd.n_rx = 1;
	cmd.rx_buf = otp;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_RX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d get status\n", retval);
	return retval;
}

/**
 * spinand_sel_swdie- send command 0xc2 to select sw die
 * Description:
 */
static int spinand_sel_swdie(struct rts_spinand_info *info, u8 die_id)
{
	int retval;
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_SEL_SOFTWAREDIE;
	cmd.n_addr = 1;
	cmd.addr[0] = die_id;
	cmd.n_tx = 0;
	cmd.tx_buf = NULL;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d set otp\n", retval);

	return retval;
}

static int spinand_get_blocklock(struct rts_spinand_info *info, u8 *otp)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_READ_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_BLOCK_LOCK;
	cmd.n_rx = 1;
	cmd.rx_buf = otp;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_RX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d get blocklock\n", retval);
	return retval;
}

/**
 * spinand_set_blocklock- send command 0xf to set
 * the SPI Nand block lock register
 * Description:
 */
static int spinand_set_blocklock(struct rts_spinand_info *info, u8 otp)
{
	struct spinand_cmd cmd = {0};
	int retval;

	cmd.cmd = CMD_WRITE_REG;
	cmd.n_addr = 1;
	cmd.addr[0] = REG_BLOCK_LOCK;
	cmd.n_tx = 1;
	cmd.tx_buf = &otp;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	retval = spinand_cmd_process(info, &cmd);
	if (retval < 0)
		printf("error %d get otp\n", retval);
	return retval;
}

#ifdef CONFIG_MTD_SPINAND_RTSECC
/**
 * spinand_enable_ecc- send command 0x1f to
 * write the SPI Nand OTP register
 * Description:
 *   There is one bit( bit 0x10 ) to set or to
 *   clear the internal ECC.
 *   Enable chip internal ECC, set the bit to 1
 *   Disable chip internal ECC, clear the bit to 0
 */
int spinand_enable_ecc(struct rts_spinand_info *info)
{
	u8 id[3];
	u8 otp = 0;
	int retval = 0;

	retval = spinand_read_id(info, id);
	if (retval < 0)
		return retval;

	switch (id[1]) {
	case NAND_XT26G02B:
		retval = spinand_get_ecc_config(info, &otp);
		if (retval < 0)
			return retval;

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK)
			break;

		otp |= OTP_ECC_MASK;
		retval = spinand_set_ecc_config(info, &otp);
		if (retval < 0)
			return retval;

		retval = spinand_get_ecc_config(info, &otp);
		if (retval < 0)
			return retval;
		break;
	default:
		retval = spinand_get_otp(info, &otp);
		if (retval < 0)
			return retval;

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK)
			break;

		otp |= OTP_ECC_MASK;
		retval = spinand_set_otp(info, &otp);
		if (retval < 0)
			return retval;

		retval = spinand_get_otp(info, &otp);
		if (retval < 0)
			return retval;
	}
	return retval;
}

static int spinand_disable_ecc(struct rts_spinand_info *info)
{
	u8 id[3];
	u8 otp = 0;
	int retval = 0;

	retval = spinand_read_id(info, id);
	if (retval < 0)
		return retval;

	switch (id[1]) {
	case NAND_XT26G02B:
		retval = spinand_get_ecc_config(info, &otp);
		if (retval < 0)
			return retval;

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
			otp &= ~OTP_ECC_MASK;
			retval = spinand_set_ecc_config(info, &otp);
			if (retval < 0)
				return retval;
			retval = spinand_get_ecc_config(info, &otp);
			if (retval < 0)
				return retval;
		}
		break;
	default:
		retval = spinand_get_otp(info, &otp);
		if (retval < 0)
			return retval;

		if ((otp & OTP_ECC_MASK) == OTP_ECC_MASK) {
			otp &= ~OTP_ECC_MASK;
			retval = spinand_set_otp(info, &otp);
			if (retval < 0)
				return retval;

			retval = spinand_get_otp(info, &otp);
			if (retval < 0)
				return retval;
		}
	}
	return retval;
}
#endif

#ifdef CONFIG_MTD_SPINAND_RTSECC
static int spinand_enable_rtsecc_whenwrite(struct rts_spinand_info *info)
{
	int retval = 0;
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_write_ctrl  = WR_ECC_EN;

	return retval;
}

int spinand_disable_rtsecc_whenwrite(struct rts_spinand_info *info)
{
	int retval = 0;
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_write_ctrl  = WR_ECC_DIS;

	return retval;
}

static int spinand_enable_rtsecc_whenread(struct rts_spinand_info *info)
{
	int retval = 0;
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_read_ctrl |= RD_ECC_EN;
/*	controller_map->ecc_read_ctrl |= RD_ECC_DIS;*/
	return retval;
}

int spinand_disable_rtsecc_whenread(struct rts_spinand_info *info)
{
	int retval = 0;
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_read_ctrl  &= (~RD_ECC_EN);

	return retval;
}

static int spinand_set_rtsecc_threshold(struct rts_spinand_info *info,
					unsigned int threshold)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_read_ctrl &= (~ECC_THRESHOLD_MASK);

	controller_map->ecc_read_ctrl |= (threshold << 2);

	return 0;
}

static int spinand_disable_interrupt(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->imr = DISABLE_INTERRUPT;

	return 0;
}

static int spinand_get_rtsecc_status(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;
	/*printf("error status is %x, thres hold is %x\n",
	 * (controller_map->ecc_read_status),
	 * (controller_map->ecc_read_ctrl)>>2);
	 */

	/*if ((controller_map->ecc_read_status >> 1) > 0)
	 *	printf("ecc correct bit is %x, status is %x\n",
	 *	controller_map->ecc_read_status >> 1,
	 *	controller_map->ecc_read_status & 0x01);
	 */
	return (controller_map->ecc_read_status & 0x01);
}

static int spinand_clear_rtsecc_status(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->ecc_read_ctrl |= ECC_STATUS_CLEAR;

	return 0;
}
#endif

static int spinand_set_autogetfeat_count(struct rts_spinand_info *info,
					 unsigned short max_count)
{
	struct spi_nandflash_portmap *controller_map;

	unsigned short temp;

	controller_map = info->base_addr;

	temp = controller_map->auto_get_features_ctrl;
	temp = (temp & 0x10000) | max_count;
	controller_map->auto_get_features_ctrl = temp;

	return 0;
}

static int spinand_setautoread_para(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;
	/*internal dummy need config when spic frequency is high,
	 *according to test result,internal dummy central point
	 *is 3 - baudr.
	 *but in 3915 ic internal read dummy central point
	 *is equal to baudr.
	 */
#if (defined CONFIG_BOARD_RTS3917)
#ifdef CONFIG_TARGET_FPGA
	controller_map->baudr = 1;
	controller_map->read_dummy_len =
	info->spi_dev.read_dummy * controller_map->baudr * 2 +
	controller_map->baudr;

	controller_map->getfeature_dummy_len = 1;
#else
#ifdef CONFIG_FAST_BOOT
	controller_map->baudr = 2;
#else
	controller_map->baudr = 8;
#endif
	controller_map->read_dummy_len =
	info->spi_dev.read_dummy * controller_map->baudr * 2  +
	4 + controller_map->baudr;

	/*when spic frequency is high,this register need
	 *config to solve the problem of polling flash status
	 *error.This value central point is 4 + controller_map->baudr.
	 */
	controller_map->getfeature_dummy_len =  4 + controller_map->baudr;

#endif /*CONFIG_TARGET_FPGA */
#else
	controller_map->read_dummy_len =
	info->spi_dev.read_dummy * controller_map->baudr * 2 +
	3 - controller_map->baudr;

	controller_map->getfeature_dummy_len = 1;
#endif
	controller_map->random_read_cmd = info->spi_dev.read_cmd;

	if (info->spi_dev.read_type & DATA_QUAD_CH)
		controller_map->auto_read_type = AUTO_READ_ADDR1_DATA4;
	else if (info->spi_dev.read_type & DATA_DUAL_CH)
		controller_map->auto_read_type = AUTO_READ_ADDR1_DATA2;
	else
		controller_map->auto_read_type = AUTO_READ_ADDR1_DATA1;

/*	printf("read dummy length is %x, %x",
 *	info->spi_dev.read_dummy, controller_map->read_dummy_len);
 */

	return 0;
}

static int spinand_setplane(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;

	controller_map = info->base_addr;

	controller_map->plane_ctrl = info->spi_dev.flags & 0x01;

/*	printf("read dummy length is %x, %x", info->spi_dev.read_dummy,
 *	controller_map->read_dummy_len);
 */

	return 0;
}

/**
 * spinand_write_enable- send command 0x06 to enable write or erase the
 * Nand cells
 * Description:
 *   Before write and erase the Nand cells, the write enable has to be set.
 *   After the write or erase, the write enable bit is automatically
 *   cleared (status register bit 2)
 *   Set the bit 2 of the status register has the same effect
 */
static int spinand_write_enable(struct rts_spinand_info *info)
{
	struct spinand_cmd cmd = {0};

	cmd.cmd = CMD_WR_ENABLE;
	cmd.n_addr = 0;
	cmd.tx_buf = NULL;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}

static int spinand_read_page_to_cache(struct rts_spinand_info *info,
				      u32 page_id)
{
	struct spinand_cmd cmd = {0};
	u32 row;

	row = page_id;
	cmd.cmd = CMD_READ;
	cmd.n_addr = 3;
	cmd.addr[2] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[0] = (u8)(row & 0x00ff);
	cmd.tx_buf = NULL;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}

/*
 * spinand_read_from_cache- send command 0x03 to read out the data from the
 * cache register(2112 bytes max)
 * Description:
 *   The read can specify 1 to 2112 bytes of data read at the corresponding
 *   locations.
 *   No tRd delay.
 */
static int spinand_read_from_cache(struct rts_spinand_info *info, u32 page_id,
				   u16 byte_id, u16 len, u8 *rbuf)
{
	struct spinand_cmd cmd = {0};
	u16 column;

	column = byte_id;
	cmd.cmd = info->spi_dev.read_cmd;
	cmd.n_addr = 2;
	cmd.addr[1] = (u8)((column & 0xff00) >> 8);
	/*set plane bit, most 16 plane*/
	cmd.addr[1] |= (u8)(((page_id >> 6) & 0x1 & info->spi_dev.flags) << 4);
	cmd.addr[0] = (u8)(column & 0x00ff);
	cmd.n_rx = len;
	cmd.rx_buf = rbuf;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_READ;
	cmd.n_dummy = info->spi_dev.read_dummy;

	return spinand_cmd_process(info, &cmd);
}

/*
 * spinand_read_page-to read a page with:
 * @page_id: the physical page number
 * @offset:  the location from 0 to 2111
 * @len:     number of bytes to read
 * @rbuf:    read buffer to hold @len bytes
 *
 * Description:
 *   The read includes two commands to the Nand: 0x13 and 0x03 commands
 *   Poll to read status to wait for tRD time.
 */
static int spinand_read_page(struct rts_spinand_info *info, u32 page_id,
			     u16 offset, u16 len, u8 *rbuf)
{
	int ret;

	if (info->spi_dev.flags & SW_DIE_WITH2) {
		if (swdie_id != ((page_id >> 16) & 0x01)) {
			swdie_id = ((page_id >> 16) & 0x01);
			spinand_sel_swdie(info, swdie_id);
		}
		page_id &= 0xffff;
	}

#ifdef CONFIG_MTD_SPINAND_RTSECC
	// spinand_disable_rtsecc_whenread(info);
#endif

	ret = spinand_read_page_to_cache(info, page_id);

	if (ret < 0)
		return ret;

	if (spinand_wait_ready(info))
		printf("WAIT timedout!!!\n");

	ret = spinand_read_from_cache(info, page_id, offset, len, rbuf);

#ifdef CONFIG_MTD_SPINAND_RTSECC

		if ((spinand_get_rtsecc_status(info)) != RD_ECC_SUCCESS) {
			printf("ecc error, page=%d\n", page_id);
			ret = -1;
	}
	spinand_clear_rtsecc_status(info);

#else
	if ((status & STATUS_ECC_MASK) == STATUS_ECC_ERROR) {
		printf("ecc error, page=%d\n", page_id);
		ret = -1;
	}
#endif

	// spinand_enable_rtsecc_whenread(info);
	if (ret < 0)
		printf("read from cache failed!!\n");

	return ret;
}

/*
 * spinand_program_data_to_cache--to write a page to cache with:
 * @byte_id: the location to write to the cache
 * @len:     number of bytes to write
 * @rbuf:    read buffer to hold @len bytes
 *
 * Description:
 *   The write command used here is 0x84--indicating
 *   that the cache is not cleared first.
 *   Since it is writing the data to cache, there is no tPROG time.
 */
static int spinand_program_data_to_cache(struct rts_spinand_info *info,
					 u32 page_id,
					 u16 byte_id,
					 u16 len,
					 u8 *wbuf)
{
	struct spinand_cmd cmd = {0};
	u16 column;

	column = byte_id;
	cmd.cmd = info->spi_dev.write_cmd;
	cmd.n_addr = 2;
	cmd.addr[1] = (u8)((column & 0xff00) >> 8);
	cmd.addr[1] |= (u8)(((page_id >> 6) & 0x1 & info->spi_dev.flags) << 4);
	cmd.addr[0] = (u8)(column & 0x00ff);
	cmd.n_tx = len;
	cmd.tx_buf = wbuf;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_WRITE;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}

#ifdef CONFIG_MTD_SPINAND_RTSECC_TEST
/*
 * spinand_program_data_to_cache--to write a page to cache with:
 * @byte_id: the location to write to the cache
 * @len:     number of bytes to write
 * @rbuf:    read buffer to hold @len bytes
 *
 * Description:
 *   The write command used here is 0x84--indicating that the cache is
 *   not cleared first.
 *   Since it is writing the data to cache, there is no tPROG time.
 */

static int spinand_program_data_to_cache_random(struct rts_spinand_info *info,
						u32 page_id,
						u16 byte_id,
						u16 len,
						u8 *wbuf)
{
	struct spinand_cmd cmd = {0};
	u16 column;

	column = byte_id;
	cmd.cmd = CMD_PROG_PAGE;
	cmd.n_addr = 2;
	cmd.addr[1] = (u8)((column & 0xff00) >> 8);
	cmd.addr[1] |= (u8)(((page_id >> 6) & 0xf) << 4);
	cmd.addr[0] = (u8)(column & 0x00ff);
	cmd.n_tx = len;
	cmd.tx_buf = wbuf;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_WRITE;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}
#endif

/**
 * spinand_program_execute--to write a page from cache to the Nand array with
 * @page_id: the physical page location to write the p *
 * Description:
 *   The write command used here is 0x10--indicating the cache is writing to
 *   the Nand array.
 *   Need to wait for tPROG time to finish the transaction.
 */
static int spinand_program_execute(struct rts_spinand_info *info, u32 page_id)
{
	struct spinand_cmd cmd = {0};
	u32 row;

	row = page_id;
	cmd.cmd = CMD_PROG_PAGE_EXC;
	cmd.n_addr = 3;
	cmd.addr[2] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[0] = (u8)(row & 0x00ff);
	cmd.rx_buf = NULL;
	cmd.tx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}

/**
 * spinand_program_page--to write a page with:
 * @page_id: the physical page location to write the page.
 * @offset:  the location from the cache starting from 0 to 2111
 * @len:     the number of bytes to write
 * @wbuf:    the buffer to hold the number of bytes
 *
 * Description:
 *   The commands used here are 0x06, 0x84, and 0x10--indicating that
 *   the write enable is first sent, the write cache command, and the
 *   write execute command.
 *   Poll to wait for the tPROG time to finish the transaction.
 */
static int spinand_program_page(struct rts_spinand_info *info,
				u32 page_id, u16 offset, u16 len, u8 *buf)
{
	int retval;
	u8 status = 0;
	u8 *wbuf;

	if (info->spi_dev.flags & SW_DIE_WITH2) {
		if (swdie_id != ((page_id >> 16) & 0x01)) {
			swdie_id = ((page_id >> 16) & 0x01);
			spinand_sel_swdie(info, swdie_id);
		}
		page_id &= 0xffff;
	}

#ifdef CONFIG_MTD_SPINAND_RTSECC
	unsigned int i;

	wbuf = malloc(CACHE_BUF);
	memset(wbuf, 0xff, CACHE_BUF);

	for (i = 0; i < len; i++)
		wbuf[i + offset] &= buf[i];

	len = 2112;
	offset = 0;
#else
	wbuf = buf;
#endif
	retval = spinand_write_enable(info);
	if (retval < 0) {
		printf("write enable failed!!\n");
		return retval;
	}
	if (spinand_wait_ready(info))
		printf("wait timedout!!!\n");
	retval = spinand_program_data_to_cache(info, page_id,
					       offset, len, wbuf);
#ifdef CONFIG_MTD_SPINAND_RTSECC_TEST
	wbuf[0] = 0x0;
	offset = 0;
	len = 2;
	retval = spinand_program_data_to_cache_random(info, page_id,
						      offset, len, wbuf);
#endif

	if (retval < 0)
		return retval;
	retval = spinand_program_execute(info, page_id);
	if (retval < 0)
		return retval;
	while (1) {
		retval = spinand_read_status(info, &status);
		if (retval < 0) {
			printf("error %d reading status register\n", retval);
			return retval;
		}

		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_P_FAIL_MASK) == STATUS_P_FAIL) {
				printf("program error, page %d\n", page_id);
				return -1;
			}
			break;
		}
	}

	free(wbuf);
	return 0;
}

/**
 * spinand_erase_block_erase--to erase a page with:
 * @block_id: the physical block location to erase.
 *
 * Description:
 *   The command used here is 0xd8--indicating an erase command to erase
 *   one block--64 pages
 *   Need to wait for tERS.
 */
static int spinand_erase_block_erase(struct rts_spinand_info *info,
				     u32 block_id)
{
	struct spinand_cmd cmd = {0};
	u32 row;

	row = block_id;
	cmd.cmd = CMD_ERASE_BLK;
	cmd.n_addr = 3;
	cmd.addr[2] = (u8)((row & 0xff0000) >> 16);
	cmd.addr[1] = (u8)((row & 0xff00) >> 8);
	cmd.addr[0] = (u8)(row & 0x00ff);
	cmd.tx_buf = NULL;
	cmd.rx_buf = NULL;
	cmd.cmd_type = CMD_TYPE_1CH_TX;
	cmd.n_dummy = 0;

	return spinand_cmd_process(info, &cmd);
}

/**
 * spinand_erase_block--to erase a page with:
 * @block_id: the physical block location to erase.
 *
 * Description:
 *   The commands used here are 0x06 and 0xd8--indicating an erase
 *   command to erase one block--64 pages
 *   It will first to enable the write enable bit (0x06 command),
 *   and then send the 0xd8 erase command
 *   Poll to wait for the tERS time to complete the tranaction.
 */
static int spinand_erase_block(struct rts_spinand_info *info, u32 block_id)
{
	int retval;
	u8 status = 0;

	if (info->spi_dev.flags & SW_DIE_WITH2) {
		if (swdie_id != ((block_id >> 16) & 0x01)) {
			swdie_id = ((block_id >> 16) & 0x01);
			spinand_sel_swdie(info, swdie_id);
		}
		block_id &= 0xffff;
	}

	spinand_set_blocklock(info, 0);

	retval = spinand_write_enable(info);

	if (spinand_wait_ready(info))
		printf("WAIT timedout!!!\n");

	retval = spinand_erase_block_erase(info, block_id);
	while (1) {
		retval = spinand_read_status(info, &status);
		if (retval < 0) {
			printf("error %d reading status register\n",
			       (int)retval);
			return retval;
		}

		if ((status & STATUS_OIP_MASK) == STATUS_READY) {
			if ((status & STATUS_E_FAIL_MASK) == STATUS_E_FAIL) {
				printf("erase error, block %d\n",
				       block_id >> 6);
				return -1;
			}
			break;
		}
	}
	return retval;
}

static int spinand_blocklock_reg(struct rts_spinand_info *info, u8 otp)
{
	int retval = 0;
	u8 status;

	spinand_write_enable(info);
	if (spinand_wait_ready(info))
		printf("wait timedout!!!\n");

	if (info->spi_dev.flags & SW_DIE_WITH2) {
		swdie_id = 1;
		spinand_sel_swdie(info, swdie_id);
		spinand_set_blocklock(info, otp);
		swdie_id = 0;
		spinand_sel_swdie(info, swdie_id);
	}
	retval = spinand_set_blocklock(info, otp);

	spinand_get_blocklock(info, &status);

	return retval;
}

static uint8_t rts_spinand_read_byte(struct mtd_info *mtd)
{
	struct spinand_state *state = mtd_to_state(mtd);
	u8 data;

	data = state->buf[state->buf_ptr];
	state->buf_ptr++;
	return data;
}

static void rts_spinand_write_buf(struct mtd_info *mtd,
				  const u8 *buf, int len)
{
	struct spinand_state *state = mtd_to_state(mtd);

	memcpy(state->buf + state->buf_ptr, buf, len);
	state->buf_ptr += len;
}

static void rts_spinand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct spinand_state *state = mtd_to_state(mtd);

	memcpy(buf, state->buf + state->buf_ptr, len);
	state->buf_ptr += len;
}

/*
 * spinand_reset- send RESET command "0xff" to the Nand device.
 */
static void spinand_reset(struct rts_spinand_info *info)
{
	struct spi_nandflash_portmap *controller_map;
	struct spinand_cmd cmd = {0};

	controller_map = info->base_addr;
	cmd.cmd = CMD_RESET;
	controller_map->read_dummy_len = 0;
	if (spinand_cmd_process(info, &cmd) < 0)
		pr_info("spinand reset failed!\n");

	/* elapse 1ms before issuing any other command */
	udelay(1000);

	if (spinand_wait_ready(info))
		printf("wait timedout!\n");
}

static void rts_spinand_cmdfunc(struct mtd_info *mtd,
				unsigned int command,
				int column,
				int page_addr)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct rts_spinand_info *info = (struct rts_spinand_info *)chip->priv;
	struct spinand_state *state = (struct spinand_state *)info->spinand_state;

	switch (command) {
	/*
	 * READ0 - read in first  0x800 bytes
	 */
	case NAND_CMD_READ1:
	case NAND_CMD_READ0:
		state->buf_ptr = 0;
		spinand_read_page(info, page_addr, 0x0, 0x840, state->buf);
		break;
	/* READOOB reads only the OOB because no ECC is performed. */
	case NAND_CMD_READOOB:
		state->buf_ptr = 0;
		spinand_read_page(info, page_addr, 0x800, 0x40, state->buf);
		break;
	case NAND_CMD_RNDOUT:
		state->buf_ptr = column;
		break;
	case NAND_CMD_READID:
		state->buf_ptr = 0;
		spinand_read_id(info, (u8 *)state->buf);
		break;
	case NAND_CMD_PARAM:
		state->buf_ptr = 0;
		break;
	/* ERASE1 stores the block and page address */
	case NAND_CMD_ERASE1:
		spinand_erase_block(info, page_addr);
		break;
	/* ERASE2 uses the block and page address from ERASE1 */
	case NAND_CMD_ERASE2:
		break;
	/* SEQIN sets up the addr buffer and all
	 * registers except the length
	 */
	case NAND_CMD_SEQIN:
		state->col = column;
		state->row = page_addr;
		state->buf_ptr = 0;
		break;
	/* PAGEPROG reuses all of the setup from SEQIN and adds the length */
	case NAND_CMD_PAGEPROG:
		spinand_program_page(info, state->row, state->col,
				     state->buf_ptr, state->buf);
		break;
	case NAND_CMD_OTP:
		state->buf_ptr = 0;
		spinand_get_otp(info, state->buf);
		if (!(state->buf[0] & 0x80))
			state->buf[0] = 0x80;
		state->buf_ptr = 0;
		break;
	case NAND_CMD_STATUS:
		state->buf_ptr = 0;
		spinand_get_status(info, state->buf);
		break;
	/* RESET command */
	case NAND_CMD_RESET:
		if (spinand_wait_ready(info))
			printf("WAIT timedout!!!\n");
		/* a minimum of 250us must elapse before issuing RESET cmd */
		udelay(250);
		spinand_reset(info);
		break;
	case NAND_CMD_BLOCKLOCK:
		spinand_blocklock_reg(info, column);
		break;
	default:
		printf("Unknown CMD: 0x%x\n", command);
		break;
	}
}

/*
 * Nominally, the purpose of this function is to look for or create the bad
 * block table. In fact, since the we call this function at the very end of
 * the initialization process started by nand_scan(), and we doesn't have a
 * more formal mechanism, we "hook" this function to continue init process.
 *
 * At this point, the physical NAND Flash chips have been identified and
 * counted, so we know the physical geometry. This enables us to make some
 * important configuration decisions.
 *
 * The return value of this function propogates directly back to this driver's
 * call to nand_scan(). Anything other than zero will cause this driver to
 * tear everything down and declare failure.
 */
static int rts_spinand_scan_bbt(struct mtd_info *mtd)
{
	/* We use the reference implementation for bad block management. */
	return nand_default_bbt(mtd);
}

static int rts_spinand_block_bad(struct mtd_info *mtd, loff_t ofs)
{
	return 0;
}

/*
 * Select the NAND chip.
 */
static void rts_spinand_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *nand = mtd_to_nand(mtd);
	struct rts_spinand_info *nand_info = nand->priv;

	nand_info->cur_chip = chip;
}

/*
 * This is the function that we install in the cmd_ctrl function pointer of the
 * owning struct nand_chip. The only functions in the reference implementation
 * that use these functions pointers are cmdfunc and select_chip.
 *
 * In this driver, we implement our own select_chip, so this function will only
 * be called by the reference implementation's cmdfunc. For this reason, we can
 * ignore the chip enable bit and concentrate only on sending bytes to the NAND
 * Flash.
 */
static void rts_spinand_cmd_ctrl(struct mtd_info *mtd, int data,
				 unsigned int ctrl)
{
	// return;
}

/*
 * Calculate HW ECC
 *
 * function called after a write
 *
 * mtd:        MTD block structure
 * dat:        raw data (unused)
 * ecc_code:   buffer for ECC
 */
static int rts_spinand_calculate(struct mtd_info *mtd,
				 const u_char *dat, unsigned char *ecc_code)
{
	return 0;
}

/*
 * HW ECC Correction
 *
 * function called after a read
 *
 * mtd:        MTD block structure
 * dat:        raw data read from the chip
 * read_ecc:   ECC from the chip (unused)
 * isnull:     unused
 *
 * Detect and correct a 1 bit error for a page
 */
static int rts_spinand_correct(struct mtd_info *mtd, u_char *dat,
			       u_char *read_ecc, u_char *isnull)
{
	return 0;
}

/*
 * Enable HW ECC : unused on most chips
 */
static void rts_spinand_hwctl(struct mtd_info *mtd, int mode)
{
}

/*
 * Allocate DMA buffers
 */
int rts_spinand_alloc_buffers(struct rts_spinand_info *nand_info)
{
	const int size = SPI_NAND_PAGESIZE + SPI_NAND_OOBSIZE;

	nand_info->data_buf_size = size;

	return 0;
}

/* set spi nand otp register by manufactory */
static int spinand_setotp_bymf(struct rts_spinand_info *info)
{
	u8 otp = 0;

	/*for MXIC spi nand flash, quad bit is set in otp register bit0*/
	if (info->spi_dev.flags & OTP_TYPE_1) {
		if ((info->spi_dev.read_type & DATA_QUAD_CH) ||
		    (info->spi_dev.write_type & DATA_QUAD_CH))
			otp = 1;
	}

	/*for winbond spi nand flash, set bit3 to non-continuous mode,
	 *and quad function is opened by instruction
	 */
	if (info->spi_dev.flags & OTP_TYPE_2)
		otp = 8;

	/*when power on , default use software die0,
	 * set otp register of software die1 here
	 */
	if (info->spi_dev.flags & SW_DIE_WITH2) {
		swdie_id = 1;
		spinand_sel_swdie(info, swdie_id);
		spinand_set_otp(info, &otp);
		swdie_id = 0;
		spinand_sel_swdie(info, swdie_id);
	}

	printf("otp:%d\n", otp);
	spinand_set_otp(info, &otp);

	return 0;
}

/*
 * Initializes the SPINFC hardware.
 */
int rts_spinand_init(struct rts_spinand_info *info)
{
	info->base_addr = get_rts_spinfc_base();

	spinand_set_rtsecc_threshold(info, 12);
	spinand_clear_rtsecc_status(info);
	spinand_set_autogetfeat_count(info, 0xffff);
	spinand_disable_interrupt(info);

	return 0;
}

int rts_spinand_setauto(struct nand_chip *nand)
{
	struct rts_spinand_info *info = (struct rts_spinand_info *)nand->priv;

	spinand_setotp_bymf(info);
	spinand_setautoread_para(info);
	spinand_setplane(info);
	return 0;
}

int rts_spinand_ecc_init(struct nand_chip *nand)
{
	struct rts_spinand_info *info = (struct rts_spinand_info *)nand->priv;

	/* disable flash ecc and enable spic ecc */
	spinand_disable_ecc(info);
	spinand_enable_rtsecc_whenread(info);
	spinand_enable_rtsecc_whenwrite(info);
	return 0;
}

static const struct udevice_id rts_spinand_ids[] = {
	{
		.compatible = "realtek,rts3917-quadspi-nfc",
	},
	{ /* sentinel */ }
};

static int rts_spinand_probe(struct udevice *dev)
{
	struct rts_nfc *nfc = dev_get_priv(dev);
#if !CONFIG_IS_ENABLED(OF_CONTROL)
	struct rts_nfc_platdata *plat = dev_get_plat(dev);
#endif
	struct rts_spinand_info *nand_info;
	struct spinand_state *state;
	struct nand_chip *nand;
	struct mtd_info *mtd;
	int err;

	nfc->dev = dev;

#if !CONFIG_IS_ENABLED(OF_CONTROL)
	nfc->regs = plat->regs;
#else
	nfc->regs = (void *)dev_read_addr(dev);
	if ((fdt_addr_t)nfc->regs == FDT_ADDR_T_NONE)
		return -EINVAL;
#endif
	nand_info = malloc(sizeof(struct rts_spinand_info));
	if (!nand_info) {
		printf("MXS NAND: Failed to allocate private data\n");
		return -ENOMEM;
	}
	memset(nand_info, 0, sizeof(struct rts_spinand_info));

	err = rts_spinand_alloc_buffers(nand_info);
	if (err)
		goto err1;

	err = rts_spinand_init(nand_info);
	if (err)
		goto err1;

	state = malloc(sizeof(struct spinand_state));
	nand_info->spinand_state = state;
	state->buf_ptr	= 0;
	state->buf = (uint8_t *)malloc(nand_info->data_buf_size);
	if (!state->buf) {
		printf("RTS NAND: Error allocating  buffers\n");
		return -ENOMEM;
	}
	memset(state->buf, 0, nand_info->data_buf_size);

	nand = &nand_info->chip;

	nand->priv = nand_info;
	nand->options |= NAND_NO_SUBPAGE_WRITE;

	nand->cmd_ctrl		= rts_spinand_cmd_ctrl;
	nand->cmdfunc		= rts_spinand_cmdfunc;

	nand->select_chip	= rts_spinand_select_chip;
	nand->block_bad		= rts_spinand_block_bad;
	nand->scan_bbt		= rts_spinand_scan_bbt;

	nand->read_byte		= rts_spinand_read_byte;

	nand->read_buf		= rts_spinand_read_buf;
	nand->write_buf		= rts_spinand_write_buf;

	nand->ecc.layout	= &spinand_oob_64;
	nand->ecc.mode		= NAND_ECC_HW;
	nand->ecc.steps		= 1;
	nand->ecc.bytes		= 23;
	nand->ecc.size		= 2048;
	nand->ecc.strength	= 12;
	nand->ecc.calculate	= rts_spinand_calculate;
	nand->ecc.hwctl		= rts_spinand_hwctl;
	nand->ecc.correct	= rts_spinand_correct;

	mtd = nand_to_mtd(nand);
	mtd->owner = THIS_MODULE;
	mtd->dev->parent = dev;

	err = nand_scan_ident(mtd, 1, NULL);
	if (err) {
		dev_err(dev, "nand_scan_ident failed: %d\n", err);
		return err;
	}

	err = nand_scan_tail(mtd);
	if (err) {
		dev_err(dev, "nand_scan_tail failed: %d\n", err);
		return err;
	}

	/* close write protect */
	spinand_blocklock_reg(nand_info, 0x0);

	rts_spinand_setauto(nand);
	rts_spinand_ecc_init(nand);

	return nand_register(0, mtd);
err1:
	free(nand_info);
	return err;
}

U_BOOT_DRIVER(rts_spinand) = {
	.name = "rts_spinand",
	.id = UCLASS_MTD,
	.of_match = rts_spinand_ids,
	.probe = rts_spinand_probe,
	.priv_auto = sizeof(struct rts_nfc),
};

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(RTS_SPI_NAND_FLASH)
static const struct rts_nfc_platdata rts_nfc0_platdata = {
	.regs = (void *)RTS_SPINFC_BASEADDR,
};

U_BOOT_DRVINFO(rts_spinand) = {
	.name = "rts_spinand",
	.plat = &rts_nfc0_platdata,
};
#endif
/*!
 * This function is called during the driver binding process.
 *
 * @param   pdev  the device structure used to store device specific
 *                information that is used by the suspend, resume and
 *                remove functions
 *
 * @return  The function always returns 0.
 */
void board_nand_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(rts_spinand),
					  &dev);
	if (ret && ret != -ENODEV)
		printf("Failed to initialize RTS NAND controller. (error %d)\n",
		       ret);
}
