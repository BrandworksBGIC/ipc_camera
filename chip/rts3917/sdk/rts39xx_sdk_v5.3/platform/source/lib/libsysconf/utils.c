/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/sendfile.h>
#include <sys/file.h>
#include <errno.h>

int sc_unlock_file(int fd)
{
	int ret = 0;

	ret = flock(fd,LOCK_UN);

	return ret;
}

int sc_lock_file(int fd, int type, int block)
{
	int ret = 0;
	int operation = 0;

	if (type == F_WRLCK)
		operation = LOCK_EX;
	else
		operation = LOCK_SH;

	if (block == 0)
		operation |= LOCK_NB;

	ret = flock(fd, operation);

	return ret;
}

int sync_file(const char *dst, const char *src, int block)
{
	struct stat stat_buf = {0};
	int fd_src = -1, fd_dst = -1;
	int ret = 0;

	fd_src = open(src, O_RDONLY);
	fd_dst = open(dst, O_RDWR | O_TRUNC | O_CREAT | O_SYNC, S_IRWXU);
	if (fd_src < 0 || fd_dst < 0) {
		ret = -errno;
		goto out;
	}

	ret = stat(src, &stat_buf);
	if (ret) {
		ret = -errno;
		goto out;
	}

	ret = sc_lock_file(fd_src, F_RDLCK, block);
	if (ret)
		goto out;

	ret = sc_lock_file(fd_dst, F_WRLCK, block);
	if (ret)
		goto out;

	ret = sendfile(fd_dst, fd_src, NULL, stat_buf.st_size);
	if (ret != stat_buf.st_size) {
		ret = -errno;
		goto out;
	}

	ret = 0;

out:
	if (fd_dst >= 0) {
		sc_unlock_file(fd_dst);
		close(fd_dst);
	}

	if (fd_src >= 0) {
		sc_unlock_file(fd_src);
		close(fd_src);
	}

	return ret;
}
