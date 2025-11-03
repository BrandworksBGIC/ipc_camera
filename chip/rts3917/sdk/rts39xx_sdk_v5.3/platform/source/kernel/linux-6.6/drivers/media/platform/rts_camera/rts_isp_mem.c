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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/freezer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include "rts_camera.h"
#include "rts_isp_mem.h"

#define RTS_ISP_MEM_DRV_NAME			"rts_isp_mem"
#define RTS_ISP_MEM_DEV_NAME			"rtsmem"
#define RTS_ISP_MEM_DEFAULT_SIZE		(25 * 1024 * 1024)
#define RTS_ISP_MEM_DEFAULT_RING_HEIGHT		(256)

static unsigned int rtscam_resved_size;
module_param(rtscam_resved_size, uint, 0644);
MODULE_PARM_DESC(rtscam_resved_size, "reserved memory size");

struct rtscam_mem_dev {
	struct device *dev;

	struct rtscam_mem_info rtsmem;
	phys_addr_t resvd_mem_base;
	size_t resvd_mem_size;
	void *resvd_mem_virt;
	struct dma_chan *chan;

	struct rtscam_ge_device *rdev;

	struct list_head user_memorys;

	uint32_t mem_type;

	uint32_t vin_ring_height;
};

struct rtscam_dma_done {
	bool done;
	wait_queue_head_t *wait;
};

struct dma_coherent_mem {
	void *virt_base;
	dma_addr_t device_base;
	unsigned long pfn_base;
	int size;
	unsigned long *bitmap;
	spinlock_t spinlock;
	bool use_dev_dma_pfn_offset;
};

static struct rtscam_mem_dev *m_rmdev;

struct rtscam_mem_info *rts_get_mem_info(void)
{
	if (!m_rmdev || !m_rmdev->rtsmem.initialized)
		return NULL;

	return &m_rmdev->rtsmem;
}
EXPORT_SYMBOL_GPL(rts_get_mem_info);

void rts_put_mem_info(struct rtscam_mem_info *rtsmem)
{
}
EXPORT_SYMBOL_GPL(rts_put_mem_info);

static struct rtscam_mem_item *__get_buffer_by_index(
		struct rtscam_mem_dev *rmdev, int index)
{
	struct rtscam_mem_item *mem = NULL;
	int i = 0;

	mutex_lock(&rmdev->rtsmem.lock);
	list_for_each_entry(mem, &rmdev->user_memorys, ext_list) {
		if (i == index) {
			mutex_unlock(&rmdev->rtsmem.lock);
			return mem;
		}
		i++;
	}
	mutex_unlock(&rmdev->rtsmem.lock);

	return NULL;
}

static struct rtscam_mem_item *__find_buffer(struct rtscam_mem_dev *rmdev,
					     dma_addr_t phy_addr)
{
	struct rtscam_mem_item *mem = NULL;

	mutex_lock(&rmdev->rtsmem.lock);
	list_for_each_entry(mem, &rmdev->user_memorys, ext_list) {
		if (phy_addr >= mem->phy_addr &&
		    phy_addr < mem->phy_addr + mem->size) {
			mutex_unlock(&rmdev->rtsmem.lock);
			return mem;
		}
	}
	mutex_unlock(&rmdev->rtsmem.lock);

	return NULL;
}

static int __alloc_cma_buffer(struct rtscam_mem_dev *rmdev,
			struct rtscam_isp_dma_buf *pbuf)
{
	struct rtscam_mem_item *mem = NULL;
	void *vaddr;
	dma_addr_t phy_addr;

	vaddr = rtscam_mem_alloc2(&rmdev->rtsmem, pbuf->size,
			&phy_addr, pbuf->buf_io, pbuf->name);
	if (!vaddr) {
		rtsprintk(RTS_TRACE_ERROR,
			"rtscam_mem_alloc2 fail, len = %lu\n",
			pbuf->size);
		return -ENOMEM;
	}

	mem = rtscam_mem_find2(&rmdev->rtsmem, phy_addr);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR, "find cma mem fail\n");
		rtscam_mem_free2(&rmdev->rtsmem, phy_addr);
		return -ENOMEM;
	}

	pbuf->dma_addr = phy_addr;
	pbuf->offset = 0;
	pbuf->vaddr = 0;
	mutex_lock(&rmdev->rtsmem.lock);
	list_add_tail(&mem->ext_list, &rmdev->user_memorys);
	mutex_unlock(&rmdev->rtsmem.lock);

	return 0;
}

static int __alloc_dma_buffer(struct rtscam_mem_dev *rmdev,
			      struct rtscam_isp_dma_buf *pbuf, int flag_realloc)
{
	void *vaddr;
	dma_addr_t phy_addr;
	struct rtscam_mem_item *mem = NULL;

