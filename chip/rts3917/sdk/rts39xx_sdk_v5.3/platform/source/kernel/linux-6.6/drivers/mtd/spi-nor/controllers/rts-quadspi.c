// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Realtek IPCam RTS39XX SPI Controller
 *
 * Copyright (C) 2015 Darcy Lu, Realtek <darcy_lu@realsil.com.cn>
 * Copyright (C) 2016 Jim Cao, Realtek <jim_cao@realsil.com.cn>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/ctype.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/spi-nor.h>
#include <linux/mtd/cfi.h>
#include <linux/pinctrl/consumer.h>
#include "../core.h"

/*
 * Manufacturer IDs
 *
 * The first byte returned from the flash after sending opcode SPINOR_OP_RDID.
 * Sometimes these are the same as CFI IDs, but sometimes they aren't.
 */
#define SNOR_MFR_ATMEL		CFI_MFR_ATMEL
#define SNOR_MFR_GIGADEVICE	0xc8
#define SNOR_MFR_INTEL		CFI_MFR_INTEL
#define SNOR_MFR_MICRON		CFI_MFR_ST /* ST Micro <--> Micron */
#define SNOR_MFR_MACRONIX	CFI_MFR_MACRONIX
#define SNOR_MFR_SPANSION	CFI_MFR_AMD
#define SNOR_MFR_SST		CFI_MFR_SST
#define SNOR_MFR_WINBOND	0xef /* Also used by some Spansion */
#define SNOR_MFR_BOYAMICRO	0x68
#define SNOR_MFR_EON		0x1c
#define SNOR_MFR_XTX		0x0b
#define SNOR_MFR_FM			0xa1
#define SNOR_MFR_XMC		CFI_MFR_ST
#define SNOR_MFR_XMC_MT_A	0x70
#define SNOR_MFR_XMC_MT_B	0x60
#define SNOR_MFR_PUYA		0x85
#define SNOR_MFR_XD		0xd8	/* manufacturer is Gigadevice */
#define SNOR_MFR_ZBIT		0x5e

/*
 * The definition of the FIFO look up table shows below:
 *
 *  ---------------------------------------------
 *  | INSTR(1B) | ADDR(1~4B) | DATA(rest FIFO) |
 *  ---------------------------------------------
 */

/* SPI register offsets */
#define CTRLR0					0x0000
#define RX_NDF					0x0004
#define SSIENR					0x0008
#define MWCR					0x000c
#define SER					0x0010
#define BAUDR					0x0014
#define TXFTLR					0x0018
#define RXFTLR					0x001c
#define TXFLR					0x0020
#define RXFLR					0x0024
#define SR					0x0028
#define IMR					0x002c
#define ISR					0x0030
#define RISR					0x0034
#define TXOICR					0x0038
#define RXOICR					0x003c
#define RXUICR					0x0040
#define MSTICR					0x0044
#define ICR					0x0048
#define DMACR					0x004c
#define DMATDLR					0x0050
#define DMARDLR					0x0054
#define IDR					0x0058
#define SPIC_VERSION				0x005c
#define DR					0x0060
#define READ_FAST_SINGLE			0x00e0
#define READ_DUAL_DATA				0x00e4
#define READ_DUAL_ADDR_DATA			0x00e8
#define READ_QUAD_DATA				0x00ec
#define READ_QUAD_ADDR_DATA			0x00f0
#define WRITE_SINGLE				0x00f4
#define WRITE_DUAL_DATA				0x00f8
#define WRITE_DUAL_ADDR_DATA			0x00fc
#define WRITE_QUAD_DATA				0x0100
#define WRITE_QUAD_ADDR_DATA			0x0104
#define WRITE_ENABLE				0x0108
#define READ_STATUS				0x010c
#define CTRLR2					0x0110
#define FBAUDR					0x0114
#define USER_LENGTH				0x0118
#define AUTO_LENGTH				0x011c
#define VALID_CMD				0x0120
#define FLASH_SIZE				0x0124
#define FLUSH_FIFO				0x0128
#define TX_NDF				0x0130
#define PGM_RST_FIFO				0x0140

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

/* Bit fields in CTRLR0 */
#define SCPH					6
#define SCPOL					7
#define TMOD_OFFSET				8
#define TMOD_MASK				3
#define TRANSMIT_MODE				0
#define RECEIVE_MODE				3
#define DDR_EN_MASK					7
#define DDR_EN						3
#define DDR_EN_OFFSET				13
#define ADDR_CH_OFFSET				16
#define ADDR_CH_MASK				3
#define DATA_CH_OFFSET				18
#define DATA_CH_MASK				3
#define CMD_CH_OFFSET				20
#define CMD_CH_MASK				3
#define USER_MODE			31


/* Bit fields in SR */
#define BUSY					0
/* Bit fields in SSIENR */
#define SPIC_EN					0
#define ATCK_CMD				1
#define PGM_RST_TEST_EN				4

/* Bit fields in BAUDR */
#define SCKDV					0
#define SCKDV_WIDTH				16
#define SCKDV_MASK				((1 << SCKDV_WIDTH) - 1)

/* Bit fields in USER_LENGTH */
#define USER_RD_DUMMY_LENGTH				0
#define USER_RD_DUMMY_LENGTH_MASK			((1 << 12) - 1)
#define USER_CMD_LENGTH		12
#define USER_CMD_LENGTH_MASK	3
#define USER_ADDR_LENGTH		16
#define USER_ADDR_LENGTH_MASK	0xF

/* Bit fields in AUTO_LENGTH */
#define AUTO_RD_DUMMY_LENGTH		0
#define AUTO_RD_DUMMY_LENGTH_MASK	((1 << 12) - 1)
#define AUTO_IN_PHYSICAL_CYC		12
#define AUTO_IN_PHYSICAL_CYC_MASK	0xf
#define AUTO_ADDR_LENGTH		16
#define AUTO_ADDR_LENGTH_MASK		0xf
#define AUTO_RDSR_DUMMY_LENGTH		20
#define AUTO_RDSR_DUMMY_LENGTH_MASK	0xff

/* Bit fields in VALID_CMD */
#define FRD_SINGLE				0
#define RD_DUAL_I				1
#define RD_DUAL_IO				2
#define RD_QUAD_O				3
#define RD_QUAD_IO				4
#define WR_DUAL_I				5
#define WR_DUAL_II				6
#define WR_QUAD_I				7
#define WR_QUAD_II				8
#define WR_BLOCKING				9

