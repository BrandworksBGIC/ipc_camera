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

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include "rts_hw_id.h"
#include "rts_dma.h"
#include <linux/poll.h>
#include <linux/wait.h>
#include "rts_audio_driver.h"

#ifdef CONFIG_SND_SOC_RTS_DEBUG
#define DBG(args...)	pr_emerg("%s: %s", __func__, args)
#else
#define DBG(args...)
#endif
/*************************/

#define DEVICE_NAME "rts_audio_devdma"
struct rts_dma_data *rts_dma_data;
DECLARE_WAIT_QUEUE_HEAD(wq);
static int condition;

static int rts_audio_open(struct inode *inode, struct file *file)
{
	DBG("rts_audio_devdma open succeed\n");
	return 0;
}

static int rts_audio_release(struct inode *inode, struct file *file)
{
	DBG("rts_audio_devdma release succeed\n");
	return 0;
}

static long rts_audio_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	u32 reg_val;
	int retval;
	int in_mode_val, mono_mode_val;

	DBG("enter ioctl\n");
	switch (cmd) {
	case SWITCH_DDR_THERSHOLD:
		if (copy_from_user(&retval, (void __user *)arg, sizeof(int)))
			return -EFAULT;

		if (retval == 1) {
			reg_val = readl(rts_dma_data->addr +
					RTS_REG_AUDIO_INT_EN);
			reg_val = reg_val | (1 <<
					RTS_BIT_AUDIO_OUT_DDR_THRESHOLD);
			writel(reg_val, rts_dma_data->addr +
					RTS_REG_AUDIO_INT_EN);
			break;
		} else if (retval == 0) {
			reg_val = readl(rts_dma_data->addr +
					RTS_REG_AUDIO_INT_EN);
			reg_val = reg_val & ~(1 <<
					RTS_BIT_AUDIO_OUT_DDR_THRESHOLD);
			writel(reg_val, rts_dma_data->addr +
					RTS_REG_AUDIO_INT_EN);
			break;
		} else {
			pr_err("switch ddr thr failed, retval = %d\n", retval);
			break;
		}
	case TX_BUFFER_DDR_THERSHOLD:
		if (copy_from_user(&retval, (int *)arg, sizeof(int)))
			return -EFAULT;

		writel(retval, rts_dma_data->addr +
				RTS_REG_TX_BUFFER_THRESHOLD);
		break;
	case SET_MONO_IN:
		if (copy_from_user(&retval, (void __user *)arg, sizeof(int)))
			return -EFAULT;

		if (retval == 0) {
			in_mode_val = 0x2;
			mono_mode_val = RTS_SND_DMA_MONO_LEFT_IN;
		} else if (retval == 1) {
			in_mode_val = 0x3;
			mono_mode_val = RTS_SND_DMA_MONO_RIGHT_IN;
		} else {
			pr_err("set mono in mode failed, retval = %d\n", retval);
			break;
		}
		reg_val = readl(rts_dma_data->addr +
				RTS_REG_AUDIO_CTL);
		reg_val = reg_val & ~((u32)0x7 <<
				RTS_BIT_AUDIO_IN_MODE);
		reg_val = reg_val | ((u32)in_mode_val <<
				RTS_BIT_AUDIO_IN_MODE);
		writel(reg_val, rts_dma_data->addr +
				RTS_REG_AUDIO_CTL);
		rts_dma_data->subdata[1].mono_mode = mono_mode_val;
		break;
	default:
		break;
	}
	return 0;
}
static unsigned int rts_audio_poll(struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	DBG("enter poll\n");
	poll_wait(file, &wq, wait);
	if (condition)
		mask |= POLLIN | POLLRDNORM;
	condition = 0;
	return mask;
}
static const struct file_operations rts_audio_fops = {
	.owner = THIS_MODULE,
	.open = rts_audio_open,
	.release = rts_audio_release,
	.unlocked_ioctl = rts_audio_ioctl,
	.poll = rts_audio_poll,
};
/**************/
static struct snd_pcm_hardware rts_dma_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
	    SNDRV_PCM_INFO_BLOCK_TRANSFER |
	    SNDRV_PCM_INFO_DRAIN_TRIGGER |
	    SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_U8 |
	    SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_3LE |
		/*for SPDIF*/
	SNDRV_PCM_FMTBIT_S20_3LE,
	.rates = SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_11025 |
		SNDRV_PCM_RATE_16000 |
		SNDRV_PCM_RATE_22050 |
		SNDRV_PCM_RATE_32000 |
		SNDRV_PCM_RATE_44100 |
		SNDRV_PCM_RATE_48000 |
		SNDRV_PCM_RATE_88200 |
		SNDRV_PCM_RATE_96000 |
		SNDRV_PCM_RATE_176400 |
		SNDRV_PCM_RATE_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 4,
	.period_bytes_min = 32,
	.period_bytes_max = 64 * 1024,
	.periods_min = 2,
	.periods_max = 255,
	.buffer_bytes_max = 128 * 1024,
	.fifo_size = 16,
};