	if (!pbuf || !pbuf->size)
		return -EINVAL;

	if ((pbuf->direction & RTSMEM_ALLOC_CMA) &&
		(rmdev->mem_type == RTSMEM_TYPE_CMA))
		return __alloc_cma_buffer(rmdev, pbuf);

	if (flag_realloc) {
		phy_addr = pbuf->dma_addr;
		vaddr = rtscam_mem_realloc(&rmdev->rtsmem, pbuf->size,
					&phy_addr, pbuf->buf_io, pbuf->name);
		if (vaddr)
			goto next;
	}

	vaddr = rtscam_mem_alloc(&rmdev->rtsmem, pbuf->size,
			&phy_addr, pbuf->buf_io,
			pbuf->direction & 0xFF, pbuf->name);
	if (!vaddr) {
		rtsprintk(RTS_TRACE_ERROR, "rtscam_mem_alloc fail, len = %lu\n",
			  pbuf->size);
		return -ENOMEM;
	}
next:
	mem = rtscam_mem_find(&rmdev->rtsmem, phy_addr);
	if (!mem) {
		rtsprintk(RTS_TRACE_ERROR, "find mem fail\n");
		rtscam_mem_free(&rmdev->rtsmem, pbuf->size, vaddr, phy_addr);
		return -ENOMEM;
	}

	pbuf->dma_addr = phy_addr;
	pbuf->offset = mem->offset << PAGE_SHIFT;
	pbuf->vaddr = 0;
	pbuf->buf_io = mem->buf_io;
	mutex_lock(&rmdev->rtsmem.lock);
	list_add_tail(&mem->ext_list, &rmdev->user_memorys);
	mutex_unlock(&rmdev->rtsmem.lock);

	return 0;
}

static int __free_dma_buffer(struct rtscam_mem_dev *rmdev,
			     unsigned long dma_addr)
{
	struct rtscam_mem_item *mem = NULL;

	if (rmdev->mem_type == RTSMEM_TYPE_CMA) {
		mem = rtscam_mem_find2(&rmdev->rtsmem, dma_addr);
		if (mem) {
			mutex_lock(&rmdev->rtsmem.lock);
			list_del_init(&mem->ext_list);
			mutex_unlock(&rmdev->rtsmem.lock);
			rtscam_mem_free2(&rmdev->rtsmem, dma_addr);
			return 0;
		}
	}

	mem = rtscam_mem_find(&rmdev->rtsmem, dma_addr);
	mutex_lock(&rmdev->rtsmem.lock);
	if (mem)
		list_del_init(&mem->ext_list);
	mutex_unlock(&rmdev->rtsmem.lock);

	rtscam_mem_free_V2(&rmdev->rtsmem, dma_addr);

	return 0;
}

static int __set_dma_info(struct rtscam_mem_dev *rmdev,
			  struct rtscam_isp_dma_info *info)
{
	if (!rmdev || !info)
		return -EINVAL;

	return rtscam_mem_set_info(&rmdev->rtsmem, info->dma_addr, info->info);
}

static int __get_dma_info(struct rtscam_mem_dev *rmdev,
			  struct rtscam_isp_dma_info *info)
{
	struct rtscam_mem_item mem;
	int ret;

	if (!rmdev || !info)
		return -EINVAL;

	memset(&mem, 0, sizeof(mem));
	strncpy(mem.info, info->info, sizeof(mem.info));
	mem.index = info->index;

	ret = rtscam_mem_get_info(&rmdev->rtsmem, &mem);
	if (ret)
		return -EINVAL;

	info->size = mem.size;
	info->dma_addr = mem.phy_addr;

	return 0;
}

static int __get_vin_ring_height(struct rtscam_mem_dev *rmdev, uint32_t *rh)
{
	if (!rmdev || !rh)
		return -EINVAL;

	*rh = rmdev->vin_ring_height;

	return 0;
}

static int __pre_alloc_dma(struct rtscam_mem_dev *rmdev,
			  struct rtscam_isp_dma_ext_infos *pinfos)
{
	struct rtscam_isp_dma_info *ext_infos;
	struct rtscam_isp_dma_info *info;
	u32 length;
	u8 i;
	u8 ret;

	if (!rmdev || !pinfos)
		return -EINVAL;

	length = pinfos->number * sizeof(struct rtscam_isp_dma_info);
	ext_infos = kzalloc(length, GFP_KERNEL);
	if (!ext_infos)
		return -EINVAL;

	if (copy_from_user(ext_infos, (void __user *)pinfos->infos, length))
		return -EFAULT;

