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

#define TAG	"RTSTREAM"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include "linux/rtstream.h"
#include "rts_camera.h"

#define rtstream_info(...)		rtsprintk(RTS_TRACE_INFO, __VA_ARGS__)
#define rtstream_err(...)		rtsprintk(RTS_TRACE_ERROR, __VA_ARGS__)

#define RTS_RTSTREAM_DEV_NAME		"rtstream"

struct rtstream_cfg_item {
	struct list_head list;
	struct rtstream_cfg_info info;
};

struct rtstream_chn_info_t {
	struct list_head list;
	uint32_t chn_id;
	uint32_t chn_no;
	uint32_t chn_pid;
	uint8_t status;
};

struct rtstream_chn_pipe_t {
	struct list_head list;
	uint32_t src_id;
	uint32_t src_no;
	uint32_t dst_id;
	uint32_t dst_no;
	uint32_t pipe_pid;
	uint8_t status;
};

struct rtstream_unit_info_t {
	struct list_head list;
	uint8_t index;
	uint32_t chnno;
	uint32_t pid;
	uint8_t status;
};

struct rtstream_kobject {
	struct kobject *debug_obj;

	struct kobject *h26x_obj;
	struct kobject *aec_obj;
};

struct rtstream_mod_info {
	int enable;
	int pid;
	int sig;
	union {
		struct rtstream_h26x_op h26x_op;
		struct rtstream_aec_op aec_op;
	};
};

struct rtstream_manager {
	void *vmem;
	unsigned long size;
	uint8_t bitmap[RTSTREAM_BITMAP_NUM];
	struct rtscam_ge_device *rdev;
	struct mutex lock;
	struct list_head cfg_list;

	struct list_head chn_head;
	struct list_head pipe_head;
	struct list_head unit_head;
	spinlock_t slock;

	bool g_cfg_shlr;

	struct rtstream_kobject rts_obj;

	/* module debug */
	struct rtstream_mod_info h26x_info;
	struct rtstream_mod_info aec_info;

	/* signal debug */
	int stream_num;
	wait_queue_head_t debug_wq;
	int debug_signal;
	struct rtstream_signal_info s_info;
};

static struct rtstream_manager manager;
static DEFINE_SEMAPHORE(rtstream_sem, 1);

static int __rtstream_lock(void)
{
	return mutex_lock_interruptible(&manager.lock);
}

static void __rtstream_unlock(void)
{
	mutex_unlock(&manager.lock);
}

static int rtstream_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtstream_manager *rtstreamer = rtscam_ge_get_drvdata(gdev);

	filp->private_data = rtstreamer;

	return 0;
}

static int rtstream_close(struct file *filp)
{
	struct rtstream_manager *rtstreamer = filp->private_data;

	filp->private_data = NULL;

	if (!rtstreamer)
		return -EINVAL;

	return 0;
}

static ssize_t rtstream_read(struct file *filp, char __user *buf, size_t count,
			     loff_t *f_pos)
{
	struct rtstream_manager *rtstreamer = filp->private_data;
	loff_t pos;
	int ret = 0;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	pos = *f_pos;
	if (pos >= rtstreamer->size)
		goto exit;
	if (pos + count > rtstreamer->size)
		count = rtstreamer->size - pos;

	if (copy_to_user(buf, rtstreamer->vmem + pos, count)) {
		ret = -EFAULT;
		goto exit;
	}
	*f_pos = pos + count;
	ret = count;

exit:
	__rtstream_unlock();
	return ret;
}

static int __alloc_vmem(struct rtstream_manager *rtstreamer,
			unsigned long size)
{
	size += sizeof(struct rtstream_sys_t);

	if (rtstreamer->vmem) {
		if (size > rtstreamer->size)
			return -EINVAL;
		return 0;
	}

	rtstreamer->vmem = vmalloc_user(size);
	if (!rtstreamer->vmem) {
		rtstream_err("vmalloc of size %ld failed\n",
			     manager.size);
		return -ENOMEM;
	}
	rtstreamer->size = size;

	return 0;
}