static int rts_pcm_hw_params(struct snd_soc_component *component,
				struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	u32 reg_val, val;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	int stream = substream->stream;
	int format;

	DBG("rts pcm hw params\n");

	dma_data->channels = params_channels(params);
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data->subdata[stream].substream = substream;

		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_CTL);
		/* data format */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_U8:
			val = 0x0;
			format = 1;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			val = 0x1;
			format = 2;
			break;
		case SNDRV_PCM_FORMAT_S24_3LE:
			val = 0x2;
			format = 3;
			break;
		/*for SPDIF*/
		case SNDRV_PCM_FORMAT_S20_3LE:
			val = 0x3;
			format = 3;
			break;
		default:
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0x3 << RTS_BIT_OUT_DATA_WIDTH);
		reg_val = reg_val | (val << RTS_BIT_OUT_DATA_WIDTH);
		/* channels */
		switch (dma_data->channels) {
		case 1:
			switch (dma_data->subdata[stream].mono_mode) {
			case RTS_SND_DMA_MONO_OUT:
				val = 0x2;
				break;
			case RTS_SND_DMA_MONO_TO_STEREO_OUT:
				val = 0x3;
				break;
			default:
				return -EINVAL;
			}
			break;
		case 2:
			switch (dma_data->subdata[stream].stereo_mode) {
			case RTS_SND_DMA_NORMAL_STEREO_OUT:
				val = 0x0;
				break;
			case RTS_SND_DMA_EXCHANGE_STEREO_OUT:
				val = 0x1;
				break;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0x3 << RTS_BIT_AUDIO_OUT_MODE);
		reg_val = reg_val | (val << RTS_BIT_AUDIO_OUT_MODE);
		/* source select */
		switch (dma_data->subdata[stream].codec_sel) {
		case 0:
			val = 0x0;
			break;
		case 1:
			val = 0x1;
			break;
		/*for SPDIF*/
		case 2:
			val = 0x2;
			break;
		default:
			return -EINVAL;
		}

		reg_val = reg_val & ~((u32) 0x3 << RTS_BIT_OUT_SOURCE_SELECT);
		reg_val = reg_val | (val << RTS_BIT_OUT_SOURCE_SELECT);

		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_CTL);
		dma_data->subdata[stream].format = format;

		/* start address */
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_TX_SA;
		writel(reg_val, dma_data->addr + RTS_REG_TX_SA);
		/* buffer len */
		reg_val = (totbytes / format) << RTS_COMMON_FORMAT_SHIFT;
		writel(reg_val, dma_data->addr + RTS_REG_TX_LEN);
		/* tx buffer threshold */
		reg_val = reg_val >> 1;
		writel(reg_val, dma_data->addr + RTS_REG_TX_BUFFER_THRESHOLD);
		/* out timer */
		writel(0, dma_data->addr + RTS_REG_TX_TIMER_COUNT);
		reg_val = params_period_size(params);
		writel(reg_val, dma_data->addr + RTS_REG_TX_TIMER_THRESHOLD);
		/* enable out interupt & clear sts */
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_STS);
		reg_val = reg_val & 0xF0;
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_STS);
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_EN);
		reg_val = reg_val | (1 << RTS_BIT_AUDIO_OUT_TIMER);
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_EN);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_data->subdata[stream].substream = substream;

		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_CTL);
		/* data format */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_U8:
			val = 0x0;
			format = 1;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
			val = 0x1;
			format = 2;
			break;
		case SNDRV_PCM_FORMAT_S24_3LE:
			val = 0x2;
			format = 3;
			break;
		/*for SPDIF*/
		case SNDRV_PCM_FORMAT_S20_3LE:
			val = 0x3;
			format = 3;
			break;
		default:
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0x3 << RTS_BIT_IN_DATA_WIDTH);
		reg_val = reg_val | (val << RTS_BIT_IN_DATA_WIDTH);
		if (dma_data->channels == 4) {
			reg_val = reg_val &
				~((u32) 0x3 << RTS_BIT_IN_DATA_WIDTH1);
			reg_val = reg_val | (val << RTS_BIT_IN_DATA_WIDTH1);
		}
		/* channels */
		switch (dma_data->channels) {
		case 1:
			switch (dma_data->subdata[stream].mono_mode) {
			case RTS_SND_DMA_MONO_LEFT_IN:
				val = 0x2;
				break;
			case RTS_SND_DMA_MONO_RIGHT_IN:
				val = 0x3;
				break;
			case RTS_SND_DMA_MONO_MIX_IN:
				val = 0x4;
				break;
			default:
				return -EINVAL;
			}
			break;
		case 2:
		case 4:
			switch (dma_data->subdata[stream].stereo_mode) {
			case RTS_SND_DMA_NORMAL_STEREO_IN:
				val = 0x0;
				break;
			case RTS_SND_DMA_EXCHANGE_STEREO_IN:
				val = 0x1;
				break;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0x7 << RTS_BIT_AUDIO_IN_MODE);
		reg_val = reg_val | (val << RTS_BIT_AUDIO_IN_MODE);
		if (dma_data->channels == 4) {
			reg_val = reg_val &
				~((u32) 0x7 << RTS_BIT_AUDIO_IN_MODE1);
			reg_val = reg_val | (val << RTS_BIT_AUDIO_IN_MODE1);
		}
		/* source select */
		switch (dma_data->subdata[stream].codec_sel) {
		case 0:
			val = 0x0;
			break;
		case 1:
			val = 0x1;
			break;
		default:
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0x1 << RTS_BIT_IN_SOURCE_SELECT);
		reg_val = reg_val | (val << RTS_BIT_IN_SOURCE_SELECT);
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_CTL);
		dma_data->subdata[stream].format = format;

		/* start address */
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_RX_SA;
		writel(reg_val, dma_data->addr + RTS_REG_RX_SA);
		/* buffer len */
		reg_val = (totbytes / format) << RTS_COMMON_FORMAT_SHIFT;
		writel(reg_val, dma_data->addr + RTS_REG_RX_LEN);
		/* rx buffer threshold */
		reg_val = reg_val >> 1;
		writel(reg_val, dma_data->addr + RTS_REG_RX_BUFFER_THRESHOLD);
		/* in timer */
		writel(0, dma_data->addr + RTS_REG_RX_TIMER_COUNT);
		reg_val = params_period_size(params);
		writel(reg_val, dma_data->addr + RTS_REG_RX_TIMER_THRESHOLD);
		/* enable in interupt & clear sts */
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_STS);
		reg_val = reg_val & 0xF;
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_STS);
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_EN);
		reg_val = reg_val | (1 << RTS_BIT_AUDIO_IN_TIMER);
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_EN);
	} else {
		return -EINVAL;
	}

	runtime->dma_bytes = (totbytes / format) << RTS_COMMON_FORMAT_SHIFT;

	return 0;
}

