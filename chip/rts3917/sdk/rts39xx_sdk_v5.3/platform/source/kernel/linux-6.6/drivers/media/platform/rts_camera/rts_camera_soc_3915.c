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

#include <linux/reset.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include "linux/rts_camera_soc.h"
#include "rts_camera_soc_priv.h"
#include "rts_camera_subdev.h"
#include "rts_camera_soc_3915_regs.h"
#include "rts_hw_id.h"

static struct rtscam_soc_dev *to_rsocdev_obj(
		struct rtscam_soc_video_in *video_in)
{
	return video_in->priv;
}

static u32 rtscam_socdev_read_reg(
		struct rtscam_soc_video_in *video_in, off_t reg)
{
	return le32_to_cpu(ioread32(video_in->base + reg));
}

static void rtscam_socdev_write_reg(struct rtscam_soc_video_in *video_in,
		u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), video_in->base + reg);
}

static int rtscam_socdev_set_fps_dynamic(struct rtscam_video_stream *stream,
		struct v4l2_fract fps, int flag)
{
	return rtscam_soc_set_fps(stream, fps);
}

static void rtscam_socdev_isp_control(struct rtscam_soc_video_in *video_in,
		u8 idx, int enable)
{
	u32 reg;

	if (idx > RTSCAM_RGB_YONLY_STRM_IDX)
		return;

	if (idx == RTSCAM_RGB_YONLY_STRM_IDX)
		reg = RTS_REG_ISP_RGB_YONLY_STREAM_EN;
	else
		reg = RTS_REG_ISP_STREAM_EN_BASE + 4 * idx;

	if (enable)
		rtscam_socdev_write_reg(video_in, 1, reg);
	else
		rtscam_socdev_write_reg(video_in, 0, reg);
}

static void rtscam_socdev_reset_isp_reg(
		struct rtscam_soc_video_in *video_in, u8 idx)
{
	u32 reg;

	if (idx == RTSCAM_RGB_YONLY_STRM_IDX)
		reg = RTS_REG_ISP_RGB_YONLY_STREAM_RST;
	else
		reg = RTS_REG_ISP_STREAM_RST_BASE + 4 * idx;

	rtscam_socdev_write_reg(video_in, 1, reg);
	rtscam_socdev_write_reg(video_in, 0, reg);

	usleep_range(9900, 10000);
}

