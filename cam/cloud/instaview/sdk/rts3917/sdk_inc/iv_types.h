#if !defined(__INCLUDED_IPCBASE_Type____H__)
#define __INCLUDED_IPCBASE_Type____H__
#include  <unistd.h>
#include  <stdint.h>
#include  <stdlib.h>
#include  <stdio.h>
#include  <semaphore.h>
#include  <errno.h>
#include  <sys/types.h>
#include  <sys/wait.h>
#include  <pthread.h>
#include  <string.h>
#include  <sys/prctl.h>
#include  <sys/time.h>
#include  <sys/mount.h>
#include  <time.h>


#define  IV_FILE_PATH_NAME_LEN  1024
#define  IV_DEBUG_OSD_INFO_LEN  256

typedef int						IVINT32;
typedef unsigned int			IVUINT32;
typedef signed short			IVINT16;
typedef unsigned short			IVUINT16;
typedef			char			IVCHAR;
typedef unsigned char			IVUCHAR;
typedef signed char				IVSCHAR;
typedef uint64_t				IVUINT64;
typedef int64_t					IVINT64;
typedef float					IVFLOAT;
typedef double					IVDOUBLE;
typedef unsigned char			IVBOOL;
typedef void					IVVOID;
typedef unsigned long			IVULONG;
typedef long					IVLONG;
typedef IVVOID*					IVHANDLE;
#define IVTRUE					(IVBOOL)1
#define IVFALSE					(IVBOOL)0


#pragma pack(push, 4)
typedef enum {
    SDCARD_STATUS_ERROR = -1,
    SDCARD_STATUS_UMOUNT,
    SDCARD_STATUS_MOUNT
} E_MOUNT_STATUS;

typedef enum
{
	HAL_FSHARE_READER_RMM,
	HAL_FSHARE_READER_REC,
	HAL_FSHARE_READER_REC_SHORT,
	HAL_FSHARE_READER_CLOUD,
	HAL_FSHARE_READER_P2P,
	HAL_FSHARE_READER_P2P_END,
	HAL_FSHARE_READER_HTTP,
	HAL_FSHARE_READER_SW,
	HAL_FSHARE_READER_RTMP,
	HAL_FSHARE_READER_NUM
}E_FSHARE_READER_MODULE_ID;

typedef enum
{
	E_SEEK_SET = 0,/*from begin postion*/
	E_SEEK_CURR = 1,/*from current position*/
	E_SEEK_END = 2,/*from enc position*/
}E_SEEK_TYPE;


// Audio encoding formats supported
typedef enum
{
    E_AUDIO_OPUS = 0,  // OPUS format
    E_AUDIO_AAC  = 1,   // AAC format
    E_AUDIO_G711 = 2
} E_AUDIO_ENCODING_TYPE;

// Struct for audio frame information
typedef struct
{
    E_AUDIO_ENCODING_TYPE enc_type;    // Audio encoding type (OPUS/AAC)
    IVUINT32 bitrate;                  // Audio bitrate in kbps
    IVUINT32 sample_rate;              // Audio sample rate (e.g., 44100, 48000 Hz)
    IVUINT32 channels;                 // Number of audio channels (1 for mono, 2 for stereo)
    IVUINT32 bit_width;                // Bit width per sample (e.g., 16 bits, 24 bits)
    IVUINT32 data_len;                 // Length of the audio data in bytes
	IVUINT32 ext_data_len;
	IVUCHAR* audio_data;               // Pointer to the audio data
    IVUINT64 time_stamp;               // Time stamp of the audio frame (unit: ms)
	IVUINT64 frame_index;
} HalAudioFrame;

// Struct for audio encoding parameters
typedef struct
{
    E_AUDIO_ENCODING_TYPE enc_type;  // Audio encoding type (OPUS/AAC)
    IVUINT32 bitrate;                // Target bitrate in kbps
    IVUINT32 sample_rate;            // Sample rate (e.g., 44100, 48000 Hz)
    IVUINT32 channels;               // Number of audio channels (1 for mono, 2 for stereo)
    IVUINT32 bit_width;              // Bit width per sample (e.g., 16 bits, 24 bits)
} HalAudioEncodeParam;


typedef enum
{
	E_VIDEO_MAIN_STREAM    = 0,//video channel
	E_VIDEO_SECOND_STREAM,
	E_VIDEO_THIRD_STREAM,  
	E_VIDEO_MAX_NUM,
}E_VIDEO_STREAM_INDEX;

typedef enum
{
	E_P_FRAME = 0,
	E_I_FRAME = 1
}E_VIDEO_FRAME_TYPE;
 

typedef enum
{
	E_VIDEO_H264_BP		= 1,
	E_VIDEO_H264_MP		= 2,
	E_VIDEO_H264_HP		= 3,
	E_VIDEO_H265_A		= 4,
	E_VIDEO_MJPEG       = 5,
	E_VIDEO_YUV         = 6,
}E_VIDEO_ENCODING_TYPE;


typedef enum
{
	E_VIDEO_CBR = 1,
	E_VIDEO_VBR = 2,
	E_VIDEO_ECBR = 3,
	E_VIDEO_EVBR = 4
}E_VIDEO_BRMODE_TYPE;

typedef enum
{
	NIGHT_VISION_AUTO,
	NIGHT_VISION_ON,
	NIGHT_VISION_OFF,
	NIGHT_VISION_MAX,
}E_NIGHT_VISION_MODE;


