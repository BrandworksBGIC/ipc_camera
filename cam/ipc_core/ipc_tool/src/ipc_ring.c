#include "ipc_ring.h"
#include "ipc_memory.h"
#include "ipc_misc.h"
#include "ipc_thread.h"

typedef struct {
    /* Initial setting*/
    ipc_lock_t mutex; ///< Lock of data structure
    v8 mark;         ///< Shared memory mark
    s32 size;        ///< total length of Ring data
    s32 exsize;      ///< Extensible data length
    s32 new;         ///< New write address (pointing to [len][..data..][len][flag])
    s32 used;        ///< Lenght have been used
    /* Runtime data */
    s64 nseq;   ///< New serial number (next)
    s64 fseq;   ///< Serial number of the ring header
    s64 rseq;   ///< Serial number of the ring tail
    s32 front;  ///< Address of the ring header (pointing to [flag][len][..data..][len])
    s32 rear;   ///< Ring tail address (pointing to [flag][len][..data..][len])
    v8 data[0]; ///< expanded data + ring data
} ring_t, *ring_p;

typedef struct {
    ring_p h_ring; /* parent handle */
    s32 addr;      /* data address */
    s32 len;       /* data length */
    s32 offset;    /* offset of read data */
    s64 seq;       /* data sequence number */
} index_t, *index_p;

/** @brief Calculate the length of the entire data node: [len][...data...][len][flag] */
#define LEN(data_len) (sizeof(u32) + data_len + sizeof(u32) + 1)

/** @brief Address mod correction */
#define ADDR(VAL, MOD) (((VAL) + MOD) % MOD)

/** @brief Calculate the starting address of the ring data */
#define START(h_ring) (h_ring->data + h_ring->exsize)

/** @brief Empty skip */
#define _jump_ring(h_ring, off, len) (*(off) = ADDR(*(off) + (len), h_ring->size))

static void _read_ring(ring_p h_ring, ps32 offset, s32 len, vptr out)
{
    if (*offset + len < h_ring->size) {
        memcpy(out, START(h_ring) + *offset, len);
    } else {
        s32 first = h_ring->size - *offset;
        memcpy(out, START(h_ring) + *offset, first);
        memcpy(out + first, START(h_ring), len - first);
    }
    _jump_ring(h_ring, offset, len);
}

static void _write_ring(ring_p h_ring, ps32 offset, s32 len, vptr in)
{
    if (*offset + len < h_ring->size) {
        memcpy(START(h_ring) + *offset, in, len);
    } else {
        s32 first = h_ring->size - *offset;
        memcpy(START(h_ring) + *offset, in, first);
        memcpy(START(h_ring), in + first, len - first);
    }
    _jump_ring(h_ring, offset, len);
}

/** @brief Returns the length of the current front node data. If it is 0, there is no data */
static s32 _updata_front(ring_p h_ring)
{
    s32 front = h_ring->front;
    s32 len   = 0;
    u8 nflag  = 0;

    _read_ring(h_ring, &front, sizeof(nflag), &nflag);
    while (nflag) {
        h_ring->front = ADDR(front - sizeof(nflag), h_ring->size);
        _read_ring(h_ring, &front, sizeof(len), &len);
        _jump_ring(h_ring, &front, len + sizeof(len));
        _read_ring(h_ring, &front, sizeof(nflag), &nflag);
        if (nflag)
            h_ring->fseq++;
    }

    return len;
}

void ipc_ring_clear(vptr h_this)
{
    if (!h_this)
        return;
    ring_p h_ring = ((ring_p)h_this);
    s32 front_len = 0;
    s64 diff_seq  = 0;

    ipc_lock(h_ring->mutex);
    front_len = _updata_front(h_ring);
    if (front_len) {
        diff_seq     = h_ring->nseq - 1 - h_ring->fseq;
        h_ring->rseq = h_ring->fseq = h_ring->nseq;
        h_ring->nseq += diff_seq;
        h_ring->front = ADDR(h_ring->front + LEN(front_len), h_ring->size);
        h_ring->used -= ADDR(h_ring->front - h_ring->rear, h_ring->size);
        h_ring->rear = h_ring->front;
    }
    ipc_unlock(h_ring->mutex);
}

