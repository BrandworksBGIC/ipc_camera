#ifndef __IPC_THREAD_H__
#define __IPC_THREAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>
#include <pthread.h>

#define IPC_PROCESS_FLAG (1 << 7)

typedef enum {
    IPC_THREAD_MUTEX,
    IPC_THREAD_RWLOCK,
    IPC_PROCESS_MUTEX  = IPC_THREAD_MUTEX | IPC_PROCESS_FLAG,
    IPC_PROCESS_RWLOCK = IPC_THREAD_RWLOCK | IPC_PROCESS_FLAG,
} ipc_lock_e;

typedef struct {
    u8 lock_type;
    union {
        pthread_mutex_t mutex;
        pthread_rwlock_t rwlock;
    } lock;
} ipc_lock_t[1], *ipc_lock_p;
/**
 * @brief Initializes a lock
 *
 * @param h_lock The lock handle to be initialized
 * @param type The lock type to be initialized
 * @return The standard return value of ipc_std.h
 */
EXAPI s32 ipc_lock_init(ipc_lock_p h_lock, ipc_lock_e type);

/**
 * @brief Destroys a lock
 *
 * @param h_lock The lock handle obtained by ipc_lock_init
 */
EXAPI void ipc_lock_uninit(ipc_lock_p h_lock);

/**
 * @brief Locks a mutex (mutex lock)
 *
 * @param h_lock The lock handle obtained by ipc_lock_init
 * @note If the type of h_lock is a read-write lock, calling this function is equivalent to calling ipc_rwlock
 */
EXAPI void ipc_lock(ipc_lock_p h_lock);

/**
 * @brief Locks a read-write lock (write lock)
 *
 * @param h_lock The lock handle obtained by ipc_lock_init
 * @note If the type of h_lock is a mutex lock, calling this function is equivalent to calling ipc_lock
 */
EXAPI void ipc_rwlock(ipc_lock_p h_lock);

/**
 * @brief Locks a read-write lock (read lock)
 *
 * @param h_lock The lock handle obtained by ipc_lock_init
 * @note If the type of h_lock is a mutex lock, calling this function is equivalent to calling ipc_lock
 */
EXAPI void ipc_rdlock(ipc_lock_p h_lock);

/**
 * @brief Unlocks a lock
 *
 * @param h_lock The lock handle obtained by ipc_lock_init
 */
EXAPI void ipc_unlock(ipc_lock_p h_lock);
/********************************** cond *************************************/
typedef struct {
    pthread_cond_t cond;
} ipc_cond_t[1], *ipc_cond_p;

typedef enum {
    IPC_THREAD_COND,
    IPC_PROCESS_COND = IPC_THREAD_COND | IPC_PROCESS_FLAG,
} ipc_cond_e;
/**
 * @brief Initializes a condition variable
 *
 * @param h_cond The condition variable handle to be initialized
 * @param type The type of condition variable
 * @return The standard return value of ipc_std.h
 */
EXAPI s32 ipc_cond_init(ipc_cond_p h_cond, ipc_cond_e type);

/**
 * @brief Waits for a condition variable to be triggered, similar to pthread_cond_wait
 *
 * @param h_cond The handle initialized by ipc_cond_init
 * @param h_mutex The handle initialized by ipc_lock_init, and the type must be mutex lock
 * @param abs_mono_tms The absolute monotonic timestamp calculated from ipc_mono_tms(), if it is 0, it will wait
 * indefinitely
 * @return The standard return value of ipc_std.h (IPC_SUCCESS: triggered IPC_TIMEOUT: timed out IPC_FAILED: other errors)
 */
EXAPI s32 ipc_cond_wait(ipc_cond_p h_cond, ipc_lock_p h_mutex, u64 abs_mono_tms);

/**
 * @brief Wakes up a single thread waiting on this condition variable, similar to pthread_cond_signal
 *
 * @param h_cond The handle initialized by ipc_cond_init
 */
EXAPI void ipc_cond_wakeup(ipc_cond_p h_cond);

/**
 * @brief Wakes up all threads waiting on this condition variable, similar to pthread_cond_broadcast
 *
 * @param h_cond The handle initialized by ipc_cond_init
 */
EXAPI void ipc_cond_wakeup_all(ipc_cond_p h_cond);

/**
 * @brief Destroys a condition variable
 *
 * @param h_cond The handle initialized by ipc_cond_init
 */
EXAPI void ipc_cond_uninit(ipc_cond_p h_cond);
/*********************************** thread ***********************************/

/**
 * @brief Creates a thread
 *
 * @param name The name of the thread
 * @param task The thread function to be started
 * @param arg The thread parameter to be passed
 * @param stack_size The stack size of the thread
 * @param priority The priority of the thread, where 0 is the default priority (not implemented yet)
 * @return The standard return value of ipc_std.h
 */
EXAPI s32 ipc_create_thread(pv8 name, vptr (*task)(vptr arg), vptr arg, s32 stack_size, u8 priority);

#ifdef __cplusplus
}
#endif

#endif
