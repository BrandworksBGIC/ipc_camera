#ifndef __IPC_UNIX_SOCKET_H__
#define __IPC_UNIX_SOCKET_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

vptr ipc_unix_socket_open(pv8 src_path, pv8 dest_path);
vptr ipc_unix_socket_recv_only_open(pv8 src_path);
void ipc_unix_socket_close(vptr h_this);
s32  ipc_unix_socket_send(vptr h_this, vptr buf, s32 len);
s32  ipc_unix_socket_recv(vptr h_this, vptr buf, s32 max, s32 timeout_ms);

#ifdef __cplusplus
}
#endif

#endif //__IPC_UNIX_SOCKET_H__