	mutex_lock(&rmdev->rtsmem.lock);
	for (i = 0; i < pinfos->number; i++) {
		info = ext_infos + i;

		ret = rtscam_mem_pre_alloc(&rmdev->rtsmem, info->size,
				info->info, info->buf_io, info->index);
		if (ret)
			break;
	}
	mutex_unlock(&rmdev->rtsmem.lock);
	kfree(ext_infos);
	return ret;
}

static int __get_pre_alloc_status(struct rtscam_mem_dev *rmdev,
			int *status)
{
	if (!rmdev || !status)
		return -EINVAL;

	return rtscam_mem_get_pre_alloc(&rmdev->rtsmem, status);
}

static int __set_pre_alloc_status(struct rtscam_mem_dev *rmdev,
			int *status)
{
	if (!rmdev || !status)
		return -EINVAL;

	return rtscam_mem_set_pre_alloc(&rmdev->rtsmem, *status);
}

static int __remove_cma_mem(struct rtscam_mem_dev *rmdev)
{
	int num_probe, num_use;

	if (rmdev->mem_type != RTSMEM_TYPE_CMA) {
		rtsprintk(RTS_TRACE_INFO,
			"fail to release mem, not CMA type\n");
		return -1;
	}

	num_use = rtscam_mem_number(&rmdev->rtsmem, RTSMEM_IN_USE);
	num_probe = rtscam_mem_number(&rmdev->rtsmem, RTSMEM_PROBE_ALLOCATED);
	if (num_use < 0 || num_probe < 0) {
		rtsprintk(RTS_TRACE_INFO,
			"fail to release mem\n");
		return -1;
	}
	if (num_use != num_probe) {
		rtsprintk(RTS_TRACE_INFO,
			"please release used memory\n");
		return -1;
	}

	num_use = rtscam_mem_number2(&rmdev->rtsmem, RTSMEM_IN_USE);
	num_probe = rtscam_mem_number2(&rmdev->rtsmem, RTSMEM_PROBE_ALLOCATED);
	if (num_use < 0 || num_probe < 0) {
		rtsprintk(RTS_TRACE_INFO,
			"fail to release mem\n");
		return -1;
	}
	if (num_use != num_probe) {
		rtsprintk(RTS_TRACE_INFO,
			"please release used memory\n");
		return -1;
	}

	rtscam_mem_release(&rmdev->rtsmem);
	if (rmdev->resvd_mem_virt)
		dma_free_coherent(rmdev->dev, rmdev->resvd_mem_size,
			rmdev->resvd_mem_virt, rmdev->resvd_mem_base);
	rmdev->resvd_mem_virt = NULL;

	return 0;
}

static int __sync_dma_buffer(struct rtscam_mem_dev *rmdev,
			struct rtscam_isp_dma_buf *pbuf, int flags)
{
	dma_addr_t phy_addr_base = 0;
	size_t size;
	struct rtscam_mem_item *mem = NULL;
	enum dma_data_direction dir;
	unsigned long dma_addr = pbuf->dma_addr;
	unsigned long dma_size = pbuf->size;

	if (dma_size == 0)
		return 0;

	mem = rtscam_mem_find(&rmdev->rtsmem, dma_addr);
	if (!mem)
		mem = rtscam_mem_find2(&rmdev->rtsmem, dma_addr);

	if ((dma_addr - mem->phy_addr) + dma_size > mem->size)
		dma_size = mem->size - (dma_addr - mem->phy_addr);

	if (mem && (mem->buf_io != RTSCAM_ISP_BUF_COHERENT)) {
		if (flags == 0 && mem->buf_io == RTSCAM_ISP_BUF_FROM_DEVICE)
			return 0;

		if (flags == 1 && mem->buf_io == RTSCAM_ISP_BUF_TO_DEVICE)
			return 0;

		size = dma_size;
		phy_addr_base = dma_addr;

		if (mem->buf_io == RTSCAM_ISP_BUF_FROM_DEVICE)
			dir = DMA_FROM_DEVICE;
		else if (mem->buf_io == RTSCAM_ISP_BUF_TO_DEVICE)
			dir = DMA_TO_DEVICE;
		else
			dir = DMA_BIDIRECTIONAL;

		if (flags == 0)
			dma_sync_single_range_for_device(rmdev->dev,
							 phy_addr_base,
							 0,
							 size,
							 dir);
		else
			dma_sync_single_range_for_cpu(rmdev->dev,
						      phy_addr_base,
						      0,
						      size,
						      dir);
	}

	return 0;
}

