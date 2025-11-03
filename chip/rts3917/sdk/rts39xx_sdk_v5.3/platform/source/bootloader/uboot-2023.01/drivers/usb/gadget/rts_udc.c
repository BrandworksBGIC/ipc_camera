// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 */
#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <asm/io.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/bug.h>
#include <usb/rts_udc.h>
#include "rts_udc_pri.h"
#include "rts_usb_regs.h"

static u8 __aligned(4) msos[] = {
	/* Microsoft OS 2.0 Descriptor Set Header */
	MSOS20_SET_HEADER_LEN,
	0x00, // wLength of MSOS20_SET_HEADER_DESCRIPTOR
	MS_OS_20_SET_HEADER_DESCRIPTOR,
	0x00, // wDescriptorType == MSOS20_SET_HEADER_DESCRIPTOR
	0x00,
	0x00,
	0x03,
	0x06, // dwWindowsVersion ?V 0x10000000 for Windows 8
	MSOS20_SET_DFU_TOTAL_LEN % 256,
	MSOS20_SET_DFU_TOTAL_LEN / 256,

	//DeviceIdleEnabled
	(DEV_IDEL_ENABLE_LEN) % 256,
	(DEV_IDEL_ENABLE_LEN) /
		256, // wLength 0x30 (48) in bytes of this descriptor
	MS_OS_20_FEATURE_REG_PROPERTY,
	0x00, // wDescriptorType ?V MSOS20_FEATURE_REG_PROPERTY
	REG_DWORD_LITTLE_ENDIAN,
	0x00, // wPropertyDataType - REG_DWORD

	0x24,
	0x00, // wPropertyNameLength ?V 0x24 (36) bytes
	'D',
	0x00,
	'e',
	0x00,
	'v',
	0x00,
	'i',
	0x00,
	'c',
	0x00,
	'e',
	0x00,
	'I',
	0x00,
	'd',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	'E',
	0x00,
	'n',
	0x00,
	'a',
	0x00,
	'b',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	'd',
	0x00,
	0x00,
	0x00,

	0x04,
	0x00, // wPropertyDataLength ?V 4 bytes
	0x01,
	0x00,
	0x00,
	0x00, // This exposes the camera to OS as an IR only camera

	// DefaultIdleState
	(DEF_IDEL_STATE_LEN) % 256,
	(DEF_IDEL_STATE_LEN) /
		256, // wLength 0x30 (48) in bytes of this descriptor
	MS_OS_20_FEATURE_REG_PROPERTY,
	0x00, // wDescriptorType ?V MSOS20_FEATURE_REG_PROPERTY
	REG_DWORD_LITTLE_ENDIAN,
	0x00, // wPropertyDataType - REG_DWORD

	0x24,
	0x00, // wPropertyNameLength ?V 0x24 (36) bytes
	'D',
	0x00,
	'e',
	0x00,
	'f',
	0x00,
	'a',
	0x00,
	'u',
	0x00,
	'l',
	0x00,
	't',
	0x00,
	'I',
	0x00,
	'd',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	'S',
	0x00,
	't',
	0x00,
	'a',
	0x00,
	't',
	0x00,
	'e',
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x04,
	0x00, // wPropertyDataLength ?V 4 bytes
	0x01,
	0x00,
	0x00,
	0x00, // This exposes the camera to OS as an IR only camera

	// DefaultIdleTimeout
	(DEF_IDEL_TMOUT_LEN) % 256,
	(DEF_IDEL_TMOUT_LEN) /
		256, // wLength 0x30 (48) in bytes of this descriptor
	MS_OS_20_FEATURE_REG_PROPERTY,
	0x00, // wDescriptorType ?V MSOS20_FEATURE_REG_PROPERTY
	REG_DWORD_LITTLE_ENDIAN,
	0x00, // wPropertyDataType - REG_DWORD

	0x28,
	0x00, // wPropertyNameLength ?V 0x28 (40) bytes
	'D',
	0x00,
	'e',
	0x00,
	'f',
	0x00,
	'a',
	0x00,
	'u',
	0x00,
	'l',
	0x00,
	't',
	0x00,
	'I',
	0x00,
	'd',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	'T',
	0x00,
	'i',
	0x00,
	'm',
	0x00,
	'e',
	0x00,
	'o',
	0x00,
	'u',
	0x00,
	't',
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x04,
	0x00, // wPropertyDataLength ?V 4 bytes
	0x88,
	0x13,
	0x00,
	0x00, // This exposes the camera to OS as an IR only camera

	// DeviceIdleIgnoreWakeEnable
	(DEV_IDEL_IGNWK_LEN) % 256,
	(DEV_IDEL_IGNWK_LEN) /
		256, // wLength 0x30 (48) in bytes of this descriptor
	MS_OS_20_FEATURE_REG_PROPERTY,
	0x00, // wDescriptorType ?V MSOS20_FEATURE_REG_PROPERTY
	REG_DWORD_LITTLE_ENDIAN,
	0x00, // wPropertyDataType - REG_DWORD
	0x38,
	0x00, // wPropertyNameLength ?V 0x38 (56) bytes
	'D',
	0x00,
	'e',
	0x00,
	'v',
	0x00,
	'i',
	0x00,
	'c',
	0x00,
	'e',
	0x00,
	'I',
	0x00,
	'd',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	'I',
	0x00,
	'g',
	0x00,
	'n',
	0x00,
	'o',
	0x00,
	'r',
	0x00,
	'e',
	0x00,
	'W',
	0x00,
	'a',
	0x00,
	'k',
	0x00,
	'e',
	0x00,
	'E',
	0x00,
	'n',
	0x00,
	'a',
	0x00,
	'b',
	0x00,
	'l',
	0x00,
	'e',
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x04,
	0x00, // wPropertyDataLength ?V 4 bytes
	0x01,
	0x00,
	0x00,
	0x00, // This exposes the camera to OS as an IR only camera

	// MS OS 2.0 compatible ID descriptor
	MS_CMPATIBLE_ID_LEN,
	0x00, // desceiptor size 20 bytes
	MS_OS_20_FEATURE_COMPATBLE_ID,
	0x00, // MS OS 2.0 compatible ID descriptor
	0X57,
	0X49,
	0X4E,
	0X55,
	0X53,
	0X42,
	0x00,
	0x00, // compatibleID(winusb string)
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00, // subCompatibleID

	// Register property descriptor
	DEV_INTF_GUID_LEN,
	0x00, // size 130 bytes
	MS_OS_20_FEATURE_REG_PROPERTY,
	0x00, // register property
	REG_SZ,
	0x00, // strings are null-terminated unicode
	0x28,
	0x00, // size of property Name(40 bytes)

	// property name ("DeviceInterfaceGUID")
	0x44,
	0x00,
	0x65,
	0x00,
	0x76,
	0x00,
	0x69,
	0x00,
	0x63,
	0x00,
	0x65,
	0x00,
	0x49,
	0x00,
	0x6e,
	0x00,
	0x74,
	0x00,
	0x65,
	0x00,
	0x72,
	0x00,
	0x66,
	0x00,
	0x61,
	0x00,
	0x63,
	0x00,
	0x65,
	0x00,
	0x47,
	0x00,
	0x55,
	0x00,
	0x49,
	0x00,
	0x44,
	0x00,
	0x00,
	0x00,

	0x4E,
	0x00, // size of property data (78 bytes)

	// Vendor-defined property data:{817A51C5-EA75-4465-B4E0-3EEAC93103D6}
	0x7B,
	0x00,
	0x38,
	0x00,
	0x31,
	0x00,
	0x37,
	0x00,
	0x41,
	0x00,
	0x35,
	0x00,
	0x31,
	0x00,
	0x43,
	0x00,
	0x35,
	0x00,
	0x2D,
	0x00,
	0x45,
	0x00,
	0x41,
	0x00,
	0x37,
	0x00,
	0x35,
	0x00,
	0x2D,
	0x00,
	0x34,
	0x00,
	0x34,
	0x00,
	0x36,
	0x00,
	0x35,
	0x00,
	0x2D,
	0x00,
	0x42,
	0x00,
	0x34,
	0x00,
	0x45,
	0x00,
	0x30,
	0x00,
	0x2D,
	0x00,
	0x33,
	0x00,
	0x45,
	0x00,
	0x45,
	0x00,
	0x41,
	0x00,
	0x43,
	0x00,
	0x39,
	0x00,
	0x33,
	0x00,
	0x31,
	0x00,
	0x30,
	0x00,
	0x33,
	0x00,
	0x44,
	0x00,
	0x36,
	0x00,
	0x7D,
	0x00,
	0x00,
	0x00,
};

