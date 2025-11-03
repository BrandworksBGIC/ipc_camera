/* SPDX-License-Identifier: GPL-2.0 */
/*-
 * Copyright 2013 Broadcom Corporation
 *
 * Copyright (c) 2009-2010 Micron Technology, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Henry Pan <hspan@micron.com>
 *
 * based on nand.h
 */
#ifndef __RTS_SPINAND_H__
#define __RTS_SPINAND_H__

#include <linux/mtd/mtd.h>

#define DW_BIT_MASK(__bfws) ((uint32_t)((__bfws) == 32) ? \
		0x0 : (0x1 << (__bfws)))

#define DW_BIT_GET_UNSHIFTED(__datum, __bfws) \
		((uint32_t)((__datum) & DW_BIT_MASK(__bfws)))

#define RTS_SPINFC_BASEADDR 0x18038000

/* cmd */
#define CMD_READ			0x13
#define CMD_READ_RDM			0x03
#define CMD_PROG_PAGE_CLRCACHE		0x02
#define CMD_PROG_PAGE			0x84
#define CMD_PROG_PAGE_EXC		0x10
#define CMD_ERASE_BLK			0xd8
#define CMD_WR_ENABLE			0x06
#define CMD_WR_DISABLE			0x04
#define CMD_READ_ID			0x9f
#define CMD_RESET			0xff
#define CMD_READ_REG			0x0f
#define CMD_WRITE_REG			0x1f
#define CMD_SEL_SOFTWAREDIE		0xc2

/* feature/ status reg */
#define REG_BLOCK_LOCK			0xa0
#define REG_OTP				0xb0
#define REG_STATUS			0xc0
#define REG_ECC				0x90 /* only for xt26g02b*/

/* status */
#define STATUS_OIP_MASK			0x01
#define STATUS_READY			(0 << 0)
#define STATUS_BUSY			(1 << 0)

#define STATUS_E_FAIL_MASK		0x04
#define STATUS_E_FAIL			(1 << 2)

#define STATUS_P_FAIL_MASK		0x08
#define STATUS_P_FAIL			(1 << 3)

#define STATUS_ECC_MASK			0x30
#define STATUS_ECC_1BIT_CORRECTED	(1 << 4)
#define STATUS_ECC_ERROR		(2 << 4)
#define STATUS_ECC_RESERVED		(3 << 4)

/*ECC enable defines*/
#define OTP_ECC_MASK			0x10
#define OTP_ECC_OFF			0
#define OTP_ECC_ON			1

#define ECC_DISABLED
#define ECC_IN_NAND
#define ECC_SOFT

/* block lock */
#define BL_ALL_LOCKED      0x38
#define BL_1_2_LOCKED      0x30
#define BL_1_4_LOCKED      0x28
#define BL_1_8_LOCKED      0x20
#define BL_1_16_LOCKED     0x18
#define BL_1_32_LOCKED     0x10
#define BL_1_64_LOCKED     0x08
#define BL_ALL_UNLOCKED    0

#define SPI_NAND_PAGESIZE		0x800
#define SPI_NAND_OOBSIZE		0x40
#define SPI_NAND_COMMAND_BUFFER_SIZE	0x20

#define CMD_TYPE_1CH_RX		0x01
#define CMD_TYPE_1CH_TX		0x02
#define CMD_TYPE_READ		0x04
#define CMD_TYPE_WRITE		0x08

#define DATA_SINGLE_CH		0x00
#define DATA_DUAL_CH		0x40
#define DATA_QUAD_CH		0x80
#define ADDR_SINGLE_CH		0x00
#define ADDR_DUAL_CH		0x10
#define ADDR_QUAD_CH		0x20
#define TRANSMIT_MODE		0x00
#define RECEIVE_MODE		0x0c

#define WR_QUAD_II_T		0x04
#define WR_QUAD_I_T		0x03
#define WR_DUAL_II_T		0x02
#define WR_DUAL_I_T		0x01
#define WR_MULTI_NONE_T		0x00

#define RD_QUAD_IO_T		0x10
#define RD_QUAD_O_T		0x08
#define RD_DUAL_IO_T		0x04
#define RD_DUAL_O_T		0x02
#define FRD_SINGLE_T		0x01

