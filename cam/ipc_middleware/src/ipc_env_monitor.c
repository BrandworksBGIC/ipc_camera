#include "ipc_middleware_config.h"

#include "ipc_env_monitor.h"
#include "ipc_log.h"
#include "ipc_platform_api.h"
#include "ipc_factory.h"
#include "ipc_sensor_light_det.h"
#include "ipc_decrypt.h"

typedef enum {
    IPC_ENV_DAY,    
    IPC_ENV_NIGHT,   
    IPC_ENV_NUM,
} ipc_env_e;

static struct {
    u8 gorun;                           ///< Thread control
    u8 alive;                           ///< Thread feedback
    u8 env;                             ///< ipc_env_e
    u8 mode;                            ///< ipc_env_mode_e
    u64 trigger_off_tms;                ///< Time to automatically turn off lights after triggering
    u64 io_effect_tms;                  ///< Time when IO switches that might affect imaging take effect
    s32 food_light_lightness_precent;   ///< Floodlight brightness percentage
    u8 force_in_dual_light;             ///< Force dual light mode
    s32 force_open_food_or_white_light; ///< Force flood or white light on
    u8 full_color_mode_flag;
} _gh_status[1] = { {
    .food_light_lightness_precent = 100,
} };

u8 ipc_env_block_alarm(void)
{
    return ipc_mono_tms() < _gh_status->io_effect_tms + 6000; // Block alarms for 6 seconds
}

/************** IRCUT *****************/
static u8 _g_last_ircut = (u8)-1;
u8 ipc_set_ircut(u8 status)
{
    if (_g_last_ircut == status)
        return _g_last_ircut;
    _g_last_ircut             = status;
    _gh_status->io_effect_tms = ipc_mono_tms();

    ipc_plat_api(0)->io_write(IPC_IO_NAME_IRCUT_A, !status);
    ipc_plat_api(0)->io_write(IPC_IO_NAME_IRCUT_B, status);

    ipc_msleep(300);

    ipc_plat_api(0)->io_write(IPC_IO_NAME_IRCUT_A, IPC_IO_VALUE_IS_INACTIVE);
    ipc_plat_api(0)->io_write(IPC_IO_NAME_IRCUT_B, IPC_IO_VALUE_IS_INACTIVE);

    return _g_last_ircut;
}

u8 ipc_flip_ircut(void)
{
    return ipc_set_ircut(!_g_last_ircut);
}

/************** COLOR *****************/
static u8 _g_last_color = (u8)-1;
u8 ipc_set_color(u8 status)
{
    if (_g_last_color == status)
        return _g_last_color;
    _g_last_color             = status;
    _gh_status->io_effect_tms = ipc_mono_tms();
    ipc_plat_api(0)->video_isp_image_mode_set(status, NULL);

    // When switching to night vision, wait for 300 milliseconds to avoid an ugly purple color due to immediate infrared transmission
    if (status == IPC_VIDEO_MODE_NIGHT) {
        ipc_msleep(300);
    }

    return _g_last_color;
}

u8 ipc_flip_color(void)
{
    return ipc_set_color(!_g_last_color);
}

/************** RLED *****************/
static u8 _g_last_rled = (u8)-1;
u8 ipc_set_rled(u8 status)
{
    if (_g_last_rled == status)
        return _g_last_rled;
    _g_last_rled              = status;
    _gh_status->io_effect_tms = ipc_mono_tms();
    ipc_plat_api(0)->io_write(IPC_IO_NAME_INFRARED_LIGTH, status);
    return _g_last_rled;
}

u8 ipc_flip_rled(void)
{
    return ipc_set_rled(!_g_last_rled);
}

/************** BLED *****************/
static u8 _g_last_bled = (u8)-1;
u8 ipc_set_bled(u8 status)
{
    if (_g_last_bled == status)
        return _g_last_bled;
    _g_last_bled              = status;
    _gh_status->io_effect_tms = ipc_mono_tms();

    if (status) {
        ipc_plat_api(0)->io_write(IPC_IO_NAME_FLOOD_LIGHT, _gh_status->food_light_lightness_precent);
    } else {
        ipc_plat_api(0)->io_write(IPC_IO_NAME_FLOOD_LIGHT, 0);
    }

    ipc_plat_api(0)->io_write(IPC_IO_NAME_WHITE_LIGTH, status);

    return _g_last_bled;
}

