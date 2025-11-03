/*
 * f_dfu.c - USB DFU function driver
 *
 * Copyright (C) 2020 Rui Feng <rui_feng@realsil.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/poll.h>

#include "f_dfu.h"
#include "cfg_utils.h"

static struct class dfug_class = {
	.name = "usb_dfu_gadget",
};
static dev_t dfu_devno;

static int dfu_detach_flag;
static int dfu_intf_id;

static const struct dfu_function_descriptor dfu_func = {
	.bLength =		sizeof(dfu_func),
	.bDescriptorType =	DFU_DT_FUNC,
	.bmAttributes = DFU_BIT_MANIFESTATION_TOLERANT |
				DFU_BIT_CAN_DNLOAD | DFU_BIT_WILL_DETACH,
	.wDetachTimeOut =	2000,
	.wTransferSize =	DFU_USB_BUFSIZ,
	.bcdDFUVersion =	cpu_to_le16(0x0110),
};

static struct usb_interface_descriptor dfu_intf_runtime = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_APP_SPEC,
	.bInterfaceSubClass =	1,
	.bInterfaceProtocol =	1,
	/* .iInterface = DYNAMIC */
};

static struct usb_interface_descriptor dfu_intf_mode = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bAlternateSetting = 0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_APP_SPEC,
	.bInterfaceSubClass =	1,
	.bInterfaceProtocol =	2,
	/* .iInterface = DYNAMIC */
};

static struct usb_descriptor_header *dfu_runtime_descs[] = {
	(struct usb_descriptor_header *) &dfu_intf_runtime,
	(struct usb_descriptor_header *) &dfu_func,
	NULL,
};

static struct usb_descriptor_header *dfu_mode_descs[] = {
	(struct usb_descriptor_header *) &dfu_intf_mode,
	(struct usb_descriptor_header *) &dfu_func,
	NULL,
};

static char dfu_name[USB_MAX_STRING_LEN_WITH_NULL] = "Device Firmware Upgrade";

/*
 * static strings, in UTF-8
 *
 * dfu_generic configuration
 */
static struct usb_string strings_dfu_generic[1];

static struct usb_gadget_strings stringtab_dfu_generic = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dfu_generic,
};

static struct usb_gadget_strings *dfu_generic_strings[] = {
	&stringtab_dfu_generic,
	NULL,
};

static struct f_dfu_req_list *
dfu_req_alloc(unsigned int len, gfp_t gfp_flags)
{
	struct f_dfu_req_list *req_list;

	req_list = kzalloc(sizeof(*req_list), gfp_flags);
	if (!req_list)
		return NULL;

	req_list->buf = kmalloc(len, gfp_flags);
	if (req_list->buf == NULL) {
		kfree(req_list);
		return NULL;
	}

	return req_list;
}

static void dfu_req_free(struct f_dfu_req_list *req)
{
	if (req != NULL) {
		kfree(req->buf);
		kfree(req);
		req = NULL;
	}
}

static int rtsx_dfu_event_dequeue(struct dfu_fh *fh, struct dfu_event *event)
{
	struct dfu_kevent *kev;
	unsigned long flags;

	spin_lock_irqsave(&fh->fh_lock, flags);

	if (list_empty(&fh->available)) {
		spin_unlock_irqrestore(&fh->fh_lock, flags);
		return -ENOENT;
	}

	WARN_ON(fh->navailable == 0);

	kev = list_first_entry(&fh->available, struct dfu_kevent, list);
	list_del(&kev->list);
	fh->navailable--;

	*event = kev->event;
	kfree(kev);

	spin_unlock_irqrestore(&fh->fh_lock, flags);

	return 0;
}

static void __dfu_event_queue_fh(struct dfu_fh *fh, const struct dfu_event *ev)
{
	struct dfu_kevent *kev;

	/* Do we have any free events? */
	if (fh->navailable == GDFU_EVENT_MAX) {
		/* no, remove the oldest one */
		kev = list_first_entry(&fh->available, struct dfu_kevent, list);
		list_del_init(&kev->list);
		fh->navailable--;
	} else {
		kev = kmalloc(sizeof(struct dfu_kevent), GFP_ATOMIC);
		INIT_LIST_HEAD(&kev->list);
	}

	memcpy(&kev->event, ev, sizeof(*ev));
	list_add_tail(&kev->list, &fh->available);
	fh->navailable++;

	wake_up(&fh->wait);
}

