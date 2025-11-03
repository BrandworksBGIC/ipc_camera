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

#define TAG	"MJPEG"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "linux/rts_camera_jpgenc.h"
#include "rts_camera.h"
#include "rts_hw_id.h"
#include "rts_isp_mem.h"
#include "rts_camera_jpg_table.h"

#define RTS_JPG_ENC_DRV_NAME		"rts_jpgenc"
#define RTS_JPG_ENC_DEV_NAME		"rtsjpgenc"

#define JPEG_CTRL				0x00000000
#define JPEG_CONFIG				0x00000004
#define JPEG_MCUNUM				0x00000008
#define JPEG_RST_INTERVAL			0x0000000c
#define JPEG_COMPONENT0				0x00000010
#define JPEG_COMPONENT1				0x00000014
#define JPEG_COMPONENT2				0x00000018
#define JPEG_COMPONENT3				0x0000001c
#define JPEG_ISP_STREAM_MODE			0x00000024
#define JPEG_ENC_FORMAT				0x00000028
#define JPEG_COMPRESS_ADDR			0x0000002c
#define JPEG_DDR_FRAME_BUFFER_NUM		0x0000003c
#define JPEG_DDR_FRAME_BUFFER_STATUS		0x00000040
#define JPEG_DDR_FRAME_CUR_FRAME_INDEX		0x00000044
#define JPEG_Y_ADDR				0x00000048
#define JPEG_UV_ADDR				0x0000004c
#define JPEG_ENC_DOUT_BUFFER_LENGTH		0x00000050
#define JPEG_DOUT_BUFFER_RECEIVED_LENGTH	0x00000054
#define JPEG_MULTIPLY_XSIZE_YSIZE		0x00000064
#define JPEG_INT_FLAG_EN			0x00000074
#define JPEG_INT_FLAG				0x00000070
#define SW_QMEM					0x00000078
#define SW_HUFFENC				0x0000007c
#define SW_DHT					0x00000080
#define JPEG_L2F_CTRL				0x00000084

#define RTS_JPG_CLOCK_ON_NEED			1

#define RTS_JPG_INFO_NUM			2

struct rtscam_jpgenc_info {
	unsigned long blo_cnt; // buffer length overflow
	unsigned long bno_cnt; // buffer num overflow
	unsigned long lbo_cnt; // line buffer overflow
	unsigned long ve_cnt; // videoin error
	unsigned long d_cnt; // done
};

struct rtscam_jpgenc_res_item {
	struct list_head list;
	struct rtscam_jpg_buf buf;
};

struct rtscam_jpg_enc {
	struct device *dev;
	void __iomem *hwregs;

	struct clk *clk;
	struct clk *dram_clk;
	struct reset_control *jpg_rst;
	struct reset_control *sysmem;

	unsigned long clk_rate;
	atomic_t clk_count;
	struct mutex mlock;

	struct rtscam_jpg_cfg cur_cfg;
	struct rtscam_jpgenc_info jpeginfo[RTS_JPG_INFO_NUM];

	uint32_t status; /* encode status */
	wait_queue_head_t enc_wq;

	spinlock_t slock;
	struct list_head done_queue;
	struct list_head idle_queue;
	struct rtscam_jpgenc_res_item *slots[RTS_JPG_MAX_NUM];
	struct rtscam_jpgenc_res_item *sw_bufs[RTS_JPG_MAX_NUM];
	uint8_t buf_idx;

	uint8_t stream_running;

	struct rtscam_ge_device *jdev;

#ifdef DBG_ENCODE_TIME
	struct ring_buffer *timebuf;
	struct timespec64 start;
	struct timespec64 end;
	unsigned long cnt;
	int open_count;
	int wid;
#endif
};

static int jpgenc_read_reg(struct rtscam_jpg_enc *rjpgenc, off_t reg)
{
	return le32_to_cpu(ioread32(rjpgenc->hwregs + reg));
}

static void jpgenc_write_reg(struct rtscam_jpg_enc *rjpgenc,
				u32 value, off_t reg)
{
	iowrite32(cpu_to_le32(value), rjpgenc->hwregs + reg);
}

static void jpgenc_write_table(struct rtscam_jpg_enc *rjpgenc,
				off_t reg, u16 addr, u32 value)
{
	u32 val = 0x3 << 28 | (addr << 16) | value;

	jpgenc_write_reg(rjpgenc, val, reg);
}

static void jpgenc_set_dht(struct rtscam_jpg_enc *rjpgenc)
{
	u32 dc_val_num = 12;
	u32 ac_val_num = 162;
	uint16_t i = 0;
	uint16_t offset = 16;
	u32 reg = SW_DHT;

	for (i = 0; i < 16; i++)
		jpgenc_write_table(rjpgenc, reg, i, luma_dc_bits[i]);
	for (i = 0; i < dc_val_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, luma_dc_value[i]);

	offset += dc_val_num;
	for (i = 0; i < 16; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, luma_ac_bits[i]);
	offset += 16;
	for (i = 0; i < ac_val_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, luma_ac_value[i]);

	offset += ac_val_num;
	for (i = 0; i < 16; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_dc_bits[i]);
	offset += 16;
	for (i = 0; i < dc_val_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_dc_value[i]);

	offset += dc_val_num;
	for (i = 0; i < 16; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_ac_bits[i]);
	offset += 16;
	for (i = 0; i < ac_val_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_ac_value[i]);
}

