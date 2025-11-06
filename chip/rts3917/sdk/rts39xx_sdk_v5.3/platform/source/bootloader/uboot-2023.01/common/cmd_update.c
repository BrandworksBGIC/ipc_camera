// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
 *
 */
#include <common.h>
#include <command.h>
#include <spi_flash.h>
#include <env.h>
#include <env_internal.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <net.h>
#include <dfu.h>
#include <cpu_func.h>
#include <dfu.h>
#include <configs/rlxboard.h>
#include <fat.h>
#include <fs.h>
#include <mmc.h>

#define GETMEM(addr)	(*(volatile u32 *)(addr))
static char *tmpfile;

u32 get_dtb_data_of_offset(const void *data, int len)
{
	if (len == 0)
		return -1;

	if ((len % 4) == 0) {
		const __be32 *p = data;

		return fdt32_to_cpu(p[0]);
	}
	return -1;
}

static u32 get_dtb_data_of_size(const void *data, int len)
{
	if (len == 0)
		return -1;

	if ((len % 4) == 0) {
		const __be32 *p = data;

		return fdt32_to_cpu(p[1]);
	}
	return -1;
}

int fdt_node_check_label(const void *fdt, int nodeoffset,
			      const char *label)
{
	const void *prop;
	int len;

	prop = fdt_getprop(fdt, nodeoffset, "label", &len);
	if (!prop)
		return len;

	return !fdt_stringlist_contains(prop, len, label);
}

int fdt_node_offset_by_label(const void *fdt, int startoffset,
				  const char *label)
{
	int offset, err;

	/* FIXME: The algorithm here is pretty horrible: we scan each
	 * property of a node in fdt_node_check_compatible(), then if
	 * that didn't find what we want, we scan over them again
	 * making our way to the next node.  Still it's the easiest to
	 * implement approach; performance can come later.
	 */
	for (offset = fdt_next_node(fdt, startoffset, NULL);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		err = fdt_node_check_label(fdt, offset, label);
		if ((err < 0) && (err != -FDT_ERR_NOTFOUND))
			return err;
		else if (err == 0)
			return offset;
	}

	return offset; /* error from fdt_next_node() */
}

int fdt_get_mtd_index_by_label(const void *fdt, int startoffset,
				  const char *label, int *mtd_id)
{
	int offset, err;
	int count = 0;

	for (offset = fdt_next_node(fdt, startoffset, NULL);
		offset >= 0;
		offset = fdt_next_node(fdt, offset, NULL)) {
		err = fdt_node_check_label(fdt, offset, label);
		if ((err < 0) && (err != -FDT_ERR_NOTFOUND))
			return err;
		else if (err == 0) {
			*mtd_id = count;
			return 0;
		}
		if (err > 0)
			count++;
	}

	return -1;
}

int _do_get_partitions(const char *label, int *offset, int *size)
{
	struct fdt_header *working_fdt;
	int nodeoffset;	/* node offset from libfdt */
	int nodeoffset_s;
#ifdef CONFIG_RTS_EMMC_BOOT
	char *pathp = "/soc/mmc_sdhc/partitions";
#else
	char *pathp = "/soc/spic/partitions";
#endif
	char *prop = "reg";
	const void *nodep;
	int len;

	working_fdt = (struct fdt_header *)env_get_hex("fdtcontroladdr", 0);
	if (!fdt_valid(&working_fdt))
		return -1;

	nodeoffset = fdt_path_offset(working_fdt, pathp);
	if (nodeoffset < 0) {
		printf("libfdt can't find kernel label returned\n");
		return -1;
	}
	nodeoffset_s = fdt_node_offset_by_label(working_fdt, nodeoffset, label);
	if (!(nodeoffset_s > 0))
		return -1;
	nodep = fdt_getprop(working_fdt, nodeoffset_s, prop, &len);
	if (nodep && len > 0) {
		*offset = get_dtb_data_of_offset(nodep, len);
		*size = get_dtb_data_of_size(nodep, len);
		if (*offset < 0 || *size < 0)
			return -1;
	}
	printf("libfdt fdt_getprop():\n");
	return 0;
}

