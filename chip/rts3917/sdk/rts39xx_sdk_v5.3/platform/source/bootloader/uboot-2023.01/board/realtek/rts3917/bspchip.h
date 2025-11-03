/*
 * Realtek Semiconductor Corp.
 *
 * bsp.h
 * Board Support Package header file
 *
 * Tony Wu (tonywu@realtek.com.tw)
 * Dec. 07, 2007
 */

#ifndef  _BSPCHIP_H_
#define  _BSPCHIP_H_

/*
 *****************************************************************************************
 * DEFINITIONS AND TYPES
 *****************************************************************************************
 */

/* Register Macro */
#ifndef REG64
#define REG64(reg)      (*(volatile u64 *)(reg))
#endif
#ifndef REG32
#define REG32(reg)      (*(volatile u32 *)(reg))
#endif
#ifndef REG16
#define REG16(reg)      (*(volatile u16 *)(reg))
#endif
#ifndef REG8
#define REG8(reg)       (*(volatile u8  *)(reg))
#endif

/*
 *****************************************************************************************
 * Timer
 *****************************************************************************************
 */

/* DWC timer */
#define TIMER0_IOBASE		0x1FB01000	/* Timer0 register base address mapping */
#define TIMER0_FREQ		25000000	/* The frequency of timer module        */

/* Register offset from TIMERx_IOBASE */
#define TIMER_TLCR1		0x00
#define TIMER_TCV		0x04
#define TIMER_TCR		0x08
#define TIMER_EOIA		0x0c

/*
 ******************************
 * Uart
 ******************************
 */
/* Uart1 register base address mapping */
#define UART1_IOBASE		0x18810100
/* Serial Clock	*/
#define UART1_FREQ			24000000
/* Register offset from UARTx_IOBASE */
#define UART_RBR		0x00
#define UART_THR		0x00
#define UART_IER		0x04
#define UART_IIR		0x08
#define UART_LCR		0x0C
#define UART_LSR		0x14
#define UART_DLL		0x00
#define UART_DLH		0x04
#define UART_CTR		0xFC

/* uart LCR register bits */
#define UART_LCR_PEN		0x00000008	/* parity enable bit */
#define UART_LCR_EPS		0X00000010
#define UART_LCR_PARITY		(UART_LCR_PEN | UART_LCR_EPS)
#define UART_LCR_STOP		0x00000004	/* stop bit          */
#define UART_LCR_DLS		0x00000003	/* 8 bits mode       */
#define UART_LCR_DLAB		0x00000080	/* DLAB bit          */

#define UART_DL_BAUD_9600		(UART_BAUD_BASE / 9600)
#define UART_DL_BAUD_19200		(UART_BAUD_BASE / 19200)
#define UART_DL_BAUD_38400		(UART_BAUD_BASE / 38400)
#define UART_DL_BAUD_57600		(UART_BAUD_BASE / 57600)
#define UART_DL_BAUD_115200		(UART_BAUD_BASE / 115200)

/* baudrate platform specific setting */
#define UART_BAUD_BASE			(UART1_FREQ >> 4)
#define UART_BAUD_DEFAULT		UART_DL_BAUD_57600
/*
 ******************************
 * GPIO
 ******************************
 */
#define	XB2_GPIO_BASE           0x18800000
#define XB2_GPIO_REG(x)         ((x) + XB2_GPIO_BASE)
#define UART1_PULLCTRL          XB2_GPIO_REG(0x48)
#define UART1_TX_PE_SHIFT	2
#define UART1_TX_PE_MASK	0xc
#define UART1_RX_PE_SHIFT	0
#define UART1_RX_PE_MASK	3
#define UART1_PE_PU             0x2
#define UART1_PE_PD             0x1

#define UART1_PAD_CFG  XB2_GPIO_REG(0x5c)
#define PAD_XB2_UTXD_SHIFT	8
#define PAD_XB2_UTXD_MASK	(0x3 << 8)
#define UART1_TX_EN	0x2
#define PAD_XB2_URXD_SHIFT	12
#define PAD_XB2_URXD_MASK	(0x3 << 12)
#define UART1_RX_EN	0x2

