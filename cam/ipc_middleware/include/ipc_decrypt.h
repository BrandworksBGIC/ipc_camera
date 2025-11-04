#ifndef __IPC_DECRYPT_H__
#define __IPC_DECRYPT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

typedef struct {
    u64 device_id;
    u16 client_id;
    u16 product_type;  
} ipc_decrypt_ininfo_t, *ipc_decrypt_ininfo_p;

EXAPI ipc_decrypt_ininfo_p ipc_decrypt_ininfo(void);

typedef struct {
    u16 cloud_type;
    u16  info_len;
    u8  info_ctx[0];
} ipc_decrypt_exinfo_t, *ipc_decrypt_exinfo_p;

EXAPI ipc_decrypt_exinfo_p ipc_decrypt_exinfo(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_DECRYPT_H__