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
#include <linux/module.h>
#include <sound/tlv.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include "rts_audio_driver.h"

#include "rts_codec.h"
#include "rts_hw_id.h"

#ifdef CONFIG_SND_SOC_RTS_DEBUG
#define DBG(args...)	pr_emerg("%s: %s", __func__, args)
#else
#define DBG(args...)
#endif

/*************************/
#define DEVICE_NAME "rts_audio_devcodec"
static struct cdev audio_cdev;
static dev_t audio_devno;
static struct device audio_dev;
struct rts_codec_data *rts_codec_data;

static int rts_audio_open(struct inode *inode, struct file *file)
{
	DBG("rts_audio_devcodec open succeed\n");
	return 0;
}

static int rts_audio_release(struct inode *inode, struct file *file)
{
	DBG("rts_audio_devcodec release succeed\n");
	return 0;
}

static long rts_audio_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int retval;
	struct snd_soc_component *component = NULL;
	struct snd_soc_dapm_context *dapm = NULL;

	component = snd_soc_lookup_component(&rts_codec_data->pdev->dev, "rts-codec");
	if (!component) {
		DBG("fail to find correct component\n");
		return -EINVAL;
	}

	dapm = snd_soc_component_get_dapm(component);
	if (!dapm) {
		DBG("fail to find correct dapm context\n");
		return -EINVAL;
	}

	DBG("enter ioctl\n");
	switch (cmd) {
	case SET_MONO_IN:
		if (copy_from_user(&retval, (void __user *)arg, sizeof(int)))
			return -EFAULT;

		if (retval == 0) {
			rts_codec_data->mono_in_mode = RTS_CODEC_MONO_LEFT_IN;
			snd_soc_dapm_disable_pin(dapm, "AMICR");
			snd_soc_dapm_disable_pin(dapm, "LINR");
			snd_soc_dapm_enable_pin(dapm, "AMICL");
			snd_soc_dapm_enable_pin(dapm, "LINL");
		} else if (retval == 1) {
			rts_codec_data->mono_in_mode = RTS_CODEC_MONO_RIGHT_IN;
			snd_soc_dapm_disable_pin(dapm, "AMICL");
			snd_soc_dapm_disable_pin(dapm, "LINL");
			snd_soc_dapm_enable_pin(dapm, "AMICR");
			snd_soc_dapm_enable_pin(dapm, "LINR");
		} else {
			pr_err("set mono in mode failed, retval = %d\n", retval);
			break;
		}
		snd_soc_dapm_sync(dapm);
		break;
	default:
		break;
	}
	return 0;
}

static const struct file_operations rts_audio_fops = {
	.owner = THIS_MODULE,
	.open = rts_audio_open,
	.release = rts_audio_release,
	.unlocked_ioctl = rts_audio_ioctl,
};
/*************************/

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6350, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -3350, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_comp_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(dmic_boost_tlv, 0, 1000, 0);
static const DECLARE_TLV_DB_SCALE(alc_ft_boost_tlv, 0, 75, 0);
static const DECLARE_TLV_DB_SCALE(ao_gain_tlv, -50, 50, 0);
static const DECLARE_TLV_DB_SCALE(amic_gain_tlv, -600, 75, 0);

static const char * const rts_codec_afe_gain_enum[] = {"-7dB", "-6dB",
							"-5dB", "1dB"};
static SOC_ENUM_DOUBLE_DECL(rts_ana_adc_gain_enum,
		RTS_REG_ADDA_ANA_CFG2, RTS_AFEL_GAIN,
		RTS_AFER_GAIN, rts_codec_afe_gain_enum);

static const char * const rts_codec_afe_source_enum[] = {"from mic in",
							"from line in"};
static SOC_ENUM_SINGLE_DECL(rts_ana_adc_source_enum,
		RTS_REG_ADDA_ANA_CFG4, RTS_SELIN,
		rts_codec_afe_source_enum);

static const char * const rts_capture_mono_enum[] = {"select left in",
							"select right in"};
static SOC_ENUM_SINGLE_EXT_DECL(rts_codec_capture_mono_enum,
							rts_capture_mono_enum);

static const char * const rts_playback_mono_enum[] = {"select left out",
							"select right out"};
static SOC_ENUM_SINGLE_EXT_DECL(rts_codec_playback_mono_enum,
							rts_playback_mono_enum);

static int rts_codec_dai_enable_clk(struct rts_codec_data *codec_data,
								int force)
{
	mutex_lock(&codec_data->codec_mutex);
	if (codec_data->codec_ref == 0 || force == 1)
		clk_prepare_enable(codec_data->codec_clk);

	codec_data->codec_ref = force ? codec_data->codec_ref :
						codec_data->codec_ref + 1;
	mutex_unlock(&codec_data->codec_mutex);

	return 0;
}

static int rts_codec_dai_disable_clk(struct rts_codec_data *codec_data,
								int force)
{
	mutex_lock(&codec_data->codec_mutex);
	codec_data->codec_ref = force ? codec_data->codec_ref :
						codec_data->codec_ref - 1;
	if (codec_data->codec_ref == 0 || force == 1)
		clk_disable(codec_data->codec_clk);

	mutex_unlock(&codec_data->codec_mutex);

	return 0;
}

static int rts_sync_adc_register(struct snd_soc_component *component)
{
	u32 reg_val;

	DBG("rts sync adc register\n");

	reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG1);
	snd_soc_component_write(component, RTS_REG_ADC_CFG1,
						reg_val | ((u32)0x1 << 31));

	return 0;
}

static int rts_sync_dac_register(struct snd_soc_component *component)
{
	u32 reg_val;

	DBG("rts sync dac register\n");

	reg_val = snd_soc_component_read(component, RTS_REG_DAC_CFG);
	snd_soc_component_write(component, RTS_REG_DAC_CFG,
						reg_val | ((u32)0x1 << 27));

	return 0;
}

static int rts_adc_put_volsw(struct snd_kcontrol *kcontrol,
		  struct snd_ctl_elem_value *ucontrol)
{
	int err;
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);

	DBG("rts put volsw\n");

	err = snd_soc_put_volsw(kcontrol, ucontrol);
	if (err < 0)
		return err;

	rts_sync_adc_register(component);

	return 0;
}

