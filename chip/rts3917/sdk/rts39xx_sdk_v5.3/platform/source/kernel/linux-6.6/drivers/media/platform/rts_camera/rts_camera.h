/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef _RTS_CAMERA_H
#define _RTS_CAMERA_H

#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include "linux/rts_camera.h"
#include "rts_camera_fps.h"

#define RTSCAM_VIDEO_SUPPORT_MAX_STREAM_NUM	5

enum rtscam_size_type {
	RTSCAM_SIZE_DISCRETE = 0,
	RTSCAM_SIZE_STEPWISE = 1,
	RTSCAM_SIZE_CONTINUOUS = 2,
};

struct rtscam_frame_size {
	__u32 width;
	__u32 height;
};

struct rtscam_frame_frmival {
	struct v4l2_fract frmival;
	struct rtscam_frame_frmival *next;
};

struct rtscam_video_frmival {
	__u8 initialized;
	__u8 frmival_type;
	union {
		struct {
			struct rtscam_frame_frmival *frmivals;
		} discrete;
		struct {
			struct v4l2_fract max;
			struct v4l2_fract min;
			struct v4l2_fract step;
		} stepwise;
	};
};

struct rtscam_video_frame {
	struct rtscam_frame_size size;

	struct rtscam_video_frmival frmival;
	struct rtscam_video_frame *next;
};

struct rtscam_video_format {
	__u8 type;
	__u8 index;
	__u8 colorspace;
	__u8 field;
	const char name[32];
	__u32 fourcc;
	__u8 bpp;
	bool is_yuv;
	__u32 rts_code;

	__u8 initialized;

	__u8 frame_type;
	union {
		struct {
			struct rtscam_video_frame *frames;
		} discrete;
		struct {
			struct rtscam_frame_size max;
			struct rtscam_frame_size min;
			struct rtscam_frame_size step;
			struct rtscam_video_frmival frmival;
		} stepwise;
	};
	struct rtscam_video_format *next;
};

struct rtscam_video_format_xlate {
	__u8 type;
	__u8 index;
	__u8 colorspace;
	__u8 field;
	const char name[32];
	__u32 fourcc;
	__u8 bpp;
	bool is_yuv;
	__u32 rts_code;
};

enum {
	RTS_BUF_STATE_IDLE = 0,
	RTS_BUF_STATE_READY,
	RTS_BUF_STATE_HW,
	RTS_BUF_STATE_DELAYED,
	RTS_BUF_STATE_DONE,
	RTS_BUF_STATE_DEQUEUED,
	RTS_BUF_STATE_QUEUED,
	RTS_BUF_STATE_RESERVED
};

struct rtscam_video_buffer {
	struct vb2_v4l2_buffer		buf;
	struct list_head		list;
	__u32				pts;
	struct delayed_work		dwork;
	int				state;
};

struct rtscam_video_stream {
	struct rtscam_video_device *icd;

	__u8 streamid;	/*initialized by user*/

	int video_nr;	/*initialized by user*/

	struct video_device *vdev;
	struct vb2_queue vb2_vidp;

	spinlock_t	lock;
	struct mutex stream_lock;
	struct mutex queue_lock;
	struct list_head capture;

	struct rtscam_video_format *user_formats;	/*initialized by user*/
	struct rtscam_video_fps fps;

	__u32 rts_code;
	__u32 user_format;
	__u32 user_width;
	__u32 user_height;

	__u32 bytesperline;
	__u32 sizeimage;

	unsigned long sequence;
	unsigned long frame_count;
	unsigned long skip_count;
	unsigned long overflow_count;
	unsigned long error_count;
	unsigned long abnormal_count;

	atomic_t use_count;

	struct file *memory_owner;

	unsigned long delay;
	unsigned long delta;

	int fov_status;

	__u8 vin_mode;
	__u32 ring_buf_height;

	__u8 streaming;
	unsigned int memory;
};

static inline bool rtscam_is_streaming(struct rtscam_video_stream *stream)
{
	return stream->streaming;
}

struct rtscam_video_ctrl;

struct rtscam_video_ops {
	struct module *owner;

	int (*start_clock)(struct rtscam_video_device *icd);
	int (*stop_clock)(struct rtscam_video_device *icd);

	int (*get_ctrl)(struct rtscam_video_device *icd,
			struct rtscam_video_ctrl **ctrl, int index);
	int (*put_ctrl)(struct rtscam_video_device *icd,
			struct rtscam_video_ctrl *ctrl);
	int (*query_ctrl)(struct rtscam_video_device *icd, __u8 unit,
			  __u8 query, __u8 selector, __u8 length, __u8 *data);