typedef struct 
{
	E_VIDEO_STREAM_INDEX			stream_index; //channel no
	E_VIDEO_ENCODING_TYPE 			enc_type;//encode type(H265/H264/JPEG)
	E_VIDEO_FRAME_TYPE				frame_type;
	IVUINT32						width;
	IVUINT32						height;	
	IVUINT32						data_len;
	/*ext_data_len is the length of the extension data. It follows the video data.
	If ext_data_len is greater than 0,
	the effective length of the video data is data_len - ext_data_len.
	The last ext_data_len bytes of the data represent the extension data.*/
	IVUINT32						ext_data_len;
	IVUINT32                        frame_header_len;
	IVUINT32                        frame_buffer_len;
	IVUCHAR							*video_data; 
	/*time_stamp unit:ms*/
	IVUINT64						time_stamp;
	IVUINT64                        frame_index;
	   
}MfgVideoFrame;



typedef struct
{
	/*video param*/
	E_VIDEO_ENCODING_TYPE		    enc_type;
	E_VIDEO_BRMODE_TYPE				br_mode;
	IVUINT16						width;
	IVUINT16						height;
	IVUINT16						fps;
	IVUINT16						gop;
	IVUINT16						kbps;/**/
	IVUCHAR							quality_level;/*video quality 0 - 10, The larger the number, the better the image quality.*/
	IVUCHAR 						reserve[5];  
}HalVideoEncodeParam;


typedef struct {
	IVUCHAR							ir_enable; //0:OFF, 1:ON
	IVUCHAR							night_vision_mode; //0:AUTO, 1:ON, 2:OFF
	IVUCHAR							reserve[2];
} HalNightVisonParam;


typedef struct {
	IVUINT32 x;
	IVUINT32 y;
	IVUINT32 status;
	IVUINT32 speed;
	IVUINT32 saveFile;
} HalMotorMessage;

typedef struct
{
    IVUINT32  enable;                      
    IVUINT32  rotate;                      
    IVUINT32  maxW;                        
    IVUINT32  maxH;                        
    IVUINT32  alpha;                       
	IVUINT32  fTopLeftX; 
    IVUINT32  fTopLeftY; 
    IVUINT32  fWidth;   
    IVUINT32  fHeigh;   
    char     *pData;                       
} MfgOsdLogo;

typedef struct
{
    char cpu_idle[IV_DEBUG_OSD_INFO_LEN];
	char mem_free[IV_DEBUG_OSD_INFO_LEN];
	char mem_cache[IV_DEBUG_OSD_INFO_LEN];
	char enc_fps[IV_DEBUG_OSD_INFO_LEN];
	char enc_kbps[IV_DEBUG_OSD_INFO_LEN];
	char statistic_fps[IV_DEBUG_OSD_INFO_LEN];
	char statistic_kbps[IV_DEBUG_OSD_INFO_LEN];
} MfgOsdInfo;


typedef struct {
    const char* firmwarePath;  // Path to the firmware file
    char version[64];
    char reserved[64];         // Reserved space for future expansion
} FirmwareFlashContext;

typedef enum
{
	E_MEDIA_VENC_H264 = 0,
	E_MEDIA_VENC_H265 = 1
}E_MEDIA_VIDEO_ENC_TYPE;

typedef enum
{
	E_MEDIA_AUDIO_SAMPLE_RATE_16000 = 0,
	E_MEDIA_AUDIO_SAMPLE_RATE_8000 = 1
}E_MEDIA_AUDIO_SAMPLE_RATE;
typedef struct
{
	IVUINT32 count;
	IVUINT32 x;
	IVUINT32 y;
	IVUINT32 h;
	IVUINT32 w;
} MfgTrackTargetInfo;

typedef struct
{
	IVINT32 enable;		/* tracking function enable */
	IVINT32 boxEnable;	/* Humanoid box enable */
} MfgTrackConfig;

typedef struct
{
	int enable;
} MfgAIConfigInfo;

typedef enum {
    E_WAKE = 0,
    E_WAKING,
    E_SLEEPING,
    E_SLEPT,
} E_SLEEP_STATUS;

typedef enum {
    E_NONE = 0,
    E_MECHANICAL,
    E_ELECTRONIC,
} E_CHIME_TYPE;

typedef enum {
    CAMERA_STATUS_ERROR = -1,
    CAMERA_STATUS_ONLINE,
    CAMERA_STATUS_OFFLINE,
    CAMERA_STATUS_POWEROFF,
} E_CAMERA_STATUS;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((IVUINT32)(IVUCHAR)(ch0) | ((IVUINT32)(IVUCHAR)(ch1) << 8) |   \
                ((IVUINT32)(IVUCHAR)(ch2) << 16) | ((IVUINT32)(IVUCHAR)(ch3) << 24 ))
#endif				

#define AV_FRAME_HEADER					MAKEFOURCC('I','V','F','M')

#define FIRMWARE_HEADER_FLAGS			MAKEFOURCC('I','N','U','F')


#define FIELD_SZ_64		64
#define FIELD_SZ_128	128
#define FIELD_SZ_512	512
#define FIELD_SZ_1024	1024
#define FIELD_SZ_4096	4096


#pragma pack(pop)

#endif  


