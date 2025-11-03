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

#define TAG	"VIDEO"
#include <linux/err.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <media/videobuf2-memops.h>
#include "rts_camera_priv.h"
#include "rts_camera_fps.h"

// [TODO] do not set to fixed value
#define RTS_VIDEO_RING_HEIGHT	256

static int rtscam_convert_addr_from_vm_to_phy(struct rtscam_video_device *icd,
				       struct rtscam_addr *addr)
{
	struct vm_area_struct *vma = NULL;
	struct rtscam_video_stream *stream = NULL;
	struct vb2_buffer *vb = NULL;
	void *buf = NULL;
	int found = 0;
	int i;

	if (!icd->mem_ops ||
	    !icd->mem_ops->get_userptr || !icd->mem_ops->put_userptr ||
	    !icd->mem_ops->cookie)
		return -EINVAL;

	if (!addr || addr->index < 0)
		return -EINVAL;

	vma = find_vma(current->mm, addr->addr);
	if (NULL == vma)
		return -EINVAL;

	for (i = 0; i < icd->streamnum; i++) {
		stream = icd->streams + i;
		if (vma->vm_file == stream->memory_owner) {
			found = 1;
			break;
		}
	}
	if (!found) {
		rtsprintk(RTS_TRACE_ERROR, "not rts camera video buffer\n");
		return -EINVAL;
	}
	if (addr->index >= stream->vb2_vidp.num_buffers)
		return -EINVAL;

	vb = stream->vb2_vidp.bufs[addr->index];
	buf = icd->mem_ops->get_userptr(vb, icd->dev,
					addr->addr, vma->vm_end - vma->vm_start);
	if (!buf)
		return -EINVAL;

	addr->addr = *(unsigned long *)icd->mem_ops->cookie(vb, buf);
	icd->mem_ops->put_userptr(buf);

	return 0;
}

