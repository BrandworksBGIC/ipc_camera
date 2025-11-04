#include "ipc_sal_api.h"

static ipc_middleware_sal_cb_t _g_ipc_sal_api = {
    .f_ipc_sal_media_info     = ipc_middleware_sal_media_info,
    .f_ipc_sal_get_media_iter = ipc_middleware_sal_get_media_iter,
};

void ipc_middleware_sal_hook(ipc_middleware_sal_cb_p p_cb)
{
#define CHECK_AND_SET(fun) _g_ipc_sal_api.fun = (vptr)p_cb->fun ? : NOT_DO_ANYTHING
    CHECK_AND_SET(f_ipc_sal_media_info);
    CHECK_AND_SET(f_ipc_sal_get_media_iter);
}

ipc_middleware_sal_cb_p ipc_middleware_sal_api(void) 
{
    return &_g_ipc_sal_api;
}