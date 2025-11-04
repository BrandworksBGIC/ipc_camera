#ifndef __IPC_PING_H__
#define __IPC_PING_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

s32 ipc_ping4_init(pv8 net_dev);
s32 ipc_ping4(s32 fd, pv8 ip, s32 time_out_tms);
void ipc_ping4_uninit(s32 fd);
s32 ipc_get_route(pv8 ip, pv8 net_dev);

#ifdef __cplusplus
}
#endif

#endif //__IPC_PING_H__