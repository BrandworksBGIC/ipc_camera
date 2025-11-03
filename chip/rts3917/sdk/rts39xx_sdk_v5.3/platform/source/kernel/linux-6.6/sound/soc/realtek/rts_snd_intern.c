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
#include <linux/regulator/consumer.h>

#include "rts_dma.h"
#include "rts_i2s.h"

static int rts_intern_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	int i2s_clk_freq = 96000000;
	int codec_clk_freq = 96000000;
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

	snd_soc_dai_set_sysclk(cpu_dai, RTS_I2S_CODEC_PC, i2s_clk_freq, 0);
	snd_soc_dai_set_sysclk(codec_dai, RTS_I2S_CODEC_PC, codec_clk_freq, 0);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->subdata[0].codec_sel = 0;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data->subdata[1].codec_sel = 0;
	else
		return -EINVAL;

	return 0;
}

static struct snd_soc_ops rts_intern_ops = {
	.hw_params = rts_intern_hw_params,
};

SND_SOC_DAILINK_DEFS(link1,
	DAILINK_COMP_ARRAY(COMP_CPU("pcm-platform")),
	DAILINK_COMP_ARRAY(COMP_CODEC("rts-codec", "rts-codec-digital")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audio-platform-for-pcm")));

SND_SOC_DAILINK_DEFS(link2,
	DAILINK_COMP_ARRAY(COMP_CPU("pcm-platform")),
	DAILINK_COMP_ARRAY(COMP_CODEC("rts-codec", "rts-codec-analog")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audio-platform-for-pcm")));

static struct snd_soc_dai_link rts_intern_dai_link[] = {
	{
		.name = "RTS INTERN CARD DIGITAL",
		.stream_name = "RTS INTERN PCM1",
		.ops = &rts_intern_ops,
		SND_SOC_DAILINK_REG(link1),
	},
	{
		.name = "RTS INTERN CARD ANALOG",
		.stream_name = "RTS INTERN PCM2",
		.ops = &rts_intern_ops,
		SND_SOC_DAILINK_REG(link2),
	},
};

static int rts_amic_extern_power(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
#ifdef CONFIG_SND_SOC_RTS_AMIC_PMU_RTP
	struct regulator *regulator;
	int ret;
#ifdef CONFIG_SND_SOC_RTS_AMIC_PMU_RTP_LDO1
	regulator = regulator_get(NULL, "LDO1");
#endif
	if (IS_ERR(regulator)) {
		ret = PTR_ERR(regulator);
		pr_err("fail to get regulator\n");
		return ret;
	}

	ret = regulator_set_voltage(regulator, 2800000, 2800000);
	if (ret) {
		pr_err("fail to set requlator\n");
		goto out;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = regulator_enable(regulator);
		if (ret) {
			pr_err("fail to enable regulator\n");
			goto out;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = regulator_disable(regulator);
		if (ret) {
			pr_err("fail to disable regulator\n");
			goto out;
		}
		break;
	default:
		pr_err("invalid event\n");
		break;
	}
	ret = 0;
out:
	regulator_put(regulator);
	if (ret)
		return ret;
#endif
	return 0;
}

static const struct snd_soc_dapm_widget rts_intern_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("AMIC EXTERN POWER", SND_SOC_NOPM, 0, 0,
			rts_amic_extern_power,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route rts_intern_dapm_routes[] = {
	{"AMICL", NULL, "AMIC EXTERN POWER"},
	{"AMICR", NULL, "AMIC EXTERN POWER"},
};

static struct snd_soc_card rts_intern_snd_soc_card = {
	.name = "RTS_INTERN_CARD",
	.dai_link = rts_intern_dai_link,
	.num_links = ARRAY_SIZE(rts_intern_dai_link),
	.dapm_widgets = rts_intern_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rts_intern_dapm_widgets),
	.dapm_routes = rts_intern_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rts_intern_dapm_routes),
};

static struct platform_device *rts_intern_snd_device;

static int __init rts_snd_init(void)
{
	int ret = 0;

	pr_info("rts snd internal codec init\n");

	rts_intern_snd_device =
		platform_device_alloc("soc-audio", PLATFORM_DEVID_AUTO);
	if (!rts_intern_snd_device) {
		pr_err("platform device alloc failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(rts_intern_snd_device, &rts_intern_snd_soc_card);
	ret = platform_device_add(rts_intern_snd_device);
	if (ret) {
		pr_err("platform device add failed\n");
		platform_device_put(rts_intern_snd_device);
	}

	return ret;
}
module_init(rts_snd_init);

static void __exit rts_snd_exit(void)
{
	platform_device_unregister(rts_intern_snd_device);
}
module_exit(rts_snd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wind_Han <wind_han@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek RTS ALSA soc codec driver");
