#include "ipc_iter.h"

void ipc_iter_break_off(ipc_iter_p h_iter)
{
    if (!h_iter) return ;

    while(!h_iter->is_end) h_iter++; 

    do {
        if (h_iter->f_iter_uninit) {
            h_iter->f_iter_uninit(h_iter);
            h_iter->f_iter_uninit = NULL;
        }
    } while(!h_iter->is_start && h_iter--);
}

s32 ipc_iter_retval(ipc_iter_t *h_iter)
{
    if (!h_iter) return IPC_INVALID_ARGS;

    s32 idx = -1;
    do {
        idx++;
        if (h_iter[idx]->ret < 0) return h_iter[idx]->ret;
    } while(!h_iter[idx]->is_end);

    return IPC_SUCCESS;
}
