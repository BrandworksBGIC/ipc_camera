// SPDX-License-Identifier: GPL-2.0
/*
 * SHEIPA SPI controller driver
 *
 * Author: Realtek PSP Group
 *
 * Copyright 2015, Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _RTS_QSPI_H
#define _RTS_QSPI_H

#ifndef _DW_COMMON_H
#define _DW_COMMON_H

/* SPIC config */
#define ps_CC_SPI_FLASH_NUM_SLAVES            1
#define ps_CC_SPI_FLASH_TX_FIFO_DEPTH         0x20
#define ps_CC_SPI_FLASH_RX_FIFO_DEPTH         0x20
#define ps_CC_SPI_FLASH_ID                    0x0
#define ps_CC_SPI_FLASH_DFLT_SCPOL            0x0
#define ps_CC_SPI_FLASH_DFLT_SCPH             0x0
#define ps_CC_SPI_FLASH_CLK_PERIOD            400
#define ps_CC_SPI_FLASH_VERSION_ID            0x0

/*
 * Place the specified value into the specified bits of a word of data
 * (first the data is read, and the non-specified bits are re-written).
 */
#define DW_BITS_SET_VAL(__datum, __bfws, __val, bit_num)                          \
	((__datum) = ((uint32_t) (__datum) & ~DW_BIT_MASK_WIDTH(__bfws, bit_num)) |    \
	((__val << (__bfws)) & DW_BIT_MASK_WIDTH(__bfws, bit_num)))

#define DW_BIT_SET(__datum, __bfws)                          \
	((__datum) = ((uint32_t) (__datum) & ~DW_BIT_MASK(__bfws)) |    \
	(DW_BIT_MASK(__bfws)))

/*
 * Returns the relevant bits masked from the data word, still at their
 * original offset.
 */
#define DW_BIT_GET_UNSHIFTED(__datum, __bfws)                       \
	((uint32_t) ((__datum) & DW_BIT_MASK(__bfws)))

/* Returns a mask with the bits to be addressed set and all others cleared. */
#define DW_BIT_MASK(__bfws) ((uint32_t) ((__bfws) == 32) ?  \
	0x0 : (0x1 << (__bfws)))

#define DW_BIT_MASK_WIDTH(__bfws, __bits) ((uint32_t) ((__bfws) == 32) ?  \
	0xFFFFFFFF : (((1 << (__bits)) - 1) << (__bfws)))

#endif

#define CONFIG_BSP_SPIC_PADDR		0x18030000
#define SPIC_PGM_FIFO_INIT_ADDR		0x18860080

#define SPIC_NOR_DDR_CFG_ADDR		0x188600a4
/* FLASH base address for auto mode */
#define FIFO_SIZE	128
// #define FIFO_HALF_SIZE	(FIFO_SIZE / 2)

/* Maximum number of data frames */
#define MAX_NDF		0x00010000

#define NORMAL_BAUDR		4
#define QUAD_BAUDR		4
#define FAST_READ_BAUDR		2
#define RD_ID_BAUDR		4

/* Soc tuning dummy cycle only. */

#ifdef CONFIG_NOR_DTR_MODE
#define DEF_RD_TUNING_DUMMY_CYCLE  0x6
#else
#define DEF_RD_TUNING_DUMMY_CYCLE  0x2
#endif

#define DEF_WR_BLOCK_BOUND         256

