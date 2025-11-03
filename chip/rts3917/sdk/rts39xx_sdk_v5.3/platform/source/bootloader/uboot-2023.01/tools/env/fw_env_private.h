/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2002-2008
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/* Pull in the current config to define the default environment */
#include <linux/kconfig.h>

#ifndef __ASSEMBLY__
#define __ASSEMBLY__ /* get only #defines from config.h */
#include <config.h>
#undef	__ASSEMBLY__
#else
#include <config.h>
#endif

/*
 * To build the utility with the static configuration
 * comment out the next line.
 * See included "fw_env.config" sample file
 * for notes on configuration.
 */
// #define CONFIG_FILE     "/etc/fw_env.config"

#ifndef CONFIG_FILE
#include <configs/rlxboard.h>
#ifdef CONFIG_RTS_EMMC_BOOT
#define DEVICE1_NAME      "/dev/mmcblk0boot0"
#else
#define DEVICE1_NAME      "/dev/mtd0"
#endif
#define DEVICE1_OFFSET    CONFIG_ENV_OFFSET
#define ENV1_SIZE         CONFIG_ENV_SIZE
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
#define DEVICE1_ESIZE     CONFIG_ENV_SECT_SIZE
#else
#define DEVICE1_ESIZE     0x20000
#endif
//#define DEVICE1_ENVSECTORS     2
#ifdef CONFIG_SYS_REDUNDAND_ENVIRONMENT
#define HAVE_REDUND /* For systems with 2 env sectors */
#ifdef CONFIG_RTS_EMMC_BOOT
#define DEVICE2_NAME      "/dev/mmcblk0boot0"
#else
#define DEVICE2_NAME      "/dev/mtd0"
#endif
#define DEVICE2_OFFSET    CONFIG_ENV_OFFSET_REDUND
#define ENV2_SIZE         CONFIG_ENV_SIZE
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
#define DEVICE2_ESIZE     CONFIG_ENV_SECT_SIZE
#else
#define DEVICE2_ESIZE     0x20000
#endif
//#define DEVICE2_ENVSECTORS     2
#endif
#endif

#ifndef CONFIG_BAUDRATE
#define CONFIG_BAUDRATE		115200
#endif

#ifndef CONFIG_BOOTDELAY
#define CONFIG_BOOTDELAY	5	/* autoboot after 5 seconds	*/
#endif

#ifndef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND						\
	"bootp; "							\
	"setenv bootargs root=/dev/nfs nfsroot=${serverip}:${rootpath} "\
	"ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}::off; "\
	"bootm"
#endif
