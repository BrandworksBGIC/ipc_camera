#ifndef _DDR_DEF_H_
#define _DDR_DEF_H_

/*************************************
 *		DDR controller register		*
 *************************************
 */
#define DDRC_BASE	0x18010000

#define PCTL_CCR				DDRC_BASE + 0x0
#define PCTL_DCR				DDRC_BASE + 0x4
#define PCTL_IOCR				DDRC_BASE + 0x8
#define PCTL_CSR				DDRC_BASE + 0xc
#define PCTL_DRR				DDRC_BASE + 0x10
#define PCTL_TPR0				DDRC_BASE + 0x14
#define PCTL_TPR1				DDRC_BASE + 0x18
#define PCTL_TPR2				DDRC_BASE + 0x1c
#define PCTL_TPR3				DDRC_BASE + 0x20
#define PCTL_TPR4				DDRC_BASE + 0x90
#define PCTL_TPR5				DDRC_BASE + 0x94
#define PCTL_CMD_DPIN				DDRC_BASE + 0x28
#define PCTL_TIE_DPIN				DDRC_BASE + 0x2c
#define PCTL_MR_INFO				DDRC_BASE + 0x30
#define PCTL_MR0				DDRC_BASE + 0x34
#define PCTL_MR1				DDRC_BASE + 0x38
#define PCTL_MR2				DDRC_BASE + 0x3c
#define PCTL_MR3				DDRC_BASE + 0x40
#define PCTL_MR4				DDRC_BASE + 0x44
#define PCTL_MR5				DDRC_BASE + 0x48
#define PCTL_MR6				DDRC_BASE + 0x4c
#define PCTL_IMR					DDRC_BASE + 0xcc
#define PCTL_SVN_ID				DDRC_BASE + 0xF4
#define PCTL_IDR				DDRC_BASE + 0xF8
#define PCTL_MISC				DDRC_BASE + 0x224
#define PCTL_CHOP				DDRC_BASE + 0x228
#define PCTL_DRAM_SIZE				DDRC_BASE + 0x230
#define PCTL_WRAP_IDR				DDRC_BASE + 0x2A0
#define PCTL_WRAP_VERSION			DDRC_BASE + 0x2A4
#define PCTL_ICG_EN				DDRC_BASE + 0x300

#define PCTL_ROW_REMAP0				DDRC_BASE + 0x0400
#define PCTL_ROW_REMAP1				DDRC_BASE + 0x0404
#define PCTL_ROW_REMAP2				DDRC_BASE + 0x0408
#define PCTL_BANK_REMAP0			DDRC_BASE + 0x040c
#define PCTL_COLU_REMAP0			DDRC_BASE + 0x0410
#define PCTL_COLU_REMAP1			DDRC_BASE + 0x0414
#define PCTL_RANK_REMAP0			DDRC_BASE + 0x0418
#define PCTL_CH_REMAP0				DDRC_BASE + 0x041c
#define PCTL_BG_REMAP0				DDRC_BASE + 0x0420
#define PCTL_ADDR_REMAP_CTRL			DDRC_BASE + 0x0424
#define PCTL_SLOT_PARK				DDRC_BASE + 0x0460
#define PCTL_SLOT_STARVE			DDRC_BASE + 0x0464
#define PCTL_SLOT_CNTR				DDRC_BASE + 0x0468
#define PCTL_EXT_CTRL				DDRC_BASE + 0x046c
#define PCTL_CBCTL0				DDRC_BASE + 0x0480
#define PCTL_CBCLT1				DDRC_BASE + 0x0484
#define PCTL_CBSTARVE0				DDRC_BASE + 0x0488
#define PCTL_CBSTARVE1				DDRC_BASE + 0x048c
#define PCTL_CBSTARVE2				DDRC_BASE + 0x0490
#define PCTL_CBSTARVE3				DDRC_BASE + 0x0494
#define PCTL_CBSTARVE4				DDRC_BASE + 0x0498
#define PCTL_BANK_SCH_CTRL			DDRC_BASE + 0x04a0

#define PCTL_CFG_P0_PORTCTRL			DDRC_BASE + 0x1818
#define PCTL_CFG_P1_PORTCTRL			DDRC_BASE + 0x1858
#define PCTL_CFG_P2_PORTCTRL			DDRC_BASE + 0x1898
#define PCTL_CFG_P3_PORTCTRL			DDRC_BASE + 0x18d8

#define PCTL_CCR_INIT_BFO		0
#define PCTL_CCR_INIT_BFW		1
#define PCTL_CCR_DTT_BFO		1
#define PCTL_CCR_DTT_BFW		1
#define PCTL_CCR_BTT_BFO		2
#define PCTL_CCR_BTT_BFW		1
#define PCTL_CCR_DPIT_BFO		3
#define PCTL_CCR_DPIT_BFW		1
#define PCTL_CCR_FLUSH_FIFO_BFO		8
#define PCTL_CCR_FLUSH_FIFO_BFW		1

#define PCTL_DCR_DDR3_BFO		0
#define PCTL_DCR_DDR3_BFW		3
#define PCTL_DCR_SDR_BFO		3
#define PCTL_DCR_SDR_BFW		1
#define PCTL_DCR_DQ32_BFO		4
#define PCTL_DCR_DQ32_BFW		1
#define PCTL_DCR_HALF_DQ_BFO		5
#define PCTL_DCR_HALF_DQ_BFW		1
#define PCTL_DCR_DFI_RATE_BFO		8
#define PCTL_DCR_DFI_RATE_BFW		3
#define PCTL_DCR_DBI_BFO		17
#define PCTL_DCR_DBI_BFW		1
#define PCTL_DCR_PAR_BFO		18
#define PCTL_DCR_PAR_BFW		1
#define PCTL_DCR_GEAR_DOWN_BFO		19
#define PCTL_DCR_GEAR_DOWN_BFW		1

#define PCTL_IOCR_RD_PIPE_BFO		8
#define PCTL_IOCR_RD_PIPE_BFW		4
#define PCTL_IOCR_TPHY_WD_BFO		12
#define PCTL_IOCR_TPHY_WD_BFW		5
#define PCTL_IOCR_TPHY_WL_BFO		17
#define PCTL_IOCR_TPHY_WL_BFW		3
#define PCTL_IOCR_TPHY_RD_EN_BFO	20
#define PCTL_IOCR_TPHY_RD_EN_BFW	5

#define PCTL_CSR_MEM_IDLE_BFO		8
#define PCTL_CSR_MEM_IDLE_BFW		1
#define PCTL_CSR_DT_IDLE_BFO		9
#define PCTL_CSR_DT_IDLE_BFW		1
#define PCTL_CSR_BIST_IDLE_BFO		10
#define PCTL_CSR_BIST_IDLE_BFW		1
#define PCTL_CSR_DT_FAIL_BFO		11
#define PCTL_CSR_DT_FAIL_BFW		1
#define PCTL_CSR_BT_FAIL_BFO		12
#define PCTL_CSR_BT_FAIL_BFW		1

#define PCTL_DRR_TRFC_BFO		0
#define PCTL_DRR_TRFC_BFW		7
#define PCTL_DRR_TREF_BFO		8
#define PCTL_DRR_TREF_BFW		24
#define PCTL_DRR_REF_NUM_BFO		24
#define PCTL_DRR_REF_NUM_BFW		4
#define PCTL_DRR_REF_DIS_BFO		28
#define PCTL_DRR_REF_DIS_BFW		1

#define PCTL_TPR0_TRP_BFO		0
#define PCTL_TPR0_TRP_BFW		4
#define PCTL_TPR0_TRAS_BFO		4
#define PCTL_TPR0_TRAS_BFW		5
#define PCTL_TPR0_TWR_BFO		9
#define PCTL_TPR0_TWR_BFW		4
#define PCTL_TPR0_TRTP_BFO		13
#define PCTL_TPR0_TRTP_BFW		3
#define PCTL_TPR0_TCKE_BFO		17
#define PCTL_TPR0_TCKE_BFW		5
#define PCTL_TPR0_TPD_EN_BFO		23
#define PCTL_TPR0_TPD_EN_BFW		1

#define PCTL_TPR1_TRRD_BFO		0
#define PCTL_TPR1_TRRD_BFW		4
#define PCTL_TPR1_TRC_BFO		4
#define PCTL_TPR1_TRC_BFW		6
#define PCTL_TPR1_TRCD_BFO		10
#define PCTL_TPR1_TRCD_BFW		4
#define PCTL_TPR1_TCCD_BFO		14
#define PCTL_TPR1_TCCD_BFW		3
#define PCTL_TPR1_TWTR_BFO		17
#define PCTL_TPR1_TWTR_BFW		3
#define PCTL_TPR1_TRTW_BFO		20
#define PCTL_TPR1_TRTW_BFW		4
#define PCTL_TPR1_TFAW_BFO		24
#define PCTL_TPR1_TFAW_BFW		5

#define PCTL_TPR2_INIT_REF_NUM_BFO		0
#define PCTL_TPR2_INIT_REF_NUM_BFW		4
#define PCTL_TPR2_INIT_NS_EN_BFO		4
#define PCTL_TPR2_INIT_NS_EN_BFW		1
#define PCTL_TPR2_TMRD_BFO		5
#define PCTL_TPR2_TMRD_BFW		3
#define PCTL_TPR2_TNS_BFO		8
#define PCTL_TPR2_TNS_BFW		4
#define PCTL_TPR2_TUS_BFO		12
#define PCTL_TPR2_TUS_BFW		10
#define PCTL_TPR2_TRST_US_BFO		22
#define PCTL_TPR2_TRST_US_BFW		10
#define PCTL_TPR2_TIME_UNIT		1024

#define PCTL_MR_CWL_BFO		0
#define PCTL_MR_CWL_BFW		5
#define PCTL_MR_CL_BFO		5
#define PCTL_MR_CL_BFW		5
#define PCTL_MR_ADDLAT_BFO		10
#define PCTL_MR_ADDLAT_BFW		5

#define PCTL_EMR0_CASLAT_BFO		2
#define PCTL_EMR0_CASLAT_MASK		0x1D
#define PCTL_EMR0_TWRAUTO_BFO		9
#define PCTL_EMR0_TWRAUTO_BFW		3

#define PCTL_EMR1_ADDLAT_BFO		3
#define PCTL_EMR1_ADDLAT_BFW		3

#define PCTL_EMR2_CWL_BFO		3
#define PCTL_EMR2_CWL_BFW		3

#define PCTL_CMD_DPIN_RSTN_BFO		0
#define PCTL_CMD_DPIN_RSTN_BFW		1
#define PCTL_CMD_DPIN_CKE_BFO		1
#define PCTL_CMD_DPIN_CKE_BFW		1
#define PCTL_CMD_DPIN_ODT_BFO		2
#define PCTL_CMD_DPIN_ODT_BFW		1

#define PCTL_BCR_STOP_BFO		0
#define PCTL_BCR_STOP_BFW		1
#define PCTL_BCR_CMP_BFO		1
#define PCTL_BCR_CMP_BFW		1
#define PCTL_BCR_LOOP_BFO		2
#define PCTL_BCR_LOOP_BFW		1
#define PCTL_BCR_DIS_MASK_BFO		3
#define PCTL_BCR_DIS_MASK_BFW		1
#define PCTL_BCR_AT_STOP_BFO		4
#define PCTL_BCR_AT_STOP_BFW		1
#define PCTL_BCR_FLUSH_CMD_BFO		8
#define PCTL_BCR_FLUSH_CMD_BFW		1
#define PCTL_BCR_FLUSH_WD_BFO		9
#define PCTL_BCR_FLUSH_WD_BFW		1
#define PCTL_BCR_FLUSH_RGD_BFO		10
#define PCTL_BCR_FLUSH_RGD_BFW		1
#define PCTL_BCR_FLUSH_RD_BFO		11
#define PCTL_BCR_FLUSH_RD_BFW		1
#define PCTL_BCR_FLUSH_RD_EXPC_BFO		16
#define PCTL_BCR_FLUSH_RD_EXPC_BFW		14

#define PCTL_BST_ERR_FST_TH_BFO		0
#define PCTL_BST_ERR_FST_TH_BFW		12
#define PCTL_BST_ERR_CNT_BFO		16
#define PCTL_BST_ERR_CNT_BFW		14

#define PCTL_BSRAM0_CMD_LEVEL_BFO		0
#define PCTL_BSRAM0_CMD_LEVEL_BFW		12
#define PCTL_BSRAM0_WD_LEVEL_BFO		16
#define PCTL_BSRAM0_WD_LEVEL_BFW		14

#define PCTL_BSRAM1_RG_LEVEL_BFO		0
#define PCTL_BSRAM1_RG_LEVEL_BFW		14
#define PCTL_BSRAM1_RD_LEVEL_BFO		16
#define PCTL_BSRAM1_RD_LEVEL_BFW		14

#define WRAP_MISC_PAGE_SIZE_BFO		0
#define WRAP_MISC_PAGE_SIZE_BFW		4
#define WRAP_MISC_BANK_SIZE_BFO		4
#define WRAP_MISC_BANK_SIZE_BFW		2
#define WRAP_MISC_BST_SIZE_BFO		6
#define WRAP_MISC_BST_SIZE_BFW		2
#define WRAP_MISC_DDR_PARAL_BFO		8
#define WRAP_MISC_DDR_PARAL_BFW		1

#define PCTL_DPIN_START		0x08
#ifndef LANGUAGE_ASSEMBLY
struct ms_rxi310_portmap {
	volatile unsigned int ccr;           /* 0x000 */
	volatile unsigned int dcr;           /* 0x004 */
	volatile unsigned int iocr;          /* 0x008 */
	volatile unsigned int csr;           /* 0x00c */
	volatile unsigned int drr;           /* 0x010 */
	volatile unsigned int tpr0;          /* 0x014 */
	volatile unsigned int tpr1;          /* 0x018 */
	volatile unsigned int tpr2;          /* 0x01c */
	volatile unsigned int tpr3;          /* 0x020 */
	volatile unsigned int cdpindiff;   /* 0x024*/
	volatile unsigned int cdpin;         /* 0x028*/
	volatile unsigned int tdpin;         /* 0x02c */
	volatile unsigned int mr;            /* 0x030 */
	volatile unsigned int emr0;          /* 0x034 */
	volatile unsigned int emr1;          /* 0x038 */
	volatile unsigned int emr2;          /* 0x03c */
	volatile unsigned int emr3;          /* 0x040 */
	volatile unsigned int emr4;          /* 0x044 */
	volatile unsigned int emr5;          /* 0x048 */
	volatile unsigned int emr6;          /* 0x04c */
	volatile unsigned int dllcr3;        /* 0x050 */
	volatile unsigned int dllcr4;        /* 0x054 */
	volatile unsigned int dllcr5;        /* 0x058 */
	volatile unsigned int dllcr6;        /* 0x05c */
	volatile unsigned int dllcr7;        /* 0x060 */
	volatile unsigned int dllcr8;        /* 0x064 */
	volatile unsigned int dqtr0;         /* 0x068 */
	volatile unsigned int dqtr1;         /* 0x06c */
	volatile unsigned int dqtr2;         /* 0x070 */
	volatile unsigned int dqtr3;         /* 0x074 */
	volatile unsigned int dqtr4;         /* 0x078 */
	volatile unsigned int dqtr5;         /* 0x07c */
	volatile unsigned int dqtr6;         /* 0x080 */
	volatile unsigned int dqtr7;         /* 0x084 */
	volatile unsigned int dqstr;         /* 0x088 */
	volatile unsigned int dqsbtr;        /* 0x08c */
	volatile unsigned int odtcr;         /* 0x090 */
	volatile unsigned int dtr0;          /* 0x094 */
	volatile unsigned int dtr1;          /* 0x098 */
	volatile unsigned int dtar;          /* 0x09c */
	volatile unsigned int zqcr0;         /* 0x0a0 */
	volatile unsigned int zqcr1;         /* 0x0a4 */
	volatile unsigned int zqsr;          /* 0x0a8 */
	volatile unsigned int rslr0;         /* 0x0ac */
	volatile unsigned int rslr1;         /* 0x0b0 */
	volatile unsigned int rslr2;         /* 0x0b4 */
	volatile unsigned int rslr3;         /* 0x0b8 */
	volatile unsigned int rdgr0;         /* 0x0bc */
	volatile unsigned int rdgr1;         /* 0x0c0 */
	volatile unsigned int rdgr2;         /* 0x0c4 */
	volatile unsigned int rdgr3;         /* 0x0c8 */
	volatile unsigned int mxsl;          /* 0x0cc */
	volatile unsigned int bcr;           /* 0x0d0 */
	volatile unsigned int bct;           /* 0x0d4 */
	volatile unsigned int bcm;           /* 0x0d8 */
	volatile unsigned int bst;           /* 0x0dc */
	volatile unsigned int bsram0;        /* 0x0e0 */
	volatile unsigned int bsram1;        /* 0x0e4 */
	volatile unsigned int ber;           /* 0x0e8 */
	volatile unsigned int byr;           /* 0x0ec */
	volatile unsigned int err;           /* 0x0f0*/
	volatile unsigned int pctl_svn;      /* 0x0f4 */
	volatile unsigned int pctl_idr;      /* 0x0f8 */

