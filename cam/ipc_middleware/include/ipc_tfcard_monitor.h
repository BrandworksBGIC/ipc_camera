#ifndef __IPC_TFCARD_MONITOR_H__
#define __IPC_TFCARD_MONITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

/**
 * PLUG_IN |-> (RECOGNIZED) |-> MOUNT |-> (NORMAL) -> PULL_OUT -> UMOUNT
 *         |                |         |-> READONLY |-> (FIX_SUCCESS) -> REMOUNT
 *         |                |                      |-> (FIX_FAILED ) -> UMOUNT
 *         |                |                                             |
 *         |                |                                             |
 *         |                |-> FORMAT_START |-> FORMAT_FINISH            |
 *         |                                 |-> FORMAT_FAILED            |
 *         |-> UNRECOGNIZED                                               |
 *         |                                                              |
 *         |<-------------------------------------------------------------|
 *
 *  Note: (ctx) -> Parentheses contain implicit concepts
 */
typedef enum {
    IPC_TFCARD_EVENT_PLUG_IN,      ///< Card inserted detected (card present, next step is to mount)
    IPC_TFCARD_EVENT_PULL_OUT,     ///< Card removed detected (no card, next step is to unmount)
    IPC_TFCARD_EVENT_UNRECOGNIZED, ///< Unable to recognize card type [abnormal]
    IPC_TFCARD_EVENT_MOUNT,        ///< Card mounted successfully notification
    IPC_TFCARD_EVENT_UMOUNT,       ///< Card unmount notification
    IPC_TFCARD_EVENT_READONLY,     ///< Read-only [abnormal]
    IPC_TFCARD_EVENT_REMOUNT, ///< Card remount notification (called after ipc_tfcard_remount or IPC_TFCARD_EVENT_READONLY read-only anomaly is fixed
                             ///< successfully)
    IPC_TFCARD_EVENT_FORMAT_START,  ///< Formatting start
    IPC_TFCARD_EVENT_FORMAT_FAILED, ///< Formatting failed
    IPC_TFCARD_EVENT_FORMAT_FINISH, ///< Formatting complete
} ipc_tfcard_monitor_event_e;

#define TFCARD_PATH "/mnt/sdcard"

/**
 * @brief TF card event callback function
 *
 * @param event Event triggered during the TF card monitoring process
 */
typedef void (*ipc_tfcard_monitor_event_f)(ipc_tfcard_monitor_event_e event);

/**
 * @brief TF card monitoring module initialization
 *
 * @param f_event Event callback function, can be NULL
 * @return Standard return value from ipc_std.h
 */
EXAPI s32 ipc_tfcard_monitor_init(ipc_tfcard_monitor_event_f f_event);

/**
 * @brief TF card monitoring module cleanup
 *
 * @param is_wait 0: Only notify the module to exit, 1: Wait for the module to exit
 */
EXAPI void ipc_tfcard_monitor_uninit(u8 is_wait);

/**
 * @brief Format the TF card
 */
EXAPI void ipc_tfcard_format(void);

/**
 * @brief Remount the TF card
 */
EXAPI void ipc_tfcard_remount(void);

/**
 * @brief Get the current supported filesystem name, 'auto' means the system automatically determines, 'gvffs' indicates support for the gvffs
 * filesystem
 *
 * @return pv8 Filesystem name
 */
EXAPI pv8 ipc_tfcard_monitor_get_current_filesystem(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_TFCARD_MONITOR_H__