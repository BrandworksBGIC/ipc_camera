#ifndef __IV_SDK_CALLBACK_TYPE__
#define __IV_SDK_CALLBACK_TYPE__

#include "iv_types.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_NUM_MD_AREA (32)
#define MD_AREA_COUNT_H (8)
#define MD_AREA_COUNT_V (4)

typedef enum {
    WIFI_UNKNOW = 0x0,
    WIFI_LINKED,
    WIFI_UNLINKED,
    WIFI_LINKING,
    WIFI_NOTCONFIG,
} E_WIFI_STATUS;

typedef enum {
    LED_COLOR_RED = 0, 
    LED_COLOR_BLUE,
} E_LED_COLOR_MODE;

typedef enum {
    E_MODE_AUTO = 0x0,
    E_MODE_NIGHT,
    E_MODE_DAY,
} E_MODE_IRCUT;

typedef enum {
    E_MOTOR_STOP = 0x0,
    E_MOTOR_MOVING,
    E_MOTOR_CRUISE,
    E_MOTOR_RESET,
} E_MOTOR_STATUS;

typedef enum {
    E_AUDIO_FILE_RESET = 0x0,
    E_AUDIO_FILE_WIFI_OK,
    E_AUDIO_FILE_WIFI_BAD,
    E_AUDIO_FILE_WIFI_REG,
    E_AUDIO_FILE_PAIR_READY,
    E_AUDIO_FILE_PAIR_SUCCESS,
    E_AUDIO_FILE_PAIR_FAIL,
    E_AUDIO_FILE_ARMING,
    E_AUDIO_FILE_ARMED,
    E_AUDIO_FILE_DISARM,
    E_AUDIO_FILE_SIREN,
    E_AUDIO_FILE_DOORBELL,
    E_AUDIO_FILE_POWEROFF,
    E_AUDIO_FILE_INTRUDER_SIREN_BIRD,
    E_AUDIO_FILE_INTRUDER_SIREN_COMMON,
    E_AUDIO_FILE_VIDEO_CALL,
    E_AUDIO_FILE_TEST,
} E_AUDIO_FILE_NAME;

typedef enum {
    E_MD_SEN_LOW = 0x0,
    E_MD_SEN_MIDDLE,
    E_MD_SEN_HIGH,
} E_MD_SEN_LEVEL;

typedef enum {
    E_BULB_MODE_INTELLIGENT = 0x0,
    E_BULB_MODE_IR,
    E_BULB_MODE_FULLCOLOR,
    E_BULB_MODE_MANUAL,
    E_BULB_MODE_OFF,
} E_BULB_MODE;

typedef enum {
    E_LAMP_MODE_INTELLIGENT = 0x0,
    E_LAMP_MODE_AUTO,
    E_LAMP_MODE_OFF,
    E_LAMP_MODE_MANUAL,
} E_LAMP_MODE;

typedef enum {
    E_WAKEUP_UNKNOW = 0x0,
    E_WAKEUP_REMOTE,
    E_WAKEUP_PIR,
    E_WAKEUP_BUTTON,
    E_WAKEUP_TIMER,
    E_WAKEUP_DOORBELL,
} E_STARTUP_METHOD;

typedef enum {
    E_PIR_SEN_LOW = 0x0,
    E_PIR_SEN_MIDDLE,
    E_PIR_SEN_HIGH,
} E_PIR_SEN_LEVEL;

typedef enum {
    E_WIFI_CONNECTING = 0x0,
    E_WIFI_CONNECTED,
    E_WIFI_CONNECT_FAIL,
    E_QRCODE_THREAD_EXIT,
    E_PAIR_SUCCESS,
    E_PAIR_FAIL,
} E_PAIRING_STATUS;

typedef enum {
    E_AI_NOTFOUND = 0x0,
    E_AI_HUMAN,
    E_AI_ANIMAL,
    E_AI_VEHICLE,
    E_AI_PACKAGE,
} E_AI_EVENT_TYPE;

typedef enum {
    E_BUTTON_NOTFOUND = 0x0,
    E_BUTTON_RESET,
    E_BUTTON_POWER_OFF,
    E_BUTTON_DOORBELL,
} E_BUTTON_TYPE;