	/* SDR_PHY CONTROL REGISTER*/
	volatile unsigned int phy_dly0;      /* 0x100 */
	volatile unsigned int phy_dly1_rst;  /* 0x104 */
	volatile unsigned int phy_dly_clk;   /* 0x108 */
	volatile unsigned int phy_dly_st;    /* 0x10c */
	volatile unsigned int phy_dly_num;   /* 0x110 */
	volatile unsigned int reserved0[69];

	/* WRAP CONTROL REGISTER*/
	volatile unsigned int misc;          /* 0x224 */
	volatile unsigned int cq_ver;        /* 0x228 */
	volatile unsigned int cq_mon;        /* 0x22c */
	volatile unsigned int wq_ver;        /* 0x230 */
	volatile unsigned int wq_mon;        /* 0x234 */
	volatile unsigned int rq_ver;        /* 0x240 */
	volatile unsigned int rq_mon;        /* 0x244 */
	volatile unsigned int reserved1[22];
	volatile unsigned int wwrap_idr;     /* 0x2a0 */
	volatile unsigned int wrap_svn;      /* 0x2a4 */

}; /*ms_rxi310_portmap*/

#define QFIFO_CMD_BANK_BFO		34 /* [36:34]*/
#define QFIFO_CMD_BANK_BFW		3
#define QFIFO_CMD_PAGE_BFO		19 /* [33:19]*/
#define QFIFO_CMD_PAGE_BFW		15
#define QFIFO_CMD_COLU_BFO		6 /*[15: 6]*/
#define QFIFO_CMD_COLU_BFW		10
#define QFIFO_BST_LEN_BFO		1 /* [5:1]*/
#define QFIFO_BST_LEN_BFW		4
#define QFIFO_CMD_WRRD_BFO		0 /*[0]*/
#define QFIFO_CMD_WRRD_BFW		1

enum qfifo_wrrd {
	RD_CMD      = 0,
	WR_CMD      = 1
};

struct bist_cmd_data_b37 {
	uint32_t        CMD_BANK;
	uint32_t        CMD_PAGE;
	uint32_t        CMD_COLU;
	uint32_t        BST_LEN;
	enum qfifo_wrrd WRRD;
};

struct bist_data_b128 {
	uint32_t        data_3;
	uint32_t        data_2;
	uint32_t        data_1;
	uint32_t        data_0;
};

#endif /* not assembly*/

#define MR3_VALUE 0
#define DLL_RESET (1<<8)
#ifdef DDR_H5TQ2G_112M
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_HALF_DQ			1
#define DRAM_COL_BIT			10
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			8928
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE		0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			160000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			4

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			35000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			4

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			14000

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			8

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			4

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			14000

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			50000

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			40000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				5

/*read lantency:6*/
#define CL				6

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x08


/*MR0*/

#define TWR_AUTO_PRECHARGE_25		1

/*IOCR*/
#define IOCR_VALUE	((TPHY_RDATA_EN<<PCTL_IOCR_TPHY_RD_EN_BFO)|\
				(TPHY_WDATA<<PCTL_IOCR_TPHY_WD_BFO)|0x0c)

#define MR0_VALUE	((CAL_SETTING << PCTL_EMR0_CASLAT_BFO) | \
		(TWR_AUTO_PRECHARGE_25 << PCTL_EMR0_TWRAUTO_BFO)| \
					DLL_RESET)
#define CLW5_SETTING			0
#define MR2_VALUE	(CLW5_SETTING << PCTL_EMR2_CWL_BFO)
#define MR1_VALUE			0x40
#define MR_INFO_VALUE		0x04500063
#define DDRC_SET_OFFSET			1
#endif

#ifdef DDR3_1GBIT_DEMOBOARD

/* QFN88 DDRC using 2133 parameter */
#ifdef CONFIG_RTS3917_QFN88_DDR3_1GBIT
#define CONFIG_ODT_OFF
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			938
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			110000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			15000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			8

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			33000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			7

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13090

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			10

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			8

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13090

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			46090

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			35000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				10

/*read lantency:6*/
#define CL				14

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			4

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			3

#define CAL_SETTING			0x09

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		6
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x0
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x28
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x228
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE		0x088000e5

#define DDRC_SET_OFFSET	1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#else

#ifdef CONFIG_DDR_400
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_HALF_DQ			1
#define DRAM_COL_BIT			10
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1875
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			110000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			4

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			37500

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			6

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13125

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			8

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			4

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13125

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			50625

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			50000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				6

/*read lantency:6*/
#define CL				7

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			2

#define CAL_SETTING			0x06

#define DRAM_TCKE_TCK		9

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE_25		1
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
		DLL_RESET | 0x000)

/*IOCR*/
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)

#define MR2_VALUE			0x208
#define MR1_VALUE			0x6

/*
 *#define MR2_VALUE			0x8
 *#define MR1_VALUE			0x2
 */

#ifdef CLOCL_GATING
#define MR_INFO_VALUE		(0x83 | (0x04 << 15))
#else
#define MR_INFO_VALUE		0x83
#endif
#define DDRC_SET_OFFSET	1
#endif

#ifdef CONFIG_RTS3917_DDR_1066
#define CONFIG_ODT_OFF
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1875
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			260000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			4

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			37500

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			4

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13125

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			7

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			8

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13125

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			50625

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			50000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				6

/*read lantency:6*/
#define CL				7

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x0c

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		3
#endif


#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x4
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x8
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x208
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0x83)
#define DDRC_SET_OFFSET	1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#endif

#ifdef CONFIG_RTS3917_DDR_1333
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1500
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			260000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			5

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			36000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			5

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13500

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			8

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			5

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13500

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			49500

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			45000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				7

/*read lantency:6*/
#define CL				9

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x14

#define DRAM_TCKE_TCK		3

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1
/*MR0*/
#define TWR_AUTO_PRECHARGE	0x5
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x10
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x210
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0xa4)
#define DDRC_SET_OFFSET		1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#endif

#ifdef CONFIG_RTS3917_DDR_1600
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1250
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			260000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			7

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			37500

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			6

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13910

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			9

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			5

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13750

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			48750

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			40000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				9

/*read lantency:6*/
#define CL				13

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			3

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			2

#define CAL_SETTING			0x05

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		3
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x6
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x20
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x220
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0xe5)

#define DDRC_SET_OFFSET		1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#endif

#ifdef CONFIG_RTS3917_DDR_1866
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1072
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			260000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			7

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			34000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			6

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13910

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			10

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			7

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13910

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			47910

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			35000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				9

/*read lantency:6*/
#define CL				13

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			3

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			2

#define CAL_SETTING			0x05

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		3
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x7
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x20
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x220
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0xe5)

#define DDRC_SET_OFFSET		1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#endif

#ifdef CONFIG_RTS3917_DDR_2133
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			938
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			260000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			8

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			33000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			7

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			13090

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			10

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			8

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			13090

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			46090

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			35000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				10

/*read lantency:6*/
#define CL				14

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			4

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			3

#define CAL_SETTING			0x09

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		6
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x0
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			DLL_RESET | (TWR_AUTO_PRECHARGE << 9))

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

#ifdef CONFIG_ODT_OFF
#define MR2_VALUE			0x28
#define MR1_VALUE			0x2
#else
#define MR2_VALUE			0x228
#define MR1_VALUE			0x6
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0xe5)

#define DDRC_SET_OFFSET	1
#define TPR5_VALUE		0x7
#define TPD_EN			1
#endif

#endif
#endif

/* ddr2-1333 setting */
#ifdef DDR2_MCM

#ifdef CONFIG_RTS3917_QFN88_DDR2_512MBIT

#define CONFIG_ODT_OFF
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			2
#define DRAM_SDR			0
#define DRAM_BANK_BIT			2
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1500
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			130000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			15000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			7

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			18000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			47500

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			7

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			15000

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			6

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			5

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			2

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			15000

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			60000

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			45000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			5

/*write lantency: 6*/
#define CWL				6

/*read lantency:7*/
#define CL				7

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x04

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		3
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x0
#define MR0_VALUE		(0x1073 | TWR_AUTO_PRECHARGE)

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

/*MR2*/
#define MR2_VALUE			0
#ifdef CONFIG_ODT_OFF
#define MR1_VALUE			0
#else
#define MR1_VALUE			0x44
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK - 2) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0x83)

#define DDRC_SET_OFFSET		1
#define TPR5_VALUE		0xb
#define TPD_EN			1

#else
#ifdef CONFIG_RTS3917_BGA173_DDR2_512MBIT

#define CONFIG_ODT_OFF
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			2
#define DRAM_SDR			0
#define DRAM_BANK_BIT			2
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			1500
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			130000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			15000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			7

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			47500

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			7

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			15000

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			6

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			5

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			2

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			15000

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			60000

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			45000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			5

/*write lantency: 6*/
#define CWL				6

/*read lantency:7*/
#define CL				7

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x04

#ifdef CONFIG_DDR_POWER_GATING
#define DRAM_TCKE_TCK		11
#else
#define DRAM_TCKE_TCK		3
#endif

#define DRAM_TRST_TUS_PS	200000000

#define DRAM_TUS_PS		500000000

#define DRAM_TPR2_TNS		1

/*MR0*/
#define TWR_AUTO_PRECHARGE	0x0
#define MR0_VALUE		(0x1073 | TWR_AUTO_PRECHARGE)

/*IOCR & TIE_DPIN*/
#ifdef CONFIG_ODT_OFF
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x18)
#define TIE_DPIN_VALUE	0x3
#else
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define TIE_DPIN_VALUE	0x3
#endif

/*MR2*/
#define MR2_VALUE			0
#ifdef CONFIG_ODT_OFF
#define MR1_VALUE			0
#else
#define MR1_VALUE			0x44
#endif

#define MR_INFO_VALUE	((((DRAM_TRTP_TCK - 2) / DFI_RATIO + 1) << 25) | \
			((DRAM_TWR_PS / PCTL_PERIOD_PS + 1) << 20) | 0x83)

#define DDRC_SET_OFFSET		1
#define TPR5_VALUE		0xb
#define TPD_EN			1
#endif
#endif
#endif

#ifdef CONFIG_DDR_H5TQ2G_200M
#define DRAM_CHIP_NUM			1
#define DRAM_TYPE			3
#define DRAM_SDR			0
#define DRAM_HALF_DQ			1
#define DRAM_COL_BIT			10
#define DRAM_BANK_BIT			3
#define DRAM_BST_LEN			8
#define DRAM_PERIOD_PS			2500
#define DFI_RATIO			0x02
#define PCTL_PERIOD_PS			(DRAM_PERIOD_PS * DFI_RATIO)

#define DRAM_SIZE			0x3fffffff

/*Refresh Command to Refresh command:160ns,
 *JESD79-3F-DDR3, page 170, table 61
 */
#define DRAM_TRFC_PS			500000

/*Serial Refresh Command Periodic Interval:60us,
 *DRAM_TREF_PS < TREF_NUM*(7.8us)*1000000
 */
#define TREF_NUM			9
#define DRAM_TREF_PS			60000000

/*Read command to precharge delay cycles, max(4nCK, 7.5ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRTP_TCK			4

/*Write Command to Percharge Command:15ns,
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWR_PS			15000

/*Active Command to Percharge Command:35ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRAS_PS			35000

/*Active Command to Active Command at Different Bank:max(4nCK,6ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TRRD_TCK			4

/*Percharge Command to Active Command:13.75ns,
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRP_PS			14000

/*Read Command to Write Command:8,
 *large than(DRAM read latency + 6 - DRAM write latency)
 */
#define DRAM_TRTW_TCK			8

/*Write Command to Read Command:max(4nCK,7.5ns),
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TWTR_TCK			4

/*Read/Write Command to Read/Write Command:4ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TCCD_TCK			4

/*Active Command to Read/Write Command:13.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRCD_PS			14000

/*Active Command to Active Command at Same Bank:48.75ns
 *JESD79-3F-DDR3, page 174, table 65
 */
#define DRAM_TRC_PS			50000

/* Active window:37.5ns
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TFAW_PS			40000

/*Mode Register Command to The Next Valid Command:4
 *JESD79-3F-DDR3, page 183, table 68
 */
#define DRAM_TMRD_TCK			4

/*write lantency: 5*/
#define CWL				5

/*read lantency:6*/
#define CL				6

/*additional lantency*/
#define AL				0

/*delay latency from DFI read command to
 *dfi_rddata_en signal. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_RDATA_EN			1

/*delay latency from dfi_wrdata_en signal
 *to dfi_wddata. If it works on DDR,
 *the smallest value is 1
 */
#define TPHY_WDATA			1

#define CAL_SETTING			0x04


/*MR0*/

#define TWR_AUTO_PRECHARGE_25		1
#define MR0_VALUE	((CAL_SETTING  << PCTL_EMR0_CASLAT_BFO) | \
			(TWR_AUTO_PRECHARGE_25 << PCTL_EMR0_TWRAUTO_BFO))

/*IOCR*/
#define IOCR_VALUE	((TPHY_RDATA_EN << PCTL_IOCR_TPHY_RD_EN_BFO) | \
			(TPHY_WDATA << PCTL_IOCR_TPHY_WD_BFO) | 0x10)
#define CLW5_SETTING			0
#define MR2_VALUE	(CLW5_SETTING << PCTL_EMR2_CWL_BFO)

#define MR1_VALUE			0x40
#define MR_INFO_VALUE		0x62
#define DDRC_SET_OFFSET			1
#endif

#define CHOP_SIZE_VALUE			0x04

/* ddr address remap set value */
#ifdef DDR3_1GBIT_DEMOBOARD
#define ROW_REMAP0_VALUE		0x88888888
#define ROW_REMAP1_VALUE		0x88888888
#define ROW_REMAP2_VALUE		0x00000008
#define BANK_REMAP0_VALUE		0x00002928
#define COLU_REMAP0_VALUE		0x0
#define COLU_REMAP1_VALUE		0x000000ff
#define RANK_REMAP0_VALUE		0x0000001f
#define CH_REMAP0_VALUE			0x0000001f
#define BG_REMAP0_VALUE			0x000003ff
#endif

#ifdef DDR2_MCM
#define ROW_REMAP0_VALUE		0x77777777
#define ROW_REMAP1_VALUE		0x77777777
#define ROW_REMAP2_VALUE		0x00000007
#define BANK_REMAP0_VALUE		0x00007d28
#define COLU_REMAP0_VALUE		0x0
#define COLU_REMAP1_VALUE		0x000000ff
#define RANK_REMAP0_VALUE		0x0000001f
#define CH_REMAP0_VALUE			0x0000001f
#define BG_REMAP0_VALUE			0x000003ff
#endif

