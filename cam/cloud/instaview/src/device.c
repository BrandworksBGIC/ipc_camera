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
#include "ipc_handler.h"
#include <fcntl.h>
#include <ipc_core.h>
#include "ipc_iv_ble.h"

#define SESSION "instaview"

extern u8 _g_net_conn_status;
static struct {
    s32 flip_status;
    s32 ircut_status;
    s32 alarm_track_sw;
    s32 light_sw;
} _g_device_status;

static struct ipc_bitrate_adjust adjust = {
    .fps            = 20,
    .level          = 1,
    .expect_bitrate = 1024,
    .net_type       = 0,
};

int MFG_GetPrebultInfo(char* device_id, const int device_id_max_size, char* access_key,
                       const int access_key_max_size) // get basic parameters, mainly fps and resolution, others seem unused
{
    ipc_decrypt_exinfo_p cloud_info = ipc_decrypt_exinfo();
    if (cloud_info == NULL) {
        return IPC_VERIFY_FAILED;
    }
    printf("====GetPrebultInfo====\r\n");
    pv8 model     = (pv8)cloud_info->info_ctx;
    pv8 did       = model + strlen(model) + 1;
    pv8 accessKey = did + strlen(did) + 1;
    strcpy(device_id, did);
    strcpy(access_key, accessKey);
    return 0;
}

int MfgVideoParamGet(E_VIDEO_STREAM_INDEX stream_index, HalVideoEncodeParam* params)
{

    struct ipc_plat_video_fps_ctrl fps_ctrl = { .type = IPC_VIDOE_FPS_GET, .fps = 0 };
    struct ipc_plat_video_capability video_cap;
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    h_plat->video_query_capability(&video_cap);

    if (stream_index == E_VIDEO_MAIN_STREAM) {
        h_plat->video_ctrl(E_VIDEO_MAIN_STREAM, IPC_VIDEO_CTRL_CMD_FPS_CTRL, (vptr)&fps_ctrl);
        params->enc_type      = E_VIDEO_H265_A;
        params->br_mode       = E_VIDEO_VBR;
        params->width         = video_cap.res[E_VIDEO_MAIN_STREAM].width;
        params->height        = video_cap.res[E_VIDEO_MAIN_STREAM].height;
        params->fps           = fps_ctrl.fps;
        params->gop           = 3;
        params->kbps          = adjust.expect_bitrate;
        params->quality_level = adjust.level;
    } else if (stream_index == E_VIDEO_SECOND_STREAM) {
        h_plat->video_ctrl(E_VIDEO_SECOND_STREAM, IPC_VIDEO_CTRL_CMD_FPS_CTRL, (vptr)&fps_ctrl);
        params->enc_type      = E_VIDEO_H265_A;
        params->br_mode       = E_VIDEO_VBR;
        params->width         = video_cap.res[E_VIDEO_MAIN_STREAM].width;
        params->height        = video_cap.res[E_VIDEO_MAIN_STREAM].height;
        params->fps           = fps_ctrl.fps;
        params->gop           = 3;
        params->kbps          = adjust.expect_bitrate;
        params->quality_level = adjust.level;
    }
    return 0;
}

// reset handling
void ipc_iv_device_reset_process(void)
{
    ipc_rm("/conf/ipc.json");
    ipc_rm("/conf/iv_key.pem");
    ipc_rm("/conf/iv_cert.pem");
    ipc_rm("/conf/iv.cfg");
    ipc_rm("/conf/instaview.json");
    ipc_mpp_play("reset", 1);
    ipc_exec("reboot");
}

int MfgVideoParamSet(E_VIDEO_STREAM_INDEX stream_index,
                     HalVideoEncodeParam* params) // set from top to bottom, cloud platform sets underlying parameters
{
    struct ipc_api_s* h_plat = ipc_plat_api(0);

    IPC_VIDEO_CHN_TYPE chn = (stream_index == E_VIDEO_MAIN_STREAM) ? IPC_VIDEO_CHN_MAIN : IPC_VIDEO_CHN_SUB;
    adjust.fps             = params->fps;
    adjust.expect_bitrate  = params->kbps;
    adjust.level           = (params->quality_level > 3) ? 2 : params->quality_level;
    h_plat->video_ctrl(chn, IPC_VIDEO_CTRL_CMD_QOS_ADJUST, (vptr)&adjust);

    return 0;
}

