/*
 * neuralyzer.c
 *
 * Copyright(C) 2015 Micky Ching, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <inttypes.h>
#include <endian.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <neuralyzer.h>

int global_check(libmtd_t desc)
{
	const char *mtd0 = "global";
	struct mtd_dev_info mtd;

	if (mtd_get_dev_info(desc, "/dev/mtd0", &mtd) < 0) {
		print_err("mtd_get_dev_info failed\n");
		goto out_error;
	}

	if (strcmp(mtd0, mtd.name)) {
		print_err("mtd0 is not global\n");
		goto out_error;
	}

	return 0;

out_error:
	return -1;
}

int section_head_init(struct section_head *head, const void *data)
{
	const struct section_head *h = data;

	memset(head, 0, sizeof(*head));
	head->magic = be32_to_cpu(h->magic);
	if (head->magic == MTD_MAGIC_FEOF)
		return 0;
	head->reserved = be64_to_cpu(h->reserved);
	head->partition_size = be32_to_cpu(h->partition_size);
	head->burnaddr = be64_to_cpu(h->burnaddr);
	head->burnlen = be64_to_cpu(h->burnlen);
	section_head_dump(head);
	return 0;
}

void section_head_dump(const struct section_head *head)
{
	print_dbg("section: %08x %016llx\n",
		head->magic, head->reserved);
	print_dbg("\t  %016llx %08x %016llx\n", head->burnaddr,
		head->partition_size, head->burnlen);
}

int section_head_valid(const struct neu_data *data)
{
	struct section_head *head = &data->cache->head;

	if (head->burnaddr + head->burnlen > data->mtd.size) {
		print_err("burn length out of range\n");
		return -1;
	}

	return 0;
}

int section_tail_init(struct section_tail *tail, const void *data)
{
	const struct section_tail *t = data;

	tail->checksum = be32_to_cpu(t->checksum);
	section_tail_dump(tail);
	return 0;
}

void section_tail_dump(const struct section_tail *tail)
{
	print_dbg("checksum %08x\n", tail->checksum);
}

struct neu_data *neu_data_alloc(const char *filename)
{
	struct neu_data *data;
	struct stat st;

	data = calloc(1, sizeof(*data));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));

	data->filename = filename;
	data->burn_size = 1;
	if (data->filename) {
		if (stat(filename, &st) != 0) {
			print_err("%s not exist\n", data->filename);
			neu_data_free(data);
			return NULL;
		}
		data->burn_size += st.st_size;
	}

	/* Initialize libmtd */
	data->mtd_desc = libmtd_open();
	if (!data->mtd_desc) {
		print_err("can't initialize libmtd\n");
		neu_data_free(data);
		return NULL;
	}

	/* Fill in MTD device capability structure */
	if (mtd_get_dev_info1(data->mtd_desc, 0, &data->mtd) < 0) {
		print_err("mtd_get_dev_info failed\n");
		neu_data_free(data);
		return NULL;
	}
	return data;
}

void neu_data_free(struct neu_data *data)
{
	free(data->cache);
	data->cache = NULL;
	if (data->mtd_desc)
		libmtd_close(data->mtd_desc);
}

int neu_burn_finish(const struct neu_data *data)
{
	return data->burn_status == NEU_STAT_SUCCESS ||
		data->burn_status == NEU_STAT_FAILED;
}

int neu_burn_pass(const struct neu_data *data)
{
	return data->burn_status == NEU_STAT_SUCCESS;
}

static int cached_feof(struct neu_cache *cache)
{
	return cache->head.magic == MTD_MAGIC_FEOF;
}

static int align_ops_unit(struct neu_data *data, int len)
{
	if (len % data->mtd.eb_size)
		return (len + data->mtd.eb_size) & ~(data->mtd.eb_size - 1);
	return len;
}

static int neu_cache_do_flush(struct neu_data *data, int fd)
{
	struct neu_cache *cache = data->cache;
	int len, elen, ret, flag = 1;
	int mtd_fd;
	__u64 burnaddr_end;

	mtd_fd = open("/dev/mtd0", O_RDWR);
	if (mtd_fd  < 0) {
		print_err("open mtd0 fail\n");
		return -1;
	}

	burnaddr_end = cache->head.burnaddr + cache->head.partition_size;
	len = cache->head.burnlen > data->mtd.eb_size ?
		data->mtd.eb_size : cache->head.burnlen;

	while (1) {
		if (flag)
			elen = read(fd, cache->data, len);
		if (elen < 0)
			goto fail;
		else if (elen == 0)
			break;

		if (data->mtd.type == MTD_NANDFLASH) {
			ret = mtd_is_bad(&data->mtd, mtd_fd,
				cache->head.burnaddr / data->mtd.eb_size);
			if (ret < 0) {
				print_err("MTD get bad block failed at 0x%llx\n",
						cache->head.burnaddr);
				goto fail;
			} else if (ret == 1) {
				print_dbg("Skip bad block at %llx\n",
						cache->head.burnaddr);
				cache->head.burnaddr += data->mtd.eb_size;
				flag = 0;
				continue;
			}
		}

		flag = 1;
		if (mtd_erase(data->mtd_desc, &data->mtd, mtd_fd,
				cache->head.burnaddr / data->mtd.eb_size) < 0) {
			print_err("erase 0x%llx, 0x%x failed\n",
				cache->head.burnaddr, elen);
			goto fail;
		}

		if (mtd_write(data->mtd_desc, &data->mtd, mtd_fd,
				cache->head.burnaddr / data->mtd.eb_size,
				0, cache->data, align_ops_unit(data, elen),
				NULL, 0, 0) < 0) {
			print_err("write 0x%llx, 0x%x failed\n",
				cache->head.burnaddr, elen);
			goto fail;
		}

		cache->head.burnaddr += elen;
		cache->head.burnlen -= elen;
		data->burned_size += elen;

		len = cache->head.burnlen > data->mtd.eb_size ?
			data->mtd.eb_size : cache->head.burnlen;
	}

	if (data->mtd.type == MTD_NANDFLASH &&
		(cache->head.magic == ROOTFS_MAGIC
		|| cache->head.magic == USERDATA_MAGIC)) {
		if (cache->head.burnaddr % data->mtd.eb_size)
			cache->head.burnaddr += data->mtd.eb_size -
				cache->head.burnaddr % data->mtd.eb_size;
		while (cache->head.burnaddr < burnaddr_end) {
			ret = mtd_is_bad(&data->mtd, mtd_fd,
				cache->head.burnaddr / data->mtd.eb_size);
			if (ret < 0) {
				print_err("MTD get bad block failed at 0x%llx\n",
						cache->head.burnaddr);
				goto fail;
			} else if (ret == 1) {
				print_dbg("Skip bad block at %llx\n",
						cache->head.burnaddr);
				cache->head.burnaddr += data->mtd.eb_size;
				continue;
			}
			if (mtd_erase(data->mtd_desc, &data->mtd, mtd_fd,
				cache->head.burnaddr / data->mtd.eb_size) < 0) {
				print_err("erase 0x%llx, 0x%x failed\n",
					cache->head.burnaddr, elen);
				goto fail;
			}
			cache->head.burnaddr += data->mtd.eb_size;
		}
	}

	if (read(fd, cache->__tail, sizeof(struct section_tail)) < 0) {
		print_err("read checksum error!\n");
		goto fail;
	}
	close(mtd_fd);
	return 0;
fail:
	close(mtd_fd);
	return -1;
}

