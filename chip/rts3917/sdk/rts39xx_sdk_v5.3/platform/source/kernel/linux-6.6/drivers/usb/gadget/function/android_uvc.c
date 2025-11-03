/*
 *	rtscam.c -- USB rtscam gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/usb/video.h>
#include <linux/usb/g_uvc.h>
#include <linux/delay.h>
#include "f_uvc.h"
#include "uvc.h"
#include "u_uvc.h"
#include "android_uvc.h"

/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */

int rlx_uvc_debug = RLX_TRACE_DEFAULT;
EXPORT_SYMBOL_GPL(rlx_uvc_debug);
module_param(rlx_uvc_debug, int, 0644);
MODULE_PARM_DESC(rlx_uvc_debug, "activates debug info");

/* string IDs are assigned dynamically */

#define STRING_DESCRIPTION_IDX		USB_GADGET_FIRST_AVAIL_IDX

struct format_setting {
	u32 type;
	u16 width;
	u16 height;
	u32 fps;
	u32 stream_idx;
	u32 still_image;
};

struct format_setting_list {
	struct list_head list;
	struct format_setting fts;
};

static struct f_uvc_opts *uvc_opts;
/* tmp store user added formats */
static LIST_HEAD(tmp_desc_list);

#define FORMAT_MAX	(4)
static u32 formatmap[UVC_STREAMING_NUMS][FORMAT_MAX];

#ifndef CONFIG_USB_CONFIGFS
static struct usb_configuration *uvcconf;
static struct usb_composite_dev *uvccdev;
static int (*uvcbind)(struct usb_configuration *);
/* store in stream & format order */
static struct list_head uvc_desc_list[UVC_STREAMING_NUMS];

static u8 fpsbitmap[16] = {
	SENSOR_FPS_120,
	SENSOR_FPS_60,
	SENSOR_FPS_30,
	SENSOR_FPS_25,
	SENSOR_FPS_24,
	SENSOR_FPS_23,
	SENSOR_FPS_20,
	SENSOR_FPS_15,
	SENSOR_FPS_12,
	SENSOR_FPS_11,
	SENSOR_FPS_10,
	SENSOR_FPS_9,
	SENSOR_FPS_8,
	SENSOR_FPS_7,
	SENSOR_FPS_5,
	SENSOR_FPS_3,
};
/* VC Interface */

/* customed structure for n-way replace */
struct uvc_header_descriptor_customed {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubType;
	__u16 bcdUVC;
	__u16 wTotalLength;
	__u32 dwClockFrequency;
	__u8  bInCollection;
	__u8  baInterfaceNr[UVC_STREAMING_NUMS];
} __attribute__((__packed__));

static struct uvc_header_descriptor_customed
uvc_control_header = {
	.bLength			= UVC_DT_HEADER_CUSTOMED_SIZE,
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_HEADER,
#ifdef CONFIG_USB_RTSX_UVC_15
	.bcdUVC				= cpu_to_le16(0x0150),
#else
	.bcdUVC				= cpu_to_le16(0x0110),
#endif
	.wTotalLength			= 0,	/* dynamic */
	.dwClockFrequency		= cpu_to_le32(DEV_CLOCK_FRQ),
	.bInCollection			= 0,	/* dynamic */
	/* .baInterfaceNr[N] are all dynamic for every stream */
};

/* Enabled CT Controls:
 * D1: Auto-Exposure Mode
 * D2: Auto-Exposure Priority
 * D3: Exposure Time(Absolute)
 */
static struct uvc_camera_terminal_descriptor
uvc_camera_terminal = {
	.bLength			= UVC_DT_CAMERA_TERMINAL_SIZE(3),
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_INPUT_TERMINAL,
	.bTerminalID			= ENT_ID_CAMERA_TERMINAL,
	.wTerminalType			= cpu_to_le16(UVC_ITT_CAMERA),
	.bAssocTerminal			= 0,
	.iTerminal			= 0,
	.wObjectiveFocalLengthMin	= cpu_to_le16(0),
	.wObjectiveFocalLengthMax	= cpu_to_le16(0),
	.wOcularFocalLength		= cpu_to_le16(0),
	.bControlSize			= 3,
	.bmControls[0]			= 0x0e,
	.bmControls[1]			= 0x00,
	.bmControls[2]			= 0x00,
};

/* Enabled PU Controls:
 * D0:  Brightness
 * D1:  Contrast
 * D2:  Hue
 * D3:  Saturation
 * D4:  Sharpness
 * D5:  Gamma
 * D6:  White Balance Temperature
 * D10: Power Line Frequency
 * D12: White Balance Temperature, Auto
 */
