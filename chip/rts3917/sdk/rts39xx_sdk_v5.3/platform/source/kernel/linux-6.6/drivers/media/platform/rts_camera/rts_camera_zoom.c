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

#define TAG "ZOOM"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "rts_camera_zoom_regs.h"
#include "linux/rts_camera_isp_info.h"
#include "rts_camera.h"
#include "rts_camera_zoom.h"
#include "rts_camera_zoom_reg.h"
#include "rts_camera_subdev.h"

#define BT601_FULL_RANGE 1
#define BT709_FULL_RANGE 3

#define RTS_ZOOM_DRV_NAME		"rts_zoom"
#define RTS_ZOOM_DEV_NAME		"rtszoom"

struct rtscam_zoom_info {
	__u32 zoom_src_w;
	__u32 zoom_src_h;
	__u32 zoom_dst_w;
	__u32 zoom_dst_h;
};

struct rtscam_zoom_stream_fmt {
	__u32 fmt;
	__u16 width;
	__u16 height;
};

#define RTS_ZOOM_MAX_STREAM		RTSCAM_MAX_STM_COUNT
#define RTS_ZOOM_RGB_YONLY_STREAM_ID	RTSCAM_RGB_YONLY_STRM_IDX
#define RTS_ZOOM_SRC_MAX_WIDTH		2592
#define RTS_ZOOM_SRC_MAX_HEIGHT		1944

struct rtscam_zoom_stream_info {
	__u32 stream_num;
	struct rtscam_zoom_stream_fmt fmt[RTS_ZOOM_MAX_STREAM];
};

struct rtscam_zoom {
	struct device *dev;
	struct rtscam_ge_device *gdev;
	struct mutex lock;

	void __iomem *base;

	struct rtscam_subdev_t subdev;

	struct reset_control *reset;
	struct clk *clk;

	struct rtscam_zoom_isp *isp;

	struct rtscam_zoom_stream_info dt_stream_info;
	struct rtscam_zoom_stream_info stream_info;

	int (*hook)(void *master, int id, void *arg);
	void *master;

	unsigned long streaming;
	struct v4l2_fract current_fps;
	struct rtscam_zoom_stream_fmt current_fmt[RTS_ZOOM_MAX_STREAM];
	struct rtscam_subdev_crop_info crop_info[RTS_ZOOM_MAX_STREAM];

	bool zoom_in_enable;
};

static struct rtscam_zoom *m_rtszoom;

static inline u32 rtscam_zoom_read_reg(struct rtscam_zoom *zoom, u32 offset)
{
	return ioread32(zoom->base + offset);
}

static inline void rtscam_zoom_write_reg(struct rtscam_zoom *zoom,
					 u32 value, u32 offset)
{
	iowrite32(value, zoom->base + offset);
}

static inline int __get_zoom_reg_offset(struct rtscam_zoom *zoom, int stream_id)
{
	int fmt;

	fmt = zoom->current_fmt[stream_id].fmt;
	if (fmt == RTSCAM_FORMAT_TYPE_RGB || fmt == RTSCAM_FORMAT_TYPE_Y_ONLY)
		stream_id = RTS_ZOOM_RGB_YONLY_STREAM_ID;
	return stream_id * ZOOM_CH_REG_OFFSET;
}

#define ZOOM_FILTER_COEF_NUM 20
static const u8 zoom_filter[6][ZOOM_FILTER_COEF_NUM] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x01, 0x20, 0x40, 0x60, 0xa0, 0xc0, 0xe0, 0xff },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x20,
	  0x30, 0x40, 0x40, 0x50, 0x60, 0x70, 0x80, 0x80, 0x80, 0x80 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x15, 0x20, 0x35, 0x40,
	  0x4b, 0x55, 0x55, 0x55, 0x55, 0x55, 0x56, 0x56, 0x55, 0x56 },
	{ 0x08, 0x10, 0x18, 0x20, 0x20, 0x28, 0x30, 0x38, 0x40, 0x40,
	  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40 },
	{ 0x20, 0x26, 0x2d, 0x33, 0x33, 0x3a, 0x40, 0x46, 0x33, 0x33,
	  0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x33, 0x34 },
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
	  0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34 },
};

static const u8 *get_zoom_filter(u32 scale)
{
	int i;
	const u32 thd[] = { 64 * 16, 128 * 16, 192 * 16, 256 * 16, 320 * 16 };

	WARN_ON(ARRAY_SIZE(thd) + 1 != ARRAY_SIZE(zoom_filter));
	for (i = 0; i < ARRAY_SIZE(thd); i++) {
		if (scale <= thd[i])
			break;
	}
	return zoom_filter[i];
}

static int rtscam_zoom_set_zoom_filter(struct rtscam_zoom *zoom,
					     int stream_id, u32 scale)
{
	unsigned long i;
	u32 offset;
	const u8 *filter = get_zoom_filter(scale);

	for (i = 0, offset = ZOOM_CH0_COEF0 +
			__get_zoom_reg_offset(zoom, stream_id);
			i < ZOOM_FILTER_COEF_NUM; i += 4, offset += 4) {
		u32 filter_value;

		filter_value = (filter[i] | filter[i + 1] << 8 |
				filter[i + 2] << 16 | filter[i + 3] << 24);
		rtscam_zoom_write_reg(zoom, filter_value, offset);
	}

	return 0;
}

