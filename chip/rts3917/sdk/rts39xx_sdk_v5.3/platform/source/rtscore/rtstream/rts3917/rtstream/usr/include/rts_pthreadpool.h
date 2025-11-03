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

#ifndef _INCLUDE_RTS_PTHREADPOOL_H
#define _INCLUDE_RTS_PTHREADPOOL_H

typedef struct rts_pthreadpool *PthreadPool;
typedef void (*task_handle)(void *);
typedef void (*task_clear)(void *);

enum {
	RTS_TPOOL_PRIORITY_0,
	RTS_TPOOL_PRIORITY_1,
	RTS_TPOOL_PRIORITY_2,
	RTS_TPOOL_PRIORITY_3,
	RTS_TPOOL_PRIORITY_4,
	RTS_TPOOL_PRIORITY_5,
	RTS_TPOOL_PRIORITY_6,
	RTS_TPOOL_PRIORITY_RESERVED
};

#define RTS_TPOOL_PRIORITY_DFT		RTS_TPOOL_PRIORITY_3

PthreadPool rts_pthreadpool_init(int number, int priority);
void rts_pthreadpool_destroy(PthreadPool pool);
int rts_pthreadpool_add_task(PthreadPool pool, task_handle handle, void *args,
			     task_clear release, char *pthread_name);
int rts_pthreadpool_add_task_ex(PthreadPool pool,
				task_handle handle, void *args,
				task_clear release,
				unsigned int priority,
				char *pthread_name);

int rts_thread_count(PthreadPool pool);
int rts_task_count(PthreadPool pool);
int rts_pthreadpool_set_limit(PthreadPool pool, int limit);

int rts_pthreadpool_del_task(PthreadPool pool, task_handle handle, void *args);
int rts_pthreadpool_del_tasks(PthreadPool pool, task_handle handle, void *args,
			      int (*compare_task)(void *src, void *dst),
			      void (*release)(void *arg));
#endif