#define SLOT_PARK_VALUE			0x00010000
#define SLOT_STARVE_VALUE		0xff020104
#define SLOT_CNTR_VALUE			0x00201020

#define CBCTL0_VALUE			0x000f0017
#define CBCTL1_VALUE			0x000f0f03
#define CBSTARVE0_VALUE			0x400
#define CBSTARVE1_VALUE			0x400
#define CBSTARVE2_VALUE			0x400
#define CBSTARVE3_VALUE			0x400
#define CBSTARVE4_VALUE			0x400
#define BANK_SCH_CTRL_VALUE		0x0

/*CCR*/
#define DRAM_INIT_EN			0x01

/*DCR*/
#ifdef CONFIG_DDR_DQ32
#define DRAM_HALF_DQ 	0
#define DRAM_DQ32		1
#else
#define DRAM_HALF_DQ 	1
#define DRAM_DQ32		0
#endif
#define DCR_VALUE	((DFI_RATIO << PCTL_DCR_DFI_RATE_BFO) | \
			(DRAM_HALF_DQ << PCTL_DCR_HALF_DQ_BFO) | \
			(DRAM_DQ32 << PCTL_DCR_DQ32_BFO) | \
			(DRAM_SDR << PCTL_DCR_SDR_BFO) | \
			(DRAM_TYPE << PCTL_DCR_DDR3_BFO))

/*CSR*/
#define DISABLE_BIST_FUNC		1
#define DISABLE_MEMORY_ACCESS		1
#define MEM_MEM_ACCESS_STATE		0
#define DISABLE_DT_MODE			1
#define CSR_DIS_BIST_MEM  ((DISABLE_BIST_FUNC << PCTL_CSR_BIST_IDLE_BFO) | \
			(DISABLE_DT_MODE << PCTL_CSR_DT_IDLE_BFO) | \
			(DISABLE_MEMORY_ACCESS << PCTL_CSR_MEM_IDLE_BFO))
#define CSR_MEM_ACCESS_STATE	((DISABLE_BIST_FUNC << \
			PCTL_CSR_BIST_IDLE_BFO) | \
			(DISABLE_DT_MODE << PCTL_CSR_DT_IDLE_BFO) | \
			(MEM_MEM_ACCESS_STATE << PCTL_CSR_MEM_IDLE_BFO))

/*DRR*/
#define REF_DIS				1
#define REF_EN				0
#define DRR_VALUE	((REF_EN << PCTL_DRR_REF_DIS_BFO) | \
		(TREF_NUM << PCTL_DRR_REF_NUM_BFO) | \
	((DRAM_TREF_PS / PCTL_PERIOD_PS - 0x100) << PCTL_DRR_TREF_BFO) | \
		((DRAM_TRFC_PS / PCTL_PERIOD_PS + 1) << PCTL_DRR_TRFC_BFO))

/*TPR0*/
#ifdef CONFIG_TARGET_FPGA
#define TPR0_VALUE	(((DRAM_TRTP_TCK/DFI_RATIO + DDRC_SET_OFFSET) \
		<< PCTL_TPR0_TRTP_BFO) | \
		((DRAM_TWR_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
		<< PCTL_TPR0_TWR_BFO) | \
		((DRAM_TRAS_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
		<< PCTL_TPR0_TRAS_BFO) | \
		((DRAM_TRP_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
		<< PCTL_TPR0_TRP_BFO))
#else
#ifdef CONFIG_DDR_POWER_GATING
#define TPR0_VALUE		((1 << 23) | \
		((DRAM_TCKE_TCK + 1) << PCTL_TPR0_TCKE_BFO) | \
((DRAM_TRTP_TCK / DFI_RATIO + DDRC_SET_OFFSET) << PCTL_TPR0_TRTP_BFO) | \
((DRAM_TWR_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TWR_BFO) | \
((DRAM_TRAS_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TRAS_BFO) | \
((DRAM_TRP_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TRP_BFO))
#else
#define TPR0_VALUE	((TPD_EN << PCTL_TPR0_TPD_EN_BFO) | \
((DRAM_TCKE_TCK + 1) << PCTL_TPR0_TCKE_BFO) | \
((DRAM_TRTP_TCK / DFI_RATIO + DDRC_SET_OFFSET) << PCTL_TPR0_TRTP_BFO) | \
((DRAM_TWR_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TWR_BFO) | \
((DRAM_TRAS_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TRAS_BFO) | \
((DRAM_TRP_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) << PCTL_TPR0_TRP_BFO))
#endif
#endif
/*TPR1*/
#define TPR1_VALUE	(((DRAM_TFAW_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
			<< PCTL_TPR1_TFAW_BFO) | \
			((DRAM_TRTW_TCK / DFI_RATIO + DDRC_SET_OFFSET) \
			 << PCTL_TPR1_TRTW_BFO) | \
			((DRAM_TWTR_TCK / DFI_RATIO + DDRC_SET_OFFSET) \
			 << PCTL_TPR1_TWTR_BFO) | \
			((DRAM_TCCD_TCK / DFI_RATIO) << PCTL_TPR1_TCCD_BFO) | \
			((DRAM_TRCD_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
			 << PCTL_TPR1_TRCD_BFO) | \
			((DRAM_TRC_PS / PCTL_PERIOD_PS + DDRC_SET_OFFSET) \
			 << PCTL_TPR1_TRC_BFO) | \
			((DRAM_TRRD_TCK / DFI_RATIO + DDRC_SET_OFFSET) \
			 << PCTL_TPR1_TRRD_BFO))

/*TPR2*/
#define INIT_REF_NUM			4
#define INIT_NS_EN			1

#ifdef CONFIG_TARGET_FPGA
#define TPR2_VALUE	(((DRAM_TMRD_TCK / DFI_RATIO + 1) \
				<< PCTL_TPR2_TMRD_BFO) | \
				(INIT_NS_EN << PCTL_TPR2_INIT_NS_EN_BFO) | \
				(INIT_REF_NUM << PCTL_TPR2_INIT_REF_NUM_BFO))
#else
#define TPR2_VALUE	(((DRAM_TRST_TUS_PS / PCTL_PERIOD_PS / \
		PCTL_TPR2_TIME_UNIT + 10) << PCTL_TPR2_TRST_US_BFO) | \
		((DRAM_TUS_PS/PCTL_PERIOD_PS / PCTL_TPR2_TIME_UNIT \
		   + 10) << PCTL_TPR2_TUS_BFO) | \
		(DRAM_TPR2_TNS  << PCTL_TPR2_TNS_BFO) | \
		((DRAM_TMRD_TCK / DFI_RATIO + 1) << PCTL_TPR2_TMRD_BFO) | \
		(INIT_NS_EN << PCTL_TPR2_INIT_NS_EN_BFO) | \
		(INIT_REF_NUM << PCTL_TPR2_INIT_REF_NUM_BFO))
#endif
/*MR1*/
#define AL0_SETTING			0
#define RZQ_DIV4			0x04
#define RZQ_DIV7			0x02

/*MISC*/
#ifdef CONFIG_DDR_DQ32
#define DRAM_COL_BIT		11
#else
#define DRAM_COL_BIT		10
#endif
#define MISC_VLAUE  ((((DRAM_BST_LEN - 4) >> 2) << WRAP_MISC_BST_SIZE_BFO) | \
			((DRAM_BANK_BIT - 1) << WRAP_MISC_BANK_SIZE_BFO) | \
			(((DRAM_COL_BIT - 8) + (DRAM_CHIP_NUM - 1) + 1) \
			 << WRAP_MISC_PAGE_SIZE_BFO))

/********************************
 *system ddr config register	*
 ********************************
 */
#define DDR_PHY_CFG			0x18860024
#define POR15N_DV						0x1000
#define PWDPAD15N_DV					0x800
#define CFG_DPI_CRT_RST_N				0x400
#define REF_CMD_RST_N_TX				0x200
#define REF_CMD_RST_N_RX				0x100
#define REG_FORCE_RST_N_TX				0x20
#define REG_FORCE_RST_N_RX				0x10
#define REG_CLKEN_PLLREG				0x02
#define REG_CLKEN_DCPHY				0x01
#define DDR_START_STEP1			(REG_CLKEN_DCPHY |\
				REG_CLKEN_PLLREG | REG_FORCE_RST_N_RX | \
				REG_FORCE_RST_N_TX | REF_CMD_RST_N_RX  | \
				REF_CMD_RST_N_TX)
#define DDR_START_STEP2		(DDR_START_STEP1 | CFG_DPI_CRT_RST_N)
#define DDR_START_STEP3		(DDR_START_STEP2 | PWDPAD15N_DV | POR15N_DV)

#define SYS_DDR_STATUS_ADDR			0x18860010
#define DPI_PLL_STABLE				0x01

#if ((defined CONFIG_TARGET_ASIC) || (defined CONFIG_TARGET_EMULATOR))
/********************************
 *	ddr phy register	*
 *********************************
 */
#define DDR_PHY_BASE		0x18080000
#define CRT_CTL_OFFSET			0
#define DDR_PHY_CRT_CTL		(DDR_PHY_BASE + CRT_CTL_OFFSET)
#define DDR_PHY_CRT_CTL_DEF	0x0c00b021
#define MCK_CKE_SEL_CS		0x40000000
#define MCK_CKE_SEL_MASK	0x40000000
#define FORCE_RSTN_0		0x08000000
#define RELEASE_RSTN		0x00000000
#define FORCE_RSTN_MASK		0x0c000000
#define RELEASE_CKE		0x00000000
#define FORCE_CKE_MASK		0x00000c00
#define DDR_PHY_PLL_CTL_MASK	0xffffffdf
#define DPI_DDR2_EN				0x02
#define PIN_MUX_MODE			0x10
#define PIN_MUX_MODE_MASK	0x00000010

#ifdef DDR3_1GBIT_DEMOBOARD
#define DDR_MODE		0x01
#endif
#ifdef DDR2_MCM
#define DDR_MODE		0x03
#endif
#define DDR_MODE_MASK		0x00000003

#define PLL_CTL0_OFFSET				0x04
#define DDR_PHY_PLL_CTL0	(DDR_PHY_BASE + PLL_CTL0_OFFSET)
#define DPI_MCK_PLL_CLK_EN	0x1fff
#define DPI_MCK_PLL_CLK_EN_MASK	0x1fff
#define DDR_PHY_PLL_CTL0_DEF			0x200000

#define PLL_CTL1_OFFSET		0x08
#define DDR_PHY_PLL_CTL1	(DDR_PHY_BASE + PLL_CTL1_OFFSET)
#define MCK_MC_EN					0x80000000
#define MCK_MC_EN_MASK		0x80000000
#define DDR_PHY_PLL_CTL1_DEF		0x30a00000
#define PLL_SEL_INTERNAL_LDO		0x0000c000
#define PLL_SEL_EXTERNAL_LDO		0x00000000
#define PLL_SEL_LDO_MASK		0x0000c000
#define MCK_CK_EN					0x10000
#define CLK1X_EN					0x20000
#define CLK_OE						0x70000
#define CLK_OE_MASK			0x70000
#define MCK_CLK0_SYNC_EN		0x01
#define MCK_CMD_SYNC_EN			0x02
#define MCK_DQS0_SYNC_EN		0x04
#define MCK_DQS1_SYNC_EN		0x08
#define MCK_DQ0_SYNC_EN			0x10
#define MCK_DQ1_SYNC_EN			0x20
#define MCK_CS_SYNC_EN			0x40
#define DDR_PHY_PLL_CTL1_OESYNC_MASK	0x00000fff

#define PLL_CTL2_OFFSET		0x0c
#define DDR_PHY_PLL_CTL2	(DDR_PHY_BASE + PLL_CTL2_OFFSET)
#define DPI_PLL_DUM		0x00
#define DPI_PLL_DUM_MASK	0x000000ff

#define PLL_PI0_OFFSET		0x10
#define DDR_PHY_PLL_PI0		(DDR_PHY_BASE + PLL_PI0_OFFSET)
#define DDR_PHY_PLL_PI0_MASK	0x1f1f1f1f

#define PLL_PI1_OFFSET		0x14
#define DDR_PHY_PLL_PI1		(DDR_PHY_BASE + PLL_PI1_OFFSET)
#define DDR_PHY_PLL_PI1_MASK	0x001f1f1f

#define PLL_CTL3_OFFSET		0x18
#define DDR_PHY_PLL_CTL3	(DDR_PHY_BASE + PLL_CTL3_OFFSET)
#define DDR_PHY_PLL_CTL3_MASK			0xEFFCFFF7
#define CKREF_TOP			0x10000000

#define SSC0_OFFSET		0x1C
#define DDR_PHY_SSC0		(DDR_PHY_BASE + SSC0_OFFSET)
#define DDR_PHY_SSC0_DEF			0x3f48
#define DDR_PHY_SSC_FLAG			0x08
#define DDR_PHY_SSC_FLAG_MASK	0x08
#define DDR_PHY_SSC_EN				0x02
#define DDR_PHY_SSC_EN_MASK	0x02

#define SSC1_OFFSET		0x20
#define DDR_PHY_SSC1		(DDR_PHY_BASE + SSC1_OFFSET)
#define SSC2_OFFSET		0x24
#define DDR_PHY_SSC2		(DDR_PHY_BASE + SSC2_OFFSET)
#define DPI_F_CODE_MASK		0x07ff07ff
/*SSC profile step number:[27:16]*/
#define DPI_TBASE			3	0
#define DDR_PHY_F_MASK				0xffffe000

#define SSC3_OFFSET		0x28
#define DDR_PHY_SSC3		(DDR_PHY_BASE + SSC3_OFFSET)
#define DPI_OC_EN		0x04000000
#define DPI_OC_EN_MASK		0x04000000
#define DPI_N_CODE_MASK		0x0000ffff

#define DDR_PHY_DCC_MASK	0x20000007
#define DCC_CMD_OFFSET		0x30
#define DDR_PHY_DCC_CMD		(DDR_PHY_BASE + DCC_CMD_OFFSET)

#define DCC_DQS0_OFFSET		0x34
#define DDR_PHY_DCC_DQS0	(DDR_PHY_BASE + DCC_DQS0_OFFSET)

#define DCC_DQS1_OFFSET		0x38
#define DDR_PHY_DCC_DQS1	(DDR_PHY_BASE + DCC_DQS1_OFFSET)

#define DCC_DQS2_OFFSET		0x3C
#define DDR_PHY_DCC_DQS2	(DDR_PHY_BASE + DCC_DQS2_OFFSET)

#define DCC_DQS3_OFFSET		0x40
#define DDR_PHY_DCC_DQS3	(DDR_PHY_BASE + DCC_DQS3_OFFSET)

#define DCC_DQ0_OFFSET		0x44
#define DDR_PHY_DCC_DQ0		(DDR_PHY_BASE + DCC_DQ0_OFFSET)

#define DCC_DQ1_OFFSET		0x48
#define DDR_PHY_DCC_DQ1		(DDR_PHY_BASE + DCC_DQ1_OFFSET)

#define DCC_DQ2_OFFSET		0x4C
#define DDR_PHY_DCC_DQ2		(DDR_PHY_BASE + DCC_DQ2_OFFSET)

#define DCC_DQ3_OFFSET		0x50
#define DDR_PHY_DCC_DQ3		(DDR_PHY_BASE + DCC_DQ3_OFFSET)

#define DCC_CK_OFFSET		0x54
#define DDR_PHY_DCC_CK		(DDR_PHY_BASE + DCC_CK_OFFSET)

#define AFIFO_STR_0_OFFSET	0x80
#define DDR_PHY_AFIFO_STR_0	(DDR_PHY_BASE + AFIFO_STR_0_OFFSET)
#define AFIFO_STR_0_MASK	0x77777777

#define AFIFO_STR_1_OFFSET	0x84
#define DDR_PHY_AFIFO_STR_1	(DDR_PHY_BASE + AFIFO_STR_1_OFFSET)
#define AFIFO_STR_1_DEF_VALUE		0x003C0000