static int rtscam_zoom_set_zoom(struct rtscam_zoom *zoom, int stream_id)
{
	u16 src_w, src_h;
	u16 dest_w, dest_h;
	u32 scale_h, scale_v;
	u32 crop_start_x, crop_start_y;
	u32 ch_offset = __get_zoom_reg_offset(zoom, stream_id);

	crop_start_x = zoom->crop_info[stream_id].start_x;
	crop_start_y = zoom->crop_info[stream_id].start_y;
	src_w = zoom->crop_info[stream_id].width;
	src_h = zoom->crop_info[stream_id].height;
	if (src_w + crop_start_x > zoom->isp->info.width ||
			src_h + crop_start_y > zoom->isp->info.height)
		return -EINVAL;
	dest_w = zoom->current_fmt[stream_id].width;
	dest_h = zoom->current_fmt[stream_id].height;
	scale_h = ((src_w << 10) / dest_w);
	scale_v = ((src_h << 10) / dest_h);
	if (scale_h > 0x1ffff || scale_v > 0x1ffff)
		return -EINVAL;
	if (stream_id && (scale_h < 0x400 || scale_v < 0x400))
		return -EINVAL;

	rtscam_zoom_write_reg(zoom, (crop_start_y << 16 | crop_start_x),
			ZOOM_CH0_CROP_START + ch_offset);
	rtscam_zoom_write_reg(zoom, (dest_w | dest_h << 16),
			      ZOOM_CH0_OUTPUT_SIZE + ch_offset);
	rtscam_zoom_write_reg(zoom, scale_h, ZOOM_CH0_STEP_H + ch_offset);
	rtscam_zoom_write_reg(zoom, scale_v, ZOOM_CH0_STEP_V + ch_offset);
	rtscam_zoom_set_zoom_filter(zoom, stream_id, min(scale_h, scale_v));
	rtscam_zoom_write_reg(zoom, LOAD_ZOOM_PARAM,
			      ZOOM_CH0_REG_LOAD + ch_offset);

	return 0;
}

static int rtscam_zoom_enable_zoom(struct rtscam_zoom *zoom, int stream_id)
{
	int ret;
	u32 tmp;
	u32 zoom_ctrl_reg = ZOOM_CH0_CTRL +
			__get_zoom_reg_offset(zoom, stream_id);

	if (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_YUV420_SEMIPLANAR) {
		tmp = rtscam_zoom_read_reg(zoom, ZOOM_SYS_IMAGE_MODE_SEL);
		rtscam_zoom_write_reg(zoom, tmp & ~(1 << stream_id),
				      ZOOM_SYS_IMAGE_MODE_SEL);
	} else if ((zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_YUV422_SEMIPLANAR) ||
		   (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_YUYV) ||
		   (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_YVYU) ||
		   (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_128BIT_3PIXEL)) {
		tmp = rtscam_zoom_read_reg(zoom, ZOOM_SYS_IMAGE_MODE_SEL);
		rtscam_zoom_write_reg(zoom, tmp | (1 << stream_id),
				      ZOOM_SYS_IMAGE_MODE_SEL);
	} else if (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_RGB) {
		rtscam_zoom_write_reg(zoom, 1, ZOOM_SYS_IMAGE_AI_CHAN_SEL);
	} else if (zoom->current_fmt[stream_id].fmt ==
				RTSCAM_FORMAT_TYPE_Y_ONLY) {
		rtscam_zoom_write_reg(zoom, 0, ZOOM_SYS_IMAGE_AI_CHAN_SEL);
	} else {
		return -EINVAL;
	}

	ret = rtscam_zoom_set_zoom(zoom, stream_id);
	if (ret)
		return ret;

	rtscam_zoom_write_reg(zoom, ENABLE_ZOOM, zoom_ctrl_reg);

	return 0;
}

static int rtscam_zoom_disable_zoom(struct rtscam_zoom *zoom, int stream_id)
{
	u32 count = 30;
	u32 zoom_ctrl;
	u32 zoom_ctrl_reg = ZOOM_CH0_CTRL +
			__get_zoom_reg_offset(zoom, stream_id);

	zoom_ctrl = rtscam_zoom_read_reg(zoom, zoom_ctrl_reg) | DISABLE_ZOOM;
	rtscam_zoom_write_reg(zoom, zoom_ctrl, zoom_ctrl_reg);
	while (count--) {
		if (!(rtscam_zoom_read_reg(zoom, zoom_ctrl_reg) & DISABLE_ZOOM))
			break;
		usleep_range(10000, 11000);
	}

	return 0;
}

