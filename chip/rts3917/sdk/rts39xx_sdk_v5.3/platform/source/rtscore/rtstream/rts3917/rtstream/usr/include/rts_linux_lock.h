/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

#ifndef _RTS_LINUX_LOCK_H
#define _RTS_LINUX_LOCK_H

#include <sys/ipc.h>

int rts_semaphore_allocation(key_t key, int nsem, int sem_flags);
int rts_semaphore_deallocate(int semid);
int rts_semaphore_wait(int semid, int sem_num);
int rts_semaphore_post(int semid, int sem_num);
int rts_semaphore_initialize(int semid, int sem_num);
int binary_semaphore_getval(int semid, int semnum);

#endif
