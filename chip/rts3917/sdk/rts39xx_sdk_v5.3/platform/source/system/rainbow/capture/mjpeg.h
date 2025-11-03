/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MJPEG_H
#define _MJPEG_H


int rf_init_mjpeg(void);
void rf_release_mjpeg_ctrl(void);
int rf_set_mjpeg_callback(uint32_t start,
		uint32_t pic_num, uint32_t interval);
int rf_set_mjpeg_ctrl(void);

#endif
