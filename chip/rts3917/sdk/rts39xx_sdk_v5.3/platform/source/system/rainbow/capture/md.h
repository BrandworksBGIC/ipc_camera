/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MD_H__
#define __MD_H__

int rf_control_md(
	int request, void *args);
int rf_init_md(void);
void rf_release_md(void);

#endif