static int __add_mem_info(struct rtstream_manager *rtstreamer,
			int index, int chnno, int pid)
{
	struct rtstream_unit_info_t *info;
	struct rtstream_unit_info_t *item, *temp;
	int flag = 0;

	list_for_each_entry_safe(item, temp, &rtstreamer->unit_head, list) {
		if (index == item->index)
			return -ENOMEM;

		if (chnno == item->chnno && pid == item->pid)
			return -ENOMEM;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->index = index;
	info->chnno = chnno;
	info->pid = pid;
	info->status = 1;

	list_for_each_entry_safe(item, temp, &rtstreamer->unit_head, list) {
		if (info->pid < item->pid) {
			list_add_tail(&info->list, &item->list);
			flag = 1;
			break;
		}

		if (info->pid == item->pid) {
			if (info->chnno < item->chnno) {
				list_add_tail(&info->list, &item->list);
				flag = 1;
				break;
			}
		}
	}

	if (!flag)
		list_add_tail(&info->list, &rtstreamer->unit_head);

	return 0;
}

static int __del_mem_info(struct rtstream_manager *rtstreamer,
			int index, int chnno, int pid)
{
	struct rtstream_unit_info_t *item, *temp;

	list_for_each_entry_safe(item, temp, &rtstreamer->unit_head, list) {
		if (item->index == index) {
			if (item->chnno != chnno || item->pid != pid)
				return -EINVAL;
			list_del(&item->list);
			kfree(item);
		}
	}
	return 0;
}

static void __release_mem_info(struct rtstream_manager *rtstreamer)
{
	struct rtstream_unit_info_t *item, *temp;

	mutex_lock(&manager.lock);

	list_for_each_entry_safe(item, temp, &rtstreamer->unit_head, list) {
		list_del(&item->list);
		kfree(item);
	}

	mutex_unlock(&manager.lock);
}

static int __get_mem_info(struct rtstream_manager *rtstreamer,
			struct rtstream_mem_info *p)
{
	int len;
	int a, b, i, ret;

	if (!p)
		return -EINVAL;

	len = sizeof(rtstreamer->bitmap) * 8;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	for (i = 0; i < len; i++) {
		a = i / 8;
		b = i % 8;
		if (!(rtstreamer->bitmap[a] & (1 << b))) {
			rtstreamer->bitmap[a] |= (1 << b);
			break;
		}
	}

	if (i == len) {
		__rtstream_unlock();
		return -EINVAL;
	}

	ret = __add_mem_info(rtstreamer, i, p->chnno, p->pid);
	if (ret) {
		__rtstream_unlock();
		return ret;
	}

	__rtstream_unlock();
	p->index = i;
	return 0;
}

static int __put_mem_info(struct rtstream_manager *rtstreamer,
			struct rtstream_mem_info *p)
{
	int len;
	int idx;
	int a, b, ret;

	if (!p)
		return -EINVAL;

	idx = p->index;
	len = sizeof(rtstreamer->bitmap) * 8;

	if (idx >= len)
		return -EINVAL;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	a = idx / 8;
	b = idx % 8;
	if (!(rtstreamer->bitmap[a] & (1 << b))) {
		__rtstream_unlock();
		return 0;
	}

	rtstreamer->bitmap[a] &= (~(1 << b));

	ret = __del_mem_info(rtstreamer, idx, p->chnno, p->pid);
	if (ret) {
		__rtstream_unlock();
		return ret;
	}

	__rtstream_unlock();
	return 0;
}

static struct rtstream_cfg_item *__get_cfg_item(
		struct rtstream_manager *rtstreamer, char *name)
{
	struct rtstream_cfg_item *item;
	struct rtstream_cfg_item *next;

	if (!rtstreamer || !name)
		return NULL;

	list_for_each_entry_safe(item, next, &rtstreamer->cfg_list, list) {
		if (!strcmp(item->info.name, name))
			return item;
	}

	return NULL;
}

static int __set_cfg_info(struct rtstream_manager *rtstreamer,
				void *args)
{
	struct rtstream_cfg_item *item;
	struct rtstream_cfg_info *info = (struct rtstream_cfg_info *)args;
	int ret = 0;

	if (!info || !info->cfg_size)
		return -EINVAL;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	item = __get_cfg_item(rtstreamer, info->name);
	if (item) {
		ret = -EINVAL;
		goto err;
	}

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		ret = -ENOMEM;
		goto err;
	}

	item->info.cfg = kzalloc(info->cfg_size, GFP_KERNEL);
	if (!item->info.cfg) {
		kfree(item);
		ret = -ENOMEM;
		goto err;
	}
	ret = copy_from_user(item->info.cfg, (void __user *)info->cfg,
				info->cfg_size);
	if (ret != 0) {
		kfree(item->info.cfg);
		kfree(item);
		ret = -EFAULT;
		goto err;
	}
	INIT_LIST_HEAD(&item->list);
	strlcpy(item->info.name, info->name, sizeof(item->info.name));
	item->info.cfg_size = info->cfg_size;
	list_add_tail(&item->list, &rtstreamer->cfg_list);
err:
	__rtstream_unlock();

	return ret;
}

static int __get_cfg_info(struct rtstream_manager *rtstreamer,
				void *args)
{
	struct rtstream_cfg_info *info = (struct rtstream_cfg_info *)args;
	struct rtstream_cfg_item *item;
	int ret = 0;

	if (!info)
		return -EINVAL;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	item = __get_cfg_item(rtstreamer, info->name);
	if (!item) {
		ret = -EINVAL;
		goto err;
	}

	if (info->cfg_size != item->info.cfg_size) {
		ret = -EINVAL;
		goto err;
	}

	ret = copy_to_user((void __user *)info->cfg, item->info.cfg,
		info->cfg_size);
	if (ret != 0)
		ret = -EFAULT;
err:
	__rtstream_unlock();

	return ret;
}

static void __clear_cfg_info(struct rtstream_manager *rtstreamer,
				void *args)
{
	struct rtstream_cfg_info *info = (struct rtstream_cfg_info *)args;
	struct rtstream_cfg_item *item;

	if (!info || info->name[0] == 0)
		return;

	mutex_lock(&manager.lock);
	item = __get_cfg_item(rtstreamer, info->name);
	if (!item)
		goto err;

	list_del(&item->list);
	kfree(item->info.cfg);
	kfree(item);
err:
	mutex_unlock(&manager.lock);
}

static void __release_cfg_info(struct rtstream_manager *rtstreamer)
{
	struct rtstream_cfg_item *item;
	struct rtstream_cfg_item *next;

	mutex_lock(&manager.lock);

	list_for_each_entry_safe(item, next, &rtstreamer->cfg_list, list) {
		list_del(&item->list);
		kfree(item->info.cfg);
		kfree(item);
	}

	mutex_unlock(&manager.lock);
}

static int __add_sys_chn(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_info *info)
{
	struct rtstream_chn_info_t *info_t, *temp, *info_ptr;
	int flag;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	list_for_each_entry_safe(info_ptr, temp, &rtstreamer->chn_head, list) {
		if (info_ptr->chn_id == info->chn_id &&
			info_ptr->chn_no == info->chn_no &&
			info_ptr->chn_pid == info->chn_pid) {
			rtstream_info("chn exists\n");
			__rtstream_unlock();
			return 0;
		}
	}

	info_t = kzalloc(sizeof(*info_t), GFP_KERNEL);
	if (!info_t) {
		__rtstream_unlock();
		return -ENOMEM;
	}

	info_t->chn_id = info->chn_id;
	info_t->chn_no = info->chn_no;
	info_t->chn_pid = info->chn_pid;
	info_t->status = 1;
	flag = 0;
	list_for_each_entry_safe(info_ptr, temp, &rtstreamer->chn_head, list) {
		if (info_ptr->chn_pid > info_t->chn_pid) {
			list_add_tail(&info_t->list, &info_ptr->list);
			flag = 1;
			break;
		}

		if (info_ptr->chn_pid == info_t->chn_pid) {
			if (info_ptr->chn_no > info_t->chn_no) {
				list_add_tail(&info_t->list, &info_ptr->list);
				flag = 1;
				break;
			}
		}
	}

	if (flag == 0)
		list_add_tail(&info_t->list, &rtstreamer->chn_head);

	__rtstream_unlock();

	return 0;
}

static int __del_sys_chn(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_info *info)
{
	struct rtstream_chn_info_t *info_t, *temp;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	list_for_each_entry_safe(info_t, temp, &rtstreamer->chn_head, list) {
		if (info_t->chn_id == info->chn_id &&
			info_t->chn_no == info->chn_no &&
			info_t->chn_pid == info->chn_pid) {
			list_del(&info_t->list);
			kfree(info_t);
			break;
		}
	}
	__rtstream_unlock();

	return 0;
}

