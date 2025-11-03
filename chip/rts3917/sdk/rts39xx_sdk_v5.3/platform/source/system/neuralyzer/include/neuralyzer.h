/*
 * neuralyzer.h
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

/**
 * neuralyzer is a linux image burning tool library,
 * which support burn by arbitrary segment split.
 * Two burn mode is supported:
 * 1. file mode is the easiest way to use neuralyzer.
 *    include 5 step:
 *    a. neu_data_alloc() - alloc data by provide a filename.
 *    b. neu_check() - check if the file is valid.
 *    c. neu_prepare() - prepare for burning.
 *    d. neu_burn() - burning linux image.
 *    e. neu_data_free() - free data.
 * 2. iter mode is much complicate, this is also the base of file mode.
 *    a. neu_data_alloc() - alloc data by provide a NULL pointer
 *    b. neu_check_iter() - check image,
 *       please refer neu_loop() for how to use iter function.
 *    c. neu_prepare() - preprare for buring.
 *    d. neu_burn_iter() - burning linux image, refer neu_loop()
 *    e. neu_data_free() - free data.
 */

#ifndef NEURALYZER_H
#define NEURALYZER_H /* NEURALYZER_H */

#include <sys/types.h>
#include <linux/types.h>
#include <inttypes.h>
#include <mtd/mtd-user.h>
#include <mtd_swab.h>
#include <libmtd.h>

#define NEU_HEADER_LEN				256
#define NEU_CHECKSUM_ALIGN			0x100000000
#define MTD_MAGIC_FEOF				0x46454f46
#define MAX_READ_UNIT_SIZE			0x20000

#define ROOTFS_MAGIC	0x726f6f74
#define USERDATA_MAGIC	0x6a667332

#define print_dbg(fmt, arg...)				\
	printf("%s: " fmt, __func__, ##arg)
#define print_err(fmt, arg...)				\
	printf("%s: ERROR: " fmt, __func__, ##arg)

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

struct  section_head {
	__u32 magic;
	__u64 reserved;
	__u32 partition_size;
	__u64 burnaddr;
	__u64 burnlen;
} __attribute__((__packed__));

enum neu_status {
	NEU_STAT_PREPARE,
	NEU_STAT_CHECKING,
	NEU_STAT_BURNING,
	NEU_STAT_SUCCESS,
	NEU_STAT_FAILED = -1,
};

struct section_tail {
	__u32 checksum;
} __attribute__((__packed__));

struct neu_data;
struct neu_cache {
	unsigned char data[MAX_READ_UNIT_SIZE];
	struct section_head head;
	char __head[sizeof(struct section_head)];
	struct section_tail tail;
	char __tail[sizeof(struct section_tail)];
	int (*flush)(struct neu_data *data, int fd);
};

struct neu_data {
	const char *filename;
	int burn_size;
	int burn_status;
	int checked_size;
	int burned_size;
	double burn_percentage;
	struct neu_cache *cache;
	libmtd_t mtd_desc;
	struct mtd_dev_info mtd;
};

int section_head_init(struct section_head *head, const void *data);
void section_head_dump(const struct section_head *head);
int section_head_valid(const struct neu_data *data);
int section_tail_init(struct section_tail *tail, const void *data);
void section_tail_dump(const struct section_tail *tail);
int global_check(libmtd_t desc);
int neu_burn_status_set(struct neu_data *data, int status);
struct neu_data *neu_data_alloc(const char *filename);
int neu_check(struct neu_data *data);
int neu_burn(struct neu_data *data);
int neu_burn_pass(const struct neu_data *data);
void neu_data_free(struct neu_data *data);
int read_pos_ll(const char *file, long long *value);
#endif /* NEURALYZER_H */
