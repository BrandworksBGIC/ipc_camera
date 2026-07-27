#include <time.h>
#include <unistd.h>

#include "ipc_decrypt.h"
#include "ipc_factory.h"
#include "ipc_mpp.h"
#include "ipc_ptz.h"
#include "ipc_sal_api.h"
#include "ipc_thread.h"
#include "ipc_time.h"
#include "ipc_type_trans.h"
#include "ipc_voice_file.h"

#include "ipc_vad.h"



enum { MPP_HD, MPP_SD, MPP_AUDIO, MPP_OSD, MPP_SPEAK, MPP_JPEG, MPP_NUM };

static struct {
    u8 gorun;
    u8 osd_run;
    u8 alive[MPP_NUM];
    u8 time_sw;
    u8 logo_sw;
    u8 mic_vol;
    u8 mic_gain;
    u8 mic_codec;
    u8 io_inited;                               // Indicates whether the IO module has been initialized
    char time_format[IPC_MPP_OSD_TIME_MAX_SIZE]; ///< OSD time formatting
    void* snapshot_mem_or_path;
    s32 snapshot_mem_size;
    s32 snapshot_return;
    ipc_lock_t snapshot_lock;
    ipc_cond_t snapshot_cond;
    void* h_ai_vad; ///< JPEG ring buffer
    ipc_mpp_get_realts_f f_realts;
    ipc_mpp_push_video_f f_push_video;
    ipc_mpp_push_audio_f f_push_audio;
    ipc_mpp_play_finish_f f_play_finish;
    u8 video_channel_started[2]; // 0: MAIN, 1: SUB - track manual video start/stop in non-thread mode
    s32 video_wdg_fd[2];         // 0: MAIN, 1: SUB - watchdog file descriptors for video channels
    s32 audio_wdg_fd;            // Audio watchdog file descriptor

    u8 qr_enable;
} _gh_mpp = {
    .f_realts      = ipc_real_ts, // Default use internal system time
    .f_push_video  = NOT_DO_ANYTHING,
    .f_push_audio  = NOT_DO_ANYTHING,
    .f_play_finish = NOT_DO_ANYTHING,
    .video_wdg_fd  = {-1, -1},    // Initialize video watchdog file descriptors
    .audio_wdg_fd  = -1,          // Initialize audio watchdog file descriptor
};

/******************************** speak & player ********************************************/

typedef struct {
    u8 used; ///< Occupied flag
    u8 seq;  ///< Sequence number that increments each time this node is allocated, used for blocking checks to determine if `used` belongs to the
             ///< current allocation
    u8 type; ///< File type
    char path[60];   ///< Playback file path
    u8 has_voice;    ///< Whether the file handle has been initialized
    voice_t h_voice; ///< Playback file handle
    /* The parameters above may be concurrently written, the parameters below may be concurrently read and written */
    ipc_mpp_play_ctrl_e play_ctrl; ///< Playback control
    u32 start_ts;                 ///< Monotonic time at the moment it was set -> `.play.duration_ts` calculation
    u32 last_ts;                  ///< Last monotonic time played -> `.play.interval_ts` calculation
    u32 left_cnt;                 ///< Remaining count within the playback unit -> `.play.multiplier` count
    ipc_mpp_play_t play;           ///< Playback parameters set
} play_plan_t, *play_plan_p;

static struct {
    void* ring;          ///< Intercom ring buffer
    ipc_lock_t mutex;     ///< Lock for protecting voice playback related data
    u16 pcm_pack_size;   ///< Length of each PCM audio packet sent to the lower layer
    u8 default_vol;      ///< Default playback volume
    u8 default_gain;     ///< Default playback gain
    u8 now_vol;          ///< Current playback volume
    u8 now_gain;         ///< Current playback gain
    u8 speak_sw;         ///< Intercom switch
    u8 play_sw;          ///< Voice playback switch
    char* language;      ///< Language for playback
    play_plan_t plan[5]; ///< Playback plan
} _gh_speaker = {
    .speak_sw = 1, // Switch default on
    .play_sw  = 1, // Switch default on
};

static void _speaker_onoff(u8 onoff)
{
    static u8 last_onoff = -1;
    if (last_onoff != onoff) {
        last_onoff = onoff;
        ipc_plat_api(0)->io_write(IPC_IO_NAME_SPEAKER, onoff ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE);
        ipcdebug("Speaker %s", onoff ? "on" : "off");
    }
}

static void _set_speaker_vol(s8 vol, s8 gain, u16 ratio)
{
    /* Prevent overflow in calculations with * ratio, so use u32 */
    u32 real_vol  = vol < 0 ? _gh_speaker.default_vol : vol;
    u32 real_gain = gain < 0 ? _gh_speaker.default_gain : gain;

    real_vol  = real_vol * ratio / 100;
    real_gain = real_gain * ratio / 100;
    if (real_vol > 100)
        real_vol = 100;
    if (real_gain > 30)
        real_gain = 30;

    if (_gh_speaker.now_vol != real_vol || _gh_speaker.now_gain != real_gain) {
        ipc_plat_api(0)->audio_ao_flush_buffer(); // Prevent sudden sound changes from causing mismatched sound in the underlying buffer
        _gh_speaker.now_vol  = real_vol;
        _gh_speaker.now_gain = real_gain;
        ipc_plat_api(0)->audio_set_vol(IPC_AUDIO_DEV_OUTPUT, real_gain, real_vol);
        ipcdebug("Speaker change gain=[%d] volume=[%d]", real_gain, real_vol);
    }
}

static s32 _insert_plan(char* path, ipc_mpp_play_p play)
{
    s32 idx = 0;
    s32 max = ARRSIZE(_gh_speaker.plan);

    play_plan_p plan = NULL;

    for (idx = 0; idx < max; idx++) { // Check for duplicates, and if found, overwrite settings
        plan = &_gh_speaker.plan[idx];
        if (!plan->used)
            continue;
        if (!strcmp(plan->path, path)) {
            plan->used     = (plan->used + 1) % 0xff ?: 1; // Increment `used`, equivalent to resetting `seq`
            plan->start_ts = ipc_mono_ts();
            plan->last_ts  = 0;
            plan->left_cnt = play->multiplier;
            plan->play     = *play;
            ipcinfo("Reset voice: %s", path);
            return idx;
        }
    }

    if (!play->play_times && !play->duration_ts) {
        ipcdebug("No need to stop: %s", path);
        return IPC_NOT_FOUND;
    }

    for (idx = 0; idx < max; idx++) { // Find an idle slot
        if (!_gh_speaker.plan[idx].used)
            break;
    }
    if (idx >= max) {
        ipcerror("Voice plan full!!! %d >= %d", idx, max);
        return IPC_NOBUF;
    }

    plan   = &_gh_speaker.plan[idx];
    u8 seq = plan->seq + 1; // Increment `seq`

    memset(plan, 0, sizeof(*plan));

    if (strstr(path, ".wav")) {
        plan->type = IPC_VOICE_WAV;
    } else if (strstr(path, ".pcm")) {
        plan->type = IPC_VOICE_PCM;
    } else if (strstr(path, ".g711u")) {
        plan->type = IPC_VOICE_G711U;
    } else if (strstr(path, ".g711a")) {
        plan->type = IPC_VOICE_G711A;
    } else {
        ipcerror("Unknown voice file: %s", path);
        return IPC_NOT_SUPPORT;
    }

    snprintf(plan->path, sizeof(plan->path), "%s", path);
    plan->start_ts = ipc_mono_ts();
    plan->left_cnt = play->multiplier;
    plan->play     = *play;
    plan->seq      = seq;
    plan->used     = 1;
    ipcinfo("New voice: %s", path);

    return idx;
}

static u8 _check_need_free(play_plan_p plan, u32 now_ts)
{
    plan->left_cnt--;
    if (plan->left_cnt == 0) {
        if (plan->play.play_times) {
            plan->play.play_times--;
            if (plan->play.play_times == 0) {
                return 1;
            }
        }
        if (plan->play.duration_ts && now_ts >= plan->start_ts + plan->play.duration_ts) {
            return 1;
        }
        plan->left_cnt = plan->play.multiplier;
        plan->last_ts  = now_ts;
    }

    return 0;
}

static s32 _check_need_play(ps32 p_idx, u32 now_ts, pu8 need_free)
{
    *need_free       = 0;
    play_plan_p plan = NULL;

    for (; *p_idx < ARRSIZE(_gh_speaker.plan); (*p_idx)++) {
        plan = &_gh_speaker.plan[*p_idx];
        if (!plan->used)
            continue;

        if (!plan->play.play_times && !plan->play.duration_ts) { // Stopped externally
            *need_free = 1; // After freeing, `plan->used` will be cleared, and next time we enter we'll directly continue
            return IPC_NOT_DATA;
        }
        if (plan->play.duration_ts && now_ts >= plan->start_ts + plan->play.duration_ts) { // Time elapsed
            *need_free = 1; // After freeing, `plan->used` will be cleared, and next time we enter we'll directly continue
            return IPC_NOT_DATA;
        }
        if (plan->play_ctrl == IPC_MPP_PLAY_CTRL_STOP) {
            *need_free = 1; // After freeing, `plan->used` will be cleared, and next time we enter we'll directly continue
            return IPC_NOT_DATA;
        } else if (plan->play_ctrl == IPC_MPP_PLAY_CTRL_PAUSE) {
            continue; // Playback paused, skip to the next track
        }
        if (plan->last_ts && now_ts < plan->last_ts + plan->play.interval_ts)
            continue; // Not yet at the interval

        return IPC_SUCCESS;
    }

    *p_idx = 0;
    return IPC_NOT_NEED;
}

