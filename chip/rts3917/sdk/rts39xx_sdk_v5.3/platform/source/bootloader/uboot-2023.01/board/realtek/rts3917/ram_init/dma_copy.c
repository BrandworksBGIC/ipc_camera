#include <linux/kconfig.h>
#include <linux/types.h>
#include <dma.h>
#include <config.h>

#define REG32(reg)      (*(volatile u32 *)(reg))

static void write_enable(void)
{
	REG32(SPIC_VA + SPIC_SSIENR) = 0x0;
	REG32(SPIC_VA + SPIC_CTRLR0) = 0xc300001f;
	REG32(SPIC_VA + SPIC_DR) = 0x06;
	REG32(SPIC_VA + SPIC_USER_LENGTH) = 0x1000;
	REG32(SPIC_VA + SPIC_RX_NDF) = 0x0;
	REG32(SPIC_VA + SPIC_TX_NDF) = 0x0;
	REG32(SPIC_VA + SPIC_SSIENR) = 0x1;
	while (1) {
		if (REG32(SPIC_VA + SPIC_SSIENR) == 0)
			break;
	}
}

static void wait_flash_busy(void)
{
	while (1) {
		REG32(SPIC_VA + SPIC_SSIENR) = 0x0;
		REG32(SPIC_VA + SPIC_CTRLR0) = 0xc200031f;
		REG32(SPIC_VA + SPIC_RX_NDF) = 0x1;
		REG32(SPIC_VA + SPIC_TX_NDF) = 0x0;
		REG32(SPIC_VA + SPIC_AUTO_LENGTH) |= (2 << 12);
		REG32(SPIC_VA + SPIC_USER_LENGTH) = 0x1000;
		REG32(SPIC_VA + SPIC_DR) = 0x05;
		REG32(SPIC_VA + SPIC_SSIENR) = 0x1;
		while (1) {
			if (REG32(SPIC_VA + SPIC_SSIENR) == 0)
				break;
		}
		if ((REG32(SPIC_VA + SPIC_DR) & 0x1) == 0)
			break;
	}
}

static u32 read_status_register(u32 cmd)
{
	u32 rdata;

	REG32(SPIC_VA + SPIC_SSIENR) = 0x0;
	REG32(SPIC_VA + SPIC_CTRLR0) = 0xc200031f;
	REG32(SPIC_VA + SPIC_RX_NDF) = 0x1;
	REG32(SPIC_VA + SPIC_TX_NDF) = 0x0;
	REG32(SPIC_VA + SPIC_AUTO_LENGTH) |= (2 << 12);
	REG32(SPIC_VA + SPIC_USER_LENGTH) = 0x1000;
	REG32(SPIC_VA + SPIC_DR) = cmd;
	REG32(SPIC_VA + SPIC_SSIENR) = 0x1;
	while (1) {
		if (REG32(SPIC_VA + SPIC_SSIENR) == 0)
			break;
	}
	rdata = REG32(SPIC_VA + SPIC_DR);

	return rdata;
}

static void set_qe_bit(u32 value)
{
	REG32(SPIC_VA + SPIC_SSIENR) = 0x0;
	REG32(SPIC_VA + SPIC_CTRLR0) = 0xc300001f;
	REG32(SPIC_VA + SPIC_USER_LENGTH) = 0x1000;
	REG32(SPIC_VA + SPIC_DR) = value;
	REG32(SPIC_VA + SPIC_RX_NDF) = 0x0;
	REG32(SPIC_VA + SPIC_TX_NDF) = 0x1;
	REG32(SPIC_VA + SPIC_SSIENR) = 0x1;
	while (1) {
		if (REG32(SPIC_VA + SPIC_SSIENR) == 0)
			break;
	}
}

static void nand_fastboot(void)
{
	u16 data = 0;
	u32 tmp = 0;
	/*write enable*/
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_UCMD) = 0x06;
	REG32(SNFC_VA + SNFC_ADDR_LEN) = 0x0;
	REG32(SNFC_VA + SNFC_UADDR) = 0x0;
	REG32(SNFC_VA + SNFC_CTRLR0) = 0x0;
	REG32(SNFC_VA + SNFC_PROGRAM_DUMMY_LEN) = 0x0;
	REG32(SNFC_VA + SNFC_DATA_LEN) = 0x0;
	REG32(SNFC_VA + SNFC_DA) = 0x0;
	REG32(SNFC_VA + SNFC_SSIENR) = 0x1;
	while (1) {
		if ((REG32(SNFC_VA + SNFC_SR) & 0x1) == 0x1)
			break;
	}
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	/*read status register*/
	while (1) {
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_UCMD) = 0x0f;
	REG32(SNFC_VA + SNFC_ADDR_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_UADDR) = 0xc0;
	data = REG32(SNFC_VA + SNFC_READ_DUMMY_LEN);
	REG32(SNFC_VA + SNFC_CTRLR0) = 0xc;
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = 0xc;
	REG32(SNFC_VA + SNFC_DATA_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_DA) = 0x0;
	REG32(SNFC_VA + SNFC_SSIENR) = 0x1;
	while (1) {
		if ((REG32(SNFC_VA + SNFC_SR) & 0x1) == 0x1)
			break;
	}
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = data;
	tmp = REG32(SNFC_VA + SNFC_DV);
	if ((tmp & 0x1) == 0x0)
		break;
	}
	/*set QE bit*/
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_UCMD) = 0x1f;
	REG32(SNFC_VA + SNFC_ADDR_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_UADDR) = 0xb0;
	REG32(SNFC_VA + SNFC_CTRLR0) = 0x0;
	REG32(SNFC_VA + SNFC_PROGRAM_DUMMY_LEN) = 0x0;
	REG32(SNFC_VA + SNFC_DATA_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_DA) = 0x0;
	REG32(SNFC_VA + SNFC_DV) = 0x1;
	REG32(SNFC_VA + SNFC_SSIENR) = 0x1;
	while (1) {
		if ((REG32(SNFC_VA + SNFC_SR) & 0x1) == 0x1)
			break;
	}
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	/* read OTP register*/
	while (1) {
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_UCMD) = 0x0f;
	REG32(SNFC_VA + SNFC_ADDR_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_UADDR) = 0xb0;
	data = REG32(SNFC_VA + SNFC_READ_DUMMY_LEN);
	REG32(SNFC_VA + SNFC_CTRLR0) = 0xc;
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = 0xc;
	REG32(SNFC_VA + SNFC_DATA_LEN) = 0x1;
	REG32(SNFC_VA + SNFC_DA) = 0x0;
	REG32(SNFC_VA + SNFC_SSIENR) = 0x1;
	while (1) {
		if ((REG32(SNFC_VA + SNFC_SR) & 0x1) == 0x1)
			break;
	}
	REG32(SNFC_VA + SNFC_SSIENR) = 0x0;
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = data;
	tmp = REG32(SNFC_VA + SNFC_DV);
	if ((tmp & 0x1) == 0x1)
		break;
	}
	REG32(SNFC_VA + SNFC_CTRLR0) = 0x8c;
	/*set baud rate*/
	REG32(SNFC_VA + SNFC_BAUDR) = 0x2;
	REG32(SNFC_VA + SNFC_RANDOM_READ_CMD) = 0x6b;
	REG32(SNFC_VA + SNFC_AUTO_READ_TYPE) = 0x4;
	REG32(SNFC_VA + SNFC_GET_FEATURE_DUMMY_LEN) = 0x6;
	/*set auto length*/
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = 0x26;
}

