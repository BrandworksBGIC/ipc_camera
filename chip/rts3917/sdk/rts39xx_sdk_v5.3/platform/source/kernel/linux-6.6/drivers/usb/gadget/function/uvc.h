/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *	uvc_gadget.h  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#ifndef _UVC_GADGET_H_
#define _UVC_GADGET_H_

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/usb/composite.h>
#include <linux/videodev2.h>
#include <linux/wait.h>

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>

#include "uvc_queue.h"

struct usb_ep;
struct usb_request;
struct uvc_descriptor_header;
struct uvc_device;

/* ------------------------------------------------------------------------
 * Debugging, printing and logging
 */

#define UVC_TRACE_PROBE				(1 << 0)
#define UVC_TRACE_DESCR				(1 << 1)
#define UVC_TRACE_CONTROL			(1 << 2)
#define UVC_TRACE_FORMAT			(1 << 3)
#define UVC_TRACE_CAPTURE			(1 << 4)
#define UVC_TRACE_CALLS				(1 << 5)
#define UVC_TRACE_IOCTL				(1 << 6)
#define UVC_TRACE_FRAME				(1 << 7)
#define UVC_TRACE_SUSPEND			(1 << 8)
#define UVC_TRACE_STATUS			(1 << 9)

#define UVC_WARN_MINMAX				0
#define UVC_WARN_PROBE_DEF			1

extern unsigned int uvc_gadget_trace_param;

#define uvc_trace(flag, msg...) \
	do { \
		if (uvc_gadget_trace_param & flag) \
			printk(KERN_DEBUG "uvcvideo: " msg); \
	} while (0)

