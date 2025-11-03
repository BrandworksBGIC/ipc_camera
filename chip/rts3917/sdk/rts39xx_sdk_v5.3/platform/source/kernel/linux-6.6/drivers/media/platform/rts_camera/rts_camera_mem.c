// SPDX-License-Identifier: GPL-2.0-only
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <asm/unaligned.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "rts_camera.h"
#include "rts_camera_mem.h"
#include "rts_isp_mem.h"

#define RTS_MEM_SIZE_SMALL		16
#define RTS_MEM_IDLE_COUNT		64

#define RTS_MEM_ALLOC_FROM_END_OFFSET   52

static struct rtscam_mem_item *__alloc_mem_item(void)
{
	struct rtscam_mem_item *item = NULL;

	item = vzalloc(sizeof(*item));
	if (!item)
		return NULL;

	INIT_LIST_HEAD(&item->list);
	INIT_LIST_HEAD(&item->ext_list);

	return item;
}

static int __init_idles(struct rtscam_mem_info *rtsmem)
{
	int i;

	for (i = 0; i < RTS_MEM_IDLE_COUNT; i++) {
		struct rtscam_mem_item *item = __alloc_mem_item();

		if (item)
			list_add_tail(&item->list, &rtsmem->idles);
	}

	return 0;
}

static void __release_idles(struct rtscam_mem_info *rtsmem)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *tmp;

	list_for_each_entry_safe(item, tmp, &rtsmem->idles, list) {
		list_del_init(&item->list);
		vfree(item);
	}
}

int rtscam_mem_init(struct rtscam_mem_info *rtsmem, struct device *dev,
		    void *virt_addr, dma_addr_t device_addr, size_t size)
{
	void __iomem *mem_base = NULL;
	int pages = size >> PAGE_SHIFT;

	if (!rtsmem)
		return -EINVAL;

	if (rtsmem->initialized)
		rtscam_mem_release(rtsmem);

	if (!size)
		return -EINVAL;

	if (virt_addr) {
		mem_base = (void __iomem *)virt_addr;
		rtsmem->flag = 0;
	} else {
		mem_base = ioremap(device_addr, size);
		if (!mem_base)
			return -EINVAL;
		rtsmem->flag = 1;
	}

	rtsmem->virt_base = (void *)mem_base;
	rtsmem->device_base = device_addr;
	rtsmem->pfn_base = PFN_DOWN(device_addr);
	rtsmem->size = pages;
	rtsmem->pre_setted = 0;

	INIT_LIST_HEAD(&rtsmem->memorys);
	INIT_LIST_HEAD(&rtsmem->memorys2);
	INIT_LIST_HEAD(&rtsmem->idles);

	__init_idles(rtsmem);
	mutex_init(&rtsmem->lock);

	rtsmem->dev = get_device(dev);
	rtsmem->initialized = 1;

	rtsprintk(RTS_TRACE_DEBUG, "%s v:0x%08lx p:0x%08lx s:0x%08x\n",
		  __func__,
		  (unsigned long)rtsmem->virt_base,
		  (unsigned long)rtsmem->device_base,
		  rtsmem->size);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_init);

static void __del_mem_item(struct rtscam_mem_info *rtsmem,
			   struct rtscam_mem_item *item)
{
	if (!item)
		return;

	list_del_init(&item->ext_list);
	list_del_init(&item->list);
	list_add_tail(&item->list, &rtsmem->idles);
}

int rtscam_mem_release(struct rtscam_mem_info *rtsmem)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *tmp;

	if (!rtsmem || !rtsmem->initialized)
		return 0;

	if (rtsmem->flag && rtsmem->virt_base)
		iounmap((void __iomem *)rtsmem->virt_base);
	rtsmem->virt_base = NULL;

	mutex_lock(&rtsmem->lock);
	list_for_each_entry_safe(item, tmp, &rtsmem->memorys, list) {
		__del_mem_item(rtsmem, item);
	}

	list_for_each_entry_safe(item, tmp, &rtsmem->memorys2, list) {
		__del_mem_item(rtsmem, item);
	}

	__release_idles(rtsmem);
	mutex_unlock(&rtsmem->lock);

	put_device(rtsmem->dev);
	rtsmem->dev = NULL;
	rtsmem->virt_base = NULL;
	rtsmem->device_base = 0;
	rtsmem->pfn_base = 0;
	rtsmem->size = 0;
	rtsmem->pre_setted = 0;
	rtsmem->initialized = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_release);

static struct rtscam_mem_item *__new_mem_item(struct rtscam_mem_info *rtsmem,
					      struct list_head *head,
					      long offset, long page_cnt)
{
	struct rtscam_mem_item *item;

	if (list_empty(&rtsmem->idles)) {
		item = __alloc_mem_item();
	} else {
		item = list_first_entry(&rtsmem->idles,
					struct rtscam_mem_item, list);
		list_del_init(&item->list);
	}

	if (!item)
		return NULL;

	item->offset = offset;
	item->page_cnt = page_cnt;
	item->phy_addr = rtsmem->device_base + (item->offset << PAGE_SHIFT);
	item->size = (item->page_cnt << PAGE_SHIFT);
	item->creator = current->pid;
	item->owner = current->pid;
	item->buf_io = 0;
	item->buf_flag = 0;
	item->index = -1;

	list_add(&item->list, head);

	return item;
}

