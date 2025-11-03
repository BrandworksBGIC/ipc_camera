/*
 * Realtek Semiconductor Corp.
 *
 * Copyright 2012  Viller Hsiao (villerhsiao@realtek.com)
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <cpu_func.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <netdev.h>
#include <dm.h>
#include <linux/delay.h>
#include "bspchip.h"
#include "ddr_def.h"
#include <otp/rts_otp.h>
#include <rts_mmc.h>
#include <rts_gpio.h>

DECLARE_GLOBAL_DATA_PTR;
#define get_val(addr)		REG32(addr)
#define set_val(addr, val)	REG32(addr) = (val)
#define clear_bit(addr, val)	set_val((addr), (get_val(addr) & ~(val)))
#define set_bit(addr, val)	set_val((addr), (get_val(addr) | (val)))

void write_mask(u32 reg, u32 bit, u32 mask)
{
	u32 temp;

	temp = get_val(reg);
	set_val(reg, (temp & (~mask)) | bit);
}

void reset_cpu(void)
{
	/* set watchdog */
	/* watchdog time: 1s, reset system when timeout, enable watchdog */
	writel(WATCH_DOG_CFG_DATA, (u32 *)WATCHDOG_CFG_REG);
	printf("set watchdog, resetting...");
	while (1)
		;
}

void set_boot_left_reg_when_press_key(void)
{
	int a_left = 0;
	int b_left = 0;
	char *boot_order;
	const char *reg_addr_s;
	u32 reg_addr;

	reg_addr_s = env_get("dual_image_reg_addr");
	reg_addr = simple_strtoul(reg_addr_s, NULL, 16);

	boot_order = env_get("BOOT_ORDER");
	a_left = REG32(reg_addr) & 0xF;
	b_left = REG32(reg_addr) >> 4;

	if (strcmp(boot_order, "A B") == 0)
		REG32(reg_addr) = a_left + 1 | b_left << 4;
	else
		REG32(reg_addr) = a_left | (b_left + 1) << 4;
}

int board_init(void)
{
	return 0;
}

void config_bisr(void)
{
	u32 mdio_rdata;

	REG32(CPU_PWR_RSTN) = CA7_PWR_RSTN_ENABLE;
	REG32(CPU_BISR_RSTN) = CA7_BISR_RSTN_ENABLE;
	REG32(CPU_BISR_CTRL) = CA7_SECOND_RUN_DISABLE | CA7_HOLD_REMAP_DISABLE;
	REG32(CPU_COORD_RSTN) = CA7_COORD_RST_N_DISABLE;
	REG32(CPU_COORD_RSTN) = CA7_COORD_RST_N_ENABLE;
	REG32(CPU_MEM_SPEED_MODE) = CPU_MEM_SPEED_MODE1;

	/* Step 1: ca7_pwr_rstn */
	REG32(CPU_PWR_RSTN) = CA7_PWR_RSTN_DISABLE;
	REG32(CPU_PWR_RSTN) = CA7_PWR_RSTN_ENABLE;
	mdelay(2);

	mdio_rdata = REG32(CPU_BISR_DONE);
	if ((mdio_rdata & CA7_BISR_DONE) != CA7_BISR_DONE) {
		printf("cpu_bisr_done 1st err!\n");
		return;
	}

	mdio_rdata = REG32(CPU_BISR_FAIL);
	if ((mdio_rdata & (CA7_BISR_REPAIRED | CA7_BISR_FAIL)) == 0)
		printf("memory dont need repair\n");
	else if (((mdio_rdata & CA7_BISR_FAIL) == 0) && ((mdio_rdata & CA7_BISR_REPAIRED) == CA7_BISR_REPAIRED))
		printf("memory repaired!\n");
	else
		printf("memory can not be repaired!\n");

	/* Step 2: ca7_bisr_rstn */
	REG32(CPU_BISR_CTRL) = CA7_SECOND_RUN_ENABLE | CA7_HOLD_REMAP_DISABLE;
	REG32(CPU_BISR_RSTN) = CA7_BISR_RSTN_DISABLE;
	REG32(CPU_BISR_RSTN) = CA7_BISR_RSTN_ENABLE;
	mdelay(2);

	mdio_rdata = REG32(CPU_BISR_DONE);
	if ((mdio_rdata & CA7_BISR_DONE) != CA7_BISR_DONE) {
		printf("cpu_bisr_done 2nd err!\n");
		return;
	}

	mdio_rdata = REG32(CPU_BISR_FAIL);
	if ((mdio_rdata & (CA7_BISR_REPAIRED | CA7_BISR_FAIL)) == 0)
		printf("memory dont need repair\n");
	else if (((mdio_rdata & CA7_BISR_FAIL) == 0) && ((mdio_rdata & CA7_BISR_REPAIRED) == CA7_BISR_REPAIRED))
		printf("memory repaired!\n");
	else
		printf("memory can not be repaired!\n");

	REG32(CPU_BISR_CTRL) = CA7_SECOND_RUN_DISABLE | CA7_HOLD_REMAP_ENABLE;
	REG32(CPU_PWR_RSTN) = CA7_PWR_RSTN_DISABLE;
	REG32(CPU_PWR_RSTN) = CA7_PWR_RSTN_ENABLE;
}

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	config_bisr();
	dcache_enable();
}
#endif