static void rtsx_dfu_event_queue(struct dfu_dev *dev,
		const struct dfu_event *ev)
{
	unsigned long flags;

	if (dev == NULL)
		return;

	spin_lock_irqsave(&dev->fh.fh_lock, flags);
	__dfu_event_queue_fh(&dev->fh, ev);
	spin_unlock_irqrestore(&dev->fh.fh_lock, flags);
}

static int rtsx_dfu_open(struct inode *inode, struct file *fd)
{
	struct dfu_dev	*dev;
	unsigned long		flags;
	int			ret = -EBUSY;

	dev = container_of(inode->i_cdev, struct dfu_dev, dfu_cdev);

	spin_lock_irqsave(&dev->lock, flags);

	if (!dev->dfu_cdev_open) {
		dev->dfu_cdev_open = 1;
		fd->private_data = dev;
		ret = 0;
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	DBG(dev, "dfu_open returned %x\n", ret);
	return ret;
}

static int rtsx_dfu_release(struct inode *inode, struct file *fd)
{
	struct dfu_dev	*dev = fd->private_data;
	unsigned long		flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->dfu_cdev_open = 0;
	fd->private_data = NULL;

	spin_unlock_irqrestore(&dev->lock, flags);

	DBG(dev, "dfu_close\n");

	return 0;
}

static long rtsx_dfu_ioctl(struct file *fd,
			      unsigned int cmd, unsigned long arg)
{
	struct dfu_dev	*dev = fd->private_data;
	struct usb_composite_dev *cdev = dev->func.config->cdev;
	unsigned long		flags;
	struct dfu_event dfu_event;

	DBG(dev, "dfu_ioctl: cmd=0x%4.4x, arg=%lu\n", cmd, arg);

	spin_lock_irqsave(&dev->lock, flags);
	switch (cmd) {
	case DFU_IOC_SET_STATE:
		if (copy_from_user(&dev->dfu_appstate,
				(struct dfu_appstate *)arg,
				sizeof(struct dfu_appstate))) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return -EINVAL;
		}
		switch (dev->dfu_appstate.state) {
		case STATE_UPDATE_DONE:
			dev->dfu_status = DFU_STATUS_OK;
			break;
		case STATE_UPDATE_READY:
			dev->dfu_status = DFU_STATUS_OK;
			dev->dfu_state = DFU_STATE_dfuIDLE;
			break;
		default:
			dev->dfu_status = DFU_STATUS_errUNKNOWN;
			dev->dfu_state = DFU_STATE_dfuERROR;
			break;
		}
		break;
	case DFU_IOC_GET_STATE:
		if (copy_to_user((__u32 *)arg, &dev->dfu_state,
					sizeof(__u32))) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return -EINVAL;
		}
		break;
	case DFU_IOC_GET_EVENT:
		rtsx_dfu_event_dequeue(&dev->fh, &dfu_event);
		if (copy_to_user((struct dfu_event *)arg, &dfu_event,
				sizeof(struct dfu_event))) {
			spin_unlock_irqrestore(&dev->lock, flags);
			return -EINVAL;
		}
		break;
	case DFU_IOC_SOFT_RESET:
		if (cdev->gadget && cdev->gadget->ops->ioctl)
			cdev->gadget->ops->ioctl(cdev->gadget,
						DFU_IOC_SOFT_RESET, 0);
		break;
	default:
		spin_unlock_irqrestore(&dev->lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return 0;
}

static ssize_t rtsx_dfu_read(struct file *fd, char __user *buf,
			size_t len, loff_t *ptr)
{
	struct dfu_dev		*dev = fd->private_data;
	unsigned long			flags;
	size_t				size;
	size_t				bytes_copied;
	struct f_dfu_req_list		*req;
	/* This is a pointer to the current USB rx request. */
	struct f_dfu_req_list		*current_rx_req;
	/* This is the number of bytes in the current rx buffer. */
	size_t				current_rx_bytes;
	/* This is a pointer to the current rx buffer. */
	u8				*current_rx_buf;

	if (len == 0)
		return -EINVAL;

	DBG(dev, "dfu_read trying to read %d bytes\n", (int)len);

	mutex_lock(&dev->lock_dfu_io);
	spin_lock_irqsave(&dev->lock, flags);

	bytes_copied = 0;
	current_rx_req = dev->current_rx_req;
	current_rx_bytes = dev->current_rx_bytes;
	current_rx_buf = dev->current_rx_buf;
	dev->current_rx_req = NULL;
	dev->current_rx_bytes = 0;
	dev->current_rx_buf = NULL;

	/* Check if there is any data in the read buffers. Please note that
	 * current_rx_bytes is the number of bytes in the current rx buffer.
	 * If it is zero then check if there are any other rx_buffers that
	 * are on the completed list. We are only out of data if all rx
	 * buffers are empty.
	 */
	if ((current_rx_bytes == 0) &&
			(likely(list_empty(&dev->rx_buffers)))) {
		/* Turn interrupts back on before sleeping. */
		spin_unlock_irqrestore(&dev->lock, flags);

		/*
		 * If no data is available check if this is a NON-Blocking
		 * call or not.
		 */
		if (fd->f_flags & (O_NONBLOCK|O_NDELAY)) {
			mutex_unlock(&dev->lock_dfu_io);
			return -EAGAIN;
		}

		/* Sleep until data is available */
		wait_event_interruptible(dev->rx_wait,
				(likely(!list_empty(&dev->rx_buffers))));
		spin_lock_irqsave(&dev->lock, flags);
	}

	/* We have data to return then copy it to the caller's buffer.*/
	while ((current_rx_bytes || likely(!list_empty(&dev->rx_buffers)))
			&& len) {
		if (current_rx_bytes == 0) {
			req = container_of(dev->rx_buffers.next,
					struct f_dfu_req_list, list);
			list_del_init(&req->list);

			current_rx_req = req;
			current_rx_bytes = req->length;
			current_rx_buf = req->buf;
		}

		/* Don't leave irqs off while doing memory copies */
		spin_unlock_irqrestore(&dev->lock, flags);

		if (len > current_rx_bytes)
			size = current_rx_bytes;
		else
			size = len;

		size -= copy_to_user(buf, current_rx_buf, size);
		bytes_copied += size;
		len -= size;
		buf += size;

		spin_lock_irqsave(&dev->lock, flags);

		/* If we not returning all the data left in this RX request
		 * buffer then adjust the amount of data left in the buffer.
		 * Othewise if we are done with this RX request buffer then
		 * requeue it to get any incoming data from the USB host.
		 */
		if (size < current_rx_bytes) {
			current_rx_bytes -= size;
			current_rx_buf += size;
		} else {
			dfu_req_free(current_rx_req);
			dev->buf_n--;
			current_rx_bytes = 0;
			current_rx_buf = NULL;
			current_rx_req = NULL;
		}
	}

	dev->current_rx_req = current_rx_req;
	dev->current_rx_bytes = current_rx_bytes;
	dev->current_rx_buf = current_rx_buf;

	spin_unlock_irqrestore(&dev->lock, flags);
	mutex_unlock(&dev->lock_dfu_io);

	DBG(dev, "dfu_read returned %d bytes\n", (int)bytes_copied);

	if (bytes_copied)
		return bytes_copied;
	else
		return -EAGAIN;
}

static unsigned int
rtsx_dfu_poll(struct file *fd, poll_table *wait)
{
	struct dfu_dev	*dev = fd->private_data;
	unsigned long		flags;
	int			status = 0;

	poll_wait(fd, &dev->rx_wait, wait);
	poll_wait(fd, &dev->fh.wait, wait);

	spin_lock_irqsave(&dev->lock, flags);

	if (likely(dev->current_rx_bytes) ||
			likely(!list_empty(&dev->rx_buffers)))
		status |= POLLIN | POLLRDNORM;

	if (dev->fh.navailable)
		status |= POLLPRI;

	spin_unlock_irqrestore(&dev->lock, flags);

	return status;
}

static const struct file_operations rtsx_dfu_fops = {
	.owner		= THIS_MODULE,
	.open		= rtsx_dfu_open,
	.release	= rtsx_dfu_release,
	.read	= rtsx_dfu_read,
	.poll =		rtsx_dfu_poll,
	.unlocked_ioctl	= rtsx_dfu_ioctl,
};

static inline struct dfu_dev *func_to_dfu(struct usb_function *f)
{
	return container_of(f, struct dfu_dev, func);
}

static void gdfu_cleanup(void)
{
	if (dfu_devno)
		unregister_chrdev_region(dfu_devno, 1);

	class_unregister(&dfug_class);
}

static void f_dfu_free_inst(struct usb_function_instance *f)
{
	struct f_dfu_opts *opts;

	opts = container_of(f, struct f_dfu_opts, func_inst);
	mutex_destroy(&opts->lock);
	kfree(opts);

	gdfu_cleanup();
}

static int gdfu_setup(void)
{
	int status;

	status = class_register(&dfug_class);
	if (status) {
		pr_err("unable to create usb_gadget class %d\n", status);
		return status;
	}

	status = alloc_chrdev_region(&dfu_devno, 0, 1, "USB dfu gadget");
	if (status) {
		pr_err("alloc_chrdev_region %d\n", status);
		class_unregister(&dfug_class);
		return status;
	}

	return status;
}

static inline struct f_dfu_opts *to_f_dfu_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_dfu_opts,
			func_inst.group);
}