extern struct rts_udc controller;

static inline u32 usb_read_reg(u32 usb_base, u32 offset)
{
	return readl(usb_base + offset);
}

static inline void usb_write_reg(u32 usb_base, u32 value, u32 offset)
{
	writel(value, usb_base + offset);
}

static inline void usb_clr_reg_bit(u32 usb_base, int bit, u32 offset)
{
	u32 v;

	v = usb_read_reg(usb_base, offset) & ~(1 << bit);
	usb_write_reg(usb_base, v, offset);
}

static inline void usb_set_reg_bit(u32 usb_base, int bit, u32 offset)
{
	u32 v;

	v = usb_read_reg(usb_base, offset) | (1 << bit);
	usb_write_reg(usb_base, v, offset);
}

static inline u32 mc_read_reg(u32 mc_base, u32 offset)
{
	return readl(mc_base + offset);
}

static inline void mc_write_reg(u32 mc_base, u32 value, u32 offset)
{
	writel(value, mc_base + offset);
}

static inline void mc_clr_reg_bit(u32 mc_base, int bit, u32 offset)
{
	u32 v;

	v = mc_read_reg(mc_base, offset) & ~(1 << bit);
	mc_write_reg(mc_base, v, offset);
}

static inline void mc_set_reg_bit(u32 mc_base, int bit, u32 offset)
{
	u32 v;

	v = mc_read_reg(mc_base, offset) | (1 << bit);
	mc_write_reg(mc_base, v, offset);
}

