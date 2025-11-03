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

static int rts_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_component *component = NULL;
	int sample_rate = params_rate(params);
	int i2s_clk_freq;
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

	switch (sample_rate) {
	case 44100:
		i2s_clk_freq = 5644800;
		break;
	case 48000:
		i2s_clk_freq = 6144000;
		break;
	case 88200:
		i2s_clk_freq = 11289600;
		break;
	case 96000:
		i2s_clk_freq = 12288000;
		break;
	case 176400:
		i2s_clk_freq = 22579200;
		break;
	case 192000:
		i2s_clk_freq = 24576000;
		break;
	default:
		pr_err("invalid rate(%d)\n", sample_rate);
		return -EINVAL;
		}
	snd_soc_dai_set_sysclk(cpu_dai, RTS_I2S_CODEC_PC, i2s_clk_freq, 0);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->subdata[0].codec_sel = 2;
	else
		return -EINVAL;

	return 0;
}

static struct snd_soc_ops rts_spdif_ops = {
	.hw_params = rts_spdif_hw_params,
};

SND_SOC_DAILINK_DEFS(link1,
	DAILINK_COMP_ARRAY(COMP_CPU("spdif-platform")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("audio-platform-for-spdif")));

static struct snd_soc_dai_link rts_spdif_dai_link = {
	.name = "RTS SPDIF CARD AIF",
	.stream_name = "RTS SPDIF Playback",
	.ops = &rts_spdif_ops,
	SND_SOC_DAILINK_REG(link1),
};

static struct snd_soc_card rts_spdif_snd_soc_card = {
	.name = "RTS_SPDIF_CARD",
	.dai_link = &rts_spdif_dai_link,
	.num_links = 1,
};

static struct platform_device *rts_spdif_snd_device;

static int __init rts_snd_init(void)
{
	int ret = 0;

	pr_info("rts snd spdif init\n");

	rts_spdif_snd_device =
		platform_device_alloc("soc-audio", PLATFORM_DEVID_AUTO);
	if (!rts_spdif_snd_device) {
		pr_err("platform device alloc failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(rts_spdif_snd_device, &rts_spdif_snd_soc_card);
	ret = platform_device_add(rts_spdif_snd_device);
	if (ret) {
		pr_err("platform device add failed\n");
		platform_device_put(rts_spdif_snd_device);
	}

	return ret;
}
module_init(rts_snd_init);

static void __exit rts_snd_exit(void)
{
	platform_device_unregister(rts_spdif_snd_device);
}
module_exit(rts_snd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wind_Han <wind_han@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek RTS ALSA soc codec driver");
