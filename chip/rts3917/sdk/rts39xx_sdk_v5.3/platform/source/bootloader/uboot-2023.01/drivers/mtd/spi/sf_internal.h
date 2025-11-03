/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * SPI flash internal definitions
 *
 * Copyright (C) 2008 Atmel Corporation
 * Copyright (C) 2013 Jagannadha Sutradharudu Teki, Xilinx Inc.
 */

#ifndef _SF_INTERNAL_H_
#define _SF_INTERNAL_H_

#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/compiler.h>

#define SPI_NOR_MAX_ID_LEN	6
#define SPI_NOR_MAX_ADDR_WIDTH	4

/* QPI commands */
#define CMD_ENTER_QPI_I			0x35
#define CMD_EXIT_QPI_I			0xF5
#define CMD_ENTER_QPI_II		0x38
#define CMD_EXIT_QPI_II			0xFF

/* Write commands */
#define CMD_WRITE_STATUS		0x01
#define CMD_PAGE_PROGRAM		0x02
#define CMD_WRITE_DISABLE		0x04
#define CMD_WRITE_ENABLE		0x06
#define CMD_QUAD_PAGE_PROGRAM		0x32
#define CMD_WRITE_EVCR			0x61
#define CMD_WRSR_2	0x31	/* Winbond Write status register-2 1 byte */
#define CMD_WRSR_3	0x11	/* Winbond Write status register-3 1 byte */
#define CMD_WRITE_STATUS_1	0x01
#define CMD_WRITE_STATUS_2	0x31
#define CMD_WRITE_STATUS_3	0x11
#define CMD_WRITE_EX_READ	0x83
#define CMD_WRITE_READ_PARAMETER	0xc0

/* Read commands */
#define CMD_READ_ARRAY_SLOW		0x03
#define CMD_READ_ARRAY_FAST		0x0b
#define CMD_READ_DUAL_OUTPUT_FAST	0x3b
#define CMD_READ_DUAL_IO_FAST		0xbb
#define CMD_READ_QUAD_OUTPUT_FAST	0x6b
#define CMD_READ_QUAD_IO_FAST		0xeb
#define CMD_READ_ID			0x9f
#define CMD_READ_STATUS		0x05
#define CMD_READ_CONFIG_MX		0x15
#define CMD_READ_STATUS1		0x35
#define CMD_READ_CONFIG			0x15
#define CMD_FLAG_STATUS			0x70
#define CMD_READ_EVCR			0x65
#define CMD_READ_STATUS_1		0x05
#define CMD_READ_STATUS_2		0x35
#define CMD_READ_STATUS_3		0x15
#define CMD_READ_STATUS_4		0x09
#define CMD_READ_STATUS_5		0x95
#define CMD_READ_EX_READ		0x81

struct flash_info {
#if !CONFIG_IS_ENABLED(SPI_FLASH_TINY)
	char		*name;
#endif

	/*
	 * This array stores the ID bytes.
	 * The first three bytes are the JEDIC ID.
	 * JEDEC ID zero means "no ID" (mostly older chips).
	 */
	u8		id[SPI_NOR_MAX_ID_LEN];
	u8		id_len;

