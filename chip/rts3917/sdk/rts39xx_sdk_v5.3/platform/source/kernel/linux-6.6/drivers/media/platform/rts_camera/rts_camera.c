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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "rts_camera_priv.h"
#include "rts_camera_fps.h"

unsigned int rtscam_debug = RTS_TRACE_DEFAULT;
static unsigned int rts_clock_param = CLOCK_MONOTONIC;

EXPORT_SYMBOL_GPL(rtscam_debug);

module_param(rtscam_debug, uint, 0644);
MODULE_PARM_DESC(rtscam_debug, "activates debug info");

static int rts_clock_param_get(char *buffer, const struct kernel_param *kp)
{
	if (CLOCK_MONOTONIC == rts_clock_param)
		return sprintf(buffer, "CLOCK_MONOTONIC");
	else
		return sprintf(buffer, "CLOCK_REALTIME");
}

static int rts_clock_param_set(const char *val, const struct kernel_param *kp)
{
	if (strncasecmp(val, "clock_", strlen("clock_")) == 0)
		val += strlen("clock_");

	if (strcasecmp(val, "monotonic") == 0)
		rts_clock_param = CLOCK_MONOTONIC;
	else if (strcasecmp(val, "realtime") == 0)
		rts_clock_param = CLOCK_REALTIME;
	else
		return -EINVAL;

	return 0;
}

module_param_call(clock, rts_clock_param_set, rts_clock_param_get,
		  &rts_clock_param, 0644);
MODULE_PARM_DESC(clock, "Video buffers timestamp clock");

u64 rtscam_get_timestamp(void)
{
	if (CLOCK_MONOTONIC == rts_clock_param)
		return ktime_get_raw_ns();
	else
		return ktime_get_real_ns();
}

struct rtscam_video_format *find_format_by_fourcc(
		struct rtscam_video_stream *stream, u32 fourcc)
{
	struct rtscam_video_format *format = stream->user_formats;

	while (format) {
		if (fourcc == format->fourcc)
			return format;
		format = format->next;
	}

	return NULL;
}

int rtscam_register_format(struct rtscam_video_stream *stream,
			   struct rtscam_video_format_xlate *pfmt)
{
	struct rtscam_video_format *format = NULL;
	struct rtscam_video_format *p = NULL;

	if (find_format_by_fourcc(stream, pfmt->fourcc)) {
		rtsprintk(RTS_TRACE_ERROR, "%c%c%c%c has been registered\n",
			  v4l2pixfmtstr(pfmt->fourcc));
		return -EEXIST;
	}

	format = kzalloc(sizeof(*format), GFP_KERNEL);
	if (NULL == format)
		return -ENOMEM;

	format->type = pfmt->type;
	format->index = pfmt->index;
	format->colorspace = pfmt->colorspace;
	format->field = pfmt->field;
	format->fourcc = pfmt->fourcc;
	format->bpp = pfmt->bpp;
	format->is_yuv = pfmt->is_yuv;
	format->rts_code = pfmt->rts_code;
	memcpy((void*)format->name, (void*)pfmt->name, sizeof(format->name));

	format->next = NULL;

	if (!stream->user_format) {
		stream->rts_code = pfmt->rts_code;
		stream->user_format = pfmt->fourcc;
	}

	if (NULL == stream->user_formats) {
		stream->user_formats = format;
		return 0;
	}
	p = stream->user_formats;
	while (p->next)
		p = p->next;
	p->next = format;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_register_format);

struct rtscam_video_frame *find_frame(struct rtscam_video_format *format,
				      __u32 width, __u32 height)
{
	struct rtscam_video_frame *frame = NULL;

	frame = format->discrete.frames;

	while (frame) {
		if (frame->size.width == width && frame->size.height == height)
			return frame;
		frame = frame->next;
	}

	return NULL;
}

