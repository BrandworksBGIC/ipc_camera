// SPDX-License-Identifier: GPL-2.0+
/*
 *	uvc_gadget.c  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/g_uvc.h>
#include <linux/usb/video.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#include "f_uvc.h"
#include "u_uvc.h"

#include "uvc.h"
#include "uvc_configfs.h"
#include "uvc_v4l2.h"
#include "uvc_video.h"
#include "android_uvc.h"

unsigned int uvc_gadget_trace_param;
module_param_named(trace, uvc_gadget_trace_param, uint, 0644);
MODULE_PARM_DESC(trace, "Trace level bitmask");

static int inactive_streams[UVC_STREAMING_NUMS];
static int init;
/* --------------------------------------------------------------------------
 * Function descriptors
 */

/* string IDs are assigned dynamically */

static struct usb_string uvc_en_us_strings[] = {
	/* [UVC_STRING_CONTROL_IDX].s = DYNAMIC, */
	[UVC_STRING_STREAMING_IDX].s = "Video Streaming",
	{  }
};

static struct usb_gadget_strings uvc_stringtab = {
	.language = 0x0409,	/* en-us */
	.strings = uvc_en_us_strings,
};

static struct usb_gadget_strings *uvc_function_strings[] = {
	&uvc_stringtab,
	NULL,
};

#define UVC_INTF_VIDEO_CONTROL			0
#define UVC_INTF_VIDEO_STREAMING		1

#define UVC_STATUS_MAX_PACKET_SIZE		16	/* 16 bytes status */

static struct usb_interface_assoc_descriptor uvc_iad = {
	.bLength		= sizeof(uvc_iad),
	.bDescriptorType	= USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface	= 0,
	.bInterfaceCount	= 1 + UVC_STREAMING_NUMS,
	.bFunctionClass		= USB_CLASS_VIDEO,
	.bFunctionSubClass	= UVC_SC_VIDEO_INTERFACE_COLLECTION,
	.bFunctionProtocol	= 0x00,
	.iFunction		= 0,
};

static struct usb_interface_descriptor uvc_control_intf = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= UVC_INTF_VIDEO_CONTROL,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 0,
	.bInterfaceClass	= USB_CLASS_VIDEO,
	.bInterfaceSubClass	= UVC_SC_VIDEOCONTROL,
#ifndef CONFIG_USB_RTSX_UVC_15
	.bInterfaceProtocol	= 0x00,
#else
	.bInterfaceProtocol	= 0x01,
#endif
	.iInterface		= 0,
};

static struct usb_endpoint_descriptor uvc_interrupt_ep = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
	.bInterval		= 8,
};

static struct usb_ss_ep_comp_descriptor uvc_ss_interrupt_comp = {
	.bLength		= sizeof(uvc_ss_interrupt_comp),
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	/* The following 3 values can be tweaked if necessary. */
	.bMaxBurst		= 0,
	.bmAttributes		= 0,
	.wBytesPerInterval	= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
};

static struct uvc_control_endpoint_descriptor uvc_interrupt_cs_ep = {
	.bLength		= UVC_DT_CONTROL_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_CS_ENDPOINT,
	.bDescriptorSubType	= UVC_EP_INTERRUPT,
	.wMaxTransferSize	= cpu_to_le16(UVC_STATUS_MAX_PACKET_SIZE),
};


#define ALT_SETTINGS_MAX		6
static u16 transfer_size[ALT_SETTINGS_MAX - 2] = {0x3FC, 0x300, 0x400, 0x200};
static u8 pid[ALT_SETTINGS_MAX - 2] = {2, 2, 1, 1};

static struct usb_interface_descriptor
	uvc_streaming_intfs[UVC_STREAMING_NUMS][ALT_SETTINGS_MAX] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		[0] = { /* stream0 alt0 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 0,
			.bNumEndpoints		= 0,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
		[1] = { /* stream0 alt1 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 1,
			.bNumEndpoints		= 1,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
		[2] = { /* stream0 alt2 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 2,
			.bNumEndpoints		= 1,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
		[3] = { /* stream0 alt3 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 3,
			.bNumEndpoints		= 1,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
		[4] = { /* stream0 alt4 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 4,
			.bNumEndpoints		= 1,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
		[5] = { /* stream0 alt5 */
			.bLength		= USB_DT_INTERFACE_SIZE,
			.bDescriptorType	= USB_DT_INTERFACE,
			.bInterfaceNumber	= 0, /* dynamic */
			.bAlternateSetting	= 5,
			.bNumEndpoints		= 1,
			.bInterfaceClass	= USB_CLASS_VIDEO,
			.bInterfaceSubClass	= UVC_SC_VIDEOSTREAMING,
		#ifndef CONFIG_USB_RTSX_UVC_15
			.bInterfaceProtocol	= 0x00,
		#else
			.bInterfaceProtocol	= 0x01,
		#endif
			.iInterface		= 0,
		},
	},
};

#define FS_IDX		0
#define HS_IDX		(ALT_SETTINGS_MAX - 1)
#define SS_IDX		(2 * (ALT_SETTINGS_MAX - 1))
#define SPEED_MAX	(3 * (ALT_SETTINGS_MAX - 1))

static struct usb_endpoint_descriptor
	uvc_streaming_eps[UVC_STREAMING_NUMS][SPEED_MAX] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		[FS_IDX ... FS_IDX + ALT_SETTINGS_MAX - 2] = {
			.bLength		= USB_DT_ENDPOINT_SIZE,
			.bDescriptorType	= USB_DT_ENDPOINT,
			.bEndpointAddress	= USB_DIR_IN,
			.bmAttributes		= USB_ENDPOINT_SYNC_ASYNC
						| USB_ENDPOINT_XFER_ISOC,
			.bInterval		= 1,
			/* The wMaxPacketSize and bInterval values will be initialized from
			 * module parameters.
			 */
		},
		[HS_IDX ... HS_IDX + ALT_SETTINGS_MAX - 2] = {
			.bLength		= USB_DT_ENDPOINT_SIZE,
			.bDescriptorType	= USB_DT_ENDPOINT,
			.bEndpointAddress	= USB_DIR_IN,
			.bmAttributes		= USB_ENDPOINT_SYNC_ASYNC
						| USB_ENDPOINT_XFER_ISOC,
			.bInterval		= 1,
			/* The wMaxPacketSize and bInterval values will be initialized from
			 * module parameters.
			 */
		},
		[SS_IDX ... SS_IDX + ALT_SETTINGS_MAX - 2] = {
			.bLength		= USB_DT_ENDPOINT_SIZE,
			.bDescriptorType	= USB_DT_ENDPOINT,
			.bEndpointAddress	= USB_DIR_IN,
			.bmAttributes		= USB_ENDPOINT_SYNC_ASYNC
						| USB_ENDPOINT_XFER_ISOC,
			.bInterval		= 1,
		},
	},
};

static struct usb_ss_ep_comp_descriptor uvc_ss_streaming_comp = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	/*
	 * The bMaxBurst, bmAttributes and wBytesPerInterval values will be
	 * initialized from module parameters.
	 */
};

static int outdatastall;

/* --------------------------------------------------------------------------
 * Control requests
 */

