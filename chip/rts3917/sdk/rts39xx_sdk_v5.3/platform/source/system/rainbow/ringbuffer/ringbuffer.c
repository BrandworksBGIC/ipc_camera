/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>

#include "ringbuffer.h"

#define MAKETAG(a, b, c, d) (((uint32_t)d << 24) | ((uint32_t)c << 16) \
| ((uint32_t)b << 8) | a)
#define RING_BEGIN_OFFSET 1024
#define RF_PKT_FLAG_KEY (1 << 0)

struct ringbuffer_context {
	void *shm_buf;

	int direction;
	uint32_t shm_size;
	uint32_t ringbuffer_size;

	char *ring_begin;
	char *ring_pos;
	char *ring_end;
	uint32_t prev_index;
};

struct ringbuffer_header {
	uint32_t tag;
	uint32_t length;
	uint32_t ring_begin_offset;
	uint32_t write_index;
	uint32_t done;
	uint32_t reserved[2];
	char stream_format[0];
};

#define MAGIC 0xffffffff
struct packet_header {
	uint32_t magic;
	uint32_t length;
	uint64_t timestamp;
	uint32_t flags;
	uint32_t type;
	uint32_t index;
	uint32_t checksum;
	uint32_t next_index;
	uint32_t prev_index;
};

enum {
	PKT_FL_ALLOCED = 1,
	PKT_FL_CACHE_INUSE = 1 << 1,
	PKT_FL_DATA_ALLOCED = 1 << 2,
};

struct inner_packet {
	packet_t p;
	uint32_t flags;
};

static int open_shm(char *filename, uint32_t size, void **shm)
{
	key_t key = ftok(filename, 0);
	int shm_id;
	void *shm_buf;

	if (key == -1) {
		fopen(filename, "w+");
		key = ftok(filename, 0);
	}

	if (key == -1)
		return -errno;

	/*if size is 0 open shm, otherwise create*/
	shm_id = shmget(key, size, IPC_CREAT | 0666);
	if (shm_id < 0)
		return -errno;

	shm_buf = shmat(shm_id, NULL, 0);
	if (((void *) -1) == shm_buf) {
		shm_buf = NULL;
		return -errno;
	}

	*shm = shm_buf;
	return 0;
}


static void release_shm(void *shm)
{
	if (shm)
		shmdt(shm);
}


static int ringbuffer_create(struct ringbuffer_context *rb,
			char *filename, uint32_t size)
{
	uint32_t div, mod;
	int ret = 0;
	uint32_t tag = MAKETAG('R', 'I', 'N', 'G');
	struct ringbuffer_header *hdr = NULL;

#define BLOCK 4096
	div = size / BLOCK;
	mod = size - (div * BLOCK);
	if (mod)
		div++;
	rb->shm_size = div * BLOCK;

	ret = open_shm(filename, rb->shm_size, &rb->shm_buf);
	if (ret < 0)
		goto failed;

	rb->direction = 1;
	rb->ring_begin = rb->shm_buf + RING_BEGIN_OFFSET;
	rb->ring_end = rb->shm_buf + rb->shm_size;
	rb->ring_pos = rb->ring_begin;
	rb->ringbuffer_size = rb->ring_end - rb->ring_begin;
	rb->prev_index = -1;

	hdr = (struct ringbuffer_header *)rb->shm_buf;
	hdr->tag = tag;
	hdr->length = rb->shm_size;
	hdr->ring_begin_offset = rb->ring_begin - (char *)rb->shm_buf;
	hdr->write_index = 0;
	hdr->done = 0;
	hdr->reserved[0] = 0;
	hdr->reserved[1] = 0;

	return 0;

failed:
	if (rb && rb->shm_buf) {
		release_shm(rb->shm_buf);
		rb->shm_buf = NULL;
	}

	return ret;
}


static int ringbuffer_open(struct ringbuffer_context *rb, char *filename)
{
	struct ringbuffer_header *hdr = NULL;
	uint32_t tag = MAKETAG('R', 'I', 'N', 'G');
	uint32_t tag_read;
	int ret = 0;

	ret = open_shm(filename, 0, &rb->shm_buf);
	if (ret < 0)
		goto failed;

	hdr = (struct ringbuffer_header *)rb->shm_buf;
	tag_read = hdr->tag;

	if (tag_read != tag) {
		ret = -EINVAL;
		goto failed;
	}

	rb->direction = 0;
	rb->shm_size = hdr->length;
	rb->ring_end = rb->shm_buf + rb->shm_size;
	rb->ring_begin = rb->shm_buf + hdr->ring_begin_offset;
	rb->ring_pos = rb->ring_begin + hdr->write_index;

	rb->ringbuffer_size = rb->ring_end - rb->ring_begin;
	rb->prev_index = -1;

	return 0;

failed:
	if (rb && rb->shm_buf) {
		release_shm(rb->shm_buf);
		rb->shm_buf = NULL;
	}

	return ret;
}


