#ifndef __IPC_FACTORY_H__
#define __IPC_FACTORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_middleware_config.h"
#include <ipc_core.h>

typedef enum {
    IPC_MONO_LRLED,
    IPC_MONO_WHLED,
    IPC_DUAL_LIGHT,
} ipc_light_ctrl_mode_e;

typedef struct {
    u8 version;
    u8 spk_vol;                // Speaker volume (percentage)
    u8 spk_gain;               // Speaker volume gain (1-30)
    u8 mic_vol;                // Microphone volume (percentage)
    u8 mic_gain;               // Microphone volume gain (1-30)
    u8 image_flip;             // Whether the image needs to be flipped
    u8 ircut_flip;             // Whether ircut needs to be flipped
    u8 irled_flip;             // Whether the infrared LED needs to be flipped
    u8 light_sensor_flip;      // Whether the light sensor needs to be flipped
    u8 white_light_flip;       // Whether the white light needs to be flipped
    u8 flood_light_flip;       // Whether the floodlight (PWM IO port, e.g., D5) needs to be flipped
    u8 indicator_lighta_flip;  // Whether indicator light A needs to be flipped
    u8 indicator_lightb_flip;  // Whether indicator light B needs to be flipped
    u8 spk_flip;               // Whether speaker switch validity needs to be flipped
    u8 light_ctrl_mode;        // 0: Single infrared 1: Single white light 2: Dual light sources (infrared + white light)
    f32 lens_max_angle;        // Maximum visible angle of the lens (1-180 diagonal), related to the motor rotation angle for motion tracking
    f32 ptz_v_max_angle;       // Maximum angle the vertical motor can turn (1-360) limited by the housing
    f32 ptz_h_max_angle;       // Maximum angle the horizontal motor can turn (1-360) limited by the housing
    f32 ptz_v_init_angle;      // Initial angle of the vertical motor after self-check (0-360), 0 means default is half of the maximum angle
    f32 ptz_h_init_angle;      // Initial angle of the horizontal motor after self-check (0-360), 0 means default is half of the maximum angle
    f32 ptz_v_limit_min_angle; // Minimum physical working angle of the vertical motor
    f32 ptz_h_limit_min_angle; // Minimum physical working angle of the horizontal motor
    f32 ptz_v_limit_max_angle; // Maximum physical working angle of the vertical motor
    f32 ptz_h_limit_max_angle; // Maximum physical working angle of the horizontal motor
    u16 ptz_circle_step;       // Step value for the motor to complete a full rotation (step pitch: 5.625/64 calculated as: 360/(5.625/64))
    u16 ptz_track_reset_ts;    // Reset time for motion tracking (seconds)
    u8 ptz_v_flip;             // Whether the direction of the vertical motor movement needs to be reversed
    u8 ptz_h_flip;             // Whether the direction of the horizontal motor movement needs to be reversed
    u8 ptz_ctrl_speed;         // Speed of the motor when manually controlled via the app (1-10, smaller values are faster)
    u8 ptz_track_speed;        // Speed of the motor during motion tracking (1-10, smaller values are faster)
    u8 ptz_h_ctrl_speed;  // Speed of the horizontal motor when manually controlled via the app (1-10, smaller values are faster), only set this value
                          // if the default requires different speeds for both motors, default priority for this value
    u8 ptz_h_track_speed; // Speed of the horizontal motor during motion tracking (1-10, smaller values are faster), only set this value if the
                          // default requires different speeds for both motors, default priority for this value
    u8 ptz_v_ctrl_speed; // Speed of the vertical motor when manually controlled via the app (1-10, smaller values are faster), only set this value if
                         // the default requires different speeds for both motors, default priority for this value
    u8 ptz_v_track_speed;  // Speed of the vertical motor during motion tracking (1-10, smaller values are faster), only set this value if the default
                           // requires different speeds for both motors, default priority for this value
    u8 ptz_v_track_enable; // Enable vertical (vertical motor) motion tracking
    f32 ptz_h_gear_ratio;  // External gear ratio of the horizontal motor, some housings have added gears to the motor output, need to configure the
                           // corresponding gear ratio, ratio can include decimals, default is 1, no gearing
    f32 ptz_v_gear_ratio;  // External gear ratio of the vertical motor, some housings have added gears to the motor output, need to configure the
                           // corresponding gear ratio, ratio can include decimals, default is 1, no gearing
    u8 ptz_self_check;     // Whether to start motor self-check
    u8 ptz_position_in_privacy_mode; // Position of the motors when privacy mode is activated, 0: do nothing, 1: up, 2: down
    u8 disable_light_sensor;         // 0: product default 1: disable light sensor, force use sensor light measurement mode

    v8 pid[64];                     // PID to replace or modify
    v8 language[8];                 // Language setting for voice prompts en: English ru: Russian ge: German
    u8 laohua;                      // Device aging function, 1 enabled, 0 disabled (default)
    u32 test_number;                // Test slot number, 0 indicates not set
    u16 disable_sensor_det_protect_lock_s;
    u8 sensor_det_measure_time_s;
    u16 mplitude_80db;         // Amplitude value at 80dB, default 10000
    s32 day_to_night_exp_val;  // Exposure value for switching from day to night vision
    s32 night_to_day_exp_val;  // Exposure value for switching from night vision to day
    s32 night_to_day_g_r_diff; // Green-red difference
    s32 night_to_day_g_b_diff; // Green-blue difference
    v8 ptz_gpioH_seq[12];      // Horizontal PTZ GPIO sequence, default 0,1,2,3
    v8 ptz_gpioV_seq[12];      // Vertical PTZ GPIO sequence, default 0,1,2,3
} ipc_factory_parm_t, *ipc_factory_parm_p;

