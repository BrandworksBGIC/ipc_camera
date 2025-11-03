/*
 * Realtek Semiconductor Corp.
 *
 * bsp/include/mach/uncompress.h:
 *
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */
#ifndef _UNCOMPRESS_H_
#define _UNCOMPRESS_H_

#include <linux/serial_reg.h>

volatile unsigned long *uart;

static void putc(int c)
{
	while (!(uart[UART_LSR] & UART_LSR_THRE))
		barrier();

	uart[UART_TX] = c;
}

static inline void flush(void)
{
	while (!(uart[UART_LSR] & UART_LSR_THRE))
		barrier();
}

static inline void early_uart_init(void)
{
	uart[UART_LCR] = 0x80;
	uart[UART_IER] = 0x0;
	/*
	 *  baud rate = (serial clock freq) / (16 * divisor)
	 *  baud rate = 57600
	 *  serial clock freq = 25MHz
	 */
	uart[UART_TX] = 0x1b;
	uart[UART_LCR] = 0x3;
}

static inline void __arch_decomp_setup(unsigned long arch_id)
{
	uart = (unsigned long *)CONFIG_DEBUG_UART_PHYS;
	early_uart_init();
}

#define arch_decomp_setup()	__arch_decomp_setup(arch_id)

/*
 * nothing to do
 */
#define arch_decomp_wdog()

#endif /* mach/uncompress.h */
