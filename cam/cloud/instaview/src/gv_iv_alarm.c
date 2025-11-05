#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "iv_types.h"
#include "iv_callback_type.h"
#include "iv_interface.h"
#include "iv_error_code.h"
#include "iv_log.h"
#include "ipc_std.h"
#include "ipc_iv_queue.h"
#include "ipc_handler.h"
#include "ipc_middleware.h"
#include "device.h"
#include <fcntl.h>
#include <ipc_core.h>
#include "ipc_iv_alarm.h"

#define SESSION "instaview"

static ipc_alarm_rect_t _g_alarm_rect_info[MAX_NUM_MD_AREA];
static s32 _g_alarm_ignore_count = 0;
static u8 _g_md_signal = 0;  // signal to indicate motion detection
#if !defined(__CHIP_AKV300__) || !defined(__CHIP_AKV130__)
static u8 _g_ai_signal = 0;  // signal to indicate AI detection
#endif
static s32 g_sensitivity_level = 1;
/************************** alarm settings *************************/
static s32 _g_md_sw = 0;

#if defined(__SIERN_LIGHT__)
extern u8 _g_security_light_status;
static s32 _gh_light_siren_timer_id = -1;
static void light_siren_off()
{
    ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_B, IPC_IO_VALUE_IS_INACTIVE);
}
static void light_siren_on()
{
    ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_B, IPC_IO_VALUE_IS_ACTIVE);
}
static s32 light_siren_timer_cb(vptr usr_arg, pu8 tmp_mem, s32 tmp_mem_size)
{
    light_siren_off();
    _gh_light_siren_timer_id = -1;
    return -1;
}
#endif

#if defined(__CHIP_AKV300__) || defined(__CHIP_AKV130__)
#define ONE_METER_MOVE_SENSITIVITY 10000
#define TWO_METER_MOVE_SENSITIVITY 2000
#elif defined(__CHIP_RTS3903__)
#define ONE_METER_MOVE_SENSITIVITY 15000
#define TWO_METER_MOVE_SENSITIVITY 5000
#else
#define ONE_METER_MOVE_SENSITIVITY 6000
#define TWO_METER_MOVE_SENSITIVITY 1024
#endif

static void _set_motion_status(int max_area)
{

    static int alarm_count = 0;
    static u64 last_alarm_time = 0;
    u64 now = ipc_mono_tms();
    if (now - last_alarm_time <= 1000) {
        if (g_sensitivity_level == 0 && max_area > ONE_METER_MOVE_SENSITIVITY) { 

            alarm_count++;

        } else if (g_sensitivity_level == 1 && max_area > TWO_METER_MOVE_SENSITIVITY) {

            alarm_count++;

        } else if (g_sensitivity_level == 2 && max_area > 100) { 

            alarm_count++;

        }
    } else {
        alarm_count = 0;
    }

    last_alarm_time = now;
    if (alarm_count >= _g_alarm_ignore_count) {
        alarm_count = 0;
#ifndef __BLACK_LIGHT__
#if defined(__CHIP_AKV300__) || defined(__CHIP_RTS3903__) || defined(__CHIP_AKV130__)
        ipc_light_trigger(40);
#elif defined(__SIERN_LIGHT__)
        ipc_light_trigger(15); // 15s required for E2 models
#else
        ipc_light_trigger(3 * 60);//3 minutes required for instaview models
#endif
#endif
        _g_md_signal = 1;
    }
}
static u8 _is_rect_intersect(ipc_alarm_rect_p rect1, ipc_alarm_rect_p rect2)
{
    s32 w1 = rect1->rdx - rect1->lux;
    s32 h1 = rect1->rdy - rect1->luy;
    s32 w2 = rect2->rdx - rect2->lux;
    s32 h2 = rect2->rdy - rect2->luy;
    s32 w = abs((rect1->lux + rect1->rdx) / 2 - (rect2->lux + rect2->rdx) / 2);
    s32 h = abs((rect1->luy + rect1->rdy) / 2 - (rect2->luy + rect2->rdy) / 2);
 
    if( w < (w1 + w2) / 2 && h < (h1 + h2) / 2) 
        return 1;

    return 0;
}