u8 ipc_flip_bled(void)
{
    return ipc_set_bled(!_g_last_bled);
}

/************** ENV *****************/

static ipc_env_e _get_env(void)
{
    IPC_IO_VALUE_TYPE value = 0;
    s32 adc_value          = 0;
    adc_value              = ipc_plat_api(0)->io_read(IPC_IO_NAME_LIGHT_SENSOR, &value);
    if (value != IPC_IO_VALUE_IS_NUMBER) {
        return value == IPC_IO_VALUE_IS_ACTIVE ? IPC_ENV_NIGHT : IPC_ENV_DAY;
    }

    ipctrace("ls_adc_val:%d", adc_value);

    struct ipc_plat_io_env_adc_info_s env_adc_info;

    ipc_plat_api(0)->misc_ctrl(IPC_PLAT_MISC_CTRL_CMD_GET_ENV_ADC_INFO, NULL, &env_adc_info);

    if (_gh_status->env == IPC_ENV_DAY) {
        return adc_value < env_adc_info.adc_change_to_night_value ? IPC_ENV_NIGHT : IPC_ENV_DAY;
    } else {
        return adc_value > env_adc_info.adc_change_to_day_value ? IPC_ENV_DAY : IPC_ENV_NIGHT;
    }
}

static void _wait_ae_to_stable(void)
{
    s32 i = 0;
    s32 j = 0;
    struct ipc_plat_isp_exp_status status;
    s32 sum         = 0;
    u32 ae_array[5] = { 0 };
    s32 offset      = 0;

    ipcdebug("1%s:%d:%ld\n", __FUNCTION__, __LINE__, time(NULL));
    // Break out of the loop on timeout or external mode change
    for (i = 0; i < 30 && _gh_status->full_color_mode_flag > 0; i++) {
        ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

        ae_array[offset] = status.ev;
        offset++;
        if (offset >= sizeof(ae_array) / sizeof(ae_array[0])) {
            offset = 0;
        }

        ipc_msleep(500);

        if (i > 5) {
            sum = 0;
            for (j = 0; j < sizeof(ae_array) / sizeof(ae_array[0]); j++) {
                sum += ae_array[j];
            }
            sum /= sizeof(ae_array) / sizeof(ae_array[0]);

            ipcdebug("3%s:%d:%u:%u:%d\n", __FUNCTION__, __LINE__, sum, status.ev, abs(sum - (int)status.ev));

            if (abs(sum - (int)status.ev) <= (sum / 1000)) {
                ipcdebug("2%s:%d:%ld\n", __FUNCTION__, __LINE__, time(NULL));
                break;
            }
        }
    }
}

