#include "ipc_audio.h"
#include "ipc_audio_ai.h"
#include "ipc_audio_ao.h"

s32 ipc_plat_audio_init(struct ipc_plat_audio_init_attr* attr)
{
    s32 ret = 0;
    ipc_plat_audio_ai_init(attr);
    ipc_plat_audio_ao_init(attr);

    if (attr->enable_aec) { }

    return ret;
}

s32 ipc_plat_audio_uninit(void)
{
    s32 ret = 0;

    ipc_plat_audio_ai_uninit();
    ipc_plat_audio_ao_uninit();

    return ret;
}

s32 ipc_plat_audio_query_capability(struct ipc_plat_audio_capability* cap)
{
    cap->audio_enc_support = IPC_AUDIO_ENC_TYPE_PCM;
    cap->channel           = IPC_AUDIO_CHANNEL_MONO;
    cap->databits          = IPC_AUDIO_DATABITS_16;
    cap->is_support_aec    = 1;
    cap->sample_support    = IPC_AUDIO_SAMPLE_8K | IPC_AUDIO_SAMPLE_16K;
    cap->default_ai_vol    = 70;
    cap->default_ai_gain   = 70;
    cap->default_ao_vol    = 100;

    return 0;
}

s32 ipc_plat_audio_start(s32 enable_ai, s32 enable_ao)
{
    s32 ret = 0;
    if (enable_ai) {
        ret = ipc_plat_audio_start_ai();
    }

    if (enable_ao) {
        ret |= ipc_plat_audio_start_ao();
    }

    return ret;
}

s32 ipc_plat_audio_stop(s32 disable_ai, s32 disable_ao)
{
    s32 ret = 0;
    if (disable_ai) {
        ret = ipc_plat_audio_stop_ai();
    }

    if (disable_ao) {
        ret |= ipc_plat_audio_stop_ao();
    }

    return ret;
}

s32 ipc_plat_audio_set_vol(IPC_AUDIO_DEV dev, s32 gain, s32 vol)
{
    if (vol < 0 || vol > 100) {
        printf("vol error[%d]\n", vol);
        return -1;
    }

    if (IPC_AUDIO_DEV_INPUT == dev) {
        __audio_ai_set_vol(gain, vol);
    } else if (IPC_AUDIO_DEV_OUTPUT == dev) {
        __audio_ao_set_vol(gain, vol);
    }

    return 0;
}