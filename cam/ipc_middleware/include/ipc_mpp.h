#ifndef __IPC_MPP_H__
#define __IPC_MPP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_core.h>
#include <ipc_platform_api.h>
#include "ipc_middleware_config.h"

typedef struct {
    s32 vad_det_number; // Default start of vad algorithm >=0 returns the number of frames detected, internal algorithm detects every 100 frames. If initialized with 25 frames, the maximum is 100/25.
    f32 sound_frame_max_db; // Maximum volume db value of the current audio frame
} ipc_mpp_ai_extinfo_t, *ipc_mpp_ai_extinfo_p;

/**
 * @brief Real-time timestamp callback
 * @return Real-time timestamp (including daylight savings and timezone adjustments)
 */
typedef u32 (*ipc_mpp_get_realts_f)(void);

/**
 * @brief Video stream output callback
 * 
 * @param chn Stream channel, only IPC_VIDEO_CHN_MAIN and IPC_VIDEO_CHN_SUB will appear
 * @param frame Frame data, see ipc_platform_api.h for details
 * @param fnode Iterator handle for the stream, see ipc_platform_api.h for details
 */
typedef void (*ipc_mpp_push_video_f)(IPC_VIDEO_CHN_TYPE chn, struct ipc_frame_data_s* frame);

/**
 * @brief Audio stream output callback
 * 
 * @param data Audio data buffer
 * @param len Length of audio data
 * @param exinfo Extended information about the current audio frame
 */
typedef void (*ipc_mpp_push_audio_f)(vptr data, s32 len, ipc_mpp_ai_extinfo_p exinfo);

/**
 * @brief Notification callback after voice announcement playback has finished
 * 
 * @param path Full path of the played file
 */
typedef void (*ipc_mpp_play_finish_f)(pv8 path);

typedef struct {
    ipc_mpp_push_video_f f_push_video;
    ipc_mpp_push_audio_f f_push_audio;
    ipc_mpp_get_realts_f f_realts;
    ipc_mpp_play_finish_f f_play_finish;
} ipc_mpp_cb_t, *ipc_mpp_cb_p;

/**
 * @brief Initializes resources for the streaming media/board I/O related functional module
 * 
 * @param is_flip Initial flip state
 * @param f_push_video Video stream callback
 * @param f_push_audio Audio stream callback
 * @param f_realts Real-time timestamp callback (if NULL, default ipc_real_ts is used)
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_mpp_init(ipc_mpp_cb_p mpp_cb);

/**
 * @brief Destroys resources for the streaming media/board I/O related functional module
 * 
 * @param is_wait 0: Notify internal threads to exit (do not wait, resources are not completely destroyed), 1: Wait for internal threads to fully exit and destroy all resources
 * (Note: First use is_wait = 0 to notify all modules to exit, then use is_wait = 1 to wait for resource recovery, which can achieve concurrent exit of each module)
 */
EXAPI void ipc_mpp_uninit(s32 is_wait);

/**
 * @brief Switches the logo watermark display
 * 
 * @param sw 0: Off; other: On
 */
EXAPI void ipc_mpp_osd_logo_switch(u8 sw);

/**
 * @brief Switches the time watermark display
 * 
 * @param sw 0: Off; other: On
 */
EXAPI void ipc_mpp_osd_time_switch(u8 sw);

#define IPC_MPP_OSD_TIME_MAX_SIZE 32
/**
 * @brief Platform dynamically switches OSD time format
 * 
 * @param format OSD time format, follows ipc_ts2str/strftime format
 * @note The relevant description of the target character (pseudo-font) must exist within /app/osd/osd.json
 */
EXAPI void ipc_mpp_osd_time_format(v8 format[IPC_MPP_OSD_TIME_MAX_SIZE]);

/**
 * @brief Restarts the OSD thread and resources
 * 
 */
EXAPI s32 ipc_mpp_osd_uninit(void);
EXAPI s32 ipc_mpp_osd_init(void);

#ifdef IPC_MPP_OSD_DYN_FONT