static void _process_full_color_mode(pu8 bled, pu8 color, s32 is_use_sendet)
{
    static struct ipc_plat_isp_exp_status before_light_off_status;
    static u64 start_count_ms = 0;
    struct ipc_plat_isp_exp_status status;
    struct ipc_plat_isp_sensor_metering_threshold_val sensor_val = { 0 };

    switch (_gh_status->full_color_mode_flag) {
        case 0: {
            *color = _gh_status->env == IPC_ENV_DAY ? IPC_VIDEO_MODE_DAY : IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
            *bled  = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;
            // Transition to the lit state
            _gh_status->full_color_mode_flag = _gh_status->env == IPC_ENV_DAY ? 0 : 1;
            // printf("%s:%d:%d\n", __func__, __LINE__, _gh_status->full_color_mode_flag);
            break;
        }
        case 1: {
            if (_gh_status->env == IPC_ENV_DAY) {

                // Record the values before turning off the light
                _wait_ae_to_stable();

                ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &before_light_off_status);

                ipc_set_bled(IPC_IO_VALUE_IS_INACTIVE);

                _gh_status->env = IPC_ENV_DAY;

                if (is_use_sendet) {
                    _wait_ae_to_stable();

                    // Wait until the ISP stabilizes
                    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

                    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL, &sensor_val);

                    ipcdebug("1%s:%d:%d\n", __func__, status.ev, before_light_off_status.ev);
                    if (status.ev > sensor_val.day_to_night_exp_val) {
                        _gh_status->env = IPC_ENV_NIGHT;
                    }
                } else {
                    ipc_msleep(2500);

                    _gh_status->env = _get_env();
                }

                // If the environment remains daytime after turning off the white light
                if (_gh_status->env == IPC_ENV_DAY) {
                    _gh_status->full_color_mode_flag = 0;
                    *color                           = IPC_VIDEO_MODE_DAY;
                    *bled                            = IPC_IO_VALUE_IS_INACTIVE;

                } else {
                    // Reflexion occurred, avoid it
                    *color = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
                    *bled  = IPC_IO_VALUE_IS_ACTIVE;
                    // Change the scene to daytime to prevent repeated switching
                    _gh_status->env = IPC_ENV_DAY;

                    ipc_set_bled(IPC_IO_VALUE_IS_ACTIVE);

                    _gh_status->full_color_mode_flag = 2;

                    start_count_ms = 0;

                    _wait_ae_to_stable();
                }
            } else {
                *color = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
                *bled  = IPC_IO_VALUE_IS_ACTIVE;
            }
            break;
        }
        default: {
            // Anti-reflection state

            if (_gh_status->env == IPC_ENV_NIGHT) {
                // Reflexion disappears
                _gh_status->full_color_mode_flag = 1;
                *color                           = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
                *bled                            = IPC_IO_VALUE_IS_ACTIVE;
                start_count_ms                   = 0;
                break;
            }

            ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL, &sensor_val);

            ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

            ipcdebug("%s:%d:%d\n", __func__, status.ev, before_light_off_status.ev);

            // If it is twice as dark as before turning off the light or darker than the transition from day to night, assume the reflexion source has
            // disappeared
            if (status.ev > before_light_off_status.ev * 3) {
                // Reflexion disappears
                _gh_status->full_color_mode_flag = 1;
                *color                           = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
                *bled                            = IPC_IO_VALUE_IS_ACTIVE;
                start_count_ms                   = 0;
                break;
            }

            // If it is brighter by 1/5 of the AE value before turning off the light, try again
            if (status.ev < before_light_off_status.ev - (before_light_off_status.ev / 5)) {
                if (start_count_ms == 0) {
                    start_count_ms = ipc_mono_tms();
                }
            } else {
                start_count_ms = 0;
            }

            // If it remains brighter than before turning off the light for 15 seconds, retry once
            if (ipc_mono_tms() - start_count_ms > 1000 * 15 && start_count_ms > 0) {
                start_count_ms                   = 0;
                _gh_status->full_color_mode_flag = 1;
                break;
            }

            break;
        }
    }
}

