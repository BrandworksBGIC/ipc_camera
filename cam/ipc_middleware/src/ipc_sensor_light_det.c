#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ipc_log.h"
#include "ipc_time.h"

#include "ipc_sensor_light_det.h"

#include "ipc_factory.h"

static struct {
    struct ipc_sensor_light_det_attr attr;
    s32 cur_mode;
    s32 last_switch_to_day_time;
    s32 switch_to_day_short_time_count;
    s32 ignore_time_s;
    s32 last_mode;
    s32 last_change_mode_time;
    s32 switch_to_day_exp_val;
    s32 befor_switch_to_day_exp_val;
    s32 min_switch_to_day_exp_val;
    s32 switch_to_day_total_rb_g_color_diff;
    s32 befor_switch_to_day_total_rb_g_color_diff;
    s32 befor_switch_to_day_total_r_g_color_diff;
    u32 max_r_g_color_diff;
} sndet_ctx = {
    .cur_mode  = -1,
    .last_mode = -1,
};

/*
    If in the sun, red will be very intense relative to green, which can cause misjudgment, so use multiplication. After multiplying by 0, even if the
   red component is very high, it can switch back.
*/

#define ADD_IGNORE_TIME_S(time_len) sndet_ctx.ignore_time_s = ipc_mono_ts() + (time_len);

static void __check_night_env_changed(u32 cur_exp_value)
{
    // If the scene has adapted, jump out of the limit when the change exceeds 0.3
    if (sndet_ctx.switch_to_day_exp_val < sndet_ctx.attr.night_to_day_exp_val) {
        if (abs(sndet_ctx.switch_to_day_exp_val - (s32)cur_exp_value) > sndet_ctx.switch_to_day_exp_val * 0.3) {
            sndet_ctx.switch_to_day_exp_val = sndet_ctx.attr.night_to_day_exp_val;
        }
    }
}

static void __check_reflex_env(s32 now)
{
    if (sndet_ctx.last_switch_to_day_time == 0) {
        return;
    }

    u16 sensor_det_measure_time_s         = ipc_factory(sensor_det_measure_time_s);
    u16 disable_sensor_det_protect_lock_s = ipc_factory(disable_sensor_det_protect_lock_s);

    if (now - sndet_ctx.last_switch_to_day_time < (5 * sensor_det_measure_time_s)) {
        sndet_ctx.switch_to_day_exp_val               = sndet_ctx.befor_switch_to_day_exp_val * 0.8;
        sndet_ctx.switch_to_day_total_rb_g_color_diff = sndet_ctx.befor_switch_to_day_total_rb_g_color_diff * 0.8;

        ipcwarn("in reflex env change [%d:%d]\n", sndet_ctx.switch_to_day_exp_val, sndet_ctx.switch_to_day_total_rb_g_color_diff);

        // Must be a strange exception, change the number to normal
        if (sndet_ctx.switch_to_day_exp_val < sndet_ctx.min_switch_to_day_exp_val) {
            sndet_ctx.switch_to_day_exp_val = sndet_ctx.attr.night_to_day_exp_val;
        }

        sndet_ctx.switch_to_day_short_time_count++;

        ADD_IGNORE_TIME_S(disable_sensor_det_protect_lock_s);

        ipcwarn("lock in night mode for %d s\n", sndet_ctx.ignore_time_s - now);
    } else {
        sndet_ctx.switch_to_day_short_time_count = 0;
    }
}

static s32 __auto_mode_process(u32 cur_exp_value, u32 rb_g_color_diff)
{

    s32 now = ipc_mono_ts();

    s32 delay_count = sndet_ctx.ignore_time_s - now;

    if (delay_count > 0) {
        ipctrace("delay[%d:%d]\n", delay_count, sndet_ctx.ignore_time_s);
        return -1;
    } else {
        sndet_ctx.ignore_time_s = 0;
    }

    switch (sndet_ctx.cur_mode) {
        case IPC_SL_DET_DAY_MODE:
            if (cur_exp_value > sndet_ctx.attr.day_to_night_exp_val) {
                sndet_ctx.cur_mode = IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON;
                ipcinfo("IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON\n");
                __check_reflex_env(now);
            }
            break;
        case IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON:
            __check_night_env_changed(cur_exp_value);
            if (cur_exp_value < sndet_ctx.switch_to_day_exp_val
                && (rb_g_color_diff <= sndet_ctx.switch_to_day_total_rb_g_color_diff)) {
                sndet_ctx.cur_mode = IPC_SL_DET_DAY_MODE;
                ipcinfo("IPC_SL_DET_DAY_MODE\n");

                sndet_ctx.befor_switch_to_day_exp_val               = cur_exp_value;
                sndet_ctx.befor_switch_to_day_total_rb_g_color_diff = rb_g_color_diff;

                sndet_ctx.last_switch_to_day_time = now;
            }

            break;

        default:
            if (cur_exp_value > sndet_ctx.attr.day_to_night_exp_val) {
                sndet_ctx.cur_mode = IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON;
            } else {
                sndet_ctx.cur_mode = IPC_SL_DET_DAY_MODE;
            }
    }

    return sndet_ctx.cur_mode;
}

s32 ipc_sensor_light_det_init(struct ipc_sensor_light_det_attr* attr)
{
    sndet_ctx.attr = *attr;
    sndet_ctx.switch_to_day_total_rb_g_color_diff
        = sndet_ctx.attr.night_to_day_wb_bgain * sndet_ctx.attr.night_to_day_wb_rgain;
    sndet_ctx.switch_to_day_exp_val     = sndet_ctx.attr.night_to_day_exp_val;
    sndet_ctx.min_switch_to_day_exp_val = 1000;

    sndet_ctx.attr.night_to_day_wb_bgain
        = sndet_ctx.attr.night_to_day_wb_bgain == 0 ? 1 : sndet_ctx.attr.night_to_day_wb_bgain;
    sndet_ctx.attr.night_to_day_wb_rgain
        = sndet_ctx.attr.night_to_day_wb_rgain == 0 ? 1 : sndet_ctx.attr.night_to_day_wb_rgain;

    clog_init("sendet", " sensor_light_det");

    return 0;
}

