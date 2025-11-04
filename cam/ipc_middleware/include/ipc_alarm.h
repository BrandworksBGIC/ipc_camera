#ifndef __IPC_ALARM_H__
#define __IPC_ALARM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>

typedef enum {
    IPC_ALARM_MD         = 1 << 0, ///< Motion Detection
    IPC_ALARM_AI         = 1 << 1, ///< AI Human Detection (TODO: Later change to AI_PEOPLE)
    IPC_ALARM_PIR        = 1 << 2, ///< PIR Alarm
    IPC_ALARM_AI_VEHICLE = 1 << 3, ///< AI Vehicle Detection
} ipc_alarm_type_e;

typedef struct {
    ipc_alarm_type_e alarm_type; ///< Alarm box type
    u16 lux;                    ///< Top-left X coordinate
    u16 luy;                    ///< Top-left Y coordinate
    u16 rdx;                    ///< Bottom-right X coordinate
    u16 rdy;                    ///< Bottom-right Y coordinate
} ipc_alarm_rect_t, *ipc_alarm_rect_p;

typedef struct {
    s32 width;               // Base pixel width for detection results
    s32 height;              // Base pixel height for detection results
    f32 alarm_image_percent; // Alarm image percentage, useful for more sophisticated MD sensitivity filtering. Not supported by AI, value is -1.
} ipc_alarm_result_extinfo_t, *ipc_alarm_result_extinfo_p;

/**
 * @brief Alarm detection callback
 *
 * @param rect_info Box information
 * @param rect_num Number of boxes
 * @param extinfo Extended information description
 */
typedef void (*ipc_alarm_detect_f)(ipc_alarm_rect_p rect_info, s32 rect_num, ipc_alarm_result_extinfo_p extinfo);

/**
 * @brief Initialize the alarm module
 *
 * @return ipc_std Standard return value
 */
EXAPI s32 ipc_alarm_init(ipc_alarm_detect_f f_detect);

/**
 * @brief Destroy the alarm module
 *
 * @param is_wait 0: Notify the module to exit (only notification, does not destroy resources) 1: Synchronously wait for destruction (completely
 * destroys resources)
 */
EXAPI void ipc_alarm_uninit(u8 is_wait);

/**
 * @brief AI vehicle detection switch (affects alarm method and motion tracking)
 *
 * @param is_enable 1: Enable 0: Disable
 */
EXAPI void ipc_alarm_ai_vehicle_sw(u8 is_enable);

/**
 * @brief AI human detection switch (affects alarm method and motion tracking)
 *
 * @param is_enable 1: Enable 0: Disable
 */
EXAPI void ipc_alarm_ai_sw(u8 is_enable);

/**
 * @brief Motion tracking switch
 *
 * @param is_enable 1: Enable 0: Disable
 */
EXAPI void ipc_alarm_track_sw(u8 is_enable);

/**
 * @brief Sensitivity setting
 *
 * @param type Alarm type for which to set the sensitivity
 * @param percent Sensitivity percentage
 */
EXAPI void ipc_alarm_set_sensitivity(ipc_alarm_type_e type, f32 percent);

/**
 * @brief Set the auto-reset time for motion tracking without alarm trigger (default 60s)
 *
 * @param reset_ts Reset time (seconds) (if 0, restores the factory default value)
 */
EXAPI void ipc_alarm_track_reset_ts(u16 reset_ts);

/**
 * @brief Set large scene change filter
 *
 * @param en Enable/disable switch (default enabled)
 * @param ratio Change filter ratio, -1 restores the default value (0.6)
 */
EXAPI void ipc_alarm_big_change_filter(u8 en, f32 ratio);

/**
 * @brief Auto-zoom
 *
 * @param en Auto-zoom enable
 */
EXAPI void ipc_alarm_auto_zoom_in(u8 en);

#ifdef __cplusplus
}
#endif

#endif //__IPC_ALARM_H__