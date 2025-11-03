/*
 * Command for accessing Ethernet otp.
 *
 * Copyright (C) Realsil Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include "bspchip.h"

#define USB_SLBTEST		0x18290030
#define USB_DPHY_CFG1		0x18290804
#define USB_DPHY_STS		0x1829080c

#define get_val(addr)		REG32(addr)
#define set_val(addr, val)	REG32(addr) = (val)

static int slb_test(char *type, int hs_en, int mode, int seed)
{
	u32 rdata, wdata;

	printf("%s ", type);
	rdata = get_val(USB_DPHY_CFG1);
	if (hs_en)
		wdata = (rdata & 0xffe01ff) | 0x1 << 10 | 0x1 << 9; //utmi cfg
	else
		wdata = (rdata & 0xffe01ff) | 0x1 << 16 | 0x1 << 15 |
			0x1 << 13 | 0x1 << 10 | 0x1 << 9; //utmi cfg
	set_val(USB_DPHY_CFG1, wdata);

	rdata = get_val(USB_DPHY_CFG1);
	wdata = (rdata & 0x3fffff) | seed << 24 | mode << 22; // slb mode & seed
	set_val(USB_DPHY_CFG1, wdata);

	rdata = get_val(USB_DPHY_CFG1);
	wdata = rdata | 0x1 << 21 | 0x1 << 20; // slb rst
	set_val(USB_DPHY_CFG1, wdata);

	wdata = (rdata & 0xffcfffff) | 0x1 << 20; //slb en
	set_val(USB_DPHY_CFG1, wdata);

	mdelay(10);

	rdata = get_val(USB_DPHY_STS);
	if ((rdata & 0x1) == 0)
		printf("SLB done bit is not set\n");
	else if ((rdata & 0x2) == 0x2)
		printf("SLB error bit is set\n");
	else
		printf("Pass!\n");

	rdata = get_val(USB_DPHY_CFG1);
	wdata = rdata & 0xffefffff; // clear slb_en
	set_val(USB_DPHY_CFG1, wdata);

	return 0;
}

static int do_usb_slb(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i, ret = 0;
	char *cmd;
	char *slb_type[4] = {"all zeros", "all ones", "pseudo random",
			     "incremental counter"};
	int slb_mode[4] = {0, 1, 2, 3};
	int slb_seed[4] = {0, 0xff, 0x1, 0};

	if (argc < 2)
		return -1;
	cmd = argv[1];

	while ((get_val(USB_DPHY_STS) & 0x400) != 0x400)
		;

	mdelay(1000);
	printf("PLL ready!\n");

	if (strcmp(cmd, "high") == 0) {
		for (i = 0; i < 4; i++) {
			printf("SLB Hogh Speed Test ");
			slb_test(slb_type[i], 1, slb_mode[i], slb_seed[i]);
		}
	} else if (strcmp(cmd, "full") == 0) {
		for (i = 0; i < 4; i++) {
			printf("SLB Full Speed Test ");
			slb_test(slb_type[i], 0, slb_mode[i], slb_seed[i]);
		}
	}
	return ret;
}

U_BOOT_CMD(
	rts_usb_slb, 2, 0, do_usb_slb,
	"rts_usb_slb high/full",
	"rts_usb_slb high - usb device Self-Loop-Back high speed test\n"
	"rts_usb_slb full - usb device Self-Loop-Back full speed test\n"
);