static int rts_gadget_wakeup(struct usb_gadget *g)
{
	struct rts_udc *rtsusb = &controller;

	RTS_DEBUG("%s()\n", __func__);

	usb_set_reg_bit(rtsusb->usb_base, WAKEUP_EN_OFFSET, USB_CTRL);

	return 0;
}

static void rts_usb_connect(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);
	usb_set_reg_bit(rtsusb->usb_base, CONNECT_EN_OFFSET, USB_CTRL);
}

static void rts_usb_disconnect(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);
	usb_clr_reg_bit(rtsusb->usb_base, CONNECT_EN_OFFSET, USB_CTRL);
}

static int rts_gadget_vbus_session(struct usb_gadget *g, int is_active)
{
	struct rts_udc *rtsusb = gadget_to_rts(g);

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->vbuson = (is_active != 0);
	if (rtsusb->gadgetstart && rtsusb->vbuson)
		rts_usb_connect(rtsusb);
	else if (rtsusb->gadgetstart == 0 || !rtsusb->vbuson)
		rts_usb_disconnect(rtsusb);

	return 0;
}

static int rts_gadget_pullup(struct usb_gadget *g, int is_on)
{
	struct rts_udc *rtsusb = gadget_to_rts(g);

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->gadgetstart = is_on;
	if (rtsusb->gadgetstart && rtsusb->vbuson)
		rts_usb_connect(rtsusb);
	else if (rtsusb->gadgetstart == 0)
		rts_usb_disconnect(rtsusb);

	return 0;
}

static int rts_gadget_free_ep0(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);

	list_del(&rtsusb->ep0.queue);
	return 0;
}

static int rts_gadget_start(struct usb_gadget *g,
			    struct usb_gadget_driver *driver)
{
	struct rts_udc *rtsusb = &controller;

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->gadget_driver = driver;

	return 0;
}

static int rts_gadget_stop(struct usb_gadget *g)
{
	struct rts_udc *rtsusb = &controller;

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->gadget_driver = NULL;
	rts_gadget_free_ep0(rtsusb);
	free(rtsusb->setup_buf);
	rts_gadget_pullup(&rtsusb->gadget, 0);

	return 0;
}

static const struct usb_gadget_ops rts_gadget_ops = {
	.wakeup = rts_gadget_wakeup,
	.vbus_session = rts_gadget_vbus_session,
	.pullup = rts_gadget_pullup,
	.udc_start = rts_gadget_start,
	.udc_stop = rts_gadget_stop,
};

/**
 * rts_ep_enable
 */
static int rts_ep_enable(struct usb_ep *ep,
			 const struct usb_endpoint_descriptor *desc)
{
	struct rts_endpoint *priv_ep = ep_to_rts_ep(ep);

	RTS_DEBUG("%s()\n", __func__);

	priv_ep->desc = desc;
	priv_ep->epnum = usb_endpoint_num(desc);
	return 0;
}

/*
 * rts_set_cxdone
 * EP0 return host ACK
 */
static void rts_set_cxdone(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);
	usb_set_reg_bit(rtsusb->usb_base, EP0_CSH_OFFSET, USB_EP_CTL1);
}

static void rts_done(struct rts_endpoint *priv_ep, struct rts_request *priv_req,
		     int status)
{
	struct rts_udc *rtsusb = &controller;
	bool stopped = priv_ep->stopped;

	RTS_DEBUG("%s() ep %d\n", __func__, priv_ep->epnum);

	list_del_init(&priv_req->queue);

	priv_ep->stopped = 1;

	/* don't modify queue heads during completion callback */
	if (priv_ep->rts_dev->gadget.speed == USB_SPEED_UNKNOWN)
		priv_req->request.status = -ESHUTDOWN;
	else
		priv_req->request.status = status;

	if (priv_ep->dir_in) {
		usb_clr_reg_bit(rtsusb->usb_base, IE_EP0IN_OFFSET, USB_IRQ_EN);
		mc_clr_reg_bit(rtsusb->mc_base, U_BUF0_EP0_TX_EN_OFFSET,
			       R_EP0_MC_BUF_CTL);
	} else {
		usb_clr_reg_bit(rtsusb->usb_base, IE_EP0OUT_OFFSET, USB_IRQ_EN);
		mc_clr_reg_bit(rtsusb->mc_base, U_BUF0_EP0_RX_EN_OFFSET,
			       R_EP0_MC_BUF_CTL);
	}
	rts_set_cxdone(priv_ep->rts_dev);

	priv_req->request.complete(&priv_ep->endpoint, &priv_req->request);

	if (priv_ep->rts_dev->request_pending > 0)
		--priv_ep->rts_dev->request_pending;
	priv_ep->stopped = stopped;
}