#define ERROR_THRESHOLD		0x06
#define WR_ECC_EN		0x01
#define WR_ECC_DIS		0x0
#define ECC_STATUS_CLEAR	0x02
#define ECC_THRESHOLD_MASK	0x3c
#define RD_ECC_EN		0x01
#define RD_ECC_DIS		0x0
#define RD_ECC_SUCCESS		0x0
#define RD_ECC_FAIL		0x1

#define PLANE_WITH_2		1
#define PLANE_WITH_1		0
#define OTP_TYPE_1		0x2
#define OTP_TYPE_2		0x4
#define SW_DIE_WITH2		0x8

#define AUTO_GETFEAT_TIMEOUT	0x10
#define AUTO_GETFEAT_IDLE	0x08
#define ECC_ERROR		0x04
#define SPIC_BUSY		0x01
#define SPIC_READY		0x00

#define DISABLE_INTERRUPT	0x1f

#define AUTO_READ_ADDR1_DATA1	0x00
#define AUTO_READ_ADDR1_DATA2	0x02
#define AUTO_READ_ADDR1_DATA4	0x04

struct spinand_info {
	struct nand_ecclayout *ecclayout;
	struct spi_device *spi;
	void *priv;
};

struct spinand_state {
	u32	col;
	u32	row;
	int	buf_ptr;
	u8	*buf;
};

struct spinand_cmd {
	u8		cmd;
	u32		n_addr;		/* Number of address */
	u8		addr[3];	/* Reg Offset */
	u32		n_dummy;	/* Dummy use */
	u32		n_tx;		/* Number of tx bytes */
	u8		*tx_buf;	/* Tx buf */
	u32		n_rx;		/* Number of rx bytes */
	u8		*rx_buf;	/* Rx buf */
	u8		cmd_type;
};

/***** spi_flash.api/portmap
 * DESCRIPTION
 *  This is the structure used for accessing the spi_flash register
 *  portmap.
 * EXAMPLE
 *  struct spi_flash_portmap *portmap;
 *  portmap = (struct spi_nandflash_portmap *) spi_flash_BASE;
 *  foo = INP(portmap->ctrlr0 );
 * SOURCE
 */
