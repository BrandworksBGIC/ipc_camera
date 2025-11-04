#include "ipc_timer.h"
#include "ipc_memory.h"
#include "ipc_misc.h"
#include "ipc_thread.h"
#include "ipc_time.h"

typedef struct {
    u16 heap_idx;      ///< The current node's index in the min-heap (corresponding heap node index)
    u16 task_run : 1;  ///< Task running flag
    u16 task_seq : 15; ///< Current task sequence (incremented by 1 each time the node is reoccupied, used to determine
                       ///< if the task has expired)
    ipc_timer_task_f f_task; ///< Task callback function
    vptr usr_arg;           ///< User argument for the task callback
    u64 target_tms;         ///< Target time for the next trigger of the task
} ipc_timer_t, *ipc_timer_p;

typedef struct {
    u8 gorun;             ///< Thread control flag
    u8 alive;             ///< Thread alive feedback
    ipc_lock_t mutex;      ///< Mutex lock
    ipc_cond_t cond;       ///< Condition variable for waking up
    s32 tmp_mem_size;     ///< Size of temporary memory block per timer
    u16 timer_num;        ///< Number of currently used timers
    u16 timer_max;        ///< Maximum number of used timers
    ipc_timer_t timers[0]; ///< Timers
    ///< This is a min-heap
    ///< Here are the index-mapped tmp_mem blocks for all timers (ensure each memory block address is aligned)
} ipc_timer_bus_t, *ipc_timer_bus_p;

static u16 _float_up(pu16 heap, ipc_timer_p timers, u16 c_idx)
{
    u16 timer_idx;
    u16 f_idx = 0;

    while (c_idx) {
        f_idx = (c_idx - 1) >> 1;
        if (timers[heap[c_idx]].target_tms >= timers[heap[f_idx]].target_tms)
            break; /* Float-up complete */
        timer_idx                    = heap[c_idx];
        heap[c_idx]                  = heap[f_idx];
        heap[f_idx]                  = timer_idx;
        timers[heap[c_idx]].heap_idx = c_idx; // Update heap_idx in the corresponding timer
        c_idx                        = f_idx;
    }

    timers[heap[c_idx]].heap_idx = c_idx; // Update heap_idx in the corresponding timer
    return c_idx;
}

static u16 _sink_down(pu16 heap, ipc_timer_p timers, u16 num, u16 f_idx)
{
    u16 timer_idx = 0;
    s32 cl_idx    = 0; /* Left child (default choice) using s32 to prevent overflow */
    s32 cr_idx    = 0; /* Right child */

    while (f_idx < num) { /* Node within range */
        cr_idx = (f_idx + 1) << 1;
        cl_idx = cr_idx - 1;

        if (cl_idx >= num)
            break; /* Both children do not exist, this node is a leaf node */

        if (cr_idx < num) { /* Right child exists, choose the smallest */
            cl_idx = timers[heap[cl_idx]].target_tms < timers[heap[cr_idx]].target_tms ? cl_idx : cr_idx;
        }
        if (timers[heap[cl_idx]].target_tms > timers[heap[f_idx]].target_tms)
            break; /* Smaller than the smallest child, determined position */

        timer_idx                    = heap[cl_idx];
        heap[cl_idx]                 = heap[f_idx];
        heap[f_idx]                  = timer_idx;
        timers[heap[f_idx]].heap_idx = f_idx; // Update heap_idx in the corresponding timer

        f_idx = cl_idx;
    }

    timers[heap[f_idx]].heap_idx = f_idx; // Update heap_idx in the corresponding timer
    return f_idx;
}

