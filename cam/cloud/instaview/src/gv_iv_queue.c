#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipc_iv_queue.h"
#include "ipc_middleware.h"
#include "ipc_middleware_sal.h"
#include "ipc_mpp.h"
#include "ipc_platform_api.h"
#include "ipc_thread.h"
#include "iv_error_code.h"
#include "iv_types.h"
#include <fcntl.h>

#include "ipc_aac.h"

static u8 g_is_init = 0;
static vptr _gh_aac = NULL;
static pthread_mutex_t _g_aac_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ipc_plat_video_capability _g_video_cap;
static pthread_once_t _g_video_cap_once = PTHREAD_ONCE_INIT;

static void _query_video_capability(void)
{
    struct ipc_api_s* platform = ipc_plat_api(0);
    if (platform && platform->video_query_capability) {
        platform->video_query_capability(&_g_video_cap);
    }
}

// Context structure for passing frame data between callback and main function
typedef struct {
    MfgVideoFrame* pframeInfo;
    unsigned char** frame_data;
    int* frame_buffer_len;
    E_VIDEO_STREAM_INDEX stream_index;
    int iter_index;
    int frame_len;
    int error;
} mfg_frame_context_t;

// Context structure for passing audio data between callback and main function
typedef struct {
    char* buffer;
    int32_t len;
    int64_t* time_stamp;
    int64_t* frame_index;
    int audio_available;
    int audio_len;
} mfg_audio_context_t;

// Static callback function that fills MfgVideoFrame structure
static void mfg_video_frame_callback(struct ipc_frame_data_s* received_frame, void* user_data)
{
    static IVUINT32 video_frame_index[2] = { 0 };

    mfg_frame_context_t* ctx = (mfg_frame_context_t*)user_data;
    if (received_frame == NULL || received_frame->pack_num <= 0 || ctx == NULL
        || ctx->pframeInfo == NULL || ctx->frame_data == NULL
        || ctx->frame_buffer_len == NULL) {
        return;
    }

    pthread_once(&_g_video_cap_once, _query_video_capability);

    // Get frame data from received frame
    s32 frame_len = received_frame->pack[0].data_len;
    if (frame_len <= 0 || received_frame->pack[0].data == NULL) {
        ctx->error = IPC_NOT_DATA;
        return;
    }

    size_t frame_header_len = ctx->pframeInfo->frame_header_len;
    if (frame_header_len > (size_t)INT_MAX
        || (size_t)frame_len > (size_t)INT_MAX - frame_header_len) {
        ctx->error = IPC_OUT_OF_RANGE;
        return;
    }
    size_t required_size = frame_header_len + (size_t)frame_len;

    // Check if we need to allocate/reallocate frame buffer
    if (*(ctx->frame_data) == NULL || *(ctx->frame_buffer_len) < 0
        || (size_t)*(ctx->frame_buffer_len) < required_size) {
        unsigned char* new_frame_data = (unsigned char*)calloc(required_size, 1);
        if (new_frame_data == NULL) {
            ctx->error = IPC_NOMEM;
            return;
        }
        free(*(ctx->frame_data));
        *(ctx->frame_data) = new_frame_data;
        *(ctx->frame_buffer_len) = (int)required_size;
    }

    // Fill MfgVideoFrame structure
    ctx->pframeInfo->video_data   = *(ctx->frame_data) + frame_header_len;
    ctx->pframeInfo->stream_index = ctx->stream_index;
    ctx->pframeInfo->time_stamp   = received_frame->timestamp;
    ctx->pframeInfo->data_len     = frame_len;
    ctx->pframeInfo->frame_type   = (received_frame->is_key) ? E_I_FRAME : E_P_FRAME;

    // Set encoding type based on platform capability
    if (ctx->iter_index == 0) {
        ctx->pframeInfo->enc_type = (_g_video_cap.type[0] == IPC_VIDEO_ENC_TYPE_H265) ? E_VIDEO_H265_A : E_VIDEO_H264_HP;
    } else {
        ctx->pframeInfo->enc_type = (_g_video_cap.type[1] == IPC_VIDEO_ENC_TYPE_H265) ? E_VIDEO_H265_A : E_VIDEO_H264_HP;
    }

    // Set resolution
    ctx->pframeInfo->width            = _g_video_cap.res[ctx->iter_index].width;
    ctx->pframeInfo->height           = _g_video_cap.res[ctx->iter_index].height;
    ctx->pframeInfo->frame_index      = __sync_fetch_and_add(&video_frame_index[ctx->iter_index], 1);
    ctx->pframeInfo->ext_data_len     = 0;
    ctx->pframeInfo->frame_buffer_len = *(ctx->frame_buffer_len);

    // Copy frame data
    if (frame_len > 0) {
        memcpy(ctx->pframeInfo->video_data, received_frame->pack[0].data, (size_t)frame_len);
    }

    ctx->frame_len = frame_len; // Store result in context
}