static int _do_write_file_(u32 offset, u32 len, u32 loadaddr,
				u32 partition_size, u32 magic_num)
{
	int ret = 0;
#ifdef CONFIG_RTS_EMMC_BOOT
	struct mmc *mmc;
	int dev = mmc_get_env_dev();
	uint blk_start, blk_cnt, n;
#else
	char *buf;

	buf = (char *)loadaddr;
#endif
	if (len == 0)
		return CMD_RET_FAILURE;

	if (partition_size == 0)
		partition_size = len;

#if defined CONFIG_RTS_QSPI
	ret = spi_flash_update_external(offset, len, buf);
#endif

#ifdef CONFIG_RTS_SPI_NAND_FLASH
	ret = update_image_for_nand(offset, len, (unsigned char *)buf,
			partition_size);
#endif

#ifdef CONFIG_RTS_EMMC_BOOT
	mmc = find_mmc_device(dev);
	if ((strcmp(tmpfile, "/u-boot.bin") == 0) || magic_num == 0x626f6f74) {
		blk_select_hwpart_devnum(UCLASS_MMC, dev, 1);
		mmc_set_part_conf(mmc, 0, 1, 1);
	} else {
		blk_select_hwpart_devnum(UCLASS_MMC, 0, 0);
		mmc_set_part_conf(mmc, 0, 0, 0);
	}

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return -1;
	}
	blk_start = ALIGN(offset, mmc->read_bl_len) / mmc->read_bl_len;
	blk_cnt = ALIGN(len, mmc->read_bl_len) / mmc->read_bl_len;
	printf("start is %x, cnt is %x\n", blk_start, blk_cnt);
	blk_derase(mmc_get_blk_desc(mmc), blk_start, blk_cnt);
	n = blk_dwrite(mmc_get_blk_desc(mmc), blk_start, blk_cnt,
		(void *)loadaddr);

	if (n == blk_cnt)
		ret = 0;
	else
		ret = -1;

	blk_select_hwpart_devnum(UCLASS_MMC, dev, 1);
	mmc_set_part_conf(mmc, 0, 1, 1);
#endif

	env_set("fileaddr", NULL);
	return ret;
}

#ifndef CONFIG_FAST_BOOT
static u32 _do_get_file_(char *filename)
{
	u32 len;

	tmpfile = filename;

	//if  (net_loop(TFTPSRV) < 0)
	//	return 0;

	len = env_get_hex("filesize", 0);

	env_set("filesize", NULL);
	return len;
}

static int do_update_uboot(const char *dfu)
{
	int ret = 0;
	u32 len = 0;
	u32 offset = 0;
	u32 loadaddr = CONFIG_SYS_LOAD_ADDR;

#ifdef CONFIG_CMD_DFU
	if (strcmp(dfu, "dfu") == 0) {
		char *argv[4] = {"dfu", "0", "ram", "0"};
		char **p = argv;

		ret = do_dfu(NULL, 0, 4, p);
		if (ret)
			return CMD_RET_FAILURE;
		len = dfu_all_len;
		dfu_all_len = 0;
	} else
#endif
		// len = _do_get_file_("/u-boot.bin");

	ret = _do_write_file_(offset, len, loadaddr, 0, 0);

	debug("get %s from %x , write %x byte to offset %x\n",
		tmpfile, loadaddr, len, offset);
	return ret;
}

