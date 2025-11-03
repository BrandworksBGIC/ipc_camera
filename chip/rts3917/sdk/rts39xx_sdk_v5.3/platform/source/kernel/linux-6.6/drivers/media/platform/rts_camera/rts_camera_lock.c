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
#include "rts_camera.h"
#include "rts_camera_waitqueue.h"
#include "linux/rts_camera_lock.h"

#define RTS_CAM_LOCK_DEV_NAME		"rtslock"

struct rtscam_lock_queue {
	struct list_head list;
	unsigned long key;
	char name[16];
	struct rts_wait_queue *queue;
};

struct rtscam_lock_t {
	struct list_head queues;

	struct rtscam_ge_device *rdev;
	struct mutex lock;
};

static struct rtscam_lock_t m_rtslock;

static struct rtscam_lock_queue *__find_queue(struct rtscam_lock_t *rtslock,
					      unsigned long key)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return NULL;

	list_for_each_entry(queue, &rtslock->queues, list) {
		if (queue->key == key)
			return queue;
	}

	return NULL;
}

static int __rtscam_lock_alloc(struct rtscam_lock_t *rtslock, unsigned long key)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;

	queue = __find_queue(rtslock, key);
	if (queue)
		return 0;

	queue = vzalloc(sizeof(*queue));
	if (!queue)
		return -ENOMEM;

	queue->queue = rtscam_allocate_wait_queue(&rtslock->rdev->dev);
	if (!queue->queue) {
		vfree(queue);
		return -ENOMEM;
	}

	queue->key = key;
	list_add_tail(&queue->list, &rtslock->queues);

	return 0;
}

static int rtscam_lock_alloc(struct rtscam_lock_t *rtslock, unsigned long key)
{
	int ret;

	if (!rtslock)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	ret = __rtscam_lock_alloc(rtslock, key);
	mutex_unlock(&rtslock->lock);

	return ret;
}

static int __rtscam_lock_free(struct rtscam_lock_t *rtslock, unsigned long key)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;

	queue = __find_queue(rtslock, key);
	if (!queue)
		return 0;

	list_del_init(&queue->list);
	rtscam_release_wait_queue(queue->queue);
	queue->queue = NULL;

	vfree(queue);

	return 0;
}

static int rtscam_lock_free(struct rtscam_lock_t *rtslock, unsigned long key)
{
	int ret;

	if (!rtslock)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	ret = __rtscam_lock_free(rtslock, key);
	mutex_unlock(&rtslock->lock);

	return ret;
}

static int __rtscam_lock_set_num(struct rtscam_lock_t *rtslock,
				 unsigned long key, int num)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;

	queue = __find_queue(rtslock, key);
	if (!queue)
		return -EINVAL;

	return rtscam_init_wait_queue(queue->queue, num);
}

static int rtscam_lock_set_num(struct rtscam_lock_t *rtslock,
			unsigned long key, int num)
{
	int ret;

	if (!rtslock)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	ret = __rtscam_lock_set_num(rtslock, key, num);
	mutex_unlock(&rtslock->lock);

	return ret;
}

static int rtscam_lock_set_name(struct rtscam_lock_t *rtslock,
			 struct rtscam_lock_name *name)
{
	int ret;
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;
	if (!name)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	queue = __find_queue(rtslock, name->key);
	if (!queue) {
		ret = -EINVAL;
		goto exit;
	}
	strncpy(queue->name, name->name, sizeof(name->name) - 1);
	ret = 0;
exit:
	mutex_unlock(&rtslock->lock);

	return ret;
}

static int rtscam_lock_wait(struct rtscam_lock_t *rtslock, unsigned long key)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	queue = __find_queue(rtslock, key);
	mutex_unlock(&rtslock->lock);
	if (!queue)
		return -EINVAL;

	return rtscam_wait(queue->queue);
}

static int rtscam_lock_post(struct rtscam_lock_t *rtslock, unsigned long key)
{
	struct rtscam_lock_queue *queue;

	if (!rtslock)
		return -EINVAL;

	mutex_lock(&rtslock->lock);
	queue = __find_queue(rtslock, key);
	mutex_unlock(&rtslock->lock);
	if (!queue)
		return -EINVAL;

	return rtscam_post(queue->queue);
}

static int rtscam_lock_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_lock_t *rtslock = rtscam_ge_get_drvdata(gdev);

	filp->private_data = rtslock;

	return 0;
}

static int rtscam_lock_close(struct file *filp)
{
	struct rtscam_lock_t *rtslock = filp->private_data;

	filp->private_data = NULL;

	if (!rtslock)
		return -EINVAL;

	return 0;
}