int rtscam_get_phy_addr(struct vb2_buffer *vb, dma_addr_t *y_phy_addr,
						dma_addr_t *uv_phy_addr)
{
	struct rtscam_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct rtscam_video_device *icd = stream->icd;
	dma_addr_t *dma_addr = NULL;

	if (!icd->mem_ops)
		return -EINVAL;

	dma_addr = vb2_plane_cookie(vb, 0);
	if (!dma_addr)
		return -EINVAL;

	if (y_phy_addr)
		*y_phy_addr = *dma_addr;

	if (stream->rts_code == RTSCAM_FORMAT_TYPE_YUV420_SEMIPLANAR &&
			stream->memory == V4L2_MEMORY_USERPTR &&
			stream->vin_mode != RTS_VIN_MODE_RING_MEM) {
		dma_addr = NULL;
		dma_addr = vb2_plane_cookie(vb, 1);
		if (!dma_addr)
			return -EINVAL;

		if (uv_phy_addr)
			*uv_phy_addr = *dma_addr;
	} else {
		if (uv_phy_addr)
			*uv_phy_addr = 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_get_phy_addr);

static void rtscam_init_capture_buffer(struct rtscam_video_stream *stream)
{
	spin_lock_irq(&stream->lock);
	INIT_LIST_HEAD(&stream->capture);
	rtscam_call_video_op(stream->icd, init_capture_buffers, stream);
	spin_unlock_irq(&stream->lock);
}

struct rtscam_video_buffer *rtscam_get_video_buffer(
		struct rtscam_video_stream *stream, int index)
{
	struct rtscam_video_buffer *buffer = NULL;
	struct vb2_buffer *vb = NULL;

	if (!stream)
		return NULL;

	if (index < 0 || index >= stream->vb2_vidp.num_buffers)
		return NULL;

	vb = stream->vb2_vidp.bufs[index];
	buffer = to_rtscam_vbuf(vb);

	return buffer;
}
EXPORT_SYMBOL_GPL(rtscam_get_video_buffer);

static int rtscam_set_buffer_state(struct rtscam_video_buffer *buffer,
					int state)
{
	if (!buffer)
		return -EINVAL;

	if (state < RTS_BUF_STATE_IDLE || state >= RTS_BUF_STATE_RESERVED)
		return -EINVAL;

	buffer->state = state;

	return 0;
}

static struct rtscam_video_buffer *rtscam_pop_ready_buffer(
		struct rtscam_video_stream *stream)
{
	struct rtscam_video_buffer *buf = NULL;

	if (!stream)
		return NULL;

	if (list_empty(&stream->capture))
		return NULL;

	buf = list_first_entry(&stream->capture,
			       struct rtscam_video_buffer, list);
	list_del_init(&buf->list);

	return buf;
}

int rtscam_push_back_ready_buffer(struct rtscam_video_stream *stream,
				  struct rtscam_video_buffer *buf)
{
	struct rtscam_video_buffer *rbuf;

	if (!stream || !buf)
		return -EINVAL;

	list_for_each_entry(rbuf, &stream->capture, list) {
		if (rbuf == buf)
			return 0;
	}

	rtscam_set_buffer_state(buf, RTS_BUF_STATE_READY);
	list_add_tail(&buf->list, &stream->capture);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_push_back_ready_buffer);

static void rtscam_clr_ready_buffer(struct rtscam_video_stream *stream)
{
	struct rtscam_video_buffer *buffer;
	struct rtscam_video_buffer *tmp;

	if (!stream)
		return;

	list_for_each_entry_safe(buffer, tmp, &stream->capture, list) {
		list_del_init(&buffer->list);
		rtscam_set_buffer_state(buffer, RTS_BUF_STATE_IDLE);
	}

	INIT_LIST_HEAD(&stream->capture);
}

static void rtscam_return_buffers(struct rtscam_video_stream *stream)
{
	struct rtscam_video_buffer *buffer;
	int i;

	for (i = 0; i < stream->vb2_vidp.num_buffers; i++) {
		buffer = rtscam_get_video_buffer(stream, i);

		if (buffer->state == RTS_BUF_STATE_DONE ||
		    buffer->state == RTS_BUF_STATE_DEQUEUED)
			continue;
		vb2_buffer_done(&buffer->buf.vb2_buf, VB2_BUF_STATE_ERROR);
		rtscam_set_buffer_state(buffer, RTS_BUF_STATE_DONE);
	}
}

int rtscam_submit_buffer(struct rtscam_video_stream *stream,
			 struct rtscam_video_buffer *buf)
{
	struct rtscam_video_buffer *rbuf = NULL;
	int ret;

	if (buf)
		rtscam_push_back_ready_buffer(stream, buf);

	rbuf = rtscam_pop_ready_buffer(stream);
	if (!rbuf)
		return -EINVAL;

	ret = rtscam_call_video_op(stream->icd, submit_buffer, stream, rbuf);
	if (ret) {
		rtscam_push_back_ready_buffer(stream, rbuf);
		return ret;
	}

	rtscam_set_buffer_state(rbuf, RTS_BUF_STATE_HW);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_submit_buffer);

static void __rtscam_buffer_done(struct rtscam_video_buffer *buffer)
{
	if (!buffer)
		return;

	rtscam_set_buffer_state(buffer, RTS_BUF_STATE_DONE);
	vb2_buffer_done(&buffer->buf.vb2_buf, VB2_BUF_STATE_DONE);
}

static void rtscam_buffer_wq_handle(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct rtscam_video_buffer *rbuf;

	if (!work)
		return;

	dwork = to_delayed_work(work);
	rbuf = container_of(dwork, struct rtscam_video_buffer, dwork);

	__rtscam_buffer_done(rbuf);
}

static void rtscam_delay_buffer(struct rtscam_video_buffer *rbuf,
			unsigned long delay)
{
	struct delayed_work *dwork = &rbuf->dwork;

	INIT_DELAYED_WORK(dwork, rtscam_buffer_wq_handle);

	rtscam_set_buffer_state(rbuf, RTS_BUF_STATE_DELAYED);

	schedule_delayed_work(dwork, delay * HZ / 1000);
}

static void rtscam_clr_delayed_buffer(struct rtscam_video_stream *stream)
{
	int i;
	struct rtscam_video_buffer *buffer;

	if (!stream)
		return;

	for (i = 0; i < stream->vb2_vidp.num_buffers; i++) {
		buffer = rtscam_get_video_buffer(stream, i);
		if (!buffer)
			continue;

		if (buffer->state == RTS_BUF_STATE_DELAYED) {
			cancel_delayed_work_sync(&buffer->dwork);
			rtscam_set_buffer_state(buffer, RTS_BUF_STATE_IDLE);
		}

	}
}

void rtscam_buffer_done(struct rtscam_video_stream *stream,
			struct rtscam_video_buffer *rbuf,
			unsigned long bytesused)
{
	struct rtscam_video_format *format;

	if (!stream || !rbuf)
		return;

	format = find_format_by_fourcc(stream, stream->user_format);
	if (format)
		rbuf->buf.field = format->field;
	else
		rbuf->buf.field = V4L2_FIELD_NONE;
	rbuf->buf.sequence = stream->sequence++;

	rbuf->buf.vb2_buf.timestamp = rtscam_get_timestamp();
	rtsprintk(RTS_TRACE_PTS, "0x%08x\n", rbuf->pts);
	if (stream->user_format == V4L2_PIX_FMT_NV12 &&
			stream->memory == V4L2_MEMORY_USERPTR &&
			stream->vin_mode != RTS_VIN_MODE_RING_MEM) {
		vb2_set_plane_payload(&rbuf->buf.vb2_buf, 0, bytesused * 2 / 3);
		vb2_set_plane_payload(&rbuf->buf.vb2_buf, 1, bytesused / 3);
	} else
		vb2_set_plane_payload(&rbuf->buf.vb2_buf, 0, bytesused);

	if (stream->delay)
		rtscam_delay_buffer(rbuf, stream->delay);
	else
		__rtscam_buffer_done(rbuf);
}
EXPORT_SYMBOL_GPL(rtscam_buffer_done);

static int rtscam_queue_setup(struct vb2_queue *q,
		       unsigned int *num_buffers, unsigned int *num_planes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtscam_video_stream *stream = vb2_get_drv_priv(q);
	struct rtscam_video_format *format = NULL;
	u32 pixelformat;

	pixelformat = stream->user_format;

	format = find_format_by_fourcc(stream, pixelformat);
	if (!format)
		return -EINVAL;

	*num_planes = 1;
	sizes[0] = ((stream->user_width * format->bpp) >> 3) *
						stream->user_height;

	if (stream->vin_mode == RTS_VIN_MODE_RING_MEM &&
					stream->streamid == 0) {
		sizes[0] = ((stream->user_width * format->bpp) >> 3) *
					RTS_VIDEO_RING_HEIGHT;
		goto out;
	}

	if (pixelformat == V4L2_PIX_FMT_128BIT_3PIXEL) {
		sizes[0] = (stream->user_width *
			stream->user_height + 2) / 3 * (format->bpp >> 3);
		goto out;
	}

	if (q->memory == V4L2_MEMORY_MMAP)
		goto out;

	if (pixelformat == V4L2_PIX_FMT_NV12) {
		*num_planes = 2;
		sizes[0] = stream->user_width * stream->user_height;
		sizes[1] = stream->user_width * stream->user_height >> 1;
		alloc_devs[1] = stream->icd->dev;
	}

out:
	if (sizes[0] == 0 || (*num_planes == 2 && sizes[1] == 0)) {
		rtsprintk(RTS_TRACE_ERROR, "frame buf size is 0\n");
		return -EINVAL;
	}

	alloc_devs[0] = stream->icd->dev;

	if (!(*num_buffers))
		*num_buffers = 2;

	if (*num_buffers > RTSCAM_MAX_BUFFER_NUM)
		*num_buffers = RTSCAM_MAX_BUFFER_NUM;

	if (*num_planes > RTSCAM_MAX_PLANES_NUM)
		*num_planes = RTSCAM_MAX_PLANES_NUM;

	stream->memory = q->memory;

	return 0;
}

static int rtscam_buf_init(struct vb2_buffer *vb)
{
	struct rtscam_video_buffer *buf = to_rtscam_vbuf(vb);

	rtscam_set_buffer_state(buf, RTS_BUF_STATE_IDLE);

	return 0;
}

static int rtscam_buf_prepare(struct vb2_buffer *vb)
{
	struct rtscam_video_buffer *buf = to_rtscam_vbuf(vb);

	rtscam_set_buffer_state(buf, RTS_BUF_STATE_QUEUED);

	return 0;
}

static void rtscam_buf_finish(struct vb2_buffer *vb)
{
	struct rtscam_video_buffer *buf = to_rtscam_vbuf(vb);

	rtscam_set_buffer_state(buf, RTS_BUF_STATE_DEQUEUED);
}

static void rtscam_buf_cleanup(struct vb2_buffer *vb)
{
}

static int rtscam_buf_start_streaming(struct vb2_queue *q, unsigned int count)
{
	return 0;
}

static void rtscam_buf_stop_streaming(struct vb2_queue *q)
{
	struct rtscam_video_stream *stream = vb2_get_drv_priv(q);

	rtscam_clr_delayed_buffer(stream);
	spin_lock_irq(&stream->lock);
	rtscam_clr_ready_buffer(stream);
	rtscam_return_buffers(stream);
	spin_unlock_irq(&stream->lock);

}

static void rtscam_buf_queue(struct vb2_buffer *vb)
{
	struct rtscam_video_stream *stream = vb2_get_drv_priv(vb->vb2_queue);
	struct rtscam_video_buffer *buf = to_rtscam_vbuf(vb);

	spin_lock_irq(&stream->lock);
	rtscam_submit_buffer(stream, buf);
	spin_unlock_irq(&stream->lock);
}

static struct vb2_ops rtscam_vb2_ops = {
	.queue_setup = rtscam_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = rtscam_buf_init,
	.buf_prepare = rtscam_buf_prepare,
	.buf_finish = rtscam_buf_finish,
	.buf_cleanup = rtscam_buf_cleanup,
	.start_streaming = rtscam_buf_start_streaming,
	.stop_streaming = rtscam_buf_stop_streaming,
	.buf_queue = rtscam_buf_queue,
};

static int rtscam_video_init_videobuf2(struct vb2_queue *queue,
				       struct rtscam_video_stream *stream)
{
	int ret;
	struct rtscam_video_device *icd = stream->icd;

	queue->type = icd->type;
	queue->io_modes = VB2_MMAP | VB2_USERPTR;
	queue->drv_priv = stream;
	queue->ops = &rtscam_vb2_ops;
	queue->mem_ops = icd->mem_ops;
	queue->buf_struct_size = sizeof(struct rtscam_video_buffer);
	queue->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	queue->lock = &stream->queue_lock;

	ret = vb2_queue_init(queue);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&stream->capture);

	return 0;
}

