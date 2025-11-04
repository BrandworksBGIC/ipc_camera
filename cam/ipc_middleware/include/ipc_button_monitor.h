#ifndef __IPC_BUTTON_MONITOR_H__
#define __IPC_BUTTON_MONITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

typedef enum {
    IPC_BUTTON_RESET, ///< Reset button
    IPC_BUTTON_FRONT, ///< Front button
    IPC_BUTTON_NUM,
} ipc_button_type_e;

/**
 * @brief Button monitoring trigger feedback interface
 *
 * @param usr_args User data passed during ipc_button_monitor_init
 * @param button See ipc_button_type_e
 * @param hold_on 1: Current state is pressed (continuous trigger) 0: Current state is released at that moment
 * @param tms Milliseconds already continuously triggered, this parameter is meaningless when button is IPC_BUTTON_RESET
 * @note Do not perform blocking operations or operations that take too long in the callback (unless the final operation is a reboot)
 */
typedef void (*ipc_button_trigger_f)(vptr usr_args, ipc_button_type_e button, u8 hold_on, u32 tms);

/**
 * @brief Initializes the button monitoring module
 *
 * @param interval_ms Interval for detecting buttons
 * @return IPC_SUCCESS: Success; other: Failure
 */
EXAPI s32 ipc_button_monitor_init(ipc_button_trigger_f handler, vptr usr_args);

/**
 * @brief Destroys resources for the button monitoring module
 *
 * @param is_wait 0: Notify the module to destroy resources (no waiting) 1: Wait for the module to destroy resources
 */
EXAPI void ipc_button_monitor_uninit(s32 is_wait);

#ifdef __cplusplus
}
#endif

#endif //__IPC_BUTTON_MONITOR_H__