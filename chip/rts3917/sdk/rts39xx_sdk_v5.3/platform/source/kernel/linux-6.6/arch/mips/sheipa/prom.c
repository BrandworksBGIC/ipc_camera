/*
 * Realtek Semiconductor Corp.
 *
 * bsp/prom.c
 *     bsp early initialization code
 *
 * Copyright (C) 2006-2015 Tony Wu (tonywu@realtek.com)
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/prom.h>
#include <asm/fw/fw.h>
#include <asm/taroko-mmcr.h>

const char *get_system_type(void)
{
	return "Sheipa";
}

void __init prom_free_prom_memory(void)
{
	return;
}

#ifdef CONFIG_SHEIPA_TAROKO
static void plat_l2c_enable(void)
{
	if (IS_ENABLED(CONFIG_SMP)) {
		MMCR_REG32(L2C_CTRL) = MMCR_REG32(L2C_CTRL) | 0x1;
	} else {
		change_c0_cctl1(CCTL_L2CON, 1);
		change_c0_cctl1(CCTL_L2COFF, 0);
	}
}
#else
static void plat_l2c_enable(void)
{
}
#endif

/* Do basic initialization */
void __init prom_init(void)
{
	extern void plat_smp_init(void);
	extern void early_uart_init(void);

	fw_init_cmdline();

#ifdef CONFIG_EARLY_PRINTK
	early_uart_init();
#endif

#ifdef CONFIG_SMP
	plat_smp_init();
#endif
	plat_l2c_enable();
}
