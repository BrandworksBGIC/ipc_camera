/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common header file for U-Boot
 *
 * This file still includes quite a few headers that should be included
 * individually as needed. Patches to remove things are welcome.
 *
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#ifndef __COMMON_H_
#define __COMMON_H_	1

#ifndef __ASSEMBLY__		/* put C only stuff in this section */
#include <config.h>
#include <errno.h>
#include <time.h>
#include <asm-offsets.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <stdarg.h>
#include <stdio.h>
#include <linux/kernel.h>
#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <asm/global_data.h>	/* global data used for startup functions */
#include <vsprintf.h>
#include <display_options.h>

u8 get_rst_mode(void);
int exit_spi_qpi_mode(void);

int spi_flash_update_external(u32 offset, size_t len, void *buf);
int spi_flash_read_external(u32 offset, size_t len, void *buf);

#ifdef CONFIG_CRYPTO_BOOT
int crypto_init(void);
extern int rlx_aes_ecb_encrypt(u8 *dst, u8 *src, unsigned int nbytes);
extern int rlx_aes_ecb_decrypt(u8 *dst, u8 *src, unsigned int nbytes);
int load_key_from_sd(void);
int load_iv_from_sd(void);
#endif

#ifdef CONFIG_UPDATE_KERNEL_FROM_SD
int load_bin_from_sd(void);
#endif
/* nand_ops.c */
int update_image_for_nand(ulong offset, size_t length,
			  unsigned char *buf, size_t erase_length);
int update_image_to_nand(ulong offset, size_t length, unsigned char *buf);
int get_data_length_from_nand(ulong offset, ulong *data_length);
int copy_imagedata_to_ram(ulong offset, ulong ram_addr, ulong data_length);

int do_write_for_rescure(void);

/*mmc_ops.c*/
int get_data_length_from_mmc(ulong offset, ulong *data_length);
int copy_mmcdata_to_ram(ulong offset, ulong ram_addr, ulong data_length);

int fdt_node_offset_by_label(const void *fdt, int startoffset,
				  const char *label);
int fdt_get_mtd_index_by_label(const void *fdt, int startoffset,
				  const char *label, int *mtd_id);
u32 get_dtb_data_of_offset(const void *data, int len);
void set_boot_left_reg_when_press_key(void);
#endif	/* __ASSEMBLY__ */

/* Pull in stuff for the build system */
#ifdef DO_DEPS_ONLY
# include <env_internal.h>
#endif

#endif	/* __COMMON_H_ */