static inline void _clear_plan(play_plan_p plan)
{
    if (plan->has_voice) {
        plan->has_voice = 0;
        ipc_voice_close(&plan->h_voice);
    }

    u8 used = plan->used;
    _gh_mpp.f_play_finish(plan->path);
    if (used != plan->used)
        return; // The voice was reset inside `f_play_finish`

    ipcdebug("Clear voice plan: %s", plan->path);
    plan->used = 0;
}

static void _clear_all_plan(void)
{
    play_plan_p plan = NULL;

    for (s32 idx = 0; idx < ARRSIZE(_gh_speaker.plan); idx++) {
        plan = &_gh_speaker.plan[idx];
        if (!plan->used)
            continue;
        _clear_plan(plan);
    }
}

static s32 _try_play(ps32 p_idx, void* buf, s32 max)
{
    u8 need_free = 0;
    u32 now_ts   = ipc_mono_ts();

RECHECK:
    ipc_lock(_gh_speaker.mutex);
    s32 ret          = _check_need_play(p_idx, now_ts, &need_free);
    play_plan_p plan = &_gh_speaker.plan[*p_idx];
    s8 vol           = plan->play.vol;
    s8 gain          = plan->play.gain;
    u16 ratio        = plan->play.ratio;
    ipc_unlock(_gh_speaker.mutex);
    if (need_free) {
        _clear_plan(plan);
        goto RECHECK;
    }
    if (ret < 0)
        return ret;

    if (!plan->has_voice) {
        ret = ipc_voice_open(&plan->h_voice, plan->path, plan->type);
        if (ret < 0) {
            _clear_plan(plan);
            goto RECHECK;
        }
        plan->has_voice = 1;
    }

    ret = ipc_voice_read(&plan->h_voice, buf, max);
    if (ret < 0) {
        _clear_plan(plan);
        goto RECHECK;
    } else if (ret == 0) {
        ipc_lock(_gh_speaker.mutex);
        need_free = _check_need_free(plan, now_ts);
        ipc_unlock(_gh_speaker.mutex);
        if (need_free) {
            ipc_plat_api(0)->audio_ao_flush_buffer(); // If not blocked here, it could cause `ipc_mpp_play_voice` to exit prematurely
            _clear_plan(plan);
        } else {
            ipc_voice_reset(&plan->h_voice);
        }
        goto RECHECK;
    }

    _set_speaker_vol(vol, gain, ratio);

    return ret;
}

static vptr _pth_speaker(vptr arg)
{
    s32 mpp_chn = (s32)(word)arg;

    s32 cache_len = 0;
    v8 buff[_gh_speaker.pcm_pack_size];
    struct ipc_frame_pack_s pack = { buff, 0 };

    ipc_ring_iter_t iter;
    ipc_ring_cmd_e cmd = IPC_RING_TAIL; // Only fetch the oldest data initially

    s32 ret     = 0;
    u8 has_talk = 0, has_speak = 0;
    u32 no_data_cnt = 0;
    s32 play_idx    = 0;
    _speaker_onoff(0);

    _gh_mpp.alive[mpp_chn] = 1;

    while (_gh_mpp.gorun) {

        ret = ipc_ring_iter_ctrl(_gh_speaker.ring, cmd, iter);
        if (ret == IPC_NOT_FOUND) {
            cmd = IPC_RING_TAIL; // Overwritten, reposition the data
            continue;
        } else if (ret >= 0) { // Success
            has_talk    = 1;
            no_data_cnt = 0;
            _set_speaker_vol(-1, -1, 100); // Use default volume
            _clear_all_plan();             // Clear all voice announcements
            _speaker_onoff(_gh_speaker.speak_sw);
            cmd = IPC_RING_NEXT; // Success, fetch the next data block next time
            while (1) {
                ret = ipc_ring_iter_pop(iter, buff, _gh_speaker.pcm_pack_size, -1);
                if (ret == IPC_NOT_FOUND)
                    cmd = IPC_RING_TAIL;
                if (ret <= 0)
                    break;
                pack.data_len = ret;
                if (_gh_speaker.speak_sw) {
                    ipc_plat_api(0)->audio_ao_send_frame(&pack, 1000);
                }
            }
            continue;
        }

        if (has_talk) { // Still within the 2-second talk authority
            if (no_data_cnt < 20) {
                no_data_cnt++;
                _speaker_onoff(_gh_speaker.speak_sw); // Convenient for quick on/off
                ipc_msleep(100);
                continue;
            } else {
                no_data_cnt = 0;
                has_talk    = 0;
                cache_len   = 0;
                _speaker_onoff(0); // Turn off before flushing buffer
                ipc_plat_api(0)->audio_ao_flush_buffer();
            }
        }

        if (cache_len) { // Send cached voice announcement data first
            pack.data_len = cache_len;
            if (_gh_speaker.play_sw && ipc_plat_api(0)->audio_ao_send_frame(&pack, 0) < 0) {
                ipc_msleep(100);
                continue;
            }
            cache_len = 0;
        }

        while (1) {
            ret = _try_play(&play_idx, buff, _gh_speaker.pcm_pack_size);
            if (ret < 0) {
                if (has_speak) {
                    has_speak = 0;
                    ipc_plat_api(0)->audio_ao_flush_buffer();
                    _speaker_onoff(0);
                }
                break;
            }
            has_speak = 1;
            _speaker_onoff(_gh_speaker.play_sw);
            pack.data_len = ret;
            if (_gh_speaker.play_sw && ipc_plat_api(0)->audio_ao_send_frame(&pack, 0) < 0) {
                cache_len = pack.data_len;
                break;
            }
        }

        ipc_msleep(100);
    }

    _clear_all_plan();
    _speaker_onoff(0);
    _gh_mpp.alive[mpp_chn] = 0;

    return NULL;
}

static s32 _name_to_absolute(pv8 abs_buff, s32 abs_size, pv8 name)
{
    pv8 build_in_suffix[] = { ".wav", ".g711u", ".g711a", ".pcm" }; // Built-in suffixes

    struct {
        pv8 basic;  // Basic built-in path
        pv8 custom; // Custom field
    } build_in_path[] = {
        { .basic = "/app/sound", .custom = _gh_speaker.language ?: ipc_factory(language) }, // Sound custom factory test field corresponds to language
        { .basic = "/app/music", .custom = NULL }, // Music currently does not have a custom factory test switch
    };

    for (s32 path_idx = 0; path_idx < ARRSIZE(build_in_path); path_idx++) {
        for (s32 suffix_idx = 0; suffix_idx < ARRSIZE(build_in_suffix); suffix_idx++) { // Prioritize custom if available
            if (!build_in_path[path_idx].custom || !build_in_path[path_idx].custom[0])
                continue; // This voice is not customized
            snprintf(abs_buff, abs_size, "%s/%s/%s%s", build_in_path[path_idx].basic, build_in_path[path_idx].custom, name,
                     build_in_suffix[suffix_idx]);
            if (!access(abs_buff, F_OK))
                return IPC_SUCCESS;
            ipcdebug("No voice file: %s", abs_buff);
        }
        for (s32 suffix_idx = 0; suffix_idx < ARRSIZE(build_in_suffix); suffix_idx++) {
            snprintf(abs_buff, abs_size, "%s/%s%s", build_in_path[path_idx].basic, name, build_in_suffix[suffix_idx]);
            if (!access(abs_buff, F_OK))
                return IPC_SUCCESS;
            ipcdebug("No voice file: %s", abs_buff);
        }
    }
    ipcwarn("No file with related name:[%s] found", name);
    return IPC_NOT_FOUND;
}

static s32 _mpp_play_ctrl_all(ipc_mpp_play_ctrl_e ctrl)
{
    ipc_lock(_gh_speaker.mutex);
    play_plan_p plan = NULL;
    for (s32 idx = 0; idx < ARRSIZE(_gh_speaker.plan); idx++) {
        plan = &_gh_speaker.plan[idx];
        if (!plan->used)
            continue;
        plan->play_ctrl = ctrl;
        ipcinfo("Ctrl %s: %d", plan->path, ctrl);
    }
    ipc_unlock(_gh_speaker.mutex);

    return IPC_SUCCESS;
}