static void rtscam_video_release_videobuf2(struct vb2_queue *queue,
					   struct rtscam_video_stream *stream)
{
	mutex_lock(&stream->queue_lock);
	vb2_queue_release(queue);
	mutex_unlock(&stream->queue_lock);
}

static int rtscam_video_reqbufs(struct file *file, void *fh,
						struct v4l2_requestbuffers *buf)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	int ret = 0;

	ret = vb2_ioctl_reqbufs(file, fh, buf);
	if (!ret && buf->count) {
		rtscam_init_capture_buffer(stream);
		stream->memory_owner = file;
	}

	return ret;
}

static int rtscam_video_qbuf(struct file *file, void *fh,
						struct v4l2_buffer *buf)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_buffer *buffer =
			rtscam_get_video_buffer(stream, buf->index);
	int ret = 0;

	if (!buffer)
		return -EINVAL;

	if (buffer->state == RTS_BUF_STATE_QUEUED)
		ret = 0;
	else
		ret = vb2_ioctl_qbuf(file, fh, buf);

	return ret;
}

static int rtscam_video_querycap(struct file *file, void *fh,
						struct v4l2_capability *cap)
{
	struct rtscam_video_stream *stream = video_drvdata(file);

	strlcpy(cap->driver, stream->icd->drv_name, sizeof(cap->driver));
	if (stream->icd->dev_name)
		strlcpy(cap->card, stream->icd->dev_name, sizeof(cap->card));

	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int rtscam_video_enum_fmt(struct file *file, void *fh,
						struct v4l2_fmtdesc *f)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_format *format;
	int index = 0;

