// SPDX-License-Identifier: GPL-2.0
/*
 * Realtek SPI controller driver
 *
 * Copyright 2015  Jethro Hsu (jethro@realtek.com)
 *
 */

#include <common.h>
#include <malloc.h>
#include <spi.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/mtd/spi-nor.h>
#include <configs/bspchip.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include "../mtd/spi/sf_internal.h"
#include "rts_qspi.h"

#define CMD_ADDR_FORMAT(cmd, addr) (((cmd) & 0x000000ff) | \
				((addr & 0x000000ff) << 24) | \
				((addr & 0x0000ff00) << 8) | \
				((addr & 0x00ff0000) >> 8))
#define ADDR_FORMAT(addr) ((addr & 0x000000ff) << 24) | \
				((addr & 0x0000ff00) << 8) | \
				((addr & 0x00ff0000) >> 8) | \
				((addr & 0xff000000) >> 24)

u8 addr_4B_mode;
u8 QPIMode;

/*
 * This function is used to set the ser register.
 */
static u32 spi_flash_setser(struct rts_spi *dev, u32 ser_num)
{
	struct spi_flash_portmap *spi_flash_map;
	struct spi_flash_param *spi_flash_para;

	spi_flash_map = dev->regs;
	spi_flash_para = dev->comp_param;

	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	if (ser_num >= (1 << spi_flash_para->spi_flash_num_slaves)) {
		return -EINVAL;
	} else {
		spi_flash_map->ser = 1 << ser_num;
		return 0;
	}
}

/*
 * This function is used to set the control register.
 */
static void spi_flash_set_tx_mode(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 & TMOD_SEND;
}

/*
 * This function is used to set the control register.
 */
static void spi_flash_set_rx_mode(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | TMOD_RECV;
}

/*
 * This function is used to set the ctrlr1 controller.
 */
static u32 spi_flash_setctrlr1(struct rts_spi *dev, u32 num_frame)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	if (num_frame > MAX_NDF) {
		return -ENODATA;
	} else {
		spi_flash_map->ctrlr1 = num_frame;
		return 0;
	}
}

/*
 * This function is used to set the dr controller.
 */
static u32 spi_flash_setdr(struct rts_spi *dev,
			   enum spi_flash_dr_number dr_num,
			   u32 data,
			   enum spi_flash_byte_num byte_num)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 wr_data;

	spi_flash_map = dev->regs;

	if (byte_num == DATA_BYTE)
		wr_data = data & 0x000000ff;
	else if (byte_num == DATA_HALF)
		wr_data = cpu_to_le16(data);
	else if (byte_num == DATA_WORD)
		wr_data = cpu_to_le32(data);

	if (dr_num > DR31) {
		return -EINVAL;
	} else {
		if (byte_num == DATA_BYTE)
			spi_flash_map->dr[dr_num].byte = wr_data;
		else if (byte_num == DATA_HALF)
			spi_flash_map->dr[dr_num].half = wr_data;
		else if (byte_num == DATA_WORD)
			spi_flash_map->dr[dr_num].word = wr_data;
		else
			return -EINVAL;
	}

	return 0;
}

/*
 * This function is used to set the tx_ndf controller.
 */
static u32 spi_flash_settxndf(struct rts_spi *dev, u32 num)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	spi_flash_map->tx_ndf = num;

	return 0;
}

/*
 * This function is used to set the baud rate register.
 */
static u32 spi_flash_setbaudr(struct rts_spi *dev, u32 baudrate)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	if ((baudrate & 0xffff0000) || (baudrate == 0)) {
		return -EINVAL;
	} else {
		spi_flash_map->baudr = baudrate;
		return 0;
	}
}

/*
 * This function is used to set the fast baud rate register for fast read cmd.
 */
static u32 spi_flash_setfbaudr(struct rts_spi *dev, u32 fbaudrate)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	if ((fbaudrate & 0xffff0000) || (fbaudrate == 0)) {
		return -EINVAL;
	} else {
		spi_flash_map->fbaudr = fbaudrate;
		return 0;
	}
}

/*
 * This function is used to set the user dummy length register.
 */
static u32 spi_flash_set_user_dummy_cycle(struct rts_spi *dev, u32 dum_cycle)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 cycle;

	cycle = 0;
	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	/* if using fast_read baud_rate */
	if ((spi_flash_map->ctrlr0) & CMD_CH_DUAL)
		cycle = spi_flash_map->fbaudr;
	else
		cycle = spi_flash_map->baudr;

	cycle = (cycle * dum_cycle * 2);

	if (cycle > 0x10000)
		return -ECHRNG;

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_DUM_SHIFT,
			cycle,
			SPI_FLASH_USER_LEN_DUM_WIDTH);
	/* set pad delay */
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			DEF_RD_TUNING_DUMMY_CYCLE,
			SPI_FLASH_RD_PHY_DUM_WIDTH);

	return 0;
}

/*
 * This function is used to set the Baudr controller.
 */
static u32 spi_flash_set_auto_dummy_cycle(struct rts_spi *dev, u32 dum_cycle)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 cycle;

	cycle = 0;
	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	/* if using fast_read baud_rate */
	if ((spi_flash_map->ctrlr0) & CMD_CH_DUAL)
		cycle = spi_flash_map->fbaudr;
	else
		cycle = spi_flash_map->baudr;

	cycle = (cycle * dum_cycle * 2);
	if (cycle > 0x10000)
		return -ECHRNG;

	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_AUTO_LEN_DUM_SHIFT,
			cycle,
			SPI_FLASH_AUTO_LEN_DUM_WIDTH);
	/* set pad delay */
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			DEF_RD_TUNING_DUMMY_CYCLE,
			SPI_FLASH_RD_PHY_DUM_WIDTH);

	return 0;
}

/*
 * This function is used to read the dr register.
 */
static u32 spi_flash_getdr(struct rts_spi *dev,
			   enum spi_flash_dr_number dr_num,
			   enum spi_flash_byte_num byte_num)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 rd_data;
	u32 data;

	spi_flash_map = (struct spi_flash_portmap *)dev->regs;

	if (dr_num > DR31) {
		return -ECHRNG;
	} else {
		if (byte_num == DATA_BYTE) {
			data = spi_flash_map->dr[dr_num].byte & 0x000000ff;
		} else if (byte_num == DATA_HALF) {
			rd_data = spi_flash_map->dr[dr_num].half & 0x0000ffff;
			data = le16_to_cpu(rd_data);
		} else if (byte_num == DATA_WORD) {
			rd_data =  spi_flash_map->dr[dr_num].word;
			data = le32_to_cpu(rd_data);
		} else {
			return -EIO;
		}
		return data;
	}
}

/*
 * This function is used to wait the spi_flash is not at busy state.
 */
void spi_flash_wait_busy(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	while (1) {
		if (DW_BIT_GET_UNSHIFTED(spi_flash_map->sr,
					 SPI_FLASH_SR_TXE_SHIFT)) {
			printf("spi_flash: transfer error. base=0x%x\n",
			       (unsigned int)spi_flash_map);
			break;
		} else {
			if (!DW_BIT_GET_UNSHIFTED(spi_flash_map->ssienr,
						  SPI_FLASH_SSIENR)) {
				break;
			}
		}
	}
}