static int rtscam_socdev_submit_buffer(struct rtscam_soc_video_in *video_in,
		struct rtscam_video_stream *stream,
		u32 phy_addr_y, u32 phy_addr_uv, int nr)
{
	u32 reg_addr;
	u32 reg_addr_uv;
	u32 reg_status;
	u32 status;

	if (!video_in || !stream)
		return -EINVAL;

	if (stream->vin_mode == RTS_VIN_MODE_RING_MEM && !stream->streamid) {
		u32 len_y = 0, len_uv = 0;
		u32 height = stream->ring_buf_height;

		if (phy_addr_y != 0) {
			len_y = stream->user_width * height;
			len_uv = stream->bytesperline * height - len_y;
		}

		if (stream->rts_code != RTSCAM_FORMAT_TYPE_YUYV &&
			stream->rts_code != RTSCAM_FORMAT_TYPE_YVYU) {
			rtscam_socdev_write_reg(video_in,
				phy_addr_y + len_y, RTS_REG_RINGBUF_ADDRUV);
			rtscam_socdev_write_reg(video_in,
				len_uv, RTS_REG_RINGBUF_LENUV);
		} else
			len_y += len_uv;

		rtscam_socdev_write_reg(video_in,
				phy_addr_y, RTS_REG_RINGBUF_ADDRY);
		rtscam_socdev_write_reg(video_in,
				len_y, RTS_REG_RINGBUF_LENY);

		return 0;
	}

	if (rtscam_soc_is_yuv(stream->rts_code)) {
		u32 idx = 4 * nr;
		u32 offset = stream->streamid *
				RTS_REG_YUV_FRAME_START_ADDRESS_INTERVAL;
		u32 size = stream->user_width * stream->user_height;

		if (phy_addr_y != 0 && phy_addr_uv == 0)
			phy_addr_uv = phy_addr_y + size;

		reg_addr =
			RTS_REG_YUV_FRAME_Y_START_ADDRESS_BASE + offset + idx;
		reg_addr_uv =
			RTS_REG_YUV_FRAME_UV_START_ADDRESS_BASE + offset + idx;
		reg_status = RTS_REG_YUV_FRAME_BUFFER_STATUS_BASE +
			stream->streamid * 4;
		status = 0x30 << (8 * nr);

		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);
		if (stream->rts_code != RTSCAM_FORMAT_TYPE_YUYV &&
			stream->rts_code != RTSCAM_FORMAT_TYPE_YVYU)
			rtscam_socdev_write_reg(
				video_in, phy_addr_uv, reg_addr_uv);
	} else if (rtscam_soc_is_128bit3pixel(stream->rts_code)) {
		u32 idx = 4 * nr;
		u32 offset = stream->streamid *
				RTS_REG_YUV_FRAME_START_ADDRESS_INTERVAL;

		reg_addr =
			RTS_REG_YUV_FRAME_Y_START_ADDRESS_BASE + offset + idx;
		reg_status = RTS_REG_YUV_FRAME_BUFFER_STATUS_BASE +
			stream->streamid * 4;
		status = 0x30 << (8 * nr);

		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);
	} else if (rtscam_soc_is_rgb(stream->rts_code)) {
		u32 offset = stream->user_width * stream->user_height;
		if (phy_addr_y == 0)
			offset = 0;

		reg_status = RTS_REG_RGB_YONLY_FRAME_BUFFER_STATUS;
		status = 0x18 << (8 * nr);

		reg_addr = RTS_REG_RGB_R_FRAME_START_ADDRESS_BASE + 4 * nr;
		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);

		phy_addr_y += offset;
		reg_addr = RTS_REG_RGB_G_FRAME_START_ADDRESS_BASE  + 4 * nr;
		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);

		phy_addr_y += offset;
		reg_addr = RTS_REG_RGB_B_FRAME_START_ADDRESS_BASE  + 4 * nr;
		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);
	} else if (rtscam_soc_is_yonly(stream->rts_code)) {
		reg_status = RTS_REG_RGB_YONLY_FRAME_BUFFER_STATUS;
		status = 0x18 << (8 * nr);

		reg_addr = RTS_REG_RGB_R_FRAME_START_ADDRESS_BASE + 4 * nr;
		rtscam_socdev_write_reg(video_in, phy_addr_y, reg_addr);
	} else {
		rtsprintk(RTS_TRACE_BUF,
			  "invalid stream format (%d)\n", stream->rts_code);
		return -EINVAL;
	}

	rtscam_socdev_write_reg(video_in, status, reg_status);
	status = rtscam_socdev_read_reg(video_in, reg_status);
	rtsprintk(RTS_TRACE_DEBUG,
		  "clear frame status, reg = 0x%08x, value = 0x%08x\n",
		  reg_status, status);
	return 0;
}

static int rtscam_socdev_process_frame(struct rtscam_soc_video_in *video_in,
		struct rtscam_video_stream *stream, int frameid)
{
	struct rtscam_soc_dev *rsocdev = to_rsocdev_obj(video_in);
	struct rtscam_soc_slot_info *info;
	struct rtscam_video_buffer *rbuf;
	u32 reg;
	u32 status;
	u32 mask;
	u8 idx = rtscam_soc_get_stream_reg_index(stream);
	unsigned long bytesused;
	int skip;

	info = rtscam_soc_get_skip_info(rsocdev, stream->streamid);

	if (frameid < 0 || frameid >= info->slot_num)
		return -EINVAL;

	reg = RTS_REG_YUV_FRAME_BUFFER_STATUS_BASE + 4 * idx;
	status = rtscam_socdev_read_reg(video_in, reg);

	if (rtscam_soc_is_yuv(stream->rts_code) ||
			rtscam_soc_is_128bit3pixel(stream->rts_code))
		mask = 0x20 << (8 * frameid);
	else
		mask = 0x10 << (8 * frameid);
	if (!(mask & status))
		return 0;

	rbuf = info->slots[frameid];
	info->slots[frameid] = NULL;

	if (rtscam_soc_is_yuv(stream->rts_code) ||
			rtscam_soc_is_128bit3pixel(stream->rts_code))
		mask = 1 << (8 * frameid + 4);
	else
		mask = 1 << (8 * frameid + 3);
	if (mask & status) {
		rtscam_submit_buffer(stream, rbuf);
		rtscam_soc_inc_errors(stream);

		rtsprintk(RTS_TRACE_WARNING,
			  "frame error:stream<%d> frame<%d> : 0x%08x\n",
			  stream->streamid, frameid, status);
		return -EINVAL;
	}

