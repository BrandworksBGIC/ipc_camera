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
#include <linux/module.h>

#include "rts_dma.h"
#include "rts_i2s.h"

#ifdef CONFIG_SND_SOC_RTS_DEBUG
#define DBG(...) \
	do { \
		pr_emerg("%s\n", __func__); \
		pr_emerg(__VA_ARGS__); \
	} while (0)
#else
#define DBG(...)
#endif

#define RTS_I2S_CODEC_MODE	RTS_I2S_CODEC_CE_256FS

static int rts_extern_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	struct snd_soc_component *component = NULL;
	int sample_rate = params_rate(params);
	int ret, i2s_clk_freq, mclk_div, bclk_div;
	struct rts_dma_data *dma_data = NULL;

	if (!component)
		component =
			snd_soc_rtdcom_lookup(rtd, "audio-platform-for-pcm");

	if (!component)
		component =
			snd_soc_rtdcom_lookup(rtd, "audio-platform-for-i2s");

	if (!component)
		component =
			snd_soc_rtdcom_lookup(rtd, "audio-platform-for-spdif");

	if (!component) {
		pr_err("obtain component fail\n");
		return -EINVAL;
	}

	dma_data = snd_soc_component_get_drvdata(component);

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
				  | SND_SOC_DAIFMT_NB_NF
				  | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
				  | SND_SOC_DAIFMT_NB_NF
				  | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	switch (RTS_I2S_CODEC_MODE) {
	case RTS_I2S_CODEC_PC:
		i2s_clk_freq = 96000000;
		mclk_div = 4;
		bclk_div = 0;
		break;
	case RTS_I2S_CODEC_CE_256FS:
		switch (sample_rate) {
		case 8000:
			i2s_clk_freq = 16384000;
			mclk_div = 4;
			bclk_div = 32;
			break;
		case 16000:
			i2s_clk_freq = 16384000;
			mclk_div = 4;
			bclk_div = 16;
			break;
		case 32000:
			i2s_clk_freq = 16384000;
			mclk_div = 2;
			bclk_div = 8;
			break;
		case 44100:
			i2s_clk_freq = 22579200;
			mclk_div = 2;
			bclk_div = 8;
			break;
		case 48000:
			i2s_clk_freq = 24576000;
			mclk_div = 2;
			bclk_div = 8;
			break;
		case 11025:
			i2s_clk_freq = 2822400;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 12000:
			i2s_clk_freq = 3072000;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 22050:
			i2s_clk_freq = 5644800;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 24000:
			i2s_clk_freq = 6144000;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 88200:
			i2s_clk_freq = 22579200;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 96000:
			i2s_clk_freq = 24576000;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 176400:
			i2s_clk_freq = 45158400;
			mclk_div = 1;
			bclk_div = 4;
			break;
		case 192000:
			i2s_clk_freq = 49152000;
			mclk_div = 1;
			bclk_div = 4;
			break;
		default:
			pr_err("invalid rate(%d)\n", sample_rate);
			return -EINVAL;
		}
		break;
	case RTS_I2S_CODEC_CE_512FS:
		switch (sample_rate) {
		case 8000:
			i2s_clk_freq = 16384000;
			mclk_div = 4;
			bclk_div = 32;
			break;
		case 16000:
			i2s_clk_freq = 16384000;
			mclk_div = 2;
			bclk_div = 16;
			break;
		case 32000:
			i2s_clk_freq = 16384000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 44100:
			i2s_clk_freq = 22579200;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 48000:
			i2s_clk_freq = 24576000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 11025:
			i2s_clk_freq = 5644800;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 12000:
			i2s_clk_freq = 6144000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 22050:
			i2s_clk_freq = 11289600;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 24000:
			i2s_clk_freq = 12288000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 88200:
			i2s_clk_freq = 45158400;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 96000:
			i2s_clk_freq = 49152000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 176400:
			i2s_clk_freq = 90316800;
			mclk_div = 1;
			bclk_div = 8;
			break;
		case 192000:
			i2s_clk_freq = 98304000;
			mclk_div = 1;
			bclk_div = 8;
			break;
		default:
			pr_err("invalid rate(%d)\n", sample_rate);
			return -EINVAL;
		}
		break;
	}
	snd_soc_dai_set_sysclk(cpu_dai, RTS_I2S_CODEC_MODE, i2s_clk_freq, 0);
	snd_soc_dai_set_sysclk(codec_dai, 0, i2s_clk_freq / mclk_div, 0);

	if (mclk_div > 0) {
		if (RTS_I2S_CODEC_MODE == RTS_I2S_CODEC_PC)
			snd_soc_dai_set_clkdiv(cpu_dai,
						RTS_I2S_DIV_MCLK_PC, mclk_div);
		else
			snd_soc_dai_set_clkdiv(cpu_dai,
						RTS_I2S_DIV_MCLK, mclk_div);
	}

	if (bclk_div > 0)
		snd_soc_dai_set_clkdiv(cpu_dai, RTS_I2S_DIV_BCLK, bclk_div);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->subdata[0].codec_sel = 1;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data->subdata[1].codec_sel = 1;
	else
		return -EINVAL;

	return 0;
}

static struct snd_soc_ops rts_extern_ops = {
	.hw_params = rts_extern_hw_params,
};

SND_SOC_DAILINK_DEFS(link1,
	DAILINK_COMP_ARRAY(COMP_CPU("i2s-platform")),
	DAILINK_COMP_ARRAY(COMP_CODEC("rt5514.0-0057", "rt5514-aif1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audio-platform-for-i2s")));

static struct snd_soc_dai_link rts_extern_dai_link[] = {
	{
		.name = "RTS EXTERN CARD AIF1",
		.stream_name = "RTS EXTERN PCM1",
		.ops = &rts_extern_ops,
		SND_SOC_DAILINK_REG(link1),
	}
};

static struct snd_soc_card rts_extern_snd_soc_card = {
	.name = "RTS_EXTERN_CARD",
	.dai_link = rts_extern_dai_link,
	.num_links = ARRAY_SIZE(rts_extern_dai_link),
};

static struct platform_device *rts_extern_snd_device;
static int __init rts_snd_init(void)
{
	int ret = 0;

	DBG("rts snd external codec init\n");

	rts_extern_snd_device =
		platform_device_alloc("soc-audio", PLATFORM_DEVID_AUTO);
	if (!rts_extern_snd_device) {
		pr_err("platform device alloc failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(rts_extern_snd_device, &rts_extern_snd_soc_card);
	ret = platform_device_add(rts_extern_snd_device);
	if (ret) {
		pr_err("platform device add failed\n");
		platform_device_put(rts_extern_snd_device);
	}

	return ret;
}
module_init(rts_snd_init);

static void __exit rts_snd_exit(void)
{
	platform_device_unregister(rts_extern_snd_device);
}
module_exit(rts_snd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lei_wang <lei_wang@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek audio machine driver");
