#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "ipc_memory.h"

// TODO: Memory pool or memory monitoring
vptr ipc_malloc(u32 total_size, u32 clean_size)
{
    vptr h_mem = malloc(total_size);
    if (h_mem == NULL)
        return NULL;

    if (clean_size) {
        if (clean_size > total_size)
            clean_size = total_size;
        memset(h_mem, 0, clean_size);
    }

    return h_mem;
}

void ipc_free(vptr h_mem)
{
    free(h_mem);
}

vptr ipc_realloc(vptr h_mem, u32 new_size)
{
    return realloc(h_mem, new_size);
}

/************************* shm ***********************/
#define KEY_PATH "/"

vptr ipc_shmalloc(u8 mark, u32 size, pu8 first_create)
{
    *first_create = 1;
    key_t key     = ftok(KEY_PATH, mark);
    s32 shmid     = shmget(key, size, IPC_CREAT | 0666 | IPC_EXCL);
    if (shmid == -1) {
        shmid = shmget(key, size, IPC_CREAT | 0666);
        if (shmid == -1)
            return NULL;
        *first_create = 0;
    }

    vptr h_mem = shmat(shmid, 0, 0);
    if (h_mem == (void*)-1)
        return NULL;

    return h_mem;
}

vptr ipc_shmref(u8 mark)
{
    s32 shmid = shmget(ftok(KEY_PATH, mark), 0, 0); /* mark convert shmid */
    if (shmid == -1)
        return NULL;

    vptr h_mem = shmat(shmid, 0, 0);
    if (h_mem == (void*)-1)
        return NULL;

    return h_mem;
}

void ipc_shmunref(vptr h_mem)
{
    if (h_mem != NULL)
        shmdt(h_mem);
}

void ipc_shmfree(u8 mark, vptr h_mem)
{
    s32 shmid = shmget(ftok(KEY_PATH, mark), 0, 0); /* mark convert shmid */
    if (shmid == -1)
        return;

    ipc_shmunref(h_mem);

    shmctl(shmid, IPC_RMID, NULL);
}
