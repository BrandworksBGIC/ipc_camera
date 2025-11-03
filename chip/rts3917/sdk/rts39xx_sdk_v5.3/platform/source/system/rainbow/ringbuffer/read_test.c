/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "ringbuffer.h"

int g_exit;

void sigint_handle(int sig)
{
	g_exit = 1;
}


int main(void)
{
	void *handle = rts_ringbuffer_init("/var/tmp/capture.shm", 0);
	packet_t *p;
	int ret = 0;


	if (!handle) {
		perror("init err");
		return -1;
	}

	signal(SIGINT, sigint_handle);
	signal(SIGTERM, sigint_handle);

	while (!g_exit) {
		ret = rts_ringbuffer_read_packet(handle, &p);

		if (ret) {
			if (ret == -EAGAIN) {
				usleep(40000);
				continue;
			} else if (ret == 1) {
				printf("closed\n");
				break;
			}

			printf("read err %d\n", ret);
			break;
		}

		printf("recv_pkt %d [%lld] size:%d\n", p->index,
				p->timestamp, p->length);
		rts_ringbuffer_free_packet(p);
	}

	rts_ringbuffer_release(handle);

	return ret;
}