static int do_update_kernel(const char *dfu)
{
	int ret = 0;
	u32 offset, size;
	u32 len = 0;
	u32 loadaddr = CONFIG_SYS_LOAD_ADDR;
	char *label = "kernel";

	ret = _do_get_partitions(label, &offset, &size);
	if (ret < 0)
		return 1;
	printf("update kernel to offset %x, partition size: %x\n",
		offset, size);

#ifdef CONFIG_CMD_DFU
	if (strcmp(dfu, "dfu") == 0) {
		char *argv[4] = {"dfu", "0", "ram", "0"};
		char **p = argv;

		ret = do_dfu(NULL, 0, 4, p);
		if (ret)
			return CMD_RET_FAILURE;
		len = dfu_all_len;
		dfu_all_len = 0;
	} else
#endif
		len = _do_get_file_("/vmlinux.img");

#ifdef CONFIG_CRYPTO_UPDATE
	len = patchkernelimg(loadaddr, len);
#endif
	ret = _do_write_file_(offset, len, loadaddr, size, 0);

	debug("get %s from %x , write %x byte to offset %x\n",
		tmpfile, loadaddr, len, offset);
	return ret;
}

#ifdef SPI_ROOTFS_OFFSET
static int do_update_rootfs(void)
{
	int ret = 0;
	u32 offset;
	u32 size;
	u32 len = 0;
	u32 loadaddr = CONFIG_SYS_LOAD_ADDR;
	char *label = "rootfs";

	ret = _do_get_partitions(label, &offset, &size);
	if (ret < 0)
		return 1;
	printf("update rootfs to offset %x, partition size: %x\n",
		offset, size);
	len = _do_get_file_("/rootfs.bin");
	ret = _do_write_file_(offset, len, loadaddr, size, 0);

	debug("get %s from %x , write %x byte to offset %x\n",
		tmpfile, loadaddr, len, offset);
	return ret;
}
#endif

static int _do_write_all_(u32 all_len)
{
	int ret = 0;
	u32 magic_num;
	u32 ram_offset = 0x100;
	u32 flash_offset = 0;
	u32 filelen;
	u32 partition_size = 0;
#ifdef CONFIG_CRYPTO_UPDATE
	u32 klen;
	u32 addr;
	u32 all_len_temp;

	all_len_temp = all_len;
#endif

	all_len -= 0x100;
	while (all_len != 0) {
		magic_num = htonl(GETMEM(CONFIG_SYS_LOAD_ADDR + ram_offset));
		debug("%x:%x\n", (CONFIG_SYS_LOAD_ADDR + ram_offset), magic_num);
		partition_size = htonl(GETMEM(CONFIG_SYS_LOAD_ADDR +
				ram_offset + 0x0C));
		flash_offset = htonl(GETMEM(CONFIG_SYS_LOAD_ADDR + ram_offset + 0x14));
		filelen = htonl(GETMEM(CONFIG_SYS_LOAD_ADDR + ram_offset + 0x1C));

		switch (magic_num) {
		/*uboot*/
		case 0x626f6f74:

		/*MCU fw*/
		case 0x6669726d:

		/*hw config*/
		case 0x68617264:

		/*sw config*/
		case 0x6a667332:

		/*kernel*/
		case 0x6c696e78:

		/*rootfs*/
		case 0x726f6f74:

		/*ldc table*/
		case 0x6c646300:

		/*gpt head*/
		case 0x67707468:

		/*gpt tail*/
		case 0x67707474:
			printf("offset is %x\n", flash_offset);
			break;

		/*file end*/
		case 0x46454f46:
			printf("file end\n");
			return 0;

		default:
			printf("user defined magic number %x at %x\n",
			magic_num, (CONFIG_SYS_LOAD_ADDR + ram_offset));
			break;
		}
#ifdef CONFIG_CRYPTO_UPDATE
		if (flash_offset == SPI_KERNEL_OFFSET) {
			addr = CONFIG_SYS_LOAD_ADDR + all_len_temp;
			addr = (addr + 0x3) & ~0x3;
			memcpy((void *)addr,
			(void *)(CONFIG_SYS_LOAD_ADDR + ram_offset + 0x20), filelen);
			klen = patchkernelimg(addr, filelen);
			ret = _do_write_file_(flash_offset, klen, addr,
						partition_size, magic_num);
		} else
#endif
			ret = _do_write_file_(flash_offset, filelen,
				(CONFIG_SYS_LOAD_ADDR + ram_offset + 0x20),
					partition_size, magic_num);

		if (ret != 0)
			return ret;
		printf("get %x file from %x, wtite %x bytes to flash offset %x\n",
			magic_num, ram_offset, filelen, flash_offset);
		all_len -= (filelen + 0x24);
		ram_offset += (filelen + 0x24);
	}
	debug("update all image from ethernet and write to flash\n");
	return ret;
}