void spi_flash_fifo_busy(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	while (1) {
		if (!DW_BIT_GET_UNSHIFTED(spi_flash_map->sr,
					  SPI_FLASH_SR_BUSY_SHIFT)) {
			debug("spi_flash: tx/rx FIFO empty\n");
			break;
		}
	}
}

/*
 * The function is used to set
 * cmd(1 byte) + address (3 byte) or
 * cmd(1 byte) + address (4 byte) according to
 * enable 4 byte address mode(EN4B)
 */
static void spi_flash_set_cmd_addr(struct rts_spi *dev,
				   u32 wr_addr, u8 wr_cmd)
{
	u32 wr_cmd_addr;

	if (addr_4B_mode == 0) {
		/* set flash cmd and addr */
		wr_cmd_addr = CMD_ADDR_FORMAT(wr_cmd, wr_addr);
		/* Write cmd, addr, data into FIFO */
		spi_flash_setdr(dev, DR0, wr_cmd_addr, DATA_WORD);
	} else {
		wr_addr = ADDR_FORMAT(wr_addr);
		/* Write cmd into FIFO */
		spi_flash_setdr(dev, DR0, wr_cmd, DATA_BYTE);
		/* Write addr into FIFO */
		spi_flash_setdr(dev, DR0, wr_addr, DATA_WORD);
	}
}

void spi_flash_set_usr_addrlen(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT, 0,
			SPI_FLASH_USER_LEN_ADDR_WIDTH);

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_CMD_SHIFT, 1,
			SPI_FLASH_USER_LEN_CMD_WIDTH);
}

void spi_flash_set_usr_addrlen_init(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	if (addr_4B_mode == 0)
		DW_BITS_SET_VAL(spi_flash_map->user_length,
				SPI_FLASH_USER_LEN_ADDR_SHIFT, 3,
				SPI_FLASH_USER_LEN_ADDR_WIDTH);
	else
		DW_BITS_SET_VAL(spi_flash_map->user_length,
				SPI_FLASH_USER_LEN_ADDR_SHIFT, 4,
				SPI_FLASH_USER_LEN_ADDR_WIDTH);

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_CMD_SHIFT, 1,
			SPI_FLASH_USER_LEN_CMD_WIDTH);
}

/*
 * This function is used to set tx command such as WREN, CE command.
 */
static void flash_tx_cmd(struct rts_spi *dev, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;
	/* set ctrlr0: TX mode */
	spi_flash_set_tx_mode(dev);

	if (QPIMode == 1)
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | QPI_CH;
	else
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE;
	/* set flash_cmd: wren to fifo */
	spi_flash_setdr(dev, DR0, cmd, DATA_BYTE);
	spi_flash_settxndf(dev, 0);
	spi_flash_set_usr_addrlen(dev);

	debug("tx cmd, ctrlr0 is %x, usr_len is %x, cmd is %x\n",
	      spi_flash_map->ctrlr0, spi_flash_map->user_length, cmd);

	/* Enable SPI_FLASH */
	spi_flash_map->ssienr = 1;
	/* set QPI flags */
	if (cmd == CMD_ENTER_QPI_I || cmd == CMD_ENTER_QPI_II)
		QPIMode = 1;

	if (cmd == CMD_EXIT_QPI_I || cmd == CMD_EXIT_QPI_II)
		QPIMode = 0;

	spi_flash_wait_busy(dev);
	spi_flash_fifo_busy(dev);
	flash_wait_busy(dev);
	spi_flash_set_usr_addrlen_init(dev);
}

/*
 * This function is used to set tx command such as RDID, RDSR command.
 */
static void flash_rx_cmd(struct rts_spi *dev, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = (struct spi_flash_portmap *)dev->regs;

	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;
	/* set ctrlr0: RX_mode */
	spi_flash_set_rx_mode(dev);

	if (QPIMode == 1)
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | QPI_CH;
	else
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE;
	spi_flash_set_usr_addrlen(dev);
	/* set flash_cmd: write cmd to fifo */
	spi_flash_setdr(dev, DR0, cmd, DATA_BYTE);
	debug("rx cmd, ctrlr0 is %x, usr_len is %x, cmd is %x\n",
	      spi_flash_map->ctrlr0, spi_flash_map->user_length, cmd);
}

/*
 * This function is used to set flash status register.
 */
static void flash_set_status(struct rts_spi *dev, u32 addr, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;

	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif
	/* set ctrlr0: TX mode */
	spi_flash_set_tx_mode(dev);

	if (QPIMode == 1)
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | QPI_CH;
	else
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE;

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT, 0,
			SPI_FLASH_USER_LEN_ADDR_WIDTH);
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_CMD_SHIFT, 1,
			SPI_FLASH_USER_LEN_CMD_WIDTH);

	/* Set flash_cmd: WRSR to FIFO */
	spi_flash_setdr(dev, DR0, cmd, DATA_BYTE);
}

/*
 * This function is used to get flash status for flash_wait_busy.
 */
static u8 flash_get_status(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif
	/* Set Ctrlr1; 1 byte data frames */
	spi_flash_map->ctrlr1 = 1;
	spi_flash_map->tx_ndf = 0;

	/* Set tuning dummy cycles */
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_AUTO_LEN_DUM_SHIFT,
			0,
			SPI_FLASH_AUTO_LEN_DUM_WIDTH);
	/* set pad delay */
#ifdef CONFIG_NOR_DTR_MODE
	DW_BITS_SET_VAL(spi_flash_map->auto_length, SPI_FLASH_RD_PHY_DUM_SHIFT,
			2, SPI_FLASH_RD_PHY_DUM_WIDTH);
#else
	DW_BITS_SET_VAL(spi_flash_map->auto_length, SPI_FLASH_RD_PHY_DUM_SHIFT,
			DEF_RD_TUNING_DUMMY_CYCLE, SPI_FLASH_RD_PHY_DUM_WIDTH);
#endif

	/* Set flash_cmd: RDSR to FIFO */
	flash_rx_cmd(dev, CMD_READ_STATUS_1);
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT, 0,
			SPI_FLASH_USER_LEN_ADDR_WIDTH);

	/* Enable SPI_FLASH */
	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);

	return spi_flash_getdr(dev, DR0, DATA_BYTE);
}

/*
 * This function is used to read status for mtd.
 */
static void flash_read_status(struct rts_spi *dev, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif

	/* Set dummy cycles */
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_AUTO_LEN_DUM_SHIFT,
			0,
			SPI_FLASH_AUTO_LEN_DUM_WIDTH);
	/* set pad delay */
#ifdef CONFIG_NOR_DTR_MODE
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			2,
			SPI_FLASH_RD_PHY_DUM_WIDTH);
#else
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			DEF_RD_TUNING_DUMMY_CYCLE,
			SPI_FLASH_RD_PHY_DUM_WIDTH);
#endif

	/* Set flash_cmd: RDSR to FIFO */
	flash_rx_cmd(dev, cmd);
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT,
			0,
			SPI_FLASH_USER_LEN_ADDR_WIDTH);
}

