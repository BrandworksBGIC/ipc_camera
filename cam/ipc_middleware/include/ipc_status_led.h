#ifndef __GV_STATUS_LED_H__
#define __GV_STATUS_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

typedef enum {
    IPC_STATUS_LED_A, ///< Status LED A, the specific meaning is determined by the requirements and board type
    IPC_STATUS_LED_B, ///< Status LED B, the specific meaning is determined by the requirements and board type
    IPC_STATUS_LED_NUM,
} ipc_status_led_e;

typedef struct {
    ipc_status_led_e led; ///< The LED that needs to be set
    s32 effective_tms;   ///< The effective time that needs to be staggered (for synchronizing LED timing, only effective in the ipc_status_led_set_all
                         ///< interface)
    s32 light_on_tms;    ///< Light-on time (milliseconds)
    s32 light_off_tms;   ///< Light-off time (milliseconds)
} ipc_status_led_ctrl_t, *ipc_status_led_ctrl_p;

/**
 * @brief Initialize the status indicator LEDs
 *
 * @return ipc_std standard return value
 */
EXAPI s32 ipc_status_led_init(void);

/**
 * @brief Destroy the status indicator LEDs
 *
 * @param is_wait Notify module resource destruction (do not wait) 1: Wait for module resource destruction
 */
EXAPI void ipc_status_led_uninit(u8 is_wait);

/**
 * @brief Set the on/off pattern (on/off time ratio) for a single status indicator LED
 *
 * @param ctrl Parameters for the controlled LED
 * @note When both light_on_tms and light_off_tms are 0, it is defined as turning off the light
 */
EXAPI void ipc_status_led_set_one(ipc_status_led_ctrl_p ctrl);

/**
 * @brief Set the on/off pattern (on/off time ratio) for all status indicator LEDs and forcibly synchronize the timing of all LEDs
 *
 * @param ctrls All LED control parameters
 * @param ctrl_num Number of control parameters (array length)
 * @note When both light_on_tms and light_off_tms are 0, it is defined as turning off the light
 * @note After each call to this interface, any LED that was not set will be in the off state (so if multiple LEDs need to work together, they should
 * be set at once)
 */
EXAPI void ipc_status_led_set_all(ipc_status_led_ctrl_p ctrls, u32 ctrl_num);

// Compatibility with old API
#define ipc_status_led_set ipc_status_led_set_all

/**
 * @brief Control the master switch for the indicator lights.
 *
 * @param sw 1: Turn on the master switch for the indicator lights 0: Turn off the master switch for the indicator lights
 */
EXAPI void ipc_status_led_switch(u8 sw);

#ifdef __cplusplus
}
#endif

#endif //__IPC_STATUS_LED_H__