	if (f->type != stream->icd->type)
		return -EINVAL;

	if (f->index < 0)
		f->index = 0;

	format = stream->user_formats;
	index = 0;

	while (format) {
		if (index == f->index)
			break;
		format = format->next;
		index++;
	}

	if (!format)
		return -EINVAL;

	strlcpy(f->description, format->name, sizeof(f->description));
	f->pixelformat = format->fourcc;

	return 0;
}

static int rtscam_video_try_fmt(struct file *file, void *fh,
						struct v4l2_format *f)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int ret;

	if (f->type != stream->icd->type)
		return -EINVAL;

	ret = rtscam_try_user_format(stream, pix->pixelformat,
				     pix->width, pix->height);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "format (%c%c%c%c) (%dx%d) unsupportted\n",
			  v4l2pixfmtstr(pix->pixelformat),
			  pix->width, pix->height);
		return ret;
	}

	return 0;
}

static int rtscam_check_crop_info_by_fmt(struct rtscam_video_stream *stream,
					struct v4l2_selection *selection)
{
	int ret = 0;

	if (!stream->streamid) {
		if (stream->user_width > 5 * selection->r.width / 2 ||
		    stream->user_height > 5 * selection->r.height / 2)
			ret = -EINVAL;
	} else {
		if (stream->user_width > selection->r.width ||
		    stream->user_height > selection->r.height)
			ret = -EINVAL;
	}

	if (ret)
		rtsprintk(RTS_TRACE_ERROR,
			"invalid crop setting(%dx%d) for resolution(%dx%d)\n",
			selection->r.width, selection->r.height,
			stream->user_width, stream->user_height);
	return ret;
}

static int rtscam_video_set_selection(struct file *file, void *fh,
					struct v4l2_selection *selection)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	int ret = 0;

	if (rtscam_is_streaming(stream)) {
		ret = rtscam_check_crop_info_by_fmt(stream, selection);
		if (ret < 0)
			goto out;

		selection->type = 1;
		ret = rtscam_call_video_op(stream->icd,
				set_selection, stream, selection);
	} else {
		selection->type = 0;
		ret = rtscam_call_video_op(stream->icd,
				set_selection, stream, selection);
	}
out:
	return ret;
}