#ifdef CONFIG_USB_RTSX_UVC_15
static struct uvc_processing_unit_descriptor uvc_processing_unit = {
	.bLength			= UVC_DT_PROCESSING_UNIT_SIZE(3),
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_PROCESSING_UNIT,
	.bUnitID			= ENT_ID_PROCESSING_UNIT,
	.bSourceID			= ENT_ID_CAMERA_TERMINAL,
	.wMaxMultiplier			= 0,
	.bControlSize			= 3,
	.bmControls[0]			= 0x7f,
	.bmControls[1]			= 0x14,
	.bmControls[2]			= 0,
	.iProcessing			= 0,
	.bmVideoStandards		= 0,
};
#else
static struct uvc_processing_unit_descriptor uvc_processing_unit = {
	.bLength			= UVC_DT_PROCESSING_UNIT_SIZE(2),
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_PROCESSING_UNIT,
	.bUnitID			= ENT_ID_PROCESSING_UNIT,
	.bSourceID			= ENT_ID_CAMERA_TERMINAL,
	.wMaxMultiplier			= 0,
	.bControlSize			= 2,
	.bmControls[0]			= 0x7f,
	.bmControls[1]			= 0x14,
	.iProcessing			= 0,
	.bmVideoStandards		= 0,/*This bit is added after UVC1.1*/
};
#endif

static const struct uvc_output_terminal_descriptor
uvc_output_terminal = {
	.bLength			= UVC_DT_OUTPUT_TERMINAL_SIZE,
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_OUTPUT_TERMINAL,
	.bTerminalID			= ENT_ID_OUTPUT_TERMINAL,
	.wTerminalType			= cpu_to_le16(UVC_TT_STREAMING),
	.bAssocTerminal			= 0,
	.bSourceID			= ENT_ID_EXTENSION_UNIT,
	.iTerminal			= 0,
};

#ifdef CONFIG_USB_RTSX_UVC_15
static const struct uvc_encode_unit_descriptor
uvc_encode_unit = {
	.bLength			= UVC_DT_ENCODE_UNIT_SIZE,
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VC_ENCODING_UNIT,
	.bUnitID			= ENT_ID_ENCODING_UNIT,
	.bSourceID			= ENT_ID_PROCESSING_UNIT,
	.iEncoding			= 0x00,
	.bControlSize			= 0x03,
	.bmControls[0]			= 0x00,
	.bmControls[1]			= 0x00,
	.bmControls[2]			= 0x00,
	.bmControlsRuntime[0]		= 0x00,
	.bmControlsRuntime[1]		= 0x00,
	.bmControlsRuntime[2]		= 0x00,
};
#endif

/* VS Interface */

/* customed structure for n-format replace */
struct uvc_input_header_descriptor_customed {
	__u8  bLength;
	__u8  bDescriptorType;
	__u8  bDescriptorSubType;
	__u8  bNumFormats;
	__u16 wTotalLength;
	__u8  bEndpointAddress;
	__u8  bmInfo;
	__u8  bTerminalLink;
	__u8  bStillCaptureMethod;
	__u8  bTriggerSupport;
	__u8  bTriggerUsage;
	__u8  bControlSize;
	__u8  bmaControls[1][FORMAT_MAX];
} __attribute__((__packed__));

#define UVC_DT_INPUT_HEADER_CUSTOMED_SIZE	(13+FORMAT_MAX)

static struct uvc_input_header_descriptor_customed
uvc_streaming_header[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		.bLength		= UVC_DT_INPUT_HEADER_CUSTOMED_SIZE,
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubType	= UVC_VS_INPUT_HEADER,
		.bNumFormats		= 1,	/* dynamic */
		.wTotalLength		= 0,	/* dynamic */
		.bEndpointAddress	= 0,	/* dynamic */
		.bmInfo			= 0,
		.bTerminalLink		= ENT_ID_OUTPUT_TERMINAL,
		.bStillCaptureMethod	= 0,
		.bTriggerSupport	= 0,
		.bTriggerUsage		= 0,
		.bControlSize		= 1,
		/* .bmaControls[0][0 ... FORMAT_MAX - 1] = 0, */
	}
};

static struct uvc_format_mjpeg uvc_format_mjpg[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		.bLength		= UVC_DT_FORMAT_MJPEG_SIZE,
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubType	= UVC_VS_FORMAT_MJPEG,
		.bFormatIndex		= 0,	/* dynamic */
		.bNumFrameDescriptors	= 0,	/* dynamic */
		.bmFlags		= 1,	/* fixed size samples */
		.bDefaultFrameIndex	= 1,
		.bAspectRatioX		= 0,
		.bAspectRatioY		= 0,
		.bmInterfaceFlags	= 0,
		.bCopyProtect		= 0,
	}
};

static struct uvc_format_uncompressed uvc_format_yuy2[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
		.bFormatIndex		= 0,	/* dynamic */
		.bNumFrameDescriptors	= 0,	/* dynamic */
		.guidFormat		= UVC_GUID_FORMAT_YUY2,
		.bBitsPerPixel		= 16,
		.bDefaultFrameIndex	= 1,
		.bAspectRatioX		= 0,
		.bAspectRatioY		= 0,
		.bmInterfaceFlags	= 0,
		.bCopyProtect		= 0,
	}
};

