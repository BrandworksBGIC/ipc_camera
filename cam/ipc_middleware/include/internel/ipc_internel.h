#ifndef __IPC_INTERNEL_H__
#define __IPC_INTERNEL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

s32 ipc_mpp_net_wireless_io_ctrl(s32 on_off);

s32 ipc_mpp_eth_rst_io_ctrl(void);


#ifdef __cplusplus
}
#endif

#endif //__IPC_INTERNEL_H__