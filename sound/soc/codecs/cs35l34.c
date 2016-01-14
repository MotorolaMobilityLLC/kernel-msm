/*
 * cs35l34.c -- CS35l34 ALSA SoC audio driver
 *
 * Copyright 2015 Cirrus Logic, Inc.
 *
 * Author: Alex Kovacs <alex.kovacs@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/cs35l34.h>

#include "cs35l34.h"

struct  cs35l34_private {
	struct snd_soc_codec *codec;
	struct cs35l34_platform_data pdata;
	struct regmap *regmap;
	void *control_data;
	int mclk_int;
	/* GPIO for !RST */
	struct gpio_desc *gpio_nreset;
};

static const struct reg_default cs35l34_reg[] = {
	{CS35L34_PWRCTL1, 0x01},
	{CS35L34_PWRCTL2, 0x19},
	{CS35L34_PWRCTL3, 0x01},
	{CS35L34_ADSP_CLK_CTL, 0x08},
	{CS35L34_MCLK_CTL, 0x01},
	{CS35L34_AMP_INP_DRV_CTL, 0x01},
	{CS35L34_AMP_DIG_VOL_CTL, 0x12},
	{CS35L34_AMP_DIG_VOL, 0x00},
	{CS35L34_AMP_ANLG_GAIN_CTL, 0x09},
	{CS35L34_PROTECT_CTL, 0x06},
	{CS35L34_AMP_KEEP_ALIVE_CTL, 0x04},
	{CS35L34_BST_CVTR_V_CTL, 0x00},
	{CS35L34_BST_PEAK_I, 0x10},
	{CS35L34_BST_RAMP_CTL, 0x06},
	{CS35L34_BST_CONV_COEF_1, 0x0F},
	{CS35L34_BST_CONV_COEF_2, 0x0C},
	{CS35L34_BST_CONV_SLOPE_COMP, 0x4E},
	{CS35L34_BST_CONV_SW_FREQ, 0x40},
	{CS35L34_CLASS_H_CTL, 0x0B},
	{CS35L34_CLASS_H_HEADRM_CTL, 0x0B},
	{CS35L34_CLASS_H_RELEASE_RATE, 0x08},
	{CS35L34_CLASS_H_FET_DRIVE_CTL, 0x41},
	{CS35L34_CLASS_H_VP_CH_CTL, 0xC5},
	{CS35L34_CLASS_H_STATUS, 0x05},
	{CS35L34_VPBR_CTL, 0x0A},
	{CS35L34_VPBR_VOL_CTL, 0x91},
	{CS35L34_VPBR_TIMING_CTL, 0x6A},
	{CS35L34_PRED_MAX_ATTEN_SPK_LOAD, 0x96},
	{CS35L34_PRED_BROWNOUT_THRESH, 0x1C},
	{CS35L34_PRED_BROWNOUT_VOL_CTL, 0x00},
	{CS35L34_PRED_BROWNOUT_RATE_CTL, 0x10},
	{CS35L34_PRED_WAIT_CTL, 0x10},
	{CS35L34_PRED_ZVP_INIT_IMP_CTL, 0x08},
	{CS35L34_PRED_MAN_SAFE_VPI_CTL, 0x80},
	{CS35L34_VPBR_ATTEN_STATUS, 0x00},
	{CS35L34_PRED_BRWNOUT_ATT_STATUS, 0x00},
	{CS35L34_SPKR_MON_CTL, 0xC6},
	{CS35L34_ADSP_I2S_CTL, 0x00},
	{CS35L34_ADSP_TDM_CTL, 0x00},
	{CS35L34_TDM_TX_CTL_1_VMON, 0x00},
	{CS35L34_TDM_TX_CTL_2_IMON, 0x04},
	{CS35L34_TDM_TX_CTL_3_VPMON, 0x03},
	{CS35L34_TDM_TX_CTL_4_VBSTMON, 0x07},
	{CS35L34_TDM_TX_CTL_5_FLAG1, 0x08},
	{CS35L34_TDM_TX_CTL_6_FLAG2, 0x09},
	{CS35L34_TDM_TX_CTL_7_LBST, 0x0A},
	{CS35L34_TDM_TX_CTL_8_NSNS, 0x0B},
	{CS35L34_TDM_TX_SLOT_EN_1, 0x00},
	{CS35L34_TDM_TX_SLOT_EN_2, 0x00},
	{CS35L34_TDM_TX_SLOT_EN_3, 0x00},
	{CS35L34_TDM_TX_SLOT_EN_4, 0x00},
	{CS35L34_TDM_RX_CTL_1_AUDIN, 0x40},
	{CS35L34_TDM_RX_CTL_2_SPLY, 0x03},
	{CS35L34_TDM_RX_CTL_3_ALIVE, 0x04},
	{CS35L34_MULT_DEV_SYNCH1, 0x00},
	{CS35L34_MULT_DEV_SYNCH2, 0x80},
	{CS35L34_PROT_RELEASE_CTL, 0x00},
	{CS35L34_DIAG_MODE_REG_LOCK, 0x00},
	{CS35L34_DIAG_MODE_CTL_1, 0x00},
	{CS35L34_DIAG_MODE_CTL_2, 0x00},
	{CS35L34_INT_MASK_1, 0xFF},
	{CS35L34_INT_MASK_2, 0xFF},
	{CS35L34_INT_MASK_3, 0xFF},
	{CS35L34_INT_MASK_4, 0xFF},
	{CS35L34_INT_STATUS_1, 0x30},
	{CS35L34_INT_STATUS_2, 0x05},
	{CS35L34_INT_STATUS_3, 0x00},
	{CS35L34_INT_STATUS_4, 0x00},
	{CS35L34_OTP_TRIM_STATUS, 0x00},
	{CS35L34_PAGE_UNLOCK, 0x00},
};