static u8 _update_io_status(u64 now_tms, s32 is_use_sendet)
{
    /* Default parameters */
    u8 ignore_flag = 0;
    u8 rled        = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;
    u8 bled        = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_INACTIVE;
    u8 color       = _gh_status->env == IPC_ENV_DAY ? IPC_VIDEO_MODE_DAY : IPC_VIDEO_MODE_NIGHT;

    if ((ipc_factory(light_ctrl_mode) == IPC_DUAL_LIGHT) || _gh_status->force_in_dual_light) { /* Dual light source */
        if ((_gh_status->mode == IPC_ENV_AUTO) && (now_tms < _gh_status->trigger_off_tms)) {
            rled  = IPC_IO_VALUE_IS_INACTIVE;
            bled  = IPC_IO_VALUE_IS_ACTIVE;
            color = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;

            ignore_flag = 2;
        }
    }

    if (_gh_status->mode == IPC_ENV_FULL_COLOR) {
        rled  = IPC_IO_VALUE_IS_INACTIVE;
        bled  = IPC_IO_VALUE_IS_ACTIVE;
        color = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;

        // Various peculiar forced modes, skip reflection detection to avoid turning lights on or off
        if (_gh_status->force_open_food_or_white_light == 0) {
            _process_full_color_mode(&bled, &color, is_use_sendet);
        } else {
            color = _gh_status->env == IPC_ENV_DAY ? IPC_VIDEO_MODE_DAY : IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
            bled  = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;
        }
    }

    if (ipc_factory(light_ctrl_mode) == IPC_MONO_WHLED) {
        color = _gh_status->env == IPC_ENV_DAY ? IPC_VIDEO_MODE_DAY : IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
        rled  = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;
        bled  = _gh_status->env == IPC_ENV_DAY ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;
    }

    if (_gh_status->mode == IPC_ENV_MODE_DAY) {
        rled        = IPC_IO_VALUE_IS_INACTIVE;
        bled        = IPC_IO_VALUE_IS_INACTIVE;
        color       = IPC_VIDEO_MODE_DAY;
        ignore_flag = 1;
    } else if (_gh_status->mode == IPC_ENV_MODE_NIGHT) {
        rled        = IPC_IO_VALUE_IS_ACTIVE;
        bled        = IPC_IO_VALUE_IS_INACTIVE;
        color       = IPC_VIDEO_MODE_NIGHT;
        ignore_flag = 1;
    } else if (_gh_status->mode == IPC_ENV_WITH_NO_INFRARED) {
        rled = IPC_IO_VALUE_IS_INACTIVE;
        bled = IPC_IO_VALUE_IS_INACTIVE;
    }

    if (_gh_status->force_open_food_or_white_light == 1) {
        rled        = IPC_IO_VALUE_IS_INACTIVE;
        bled        = IPC_IO_VALUE_IS_ACTIVE;
        color       = IPC_VIDEO_MODE_NIGHT_FULL_COLOR;
        ignore_flag = 1;
    } else if (_gh_status->force_open_food_or_white_light == 2) {
        if (color == IPC_VIDEO_MODE_NIGHT_FULL_COLOR) {
            bled = IPC_IO_VALUE_IS_INACTIVE;
        }
    } else if (_gh_status->force_open_food_or_white_light == -1) {
        bled = IPC_IO_VALUE_IS_INACTIVE;
    }

    static u8 last_rled  = -1;
    static u8 last_bled  = -1;
    static u8 last_color = -1;
    u8 ircut             = 0;
    if (last_rled == rled && last_bled == bled && last_color == color) { 
        return ignore_flag;
    }
    last_rled  = rled;
    last_bled  = bled;
    last_color = color;

    ircut = color == IPC_VIDEO_MODE_NIGHT ? IPC_IO_VALUE_IS_INACTIVE : IPC_IO_VALUE_IS_ACTIVE;

    ipc_set_color(color);
    ipc_set_ircut(ircut);
    ipc_set_rled(rled);
    ipc_set_bled(bled);

    ipcdebug("ircut: %d", !rled);
    ipcdebug("rled : %d", rled);
    ipcdebug("color: %d", color);
    ipcdebug("bled : %d", bled);
    ipcdebug("trigger light: %d", now_tms < _gh_status->trigger_off_tms);

    return ignore_flag;
}

static vptr _pth_env_listen(vptr arg)
{
    clog_init("env", "Respond to activities based on day and night");

    _gh_status->alive = 1;
    _gh_status->env = _get_env(); 

    u8  env = 0;
    u64 now_tms = 0; 
    u64 sw_tms  = 0;
 
    while (_gh_status->gorun) {
        
        env = _get_env();
        now_tms = ipc_mono_tms();
        
        if (_gh_status->env != env) {
            if (!sw_tms) {
                sw_tms = now_tms + 3000; 
            }
            else if (now_tms >= sw_tms) {
                sw_tms = 0;
                _gh_status->env = env;
            }
        } else {
            sw_tms = 0;
        }

        _update_io_status(now_tms, 0);
        ipc_msleep(100);
    }

    _gh_status->alive = 0;

    return NULL;
}

static void __cmd_handler(s32 cmd, vptr __user)
{
    s32 det_mode = *(ps32)__user;
    if (det_mode == IPC_SL_DET_AUTO) {
        _gh_status->env = cmd == IPC_SL_DET_DAY_MODE ? IPC_ENV_DAY : IPC_ENV_NIGHT;
    }

    ipctrace("%s: env %d : mode %d\n", __func__, _gh_status->env, _gh_status->mode);
}

