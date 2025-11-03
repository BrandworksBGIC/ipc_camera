/*
 * SPDX-License-Identifier:	GPL-2.0+
 * Copyright (C) Realsil Corporation
 * Copyright (C) 2021, Sherry Cheng <sherry_cheng@realsil.com.cn>
 */
#include <common.h>
#include <asm/io.h>
#include <configs/bspchip.h>

#define get_val(addr)		REG32(addr)
#define set_val(addr, val)	REG32(addr) = (val)
#define clear_bit(addr, val)	set_val((addr), (get_val(addr) & ~(val)))
#define set_bit(addr, val)	set_val((addr), (get_val(addr) | (val)))

static void write_mask(u32 reg, u32 bit, u32 mask)
{
	u32 temp;

	temp = get_val(reg);
	set_val(reg, (temp & (~mask)) | bit);
}

static u8 gc_next(u8 din)
{
	u8 dout;
	u8 din_7 = din / 128;

	dout = ((din % 128) << 1) ^ (din_7 ? 29 : 0);

	return dout;
}

static u8 mult(u8 din, u8 k)
{
	u8 expo = 1;
	u8 dout = 0;
	u8 din_shift = din;
	int i;

	for (i = 0; i < k; i++)
		expo = gc_next(expo);

	for (i = 0; i < 8; i++) {
		dout = dout ^ ((din_shift % 2) ? expo : 0);
		expo = gc_next(expo);
		din_shift = din_shift >> 1;
	}

	// printf("%d\n", dout);
	return dout;
}

static u16 calc_ecc(u32 *addr)
{
	u8 p0[16], p1[16];
	u8 par0 = 0;
	u8 par1 = 0;
	int i;

	p0[0] = mult(*addr & 0xff, 43);
	p0[1] = mult((*addr >> 8) & 0xff, 120);
	p0[2] = mult((*addr >> 16) & 0xff, 8);
	p0[3] = mult((*addr >> 24) & 0xff, 199);
	p0[4] = mult((*(addr + 1)) & 0xff, 74);
	p0[5] = mult((*(addr + 1) >> 8) & 0xff, 102);
	p0[6] = mult((*(addr + 1) >> 16) & 0xff, 220);
	p0[7] = mult((*(addr + 1) >> 24) & 0xff, 251);
	p0[8] = mult((*(addr + 2)) & 0xff, 95);
	p0[9] = mult((*(addr + 2) >> 8) & 0xff, 175);
	p0[10] = mult((*(addr + 2) >> 16) & 0xff, 87);
	p0[11] = mult((*(addr + 2) >> 24) & 0xff, 166);
	p0[12] = mult((*(addr + 3)) & 0xff, 113);
	p0[13] = mult((*(addr + 3) >> 8) & 0xff, 75);
	p0[14] = mult((*(addr + 3) >> 16) & 0xff, 198);
	p0[15] = mult((*(addr + 3) >> 24) & 0xff, 25);

	p1[0] = mult(*addr & 0xff, 121);
	p1[1] = mult((*addr >> 8) & 0xff, 9);
	p1[2] = mult((*addr >> 16) & 0xff, 200);
	p1[3] = mult((*addr >> 24) & 0xff, 75);
	p1[4] = mult((*(addr + 1)) & 0xff, 103);
	p1[5] = mult((*(addr + 1) >> 8) & 0xff, 221);
	p1[6] = mult((*(addr + 1) >> 16) & 0xff, 252);
	p1[7] = mult((*(addr + 1) >> 24) & 0xff, 96);
	p1[8] = mult((*(addr + 2)) & 0xff, 176);
	p1[9] = mult((*(addr + 2) >> 8) & 0xff, 88);
	p1[10] = mult((*(addr + 2) >> 16) & 0xff, 167);
	p1[11] = mult((*(addr + 2) >> 24) & 0xff, 114);
	p1[12] = mult((*(addr + 3)) & 0xff, 76);
	p1[13] = mult((*(addr + 3) >> 8) & 0xff, 199);
	p1[14] = mult((*(addr + 3) >> 16) & 0xff, 26);
	p1[15] = mult((*(addr + 3) >> 24) & 0xff, 1);

	for (i = 0; i < 16; i++) {
		par0 = par0 ^ p0[i];
		par1 = par1 ^ p1[i];
	}

	printf("par0:\n%#x\npar1:\n%#x\n", par0, par1);

	return (par0 | par1 << 8);
}

static int check_hw_start_reg(void)
{
	while (1) {
		if (get_val(HW_START_REG) == 0)
			break;
	}
	return 0;
}

void rts_otp_enable(void)
{
	/*reset and enable clk*/
	set_bit(FORCE_REG_ASYNC_RST, FORCE_OTP_RST);
	set_val(OTP_CLK_CFG_REG, CLK_ENABLE);
	clear_bit(FORCE_REG_ASYNC_RST, FORCE_OTP_RST);
}

int rts_otp_load(u16 group, u16 length)
{
	set_bit(SF_CTRL_0, RG_PENVDD2_VDD2_SW);
	set_val(ADDR_CTRL, group << 12);
	set_val(ECC_CTRL1, 0xffffffff);
	set_val(ECC_CTRL2, 0xffffff);
	set_val(ECC_CTRL_AUTO, 1);
	set_val(BURST_CTRL, length - 1);
	write_mask(SF_MODE_CTRL, 0 << RG_PTM_SW_OFFSET, RG_PTM_SW_MASK);
	set_val(HW_START_REG, 1);
	check_hw_start_reg();
	return 0;
}

