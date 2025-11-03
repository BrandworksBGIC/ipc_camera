// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <linux/mtd/mtd.h>
#include <command.h>
#include <watchdog.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <net.h>
#include <mapmem.h>
#include <nand.h>
#include <image.h>

int get_data_length_from_nand(ulong offset, ulong *data_length)
{
	const struct legacy_img_hdr *hdr;
	struct mtd_info *nand;
	size_t read_len;
	ulong addr;
	int ret;

	read_len = sizeof(struct legacy_img_hdr);
	addr = CONFIG_SYS_LOAD_ADDR;
	nand = get_nand_dev_by_index(0);
	if (!nand)
		return 1;
	printf("nand read header offset is %#lx, read_len is %#x\n",
	       offset, read_len);
	ret = nand_read_skip_bad(nand, offset, &read_len,
				 NULL, nand->size, (u_char *)addr);

	hdr = (struct legacy_img_hdr *)addr;

	*data_length = htonl(hdr->ih_size);
	printf("data length is %#lx\n", *data_length);

	return ret;
}

int copy_imagedata_to_ram(ulong offset, ulong ram_addr, ulong data_length)
{
	struct mtd_info *nand;
	u32 read_len;
	int ret;

	nand = get_nand_dev_by_index(0);
	if (!nand)
		return 1;
	if (IS_ENABLED(CONFIG_FIT))
		read_len = data_length + 0x2000;
	else
		read_len = data_length + 64;

	printf("image: nand read data from offset ");
	printf("%#lx to  %#lx, length is  %#x, limit is %#llx\n",
	       offset, ram_addr, read_len, nand->size);

	ret = nand_read_skip_bad(nand, offset, &read_len,
				 NULL, nand->size, (u_char *)ram_addr);

	return ret;
}

int copy_fdt_to_ram(ulong offset, ulong ram_addr, ulong data_length)
{
	struct mtd_info *nand;
	u32 read_len;
	int ret;

	nand = get_nand_dev_by_index(0);
	if (!nand)
		return 1;
	read_len = data_length;

	printf("fdt: nand read data from offset ");
	printf("%#lx to  0x%lx, length is  %#x, limit is %#llx\n",
	       offset, ram_addr, read_len, nand->size);

	ret = nand_read_skip_bad(nand, offset, &read_len,
				 NULL, nand->size, (u_char *)ram_addr);

	return ret;
}

int update_image_to_nand(ulong offset, size_t length, unsigned char *buf)
{
	struct mtd_info *nand;
	nand_erase_options_t opts;
	int ret;

	memset(&opts, 0, sizeof(opts));
	/*printf("offset is %x, len is %x, buf is 0x%x\n",
	 * offset, length, buf);
	 */

	nand = get_nand_dev_by_index(0);
	if (!nand)
		return 1;
	opts.offset = offset;
	/*
	 *if (length%nand->erasesize)
	 *	opts.length = (length + nand->erasesize)
	 *	& (~(nand->erasesize -1));
	 *else
	 */
	opts.length = length;
	/*erase size does not include bad block size*/
	opts.spread = 1;
	/*	printf("erase offset is 0x%x, len is 0x%x\n",
	 *	opts.offset, opts.length);
	 */
	ret = nand_erase_opts(nand, &opts);
	ret = nand_write_skip_bad(nand, offset,  &length,
				  NULL, nand->size, buf, 0);

	return ret;
}

int update_image_for_nand(ulong offset, size_t length, unsigned char *buf,
			  size_t erase_length)
{
	struct mtd_info *nand;
	nand_erase_options_t opts;
	int ret;

	memset(&opts, 0, sizeof(opts));
	/*printf("offset is %x, len is %x, buf is 0x%x\n",
	 * offset, length, buf);
	 */

	nand = get_nand_dev_by_index(0);
	if (!nand)
		return 1;
	opts.offset = offset;
	opts.length = erase_length;
	/*erase size does not include bad block size*/
	opts.spread = 1;
	/*	printf("erase offset is 0x%x, len is 0x%x\n",
	 *	opts.offset, opts.length);
	 */
	ret = nand_erase_opts(nand, &opts);
	ret = nand_write_skip_bad(nand, offset,  &length,
				  NULL, nand->size, buf, 0);

	return ret;
}