static vptr _pth_sensor_env_listen(vptr arg)
{
    clog_init("env", "Respond to activities based on day and night");

    _gh_status->alive = 1;
    //_gh_status->env = ; // Initial state

    struct ipc_sensor_light_det_attr attr                        = { 0 };
    struct ipc_plat_isp_sensor_metering_threshold_val sensor_val = { 0 };
    u8 ignore_flag                                              = 0;
    u32 cur_ev_value                                            = 0;
    u32 delay_count                                             = 0;
    s32 det_mode                                                = IPC_SL_DET_AUTO;

    attr.cmd_handler           = __cmd_handler;
    attr.user                  = &det_mode;
    attr.day_to_night_exp_val  = 40000;
    attr.night_to_day_exp_val  = 8000;
    attr.night_to_day_wb_bgain = 1;
    attr.night_to_day_wb_rgain = 235;

    if (ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL, &sensor_val) == 0) {
        attr.day_to_night_exp_val  = sensor_val.day_to_night_exp_val;
        attr.night_to_day_exp_val  = sensor_val.night_to_day_exp_val;
        attr.night_to_day_wb_bgain = sensor_val.night_to_day_wb_r_g_diff;
        attr.night_to_day_wb_rgain = sensor_val.night_to_day_wb_b_g_diff;
    }

    attr.day_to_night_exp_val  = ipc_factory(day_to_night_exp_val) >= 0 ? ipc_factory(day_to_night_exp_val) : attr.day_to_night_exp_val;
    attr.night_to_day_exp_val  = ipc_factory(night_to_day_exp_val) >= 0 ? ipc_factory(night_to_day_exp_val) : attr.night_to_day_exp_val;
    attr.night_to_day_wb_bgain = ipc_factory(night_to_day_g_r_diff) >= 0 ? ipc_factory(night_to_day_g_r_diff) : attr.night_to_day_wb_bgain;
    attr.night_to_day_wb_rgain = ipc_factory(night_to_day_g_b_diff) >= 0 ? ipc_factory(night_to_day_g_b_diff) : attr.night_to_day_wb_rgain;

    ipc_sensor_light_det_init(&attr);

    while (_gh_status->gorun) {

        ipc_msleep(500);

        struct ipc_plat_isp_exp_status status;

        ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

        switch (_gh_status->mode) {
            case IPC_ENV_MODE_NIGHT:
                det_mode = IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON;
                break;
            case IPC_ENV_MODE_DAY:
                det_mode = IPC_SL_DET_DAY_MODE;
                break;
            default:
                // Infrared mode also changed to automatic, allowing switching between day and night. Changed to match the light sensor's behavior.
                // Initially, under sensor metering mode, night vision and infrared modes were not separated. This modification could cause devices
                // locked in night vision to switch back to白天. Considering that few users utilize this mode, this issue is recognized but will not
                // be addressed further. Users merely need to switch to another mode and then back again to resolve it, avoiding accruing technical
                // debt.
                det_mode = IPC_SL_DET_AUTO;
                if (ignore_flag == 2) {
                    delay_count++;
                    if (delay_count >= 20 && status.ev < attr.night_to_day_exp_val) {
                        if (status.ev > cur_ev_value) {
                            // Became darker, refresh the threshold
                            cur_ev_value    = status.ev;
                            _gh_status->env = IPC_ENV_NIGHT;
                            ipcdebug("----------Restore in night mode now EV value %u--------------\n", cur_ev_value);
                        } else if (status.ev < cur_ev_value - 2) {
                            // Indeed became brighter, can switch to day mode
                            cur_ev_value    = status.ev;
                            _gh_status->env = IPC_ENV_DAY;
                            ipcdebug("---------------Restore in day mode now EV value %u--------------\n", cur_ev_value);
                            // Clear cached statistics
                            ipc_sensor_light_det_process(IPC_SL_DET_DAY_MODE, &status);

                            _gh_status->io_effect_tms = ipc_mono_tms();
                        }
                    }
                }
                break;
        }

        // When the sensor is metering, in a locked state, statistics should not be updated
        if (ignore_flag == 0) {
            delay_count  = 0;
            cur_ev_value = 0;
            ipc_sensor_light_det_process(det_mode, &status);
        }

        // Must process the sensor metering first; upon entering the lock state, notify the sensor metering module to clear statistics and then lock,
        // without conducting further statistics collection
        ignore_flag = _update_io_status(ipc_mono_tms(), 1);

        ipctrace("%s:ignore_flag:%d env %d : mode %d\n", __func__, ignore_flag, _gh_status->env, _gh_status->mode);
    }

    _gh_status->alive = 0;

    return NULL;
}
static vptr _pth_single_white_light_sensor_env_listen(vptr arg)
{
    clog_init("env", "Respond to activities based on day and night");

    _gh_status->alive = 1;

    struct ipc_sensor_light_det_attr attr                        = { 0 };
    struct ipc_plat_isp_sensor_metering_threshold_val sensor_val = { 0 };
    struct ipc_plat_isp_exp_status status;
    int state_switch_count                          = 0;
    int darker_count                                = 0;
    int update_night_to_day_ae_val_flag             = 0;
    unsigned int cur_night_to_day_exp_val           = 0;
    unsigned long long last_switch_day_to_night_tms = 0;
    int delay_count                                 = 0;

    attr.day_to_night_exp_val  = 40000;
    attr.night_to_day_exp_val  = 8000;
    attr.night_to_day_wb_bgain = 235;
    attr.night_to_day_wb_rgain = 235;

    if (ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL, &sensor_val) == 0) {
        attr.day_to_night_exp_val  = sensor_val.day_to_night_exp_val;
        attr.night_to_day_exp_val  = sensor_val.night_to_day_exp_val;
        attr.night_to_day_wb_bgain = sensor_val.night_to_day_wb_b_g_diff;
        attr.night_to_day_wb_rgain = sensor_val.night_to_day_wb_r_g_diff;
    }

    attr.day_to_night_exp_val  = ipc_factory(day_to_night_exp_val) >= 0 ? ipc_factory(day_to_night_exp_val) : attr.day_to_night_exp_val;
    attr.night_to_day_exp_val  = ipc_factory(night_to_day_exp_val) >= 0 ? ipc_factory(night_to_day_exp_val) : attr.night_to_day_exp_val;
    attr.night_to_day_wb_bgain = ipc_factory(night_to_day_g_r_diff) >= 0 ? ipc_factory(night_to_day_g_r_diff) : attr.night_to_day_wb_bgain;
    attr.night_to_day_wb_rgain = ipc_factory(night_to_day_g_b_diff) >= 0 ? ipc_factory(night_to_day_g_b_diff) : attr.night_to_day_wb_rgain;

    cur_night_to_day_exp_val = attr.night_to_day_exp_val;

    _gh_status->full_color_mode_flag = 1; // Wait for AE stability
    _wait_ae_to_stable();
    _gh_status->full_color_mode_flag = 0;

    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

    if (status.ev < cur_night_to_day_exp_val) {
        _gh_status->env = IPC_ENV_DAY;
    } else {
        _gh_status->env = IPC_ENV_NIGHT;
    }

    while (_gh_status->gorun) {

        ipc_msleep(500);

        if (delay_count > 0) {
            delay_count -= 1;
            ipctrace("%s delay_count:%d", __func__, delay_count);
            continue;
        }

        u64 now_tms = ipc_mono_tms();

        ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

        if ((_gh_status->env == IPC_ENV_DAY) && (status.ev > attr.day_to_night_exp_val)) {
            state_switch_count++;
            if (state_switch_count > 2 * 10) {
                state_switch_count               = 0;
                darker_count                     = 0;
                _gh_status->env                  = IPC_ENV_NIGHT;
                _gh_status->full_color_mode_flag = 1; // Wait for AE stability
                update_night_to_day_ae_val_flag  = 1;
            }
        } else if (_gh_status->env == IPC_ENV_NIGHT) {
            if (status.ev < cur_night_to_day_exp_val) {
                state_switch_count++;
                if (state_switch_count > 2 * 10) {
                    state_switch_count               = 0;
                    darker_count                     = 0;
                    _gh_status->env                  = IPC_ENV_DAY;
                    _gh_status->full_color_mode_flag = 0;
                }
            } else if (status.ev > cur_night_to_day_exp_val * 1.2 || status.ev > attr.night_to_day_exp_val) {
                darker_count++;
                state_switch_count = 0;
                if (darker_count > 2 * 10) {
                    darker_count             = 0;
                    cur_night_to_day_exp_val = attr.night_to_day_exp_val;
                }
            } else {
                state_switch_count = 0;
            }

        } else {
            state_switch_count = 0;
            darker_count       = 0;
        }

        _update_io_status(now_tms, 1);

        ipctrace("%s env %d : count %d: ev:[%d --<dn:%d nd:%d>--]\n", __func__, _gh_status->env, state_switch_count, status.ev,
               attr.day_to_night_exp_val, cur_night_to_day_exp_val);

        if (update_night_to_day_ae_val_flag) {
            update_night_to_day_ae_val_flag = 0;

            if (now_tms - last_switch_day_to_night_tms < 60 * 1000) {
                delay_count = 2 * 2 * 60;
                // If it switches back within 35 seconds, consider it a reflection and delay for two minutes.
            }

            last_switch_day_to_night_tms = now_tms;

            _wait_ae_to_stable();

            ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, &status);

            cur_night_to_day_exp_val = status.ev - status.ev / 5;
        }
    }

    _gh_status->alive = 0;

    return NULL;
}