/**
 * rts_ep_disable
 */
static int rts_ep_disable(struct usb_ep *ep)
{
	struct rts_endpoint *priv_ep;
	struct rts_request *priv_req;

	RTS_DEBUG("%s()\n", __func__);

	BUG_ON(!ep);

	priv_ep = ep_to_rts_ep(ep);

	while (!list_empty(&priv_ep->queue)) {
		priv_req = list_entry(priv_ep->queue.next, struct rts_request,
				      queue);
		rts_done(priv_ep, priv_req, -ECONNRESET);
	}
	return 0;
}

/**
 * rts_ep_alloc_request Allocates request
 * @ep: endpoint object associated with request
 * @gfp_flags: gfp flags
 *
 * Returns allocated request address, NULL on allocation error
 */
static struct usb_request *rts_ep_alloc_request(struct usb_ep *ep,
						gfp_t gfp_flags)
{
	struct rts_request *priv_req;

	RTS_DEBUG("%s()\n", __func__);

	priv_req = malloc(sizeof(*priv_req));
	if (!priv_req)
		return NULL;

	INIT_LIST_HEAD(&priv_req->queue);
	return &priv_req->request;
}

/**
 * rts_ep_free_request Free memory occupied by request
 * @ep: endpoint object associated with request
 * @request: request to free memory
 */
static void rts_ep_free_request(struct usb_ep *ep, struct usb_request *request)
{
	struct rts_request *priv_req = to_rts_request(request);

	RTS_DEBUG("%s()\n", __func__);
	kfree(priv_req);
}

static void rts_start_ep0_transfer(struct rts_endpoint *priv_ep,
				   struct rts_request *priv_req)
{
	struct rts_udc *rtsusb = &controller;
	u32 *buffer;
	u16 length;
	int i;

	RTS_DEBUG("%s() -> ep%d\n", __func__, priv_ep->epnum);

	if (priv_ep->dir_in) {
		usb_set_reg_bit(rtsusb->usb_base, IE_EP0IN_OFFSET, USB_IRQ_EN);
		RTS_DEBUG("%s() ep0 in request.length %d\n", __func__,
			  priv_req->request.length);
		buffer = priv_req->request.buf + priv_req->request.actual;
		length = priv_req->request.length - priv_req->request.actual;
		if (length > priv_ep->endpoint.maxpacket)
			length = priv_ep->endpoint.maxpacket;
		RTS_DEBUG("ep0 in length %d\n", length);
		for (i = 0; i <= (length - 1) / 4; i++, buffer++)
			mc_write_reg(rtsusb->mc_base, *buffer,
				     EP0_BASE + i * 4);

		mc_write_reg(rtsusb->mc_base, length & 0xff, R_EP0_MC_BUF_BC);
		priv_req->ep0_in_last_length = length;
		mc_set_reg_bit(rtsusb->mc_base, U_BUF0_EP0_TX_EN_OFFSET,
			       R_EP0_MC_BUF_CTL);
	} else {
		usb_set_reg_bit(rtsusb->usb_base, IE_EP0OUT_OFFSET, USB_IRQ_EN);
		RTS_DEBUG("%s() ep0 out\n", __func__);
		mc_set_reg_bit(rtsusb->mc_base, U_BUF0_EP0_RX_EN_OFFSET,
			       R_EP0_MC_BUF_CTL);
	}
}

static int rts_ep0_queue(struct rts_endpoint *priv_ep,
			 struct rts_request *priv_req)
{
	int ret = 0;

	RTS_DEBUG("%s()\n", __func__);

	if (!priv_req->request.length) {
		rts_done(priv_ep, priv_req, 0);
		return ret;
	}

	rts_start_ep0_transfer(priv_ep, priv_req);

	return ret;
}

/**
 * rts_ep_queue Transfer data on endpoint zero
 * @ep: pointer to endpoint zero object
 * @request: pointer to request object
 * @gfp_flags: gfp flags
 *
 * Returns 0 on success, error code elsewhere
 */
static int rts_ep_queue(struct usb_ep *ep, struct usb_request *request,
			gfp_t gfp_flags)
{
	struct rts_endpoint *priv_ep = ep_to_rts_ep(ep);
	struct rts_request *priv_req = to_rts_request(request);
	int ret = 0;

	RTS_DEBUG("%s() ep%d in %d out %d\n", __func__, priv_ep->epnum,
		  priv_ep->dir_in, priv_ep->dir_out);

	list_add_tail(&priv_req->queue, &priv_ep->queue);
	++priv_ep->rts_dev->request_pending;

	priv_req->request.actual = 0;
	priv_req->request.status = -EINPROGRESS;

	if (!priv_ep->epnum) /* ep0 */
		ret = rts_ep0_queue(priv_ep, priv_req);

	return ret;
}