static void
uvc_function_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct uvc_device *uvc = req->context;
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;

	if (uvc->event_setup_out) {
		uvc->event_setup_out = 0;

		memset(&v4l2_event, 0, sizeof(v4l2_event));
		v4l2_event.type = UVC_EVENT_DATA;
		uvc_event->data.length = min_t(unsigned int, req->actual,
			sizeof(v4l2_event.u.data) - (sizeof(uvc_event->data)
				- sizeof(uvc_event->data.data)));
		memcpy(&uvc_event->data.data, req->buf, uvc_event->data.length);
		v4l2_event_queue(&uvc->vdev[0], &v4l2_event);
	}
}

static void
uvc_function_epc_complete(struct usb_ep *ep, struct usb_request *req)
{
}

static int __get_stream_idx_from_ep(struct uvc_device *uvc, __u8 ep)
{
	int i;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i)
		if (!inactive_streams[i] &&
			ep == (uvc->video[i].ep->address & 0x7f))
			return i;

	return -1;
}

static int __get_stream_idx(struct uvc_device *uvc, __u8 interface)
{
	int i;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i)
		if (!inactive_streams[i] &&
			interface == uvc->streaming_intfs[i])
			return i;

	return -1;
}
/*
static void notify_stream_off(struct video_device *vdev)
{
	struct v4l2_event v4l2_event = {0};

	v4l2_event.type = UVC_EVENT_STREAMOFF;
	v4l2_event_queue(vdev, &v4l2_event);
}

static int handle_clear_feature(__u8 endpoint, struct usb_function *f)
{
	int stream;
	struct uvc_device *uvc = to_uvc(f);

	stream = __get_stream_idx_from_ep(uvc, endpoint);
	if (stream == -1)
		return -1;

	if (uvc->state[stream] == UVC_STATE_DISCONNECTED)
		return 0;

	if (uvc->state[stream] == UVC_STATE_STREAMING) {
*/		/* windows & linux issues clearfeature to notify stream off */
/*		rlxprintk(RLX_TRACE_STATUS, "%s change %d to stop\n",
				__func__, uvc->state[stream]);
		uvc->state[stream] = UVC_STATE_STOPPING;
		notify_stream_off(&uvc->vdev[stream]);
	} else {
*/		/* windows & linux issues clearfeature when ep not response */
/*		usb_ep_enable(uvc->video[stream].ep);
	}

	return 0;
}
*/
static int __is_streaming_interface(struct uvc_device *uvc, int intf)
{
	int i;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i])
			continue;

		if (uvc->streaming_intfs[i] == intf)
			return 1;
	}

	return 0;
}

static int _is_streaming_commit(struct uvc_device *uvc,
		const struct usb_ctrlrequest *ctrl)
{
	__u8 cs, entity, interface;

	if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS)
		return 0;

	interface = (__u8)(ctrl->wIndex && 0x7f);
	if (!__is_streaming_interface(uvc, interface))
		return 0;

	entity = (__u8)(ctrl->wIndex >> 8);
	if (entity != ENT_ID_STREAMING_INTERFACE)
		return 0;

	cs = (__u8)(ctrl->wValue >> 8);
	if (cs != UVC_VS_COMMIT_CONTROL)
		return 0;

	if (ctrl->bRequest != UVC_SET_CUR)
		return 0;

	return 1;
}

static void _enable_endpoint(struct uvc_device *uvc,
		const struct usb_ctrlrequest *ctrl)
{
	__u8 endpoint;
	int stream;

	endpoint = (__u8)ctrl->wIndex & 0x7f;
	stream = __get_stream_idx_from_ep(uvc, endpoint);
	if (stream == -1)
		return;

	uvc->video[stream].flag = 1;
	usb_ep_enable(uvc->video[stream].ep);
}

static int
uvc_function_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	//__u8 endpoint;
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	unsigned int interface = le16_to_cpu(ctrl->wIndex) & 0xff;
	struct usb_ctrlrequest *mctrl;

	rlxprintk(RLX_TRACE_DEBUG,
		"type:0x%x,request:0x%x,idx:0x%x,val:0x%x,len:0x%x\n",
		ctrl->bRequestType, ctrl->bRequest, ctrl->wIndex,
		ctrl->wValue, ctrl->wLength);
	//if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS) {
	/*if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD &&
	     ctrl->bRequest == USB_REQ_CLEAR_FEATURE) {
		rlxprintk(RLX_TRACE_DEBUG, "setup USB_REQ_CLEAR_FEATURE\n");
		endpoint = (__u8)ctrl->wIndex & 0x7f;
		handle_clear_feature(endpoint, f);
		return 0;
	}
	deal clear feature in composite
	*/
	if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS &&
	    (ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_VENDOR) {
		uvcg_info(f, "invalid request type\n");
		return -EINVAL;
	}

	/* Since android does not issues clearfeature when ep is disabled,
	 * so we need to enable ep after probe&commit, IN token is always
	 * follow the commit.
	 */
	if (_is_streaming_commit(uvc, ctrl))
		_enable_endpoint(uvc, ctrl);
	/* Stall too big requests. */
	if (le16_to_cpu(ctrl->wLength) > UVC_MAX_REQUEST_SIZE)
		return -EINVAL;

	/*
	 * Tell the complete callback to generate an event for the next request
	 * that will be enqueued by UVCIOC_SEND_RESPONSE.
	 */
	uvc->event_setup_out = !(ctrl->bRequestType & USB_DIR_IN);
	uvc->event_length = le16_to_cpu(ctrl->wLength);

	memset(&v4l2_event, 0, sizeof(v4l2_event));
	v4l2_event.type = UVC_EVENT_SETUP;
	memcpy(&uvc_event->req, ctrl, sizeof(uvc_event->req));

	/* check for the interface number, fixup the interface number in
	 * the ctrl request so the userspace doesn't have to bother with
	 * offset and configfs parsing
	 */
	mctrl = &uvc_event->req;

	mctrl->wIndex &= ~cpu_to_le16(0xff);
	if (__is_streaming_interface(uvc, interface))
		mctrl->wIndex = cpu_to_le16(interface);

	v4l2_event_queue(&uvc->vdev[0], &v4l2_event);

	return 0;
}
int uvc_outdata_setstall(void)
{
	outdatastall = 1;
	return 0;
}
EXPORT_SYMBOL_GPL(uvc_outdata_setstall);

static int
uvc_function_filteroutdata(struct usb_function *f,
			   const struct usb_ctrlrequest *ctrl,
			   unsigned char *buf, int len)
{
	unsigned char brequesttype;
	unsigned char brequest;
	unsigned short wvalue;
	unsigned short windex;
	unsigned short wlength;
	unsigned char wvalue_h;
	unsigned char wvalue_l;
	unsigned char windex_h;
	unsigned char windex_l;

	if (!outdatastall)
		return 0;

	brequesttype = ctrl->bRequestType;
	brequest = ctrl->bRequest;
	wvalue = ctrl->wValue;
	windex = ctrl->wIndex;
	wlength = ctrl->wLength;
	wvalue_h = ((char *)&(ctrl->wValue))[1];
	wvalue_l = ((char *)&(ctrl->wValue))[0];
	windex_h = ((char *)&(ctrl->wIndex))[1];
	windex_l = ((char *)&(ctrl->wIndex))[0];

	if ((brequesttype == 0x82) && (brequest == 0x0c))
		return 0;
	if ((brequesttype & REQUEST_TYPE) != CLASS_REQUEST)
		return 0;
	if (brequest != SET_CUR)
		return 0;
	if (((brequesttype & 0x83) != 0x1) && ((brequesttype & 0x83) != 0x81))
		return 0;
	if (windex_l != IF_IDX_VIDEOCONTROL)
		return 0;

	outdatastall = 0;
	return -1;
}

