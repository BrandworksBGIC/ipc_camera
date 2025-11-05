#ifndef __RT_AUDIO_AO_H__
#define __RT_AUDIO_AO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

#include "ipc_platform_api.h"

s32 ipc_plat_audio_ao_init(struct ipc_plat_audio_init_attr* attr);
s32 ipc_plat_audio_ao_uninit(void);
s32 ipc_plat_audio_start_ao(void);
s32 ipc_plat_audio_stop_ao(void);
s32 __audio_ao_set_vol(int gain, int vol);

#ifdef __cplusplus
}
#endif

#endif //__RT_AUDIO_AO_H__