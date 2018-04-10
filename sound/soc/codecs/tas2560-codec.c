/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2560-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2560 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2560_CODEC

/* #define DEBUG */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2560.h"
#include "tas2560-core.h"

#define TAS2560_MDELAY 0xFFFFFFFE
#define KCONTROL_CODEC

static unsigned int tas2560_codec_read(struct snd_soc_codec *codec,  unsigned int reg)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;
	int ret;

	ret = pTAS2560->read(pTAS2560, reg, &value);
	if (ret >= 0)
		return value;
	else
		return ret;
}

static int tas2560_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	return pTAS2560->write(pTAS2560, reg, value);
}

static int tas2560_codec_suspend(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2560->codec_lock);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->runtime_suspend(pTAS2560);

	mutex_unlock(&pTAS2560->codec_lock);
	return ret;
}

static int tas2560_codec_resume(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2560->codec_lock);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->runtime_resume(pTAS2560);

	mutex_unlock(&pTAS2560->codec_lock);
	return ret;
}

static int tas2560_AIF_post_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMU");
	break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMD");
	break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget tas2560_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0,
		tas2560_AIF_post_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUT_DRV("ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route tas2560_audio_map[] = {
	{"DAC", NULL, "ASI1"},
	{"ClassD", NULL, "DAC"},
	{"OUT", NULL, "ClassD"},
	{"DAC", NULL, "PLL"},
};

static int tas2560_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static void tas2560_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
}

static int tas2560_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2560->codec_lock);
	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, mute);
	tas2560_enable(pTAS2560, !mute);
	mutex_unlock(&pTAS2560->codec_lock);
	return 0;
}

static int tas2560_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
			unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return ret;
}

static int tas2560_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s, format=0x%x\n", __func__, fmt);

	return ret;
}

static struct snd_soc_dai_ops tas2560_dai_ops = {
	.startup = tas2560_startup,
	.shutdown = tas2560_shutdown,
	.digital_mute = tas2560_mute,
	.hw_params  = tas2560_hw_params,
	.prepare    = tas2560_prepare,
	.set_sysclk = tas2560_set_dai_sysclk,
	.set_fmt    = tas2560_set_dai_fmt,
};

#define TAS2560_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2560_dai_driver[] = {
	{
		.name = "tas2560 ASI1",
		.id = 0,
		.playback = {
			.stream_name    = "ASI1 Playback",
			.channels_min   = 2,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2560_FORMATS,
		},
		.ops = &tas2560_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tas2560_codec_probe(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int tas2560_get_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	pUcontrol->value.integer.value[0] = pTAS2560->mnLoad;

	return 0;
}

static int tas2560_set_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2560->codec_lock);
	pTAS2560->mnLoad = pUcontrol->value.integer.value[0];
	dev_dbg(pCodec->dev, "%s:load = 0x%x\n", __func__,
			pTAS2560->mnLoad);
	tas2560_setLoad(pTAS2560, pTAS2560->mnLoad);
	mutex_unlock(&pTAS2560->codec_lock);
	return 0;
}

static int tas2560_get_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	pUcontrol->value.integer.value[0] = pTAS2560->mnSamplingRate;
	dev_dbg(pCodec->dev, "%s: %d\n", __func__,
			pTAS2560->mnSamplingRate);
	return 0;
}

static int tas2560_set_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int sampleRate = pUcontrol->value.integer.value[0];

	mutex_lock(&pTAS2560->codec_lock);
	dev_dbg(pCodec->dev, "%s: %d\n", __func__, sampleRate);
	tas2560_set_SampleRate(pTAS2560, sampleRate);
	mutex_unlock(&pTAS2560->codec_lock);

	return 0;
}

static int tas2560_power_ctrl_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2560->mbPowerUp;
	dev_dbg(codec->dev, "tas2560_power_ctrl_get = 0x%x\n",
					pTAS2560->mbPowerUp);

	return 0;
}

static int tas2560_power_ctrl_put(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int bPowerUp = pValue->value.integer.value[0];

	mutex_lock(&pTAS2560->codec_lock);
	tas2560_enable(pTAS2560, bPowerUp);
	mutex_unlock(&pTAS2560->codec_lock);

	return 0;
}

static int tas2560_ear_switch_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	ucontrol->value.integer.value[0] = pTAS2560->enablePmicEarPath;
	dev_dbg(pCodec->dev, "%s: ear_switch_get = %d\n", __func__, (int)(pTAS2560->enablePmicEarPath));

	return 0;
}

