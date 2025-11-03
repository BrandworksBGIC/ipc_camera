/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 */

#ifndef __INITCALL_H
#define __INITCALL_H

typedef int (*init_fnc_t)(void);

struct init_fnc_a {
	init_fnc_t func;
	char *name;
};

#define FUNC_I(func) {func, #func}

#include <log.h>
#ifdef CONFIG_EFI_APP
#include <efi.h>
#endif
#include <asm/global_data.h>

/*
 * To enable debugging. add #define DEBUG at the top of the including file.
 *
 * To find a symbol, use grep on u-boot.map
 */
/* static inline int initcall_run_list(const init_fnc_t init_sequence[]) */
static inline int initcall_run_list(struct init_fnc_a init_sequence[])
{
	struct init_fnc_a *init_fnc_a_ptr;
	int i = 0;

	for (init_fnc_a_ptr = init_sequence; init_fnc_a_ptr->func; ++init_fnc_a_ptr) {
		unsigned long reloc_ofs = 0;
		int ret;
		const init_fnc_t init_fnc_ptr = init_fnc_a_ptr->func;
		const char *name = init_fnc_a_ptr->name;

		/*
		 * Sandbox is relocated by the OS, so symbols always appear at
		 * the relocated address.
		 */
		if (IS_ENABLED(CONFIG_SANDBOX) || (gd->flags & GD_FLG_RELOC))
			reloc_ofs = gd->reloc_off;
#ifdef CONFIG_EFI_APP
		reloc_ofs = (unsigned long)image_base;
#endif
		if (reloc_ofs)
			debug("initcall: %p (relocated to %p)\n",
					(char *)init_fnc_ptr - reloc_ofs,
					(char *)init_fnc_ptr);
		else
			debug("initcall: %p\n", (char *)init_fnc_ptr - reloc_ofs);

		ret = (init_fnc_ptr)();
		void ts_delta(int level, const char *func, int line);
		ts_delta(2, name, __LINE__);
		if (ret) {
			printf("initcall sequence %p failed at call %p (err=%d)\n",
			       init_sequence,
			       (char *)init_fnc_ptr - reloc_ofs, ret);
			return -1;
		}
	}
	return 0;
}

#endif
