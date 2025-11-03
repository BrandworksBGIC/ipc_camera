/*
 * Realtek Semiconductor Corp.
 *
 * bsp/timer.c
 *     bsp timer initialization code
 *
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 */
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/delay.h>
#include <linux/clockchips.h>
#include <linux/irq.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/timex.h>
#include <asm/mach/time.h>
#ifdef CONFIG_ARCH_CEVT_CA7
#include <clocksource/arm_arch_timer.h>
#endif
#include <mach/hardware.h>

#ifdef CONFIG_ARCH_CEVT_DWC
void inline plat_timer_ack(void)
{
	unsigned volatile int eoi;
	eoi = REG32(BSP_TIMER0_EOI);
}

extern int ext_clockevent_init(unsigned irq);
static void __init dwc_timer_init(void)
{
	/* disable timer */
	REG32(BSP_TIMER0_TCR) = 0x00000000;

	/* initialize timer registers */
	REG32(BSP_TIMER0_TLCR) = BSP_TIMER0_FREQ / HZ;

	/* hook up timer interrupt handler */
	ext_clockevent_init(BSP_IRQ_TIMER0);

	/* enable timer */
	REG32(BSP_TIMER0_TCR) = 0x00000003;       /* 0000-0000-0000-0011 */
}
#endif

void __init plat_init_time(void)
{
	extern void plat_init_clock(void);

	plat_init_clock();

#ifdef CONFIG_ARCH_CEVT_DWC
	dwc_timer_init();
#endif

#ifdef CONFIG_ARCH_CEVT_CA7
	arch_timer_init(false);
#endif
}