	if (rbuf == NULL) {
		rtsprintk(RTS_TRACE_WARNING,
			  "frameid[%d] is invalid\n", frameid);
		return -EINVAL;
	}

	if (rtscam_soc_get_drops(stream)) {
		rtscam_soc_dec_drops(stream);
		rtscam_soc_inc_skips(stream);
		rtscam_submit_buffer(stream, rbuf);
		rtsprintk(RTS_TRACE_VIDEO, "[%d]--\n", stream->streamid);
		return 0;
	}

	skip = rtscam_skip_frame(stream);

	rtsprintk(RTS_TRACE_VIDEO, "frameid = %d, skip = %d\n", frameid, skip);
	if (skip) {
		rtscam_soc_inc_skips(stream);
		rtscam_submit_buffer(stream, rbuf);
		return 0;
	}

	bytesused = stream->sizeimage;
	rtscam_soc_inc_frames(stream);
	rtscam_buffer_done(stream, rbuf, bytesused);
	rtscam_submit_buffer(stream, NULL);
	return 0;
}

static int __pre_process_rgb_yonly_irq(struct rtscam_soc_video_in *video_in)
{
	const u32 reg = RTS_REG_INT_FLAG_RGB_YONLY_HOST;
	u32 status;
	u32 mask = 0;
	int i;
	struct rtscam_video_stream *stream = NULL;

	status = rtscam_socdev_read_reg(video_in, reg);
	if (!status)
		return 0;

	/*
	 * clear reserved isp_host interrupt
	 * clear AXI overflow
	 */
	mask = 0xffffff07;
	if (status & mask) {
		rtscam_socdev_write_reg(video_in, mask, reg);
		return 1;
	}

	/*ddr len overflow*/
	mask = 0xe0;
	if (status & mask) {
		rtsprintk(RTS_TRACE_ERROR,
			"rgb/yonly ddr len overflow : 0x%08x\n", status);
		rtscam_socdev_write_reg(video_in, mask, reg);
		rtscam_soc_inc_abnormals(to_rsocdev_obj(video_in),
				RTSCAM_RGB_YONLY_STRM_IDX);
		return 1;
	}

	mask = 0x8;
	if (status & mask) {
		rtsprintk(RTS_TRACE_WARNING, "rgb/yonly overflow : 0x%08x\n",
				  status);
		rtscam_socdev_write_reg(video_in, mask, reg);
		rtscam_soc_inc_overflow(to_rsocdev_obj(video_in),
				RTSCAM_RGB_YONLY_STRM_IDX);

		stream = rtscam_soc_get_stream_from_reg_index(
				to_rsocdev_obj(video_in),
				RTSCAM_RGB_YONLY_STRM_IDX);
		spin_lock(&stream->lock);
		rtscam_submit_buffer(stream, NULL);
		spin_unlock(&stream->lock);
		return 1;
	}

	mask = 1 << 4;
	if (status & mask) {
		int cnt;

		rtscam_socdev_write_reg(video_in, mask, reg);

		stream = rtscam_soc_get_stream_from_reg_index(
				to_rsocdev_obj(video_in),
				RTSCAM_RGB_YONLY_STRM_IDX);
		if (stream == NULL) {
			rtsprintk(RTS_TRACE_WARNING, "no stream found\n");
			return 1;
		}

		spin_lock(&stream->lock);
		mask = 0x3;
		i = mask & rtscam_socdev_read_reg(video_in,
				RTS_REG_RGB_R_CUR_FRAME_INDEX_OFFSET);
		for (cnt = 0; cnt < RTSCAM_SOC_HW_SLOT_NUM; cnt++) {
			rtscam_socdev_process_frame(video_in, stream, i++);
			i %= RTSCAM_SOC_HW_SLOT_NUM;
		}
		spin_unlock(&stream->lock);

		return 1;
	}
	return 0;
}

static int rtscam_socdev_process_isp_irq(
		struct rtscam_soc_video_in *video_in)
{
	const u32 reg_isp = RTS_REG_INT_FLAG_ISP_HOST;
	u32 status;
	u32 mask = 0;
	int i, cnt;
	struct rtscam_video_stream *stream = NULL;

	if (__pre_process_rgb_yonly_irq(video_in))
		return IRQ_HANDLED;

	status = rtscam_socdev_read_reg(video_in, reg_isp);