s32 ipc_sensor_light_det_process(s32 sensor_det_mode, struct ipc_plat_isp_exp_status* status)
{
    u16 sensor_det_measure_time_s = ipc_factory(sensor_det_measure_time_s);

    if (sndet_ctx.cur_mode == IPC_SL_DET_DAY_MODE) {
        sensor_det_measure_time_s = 1;
    }

    ipctrace("time %u ev:%u wb_statis_rdiff:%u wb_statis_bdiff:%u \n", ipc_mono_ts(), status->ev, status->wb_statis_r_g_diff,
           status->wb_statis_b_g_diff);

    ipctrace("dnexp_val:%u ndexp_val:%u nd_bdiff:%u nd_rdiff:%u \n ", sndet_ctx.attr.day_to_night_exp_val, sndet_ctx.attr.night_to_day_exp_val,
           sndet_ctx.attr.night_to_day_wb_bgain, sndet_ctx.attr.night_to_day_wb_rgain);

    ipctrace("rb_g_color_diff:[%d:%d] switch_to_day_exp_val:%d\n", sndet_ctx.switch_to_day_total_rb_g_color_diff,
           status->wb_statis_r_g_diff * status->wb_statis_b_g_diff, sndet_ctx.switch_to_day_exp_val);

    switch (sensor_det_mode) {
        case IPC_SL_DET_DAY_MODE:
        case IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON:
            // Lock mode has been handed over to the outside, and here we just clear the statistics and exit.
            // cur_mode = sensor_det_mode;
            ADD_IGNORE_TIME_S(5); // manually switch to automatic mode, with a 5-second delay to avoid abnormal switching.
            return 0;
            break;
        default: {
            static u32 exp_value_sum            = 0;
            static u32 total_color_diff_sum     = 0;
            static s32 index                    = 0;
            static u32 max_exp_value            = 0;
            static u32 min_exp_value            = (u32)-1;
            static u32 max_total_color_diff_sum = 0;
            static u32 min_total_color_diff_sum = (u32)-1;
            static s32 error_count              = 0;

            u32 exp_value_average            = 0;
            u32 total_color_diff_average     = 0;
            s32 exp_max_min_value_diff       = 0;
            s32 color_max_min_value_diff_sum = 0;
            u32 total_color_diff             = status->wb_statis_r_g_diff * status->wb_statis_b_g_diff;

            min_exp_value = status->ev < min_exp_value ? status->ev : min_exp_value;
            max_exp_value = status->ev > max_exp_value ? status->ev : max_exp_value;

            min_total_color_diff_sum = total_color_diff < min_total_color_diff_sum ? total_color_diff : min_total_color_diff_sum;
            max_total_color_diff_sum = total_color_diff > max_total_color_diff_sum ? total_color_diff : max_total_color_diff_sum;

            exp_value_sum += status->ev;
            total_color_diff_sum += total_color_diff;
            index++;

            if (index != (2 * sensor_det_measure_time_s)) {
                return 0;
            }

            exp_value_average        = exp_value_sum / index;
            total_color_diff_average = total_color_diff_sum / index;

            total_color_diff_sum         = 0;
            exp_value_sum                = 0;
            index                        = 0;
            exp_max_min_value_diff       = max_exp_value - min_exp_value;
            color_max_min_value_diff_sum = max_total_color_diff_sum - min_total_color_diff_sum;

            max_exp_value            = 0;
            min_exp_value            = (u32)-1;
            max_total_color_diff_sum = 0;
            min_total_color_diff_sum = (u32)-1;

            ipctrace("exp_max_min_value_diff:%d:%f\n", exp_max_min_value_diff, exp_value_average * 0.01);
            ipctrace("color_max_min_value_diff_sum:%d:%f\n", color_max_min_value_diff_sum, total_color_diff_average * 0.2);

            // If the difference between the maximum and minimum values is greater than 0.01% of the average value, discard this value.
            // If it is less than 1000, it is likely that the customer is testing.
            if (exp_value_average > sndet_ctx.min_switch_to_day_exp_val) {
                if ((exp_max_min_value_diff > exp_value_average * 0.01)
                    || ((color_max_min_value_diff_sum > total_color_diff_average * 0.2)
                        && (sndet_ctx.cur_mode == IPC_SL_DET_NIGHT_MODE_WITH_INFRARED_ON))) {

                    error_count++;
                    if (error_count < sensor_det_measure_time_s) {
                        return 0;
                    }
                }
            } else if (exp_value_average < sndet_ctx.min_switch_to_day_exp_val / 3) {
                sndet_ctx.ignore_time_s = 0;
            }

            error_count = 0;

            if (__auto_mode_process(exp_value_average, total_color_diff_average) < 0) {
                // Still waiting for the delay to pass
                return 0;
            }
            break;
        }
    }

    if (sndet_ctx.cur_mode != sndet_ctx.last_mode) {
        sndet_ctx.last_mode = sndet_ctx.cur_mode;

        s32 now         = ipc_mono_ts();
        s32 delay_count = sndet_ctx.ignore_time_s - now;
        if (delay_count < 5) {
            sndet_ctx.ignore_time_s = now + 5;
        }
    }

    sndet_ctx.attr.cmd_handler(sndet_ctx.cur_mode, sndet_ctx.attr.user);

    return 0;
}