static void dfu_attr_release(struct config_item *item)
{
	struct f_dfu_opts *opts = to_f_dfu_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations dfu_item_ops = {
	.release	= dfu_attr_release,
};

F_STR_ATTR(dfu_opts, f_dfu_opts, iname);

static struct configfs_attribute *dfu_attrs[] = {
	&f_dfu_opts_attr_iname,
	NULL,
};
static struct config_item_type dfu_func_type = {
	.ct_item_ops	= &dfu_item_ops,
	.ct_attrs	= dfu_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct usb_function_instance *f_dfu_alloc_inst(void)
{
	int status = 0;
	struct f_dfu_opts		*opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&opts->lock);
	opts->iname = dfu_name;
	opts->func_inst.free_func_inst = f_dfu_free_inst;

	status = gdfu_setup();
	if (status) {
		kfree(opts);
		return ERR_PTR(status);
	}

	/* add configfs interface */
	config_group_init_type_name(&opts->func_inst.group, "",
			&dfu_func_type);

	return &opts->func_inst;
}

static void f_dfu_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct dfu_dev	*dev;
	struct f_dfu_req_list	*req;
	struct dfu_kevent		*kev;
	struct usb_composite_dev *cdev = c->cdev;

	dev = func_to_dfu(f);

	device_destroy(&dfug_class, dfu_devno);

	/* Remove Character Device */
	cdev_del(&dev->dfu_cdev);

	/* Free all memory for this driver. */

	if (dev->current_rx_req != NULL)
		dfu_req_free(dev->current_rx_req);

	while (!list_empty(&dev->rx_buffers)) {
		req = container_of(dev->rx_buffers.next,
				struct f_dfu_req_list, list);
		list_del(&req->list);
		dfu_req_free(req);
	}
	dev->buf_n = 0;
	while (!list_empty(&dev->fh.available)) {
		kev = container_of(dev->fh.available.next, struct dfu_kevent,
				list);
		list_del(&kev->list);
		kfree(kev);
	}

	usb_free_all_descriptors(f);
	cdev->get_dfu_interface_id = NULL;
}