/* General flash opcode. */
#define CMD_PAGE_PROGRAM		0x02
#define CMD_PAGE_PROGRAM_4B		0X12
#define CMD_READ_ARRAY_SLOW		0x03
#define CMD_WRITE_DISABLE		0x04	/* write disable */
#define CMD_READ_CONF_STATUS		0x15
#define CMD_WRITE_ENABLE		0x06
#define CMD_READ_ARRAY_FAST		0x0b
#define CMD_READ_ARRAY_FAST_4B		0x0c
#define CMD_READ_ID			0x9f
#define CMD_ERASE_CHIP			0xc7	/* chip erase */
#define CMD_ERASE_4K			0x20	/* erase 4KiB Block */
#define CMD_ERASE_4K_4B			0x21
#define CMD_ERASE_64K			0xd8	/* sector erase(usually 64KiB) */
#define CMD_ERASE_64K_4B		0xdc
#define CMD_ENTER_4B			0xb7
#define CMD_EXIT_4B			0xe9
#define CMD_READ_4B			0x13
#define CMD_WRITE_STATUS_1		0x01
#define CMD_WRITE_STATUS_2		0x31
#define CMD_WRITE_STATUS_3		0x11
#define CMD_WRITE_EX_READ		0x83
#define CMD_WRITE_READ_PARAMETER	0xc0
#define CMD_READ_STATUS_1		0x05
#define CMD_READ_STATUS_2		0x35
#define CMD_READ_STATUS_4		0x09
#define CMD_READ_STATUS_5		0x95
#define CMD_READ_EX_READ		0x81
#define CMD_ENTER_QPI_I			0x35
#define CMD_EXIT_QPI_I			0xF5
#define CMD_ENTER_QPI_II		0x38
#define CMD_EXIT_QPI_II			0xFF

/* Support auto mode flash opcode only. */
/* Macronix: Extend flash opcode and used */
#define PPX2_I		0x02
#define PPX2_II		0x02
#define PPX4_I		0x02
#define PPX4_II		0x38
#define READX2_I	0x03
#define READX2_IO	0xbb	/* data and addr channel */
#define READX4_I	0x03
#define READX4_IO	0xeb
#define ERROR_SINGLE_WRITE_CMD	0xebeb

/* support auto mode */
#define AUTO_MODE	0xf5

/* Support address 4 byte opcode for large size flash */
#define EN4B		0xb7	/* Enter 4 byte mode */
#define EX4B		0xe9	/* Exit  4 byte mode*/

/* Bank addr access commands */
#define CMD_EXTNADDR_WREAR	0xC5	/* Write extended address register */
#define CMD_EXTNADDR_RDEAR	0xC8	/* Read extended address register */

/* SPIC CFG register offsets */
#define SPIC_PGM_FIFO_INIT0		0x0000
#define SPIC_PGM_FIFO_INIT1		0x0004
#define SPIC_PGM_FIFO_INIT2		0x0008
#define SPIC_PGM_FIFO_INIT3		0x000c
#define SPIC_PGM_FIFO_INIT4		0x0010
#define SPIC_PGM_FIFO_INIT5		0x0014
#define SPIC_PGM_FIFO_INIT6		0x0018
#define SPIC_PGM_FIFO_INIT7		0x001c
#define SPIC_PGM_FIFO_WPTR		0x0020
#define SPIC_NOR_DDR_CFG		0x0024

/* macros to compose the values */
#define TMOD_SEND	0xfffffcff
#define USER_MODE	0x80000000
#define UAR			0x40000000
#define TMOD_RECV	0x0300
#define CLC_CH	0xffc0ffff
#define CMD_CH_DUAL	0x00100000
#define CMD_CH_QUAD	0x00200000
#define ST_CMD_CH_QUAD	0x08000000
#define DATA_CH_SINGLE	0xfff3ffff
#define DATA_CH_DUAL	0x00040000
#define DATA_CH_QUAD	0x00080000
#define ADDR_CH_SINGLE	0xfffcffff
#define ADDR_CH_DUAL	0x00010000
#define ADDR_CH_QUAD	0x00020000
#define CTRLR0_CH_VALID_CMD_EN	(1 << 12)
#define QPI_CH	(CMD_CH_QUAD|ADDR_CH_QUAD|DATA_CH_QUAD)

#define TIME_OUT_VALUE	0x02000000
#define TIME_OUT_VALUE_QPI	0x04000000
#define AUTO_CHECK_BAUDR	0x10

#define ACEIM	0x00000100
#define ACSIM	0x00000800

#define DDR_EN	0x6000
#define DDR_MODE_EN	0x100

/*
 *  Used in conjunction with bitops.h to access register bitfields.
 *  They are defined as bit offset/mask pairs for each DMA register
 *  bitfield.
 */