static s32 _ctrl_plan(pv8 path, ipc_mpp_play_ctrl_e ctrl)
{
    play_plan_p plan = NULL;
    for (s32 idx = 0; idx < ARRSIZE(_gh_speaker.plan); idx++) {
        plan = &_gh_speaker.plan[idx];
        if (!plan->used)
            continue;
        if (!strcmp(plan->path, path)) {
            plan->play_ctrl = ctrl;
            ipcinfo("Ctrl %s: %d", path, ctrl);
            return idx;
        }
    }

    ipcdebug("Not found: %s", path);
    return IPC_NOT_FOUND;
}

s32 ipc_mpp_play_ctrl(pv8 name, u8 block, ipc_mpp_play_ctrl_e ctrl)
{
    // coverity[NO_EFFECT :SUPPRESS]
    if (ctrl < 0 || ctrl >= IPC_MPP_PLAY_CTRL_NUM)
        return IPC_INVALID_ARGS;

    if (!name || !name[0])
        return _mpp_play_ctrl_all(ctrl);

    pv8 path  = name;
    u8 is_abs = path[0] == '/'; // Check if it starts with '/' for absolute path
    v8 abs_path[sizeof(_gh_speaker.plan->path)];
    if (is_abs) {
        if (access(path, F_OK))
            return IPC_NOT_FOUND;
    } else {
        s32 ret = _name_to_absolute(abs_path, sizeof(abs_path), name);
        if (ret < 0)
            return ret;
        path = abs_path;
    }

    ipc_lock(_gh_speaker.mutex);
    s32 idx = _ctrl_plan(path, ctrl);
    u8 seq  = idx >= 0 ? _gh_speaker.plan[idx].seq : 0;
    ipc_unlock(_gh_speaker.mutex);
    if (idx < 0)
        return idx;

    if (block) {
        while (_gh_speaker.plan[idx].used && _gh_speaker.plan[idx].seq == seq) { // If 'used' is instantly occupied, then 'seq' will not match
            ipc_msleep(100);
        }
    }

    return IPC_SUCCESS;
}

s32 ipc_mpp_play_voice(pv8 name, u8 block, ipc_mpp_play_p play)
{
    if (!name || !name[0])
        return IPC_INVALID_ARGS;

    ipc_mpp_play_t default_play = { .vol = -1, .gain = -1, .ratio = 100, .multiplier = 1, .interval_ts = 0, .play_times = 1, .duration_ts = 0 };
    if (!play)
        play = &default_play;

    pv8 path  = name;
    u8 is_abs = path[0] == '/'; // Check if it starts with '/' for absolute path
    v8 abs_path[sizeof(_gh_speaker.plan->path)];
    if (is_abs) {
        if (access(path, F_OK))
            return IPC_NOT_FOUND;
    } else {
        s32 ret = _name_to_absolute(abs_path, sizeof(abs_path), name);
        if (ret < 0)
            return ret;
        path = abs_path;
    }

    if (!play->multiplier)
        play->multiplier = 1; // This parameter must be at least 1

    ipc_lock(_gh_speaker.mutex);
    s32 idx = _insert_plan(path, play);
    u8 seq  = idx >= 0 ? _gh_speaker.plan[idx].seq : 0;
    ipc_unlock(_gh_speaker.mutex);
    if (idx < 0)
        return idx;

    if (block) {
        while (_gh_speaker.plan[idx].used && _gh_speaker.plan[idx].seq == seq) { // If 'used' is instantly occupied, then 'seq' will not match
            ipc_msleep(100);
        }
    }

    return IPC_SUCCESS;
}

void ipc_mpp_play_language(pv8 language)
{
    if (language) {
        static v8 _dyn_language[sizeof(ipc_factory(language))];
        snprintf(_dyn_language, sizeof(_dyn_language), "%s", language);
        _gh_speaker.language = _dyn_language;
    } else {
        _gh_speaker.language = NULL; // default
    }
}

s32 ipc_mpp_speak(vptr data, s32 len, IPC_AUDIO_ENC_TYPE enc)
{
    if (!data || len <= 0)
        return IPC_INVALID_ARGS;

    ipc_ring_block_t block[1];

    if (enc == IPC_AUDIO_ENC_TYPE_PCM) {
        block->data = data;
        block->len  = len;
        return ipc_ring_push(_gh_speaker.ring, block, ARRSIZE(block));
    }

    v8 decode_data[len * 2];
    if (enc == IPC_AUDIO_ENC_TYPE_G711U) {
        len = ipc_g711u_decode(decode_data, data, len);
    } else if (enc == IPC_AUDIO_ENC_TYPE_G711A) {
        len = ipc_g711a_decode(decode_data, data, len);
    } else {
        return IPC_NOT_SUPPORT;
    }

    block->data = decode_data;
    block->len  = len;

    return ipc_ring_push(_gh_speaker.ring, block, ARRSIZE(block));
}

void ipc_mpp_set_speaker_volume(s32 vol, s32 gain)
{
    if (vol >= 0)
        _gh_speaker.default_vol = vol;
    if (gain >= 0)
        _gh_speaker.default_gain = gain;
}

void ipc_mpp_get_speaker_volume(ps32 vol, ps32 gain)
{
    if (vol)
        *vol = _gh_speaker.default_vol;
    if (gain)
        *gain = _gh_speaker.default_gain;
}

void ipc_mpp_speak_switch(u8 sw)
{
    _gh_speaker.speak_sw = !!sw;
}

void ipc_mpp_play_switch(u8 sw)
{
    _gh_speaker.play_sw = !!sw;
}

/**************************************************************************/

static void __plat_recv_video_frame_cb(struct ipc_frame_data_s* frame, vptr _user)
{
    s32 plat_chn = *(ps32)_user;
    ipctrace("Chn_%d pack_num=[%d], %s key frame", plat_chn, frame->pack_num, frame->is_key ? "is" : "not");
    if (!_gh_mpp.qr_enable) {
        _gh_mpp.f_push_video(plat_chn, frame);
    }
}

s32 ipc_mpp_recv_video(IPC_VIDEO_CHN_TYPE chn,
                       void (*callback)(struct ipc_frame_data_s* frame, void* user_data),
                       void* user_data,
                       s32 timeout_ms)
{
    if (!callback || (chn != IPC_VIDEO_CHN_MAIN && chn != IPC_VIDEO_CHN_SUB)) {
        return IPC_INVALID_ARGS;
    }

    struct ipc_api_s* h_plat = ipc_plat_api(0);
    if (!h_plat) {
        return IPC_NOT_INIT;
    }

    // Check if video channel has been started via ipc_mpp_video_start
    s32 idx = chn - IPC_VIDEO_CHN_MAIN;
    if (idx >= 0 && idx < 2) {
        if (!_gh_mpp.video_channel_started[idx]) {
            ipctrace("Video channel %d not started, call ipc_mpp_video_start first", chn);
            return IPC_NOT_INIT; // Channel not started
        }
    }

    // Watchdog feeding logic - separate for each channel
    if (_gh_mpp.video_wdg_fd[idx] < 0) {
        _gh_mpp.video_wdg_fd[idx] = ipc_swdg_reg(1);
    }
    if (_gh_mpp.video_wdg_fd[idx] >= 0) {
        ipc_swdg_feed(_gh_mpp.video_wdg_fd[idx], 10);
    }

    // Call platform API with user callback
    return h_plat->video_recv_frame(chn, callback, user_data, timeout_ms);
}

static vptr _pth_video(vptr arg)
{
    s32 mpp_chn  = (s32)(word)arg;
    s32 plat_chn = mpp_chn == MPP_HD ? IPC_VIDEO_CHN_MAIN : IPC_VIDEO_CHN_SUB;

    ipc_log_p log_father = __IPC_LOG__;
    ipc_log_t __IPC_LOG__ = { { NULL, NULL } };
    clog_init(mpp_chn == MPP_HD ? "hd" : "sd", "Video out stream", log_father);

    struct ipc_api_s* h_plat = ipc_plat_api(0);

    s32 wdg_fd = ipc_swdg_reg(1);
    ipc_swdg_feed(wdg_fd, 10);

    s32 ret = h_plat->video_start(plat_chn, 0);
    if (ret != IPC_SUCCESS) {
        ipc_swdg_unreg(wdg_fd);
        ipcfatal("Plat video chn=[%d] start failed! retcode=[%d]", plat_chn, ret);
        return NULL;
    }

    ipcinfo("Get video stream start=[%d] watchdog fd=[%d]", plat_chn, wdg_fd);
    _gh_mpp.alive[mpp_chn] = 1;

    ipc_swdg_feed(wdg_fd, 10);
    while (_gh_mpp.gorun) {
        ret = h_plat->video_recv_frame(plat_chn, __plat_recv_video_frame_cb, &plat_chn, 1 * 1000);
        if (ret) {
            ipctrace("Chn_%d waiting...", plat_chn);
            ipc_msleep(20);
            continue;
        }
        ipc_swdg_feed(wdg_fd, 10);
    }

    h_plat->video_stop(plat_chn);
    ipc_swdg_unreg(wdg_fd);

    _gh_mpp.alive[mpp_chn] = 0;
    ipcinfo("Get video stream exit=[%d]", plat_chn);

    return NULL;
}