int rtscam_register_frame_discrete(struct rtscam_video_stream *stream,
				   __u32 fourcc, struct rtscam_frame_size *size)
{
	struct rtscam_video_format *format;
	struct rtscam_video_frame *frame = NULL;
	struct rtscam_video_frame *p;

	format = find_format_by_fourcc(stream, fourcc);

	if (NULL == format) {
		rtsprintk(RTS_TRACE_ERROR,
			  "please register format(%c%c%c%c) first\n",
			  v4l2pixfmtstr(fourcc));
		return -EINVAL;
	}
	if (format->initialized && RTSCAM_SIZE_DISCRETE != format->frame_type)
		return -EINVAL;

	if (find_frame(format, size->width, size->height))
		return -EEXIST;

	frame = kzalloc(sizeof(*frame), GFP_KERNEL);
	if (NULL == frame)
		return -ENOMEM;

	frame->size.width = size->width;
	frame->size.height = size->height;
	frame->next = NULL;

	if (stream->user_format == fourcc &&
	    (!stream->user_width || !stream->user_height)) {
		stream->user_width = size->width;
		stream->user_height = size->height;
		stream->bytesperline = (stream->user_width * format->bpp) >> 3;
		stream->sizeimage = stream->user_height * stream->bytesperline;
	}

	if (format->discrete.frames == NULL) {
		format->discrete.frames = frame;
	} else {
		p = format->discrete.frames;
		while (p->next)
			p = p->next;
		p->next = frame;
	}

	format->frame_type = RTSCAM_SIZE_DISCRETE;
	format->initialized = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_register_frame_discrete);

int rtscam_register_frame_stepwise(struct rtscam_video_stream *stream,
				   __u32 fourcc,
				   struct rtscam_frame_size *max,
				   struct rtscam_frame_size *min,
				   struct rtscam_frame_size *step)
{
	struct rtscam_video_format *format;

	format = find_format_by_fourcc(stream, fourcc);

	if (NULL == format) {
		rtsprintk(RTS_TRACE_ERROR,
			  "please register format(%c%c%c%c) first\n",
			  v4l2pixfmtstr(fourcc));
		return -EINVAL;
	}

	if (format->initialized &&
	    RTSCAM_SIZE_STEPWISE != format->frame_type &&
	    RTSCAM_SIZE_CONTINUOUS != format->frame_type)
		return -EINVAL;

	if (stream->user_format == fourcc &&
			(stream->user_width != max->width ||
			stream->user_height != max->height)) {
		stream->user_width = max->width;
		stream->user_height = max->height;
		stream->bytesperline = (stream->user_width * format->bpp) >> 3;
		stream->sizeimage = stream->user_height * stream->bytesperline;
	}

	format->stepwise.max.width = max->width;
	format->stepwise.max.height = max->height;
	format->stepwise.min.width = min->width;
	format->stepwise.min.height = min->height;
	format->stepwise.step.width = step->width;
	format->stepwise.step.height = step->height;

	format->frame_type = RTSCAM_SIZE_STEPWISE;
	format->initialized = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_register_frame_stepwise);

static int rtscam_clr_frame(struct rtscam_video_frame *frame)
{
	struct rtscam_video_frame *cur = frame;
	struct rtscam_video_frame *next;

	while (cur) {
		next = cur->next;
		rtscam_clr_frmival(&cur->frmival);
		kfree(cur);
		cur = next;
	}

	return 0;
}

