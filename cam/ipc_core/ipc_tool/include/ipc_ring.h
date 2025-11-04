#ifndef __IPC_RING_H__
#define __IPC_RING_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>

typedef struct {
    vptr data;
    s32 len;
} ipc_ring_block_t, *ipc_ring_block_p;

typedef struct {
    v8 fill[64];
} ipc_ring_iter_t[1], *ipc_ring_iter_p;

typedef enum {
    IPC_RING_TAIL,
    IPC_RING_HEAD,
    IPC_RING_LAST,
    IPC_RING_NEXT,
} ipc_ring_cmd_e;

/**
 * @brief Initialization of the ring buffer
 *
 * @param mark 0: Use ordinary memory >0: Use shared memory, and mark as the handle of this shared memory
 * @param expand_size Expanded data that can be used by the caller to store their own management structure
 * @param queue_size The length of the initialized ring queue
 * @return A pointer to the expanded data, which is also the handle of all the following APIs
 */
EXAPI vptr ipc_ring_init(v8 mark, s32 expand_size, s32 queue_size);

/**
 * @brief Destruction of the ring buffer
 *
 * @param h_this Handle obtained by ipc_ring_init
 * @note Make sure that all ipc_ring_refers have called ipc_ring_unref before calling this API, otherwise the consequences
 * are unknown
 */
EXAPI void ipc_ring_uninit(vptr h_this);

/**
 * @brief Reference of the ring buffer (only when mark is non-zero in ipc_ring_init -> shared memory takes effect)
 *
 * @param mark Mark at the time of ipc_ring_init
 * @return Consistent with ipc_ring_init
 * @note Note that when the ring of the corresponding mark is not initialized, NULL is returned
 */
EXAPI vptr ipc_ring_ref(v8 mark);

/**
 * @brief De-reference of the ring buffer
 *
 * @param h_this Pointer returned by ipc_ring_ref
 * @note Do not put it in a non-shared memory, that is, do not put it in a handle initialized when ipc_ring_init mark is
 * 0
 */
EXAPI void ipc_ring_unref(vptr h_this);

/**
 * @brief Clear the ring buffer
 *
 * @param h_this Handle obtained by ipc_ring_init or ipc_ring_ref
 * @note The implementation is very lightweight and can be called frequently
 */
EXAPI void ipc_ring_clear(vptr h_this);

/**
 * @brief Push data to the ring buffer
 *
 * @param h_this Handle obtained by ipc_ring_init or ipc_ring_ref
 * @param p_block An array of fragmented data
 * @param block_num The number of fragmented data
 * @return <0: Standard return value of ipc_std.h >=0: The total size of the written data
 * @note When p_block->len < 0, it means that p_block->data no longer points to fragmented data,
 *       but points to the next level of nested ipc_ring_block_t[], and abs(p_block->len) is
 *       the number of fragments of this ipc_ring_block_t[], and it can be nested indefinitely under ideal resource
 * conditions
 */
EXAPI s32 ipc_ring_push(vptr h_this, ipc_ring_block_p p_block, s32 block_num);

/**
 * @brief Iterative pop data control of the ring buffer
 *
 * @param h_this Handle obtained by ipc_ring_init or ipc_ring_ref
 * @param cmd Locate data blocks, such as locating head, tail, next, and last data
 * @param p_iter The iterator handle that needs to be initialized
 * @return <0: Standard return value of ipc_std.h >=0: The size of the entire data block
 * @note Key return values:
 *       IPC_NOT_READY: No new data, please wait
 *       IPC_NOT_FOUND: The data to be taken has been overwritten
 */
EXAPI s32 ipc_ring_iter_ctrl(vptr h_this, ipc_ring_cmd_e cmd, ipc_ring_iter_p p_iter);

/**
 * @brief Iterate over fragments to extract data
 *
 * @param p_iter The iterator handle of the data block located by ipc_ring_iter_ctrl
 * @param data External data buffer
 * @param max External data size
 * @param offset The offset of the data to be taken in the data block. If <0, the offset is internally iteratively
 * calculated
 * @return >=0: The length of the popped data <0: The standard return value of ipc_std.h
 * @note Key return values:
 *       IPC_NOT_FOUND: The data to be taken has been overwritten
 */
EXAPI s32 ipc_ring_iter_pop(ipc_ring_iter_p p_iter, vptr data, s32 max, s32 offset);

/**
 * @brief Get the total size of the data block
 *
 * @param p_iter The iterator handle of the data block located by ipc_ring_iter_ctrl
 * @return <0: Standard return value of ipc_std.h >=0: The size of the entire data block
 */
EXAPI s32 ipc_ring_get_len(ipc_ring_iter_p p_iter);

/**
 * @brief Calculate the total data length at the ipc_ring_block_t fragmentation rule level
 *
 * @param p_block An array of fragmented data
 * @param node_num The number of fragmented data
 * @return <0: Standard return value of ipc_std.h >=0: The total size of the entire data block
 */
s32 ipc_cal_block_total(ipc_ring_block_p p_block, s32 node_num);

#ifdef __cplusplus
}
#endif

#endif //__IPC_RING_H__