static vptr _pth_timer(vptr arg)
{
    ipc_timer_bus_p h_bus = arg;
    pu16 heap            = (pu16)&h_bus->timers[h_bus->timer_max];
    vptr tmp_mems        = (pu8)heap + ALIGN(h_bus->timer_max * sizeof(u16));

    h_bus->alive = 1;

    ipc_lock(h_bus->mutex);

    while (h_bus->gorun) {

        u64 target_tms = h_bus->timer_num ? h_bus->timers[heap[0]].target_tms : 0; // Get the latest head node
        s32 ret        = ipc_cond_wait(h_bus->cond, h_bus->mutex, target_tms);
        if (ret != IPC_TIMEOUT || !h_bus->timer_num || h_bus->timers[heap[0]].target_tms != target_tms)
            continue; // Task not due or task due but already deleted

        u16 timer_idx      = heap[0];
        ipc_timer_p h_timer = &h_bus->timers[timer_idx];
        vptr tmp_mem       = h_bus->tmp_mem_size ? tmp_mems + timer_idx * h_bus->tmp_mem_size : NULL;

        h_timer->task_run = 1;   // Mark the task as running, meaning ipc_timer_stop cannot interrupt this task
        ipc_unlock(h_bus->mutex); // The entire interval except for task execution needs to be outside of the critical
                                 // section, because tasks can internally call ipc_timer_start or ipc_timer_stop,
                                 // preventing repeated locking
        s32 after_tms = h_timer->f_task(h_timer->usr_arg, tmp_mem, h_bus->tmp_mem_size);
        ipc_lock(h_bus->mutex);
        h_timer->task_run = 0;

        u16 heap_idx = h_timer->heap_idx; // Although this must be 0, consider that tasks can use ipc_timer_start and
                                          // ipc_timer_stop internally, so prevent the possibility of the heap_idx being
                                          // changed under certain clock mismatches
        if (after_tms < 0) {              // Disable the timer task
            h_timer->task_seq++;
            heap[heap_idx]         = heap[--h_bus->timer_num]; /* Swap with the removed node */
            heap[h_bus->timer_num] = timer_idx;
        } else {
            h_timer->target_tms = ipc_mono_tms() + after_tms;
        }
        if (_float_up(heap, h_bus->timers, heap_idx) == heap_idx) { /* If float-up fails, try sinking down */
            _sink_down(heap, h_bus->timers, h_bus->timer_num, heap_idx);
        }
    }

    ipc_unlock(h_bus->mutex);

    h_bus->alive = 0;

    return NULL;
}

vptr ipc_timer_init(u16 timer_max, s32 tmp_mem_size)
{
    if (!timer_max || tmp_mem_size < 0)
        return NULL;
    tmp_mem_size = ALIGN(tmp_mem_size); // Ensure each memory block is aligned

    s32 timers_size      = ALIGN(timer_max * sizeof(ipc_timer_t));
    s32 heaps_size       = ALIGN(timer_max * sizeof(u16));
    s32 tmp_mems_size    = ALIGN(timer_max * tmp_mem_size);
    s32 all_size         = sizeof(ipc_timer_bus_t) + timers_size + heaps_size + tmp_mems_size;
    ipc_timer_bus_p h_bus = ipc_malloc(all_size, sizeof(ipc_timer_bus_t));
    if (!h_bus)
        return NULL;

    pu16 heap = (pu16)&h_bus->timers[timer_max];
    for (s32 idx = 0; idx < timer_max;
         idx++) { // Initialize the min-heap indices pointing to the corresponding timer nodes (one-to-one)
        heap[idx] = idx;
    }

    h_bus->timer_num    = 0;
    h_bus->timer_max    = timer_max;
    h_bus->tmp_mem_size = tmp_mem_size;

    ipc_lock_init(h_bus->mutex, IPC_THREAD_MUTEX);
    ipc_cond_init(h_bus->cond, IPC_THREAD_COND);
    h_bus->gorun = 1;

    s32 ret = ipc_create_thread("ipc_timer_bus", _pth_timer, h_bus, 16 * 1024, 0);
    if (ret < 0) {
        ipc_lock_uninit(h_bus->mutex);
        ipc_cond_uninit(h_bus->cond);
        ipc_free(h_bus);
        return NULL;
    }

    return h_bus;
}