int rtscam_clr_format(struct rtscam_video_stream *stream,
			int keep)
{
	struct rtscam_video_format *cur = stream->user_formats;
	struct rtscam_video_format *next = NULL;

	while (cur) {
		next = cur->next;

		if (cur->initialized) {
			if (RTSCAM_SIZE_DISCRETE == cur->frame_type) {
				rtscam_clr_frame(cur->discrete.frames);
				cur->discrete.frames = NULL;
			} else {
				rtscam_clr_frmival(&cur->stepwise.frmival);
			}
			cur->initialized = 0;
		}
		kfree(cur);
		cur = next;
	}
	stream->user_formats = NULL;

	if (!keep) {
		stream->rts_code = 0;
		stream->user_format = 0;
		stream->user_width = 0;
		stream->user_height = 0;

		rtscam_set_user_fps(&stream->fps, 1, 1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_clr_format);

static int rtscam_check_frmival(struct rtscam_video_frmival *frmival)
{
	if (!frmival)
		return -EINVAL;

	if (!frmival->initialized)
		return -EINVAL;

	if (RTSCAM_SIZE_DISCRETE == frmival->frmival_type) {
		struct rtscam_frame_frmival *ival;

		if (!frmival->discrete.frmivals)
			return -EINVAL;

		ival = frmival->discrete.frmivals;
		while (ival) {
			if (ival->frmival.numerator == 0 ||
			    ival->frmival.denominator == 0)
				return -EINVAL;
			ival = ival->next;
		}
	} else {
		if (frmival->stepwise.max.numerator == 0 ||
		    frmival->stepwise.max.denominator == 0 ||
		    frmival->stepwise.min.numerator == 0 ||
		    frmival->stepwise.min.denominator == 0 ||
		    frmival->stepwise.step.numerator == 0 ||
		    frmival->stepwise.step.denominator == 0)
			return -EINVAL;
	}

	return 0;
}

int rtscam_check_stream_format(struct rtscam_video_stream *stream)
{
	struct rtscam_video_format *format = NULL;
	int ret;

	if (!stream)
		return -EINVAL;

	if (!stream->user_formats)
		return -EINVAL;

	format = stream->user_formats;

	while (format) {
		if (format->fourcc == 0)
			return -EINVAL;

		if (!format->initialized)
			return -EINVAL;

		if (RTSCAM_SIZE_DISCRETE == format->frame_type) {
			struct rtscam_video_frame *frame = NULL;

			if (!format->discrete.frames)
				return -EINVAL;

			frame = format->discrete.frames;
			while (frame) {
				if (frame->size.width == 0 ||
				    frame->size.height == 0)
					return -EINVAL;

				ret = rtscam_check_frmival(&frame->frmival);
				if (ret)
					return ret;
				frame = frame->next;
			}
		} else {
			if (format->stepwise.max.width == 0 ||
			    format->stepwise.max.height == 0 ||
			    format->stepwise.min.width == 0 ||
			    format->stepwise.min.height == 0 ||
			    format->stepwise.step.width == 0 ||
			    format->stepwise.step.height == 0)
				return -EINVAL;

			ret = rtscam_check_frmival(&format->stepwise.frmival);
			if (ret)
				return ret;
		}

		format = format->next;
	}

	return 0;
}

static int rtscam_check_user_frmival(struct rtscam_video_frmival *frmival,
				     __u32 user_numerator,
				     __u32 user_denominator)
{
	if (RTSCAM_SIZE_DISCRETE == frmival->frmival_type) {
		struct rtscam_frame_frmival *ival = frmival->discrete.frmivals;
		while (ival) {
			if (ival->frmival.denominator * user_numerator >=
			    ival->frmival.numerator * user_denominator)
				break;
			ival = ival->next;
		}
		if (!ival)
			return -EINVAL;
	} else {
		int valid = 1;

		if (user_denominator * frmival->stepwise.max.numerator >
			user_numerator * frmival->stepwise.max.denominator)
			valid = 0;
		if ((user_denominator * frmival->stepwise.step.numerator) %
			(user_numerator * frmival->stepwise.step.denominator))
			valid = 0;
		if (!valid)
			return -EINVAL;
	}

	return 0;
}

int rtscam_check_user_format(struct rtscam_video_stream *stream,
			     u32 user_format, u32 user_width, u32 user_height,
			     u32 user_numerator, u32 user_denominator)
{
	struct rtscam_video_format *format = NULL;
	struct rtscam_video_frmival *frmival = NULL;
	int ret;

	if (user_format == 0 || user_width == 0 || user_height == 0 ||
	    user_numerator == 0 || user_denominator == 0)
		return -EINVAL;

	format = find_format_by_fourcc(stream, user_format);
	if (!format) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user format(%c%c%c%c)\n",
			  v4l2pixfmtstr(user_format));
		return -EINVAL;
	}

	frmival = rtscam_get_video_frmival(stream, user_format,
				    user_width, user_height);
	if (!frmival) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user format\n");
		return -EINVAL;
	}

	ret = rtscam_check_user_frmival(frmival,
					user_numerator, user_denominator);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user fps\n");
		return ret;
	}

	return 0;
}