static struct rtscam_mem_item *__alloc_p(struct rtscam_mem_info *rtsmem,
			long page_cnt)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *prev = NULL;
	long offset = 0;
	int found = 0;

	list_for_each_entry(item, &rtsmem->memorys, list) {
		if (item->offset - offset >= page_cnt) {
			found = 1;
			goto exit;
		}
		offset = item->offset + item->page_cnt;
		prev = item;
	}

	if (rtsmem->size - offset >= page_cnt)
		found = 1;
exit:
	if (!found)
		return NULL;

	if (prev)
		return __new_mem_item(rtsmem, &prev->list, offset, page_cnt);

	return __new_mem_item(rtsmem, &rtsmem->memorys, offset, page_cnt);
}

static struct rtscam_mem_item *__alloc_n(struct rtscam_mem_info *rtsmem,
					 long page_cnt, int flag_offset)
{
	struct rtscam_mem_item *item;
	long offset;
	int found = 0;

	if (flag_offset)
		offset = rtsmem->size - RTS_MEM_ALLOC_FROM_END_OFFSET;
	else
		offset = rtsmem->size;

	list_for_each_entry_reverse(item, &rtsmem->memorys, list) {
		if (offset <= (long)item->offset)
			continue;

		if (offset - (long)item->offset -
				(long)item->page_cnt >= page_cnt) {
			found = 1;
			goto exit;
		}
		offset = item->offset;
	}
	item = NULL;

	if (offset >= page_cnt)
		found = 1;

exit:
	if (!found)
		return NULL;

	offset -= page_cnt;

	if (item)
		item = __new_mem_item(rtsmem, &item->list, offset, page_cnt);
	else
		item = __new_mem_item(rtsmem,
			&rtsmem->memorys, offset, page_cnt);

	return item;
}

static struct rtscam_mem_item *__find_mem(struct rtscam_mem_info *rtsmem,
			long offset)
{
	struct rtscam_mem_item *item;

	list_for_each_entry(item, &rtsmem->memorys, list) {
		if (item->offset <= offset &&
		    item->offset + item->page_cnt > offset)
			return item;
	}

	return NULL;
}

static struct rtscam_mem_item *__find_mem2(struct rtscam_mem_info *rtsmem,
			dma_addr_t phy_addr)
{
	struct rtscam_mem_item *item;

	list_for_each_entry(item, &rtsmem->memorys2, list) {
		if (item->phy_addr <= phy_addr &&
		    item->phy_addr + item->size > phy_addr)
			return item;
	}

	return NULL;
}

static int __check_phyaddr_assign(struct rtscam_mem_info *rtsmem,
				  long offset, long page_cnt,
				  struct rtscam_mem_item **pprev)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *prev = NULL;
	int found = 0;

	list_for_each_entry(item, &rtsmem->memorys, list) {
		if (item->offset < offset) {
			prev = item;
			continue;
		}

		if (item->offset == offset)
			return 0;

		if (item->offset > offset) {
			if (item->offset - offset >= page_cnt) {
				if (prev && offset - prev->offset >=
				    prev->page_cnt)
					found = 1;
				if (!prev)
					found = 1;
			}
			goto exit;
		}
	}

	if (rtsmem->size - offset >= page_cnt) {
		if (prev && offset - prev->offset >= prev->page_cnt)
			found = 1;
		if (!prev)
			found = 1;
	}
exit:
	*pprev = prev;
	return found;
}

static struct rtscam_mem_item *__find_mem_from_setting(
			struct rtscam_mem_info *rtsmem,
			long page_cnt, const char *name,
			dma_addr_t phy_addr, int32_t idx, int flag_alloc)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *tmp = NULL;

	if (!name)
		return NULL;

	list_for_each_entry(item, &rtsmem->memorys, list) {
		if (item->info[0] == 0)
			continue;

		if (strcmp(item->info, name))
			continue;

		if (page_cnt && item->page_cnt != page_cnt)
			continue;

		if (phy_addr) {
			long start, end;

			start = (phy_addr - rtsmem->device_base) >> PAGE_SHIFT;
			end = start + page_cnt;
			if (start < item->offset ||
					end > (item->offset + item->page_cnt))
				continue;
		}

		if ((item->buf_flag & RTSMEM_IN_USE) && flag_alloc)
			continue;

		if (!(item->buf_flag & RTSMEM_PRE_ALLOCATED) && flag_alloc)
			continue;

		if (phy_addr)
			break;

		if (idx != -1) {
			if (item->index == idx)
				return item;
			continue;
		}

		if (tmp) {
			if (tmp->page_cnt > item->page_cnt)
				tmp = item;
		} else {
			tmp = item;
		}
	}
	return tmp;
}