#define DEBUG
#define uvcg_dbg(f, fmt, args...) \
	dev_dbg(&(f)->config->cdev->gadget->dev, "%s: " fmt, (f)->name, ##args)
#define uvcg_info(f, fmt, args...) \
	dev_info(&(f)->config->cdev->gadget->dev, "%s: " fmt, (f)->name, ##args)
#define uvcg_warn(f, fmt, args...) \
	dev_warn(&(f)->config->cdev->gadget->dev, "%s: " fmt, (f)->name, ##args)
#define uvcg_err(f, fmt, args...) \
	dev_err(&(f)->config->cdev->gadget->dev, "%s: " fmt, (f)->name, ##args)

/* ------------------------------------------------------------------------
 * Driver specific constants
 */

#define UVC_INTF_CONTROL		0
#define UVC_INTF_STREAMING		1
#define UVCG_REQUEST_HEADER_LEN		12

#define STREAM_BASE_NUM			41
#define UVC_STREAMING_NUMS		2
#ifdef CONFIG_USB_RTSX_UVC_15
#define BASIC_ENTITY_NUMS		4
#else
#define BASIC_ENTITY_NUMS		3
#endif

#define METADATA_LEN_MAX		243
/* ------------------------------------------------------------------------
 * Structures
 */
struct uvc_request {
	struct usb_request *req;
	u8 *req_buffer;
	struct uvc_video *video;
	struct sg_table sgt;
	u8 header[UVCG_REQUEST_HEADER_LEN];
	struct uvc_buffer *last_buf;
	dma_addr_t req_phys;
};

struct metadata_buffer {
	int		index;
	__u32		length;
	void		*userptr;
	__u32		length_actual;
};

struct uvc_video {
	struct uvc_device *uvc;
	struct usb_ep *ep;

	struct work_struct pump;
	struct workqueue_struct *async_wq;
	bool stop_pump;
	int stream_idx;
	int flag;		/* for compatibility */

	/* Frame parameters */
	u8 bpp;
	u32 fcc;
	unsigned int width;
	unsigned int height;
	unsigned int imagesize;
	unsigned int maximagesize;
	struct mutex mutex;	/* protects frame parameters */

	unsigned int uvc_num_requests;

	/* Requests */
	unsigned int req_size;
	struct uvc_request *ureq;
	struct list_head req_free;
	spinlock_t req_lock;

	unsigned int req_int_count;
	int req_refcnt;

	void (*encode) (struct usb_request *req, struct uvc_video *video,
			struct uvc_buffer *buf);

	/* Context data used by the completion handler */
	__u32 payload_size;
	__u32 max_payload_size;
	struct metadata_buffer mbuf;

	struct uvc_video_queue queue;
	unsigned int fid;
	struct usb_gadget *gadget;
};

/**
 * enum uvc_state - the states of a struct uvc_device
 * @UVC_STATE_DISCONNECTED: not connected state
 *			    - transition to connected state on .set_alt
 * @UVC_STATE_CONNECTED:    connected state
 *			    - transition to disconnected state on .disable
 *			      and alloc
 *			    - transition to starting state on .set_alt 1
 * @UVC_STATE_STARTING:     starting state
 *			    - transition to streaming state on streamon ioctl
 *			    - transition to stopping state on set_alt 0
 * @UVC_STATE_STREAMING:    streaming state
 *			    - transition to stopping state on .set_alt 0
 * @UVC_STATE_STOPPING:     stopping state
 *			    - transition to connected on streamoff ioctl
 *
 * Diagram of state transitions:
 *
 *                  disable
 *   +---------------------------+
 *   v                           |
 * +--------------+  set_alt   +-----------+
 * | DISCONNECTED | ---------> | CONNECTED |
 * +--------------+            +-----------+
 *                                |     ^
 *                 set_alt 1      |     |     streamoff
 *         +----------------------+     --------------------+
 *         V                                                |
 *  +----------+  streamon   +-----------+  set_alt 0   +----------+
 *  | STARTING | ----------> | STREAMING | -----------> | STOPPING |
 *  +----------+             +-----------+              +----------+
 *         |                                                ^
 *         |                   set_alt 0                    |
 *         +------------------------------------------------+
 */
enum uvc_state {
	UVC_STATE_DISCONNECTED,
	UVC_STATE_CONNECTED,
	UVC_STATE_STARTING,
	UVC_STATE_STREAMING,
	UVC_STATE_STOPPING,
};

struct uvc_device {
	struct video_device vdev[UVC_STREAMING_NUMS];
	struct v4l2_device v4l2_dev;
	enum uvc_state state[UVC_STREAMING_NUMS];
	struct usb_function func;
	struct uvc_video video[UVC_STREAMING_NUMS];
	bool func_connected;
	wait_queue_head_t func_connected_queue;

	struct uvcg_streaming_header *header;

	/* Descriptors */
	struct {
		struct uvc_descriptor_header **fs_control;
		struct uvc_descriptor_header **ss_control;
		const struct uvc_descriptor_header * const *fs_streaming;
		const struct uvc_descriptor_header * const *hs_streaming;
		const struct uvc_descriptor_header * const *ss_streaming;
		struct list_head *extension_units;
	} desc;

	unsigned int control_intf;
	struct usb_ep *interrupt_ep;
	struct usb_request *control_req;
	void *control_buf;
	bool enable_interrupt_ep;

	/* for streaming ep */
	int active_streaming;
	unsigned int streaming_intfs[UVC_STREAMING_NUMS];
	unsigned int streaming_desc_num[UVC_STREAMING_NUMS];
	int alt;

	/* for status ep */
	struct usb_request *int_req;
	void *int_buf;

	/* Events */
	unsigned int event_length;
	unsigned int event_setup_out : 1;

	/* for android_uvc */
	struct cdev android_uvc_cdev;
};

static inline struct uvc_device *to_uvc(struct usb_function *f)
{
	return container_of(f, struct uvc_device, func);
}

struct uvc_file_handle {
	struct v4l2_fh vfh;
	struct uvc_video *device;
	bool is_uvc_app_handle;
};

#define to_uvc_file_handle(handle) \
	container_of(handle, struct uvc_file_handle, vfh)

/* ------------------------------------------------------------------------
 * Functions
 */

extern void uvc_function_setup_continue(struct uvc_device *uvc);
extern void uvc_function_connect(struct uvc_device *uvc);
extern void uvc_function_disconnect(struct uvc_device *uvc);

#endif /* _UVC_GADGET_H_ */