EXAPI ipc_factory_parm_p ipc_factory_parm_get(void);
#define ipc_factory(name) ipc_factory_parm_get()->name

/****************************************************************/

enum IPC_FTY_ERRCODE {
    IPC_FTY_ERRCODE_NOT_SUPPORT = 1, // Command not supported
    IPC_FTY_ERRCODE_SUCCESS,         // Execution successful
    IPC_FTY_ERRCODE_FAIL,            // Execution failed
    IPC_FTY_ERRCODE_INVALID_PARAM,
};

typedef enum {
    IPC_FTY_FEEDER_FEED,                  ///< Command the feeder to dispense food
    IPC_FTY_PUT_AUDIO,                    ///< Push audio outward
    IPC_FTY_RESET_DEVICE,                 ///< Reset the device
    IPC_FTY_GET_EXINFO,                   ///< Get extended parameters
    IPC_FTY_MCU_SELF_CHECK,               ///< Command MCU self-check
    IPC_FTY_GET_TEMPERATURE_AND_HUMIDITY, ///< Get temperature and humidity values
    IPC_FTY_THROWER_THROW_FOOD,           /// Command the thrower to dispense food
    IPC_FTY_QR_DECODE, /// Decode QR data, send the decoded string through ipc_factory_cmd_send, exinfo parameter forcibly converted type
} ipc_fty_misc_e;

typedef struct {
    vptr yuv;
    s32 w;
    s32 h;
} ipc_fty_yuv_t, *ipc_fty_yuv_p;

typedef struct { // Used for IPC_FTY_GET_EXINFO

    struct {
        v8 key[24];  ///< Key for extended data
        v8 val[104]; ///< Value for extended data
    } string[10];    ///< Maximum of 10 sets of key-value pairs (values are strings)

    struct {
        v8 key[24]; ///< Key for extended data
        s32 val;    ///< Value for extended data
    } integer[10];  ///< Maximum of 10 sets of key-value pairs (values are integers)

} ipc_fty_exinfo_t, *ipc_fty_exinfo_p;

/**
 * @brief Callback for extended control commands during factory testing.
 *
 * @param cmd The callback command.
 * @param data Data address.
 * @param len Data length.
 * @return <0: Standard return value from ipc_std.h >=0 Depending on the command.
 */
typedef s32 (*ipc_factory_misc_cb_f)(ipc_fty_misc_e cmd, vptr data, s32 len);

/**
 * @brief Factory testing mode.
 *
 * @param cloud Name of the cloud platform.
 * @param version Firmware version number.
 * @param f_misc_cb Some special callbacks for factory testing.
 * @return Standard return value from ipc_std.h.
 * @note This function call after sd is mounted
 */
EXAPI s32 ipc_factory_try_run(pv8 cloud, pv8 version, ipc_factory_misc_cb_f f_misc_cb);

/**
 * @brief Response to a factory tool command request.
 * @param cmd Factory testing command.
 * @param errcode Error code.
 * @param errmsg Error message.
 * @param exinfo Response result.
 * @return
 * @note Only effective internally when the tool is connected.
 */
EXAPI void ipc_factory_cmd_send(ipc_fty_misc_e cmd, s32 errcode, pv8 errmsg, ipc_fty_exinfo_p exinfo);

/**
 * @brief Start aging log recording.
 * @param aging_log_dir Directory to save the aging logs.
 * @note Automatically records system information every two minutes internally.
 */

EXAPI void ipc_factory_aging_test(pv8 aging_log_dir);

/**
 * @brief Create a silent reboot flag.
 *
 */
EXAPI s32 ipc_factory_create_silent_reboot_flag(void);

/**
 * @brief Test and remove the silent reboot flag.
 * @return 0 No silent reboot flag present, 1 Silent reboot flag present.
 */
EXAPI s32 ipc_factory_test_and_rm_silent_reboot_flag(void);

/**
 * @brief update factory param
 *
 * @param src_file
 * @return Standard return value from ipc_std.h.
 */
EXAPI s32 ipc_factory_param_update(pv8 src_file);

#ifdef __cplusplus
}
#endif

#endif //__IPC_FACTORY_H__