/* Bit fields in CTRLR2 */
#define SO_DNUM					0
#define WPN_SET					1
#define WPN_DNUM				2
#define SEQ_EN					3
#define FIFO_ENTRY				4
#define FIFO_ENTRY_MASK				(0xf << 4)
#define RX_FIFO_ENTRY				8

/* Bit fileds in PGM_RST ctrl reg */
#define PGMRST_CMD_VAL				0
#define PGMRST_CMD_CH				8
#define PGMRST_STATE				10
#define PGMRST_COUNT				12

/* Bit fileds in SPIC_NOR_DDR_CFG reg */
#define SPIC_DDR_EN			0x100

#define SPI_CH				0
#define QSPI_CH				1
#define QPI_CH				2
#define OPI_CH				3

#define STATE_END			0
#define STATE_NEXT			1
#define STATE_KEEP			2
#define STATE_NOP			3

#define COUNT_2EXP4			2
#define COUNT_2EXP16			4
#define COUNT_2EXP23			8

#define RTSX_QSPI_DRV_NAME		"rts-spi-nor"

static char *channels;
module_param(channels, charp, 0444);
MODULE_PARM_DESC(channels,
	"spi channel mode, fast/dual/quad/qpi/dtr");

/**
 * struct spi_nor_xfer_cfg - Structure for defining a Serial Flash transfer
 * @wren:               command for "Write Enable", or 0x00 for not required
 * @cmd:                command for operation
 * @cmd_pins:           number of pins to send @cmd (1, 2, 4)
 * @addr:               address for operation
 * @addr_pins:          number of pins to send @addr (1, 2, 4)
 * @addr_width:         number of address bytes
 *                      (3,4, or 0 for address not required)
 * @mode:               mode data
 * @mode_pins:          number of pins to send @mode (1, 2, 4)
 * @mode_cycles:        number of mode cycles (0 for mode not required)
 * @dummy_cycles:       number of dummy cycles (0 for dummy not required)
 */
struct spi_nor_xfer_cfg {
	u8			wren;
	u8			cmd;
	u8			cmd_pins;
	u32			addr;
	u8			addr_pins;
	u8			addr_width;
	u8			ddr_en;
	u8			mode;
	u8			mode_pins;
	u8			mode_cycles;
	u8			dummy_cycles;
};

enum rts_qspi_devtype {
	TYPE_FPGA = (1 << 0),
	RTS_QUADSPI_RTS3917 = (1 << 1),
};

enum read_mode {
	SPI_NOR_NORMAL = 0,
	SPI_NOR_FAST,
	SPI_NOR_DUAL,
	SPI_NOR_QUAD,
	SPI_NOR_DTR,
	SPI_NOR_QIO,
};

/* rts qspi */
struct rts_qspi {
	struct spi_nor		nor;
	struct platform_device	*pdev;
	int			irq;
	void __iomem		*regs;
	void __iomem		*spic_cfg_regs;
	phys_addr_t		phybase;

	struct clk		*clk;
	u32			spiclk_hz;
	u32			max_speed_hz;
	u32			min_speed_hz;

	struct spi_board_info	*bi;

	bool			use_dma;
	int			fifo_size;
	int			fifo_entry;

	struct completion		*done;
	u8				status;
#define CHECK_TIMEOUT			BIT(0)
	int				timeout_ms;
	spinlock_t			__lock;
	unsigned long			__lock_flags;

	enum rts_qspi_devtype devtype;

	struct {
		struct pinctrl *p;
		struct pinctrl_state *quad_state;
	} pins;
};

static struct spi_swp {
	unsigned long time;
	bool swp_flag;
	struct task_struct *enable_swp_task;
} spi_nor_swp;

struct rts_qspi_devtype_data {
	enum rts_qspi_devtype devtype;
	int fifo_entry;
	int fifo_size;
};

static struct rts_qspi_devtype_data rts3917_data = {
	.devtype = RTS_QUADSPI_RTS3917,
	.fifo_entry = 7,
	.fifo_size = 128,
};

static inline u32 rts_readl(struct rts_qspi *rqspi, u32 reg)
{
	return readl(rqspi->regs + reg);
}

static inline u16 rts_readw(struct rts_qspi *rqspi, u32 reg)
{
	return readw(rqspi->regs + reg);
}

static inline u8 rts_readb(struct rts_qspi *rqspi, u32 reg)
{
	return readb(rqspi->regs + reg);
}

static inline void rts_writel(struct rts_qspi *rqspi, u32 reg, u32 val)
{
	writel(val, rqspi->regs + reg);
}

static inline void rts_writew(struct rts_qspi *rqspi, u32 reg, u16 val)
{
	writew(val, rqspi->regs + reg);
}

static inline void rts_writeb(struct rts_qspi *rqspi, u32 reg, u8 val)
{
	writeb(val, rqspi->regs + reg);
}

static void addr2cmd(int addr_width, u32 addr, u8 *cmd)
{
	cmd[1] = addr >> (addr_width * 8 - 8);
	cmd[2] = addr >> (addr_width * 8 - 16);
	cmd[3] = addr >> (addr_width * 8 - 24);
	cmd[4] = addr >> (addr_width * 8 - 32);
}

static int rts_qspi_controller_ready(struct rts_qspi *rqspi)
{
	u32 cnt;
	u32 reg;

	for (cnt = 0; cnt < 1000; cnt++) {
		reg = rts_readl(rqspi, SSIENR);
		if (!(reg & BIT(SPIC_EN)))
			return 0;
		udelay(1);
	}
	return -EBUSY;
}

static inline int rts_qspi_set_dummy(struct rts_qspi *rqspi, u32 cycle)
{
	u32 baud;
	u32 dummy;
	u32 reg;
	u32 internal_dummy;

	internal_dummy = 0;

	if (cycle == 0) {
		dummy = 0;
	} else {
		baud = rts_readl(rqspi, BAUDR);
		dummy = baud * cycle * 2 + internal_dummy;
	}

	if (dummy > USER_RD_DUMMY_LENGTH_MASK)
		return -EINVAL;

	reg = rts_readl(rqspi, USER_LENGTH);
	reg = (reg & ~USER_RD_DUMMY_LENGTH_MASK) | dummy;
	rts_writel(rqspi, USER_LENGTH, reg);

	return 0;
}