static int rtscam_video_get_selection(struct file *file, void *fh,
					struct v4l2_selection *selection)
{
	struct rtscam_video_stream *stream = video_drvdata(file);

	return  rtscam_call_video_op(stream->icd,
			get_selection, stream, selection);
}

static int rtscam_check_fmt_by_crop_info(struct rtscam_video_stream *stream,
							u32 width, u32 height)
{
	struct v4l2_selection selection;
	int ret;

	ret = rtscam_call_video_op(stream->icd,
				get_selection, stream, &selection);
	if (ret) {
		if (ret == -EPERM)
			return 0;

		rtsprintk(RTS_TRACE_ERROR, "get selection setting fail\n");
		return ret;
	}

	if (!stream->streamid) {
		if (width > 5 * selection.r.width / 2 ||
		    height > 5 * selection.r.height / 2)
			ret = -EINVAL;

	} else {
		if (width > selection.r.width || height > selection.r.height)
			ret = -EINVAL;
	}

	if (ret)
		rtsprintk(RTS_TRACE_ERROR,
			"invalid resolution(%dx%d) for crop setting(%dx%d)\n",
			width, height, selection.r.width, selection.r.height);
	return ret;
}

static int rtscam_video_set_fmt(struct file *file, void *fh,
						struct v4l2_format *f)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct rtscam_video_format *fmt;
	int ret;

	if (vb2_is_busy(&stream->vb2_vidp))
		return -EBUSY;

	if (rtscam_is_streaming(stream)) {
		ret = -EBUSY;
		goto done;
	}

	ret = rtscam_video_try_fmt(file, fh, f);
	if (ret)
		goto done;

	ret = rtscam_check_fmt_by_crop_info(stream, pix->width, pix->height);
	if (ret)
		goto done;

	fmt = find_format_by_fourcc(stream, pix->pixelformat);

	ret = rtscam_set_user_format(stream, pix->pixelformat,
				     pix->width, pix->height);
	if (ret)
		goto done;

	pix->field = fmt->field;
	pix->bytesperline = stream->bytesperline;
	pix->sizeimage = stream->sizeimage;

done:
	return ret;
}

static int rtscam_video_get_fmt(struct file *file, void *fh,
						struct v4l2_format *f)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct rtscam_video_format *fmt;

	if (f->type != stream->icd->type)
		return -EINVAL;

	fmt = find_format_by_fourcc(stream, stream->user_format);
	if (!fmt)
		return -EINVAL;

	pix->pixelformat = stream->user_format;
	pix->width = stream->user_width;
	pix->height = stream->user_height;
	pix->bytesperline = stream->bytesperline;
	pix->sizeimage = stream->sizeimage;
	pix->field = fmt->field;
	pix->colorspace = fmt->colorspace;

	return 0;
}

static int rtscam_video_enum_framesizes(struct file *file, void *fh,
						struct v4l2_frmsizeenum *fsize)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_format *format = NULL;

	format = find_format_by_fourcc(stream, fsize->pixel_format);
	if (!format || !format->initialized)
		return -EINVAL;

	if (RTSCAM_SIZE_DISCRETE == format->frame_type) {
		struct rtscam_video_frame *frame = format->discrete.frames;
		int index = 0;

		while (frame) {
			if (fsize->index == index)
				break;
			frame = frame->next;
			index++;
		}
		if (!frame)
			return -EINVAL;

		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frame->size.width;
		fsize->discrete.height = frame->size.height;
	} else {
		if (fsize->index)
			return -EINVAL;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise.max_width = format->stepwise.max.width;
		fsize->stepwise.max_height = format->stepwise.max.height;
		fsize->stepwise.min_width = format->stepwise.min.width;
		fsize->stepwise.min_height = format->stepwise.min.height;
		fsize->stepwise.step_width = format->stepwise.step.width;
		fsize->stepwise.step_height = format->stepwise.step.height;
	}

	return 0;
}

static int rtscam_video_enum_frameintervals(struct file *file, void *fh,
						struct v4l2_frmivalenum *fival)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_frmival *frmival = NULL;

	frmival = rtscam_get_video_frmival(stream, fival->pixel_format,
					   fival->width, fival->height);
	if (!frmival || !frmival->initialized)
		return -EINVAL;

	if (RTSCAM_SIZE_DISCRETE == frmival->frmival_type) {
		struct rtscam_frame_frmival *ival = frmival->discrete.frmivals;
		int index = 0;

		while (ival) {
			if (fival->index == index)
				break;
			ival = ival->next;
			index++;
		}
		if (!ival)
			return -EINVAL;

		fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		fival->discrete.numerator = ival->frmival.numerator;
		fival->discrete.denominator = ival->frmival.denominator;
	} else {
		if (fival->index)
			return -EINVAL;

		fival->type = V4L2_FRMIVAL_TYPE_STEPWISE;
		fival->stepwise.max = frmival->stepwise.max;
		fival->stepwise.min = frmival->stepwise.min;
		fival->stepwise.step = frmival->stepwise.step;
	}

	return 0;
}

