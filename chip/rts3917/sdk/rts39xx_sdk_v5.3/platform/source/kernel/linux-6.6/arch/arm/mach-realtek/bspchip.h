/*
 * Realtek Semiconductor Corp.
 *
 * bsp/bspchip.h:
 *     bsp chip address and IRQ mapping file
 *
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 */

#ifndef _BSPCHIP_H_
#define _BSPCHIP_H_

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
 * ARM PERIPHBASE
 */
#define BSP_PERIPHERAL_PADDR	0x02010000
#define BSP_SCU_PADDR		(BSP_PERIPHERAL_PADDR + 0x0000)

#define BSP_PERIPHERAL_VADDR	0xef210000
#define BSP_SCU_VADDR		(BSP_PERIPHERAL_VADDR + 0x0000)

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
#define BSP_SMP_PADDR		0x1fb070f8UL
#define BSP_SMP_VADDR		0xfee070f8UL

/*
 * Earlycon address
 */
#define BSP_EARLYCON_PADDR	0x18810100UL
#define BSP_EARLYCON_VADDR	0xfee10100UL

/*
 * LS SMU
 */
#define BSP_SMU_VADDR		0xbb007800UL
#define BSP_SMU_PADDR		0x1b007800UL
#define BSP_SMU_PSIZE		0x00000800UL

#endif   /* _BSPCHIP_H */
