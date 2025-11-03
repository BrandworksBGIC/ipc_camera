/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __RTS_UDC_H__
#define __RTS_UDC_H__

#define RTS_UDC_BASE 0x18290000
#define RTS_UDC_MC_BASE 0x18280000

struct rts_udc_plat {
	uint32_t base_addr;
	uint32_t mc_base_addr;
};
#endif /* __RTS_UDC_H__ */