// Video frame reception function using direct MPP API call
int MFG_ReadVideoFrame_callback(E_VIDEO_STREAM_INDEX stream_index, MfgVideoFrame* pframeInfo, unsigned char** frame_data, int* frame_buffer_len)
{
    s32 ret = 0;

    if (pframeInfo == NULL || frame_data == NULL || frame_buffer_len == NULL) {
        return IPC_INVALID_ARGS;
    }
    if (stream_index != E_VIDEO_MAIN_STREAM && stream_index != E_VIDEO_SECOND_STREAM) {
        return IPC_INVALID_ARGS;
    }

    // Create context for callback
    mfg_frame_context_t ctx = { .pframeInfo       = pframeInfo,
                                .frame_data       = frame_data,
                                .frame_buffer_len = frame_buffer_len,
                                .stream_index     = stream_index,
                                .iter_index       = (stream_index == E_VIDEO_MAIN_STREAM) ? 0 : 1,
                                .frame_len        = 0,
                                .error            = IPC_SUCCESS };

    // Map stream index to IPC channel
    IPC_VIDEO_CHN_TYPE ipc_chn = (stream_index == E_VIDEO_MAIN_STREAM) ? IPC_VIDEO_CHN_MAIN : IPC_VIDEO_CHN_SUB;

    // Call ipc_mpp_recv_video - this will block and call the callback
    ret = ipc_mpp_recv_video(ipc_chn, mfg_video_frame_callback, &ctx, 1000);

    // Return the frame length (set by callback) or error
    if (ret >= 0 && ctx.frame_len > 0) {
        return ctx.frame_len;
    }

    if (ctx.error < 0) {
        return ctx.error;
    }

    return ret;
}

// Static callback function that fills audio buffer
static void mfg_audio_frame_callback(struct ipc_frame_data_s* received_frame, void* user_data)
{
    static int64_t audio_frame_index_counter = 0;

    mfg_audio_context_t* ctx = (mfg_audio_context_t*)user_data;
    if (received_frame == NULL || received_frame->pack_num <= 0
        || received_frame->pack[0].data == NULL || ctx == NULL
        || ctx->buffer == NULL || ctx->len <= 0) {
        return;
    }

    // Get audio data from received frame
    int32_t audio_len = received_frame->pack[0].data_len;

    // Check if buffer is large enough
    if (audio_len <= 0) {
        return;
    }
    if (audio_len > ctx->len) {
        // Buffer too small, truncate or return error
        audio_len = ctx->len;
        if (audio_len <= 0) {
            return;
        }
    }

    // Copy audio data to buffer
    memcpy(ctx->buffer, received_frame->pack[0].data, (size_t)audio_len);

    // Set timestamp and frame index
    if (ctx->time_stamp != NULL) {
        *(ctx->time_stamp) = received_frame->timestamp * 1000LL; // Convert to microseconds if needed
    }

    if (ctx->frame_index != NULL) {
        *(ctx->frame_index) = audio_frame_index_counter++;
    }

    ctx->audio_available = 1;
    ctx->audio_len       = audio_len;
}

static s32 _is_need_slient_audio(s32 len)
{
    static s32 ignored_frame_len = 0;

    if (!ipc_ptz_is_stop(IPC_PTZ_H) || !ipc_ptz_is_stop(IPC_PTZ_V)) {
        ignored_frame_len = 1 * 2 * 16000 / 2;
    }

    if (ignored_frame_len > 0) {
        ignored_frame_len -= len;
    }

    return ignored_frame_len > 0;
}

