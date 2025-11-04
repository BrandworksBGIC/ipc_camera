/**
 * @file ipc_platform_api.h
 * @author ipcplus
 * @brief This is the platform standard layer interface.
 * @version 0.1
 * @date 2021-3-18
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef __IPC_PLATFORM_API_H__
#define __IPC_PLATFORM_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

/** @brief Miscellaneous control commands */
typedef enum {
    IPC_PLAT_MISC_CTRL_CMD_GET_24C02_DEV_NODE,     ///< req empty, rsp char type buffer
    IPC_PLAT_MISC_CTRL_CMD_GET_PTZ_IO_GROUP_INDEX, ///< req empty, rsp ps32 type
    IPC_PLAT_MISC_CTRL_CMD_GET_ENV_ADC_INFO,       ///< req empty, rsp struct ipc_plat_io_env_adc_info_s* type
    IPC_PLAT_MISC_CTRL_CMD_MAX = 0xffff,
} IPC_PLAT_MISC_CTRL_CMD;

/** @brief Video channel types */
typedef enum {
    IPC_VIDEO_CHN_MAIN,       ///< HD stream channel
    IPC_VIDEO_CHN_SUB,        ///< SD stream channel
    IPC_VIDEO_CHN_JPEG,       ///< Snapshot stream channel
    IPC_VIDEO_CHN_YUV,        ///< YUV stream channel
    IPC_VIDEO_CHN_NUM = 0xFF, ///< Maximum number of channels
} IPC_VIDEO_CHN_TYPE;

/** @brief Product type definitions */
typedef enum {
    IPC_PRODUCT_TYPE_PTZ           = 1,  ///< PTZ camera
    IPC_PRODUCT_TYPE_38            = 2,  ///< 38 gun camera
    IPC_PRODUCT_TYPE_CARD          = 3,  ///< Card camera
    IPC_PRODUCT_TYPE_FEEDER_V      = 4,  ///< Video version feeder
    IPC_PRODUCT_TYPE_SAFE_LIGHT    = 5,  ///< Security light
    IPC_PRODUCT_TYPE_PANO_360      = 6,  ///< Panoramic 360
    IPC_PRODUCT_TYPE_BABY_MONITORS = 7,  ///< Baby monitors
    IPC_PRODUCT_TYPE_38_PTZ        = 8,  ///< PTZ gun camera
    IPC_PRODUCT_TYPE_FOOD_THROWER  = 9,  ///< Food thrower
    IPC_PRODUCT_TYPE_DOORBELL      = 10, ///< Doorbell
    IPC_PRODUCT_TYPE_NUM,                ///< Number of existing product definitions
} IPC_PRODUCT_TYPE;

/** @brief Video encoding type definitions */
typedef enum {
    IPC_VIDEO_ENC_TYPE_H264    = 0x1 << 0, ///< H264 encoding type
    IPC_VIDEO_ENC_TYPE_H265    = 0x1 << 1, ///< H265 encoding type
    IPC_VIDEO_ENC_TYPE_JPEG    = 0x1 << 2, ///< JPEG encoding type
    IPC_VIDEO_ENC_TYPE_YUV     = 0x1 << 3, ///< YUV type
    IPC_VIDEO_ENC_TYPE_RESERVE = 0x1 << 15,
} IPC_VIDEO_ENC_TYPE;

/** @brief Day/Night image mode definitions */
typedef enum {
    IPC_VIDEO_MODE_DAY,              ///< Daytime
    IPC_VIDEO_MODE_NIGHT,            ///< Night vision
    IPC_VIDEO_MODE_NIGHT_FULL_COLOR, ///< Night vision full color
} IPC_VIDEO_MODE;

/** @brief Description of flip modes */
typedef enum {
    IPC_ISP_MIRRORFLIP_TYPE_NORMAL      = 0, ///< Normal image mode
    IPC_ISP_MIRRORFLIP_TYPE_MIRROR      = 1, ///< Mirror mode
    IPC_ISP_MIRRORFLIP_TYPE_FLIP        = 2, ///< Flip mode
    IPC_ISP_MIRRORFLIP_TYPE_MIRROR_FLIP = 3, ///< Mirror and flip mode
    IPC_ISP_MIRRORFLIP_TYPE_BUTT
} IPC_ISP_MIRRORFLIP_TYPE;

