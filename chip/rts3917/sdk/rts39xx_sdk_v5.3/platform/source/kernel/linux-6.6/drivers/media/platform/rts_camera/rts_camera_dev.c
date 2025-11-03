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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "rts_camera_priv.h"

#define RTSCAM_NUM_DEVICES		256
#define RTSCAM_DEV_NAME			"rts_camera"

static struct class rtscam_ge_class = {
		.name = RTSCAM_DEV_NAME,
	};

static struct rtscam_ge_device *rtscam_devices[RTSCAM_NUM_DEVICES];
static DEFINE_MUTEX(rtscamdev_lock);
static DECLARE_BITMAP(rtscam_devnode_nums, RTSCAM_NUM_DEVICES);

struct rtscam_ge_device *rtscam_ge_device_alloc(void)
{
	return kzalloc(sizeof(struct rtscam_ge_device), GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(rtscam_ge_device_alloc);

void rtscam_ge_device_release(struct rtscam_ge_device *rdev)
{
	if (rdev)
		kfree(rdev);
}
EXPORT_SYMBOL_GPL(rtscam_ge_device_release);

static inline int rtscam_ge_is_registered(struct rtscam_ge_device *rdev)
{
	return test_bit(RTSCAM_FL_REGISTERED, &rdev->flags);
}

static inline void rtscam_get(struct rtscam_ge_device *rdev)
{
	get_device(&rdev->dev);
}

static inline void rtscam_put(struct rtscam_ge_device *rdev)
{
	put_device(&rdev->dev);
}

struct rtscam_ge_device *rtscam_devdata(struct file *filp)
{
	return rtscam_devices[iminor(file_inode(filp))];
}
EXPORT_SYMBOL_GPL(rtscam_devdata);

static int rtscam_ge_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);

	get_device(&rdev->dev);

	if (rdev->fops->open) {
		if (rtscam_ge_is_registered(rdev))
			ret = rdev->fops->open(filp);
		else
			ret = -ENODEV;
	}

	if (ret)
		put_device(&rdev->dev);

	return ret;
}

static ssize_t rtscam_ge_read(struct file *filp, char __user *buf,
			      size_t sz, loff_t *off)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = -ENODEV;

	if (!rdev->fops->read)
		return -EINVAL;

	if (rtscam_ge_is_registered(rdev))
		ret = rdev->fops->read(filp, buf, sz, off);

	return ret;
}

static ssize_t rtscam_ge_write(struct file *filp, const char __user *buf,
			       size_t sz, loff_t *off)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = -ENODEV;

	if (!rdev->fops->write)
		return -EINVAL;

	if (rtscam_ge_is_registered(rdev))
		ret = rdev->fops->write(filp, buf, sz, off);

	return ret;
}

static unsigned int rtscam_ge_poll(struct file *filp,
				   struct poll_table_struct *poll)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	unsigned int res = POLLERR | POLLHUP;

	if (!rdev->fops->poll)
		return DEFAULT_POLLMASK;

	if (rtscam_ge_is_registered(rdev))
		res = rdev->fops->poll(filp, poll);

	return res;
}

static long rtscam_ge_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = -ENODEV;

	if (!rtscam_ge_is_registered(rdev))
		return -ENODEV;

	if (rdev->fops->ioctl) {
		if (mutex_lock_interruptible(&rdev->lock))
			return -ERESTARTSYS;

		ret = rdev->fops->ioctl(filp, cmd, arg);

		mutex_unlock(&rdev->lock);

		if (ret != -ENOTTY)
			return ret;
	}
	if (rdev->fops->unlocked_ioctl)
		ret = rdev->fops->unlocked_ioctl(filp, cmd, arg);

	return ret;
}

static int rtscam_ge_mmap(struct file *filp, struct vm_area_struct *vm)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = -ENODEV;

	if (!rdev->fops->mmap)
		return -ENODEV;

	if (rtscam_ge_is_registered(rdev))
		ret = rdev->fops->mmap(filp, vm);

	return ret;
}

static int rtscam_ge_fasync(int fd, struct file *filp, int mode)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = -ENODEV;

	if (!rtscam_ge_is_registered(rdev))
		return -ENODEV;

	if (rdev->fops->fasync)
		ret = rdev->fops->fasync(fd, filp, mode);
	else
		ret = fasync_helper(fd, filp, mode, &rdev->async_queue);

	return ret;
}

static int rtscam_ge_release(struct inode *inode, struct file *filp)
{
	struct rtscam_ge_device *rdev = rtscam_devdata(filp);
	int ret = 0;

	if (rdev->fops->release)
		ret = rdev->fops->release(filp);

	rtscam_ge_fasync(-1, filp, 0);

	put_device(&rdev->dev);

	return ret;
}

void rtscam_ge_kill_fasync(struct rtscam_ge_device *rdev, int sig, int band)
{
	if (rdev && rdev->async_queue)
		kill_fasync(&rdev->async_queue, sig, band);
}
EXPORT_SYMBOL_GPL(rtscam_ge_kill_fasync);

