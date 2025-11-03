/*
 * Realtek Semiconductor Corp.
 *
 * bsp/irq.c
 *
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>

void __iomem *gic_dist_base_addr;

void __init plat_init_irq(void)
{
	void __iomem *gic_cpu_base;

	/* Static mapping, never released */
	gic_dist_base_addr = ioremap(BSP_DIST_PADDR, SZ_4K);
	BUG_ON(!gic_dist_base_addr);

	/* Static mapping, never released */
	gic_cpu_base = ioremap(BSP_GIC_PADDR, SZ_512);
	BUG_ON(!gic_cpu_base);

	gic_init(0, GIC_IRQ_OFS, gic_dist_base_addr, gic_cpu_base);
}