static int rts_pcm_hw_free(struct snd_soc_component *component,
				struct snd_pcm_substream *substream)
{
	u32 reg_val;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	int stream = substream->stream;

	DBG("rts pcm hw free\n");

	snd_pcm_set_runtime_buffer(substream, NULL);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data->subdata[stream].substream = NULL;
		/* disable out interupt */
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_EN);
		reg_val = reg_val & ~(1 << RTS_BIT_AUDIO_OUT_TIMER);
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_EN);
		/* flush tx fifo */
		reg_val = readl(dma_data->addr + RTS_REG_FIFO_CTL);
		reg_val = reg_val | ((u32) 0x1 << RTS_BIT_TX_FIFO_FLUSH);
		writel(reg_val, dma_data->addr + RTS_REG_FIFO_CTL);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_data->subdata[stream].substream = NULL;
		/* disable in interupt */
		reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_EN);
		reg_val = reg_val & ~(1 << RTS_BIT_AUDIO_IN_TIMER);
		writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_EN);
		/* flush rx fifo */
		reg_val = readl(dma_data->addr + RTS_REG_FIFO_CTL);
		reg_val = reg_val | ((u32) 0x1 << RTS_BIT_RX_FIFO_FLUSH);
		writel(reg_val, dma_data->addr + RTS_REG_FIFO_CTL);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int rts_pcm_prepare(struct snd_soc_component *component,
				struct snd_pcm_substream *substream)
{
	u32 reg_val;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	int stream = substream->stream;

	DBG("rts pcm prepare\n");

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* clear rp to tx buffer start address */
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_TX_RP;
		writel(reg_val, dma_data->addr + RTS_REG_TX_RP);
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_TX_WP;
		writel(reg_val, dma_data->addr + RTS_REG_TX_WP);
		/* flush tx fifo */
		reg_val = readl(dma_data->addr + RTS_REG_FIFO_CTL);
		reg_val = reg_val | ((u32) 0x1 << RTS_BIT_TX_FIFO_FLUSH);
		writel(reg_val, dma_data->addr + RTS_REG_FIFO_CTL);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* clear rp & wp to tx buffer start address */
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_RX_RP;
		writel(reg_val, dma_data->addr + RTS_REG_RX_RP);
		reg_val = runtime->dma_addr;
		reg_val = reg_val << RTS_BIT_RX_WP;
		writel(reg_val, dma_data->addr + RTS_REG_RX_WP);
		/* flush rx fifo */
		reg_val = readl(dma_data->addr + RTS_REG_FIFO_CTL);
		reg_val = reg_val | ((u32) 0x1 << RTS_BIT_RX_FIFO_FLUSH);
		writel(reg_val, dma_data->addr + RTS_REG_FIFO_CTL);
	} else {
		return -EINVAL;
	}

	return 0;
}

static void rts_enable_timer(struct rts_dma_data *dma_data,
		int dir, int enable)
{
	u32 reg_val;
	u8 reg, shift;

	if (dir == RTS_SND_DMA_TIMER_OUT) {
		/* out timer */
		reg = RTS_REG_TX_TIMER_THRESHOLD;
		shift = RTS_BIT_AUDIO_OUT_TIMER_EN;
	} else {
		/* in timer */
		reg = RTS_REG_RX_TIMER_THRESHOLD;
		shift = RTS_BIT_AUDIO_IN_TIMER_EN;
	}

	reg_val = readl(dma_data->addr + reg);
	if (enable)
		reg_val = reg_val | ((u32) 0x1 << shift);
	else
		reg_val = reg_val & ~((u32) 0x1 << shift);
	writel(reg_val, dma_data->addr + reg);
}

static int rts_pcm_trigger(struct snd_soc_component *component,
				struct snd_pcm_substream *substream, int cmd)
{
	u32 reg_val;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	int stream = substream->stream;

	DBG("rts pcm trigger\n");

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* enable out timer */
			rts_enable_timer(dma_data, RTS_SND_DMA_TIMER_OUT, 1);