static void _push_audio(pv8 data, s32 len, ipc_mpp_ai_extinfo_p extinfo, ipc_log_p __IPC_LOG__)
{
    if (_gh_mpp.mic_codec == IPC_AUDIO_ENC_TYPE_PCM) {
        ipctrace("PCM len=[%d]", len);
        _gh_mpp.f_push_audio(data, len, extinfo);
        return;
    }

    v8 encode_data[len / 2 + 1];
    if (_gh_mpp.mic_codec == IPC_AUDIO_ENC_TYPE_G711U) {
        len = ipc_g711u_encode(encode_data, data, len);
        ipctrace("G711U len=[%d]", len);
    } else if (_gh_mpp.mic_codec == IPC_AUDIO_ENC_TYPE_G711A) {
        len = ipc_g711a_encode(encode_data, data, len);
        ipctrace("G711A len=[%d]", len);
    } else {
        ipcerror("Not support audio coding!");
        return;
    }

    _gh_mpp.f_push_audio(encode_data, len, extinfo);
}

static f32 _get_pcm_db(s32 mplitude)
{
    return 20 * log10f(mplitude);
}

static f32 _get_sound_frame_max_db(f32 fix_80db, ps16 pcm, s32 data_bytes, ipc_log_p __IPC_LOG__)
{
    s32 max_mplitude = 0;
    s32 sample_count = data_bytes / (s32)sizeof(*pcm);

    for (s32 i = 0; i < sample_count; i++) {
        s32 mplitude = abs(pcm[i]);
        max_mplitude = max_mplitude > mplitude ? max_mplitude : mplitude;
    }

    ipctrace("max_mplitude [%d]", max_mplitude);

    return _get_pcm_db(max_mplitude) + fix_80db;
}

static void __plat_recv_audio_frame_cb(struct ipc_frame_data_s* frame, vptr _user)
{
    ipc_mpp_ai_extinfo_t extinfo = { 0 };
    f32 fix_80db                = *(pf32)_user;

    extinfo.vad_det_number     = ipc_vad_process_ai_frame(_gh_mpp.h_ai_vad, frame->pack[0].data, frame->pack[0].data_len);
    extinfo.sound_frame_max_db = _get_sound_frame_max_db(fix_80db, frame->pack[0].data, frame->pack[0].data_len, __IPC_LOG__);

    _push_audio(frame->pack[0].data, frame->pack[0].data_len, &extinfo, __IPC_LOG__);
}

// Audio wrapper callback structure and function
typedef struct {
    void (*user_callback)(struct ipc_frame_data_s* frame, void* user_data);
    void* user_data;
    ipc_mpp_ai_extinfo_p extinfo;
} ipc_audio_wrapper_ctx_t;

static void ipc_audio_wrapper_callback(struct ipc_frame_data_s* frame, void* ctx)
{
    ipc_audio_wrapper_ctx_t* wctx = (ipc_audio_wrapper_ctx_t*)ctx;

    if (wctx->extinfo) {
        // Calculate fix_80db
        u16 amplitude_80db = ipc_factory(mplitude_80db);
        f32 fix_80db = _get_pcm_db(amplitude_80db) - 80;

        // Calculate VAD detection number
        wctx->extinfo->vad_det_number = ipc_vad_process_ai_frame(_gh_mpp.h_ai_vad,
                                                                 frame->pack[0].data,
                                                                 frame->pack[0].data_len);

        // Calculate sound frame max db
        wctx->extinfo->sound_frame_max_db = _get_sound_frame_max_db(fix_80db,
                                                                 (ps16)frame->pack[0].data,
                                                                 frame->pack[0].data_len,
                                                                 __IPC_LOG__);
    }

    // Call user callback with calculated extended information
    wctx->user_callback(frame, wctx->user_data);
}

s32 ipc_mpp_recv_audio(void (*callback)(struct ipc_frame_data_s* frame, void* user_data),
                       void* user_data,
                       ipc_mpp_ai_extinfo_p extinfo,
                       s32 timeout_ms)
{
    if (!callback) {
        return IPC_INVALID_ARGS;
    }

    struct ipc_api_s* h_plat = ipc_plat_api(0);
    if (!h_plat) {
        return IPC_NOT_INIT;
    }

    // Watchdog feeding logic
    if (_gh_mpp.audio_wdg_fd < 0) {
        _gh_mpp.audio_wdg_fd = ipc_swdg_reg(1);
    }
    if (_gh_mpp.audio_wdg_fd >= 0) {
        ipc_swdg_feed(_gh_mpp.audio_wdg_fd, 10);
    }

    // Create wrapper callback to calculate audio extended information
    ipc_audio_wrapper_ctx_t wrapper_ctx = {callback, user_data, extinfo};

    // Call platform API with wrapper callback
    return h_plat->audio_ai_recv_frame(ipc_audio_wrapper_callback, &wrapper_ctx, timeout_ms);
}

s32 ipc_mpp_video_start(IPC_VIDEO_CHN_TYPE chn, s32 param)
{
    if (chn != IPC_VIDEO_CHN_MAIN && chn != IPC_VIDEO_CHN_SUB) {
        return IPC_INVALID_ARGS;
    }

    struct ipc_api_s* h_plat = ipc_plat_api(0);
    if (!h_plat) {
        return IPC_NOT_INIT;
    }

    s32 ret = h_plat->video_start(chn, (vptr)(word)param);
    if (ret == IPC_SUCCESS) {
        // Record that this channel was started manually in non-thread mode
        s32 idx = chn - IPC_VIDEO_CHN_MAIN;
        if (idx >= 0 && idx < 2) {
            _gh_mpp.video_channel_started[idx] = 1;
        }
    }

    return ret;
}

s32 ipc_mpp_video_stop(IPC_VIDEO_CHN_TYPE chn)
{
    if (chn != IPC_VIDEO_CHN_MAIN && chn != IPC_VIDEO_CHN_SUB) {
        return IPC_INVALID_ARGS;
    }
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    if (!h_plat) {
        return IPC_NOT_INIT;
    }

      // Clear the manual start state for this channel
    s32 idx = chn - IPC_VIDEO_CHN_MAIN;
    if (idx >= 0 && idx < 2) {
        _gh_mpp.video_channel_started[idx] = 0;

        // Unregister watchdog for this channel
        if (_gh_mpp.video_wdg_fd[idx] >= 0) {
            ipc_swdg_unreg(_gh_mpp.video_wdg_fd[idx]);
            _gh_mpp.video_wdg_fd[idx] = -1;
            ipctrace("Unregistered watchdog for video channel %d", chn);
        }
    }

    ipc_sleep(1);

    return h_plat->video_stop(chn);
}

static vptr _pth_audio(vptr arg)
{
    ipc_log_p log_father = __IPC_LOG__;
    ipc_log_t __IPC_LOG__ = { { NULL, NULL } };
    clog_init("ai", "Audio in stream", log_father);

    s32 mpp_chn             = (s32)(word)arg;
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    s32 ret                 = 0;

    _gh_mpp.alive[mpp_chn] = 1;

    s32 wdg_fd = ipc_swdg_reg(1);
    ipc_swdg_feed(wdg_fd, 10);
    ipcinfo("Get audio stream start watchdog fd=[%d]", wdg_fd);

    f32 fix_80db = _get_pcm_db(ipc_factory(mplitude_80db)) - 80;

    while (_gh_mpp.gorun) {
        ret = h_plat->audio_ai_recv_frame(__plat_recv_audio_frame_cb, &fix_80db, 1 * 1000);
        if (ret) {
            ipctrace("waiting...");
            ipc_msleep(10);
            continue;
        }
        ipc_swdg_feed(wdg_fd, 10);
    }

    ipc_swdg_unreg(wdg_fd);

    _gh_mpp.alive[mpp_chn] = 0;
    ipcinfo("Get audio stream exit");

    return NULL;
}

s32 ipc_mpp_vad_sensitivity(s32 sensitivity)
{
    if (!_gh_mpp.h_ai_vad) {
        return IPC_NOT_INIT;
    }

    if (sensitivity < 0 || sensitivity > 3) {
        return IPC_INVALID_ARGS;
    }

    ipc_vad_set_level(_gh_mpp.h_ai_vad, sensitivity);

    return IPC_SUCCESS;
}