/*
static void __dma_callback(void *arg)
{
	struct rtscam_dma_done *done = arg;

	done->done = true;
	wake_up_all(done->wait);
}

static int __dma_sg_copy(struct dma_chan *chan,
			 struct scatterlist *dst_sg, unsigned int dst_nents,
			 struct scatterlist *src_sg, unsigned int src_nents)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_wait);
	struct rtscam_dma_done done = {
		.wait = &done_wait,
	};
	struct dma_device *dev;
	dma_cookie_t cookie;
	struct dma_async_tx_descriptor *tx = NULL;
	enum dma_ctrl_flags flags;
	enum dma_status status;

	dev = chan->device;
	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;

	tx = dev->device_prep_dma_sg(chan, dst_sg, dst_nents,
				     src_sg, src_nents, flags);
	if (tx == NULL) {
		pr_err("prep dma sg failed\n");
		goto exit;
	}

	tx->callback = __dma_callback;
	tx->callback_param = &done;
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie)) {
		pr_err("dma submit failed\n");
		goto exit;
	}

	dma_async_issue_pending(chan);
	wait_event_freezable_timeout(done_wait, done.done,
				     msecs_to_jiffies(10000));
	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);

	if (!done.done) {
		pr_err("dma transfer never finish\n");
		goto exit;
	}
	if (status != DMA_COMPLETE) {
		pr_err("dma transfer failed\n");
		goto exit;
	}
	return 0;

exit:
	return -EIO;
}

static int __adma_copy(struct rtscam_mem_dev *rmdev, unsigned long dst,
		       unsigned long src, unsigned long size)
{
	struct rtscam_sg_buf *sg_src;
	struct rtscam_sg_buf *sg_dst;
	int ret;

	sg_src = rtscam_get_sg_buf(&rmdev->rtsmem, src, size, 0);
	if (IS_ERR(sg_src)) {
		rtsprintk(RTS_TRACE_ERROR, "invalid src 0x%lx 0x%lx\n",
			  src, size);
		return IS_ERR(sg_src);
	}

	sg_dst = rtscam_get_sg_buf(&rmdev->rtsmem, dst, size, 1);
	if (IS_ERR(sg_dst)) {
		rtsprintk(RTS_TRACE_ERROR, "invalid dst 0x%lx 0x%lx\n",
			  dst, size);
		rtscam_put_sg_buf(sg_src);
		return IS_ERR(sg_dst);
	}

	ret = __dma_sg_copy(rmdev->chan,
			    sg_dst->sgt->sgl, sg_dst->sgt->orig_nents,
			    sg_src->sgt->sgl, sg_src->sgt->orig_nents);

	rtscam_put_sg_buf(sg_src);
	rtscam_put_sg_buf(sg_dst);

	return ret;
}

static int rtscam_adma_copy(struct rtscam_mem_dev *rmdev,
			    struct rtscam_adma_cp_stru *copy)
{
	const unsigned long SINGLE_MAX_SIZE =
			((SG_MAX_SINGLE_ALLOC - 1) * PAGE_SIZE);
	unsigned long start = 0;
	int ret;

	if (!rmdev || !copy)
		return -EINVAL;

	if (!rmdev->chan)
		return -EINVAL;

	if (!copy->dst || !copy->src || !copy->size)
		return -EINVAL;

	if (rtscam_vaddr_is_io(copy->src) && rtscam_vaddr_is_io(copy->dst))
		return __adma_copy(rmdev, copy->dst, copy->src, copy->size);

	while (start < copy->size) {
		unsigned long size = copy->size - start;

		if (size > SINGLE_MAX_SIZE)
			size = SINGLE_MAX_SIZE;

		ret = __adma_copy(rmdev,
				  copy->dst + start, copy->src + start, size);
		if (ret)
			return ret;

		start += size;
	}

	return 0;
}
*/

static int rtscam_mem_is_io(unsigned long *arg)
{
	unsigned long vaddr;
	struct vm_area_struct *vma = NULL;

	if (!arg)
		return -EINVAL;

	vaddr = *arg;

	vma = find_vma(current->mm, vaddr);
	if (!vma) {
		rtsprintk(RTS_TRACE_ERROR, "no vma for address 0x%lx\n", vaddr);
		return -EFAULT;
	}

	if (rtscam_vma_is_io(vma))
		*arg = 1;
	else
		*arg = 0;

	return 0;
}

static int rtscam_isp_mem_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_mem_dev *rmdev = rtscam_ge_get_drvdata(gdev);

	filp->private_data = rmdev;

	return 0;
}

static int rtscam_isp_mem_close(struct file *filp)
{
	filp->private_data = NULL;

	return 0;
}