void *rtscam_mem_alloc2(struct rtscam_mem_info *rtsmem,
		size_t size, dma_addr_t *phy_addr, uint32_t io, char *name)
{
	struct rtscam_mem_item *mem = NULL;
	long page_cnt;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	if (size == 0)
		return NULL;

	size = PAGE_ALIGN(size);
	page_cnt = size >> PAGE_SHIFT;

	mutex_lock(&rtsmem->lock);
	mem = __new_mem_item(rtsmem, &rtsmem->memorys2, 0, page_cnt);
	if (!mem) {
		mutex_unlock(&rtsmem->lock);
		return NULL;
	}

	mem->virt_addr = dma_alloc_coherent(rtsmem->dev, size,
			&mem->phy_addr, GFP_KERNEL);
	if (!mem->virt_addr) {
		__del_mem_item(rtsmem, mem);
		mutex_unlock(&rtsmem->lock);
		return NULL;
	}
	mutex_unlock(&rtsmem->lock);

	mem->buf_flag |= RTSMEM_IN_USE;
	mem->buf_io = io;
	if (name)
		strncpy(mem->info, name, sizeof(mem->info) - 1);

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]+++!%8ld %8ld\n", current->pid,
		  (unsigned long)mem->phy_addr, mem->page_cnt);

	if (phy_addr)
		*phy_addr = mem->phy_addr;

	return mem->virt_addr;
}
EXPORT_SYMBOL_GPL(rtscam_mem_alloc2);

void rtscam_mem_free2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr)
{
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized || !phy_addr)
		return;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem2(rtsmem, phy_addr);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR, "find mem fail\n");
		mutex_unlock(&rtsmem->lock);
		return;
	}

	dma_free_coherent(rtsmem->dev, mem->size, mem->virt_addr,
			mem->phy_addr);
	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]---!%8ld %8ld\n", current->pid,
		  (unsigned long)mem->phy_addr, mem->page_cnt);

	__del_mem_item(rtsmem, mem);
	mutex_unlock(&rtsmem->lock);
}
EXPORT_SYMBOL_GPL(rtscam_mem_free2);

/*
 *  dir: 0 - default, 1 - end, 2 - begin
 */
void *rtscam_mem_alloc(struct rtscam_mem_info *rtsmem,
		       size_t size, dma_addr_t *phy_addr,
		       uint32_t io, uint8_t dir, const char *name)
{
	long page_cnt;
	long offset;
	struct rtscam_mem_item *mem = NULL;
	int use_setted = 0;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	if (size == 0)
		return NULL;

	size = PAGE_ALIGN(size);
	page_cnt = size >> PAGE_SHIFT;

	mutex_lock(&rtsmem->lock);
	if (rtsmem->pre_setted) {
		mem = __find_mem_from_setting(rtsmem, page_cnt, name, 0, -1, 1);
		if (!mem) {
			if (dir == RTSMEM_ALLOC_END)
				mem = __alloc_n(rtsmem, page_cnt, 0);
			else
				mem = __alloc_p(rtsmem, page_cnt);
		} else {
			use_setted = 1;
		}
	} else {
		if (dir == RTSMEM_ALLOC_END) {
			mem = __alloc_n(rtsmem, page_cnt, 1);
		} else if (dir == RTSMEM_ALLOC_BEGIN) {
			mem = __alloc_p(rtsmem, page_cnt);
		} else if (page_cnt <= RTS_MEM_SIZE_SMALL) {
			mem = __alloc_n(rtsmem, page_cnt, 0);
			if (!mem)
				mem = __alloc_n(rtsmem, page_cnt, 1);
		} else {
			mem = __alloc_p(rtsmem, page_cnt);
		}
	}
	mutex_unlock(&rtsmem->lock);

	if (!mem)
		return NULL;

	if (!use_setted) {
		mem->buf_io = io;
		if (name)
			strncpy(mem->info, name, sizeof(mem->info) - 1);
	}
	mem->buf_flag |= RTSMEM_IN_USE;

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]++++%8ld %8ld\n", current->pid,
		  mem->offset, mem->page_cnt);

	offset = (mem->offset << PAGE_SHIFT);

	if (phy_addr)
		*phy_addr = mem->phy_addr;

	return rtsmem->virt_base + offset;
}
EXPORT_SYMBOL_GPL(rtscam_mem_alloc);

