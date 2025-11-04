#ifndef __IPC_GETOPT_H__
#define __IPC_GETOPT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ipc_iter.h"
#include <ipc_std.h>

typedef struct {
    v8 tag;         ///< Abbreviation for a single letter
    pv8 name;       ///< name
    u8 ignore_case; ///< Whether to ignore case
} ipc_opt_attr_t, *ipc_opt_attr_p;

EXAPI s32 ipc_getopt_iter(ipc_iter_p h_iter, s32 argc, pcv8 argv[], ipc_opt_attr_p p_attr, ps32 attr_num);

#ifdef __cplusplus
}
#endif

#endif //__IPC_GETOPT_H__