static void _save_snapshot(struct ipc_frame_data_s* frame)
{
    s32 len  = 0;
    s32 ret  = 0;
    pv8 path = NULL;
    pv8 mem  = NULL;

    if (!_gh_mpp.snapshot_mem_or_path) {
        return;
    }

    if (_gh_mpp.snapshot_mem_size > 0) {
        mem = _gh_mpp.snapshot_mem_or_path;
        ipcdebug("Snapshot to memory");
    } else {
        path = _gh_mpp.snapshot_mem_or_path;
        ipcdebug("Snapshot to %s", path);
    }

    if (path) {
        ipc_file_t file;
        ret = ipc_file_open(file, path, IPC_FILE_WRONLY, __IPC_LOG__);
        if (ret != IPC_SUCCESS) {
            goto exit;
        }

        for (s32 i = 0; i < frame->pack_num; i++) {
            // coverity[CHECKED_RETURN :SUPPRESS]
            ipc_file_write(file, frame->pack[i].data, frame->pack[i].data_len);
        }

        ipc_file_close(file);

    } else {
        for (s32 i = 0; i < frame->pack_num; i++) {
            if (len + frame->pack[i].data_len > _gh_mpp.snapshot_mem_size) {
                ret = IPC_NOBUF;
                break;
            }
            memcpy(mem + len, frame->pack[i].data, frame->pack[i].data_len);
            len += frame->pack[i].data_len;
        }
    }
exit:
    _gh_mpp.snapshot_mem_or_path = NULL;
    _gh_mpp.snapshot_mem_size    = 0;

    _gh_mpp.snapshot_return = ret < 0 ? ret : len;
}

static void __plat_recv_jpeg_frame_cb(struct ipc_frame_data_s* frame, vptr _user)
{
    ipctrace("jpeg pack_num=[%d], tms=[%lld]", frame->pack_num, frame->timestamp);

    ipc_lock(_gh_mpp.snapshot_lock);

    if (!_gh_mpp.qr_enable) {
        _save_snapshot(frame);
    }

    ipc_cond_wakeup(_gh_mpp.snapshot_cond);

    ipc_unlock(_gh_mpp.snapshot_lock);
}

static vptr _pth_jpeg(vptr arg)
{
    s32 mpp_chn = (s32)(word)arg;

    s32 chn                 = IPC_VIDEO_CHN_JPEG;
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    s32 ret                 = h_plat->video_start(chn, 0);
    if (ret != IPC_SUCCESS) {
        ipcfatal("Snapshot start failed! retcode=[%d]", ret);
        return NULL;
    }

    _gh_mpp.alive[mpp_chn] = 1;

    s32 wdg_fd = ipc_swdg_reg(1);
    ipc_swdg_feed(wdg_fd, 10);

    ipcinfo("Snapshot watchdog fd=[%d]", wdg_fd);

    while (_gh_mpp.gorun) {

        ret = h_plat->video_recv_frame(chn, __plat_recv_jpeg_frame_cb, NULL, 1000);
        if (ret) {
            ipctrace("Chn_%d waiting...", chn);
            ipc_msleep(40);
            continue;
        }

        ipc_swdg_feed(wdg_fd, 10);
    }

    h_plat->video_stop(chn);
    ipc_swdg_unreg(wdg_fd);

    _gh_mpp.alive[mpp_chn] = 0;

    return NULL;
}
s32 ipc_mpp_snapshot(pv8 mem_or_path, s32 mem_size)
{
    if (!mem_or_path)
        return IPC_INVALID_ARGS;
    // coverity[UNUSED_VALUE :SUPPRESS]
    s32 ret = IPC_TIMEOUT;

    ipc_lock(_gh_mpp.snapshot_lock);

    _gh_mpp.snapshot_mem_or_path = mem_or_path;
    _gh_mpp.snapshot_mem_size    = mem_size;

    ret = ipc_cond_wait(_gh_mpp.snapshot_cond, _gh_mpp.snapshot_lock, ipc_mono_tms() + 500);
    if (ret == 0) {
        ret = _gh_mpp.snapshot_return;
    }

    _gh_mpp.snapshot_mem_or_path = NULL;
    _gh_mpp.snapshot_mem_size    = 0;

    ipc_unlock(_gh_mpp.snapshot_lock);

    return ret;
}

void ipc_mpp_force_iframe(IPC_VIDEO_CHN_TYPE chn)
{
    static u64 last[2] = { 0 };
    if (chn >= ARRSIZE(last))
        return;

    u64 now = ipc_mono_tms();
    if (now >= last[chn] + 500) {
        ipcinfo("Force iframe");
        last[chn] = now;
        ipc_plat_api(0)->video_request_key_frame(chn);
    }
}

void ipc_mpp_image_flip(u8 is_flip)
{
    IPC_ISP_MIRRORFLIP_TYPE type = (!!is_flip) ^ (!!ipc_factory(image_flip)) ? IPC_ISP_MIRRORFLIP_TYPE_MIRROR_FLIP : IPC_ISP_MIRRORFLIP_TYPE_NORMAL;
    ipc_plat_api(0)->video_ctrl(0, IPC_VIDEO_CTRL_CMD_MIRRORFILP, &type);
    ipcinfo("Image flip %s", is_flip ? "on" : "off");
}

void ipc_mpp_set_mic_volume(s32 vol, s32 gain)
{
    if (vol >= 0)
        _gh_mpp.mic_vol = vol;
    if (gain >= 0)
        _gh_mpp.mic_gain = gain;
    ipc_plat_api(0)->audio_set_vol(IPC_AUDIO_DEV_INPUT, _gh_mpp.mic_gain, _gh_mpp.mic_vol);
    ipcinfo("Set mic gain=[%d] volume=[%d]", _gh_mpp.mic_gain, _gh_mpp.mic_vol);
}

void ipc_mpp_get_mic_volume(ps32 vol, ps32 gain)
{
    if (vol)
        *vol = _gh_mpp.mic_vol;
    if (gain)
        *gain = _gh_mpp.mic_gain;
}

/***************************************** osd ***********************************************/

#define MAX_OSD_CHAR 127
static s8 _g_osd_char_map[MAX_OSD_CHAR + 1] = {
    [0 ... MAX_OSD_CHAR] = -1,
};

static u8 _g_char_num = 0;
static struct {
    v8 field[11];
    u8 len;
} _g_osd_hooks[10];

typedef struct {
    v8 data[2][63]; /* Points to the buffer */
    u8 idx;         /* Index of the current buffer being used */
} osd_ctx_t, *osd_ctx_p;

static u32 _str_to_image(pu32 buff, s32 max_date_len, s32 font_dest_w, s32 font_dest_h, pu32 font_data, s32 font_src_w, s32 font_src_h, osd_ctx_p ctx)
{
    s32 char_idx    = 0; // String index
    s32 font_idx    = 0; // Destination font buffer index
    s32 hook_idx    = 0; // Buffer index for hooked string sets
    s32 font_offset = 0; // Source font offset

    while (font_idx < max_date_len) {

        font_offset = _g_char_num;
        for (hook_idx = 0; hook_idx < ARRSIZE(_g_osd_hooks); hook_idx++) {
            if (!_g_osd_hooks[hook_idx].field[0])
                break; // No character hooks
            s32 ch = 0;
            for (ch = 0; _g_osd_hooks[hook_idx].field[ch]; ch++) { // Check if fields match
                if (!ctx->data[ctx->idx][char_idx + ch])
                    break; // Data is invalid
                if (_g_osd_hooks[hook_idx].field[ch] != ctx->data[ctx->idx][char_idx + ch])
                    break;
            }
            if (!_g_osd_hooks[hook_idx].field[ch]) { // String matched
                if (font_idx + _g_osd_hooks[hook_idx].len <= max_date_len
                    && memcmp(&ctx->data[ctx->idx][char_idx], &ctx->data[!ctx->idx][char_idx],
                              ch)) { // Filter out characters identical to the previous ones
                    ipc_32bit_interpolation(buff + font_idx * font_dest_w, font_dest_w * _g_osd_hooks[hook_idx].len, font_dest_h,
                                           font_data + font_offset * font_src_w * font_src_h, font_src_w * _g_osd_hooks[hook_idx].len, font_src_h,
                                           font_dest_w * max_date_len);
                }
                font_idx += _g_osd_hooks[hook_idx].len;
                char_idx += ch;
                break;
            }
            font_offset += _g_osd_hooks[hook_idx].len;
        }
        if (hook_idx < ARRSIZE(_g_osd_hooks) && _g_osd_hooks[hook_idx].field[0])
            continue; // Data exists

        s32 now_ch = ctx->data[ctx->idx][char_idx];
        if (!now_ch) { // Insufficient string content to fill the screen
            for (s32 row = 0; row < font_dest_h; row++) {
                memset(buff + font_idx * font_dest_w + row * font_dest_w * max_date_len, 0, font_dest_w * sizeof(u32));
            }
            font_idx++;
            continue;
        }

        s32 last_ch = ctx->data[!ctx->idx][char_idx];
        if (now_ch != last_ch) { // Refresh only if different
            font_offset = -1;
            if (now_ch >= 0 && now_ch <= MAX_OSD_CHAR) {
                font_offset = _g_osd_char_map[now_ch];
            }
            if (font_offset < 0) { // Character is invalid, clear all (transparent)
                for (s32 row = 0; row < font_dest_h; row++) {
                    memset(buff + font_idx * font_dest_w + row * font_dest_w * max_date_len, 0, font_dest_w * sizeof(u32));
                }
            } else {
                ipc_32bit_interpolation(buff + font_idx * font_dest_w, font_dest_w, font_dest_h, font_data + font_offset * font_src_w * font_src_h,
                                       font_src_w, font_src_h, font_dest_w * max_date_len);
            }
        }
        font_idx++;
        char_idx++;
    }

    return max_date_len * font_dest_w * font_dest_h * sizeof(u32);
}

