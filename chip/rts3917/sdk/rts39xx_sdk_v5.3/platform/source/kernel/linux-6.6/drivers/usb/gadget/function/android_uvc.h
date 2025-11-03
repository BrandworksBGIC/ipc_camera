#ifndef _ANDROID_UVC_H
#define _ANDROID_UVC_H

#define UVC_DT_HEADER_CUSTOMED_SIZE	(12+UVC_STREAMING_NUMS)

/* some are copied from drivers/media/usb/uvc/uvcvideo.h */
#define UVC_GUID_UVC_EXTENSION \
	{0x8C, 0xA7, 0x29, 0x12, 0xB4, 0x47, 0x94, 0x40,\
	 0xB0, 0xCE, 0xDB, 0x07, 0x38, 0x6F, 0xB9, 0x38}
#define UVC_GUID_FORMAT_YUY2 \
	{ 'Y',  'U',  'Y',  '2', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_NV12 \
	{ 'N',  'V',  '1',  '2', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_MJPEG \
	{ 'M',  'J',  'P',  'G', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define UVC_GUID_FORMAT_H264 \
	{ 'H',  '2',  '6',  '4', 0x00, 0x00, 0x10, 0x00, \
	 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

struct uvc_dma_buf {
	void *vm_addr;
	void *kvm_addr;
	uint32_t length;
	uint32_t bytesused;
	uint32_t buf_io;
	uint32_t phy_addr;
	uint32_t direction;
};

int rtsx_uvc_function_bind(struct usb_configuration *c);
int rtsx_uvc_function_init(struct f_uvc_opts *opts);
void rtsx_uvc_function_cleanup(struct f_uvc_opts *opts);
int rtsx_fill_format_map(int strm_idx, int fmt_idx, u32 format,
		u16 w, u16 h, u32 still_image);
#ifndef CONFIG_USB_CONFIGFS
int uvc_get_stream_descs_num(int stream);
int uvc_stream_empty(int stream);
int uvc_get_stream_descs_len(int stream);
int uvc_copy_stream_descs(int stream, void **mem,
	struct usb_descriptor_header ***dst);

int rtsx_uvc_add_config(struct usb_composite_dev *cdev,
			struct usb_configuration *config,
			int (*bind)(struct usb_configuration *));
#endif

extern int rts_get_metadata_support(void);
#endif
