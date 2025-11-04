#ifndef __IPC_SOFTWDG_H__
#define __IPC_SOFTWDG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/******************** Users (feeders) ********************/

/**
 * @brief Register a software watchdog
 *
 * @param need_reboot Whether a reboot is required when the watchdog times out
 * @return <0: standard return value in ipc_std.h >=0: software watchdog descriptor
 * @note When you first initialize it, the software watchdog is in a paused state and will only start counting after you
 * call ipc_swdg_feed
 * @note The software watchdog is implemented using shared memory and is very lightweight. It can be used in various
 * places that require reliability, and can even be used before calling a mutex lock and destroyed after unlocking to
 * prevent deadlocks
 */
EXAPI s32 ipc_swdg_reg(u8 need_reboot);

/**
 * @brief Unregister a software watchdog
 *
 * @param fd The software watchdog descriptor returned by ipc_swdg_reg
 */
EXAPI void ipc_swdg_unreg(s32 fd);

/**
 * @brief Feed the watchdog
 *
 * @param fd The software watchdog descriptor returned by ipc_swdg_reg
 * @param tick The number of ticks for feeding the watchdog, and the specific unit of tick depends on the interval set
 * by the guardian ipc_swdg_check, usually 1 second/tick
 * @note When tick is 0, it means to pause the software watchdog
 */
EXAPI void ipc_swdg_feed(s32 fd, u16 tick);

/******************** Supervisors (keepers) ********************/

/**
 * @brief Check the remaining food of each software watchdog
 *
 * @param p_fd The descriptor of each watchdog dog is iterated
 * @return <0: standard return value in ipc_std.h, =0: starved dog, whose caller ipc_swdg_reg marked need reboot, >0:
 * process pid of starved dog
 * @note After each call to this api for detection, if there is no starving dog, IPC_NOT_MATCH will be returned
 * @note In general, the return value:
 *        <0, then sleep for a tick (custom tick time unit) interval,
 *        =0, then reboot the system
 *        >0, then kill the process corresponding to the pid and call ipc_swdg_rmpid to clear other software watchdogs
 * registered under the same process that have been harmed. If ipc_swdg_rmpid is not called, the check cannot continue Of
 * course, for some reasons, if you do not want to perform a matching operation, but want to continue the normal check,
 * you can choose to call ipc_swdg_unreg(*p_fd) to help the dead dog clean up the body, and then continue as if nothing
 * happened
 */
EXAPI s32 ipc_swdg_check(ps32 p_fd);

/**
 * @brief Clear the registration records of other software watchdogs in the same process among the dead software
 * watchdogs
 *
 * @param pid The process pid of the dead dog returned by ipc_swdg_check
 * @note In the same process, if a dog dies, at least the process must be restarted, just like a plague. If a dog in the
 * same process gets sick, all the dogs must be wiped out.
 */
EXAPI void ipc_swdg_rmpid(s32 pid);

#ifdef __cplusplus
}
#endif

#endif //__IPC_WATCHDOG_H__