static int tas2560_ear_switch_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n", __func__, ucontrol->value.integer.value[0]);

	 pTAS2560->enablePmicEarPath = (ucontrol->value.integer.value[0] ? true : false);

	if (!gpio_is_valid(pTAS2560->mnSwitchGPIO)) {
		dev_dbg(codec->dev, "%s: Invalid gpio: %d\n", __func__, pTAS2560->mnSwitchGPIO);
		return false;
	}

	gpio_set_value_cansleep(pTAS2560->mnSwitchGPIO,  pTAS2560->enablePmicEarPath);

	if (gpio_is_valid(pTAS2560->mnSwitchGPIO2))
		gpio_set_value_cansleep(pTAS2560->mnSwitchGPIO2,
			pTAS2560->enablePmicEarPath);

	return 0;
}


static int tas2560_ppg_ctrl_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = tas2560_get_PPG(pTAS2560);
	dev_dbg(codec->dev, "tas2560_ppg_ctrl_get = 0x%x\n",
					pTAS2560->mnPPG);

	return 0;
}

static int tas2560_ppg_ctrl_put(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int mnPPG = pValue->value.integer.value[0];

	mutex_lock(&pTAS2560->codec_lock);
	tas2560_set_PPG(pTAS2560, mnPPG);
	mutex_unlock(&pTAS2560->codec_lock);

	return 0;
}

static const char *load_text[] = {"8_Ohm", "6_Ohm", "4_Ohm"};

static const struct soc_enum load_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(load_text), load_text),
};

static const char *Sampling_Rate_text[] = {"48_khz", "44.1_khz", "16_khz", "8_khz"};

static const struct soc_enum Sampling_Rate_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Sampling_Rate_text), Sampling_Rate_text),
};

static const char * const tas2560_ear_switch_ctrl_text[] = {"DISABLE", "ENABLE"};
static const struct soc_enum tas2560_ear_switch_ctl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_ear_switch_ctrl_text), tas2560_ear_switch_ctrl_text),
};

/*
 * DAC digital volumes. From 0 to 15 dB in 1 dB steps
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, 0, 100, 0);

static const struct snd_kcontrol_new tas2560_snd_controls[] = {
	SOC_SINGLE_TLV("DAC Playback Volume", TAS2560_SPK_CTRL_REG, 0, 0x0f, 0,
			dac_tlv),
	SOC_ENUM_EXT("TAS2560 Boost load", load_enum[0],
			tas2560_get_load, tas2560_set_load),
	SOC_ENUM_EXT("TAS2560 Sampling Rate", Sampling_Rate_enum[0],
			tas2560_get_Sampling_Rate, tas2560_set_Sampling_Rate),
	SOC_SINGLE_EXT("TAS2560 PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
			tas2560_power_ctrl_get, tas2560_power_ctrl_put),
	SOC_ENUM_EXT("TAS2560 EAR Switch", tas2560_ear_switch_ctl_enum[0], tas2560_ear_switch_get, tas2560_ear_switch_set),
	SOC_SINGLE_EXT("TAS2560 PPG", SND_SOC_NOPM, 0, 0x006E, 0,
			tas2560_ppg_ctrl_get, tas2560_ppg_ctrl_put),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2560 = {
	.probe			= tas2560_codec_probe,
	.remove			= tas2560_codec_remove,
	.read			= tas2560_codec_read,
	.write			= tas2560_codec_write,
	.suspend		= tas2560_codec_suspend,
	.resume			= tas2560_codec_resume,
	.controls		= tas2560_snd_controls,
	.num_controls		= ARRAY_SIZE(tas2560_snd_controls),
	.dapm_widgets		= tas2560_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tas2560_dapm_widgets),
	.dapm_routes		= tas2560_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(tas2560_audio_map),
};

int tas2560_register_codec(struct tas2560_priv *pTAS2560)
{
	int nResult = 0;

	dev_info(pTAS2560->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2560->dev,
		&soc_codec_driver_tas2560,
		tas2560_dai_driver, ARRAY_SIZE(tas2560_dai_driver));

	return nResult;
}

int tas2560_deregister_codec(struct tas2560_priv *pTAS2560)
{
	snd_soc_unregister_codec(pTAS2560->dev);

	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif /* CONFIG_TAS2560_CODEC */