static struct uvc_format_uncompressed uvc_format_nv12[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		.bLength		= UVC_DT_FORMAT_UNCOMPRESSED_SIZE,
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubType	= UVC_VS_FORMAT_UNCOMPRESSED,
		.bFormatIndex		= 0,	/* dynamic */
		.bNumFrameDescriptors	= 0,	/* dynamic */
		.guidFormat		= UVC_GUID_FORMAT_NV12,
		.bBitsPerPixel		= 12,
		.bDefaultFrameIndex	= 1,
		.bAspectRatioX		= 0,
		.bAspectRatioY		= 0,
		.bmInterfaceFlags	= 0,
		.bCopyProtect		= 0,
	}
};

#ifdef CONFIG_USB_RTSX_UVC_15
struct uvc_format_h264 uvc_format_h264[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
	.bLength			= UVC_DT_FORMAT_H264_SIZE,
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= VS_FORMAT_H264,
	.bFormatIndex			= 0,	/* dynamic */
	.bNumFrameDescriptors		= 0,	/* dynamic */
	.bDefaultFrameIndex		= 1,
	.bMaxCodecConfigDelay		= 0x01,
	.bmSupportedSliceModes		= 0x00,
	.bmSupportedSyncFrameTypes	= 0x03,
	.bResolutionScaling		= 0x03,
	.Reserved1			= 0x00,
	.bmSupportedRateControlModes	= H264_RATE_CONTROL_MODE,
	.wMaxMBperSecOneResolutionNoScalability			= 0x6C00,
	.wMaxMBperSecTwoResolutionsNoScalability		= 0x0000,
	.wMaxMBperSecThreeResolutionsNoScalability		= 0x0000,
	.wMaxMBperSecFourResolutionsNoScalability		= 0x0000,
	.wMaxMBperSecOneResolutionTemporalScalability		= 0x6C00,
	.wMaxMBperSecTwoResolutionsTemporalScalablility		= 0x0000,
	.wMaxMBperSecThreeResolutionsTemporalScalability	= 0x0000,
	.wMaxMBperSecFourResolutionsTemporalScalability		= 0x0000,
	.wMaxMBperSecOneResolutionTemporalQualityScalability	= 0x0000,
	.wMaxMBperSecTwoResolutionsTemporalQualityScalability	= 0x0000,
	.wMaxMBperSecThreeResolutionsTemporalQualityScalability = 0x0000,
	.wMaxMBperSecFourResolutionsTemporalQualityScalability	= 0x0000,
	.wMaxMBperSecOneResolutionTemporalSpatialScalability	= 0x0000,
	.wMaxMBperSecTwoResolutionsTemporalSpatialScalability	= 0x0000,
	.wMaxMBperSecThreeResolutionsTemporalSpatialScalability	= 0x0000,
	.wMaxMBperSecFourResolutionsTemporalSpatialScalability	= 0x0000,
	.wMaxMBperSecOneResolutionFullScalability		= 0x0000,
	.wMaxMBperSecTwoResolutionsFullScalability		= 0x0000,
	.wMaxMBperSecThreeResolutionsFullScalability		= 0x0000,
	.wMaxMBperSecFourResolutionsFullScalability		= 0x0000,
	}
};
#else
struct uvc_format_h264 uvc_format_h264[UVC_STREAMING_NUMS] = {
	[0 ... UVC_STREAMING_NUMS - 1] = {
		.bLength		= UVC_DT_FORMAT_H264_SIZE,
		.bDescriptorType	= USB_DT_CS_INTERFACE,
		.bDescriptorSubType	= UVC_VS_FORMAT_FRAME_BASED,
		.bFormatIndex		= 0,	/* dynamic */
		.bNumFrameDescriptors	= 0,	/* dynamic */
		.guidFormat		= UVC_GUID_FORMAT_H264,
		.bBitsPerPixel		= 16,	/* encode YUY2 */
		.bDefaultFrameIndex	= 1,
		.bVariableSize		= 1,	/* data size variable */
	}
};
#endif

static const struct uvc_color_matching_descriptor
uvc_color_matching = {
	.bLength			= UVC_DT_COLOR_MATCHING_SIZE,
	.bDescriptorType		= USB_DT_CS_INTERFACE,
	.bDescriptorSubType		= UVC_VS_COLORFORMAT,
	.bColorPrimaries		= 1,
	.bTransferCharacteristics	= 1,
	.bMatrixCoefficients		= 4,
};

struct uvc_descriptor_header *
	uvc_fs_control_cls[BASIC_ENTITY_NUMS + UVC_STREAMING_NUMS + 1] = {
	[0] = (struct uvc_descriptor_header *)&uvc_control_header,
	[1] = (struct uvc_descriptor_header *)&uvc_camera_terminal,
	[2] = (struct uvc_descriptor_header *)&uvc_processing_unit,
#ifdef CONFIG_USB_RTSX_UVC_15
	[3] = (struct uvc_descriptor_header *)&uvc_encode_unit,
#endif
	[BASIC_ENTITY_NUMS ... BASIC_ENTITY_NUMS + UVC_STREAMING_NUMS - 1] =
		(struct uvc_descriptor_header *)&uvc_output_terminal,
	[BASIC_ENTITY_NUMS + UVC_STREAMING_NUMS] = NULL,
};
EXPORT_SYMBOL_GPL(uvc_fs_control_cls);