void uvc_function_setup_continue(struct uvc_device *uvc)
{
	struct usb_composite_dev *cdev = uvc->func.config->cdev;

	usb_composite_setup_continue(cdev);
}

static int
uvc_function_get_alt(struct usb_function *f, unsigned interface)
{
	struct uvc_device *uvc = to_uvc(f);
	int stream;

	uvcg_info(f, "%s(%u)\n", __func__, interface);

	if (interface == uvc->control_intf)
		return 0;

	stream = __get_stream_idx(uvc, interface);
	if (stream != -1)
		return uvc->alt;

	return -EINVAL;
}

static int
uvc_function_set_alt(struct usb_function *f, unsigned interface, unsigned alt)
{
	struct uvc_device *uvc = to_uvc(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	int i, stream;
	int ret;

	uvcg_info(f, "%s(%u, %u)\n", __func__, interface, alt);

	if (interface == uvc->control_intf) {
		if (alt)
			return -EINVAL;

		if (uvc->enable_interrupt_ep) {
			uvcg_info(f, "reset UVC interrupt endpoint\n");
			usb_ep_disable(uvc->interrupt_ep);

			if (!uvc->interrupt_ep->desc)
				if (config_ep_by_speed(cdev->gadget, f,
						       uvc->interrupt_ep))
					return -EINVAL;

			usb_ep_enable(uvc->interrupt_ep);
		}

		for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
			if (!inactive_streams[i] &&
				uvc->state[i] == UVC_STATE_DISCONNECTED) {
				uvc->state[i] = UVC_STATE_CONNECTED;
				if (i)
					continue;

				memset(&v4l2_event, 0, sizeof(v4l2_event));
				v4l2_event.type = UVC_EVENT_CONNECT;
				uvc_event->speed = cdev->gadget->speed;
				v4l2_event_queue(&uvc->vdev[0], &v4l2_event);
			}
		}

		return 0;
	}

	stream = __get_stream_idx(uvc, interface);
	if (stream == -1)
		return -EINVAL;

	/* TODO
	if (usb_endpoint_xfer_bulk(&uvc->desc.vs_ep))
		return alt ? -EINVAL : 0;
	*/

	uvc->alt = alt;
	switch (alt) {
	case 0:
		if (uvc->state[stream] != UVC_STATE_STREAMING &&
			uvc->state[stream] != UVC_STATE_STARTING) {
			if (!uvc->video[stream].ep || (uvc->video[stream].ep
					&& !uvc->video[stream].ep->enabled)) {
				rlxprintk(RLX_TRACE_STATUS,
						"invalid off state:%d\n",
						uvc->state[stream]);
				return 0;
			}
		}

		if (uvc->video[stream].ep)
			usb_ep_disable(uvc->video[stream].ep);

		rlxprintk(RLX_TRACE_STATUS, "%s change %d to stop\n",
				__func__, uvc->state[stream]);
		uvc->state[stream] = UVC_STATE_STOPPING;
		memset(&v4l2_event, 0, sizeof(v4l2_event));
		v4l2_event.type = UVC_EVENT_STREAMOFF;
		v4l2_event_queue(&uvc->vdev[stream], &v4l2_event);
		return 0;

	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if (!uvc->video[stream].ep)
			return -EINVAL;

		if (uvc->state[stream] != UVC_STATE_STOPPING &&
				uvc->state[stream] != UVC_STATE_CONNECTED
				&& uvc->video[stream].ep->enabled) {
			rlxprintk(RLX_TRACE_STATUS,
					"invalid on state:%d\n",
					uvc->state[stream]);
			return 0;
		}

		uvcg_info(f, "reset UVC\n");
		usb_ep_disable(uvc->video[stream].ep);
		ret = config_ep_by_speed_and_alt_idx(cdev->gadget, f,
				uvc->video[stream].ep, alt, stream);
		if (ret)
			return ret;

		usb_ep_enable(uvc->video[stream].ep);

		rlxprintk(RLX_TRACE_STATUS, "%s change %d to start\n",
				__func__, uvc->state[stream]);
		uvc->state[stream] = UVC_STATE_STARTING;
		memset(&v4l2_event, 0, sizeof(v4l2_event));
		v4l2_event.type = UVC_EVENT_STREAMON;
		v4l2_event_queue(&uvc->vdev[stream], &v4l2_event);
		return 0;
	default:
		return -EINVAL;
	}
}

static void
uvc_function_disable(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct v4l2_event v4l2_event;
	int i;

	uvcg_info(f, "%s()\n", __func__);

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		memset(&v4l2_event, 0, sizeof(v4l2_event));
		v4l2_event.type = UVC_EVENT_DISCONNECT;
		v4l2_event_queue(&uvc->vdev[i], &v4l2_event);
		rlxprintk(RLX_TRACE_STATUS, "%s change %d to disconnect\n",
				__func__, uvc->state[i]);
		uvc->state[i] = UVC_STATE_DISCONNECTED;
		if (uvc->video[i].ep)
			usb_ep_disable(uvc->video[i].ep);
	}
	if (uvc->enable_interrupt_ep)
		usb_ep_disable(uvc->interrupt_ep);
	init = 0;
}

/* --------------------------------------------------------------------------
 * Connection / disconnection
 */

void
uvc_function_connect(struct uvc_device *uvc)
{
	int ret;

	if ((ret = usb_function_activate(&uvc->func)) < 0)
		uvcg_info(&uvc->func, "UVC connect failed with %d\n", ret);
}

void
uvc_function_disconnect(struct uvc_device *uvc)
{
	int ret;

	if ((ret = usb_function_deactivate(&uvc->func)) < 0)
		uvcg_info(&uvc->func, "UVC disconnect failed with %d\n", ret);
}

/* --------------------------------------------------------------------------
 * USB probe and disconnect
 */

static ssize_t function_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct uvc_device *uvc = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", uvc->func.fi->group.cg_item.ci_name);
}

static DEVICE_ATTR_RO(function_name);

static int
uvc_register_video(struct uvc_device *uvc)
{
	struct usb_composite_dev *cdev = uvc->func.config->cdev;
	int ret;
	int i;

	/* TODO reference counting. */
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		memset(&uvc->vdev[i], 0, sizeof(uvc->vdev[0]));
		uvc->vdev[i].v4l2_dev = &uvc->v4l2_dev;
		uvc->vdev[i].v4l2_dev->dev = &cdev->gadget->dev;
		uvc->vdev[i].fops = &uvc_v4l2_fops;
		uvc->vdev[i].ioctl_ops = &uvc_v4l2_ioctl_ops;
		uvc->vdev[i].release = video_device_release_empty;
		uvc->vdev[i].vfl_dir = VFL_DIR_TX;
		uvc->vdev[i].lock = &uvc->video[i].mutex;
		uvc->vdev[i].device_caps = V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING;
		strlcpy(uvc->vdev[i].name, cdev->gadget->name,
			sizeof(uvc->vdev[i].name));
		video_set_drvdata(&uvc->vdev[i], uvc);
		ret = video_register_device(&uvc->vdev[i],
			VFL_TYPE_VIDEO, STREAM_BASE_NUM + i);
		if (ret)
			return ret;
	}

	ret = device_create_file(&uvc->vdev[0].dev, &dev_attr_function_name);
	if (ret < 0) {
		video_unregister_device(&uvc->vdev[0]);
		return ret;
	}

	return 0;
}
#define UVC_COPY_DESCRIPTOR(mem, dst, desc) \
	do { \
		memcpy(mem, desc, (desc)->bLength); \
		*(dst)++ = mem; \
		mem += (desc)->bLength; \
	} while (0);