void fast_copy(void)
{
	u32 len, dest_addr, src_addr;
	u32 rdata, wdata, tmp;
#ifdef CONFIG_TARGET_ASIC
#ifdef CONFIG_RTS_NOR_BOOT
#ifdef CONFIG_FAST_BOOT

	/*set baud rate , dramc clock/(4*2) */
	REG32(SPIC_VA + SPIC_BAUDR) = 0x02;

	/*set dual_io  read command*/
	REG32(SPIC_VA + SPIC_READ_DUAL_DATA) = 0x6b;

	/*set dual_io read dummy to : baud *2 * dummy */
	/*set pad delay(IN_PHYSICAL_CYC) to 2 */
	REG32(SPIC_VA + SPIC_AUTO_LENGTH) = 0x00032020;

	/*set dual_io auto read valid*/
	REG32(SPIC_VA + SPIC_VALID_CMD) = 0x4008;

	/*try quad read. If error occurs, set QE bit in status register1 bit6 */
	if (REG32(0x0) != 0xea000014) {
		rdata = read_status_register(0x05);
		wdata = (((rdata | (1 << 6)) << 8) | 0x01);
		write_enable();
		set_qe_bit(wdata);
		wait_flash_busy();

		/* if error occurs again, set QE bit in status register2 bit1 */
		if (REG32(0x0) != 0xea000014) {
			tmp = read_status_register(0x35);
			tmp = (((tmp | (1 << 1)) << 8) | 0x31);
			write_enable();
			set_qe_bit(tmp);
			wait_flash_busy();

			/* clear the wrongly set QE bit in status register1 */
			wdata = (((rdata & ~(1 << 6)) << 8) | 0x01);
			write_enable();
			set_qe_bit(wdata);
			wait_flash_busy();
		}
	}
#else
	/*set baud rate , dramc clock/(4*2) */
	REG32(SPIC_VA + SPIC_BAUDR) = 0x04;

	/*set dual_o  read command*/
	REG32(SPIC_VA + SPIC_READ_DUAL_DATA) = 0x3b;

	/*set dual_o read dummy to : baud *2 * dummy */
	/*set pad delay(IN_PHYSICAL_CYC) to 2 */
	REG32(SPIC_VA + SPIC_AUTO_LENGTH) = 0x00032040;

	/*set dual_o auto read valid*/
	REG32(SPIC_VA + SPIC_VALID_CMD) = 0x4002;
#endif
#endif
#ifdef CONFIG_RTS_NAND_BOOT
#ifdef CONFIG_FIT
	/*set baud rate*/
	REG32(SNFC_VA + SNFC_BAUDR) = 8;

// 	/*set auto length*/
	REG32(SNFC_VA + SNFC_READ_DUMMY_LEN) = 0x8c;
#endif
	REG32(SNFC_VA + SNFC_AUTO_GET_FEATURES_CTRL) = SNFC_MAX_COUNT;
	while (1) {
		if (REG32(SNFC_VA + SNFC_AUTO_GET_FEATURES_CTRL)
			== SNFC_MAX_COUNT)
			break;
		}
#ifdef CONFIG_FAST_BOOT
	nand_fastboot();
#endif
#endif
#endif

	dest_addr = CONFIG_LOAD_BASE;
	src_addr = CONFIG_SYS_FLASH_BASE + CONFIG_RAM_OFFSET;
	len = CONFIG_RAM_SIZE;


	if (len % 8)
		len = (len / 8) * 8 + 8;
	else
		len = len;


	dma_copy(src_addr, dest_addr, len);

#ifdef CONFIG_TARGET_ASIC
// #ifdef CONFIG_RTS_NOR_BOOT
//	REG32(SPIC_VA + SPIC_BAUDR) = 0x08;
// #endif
#endif
}