static long rtscam_isp_mem_do_ioctl(struct file *filp, unsigned int cmd,
				    void *arg)
{
	struct rtscam_mem_dev *rmdev = filp->private_data;
	long ret = -EINVAL;

	switch (cmd) {
	case RTSOCIOC_ALLOC_DMA:
		ret = __alloc_dma_buffer(rmdev, arg, 0);
		break;
	case RTSOCIOC_FREE_DMA:
		ret = __free_dma_buffer(rmdev, *(unsigned long *)arg);
		break;
	case RTSOCIOC_REALLOC_DMA:
		ret = __alloc_dma_buffer(rmdev, arg, 1);
		break;
	case RTSOCIOC_SYNC_FOR_DEVICE:
		ret = __sync_dma_buffer(rmdev, arg, 0);
		break;
	case RTSOCIOC_SYNC_FOR_CPU:
		ret = __sync_dma_buffer(rmdev, arg, 1);
		break;
	case RTSOCIOC_SET_DMA_INFO:
		ret = __set_dma_info(rmdev, arg);
		break;
	case RTSOCIOC_GET_DMA_INFO:
		ret = __get_dma_info(rmdev, arg);
		break;
	case RTSOCIOC_ADMA_COPY:
	case RTSOCIOC_MEM_IS_IO:
		ret = -ENOTTY;
		break;
	case RTSOCIOC_PRE_ALLOC_MEM:
		ret = __pre_alloc_dma(rmdev, arg);
		break;
	case RTSOCIOC_SET_PRE_ALLOC_STATUS:
		ret = __set_pre_alloc_status(rmdev, arg);
		break;
	case RTSOCIOC_GET_PRE_ALLOC_STATUS:
		ret = __get_pre_alloc_status(rmdev, arg);
		break;
	case RTSOCIOC_REMOVE_CMA_MEM:
		ret = __remove_cma_mem(rmdev);
		break;
	case RTSOCIOC_GET_VIN_RING_HEIGHT:
		ret = __get_vin_ring_height(rmdev, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_WARNING,
			  "unknown[rtsmem] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static long rtscam_isp_mem_do_unlocked_ioctl(struct file *filp,
					     unsigned int cmd, void *arg)
{
	/* struct rtscam_mem_dev *rmdev = filp->private_data; */
	long ret = -EINVAL;

	switch (cmd) {
	case RTSOCIOC_ADMA_COPY:
		/* TODO: DMA memory copy */
		/* ret = rtscam_adma_copy(rmdev, arg); */
		break;
	case RTSOCIOC_MEM_IS_IO:
		ret = rtscam_mem_is_io(arg);
		break;
	default:
		rtsprintk(RTS_TRACE_WARNING,
			  "unknown[rtsmem] unlocked ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static long rtscam_isp_mem_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_isp_mem_do_ioctl);
}

static long rtscam_isp_mem_unlocked_ioctl(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg,
			       rtscam_isp_mem_do_unlocked_ioctl);
}

static void rtscam_vm_open(struct vm_area_struct *vma)
{
	struct rtscam_mem_item *mem = vma->vm_private_data;

	if (mem)
		mem->owner = current->pid;
}

static void rtscam_vm_close(struct vm_area_struct *vma)
{
	struct rtscam_mem_item *mem = vma->vm_private_data;

	if (mem)
		mem->owner = 0;
}

static const struct vm_operations_struct rtscam_vm_ops = {
	.open = rtscam_vm_open,
	.close = rtscam_vm_close,
};

static int rtscam_isp_mem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rtscam_mem_dev *rmdev = filp->private_data;
	off_t offset;
	dma_addr_t phy_addr;
	void *vaddr;
	size_t size;
	int ret;
	struct rtscam_mem_item *mem = NULL;

	size = vma->vm_end - vma->vm_start;
	if (rmdev->mem_type == RTSMEM_TYPE_CMA) {
		phy_addr = vma->vm_pgoff << PAGE_SHIFT;
		mem = rtscam_mem_find2(&rmdev->rtsmem, phy_addr);
		if (mem) {
			vma->vm_pgoff = 0;
			ret = rtscam_mem_mmap2(&rmdev->rtsmem, vma,
					phy_addr, size);
			if (ret) {
				rtsprintk(RTS_TRACE_ERROR,
				  "maping dma memory fail, error =  %d\n",
				  ret);
				return ret;
			}
			goto next;
		}
	}

	offset = (vma->vm_pgoff << PAGE_SHIFT) - rmdev->rtsmem.device_base;
	vma->vm_pgoff = 0;

	phy_addr = offset + rmdev->rtsmem.device_base;

	vaddr = offset + rmdev->rtsmem.virt_base;

	ret = rtscam_mem_mmap(&rmdev->rtsmem, vma, vaddr, phy_addr, size);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "maping dma memory fail, error =  %d\n", ret);
		return ret;
	}
next:
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

	vma->vm_private_data = __find_buffer(rmdev, phy_addr);
	vma->vm_ops = &rtscam_vm_ops;

	vma->vm_ops->open(vma);

	return 0;
}