static int __set_sys_chn_info(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_info *info)
{
	int ret = 0;

	if (info->chn_op == RTSTREAM_CHN_OP_CREATE)
		ret = __add_sys_chn(rtstreamer, info);
	else if (info->chn_op == RTSTREAM_CHN_OP_RELEASE)
		ret = __del_sys_chn(rtstreamer, info);

	return ret;
}

static int __add_sys_chn_pipe(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_pipe *p)
{
	struct rtstream_chn_pipe_t *pipe_t, *temp, *pipe_ptr;
	int flag;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	list_for_each_entry_safe(pipe_ptr, temp,
			&rtstreamer->pipe_head, list) {
		if (pipe_ptr->src_id == p->src_id &&
			pipe_ptr->src_no == p->src_no &&
			pipe_ptr->dst_id == p->dst_id &&
			pipe_ptr->dst_no == p->dst_no &&
			pipe_ptr->pipe_pid == p->pipe_pid) {
			rtstream_info("pipe exists\n");
			__rtstream_unlock();
			return 0;
		}
	}

	pipe_t = kzalloc(sizeof(*pipe_t), GFP_KERNEL);
	if (!pipe_t) {
		__rtstream_unlock();
		return -ENOMEM;
	}

	pipe_t->src_id = p->src_id;
	pipe_t->src_no = p->src_no;
	pipe_t->dst_id = p->dst_id;
	pipe_t->dst_no = p->dst_no;
	pipe_t->pipe_pid = p->pipe_pid;
	pipe_t->status = 1;
	flag = 0;
	list_for_each_entry_safe(pipe_ptr, temp,
			&rtstreamer->pipe_head, list) {
		if (pipe_ptr->pipe_pid > pipe_t->pipe_pid) {
			list_add_tail(&pipe_t->list, &pipe_ptr->list);
			flag = 1;
			break;
		}

		if (pipe_ptr->pipe_pid == pipe_t->pipe_pid) {
			if (pipe_ptr->src_no > pipe_t->src_no) {
				list_add_tail(&pipe_t->list, &pipe_ptr->list);
				flag = 1;
				break;
			}
		}
	}

	if (flag == 0)
		list_add_tail(&pipe_t->list, &rtstreamer->pipe_head);

	__rtstream_unlock();

	return 0;
}

static int __del_sys_chn_pipe(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_pipe *p)
{
	struct rtstream_chn_pipe_t *pipe_t, *temp;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	list_for_each_entry_safe(pipe_t, temp, &rtstreamer->pipe_head, list) {
		if (pipe_t->src_id == p->src_id &&
			pipe_t->src_no == p->src_no &&
			pipe_t->dst_id == p->dst_id &&
			pipe_t->dst_no == p->dst_no &&
			pipe_t->pipe_pid == p->pipe_pid) {
			list_del(&pipe_t->list);
			kfree(pipe_t);
			break;
		}
	}
	__rtstream_unlock();

	return 0;
}

static int __set_sys_chn_pipe(struct rtstream_manager *rtstreamer,
			struct rtstream_chn_pipe *p)
{
	int ret = 0;

	if (p->pipe_op == RTSTREAM_PIPE_OP_BIND)
		ret = __add_sys_chn_pipe(rtstreamer, p);
	else if (p->pipe_op == RTSTREAM_PIPE_OP_UNBIND)
		ret = __del_sys_chn_pipe(rtstreamer, p);

	return ret;
}

static int __set_sys_stream_info(struct rtstream_manager *rtstreamer,
			struct rtstream_stream_info *p)
{
	if (p->stream_op == RTSTREAM_STREAM_OP_CREATE)
		rtstreamer->stream_num++;
	else if (p->stream_op == RTSTREAM_STREAM_OP_RELEASE)
		rtstreamer->stream_num--;

	return 0;
}

static int __get_sys_signal_info(struct rtstream_manager *rtstreamer,
			struct rtstream_signal_info *p)
{
	if (!p)
		return -EINVAL;

	p->chnno = rtstreamer->s_info.chnno;
	p->pid = rtstreamer->s_info.pid;

	return 0;
}

static int __process_sys_h26x_op(struct rtstream_manager *rtstreamer,
			struct rtstream_h26x_op *op)
{
	int rw;

	rw = op->rw;

	if (rw == 0)
		memcpy(op, &rtstreamer->h26x_info.h26x_op, sizeof(*op));
	else if (rw == 1)
		memcpy(&rtstreamer->h26x_info.h26x_op, op, sizeof(*op));
	else
		return 0;

	op->rw = rw;

	return 0;
}

static int __process_sys_aec_op(struct rtstream_manager *rtstreamer,
			struct rtstream_aec_op *op)
{
	int rw;

	rw = op->rw;

	if (rw == 0)
		memcpy(op, &rtstreamer->aec_info.aec_op, sizeof(*op));
	else if (rw == 1)
		memcpy(&rtstreamer->aec_info.aec_op, op, sizeof(*op));
	else
		return 0;

	op->rw = rw;

	return 0;
}