static int rtscam_zoom_enable_stream(struct rtscam_zoom *zoom, int stream_id)
{
	int ret;
	struct v4l2_fract fps = {1, 0};

	if (zoom->streaming & (1 << stream_id))
		return -EBUSY;

	if (!zoom->current_fps.denominator || !zoom->current_fps.numerator) {
		rtsprintk(RTS_TRACE_ERROR, "set fps first\n");
		return -EPERM;
	}

	if (!zoom->streaming) {
		ret = zoom->isp->set_fps(zoom->isp, zoom->current_fps);
		if (ret)
			return ret;
	}

	ret = rtscam_zoom_enable_zoom(zoom, stream_id);
	if (ret) {
		if (!zoom->streaming)
			zoom->isp->set_fps(zoom->isp, fps);
		return ret;
	}

	set_bit(stream_id, &zoom->streaming);

	return 0;
}

static int rtscam_zoom_set_clock(struct rtscam_zoom *zoom, int enable)
{
	if (enable)
		clk_prepare_enable(zoom->clk);
	else
		clk_disable_unprepare(zoom->clk);
	return 0;
}


static int rtscam_zoom_disable_stream(struct rtscam_zoom *zoom, int stream_id)
{
	int ret;
	struct v4l2_fract fps = {1, 0};

	if ((zoom->streaming & (1 << stream_id)) == 0)
		return 0;

	ret = rtscam_zoom_disable_zoom(zoom, stream_id);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR,
			  "disable zoom at %d fail\n", stream_id);
		return ret;
	}
	clear_bit(stream_id, &zoom->streaming);
	if (!zoom->streaming)
		ret = zoom->isp->set_fps(zoom->isp, fps);

	return ret;
}

static int rtscam_zoom_subdev_set_stream(struct rtscam_subdev_t *subdev,
					 int stream_id, int enable)
{
	int ret;
	struct rtscam_zoom *zoom;

	if (!subdev)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (!zoom->isp)
		return -EINVAL;

	if (stream_id < 0 || stream_id >= zoom->stream_info.stream_num)
		return -EINVAL;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;
	if (enable) {
		rtscam_zoom_set_clock(zoom, 1);
		ret = rtscam_zoom_enable_stream(zoom, stream_id);
		if (ret)
			rtscam_zoom_set_clock(zoom, 0);
	} else {
		ret = rtscam_zoom_disable_stream(zoom, stream_id);
		if (!ret)
			rtscam_zoom_set_clock(zoom, 0);
	}
	mutex_unlock(&zoom->lock);

	return ret;
}

static int rtscam_zoom_subdev_enable(struct rtscam_subdev_t *subdev, int enable)
{
	return 0;
}

static int rtscam_zoom_subdev_set_fmt(struct rtscam_subdev_t *subdev,
				      int stream_id, u32 rts_code, u32 w, u32 h)
{
	int ret = 0;
	struct rtscam_zoom *zoom;

	if (!subdev)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;
	if (stream_id >= zoom->stream_info.stream_num) {
		ret = -EINVAL;
		goto out;
	}
	if (!w || w > zoom->stream_info.fmt[stream_id].width ||
	    !h || h > zoom->stream_info.fmt[stream_id].height ||
	    (rts_code & zoom->stream_info.fmt[stream_id].fmt) == 0) {
		ret = -EINVAL;
		goto out;
	}
	if (zoom->streaming & (1 << stream_id)) {
		ret = -EBUSY;
		goto out;
	}

	zoom->current_fmt[stream_id].fmt = rts_code;
	zoom->current_fmt[stream_id].width = w;
	zoom->current_fmt[stream_id].height = h;

out:
	mutex_unlock(&zoom->lock);

	return ret;
}

static int rtscam_zoom_subdev_set_fps(struct rtscam_subdev_t *subdev,
				int stream_id, struct v4l2_fract fps)
{
	int ret = 0;
	struct rtscam_zoom *zoom;

	if (!subdev || !fps.denominator || !fps.numerator)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	if (stream_id < 0 || stream_id >= zoom->stream_info.stream_num) {
		ret = -EINVAL;
		goto out;
	}

	if (!zoom->isp ||
	    zoom->isp->info.fps_max.numerator * fps.denominator >
		    fps.numerator * zoom->isp->info.fps_max.denominator) {
		ret = -EINVAL;
		goto out;
	}

	if (zoom->streaming) {
		ret = zoom->isp->set_fps(zoom->isp, fps);
		if (ret)
			goto out;
	}

	memcpy(&zoom->current_fps, &fps, sizeof(zoom->current_fps));
out:
	mutex_unlock(&zoom->lock);

	return ret;
}

static int
rtscam_zoom_subdev_set_hook(struct rtscam_subdev_t *subdev, void *master,
			    int (*hook)(void *master, int id, void *arg))
{
	struct rtscam_zoom *zoom;

	if (!subdev)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	/* do not use lock here */
	zoom->hook = hook;
	zoom->master = master;

	return 0;
}

static int rtscam_zoom_subdev_get_ive_ctrl(struct rtscam_subdev_t *subdev,
					   struct rtscam_soc_ive_ctrl *ctrl)
{
	u32 val_norm_mean;
	struct rtscam_zoom *zoom;