static vptr _pth_osd(vptr arg)
{
#define BASE_PIXEL_HEIGHT 720 /* Base height of 720 means that all OSD files are designed for 720p resolution. */

    s32 mpp_chn = (s32)(word)arg;

    struct ipc_api_s* h_plat = ipc_plat_api(0);
    struct ipc_plat_video_capability cap;
    h_plat->video_query_capability(&cap);

    /* Calculate scaling based on height */
    f32 sd_scale = cap.res[IPC_VIDEO_CHN_SUB].height * 1.0f / BASE_PIXEL_HEIGHT;
    f32 hd_scale = cap.res[IPC_VIDEO_CHN_MAIN].height * 1.0f / BASE_PIXEL_HEIGHT;

    ipc_json_t hooks_json[] = {
        json_string("field", _g_osd_hooks->field),
        json_uint("len", _g_osd_hooks->len),
    };

    u8 has_time    = 0;
    f32 time_x_pct = 0, time_y_pct = 0;
    s32 font_w_pix = 0, font_h_pix = 0;
    s32 max_date_len                         = 0;
    v8 time_format[IPC_MPP_OSD_TIME_MAX_SIZE] = { 0 };
    v8 char_sets[128]                        = { 0 };
    v8 time_file[128]                        = { 0 };
    ipc_json_t time_json[]                    = {
        json_double("x_pct", time_x_pct, CHECK), json_double("y_pct", time_y_pct, CHECK),
        json_int("max", max_date_len, CHECK),    json_string("format", time_format, CHECK),
        json_int("w_pix", font_w_pix, CHECK),    json_int("h_pix", font_h_pix, CHECK),
        json_string("order", char_sets, CHECK),  json_arrobj("order_expand", _g_osd_hooks, hooks_json),
        json_string("file", time_file, CHECK),
    };

    u8 has_logo    = 0;
    f32 logo_x_pct = 0, logo_y_pct = 0;
    s32 logo_w_pix = 0, logo_h_pix = 0;
    v8 logo_file[128]     = { 0 };
    ipc_json_t logo_json[] = {
        json_double("x_pct", logo_x_pct, CHECK), json_double("y_pct", logo_y_pct, CHECK), json_int("w_pix", logo_w_pix, CHECK),
        json_int("h_pix", logo_h_pix, CHECK),    json_string("file", logo_file, CHECK),
    };

    ipc_json_t osd_json[] = {
        json_object("time", NULL, time_json, NUMBER, has_time),
        json_object("logo", NULL, logo_json, NUMBER, has_logo),
    };

    {
        v8 osd_conf[1024] = { 0 };
        // coverity[CHECKED_RETURN :SUPPRESS]
        ipc_file_read_once("/app/osd/osd.json", osd_conf, sizeof(osd_conf), __IPC_LOG__);
        ipc_json_parse(osd_conf, osd_json, ARRSIZE(osd_json));
        _g_char_num = strlen(char_sets);
    }

    struct ipc_video_osd_attr_s attr;
    memset(&attr, 0, sizeof(struct ipc_video_osd_attr_s));

    s32 sd_font_w     = 0;
    s32 sd_font_h     = 0;
    s32 hd_font_w     = 0;
    s32 hd_font_h     = 0;
    vptr time_buff    = NULL;
    pu32 font_data    = NULL;
    pu32 hd_time_data = NULL;
    pu32 sd_time_data = NULL;
    if (has_time) {

        s32 char_num = 0;
        while (char_sets[char_num]) { // Initialize global character mapping table
            if (char_sets[char_num] >= 0 && char_sets[char_num] <= MAX_OSD_CHAR) {
                _g_osd_char_map[(u8)char_sets[char_num]] = char_num;
            }
            char_num++;
        }
        for (s32 idx = 0; idx < ARRSIZE(_g_osd_hooks); idx++) { // Expand data size
            if (!_g_osd_hooks[idx].field[0])
                break;
            char_num += _g_osd_hooks[idx].len;
        }

        sd_font_w = (s32)(sd_scale * font_w_pix);
        if (sd_font_w % 2)
            sd_font_w--;
        sd_font_h = (s32)(sd_scale * font_h_pix);
        if (sd_font_h % 2)
            sd_font_h--;

        hd_font_w = (s32)(hd_scale * font_w_pix);
        if (hd_font_w % 2)
            hd_font_w--;
        hd_font_h = (s32)(hd_scale * font_h_pix);
        if (hd_font_h % 2)
            hd_font_h--;

        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].enable   = 1;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].fmt_type = IPC_VIDEO_OSD_FMT_RGBA;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].x        = cap.res[IPC_VIDEO_CHN_MAIN].width * time_x_pct / 100;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].y        = cap.res[IPC_VIDEO_CHN_MAIN].height * time_y_pct / 100;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].width    = max_date_len * hd_font_w;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].height   = hd_font_h;
        if (time_x_pct < 0) { // Negative, adjust modulus, correct to right boundary as origin
            attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].x += cap.res[IPC_VIDEO_CHN_MAIN].width - attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].width;
        }
        if (time_y_pct < 0) { // Negative, adjust modulus, correct to bottom boundary as origin
            attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].y += cap.res[IPC_VIDEO_CHN_MAIN].height - attr.chn[IPC_VIDEO_CHN_MAIN].rgn[0].height;
        }

        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].enable   = 1;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].fmt_type = IPC_VIDEO_OSD_FMT_RGBA;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].x        = cap.res[IPC_VIDEO_CHN_SUB].width * time_x_pct / 100;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].y        = cap.res[IPC_VIDEO_CHN_SUB].height * time_y_pct / 100;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].width    = max_date_len * sd_font_w;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].height   = sd_font_h;
        if (time_x_pct < 0) { // Negative, adjust modulus, correct to right boundary as origin
            attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].x += cap.res[IPC_VIDEO_CHN_SUB].width - attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].width;
        }
        if (time_y_pct < 0) { // Negative, adjust modulus, correct to bottom boundary as origin
            attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].y += cap.res[IPC_VIDEO_CHN_SUB].height - attr.chn[IPC_VIDEO_CHN_SUB].rgn[0].height;
        }

        s32 font_buff_len = font_w_pix * font_h_pix * char_num * sizeof(u32);   // Font buffer
        s32 sd_buff_len   = sd_font_w * sd_font_h * max_date_len * sizeof(u32); // HD display buffer
        s32 hd_buff_len   = hd_font_w * hd_font_h * max_date_len * sizeof(u32); // SD display buffer

        time_buff = ipc_malloc(font_buff_len + hd_buff_len + sd_buff_len, 0); /* Allocate three blocks of memory together */
        if (time_buff == NULL) {
            ipcfatal("Malloc OSD memory failed!");
            return NULL;
        }

        font_data    = time_buff;
        hd_time_data = time_buff + font_buff_len;
        sd_time_data = time_buff + font_buff_len + hd_buff_len;
        ipc_file_read_once(time_file, (pv8)font_data, font_buff_len, __IPC_LOG__); // Load font buffer
    }

    s32 sd_logo_w = 0;
    s32 sd_logo_h = 0;
    s32 hd_logo_w = 0;
    s32 hd_logo_h = 0;
    if (has_logo) {
        sd_logo_w = (s32)(sd_scale * logo_w_pix);
        if (sd_logo_w % 2)
            sd_logo_w--;
        sd_logo_h = (s32)(sd_scale * logo_h_pix);
        if (sd_logo_h % 2)
            sd_logo_h--;

        hd_logo_w = (s32)(hd_scale * logo_w_pix);
        if (hd_logo_w % 2)
            hd_logo_w--;
        hd_logo_h = (s32)(hd_scale * logo_h_pix);
        if (hd_logo_h % 2)
            hd_logo_h--;

        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].enable   = 1;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].fmt_type = IPC_VIDEO_OSD_FMT_RGBA;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].x        = cap.res[IPC_VIDEO_CHN_MAIN].width * logo_x_pct / 100;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].y        = cap.res[IPC_VIDEO_CHN_MAIN].height * logo_y_pct / 100;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].width    = hd_logo_w;
        attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].height   = hd_logo_h;
        if (logo_x_pct < 0) {
            attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].x += cap.res[IPC_VIDEO_CHN_MAIN].width - attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].width;
        }
        if (logo_y_pct < 0) {
            attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].y += cap.res[IPC_VIDEO_CHN_MAIN].height - attr.chn[IPC_VIDEO_CHN_MAIN].rgn[1].height;
        }

        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].enable   = 1;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].fmt_type = IPC_VIDEO_OSD_FMT_RGBA;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].x        = cap.res[IPC_VIDEO_CHN_SUB].width * logo_x_pct / 100;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].y        = cap.res[IPC_VIDEO_CHN_SUB].height * logo_y_pct / 100;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].width    = sd_logo_w;
        attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].height   = sd_logo_h;
        if (logo_x_pct < 0) {
            attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].x += cap.res[IPC_VIDEO_CHN_SUB].width - attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].width;
        }
        if (logo_y_pct < 0) {
            attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].y += cap.res[IPC_VIDEO_CHN_SUB].height - attr.chn[IPC_VIDEO_CHN_SUB].rgn[1].height;
        }
    }

    _gh_mpp.alive[mpp_chn]
        = 1; // Since resetting _gh_mpp.dyn_font_attr requires reinitializing the OSD module, alive must be set before setting dyn_font_attr.

    int ret = h_plat->video_osd_init(&attr);
    if (ret != IPC_SUCCESS) {
        ipcfatal("Platform OSD initialization failed! Return code=[%d]", ret);
        if (time_buff)
            ipc_free(time_buff);
        _gh_mpp.alive[mpp_chn] = 0;
        return NULL;
    }

    s32 image_len = 0;
    u8 time_sw    = 0;
    u8 logo_sw    = 0;

    osd_ctx_t ctx[1] = { { { { 0 }, { 0 } }, 0 } }; // Initialize ctx
    u32 last_ts      = 0;
    u32 now_ts       = 0;
    while (_gh_mpp.gorun) {

        if (!_gh_mpp.osd_run) {
            break;
        }

        if (has_logo && logo_sw != _gh_mpp.logo_sw) {
            logo_sw = _gh_mpp.logo_sw;
            if (!logo_sw) {
                ipcinfo("Logo hidden!");
                h_plat->video_osd_set(IPC_VIDEO_CHN_MAIN, 1, 0, NULL, 0);
                h_plat->video_osd_set(IPC_VIDEO_CHN_SUB, 1, 0, NULL, 0);
            } else {
                ipcinfo("Logo shown!");
                s32 logo_src_len   = logo_w_pix * logo_h_pix * sizeof(u32);
                pu32 logo_src_data = ipc_malloc(logo_src_len, 0);
                if (logo_src_data) {
                    ipc_file_read_once(logo_file, (pv8)logo_src_data, logo_src_len, __IPC_LOG__);
                    s32 hd_logo_len     = hd_logo_w * hd_logo_h * sizeof(u32);
                    s32 sd_logo_len     = sd_logo_w * sd_logo_h * sizeof(u32);
                    pu32 logo_dest_data = ipc_malloc(MAX(hd_logo_len, sd_logo_len), 0);
                    if (logo_dest_data) {
                        ipc_32bit_interpolation(logo_dest_data, hd_logo_w, hd_logo_h, logo_src_data, logo_w_pix, logo_h_pix, 0);
                        h_plat->video_osd_set(IPC_VIDEO_CHN_MAIN, 1, 1, logo_dest_data, hd_logo_len);
                        ipc_32bit_interpolation(logo_dest_data, sd_logo_w, sd_logo_h, logo_src_data, logo_w_pix, logo_h_pix, 0);
                        h_plat->video_osd_set(IPC_VIDEO_CHN_SUB, 1, 1, logo_dest_data, sd_logo_len);
                        ipc_free(logo_dest_data);
                    }
                    ipc_free(logo_src_data);
                }
            }
        }

        if (has_time && time_sw != _gh_mpp.time_sw) {
            time_sw = _gh_mpp.time_sw;
            if (!time_sw) {
                ipcinfo("Time hidden!");
                h_plat->video_osd_set(IPC_VIDEO_CHN_MAIN, 0, 0, NULL, 0);
                h_plat->video_osd_set(IPC_VIDEO_CHN_SUB, 0, 0, NULL, 0);
            } else {
                ipcinfo("Time shown!");
            }
        }

        if (has_time && time_sw) {
            now_ts = _gh_mpp.f_realts();
            if (last_ts != now_ts) {
                last_ts = now_ts;
                ipc_ts2str(ctx->data[ctx->idx], sizeof(ctx->data[0]) - 1, _gh_mpp.time_format[0] ? _gh_mpp.time_format : time_format, now_ts);
                ipcdebug("Time: %s", ctx->data[ctx->idx]);
                image_len = _str_to_image(hd_time_data, max_date_len, hd_font_w, hd_font_h, font_data, font_w_pix, font_h_pix, ctx);
                h_plat->video_osd_set(IPC_VIDEO_CHN_MAIN, 0, 1, hd_time_data, image_len);
                image_len = _str_to_image(sd_time_data, max_date_len, sd_font_w, sd_font_h, font_data, font_w_pix, font_h_pix, ctx);
                h_plat->video_osd_set(IPC_VIDEO_CHN_SUB, 0, 1, sd_time_data, image_len);
                ctx->idx = !ctx->idx; // Toggle buffer
            }
        }


        ipc_msleep(200);
    }

    h_plat->video_osd_uninit();
    if (time_buff)
        ipc_free(time_buff);

    _gh_mpp.alive[mpp_chn] = 0;

    return NULL;
}