void *rts_ringbuffer_init(char *filename, uint32_t size)
{
	struct ringbuffer_context *rb = calloc(sizeof(*rb), 1);
	int ret = 0;

	if (!rb)
		return NULL;

	if (size)
		ret = ringbuffer_create(rb, filename, size);
	else
		ret = ringbuffer_open(rb, filename);

	if (ret)
		goto failed;
	return rb;

failed:
	if (rb) {
		free(rb);
		rb = NULL;
	}

	errno = -ret;
	return NULL;
}


int rts_ringbuffer_set_stream_format(void *handle, stream_format *fmt)
{
	struct ringbuffer_context *rb = handle;
	struct ringbuffer_header *phdr;

	if (!rb || !rb->shm_buf)
		return -1;

	phdr = (struct ringbuffer_header *)rb->shm_buf;
	memcpy(phdr->stream_format, (void *)fmt, sizeof(stream_format));

	return 0;
}


int rts_ringbuffer_get_stream_format(void *handle, stream_format *fmt)
{
	struct ringbuffer_context *rb = handle;
	struct ringbuffer_header *phdr;

	if (!rb || !rb->shm_buf)
		return -1;
	phdr = (struct ringbuffer_header *)rb->shm_buf;
	memcpy((void *)fmt, phdr->stream_format, sizeof(stream_format));
	return 0;
}


static inline uint32_t __checksum(struct packet_header *ph)
{
	return (ph->magic
		^ ph->length
		^ (ph->timestamp & 0xffffffff)
		^ ((ph->timestamp >> 32) & 0xffffffff)
		^ ph->flags
		^ ph->type
		^ ph->index);
}


static inline void update_write_index(struct ringbuffer_context *rb,
					uint32_t write_index)
{
	struct ringbuffer_header *hdr = (struct ringbuffer_header *)rb->shm_buf;

	hdr->write_index = write_index;
}


static inline uint32_t get_write_index(struct ringbuffer_context *rb)
{
	struct ringbuffer_header *hdr = (struct ringbuffer_header *)rb->shm_buf;

	return hdr->write_index;
}


int rts_ringbuffer_write_packet(void *handle, packet_t *b)
{
	struct ringbuffer_context *rb = (struct ringbuffer_context *)handle;
	struct packet_header *hdr, *hdr_end;
	uint32_t size_left;
	uint32_t written, to_write;
	uint32_t write_index;

	/*pkt too large: we need a pkt with hdr & an end pkt hdr*/
	if (b->length > (rb->ringbuffer_size - 2 * sizeof(*hdr)))
		return -EINVAL;

	size_left = rb->ring_end - rb->ring_pos;
	if (size_left < sizeof(struct packet_header))
		rb->ring_pos = rb->ring_begin;

	hdr = (struct packet_header *)rb->ring_pos;

	/*tempply write a eof here, commit it when pkt is all written.*/
	hdr->magic = MAGIC;
	hdr->length = 0;
	hdr->checksum = __checksum(hdr);

	rb->ring_pos += sizeof(*hdr);

	to_write = b->length;
	size_left = rb->ring_end - rb->ring_pos;
	if (size_left < b->length) {
		memcpy(rb->ring_pos, b->vm_addr, size_left);
		rb->ring_pos = rb->ring_begin;
		to_write -= size_left;
		memcpy(rb->ring_pos, b->vm_addr + size_left, to_write);
		rb->ring_pos += to_write;
		to_write = 0;
	} else {
		memcpy(rb->ring_pos, b->vm_addr, to_write);
		rb->ring_pos += to_write;
		to_write = 0;
	}

	/*write a real tail, to indicate eof*/
	size_left = rb->ring_end - rb->ring_pos;
	if (size_left < sizeof(*hdr))
		rb->ring_pos = rb->ring_begin;

	hdr_end = (struct packet_header *)rb->ring_pos;
	hdr_end->magic = MAGIC;
	hdr_end->length = 0;
	hdr_end->checksum = __checksum(hdr_end);
	hdr_end->prev_index = ((char *)hdr - rb->ring_begin);
	hdr_end->next_index = -1;

	/*after eof is written, we commit the prev pkt*/
	hdr->next_index = ((char *)hdr_end - rb->ring_begin);
	hdr->prev_index = rb->prev_index;
	rb->prev_index = (char *)hdr - rb->ring_begin;

	hdr->length = b->length;
	hdr->timestamp = b->timestamp;
	hdr->flags = b->flags;
	hdr->type = b->type;
	hdr->index = b->index;
	hdr->checksum = __checksum(hdr);

	write_index = ((char *)hdr - rb->ring_begin);
	update_write_index(rb, write_index);

	rb->ring_pos = (char *)hdr_end;

	return 0;
}


static struct inner_packet *cache_alloc_packet(void)
{
#define CACHED_PACKETS_NUM 3
	static struct inner_packet pkts[CACHED_PACKETS_NUM] = {0};
	struct inner_packet *p = NULL;
	int i, got = 0;

