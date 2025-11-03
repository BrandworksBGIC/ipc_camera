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
#include <linux/clk.h>
#include <sound/pcm_params.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>

#include "rts_hw_id.h"
#include "rts_i2s.h"

#ifdef CONFIG_SND_SOC_RTS_DEBUG
#define DBG(args...)	pr_emerg("%s: %s", __func__, args)
#else
#define DBG(args...)
#endif

#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
static int rts_i2s_dai_enable_ldoi2s(struct rts_i2s_dai_data *dai_data,
								int force)
{
	u32 reg_val;

	mutex_lock(&dai_data->ldoi2s_mutex);
	if (dai_data->ldoi2s_ref == 0 || force == 1) {
		reg_val = readl(dai_data->addr + RTS_REG_I2S_OCP_CTL);
		writel(reg_val | ((u32)0x1 << RTS_LDOI2S_POW),
				dai_data->addr + RTS_REG_I2S_OCP_CTL);
	}
	dai_data->ldoi2s_ref = force ? dai_data->ldoi2s_ref :
						dai_data->ldoi2s_ref + 1;
	mutex_unlock(&dai_data->ldoi2s_mutex);

	return 0;
}

static int rts_i2s_dai_disable_ldoi2s(struct rts_i2s_dai_data *dai_data,
								int force)
{
	u32 reg_val;

	mutex_lock(&dai_data->ldoi2s_mutex);
	dai_data->ldoi2s_ref = force ? dai_data->ldoi2s_ref :
						dai_data->ldoi2s_ref - 1;
	if (dai_data->ldoi2s_ref == 0 || force == 1) {
		reg_val = readl(dai_data->addr + RTS_REG_I2S_OCP_CTL);
		writel(reg_val & ~((u32)0x1 << RTS_LDOI2S_POW),
				dai_data->addr + RTS_REG_I2S_OCP_CTL);
	}
	mutex_unlock(&dai_data->ldoi2s_mutex);

	return 0;
}

static int rts_i2s_dai_enable_clk(struct rts_i2s_dai_data *dai_data, int force)
{
	mutex_lock(&dai_data->clk_mutex);
	if (dai_data->clk_ref == 0 || force == 1)
		clk_prepare_enable(dai_data->i2s_clk);
	dai_data->clk_ref = force ? dai_data->clk_ref : dai_data->clk_ref + 1;

	mutex_unlock(&dai_data->clk_mutex);

	return 0;
}

static int rts_i2s_dai_disable_clk(struct rts_i2s_dai_data *dai_data, int force)
{
	mutex_lock(&dai_data->clk_mutex);
	dai_data->clk_ref = force ? dai_data->clk_ref : dai_data->clk_ref - 1;
	if (dai_data->clk_ref == 0 || force == 1)
		clk_disable(dai_data->i2s_clk);

	mutex_unlock(&dai_data->clk_mutex);

	return 0;
}

static int rts_i2s_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val, val, reg_val1;

	DBG("rts i2s dai hw params\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	reg_val = readl(dai_data->addr + RTS_REG_I2S_CTL);
	reg_val1 = readl(dai_data->addr + RTS_REG_I2S_CFG);

	switch (params_rate(params)) {
	case 8000:
		val = 0x0;
		break;
	case 16000:
		val = 0x1;
		break;
	case 32000:
		val = 0x2;
		break;
	case 44100:
		val = 0xA;
		break;
	case 48000:
		val = 0x5;
		break;
	case 12000:
		val = 0x3;
		break;
	case 24000:
		val = 0x4;
		break;
	case 96000:
		val = 0x6;
		break;
	case 192000:
		val = 0x7;
		break;
	case 11025:
		val = 0x8;
		break;
	case 22050:
		val = 0x9;
		break;
	case 88200:
		val = 0xB;
		break;
	case 176400:
		val = 0xC;
		break;
	default:
		pr_err("invalid rate(%d)\n", params_rate(params));
		return -EINVAL;
	}
	reg_val = reg_val & ~((u32) 0xF << RTS_I2S_SAMPLE_RATE);
	reg_val = reg_val | (val << RTS_I2S_SAMPLE_RATE);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
		val = 0x4;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		val = 0x0;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 0x2;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val = 0x1;
		break;
	default:
		return -EINVAL;
	}

	reg_val1 = reg_val1 & ~((u32) 0x7 << RTS_SEL_I2S_DATA_LEN);
	reg_val1 = reg_val1 | (val << RTS_SEL_I2S_DATA_LEN);
	writel(reg_val1, dai_data->addr + RTS_REG_I2S_CFG);
	writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);

	dai_data->channels = params_channels(params);

	return 0;
}

