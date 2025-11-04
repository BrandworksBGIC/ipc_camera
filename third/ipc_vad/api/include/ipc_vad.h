#ifndef __IPC_VAD_H__
#define __IPC_VAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

EXAPI void* ipc_vad_init(s32 sample_rate, s32 level);

EXAPI int ipc_vad_set_level(void* vad_handle, s32 level);

EXAPI int ipc_vad_process_ai_frame(void* avd_handle, vptr ai_frame_data, s32 frame_len);

EXAPI void ipc_vad_uninit(void* vad_handle);


EXAPI int ipc_vad_advance_init(s32 sample_rate, s32 level);

EXAPI int ipc_vad_advance_set_level(s32 level);

EXAPI int ipc_vad_advance_process_ai_frame(vptr ai_frame_data, s32 frame_len);

EXAPI void ipc_vad_advance_uninit(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_AEC_H__