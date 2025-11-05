#ifndef __IPC_IV_BLE_H__
#define __IPC_IV_BLE_H__
#include "ipc_middleware.h"

int MFG_EnableBluetooth();
void ipc_iv_ble_uninit();

enum IPC_BLE_CON_STATUS_E{
    IPC_BLE_DISCONNECT = 0,
    IPC_BLE_CONNECTED,
    IPC_BLE_CON_FAILED
};

typedef s32 (*ipc_ble_con_status_cb)(s32 con_status);
typedef s32 (*ipc_ble_data_recv_cb)(pu8 data, s32 len);
#endif // !__