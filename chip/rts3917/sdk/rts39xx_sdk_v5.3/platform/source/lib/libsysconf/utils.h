/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __UTILS_H__
#define __UTILS_H__

int sc_unlock_file(int fd);
int sc_lock_file(int fd, int cmd, int block);
int sync_file(const char *dst, const char *src, int block);

#endif
