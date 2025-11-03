// SPDX-License-Identifier: GPL-2.0+
#include <linux/kconfig.h>
#include <command.h>
#include <lzma/LzmaDec.h>
#include <lzma/LzmaTools.h>
#include <malloc.h>

void decompress_lzma(void)
{
	u32 destLen = 0x100000, len;
	u32 destAddr, srcAddr;

	destAddr = CONFIG_LOAD_BASE;
	srcAddr = CONFIG_DELZMA_FUNC_BASE + CONFIG_DELZMA_FUNC_SIZE;
	len = CONFIG_RAM_SIZE_COMP;

	mem_malloc_init(CONFIG_DELZMA_FUNC_BASE + 0xf00000, 0xf00000);

	lzmaBuffToBuffDecompress((unsigned char *)destAddr,
				 &destLen, (unsigned char *)srcAddr, len);
}

int raise(int a)
{
	return 0;
}

void puts(const char *s)
{
	return;
}