			/* enable tx fifo */
			reg_val = readl(dma_data->addr + RTS_REG_FIFO_ENABLE);
			reg_val =
			    reg_val | ((u32) 0x1 << RTS_BIT_TX_FIFO_ENABLE);
			writel(reg_val, dma_data->addr + RTS_REG_FIFO_ENABLE);

			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			/* disable tx fifo */
			reg_val = readl(dma_data->addr + RTS_REG_FIFO_ENABLE);
			reg_val =
			    reg_val & ~((u32) 0x1 << RTS_BIT_TX_FIFO_ENABLE);
			writel(reg_val, dma_data->addr + RTS_REG_FIFO_ENABLE);

			/* disable out timer */
			rts_enable_timer(dma_data, RTS_SND_DMA_TIMER_OUT, 0);
			break;
		case SNDRV_PCM_TRIGGER_DRAIN:
			runtime->control->appl_ptr--;
			break;
		default:
			return -EINVAL;
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			/* enable in timer */
			rts_enable_timer(dma_data, RTS_SND_DMA_TIMER_IN, 1);

			/* enable rx fifo */
			reg_val = readl(dma_data->addr + RTS_REG_FIFO_ENABLE);
			reg_val =
			    reg_val | ((u32) 0x1 << RTS_BIT_RX_FIFO_ENABLE);
			writel(reg_val, dma_data->addr + RTS_REG_FIFO_ENABLE);

			reg_val = readl(dma_data->addr + RTS_REG_AUDIO_CTL);
			if (dma_data->channels == 1 || dma_data->channels == 2)
				reg_val = reg_val | ((u32)0x1 <<
						RTS_BIT_AUDIO_IN_C0_EN);
			else if (dma_data->channels == 4)
				reg_val = reg_val | ((u32)0x1 <<
					RTS_BIT_AUDIO_IN_C0_EN |
					(u32)0x1 << RTS_BIT_AUDIO_IN_C1_EN);
			else
				return -EINVAL;

			writel(reg_val, dma_data->addr + RTS_REG_AUDIO_CTL);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			reg_val = readl(dma_data->addr + RTS_REG_AUDIO_CTL);
			if (dma_data->channels == 1 || dma_data->channels == 2)
				reg_val = reg_val & ~((u32)0x1 <<
						RTS_BIT_AUDIO_IN_C0_EN);
			else if (dma_data->channels == 4)
				reg_val = reg_val & ~((u32)0x1 <<
					RTS_BIT_AUDIO_IN_C0_EN |
					(u32)0x1 << RTS_BIT_AUDIO_IN_C1_EN);
			else
				return -EINVAL;

			writel(reg_val, dma_data->addr + RTS_REG_AUDIO_CTL);

			/* disable rx fifo */
			reg_val = readl(dma_data->addr + RTS_REG_FIFO_ENABLE);
			reg_val =
			    reg_val & ~((u32) 0x1 << RTS_BIT_RX_FIFO_ENABLE);
			writel(reg_val, dma_data->addr + RTS_REG_FIFO_ENABLE);

			/* disable in timer */
			rts_enable_timer(dma_data, RTS_SND_DMA_TIMER_IN, 0);
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t rts_pcm_pointer(struct snd_soc_component *component,
					struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	unsigned long res;
	u32 reg_val1, reg_val2;
	int stream = substream->stream;

	DBG("rts pcm pointer\n");

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg_val1 = readl(dma_data->addr + RTS_REG_TX_RP);
		reg_val2 = readl(dma_data->addr + RTS_REG_TX_SA);
		res = (reg_val1 >> RTS_BIT_TX_RP) -
		      (reg_val2 >> RTS_BIT_TX_SA);
		res = (res >> RTS_COMMON_FORMAT_SHIFT) *
		      dma_data->subdata[stream].format;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		reg_val1 = readl(dma_data->addr + RTS_REG_RX_WP);
		reg_val2 = readl(dma_data->addr + RTS_REG_RX_SA);
		res = (reg_val1 >> RTS_BIT_RX_WP) -
		      (reg_val2 >> RTS_BIT_RX_SA);
		res = (res >> RTS_COMMON_FORMAT_SHIFT) *
		      dma_data->subdata[stream].format;
	} else {
		return SNDRV_PCM_POS_XRUN;
	}

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	return bytes_to_frames(substream->runtime, res);
}

static int rts_pcm_open(struct snd_soc_component *component,
			struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data;
	int stream = substream->stream;

	DBG("rts pcm open\n");

	if (!component) {
		pr_err("obtain component fail\n");
		return -EINVAL;
	}

	dma_data = snd_soc_component_get_drvdata(component);
	if (dma_data->runflag[stream] == 0)
		dma_data->runflag[stream] = 1;
	else
		return -EBUSY;

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
	snd_soc_set_runtime_hwparams(substream, &rts_dma_hardware);

	runtime->private_data = dma_data;
	return 0;
}

static int rts_pcm_close(struct snd_soc_component *component,
				struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	int stream = substream->stream;

	DBG("rts pcm close\n");

	dma_data->runflag[stream] = 0;
	runtime->private_data = NULL;

	return 0;
}