/** @brief Description of anti-flicker modes */
typedef enum {
    IPC_ISP_ANTIFLICKER_DISABLE, ///< Disable ISP flicker reduction feature
    IPC_ISP_ANTIFLICKER_50HZ,    ///< Enable ISP flicker reduction feature, set frequency to 50HZ
    IPC_ISP_ANTIFLICKER_60HZ,    ///< Enable ISP flicker reduction feature, set frequency to 60HZ
    IPC_ISP_ANTIFLICKER_BUTT,    ///< Used to check parameter validity, parameter size must be less than this value
} IPC_ISP_ANTIFLICKER_TYPE;

/** @brief Video control command definitions */
typedef enum {
    IPC_VIDEO_CTRL_CMD_QOS_ADJUST, ///< Bitrate adjustment command, parameters use #struct ipc_bitrate_adjust*

    IPC_VIDEO_CTRL_CMD_MIRRORFILP, ///< Mirror and flip command; chn
                                  ///< Use 0, flipping affects globally; parameters use
                                  ///< #IPC_ISP_MIRRORFLIP_TYPE

    IPC_VIDEO_CTRL_CMD_GET_AE_EXP_STATUS, ///< Get exposure time and color gain data parameters use
                                         ///< #struct ipc_plat_isp_exp_status*

    IPC_VIDEO_CTRL_CMD_GET_SENSOR_METERING_THRESHOLD_VAL, ///< Get sensor metering day/night mode switching threshold
                                                         ///< parameters use #struct
                                                         ///< ipc_plat_isp_sensor_metering_threshold_val*

    IPC_VIDEO_CTRL_CMD_SET_ANTI_FLICKER_MODE, ///< Set anti-flicker mode, parameters use
                                             ///< #IPC_ISP_ANTIFLICKER_TYPE

    IPC_VIDEO_CTRL_CMD_QR_ENHANCE_BY_ISP_MODE, ///< QR code scanning enhancement, parameters use
                                              ///< #non-pointer s32 1: enable enhancement
                                              ///< 2: disable enhancement

    IPC_VIDEO_CTRL_CMD_GET_PANO_CROP_PARAM, ///< Get current frame cropping parameters #struct
                                           ///< ipc_plat_video_pano_crop*

    IPC_VIDEO_CTRL_CMD_SET_PANO_CROP_PARAM, ///< Set current frame cropping parameters #struct
                                           ///< ipc_plat_video_pano_crop*

    IPC_VIDEO_CTRL_CMD_SET_SHARP_PARAM, ///< Set sharpness for ISP     #u8 pointer sharpness parameter value

    IPC_VIDEO_CTRL_CMD_SET_CONTRAST_PARAM, ///< Set contrast for ISP   #u8 pointer
                                          ///< contrast parameter value

    IPC_VIDEO_CTRL_CMD_SET_BRIGHT_PARAM, ///< Set brightness for ISP     #u8 pointer brightness parameter value

    IPC_VIDEO_CTRL_CMD_GET_AF_METRIC, ///< Get AF focus parameters #struct
                                     ///< ipc_plat_isp_af_metric*

    IPC_VIDEO_CTRL_CMD_WDR_MODE, ///< WDR mode, parameters use #non-pointer s32 1: enable 2: disable

    IPC_VIDEO_CTRL_CMD_CHECK_IMAGE_IS_IN_CHANGING, ///< Check if the frame is changing,, parameters use
                                                  ///< #pointer pu64, if changes occur then the underlying layer updates
                                                  ///< the current millisecond monotonic timestamp, provided to the
                                                  ///< upper layer for delay judgment

    IPC_VIDEO_CTRL_CMD_GET_ISP_CROP, ///< Get current digital zoom parameters
                                    ///< struct ipc_plat_video_isp_crop*

    IPC_VIDEO_CTRL_CMD_SET_ISP_CROP, ///< Set current digital zoom parameters
                                    ///< struct ipc_plat_video_isp_crop*

    IPC_VIDEO_CTRL_CMD_FPS_CTRL, ///< Frame rate control, limited by the underlying initialized frame rate (will not
                                ///< exceed the maximum underlying frame rate) struct ipc_plat_video_fps_ctrl*

    IPC_VIDEP_CTRL_CMD_RESERVE = 1000,
} IPC_VIDEO_CTRL_CMD;

