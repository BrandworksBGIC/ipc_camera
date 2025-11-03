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

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/refcount.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>
#include "rts-dma-contig.h"
#include "rts_isp_mem.h"

struct rts_dc_buf {
	struct rtscam_mem_info	*rtsmem;
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	dma_addr_t			dma_addr;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;
	int                             buf_io;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	refcount_t			refcount;

	struct vb2_buffer		*vb;
	bool				non_coherent_mem;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static unsigned long rts_dc_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *rts_dc_cookie(struct vb2_buffer *vb, void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;

	return &buf->dma_addr;
}

/*
 * This function may fail if:
 *
 * - dma_buf_vmap() fails
 *   E.g. due to lack of virtual mapping address space, or due to
 *   dmabuf->ops misconfiguration.
 *
 * - dma_vmap_noncontiguous() fails
 *   For instance, when requested buffer size is larger than totalram_pages().
 *   Relevant for buffers that use non-coherent memory.
 *
 * - Queue DMA attrs have DMA_ATTR_NO_KERNEL_MAPPING set
 *   Relevant for buffers that use coherent memory.
 */
static void *rts_dc_vaddr(struct vb2_buffer *vb, void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;

	if (buf->vaddr)
		return buf->vaddr;

	if (buf->non_coherent_mem)
		buf->vaddr = dma_vmap_noncontiguous(buf->dev, buf->size,
						    buf->dma_sgt);
	return buf->vaddr;
}

static unsigned int rts_dc_num_users(void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;

	return refcount_read(&buf->refcount);
}

static void rts_dc_prepare(void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* This takes care of DMABUF and user-enforced cache sync hint */
	if (buf->vb->skip_cache_sync_on_prepare)
		return;

	if (!buf->non_coherent_mem || !buf->buf_io)
		return;

	/* Non-coherent MMAP only */
	if (buf->vaddr)
		flush_kernel_vmap_range(buf->vaddr, buf->size);

	/* For both USERPTR and non-coherent MMAP */
	dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

static void rts_dc_finish(void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* This takes care of DMABUF and user-enforced cache sync hint */
	if (buf->vb->skip_cache_sync_on_finish)
		return;

	if (!buf->non_coherent_mem || !buf->buf_io)
		return;

	/* Non-coherent MMAP only */
	if (buf->vaddr)
		invalidate_kernel_vmap_range(buf->vaddr, buf->size);

	/* For both USERPTR and non-coherent MMAP */
	dma_sync_sgtable_for_cpu(buf->dev, sgt, buf->dma_dir);
}

/*********************************************/
/*        callbacks for MMAP buffers         */
/*********************************************/

static void rts_dc_put(void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;

	if (!refcount_dec_and_test(&buf->refcount))
		return;

	rtscam_mem_free(buf->rtsmem, buf->size, buf->vaddr, buf->dma_addr);
	put_device(buf->dev);
	rts_put_mem_info(buf->rtsmem);
	kfree(buf);
}

static void *rts_dc_alloc(struct vb2_buffer *vb,
			  struct device *dev,
			  unsigned long size)
{
	struct rts_dc_buf *buf;
	struct rtscam_mem_item *item;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->rtsmem = rts_get_mem_info();
	if (!buf->rtsmem) {
		dev_err(dev, "get mem info fail, alloc fail\n");
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->vaddr = rtscam_mem_alloc(buf->rtsmem, size,
				&buf->dma_addr, 0, 0, "isp frame");
	if (!buf->vaddr) {
		dev_err(dev, "alloc memory of size %ld failed\n", size);
		rts_put_mem_info(buf->rtsmem);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	item = rtscam_mem_find(buf->rtsmem, buf->dma_addr);
	if (!item) {
		dev_err(dev, "find isp frame failed\n");
		rtscam_mem_free(buf->rtsmem, size, buf->vaddr, buf->dma_addr);
		rts_put_mem_info(buf->rtsmem);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	buf->dma_dir = vb->vb2_queue->dma_dir;
	buf->vb = vb;
	buf->buf_io = item->buf_io;
	buf->non_coherent_mem = vb->vb2_queue->non_coherent_mem;

	buf->size = size;
	/* Prevent the device from being released while the buffer is used */
	buf->dev = get_device(dev);

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = rts_dc_put;
	buf->handler.arg = buf;

	refcount_set(&buf->refcount, 1);

	return buf;
}

static int rts_dc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct rts_dc_buf *buf = buf_priv;
	int ret;

	if (!buf) {
		printk(KERN_ERR "No buffer to map\n");
		return -EINVAL;
	}

	ret = rtscam_mem_mmap(buf->rtsmem, vma, buf->vaddr,
					buf->dma_addr, buf->size);
	if (ret) {
		pr_err("Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	pr_debug("%s: mapped dma addr 0x%08lx at 0x%08lx, size %lu\n",
		 __func__, (unsigned long)buf->dma_addr, vma->vm_start,
		 buf->size);

	return 0;
}

/*********************************************/
/*       callbacks for USERPTR buffers       */
/*********************************************/

static void rts_dc_put_userptr(void *buf_priv)
{
	struct rts_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;
	int i;
	struct page **pages;

	if (sgt) {
		/*
		 * No need to sync to CPU, it's already synced to the CPU
		 * since the finish() memop will have been called before this.
		 */
		dma_unmap_sgtable(buf->dev, sgt, buf->dma_dir,
				  DMA_ATTR_SKIP_CPU_SYNC);
		pages = frame_vector_pages(buf->vec);
		/* sgt should exist only if vector contains pages... */
		BUG_ON(IS_ERR(pages));
		if (buf->dma_dir == DMA_FROM_DEVICE ||
		    buf->dma_dir == DMA_BIDIRECTIONAL)
			for (i = 0; i < frame_vector_count(buf->vec); i++)
				set_page_dirty_lock(pages[i]);
		sg_free_table(sgt);
		kfree(sgt);
	} else {
		dma_unmap_resource(buf->dev, buf->dma_addr, buf->size,
				   buf->dma_dir, 0);
	}
	vb2_destroy_framevec(buf->vec);
	kfree(buf);
}

/**
 * rts_get_vaddr_frames() - map virtual addresses to pfns
 * @start:	starting user address
 * @nr_frames:	number of pages / pfns from start to map
 * @write:      the mapped address has write permission
 * @vec:	structure which receives pages / pfns of the addresses mapped.
 *		It should have space for at least nr_frames entries.
 *
 * This function maps virtual addresses from @start and fills @vec structure
 * with page frame numbers or page pointers to corresponding pages (choice
 * depends on the type of the vma underlying the virtual address). If @start
 * belongs to a normal vma, the function grabs reference to each of the pages
 * to pin them in memory. If @start belongs to VM_IO | VM_PFNMAP vma, we don't
 * touch page structures and the caller must make sure pfns aren't reused for
 * anything else while he is using them.
 *
 * The function returns number of pages mapped which may be less than
 * @nr_frames. In particular we stop mapping if there are more vmas of
 * different type underlying the specified range of virtual addresses.
 * When the function isn't able to map a single page, it returns error.
 *
 * This function takes care of grabbing mmap_lock as necessary.
 */
static int rts_get_vaddr_frames(unsigned long start, unsigned int nr_frames,
		bool write, struct frame_vector *vec)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	int ret = 0;
	int err;
	int locked;
	unsigned int gup_flags = FOLL_LONGTERM;

	if (nr_frames == 0)
		return 0;

	if (WARN_ON_ONCE(nr_frames > vec->nr_allocated))
		nr_frames = vec->nr_allocated;

	start = untagged_addr(start);
	if (write)
		gup_flags |= FOLL_WRITE;

	mmap_read_lock(mm);
	locked = 1;
	vma = find_vma_intersection(mm, start, start + 1);
	if (!vma) {
		ret = -EFAULT;
		goto out;
	}

	/*
	 * While get_vaddr_frames() could be used for transient (kernel
	 * controlled lifetime) pinning of memory pages all current
	 * users establish long term (userspace controlled lifetime)
	 * page pinning. Treat get_vaddr_frames() like
	 * get_user_pages_longterm() and disallow it for filesystem-dax
	 * mappings.
	 */
	if (vma_is_fsdax(vma)) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!(vma->vm_flags & (VM_IO | VM_PFNMAP))) {
		vec->got_ref = true;
		vec->is_pfns = false;
		ret = pin_user_pages_fast(start, nr_frames,
			gup_flags, (struct page **)(vec->ptrs));
		goto out;
	}

	vec->got_ref = false;
	vec->is_pfns = true;
	do {
		unsigned long *nums = frame_vector_pfns(vec);

		while (ret < nr_frames && start + PAGE_SIZE <= vma->vm_end) {
			err = follow_pfn(vma, start, &nums[ret]);
			if (err) {
				if (ret == 0)
					ret = err;
				goto out;
			}
			start += PAGE_SIZE;
			ret++;
		}
		/*
		 * We stop if we have enough pages or if VMA doesn't completely
		 * cover the tail page.
		 */
		if (ret >= nr_frames || start < vma->vm_end)
			break;
		vma = find_vma_intersection(mm, start, start + 1);
	} while (vma && vma->vm_flags & (VM_IO | VM_PFNMAP));
out:
	if (locked)
		mmap_read_unlock(mm);
	if (!ret)
		ret = -EFAULT;
	if (ret > 0)
		vec->nr_frames = ret;
	return ret;
}

/**
 * rts_create_framevec() - map virtual addresses to pfns
 * @start:	Virtual user address where we start mapping
 * @length:	Length of a range to map
 * @write:      Should we map for writing into the area
 *
 * This function allocates and fills in a vector with pfns corresponding to
 * virtual address range passed in arguments. If pfns have corresponding pages,
 * page references are also grabbed to pin pages in memory. The function
 * returns pointer to the vector on success and error pointer in case of
 * failure. Returned vector needs to be freed via vb2_destroy_pfnvec().
 */
static struct frame_vector *rts_create_framevec(unsigned long start,
					 unsigned long length,
					 bool write)
{
	int ret;
	unsigned long first, last;
	unsigned long nr;
	struct frame_vector *vec;

	first = start >> PAGE_SHIFT;
	last = (start + length - 1) >> PAGE_SHIFT;
	nr = last - first + 1;
	vec = frame_vector_create(nr);
	if (!vec)
		return ERR_PTR(-ENOMEM);
	ret = rts_get_vaddr_frames(start & PAGE_MASK, nr, write, vec);
	if (ret < 0)
		goto out_destroy;
	/* We accept only complete set of PFNs */
	if (ret != nr) {
		ret = -EFAULT;
		goto out_release;
	}
	return vec;
out_release:
	put_vaddr_frames(vec);
out_destroy:
	frame_vector_destroy(vec);
	return ERR_PTR(ret);
}

static void *rts_dc_get_userptr(struct vb2_buffer *vb, struct device *dev,
				unsigned long vaddr, unsigned long size)
{
	struct rts_dc_buf *buf;
	struct frame_vector *vec;
	unsigned int offset;
	int n_pages, i;
	int ret = 0;
	struct sg_table *sgt;
	unsigned long contig_size;

	if (!size) {
		pr_debug("size is zero\n");
		return ERR_PTR(-EINVAL);
	}

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	buf->dma_dir = vb->vb2_queue->dma_dir;
	buf->vb = vb;

	offset = lower_32_bits(offset_in_page(vaddr));
	vec = rts_create_framevec(vaddr, size,
					buf->dma_dir == DMA_FROM_DEVICE ||
					buf->dma_dir == DMA_BIDIRECTIONAL);
	if (IS_ERR(vec)) {
		ret = PTR_ERR(vec);
		goto fail_buf;
	}
	buf->vec = vec;
	n_pages = frame_vector_count(vec);
	ret = frame_vector_to_pages(vec);
	if (ret < 0) {
		unsigned long *nums = frame_vector_pfns(vec);

		/*
		 * Failed to convert to pages... Check the memory is physically
		 * contiguous and use direct mapping
		 */
		for (i = 1; i < n_pages; i++)
			if (nums[i-1] + 1 != nums[i])
				goto fail_pfnvec;
		buf->dma_addr = dma_map_resource(buf->dev,
				__pfn_to_phys(nums[0]), size, buf->dma_dir, 0);
		if (dma_mapping_error(buf->dev, buf->dma_addr)) {
			ret = -ENOMEM;
			goto fail_pfnvec;
		}
		goto out;
	}

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		pr_err("failed to allocate sg table\n");
		ret = -ENOMEM;
		goto fail_pfnvec;
	}

	ret = sg_alloc_table_from_pages(sgt, frame_vector_pages(vec), n_pages,
		offset, size, GFP_KERNEL);
	if (ret) {
		pr_err("failed to initialize sg table\n");
		goto fail_sgt;
	}

	/*
	 * No need to sync to the device, this will happen later when the
	 * prepare() memop is called.
	 */
	if (dma_map_sgtable(buf->dev, sgt, buf->dma_dir,
			    DMA_ATTR_SKIP_CPU_SYNC)) {
		pr_err("failed to map scatterlist\n");
		ret = -EIO;
		goto fail_sgt_init;
	}

	contig_size = rts_dc_get_contiguous_size(sgt);
	if (contig_size < size) {
		pr_err("contiguous mapping is too small %lu/%lu\n",
			contig_size, size);
		ret = -EFAULT;
		goto fail_map_sg;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;
	buf->non_coherent_mem = 1;

out:
	buf->size = size;

	return buf;

fail_map_sg:
	dma_unmap_sgtable(buf->dev, sgt, buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);

fail_sgt_init:
	sg_free_table(sgt);

fail_sgt:
	kfree(sgt);

fail_pfnvec:
	vb2_destroy_framevec(vec);

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops rts_dma_contig_memops = {
	.alloc		= rts_dc_alloc,
	.put		= rts_dc_put,
	.cookie		= rts_dc_cookie,
	.vaddr		= rts_dc_vaddr,
	.mmap		= rts_dc_mmap,
	.get_userptr	= rts_dc_get_userptr,
	.put_userptr	= rts_dc_put_userptr,
	.prepare	= rts_dc_prepare,
	.finish		= rts_dc_finish,
	.num_users	= rts_dc_num_users,
};
EXPORT_SYMBOL_GPL(rts_dma_contig_memops);

MODULE_DESCRIPTION("DMA-contig memory handling routines for videobuf2");
MODULE_AUTHOR("Pawel Osciak <pawel@osciak.com>");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(DMA_BUF);
