#ifndef __IPC_TIMER_H__
#define __IPC_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/**
 * @brief Timer task callback
 *
 * @param usr_arg The user parameter passed in when calling ipc_timer_start
 * @param tmp_mem The temporary memory space dedicated to the task
 * @param tmp_mem_size The temporary memory space dedicated to the task (which is the tmp_mem_size passed in when
 * calling ipc_timer_init)
 * @return >=0: The time in milliseconds until the next task run <0: Stop the task and destroy the timer
 * @note Note that returning 0 means executing this timer task again immediately, and misusing it may cause unexpected
 * deadlocks
 */
typedef s32 (*ipc_timer_task_f)(vptr usr_arg, pu8 tmp_mem, s32 tmp_mem_size);

/**
 * @brief Initialize the timer pool
 *
 * @param timer_max maximum number of timers initialized (maximum 65535)
 * @param tmp_mem_size The maximum size of the built-in temporary memory space for each timed task (note that this
 * number will affect the memory usage of all timers) (if the size is not enough for memory alignment, it will be
 * rounded up to memory alignment)
 * @return Timer pool handle
 */
EXAPI vptr ipc_timer_init(u16 timer_max, s32 tmp_mem_size);

/**
 * @brief Destroy the timer pool
 *
 * @param handle The timer pool handle obtained by ipc_timer_init initialization
 * @param is_wait 0: Notify the thread to exit but do not destroy resources 1: blocking destruction of resources (you
 * can choose to first notify the exit thread by is_wait 0, and then destroy resources by is_wait 1)
 */
EXAPI void ipc_timer_uninit(vptr handle, u8 is_wait);

/**
 * @brief Allocate a specific timer from the timer pool for a timed task
 *
 * @param handle The timer pool handle obtained by ipc_timer_init initialization
 * @param after_tms The time in milliseconds until the task is triggered
 * @param f_task The task callback function
 * @param usr_arg The user parameter passed to the task callback function
 * @return <0: ipc_std.h standard return value >=0: timer id
 */
EXAPI s32 ipc_timer_start(vptr handle, s32 after_tms, ipc_timer_task_f f_task, vptr usr_arg);

/**
 * @brief Stop a specified timer
 *
 * @param handle The timer pool handle obtained by ipc_timer_init initialization
 * @param timer_id The timer ID handle obtained by ipc_timer_start
 * @return <0: ipc_std.h standard return value
 *             IPC_ACTION_BUSY: The task is being executed and cannot be interrupted
 *             IPC_NOT_FOUND: The task has been completed and the timer has been destroyed
 *         >0: Time remaining until the next task trigger (milliseconds)
 * @note Note that when the return value is IPC_ACTION_BUSY, it means that the timer task is running and cannot be
 * stopped temporarily during the running atomic process, and external waiting is required
 *
 * @note 1. Question: Why not design it as setting a flag bit and destroy it after the internal timer task finishes
 * running?
 *       1. Answer: Timer tasks often have resource competition problems (depending on the caller), and asynchronous
 * destruction may cause problems beyond the caller's expectations, so the design of this function must be clearly
 * synchronized destruction If the caller knows that there will be no resource competition problems and the task itself
 * only runs once, IPC_ACTION_BUSY return value can be ignored
 *
 * @note 2. Question: Since there is a resource competition problem, why introduce IPC_ACTION_BUSY to let the outside
 * wait? It is too much trouble to call.
 *       2. Answer: In extreme cases, this timer task may run for a relatively long time (although it is not recommended
 * that the timer task run too much code), then the decision of whether stop should block can be decided by the caller
 *              Secondly, this API is designed to be called within the timer task (can be used to destroy other timer
 * tasks), if the timer task calls ipc_timer_stop to destroy itself, It will fall into a deadlock state where the [timer
 * task waits for ipc_timer_stop] to finish, while the [ipc_timer_stop waits for the timer task] to finish.
 */
EXAPI s32 ipc_timer_stop(vptr handle, s32 timer_id);

#ifdef __cplusplus
}
#endif

#endif //__IPC_TIMER_H__