static void flash_wait_busy(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	u32 time0 = (CONFIG_SYS_HZ * 15) / 1000;
	int timeout_flag = 1;
	u32 baudr_tmp;

	baudr_tmp = spi_flash_map->baudr;

	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

	/* auto check flash status */
	spi_flash_map->ctrlr1 = 0;
#ifdef CONFIG_NOR_DTR_MODE
	spi_flash_map->baudr = AUTO_CHECK_BAUDR;
#endif

	if (QPIMode == 1) {
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 |
		QPI_CH | TIME_OUT_VALUE_QPI;
		spi_flash_map->rd_status = spi_flash_map->rd_status |
		ST_CMD_CH_QUAD;
	} else {
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | TIME_OUT_VALUE;
	}

	// spi_flash_map->imr = spi_flash_map->imr | ACEIM | ACSIM;
	// spi_flash_map->user_length = 0;
	// spi_flash_map->tx_ndf = 0;
	// spi_flash_map->ssienr = 0x3;

	// while (1) {
	//	if (DW_BIT_GET_UNSHIFTED(spi_flash_map->risr,
	//	SPI_FLASH_RISR_ACEIR)) {
	//		DW_BIT_SET(spi_flash_map->icr, 0);
	//		// printf("timeout!\n");
	//		timeout_flag = 1;
	//		break;
	//	} else if (DW_BIT_GET_UNSHIFTED(spi_flash_map->risr,
	//	SPI_FLASH_RISR_ACSIR)) {
	//		DW_BIT_SET(spi_flash_map->icr, 0);
	//		// spi_flash_map->ssienr = 0;
	//		// printf("auto check!\n");
	//		break;
	//	}
	// }

	spi_flash_map->baudr = baudr_tmp;

	/* manual check flash status */
	if (timeout_flag == 1) {
		u32 time_start = get_timer(0);

		while ((get_timer(time_start) < time0) &&
		       (flash_get_status(dev) & 0x1))
			;
	}
}

/*
 * This function is used to chip erase.
 */
static void flash_chip_erase(struct rts_spi *dev, u8 cmd)
{
	flash_tx_cmd(dev, cmd);
}

/*
 * This function is used to sector erase 4K bytes.
 */
static void flash_be_4k_erase(struct rts_spi *dev,
			      u32 addr, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

	/* set ctrlr0: TX_mode */
	DW_BITS_SET_VAL(spi_flash_map->ctrlr0, SPI_FLASH_CTRLR0_TMOD_SHIFT,
			0, SPI_FLASH_CTRLR0_TMOD_WIDTH);

	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);
	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);
	spi_flash_fifo_busy(dev);
	flash_wait_busy(dev);
	spi_flash_set_usr_addrlen_init(dev);
}

/*
 * This function is used to bolck erase 64K bytes.
 */
static void flash_se_erase(struct rts_spi *dev, u32 addr, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif
	/* set ctrlr0: TX_mode */
	DW_BITS_SET_VAL(spi_flash_map->ctrlr0, SPI_FLASH_CTRLR0_TMOD_SHIFT,
			0, SPI_FLASH_CTRLR0_TMOD_WIDTH);

	if (addr_4B_mode == 0)
		DW_BITS_SET_VAL(spi_flash_map->user_length,
				SPI_FLASH_USER_LEN_ADDR_SHIFT,
				3,
				SPI_FLASH_USER_LEN_ADDR_WIDTH);
	else
		DW_BITS_SET_VAL(spi_flash_map->user_length,
				SPI_FLASH_USER_LEN_ADDR_SHIFT,
				4,
				SPI_FLASH_USER_LEN_ADDR_WIDTH);

	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);
	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);
	spi_flash_fifo_busy(dev);
	flash_wait_busy(dev);
	spi_flash_set_usr_addrlen_init(dev);
}

static void flash_write_disable(struct rts_spi *dev)
{
	flash_tx_cmd(dev, CMD_WRITE_DISABLE);
}

static void flash_write_enable(struct rts_spi *dev)
{
	flash_tx_cmd(dev, CMD_WRITE_ENABLE);
}

static void flash_exit_4B_mode(struct rts_spi *dev)
{
	flash_tx_cmd(dev, CMD_EXIT_4B);
	addr_4B_mode = 0;
}

static void flash_enter_4B_mode(struct rts_spi *dev)
{
	flash_tx_cmd(dev, CMD_ENTER_4B);
	addr_4B_mode = 1;
}

static void flash_read_id(struct rts_spi *dev, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif

	/* Set Ctrlr1; 3 byte data frames */
	spi_flash_setctrlr1(dev, 3);

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_DUM_SHIFT,
			0,
			SPI_FLASH_USER_LEN_DUM_WIDTH);
	/* set pad delay */
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			2,
			SPI_FLASH_RD_PHY_DUM_WIDTH);

	/* use slow speed to read id */
	spi_flash_setbaudr(dev, RD_ID_BAUDR);

	flash_rx_cmd(dev, cmd);
}

/*
 * This function is used to send single write command.
 */
static void flash_write_cmd(struct rts_spi *dev, u32 addr,
			    enum spi_flash_byte_num byte_num, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;
	/* set ctrlr0: TX mode, data_ch, addr_ch */
	spi_flash_set_tx_mode(dev);

	if (QPIMode == 1)
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 | QPI_CH;
	else
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE;
	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);
}

/*
 * This function is used to send data to SPIC FIFO.
 */
static void flash_write_data(struct rts_spi *dev, u32 data,
			     enum spi_flash_byte_num byte_num)
{
	if (byte_num == DATA_BYTE)
		spi_flash_setdr(dev, DR0, data, DATA_BYTE);
	else if (byte_num == DATA_HALF)
		spi_flash_setdr(dev, DR0, data, DATA_HALF);
	else if (byte_num == DATA_WORD)
		spi_flash_setdr(dev, DR0, data, DATA_WORD);
}

/*
 * This function is used to send single read command.
 */
static void flash_read(struct rts_spi *dev, u32 addr,
		       enum spi_flash_byte_num byte_num, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

	/* set ctrlr0: RX mode, data_ch, addr_ch */
	spi_flash_set_rx_mode(dev);
	spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
				DATA_CH_SINGLE & ADDR_CH_SINGLE;
	spi_flash_set_usr_addrlen_init(dev);
	/* Set dummy cycles */
	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_AUTO_LEN_DUM_SHIFT,
			0,
			SPI_FLASH_AUTO_LEN_DUM_WIDTH);
	/* set pad delay */
	DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_RD_PHY_DUM_SHIFT,
			DEF_RD_TUNING_DUMMY_CYCLE,
			SPI_FLASH_RD_PHY_DUM_WIDTH);

	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);
}

/*
 * This function is used to send fast read command.
 */
static void flash_fastread(struct rts_spi *dev, u32 addr,
			   enum spi_flash_byte_num byte_num,
			   u32 dummy, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

	spi_flash_set_user_dummy_cycle(dev, dummy);
	/* set ctrlr0: RX mode, data_ch, addr_ch */
	spi_flash_set_rx_mode(dev);
	if (QPIMode == 1)
		spi_flash_map->ctrlr0 = (spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE) | QPI_CH;
	else
		spi_flash_map->ctrlr0 = spi_flash_map->ctrlr0 &
			DATA_CH_SINGLE & ADDR_CH_SINGLE;
	spi_flash_set_usr_addrlen_init(dev);
	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);
}

/*
 * This function is used to send 2-channel read command.
 */
