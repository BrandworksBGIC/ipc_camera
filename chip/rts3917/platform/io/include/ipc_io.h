#ifndef __CP_IO_H__
#define __CP_IO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>
#include "ipc_platform_api.h"
#include <stdio.h>

s32 ipc_plat_io_init(struct ipc_io_active_level_flip* flip_table, int num);
s32 ipc_plat_io_uninit(void);
s32 ipc_plat_io_read(IPC_IO_NAME name, IPC_IO_VALUE_TYPE* type);
s32 ipc_plat_io_write(IPC_IO_NAME name, IPC_IO_VALUE_TYPE type);

#ifdef __cplusplus
}
#endif

#endif //__RT_IO_H__