int rtscam_try_user_format(struct rtscam_video_stream *stream,
			   u32 user_format, u32 user_width, u32 user_height)
{
	struct rtscam_video_frmival *frmival = NULL;

	if (user_format == 0 || user_width == 0 || user_height == 0)
		return -EINVAL;

	frmival = rtscam_get_video_frmival(stream, user_format,
				    user_width, user_height);
	if (!frmival) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user format\n");
		return -EINVAL;
	}

	return 0;
}

int rtscam_set_user_format(struct rtscam_video_stream *stream,
			   u32 user_format, u32 user_width, u32 user_height)
{
	struct rtscam_video_format *format = NULL;
	struct rtscam_video_frmival *frmival = NULL;
	int ret = 0;

	if (user_format == 0 || user_width == 0 || user_height == 0)
		return -EINVAL;

	format = find_format_by_fourcc(stream, user_format);
	if (!format) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user format(%c%c%c%c)\n",
			  v4l2pixfmtstr(user_format));
		return -EINVAL;
	}

	frmival = rtscam_get_video_frmival(stream, user_format,
				    user_width, user_height);
	if (!frmival) {
		rtsprintk(RTS_TRACE_ERROR, "invalid user format\n");
		return -EINVAL;
	}

	ret = rtscam_check_user_frmival(frmival,
			stream->fps.user_setting.numerator,
			stream->fps.user_setting.denominator);

	if (!ret)
		goto exit;

	if (RTSCAM_SIZE_DISCRETE == frmival->frmival_type) {
		struct rtscam_frame_frmival *ival = frmival->discrete.frmivals;
		if (!ival)
			return -EINVAL;

		rtscam_set_user_fps(&stream->fps, ival->frmival.numerator,
				ival->frmival.denominator);
	} else {
		rtscam_set_user_fps(&stream->fps,
				frmival->stepwise.max.numerator,
				frmival->stepwise.max.denominator);
	}

exit:
	stream->rts_code = format->rts_code;
	stream->user_format = user_format;
	stream->user_width = user_width;
	stream->user_height = user_height;

	if (user_format == V4L2_PIX_FMT_128BIT_3PIXEL) {
		stream->sizeimage = (stream->user_width *
			stream->user_height + 2) / 3 * (format->bpp >> 3);
		stream->bytesperline = stream->sizeimage / stream->user_height;
	} else {
		stream->bytesperline = (stream->user_width * format->bpp) >> 3;
		stream->sizeimage = stream->user_height * stream->bytesperline;
	}

	return 0;
}

int rtscam_set_user_frmival(struct rtscam_video_stream *stream,
			    u32 user_numerator, u32 user_denominator)
{
	int ret = 0;

	ret = rtscam_check_user_format(stream, stream->user_format,
				       stream->user_width, stream->user_height,
				       user_numerator, user_denominator);
	if (ret)
		return ret;

	rtscam_set_user_fps(&stream->fps, user_numerator, user_denominator);
	return 0;
}

static int __find_max_frame_size_from_discrete_format(
		struct rtscam_video_format *format,
		struct rtscam_frame_size *max)
{
	u32 width = 0;
	u32 height = 0;
	struct rtscam_video_frame *frame = NULL;

	if (!format || RTSCAM_SIZE_DISCRETE != format->frame_type)
		return -EINVAL;

	if (!max)
		return -EINVAL;

	frame = format->discrete.frames;

	while (frame) {
		if (width * height < frame->size.width * frame->size.height) {
			width = frame->size.width;
			height = frame->size.height;
		}
		frame = frame->next;
	}

	max->width = width;
	max->height = height;

	return 0;
}

int rtscam_get_max_frame_size(struct rtscam_video_format *format,
			      struct rtscam_frame_size *max)
{
	int ret;

	if (!format || !max)
		return -EINVAL;