static int rts_ep_dequeue(struct usb_ep *ep, struct usb_request *request)
{
	struct rts_endpoint *priv_ep = ep_to_rts_ep(ep);
	struct rts_request *priv_req = to_rts_request(request);

	RTS_DEBUG("%s()\n", __func__);

	if (!list_empty(&priv_ep->queue))
		rts_done(priv_ep, priv_req, -ECONNRESET);

	return 0;
}

static void rts_set_epnstall(struct rts_endpoint *priv_ep)
{
	struct rts_udc *rtsusb = &controller;
	u32 epnum = priv_ep->epnum;

	RTS_DEBUG("%s() ep %d\n", __func__, priv_ep->epnum);

	priv_ep->stall = 1;
	if (priv_ep->dir_in) {
		switch (epnum) {
		case 0:
			usb_set_reg_bit(rtsusb->usb_base, EP0_STALL_OFFSET,
					USB_EP_CTL0);
			break;
		default:
			break;
		}
	} else {
		switch (epnum) {
		case 0:
			usb_set_reg_bit(rtsusb->usb_base, EP0_STALL_OFFSET,
					USB_EP_CTL0);
			break;
		default:
			break;
		}
	}
}

static int rts_set_halt_and_wedge(struct usb_ep *ep, int value, int wedge)
{
	struct rts_endpoint *priv_ep = ep_to_rts_ep(ep);

	RTS_DEBUG("%s()\n", __func__);

	if (value) {
		rts_set_epnstall(priv_ep);
		priv_ep->stall = 1;
		priv_ep->stopped = 1;
		if (wedge)
			priv_ep->wedged = 1;
	} else {
		priv_ep->stall = 0;
		priv_ep->wedged = 0;
		priv_ep->stopped = 0;
	}

	return 0;
}

static int rts_ep_set_halt(struct usb_ep *ep, int value)
{
	RTS_DEBUG("%s()\n", __func__);
	return rts_set_halt_and_wedge(ep, value, 0);
}

static int rts_ep_set_wedge(struct usb_ep *ep)
{
	RTS_DEBUG("%s()\n", __func__);
	return rts_set_halt_and_wedge(ep, 1, 1);
}

static const struct usb_ep_ops rts_ep_ops = {
	.enable = rts_ep_enable,
	.disable = rts_ep_disable,

	.alloc_request = rts_ep_alloc_request,
	.free_request = rts_ep_free_request,

	.queue = rts_ep_queue,
	.dequeue = rts_ep_dequeue,

	.set_halt = rts_ep_set_halt,
	.set_wedge = rts_ep_set_wedge,
};

static struct usb_endpoint_descriptor ep0_desc = {
	.bLength = sizeof(struct usb_endpoint_descriptor),
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_CONTROL,
};

struct rts_udc controller = {
	.gadget = {
		.name = "rts_gadget",
		.ops = &rts_gadget_ops,
		.ep0 = &controller.ep0.endpoint,
		.speed = USB_SPEED_HIGH,
	},
	.ep0 = {
		.endpoint = {
			.name = "ep0",
			.ops = &rts_ep_ops,
			.maxpacket = USB_EP0_MAX_PKT_SIZE,
		},
		.rts_dev = &controller,
		.desc = &ep0_desc,
		.epnum = 0,
	},
};

/*
 * rts_set_cxstall
 * EP0 return host stall
 */
static void rts_set_cxstall(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);
	usb_set_reg_bit(rtsusb->usb_base, EP0_STALL_OFFSET, USB_EP_CTL0);
}

static void rts_usb_req_ep0_get_status(struct rts_udc *rtsusb,
				       struct usb_ctrlrequest *ctrl_req)
{
	u8 epnum;

	RTS_DEBUG("%s()\n", __func__);

	switch (ctrl_req->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		rtsusb->ep0_data = 1 << USB_DEVICE_SELF_POWERED;
		break;
	case USB_RECIP_INTERFACE:
		rtsusb->ep0_data = 0;
		break;
	case USB_RECIP_ENDPOINT:
		epnum = ctrl_req->wIndex & USB_ENDPOINT_NUMBER_MASK;
		if (epnum)
			rtsusb->ep0_data = 0;
		else
			rts_set_cxstall(rtsusb);
		break;
	default:
		rts_set_cxstall(rtsusb);
		return;
	}
	rtsusb->ep0_req->buf = &rtsusb->ep0_data;
	rtsusb->ep0_req->length = 2;
	rtsusb->ep0_req->complete = rts_ep_free_request;

	rts_ep_queue(rtsusb->gadget.ep0, rtsusb->ep0_req, GFP_ATOMIC);
}

static void rts_usb_req_ep0_clear_feature(struct rts_udc *rtsusb,
					  struct usb_ctrlrequest *ctrl_req)
{
	struct rts_endpoint *ep;

	if (ctrl_req->wIndex & USB_ENDPOINT_DIR_MASK)
		ep = &rtsusb->ep0;
	else
		ep = &rtsusb->ep0;

