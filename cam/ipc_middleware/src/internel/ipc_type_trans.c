#include "ipc_type_trans.h"

s32 ipc_trans_channel(IPC_AUDIO_CHANNEL_E channel)
{
    return channel == IPC_AUDIO_CHANNEL_STERO ? 2 : 1;
}

s32 ipc_trans_databits(IPC_AUDIO_DATABITS_E databits)
{
    return databits == IPC_AUDIO_DATABITS_8 ? 8 : 16;
}

s32 ipc_trans_sample(IPC_AUDIO_SAMPLE_E sample)
{
    switch (sample) {
        case IPC_AUDIO_SAMPLE_11K: sample = 11025; break;
        case IPC_AUDIO_SAMPLE_12K: sample = 12050; break;
        case IPC_AUDIO_SAMPLE_16K: sample = 16000; break;
        default: sample = 8000; break;
    }
    return sample;
}