static void _alarm_detect(ipc_alarm_rect_p rect_info, s32 rect_num, ipc_alarm_result_extinfo_p extinfo)
{
    s32 max_area = 0;

    static u8 _first                                 = 0;
    static struct ipc_plat_video_capability video_cap = { 0 };

    if (!_first) {
        _first = 1;

        ipc_plat_api(0)->video_query_capability(&video_cap);

        printf("main width: %d, main height: %d\n", video_cap.res[IPC_VIDEO_CHN_MAIN].width,
               video_cap.res[IPC_VIDEO_CHN_MAIN].height);
    }

    if (!_g_md_sw) {
        return;
    }
    // iterate through alarm rectangle information
    for (s32 rect_idx = 0; rect_idx < rect_num; rect_idx++) {
        // if alarm type is motion detection and motion detection switch is on
        if ((rect_info[rect_idx].alarm_type & IPC_ALARM_MD) && _g_md_sw) {
            // check if within required area
            s32 is_in_intersect_rgn = 0;
            ipc_alarm_rect_t dest;
            for (s32 idx = 0; idx < MAX_NUM_MD_AREA; idx++) {
                dest.lux = _g_alarm_rect_info[idx].lux * extinfo->width / video_cap.res[IPC_VIDEO_CHN_MAIN].width;
                dest.luy = _g_alarm_rect_info[idx].luy * extinfo->height / video_cap.res[IPC_VIDEO_CHN_MAIN].height;
                dest.rdx = _g_alarm_rect_info[idx].rdx * extinfo->width / video_cap.res[IPC_VIDEO_CHN_MAIN].width;
                dest.rdy = _g_alarm_rect_info[idx].rdy * extinfo->height / video_cap.res[IPC_VIDEO_CHN_MAIN].height;
                for (s32 alarm_rect_idx = 0; alarm_rect_idx < rect_num; alarm_rect_idx++) {
                    // check if rectangle overlaps with specified area
                    if (_is_rect_intersect(&rect_info[alarm_rect_idx], &dest)) {
                        is_in_intersect_rgn = 1;
                    }
                }
                if (is_in_intersect_rgn) {
                    s32 area = (rect_info[rect_idx].rdy - rect_info[rect_idx].luy)
                               * (rect_info[rect_idx].rdx - rect_info[rect_idx].lux);
                    if (area > max_area) {
                        max_area = area;
                    }
                    _set_motion_status(max_area);
#if !defined(__CHIP_AKV300__) || !defined(__CHIP_AKV130__)
                    if (rect_info[rect_idx].alarm_type & IPC_ALARM_AI)
                    {
                        _g_ai_signal = 1;
                    }
#endif
                }
            }
        }
    }
}


void set_alarm_sensitivity(int sensitivity)
{
    f32 ai_percent = 50;

    switch (sensitivity) {
        case 0:
            _g_alarm_ignore_count = 9;
            ai_percent = 85;
            g_sensitivity_level = 0;
            break;
        case 1:
            _g_alarm_ignore_count = 6;
            ai_percent = 40;
            g_sensitivity_level = 1;
            break;
        case 2:
            _g_alarm_ignore_count = 3;
            ai_percent = 30;
            g_sensitivity_level = 2;
            break;
        default:
            _g_alarm_ignore_count = 9;
            ai_percent = 85;
            g_sensitivity_level = 0;
            break;
    }
    // interface definition: larger numbers mean more sensitive
    ipc_alarm_set_sensitivity(IPC_ALARM_MD, 100 - ai_percent);
}

void save_md_area_to_alarm_rect(IV_MDConfig_t MDconfig) {
    for (int i = 0; i < MAX_NUM_MD_AREA; i++) {
        _g_alarm_rect_info[i].lux = MDconfig.md_area[i].topLeftX;
        _g_alarm_rect_info[i].luy = MDconfig.md_area[i].topLeftY;
        _g_alarm_rect_info[i].rdx = MDconfig.md_area[i].topLeftX + MDconfig.md_area[i].width;
        _g_alarm_rect_info[i].rdy = MDconfig.md_area[i].topLeftY + MDconfig.md_area[i].heigh;
        _g_alarm_rect_info[i].alarm_type = IPC_ALARM_MD; // assuming this is motion detection alarm type
    }
}

