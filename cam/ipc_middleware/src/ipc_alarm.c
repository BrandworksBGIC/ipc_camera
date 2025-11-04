#include "ipc_middleware_config.h"
#include "ipc_log.h"
#include "ipc_platform_api.h"
#include "ipc_alarm.h"
#include "ipc_motion_detect.h"
#include "ipc_factory.h"
#include "ipc_ptz.h"
#include "ipc_env_monitor.h"
#include "ipc_internel.h"
#include <stdio.h>

/**************************** filter *****************************/

#define PI 3.14159265
#define FOCAL(w, h, lens_max_angle) (f32)(sqrt((pow(w / 2, 2) + pow(h / 2, 2))) / tan(lens_max_angle * PI / 180 / 2))
#define ANGLE(xy, focal) (f32)(atan(xy / focal) * 180 / PI)

/**
 * Constraints:
 *     1. Within 1/4, consider as center point; filter out movements that trigger at the center.
 *     2. Outside 1/4, two points are required to determine the direction of motion; filter out movements towards the center, but allow those away
 * from it.
 *     3. The points collected for determining the direction of motion in 2 must be on the same side; they cannot be one on the left and one on the
 * right.
 */
static s32 _track_filter(s32 wh, s32 xy, ps32 last_xy)
{
#define SIGN_BIT (1 << (sizeof(xy) * 8 - 1))

    s32 ret = IPC_NOT_READY;
    if (abs(xy) < wh / 6) {
        *last_xy = 0; /* Passed through the center, clear the last record */
        return ret;
    }

    /* Outer 3/4 */
    if (*last_xy && (*last_xy & SIGN_BIT) == (xy & SIGN_BIT)
        && abs(xy) >= abs(*last_xy)) { /* Previous coordinate exists and on the same side and moving away from the center */
        *last_xy
            = 0; /* After turning, the coordinate is definitely in the center; according to the definition, clear the record when in the center */
        ret = IPC_SUCCESS;
    } else {
        *last_xy = xy;
    }

    return ret;
}

static void _lock_alarm_fps(u32 fps)
{
    // Stabilize frame rate
    static u64 last_tms = 0;
    s64 diff_tms        = 0;
    u64 now_tms         = ipc_mono_tms();
    u32 interval_ms     = (u32)(1000.f / fps);
    diff_tms            = now_tms - last_tms;
    diff_tms            = interval_ms - diff_tms;

    // This is to avoid too short intervals causing frame rate locking to fail
    diff_tms = diff_tms <= 0 ? 1 : diff_tms;

    if (diff_tms < interval_ms) {
        ipc_msleep(diff_tms);
    }

    last_tms = ipc_mono_tms();
}

static f32 _get_cur_lens_max_angle(void)
{
    f32 lens_max_angle = ipc_factory(lens_max_angle);

    struct ipc_plat_video_isp_crop isp_crop = { 0 };
    if (ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_GET_ISP_CROP, &isp_crop) == 0) {
        lens_max_angle *= sqrt((pow(isp_crop.cur_width / 2, 2) + pow(isp_crop.cur_height / 2, 2)))
                          / sqrt((pow(isp_crop.max_width / 2, 2) + pow(isp_crop.max_height / 2, 2)));
        ipctrace("New lens_max_angle %f\n", lens_max_angle);
    }

    return lens_max_angle;
}

static f32 _get_track_focal(u32 width, u32 height)
{
    f32 focal = FOCAL(width, height, _get_cur_lens_max_angle());
    return focal;
}

/******************************* detect ***********************************/

typedef struct {
    struct {
        u8 need_turn;
        u8 speed;
        f32 angle;
    } x, y;
    struct {
        u16 image_width;
        u16 image_height;
        u16 lux; ///< Top-left corner X coordinate
        u16 luy; ///< Top-left corner Y coordinate
        u16 rdx; ///< Bottom-right corner X coordinate
        u16 rdy; ///< Bottom-right corner Y coordinate
    } auto_zoom;
} track_t, *track_p;

