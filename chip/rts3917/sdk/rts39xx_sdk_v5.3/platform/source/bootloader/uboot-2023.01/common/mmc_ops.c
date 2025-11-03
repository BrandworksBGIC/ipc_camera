#include <common.h>
#include <linux/mtd/mtd.h>
#include <command.h>
#include <image.h>
#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <net.h>
#include <mapmem.h>
#include <mmc.h>

int get_data_length_from_mmc(ulong offset, ulong *data_length)
{
	const struct legacy_img_hdr *hdr;
	struct mmc *mmc;
	ulong addr;
	int ret = 0;

	addr = CONFIG_SYS_LOAD_ADDR;
	mmc = find_mmc_device(0);
	blk_select_hwpart_devnum(UCLASS_MMC, 0, 0);
	mmc_set_part_conf(mmc, 0, 0, 0);
	printf("mmc read header offset is %lu\n", offset);
	blk_dread(mmc_get_blk_desc(mmc), (offset >> 9), 1, (void *)addr);

	hdr = (struct legacy_img_hdr *)addr;

	*data_length = htonl(hdr->ih_size);
	printf("data length is %lx\n", *data_length);

	blk_select_hwpart_devnum(UCLASS_MMC, 0, 1);
	mmc_set_part_conf(mmc, 0, 1, 1);
	return ret;
}

int copy_mmcdata_to_ram(ulong offset, ulong ram_addr, ulong data_length)
{
	struct mmc *mmc;
	u32 read_len, n = 0;
	int ret = 0;

	read_len = data_length + 64;
	mmc = find_mmc_device(0);

	blk_select_hwpart_devnum(UCLASS_MMC, 0, 0);
	mmc_set_part_conf(mmc, 0, 0, 0);

	printf("emmc read data from offset 0x%lx to  0x%lx, length is  0x%x\n",
	       offset, ram_addr, read_len);

	blk_dread(mmc_get_blk_desc(mmc), (offset >> 9),
		  (read_len >> 9) + 2, (void *)ram_addr);

	if (n == ((read_len >> 9) + 2))
		ret = 0;
	else
		ret = -1;

	blk_select_hwpart_devnum(UCLASS_MMC, 0, 1);
	mmc_set_part_conf(mmc, 0, 1, 1);
	return ret;
}