int rts_otp_load_aes_key(u16 group, u16 length)
{
	u16 k = group - 1;

	set_bit(SF_CTRL_0, RG_PENVDD2_VDD2_SW);
	write_mask(REG_AES_KEY_SEL, KEY(0, k) << KEY_SEL(0), KEY_SEL_MASK(0));
	write_mask(REG_AES_KEY_SEL, KEY(1, k) << KEY_SEL(1), KEY_SEL_MASK(1));
	/* enable load key */
	set_val(REG_LOAD_KEY, 1);
	set_val(ADDR_CTRL, group << 12);
	set_val(ECC_CTRL1, 0xffffffff);
	set_val(ECC_CTRL2, 0xffffff);
	set_val(ECC_CTRL_AUTO, 1);
	set_val(BURST_CTRL, length - 1);
	write_mask(SF_MODE_CTRL, 0 << RG_PTM_SW_OFFSET, RG_PTM_SW_MASK);
	set_val(HW_START_REG, 1);
	check_hw_start_reg();

	/* disable load key */
	set_val(REG_LOAD_KEY, 0);
	/*disable power up protect */
	clear_bit(SF_CTRL_0, RG_PENVDD2_VDD2_SW);
	return 0;
}

int rts_otp_read(u32 addr, u16 group, u16 length)
{
	u32 *otp_data_addr;
	u32 *addr_t;
	u8 i;

	if (length == 0)
		return -1;

	rts_otp_load(group, length);
	if (group == 0)
		otp_data_addr = (u32 *)OTP_CONTROL_DATA_0;
	else
		otp_data_addr = (u32 *)OTP_READ_DATA_0;
	addr_t = (u32 *)addr;
	for (i = 0; i <= (length - 1) / 4; i++) {
		*addr_t = *otp_data_addr;
		addr_t++;
		otp_data_addr++;
	}
	return 0;
}

static int set_ecc(u32 ecc, u16 group)
{
	set_val(ADDR_CTRL, group << 12 | 32 << 3);
	set_val(BURST_CTRL, 31);
	write_mask(SF_MODE_CTRL, 0x2 << RG_PTM_SW_OFFSET,
		   RG_PTM_SW_MASK);
	set_val(OTP_PROGRAM_DATA, ecc);
	set_val(HW_START_REG, 1);
	check_hw_start_reg();
	return 0;
}

int rts_otp_write(u32 addr, u16 group, u16 sub_group)
{
	u32 *addr_t;
	u32 *otp_data_addr;
	u32 ecc, ecc0, ecc1;
	u8 i;

	if (group == 0)
		return -1;
	addr_t = (u32 *)addr;
	otp_data_addr = (u32 *)OTP_PROGRAM_DATA;
	set_bit(SF_CTRL_0, RG_PENVDD2_VDD2_SW);
	/* set data 0 */
	if (sub_group == 0) {
		for (i = 0; i < 4; i++) {
			set_val(ADDR_CTRL, group << 12 | i * 4 << 3);
			set_val(BURST_CTRL, 31);
			write_mask(SF_MODE_CTRL, 0x2 << RG_PTM_SW_OFFSET,
				   RG_PTM_SW_MASK);
			*otp_data_addr = *addr_t;
			set_val(HW_START_REG, 1);
			check_hw_start_reg();
			addr_t++;
		}
		ecc0 = calc_ecc((u32 *)addr) | 0xffff0000;
		set_ecc(ecc0, group);
	}
	/* set data 1 */
	if (sub_group == 1) {
		for (i = 0; i < 4; i++) {
			set_val(ADDR_CTRL, group << 12 | (i + 4) * 4 << 3);
			set_val(BURST_CTRL, 31);
			write_mask(SF_MODE_CTRL, 0x2 << RG_PTM_SW_OFFSET,
				   RG_PTM_SW_MASK);
			*otp_data_addr = *addr_t;
			set_val(HW_START_REG, 1);
			check_hw_start_reg();
			addr_t++;
		}
		ecc1 = calc_ecc((u32 *)addr) << 16 | 0xffff;
		set_ecc(ecc1, group);
	}
	/* set data 0 & 1 all*/
	if (sub_group == 2) {
		for (i = 0; i < 8; i++) {
			set_val(ADDR_CTRL, group << 12 | i * 4 << 3);
			set_val(BURST_CTRL, 31);
			write_mask(SF_MODE_CTRL, 0x2 << RG_PTM_SW_OFFSET,
				   RG_PTM_SW_MASK);
			*otp_data_addr = *addr_t;
			set_val(HW_START_REG, 1);
			check_hw_start_reg();
			addr_t++;
		}
		ecc0 = calc_ecc((u32 *)addr);
		ecc1 = calc_ecc((u32 *)addr + 4);
		ecc = ecc0 | (ecc1 << 16);
		set_ecc(ecc, group);
	}
	/* set group 0 */
	// set_val(ADDR_CTRL, 0);
	// set_val(BURST_CTRL, 31);
	// write_mask(SF_MODE_CTRL, 0x2 << RG_PTM_SW_OFFSET,
	// 		RG_PTM_SW_MASK);
	// set_val(OTP_PROGRAM_DATA, ~(1 << group - 1));
	// set_val(HW_START_REG, 1);
	// check_hw_start_reg();

	return 0;
}
