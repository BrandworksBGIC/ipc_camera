/*
 * Realtek Semiconductor Corp.
 *
 * Common clock framework to the simplest
 *
 * Tony Wu (tonywu@realtek.com)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-private.h>

#include <sheipa.h>

static void plat_add_clk(const char *name, unsigned long rate)
{
	struct clk *clk;
	clk = clk_register_fixed_rate(NULL, name, NULL, CLK_IS_ROOT, rate);
	clk_register_clkdev(clk, NULL, name);
}

void __init plat_init_clock(void)
{
	plat_add_clk("cpu", BSP_CPU_FREQ);
	plat_add_clk("smp_ca7", BSP_CPU_FREQ / 2);
}
