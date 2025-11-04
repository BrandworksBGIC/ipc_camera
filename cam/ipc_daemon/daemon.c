#include <ipc_core.h>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define REBOOT_CMD "reboot"

typedef struct {
    s32 argc;      // Number of parameters passed in
    pcv8* argv;    // Passed parameters
    pid_t pids[0]; // A collection of PIDs for guarded apps
} apps_t, *apps_p;
static apps_p _gh_apps = NULL;

/* @app="arg1, arg2,...| rearg1, rearg2" */
/* arg: Parameters that the app needs to pass in, rearg: If it exists, it will replace arg and pass in rearg when the
 * app restarts */
static void _exec_app(pcv8 app_info, u8 is_first)
{
    v8 args[16][16] = { { 0 } };
    pv8 pvargs[16]  = { NULL };

    s32 num = 0, off = 0, tmp = 0;

    if (app_info[0] == '@')
        off++;                                        /* Skip the restart flag */
    sscanf(app_info + off, "%[^=]%n", args[0], &tmp); /* Get the app name */
    off = tmp;
    num = 1;

    if (app_info[off] == '=') {
        if (!is_first) {
            pv8 str = strchr(app_info + off, '|');
            if (str != NULL)
                off = str - app_info;
        }
        off++;
        while (1) { /* Pick up parameters separated by, before = and after | */
            tmp = 0;
            sscanf(app_info + off, "%[^,|]%n", args[num], &tmp);
            off += tmp;
            num++;
            if (app_info[off] != ',')
                break;
            off++;
        }
        if (num == 2 && args[1][0] == '\0')
            num--; /* If there is only one parameter and nothing is passed, then this parameter is meaningless */
    }

    for (s32 idx = 0; idx < num; idx++) {
        pvargs[idx] = args[idx];
    }

    execvp(pvargs[0], pvargs);
}

static void _sig_child(s32 sig)
{
    pid_t die_pid = 0;
    pid_t new_pid = 0;
    s32 stat      = 0;

    while ((die_pid = waitpid(-1, &stat, WNOHANG))
           > 0) { /* Loop through to reclaim the child process carcasses and resurrect them */
        for (s32 idx = 0; idx < _gh_apps->argc; idx++) {
            if (die_pid != _gh_apps->pids[idx])
                continue;
            ipc_swdg_rmpid(die_pid);
            if (WIFEXITED(stat)) {
                if (WEXITSTATUS(stat) == IPC_SUCCESS) { /* Normal exit does not resurrect */
                    ipcinfo("App -> [%s] normal exit", _gh_apps->argv[idx]);
                    _gh_apps->pids[idx] = 0; /* Prevent possible pid collisions in the future */
                    break;
                }
                ipcerror("Exit value=[%d]", (s8)WEXITSTATUS(stat));
            }
            if (WIFSIGNALED(stat)) {
                ipcerror("Kill by signal=[%d]", WTERMSIG(stat));
            }

            if (_gh_apps->argv[idx][0] == '@') {
                ipcfatal("Waitpid check app -> [%s] need " REBOOT_CMD, _gh_apps->argv[idx]);
                system(REBOOT_CMD); /* Restart when the program crashes with a flag */
                exit(-1);
            }

            new_pid = vfork();
            if (new_pid < 0) {
                ipcfatal("Fork failed! errmsg=[%s]", strerror(errno));
                return;
            } else if (new_pid == 0) {
                _exec_app(_gh_apps->argv[idx], 0);
                _exit(127);
            }
            ipcwarn("Rerun app -> [%s]", _gh_apps->argv[idx]);
            _gh_apps->pids[idx] = new_pid;
        }
    }
}

static s32 _start_apps(s32 argc, pcv8 argv[])
{
    if (argc <= 0)
        return IPC_SUCCESS;

    signal(SIGCHLD, _sig_child);

    s32 len  = sizeof(apps_t) + sizeof(pid_t) * argc;
    _gh_apps = ipc_malloc(len, len);
    if (!_gh_apps) {
        ipcfatal("Malloc failed!");
        return IPC_NOMEM;
    }

    _gh_apps->argc = argc;
    _gh_apps->argv = argv;

    pid_t pid = 0;
    for (s32 idx = 0; idx < argc; idx++) {
        pid = vfork();
        if (pid < 0) {
            ipcfatal("Fork failed! errmsg=[%s]", strerror(errno));
            return IPC_THREAD_ERROR;
        } else if (pid == 0) { /* child process */
            _exec_app(argv[idx], 1);
            _exit(127); /* Although it will not run here, it is a habit */
        }
        _gh_apps->pids[idx] = pid;
        ipcinfo("Start app -> [%s]", argv[idx]);
    }

    return IPC_SUCCESS;
}

#include <fcntl.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>

static u8 _g_go_exit = 0;
static void _signal_exit(s32 sig)
{
    _g_go_exit = 1;
}

#include <sys/stat.h>
#include <sys/types.h>

static void _daemon(void)
{
    if (fork() != 0)
        exit(0);
    setsid();
    chdir("/");
    umask(0);
    close(0);
    // close(1);
    // close(2);
}

s32 main(s32 argc, pcv8 argv[])
{
    _daemon();

    clog_init("daemon", "Daemon and hardware/software watchdog");

    signal(SIGINT, _signal_exit);

    _start_apps(argc - 1, argv + 1);

    s32 pid     = 0;
    s32 swdg_fd = 0;
    s32 hwdg_fd = open("/dev/watchdog", O_WRONLY);
    if (hwdg_fd < 0) {
        ipcfatal("Open hw watchdog failed! errmsg=[%s]", strerror(errno));
        return IPC_OPEN_ERROR;
    }

    s32 flags = WDIOS_ENABLECARD;
    s32 ret   = -1;

    ret = ioctl(hwdg_fd, WDIOC_SETOPTIONS, &flags);
    if (ret != 0) {
        ipcfatal("Ioctl hw watchdog failed! errmsg=[%s]", strerror(errno));
        return IPC_IOCTL_ERROR;
    }

    flags = 5;
    ret   = ioctl(hwdg_fd, WDIOC_SETTIMEOUT, &flags);
    if (ret != 0) {
      ipcfatal("Ioctl hw watchdog failed! errmsg=[%s]", strerror(errno));
      return IPC_IOCTL_ERROR;
    }

    while (!_g_go_exit) {
        ipcdebug("Feed system watchdog");
        ioctl(hwdg_fd, WDIOC_KEEPALIVE, 0);
        pid = ipc_swdg_check(&swdg_fd);
        if (pid == 0) {
            ipcfatal("Softwdg check need reboot! fd=[%d]", swdg_fd);
            system(REBOOT_CMD);
            exit(-1);
        }
        if (pid > 0) {
            ipc_swdg_rmpid(pid);
            ipcwarn("Kill %d, fd=[%d]", pid, swdg_fd);
            kill(pid, SIGKILL);
            continue;
        }
        ipc_sleep(1);
    }

    flags = -1;
    ioctl(hwdg_fd, WDIOC_SETTIMEOUT, &flags);

    close(hwdg_fd);

    return 0;
}