static struct {
    u8 init;                     ///< Resource initialization flag
    u8 gorun;                    ///< Module running status control
    u8 alive;                    ///< Module running status feedback
    u8 has_ai;                   ///< Whether AI functionality is present
    u8 ai_sw;                    ///< AI mode switch (human-shaped)
    u8 ai_vehicle_sw;            ///< Vehicle detection switch
    u8 track_sw;                 ///< Motion tracking switch
    s8 pir_sensitivity;          ///< PIR alarm sensitivity
    u8 big_change_filter;        ///< Large change filter
    f32 big_change_filter_ratio; ///< Large change area ratio
    u16 track_reset_ts;          ///< Motion tracking reset time
    f32 track_reset_h_angle;     ///< Motion tracking reset horizontal absolute angle
    f32 track_reset_v_angle;     ///< Motion tracking reset vertical absolute angle
    struct ipc_plat_video_isp_crop track_reset_isp_crop;
    ipc_alarm_detect_f f_detect;
} _gh_alarm[1] = { {
    .f_detect                = NOT_DO_ANYTHING,
    .pir_sensitivity         = -1,
    .big_change_filter       = 1,
    .big_change_filter_ratio = 0.6,
} };

/**************************** object detect *****************************/

static struct {
    s32 width;             ///< Image width
    s32 height;            ///< Image height
    s32 last_x;            ///< Last alarm X coordinate
    s32 last_y;            ///< Last alarm Y coordinate
    f32 focal;             ///< Relative pixel distance of focus
    s32 swdg_fd;           ///< Software watchdog
    u8 filter_alarm_frame; ///< Alarm frame filter count
    u8 filter_track_frame; ///< Tracking frame filter count
} _gh_obj[1];

#define YUV_CHN IPC_VIDEO_CHN_YUV
static s32 _object_detect_init(void)
{
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    struct ipc_plat_video_capability cap;
    h_plat->video_query_capability(&cap);

    ipcinfo("YUV width=[%d], height=[%d]", cap.res[YUV_CHN].width, cap.res[YUV_CHN].height);

    ipc_motion_detect_attr_t detect_attr = {
        .width       = cap.res[YUV_CHN].width,
        .height      = cap.res[YUV_CHN].height,
        .sensitivity = 0.25,
        .sad_type    = IPC_MOTION_SAD_16x16,
    };

    s32 ret = ipc_motion_detect_init(&detect_attr);
    if (ret < 0) {
        ipcfatal("Motion detect init failed! retcode=%d", ret);
        return ret;
    }

    ret = h_plat->video_start(YUV_CHN, 0);
    if (ret != IPC_SUCCESS) {
        ipcfatal("Yuv channel start failed!");
        return IPC_FAILED;
    }

    _gh_obj->width   = cap.res[YUV_CHN].width;
    _gh_obj->height  = cap.res[YUV_CHN].height;
    _gh_obj->last_x  = 0;
    _gh_obj->swdg_fd = ipc_swdg_reg(1);
    ipcinfo("Object detect init success! watchdog fd=[%d]", _gh_obj->swdg_fd);

    return IPC_SUCCESS;
}

static void _object_detect_start(void)
{
    ipcinfo("Start object detect");
    ipc_swdg_feed(_gh_obj->swdg_fd, 10); // Feed watchdog
}

static void _object_detect_stop(void)
{
    ipcinfo("Stop object detect");
    ipc_swdg_feed(_gh_obj->swdg_fd, 0); // Pause software watchdog
}

static void _object_detect_uninit(void)
{
    ipc_swdg_unreg(_gh_obj->swdg_fd);
    ipc_plat_api(0)->video_stop(YUV_CHN);
    ipc_motion_detect_uninit();
    ipcinfo("Object detect uninit!");
}

static s32 _object_detect_image_is_changing(void)
{
    u64 changed_time = 0;
    u64 now          = ipc_mono_tms();

    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_CHECK_IMAGE_IS_IN_CHANGING, &changed_time);

    return (now - changed_time) < 3000;
}

static void __plat_recv_yuv_frame_cb(struct ipc_frame_data_s* frame, vptr _user)
{
    ipc_alarm_result_p result = (ipc_alarm_result_p)_user;

    ipc_motion_detect_process(frame->pack[0].data, result);
}

