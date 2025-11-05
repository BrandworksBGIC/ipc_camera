#ifndef __IPC_AUDIO_H__
#define __IPC_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_platform_api.h"
#include "ipc_std.h"

s32 ipc_plat_audio_init(struct ipc_plat_audio_init_attr* attr);

s32 ipc_plat_audio_uninit(void);

s32 ipc_plat_audio_query_capability(struct ipc_plat_audio_capability* cap);
s32 ipc_plat_audio_start(s32 enable_ai, s32 enable_ao);
s32 ipc_plat_audio_stop(s32 disable_ai, s32 disable_ao);

s32 ipc_plat_audio_ai_recv_frame(ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms);

s32 ipc_plat_audio_ao_send_frame(struct ipc_frame_pack_s *fpack, s32 ms);

s32 ipc_plat_audio_ao_flush_buffer(void);

s32 ipc_plat_audio_set_vol(IPC_AUDIO_DEV dev, s32 gain, s32 vol);

#ifdef __cplusplus
}
#endif

#endif
