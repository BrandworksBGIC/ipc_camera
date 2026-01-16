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
#include "device.h"
#include "ipc_handler.h"
#include <fcntl.h>
#include "ipc_alarm.h"
#include "ipc_iv_alarm.h"
#include "ipc_iv_ble.h"

#include "ipc_middleware_sal.h"

#define SESSION "instaview"

// Helper function to extract version part before '-' character
static void extract_version(const char *full_version, char *version_only, size_t buffer_size)
{
    char *dash_pos = strchr(full_version, '-');
    if (dash_pos != NULL) {
        // Calculate length of version part
        size_t version_len = dash_pos - full_version;
        // Ensure we don't exceed buffer size
        if (version_len < buffer_size) {
            strncpy(version_only, full_version, version_len);
            version_only[version_len] = '\0'; // Null-terminate
        } else {
            // Fallback: copy up to buffer size - 1
            strncpy(version_only, full_version, buffer_size - 1);
            version_only[buffer_size - 1] = '\0';
        }
    } else {
        // No dash found, use original string (fallback)
        strncpy(version_only, full_version, buffer_size - 1);
        version_only[buffer_size - 1] = '\0';
    }
}

static MFG_CALLBACK_FUNCTIONS callback_functions;
static bool g_exit = false;
ipc_qrcode_info_t _g_wifi_info;
static vptr _gh_timer_pool = NULL;
void *ipc_global_timer_pool(void)
{
    return _gh_timer_pool;
}
static u8 _g_has_wifi = 0;
u8 _g_net_conn_status = 0;
static u8 _g_has_tfcard = 0;
void set_tfcard_format_status(u8 status);
u8 get_tfcard_format_status(void);

#if defined(__CHIP_AKV130__)
#define IPC_TAG "/tmp/ipc_tag"
static void ipc_handler_write_tag_int(pv8 key, s32 value)
{
    ipc_json_t json[] = {json_int(key, value)};
    ipc_json_wrconf(IPC_TAG, json, ARRSIZE(json));
}
#endif

static void _tfcard_monitor_event(ipc_tfcard_monitor_event_e event)
{
    switch (event)
    {
        case IPC_TFCARD_EVENT_PLUG_IN:
            _g_has_tfcard = 1;
            ipc_tfcard_remount();
            break;
        case IPC_TFCARD_EVENT_PULL_OUT:
            _g_has_tfcard = 0;
            break;
        case IPC_TFCARD_EVENT_MOUNT:
        {
            _g_has_tfcard = 1;
        }
        break;
        case IPC_TFCARD_EVENT_UMOUNT:
            _g_has_tfcard = 0;
            break;
        case IPC_TFCARD_EVENT_READONLY:
            _g_has_tfcard = 0;
            break;
        case IPC_TFCARD_EVENT_REMOUNT:
            _g_has_tfcard = 1;
            break;
        case IPC_TFCARD_EVENT_UNRECOGNIZED:
            _g_has_tfcard = 0;
            break;
        case IPC_TFCARD_EVENT_FORMAT_FAILED:
            set_tfcard_format_status(1);
            break;
        case IPC_TFCARD_EVENT_FORMAT_FINISH:
            set_tfcard_format_status(1);
            _g_has_tfcard = 1;
            break;
        default:
            break;
    }
}
//_g_format_finish 1: format completed, 0: formatting in progress
static u8 _g_format_finish = 1;
void set_tfcard_format_status(u8 status)
{
    if (status != _g_format_finish) {
        _g_format_finish = status;
    }
}

u8 get_tfcard_format_status()
{
    return _g_format_finish;
}

static int MFG_SDCardFormat()
{
    s32 cnt = 0;
    _g_format_finish = 0;

    ipc_tfcard_format();

    while (!_g_format_finish && cnt < 600) {
        cnt++;
        ipc_msleep(100);
    }
    return 0;
}

static E_MOUNT_STATUS MFG_SDCardMountStatus()
{
    return _g_has_tfcard;
}