typedef enum {
    E_VIDEO_CALL_STATUS_CALLING = 0x0,
    E_VIDEO_CALL_STATUS_ACCEPTED,
    E_VIDEO_CALL_STATUS_DISCONNECTED,
} E_VIDEO_CALL_STATUS;


typedef enum {
    E_IV_EVENT_NOTFOUND = 0x0,
    E_IV_EVENT_DOORBELL,
    E_IV_EVENT_VIDEO_CALL,
} E_IV_EVENT_TYPE;

typedef enum {
    E_SLEEP_NORMAL = 0x0,
    E_SLEEP_DEEP,
} E_MODE_SLEEP;

typedef enum {
    E_NETWORK_UNAVAILABLE = 0x0,
    E_NETWORK_AVAILABLE
} E_NETWORK_STATUS;



typedef struct _PTZ_STATUS_T {
    int x;
    int y;
    E_MOTOR_STATUS status; // 0: stop, 1: moving, 2: cruise
    int speed;
} PTZ_STATUS_ST;

typedef struct {
    unsigned int topLeftX;
    unsigned int topLeftY;
    unsigned int width;
    unsigned int heigh;
} IV_Rect_t;

typedef struct {
    int enable; 
    E_MD_SEN_LEVEL threshold;
    unsigned int framedelay;
    IV_Rect_t md_area[MAX_NUM_MD_AREA];
} IV_MDConfig_t;

typedef struct {
    bool en;
    struct {
        int hr;
        int mi;
    }start_time, stop_time;
} IV_PIRConfig_t;

typedef enum {
    E_FLOOD_LIGHT_OFF = 0,
    E_FLOOD_LIGHT_INTELLIGENT,
    E_FLOOD_LIGHT_FULLCOLOR,
} E_FLOOD_LIGHT_MODE;

typedef enum {
    E_SECURITY_LIGHT_ALARM = 0,
    E_SECURITY_LIGHT_ALARM_AND_MOTION,
} E_SECURITY_LIGHT_MODE;