static void jpgenc_set_huff(struct rtscam_jpg_enc *rjpgenc)
{
	u32 dc_num = 16;
	u32 ac_num = 176;
	uint16_t i = 0;
	uint16_t offset = ac_num;
	u32 reg = SW_HUFFENC;

	for (i = 0; i < ac_num; i++)
		jpgenc_write_table(rjpgenc, reg, i, luma_ac_huff[i]);
	for (i = 0; i < ac_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_ac_huff[i]);

	offset += ac_num;
	for (i = 0; i < dc_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, luma_dc_huff[i]);
	offset += dc_num;
	for (i = 0; i < dc_num; i++)
		jpgenc_write_table(rjpgenc, reg,
				i + offset, chroma_dc_huff[i]);
}

static void jpgenc_set_qtable(struct rtscam_jpg_enc *rjpgenc, u32 quality)
{
	uint16_t i = 0;
	u32 reg = SW_QMEM;
	u8 minq = 2;
	u8 maxq = 255;
	u32 scaler;
	u32 tmp;

	if (quality < 1)
		quality = 1;
	else if (quality > 100)
		quality = 100;

	if (quality < 50)
		scaler = 5000 / quality;
	else
		scaler = 200 - quality * 2;

	for (i = 0; i < 128; i++) {
		tmp = (m_jpeg_qtable_lowq[i] * scaler + 50) / 100;

		if (tmp < minq)
			tmp = minq;
		if (tmp > maxq)
			tmp = maxq;

		jpgenc_write_table(rjpgenc, reg, i, tmp);
	}
}

#ifdef DBG_ENCODE_TIME
static int __init_timebuf(struct rtscam_jpg_enc *rjpgenc)
{
	if (rtscam_init_ring_buf(rjpgenc->timebuf) < 0)
		return -EINVAL;

	if (!rjpgenc->open_count)
		rjpgenc->cnt = 0;
	rjpgenc->open_count++;
	return 0;
}

static void __set_start_encode_time(struct rtscam_jpg_enc *rjpgenc)
{
	if (rjpgenc->start.tv_sec) {
		long delta;

		delta = (rjpgenc->end.tv_nsec - rjpgenc->start.tv_nsec) /
			1000 / 1000 +
			(rjpgenc->end.tv_sec - rjpgenc->start.tv_sec) * 1000;

		rtscam_write_ring_buf(rjpgenc->timebuf, rjpgenc->cur_cfg.width,
						delta, rjpgenc->cnt++);
	}
	ktime_get_raw_ts64(&rjpgenc->start);
}

static int __alloc_time_buf(struct rtscam_jpg_enc *rjpgenc)
{
	rjpgenc->timebuf = kmalloc(sizeof(*rjpgenc->timebuf),
						GFP_KERNEL);
	if (!rjpgenc->timebuf) {
		rtsprintk(RTS_TRACE_ERROR, "fail to alloc timebuf mem\n");
		return -ENOMEM;
	}

	return 0;
}

static void __put_time_buf(struct rtscam_jpg_enc *rjpgenc)
{
	if (rjpgenc->timebuf)
		kfree(rjpgenc->timebuf);
}

#else

static int __init_timebuf(struct rtscam_jpg_enc *rjpgenc)
{
	return 0;
}

static void __set_start_encode_time(struct rtscam_jpg_enc *rjpgenc)
{
}

static int __alloc_time_buf(struct rtscam_jpg_enc *rjpgenc)
{
	return 0;
}

static void __put_time_buf(struct rtscam_jpg_enc *rjpgenc)
{
}
#endif

static void jpgenc_update_clk_rate(struct rtscam_jpg_enc *rjpgenc)
{
	unsigned long rate;

	if (!rjpgenc || !rjpgenc->clk)
		return;

	rate = clk_get_rate(rjpgenc->clk);
	if (rate != rjpgenc->clk_rate)
		clk_set_rate(rjpgenc->clk, rjpgenc->clk_rate);

	rate = clk_get_rate(rjpgenc->clk);
	if (rate != rjpgenc->clk_rate) {
		rtsprintk(RTS_TRACE_ERROR,
			"Couldn't change jpeg clk %lu\n", rjpgenc->clk_rate);
		return;
	}

	rtsprintk(RTS_TRACE_DEBUG, "Change jpeg clk to %ld\n", rate);
}

static int jpgenc_enable_clk(struct rtscam_jpg_enc *rjpgenc, int enable)
{
	if (!rjpgenc || !rjpgenc->clk || !rjpgenc->dram_clk)
		return -EINVAL;

	mutex_lock(&rjpgenc->mlock);
	if (enable) {
		if (atomic_inc_return(&rjpgenc->clk_count) == 1) {
			clk_prepare_enable(rjpgenc->dram_clk);
			clk_prepare_enable(rjpgenc->clk);
		}
	} else {
		if (atomic_dec_return(&rjpgenc->clk_count) == 0) {
			clk_disable_unprepare(rjpgenc->clk);
			clk_disable_unprepare(rjpgenc->dram_clk);
		}
	}
	mutex_unlock(&rjpgenc->mlock);

	return 0;
}