static struct rtscam_ge_file_operations rts_isp_mem_fops = {
	.owner = THIS_MODULE,
	.open = rtscam_isp_mem_open,
	.release = rtscam_isp_mem_close,
	.ioctl = rtscam_isp_mem_ioctl,
	.unlocked_ioctl = rtscam_isp_mem_unlocked_ioctl,
	.mmap = rtscam_isp_mem_mmap,
};

static int __request_dma_chan(struct rtscam_mem_dev *rmdev)
{
	dma_cap_mask_t mask;

	if (rmdev->chan)
		return 0;

	dma_cap_zero(mask);
	rmdev->chan = dma_request_channel(mask, NULL, NULL);
	if (!rmdev->chan) {
		pr_err("no dma channel\n");
		return -ENODEV;
	}
	return 0;
}

static int __create_device(struct rtscam_mem_dev *rmdev)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (rmdev->rdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_ISP_MEM_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(rmdev->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rts_isp_mem_fops;

	rtscam_ge_set_drvdata(gdev, rmdev);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rmdev->rdev = gdev;

	return 0;
}

static void __remove_device(struct rtscam_mem_dev *rmdev)
{
	struct rtscam_ge_device *gdev;

	if (!rmdev->rdev)
		return;

	gdev = rmdev->rdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
	rmdev->rdev = NULL;
}

static ssize_t show_meminfo(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct rtscam_mem_dev *rmdev = dev_get_drvdata(dev);
	int num = 0;
	int i;
	const int line_len = 8;
	unsigned long *bitmap;

	num += scnprintf(buf + num, PAGE_SIZE - num, "total : %12ld\n",
			 rtscam_mem_get_total_size(&rmdev->rtsmem));
	num += scnprintf(buf + num, PAGE_SIZE - num, "used  : %12ld\n",
			 rtscam_mem_get_used_size(&rmdev->rtsmem));
	num += scnprintf(buf + num, PAGE_SIZE - num, "left  : %12ld\n",
			 rtscam_mem_get_left_size(&rmdev->rtsmem));

	bitmap = rtscam_mem_get_bitmap(&rmdev->rtsmem);
	if (!bitmap)
		return num;

	for (i = 0; i < BITS_TO_LONGS(rmdev->rtsmem.size); i++) {
		unsigned long bm = *(bitmap + i);

		if (num + 10 > PAGE_SIZE)
			break;

		num += scnprintf(buf + num, PAGE_SIZE - num, "%08lx", bm);
		if (i % line_len == (line_len - 1))
			num += scnprintf(buf + num, PAGE_SIZE - num, "\n");
		else
			num += scnprintf(buf + num, PAGE_SIZE - num, " ");
	}
	rtscam_mem_put_bitmap(bitmap);

	return num;
}
static DEVICE_ATTR(meminfo, 0444, show_meminfo, NULL);

static ssize_t get_memctrl(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct rtscam_mem_dev *rmdev = dev_get_drvdata(dev);
	struct rtscam_mem_item *mem = NULL;
	int num = 0;
	int i = 0;

	num = scnprintf(buf, PAGE_SIZE - num, "No.\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "   address\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "      size\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "offset\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, " count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, " pid\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "info\n");

	list_for_each_entry(mem, &rmdev->user_memorys, ext_list) {
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "%3d\t0x%08x\t%10d\t%6ld\t%6ld\t%4d\t%s\n",
				 i++,
				 mem->phy_addr, mem->size, mem->offset,
				 mem->page_cnt, mem->owner, mem->info);
	}

	return num;
}

static void __cleanup_unused_mems(struct rtscam_mem_dev *rmdev)
{
	struct rtscam_mem_item *mem = NULL;
	struct rtscam_mem_item *tmp = NULL;

	list_for_each_entry_safe(mem, tmp, &rmdev->user_memorys, ext_list) {
		if (mem->owner)
			continue;
		__free_dma_buffer(rmdev, mem->phy_addr);
	}
}