static u32 flash_readx2(struct rts_spi *dev, u32 addr,
			enum spi_flash_byte_num byte_num,
			u32 dummy, u32 type, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 ctrlr0;

	spi_flash_map = dev->regs;
	spi_flash_set_user_dummy_cycle(dev, dummy);
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

	/* set ctrlr0: RX_mode */
	ctrlr0 = spi_flash_map->ctrlr0;

	spi_flash_set_usr_addrlen_init(dev);
	if (type == RD_DUAL_IO)
		spi_flash_map->ctrlr0 = (ctrlr0 & DATA_CH_SINGLE &
			ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_DUAL |
			ADDR_CH_DUAL | TMOD_RECV);
	else if (type == RD_DUAL_O)
		spi_flash_map->ctrlr0 = (ctrlr0 & DATA_CH_SINGLE &
			ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_DUAL |
			TMOD_RECV);
	else {
		printf("Not support readx2 command.\n");
		return  -EPERM;
	}
	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);

	return 0;
}

/*
 * This function is used to send 4-channel write command.
 */
static u32 flash_writex4(struct rts_spi *dev, u32 addr,
			 enum spi_flash_byte_num byte_num,
			 u32 type, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 init_data;

	spi_flash_map = dev->regs;
	/* Not support writex4 */
	if (type == WR_MULTI_NONE) {
		printf("INFO:Not support Writex4 command.\n");
		return  -EPERM;
	}

	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	disable_spi_nor_ddr_mode(dev);
#endif

	/* set ctrlr0: TX mode, data_ch, addr_ch */
	init_data = spi_flash_map->ctrlr0;
	spi_flash_set_usr_addrlen_init(dev);

	if (type == WR_QUAD_II)
		spi_flash_map->ctrlr0 = (init_data & DATA_CH_SINGLE &
			ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_QUAD |
			ADDR_CH_QUAD);
	else if (type == WR_QUAD_I)
		spi_flash_map->ctrlr0 = (init_data & DATA_CH_SINGLE &
			ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_QUAD);
	else {
		printf("INFO:Not support Writex4 command.\n");
		return  -EPERM;
	}
	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);

	return 0;
}

/*
 * This function is used to send 4-channel read command.
 */
static u32 flash_readx4(struct rts_spi *dev, u32 addr,
			enum spi_flash_byte_num byte_num,
			u32 dummy, u32 type, u8 cmd)
{
	struct spi_flash_portmap *spi_flash_map;
	u32 ctrlr0;

	spi_flash_map = dev->regs;
	spi_flash_set_user_dummy_cycle(dev, dummy);
	/* Disable SPI_FLASH */
	spi_flash_wait_busy(dev);
	spi_flash_map->ctrlr0 |= USER_MODE | UAR;

#ifdef CONFIG_NOR_DTR_MODE
	enable_spi_nor_ddr_mode(dev);
#endif

	/* set ctrlr0: RX_mode */
	ctrlr0 = spi_flash_map->ctrlr0;
	spi_flash_set_usr_addrlen_init(dev);

	if (type == RD_QUAD_IO) {
		if (QPIMode == 1)
			spi_flash_map->ctrlr0 = (ctrlr0 & DATA_CH_SINGLE &
				ADDR_CH_SINGLE & TMOD_SEND) | (QPI_CH |
				TMOD_RECV);
		else
			spi_flash_map->ctrlr0 = (ctrlr0 & DATA_CH_SINGLE &
				ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_QUAD |
				ADDR_CH_QUAD | TMOD_RECV);
	} else {
		if (type == RD_QUAD_O)
			spi_flash_map->ctrlr0 = (ctrlr0 & DATA_CH_SINGLE &
				ADDR_CH_SINGLE & TMOD_SEND) | (DATA_CH_QUAD |
				TMOD_RECV);
		else {
			printf("INFO:Not support readx4 command type = 0x%x\n",
					type);
			return  -EPERM;
		}
	}

	/* set flash cmd + addr and write to fifo */
	spi_flash_set_cmd_addr(dev, addr, cmd);

	return 0;
}

/*
 * This function is used to spic and flash initialization.
 */
static void flash_chip_init(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = (struct spi_flash_portmap *)dev->regs;

	/* Set baud_rate */
#if defined CONFIG_NOR_DTR_MODE || defined CONFIG_NOR_QPI_MODE || \
	defined CONFIG_NOR_QUAD_CHANNEL
	spi_flash_setbaudr(dev, QUAD_BAUDR);
#else
	spi_flash_setbaudr(dev, NORMAL_BAUDR);
#endif

	/* Set fast baudr rate */
	spi_flash_setfbaudr(dev, FAST_READ_BAUDR);
	/* Set slave(FLASH) num */
	spi_flash_setser(dev, 0);
	/* init addr length 3 byte */
	spi_flash_map->user_length = 0x31000;
	/* using to init write signal */
	flash_write_disable(dev);
}

/* Enable chip select */
static void enable_cs_write(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* Enable SPI_FLASH */
	debug("en write, ctrlr0 is %x, user_len is %x rx_ndf %d tx_ndf %d\n",
	      spi_flash_map->ctrlr0, spi_flash_map->user_length,
	      spi_flash_map->ctrlr1, spi_flash_map->tx_ndf);
	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);
	spi_flash_fifo_busy(dev);

	flash_wait_busy(dev);
	spi_flash_set_usr_addrlen_init(dev);

#ifdef CONFIG_NOR_DTR_MODE
	enable_spi_nor_ddr_mode(dev);
#endif

}

static void enable_cs_read(struct rts_spi *dev, u32 len)
{
	struct spi_flash_portmap *spi_flash_map;

	spi_flash_map = dev->regs;
	/* set receive data length */
	spi_flash_setctrlr1(dev, len);

	debug("en read, ctrlr0 is %x, user_len is %x RX_NDF %x\n",
		spi_flash_map->ctrlr0,
		spi_flash_map->user_length,
		spi_flash_map->ctrlr1);

	/* Enable SPI_FLASH */
	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);

#ifdef CONFIG_NOR_DTR_MODE
	enable_spi_nor_ddr_mode(dev);
#endif

}