static unsigned int bitconts(unsigned int u)
{
	unsigned int ret = 0;

	while (u) {
		u = (u & (u - 1));
		ret++;
	}

	return ret;
}

static void init_frame_desc(struct uvc_frame_uncompressed *desc,
		struct format_setting *fts, u8 frameindex)
{
	u32 pixels;
	u8 bFrameIntervalNum;
	u8 minfps, maxfps;
	u32 fps;
	int i;

	fps = fts->fps;
	bFrameIntervalNum = bitconts(fps);
	pixels = fts->width * fts->height;
	minfps = fpsbitmap[fls(fps) - 1];
	maxfps = fpsbitmap[ffs(fps) - 1];

	desc->bLength = UVC_DT_FRAME_UNCOMPRESSED_SIZE(bFrameIntervalNum);
	desc->bDescriptorType = USB_DT_CS_INTERFACE;
	desc->bFrameIndex = frameindex;
	desc->bmCapabilities = 0;
	desc->wWidth = cpu_to_le16(fts->width);
	desc->wHeight = cpu_to_le16(fts->height);
	desc->dwMinBitRate = cpu_to_le32(pixels * minfps << 4);
	desc->dwMaxBitRate = cpu_to_le32(pixels * maxfps << 4);
	desc->dwMaxVideoFrameBufferSize = cpu_to_le32(pixels << 1);
	desc->dwDefaultFrameInterval = cpu_to_le32(10000000 / maxfps);
	desc->bFrameIntervalType = bFrameIntervalNum;
	for (i = 0; i < bFrameIntervalNum; i++) {
		desc->dwFrameInterval[i] =
		    cpu_to_le32(10000000 / (fpsbitmap[ffs(fps) - 1]));
		fps &= ~(1 << (ffs(fps) - 1));

	}
}

static void init_h264_frame_desc(struct uvc_frame_h264 *desc,
		struct format_setting *fts, u8 frameindex)
{
	u32 pixels;
	u8 bFrameIntervalNum;
	u8 minfps, maxfps;
	u32 fps;
	int i;

	fps = fts->fps;
	bFrameIntervalNum = bitconts(fps);
	pixels = fts->width * fts->height;
	minfps = fpsbitmap[fls(fps) - 1];
	maxfps = fpsbitmap[ffs(fps) - 1];

	desc->bLength = UVC_DT_FRAME_H264_SIZE(bFrameIntervalNum);
	desc->bDescriptorType = USB_DT_CS_INTERFACE;
	desc->bFrameIndex = frameindex;
#ifdef CONFIG_USB_RTSX_UVC_15
	desc->wSARwidth = 0x0001;
	desc->wSARheight = 0x0001;
	desc->wProfile = 0x6400;
	desc->bLevelIDC = 40;
	desc->wConstrainedToolset = 0;
	desc->bmSupportedUsages[0] = 0x01;
	desc->bmSupportedUsages[1] = 0x00;
	desc->bmSupportedUsages[2] = 0x01;
	desc->bmSupportedUsages[3] = 0x00;
	desc->bmCapabilities[0] = H264_ENC_CAPBILITY;
	desc->bmCapabilities[1] = 0x00;
	desc->bmSVCCapabilities[0] = 0;
	desc->bmSVCCapabilities[1] = 0;
	desc->bmSVCCapabilities[2] = 0;
	desc->bmSVCCapabilities[3] = 0;
	desc->bmMVCCapabilities[0] = 0;
	desc->bmMVCCapabilities[1] = 0;
	desc->bmMVCCapabilities[2] = 0;
	desc->bmMVCCapabilities[3] = 0;
	desc->bNumFrameIntervals = bFrameIntervalNum;
#else
	desc->bmCapabilities = 2; /* Fixed Frame-rate */
	desc->bFrameIntervalType = bFrameIntervalNum;
	desc->dwBytesPerLine = 0; /* Variable Size */
#endif
	desc->wWidth = cpu_to_le16(fts->width);
	desc->wHeight = cpu_to_le16(fts->height);
	desc->dwMinBitRate = cpu_to_le32(pixels * minfps << 4);
	desc->dwMaxBitRate = cpu_to_le32(pixels * maxfps << 4);
	desc->dwDefaultFrameInterval = cpu_to_le32(10000000 / maxfps);
	for (i = 0; i < bFrameIntervalNum; i++) {
		desc->dwFrameInterval[i] =
		    cpu_to_le32(10000000 / (fpsbitmap[ffs(fps) - 1]));
		fps &= ~(1 << (ffs(fps) - 1));
	}
}

