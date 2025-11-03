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

#include <linux/suspend.h>
#include <linux/pm.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <asm/tlb.h>

#include "pm.h"

void __iomem *suspend_sram_base;
void (*rts_suspend_in_sram_fn)(void __iomem *sram_vbase);

#ifdef	CONFIG_RTS3917_SUSPEND_TO_RAM
static int rts_suspend_finish(unsigned long val)
{
	if (IS_ENABLED(CONFIG_RTS3917_SUSPEND_TO_RAM)) {
		local_flush_tlb_all();
		flush_cache_all();
		rts_suspend_in_sram_fn(suspend_sram_base);
		rts_pm_suspend_to_ram_unmap();
	} else {
		cpu_do_idle();
	}

	return 0;
}
#endif

static int rts_pm_enter(suspend_state_t state)
{
	int ret = 0;

	switch (state) {
	case PM_SUSPEND_STANDBY:
		local_flush_tlb_all();
		flush_cache_all();
		cpu_do_idle();
		break;
	case PM_SUSPEND_MEM:
#ifdef	CONFIG_RTS3917_SUSPEND_TO_RAM
		ret = rts_pm_suspend_to_ram_init();
		if (ret) {
			pr_warn("Suspend initialization failed!\n");
			break;
		}
		cpu_suspend(0, rts_suspend_finish);
#endif
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int rts_pm_valid_state(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_ON:
	case PM_SUSPEND_STANDBY:
		return 1;
	case PM_SUSPEND_MEM:
		if (IS_ENABLED(CONFIG_RTS3917_SUSPEND_TO_RAM))
			return 1;
		else
			return 0;
	default:
		return 0;
	}
}

static const struct platform_suspend_ops rts_pm_ops = {
	.valid = rts_pm_valid_state,
	.enter = rts_pm_enter,
};

static int __init rts_pm_init(void)
{
	suspend_set_ops(&rts_pm_ops);
	return 0;
}

late_initcall(rts_pm_init);