/** @brief OSD format definitions */
typedef enum {
    IPC_VIDEO_OSD_FMT_TEXT, ///< ASCII text
    IPC_VIDEO_OSD_FMT_RGBA, ///< RGBA format image, rgbargba.....(note endianness)
    IPC_VIDEO_OSD_FMT_MAX = 0xff,
} IPC_VIDEO_OSD_FMT_TYPE;

/** @brief Audio device type definitions */
typedef enum {
    IPC_AUDIO_DEV_INPUT,
    IPC_AUDIO_DEV_OUTPUT,
} IPC_AUDIO_DEV;

/** @brief Audio frame encoding type definitions */
typedef enum {
    IPC_AUDIO_ENC_TYPE_PCM   = 0x1 << 0, ///< PCM audio data
    IPC_AUDIO_ENC_TYPE_G711A = 0x1 << 1, ///< G711A encoded audio data
    IPC_AUDIO_ENC_TYPE_G711U = 0x1 << 2, ///< G711U encoded audio data
    IPC_AUDIO_ENC_TYPE_AAC   = 0x1 << 3, ///< AAC encoded audio data
    IPC_AUDIO_ENC_TYPE_MAX   = 0xff,
} IPC_AUDIO_ENC_TYPE;

/** @brief Audio sampling rate descriptions */
typedef enum {
    IPC_AUDIO_SAMPLE_8K  = 0x1 << 0,
    IPC_AUDIO_SAMPLE_11K = 0x1 << 1,
    IPC_AUDIO_SAMPLE_12K = 0x1 << 2,
    IPC_AUDIO_SAMPLE_16K = 0x1 << 3,
    IPC_AUDIO_SAMPLE_MAX = 0xff,
} IPC_AUDIO_SAMPLE_E;

/** @brief Defines audio sample bit-width description */
typedef enum {
    IPC_AUDIO_DATABITS_8   = 0x1 << 0,
    IPC_AUDIO_DATABITS_16  = 0x1 << 1,
    IPC_AUDIO_DATABITS_MAX = 0xFF
} IPC_AUDIO_DATABITS_E;

/** @brief Defines audio channel descriptions */
typedef enum {
    IPC_AUDIO_CHANNEL_MONO  = 0x1 << 0,
    IPC_AUDIO_CHANNEL_STERO = 0x1 << 1,
} IPC_AUDIO_CHANNEL_E;

/** @brief Describes the IOs on the board by function */
typedef enum {
    IPC_IO_NAME_SPEAKER = 0,        ///< Speaker switch
    IPC_IO_NAME_RESET_BUTTON,       ///< Reset button
    IPC_IO_NAME_WHITE_LIGTH,        ///< White light switch
    IPC_IO_NAME_INFRARED_LIGTH,     ///< Infrared light switch
    IPC_IO_NAME_LIGHT_SENSOR,       ///< Light sensor
    IPC_IO_NAME_IRCUT_A,            ///< IR-CUT+, if single-pin IR-CUT controller, should only use IRCUT_A
    IPC_IO_NAME_IRCUT_B,            ///< IR-CUT-
    IPC_IO_NAME_STATUS_INDICATOR_A, ///< Status indicator A
    IPC_IO_NAME_FLOOD_LIGHT,        ///< Security light floodlight, 0 indicates off, 1~100 brightness adjustment
    IPC_IO_NAME_PIR_ALARM,          ///< PIR alarm pointer s32
    IPC_IO_NAME_STATUS_INDICATOR_B, ///< Status indicator B
    IPC_IO_NAME_RGB_LIGHT_RED,      ///< Red RGB light
    IPC_IO_NAME_RGB_LIGHT_GREEN,    ///< Green RGB light
    IPC_IO_NAME_RGB_LIGHT_BLUE,     ///< Blue RGB light
    IPC_IO_NAME_WIRELESS_PWR,       ///< Wireless module (WiFi/4G) switch
    IPC_IO_NAME_FRONT_BUTTON,       ///< Front button responsible for user interaction
    IPC_IO_NAME_ETH0_RST,           ///< Wired network card reset pin
    IPC_IO_NAME_NUM,                ///< Number of IOs
} IPC_IO_NAME;