static int jpgenc_set_config(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_info *info)
{
	u32 val;
	int i;
	struct rtscam_jpg_cfg *cfg = NULL;
	struct rtscam_jpg_buf *buf = NULL;

	if (!rjpgenc || !info)
		return -EINVAL;

	cfg = &info->cfg;
	buf = info->buf;

	if (cfg->yuv_mode > 1 || cfg->enc_mode > 2)
		return -EINVAL;

	if (cfg->buf_num > RTS_JPG_MAX_NUM || cfg->buf_num < 1)
		return -EINVAL;

	if ((cfg->enc_mode != 0) && (cfg->vin_chn > RTS_JPG_INFO_NUM))
		return -EINVAL;

	val = 0x192 | ((cfg->height & 0xFFFF) << 16) | ((cfg->re & 0x1) << 2);
	jpgenc_write_reg(rjpgenc, val, JPEG_CONFIG);

	jpgenc_write_reg(rjpgenc, cfg->nmcu & 0x3FFFFFF, JPEG_MCUNUM);

	val = (cfg->width & 0xFFFF) << 16;
	if (cfg->re)
		val |= ((cfg->width >> 4) - 1) & 0xFFFF;
	jpgenc_write_reg(rjpgenc, val, JPEG_RST_INTERVAL);

	val = ((cfg->rotation & 0xF) << 4) | (cfg->yuv_mode & 0xF);
	jpgenc_write_reg(rjpgenc, val, JPEG_ENC_FORMAT);

	if (cfg->yuv_mode == 0) {
		jpgenc_write_reg(rjpgenc, 0x2230, JPEG_COMPONENT0);
	} else if (cfg->yuv_mode == 1) {
		if (cfg->rotation < 4)
			jpgenc_write_reg(rjpgenc, 0x2110, JPEG_COMPONENT0);
		else
			jpgenc_write_reg(rjpgenc, 0x1210, JPEG_COMPONENT0);
	}
	jpgenc_write_reg(rjpgenc, 0x1107, JPEG_COMPONENT1);
	jpgenc_write_reg(rjpgenc, 0x1107, JPEG_COMPONENT2);

	val = cfg->width * cfg->height;
	jpgenc_write_reg(rjpgenc, val, JPEG_MULTIPLY_XSIZE_YSIZE);

	if (cfg->quality != rjpgenc->cur_cfg.quality)
		jpgenc_set_qtable(rjpgenc, cfg->quality);

	jpgenc_write_reg(rjpgenc, cfg->buf_num, JPEG_DDR_FRAME_BUFFER_NUM);
	jpgenc_write_reg(rjpgenc, buf[0].ddr_size,
				JPEG_ENC_DOUT_BUFFER_LENGTH);
	for (i = 0; i < cfg->buf_num; i++) {
		if (!buf[i].ddr_phy)
			return -EINVAL;

		jpgenc_write_reg(rjpgenc, buf[i].ddr_phy,
				JPEG_COMPRESS_ADDR + 4 * i);
		jpgenc_write_reg(rjpgenc, 0x1 << i,
				JPEG_DDR_FRAME_BUFFER_STATUS);
	}

	if (cfg->enc_mode == 0) { /* normal */
		jpgenc_write_reg(rjpgenc, 0x0, JPEG_ISP_STREAM_MODE);
		jpgenc_write_reg(rjpgenc, cfg->y, JPEG_Y_ADDR);
		jpgenc_write_reg(rjpgenc, cfg->uv, JPEG_UV_ADDR);
	} else if (cfg->enc_mode == 1) { /* trig */
		val = 0x1 | ((~(cfg->vin_chn) & 0x1) << 1);
		jpgenc_write_reg(rjpgenc, val, JPEG_ISP_STREAM_MODE);
	} else { /* stream */
		val = 0x5 | ((~(cfg->vin_chn) & 0x1) << 1);
		jpgenc_write_reg(rjpgenc, val, JPEG_ISP_STREAM_MODE);
	}

	jpgenc_write_reg(rjpgenc, cfg->l2f, JPEG_L2F_CTRL);

	/* save cur cfg */
	memcpy(&rjpgenc->cur_cfg, cfg, sizeof(*cfg));

	return 0;
}

static void jpgenc_start(struct rtscam_jpg_enc *rjpgenc)
{
	u32 val;

	if (!rjpgenc)
		return;

	if (rjpgenc->cur_cfg.enc_mode != RTSCAM_JPG_MODE_NORMAL) {
		val = jpgenc_read_reg(rjpgenc, JPEG_ISP_STREAM_MODE);
		jpgenc_write_reg(rjpgenc, val | 0x10, JPEG_ISP_STREAM_MODE);
	}
	jpgenc_write_reg(rjpgenc, 0x1, JPEG_CTRL);
#ifdef DBG_ENCODE_TIME
	__set_start_encode_time(rjpgenc);
#endif
}

static void jpgenc_stop(struct rtscam_jpg_enc *rjpgenc)
{
	u32 buf_num;
	int i;
	u32 mask = 0;
	u32 status;

	if (!rjpgenc)
		return;

	if (rjpgenc->cur_cfg.enc_mode == RTSCAM_JPG_MODE_STREAM) {
		buf_num = jpgenc_read_reg(rjpgenc, JPEG_DDR_FRAME_BUFFER_NUM);
		for (i = 0; i < buf_num; i++)
			mask = (mask << 1) | 0x1;
		for (i = 0; i < 100 * buf_num; i++) {
			status = jpgenc_read_reg(rjpgenc,
					JPEG_DDR_FRAME_BUFFER_STATUS);
			if (status == mask)
				break;
			msleep(10);
		}
	} else {
		mask = 0x4;
		for (i = 0; i < 100; i++) {
			status = jpgenc_read_reg(rjpgenc, JPEG_CTRL);
			if (status & mask)
				break;
			msleep(10);
		}
	}

	jpgenc_write_reg(rjpgenc, 0x2, JPEG_CTRL);
}

