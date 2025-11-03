/*
 * Realtek Semiconductor Corp.
 *
 * Copyright 2012  Jethro Hsu (jethro@realtek.com)
 * Copyright 2012  Tony Wu (tonywu@realtek.com)
 */

#include <config.h>
#include <common.h>
#include <serial.h>

#include <linux/compiler.h>

#include "bspchip.h"

DECLARE_GLOBAL_DATA_PTR;
extern void write_mask(u32 reg, u32 bit, u32 mask);
/* Uart baudrate */
#define UART_BAUD_RATEL		(UART_BAUD_DEFAULT & 0x00ff)
#define UART_BAUD_RATEH		((UART_BAUD_DEFAULT & 0xff00) >> 8)
#define UART_IOBASE		UART1_IOBASE
#ifdef RS485_TEST
#define UART0_IOBASE		0xb8810000
#endif
/******************************************************************************
*
* dwc_serial_init - initialize a channel
*
* This routine initializes the number of data bits, parity
* and set the selected baud rate. Interrupts are disabled.
* Set the modem control signals if the option is selected.
*
* RETURNS: N/A
*/
static int dwc_serial_init (void)
{
	/*enable uart1 pad*/
	write_mask(UART1_PAD_CFG, UART1_TX_EN << PAD_XB2_UTXD_SHIFT,
		   PAD_XB2_UTXD_MASK);
	write_mask(UART1_PAD_CFG, UART1_RX_EN << PAD_XB2_URXD_SHIFT,
		   PAD_XB2_URXD_MASK);
	/*enable uart1 pad pull up*/
	write_mask(UART1_PULLCTRL, UART1_PE_PU << UART1_TX_PE_SHIFT,
		   UART1_TX_PE_MASK);
	write_mask(UART1_PULLCTRL, UART1_PE_PU << UART1_RX_PE_SHIFT,
		   UART1_RX_PE_MASK);

	REG32(UART_IOBASE + UART_IER) = 0;

	REG32(UART_IOBASE + UART_LCR) = UART_LCR_DLAB;
	REG32(UART_IOBASE + UART_DLL) = UART_BAUD_RATEL;
	REG32(UART_IOBASE + UART_DLH) = UART_BAUD_RATEH;

	REG32(UART_IOBASE + UART_LCR) = UART_LCR_DLS;

	if (REG32(UART_IOBASE + UART_CTR) == 0x44570110) {
		REG32(UART1_CLK_CFG_REG) = CLK_ENABLE |
		UART_SELECT_USBPLL_5 | CLOCK_SELECT_DIV4;
	}
#ifdef RS485_TEST

	REG32(UART0_IOBASE + UART_IER) = 0;

	REG32(UART0_IOBASE + UART_LCR) = UART_LCR_DLAB;
	REG32(UART0_IOBASE + UART_DLL) = UART_BAUD_RATEL;
	REG32(UART0_IOBASE + UART_DLH) = UART_BAUD_RATEH;

	REG32(UART0_IOBASE + UART_LCR) = UART_LCR_DLS;

	if (REG32(UART0_IOBASE + UART_CTR) == 0x44570110) {
		REG32(UART_CLK_CFG_REG) = CLK_ENABLE |
		UART_SELECT_USBPLL_5 | CLOCK_SELECT_DIV4;
		REG32(UART_TX_EN_REG) = UART0_TX_EN;
	}
#endif

	return 0;
}


static void dwc_serial_setbrg (void)
{
	/* Not implemented yet */
	return;
}

static void dwc_serial_putc (const char c)
{
#ifdef CONFIG_TARGET_EMULATOR
	REG32(0x18dfff00) = c;
#else
#ifdef CONFIG_SILENT_CONSOLE
#else
	volatile u32 status = 0;

	while (!status)
		status = REG32(UART_IOBASE + UART_LSR) & 0x00000020;

	REG32(UART_IOBASE+UART_THR) = c;
	if (c == '\n') {
		status = 0;
		while (!status)
			status = REG32(UART_IOBASE + UART_LSR) & 0x00000020;
		REG32(UART_IOBASE + UART_THR) = '\r';
	}
#endif
#endif
}

static void dwc_serial_puts (const char *s)
{
#ifdef CONFIG_SILENT_CONSOLE
#else
	while (*s)
		dwc_serial_putc(*s++);
#endif
}

static int dwc_serial_getc (void)
{
	volatile u32 status = 0;
	u32 ret;

	while (!status)
		status = REG32(UART_IOBASE + UART_LSR) & 0x00000001;

	ret = REG32(UART_IOBASE + UART_RBR);
	return (char)ret;
}

static int dwc_serial_tstc (void)
{
	int ret;

	ret = (REG32(UART_IOBASE + UART_LSR) & 0x00000001) ? 1 : 0;
	return ret;
}

static struct serial_device rlxboard_serial_drv = {
	.name   = "rlxboard_serial",
	.start  = dwc_serial_init,
	.stop   = NULL,
	.setbrg = dwc_serial_setbrg,
	.putc   = dwc_serial_putc,
	.puts   = dwc_serial_puts,
	.getc   = dwc_serial_getc,
	.tstc   = dwc_serial_tstc,
};

struct serial_device *default_serial_console(void)
{
	return &rlxboard_serial_drv;
}

#define read_sysreg64(op1, CRm)						\
({									\
	uint64_t val;							\
	__asm__ volatile ("mrrc p15, " #op1 ", %Q0, %R0, c"		\
			  #CRm : "=r" (val) :: "memory");		\
	val;								\
})
#define MAKE_REG64_HELPER(reg, op1, CRm)                                \
        static uint64_t read_##reg(void)                  \
        {                                                               \
                return read_sysreg64(op1, CRm);                         \
        }
MAKE_REG64_HELPER(cntvct,    1, 14);

static uint64_t prev_cyc = -1;

static void ts_delta_putc (const char c)
{
	volatile u32 status = 0;

	while (!status)
		status = REG32(UART_IOBASE + UART_LSR) & 0x00000020;

	REG32(UART_IOBASE+UART_THR) = c;
	if (c == '\n') {
		status = 0;
		while (!status)
			status = REG32(UART_IOBASE + UART_LSR) & 0x00000020;
		REG32(UART_IOBASE + UART_THR) = '\r';
	}
}
static void ts_delta_puts (const char *s)
{
	while (*s)
		ts_delta_putc(*s++);
}
enum {
	TS_DELTA_LEVEL_HIGH,
	TS_DELTA_LEVEL_LOW,
	TS_DELTA_LEVEL_MIN
};
#define TS_DELTA_LEVEL_DEF TS_DELTA_LEVEL_HIGH

void ts_delta_init()
{
	dwc_serial_init();
	prev_cyc = read_cntvct();
}

void ts_delta(int level, const char *func, int line)
{
	if ((prev_cyc != -1) &&  (level <= TS_DELTA_LEVEL_DEF)) {
		int64_t delta;
		uint64_t curr_cyc;
		char debug_str[64] = {0};

		curr_cyc = read_cntvct();
		delta = (curr_cyc - prev_cyc) / 25;
		snprintf(debug_str, sizeof(debug_str), "%s:%d delta %lld us\n", func, line, delta);
		ts_delta_puts(debug_str);

		prev_cyc = read_cntvct();
	}

	return;
}