#define UVC_COPY_DESCRIPTORS(mem, dst, src) \
	do { \
		const struct usb_descriptor_header * const *__src; \
		for (__src = src; *__src; ++__src) { \
			memcpy(mem, *__src, (*__src)->bLength); \
			*dst++ = mem; \
			mem += (*__src)->bLength; \
		} \
	} while (0)

#define UVC_COPY_DESCRIPTORS_MAX(mem, dst, src, max) \
	do { \
		const struct usb_descriptor_header * const *__src; \
		for (__src = src; *__src; ++__src) { \
			memcpy(mem, *__src, (*__src)->bLength); \
			*dst++ = mem; \
			mem += (*__src)->bLength; \
			if (--max <= 0) \
				break; \
		} \
	} while (0)

#define UVC_COPY_XU_DESCRIPTOR(mem, dst, desc)					\
	do {									\
		*(dst)++ = mem;							\
		memcpy(mem, desc, 22); /* bLength to bNrInPins */		\
		mem += 22;							\
										\
		memcpy(mem, (desc)->baSourceID, (desc)->bNrInPins);		\
		mem += (desc)->bNrInPins;					\
										\
		memcpy(mem, &(desc)->bControlSize, 1);				\
		mem++;								\
										\
		memcpy(mem, (desc)->bmControls, (desc)->bControlSize);		\
		mem += (desc)->bControlSize;					\
										\
		memcpy(mem, &(desc)->iExtension, 1);				\
		mem++;								\
	} while (0)

static struct usb_descriptor_header **
uvc_copy_descriptors(struct uvc_device *uvc, enum usb_device_speed speed)
{
	struct uvc_input_header_descriptor *uvc_streaming_header;
	struct usb_interface_assoc_descriptor *uvc_iad_desc;
	struct uvc_header_descriptor *uvc_control_header;
	struct uvc_output_terminal_descriptor *uvc_output_terminal_desc;
	struct uvc_descriptor_header **uvc_control_desc;
	const struct uvc_descriptor_header * const *uvc_streaming_cls;
	struct usb_descriptor_header **src;
	struct usb_descriptor_header **dst;
	struct usb_descriptor_header **hdr;
	struct uvcg_extension *xu;
	unsigned int control_size;
	unsigned int streaming_size[UVC_STREAMING_NUMS] = {0};
	unsigned int n_desc;
	unsigned int bytes;
	void *mem;
	unsigned int n_strm_desc = 0;
	int i, j, speed_idx, ot_offset = 0;
#ifdef CONFIG_USB_CONFIGFS
	int num, max;
#endif

	switch (speed) {
	case USB_SPEED_SUPER:
		uvc_control_desc = uvc->desc.ss_control;
		uvc_streaming_cls = uvc->desc.ss_streaming;
		speed_idx = SS_IDX;
		break;

	case USB_SPEED_HIGH:
		uvc_control_desc = uvc->desc.fs_control;
		uvc_streaming_cls = uvc->desc.hs_streaming;
		speed_idx = HS_IDX;
		break;

	case USB_SPEED_FULL:
	default:
		uvc_control_desc = uvc->desc.fs_control;
		uvc_streaming_cls = uvc->desc.fs_streaming;
		speed_idx = FS_IDX;
		break;
	}

	/*
	 * Descriptors layout
	 *
	 * uvc_iad
	 * uvc_control_intf
	 * Class-specific UVC control descriptors
	 * uvc_interrupt_ep Standard VC Interrupt Endpoint Descriptor
	 * uvc_interrupt_cs_ep Class-specific VC Interrupt Endpoint Descriptor
	 * uvc_ss_interrupt_comp (for SS only)
	 * uvc_streaming_intf_alts
	 * Class-specific UVC streaming descriptors
	 * uvc_{fs|hs}_streaming
	 */

	/* Count descriptors and compute their size. */
	control_size = 0;
	uvc_iad.bInterfaceCount = 1 + uvc->active_streaming;
	bytes = uvc_iad.bLength + uvc_control_intf.bLength;

	n_desc = 2;
	if (uvc->enable_interrupt_ep) {
		bytes += uvc_interrupt_ep.bLength + uvc_interrupt_cs_ep.bLength;
		n_desc += 2;

		if (speed == USB_SPEED_SUPER) {
			bytes += uvc_ss_interrupt_comp.bLength;
			n_desc += 1;
		}
	}

	for (src = (struct usb_descriptor_header **)uvc_control_desc,
		i = 0; *src; ++src, ++i) {
		if (i < BASIC_ENTITY_NUMS) {
			if (i == 0)
				(*src)->bLength = UVC_DT_HEADER_CUSTOMED_SIZE
					- UVC_STREAMING_NUMS
					+ uvc->active_streaming;
			ot_offset += (*src)->bLength;
		} else if (inactive_streams[i - BASIC_ENTITY_NUMS]) {
			*src = NULL;
			break;
		}
		control_size += (*src)->bLength;
		bytes += (*src)->bLength;
		n_desc++;
	}

	list_for_each_entry(xu, uvc->desc.extension_units, list) {
		control_size += xu->desc.bLength;
		bytes += xu->desc.bLength;
		n_desc++;
	}

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i])
			continue;

		/* streaming interface desc */
		bytes += uvc_streaming_intfs[i][0].bLength;
		++n_desc;

		/* format, frame and color match desc */
#ifndef CONFIG_USB_CONFIGFS
		bytes += uvc_get_stream_descs_len(i);
		n_desc += uvc_get_stream_descs_num(i);
#else
		for (src = (struct usb_descriptor_header **)
				uvc_streaming_cls + n_strm_desc, num = 1;
				*src; ++src, ++num) {
			streaming_size[i] += (*src)->bLength;
			bytes += (*src)->bLength;
			n_desc++;
			if (num >= uvc->streaming_desc_num[i])
				break;
		}
		n_strm_desc += num;
#endif

		for (j = 0; j < ALT_SETTINGS_MAX - 1; j++) {
			/* streaming interface j desc */
			bytes += uvc_streaming_intfs[i][j + 1].bLength;
			++n_desc;

			/* endpoints desc */
			bytes += uvc_streaming_eps[i][speed_idx + j].bLength;
			n_desc++;
		}
		if (speed_idx == SS_IDX) {
			bytes += uvc_ss_streaming_comp.bLength;
			n_desc++;
		}
	}

	mem = kmalloc((n_desc + 1) * sizeof(*src) + bytes, GFP_KERNEL);
	if (mem == NULL)
		return NULL;

	hdr = mem;
	dst = mem;
	mem += (n_desc + 1) * sizeof(*src);

	/* Copy the descriptors. */
	uvc_iad_desc = mem;
	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_iad);
	UVC_COPY_DESCRIPTOR(mem, dst, &uvc_control_intf);

	uvc_control_header = mem;
	uvc_output_terminal_desc = mem + ot_offset;
	UVC_COPY_DESCRIPTORS(mem, dst,
		(const struct usb_descriptor_header **)uvc_control_desc);

	list_for_each_entry(xu, uvc->desc.extension_units, list) {
		UVC_COPY_XU_DESCRIPTOR(mem, dst, &xu->desc);
	}

	uvc_control_header->wTotalLength = cpu_to_le16(control_size);
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i])
			continue;

		uvc_control_header->bInCollection =
			uvc->active_streaming;
		uvc_control_header->baInterfaceNr[i] =
			uvc->streaming_intfs[i];
		uvc_output_terminal_desc[i].bTerminalID =
			ENT_ID_OUTPUT_TERMINAL + i;
	}

	if (uvc->enable_interrupt_ep) {
		UVC_COPY_DESCRIPTOR(mem, dst, &uvc_interrupt_ep);
		if (speed == USB_SPEED_SUPER)
			UVC_COPY_DESCRIPTOR(mem, dst, &uvc_ss_interrupt_comp);

		UVC_COPY_DESCRIPTOR(mem, dst, &uvc_interrupt_cs_ep);
	}

	n_strm_desc = 0;
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i] == 1)
			continue;

		/* copy streaming intf desc */
		UVC_COPY_DESCRIPTOR(mem, dst, &uvc_streaming_intfs[i][0]);

		/* copy streaming format, frame and color match desc */
		uvc_streaming_header = mem;