static int jpgenc_encode_frame(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_info *info)
{
	int ret = 0;

	if (!rjpgenc || !info)
		return -EINVAL;

	rjpgenc->status = 0;

	ret = jpgenc_set_config(rjpgenc, info);
	if (ret)
		return ret;

	jpgenc_start(rjpgenc);

	ret = wait_event_timeout(rjpgenc->enc_wq,
			rjpgenc->status != 0, msecs_to_jiffies(info->timeout));
	if (ret < 0)
		return ret;
	else if (ret == 0)
		return -ETIME;

	info->buf[0].ddr_used = jpgenc_read_reg(rjpgenc,
			JPEG_DOUT_BUFFER_RECEIVED_LENGTH);
	info->buf[0].status = rjpgenc->status;
	info->buf[0].time_stamp = ktime_get_raw_ns();

	if (rjpgenc->status & RTSCAM_JPG_STATUS_LINE_BUF_OVERFLOW)
		jpgenc_stop(rjpgenc);

	return 0;
}

static int jpgenc_init_slots(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_info *info)
{
	struct rtscam_jpgenc_res_item *item = NULL;
	int buf_num, i;
	int ret = 0;

	if (!rjpgenc || !info)
		return -EINVAL;

	buf_num = info->cfg.buf_num;

	for (i = 0; i < buf_num; i++) {
		item = kzalloc(sizeof(*item), GFP_KERNEL);
		if (!item) {
			ret = -ENOMEM;
			goto exit;
		}

		if (info->buf[i].ddr_idx != i) {
			ret = -EINVAL;
			goto exit;
		}

		memcpy(&item->buf, &info->buf[i], sizeof(item->buf));
		rjpgenc->slots[i] = item;
		rjpgenc->sw_bufs[i] = item;
	}

	return 0;
exit:
	for (i = 0; i < buf_num; i++) {
		if (rjpgenc->slots[i])
			kfree(rjpgenc->slots[i]);
		rjpgenc->slots[i] = NULL;
		rjpgenc->sw_bufs[i] = NULL;
	}

	return ret;
}

static int jpgenc_clr_items(struct rtscam_jpg_enc *rjpgenc)
{
	struct rtscam_jpgenc_res_item *item = NULL;
	struct rtscam_jpgenc_res_item *tmp = NULL;
	int i;

	list_for_each_entry_safe(item, tmp, &rjpgenc->idle_queue, list) {
		list_del_init(&item->list);
		kfree(item);
	}

	list_for_each_entry_safe(item, tmp, &rjpgenc->done_queue, list) {
		list_del_init(&item->list);
		kfree(item);
	}

	INIT_LIST_HEAD(&rjpgenc->idle_queue);
	INIT_LIST_HEAD(&rjpgenc->done_queue);

	for (i = 0; i < rjpgenc->cur_cfg.buf_num; i++) {
		item = rjpgenc->slots[i];
		if (!item)
			continue;

		kfree(item);
		rjpgenc->slots[i] = NULL;
	}

	return 0;
}

static void jpgenc_enc_stop(struct rtscam_jpg_enc *rjpgenc)
{
	if (!rjpgenc)
		return;

	jpgenc_stop(rjpgenc);

	if (rjpgenc->cur_cfg.enc_mode == RTSCAM_JPG_MODE_STREAM) {
		spin_lock_irq(&rjpgenc->slock);
		jpgenc_clr_items(rjpgenc);
		spin_unlock_irq(&rjpgenc->slock);
		rjpgenc->stream_running = 0;
	}
}

static int jpgenc_start_stream(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_info *info)
{
	int ret = 0;
	int vin;

	if (!rjpgenc || !info)
		return -EINVAL;

	spin_lock_irq(&rjpgenc->slock);
	jpgenc_clr_items(rjpgenc);
	spin_unlock_irq(&rjpgenc->slock);

	ret = jpgenc_set_config(rjpgenc, info);
	if (ret)
		return ret;

	// init slots
	ret = jpgenc_init_slots(rjpgenc, info);
	if (ret)
		return ret;

	rjpgenc->buf_idx = 0;
	rjpgenc->stream_running = 1;
	vin = rjpgenc->cur_cfg.vin_chn;
	rjpgenc->jpeginfo[vin].blo_cnt = 0;
	rjpgenc->jpeginfo[vin].bno_cnt = 0;
	rjpgenc->jpeginfo[vin].lbo_cnt = 0;
	rjpgenc->jpeginfo[vin].ve_cnt = 0;
	rjpgenc->jpeginfo[vin].d_cnt = 0;

	jpgenc_start(rjpgenc);

	return 0;
}