	if (!subdev || !ctrl)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	rtscam_zoom_set_clock(zoom, 1);
	ctrl->enable = rtscam_zoom_read_reg(zoom, IVE_ENABLE);
	val_norm_mean = rtscam_zoom_read_reg(zoom, IVE_NORM_MEAN);
	ctrl->normal_mean.r = val_norm_mean & 0xff;
	ctrl->normal_mean.g = (val_norm_mean >> 8) & 0xff;
	ctrl->normal_mean.b = (val_norm_mean >> 16) & 0xff;
	ctrl->normal_scale =
		rtscam_zoom_read_reg(zoom, IVE_NORM_SCALE) & 0xffff;
	ctrl->quant_len = rtscam_zoom_read_reg(zoom, IVE_QUANT_LEN) & 0x1f;
	ctrl->asym_inv_scale =
		rtscam_zoom_read_reg(zoom, IVE_ASYM_INV_SCALE) & 0x3ffff;
	ctrl->asym_zero_point =
		rtscam_zoom_read_reg(zoom, IVE_ASYM_ZERO_POINT) & 0xff;
	rtscam_zoom_set_clock(zoom, 0);

	mutex_unlock(&zoom->lock);

	return 0;
}

static int rtscam_zoom_subdev_set_ive_ctrl(struct rtscam_subdev_t *subdev,
					   struct rtscam_soc_ive_ctrl *ctrl)
{
	u32 val_norm_mean;
	struct rtscam_zoom *zoom;

	if (!subdev || !ctrl)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	val_norm_mean = (ctrl->normal_mean.r & 0xff) |
		((ctrl->normal_mean.g & 0xff) << 8) |
		((ctrl->normal_mean.b & 0xff) << 16);

	rtscam_zoom_set_clock(zoom, 1);

	rtscam_zoom_write_reg(zoom, val_norm_mean, IVE_NORM_MEAN);
	rtscam_zoom_write_reg(zoom, ctrl->normal_scale & 0xffff,
			      IVE_NORM_SCALE);
	rtscam_zoom_write_reg(zoom, ctrl->quant_len & 0x1f,
			      IVE_QUANT_LEN);
	rtscam_zoom_write_reg(zoom, ctrl->asym_inv_scale & 0x3ffff,
			      IVE_ASYM_INV_SCALE);
	rtscam_zoom_write_reg(zoom, ctrl->asym_zero_point & 0xff,
			      IVE_ASYM_ZERO_POINT);
	rtscam_zoom_write_reg(zoom, ctrl->enable, IVE_ENABLE);

	rtscam_zoom_set_clock(zoom, 0);

	mutex_unlock(&zoom->lock);

	return 0;
}

static int rtscam_zoom_set_color_range(struct rtscam_zoom *zoom, int range)
{
	u32 yr, yg, yb;
	u32 ur, ug, ub;
	u32 vr, vg, vb;
	u32 y_offset;

	if (range == BT601_FULL_RANGE) {
		yr = 256;
		ur = 0;
		vr = 359;
		yg = 256;
		ug = 88;
		vg = 183;
		yb = 256;
		ub = 454;
		vb = 0;
		y_offset = 0;
	} else if (range == BT709_FULL_RANGE) {
		yr = 256;
		ur = 0;
		vr = 403;
		yg = 256;
		ug = 48;
		vg = 120;
		yb = 256;
		ub = 475;
		vb = 0;
		y_offset = 0;
	} else {
		return -EINVAL;
	}

	rtscam_zoom_write_reg(zoom, yg << 16 | yr, YUV2RGB0);
	rtscam_zoom_write_reg(zoom, ur << 16 | yb, YUV2RGB1);
	rtscam_zoom_write_reg(zoom, ub << 16 | ug, YUV2RGB2);
	rtscam_zoom_write_reg(zoom, vg << 16 | vr, YUV2RGB3);
	rtscam_zoom_write_reg(zoom, vb, YUV2RGB4);
	rtscam_zoom_write_reg(zoom, y_offset, YUV2RGB_YOFFSET);

	return 0;
}

static int rtscam_zoom_subdev_set_crop(struct rtscam_subdev_t *subdev,
			int stream_id, struct rtscam_subdev_crop_info *crop)
{
	struct rtscam_zoom *zoom;

	if (!subdev || !crop)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);
	if (!zoom->isp)
		return -EINVAL;

	if (crop->start_x + crop->width > zoom->isp->info.width ||
		crop->start_y + crop->height > zoom->isp->info.height)
		return -EINVAL;

	if (!memcmp(&zoom->crop_info[stream_id], crop, sizeof(*crop)))
		return 0;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	memcpy(&zoom->crop_info[stream_id], crop, sizeof(*crop));

	if (crop->mode) {
		if (rtscam_zoom_set_zoom(zoom, stream_id)) {
			mutex_unlock(&zoom->lock);
			rtsprintk(RTS_TRACE_ERROR, "invalid crop info\n");
			return -EINVAL;
		}
	}
	mutex_unlock(&zoom->lock);
	return 0;
}