#define SPI_FLASH_CTRLR0_TMOD_SHIFT         ((uint32_t)    8)
#define SPI_FLASH_CTRLR0_TMOD_WIDTH         ((uint32_t)    2)
#define SPI_FLASH_SR_BUSY_SHIFT             ((uint32_t)    0)
#define SPI_FLASH_SR_TXE_SHIFT              ((uint32_t)    5)
#define SPI_FLASH_AUTO_LEN_DUM_SHIFT        ((uint32_t)     0)
#define SPI_FLASH_AUTO_LEN_DUM_WIDTH        ((uint32_t)    12)
#define SPI_FLASH_RD_PHY_DUM_SHIFT	    ((uint32_t)    12)
#define SPI_FLASH_RD_PHY_DUM_WIDTH	    ((uint32_t)    4)
#define SPI_FLASH_USER_LEN_DUM_SHIFT        ((uint32_t)     0)
#define SPI_FLASH_USER_LEN_DUM_WIDTH        ((uint32_t)    12)
#define SPI_FLASH_USER_LEN_CMD_SHIFT        ((uint32_t)    12)
#define SPI_FLASH_USER_LEN_CMD_WIDTH        ((uint32_t)     2)
#define SPI_FLASH_USER_LEN_ADDR_SHIFT        ((uint32_t)   16)
#define SPI_FLASH_USER_LEN_ADDR_WIDTH        ((uint32_t)    4)
#define SPI_FLASH_SSIENR                     ((uint32_t)    0)
#define SPI_FLASH_RISR_ACEIR				((uint32_t)     8)
#define SPI_FLASH_RISR_ACSIR				((uint32_t)     11)

/*
 *  This data type is used to describe read type with multi_channel
 */
enum spi_flash_byte_num {
	DATA_BYTE = 0,
	DATA_HALF = 1,
	DATA_WORD = 2
};

enum spi_flash_dr_number {
	DR0               = 0,
	DR1               = 1,
	DR2               = 2,
	DR3               = 3,
	DR4               = 4,
	DR5               = 5,
	DR6               = 6,
	DR7               = 7,
	DR8               = 8,
	DR9               = 9,
	DR10              = 10,
	DR11              = 11,
	DR12              = 12,
	DR13              = 13,
	DR14              = 14,
	DR15              = 15,
	DR16              = 16,
	DR17              = 17,
	DR18              = 18,
	DR19              = 19,
	DR20              = 20,
	DR21              = 21,
	DR22              = 22,
	DR23              = 23,
	DR24              = 24,
	DR25              = 25,
	DR26              = 26,
	DR27              = 27,
	DR28              = 28,
	DR29              = 29,
	DR30              = 30,
	DR31              = 31
};

enum flash_rd_multi_type {
	RD_MULTI_NONE = 0x00,
	RD_DUAL_O     = 0x01,
	RD_DUAL_IO    = 0x02,
	RD_QUAD_O     = 0x03,
	RD_QUAD_IO    = 0x04
};

/*
 * This data type is used to describe write type with multi_channel
 */
enum flash_wr_multi_type {
	WR_MULTI_NONE = 0x10,
	WR_DUAL_I     = 0x11,
	WR_DUAL_II    = 0x12,
	WR_QUAD_I     = 0x13,
	WR_QUAD_II    = 0x14
};

struct rts_spi_platdata {
	void *regs;
	void *rst_regs;
	int frequency;			/* default clock frequency, -1 for none */
	uint deactivate_delay_us;	/* Delay to wait after deactivate */
};

struct rts_spi {
	struct spi_slave *slave;
	void *regs;
	void *rst_regs;
	void *comp_param;
	unsigned int mode;
	unsigned int freq;
	unsigned long last_transaction_us;
};

#define RD_QUAD_IO_T	0x10
#define RD_QUAD_O_T	0x08
#define RD_DUAL_IO_T	0x04
#define RD_DUAL_O_T	0x02
#define FRD_SINGLE_T	0x01

/*
 * This is the structure used for accessing the spi_flash register
 * portmap.
 */