static long rtstream_do_ioctl(struct file *filp, unsigned int cmd,
			      void *arg)
{
	struct rtstream_manager *rtstreamer = filp->private_data;
	int ret = -ENOTTY;

	if (!rtstreamer)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != RTS_RTSTREAM_IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case RTSTREAM_IOC_G_MEM:
		ret = __get_mem_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_P_MEM:
		ret = __put_mem_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_G_CFG:
		ret = __get_cfg_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_S_CFG:
		__clear_cfg_info(rtstreamer, arg);
		ret = __set_cfg_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_C_CFG:
		__clear_cfg_info(rtstreamer, arg);
		ret = 0;
		break;
	case RTSTREAM_IOC_LOCK:
	case RTSTREAM_IOC_UNLOCK:
		ret = -ENOTTY;
		break;
	case RTSTREAM_IOC_STREAM_INFO:
		ret = __set_sys_stream_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_CHN_INFO:
		ret = __set_sys_chn_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_CHN_PIPE:
		ret = __set_sys_chn_pipe(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_GCFG_SHLR:
		*(bool *)arg = rtstreamer->g_cfg_shlr;
		ret = 0;
		break;
	case RTSTREAM_IOC_SIGNAL_INFO:
		ret = __get_sys_signal_info(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_H26X_SIG:
		*(int *)arg = rtstreamer->h26x_info.sig;
		ret = 0;
		break;
	case RTSTREAM_IOC_H26X_OP:
		ret = __process_sys_h26x_op(rtstreamer, arg);
		break;
	case RTSTREAM_IOC_AEC_SIG:
		*(int *)arg = rtstreamer->aec_info.sig;
		ret = 0;
		break;
	case RTSTREAM_IOC_AEC_OP:
		ret = __process_sys_aec_op(rtstreamer, arg);
		break;
	default:
		rtstream_err("unknown ioctl 0x%08x, '%c' 0x%x\n",
			     cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static long rtstream_do_unlocked_ioctl(struct file *filp, unsigned int cmd,
			void *arg)
{
	struct rtstream_manager *rtstreamer = filp->private_data;
	int ret = -ENOTTY;

	if (!rtstreamer)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != RTS_RTSTREAM_IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case RTSTREAM_IOC_LOCK:
		ret = down_interruptible(&rtstream_sem);
		break;
	case RTSTREAM_IOC_UNLOCK:
		up(&rtstream_sem);
		ret = 0;
		break;
	default:
		rtstream_err("unknown ioctl 0x%08x, '%c' 0x%x\n",
			     cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static long rtstream_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtstream_do_ioctl);
}

static long rtstream_unlocked_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtstream_do_unlocked_ioctl);
}

static void rtstream_vm_open(struct vm_area_struct *vma)
{
	pid_t *pt;

	pt = kzalloc(sizeof(*pt), GFP_KERNEL);
	if (!pt)
		return;

	*pt = current->pid;

	vma->vm_private_data = pt;
}

static void rtstream_vm_close(struct vm_area_struct *vma)
{
	pid_t *pt = vma->vm_private_data;
	struct rtstream_unit_info_t *uinfo, *utemp;
	struct rtstream_chn_info_t *cinfo, *ctemp;
	struct rtstream_chn_pipe_t *pinfo, *ptemp;

	if (!pt)
		return;

	mutex_lock(&manager.lock);

	list_for_each_entry_safe(uinfo, utemp, &manager.unit_head, list) {
		if (uinfo->pid == *pt)
			uinfo->status = 0;
	}

	list_for_each_entry_safe(cinfo, ctemp, &manager.chn_head, list) {
		if (cinfo->chn_pid == *pt)
			cinfo->status = 0;
	}

	list_for_each_entry_safe(pinfo, ptemp, &manager.pipe_head, list) {
		if (pinfo->pipe_pid == *pt)
			pinfo->status = 0;
	}
	kfree(pt);

	mutex_unlock(&manager.lock);
}

static const struct vm_operations_struct rtstream_vm_ops = {
	.open = rtstream_vm_open,
	.close = rtstream_vm_close
};

static int rtstream_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rtstream_manager *rtstreamer = filp->private_data;
	int ret = 0;

	if (!rtstreamer || !rtstreamer->vmem)
		return -EINVAL;

	ret = remap_vmalloc_range(vma, rtstreamer->vmem, 0);
	if (ret) {
		rtstream_err("Remapping vmalloc memory, error: %d\n", ret);
		return ret;
	}

	vm_flags_set(vma, VM_DONTEXPAND);
	vma->vm_ops = &rtstream_vm_ops;

	vma->vm_ops->open(vma);

	return ret;
}

static unsigned int rtstream_poll(struct file *filp,
				struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	unsigned long req_events = poll_requested_events(wait);

	if (!(req_events & (POLLIN | POLLRDNORM)))
		return mask;

	if (!manager.debug_signal)
		poll_wait(filp, &manager.debug_wq, wait);

	if (manager.debug_signal) {
		mask = POLLIN | POLLRDNORM;
		manager.debug_signal--;
	}

	return mask;
}

static struct rtscam_ge_file_operations rtstream_fops = {
	.owner = THIS_MODULE,
	.open = rtstream_open,
	.release = rtstream_close,
	.ioctl = rtstream_ioctl,
	.unlocked_ioctl = rtstream_unlocked_ioctl,
	.mmap = rtstream_mmap,
	.read = rtstream_read,
	.poll = rtstream_poll,
};

static int __create_device(struct rtstream_manager *rtstreamer)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (rtstreamer->rdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_RTSTREAM_DEV_NAME, sizeof(gdev->name));
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtstream_fops;

	rtscam_ge_set_drvdata(gdev, rtstreamer);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rtstreamer->rdev = gdev;

	return 0;
}

static void __remove_device(struct rtstream_manager *rtstreamer)
{
	struct rtscam_ge_device *gdev;

	if (!rtstreamer->rdev)
		return;

	gdev = rtstreamer->rdev;

	rtscam_ge_unregister_device(gdev);
	rtstreamer->rdev = NULL;
}

static void __release_chn_list(struct rtstream_manager *rtstreamer)
{
	struct rtstream_chn_info_t *info_t, *temp1;
	struct rtstream_chn_pipe_t *pipe_t, *temp2;

	mutex_lock(&manager.lock);
	list_for_each_entry_safe(info_t, temp1, &rtstreamer->chn_head, list) {
		list_del(&info_t->list);
		kfree(info_t);
	}
	list_for_each_entry_safe(pipe_t, temp2, &rtstreamer->pipe_head, list) {
		list_del(&pipe_t->list);
		kfree(pipe_t);
	}
	mutex_unlock(&manager.lock);
}

#define RTS_OBJ_ATTR(_var, _name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_var = __ATTR(_name, _mode, _show, _store)

#define RTS_ATTR_GROUP(_var, _name, _mode, _show, _store) \
	static RTS_OBJ_ATTR(_var, _name, _mode, _show, _store);			\
	static struct attribute *sysfs_##_var##_attributes[] = {		\
		&kobj_attr_##_var.attr,						\
		NULL								\
	};									\
	static const struct attribute_group sysfs_##_var##_attr_group = {	\
		.attrs = sysfs_##_var##_attributes,				\
	}