static int rts_pcm_ioctl(struct snd_soc_component *component,
				struct snd_pcm_substream *substream,
				unsigned int cmd, void *arg)
{
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int rts_pcm_ack(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	unsigned long appl_ptr = runtime->control->appl_ptr;
	int stream = substream->stream;
	snd_pcm_uframes_t appl_ofs;
	u32 reg_val;

	DBG("rts pcm ack\n");

	if (appl_ptr == (unsigned long)0) {
		DBG("rts_pcm_ack, appl_ptr == 0\n");
		appl_ofs = runtime->buffer_size - 1;
	} else {
		appl_ofs = (appl_ptr - 1) % runtime->buffer_size;
	}
	reg_val = frames_to_bytes(runtime, appl_ofs);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg_val = (reg_val / dma_data->subdata[stream].format) <<
			  RTS_COMMON_FORMAT_SHIFT;
		reg_val = (reg_val + runtime->dma_addr) << RTS_BIT_TX_WP;
		writel(reg_val, dma_data->addr + RTS_REG_TX_WP);
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		reg_val = (reg_val / dma_data->subdata[stream].format) <<
			  RTS_COMMON_FORMAT_SHIFT;
		reg_val = (reg_val + runtime->dma_addr) << RTS_BIT_RX_RP;
		writel(reg_val, dma_data->addr + RTS_REG_RX_RP);
	}

	return 0;
}

static int rts_pcm_copy(struct snd_soc_component *component,
			struct snd_pcm_substream *substream, int channel,
			unsigned long pos, struct iov_iter *iter,
			unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rts_dma_data *dma_data = runtime->private_data;
	char __user *apptr = iter->ubuf;
	unsigned int *hwptr;
	int stream = substream->stream;
	int pos_bytes, format, val;
	u8 val_8b;
	u16 val_16b;
	u32 val_32b;

	DBG("rts pcm copy\n");

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		format = dma_data->subdata[stream].format;
		pos_bytes = (pos / format) << RTS_COMMON_FORMAT_SHIFT;
		hwptr = (unsigned int *)(runtime->dma_area + pos_bytes);
		while (bytes > 0) {
			val = 0;
			if (format == 1) {
				get_user(val_8b, apptr);
				val = val_8b & 0xff;
				val = (val - 0x80) & 0xff;
			} else if (format == 2) {
				get_user(val_16b, (u16 *)apptr);
				val = val_16b & 0xffff;
			} else {
				get_user(val_32b, (u32 *)apptr);
				val = val_32b & 0xffffff;
			}
			*hwptr = (val << ((4 - format) * 8)) >> 7;
			hwptr = hwptr + 1;
			apptr = apptr + format;
			bytes = bytes - format;
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		format = dma_data->subdata[stream].format;
		pos_bytes = (pos / format) << RTS_COMMON_FORMAT_SHIFT;
		hwptr = (unsigned int *)(runtime->dma_area + pos_bytes);
		while (bytes > 0) {
			if (format == 1) {
				val_8b = (((*hwptr) >> 17) + 0x80) & 0xff;
				put_user(val_8b, apptr);
			} else if (format == 2) {
				val = ((*hwptr) << 7) >> ((4 - format) * 8);
				val_16b = val & 0xffff;
				put_user(val_16b, (u16 *)apptr);
			} else {
				val = ((*hwptr) << 7) >> ((4 - format) * 8);
				val_32b = val & 0xffffff;
				put_user(val_32b, (u32 *)apptr);
			}
			hwptr = hwptr + 1;
			apptr = apptr + format;
			bytes = bytes - format;
		}
	}

	return 0;
}

static irqreturn_t rts_irq_handler(int irq, void *data)
{
	int reg_val, reg;
	struct rts_dma_data *dma_data;
	struct snd_pcm_substream *substream;

	dma_data = (struct rts_dma_data *)data;
	reg_val = readl(dma_data->addr + RTS_REG_AUDIO_INT_STS);
	reg = readl(dma_data->addr + RTS_REG_AUDIO_INT_EN) & reg_val;

	if (reg & (1 << RTS_BIT_AUDIO_IN_TIMER)) {
		substream = dma_data->subdata[1].substream;
		if (substream)
			snd_pcm_period_elapsed(substream);
	}

	if (reg & (1 << RTS_BIT_AUDIO_OUT_TIMER)) {
		substream = dma_data->subdata[0].substream;
		if (substream)
			snd_pcm_period_elapsed(substream);
	}
	if (reg & (1 << RTS_BIT_AUDIO_OUT_DDR_THRESHOLD)) {
		DBG("aplay irq success\n");
		reg_val = readl(rts_dma_data->addr + RTS_REG_AUDIO_INT_EN);
		reg_val = reg_val & ~(1 << RTS_BIT_AUDIO_OUT_DDR_THRESHOLD);
		writel(reg_val, rts_dma_data->addr + RTS_REG_AUDIO_INT_EN);
		condition = 1;
		wake_up_interruptible(&wq);
	}

	writel(reg, dma_data->addr + RTS_REG_AUDIO_INT_STS);

	return IRQ_HANDLED;
}

static void rts_init_dma_buffer(struct snd_pcm *pcm,
				struct snd_soc_component *component,
				struct snd_soc_pcm_runtime *rtd, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct rts_dma_data *dma_data;
	struct snd_pcm_ops *ops;

	DBG("rts init dma buffer\n");

	if (!component) {
		pr_err("obtain component fail\n");
		return;
	}

	dma_data = snd_soc_component_get_drvdata(component);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = component->dev;
	buf->private_data = NULL;
	buf->area = dma_data->dma_buf[stream].area;
	buf->addr = dma_data->dma_buf[stream].addr;
	buf->bytes = dma_data->dma_buf[stream].bytes;

	ops = (struct snd_pcm_ops *)substream->ops;
	if (!ops->ack)
		ops->ack = rts_pcm_ack;
}

