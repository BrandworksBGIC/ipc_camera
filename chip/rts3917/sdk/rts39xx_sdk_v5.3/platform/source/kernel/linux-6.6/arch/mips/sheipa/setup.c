/*
 * Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Copyright (C) 2005-2022 Tony Wu <tonywu@realtek.com>
 */

#include <linux/memblock.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/irqchip.h>

#include <asm/prom.h>
#include <asm/time.h>
#include <asm/reboot.h>
#include <asm/fw/fw.h>
#include <asm/cpu-features.h>

static void plat_machine_restart(char *command)
{
	local_irq_disable();
	while (1) ;
}

static void plat_machine_halt(void)
{
	printk("System halted.\n");
	while(1);
}

/* callback function */
#define SZ_LOWMEM	(320 << 20)
void __init plat_mem_setup(void)
{
	void *dtb;

	set_io_port_base(KSEG1);

	dtb = get_fdt();
	__dt_setup_arch(dtb);

	/* set up memory */
	if (!early_init_dt_scan_memory()) {
		return;
	}
	/* Memory region is shown as follows:
	 *
	 * 0x00000000 - 0x14000000 - 0x20000000 => ....
	 *      dram (320MB)      mmio        highmem
	 */
	if (cpu_mem_size <= SZ_LOWMEM)
		memblock_add(0, cpu_mem_size);
	else {
		memblock_add(0, SZ_LOWMEM);
		if (IS_ENABLED(CONFIG_HIGHMEM)) {
			memblock_add(HIGHMEM_START, (cpu_mem_size - SZ_LOWMEM));
		}
	}
}

static int __init plat_reboot_setup(void)
{
	/* set reset vectors */
	_machine_restart = plat_machine_restart;
	_machine_halt = plat_machine_halt;
	pm_power_off = plat_machine_halt;

	return 0;
}
arch_initcall(plat_reboot_setup);

void __init plat_irq_init(void)
{
	irqchip_init();
}

void __init device_tree_init(void)
{
	if (!initial_boot_params)
		return;
	unflatten_and_copy_device_tree();
}

static int __init plat_of_setup(void)
{
	__dt_register_buses("rtk,sheipa", "sheipa-bus");
	return 0;
}
arch_initcall(plat_of_setup);
