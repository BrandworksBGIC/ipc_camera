#ifndef __IPC_PTZ_H__
#define __IPC_PTZ_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

/* Motor direction */
typedef enum {
    IPC_PTZ_ANTICLKWISE, ///< Counterclockwise
    IPC_PTZ_CLKWISE,     ///< Clockwise
} ipc_ptz_turn_e;

/* Motor action */
typedef enum {
    IPC_PTZ_H, ///< Horizontal
    IPC_PTZ_V, ///< Vertical
    IPC_PTZ_ACTION_NUM,
} ipc_ptz_action_e;

/* Motor direction */
typedef enum {
    IPC_PTZ_LEFT  = (IPC_PTZ_H << 1) | IPC_PTZ_ANTICLKWISE, ///< Horizontal counterclockwise
    IPC_PTZ_RIGHT = (IPC_PTZ_H << 1) | IPC_PTZ_CLKWISE,     ///< Horizontal clockwise
    IPC_PTZ_UP    = (IPC_PTZ_V << 1) | IPC_PTZ_ANTICLKWISE, ///< Vertical counterclockwise
    IPC_PTZ_DOWN  = (IPC_PTZ_V << 1) | IPC_PTZ_CLKWISE,     ///< Vertical clockwise
    IPC_PTZ_CTRL_NUM,
} ipc_ptz_dir_e;

/**
 * @brief PTZ initialization
 *
 * @return ipc_std standard return value
 */
EXAPI s32 ipc_ptz_init(void);

/**
 * @brief Flip the PTZ motor direction
 *
 * @param act The motor to be set (horizontal or vertical)
 * @param is_flip 0: Reset 1: Flip
 */
EXAPI void ipc_ptz_flip(ipc_ptz_action_e act, s8 is_flip);

/**
 * @brief Check if the PTZ motor is stopped
 *
 * @param act The motor to be checked (horizontal or vertical)
 * @return 0: Not stopped; 1: Stopped
 */
EXAPI s32 ipc_ptz_is_stop(ipc_ptz_action_e act);

/**
 * @brief Stop the PTZ motor
 *
 * @param act The motor to be stopped (horizontal or vertical)
 */
EXAPI void ipc_ptz_stop(ipc_ptz_action_e act);

/**
 * @brief Destroy PTZ resources
 */
EXAPI void ipc_ptz_uninit(void);

/**
 * @brief Control the PTZ motor direction and angle (relative angle)
 *
 * @param ctrl Motor direction (up, down, left, right)
 * @param angle Rotation angle 1-360
 */
EXAPI void ipc_ptz_turn(ipc_ptz_dir_e ctrl, f32 angle);

/**
 * @brief Set the motor speed (affects motor speed except for motion tracking)
 *
 * @param act The motor to be set (horizontal or vertical)
 * @param speed Motor speed, range 1-10, floating point range
 * @note Due to various casing reasons, the motor may not move even within the specified range; usage depends on the situation.
 */
EXAPI void ipc_ptz_speed(ipc_ptz_action_e act, f32 speed);

/**
 * @brief Control the PTZ motor to a specific angle (absolute angle)
 *
 * @param act The motor to be set (horizontal or vertical)
 * @param angle Rotation angle 1-360
 */
EXAPI void ipc_ptz_turn_abs(ipc_ptz_action_e act, f32 angle);

/**
 * @brief Get the current angle of the PTZ motor (absolute angle)
 *
 * @param act The motor to be checked (horizontal or vertical)
 * @param angle Current absolute angle
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_ptz_get_abs(ipc_ptz_action_e act, pf32 angle);

/**
 * @brief Motor auto-rotation
 *
 * @param act The motor to be set (horizontal or vertical)
 */
EXAPI void ipc_ptz_turn_auto(ipc_ptz_action_e act);

/**
 * @brief Recheck (used when the motor has been manually moved out of alignment, recheck to rebuild the positioning model)
 *
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_ptz_recheck(void);

/**
 * @brief Set the motor reference position angle (affects the reset position for motion tracking and the target position during motor initialization
 * self-check)
 *
 * @param act The motor to be set (horizontal or vertical)
 * @param angle >= 0: Valid angle <0: Clear angle setting (note floating-point comparison precision issues, generally use -1)
 */
EXAPI void ipc_ptz_set_init_angle(ipc_ptz_action_e act, f32 angle);

/**
 * @brief Get the motor reference position angle (affects the reset position for motion tracking and the target position during motor initialization
 * self-check)
 *
 * @param act The motor to be checked (horizontal or vertical)
 * @return <0: Error in the act value >=0: Reference position angle
 */
EXAPI f32 ipc_ptz_get_init_angle(ipc_ptz_action_e act);

/**
 * @brief Set the zoom multiplier
 *
 * @param multiplier The multiplier to be set
 * @return 0: Success Non-zero: Failure
 */
EXAPI s32 ipc_ptz_zoom_multiplier_set(f32 multiplier);

/************************* Internal *******************/

/* @brief Dedicated for motion tracking use */
void ipc_ptz_track(ipc_ptz_dir_e ctrl, f32 angle, f32 speed_level);

#ifdef __cplusplus
}
#endif

#endif //__IPC_PTZ_H__