static int rts_pcm_construct(struct snd_soc_component *component,
				struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;

	DBG("rts pcm construct\n");

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream)
		rts_init_dma_buffer(pcm, component, rtd, SNDRV_PCM_STREAM_PLAYBACK);

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream)
		rts_init_dma_buffer(pcm, component, rtd, SNDRV_PCM_STREAM_CAPTURE);

	return 0;
}

static void rts_pcm_destruct(struct snd_soc_component *component,
				struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	DBG("rts pcm free dma buffer\n");

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		buf->area = NULL;
	}
}

static const char *rts_dma_mode_enum[] = {"normal stereo out",
					"L/R exchange R/L out",
					"mono out(single channel)",
					"mono out(channel copy stereo)",
					"normal stereo in",
					"L/R exchange R/L in",
					"mono in(select left channel)",
					"mono in(select right channel)",
					"mono in(half of left and right)"};

static SOC_ENUM_SINGLE_EXT_DECL(rts_dma_soc_mode_enum,
		rts_dma_mode_enum);

static int rts_dma_get_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_dma_data *dma_data =
				snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] =
		(dma_data->subdata[0].stereo_mode * 1000) +
		(dma_data->subdata[0].mono_mode * 100) +
		(dma_data->subdata[1].stereo_mode * 10) +
		(dma_data->subdata[1].mono_mode);

	return 0;
}