static s32 _object_detect(track_p track)
{
    if (ipc_env_block_alarm() || !ipc_ptz_is_stop(IPC_PTZ_H) || !ipc_ptz_is_stop(IPC_PTZ_V) || _object_detect_image_is_changing()) {
        _gh_obj->filter_track_frame = 1;
        _gh_obj->filter_alarm_frame = 40; // Filter alarms for 4 seconds
        ipc_swdg_feed(_gh_obj->swdg_fd, 10);
        return IPC_FAILED; /* Return success because we need to trigger periodic reset */
    }

    s32 ret = 0;
    ipc_alarm_result_t result;

    ret = ipc_plat_api(0)->video_recv_frame(YUV_CHN, __plat_recv_yuv_frame_cb, &result, 1000);
    if (ret)
        return IPC_NOT_READY;

    ipc_swdg_feed(_gh_obj->swdg_fd, 10);

    if (_gh_obj->filter_track_frame) { /* Filter tracking */
        _gh_obj->filter_track_frame--;
        return IPC_NOT_READY;
    }

    if (_gh_obj->filter_alarm_frame) { /* Filter alarms */
        _gh_obj->filter_alarm_frame--;
    }

    if (!result.rect_num)
        return IPC_NOT_READY; // No alarms

    ipc_alarm_rect_t alarm_rect[result.rect_num];

    s32 area    = 0;
    s32 max     = 0;
    s32 max_idx = -1;
    for (s32 idx = 0; idx < result.rect_num; idx++) {
        alarm_rect[idx].alarm_type = IPC_ALARM_MD;
        alarm_rect[idx].lux        = result.rect[idx].lux;
        alarm_rect[idx].luy        = result.rect[idx].luy;
        alarm_rect[idx].rdx        = result.rect[idx].rdx;
        alarm_rect[idx].rdy        = result.rect[idx].rdy;
        area                       = (result.rect[idx].rdy - result.rect[idx].luy) * (result.rect[idx].rdx - result.rect[idx].lux);
        if (max < area) {
            max     = area;
            max_idx = idx;
        }

        if (_gh_alarm->big_change_filter && ((area / (float)(_gh_obj->width * _gh_obj->height)) > _gh_alarm->big_change_filter_ratio)) {
            max_idx = -1;
            break;
        }
    }
    if (max_idx < 0)
        return IPC_FAILED; // Data seems incorrect

    if (_gh_obj->filter_alarm_frame <= 0) { /* Filter alarms */
        ipc_alarm_result_extinfo_t extinfo;
        extinfo.width               = _gh_obj->width;
        extinfo.height              = _gh_obj->height;
        extinfo.alarm_image_percent = result.alarm_image_percent;

        _gh_alarm->f_detect(alarm_rect, result.rect_num, &extinfo);
    }

    s32 center_x = (result.rect[max_idx].rdx - result.rect[max_idx].lux) / 2 + result.rect[max_idx].lux - _gh_obj->width / 2;
    s32 center_y = (result.rect[max_idx].rdy - result.rect[max_idx].luy) / 2 + result.rect[max_idx].luy - _gh_obj->height / 2;
    ipctrace("Track object center x=[%d], y=[%d], filter[%hhu]", center_x, center_y, _gh_obj->filter_alarm_frame);

    track->auto_zoom.image_width  = _gh_obj->width;
    track->auto_zoom.image_height = _gh_obj->height;

    track->auto_zoom.lux = result.rect[max_idx].lux;
    track->auto_zoom.luy = result.rect[max_idx].luy;
    track->auto_zoom.rdx = result.rect[max_idx].rdx;
    track->auto_zoom.rdy = result.rect[max_idx].rdy;

    _gh_obj->focal = _get_track_focal(_gh_obj->width, _gh_obj->height);

    ret = _track_filter(_gh_obj->width, center_x, &_gh_obj->last_x);
    if (ret == IPC_SUCCESS) {
        track->x.need_turn = 1;
        track->x.angle     = ANGLE(center_x, _gh_obj->focal);
        track->x.speed     = ipc_factory(ptz_h_track_speed);
    }

    ret = _track_filter(_gh_obj->height, center_y, &_gh_obj->last_y);
    if (ret == IPC_SUCCESS) {
        track->y.need_turn = 1;
        track->y.angle     = ANGLE(center_y, _gh_obj->focal);
        track->y.speed     = ipc_factory(ptz_v_track_speed);
    }

    return IPC_SUCCESS;
}
/**************************** human detect *****************************/

