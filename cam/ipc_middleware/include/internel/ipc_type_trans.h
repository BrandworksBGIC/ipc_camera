#ifndef __IPC_TYPE_TRANS_H__
#define __IPC_TYPE_TRANS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>
#include "ipc_platform_api.h"

s32 ipc_trans_channel(IPC_AUDIO_CHANNEL_E channel);
s32 ipc_trans_databits(IPC_AUDIO_DATABITS_E databits);
s32 ipc_trans_sample(IPC_AUDIO_SAMPLE_E sample);

#ifdef __cplusplus
}
#endif

#endif //__IPC_TYPE_TRANS_H__