int uvc_get_stream_descs_num(int stream)
{
	struct desc_entry *q;
	int count = 0;

	list_for_each_entry(q, &uvc_desc_list[stream], list) {
		count++;
	}

	return count;
}
EXPORT_SYMBOL_GPL(uvc_get_stream_descs_num);

int uvc_stream_empty(int stream)
{
	struct desc_entry *q;
	int count = 0;

	list_for_each_entry(q, &uvc_desc_list[stream], list) {
		if (++count > 1)
			return 0;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(uvc_stream_empty);

int uvc_get_stream_descs_len(int stream)
{
	struct desc_entry *q;
	int len = 0;

	list_for_each_entry(q, &uvc_desc_list[stream], list) {
		len += q->desc->bLength;
	}

	return len;
}
EXPORT_SYMBOL_GPL(uvc_get_stream_descs_len);

int uvc_copy_stream_descs(int stream, void **mem,
	struct usb_descriptor_header ***dst)
{
	struct desc_entry *q;
	int len = 0;

	list_for_each_entry(q, &uvc_desc_list[stream], list) {
		len += q->desc->bLength;
		/* UVC_COPY_DESCRIPTOR */
		memcpy(*mem, q->desc, q->desc->bLength);
		*(*dst)++ = *mem;
		*mem += q->desc->bLength;
	}

	return len;
}
EXPORT_SYMBOL_GPL(uvc_copy_stream_descs);

/*
 * The first five bytes of format descriptors and first four bytes of
 * frame descriptors are the same between different formats, so use struct
 * uvc_format_uncompressed and uvc_frame_uncompressed for convenience
 */
static void init_format_desc(u32 format, int *fmt_idx)
{
	int init[UVC_STREAMING_NUMS] = {0}, size = 0, i;
	u8 frameindex[UVC_STREAMING_NUMS] = {0};
	u32 subtype = 0;
	struct format_setting_list *q;
	struct desc_entry *desc = NULL;
	struct uvc_format_uncompressed *fmt_desc[UVC_STREAMING_NUMS] = {0};
	struct uvc_frame_uncompressed *frame_desc = NULL;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		switch (format) {
		case V4L2_PIX_FMT_NV12:
			fmt_desc[i] = &uvc_format_nv12[i];
			break;
		case V4L2_PIX_FMT_YUYV:
			fmt_desc[i] = &uvc_format_yuy2[i];
			break;
		case V4L2_PIX_FMT_MJPEG:
			fmt_desc[i] = (struct uvc_format_uncompressed *)
				&uvc_format_mjpg[i];
			break;
		case V4L2_PIX_FMT_H264:
			fmt_desc[i] = (struct uvc_format_uncompressed *)
				&uvc_format_h264[i];
			break;
		default:
			return;
		}
	}

	list_for_each_entry(q, &tmp_desc_list, list) {
		i = q->fts.stream_idx;
		if (q->fts.type == format) {
			if (init[i] == 0) {
				uvc_streaming_header[i].bLength++;
				desc = kzalloc(sizeof(*desc), GFP_KERNEL);
				if (!desc)
					continue;

				desc->desc = (struct uvc_descriptor_header *)
						fmt_desc[i];
				fmt_desc[i]->bFormatIndex = ++fmt_idx[i];
				list_add_tail(&desc->list, &uvc_desc_list[i]);
				init[i] = 1;
			}

			desc = kzalloc(sizeof(*desc), GFP_KERNEL);
			if (!desc)
				continue;

			switch (format) {
			case V4L2_PIX_FMT_NV12:
			case V4L2_PIX_FMT_YUYV:
				size = UVC_DT_FRAME_UNCOMPRESSED_SIZE(
						bitconts(q->fts.fps));
				subtype = VS_FRAME_UNCOMPRESSED;
				break;
			case V4L2_PIX_FMT_MJPEG:
				size = UVC_DT_FRAME_MJPEG_SIZE(
						bitconts(q->fts.fps));
				subtype = VS_FRAME_MJPEG;
				break;
			case V4L2_PIX_FMT_H264:
				size = UVC_DT_FRAME_H264_SIZE(
						bitconts(q->fts.fps));
#ifdef CONFIG_USB_RTSX_UVC_15
				subtype = VS_FRAME_H264;
#else
				subtype = VS_FRAME_FRAME_BASED;
#endif
				break;
			default:
				format = VS_FRAME_VENDOR;
				break;
			}

			frame_desc = kzalloc(size, GFP_KERNEL);
			if (!frame_desc)
				continue;

			frame_desc->bDescriptorSubType = subtype;
			desc->desc = (struct uvc_descriptor_header *)frame_desc;
			desc->desc_alloc = 1;

			if (format == V4L2_PIX_FMT_H264)
				init_h264_frame_desc(
					(struct uvc_frame_h264 *)
					desc->desc, &(q->fts), ++frameindex[i]);
			else
				init_frame_desc(
					(struct uvc_frame_uncompressed *)
					desc->desc, &(q->fts), ++frameindex[i]);

			list_add_tail(&desc->list, &uvc_desc_list[i]);
		}
	}

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		fmt_desc[i]->bNumFrameDescriptors = frameindex[i];
		if (init[i]) {
			desc = kzalloc(sizeof(*desc), GFP_KERNEL);
			if (!desc)
				return;

			desc->desc = (struct uvc_descriptor_header *)
				&uvc_color_matching;
			list_add_tail(&desc->list, &uvc_desc_list[i]);
			formatmap[i][fmt_idx[i] - 1] = format;
		}
	}
}