/*
 *****************************************************************************************
 * Time stamp generator
 *****************************************************************************************
 */

#define BSP_USE_CORESIGHT_TIMESTAMP	0

#define TSGEN_BASE      		0x02000000
#define TSGEN_CNTFID0   		(TSGEN_BASE + 0x20)
#define TSGEN_FREQ      		0x017d7840      /* 25 MHz */

#define TSGEN_CNTCR     		(TSGEN_BASE + 0x0)
#define TSGEN_ENABLE    		0x1

/*
 *****************************************************************************************
 * CCI Bus
 *****************************************************************************************
 */

#define BSP_USE_CCI_BUS			0

#define BSP_CCI_BASE			0xf00c0000
#define BSP_CCI_STATUS			(BSP_CCI_BASE + 0xc)

#define BSP_CCI_SLAVE0			(BSP_CCI_BASE + 0x4000)
#define BSP_CCI_SLAVE1			(BSP_CCI_BASE + 0x5000)

#define CCI_ENABLE_SNOOP_REQ		0x1
#define CCI_ENABLE_DVM_REQ		0x2
#define CCI_ENABLE_REQ			(CCI_ENABLE_SNOOP_REQ)

/*
 *******************************************************************************
 * PLL
 *******************************************************************************
 */
#define SYS_GPLL_BASE	0x188b0000
#define SYSGPLL_REG(x) ((x) + SYS_GPLL_BASE)
#define SYS_GPLL0_CTRL			SYSGPLL_REG(0x00)
#define SYS_GPLL0_CFG			SYSGPLL_REG(0x04)
#define SYS_GPLL0_SCCG_CFG0	SYSGPLL_REG(0x08)
#define SYS_GPLL0_SCCG_CFG1	SYSGPLL_REG(0x0c)
#define SYS_GPLL0_STATUS		SYSGPLL_REG(0x10)
#define SYS_GPLL0_SCCG_CFG2	SYSGPLL_REG(0x14)
#define SYS_GPLL0_SCCG_CFG3	SYSGPLL_REG(0x18)

#define SYS_GPLL1_CTRL			SYSGPLL_REG(0x100)
#define SYS_GPLL1_CFG			SYSGPLL_REG(0x104)
#define SYS_GPLL1_SCCG_CFG0	SYSGPLL_REG(0x108)
#define SYS_GPLL1_SCCG_CFG1	SYSGPLL_REG(0x10c)
#define SYS_GPLL1_STATUS		SYSGPLL_REG(0x110)
#define SYS_GPLL1_SCCG_CFG2	SYSGPLL_REG(0x114)
#define SYS_GPLL1_SCCG_CFG3	SYSGPLL_REG(0x118)

#define SYS_GPLL2_CTRL			SYSGPLL_REG(0x200)
#define SYS_GPLL2_CFG			SYSGPLL_REG(0x204)
#define SYS_GPLL2_SCCG_CFG0	SYSGPLL_REG(0x208)
#define SYS_GPLL2_SCCG_CFG1	SYSGPLL_REG(0x20c)
#define SYS_GPLL2_STATUS		SYSGPLL_REG(0x210)
#define SYS_GPLL2_SCCG_CFG2	SYSGPLL_REG(0x214)
#define SYS_GPLL2_SCCG_CFG3	SYSGPLL_REG(0x218)

#define SYS_PLL_DISABLE		0x00
#define SYS_PLL_ENABLE_STEP1	0x02
#define SYS_PLL_ENABLE_STEP2	0x03
#define SYS_PLL_CK_RDY		0x01

#define CMU_SSC_RSTB		0x01
#define CMU_SSC_EN		0x01

#ifdef CONFIG_TARGET_FPGA
#define CPU_FREQUCNCY		50000000 /* Hz*/
#endif

#ifdef CONFIG_TARGET_EMULATOR
#define CPU_FREQUCNCY		1300000000 /* Hz*/
#endif

/* clk_source: 25MHz */
#ifdef CONFIG_TARGET_ASIC
#ifdef CONFIG_CPU_800M
/* GPLL0: 800MHz*/
#define SYS_GPLL0_N_CODE_VALUE	30
#define CPU_FREQUCNCY		800000000 /* Hz*/
#endif