#ifndef CONFIG_USB_CONFIGFS
		streaming_size[i] = uvc_copy_stream_descs(i, &mem, &dst);
#else
		max = uvc->streaming_desc_num[i];
		UVC_COPY_DESCRIPTORS_MAX(mem, dst,
			(const struct usb_descriptor_header **)uvc_streaming_cls
			+ n_strm_desc, max);
		n_strm_desc += uvc->streaming_desc_num[i];
#endif
		uvc_streaming_header->wTotalLength =
			cpu_to_le16(streaming_size[i]);
		uvc_streaming_header->bEndpointAddress =
			uvc->video[i].ep->address;
		uvc_streaming_header->bTerminalLink =
			ENT_ID_OUTPUT_TERMINAL + i;
		for (j = 0; j < ALT_SETTINGS_MAX - 1; j++) {
			/* copy streaming intf desc */
			UVC_COPY_DESCRIPTOR(mem, dst,
					&uvc_streaming_intfs[i][j + 1]);

			/* copy streaming endpoint desc */
			UVC_COPY_DESCRIPTOR(mem, dst,
					&uvc_streaming_eps[i][speed_idx + j]);
		}
		if (speed_idx == SS_IDX)
			UVC_COPY_DESCRIPTOR(mem, dst,
					&uvc_ss_streaming_comp);
	}

	*dst = NULL;
	return hdr;
}

static struct usb_ep *usb_ep_config_ss(
	struct usb_gadget		*gadget,
	struct usb_endpoint_descriptor	*desc,
	struct usb_ss_ep_comp_descriptor *ep_comp
)
{
	struct usb_ep	*ep;

	ep = gadget_find_ep_by_name(gadget, "ep5in");
	if (ep && !ep->claimed)
		goto found_ep;

	ep = gadget_find_ep_by_name(gadget, "ep6in");
	if (ep && !ep->claimed)
		goto found_ep;

	/* Fail */
	return NULL;
found_ep:

	/*
	 * If the protocol driver hasn't yet decided on wMaxPacketSize
	 * and wants to know the maximum possible, provide the info.
	 */
	if (desc->wMaxPacketSize == 0)
		desc->wMaxPacketSize = cpu_to_le16(ep->maxpacket_limit);

	/* report address */
	desc->bEndpointAddress &= USB_DIR_IN;
	if (isdigit(ep->name[2])) {
		u8 num = simple_strtoul(&ep->name[2], NULL, 10);
		desc->bEndpointAddress |= num;
	} else if (desc->bEndpointAddress & USB_DIR_IN) {
		if (++gadget->in_epnum > 15)
			return NULL;
		desc->bEndpointAddress = USB_DIR_IN | gadget->in_epnum;
	} else {
		if (++gadget->out_epnum > 15)
			return NULL;
		desc->bEndpointAddress |= gadget->out_epnum;
	}

	ep->address = desc->bEndpointAddress;
	ep->desc = NULL;
	ep->comp_desc = NULL;
	ep->claimed = true;
	return ep;
}

static struct usb_ep *usb_ep_config(
	struct usb_gadget		*gadget,
	struct usb_endpoint_descriptor	*desc
)
{
	struct usb_ep	*ep;
	u8		type;

	ep = usb_ep_config_ss(gadget, desc, NULL);
	if (!ep)
		return NULL;

	type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

	/* report (variable) full speed bulk maxpacket */
	if (type == USB_ENDPOINT_XFER_BULK) {
		int size = ep->maxpacket_limit;

		/* min() doesn't work on bitfields with gcc-3.5 */
		if (size > 64)
			size = 64;
		desc->wMaxPacketSize = cpu_to_le16(size);
	}

	return ep;
}

