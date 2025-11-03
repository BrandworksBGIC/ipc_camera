/*
 * Command for accessing Ethernet otp.
 *
 * Copyright (C) Realsil Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <otp/rts_otp.h>

static int do_rts_otp(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret = 0;
	char *cmd;
	char *endp;
	u16 group, length;
	u32 addr;

	/* at least five arguments please */
	if (argc < 5)
		goto usage;

	cmd = argv[1];

	addr = simple_strtoul(argv[2], &endp, 16);
	if (*argv[2] == 0 || *endp != 0)
		return -1;

	group = simple_strtoul(argv[3], &endp, 10);
	if (*endp != 0)
		return -1;

	if (strcmp(argv[4], "all") == 0) {
		length = 2;
	} else {
		length = simple_strtoul(argv[4], &endp, 10);
		if (*endp != 0)
			return -1;
	}

	if (group == 0 && length > 32)
		return -1;
	if (strcmp(cmd, "read") == 0)
		ret = rts_otp_read(addr, group, length);
	else if (strcmp(cmd, "write") == 0)
		ret = rts_otp_write(addr, group, length);
	else
		ret = -1;

	if (ret != -1)
		return ret;

usage:
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	rts_otp, 5, 0, do_rts_otp,
	"rts_otp read/write",
	"rts_otp read addr group length  -  read `length' bytes starting from\n"
	"		`group' of otp to memory at `addr'\n"
	"rts_otp write addr group sub-group  -  write data to 'sub-group' in\n"
	"		'group' of otp from memory at 'addr'\n"
	"group is 0 - 57, when read unit is word align\n"
	"length is 1 - 36, unit: byte, when read unit is word align\n"
	"max length of group 0 is 32, others' is 36\n"
	"addr should be hexadecimal, group and length should be decimal\n"
	"sub-group is 0 or 1 or all, your data at 'addr' must be 16 bytes,\n"
	"		if 0 or 1; 32 bytes if all.\n"
);