static int rts_qspi_read_xfer(struct spi_nor *nor,
		struct spi_nor_xfer_cfg *cfg, u8 *buf, size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret)
		goto FAIL;

	reg = rts_readl(rqspi, CTRLR0);
	reg &= ~((TMOD_MASK << TMOD_OFFSET) |
		(DDR_EN_MASK << DDR_EN_OFFSET) |
		(ADDR_CH_MASK << ADDR_CH_OFFSET) |
		(DATA_CH_MASK << DATA_CH_OFFSET) |
		(CMD_CH_MASK << CMD_CH_OFFSET));
	reg |= (((u32)(cfg->mode) << TMOD_OFFSET) |
		((u32)(cfg->ddr_en) << DDR_EN_OFFSET) |
		((u32)(cfg->addr_pins >> 1) << ADDR_CH_OFFSET) |
		((u32)(cfg->mode_pins >> 1) << DATA_CH_OFFSET) |
		((u32)(cfg->cmd_pins >> 1) << CMD_CH_OFFSET));

	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);
	if (cfg->addr_width) {
		u8 cmd[5], cmd_len;
		int cnt;

		cmd[0] = cfg->cmd;
		addr2cmd(cfg->addr_width, cfg->addr, cmd);
		cmd_len = cfg->addr_width + 1;

		for (cnt = 0; cnt < cmd_len; cnt++)
			rts_writeb(rqspi, DR, cmd[cnt]);

		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH)
			| (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (cfg->addr_width << USER_ADDR_LENGTH);
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);
	} else {
		rts_writeb(rqspi, DR, cfg->cmd);
		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH)
			| (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);
	}

	rts_writel(rqspi, TX_NDF, 0);
	rts_writel(rqspi, RX_NDF, len);
	rts_writel(rqspi, SSIENR, 1);

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "controller busy\n");
		goto FAIL;
	}

	/* Optimize for 4 Byte FIFO read */
	for (cnt = 0; cnt < len / 4; cnt++) {
		u32 *buf32 = (u32 *)buf;

		buf32[cnt] = rts_readl(rqspi, DR);
	}
	for (cnt = len - len % 4; cnt < len; cnt++)
		buf[cnt] = rts_readb(rqspi, DR);

	return 0;
FAIL:
	dev_err(nor->dev, "%s() failed, ret = %d\n", __func__, ret);
	return ret;
}

static int rts_qspi_write_xfer(struct spi_nor *nor,
		struct spi_nor_xfer_cfg *cfg, const u8 *buf, size_t len)
{
	struct rts_qspi *rqspi = nor->priv;
	int ret, cnt;
	u32 reg;

	ret = rts_qspi_set_dummy(rqspi, cfg->dummy_cycles);
	if (ret)
		goto FAIL;

	reg = rts_readl(rqspi, CTRLR0);
	reg &= ~((TMOD_MASK << TMOD_OFFSET) |
		(ADDR_CH_MASK << ADDR_CH_OFFSET) |
		(DATA_CH_MASK << DATA_CH_OFFSET) |
		(CMD_CH_MASK << CMD_CH_OFFSET));
	reg |= (((u32)(cfg->mode) << TMOD_OFFSET) |
		((u32)(cfg->addr_pins >> 1) << ADDR_CH_OFFSET) |
		((u32)(cfg->mode_pins >> 1) << DATA_CH_OFFSET) |
		((u32)(cfg->cmd_pins >> 1) << CMD_CH_OFFSET));
	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);

	/* when transmited bytes are not greater than 4, use ADDR_LENGTH
	 * to indicate non-cmd bytes. When len equals zero, we don't
	 * push data into FIFO, just ignore it.
	 */
	if (cfg->addr_width == 0) {
		/* nor->write_reg */
		rts_writeb(rqspi, DR, cfg->cmd);
		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH)
			| (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (1 << USER_CMD_LENGTH);

		rts_writel(rqspi, USER_LENGTH, reg);

		for (cnt = 0; cnt < len; cnt++)
			rts_writeb(rqspi, DR, buf[cnt]);
	} else {
		/* nor->write */
		u8 cmd[5];
		u8 cmd_len;

		cmd[0] = cfg->cmd;
		addr2cmd(cfg->addr_width, cfg->addr, cmd);
		cmd_len = cfg->addr_width + 1;

		for (cnt = 0; cnt < cmd_len; cnt++)
			rts_writeb(rqspi, DR, cmd[cnt]);

		reg = rts_readl(rqspi, USER_LENGTH);
		reg &= ~((USER_CMD_LENGTH_MASK << USER_CMD_LENGTH)
			| (USER_ADDR_LENGTH_MASK << USER_ADDR_LENGTH));
		reg |= (cfg->addr_width << USER_ADDR_LENGTH);
		reg |= (1 << USER_CMD_LENGTH);
		rts_writel(rqspi, USER_LENGTH, reg);

		/* Optimize for 4 Byte FIFO write */
		for (cnt = 0; cnt < len / 4; cnt++) {
			u32 *buf32 = (u32 *)buf;

			rts_writel(rqspi, DR, buf32[cnt]);
		}

		for (cnt = len - len % 4; cnt < len; cnt++)
			rts_writeb(rqspi, DR, buf[cnt]);
	}
	rts_writel(rqspi, RX_NDF, 0);
	rts_writel(rqspi, TX_NDF, len);
	rts_writel(rqspi, SSIENR, 1);

	ret = rts_qspi_controller_ready(rqspi);
	if (ret) {
		dev_err(nor->dev, "controller busy\n");
		goto FAIL;
	}

	return 0;

FAIL:
	dev_err(nor->dev, "%s() failed, errno = %d\n", __func__, ret);
	return ret;
}

static int rts_qspi_read_reg(struct spi_nor *nor,
			u8 opcode, u8 *buf, size_t len)
{
	struct spi_nor_xfer_cfg cfg = {0};

	cfg.wren = 0;
	cfg.cmd = opcode;
	cfg.cmd_pins = 1;
	cfg.addr = 0;
	cfg.addr_width = 0;
	cfg.addr_pins = 1;
	cfg.mode = RECEIVE_MODE;
	cfg.mode_pins = 1;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;

	if (nor->read_proto == SNOR_PROTO_4_4_4) {
		cfg.cmd_pins = 4;
		cfg.addr_pins = 4;
		cfg.mode_pins = 4;
	}

	return rts_qspi_read_xfer(nor, &cfg, buf, len);
}