/** @brief Describes the value attribute of an IO */
typedef enum {
    IPC_IO_VALUE_IS_INACTIVE, ///< Inactive state
    IPC_IO_VALUE_IS_ACTIVE,   ///< Active state
    IPC_IO_VALUE_IS_NUMBER,   ///< Numeric value, e.g., used for ADC
} IPC_IO_VALUE_TYPE;

/** @brief Defines alarm control commands */
typedef enum {
    IPC_PLAT_ALARM_CTRL_CMD_START,                  ///< Start alarm function
    IPC_PLAT_ALARM_CTRL_CMD_STOP,                   ///< Stop alarm function
    IPC_PLAT_ALARM_CTRL_CMD_SET_SENSITIVITY,        ///< Control alarm sensitivity #pointer f32
                                                   ///< 1~100, higher is less sensitive
    IPC_PLAT_ALARM_CTRL_CMD_NOTICE_IMAGE_CHANGING,  ///< Notify that the frame is changing
    IPC_PLAT_ALARM_CTRL_CMD_FRAME_RATE_CTRL_NEEDED, ///< #pointer u32 0 indicates no frame rate control needed, greater
                                                   ///< than 0 indicates the upper layer application needs to call at
                                                   ///< what frame rate
} IPC_PLAT_ALARM_CTRL_CMD;

/** @brief Defines alarm types */
typedef enum {
    IPC_PLAT_ALARM_TYPE_MD         = 1 << 0, ///< Motion detection
    IPC_PLAT_ALARM_TYPE_AI_PEOPLE  = 1 << 1, ///< AI human detection
    IPC_PLAT_ALARM_TYPE_AI_VEHICLE = 1 << 2, ///< AI vehicle detection
    IPC_PLAT_ALARM_TYPE_AI_TRACK   = 1 << 3, ///< AI tracking
} IPC_PLAT_ALARM_TYPE;

/** @brief Defines the alarm result description */
struct ipc_plat_alarm_result_s {
    s16 image_width;      ///< Width of the image used for analysis
    s16 image_height;     ///< Height of the image used for analysis
    s16 alarm_result_num; ///< Number of alarm results
    vptr plat_pri;        ///< Pointer to platform alarm data, hides structure upwards
    struct {
        u16 alarm_type; ///< Alarm type #IPC_PLAT_ALARM_TYPE
        u16 lux;        ///< X coordinate of the top-left corner
        u16 luy;        ///< Y coordinate of the top-left corner
        u16 rdx;        ///< X coordinate of the bottom-right corner
        u16 rdy;        ///< Y coordinate of the bottom-right corner
    } rect[32];
};

typedef enum {
    IPC_AUDIO_CTRL_CMD_SET_AEC = 0, ///< Set AEC (Acoustic Echo Cancellation)
    IPC_AUDIO_CTRL_CMD_RESERVE,
} IPC_AUDIO_CTRL_CMD;

typedef enum {
    IPC_VIDEO_FPS_CHANGE, ///< Change frame rate
    IPC_VIDOE_FPS_GET,    ///< Get frame rate
    IPC_VIDEO_FPS_RESUME, ///< Resume frame rate
} IPC_VIDEO_FPS_CTRL_E;

/** @brief Describes the inversion of an IO's active level */
struct ipc_io_active_level_flip {
    IPC_IO_NAME name; ///< IO name
    s32 is_flip;     ///< 1 for inverted, 0 for default configuration
};

/** @brief Defines the structure for sub-packets within a data frame */
struct ipc_frame_pack_s {
    vptr data;    ///< Pointer to packet data
    s32 data_len; ///< Length of packet data
};

/** @brief Defines the data frame structure */
struct ipc_frame_data_s {
    struct ipc_frame_pack_s pack[10];
    s32 pack_num;  ///< Number of packs remaining after each iteration of ipc_plat_iter_f
    s64 timestamp; ///< Timestamp in milliseconds
    s32 is_key;    ///< 1, key frame; 0, non-key frame
};