static bool cs35l34_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L34_DEVID_AB:
	case CS35L34_DEVID_CD:
	case CS35L34_DEVID_E:
	case CS35L34_FAB_ID:
	case CS35L34_REV_ID:
	case CS35L34_INT_STATUS_1:
	case CS35L34_INT_STATUS_2:
	case CS35L34_INT_STATUS_3:
	case CS35L34_INT_STATUS_4:
		return true;
	default:
		return false;
	}
}

static bool cs35l34_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case	CS35L34_CHIP_ID:
	case	CS35L34_DEVID_AB:
	case	CS35L34_DEVID_CD:
	case	CS35L34_DEVID_E:
	case	CS35L34_FAB_ID:
	case	CS35L34_REV_ID:
	case	CS35L34_PWRCTL1:
	case	CS35L34_PWRCTL2:
	case	CS35L34_PWRCTL3:
	case	CS35L34_ADSP_CLK_CTL:
	case	CS35L34_MCLK_CTL:
	case	CS35L34_AMP_INP_DRV_CTL:
	case	CS35L34_AMP_DIG_VOL_CTL:
	case	CS35L34_AMP_DIG_VOL:
	case	CS35L34_AMP_ANLG_GAIN_CTL:
	case	CS35L34_PROTECT_CTL:
	case	CS35L34_AMP_KEEP_ALIVE_CTL:
	case	CS35L34_BST_CVTR_V_CTL:
	case	CS35L34_BST_PEAK_I:
	case	CS35L34_BST_RAMP_CTL:
	case	CS35L34_BST_CONV_COEF_1:
	case	CS35L34_BST_CONV_COEF_2:
	case	CS35L34_BST_CONV_SLOPE_COMP:
	case	CS35L34_BST_CONV_SW_FREQ:
	case	CS35L34_CLASS_H_CTL:
	case	CS35L34_CLASS_H_HEADRM_CTL:
	case	CS35L34_CLASS_H_RELEASE_RATE:
	case	CS35L34_CLASS_H_FET_DRIVE_CTL:
	case	CS35L34_CLASS_H_VP_CH_CTL:
	case	CS35L34_CLASS_H_STATUS:
	case	CS35L34_VPBR_CTL:
	case	CS35L34_VPBR_VOL_CTL:
	case	CS35L34_VPBR_TIMING_CTL:
	case	CS35L34_PRED_MAX_ATTEN_SPK_LOAD:
	case	CS35L34_PRED_BROWNOUT_THRESH:
	case	CS35L34_PRED_BROWNOUT_VOL_CTL:
	case	CS35L34_PRED_BROWNOUT_RATE_CTL:
	case	CS35L34_PRED_WAIT_CTL:
	case	CS35L34_PRED_ZVP_INIT_IMP_CTL:
	case	CS35L34_PRED_MAN_SAFE_VPI_CTL:
	case	CS35L34_VPBR_ATTEN_STATUS:
	case	CS35L34_PRED_BRWNOUT_ATT_STATUS:
	case	CS35L34_SPKR_MON_CTL:
	case	CS35L34_ADSP_I2S_CTL:
	case	CS35L34_ADSP_TDM_CTL:
	case	CS35L34_TDM_TX_CTL_1_VMON:
	case	CS35L34_TDM_TX_CTL_2_IMON:
	case	CS35L34_TDM_TX_CTL_3_VPMON:
	case	CS35L34_TDM_TX_CTL_4_VBSTMON:
	case	CS35L34_TDM_TX_CTL_5_FLAG1:
	case	CS35L34_TDM_TX_CTL_6_FLAG2:
	case	CS35L34_TDM_TX_CTL_7_LBST:
	case	CS35L34_TDM_TX_CTL_8_NSNS:
	case	CS35L34_TDM_TX_SLOT_EN_1:
	case	CS35L34_TDM_TX_SLOT_EN_2:
	case	CS35L34_TDM_TX_SLOT_EN_3:
	case	CS35L34_TDM_TX_SLOT_EN_4:
	case	CS35L34_TDM_RX_CTL_1_AUDIN:
	case	CS35L34_TDM_RX_CTL_2_SPLY:
	case	CS35L34_TDM_RX_CTL_3_ALIVE:
	case	CS35L34_MULT_DEV_SYNCH1:
	case	CS35L34_MULT_DEV_SYNCH2:
	case	CS35L34_PROT_RELEASE_CTL:
	case	CS35L34_DIAG_MODE_REG_LOCK:
	case	CS35L34_DIAG_MODE_CTL_1:
	case	CS35L34_DIAG_MODE_CTL_2:
	case	CS35L34_INT_MASK_1:
	case	CS35L34_INT_MASK_2:
	case	CS35L34_INT_MASK_3:
	case	CS35L34_INT_MASK_4:
	case	CS35L34_INT_STATUS_1:
	case	CS35L34_INT_STATUS_2:
	case	CS35L34_INT_STATUS_3:
	case	CS35L34_INT_STATUS_4:
	case	CS35L34_VA_INDEPEN_COEFF_1:
	case	CS35L34_VA_INDEPEN_COEFF_2:
	case	CS35L34_OTP_TRIM_STATUS:
	case	CS35L34_PAGE_UNLOCK:
		return true;
	default:
		return false;
	}
}