static int
uvc_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct uvc_device *uvc = to_uvc(f);
	struct uvcg_extension *xu;
	struct usb_string *us;
	unsigned int max_packet_mult;
	unsigned int max_packet_size;
	struct usb_ep *ep;
	struct f_uvc_opts *opts;
	int i, j, ret = -EINVAL;
	int vs_idx = 0;

	uvcg_info(f, "%s()\n", __func__);
	uvc->active_streaming = UVC_STREAMING_NUMS;
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
#ifndef CONFIG_USB_CONFIGFS
		if (uvc_stream_empty(i)) {
#else
		if (!uvc->streaming_desc_num[i]) {
#endif
			inactive_streams[i] = 1;
			--uvc->active_streaming;
		}
	}

	opts = fi_to_f_uvc_opts(f->fi);
	/* Sanity check the streaming endpoint module parameters. */
	opts->streaming_interval = clamp(opts->streaming_interval, 1U, 16U);
	opts->streaming_maxpacket = clamp(opts->streaming_maxpacket, 1U, 3072U);
	opts->streaming_maxburst = min(opts->streaming_maxburst, 15U);

	/* For SS, wMaxPacketSize has to be 1024 if bMaxBurst is not 0 */
	if (opts->streaming_maxburst &&
	    (opts->streaming_maxpacket % 1024) != 0) {
		opts->streaming_maxpacket = roundup(opts->streaming_maxpacket, 1024);
		uvcg_info(f, "overriding streaming_maxpacket to %d\n",
			  opts->streaming_maxpacket);
	}

	/*
	 * Fill in the FS/HS/SS Video Streaming specific descriptors from the
	 * module parameters.
	 *
	 * NOTE: We assume that the user knows what they are doing and won't
	 * give parameters that their UDC doesn't support.
	 */
	if (opts->streaming_maxpacket <= 1024) {
		max_packet_mult = 1;
		max_packet_size = opts->streaming_maxpacket;
	} else if (opts->streaming_maxpacket <= 2048) {
		max_packet_mult = 2;
		max_packet_size = opts->streaming_maxpacket / 2;
	} else {
		max_packet_mult = 3;
		max_packet_size = opts->streaming_maxpacket / 3;
	}

	uvc_ss_streaming_comp.bmAttributes = max_packet_mult - 1;
	uvc_ss_streaming_comp.bMaxBurst = opts->streaming_maxburst;
	uvc_ss_streaming_comp.wBytesPerInterval =
		cpu_to_le16(max_packet_size * max_packet_mult *
			    (opts->streaming_maxburst + 1));

	/* Allocate endpoints. */
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i])
			continue;

		if (gadget_is_superspeed(c->cdev->gadget)) {
			uvc_streaming_eps[i][SS_IDX].wMaxPacketSize =
						cpu_to_le16(max_packet_size);
			uvc_streaming_eps[i][SS_IDX].bInterval =
						opts->streaming_interval;
			for (j = 0; j < ALT_SETTINGS_MAX - 2; j++) {
				uvc_streaming_eps[i][SS_IDX + j + 1].wMaxPacketSize =
						cpu_to_le16(transfer_size[j]);
				uvc_streaming_eps[i][SS_IDX + j + 1].bInterval =
						opts->streaming_interval;
			}
			ep = usb_ep_config_ss(cdev->gadget,
					&uvc_streaming_eps[i][SS_IDX],
					&uvc_ss_streaming_comp);
		} else if (gadget_is_dualspeed(cdev->gadget)) {
			uvc_streaming_eps[i][HS_IDX].wMaxPacketSize =
				cpu_to_le16(max_packet_size |
				((max_packet_mult - 1) << 11));
			/* A high-bandwidth endpoint must specify a
			 * bInterval value of 1
			 */
			if (max_packet_mult > 1)
				uvc_streaming_eps[i][HS_IDX].bInterval = 1;
			else
				uvc_streaming_eps[i][HS_IDX].bInterval =
						opts->streaming_interval;
			for (j = 0; j < ALT_SETTINGS_MAX - 2; j++) {
				uvc_streaming_eps[i][HS_IDX + j + 1].wMaxPacketSize =
						cpu_to_le16(transfer_size[j] |
							(pid[j] - 1) << 11);
				if (max_packet_mult > 1)
					uvc_streaming_eps[i][HS_IDX + j + 1].bInterval
						= 1;
				else
					uvc_streaming_eps[i][HS_IDX + j + 1].bInterval
						= opts->streaming_interval;
			}
			uvc_streaming_eps[i][FS_IDX].wMaxPacketSize =
				cpu_to_le16(min(opts->streaming_maxpacket, 1023U));
			uvc_streaming_eps[i][FS_IDX].bInterval = 1;
			uvc_streaming_eps[i][FS_IDX + 1].wMaxPacketSize = 512;
			uvc_streaming_eps[i][FS_IDX + 1].bInterval = 1;
			for (j = 2; j < ALT_SETTINGS_MAX - 1; j++) {
				uvc_streaming_eps[i][FS_IDX + j].wMaxPacketSize = 128;
				uvc_streaming_eps[i][FS_IDX + j].bInterval = 1;
			}
			ep = usb_ep_config(cdev->gadget,
					&uvc_streaming_eps[i][HS_IDX]);
		} else {
			uvc_streaming_eps[i][FS_IDX].wMaxPacketSize =
			cpu_to_le16(min(opts->streaming_maxpacket, 1023U));
			uvc_streaming_eps[i][FS_IDX].bInterval =
						opts->streaming_interval;
			ep = usb_ep_config(cdev->gadget,
					&uvc_streaming_eps[i][FS_IDX]);
		}


		if (!ep) {
			INFO(cdev, "Unable to allocate streaming EP %d\n", i);
			goto error;
		}

		uvc->video[i].ep = ep;
		for (j = 0; j < ALT_SETTINGS_MAX - 1; j++) {
			uvc_streaming_eps[i][FS_IDX + j].bEndpointAddress
				= ep->address;
			uvc_streaming_eps[i][HS_IDX + j].bEndpointAddress
				= ep->address;
			uvc_streaming_eps[i][SS_IDX + j].bEndpointAddress
				= ep->address;
		}
	}

	/*
	 * gadget_is_{super|dual}speed() API check UDC controller capitblity. It should pass down
	 * highest speed endpoint descriptor to UDC controller. So UDC controller driver can reserve
	 * enough resource at check_config(), especially mult and maxburst. So UDC driver (such as
	 * cdns3) can know need at least (mult + 1) * (maxburst + 1) * wMaxPacketSize internal
	 * memory for this uvc functions. This is the only straightforward method to resolve the UDC
	 * resource allocation issue in the current gadget framework.
	 */
	if (opts->enable_interrupt_ep) {
		if (gadget_is_superspeed(c->cdev->gadget))
			ep = usb_ep_autoconfig_ss(cdev->gadget, &uvc_interrupt_ep,
						&uvc_ss_streaming_comp);
		else
			ep = usb_ep_autoconfig(cdev->gadget, &uvc_interrupt_ep);
		if (!ep) {
			uvcg_info(f, "Unable to allocate interrupt EP\n");
			goto error;
		}
		uvc->interrupt_ep = ep;
		uvc_control_intf.bNumEndpoints = 1;
	}
	uvc->enable_interrupt_ep = opts->enable_interrupt_ep;

	/*
	 * XUs can have an arbitrary string descriptor describing them. If they
	 * have one pick up the ID.
	 */
	list_for_each_entry(xu, &opts->extension_units, list)
		if (xu->string_descriptor_index)
			xu->desc.iExtension = cdev->usb_strings[xu->string_descriptor_index].id;

	/*
	 * We attach the hard-coded defaults incase the user does not provide
	 * any more appropriate strings through configfs.
	 */
	uvc_en_us_strings[UVC_STRING_CONTROL_IDX].s = opts->function_name;
	us = usb_gstrings_attach(cdev, uvc_function_strings,
				 ARRAY_SIZE(uvc_en_us_strings));
	if (IS_ERR(us)) {
		ret = PTR_ERR(us);
		goto error;
	}

	uvc_iad.iFunction = opts->iad_index ? cdev->usb_strings[opts->iad_index].id :
			    us[UVC_STRING_CONTROL_IDX].id;
	/* must be equal to uvc_iad.iFunciton, spec 3.6 */
	uvc_control_intf.iInterface = uvc_iad.iFunction;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (i == 1) {
			vs_idx = opts->vs1_index;
		} else if (i > 1) {
			uvcg_info(f, "stream string only support max 2 way\n");
			goto error;
		}

		for (j = 0; j < ALT_SETTINGS_MAX; j++)
			uvc_streaming_intfs[i][j].iInterface = vs_idx ?
				cdev->usb_strings[vs_idx].id :
				 cdev->usb_strings[UVC_STRING_STREAMING_IDX + i].id;
	}
	/* Allocate interface IDs. */
	if ((ret = usb_interface_id(c, f)) < 0)
		goto error;
	uvc_iad.bFirstInterface = ret;
	uvc_control_intf.bInterfaceNumber = ret;
	uvc->control_intf = ret;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (inactive_streams[i])
			continue;

		if ((ret = usb_interface_id(c, f)) < 0)
			goto error;
		for (j = 0; j < ALT_SETTINGS_MAX; j++)
			uvc_streaming_intfs[i][j].bInterfaceNumber = ret;
		uvc->streaming_intfs[i] = ret;
	}

	/* Copy descriptors */
	f->fs_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_FULL);
	if (IS_ERR(f->fs_descriptors)) {
		ret = PTR_ERR(f->fs_descriptors);
		f->fs_descriptors = NULL;
		goto error;
	}

	f->hs_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_HIGH);
	if (IS_ERR(f->hs_descriptors)) {
		ret = PTR_ERR(f->hs_descriptors);
		f->hs_descriptors = NULL;
		goto error;
	}

	f->ss_descriptors = uvc_copy_descriptors(uvc, USB_SPEED_SUPER);
	if (IS_ERR(f->ss_descriptors)) {
		ret = PTR_ERR(f->ss_descriptors);
		f->ss_descriptors = NULL;
		goto error;
	}

	/* Preallocate control endpoint request. */
	uvc->control_req = usb_ep_alloc_request(cdev->gadget->ep0, GFP_KERNEL);
	uvc->control_buf = kmalloc(UVC_MAX_REQUEST_SIZE, GFP_KERNEL);
	if (uvc->control_req == NULL || uvc->control_buf == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	uvc->control_req->buf = uvc->control_buf;
	uvc->control_req->complete = uvc_function_ep0_complete;
	uvc->control_req->context = uvc;

	if (v4l2_device_register(&cdev->gadget->dev, &uvc->v4l2_dev)) {
		uvcg_err(f, "failed to register V4L2 device\n");
		goto error;
	}

	if (uvc->enable_interrupt_ep) {
		/* Preallocate status endpoint request. */
		uvc->int_req = usb_ep_alloc_request(uvc->interrupt_ep, GFP_KERNEL);
		uvc->int_buf = kmalloc(UVC_MAX_REQUEST_SIZE, GFP_KERNEL);
		if (uvc->int_req == NULL || uvc->int_buf == NULL) {
			ret = -ENOMEM;
			goto error;
		}

		uvc->int_req->buf = uvc->int_buf;
		uvc->int_req->complete = uvc_function_epc_complete;
		uvc->int_req->context = uvc;
	}
	/* Initialise video. */
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		uvc->video[i].gadget = cdev->gadget;	// for sof frame number
		ret = uvcg_video_init(i, &uvc->video[i], uvc);
		if (ret < 0)
			goto error;
	}

	/* Register a V4L2 device. */
	ret = uvc_register_video(uvc);
	if (ret < 0) {
		rlxprintk(RLX_TRACE_ERROR, "failed to register video device\n");
		goto error;
	}

	cdev->gadget->dev.coherent_dma_mask = 0xffffffff;
	return 0;