static int jpgenc_submit_buffer(struct rtscam_jpg_enc *rjpgenc)
{
	struct rtscam_jpgenc_res_item *item = NULL;

	if (!rjpgenc)
		return -EINVAL;

	while (!list_empty(&rjpgenc->idle_queue)) {
		if (rjpgenc->slots[rjpgenc->buf_idx]) {
			rtsprintk(RTS_TRACE_BUF,
					"there is no free slot now\n");
			return 0;
		}

		item = list_first_entry(&rjpgenc->idle_queue,
				struct rtscam_jpgenc_res_item, list);
		list_del_init(&item->list);
		rjpgenc->slots[rjpgenc->buf_idx] = item;
		jpgenc_write_reg(rjpgenc, item->buf.ddr_phy,
				JPEG_COMPRESS_ADDR + rjpgenc->buf_idx * 4);
		jpgenc_write_reg(rjpgenc, 0x1 << rjpgenc->buf_idx,
					JPEG_DDR_FRAME_BUFFER_STATUS);

		rjpgenc->buf_idx = (rjpgenc->buf_idx + 1) %
				rjpgenc->cur_cfg.buf_num;
	}

	return 0;
}

static int jpgenc_stream_qbuf(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_buf *buf)
{
	struct rtscam_jpgenc_res_item *item = NULL;

	if (!rjpgenc || !buf)
		return -EINVAL;

	/* check idx */
	if (buf->ddr_idx >= RTS_JPG_MAX_NUM)
		return -EINVAL;

	if (rjpgenc->sw_bufs[buf->ddr_idx])
		return -EINVAL;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	memcpy(&item->buf, buf, sizeof(*buf));

	rjpgenc->sw_bufs[buf->ddr_idx] = item;
	list_add_tail(&item->list, &rjpgenc->idle_queue);

	return jpgenc_submit_buffer(rjpgenc);
}

static int jpgenc_stream_dqbuf(struct rtscam_jpg_enc *rjpgenc,
			struct rtscam_jpg_buf *buf)
{
	struct rtscam_jpgenc_res_item *item = NULL;

	if (!rjpgenc || !buf)
		return -EINVAL;

	if (list_empty(&rjpgenc->done_queue))
		return -ENODATA;

	item = list_first_entry(&rjpgenc->done_queue,
			struct rtscam_jpgenc_res_item, list);
	list_del_init(&item->list);

	memcpy(buf, &item->buf, sizeof(*buf));

	rjpgenc->sw_bufs[buf->ddr_idx] = NULL;
	kfree(item);

	return 0;
}

static long jpgenc_do_ioctl(struct file *filp, unsigned int cmd, void *arg)
{
	struct rtscam_jpg_enc *rjpgenc = filp->private_data;
	int err = 0;

	if (_IOC_TYPE(cmd) != RTSJPGENC_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RTSJPGENC_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RTSJPGENC_IOC_SETIME:
		__set_start_encode_time(rjpgenc);
		break;
	case RTSJPGENC_IOC_GETIMEBUF:
#ifdef DBG_ENCODE_TIME
		err = rtscam_read_ring_buf(rjpgenc->timebuf,
						(void __user *)arg);
#else
		err = -ENOTTY;
#endif
		break;
	case RTSJPGENC_IOC_ENC_FRAME:
		if (rjpgenc->stream_running)
			return -EBUSY;
		err = jpgenc_encode_frame(rjpgenc, arg);
		break;
	case RTSJPGENC_IOC_ENC_STOP:
		jpgenc_enc_stop(rjpgenc);
		break;
	case RTSJPGENC_IOC_ENC_STREAM:
		err = jpgenc_start_stream(rjpgenc, arg);
		break;
	case RTSJPGENC_IOC_QBUF:
		if (!rjpgenc->stream_running)
			return -EPERM;
		spin_lock_irq(&rjpgenc->slock);
		err = jpgenc_stream_qbuf(rjpgenc, arg);
		spin_unlock_irq(&rjpgenc->slock);
		break;
	case RTSJPGENC_IOC_DQBUF:
		if (!rjpgenc->stream_running)
			return -EPERM;
		spin_lock_irq(&rjpgenc->slock);
		err = jpgenc_stream_dqbuf(rjpgenc, arg);
		spin_unlock_irq(&rjpgenc->slock);
		break;
	default:
		rtsprintk(RTS_TRACE_ERROR,
			"unknown[rtsjpgenc] ioctl 0x%08x, '%c' 0x%x\n",
			cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		err = -ENOTTY;
		break;
	}

	return err;
}

static long jpgenc_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return rtscam_usercopy(filp, cmd, arg, jpgenc_do_ioctl);
}

static unsigned int jpgenc_poll(struct file *filp,
			struct poll_table_struct *wait)
{
	struct rtscam_jpg_enc *rjpgenc = filp->private_data;
	unsigned int mask = 0;
	unsigned long req_events = poll_requested_events(wait);

	if (!(req_events & (POLLIN | POLLRDNORM)))
		return mask;

	if (list_empty(&rjpgenc->done_queue))
		poll_wait(filp, &rjpgenc->enc_wq, wait);