	int (*init_capture_buffers)(struct rtscam_video_stream *stream);
	int (*submit_buffer)(struct rtscam_video_stream *stream,
			     struct rtscam_video_buffer *buffer);
	int (*s_stream)(struct rtscam_video_stream *stream, int enable);

	int (*set_selection)(struct rtscam_video_stream *stream,
				struct v4l2_selection *selection);
	int (*get_selection)(struct rtscam_video_stream *stream,
				struct v4l2_selection *selection);

	int (*exec_command)(struct rtscam_video_stream *stream,
			    struct rtscam_vcmd *pcmd);
};

struct rtscam_video_device {
	struct v4l2_device v4l2_dev;
	struct device *dev;		/*initialized by user*/

	const struct vb2_mem_ops *mem_ops;	/*initialized by user*/
	unsigned streamnum;		/*initialized by user*/
	/*initialized by user*/
	struct rtscam_video_stream streams[RTSCAM_VIDEO_SUPPORT_MAX_STREAM_NUM];
	struct rtscam_video_ops *ops;	/*initialized by user*/
	const char *drv_name;	/*initialized by user*/
	const char *dev_name;	/*initialized by user*/
	u32 type;		/*initialized by user*/

	struct mutex dev_lock;
	struct mutex reg_lock;

	struct mutex ctrl_lock;
	struct list_head controls;

	int streaming_count;

	atomic_t use_count;

	unsigned long flags;
	void *priv;	/*initialized by user*/
	int initialized;	/*initialized by user*/
};

static inline struct rtscam_video_device *to_rtscam_video_device(
		const struct device *dev)
{
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	return container_of(v4l2_dev, struct rtscam_video_device, v4l2_dev);
}

static inline struct rtscam_video_buffer *to_rtscam_vbuf(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *buf = to_vb2_v4l2_buffer(vb);

	return container_of(buf, struct rtscam_video_buffer, buf);
}

struct rtscam_region {
	u32 base;
	u32 size;
};

/* Ctrl flags */
#define RTS_CTRL_FLAG_SET_CUR			(1 << 0)
#define RTS_CTRL_FLAG_GET_CUR			(1 << 1)
#define RTS_CTRL_FLAG_GET_MIN			(1 << 2)
#define RTS_CTRL_FLAG_GET_MAX			(1 << 3)
#define RTS_CTRL_FLAG_GET_RES			(1 << 4)
#define RTS_CTRL_FLAG_GET_LEN			(1 << 5)
#define RTS_CTRL_FLAG_GET_INFO			(1 << 6)
#define RTS_CTRL_FLAG_GET_DEF			(1 << 7)
/* Control should be saved at suspend and restored at resume. */
#define RTS_CTRL_FLAG_RESTORE			(1 << 8)
#define RTS_CTRL_FLAG_AUTO_UPDATE		(1 << 9)

#define RTS_CTRL_FLAG_GET_RANGE		\
	(RTS_CTRL_FLAG_GET_CUR | RTS_CTRL_FLAG_GET_MIN | \
	 RTS_CTRL_FLAG_GET_MAX | RTS_CTRL_FLAG_GET_RES | \
	 RTS_CTRL_FLAG_GET_DEF)

#define RTS_CTRL_DATA_TYPE_RAW			0
#define RTS_CTRL_DATA_TYPE_SIGNED		1
#define RTS_CTRL_DATA_TYPE_UNSIGNED		2
#define RTS_CTRL_DATA_TYPE_BOOLEAN		3
#define RTS_CTRL_DATA_TYPE_ENUM			4
#define RTS_CTRL_DATA_TYPE_BITMASK		5

#define RTS_CTRL_DATA_CURRENT			0
#define RTS_CTRL_DATA_BACKUP			1
#define RTS_CTRL_DATA_MIN			2
#define RTS_CTRL_DATA_MAX			3
#define RTS_CTRL_DATA_RES			4
#define RTS_CTRL_DATA_DEF			5
#define RTS_CTRL_DATA_LAST			6

#define RTS_CTRL_SET_CUR			0x01
#define RTS_CTRL_GET_CUR			0x81
#define RTS_CTRL_GET_MIN			0x82
#define RTS_CTRL_GET_MAX			0x83
#define RTS_CTRL_GET_RES			0x84
#define RTS_CTRL_GET_LEN			0x85
#define RTS_CTRL_GET_INFO			0x86
#define RTS_CTRL_GET_DEF			0x87