static int rts_dac_put_volsw(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	int err;
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);

	DBG("rts put volsw\n");

	err = snd_soc_put_volsw(kcontrol, ucontrol);
	if (err < 0)
		return err;

	rts_sync_dac_register(component);

	return 0;
}

static int rts_codec_capture_mono_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > e->items)
		return -EINVAL;

	codec_data->mono_in_mode = value;

	return 0;
}

static int rts_codec_capture_mono_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = codec_data->mono_in_mode;

	return 0;
}

static int rts_codec_playback_mono_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > e->items)
		return -EINVAL;

	codec_data->mono_out_mode = value;

	return 0;
}

static int rts_codec_playback_mono_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = codec_data->mono_out_mode;

	return 0;
}

static const struct snd_kcontrol_new rts_soc_codec_controls[] = {
	SOC_DOUBLE_EXT_TLV("Master Playback Volume", RTS_REG_DAC_CFG,
			   RTS_DA_GAIN_L, RTS_DA_GAIN_R, 127, 0,
			   snd_soc_get_volsw, rts_dac_put_volsw, dac_vol_tlv),
	SOC_DOUBLE("Master Playback Switch", RTS_REG_DAC_CFG,
		   RTS_DA_MUTE_L, RTS_DA_MUTE_R, 1, 1),
	SOC_SINGLE_TLV("ADC Compensate Capture Volume", RTS_REG_ADC_CFG1,
		       RTS_AD_COMP_GAIN, 3, 0, adc_comp_tlv),
	SOC_DOUBLE_EXT_TLV("Rear Dmic Capture Volume", RTS_REG_ADC_CFG1,
			   RTS_DMIC_BOOST_GAIN_L,
			   RTS_DMIC_BOOST_GAIN_R,
			   3, 0, snd_soc_get_volsw,
			   rts_adc_put_volsw, dmic_boost_tlv),
	SOC_DOUBLE("Dmic Capture Switch", RTS_REG_ADC_CFG2,
		   RTS_DMIC_MIX_MUTE_L, RTS_DMIC_MIX_MUTE_R, 1, 1),
	SOC_DOUBLE("Amic Capture Switch", RTS_REG_ADC_CFG2,
		   RTS_AD_MIX_MUTE_L, RTS_AD_MIX_MUTE_R, 1, 1),
	SOC_SINGLE_EXT_TLV("Front Amic Capture Volume", RTS_REG_AGC_CFG5,
			   RTS_ALC_FT_BOOST, 39, 0,
			   snd_soc_get_volsw, rts_adc_put_volsw,
			   alc_ft_boost_tlv),
	SOC_DOUBLE_EXT_TLV("Master Capture Volume", RTS_REG_ADC_CFG1,
			RTS_AD_GAIN_L, RTS_AD_GAIN_R, 100, 0,
			snd_soc_get_volsw, rts_adc_put_volsw, adc_vol_tlv),
	SOC_DOUBLE_EXT("Master Capture Switch", RTS_REG_ADC_CFG1,
		       RTS_AD_MUTE_L, RTS_AD_MUTE_R, 1, 1,
		       snd_soc_get_volsw, rts_adc_put_volsw),
	SOC_ENUM("Rear Line Capture Volume", rts_ana_adc_gain_enum),
	SOC_DOUBLE_TLV("Rear Amic Capture Volume", RTS_REG_ALC_PGA_CFG4,
		RTS_MICL_GAIN, RTS_MICR_GAIN, 69, 0,
		amic_gain_tlv),
	SOC_ENUM("Capture Source Select", rts_ana_adc_source_enum),
	SOC_DOUBLE_TLV("Line Playback Volume", RTS_REG_ADDA_ANA_CFG4,
		RTS_LOL_GAIN, RTS_LOR_GAIN, 3, 0, ao_gain_tlv),
	SOC_SINGLE_EXT_TLV("Master Capture L Volume", RTS_REG_ADC_CFG1,
			RTS_AD_GAIN_L, 100, 0,
			snd_soc_get_volsw, rts_adc_put_volsw, adc_vol_tlv),
	SOC_SINGLE_EXT_TLV("Master Capture R Volume", RTS_REG_ADC_CFG1,
			RTS_AD_GAIN_R, 100, 0,
			snd_soc_get_volsw, rts_adc_put_volsw, adc_vol_tlv),
	SOC_ENUM_EXT("Capture Mono Select L/R", rts_codec_capture_mono_enum,
			rts_codec_capture_mono_get, rts_codec_capture_mono_put),
	SOC_ENUM_EXT("Playback Mono Select L/R", rts_codec_playback_mono_enum,
		rts_codec_playback_mono_get, rts_codec_playback_mono_put),
};

static const struct snd_kcontrol_new rts_soc_dmic2_controls[] = {
	SOC_DOUBLE_EXT_TLV("ADC2 Volume", RTS_REG_ADC2_CFG1,
			RTS_AD_GAIN_L, RTS_AD_GAIN_R, 127, 0,
			snd_soc_get_volsw, rts_adc_put_volsw, adc_vol_tlv),
	SOC_DOUBLE_EXT("ADC2 Switch", RTS_REG_ADC2_CFG1,
		       RTS_AD_MUTE_L, RTS_AD_MUTE_R, 1, 1,
		       snd_soc_get_volsw, rts_adc_put_volsw),
	SOC_SINGLE_TLV("ADC2 Compensate Capture Volume", RTS_REG_ADC2_CFG1,
		       RTS_AD_COMP_GAIN, 3, 0, adc_comp_tlv),
	SOC_DOUBLE_EXT_TLV("Rear Dmic2 Capture Volume", RTS_REG_ADC2_CFG1,
			   RTS_DMIC_BOOST_GAIN_L,
			   RTS_DMIC_BOOST_GAIN_R,
			   3, 0, snd_soc_get_volsw,
			   rts_adc_put_volsw, dmic_boost_tlv),
	SOC_DOUBLE("Dmic2 Capture Switch", RTS_REG_ADC2_CFG2,
		   RTS_DMIC_MIX_MUTE_L, RTS_DMIC_MIX_MUTE_R, 1, 1),
};