static void dfu_set_poll_timeout(struct dfu_status *dstat, unsigned int ms)
{
	/*
	 * The bwPollTimeout DFU_GETSTATUS request payload provides information
	 * about minimum time, in milliseconds, that the host should wait before
	 * sending a subsequent DFU_GETSTATUS request
	 *
	 * This permits the device to vary the delay depending on its need to
	 * erase or program the memory
	 *
	 */

	unsigned char *p = (unsigned char *)&ms;

	if (!ms || (ms & ~DFU_POLL_TIMEOUT_MASK)) {
		dstat->bwPollTimeout[0] = 0;
		dstat->bwPollTimeout[1] = 0;
		dstat->bwPollTimeout[2] = 0;

		return;
	}

	dstat->bwPollTimeout[0] = *p++;
	dstat->bwPollTimeout[1] = *p++;
	dstat->bwPollTimeout[2] = *p;
}

/*-------------------------------------------------------------------------*/

static void dnload_request_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct dfu_dev	*dev = req->context;
	int			status = req->status;
	unsigned long		flags;
	struct f_dfu_req_list *req_list;

	switch (status) {

	/* normal completion */
	case 0:
		if (req->actual > 0) {
			spin_lock_irqsave(&dev->lock, flags);
			/* Do we have any free buf? */
			if (dev->buf_n == dev->blen) {
				dev->dfu_status = DFU_STATUS_errFIRMWARE;
				dev->dfu_state = DFU_STATE_dfuERROR;
				spin_unlock_irqrestore(&dev->lock, flags);
				return;
			}

			req_list = dfu_req_alloc(DFU_USB_BUFSIZ, GFP_ATOMIC);
			if (!req_list) {
				spin_unlock_irqrestore(&dev->lock, flags);
				return;
			}

			memcpy(req_list->buf, req->buf, req->actual);
			req_list->length = req->actual;
			req_list->blk_seq_num = dev->blk_seq_num;

			list_add_tail(&req_list->list, &dev->rx_buffers);
			dev->buf_n++;
			spin_unlock_irqrestore(&dev->lock, flags);
			wake_up_interruptible(&dev->rx_wait);
		}
		break;

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		VDBG(dev, "rx shutdown, code %d\n", status);
		break;

	/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		DBG(dev, "rx %s reset\n", ep->name);
		break;

	/* data overrun */
	case -EOVERFLOW:
		/* FALLTHROUGH */

	default:
		DBG(dev, "rx status %d\n", status);
		break;
	}

	if (status) {
		dev->dfu_status = DFU_STATUS_errUNKNOWN;
		dev->dfu_state = DFU_STATE_dfuERROR;
	}
}