	/* The size listed here is what works with SPINOR_OP_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned int	sector_size;
	u16		n_sectors;

	u16		page_size;
	u16		addr_width;

	u32		flags;
#define SECT_4K			BIT(0)	/* SPINOR_OP_BE_4K works uniformly */
#define SPI_NOR_NO_ERASE	BIT(1)	/* No erase command needed */
#define SST_WRITE		BIT(2)	/* use SST byte programming */
#define SPI_NOR_NO_FR		BIT(3)	/* Can't do fastread */
#define SECT_4K_PMC		BIT(4)	/* SPINOR_OP_BE_4K_PMC works uniformly */
#define SPI_NOR_DUAL_READ	BIT(5)	/* Flash supports Dual Read */
#define SPI_NOR_QUAD_READ	BIT(6)	/* Flash supports Quad Read */
#define USE_FSR			BIT(7)	/* use flag status register */
#define SPI_NOR_HAS_LOCK	BIT(8)	/* Flash supports lock/unlock via SR */
#define SPI_NOR_HAS_TB		BIT(9)	/*
					 * Flash SR has Top/Bottom (TB) protect
					 * bit. Must be used with
					 * SPI_NOR_HAS_LOCK.
					 */
#define	SPI_S3AN		BIT(10)	/*
					 * Xilinx Spartan 3AN In-System Flash
					 * (MFR cannot be used for probing
					 * because it has the same value as
					 * ATMEL flashes)
					 */
#define SPI_NOR_4B_OPCODES	BIT(11)	/*
					 * Use dedicated 4byte address op codes
					 * to support memory size above 128Mib.
					 */
#define NO_CHIP_ERASE		BIT(12) /* Chip does not support chip erase */
#define SPI_NOR_SKIP_SFDP	BIT(13)	/* Skip parsing of SFDP tables */
#define USE_CLSR		BIT(14)	/* use CLSR command */
#define SPI_NOR_HAS_SST26LOCK	BIT(15)	/* Flash supports lock/unlock via BPR */
#define SPI_NOR_OCTAL_READ	BIT(16)	/* Flash supports Octal Read */
#define SPI_NOR_OCTAL_DTR_READ	BIT(17)	/* Flash supports Octal DTR Read */
#define SPI_NOR_WR_QUAD_I	BIT(18) /* Flash quad mode WR_I */
#define SPI_NOR_WR_QUAD_II	BIT(19) /* Flash quad mode WR_II */
#define SPI_NOR_2IO_READ	BIT(20)	/* 2xIO Read */
#define SPI_NOR_4IO_READ	BIT(21)	/* 4xIO Read */
#define SPI_NOR_DTR		BIT(22)	/* DTR Read */
#define QPI_I			BIT(23)	/* Enter QPI:0x35, Exit QPI:0xF5 */
#define QPI_II			BIT(24)	/* Enter QPI:0x38, Exit QPI:0xFF */

	u16		reg_flags;
#define SR_CFG			BIT(0)
#define SR_3REG			BIT(1)
#define SR_3REG1		BIT(2)
#define SR_3REG2		BIT(3)
#define SR_1			BIT(4)
#define SR_REG1			BIT(5)
#define SR_CFG1			BIT(6)
#define SR_EX_RD_REG2		BIT(7)
#define SR_RD_REG2		BIT(8)
	u8		dummy_cycle;

	u8		bp_flags;
/*
 * Flash SR has 4 bit fields (BP0-3)
 * for block protection.
 */
#define SPI_NOR_4BIT_BP		BIT(0)
/*
 * BP3 is bit 6 of status register.
 * Must be used with SPI_NOR_4BIT_BP.
 */
#define SPI_NOR_BP3_SR_BIT6	BIT(1)
/* Software write protect */
#define	SPI_NOR_SWP		BIT(2)
	u16		reset_flow[4];
};

extern const struct flash_info spi_nor_ids[];

#define JEDEC_MFR(info)	((info)->id[0])
#define JEDEC_ID(info)		(((info)->id[1]) << 8 | ((info)->id[2]))

/* Get software write-protect value (BP bits) */
int spi_flash_cmd_get_sw_write_prot(struct spi_flash *flash);


#if CONFIG_IS_ENABLED(SPI_FLASH_MTD)
int spi_flash_mtd_register(struct spi_flash *flash);
void spi_flash_mtd_unregister(struct spi_flash *flash);
#else
static inline int spi_flash_mtd_register(struct spi_flash *flash)
{
	return 0;
}

static inline void spi_flash_mtd_unregister(struct spi_flash *flash)
{
}
#endif

int flash_set_rst_fifo(struct spi_slave *slave, const u16 reset_flow[]);
int spi_flash_set_auto_mode(struct spi_nor *nor);
int flash_enable_qpi(struct spi_nor *nor);

#endif /* _SF_INTERNAL_H_ */