static bool cs35l34_precious_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L34_INT_STATUS_1:
	case CS35L34_INT_STATUS_2:
	case CS35L34_INT_STATUS_3:
	case CS35L34_INT_STATUS_4:
		return true;
	default:
		return false;
	}
}

static int cs35l34_sdin_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs35l34_private *priv = snd_soc_codec_get_drvdata(codec);
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = regmap_update_bits(priv->regmap, CS35L34_PWRCTL1,
			 PDN_ALL, 0);
		if (ret < 0) {
			dev_err(codec->dev, "Cannot set Power bits %d\n", ret);
			return ret;
		}
		usleep_range(5000, 5000);
	break;
	case SND_SOC_DAPM_POST_PMD:
		ret = regmap_update_bits(priv->regmap, CS35L34_PWRCTL1,
			 PDN_ALL, PDN_ALL);
	break;
	default:
		pr_err("Invalid event = 0x%x\n", event);
	}
	return 0;
}

static int cs35l34_main_amp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs35l34_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(priv->regmap, CS35L34_BST_CVTR_V_CTL,
			CS35L34_BST_CTL_MASK,
			0x30);
		usleep_range(5000, 5000);
		regmap_update_bits(priv->regmap, CS35L34_PROTECT_CTL,
			AMP_MUTE,
			0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(priv->regmap, CS35L34_BST_CVTR_V_CTL,
			CS35L34_BST_CTL_MASK,
			0x00);
		regmap_update_bits(priv->regmap, CS35L34_PROTECT_CTL,
			AMP_MUTE,
			AMP_MUTE);
		usleep_range(5000, 5000);
		break;
	default:
		pr_err("Invalid event = 0x%x\n", event);
	}
	return 0;
}

static DECLARE_TLV_DB_SCALE(dig_vol_tlv, -10200, 50, 0);

static const int gain_ranges[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB,
			0xC, 0xD, 0xE, 0xF};

static const char * const gain_labels[] = {"3dB", "4dB", "5dB", "6dB", "7dB",
			"8dB", "9dB", "10dB", "11dB", "12dB", "13dB", "14dB",
			"15dB", "16dB", "17dB", "18dB"};

static const struct soc_enum amp_gain = SOC_VALUE_ENUM_SINGLE(
				CS35L34_AMP_ANLG_GAIN_CTL,
				0, 0xF, ARRAY_SIZE(gain_labels),
				gain_labels, gain_ranges);

static const struct snd_kcontrol_new cs35l34_snd_controls[] = {
	SOC_SINGLE_SX_TLV("Digital Volume", CS35L34_AMP_DIG_VOL,
				0, 0x34, 0x18, dig_vol_tlv),
	SOC_ENUM("AMP Gain", amp_gain),
};