void *rtscam_mem_realloc(struct rtscam_mem_info *rtsmem,
		size_t size, dma_addr_t *phy_addr, uint32_t io, char *name)
{
	long page_cnt;
	long offset;
	struct rtscam_mem_item *mem = NULL;
	struct rtscam_mem_item *prev_item = NULL;
	int use_setted = 0;

	if (!rtsmem || !rtsmem->initialized || !phy_addr)
		return NULL;

	if (size == 0)
		return NULL;

	if (*phy_addr < rtsmem->device_base)
		return NULL;

	if (*phy_addr > (rtsmem->device_base + (rtsmem->size << PAGE_SHIFT)))
		return NULL;

	size = PAGE_ALIGN(size);
	page_cnt = size >> PAGE_SHIFT;
	offset = (*phy_addr - rtsmem->device_base) >> PAGE_SHIFT;

	mutex_lock(&rtsmem->lock);
	if (rtsmem->pre_setted) {
		mem = __find_mem_from_setting(rtsmem, page_cnt,
				name, *phy_addr, -1, 1);
		if (mem) {
			use_setted = 1;
			goto next;
		}
	}

	if (!__check_phyaddr_assign(rtsmem, offset, page_cnt, &prev_item))
		goto next;

	if (prev_item)
		mem = __new_mem_item(rtsmem,
				     &prev_item->list, offset, page_cnt);
	else
		mem = __new_mem_item(rtsmem,
				     &rtsmem->memorys, offset, page_cnt);
	if (name)
		strncpy(mem->info, name, sizeof(mem->info) - 1);
next:
	mutex_unlock(&rtsmem->lock);
	if (!mem)
		return NULL;

	if (!use_setted)
		mem->buf_io = io;
	mem->buf_flag |= RTSMEM_IN_USE;

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]++++%8ld %8ld\n", current->pid,
		  mem->offset, mem->page_cnt);

	offset = (mem->offset << PAGE_SHIFT);
	*phy_addr = mem->phy_addr;

	return rtsmem->virt_base + offset;
}
EXPORT_SYMBOL_GPL(rtscam_mem_realloc);

void rtscam_mem_free(struct rtscam_mem_info *rtsmem, size_t size,
		     void *vaddr, dma_addr_t phy_addr)
{
	long page_cnt;
	long start;
	long end;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return;

	if (!vaddr || !phy_addr)
		return;

	if (size == 0)
		return;

	if (vaddr - rtsmem->virt_base != phy_addr - rtsmem->device_base) {
		rtsprintk(RTS_TRACE_ERROR, "<%s, %d>invalid memory for free\n",
			  __func__, __LINE__);
		return;
	}

	size = PAGE_ALIGN(size);
	page_cnt = size >> PAGE_SHIFT;
	start = (phy_addr - rtsmem->device_base) >> PAGE_SHIFT;
	end = start + page_cnt;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem(rtsmem, start);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR, "find mem fail\n");
		mutex_unlock(&rtsmem->lock);
		return;
	}

	if (end > mem->offset + mem->page_cnt) {
		rtsprintk(RTS_TRACE_ERROR, "invalid mem size\n");
		mutex_unlock(&rtsmem->lock);
		return;
	}

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]----%8ld %8ld\n", current->pid,
		  mem->offset, mem->page_cnt);

	mem->buf_flag &= ~RTSMEM_IN_USE;
	if (!(mem->buf_flag & RTSMEM_PRE_ALLOCATED))
		__del_mem_item(rtsmem, mem);
	mutex_unlock(&rtsmem->lock);
}
EXPORT_SYMBOL_GPL(rtscam_mem_free);

void rtscam_mem_free_V2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr)
{
	long offset;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return;

	offset = (phy_addr - rtsmem->device_base) >> PAGE_SHIFT;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem(rtsmem, offset);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR, "find mem fail\n");
		mutex_unlock(&rtsmem->lock);
		return;
	}

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d]----%8ld %8ld\n", current->pid,
		  mem->offset, mem->page_cnt);

	mem->buf_flag &= ~RTSMEM_IN_USE;
	if (!(mem->buf_flag & RTSMEM_PRE_ALLOCATED))
		__del_mem_item(rtsmem, mem);
	mutex_unlock(&rtsmem->lock);
}
EXPORT_SYMBOL_GPL(rtscam_mem_free_V2);

dma_addr_t rtscam_mem_convert_v2d(struct rtscam_mem_info *rtsmem, void *vaddr)
{
	off_t offset;

	if (!rtsmem || !rtsmem->initialized)
		return 0;

	if (vaddr < rtsmem->virt_base ||
	    vaddr >= rtsmem->virt_base + (rtsmem->size << PAGE_SHIFT)) {
		rtsprintk(RTS_TRACE_ERROR, "invalid vaddr : 0x%08lx\n",
			  (long)vaddr);
		return 0;
	}

	offset = vaddr - rtsmem->virt_base;
	if (offset % PAGE_SIZE) {
		rtsprintk(RTS_TRACE_ERROR, "invalid offset: 0x%08lx\n",
			  offset);
		return 0;
	}

	return rtsmem->device_base + offset;
}
EXPORT_SYMBOL_GPL(rtscam_mem_convert_v2d);

void *rtscam_mem_convert_d2v(struct rtscam_mem_info *rtsmem,
			     dma_addr_t phy_addr)
{
	off_t offset;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	if (phy_addr < rtsmem->device_base ||
	    phy_addr >= rtsmem->device_base + (rtsmem->size << PAGE_SHIFT)) {
		rtsprintk(RTS_TRACE_ERROR, "invalid phy_addr : 0x%08lx\n",
			  (long)phy_addr);
		return NULL;
	}

	offset = phy_addr - rtsmem->device_base;
	if (offset % PAGE_SIZE) {
		rtsprintk(RTS_TRACE_ERROR, "invalid offset: 0x%08lx\n",
			  offset);
		return NULL;
	}

	return rtsmem->virt_base + offset;
}
EXPORT_SYMBOL_GPL(rtscam_mem_convert_d2v);