static int rtscam_video_get_parm(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{
	struct rtscam_video_stream *stream = video_drvdata(file);

	if (parm->type != stream->icd->type)
		return -EINVAL;

	parm->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	parm->parm.capture.capturemode = 0;

	if (rtscam_is_streaming(stream)) {
		parm->parm.capture.timeperframe.numerator =
				stream->fps.user_actual.numerator;
		parm->parm.capture.timeperframe.denominator =
				stream->fps.user_actual.denominator;
	} else {
		parm->parm.capture.timeperframe.numerator =
				stream->fps.user_setting.numerator;
		parm->parm.capture.timeperframe.denominator =
				stream->fps.user_setting.denominator;
	}

	parm->parm.capture.extendedmode = 0;
	parm->parm.capture.readbuffers = 0;

	return 0;
}

static int rtscam_video_enum_input(struct file *file, void *fh,
						struct v4l2_input *input)
{
	if (input->index)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	strcpy(input->name, "Camera");

	return 0;
}

static int rtscam_video_get_input(struct file *file, void *fh, unsigned int *i)
{
	*i = 0;

	return 0;
}

static int rtscam_video_set_input(struct file *file, void *fh, unsigned int i)
{
	if (i)
		return -EINVAL;

	return 0;
}

static int rtscam_video_set_parm(struct file *file, void *fh,
					struct v4l2_streamparm *parm)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	u32 numerator = parm->parm.capture.timeperframe.numerator;
	u32 denominator = parm->parm.capture.timeperframe.denominator;
	int ret = 0;

	if (parm->type != stream->icd->type)
		return -EINVAL;

	ret = rtscam_set_user_frmival(stream, numerator, denominator);

	return ret;
}

static int rtscam_video_streamon(struct file *file, void *fh,
						enum v4l2_buf_type type)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	int ret = 0;

	if (rtscam_is_streaming(stream))
		return -EBUSY;

	if (stream->vin_mode == RTS_VIN_MODE_NO_MEM)
		goto out;

	ret = vb2_ioctl_streamon(file, fh, type);
	if (ret)
		return ret;
out:
	mutex_lock(&stream->icd->dev_lock);
	ret = rtscam_call_video_op(stream->icd, s_stream, stream, 1);
	if (!ret) {
		stream->streaming = 1;
		stream->icd->streaming_count++;
	}
	mutex_unlock(&stream->icd->dev_lock);

	return ret;
}

static int rtscam_video_streamoff(struct file *file, void *fh,
						enum v4l2_buf_type type)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	int ret = 0;

	if (!rtscam_is_streaming(stream))
		return 0;

	mutex_lock(&stream->icd->dev_lock);
	ret = rtscam_call_video_op(stream->icd, s_stream, stream, 0);

	if (stream->vin_mode == RTS_VIN_MODE_NO_MEM)
		goto out;

	ret = vb2_ioctl_streamoff(file, fh, type);
out:
	if (!ret) {
		stream->streaming = 0;
		stream->icd->streaming_count--;
	}
	mutex_unlock(&stream->icd->dev_lock);

	return ret;
}