#define AFIFO_STR_2_OFFSET	0x88
#define DDR_PHY_AFIFO_STR_2	(DDR_PHY_BASE + AFIFO_STR_2_OFFSET)

#define CRT_RST_CTL_OFFSET	0x8C
#define DDR_PHY_CRT_RST_CTL	(DDR_PHY_BASE + CRT_RST_CTL_OFFSET)
#define PTR_RST_N					0x08
#define PTR_RST_N_MASK		0x08
#define RST_N						0x04
#define RST_N_MASK		0x04
#define PLL_SSC_DIG_RST_N			0x02
#define PLL_SSC_DIG_RST_N_MASK	0x02
#define PLL_LDO_RST_N				0x01
#define PLL_LDO_RST_N_MASK	0x01

#define AFIFO_STR_SEL_OFFSET		0x94
#define DDR_PHY_AFIFO_STR_SEL	(DDR_PHY_BASE + AFIFO_STR_SEL_OFFSET)
#define CS_AFIFO_SEL				0x10000000

#define PLL_PI2_OFFSET			0x98
#define DDR_PHY_PLL_PI2		(DDR_PHY_BASE + PLL_PI2_OFFSET)
#define DDR_PHY_PLL_PI2_MASK	0x1f1f1f1f

#define PLL_PI3_OFFSET			0xA0
#define DDR_PHY_PLL_PI3		(DDR_PHY_BASE + PLL_PI3_OFFSET)
#define DDR_PHY_PLL_PI3_MASK	0x00001f1f

#define PLL_CTL4_OFFSET			0x9C
#define DDR_PHY_PLL_CTL4	(DDR_PHY_BASE + PLL_CTL4_OFFSET)

#define DCC_CS_OFFSET			0xA4
#define DDR_PHY_DCC_CS		(DDR_PHY_BASE + DCC_CS_OFFSET)

#define DCC_CS1_OFFSET		0xA8
#define DDR_PHY_DCC_CS1		(DDR_PHY_BASE + DCC_CS1_OFFSET)
#define DCC_DEFAULT		0x15052800

#define PLL_CTL5_OFFSET		0xAC
#define DDR_PHY_PLL_CTL5	(DDR_PHY_BASE + PLL_CTL5_OFFSET)
#define DDR_PHY_PLL_CTL5_DEF		0x2480

#define REF_PLL					25000000
#define DDR_PLL	(REF_PLL * (DPI_N_CODE + 2 + DPI_F_CODE / 8192))

#define ADR_DLY_0_OFFSET	0x100
#define DDR_PHY_ADR_DLY_0	(DDR_PHY_BASE + ADR_DLY_0_OFFSET)
#define ADR_DLY_0_VALUE		0x00000000

#define ADR_DLY_1_OFFSET	0x104
#define DDR_PHY_ADR_DLY_1	(DDR_PHY_BASE + ADR_DLY_1_OFFSET)
#define ADR_DLY_1_VALUE		0x00000000

#define ADR_DLY_2_OFFSET	0x108
#define DDR_PHY_ADR_DLY_2	(DDR_PHY_BASE + ADR_DLY_2_OFFSET)
#define ADR_DLY_2_VALUE		0x00000000

#define CMD_DLY_0_OFFSET	0x10C
#define DDR_PHY_CMD_DLY_0	(DDR_PHY_BASE +  CMD_DLY_0_OFFSET)
#define CMD_DLY_0_VALUE		0x00000000

#define CMD_DLY_1_OFFSET	0x110
#define DDR_PHY_CMD_DLY_1	(DDR_PHY_BASE + CMD_DLY_1_OFFSET)
#define CMD_DLY_1_VALUE		0x00000000

#define PAD_OE_CTL_OFFSET	0x114
#define DDR_PHY_PAD_OE_CTL	(DDR_PHY_BASE + PAD_OE_CTL_OFFSET)
#define DDR_PHY_PAD_OE_CTL_DEF	0x9fffffff

#define PAD_OE_CLT_EX_OFFSET	0x118
#define DDR_PHY_PAD_OE_CTL_EX	(DDR_PHY_BASE + PAD_OE_CLT_EX_OFFSET)
#define DDR_PHY_PAD_OE_CTL_EX_DEF 0x3ff07

#define PAD_CTRL_PROG_OFFSET	0x130
#define DDR_PHY_PAD_CTRL_PROG	(DDR_PHY_BASE + PAD_CTRL_PROG_OFFSET)
#define ZCLK_DIV_32					0x4000000
#define RZQ_CAL_EN			0x10000
#define RZQ_CAL_EN_MASK			0x10000
#define DZQ_UP_SEL_MASK			0x70000000
#define DZQ_UP_SEL0_VALUE			0x00000000
#define DZQ_UP_SEL1_VALUE			0x10000000
#define DZQ_UP_SEL2_VALUE			0x20000000
#define DZQ_AUTO_UP_MASK			0x08000000
#define DZQ_AUTO_UP_VALUE			0x08000000
#define ZCTL_START_MASK			0x01000000
#define ZCTL_START1_VALUE			0x01000000
#define ZCTL_START0_VALUE			0x00000000
#define PAD_CTRL_PROG_DEF			0x02000003
#define PAD_CTRL_PROG_VALUE		0x02000c99

#define ZQ_NOCD2_OFFSET		0x13C
#define DDR_PHY_ZQ_NOCD2	(DDR_PHY_BASE + ZQ_NOCD2_OFFSET)
#define ZQ_OVWR_NOCD_DATA	0x00000000
#define ZQ_OVWR_NOCD_DATA_MASK	0x00003f00
#define ZQ_ENA_NOCD2		0x00000000
#define ZQ_ENA_NOCD2_MASK	0x00000001

#define PAD_ZCTRL_STATUS_OFFSET		0x140
#define DDR_PHY_PAD_ZCTRL_STATUS	(DDR_PHY_BASE + PAD_ZCTRL_STATUS_OFFSET)
#define ZCTRL_STATUS					0x80000000

#define INT_CTRL_OFFSET		0x254
#define DDR_PHY_INT_CTRL	(DDR_PHY_BASE + INT_CTRL_OFFSET)
#define INT_CTRL_VALUE		0x03
#define INT_CTRL_VALUE_MASK	0x03

#define DQ_DLY_0_0_OFFSET		0x480
#define DDR_PHY_DQ_DLY_0_0	(DDR_PHY_BASE + DQ_DLY_0_0_OFFSET)
#define DQ_DLY_0_VALUE		0x88888888

#define DQ_DLY_0_1_OFFSET		0x484
#define DDR_PHY_DQ_DLY_0_1	(DDR_PHY_BASE + DQ_DLY_0_1_OFFSET)

#define DQ_DLY_0_2_OFFSET		0x680
#define DDR_PHY_DQ_DLY_0_2	(DDR_PHY_BASE + DQ_DLY_0_2_OFFSET)

#define DQ_DLY_0_3_OFFSET		0x684
#define DDR_PHY_DQ_DLY_0_3	(DDR_PHY_BASE + DQ_DLY_0_3_OFFSET)

#define DQ_DLY_1_0_OFFSET		0x488
#define DDR_PHY_DQ_DLY_1_0	(DDR_PHY_BASE + DQ_DLY_1_0_OFFSET)
#define DQ_DLY_1_VALUE		0x00000888
#define DQ_DLY_1_MASK		0x00000fff

#define DQ_DLY_1_1_OFFSET		0x48c
#define DDR_PHY_DQ_DLY_1_1	(DDR_PHY_BASE + DQ_DLY_1_1_OFFSET)

#define DQ_DLY_1_2_OFFSET		0x688
#define DDR_PHY_DQ_DLY_1_2	(DDR_PHY_BASE + DQ_DLY_1_2_OFFSET)

#define DQ_DLY_1_3_OFFSET		0x68c
#define DDR_PHY_DQ_DLY_1_3	(DDR_PHY_BASE + DQ_DLY_1_3_OFFSET)

#define READ_CTRL_0_0_OFFSET		0x5F8
#define DDR_PHY_READ_CTRL_0_0	(DDR_PHY_BASE + READ_CTRL_0_0_OFFSET)
#define READ_CTRL_0_MASK	0x7c00007f

#define READ_CTRL_0_1_OFFSET		0x5FC
#define DDR_PHY_READ_CTRL_0_1	(DDR_PHY_BASE + READ_CTRL_0_1_OFFSET)

#define READ_CTRL_0_2_OFFSET		0x7F8
#define DDR_PHY_READ_CTRL_0_2	(DDR_PHY_BASE + READ_CTRL_0_2_OFFSET)

#define READ_CTRL_0_3_OFFSET		0x7FC
#define DDR_PHY_READ_CTRL_0_3	(DDR_PHY_BASE + READ_CTRL_0_3_OFFSET)
/*dqs_en latency from read command: [4:0]*/
#define TM_DQS_EN_2_CYCLE			0x04
#define TM_DQS_EN_2_CYCLE_5			0x05
#define TM_DQS_EN_3_CYCLE			0x06
#define TM_DQS_EN_3_CYCLE_5			0x07
#define TM_DQS_EN_4_CYCLE			0x08
#define TM_DQS_EN_4_CYCLE_5			0x09
#define TM_DQS_EN_5_CYCLE			0x0a
#define TM_DQS_EN_5_CYCLE_5			0x0b
#define TM_DQS_EN_6_CYCLE			0x0c
#define TM_DQS_EN_6_CYCLE_5			0x0d
#define TM_DQS_EN_7_CYCLE			0x0e
#define TM_DQS_EN_7_CYCLE_5			0x0f
#define TM_DQS_EN_8_CYCLE			0x10
#define TM_DQS_EN_8_CYCLE_5			0x11
#define TM_DQS_EN_9_CYCLE			0x12
#define TM_DQS_EN_9_CYCLE_5			0x13
#define TM_DQS_EN_10_CYCLE			0x14
#define TM_DQS_EN_10_CYCLE_5			0x15
#define TM_DQS_EN_11_CYCLE			0x16
#define TM_DQS_EN_11_CYCLE_5			0x17
#define TM_DQS_EN_12_CYCLE			0x18
#define TM_DQS_EN_12_CYCLE_5			0x19
#define TM_DQS_EN_13_CYCLE			0x1a
#define TM_DQS_EN_13_CYCLE_5			0x1b
#define TM_DQS_EN_14_CYCLE			0x1c
#define TM_DQS_EN_14_CYCLE_5			0x1d
#define TM_DQS_EN_15_CYCLE			0x1e
#define TM_DQS_EN_15_CYCLE_5			0x1f
#define READ_CTRL_00_SETTING			TM_DQS_EN_15_CYCLE_5
#define READ_CTRL_01_SETTING			TM_DQS_EN_15_CYCLE_5

#define READ_CTRL_1_OFFSET			0x170
#define DDR_PHY_READ_CTRL_1		(DDR_PHY_BASE + READ_CTRL_1_OFFSET)
#define TM_RD_FIFO_MASK			0x8000003f
/*Central point of DQS enable calibration
 * (FW change value during dqsen_cal_en == 0):[17:12]
 */
#define DQSEN_CAL_EN				0x10000
/*read FIFO latency from read command:[4:0]*/
#define TM_RD_FIFO_2_CYCLE			0x04
#define TM_RD_FIFO_2_CYCLE_5			0x05
#define TM_RD_FIFO_3_CYCLE			0x06
#define TM_RD_FIFO_3_CYCLE_5			0x07
#define TM_RD_FIFO_4_CYCLE			0x08
#define TM_RD_FIFO_4_CYCLE_5			0x09
#define TM_RD_FIFO_5_CYCLE			0x0a
#define TM_RD_FIFO_5_CYCLE_5			0x0b
#define TM_RD_FIFO_6_CYCLE			0x0c
#define TM_RD_FIFO_6_CYCLE_5			0x0d
#define TM_RD_FIFO_7_CYCLE			0x0e
#define TM_RD_FIFO_7_CYCLE_5			0x0f
#define TM_RD_FIFO_8_CYCLE			0x10
#define TM_RD_FIFO_8_CYCLE_5			0x11
#define TM_RD_FIFO_9_CYCLE			0x12
#define TM_RD_FIFO_9_CYCLE_5			0x13
#define TM_RD_FIFO_10_CYCLE			0x14
#define TM_RD_FIFO_10_CYCLE_5			0x15
#define TM_RD_FIFO_11_CYCLE			0x16
#define TM_RD_FIFO_11_CYCLE_5			0x17
#define TM_RD_FIFO_12_CYCLE			0x18
#define TM_RD_FIFO_12_CYCLE_5			0x19
#define TM_RD_FIFO_13_CYCLE			0x1a
#define TM_RD_FIFO_13_CYCLE_5			0x1b
#define TM_RD_FIFO_14_CYCLE			0x1c
#define TM_RD_FIFO_14_CYCLE_5			0x1d
#define TM_RD_FIFO_15_CYCLE			0x1e
#define TM_RD_FIFO_15_CYCLE_5			0x1f
#define READ_CTRL_1_SETTING		(DQSEN_CAL_EN | TM_RD_FIFO_6_CYCLE)
#define READ_CTRL_1_MASK				0xffffffe0

#define RX_CAL_CTRL_0_OFFSET			0x2d0
#define DDR_PHY_RX_CAL_CTRL_0		(DDR_PHY_BASE + RX_CAL_CTRL_0_OFFSET)
#define DELTA_FORCE				0x20
#define DELTA_FORCE_MASK			0x20
#define VR_UPD_VALUE				0x0
#define VR_UPD_MASK				0xf

#define CAL_LS_SEL_0_OFFSET			0x4F8
#define DDR_PHY_CAL_LS_SEL_0		(DDR_PHY_BASE + CAL_LS_SEL_0_OFFSET)

#define CAL_LS_SEL_1_OFFSET			0x4FC
#define DDR_PHY_CAL_LS_SEL_1		(DDR_PHY_BASE + CAL_LS_SEL_1_OFFSET)

#define CAL_LS_SEL_2_OFFSET			0x6F8
#define DDR_PHY_CAL_LS_SEL_2		(DDR_PHY_BASE + CAL_LS_SEL_2_OFFSET)

#define CAL_LS_SEL_3_OFFSET			0x6FC
#define DDR_PHY_CAL_LS_SEL_3		(DDR_PHY_BASE + CAL_LS_SEL_3_OFFSET)

#define CAL_LS_SEL_VALUE				0x11111111

#define CAL_RS_SEL_0_OFFSET			0x508
#define DDR_PHY_CAL_RS_SEL_0		(DDR_PHY_BASE + CAL_RS_SEL_0_OFFSET)

#define CAL_RS_SEL_1_OFFSET			0x50C
#define DDR_PHY_CAL_RS_SEL_1		(DDR_PHY_BASE + CAL_RS_SEL_1_OFFSET)

#define CAL_RS_SEL_2_OFFSET			0x708
#define DDR_PHY_CAL_RS_SEL_2		(DDR_PHY_BASE + CAL_RS_SEL_2_OFFSET)

#define CAL_RS_SEL_3_OFFSET			0x70C
#define DDR_PHY_CAL_RS_SEL_3		(DDR_PHY_BASE + CAL_RS_SEL_3_OFFSET)

#define CAL_RS_SEL_VALUE				0x11111111

#define DQ_IO_CTL_2_0_OFFSET		0x518
#define DDR_PHY_DQ_IO_CTL_2_0		(DDR_PHY_BASE + DQ_IO_CTL_2_0_OFFSET)
#define DQM_OE_TIME			0x00001000
#define DQM_OE_TIME_MASK		0x0000f000

#define DQ_IO_CTL_2_1_OFFSET		0x51c
#define DDR_PHY_DQ_IO_CTL_2_1		(DDR_PHY_BASE + DQ_IO_CTL_2_1_OFFSET)

