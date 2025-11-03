/*
 * Realtek Semiconductor Corp.
 *
 * bsp/prcm.c
 *
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <asm/mach/arch.h>	/* for MACHINE_START */
#include <asm/mach-types.h>	/* for MACH_TYPE_RLXARM */

#include <mach/hardware.h>

void (*arch_reset)(char, const char *) = plat_arch_reset;