static void produce_streaming_desc(void)
{
	int i, fmt_idx[UVC_STREAMING_NUMS] = {0};
	struct desc_entry *desc = NULL;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		desc = kzalloc(sizeof(*desc), GFP_KERNEL);
		if (!desc)
			return;

		desc->desc = (struct uvc_descriptor_header *)
			&uvc_streaming_header[i];
		list_add_tail(&desc->list, &uvc_desc_list[i]);
	}

	init_format_desc(V4L2_PIX_FMT_NV12,  fmt_idx);
	init_format_desc(V4L2_PIX_FMT_YUYV,  fmt_idx);
	init_format_desc(V4L2_PIX_FMT_MJPEG, fmt_idx);
	init_format_desc(V4L2_PIX_FMT_H264,  fmt_idx);

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		uvc_streaming_header[i].bLength =
			UVC_DT_INPUT_HEADER_SIZE(fmt_idx[i], 1);
		uvc_streaming_header[i].bNumFormats = fmt_idx[i];
	}
}

void __clean_stream_desc(void)
{
	int i;
	struct desc_entry *q, *q_tmp;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i) {
		list_for_each_entry_safe(q, q_tmp, &uvc_desc_list[i], list) {
			list_del(&q->list);

			if (q->desc_alloc)
				kfree(q->desc);
			kfree(q);
		}
	}
}

int rtsx_uvc_add_config(struct usb_composite_dev *cdev,
			struct usb_configuration *config,
			int (*bind)(struct usb_configuration *))
{
	uvcconf = config;
	uvccdev = cdev;
	uvcbind = bind;
	return 0;
}
EXPORT_SYMBOL_GPL(rtsx_uvc_add_config);
#endif

int rtsx_fill_format_map(int strm_idx, int fmt_idx, u32 format,
		u16 w, u16 h, u32 still_image)
{
	struct format_setting_list *fsl = NULL;

	if (strm_idx > UVC_STREAMING_NUMS)
		return -EINVAL;

	formatmap[strm_idx][fmt_idx] = format;
	fsl = kzalloc(sizeof(*fsl), GFP_KERNEL);
	if (!fsl)
		return -ENOMEM;

	fsl->fts.type = format;
	fsl->fts.width = w;
	fsl->fts.height = h;
	fsl->fts.fps = 0;//fps is not useful for configfs
	fsl->fts.stream_idx = strm_idx;
	fsl->fts.still_image = still_image;
	list_add_tail(&fsl->list, &tmp_desc_list);
	return 0;
}
EXPORT_SYMBOL_GPL(rtsx_fill_format_map);

static int rtscam_query_desc(int fmtindex,
	int frmindex, int still_image,
	struct format_setting *fts)
{
	struct format_setting_list *q = 0;
	int i = 1, stream = fts->stream_idx;
	int still_frame_index = 0;

	if (stream < 0 || stream > UVC_STREAMING_NUMS)
		return -1;

	if (fmtindex < 1 || fmtindex > FORMAT_MAX)
		return -1;

	if (formatmap[stream][fmtindex - 1] == 0)
		return -1;

	list_for_each_entry(q, &tmp_desc_list, list) {
		if (still_image == 0) {
			if (q->fts.stream_idx == stream &&
				q->fts.type ==
				formatmap[stream][fmtindex - 1]) {
				if (i++ == frmindex) {
					*fts = q->fts;
					return 0;
				}
			}
		} else {
			if (q->fts.type == formatmap[stream][fmtindex - 1]) {
				if (q->fts.still_image == 1)
					still_frame_index++;

				if (still_frame_index == frmindex) {
					*fts = q->fts;
					return 0;
				}
			}
		}
	}

	return -1;

}

static dev_t android_uvc_devno = MKDEV(243, 0);

static int android_uvc_open(struct inode *inode, struct file *file)
{
	struct uvc_device *uvc = container_of(inode->i_cdev,
			struct uvc_device, android_uvc_cdev);

	file->private_data = uvc;
	if (uvc_opts && uvc_opts->lock_module)
		uvc_opts->lock_module();

	return 0;
}

static int android_uvc_release(struct inode *inode, struct file *file)
{
	if (uvc_opts && uvc_opts->release_module)
		uvc_opts->release_module();

	return 0;
}