int MFG_ChangeResolution_Callback(E_VIDEO_STREAM_INDEX stream_index, int width, int height)
{
    if (stream_index == E_VIDEO_MAIN_STREAM) {

    } else if (stream_index == E_VIDEO_SECOND_STREAM) {

    } else {
        return -1;
    }
    return 0;
}

int MFG_LedInit_callback()
{
    return 0;
}
int MFG_LedSetColor_Callback(E_LED_COLOR_MODE led, int value) // LED control function, provides IO port, SDK controls LED lights
{
    s32 ret = -1;
    if (led == LED_COLOR_BLUE) {
        ret = ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_A, value == 1 ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE);
        if (ret < 0) {
            return ret;
        }
        ipc_handler_write_int("led_state", value);
    }
    return 0;
}

int MFG_VideoStreamInit_callback(int stream_index)
{

    printf("%s:%d\n", __func__, stream_index);

    
    ipc_mpp_video_start(IPC_VIDEO_CHN_MAIN, 0);
    ipc_mpp_video_start(IPC_VIDEO_CHN_SUB, 0);
    return 0;
}

int MFG_VideoStreamDeInit_callback(void)
{
    s32 ret = 0;

    // Stop main video channel
    ret = ipc_mpp_video_stop(IPC_VIDEO_CHN_MAIN);
    if (ret < 0) {
        printf("Failed to stop main video channel: %d\n", ret);
    }

    // Stop sub video channel
    ret = ipc_mpp_video_stop(IPC_VIDEO_CHN_SUB);
    if (ret < 0) {
        printf("Failed to stop sub video channel: %d\n", ret);
    }

    printf("Video channels stopped\n");
    return 0;
}

int MFG_AudioInit(int audio_rate, int bit_width, int channels)
{
    return 0;
}

int MFG_AudioDeInit()
{
    return 0;
}

int MFG_VideoKeyFrameReq(E_VIDEO_STREAM_INDEX stream_index) // force I-frame request
{
    printf("======[chn:%d][force iframe]======\n", stream_index);
    IPC_VIDEO_CHN_TYPE chn = (stream_index == E_VIDEO_MAIN_STREAM) ? IPC_VIDEO_CHN_MAIN : IPC_VIDEO_CHN_SUB;
    ipc_mpp_force_iframe(chn);
    return 0;
}

/*********************wifi QR code scanning connection**********************************/
static s32 _g_scaning_wifi = 0;

// Context structure for raw video frame callback
typedef struct {
    char* raw_data;
    int raw_data_length;
    int* width;
    int* height;
    int frame_available;
    int frame_len;
    struct ipc_plat_video_capability video_cap;
} mfg_raw_video_context_t;

// Static callback function for raw video data
static void mfg_raw_video_frame_callback(struct ipc_frame_data_s* received_frame, void* user_data)
{
    mfg_raw_video_context_t* ctx = (mfg_raw_video_context_t*)user_data;
    if (received_frame == NULL || ctx == NULL || ctx->raw_data == NULL) {
        return;
    }

    // Get frame data from received frame
    s32 frame_len = received_frame->pack[0].data_len;

    // Check if buffer is large enough
    if (frame_len > ctx->raw_data_length) {
        frame_len = ctx->raw_data_length;
        if (frame_len <= 0) {
            return;
        }
    }

    // Copy raw video data to buffer
    if (frame_len > 0 && received_frame->pack[0].data != NULL) {
        memcpy(ctx->raw_data, received_frame->pack[0].data, frame_len);
    }

    // Set width and height from video capability
    if (ctx->width != NULL) {
        *(ctx->width) = ctx->video_cap.res[3].width;
    }

    if (ctx->height != NULL) {
        *(ctx->height) = ctx->video_cap.res[3].height;
    }

    ctx->frame_available = 1;
    ctx->frame_len       = frame_len;
}