static int rts_codec_rwable_register(struct snd_soc_component *component,
				     unsigned int reg)
{
	DBG("rts codec rwable register\n");

	switch (reg) {
	case RTS_REG_DAC_CFG:
	case RTS_REG_DAC_PDM_DFG:
	case RTS_REG_ADC_CFG1:
	case RTS_REG_ADC_CFG2:
	case RTS_REG_AGC_CFG1:
	case RTS_REG_AGC_CFG2:
	case RTS_REG_AGC_CFG3:
	case RTS_REG_AGC_CFG4:
	case RTS_REG_AGC_CFG5:
	case RTS_REG_AGC_CFG6:
	case RTS_REG_ADC2_CFG1:
	case RTS_REG_ADC2_CFG2:
	case RTS_REG_AGC2_CFG1:
	case RTS_REG_AGC2_CFG2:
	case RTS_REG_TCON_CFG:
	case RTS_REG_ADDA_ANA_CFG1:
	case RTS_REG_ADDA_ANA_CFG2:
	case RTS_REG_ADDA_ANA_CFG3:
	case RTS_REG_ADDA_ANA_CFG4:
	case RTS_REG_ALC_PGA_CFG1:
	case RTS_REG_ALC_PGA_CFG2:
	case RTS_REG_ALC_PGA_CFG3:
	case RTS_REG_ALC_PGA_CFG4:
	case RTS_REG_ALC_PGA_CFG5:
	case RTS_REG_ALC_PGA_CFG6:
		return 1;
	default:
		return 0;
	}
}

static unsigned int rts_soc_codec_read(struct snd_soc_component *component,
				       unsigned int reg)
{
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	DBG("rts soc codec read\n");

	if (rts_codec_rwable_register(component, reg) == 0) {
		pr_err("invalid register address 0x%x\n", reg);
		return 0;
	}

	return readl(codec_data->addr + reg);
}

static int rts_soc_codec_write(struct snd_soc_component *component, unsigned int reg,
			       unsigned int value)
{
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	DBG("rts soc codec write\n");

	if (rts_codec_rwable_register(component, reg) == 0) {
		pr_err("invalid register address 0x%x\n", reg);
		return 0;
	}

	writel(value, codec_data->addr + reg);

	return 0;
}

static int rts_codec_adc_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG1);
		snd_soc_component_write(component, RTS_REG_ADC_CFG1,
				reg_val & ~((u32)0x1 << RTS_AD_RST_N));

		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG1);
		snd_soc_component_write(component, RTS_REG_ADC_CFG1,
				reg_val | ((u32)0x1 << RTS_AD_RST_N));
		break;
	case SND_SOC_DAPM_POST_PMU:
		udelay(3);
		rts_sync_adc_register(component);
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_dac_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		reg_val = snd_soc_component_read(component, RTS_REG_DAC_CFG);
		snd_soc_component_write(component, RTS_REG_DAC_CFG,
				reg_val &
				~((u32)0x1 << RTS_DA_RST_N) &
				~((u32)0x1 << RTS_DAMOD_RST_N));

		reg_val = snd_soc_component_read(component, RTS_REG_DAC_CFG);
		snd_soc_component_write(component, RTS_REG_DAC_CFG,
				reg_val |
				((u32)0x1 << RTS_DA_RST_N) |
				((u32)0x1 << RTS_DAMOD_RST_N));
		break;
	case SND_SOC_DAPM_POST_PMU:
		rts_sync_dac_register(component);
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_pdm_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		reg_val = snd_soc_component_read(component,
							RTS_REG_DAC_PDM_DFG);
		snd_soc_component_write(component, RTS_REG_DAC_PDM_DFG,
				reg_val & ~((u32)0x1 << RTS_SDM_RST_N));

		reg_val = snd_soc_component_read(component,
							RTS_REG_DAC_PDM_DFG);
		snd_soc_component_write(component, RTS_REG_DAC_PDM_DFG,
				reg_val | ((u32)0x1 << RTS_SDM_RST_N));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_tcon_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* set tcon rst_n */
		reg_val = snd_soc_component_read(component, RTS_REG_TCON_CFG);
		snd_soc_component_write(component, RTS_REG_TCON_CFG,
				reg_val &
				~((u32)0x1 << RTS_AUDIO_IP_TCON_RST_N));

		/* release tcon rst_n */
		reg_val = snd_soc_component_read(component, RTS_REG_TCON_CFG);
		snd_soc_component_write(component, RTS_REG_TCON_CFG,
				reg_val |
				((u32)0x1 << RTS_AUDIO_IP_TCON_RST_N));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_power_vcm(struct snd_soc_component *component,
							int on, int dir)
{
	static int rts_codec_power_count = 0;
	u32 reg_val;

	if (on) {
		if (rts_codec_power_count <= 0) {
			reg_val = snd_soc_component_read(component,
					RTS_REG_ADDA_ANA_CFG1);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG1, reg_val |
					((u32)0x1 << RTS_POW_MBIAS) |
					((u32)0x1 << RTS_POW_VCM));
		}
		rts_codec_power_count++;
		if (dir == 0) {
			reg_val = snd_soc_component_read(component,
					RTS_REG_ADDA_ANA_CFG2);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG2,
					reg_val |
					((u32)0x1 << RTS_VCM_READY));
		}
	} else {
		rts_codec_power_count--;
		if (dir == 0) {
			reg_val = snd_soc_component_read(component,
					RTS_REG_ADDA_ANA_CFG2);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG2,
					reg_val &
					~((u32)0x1 << RTS_VCM_READY));
		}
		if (rts_codec_power_count <= 0) {
			reg_val = snd_soc_component_read(component,
					RTS_REG_ADDA_ANA_CFG1);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG1,
					reg_val &
					~((u32)0x1 << RTS_POW_MBIAS) &
					~((u32)0x1 << RTS_POW_VCM));
		}
	}

	return 0;
}