static long __get_used_count(struct rtscam_mem_info *rtsmem)
{
	struct rtscam_mem_item *mem;
	long used_cnt = 0;

	list_for_each_entry(mem, &rtsmem->memorys, list) {
		if (!(mem->buf_flag & RTSMEM_IN_USE))
			continue;
		used_cnt += mem->page_cnt;
	}

	return used_cnt;
}

long rtscam_mem_get_left_size(struct rtscam_mem_info *rtsmem)
{
	long used_cnt;

	if (!rtsmem || !rtsmem->initialized)
		return 0;

	mutex_lock(&rtsmem->lock);
	used_cnt = __get_used_count(rtsmem);
	mutex_unlock(&rtsmem->lock);

	return (rtsmem->size - used_cnt) << PAGE_SHIFT;
}
EXPORT_SYMBOL_GPL(rtscam_mem_get_left_size);

long rtscam_mem_get_used_size(struct rtscam_mem_info *rtsmem)
{
	long used_cnt;

	if (!rtsmem || !rtsmem->initialized)
		return 0;

	mutex_lock(&rtsmem->lock);
	used_cnt = __get_used_count(rtsmem);
	mutex_unlock(&rtsmem->lock);

	return used_cnt << PAGE_SHIFT;
}
EXPORT_SYMBOL_GPL(rtscam_mem_get_used_size);

long rtscam_mem_get_total_size(struct rtscam_mem_info *rtsmem)
{
	if (!rtsmem || !rtsmem->initialized)
		return 0;

	return rtsmem->size << PAGE_SHIFT;
}
EXPORT_SYMBOL_GPL(rtscam_mem_get_total_size);

int rtscam_mem_check(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr,
		     size_t size, struct rtscam_mem_item **ppitem)
{
	long offset;
	long page_cnt;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	if (phy_addr < rtsmem->device_base)
		return -EINVAL;

	if (phy_addr + size >
	    rtsmem->device_base + (rtsmem->size << PAGE_SHIFT))
		return -EINVAL;

	offset = (phy_addr - rtsmem->device_base) >> PAGE_SHIFT;
	page_cnt = PAGE_ALIGN(size) >> PAGE_SHIFT;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem(rtsmem, offset);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR,
			  "find mem fail at %s\n", __func__);
		mutex_unlock(&rtsmem->lock);
		return -EINVAL;
	}
	mutex_unlock(&rtsmem->lock);

	if (offset + page_cnt > mem->offset + mem->page_cnt) {
		rtsprintk(RTS_TRACE_ERROR,
			  "memory out of range at %s\n", __func__);
		return -EINVAL;
	}

	if (ppitem)
		*ppitem = mem;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_check);

int rtscam_mem_check2(struct rtscam_mem_info *rtsmem, dma_addr_t phy_addr,
		size_t size, struct rtscam_mem_item **ppitem)
{
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem2(rtsmem, phy_addr);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR,
			  "find mem fail at %s\n", __func__);
		mutex_unlock(&rtsmem->lock);
		return -EINVAL;
	}
	mutex_unlock(&rtsmem->lock);

	if (phy_addr + size > mem->phy_addr + mem->size) {
		rtsprintk(RTS_TRACE_ERROR,
			  "memory out of range at %s\n", __func__);
		return -EINVAL;
	}

	if (ppitem)
		*ppitem = mem;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_check2);

int rtscam_mem_mmap(struct rtscam_mem_info *rtsmem,
		    struct vm_area_struct *vma, void *cpu_addr,
		    dma_addr_t dma_addr, size_t size)
{
	int ret;
	unsigned long user_count = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	unsigned long count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long start = (dma_addr - rtsmem->device_base) >> PAGE_SHIFT;
	unsigned long pfn = rtsmem->pfn_base + start;
	unsigned long off = vma->vm_pgoff;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	ret = rtscam_mem_check(rtsmem, dma_addr, size, &mem);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "invalid 0x%08lx : 0x%08lx for mmap\n",
			  (unsigned long)dma_addr, (unsigned long)size);
		return -EINVAL;
	}

	if (dma_addr - rtsmem->device_base != cpu_addr - rtsmem->virt_base) {
		rtsprintk(RTS_TRACE_ERROR, "invalid cpu_addr : 0x%08lx\n",
			  (unsigned long)cpu_addr);
		return -EINVAL;
	}

	ret = -ENXIO;
	if (mem->buf_io == RTSCAM_ISP_BUF_COHERENT)
		vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);

	if (off < count && user_count <= (count - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      pfn + off,
				      user_count << PAGE_SHIFT,
				      vma->vm_page_prot);
	}
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "maping dma memory fail, error =  %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_mmap);

int rtscam_mem_mmap2(struct rtscam_mem_info *rtsmem,
		struct vm_area_struct *vma, dma_addr_t dma_addr, size_t size)
{
	int ret;
	unsigned long user_count = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	unsigned long count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	unsigned long off = vma->vm_pgoff;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	ret = rtscam_mem_check2(rtsmem, dma_addr, size, &mem);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "invalid 0x%08lx : 0x%08lx for mmap\n",
			  (unsigned long)dma_addr, (unsigned long)size);
		return -EINVAL;
	}

	ret = -ENXIO;
	if (mem->buf_io == RTSCAM_ISP_BUF_COHERENT)
		vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);

	if (off < count && user_count <= (count - off)) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      PFN_DOWN(mem->phy_addr),
				      user_count << PAGE_SHIFT,
				      vma->vm_page_prot);
	}
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "maping dma memory fail, error =  %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_mmap2);