static int rtscam_zoom_subdev_get_crop(struct rtscam_subdev_t *subdev,
			int stream_id, struct rtscam_subdev_crop_info *crop)
{
	struct rtscam_zoom *zoom;

	if (!subdev || !crop)
		return -EINVAL;

	zoom = container_of(subdev, struct rtscam_zoom, subdev);

	if (!zoom->isp)
		return -EINVAL;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	memcpy(crop, &zoom->crop_info[stream_id], sizeof(*crop));

	mutex_unlock(&zoom->lock);
	return 0;
}

static int rtscam_zoom_register_subdev_unlock(struct rtscam_zoom *zoom)
{
	int i;
	int ret;
	struct rtscam_subdev_t *subdev;
	struct rtscam_zoom_stream_info *info;
	struct rtscam_subdev_fps *fps;
	int flag_regist = 0;

	if (!zoom)
		return -EINVAL;

	if (!zoom->isp) {
		rtsprintk(RTS_TRACE_ERROR,
			"no isp available, please regist it\n");
		return -EINVAL;
	}

	subdev = &zoom->subdev;
	if (subdev->master) {
		rtscam_unregister_subdev_ext(subdev);
		flag_regist = 1;
	}

	memset(subdev, 0, sizeof(*subdev));

	if (zoom->isp->info.fps_max.denominator >
	    RTSCAM_SOC_MAX_FPS * zoom->isp->info.fps_max.numerator)
		return -EINVAL;

	fps = &subdev->desc.fps;
	fps->type = RTSCAM_SUBDEV_FPS_CONTINUOUS;
	fps->stepwise.step.numerator = RTSCAM_SOC_FPS_CVRT_NUMERATOR;
	fps->stepwise.step.denominator = 1;
	fps->stepwise.min = zoom->isp->info.fps_min;
	fps->stepwise.max = zoom->isp->info.fps_max;

	info = &zoom->stream_info;
	for (i = 0; i < info->stream_num; i++) {
		subdev->desc.strms[i].format_bitmap = info->fmt[i].fmt;
		subdev->desc.strms[i].width = info->fmt[i].width;
		subdev->desc.strms[i].height = info->fmt[i].height;
	}

	subdev->dev = zoom->dev;
	subdev->enable = rtscam_zoom_subdev_enable;
	subdev->set_stream = rtscam_zoom_subdev_set_stream;
	subdev->set_fmt = rtscam_zoom_subdev_set_fmt;
	subdev->set_fps = rtscam_zoom_subdev_set_fps;
	subdev->set_hook = rtscam_zoom_subdev_set_hook;
	subdev->get_ive_ctrl = rtscam_zoom_subdev_get_ive_ctrl;
	subdev->set_ive_ctrl = rtscam_zoom_subdev_set_ive_ctrl;
	subdev->set_crop = rtscam_zoom_subdev_set_crop;
	subdev->get_crop = rtscam_zoom_subdev_get_crop;

	if (flag_regist)
		ret = rtscam_register_subdev_ext(subdev);
	else
		ret = rtscam_register_subdev(subdev);
	if (ret)
		rtsprintk(RTS_TRACE_ERROR,
			  "fail to register rts camera subdev\n");
	return ret;
}

static int rtscam_zoom_unregister_subdev_unlock(struct rtscam_zoom *zoom)
{
	if (!zoom)
		return -EINVAL;

	if (!zoom->subdev.master)
		return 0;

	return rtscam_unregister_subdev(&zoom->subdev);
}

static int rtscam_zoom_on_event(void *master, int id, void *arg)
{
	struct rtscam_zoom *zoom = master;

	if (!zoom)
		return -EINVAL;

	switch (id) {
	case RTSCAM_ZOOM_EVT_COLOR_RANGE_CHANGED:
		rtscam_zoom_set_color_range(zoom, *(int *)arg);
		break;
	default:
		if (zoom->hook)
			return zoom->hook(zoom->subdev.master, id, arg);
	}

	return 0;
}