int get_dram_clock(void)
{
#if defined(CONFIG_TARGET_ASIC) || defined(CONFIG_TARGET_EMULATOR)
	u32 dpi_n_code, dpi_f_code, clock = 0;

	dpi_f_code = REG32(DDR_PHY_SSC2) & 0x7ff;
	dpi_n_code = REG32(DDR_PHY_SSC3) & 0xff;

	/* clock = REF_PLL * (dpi_f_code * 2 / 2048 + (dpi_n_code + 3) * 2) */
	clock  = ((50  * dpi_f_code * 2 + 2048)  >> 12) +
		25 * (dpi_n_code + 3) * 2;

	return (int)clock;
#endif

#ifdef CONFIG_TARGET_FPGA
	return 112;
#endif

	return 0;
}

int dram_init(void)
{
	/* Sdram is setup by assembler code */
	/* If memory could be changed, we should return the true value here */
	u32 dram_size;
	u32 drr_value_temp;

	REG32(PCTL_DRAM_SIZE) = 0x3fffffff;

	REG32(0xBFFFFFF8) = 0x40000000;

	REG32(0x9FFFFFF8) = 0x20000000;

	REG32(0x8FFFFFF8) = 0x10000000;

	REG32(0x87FFFFF8) = 0x08000000;

	REG32(0x83FFFFF8) = 0x04000000;

	mdelay(1);
//	printf(" detect size is 0x%x\n", REG32(0xBFFFFFF8));
	dram_size = REG32(0xBFFFFFF8);
	REG32(PCTL_DRAM_SIZE) = dram_size - 1;

	drr_value_temp = REG32(PCTL_DRR);

	/* change trfc parameter for external ddr */
#ifdef CONFIG_RTS3917_BGA240_DDR3_GENERAL
	if (dram_size == 0x08000000) {
		REG32(PCTL_DRR) = (drr_value_temp & 0xffffff00) |
			(110000 / PCTL_PERIOD_PS + 1);
	}

	if (dram_size == 0x10000000) {
		REG32(PCTL_DRR) = (drr_value_temp & 0xffffff00) |
			(160000 / PCTL_PERIOD_PS + 1);
	}

	if (dram_size == 0x20000000) {
		REG32(PCTL_DRR) = (drr_value_temp & 0xffffff00) |
			(260000 / PCTL_PERIOD_PS + 1);
	}

	if (dram_size == 0x40000000) {
		REG32(PCTL_DRR) = (drr_value_temp & 0xffffff00) |
			(350000 / PCTL_PERIOD_PS + 1);
	}
	REG32(PCTL_CCR) = 0x80000000;
#endif

#ifdef _DDR_SETTING_DEBUG_
	printf("DDR controller setting\n");
	printf("2a0 wrap_idr: %x\n", REG32(PCTL_WRAP_IDR));
	printf("f4 svn_id: %x\n", REG32(PCTL_SVN_ID));
	printf("f8 idr: %x\n", REG32(PCTL_IDR));
	printf("224 misc: %x\n", REG32(PCTL_MISC));
	printf("4 dcr: %x\n", REG32(PCTL_DCR));
	printf("08 iocr: %x\n", REG32(PCTL_IOCR));
	printf("3c mr2: %x\n", REG32(PCTL_MR2));
	printf("38 mr1: %x\n", REG32(PCTL_MR1));
	printf("34 mr0: %x\n", REG32(PCTL_MR0));
	printf("10 drr: %x\n", REG32(PCTL_DRR));
	printf("14 tpr0: %x\n", REG32(PCTL_TPR0));
	printf("18 tpr1: %x\n", REG32(PCTL_TPR1));
	printf("1c tpr2: %x\n", REG32(PCTL_TPR2));
	printf("0c csr: %x\n", REG32(PCTL_CSR));
	printf("30 mrinfo: %x\n", REG32(PCTL_MR_INFO));
#endif
	gd->ram_size = dram_size;
	return 0;
}

void load_otp(void)
{
	rts_otp_enable();
	rts_otp_load(0, 32);
}