/* chn info */
static ssize_t get_chn_info(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	struct rtstream_mem *p;
	struct rtstream_unit_info_t *unit, *temp;
	int i = 0;
	int num = 0;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	num += scnprintf(buf + num, PAGE_SIZE, "channel information:\n");
	num += scnprintf(buf + num, PAGE_SIZE, "------------------------\n");
	num += scnprintf(buf + num, PAGE_SIZE, "chnno\tname\tstate\tpoll times\trun times\tdo run times\trun s times\trun f times\tpid\n");

	list_for_each_entry_safe(unit, temp, &manager.unit_head, list) {
		p = manager.vmem + RTSTREAM_MEM_STEPSIZE * unit->index;

		num += scnprintf(buf + num, PAGE_SIZE,
				"%d\t%s\t%ld\t%-10ld\t%-10ld\t%-10ld\t%-10ld\t%-10ld\t%d",
				unit->chnno, p->statis.name,
				(unsigned long)p->statis.unit_state,
				p->statis.poll_times,
				p->statis.run_times,
				p->statis.do_run_times,
				p->statis.run_success_times,
				p->statis.run_fail_times,
				unit->pid);

		num += scnprintf(buf + num, PAGE_SIZE, "\t");
		for (i = 0; i < RTSTREAM_UNIT_OUTBUF_MAX; i++)
			num += scnprintf(buf + num, PAGE_SIZE, "[%d]:%d  ",
					i, p->statis.buf_state[i]);
		num += scnprintf(buf + num, PAGE_SIZE, "\n");
	}

	__rtstream_unlock();

	return num;
}

static void __cleanup_unused_unit(void)
{
	struct rtstream_unit_info_t *uinfo, *utemp;
	struct rtstream_mem_info mem;

	list_for_each_entry_safe(uinfo, utemp, &manager.unit_head, list) {
		if (uinfo->status)
			continue;
		mem.chnno = uinfo->chnno;
		mem.index = uinfo->index;
		mem.pid = uinfo->pid;
		__put_mem_info(&manager, &mem);
	}
}

static ssize_t set_chn_info(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t count)
{
	if (count != 3)
		return -EINVAL;

	if (memcmp(buf, "-1", 2))
		return -EINVAL;

	__cleanup_unused_unit();
	return count;
}
RTS_ATTR_GROUP(chn_info, chn_info, 0664, get_chn_info, set_chn_info);

/* stream */
static ssize_t get_stream(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	int num = 0;
	struct rtstream_chn_info_t *info_t, *temp1;
	struct rtstream_chn_pipe_t *pipe_t, *temp2;

	if (__rtstream_lock())
		return -ERESTARTSYS;
	num += scnprintf(buf + num, PAGE_SIZE, "name\tchnno\tpid\n");
	num += scnprintf(buf + num, PAGE_SIZE, "==========================\n");

	list_for_each_entry_safe(info_t, temp1, &manager.chn_head, list) {
		num += scnprintf(buf + num, PAGE_SIZE, "%c%c%c%c\t",
				info_t->chn_id & 0xFF,
				(info_t->chn_id >> 8) & 0xFF,
				(info_t->chn_id >> 16) & 0xFF,
				(info_t->chn_id >> 24) & 0xFF);
		num += scnprintf(buf + num, PAGE_SIZE, "%u\t%u\n",
				info_t->chn_no, info_t->chn_pid);
	}

	num += scnprintf(buf + num, PAGE_SIZE, "\nsrc\tsrc_no\t\tdst\tdst_no\tpid\n");
	num += scnprintf(buf + num, PAGE_SIZE, "=============================================\n");

	list_for_each_entry_safe(pipe_t, temp2, &manager.pipe_head, list) {
		num += scnprintf(buf + num, PAGE_SIZE,
				"%c%c%c%c\t%u", pipe_t->src_id & 0xFF,
				(pipe_t->src_id >> 8) & 0xFF,
				(pipe_t->src_id >> 16) & 0xFF,
				(pipe_t->src_id >> 24) & 0xFF,
				pipe_t->src_no);
		num += scnprintf(buf + num, PAGE_SIZE, "\t-->\t");
		num += scnprintf(buf + num, PAGE_SIZE,
				"%c%c%c%c\t%u", pipe_t->dst_id & 0xFF,
				(pipe_t->dst_id >> 8) & 0xFF,
				(pipe_t->dst_id >> 16) & 0xFF,
				(pipe_t->dst_id >> 24) & 0xFF,
				pipe_t->dst_no);
		num += scnprintf(buf + num, PAGE_SIZE, "\t%u\n", pipe_t->pipe_pid);
	}

	__rtstream_unlock();

	return num;
}

static void __cleanup_unused_chn(void)
{
	struct rtstream_chn_info_t *cinfo, *ctemp;
	struct rtstream_chn_pipe_t *pinfo, *ptemp;
	struct rtstream_chn_info chn;
	struct rtstream_chn_pipe pipe;

	list_for_each_entry_safe(cinfo, ctemp, &manager.chn_head, list) {
		if (cinfo->status)
			continue;

		chn.chn_id = cinfo->chn_id;
		chn.chn_no = cinfo->chn_no;
		chn.chn_pid = cinfo->chn_pid;
		__del_sys_chn(&manager, &chn);
	}

	list_for_each_entry_safe(pinfo, ptemp, &manager.pipe_head, list) {
		if (pinfo->status)
			continue;

		pipe.src_id = pinfo->src_id;
		pipe.src_no = pinfo->src_no;
		pipe.dst_id = pinfo->dst_id;
		pipe.dst_no = pinfo->dst_no;
		pipe.pipe_pid = pinfo->pipe_pid;
		__del_sys_chn_pipe(&manager, &pipe);
	}
}

static ssize_t set_stream(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t count)
{
	if (count != 3)
		return -EINVAL;

	if (memcmp(buf, "-1", 2))
		return -EINVAL;

	__cleanup_unused_chn();
	return count;
}
RTS_ATTR_GROUP(stream, stream, 0664, get_stream, set_stream);

/* global config */
static ssize_t get_g_cfg_shlr(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int num = 0;

	if (manager.g_cfg_shlr)
		num += scnprintf(buf + num, PAGE_SIZE, "1\n");
	else
		num += scnprintf(buf + num, PAGE_SIZE, "0\n");

	return num;
}

static ssize_t set_g_cfg_shlr(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (count != 2)
		return -EINVAL;

	if (!memcmp(buf, "1", 1))
		manager.g_cfg_shlr = 1;
	else if (!memcmp(buf, "0", 1))
		manager.g_cfg_shlr = 0;
	else
		return -EINVAL;

	return count;
}
static RTS_OBJ_ATTR(g_cfg_shlr, support_h26x_long_ref,
		0664, get_g_cfg_shlr, set_g_cfg_shlr);

static struct attribute *sysfs_g_cfg_attributes[] = {
	&kobj_attr_g_cfg_shlr.attr,
	NULL
};

static const struct attribute_group sysfs_g_cfg_attr_group = {
	.name = "g_cfg",
	.attrs = sysfs_g_cfg_attributes,
};