	RTS_DEBUG("%s()\n", __func__);

	switch (ctrl_req->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		rts_set_cxdone(rtsusb);
		break;
	case USB_RECIP_INTERFACE:
		rts_set_cxdone(rtsusb);
		break;
	case USB_RECIP_ENDPOINT:
		if (ctrl_req->wIndex & USB_ENDPOINT_NUMBER_MASK) {
			if (ep->wedged) {
				rts_set_cxdone(rtsusb);
				break;
			}
			if (ep->stall)
				rts_set_halt_and_wedge(&ep->endpoint, 0, 0);
		}
		rts_set_cxdone(rtsusb);
		break;
	default:
		rts_set_cxstall(rtsusb);
		break;
	}
}

static void rts_usb_req_ep0_set_feature(struct rts_udc *rtsusb,
					struct usb_ctrlrequest *ctrl_req)
{
	RTS_DEBUG("%s()\n", __func__);

	switch (ctrl_req->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		rts_set_cxdone(rtsusb);
		break;
	case USB_RECIP_INTERFACE:
		rts_set_cxdone(rtsusb);
		break;
	case USB_RECIP_ENDPOINT: {
		u8 epnum;
		struct rts_endpoint *ep;

		epnum = ctrl_req->wIndex & USB_ENDPOINT_NUMBER_MASK;
		ep = &rtsusb->ep0;

		if (epnum)
			rts_set_epnstall(ep);
		else
			rts_set_cxstall(rtsusb);
		rts_set_cxdone(rtsusb);
	} break;
	default:
		rts_set_cxstall(rtsusb);
		break;
	}
}

static void rts_set_dev_addr(struct rts_udc *rtsusb, u32 addr)
{
	u32 value = usb_read_reg(rtsusb->usb_base, USB_ADDR);

	RTS_DEBUG("%s()\n", __func__);

	value |= (addr & 0x7f);
	if (usb_read_reg(rtsusb->usb_base, USB_ADDR) & 0x7f) {
		rts_set_cxdone(rtsusb);
		usb_write_reg(rtsusb->usb_base, value, USB_ADDR);
	} else {
		usb_write_reg(rtsusb->usb_base, value, USB_ADDR);
		rts_set_cxdone(rtsusb);
	}
}

static void rts_usb_req_ep0_set_address(struct rts_udc *rtsusb,
					struct usb_ctrlrequest *ctrl_req)
{
	RTS_DEBUG("%s()\n", __func__);

	if (ctrl_req->wValue >= 0x100)
		rts_set_cxstall(rtsusb);
	else
		rts_set_dev_addr(rtsusb, ctrl_req->wValue);
}

static void rts_usb_req_ep0_set_configuration(struct rts_udc *rtsusb,
					      struct usb_ctrlrequest *ctrl_req)
{
	RTS_DEBUG("%s()\n", __func__);
	// usb_set_reg_bit(rtsusb->usb_base, FORCE_DEVADDR_OFFSET, USB_ADDR);
}

static int rts_usb_ep0_standard_request(struct rts_udc *rtsusb,
					struct usb_ctrlrequest *ctrl_req)
{
	int ret = 0;

	RTS_DEBUG("%s()\n", __func__);

