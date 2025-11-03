#ifndef __RTS_XB2_TIMER__
#define __RTS_XB2_TIMER__

#include <linux/io.h>
#include "ty_typedefs.h"

#define XB2_PHY_BASE_ADDR   0x18890000
#define XB2_PHY_ADDR_LEN    0x100

#define MODE_ONESHOT		0x00
#define MODE_PERIODIC		0x01

#define US_TO_PERIOD_VALUE(us)		(us * 25)

struct timer_node {
	void __iomem *io_base;
	unsigned int timer_period;
	int irq;
};

struct xb2_timer_reg {
	INT rts_timer0_en;
	INT rts_timer0_compare;
	INT rts_timer0_mode;
	INT rts_timer0_int_en;
	INT rts_timer0_int_sts;
	INT rts_irq_timer0;
};

void rts_xb2_timer_enable(struct timer_node *ptimer, struct xb2_timer_reg *reg);

int rts_xb2_timer_int_sts(struct timer_node *ptimer, struct xb2_timer_reg *reg);

void rts_xb2_timer_clr_int_sts(struct timer_node *ptimer, struct xb2_timer_reg *reg);

void rts_xb2_timer_disable(struct timer_node *ptimer, struct xb2_timer_reg *reg);

#endif