static const struct snd_soc_dapm_widget cs35l34_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("SDIN", NULL, 0, CS35L34_PWRCTL3,
					1, 1, cs35l34_sdin_event,
					SND_SOC_DAPM_PRE_PMU |
					SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT("SDOUT", NULL, 0, CS35L34_PWRCTL3, 2, 1),

	SND_SOC_DAPM_OUTPUT("SPK"),

	SND_SOC_DAPM_INPUT("VP"),
	SND_SOC_DAPM_INPUT("VPST"),
	SND_SOC_DAPM_INPUT("ISENSE"),
	SND_SOC_DAPM_INPUT("VSENSE"),

	SND_SOC_DAPM_ADC("VMON ADC", NULL, CS35L34_PWRCTL2, 7, 1),
	SND_SOC_DAPM_ADC("IMON ADC", NULL, CS35L34_PWRCTL2, 6, 1),
	SND_SOC_DAPM_ADC("VPMON ADC", NULL, CS35L34_PWRCTL3, 3, 1),
	SND_SOC_DAPM_ADC("VBSTMON ADC", NULL, CS35L34_PWRCTL3, 4, 1),
	SND_SOC_DAPM_ADC("CLASS H", NULL, CS35L34_PWRCTL2, 5, 1),
	SND_SOC_DAPM_ADC("BOOST", NULL, CS35L34_PWRCTL2, 2, 1),

	SND_SOC_DAPM_OUT_DRV_E("Main AMP", CS35L34_PWRCTL2, 0, 1, NULL, 0,
		cs35l34_main_amp_event, SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route cs35l34_audio_map[] = {
	{"SDIN", NULL, "AMP Playback"},
	{"BOOST", NULL, "SDIN"},
	{"CLASS H", NULL, "BOOST"},
	{"Main AMP", NULL, "CLASS H"},
	{"SPK", NULL, "Main AMP"},

	{"VPMON ADC", NULL, "CLASS H"},
	{"VBSTMON ADC", NULL, "CLASS H"},
	{"SPK", NULL, "VPMON ADC"},
	{"SPK", NULL, "VBSTMON ADC"},

	{"IMON ADC", NULL, "ISENSE"},
	{"VMON ADC", NULL, "VSENSE"},
	{"SDOUT", NULL, "IMON ADC"},
	{"SDOUT", NULL, "VMON ADC"},
	{"AMP Capture", NULL, "SDOUT"},
};

struct cs35l34_mclk_div {
	int mclk;
	int srate;
	u8 adsp_rate;
	u8 int_fs_ratio;
};

static struct cs35l34_mclk_div cs35l34_mclk_coeffs[] = {

	/* MCLK, Sample Rate, adsp_rate, int_fs_ratio */

	{5644800,  8000, 0x0, 0},
	{5644800, 11025, 0x1, 0},
	{5644800, 12000, 0x2, 0},
	{5644800, 16000, 0x3, 0},
	{5644800, 22050, 0x4, 0},
	{5644800, 24000, 0x5, 0},
	{5644800, 32000, 0x6, 0},
	{5644800, 44100, 0x7, 0},
	{5644800, 48000, 0x8, 0},

	{6000000,  8000, 0x0, 0},
	{6000000, 11025, 0x1, 0},
	{6000000, 12000, 0x2, 0},
	{6000000, 16000, 0x3, 0},
	{6000000, 22050, 0x4, 0},
	{6000000, 24000, 0x5, 0},
	{6000000, 32000, 0x6, 0},
	{6000000, 44100, 0x7, 0},
	{6000000, 48000, 0x8, 0},

	{6144000,  8000, 0x0, 0},
	{6144000, 11025, 0x1, 0},
	{6144000, 12000, 0x2, 0},
	{6144000, 16000, 0x3, 0},
	{6144000, 22050, 0x4, 0},
	{6144000, 24000, 0x5, 0},
	{6144000, 32000, 0x6, 0},
	{6144000, 44100, 0x7, 0},
	{6144000, 48000, 0x8, 0},
};

static int cs35l34_get_mclk_coeff(int mclk, int srate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l34_mclk_coeffs); i++) {
		if (cs35l34_mclk_coeffs[i].mclk == mclk &&
			cs35l34_mclk_coeffs[i].srate == srate)
			return i;
	}
	return -EINVAL;
}

static int cs35l34_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs35l34_private *priv = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		regmap_update_bits(priv->regmap, CS35L34_ADSP_CLK_CTL,
				    0x80, 0x80);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		regmap_update_bits(priv->regmap, CS35L34_ADSP_CLK_CTL,
				    0x80, 0x00);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int cs35l34_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l34_private *priv = snd_soc_codec_get_drvdata(codec);
	int srate = params_rate(params);
	unsigned int clk_reg;
	int ret;

	int coeff = cs35l34_get_mclk_coeff(priv->mclk_int, srate);

	if (coeff < 0)
		return coeff;

	ret = regmap_read(priv->regmap, CS35L34_ADSP_CLK_CTL, &clk_reg);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to read clock state %d\n", ret);
		return ret;
	}
	clk_reg &= 0xF0;
	clk_reg |= (cs35l34_mclk_coeffs[coeff].int_fs_ratio |
		cs35l34_mclk_coeffs[coeff].adsp_rate);
	ret = regmap_write(priv->regmap, CS35L34_ADSP_CLK_CTL, clk_reg);
	if (ret != 0)
		dev_err(codec->dev, "Failed to set clock state %d\n", ret);

	return ret;
}

static int cs35l34_set_tristate(struct snd_soc_dai *dai, int tristate)
{

	struct snd_soc_codec *codec = dai->codec;

	if (tristate)
		snd_soc_update_bits(codec, CS35L34_PWRCTL3,
			PDN_SDOUT, PDN_SDOUT);
	else
		snd_soc_update_bits(codec, CS35L34_PWRCTL3,
			PDN_SDOUT, 0);
	return 0;
}

