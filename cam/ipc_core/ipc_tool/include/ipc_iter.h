#ifndef __IPC_ITER_H__
#define __IPC_ITER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_std.h>
#include "ipc_log.h"

typedef struct ipc_iter {
    u8   is_start;                               ///< The first in the associated iterator collection
    u8   is_end;                                 ///< The last one in the associated iterator collection
    ipc_log_p fa_log;                             ///< Parent node log handle
    void (*f_iter_uninit)(struct ipc_iter* h_iter); ///< Deinitialization function, to be filled by the iterator implementer
    s32  ret;                                    ///< Return value after the end of the loop
    v8 private[128]; ///< Private to the implementer
} ipc_iter_t[1], *ipc_iter_p;

typedef enum {
    IPC_ITER_BREAK    = 0,
    IPC_ITER_CONTINUE = 1,
} ipc_iter_ctrl_e;

#define ITER_INIT(iter, num) ipc_iter_t iter[num];                  \
                             memset(&iter, 0, sizeof(iter));       \
                             iter[0]->is_start = 1;                \
                             iter[num-1]->is_end = 1;              \
                             for (s32 idx = 0; idx < num; idx++) { \
                                iter[idx]->fa_log = __IPC_LOG__;    \
                             }

/* If the loop needs to be terminated early from the outside, this function needs to be called for resource destruction. The iterator implementer needs to call this function for resource destruction at the end. */
EXAPI void ipc_iter_break_off(ipc_iter_p h_iter);
EXAPI s32  ipc_iter_retval(ipc_iter_t *h_iter);

#ifdef __cplusplus
}
#endif

#endif //__IPC_ITER_H__
