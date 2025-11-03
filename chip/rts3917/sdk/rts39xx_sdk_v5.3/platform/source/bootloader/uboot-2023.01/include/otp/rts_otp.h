/*
 * SPDX-License-Identifier:	GPL-2.0+
 * Copyright (C) Realsil Corporation
 * Copyright (C) 2021, Sherry Cheng <sherry_cheng@realsil.com.cn>
 */
#ifndef _RTS_OTP_H_
#define _RTS_OTP_H_

void rts_otp_enable(void);
int rts_otp_load(u16 group, u16 length);
int rts_otp_read(u32 addr, u16 group, u16 length);
int rts_otp_write(u32 addr, u16 group, u16 sub_group);

#endif