static s32 _human_detect_init(void)
{
    IPC_PLAT_ALARM_TYPE support_alarm_type = 0;
    if (ipc_plat_api(0)->alarm_init) {
        s32 ret = ipc_plat_api(0)->alarm_init(&support_alarm_type);
        if (ret < 0) {
            ipcfatal("Plat alarm init failed! retcode=[%d]", ret);
            return IPC_FAILED;
        }
        if (support_alarm_type & IPC_PLAT_ALARM_TYPE_AI_PEOPLE
         || support_alarm_type & IPC_PLAT_ALARM_TYPE_AI_VEHICLE) {
            ipcinfo("Human detect init success!");
            _gh_alarm->has_ai = 1;
        }
        else {
            ipcinfo("No human detect fucntion!");
            ipc_plat_api(0)->alarm_uninit();
        }
    }
    else {
        ipcinfo("No human detect fucntion!");
    }
    
    return IPC_SUCCESS;
}

static void _human_detect_start(void)
{
    if (!_gh_alarm->has_ai) return ;
    ipcinfo("Start human detect");
    ipc_plat_api(0)->alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD_START, NULL);
}

static void _human_detect_stop(void)
{
    if (!_gh_alarm->has_ai) return ;
    ipcinfo("Stop human detect");
    ipc_plat_api(0)->alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD_STOP, NULL);
}

static void _human_detect_uninit(void)
{
    if (!_gh_alarm->has_ai) return ;
    ipc_plat_api(0)->alarm_uninit();
    ipcinfo("Human detect uninit!");
}

static s32 _human_detect(track_p track)
{
    if (!_gh_alarm->has_ai) return IPC_NOT_SUPPORT;

    if (ipc_env_block_alarm() 
    || !ipc_ptz_is_stop(IPC_PTZ_H) 
    || !ipc_ptz_is_stop(IPC_PTZ_V)) {
        ipctrace("image changing");
        ipc_plat_api(0)->alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD_NOTICE_IMAGE_CHANGING, NULL);
    }

    struct ipc_plat_alarm_result_s result = { 0 };
    s32 ret = ipc_plat_api(0)->alarm_recv_result(&result, 1000);
    if (ret < 0) return IPC_NOT_READY;

    ipc_plat_api(0)->alarm_release_result(&result);

    ipctrace("Track human alarm_result_num=[%d]", result.alarm_result_num);

    if (!result.alarm_result_num) return IPC_NOT_READY;

    ipc_alarm_rect_t alarm_rect[result.alarm_result_num];

    s32 area      = 0;
    s32 max       = 0;
    s32 max_idx   = -1;
    s32 alarm_idx = 0;
    for (s32 idx = 0; idx < result.alarm_result_num; idx++) {
        alarm_rect[alarm_idx].alarm_type
            = (result.rect[idx].alarm_type & IPC_PLAT_ALARM_TYPE_MD) ? IPC_ALARM_MD : 0; // Default motion detection always on
        alarm_rect[alarm_idx].alarm_type |= (_gh_alarm->ai_sw && (result.rect[idx].alarm_type & IPC_PLAT_ALARM_TYPE_AI_PEOPLE)) ? IPC_ALARM_AI : 0;
        alarm_rect[alarm_idx].alarm_type
            |= (_gh_alarm->ai_vehicle_sw && (result.rect[idx].alarm_type & IPC_PLAT_ALARM_TYPE_AI_VEHICLE)) ? IPC_ALARM_AI_VEHICLE : 0;
        if (!alarm_rect[alarm_idx].alarm_type)
            continue; // Alarm but corresponding switch not enabled, proceed only if it's MD or AI alarm with the switch on
        alarm_rect[alarm_idx].lux = result.rect[idx].lux;
        alarm_rect[alarm_idx].luy = result.rect[idx].luy;
        alarm_rect[alarm_idx].rdx = result.rect[idx].rdx;
        alarm_rect[alarm_idx].rdy = result.rect[idx].rdy;
        if (alarm_rect[alarm_idx].alarm_type & (IPC_ALARM_AI | IPC_ALARM_AI_VEHICLE)) { // Calculate area only for AI objects
            area = (alarm_rect[alarm_idx].rdy - alarm_rect[alarm_idx].luy) * (alarm_rect[alarm_idx].rdx - alarm_rect[alarm_idx].lux);
            if (max < area) {
                max     = area;
                max_idx = idx;
            }
        }
        alarm_idx++;
    }
    if (alarm_idx == 0)
        return IPC_NOT_READY; // No valid data

    ipc_alarm_result_extinfo_t extinfo;
    extinfo.alarm_image_percent = -1;
    extinfo.width               = result.image_width;
    extinfo.height              = result.image_height;

    _gh_alarm->f_detect(alarm_rect, alarm_idx, &extinfo);

    if (max_idx < 0)
        return IPC_NOT_READY; // No AI detected

    s32 center_x = (result.rect[max_idx].rdx - result.rect[max_idx].lux) / 2 + result.rect[max_idx].lux - result.image_width / 2;
    s32 center_y = (result.rect[max_idx].rdy - result.rect[max_idx].luy) / 2 + result.rect[max_idx].luy - result.image_height / 2;
    ipctrace("Track human center x=[%d], y=[%d]", center_x, center_y);

    track->auto_zoom.image_width  = result.image_width;
    track->auto_zoom.image_height = result.image_height;

    track->auto_zoom.lux = result.rect[max_idx].lux;
    track->auto_zoom.luy = result.rect[max_idx].luy;
    track->auto_zoom.rdx = result.rect[max_idx].rdx;
    track->auto_zoom.rdy = result.rect[max_idx].rdy;

    f32 focal = _get_track_focal(result.image_width, result.image_height);

    s32 speed = 0;
    s32 level = abs(center_x) / (result.image_width / 8);
    switch (level) {
        case 0:
        case 1:
            speed = 10;
            break;
        case 2:
            speed = 8;
            break;
        default:
            speed = 6;
            break;
    }

    if (level > 0) { /* Do not move in the center quarter */
        track->x.need_turn = 1;
        track->x.angle     = ANGLE(center_x, focal) * 0.5; // Angle should be smaller, gradually moving towards the center region
        track->x.speed     = speed;
    }

    level = abs(center_y) / (result.image_height / 8);
    switch (level) {
        case 0:
        case 1:
            speed = 10;
            break;
        case 2:
            speed = 8;
            break;
        default:
            speed = 6;
            break;
    }

    if (level > 0) {
        track->y.need_turn = 1;
        track->y.angle     = ANGLE(center_y, focal) * 0.5;
        track->y.speed     = speed;
    }

    return IPC_SUCCESS;
}