long rtscam_video_do_ctrl_ioctl(struct rtscam_video_device *icd,
				unsigned int cmd, void *arg)
{
	long ret = 0;

	if (!icd)
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_QUERYCTRL:
		ret = rtscam_query_v4l2_ctrl(icd, arg);
		break;
	case VIDIOC_G_CTRL:
		ret = rtscam_get_ctrl(icd, arg);
		break;
	case VIDIOC_S_CTRL:
		ret = rtscam_set_ctrl(icd, arg);
		break;
	case VIDIOC_G_EXT_CTRLS:
		ret = rtscam_get_ext_ctrls(icd, arg);
		break;
	case VIDIOC_S_EXT_CTRLS:
		ret = rtscam_set_ext_ctrls(icd, arg);
		break;
	case VIDIOC_TRY_EXT_CTRLS:
		ret = rtscam_try_ext_ctrls(icd, arg);
		break;
	case RTSCAMIOC_VENDOR_CMD:
		mutex_lock(&icd->dev_lock);
		ret = rtscam_call_video_op(icd, exec_command,
					   icd->streams, arg);
		mutex_unlock(&icd->dev_lock);
		break;
	case RTSCAMIOC_GET_PHYADDDR:
		ret = rtscam_convert_addr_from_vm_to_phy(icd, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "Unknown[ctrl] ioctl 0x%08x, type = '%c' nr = 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_video_do_ctrl_ioctl);

static long rtscam_video_ioctl_default(struct file *file, void *fh,
				bool valid_prio, unsigned int cmd, void *arg)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_device *icd = stream->icd;
	long ret = 0;

	switch (cmd) {
	case RTS_VIDIOC_SET_FOV_MODE: {
		int status = *(int *)arg ? 1 : 0;

		if (stream->fov_status == status)
			break;

		stream->fov_status = status;
		break;
	}
	case RTS_VIDIOC_GET_FOV_MODE:
		*(int *)arg = stream->fov_status;
		break;
	case VIDIOC_SET_VIN_MODE: {
		struct video_device *vdev = stream->vdev;
		u8 vin_mode = *(u8 *)arg;
		/* vb2_ioctl_reqbufs success, can't set vin mode */
		if (vdev->queue->owner &&
				vdev->queue->owner == file->private_data) {
			ret = -EINVAL;
			break;
		}

		if (vin_mode < RTS_VIN_MODE_NO_MEM || vin_mode > RTS_VIN_MODE_NORMAL)
			ret = -EINVAL;

		stream->vin_mode = vin_mode;

		if (stream->vin_mode == RTS_VIN_MODE_RING_MEM) {
			stream->ring_buf_height = *(u32 *)arg >> 8;
			rtscam_set_user_fps_actual(&stream->fps);
		}

		rtsprintk(RTS_TRACE_VIDEO, "VIN MODE: %d\n", stream->vin_mode);
		break;
	}
	case RTSCAMIOC_VENDOR_CMD:
	case RTSCAMIOC_GET_PHYADDDR:
		ret = rtscam_video_do_ctrl_ioctl(icd, cmd, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "Unknown[video] ioctl 0x%08x, type = '%c' nr = 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}
	rtsprintk(RTS_TRACE_DEBUG,
		  "[video] ioctl 0x%08x, type = '%c' nr = 0x%x (%d)\n",
		  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_NR(cmd));

	return ret;
}

const struct v4l2_ioctl_ops rtscam_video_ioctl_ops = {
	.vidioc_querycap = rtscam_video_querycap,
	.vidioc_enum_fmt_vid_cap = rtscam_video_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = rtscam_video_get_fmt,
	.vidioc_s_fmt_vid_cap_mplane = rtscam_video_set_fmt,
	.vidioc_try_fmt_vid_cap_mplane = rtscam_video_try_fmt,
	.vidioc_g_selection = rtscam_video_get_selection,
	.vidioc_s_selection = rtscam_video_set_selection,
	.vidioc_enum_framesizes = rtscam_video_enum_framesizes,
	.vidioc_enum_frameintervals = rtscam_video_enum_frameintervals,
	.vidioc_g_parm = rtscam_video_get_parm,
	.vidioc_s_parm = rtscam_video_set_parm,
	.vidioc_enum_input = rtscam_video_enum_input,
	.vidioc_g_input = rtscam_video_get_input,
	.vidioc_s_input = rtscam_video_set_input,
	.vidioc_reqbufs = rtscam_video_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = rtscam_video_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = rtscam_video_streamon,
	.vidioc_streamoff = rtscam_video_streamoff,
	.vidioc_default = rtscam_video_ioctl_default,
};

static int rtscam_video_open(struct file *file)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_device *icd;
	int ret;

	if (!stream)
		return -ENODEV;

	icd = stream->icd;

	if (!icd || !icd->ops)
		return -EINVAL;

	ret = try_module_get(icd->ops->owner) ? 0 : -ENODEV;
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "couldn't lock capture driver\n");
		return ret;
	}

	if (mutex_lock_interruptible(&icd->dev_lock)) {
		ret = -ERESTARTSYS;
		goto elockdev;
	}

	if (atomic_inc_return(&icd->use_count) == 1) {
		ret = rtscam_call_video_op(icd, start_clock, icd);
		if (ret < 0) {
			rtsprintk(RTS_TRACE_ERROR,
				  "couldn't activate the camera:%d\n",
				  ret);
			atomic_dec(&icd->use_count);
			goto estartclock;
		}
	}
	mutex_unlock(&icd->dev_lock);

	v4l2_fh_open(file);

	return 0;

estartclock:
	mutex_unlock(&icd->dev_lock);
elockdev:
	module_put(icd->ops->owner);
	return ret;
}

static int rtscam_video_close(struct file *file)
{
	struct rtscam_video_stream *stream = video_drvdata(file);
	struct rtscam_video_device *icd;

	icd = stream->icd;

	vb2_fop_release(file);

	mutex_lock(&icd->dev_lock);
	if (atomic_dec_return(&icd->use_count) == 0)
		rtscam_call_video_op(icd, stop_clock, icd);
	mutex_unlock(&icd->dev_lock);
	module_put(icd->ops->owner);

	return 0;
}