static void handle_getstatus(struct dfu_dev *f_dfu, struct usb_request *req)
{
	struct dfu_status *dstat = (struct dfu_status *)req->buf;
	unsigned int                    poll_timeout = 0;

	dfu_set_poll_timeout(dstat, 0);

	switch (f_dfu->dfu_state) {
	case DFU_STATE_dfuDNLOAD_SYNC:
	case DFU_STATE_dfuDNBUSY:
		f_dfu->dfu_state = DFU_STATE_dfuDNLOAD_IDLE;
		poll_timeout = f_dfu->poll_timeout;
		break;
	case DFU_STATE_dfuMANIFEST_SYNC:
		if (f_dfu->dfu_appstate.state == STATE_UPDATE_DONE)
			f_dfu->dfu_state = DFU_STATE_dfuMANIFEST;
		poll_timeout = f_dfu->poll_timeout;
		break;
	case DFU_STATE_dfuMANIFEST:
		poll_timeout = DFU_MANIFEST_POLL_TIMEOUT;
		break;
	default:
		break;
	}

	if (poll_timeout)
		dfu_set_poll_timeout(dstat, poll_timeout);

	/* send status response */
	dstat->bStatus = f_dfu->dfu_status;
	dstat->bState = f_dfu->dfu_state;
	dstat->iString = 0;
}

static void handle_getstate(struct dfu_dev *f_dfu, struct usb_request *req)
{
	((u8 *)req->buf)[0] = f_dfu->dfu_state;
	req->actual = sizeof(u8);
}

static int handle_dnload(struct usb_gadget *gadget,
		struct dfu_dev *f_dfu, u16 len)
{
	struct usb_composite_dev *cdev = get_gadget_data(gadget);
	struct usb_request *req = cdev->req;

	if (len == 0)
		f_dfu->dfu_state = DFU_STATE_dfuMANIFEST_SYNC;

