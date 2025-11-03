/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RF_MSG_SVR_H__
#define __RF_MSG_SVR_H__
#include <stdint.h>

#include "rf_msg.h"

int rf_init_msgd(void);
void rf_release_msgd(void);
void rf_start_msgd(void);
void rf_stop_msgd(void);

#endif