static struct v4l2_file_operations rtscam_video_fops = {
	.owner = THIS_MODULE,
	.open = rtscam_video_open,
	.release = rtscam_video_close,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};

static int video_dev_create(struct rtscam_video_stream *stream)
{
	struct video_device *vdev = video_device_alloc();
	int nr = stream->video_nr;
	int ret;

	if (!vdev)
		return -ENOMEM;

	ret = rtscam_check_stream_format(stream);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "please init stream format first\n");
		return ret;
	}
	ret = rtscam_check_user_format(
			      stream, stream->user_format,
			      stream->user_width,
			      stream->user_height,
			      stream->fps.user_setting.numerator,
			      stream->fps.user_setting.denominator);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "please init user format first\n");
		return ret;
	}

	strlcpy(vdev->name, stream->icd->drv_name, sizeof(vdev->name));

	vdev->v4l2_dev = &stream->icd->v4l2_dev;
	vdev->fops = &rtscam_video_fops;
	vdev->ioctl_ops = &rtscam_video_ioctl_ops;
	vdev->release = video_device_release;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	vdev->lock = &stream->stream_lock;

	video_set_drvdata(vdev, stream);

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, nr);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "register video device %d fail, %d\n", nr, ret);
		video_device_release(vdev);
		return ret;
	}

	ret = rtscam_video_init_videobuf2(&stream->vb2_vidp, stream);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR,
			  "video device %d init vb2 fail, %d\n", nr, ret);
		video_unregister_device(vdev);
		return ret;
	}

	vdev->queue = &stream->vb2_vidp;
	stream->vdev = vdev;

	return 0;
}

static int rtscam_video_device_remove(struct rtscam_video_device *icd)
{
	int i;

	rtscam_video_release_ctrl(icd);

	for (i = 0; i < icd->streamnum; i++) {
		struct rtscam_video_stream *stream = icd->streams + i;
		if (!stream->vdev)
			continue;
		rtscam_video_release_videobuf2(&stream->vb2_vidp, stream);
		video_unregister_device(stream->vdev);
		stream->vdev = NULL;
	}

	return 0;
}

static int rtscam_video_device_probe(struct rtscam_video_device *icd)
{
	int ret = 0;
	int i;
	int num = 0;

	rtsprintk(RTS_TRACE_VIDEO,
		  "Probing %s\n", dev_name(icd->v4l2_dev.dev));

	if (!icd->drv_name)
		return -EINVAL;

	/*init ctrls*/
	ret = rtscam_video_init_ctrl(icd);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Init ctrls fail\n");
		return ret;
	}

	if (0 == icd->streamnum) {
		rtscam_video_release_ctrl(icd);
		rtsprintk(RTS_TRACE_ERROR, "No stream found in icd\n");
		return -EINVAL;
	}

	for (i = 0; i < icd->streamnum; i++) {
		struct rtscam_video_stream *stream = icd->streams + i;

		if (stream->video_nr < 0 || stream->video_nr > 64)
			stream->video_nr = -1;

		stream->icd = icd;
		stream->vdev = NULL;

		atomic_set(&stream->use_count, 0);

		ret = video_dev_create(stream);
		if (ret < 0)
			continue;
		num++;
	}
	if (0 == num) {
		ret = -EINVAL;
		goto error;
	}

	return 0;

error:
	rtscam_video_device_remove(icd);

	return ret;
}

int rtscam_video_register_device(struct rtscam_video_device *icd)
{
	int ret = 0;

	if (!icd)
		return -EINVAL;

	if (!icd->mem_ops)
		return -EINVAL;

	if (!icd->initialized)
		return -EINVAL;

	icd->v4l2_dev.dev = icd->dev;
	ret = v4l2_device_register(icd->v4l2_dev.dev, &icd->v4l2_dev);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "%s:v4l2_device_register fail\n", __func__);
		return ret;
	}

	atomic_set(&icd->use_count, 0);
	icd->streaming_count = 0;

	ret = rtscam_video_device_probe(icd);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "video device <%s> probe fail : %d\n",
			  icd->dev_name, ret);
		v4l2_device_unregister(&icd->v4l2_dev);
	} else {
		rtsprintk(RTS_TRACE_INFO,
			  "video device <%s> registered\n",
			  icd->dev_name);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_video_register_device);

int rtscam_video_unregister_device(struct rtscam_video_device *icd)
{
	rtscam_video_device_remove(icd);

	v4l2_device_unregister(&icd->v4l2_dev);

	return 0;
}
EXPORT_SYMBOL_GPL(rtscam_video_unregister_device);