/**
 * @brief Iterates through packs in a frame node.
 *
 * @param frame Node obtained from video_recv_frame or audio_ai_recv_frame.
 */
typedef void (*ipc_plat_recv_frame_cb_f)(struct ipc_frame_data_s* frame, vptr _user);

struct ipc_video_osd_attr_s {
    struct {
        struct {
            u8 enable;                      ///< Whether to enable this region.
            IPC_VIDEO_OSD_FMT_TYPE fmt_type; ///< Format type of OSD overlay.
            s32 x;                          ///< Horizontal coordinate of top-left corner.
            s32 y;                          ///< Vertical coordinate of top-left corner.
            s32 width;                      ///< Horizontal width of the region.
            s32 height;                     ///< Vertical height of the region.
        } rgn[5];                           ///< Description of OSD regions.
    } chn[2];                               ///< Channel OSD description.
};

/** @brief Defines video resolution. */
struct ipc_resolution {
    s32 height; ///< Video height.
    s32 width;  ///< Video width.
};

/** @brief Bitrate adjustment structure. */
struct ipc_bitrate_adjust {
    s32 net_type;       ///< 0, LAN; 1, WAN.
    s32 expect_bitrate; ///< Expected bitrate size.
    s32 level;          ///< 0 high, 1 medium, 2 low.
    s32 fps;            ///< Frame rate.
};

/** @brief Platform video capability description. */
struct ipc_plat_video_capability {
    IPC_VIDEO_ENC_TYPE video_enc_support; ///< Supported encoding types.
    s32 channel_number;                  ///< Number of channels.
    struct ipc_resolution res[5];         ///< Resolution per channel.
    IPC_VIDEO_ENC_TYPE type[5];           ///< Encoding type per channel.
    v8 sensor_name[16];                  ///< Sensor name, string name, e.g., gc2053.
    u8 reserve[12];                      ///< Reserved.
};

/** @brief Platform audio capability description. */
struct ipc_plat_audio_capability {
    IPC_AUDIO_ENC_TYPE audio_enc_support; ///< Supported encoding types.
    IPC_AUDIO_CHANNEL_E channel;          ///< Audio channel support.
    IPC_AUDIO_DATABITS_E databits;        ///< Audio sample bit-width support.
    IPC_AUDIO_SAMPLE_E sample_support;    ///< Sampling rate support.
    u8 is_support_aec;                   ///< Whether AEC noise reduction feature is supported.
    s32 default_ai_vol;                  ///< Default audio input volume, 0~100.
    s32 default_ao_vol;                  ///< Default audio output volume, 0~100.
    s32 default_ai_gain;                 ///< Default audio input gain, 0~30.
    s32 default_ao_gain;                 ///< Default audio output gain, 0~30.
};

/** @brief Platform audio initialization attribute description. */
struct ipc_plat_audio_init_attr {
    IPC_AUDIO_ENC_TYPE audio_enc;  ///< Encoding type.
    IPC_AUDIO_CHANNEL_E channel;   ///< Audio channel.
    IPC_AUDIO_DATABITS_E databits; ///< Audio sample bit-width.
    IPC_AUDIO_SAMPLE_E sample;     ///< Sampling rate.
    s32 frame_rate;               ///< Audio frame rate.
    s32 ai_vol;                   ///< Audio input volume, 0~100.
    s32 ao_vol;                   ///< Audio output volume, 0~100.
    u8 enable_aec;                ///< Whether to enable AEC noise reduction feature.
    u8 enable_ai;                 ///< Enable recording support.
    u8 enable_ao;                 ///< Enable playback support.
    s32 ai_gain;                  ///< Audio input gain, 0~30.
    s32 ao_gain;                  ///< Audio output gain, 0~30.
};

/** @brief Describes platform exposure data. */
struct ipc_plat_isp_exp_status {
    u32 ev;                 ///< Exposure value, ISO.
    u32 wb_statis_r_g_diff; ///< White balance statistical r - g.
    u32 wb_statis_b_g_diff; ///< White balance statistical b - g
};

