
#ifndef __IPC_HANDLER_H__
#define __IPC_HANDLER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "ipc_middleware.h"
typedef struct {
    v8 device_Id[20];
    v8 access_key[64];
    v8 pin[20];
    v8 pn [8];
    v8 sn[24];
    v8 mac[20];
    v8 firmware_version[32];
} ipc_dev_info_t, *ipc_dev_info_p;
typedef struct
{
    u8 mac[6]; /* mac address */
} nw_mac_t, *nw_mac_p;

ipc_dev_info_p ipc_handler_get_dev_info(void);
void ipc_handler_read_int(pv8 key, ps32 value);
void ipc_handler_write_int(pv8 key, s32 value);
void ipc_handler_led_flashing(s32 blue_flash_ms, s32 red_flash_ms);
int MFG_GetWifiIPAddress(char* ip_addr, const int ip_addr_size);
s32 ipc_handler_get_mac(nw_mac_p mac);
#ifdef __cplusplus
}
#endif

#endif //__IPC_HANDLER_H__