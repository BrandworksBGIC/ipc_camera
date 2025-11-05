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
#include <rtsavdef.h>
#include <rtscamkit.h>
#include <rtsaudio.h>

#include "ipc_audio_ai.h"

typedef struct ipc_audio_ai_chn {
    s32 capture_chn;
    s32 resample_chn;
    s32 recv_chn;
} IPC_AUDIO_AI_CHN_S;

static IPC_AUDIO_AI_CHN_S _g_ai_chn = { .capture_chn = -1, .resample_chn = -1, .recv_chn = -1 };

s32 __audio_ai_set_vol(int gain, int vol)
{
    int ret = 0;

    ret = rts_audio_set_capture_volume(vol);
    printf("%s vol[%d:%d]\n", __func__, vol, ret);

    ret = rts_audio_set_capture_analog_volume(gain);
    printf("%s gain[%d:%d]\n", __func__, gain, ret);

    return ret;
}

s32 ipc_plat_audio_ai_init(struct ipc_plat_audio_init_attr* attr)
{
    printf("%s:%d\n", __func__, __LINE__);

    int ret = 0;
    struct rts_audio_attr ai_attr;
    s32 resample_rate = 8000;

    memset(&ai_attr, 0, sizeof(ai_attr));
    snprintf(ai_attr.dev_node, sizeof(ai_attr.dev_node), "hw:0,1");
    switch (attr->sample) {
        case IPC_AUDIO_SAMPLE_8K:
            resample_rate = 8000;
            break;
        case IPC_AUDIO_SAMPLE_11K:
            resample_rate = 11000;
            break;
        case IPC_AUDIO_SAMPLE_12K:
            resample_rate = 12000;
            break;
        case IPC_AUDIO_SAMPLE_16K:
            resample_rate = 16000;
            break;
        default:
            break;
    }

    if (attr->channel == IPC_AUDIO_CHANNEL_MONO) {
        ai_attr.channels = 1;
    } else {
        ai_attr.channels = 2;
    }

    /* capture defaults to a 16K sampling rate, because it's not possible to enable AEC and NS without using a 16K
     * sampling rate */
    ai_attr.rate          = 16000;
    ai_attr.format        = 16;
    ai_attr.period_frames = ai_attr.rate / attr->frame_rate;
    _g_ai_chn.capture_chn = rts_av_create_audio_capture_chn(&ai_attr);
    if (RTS_IS_ERR(_g_ai_chn.capture_chn)) {
        ret = -1;
        printf("create audio capture chn error\n");
        goto end;
    }
    printf("audio capture chn : %d\n", _g_ai_chn.capture_chn);

    if (IPC_AUDIO_SAMPLE_16K != attr->sample) {
        _g_ai_chn.resample_chn = rts_av_create_audio_resample_chn(resample_rate, ai_attr.format, ai_attr.channels);

        ret = rts_av_bind(_g_ai_chn.capture_chn, _g_ai_chn.resample_chn);
        if (ret) {
            RTS_ERR("fail to bind aec and resample, ret = %d\n", ret);
            goto end;
        }

        _g_ai_chn.recv_chn = _g_ai_chn.resample_chn;
    } else {
        _g_ai_chn.recv_chn = _g_ai_chn.capture_chn;
    }

    __audio_ai_set_vol(attr->ai_gain, attr->ai_vol);

end:
    return ret;
}

s32 ipc_plat_audio_ai_uninit(void)
{
    printf("%s:%d\n", __func__, __LINE__);
    int ret = 0;

    RTS_SAFE_CLOSE(_g_ai_chn.capture_chn, rts_av_destroy_chn);
    RTS_SAFE_CLOSE(_g_ai_chn.resample_chn, rts_av_destroy_chn);

    return ret;
}

s32 ipc_plat_audio_start_ai(void)
{
    printf("%s:%d\n", __func__, __LINE__);
    int ret = 0;

    // coverity[CHECKED_RETURN :SUPPRESS]
    ret = rts_av_enable_chn(_g_ai_chn.capture_chn);
    if (RTS_IS_ERR(ret)) {
        printf("enable audio capture fail, ret = %d\n", ret);
        goto exit;
    }

    if (_g_ai_chn.resample_chn >= 0) {
        // coverity[CHECKED_RETURN :SUPPRESS]
        ret = rts_av_enable_chn(_g_ai_chn.resample_chn);
        if (RTS_IS_ERR(ret)) {
            printf("enable audio resampl fail, ret = %d\n", ret);
            goto exit;
        }
    }

    /* Only 16K sampling rate can enable AEC and NS */
    struct rts_audio_capture_vqe c_vqe;
    memset(&c_vqe, 0, sizeof(c_vqe));
    c_vqe.aecns_enable          = 1;
    c_vqe.aecns_attr.aec_enable = 1;
    c_vqe.aecns_attr.ns_enable  = 1;
    ret                         = rts_av_set_audio_capture_vqe(_g_ai_chn.capture_chn, &c_vqe);
    if (RTS_IS_ERR(ret)) {
        printf("Error, enable audio aec ns fail, ret = %d\n", ret);
        goto exit;
    }

    rts_av_start_recv(_g_ai_chn.recv_chn);
exit:
    return ret;
}

s32 ipc_plat_audio_stop_ai(void)
{
    printf("%s:%d\n", __func__, __LINE__);
    int ret = 0;

    if (_g_ai_chn.recv_chn >= 0)
        rts_av_stop_recv(_g_ai_chn.recv_chn);

    if (_g_ai_chn.capture_chn >= 0)
        rts_av_disable_chn(_g_ai_chn.capture_chn);
    if (_g_ai_chn.resample_chn >= 0)
        rts_av_disable_chn(_g_ai_chn.resample_chn);

    return ret;
}

s32 ipc_plat_audio_ai_recv_frame(ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms)
{
    int ret                             = 0;
    struct rts_av_buffer* buffer = NULL;

    ret = rts_av_recv_block(_g_ai_chn.recv_chn, &buffer, 100);
    if (ret /*RTS_IS_ERR(rts_av_recv_block(_g_ai_chn.recv_chn, &buffer, 100))*/) {
        printf("[%s:%d]Error, rts_av_recv_block failed, chn: %d, ret:%d\n", __func__, __LINE__, _g_ai_chn.recv_chn,
               ret);
        return -1;
    }

    if (buffer == NULL) {
        printf("%s:%d:%d:%d get ai data error\n", __func__, __LINE__, _g_ai_chn.recv_chn, ret);
        return -2;
    }

    struct ipc_frame_data_s frame = { 0 };
    frame.pack_num              = 1;
    frame.timestamp             = buffer->timestamp / 1000;
    frame.pack[0].data          = buffer->vm_addr;
    frame.pack[0].data_len      = buffer->bytesused;

    cb(&frame, __user);

    RTS_SAFE_RELEASE(buffer, rts_av_put_buffer);

    return 0;
}