/*
 * Realtek Semiconductor Corp.
 *
 * sheipa.h:
 *   bsp chip address and IRQ mapping file
 *
 * Copyright (C) 2006-2015 Tony Wu (tonywu@realtek.com)
 */

#ifndef _PLAT_SHEIPA_H_
#define _PLAT_SHEIPA_H_

#include <linux/version.h>

#include <asm/mach-generic/irq.h>

/* FREQ */
#define BSP_PLL_FREQ		25000000	   /* 25 MHz */
#define BSP_CPU_FREQ		(2 * BSP_PLL_FREQ) /* 50 MHz */

/*
 * IRQ Mapping
 *
 * We have two mappings:
 *
 * 1. UMP with ICTL
 * 2. SMP with GIC
 */
#define BSP_IRQ_GIC		(MIPS_CPU_IRQ_BASE + 2)  /* GIC IRQ */
#define BSP_IRQ_IPI_RESCHED	(MIPS_CPU_IRQ_BASE + 3)
#define BSP_IRQ_IPI_CALL	(MIPS_CPU_IRQ_BASE + 4)
#define BSP_IRQ_PERFCOUNT	(MIPS_CPU_IRQ_BASE + 6)
#define BSP_IRQ_TIMER		(MIPS_CPU_IRQ_BASE + 7)  /* external timer */
#define BSP_IRQ_COMPARE		(MIPS_CPU_IRQ_BASE + 7)  /* local timer */

/* DWAPB UART */
#define BSP_UART0_VADDR		0xbfb03000UL
#define BSP_UART0_BAUD		57600
#define BSP_UART0_FREQ		(BSP_PLL_FREQ)
#define BSP_UART0_USR		(BSP_UART0_VADDR + 0x7c)
#define BSP_UART0_FCR		(BSP_UART0_VADDR + 0x08)
#define BSP_UART0_PADDR		0x1fb03000UL
#define BSP_UART0_PSIZE		0x100

#define BSP_UART1_VADDR		0xbfb02e00UL
#define BSP_UART1_BAUD		57600
#define BSP_UART1_FREQ		(BSP_PLL_FREQ)
#define BSP_UART1_USR		(BSP_UART1_VADDR + 0x7c)
#define BSP_UART1_FCR		(BSP_UART1_VADDR + 0x08)
#define BSP_UART1_PADDR		0x1fb02e00UL
#define BSP_UART1_PSIZE		0x100

/*
 * DWAPB Timer
 */
#define BSP_TIMER_FREQ		(BSP_PLL_FREQ)
#define BSP_TIMER_VADDR		0xbfb01000UL
#define BSP_TIMER_TLCR		(BSP_TIMER_VADDR + 0x00)
#define BSP_TIMER_TCVR		(BSP_TIMER_VADDR + 0x04)
#define BSP_TIMER_TCR		(BSP_TIMER_VADDR + 0x08)
#define BSP_TIMER_EOI		(BSP_TIMER_VADDR + 0x0c)

/* LS GMAC */
#define BSP_GMAC_PADDR		0x1b007000UL
#define BSP_GMAC_PSIZE		0x00000400UL

/* LS PCIe */
#define BSP_PCIE_RC_CFG		0xbb000000UL
#define BSP_PCIE_EP_CFG		0xb9010000UL
#define BSP_PCIE_IO_PADDR	0x19200000UL
#define BSP_PCIE_IO_PSIZE	0x00200000UL
#define BSP_PCIE_MEM_PADDR	0x19400000UL
#define BSP_PCIE_MEM_PSIZE	0x00c00000UL

/* LS SMU */
#define BSP_SMU_VADDR		0xbb007800UL
#define BSP_SMU_PADDR		0x1b007800UL
#define BSP_SMU_PSIZE		0x00000800UL

/*
 * Register access macro
 */
#ifndef REG32
#define REG32(reg)	(*(volatile unsigned int   *)(reg))
#endif
#ifndef REG16
#define REG16(reg)	(*(volatile unsigned short *)(reg))
#endif
#ifndef REG08
#define REG08(reg)	(*(volatile unsigned char  *)(reg))
#endif

#endif   /* _PLAT_SHEIPA_H */
