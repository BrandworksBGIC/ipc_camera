#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <unistd.h>

#include <rtsamixer.h>
#include <rtsavapi.h>
#include <rtscamkit.h>
#include <rtsaudio.h>

#include "ipc_audio_ai.h"

#include "ipc_audio.h"

static int g_ao_playback_chn   = -1;
static s32 _g_ao_mixer_chn     = -1;
static int g_buffer_count      = 0;
static int one_sec_ao_data_len = 8000 * 2;

static int is_max_buffer_count(void)
{
    return g_buffer_count > 50;
}

static void recycle_buffer(void* master, struct rts_av_buffer* buffer)
{
    int* buffer_count = master;

    // printf("%s:%d\n", __func__, g_buffer_count);

    RTS_SAFE_RELEASE(buffer, rts_av_delete_buffer);

    if (buffer_count)
        atomic_dec(buffer_count);
}

s32 __audio_ao_set_vol(int gain, int vol)
{

    int ret = 0;

    if (vol > 0) {
        vol = 50 + vol / 2;
    }

    ret = rts_audio_set_playback_volume(vol);
    printf("%s:%d:%d\n", __func__, vol, ret);

    return ret;
}

s32 ipc_plat_audio_ao_init(struct ipc_plat_audio_init_attr* attr)
{
    int ret = 0;
    struct rts_audio_attr ao_attr;

    memset(&ao_attr, 0, sizeof(ao_attr));
    snprintf(ao_attr.dev_node, sizeof(ao_attr.dev_node), "hw:0,1");
    switch (attr->sample) {
        case IPC_AUDIO_SAMPLE_8K:
            ao_attr.rate = 8000;
            break;
        case IPC_AUDIO_SAMPLE_11K:
            ao_attr.rate = 11000;
            break;
        case IPC_AUDIO_SAMPLE_12K:
            ao_attr.rate = 12000;
            break;
        case IPC_AUDIO_SAMPLE_16K:
            ao_attr.rate = 16000;
            break;
        default:
            break;
    }

    ao_attr.format        = 16;
    ao_attr.period_frames = ao_attr.rate / attr->frame_rate;

    if (attr->channel == IPC_AUDIO_CHANNEL_MONO) {
        ao_attr.channels = 1;
    } else {
        ao_attr.channels = 2;
    }

    g_ao_playback_chn = rts_av_create_audio_playback_chn(&ao_attr);
    if (RTS_IS_ERR(g_ao_playback_chn)) {
        printf("create audio playback chn fail, ret = %d\n", g_ao_playback_chn);
    }

    if (attr->enable_aec) {
        _g_ao_mixer_chn = rts_av_create_audio_mixer_chn();
        if (RTS_IS_ERR(_g_ao_mixer_chn)) {
            ret = _g_ao_mixer_chn;
            goto exit;
        }

        ret = rts_av_bind(_g_ao_mixer_chn, g_ao_playback_chn);
        if (ret) {
            printf("Error, fail to bind mixer and playback, ret = %d\n", ret);
            goto exit;
        }
    }

    __audio_ao_set_vol(0, attr->ao_vol);

    one_sec_ao_data_len = ao_attr.rate * ao_attr.channels * 2;

exit:
    return ret;
}

s32 ipc_plat_audio_ao_uninit(void)
{
    s32 ret = 0;
    RTS_SAFE_CLOSE(_g_ao_mixer_chn, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(g_ao_playback_chn, rts_av_destroy_chn);
    return ret;
}

s32 ipc_plat_audio_start_ao(void)
{
    s32 ret = 0;

    if (_g_ao_mixer_chn >= 0) {
        // coverity[CHECKED_RETURN :SUPPRESS]
        ret = rts_av_enable_chn(_g_ao_mixer_chn);
        if (RTS_IS_ERR(ret)) {
            printf("enable audio playback fail, ret = %d\n", ret);
            goto exit1;
        }
    }
    // coverity[CHECKED_RETURN :SUPPRESS]
    ret = rts_av_enable_chn(g_ao_playback_chn);
    if (RTS_IS_ERR(ret)) {
        printf("enable audio playback fail, ret = %d\n", ret);
        goto exit1;
    }

    if (_g_ao_mixer_chn >= 0) {
        rts_av_start_send(_g_ao_mixer_chn);
    } else {
        rts_av_start_send(g_ao_playback_chn);
    }

    // ret = rts_av_start_recv(g_ao_playback_chn);
    // if (RTS_IS_ERR(ret)) {
    //     printf("rts_av_start_recv audio playback fail, ret = %d\n", ret);
    //     goto exit1;
    // }
exit1:
    return ret;
}

s32 ipc_plat_audio_stop_ao(void)
{
    s32 ret = 0;

    // rts_av_stop_recv(g_ao_playback_chn);
    if (_g_ao_mixer_chn >= 0) {
        rts_av_stop_send(_g_ao_mixer_chn);
    } else {
        rts_av_stop_send(g_ao_playback_chn);
    }

    if (_g_ao_mixer_chn >= 0) {
        rts_av_disable_chn(_g_ao_mixer_chn);
    }

    if (g_ao_playback_chn >= 0) {
        rts_av_disable_chn(g_ao_playback_chn);
    }

    return ret;
}

s32 ipc_plat_audio_ao_send_frame(struct ipc_frame_pack_s* fpack, s32 ms)
{
    s32 ret = 0;

    // printf("%s:%d:%d:%ld:%d\n", __func__, rts_av_is_idle(g_ao_playback_chn), g_buffer_count, time(NULL),
    // fpack->data_len);

    // During real-time intercom, once the buffer reaches a certain amount, discard the data frames directly.
    if (ms == 0 && is_max_buffer_count()) {
        // printf("%s:%d\n", __func__, __LINE__);
        return -1;
    }

    if (g_buffer_count > 150) {
        printf("%s:%d\n", __func__, __LINE__);
        return -1;
    }

    struct rts_av_buffer* buffer = NULL;
    buffer                       = rts_av_new_buffer(fpack->data_len);
    if (!buffer) {
        RTS_ERR("alloc buffer fail\n");
        return -1;
    }

    rts_av_set_buffer_callback(buffer, &g_buffer_count, recycle_buffer);
    atomic_inc(&g_buffer_count);
    rts_av_get_buffer(buffer);

    memcpy(buffer->vm_addr, fpack->data, fpack->data_len);

    buffer->bytesused = fpack->data_len;
    buffer->timestamp = 0;

    if (_g_ao_mixer_chn >= 0) {
        rts_av_send(_g_ao_mixer_chn, buffer);
    } else {
        rts_av_send(g_ao_playback_chn, buffer);
    }

    RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

    if (ms > 0) {
        // For delayed requests, which are voice prompts, 3903 must introduce its own delay; the buffer will not be able
        // to release itself for a period of time, so a frame time needs to be waited out.
        usleep(1000 * 1000 / (one_sec_ao_data_len / fpack->data_len));
    }

    return ret;
}

s32 ipc_plat_audio_ao_flush_buffer(void)
{
    s32 ret   = 0;
    int count = 20;
    while ((!rts_av_is_idle(g_ao_playback_chn)) && count > 0) {
        usleep(500 * 1000);
        count--;
    }

    sleep(2);

    printf("%s\n", __func__);
    return ret;
}

s32 __get_ao_playback_chn(void)
{
    return g_ao_playback_chn;
}

s32 __get_ao_playback_chn_is_idle(void)
{
    return rts_av_is_idle(g_ao_playback_chn) && (g_buffer_count == 0);
}