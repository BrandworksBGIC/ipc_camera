#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

#include "fs_mgr_priv.h"


int execle_cmd(const char *cmd)
{
	pid_t pid;
	int status = -1;

	pid = fork();
	if (pid < 0) {
		status = -1;
	} else if (pid == 0) {
		/* child process */
		execle("/usr/bin/sh", "sh", "-c", cmd,
					NULL, NULL);
		exit(1);
	} else {
		/* current process */
		while (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR) {
				status = -1;
				break;
			}
		}
	}

	return status;
}

int execvp_cmd(int argc, char *argv[])
{
	pid_t pid;
	int status = -1;
	char *argv_child[argc + 1];

	memcpy(argv_child, argv, argc * sizeof(char *));
	argv_child[argc] = NULL;

	pid = fork();
	if (pid < 0) {
		status = -1;
	} else if (pid == 0) {
		/* child process */
		if (execvp(argv_child[0], argv_child))
			exit(-1);
		exit(1);
	} else {
		/* current process */
		while (waitpid(pid, &status, 0) < 0) {
			if (errno != EINTR) {
				status = -1;
				break;
			}
		}
	}

	return status;
}