static ssize_t set_memctrl(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtscam_mem_dev *rmdev = dev_get_drvdata(dev);
	struct rtscam_mem_item *mem = NULL;
	int ctrl;

	sscanf(buf, "%d", &ctrl);

	if (!rmdev)
		return count;

	if (ctrl == -1) {
		__cleanup_unused_mems(rmdev);
		return count;
	} else if (ctrl == -2) {
		mutex_lock(&rmdev->rdev->lock);
		rtscam_mem_set_pre_alloc(&rmdev->rtsmem, 0);
		mutex_unlock(&rmdev->rdev->lock);
		return count;
	} else if (ctrl == -3) {
		mutex_lock(&rmdev->rdev->lock);
		__remove_cma_mem(rmdev);
		mutex_unlock(&rmdev->rdev->lock);
		return count;
	} else if (ctrl < -3) {
		return count;
	}

	mem = __get_buffer_by_index(rmdev, ctrl);
	if (!mem)
		return count;

	if (mem->owner) {
		rtsprintk(RTS_TRACE_INFO, "0x%08x\t%10d\t%4d is inuse\n",
			  mem->phy_addr, mem->size, mem->owner);
	} else {
		__free_dma_buffer(rmdev, mem->phy_addr);
		mem = NULL;
	}

	return count;
}
static DEVICE_ATTR(memctrl, 0664, get_memctrl, set_memctrl);

static ssize_t show_memlist(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct rtscam_mem_dev *rmdev = dev_get_drvdata(dev);
	int num = 0;
	int i = 0;

	num = scnprintf(buf, PAGE_SIZE - num, "No.\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "   address\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "      size\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "offset\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, " count\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, " pid\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "flag\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "io\t");
	num += scnprintf(buf + num, PAGE_SIZE - num, "info\n");

	while (1) {
		struct rtscam_mem_item *mem = NULL;

		mem = rtscam_mem_enum(&rmdev->rtsmem, i);
		if (!mem)
			break;

		num += scnprintf(buf + num, PAGE_SIZE - num,
			"%3d\t0x%08x\t%10d\t%6ld\t%6ld\t%4d\t0x%x\t0x%x\t%s\n",
			i++,
			mem->phy_addr, mem->size,
			mem->offset, mem->page_cnt, mem->creator,
			mem->buf_flag, mem->buf_io, mem->info);
	}
	return num;
}
static DEVICE_ATTR(memlist, 0444, show_memlist, NULL);

static int rts_isp_mem_probe(struct platform_device *pdev)
{
	struct rtscam_mem_dev *rmdev = NULL;
	struct device_node *np = NULL;
	u32 size, vin_rh;
	struct dma_coherent_mem dmem;
	int ret = 0;
	uint32_t memtype = 0;
	struct reserved_mem *rmem;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np) {
		rtsprintk(RTS_TRACE_ERROR, "No memory-region specified\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = of_property_read_u32_index(np, "size", 0, &size);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			"No memory size assigned to the region");
		goto exit;
	}

	memset(&dmem, 0, sizeof(dmem));
	if (of_device_is_compatible(np, "shared-dma-pool")) {
		ret = of_reserved_mem_device_init(&pdev->dev);
		if (ret) {
			rtsprintk(RTS_TRACE_ERROR,
			"init of reserved memory fail, ret = %d\n", ret);
			goto exit;
		}

		if (pdev->dev.dma_mem) {
			/* DMA */
			memcpy(&dmem, pdev->dev.dma_mem, sizeof(dmem));
			dmem.size = size;
			memtype = RTSMEM_TYPE_DMA;
		} else {
			/* CMA */
			if (rtscam_resved_size > 0) {
				dmem.size = rtscam_resved_size;
			} else {
				ret = of_property_read_u32_index(
					pdev->dev.of_node, "reserve-size",
					0, &dmem.size);
				if (ret) {
					dmem.size = RTS_ISP_MEM_DEFAULT_SIZE;
					rtsprintk(RTS_TRACE_INFO,
					"No reserve-size, use %u as default\n",
					dmem.size);
				}
			}

			if (dmem.size > size) {
				rtsprintk(RTS_TRACE_ERROR,
				"Error reserve-size (%u), cma size (%u)",
				dmem.size, size);
				ret = -EINVAL;
				goto exit;
			}

			dmem.virt_base = dma_alloc_coherent(&pdev->dev,
				dmem.size, &dmem.device_base, GFP_KERNEL);
			memtype = RTSMEM_TYPE_CMA;
		}

	} else {
		rmem = of_reserved_mem_lookup(np);
		if (!rmem) {
			rtsprintk(RTS_TRACE_ERROR, "No reserved memory\n");
			ret = -EINVAL;
			goto exit;
		}

		dmem.device_base = rmem->base;
		dmem.size = rmem->size;
		dmem.virt_base = memremap(rmem->base, rmem->size, MEMREMAP_WB);
		memtype = RTSMEM_TYPE_DMA;
	}
	of_node_put(np);
	np = NULL;

	if (!dmem.virt_base) {
		rtsprintk(RTS_TRACE_ERROR, "fail to alloc\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node,
				"vin-ring-height", 0, &vin_rh);
	if (ret) {
		vin_rh = RTS_ISP_MEM_DEFAULT_RING_HEIGHT;
		rtsprintk(RTS_TRACE_INFO,
			"No vin-ring-height, use %u as default\n",
			vin_rh);
	}

	rmdev = devm_kzalloc(&pdev->dev, sizeof(*rmdev), GFP_KERNEL);
	if (NULL == rmdev) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate isp mem device object\n");
		ret = -ENOMEM;
		goto exit;
	}

	rmdev->dev = get_device(&pdev->dev);
	rmdev->resvd_mem_base = dmem.device_base;
	rmdev->resvd_mem_size = dmem.size;
	rmdev->resvd_mem_virt = dmem.virt_base;
	rmdev->mem_type = memtype;
	rmdev->vin_ring_height = vin_rh;

	rtsprintk(RTS_TRACE_DEBUG,
		  "isp resvd mem addr : 0x%08x, size : 0x%x, virt: 0x%x\n",
		  rmdev->resvd_mem_base, rmdev->resvd_mem_size,
		  (unsigned int)rmdev->resvd_mem_virt);
	ret = rtscam_mem_init(&rmdev->rtsmem, rmdev->dev,
			      rmdev->resvd_mem_virt,
			      rmdev->resvd_mem_base, rmdev->resvd_mem_size);

	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "rtscam_mem_init fail : %d\n", ret);
		if (memtype == RTSMEM_TYPE_CMA)
			rmdev->resvd_mem_virt = NULL;
		goto exit;
	}

	INIT_LIST_HEAD(&rmdev->user_memorys);

	rmdev->chan = NULL;
	if (of_find_property(pdev->dev.of_node, "use-adma", NULL)) {
		ret = __request_dma_chan(rmdev);
		if (ret)
			rtsprintk(RTS_TRACE_ERROR,
				  "request_dma_chan fail : %d\n", ret);
	}

	__create_device(rmdev);

	platform_set_drvdata(pdev, rmdev);
	device_create_file(rmdev->dev, &dev_attr_meminfo);
	device_create_file(rmdev->dev, &dev_attr_memctrl);
	device_create_file(rmdev->dev, &dev_attr_memlist);

	m_rmdev = rmdev;

	return 0;