struct rtscam_video_ctrl_info {
	struct list_head mappings;
	__u8 index;
	__u8 unit;
	__u8 selector;
	__u8 size;
	__u32 flags;
};

struct rtscam_video_ctrl_menu {
	__u32 value;
	__u8 name[32];
};

struct rtscam_video_ctrl_mapping {
	struct list_head list;

	__u32 id;
	__u8 name[32];
	__u8 unit;
	__u8 selector;

	__u8 size;
	__u8 offset;
	enum v4l2_ctrl_type v4l2_type;
	__u32 data_type;

	struct rtscam_video_ctrl_menu *menu_info;
	__u32 menu_count;

	__s32 (*get)(struct rtscam_video_ctrl_mapping *mapping, __u8 query,
		     const u8 *data);
	void (*set)(struct rtscam_video_ctrl_mapping *mapping, __s32 value,
		    __u8 *data);
};

struct rtscam_video_ctrl {
	struct list_head list;
	__u8 unit;
	struct rtscam_video_ctrl_info info;

	__u8 index;
	__u8 dirty: 1,
	     loaded: 1,
	     modified: 1,
	     cached: 1,
	     initialized: 1;

	__u8 *ctrl_data;
};

struct rtscam_ge_file_operations {
	struct module *owner;
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	unsigned int (*poll)(struct file *, struct poll_table_struct *);
	long (*ioctl)(struct file *, unsigned int, unsigned long);
	long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
	int (*open)(struct file *);
	int (*release)(struct file *);
	int (*mmap)(struct file *, struct vm_area_struct *);
	int (*fasync)(int fd, struct file *filp, int mode);
};

struct rtscam_ge_device {
	struct rtscam_ge_file_operations *fops;
	struct device dev;
	struct cdev cdev;

	struct device *parent;

	int minor;
	unsigned long flags;

	char name[32];
	int debug;

	void (*release)(struct rtscam_ge_device *rdev);

	struct fasync_struct *async_queue;

	struct mutex lock;
};

#define RTSCAM_FL_REGISTERED		(0)

#define to_rtscam_ge_device(cd) container_of(cd, struct rtscam_ge_device, dev)

#define RTS_TRACE_ERROR		(1 << 0)
#define RTS_TRACE_PROBE		(1 << 1)
#define RTS_TRACE_CTRL		(1 << 2)
#define RTS_TRACE_IOCTL		(1 << 3)
#define RTS_TRACE_BUF		(1 << 4)
#define RTS_TRACE_VIDEO		(1 << 5)
#define RTS_TRACE_STATUS	(1 << 6)
#define RTS_TRACE_DEBUG		(1 << 7)
#define RTS_TRACE_INFO		(1 << 8)
#define RTS_TRACE_NOTICE	(1 << 9)
#define RTS_TRACE_WARNING	(1 << 10)
#define RTS_TRACE_REG		(1 << 11)
#define RTS_TRACE_MEMINFO	(1 << 12)
#define RTS_TRACE_DEV		(1 << 13)
#define RTS_TRACE_COMMAND	(1 << 14)
#define RTS_TRACE_POWER		(1 << 15)
#define RTS_TRACE_PTS		(1 << 16)
#define RTS_TRACE_FPS		(1 << 17)
#define RTS_TRACE_WQ		(1 << 18)
#define RTS_TRACE_MD		(1 << 19)
#define RTS_TRACE_MCU		(1 << 20)


#define RTS_TRACE_DEFAULT	(RTS_TRACE_ERROR | RTS_TRACE_INFO | RTS_TRACE_MCU)

extern unsigned int rtscam_debug;

#ifndef TAG
#define TAG     "rtscam"
#endif

#define rtsprintk(flag, arg...) \
	do {\
		if (rtscam_debug & (flag)) {\
			if ((flag) & RTS_TRACE_ERROR) \
				pr_err(TAG":"arg);\
			else \
				pr_info(TAG":"arg);\
		}\
	} while(0)

int rtscam_ctrl_add_info(struct rtscam_video_ctrl *ctrl,
			 struct rtscam_video_ctrl_info *info);
void rtscam_ctrl_clr_info(struct rtscam_video_ctrl *ctrl);
int rtscam_ctrl_add_mapping(struct rtscam_video_ctrl *ctrl,
			    struct rtscam_video_ctrl_mapping *mapping);
