#ifndef __IPC_VIDEO_H__
#define __IPC_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_platform_api.h"
#include "ipc_std.h"

s32 ipc_plat_sys_init(IPC_PRODUCT_TYPE type);
s32 ipc_plat_sys_uninit(void);

s32 ipc_plat_video_init(s32 arg);
s32 ipc_plat_video_uninit(void);

s32 ipc_plat_video_query_capability(struct ipc_plat_video_capability* cap);
s32 ipc_plat_video_start(s32 channel, vptr arg);
s32 ipc_plat_video_stop(s32 channel);

s32 ipc_plat_video_recv_frame(s32 channel, ipc_plat_recv_frame_cb_f cb, vptr __user, s32 ms);

s32 ipc_plat_video_request_key_frame(s32 channel);

s32 ipc_plat_video_ctrl(s32 channel, IPC_VIDEO_CTRL_CMD cmd, vptr arg);

s32 ipc_plat_video_isp_image_mode_set(IPC_VIDEO_MODE mode, vptr arg);

s32 __ipc_plat_video_get_resolution(ps32 width, ps32 height);

#ifdef __cplusplus
}
#endif

#endif
