#include "ipc_softwdg.h"
#include "ipc_memory.h"
#include "ipc_misc.h"
#include "ipc_thread.h"

#ifndef IPC_MAX_WDG
#define IPC_MAX_WDG 20
#endif

typedef struct {
    u8 reg;
    u16 tick;
    u16 pid;
} wdg_t, *wdg_p;

typedef struct {
    ipc_lock_t mutex;
    wdg_t wdg_sets[IPC_MAX_WDG];
} wdgman_t, *wdgman_p;

static wdgman_p _get_wdgman(void)
{
    static wdgman_p _h_man = NULL;
    if (_h_man)
        return _h_man;

    u8 first_create = 0;

    _h_man = ipc_shmalloc('w', sizeof(*_h_man), &first_create);
    if (_h_man == NULL)
        return NULL;

    if (first_create) {
        ipc_lock_init(_h_man->mutex, IPC_PROCESS_MUTEX);
    }

    return _h_man;
}

s32 ipc_swdg_reg(u8 need_reboot)
{
    wdgman_p h_man = _get_wdgman();
    if (h_man == NULL)
        return IPC_NOMEM;

    s32 idx = 0;

    ipc_lock(h_man->mutex);

    for (; idx < IPC_MAX_WDG; idx++) {
        if (h_man->wdg_sets[idx].reg)
            continue;
        h_man->wdg_sets[idx].reg  = 1; /* Register */
        h_man->wdg_sets[idx].tick = 0; /* Default pause */
        h_man->wdg_sets[idx].pid
            = need_reboot ? 0 : ipc_getpid(); /* If the system needs to be restarted, no pid needs to be recorded */
        break;
    }

    ipc_unlock(h_man->mutex);

    return idx < IPC_MAX_WDG ? idx : IPC_NOBUF;
}

void ipc_swdg_unreg(s32 fd)
{
    if (fd < 0 || fd >= IPC_MAX_WDG)
        return;

    wdgman_p h_man = _get_wdgman();
    if (!h_man)
        return;

    h_man->wdg_sets[fd].reg = 0;
}

void ipc_swdg_feed(s32 fd, u16 tick)
{
    if (fd < 0 || fd >= IPC_MAX_WDG)
        return;

    wdgman_p h_man = _get_wdgman();
    if (h_man == NULL)
        return;

    h_man->wdg_sets[fd].tick = tick; /* Reset */
}

s32 ipc_swdg_check(ps32 p_fd)
{
    if (!p_fd || *p_fd < 0 || *p_fd >= IPC_MAX_WDG)
        return IPC_INVALID_ARGS;

    wdgman_p h_man = _get_wdgman();
    if (h_man == NULL)
        return IPC_NOMEM;

    wdg_p h_wdg = NULL;
    for (; *p_fd < IPC_MAX_WDG; (*p_fd)++) {
        h_wdg = &h_man->wdg_sets[*p_fd];
        if (!h_wdg->reg || !h_wdg->tick)
            continue;         /* Empty position or 0: Pause, skip */
        if (h_wdg->tick == 1) /* 1, it will be 0 if it is subtracted, so the pid is returned directly for the upper
                                 layer to handle */
            return h_wdg->pid;

        h_wdg->tick--;
    }

    *p_fd = 0;

    return IPC_NOT_MATCH;
}

void ipc_swdg_rmpid(s32 pid)
{
    if (pid <= 0)
        return;

    wdgman_p h_man = _get_wdgman();
    if (!h_man)
        return;

    for (s32 idx = 0; idx < IPC_MAX_WDG; idx++) {
        if (h_man->wdg_sets[idx].pid != pid)
            continue;
        h_man->wdg_sets[idx].reg = 0;
    }
}

#ifdef FATHER
#include "ipc_log.h"
#include "ipc_time.h"

s32 main(s32 argc, pv8 argv[])
{
    clog_init("wdg", "Watchdog");

    /* Soft watchdog */
    s32 fd  = 0;
    s32 pid = 0;

    while (1) {
        pid = ipc_swdg_check(&fd);
        if (pid > 0) {
            cpwarn("kill %d", pid);
            ipc_swdg_rmpid(pid);
            kill(pid, SIGINT);
            continue;
        }
        ipc_sleep(1);
    }
    return 0;
}

#endif

#ifdef CHILD
#include "ipc_time.h"
s32 main(s32 argc, pv8 argv[])
{
    s32 fd = ipc_swdg_reg(0);
    s32 ts = 0;

    printf("fd : %d\n", fd);
    srand(fd);
    while (1) {
        ipc_swdg_feed(fd, 5);
        ts = rand() % 9 + 1;
        printf("ts : %d\n", ts);
        ipc_sleep(ts);
    }
}

#endif