static const struct file_operations rtscam_fops = {
	.owner = THIS_MODULE,
	.open = rtscam_ge_open,
	.release = rtscam_ge_release,
	.read = rtscam_ge_read,
	.write = rtscam_ge_write,
	.poll = rtscam_ge_poll,
	.unlocked_ioctl = rtscam_ge_ioctl,
	.mmap = rtscam_ge_mmap,
	.fasync = rtscam_ge_fasync,
};

static void rtscam_device_release(struct device *cd)
{
	struct rtscam_ge_device *rdev = to_rtscam_ge_device(cd);

	cdev_del(&rdev->cdev);
	mutex_lock(&rtscamdev_lock);
	rtscam_devices[rdev->minor] = NULL;
	clear_bit(rdev->minor, rtscam_devnode_nums);
	mutex_unlock(&rtscamdev_lock);

	rdev->release(rdev);
}

int rtscam_ge_register_device(struct rtscam_ge_device *rdev)
{
	int ret;
	int nr;
	dev_t devno;

	if (!rdev || !rdev->fops || !rdev->release)
		return -EINVAL;

	rdev->async_queue = NULL;

	mutex_lock(&rtscamdev_lock);
	nr = find_next_zero_bit(rtscam_devnode_nums, RTSCAM_NUM_DEVICES, 0);
	if (nr == RTSCAM_NUM_DEVICES) {
		rtsprintk(RTS_TRACE_ERROR,
			  "could not get a gree device node number\n");
		mutex_unlock(&rtscamdev_lock);
		return -ENFILE;
	}

	rdev->minor = nr;
	set_bit(nr, rtscam_devnode_nums);

	devno = MKDEV(RTSCAM_DEV_MAJOR, rdev->minor);

	mutex_unlock(&rtscamdev_lock);

	cdev_init(&rdev->cdev, &rtscam_fops);
	rdev->cdev.owner = rdev->fops->owner;
	ret = cdev_add(&rdev->cdev, devno, 1);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR, "%s:cdev_add fail\n", __func__);
		goto cleanup;
	}

	rdev->dev.class = &rtscam_ge_class;
	rdev->dev.devt = devno;
	if (rdev->parent)
		rdev->dev.parent = rdev->parent;
	if (strlen(rdev->name) > 0)
		dev_set_name(&rdev->dev, "%s", rdev->name);
	else {
		dev_set_name(&rdev->dev, "%s%d", RTSCAM_DEV_NAME, nr);
		snprintf(rdev->name, sizeof(rdev->name), "%s%d",
			 RTSCAM_DEV_NAME, nr);
	}
	ret = device_register(&rdev->dev);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "%s:device_register fail\n", __func__);
		goto cleanup;
	}
	rdev->dev.release = rtscam_device_release;

	mutex_init(&rdev->lock);

	set_bit(RTSCAM_FL_REGISTERED, &rdev->flags);
	mutex_lock(&rtscamdev_lock);
	rtscam_devices[rdev->minor] = rdev;
	mutex_unlock(&rtscamdev_lock);

	return 0;
cleanup:
	cdev_del(&rdev->cdev);
	mutex_lock(&rtscamdev_lock);
	clear_bit(rdev->minor, rtscam_devnode_nums);
	mutex_unlock(&rtscamdev_lock);
	rdev->minor = -1;
	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_ge_register_device);

void rtscam_ge_unregister_device(struct rtscam_ge_device *rdev)
{
	if (!rdev || !rtscam_ge_is_registered(rdev))
		return;

	mutex_lock(&rtscamdev_lock);
	clear_bit(RTSCAM_FL_REGISTERED, &rdev->flags);
	mutex_unlock(&rtscamdev_lock);

	device_unregister(&rdev->dev);
}
EXPORT_SYMBOL_GPL(rtscam_ge_unregister_device);

static int __init rtscam_dev_init(void)
{
	dev_t devno = MKDEV(RTSCAM_DEV_MAJOR, 0);
	int ret;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	ret = register_chrdev_region(devno,
				     RTSCAM_NUM_DEVICES, RTSCAM_DEV_NAME);

	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "rtscam_dev:unable to get major %d\n",
			  RTSCAM_DEV_MAJOR);
		return ret;
	}

	ret = class_register(&rtscam_ge_class);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "rtscam_dev:class_register fail\n");
		unregister_chrdev_region(devno, RTSCAM_NUM_DEVICES);
		return ret;
	}

	return 0;
}

static void __exit rtscam_dev_exit(void)
{
	dev_t devno = MKDEV(RTSCAM_DEV_MAJOR, 0);

	class_unregister(&rtscam_ge_class);
	unregister_chrdev_region(devno, RTSCAM_NUM_DEVICES);
}

subsys_initcall(rtscam_dev_init);
module_exit(rtscam_dev_exit);

MODULE_AUTHOR("Ming Qian <ming_qian@realsil.com.cn>");
MODULE_DESCRIPTION("Device registrar for rts_camera drivers");
MODULE_LICENSE("GPL v2");
