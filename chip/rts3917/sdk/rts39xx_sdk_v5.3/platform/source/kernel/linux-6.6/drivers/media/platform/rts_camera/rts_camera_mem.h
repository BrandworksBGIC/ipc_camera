/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _RTS_CAMERA_MEM_H
#define _RTS_CAMERA_MEM_H

struct rtscam_mem_info {
	struct device *dev;
	void *virt_base;
	dma_addr_t device_base;
	phys_addr_t pfn_base;
	int size;
	int initialized;
	struct list_head memorys;
	struct list_head memorys2;
	struct list_head idles;
	int pre_setted;
	struct mutex lock;
	int flag;
};

enum {
	RTSMEM_TYPE_DMA,
	RTSMEM_TYPE_CMA,
};

enum {
	RTSMEM_PRE_ALLOCATED	= (1 << 0),
	RTSMEM_IN_USE		= (1 << 1),
	RTSMEM_PROBE_ALLOCATED	= (1 << 2),
};

enum {
	RTSMEM_ALLOC_DEFAULT = 0,
	RTSMEM_ALLOC_END,
	RTSMEM_ALLOC_BEGIN,
	RTSMEM_ALLOC_CMA = (0x1 << 8),
};

struct rtscam_mem_item {
	struct list_head list;
	unsigned long offset;
	unsigned long page_cnt;

	void *virt_addr;
	dma_addr_t phy_addr;
	size_t size;

	int buf_io;
	pid_t creator;
	pid_t owner;
	int buf_flag;
	int index;

	struct list_head ext_list;
	char info[32];
};

struct rtscam_sg_buf {
	struct rtscam_mem_info *rtsmem;
	struct device *dev;
	unsigned long vaddr;
	unsigned long size;
	enum dma_data_direction	dma_dir;
	struct sg_table *sgt;

	struct vm_area_struct		*vma;
};

int rtscam_mem_init(struct rtscam_mem_info *rtsmem, struct device *dev,
		    void *virt_addr, dma_addr_t device_addr, size_t size);
int rtscam_mem_release(struct rtscam_mem_info *rtsmem);
int rtscam_mem_pre_alloc(struct rtscam_mem_info *rtsmem,
		size_t size, const char *name, uint32_t io, int32_t idx);
int rtscam_mem_set_pre_alloc(struct rtscam_mem_info *rtsmem, int status);
int rtscam_mem_get_pre_alloc(struct rtscam_mem_info *rtsmem, int *status);

void *rtscam_mem_alloc(struct rtscam_mem_info *rtsmem,
			size_t size, dma_addr_t *phy_addr,
			uint32_t io, uint8_t dir, const char *name);
void *rtscam_mem_realloc(struct rtscam_mem_info *rtsmem,
		size_t size, dma_addr_t *phy_addr, uint32_t io, char *name);
void rtscam_mem_free(struct rtscam_mem_info *rtsmem, size_t size,
		     void *vaddr, dma_addr_t phy_addr);
void rtscam_mem_free_V2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr);

void *rtscam_mem_alloc2(struct rtscam_mem_info *rtsmem,
		       size_t size, dma_addr_t *phy_addr,
		       uint32_t io, char *name);
void rtscam_mem_free2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr);

long rtscam_mem_get_left_size(struct rtscam_mem_info *rtsmem);
long rtscam_mem_get_used_size(struct rtscam_mem_info *rtsmem);
long rtscam_mem_get_total_size(struct rtscam_mem_info *rtsmem);
dma_addr_t rtscam_mem_convert_v2d(struct rtscam_mem_info *rtsmem, void *vaddr);
void *rtscam_mem_convert_d2v(struct rtscam_mem_info *rtsmem,
			     dma_addr_t phy_addr);
int rtscam_mem_mmap(struct rtscam_mem_info *rtsmem,
		    struct vm_area_struct *vma, void *cpu_addr,
		    dma_addr_t dma_addr, size_t size);
int rtscam_mem_mmap2(struct rtscam_mem_info *rtsmem,
		    struct vm_area_struct *vma,
		    dma_addr_t dma_addr, size_t size);
struct rtscam_mem_item *rtscam_mem_enum(struct rtscam_mem_info *rtsmem,
					int index);
struct rtscam_mem_item *rtscam_mem_find(struct rtscam_mem_info *rtsmem,
					dma_addr_t phy_addr);
struct rtscam_mem_item *rtscam_mem_find2(struct rtscam_mem_info *rtsmem,
					dma_addr_t phy_addr);
int rtscam_mem_number(struct rtscam_mem_info *rtsmem, int flag);
int rtscam_mem_number2(struct rtscam_mem_info *rtsmem, int flag);

unsigned long *rtscam_mem_get_bitmap(struct rtscam_mem_info *rtsmem);
void rtscam_mem_put_bitmap(unsigned long *bitmap);

int rtscam_vma_is_io(struct vm_area_struct *vma);
int rtscam_vaddr_is_io(unsigned long vaddr);
int rtscam_get_user_pages(unsigned long start, struct page **pages,
			  int n_pages, struct vm_area_struct *vma, int write);
struct rtscam_sg_buf *rtscam_get_sg_buf(struct rtscam_mem_info *rtsmem,
					unsigned long vaddr,
					unsigned long size, int write);
void rtscam_put_sg_buf(struct rtscam_sg_buf *buf);
int rtscam_mem_set_info(struct rtscam_mem_info *rtsmem,
			dma_addr_t phy_addr, const char *info);
int rtscam_mem_get_info(struct rtscam_mem_info *rtsmem,
			struct rtscam_mem_item *mem);
int rtscam_mem_add_property(struct rtscam_mem_info *rtsmem,
			dma_addr_t phy_addr, uint32_t property);
int rtscam_mem_check(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr,
		size_t size, struct rtscam_mem_item **ppitem);
int rtscam_mem_check2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr,
		size_t size, struct rtscam_mem_item **ppitem);
#endif