int MFG_ReadRawVideoData_callback(E_VIDEO_STREAM_INDEX stream_index, char* raw_data, const int raw_data_lenth, int* width, int* height, int type)
{
    // printf("waiting for qrcode!!!\n");
    static int _g_video_start = -1;
    _g_scaning_wifi           = 1;
    int chn                   = IPC_VIDEO_CHN_YUV; // YUV channel for QR code detection
    type                      = 0;                 // 0: NV12 for QR code, 1: UYVY for AI

    if (raw_data == NULL || raw_data_lenth <= 0) {
        return 0;
    }

    // Get platform API handle
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    if (h_plat == NULL) {
        return 0;
    }

    // Create context for callback
    mfg_raw_video_context_t ctx
        = { .raw_data = raw_data, .raw_data_length = raw_data_lenth, .width = width, .height = height, .frame_available = 0, .frame_len = 0 };

    // Query video capability once
    h_plat->video_query_capability(&ctx.video_cap);
    if (width != NULL) {
        *width = ctx.video_cap.res[chn].width;
    }
    if (height != NULL) {
        *height = ctx.video_cap.res[chn].height;
    }

    // Start YUV channel if not started
    if (_g_video_start == -1) {
        h_plat->video_start(chn, 0);
        _g_video_start = 1;
    }

    // Receive raw video frame using new callback API
    s32 ret = h_plat->video_recv_frame(chn, mfg_raw_video_frame_callback, &ctx, 1000);

    // Return the frame length (set by callback) or error
    if (ret >= 0 && ctx.frame_available && ctx.frame_len > 0) {
        return ctx.frame_len;
    }

    return 0;
}
extern ipc_qrcode_info_t _g_wifi_info;
static s32 wifi_thread_running = 0;
static vptr wifi_link() // wifi connection function
{
    if (_g_scaning_wifi) {
        ipc_wifi_sta_connect(_g_wifi_info.ssid, _g_wifi_info.password, NULL, 60);
    } else {
        ipc_wifi_sta_connect(_g_wifi_info.ssid, _g_wifi_info.password, NULL, 0);
    }
    wifi_thread_running = 0;
    return NULL;
}
int MFG_WifiLink_Callback(const char* ssid, const char* password)
{
    if (!ssid) {
        printf("ssid is null!!\r\n");
        return -1;
    }

    printf("wifi start!!!\r\n");
    memset(_g_wifi_info.ssid, 0, sizeof(_g_wifi_info.ssid));
    memset(_g_wifi_info.password, 0, sizeof(_g_wifi_info.password));

    strncpy(_g_wifi_info.ssid, ssid, sizeof(_g_wifi_info.ssid) - 1);

    if (password) {
        strncpy(_g_wifi_info.password, password, sizeof(_g_wifi_info.password) - 1);
        _g_wifi_info.password[sizeof(_g_wifi_info.password) - 1] = '\0';
    } else {
        _g_wifi_info.password[0] = '\0';
    }
    printf("wifi id:%s\r\n", _g_wifi_info.ssid);
    printf("wifi password:%s\r\n", _g_wifi_info.password);

    if (!wifi_thread_running) {
        wifi_thread_running = 1;
        s32 ret             = ipc_create_thread("wifi_link", wifi_link, NULL, 256 * 1024, 0);
        if (ret < 0) {
            wifi_thread_running = 0;
        }
    }

    return 0;
}

E_WIFI_STATUS MFG_GetWifiLinkStatus() // get wifi status, obtained based on previous callbacks
{
    if (_g_net_conn_status == 1) {
        printf("wifi status:connect success!!!\r\n");
        // ipc_iv_ble_uninit();
        return WIFI_LINKED;
    } else if (_g_net_conn_status == 0) {
        printf("wifi status:connect failed!!!\r\n");
        return WIFI_UNLINKED;
    }
    // else if (_g_net_wating == 1)
    // {
    //     return WIFI_LINKING;
    // }
    return WIFI_NOTCONFIG;
}

#define WLAN_DEV "wlan0"

int MFG_GetWifiRSSI_Callback(char* rssi, const int size)
{
    if (rssi == NULL || size <= 0) {
        return -1;
    }

    char command[128];
    snprintf(command, sizeof(command), "iwconfig %s", WLAN_DEV);

    FILE* pp = popen(command, "r");
    if (pp == NULL) {
        return -1;
    }

    char tmp[256]        = { 0 };
    int found_rssi       = 0;
    char level_value[16] = { 0 };

    while (fgets(tmp, sizeof(tmp), pp) != NULL) {
        if (!found_rssi) {
            char* rssi_ptr = strstr(tmp, "level=");
            if (rssi_ptr != NULL) {
                if (sscanf(rssi_ptr + strlen("level="), "%15s", level_value) == 1) {
                    snprintf(rssi, size, "%s", level_value);
                    found_rssi = 1;
                }
            }
        }
        if (found_rssi) {
            break;
        }
    }
    pclose(pp);

    if (!found_rssi) {
        return -1;
    }

    printf("Get Conn AP RSSI: %s dBm\n", rssi);
    return 0;
}