static int rts_i2s_dai_trigger(struct snd_pcm_substream *substream,
			       int cmd, struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val;

	DBG("rts i2s dai trigger\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	reg_val = readl(dai_data->addr + RTS_REG_I2S_CTL);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			reg_val = reg_val | ((u32) 0x1 << RTS_I2S_OUT_GO);
			writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			reg_val = reg_val & ~((u32) 0x1 << RTS_I2S_OUT_GO);
			writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
			break;
		default:
			return -EINVAL;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			reg_val = reg_val | ((u32) 0x1 << RTS_I2S_IN_GO);
			writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			reg_val = reg_val & ~((u32) 0x1 << RTS_I2S_IN_GO);
			writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int rts_i2s_config_output_voltage(struct rts_i2s_dai_data *dai_data)
{
	u32 reg_val;

	reg_val = readl(dai_data->addr + RTS_REG_I2S_OCP_CFG);
	reg_val &= ~((u32)0x7 << RTS_REG_TUNE_VO);
	reg_val |= ((u32)0x7 << RTS_REG_TUNE_VO);
	writel(reg_val, dai_data->addr + RTS_REG_I2S_OCP_CFG);

	return 0;
}

static int rts_i2s_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;

	DBG("rts i2s dai startup\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	dai_data->substream[substream->stream] = substream;
	rts_i2s_dai_enable_ldoi2s(dai_data, 0);

	rts_i2s_dai_enable_clk(dai_data, 0);

	return 0;
}

static void rts_i2s_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;

	DBG("rts i2s dai shutdown\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	dai_data->substream[substream->stream] = NULL;
#ifndef CONFIG_SND_SOC_RTS_RT5677
	rts_i2s_dai_disable_clk(dai_data, 0);
	rts_i2s_dai_disable_ldoi2s(dai_data, 0);
#endif
}

#ifdef CONFIG_SND_SOC_RTS_RT5677
void rts_i2s_clock_disable(struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;

	dai_data = snd_soc_dai_get_drvdata(dai);

	rts_i2s_dai_disable_clk(dai_data, 0);
	rts_i2s_dai_disable_ldoi2s(dai_data, 0);
}
EXPORT_SYMBOL(rts_i2s_clock_disable);
#endif

static int rts_i2s_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val, val, reg_val1, mono, i2s_lj_mono;
	u32 inv_bclk, inv_lrck;
	u32 mode_bclk, mode_lrck;

	DBG("rts i2s dai set fmt\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	reg_val = readl(dai_data->addr + RTS_REG_I2S_CFG);
	reg_val1 = readl(dai_data->addr + RTS_REG_I2S_CTL);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		val = 0x2;
		if (dai_data->channels == 1)
			mono = 1;
		else
			mono = 0;

		break;
	case SND_SOC_DAIFMT_DSP_B:
		val = 0x3;
		if (dai_data->channels == 1)
			mono = 1;
		else
			mono = 0;

		break;
	case SND_SOC_DAIFMT_I2S:
		val = 0x0;
		if (dai_data->channels == 1)
			i2s_lj_mono = 1;
		else
			i2s_lj_mono = 0;

		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val = 0x1;
		if (dai_data->channels == 1)
			i2s_lj_mono = 1;
		else
			i2s_lj_mono = 0;

		break;
	default:
		return -EINVAL;
	}

	reg_val = reg_val & (~((u32) 0x7 << RTS_SEL_I2S_FORMAT) &
				~((u32) 0x7 << RTS_TCON_SEL_I2S_DATA_FORMAT));
	reg_val = reg_val | (val << RTS_SEL_I2S_FORMAT) |
				(val << RTS_TCON_SEL_I2S_DATA_FORMAT);
	reg_val1 = reg_val1 & (~((u32) 0x1 << RTS_EN_I2S_MONO) &
				~((u32) 0x1 << RTS_I2S_LJ_MODE_SEL));
	reg_val1 = reg_val1 | (mono << RTS_EN_I2S_MONO) |
				(i2s_lj_mono << RTS_I2S_LJ_MODE_SEL);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		inv_bclk = 0x0;
		inv_lrck = 0x0;
		val = 0x0;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		inv_bclk = 0x1;
		inv_lrck = 0x1;
		val = 0x3;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		inv_bclk = 0x1;
		inv_lrck = 0x0;
		val = 0x1;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		inv_bclk = 0x0;
		inv_lrck = 0x1;
		val = 0x2;
		break;
	default:
		return -EINVAL;
	}

	reg_val = reg_val & (~((u32) 0x1 << RTS_INV_TDM_LRCK) &
					~((u32) 0x1 << RTS_INV_TDM_BCLK));
	reg_val = reg_val | (inv_lrck << RTS_INV_TDM_LRCK) |
					(inv_bclk << RTS_INV_TDM_BCLK);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		mode_bclk = 0x0;
		mode_lrck = 0x0;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		mode_bclk = 0x1;
		mode_lrck = 0x0;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		mode_bclk = 0x0;
		mode_lrck = 0x1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		mode_bclk = 0x1;
		mode_lrck = 0x1;
		break;
	default:
		return -EINVAL;
	}

	inv_bclk &= mode_bclk;
	inv_lrck &= mode_lrck;
	reg_val = reg_val & (~((u32) 0x1 << RTS_INV_TDM_LRCK_MST) &
				~((u32) 0x1 << RTS_INV_TDM_BCLK_MST));
	reg_val = reg_val | (inv_lrck << RTS_INV_TDM_LRCK_MST) |
				(inv_bclk << RTS_INV_TDM_BCLK_MST);
	reg_val1 = reg_val1 & (~((u32) 0x1 << RTS_I2S_WS_OE) &
				~((u32) 0x1 << RTS_I2S_SCK_OE));
	reg_val1 = reg_val1 | (mode_lrck << RTS_I2S_WS_OE) |
				(mode_bclk << RTS_I2S_SCK_OE);
	writel(reg_val1, dai_data->addr + RTS_REG_I2S_CTL);

	writel(reg_val, dai_data->addr + RTS_REG_I2S_CFG);

	return 0;
}

