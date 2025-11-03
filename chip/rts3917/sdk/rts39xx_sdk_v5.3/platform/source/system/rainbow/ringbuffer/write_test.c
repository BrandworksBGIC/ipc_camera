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
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "ringbuffer.h"

int g_exit;

void sigint_handle(int sig)
{
	g_exit = 1;
}

uint64_t get_ts(void)
{
	struct timespec tv;

	clock_gettime(CLOCK_MONOTONIC, &tv);
	return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_nsec / 1000;
}


int main(void)
{
	char buf[1024];
	int i, ret = 0;
	void *handle = rts_ringbuffer_init("/var/tmp/capture.shm", 1024 * 1024);
	packet_t p;

	if (!handle) {
		perror("ringbuffer_init err");
		return -1;
	}

	signal(SIGINT, sigint_handle);
	signal(SIGTERM, sigint_handle);

	for (i = 0; i < 1024; i++)
		buf[i] = i;

	i = 0;
	while (!g_exit) {
		p.vm_addr = buf;
		p.length = 1024;
		p.timestamp = get_ts();
		p.flags = p.type = 0;
		p.index = i++;

		ret = rts_ringbuffer_write_packet(handle, &p);
		if (ret < 0) {
			perror("write err");
			break;
		}
		usleep(40000);
	}

	rts_ringbuffer_release(handle);

	return ret;
}