static const struct attribute_group *sysfs_rtstream_attr_groups[] = {
	&sysfs_chn_info_attr_group,
	&sysfs_stream_attr_group,
	&sysfs_g_cfg_attr_group,
	NULL
};

static int __rtstream_trig_signal(int chnno, int pid)
{
	manager.s_info.chnno = chnno;
	manager.s_info.pid = pid;

	manager.debug_signal = manager.stream_num;
	wake_up_interruptible(&manager.debug_wq);

	return 0;
}

/* signal */
static ssize_t get_mod_signal(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int num = 0;

	if (memcmp(kobj->name, "h26x", 4) == 0) {
		manager.h26x_info.sig = RTSTREAM_SIG_OP_R_PARAM;
		__rtstream_trig_signal(manager.h26x_info.enable,
				manager.h26x_info.pid);
		num = scnprintf(buf, PAGE_SIZE, "h26x signal\n");
	} else if (memcmp(kobj->name, "aec", 3) == 0) {
		manager.aec_info.sig = RTSTREAM_SIG_OP_R_PARAM;
		__rtstream_trig_signal(manager.aec_info.enable,
				manager.aec_info.pid);
		num = scnprintf(buf, PAGE_SIZE, "aec signal\n");
	} else {
		num = scnprintf(buf, PAGE_SIZE, "no kobject\n");
	}

	return num;
}

static ssize_t set_mod_signal(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (count != 3)
		return -EINVAL;

	if (memcmp(buf, "-1", 2))
		return -EINVAL;

	manager.debug_signal = 0;

	return count;
}
RTS_ATTR_GROUP(mod_signal, signal, 0664, get_mod_signal, set_mod_signal);

/* pid */
static ssize_t get_mod_pid(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (memcmp(kobj->name, "h26x", 4) == 0)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				manager.h26x_info.pid);
	else if (memcmp(kobj->name, "aec", 3) == 0)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				manager.aec_info.pid);

	return scnprintf(buf, PAGE_SIZE, "no kobject\n");
}

static ssize_t set_mod_pid(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int find = 0, pid, ret;
	struct rtstream_chn_info_t *info_t, *temp1;
	struct rtstream_mod_info *m_info = NULL;

	ret = kstrtoint(buf, 0, &pid);
	if (ret)
		return count;

	if (memcmp(kobj->name, "h26x", 4) == 0)
		m_info = &manager.h26x_info;
	else if (memcmp(kobj->name, "aec", 3) == 0)
		m_info = &manager.aec_info;
	else
		return count;

	if (__rtstream_lock())
		return -ERESTARTSYS;

	list_for_each_entry_safe(info_t, temp1, &manager.chn_head, list) {
		if ((info_t->chn_no == m_info->enable) &&
			(info_t->chn_pid == pid)) {
			find = 1;
			break;
		}
	}

	__rtstream_unlock();

	if (find == 0) {
		rtstream_err("error pid(%d) for chnno(%d)\n",
				pid, m_info->enable);
		return -EINVAL;
	}

	m_info->pid = pid;

	return count;
}
RTS_ATTR_GROUP(mod_pid, pid, 0664, get_mod_pid, set_mod_pid);

