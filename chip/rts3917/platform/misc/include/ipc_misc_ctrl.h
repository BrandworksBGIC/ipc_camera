#ifndef __IPC_MISC_CTRL_H__
#define __IPC_MISC_CTRL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>
#include "ipc_platform_api.h"

s32 ipc_plat_misc_ctrl(IPC_PLAT_MISC_CTRL_CMD cmd, vptr req, vptr rsp);

#ifdef __cplusplus
}
#endif

#endif //__IPC_MISC_CTRL_H__