typedef struct {
    /*
    * read prebuilt device id/access key from camera
    * @param[out]      char*                 device id
    * @param[in]       int                   the size of device id
    * @param[out]      char*                 access key
    * @param[in]       int                   the size of access key
    * @return          int                   0: success, -1: failed
    */
    int (*MFG_ReadPrebultInfo_Callback)(char *, const int, char *, const int);

    /*
    * start wifi connection service
    * @param[in]  const char*   wifi ssid
    * @param[in]  const char*   wifi password (If it is empty, it means there is
    * no password)
    * @return     int           0: success, -1: failed
    */
    int (*MFG_WifiLink_Callback)(const char *, const char *);

    /*
    * get wifi link status
    * @return     int                   0: unknow,
                                      1: linked
                                      2: unlinked
                                      3: linking
                                      4: not configured
    */
    E_WIFI_STATUS (*MFG_GetWifiLinkStatus_Callback)();

    /*
    * get rssi
    * @param[out]      char*                 rssi
    * @param[in]       int                   the size of rssi
    * @return          int                   0: success, -1: failed
    */
    int (*MFG_GetWifiRSSI_Callback)(char *, const int);

    /*
    * get ip address
    * @param[out]      char*                 ip address
    * @param[in]       int                   the size of ip address
    * @return          int                   0: success, -1: failed
    */
    int (*MFG_GetIPAddress_Callback)(char *, const int);

    /*
    * init ircut service
    * @return     int       0: success, -1: failed
    */
    int (*MFG_InitIRCut_Callback)();

    /*
    * deinit ircut service
    * @return     void
    */
    void (*MFG_DeinitIRCut_Callback)();

    /*
    * set ircut mode
    * @param[in]  int       0:auto, 1:night, 2:day
    * @return     int       0: success, -1: failed
    */
    int (*MFG_SetIRCutMode_Callback)(E_MODE_IRCUT);

    /*
    * get ircut mode
    * @return     int       0:auto, 1:night, 2:day
    */
    E_MODE_IRCUT (*MFG_GetIRCutMode_Callback)();

    /*
    * enable ircut
    * @param[in]  int       0:disable, 1:enable
    * @return     int       0: success, -1: failed
    */
    int (*MFG_EnableIRCut_Callback)(int);

    /*
    * start motion detection service
    * @param[in]    IV_MDConfig_t         MD config
    * @return       int                   0: success, -1: failed
    */
    int (*MFG_InitMD_Callback)(IV_MDConfig_t);

    /*
    * stop motion detection service
    */
    void (*MFG_DeinitMD_Callback)();

    /*
    * get MD check result
    * @return     int                   0: No detected, 1: detected
    */
    int (*MFG_GetMDResult_Callback)();

    /*
    * set MD sensitivity
    * @param[in] int       sensitivity
    * @return    int       0: success, -1: failed
    */
    int (*MFG_SetMDSensitivity_Callback)(int);

    /*
    * set MD sensitivity
    * @param[in] int	    0-255
    * @return    int       0: success, -1: failed
    */
    int (*MFG_SetMDSensitivityEx_Callback)(int);

    /*
    * set MD area
    * @param[in] IV_Rect_t area config
    * @return    int       0: success, -1: failed
    */
    int (*MFG_SetMDArea_Callback)(IV_Rect_t*);

    /*
    * enable MD 
    * @param[in] int       enable
    * @return    int       0: success, -1: failed
    */
    int (*MFG_EnableMD_Callback)(int);


    /*
    * Initialize the PTZ.
    * @return     int         0: success, -1: failed
    */
    int (*MFG_PtzInit_Callback)();

    /*
    * Reset the PTZ.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzReset_Callback)();

    /*
    * Move the PTZ.
    * @param[in]  pan : the number of steps the len is panned.
    * @param[in]  tilt : the number of steps the len is tilted.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzMove_Callback)(int, int);
    /*
    * Move the PTZ and save ptz coordinate to file.
    * @param[in]  pan : the number of steps the len is panned.
    * @param[in]  tilt : the number of steps the len is tilted.
    * @param[in]  int          1:save file    0:not save file
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzMove_Save_Callback)(int, int, int);
    /*
    * Stop the PTZ.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzStop_Callback)();

    /*
    * Get the status of the PTZ.
    * @param[out]  PTZ_STATUS_ST* : the status of the PTZ.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzGetStatus_Callback)(PTZ_STATUS_ST *);

    /*
    * Get the params of the PTZ.
    * @param[out] int*        motor max range horizontal
    * @param[out] int*        motor max range vertical
    * @param[out] int*        motor max speed
    * @return     int         0: success, -1: failed
    */
    int (*MFG_PtzGetParams_Callback)(int *, int *, int *);

    /*
    * Let the PTZ cruise.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzCruise_Callback)();

    /*
    * set the speed of the PTZ.
    * @param[in]  speed : the desired speed of the PTZ.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzSetSpeed_Callback)(unsigned int);

    /*
    * Exit the environment suitable for the PTZ run.
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_PtzDeinit_Callback)();

    /*
    * init led gpio direction
    * @return   int  0: success, -1: failed
    */
    int (*MFG_LedInit_Callback)();

    /*
    * set led color
    * @param[in] : the color of led.
    * @param[in] : the value of led.
    * @return          int  0: success, -1: failed
    */
    int (*MFG_LedSetColor_Callback)(E_LED_COLOR_MODE, int);

    /*
    * enable/disable led
    * @param[in] : 0: disable, 1: enable
    * @return      int  0: success, -1: failed
    */
    int (*MFG_EnableLED_Callback)(int);

    /*
    * init reset button gpio direction
    * @return   int  0: success, -1: failed
    */
    int (*MFG_ResetButtonInit_Callback)();

    /*
    * read the reset button gpio value
    * @return	int  0|1:  reset button gpio value	  -1:failed
    */
    int (*MFG_ResetButtonReadValue_Callback)();

    /*
    * reboot the system
    * @return          int  0: success, -1: failed
    */
    int (*MFG_SystemReboot_Callback)();

    /*
    * before ota download, run this callback to tell you we are going to download the ota file
    @param[in] char*	 A pointer to the ota file path
    * @return          int  0: success, -1: failed
    */	
    int (*MFG_OtaStart_Callback)(char *);

	    /*
    * if ota file download failed ,will run this callback to let you know
    * @return          int  0: success, -1: failed
    */	
    int (*MFG_OtaDownloadFailed_Callback)();

    /*To check the new version of the firmware is suitable the camera
    @param[in] FirmwareFlashContext*	A pointer to a FirmwareCheckContext
    structure.
    @return          					int  0: success, -1:
    failed
    */
    int (*MFG_FlashFirmware_Callback)(FirmwareFlashContext *);
    
    /*
    * get ISP vi width and height
    * @param[out] int*        isp vi width
    * @param[out] int*        isp vi height
    * @return     int         0: success, -1: failed
    */
    int (*MFG_GetISPParams_Callback)(int *, int *);

    /*set the flip and mirror
    * @param[in]  int                   flip 0: no flip, 1: flip
    * @param[in]  int                   mirror 0: no mirror, 1: mirror
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_VideoSetMirrorAndflip_Callback)(int, int);

    /*
    * @param[out]  int*                  flip 0: no flip, 1: flip
    * @param[out]  int*                  mirror 0: no mirror, 1: mirror
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_VideoGetMirrorAndflip_Callback)(int *, int *);

    /*int video encode
    * @param[in]  int                   Number of channels
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_VideoStreamInit_Callback)(int);

    /*Deinit video encode
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_VideoStreamDeInit_Callback)();

    /*get the raw video data
    * @param[in]  E_VIDEO_STREAM_INDEX    channel
    * @param[in]  char*  raw data buffer
    * @param[in]  int    raw data buffer length
    * @param[in]  int*   width
    * @param[in]  int*   height
    * @param[in]  int    yuv data type(0 NV12 for qr code
    *                                  1 UYVY for ai)
    * @return     int                   the length of raw data which get form chip sdk, -1: failed
    */
    int (*MFG_ReadRawVideoData_Callback)(E_VIDEO_STREAM_INDEX, char *, const int, int *, int *, int);

    /*get video data
    * @param[in]  E_VIDEO_STREAM_INDEX  channel
    * @param[in]  MfgVideoFrame*        the struct of video frame info, please memcpy encode video data to video_frame->video_data
    * @param[in]  unsigned char**       Pointer to the buffer where the video data is stored. Note that you only need to free and malloc again if the encoded data is larger than the buffer size, 
    *                                   and then assign the pointer of the new buffer to this parameter, in fact: video_frame->video_data = this parameter + video_frame->frame_header_len
    * @param[in]  int*        		   the size of the buffer
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_ReadVideoFrame_Callback)(E_VIDEO_STREAM_INDEX, MfgVideoFrame *, unsigned char**, int*);

    /*change the channel resolution
    * @param[in]  E_VIDEO_STREAM_INDEX        			channel
    * @param[in]  int   				width
    * @param[in]  int   				height
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_ChangeResolution_Callback)(E_VIDEO_STREAM_INDEX, int, int);


    /*force key frame
    * @param[in]  E_VIDEO_STREAM_INDEX  the video channel
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_VideoKeyFrameReq_Callback)(E_VIDEO_STREAM_INDEX);

    /*set video param
    * @return     int                   0: success, -1: failed
    */
    int (*MfgVideoParamSet_Callback)(E_VIDEO_STREAM_INDEX, HalVideoEncodeParam *);

    /*get video param
    * @param[in]  E_VIDEO_STREAM_INDEX  the video channel
    * @param[out] HalVideoEncodeParam*  the video param
    * @return     int                   0: success, -1: failed
    */
    int (*MfgVideoParamGet_Callback)(E_VIDEO_STREAM_INDEX, HalVideoEncodeParam *);

    /*Get a snapshot and save it to a file
    * @param[in]  char*                 Path to save the snapshot file
    * @return     int                   0 : success, -1: failed
    */
    int (*MFG_Snapshot_Callback)(char *);

    /*init osd
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_OsdInit_Callback)();

    /*deinit osd
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_OsdDeInit_Callback)();

    /*osd set logo
    * @param[in]  MfgOsdLogo *          osd logo setting attributes
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_OsdSetLogo_Callback)(MfgOsdLogo *);


    /*osd show time enable
    * @param[in]  int           enable,  0:close, 1:open
    * @return     int           0: success, -1: failed
    */
    int (*MFG_OsdShowTime_callback)(int);

    /*
    * init Audio
    * @param[in] int	          sample_rate
    * @param[in] int	          bit_width
    * @param[in] int           channels
    * @return   int  0: success, -1: failed
    */
    int (*MFG_AudioInit_Callback)(int, int, int);

    /* deinit Audio
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_AudioDeInit_Callback)();

    /* read pcm stream
    * @param[out] char*	          buff
    * @param[in] int32_t	      buff size
    * @param[out] int64_t*        timeStamp
    * @param[out] int64_t*	      frameIndex
    * @return     int             > 0: data len, <= 0: failed
    */
    int (*MfgReadAudioPcmFrame_Callback)(char *, int32_t, int64_t *, int64_t *);

    /* play pcm audio
    * @param[in] char *	          pcm data
    * @param[in] int32_t	      len
    * @return    int              0: success, -1: failed
    */
    int (*MfgWriteAudioPcmFrame_Callback)(char *, int32_t);


    /* play aac file
    * @param[in] E_AUDIO_FILE_NAME	   aac file
    * @return    int                   0: success, -1: failed
    */
    int (*MfgPlayAacAudiofile_Callback)(E_AUDIO_FILE_NAME);

#ifdef ALARM_SOUND_ROUTINE

    /* enable play aac audio file loop routine
    * @param[in] E_AUDIO_FILE_NAME	   aac file
    * @return    int                   0: success, -1: failed
    */
    int (*MfgEnablePlayAacAudiofileRoutine_Callback)(E_AUDIO_FILE_NAME);

    /* disable play aac audio file loop routine
    * @param[in] E_AUDIO_FILE_NAME	   aac file
    * @return    int                   0: success, -1: failed
    */
    int (*MfgDisablePlayAacAudiofileRoutine_Callback)(E_AUDIO_FILE_NAME);

#endif

    /* set speaker volume
    * @param[in] int	                  percentage
    * @return    int                   0: success, -1: failed
    */
    int (*MfgSetVolume_Callback)(int);

    /* enable speaker
    * @param[in] int                   0: close, 1: open
    * @return    int                   0: success, -1: failed
    */
    int (*MfgEnableSpeaker_Callback)(int);

    /* get aac encode data
    * @param[in]  char*	          pcm data
    * @param[in]  int32_t	      pcm data len
    * @param[out] char*           aac buff
    * @param[in]  int32_t	      aac buff size
    * @return     int              > 0: data len, <= 0: failed
    */
    int (*MfgEncodeAACAudio_Callback)(char *, int32_t , char *, int32_t);

    /*enable/disable watchdog
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_EnableWatchdog_Callback)(int);

    /*refresh watchdog
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_RefreshWatchdog_Callback)();

    /*set watchdog timeout
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_SetWatchdogTimeout_Callback)(int);

    /* enable bulb
    * @param[in] int                   0: close Bulb, 1: open Bulb
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_EnableBulb_Callback)(int);

    /* set bulb mode
    * @param[in] int                   0: Intelligent,
                                      1: Infrared,
                                      2: Full-Color,
                                      3: Manual
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_SetBulbMode_Callback)(E_BULB_MODE);

    /* init track function
    * @param[in] int                   0: close, 1: open
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_TrackInit_Callback)(void);
    /* deinit track function
    * @param[in] int                   0: close, 1: open
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_TrackDeInit_Callback)(void);
    /* get the XY of the tracking target
    * @param[in] int                   0: close, 1: open
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_TrackGetTargetInfo_Callback)(MfgTrackTargetInfo *target);

    /* set track configure
    * @param[in] MfgTrackConfig        track parameter configure
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_TrackSetConfig_Callback)(MfgTrackConfig *config);
	
    /*debug  osd ,show fps,bps,memory
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_DebugOsdInfo_Callback)(MfgOsdInfo *);


    /* Enable/Disable Bluetooth,if your camear support Bluetooth,it will return success

    @param[in] int   1 to enable, 0 to disable

    @return    int   0: success, -1: failed
    */
    int (*MfgEnableBluetooth_Callback)(int);  

    /* Enable/Disable Pvc Mode,if your camear support Pvc Mode,it will return success
    * @param[in] int   1 to enable, 0 to disable
    */
    int (*MFG_PtzSetPvcMode_Callback)(int); 

	/* only for battery camera, when Enable/Disable Pvc Mode,notify you to handle low power task,such as pir disable
    * @param[in] int   1 to enable, 0 to disable
    */
    int (*MFG_SetPvcModeNotify_Callback)(int); 

    /*
    * reset the camera
    * @return          int  0: success, -1: failed
    */
    int (*MFG_SystemReset_Callback)(void);


    /*
    * Clear all of position to store for PTZ .
    * @return     int                   0: success, -1: failed
    */
    int (*Mfg_MotorClearPtz_Callback)();

    /*
    * Format sdcard
    * @return     int                   0: success, -1: failed
    */
    int (*MFG_SDCardFormat_Callback)();

	/*
    * sdcard mount status
    * @return     int                   0: umount, 1: mount, -1 error
    * note:when sdcard insert or remove ,need to let this interface return 0 or -1
    */
	E_MOUNT_STATUS (*MFG_SDCardMountStatus_Callback)();

    /*
    * set PIR detection cooldown period
    * @param[in]    int           second
    * @return       int           0: success, -1: failed
    */
    int (*MFG_SetPirCooldownPeriod_Callback)(int);

    /*
    * set PIR Schedule
    * @param[in]    IV_PIRConfig_t*  pir config
    * @return       int           0: success, -1: failed
    */
    int (*MFG_SetPirSchedule_Callback)(IV_PIRConfig_t*);

#ifdef OEM_XMI
    int (*MFG_ReadFrame_Callback)(MfgVideoFrame *);

    /*
    * Notification mcu sleep
    * @return       int           0: success, -1: failed
    */
    int (*MFG_NotificationSleep_Callback)(void);

    /*
    * Notification mcu wakeup
    * @return       int           0: success, -1: failed
    */
    int (*MFG_Wakeup_Callback)(void);

    /*
    * Enable Audio Video
    @param[in] int   1 to enable, 0 to disable
    * @return       int           0: success, -1: failed
    */
    int (*MFG_EnableAudioVideo_Callback)(int);

    /*
    * Enable Liveview
    @param[in] int   1 to enable, 0 to disable
    * @return       int           0: success, -1: failed
    */
    int (*MFG_EnableLiveview_Callback)(int);

    /*
    * set radar area
    * @param[in] int zone count
    * @param[in] int* zone array
    * @return    int       0: success, -1: failed
    */
    int (*MFG_SetRadarZone_Callback)(int, int*);

    /*
    * set chime type
    * @param[in]    (duration << 8) | E_CHIME_TYPE
    * @return       int           0: success, -1: failed
    */
    int (*MFG_SetChimeType_Callback)(int);

    /*
    * get camera status
    * @return    E_CAMERA_STATUS
    */
    E_CAMERA_STATUS (*MFG_GetCameraStatus_Callback)(void);

    /*
    * soft reboot
    * @return          int  0: success, -1: failed
    */
    int (*MFG_SoftReboot_Callback)();
#else
    /*
    * Notification mcu sleep
    * @param[in]    E_MODE_SLEEP  0:normal, 1:deep
    * @return       int           0: success, -1: failed
    */
    int (*MFG_NotificationSleep_Callback)(E_MODE_SLEEP);
#endif

    /*
    * get battery power
    * @param[out]   int           power percent
    * @return       int           0: success, -1: failed
    */
    int (*MFG_GetBatteryPower_Callback)(int *);
    
    /*
    * set mcu mqtt URL
    * @param[in]    const char*   mqtt url
    * @return       int           0: success, -1: failed
    */
    int (*MFG_SetMcuMqttURL_Callback)(const char*);

    /*
    * set PIR Sensitivity
    * @param[in]    E_PIR_SEN_LEVEL 0:low, 1:middle, 2:high
    * @return       int             0: success, -1: failed
    */
    int (*MFG_SetPIRSensitivity_Callback)(E_PIR_SEN_LEVEL level);
    
    /*
    * set PIR Sensitivity
    * @param[in]    int	     0-255
    * @return       int             0: success, -1: failed
    */
    int (*MFG_SetPIRSensitivityEx_Callback)(int);
    
    /*
    * brun mcu firmware
    * @param[in] const char*  ota package path
    * @return     int         0: success, -1: failed
    */
    int (*MFG_BurnMcuFirmware_Callback)(const char *);

    /* set flood light mode
    * @param[in] int                  0: Off,
                                      1: Intelligent,
                                      2: Full-Color,
    * @return    int                   0: success, -1: failed
    */
   int (*MFG_SetFloodLightMode_Callback)(E_FLOOD_LIGHT_MODE);

   /* set flood light mode
    * @param[in] int                  0: Alarm,
                                      1: Alarm and Motion,
    * @return    int                   0: success, -1: failed
    */
   int (*MFG_SetSecurityLightMode_Callback)(E_SECURITY_LIGHT_MODE);

    /*
    * sync time
    * @param[in] long long    timestamp_ms
    * @return     int         0: success, 1: failed
    */
    int (*MFG_SyncTime_Callback)(long long timestamp_ms);
    
    /*
    * mcu wakeup timer
    * @param[in] int    period (unit second)
    * @return    int    0: success, 1: failed
    */
    int (*MFG_McuWakeupTimer_Callback)(int period);
    
    /*
    * set pairing state
    * @param[in] int    state
    * @return    int    0: success, 1: failed
    */
    int (*MFG_SetPairingStatus_Callback)(E_PAIRING_STATUS state);

    /*
    * get charging status
    * @return    int    0: not charging, 1: charging
    */
    int (*MFG_GetChargingStatus_Callback)(void);

    /*
    * get AI checking result
    * @return    int    0: not found
                        1: human
                        2: animal
                        3: vehicle
                        4: package
    */
    E_AI_EVENT_TYPE (*MFG_GetAIResult_Callback)(void);

    /*
    * set AI configure
    * @param[in] MfgAIConfigInfo*   AI configure
    * @return    int    0: success, 1: failed
    */
    int (*MFG_SetAIConfig_Callback)(MfgAIConfigInfo *);

    /*
    * get PIR checking result
    * @return    int    0: No detected, 1: detected
    */
    int (*MFG_GetPIRResult_Callback)(void);

    /*
    * HDR enable
    * @param[in]    int  0:disable, 1:enable
    * @return       int  0: success, -1: failed
    */
    int (*MFG_EnableHDR_Callback)(int);

    /*
    * mcu report
    * @param[in] const char* topic
    * @param[in] const char* message
    * @return    int    0: success, 1: failed
    */
    int (*MFG_McuReport_Callback)(const char* topic, const char* message);

    /*
    * set timezone
    * @param[in]    const char*  timezone
    * @return       int  0: success, -1: failed
    */
    int (*MFG_SetTZ_Callback)(const char* tz);

    /*
    * set lamp mode
    * @param[in] int                  0: Intelligent
                                      1: Auto
                                      2: Off
                                      3: MANUAL
    * @return    int                  0: success, -1: failed
    */
    int (*MFG_SetLampMode_Callback)(E_LAMP_MODE);

    /* 
    * enable Lamp
    * @param[in] int                   0: turn off , 1: turn on
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_EnableLamp_Callback)(int);

	/* 
    * notify oem when mqtt connect to cloud server success
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_mqtt_online_Callback)();
    
	/* 
    * start light flash
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_StartLightFlash_Callback)();
    
	/* 
    * stop light flash
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_StopLightFlash_Callback)();

	/* 
    * when app speaker button click/release will notify you
    * @param[in] int                   0: button release , 1: button click
    * @return    int                   0: success, -1: failed
    */
	int (*MFG_speaker_button_notify)(int);

#ifdef TWO_WAY_VIDEO
	 /* play video
    * @param[in] char *	          video data
    * @param[in] int 		      video len
    * @param[in] int 		      video index
    * @param[in] int		      video type :   1:keyframe  0:not keyframe
    * @param[in] long long	      timestamp  :	 ms
    * @return    int              0: success, -1: failed
    */
    int (*MfgWriteVideoFrame_Callback)(char *, int, int, int, long long);

	/* when app calling accepted disconnect will notify oem
    * @param[in] E_VIDEO_CALL_STATUS	          
    * @return    int              0: success, -1: failed
    */
    int (*Mfg_videoCallStatus_Callback)(E_VIDEO_CALL_STATUS);
#endif


#ifdef VEEPAI_DEVICE
	/* notify event start uploading
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_NotifyEventStartUploading_Callback)();

	/* notify event stop uploading
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_NotifyEventStopUploading_Callback)();
#endif

	/* 
    * notify oem network status:ping check and tcp check,only for some battery camera, they need this before sleep
    * @param[in] E_NETWORK_STATUS
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_network_check_Callback)(E_NETWORK_STATUS);

	/* 
    * when net not work，sdk will call this to restart net，this callback only for ac camera
    * It is recommended that OEMs implement their own network monitoring and reconnection strategies.
    * @return    int                   0: success, -1: failed
    */
    int (*MFG_wifi_restart_Callback)();

} MFG_CALLBACK_FUNCTIONS;

#endif