/* h26x object */
#define RTS_H26X_SYS_PARAM(_name)									\
	static ssize_t get_h26x_##_name(struct kobject *kobj, struct kobj_attribute *attr, char *buf)	\
	{												\
		return scnprintf(buf, PAGE_SIZE, "%d\n", manager.h26x_info.h26x_op._name);		\
	}												\
	static ssize_t set_h26x_##_name(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) \
	{												\
		int value, ret;										\
		ret = kstrtoint(buf, 0, &value);							\
		if (ret)										\
			return count;									\
		manager.h26x_info.h26x_op._name = value;								\
		return count;										\
	}												\
	static RTS_OBJ_ATTR(h26x_##_name, _name, 0664, get_h26x_##_name, set_h26x_##_name)

RTS_H26X_SYS_PARAM(EncBitrate);
RTS_H26X_SYS_PARAM(IntraQpOffset);
RTS_H26X_SYS_PARAM(MaxDeltaQp);
RTS_H26X_SYS_PARAM(MinQp);
RTS_H26X_SYS_PARAM(MaxQp);
RTS_H26X_SYS_PARAM(IntraMinQp);
RTS_H26X_SYS_PARAM(IntraMaxQp);
RTS_H26X_SYS_PARAM(GOP);
RTS_H26X_SYS_PARAM(CULevelRCEnable);
RTS_H26X_SYS_PARAM(HvsQpEnable);
RTS_H26X_SYS_PARAM(HvsQpScale);
RTS_H26X_SYS_PARAM(DisableDBK);
RTS_H26X_SYS_PARAM(BetaOffsetDiv2);
RTS_H26X_SYS_PARAM(TcOffsetDiv2);
RTS_H26X_SYS_PARAM(CbQpOffset);
RTS_H26X_SYS_PARAM(CrQpOffset);
RTS_H26X_SYS_PARAM(BgDetectLevel);
RTS_H26X_SYS_PARAM(DeNoiseLevel);
RTS_H26X_SYS_PARAM(RcCvbrMv0);
RTS_H26X_SYS_PARAM(RcCvbrMv1);
RTS_H26X_SYS_PARAM(RcCvbrMv2);
RTS_H26X_SYS_PARAM(RcCvbrVar0);
RTS_H26X_SYS_PARAM(RcCvbrVar1);
RTS_H26X_SYS_PARAM(RcCvbrVar2);
RTS_H26X_SYS_PARAM(RcCvbrMvWeightScale);
RTS_H26X_SYS_PARAM(RcCvbrVarWeightScale);
RTS_H26X_SYS_PARAM(IDqp);
RTS_H26X_SYS_PARAM(PDqp);
RTS_H26X_SYS_PARAM(PDqpFirst);
RTS_H26X_SYS_PARAM(AnchorDqp);
RTS_H26X_SYS_PARAM(EnCustomMD);
RTS_H26X_SYS_PARAM(CU08MergeDeltaRate);
RTS_H26X_SYS_PARAM(CU16MergeDeltaRate);
RTS_H26X_SYS_PARAM(CU32MergeDeltaRate);
RTS_H26X_SYS_PARAM(CU08IntraDeltaRate);
RTS_H26X_SYS_PARAM(CU16IntraDeltaRate);
RTS_H26X_SYS_PARAM(CU32IntraDeltaRate);
RTS_H26X_SYS_PARAM(CU08InterDeltaRate);
RTS_H26X_SYS_PARAM(CU16InterDeltaRate);
RTS_H26X_SYS_PARAM(CU32InterDeltaRate);
RTS_H26X_SYS_PARAM(PU04DeltaRate);
RTS_H26X_SYS_PARAM(PU08DeltaRate);
RTS_H26X_SYS_PARAM(PU16DeltaRate);
RTS_H26X_SYS_PARAM(PU32DeltaRate);

static struct attribute *sysfs_h26x_param_attributes[] = {
	&kobj_attr_h26x_EncBitrate.attr,
	&kobj_attr_h26x_IntraQpOffset.attr,
	&kobj_attr_h26x_MaxDeltaQp.attr,
	&kobj_attr_h26x_MinQp.attr,
	&kobj_attr_h26x_MaxQp.attr,
	&kobj_attr_h26x_IntraMinQp.attr,
	&kobj_attr_h26x_IntraMaxQp.attr,
	&kobj_attr_h26x_GOP.attr,
	&kobj_attr_h26x_CULevelRCEnable.attr,
	&kobj_attr_h26x_HvsQpEnable.attr,
	&kobj_attr_h26x_HvsQpScale.attr,
	&kobj_attr_h26x_DisableDBK.attr,
	&kobj_attr_h26x_BetaOffsetDiv2.attr,
	&kobj_attr_h26x_TcOffsetDiv2.attr,
	&kobj_attr_h26x_CbQpOffset.attr,
	&kobj_attr_h26x_CrQpOffset.attr,
	&kobj_attr_h26x_BgDetectLevel.attr,
	&kobj_attr_h26x_DeNoiseLevel.attr,
	&kobj_attr_h26x_RcCvbrMv0.attr,
	&kobj_attr_h26x_RcCvbrMv1.attr,
	&kobj_attr_h26x_RcCvbrMv2.attr,
	&kobj_attr_h26x_RcCvbrVar0.attr,
	&kobj_attr_h26x_RcCvbrVar1.attr,
	&kobj_attr_h26x_RcCvbrVar2.attr,
	&kobj_attr_h26x_RcCvbrMvWeightScale.attr,
	&kobj_attr_h26x_RcCvbrVarWeightScale.attr,
	&kobj_attr_h26x_IDqp.attr,
	&kobj_attr_h26x_PDqp.attr,
	&kobj_attr_h26x_PDqpFirst.attr,
	&kobj_attr_h26x_AnchorDqp.attr,
	&kobj_attr_h26x_EnCustomMD.attr,
	&kobj_attr_h26x_CU08MergeDeltaRate.attr,
	&kobj_attr_h26x_CU16MergeDeltaRate.attr,
	&kobj_attr_h26x_CU32MergeDeltaRate.attr,
	&kobj_attr_h26x_CU08IntraDeltaRate.attr,
	&kobj_attr_h26x_CU16IntraDeltaRate.attr,
	&kobj_attr_h26x_CU32IntraDeltaRate.attr,
	&kobj_attr_h26x_CU08InterDeltaRate.attr,
	&kobj_attr_h26x_CU16InterDeltaRate.attr,
	&kobj_attr_h26x_CU32InterDeltaRate.attr,
	&kobj_attr_h26x_PU04DeltaRate.attr,
	&kobj_attr_h26x_PU08DeltaRate.attr,
	&kobj_attr_h26x_PU16DeltaRate.attr,
	&kobj_attr_h26x_PU32DeltaRate.attr,
	NULL
};

static const struct attribute_group sysfs_h26x_param_attr_group = {
	.name = "parameters",
	.attrs = sysfs_h26x_param_attributes,
};

static const struct attribute_group *sysfs_h26x_2_attr_groups[] = {
	&sysfs_mod_pid_attr_group,
	&sysfs_mod_signal_attr_group,
	&sysfs_h26x_param_attr_group,
	NULL
};

/* aec object */
static ssize_t get_aec_dump_path(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", manager.aec_info.aec_op.dump_path);
}

static ssize_t set_aec_dump_path(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (count >= 126)
		return -EINVAL;

	memset(manager.aec_info.aec_op.dump_path, 0, 128);
	memcpy(manager.aec_info.aec_op.dump_path, buf, count - 1);

	return count;
}
static RTS_OBJ_ATTR(aec_dump_path, dump_path, 0664, get_aec_dump_path, set_aec_dump_path);

static ssize_t get_aec_dump_enable(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", manager.aec_info.aec_op.dump_enable);
}

static ssize_t set_aec_dump_enable(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value, ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret)
		return count;

	manager.aec_info.aec_op.dump_enable = value;

	return count;
}
static RTS_OBJ_ATTR(aec_dump_enable, dump_enable, 0664, get_aec_dump_enable, set_aec_dump_enable);

static struct attribute *sysfs_aec_param_attributes[] = {
	&kobj_attr_aec_dump_enable.attr,
	&kobj_attr_aec_dump_path.attr,
	NULL
};

static const struct attribute_group sysfs_aec_param_attr_group = {
	.name = "parameters",
	.attrs = sysfs_aec_param_attributes,
};

static const struct attribute_group *sysfs_aec_2_attr_groups[] = {
	&sysfs_mod_pid_attr_group,
	&sysfs_mod_signal_attr_group,
	&sysfs_aec_param_attr_group,
	NULL
};

static ssize_t get_mod_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (memcmp(kobj->name, "h26x", 4) == 0)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				manager.h26x_info.enable);
	else if (memcmp(kobj->name, "aec", 3) == 0)
		return scnprintf(buf, PAGE_SIZE, "%d\n",
				manager.aec_info.enable);

	return scnprintf(buf, PAGE_SIZE, "no kobject\n");
}