#ifdef CONFIG_CPU_1000M
/* GPLL0: 1000MHz*/
#define SYS_GPLL0_N_CODE_VALUE	38
#define CPU_FREQUCNCY		1000000000 /* Hz*/
#endif

#ifdef CONFIG_CPU_1200M
/* GPLL0: 1200MHz*/
#define SYS_GPLL0_N_CODE_VALUE	46
#define CPU_FREQUCNCY		1200000000 /* Hz*/
#endif

#ifdef CONFIG_CPU_1300M
/* GPLL0: 1300MHz*/
#define SYS_GPLL0_N_CODE_VALUE	50
#define CPU_FREQUCNCY		1300000000 /* Hz*/
#endif

/* GPLL1: 1080MHz*/
#define SYS_GPLL1_N_CODE_VALUE	41
#define SYS_GPLL1_F_CODE_VALUE	819

/* GPLL2: 1000MHz */
#define SYS_GPLL2_N_CODE_VALUE	38
#endif
/*
 *******************************************************************************
 * CLOCK
 *******************************************************************************
 */

#define SYS_CLK_BASE                    0x188d0000
#define SYSCLK_REG(x)                   ((x) + SYS_CLK_BASE)

#define CLK_CHANGE		SYSCLK_REG(0x00)
#define BUS_CLK_CHANGE			0x80
#define XB2_CLK_CHANGE			0x40
#define CPU_CLK_CHANGE			0x20
#define P2BUS_CLK_CHANGE		0x10
#define P1BUS_CLK_CHANGE		0x8
#define CLK_ENABLE 0x1000000

#define CLOCK_SELECT_DIV2	(0x1 << 2)
#define CLOCK_SELECT_DIV4	(0x2 << 2)
#define CLOCK_SELECT_DIV6	(0x3 << 2)
#define CLOCK_SELECT_DIV8	(0x4 << 2)
#define CLOCK_SELECT_DIV10	(0x5 << 2)
#define CLOCK_SELECT_DIV12	(0x6 << 2)
#define CLOCK_SELECT_DIV14	(0x7 << 2)

#define P1BUS_CLK_CFG_REG		SYSCLK_REG(0x08)
#define P1BUS_SELECT_USBPLL_2	0
#define P1BUS_SELECT_GPLL0_3	0x01
#define P1BUS_SELECT_GPLL1_3	0x02
#define P1BUS_SELECT_GPLL2_3	0x03

#define P2BUS_CLK_CFG_REG		SYSCLK_REG(0x0C)
#define P2BUS_SELECT_USBPLL_2	0
#define P2BUS_SELECT_GPLL0_3	0x01
#define P2BUS_SELECT_GPLL1_3	0x02
#define P2BUS_SELECT_GPLL2_3	0x03

#define CPU_CLK_CFG_REG		SYSCLK_REG(0x10)
#define CPU_SELECT_USBPLL_3	0
#define CPU_SELECT_GPLL0_1		0x01
#define CPU_SELECT_GPLL1_1		0x02
#define CPU_SELECT_GPLL2_1	0x03

#define XB2_CLK_CFG_REG		SYSCLK_REG(0x14)
#define XB2_SELECT_USBPLL_2	0
#define XB2_SELECT_GPLL0_2	0x01
#define XB2_SELECT_GPLL0_3	0x02

#define BUS_CLK_CFG_REG		SYSCLK_REG(0x18)
#define BUS_SELECT_USBPLL_2	0
#define BUS_SELECT_GPLL0_2	0x01
#define BUS_SELECT_GPLL1_3	0x02
#define BUS_SELECT_GPLL2_2	0x03

#define ETHERNET_CLK_CFG_REG		SYSCLK_REG(0xc4)

#define MAC_BYPASS_CLK_CFG_REG	SYSCLK_REG(0xcc)
#define MAC_BYPASS_CLK_EN			0x01000000
#define MIPI_BYPASS_CLK_GPLL0_DIV2	0

#define UART1_CLK_CFG_REG		SYSCLK_REG(0x104)
#define UART_SELECT_USBPLL_5    0x0
#define UART_SELECT_GPLL0_5    0x01
#define UART_SELECT_GPLL1_5    0x02
#define OTP_CLK_CFG_REG         SYSCLK_REG(0xc8)
#define DMA_CLK_CFG_REG		SYSCLK_REG(0xd8)