static int cs35l34_codec_set_sysclk(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l34_private *cs35l34 = snd_soc_codec_get_drvdata(codec);

	switch (freq) {
	case CS35L34_MCLK_5644:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0x0F, 0x00);
		cs35l34->mclk_int = freq;
	break;
	case CS35L34_MCLK_6:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0x0F, 0x01);
		cs35l34->mclk_int = freq;
	break;
	case CS35L34_MCLK_6144:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0x0F, 0x02);
		cs35l34->mclk_int = freq;
	break;
	case CS35L34_MCLK_11289:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0xFF, 0x10);
		cs35l34->mclk_int = freq/2;
	break;
	case CS35L34_MCLK_12:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0xFF, 0x11);
		cs35l34->mclk_int = freq/2;
	break;
	case CS35L34_MCLK_12288:
		snd_soc_update_bits(codec, CS35L34_MCLK_CTL,
				0xFF, 0x12);
		cs35l34->mclk_int = freq/2;
	break;
	default:
		cs35l34->mclk_int = 0;
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops cs35l34_ops = {
	.set_tristate = cs35l34_set_tristate,
	.set_fmt = cs35l34_set_dai_fmt,
	.hw_params = cs35l34_pcm_hw_params,
	.set_sysclk = cs35l34_codec_set_sysclk,
};

static struct snd_soc_dai_driver cs35l34_dai = {
		.name = "cs35l34",
		.id = 0,
		.playback = {
			.stream_name = "AMP Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = CS35L34_RATES,
			.formats = CS35L34_FORMATS,
		},
		.capture = {
			.stream_name = "AMP Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = CS35L34_RATES,
			.formats = CS35L34_FORMATS,
		},
		.ops = &cs35l34_ops,
		.symmetric_rates = 1,
};

static int cs35l34_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct cs35l34_private *cs35l34 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg;

	codec->control_data = cs35l34->regmap;

	regmap_read(cs35l34->regmap, CS35L34_PROTECT_CTL, &reg);
	reg &= ~(1 << 2);
	reg |= 1 << 3;
	regmap_write(cs35l34->regmap, CS35L34_PROTECT_CTL, reg);

	/* Set Power control registers 2 and 3 to have everyting
	 * powered down at initialization
	*/
	regmap_write(cs35l34->regmap, CS35L34_PWRCTL2, 0xFD);
	regmap_write(cs35l34->regmap, CS35L34_PWRCTL3, 0x1F);

	/* Set mute bit at startup */
	regmap_update_bits(cs35l34->regmap, CS35L34_PROTECT_CTL,
		AMP_MUTE,
		AMP_MUTE);

	/* Set Platform Data */
	if (cs35l34->pdata.boost_ctl)
		regmap_update_bits(cs35l34->regmap, CS35L34_BST_CVTR_V_CTL,
			CS35L34_BST_CTL_MASK,
			cs35l34->pdata.boost_ctl);

	snd_soc_dapm_ignore_suspend(&codec->dapm, "SDIN");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SDOUT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "VP");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "VPST");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "ISENSE");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "VSENSE");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Main AMP");

	if (cs35l34->pdata.gain_zc)
		regmap_update_bits(cs35l34->regmap, CS35L34_PROTECT_CTL,
			AMP_GAIN_ZC_MASK,
			cs35l34->pdata.gain_zc << AMP_GAIN_ZC_SHIFT);

	return ret;
}


static struct snd_soc_codec_driver soc_codec_dev_cs35l34 = {
	.probe = cs35l34_probe,

	.dapm_widgets = cs35l34_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l34_dapm_widgets),
	.dapm_routes = cs35l34_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs35l34_audio_map),

	.controls = cs35l34_snd_controls,
	.num_controls = ARRAY_SIZE(cs35l34_snd_controls),
};

static struct regmap_config cs35l34_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS35L34_MAX_REGISTER,
	.reg_defaults = cs35l34_reg,
	.num_reg_defaults = ARRAY_SIZE(cs35l34_reg),
	.volatile_reg = cs35l34_volatile_register,
	.readable_reg = cs35l34_readable_register,
	.precious_reg = cs35l34_precious_register,
	.cache_type = REGCACHE_RBTREE,
};

static int cs35l34_handle_of_data(struct i2c_client *i2c_client,
				struct cs35l34_platform_data *pdata)
{
	struct device_node *np = i2c_client->dev.of_node;
	unsigned int val;

	of_property_read_u32(np, "cirrus,boost-manager", &val);
	switch (val) {
		pdata->boost_mng = val;
		break;
	default:
		dev_err(&i2c_client->dev,
			"Wrong cirrus,boost-manager DT value %d\n", val);
	}

	of_property_read_u32(np, "cirrus,sdout-datacfg", &val);
	switch (val) {
		pdata->sdout_datacfg = val;
		break;
	default:
		dev_err(&i2c_client->dev,
			"Wrong cirrus,sdout-datacfg DT value %d\n", val);
	}