int rtscam_zoom_refresh_isp_info(struct rtscam_zoom_isp *isp, int force_reset)
{
	u32 i;
	int ret;
	struct rtscam_subdev_t *subdev;
	struct rtscam_subdev_fps *fps;
	struct rtscam_zoom *zoom = m_rtszoom;
	struct rtscam_zoom_stream_info *info;
	const struct rtscam_zoom_stream_info *dt_info;

	if (!zoom || !isp || !zoom->isp)
		return -EINVAL;

	if (!isp->dev || !isp->set_fps || !isp->set_hook ||
		!isp->info.fps_max.denominator ||
		!isp->info.fps_max.numerator
		|| !isp->info.width || !isp->info.height)
		return -EINVAL;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;

	if (isp != zoom->isp) {
		rtsprintk(RTS_TRACE_ERROR, "invalid isp to refresh\n");
		mutex_unlock(&zoom->lock);
		return -EINVAL;
	}

	if (!isp->master || isp->master != zoom) {
		rtsprintk(RTS_TRACE_ERROR, "invalid isp master\n");
		mutex_unlock(&zoom->lock);
		return -EINVAL;
	}

	if (isp->info.fps_max.denominator > RTSCAM_SOC_MAX_FPS
		* isp->info.fps_max.numerator) {
		mutex_unlock(&zoom->lock);
		return -EINVAL;
	}

	info = &zoom->stream_info;
	dt_info = &zoom->dt_stream_info;
	info->stream_num = dt_info->stream_num;
	for (i = 0; i < info->stream_num; i++) {
		info->fmt[i].fmt = dt_info->fmt[i].fmt;
		if (i == 0 && zoom->zoom_in_enable) {
			info->fmt[i].width = dt_info->fmt[i].width;
			info->fmt[i].height = dt_info->fmt[i].height;
		} else {
			info->fmt[i].width = min(dt_info->fmt[i].width,
						 isp->info.width);
			info->fmt[i].height = min(dt_info->fmt[i].height,
						  isp->info.height);
		}
		zoom->crop_info[i].start_x = 0;
		zoom->crop_info[i].start_y = 0;
		zoom->crop_info[i].width = isp->info.width;
		zoom->crop_info[i].height = isp->info.height;
	}

	subdev = &zoom->subdev;
	ret = rtscam_unregister_subdev_update(subdev, force_reset);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR, "unregister subdev to fresh fail\n");
		mutex_unlock(&zoom->lock);
		return ret;
	}

	fps = &subdev->desc.fps;
	fps->type = RTSCAM_SUBDEV_FPS_CONTINUOUS;
	fps->stepwise.step.numerator = RTSCAM_SOC_FPS_CVRT_NUMERATOR;
	fps->stepwise.step.denominator = 1;
	fps->stepwise.min = zoom->isp->info.fps_min;
	fps->stepwise.max = zoom->isp->info.fps_max;
	for (i = 0; i < info->stream_num; i++) {
		subdev->desc.strms[i].format_bitmap = info->fmt[i].fmt;
		subdev->desc.strms[i].width = info->fmt[i].width;
		subdev->desc.strms[i].height = info->fmt[i].height;
	}

	ret = rtscam_register_subdev_update(subdev, force_reset);
	mutex_unlock(&zoom->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_zoom_refresh_isp_info);