static int rts_codec_power_depop(struct snd_soc_component *component, int on)
{
	u32 reg_val;

	if (on) {
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
					reg_val |
					((u32)0x1 << RTS_POW_DEPOP) |
					((u32)0x1 << RTS_POW_DEPOP_CK) |
					((u32)0x1 << RTS_POW_DEPOP_OP));
	} else {
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
					reg_val &
					~((u32)0x1 << RTS_POW_DEPOP) &
					~((u32)0x1 << RTS_POW_DEPOP_CK) &
					~((u32)0x1 << RTS_POW_DEPOP_OP));
	}

	return 0;
}

static int rts_codec_loutl_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (codec_data->channels == 1 &&
			codec_data->mono_out_mode == RTS_CODEC_MONO_LEFT_OUT) {
			/* norm = 0 */
			reg_val = snd_soc_component_read(component,
							RTS_REG_ADDA_ANA_CFG3);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG3,
					reg_val & ~((u32)0x1 << RTS_NORM));
		}
		/* lo pow on */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val | ((u32)0x1 << RTS_POW_LOL));
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val & ~((u32)0x1 << RTS_MUTE_L));
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (codec_data->channels == 1 &&
			codec_data->mono_out_mode == RTS_CODEC_MONO_LEFT_OUT) {
			/* norm = 0 */
			reg_val = snd_soc_component_read(component,
							RTS_REG_ADDA_ANA_CFG3);
			snd_soc_component_write(component,
					RTS_REG_ADDA_ANA_CFG3,
					reg_val & ~((u32)0x1 << RTS_NORM));
		}
		/* lo pow off */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val | ((u32)0x1 << RTS_MUTE_L));
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val & ~((u32)0x1 << RTS_POW_LOL));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_loutr_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* norm = 0 */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val & ~((u32)0x1 << RTS_NORM));
		/* lo pow on */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val | ((u32)0x1 << RTS_POW_LOR));
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val & ~((u32)0x1 << RTS_MUTE_R));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* norm = 0 */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val & ~((u32)0x1 << RTS_NORM));
		/* lo pow off */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val | ((u32)0x1 << RTS_MUTE_R));
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val & ~((u32)0x1 << RTS_POW_LOR));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}


static int rts_codec_ana_dac_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* depop on */
		rts_codec_power_depop(component, 1);
		/* vcm mbias on */
		rts_codec_power_vcm(component, 1, 0);
		/* norm = 1 */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG3);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG3,
				reg_val | ((u32)0x1 << RTS_NORM));
		/* depop off */
		rts_codec_power_depop(component, 0);
		/* dacvref on */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val | ((u32)0x1 << RTS_POW_DACVREF));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* dacref off */
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADDA_ANA_CFG1);
		snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val & ~((u32)0x1 << RTS_POW_DACVREF));
		/* vcm mbias off */
		rts_codec_power_vcm(component, 0, 0);
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_ana_dacl_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	if (codec_data->channels == 2 || (codec_data->channels == 1 &&
			codec_data->mono_out_mode == RTS_CODEC_MONO_LEFT_OUT))
		return rts_codec_ana_dac_event(w, kcontrol, event);

	return 0;
}

static int rts_codec_ana_dacr_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);

	if (codec_data->channels == 1 &&
			codec_data->mono_out_mode == RTS_CODEC_MONO_RIGHT_OUT)
		return rts_codec_ana_dac_event(w, kcontrol, event);

	return 0;
}

static int rts_codec_vcm_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		rts_codec_power_vcm(component, 1, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		rts_codec_power_vcm(component, 0, 1);
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_ana_adcl_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val &
				~((u32)0x1 << RTS_AD_MIX_MUTE_L));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val |
				((u32)0x1 << RTS_AD_MIX_MUTE_L));
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_ana_adcr_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val &
				~((u32)0x1 << RTS_AD_MIX_MUTE_R));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val |
				((u32)0x1 << RTS_AD_MIX_MUTE_R));
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_dmic_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val &
				~((u32)0x1 << RTS_DMIC_MIX_MUTE_L) &
				~((u32)0x1 << RTS_DMIC_MIX_MUTE_R));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		reg_val = snd_soc_component_read(component, RTS_REG_ADC_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC_CFG2,
				reg_val |
				((u32)0x1 << RTS_DMIC_MIX_MUTE_L) |
				((u32)0x1 << RTS_DMIC_MIX_MUTE_R));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static int rts_codec_dmic2_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	u32 reg_val;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADC2_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC2_CFG2,
				reg_val &
				~((u32)0x1 << RTS_DMIC_MIX_MUTE_L) &
				~((u32)0x1 << RTS_DMIC_MIX_MUTE_R));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		reg_val = snd_soc_component_read(component,
						RTS_REG_ADC2_CFG2);
		snd_soc_component_write(component, RTS_REG_ADC2_CFG2,
				reg_val |
				((u32)0x1 << RTS_DMIC_MIX_MUTE_L) |
				((u32)0x1 << RTS_DMIC_MIX_MUTE_R));
		break;
	default:
		pr_err("invalid event\n");
		break;
	}

	return 0;
}

static const char *rts_codec_out_l_src[] = {"DAC_L", "DAC_R",
						"AIN_L", "AIN_R"};
static const char *rts_codec_out_r_src[] = {"DAC_R", "DAC_L",
						"AIN_R", "AIN_L"};
static SOC_ENUM_SINGLE_DECL(rts_codec_aout_l_enum,
	RTS_REG_ADDA_ANA_CFG3, RTS_MUX_L, rts_codec_out_l_src);

static const struct snd_kcontrol_new rts_codec_aout_l_mux =
	SOC_DAPM_ENUM("AOUT left channel source", rts_codec_aout_l_enum);

static SOC_ENUM_SINGLE_DECL(rts_codec_aout_r_enum,
	RTS_REG_ADDA_ANA_CFG3, RTS_MUX_R, rts_codec_out_r_src);

static const struct snd_kcontrol_new rts_codec_aout_r_mux =
	SOC_DAPM_ENUM("AOUT right channel source", rts_codec_aout_r_enum);