static int do_update_all(const char *dfu)
{
	int ret = 0;
	u32 all_len = 0;

#ifdef CONFIG_CMD_DFU
	if (strcmp(dfu, "dfu") == 0) {
		char *argv[4] = {"dfu", "0", "ram", "0"};
		char **p = argv;

		ret = do_dfu(NULL, 0, 4, p);
		if (ret)
			return CMD_RET_FAILURE;
		all_len = dfu_all_len;
		dfu_all_len = 0;
	} else
#endif
		all_len = _do_get_file_("/linux.bin");

	if (all_len == 0)
		return CMD_RET_FAILURE;

	ret = _do_write_all_(all_len);

	return ret;
}

static int do_update(struct cmd_tbl  *cmdtp, int flag, int argc,
			char * const argv[])
{

	const char *cmd;
	char *dfu_argv = NULL;
	int ret = CMD_RET_FAILURE;

	/* need at least two arguments */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	if (argc == 3)
		dfu_argv = argv[2];

	if (strcmp(cmd, "uboot") == 0) {
		ret = do_update_uboot(dfu_argv);
		goto done;
	}

	if (strcmp(cmd, "kernel") == 0) {
		ret = do_update_kernel(dfu_argv);
		goto done;
	}

#ifdef SPI_ROOTFS_OFFSET
	if (strcmp(cmd, "rootfs") == 0) {
		ret = do_update_rootfs();
		goto done;
	}
#endif

	if (strcmp(cmd, "all") == 0) {
		ret = do_update_all(dfu_argv);
		goto done;
	}

done:
	if (ret == CMD_RET_SUCCESS)
		printf("update done\n\n");
	else
		printf("update failed\n\n");

	return ret;

usage:
	return CMD_RET_USAGE;
}
#endif //CONFIG_FAST_BOOT

#ifdef CONFIG_CMD_SF_TEST
#define UPDATE_TEST_HELP "\nsf test offset len		" \
		"- run a very basic destructive test"
#else
#define UPDATE_TEST_HELP
#endif

int do_write_for_rescure(void)
{
	int ret = 0;

	u32 magic_num;
	u32 ram_offset = 0x0;
	u32 flash_offset = 0;
	u32 filelen;
	u32 partition_size = 0;

	while (1) {
		magic_num = htonl(REG32(RESCUE_LINUX_LOAD_ADDR + ram_offset));
		debug("%x:%x\n", (RESCUE_LINUX_LOAD_ADDR + ram_offset),
			magic_num);
		partition_size = htonl(REG32(RESCUE_LINUX_LOAD_ADDR +
				ram_offset + 0x0C));
		flash_offset = htonl(REG32(RESCUE_LINUX_LOAD_ADDR +
			ram_offset + 0x14));
		filelen = htonl(REG32(RESCUE_LINUX_LOAD_ADDR +
			ram_offset + 0x1C));

		switch (magic_num) {
		/*uboot*/
		case 0x626f6f74:

		/*MCU fw*/
		case 0x6669726d:

		/*hw config*/
		case 0x68617264:

		/*sw config*/
		case 0x6a667332:

		/*kernel*/
		case 0x6c696e78:

		/*rootfs*/
		case 0x726f6f74:

		/*ldc table*/
		case 0x6c646300:
			printf("offset is %x\n", flash_offset);
			break;

		/*file end*/
		case 0x46454f46:
			printf("file end\n");
			return 0;

		default:
			printf("user defined magic number %x at %x\n",
			magic_num, (RESCUE_LINUX_LOAD_ADDR + ram_offset));
			break;
		}

		ret = _do_write_file_(flash_offset, filelen,
			(RESCUE_LINUX_LOAD_ADDR + ram_offset + 0x20),
				partition_size, magic_num);
		if (ret != 0)
			return ret;
		printf("get %x file from %x, wtite %x bytes to flash %x\n",
			magic_num, ram_offset, filelen, flash_offset);
		ram_offset += (filelen + 0x24);
	}

	return ret;
}