/**
 * @brief Describes the platform layer sensor metering threshold values, which may vary for each sensor.
 */
struct ipc_plat_isp_sensor_metering_threshold_val {
    u32 day_to_night_exp_val; // Exposure value
    u32 night_to_day_exp_val;
    u32 night_to_day_wb_r_g_diff;
    u32 night_to_day_wb_b_g_diff;
};

/**
 * @brief Describes panoramic view cropping parameters. For platforms where sub-streams need independent settings, crop
 * parameters should be calculated according to the resolution ratio.
 */
struct ipc_plat_video_pano_crop {
    u32 max_x_position; ///< Maximum supported starting X-axis coordinate for cropping, i.e., the difference between
                        ///< sensor width and height
    u32 cur_x_position; ///< Current starting X-axis coordinate
};

/**
 * @brief Describes AF (Auto Focus) metric values and AE (Auto Exposure) stability state.
 */
struct ipc_plat_isp_af_metric {
    u32 af_metric;   ///< AF metric
    u32 sensor_fps;  ///< Sensor frame rate
    u8 ae_is_stable; ///< Whether AE is converging
};

/**
 * @brief Describes the cropping parameters for current digital zoom.
 */
struct ipc_plat_video_isp_crop {
    u32 max_width;      ///< Maximum width
    u32 min_width;      ///< Minimum width
    u32 cur_width;      ///< Current width
    u32 max_height;     ///< Maximum height
    u32 min_height;     ///< Minimum height
    u32 cur_height;     ///< Current height
    u32 cur_x_position; ///< Current horizontal starting coordinate
    u32 cur_y_position; ///< Current vertical starting coordinate
    f32 cur_multiplier; ///< Current zoom factor
};

struct ipc_plat_video_fps_ctrl {
    IPC_VIDEO_FPS_CTRL_E type; ///< Frame rate control type
    s32 fps;                  ///< Frame rate to change to; ignored when type is IPC_VIDEO_FPS_RESUME
};

/**
 * @brief Defines the description for IPC_IO_NAME_LIGHT_SENSOR IO ADC.
 * The default ADC value should indicate that lower values correspond to darker conditions.
 */
struct ipc_plat_io_env_adc_info_s {
    s32 adc_max_value;             ///< Maximum ADC value, actually the maximum value under the ADC reference voltage
    s32 adc_change_to_day_value;   ///< ADC value to switch from night vision to daytime
    s32 adc_change_to_night_value; ///< ADC value to switch from daytime to night vision
};

struct ipc_api_s {
    s32 apiver;  ///< Specifies the API version for the platform layer.
    pv8 platver; ///< Overall version number for the platform layer.