typedef struct {
    u32 unicode[50 * 5]; ///< Unicode codes to be rendered
    u32 unicode_cnt;     ///< Number of unicode codes
    f32 panding_x_pct;   ///< >=0: Distance from the left side of the image <0: Distance from the right side of the image (in percentage of the image size, same as /app/osd/osd.json::x_pct)
    f32 panding_y_pct;   ///< >=0: Distance from the top of the image <0: Distance from the bottom of the image (in percentage of the image size, same as /app/osd/osd.json::y_pct)
    f32 font_size_pct;   ///< Font size (in percentage of the image height)
    // Additional attributes: letter spacing, line spacing, font color, background color, font outline ===
} ipc_osd_dyn_font_attr_t, *ipc_osd_dyn_font_attr_p;

/**
 * @brief Switches the dynamic font display
 * 
 * @param sw 0: Off; other: On
 */
EXAPI void ipc_mpp_osd_dyn_font_switch(u8 sw);

/**
 * @brief Sets the properties of the dynamic font
 * 
 * @param ttf_path Path to the TTF font file, if NULL, uses the default /app/font/default.ttf
 * @param attr See ipc_osd_dyn_font_attr_t for details
 * @return <0: ipc_std standard return value
 * @note Unstable API !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @note Unstable API !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @note Unstable API !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */
EXAPI s32 ipc_mpp_osd_dyn_font_attr(pcv8 ttf_path, ipc_osd_dyn_font_attr_p attr);

#endif

/**
 * @brief Screenshot function. For more detailed screenshot channel information, use the ipc_platform_api's video_query_capability API.
 * 
 * @param mem_or_path Memory address or storage path for exporting the screenshot
 * @param mem_size If mem_size > 0, mem_or_path is treated as a memory address; otherwise, it is treated as a storage path
 * @return <0: ipc_std error code; >0: Image size
 */
EXAPI s32 ipc_mpp_snapshot(pv8 mem_or_path, s32 mem_size);

/**
 * @brief Forces an I-frame (keyframe)
 * 
 * @param chn Channel to force the keyframe on
 * @note To prevent frequent forced I-frames, there is an internal enforced interval.
 */
EXAPI void ipc_mpp_force_iframe(IPC_VIDEO_CHN_TYPE chn);

/**
 * @brief Flips the image
 * 
 * @param is_flip 0: Restore normal orientation 1: Flip orientation
 */
EXAPI void ipc_mpp_image_flip(u8 is_flip);

/**
 * @brief Sets the microphone volume
 * 
 * @param vol Sets the microphone volume percentage (<0: no change)
 * @param gain Sets the microphone volume gain (<0: no change)
 */
EXAPI void ipc_mpp_set_mic_volume(s32 vol, s32 gain);

/**
 * @brief Gets the microphone volume
 * 
 * @param vol Retrieves the microphone volume (if null, this value is not retrieved)
 * @param gain Retrieves the microphone gain (if null, this value is not retrieved)
 */
EXAPI void ipc_mpp_get_mic_volume(ps32 vol, ps32 gain);

/**
 * @brief Sets the speaker volume
 * 
 * @param vol Sets the default speaker volume (<0: no change)
 * @param gain Sets the default speaker gain (<0: no change)
 */
EXAPI void ipc_mpp_set_speaker_volume(s32 vol, s32 gain);

/**
 * @brief Gets the speaker volume
 * 
 * @param vol Retrieves the default speaker volume (if null, this value is not retrieved)
 * @param gain Retrieves the default speaker gain (if null, this value is not retrieved)
 */
EXAPI void ipc_mpp_get_speaker_volume(ps32 vol, ps32 gain);

/**
 * @brief Sets the voice announcement switch (default is on)
 * 
 * @param sw 1: On 0: Off
 * @note After turning off voice announcements, the playback schedule still executes but does not produce sound.
 */
EXAPI void ipc_mpp_play_switch(u8 sw);

