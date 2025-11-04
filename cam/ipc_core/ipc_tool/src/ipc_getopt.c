#include "ipc_getopt.h"

typedef struct {
    s32 nattr;  ///< Initial attr quantity (attr_num bits are multiplexed)
    s32 is_tag; ///< Indicates that the current parsing is in single-letter mode
    s32 idx;    ///< The index of the parameter to be parsed
    s32 sub;    ///< The index within the parameter to be parsed argv[idx][sub]
} getopt_t, *getopt_p;

s32 ipc_getopt_iter(ipc_iter_p h_iter, s32 argc, pcv8 argv[], ipc_opt_attr_p p_attr, ps32 attr_num)
{
    if (!h_iter || argc <= 0 || !argv || !p_attr || !attr_num)
        return IPC_ITER_BREAK;

    getopt_p h_opt = (getopt_p)h_iter->private;
    if (!h_opt->idx && !h_opt->sub)
        h_opt->nattr = *attr_num;

RERUN:
    if (h_opt->idx >= argc)
        return IPC_ITER_BREAK; /* Parse complete */

    if (h_opt->sub == 0) { /* At the beginning of each parameter parsing */
        while (argv[h_opt->idx][h_opt->sub] == '-')
            h_opt->sub++;      /* Calculate the number of '-' */
        if (h_opt->sub == 0) { /* No '-' */
            h_opt->idx++;
            goto RERUN;
        }
        h_opt->is_tag = h_opt->sub == 1;
    }

    s32 idx = 0;
    if (h_opt->is_tag) { /* Short match */
        for (idx = 0; idx < h_opt->nattr; idx++) {
            if (!p_attr[idx].tag)
                continue; /* Skip */
            if (p_attr[idx].ignore_case ? toupper(argv[h_opt->idx][h_opt->sub]) == toupper(p_attr[idx].tag)
                                        : argv[h_opt->idx][h_opt->sub] == p_attr[idx].tag) {
                *attr_num = idx;
                break;
            }
        }
        h_opt->sub++;                               /* Will parse the next sub */
        if (argv[h_opt->idx][h_opt->sub] == '\0') { /* No next sub, point to the next idx */
            h_opt->idx++;
            h_opt->sub = 0;
        }
        if (idx >= h_opt->nattr)
            goto RERUN; /* Not found */
        return IPC_ITER_CONTINUE;
    }

    for (idx = 0; idx < h_opt->nattr; idx++) { /* Long match */
        if (!p_attr[idx].name)
            continue; /* Empty, skip mismatch */
        if (p_attr[idx].ignore_case ? !strcasecmp(&argv[h_opt->idx][h_opt->sub], p_attr[idx].name)
                                    : !strcmp(&argv[h_opt->idx][h_opt->sub], p_attr[idx].name)) {
            *attr_num = idx;
            break;
        }
    }
    h_opt->idx++;
    h_opt->sub = 0;
    if (idx >= h_opt->nattr)
        goto RERUN; /* Not found */
    return IPC_ITER_CONTINUE;
}

#ifdef GETOPT

s32 main(s32 argc, pcv8 argv[])
{

    ipc_opt_attr_t attr[] = {
        { 'a', NULL, 0 },
        { 's', "stop", 1 },
        { 'd', "dump", 0 },
        { '8', "xx", 1 },
    };

    s32 idx = ARRSIZE(attr);
    ITER_INIT(iter, 1);
    while (ipc_getopt_iter(iter[0], argc - 1, argv + 1, attr, &idx)) {
        printf("idx=[%d], tag=[%c], name=[%s]\n", idx, attr[idx].tag, attr[idx].name);
    }
}

#endif