#define DQS_IN_DLY_0_0_OFFSET		0x440
#define DDR_PHY_DQS_IN_DLY_0_0		(DDR_PHY_BASE + DQS_IN_DLY_0_0_OFFSET)
#define DQS_IN_DLY_MASK			0x1f1f1f1f

#define DQS_IN_DLY_0_1_OFFSET		0x444
#define DDR_PHY_DQS_IN_DLY_0_1		(DDR_PHY_BASE + DQS_IN_DLY_0_1_OFFSET)

#define DQS_IN_DLY_0_2_OFFSET		0x640
#define DDR_PHY_DQS_IN_DLY_0_2		(DDR_PHY_BASE + DQS_IN_DLY_0_2_OFFSET)

#define DQS_IN_DLY_0_3_OFFSET		0x644
#define DDR_PHY_DQS_IN_DLY_0_3		(DDR_PHY_BASE + DQS_IN_DLY_0_3_OFFSET)

#define DQS_IN_DLY_1_0_OFFSET		0x448
#define DDR_PHY_DQS_IN_DLY_1_0		(DDR_PHY_BASE + DQS_IN_DLY_1_0_OFFSET)

#define DQS_IN_DLY_1_1_OFFSET		0x44c
#define DDR_PHY_DQS_IN_DLY_1_1		(DDR_PHY_BASE + DQS_IN_DLY_1_1_OFFSET)

#define DQS_IN_DLY_1_2_OFFSET		0x648
#define DDR_PHY_DQS_IN_DLY_1_2		(DDR_PHY_BASE + DQS_IN_DLY_1_2_OFFSET)

#define DQS_IN_DLY_1_3_OFFSET		0x64C
#define DDR_PHY_DQS_IN_DLY_1_3		(DDR_PHY_BASE + DQS_IN_DLY_1_3_OFFSET)

#define DQS_IN_DLY_2_0_OFFSET		0x458
#define DDR_PHY_DQS_IN_DLY_2_0		(DDR_PHY_BASE + DQS_IN_DLY_2_0_OFFSET)

#define DQS_IN_DLY_2_1_OFFSET		0x45c
#define DDR_PHY_DQS_IN_DLY_2_1		(DDR_PHY_BASE + DQS_IN_DLY_2_1_OFFSET)

#define DQS_IN_DLY_2_2_OFFSET		0x658
#define DDR_PHY_DQS_IN_DLY_2_2		(DDR_PHY_BASE + DQS_IN_DLY_2_2_OFFSET)

#define DQS_IN_DLY_2_3_OFFSET		0x65c
#define DDR_PHY_DQS_IN_DLY_2_3		(DDR_PHY_BASE + DQS_IN_DLY_2_3_OFFSET)

#define DQS_IN_DLY_3_0_OFFSET		0x460
#define DDR_PHY_DQS_IN_DLY_3_0		(DDR_PHY_BASE + DQS_IN_DLY_3_0_OFFSET)

#define DQS_IN_DLY_3_1_OFFSET		0x464
#define DDR_PHY_DQS_IN_DLY_3_1		(DDR_PHY_BASE + DQS_IN_DLY_3_1_OFFSET)

#define DQS_IN_DLY_3_2_OFFSET		0x660
#define DDR_PHY_DQS_IN_DLY_3_2		(DDR_PHY_BASE + DQS_IN_DLY_3_2_OFFSET)

#define DQS_IN_DLY_3_3_OFFSET		0x664
#define DDR_PHY_DQS_IN_DLY_3_3		(DDR_PHY_BASE + DQS_IN_DLY_3_3_OFFSET)

#define PAD_BUS_2_OFFSET		0x1FC
#define DDR_PHY_PAD_BUS_2		(DDR_PHY_BASE + PAD_BUS_2_OFFSET)
#define PAD_BUS_2_VALUE			0x8e8e8e8e

#define DQS_OCD_SEL0_OFFSET		0x1E0
#define DDR_PHY_DQS_OCD_SEL0		(DDR_PHY_BASE + DQS_OCD_SEL0_OFFSET)
#define DQS_OCD_SEL_VALUE		0x00000000
#define DQS_OCD_SEL_MASK		0x77777777

#define DQS_OCD_SEL1_OFFSET		0x1E4
#define DDR_PHY_DQS_OCD_SEL1		(DDR_PHY_BASE + DQS_OCD_SEL1_OFFSET)

#define CKE_OCD_SEL_OFFSET		0x1E8
#define DDR_PHY_CKE_OCD_SEL		(DDR_PHY_BASE + CKE_OCD_SEL_OFFSET)
#define CKE_OCD_SEL_VALUE		0x00001111
#define CKE_OCD_SEL_MASK		0x00007777

#define ADR_OCD_SEL_OFFSET		0x1EC
#define DDR_PHY_ADR_OCD_SEL		(DDR_PHY_BASE + ADR_OCD_SEL_OFFSET)
#define ADR_OCD_SEL_VALUE		0x00001111
#define ADR_OCD_SEL_MASK		0x00007700

#define CK_OCD_SEL_OFFSET		0x1F0
#define DDR_PHY_CK_OCD_SEL		(DDR_PHY_BASE + CK_OCD_SEL_OFFSET)
#define CK_OCD_SEL_VALUE		0x00000000
#define CK_OCD_SEL_MASK			0x77777777

#define PAD_BUS_0_OFFSET		0x1F4
#define DDR_PHY_PAD_BUS_0		(DDR_PHY_BASE + PAD_BUS_0_OFFSET)
#define DDR_PHY_PAD_BUS_0_DEF			0x89580000
#define EN_DCC							0x80000000
#define EN_DCC_MASK			0x80000000
#define PD_CK				0x40000000
#define PD_CK_MASK			0x40000000
#define VREF_SEL						0x10000000
#define VREF_RANGE						0x08000000
#define VREF_S							(0x32<<21)
#define VREF_CURRENT_SEL				(0x03<<19)
#define VREF_PD							(0 << 18)
#define IBIAS_SEL						(0x03 << 16)
#define DDR_PHY_PAD_BUS_0_VALUE		(EN_DCC | VREF_SEL | VREF_RANGE | \
					VREF_S | VREF_CURRENT_SEL | \
					VREF_PD | IBIAS_SEL)
#define DDR_PHY_PAD_BUS_0_MASK		0xffff0000

#define PAD_BUS_1_OFFSET		0x1F8
#define DDR_PHY_PAD_BUS_1		(DDR_PHY_BASE + PAD_BUS_1_OFFSET)
#define ZQ_POWER_DOWN			0x40
#define ZQ_POWER_DOWN_MASK		0x40

#define ODT_TTCP0_SET0_OFFSET		0x180
#define DDR_PHY_ODT_TTCP0_SET0		(DDR_PHY_BASE + ODT_TTCP0_SET0_OFFSET)

#define OCD_TTCP1_SET0_OFFSET		0x184
#define DDR_PHY_ODT_TTCP1_SET0		(DDR_PHY_BASE + OCD_TTCP1_SET0_OFFSET)

#define ODT_TTCN0_SET0_OFFSET		0x188
#define DDR_PHY_ODT_TTCN0_SET0		(DDR_PHY_BASE + ODT_TTCN0_SET0_OFFSET)

#define ODT_TTCN1_SET0_OFFSET		0x18C
#define DDR_PHY_ODT_TTCN1_SET0		(DDR_PHY_BASE + ODT_TTCN1_SET0_OFFSET)

#define OCDP0_SET0_OFFSET		0x190
#define DDR_PHY_OCDP0_SET0		(DDR_PHY_BASE + OCDP0_SET0_OFFSET)

#define OCDP1_SET0_OFFSET		0x194
#define DDR_PHY_OCDP1_SET0		(DDR_PHY_BASE + OCDP1_SET0_OFFSET)

#define OCDN0_SET0_OFFSET		0x198
#define DDR_PHY_OCDN0_SET0		(DDR_PHY_BASE + OCDN0_SET0_OFFSET)

#define OCDN1_SET0_OFFSET		0x19C
#define DDR_PHY_OCDN1_SET0		(DDR_PHY_BASE + OCDN1_SET0_OFFSET)

#define ODT_TTCP0_SET1_OFFSET		0x1A0
#define DDR_PHY_ODT_TTCP0_SET1		(DDR_PHY_BASE + ODT_TTCP0_SET1_OFFSET)

#define ODT_TTCP1_SET1_OFFSET		0x1A4
#define DDR_PHY_ODT_TTCP1_SET1		(DDR_PHY_BASE + ODT_TTCP1_SET1_OFFSET)

#define ODT_TTCN0_SET1_OFFSET		0x1A8
#define DDR_PHY_ODT_TTCN0_SET1		(DDR_PHY_BASE + ODT_TTCN0_SET1_OFFSET)

#define ODT_TTCN1_SET1_OFFSET		0x1AC
#define DDR_PHY_ODT_TTCN1_SET1		(DDR_PHY_BASE + ODT_TTCN1_SET1_OFFSET)

#define OCDP0_SET1_OFFSET			0x180
#define DDR_PHY_OCDP0_SET1		(DDR_PHY_BASE + OCDP0_SET1_OFFSET)

#define OCDP1_SET1_OFFSET			0x1B4
#define DDR_PHY_OCDP1_SET1		(DDR_PHY_BASE + OCDP1_SET1_OFFSET)

#define OCDN0_SET1_OFFSET			0x1B8
#define DDR_PHY_OCDN0_SET1		(DDR_PHY_BASE + OCDN0_SET1_OFFSET)

#define OCDN1_SET1_OFFSET			0x1BC
#define DDR_PHY_OCDN1_SET1		(DDR_PHY_BASE + OCDN1_SET1_OFFSET)

#define DQ_ODT_SEL0_OFFSET			0x1C0
#define DDR_PHY_DQ_ODT_SEL0		(DDR_PHY_BASE + DQ_ODT_SEL0_OFFSET)
#define DQ_ODT_SEL_MASK			0x00007777

#define DQ_ODT_SEL1_OFFSET			0x1C4
#define DDR_PHY_DQ_ODT_SEL1		(DDR_PHY_BASE + DQ_ODT_SEL1_OFFSET)

#define DPI_CTRL_0_OFFSET			0x238
#define DDR_PHY_DPI_CTRL_0		(DDR_PHY_BASE + DPI_CTRL_0_OFFSET)
#define DDR_PHY_DPI_CTRL_0_DEF	0x003e2004
#define DATA_PRE			0x00200000
#define DATA_PRE_MASK			0x00200000
#define DATA_INIT			0x00000000
#define DATA_INIT_MASK			0x00100000
/*Command output delay:[27:25]*/
#define CMD_PH_SEL_1_CYCLE			0x00
#define CMD_PH_SEL_2_CYCLE			0x2000000
#define CMD_PH_SEL_3_CYCLE			0x4000000
#define CMD_PH_SEL_4_CYCLE			0x6000000
#define CMD_PH_SEL_5_CYCLE			0x8000000
#define CMD_PH_SEL_6_CYCLE			0xa000000
#define CMD_PH_SEL_7_CYCLE			0xc000000
#define CMD_PH_SEL_8_CYCLE			0xe000000
/*Command output delay enable:[24]*/
#define CMD_PH_EN				0x1000000
/*Update delay chain setting after write data enable:[23:21]*/
#define WR_UPDATA_DLY				0xE00000
/*Update delay chain setting after read data enable:[20:16]*/
#define RD_UPDATA_DLY				0xF0000
/*Update delay chain setting after receiving refresh command:[13:8]*/
#define REF_UPDATA_DLY				0x2000
/*3-point calibration set mode[5:4]
 *[00] during no read operation
 *[01] during refresh
 *[10] immediately
 *[11] disable auto calibration
 */
#define CAL_SET_MODE				0x30
#define CAL_SET_MODE_MASK			0x30
/*Reset read FIFO pointer mode[3:2]
 *[00] during no read operation
 *[01] during refresh
 *[10] disable reset FIFO pointer
 *[11] force reset FIFO pointer
 */
#define RST_FIFO_MODE				0x08
#define RST_FIFO_MODE_MASK			0x0c
/*FW set delay chain of data slice mode[1:0]
 *[00] Read : during no read
 *	Write: during refresh
 *[01] during refresh
 *[10] immediately
 *[11] reserved
 */
#define DQS_EN_PUPD3				0x40
#define FW_SET_MODE_IMME			0x02
#define FW_SET_MODE_HW			0x00
#define FW_SET_MODE_MASK		0x03
#define DPI_CTRL_0_SETTING	(CMD_PH_SEL_4_CYCLE | CMD_PH_EN | \
		WR_UPDATA_DLY | RD_UPDATA_DLY | REF_UPDATA_DLY | CAL_SET_MODE)
#define CMD_OUTPUT_DLY_2T			0x3ef2000

#define DPI_CTRL_0_CMD_DLY_MASK	0xf0ffffff
#define DPI_CTRL_SET_MODE_MASK	0xfffffffc
#define DPI_CTRL_DQS_EN_MASK		0xffffff3f

#define DPI_CTRL_1_OFFSET			0x23C
#define DDR_PHY_DPI_CTRL_1		(DDR_PHY_BASE + DPI_CTRL_1_OFFSET)
/*Enable of the following row:[3]*/
#define WRITE_EN_1				0x08
#define WRITE_EN_0				0x02
#define WRITE_EN_2				0x20
#define WRITE_EN_3				0x80
#define FW_SET_RD_DET			0x40
#define FW_SET_CAL_RANGE		0x10
#define SET_REG_VALID 			0x40000000
#define WRITE_EN_4				0x80000000
#define SET_REG_VALID_MASK		0xc0000000
/*FW set read delay chain of data slice
 * (one cycle pulse, HW auto clear) : [2]
 */
#define FW_SET_RD_DLY				0x04
#define FW_SET_WR_DLY				0x01
#define DPI_CTRL_1_SET_RD_DLY			(WRITE_EN_1 | FW_SET_RD_DLY)
#define DPI_CTRL_1_SET_WR_DLY			(WRITE_EN_0 | FW_SET_WR_DLY)
#define FW_SET_RD_DLY_MASK		0x0000000c
#define FW_SET_WR_DLY_MASK		0x00000003

#define DPI_CTRL_2_OFFSET		0x240
#define DDR_PHY_DPI_CTRL_2		(DDR_PHY_BASE + DPI_CTRL_2_OFFSET)
#define DDR_PHY_DPI_CTRL_2_DEF		0x80161f00
#define SET_REG_VALID_MODE		0x10000000
#define SET_REG_VALID_MODE_MASK		0x10000000
#define FORCE_RESET_3POINT		0x04000000
#define FORCE_RESET_3POINT_MASK		0x0c000000

#define PAD_RZCTRL_STATUS_OFFSET	0x14C
#define DDR_PHY_PAD_RZCTRL_STATUS (DDR_PHY_BASE + PAD_RZCTRL_STATUS_OFFSET)
#define RZQ_CAL_DONE					0x01

#define BIST_2TO1_0_OFFSET			0x244
#define DDR_PHY_BIST_2TO1_0		(DDR_PHY_BASE + BIST_2TO1_0_OFFSET)
/*For address and command slice:[31]
 *1 '1:2 frequency ratio between PHY & MC
 *0 '1:1 frequency ratio between PHY & MC
 */
#define ADDR_CMD_RATIO_2to1			0x80000000
/*For data slice:[28]
 *1 '1:2 frequency ratio between PHY & MC
 *0 '1:1 frequency ratio between PHY & MC
 */
#define DATA_TATIO_2to1				0x10000000
#define DFI_RATIO_SETTING	(ADDR_CMD_RATIO_2to1 | DATA_TATIO_2to1)
#define ADDR_CMD_RATIO_2to1_MASK		0x6fffffff
#define RD_CMD_DLY	0x40000000
#define WR_CMD_DLY 0x20000000

#define CMD_ADR_PH_OFFSET			0x154
#define DDR_PHY_CMD_ADR_PH	(DDR_PHY_BASE + CMD_ADR_PH_OFFSET)
#define CMD_ADR_PH_EN					0x7c000000
#define CMD_ADR_PH_DIS					0
#define CMD_ADR_PH_MASK		0x7c077777

