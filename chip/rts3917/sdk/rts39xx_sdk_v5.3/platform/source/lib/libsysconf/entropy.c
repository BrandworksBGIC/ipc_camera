/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU LESSER General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/inotify.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <dirent.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "list.h"
#include "entropy.h"
#include "utils.h"

struct conf_file {
	struct list_head next;
	char name[NAME_MAX + 1];
	struct timeval update;
	uint32_t event;
	int entropy;

};

static struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"daemon", no_argument, NULL, 'D'},
	{"log_level", required_argument, NULL, 'v'},
	{"domain", required_argument, NULL, 'f'},
	{0, 0, 0, 0},
};

#define VERSION "0.1"
static void print_usage()
{
	fprintf(stdout, "entropy-%s - Sync config file to flash.\n", VERSION);
	fprintf(stdout, "build at: %s %s\n", __TIME__, __DATE__);
	fprintf(stdout, "usage:\n");
	fprintf(stdout, "\t-h | --help   show this help and exit\n");
	fprintf(stdout, "\t-D | --daemon run octopus as daemon\n");
	fprintf(stdout, "\t-v | --log_level <level> set log level\n");

}

static int g_run_as_daemon = 0;
uint32_t ep_log_level = 0x18;

static int parse_arg(int argc, char **argv)
{
	int ret = 0;
	int c = '0';

	while ((c =
		getopt_long(argc, argv, "-:v:Dh", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			ret = -1;
			break;
		case 'D':
			g_run_as_daemon = 1;
			break;
		case 'v':
			ep_log_level = strtoll(optarg, NULL, 0);
			break;
		case ':':
			fprintf(stdout, "%c require argument\n", optopt);
			ret = -1;
			break;
		case '?':
			fprintf(stdout, "%c invalid argument\n", optopt);
			ret = -1;
			break;
		default:
			break;
		}
	}

	return ret;
}

#define INOTIFY_EVENT_SIZE (sizeof(struct inotify_event) + NAME_MAX + 16)

static int sync_conf_file(struct conf_file *cf)
{
	char *file_src = malloc(strlen(cf->name) + strlen(CONF_DIR_RAMFS) + 8);
	char *file_dst = malloc(strlen(cf->name) + strlen(CONF_DIR_JFFS2) + 8);
	int ret = 0;

	if (!file_src || !file_dst) {
		ret = -ENOMEM;
		ep_error("alloc memory fail\n");
		goto out;
	}

	sprintf(file_src, "%s/%s", CONF_DIR_RAMFS, cf->name);
	sprintf(file_dst, "%s/%s", CONF_DIR_JFFS2, cf->name);

	ep_info("sync %s --> %s\n", file_src, file_dst);

	ret  = sync_file(file_dst, file_src, 0);
	if (ret) {
		ep_error("sync file %s to %s fail:%s\n",
				file_src, file_dst, strerror(-ret));
		goto out;
	}

out:
	if (file_src)
		free(file_src);
	if (file_dst)
		free(file_dst);
	return ret;

}

static int delete_conf_file(struct conf_file *cf)
{
	char *file_dst = malloc(strlen(cf->name) + strlen(CONF_DIR_JFFS2) + 8);
	int ret = 0;

	if (!file_dst) {
		ret = -ENOMEM;
		ep_error("alloc memory fail\n");
		goto out;
	}

	sprintf(file_dst, "%s/%s", CONF_DIR_JFFS2, cf->name);
	ep_info("delete conf:%s\n", cf->name);
	unlink(file_dst);

out:
	if (file_dst)
		free(file_dst);

	return ret;
}

static void process_events(struct list_head *conf_files, int timeout)
{
	struct list_head *pos, *tmp;
	struct timeval tv = {0};

	gettimeofday(&tv, NULL);

	list_for_each_safe(pos, tmp, conf_files) {
		struct conf_file *cf = list_entry(pos, struct conf_file, next);
		int elapse = (tv.tv_sec - cf->update.tv_sec) * 1000
			+ tv.tv_usec / 1000
			- cf->update.tv_usec / 1000;

		if (elapse > timeout) {

			switch (cf->event) {
			case IN_CREATE:
			case IN_MODIFY:
				sync_conf_file(cf);
				/* TODO: error handle */
				break;
			case IN_DELETE:
				delete_conf_file(cf);
				break;
			}

			list_del(pos);
			free(cf);
		}
	}
}

static void add_event(struct list_head *conf_files,
		struct inotify_event *event)
{
	struct list_head *pos;
	int found = 0;

	ep_info("new event:%#x\n",  event->mask);

	list_for_each(pos, conf_files) {
		struct conf_file *cf = list_entry(pos, struct conf_file, next);

		if (0 == strcmp(event->name, cf->name)) {
			cf->event = event->mask;
			gettimeofday(&cf->update, NULL);
			found = 1;
			break;
		}
	}

	if (!found) {
		struct conf_file *cf = calloc(1, sizeof(*cf));

		if (!cf) {
			ep_error("alloc memory fail\n");
			return;
		}

		gettimeofday(&cf->update, NULL);
		snprintf(cf->name, sizeof(cf->name), "%s", event->name);
		cf->event = event->mask;
		list_add_tail(&cf->next, conf_files);
	}
}

static int g_quit;

static void handle_signal(int sig)
{
	if (SIGTERM == sig)
		g_quit = 1;
}

static struct sigaction sigact = {
	.sa_handler = handle_signal,
};


static int do_job()
{
	struct inotify_event *event = NULL;
	struct list_head conf_files;
	int fd = -1;
	int wd = -1;
	int ret = 0;

	INIT_LIST_HEAD(&conf_files);

	fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (fd < 0) {
		ret = -errno;
		ep_error("init inotify fail:%s\n", strerror(errno));
		goto out;
	}

	wd = inotify_add_watch(fd, CONF_DIR_RAMFS, IN_MODIFY | IN_DELETE);
	if (wd < 0) {
		ret = -errno;
		ep_error("add watch event fail:%s\n", strerror(errno));
		goto out;
	}

	event = calloc(1, INOTIFY_EVENT_SIZE);
	if (!event) {
		ret = -ENOMEM;
		ep_error("alloc memory fail\n");
		goto out;
	}

	do {
		char read_buf[INOTIFY_EVENT_SIZE] = {0};
		int event_len = 0;
		int nread = 0;
		int len = 0;
		memset(read_buf, 0, INOTIFY_EVENT_SIZE);
		len = read(fd, read_buf, INOTIFY_EVENT_SIZE);

		while (len > 0) {
			event_len = ((struct inotify_event *)(read_buf + nread))->len;
			memset(event, 0, INOTIFY_EVENT_SIZE);
			memcpy(event, read_buf + nread, sizeof(struct inotify_event) + event_len);
			nread = nread + sizeof(struct inotify_event) + event_len;
			len = len - sizeof(struct inotify_event) - event_len;
			add_event(&conf_files, event);
		}
		process_events(&conf_files, 2000);

		usleep(1000 * 100);
	} while (!g_quit);

	process_events(&conf_files, 0);
out:
	if (wd > 0)
		inotify_rm_watch(fd, wd);
	if (fd > 0)
		close(fd);
	if (event)
		free(event);

	return ret;
}

int main(int argc, char **argv)
{
	struct sigaction sg;
	struct rlimit rl;
	int ret = 0;
	pid_t pid = 0;

	g_quit = 0;
	ret = sigaction(SIGTERM, &sigact, NULL);
	if (ret < 0)
		goto out;

	ret = parse_arg(argc, argv);
	if (ret) {
		print_usage();
		exit(1);
	}

	if (!g_run_as_daemon)
		return do_job();

	umask(0);

	ret = getrlimit(RLIMIT_NOFILE, &rl);
	if (ret < 0)
		goto out;

	pid = fork();

	if (pid < 0)
		goto out;
	else if (pid > 0) /* parend */
		exit(0);

	setsid();

	sg.sa_handler = SIG_IGN;
	sigemptyset(&sg.sa_mask);
	sg.sa_flags = 0;
	ret = sigaction(SIGHUP, &sg, NULL);
	if (ret < 0)
		goto out;

	pid = fork();
	if (pid < 0)
		goto out;
	else if (pid > 0) /* parend */
		exit(0);

	ret = chdir("/");
	if (ret < 0)
		goto out;

	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;

	do {
		int i = 0;
		int fd0, fd1, fd2;
		for (i = 0; i < rl.rlim_max; i++)
			close(i);

		fd0 = open("/dev/null", O_RDWR);
		fd1 = dup(0);
		fd2 = dup(1);

		openlog(argv[0], LOG_CONS | LOG_PID, LOG_DAEMON);

		if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
			ep_error("unexpedted file descriptors %d %d %d\n",
					fd0, fd1, fd2);
			ep_error("%s\n", strerror(errno));
			exit(1);
		}
	} while (0);


	return do_job();

out:
	if (ret)
		ep_error("%s\n", strerror(errno));

	return ret;
}

