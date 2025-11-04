#ifndef __IPC_KEY_MANAGE_H__
#define __IPC_KEY_MANAGE_H__

#include "ipc_std.h"

EXAPI s32 key_manage_init(void);
EXAPI s32 key_manage_delete_conf_key_1(void);
EXAPI s32 key_manage_decrypt_with_conf_key_1(pv8 path, pv8 data, s32 len);
EXAPI s32 key_manage_encrypt_with_conf_key_1(pv8 path, pv8 data, s32 len);

EXAPI s32 key_manage_decrypt_with_root_key(pu8 data, s32 len);
EXAPI s32 key_manage_encrypt_with_root_key(pu8 data, s32 len);

EXAPI s32 key_manage_delete_conf_key_2(void);
EXAPI s32 key_manage_create_conf_key_2(pv8 seed);
EXAPI s32 key_manage_encrypt_with_conf_key_2(pu8 data, s32 len);
EXAPI s32 key_manage_decrypt_with_conf_key_2(pu8 data, s32 len);

#endif