	req->context = f_dfu;
	req->complete = dnload_request_complete;

	return len;
}

/*-------------------------------------------------------------------------*/
/* DFU state machine  */
static int state_app_idle(struct dfu_dev *f_dfu,
			  const struct usb_ctrlrequest *ctrl,
			  struct usb_gadget *gadget,
			  struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	case USB_REQ_DFU_DETACH:
		f_dfu->dfu_state = DFU_STATE_appDETACH;
		value = RET_ZLP;
		break;
	default:
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_app_detach(struct dfu_dev *f_dfu,
			    const struct usb_ctrlrequest *ctrl,
			    struct usb_gadget *gadget,
			    struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_appIDLE;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_idle(struct dfu_dev *f_dfu,
			  const struct usb_ctrlrequest *ctrl,
			  struct usb_gadget *gadget,
			  struct usb_request *req)
{
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 len = le16_to_cpu(ctrl->wLength);
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_DNLOAD:
		if (len == 0) {
			f_dfu->dfu_state = DFU_STATE_dfuERROR;
			value = RET_STALL;
			break;
		}
		f_dfu->dfu_state = DFU_STATE_dfuDNLOAD_SYNC;
		f_dfu->blk_seq_num = w_value;
		value = handle_dnload(gadget, f_dfu, len);
		break;
	case USB_REQ_DFU_ABORT:
		/* no zlp? */
		value = RET_ZLP;
		break;
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	case USB_REQ_DFU_DETACH:
		/*
		 * Proprietary extension: 'detach' from idle mode and
		 * get back to runtime mode in case of USB Reset.  As
		 * much as I dislike this, we just can't use every USB
		 * bus reset to switch back to runtime mode, since at
		 * least the Linux USB stack likes to send a number of
		 * resets in a row :(
		 */
		f_dfu->dfu_state = DFU_STATE_appIDLE;
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_dnload_sync(struct dfu_dev *f_dfu,
				 const struct usb_ctrlrequest *ctrl,
				 struct usb_gadget *gadget,
				 struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_dnbusy(struct dfu_dev *f_dfu,
			    const struct usb_ctrlrequest *ctrl,
			    struct usb_gadget *gadget,
			    struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_dnload_idle(struct dfu_dev *f_dfu,
				 const struct usb_ctrlrequest *ctrl,
				 struct usb_gadget *gadget,
				 struct usb_request *req)
{
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 len = le16_to_cpu(ctrl->wLength);
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_DNLOAD:
		f_dfu->dfu_state = DFU_STATE_dfuDNLOAD_SYNC;
		f_dfu->blk_seq_num = w_value;
		value = handle_dnload(gadget, f_dfu, len);
		break;
	case USB_REQ_DFU_ABORT:
		f_dfu->dfu_state = DFU_STATE_dfuIDLE;
		value = RET_ZLP;
		break;
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_manifest_sync(struct dfu_dev *f_dfu,
				   const struct usb_ctrlrequest *ctrl,
				   struct usb_gadget *gadget,
				   struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		/* We're MainfestationTolerant */
		handle_getstatus(f_dfu, req);
		f_dfu->blk_seq_num = 0;
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

static int state_dfu_manifest(struct dfu_dev *f_dfu,
			      const struct usb_ctrlrequest *ctrl,
			      struct usb_gadget *gadget,
			      struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		/* We're MainfestationTolerant */
		f_dfu->dfu_state = DFU_STATE_dfuIDLE;
		handle_getstatus(f_dfu, req);
		f_dfu->blk_seq_num = 0;
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}
	return value;
}

static int state_dfu_error(struct dfu_dev *f_dfu,
				 const struct usb_ctrlrequest *ctrl,
				 struct usb_gadget *gadget,
				 struct usb_request *req)
{
	int value = 0;

	switch (ctrl->bRequest) {
	case USB_REQ_DFU_GETSTATUS:
		handle_getstatus(f_dfu, req);
		value = RET_STAT_LEN;
		break;
	case USB_REQ_DFU_GETSTATE:
		handle_getstate(f_dfu, req);
		break;
	case USB_REQ_DFU_CLRSTATUS:
		f_dfu->dfu_state = DFU_STATE_dfuIDLE;
		f_dfu->dfu_status = DFU_STATUS_OK;
		/* no zlp? */
		value = RET_ZLP;
		break;
	default:
		f_dfu->dfu_state = DFU_STATE_dfuERROR;
		value = RET_STALL;
		break;
	}

	return value;
}

typedef int (*dfu_state_fn) (struct dfu_dev *,
			     const struct usb_ctrlrequest *,
			     struct usb_gadget *,
			     struct usb_request *);

static dfu_state_fn dfu_state[] = {
	state_app_idle,          /* DFU_STATE_appIDLE */
	state_app_detach,        /* DFU_STATE_appDETACH */
	state_dfu_idle,          /* DFU_STATE_dfuIDLE */
	state_dfu_dnload_sync,   /* DFU_STATE_dfuDNLOAD_SYNC */
	state_dfu_dnbusy,        /* DFU_STATE_dfuDNBUSY */
	state_dfu_dnload_idle,   /* DFU_STATE_dfuDNLOAD_IDLE */
	state_dfu_manifest_sync, /* DFU_STATE_dfuMANIFEST_SYNC */
	state_dfu_manifest,	 /* DFU_STATE_dfuMANIFEST */
	NULL,                    /* DFU_STATE_dfuMANIFEST_WAIT_RST */
	NULL,   /* DFU_STATE_dfuUPLOAD_IDLE */
	state_dfu_error          /* DFU_STATE_dfuERROR */
};

/*-------------------------------------------------------------------------*/

static int
f_dfu_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_gadget *gadget = f->config->cdev->gadget;
	struct usb_request *req = f->config->cdev->req;
	struct dfu_dev				*f_dfu = func_to_dfu(f);
	u16 len = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	int value = 0;
	u8 req_type = ctrl->bRequestType & USB_TYPE_MASK;
	struct dfu_event	event;

	DBG(f_dfu, "w_value: 0x%x len: 0x%x\n", w_value, len);
	DBG(f_dfu, "req_type: 0x%x ctrl->bRequest: 0x%x f_dfu->dfu_state: 0x%x\n",
	       req_type, ctrl->bRequest, f_dfu->dfu_state);

	if (req_type == USB_TYPE_STANDARD) {
		if (ctrl->bRequest == USB_REQ_GET_DESCRIPTOR &&
		    (w_value >> 8) == DFU_DT_FUNC) {
			value = min_t(u16, len, (u16) sizeof(dfu_func));
			memcpy(req->buf, &dfu_func, value);
		}
	} else /* DFU specific request */
		value = dfu_state[f_dfu->dfu_state] (f_dfu, ctrl, gadget, req);

	if (value >= 0) {
		req->length = value;
		req->zero = value < len;
		value = usb_ep_queue(gadget->ep0, req, 0);
		if (value < 0) {
			DBG(f_dfu, "ep_queue --> %d\n", value);
			req->status = 0;
		}
	}
	if (ctrl->bRequest == USB_REQ_DFU_DETACH) {
		dfu_detach_flag = 1 - dfu_detach_flag;
		event.type = DFU_EVENT_DETACH;
		rtsx_dfu_event_queue(f_dfu, &event);
	}

	return value;
}

static void f_dfu_free(struct usb_function *f)
{
	struct dfu_dev *dfu;
	struct f_dfu_opts	*opts;

	opts = container_of(f->fi, struct f_dfu_opts, func_inst);
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);

	dfu = func_to_dfu(f);
	kfree(dfu);
}

/* TODO: what we need here? */
static void f_dfu_disable(struct usb_function *f)
{
}

static int f_dfu_set_alt(struct usb_function *f, unsigned int intf,
				unsigned int alt)
{
	struct usb_composite_dev		*cdev = f->config->cdev;
	struct dfu_dev				*dev = func_to_dfu(f);

	VDBG(cdev, "dfu_set_alt intf:%d alt:%d\n", intf, alt);

	dev->dfu_status = DFU_STATUS_OK;

	return 0;
}

static int get_dfu_interface_id(void)
{
	return dfu_intf_id;
}

static int f_dfu_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct dfu_dev		*dfu = func_to_dfu(f);
	int id;
	struct device *pdev;
	struct usb_string	*us;

	cdev->b_vendor_code = MS_VendorCode;
	strings_dfu_generic[0].s = dfu_name;
	us = usb_gstrings_attach(c->cdev, dfu_generic_strings,
		ARRAY_SIZE(strings_dfu_generic));
	if (IS_ERR(us))
		return PTR_ERR(us);
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	if (dfu_detach_flag) {
		dfu_intf_mode.bInterfaceNumber = id;
		dfu->dfu_state = DFU_STATE_appDETACH;
	} else {
		dfu_intf_runtime.bInterfaceNumber = id;
		dfu->dfu_state = DFU_STATE_appIDLE;
	}
	dfu_intf_id = id;
	dfu->dfu_status = DFU_STATUS_OK;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dfu_generic[0].id = id;
	if (dfu_detach_flag) {
		dfu_intf_mode.iInterface = id;
		id = usb_assign_descriptors(f, dfu_mode_descs,
					    dfu_mode_descs, NULL, NULL);
	} else {
		dfu_intf_runtime.iInterface = id;
		id = usb_assign_descriptors(f, dfu_runtime_descs,
					    dfu_runtime_descs, NULL, NULL);
	}

	pdev = device_create(&dfug_class, NULL, dfu_devno,
				  NULL, "%s", "dfu");
	if (IS_ERR(pdev)) {
		ERROR(dfu, "Failed to create device: g_dfu\n");
		id = PTR_ERR(pdev);
		goto error;
	}

	cdev_init(&dfu->dfu_cdev, &rtsx_dfu_fops);
	dfu->dfu_cdev.owner = THIS_MODULE;
	id = cdev_add(&dfu->dfu_cdev, dfu_devno, 1);
	if (id) {
		ERROR(dfu, "Failed to open char device\n");
		goto fail_cdev_add;
	}
	cdev->get_dfu_interface_id = get_dfu_interface_id;

	return 0;

fail_cdev_add:
	device_destroy(&dfug_class, dfu_devno);

error:
	return id;
}

static struct usb_function *f_dfu_alloc(struct usb_function_instance *fi)
{
	struct dfu_dev	*dfu;
	struct f_dfu_opts *opts;

	/* allocate and initialize one new instance */
	dfu = kzalloc(sizeof(*dfu), GFP_KERNEL);
	if (!dfu)
		return ERR_PTR(-ENOMEM);

	dfu->blen = DFU_BLEN;

	dfu->func.name = "dfu";
	dfu->func.bind = f_dfu_bind;
	dfu->func.unbind = f_dfu_unbind;
	dfu->func.set_alt = f_dfu_set_alt;
	dfu->func.setup = f_dfu_setup;
	dfu->func.disable = f_dfu_disable;
	dfu->func.free_func = f_dfu_free;
	dfu->func.strings = dfu_generic_strings;
	dfu->poll_timeout = DFU_DEFAULT_POLL_TIMEOUT;

	INIT_LIST_HEAD(&dfu->rx_buffers);
	spin_lock_init(&dfu->lock);
	mutex_init(&dfu->lock_dfu_io);
	init_waitqueue_head(&dfu->rx_wait);
	dfu->dfu_cdev_open = 0;
	dfu->current_rx_req = NULL;
	dfu->current_rx_bytes = 0;
	dfu->current_rx_buf = NULL;

	spin_lock_init(&dfu->fh.fh_lock);
	init_waitqueue_head(&dfu->fh.wait);
	INIT_LIST_HEAD(&dfu->fh.available);
	dfu->fh.dev = dfu;

	opts = container_of(fi, struct f_dfu_opts, func_inst);
	mutex_lock(&opts->lock);
	++opts->refcnt;
	mutex_unlock(&opts->lock);

	return &dfu->func;
}

DECLARE_USB_FUNCTION_INIT(dfu, f_dfu_alloc_inst, f_dfu_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rui Feng");
