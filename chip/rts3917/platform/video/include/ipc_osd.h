#ifndef __RT_OSD_H__
#define __RT_OSD_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

#include "ipc_platform_api.h"

s32 ipc_plat_video_osd_init(struct ipc_video_osd_attr_s* attr);
s32 ipc_plat_video_osd_uninit(void);
s32 ipc_plat_video_osd_set(s32 channel, s32 rgn_num, s32 is_show, void* data, s32 data_len);

s32 __osd_internel_init(void);

s32 __osd_internel_uninit(void);

#ifdef __cplusplus
}
#endif

#endif //__RT_OSD_H__