int checkboard(void)
{
	u32 pll_n_code, pll_f_code, pll_clk = 0;
	u32 cpu_clk_sel, cpu_clk_div, cpu_clk = 0;

	cpu_clk_sel = REG32(CPU_CLK_CFG_REG) & 0x3;
	cpu_clk_div = (REG32(CPU_CLK_CFG_REG) >> 2) & 0x7;
	if (cpu_clk_div == 0)
		cpu_clk_div = 1;
	else
		cpu_clk_div *= 2;

	switch (cpu_clk_sel) {
	case CPU_SELECT_USBPLL_3:
		pll_clk = 480000000;
		break;
	case CPU_SELECT_GPLL0_1:
		pll_n_code = REG32(SYS_GPLL0_SCCG_CFG1) & 0x1ff;
		pll_f_code = (REG32(SYS_GPLL0_SCCG_CFG1) >> 8) & 0xfff;
		pll_clk = (pll_n_code + 2 + pll_f_code / 4096) * 25000000;
		break;
	case CPU_SELECT_GPLL1_1:
		pll_n_code = REG32(SYS_GPLL1_SCCG_CFG1) & 0x1ff;
		pll_f_code = (REG32(SYS_GPLL1_SCCG_CFG1) >> 8) & 0xfff;
		pll_clk = (pll_n_code + 2 + pll_f_code / 4096) * 25000000;
		break;
	case CPU_SELECT_GPLL2_1:
		pll_n_code = REG32(SYS_GPLL2_SCCG_CFG1) & 0x1ff;
		pll_f_code = (REG32(SYS_GPLL2_SCCG_CFG1) >> 8) & 0xfff;
		pll_clk = (pll_n_code + 2 + pll_f_code / 4096) * 25000000;
		break;
	default:
		break;
	}

	cpu_clk = pll_clk / cpu_clk_div;

	printf("Board: IPCAM RTS3917\n");
	printf("CPU:   ARM Cortex-A @ %dM\n", cpu_clk / 1000000);

	load_otp();

	return 0;
}

u8 get_rst_mode(void)
{
	u8 mode = 0;

	mode = (REG32(RST_MODE_ADDR) & RST_MASK);

	return mode;
}

#if defined(CONFIG_DISPLAY_CPUINFO)
/*
 * Print CPU information
 */
int print_cpuinfo(void)
{
	// puts("CPU  : SHEIPA SOC");

	return 0;
}
#endif

#if defined(CONFIG_DM_SPI_FLASH)
int init_dm_spi_flash_sheipa(void)
{
	struct udevice *dev;
	const char flash_name[] = "flash@0";

	uclass_first_device(UCLASS_SPI_FLASH, &dev);
	if (!dev) {
		pr_debug("sheipa flash init failed");
		return 1;
	}
	while (strcmp(dev->name, flash_name) != 0) {
		uclass_next_device(&dev);
		if (!dev) {
			pr_debug("sheipa flash init failed");
			return 1;
		}
	}

	return 0;
}
#endif

#if defined(CONFIG_BOARD_EARLY_INIT_R)
/*
 * init spi flash
 */
int board_early_init_r(void)
{
#if defined(CONFIG_DM_SPI_FLASH)
	return init_dm_spi_flash_sheipa();
#endif
	return 0;
}
#endif

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(MMC_RTS)
static const struct rts_mmc_plat rts_mmc0_plat = {
	.base_addr = RTS_MMC_BASEADDR,
	.cfg.name  = "rtsmmc",
	.cfg.f_max = RTS_MMC_DUMMY_F_MAX,
	.cfg.f_min = RTS_MMC_DUMMY_F_MIN,
	.cfg.b_max = RTS_MMC_MAX_BLOCK_LEN,
	.cfg.voltages = MMC_VDD_32_33 | MMC_VDD_33_34,
};

U_BOOT_DRVINFO(rts_mmc0) = {
	.name = "rts_mmc",
	.plat = &rts_mmc0_plat,
};
#endif

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(RTS_GPIO)
U_BOOT_DRVINFO(rts_gpio) = {
	.name = "rts_gpio"
};
#endif

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(RTL8168)
U_BOOT_DRVINFO(rts_eth) = {
	.name = "eth_rtl8168",
};
#endif

#if !CONFIG_IS_ENABLED(OF_CONTROL) && CONFIG_IS_ENABLED(REALTEK_USB_DEVICE)
#include <usb/rts_udc.h>
static const struct rts_udc_plat rts_udc_plat = {
	.base_addr = RTS_UDC_BASE,
	.mc_base_addr = RTS_UDC_MC_BASE,
};

U_BOOT_DRVINFO(rts_udc) = {
	.name = "rts_udc",
	.plat = &rts_udc_plat,
};
#endif