struct rtscam_mem_item *rtscam_mem_enum(struct rtscam_mem_info *rtsmem,
					int index)
{
	int idx = 0, idx2;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	mutex_lock(&rtsmem->lock);
	list_for_each_entry(mem, &rtsmem->memorys, list) {
		if (idx == index) {
			mutex_unlock(&rtsmem->lock);
			return mem;
		}
		idx++;
	}

	idx2 = idx;
	idx = 0;
	list_for_each_entry(mem, &rtsmem->memorys2, list) {
		if (idx + idx2 == index) {
			mutex_unlock(&rtsmem->lock);
			return mem;
		}
		idx++;
	}
	mutex_unlock(&rtsmem->lock);
	rtsprintk(RTS_TRACE_DEBUG, "enum isp dma %d fail\n", index);
	return NULL;
}
EXPORT_SYMBOL_GPL(rtscam_mem_enum);

static struct rtscam_mem_item *__rtscam_mem_find(struct rtscam_mem_info *rtsmem,
					dma_addr_t phy_addr)
{
	long offset;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	offset = (phy_addr - rtsmem->device_base) >> PAGE_SHIFT;

	return __find_mem(rtsmem, offset);
}

struct rtscam_mem_item *rtscam_mem_find(struct rtscam_mem_info *rtsmem,
					dma_addr_t phy_addr)
{
	struct rtscam_mem_item *mem;

	mutex_lock(&rtsmem->lock);
	mem = __rtscam_mem_find(rtsmem, phy_addr);
	mutex_unlock(&rtsmem->lock);

	return mem;
}
EXPORT_SYMBOL_GPL(rtscam_mem_find);

struct rtscam_mem_item *rtscam_mem_find2(struct rtscam_mem_info *rtsmem,
					dma_addr_t phy_addr)
{
	struct rtscam_mem_item *mem;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	mutex_lock(&rtsmem->lock);
	mem = __find_mem2(rtsmem, phy_addr);
	mutex_unlock(&rtsmem->lock);

	return mem;
}
EXPORT_SYMBOL_GPL(rtscam_mem_find2);

int rtscam_mem_number(struct rtscam_mem_info *rtsmem, int flag)
{
	struct rtscam_mem_item *mem = NULL;
	int num = 0;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	switch (flag) {
	case RTSMEM_PRE_ALLOCATED:
	case RTSMEM_IN_USE:
	case RTSMEM_PROBE_ALLOCATED:
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&rtsmem->lock);
	list_for_each_entry(mem, &rtsmem->memorys, list) {
		if (mem->buf_flag & flag)
			num++;
	}
	mutex_unlock(&rtsmem->lock);

	return num;
}
EXPORT_SYMBOL_GPL(rtscam_mem_number);

int rtscam_mem_number2(struct rtscam_mem_info *rtsmem, int flag)
{
	struct rtscam_mem_item *mem = NULL;
	int num = 0;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	switch (flag) {
	case RTSMEM_PRE_ALLOCATED:
	case RTSMEM_IN_USE:
	case RTSMEM_PROBE_ALLOCATED:
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&rtsmem->lock);
	list_for_each_entry(mem, &rtsmem->memorys2, list) {
		if (mem->buf_flag & flag)
			num++;
	}
	mutex_unlock(&rtsmem->lock);

	return num;
}
EXPORT_SYMBOL_GPL(rtscam_mem_number2);

unsigned long *rtscam_mem_get_bitmap(struct rtscam_mem_info *rtsmem)
{
	unsigned long *bitmap;
	int bitmap_size;
	struct rtscam_mem_item *mem;

	if (!rtsmem || !rtsmem->initialized)
		return NULL;

	bitmap_size = BITS_TO_LONGS(rtsmem->size) * sizeof(long);
	bitmap = vzalloc(bitmap_size);
	if (!bitmap)
		return NULL;

	mutex_lock(&rtsmem->lock);
	list_for_each_entry(mem, &rtsmem->memorys, list) {
		long i;

		if (!(mem->buf_flag & RTSMEM_IN_USE))
			continue;

		for (i = 0; i < mem->page_cnt; i++)
			set_bit(mem->offset + i, bitmap);
	}
	mutex_unlock(&rtsmem->lock);

	return bitmap;
}
EXPORT_SYMBOL_GPL(rtscam_mem_get_bitmap);

void rtscam_mem_put_bitmap(unsigned long *bitmap)
{
	if (bitmap)
		vfree(bitmap);
}
EXPORT_SYMBOL_GPL(rtscam_mem_put_bitmap);

int rtscam_vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}
EXPORT_SYMBOL_GPL(rtscam_vma_is_io);

