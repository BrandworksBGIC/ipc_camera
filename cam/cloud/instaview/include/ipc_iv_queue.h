#ifndef __IPC_IV_QUEUE_H__
#define __IPC_IV_QUEUE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "ipc_middleware.h"


void ipc_iv_queue_uninit_aac_encode(void);
s32 ipc_iv_queue_uninit(void);
s32 ipc_iv_queue_init_aac_encode(void);
int MFG_EncodeAACAudio(char *pcm_data, int32_t pcm_len, char *encoded_data, int32_t encoded_len);
#ifdef __cplusplus
}
#endif

#endif //__IPC_IV_QUEUE_H__