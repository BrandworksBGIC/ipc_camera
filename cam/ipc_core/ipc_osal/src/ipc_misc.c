#include "ipc_misc.h"

word ipc_not_do_anything(){ return 0; };

#include "ipc_dfs.h"
#include "ipc_time.h"
s32 ipc_rand(void)
{
    s32 num = 0;
    s32 ret = ipc_file_read_once("/dev/urandom", (pv8)&num, sizeof(num), NULL);
    if (ret < 0) {
        num = 1 - (s32)ipc_mono_ts();
    }
    return num;
}

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

s32 ipc_getpid(void)
{
    return (s32)getpid();
}

s8 ipc_exec(pcv8 format, ...)
{
	pid_t pid = vfork();
	if (pid < 0) return IPC_FAILED;
	else if (pid == 0) {
        s32 max = sysconf(_SC_OPEN_MAX);
		for (s32 i = 0; i < max; i++) {
			if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO) {
				close(i);
            }
        }

        v8 cmd[512];
        va_list va_argp;
        va_start(va_argp, format);
        vsnprintf(cmd, sizeof(cmd), format, va_argp);
        va_end(va_argp);

        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
	else {
		s32 stat = 0;
        waitpid(pid, &stat, 0);
        return WIFEXITED(stat) ? (s8)WEXITSTATUS(stat) : IPC_FAILED;
    }
}