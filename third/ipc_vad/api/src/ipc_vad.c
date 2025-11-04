//#include "modules/audio_processing/voice_detection.h"
#include "common_audio/vad/include/webrtc_vad.h"

#include "ipc_vad.h"
#include "rtc_base/checks.h"

struct ipc_vad_s {
    void* webrtc_handle;
    s32 sample_rate;
    s32 one_frame_size;
};

EXAPI void* ipc_vad_init(s32 sample_rate, s32 level)
{
    struct ipc_vad_s* vad = malloc(sizeof(struct ipc_vad_s));
    if (vad == NULL) {
        return NULL;
    }

    void* handle = WebRtcVad_Create();
    if (handle == NULL) {
        free(vad);
        return NULL;
    }
    WebRtcVad_Init(handle);

    WebRtcVad_set_mode(handle, level);

    vad->webrtc_handle = handle;
    vad->sample_rate = sample_rate;
    vad->one_frame_size = (sample_rate * 2) / 50;
    return vad;
}

EXAPI int ipc_vad_set_level(void* vad_handle, s32 level)
{
    struct ipc_vad_s* vad = (struct ipc_vad_s*)vad_handle;
    if (vad_handle == NULL) {
        return -1;
    }

    return WebRtcVad_set_mode(vad->webrtc_handle, level);
}

EXAPI int ipc_vad_process_ai_frame(void* vad_handle, vptr frame_data, s32 frame_len)
{
    struct ipc_vad_s* vad = (struct ipc_vad_s*)vad_handle;
    if (vad_handle == NULL) {
        return -1;
    }

    if (frame_len < vad->one_frame_size) {
        return WebRtcVad_Process(vad->webrtc_handle, vad->sample_rate, frame_data, frame_len / 2);
    }

    int count = frame_len / vad->one_frame_size;
    int i = 0;
    int ret_count = 0;
    do {
        int ret = WebRtcVad_Process(vad->webrtc_handle, vad->sample_rate, frame_data + i * vad->one_frame_size, vad->one_frame_size / 2) * 2;
        if (ret > 1000 || ret < 0) {
            break;
        }
        ret_count += ret;
        i++;
    } while (i < count);

    return ret_count;
}

EXAPI void ipc_vad_uninit(void* vad_handle)
{
    struct ipc_vad_s* vad = (struct ipc_vad_s*)vad_handle;
    if (vad_handle == NULL) {
        return;
    }

    if (vad->webrtc_handle) {
        WebRtcVad_Free(vad->webrtc_handle);
    }
    free(vad);
}

static vptr g_advance_api_handle;

int ipc_vad_advance_init(s32 sample_rate, s32 level)
{
    if (g_advance_api_handle) {
        return -1;
    }

    g_advance_api_handle = ipc_vad_init(sample_rate, level);
    if (!g_advance_api_handle) {
        return -1;
    }

    return 0;
}

int ipc_vad_advance_set_level(s32 level)
{
    return ipc_vad_set_level(g_advance_api_handle, level);
}

int ipc_vad_advance_process_ai_frame(vptr ai_frame_data, s32 frame_len)
{
    return ipc_vad_process_ai_frame(g_advance_api_handle, ai_frame_data, frame_len);
}

void ipc_vad_advance_uninit(void)
{
    ipc_vad_uninit(g_advance_api_handle);

    g_advance_api_handle = NULL;

}