	if (!status)
		return IRQ_NONE;

	/*
	 * clear reserved isp_host interrupt
	 * clear AXI overflow
	 */
	stream = rtscam_soc_get_stream_from_reg_index(
					to_rsocdev_obj(video_in), 0);
	if (stream->vin_mode == RTS_VIN_MODE_RING_MEM)
		mask = 0xff00333f;
	else
		mask = 0xff003333;

	if (status & mask) {
		rtscam_socdev_write_reg(video_in, mask, reg_isp);
		return IRQ_HANDLED;
	}

	/*check isp overflow*/
	for (i = 0; i < RTSCAM_YUV_MAX_STRM_NUM; i++) {
		/*ddr len overflow*/
		mask = 0x3 << (16 + 2 * i);
		if (status & mask) {
			rtsprintk(RTS_TRACE_ERROR,
				"ddr len overflow : 0x%08x\n", status);
			rtscam_socdev_write_reg(video_in, mask, reg_isp);
			rtscam_soc_inc_abnormals(to_rsocdev_obj(video_in), i);
			return IRQ_HANDLED;
		}

		mask = 0x4 << (4 * i);
		if (status & mask) {
			rtsprintk(RTS_TRACE_WARNING, "isp overflow : 0x%08x\n",
				  status);
			rtscam_socdev_write_reg(video_in, mask, reg_isp);
			rtscam_soc_inc_overflow(to_rsocdev_obj(video_in), i);

			stream = rtscam_soc_get_stream_from_reg_index(
					to_rsocdev_obj(video_in), i);

			spin_lock(&stream->lock);
			rtscam_submit_buffer(stream, NULL);
			spin_unlock(&stream->lock);
			return IRQ_HANDLED;
		}
	}

	for (i = 0; i < RTSCAM_MAX_STREAM_NUM; i++) {
		mask = 1 << (4 * i + 3);
		if (status & mask) {
			rtscam_socdev_write_reg(video_in, mask, reg_isp);
			break;
		}
	}

	stream = rtscam_soc_get_stream_from_reg_index(
			to_rsocdev_obj(video_in), i);
	if (stream == NULL) {
		rtsprintk(RTS_TRACE_WARNING, "no stream found\n");
		return IRQ_HANDLED;
	}

	spin_lock(&stream->lock);
	mask = 0x3;
	i = mask & rtscam_socdev_read_reg(video_in,
			RTS_REG_YUV_Y_CUR_FRAME_INDEX_OFFSET_BASE +
			i * RTS_REG_YUV_CUR_FRAME_INDEX_OFFSET_INTERVAL);
	for (cnt = 0; cnt < RTSCAM_SOC_HW_SLOT_NUM; cnt++) {
		rtscam_socdev_process_frame(video_in, stream, i++);
		i %= RTSCAM_SOC_HW_SLOT_NUM;
	}
	spin_unlock(&stream->lock);

	return IRQ_HANDLED;
}

static void rtscam_socdev_enable_interrupt(
			struct rtscam_soc_video_in *video_in, int enable)
{
	u32 int_en;

	if (enable)
		int_en = 0xffcccc;
	else
		int_en = 0;

	rtscam_socdev_write_reg(video_in, int_en, RTS_REG_INT_EN_ISP_TO_HOST);
	rtscam_socdev_write_reg(video_in,
			0xffffffff, RTS_REG_INT_FLAG_ISP_HOST);

	if (enable)
		int_en = 0xf8;

	rtscam_socdev_write_reg(video_in,
			int_en, RTS_REG_INT_EN_RGB_YONLY_TO_HOST);
	rtscam_socdev_write_reg(video_in,
			0xffffffff, RTS_REG_INT_FLAG_RGB_YONLY_HOST);
}

/*
 * fix last row bug of low-latency
 * [TODO] read value from MAX_BURST_LENGTH_SELECT reg, 128 or 256
 */