/* SPIC */
#define SPIC_CTRLR0			0x0000
#define SPIC_RX_NDF			0x0004
#define SPIC_SSIENR			0x0008
#define SPIC_DR				0x0060
#define SPIC_CTRLR2			0x0110
#define SPIC_USER_LENGTH	0x0118
#define SPIC_TX_NDF			0x0130

#define	SPIC_VA			0x18030000
#define SPIC_BAUDR		0x14
#define SPIC_READ_DUAL_DATA	0xe4
#define SPIC_AUTO_LENGTH	0x11c
#define SPIC_VALID_CMD		0x120

/* SNFC */
#define SNFC_VA			0x18038000
#define SNFC_CTRLR0		0x0000
#define SNFC_SSIENR		0x0004
#define SNFC_BAUDR		0x0008
#define SNFC_UCMD		0x0010
#define SNFC_UADDR		0x0014
#define SNFC_ADDR_LEN	0x0018
#define SNFC_DATA_LEN	0x001c
#define SNFC_READ_DUMMY_LEN	0x0024
#define SNFC_PROGRAM_DUMMY_LEN	0x0028
#define SNFC_GET_FEATURE_DUMMY_LEN	0x002c
#define SNFC_RANDOM_READ_CMD	0x0044
#define SNFC_AUTO_GET_FEATURES_CTRL	0x005c
#define SNFC_AUTO_READ_TYPE	0x0060
#define SNFC_SR				0x0068
#define SNFC_DA				0x0100
#define SNFC_DV				0x0104
#define SNFC_DATA_FIFO		0x1000
#define SNFC_MAX_COUNT		0x7fff

/*ethernet config*/
#define ETHERNET_CFG_REG	0x18862024
#define ETH_EPHY_ADDR	(1 << 9)
#define AFE_POW_STATE	(1 << 18)
#define ETH_EPHY_CLK_BYPASS	(1 << 27)

#define FEPHY_LDO_CTRL		0x1886202c

#define LDO_FEPHY_CFG		0x18862030

#define ETH_PDWN_CONTROL	0x188dd008

/* some register about reset */
#define SYS_RESET_BASE	0x188d8000
#define SYSRESET_REG(x) ((x) + SYS_RESET_BASE)

#define FORCE_REG_RST_FWC	SYSRESET_REG(0x4)
#define FORCE_FEPHY_RST	(1 << 5)

#define FORCE_REG_ASYNC_RST	SYSRESET_REG(0x8)
#define FORCE_ETHERNET_RST	(1 << 6)
#define FORCE_OTP_RST	        BIT(17)

#define WATCHDOG_CFG_REG SYSRESET_REG(0x2000)

#define WATCH_DOG_PAD_PULLUP		0x1
#define WATCH_DOG_TIMEOUT_1S		0x0
#define WATCH_DOG_TIMEOUT_2S		0x1
#define WATCH_DOG_TIMEOUT_4S		0x2
#define WATCH_DOG_TIMEOUT_8S		0x3
#define WATCH_DOG_TIMEOUT_16S		0x4
#define WATCH_DOG_TIMEOUT_32S		0x5
#define WATCH_DOG_TIMEOUT_64S		0x6

#define WATCH_DOG_RESET_SYSTEM		0x1
#define WATCH_DOG_ENABLE		0x1
#define WATCH_DOG_CFG_DATA		((WATCH_DOG_PAD_PULLUP << 9) \
					| (WATCH_DOG_TIMEOUT_1S << 16) \
					| (WATCH_DOG_RESET_SYSTEM << 1) \
					| (WATCH_DOG_ENABLE << 0))

#define SYS_MEM_SD		0x188c0228
#define ONCHIP_MEM_SD	BIT(16)
#define CPU_MEM_SD	BIT(13)
#define SDIO2_MEM_SD	BIT(7)
#define SDIO_MEM_SD	BIT(6)
#define ETH_MEM_SD	BIT(3)
#define CIPHER_MEM_SD	BIT(2)