#define READ_CRTL_6_0_OFFSET			0x418
#define DDR_PHY_READ_CTRL_6_0	(DDR_PHY_BASE + READ_CRTL_6_0_OFFSET)

#define READ_CRTL_6_1_OFFSET			0x41C
#define DDR_PHY_READ_CTRL_6_1	(DDR_PHY_BASE + READ_CRTL_6_1_OFFSET)

#define READ_CRTL_6_2_OFFSET			0x618
#define DDR_PHY_READ_CTRL_6_2	(DDR_PHY_BASE + READ_CRTL_6_2_OFFSET)

#define READ_CRTL_6_3_OFFSET			0x61C
#define DDR_PHY_READ_CTRL_6_3	(DDR_PHY_BASE + READ_CRTL_6_3_OFFSET)

#define READ_CRTL_2_0_OFFSET			0x408
#define DDR_PHY_READ_CTRL_2_0		(DDR_PHY_BASE + READ_CRTL_2_0_OFFSET)
#define DQ_DQS_ODT_FORCE_CTRL_VALUE	0x00f00000
#define DQ_DQS_ODT_FORCE_CTRL_MASK	0x00f00000

#define READ_CRTL_2_1_OFFSET			0x40C
#define DDR_PHY_READ_CTRL_2_1		(DDR_PHY_BASE + READ_CRTL_2_1_OFFSET)

#define READ_CRTL_2_2_OFFSET			0x608
#define DDR_PHY_READ_CTRL_2_2		(DDR_PHY_BASE + READ_CRTL_2_2_OFFSET)

#define READ_CRTL_2_3_OFFSET			0x60C
#define DDR_PHY_READ_CTRL_2_3		(DDR_PHY_BASE + READ_CRTL_2_3_OFFSET)
#define ODT_FORCE_SEL					0x800000
#define ODT_FORCE_SIG					0x400000
#define TM_ODT_EN_ODD					0x08

#define PAD_CTRL_ZPRPG0_OFFSET		0x2E8
#define DDR_PHY_PAD_CTRL_ZPROG0		(DDR_PHY_BASE + PAD_CTRL_ZPRPG0_OFFSET)
#define CTRL_ZPROG_MASK			0x073f073f

#define PAD_CTRL_ZPRPG1_OFFSET		0x2EC
#define DDR_PHY_PAD_CTRL_ZPROG1		(DDR_PHY_BASE + PAD_CTRL_ZPRPG1_OFFSET)

#define PAD_CTRL_ZPRPG2_OFFSET		0x2F0
#define DDR_PHY_PAD_CTRL_ZPROG2		(DDR_PHY_BASE + PAD_CTRL_ZPRPG2_OFFSET)

#define PAD_NOCD2_ZPRPG0_OFFSET		0x308
#define DDR_PHY_PAD_NOCD2_ZPROG0 (DDR_PHY_BASE + PAD_NOCD2_ZPRPG0_OFFSET)
#define NOCD2_ZPROG_MASK		0x0000073f

#define PAD_NOCD2_ZPRPG1_OFFSET		0x30C
#define DDR_PHY_PAD_NOCD2_ZPROG1 (DDR_PHY_BASE + PAD_NOCD2_ZPRPG1_OFFSET)

#define PAD_NOCD2_ZPRPG2_OFFSET		0x310
#define DDR_PHY_PAD_NOCD2_ZPROG2 (DDR_PHY_BASE + PAD_NOCD2_ZPRPG2_OFFSET)

#define DQ_OCD_SEL0_OFFSET		0x1C8
#define DDR_PHY_DQ_OCD_SEL0	(DDR_PHY_BASE + DQ_OCD_SEL0_OFFSET)
#define DQ_OCD_SEL_MASK		0x00007777

#define DQ_OCD_SEL1_OFFSET		0x1CC
#define DDR_PHY_DQ_OCD_SEL1	(DDR_PHY_BASE + DQ_OCD_SEL1_OFFSET)

#define DQS_P_ODT_SEL_0_OFFSET		0x1D0
#define DDR_PHY_DQS_P_ODT_SEL_0		(DDR_PHY_BASE + DQS_P_ODT_SEL_0_OFFSET)
#define DQS_P_ODT_SEL_VALUE		0x00000011
#define DQS_P_ODT_SEL_MASK		0x00007777

#define DQS_P_ODT_SEL_1_OFFSET		0x1D4
#define DDR_PHY_DQS_P_ODT_SEL_1		(DDR_PHY_BASE + DQS_P_ODT_SEL_1_OFFSET)

#define DQS_N_ODT_SEL_0_OFFSET		0x1D8
#define DDR_PHY_DQS_N_ODT_SEL_0		(DDR_PHY_BASE + DQS_N_ODT_SEL_0_OFFSET)
#define DQS_N_ODT_SEL_VALUE		0x00002200
#define DQS_N_ODT_SEL_MASK		0x00007777

#define DQS_N_ODT_SEL_1_OFFSET		0x1DC
#define DDR_PHY_DQS_N_ODT_SEL_1		(DDR_PHY_BASE + DQS_N_ODT_SEL_1_OFFSET)

#define ODT_SEL_0_OFFSET			0x228
#define DDR_PHY_ODT_SEL_0	(DDR_PHY_BASE + ODT_SEL_0_OFFSET)
#define ODT_SEL_VALUE		0x77
#define ODT_SEL_MASK		0x77

#define CKE_ODT_SEL_OFFSET	0x22C
#define DDR_PHY_CKE_ODT_SEL	(DDR_PHY_BASE + CKE_ODT_SEL_OFFSET)
#define CKE_ODT_SEL_VALUE	0x7777
#define CKE_ODT_SEL_MASK	0x7777

#define ADR_ODT_SEL_OFFSET			0x230
#define DDR_PHY_ADR_ODT_SEL	(DDR_PHY_BASE + ADR_ODT_SEL_OFFSET)
#define ADR_ODT_SEL_VALUE	0x77007777
#define ADR_ODT_SEL_MASK	0x77007777

#define CLK_ODT_SEL_OFFSET			0x234
#define DDR_PHY_CLK_ODT_SEL	(DDR_PHY_BASE + CLK_ODT_SEL_OFFSET)
#define CLK_ODT_SEL_VALUE	0x7777
#define CLK_ODT_SEL_MASK	0x7777

#define DQ_OCD_SEL_2_0_OFFSET			0x328
#define DDR_PHY_DQ_OCD_SEL_2_0	(DDR_PHY_BASE + DQ_OCD_SEL_2_0_OFFSET)
#define DQ_OCD_SEL_2_0_VALUE	0x00000000
#define DQ_OCD_SEL_2_0_MASK	0x77777777

#define DQ_OCD_SEL_2_1_OFFSET			0x32C
#define DDR_PHY_DQ_OCD_SEL_2_1	(DDR_PHY_BASE + DQ_OCD_SEL_2_1_OFFSET)
#define DQ_OCD_SEL_2_1_VALUE	0x77777777
#define DQ_OCD_SEL_2_1_MASK	0x77777777

#define DQS_OCD_SEL_2_0_OFFSET			0x330
#define DDR_PHY_DQS_OCD_SEL_2_0 (DDR_PHY_BASE + DQS_OCD_SEL_2_0_OFFSET)
#define DQS_OCD_SEL_2_0_VALUE	0x00000000
#define DQS_OCD_SEL_2_0_MASK	0x77777777

#define DQS_OCD_SEL_2_1_OFFSET			0x334
#define DDR_PHY_DQS_OCD_SEL_2_1 (DDR_PHY_BASE + DQS_OCD_SEL_2_1_OFFSET)
#define DQS_OCD_SEL_2_1_VALUE	0x77777777
#define DQS_OCD_SEL_2_1_MASK	0x77777777

#define RW_SEL_EMPH_CTL_0_OFFSET		0x4f0
#define DDR_PHY_RW_SEL_EMPH_CTL_0 (DDR_PHY_BASE + RW_SEL_EMPH_CTL_0_OFFSET)
#define RW_SEL_EMPH_CTL_0_VALUE	0x00110011
#define RW_SEL_EMPH_CTL_0_MASK	0x00ff00ff

#define RW_SEL_EMPH_CTL_1_OFFSET		0x4f4
#define DDR_PHY_RW_SEL_EMPH_CTL_1 (DDR_PHY_BASE + RW_SEL_EMPH_CTL_1_OFFSET)
#define RW_SEL_EMPH_CTL_1_VALUE	0x00110011
#define RW_SEL_EMPH_CTL_1_MASK	0x00ff00ff

#define RW_SEL_EMPH_CTL_2_OFFSET		0x6f0
#define DDR_PHY_RW_SEL_EMPH_CTL_2 (DDR_PHY_BASE + RW_SEL_EMPH_CTL_2_OFFSET)
#define RW_SEL_EMPH_CTL_2_VALUE	0x00110011
#define RW_SEL_EMPH_CTL_2_MASK	0x00ff00ff

#define RW_SEL_EMPH_CTL_3_OFFSET		0x6f4
#define DDR_PHY_RW_SEL_EMPH_CTL_3 (DDR_PHY_BASE + RW_SEL_EMPH_CTL_3_OFFSET)
#define RW_SEL_EMPH_CTL_3_VALUE	0x00110011
#define RW_SEL_EMPH_CTL_3_MASK	0x00ff00ff

#define RW_EMPH_CTL_0_OFFSET			0x4e8
#define DDR_PHY_RW_EMPH_CTL_0	(DDR_PHY_BASE + RW_EMPH_CTL_0_OFFSET)
#define RW_EMPH_CTL_0_VALUE	0x00000000
#define RW_EMPH_CTL_0_MASK	0xffffffff

#define RW_EMPH_CTL_1_OFFSET			0x4ec
#define DDR_PHY_RW_EMPH_CTL_1	(DDR_PHY_BASE + RW_EMPH_CTL_1_OFFSET)
#define RW_EMPH_CTL_1_VALUE	0x00000000
#define RW_EMPH_CTL_1_MASK	0xffffffff

#define RW_EMPH_CTL_2_OFFSET			0x6e8
#define DDR_PHY_RW_EMPH_CTL_2	(DDR_PHY_BASE + RW_EMPH_CTL_2_OFFSET)
#define RW_EMPH_CTL_2_VALUE	0x00000000
#define RW_EMPH_CTL_2_MASK	0xffffffff

#define RW_EMPH_CTL_3_OFFSET			0x6ec
#define DDR_PHY_RW_EMPH_CTL_3	(DDR_PHY_BASE + RW_EMPH_CTL_3_OFFSET)
#define RW_EMPH_CTL_3_VALUE	0x00000000
#define RW_EMPH_CTL_3_MASK	0xffffffff

#define DT_SET0_OFFSET				0x17c
#define DDR_PHY_DT_SET0		(DDR_PHY_BASE + DT_SET0_OFFSET)
#define DT_SET0_VALUE		0x0030f0f0

#define READ_DEBUG_CTRL_0_OFFSET		0x4e8
#define DDR_PHY_READ_DEBUG_CTRL_0 (DDR_PHY_BASE + READ_DEBUG_CTRL_0_OFFSET)

#define READ_DEBUG_CTRL_1_OFFSET		0x4ec
#define DDR_PHY_READ_DEBUG_CTRL_1 (DDR_PHY_BASE + READ_DEBUG_CTRL_1_OFFSET)

#define READ_DEBUG_CTRL_2_OFFSET		0x6e8
#define DDR_PHY_READ_DEBUG_CTRL_2 (DDR_PHY_BASE + READ_DEBUG_CTRL_2_OFFSET)

#define READ_DEBUG_CTRL_3_OFFSET		0x6ec
#define DDR_PHY_READ_DEBUG_CTRL_3 (DDR_PHY_BASE + READ_DEBUG_CTRL_3_OFFSET)

#ifdef CONFIG_DDR_H5TQ2G_200M
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE					0x0E

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0

#define DDR_PHY_PLL_CTL0_VALUE		0x1000

#define DDR_PHY_PLL_CTL3_VALUE	0x25105134

#define DDR_PHY_DQS_IN_DLY_0_0_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_0_1_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_0_2_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_0_3_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_1_0_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_1_1_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_1_2_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_1_3_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_2_0_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_2_1_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_2_2_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_2_3_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_3_0_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_3_1_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_3_2_VALUE		0x14141414
#define DDR_PHY_DQS_IN_DLY_3_3_VALUE		0x14141414

#define DDR_PHY_PLL_CTL_VALUE	0x40

#define DDR_PHY_PLL_PI0_VALUE			0x0000000a

#define DDR_PHY_READ_CTRL_5_VALUE	0x00c03fff

#define ZPROG_VALUE0				0x243a
#define ZPROG_VALUE1				0x43a

#define READ_CTRL_1_VALUE				0x1000a

#define TM_RD_FIFO			TM_DQS_EN_6_CYCLE

#define CS_OUTPUT_DLY_2T			0x20200ff0

#define MCK_EN_SETTING		(DPI_POST_PI_CL | MCK_DQS1_EN | MCK_DQS0_EN | \
				MCK_CMD_EN | MCK_CLK0_EN | DPI_POST_PI_EN)
#endif

#ifdef DDR3_1GBIT_DEMOBOARD

#ifdef CONFIG_RTS3917_QFN88_DDR3_1GBIT
#define CONFIG_CLK_GATING

#ifdef CONFIG_RTS3917_DDR_2133
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x282727
/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x05470547

#define DDR_PHY_PLL_CTL3_VALUE			0x04c05577
#endif

#ifdef CONFIG_RTS3917_DDR_1866
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x282222

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE			0x02c05477
#endif

#ifdef CONFIG_RTS3917_DDR_1600

/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281d1d

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0

#define DDR_PHY_PLL_CTL3_VALUE			0x02c05477
#endif

#ifdef CONFIG_RTS3917_DDR_1333
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281717

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x05470547

#define DDR_PHY_PLL_CTL3_VALUE			0x00c05354
#endif

#ifdef CONFIG_RTS3917_DDR_1066
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281212

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE			0x06405254
#endif

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x0c
#define CMD_PI					0
#define DQS0_PI					0x10
#define DQS1_PI					0x11
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x15
#define CS1_PI					0x00
#define DQ_PI					0x17

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			1
#define DQS1_OESYNC			1
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_EN
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x03
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x04

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x07
#define RX_DQ1_P_DLY				0x07
#define RX_DQ2_P_DLY				0x07
#define RX_DQ3_P_DLY				0x07
#define RX_DQ4_P_DLY				0x07
#define RX_DQ5_P_DLY				0x07
#define RX_DQ6_P_DLY				0x07
#define RX_DQ7_P_DLY				0x07

#define RX_DQ8_P_DLY				0x07
#define RX_DQ9_P_DLY				0x07
#define RX_DQ10_P_DLY				0x07
#define RX_DQ11_P_DLY				0x07
#define RX_DQ12_P_DLY				0x07
#define RX_DQ13_P_DLY				0x07
#define RX_DQ14_P_DLY				0x07
#define RX_DQ15_P_DLY				0x07

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x07
#define RX_DQ1_N_DLY				0x07
#define RX_DQ2_N_DLY				0x07
#define RX_DQ3_N_DLY				0x07
#define RX_DQ4_N_DLY				0x07
#define RX_DQ5_N_DLY				0x07
#define RX_DQ6_N_DLY				0x07
#define RX_DQ7_N_DLY				0x07

#define RX_DQ8_N_DLY				0x07
#define RX_DQ9_N_DLY				0x07
#define RX_DQ10_N_DLY				0x07
#define RX_DQ11_N_DLY				0x07
#define RX_DQ12_N_DLY				0x07
#define RX_DQ13_N_DLY				0x07
#define RX_DQ14_N_DLY				0x07
#define RX_DQ15_N_DLY				0x07

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL			TM_ODT_EN_ODD

#else