/****************************** main *********************************/

void ipc_alarm_set_sensitivity(ipc_alarm_type_e type, f32 percent)
{
    if (percent > 100) percent = 100;
    if (percent < 0)   percent = 0;
    if (type & IPC_ALARM_PIR) {
        _gh_alarm->pir_sensitivity = percent;
    } else if (type & IPC_ALARM_MD) {
        ipc_motion_detect_set_sensitivity(2 - percent / 50.f);
    } else if (type & (IPC_ALARM_AI | IPC_ALARM_AI_VEHICLE) ) {
        if (ipc_plat_api(0)->alarm_ctrl) {
            ipc_plat_api(0)->alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD_SET_SENSITIVITY, (vptr)&percent);
        }
    }
}

static void _try_track(track_p track)
{
    f32 angle; 
    pv8 dir_str;
    ipc_ptz_dir_e dir;

    if (track->x.need_turn) {
        if (track->x.angle > 0.1) {
            angle = track->x.angle;
            dir_str = "right";
            dir = IPC_PTZ_RIGHT;
        } else {
            angle = -track->x.angle;
            dir_str = "left";
            dir = IPC_PTZ_LEFT;
        }
        ipctrace("ptz turn %s=[%f]", dir_str, angle);
        ipc_ptz_track(dir, angle, track->x.speed);
    }

    if (track->y.need_turn && ipc_factory(ptz_v_track_enable)) {
        if (track->y.angle > 0.1) {
            angle = track->y.angle;
            dir_str = "down";
            dir = IPC_PTZ_DOWN;
        } else {
            angle = -track->y.angle;
            dir_str = "up";
            dir = IPC_PTZ_UP;
        }
        ipctrace("ptz turn %s=[%f]", dir_str, angle);
        ipc_ptz_track(dir, angle, track->y.speed);
    }
}