    /**
     * @brief Initializes resources at the platform system layer.
     *
     * @param type Product type.
     * @param arg Pointer to return the chip platform type.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*sys_init)(IPC_PRODUCT_TYPE type);

    /**
     * @brief Deinitializes resources at the platform system layer.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*sys_uninit)(void);

    /**
     * @brief Initializes video resources.
     *
     * @param arg Reserved parameter, set to 0.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_init)(s32 arg);

    /**
     * @brief Deinitializes video resources.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_uninit)(void);

    /**
     * @brief Queries the platform's video capabilities. This can be used without first calling video_init, but should
     * be called after sys_init.
     *
     * @param cap Structure describing the platform video capabilities, see struct ipc_plat_video_capability.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_query_capability)(struct ipc_plat_video_capability* cap);

    /**
     * @brief Starts streaming encoded data from a specified channel.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE.
     * @param arg Reserved parameter.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_start)(s32 channel, vptr arg);

    /**
     * @brief Retrieves a frame data node for a specified channel.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE.
     * @param cb recv Frame callback.
     * @param __user
     * @param ms Timeout for receiving a frame in milliseconds.
     * @return null if failed or timed out, otherwise returns the ipc_plat_iter_f function.
     */
    s32 (*video_recv_frame)(s32 channel, ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms);
    /**
     * @brief Stops the output stream for a specified channel.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_stop)(s32 channel);

    /**
     * @brief Requests a key frame for a specified channel.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_request_key_frame)(s32 channel);

    /**
     * @brief Controls video parameters for a specified channel.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE.
     * @param cmd Control command.
     * @param arg Pointer to the command argument.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_ctrl)(s32 channel, IPC_VIDEO_CTRL_CMD cmd, vptr arg);

    /**
     * @brief Controls the switching between night vision and daytime modes for video.
     *
     * @param mode Image mode, see IPC_VIDEO_MODE.
     * @param arg Reserved parameter.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_isp_image_mode_set)(IPC_VIDEO_MODE mode, vptr arg);

    /**
     * @brief Initializes video OSD (On-Screen Display) resources.
     *
     * @param attr Initial OSD attributes.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_osd_init)(struct ipc_video_osd_attr_s* attr);

    /**
     * @brief Deinitializes video OSD resources.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_osd_uninit)(void);
    /**
     * @brief Sets the properties of an OSD region.
     *
     * @param channel Video channel number, see IPC_VIDEO_CHN_TYPE. Only supports IPC_VIDEO_CHN_MAIN and IPC_VIDEO_CHN_SUB.
     * @param rgn_num Region number of the OSD, each channel has its own independent numbering.
     * @param is_show Whether this region should be displayed.
     * @param data Pointer to image data or character data.
     * @param data_len Length of the data.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_osd_set)(s32 channel, s32 rgn_num, s32 is_show, void* data, s32 data_len);

    /**
     * @brief Sets the bounding box for an OSD area.
     *
     * @param index Box number.
     * @param lux Horizontal coordinate of the top-left corner.
     * @param luy Vertical coordinate of the top-left corner.
     * @param rdx Horizontal coordinate of the bottom-right corner.
     * @param rdy Vertical coordinate of the bottom-right corner.
     * @note The maximum number of boxes should match the number of boxes that the alarm module can output at the low
     * level. No restrictions are imposed at the higher level. The low-level implementation should clear the boxes after
     * a certain period if no boxes are to be drawn. All parameters should be set to 0 when there are no alarm results,
     * maintaining one call per alarm result.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_osd_show_rect)(s32 index, s32 lux, s32 luy, s32 rdx, s32 rdy);

    /**
     * @brief Initializes audio resources. This is handled by the low-power base layer and should not be called by upper
     * layers.
     *
     * @param attr Attributes for audio initialization.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_init)(struct ipc_plat_audio_init_attr* attr);

    /**
     * @brief Deinitializes audio resources. This is handled by the low-power base layer and should not be called by
     * upper layers.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_uninit)(void);

    /**
     * @brief Queries the audio capabilities of the platform.
     *
     * @param cap Description of the platform's audio capabilities.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_query_capability)(struct ipc_plat_audio_capability* cap);

    /**
     * @brief Starts audio output or sending stream.
     *
     * @param enable_ai Set to 1 to start audio recording. Some platforms do not support separate initialization of AI
     * (Audio Input) and AO (Audio Output), so this parameter might be ineffective, and calling this function will start
     * all audio devices.
     * @param enable_ao Set to 1 to start audio playback. Some platforms do not support separate initialization of AI
     * and AO, so this parameter might be ineffective, and calling this function will start all audio devices.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_start)(s32 enable_ai, s32 enable_ao);

    /**
     * @brief Retrieves a node for a frame of audio data.
     *
     * @param fnode Frame node structure, see struct ipc_frame_node_s.
     * @param ms Timeout for receiving a frame in milliseconds.
     * @return null if failed or timed out, otherwise returns the ipc_plat_iter_f function.
     */
    s32 (*audio_ai_recv_frame)(ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms);

    /**
     * @brief Plays a frame of audio data.
     *
     * @param fpack Frame package structure, see struct ipc_frame_pack_s.
     * @param ms Timeout for sending the frame in milliseconds.
     * @return 0 for timeout, 1 for success, any other value indicates failure.
     */
    s32 (*audio_ao_send_frame)(struct ipc_frame_pack_s* fpack, s32 ms);