#define RTSCAM_DEFAULT_BURST_LEN_Y0	128
static void __iomem *g_base;
extern u32 g_pix_cnt_thd;
int rtscam_check_pixel_cnt(void)
{
	u32 val;

	if (!g_base)
		return 0;

	val =  le32_to_cpu(ioread32(g_base +
			RTS_REG_YUV_Y_CUR_FRAME_INDEX_OFFSET_BASE));
	val = (val >> 2) & 0xfffff;

	if (val < RTSCAM_DEFAULT_BURST_LEN_Y0 || val > g_pix_cnt_thd)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL_GPL(rtscam_check_pixel_cnt);

static void rtscam_socdev_release_3915(struct rtscam_soc_video_in *video_in)
{
	kfree(video_in);
}

int rtscam_socdev_init_videoin(struct rtscam_soc_video_in **pvideo_in,
		struct device *dev, struct resource *res)
{
	struct rtscam_soc_video_in *video_in;
	void __iomem *base;
	kernel_ulong_t devtype;

	if (!pvideo_in || !dev || !res)
		return -EINVAL;

	devtype = rtscam_soc_get_devtype();
	if (devtype != TYPE_RTS3915 && devtype != TYPE_RTS3917)
		return -EINVAL;

	video_in = kzalloc(sizeof(*video_in), GFP_KERNEL);
	if (video_in == NULL) {
		rtsprintk(RTS_TRACE_ERROR,
			  "Couldn't allocate rts camera custom dev object\n");
		return -ENOMEM;
	}

	video_in->reset_video = devm_reset_control_get(dev, "video");
	if (IS_ERR(video_in->reset_video)) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reset : video\n");
		return -EINVAL;
	}

	video_in->sysmem = devm_reset_control_get(dev, "video-sysmem-up");
	if (IS_ERR(video_in->sysmem)) {
		rtsprintk(RTS_TRACE_ERROR, "fail to get reset : sysmem\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap resource\n");
		return PTR_ERR(base);
	}
	video_in->base = base;
	g_base = base;

	video_in->width_step = 4;
	video_in->height_step = 4;

	video_in->enable_interrupt = rtscam_socdev_enable_interrupt;
	video_in->write_reg = rtscam_socdev_write_reg;
	video_in->read_reg = rtscam_socdev_read_reg;
	video_in->submit_buffer = rtscam_socdev_submit_buffer;
	video_in->process_irq = rtscam_socdev_process_isp_irq;
	video_in->reset_isp_reg = rtscam_socdev_reset_isp_reg;
	video_in->isp_control = rtscam_socdev_isp_control;
	video_in->set_fps = rtscam_soc_set_fps;
	video_in->set_fps_dynamic = rtscam_socdev_set_fps_dynamic;
	video_in->release = rtscam_socdev_release_3915;

	video_in->support_rgb = 1;
	video_in->reg.yuv_axibuf_base = RTS_REG_YUV_AXI_BUF_CONFIG_BASE;
	video_in->reg.yuv_base_val_bit = 4;
	video_in->reg.yuv_size_val_bit = 20;
	video_in->reg.rgb_axibuf_base = RTS_REG_RGB_R_AXI_BUF_CONFIG;
	video_in->reg.rgb_axibuf_interval =
			RTS_REG_RGB_AXI_BUF_CONFIG_INTERVAL;
	video_in->reg.rgb_frame_len_r = RTS_REG_RGB_FRAME_LEN_R;
	video_in->reg.rgb_frame_len_g = RTS_REG_RGB_FRAME_LEN_G;
	video_in->reg.rgb_frame_len_b = RTS_REG_RGB_FRAME_LEN_B;
	video_in->reg.y_frame_len_base = RTS_REG_YUV_FRAME_LEN_Y_BASE;
	video_in->reg.uv_frame_len_base = RTS_REG_YUV_FRAME_LEN_UV_BASE;
	video_in->reg.yuv_frame_len_interval = RTS_REG_YUV_FRAME_LEN_INTERVAL;
	video_in->reg.yuv_frame_buf_cnt = RTS_REG_YUV_FRAME_BUFFER_COUNT;
	video_in->reg.rgb_frame_buf_cnt =
			RTS_REG_RGB_YONLY_FRAME_BUFFER_COUNT;
	video_in->reg.yuv_interleave_select = RTS_REG_YUV_INTERLEAVE_SELECT;
	video_in->reg.isp_nv12_select = RTS_REG_ISP_NV12_SELECT;
	video_in->reg.td_buf_cfg = 0;
	video_in->reg.multi_read = 0;
	video_in->reg.ringbuf_mode = RTS_REG_RINGBUF_MODE;
	video_in->reg.ringbuf_rowend_delay = RTS_REG_RINGBUF_ROWEND_DELAY;
	video_in->reg.buf_status_base =
			RTS_REG_YUV_FRAME_BUFFER_STATUS_BASE;

	*pvideo_in = video_in;
	return 0;
}