int MFG_Snapshot_callback(char* PATH) // input image path
{
    printf("[GvDebug]func:%s,line:%d\r\n", __func__, __LINE__);
    printf("snapshot\r\n");
    if (PATH != NULL) {
        s32 ret = ipc_mpp_snapshot((pv8)PATH, 0);
        if (ret > 0) {
            return ret;
        }
    }
    return 0;
}

int MFG_VideoSetMirrorAndflip(int flip, int mirror)
{
    if (flip == 0 || flip == 1) {
        _g_device_status.flip_status = flip;
    } else {
        return -1;
    }
    ipc_mpp_image_flip(_g_device_status.flip_status);
    ipc_ptz_flip(IPC_PTZ_H, _g_device_status.flip_status);
    ipc_ptz_flip(IPC_PTZ_V, _g_device_status.flip_status);
    return 0;
}

int MFG_VideoGetMirrorAndflip(int* flip, int* mirror)
{
    *flip = _g_device_status.flip_status;
    return 0;
}

/*************************night vision mode switching and white light control*************************/
int MFG_InitIRCut_Callback()
{
    return 0;
}

void MFG_DeinitIRCut()
{
}

int MFG_EnableBulb(int light_switch)
{
    _g_device_status.light_sw = light_switch;
    ipc_light_force_open(light_switch);
    ipc_handler_write_int("light_switch", light_switch);
    return 0;
}

int MFG_SetBulbMode(E_BULB_MODE Bulb_mode)
{
    ipc_env_mode_e _env_mode = IPC_ENV_AUTO;
    switch (Bulb_mode) {
        case E_BULB_MODE_INTELLIGENT: {
            _env_mode                     = IPC_ENV_AUTO;
            _g_device_status.ircut_status = E_BULB_MODE_INTELLIGENT;
            break;
        }
        case E_BULB_MODE_IR: {
            _env_mode                     = IPC_ENV_INFRARED;
            _g_device_status.ircut_status = E_BULB_MODE_IR;
            break;
        }

        case E_BULB_MODE_FULLCOLOR: {
            _env_mode                     = IPC_ENV_FULL_COLOR;
            _g_device_status.ircut_status = E_BULB_MODE_FULLCOLOR;
            break;
        }
        case E_BULB_MODE_OFF: {
            _env_mode                     = IPC_ENV_WITH_NO_INFRARED;
            _g_device_status.ircut_status = E_BULB_MODE_MANUAL;
            break;
        }
        default: {
            _env_mode                     = IPC_ENV_AUTO;
            _g_device_status.ircut_status = E_BULB_MODE_INTELLIGENT;
            break;
        }
    }
    ipc_handler_write_int("env_mode", _env_mode);
    ipc_env_set_mode(_env_mode);

    return 0;
}

ipc_env_mode_e ipc_iv_device_get_env_mode(void)
{
    ipc_env_mode_e _env_mode = IPC_ENV_MODE_DAY;
    switch (_g_device_status.ircut_status) {
        case E_BULB_MODE_INTELLIGENT: {
            _env_mode = IPC_ENV_AUTO;
            break;
        }
        case E_BULB_MODE_IR: {
            _env_mode = IPC_ENV_INFRARED;
            break;
        }
        case E_BULB_MODE_FULLCOLOR: {
            _env_mode = IPC_ENV_FULL_COLOR;
            break;
        }
        case E_BULB_MODE_OFF: {
            _env_mode = IPC_ENV_WITH_NO_INFRARED;
            break;
        }
        default: {
            _env_mode = IPC_ENV_MODE_DAY;
            break;
        }
    }
    return _env_mode;
}

//===========motor control function callbacks=============//

int MFG_PtzInit_Callback()
{
    return 0;
}