static int rts_dma_put_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_dma_data *dma_data =
				snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > e->items)
		return -EINVAL;

	switch (value) {
	case RTS_SND_DMA_NORMAL_STEREO_OUT:
	case RTS_SND_DMA_EXCHANGE_STEREO_OUT:
		dma_data->subdata[0].stereo_mode = value;
		break;
	case RTS_SND_DMA_MONO_OUT:
	case RTS_SND_DMA_MONO_TO_STEREO_OUT:
		dma_data->subdata[0].mono_mode = value;
		break;
	case RTS_SND_DMA_NORMAL_STEREO_IN:
	case RTS_SND_DMA_EXCHANGE_STEREO_IN:
		dma_data->subdata[1].stereo_mode = value;
		break;
	case RTS_SND_DMA_MONO_LEFT_IN:
	case RTS_SND_DMA_MONO_RIGHT_IN:
	case RTS_SND_DMA_MONO_MIX_IN:
		dma_data->subdata[1].mono_mode = value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_kcontrol_new rts_soc_platform_controls[] = {
	SOC_ENUM_EXT("Audio Mono/Stereo In/Out Mode", rts_dma_soc_mode_enum,
			rts_dma_get_enum, rts_dma_put_enum),
};

static struct snd_soc_component_driver rts_dma_platform_driver = {
	.open = rts_pcm_open,
	.close = rts_pcm_close,
	.ioctl = rts_pcm_ioctl,
	.hw_params = rts_pcm_hw_params,
	.hw_free = rts_pcm_hw_free,
	.prepare = rts_pcm_prepare,
	.trigger = rts_pcm_trigger,
	.pointer = rts_pcm_pointer,
	.copy = rts_pcm_copy,

	.pcm_construct = rts_pcm_construct,
	.pcm_destruct = rts_pcm_destruct,
	.controls = rts_soc_platform_controls,
	.num_controls = ARRAY_SIZE(rts_soc_platform_controls),
};

static u64 dma_mask = DMA_BIT_MASK(32);

static int rts_alloc_dma_buffer(struct rts_dma_data *dma_data)
{
	int i;
	struct snd_dma_buffer *buf;
	struct device *dev = &dma_data->pdev->dev;
	size_t size = rts_dma_hardware.buffer_bytes_max << 2;

	DBG("rts alloc dma buffer\n");

	if (!dev->dma_mask)
		dev->dma_mask = &dma_mask;
	if (!dev->coherent_dma_mask)
		dev->coherent_dma_mask = 0xffffffff;

	for (i = 0; i < 2; i++) {
		buf = &dma_data->dma_buf[i];
		buf->area = dma_alloc_coherent(dev, size,
				&buf->addr, GFP_KERNEL);
		if (!buf->area)
			return -ENOMEM;

		buf->bytes = size;
	}

	return 0;
}

static void rts_free_dma_buffer(struct rts_dma_data *dma_data)
{
	int i;
	struct snd_dma_buffer *buf;

	DBG("rts free dma buffer\n");

	for (i = 0; i < 2; i++) {
		buf = &dma_data->dma_buf[i];
		if (buf->area) {
			dma_free_coherent(&dma_data->pdev->dev, buf->bytes,
					buf->area, buf->addr);
			buf->area = NULL;
		}
	}
}

static void rts_dma_iounmap(struct rts_dma_data *dma_data)
{
	struct platform_device *pdev = dma_data->pdev;
	struct resource *res;

	if (dma_data->addr) {
		iounmap(dma_data->addr);
		dma_data->addr = NULL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
}

static int rts_dma_ioremap(struct rts_dma_data *dma_data)
{
	struct platform_device *pdev = dma_data->pdev;
	struct resource *res;
	int ret = 0;
	unsigned long size;
	u32 base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("Unable to get DMA address\n");
		ret = -ENXIO;
		goto out;
	}

	if (!request_mem_region(res->start, resource_size(res), "rts-dma")) {
		pr_err("Unable to request mem region\n");
		ret = -EBUSY;
		goto out;
	}

	base = res->start;
	size = res->end - res->start + 1;

	dma_data->addr = ioremap(base, size);
	if (dma_data->addr == NULL) {
		pr_err("failed to ioremap\n");
		ret = -ENXIO;
		goto out;
	}
out:
	if (ret)
		rts_dma_iounmap(dma_data);

	return ret;
}

static const struct of_device_id rts_dma_ids[] = {
	{
		.compatible = "realtek,rts3917-fpga-adma",
		.data = (void *)(TYPE_RTS3917 | TYPE_FPGA),
	}, {
		.compatible = "realtek,rts3917-adma",
		.data = (void *)TYPE_RTS3917,
	},
	{ },
};

static struct pinctrl *audio_pinctrl;

static struct cdev audio_cdev;
static dev_t audio_devno;
static struct device audio_dev;

static void rts_dma_dev_release(struct device *cd)
{
	put_device(cd->parent);
}

static int rts_dma_probe(struct platform_device *pdev)
{
	int ret, reg_val, result;
	// struct resource *res;
	int irq_res;
	struct rts_dma_data *dma_data;
	struct device_node *node;
	struct pinctrl_state *default_state = NULL;
	u32 size = 0;

	DBG("rts dma probe\n");

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		pr_err("reserved memory init fail, ret = %d\n", ret);
		return ret;
	}

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		pr_err("get memory-region property fail\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = of_property_read_u32_index(node, "size", 0, &size);
	of_node_put(node);
	node = NULL;
	if (ret) {
		pr_err("obtain reserved memory size fail, ret = %d\n", ret);
		goto exit;
	}

	if (size > 0)
		rts_dma_hardware.buffer_bytes_max = size / 8;

	dma_data =
	    devm_kzalloc(&pdev->dev, sizeof(struct rts_dma_data), GFP_KERNEL);
	if (dma_data == NULL) {
		pr_err("Unable to alloc dma data\n");
		ret = -ENOMEM;
		goto exit;
	}

	dma_data->sysmem_sd = devm_reset_control_get(&pdev->dev,
							"audio-sysmem-up");
	if (IS_ERR(dma_data->sysmem_sd)) {
		pr_err("not find audio sysmem sd control");
		ret = -EINVAL;
		goto io_err;
	}

	/* turn on system memory of audio */
	reset_control_deassert(dma_data->sysmem_sd);

	mdelay(5);

	dma_data->pdev = pdev;
	dma_data->devtype = (u32)of_device_get_match_data(&pdev->dev);

	audio_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (audio_pinctrl) {
#ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC_DMIC
		default_state = pinctrl_lookup_state(audio_pinctrl, "dmic");
#else
		default_state = pinctrl_lookup_state(audio_pinctrl, "amic");
#endif
		if (default_state) {
			ret = pinctrl_select_state(audio_pinctrl,
					default_state);
			if (ret)
				dev_err(&pdev->dev,
					"Audio: set default pins error %d\n",
					ret);
		}
	}

	ret = rts_dma_ioremap(dma_data);
	if (ret)
		goto io_err;

	// res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	irq_res = platform_get_irq_optional(pdev, 0);
	if (irq_res < 0) {
		pr_err("Unable to get dma irq\n");
		ret = -ENXIO;
		goto io_err;
	}

	dma_data->irq = irq_res;

	dma_data->runflag[0] = 0;
	dma_data->runflag[1] = 0;

	dma_data->subdata[0].substream = NULL;
	dma_data->subdata[1].substream = NULL;
	dma_data->subdata[0].mono_mode = RTS_SND_DMA_MONO_TO_STEREO_OUT;
	dma_data->subdata[1].mono_mode = RTS_SND_DMA_MONO_LEFT_IN;
	dma_data->subdata[0].stereo_mode = RTS_SND_DMA_EXCHANGE_STEREO_OUT;
	dma_data->subdata[1].stereo_mode = RTS_SND_DMA_EXCHANGE_STEREO_IN;

	node = of_parse_phandle(pdev->dev.of_node, "mono-mode", 0);
	if (node) {
		if (of_property_match_string(node,
					"mono-in", "left-channel") >= 0)
			dma_data->subdata[1].mono_mode =
						RTS_SND_DMA_MONO_LEFT_IN;
		else if (of_property_match_string(node,
					"mono-in", "right-channel") >= 0)
			dma_data->subdata[1].mono_mode =
						RTS_SND_DMA_MONO_RIGHT_IN;

		of_node_put(node);
		node = NULL;
	}

	dma_data->dma_buf[0].area = NULL;
	dma_data->dma_buf[1].area = NULL;
	ret = rts_alloc_dma_buffer(dma_data);
	if (ret) {
		pr_err("failed to alloc dma buffer\n");
		goto dma_err;
	}

	ret = request_irq(dma_data->irq, rts_irq_handler,
			  0, "audio-platform", dma_data);
	if (ret) {
		pr_err("failed to request irq %d\n", dma_data->irq);
		goto dma_err;
	}

#ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC
	pdev->dev.init_name = "audio-platform-for-pcm";
	rts_dma_platform_driver.name = "audio-platform-for-pcm";
	ret = devm_snd_soc_register_component(&pdev->dev,
					&rts_dma_platform_driver, NULL, 0);
	if (ret) {
		pr_err("register pcm platform failed\n");
		goto irq_err;
	}
#endif

#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	pdev->dev.init_name = "audio-platform-for-i2s";
	rts_dma_platform_driver.name = "audio-platform-for-i2s";
	ret = devm_snd_soc_register_component(&pdev->dev,
					&rts_dma_platform_driver, NULL, 0);
	if (ret) {
		pr_err("register i2s platform failed\n");
		goto irq_err;
	}
#endif

#ifdef CONFIG_SND_SOC_RTS_SPDIF
	pdev->dev.init_name = "audio-platform-for-spdif";
	rts_dma_platform_driver.name = "audio-platform-for-spdif";
	ret = devm_snd_soc_register_component(&pdev->dev,
					&rts_dma_platform_driver, NULL, 0);
	if (ret) {
		pr_err("register spdif platform failed\n");
		goto irq_err;
	}
#endif

	dev_set_drvdata(&pdev->dev, dma_data);

	/* init interupt en */
	reg_val = 0;
	writel(reg_val, dma_data->addr + RTS_REG_AUDIO_INT_EN);

	cdev_init(&audio_cdev, &rts_audio_fops);
	result = alloc_chrdev_region(&audio_devno, 0, 1, "rts_audio_devdma");
	if (result < 0) {
		pr_err("alloc_chrdev_region failed! result: %d\n", result);
		goto cdev_err;
	}
	result = cdev_add(&audio_cdev, audio_devno, 1);
	if (result < 0) {
		pr_err("cdev_add failed!result:%d\n", result);
		goto cdev_err;
	}

	audio_dev.devt = audio_devno;
	dev_set_name(&audio_dev, "rts_audio_devdma");
	audio_dev.parent = get_device(&pdev->dev);
	audio_dev.release = rts_dma_dev_release;
	ret = device_register(&audio_dev);
	if (ret) {
		pr_err("could not create chrdev node\n");
		goto cdev_err;
	}
	rts_dma_data = dma_data;
	return 0;

cdev_err:
	cdev_del(&audio_cdev);
	device_unregister(&audio_dev);
	unregister_chrdev_region(audio_devno, 1);
irq_err:
	free_irq(dma_data->irq, dma_data);
dma_err:
	rts_free_dma_buffer(dma_data);
io_err:
	if (dma_data->sysmem_sd)
		reset_control_assert(dma_data->sysmem_sd);
	rts_dma_iounmap(dma_data);
	devm_kfree(&pdev->dev, dma_data);
	dma_data = NULL;
exit:
	of_reserved_mem_device_release(&pdev->dev);

	return ret;
}

static int rts_dma_remove(struct platform_device *pdev)
{
	struct rts_dma_data *dma_data;
	struct pinctrl_state *default_state = NULL;

	int ret;

	dma_data = dev_get_drvdata(&pdev->dev);
	if (dma_data->sysmem_sd)
		reset_control_assert(dma_data->sysmem_sd);
	rts_dma_iounmap(dma_data);
	free_irq(dma_data->irq, dma_data);
	rts_free_dma_buffer(dma_data);

	devm_kfree(&pdev->dev, dma_data);
	dev_set_drvdata(&pdev->dev, NULL);
	dma_data = NULL;

	cdev_del(&audio_cdev);
	device_unregister(&audio_dev);
	unregister_chrdev_region(audio_devno, 1);
	if (audio_pinctrl) {
		default_state = pinctrl_lookup_state(audio_pinctrl,
						PINCTRL_STATE_DEFAULT);
		if (default_state) {
			ret = pinctrl_select_state(audio_pinctrl,
							default_state);
			if (ret)
				dev_err(&pdev->dev,
					"Audio: set default pins error %d\n",
					ret);
		}
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rts_dma_suspend(struct device *dev)
{
	struct rts_dma_data *dma_data = dev_get_drvdata(dev);

	DBG("%s\n", __func__);

	/* turn off system memory of audio */
	reset_control_assert(dma_data->sysmem_sd);

	return 0;
}

static int rts_dma_resume(struct device *dev)
{
	struct rts_dma_data *dma_data = dev_get_drvdata(dev);

	DBG("%s\n", __func__);

	/* turn on system memory of audio */
	reset_control_deassert(dma_data->sysmem_sd);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rts_dma_pm, rts_dma_suspend, rts_dma_resume);
#define rts_dma_pm_ops (&rts_dma_pm)
#else
#define rts_dma_pm_ops NULL
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver rts_dma_driver = {
	.driver = {
		.name = "audio-platform",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rts_dma_ids),
		.pm = rts_dma_pm_ops,
	},
	.probe = rts_dma_probe,
	.remove = rts_dma_remove,
};

static int __init rts_platform_init(void)
{
	DBG("rts platform init\n");

	return platform_driver_register(&rts_dma_driver);
}
module_init(rts_platform_init);

static void __exit rts_platform_exit(void)
{
	platform_driver_unregister(&rts_dma_driver);
}
module_exit(rts_platform_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wind_Han <wind_han@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek RTS ALSA soc codec driver");