	if (!list_empty(&rjpgenc->done_queue))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int jpgenc_open(struct file *filp)
{
	struct rtscam_ge_device *gdev = rtscam_devdata(filp);
	struct rtscam_jpg_enc *rjpgenc = rtscam_ge_get_drvdata(gdev);

	if (__init_timebuf(rjpgenc) < 0)
		return -EINVAL;

#if RTS_JPG_CLOCK_ON_NEED
	jpgenc_enable_clk(rjpgenc, 1);
#endif

	filp->private_data = rjpgenc;

	return 0;
}

static int jpgenc_close(struct file *filp)
{
	struct rtscam_jpg_enc *rjpgenc = filp->private_data;

	filp->private_data = NULL;

	if (!rjpgenc)
		return -EINVAL;

#if RTS_JPG_CLOCK_ON_NEED
	jpgenc_enable_clk(rjpgenc, 0);
#endif

#ifdef DBG_ENCODE_TIME
	rjpgenc->open_count--;
#endif

	return 0;
}

static struct rtscam_ge_file_operations rtscam_jpgenc_fops = {
	.owner = THIS_MODULE,
	.open = jpgenc_open,
	.release = jpgenc_close,
	.ioctl = jpgenc_ioctl,
	.poll = jpgenc_poll,
};

static int jpgenc_process_frame(struct rtscam_jpg_enc *rjpgenc, int state)
{
	u32 val;
	u32 status;
	u32 mask;
	struct rtscam_jpgenc_res_item *item = NULL;
	struct list_head *queue = &rjpgenc->done_queue;

	if (rjpgenc->cur_cfg.enc_mode != RTSCAM_JPG_MODE_STREAM)
		return 0;

	val = jpgenc_read_reg(rjpgenc, JPEG_DDR_FRAME_CUR_FRAME_INDEX);
	val = ((val & 0x3) + rjpgenc->cur_cfg.buf_num - 1) %
				rjpgenc->cur_cfg.buf_num;

	status = jpgenc_read_reg(rjpgenc, JPEG_DDR_FRAME_BUFFER_STATUS);
	if (!status)
		return 0;

	mask = 0x1 << val;
	if (mask & status) {
		item = rjpgenc->slots[val];
		if (!item) {
			rtsprintk(RTS_TRACE_ERROR, "slots[%d] is null\n", val);
			return -EINVAL;
		}
		item->buf.ddr_used = jpgenc_read_reg(rjpgenc,
			JPEG_DOUT_BUFFER_RECEIVED_LENGTH + val * 4);
		item->buf.time_stamp = ktime_get_raw_ns();
		item->buf.status = state;
		list_add_tail(&item->list, queue);
		rjpgenc->slots[val] = NULL;
		jpgenc_submit_buffer(rjpgenc);
	}

	return 0;
}

static irqreturn_t jpgenc_irq(int irq, void *data)
{
	struct rtscam_jpg_enc *rjpgenc = (struct rtscam_jpg_enc *)data;
	u32 flag;
	u32 status = 0;
	u32 buf_status = 0;
	u32 mask;
	int vin = rjpgenc->cur_cfg.vin_chn;
	int mode = rjpgenc->cur_cfg.enc_mode;

	flag = jpgenc_read_reg(rjpgenc, JPEG_INT_FLAG);
	jpgenc_write_reg(rjpgenc, flag, JPEG_INT_FLAG);
	if ((flag & 0x2B) == 0)
		return IRQ_NONE;

	/* buffer number overflow */
	if (flag & 0x20) {
		if (vin < RTS_JPG_INFO_NUM && mode == RTSCAM_JPG_MODE_STREAM)
			rjpgenc->jpeginfo[vin].bno_cnt++;
		rtsprintk(RTS_TRACE_DEBUG,
			"jpgenc:frame buffer num overflow(status:0x%x)\n",
			flag);
		status |= RTSCAM_JPG_STATUS_BUF_NUM_OVERFLOW;
	}

	/* buffer length overflow, 1 */
	if (flag & 0x2) {
		if (vin < RTS_JPG_INFO_NUM && mode == RTSCAM_JPG_MODE_STREAM)
			rjpgenc->jpeginfo[vin].blo_cnt++;
		rtsprintk(RTS_TRACE_ERROR,
			"jpgenc:frame buffer len overflow(0x%x)\n",
			flag);
		status |= RTSCAM_JPG_STATUS_BUF_LEN_OVERFLOW;
	}

	/* line buffer overflow */
	if (flag & 0x8) {
		if (vin < RTS_JPG_INFO_NUM && mode == RTSCAM_JPG_MODE_STREAM)
			rjpgenc->jpeginfo[vin].lbo_cnt++;
		rtsprintk(RTS_TRACE_DEBUG,
			"jpgenc:line buffer overflow(0x%x)\n",
			flag);
		status |= RTSCAM_JPG_STATUS_LINE_BUF_OVERFLOW;
	}

	/* vin abandond, 1 */
	if (mode != RTSCAM_JPG_MODE_NORMAL) {
		rtscam_get_video_in_buf_status(vin, &buf_status);
		if (buf_status & 0x10) {
			if (vin < RTS_JPG_INFO_NUM &&
				mode == RTSCAM_JPG_MODE_STREAM)
				rjpgenc->jpeginfo[vin].ve_cnt++;
			rtsprintk(RTS_TRACE_DEBUG,
				"jpgenc:vin fail, abandon this frame(0x%x)\n",
				buf_status);
			rtscam_set_video_in_buf_status(vin, buf_status);
			status |= RTSCAM_JPG_STATUS_VIN_ERROR;
		}
	}

	/* encode finish */
	if (flag & 0x1) {
#ifdef DBG_ENCODE_TIME
		ktime_get_raw_ts64(&rjpgenc->end);
#endif
		if (vin < RTS_JPG_INFO_NUM && mode == RTSCAM_JPG_MODE_STREAM)
			rjpgenc->jpeginfo[vin].d_cnt++;
		rtsprintk(RTS_TRACE_DEBUG,
				"jpgenc:mjpeg encode finish\n");
		status |= RTSCAM_JPG_STATUS_ENC_DONE;
	}

	if (mode == RTSCAM_JPG_MODE_STREAM) {
		spin_lock(&rjpgenc->slock);
		mask = RTSCAM_JPG_STATUS_BUF_LEN_OVERFLOW |
			RTSCAM_JPG_STATUS_LINE_BUF_OVERFLOW |
			RTSCAM_JPG_STATUS_VIN_ERROR;
		if (((status & mask) ||
			(status == RTSCAM_JPG_STATUS_ENC_DONE)) &&
			!(status & RTSCAM_JPG_STATUS_BUF_NUM_OVERFLOW))
			jpgenc_process_frame(rjpgenc, status);
		spin_unlock(&rjpgenc->slock);
	}

	rjpgenc->status = status;
	wake_up(&rjpgenc->enc_wq);

	return IRQ_HANDLED;
}

static int jpgenc_create_device(struct rtscam_jpg_enc *rjpgenc)
{
	struct rtscam_ge_device *gdev;
	int ret;

	gdev = rtscam_ge_device_alloc();
	if (!gdev)
		return -ENOMEM;

	strlcpy(gdev->name, RTS_JPG_ENC_DEV_NAME, sizeof(gdev->name));
	gdev->parent = get_device(rjpgenc->dev);
	gdev->release = rtscam_ge_device_release;
	gdev->fops = &rtscam_jpgenc_fops;

	rtscam_ge_set_drvdata(gdev, rjpgenc);
	ret = rtscam_ge_register_device(gdev);
	if (ret) {
		rtscam_ge_device_release(gdev);
		return ret;
	}

	rjpgenc->jdev = gdev;

	return 0;
}

static void jpgenc_remove_device(struct rtscam_jpg_enc *rjpgenc)
{
	struct rtscam_ge_device *gdev;

	if (!rjpgenc->jdev)
		return;

	gdev = rjpgenc->jdev;
	put_device(gdev->parent);
	rtscam_ge_unregister_device(gdev);
	rjpgenc->jdev = NULL;
}

static ssize_t jpgenc_show_jpeginfo(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct rtscam_jpg_enc *rjpgenc = dev_get_drvdata(dev);
	int num = 0;
	int i;

	num += scnprintf(buf + num, PAGE_SIZE - num,
			"jpeginfo valid when bind isp, don't show info when unbind!\n");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"isp chn(bind)\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"outbuf len over\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"outbuf num over\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"line buf over\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"videoin err\t");
	num += scnprintf(buf + num, PAGE_SIZE - num,
			"done count\n");
	for (i = 0; i < RTS_JPG_INFO_NUM; i++) {
		num += scnprintf(buf + num, PAGE_SIZE - num,
			"%-10d\t%-10ld\t%-10ld\t%-10ld\t%-10ld\t%-10ld\n",
			i, rjpgenc->jpeginfo[i].blo_cnt,
			rjpgenc->jpeginfo[i].bno_cnt,
			rjpgenc->jpeginfo[i].lbo_cnt,
			rjpgenc->jpeginfo[i].ve_cnt,
			rjpgenc->jpeginfo[i].d_cnt);
	}