	switch (ctrl_req->bRequest) {
	case USB_REQ_GET_STATUS:
		RTS_DEBUG("\nget_status\n");
		rts_usb_req_ep0_get_status(rtsusb, ctrl_req);
		break;
	case USB_REQ_CLEAR_FEATURE:
		RTS_DEBUG("\nclear_feature\n");
		rts_usb_req_ep0_clear_feature(rtsusb, ctrl_req);
		break;
	case USB_REQ_SET_FEATURE:
		RTS_DEBUG("\nset_feature\n");
		rts_usb_req_ep0_set_feature(rtsusb, ctrl_req);
		break;
	case USB_REQ_SET_ADDRESS:
		RTS_DEBUG("\nset_address\n");
		rts_usb_req_ep0_set_address(rtsusb, ctrl_req);
		break;
	case USB_REQ_SET_CONFIGURATION:
		RTS_DEBUG("\nset_configuration\n");
		rts_usb_req_ep0_set_configuration(rtsusb, ctrl_req);
		ret = 1;
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static int rts_usb_ep0_vendor_requset(struct rts_udc *rtsusb,
				      struct usb_ctrlrequest *ctrl_req)
{
	int ret = 0;

	RTS_DEBUG("%s()\n", __func__);

	switch (ctrl_req->bRequest) {
	case USB_VDREQUEST_MS_VENDOR_CODE:
		RTS_DEBUG("\nget_ms\n");
		rtsusb->ep0_req_msos->buf = msos;
		rtsusb->ep0_req_msos->length = 382;
		rtsusb->ep0_req_msos->complete = rts_ep_free_request;

		rts_ep_queue(rtsusb->gadget.ep0, rtsusb->ep0_req_msos,
			     GFP_ATOMIC);
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static int rts_usb_setup_process(struct rts_udc *rtsusb)
{
	struct usb_ctrlrequest *ctrl = rtsusb->setup_buf;
	int ret = 1;

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->ep0.dir_in = ctrl->bRequestType & USB_DIR_IN;
	rtsusb->gadget.speed = USB_SPEED_HIGH;
	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		ret = rts_usb_ep0_standard_request(rtsusb, ctrl);
		break;
	case USB_TYPE_VENDOR:
		ret = rts_usb_ep0_vendor_requset(rtsusb, ctrl);
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static void rts_ep0_transfer_process(struct rts_endpoint *priv_ep)
{
	struct rts_udc *rtsusb = &controller;
	struct rts_request *priv_req;
	u32 *buffer;
	u16 length;
	int i;

	assert(!priv_ep);
	RTS_DEBUG("%s() -> ep%d\n", __func__, priv_ep->epnum);

	if (list_empty(&priv_ep->queue))
		return;
	priv_req = list_entry(priv_ep->queue.next, struct rts_request, queue);

	if (priv_req->request.length != priv_req->request.actual) {
		if (priv_ep->dir_in) {
			priv_req->request.actual +=
				priv_req->ep0_in_last_length;
		} else {
			buffer = priv_req->request.buf +
				 priv_req->request.actual;
			length = priv_req->request.length -
				 priv_req->request.actual;
			if (length > priv_ep->endpoint.maxpacket)
				length = priv_ep->endpoint.maxpacket;
			for (i = 0; i <= (length - 1) / 4; i++) {
				*buffer = mc_read_reg(rtsusb->mc_base,
						      EP0_BASE + i * 4);
				buffer++;
			}
			priv_req->request.actual += length;
			RTS_DEBUG("continue actual %d request.length %d\n",
				  priv_req->request.actual,
				  priv_req->request.length);
		}
	}

	if (priv_req->request.length == priv_req->request.actual ||
	    priv_req->request.actual < priv_ep->endpoint.maxpacket)
		rts_done(priv_ep, priv_req, 0);
	else
		rts_start_ep0_transfer(priv_ep, priv_req);
}

/**
 * rts_usb_init
 * init usb irq, configs
 */
static int rts_usb_init(struct rts_udc *rtsusb)
{
	RTS_DEBUG("%s()\n", __func__);

	usb_write_reg(rtsusb->usb_base, 0, USB_IRQ_EN);
	mc_write_reg(rtsusb->mc_base, 0, R_EP0_MC_BUF_CTL);
	mc_write_reg(rtsusb->mc_base, 0x1, R_MC_DEV_CFG);
	usb_set_reg_bit(rtsusb->usb_base, IE_SETUP_OFFSET, USB_IRQ_EN);
	usb_write_reg(rtsusb->usb_base, 0xFFFFFFFF, USB_IRQ_STATUS);
	usb_set_reg_bit(rtsusb->usb_base, EP0_RESET_OFFSET, USB_EP_CTL0);
	usb_write_reg(rtsusb->usb_base, USB_EP0_MAX_PKT_SIZE, USB_EP_MAXPKT0);
	usb_clr_reg_bit(rtsusb->usb_base, EP0_NAKOUT_MODE_OFFSET, USB_EP_CFG0);

	return 0;
}

/*
 * rts_phy_event - Called by phy upon VBus event
 * @action: phy action, is vbus connect or disconnect
 * Called by the USB Phy when a cable connect or disconnect is sensed.
 *
 * Returns 0
 */
int rts_phy_event(unsigned long action)
{
	struct rts_udc *rtsusb = &controller;

	RTS_DEBUG("%s()\n", __func__);

	switch (action) {
	case USB_EVENT_VBUS:
		usb_gadget_vbus_connect(&rtsusb->gadget);
		break;
	case USB_EVENT_NONE:
		usb_gadget_vbus_disconnect(&rtsusb->gadget);
		break;
	}

	return 0;
}

int dm_usb_gadget_handle_interrupts(struct udevice *dev)
{
	int ret = 0;
	struct rts_udc *rtsusb = &controller;
	u32 int_val;

	RTS_DEBUG("%s()\n", __func__);

	int_val = (usb_read_reg(rtsusb->usb_base, USB_IRQ_EN) &
		   usb_read_reg(rtsusb->usb_base, USB_IRQ_STATUS));
	RTS_DEBUG("interrupt_val %#x\n", int_val);
	if (int_val & BIT(I_SETUPF_OFFSET)) {
		usb_set_reg_bit(rtsusb->usb_base, I_EP0OUTF_OFFSET,
				USB_IRQ_STATUS);
		RTS_DEBUG("\nrecieve setup irq\n");

		rtsusb->setup_buf->bRequestType =
			usb_read_reg(rtsusb->usb_base, USB_EP0_SETUP_DATA0) &
			0xff;
		rtsusb->setup_buf->bRequest =
			(usb_read_reg(rtsusb->usb_base, USB_EP0_SETUP_DATA0) >>
			 8) &
			0xff;
		rtsusb->setup_buf->wValue =
			(usb_read_reg(rtsusb->usb_base, USB_EP0_SETUP_DATA0) >>
			 16) &
			0xffff;
		rtsusb->setup_buf->wIndex =
			usb_read_reg(rtsusb->usb_base, USB_EP0_SETUP_DATA1) &
			0xffff;
		rtsusb->setup_buf->wLength =
			(usb_read_reg(rtsusb->usb_base, USB_EP0_SETUP_DATA1) >>
			 16) &
			0xffff;

		usb_set_reg_bit(rtsusb->usb_base, I_SETUPF_OFFSET,
				USB_IRQ_STATUS);
		ret = rts_usb_setup_process(rtsusb);
		if (ret) {
			if (rtsusb->gadget_driver->setup(&rtsusb->gadget,
							 rtsusb->setup_buf) < 0)
				rts_set_cxstall(rtsusb);
		}
	}

	if (int_val & BIT(I_EP0INF_OFFSET)) {
		RTS_DEBUG("\nrecieve ep0 in transmitted irq\n");
		usb_set_reg_bit(rtsusb->usb_base, I_EP0INF_OFFSET,
				USB_IRQ_STATUS);
		rts_ep0_transfer_process(&rtsusb->ep0);
	}

	if (int_val & BIT(I_EP0OUTF_OFFSET)) {
		RTS_DEBUG("\nrecieve ep0 out received irq\n");
		usb_set_reg_bit(rtsusb->usb_base, I_EP0OUTF_OFFSET,
				USB_IRQ_STATUS);
		rts_ep0_transfer_process(&rtsusb->ep0);
	}

	return ret;
}

static int rts_usb_device_probe(struct udevice *dev)
{
	int ret = 0;
	struct rts_udc *rtsusb = &controller;
	struct rts_udc_plat *plat = dev_get_plat(dev);

	RTS_DEBUG("%s()\n", __func__);

	rtsusb->usb_base = plat->base_addr;
	rtsusb->mc_base = plat->mc_base_addr;
	rts_usb_phy_init(rtsusb->usb_base);

	rtsusb->setup_buf = malloc(sizeof(struct usb_ctrlrequest));
	if (!rtsusb->setup_buf) {
		debug("rtsusb: malloc setup_buf failed\n");
		return -ENOMEM;
	}
	rtsusb->ep0_req =
		rts_ep_alloc_request(&rtsusb->ep0.endpoint, GFP_KERNEL);
	if (!rtsusb->ep0_req) {
		ret = -ENOMEM;
		goto err_request;
	}
	rtsusb->ep0_req_msos =
		rts_ep_alloc_request(&rtsusb->ep0.endpoint, GFP_KERNEL);
	if (!rtsusb->ep0_req_msos) {
		ret = -ENOMEM;
		goto err1;
	}
	ret = usb_add_gadget_udc((struct device *)dev, &rtsusb->gadget);
	if (ret) {
		debug("rts usb init failed\n");
		goto err;
	}
	/*
	 * enable usb phy clk
	 */
	writel(1, USBPHY_CLK_CFG_REG);
	/*
	 * initialize usb register
	 */
	ret = rts_usb_init(rtsusb);
	if (ret) {
		debug("rts usb init failed\n");
		goto err;
	}
	INIT_LIST_HEAD(&rtsusb->gadget.ep_list);
	INIT_LIST_HEAD(&rtsusb->ep0.queue);

	rtsusb->ep0.stopped = 0;
	return ret;
err:
	rts_ep_free_request(&rtsusb->ep0.endpoint, rtsusb->ep0_req_msos);
err1:
	rts_ep_free_request(&rtsusb->ep0.endpoint, rtsusb->ep0_req);
err_request:
	rts_gadget_free_ep0(rtsusb);
	free(rtsusb->setup_buf);
	return ret;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static int rts_udc_ofdata_to_platdata(struct udevice *dev)
{
	struct rts_udc_plat *plat = dev_get_plat(dev);

	plat->base_addr = dev_read_addr_index(dev, 0);
	plat->mc_base_addr = dev_read_addr_index(dev, 1);
	return 0;
}

static const struct udevice_id rts_udc_ids[] = {
	{ .compatible = "realtek,rts3917-usb-device" },
	{},
};
#endif

U_BOOT_DRIVER(rts_udc) = {
	.name = "rts_udc",
	.id = UCLASS_USB_GADGET_GENERIC,
#if CONFIG_IS_ENABLED(OF_CONTROL)
	.of_match = rts_udc_ids,
	.of_to_plat = rts_udc_ofdata_to_platdata,
	.plat_auto = sizeof(struct rts_udc_plat),
#endif
	.probe = rts_usb_device_probe,
};
