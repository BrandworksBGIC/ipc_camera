#ifndef __DEVICE_H__
#define __DEVICE_H__


typedef struct {
    v8 ssid[64];
    v8 password[128];
} ipc_qrcode_info_t, *ipc_qrcode_info_p;

int MFG_GetPrebultInfo(char* device_id, const int device_id_max_size, char* access_key, const int access_key_max_size);
int MFG_SystemReboot_callback();
int MFG_VideoStreamInit_callback(int chn);
int MFG_ReadVideoFrame_callback(E_VIDEO_STREAM_INDEX stream_index, MfgVideoFrame* pframeInfo,unsigned char** frame_data,int* frame_buffer_len);
int MFG_VideoStreamDeInit_callback(void);
int MFG_ReadAudioPcmFrame(char* buffer, int32_t len, int64_t* time_stamp, int64_t* frame_index);
int MFG_AudioDeInit();
int MFG_AudioInit(int audio_rate,int bit_width,int channels);
int MFG_ReadRawVideoData_callback(E_VIDEO_STREAM_INDEX stream_index, char* raw_data, const int raw_data_lenth, int* width, int* height, int type);
int MFG_VideoKeyFrameReq(E_VIDEO_STREAM_INDEX channel);
int MFG_WifiLink_Callback(const char* ssid, const char* password);
int MFG_LedSetColor_Callback(E_LED_COLOR_MODE led, int value);
int MfgVideoParamGet(E_VIDEO_STREAM_INDEX stearm_index, HalVideoEncodeParam* params);
E_WIFI_STATUS MFG_GetWifiLinkStatus();
void ipc_iv_device_reset_process(void);
int MFG_Snapshot_callback(char* PATH);
int MFG_InitIRCut_Callback();
ipc_env_mode_e ipc_iv_device_get_env_mode(void);
int MFG_PtzInit_Callback();
int MFG_PtzMove_Callback(int Horizontal, int vertical);
int MFG_WriteAudioPcmFrame(char* pcm_data, int32_t pcm_len);
int MFG_SetVolume(int percentage);
s32 ipc_iv_device_get_spk_vol(void);
int MFG_EnableSpeaker(int status);
int MfgVideoParamSet(E_VIDEO_STREAM_INDEX stream_index, HalVideoEncodeParam* params);
int MFG_PtzReset();
int MFG_PtzDeinit();
int MFG_PtzGetStatus(PTZ_STATUS_ST* ptz_status);
s32 ipc_iv_device_get_time_status(void);
s32 ipc_iv_device_get_osd_status(void);
int MFG_OsdInit();
int MFG_OsdShowTime(int enable);
int MFG_OsdSetLogo(MfgOsdLogo *logo);
int MFG_PlayAacAudiofile(E_AUDIO_FILE_NAME file);
s32 ipc_iv_device_get_spk_vol(void);
s32 ipc_iv_device_get_mic_vol(void);
int MFG_EnableWatchdog(int enabledog);
int MFG_RefreshWatchdog();
int MFG_FlashFirmware_callback(FirmwareFlashContext *firmware);
int MFG_OtaStart_callback(char *file_path);
void MFG_DeinitIRCut();
int MFG_LedInit_callback();
int MFG_OsdDeInit();
int MFG_VideoSetMirrorAndflip(int flip , int mirror );
int MFG_VideoGetMirrorAndflip(int *flip, int *mirror);
void *ipc_global_timer_pool(void);
int MFG_SetMDArea(IV_Rect_t* MDArea);
int MFG_EnableBulb(int light_switch);
int MFG_SetBulbMode(E_BULB_MODE Bulb_mode);
int MFG_SetIRCutMode(E_MODE_IRCUT mode);
int MFG_PtzGetParams(int *horizontal, int *vertical, int *speed);
int MFG_EnableMD(int enableMD);
void ipc_iv_ptz_set_init();
int MFG_PtzStop_callback();
int MFG_GetWifiRSSI_Callback(char *rssi , const int size);
int MFG_TrackSetConfig_callback(MfgTrackConfig *config);
int MFG_PtzSetPvcMode(int enable);
int MFG_SetFloodLightMode(E_FLOOD_LIGHT_MODE flood_light_mode);
int MFG_SetSecurityLightMode(E_SECURITY_LIGHT_MODE security_light_mode);
#endif