static int rts_qspi_write_reg(struct spi_nor *nor, u8 opcode,
			const u8 *buf, size_t len)
{
	struct spi_nor_xfer_cfg cfg = {0};

	cfg.wren = 0;
	cfg.cmd = opcode;
	cfg.cmd_pins = 1;
	cfg.addr = 0;
	cfg.addr_width = 0;
	cfg.addr_pins = 1;
	cfg.mode = TRANSMIT_MODE;
	cfg.mode_pins = 1;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;

	if (nor->read_proto == SNOR_PROTO_4_4_4) {
		cfg.cmd_pins = 4;
		cfg.addr_pins = 4;
		cfg.mode_pins = 4;
	}

	return rts_qspi_write_xfer(nor, &cfg, buf, len);
}

static int _rts_qspi_read(struct spi_nor *nor, loff_t from,
		size_t len, size_t *retlen, u_char *buf)
{
	struct spi_nor_xfer_cfg cfg = {0};
	struct rts_qspi *rqspi = nor->priv;
	int cmd_pins, addr_pins, mode_pins;
	int ret;
	u32 reg;

	/* set channel */
	switch (nor->read_proto) {
	case SNOR_PROTO_4_4_4:
		cmd_pins = 4; addr_pins = 4; mode_pins = 4;
		break;
	case SNOR_PROTO_1_4_4:
	case SNOR_PROTO_1_4_4_DTR:
		cmd_pins = 1; addr_pins = 4; mode_pins = 4;
		break;
	case SNOR_PROTO_1_1_4:
		cmd_pins = 1; addr_pins = 1; mode_pins = 4;
		break;
	case SNOR_PROTO_1_1_2:
		cmd_pins = 1; addr_pins = 1; mode_pins = 2;
		break;
	case SNOR_PROTO_1_1_1:
		cmd_pins = 1; addr_pins = 1; mode_pins = 1;
		break;
	default:
		dev_err(nor->dev, "Invalid read proto!\n");
		return -EINVAL;
	}

	cfg.wren = 0;
	cfg.cmd = nor->read_opcode;
	cfg.cmd_pins = cmd_pins;
	cfg.addr = from;
	cfg.addr_width = nor->addr_nbytes;
	cfg.addr_pins = addr_pins;
	cfg.mode = RECEIVE_MODE;
	cfg.mode_pins = mode_pins;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = nor->read_dummy;

	if (nor->read_opcode == SPINOR_OP_READ_1_4_4_DTR ||
		nor->read_opcode == SPINOR_OP_READ_1_4_4_DTR_4B) {
		cfg.ddr_en = DDR_EN;
		reg = readl(rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);
		reg |= SPIC_DDR_EN;
		writel(reg, rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);

		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (6 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	} else {
		cfg.ddr_en = 0;
		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (2 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	}

	ret = rts_qspi_read_xfer(nor, &cfg, buf, len);

	if (nor->read_opcode == SPINOR_OP_READ_1_4_4_DTR ||
		nor->read_opcode == SPINOR_OP_READ_1_4_4_DTR_4B) {
		reg = rts_readl(rqspi, CTRLR0);
		reg &= ~(DDR_EN_MASK << DDR_EN_OFFSET);
		rts_writel(rqspi, CTRLR0, reg);

		reg = readl(rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);
		reg &= ~(SPIC_DDR_EN);
		writel(reg, rqspi->spic_cfg_regs + SPIC_NOR_DDR_CFG);

		reg = rts_readl(rqspi, AUTO_LENGTH);
		reg &= ~(AUTO_IN_PHYSICAL_CYC_MASK << AUTO_IN_PHYSICAL_CYC);
		reg |= (2 << AUTO_IN_PHYSICAL_CYC);
		rts_writel(rqspi, AUTO_LENGTH, reg);
	}

	if (ret)
		return ret;

	*retlen += len;

	return ret;
}

static ssize_t rts_qspi_read(struct spi_nor *nor, loff_t from,
		size_t len, u_char *buf)
{
	struct rts_qspi *rqspi = nor->priv;
	int fifo = rqspi->fifo_size;
	int err;
	ssize_t retlen = 0;

	while (len) {
		size_t unit = fifo, rlen = 0;

		if (len < unit)
			unit = len;

		err = _rts_qspi_read(nor, from, unit, &rlen, buf);
		if (err)
			return err;

		retlen += rlen;
		len -= unit;
		from += unit;
		buf += unit;
	}

	return retlen;
}

static int _rts_qspi_write(struct spi_nor *nor, loff_t to,
		size_t len, size_t *retlen, const u_char *buf)
{
	struct spi_nor_xfer_cfg cfg = {0};
	int ret;
	int cmd_pins, addr_pins, mode_pins;

	/* set channel */
	switch (nor->write_proto) {
	case SNOR_PROTO_1_1_1:
		cmd_pins = 1; addr_pins = 1; mode_pins = 1;
		break;
	case SNOR_PROTO_1_1_4:
		cmd_pins = 1; addr_pins = 1; mode_pins = 4;
		break;
	case SNOR_PROTO_1_4_4:
		cmd_pins = 1; addr_pins = 4; mode_pins = 4;
		break;
	case SNOR_PROTO_4_4_4:
		cmd_pins = 4; addr_pins = 4; mode_pins = 4;
		break;
	default:
		dev_err(nor->dev, "Invalid Page Program opcode\n");
		return -EINVAL;
	}

	cfg.wren = 0;
	cfg.cmd = nor->program_opcode;
	cfg.cmd_pins = cmd_pins;
	cfg.addr = to;
	cfg.addr_width = nor->addr_nbytes;
	cfg.addr_pins = addr_pins;
	cfg.mode = TRANSMIT_MODE;
	cfg.mode_pins = mode_pins;
	cfg.mode_cycles = 0;
	cfg.dummy_cycles = 0;

	ret = rts_qspi_write_xfer(nor, &cfg, (u_char *)buf, len);
	if (ret)
		return ret;

	*retlen += len;

	return ret;
}

/*
 * Set write enable latch with Write Enable command.
 * Returns negative if error occurred.
 */
static inline int write_enable(struct spi_nor *nor)
{
	return nor->controller_ops->write_reg(nor, SPINOR_OP_WREN, NULL, 0);
}

static u8 rts_qspi_get_sr_bp_mask(struct spi_nor *nor)
{
	u8 mask = SR_BP2 | SR_BP1 | SR_BP0;

	if (nor->flags & SNOR_F_HAS_SR_BP3_BIT6)
			return mask | SR_BP3_BIT6;

	if (nor->flags & SNOR_F_HAS_4BIT_BP)
			return mask | SR_BP3;

	return mask;
}

static int common_swp_enable(struct spi_nor *nor)
{
	int ret;
	u8 val, mask;

	spi_nor_read_sr(nor, &val);
	mask = rts_qspi_get_sr_bp_mask(nor);
	val |= mask;

	write_enable(nor);
	ret = spi_nor_write_sr(nor, &val, 1);
	if (ret < 0) {
		pr_err("error %d writing SR\n", ret);
		return ret;
	}

	ret = spi_nor_wait_till_ready(nor);
	if (ret)
		return ret;

	return 0;
}

static int common_swp_disable(struct spi_nor *nor)
{
	int ret;
	u8 val, mask;

	spi_nor_read_sr(nor, &val);
	mask = rts_qspi_get_sr_bp_mask(nor);
	val = val & ~(mask);

	write_enable(nor);
	ret = spi_nor_write_sr(nor, &val, 1);
	if (ret < 0) {
		pr_err("error %d writing SR\n", (int)ret);
		return ret;
	}

	ret = spi_nor_wait_till_ready(nor);
	if (ret)
		return ret;

	return 0;
}

static int spi_swp_enable(struct spi_nor *nor)
{
	int status = 0;

	status = common_swp_enable(nor);
	if (status) {
		dev_err(nor->dev, "wp not enabled.\n");
		status = -EINVAL;
	}

	return status;
}

static int spi_swp_disable(struct spi_nor *nor)
{
	int status = 0;

	status = common_swp_disable(nor);
	if (status) {
		dev_err(nor->dev, "wp not disabled.\n");
		status = -EINVAL;
	}

	return status;
}

int enable_swp_thread(void *data)
{
	struct spi_nor *nor = (struct spi_nor *)data;

	while (1) {
		mutex_lock(&nor->lock);

		if (!spi_nor_swp.swp_flag &&
			time_after_eq(jiffies, spi_nor_swp.time + HZ)) {
				if (!spi_swp_enable(nor))
					spi_nor_swp.swp_flag = true;
				else
					dev_err(nor->dev, "enable swp failed\n");
		}
		mutex_unlock(&nor->lock);
		msleep(1000);
	}
}

static void spi_nor_init_swp(struct spi_nor *nor)
{
	spi_nor_swp.time = jiffies;

	if (spi_swp_enable(nor))
		spi_nor_swp.swp_flag = false;
	else
		spi_nor_swp.swp_flag = true;

	spi_nor_swp.enable_swp_task = kthread_create(
		enable_swp_thread, (void *)nor, "enable_swp_task");
	if (IS_ERR(spi_nor_swp.enable_swp_task)) {
		dev_err(nor->dev, "Unable to start kernel thread.\n");
		spi_nor_swp.enable_swp_task = NULL;
	} else {
		wake_up_process(spi_nor_swp.enable_swp_task);
	}
}

static ssize_t rts_qspi_write(struct spi_nor *nor, loff_t to,
		size_t len, const u_char *buf)
{
	struct rts_qspi *rqspi = nor->priv;
	int fifo = rqspi->fifo_size;
	int ret;
	ssize_t retlen = 0;

	struct completion done_data;
	long timeleft;
	int timeout = rqspi->timeout_ms;

	if (spi_nor_swp.swp_flag) {
		ret = spi_swp_disable(nor);
		if (!ret) {
			spi_nor_swp.swp_flag = false;
		} else {
			dev_err(nor->dev, "disable swp failed\n");
			return ret;
		}
		spi_nor_swp.time = jiffies;
	}

	while (len) {
		size_t unit = fifo, rlen = 0;

		if (len < unit)
			unit = len;

		spin_lock_irqsave(&rqspi->__lock, rqspi->__lock_flags);
		rqspi->done = &done_data;
		init_completion(&done_data);

		write_enable(nor);

		ret = _rts_qspi_write(nor, to, unit, &rlen, buf);
		if (ret)
			goto FAIL;

		spin_unlock_irqrestore(&rqspi->__lock, rqspi->__lock_flags);
		rts_writel(rqspi, TX_NDF, 0);
		rts_writel(rqspi, USER_LENGTH, 0);
		rts_writel(rqspi, SSIENR, 3);
		timeleft = wait_for_completion_timeout(
			rqspi->done, msecs_to_jiffies(timeout));
		if (timeleft <= 0) {
			ret = -ETIMEDOUT;
			goto FAIL;
		}

		retlen += rlen;
		len -= unit;
		to += unit;
		buf += unit;
	}

	return retlen;
FAIL:
	dev_err(nor->dev, "%s() failed, ret = %d\n", __func__, ret);
	return ret;
}

static int rts_qspi_erase(struct spi_nor *nor, loff_t offs)
{
	int ret;
	u8 cmd_buf[13];

	if (spi_nor_swp.swp_flag) {
		ret = spi_swp_disable(nor);
		if (!ret) {
			spi_nor_swp.swp_flag = false;
		} else {
			dev_err(nor->dev, "disable swp failed\n");
			return ret;
		}
		spi_nor_swp.time = jiffies;
	}

	ret = write_enable(nor);
	if (ret)
		return ret;

	cmd_buf[0] = offs >> (nor->addr_nbytes * 8 - 8);
	cmd_buf[1] = offs >> (nor->addr_nbytes * 8 - 16);
	cmd_buf[2] = offs >> (nor->addr_nbytes * 8 - 24);
	cmd_buf[3] = offs >> (nor->addr_nbytes * 8 - 32);

	ret = nor->controller_ops->write_reg(nor, nor->erase_opcode,
			(const u8 *)cmd_buf, nor->addr_nbytes);
	if (ret)
		return ret;

	return 0;
}

static int rts_qspi_setup(struct rts_qspi *rqspi)
{
	struct spi_board_info	*bi = rqspi->bi;
	struct platform_device	*pdev = rqspi->pdev;
	u32			reg, baudr;
	u32			speed_hz;

	/* User can't program some control register if SSIENR
	 * is enabled. So disable it before init registers
	 */
	rts_writel(rqspi, SSIENR, 0);	/* Disable SPIC */

	reg = 0;
	/* spi mode */
	if (bi->mode & SPI_CPOL)
		reg |= BIT(SCPOL);
	if (bi->mode && SPI_CPHA)
		reg |= BIT(SCPH);

	reg |= (0x1f << 23);

	reg |= BIT(USER_MODE);
	rts_writel(rqspi, CTRLR0, reg);

	/* Set clock ratio
	 * F(spi_sclk) = F(bus) / (2 * baudr)
	 */
	speed_hz = bi->max_speed_hz;
	if ((speed_hz == 0) || (speed_hz > rqspi->max_speed_hz)) {
		speed_hz = rqspi->max_speed_hz;
		dev_warn(&pdev->dev, "request %d Hz, force to set %d Hz\n",
			bi->max_speed_hz, rqspi->max_speed_hz);
	}

	if (speed_hz < rqspi->min_speed_hz) {
		dev_err(&pdev->dev, "requested speed too low %d Hz\n",
			bi->max_speed_hz);
		return -EINVAL;
	}

	if (rqspi->devtype & TYPE_FPGA)
		baudr = 8;
	else
		baudr = DIV_ROUND_UP(rqspi->spiclk_hz, speed_hz) / 2;
	if (baudr > SCKDV_MASK) {
		dev_err(&pdev->dev, "invalid baud reg: %08X\n", baudr);
		return -EINVAL;
	}
	rts_writel(rqspi, BAUDR, baudr);

	/* pin route & FIFO depth 2^fifo_entry Byte */
	reg = rts_readl(rqspi, CTRLR2);
	reg &= ~(BIT(SO_DNUM) | BIT(WPN_DNUM) | FIFO_ENTRY_MASK);
	reg |= BIT(SO_DNUM) | ((rqspi->fifo_entry) << FIFO_ENTRY);

	WARN_ON(rqspi->fifo_entry == 0);

	rts_writel(rqspi, CTRLR2, reg);

	rts_writel(rqspi, IMR, 0x900);	/* enable ACSIM interrupt */
	rqspi->timeout_ms = 5000;

	rts_writel(rqspi, SER, 1);	/* cs actived */

	rts_writel(rqspi, WRITE_SINGLE, 0xBB);

	return 0;
}

static void rts_qspi_init_reset_rom(struct rts_qspi *rqspi)
{
	struct spi_nor *nor = &rqspi->nor;
	u16 ctrl_reg[3];
	u16 cmd_ch;

	rts_writel(rqspi, FLUSH_FIFO, 0xffffffff);
	switch (nor->id[0]) {
	case SNOR_MFR_MACRONIX:
	case SNOR_MFR_GIGADEVICE:
	case SNOR_MFR_WINBOND:
	case SNOR_MFR_XMC:
	case SNOR_MFR_EON:
	case SNOR_MFR_BOYAMICRO:
	case SNOR_MFR_XTX:
	case SNOR_MFR_FM:
	case SNOR_MFR_PUYA:
	case SNOR_MFR_ZBIT:
		if (nor->read_proto == SNOR_PROTO_4_4_4)
			cmd_ch = QPI_CH;
		else
			cmd_ch = SPI_CH;

		ctrl_reg[0] = (COUNT_2EXP16 << PGMRST_COUNT) |
				(STATE_NEXT << PGMRST_STATE) |
				(cmd_ch << PGMRST_CMD_CH) |
				(SPINOR_OP_SRSTEN);
		ctrl_reg[1] = (COUNT_2EXP23 << PGMRST_COUNT) |
				(STATE_NEXT << PGMRST_STATE) |
				(cmd_ch << PGMRST_CMD_CH) |
				(SPINOR_OP_SRST);
		ctrl_reg[2] = (COUNT_2EXP16 << PGMRST_COUNT) |
				(STATE_END << PGMRST_STATE) |
				(SPI_CH << PGMRST_CMD_CH) |
				(0x00);

		writel((ctrl_reg[1] << 16) | ctrl_reg[0],
		       rqspi->spic_cfg_regs + SPIC_PGM_FIFO_INIT0);
		writel(ctrl_reg[2], rqspi->spic_cfg_regs + SPIC_PGM_FIFO_INIT1);
		break;
	default:
		break;
	}

}

static void rts_qspi_reset(struct rts_qspi *rqspi)
{
	struct spi_nor *nor = &rqspi->nor;

	nor->controller_ops->write_reg(nor, SPINOR_OP_SRSTEN, NULL, 0);
	nor->controller_ops->write_reg(nor, SPINOR_OP_SRST, NULL, 0);
}

static int rts_channel_map(char *str, const char * const *map)
{
	char lower[10] = {0};
	char *p;
	int index;

	strncpy(lower, str, sizeof(lower) - 1);

	for (p = lower; *p; p++)
		*p = tolower(*p);

	index = 0;
	for (; *map; map++) {
		if (!strcmp(lower, *map))
			return index;
		index++;
	}

	return -EINVAL;
}

static const struct spi_nor_controller_ops rts_qspi_controller_ops = {
	.read_reg  = rts_qspi_read_reg,
	.write_reg = rts_qspi_write_reg,
	.read  = rts_qspi_read,
	.write = rts_qspi_write,
	.erase = rts_qspi_erase,
};

static int rts_qspi_nor_register(struct rts_qspi *rqspi,
				struct device_node *np, enum read_mode mode)
{
	int ret = 0;
	struct mtd_info *mtd;
	struct spi_nor_hwcaps hwcaps = {
		.mask = SNOR_HWCAPS_READ | SNOR_HWCAPS_PP,
	};

	switch (mode) {
	case SPI_NOR_QIO:
		hwcaps.mask |= SNOR_HWCAPS_READ_1_4_4 |
				SNOR_HWCAPS_READ_1_4_4;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_4_4;
		break;
	case SPI_NOR_DTR:
		hwcaps.mask |= SNOR_HWCAPS_READ_1_4_4_DTR;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_4_4;
		break;
	case SPI_NOR_QUAD:
		hwcaps.mask |= SNOR_HWCAPS_READ_1_1_4;
		hwcaps.mask |= SNOR_HWCAPS_PP_1_1_4 |
				SNOR_HWCAPS_PP_1_4_4;
		break;
	case SPI_NOR_DUAL:
		hwcaps.mask |= SNOR_HWCAPS_READ_1_1_2;
		break;
	case SPI_NOR_FAST:
		hwcaps.mask |= SNOR_HWCAPS_READ_FAST;
		break;

	case SPI_NOR_NORMAL:
		break;
	default:
		dev_err(&(rqspi->pdev->dev), "Unsupported Read opcode!\n");
		return -EINVAL;
	}

	rqspi->nor.dev = &(rqspi->pdev->dev);
	spi_nor_set_flash_node(&rqspi->nor, np);
	rqspi->nor.priv = rqspi;
	rqspi->nor.controller_ops = &rts_qspi_controller_ops;

	ret = spi_nor_scan(&rqspi->nor, NULL, &hwcaps);
	if (ret) {
		dev_err(&(rqspi->pdev->dev), "device scan failed\n");
		return ret;
	}

	spi_nor_debugfs_register(&rqspi->nor);

	mtd = &rqspi->nor.mtd;
	mtd->name = np->name;

	ret = mtd_device_register(mtd, NULL, 0);
	if (ret) {
		dev_err(&(rqspi->pdev->dev), "mtd device parse failed\n");
		return ret;
	}

	spi_nor_init_swp(&rqspi->nor);

	return 0;
}

static irqreturn_t rtsx_qspi_isr(int irq, void *dev_id)
{
	struct rts_qspi *rqspi = dev_id;
	u32 int_reg;

	if (!rqspi)
		return IRQ_NONE;

	spin_lock(&(rqspi->__lock));

	int_reg = rts_readl(rqspi, ISR);
	if (!int_reg) {
		spin_unlock(&(rqspi->__lock));
		return IRQ_NONE;
	}
	rts_writel(rqspi, ICR, 0); /* clear ICR */

	dev_dbg(&(rqspi->pdev->dev), "----- IRQ: 0x%08x -----\n", int_reg);

	if (int_reg & 0x900) {
		if (int_reg & 0x100)
			rqspi->status = CHECK_TIMEOUT;
		else
			rqspi->status = 0;
		if (rqspi->done)
			complete(rqspi->done);
	}

	spin_unlock(&(rqspi->__lock));

	return IRQ_HANDLED;
}

static int rtsx_qspi_acquire_irq(struct rts_qspi *rqspi)
{
	int err = 0;

	spin_lock_init(&rqspi->__lock);
	err = request_irq(rqspi->irq, rtsx_qspi_isr, IRQF_SHARED,
			RTSX_QSPI_DRV_NAME, rqspi);
	if (err)
		dev_err(&(rqspi->pdev->dev), "request IRQ %d failed\n",
				rqspi->irq);

	return err;
}

static ssize_t flash_id_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct rts_qspi *rqspi = (struct rts_qspi *)(dev->driver_data);
	int ret = 0;
	u8 id[3];

	ret = rts_qspi_read_reg(&rqspi->nor, SPINOR_OP_RDID, id, 3);
	if (ret < 0) {
		pr_err("read flash id error\n");
		return 0;
	}

	return sprintf(buf, "%x%x%x\n", id[0], id[1], id[2]);
}

static DEVICE_ATTR(flash_id, 0444, flash_id_show, NULL);

static const struct of_device_id rts_qspi_dt_ids[] = {
	{ .compatible = "realtek,rts3917-quadspi",
		.data = (void *)&rts3917_data,},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_qspi_dt_ids);

static struct flash_platform_data rts_spiflash_data = {
	.name		= "m25p80",
	.type		= "mx25l12835f",
};

static struct spi_board_info rts_spi_board_info[] = {
	{
		.modalias		= "m25p80",
		.platform_data		= &rts_spiflash_data,
		.mode			= SPI_MODE_0,
		.max_speed_hz		= 60000000,
		.bus_num		= 0,
		.chip_select		= 0,
		.controller_data	= (void *)SPI_NOR_DUAL,
	},
};

static int rts_qspi_probe(struct platform_device *pdev)
{
	struct resource				*res, *spic_cfg_res;
	struct rts_qspi				*rqspi;
	struct clk				*clk;
	struct spi_nor				*nor;
	struct spi_board_info			*board_info;
	struct mtd_info				*mtd;
	struct rts_qspi_devtype_data		*qspi_devtype;
	const struct flash_platform_data	*data;
	int					irq, ret;
	u32					spiclk_hz;
	void __iomem				*regs, *spic_cfg_regs;
	enum read_mode				mode;
	const struct of_device_id *of_id;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *flash_np;
	const char		*spi_transfer_mode;
	char			spi_transfer_channel;

	of_id = of_match_device(rts_qspi_dt_ids, &pdev->dev);

	qspi_devtype = (void *)of_id->data;

	/* fpga board */
	if (of_machine_is_compatible("realtek,rts_fpga"))
		qspi_devtype->devtype |= TYPE_FPGA;

	rqspi = devm_kzalloc(&pdev->dev, sizeof(*rqspi), GFP_KERNEL);
	if (!rqspi)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	spic_cfg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!res || !spic_cfg_res) {
		dev_err(&pdev->dev, "get resource failed!\n");
		return -EINVAL;
	}

	res = devm_request_mem_region(&pdev->dev,
		res->start, resource_size(res), pdev->name);
	spic_cfg_res = devm_request_mem_region(&pdev->dev,
		spic_cfg_res->start, resource_size(spic_cfg_res), pdev->name);
	if (!res || !spic_cfg_res) {
		dev_err(&pdev->dev, "request memory region failed!\n");
		return -ENOMEM;
	}

	regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	spic_cfg_regs = devm_ioremap(&pdev->dev, spic_cfg_res->start,
			resource_size(spic_cfg_res));
	if (!regs || !spic_cfg_regs) {
		dev_err(&pdev->dev, "ioremap failed, phy address:%x, %x\n",
				res->start, spic_cfg_res->start);
		return -ENOMEM;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "get irq failed!\n");
		return -ENXIO;
	}

	clk = devm_clk_get(&pdev->dev, "spi_ck");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "get clock failed!\n");
		return PTR_ERR(clk);
	}

	spiclk_hz = clk_get_rate(clk);
	if (!spiclk_hz)  {
		dev_err(&pdev->dev,
			"get invalid clock rate: %d\n", spiclk_hz);
		return -EINVAL;
	}

	board_info = rts_spi_board_info;
	data = board_info->platform_data;
	if (!(data && data->type)) {
		dev_err(&pdev->dev, "get invalid flash data info\n");
		return -EINVAL;
	}

	ret = of_property_read_string(np, "spi-transfer-channel",
			&spi_transfer_mode);
	if (ret) {
		dev_err(&pdev->dev, "There's no spi-transfer-channel propert.\n");
		return -EINVAL;
	}

	if (!strcmp(spi_transfer_mode, "normal"))
		spi_transfer_channel = SPI_NOR_NORMAL;
	else if (!strcmp(spi_transfer_mode, "fast"))
		spi_transfer_channel = SPI_NOR_FAST;
	else if (!strcmp(spi_transfer_mode, "dual"))
		spi_transfer_channel = SPI_NOR_DUAL;
	else if (!strcmp(spi_transfer_mode, "quad"))
		spi_transfer_channel = SPI_NOR_QUAD;
	else if (!strcmp(spi_transfer_mode, "dtr"))
		spi_transfer_channel = SPI_NOR_DTR;
	else if (!strcmp(spi_transfer_mode, "qio"))
		spi_transfer_channel = SPI_NOR_QIO;
	else {
		dev_err(&pdev->dev, "The %s mode is not supported.\n",
				spi_transfer_mode);
		return -EINVAL;
	}

	rqspi->bi = board_info;

	mode = spi_transfer_channel;
	if (mode <= 0 || mode > SPI_NOR_QIO) {
		dev_err(&pdev->dev, "get invalid spi channel info\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "spi-max-frequency",
			&(rqspi->bi->max_speed_hz));
	if (ret) {
		dev_err(&pdev->dev, "There's no spi-max-frequency propert.\n");
		return -EINVAL;
	}

	if (channels) {
		int channel_mode;
		static const char * const channel_str[] = {
			[SPI_NOR_NORMAL] = "normal",
			[SPI_NOR_FAST] = "fast",
			[SPI_NOR_DUAL] = "dual",
			[SPI_NOR_QUAD] = "quad",
			[SPI_NOR_DTR]  = "dtr",
			[SPI_NOR_QIO]  = "qio",
			NULL,
		};

		channel_mode = rts_channel_map(channels, channel_str);
		if (channel_mode <= 0) {
			dev_info(&pdev->dev,
				"invalid channel parameter: %s\n", channels);
			return -EINVAL;
		} else if (channel_mode != mode) {
			dev_info(&pdev->dev,
				"force to set channels from %s mode to %s mode\n",
				channel_str[mode], channel_str[channel_mode]);
			mode = channel_mode;
		}
	}

	/* work around for spi_nor_scan check */
	pdev->dev.platform_data = (struct flash_platform_data *)data;

	platform_set_drvdata(pdev, rqspi);
	rqspi->irq = irq;
	rqspi->clk = clk;
	rqspi->regs = regs;
	rqspi->spic_cfg_regs = spic_cfg_regs;
	rqspi->phybase = res->start;
	rqspi->use_dma = false;
	rqspi->fifo_size = qspi_devtype->fifo_size;
	rqspi->fifo_entry = qspi_devtype->fifo_entry;
	rqspi->spiclk_hz = spiclk_hz;
	rqspi->max_speed_hz = DIV_ROUND_UP(spiclk_hz, 1) / 2;
	rqspi->min_speed_hz =
		DIV_ROUND_UP(spiclk_hz, ((1 << SCKDV_WIDTH) - 2)) / 2;
	rqspi->pdev = pdev;
	rqspi->devtype = qspi_devtype->devtype;

	nor = &rqspi->nor;
	nor->dev = &pdev->dev;
	nor->priv = rqspi;
	mtd = &nor->mtd;
	mtd->priv = nor;

	ret = rtsx_qspi_acquire_irq(rqspi);
	if (ret < 0)
		return ret;
	synchronize_irq(rqspi->irq);

	/* Initialize the hardware */
	ret = rts_qspi_setup(rqspi);
	if (ret)
		return ret;

	spi_nor_set_flash_node(nor, np->child);
	if (mode == SPI_NOR_QUAD || mode == SPI_NOR_QIO ||
		mode == SPI_NOR_DTR) {
		rqspi->pins.p = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(rqspi->pins.p)) {
			ret = PTR_ERR(rqspi->pins.p);
			return ret;
		}

		rqspi->pins.quad_state = pinctrl_lookup_state(rqspi->pins.p,
				"quad");
		if (IS_ERR(rqspi->pins.quad_state)) {
			dev_err(&pdev->dev, "get quad state fail\n");
			devm_pinctrl_put(rqspi->pins.p);
			ret = PTR_ERR(rqspi->pins.quad_state);
			return ret;
		}
		pinctrl_select_state(rqspi->pins.p, rqspi->pins.quad_state);
	}

	flash_np = of_get_next_available_child(np, NULL);
	if (!flash_np) {
		dev_err(&pdev->dev, "no SPI flash device to configure\n");
		return -ENODEV;
	}

	ret = rts_qspi_nor_register(rqspi, flash_np, mode);
	of_node_put(flash_np);
	if (ret) {
		dev_err(&pdev->dev, "unable to setup flash chip\n");
		return ret;
	}

	/* restore platform_data */
	pdev->dev.platform_data = board_info;

	rts_qspi_init_reset_rom(rqspi);

	device_create_file(nor->dev, &dev_attr_flash_id);

	dev_info(&pdev->dev, "Realtek QSPI Controller at 0x%08lx (irq %d)\n",
			(unsigned long)res->start, irq);

	return 0;
}

static int rts_qspi_remove(struct platform_device *pdev)
{
	struct rts_qspi *rqspi = platform_get_drvdata(pdev);

	rts_qspi_reset(rqspi);

	mtd_device_unregister(&rqspi->nor.mtd);

	return 0;
}

static const struct platform_device_id rts_qspi_id_table[] = {
	{ .name = "rts3917-qspi",
		.driver_data = (kernel_ulong_t)&rts3917_data, },
	{},
};
MODULE_DEVICE_TABLE(platform, rts_qspi_id_table);

static struct platform_driver rts_qspi_driver = {
	.driver = {
		.name	= "rts-quadspi",
		.owner	= THIS_MODULE,
		.of_match_table = rts_qspi_dt_ids,
	},
	.id_table	= rts_qspi_id_table,
	.probe		= rts_qspi_probe,
	.remove		= rts_qspi_remove,
};
module_platform_driver(rts_qspi_driver);

MODULE_ALIAS("platform:rts-quadspi");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("QuadSPI flash controller driver for realtek rts39xx ipcam soc");
MODULE_AUTHOR("Jim Cao <jim_cao@realsil.com.cn>, Darcy Lu <darcy_lu@realsil.com.cn>");