static void select_op(struct rts_spi *dws, u32 addr,
		      u32 dummy, u32 type, u8 cmd)
{
	debug("cmd=0x%x, addr=0x%x, dummy=0x%x, type=0x%x\n",
	      cmd, addr, dummy, type);

	switch (cmd) {
	case CMD_PAGE_PROGRAM:
	case CMD_PAGE_PROGRAM_4B:
		flash_write_cmd(dws, addr, DATA_WORD, cmd);
		break;
	case CMD_WRITE_ENABLE:
		flash_write_enable(dws);
		break;
	case CMD_WRITE_DISABLE:
		flash_write_disable(dws);
		break;
	case CMD_WRITE_STATUS_1:
	case CMD_WRITE_STATUS_2:
	case CMD_WRITE_STATUS_3:
	case CMD_EXTNADDR_WREAR:
	case CMD_WRITE_EX_READ:
	case CMD_WRITE_READ_PARAMETER:
		flash_set_status(dws, addr, cmd);
		break;
	case CMD_READ_STATUS_1:
	case CMD_READ_STATUS_2:
	case CMD_READ_STATUS_4:
	case CMD_READ_STATUS_5:
	case CMD_EXTNADDR_RDEAR:
	case CMD_READ_CONF_STATUS:
	case CMD_READ_EX_READ:
		flash_read_status(dws, cmd);
		break;
	case CMD_READ_ID:
		flash_read_id(dws, cmd);
		break;
	case CMD_READ_ARRAY_SLOW:
	case CMD_READ_4B:
		flash_read(dws, addr, DATA_WORD, cmd);
		break;
	case CMD_READ_ARRAY_FAST:
	case CMD_READ_ARRAY_FAST_4B:
		flash_fastread(dws, addr, DATA_WORD, dummy, cmd);
		break;
	case CMD_ERASE_CHIP:
		flash_chip_erase(dws, cmd);
		break;
	case CMD_ERASE_4K:
	case CMD_ERASE_4K_4B:
		flash_be_4k_erase(dws, addr, cmd);
		break;
	case CMD_ERASE_64K_4B:
	case CMD_ERASE_64K:
		flash_se_erase(dws, addr, cmd);
		break;
	case CMD_ENTER_4B:
		flash_enter_4B_mode(dws);
		break;
	case CMD_EXIT_4B:
		flash_exit_4B_mode(dws);
		break;
	case SPINOR_OP_READ_1_1_2:
	case SPINOR_OP_READ_1_1_2_4B:
		flash_readx2(dws, addr, DATA_WORD, dummy, RD_DUAL_O, cmd);
		break;
	case SPINOR_OP_READ_1_2_2:
	case SPINOR_OP_READ_1_2_2_4B:
	case SPINOR_OP_READ_1_2_2_DTR:
	case SPINOR_OP_READ_1_2_2_DTR_4B:
		flash_readx2(dws, addr, DATA_WORD, dummy, RD_DUAL_IO, cmd);
		break;
	case SPINOR_OP_READ_1_1_4:
	case SPINOR_OP_READ_1_1_4_4B:
		flash_readx4(dws, addr, DATA_WORD, dummy, RD_QUAD_O, cmd);
		break;
	case SPINOR_OP_READ_1_4_4:
	case SPINOR_OP_READ_1_4_4_4B:
	case SPINOR_OP_READ_1_4_4_DTR:
	case SPINOR_OP_READ_1_4_4_DTR_4B:
		flash_readx4(dws, addr, DATA_WORD, dummy, RD_QUAD_IO, cmd);
		break;
	case SPINOR_OP_PP_1_1_4:
	case SPINOR_OP_PP_1_1_4_4B:
		flash_writex4(dws, addr, DATA_WORD, WR_QUAD_I, cmd);
		break;
	case SPINOR_OP_PP_1_4_4:
	case SPINOR_OP_PP_1_4_4_4B:
		flash_writex4(dws, addr, DATA_WORD, WR_QUAD_II, cmd);
		break;
	default:
		printf("INFO:No support flash cmd:0x%x\n", cmd);
		break;
	}
}

static int dw_spi_setup(struct rts_spi *dev, unsigned int cs)
{
	struct spi_flash_param ps_para = { ps_CC_SPI_FLASH_NUM_SLAVES,
					   ps_CC_SPI_FLASH_TX_FIFO_DEPTH,
					   ps_CC_SPI_FLASH_RX_FIFO_DEPTH,
					   ps_CC_SPI_FLASH_ID,
					   ps_CC_SPI_FLASH_DFLT_SCPOL,
					   ps_CC_SPI_FLASH_DFLT_SCPH,
					   ps_CC_SPI_FLASH_CLK_PERIOD,
					   ps_CC_SPI_FLASH_VERSION_ID
					 };

	/* iniitialize Flash_Device_information */
	dev->comp_param = &ps_para;
	/* user mode init setting */
	flash_chip_init(dev);
	return 0;
}

u8 cmd_s;
u32 addr_s;
u8 dummy_s;
u8 type_s;

static int do_spi_send(struct rts_spi *dev,
		       const void *tx_data,
		       unsigned int len,
		       unsigned long flags)
{
	int cnt = 0, rem = 0, loop = 0;
	int i, j;
	u8 cmd, dummy, type;
	u32 addr;
	u8 *data_addr = (u8 *)tx_data;
	u32 data;

	debug("%s: flags is %lx, len is %d\n", __func__, flags, len);

	if (flags & SPI_XFER_BEGIN) {
		/* extract cmd and addr */
		cmd = data_addr[0];
		if (addr_4B_mode == 1 && len >= 5) {
			addr = data_addr[1] << 24 | data_addr[2] << 16 |
					data_addr[3] << 8 | data_addr[4];
			dummy = (len - 5) * 8;
		} else if (len >= 4) {
			addr = data_addr[1] << 16 | data_addr[2] << 8 |
					data_addr[3];
			dummy = (len - 4) * 8;
		}

		if (QPIMode == 1 || (CONFIG_IS_ENABLED(NOR_DTR_MODE)))
			dummy = dummy / 4;

		if (flags & SPI_XFER_END) {
			/* Only command to write ex: WREN, BE_4K, SE, and CE  */
			select_op(dev, addr, 0, 0, cmd);
			return 0;
		}

		cmd_s = cmd;
		addr_s = addr;
		dummy_s = dummy;
		type_s = type;
		select_op(dev, addr_s, dummy_s, type_s, cmd_s);
		return 0;
	}

	if (flags & SPI_XFER_END)
		spi_flash_settxndf(dev, len);
	else
		spi_flash_settxndf(dev, 0);

	if (flags & SPI_XFER_END) {
		/* write data to FIFO for the command */
		if (!cmd_s) {
			printf("spi command error!\n");
			return -1;
		}

		if (len <= FIFO_SIZE) {
			cnt = len / 4;
			rem = len % 4;

			for (i = 0; i < cnt; i++) {
				memcpy(&data, (void *)(data_addr + (i * 4)), 4);
				flash_write_data(dev, data, DATA_WORD);
			}

			if (rem) {
				memcpy(&data,
				       (void *)(data_addr + (cnt * 4)), rem);

				switch (rem) {
				case 3:
					data = cpu_to_le32(data);
					flash_write_data(dev, data, DATA_BYTE);
					flash_write_data(dev,
							 data >> 8, DATA_HALF);
					break;
				case 2:
					flash_write_data(dev, data, DATA_HALF);
					break;
				case 1:
					spi_flash_setdr(dev,
							DR0, data, DATA_BYTE);
					break;
				}
			}

			enable_cs_write(dev);
		} else {
			loop  = len / FIFO_SIZE;
			cnt = FIFO_SIZE / 4;
			rem = len % FIFO_SIZE;

			for (i = 1; i <= loop; i++) {
				for (j = 0; j < cnt; j++) {
					memcpy(&data,
					       (void *)(data_addr + (j * 4)),
					       4);
					flash_write_data(dev, data, DATA_WORD);
				}
				spi_flash_settxndf(dev, FIFO_SIZE);
				enable_cs_write(dev);
				data_addr += FIFO_SIZE;

				/* Don't do command in final loop */
				if (i == loop)
					break;
				/* Set write enable before write operation */
				flash_write_enable(dev);
				/* re-send command for rest data */
				select_op(dev, addr_s + (i * FIFO_SIZE),
					  dummy_s, type_s, cmd_s);
			}

			if (rem) {
				/* Set write enable before write operation */
				flash_write_enable(dev);
				/* Re-send command for rest data */
				select_op(dev, addr_s + (loop * FIFO_SIZE),
					  dummy_s, type_s, cmd_s);
				cnt  = rem / 4;
				for (i = 0; i < cnt; i++) {
					memcpy(&data,
					       (void *)(data_addr + (i * 4)),
					       4);
					flash_write_data(dev, data, DATA_WORD);
				}
				spi_flash_settxndf(dev, rem);
				rem = rem % 4;
				if (rem) {
					memcpy(&data,
					       (void *)(data_addr + (cnt * 4)),
					       rem);
					switch (rem) {
					case 3:
						data = cpu_to_le32(data);
						flash_write_data(dev, data,
								 DATA_BYTE);
						flash_write_data(dev, data >> 8,
								 DATA_HALF);
						break;
					case 2:
						flash_write_data(dev, data,
								 DATA_HALF);
						break;
					case 1:
						spi_flash_setdr(dev, DR0, data,
								DATA_BYTE);
						break;
					}
				}
				enable_cs_write(dev);
			}
		}
		/* clear command */
		cmd_s = addr_s = dummy_s = type_s = 0;
	}

	return 0;

}