	return num;
}

static ssize_t jpgenc_clr_jpeginfo(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int i;
	struct rtscam_jpg_enc *rjpgenc = dev_get_drvdata(dev);

	for (i = 0; i < RTS_JPG_INFO_NUM; i++) {
		rjpgenc->jpeginfo[i].blo_cnt = 0;
		rjpgenc->jpeginfo[i].bno_cnt = 0;
		rjpgenc->jpeginfo[i].lbo_cnt = 0;
		rjpgenc->jpeginfo[i].ve_cnt = 0;
		rjpgenc->jpeginfo[i].d_cnt = 0;
	}

	return count;
}
static DEVICE_ATTR(jpeginfo, 0664,
		jpgenc_show_jpeginfo, jpgenc_clr_jpeginfo);

static ssize_t jpgenc_get_clk_rate(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct rtscam_jpg_enc *rjpgenc = dev_get_drvdata(dev);
	int num = 0;

	num += scnprintf(buf, PAGE_SIZE, "%lu\n", clk_get_rate(rjpgenc->clk));

	return num;
}

static ssize_t jpgenc_set_clk_rate(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long rate;
	struct rtscam_jpg_enc *rjpgenc = dev_get_drvdata(dev);

	sscanf(buf, "%lu", &rate);

	if (rate)
		rjpgenc->clk_rate = rate;

	return count;
}

static DEVICE_ATTR(clock, 0664,
		jpgenc_get_clk_rate, jpgenc_set_clk_rate);

