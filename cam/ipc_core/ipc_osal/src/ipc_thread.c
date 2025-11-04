#include "ipc_thread.h"
#include <errno.h>

static struct {
    s32 (*f_wrlock)(vptr);
    s32 (*f_rdlock)(vptr);
    s32 (*f_unlock)(vptr);
    s32 (*f_destroy)(vptr);
} lock_ctrl[] = {
    {
        (vptr)pthread_mutex_lock,
        (vptr)pthread_mutex_lock,
        (vptr)pthread_mutex_unlock,
        (vptr)pthread_mutex_destroy,
    },
    { 
        (vptr)pthread_rwlock_wrlock, 
        (vptr)pthread_rwlock_rdlock, 
        (vptr)pthread_rwlock_unlock,
        (vptr)pthread_rwlock_destroy 
    },
};

s32 ipc_lock_init(ipc_lock_p h_lock, ipc_lock_e type)
{
    // coverity[NO_EFFECT :SUPPRESS]
    if (!h_lock || type < 0)
        return IPC_INVALID_ARGS;

    memset(h_lock, 0, sizeof(*h_lock));
    h_lock->lock_type = type & ~IPC_PROCESS_FLAG; /* Basic type */

    if (type & IPC_PROCESS_FLAG) {
        pthread_mutexattr_t mutex_attr;
        pthread_rwlockattr_t rwlock_attr;
        if (h_lock->lock_type == IPC_THREAD_MUTEX) {
            pthread_mutexattr_init(&mutex_attr);
            pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&h_lock->lock.mutex, &mutex_attr);
            pthread_mutexattr_destroy(&mutex_attr);
        } else if (h_lock->lock_type == IPC_THREAD_RWLOCK) {
            pthread_rwlockattr_init(&rwlock_attr);
            pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);
            pthread_rwlock_init(&h_lock->lock.rwlock, &rwlock_attr);
            pthread_rwlockattr_destroy(&rwlock_attr);
        } else {
            return IPC_INVALID_ARGS;
        }
    } else {
        if (h_lock->lock_type == IPC_THREAD_MUTEX) {
            pthread_mutex_init(&h_lock->lock.mutex, NULL);
        } else if (h_lock->lock_type == IPC_THREAD_RWLOCK) {
            pthread_rwlock_init(&h_lock->lock.rwlock, NULL);
        } else {
            return IPC_INVALID_ARGS;
        }
    }

    return IPC_SUCCESS;
}

void ipc_lock_uninit(ipc_lock_p h_lock)
{
    lock_ctrl[h_lock->lock_type].f_destroy(&h_lock->lock);
}

void ipc_lock(ipc_lock_p h_lock)
{
    lock_ctrl[h_lock->lock_type].f_wrlock(&h_lock->lock);
}

void ipc_rwlock(ipc_lock_p h_lock)
{
    lock_ctrl[h_lock->lock_type].f_wrlock(&h_lock->lock);
}

void ipc_rdlock(ipc_lock_p h_lock)
{
    lock_ctrl[h_lock->lock_type].f_rdlock(&h_lock->lock);
}

void ipc_unlock(ipc_lock_p h_lock)
{
    lock_ctrl[h_lock->lock_type].f_unlock(&h_lock->lock);
}

/***************************************************************************/

s32 ipc_cond_init(ipc_cond_p h_cond, ipc_cond_e type)
{
    // coverity[NO_EFFECT :SUPPRESS]
    if (!h_cond || type < 0)
        return IPC_INVALID_ARGS;

    memset(h_cond, 0, sizeof(*h_cond));

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

    if (type & IPC_PROCESS_FLAG) {
        pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    }

    // coverity[CHECKED_RETURN :SUPPRESS]
    pthread_cond_init(&h_cond->cond, &attr);
    pthread_condattr_destroy(&attr);

    return IPC_SUCCESS;
}

s32 ipc_cond_wait(ipc_cond_p h_cond, ipc_lock_p h_mutex, u64 abs_mono_tms)
{
    if (!h_cond || !h_mutex || (h_mutex->lock_type & ~IPC_PROCESS_FLAG) != IPC_THREAD_MUTEX)
        return IPC_INVALID_ARGS;

    s32 ret = 0;
    if (abs_mono_tms > 0) {
        struct timespec abs_tm = {
            .tv_sec  = abs_mono_tms / 1000,
            .tv_nsec = (abs_mono_tms % 1000) * 1000 * 1000,
        };
        ret = pthread_cond_timedwait(&h_cond->cond, &h_mutex->lock.mutex, &abs_tm);
        if (ret == ETIMEDOUT)
            return IPC_TIMEOUT;
    } else {
        ret = pthread_cond_wait(&h_cond->cond, &h_mutex->lock.mutex);
    }
    return ret != 0 ? IPC_FAILED : IPC_SUCCESS;
}

void ipc_cond_wakeup(ipc_cond_p h_cond)
{
    if (!h_cond)
        return;
    s32 ret = pthread_cond_signal(&h_cond->cond);
    if (ret < 0) {
        perror("pthread_cond_signal");
    }
}

void ipc_cond_wakeup_all(ipc_cond_p h_cond)
{
    if (!h_cond)
        return;
    s32 ret = pthread_cond_broadcast(&h_cond->cond);
    if (ret < 0) {
        perror("pthread_cond_signal");
    }
}

void ipc_cond_uninit(ipc_cond_p h_cond)
{
    if (!h_cond)
        return;
    pthread_cond_destroy(&h_cond->cond);
}

/***************************************************************************/
#include <sys/prctl.h>

typedef struct {
    pv8 name;
    vptr (*task)(vptr arg);
    vptr arg;
    ipc_lock_t mutex;
} thread_t, *thread_p;

static vptr _inline_thread(vptr thread)
{
    thread_p p_thd = (thread_p)thread;

    prctl(PR_SET_NAME, p_thd->name);
    vptr (*task)(vptr arg) = p_thd->task;
    vptr arg               = p_thd->arg;

    ipc_unlock(p_thd->mutex); /* Release the parent thread */

    return task(arg);
}

s32 ipc_create_thread(pv8 name, vptr (*task)(vptr arg), vptr arg, s32 stack_size, u8 priority)
{
    thread_t thread = { name, task, arg };
    ipc_lock_init(thread.mutex, IPC_THREAD_MUTEX);
    ipc_lock(thread.mutex); /* Lock it once */

    pthread_t pid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stack_size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    s32 ret = pthread_create(&pid, &attr, _inline_thread, &thread);
    pthread_attr_destroy(&attr);

    if (ret == 0)
        ipc_lock(thread.mutex); /* Wait for the thread to unlock */

    ipc_unlock(thread.mutex);
    ipc_lock_uninit(thread.mutex);

    return ret ? IPC_THREAD_ERROR : IPC_SUCCESS;
}