struct spi_flash_portmap {
/* Channel registers
 * The offset address for each of the channel registers
 * is shown for channel 0. For other channel numbers
 * use the following equation.
 * offset = (channel_num * 0x058) + channel_0 offset
 */
	volatile uint32_t ctrlr0;    /* Control Reg 0           (0x000) */
	volatile uint32_t ctrlr1;
	volatile uint32_t ssienr;    /* SPIC enable Reg1        (0x008) */
	volatile uint32_t mwcr;
	volatile uint32_t ser;       /* Slave enable Reg        (0x010) */
	volatile uint32_t baudr;
	volatile uint32_t txftlr;    /* TX_FIFO threshold level (0x018) */
	volatile uint32_t rxftlr;
	volatile uint32_t txflr;     /* TX_FIFO threshold level (0x020) */
	volatile uint32_t rxflr;
	volatile uint32_t sr;        /* Destination Status Reg  (0x028) */
	volatile uint32_t imr;
	volatile uint32_t isr;       /* Interrupt Stauts Reg    (0x030) */
	volatile uint32_t risr;
	volatile uint32_t txoicr;    /* TX_FIFO overflow_INT clear (0x038) */
	volatile uint32_t rxoicr;
	volatile uint32_t rxuicr;    /* RX_FIFO underflow_INT clear (0x040) */
	volatile uint32_t msticr;
	volatile uint32_t icr;       /* Interrupt clear Reg     (0x048) */
	volatile uint32_t dmacr;
	volatile uint32_t dmatdlr;   /* DMA TX_data level       (0x050) */
	volatile uint32_t dmardlr;
	volatile uint32_t idr;       /* Identiation Scatter Reg (0x058) */
	volatile uint32_t spi_flash_version;
	union{
		volatile uint8_t  byte;
		volatile uint16_t half;
		volatile uint32_t word;
	} dr[32];
	volatile uint32_t rd_fast_single;
	volatile uint32_t rd_dual_o; /* Read dual data cmd Reg  (0x0e4) */
	volatile uint32_t rd_dual_io;
	volatile uint32_t rd_quad_o; /* Read quad data cnd Reg  (0x0ec) */
	volatile uint32_t rd_quad_io;
	volatile uint32_t wr_single; /* write single cmd Reg    (0x0f4) */
	volatile uint32_t wr_dual_i;
	volatile uint32_t wr_dual_ii;/* write dual addr/data cmd(0x0fc) */
	volatile uint32_t wr_quad_i;
	volatile uint32_t wr_quad_ii;/* write quad addr/data cnd(0x104) */
	volatile uint32_t wr_enable;
	volatile uint32_t rd_status; /* read status cmd Reg     (0x10c) */
	volatile uint32_t ctrlr2;
	volatile uint32_t fbaudr;    /* fast baud rate Reg      (0x114) */
	volatile uint32_t user_length;
	volatile uint32_t auto_length; /* Auto addr length Reg  (0x11c) */
	volatile uint32_t valid_cmd;
	volatile uint32_t flash_size; /* Flash size Reg         (0x124) */
	volatile uint32_t flush_fifo;
	volatile uint32_t dummy_byte; /*Dummy byte value        (0x12c) */
	volatile uint32_t tx_ndf;
	volatile uint32_t page_size;  /*page size               (0x134) */
	volatile uint32_t trp0;
};

struct spi_flash_param {
	uint32_t spi_flash_num_slaves;		/* slaves number */
	uint32_t spi_flash_tx_fifo_depth;	/* TX fifo depth number */
	uint32_t spi_flash_rx_fifo_depth;	/* RX fifo depth number */
	uint32_t spi_flash_idr;			/* ID code */
	uint32_t spi_flash_scpol;		/* Serial clock polarity */
	uint32_t spi_flash_scph;		/* Serial clock phase */
	uint32_t spi_flash_clk_period;		/* serial clock period */
	uint32_t spi_flash_version_id;		/* spi flash ID */
};

/* This function is used to wait the SSI is not at busy state. */
static void flash_wait_busy(struct rts_spi *dev);
/* This function is used to read status of flash. */
static uint8_t flash_get_status(struct rts_spi *dev);

#ifdef CONFIG_DM_SPI
static void spi_cs_activate(struct udevice *uflash);
static void spi_cs_deactivate(struct udevice *uflash);
#endif

void spi_flash_wait_busy(struct rts_spi *dev);

int flash_flush_rst_fifo(struct spi_slave *dev);

int flash_set_rst_fifo_wptr(int reset_flow_cmd_num);

int enable_spi_nor_ddr_mode(struct rts_spi *dev);

int disable_spi_nor_ddr_mode(struct rts_spi *dev);

#endif
