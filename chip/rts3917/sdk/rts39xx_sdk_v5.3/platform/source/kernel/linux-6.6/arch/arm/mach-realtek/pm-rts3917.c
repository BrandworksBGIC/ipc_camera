// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <asm/fncpy.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "pm.h"
#include "pm-rts3917.h"

int rts_pm_suspend_to_ram_init(void)
{
	struct rts_pm_dev_info *pm_info;

	suspend_sram_base = __arm_ioremap_exec(SRAM_BASE, SRAM_SIZE, false);

	memset(suspend_sram_base, 0, sizeof(*pm_info));
	pm_info = suspend_sram_base;
	pm_info->sram_base.pbase = SRAM_BASE;
	pm_info->sram_base.vbase = suspend_sram_base;

	pm_info->ddrc_base.pbase = DDRC_BASE;
	pm_info->ddrc_base.vbase = ioremap(DDRC_BASE, DDRC_SIZE);
	if (!pm_info->ddrc_base.vbase) {
		pr_warn("%s: failed to get ddrc base %d!\n", __func__, 0);
		goto ddrc_remap_failed;
	}

	pm_info->ddrp_base.pbase = DDRP_BASE;
	pm_info->ddrp_base.vbase = ioremap(DDRP_BASE, DDRP_SIZE);
	if (!pm_info->ddrp_base.vbase) {
		pr_warn("%s: failed to get ddr phy base %d!\n", __func__, 0);
		goto ddrp_remap_failed;
	}

	pm_info->misc_base.pbase = MISC_BASE;
	pm_info->misc_base.vbase = ioremap(MISC_BASE, MISC_SIZE);
	if (!pm_info->misc_base.vbase) {
		pr_warn("%s: failed to get misc base %d!\n", __func__, 0);
		goto misc_remap_failed;
	}

	pm_info->gpll_base.pbase = GPLL_BASE;
	pm_info->gpll_base.vbase = ioremap(GPLL_BASE, GPLL_SIZE);
	if (!pm_info->gpll_base.vbase) {
		pr_warn("%s: failed to get gpll base %d!\n", __func__, 0);
		goto gpll_remap_failed;
	}

	pm_info->clk_base.pbase = CLOCK_BASE;
	pm_info->clk_base.vbase = ioremap(CLOCK_BASE, CLOCK_SIZE);
	if (!pm_info->clk_base.vbase) {
		pr_warn("%s: failed to get clock base %d!\n", __func__, 0);
		goto clock_remap_failed;
	}

	pm_info->wdt_base.pbase = WATCHDOG_BASE;
	pm_info->wdt_base.vbase = ioremap(WATCHDOG_BASE, WATCHDOG_SIZE);
	if (!pm_info->wdt_base.vbase) {
		pr_warn("%s: failed to get watchdog base %d!\n", __func__, 0);
		goto wdt_remap_failed;
	}

#ifdef CONFIG_RTSX_WATCHDOG
	pm_info->wdt_flag = g_wdt_flag;
#endif

	/* copy rts_suspend function to sram */
	rts_suspend_in_sram_fn = fncpy(
		suspend_sram_base + sizeof(*pm_info),
		&rts_suspend,
		SRAM_SIZE - sizeof(*pm_info));

	goto success;

wdt_remap_failed:
	iounmap(pm_info->wdt_base.vbase);

clock_remap_failed:
	iounmap(pm_info->clk_base.vbase);

gpll_remap_failed:
	iounmap(pm_info->gpll_base.vbase);

misc_remap_failed:
	iounmap(pm_info->misc_base.vbase);

ddrp_remap_failed:
	iounmap(pm_info->ddrp_base.vbase);

ddrc_remap_failed:
	iounmap(pm_info->ddrc_base.vbase);

	return -1;

success:
	return 0;
}

void rts_pm_suspend_to_ram_unmap(void)
{
	struct rts_pm_dev_info *pm_info;

	if (suspend_sram_base) {
		pm_info = suspend_sram_base;
		iounmap(pm_info->wdt_base.vbase);
		iounmap(pm_info->clk_base.vbase);
		iounmap(pm_info->gpll_base.vbase);
		iounmap(pm_info->misc_base.vbase);
		iounmap(pm_info->ddrp_base.vbase);
		iounmap(pm_info->ddrc_base.vbase);
		iounmap(suspend_sram_base);
		suspend_sram_base = NULL;
	}
}
