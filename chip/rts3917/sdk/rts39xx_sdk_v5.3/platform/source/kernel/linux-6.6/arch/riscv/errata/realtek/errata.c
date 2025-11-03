// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductors Inc.
 */

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <asm/alternative.h>
#include <asm/cacheflush.h>
#include <asm/errata_list.h>
#include <asm/patch.h>
#include <asm/vendorid_list.h>

static bool errata_probe_cmo(unsigned int stage, unsigned long arch_id,
			     unsigned long impid)
{
	if (!IS_ENABLED(CONFIG_ERRATA_REALTEK_CMO))
		return false;

	/*
	 * Affected cores: tr9 with release 1.5.0+ version
	 */
	if (arch_id != 0x8052544b || ((impid & 0xfff) < 0x150))
		return false;

	if (stage == RISCV_ALTERNATIVES_EARLY_BOOT)
		return false;

	riscv_cbom_block_size = __DCACHE_LINE_SIZE;
	riscv_noncoherent_supported();
	return true;
}

static u32 realtek_errata_probe(unsigned int stage, unsigned long archid,
				unsigned long impid)
{
	u32 cpu_req_errata = 0;

	if (errata_probe_cmo(stage, archid, impid))
		cpu_req_errata |= BIT(ERRATA_REALTEK_CMO);

	return cpu_req_errata;
}

void __init_or_module realtek_errata_patch_func(struct alt_entry *begin,
						struct alt_entry *end,
						unsigned long archid,
						unsigned long impid,
						unsigned int stage)
{
	struct alt_entry *alt;
	u32 cpu_req_errata = realtek_errata_probe(stage, archid, impid);
	u32 tmp;

	for (alt = begin; alt < end; alt++) {
		if (alt->vendor_id != REALTEK_VENDOR_ID)
			continue;
		if (alt->patch_id >= ERRATA_REALTEK_NUMBER)
			continue;

		tmp = (1U << alt->patch_id);
		if (cpu_req_errata & tmp) {
			/* On vm-alternatives, the mmu isn't running yet */
			if (stage == RISCV_ALTERNATIVES_EARLY_BOOT) {
				memcpy(ALT_OLD_PTR(alt), ALT_ALT_PTR(alt),
				       alt->alt_len);
			} else {
				mutex_lock(&text_mutex);
				patch_text_nosync(ALT_OLD_PTR(alt), ALT_ALT_PTR(alt),
						  alt->alt_len);
				mutex_unlock(&text_mutex);
			}
		}
	}

	if (stage == RISCV_ALTERNATIVES_EARLY_BOOT)
		local_flush_icache_all();
}
