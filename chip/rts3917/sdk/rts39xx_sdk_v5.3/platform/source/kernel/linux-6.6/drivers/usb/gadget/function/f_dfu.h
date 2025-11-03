/*
 * f_dfu.h - Utility definitions for DFU function
 *
 * Copyright (C) 2025 Rui Feng <rui_feng@realsil.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __U_DFU_H
#define __U_DFU_H

#include <linux/usb/composite.h>
#include <linux/cdev.h>

//#define DBG(dev, fmt, args...) printk(KERN_INFO "[%s]\t"fmt, __func__, ##args)
//#define VDBG(dev, fmt, args...) printk(KERN_INFO "[%s]\t"fmt, __func__, ##args)


enum {
	STATE_INIT = 0,
	STATE_CHECKING,
	STATE_BURNING,
	STATE_UPDATE_DONE,
	STATE_UPDATE_READY,
	STATE_WRONG,
};

struct dfu_appstate {
	uint8_t state;
};

struct dfu_event;
#define RTSX_DFU_IOC_MAGIC	0x77

#define DFU_IOC_SET_STATE       _IOW(RTSX_DFU_IOC_MAGIC, 0x01, \
					struct dfu_appstate)
#define DFU_IOC_GET_STATE       _IOW(RTSX_DFU_IOC_MAGIC, 0x02, __u32)
#define DFU_IOC_GET_EVENT	_IOR(RTSX_DFU_IOC_MAGIC, \
		0x03, struct dfu_event)
#define DFU_IOC_SOFT_RESET       _IOW(RTSX_DFU_IOC_MAGIC, 0x04, __u32)

#define DFU_DT_FUNC			0x21

#define DFU_BIT_WILL_DETACH		BIT(3)
#define DFU_BIT_MANIFESTATION_TOLERANT	BIT(2)
#define DFU_BIT_CAN_UPLOAD		BIT(1)
#define DFU_BIT_CAN_DNLOAD		BIT(0)

#define DFU_USB_BUFSIZ			4096
#define DFU_BLEN			(1024 * 4)
#define DFU_DEFAULT_POLL_TIMEOUT	1000
#define DFU_MANIFEST_POLL_TIMEOUT       25000

#define USB_REQ_DFU_DETACH		0x00
#define USB_REQ_DFU_DNLOAD		0x01
#define USB_REQ_DFU_UPLOAD		0x02
#define USB_REQ_DFU_GETSTATUS		0x03
#define USB_REQ_DFU_CLRSTATUS		0x04
#define USB_REQ_DFU_GETSTATE		0x05
#define USB_REQ_DFU_ABORT		0x06

#define DFU_STATUS_OK			0x00
#define DFU_STATUS_errTARGET		0x01
#define DFU_STATUS_errFILE		0x02
#define DFU_STATUS_errWRITE		0x03
#define DFU_STATUS_errERASE		0x04
#define DFU_STATUS_errCHECK_ERASED	0x05
#define DFU_STATUS_errPROG		0x06
#define DFU_STATUS_errVERIFY		0x07
#define DFU_STATUS_errADDRESS		0x08
#define DFU_STATUS_errNOTDONE		0x09
#define DFU_STATUS_errFIRMWARE		0x0a
#define DFU_STATUS_errVENDOR		0x0b
#define DFU_STATUS_errUSBR		0x0c
#define DFU_STATUS_errPOR		0x0d
#define DFU_STATUS_errUNKNOWN		0x0e
#define DFU_STATUS_errSTALLEDPKT	0x0f

#define RET_STALL			-1
#define RET_ZLP				0
#define RET_STAT_LEN			6

enum dfu_state {
	DFU_STATE_appIDLE		= 0,
	DFU_STATE_appDETACH		= 1,
	DFU_STATE_dfuIDLE		= 2,
	DFU_STATE_dfuDNLOAD_SYNC	= 3,
	DFU_STATE_dfuDNBUSY		= 4,
	DFU_STATE_dfuDNLOAD_IDLE	= 5,
	DFU_STATE_dfuMANIFEST_SYNC	= 6,
	DFU_STATE_dfuMANIFEST		= 7,
	DFU_STATE_dfuMANIFEST_WAIT_RST	= 8,
	DFU_STATE_dfuUPLOAD_IDLE	= 9,
	DFU_STATE_dfuERROR		= 10,
};

struct dfu_status {
	__u8				bStatus;
	__u8				bwPollTimeout[3];
	__u8				bState;
	__u8				iString;
} __packed;

struct dfu_function_descriptor {
	__u8				bLength;
	__u8				bDescriptorType;
	__u8				bmAttributes;
	__le16				wDetachTimeOut;
	__le16				wTransferSize;
	__le16				bcdDFUVersion;
} __packed;

#define DFU_POLL_TIMEOUT_MASK           (0xFFFFFFUL)

struct f_dfu_req_list {
	void			*buf;
	unsigned int		length;
	int			blk_seq_num;
	struct list_head	list;
};

#define GDFU_EVENT_MAX		16
#define GDFU_EVENT_DATA_LEN	64

enum dfu_ev {
	DFU_EVENT_DETACH = 0,
};

struct dfu_event {
	__u8			type;
	__u8			data[GDFU_EVENT_DATA_LEN];
};

struct dfu_kevent {
	struct list_head	list;
	struct dfu_event	event;
};

struct dfu_dev;
struct dfu_fh {
	struct dfu_dev		*dev;

	/* Events */
	wait_queue_head_t	wait;
	struct list_head	available;
	unsigned int		navailable;
	spinlock_t		fh_lock;
};

struct dfu_dev {
	spinlock_t		lock;		/* lock this structure */
	/* lock buffer lists during read/write calls */
	struct mutex		lock_dfu_io;
	struct usb_gadget	*gadget;
	struct usb_function	func;
	unsigned int		blen;
	unsigned int		buf_n;

	struct list_head	rx_buffers;	/* List of completed xfers */
	/* wait until there is data to be read. */
	wait_queue_head_t	rx_wait;
	struct f_dfu_req_list	*current_rx_req;
	size_t			current_rx_bytes;
	u8			*current_rx_buf;
	struct cdev		dfu_cdev;
	u8			dfu_cdev_open;

	enum dfu_state		dfu_state;
	unsigned int		dfu_status;
	struct dfu_appstate	dfu_appstate;

	/* Send/received block number */
	int			blk_seq_num;
	unsigned int		poll_timeout;

	struct dfu_fh		fh;
};

struct f_dfu_opts {
	struct usb_function_instance	func_inst;
	char				*iname;
	struct mutex			lock;
	int				refcnt;
};
#endif /* __U_DFU_H */