static s32 _gh_ptz_control_timer_id = -1;
static struct {
    f32 h;
    f32 v;
} _g_ptz_abs                              = { .h = -1, .v = -1 };
static ipc_json_t _g_ptz_reset_position[] = {
    json_double("h", _g_ptz_abs.h),
    json_double("v", _g_ptz_abs.v),
};

static ipc_json_t _g_ptz_reset_postion_json[] = { json_object("instaview_ptz_reset_postion", _g_ptz_abs, _g_ptz_reset_position) };
static s32 _record_ptz_position(vptr usr_arg, pu8 tmp_mem, s32 tmp_mem_size)
{
    if (ipc_ptz_is_stop(IPC_PTZ_H) == 1 && ipc_ptz_is_stop(IPC_PTZ_V) == 1) {
        ipc_ptz_get_abs(IPC_PTZ_V, &_g_ptz_abs.v);
        ipc_ptz_get_abs(IPC_PTZ_H, &_g_ptz_abs.h);

        ipc_ptz_set_init_angle(IPC_PTZ_V, _g_ptz_abs.v);
        ipc_ptz_set_init_angle(IPC_PTZ_H, _g_ptz_abs.h);

        ipc_json_wrconf(SESSION, _g_ptz_reset_postion_json, ARRSIZE(_g_ptz_reset_postion_json));
        return -1; // stop timer
    }
    return 500;
}

void ipc_iv_ptz_set_init()
{
    ipc_json_rdconf(SESSION, _g_ptz_reset_postion_json, ARRSIZE(_g_ptz_reset_postion_json));
    ipc_ptz_set_init_angle(IPC_PTZ_V, _g_ptz_abs.v);
    ipc_ptz_set_init_angle(IPC_PTZ_H, _g_ptz_abs.h);
}
// function to get current time
long long current_time_in_millis()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec * 1000LL) + (time.tv_usec / 1000);
}

int MFG_PtzMove_Callback(int Horizontal, int vertical)
{
    static int direct = 0;

    if (Horizontal == 0) {
        if (vertical < 0) {
            direct = (abs(vertical) / 50) * 10;
            if (direct > 360) {
                direct = 360;
            }
            if (_g_device_status.flip_status) {
                ipc_ptz_turn(IPC_PTZ_DOWN, direct);
            } else {
                ipc_ptz_turn(IPC_PTZ_UP, direct);
            }
        } else if (vertical > 0) {
            direct = (vertical / 50) * 10;
            if (direct > 360) {
                direct = 360;
            }
            if (_g_device_status.flip_status) {
                ipc_ptz_turn(IPC_PTZ_UP, direct);
            } else {
                ipc_ptz_turn(IPC_PTZ_DOWN, direct);
            }
        }
    } else if (vertical == 0) {
        if (Horizontal < 0) {
            direct = (abs(Horizontal) / 100) * 10;
            if (direct > 360) {
                direct = 360;
            }
            if (_g_device_status.flip_status) {
                ipc_ptz_turn(IPC_PTZ_LEFT, direct);
            } else {
                ipc_ptz_turn(IPC_PTZ_RIGHT, direct);
            }

        } else if (Horizontal > 0) {
            direct = (Horizontal / 100) * 10;
            if (direct > 360) {
                direct = 360;
            }
            if (_g_device_status.flip_status) {
                ipc_ptz_turn(IPC_PTZ_RIGHT, direct);
            } else {
                ipc_ptz_turn(IPC_PTZ_LEFT, direct);
            }
        }
    }
    ipc_timer_stop(ipc_global_timer_pool(), _gh_ptz_control_timer_id);
    printf("ptz start move step:%d \r\n", direct);
    _gh_ptz_control_timer_id = ipc_timer_start(ipc_global_timer_pool(), 2 * 1000, _record_ptz_position, NULL);
    return 0;
}
int MFG_PtzStop_callback()
{
    printf("Ptz Stop\r\n");
    ipc_ptz_stop(IPC_PTZ_H);
    ipc_ptz_stop(IPC_PTZ_V);
    return 0;
}
static u8 is_first = 0;
int MFG_PtzReset()
{
    printf("ptz reset \r\n");
    if (is_first != 0) {
        ipc_ptz_set_init_angle(IPC_PTZ_V, -1);
        ipc_ptz_set_init_angle(IPC_PTZ_H, -1);
        _g_ptz_abs.h = -1;
        _g_ptz_abs.v = -1;
        ipc_json_wrconf(SESSION, _g_ptz_reset_postion_json, ARRSIZE(_g_ptz_reset_postion_json));
        ipc_ptz_recheck();
    }
    is_first = 1;
    return 0;
}