error:
	v4l2_device_unregister(&uvc->v4l2_dev);
	if (uvc->control_req)
		usb_ep_free_request(cdev->gadget->ep0, uvc->control_req);
	kfree(uvc->control_buf);

	if (uvc->int_req)
		usb_ep_free_request(uvc->interrupt_ep, uvc->int_req);
	kfree(uvc->int_buf);

	usb_free_all_descriptors(f);
	return ret;
}

/* --------------------------------------------------------------------------
 * USB gadget function
 */

static void uvc_free_inst(struct usb_function_instance *f)
{
	struct f_uvc_opts *opts = fi_to_f_uvc_opts(f);

	rtsx_uvc_function_cleanup(opts);
	mutex_destroy(&opts->lock);
	kfree(opts->uvc);
	kfree(opts);
}

static struct usb_function_instance *uvc_alloc_inst(void)
{
	struct f_uvc_opts *opts;
	struct uvc_camera_terminal_descriptor *cd;
	struct uvc_processing_unit_descriptor *pd;
	struct uvc_output_terminal_descriptor *od;
	struct uvc_still_frame_descriptor    *sd;
	struct uvc_descriptor_header **ctl_cls;
	/* Guid(Int32, Int16, Int16, Byte[8]) */
	/* {1229a78c-47b4-4094-b0ce-db07386fb938} */
	int ret, i;
#ifdef CONFIG_USB_RTSX_UVC_15
	struct uvc_encode_unit_descriptor *ed;
	int pu_ctrl_size = 3;
#else
	int pu_ctrl_size = 2;
#endif

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	opts->uvc = kzalloc(sizeof(*opts->uvc), GFP_KERNEL);
	if (!opts->uvc)
		return ERR_PTR(-ENOMEM);

	opts->func_inst.free_func_inst = uvc_free_inst;
	mutex_init(&opts->lock);

	cd = &opts->uvc_camera_terminal;
	cd->bLength			= UVC_DT_CAMERA_TERMINAL_SIZE(3);
	cd->bDescriptorType		= USB_DT_CS_INTERFACE;
	cd->bDescriptorSubType		= UVC_VC_INPUT_TERMINAL;
	cd->bTerminalID			= 1;
	cd->wTerminalType		= cpu_to_le16(0x0201);
	cd->bAssocTerminal		= 0;
	cd->iTerminal			= 0;
	cd->wObjectiveFocalLengthMin	= cpu_to_le16(0);
	cd->wObjectiveFocalLengthMax	= cpu_to_le16(0);
	cd->wOcularFocalLength		= cpu_to_le16(0);
	cd->bControlSize		= 3;
	cd->bmControls[0]		= 2;
	cd->bmControls[1]		= 0;
	cd->bmControls[2]		= 0;

	pd = &opts->uvc_processing;
	pd->bLength                     = UVC_DT_PROCESSING_UNIT_SIZE(
						pu_ctrl_size);
	pd->bDescriptorType		= USB_DT_CS_INTERFACE;
	pd->bDescriptorSubType		= UVC_VC_PROCESSING_UNIT;
	pd->bUnitID			= 2;
	pd->bSourceID			= 1;
	pd->wMaxMultiplier		= cpu_to_le16(16*1024);
	pd->bControlSize		= pu_ctrl_size;
	memset(pd->bmControls, 0, pu_ctrl_size);
	pd->bmControls[0]		= 1;
	pd->iProcessing			= 0;
	pd->bmVideoStandards		= 0;
#ifdef CONFIG_USB_RTSX_UVC_15
	ed = &opts->uvc_encode;
	ed->bLength			= UVC_DT_ENCODE_UNIT_SIZE;
	ed->bDescriptorType		= USB_DT_CS_INTERFACE;
	ed->bDescriptorSubType		= UVC_VC_ENCODING_UNIT;
	ed->bUnitID			= ENT_ID_ENCODING_UNIT;
	ed->bSourceID			= ENT_ID_PROCESSING_UNIT;
	ed->iEncoding			= 0x00;
	ed->bControlSize		= 0x03;
	ed->bmControls[0]		= 0x00;
	ed->bmControls[1]		= 0x00;
	ed->bmControls[2]		= 0x00;
	ed->bmControlsRuntime[0]	= 0x00;
	ed->bmControlsRuntime[1]	= 0x00;
	ed->bmControlsRuntime[2]	= 0x00;
#endif
	od = &opts->uvc_output_terminal;
	od->bLength			= UVC_DT_OUTPUT_TERMINAL_SIZE;
	od->bDescriptorType		= USB_DT_CS_INTERFACE;
	od->bDescriptorSubType		= UVC_VC_OUTPUT_TERMINAL;
	od->bTerminalID			= ENT_ID_OUTPUT_TERMINAL;
	od->wTerminalType		= cpu_to_le16(0x0101);
	od->bAssocTerminal		= 0;
	od->bSourceID			= ENT_ID_LAST_EXTENSION_UNIT;
	od->iTerminal			= 0;

	/*
	 * With the ability to add XUs to the UVC function graph, we need to be
	 * able to allocate unique unit IDs to them. The IDs are 1-based, with
	 * the CT, PU and OT above consuming the first 3.
	 */
	opts->last_unit_id		= 3;

	for (i = 0; i < FORMAT_CNT; i++) {
		sd = &opts->uvc_still_frame[i];

		sd->bLength = 10;
		sd->bDescriptorType = USB_DT_CS_INTERFACE;
		sd->bDescriptorSubtype = UVC_VS_STILL_IMAGE_FRAME;
		sd->bEndpointAddress = 0;
		sd->bNumImageSizePatterns = 0;
		sd->bNumCompressionPattern = 0;
	}