static int rts_i2s_dai_set_clkdiv(struct snd_soc_dai *dai, int div_id, int div)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val, val;

	DBG("rts i2s dai set clkdiv\n");

	dai_data = snd_soc_dai_get_drvdata(dai);

	reg_val = readl(dai_data->addr + RTS_REG_I2S_CTL);
	switch (div_id) {
	case RTS_I2S_DIV_BCLK:
		div = div >> 2;
		val = div;
		reg_val = reg_val & ~((u32) 0xFF << RTS_BCLK_INT_DIV);
		reg_val = reg_val | (val << RTS_BCLK_INT_DIV);
		writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
		break;
	case RTS_I2S_DIV_MCLK:
		switch (div) {
		case 1:
			val = 0x0;
			break;
		case 2:
			val = 0x1;
			break;
		case 3:
			val = 0x2;
			break;
		case 4:
			val = 0x3;
			break;
		case 6:
			val = 0x4;
			break;
		case 8:
			val = 0x5;
			break;
		case 12:
			val = 0x6;
			break;
		case 16:
			val = 0x7;
			break;
		case 32:
			val = 0x8;
			break;
		case 64:
			val = 0x9;
			break;
		case 128:
			val = 0xA;
			break;
		default:
			pr_err("invalid clock divider(%d)\n", div);
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0xF << RTS_MCLK_DIV_SEL);
		reg_val = reg_val | (val << RTS_MCLK_DIV_SEL);
		writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
		break;
	case RTS_I2S_DIV_MCLK_PC:
		switch (div) {
		case 1:
			val = 0x0;
			break;
		case 2:
			val = 0x1;
			break;
		case 3:
			val = 0x2;
			break;
		case 4:
			val = 0x3;
			break;
		case 6:
			val = 0x4;
			break;
		case 8:
			val = 0x5;
			break;
		case 12:
			val = 0x6;
			break;
		case 16:
			val = 0x7;
			break;
		case 32:
			val = 0x8;
			break;
		case 64:
			val = 0x9;
			break;
		case 128:
			val = 0xA;
			break;
		default:
			pr_err("invalid clock divider(%d)\n", div);
			return -EINVAL;
		}
		reg_val = reg_val & ~((u32) 0xF << RTS_MCLK_DIV_SEL);
		reg_val = reg_val | (val << RTS_MCLK_DIV_SEL);
		writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);
		break;
	default:
		pr_err("invalid clock divider id(%d)\n", div_id);
		return -EINVAL;
	}

	return 0;
}

