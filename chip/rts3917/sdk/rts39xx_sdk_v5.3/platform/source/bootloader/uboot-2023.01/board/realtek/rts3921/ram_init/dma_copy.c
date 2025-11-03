#include <linux/types.h>
#include <dma.h>
#include <config.h>

#define REG32(reg)      (*(volatile u32 *)(reg))

void fast_copy(void)
{
	u32 len, dest_addr, src_addr;

#ifdef CONFIG_TARGET_ASIC
#ifdef CONFIG_RTS_NOR_BOOT
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

#ifdef CONFIG_RTS_NAND_BOOT
	// /*set baud rate*/
	// REG32(SNFC_BAUDR) = 8;

	// /*set auto length*/
	// REG32(SNFC_AUTO_LENGTH) = 0x8c;
	REG32(SNFC_AUTO_GET_FEATURES_CTRL) = SNFC_MAX_COUNT;
	while (1) {
		if (REG32(SNFC_AUTO_GET_FEATURES_CTRL) == SNFC_MAX_COUNT)
			break;
	}

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