static s32 _get_new_addr(ring_p h_ring, s32 len)
{
    s32 node_len = LEN(len);
    s32 tmp_rear = h_ring->rear;
    s32 tmp_used = h_ring->used + node_len;
    s64 tmp_seq  = h_ring->rseq;
    s32 tmp_len  = 0;

    _updata_front(h_ring);

    while (tmp_used > h_ring->size) {
        if (tmp_rear == h_ring->front)
            return IPC_NOBUF;

        _jump_ring(h_ring, &tmp_rear, 1);
        _read_ring(h_ring, &tmp_rear, sizeof(tmp_len), &tmp_len);
        _jump_ring(h_ring, &tmp_rear, tmp_len + sizeof(tmp_len));
        tmp_used -= LEN(tmp_len);
        tmp_seq++;
    }

    h_ring->used = tmp_used;
    h_ring->rear = tmp_rear;
    h_ring->rseq = tmp_seq;
    h_ring->nseq++;

    s32 addr = h_ring->new;
    u8 flag  = 0;

    _jump_ring(h_ring, &h_ring->new, node_len - 1);
    _write_ring(h_ring, &h_ring->new, sizeof(flag), &flag);

    return addr;
}

s32 ipc_cal_block_total(ipc_ring_block_p p_block, s32 node_num)
{
    s32 ret = 0, len = 0, idx = 0;
    for (idx = 0; idx < node_num; idx++) {
        if (!p_block[idx].data)
            return IPC_INVALID_ARGS;
        if (p_block[idx].len >= 0) {
            len += p_block[idx].len;
        } else {
            ret = ipc_cal_block_total(p_block[idx].data, -p_block[idx].len);
            if (ret < 0)
                return ret;
            len += ret;
        }
    }
    return len;
}

static s32 _write_total_data(ring_p h_ring, ps32 p_addr, ipc_ring_block_p p_block, s32 node_num)
{
    s32 len = 0, idx = 0;
    for (idx = 0; idx < node_num; idx++) {
        if (p_block[idx].len >= 0) {
            _write_ring(h_ring, p_addr, p_block[idx].len, p_block[idx].data);
        } else {
            _write_total_data(h_ring, p_addr, p_block[idx].data, -p_block[idx].len);
        }
    }
    return len;
}

s32 ipc_ring_push(vptr h_this, ipc_ring_block_p p_block, s32 block_num)
{
    if (!h_this || !p_block || block_num <= 0)
        return IPC_INVALID_ARGS;

    ring_p h_ring = ((ring_p)h_this);
    s32 addr      = 0;
    s32 len       = ipc_cal_block_total(p_block, block_num);
    if (len <= 0)
        return IPC_INVALID_ARGS;

    ipc_lock(h_ring->mutex);
    addr = _get_new_addr(h_ring, len);
    ipc_unlock(h_ring->mutex);
    if (addr < 0)
        return addr;

    s32 flag_addr = ADDR(addr - 1, h_ring->size);
    u8 flag       = 1;

    _write_ring(h_ring, &addr, sizeof(len), &len);
    _write_total_data(h_ring, &addr, p_block, block_num);
    _write_ring(h_ring, &addr, sizeof(len), &len);
    _write_ring(h_ring, &flag_addr, sizeof(flag), &flag);

    return len;
}

static s32 _get_data_idx(ring_p h_ring, ipc_ring_cmd_e cmd, index_p p_idx)
{
    s64 seq  = 0;
    s32 len  = 0;
    s32 addr = 0;
    u8 nflag = 0;

    switch (cmd) {
        case IPC_RING_TAIL:
        case IPC_RING_HEAD:
            if (cmd == IPC_RING_HEAD) {
                _updata_front(h_ring);
                addr = h_ring->front;
                seq  = h_ring->fseq;
            } else {
                addr = h_ring->rear;
                seq  = h_ring->rseq;
            }
            _read_ring(h_ring, &addr, sizeof(nflag), &nflag);
            if (!nflag)
                return IPC_NOT_READY;
            _read_ring(h_ring, &addr, sizeof(len), &len);
            p_idx->h_ring = h_ring;
            break;
        case IPC_RING_LAST:
        case IPC_RING_NEXT:
            if (p_idx->h_ring != h_ring)
                return IPC_INVALID_ARGS;
            addr = p_idx->addr;
            seq  = p_idx->seq;
            len  = p_idx->len;
            if (cmd == IPC_RING_LAST)
                seq--;
            if (seq < h_ring->rseq)
                return IPC_NOT_FOUND;
            if (seq >= h_ring->nseq)
                return IPC_NOT_READY;
            if (cmd == IPC_RING_LAST) {
                _jump_ring(h_ring, &addr, -(sizeof(len) * 2 + 1));
                _read_ring(h_ring, &addr, sizeof(len), &len);
                _jump_ring(h_ring, &addr, -(sizeof(len) + len));
                break;
            }
            if (cmd == IPC_RING_NEXT) {
                _jump_ring(h_ring, &addr, len + sizeof(len));
                _read_ring(h_ring, &addr, sizeof(nflag), &nflag);
                if (!nflag)
                    return IPC_NOT_READY;
                _read_ring(h_ring, &addr, sizeof(len), &len);
                seq++;
                break;
            }
        default:
            return IPC_INVALID_ARGS;
    }

    p_idx->addr   = addr;
    p_idx->len    = len;
    p_idx->seq    = seq;
    p_idx->offset = 0;

    return len;
}