static __u64 sum_buffer(const unsigned char *buf, __u64 len)
{
	__u64 sum = 0;
	__u64 i = 0;

	while (i++ < len)
		sum += *buf++;

	return sum;
}

int cache_section_header(struct neu_data *data, int fd)
{
	int len;
	struct neu_cache *cache = data->cache;

	len = read(fd, cache->__head, sizeof(struct section_head));
	if (len < 0) {
		print_err("Read section head error!");
		return -1;
	}

	section_head_init(&cache->head, cache->__head);
	return len;
}

static int neu_cache_do_check(struct neu_data *data, int fd)
{
	struct neu_cache *cache = data->cache;
	int len, step;
	struct section_tail tail;

	len = cache->head.burnlen > data->mtd.eb_size ?
		data->mtd.eb_size : cache->head.burnlen;

	while (1) {
		step = read(fd, cache->data, len);
		if (step < 0)
			return -1;
		else if (step == 0)
			break;
		cache->tail.checksum += sum_buffer(cache->data, step);
		cache->head.burnaddr += step;
		cache->head.burnlen -= step;
		data->checked_size += step;
		len = cache->head.burnlen > data->mtd.eb_size ?
			data->mtd.eb_size : cache->head.burnlen;
	}

	cache->tail.checksum %= NEU_CHECKSUM_ALIGN;
	cache->tail.checksum = NEU_CHECKSUM_ALIGN -
		cache->tail.checksum;
	if (read(fd, cache->__tail, sizeof(struct section_tail)) < 0) {
		print_err("Read checksum error!\n");
		return -1;
	}

	section_tail_init(&tail, cache->__tail);

	if (cache->tail.checksum != tail.checksum) {
		print_err("checksum 0x%x != 0x%x\n",
			cache->tail.checksum, tail.checksum);
		return -1;
	}
	cache->tail.checksum = 0;
	return 0;
}

int neu_burn_status_set(struct neu_data *data, int status)
{
	if (data->burn_status != status)
		print_dbg("%d -> %d\n", data->burn_status, status);
	data->burn_status = status;
	return status;
}

static int neu_loop(struct neu_data *data,
		int (*iter)(struct neu_data *data, int file))
{
	int fd;
	int err = 0;

	if (!data->filename)
		return -1;

	fd = open(data->filename, O_RDONLY);
	if (fd < 0)
		return fd;

	if (!data->cache) {
		data->cache = calloc(1, sizeof(*data->cache));
		if (!data->cache) {
			close(fd);
			return -1;
		}
		data->cache->flush = iter;
	}

	if (iter == neu_cache_do_check)
		neu_burn_status_set(data, NEU_STAT_CHECKING);
	else if (iter == neu_cache_do_flush)
		neu_burn_status_set(data, NEU_STAT_BURNING);

	if (lseek(fd, NEU_HEADER_LEN, SEEK_SET) < 0) {
		print_err("lseek error\n");
		err = -1;
	}

	while (1) {
		if (cache_section_header(data, fd) < 0) {
			err = -1;
			break;
		}

		if (cached_feof(data->cache)) {
			if (iter == neu_cache_do_flush)
				neu_burn_status_set(data, NEU_STAT_SUCCESS);
			err = 0;
			break;
		}

		if (section_head_valid(data) < 0) {
			err = -1;
			break;
		}

		if (data->cache->flush(data, fd) < 0) {
			neu_burn_status_set(data, NEU_STAT_FAILED);
			err = -1;
			break;
		}
	}

	free(data->cache);
	data->cache = NULL;
	close(fd);
	return err;
}

int neu_check(struct neu_data *data)
{
	if (neu_loop(data, neu_cache_do_check) < 0)
		return -1;
	return 0;
}

int neu_burn(struct neu_data *data)
{
	if (neu_loop(data, neu_cache_do_check) < 0)
		return -1;

	if (neu_loop(data, neu_cache_do_flush) < 0)
		return -1;

	return 0;
}