static s32 _factory_misc(ipc_fty_misc_e cmd, vptr data, s32 len)
{
    switch (cmd)
    {
        case IPC_FTY_RESET_DEVICE:
        {
            ipc_iv_device_reset_process();
            break;
        }
        case IPC_FTY_GET_EXINFO:
        {
            ipc_fty_exinfo_p param = (ipc_fty_exinfo_p)data;

            ipc_decrypt_exinfo_p cloud_info = ipc_decrypt_exinfo();
            pv8 model  = (pv8)cloud_info->info_ctx;
            pv8 did = model + strlen(model) + 1;
            s32 item_count = 0;

            snprintf(param->string[item_count].key, sizeof(param->string[item_count].key), "instaview_model");
            snprintf(param->string[item_count].val, sizeof(param->string[item_count].val), model);
            item_count++;

            snprintf(param->string[item_count].key, sizeof(param->string[item_count].key), "instaview_did");
            snprintf(param->string[item_count].val, sizeof(param->string[item_count].val), did);
            item_count++;

            snprintf(param->string[item_count].key, sizeof(param->string[item_count].key), "mac");
            nw_mac_t mac = {{0}};
            s32 ret = ipc_handler_get_mac(&mac);
            if (ret == 0) {
                snprintf(param->string[item_count].val, sizeof(param->string[item_count].val), "%02X%02X%02X%02X%02X%02X", mac.mac[0], mac.mac[1], mac.mac[2], mac.mac[3], mac.mac[4], mac.mac[5]);
                printf("get mac[%s]", param->string[2].val);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return IPC_SUCCESS;
}




static u32 _mpp_get_realts(void)
{
    time_t time_seconds = time(NULL);
    struct tm now_time;

    localtime_r(&time_seconds, &now_time);

    ipc_date_tm_t ipc_data = {
        .year = now_time.tm_year + 1900,
        .mon = now_time.tm_mon + 1,
        .day = now_time.tm_mday,
        .hour = now_time.tm_hour,
        .min = now_time.tm_min,
        .sec = now_time.tm_sec,
    };

    return ipc_date2ts(&ipc_data);
}

EXAPI vptr ipc_middleware_sal_media_info(struct ipc_plat_audio_init_attr *audio_attr)
{
    struct ipc_api_s *h_plat = NULL;
    struct ipc_plat_audio_capability audio_cap;

    if (audio_attr)
    {
        h_plat = ipc_plat_api(0);
        h_plat->audio_query_capability(&audio_cap);
        audio_attr->audio_enc = IPC_AUDIO_ENC_TYPE_PCM;
        audio_attr->channel = IPC_AUDIO_CHANNEL_MONO;
        audio_attr->databits = IPC_AUDIO_DATABITS_16;
#if 1
        audio_attr->sample = IPC_AUDIO_SAMPLE_8K;
        audio_attr->frame_rate = 25;
#else
        audio_attr->sample = IPC_AUDIO_SAMPLE_16K;
        audio_attr->frame_rate = 50;
#endif
        audio_attr->ai_vol = audio_cap.default_ai_vol;
        audio_attr->ao_vol = audio_cap.default_ao_vol;
        audio_attr->ai_gain = audio_cap.default_ai_gain;
        audio_attr->ao_gain = audio_cap.default_ao_gain;
        audio_attr->enable_aec = 1;
        audio_attr->enable_ai = 1;
        audio_attr->enable_ao = 1;
    }

    return NULL;
}


s32 frame_upgrade_falg = 0;
int MFG_SystemReset_callback()
{
    ipc_timer_uninit(_gh_timer_pool, 1);
    return 0;
}
int MFG_SystemReboot_callback()
{
    if(frame_upgrade_falg == 0)
    {
        printf("system reboot!!\r\n");
        ipc_rm("/conf/gv.json");
        ipc_rm("/conf/instaview.json");
        ipc_exec("reboot");
        return 0;
    }
    return 0;
}

int MFG_ResetButtonInit_Callback()
{
    return 0;
}

int MFG_ResetButtonReadValue_callback()
{
    s32 ret = -1;
    IPC_IO_VALUE_TYPE value = 0;
    ret = ipc_plat_api(0)->io_read(IPC_IO_NAME_RESET_BUTTON, &value);
    if(ret < 0)
    {
        return ret;
    }
    if (value == IPC_IO_VALUE_IS_ACTIVE)
    {
        return 0;
    }else{
        return 1;
    }
}

static void _net_event(ipc_net_event_e event)
{
    switch (event)
    {
        case IPC_NET_EVENT_WIRED_CONNECT:
        {
            _g_net_conn_status = 1;
        }
        break;
        case IPC_NET_EVENT_WIRED_DISCONNECT:
            _g_net_conn_status = 0;
            break;
        case IPC_NET_WIFI_INIT_SUCCESS:
            _g_has_wifi = 1;
            break;
        case IPC_NET_EVENT_STA_CONNECT_SUCCESS:
            _g_net_conn_status = 1;
            break;
        case IPC_NET_EVENT_STA_CONNECT_FAILED:
            _g_net_conn_status = 0;
            break;
        case IPC_NET_EVENT_STA_DISCONNECT:
            _g_net_conn_status = 0;
            break;
        case IPC_NET_EVENT_STA_RECONNECT:
            _g_net_conn_status = 1;
            break;
        case IPC_NET_EVENT_STA_PASSWORD_ERROR:
            _g_net_conn_status = 0;
            break;
        case IPC_NET_EVENT_INSMODE_WIFI_DRIVER:
        {
            break;
        }
        default:
            break;
    }
}


static int MFG_StartLightFlash_Callback()
{

    return 0;
}

static int MFG_StopLightFlash_Callback()
{

    return 0;
}


void SignalHandler(int signum) 
{
    int pid = 0;
    pid = (unsigned)pthread_self();

    switch (signum) {
        case SIGCHLD:
            break;

        case SIGSEGV:
            case SIGILL:
            printf("SignalHander(signal:%u)(pid:%u)\n", (unsigned int)signum, (unsigned int)pid);
            printf("SIGILL or SIGSEGV break\n");
            g_exit = true;
            break;

        case SIGFPE:
            break;

        case SIGINT:
        case SIGTERM:
        case SIGKILL:
            g_exit = true;
            break;

        case SIGABRT:
        case SIGSTOP:
            printf("SignalHander(signal:%u)(pid:%u)\n", (unsigned int)signum, (unsigned int)pid);
            g_exit = true;
            break;

        default:
            break;
    }
}

static void exitHandler(void) 
{ 
    g_exit = true; 
}

void SignalRegister()
{
    signal(SIGINT, SignalHandler);
    signal(SIGPIPE, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGKILL, SignalHandler);
    signal(SIGABRT, SignalHandler);
    signal(SIGSTOP, SignalHandler);
}
static void init_callback_functions() 
{
    callback_functions.MFG_ReadPrebultInfo_Callback     = MFG_GetPrebultInfo;
    callback_functions.MFG_SystemReboot_Callback        = MFG_SystemReboot_callback;
    callback_functions.MFG_SystemReset_Callback         = MFG_SystemReset_callback;
    callback_functions.MFG_ResetButtonReadValue_Callback= MFG_ResetButtonReadValue_callback;
    callback_functions.MFG_ResetButtonInit_Callback     = MFG_ResetButtonInit_Callback;
    callback_functions.MFG_LedInit_Callback             = MFG_LedInit_callback;
    callback_functions.MFG_LedSetColor_Callback         = MFG_LedSetColor_Callback;
    //----video----//
    callback_functions.MFG_VideoStreamInit_Callback     = MFG_VideoStreamInit_callback;
    callback_functions.MFG_ReadVideoFrame_Callback      = MFG_ReadVideoFrame_callback;
    callback_functions.MFG_VideoStreamDeInit_Callback   = MFG_VideoStreamDeInit_callback;
    callback_functions.MfgVideoParamGet_Callback        = MfgVideoParamGet;
    callback_functions.MfgVideoParamSet_Callback        = MfgVideoParamSet;//empty implementation
    // callback_functions.MFG_ChangeResolution_Callback    = MFG_ChangeResolution_callback;//implementation unknown

    callback_functions.MFG_VideoSetMirrorAndflip_Callback = MFG_VideoSetMirrorAndflip;
    callback_functions.MFG_VideoGetMirrorAndflip_Callback = MFG_VideoGetMirrorAndflip;
    //----audio----//
    callback_functions.MFG_AudioInit_Callback           = MFG_AudioInit;
    callback_functions.MfgReadAudioPcmFrame_Callback    = MFG_ReadAudioPcmFrame;
    callback_functions.MFG_AudioDeInit_Callback         = MFG_AudioDeInit;
    callback_functions.MfgWriteAudioPcmFrame_Callback   = MFG_WriteAudioPcmFrame;
    callback_functions.MfgSetVolume_Callback            = MFG_SetVolume;
    callback_functions.MfgEnableSpeaker_Callback        = MFG_EnableSpeaker;
    callback_functions.MfgPlayAacAudiofile_Callback     = MFG_PlayAacAudiofile;
    callback_functions.MfgEncodeAACAudio_Callback       = MFG_EncodeAACAudio;
    //----wifi----//
    callback_functions.MFG_ReadRawVideoData_Callback    = MFG_ReadRawVideoData_callback;
    callback_functions.MFG_WifiLink_Callback            = MFG_WifiLink_Callback;
    callback_functions.MFG_VideoKeyFrameReq_Callback    = MFG_VideoKeyFrameReq;
    callback_functions.MFG_GetWifiLinkStatus_Callback   = MFG_GetWifiLinkStatus;
    callback_functions.MFG_GetIPAddress_Callback        = MFG_GetWifiIPAddress;
    callback_functions.MFG_GetWifiRSSI_Callback         = MFG_GetWifiRSSI_Callback;

    //---event callbacks---//
    callback_functions.MFG_Snapshot_Callback            = MFG_Snapshot_callback;
    callback_functions.MFG_InitMD_Callback              = MFG_InitMD;
     callback_functions.MFG_EnableMD_Callback           = MFG_EnableMD;
    callback_functions.MFG_GetMDResult_Callback         = MFG_GetMDResult;
    callback_functions.MFG_DeinitMD_Callback            = MFG_DeinitMD;
    callback_functions.MFG_SetMDSensitivity_Callback    = MFG_SetMDSensitivity;
    callback_functions.MFG_SetMDSensitivityEx_Callback  = MFG_SetMDSensitivityEx;
    callback_functions.MFG_SetMDArea_Callback           = MFG_SetMDArea;
    callback_functions.MFG_SetAIConfig_Callback         = MFG_SetAIConfig_Callback;

#if !defined(__CHIP_AKV300__) || !defined(__CHIP_AKV130__)
    callback_functions.MFG_GetAIResult_Callback         = MFG_GetAIResult_Callback;
#endif
    //----ircut----//
    callback_functions.MFG_InitIRCut_Callback           = MFG_InitIRCut_Callback;
    callback_functions.MFG_DeinitIRCut_Callback         = MFG_DeinitIRCut;
    callback_functions.MFG_EnableBulb_Callback          = MFG_EnableBulb;
    callback_functions.MFG_SetBulbMode_Callback         = MFG_SetBulbMode;
    callback_functions.MFG_SetIRCutMode_Callback        = MFG_SetIRCutMode;
    //----motor control callbacks----//
    callback_functions.MFG_PtzInit_Callback             = MFG_PtzInit_Callback;
    callback_functions.MFG_PtzMove_Callback             = MFG_PtzMove_Callback;
    callback_functions.MFG_PtzReset_Callback            = MFG_PtzReset;
    callback_functions.MFG_PtzDeinit_Callback           = MFG_PtzDeinit;
    callback_functions.MFG_PtzGetStatus_Callback        = MFG_PtzGetStatus;
    callback_functions.MFG_PtzGetParams_Callback        = MFG_PtzGetParams;
    callback_functions.MFG_PtzStop_Callback             = MFG_PtzStop_callback;
    callback_functions.MFG_TrackSetConfig_Callback      = MFG_TrackSetConfig_callback;
    //callback_functions.MFG_PtzSetSpeed_Callback         = MFG_PtzSetSpeed;//platform reserved interface, no implementation needed
    //callback_functions.MFG_PtzStop_Callback             = MFG_PtzStop_Callback;//no implementation needed
    //callback_functions.MFG_PtzCruise_Callback           = MFG_PtzCruise;//not available, no implementation needed

    callback_functions.MFG_OsdInit_Callback             = MFG_OsdInit;
    callback_functions.MFG_OsdShowTime_callback         = MFG_OsdShowTime;
    //callback_functions.MFG_OsdSetLogo_Callback          = MFG_OsdSetLogo;//logo RGB image data provided by platform, to be implemented later
    callback_functions.MFG_OsdDeInit_Callback           = MFG_OsdDeInit;
    //---watch----//
    callback_functions.MFG_EnableWatchdog_Callback      = MFG_EnableWatchdog;
    callback_functions.MFG_RefreshWatchdog_Callback     = MFG_RefreshWatchdog;
    callback_functions.MFG_FlashFirmware_Callback       = MFG_FlashFirmware_callback;
    callback_functions.MFG_OtaStart_Callback            = MFG_OtaStart_callback;
    callback_functions.MFG_SDCardFormat_Callback        = MFG_SDCardFormat;
    callback_functions.MFG_SDCardMountStatus_Callback   = MFG_SDCardMountStatus;
    callback_functions.MfgEnableBluetooth_Callback      = NULL; //MFG_EnableBluetooth;
    callback_functions.MFG_PtzSetPvcMode_Callback       = MFG_PtzSetPvcMode;

    callback_functions.MFG_SetFloodLightMode_Callback   = MFG_SetFloodLightMode;
    callback_functions.MFG_SetSecurityLightMode_Callback= MFG_SetSecurityLightMode;

    callback_functions.MFG_StartLightFlash_Callback = MFG_StartLightFlash_Callback;
    callback_functions.MFG_StopLightFlash_Callback  = MFG_StopLightFlash_Callback;
    #if 0
    callback_functions.MFG_GetISPParams_Callback        = MFG_GetISPParams;
    //third phase callbacks to be implemented 
    callback_functions.MFG_SetWatchdogTimeout_Callback = MFG_SetWatchdogTimeout;
    callback_functions.MFG_OsdSetText_Callback          = MFG_OsdSetText;
    #endif
}

s32 init_env_set_mode(vptr usr_arg, pu8 tmp_mem, s32 tmp_mem_size)
{

#ifdef __BLACK_LIGHT__
    ipc_env_set_mode(IPC_ENV_INFRARED);    
#else
    s32 _env_mode = 0;
    ipc_handler_read_int("env_mode", &_env_mode);
    ipc_env_set_mode(IPC_ENV_AUTO);
    sleep(3);
    ipc_env_set_mode(_env_mode);
#endif
    return -1;
}

s32 ipc_middleware_main_process(pv8 ipc_version)
{
    _gh_timer_pool = ipc_timer_init(10, 0);
    /* signal handling */
    s32 ret = 0;
    signal(SIGPIPE, SIG_IGN);
    clog_init("main", "instaview main process");
    atexit(exitHandler);
    SignalRegister();


    ret = ipc_tfcard_monitor_init(_tfcard_monitor_event); // TODO
    if (ret < 0) {
        return ret;
    }

    for (s32 cnt = 0; !_g_has_tfcard && cnt < 100; cnt++) {
        ipc_msleep(50); // delayed detection for TF card existence
    }
    
    ipc_decrypt_ininfo_p ipc_info = ipc_decrypt_ininfo();

    struct stat sb;
    pv8 aging_log_dir = "/mnt/sdcard/jd_laohua";
    
    
    if (lstat(aging_log_dir, &sb) == 0) {
        if ((sb.st_mode & S_IFMT) == S_IFDIR) {
            ipc_factory_aging_test(aging_log_dir);
        }
    }
    sleep(1);
    printf("=====device_id:%llu====\r\n",ipc_info->device_id);

    ipc_decrypt_exinfo_p cloud_info = ipc_decrypt_exinfo();
    pv8 model = (pv8)cloud_info->info_ctx;
    pv8 did = model + strlen(model) + 1;

    printf("=======[did:%s]=======\n", did);


    
    ipc_factory_try_run("instaview", ipc_version, _factory_misc);
    

    ipc_mpp_cb_t mpp_cb = {NULL, NULL, _mpp_get_realts, NULL};//active push streaming
    ret = ipc_mpp_init(&mpp_cb);//initialization
    if (ret < 0) {
        return ret;
    }

    ipc_iv_ptz_set_init();
    
    ret = ipc_ptz_init();//motor initialization
    if (ret < 0) {
        return ret;
    }
    
    ret = ipc_net_init(1, _net_event); // TODO
    if (ret < 0) {
        return ret;
    }

    ipc_mpp_set_speaker_volume(ipc_iv_device_get_spk_vol(), -1);

    for (s32 cnt = 0; !_g_has_wifi && cnt < 100; cnt++) {
        ipc_msleep(50); // USB detection may be delayed
    }

    ipc_mpp_osd_time_switch(ipc_iv_device_get_time_status());
    ipc_mpp_osd_logo_switch(ipc_iv_device_get_time_status());

    ipc_status_led_init();

    ret = ipc_env_monitor_init(ipc_iv_device_get_env_mode());
    if (ret < 0){
        return ret;
    }
    


    ipc_plat_api(0)->io_write(IPC_IO_NAME_STATUS_INDICATOR_B, IPC_IO_VALUE_IS_INACTIVE);

    IPC_IO_VALUE_TYPE value = 0;
    ipc_plat_api(0)->io_read(IPC_IO_NAME_FILL_LIGHT, &value);
    if (value) {
        // ipc_fill_light_sw(0);
    }
    else{
        ipc_light_force_open(0);
    }

    struct ipc_plat_video_capability video_cap = {0};
    struct ipc_api_s *h_plat = ipc_plat_api(0);
    h_plat->video_query_capability(&video_cap);
    iv_sdk_set_venc_type(video_cap.type[IPC_VIDEO_CHN_MAIN] == IPC_VIDEO_ENC_TYPE_H265 ? E_MEDIA_VENC_H265 : E_MEDIA_VENC_H264);

    ipc_iv_queue_init_aac_encode();
    /* The following function fills in the callbacks within the callback_functions structure*/
    memset(&callback_functions, 0, sizeof(MFG_CALLBACK_FUNCTIONS));
    init_callback_functions();
    /* This function is used to register callback functions. You need to provide a MFG_CALL_BACK_FUNCTIONS structure pointer, which contains a set of callback function pointers */
    iv_sdk_register_callback_functions(&callback_functions);
    iv_sdk_set_cfg_path("/conf");
    iv_sdk_set_sdcard_mount_path("/mnt/sdcard");
    /* This function is used to initialize the SDK */
    // Extract version part before '-' character
    char version_only[64]; // Buffer large enough for version format like "01.00.001"
    extract_version(ipc_version, version_only, sizeof(version_only));

    iv_sdk_set_ota_version(version_only);

    iv_sdk_set_venc_type(E_MEDIA_VENC_H265);

    iv_sdk_set_substream(1);

    iv_sdk_init();
    ipc_timer_start(ipc_global_timer_pool(), 40 * 1000, init_env_set_mode, NULL);
    while (g_exit == false)
    {
        sleep(1);
    }
    /* This function is used to deinitialize the SDK */
    iv_sdk_deinit();
    return 0;
}
