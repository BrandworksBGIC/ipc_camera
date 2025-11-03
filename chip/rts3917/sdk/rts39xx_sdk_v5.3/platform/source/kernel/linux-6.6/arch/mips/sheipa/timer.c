/*
 * Realtek Semiconductor Corp.
 *
 * bsp/timer.c:
 *     bsp timer initialization code
 *
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 */
#include <linux/init.h>
#include "sheipa.h"

#ifdef CONFIG_CEVT_R4K
#include <asm/time.h>

unsigned int get_c0_compare_int(void)
{
	return BSP_IRQ_COMPARE;
}

void __init plat_time_init(void)
{
	mips_hpt_frequency = BSP_CPU_FREQ;
	cp0_compare_irq = BSP_IRQ_COMPARE;

	write_c0_count(0);
	clear_c0_cause(CAUSEF_DC);
}
#endif