static const struct snd_soc_dapm_widget rts_soc_codec_dapm_widgets[] = {
	/* Capture widgets */
	SND_SOC_DAPM_AIF_OUT("RTSD Capture", "RTS-CODEC Digital Capture",
			0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("RTSA Capture", "RTS-CODEC Analog Capture",
			0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC Filter", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC2 Filter", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SUPPLY("ADC Filter CLK", RTS_REG_TCON_CFG, RTS_AD_CLK_EN,
			0, rts_codec_adc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("ADC2 Filter CLK", RTS_REG_TCON_CFG, RTS_AD2_CLK_EN,
			0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DMIC CLK", RTS_REG_TCON_CFG, RTS_DMIC_CLK_EN,
			0, rts_codec_dmic_event,
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC2 CLK", RTS_REG_TCON_CFG, RTS_DMIC2_CLK_EN,
			0, rts_codec_dmic2_event,
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* analog widgets */
	SND_SOC_DAPM_ADC_E("Analog ADCL Filter", NULL,
			RTS_REG_ADDA_ANA_CFG1,
			RTS_POW_ADCL, 0, rts_codec_ana_adcl_event,
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("Analog ADCR Filter", NULL,
			RTS_REG_ADDA_ANA_CFG1,
			RTS_POW_ADCR, 0, rts_codec_ana_adcr_event,
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("ADC ANA CLK", RTS_REG_TCON_CFG, RTS_AD_ANA_CLK_EN,
			0, NULL, 0),
	SND_SOC_DAPM_PGA_E("MICBIASL", SND_SOC_NOPM,
			RTS_POW_MICBIAS, 0, NULL, 0,
			rts_codec_vcm_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MICBIASR", SND_SOC_NOPM,
			RTS_POW_MICBIAS, 0, NULL, 0,
			rts_codec_vcm_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Playback widgets */
	SND_SOC_DAPM_AIF_IN("RTSD Playback", "RTS-CODEC Digital Playback",
			0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("RTSA Playback", "RTS-CODEC Analog Playback",
			0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Filter", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SUPPLY("DAC Filter CLK", RTS_REG_TCON_CFG, RTS_DA_CLK_EN,
			0, rts_codec_dac_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("PDM CLK", RTS_REG_TCON_CFG, RTS_PDM_CLK_EN,
			0, rts_codec_pdm_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("TCON CLK", SND_SOC_NOPM, 0,
			0, rts_codec_tcon_event, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_DAC_E("LOUTL Filter", NULL, SND_SOC_NOPM,
			0, 0, rts_codec_loutl_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LOUTR Filter", NULL, SND_SOC_NOPM,
			0, 0, rts_codec_loutr_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	/* analog widgets */
	SND_SOC_DAPM_DAC_E("Analog DACR Filter", NULL,
			RTS_REG_ADDA_ANA_CFG1, RTS_POW_DACR, 0,
			rts_codec_ana_dacr_event,
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_DAC_E("Analog DACL Filter", NULL,
			RTS_REG_ADDA_ANA_CFG1, RTS_POW_DACL, 0,
			rts_codec_ana_dacl_event,
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MUX("AOUT MUXL", SND_SOC_NOPM, 0, 0,
			&rts_codec_aout_l_mux),
	SND_SOC_DAPM_MUX("AOUT MUXR", SND_SOC_NOPM, 0, 0,
			&rts_codec_aout_r_mux),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("PDM"),
	SND_SOC_DAPM_INPUT("DMIC"),
	SND_SOC_DAPM_INPUT("DMIC2"),

	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),

	SND_SOC_DAPM_INPUT("LINL"),
	SND_SOC_DAPM_INPUT("LINR"),
	SND_SOC_DAPM_INPUT("AMICL"),
	SND_SOC_DAPM_INPUT("AMICR"),
};

static const struct snd_soc_dapm_route rts_soc_codec_dapm_routes[] = {
	/* PDM */
	{"DAC Filter", NULL, "RTSD Playback"},
	{"DAC Filter", NULL, "RTSA Playback"},
	{"DAC Filter", NULL, "DAC Filter CLK"},
	{"PDM", NULL, "DAC Filter"},
	{"PDM", NULL, "PDM CLK"},
	{"PDM CLK", NULL, "TCON CLK"},
	{"DAC Filter CLK", NULL, "TCON CLK"},

	/* DMIC */
	{"RTSD Capture", NULL, "ADC Filter"},
	{"RTSA Capture", NULL, "ADC Filter"},
	{"ADC Filter", NULL, "ADC Filter CLK"},
	{"ADC Filter", NULL, "DMIC"},
	{"DMIC", NULL, "DMIC CLK"},
	{"DMIC CLK", NULL, "TCON CLK"},
	{"ADC Filter CLK", NULL, "TCON CLK"},

	/* DMIC2 */
	{"RTSD Capture", NULL, "ADC2 Filter"},
	{"ADC2 Filter", NULL, "ADC2 Filter CLK"},
	{"ADC2 Filter", NULL, "DMIC2"},
	{"DMIC2", NULL, "DMIC2 CLK"},
	{"DMIC2 CLK", NULL, "TCON CLK"},
	{"ADC2 Filter CLK", NULL, "TCON CLK"},

	/* LOUT */
	{"Analog DACL Filter", NULL, "DAC Filter"},
	{"Analog DACR Filter", NULL, "DAC Filter"},

	{"AOUT MUXL", "DAC_L", "Analog DACL Filter"},
	{"AOUT MUXL", "DAC_R", "Analog DACR Filter"},
	{"AOUT MUXL", "AIN_L", "Analog ADCL Filter"},
	{"AOUT MUXL", "AIN_R", "Analog ADCR Filter"},

	{"AOUT MUXR", "DAC_R", "Analog DACR Filter"},
	{"AOUT MUXR", "DAC_L", "Analog DACL Filter"},
	{"AOUT MUXR", "AIN_R", "Analog ADCR Filter"},
	{"AOUT MUXR", "AIN_L", "Analog ADCL Filter"},

	{"LOUTL Filter", NULL, "AOUT MUXL"},
	{"LOUTR Filter", NULL, "AOUT MUXR"},

	{"LOUTL", NULL, "LOUTL Filter"},
	{"LOUTR", NULL, "LOUTR Filter"},

	/* AMIC & LIN */
	{"ADC ANA CLK", NULL, "TCON CLK"},

	{"ADC Filter", NULL, "Analog ADCL Filter"},
	{"ADC Filter", NULL, "Analog ADCR Filter"},
	{"Analog ADCL Filter", NULL, "ADC ANA CLK"},
	{"Analog ADCR Filter", NULL, "ADC ANA CLK"},

	{"Analog ADCL Filter", NULL, "LINL"},
	{"Analog ADCR Filter", NULL, "LINR"},
	{"Analog ADCL Filter", NULL, "MICBIASL"},
	{"Analog ADCR Filter", NULL, "MICBIASR"},

	{"MICBIASL", NULL, "AMICL"},
	{"MICBIASR", NULL, "AMICR"},
};
/****************************************************************************/

static int rts_codec_dai_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct rts_codec_data *codec_data;

	DBG("rts codec dai startup\n");

	codec_data = snd_soc_component_get_drvdata(component);

	codec_data->substream[substream->stream] = substream;

	rts_codec_dai_enable_clk(codec_data, 0);

	/* adjust lineout current */
	snd_soc_component_update_bits(component, RTS_REG_ADDA_ANA_CFG2,
			0x3 << RTS_CUR_LO, 0x2 << RTS_CUR_LO);

	return 0;
}

static void rts_codec_dai_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);
	struct rts_codec_data *codec_data;

	DBG("rts codec dai shutdown\n");

	codec_data = snd_soc_component_get_drvdata(component);

	rts_codec_dai_disable_clk(codec_data, 0);

	codec_data->substream[substream->stream] = NULL;

	if (dai->id == RTS_CODEC_DIGITAL) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_disable_pin(dapm, "PDM");
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			snd_soc_dapm_disable_pin(dapm, "DMIC");
			if (codec_data->channels == 4)
				snd_soc_dapm_disable_pin(dapm, "DMIC2");
		}
	} else if (dai->id == RTS_CODEC_ANALOG) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (codec_data->channels == 2) {
				snd_soc_dapm_disable_pin(dapm, "LOUTL");
				snd_soc_dapm_disable_pin(dapm, "LOUTR");
				goto out;
			}

			if (codec_data->mono_out_mode ==
						RTS_CODEC_MONO_RIGHT_OUT)
				snd_soc_dapm_disable_pin(dapm, "LOUTR");
			else
				snd_soc_dapm_disable_pin(dapm, "LOUTL");
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (codec_data->channels == 2) {
				snd_soc_dapm_disable_pin(dapm, "AMICL");
				snd_soc_dapm_disable_pin(dapm, "LINL");
				snd_soc_dapm_disable_pin(dapm, "AMICR");
				snd_soc_dapm_disable_pin(dapm, "LINR");
				goto out;
			}

			if (codec_data->mono_in_mode ==
						RTS_CODEC_MONO_RIGHT_IN) {
				snd_soc_dapm_disable_pin(dapm, "AMICR");
				snd_soc_dapm_disable_pin(dapm, "LINR");
			} else {
				snd_soc_dapm_disable_pin(dapm, "AMICL");
				snd_soc_dapm_disable_pin(dapm, "LINL");
			}
		}
	}
out:
	snd_soc_dapm_sync(dapm);
}

static int rts_codec_dai_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);
	struct rts_codec_data *codec_data =
				snd_soc_component_get_drvdata(component);
	u32 reg_val, val;

	DBG("rts codec dai hw params\n");

	codec_data->channels = params_channels(params);
	reg_val = readl(codec_data->addr + RTS_REG_TCON_CFG);
	switch (params_rate(params)) {
	case 8000:
		val = 0x07;
		break;
	case 11025:
		val = 0x0E;
		break;
	case 12000:
		val = 0x06;
		break;
	case 16000:
		val = 0x05;
		break;
	case 22050:
		val = 0x0C;
		break;
	case 24000:
		val = 0x04;
		break;
	case 32000:
		val = 0x03;
		break;
	case 44100:
		val = 0x08;
		break;
	case 48000:
		val = 0x00;
		break;
	case 88200:
		val = 0x09;
		break;
	case 96000:
		val = 0x01;
		break;
	case 176400:
		val = 0x0A;
		break;
	case 192000:
		val = 0x02;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg_val = reg_val & ~((u32) 0xF << RTS_DAC_SAMPLE_RATE);
		reg_val = reg_val | (val << RTS_DAC_SAMPLE_RATE);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		reg_val = reg_val & ~((u32) 0xF << RTS_ADC_SAMPLE_RATE);
		reg_val = reg_val | (val << RTS_ADC_SAMPLE_RATE);
	} else {
		return -EINVAL;
	}

	writel(reg_val, codec_data->addr + RTS_REG_TCON_CFG);

	if (dai->id == RTS_CODEC_DIGITAL) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_enable_pin(dapm, "PDM");
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			snd_soc_dapm_enable_pin(dapm, "DMIC");
			if (codec_data->channels == 4)
				snd_soc_dapm_enable_pin(dapm, "DMIC2");
		}
	} else if (dai->id == RTS_CODEC_ANALOG) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (codec_data->channels == 2) {
				snd_soc_dapm_enable_pin(dapm, "LOUTL");
				snd_soc_dapm_enable_pin(dapm, "LOUTR");
				goto out;
			}

			if (codec_data->mono_out_mode ==
						RTS_CODEC_MONO_RIGHT_OUT)
				snd_soc_dapm_enable_pin(dapm, "LOUTR");
			else
				snd_soc_dapm_enable_pin(dapm, "LOUTL");
		} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (codec_data->channels == 2) {
				snd_soc_dapm_enable_pin(dapm, "AMICL");
				snd_soc_dapm_enable_pin(dapm, "LINL");
				snd_soc_dapm_enable_pin(dapm, "AMICR");
				snd_soc_dapm_enable_pin(dapm, "LINR");
				goto out;
			}

			if (codec_data->mono_in_mode ==
						RTS_CODEC_MONO_RIGHT_IN) {
				snd_soc_dapm_enable_pin(dapm, "AMICR");
				snd_soc_dapm_enable_pin(dapm, "LINR");
			} else {
				snd_soc_dapm_enable_pin(dapm, "AMICL");
				snd_soc_dapm_enable_pin(dapm, "LINL");
			}
		}
	}