static int do_spi_recv(struct rts_spi *dev,
		       const unsigned char *rx_data,
		       unsigned int len,
		       unsigned long flags)
{
	int cnt = 0, rem = 0, loop = 0;
	int i, j;
	u8 *data_addr = (u8 *)rx_data;
	u32 data;

	debug("%s: flag is %lx, len is %d\n", __func__, flags, len);

	/* read data for last command */
	if (!cmd_s) {
		printf("spi command error!\n");
		return -1;
	}

	if (len <= FIFO_SIZE) {
		enable_cs_read(dev, len);
		cnt = len / 4;
		rem = len % 4;

		for (i = 0; i < cnt; i++) {
			data = spi_flash_getdr(dev, DR0, DATA_WORD);
			memcpy((void *)(data_addr + (i * 4)), &data, 4);
		}

		if (rem) {
			data = spi_flash_getdr(dev, DR0, DATA_WORD);
			memcpy((void *)(data_addr + cnt * 4), &data, rem);
		}
		spi_flash_fifo_busy(dev);
	} else {
		loop = len / FIFO_SIZE;
		cnt = FIFO_SIZE / 4;
		rem = len % FIFO_SIZE;

		for (i = 1; i <= loop; i++) {
			enable_cs_read(dev, FIFO_SIZE);
			for (j = 0; j < cnt; j++) {
				data = spi_flash_getdr(dev, DR0, DATA_WORD);
				memcpy((void *)(data_addr + (j * 4)), &data, 4);
			}

			data_addr += FIFO_SIZE;

			/* Don't send command in final loop */
			if (i == loop)
				break;

			/* Re-send command for rest data */
			select_op(dev, addr_s + (i * cnt * 4),
				  dummy_s, type_s, cmd_s);
		}

		if (rem) {
			/* Re-send command for rest data */
			select_op(dev, addr_s + (loop * FIFO_SIZE),
				  dummy_s, type_s, cmd_s);
			enable_cs_read(dev, rem);

			cnt  = rem / 4;
			for (i = 0; i < cnt; i++) {
				data = spi_flash_getdr(dev, DR0, DATA_WORD);
				memcpy((void *)(data_addr + (i * 4)), &data, 4);
			}

			rem = rem % 4;
			if (rem) {
				data = spi_flash_getdr(dev, DR0, DATA_WORD);
				memcpy((void *)(data_addr + (cnt * 4)),
				       &data, rem);
			}
		}
	}

	if (cmd_s == CMD_READ_ID)
#if defined CONFIG_NOR_DTR_MODE || defined CONFIG_NOR_QPI_MODE || \
	defined CONFIG_NOR_QUAD_CHANNEL
		spi_flash_setbaudr(dev, QUAD_BAUDR);
#else
		spi_flash_setbaudr(dev, NORMAL_BAUDR);
#endif
	cmd_s = addr_s = dummy_s = type_s = 0;
	return 0;
}

static inline struct rts_spi *to_rts_spi(struct spi_slave *slave)
{
	return (struct rts_spi *)(slave->dev->parent->priv_);
}

/*only in QPI mode*/
int flash_set_read_para(struct spi_slave *slave, u16 data, u8 cmd, u8 data_len)
{
	struct rts_spi *dev = to_rts_spi(slave);

	struct spi_flash_portmap *spi_flash_map = dev->regs;
	uint8_t cmd_wrsr, rd_data;
	uint32_t info_tmp;

	info_tmp = spi_flash_map->user_length;

	/* Set flash_cmd: WREN to FIFO*/
	flash_tx_cmd(dev, CMD_WRITE_ENABLE);

	/* Disable SPI_FLASH*/
	spi_flash_map->ssienr = 0;

	rd_data = spi_flash_map->ctrlr0;

	/* set ctrlr0: TX mode */
	spi_flash_set_tx_mode(dev);

	spi_flash_map->ctrlr0 = (rd_data & CLC_CH) | QPI_CH;

	DW_BITS_SET_VAL(spi_flash_map->user_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT,
			data_len,
			SPI_FLASH_USER_LEN_ADDR_WIDTH);

	/* Set flash_cmd: WRSR to FIFO*/
	cmd_wrsr = cmd;

	spi_flash_map->dr[DR0].byte = cmd_wrsr;

	spi_flash_map->dr[DR0].byte = data & 0xff;

	spi_flash_map->ssienr = 1;
	spi_flash_wait_busy(dev);

	spi_flash_map->ssienr = 0;
	spi_flash_map->user_length = info_tmp;
	spi_flash_map->ctrlr0 = rd_data;

	flash_wait_busy(dev);
	return 0;
}

int flash_enable_qpi(struct spi_nor *nor)
{
	struct rts_spi *dev = dev_get_priv(nor->dev->parent);
	struct spi_slave *slave = nor->spi;
	int res = 0;
	u8 cmd = 0;

	if (nor->info->flags & QPI_I)
		cmd = CMD_ENTER_QPI_I;
	else if (nor->info->flags & QPI_II)
		cmd = CMD_ENTER_QPI_II;
	else
		return res;

	/* Set flash_cmd: WREN to FIFO*/
	flash_tx_cmd(dev, CMD_WRITE_ENABLE);
	flash_tx_cmd(dev, cmd);

	if (nor->info->flags & QPI_II) {
		if (nor->info->flags & SPI_NOR_4IO_READ)
			res = flash_set_read_para(slave, 0x20, 0xc0, 1);
		else
			res = flash_set_read_para(slave, 0x30, 0xc0, 1);
	}

	flash_tx_cmd(dev, CMD_WRITE_DISABLE);

	return res;
}

int flash_exit_qpi(struct spi_nor *nor)
{
	struct rts_spi *dev = dev_get_priv(nor->dev->parent);
	int res = 0;
	u8 cmd = 0;

	if (nor->info->flags & QPI_I)
		cmd = CMD_EXIT_QPI_I;
	else if (nor->info->flags & QPI_II)
		cmd = CMD_EXIT_QPI_II;
	else
		return res;

	flash_tx_cmd(dev, cmd);

	return res;
}

