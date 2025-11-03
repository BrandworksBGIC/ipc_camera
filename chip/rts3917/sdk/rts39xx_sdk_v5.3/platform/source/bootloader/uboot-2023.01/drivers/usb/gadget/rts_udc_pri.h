/* SPDX-License-Identifier: GPL-2.0-only
 *
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 */
#ifndef __DRIVERS_USB_REALTEK_UDC_PRI_H__
#define __DRIVERS_USB_REALTEK_UDC_PRI_H__

#define RTS_USB_DRIVER_VERSION "v0.0"

// #define RTS_USB_DEBUG
#define RTS_USB "---RTS_USB: "
#define RTS_USBPHY "~~~~RTS_USBPHY: "
#ifdef RTS_USB_DEBUG
#define RTS_DEBUG(x...) printf(RTS_USB x)
#define RTS_PHY_DEBUG(x...) printf(RTS_USBPHY x)
#else
#define RTS_DEBUG(x...)
#define RTS_PHY_DEBUG(x...)
#endif

#define gadget_to_rts(g) (container_of(g, struct rts_udc, gadget))
#define ep_to_rts_ep(ep) (container_of(ep, struct rts_endpoint, endpoint))
#define to_rts_request(r) (container_of(r, struct rts_request, request))

#define USB_EP0_MAX_PKT_SIZE 0x40

#define USB_EVENT_NONE 0
#define USB_EVENT_VBUS 1

#define USB_VDREQUEST_MS_VENDOR_CODE 0x15
#define MSOS20_SET_HEADER_LEN 0x0A
#define MSOS20_CONFIG_SUBSET_HEADER_LEN 0x08
#define MSOS20_FUNCTION_SUBSET_HEADER_LEN 0x08
#define MS_CMPATIBLE_ID_LEN 0x14
#define DEV_INTF_GUID_LEN 0x80

//wPropertyDataType
#define RESERVED 0x00
#define REG_SZ 0x01
#define REG_EXPAND_SZ 0x02
#define REG_BINARY 0x03
#define REG_DWORD_LITTLE_ENDIAN 0x04
#define REG_DWORD_BIG_ENDIAN 0x05
#define REG_LINK 0x06
#define REG_MULTI_SZ 0x07

#define DEV_IDEL_ENABLE_LEN 0x32
#define DEF_IDEL_STATE_LEN 0x32
#define DEF_IDEL_TMOUT_LEN 0x36
#define DEV_IDEL_IGNWK_LEN 0x46
#define MSOS20_SET_HEADER_LEN 0x0A
#define MSOS20_CONFIG_SUBSET_HEADER_LEN 0x08
#define MSOS20_FUNCTION_SUBSET_HEADER_LEN 0x08
#define MS_CMPATIBLE_ID_LEN 0x14
#define DEV_INTF_GUID_LEN 0x80

// Bos Desc
// Microsoft OS 2.0 descriptor types
#define MS_OS_20_SET_HEADER_DESCRIPTOR 0x00
#define MS_OS_20_SUBSET_HEADER_CONFIGURATION 0x01
#define MS_OS_20_SUBSET_HEADER_FUNCTION 0x02
#define MS_OS_20_FEATURE_COMPATBLE_ID 0x03
#define MS_OS_20_FEATURE_REG_PROPERTY 0x04
#define MS_OS_20_FEATURE_MIN_RESUME_TIME 0x05
#define MS_OS_20_FEATURE_MODEL_ID 0x06
#define MS_OS_20_FEATURE_CCGP_DEVICE 0x07

#define MSOS20_SET_HEADER_LEN 0x0A
#define MSOS20_CONFIG_SUBSET_HEADER_LEN 0x08
#define MSOS20_FUNCTION_SUBSET_HEADER_LEN 0x08

// Bos Desc
#define MSOS_DFU_SUBSET_LEN                                             \
	(MSOS20_FUNCTION_SUBSET_HEADER_LEN + MS_CMPATIBLE_ID_LEN +      \
	 DEV_INTF_GUID_LEN + DEV_IDEL_ENABLE_LEN + DEF_IDEL_STATE_LEN + \
	 DEF_IDEL_TMOUT_LEN + DEV_IDEL_IGNWK_LEN)
#define MSOS_FP_SUBSET_LEN MSOS_DFU_SUBSET_LEN
#define MSOS20_CONFIG_SUBSET_LEN                                 \
	(MSOS20_CONFIG_SUBSET_HEADER_LEN + MSOS_DFU_SUBSET_LEN + \
	 MSOS_FP_SUBSET_LEN)
#define MSOS20_SET_TOTAL_LEN                                       \
	(MSOS20_CONFIG_SUBSET_HEADER_LEN + MSOS20_SET_HEADER_LEN + \
	 MSOS_DFU_SUBSET_LEN + MSOS_FP_SUBSET_LEN)
#define MSOS20_SET_DFU_TOTAL_LEN                                           \
	(MSOS20_SET_HEADER_LEN + MS_CMPATIBLE_ID_LEN + DEV_INTF_GUID_LEN + \
	 DEV_IDEL_ENABLE_LEN + DEF_IDEL_STATE_LEN + DEF_IDEL_TMOUT_LEN +   \
	 DEV_IDEL_IGNWK_LEN)

/*-------------------------------------------------------------------------*/
/* Used structs */
struct rts_udc;

struct rts_endpoint {
	struct usb_ep endpoint;
	struct rts_udc *rts_dev;
	unsigned char epnum;

	struct list_head queue;
	unsigned stall : 1;
	unsigned wedged : 1;
	bool stopped;

	unsigned char dir_in;
	unsigned char dir_out;

	const struct usb_endpoint_descriptor *desc;
};

struct rts_request {
	struct usb_request request;
	struct list_head queue;
	u16 ep0_in_last_length;
};

struct rts_udc {
	struct usb_gadget gadget;
	struct usb_gadget_driver *gadget_driver;

	struct usb_ctrlrequest *setup_buf;

	unsigned int irq;
	struct usb_phy *phy;

	enum usb_device_state state;

	u32 usb_base;
	u32 mc_base;

	struct rts_endpoint ep0;
	struct usb_request *ep0_req;
	struct usb_request *ep0_req_msos;
	__le16 ep0_data;

	/* gadget start */
	u8 gadgetstart;
	/* vbus on */
	u8 vbuson;
	/** bypass pullup **/
	u32 request_pending;
};

struct rts_usb_phy {
	struct rts_usb_phy_regs __iomem *regs;
	struct rts_usb_dev_phy_regs __iomem *dev_regs;
	struct mutex mutex;
#ifdef CONFIG_USB_RTS_PHY_DEBUG
	struct cdev cdev;
#endif
};

int rts_phy_event(unsigned long action);
int rts_usb_phy_init(u32 usb_base);

#endif /* __DRIVERS_USB_REALTEK_UDC_PRI_H__ */