int rtscam_vaddr_is_io(unsigned long vaddr)
{
	struct vm_area_struct *vma;

	vma = find_vma(current->mm, vaddr);
	if (!vma) {
		rtsprintk(RTS_TRACE_ERROR, "no vma for address 0x%lx\n", vaddr);
		return 0;
	}

	return rtscam_vma_is_io(vma);
}
EXPORT_SYMBOL_GPL(rtscam_vaddr_is_io);

int rtscam_get_user_pages(unsigned long start, struct page **pages,
			  int n_pages, struct vm_area_struct *vma, int write)
{
	if (rtscam_vma_is_io(vma)) {
		unsigned int i;

		for (i = 0; i < n_pages; ++i, start += PAGE_SIZE) {
			unsigned long pfn;
			int ret = follow_pfn(vma, start, &pfn);

			if (ret) {
				pr_err("no page for address 0x%lx\n", start);
				return ret;
			}
			pages[i] = pfn_to_page(pfn);
		}
	} else {
		long n;

		n = get_user_pages(start & PAGE_MASK, n_pages,
				   write ? FOLL_WRITE : 0, pages);
		/* negative error means that no page was pinned */
		n = max(n, 0L);
		if (n != n_pages) {
			pr_err("got only %ld of %d user pages\n", n, n_pages);
			while (n)
				put_page(pages[--n]);
			return -EFAULT;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_get_user_pages);

static void rtscam_put_dirty_page(struct page *page)
{
	set_page_dirty_lock(page);
	put_page(page);
}

static void rtscam_sgt_foreach_page(struct sg_table *sgt,
				    void (*cb)(struct page *pg))
{
	struct scatterlist *s;
	unsigned int i;

	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		struct page *page = sg_page(s);
		unsigned int n_pages = PAGE_ALIGN(s->offset + s->length)
				       >> PAGE_SHIFT;
		unsigned int j;

		for (j = 0; j < n_pages; ++j, ++page)
			cb(page);
	}
}

struct rtscam_sg_buf *rtscam_get_sg_buf(struct rtscam_mem_info *rtsmem,
					unsigned long vaddr,
					unsigned long size, int write)
{
	struct rtscam_sg_buf *buf;
	unsigned long start;
	unsigned long offset;
	unsigned long end;
	int n_pages;
	struct vm_area_struct *vma;
	struct page **pages;
	struct sg_table *sgt;
	unsigned long dma_align = dma_get_cache_alignment();
	int ret;

	if (!size) {
		rtsprintk(RTS_TRACE_ERROR, "size is zero\n");
		return ERR_PTR(-EINVAL);
	}

	if (!IS_ALIGNED(vaddr | size, dma_align)) {
		pr_debug("user data must be aligned to %lu bytes\n", dma_align);
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->rtsmem = rtsmem;
	buf->dev = rtsmem->dev;
	buf->dma_dir = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	start = vaddr & PAGE_MASK;
	offset = vaddr & ~PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);
	n_pages = (end - start) >> PAGE_SHIFT;

	pages = kmalloc_array(n_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		rtsprintk(RTS_TRACE_ERROR, "failed to allocate pages table\n");
		goto fail_buf;
	}

	vma = find_vma(current->mm, vaddr);
	if (!vma) {
		rtsprintk(RTS_TRACE_ERROR, "no vma for address 0x%lx\n", vaddr);
		ret = -EFAULT;
		goto fail_pages;
	}

	if (vma->vm_end < vaddr + size) {
		rtsprintk(RTS_TRACE_ERROR,
			  "vma at 0x%lx is too small for 0x%lx bytes\n",
			  vaddr, size);
		ret = -EFAULT;
		goto fail_pages;
	}

	buf->vma = vma;
	ret = rtscam_get_user_pages(start, pages, n_pages, vma, write);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "failed to get user pages\n");
		goto  fail_vma;
	}

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		rtsprintk(RTS_TRACE_ERROR, "failed to allocate sg table\n");
		ret = -ENOMEM;
		goto fail_get_user_pages;
	}
	ret = sg_alloc_table_from_pages(sgt, pages, n_pages,
					offset, size, GFP_KERNEL);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "failed to initialize sg table\n");
		goto fail_sgt;
	}

	sgt->nents = dma_map_sg(buf->dev, sgt->sgl, sgt->orig_nents,
				buf->dma_dir);
	if (sgt->nents <= 0) {
		pr_err("failed to map scatterlist\n");
		ret = -EIO;
		goto fail_sgt_init;
	}

	/* pages are no longer needed */
	kfree(pages);
	pages = NULL;

	buf->vaddr = vaddr;
	buf->size = size;
	buf->sgt = sgt;

	return buf;

fail_sgt_init:
	if (!rtscam_vma_is_io(buf->vma))
		rtscam_sgt_foreach_page(sgt, put_page);
	sg_free_table(sgt);

fail_sgt:
	kfree(sgt);

fail_get_user_pages:
	if (pages && !rtscam_vma_is_io(buf->vma))
		while (n_pages)
			put_page(pages[--n_pages]);
fail_vma:

fail_pages:
	kfree(pages); /* kfree is NULL-proof */

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(rtscam_get_sg_buf);