// Audio frame reception function using direct MPP API call
int MFG_ReadAudioPcmFrame(char* buffer, int32_t len, int64_t* time_stamp, int64_t* frame_index)
{
    s32 ret = 0;

    if (buffer == NULL || len <= 0) {
        return -1;
    }

    // Create context for callback
    mfg_audio_context_t ctx
        = { .buffer = buffer, .len = len, .time_stamp = time_stamp, .frame_index = frame_index, .audio_available = 0, .audio_len = 0 };

    // Call ipc_mpp_recv_audio - this will block and call the callback
    ipc_mpp_ai_extinfo_t extinfo;
    ret = ipc_mpp_recv_audio(mfg_audio_frame_callback, &ctx, &extinfo, 1000);

    // Return the audio length (set by callback) or error
    if (ret >= 0 && ctx.audio_available && ctx.audio_len > 0) {
        if (_is_need_slient_audio(ctx.audio_len)) {
            memset(buffer, 0, ctx.audio_len);
        }
        return ctx.audio_len;
    }

    return -1;
}

int MFG_EncodeAACAudio(char* pcm_data, int32_t pcm_len, char* encoded_data, int32_t encoded_len)
{

#if 1
    if (!pcm_data || pcm_len <= 0 || !encoded_data || encoded_len <= 0) {
        return -1;
    }

    pthread_mutex_lock(&_g_aac_mutex);
    if (!g_is_init || !_gh_aac) {
        pthread_mutex_unlock(&_g_aac_mutex);
        return -1;
    }

    ipc_aac_data_p _aac_data = ipc_aac_encode_iter(_gh_aac, pcm_data, pcm_len);
    if (!_aac_data || !_aac_data->buf || _aac_data->len <= 0
        || _aac_data->len > encoded_len) {
        pthread_mutex_unlock(&_g_aac_mutex);
        return -1;
    }

    memcpy(encoded_data, _aac_data->buf, _aac_data->len);
    s32 encoded_size = _aac_data->len;
    pthread_mutex_unlock(&_g_aac_mutex);
    return encoded_size;
#else
    int ret     = -1;
    int aac_len = 0;

    if (!g_is_init) {
        goto _exit;
    }

    ipc_aac_data_p _aac_data = NULL;
    if (_gh_aac) {
        while ((_aac_data = ipc_aac_encode_iter(_gh_aac, pcm_data, pcm_len))) {
            if (aac_len + _aac_data->len > encoded_len) {
                goto _exit;
            }
            memcpy(encoded_data + aac_len, _aac_data->buf, _aac_data->len);
            aac_len += _aac_data->len;
        }
    }

    if (aac_len > 0) {
        return aac_len;
    } else if (aac_len < 0) {
        goto _exit;
    }

_exit:
    return ret;
#endif
}
s32 ipc_iv_queue_init_aac_encode(void)
{
    s32 ret = -1;

#if 1
    pthread_mutex_lock(&_g_aac_mutex);
    if (g_is_init && _gh_aac) {
        pthread_mutex_unlock(&_g_aac_mutex);
        return IPC_SUCCESS;
    }
    _gh_aac = ipc_aac_encode_open(16, 8000, 1);
    if (_gh_aac == NULL) {
        pthread_mutex_unlock(&_g_aac_mutex);
        goto _exit;
    }
    g_is_init = 1;
    pthread_mutex_unlock(&_g_aac_mutex);
#else
    _gh_aac = ipc_aac_encode_open(16, 16000, 1);
    if (_gh_aac == NULL) {
        goto _exit;
    }
#endif

    ret = 0;
_exit:
    return ret;
}

void ipc_iv_queue_uninit_aac_encode(void)
{
    pthread_mutex_lock(&_g_aac_mutex);
    g_is_init = 0;
    if (_gh_aac) {
        ipc_aac_encode_close(_gh_aac);
        _gh_aac = NULL;
    }
    pthread_mutex_unlock(&_g_aac_mutex);
}
