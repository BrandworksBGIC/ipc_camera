/*
 * Command for accessing Ethernet otp.
 *
 * Copyright (C) Realsil Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <asm/io.h>
#include "bspchip.h"

#define USB_SLBTEST		0x18290030
#define USB_DPHY_CFG1		0x18290804
#define USB_DPHY_STS		0x1829080c

#define U2_HOST_DPHY_CFG1	0x1814001c
#define U2_HOST_DPHY_CFG2	0x18140020

#define get_val(addr)		REG32(addr)
#define set_val(addr, val)	REG32(addr) = (val)

static int slb_test_device(char *type, int hs_en, int mode, int seed)
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

static void slb_test_host(void)
{
	u32 rdata, wdata;

	set_val(U2_HOST_DPHY_CFG2, 0x700);
	set_val(U2_HOST_DPHY_CFG1, 0xb99);
	set_val(U2_HOST_DPHY_CFG1, 0xf99);
	set_val(U2_HOST_DPHY_CFG1, 0x799);
	mdelay(1000);

	rdata = get_val(U2_HOST_DPHY_CFG1);
	if (rdata != 0x01800799)
		printf("\tall zeros err\n");
	else
		printf("\tall zeros pass\n");

	set_val(U2_HOST_DPHY_CFG2, 0x700);
	set_val(U2_HOST_DPHY_CFG1, 0x3fdb99);
	set_val(U2_HOST_DPHY_CFG1, 0x3fdf99);
	set_val(U2_HOST_DPHY_CFG1, 0x3fd799);
	mdelay(1000);

	rdata = get_val(U2_HOST_DPHY_CFG1);
	if (rdata != 0x01bfd799)
		printf("\tload from SLBSEED err\n");
	else
		printf("\tload from SLBSEED pass\n");

	set_val(U2_HOST_DPHY_CFG2, 0x700);
	set_val(U2_HOST_DPHY_CFG1, 0x6b99);
	set_val(U2_HOST_DPHY_CFG1, 0x6f99);
	set_val(U2_HOST_DPHY_CFG1, 0x6799);
	mdelay(1000);

	rdata = get_val(U2_HOST_DPHY_CFG1);
	if (rdata != 0x01806799)
		printf("\tpseudo random pattern err\n");
	else
		printf("\tpseudo random pattern pass\n");

	set_val(U2_HOST_DPHY_CFG2, 0x700);
	set_val(U2_HOST_DPHY_CFG1, 0x3b99);
	set_val(U2_HOST_DPHY_CFG1, 0x3f99);
	set_val(U2_HOST_DPHY_CFG1, 0x3799);
	mdelay(1000);

	rdata = get_val(U2_HOST_DPHY_CFG1);
	if (rdata != 0x01803799)
		printf("\tincremental counter err\n");
	else
		printf("\tincremental counter pass\n");
}

static int do_usb_slb(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	int i;
	char *cmd1, *cmd2;
	char *slb_type[4] = {"all zeros", "all ones", "pseudo random",
			     "incremental counter"};
	int slb_mode[4] = {0, 1, 2, 3};
	int slb_seed[4] = {0, 0xff, 0x1, 0};

	if (argc < 3)
		return -1;
	cmd1 = argv[1];
	cmd2 = argv[2];

	if (!strcmp(cmd2, "device")) {
		while ((get_val(USB_DPHY_STS) & 0x400) != 0x400)
			;
		mdelay(1000);
		printf("PLL ready!\n");
		if (!strcmp(cmd1, "high")) {
			for (i = 0; i < 4; i++) {
				printf("Device SLB High Speed Test ");
				slb_test_device(slb_type[i], 1, slb_mode[i],
						slb_seed[i]);
			}
		} else if (!strcmp(cmd1, "full")) {
			for (i = 0; i < 4; i++) {
				printf("Device SLB Full Speed Test ");
				slb_test_device(slb_type[i], 0, slb_mode[i],
						slb_seed[i]);
			}
		}
	} else if (!strcmp(cmd2, "host")) {
		if (!strcmp(cmd1, "high")) {
			printf("Host SLB High Speed Test\n");
			slb_test_host();
		}
	}

	return 0;
}

U_BOOT_CMD(
	rts_usb_slb, 3, 0, do_usb_slb,
	"rts_usb_slb high/full device / rts_usb_slb high host",
	"rts_usb_slb high device - usb device Self-Loop-Back high speed test\n"
	"rts_usb_slb full device - usb device Self-Loop-Back full speed test\n"
	"rts_usb_slb high host - usb host Self-Loop-Back high speed test\n"
);