static int rtscam_jpgenc_probe(struct platform_device *pdev)
{
	struct rtscam_jpg_enc *rjpgenc;
	struct resource *res;
	int ret = 0;
	int irq;
	u32 val;

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	/* get mem & irq resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		rtsprintk(RTS_TRACE_ERROR, "Missing platform resource data\n");
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Error IRQ Number\n");
		return irq;
	}

	/* alloc main structure rtscam_jpg_enc */
	rjpgenc = devm_kzalloc(&pdev->dev, sizeof(*rjpgenc), GFP_KERNEL);
	if (!rjpgenc) {
		rtsprintk(RTS_TRACE_ERROR,
			"Couldn't allocate rts camera jpgenc object\n");
		return -ENOMEM;
	}

	/* init structure parameters */
	rjpgenc->dev = get_device(&pdev->dev);
	rjpgenc->hwregs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rjpgenc->hwregs)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't ioremap resource\n");
		ret = PTR_ERR(rjpgenc->hwregs);
		goto error;
	}

	rjpgenc->clk = devm_clk_get(&pdev->dev, "jpeg_ck");
	if (IS_ERR(rjpgenc->clk)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get mjpeg clk\n");
		ret = PTR_ERR(rjpgenc->clk);
		goto error;
	}

	rjpgenc->dram_clk = devm_clk_get(&pdev->dev, "jpegdram_ck");
	if (IS_ERR(rjpgenc->dram_clk)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get dram clk\n");
		ret = PTR_ERR(rjpgenc->dram_clk);
		goto error;
	}

	rjpgenc->jpg_rst = devm_reset_control_get(&pdev->dev, "jpeg-rst");
	if (IS_ERR(rjpgenc->jpg_rst)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get jpeg reset\n");
		ret = PTR_ERR(rjpgenc->jpg_rst);
		goto error;
	}

	rjpgenc->sysmem = devm_reset_control_get(&pdev->dev, "jpeg-sysmem-up");
	if (IS_ERR(rjpgenc->sysmem)) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't get jpeg sysmem\n");
		ret = PTR_ERR(rjpgenc->sysmem);
		goto error;
	}

	atomic_set(&rjpgenc->clk_count, 0);
	init_waitqueue_head(&rjpgenc->enc_wq);
	INIT_LIST_HEAD(&rjpgenc->done_queue);
	INIT_LIST_HEAD(&rjpgenc->idle_queue);
	spin_lock_init(&rjpgenc->slock);
	mutex_init(&rjpgenc->mlock);

	ret = __alloc_time_buf(rjpgenc);
	if (ret)
		goto error;

	/* init irq handle */
	ret = devm_request_irq(&pdev->dev, irq, jpgenc_irq,
			0, RTS_JPG_ENC_DRV_NAME, rjpgenc);
	if (ret) {
		rtsprintk(RTS_TRACE_ERROR, "Fail to request irq\n");
		goto error;
	}

	/* create /dev/rtsjpgenc */
	ret = jpgenc_create_device(rjpgenc);
	if (ret < 0) {
		rtsprintk(RTS_TRACE_ERROR, "Couldn't create device\n");
		goto error;
	}

	/* reset */
	reset_control_reset(rjpgenc->jpg_rst);

	/* sysmem up */
	reset_control_deassert(rjpgenc->sysmem);

	/* clock */
	rjpgenc->clk_rate = clk_get_rate(rjpgenc->clk);
	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &val);
	if (!ret && val) {
		rjpgenc->clk_rate = val;
	}
	jpgenc_update_clk_rate(rjpgenc);

	// enable clk before write/read reg
	jpgenc_enable_clk(rjpgenc, 1);
	/* set dht & huff */
	jpgenc_set_dht(rjpgenc);
	jpgenc_set_huff(rjpgenc);

	/* enable interrupt */
	jpgenc_write_reg(rjpgenc, 0x23, JPEG_INT_FLAG_EN);
	jpgenc_write_reg(rjpgenc, 0xFFFFFFFF, JPEG_INT_FLAG);
#if RTS_JPG_CLOCK_ON_NEED
	jpgenc_enable_clk(rjpgenc, 0);
#endif

	platform_set_drvdata(pdev, rjpgenc);
	device_create_file(rjpgenc->dev, &dev_attr_clock);
	device_create_file(rjpgenc->dev, &dev_attr_jpeginfo);

	return 0;
error:
	if (rjpgenc) {
		__put_time_buf(rjpgenc);
		jpgenc_remove_device(rjpgenc);

		if (rjpgenc->dev) {
			put_device(rjpgenc->dev);
			rjpgenc->dev = NULL;
		}
	}

	return ret;
}

static int rtscam_jpgenc_remove(struct platform_device *pdev)
{
	struct rtscam_jpg_enc *rjpgenc = platform_get_drvdata(pdev);

	rtsprintk(RTS_TRACE_INFO, "%s\n", __func__);

	if (rjpgenc) {
		device_remove_file(rjpgenc->dev, &dev_attr_clock);
		device_remove_file(rjpgenc->dev, &dev_attr_jpeginfo);

		__put_time_buf(rjpgenc);

		/* clock */
#if !RTS_JPG_CLOCK_ON_NEED
		jpgenc_enable_clk(rjpgenc, 0);
#endif

		/* sysmem up */
		reset_control_assert(rjpgenc->sysmem);

		jpgenc_remove_device(rjpgenc);

		put_device(rjpgenc->dev);
		rjpgenc->dev = NULL;
	}

	return 0;
}

static const struct of_device_id rtscam_jpgenc_ids[] = {
	{ .compatible = "realtek,rts3917-jpgenc", },
	{ /* sentinel */ },
};

static struct platform_driver rtscam_jpgenc_driver = {
	.driver = {
		.name = RTS_JPG_ENC_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rtscam_jpgenc_ids),
	},
	.probe = rtscam_jpgenc_probe,
	.remove = rtscam_jpgenc_remove,
};

module_platform_driver(rtscam_jpgenc_driver);

MODULE_DESCRIPTION("Realsil RTS3917 Mjpeg encoder device driver");
MODULE_AUTHOR("Mona Mao <mona_mao@realsil.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
MODULE_ALIAS("platform:" RTS_JPG_ENC_DRV_NAME);