s32 ipc_ring_iter_ctrl(vptr h_this, ipc_ring_cmd_e cmd, ipc_ring_iter_p p_iter)
{
    if (!h_this || !p_iter)
        return IPC_INVALID_ARGS;

    ring_p h_ring = ((ring_p)h_this);

    ipc_lock(h_ring->mutex);
    s32 len = _get_data_idx(h_ring, cmd, (index_p)p_iter);
    ipc_unlock(h_ring->mutex);

    return len;
}

s32 ipc_ring_get_len(ipc_ring_iter_p p_iter)
{
    if (!p_iter)
        return IPC_INVALID_ARGS;
    index_p p_idx = (index_p)p_iter;
    return p_idx->len;
}

s32 ipc_ring_iter_pop(ipc_ring_iter_p p_iter, vptr data, s32 max, s32 offset)
{
    if (!p_iter || !data || max <= 0)
        return IPC_INVALID_ARGS;

    index_p p_idx = (index_p)p_iter;
    ring_p h_ring = p_idx->h_ring;
    s32 addr      = p_idx->addr;
    s32 len       = p_idx->len;

    if (h_ring == NULL)
        return IPC_NOT_INIT;
    if (offset < 0)
        offset = p_idx->offset;
    if (offset >= len)
        return 0;

    len -= offset;
    len = MIN(max, len);
    _jump_ring(h_ring, &addr, offset);
    _read_ring(h_ring, &addr, len, data);

    if (p_idx->seq < h_ring->rseq) {
        return IPC_NOT_FOUND;
    }

    p_idx->offset = offset + len;

    return len;
}

vptr ipc_ring_init(v8 mark, s32 expand_size, s32 queue_size)
{
    if (expand_size < 0 || queue_size <= LEN(1))
        return NULL;

    u32 all_size = sizeof(ring_t) + expand_size + queue_size;

    u8 first_create     = 0;
    ring_p h_ring       = NULL;
    ipc_lock_e lock_type = 0;

    if (mark) {
        lock_type = IPC_PROCESS_MUTEX;
        h_ring    = ipc_shmalloc(mark, all_size, &first_create);
    } else {
        lock_type    = IPC_THREAD_MUTEX;
        first_create = 1;
        h_ring       = ipc_malloc(all_size, 0);
    }

    if (h_ring == NULL)
        return NULL;
    if (first_create) {
        memset(h_ring, 0, sizeof(ring_t) + expand_size + 1);
        ipc_lock_init(h_ring->mutex, lock_type);
        h_ring->size   = queue_size;
        h_ring->exsize = expand_size;
        h_ring->mark   = mark;
        h_ring->new    = 1;
        h_ring->used   = 1;
    }

    return (vptr)h_ring;
}

void ipc_ring_uninit(vptr h_this)
{
    if (!h_this)
        return;

    ring_p h_ring = ((ring_p)h_this);
    ipc_lock_uninit(h_ring->mutex);
    ipc_shmfree(h_ring->mark, h_ring);
}

vptr ipc_ring_ref(v8 mark)
{
    if (!mark)
        return NULL;

    ring_p h_ring = ipc_shmref(mark);
    if (h_ring == NULL)
        return NULL;

    return h_ring;
}

void ipc_ring_unref(vptr h_this)
{
    if (!h_this)
        return;
    ring_p h_queue = ((ring_p)h_this);
    ipc_shmunref(h_queue);
}