typedef struct {
    s8  vol;          ///< Playback volume percentage, if -1, uses the default volume
    s8  gain;         ///< Playback volume gain, if -1, uses the default gain value
    u16 ratio;        ///< Playback volume ratio (%0 - %), based on the playback vol and gain
    u32 multiplier;   ///< Set the multiplier times as one <play unit>
    u32 interval_ts;  ///< Interval before the next <play unit> after one has completed
    u32 play_times;   ///< Set the number of <play units> to play before ending, mutually exclusive with duration_ts, at least one of the parameters must exist. If neither exists, it interrupts the specified playback task.
    u32 duration_ts;  ///< Set the playback duration
} ipc_mpp_play_t, *ipc_mpp_play_p;

/**
 * @brief Voice announcement interface
 *
 * @param name Name of the voice announcement /app/[sound|music]/$(name).[wav|g711u|g711a|pcm]. If the name starts with '/', it is treated as an
 * absolute path for file lookup.
 * @param block Whether to block until playback is complete
 * @param play Detailed configuration for voice announcement. If NULL, plays once at default volume.
 * @return ipc_std.h standard return value
 * @note Now supports variadic argument calls. By default, block = 0, play = NULL.
 */
EXAPI s32 ipc_mpp_play_voice(pv8 name, u8 block, ipc_mpp_play_p play);
#define __PLAY_ARG2(arg2) (vptr)(word)(#arg2[0] ? arg2 : 0)
#define __PLAY_ARG1(arg1, ...) #arg1[0] ? arg1 : 0, __PLAY_ARG2(__VA_ARGS__)
#define ipc_mpp_play(name, ...) ipc_mpp_play_voice(name, __PLAY_ARG1(__VA_ARGS__))

typedef enum {
    IPC_MPP_PLAY_CTRL_RESUME, ///< Resume playback
    IPC_MPP_PLAY_CTRL_PAUSE,  ///< Pause playback
    IPC_MPP_PLAY_CTRL_STOP,   ///< Stop playback -> Equivalent to clearing play_times and duration_ts in ipc_mpp_play
    IPC_MPP_PLAY_CTRL_NUM,
} ipc_mpp_play_ctrl_e;

/**
 * @brief Voice playback control
 *
 * @param name Name of the voice announcement /app/[sound|music]/$(name).[wav|g711u|g711a|pcm]. If the name starts with '/', it is treated as an
 * absolute path for file lookup. If name is NULL or "", controls all voices.
 * @param block Whether to block until playback is complete, only valid when name is not empty.
 * @param ctrl Control command
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_mpp_play_ctrl(pv8 name, u8 block, ipc_mpp_play_ctrl_e ctrl);

/**
 * @brief Sets the intercom switch (default is on)
 *
 * @param sw 1: On 0: Off
 * @note Note that this switch is used to mute ipc_mpp_speak announcements, not to enable functionality.
 */
EXAPI void ipc_mpp_speak_switch(u8 sw);

/**
 * @brief Dynamically changes the language of voice announcements
 *
 * @param language /app/sound/$(language)/speech. If NULL, uses the default language.
 * @note This differs from ipc_factory(language) in that ipc_factory(language) sets the factory default language, while ipc_mpp_play_language sets the
 * platform dynamically issued language.
 */
EXAPI void ipc_mpp_play_language(pv8 language);

/**
 * @brief Intercom (voice) playback. Must call ipc_mpp_ctrl_speaker before and after usage.
 *
 * @param data Audio data. Specific playback parameters are obtained from ipc_middleware_sal_media_info.
 * @param len Length of data
 * @param enc Encoding of the playback data
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_mpp_speak(vptr data, s32 len, IPC_AUDIO_ENC_TYPE enc);

/**
 * @brief VAD (Voice Activity Detection) algorithm sensitivity
 *
 * @param sensitivity 0~3, higher values are less sensitive
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_mpp_vad_sensitivity(s32 sensitivity);

/**
 * @brief QR enhancement control
 *
 * @param enable Enable
 * @return ipc_std.h standard return value
 */
EXAPI s32 ipc_mpp_qr_enhancement(u32 enable);

#ifdef __cplusplus
}
#endif

#endif //__IPC_MPP_H__
