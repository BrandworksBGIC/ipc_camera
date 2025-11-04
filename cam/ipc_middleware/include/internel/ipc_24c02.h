#ifndef __IPC_24C02_H__
#define __IPC_24C02_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

s32 ipc_24c02_read(pv8 dev_node, pu8 buf, s32 len, u32 reg_width);

#ifdef __cplusplus
}
#endif

#endif //__IPC_24C02_H__