static int rts_i2s_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int rfs, int dir)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val;

	DBG("rts i2s dai set sysclk\n");

	dai_data = snd_soc_dai_get_drvdata(dai);

	reg_val = readl(dai_data->addr + RTS_REG_I2S_CTL);

	switch (clk_id) {
	case RTS_I2S_CODEC_PC:
		/* pc codec */
		reg_val = reg_val & ~((u32) 0x1 << RTS_SCK_GEN_SEL);
		break;
	case RTS_I2S_CODEC_CE_256FS:
	case RTS_I2S_CODEC_CE_512FS:
		/* ce codec */
		reg_val = reg_val | ((u32) 0x1 << RTS_SCK_GEN_SEL);
		break;
	default:
		pr_err("invalid sys clock select(%d)\n", clk_id);
		return -EINVAL;
	}
	writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);

	/* set i2s_clk freq */
	clk_set_rate(dai_data->i2s_clk, rfs);

	return 0;
}

static struct snd_soc_dai_ops rts_i2s_dai_ops = {
	.startup = rts_i2s_dai_startup,
	.shutdown = rts_i2s_dai_shutdown,
	.trigger = rts_i2s_dai_trigger,
	.hw_params = rts_i2s_dai_hw_params,
	.set_fmt = rts_i2s_dai_set_fmt,
	.set_clkdiv = rts_i2s_dai_set_clkdiv,
	.set_sysclk = rts_i2s_dai_set_sysclk,
};

static struct snd_soc_dai_driver rts_i2s_dai = {
	.name = "i2s-platform",
	.id = 1,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = RTS_I2S_RATES,
		.formats = RTS_I2S_FORMATS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = RTS_I2S_RATES,
		.formats = RTS_I2S_FORMATS,
	},
	.ops = &rts_i2s_dai_ops,
};
#endif

#ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC
static int rts_pcm_dai_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	DBG("rts pcm dai startup\n");

	return 0;
}

static void rts_pcm_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	DBG("rts pcm dai shutdown\n");
}

static int rts_pcm_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int rfs, int dir)
{
	DBG("rts pcm dai set sysclk\n");

	return 0;
}

static struct snd_soc_dai_ops rts_pcm_dai_ops = {
	.startup = rts_pcm_dai_startup,
	.shutdown = rts_pcm_dai_shutdown,
	.set_sysclk = rts_pcm_dai_set_sysclk,
};

static struct snd_soc_dai_driver rts_pcm_dai = {
	.name = "pcm-platform",
	.id = 1,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = RTS_I2S_RATES,
		.formats = RTS_I2S_FORMATS,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 4,
		.rates = RTS_I2S_RATES,
		.formats = RTS_I2S_FORMATS,
	},
	.ops = &rts_pcm_dai_ops,
};
#endif

#ifdef CONFIG_SND_SOC_RTS_SPDIF
static int rts_spdif_dai_enable_clk(struct rts_i2s_dai_data *dai_data,
								int force)
{
	mutex_lock(&dai_data->spdif_mutex);
	if (dai_data->spdif_ref == 0 || force == 1)
		clk_prepare_enable(dai_data->spdif_clk);

	dai_data->spdif_ref = force ? dai_data->spdif_ref :
						dai_data->spdif_ref + 1;
	mutex_unlock(&dai_data->spdif_mutex);

	return 0;
}

static int rts_spdif_dai_disable_clk(struct rts_i2s_dai_data *dai_data,
								int force)
{
	mutex_lock(&dai_data->spdif_mutex);
	dai_data->spdif_ref = force ? dai_data->spdif_ref :
						dai_data->spdif_ref - 1;
	if (dai_data->spdif_ref == 0 || force == 1)
		clk_disable(dai_data->spdif_clk);

	mutex_unlock(&dai_data->spdif_mutex);

	return 0;
}

static int rts_spdif_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;

	DBG("rts spdif dai startup\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	dai_data->substream[substream->stream] = substream;

	rts_spdif_dai_enable_clk(dai_data, 0);

	return 0;
}

static void rts_spdif_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;

	DBG("rts spdif dai shutdown\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	dai_data->substream[substream->stream] = NULL;
	rts_spdif_dai_disable_clk(dai_data, 0);
}