static long android_uvc_ioctl(struct file *file,
			      unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct uvc_device *uvc = file->private_data;

	switch (cmd) {
	case ANDROIDUVC_IOC_SETFORMAT:{
		struct format_setting_list *fsl = NULL;

		fsl = kzalloc(sizeof(*fsl), GFP_KERNEL);
		if (!fsl)
			return -ENOMEM;

		if (copy_from_user(&(fsl->fts), (char *)arg,
				   sizeof(struct format_setting))) {
			kfree(fsl);
			retval = -EFAULT;
		}

		/* drop invalid stream format settings */
		if (fsl->fts.stream_idx < 0 ||
			fsl->fts.stream_idx >= UVC_STREAMING_NUMS) {
			kfree(fsl);
			retval = -EFAULT;
			break;
		}

		list_add_tail(&fsl->list, &tmp_desc_list);
		break;
	}
	case ANDROIDUVC_IOC_PRODUCE_DESC:
#ifndef CONFIG_USB_CONFIGFS
		rlxprintk(RLX_TRACE_DEBUG, "produce uvc desc\n");
		produce_streaming_desc();
		usb_add_config(uvccdev, uvcconf, uvcbind);
#endif
		break;
	case ANDROIDUVC_IOC_QUERYFORMAT:{
		struct format_setting fs;

		if (copy_from_user(&fs, (char *)arg,
			sizeof(struct format_setting))) {
			retval = -EFAULT;
			break;
		}

		if (rtscam_query_desc(fs.width, fs.height, fs.still_image, &fs))
			return -EINVAL;

		retval = copy_to_user((char *)arg, &fs,
			     sizeof(struct format_setting));
		break;
	}
	case ANDROIDUVC_IOC_CLRFORMAT:{
		struct format_setting_list *fsl, *fsl_copy;

		if (list_empty(&tmp_desc_list))
			break;

		list_for_each_entry_safe(fsl, fsl_copy,
			&tmp_desc_list, list) {
			list_del(&fsl->list);
			kfree(fsl);
		}

		memset(formatmap, 0, sizeof(formatmap));
		break;
	}
	case ANDROIDUVC_IOC_WAIT_STREAM:{
		int stream, endpoint;
		struct usb_gadget *gadget = NULL;

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}

		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;

		if (!uvc->video[stream].ep)
			return -EINVAL;
		endpoint = uvc->video[stream].ep->address & 0x7f;
		gadget = uvc->video[stream].gadget;
		if (gadget && gadget->ops->ioctl)
			retval = gadget->ops->ioctl(gadget,
				ANDROIDUVC_IOC_WAIT_STREAM, endpoint);
		break;
	}
	case ANDROIDUVC_IOC_STREAM_INTF:{
		int stream;

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}

		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;

		retval = uvc->streaming_intfs[stream];
		break;
	}
	case ANDROIDUVC_IOC_CTL_INTF:{
		retval = uvc->control_intf;
		break;
	}
	case ANDROIDUVC_IOC_RESET_EP:{
		int stream;
		unsigned long flags;

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}

		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;

		usb_ep_disable(uvc->video[stream].ep);
		msleep(20);
		usb_ep_enable(uvc->video[stream].ep);
		spin_lock_irqsave(&(uvc->video[stream].queue).irqlock, flags);
		uvc->video[stream].queue.buf_used = 0;
		uvc->video[stream].payload_size = 0;
		uvc->video[stream].queue.flags &= ~UVC_QUEUE_DISCONNECTED;
		spin_unlock_irqrestore(&(uvc->video[stream].queue).irqlock,
					flags);
		break;
	}
	case ANDROIDUVC_IOC_GET_METADATA_SUPPORT:{
		int metadata_support = rts_get_metadata_support();

		retval = copy_to_user((char *)arg, &metadata_support,
					sizeof(int));
		break;
	}
	case ANDROIDUVC_IOC_STILL_IMAGE:{
		int stream, endpoint;
		struct usb_gadget *gadget = NULL;

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}
		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;
		if (!uvc->video[stream].ep)
			return -EINVAL;
		endpoint = uvc->video[stream].ep->address & 0x7f;
		gadget = uvc->video[stream].gadget;
		if (gadget && gadget->ops->ioctl)
			retval = gadget->ops->ioctl(gadget,
				ANDROIDUVC_IOC_STILL_IMAGE, endpoint);
		break;
	}
	case ANDROIDUVC_IOC_STILL_IMAGE_CLEAR:{
		int stream, endpoint;
		struct usb_gadget *gadget = NULL;

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}
		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;
		if (!uvc->video[stream].ep)
			return -EINVAL;
		endpoint = uvc->video[stream].ep->address & 0x7f;
		gadget = uvc->video[stream].gadget;
		if (gadget && gadget->ops->ioctl)
			retval = gadget->ops->ioctl(gadget,
				ANDROIDUVC_IOC_STILL_IMAGE_CLEAR, endpoint);
		break;
	}
	case ANDROIDUVC_IOC_SEND_METADATA:{
		int stream;

		if (!rts_get_metadata_support()) {
			retval = -EPERM;
			break;
		}

		if (copy_from_user(&stream, (char *)arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}
		if (stream < 0 || stream > UVC_STREAMING_NUMS - 1)
			return -EINVAL;
		if (copy_from_user(&uvc->video[stream].mbuf, (char *)arg,
				   sizeof(struct metadata_buffer))) {
			retval = -EFAULT;
			break;
		}
		if (uvc->video[stream].mbuf.length_actual > METADATA_LEN_MAX) {
			uvc->video[stream].mbuf.length_actual = 0;
			return -EINVAL;
		}
		break;
	}
	case ANDROIDUVC_IOC_ALLOC_BUF:{
		struct uvc_dma_buf buf;

		if (copy_from_user(&buf, (struct uvc_dma_buf *)arg,
					sizeof(struct uvc_dma_buf))) {
			retval = -EFAULT;
			break;
		}
		if (!buf.length) {
			retval = -EFAULT;
			break;
		}
		buf.kvm_addr = dma_alloc_coherent(&uvc->video[0].gadget->dev,
				PAGE_ALIGN(buf.length), &buf.phy_addr, GFP_KERNEL);
		if (!buf.kvm_addr) {
			retval = -EFAULT;
			break;
		}
		retval = copy_to_user((char *)arg, &buf,
			     sizeof(struct uvc_dma_buf));
		break;
	}
	case ANDROIDUVC_IOC_FREE_BUF:{
		struct uvc_dma_buf buf;

		if (copy_from_user(&buf, (struct uvc_dma_buf *)arg,
					sizeof(struct uvc_dma_buf))) {
			retval = -EFAULT;
			break;
		}
		if (!buf.kvm_addr || !buf.phy_addr || !buf.length) {
			retval = -EFAULT;
			break;
		}

		dma_free_coherent(&uvc->video[0].gadget->dev,
				buf.length, buf.kvm_addr, buf.phy_addr);
		break;
	}
	default:
		return -EINVAL;
	}

	return retval;
}