out:
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int rts_codec_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int rfs, int dir)
{
	struct rts_codec_data *codec_data;
	struct snd_soc_component *component = dai->component;

	DBG("rts codec set sysclk\n");

	codec_data = snd_soc_component_get_drvdata(component);

	/* set codec_clk freq */
	clk_set_rate(codec_data->codec_clk, rfs);

	return 0;
}

static struct snd_soc_dai_ops rts_codec_dai_ops = {
	.startup = rts_codec_dai_startup,
	.hw_params = rts_codec_dai_hw_params,
	.shutdown = rts_codec_dai_shutdown,
	.set_sysclk = rts_codec_dai_set_sysclk,
};

static struct snd_soc_dai_driver rts_codec_dai[] = {
	{
		.name = "rts-codec-analog",
		.id = RTS_CODEC_ANALOG,
		.playback = {
			.stream_name = "RTS-CODEC Analog Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RTS_CODEC_RATES,
			.formats = RTS_CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "RTS-CODEC Analog Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RTS_CODEC_RATES,
			.formats = RTS_CODEC_FORMATS,
		},
		.ops = &rts_codec_dai_ops,
	},
	{
		.name = "rts-codec-digital",
		.id = RTS_CODEC_DIGITAL,
		.playback = {
			.stream_name = "RTS-CODEC Digital Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RTS_CODEC_RATES,
			.formats = RTS_CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "RTS-CODEC Digital Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = RTS_CODEC_RATES,
			.formats = RTS_CODEC_FORMATS,
		},
		.ops = &rts_codec_dai_ops,
	},
};