static int rts_spdif_dai_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val;

	DBG("rts spdif dai trigger\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	reg_val = readl(dai_data->addr + RTS_REG_SPDIF_CFG);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			reg_val = reg_val | ((u32) 0x1 << RTS_BIT_SPDIF_OUT_EN);
			writel(reg_val, dai_data->addr + RTS_REG_SPDIF_CFG);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			reg_val = reg_val & ~((u32) 0x1 <<
							RTS_BIT_SPDIF_OUT_EN);
			writel(reg_val, dai_data->addr + RTS_REG_SPDIF_CFG);
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int rts_spdif_dai_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val, val;

	DBG("rts spdif dai hw params\n");

	dai_data = snd_soc_dai_get_drvdata(dai);
	reg_val = readl(dai_data->addr + RTS_REG_SPDIF_CFG);
	switch (params_rate(params)) {
	case 48000:
		val = 0x0;
		break;
	case 96000:
		val = 0x1;
		break;
	case 192000:
		val = 0x2;
		break;
	case 44100:
		val = 0x4;
		break;
	case 88200:
		val = 0x5;
		break;
	case 176400:
		val = 0x6;
		break;
	default:
		pr_err("invalid rate(%d)\n", params_rate(params));
		return -EINVAL;
	}
	reg_val = reg_val & ~((u32) 0x7 << RTS_BIT_SPDIF_SR);
	reg_val = reg_val | (val << RTS_BIT_SPDIF_SR);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val = 0x0;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val = 0x1;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 0x2;
		break;
	default:
		return -EINVAL;
	}
	reg_val = reg_val & ~((u32) 0x7 << RTS_BIT_SPDIF_BITS);
	reg_val = reg_val | (val << RTS_BIT_SPDIF_BITS);
	writel(reg_val, dai_data->addr + RTS_REG_SPDIF_CFG);

	dai_data->channels = params_channels(params);

	return 0;
}

static int rts_spdif_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int rfs, int dir)
{
	struct rts_i2s_dai_data *dai_data;
	u32 reg_val;

	DBG("rts spdif dai set sysclk\n");

	dai_data = snd_soc_dai_get_drvdata(dai);

	reg_val = readl(dai_data->addr + RTS_REG_I2S_CTL);
	reg_val = reg_val | ((u32) 0x1 << RTS_SCK_GEN_SEL);
	writel(reg_val, dai_data->addr + RTS_REG_I2S_CTL);

	/* set spdif_clk freq */
	clk_set_rate(dai_data->spdif_clk, rfs);

	return 0;
}

static struct snd_soc_dai_ops rts_spdif_dai_ops = {
	.startup = rts_spdif_dai_startup,
	.shutdown = rts_spdif_dai_shutdown,
	.trigger = rts_spdif_dai_trigger,
	.hw_params = rts_spdif_dai_hw_params,
	.set_sysclk = rts_spdif_dai_set_sysclk,
};

static struct snd_soc_dai_driver rts_spdif_dai = {
	.name = "spdif-platform",
	.id = 1,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = RTS_SPDIF_RATES,
		.formats = RTS_SPDIF_FORMATS,
	},
	.ops = &rts_spdif_dai_ops,
};
#endif

#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
static const struct snd_soc_component_driver rts_i2s_component = {
	.name = "i2s-dai-platform",
};
#endif

#ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC
static const struct snd_soc_component_driver rts_pcm_component = {
	.name = "pcm-dai-platform",
};
#endif

#ifdef CONFIG_SND_SOC_RTS_SPDIF
static const struct snd_soc_component_driver rts_spdif_component = {
	.name = "spdif-dai-platform",
};
#endif

static void rts_i2s_iounmap(struct rts_i2s_dai_data *dai_data)
{
	struct platform_device *pdev = dai_data->pdev;
	struct resource *res;

	if (dai_data->addr) {
		iounmap(dai_data->addr);
		dai_data->addr = NULL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
}

static int rts_i2s_ioremap(struct rts_i2s_dai_data *dai_data)
{
	struct platform_device *pdev = dai_data->pdev;
	struct resource *res;
	int ret = 0;
	unsigned long size;
	u32 base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("Unable to get I2S address\n");
		ret = -ENXIO;
		goto out;
	}

	if (!request_mem_region(res->start, resource_size(res),
				"dai-platform")) {
		pr_err("Unable to request mem region\n");
		ret = -EBUSY;
		goto out;
	}

	base = res->start;
	size = res->end - res->start + 1;

	dai_data->addr = ioremap(base, size);
	if (dai_data->addr == NULL) {
		pr_err("failed to ioremap\n");
		ret = -ENXIO;
		goto out;
	}
out:
	if (ret)
		rts_i2s_iounmap(dai_data);

	return ret;
}