static int android_uvc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size;
	off_t phy_addr;
	int ret;

	phy_addr = vma->vm_pgoff << PAGE_SHIFT;
	size = vma->vm_end - vma->vm_start;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			size, vma->vm_page_prot);
	return ret;
}

static const struct file_operations android_uvc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = android_uvc_ioctl,
	.open = android_uvc_open,
	.release = android_uvc_release,
	.mmap = android_uvc_mmap,
};

static struct class *android_uvc_class;

int rtsx_uvc_function_init(struct f_uvc_opts *opts)
{
	int err;
	struct device *device;
	struct cdev *cdev = &opts->uvc->android_uvc_cdev;
#ifndef CONFIG_USB_CONFIGFS
	int i;

	for (i = 0; i < UVC_STREAMING_NUMS; ++i)
		INIT_LIST_HEAD(&uvc_desc_list[i]);
#endif
	android_uvc_class = class_create("android_uvc");
	if (IS_ERR(android_uvc_class)) {
		rlxprintk(RLX_TRACE_ERROR, "class_create fail\n");
		return PTR_ERR(android_uvc_class);
	}

	err = register_chrdev_region(android_uvc_devno, 1, "android_uvc");
	if (err) {
		rlxprintk(RLX_TRACE_ERROR, "register_chrdev_region fail\n");
		goto fail_register_chrdev;
	}

	cdev_init(cdev, &android_uvc_fops);
	cdev->owner = THIS_MODULE;

	err = cdev_add(cdev, android_uvc_devno, 1);
	if (err) {
		rlxprintk(RLX_TRACE_ERROR, "cdev_add fail\n");
		goto fail_cdev_add;
	}

	device = device_create(android_uvc_class, NULL, android_uvc_devno,
			NULL, "%s", "android_uvc");
	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		goto fail_device_create;
	}

	uvc_opts = opts;

	return 0;

fail_device_create:
	cdev_del(cdev);
fail_cdev_add:
	unregister_chrdev_region(android_uvc_devno, 1);
fail_register_chrdev:
	class_destroy(android_uvc_class);

	return err;
}
EXPORT_SYMBOL_GPL(rtsx_uvc_function_init);

void rtsx_uvc_function_cleanup(struct f_uvc_opts *opts)
{
	struct format_setting_list *fsl, *fsl_tmp;
	struct cdev *cdev = &opts->uvc->android_uvc_cdev;

	device_destroy(android_uvc_class, android_uvc_devno);
	cdev_del(cdev);
	unregister_chrdev_region(android_uvc_devno, 1);
	class_destroy(android_uvc_class);
#ifndef CONFIG_USB_CONFIGFS
	__clean_stream_desc();
#endif

	list_for_each_entry_safe(fsl, fsl_tmp, &tmp_desc_list, list) {
		list_del(&fsl->list);
		kfree(fsl);
	}

	uvc_opts = NULL;
}
EXPORT_SYMBOL_GPL(rtsx_uvc_function_cleanup);

MODULE_LICENSE("GPL");