	/* Prepare fs control class descriptors for configfs-based gadgets */
	ctl_cls = opts->uvc_fs_control_cls;
	i = 0;
	ctl_cls[i++] = NULL;	/* assigned elsewhere by configfs */
	ctl_cls[i++] = (struct uvc_descriptor_header *)cd;
	ctl_cls[i++] = (struct uvc_descriptor_header *)pd;
#ifdef CONFIG_USB_RTSX_UVC_15
	ctl_cls[i++] = (struct uvc_descriptor_header *)ed;
#endif
	ctl_cls[i++] = (struct uvc_descriptor_header *)od;
	ctl_cls[i] = NULL;	/* NULL-terminate */
	opts->fs_control =
		(struct uvc_descriptor_header **)ctl_cls;

	/* Prepare hs control class descriptors for configfs-based gadgets */
	ctl_cls = opts->uvc_ss_control_cls;
	i = 0;
	ctl_cls[i++] = NULL;	/* assigned elsewhere by configfs */
	ctl_cls[i++] = (struct uvc_descriptor_header *)cd;
	ctl_cls[i++] = (struct uvc_descriptor_header *)pd;
#ifdef CONFIG_USB_RTSX_UVC_15
	ctl_cls[i++] = (struct uvc_descriptor_header *)ed;
#endif
	ctl_cls[i++] = (struct uvc_descriptor_header *)od;
	ctl_cls[i] = NULL;	/* NULL-terminate */
	opts->ss_control =
		(struct uvc_descriptor_header **)ctl_cls;
	INIT_LIST_HEAD(&opts->extension_units);

	opts->streaming_interval = 1;
	opts->streaming_maxpacket = 1024;
	snprintf(opts->function_name, sizeof(opts->function_name), "UVC Camera");

	ret = uvcg_attach_configfs(opts);
	if (ret < 0) {
		kfree(opts);
		return ERR_PTR(ret);
	}

	if (rtsx_uvc_function_init(opts)) {
		kfree(opts);
		return NULL;
	}

	return &opts->func_inst;
}

static void uvc_free(struct usb_function *f)
{
	struct uvc_device *uvc = to_uvc(f);
	struct f_uvc_opts *opts = container_of(f->fi, struct f_uvc_opts,
					       func_inst);
	if (!opts->header)
		config_item_put(&uvc->header->item);
	--opts->refcnt;
	memset(&uvc->func, 0, sizeof(uvc->func));
	memset(&uvc->desc, 0, sizeof(uvc->desc));
}

static void uvc_function_unbind(struct usb_configuration *c,
				struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct uvc_device *uvc = to_uvc(f);
	long wait_ret = 1;
	int i;

	uvcg_info(f, "%s()\n", __func__);

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		if (uvc->video[i].async_wq)
			destroy_workqueue(uvc->video[i].async_wq);
	}

	/*
	 * If we know we're connected via v4l2, then there should be a cleanup
	 * of the device from userspace either via UVC_EVENT_DISCONNECT or
	 * though the video device removal uevent. Allow some time for the
	 * application to close out before things get deleted.
	 */
	if (uvc->func_connected) {
		uvcg_dbg(f, "waiting for clean disconnect\n");
		wait_ret = wait_event_interruptible_timeout(uvc->func_connected_queue,
				uvc->func_connected == false, msecs_to_jiffies(500));
		uvcg_dbg(f, "done waiting with ret: %ld\n", wait_ret);
	}

	device_remove_file(&uvc->vdev[0].dev, &dev_attr_function_name);
	for (i = 0; i < UVC_STREAMING_NUMS; ++i)
		video_unregister_device(&uvc->vdev[i]);
	v4l2_device_unregister(&uvc->v4l2_dev);

	if (uvc->func_connected) {
		/*
		 * Wait for the release to occur to ensure there are no longer any
		 * pending operations that may cause panics when resources are cleaned
		 * up.
		 */
		uvcg_warn(f, "%s no clean disconnect, wait for release\n", __func__);
		wait_ret = wait_event_interruptible_timeout(uvc->func_connected_queue,
				uvc->func_connected == false, msecs_to_jiffies(1000));
		uvcg_dbg(f, "done waiting for release with ret: %ld\n", wait_ret);
	}

	usb_ep_free_request(cdev->gadget->ep0, uvc->control_req);
	kfree(uvc->control_buf);

	usb_ep_free_request(uvc->interrupt_ep, uvc->int_req);
	kfree(uvc->int_buf);
	usb_free_all_descriptors(f);
}

static struct usb_function *uvc_alloc(struct usb_function_instance *fi)
{
	int i;
	struct f_uvc_opts *opts = fi_to_f_uvc_opts(fi);
	struct uvc_device *uvc = opts->uvc;
	struct uvc_descriptor_header **strm_cls;
	struct config_item *streaming, *header, *h;

	init_waitqueue_head(&uvc->func_connected_queue);
	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		mutex_init(&uvc->video[i].mutex);
		uvc->state[i] = UVC_STATE_DISCONNECTED;
		rlxprintk(RLX_TRACE_STATUS, "init to disconnect\n");
	}

	mutex_lock(&opts->lock);
	if (opts->uvc_fs_streaming_cls) {
		strm_cls = opts->uvc_fs_streaming_cls;
		opts->fs_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}
	if (opts->uvc_hs_streaming_cls) {
		strm_cls = opts->uvc_hs_streaming_cls;
		opts->hs_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}
	if (opts->uvc_ss_streaming_cls) {
		strm_cls = opts->uvc_ss_streaming_cls;
		opts->ss_streaming =
			(const struct uvc_descriptor_header * const *)strm_cls;
	}

	/* android_uvc.c overwite opts->fs_control defined in configfs */
	uvc->desc.fs_control = opts->fs_control;
	uvc->desc.ss_control = opts->ss_control;
	uvc->desc.fs_streaming = opts->fs_streaming;
	uvc->desc.hs_streaming = opts->hs_streaming;
	uvc->desc.ss_streaming = opts->ss_streaming;

	if (opts->header) {
		uvc->header = opts->header;
	} else {
		streaming = config_group_find_item(&opts->func_inst.group, "streaming");
		if (!streaming)
			goto err_config;

		header = config_group_find_item(to_config_group(streaming), "header");
		config_item_put(streaming);
		if (!header)
			goto err_config;

		h = config_group_find_item(to_config_group(header), "h");
		config_item_put(header);
		if (!h)
			goto err_config;

		uvc->header = to_uvcg_streaming_header(h);
		if (!uvc->header->linked) {
			mutex_unlock(&opts->lock);
			kfree(uvc);
			return ERR_PTR(-EBUSY);
		}
	}

	uvc->desc.extension_units = &opts->extension_units;

	++opts->refcnt;
	mutex_unlock(&opts->lock);

	/* Register the function. */
	uvc->func.name = "uvc";
	uvc->func.bind = uvc_function_bind;
	uvc->func.unbind = uvc_function_unbind;
	uvc->func.get_alt = uvc_function_get_alt;
	uvc->func.set_alt = uvc_function_set_alt;
	uvc->func.disable = uvc_function_disable;
	uvc->func.setup = uvc_function_setup;
	uvc->func.free_func = uvc_free;
	uvc->func.bind_deactivated = true;
	uvc->func.filteroutdata = uvc_function_filteroutdata;

	return &uvc->func;

err_config:
	mutex_unlock(&opts->lock);
	kfree(uvc);
	return ERR_PTR(-ENOENT);
}

DECLARE_USB_FUNCTION_INIT(uvc, uvc_alloc_inst, uvc_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Laurent Pinchart");