	for (i = 0; i < CACHED_PACKETS_NUM; i++) {
		if (pkts[i].flags & PKT_FL_CACHE_INUSE)
			continue;
		got = 1;
		break;
	}

	if (got) {
		pkts[i].flags = PKT_FL_CACHE_INUSE;
		return &pkts[i];
	}

	p = calloc(sizeof(struct inner_packet), 1);
	if (!p)
		return NULL;

	p->flags = PKT_FL_ALLOCED;
	return p;
}


int rts_ringbuffer_read_packet(void *handle, packet_t **b)
{
	struct ringbuffer_context *rb = handle;
	char *pos = rb->ring_pos;
	struct packet_header *phdr = (struct packet_header *)pos;
	struct inner_packet *ib = NULL;
	packet_t *_b = NULL;
	uint32_t checksum;
	uint32_t size_left;
	uint32_t to_read;

	if (phdr->magic != MAGIC || phdr->checksum != __checksum(phdr)
			|| phdr->length > rb->ringbuffer_size) {
		uint32_t write_index;

		write_index = get_write_index(rb);
		rb->ring_pos = rb->ring_begin + write_index;
		return -EINVAL;
	}

	to_read = phdr->length;
	if (to_read == 0) {
		/*check if ringbuffer write is done*/
		struct ringbuffer_header *hdr = rb->shm_buf;
		if (hdr->done)
			return 1;
		return -EAGAIN;
	}

	ib = cache_alloc_packet();
	if (!ib)
		return -ENOMEM;

	_b = &ib->p;
	_b->length = phdr->length;
	_b->timestamp = phdr->timestamp;
	_b->flags = phdr->flags;
	_b->type = phdr->type;
	_b->index = phdr->index;

	pos += sizeof(*phdr);
	size_left = rb->ring_end - pos;
	if (to_read > size_left) {
		char *data = malloc(phdr->length);

		if (!data)
			return -ENOMEM;

		memcpy(data, pos, size_left);
		pos = rb->ring_begin;
		memcpy(data + size_left, pos, to_read - size_left);
		_b->vm_addr = data;
		ib->flags |= PKT_FL_DATA_ALLOCED;
		pos += (to_read - size_left);
	} else {
		_b->vm_addr = pos;
		pos += to_read;
	}

	*b = _b;

	rb->ring_pos = rb->ring_begin + phdr->next_index;
	return 0;
}


void rts_ringbuffer_free_packet(packet_t *b)
{
	if (b) {
		struct inner_packet *ib = (struct inner_packet *)b;

		if (b->vm_addr && (ib->flags & PKT_FL_DATA_ALLOCED))
			free(b->vm_addr);

		if (ib->flags & PKT_FL_ALLOCED)
			free(ib);
		else
			ib->flags = 0;
	}
}


void rts_ringbuffer_release(void *handle)
{
	struct ringbuffer_context *rb;

	if (handle) {
		rb = (struct ringbuffer_context *)handle;
		if (rb->shm_buf) {
			if (rb->direction) {
				/*mark written done*/
				struct ringbuffer_header *hdr = rb->shm_buf;

				hdr->done = 1;
			}
			release_shm(rb->shm_buf);
		}
		free(handle);
	}
}


int rts_ringbuffer_seek_timestamp(void *handle, uint64_t timestamp)
{
	struct ringbuffer_context *rb = (struct ringbuffer_context *)handle;
	struct packet_header *hdr;
	uint32_t write_index;
	stream_format fFmt;
	char *pos;
	int got = 0;

	write_index = get_write_index(rb);
	pos = rb->ring_begin + write_index;
	hdr = (struct packet_header *)pos;


	if (hdr->magic != MAGIC || __checksum(hdr) != hdr->checksum)
		return -EINVAL;

	rts_ringbuffer_get_stream_format(rb, &fFmt);

	while (1) {
		uint32_t prev_index;
		struct packet_header *hdr_prev;
		uint64_t ts = hdr->timestamp;

		if (fFmt.fmt < RB_A_FMT_AUDIO
			|| ts <= timestamp
			|| (hdr->flags & RF_PKT_FLAG_KEY)  == 1) {
			got = 1;
			break;
		}

		if (fFmt.fmt > RB_A_FMT_AUDIO
			|| ts <= timestamp) {
			got = 1;
			break;
		}

		prev_index = hdr->prev_index;
		if (prev_index < 0)
			break;

		pos = rb->ring_begin + prev_index;
		hdr_prev = (struct packet_header *)pos;

		if (hdr_prev->magic != MAGIC
			|| hdr_prev->checksum != __checksum(hdr_prev))
			break;
		hdr = hdr_prev;
	}

	if (got) {
		rb->ring_pos = (char *)hdr;
		return 0;
	}

	return -EINVAL;
}
