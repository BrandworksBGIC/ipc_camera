#ifndef __IPC_WPA_H__
#define __IPC_WPA_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>

s32 ipc_wpa_get_password_psk(pv8 ssid, pv8 password, pv8 psk_buffer, s32 psk_buffer_size);

#ifdef __cplusplus
}
#endif

#endif //__IPC_WPA_H__