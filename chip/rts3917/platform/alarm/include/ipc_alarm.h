#ifndef __IPC_ALARM_H__
#define __IPC_ALARM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>


#include "ipc_platform_api.h"

s32 ipc_plat_alarm_init(IPC_PLAT_ALARM_TYPE* support_alarm_type);

s32 ipc_plat_alarm_uninit(void);

s32 ipc_plat_alarm_ctrl(IPC_PLAT_ALARM_CTRL_CMD cmd, vptr arg);

s32 ipc_plat_alarm_recv_result(struct ipc_plat_alarm_result_s* result, s32 timeout_ms);

s32 ipc_plat_alarm_release_result(struct ipc_plat_alarm_result_s* result);

#ifdef __cplusplus
}
#endif

#endif //__IPC_ALARM_H__