static ssize_t set_mod_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int chn_no, pid, ret;
	int find = 0;
	struct rtstream_chn_info_t *info_t, *temp1;

	ret = kstrtoint(buf, 0, &chn_no);
	if (ret)
		return -EINVAL;

	if (memcmp(kobj->name, "h26x", 4) == 0) {
		if ((chn_no >= 0 && chn_no < 20) || (chn_no >= 30 &&
					chn_no < 50) || chn_no >= 60) {
			rtstream_err("h26x: error chnno, %d\n", chn_no);
			return -EINVAL;
		}
		if (chn_no < 0) {
			if (manager.h26x_info.enable >= 0)
				sysfs_remove_groups(kobj,
						sysfs_h26x_2_attr_groups);
			manager.h26x_info.enable = -1;
			return count;
		}
	} else if (memcmp(kobj->name, "aec", 3) == 0) {
		if ((chn_no >= 0 && chn_no < 220) || chn_no >= 230) {
			rtstream_err("aec: error chnno, %d\n", chn_no);
			return -EINVAL;
		}
		if (chn_no < 0) {
			if (manager.aec_info.enable >= 0)
				sysfs_remove_groups(kobj,
						sysfs_aec_2_attr_groups);
			manager.aec_info.enable = -1;
			return count;
		}
	} else {
		return -EINVAL;
	}

	/* check chnno */
	if (__rtstream_lock())
		return -ERESTARTSYS;

	list_for_each_entry_safe(info_t, temp1, &manager.chn_head, list) {
		if (info_t->chn_no == chn_no) {
			find = 1;
			pid = info_t->chn_pid;
			break;
		}
	}

	__rtstream_unlock();

	if (find == 0) {
		rtstream_err("no chnno(%d) channel\n", chn_no);
		return -EINVAL;
	}

	if (memcmp(kobj->name, "h26x", 4) == 0) {
		if (manager.h26x_info.enable < 0) {
			ret = sysfs_create_groups(kobj,
					sysfs_h26x_2_attr_groups);
			if (ret < 0)
				return count;
		}
		manager.h26x_info.enable = chn_no;
		manager.h26x_info.pid = pid;
		manager.h26x_info.sig = RTSTREAM_SIG_OP_W_PARAM;
		__rtstream_trig_signal(manager.h26x_info.enable,
				manager.h26x_info.pid);
	} else if (memcmp(kobj->name, "aec", 3) == 0) {
		if (manager.aec_info.enable < 0) {
			ret = sysfs_create_groups(kobj,
					sysfs_aec_2_attr_groups);
			if (ret < 0)
				return count;
		}
		manager.aec_info.enable = chn_no;
		manager.aec_info.pid = pid;
		manager.aec_info.sig = RTSTREAM_SIG_OP_W_PARAM;
		__rtstream_trig_signal(manager.aec_info.enable,
				manager.aec_info.pid);
	}

	return count;
}
RTS_ATTR_GROUP(mod_enable, enable, 0664, get_mod_enable, set_mod_enable);

static const struct attribute_group *sysfs_h26x_1_attr_groups[] = {
	&sysfs_mod_enable_attr_group,
	NULL
};

static const struct attribute_group *sysfs_aec_1_attr_groups[] = {
	&sysfs_mod_enable_attr_group,
	NULL
};

static void __release_kobj(struct rtstream_manager *rtstreamer)
{
	if (rtstreamer->rts_obj.h26x_obj) {
		if (manager.h26x_info.enable > 0)
			sysfs_remove_groups(rtstreamer->rts_obj.h26x_obj,
					sysfs_h26x_2_attr_groups);
		sysfs_remove_groups(rtstreamer->rts_obj.h26x_obj,
			sysfs_h26x_1_attr_groups);
		kobject_put(rtstreamer->rts_obj.h26x_obj);
	}

	if (rtstreamer->rts_obj.aec_obj) {
		if (manager.aec_info.enable > 0)
			sysfs_remove_groups(rtstreamer->rts_obj.aec_obj,
					sysfs_aec_2_attr_groups);
		sysfs_remove_groups(rtstreamer->rts_obj.aec_obj,
			sysfs_aec_1_attr_groups);
		kobject_put(rtstreamer->rts_obj.aec_obj);
	}

	if (rtstreamer->rts_obj.debug_obj) {
		sysfs_remove_groups(rtstreamer->rts_obj.debug_obj,
			sysfs_rtstream_attr_groups);
		kobject_put(rtstreamer->rts_obj.debug_obj);
	}
}

static int __create_kobj(struct rtstream_manager *rtstreamer)
{
	struct kobject *root_obj = &rtstreamer->rdev->dev.kobj;
	int ret = 0;

	rtstreamer->rts_obj.debug_obj =
		kobject_create_and_add("debug", root_obj);
	if (rtstreamer->rts_obj.debug_obj == NULL)
		goto error;

	ret = sysfs_create_groups(rtstreamer->rts_obj.debug_obj,
			sysfs_rtstream_attr_groups);
	if (ret < 0)
		goto error;

	rtstreamer->rts_obj.h26x_obj =
		kobject_create_and_add("h26x", rtstreamer->rts_obj.debug_obj);
	if (rtstreamer->rts_obj.h26x_obj == NULL)
		goto error;

	ret = sysfs_create_groups(rtstreamer->rts_obj.h26x_obj,
			sysfs_h26x_1_attr_groups);
	if (ret < 0)
		goto error;

	rtstreamer->rts_obj.aec_obj =
		kobject_create_and_add("aec", rtstreamer->rts_obj.debug_obj);
	if (rtstreamer->rts_obj.aec_obj == NULL)
		goto error;

	ret = sysfs_create_groups(rtstreamer->rts_obj.aec_obj,
			sysfs_aec_1_attr_groups);
	if (ret < 0)
		goto error;

	return 0;
error:
	__release_kobj(rtstreamer);

	return ret;
}

static int __init rtstream_init(void)
{
	rtstream_info("%s\n", __func__);

	mutex_init(&manager.lock);

	__create_device(&manager);
	__alloc_vmem(&manager, RTSTREAM_MEM_TOTAL_SIZE);
	INIT_LIST_HEAD(&manager.cfg_list);
	INIT_LIST_HEAD(&manager.chn_head);
	INIT_LIST_HEAD(&manager.pipe_head);
	INIT_LIST_HEAD(&manager.unit_head);

	__create_kobj(&manager);

	manager.stream_num = 0;
	manager.h26x_info.enable = -1;
	manager.aec_info.enable = -1;
	manager.debug_signal = 0;
	init_waitqueue_head(&manager.debug_wq);

	return 0;
}

static void __exit rtstream_exit(void)
{
	rtstream_info("%s\n", __func__);

	__release_kobj(&manager);

	__remove_device(&manager);
	__release_chn_list(&manager);
	__release_mem_info(&manager);

	if (manager.vmem) {
		vfree(manager.vmem);
		manager.vmem = NULL;
	}

	__release_cfg_info(&manager);
	manager.size = 0;
}

module_init(rtstream_init);
module_exit(rtstream_exit);

MODULE_DESCRIPTION("Realsil rtstream driver");
MODULE_AUTHOR("Wind Han <wind_han@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