int MFG_SetAIConfig_Callback(MfgAIConfigInfo* info)
{
    ipc_alarm_ai_sw(info->enable ? 1 : 0);
    return 0;
}

int MFG_InitMD(IV_MDConfig_t MDconfig)
{
    static s32 sensitivity_level = 1;
    //ipc_alarm_ai_sw(1);
    ipc_alarm_init(_alarm_detect);
    save_md_area_to_alarm_rect(MDconfig);
    switch (MDconfig.threshold)
    {
    case E_MD_SEN_HIGH:
        sensitivity_level = 2;
        printf("set senstivi:hight\r\n");
        break;
    case E_MD_SEN_MIDDLE:
        sensitivity_level = 1;
        printf("set senstivi:middle\r\n");
        break;
    case E_MD_SEN_LOW:
        sensitivity_level = 0;
        printf("set senstivi:low\r\n");
    default:
        sensitivity_level = 1;
        printf("set senstivi:middle\r\n");
        break;
    }
    set_alarm_sensitivity(sensitivity_level);
    _g_md_sw = MDconfig.enable ? (printf("===MD init enable===\r\n"), 1) : (printf("===MD init disable===\r\n"), 0);
    return 0;
}
int MFG_EnableMD(int enableMD)
{
    if (enableMD == 1) {
        printf("===MD enable===\r\n");
        _g_md_sw = 1;
    } else if (enableMD == 0) {
        printf("===MD disable===\r\n");
        _g_md_sw = 0;
    }
    return 0;
}

#if !defined(__CHIP_AKV300__) || !defined(__CHIP_AKV130__)
E_AI_EVENT_TYPE MFG_GetAIResult_Callback(void)
{
    if (_g_ai_signal == 1)
    {
        _g_ai_signal = 0; // reset signal
        return E_AI_HUMAN;
    }

    return E_AI_NOTFOUND;
}
#endif
int MFG_GetMDResult()
{
    if (!_g_md_sw)
    {
        return 0;
    }
    if(ipc_ptz_is_stop(IPC_PTZ_H) == 0 || ipc_ptz_is_stop(IPC_PTZ_V) == 0){
      ipc_light_trigger(40);
      _g_md_signal = 0;
      return 1;
    }
    if (_g_md_signal == 1) {
#if defined(__SIERN_LIGHT__)
        static u32 now_ts = 0;
        if (_g_security_light_status)
        {
            if (ipc_mono_ts() >= now_ts + 35)
            {
                light_siren_on();
                ipc_timer_stop(ipc_global_timer_pool(), _gh_light_siren_timer_id);
                _gh_light_siren_timer_id = ipc_timer_start(ipc_global_timer_pool(), 15 * 1000, light_siren_timer_cb, NULL);
                now_ts = ipc_mono_ts();
            }

    }
#endif
        _g_md_signal = 0; // reset signal
        return 1;
    }

    return 0;
}

void MFG_DeinitMD()
{
    ipc_alarm_uninit(1);
    sleep(1);
}

int MFG_SetMDSensitivity(int Sensitivity)
{

    set_alarm_sensitivity(Sensitivity);
    return 0;
}

int MFG_SetMDSensitivityEx(int SensitivityEx)
{
    set_alarm_sensitivity(SensitivityEx);
    return 0;

}

int MFG_SetMDArea(IV_Rect_t* MDArea)
{
    for(int i = 0; i < MAX_NUM_MD_AREA; i++) {
        _g_alarm_rect_info[i].lux = MDArea[i].topLeftX;
        _g_alarm_rect_info[i].luy = MDArea[i].topLeftY;
        _g_alarm_rect_info[i].rdx = MDArea[i].topLeftX + MDArea[i].width;
        _g_alarm_rect_info[i].rdy = MDArea[i].topLeftY + MDArea[i].heigh;
        _g_alarm_rect_info[i].alarm_type = IPC_ALARM_MD; // assuming this is motion detection alarm type
    }
    return 0;
}