	if (of_property_read_u32(np, "cirrus,boost-ctl", &val) >= 0)
		pdata->boost_ctl = val;
	if (of_property_read_u32(np, "cirrus,gain-zc", &val) >= 0)
		pdata->gain_zc = val;
	if (of_property_read_u32(np, "cirrus,pred-brownout", &val) >= 0)
		pdata->pred_brownout = val;
	if (of_property_read_u32(np, "cirrus,vpbr-thld1", &val) >= 0)
		pdata->vpbr_thld1 = val & 0x0F;
	if (of_property_read_u32(np, "cirrus,vpbr-vol", &val) >= 0)
		pdata->vpbr_vol = val & 0xF0;
	if (of_property_read_u32(np, "cirrus,vpbr-mute", &val) > 0)
		pdata->vpbr_mute = 0x08;
	if (of_property_read_u32(np, "cirrus,vpbr-wait", &val) >= 0)
		pdata->vpbr_wait = val;
	if (of_property_read_u32(np, "cirrus,vpbr-rel", &val) >= 0)
		pdata->vpbr_rel = val;
	if (of_property_read_u32(np, "cirrus,vpbr-atk", &val) >= 0)
		pdata->vpbr_atk = val;
	if (of_property_read_u32(np, "cirrus,vpbr-att", &val) >= 0)
		pdata->vpbr_att = val;
	if (of_property_read_u32(np, "cirrus,vpbr-preload", &val) >= 0)
		pdata->vpbr_preload = val;
	if (of_property_read_u32(np, "cirrus,vpbr-thld", &val) >= 0)
		pdata->vpbr_rthld = val;
	if (of_property_read_u32(np, "cirrus,vpbr-relvol", &val) >= 0)
		pdata->vpbr_relvol = val;
	if (of_property_read_u32(np, "cirrus,vpbr-atkvol", &val) >= 0)
		pdata->vpbr_atkvol = val;
	if (of_property_read_u32(np, "cirrus,vpbr-relrate", &val) >= 0)
		pdata->vpbr_relrate = val;
	if (of_property_read_u32(np, "cirrus,vpbr-atkrate", &val) >= 0)
		pdata->vpbr_atkrate = val;
	if (of_property_read_u32(np, "cirrus,vpbr-predwait", &val) >= 0)
		pdata->vpbr_predwait = val;
	if (of_property_read_u32(np, "cirrus,vpbr-predsafe", &val) >= 0)
		pdata->vpbr_predsafe = val;
	if (of_property_read_u32(np, "cirrus,vpbr-predzvp", &val) >= 0)
		pdata->vpbr_predzvp = val;
	if (of_property_read_u32(np, "cirrus,vpbr-safevpi", &val) >= 0)
		pdata->vpbr_safevpi = val;
	if (of_property_read_u32(np, "cirrus,imon-pol", &val) >= 0)
		pdata->imon_pol = val;
	if (of_property_read_u32(np, "cirrus,vmon-pol", &val) >= 0)
		pdata->vmon_pol = val;
	if (of_property_read_u32(np, "cirrus,i2s-sdinloc", &val) >= 0)
		pdata->i2s_sdinloc = val;
	if (of_property_read_u32(np, "cirrus,i2s-txout", &val) >= 0)
		pdata->i2s_txout = val;
	if (of_property_read_u32(np, "cirrus,react-brownout", &val) >= 0)
		pdata->react_brownout = val;
	if (of_property_read_u32(np, "cirrus,sdout-share", &val) >= 0)
		pdata->sdout_share = val;
	if (of_property_read_u32(np, "cirrus,boost-peak", &val) >= 0)
		pdata->boost_peak = val;
	if (of_property_read_u32(np, "cirrus,boost-limit", &val) >= 0)
		pdata->boost_limit = val;

	return 0;
}