void ipc_light_trigger(unsigned int sec)
{
    ipcdebug("%s env[%d] delay[%u]\n", __FUNCTION__, _gh_status->env, sec);

    if (_gh_status->env == IPC_ENV_NIGHT) {
        // coverity[OVERFLOW_BEFORE_WIDEN :SUPPRESS]
        _gh_status->trigger_off_tms = ipc_mono_tms() + (u64)sec * 1000; // Current time + trigger light-on time = turn-off time
    } else if (sec == 0) {                                            // In daytime, allow clearing the trigger light-on time
        _gh_status->trigger_off_tms = 0;
    }
}

void ipc_env_set_mode(ipc_env_mode_e mode)
{
    if (mode >= IPC_ENV_MODE_NUM)
        return;
    _gh_status->mode                 = mode;
    _gh_status->trigger_off_tms      = 0; // Turn off lights when changing modes
    _gh_status->full_color_mode_flag = 0;
}

s32 ipc_env_monitor_init(ipc_env_mode_e mode)
{
    s32 ret = 0;
    // coverity[NO_EFFECT :SUPPRESS]
    if (mode < 0 || mode >= IPC_ENV_MODE_NUM)
        return IPC_INVALID_ARGS;
    if (_gh_status->alive)
        return IPC_EXIST;

    _gh_status->gorun = 1;
    _gh_status->mode  = mode;

    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();
    // coverity[NULL_RETURNS :SUPPRESS]
    if (decrypt && decrypt->product_type == IPC_PRODUCT_TYPE_SAFE_LIGHT) {
        _gh_status->force_in_dual_light = 1;
    }

    // coverity[PW.MIXED_ENUM_TYPE :SUPPRESS]
    IPC_IO_VALUE_TYPE value = 0;

    ret = ipc_plat_api(0)->io_read(IPC_IO_NAME_LIGHT_SENSOR, &value);
    if (ret < 0 || ipc_factory(disable_light_sensor) > 0) {
        if (ipc_factory(light_ctrl_mode) == IPC_MONO_WHLED) {
            ret = ipc_create_thread("ipc_senv_monitor", _pth_single_white_light_sensor_env_listen, NULL, 64 * 1024, 0);
        } else {
            ret = ipc_create_thread("ipc_senv_monitor", _pth_sensor_env_listen, NULL, 64 * 1024, 0);
        }
    } else {
        ret = ipc_create_thread("ipc_env_monitor", _pth_env_listen, NULL, 64 * 1024, 0);
    }

    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        return ret;
    }
    ipcinfo("Init complete!");
    return ret;
}

void ipc_env_monitor_uninit(u8 is_wait) 
{
    _gh_status->gorun = 0;

    if (!is_wait) return ;

    while(_gh_status->alive) ipc_msleep(100);
    ipcinfo("Exit complete!");
}

void ipc_light_set_lightness(s32 lightness_percent)
{
    if (_gh_status->food_light_lightness_precent == lightness_percent) return ;

    _gh_status->food_light_lightness_precent = lightness_percent;
    if (_g_last_bled) {
        _gh_status->io_effect_tms = ipc_mono_tms();
        ipc_plat_api(0)->io_write(IPC_IO_NAME_FLOOD_LIGHT, _gh_status->food_light_lightness_precent);
    }
}

void ipc_light_force_open(s32 is_open)
{
    _gh_status->force_open_food_or_white_light = is_open;
}