void ipc_timer_uninit(vptr handle, u8 is_wait)
{
    if (!handle)
        return;

    ipc_timer_bus_p h_bus = handle;

    h_bus->gorun = 0;
    ipc_lock(h_bus->mutex);
    ipc_cond_wakeup(h_bus->cond); // Prevent the thread from sleeping on the condition variable
    ipc_unlock(h_bus->mutex);

    if (!is_wait)
        return;

    while (h_bus->alive) {
        ipc_lock(h_bus->mutex);
        ipc_cond_wakeup(h_bus->cond);
        ipc_unlock(h_bus->mutex);
        ipc_msleep(100);
    }

    ipc_cond_uninit(h_bus->cond);
    ipc_lock_uninit(h_bus->mutex);
    ipc_free(h_bus);
}

s32 ipc_timer_start(vptr handle, s32 after_tms, ipc_timer_task_f f_task, vptr usr_arg)
{
    if (!handle || after_tms < 0 || !f_task)
        return IPC_INVALID_ARGS;

    ipc_timer_bus_p h_bus = handle;

    ipc_lock(h_bus->mutex);

    if (h_bus->timer_num >= h_bus->timer_max) {
        ipc_unlock(h_bus->mutex);
        return IPC_NOBUF;
    }

    pu16 heap = (pu16)&h_bus->timers[h_bus->timer_max];

    u16 heap_idx       = h_bus->timer_num++; // Find an idle heap index
    u16 timer_idx      = heap[heap_idx];     // Idle heap index points to the timer
    ipc_timer_p h_timer = &h_bus->timers[timer_idx];

    h_timer->task_run   = 0;
    h_timer->f_task     = f_task;
    h_timer->usr_arg    = usr_arg;
    h_timer->target_tms = ipc_mono_tms() + after_tms;

    if (_float_up(heap, h_bus->timers, heap_idx) == 0) { // Insert at the first position, immediately wake up the thread
        ipc_cond_wakeup(h_bus->cond);
    }

    if (h_bus->tmp_mem_size) {
        vptr tmp_mems = (pu8)heap + ALIGN(h_bus->timer_max * sizeof(u16)); // Skip over the heap
        memset(tmp_mems + timer_idx * h_bus->tmp_mem_size, 0,
               h_bus->tmp_mem_size); // Clear the temporary memory associated with the timer
    }

    s32 timer_id = h_timer->task_seq << 16 | timer_idx;

    ipc_unlock(h_bus->mutex);

    return timer_id;
}

s32 ipc_timer_stop(vptr handle, s32 timer_id)
{
    if (!handle || timer_id < 0)
        return IPC_INVALID_ARGS;

    ipc_timer_bus_p h_bus = handle;
    pu16 heap            = (pu16)&h_bus->timers[h_bus->timer_max];

    u16 timer_idx = timer_id & 0xFFFF;
    if (timer_idx >= h_bus->timer_max) {
        return IPC_OUT_OF_RANGE;
    }

    ipc_lock(h_bus->mutex);

    ipc_timer_p h_timer = &h_bus->timers[timer_idx];

    u16 task_seq = timer_id >> 16;
    if (task_seq != h_timer->task_seq) { // Sequence does not match, the timer has been destroyed
        ipc_unlock(h_bus->mutex);
        return IPC_NOT_FOUND;
    }

    if (h_timer->task_run) {
        ipc_unlock(h_bus->mutex);
        return IPC_ACTION_BUSY;
    }

    h_timer->task_seq++; // Mark for destruction

    u64 now_tms  = ipc_mono_tms();
    s32 left_tms = h_timer->target_tms > now_tms ? h_timer->target_tms - now_tms : 0;

    u16 heap_idx           = h_timer->heap_idx;
    heap[heap_idx]         = heap[--h_bus->timer_num]; /* Backtrack the last node and swap with the removed node */
    heap[h_bus->timer_num] = timer_idx;

    if (_float_up(heap, h_bus->timers, heap_idx) == heap_idx) { /* If float-up fails, try sinking down */
        _sink_down(heap, h_bus->timers, h_bus->timer_num, heap_idx);
    }

    ipc_unlock(h_bus->mutex);

    return left_tms;
}