static int cs35l34_i2c_probe(struct i2c_client *i2c_client,
			      const struct i2c_device_id *id)
{
	struct cs35l34_private *cs35l34;
	struct cs35l34_platform_data *pdata =
		dev_get_platdata(&i2c_client->dev);
	/*int i;*/
	int ret;
	unsigned int devid = 0;
	unsigned int reg;

	cs35l34 = devm_kzalloc(&i2c_client->dev,
			       sizeof(struct cs35l34_private),
			       GFP_KERNEL);
	if (!cs35l34) {
		dev_err(&i2c_client->dev, "could not allocate codec\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c_client, cs35l34);
	cs35l34->regmap = devm_regmap_init_i2c(i2c_client, &cs35l34_regmap);
	if (IS_ERR(cs35l34->regmap)) {
		ret = PTR_ERR(cs35l34->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		goto err;
	}

	if (pdata) {
		cs35l34->pdata = *pdata;
	} else {
		pdata = devm_kzalloc(&i2c_client->dev,
				     sizeof(struct cs35l34_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c_client->dev,
				"could not allocate pdata\n");
			return -ENOMEM;
		}
		if (i2c_client->dev.of_node) {
			ret = cs35l34_handle_of_data(i2c_client, pdata);
			if (ret != 0)
				return ret;

		}
		cs35l34->pdata = *pdata;
	}
	/* We could issue !RST or skip it based on AMP topology */
	/* Reset the Device */
	cs35l34->pdata.gpio_nreset = of_get_named_gpio(i2c_client->dev.of_node,
			"cirrus,reset", 0);
	if (cs35l34->pdata.gpio_nreset < 0)
		cs35l34->pdata.gpio_nreset = -1;

	if (gpio_is_valid(cs35l34->pdata.gpio_nreset))
		gpio_set_value_cansleep(cs35l34->pdata.gpio_nreset, 1);

	/* initialize codec */
	ret = regmap_read(cs35l34->regmap, CS35L34_DEVID_AB, &reg);

	devid = (reg & 0xFF) << 12;
	ret = regmap_read(cs35l34->regmap, CS35L34_DEVID_CD, &reg);
	devid |= (reg & 0xFF) << 4;
	ret = regmap_read(cs35l34->regmap, CS35L34_DEVID_E, &reg);
	devid |= (reg & 0xF0) >> 4;

	if (devid != CS35L34_CHIP_ID && devid != CS35L34_CHIP_ID_A1) {
		dev_err(&i2c_client->dev,
			"CS35l34 Device ID (%X).\n",
			devid);
		ret = -ENODEV;
		goto err;
	}

	ret = regmap_read(cs35l34->regmap, CS35L34_REV_ID, &reg);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "Get Revision ID failed\n");
		goto err;
	}

	dev_info(&i2c_client->dev,
		"Cirrus Logic CS35l34 (%x), Revision: %02X\n",
		devid, ret & 0xFF);

	/* Predictive and reactive brownout flags,inverted 0 is on 1 is off */
	if (cs35l34->pdata.pred_brownout > 0) {
		regmap_update_bits(cs35l34->regmap, CS35L34_PWRCTL2,
				1 << 3, 1 << 3);
	}
	if (cs35l34->pdata.vpbr_thld1 > 0 &&
				cs35l34->pdata.vpbr_thld1 <= 0x18) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_CTL,
				0x1F, cs35l34->pdata.vpbr_thld1);
	}
	if (cs35l34->pdata.vpbr_vol > 0 && cs35l34->pdata.vpbr_vol <= 0xF0) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_VOL_CTL,
				0xF0, cs35l34->pdata.vpbr_vol);
	}
	if (cs35l34->pdata.vpbr_wait > 0 && cs35l34->pdata.vpbr_mute == 0x08) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_VOL_CTL,
				0x08, cs35l34->pdata.vpbr_mute);
	}
	if (cs35l34->pdata.vpbr_wait >= 0
	  && cs35l34->pdata.vpbr_wait <= 0xC0) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_TIMING_CTL,
				0xC0, cs35l34->pdata.vpbr_wait);
	}
	if (cs35l34->pdata.vpbr_rel >= 0 && cs35l34->pdata.vpbr_rel <= 0x38) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_TIMING_CTL,
				0x38, cs35l34->pdata.vpbr_rel);
	}
	if (cs35l34->pdata.vpbr_atk >= 0 && cs35l34->pdata.vpbr_atk <= 0x06) {
		regmap_update_bits(cs35l34->regmap, CS35L34_VPBR_TIMING_CTL,
				0x07, cs35l34->pdata.vpbr_atk);
	}
	if (cs35l34->pdata.vpbr_att >= 0 && cs35l34->pdata.vpbr_att <= 0xF0) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_MAX_ATTEN_SPK_LOAD,
				0xF0, cs35l34->pdata.vpbr_att);
	}
	if (cs35l34->pdata.vpbr_preload >= 0 &&
				cs35l34->pdata.vpbr_preload <= 0x06) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_MAX_ATTEN_SPK_LOAD,
				0x07, cs35l34->pdata.vpbr_preload);
	}
	if (cs35l34->pdata.vpbr_rthld >= 0 &&
				cs35l34->pdata.vpbr_rthld <= 0x3F) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_BROWNOUT_THRESH,
				0x3F, cs35l34->pdata.vpbr_rthld);
	}
	if (cs35l34->pdata.vpbr_relvol >= 0 &&
				cs35l34->pdata.vpbr_relvol <= 0x40) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_BROWNOUT_VOL_CTL,
				0x70, cs35l34->pdata.vpbr_relvol);
	}
	if (cs35l34->pdata.vpbr_atkvol >= 0 &&
				cs35l34->pdata.vpbr_atkvol <= 0x04) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_BROWNOUT_VOL_CTL,
				0x07, cs35l34->pdata.vpbr_atkvol);
	}
	if (cs35l34->pdata.vpbr_relrate >= 0 &&
				cs35l34->pdata.vpbr_relrate <= 0xF0) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_BROWNOUT_RATE_CTL,
				0xF0, cs35l34->pdata.vpbr_relrate);
	}
	if (cs35l34->pdata.vpbr_atkrate >= 0 &&
				cs35l34->pdata.vpbr_atkrate <= 0x06) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_BROWNOUT_RATE_CTL,
				0x07, cs35l34->pdata.vpbr_atkrate);
	}
	if (cs35l34->pdata.vpbr_predwait >= 0 &&
				cs35l34->pdata.vpbr_predwait <= 0xB0) {
		regmap_update_bits(cs35l34->regmap, CS35L34_PRED_WAIT_CTL,
				0xF0, cs35l34->pdata.vpbr_predwait);
	}
	if (cs35l34->pdata.vpbr_predsafe >= 0 &&
				cs35l34->pdata.vpbr_predsafe == 0x80) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_ZVP_INIT_IMP_CTL,
				0x80, cs35l34->pdata.vpbr_predsafe);
	}
	if (cs35l34->pdata.vpbr_predzvp >= 0 &&
				cs35l34->pdata.vpbr_predzvp <= 0x1F) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_ZVP_INIT_IMP_CTL,
				0x1F, cs35l34->pdata.vpbr_predzvp);
	}
	if (cs35l34->pdata.vpbr_safevpi >= 0 &&
				cs35l34->pdata.vpbr_safevpi <= 0xD0) {
		regmap_update_bits(cs35l34->regmap,
				CS35L34_PRED_MAN_SAFE_VPI_CTL,
				0xD0, cs35l34->pdata.vpbr_safevpi);
	}
	if (cs35l34->pdata.imon_pol >= 0 && cs35l34->pdata.imon_pol == 0x80) {
		regmap_update_bits(cs35l34->regmap, CS35L34_SPKR_MON_CTL,
				0x80, cs35l34->pdata.imon_pol);
	}
	if (cs35l34->pdata.vmon_pol >= 0 && cs35l34->pdata.vmon_pol == 0x40) {
		regmap_update_bits(cs35l34->regmap, CS35L34_SPKR_MON_CTL,
				0x40, cs35l34->pdata.vmon_pol);
	}
	if (cs35l34->pdata.i2s_sdinloc >= 0 &&
		cs35l34->pdata.i2s_sdinloc <= 0x0C) {
		regmap_update_bits(cs35l34->regmap, CS35L34_ADSP_I2S_CTL,
				0x0C, cs35l34->pdata.i2s_sdinloc);
	}
	if (cs35l34->pdata.i2s_txout >= 0
	  && cs35l34->pdata.i2s_txout <= 0x03) {
		regmap_update_bits(cs35l34->regmap, CS35L34_ADSP_I2S_CTL,
				0x03, cs35l34->pdata.i2s_txout);
	}
	if (cs35l34->pdata.react_brownout > 0) {
		regmap_update_bits(cs35l34->regmap, CS35L34_PWRCTL2,
				1 << 4, 1 << 4);
	}
	/* Multi device synchronization flags */
	if (cs35l34->pdata.sdout_share > 0 &&
				cs35l34->pdata.sdout_share <= 0x0F) {
		regmap_update_bits(cs35l34->regmap, CS35L34_MULT_DEV_SYNCH1,
				0x0F, cs35l34->pdata.sdout_share);
	} else {
		regmap_update_bits(cs35l34->regmap, CS35L34_MULT_DEV_SYNCH1,
				0x0F, 0x80);
	}
	if (cs35l34->pdata.boost_peak > 0
	  && cs35l34->pdata.boost_peak < 0x22) {
		regmap_update_bits(cs35l34->regmap, CS35L34_BST_PEAK_I,
				0x3F, cs35l34->pdata.boost_peak);
	}

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs35l34, &cs35l34_dai, 1);
	if (ret < 0) {
		dev_err(&i2c_client->dev,
			"%s: Register codec failed\n", __func__);
		goto err;
	}
	return 0;