static int rts_soc_codec_probe(struct snd_soc_component *component)
{
	u32 reg_val;
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);

	snd_soc_add_component_controls(component, rts_soc_codec_controls,
					ARRAY_SIZE(rts_soc_codec_controls));

	snd_soc_add_component_controls(component,
					rts_soc_dmic2_controls,
					ARRAY_SIZE(rts_soc_dmic2_controls));

	snd_soc_dapm_disable_pin(dapm, "PDM");
	snd_soc_dapm_disable_pin(dapm, "DMIC");
	snd_soc_dapm_disable_pin(dapm, "DMIC2");
	snd_soc_dapm_disable_pin(dapm, "LOUTL");
	snd_soc_dapm_disable_pin(dapm, "LOUTR");
	snd_soc_dapm_disable_pin(dapm, "AMICL");
	snd_soc_dapm_disable_pin(dapm, "AMICR");
	snd_soc_dapm_disable_pin(dapm, "LINL");
	snd_soc_dapm_disable_pin(dapm, "LINR");

	snd_soc_dapm_sync(dapm);

	/* keep power of micbias & micl & micr on when codec probe */
	/* to avoid popping noise which occours when start recording*/
	reg_val = snd_soc_component_read(component, RTS_REG_ADDA_ANA_CFG1);
	snd_soc_component_write(component, RTS_REG_ADDA_ANA_CFG1,
				reg_val |
				((u32)0x1 << RTS_POW_MICBIAS) |
				((u32)0x1 << RTS_POW_MICL) |
				((u32)0x1 << RTS_POW_MICR));

	/* set a-gain as maximum value 69 when codec probe */
	reg_val = snd_soc_component_read(component, RTS_REG_ALC_PGA_CFG4);
	reg_val &= ~(((u32)0x7f << RTS_MICL_GAIN) |
			((u32)0x7f << RTS_MICR_GAIN));
	reg_val |= (((u32)0x45 << RTS_MICL_GAIN) |
			((u32)0x45 << RTS_MICR_GAIN));
	snd_soc_component_write(component, RTS_REG_ALC_PGA_CFG4, reg_val);

	return 0;
}

static void rts_soc_codec_remove(struct snd_soc_component *component)
{
}

static struct snd_soc_component_driver rts_soc_codec_driver = {
	.name = "rts-codec",
	.probe = rts_soc_codec_probe,
	.remove = rts_soc_codec_remove,
	.read = rts_soc_codec_read,
	.write = rts_soc_codec_write,
	.dapm_widgets = rts_soc_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rts_soc_codec_dapm_widgets),
	.dapm_routes = rts_soc_codec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rts_soc_codec_dapm_routes),
};

static void rts_codec_iounmap(struct rts_codec_data *codec_data)
{
	struct platform_device *pdev = codec_data->pdev;
	struct resource *res;

	if (codec_data->addr) {
		iounmap(codec_data->addr);
		codec_data->addr = NULL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
}

static int rts_codec_ioremap(struct rts_codec_data *codec_data)
{
	struct platform_device *pdev = codec_data->pdev;
	struct resource *res;
	int ret = 0;
	unsigned long size;
	u32 base;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("Unable to get CODEC address\n");
		ret = -ENXIO;
		goto out;
	}

	if (!request_mem_region(res->start, resource_size(res),
				"rts-codec")) {
		pr_err("Unable to request mem region\n");
		ret = -EBUSY;
		goto out;
	}

	base = res->start;
	size = res->end - res->start + 1;

	codec_data->addr = ioremap(base, size);
	if (codec_data->addr == NULL) {
		pr_err("failed to ioremap\n");
		ret = -ENXIO;
		goto out;
	}
out:
	if (ret)
		rts_codec_iounmap(codec_data);

	return ret;
}

static const struct of_device_id rts_codec_ids[] = {
	{
		.compatible = "realtek,rts3917-fpga-acodec",
		.data = (void *)(TYPE_RTS3917 | TYPE_FPGA),
	}, {
		.compatible = "realtek,rts3917-acodec",
		.data = (void *)TYPE_RTS3917,
	},
	{ },
};

static void rts_codec_dev_release(struct device *cd)
{
	put_device(cd->parent);
}