	if (!format->initialized)
		return -EINVAL;

	switch (format->frame_type) {
	case RTSCAM_SIZE_DISCRETE:
		ret = __find_max_frame_size_from_discrete_format(format, max);
		break;
	case RTSCAM_SIZE_STEPWISE:
	case RTSCAM_SIZE_CONTINUOUS:
		max->width = format->stepwise.max.width;
		max->height = format->stepwise.max.height;
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


int rts_test_bit(const __u8 *data, int bit)
{
	return (data[bit >> 3] >> (bit & 7)) & 1;
}
EXPORT_SYMBOL_GPL(rts_test_bit);

void rts_clear_bit(__u8 *data, int bit)
{
	data[bit >> 3] &= ~(1 << (bit & 7));
}
EXPORT_SYMBOL_GPL(rts_clear_bit);

long rtscam_usercopy(struct file *filp, unsigned int cmd, unsigned long arg,
		     rtscam_kioctl func)
{
	char sbuf[64];
	void *mbuf = NULL;
	void *parg = (void *)arg;
	unsigned int size = _IOC_SIZE(cmd);
	int ret = -EINVAL;

	if (!func)
		return -EINVAL;

	if (_IOC_DIR(cmd) != _IOC_NONE) {
		if (size <= sizeof(sbuf)) {
			memset(sbuf, 0, size);
			parg = sbuf;
		} else {
			mbuf = kzalloc(size, GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			ret = copy_from_user(parg, (void __user *)arg, size);
			if (ret)
				goto out;
		}
	}

	ret = func(filp, cmd, parg);

	if (0 == ret && (_IOC_DIR(cmd) & _IOC_READ))
		ret = copy_to_user((void __user *)arg, parg, size);
out:
	if (mbuf)
		kfree(mbuf);
	mbuf = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_usercopy);

#ifdef DBG_ENCODE_TIME

int rtscam_init_ring_buf(struct ring_buffer *ring_buf)
{
	if (!ring_buf)
		return -EINVAL;

	init_waitqueue_head(&ring_buf->ewait);
	ring_buf->rnum = 0;
	ring_buf->wnum = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_init_ring_buf);

int rtscam_write_ring_buf(struct ring_buffer *ring_buf, int width,
			long delta, unsigned long cnt)
{
	if (!ring_buf)
		return -EINVAL;

	ring_buf->wnum = (ring_buf->wnum + 1) % RING_BUFFER_ITEM_NUM;
	ring_buf->etime[ring_buf->wnum].width = width;
	ring_buf->etime[ring_buf->wnum].delta = delta;
	ring_buf->etime[ring_buf->wnum].cnt = cnt;

	if (ring_buf->wnum == ring_buf->rnum)
		ring_buf->rnum = (ring_buf->rnum + 1) % RING_BUFFER_ITEM_NUM;

	wake_up_interruptible(&ring_buf->ewait);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_write_ring_buf);

int rtscam_read_ring_buf(struct ring_buffer *ring_buf, void __user *arg)
{
	int ret = 0;

	if (!ring_buf)
		return -EINVAL;

	if (ring_buf->rnum == ring_buf->wnum)
		return -1;

	ret = wait_event_interruptible(ring_buf->ewait,
				(ring_buf->rnum != ring_buf->wnum));
	if (ret != 0) {
		rtsprintk(RTS_TRACE_ERROR, "ringbuf wait event fail\n");
		return -EFAULT;
	}

	ret = copy_to_user(arg, &ring_buf->etime[ring_buf->rnum],
				sizeof(struct encode_time));
	if (ret != 0) {
		rtsprintk(RTS_TRACE_ERROR, "ringbuf copy fail\n");
		return -EFAULT;
	}
	ring_buf->rnum = (ring_buf->rnum + 1) % RING_BUFFER_ITEM_NUM;

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_read_ring_buf);
#endif

MODULE_AUTHOR("Ming Qian <ming_qian@realsil.com.cn>");
MODULE_DESCRIPTION("rts_camera commmon");
MODULE_LICENSE("GPL v2");