#ifdef CONFIG_UPDATE_KERNEL_FROM_SD
int load_bin_from_sd(void)
{
        unsigned long bytes;
        unsigned long pos;
        int len_read;
        unsigned long time;
        int ret;
        int *temp = &len_read;
        const char *filename  = "linux.bin";
        unsigned long loadaddr = CONFIG_SYS_LOAD_ADDR;

        if (fs_set_blk_dev("mmc", "0", FS_TYPE_FAT))
                if (fs_set_blk_dev("mmc", "1", FS_TYPE_FAT))
                        return 0;

        bytes = 0;
        pos = 0;
        time = get_timer(0);
        ret = fs_read(filename, loadaddr, pos, bytes, (loff_t *)temp);
        printf("in load_bin_from_sd\n");
        time = get_timer(time);
        if (ret < 0)
                return 0;

	printf("%d bytes read in %lu ms", len_read, time);
        if (time > 0) {
                puts(" (");
                print_size(len_read / time * 1000, "/s");
                puts(")");
        }
        puts("\n");

        _do_write_all_(len_read);

        return 0;
}
#endif

#ifdef CONFIG_CRYPTO_BOOT
int load_key_from_sd(void)
{
	unsigned long addr;
	const char *filename;
	unsigned long bytes;
	unsigned long pos;
	int len_read;
	unsigned long time;
	int ret;
	int *temp = &len_read;

	if (fs_set_blk_dev("mmc", "0", FS_TYPE_FAT))
		if (fs_set_blk_dev("mmc", "1", FS_TYPE_FAT))
			return 0;

	addr = CONFIG_SYS_LOAD_ADDR;
	printf("load key from sd addr %lx\n", addr);

	filename = "ipcam_crypto.bin";

	bytes = 0;
	pos = 0;
	time = get_timer(0);
	ret = fs_read(filename, addr, pos, bytes, (loff_t *)temp);
	printf("in load_key_from_sd\n");
	time = get_timer(time);
	if (ret < 0)
		return 0;

	printf("%d bytes read in %lu ms", len_read, time);
	if (time > 0) {
		puts(" (");
		print_size(len_read / time * 1000, "/s");
		puts(")");
	}
	puts("\n");

	return 0;
}

int load_iv_from_sd(void)
{
	unsigned long addr;
	const char *filename;
	unsigned long bytes;
	unsigned long pos;
	int len_read;
	unsigned long time;
	int ret;
	int *temp = &len_read;

	if (fs_set_blk_dev("mmc", "0", FS_TYPE_FAT))
		if (fs_set_blk_dev("mmc", "1", FS_TYPE_FAT))
			return 0;

	addr = CONFIG_SYS_LOAD_ADDR;
	printf("load iv from sd addr %lx\n", addr);

	filename = "ipcam_crypto_iv.bin";

	bytes = 0;
	pos = 0;
	time = get_timer(0);
	ret = fs_read(filename, addr, pos, bytes, (loff_t *)temp);
	printf("in load_iv_from_sd\n");
	time = get_timer(time);
	if (ret < 0)
		return 0;

	printf("%d bytes read in %lu ms", len_read, time);
	if (time > 0) {
		puts(" (");
		print_size(len_read / time * 1000, "/s");
		puts(")");
	}
	puts("\n");

	return 0;
}
#endif

#ifndef CONFIG_FAST_BOOT
U_BOOT_CMD(
	update,	3,	0,	do_update,
	"update xxx",
	"uboot	- get uboot.bin by tftp and write to flash\n"
	"update kernel	- get kernel image by tftp and write to flash\n"
	"update rootfs	- get rootfs by tftp and write to flash\n"
	"update all	- get all image by tftp , parse it and  write to flash\n"
	UPDATE_TEST_HELP
);
#endif