int rtscam_zoom_register_isp(struct rtscam_zoom_isp *isp)
{
	u32 i;
	int ret;
	struct rtscam_zoom *zoom = m_rtszoom;
	struct rtscam_zoom_stream_info *info;
	const struct rtscam_zoom_stream_info *dt_info;

	if (!zoom || !isp)
		return -EINVAL;
	if (!isp->dev || !isp->set_fps || !isp->set_hook ||
		!isp->info.fps_max.denominator ||
		!isp->info.fps_max.numerator ||
		!isp->info.width || !isp->info.height)
		return -EINVAL;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;
	if (zoom->isp) {
		rtsprintk(RTS_TRACE_ERROR, "there is already one isp\n");
		mutex_unlock(&zoom->lock);
		return -EINVAL;
	}
	zoom->isp = isp;
	get_device(isp->dev);
	isp->master = zoom;
	isp->set_hook(isp, zoom, rtscam_zoom_on_event);

	info = &zoom->stream_info;
	dt_info = &zoom->dt_stream_info;
	info->stream_num = dt_info->stream_num;
	for (i = 0; i < info->stream_num; i++) {
		info->fmt[i].fmt = dt_info->fmt[i].fmt;
		if (i == 0 && zoom->zoom_in_enable) {
			info->fmt[i].width = dt_info->fmt[i].width;
			info->fmt[i].height = dt_info->fmt[i].height;
		} else {
			info->fmt[i].width = min(dt_info->fmt[i].width,
						 isp->info.width);
			info->fmt[i].height = min(dt_info->fmt[i].height,
						  isp->info.height);
		}
		zoom->crop_info[i].start_x = 0;
		zoom->crop_info[i].start_y = 0;
		zoom->crop_info[i].width = isp->info.width;
		zoom->crop_info[i].height = isp->info.height;
	}

	ret = rtscam_zoom_register_subdev_unlock(zoom);
	if (ret) {
		isp->set_hook(isp, NULL, NULL);
		put_device(isp->dev);
		zoom->isp = NULL;
		isp->master = NULL;
	}
	mutex_unlock(&zoom->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_zoom_register_isp);

int rtscam_zoom_unregister_isp(struct rtscam_zoom_isp *isp)
{
	int ret;
	struct rtscam_zoom *zoom;

	if (!isp)
		return -EINVAL;
	zoom = isp->master;
	if (!zoom)
		return -EINVAL;

	if (mutex_lock_interruptible(&zoom->lock))
		return -ERESTARTSYS;
	if (zoom->isp != isp) {
		rtsprintk(RTS_TRACE_ERROR, "invalid isp to unregister\n");
		mutex_unlock(&zoom->lock);
		return -EINVAL;
	}
	isp->set_hook(isp, NULL, NULL);
	put_device(isp->dev);
	zoom->isp = NULL;
	isp->master = NULL;

	ret = rtscam_zoom_unregister_subdev_unlock(zoom);

	mutex_unlock(&zoom->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rtscam_zoom_unregister_isp);

static int rtscam_zoom_parse_stream_dt(struct rtscam_zoom_stream_info *info,
				       struct device_node *dev_node)
{
	int ret;
	u32 i;
	struct device_node *stream_node;
	struct device_node *stream_parent;

	stream_parent = of_find_node_by_name(dev_node, "streams");
	if (!stream_parent) {
		rtsprintk(RTS_TRACE_ERROR, "device node streams not found\n");
		return -EINVAL;
	}

	i = 0;
	for_each_child_of_node(stream_parent, stream_node) {
		u32 w, h, fmt;

		if (i >= RTS_ZOOM_MAX_STREAM) {
			rtsprintk(RTS_TRACE_ERROR, "two many stream info\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(stream_node, "width", &w);
		if (ret)
			break;

		ret = of_property_read_u32(stream_node, "height", &h);
		if (ret)
			break;

		ret = of_property_read_u32(stream_node, "fmt", &fmt);
		if (ret)
			break;

		info->fmt[i].fmt = fmt;
		info->fmt[i].width = w;
		info->fmt[i].height = h;
		i++;
	}

	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "parse stream info fail\n");
		return ret;
	}
	if (i == 0) {
		rtsprintk(RTS_TRACE_ERROR, "no stream info at dtb\n");
		return ret;
	}
	info->stream_num = i;

	for (i = 0; i < info->stream_num; i++) {
		if (!(info->fmt[i].fmt & RTSCAM_FORMAT_TYPE_RGB) &&
				!(info->fmt[i].fmt & RTSCAM_FORMAT_TYPE_Y_ONLY))
			continue;
		if (i != info->stream_num - 1) {
			rtsprintk(RTS_TRACE_ERROR, "invalid stream info, "
				"please adjust RGB/YONLY stream location\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < info->stream_num; i++)
		rtsprintk(RTS_TRACE_DEBUG, "stream %u: width: %u, height: %u\n",
			  i, info->fmt[i].width, info->fmt[i].height);

	return 0;
}

static int rtscam_zoom_parse_filter_dt(struct device_node *np)
{
	const char *p = "zoom-filters";

	if (!of_find_property(np, p, NULL))
		return 0;

	if (of_property_count_elems_of_size(np, p, 1) != sizeof(zoom_filter)) {
		rtsprintk(RTS_TRACE_ERROR, "zoom filter fmt in dts is error\n");
		return 0;
	}
	rtsprintk(RTS_TRACE_DEBUG, "loading zoom filter from dts!\n");
	of_property_read_variable_u8_array(np, p, (u8 *)zoom_filter,
					   sizeof(zoom_filter),
					   sizeof(zoom_filter));
	return 0;
}

static int rtscam_zoom_parse_dt(struct rtscam_zoom *zoom)
{
	int ret;
	u32 freq;
	struct device_node *dev_node;

	if (!zoom)
		return -EINVAL;

	dev_node = zoom->dev->of_node;

	zoom->zoom_in_enable = of_property_read_bool(dev_node,
						     "zoom-in-enable");

	ret = of_property_read_u32(dev_node, "clock-frequency", &freq);
	if (ret)
		freq = 360000000;
	freq = clk_round_rate(zoom->clk, freq);
	clk_set_rate(zoom->clk, freq);

	ret = rtscam_zoom_parse_stream_dt(&zoom->dt_stream_info, dev_node);
	if (ret)
		return ret;
	memcpy(&zoom->stream_info, &zoom->dt_stream_info,
			sizeof(zoom->stream_info));
	return rtscam_zoom_parse_filter_dt(dev_node);
}

static int rtscam_zoom_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_zoom *zoom = rtscam_ge_get_drvdata(gdev);

	filp->private_data = zoom;
	return 0;
}

static int rtscam_zoom_close(struct file *filp)
{
	struct rtscam_zoom *zoom = filp->private_data;

	filp->private_data = NULL;

	if (!zoom)
		return -EINVAL;

	return 0;
}

static long rtscam_zoom_do_ioctl(struct file *filp, unsigned int cmd,
				    void *arg)
{
	u32 val;
	int fmt;
	int ret = -1;
	struct rts_zoom_color_range_cfg *range_cfg;

	struct rtscam_zoom *zoom = filp->private_data;

	rtscam_zoom_set_clock(zoom, 1);
	switch (cmd) {
	case RTSZOOMIOC_SET_LIMIT_COLOR_RANGE:
		range_cfg = arg;
		fmt = zoom->current_fmt[range_cfg->stream_id].fmt;
		if (fmt == RTSCAM_FORMAT_TYPE_RGB ||
		    fmt == RTSCAM_FORMAT_TYPE_Y_ONLY) {
			ret = -ERANGE;
			break;
		}
		val = rtscam_zoom_read_reg(zoom, ZOOM_SYS_YUV_TRAN_CTRL);
		val &= ~(LIMIT_MASK << range_cfg->stream_id);
		if (range_cfg->limit_enable)
			val |= LIMIT_MASK << range_cfg->stream_id;
		rtscam_zoom_write_reg(zoom, val, ZOOM_SYS_YUV_TRAN_CTRL);
		ret = 0;
		break;
	case RTSZOOMIOC_GET_LIMIT_COLOR_RANGE:
		range_cfg = arg;
		fmt = zoom->current_fmt[range_cfg->stream_id].fmt;
		if (fmt == RTSCAM_FORMAT_TYPE_RGB ||
		    fmt == RTSCAM_FORMAT_TYPE_Y_ONLY) {
			ret = -ERANGE;
			break;
		}
		val = rtscam_zoom_read_reg(zoom, ZOOM_SYS_YUV_TRAN_CTRL);
		range_cfg->limit_enable = !!(val & BIT(range_cfg->stream_id));
		ret = 0;
		break;
	case RTSZOOMIOC_SET_DYNAMIC_FPS:
		ret = rtscam_zoom_subdev_set_fps(&zoom->subdev, 0,
						*(struct v4l2_fract *)arg);
		if (ret)
			break;

		if (zoom->hook)
			ret = zoom->hook(zoom->subdev.master,
					RTSCAM_ZOOM_EVT_DYN_FPS_CHANGED, arg);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			  "Unknown[rtscam] ioctl 0x%08x, type = '%c' nr = 0x%x\n",
			  cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}
	rtscam_zoom_set_clock(zoom, 0);

	return ret;
}

static long rtscam_zoom_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, rtscam_zoom_do_ioctl);
}

static struct rtscam_ge_file_operations rtscam_zoom_fops = {
	.owner = THIS_MODULE,
	.open = rtscam_zoom_open,
	.release = rtscam_zoom_close,
	.ioctl = rtscam_zoom_ioctl,
};

static void rtscam_zoom_remove_dev(struct rtscam_zoom *zoom)
{
	struct rtscam_ge_device *gdev;

	if (!zoom->gdev)
		return;

	gdev = zoom->gdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
}

static int rtscam_zoom_create_device(struct rtscam_zoom *zoom)
{
	struct rtscam_ge_device *gdev;
	int ret;

	if (zoom->gdev)
		return 0;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_ZOOM_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(zoom->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_zoom_fops;

	rtscam_ge_set_drvdata(gdev, zoom);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	zoom->gdev = gdev;

	return 0;
}

static int rtscam_zoom_init_hardware(struct rtscam_zoom *zoom)
{
	rtscam_zoom_set_clock(zoom, 1);
	rtscam_zoom_write_reg(zoom, 0x1, ZOOM_SYS_CONTROL);
	rtscam_zoom_write_reg(zoom, 0x101, ZOOM_SYS_SPEED_CTRL);
	rtscam_zoom_write_reg(zoom, 0x4, ZOOM_SYS_LAST_BREAK);
	rtscam_zoom_set_clock(zoom, 0);
	return 0;
}

static int rtscam_zoom_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct rtscam_zoom *zoom;
	struct device *dev = &pdev->dev;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	zoom = devm_kzalloc(dev, sizeof(*zoom), GFP_KERNEL);
	if (!zoom) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera zoom object\n");
		return -ENOMEM;
	}
	zoom->dev = get_device(dev);

	res = platform_get_resource(to_platform_device(dev), IORESOURCE_MEM, 0);
	if (res == NULL) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}
	zoom->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(zoom->base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap isp resource\n");
		return PTR_ERR(zoom->base);
	}

	mutex_init(&zoom->lock);

	zoom->reset = devm_reset_control_get(zoom->dev, "zoom_reset");
	if (IS_ERR(zoom->reset)) {
		rtsprintk(RTS_TRACE_ERROR, "get zoom reset fail\n");
		return ret;
	}
	reset_control_reset(zoom->reset);

	zoom->clk = devm_clk_get(zoom->dev, "zoom_clk");
	if (IS_ERR(zoom->clk)) {
		rtsprintk(RTS_TRACE_ERROR, "get zoom clk fail\n");
		return ret;
	}

	ret = rtscam_zoom_parse_dt(zoom);
	if (ret)
		return ret;
	ret = rtscam_zoom_init_hardware(zoom);
	if (ret)
		return ret;

	if (rtscam_zoom_create_device(zoom))
		rtsprintk(RTS_TRACE_ERROR, "fail to create zoom dev\n");

	platform_set_drvdata(pdev, zoom);
	m_rtszoom = zoom;

	return 0;
}

static int rtscam_zoom_remove(struct platform_device *pdev)
{
	struct rtscam_zoom *zoom = platform_get_drvdata(pdev);

	rtscam_unregister_subdev(&zoom->subdev);
	rtscam_zoom_remove_dev(zoom);
	put_device(zoom->dev);
	m_rtszoom = NULL;

	return 0;
}

static const struct of_device_id rtscam_zoom_ids[] = {
	{ .compatible = "realtek,rts3917-zoom" },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_zoom_driver = {
	.driver = {
		.name = RTS_ZOOM_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_zoom_ids),
	},
	.probe = rtscam_zoom_probe,
	.remove = rtscam_zoom_remove,
};

module_platform_driver(rtscam_zoom_driver);

MODULE_DESCRIPTION("Realsil zoom device driver");
MODULE_AUTHOR("Grant Shen <grant_shen@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1.0");
MODULE_ALIAS("platform:" RTS_ZOOM_DRV_NAME);