void rtscam_ctrl_clr_mapping(struct rtscam_video_ctrl *ctrl);

int rtscam_video_register_device(struct rtscam_video_device *icd);
int rtscam_video_unregister_device(struct rtscam_video_device *icd);

struct rtscam_ge_device *rtscam_ge_device_alloc(void);
void rtscam_ge_device_release(struct rtscam_ge_device *rdev);
struct rtscam_ge_device *rtscam_devdata(struct file *filp);

static inline void *rtscam_ge_get_drvdata(struct rtscam_ge_device *rdev)
{
	return dev_get_drvdata(&rdev->dev);
}

static inline void rtscam_ge_set_drvdata(struct rtscam_ge_device *rdev,
					 void *data)
{
	dev_set_drvdata(&rdev->dev, data);
}

int rtscam_ge_register_device(struct rtscam_ge_device *rdev);
void rtscam_ge_unregister_device(struct rtscam_ge_device *rdev);

void rtscam_ge_kill_fasync(struct rtscam_ge_device *rdev, int sig, int band);

/**/
int rtscam_register_format(struct rtscam_video_stream *stream,
			   struct rtscam_video_format_xlate *pfmt);
int rtscam_register_frame_discrete(struct rtscam_video_stream *stream,
				   __u32 fourcc,
				   struct rtscam_frame_size *size);
int rtscam_register_frame_stepwise(struct rtscam_video_stream *stream,
				   __u32 fourcc,
				   struct rtscam_frame_size *max,
				   struct rtscam_frame_size *min,
				   struct rtscam_frame_size *step);
int rtscam_clr_format(struct rtscam_video_stream *stream, int keep);

int rtscam_get_phy_addr(struct vb2_buffer *vb,
		dma_addr_t *y_phy_addr, dma_addr_t *uv_phy_addr);

int rts_test_bit(const __u8 *data, int bit);
void rts_clear_bit(__u8 *data, int bit);

struct rtscam_video_buffer *rtscam_get_video_buffer(
		struct rtscam_video_stream *stream, int index);
int rtscam_push_back_ready_buffer(struct rtscam_video_stream *stream,
				  struct rtscam_video_buffer *buf);
int rtscam_submit_buffer(struct rtscam_video_stream *stream,
			 struct rtscam_video_buffer *buf);
void rtscam_buffer_done(struct rtscam_video_stream *stream,
			struct rtscam_video_buffer *rbuf,
			unsigned long bytesused);

long rtscam_video_do_ctrl_ioctl(struct rtscam_video_device *icd,
				unsigned int cmd, void *arg);

typedef long (*rtscam_kioctl)(struct file *file, unsigned int cmd, void *arg);

long rtscam_usercopy(struct file *filp, unsigned int cmd, unsigned long arg,
		     rtscam_kioctl func);

struct rtscam_video_format *find_format_by_fourcc(
		struct rtscam_video_stream *stream, u32 fourcc);

struct rtscam_video_frame *find_frame(struct rtscam_video_format *format,
				      __u32 width, __u32 height);

#define RTSCAM_L_BYTE(a)	((a) & 0xff)
#define RTSCAM_H_BYTE(a)	(((a) >> 8) & 0xff)
#define RTSCAM_L_WORD(a)	((a) & 0xffff)
#define RTSCAM_H_WORD(a)	(((a) >> 16) & 0xffff)

#define RTSCAM_ALIGN(x, a)	(((x) + (a) - 1) & (~((a) - 1)))

//#define DBG_ENCODE_TIME		1
#ifdef DBG_ENCODE_TIME
#define RING_BUFFER_ITEM_NUM	8
struct encode_time {
	long delta;
	int width;
	unsigned long cnt;
};

struct ring_buffer {
	struct encode_time etime[RING_BUFFER_ITEM_NUM];
	wait_queue_head_t  ewait;
	int rnum;
	int wnum;
};
int rtscam_init_ring_buf(struct ring_buffer *ring_buf);
int rtscam_write_ring_buf(struct ring_buffer *ring_buf, int width,
				long delta, unsigned long cnt);
int rtscam_read_ring_buf(struct ring_buffer *ring_buf, void __user *arg);
#endif

int rtscam_get_video_in_buf_status(int streamid, u32 *val);
int rtscam_set_video_in_buf_status(int streamid, u32 val);
int rtscam_check_pixel_cnt(void);

#endif