s32 ipc_mpp_init(ipc_mpp_cb_p mpp_cb)
{
    if (!mpp_cb)
        return IPC_INVALID_ARGS;

    clog_init("mpp", "Media Process Platform");
    s32 ret = 0;

    if (mpp_cb->f_realts)
        _gh_mpp.f_realts = mpp_cb->f_realts;
    if (mpp_cb->f_push_video)
        _gh_mpp.f_push_video = mpp_cb->f_push_video;
    if (mpp_cb->f_push_audio)
        _gh_mpp.f_push_audio = mpp_cb->f_push_audio;
    if (mpp_cb->f_play_finish)
        _gh_mpp.f_play_finish = mpp_cb->f_play_finish;

    // Default create_threads to 1 if not specified (for backward compatibility)
    u8 create_threads = (mpp_cb->create_threads == 0) ? 0 : 1;

    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();
    if (decrypt == NULL) {
        ipcfatal("Decrypt verify failed");
        return IPC_VERIFY_FAILED;
    }

    struct ipc_api_s* h_plat = ipc_plat_api(0);

    _gh_mpp.gorun = 1;

    ret = h_plat->sys_init(decrypt->product_type);
    if (ret < 0) {
        ipcfatal("Plat sys init failed!! retcode=[%d]", ret);
        return IPC_FAILED;
    }

    struct ipc_plat_audio_init_attr audio_attr;
    ipc_middleware_sal_api()->f_ipc_sal_media_info(&audio_attr);
    if (ipc_factory(spk_vol))
        audio_attr.ao_vol = ipc_factory(spk_vol);
    if (ipc_factory(mic_vol))
        audio_attr.ai_vol = ipc_factory(mic_vol);
    if (ipc_factory(spk_gain))
        audio_attr.ao_gain = ipc_factory(spk_gain);
    if (ipc_factory(mic_gain))
        audio_attr.ai_gain = ipc_factory(mic_gain);
    _gh_mpp.mic_vol      = audio_attr.ai_vol;
    _gh_mpp.mic_gain     = audio_attr.ai_gain;
    _gh_mpp.mic_codec    = audio_attr.audio_enc;
    audio_attr.audio_enc = IPC_AUDIO_ENC_TYPE_PCM;

    _gh_mpp.h_ai_vad = ipc_vad_init(ipc_trans_sample(audio_attr.sample), 3);
    if (_gh_mpp.h_ai_vad == NULL) {
        ipcfatal("Ai vad init failed!!");
        return IPC_FAILED;
    }

    struct ipc_io_active_level_flip io_flip[] = {
        { IPC_IO_NAME_SPEAKER, !!ipc_factory(spk_flip) },
        { IPC_IO_NAME_WHITE_LIGTH, !!ipc_factory(white_light_flip) },
        { IPC_IO_NAME_INFRARED_LIGTH, !!ipc_factory(irled_flip) },
        { IPC_IO_NAME_LIGHT_SENSOR, !!ipc_factory(light_sensor_flip) },
        { IPC_IO_NAME_IRCUT_A, !!ipc_factory(ircut_flip) },
        { IPC_IO_NAME_IRCUT_B, !!ipc_factory(ircut_flip) },
        { IPC_IO_NAME_FLOOD_LIGHT, !!ipc_factory(flood_light_flip) },
        { IPC_IO_NAME_STATUS_INDICATOR_A, !!ipc_factory(indicator_lighta_flip) },
        { IPC_IO_NAME_STATUS_INDICATOR_B, !!ipc_factory(indicator_lightb_flip) },
    };

    ret = h_plat->io_init(io_flip, ARRSIZE(io_flip));
    if (ret < 0) {
        ipcfatal("Plat io init failed!! retcode=[%d]", ret);
        return IPC_FAILED;
    }

    _gh_mpp.io_inited = 1;

    ret = h_plat->video_init(0);
    if (ret < 0) {
        ipcfatal("Plat video init failed!! retcode=[%d]", ret);
        return IPC_FAILED;
    }

    ipc_mpp_image_flip(0);

    ret = h_plat->audio_init(&audio_attr);
    if (ret < 0) {
        ipcfatal("Plat audio init failed!! retcode=[%d]", ret);
        return IPC_FAILED;
    }

    ret = h_plat->audio_start(1, 1);
    if (ret < 0) {
        ipcfatal("Plat audio start failed! retcode=[%d]", ret);
        return IPC_FAILED;
    }

    // Create audio/video threads only if requested
    if (create_threads) {
        ret = ipc_create_thread("ipc_video_hd", _pth_video, (vptr)MPP_HD, 128 * 1024, 0);
        if (ret < 0) {
            ipcfatal("Create video HD thread failed! retcode=[%d]", ret);
            return ret;
        }

        ret = ipc_create_thread("ipc_video_sd", _pth_video, (vptr)MPP_SD, 128 * 1024, 0);
        if (ret < 0) {
            ipcfatal("Create video SD thread failed! retcode=[%d]", ret);
            return ret;
        }
    }

    ipc_lock_init(_gh_mpp.snapshot_lock, IPC_THREAD_MUTEX);
    ipc_cond_init(_gh_mpp.snapshot_cond, IPC_THREAD_COND);

    ret = ipc_create_thread("ipc_jpeg", _pth_jpeg, (vptr)MPP_JPEG, 128 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create jpeg thread failed! retcode=[%d]", ret);
        return ret;
    }

    // Create audio thread only if requested
    if (create_threads) {
        ret = ipc_create_thread("ipc_audio", _pth_audio, (vptr)MPP_AUDIO, 128 * 1024, 0);
        if (ret < 0) {
            ipcfatal("Create audio in thread failed! retcode=[%d]", ret);
            return ret;
        }
    }

    if (decrypt->product_type != IPC_PRODUCT_TYPE_PANO_360) {
        _gh_mpp.osd_run = 1;
        ret             = ipc_create_thread("ipc_osd", _pth_osd, (vptr)MPP_OSD, 128 * 1024, 0);
        if (ret < 0) {
            ipcfatal("Create osd thread failed! retcode=[%d]", ret);
            return ret;
        }
    }

    s32 audio_bitrate = ipc_trans_channel(audio_attr.channel) * ipc_trans_databits(audio_attr.databits) / 8 * ipc_trans_sample(audio_attr.sample);
    _gh_speaker.pcm_pack_size = audio_bitrate / audio_attr.frame_rate;
    _gh_speaker.now_vol = _gh_speaker.default_vol = audio_attr.ao_vol;
    _gh_speaker.now_gain = _gh_speaker.default_gain = audio_attr.ao_gain;
    ipc_lock_init(_gh_speaker.mutex, IPC_THREAD_MUTEX);
    _gh_speaker.ring = ipc_ring_init(0, 0, audio_bitrate);
    if (_gh_speaker.ring == NULL) {
        ipcfatal("Player ring buffer init failed!");
        return IPC_NOMEM;
    }

    ret = ipc_create_thread("ipc_speaker", _pth_speaker, (vptr)MPP_SPEAK, 128 * 1024, 0);
    if (ret < 0) {
        ipcfatal("Create speaker thread failed! retcode=[%d]", ret);
        return ret;
    }

    ipcinfo("Init complete!\n%s", __IPC_INFO__);

    return IPC_SUCCESS;
}