static vptr _pth_alarm(vptr arg)
{
    track_t track[1];
    s32 ret = 0;
    u8  last_ai_sw = -1;
    u64 last_alarm_tms  = 0;
    u8 need_turn_center = 0;
    u32 alarm_fps = 50;

    if (ipc_plat_api(0)->alarm_ctrl) {
        ipc_plat_api(0)->alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD_FRAME_RATE_CTRL_NEEDED, (vptr)&alarm_fps);
    }

    _gh_alarm->alive = 1;

    while (_gh_alarm->gorun) {

        memset(track, 0, sizeof(track));
        if (_gh_alarm->has_ai && (_gh_alarm->ai_sw || _gh_alarm->ai_vehicle_sw)) {
            if (last_ai_sw != 1) {
                last_ai_sw = 1;
                _human_detect_start();
                _object_detect_stop();
            }
            ret = _human_detect(track);
        }
        else {
            if (last_ai_sw != 0) { 
                last_ai_sw = 0;
                _human_detect_stop();
                _object_detect_start();
            }
            ret = _object_detect(track);
        }

        if (_gh_alarm->track_sw) {

            if (ret < 0) {

                if (need_turn_center && ipc_mono_tms() > last_alarm_tms + (_gh_alarm->track_reset_ts ? : ipc_factory(ptz_track_reset_ts)) * 1000LL) {
                    need_turn_center = 0;
                    ipc_ptz_turn_abs(IPC_PTZ_H, ipc_ptz_get_init_angle(IPC_PTZ_H));
                    ipc_ptz_turn_abs(IPC_PTZ_V, ipc_ptz_get_init_angle(IPC_PTZ_V));
                }
            }
            else {
                ipctrace("Alarm trigger");
                last_alarm_tms = ipc_mono_tms();

                need_turn_center = 1;

                _try_track(track);

            }
        }

        _lock_alarm_fps(last_ai_sw == 1 ? alarm_fps : 6);
    }

    _object_detect_stop();
    _human_detect_stop();

    _gh_alarm->alive = 0;

    return NULL;
}

s32 ipc_alarm_init(ipc_alarm_detect_f f_detect)
{
    if (_gh_alarm->init) return IPC_EXIST;

    clog_init("alarm", "Alarm detection and tracking");

    if (f_detect) _gh_alarm->f_detect = f_detect;

    s32 ret = _object_detect_init();
    if (ret < 0) {
        ipcfatal("Object detect init failed! retcode=[%d]", ret);
        return ret;
    }

    ret = _human_detect_init();
    if (ret < 0) {
        ipcfatal("Human detect init failed! retcode=[%d]", ret);
        return ret;
    }

    _gh_alarm->gorun = 1;
    ret = ipc_create_thread("ipc_alarm", _pth_alarm, NULL, 64 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create thread failed! retcode=[%d]", ret);
        _human_detect_uninit();
        _object_detect_uninit();
        return ret;
    }

    _gh_alarm->init = 1;
    ipcinfo("Init complete!");

    return IPC_SUCCESS;
}

void ipc_alarm_track_reset_ts(u16 reset_ts)
{
    _gh_alarm->track_reset_ts = reset_ts;
}

void ipc_alarm_uninit(u8 is_wait)
{
    if (!_gh_alarm->init) return ;

    _gh_alarm->gorun = 0;
    if (!is_wait) return ;
    while (_gh_alarm->alive) ipc_msleep(100);
    
    _human_detect_uninit();
    _object_detect_uninit();
    _gh_alarm->init = 0;
    ipcinfo("Exit complete!");
}

void ipc_alarm_ai_vehicle_sw(u8 is_enable)
{
    _gh_alarm->ai_vehicle_sw = is_enable;
}

void ipc_alarm_ai_sw(u8 is_enable)
{
    _gh_alarm->ai_sw = is_enable;
}

void ipc_alarm_track_sw(u8 is_enable)
{
    _gh_alarm->track_sw = is_enable;
}

void ipc_alarm_big_change_filter(u8 en, f32 ratio)
{
    _gh_alarm->big_change_filter = en;
    if (ratio > 0) {
        _gh_alarm->big_change_filter_ratio = ratio;
    }
}