static const struct of_device_id rts_i2s_ids[] = {
	{
		.compatible = "realtek,rts3917-fpga-adai",
		.data = (void *)(TYPE_RTS3917 | TYPE_FPGA),
	}, {
		.compatible = "realtek,rts3917-adai",
		.data = (void *)TYPE_RTS3917,
	},
	{ },
};

static int rts_i2s_probe(struct platform_device *pdev)
{
	int ret;
	struct rts_i2s_dai_data *dai_data;

	DBG("rts i2s probe\n");

	dai_data =
	    devm_kzalloc(&pdev->dev, sizeof(struct rts_i2s_dai_data),
			 GFP_KERNEL);
	if (dai_data == NULL) {
		pr_err("Unable to alloc I2S data\n");
		return -ENOMEM;
	}
	dai_data->pdev = pdev;
	dai_data->devtype = (u32)of_device_get_match_data(&pdev->dev);
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	/* reset spdif */
	dai_data->spdif_rst = devm_reset_control_get(&pdev->dev, "reset-spdif");
	if (!dai_data->spdif_rst) {
		pr_err("can't find spdif control\n");
		ret = -EINVAL;
		goto spdif_err;
	}
	reset_control_reset(dai_data->spdif_rst);
#endif
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	/* reset i2s */
	dai_data->i2s_rst = devm_reset_control_get(&pdev->dev, "reset-i2s");
	if (!dai_data->i2s_rst) {
		pr_err("can't find i2s control\n");
		ret = -EINVAL;
		goto spdif_err;
	}
	reset_control_reset(dai_data->i2s_rst);
#endif

	ret = rts_i2s_ioremap(dai_data);
	if (ret)
		goto spdif_err;

#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	dai_data->i2s_clk = clk_get(NULL, "i2s_ck");
	if (IS_ERR(dai_data->i2s_clk)) {
		pr_err("failed to get i2s_ck\n");
		ret = -ENOENT;
		goto spdif_err;
	}

	if (TYPE_FPGA & dai_data->devtype)
		clk_set_parent(dai_data->i2s_clk, clk_get(NULL, "usb_pll_7"));
#endif

#ifdef CONFIG_SND_SOC_RTS_SPDIF
	dai_data->spdif_clk = clk_get(NULL, "spdif_ck");
	if (IS_ERR(dai_data->spdif_clk)) {
		pr_err("failed to get spdif_ck\n");
		ret = -ENOENT;
		goto spdif_err;
	}
	dai_data->spdif_ref = 0;
	mutex_init(&dai_data->spdif_mutex);
#endif
	dai_data->clk_ref = 0;
	mutex_init(&dai_data->clk_mutex);
	dai_data->ldoi2s_ref = 0;
	mutex_init(&dai_data->ldoi2s_mutex);

#ifdef CONFIG_SND_SOC_RTS_INTERN_CODEC
	ret = devm_snd_soc_register_component(&pdev->dev,
				&rts_pcm_component, &rts_pcm_dai, 1);

	if (ret) {
		pr_err("register rts_pcm_component failed\n");
		goto spdif_err;
	}
#endif

#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	ret = devm_snd_soc_register_component(&pdev->dev,
				&rts_i2s_component, &rts_i2s_dai, 1);

	if (ret) {
		pr_err("register rts_i2s_component failed\n");
		goto spdif_err;
	}

	rts_i2s_config_output_voltage(dai_data);
#endif

#ifdef CONFIG_SND_SOC_RTS_SPDIF
	ret = devm_snd_soc_register_component(&pdev->dev,
				&rts_spdif_component, &rts_spdif_dai, 1);

	if (ret) {
		pr_err("register rts_spdif_component failed\n");
		goto spdif_err;
	}
#endif

	dev_set_drvdata(&pdev->dev, dai_data);

	return 0;
spdif_err:
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	if (dai_data->spdif_rst)
		reset_control_assert(dai_data->spdif_rst);
#endif
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	if (dai_data->i2s_rst)
		reset_control_assert(dai_data->i2s_rst);