#ifdef CONFIG_RTS3917_DDR_1066
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE					0x281212

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE	0x06405254

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x07
#define CMD_PI					0
#define DQS0_PI					0x0f
#define DQS1_PI					0x0f
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x12
#define CS1_PI					0x00
#define DQ_PI					0x13

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			0
#define DQS1_OESYNC			0
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_EN
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x01
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x03

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x08
#define RX_DQ1_P_DLY				0x08
#define RX_DQ2_P_DLY				0x08
#define RX_DQ3_P_DLY				0x08
#define RX_DQ4_P_DLY				0x08
#define RX_DQ5_P_DLY				0x08
#define RX_DQ6_P_DLY				0x08
#define RX_DQ7_P_DLY				0x08

#define RX_DQ8_P_DLY				0x08
#define RX_DQ9_P_DLY				0x08
#define RX_DQ10_P_DLY				0x08
#define RX_DQ11_P_DLY				0x08
#define RX_DQ12_P_DLY				0x08
#define RX_DQ13_P_DLY				0x08
#define RX_DQ14_P_DLY				0x08
#define RX_DQ15_P_DLY				0x08

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x08
#define RX_DQ1_N_DLY				0x08
#define RX_DQ2_N_DLY				0x08
#define RX_DQ3_N_DLY				0x08
#define RX_DQ4_N_DLY				0x08
#define RX_DQ5_N_DLY				0x08
#define RX_DQ6_N_DLY				0x08
#define RX_DQ7_N_DLY				0x08

#define RX_DQ8_N_DLY				0x08
#define RX_DQ9_N_DLY				0x08
#define RX_DQ10_N_DLY				0x08
#define RX_DQ11_N_DLY				0x08
#define RX_DQ12_N_DLY				0x08
#define RX_DQ13_N_DLY				0x08
#define RX_DQ14_N_DLY				0x08
#define RX_DQ15_N_DLY				0x08

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL			TM_ODT_EN_ODD

#endif

#ifdef CONFIG_RTS3917_DDR_1333
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE					0x281717

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x05470547

#define DDR_PHY_PLL_CTL3_VALUE	0x00c05354

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x07
#define CMD_PI					0
#define DQS0_PI					0x0f
#define DQS1_PI					0x0f
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x12
#define CS1_PI					0x00
#define DQ_PI					0x13

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			0
#define DQS1_OESYNC			0
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_DIS
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x04
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x05

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x08
#define RX_DQ1_P_DLY				0x08
#define RX_DQ2_P_DLY				0x08
#define RX_DQ3_P_DLY				0x08
#define RX_DQ4_P_DLY				0x08
#define RX_DQ5_P_DLY				0x08
#define RX_DQ6_P_DLY				0x08
#define RX_DQ7_P_DLY				0x08

#define RX_DQ8_P_DLY				0x08
#define RX_DQ9_P_DLY				0x08
#define RX_DQ10_P_DLY				0x08
#define RX_DQ11_P_DLY				0x08
#define RX_DQ12_P_DLY				0x08
#define RX_DQ13_P_DLY				0x08
#define RX_DQ14_P_DLY				0x08
#define RX_DQ15_P_DLY				0x08

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x08
#define RX_DQ1_N_DLY				0x08
#define RX_DQ2_N_DLY				0x08
#define RX_DQ3_N_DLY				0x08
#define RX_DQ4_N_DLY				0x08
#define RX_DQ5_N_DLY				0x08
#define RX_DQ6_N_DLY				0x08
#define RX_DQ7_N_DLY				0x08

#define RX_DQ8_N_DLY				0x08
#define RX_DQ9_N_DLY				0x08
#define RX_DQ10_N_DLY				0x08
#define RX_DQ11_N_DLY				0x08
#define RX_DQ12_N_DLY				0x08
#define RX_DQ13_N_DLY				0x08
#define RX_DQ14_N_DLY				0x08
#define RX_DQ15_N_DLY				0x08

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL			TM_ODT_EN_ODD

#endif

#ifdef CONFIG_RTS3917_DDR_1600

/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE					0x281d1d

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0

#define DDR_PHY_PLL_CTL3_VALUE	0x02c05477

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x07
#define CMD_PI					0
#define DQS0_PI					0x0f
#define DQS1_PI					0x0f
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x12
#define CS1_PI					0x00
#define DQ_PI					0x13

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			0
#define DQS1_OESYNC			0
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_DIS
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x04
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x05

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x08
#define RX_DQ1_P_DLY				0x08
#define RX_DQ2_P_DLY				0x08
#define RX_DQ3_P_DLY				0x08
#define RX_DQ4_P_DLY				0x08
#define RX_DQ5_P_DLY				0x08
#define RX_DQ6_P_DLY				0x08
#define RX_DQ7_P_DLY				0x08

#define RX_DQ8_P_DLY				0x08
#define RX_DQ9_P_DLY				0x08
#define RX_DQ10_P_DLY				0x08
#define RX_DQ11_P_DLY				0x08
#define RX_DQ12_P_DLY				0x08
#define RX_DQ13_P_DLY				0x08
#define RX_DQ14_P_DLY				0x08
#define RX_DQ15_P_DLY				0x08

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x08
#define RX_DQ1_N_DLY				0x08
#define RX_DQ2_N_DLY				0x08
#define RX_DQ3_N_DLY				0x08
#define RX_DQ4_N_DLY				0x08
#define RX_DQ5_N_DLY				0x08
#define RX_DQ6_N_DLY				0x08
#define RX_DQ7_N_DLY				0x08

#define RX_DQ8_N_DLY				0x08
#define RX_DQ9_N_DLY				0x08
#define RX_DQ10_N_DLY				0x08
#define RX_DQ11_N_DLY				0x08
#define RX_DQ12_N_DLY				0x08
#define RX_DQ13_N_DLY				0x08
#define RX_DQ14_N_DLY				0x08
#define RX_DQ15_N_DLY				0x08

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL	TM_ODT_EN_ODD
#endif

#ifdef CONFIG_RTS3917_DDR_1866

/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE			0x282222

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE			0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE		0x02c05477

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x07
#define CMD_PI					0
#define DQS0_PI					0x0f
#define DQS1_PI					0x0f
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x12
#define CS1_PI					0x00
#define DQ_PI					0x13

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			0
#define DQS1_OESYNC			0
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_DIS
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x04
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x05

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x08
#define RX_DQ1_P_DLY				0x08
#define RX_DQ2_P_DLY				0x08
#define RX_DQ3_P_DLY				0x08
#define RX_DQ4_P_DLY				0x08
#define RX_DQ5_P_DLY				0x08
#define RX_DQ6_P_DLY				0x08
#define RX_DQ7_P_DLY				0x08

#define RX_DQ8_P_DLY				0x08
#define RX_DQ9_P_DLY				0x08
#define RX_DQ10_P_DLY				0x08
#define RX_DQ11_P_DLY				0x08
#define RX_DQ12_P_DLY				0x08
#define RX_DQ13_P_DLY				0x08
#define RX_DQ14_P_DLY				0x08
#define RX_DQ15_P_DLY				0x08

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x08
#define RX_DQ1_N_DLY				0x08
#define RX_DQ2_N_DLY				0x08
#define RX_DQ3_N_DLY				0x08
#define RX_DQ4_N_DLY				0x08
#define RX_DQ5_N_DLY				0x08
#define RX_DQ6_N_DLY				0x08
#define RX_DQ7_N_DLY				0x08

#define RX_DQ8_N_DLY				0x08
#define RX_DQ9_N_DLY				0x08
#define RX_DQ10_N_DLY				0x08
#define RX_DQ11_N_DLY				0x08
#define RX_DQ12_N_DLY				0x08
#define RX_DQ13_N_DLY				0x08
#define RX_DQ14_N_DLY				0x08
#define RX_DQ15_N_DLY				0x08

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL	TM_ODT_EN_ODD
#endif

#ifdef CONFIG_RTS3917_DDR_2133

/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE					0x282727

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x05470547

#define DDR_PHY_PLL_CTL3_VALUE	0x04c05577

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x08
#define CMD_PI					0
#define DQS0_PI					0x0d
#define DQS1_PI					0x0d
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x14
#define CS1_PI					0x00
#define DQ_PI					0x11

#define CLK_OESYNC				0x0
#define DQS0_OESYNC			0
#define DQS1_OESYNC			0
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_EN
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x04
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x05

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x07
#define RX_DQ1_P_DLY				0x07
#define RX_DQ2_P_DLY				0x07
#define RX_DQ3_P_DLY				0x07
#define RX_DQ4_P_DLY				0x07
#define RX_DQ5_P_DLY				0x07
#define RX_DQ6_P_DLY				0x07
#define RX_DQ7_P_DLY				0x07

#define RX_DQ8_P_DLY				0x07
#define RX_DQ9_P_DLY				0x07
#define RX_DQ10_P_DLY				0x07
#define RX_DQ11_P_DLY				0x07
#define RX_DQ12_P_DLY				0x07
#define RX_DQ13_P_DLY				0x07
#define RX_DQ14_P_DLY				0x07
#define RX_DQ15_P_DLY				0x07

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x07
#define RX_DQ1_N_DLY				0x07
#define RX_DQ2_N_DLY				0x07
#define RX_DQ3_N_DLY				0x07
#define RX_DQ4_N_DLY				0x07
#define RX_DQ5_N_DLY				0x07
#define RX_DQ6_N_DLY				0x07
#define RX_DQ7_N_DLY				0x07

#define RX_DQ8_N_DLY				0x07
#define RX_DQ9_N_DLY				0x07
#define RX_DQ10_N_DLY				0x07
#define RX_DQ11_N_DLY				0x07
#define RX_DQ12_N_DLY				0x07
#define RX_DQ13_N_DLY				0x07
#define RX_DQ14_N_DLY				0x07
#define RX_DQ15_N_DLY				0x07

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL	TM_ODT_EN_ODD
#endif
#endif
#endif

#ifdef DDR2_MCM
#ifdef CONFIG_RTS3917_QFN88_DDR2_512MBIT

#ifdef CONFIG_RTS3917_DDR_1200
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281515

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x0

#define DDR_PHY_PLL_CTL3_VALUE			0x00c05355
#endif

#ifdef CONFIG_RTS3917_DDR_1150
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281414

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x0

#define DDR_PHY_PLL_CTL3_VALUE			0x00c05355
#endif

#ifdef CONFIG_RTS3917_DDR_1066
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281212

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE			0x06405254
#endif

#ifdef CONFIG_RTS3917_DDR_800
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x280d0d

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0

#define DDR_PHY_PLL_CTL3_VALUE			0x04405154
#endif

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x12
#define CMD_PI					0
#define DQS0_PI					0x14
#define DQS1_PI					0x14
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x16
#define CS1_PI					0x00
#define DQ_PI					0x1c

#define CLK_OESYNC				0x1
#define DQS0_OESYNC			1
#define DQS1_OESYNC			1
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_EN
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x01
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x03

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x0d
#define RX_DQ1_P_DLY				0x0d
#define RX_DQ2_P_DLY				0x0d
#define RX_DQ3_P_DLY				0x0d
#define RX_DQ4_P_DLY				0x0d
#define RX_DQ5_P_DLY				0x0d
#define RX_DQ6_P_DLY				0x0d
#define RX_DQ7_P_DLY				0x0d

#define RX_DQ8_P_DLY				0x0d
#define RX_DQ9_P_DLY				0x0d
#define RX_DQ10_P_DLY				0x0d
#define RX_DQ11_P_DLY				0x0d
#define RX_DQ12_P_DLY				0x0d
#define RX_DQ13_P_DLY				0x0d
#define RX_DQ14_P_DLY				0x0d
#define RX_DQ15_P_DLY				0x0d

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x0d
#define RX_DQ1_N_DLY				0x0d
#define RX_DQ2_N_DLY				0x0d
#define RX_DQ3_N_DLY				0x0d
#define RX_DQ4_N_DLY				0x0d
#define RX_DQ5_N_DLY				0x0d
#define RX_DQ6_N_DLY				0x0d
#define RX_DQ7_N_DLY				0x0d

#define RX_DQ8_N_DLY				0x0d
#define RX_DQ9_N_DLY				0x0d
#define RX_DQ10_N_DLY				0x0d
#define RX_DQ11_N_DLY				0x0d
#define RX_DQ12_N_DLY				0x0d
#define RX_DQ13_N_DLY				0x0d
#define RX_DQ14_N_DLY				0x0d
#define RX_DQ15_N_DLY				0x0d

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0xff
#define DQS1_READ_ODT_RANGE		0xff
#define DQS2_READ_ODT_RANGE		0xff
#define DQS3_READ_ODT_RANGE		0xff
#define ODT_CTRL			TM_ODT_EN_ODD

#else
#ifdef CONFIG_RTS3917_BGA173_DDR2_512MBIT

#ifdef CONFIG_RTS3917_DDR_1066
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x281212

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0x028f028f

#define DDR_PHY_PLL_CTL3_VALUE			0x06405254
#endif

#ifdef CONFIG_RTS3917_DDR_800
/*Integer code of loop divider:[7:0]*/
#define DPI_N_CODE				0x280d0d

/*Fractional code of loop divider:[12:0]*/
#define DPI_F_CODE				0

#define DDR_PHY_PLL_CTL3_VALUE			0x04405154
#endif

#define DCC_CMD_VALUE			0x37
#define DCC_DQS0_VALUE			0x37
#define DCC_DQS1_VALUE			0x37
#define DCC_DQS2_VALUE			0x37
#define DCC_DQS3_VALUE			0x37
#define DCC_DQ0_VALUE			0x37
#define DCC_DQ1_VALUE			0x37
#define DCC_DQ2_VALUE			0x37
#define DCC_DQ3_VALUE			0x37
#define DCC_CK_VALUE			0x37
#define DCC_CS_VALUE			0x37
#define DCC_CS1_VALUE			0x37

#define CLK_PI					0x14
#define CMD_PI					0
#define DQS0_PI					0x15
#define DQS1_PI					0x15
#define DQS2_PI					0x00
#define DQS3_PI					0x00
#define POST_PI					0
#define CS0_PI					0x19
#define CS1_PI					0x00
#define DQ_PI					0x1c

#define CLK_OESYNC				0x1
#define DQS0_OESYNC			1
#define DQS1_OESYNC			1
#define DQS2_OESYNC			0
#define DQS3_OESYNC			0
#define DQ_OESYNC				0x1
#define CS0_OESYNC				0x1
#define CS1_OESYNC				0x0

#define CMD_RD_STR_NUM			0x04
#define CMD_RXT_RD_STR_NUM		0x03
#define CMD_RD_STR_NUM_SEL_H		0x00
#define CMD_RD_STR_NUM_SEL		0x14000000

#define RX_RD_STR_NUM			0x03
#define DDR_PHY_CRT_CTL_VALUE  DDR_PHY_CRT_CTL_DEF

#define AFIFO_STR_0_VALUE		0x33333333

#define DDR_PHY_PLL_CTL0_VALUE	0x1fff1fff

#define CMD_ADR_PH_OPTION		CMD_ADR_PH_EN
#define CMD_ADR_PH_SEL			1

#define VREF_MODE			VREF_DDR3_MODE

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG0_VALUE		0x0014011e

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG0_VALUE	0x11e

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG1_VALUE		0x00100114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG1_VALUE	0x114

/*odt = 240ohm, odt = 34ohm*/
#define PAD_CTRL_ZPROG2_VALUE		0x000a0114

/*odt = 320ohm, odt = 45ohm*/
#define PAD_NOCD2_ZPROG2_VALUE	0x114

#define DQ_ODT_SEL_VALUE		0x00000000
#define DQ_OCD_SEL_VALUE		0x00001111

#define DQS_EN_DLY					0x01
#define DQS_RC1IN_DLY_SEL_0		0x0
#define DQS_RC1IN_DLY_SEL_1		0x0
#define DQS_RC1IN_DLY_SEL_2		0x0
#define DQS_RC1IN_DLY_SEL_3		0x0

