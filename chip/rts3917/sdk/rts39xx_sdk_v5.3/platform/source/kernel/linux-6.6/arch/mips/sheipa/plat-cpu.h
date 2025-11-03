/*
 * Realtek Semiconductor Corp.
 *
 * sheipa/plat-cpu.h
 *     Platform cpu and memory header file
 *
 * Copyright (C) 2006-2015 Tony Wu (tonywu@realtek.com)
 */
#ifndef _PLAT_CPU_H_
#define _PLAT_CPU_H_

#define cpu_mem_size		(256 << 20)

#define cpu_icache_size		(64 << 10)
#define cpu_dcache_size		(32 << 10)
#define cpu_scache_size		(256 << 10)
#define cpu_scache_line		32
#define cpu_tlb_entry		64
#define cpu_imem_size		0
#define cpu_dmem_size		0
#define cpu_smem_size		0

#define cpu_icache_line		32
#define cpu_dcache_line		32

/* wmpu entries are either 0, 4, or 8 */
#define BSP_WATCH_NUM		8

#endif
