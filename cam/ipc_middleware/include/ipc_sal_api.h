#ifndef __IPC_SAL_API_H__
#define __IPC_SAL_API_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>
#include "ipc_middleware_sal.h"

typedef struct {
    vptr (*f_ipc_sal_media_info)(struct ipc_plat_audio_init_attr *audio_attr);                                 ///< 见 ipc_middleware_sal_media_info
} ipc_middleware_sal_cb_t, *ipc_middleware_sal_cb_p;

EXAPI void ipc_middleware_sal_hook(ipc_middleware_sal_cb_p p_cb);
ipc_middleware_sal_cb_p ipc_middleware_sal_api(void);

#ifdef __cplusplus
}
#endif

#endif //__IPC_SAL_API_H__