    /**
     * @brief Closes audio output or playback support. This is handled by the low-power base layer and should not be
     * called by upper layers.
     *
     * @param disable_ai Set to 1 to disable audio input.
     * @param disable_ao Set to 1 to disable audio output.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_stop)(s32 disable_ai, s32 disable_ao);

    /**
     * @brief Waits for the last frame of audio data to finish playing.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_ao_flush_buffer)(void);

    /**
     * @brief Sets the volume for an audio device.
     *
     * @param dev Audio device type, see IPC_AUDIO_DEV.
     * @param gain Volume level: 0~30.
     * @param vol Volume percentage: 0~100.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_set_vol)(IPC_AUDIO_DEV dev, s32 gain, s32 vol);

    /**
     * @brief Initializes IO resources.
     *
     * @param flip_table Table for IO inversion levels.
     * @param num Number of inverted IOs.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*io_init)(struct ipc_io_active_level_flip* flip_table, int num);

    /**
     * @brief Deinitializes IO resources.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*io_uninit)(void);

    /**
     * @brief Reads the value of an IO.
     *
     * @param name Selects the IO to control.
     * @param type Property of the IO value being read (high, low, ADC).
     * @return Less than 0 for failure, 0 for success, greater than 0 for ADC value.
     */
    s32 (*io_read)(IPC_IO_NAME name, IPC_IO_VALUE_TYPE* type);

    /**
     * @brief Writes a value to an IO.
     *
     * @param name Selects the IO to control.
     * @param type Property of the IO being set (high, low, does not support writing ADC analog values).
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*io_write)(IPC_IO_NAME name, IPC_IO_VALUE_TYPE type);

    /**
     * @brief Initializes resources for the alarm module.
     *
     * @param support_alarm_type Obtains the alarm types supported by the platform.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*alarm_init)(IPC_PLAT_ALARM_TYPE* support_alarm_type);
    /**
     * @brief Deinitializes resources for the alarm module.
     *
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*alarm_uninit)(void);

    /**
     * @brief Receives and processes alarm control commands.
     *
     * @param cmd Alarm control command.
     * @param arg Control argument.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*alarm_ctrl)(IPC_PLAT_ALARM_CTRL_CMD cmd, vptr arg);

    /**
     * @brief Receives alarm results.
     *
     * @param result Pointer to the alarm result structure.
     * @param timeout_ms Timeout in milliseconds for waiting for the result.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*alarm_recv_result)(struct ipc_plat_alarm_result_s* result, s32 timeout_ms);

    /**
     * @brief Releases the resources associated with an alarm result.
     *
     * @param result Pointer to the alarm result structure.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*alarm_release_result)(struct ipc_plat_alarm_result_s* result);

    /**
     * @brief Miscellaneous control for abstracting special controls and information queries not handled by other module
     * APIs.
     *
     * @param cmd Control command requested.
     * @param req Pointer to the request parameters.
     * @param rsp Pointer to the response/result.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*misc_ctrl)(IPC_PLAT_MISC_CTRL_CMD cmd, vptr req, vptr rsp);
    /**
     * @brief Sets the OSD privacy zone (maximum of 5 zones).
     *
     * @param index Zone number of the privacy area.
     * @param lux Horizontal coordinate of the top-left corner.
     * @param luy Vertical coordinate of the top-left corner.
     * @param rdx Horizontal coordinate of the bottom-right corner.
     * @param rdy Vertical coordinate of the bottom-right corner.
     * @param color Color of the privacy area.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*video_osd_show_cover)(s32 index, s32 lux, s32 luy, s32 rdx, s32 rdy, u32 color);

    /**
     * @brief Gets the unique chip ID, assembled by the chip model + the unique ID provided by the underlying interface.
     *
     */
    pv8 (*sys_get_chip_id)(void);

    /**
     * @brief Controls audio parameters.
     *
     * @param cmd Control command.
     * @param arg Pointer to the command argument.
     * @return 0 for success, any other value indicates failure.
     */
    s32 (*audio_ctrl)(IPC_AUDIO_CTRL_CMD cmd, vptr arg);
    vptr reserve[48];
};

/**
 * @brief ipc_plat_api
 *
 * @param arg Argument passed to the function.
 * @return Returns a collection of platform control APIs.
 */
EXAPI struct ipc_api_s* ipc_plat_api(s32 arg);

#ifdef __cplusplus
}
#endif

#endif
