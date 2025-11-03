/*
 * videobuf2-memops.c - generic memory handling routines for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *	   Marek Szyprowski <m.szyprowski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

/**
 * __rts_get_vaddr_frames() - map virtual addresses to pfns
 * @start:	starting user address
 * @nr_frames:	number of pages / pfns from start to map
 * @gup_flags:	flags modifying lookup behaviour
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
static int __rts_get_vaddr_frames(unsigned long start, unsigned int nr_frames,
		     bool write, struct frame_vector *vec)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	int ret = 0;
	int err;
	int locked;
	unsigned int gup_flags = FOLL_FORCE;

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
		ret = pin_user_pages_fast(start, nr_frames, gup_flags,
				  (struct page **)(vec->ptrs));
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
 * vb2_create_framevec() - map virtual addresses to pfns
 * @start:	Virtual user address where we start mapping
 * @length:	Length of a range to map
 * @write:	Should we map for writing into the area
 *
 * This function allocates and fills in a vector with pfns corresponding to
 * virtual address range passed in arguments. If pfns have corresponding pages,
 * page references are also grabbed to pin pages in memory. The function
 * returns pointer to the vector on success and error pointer in case of
 * failure. Returned vector needs to be freed via vb2_destroy_pfnvec().
 */
struct frame_vector *vb2_create_framevec(unsigned long start,
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
	ret = __rts_get_vaddr_frames(start & PAGE_MASK, nr, write, vec);
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
EXPORT_SYMBOL(vb2_create_framevec);

/**
 * vb2_destroy_framevec() - release vector of mapped pfns
 * @vec:	vector of pfns / pages to release
 *
 * This releases references to all pages in the vector @vec (if corresponding
 * pfns are backed by pages) and frees the passed vector.
 */
void vb2_destroy_framevec(struct frame_vector *vec)
{
	put_vaddr_frames(vec);
	frame_vector_destroy(vec);
}
EXPORT_SYMBOL(vb2_destroy_framevec);

/**
 * vb2_common_vm_open() - increase refcount of the vma
 * @vma:	virtual memory region for the mapping
 *
 * This function adds another user to the provided vma. It expects
 * struct vb2_vmarea_handler pointer in vma->vm_private_data.
 */
static void vb2_common_vm_open(struct vm_area_struct *vma)
{
	struct vb2_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
	       __func__, h, refcount_read(h->refcount), vma->vm_start,
	       vma->vm_end);

	refcount_inc(h->refcount);
}

/**
 * vb2_common_vm_close() - decrease refcount of the vma
 * @vma:	virtual memory region for the mapping
 *
 * This function releases the user from the provided vma. It expects
 * struct vb2_vmarea_handler pointer in vma->vm_private_data.
 */
static void vb2_common_vm_close(struct vm_area_struct *vma)
{
	struct vb2_vmarea_handler *h = vma->vm_private_data;

	pr_debug("%s: %p, refcount: %d, vma: %08lx-%08lx\n",
	       __func__, h, refcount_read(h->refcount), vma->vm_start,
	       vma->vm_end);

	h->put(h->arg);
}

/*
 * vb2_common_vm_ops - common vm_ops used for tracking refcount of mmapped
 * video buffers
 */
const struct vm_operations_struct vb2_common_vm_ops = {
	.open = vb2_common_vm_open,
	.close = vb2_common_vm_close,
};
EXPORT_SYMBOL_GPL(vb2_common_vm_ops);

MODULE_DESCRIPTION("common memory handling routines for videobuf2");
MODULE_AUTHOR("Pawel Osciak <pawel@osciak.com>");
MODULE_LICENSE("GPL");