int flash_flush_rst_fifo(struct spi_slave *slave)
{
	struct spi_flash_portmap *spi_flash_map;
	struct rts_spi *dev = to_rts_spi(slave);

	spi_flash_map = dev->regs;

	/* flush fifo */
	spi_flash_map->ssienr = 0;
	spi_flash_map->flush_fifo = 2;

	return 0;
}

int flash_set_rst_fifo(struct spi_slave *slave, const u16 reset_flow[])
{
	struct rts_spi *dev = to_rts_spi(slave);
	u32 addr = (u32)dev->rst_regs;
	int reset_flow_cmd_num = 3;
	int res = 0;
	u32 temp;

	flash_flush_rst_fifo(slave);

	if ((reset_flow[2] & 0xc00) != 0)
		reset_flow_cmd_num = 4;

	REG32(addr + SPIC_PGM_FIFO_WPTR) = reset_flow_cmd_num;

	temp = reset_flow[0] | (reset_flow[1] << 16);
	REG32(addr + SPIC_PGM_FIFO_INIT0) = temp;
	temp = reset_flow[2] | (reset_flow[3] << 16);
	REG32(addr + SPIC_PGM_FIFO_INIT1) = temp;

	return res;
}

int enable_spi_nor_ddr_mode(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map = dev->regs;
	u32 ddr_cfg = (u32)dev->rst_regs + SPIC_NOR_DDR_CFG;

	REG32(ddr_cfg) |= DDR_MODE_EN;
	spi_flash_map->ctrlr0 |= DDR_EN;

	return 0;
}

int disable_spi_nor_ddr_mode(struct rts_spi *dev)
{
	struct spi_flash_portmap *spi_flash_map = dev->regs;
	u32 ddr_cfg = (u32)dev->rst_regs + SPIC_NOR_DDR_CFG;

	REG32(ddr_cfg) &= (~DDR_MODE_EN);
	spi_flash_map->ctrlr0 &= (~DDR_EN);

	return 0;
}

int spi_flash_set_auto_mode(struct spi_nor *nor)
{
	struct rts_spi *dev = dev_get_priv(nor->dev->parent);
	struct spi_flash_portmap *spi_flash_map;
	u8 autoread_type = FRD_SINGLE_T;

	spi_flash_map = dev->regs;

	/* Disable SPI_FLASH*/
	spi_flash_wait_busy(dev);
	spi_flash_fifo_busy(dev);

	if (nor->read_proto == SNOR_PROTO_1_1_1)
		autoread_type = FRD_SINGLE_T;
	else if (nor->read_proto == SNOR_PROTO_1_1_2)
		autoread_type = RD_DUAL_O_T;
	else if (nor->read_proto == SNOR_PROTO_1_2_2)
		autoread_type = RD_DUAL_IO_T;
	else if (nor->read_proto == SNOR_PROTO_1_1_4)
		autoread_type = RD_QUAD_O_T;
	else if (nor->read_proto == SNOR_PROTO_1_4_4)
		autoread_type = RD_QUAD_IO_T;
	else if (nor->read_proto == SNOR_PROTO_4_4_4)
		autoread_type = RD_QUAD_IO_T;

	/*Set valid_cmd_reg: auto_cmd*/
	if (autoread_type & RD_QUAD_IO_T)
		spi_flash_map->rd_quad_io = nor->read_opcode;
	else if (autoread_type & RD_QUAD_O_T)
		spi_flash_map->rd_quad_o = nor->read_opcode;
	else if (autoread_type & RD_DUAL_IO_T)
		spi_flash_map->rd_dual_io = nor->read_opcode;
	else if (autoread_type & RD_DUAL_O_T)
		spi_flash_map->rd_dual_o = nor->read_opcode;
	else if (autoread_type & FRD_SINGLE_T)
		spi_flash_map->rd_fast_single = nor->read_opcode;

	if (addr_4B_mode == 0)
		DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT,
			3, SPI_FLASH_USER_LEN_ADDR_WIDTH);
	else
		DW_BITS_SET_VAL(spi_flash_map->auto_length,
			SPI_FLASH_USER_LEN_ADDR_SHIFT,
			4, SPI_FLASH_USER_LEN_ADDR_WIDTH);

	spi_flash_set_auto_dummy_cycle(dev, nor->read_dummy);

#ifdef CONFIG_NOR_DTR_MODE
	enable_spi_nor_ddr_mode(dev);
#endif
	spi_flash_map->valid_cmd = autoread_type | 0x4000;

	/* disable auto write single, send error cmd: 0xeb. */
	spi_flash_map->wr_single = ERROR_SINGLE_WRITE_CMD;

	/*When QPI mode, auto read channel need to using ctrlr0 config.
	 *Whenever auto read command channel always using ctrlr0 config.
	 *Auto read address/data channel using ctrlr0 config when
	 *enable ctrlr0 bit in valid_cmd or using valid_cmd config.
	 */
	if (QPIMode == 1)
		spi_flash_map->valid_cmd |= CTRLR0_CH_VALID_CMD_EN;
	return 0;
}