void rtscam_put_sg_buf(struct rtscam_sg_buf *buf)
{
	struct sg_table *sgt = buf->sgt;

	dma_unmap_sg(buf->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
	if (!rtscam_vma_is_io(buf->vma))
		rtscam_sgt_foreach_page(sgt, rtscam_put_dirty_page);

	sg_free_table(sgt);
	kfree(sgt);
	kfree(buf);
}
EXPORT_SYMBOL_GPL(rtscam_put_sg_buf);

int rtscam_mem_set_info(struct rtscam_mem_info *rtsmem,
			dma_addr_t phy_addr, const char *info)
{
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !phy_addr || !info)
		return -EINVAL;

	mutex_lock(&rtsmem->lock);
	mem = __rtscam_mem_find(rtsmem, phy_addr);
	if (!mem) {
		mutex_unlock(&rtsmem->lock);
		return -EINVAL;
	}

	strncpy(mem->info, info, sizeof(mem->info) - 1);
	mutex_unlock(&rtsmem->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_set_info);

int rtscam_mem_get_info(struct rtscam_mem_info *rtsmem,
			struct rtscam_mem_item *mem)
{
	struct rtscam_mem_item *item;

	if (!rtsmem || !mem)
		return -EINVAL;

	mutex_lock(&rtsmem->lock);
	item = __find_mem_from_setting(rtsmem, 0, mem->info, 0, mem->index, 0);
	mutex_unlock(&rtsmem->lock);
	if (!item)
		return -EINVAL;

	mem->phy_addr = item->phy_addr;
	mem->size = item->size;
	return 0;
}

int rtscam_mem_add_property(struct rtscam_mem_info *rtsmem,
			dma_addr_t phy_addr, uint32_t property)
{
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !phy_addr || !property)
		return -EINVAL;

	mutex_lock(&rtsmem->lock);
	mem = __rtscam_mem_find(rtsmem, phy_addr);
	if (!mem) {
		mutex_unlock(&rtsmem->lock);
		return -EINVAL;
	}

	mem->buf_flag |= property;
	mutex_unlock(&rtsmem->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_mem_add_property);

int rtscam_mem_pre_alloc(struct rtscam_mem_info *rtsmem,
		       size_t size, const char *name, uint32_t io, int32_t idx)
{
	long page_cnt;
	struct rtscam_mem_item *mem = NULL;

	if (!rtsmem || !rtsmem->initialized || !rtsmem->pre_setted || !name)
		return -EINVAL;

	if (size == 0)
		return -EINVAL;

	size = PAGE_ALIGN(size);
	page_cnt = size >> PAGE_SHIFT;

	mem = __alloc_p(rtsmem, page_cnt);
	if (!mem)
		return -ENOMEM;

	mem->buf_flag |= RTSMEM_PRE_ALLOCATED;
	mem->buf_io = io;
	mem->index = idx;

	strncpy(mem->info, name, sizeof(mem->info) - 1);

	rtsprintk(RTS_TRACE_MEMINFO, "[%4d] pre_alloc: %s %8ld %8ld\n",
			current->pid, name, mem->offset, mem->page_cnt);
	return 0;
}

int rtscam_mem_get_pre_alloc(struct rtscam_mem_info *rtsmem, int *status)
{
	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	*status = rtsmem->pre_setted;

	return 0;
}

int rtscam_mem_set_pre_alloc(struct rtscam_mem_info *rtsmem, int status)
{
	struct rtscam_mem_item *item;
	struct rtscam_mem_item *tmp;
	int ret = -1;

	if (!rtsmem || !rtsmem->initialized)
		return -EINVAL;

	mutex_lock(&rtsmem->lock);
	if (status) {
		if (rtsmem->pre_setted) {
			rtsprintk(RTS_TRACE_ERROR,
				"please release pre alloc firstly\n");
			goto exit;
		}

		list_for_each_entry_safe(item, tmp, &rtsmem->memorys, list) {
			if (item->buf_flag & RTSMEM_PRE_ALLOCATED) {
				rtsprintk(RTS_TRACE_ERROR,
					"please release pre alloc %s\n",
					item->info);
				goto exit;
			}

			if (!(item->buf_flag & RTSMEM_PROBE_ALLOCATED)
				&& (item->buf_flag & RTSMEM_IN_USE)) {
				rtsprintk(RTS_TRACE_ERROR,
					"please release used buffer\n");
				goto exit;
			}
		}

		rtsmem->pre_setted = 1;
		ret = 0;
	} else {
		if (!rtsmem->pre_setted) {
			ret = 0;
			goto exit;
		}

		list_for_each_entry_safe(item, tmp, &rtsmem->memorys, list) {
			if ((item->buf_flag & RTSMEM_PRE_ALLOCATED)
					&& (item->buf_flag & RTSMEM_IN_USE)) {
				rtsprintk(RTS_TRACE_ERROR,
					"please release used buffer\n");
				goto exit;
			}
		}

		list_for_each_entry_safe(item, tmp, &rtsmem->memorys, list) {
			if (item->buf_flag & RTSMEM_PRE_ALLOCATED)
				__del_mem_item(rtsmem, item);
		}

		rtsmem->pre_setted = 0;
		ret = 0;
	}
exit:
	mutex_unlock(&rtsmem->lock);
	return ret;
}
