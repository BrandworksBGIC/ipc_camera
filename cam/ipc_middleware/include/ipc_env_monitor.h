#ifndef __IPC_ENV_MONITOR_H__
#define __IPC_ENV_MONITOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

typedef enum {
    IPC_ENV_AUTO,             ///< Auto mode, lights turn on only when motion is detected at night
    IPC_ENV_INFRARED,         ///< Infrared mode, lights never turn on at night
    IPC_ENV_FULL_COLOR,       ///< Full color mode, lights always on at night
    IPC_ENV_MODE_DAY,         ///< Daytime mode
    IPC_ENV_MODE_NIGHT,       ///< Nighttime mode
    IPC_ENV_WITH_NO_INFRARED, ///< Dark mode, lights never turn on at night, infrared lights also off, but normal day-night switching
    IPC_ENV_MODE_NUM,
} ipc_env_mode_e;

/**
 * @brief Initializes the day/night environment monitoring
 *
 * @param mode Initializes the current control mode
 * @return IPC_SUCCESS: Success; other: Failure
 */
EXAPI s32 ipc_env_monitor_init(ipc_env_mode_e mode);

/**
 * @brief Destroys resources for the day/night environment monitoring
 *
 * @param is_wait 0: Notify the thread to exit (no waiting) 1: Wait for the thread to exit
 */
EXAPI void ipc_env_monitor_uninit(u8 is_wait);

/**
 * @brief Sets the video color mode
 *
 * @param mode See ipc_env_mode_e
 */
EXAPI void ipc_env_set_mode(ipc_env_mode_e mode);

/**
 * @brief Triggers the lights to turn on for a certain period (at night, in auto mode)
 *
 * @param sec Duration the lights stay on, after which they revert to the regular settings if not triggered again
 */
EXAPI void ipc_light_trigger(u32 sec);

/**
 * @brief Sets the food light brightness
 *
 * @param lightness_percent Brightness percentage
 */
EXAPI void ipc_light_set_lightness(s32 lightness_percent);

/**
 * @brief Forces the lights to turn on
 * @param is_open 1, forces the lights to turn on, 0, turns off forced lighting, 2, forces the white light to turn off in full color mode, -1, forces
 * the white light to turn off (for the Anju Cloud platform)
 */

EXAPI void ipc_light_force_open(s32 is_open);

/***************************** Internal ********************************/

u8 ipc_set_ircut(u8 sw);
u8 ipc_flip_ircut(void);

u8 ipc_set_color(u8 status);
u8 ipc_flip_color(void);

u8 ipc_set_rled(u8 status);
u8 ipc_flip_rled(void);

u8 ipc_set_bled(u8 sw);
u8 ipc_flip_bled(void);

/************************* Alarm ************************/

u8 ipc_env_block_alarm(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_ENV_MONITOR_H__