struct spi_nandflash_portmap {
		/* Control Reg 0	(0x000) */
		volatile u32 ctrlr0;
		/* SPIC enable Reg1 */
		volatile u32 ssienr;
		/* baud rate	(0x008) */
		volatile u32 baudr;
		/* reset controller */
		volatile u32 ureset;
		/* spi nand flash command	(0x010) */
		volatile u32 ucmd;
		/* user read/write address */
		volatile u32 uaddr;
		/* user read/write address length	(0x018) */
		volatile u32 addr_len;
		/* user read/write data length */
		volatile u32 data_len;
		/* auto read page read address length	(0x020)*/
		volatile u32 page_read_addr_len;
		/* read dummy length */
		volatile u32 read_dummy_len;
		/* program dummy length */
		volatile u32 program_dummy_len;
		/* get features dummy length */
		volatile u32 getfeature_dummy_len;
		/* dummy0[4]	0x030, 0x034, 0x038, 0x03c */
		volatile u32 dummy0[4];
		/* read page command in auto mode	(0x040) */
		volatile u32 read_page_cmd;
		/* random read command in auto mode */
		volatile u32 random_read_cmd;
		/* get features command in auto mode (0x048) */
		volatile u32 get_features_cmd;
		/* get features adderss in auto mode */
		volatile u32 get_features_addr;
		/* get features stop value     (0x050) */
		volatile u32 get_features_stop_value;
		/* get features stop mask */
		volatile u32 get_features_stop_mask;
		/* get feature command return value       (0x058) */
		volatile u32 get_features_value;
		/* auto_get_features_ctrl	(0x05c) */
		volatile u32 auto_get_features_ctrl;
		/* auto read type */
		volatile u32 auto_read_type;
		/* plane setting */
		volatile u32 plane_ctrl;
		/* status register	(0x068) */
		volatile u32 sr;
		/* interrupt status register */
		volatile u32 isr;
		/* interrupt mask register	(0x070) */
		volatile u32 imr;
		/* interrupt clear register */
		volatile u32 isr_cr;
		/* dummy2[2]	(0x078), (0x07c) */
		volatile u32 dummy2[2];
		/* logic address status register	(0x080) */
		volatile u32 la_status;
		/* physical address of logic block0 */
		volatile u32 pa0;
		/* physical address of logic block1	(0x088) */
		volatile u32 pa1;
		/* physical address of logic blockx */
		volatile u32 pax;
		/* logic address of logic blockx	(0x090) */
		volatile u32 lax;
		/* It is page offset in block which is in cache */
		volatile u32 current_page;
		/* dummy3[2]	0x098, 0x09c */
		volatile u32 dummy3[2];
		/* main state	(0x0a0) */
		volatile u32 main_state;
		/* logic find state */
		volatile u32 la_find_state;
		/* auto read state	(0x0a8) */
		volatile u32 auto_read_state;
		/* protocal state */
		volatile u32 protocol_state;
		/* gp state	(0xb0) */
		volatile u32 gp_state;
		/* dummy4[3]	0x0b4, 0x0b8, 0x0bc */
		volatile u32 dummy4[3];
		/* ecc write control register	(0x0c0) */
		volatile u32 ecc_write_ctrl;
		volatile u32 dummy5;
		/* ecc read control register	(0x0c8) */
		volatile u32 ecc_read_ctrl;
		/* ecc read statue register */
		volatile u32 ecc_read_status;
		/* dummy6[8]	0x0d0 - 0x0ec */
		volatile u32 dummy6[8];
		/* debug address0	(0x0f0) */
		volatile u32 debug_addr0;
		/* debug address1 */
		volatile u32 debug_addr1;
		/* dummy7[2]	0x0f8 , 0x0fc */
		volatile u32 dummy7[2];
		/* data address 0x100 */
		volatile u32 da;
		/* data value */
		volatile u32 dv;
		/* dummy8[958]	0x108 - 0xffc */
		volatile u32 dummy8[958];
		/* data_fifo[528]	0x1000 - 0x1840 */
		volatile u32 data_fifo[528];
};

struct rts_nfc {
	struct nand_hw_control controller;
	const struct nfc_cfg *cfg;
	struct udevice *dev;

	struct clk *nfc_clk;
	struct clk *ahb_clk;
	void __iomem *regs;

	int selected_bank;
	u32 band_offset;
	u32 cur_ecc;
	u32 cur_timing;

	u8 *page_buf;
	u32 *oob_buf;

	unsigned long assigned_cs;
};

struct spinand_slave {
	unsigned int max_write_size;
	unsigned short flags;
	unsigned char read_cmd;
	unsigned char read_type;
	unsigned char read_dummy;
	unsigned char write_cmd;
	unsigned char write_type;
	unsigned char write_dummy;
};

struct rts_spinand_info {
	struct	nand_chip chip;
	int	cur_chip;
	u32	data_buf_size;

	u8	marking_block_bad;
	u8	raw_oob_mode;

	/* Functions with altered behaviour */
	int	(*hooked_read_oob)(struct mtd_info *mtd,
			loff_t from, struct mtd_oob_ops *ops);
	int	(*hooked_write_oob)(struct mtd_info *mtd,
			loff_t to, struct mtd_oob_ops *ops);
	int	(*hooked_block_markbad)(struct mtd_info *mtd,
			loff_t ofs);

	/* spi nand flash controller  */
	struct spi_nandflash_portmap	*base_addr;
	struct spinand_state		*spinand_state;
	struct spinand_slave		spi_dev;

	/*logic address when encounter bad block*/
	u32 logic_ad;
};

struct rts_nfc_platdata {
	void *regs;
};

static inline struct spi_nandflash_portmap *get_rts_spinfc_base(void)
{
	return (struct spi_nandflash_portmap *)RTS_SPINFC_BASEADDR;
}

extern int spinand_mtd(struct mtd_info *mtd);
extern void spinand_mtd_release(struct mtd_info *mtd);

#endif /* __RTS_SPINAND_H__ */