/*ethernet phy config*/
#define ETHERNET_LDPHY_CFG 0x18862034
#define LDPHY_SEL			0x80000000
#define RMII_REF_CLK_EN		0x08000000
#define SPD					0x01000000

#define ETHERNET_LDPHY_CFG2	0x18862038
#define RMII_RX_CLK_INV		0x80000000
#define TX_PHASE_SEL		0x00030000
#define INT_GPIO7			0x3

/*
 *******************************************************************************
 * OTP
 *******************************************************************************
 */
#define MASK(s, n)	(((1UL << (n)) - 1) << (s))
#define SYS_OTP_BASE			0x188a0300
#define SYS_OTP_REG(x)			((x) + SYS_OTP_BASE)
#define SF_CTRL_0			SYS_OTP_REG(0x00)
#define RG_PENVDD2_VDD2_SW		BIT(0)
#define SF_MODE_CTRL			SYS_OTP_REG(0x04)
#define RG_PTM_SW_OFFSET		0
#define RG_PTM_SW_BITS			3
#define RG_PTM_SW_MASK			(((1 << 3) - 1) << 0)
#define ADDR_CTRL			SYS_OTP_REG(0x10)
#define BURST_CTRL			SYS_OTP_REG(0x14)
#define OTP_PROGRAM_DATA		SYS_OTP_REG(0x18)
#define HW_START_REG			SYS_OTP_REG(0x30)
#define ECC_CTRL_AUTO			SYS_OTP_REG(0x34)
#define ECC_CTRL1			SYS_OTP_REG(0x3c)
#define ECC_CTRL2			SYS_OTP_REG(0x40)
#define OTP_READ_DATA_0			SYS_OTP_REG(0x48)
#define OTP_CONTROL_DATA_0		SYS_OTP_REG(0x70)
#define REG_AES_KEY_SEL			SYS_OTP_REG(0x88)
#define KEY_SEL(i)			((i) * 8)
#define KEY_SEL_MASK(i)			MASK((i) * 8, 3)
#define KEY(i, k)			(((i) % 2) + (k) * 2)
#define REG_AES_KEY_GROUP		SYS_OTP_REG(0x90)
#define REG_LOAD_KEY			SYS_OTP_REG(0x94)

/*
 *******************************************************************************
 * CPU BISR registers
 *******************************************************************************
 */
#define SYS_MEM_POWER_CTRL_BASE				0x188c0000
#define SYS_MEM_POWER_CTRL_REG(x)			((x) + SYS_MEM_POWER_CTRL_BASE)
#define CPU_PWR_RSTN			SYS_MEM_POWER_CTRL_REG(0x164)
#define CPU_BISR_RSTN			SYS_MEM_POWER_CTRL_REG(0x168)
#define CPU_BISR_CTRL			SYS_MEM_POWER_CTRL_REG(0x16c)
#define CPU_BISR_DONE			SYS_MEM_POWER_CTRL_REG(0x170)
#define CPU_BISR_FAIL			SYS_MEM_POWER_CTRL_REG(0x174)
#define CPU_MEM_SPEED_MODE			SYS_MEM_POWER_CTRL_REG(0x1b0)
#define CPU_COORD_RSTN			SYS_MEM_POWER_CTRL_REG(0x1b8)

#define CA7_PWR_RSTN_DISABLE		0x0
#define CA7_PWR_RSTN_ENABLE			0x1
#define CA7_BISR_RSTN_DISABLE		0x0
#define CA7_BISR_RSTN_ENABLE		0x1
#define CA7_SECOND_RUN_DISABLE		(0x0 << 1)
#define CA7_SECOND_RUN_ENABLE		(0x1 << 1)
#define CA7_HOLD_REMAP_DISABLE		0x0
#define CA7_HOLD_REMAP_ENABLE		0x1
#define CA7_BISR_DONE				0x1
#define CA7_BISR_REPAIRED			(0x1 << 8)
#define CA7_BISR_FAIL				0xff
#define CA7_COORD_RST_N_DISABLE		0x0
#define CA7_COORD_RST_N_ENABLE		0x1
#define CPU_MEM_SPEED_MODE1			0x1

#endif /*  _BSPCHIP_H_ */