exit:
	if (memtype == RTSMEM_TYPE_CMA)
		dma_free_coherent(&pdev->dev, dmem.size,
				dmem.virt_base, dmem.device_base);

	if (rmdev) {
		put_device(rmdev->dev);
		rmdev->dev = NULL;
	}

	of_reserved_mem_device_release(&pdev->dev);
	if (np)
		of_node_put(np);
	np = NULL;

	return ret;
}

static int rts_isp_mem_remove(struct platform_device *pdev)
{
	struct rtscam_mem_dev *rmdev = platform_get_drvdata(pdev);

	m_rmdev = NULL;

	device_remove_file(rmdev->dev, &dev_attr_meminfo);
	device_remove_file(rmdev->dev, &dev_attr_memctrl);
	device_remove_file(rmdev->dev, &dev_attr_memlist);
	__remove_device(rmdev);

	if (rmdev->chan) {
		dma_release_channel(rmdev->chan);
		rmdev->chan = NULL;
	}
	rtscam_mem_release(&rmdev->rtsmem);
	if (rmdev->mem_type == RTSMEM_TYPE_CMA) {
		dma_free_coherent(&pdev->dev, rmdev->resvd_mem_size,
			rmdev->resvd_mem_virt, rmdev->resvd_mem_base);
		rmdev->resvd_mem_virt = NULL;
	}
	of_reserved_mem_device_release(&pdev->dev);
	put_device(rmdev->dev);
	rmdev->dev = NULL;

	return 0;
}

static const struct of_device_id rts_isp_mem_dt_ids[] = {
	{ .compatible = "realtek,rts3917-ispmem", },
	{ /* sentinel */ },
};

static struct platform_driver rts_isp_mem_driver = {
	.driver = {
		.name = RTS_ISP_MEM_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rts_isp_mem_dt_ids),
	},
	.probe = rts_isp_mem_probe,
	.remove = rts_isp_mem_remove,
};

module_platform_driver(rts_isp_mem_driver);

MODULE_DESCRIPTION("Realsil ISP Memory device driver");
MODULE_AUTHOR("Ming Qian <ming_qian@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");
MODULE_ALIAS("platform:" RTS_ISP_MEM_DRV_NAME);
