/*
 * Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright 2014  Tony Wu (tonywu@realtek.com)
 */

#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>

#include <linux/clkdev.h>
#include <linux/clk-provider.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>

/* allocate io resource */
static struct map_desc bsp_io_desc[] __initdata = {
	{
                .virtual = BSP_SMP_VADDR,
                .pfn = __phys_to_pfn(BSP_SMP_PADDR),
                .length = SZ_4,
                .type = MT_DEVICE_NONSHARED,
        },
	{
                .virtual = BSP_EARLYCON_VADDR,
                .pfn = __phys_to_pfn(BSP_EARLYCON_PADDR),
                .length = SZ_256,
                .type = MT_DEVICE,
        },
};

static void __init plat_map_io(void)
{
	extern void plat_smp_map_io(void);

	iotable_init(bsp_io_desc, ARRAY_SIZE(bsp_io_desc));
#ifdef CONFIG_SMP
	plat_smp_map_io();
#endif
}

static void __init plat_init_machine(void)
{
	extern void plat_init_cpufreq(void);

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
#ifdef CONFIG_CPU_FREQ
	plat_init_cpufreq();
#endif
}

static void plat_arch_reset(char mode, const char *cmd)
{
}

void (*arch_reset)(char, const char *) = plat_arch_reset;

#ifdef CONFIG_SMP
extern struct smp_operations plat_smp_ops;
#endif

/*
 * Realtek Sheipa DT machine
 */
static const char * const plat_dt_match[] __initconst = {
        "rtk,sheipa",
        NULL
};

DT_MACHINE_START(RLXARM_DT, "Realtek ARM (Flattened Device Tree)")
	.dt_compat = plat_dt_match,
	.init_machine = plat_init_machine,
	.map_io = plat_map_io,
#ifdef CONFIG_SMP
	.smp = smp_ops(plat_smp_ops),
#endif
MACHINE_END