err:
	return ret;
}

static int cs35l34_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct of_device_id cs35l34_of_match[] = {
	{.compatible = "cirrus,cs35l34"},
	{},
};
MODULE_DEVICE_TABLE(of, cs35l34_of_match);

static const struct i2c_device_id cs35l34_id[] = {
	{"cs35l34", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs35l34_id);

static struct i2c_driver cs35l34_i2c_driver = {
	.driver = {
		.name = "cs35l34",
		.owner = THIS_MODULE,
		.of_match_table = cs35l34_of_match,

		},
	.id_table = cs35l34_id,
	.probe = cs35l34_i2c_probe,
	.remove = cs35l34_i2c_remove,

};

static int __init cs35l34_modinit(void)
{
	int ret;
	ret = i2c_add_driver(&cs35l34_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register CS35l34 I2C driver: %d\n", ret);
		return ret;
	}
	return 0;
}

module_init(cs35l34_modinit);

static void __exit cs35l34_exit(void)
{
	i2c_del_driver(&cs35l34_i2c_driver);
}

module_exit(cs35l34_exit);

MODULE_DESCRIPTION("ASoC CS35l34 driver");
MODULE_AUTHOR("Alex Kovacs, Cirrus Logic Inc, <alex.kovacs@cirrus.com>");
MODULE_LICENSE("GPL");