static long rtscam_lock_do_ioctl(struct file *filp, unsigned int cmd,
				 void *arg)
{
	struct rtscam_lock_t *rtslock = filp->private_data;
	unsigned long key;
	struct rtscam_lock_num *pnum;
	int ret = 0;

	if (!rtslock)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != RTSCAMLOCK_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case RTSCAMLOCK_IOC_ALLOC:
		key = *(unsigned long *)arg;
		ret = rtscam_lock_alloc(rtslock, key);
		break;
	case RTSCAMLOCK_IOC_FREE:
		key = *(unsigned long *)arg;
		ret = rtscam_lock_free(rtslock, key);
		break;
	case RTSCAMLOCK_IOC_INIT:
		pnum = arg;
		ret = rtscam_lock_set_num(rtslock, pnum->key, pnum->num);
		break;
	case RTSCAMLOCK_IOC_WAIT:
		key = *(unsigned long *)arg;
		ret = rtscam_lock_wait(rtslock, key);
		break;
	case RTSCAMLOCK_IOC_POST:
		key = *(unsigned long *)arg;
		ret = rtscam_lock_post(rtslock, key);
		break;
	case RTSCAMLOCK_IOC_SET_NAME:
		ret = rtscam_lock_set_name(rtslock, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "unknown[rtslock] ioctl 0x%08x, '%c' 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static long rtscam_lock_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_lock_do_ioctl);
}

static struct rtscam_ge_file_operations rtscam_lock_fops = {
	.owner		= THIS_MODULE,
	.open		= rtscam_lock_open,
	.release	= rtscam_lock_close,
	.unlocked_ioctl	= rtscam_lock_ioctl,
};

static ssize_t show_lockinfo(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct rtscam_lock_t *rtslock = dev_get_drvdata(dev);
	struct rtscam_lock_queue *queue;
	int num = 0;

	mutex_lock(&rtslock->lock);
	list_for_each_entry(queue, &rtslock->queues, list) {
		num += scnprintf(buf + num, PAGE_SIZE - num,
				 "0x%lx\t%16s\t%d\t%d\n",
				 queue->key, queue->name, queue->queue->value,
				 queue->queue->refcount);
	}
	mutex_unlock(&rtslock->lock);

	return num;
}
static DEVICE_ATTR(lockinfo, 0444, show_lockinfo, NULL);

static ssize_t set_lock_wait(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct rtscam_lock_t *rtslock = dev_get_drvdata(dev);
	unsigned long key;
	int ret;

	ret = kstrtoul(buf, 0, &key);
	if (ret)
		return ret;

	ret = rtscam_lock_wait(rtslock, key);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(wait, 0220, NULL, set_lock_wait);

static ssize_t set_lock_post(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct rtscam_lock_t *rtslock = dev_get_drvdata(dev);
	unsigned long key;

	int ret;

	ret = kstrtoul(buf, 0, &key);
	if (ret)
		return ret;

	ret = rtscam_lock_post(rtslock, key);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(post, 0220, NULL, set_lock_post);

static int __create_device(struct rtscam_lock_t *rtslock)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (rtslock->rdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_CAM_LOCK_DEV_NAME, sizeof(gdev->name));
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_lock_fops;

	rtscam_ge_set_drvdata(gdev, rtslock);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rtslock->rdev = gdev;
	device_create_file(&gdev->dev, &dev_attr_lockinfo);
	device_create_file(&gdev->dev, &dev_attr_wait);
	device_create_file(&gdev->dev, &dev_attr_post);

	return 0;
}

static void __remove_device(struct rtscam_lock_t *rtslock)
{
	struct rtscam_ge_device *gdev;

	if (!rtslock->rdev)
		return;

	gdev = rtslock->rdev;

	device_remove_file(&gdev->dev, &dev_attr_lockinfo);
	device_remove_file(&gdev->dev, &dev_attr_wait);
	device_remove_file(&gdev->dev, &dev_attr_post);

	rtscam_ge_unregister_device(gdev);
	rtslock->rdev = NULL;
}

static int __init rtscam_lock_init(void)
{

	INIT_LIST_HEAD(&m_rtslock.queues);
	mutex_init(&m_rtslock.lock);

	__create_device(&m_rtslock);

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	return 0;
}

static void __exit rtscam_lock_exit(void)
{
	struct rtscam_lock_t *rtslock = &m_rtslock;
	struct rtscam_lock_queue *queue;
	struct rtscam_lock_queue *tmp;

	__remove_device(rtslock);

	mutex_lock(&rtslock->lock);
	list_for_each_entry_safe(queue, tmp, &rtslock->queues, list) {
		list_del_init(&queue->list);
		rtscam_release_wait_queue(queue->queue);
		queue->queue = NULL;

		vfree(queue);
	}
	mutex_unlock(&rtslock->lock);
}

module_init(rtscam_lock_init);
module_exit(rtscam_lock_exit);

MODULE_DESCRIPTION("Realsil cam lock driver");
MODULE_AUTHOR("Ming Qian <ming_qian@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");