void ipc_mpp_uninit(s32 is_wait)
{
    _gh_mpp.gorun = 0;
    if (!is_wait)
        return;

    u8 alive = 0;
    u8 idx   = 0;

    while (1) {
        for (alive = 0, idx = 0; idx < ARRSIZE(_gh_mpp.alive); idx++) {
            alive |= _gh_mpp.alive[idx];
        }
        if (!alive)
            break;
        ipc_msleep(100);
    }

    ipc_ring_uninit(_gh_speaker.ring);
    ipc_lock_uninit(_gh_speaker.mutex);
    struct ipc_api_s* h_plat = ipc_plat_api(0);

    ipc_lock_uninit(_gh_mpp.snapshot_lock);
    ipc_cond_uninit(_gh_mpp.snapshot_cond);

    // Auto-stop video channels that were manually started in non-thread mode
    for (s32 i = 0; i < 2; i++) {
        if (_gh_mpp.video_channel_started[i]) {
            IPC_VIDEO_CHN_TYPE chn = (i == 0) ? IPC_VIDEO_CHN_MAIN : IPC_VIDEO_CHN_SUB;
            ipctrace("Auto-stopping video channel %d in uninit", chn);
            ipc_mpp_video_stop(chn);  // Use the wrapper interface instead of direct platform call
        }
    }

    // Stop audio and cleanup watchdog in non-thread mode
    if (_gh_mpp.audio_wdg_fd >= 0) {
        ipctrace("Auto-unregistering audio watchdog in uninit");
        ipc_swdg_unreg(_gh_mpp.audio_wdg_fd);
        _gh_mpp.audio_wdg_fd = -1;
    }

    ipc_sleep(1);

    h_plat->audio_stop(1, 1);
    h_plat->audio_uninit();
    h_plat->video_uninit();
    h_plat->io_uninit();
    h_plat->sys_uninit();

    ipc_vad_uninit(_gh_mpp.h_ai_vad);
    ipcinfo("Exit complete!");
}

s32 ipc_mpp_qr_enhancement(u32 enable)
{
    struct ipc_api_s* h_plat = ipc_plat_api(0);
    // coverity[UNUSED_VALUE :SUPPRESS] 
    f32 multiplier          = 1.0;

    _gh_mpp.qr_enable = enable;

    h_plat->video_ctrl(0, IPC_VIDEO_CTRL_CMD_QR_ENHANCE_BY_ISP_MODE, (vptr)(word)enable);

    multiplier = enable ? 1.7 : 1.0;

    // coverity[CHECKED_RETURN :SUPPRESS]
    ipc_ptz_zoom_multiplier_set(multiplier);

    return IPC_SUCCESS;
}

void ipc_mpp_osd_logo_switch(u8 sw)
{
    _gh_mpp.logo_sw = !!sw;
}

void ipc_mpp_osd_time_switch(u8 sw)
{
    _gh_mpp.time_sw = !!sw;
}

void ipc_mpp_osd_time_format(v8 format[IPC_MPP_OSD_TIME_MAX_SIZE])
{
    if (!format)
        format = "";
    snprintf(_gh_mpp.time_format, sizeof(_gh_mpp.time_format), "%s", format);
}

s32 ipc_mpp_osd_uninit(void)
{
    _gh_mpp.osd_run = 0;

    while (1) {
        if (!_gh_mpp.alive[MPP_OSD])
            break;
        ipc_msleep(100);
    }

    return IPC_SUCCESS;
}

s32 ipc_mpp_osd_init(void)
{
    s32 ret                     = 0;
    ipc_decrypt_ininfo_p decrypt = ipc_decrypt_ininfo();
    if (decrypt == NULL) {
        ipcfatal("Decrypt verify failed");
        return IPC_VERIFY_FAILED;
    }

    if (decrypt->product_type != IPC_PRODUCT_TYPE_PANO_360) {
        _gh_mpp.osd_run = 1;
        ret             = ipc_create_thread("ipc_osd", _pth_osd, (vptr)MPP_OSD, 128 * 1024, 0);
        if (ret < 0) {
            ipcfatal("Create osd thread failed! retcode=[%d]", ret);
            return ret;
        }
    }
    return IPC_SUCCESS;
}

s32 ipc_mpp_net_wireless_io_ctrl(s32 on_off)
{
    word volatile plat_api = (word)ipc_plat_api;
    if (!plat_api) {
        return IPC_SUCCESS;
    }

    if (!_gh_mpp.io_inited) {
        return IPC_NOT_INIT;
    }

    IPC_IO_VALUE_TYPE val = on_off ? IPC_IO_VALUE_IS_ACTIVE : IPC_IO_VALUE_IS_INACTIVE;

    ipc_plat_api(0)->io_write(IPC_IO_NAME_WIRELESS_PWR, val);

    return IPC_SUCCESS;
}

s32 ipc_mpp_eth_rst_io_ctrl(void)
{
    word volatile plat_api = (word)ipc_plat_api;
    if (!plat_api) {
        return IPC_FAILED;
    }

    if (!_gh_mpp.io_inited) {
        return IPC_NOT_INIT;
    }

    ipcdebug("eth rst start\n");
    ipc_plat_api(0)->io_write(IPC_IO_NAME_ETH0_RST, IPC_IO_VALUE_IS_ACTIVE);
    ipc_msleep(600);
    ipc_plat_api(0)->io_write(IPC_IO_NAME_ETH0_RST, IPC_IO_VALUE_IS_INACTIVE);
    ipcdebug("eth rst over\n");

    return IPC_SUCCESS;
}
