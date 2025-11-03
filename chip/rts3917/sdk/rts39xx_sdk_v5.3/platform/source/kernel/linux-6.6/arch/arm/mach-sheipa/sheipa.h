/*
 * Realtek Semiconductor Corp.
 *
 * sheipa/sheipa.h:
 *     bsp chip address and IRQ mapping file
 *
 * Copyright (C) 2006-2022 Tony Wu (tonywu@realtek.com)
 */

#ifndef _MACH_SHEIPA_SHEIPA_H_
#define _MACH_SHEIPA_SHEIPA_H_

#include <linux/version.h>
#include <linux/irqchip/arm-gic.h>

/*
 * Register access macro
 */
#ifndef REG32
#define REG32(reg)		(*(volatile unsigned int   *)(reg))
#endif
#ifndef REG16
#define REG16(reg)		(*(volatile unsigned short *)(reg))
#endif
#ifndef REG08
#define REG08(reg)		(*(volatile unsigned char  *)(reg))
#endif

/*
 * IRQ Mapping
 *
 * In ARM_GIC mode, IRQ 0:26 are reserved for IPI, so we
 * add the following mapping for GIC installation:
 *
 * GIC_IRQ_SPI starts at 32.
 * GIC_IRQ_SPI(x) = 32 + x
 */

/*
 * SMPBOOT pen holding address
 */
#define BSP_SMP_PADDR 0x600010f8UL
#define BSP_SMP_VADDR 0xfee010f8UL

/*
 * Earlycon address
 */
#define BSP_EARLYCON_PADDR 0x60007000UL
#define BSP_EARLYCON_VADDR 0xfee07000UL

/*
 * LS SMU
 */
#define BSP_SMU_VADDR 0xbb001000UL
#define BSP_SMU_PADDR 0x70001000UL
#define BSP_SMU_PSIZE 0x00000800UL

#endif   /* _BSPCHIP_H */