static int rts_codec_probe(struct platform_device *pdev)
{
	int ret, result;
	struct rts_codec_data *codec_data;
	struct device_node *node = NULL;

	DBG("rts codec probe\n");

	pdev->dev.init_name = "rts-codec";

	codec_data =
	    devm_kzalloc(&pdev->dev, sizeof(struct rts_codec_data),
			 GFP_KERNEL);
	if (codec_data == NULL) {
		pr_err("Unable to alloc codec data\n");
		return -ENOMEM;
	}
	codec_data->pdev = pdev;
	codec_data->devtype = (u32)of_device_get_match_data(&pdev->dev);

	codec_data->rst = devm_reset_control_get(&pdev->dev, "reset-codec");
	if (!codec_data->rst) {
		pr_err("can't find reset codec control");
		ret = -EINVAL;
		goto io_err;
	}

	/* reset codec */
	reset_control_reset(codec_data->rst);

	ret = rts_codec_ioremap(codec_data);
	if (ret)
		goto io_err;

	codec_data->codec_clk = clk_get(NULL, "codec_ck");
	if (IS_ERR(codec_data->codec_clk)) {
		pr_err("failed to get codec_ck\n");
		ret = -ENOENT;
		goto io_err;
	}

	codec_data->mono_in_mode = RTS_CODEC_MONO_LEFT_IN;
	codec_data->mono_out_mode = RTS_CODEC_MONO_LEFT_OUT;

	node = of_parse_phandle(pdev->dev.of_node, "mono-mode", 0);
	if (node) {
		if (of_property_match_string(node,
					"mono-in", "left-channel") >= 0)
			codec_data->mono_in_mode = RTS_CODEC_MONO_LEFT_IN;
		else if (of_property_match_string(node,
					"mono-in", "right-channel") >= 0)
			codec_data->mono_in_mode = RTS_CODEC_MONO_RIGHT_IN;

		if (of_property_match_string(node,
					"mono-out", "left-channel") >= 0)
			codec_data->mono_out_mode = RTS_CODEC_MONO_LEFT_OUT;
		else if (of_property_match_string(node,
					"mono-out", "right-channel") >= 0)
			codec_data->mono_out_mode = RTS_CODEC_MONO_RIGHT_OUT;

		of_node_put(node);
		node = NULL;
	}

	codec_data->codec_ref = 0;
	mutex_init(&codec_data->codec_mutex);

	ret = devm_snd_soc_register_component(&pdev->dev,
			&rts_soc_codec_driver, rts_codec_dai,
			ARRAY_SIZE(rts_codec_dai));
	if (ret) {
		pr_err("register codec failed\n");
		goto codec_err;
	}

	dev_set_drvdata(&pdev->dev, codec_data);

	/* register "rts_audio_devcodec" */
	cdev_init(&audio_cdev, &rts_audio_fops);
	result = alloc_chrdev_region(&audio_devno, 0, 1, "rts_audio_devcodec");
	if (result < 0) {
		DBG("alloc_chrdev_region failed! result: %d\n", result);
		goto cdev_err;
	}
	result = cdev_add(&audio_cdev, audio_devno, 1);
	if (result < 0) {
		DBG("cdev_add failed! result: %d\n", result);
		goto cdev_err;
	}

	audio_dev.devt = audio_devno;
	dev_set_name(&audio_dev, "rts_audio_devcodec");
	audio_dev.parent = get_device(&pdev->dev);
	audio_dev.release = rts_codec_dev_release;
	ret = device_register(&audio_dev);
	if (ret) {
		pr_err("could not create chrdev node\n");
		goto cdev_err;
	}

	rts_codec_data = codec_data;

	return 0;

cdev_err:
	cdev_del(&audio_cdev);
	device_unregister(&audio_dev);
	unregister_chrdev_region(audio_devno, 1);
codec_err:
	if (codec_data->codec_clk) {
		clk_put(codec_data->codec_clk);
		codec_data->codec_clk = NULL;
	}
io_err:
	if (codec_data->rst)
		reset_control_assert(codec_data->rst);
	rts_codec_iounmap(codec_data);

	devm_kfree(&pdev->dev, codec_data);
	codec_data = NULL;

	return ret;
}

static int rts_codec_remove(struct platform_device *pdev)
{
	struct rts_codec_data *codec_data;

	codec_data = dev_get_drvdata(&pdev->dev);

	if (codec_data->rst)
		reset_control_assert(codec_data->rst);

	if (codec_data->codec_clk) {
		clk_put(codec_data->codec_clk);
		codec_data->codec_clk = NULL;
	}

	rts_codec_iounmap(codec_data);
	devm_kfree(&pdev->dev, codec_data);
	dev_set_drvdata(&pdev->dev, NULL);
	codec_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rts_codec_running(struct rts_codec_data *codec_data)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (codec_data->substream[i] &&
				snd_pcm_running(codec_data->substream[i]))
			return 1;
	}

	return 0;
}

static int rts_codec_suspend(struct device *dev)
{
	struct rts_codec_data *codec_data = dev_get_drvdata(dev);

	DBG("%s\n", __func__);

	if (rts_codec_running(codec_data))
		rts_codec_dai_disable_clk(codec_data, 1);
	reset_control_assert(codec_data->rst);

	return 0;
}

static int rts_codec_resume(struct device *dev)
{
	struct rts_codec_data *codec_data = dev_get_drvdata(dev);

	DBG("%s\n", __func__);

	reset_control_reset(codec_data->rst);
	if (rts_codec_running(codec_data))
		rts_codec_dai_enable_clk(codec_data, 1);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rts_codec_pm, rts_codec_suspend, rts_codec_resume);
#define rts_codec_pm_ops (&rts_codec_pm)
#else
#define rts_codec_pm_ops NULL
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver rts_codec_driver = {
	.driver = {
		.name = "rts-codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rts_codec_ids),
		.pm = rts_codec_pm_ops,
	},
	.probe = rts_codec_probe,
	.remove = rts_codec_remove,
};

static int __init rts_codec_init(void)
{
	DBG("rts codec init\n");

	return platform_driver_register(&rts_codec_driver);
}
module_init(rts_codec_init);

static void __exit rts_codec_exit(void)
{
	platform_driver_unregister(&rts_codec_driver);
}
module_exit(rts_codec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wind_Han <wind_han@realsil.com.cn>");
MODULE_DESCRIPTION("Realtek RTS ALSA soc codec driver");