struct spi_slave *spi_setup_slave_fdt(const void *blob, int slave_node,
					int spi_node)
{
	return NULL;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int rts_dm_spi_of_to_plat(struct udevice *udev)
{
	struct rts_spi_platdata *plat = udev->plat_;
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(udev);

	/* Use 500KHz as a suitable default */
	plat->frequency = fdtdec_get_int(blob, node,
					 "spi-max-frequency", 500000);
	plat->deactivate_delay_us = fdtdec_get_int(blob, node,
						   "spi-deactivate-delay", 0);
	return 0;
}
#endif

static int rts_dm_spi_probe(struct udevice *udev)
{
	fdt_addr_t addr, rst_addr;
	struct rts_spi_platdata *plat = dev_get_plat(udev);
	struct rts_spi *priv = dev_get_priv(udev);

	if (!plat || !priv)
		return -ENODEV;

#if !CONFIG_IS_ENABLED(OF_CONTROL)
	addr = (fdt_addr_t)plat->regs;
	rst_addr = (fdt_addr_t)plat->rst_regs;
#else
	addr = dev_read_addr_index(udev, 0);
	rst_addr = dev_read_addr_index(udev, 1);
	if ((addr == FDT_ADDR_T_NONE) || (rst_addr == FDT_ADDR_T_NONE))
		return -EINVAL;
#endif
	priv->regs = (void *)addr;
	priv->rst_regs = (void *)rst_addr;

	priv->last_transaction_us = timer_get_us();
	priv->freq = plat->frequency;

#ifdef CONFIG_NOR_SINGLE_CHANNEL
	priv->mode = SPI_TX_BYTE | SPI_RX_SLOW;
#endif
#ifdef CONFIG_NOR_DUAL_CHANNEL
	priv->mode = SPI_TX_DUAL | SPI_RX_DUAL;
#endif
#if defined(CONFIG_NOR_QUAD_CHANNEL) ||	defined(CONFIG_NOR_QPI_MODE) \
	|| defined(CONFIG_NOR_DTR_MODE)
	priv->mode = SPI_TX_QUAD | SPI_RX_QUAD;
	REG32(XB2_GPIO_REG(0x26c)) = 0x2;
#endif

	/* Init flash */
	dw_spi_setup(priv, 0);

	return 0;
}

static int rts_flush_fifo(struct udevice *uflash)
{
	if (!uflash)
		return -ENODEV;
	struct rts_spi *priv = dev_get_priv(uflash->parent);
	struct spi_flash_portmap *spi_flash_map = priv->regs;

	spi_flash_map->ssienr = 0;
	spi_flash_map->flush_fifo = 0;
	return 0;
}

static int rts_dm_spi_claim_bus(struct udevice *uflash)
{
	rts_flush_fifo(uflash);
	return 0;
}

static int rts_dm_spi_release_bus(struct udevice *uflash)
{
	return 0;
}

static void spi_cs_activate(struct udevice *uflash)
{
	struct udevice *udev = uflash->parent;
	struct rts_spi_platdata *plat = dev_get_plat(udev);
	struct rts_spi *priv = dev_get_priv(udev);

	debug("activate cs\n");

	/* If it's too soon to do another transaction, wait */
	if (plat->deactivate_delay_us && priv->last_transaction_us) {
		ulong delay_us;         /* The delay completed so far */

		delay_us = timer_get_us() - priv->last_transaction_us;
		if (delay_us < plat->deactivate_delay_us)
			udelay(plat->deactivate_delay_us - delay_us);
	}
}

static void spi_cs_deactivate(struct udevice *uflash)
{
	struct udevice *udev = uflash->parent;
	struct rts_spi_platdata *plat = dev_get_plat(udev);
	struct rts_spi *priv = dev_get_priv(udev);
	struct spi_flash_portmap *spi_flash_map = priv->regs;

	debug("deactivate cs\n");

	/* cs deactivate */
	spi_flash_map->ssienr = 0;

	/* Remember time of this transaction so we can honour the bus delay */
	if (plat->deactivate_delay_us)
		priv->last_transaction_us = timer_get_us();
}

static int rts_dm_spi_xfer(struct udevice *uflash,
			      unsigned int bitlen,
			      const void *dout, void *din, unsigned long flags)
{
	struct udevice *udev = dev_get_parent(uflash);
	struct rts_spi *priv = dev_get_priv(udev);
	const unsigned char *tx_data = dout;
	unsigned char *rx_data = din;
	unsigned int len = bitlen / 8;
	int ret = 0;

	if (flags & SPI_XFER_BEGIN)
		spi_cs_activate(uflash);

	if (tx_data)
		ret = do_spi_send(priv, tx_data, len, flags);

	if (rx_data)
		ret = do_spi_recv(priv, rx_data, len, flags);

	if (flags & SPI_XFER_END)
		spi_cs_deactivate(uflash);

	return ret;
}

static int rts_dm_spi_set_speed(struct udevice *udev, uint speed)
{
	struct rts_spi_platdata *plat = udev->plat_;
	struct rts_spi *priv = dev_get_priv(udev);

	if (speed > plat->frequency)
		speed = plat->frequency;

	priv->freq = speed;
	debug("%s: regs=%p, speed=%d\n", __func__, priv->regs, priv->freq);

	return 0;
}

static int rts_dm_spi_set_mode(struct udevice *udev, uint mode)
{
	struct rts_spi *priv = dev_get_priv(udev);

	if (!mode)
		mode = SPI_TX_QUAD | SPI_RX_QUAD;
	priv->mode = mode;
	debug("%s: regs=%p, mode=%d\n", __func__, priv->regs, priv->mode);

	return 0;
}

static int rts_dm_spi_cs_info(struct udevice *udev,
			      uint cs, struct spi_cs_info *info)
{
	int ret;
	struct udevice *flash;
	struct dm_spi_slave_plat *plat = NULL;
	struct rts_spi *priv = dev_get_priv(udev);

	/* Only allow device activity on CS 0 */
	if (cs > 0)
		return -ENODEV;

	ret = uclass_find_device_by_seq(UCLASS_SPI_FLASH, cs, &flash);
	if (ret) {
		log_debug("%s: No flash %d\n", __func__, cs);
		return ret;
	}

	if (flash->parent != udev) {
		device_reparent(flash, udev);

		plat = calloc(1, sizeof(struct dm_spi_slave_plat));
		if (!plat)
			return -ENOMEM;

		plat->cs = 0;
		plat->max_hz = 50000000;
		plat->mode = priv->mode;
		dev_set_parent_plat(flash, plat);
	}

	return 0;
}

static int rts_dm_spi_child_pre_probe(struct udevice *uflash)
{
	struct dm_spi_slave_plat *plat = dev_get_parent_plat(uflash);
	struct spi_slave *slave = dev_get_parent_priv(uflash);

	/*
	 * This is needed because we pass struct spi_slave around the place
	 * instead slave->dev (a struct udevice). So we have to have some
	 * way to access the slave udevice given struct spi_slave. Once we
	 * change the SPI API to use udevice instead of spi_slave, we can
	 * drop this.
	 */
	slave->dev = uflash;

	slave->max_hz = plat->max_hz;
	slave->mode = plat->mode;
	slave->wordlen = SPI_DEFAULT_WORDLEN;

	return 0;
}

static int rts_dm_spi_child_post_bind(struct udevice *uflash)
{
	struct dm_spi_slave_plat *plat = dev_get_parent_plat(uflash);

	if (dev_has_ofnode(uflash) == false)
		return 0;

	return spi_slave_of_to_plat(uflash, plat);
}

static const struct dm_spi_ops rts_spi_ops = {
	.claim_bus      = rts_dm_spi_claim_bus,
	.release_bus    = rts_dm_spi_release_bus,
	.set_speed      = rts_dm_spi_set_speed,
	.set_mode       = rts_dm_spi_set_mode,
	.xfer           = rts_dm_spi_xfer,
	.cs_info        = rts_dm_spi_cs_info,
};

static const struct udevice_id rts_spi_ids[] = {
	{ .compatible = "realtek,rts3917-quadspi" },
	{ }
};

U_BOOT_DRIVER(rts_spi) = {
	.name   = "rts_spi",
	.id     = UCLASS_SPI,
	.of_match = rts_spi_ids,
	.ops    = &rts_spi_ops,
	.child_pre_probe = rts_dm_spi_child_pre_probe,
	.child_post_bind = rts_dm_spi_child_post_bind,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_to_plat = rts_dm_spi_of_to_plat,
#endif
	.per_child_auto = sizeof(struct spi_slave),
	.per_child_plat_auto = sizeof(struct dm_spi_slave_plat),
	.plat_auto = sizeof(struct rts_spi_platdata),
	.priv_auto = sizeof(struct rts_spi),
	.probe  = rts_dm_spi_probe,
};

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(RTS_QSPI)
static const struct rts_spi_platdata rts_spi0_platdata = {
	.regs = (void *)CONFIG_BSP_SPIC_PADDR,
	.rst_regs = (void *)SPIC_PGM_FIFO_INIT_ADDR,
	.frequency = -1,
	.deactivate_delay_us = 0,
};

U_BOOT_DRVINFO(rts_spi) = {
	.name = "rts_spi",
	.plat = &rts_spi0_platdata,
};

#endif

#if CONFIG_IS_ENABLED(RTS_QSPI)
U_BOOT_DRVINFO(spi_nor) = {
	.name = "jedec_spi_nor",
};
#endif