int MFG_PtzDeinit()
{
    ipc_ptz_turn(IPC_PTZ_UP, 360);
    ipc_ptz_turn(IPC_PTZ_RIGHT, 360); // rotate backward and upward, not facing front
    ipc_ptz_uninit();
    return 0;
}

int MFG_PtzGetStatus(PTZ_STATUS_ST* ptz_status)
{
    ipc_ptz_get_abs(IPC_PTZ_H, &_g_ptz_abs.h);
    ipc_ptz_get_abs(IPC_PTZ_V, &_g_ptz_abs.v);
    ptz_status->x = _g_ptz_abs.h;
    ptz_status->y = _g_ptz_abs.v;
    if (ipc_ptz_is_stop(IPC_PTZ_H) == 1 && ipc_ptz_is_stop(IPC_PTZ_V) == 1) {
        ptz_status->status = E_MOTOR_STOP;
        return E_MOTOR_STOP;
    } else {
        ptz_status->status = E_MOTOR_MOVING;
        return E_MOTOR_MOVING;
    }
}
int MFG_PtzGetParams(int* horizontal, int* vertical, int* speed)
{
    *horizontal = ipc_factory(ptz_h_max_angle);
    *vertical   = ipc_factory(ptz_v_max_angle);
    *speed      = ipc_factory(ptz_ctrl_speed);
    return 0;
}
int MFG_TrackSetConfig_callback(MfgTrackConfig* config)
{
    ipc_alarm_track_reset_ts(20);
    ipc_alarm_track_sw(config->enable);
    _g_device_status.alarm_track_sw = config->enable;
    if (!config->enable) {
        _g_ptz_abs.h = ipc_ptz_get_init_angle(IPC_PTZ_H);
        _g_ptz_abs.v = ipc_ptz_get_init_angle(IPC_PTZ_V);

        ipc_ptz_turn_abs(IPC_PTZ_H, _g_ptz_abs.h);
        ipc_ptz_turn_abs(IPC_PTZ_V, _g_ptz_abs.v);
    }
    return 0;
}

static void __ptz_pos_set_to_up_limit(int restore)
{

    if (ipc_factory(ptz_position_in_privacy_mode == 0)) {
        return;
    }

    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();
    if (decrypt->product_type != IPC_PRODUCT_TYPE_PTZ)
        return;

    if (restore) {
        f32 angle = ipc_ptz_get_init_angle(IPC_PTZ_V);
        ipc_ptz_turn_abs(IPC_PTZ_V, angle);
    } else {
        s32 dir = 0;

        if (ipc_factory(ptz_position_in_privacy_mode) == 1) {
            dir = _g_device_status.flip_status ? IPC_PTZ_DOWN : IPC_PTZ_UP;
        } else if (ipc_factory(ptz_position_in_privacy_mode) == 2) {
            dir = _g_device_status.flip_status ? IPC_PTZ_UP : IPC_PTZ_DOWN;
        }

        if (dir != 0) {
            ipc_ptz_turn(dir, 360);
        }
    }
}

#if defined(__SIERN_LIGHT__)
int MFG_SetFloodLightMode(E_FLOOD_LIGHT_MODE flood_light_mode)
{
    ipc_env_mode_e _env_mode = IPC_ENV_MODE_DAY;
    switch (flood_light_mode) {
        case E_FLOOD_LIGHT_OFF:
            break;
        case E_FLOOD_LIGHT_INTELLIGENT:
            _env_mode = IPC_ENV_AUTO;
            break;
        case E_FLOOD_LIGHT_FULLCOLOR:
            _env_mode = IPC_ENV_FULL_COLOR;
            break;
        default:
            printf("Unsupported flood light mode.\n");
            return -1;
    }

    ipc_env_set_flood_mode(_env_mode);

    return 0;
}

u8 _g_security_light_status;

static s32 set_siren_light(u8 siren_light_sw) // sound and light alarm function, enables indicator B, can flash red and blue lights
{
    ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_B, siren_light_sw ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE);
    return 0;
}