#define RX_FIFO_DLY					0x03

#ifdef RXSETTING_PER_BYTE
#define RX_DQ0_P_DLY				0x14
#define RX_DQ0_N_DLY				0x14
#define RX_DQ8_P_DLY				0x14
#define RX_DQ8_N_DLY				0x14
#define RX_DQ16_P_DLY				0x14
#define RX_DQ16_N_DLY				0x14
#define RX_DQ24_P_DLY				0x14
#define RX_DQ24_N_DLY				0x14
#else
#define RX_DQ0_P_DLY				0x0d
#define RX_DQ1_P_DLY				0x0d
#define RX_DQ2_P_DLY				0x0d
#define RX_DQ3_P_DLY				0x0d
#define RX_DQ4_P_DLY				0x0d
#define RX_DQ5_P_DLY				0x0d
#define RX_DQ6_P_DLY				0x0d
#define RX_DQ7_P_DLY				0x0d

#define RX_DQ8_P_DLY				0x0d
#define RX_DQ9_P_DLY				0x0d
#define RX_DQ10_P_DLY				0x0d
#define RX_DQ11_P_DLY				0x0d
#define RX_DQ12_P_DLY				0x0d
#define RX_DQ13_P_DLY				0x0d
#define RX_DQ14_P_DLY				0x0d
#define RX_DQ15_P_DLY				0x0d

#define RX_DQ16_P_DLY				0x09
#define RX_DQ17_P_DLY				0x09
#define RX_DQ18_P_DLY				0x09
#define RX_DQ19_P_DLY				0x09
#define RX_DQ20_P_DLY				0x09
#define RX_DQ21_P_DLY				0x09
#define RX_DQ22_P_DLY				0x09
#define RX_DQ23_P_DLY				0x09

#define RX_DQ24_P_DLY				0x09
#define RX_DQ25_P_DLY				0x09
#define RX_DQ26_P_DLY				0x09
#define RX_DQ27_P_DLY				0x09
#define RX_DQ28_P_DLY				0x09
#define RX_DQ29_P_DLY				0x09
#define RX_DQ30_P_DLY				0x09
#define RX_DQ31_P_DLY				0x09

#define RX_DQ0_N_DLY				0x0d
#define RX_DQ1_N_DLY				0x0d
#define RX_DQ2_N_DLY				0x0d
#define RX_DQ3_N_DLY				0x0d
#define RX_DQ4_N_DLY				0x0d
#define RX_DQ5_N_DLY				0x0d
#define RX_DQ6_N_DLY				0x0d
#define RX_DQ7_N_DLY				0x0d

#define RX_DQ8_N_DLY				0x0d
#define RX_DQ9_N_DLY				0x0d
#define RX_DQ10_N_DLY				0x0d
#define RX_DQ11_N_DLY				0x0d
#define RX_DQ12_N_DLY				0x0d
#define RX_DQ13_N_DLY				0x0d
#define RX_DQ14_N_DLY				0x0d
#define RX_DQ15_N_DLY				0x0d

#define RX_DQ16_N_DLY				0x09
#define RX_DQ17_N_DLY				0x09
#define RX_DQ18_N_DLY				0x09
#define RX_DQ19_N_DLY				0x09
#define RX_DQ20_N_DLY				0x09
#define RX_DQ21_N_DLY				0x09
#define RX_DQ22_N_DLY				0x09
#define RX_DQ23_N_DLY				0x09

#define RX_DQ24_N_DLY				0x09
#define RX_DQ25_N_DLY				0x09
#define RX_DQ26_N_DLY				0x09
#define RX_DQ27_N_DLY				0x09
#define RX_DQ28_N_DLY				0x09
#define RX_DQ29_N_DLY				0x09
#define RX_DQ30_N_DLY				0x09
#define RX_DQ31_N_DLY				0x09
#endif

#define DQS0_READ_ODT_RANGE		0x3c
#define DQS1_READ_ODT_RANGE		0x3c
#define DQS2_READ_ODT_RANGE		0x3c
#define DQS3_READ_ODT_RANGE		0x3c
#define ODT_CTRL			TM_ODT_EN_ODD

#endif

#endif

#endif

#define DDR_PHY_PLL_PI0_VALUE	(CLK_PI | (CMD_PI << 8) | \
			(DQS0_PI << 16) | (DQS1_PI << 24))
#define DDR_PHY_PLL_PI1_VALUE	(DQS2_PI | (DQS3_PI << 8) |(DQ_PI << 16) |\
		(POST_PI << 24))
#define DDR_PHY_PLL_PI2_VALUE	(DQ_PI | (DQ_PI << 8) | (DQ_PI << 16) |\
		(CS0_PI << 24))
#define DDR_PHY_PLL_PI3_VALUE CS1_PI

#define DDR_PHY_POST_PI_EN	(DPI_POST_PI_CL | DPI_POST_PI_EN)

#define OESYNC_VALUE	((CLK_OESYNC << 0) | (DQS0_OESYNC << 2) | \
			(DQS1_OESYNC << 3) | (DQ_OESYNC << 4) | \
			(DQ_OESYNC << 5) | (CS0_OESYNC << 6))

#define DDR_PHY_AFIFO_STR_1_VALUE (AFIFO_STR_1_DEF_VALUE | CMD_RD_STR_NUM | \
				(CMD_RXT_RD_STR_NUM << 4) | \
				(CMD_RD_STR_NUM_SEL_H << 8))
#define DDR_PHY_AFIFO_STR_SEL_VALUE	CMD_RD_STR_NUM_SEL

#define DDR_PHY_READ_CTRL_5_VALUE	0x00c03fff

#define RX_RD_STR_NUM_0 RX_RD_STR_NUM
#define RX_RD_STR_NUM_1 RX_RD_STR_NUM
#define RX_RD_STR_NUM_2 RX_RD_STR_NUM
#define RX_RD_STR_NUM_3 RX_RD_STR_NUM
#define DDR_PHY_AFIFO_STR_2_VALUE (RX_RD_STR_NUM_0 | (RX_RD_STR_NUM_1 << 4) | \
				(RX_RD_STR_NUM_2 << 8) | \
				(RX_RD_STR_NUM_3 << 12))

#define DDR_PHY_READ_CTRL_0_0_VALUE ((DQS_RC1IN_DLY_SEL_0 << 26) | DQS_EN_DLY)
#define DDR_PHY_READ_CTRL_0_1_VALUE ((DQS_RC1IN_DLY_SEL_1 << 26) | DQS_EN_DLY)
#define DDR_PHY_READ_CTRL_0_2_VALUE ((DQS_RC1IN_DLY_SEL_2 << 26) | DQS_EN_DLY)
#define DDR_PHY_READ_CTRL_0_3_VALUE ((DQS_RC1IN_DLY_SEL_3 << 26) | DQS_EN_DLY)

#define CKE_1_PH_SEL			(CMD_ADR_PH_SEL - 1)
#define CKE_PH_SEL			(CMD_ADR_PH_SEL - 1)
#define CS_1_PH_SEL			(CMD_ADR_PH_SEL - 1)
#define CS_PH_SEL			(CMD_ADR_PH_SEL - 1)
#define CMD_PH_SEL			(CMD_ADR_PH_SEL - 1)
#define DDR_PHY_CMD_ADR_PH_VALUE		(CMD_ADR_PH_OPTION | \
	(CKE_1_PH_SEL << 16) | (CKE_PH_SEL << 12) | (CS_1_PH_SEL << 8) | \
	(CS_PH_SEL << 4) | (CMD_PH_SEL))

#define READ_CTRL_1_VALUE		(0x80000000 | RX_FIFO_DLY)

#ifdef CONFIG_CLK_GATING
#define DDR_PHY_PLL_CTL5_VALUE	(DDR_PHY_PLL_CTL5_DEF  | 0x24)
#else
#define DDR_PHY_PLL_CTL5_VALUE	DDR_PHY_PLL_CTL5_DEF
#endif

#ifdef RXSETTING_PER_BYTE
#define RX_DQ1_P_DLY				RX_DQ0_P_DLY
#define RX_DQ2_P_DLY				RX_DQ0_P_DLY
#define RX_DQ3_P_DLY				RX_DQ0_P_DLY
#define RX_DQ4_P_DLY				RX_DQ0_P_DLY
#define RX_DQ5_P_DLY				RX_DQ0_P_DLY
#define RX_DQ6_P_DLY				RX_DQ0_P_DLY
#define RX_DQ7_P_DLY				RX_DQ0_P_DLY

#define RX_DQ9_P_DLY				RX_DQ8_P_DLY
#define RX_DQ10_P_DLY				RX_DQ8_P_DLY
#define RX_DQ11_P_DLY				RX_DQ8_P_DLY
#define RX_DQ12_P_DLY				RX_DQ8_P_DLY
#define RX_DQ13_P_DLY				RX_DQ8_P_DLY
#define RX_DQ14_P_DLY				RX_DQ8_P_DLY
#define RX_DQ15_P_DLY				RX_DQ8_P_DLY

#define RX_DQ17_P_DLY				RX_DQ16_P_DLY
#define RX_DQ18_P_DLY				RX_DQ16_P_DLY
#define RX_DQ19_P_DLY				RX_DQ16_P_DLY
#define RX_DQ20_P_DLY				RX_DQ16_P_DLY
#define RX_DQ21_P_DLY				RX_DQ16_P_DLY
#define RX_DQ22_P_DLY				RX_DQ16_P_DLY
#define RX_DQ23_P_DLY				RX_DQ16_P_DLY

#define RX_DQ25_P_DLY				RX_DQ24_P_DLY
#define RX_DQ26_P_DLY				RX_DQ24_P_DLY
#define RX_DQ27_P_DLY				RX_DQ24_P_DLY
#define RX_DQ28_P_DLY				RX_DQ24_P_DLY
#define RX_DQ29_P_DLY				RX_DQ24_P_DLY
#define RX_DQ30_P_DLY				RX_DQ24_P_DLY
#define RX_DQ31_P_DLY				RX_DQ24_P_DLY

#define RX_DQ1_N_DLY				RX_DQ0_N_DLY
#define RX_DQ2_N_DLY				RX_DQ0_N_DLY
#define RX_DQ3_N_DLY				RX_DQ0_N_DLY
#define RX_DQ4_N_DLY				RX_DQ0_N_DLY
#define RX_DQ5_N_DLY				RX_DQ0_N_DLY
#define RX_DQ6_N_DLY				RX_DQ0_N_DLY
#define RX_DQ7_N_DLY				RX_DQ0_N_DLY

#define RX_DQ9_N_DLY				RX_DQ8_N_DLY
#define RX_DQ10_N_DLY				RX_DQ8_N_DLY
#define RX_DQ11_N_DLY				RX_DQ8_N_DLY
#define RX_DQ12_N_DLY				RX_DQ8_N_DLY
#define RX_DQ13_N_DLY				RX_DQ8_N_DLY
#define RX_DQ14_N_DLY				RX_DQ8_N_DLY
#define RX_DQ15_N_DLY				RX_DQ8_N_DLY

#define RX_DQ17_N_DLY				RX_DQ16_N_DLY
#define RX_DQ18_N_DLY				RX_DQ16_N_DLY
#define RX_DQ19_N_DLY				RX_DQ16_N_DLY
#define RX_DQ20_N_DLY				RX_DQ16_N_DLY
#define RX_DQ21_N_DLY				RX_DQ16_N_DLY
#define RX_DQ22_N_DLY				RX_DQ16_N_DLY
#define RX_DQ23_N_DLY				RX_DQ16_N_DLY

#define RX_DQ25_N_DLY				RX_DQ24_N_DLY
#define RX_DQ26_N_DLY				RX_DQ24_N_DLY
#define RX_DQ27_N_DLY				RX_DQ24_N_DLY
#define RX_DQ28_N_DLY				RX_DQ24_N_DLY
#define RX_DQ29_N_DLY				RX_DQ24_N_DLY
#define RX_DQ30_N_DLY				RX_DQ24_N_DLY
#define RX_DQ31_N_DLY				RX_DQ24_N_DLY
#endif

#define DDR_PHY_DQS_IN_DLY_0_0_VALUE	(RX_DQ0_P_DLY|\
				(RX_DQ1_P_DLY<<8)|\
				(RX_DQ2_P_DLY<<16)|\
				(RX_DQ3_P_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_1_0_VALUE (RX_DQ4_P_DLY|\
				(RX_DQ5_P_DLY<<8)|\
				(RX_DQ6_P_DLY<<16)|\
				(RX_DQ7_P_DLY<<24))


#define DDR_PHY_DQS_IN_DLY_2_0_VALUE	(RX_DQ0_N_DLY|\
				(RX_DQ1_N_DLY<<8)|\
				(RX_DQ2_N_DLY<<16)|\
				(RX_DQ3_N_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_3_0_VALUE (RX_DQ4_N_DLY|\
				(RX_DQ5_N_DLY<<8)|\
				(RX_DQ6_N_DLY<<16)|\
				(RX_DQ7_N_DLY<<24))


#define DDR_PHY_DQS_IN_DLY_0_1_VALUE	(RX_DQ8_P_DLY|\
				(RX_DQ9_P_DLY<<8)|\
				(RX_DQ10_P_DLY<<16)|\
				(RX_DQ11_P_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_1_1_VALUE (RX_DQ12_P_DLY|\
				(RX_DQ13_P_DLY<<8)|\
				(RX_DQ14_P_DLY<<16)|\
				(RX_DQ15_P_DLY<<24))


#define DDR_PHY_DQS_IN_DLY_2_1_VALUE	(RX_DQ8_N_DLY|\
				(RX_DQ9_N_DLY<<8)|\
				(RX_DQ10_N_DLY<<16)|\
				(RX_DQ11_N_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_3_1_VALUE (RX_DQ12_N_DLY|\
				(RX_DQ13_N_DLY<<8)|\
				(RX_DQ14_N_DLY<<16)|\
				(RX_DQ15_N_DLY<<24))

#define DDR_PHY_DQS_IN_DLY_0_2_VALUE	(RX_DQ16_P_DLY|\
				(RX_DQ17_P_DLY<<8)|\
				(RX_DQ18_P_DLY<<16)|\
				(RX_DQ19_P_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_1_2_VALUE (RX_DQ20_P_DLY|\
				(RX_DQ21_P_DLY<<8)|\
				(RX_DQ22_P_DLY<<16)|\
				(RX_DQ23_P_DLY<<24))

#define DDR_PHY_DQS_IN_DLY_2_2_VALUE	(RX_DQ16_N_DLY|\
				(RX_DQ17_N_DLY<<8)|\
				(RX_DQ18_N_DLY<<16)|\
				(RX_DQ19_N_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_3_2_VALUE (RX_DQ20_N_DLY|\
				(RX_DQ21_N_DLY<<8)|\
				(RX_DQ22_N_DLY<<16)|\
				(RX_DQ23_N_DLY<<24))

#define DDR_PHY_DQS_IN_DLY_0_3_VALUE	(RX_DQ24_P_DLY|\
				(RX_DQ25_P_DLY<<8)|\
				(RX_DQ26_P_DLY<<16)|\
				(RX_DQ27_P_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_1_3_VALUE (RX_DQ28_P_DLY|\
				(RX_DQ29_P_DLY<<8)|\
				(RX_DQ30_P_DLY<<16)|\
				(RX_DQ31_P_DLY<<24))


#define DDR_PHY_DQS_IN_DLY_2_3_VALUE	(RX_DQ24_N_DLY|\
				(RX_DQ25_N_DLY<<8)|\
				(RX_DQ26_N_DLY<<16)|\
				(RX_DQ27_N_DLY<<24))
#define DDR_PHY_DQS_IN_DLY_3_3_VALUE (RX_DQ28_N_DLY|\
				(RX_DQ29_N_DLY<<8)|\
				(RX_DQ30_N_DLY<<16)|\
				(RX_DQ31_N_DLY<<24))

#endif
#endif