#endif
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	if (dai_data->spdif_clk) {
		clk_put(dai_data->spdif_clk);
		dai_data->spdif_clk = NULL;
	}
#endif
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	if (dai_data->i2s_clk) {
		clk_put(dai_data->i2s_clk);
		dai_data->i2s_clk = NULL;
	}
#endif
	rts_i2s_iounmap(dai_data);
	devm_kfree(&pdev->dev, dai_data);
	dai_data = NULL;

	return ret;
}

static int rts_i2s_remove(struct platform_device *pdev)
{
	struct rts_i2s_dai_data *dai_data;

	dai_data = dev_get_drvdata(&pdev->dev);
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	if (dai_data->i2s_rst)
		reset_control_assert(dai_data->i2s_rst);

	if (dai_data->i2s_clk) {
		clk_put(dai_data->i2s_clk);
		dai_data->i2s_clk = NULL;
	}
#endif
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	if (dai_data->spdif_rst)
		reset_control_assert(dai_data->spdif_rst);

	if (dai_data->spdif_clk) {
		clk_put(dai_data->spdif_clk);
		dai_data->spdif_clk = NULL;
	}
#endif
	rts_i2s_iounmap(dai_data);

	snd_soc_unregister_component(&pdev->dev);
	devm_kfree(&pdev->dev, dai_data);
	dev_set_drvdata(&pdev->dev, NULL);
	dai_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
#if defined(CONFIG_SND_SOC_RTS_EXTERN_CODEC) || defined(CONFIG_SND_SOC_RTS_SPDIF)
static int rts_dai_running(struct rts_i2s_dai_data *dai_data)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (dai_data->substream[i] &&
				snd_pcm_running(dai_data->substream[i]))
			return 1;
	}

	return 0;
}
#endif

static int rts_dai_suspend(struct device *dev)
{
#if defined(CONFIG_SND_SOC_RTS_EXTERN_CODEC) || defined(CONFIG_SND_SOC_RTS_SPDIF)
	struct rts_i2s_dai_data *dai_data = dev_get_drvdata(dev);
#endif
	DBG("%s\n", __func__);
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	if (rts_dai_running(dai_data)) {
		rts_i2s_dai_disable_clk(dai_data, 1);
		rts_i2s_dai_disable_ldoi2s(dai_data, 1);
	}
	reset_control_assert(dai_data->i2s_rst);
#endif
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	if (rts_dai_running(dai_data))
		rts_spdif_dai_disable_clk(dai_data, 1);
	reset_control_assert(dai_data->spdif_rst);
#endif
	return 0;
}

static int rts_dai_resume(struct device *dev)
{
#if defined(CONFIG_SND_SOC_RTS_EXTERN_CODEC) || defined(CONFIG_SND_SOC_RTS_SPDIF)
	struct rts_i2s_dai_data *dai_data = dev_get_drvdata(dev);
#endif
	DBG("%s\n", __func__);
#ifdef CONFIG_SND_SOC_RTS_EXTERN_CODEC
	reset_control_reset(dai_data->i2s_rst);
	if (rts_dai_running(dai_data)) {
		rts_i2s_dai_enable_ldoi2s(dai_data, 1);
		rts_i2s_dai_enable_clk(dai_data, 1);
	}
#endif
#ifdef CONFIG_SND_SOC_RTS_SPDIF
	reset_control_reset(dai_data->spdif_rst);
	if (rts_dai_running(dai_data))
		rts_spdif_dai_enable_clk(dai_data, 1);
#endif
	return 0;
}

static SIMPLE_DEV_PM_OPS(rts_dai_pm, rts_dai_suspend, rts_dai_resume);
#define rts_dai_pm_ops (&rts_dai_pm)
#else
#define rts_dai_pm_ops NULL
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver rts_i2s_driver = {
	.driver = {
		.name = "dai-platform",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rts_i2s_ids),
		.pm = rts_dai_pm_ops,
	},
	.probe = rts_i2s_probe,
	.remove = rts_i2s_remove,
};

static int __init rts_i2s_init(void)
{
	DBG("rts i2s init\n");

	return platform_driver_register(&rts_i2s_driver);
}
module_init(rts_i2s_init);

static void __exit rts_i2s_exit(void)
{
	platform_driver_unregister(&rts_i2s_driver);
}
module_exit(rts_i2s_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wind_Han <wind_han@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek RTS ALSA soc codec driver");