int MFG_SetSecurityLightMode(E_SECURITY_LIGHT_MODE security_light_mode)
{
    switch (security_light_mode) {
        case E_SECURITY_LIGHT_ALARM:
            set_siren_light(0);
            _g_security_light_status = 0;
            break;
        case E_SECURITY_LIGHT_ALARM_AND_MOTION:
            _g_security_light_status = 1;
            break;
        default:
            return -1;
    }
    return 0;
}
#else
int MFG_SetFloodLightMode(E_FLOOD_LIGHT_MODE flood_light_mode)
{

    return 0;
}

int MFG_SetSecurityLightMode(E_SECURITY_LIGHT_MODE security_light_mode)
{

    return 0;
}
#endif

int MFG_PtzSetPvcMode(int enable)
{
    printf("PVCMode enable\r\n");
    printf("enable:%d\r\n", enable);
    int led_state = -1;
    if (enable) {
        ipc_alarm_track_sw(0);
        ipc_light_trigger(0);
        ipc_status_led_switch(0);
        ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_A, IPC_IO_VALUE_IS_INACTIVE);
        ipc_env_set_mode(IPC_ENV_MODE_DAY);
        ipc_light_force_open(0);
        __ptz_pos_set_to_up_limit(0);
    } else {
        ipc_env_set_mode(_g_device_status.ircut_status);
        ipc_status_led_switch(1);
        ipc_handler_read_int("led_state", &led_state);
        if (led_state != -1) {
            ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_A, led_state == 1 ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE);
        }
        ipc_light_force_open(_g_device_status.light_sw);
        __ptz_pos_set_to_up_limit(1);
        ipc_alarm_track_sw(_g_device_status.alarm_track_sw);
    }
    return 0;
}

/**********************mic and speaker volume control *********************/
int MFG_EnableSpeaker(int status)
{
    ipc_mpp_speak_switch(status == 0 ? 0 : 1);
    return 0;
}

int MFG_WriteAudioPcmFrame(char* pcm_data, int32_t pcm_len) // Android device app continuously calls this callback and continuously sends non-all-zero
                                                            // data, causing audio playback interruption, clear playback buffer
{
    if (!pcm_data || pcm_len <= 0)
        return -1;
    // if the first four bytes are 0x00, drop this frame
    int drop_pcm = 1;
    for (int i = 0; i < 4; i++) {
        if (pcm_data[i] != 0x00) {
            drop_pcm = 0;
            break;
        }
    }
    if (drop_pcm) {
        return 0;
    }

    ipc_mpp_speak(pcm_data, pcm_len, IPC_AUDIO_ENC_TYPE_PCM);
    return 0;
}

// map progress bar value to volume value, default volume is 78
static s32 g_default_vol = 78;
int mapToVolume(int progressBarValue)
{
    return (progressBarValue * g_default_vol) / 100;
}

s32 ipc_iv_device_get_spk_vol(void) // adjust device speaker volume
{
    s32 spk_vol = 0;
    ipc_handler_read_int("spk_vol", &spk_vol);
    if (spk_vol <= 0) {
        ipc_mpp_get_speaker_volume(&g_default_vol, NULL);
        spk_vol = g_default_vol;
        return spk_vol;
    }
    printf("get speaker volume=========================[spk_vol:%d]\n", spk_vol);
    spk_vol = mapToVolume(spk_vol);
    return spk_vol;
}

int MFG_SetVolume(int volume) // set device speaker volume
{
    int tmp_vol = volume;
    volume      = mapToVolume(volume);
    ipc_mpp_set_speaker_volume(volume, -1);
    ipc_handler_write_int("spk_vol", tmp_vol);
    return 0;
}

int MFG_PlayAacAudiofile(E_AUDIO_FILE_NAME file) // audio file playback list, play corresponding audio based on filename
{
    printf("[GvDebug]func:%s,line:%d\r\n", __func__, __LINE__);
    ipc_mpp_play_t play = {
        .vol         = -1,
        .gain        = -1,  // default gain
        .ratio       = 100, // 100% vol gain
        .multiplier  = 1,
        .interval_ts = 0,
        .play_times  = 1,
        .duration_ts = 0,
    };
    ipc_mpp_play_switch(1);
    ipc_mpp_set_speaker_volume(80, -1);
    switch (file) {
        case E_AUDIO_FILE_RESET:
            ipc_mpp_play("reset", 1, &play);
            break;
        case E_AUDIO_FILE_WIFI_OK:
            ipc_mpp_play("wifiok", 1, &play);
            break;
        case E_AUDIO_FILE_WIFI_BAD:
            ipc_mpp_play("badwifi", 1, &play);
            ipc_sleep(3);
            break;
        case E_AUDIO_FILE_WIFI_REG:
            ipc_mpp_play("regwifi", 1, &play);
            break;
        case E_AUDIO_FILE_PAIR_READY:
            ipc_mpp_play("readytopair", 1, &play);
            break;
        case E_AUDIO_FILE_PAIR_SUCCESS:
            ipc_mpp_play("pairsuccess", 1, &play);
            break;
        case E_AUDIO_FILE_PAIR_FAIL:
            ipc_mpp_play("pairfail", 1, &play);
            break;
        case E_AUDIO_FILE_ARMING:
            ipc_mpp_play("arming", 1, &play);
            break;
        case E_AUDIO_FILE_ARMED:
            ipc_mpp_play("armed", 1, &play);
            break;
        case E_AUDIO_FILE_DISARM:
            ipc_mpp_play("disarming", 1, &play);
#if defined(__SIERN_LIGHT__)
            set_siren_light(0);
#endif
            break;
        case E_AUDIO_FILE_SIREN:
#if defined(__SIERN_LIGHT__)
            set_siren_light(1);
#endif
            ipc_mpp_play("siren", 1, &play);
            break;
        case E_AUDIO_FILE_INTRUDER_SIREN_BIRD:
            ipc_mpp_play("intruder_siren_bird", 1, &play);
            break;
        case E_AUDIO_FILE_INTRUDER_SIREN_COMMON:
            ipc_mpp_play("intruder_siren_common", 1, &play);
            break;
        default:
            break;
    }
    return 0;
}

/****************************** time OSD and logo control **************************/
// get time OSD switch status
s32 time_sw = 1;
s32 ipc_iv_device_get_time_status(void)
{
    return time_sw;
}

int MFG_OsdInit()
{

    return 0;
}
int MFG_OsdDeInit()
{
    return 0;
}
int MFG_OsdShowTime(int enable) // display time
{
    if (enable == 1) {
        time_sw = 1;
        ipc_mpp_osd_time_switch(time_sw);
    } else if (enable == 0) {
        time_sw = 0;
        ipc_mpp_osd_time_switch(time_sw);
    }

    return 0;
}

int MFG_OsdSetLogo(MfgOsdLogo* logo)
{
    return 0;
}

static s32 _g_swdg_fd;
int MFG_EnableWatchdog(int enabledog)
{
    _g_swdg_fd = ipc_swdg_reg(1);
    if (_g_swdg_fd < 0) {
        ipcfatal("Soft watchdog registration failed! retcode=[%d]", _g_swdg_fd);
        exit(-1);
    }
    return 0;
}

int MFG_RefreshWatchdog()
{
    printf("===feeddog===\r\n");
    ipc_swdg_feed(_g_swdg_fd, 60 * 2);
    return 0;
}

//===================OTA upgrade==================//
static void ipc_ota_before_reboot_result(s32 ret)
{
    printf("flasing:%d\n", ret);
}

static vptr instaview_ota_upgrade()
{
    sleep(3);
    ipc_ota_upgrade(ipc_ota_before_reboot_result);
    return NULL;
}
static vptr instaview_ota_prepare()
{
    ipc_ota_prepare("/tmp/ota.bin", -1, 0);
    return NULL;
}
int MFG_OtaStart_callback(char* firmware_path)
{
    ipc_swdg_unreg(_g_swdg_fd);
    printf("set ota firmware_path:%s\r\n", firmware_path);
    ipc_create_thread("ota_prepare", instaview_ota_prepare, NULL, 256 * 1024, 0);
    return 0;
}
int MFG_FlashFirmware_callback(FirmwareFlashContext* firmware)
{
    extern s32 frame_upgrade_falg;
    frame_upgrade_falg = 1;
    ipc_exec("chmod 777 /tmp/firmware.bin");
    ipc_exec("mv /tmp/firmware.bin /tmp/ota.bin");
    ipc_create_thread("ota_upgrade", instaview_ota_upgrade, NULL, 256 * 1024, 0);
    return 0;
}
