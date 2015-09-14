/*
 * moon.c  --  ALSA SoC Audio driver for MOON-class devices
 *
 * Copyright 2015 Cirrus Logic
 *
 * Author: Nikesh Oswal <nikesh@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"
#include "wm_adsp.h"
#include "moon.h"

#define MOON_NUM_ADSP 7

#define MOON_DEFAULT_FRAGMENTS       1
#define MOON_DEFAULT_FRAGMENT_SIZE   4096

#define MOON_FRF_COEFFICIENT_LEN 4

static int moon_frf_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);

#define MOON_FRF_BYTES(xname, xbase, xregs)			\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,	\
	.info = snd_soc_bytes_info, .get = snd_soc_bytes_get,	\
	.put = moon_frf_bytes_put, .private_value =		\
	((unsigned long)&(struct soc_bytes)			\
		{.base = xbase, .num_regs = xregs }) }

/* 2 mixer inputs with a stride of n in the register address */
#define MOON_MIXER_INPUTS_2_N(_reg, n)	\
	(_reg),					\
	(_reg) + (1 * (n))

/* 4 mixer inputs with a stride of n in the register address */
#define MOON_MIXER_INPUTS_4_N(_reg, n)		\
	MOON_MIXER_INPUTS_2_N(_reg, n),		\
	MOON_MIXER_INPUTS_2_N(_reg + (2 * n), n)

#define MOON_DSP_MIXER_INPUTS(_reg) \
	MOON_MIXER_INPUTS_4_N(_reg, 2),		\
	MOON_MIXER_INPUTS_4_N(_reg + 8, 2),	\
	MOON_MIXER_INPUTS_4_N(_reg + 16, 8),	\
	MOON_MIXER_INPUTS_2_N(_reg + 48, 8)

static const int moon_fx_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_EQ1MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_EQ2MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_EQ3MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_EQ4MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_DRC1LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_DRC1RMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_DRC2LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_DRC2RMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_HPLP1MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_HPLP2MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_HPLP3MIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_HPLP4MIX_INPUT_1_SOURCE, 2),
};

static const int moon_asrc1_1_inputs[] = {
	MOON_MIXER_INPUTS_2_N(CLEARWATER_ASRC1_1LMIX_INPUT_1_SOURCE, 8),
};

static const int moon_asrc1_2_inputs[] = {
	MOON_MIXER_INPUTS_2_N(CLEARWATER_ASRC1_2LMIX_INPUT_1_SOURCE, 8),
};

static const int moon_asrc2_1_inputs[] = {
	MOON_MIXER_INPUTS_2_N(CLEARWATER_ASRC2_1LMIX_INPUT_1_SOURCE, 8),
};

static const int moon_asrc2_2_inputs[] = {
	MOON_MIXER_INPUTS_2_N(CLEARWATER_ASRC2_2LMIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc1_fsl_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc1_fsh_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc2_fsl_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc2_fsh_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc3_fsl_inputs[] = {
	MOON_MIXER_INPUTS_2_N(ARIZONA_ISRC3INT1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc3_fsh_inputs[] = {
	MOON_MIXER_INPUTS_2_N(ARIZONA_ISRC3DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc4_fsl_inputs[] = {
	MOON_MIXER_INPUTS_2_N(ARIZONA_ISRC4INT1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_isrc4_fsh_inputs[] = {
	MOON_MIXER_INPUTS_2_N(ARIZONA_ISRC4DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_out_inputs[] = {
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT1LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT1RMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT2LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT2RMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT3LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT3RMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT5LMIX_INPUT_1_SOURCE, 2),
	MOON_MIXER_INPUTS_4_N(ARIZONA_OUT5RMIX_INPUT_1_SOURCE, 2),
};

static const int moon_spd1_inputs[] = {
	MOON_MIXER_INPUTS_2_N(ARIZONA_SPDIFTX1MIX_INPUT_1_SOURCE, 8),
};

static const int moon_dsp1_inputs[] = {
	MOON_DSP_MIXER_INPUTS(ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp2_inputs[] = {
	MOON_DSP_MIXER_INPUTS(ARIZONA_DSP2LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp3_inputs[] = {
	MOON_DSP_MIXER_INPUTS(ARIZONA_DSP3LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp4_inputs[] = {
	MOON_DSP_MIXER_INPUTS(ARIZONA_DSP4LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp5_inputs[] = {
	MOON_DSP_MIXER_INPUTS(CLEARWATER_DSP5LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp6_inputs[] = {
	MOON_DSP_MIXER_INPUTS(CLEARWATER_DSP6LMIX_INPUT_1_SOURCE),
};

static const int moon_dsp7_inputs[] = {
	MOON_DSP_MIXER_INPUTS(CLEARWATER_DSP7LMIX_INPUT_1_SOURCE),
};

static int moon_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#define MOON_RATE_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,\
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_get_enum_double, .put = moon_rate_put, \
	.private_value = (unsigned long)&xenum }

struct moon_compr {
	struct mutex lock;

	struct snd_compr_stream *stream;
	struct wm_adsp *adsp;

	size_t total_copied;
	bool allocated;
	bool trig;
};

struct moon_priv {
	struct arizona_priv core;
	struct arizona_fll fll[3];
	struct moon_compr compr_info;

	struct mutex fw_lock;
};

static const struct wm_adsp_region moon_dsp1_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x080000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x0e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x0a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x0c0000 },
};

static const struct wm_adsp_region moon_dsp2_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x100000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x160000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x120000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x140000 },
};

static const struct wm_adsp_region moon_dsp3_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x180000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x1e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x1a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x1c0000 },
};

static const struct wm_adsp_region moon_dsp4_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x200000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x260000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x220000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x240000 },
};

static const struct wm_adsp_region moon_dsp5_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x280000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x2e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x2a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x2c0000 },
};

static const struct wm_adsp_region moon_dsp6_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x300000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x360000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x320000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x340000 },
};

static const struct wm_adsp_region moon_dsp7_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x380000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x3e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x3a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x3c0000 },
};

static const struct wm_adsp_region *moon_dsp_regions[] = {
	moon_dsp1_regions,
	moon_dsp2_regions,
	moon_dsp3_regions,
	moon_dsp4_regions,
	moon_dsp5_regions,
	moon_dsp6_regions,
	moon_dsp7_regions,
};

static const int wm_adsp2_control_bases[] = {
	CLEARWATER_DSP1_CONFIG,
	CLEARWATER_DSP2_CONFIG,
	CLEARWATER_DSP3_CONFIG,
	CLEARWATER_DSP4_CONFIG,
	CLEARWATER_DSP5_CONFIG,
	CLEARWATER_DSP6_CONFIG,
	CLEARWATER_DSP7_CONFIG,
};

static const int moon_adsp_bus_error_irqs[MOON_NUM_ADSP] = {
	MOON_IRQ_DSP1_BUS_ERROR,
	MOON_IRQ_DSP2_BUS_ERROR,
	MOON_IRQ_DSP3_BUS_ERROR,
	MOON_IRQ_DSP4_BUS_ERROR,
	MOON_IRQ_DSP5_BUS_ERROR,
	MOON_IRQ_DSP6_BUS_ERROR,
	MOON_IRQ_DSP7_BUS_ERROR,
};

static const char * const moon_inmux_texts[] = {
	"A",
	"B",
};

static const SOC_ENUM_SINGLE_DECL(moon_in1muxl_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_1L,
				  ARIZONA_IN1L_SRC_SHIFT,
				  moon_inmux_texts);

static const SOC_ENUM_SINGLE_DECL(moon_in1muxr_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_1R,
				  ARIZONA_IN1R_SRC_SHIFT,
				  moon_inmux_texts);

static const SOC_ENUM_SINGLE_DECL(moon_in2muxl_enum,
				  ARIZONA_ADC_DIGITAL_VOLUME_2L,
				  ARIZONA_IN2L_SRC_SHIFT,
				  moon_inmux_texts);

static const struct snd_kcontrol_new moon_in1mux[2] = {
	SOC_DAPM_ENUM("IN1L Mux", moon_in1muxl_enum),
	SOC_DAPM_ENUM("IN1R Mux", moon_in1muxr_enum),
};

static const struct snd_kcontrol_new moon_in2mux =
	SOC_DAPM_ENUM("IN2L Mux", moon_in2muxl_enum);

static int moon_frf_bytes_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret, len;
	void *data;

	len = params->num_regs * component->val_bytes;

	data = kmemdup(ucontrol->value.bytes.data, len, GFP_KERNEL | GFP_DMA);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&arizona->reg_setting_lock);
	regmap_write(arizona->regmap, 0x80, 0x3);

	ret = regmap_raw_write(component->regmap, params->base,
			       data, len);

	regmap_write(arizona->regmap, 0x80, 0x0);
	mutex_unlock(&arizona->reg_setting_lock);

out:
	kfree(data);
	return ret;
}

/* Allow the worst case number of sources (FX Rate currently) */
static unsigned int mixer_sources_cache[ARRAY_SIZE(moon_fx_inputs)];

static int moon_get_sources(unsigned int reg,
				  const int **cur_sources, int *lim)
{
	int ret = 0;

	switch (reg) {
	case ARIZONA_FX_CTRL1:
		*cur_sources = moon_fx_inputs;
		*lim = ARRAY_SIZE(moon_fx_inputs);
		break;
	case CLEARWATER_ASRC1_RATE1:
		*cur_sources = moon_asrc1_1_inputs;
		*lim = ARRAY_SIZE(moon_asrc1_1_inputs);
		break;
	case CLEARWATER_ASRC1_RATE2:
		*cur_sources = moon_asrc1_2_inputs;
		*lim = ARRAY_SIZE(moon_asrc1_2_inputs);
		break;
	case CLEARWATER_ASRC2_RATE1:
		*cur_sources = moon_asrc2_1_inputs;
		*lim = ARRAY_SIZE(moon_asrc2_1_inputs);
		break;
	case CLEARWATER_ASRC2_RATE2:
		*cur_sources = moon_asrc2_2_inputs;
		*lim = ARRAY_SIZE(moon_asrc2_2_inputs);
		break;
	case ARIZONA_ISRC_1_CTRL_1:
		*cur_sources = moon_isrc1_fsh_inputs;
		*lim = ARRAY_SIZE(moon_isrc1_fsh_inputs);
		break;
	case ARIZONA_ISRC_1_CTRL_2:
		*cur_sources = moon_isrc1_fsl_inputs;
		*lim = ARRAY_SIZE(moon_isrc1_fsl_inputs);
		break;
	case ARIZONA_ISRC_2_CTRL_1:
		*cur_sources = moon_isrc2_fsh_inputs;
		*lim = ARRAY_SIZE(moon_isrc2_fsh_inputs);
		break;
	case ARIZONA_ISRC_2_CTRL_2:
		*cur_sources = moon_isrc2_fsl_inputs;
		*lim = ARRAY_SIZE(moon_isrc2_fsl_inputs);
		break;
	case ARIZONA_ISRC_3_CTRL_1:
		*cur_sources = moon_isrc3_fsh_inputs;
		*lim = ARRAY_SIZE(moon_isrc3_fsh_inputs);
		break;
	case ARIZONA_ISRC_3_CTRL_2:
		*cur_sources = moon_isrc3_fsl_inputs;
		*lim = ARRAY_SIZE(moon_isrc3_fsl_inputs);
		break;
	case ARIZONA_ISRC_4_CTRL_1:
		*cur_sources = moon_isrc4_fsh_inputs;
		*lim = ARRAY_SIZE(moon_isrc4_fsh_inputs);
		break;
	case ARIZONA_ISRC_4_CTRL_2:
		*cur_sources = moon_isrc4_fsl_inputs;
		*lim = ARRAY_SIZE(moon_isrc4_fsl_inputs);
		break;
	case ARIZONA_OUTPUT_RATE_1:
		*cur_sources = moon_out_inputs;
		*lim = ARRAY_SIZE(moon_out_inputs);
		break;
	case ARIZONA_SPD1_TX_CONTROL:
		*cur_sources = moon_spd1_inputs;
		*lim = ARRAY_SIZE(moon_spd1_inputs);
		break;
	case CLEARWATER_DSP1_CONFIG:
		*cur_sources = moon_dsp1_inputs;
		*lim = ARRAY_SIZE(moon_dsp1_inputs);
		break;
	case CLEARWATER_DSP2_CONFIG:
		*cur_sources = moon_dsp2_inputs;
		*lim = ARRAY_SIZE(moon_dsp2_inputs);
		break;
	case CLEARWATER_DSP3_CONFIG:
		*cur_sources = moon_dsp3_inputs;
		*lim = ARRAY_SIZE(moon_dsp3_inputs);
		break;
	case CLEARWATER_DSP4_CONFIG:
		*cur_sources = moon_dsp4_inputs;
		*lim = ARRAY_SIZE(moon_dsp4_inputs);
		break;
	case CLEARWATER_DSP5_CONFIG:
		*cur_sources = moon_dsp5_inputs;
		*lim = ARRAY_SIZE(moon_dsp5_inputs);
		break;
	case CLEARWATER_DSP6_CONFIG:
		*cur_sources = moon_dsp6_inputs;
		*lim = ARRAY_SIZE(moon_dsp6_inputs);
		break;
	case CLEARWATER_DSP7_CONFIG:
		*cur_sources = moon_dsp7_inputs;
		*lim = ARRAY_SIZE(moon_dsp7_inputs);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int moon_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret, err;
	int lim;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	struct moon_priv *moon = snd_soc_codec_get_drvdata(codec);
	struct arizona_priv *priv = &moon->core;
	struct arizona *arizona = priv->arizona;

	const int *cur_sources;

	unsigned int val, cur;
	unsigned int mask;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;

	val = e->values[ucontrol->value.enumerated.item[0]] << e->shift_l;
	mask = e->mask << e->shift_l;

	ret = regmap_read(arizona->regmap, e->reg, &cur);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to read current reg: %d\n", ret);
		return ret;
	}

	if ((cur & mask) == (val & mask))
		return 0;

	ret = moon_get_sources((int)e->reg, &cur_sources, &lim);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to get sources for 0x%08x: %d\n",
			e->reg,
			ret);
		return ret;
	}

	mutex_lock(&arizona->rate_lock);

	ret = arizona_cache_and_clear_sources(arizona, cur_sources,
					      mixer_sources_cache, lim);
	if (ret != 0) {
		dev_err(arizona->dev,
			"%s Failed to cache and clear sources %d\n",
			__func__,
			ret);
		goto out;
	}

	/* Apply the rate through the original callback */
	clearwater_spin_sysclk(arizona);
	udelay(300);
	mutex_lock(&codec->mutex);
	ret = snd_soc_update_bits(codec, e->reg, mask, val);
	mutex_unlock(&codec->mutex);
	clearwater_spin_sysclk(arizona);
	udelay(300);

out:
	err = arizona_restore_sources(arizona, cur_sources,
				      mixer_sources_cache, lim);
	if (err != 0) {
		dev_err(arizona->dev,
			"%s Failed to restore sources %d\n",
			__func__,
			err);
	}

	mutex_unlock(&arizona->rate_lock);
	return ret;
}

static int moon_adsp_rate_put_cb(struct wm_adsp *adsp,
				       unsigned int mask,
				       unsigned int val)
{
	int ret, err;
	int lim;
	const int *cur_sources;
	struct arizona *arizona = dev_get_drvdata(adsp->dev);
	unsigned int cur;

	ret = regmap_read(adsp->regmap, adsp->base, &cur);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to read current: %d\n", ret);
		return ret;
	}

	if ((val & mask) == (cur & mask))
		return 0;

	ret = moon_get_sources(adsp->base, &cur_sources, &lim);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to get sources for 0x%08x: %d\n",
			adsp->base,
			ret);
		return ret;
	}

	dev_dbg(arizona->dev, "%s for DSP%d\n", __func__, adsp->num);

	mutex_lock(&arizona->rate_lock);

	ret = arizona_cache_and_clear_sources(arizona, cur_sources,
					      mixer_sources_cache, lim);

	if (ret != 0) {
		dev_err(arizona->dev,
			"%s Failed to cache and clear sources %d\n",
			__func__,
			ret);
		goto out;
	}

	clearwater_spin_sysclk(arizona);
	udelay(300);
	/* Apply the rate */
	ret = regmap_update_bits(adsp->regmap, adsp->base, mask, val);
	clearwater_spin_sysclk(arizona);
	udelay(300);

out:
	err = arizona_restore_sources(arizona, cur_sources,
				      mixer_sources_cache, lim);

	if (err != 0) {
		dev_err(arizona->dev,
			"%s Failed to restore sources %d\n",
			__func__,
			err);
	}

	mutex_unlock(&arizona->rate_lock);
	return ret;
}

static int moon_sysclk_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(codec);
	struct arizona_priv *priv = &moon->core;
	struct arizona *arizona = priv->arizona;

	clearwater_spin_sysclk(arizona);
	udelay(300);

	return 0;
}

static int moon_adsp_power_ev(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(codec);
	struct arizona_priv *priv = &moon->core;
	struct arizona *arizona = priv->arizona;
	unsigned int freq;
	int ret;

	ret = regmap_read(arizona->regmap, CLEARWATER_DSP_CLOCK_2, &freq);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to read CLEARWATER_DSP_CLOCK_2: %d\n", ret);
		return ret;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->shift == 5) {
			mutex_lock(&moon->compr_info.lock);
			moon->compr_info.trig = false;
			mutex_unlock(&moon->compr_info.lock);
		}
		break;
	default:
		break;
	}

	return wm_adsp2_early_event(w, kcontrol, event, freq);
}

static DECLARE_TLV_DB_SCALE(ana_tlv, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
static DECLARE_TLV_DB_SCALE(noise_tlv, -13200, 600, 0);
static DECLARE_TLV_DB_SCALE(ng_tlv, -10200, 600, 0);

#define MOON_NG_SRC(name, base) \
	SOC_SINGLE(name " NG HPOUT1L Switch",  base,  0, 1, 0), \
	SOC_SINGLE(name " NG HPOUT1R Switch",  base,  1, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2L Switch",  base,  2, 1, 0), \
	SOC_SINGLE(name " NG HPOUT2R Switch",  base,  3, 1, 0), \
	SOC_SINGLE(name " NG HPOUT3L Switch",  base,  4, 1, 0), \
	SOC_SINGLE(name " NG HPOUT3R Switch",  base,  5, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1L Switch", base,  8, 1, 0), \
	SOC_SINGLE(name " NG SPKDAT1R Switch", base,  9, 1, 0)

#define MOON_RXANC_INPUT_ROUTES(widget, name) \
	{ widget, NULL, name " Input" }, \
	{ name " Input", "IN1L", "IN1L PGA" }, \
	{ name " Input", "IN1R", "IN1R PGA" }, \
	{ name " Input", "IN1L + IN1R", "IN1L PGA" }, \
	{ name " Input", "IN1L + IN1R", "IN1R PGA" }, \
	{ name " Input", "IN2L", "IN2L PGA" }, \
	{ name " Input", "IN2R", "IN2R PGA" }, \
	{ name " Input", "IN2L + IN2R", "IN2L PGA" }, \
	{ name " Input", "IN2L + IN2R", "IN2R PGA" }, \
	{ name " Input", "IN3L", "IN3L PGA" }, \
	{ name " Input", "IN3R", "IN3R PGA" }, \
	{ name " Input", "IN3L + IN3R", "IN3L PGA" }, \
	{ name " Input", "IN3L + IN3R", "IN3R PGA" }, \
	{ name " Input", "IN4L", "IN4L PGA" }, \
	{ name " Input", "IN4R", "IN4R PGA" }, \
	{ name " Input", "IN4L + IN4R", "IN4L PGA" }, \
	{ name " Input", "IN4L + IN4R", "IN4R PGA" }, \
	{ name " Input", "IN5L", "IN5L PGA" }, \
	{ name " Input", "IN5R", "IN5R PGA" }, \
	{ name " Input", "IN5L + IN5R", "IN5L PGA" }, \
	{ name " Input", "IN5L + IN5R", "IN5R PGA" }

#define MOON_RXANC_OUTPUT_ROUTES(widget, name) \
	{ widget, NULL, name " ANC Source" }, \
	{ name " ANC Source", "RXANCL", "RXANCL" }, \
	{ name " ANC Source", "RXANCR", "RXANCR" }

static const struct snd_kcontrol_new moon_snd_controls[] = {
SOC_ENUM_EXT("IN1 OSR", clearwater_in_dmic_osr[0],
	snd_soc_get_enum_double, moon_osr_put),
SOC_ENUM_EXT("IN2 OSR", clearwater_in_dmic_osr[1],
	snd_soc_get_enum_double, moon_osr_put),
SOC_ENUM("IN3 OSR", clearwater_in_dmic_osr[2]),
SOC_ENUM("IN4 OSR", clearwater_in_dmic_osr[3]),
SOC_ENUM("IN5 OSR", clearwater_in_dmic_osr[4]),

SOC_SINGLE_RANGE_TLV("IN1L Volume", ARIZONA_IN1L_CONTROL,
		     ARIZONA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN1R Volume", ARIZONA_IN1R_CONTROL,
		     ARIZONA_IN1R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2L Volume", ARIZONA_IN2L_CONTROL,
		     ARIZONA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2R Volume", ARIZONA_IN2R_CONTROL,
		     ARIZONA_IN2R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),

SOC_ENUM("IN HPF Cutoff Frequency", arizona_in_hpf_cut_enum),

SOC_SINGLE_EXT("IN1L LP Switch", ARIZONA_ADC_DIGITAL_VOLUME_1L,
	   MOON_IN1L_LP_MODE_SHIFT, 1, 0,
	snd_soc_get_volsw, moon_lp_mode_put),
SOC_SINGLE_EXT("IN1R LP Switch", ARIZONA_ADC_DIGITAL_VOLUME_1R,
	   MOON_IN1L_LP_MODE_SHIFT, 1, 0,
	snd_soc_get_volsw, moon_lp_mode_put),
SOC_SINGLE_EXT("IN2L LP Switch", ARIZONA_ADC_DIGITAL_VOLUME_2L,
	   MOON_IN1L_LP_MODE_SHIFT, 1, 0,
	snd_soc_get_volsw, moon_lp_mode_put),
SOC_SINGLE_EXT("IN2R LP Switch", ARIZONA_ADC_DIGITAL_VOLUME_2R,
	   MOON_IN1L_LP_MODE_SHIFT, 1, 0,
	snd_soc_get_volsw, moon_lp_mode_put),

SOC_SINGLE("IN1L HPF Switch", ARIZONA_IN1L_CONTROL,
	   ARIZONA_IN1L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN1R HPF Switch", ARIZONA_IN1R_CONTROL,
	   ARIZONA_IN1R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2L HPF Switch", ARIZONA_IN2L_CONTROL,
	   ARIZONA_IN2L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2R HPF Switch", ARIZONA_IN2R_CONTROL,
	   ARIZONA_IN2R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN3L HPF Switch", ARIZONA_IN3L_CONTROL,
	   ARIZONA_IN3L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN3R HPF Switch", ARIZONA_IN3R_CONTROL,
	   ARIZONA_IN3R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN4L HPF Switch", ARIZONA_IN4L_CONTROL,
	   ARIZONA_IN4L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN4R HPF Switch", ARIZONA_IN4R_CONTROL,
	   ARIZONA_IN4R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN5L HPF Switch", ARIZONA_IN5L_CONTROL,
	   ARIZONA_IN5L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN5R HPF Switch", ARIZONA_IN5R_CONTROL,
	   ARIZONA_IN5R_HPF_SHIFT, 1, 0),

SOC_SINGLE_TLV("IN1L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1L,
	       ARIZONA_IN1L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN1R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1R,
	       ARIZONA_IN1R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2L,
	       ARIZONA_IN2L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2R,
	       ARIZONA_IN2R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN3L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_3L,
	       ARIZONA_IN3L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN3R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_3R,
	       ARIZONA_IN3R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN4L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_4L,
	       ARIZONA_IN4L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN4R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_4R,
	       ARIZONA_IN4R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN5L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_5L,
	       ARIZONA_IN5L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN5R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_5R,
	       ARIZONA_IN5R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),

SOC_ENUM("Input Ramp Up", arizona_in_vi_ramp),
SOC_ENUM("Input Ramp Down", arizona_in_vd_ramp),

SND_SOC_BYTES_MASK("RXANC Config", ARIZONA_CLOCK_CONTROL, 1,
		   ARIZONA_CLK_R_ENA_CLR | ARIZONA_CLK_R_ENA_SET |
		   ARIZONA_CLK_L_ENA_CLR | ARIZONA_CLK_L_ENA_SET),
SND_SOC_BYTES("RXANC Coefficients", ARIZONA_ANC_COEFF_START,
	      ARIZONA_ANC_COEFF_END - ARIZONA_ANC_COEFF_START + 1),
SND_SOC_BYTES("RXANCL Config", ARIZONA_FCL_FILTER_CONTROL, 1),
SND_SOC_BYTES("RXANCL Coefficients", ARIZONA_FCL_COEFF_START,
	      ARIZONA_FCL_COEFF_END - ARIZONA_FCL_COEFF_START + 1),
SND_SOC_BYTES("RXANCR Config", CLEARWATER_FCR_FILTER_CONTROL, 1),
SND_SOC_BYTES("RXANCR Coefficients", CLEARWATER_FCR_COEFF_START,
	      CLEARWATER_FCR_COEFF_END - CLEARWATER_FCR_COEFF_START + 1),

MOON_FRF_BYTES("FRF COEFF 1L", CLEARWATER_FRF_COEFFICIENT_1L_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 1R", CLEARWATER_FRF_COEFFICIENT_1R_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 2L", CLEARWATER_FRF_COEFFICIENT_2L_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 2R", CLEARWATER_FRF_COEFFICIENT_2R_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 3L", CLEARWATER_FRF_COEFFICIENT_3L_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 3R", CLEARWATER_FRF_COEFFICIENT_3R_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 5L", CLEARWATER_FRF_COEFFICIENT_5L_1,
				 MOON_FRF_COEFFICIENT_LEN),
MOON_FRF_BYTES("FRF COEFF 5R", CLEARWATER_FRF_COEFFICIENT_5R_1,
				 MOON_FRF_COEFFICIENT_LEN),

SND_SOC_BYTES("DAC COMP 1", CLEARWATER_DAC_COMP_1, 1),
SND_SOC_BYTES("DAC COMP 2", CLEARWATER_DAC_COMP_2, 1),

ARIZONA_MIXER_CONTROLS("EQ1", ARIZONA_EQ1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ2", ARIZONA_EQ2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ3", ARIZONA_EQ3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("EQ4", ARIZONA_EQ4MIX_INPUT_1_SOURCE),

ARIZONA_EQ_CONTROL("EQ1 Coefficients", ARIZONA_EQ1_2),
SOC_SINGLE_TLV("EQ1 B1 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B2 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B3 Volume", ARIZONA_EQ1_1, ARIZONA_EQ1_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B4 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ1 B5 Volume", ARIZONA_EQ1_2, ARIZONA_EQ1_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ2 Coefficients", ARIZONA_EQ2_2),
SOC_SINGLE_TLV("EQ2 B1 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B2 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B3 Volume", ARIZONA_EQ2_1, ARIZONA_EQ2_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B4 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ2 B5 Volume", ARIZONA_EQ2_2, ARIZONA_EQ2_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ3 Coefficients", ARIZONA_EQ3_2),
SOC_SINGLE_TLV("EQ3 B1 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B2 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B3 Volume", ARIZONA_EQ3_1, ARIZONA_EQ3_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B4 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ3 B5 Volume", ARIZONA_EQ3_2, ARIZONA_EQ3_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_EQ_CONTROL("EQ4 Coefficients", ARIZONA_EQ4_2),
SOC_SINGLE_TLV("EQ4 B1 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B1_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B2 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B2_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B3 Volume", ARIZONA_EQ4_1, ARIZONA_EQ4_B3_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B4 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B4_GAIN_SHIFT,
	       24, 0, eq_tlv),
SOC_SINGLE_TLV("EQ4 B5 Volume", ARIZONA_EQ4_2, ARIZONA_EQ4_B5_GAIN_SHIFT,
	       24, 0, eq_tlv),

ARIZONA_MIXER_CONTROLS("DRC1L", ARIZONA_DRC1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC1R", ARIZONA_DRC1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC2L", ARIZONA_DRC2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DRC2R", ARIZONA_DRC2RMIX_INPUT_1_SOURCE),

SND_SOC_BYTES_MASK("DRC1", ARIZONA_DRC1_CTRL1, 5,
		   ARIZONA_DRC1R_ENA | ARIZONA_DRC1L_ENA),
SND_SOC_BYTES_MASK("DRC2", CLEARWATER_DRC2_CTRL1, 5,
		   ARIZONA_DRC2R_ENA | ARIZONA_DRC2L_ENA),

ARIZONA_MIXER_CONTROLS("LHPF1", ARIZONA_HPLP1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF2", ARIZONA_HPLP2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF3", ARIZONA_HPLP3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("LHPF4", ARIZONA_HPLP4MIX_INPUT_1_SOURCE),

SND_SOC_BYTES("LHPF1 Coefficients", ARIZONA_HPLPF1_2, 1),
SND_SOC_BYTES("LHPF2 Coefficients", ARIZONA_HPLPF2_2, 1),
SND_SOC_BYTES("LHPF3 Coefficients", ARIZONA_HPLPF3_2, 1),
SND_SOC_BYTES("LHPF4 Coefficients", ARIZONA_HPLPF4_2, 1),

SOC_ENUM("LHPF1 Mode", arizona_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", arizona_lhpf2_mode),
SOC_ENUM("LHPF3 Mode", arizona_lhpf3_mode),
SOC_ENUM("LHPF4 Mode", arizona_lhpf4_mode),

SOC_ENUM("Sample Rate 2", arizona_sample_rate[0]),
SOC_ENUM("Sample Rate 3", arizona_sample_rate[1]),
SOC_ENUM("ASYNC Sample Rate 2", arizona_sample_rate[2]),

MOON_RATE_ENUM("FX Rate", arizona_fx_rate),

MOON_RATE_ENUM("ISRC1 FSL", arizona_isrc_fsl[0]),
MOON_RATE_ENUM("ISRC2 FSL", arizona_isrc_fsl[1]),
MOON_RATE_ENUM("ISRC3 FSL", arizona_isrc_fsl[2]),
MOON_RATE_ENUM("ISRC4 FSL", arizona_isrc_fsl[3]),
MOON_RATE_ENUM("ISRC1 FSH", arizona_isrc_fsh[0]),
MOON_RATE_ENUM("ISRC2 FSH", arizona_isrc_fsh[1]),
MOON_RATE_ENUM("ISRC3 FSH", arizona_isrc_fsh[2]),
MOON_RATE_ENUM("ISRC4 FSH", arizona_isrc_fsh[3]),
MOON_RATE_ENUM("ASRC1 Rate 1", clearwater_asrc1_rate[0]),
MOON_RATE_ENUM("ASRC1 Rate 2", clearwater_asrc1_rate[1]),
MOON_RATE_ENUM("ASRC2 Rate 1", clearwater_asrc2_rate[0]),
MOON_RATE_ENUM("ASRC2 Rate 2", clearwater_asrc2_rate[1]),

ARIZONA_MIXER_CONTROLS("DSP1L", ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP1R", ARIZONA_DSP1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP2L", ARIZONA_DSP2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP2R", ARIZONA_DSP2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP3L", ARIZONA_DSP3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP3R", ARIZONA_DSP3RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP4L", ARIZONA_DSP4LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP4R", ARIZONA_DSP4RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP5L", CLEARWATER_DSP5LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP5R", CLEARWATER_DSP5RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP6L", CLEARWATER_DSP6LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP6R", CLEARWATER_DSP6RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP7L", CLEARWATER_DSP7LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP7R", CLEARWATER_DSP7RMIX_INPUT_1_SOURCE),

SOC_SINGLE_TLV("Noise Generator Volume", CLEARWATER_COMFORT_NOISE_GENERATOR,
	       CLEARWATER_NOISE_GEN_GAIN_SHIFT, 0x16, 0, noise_tlv),

ARIZONA_MIXER_CONTROLS("HPOUT1L", ARIZONA_OUT1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT1R", ARIZONA_OUT1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2L", ARIZONA_OUT2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT2R", ARIZONA_OUT2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT3L", ARIZONA_OUT3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("HPOUT3R", ARIZONA_OUT3RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1L", ARIZONA_OUT5LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDAT1R", ARIZONA_OUT5RMIX_INPUT_1_SOURCE),

SOC_SINGLE("HPOUT1 SC Protect Switch", ARIZONA_HP1_SHORT_CIRCUIT_CTRL,
	   ARIZONA_HP1_SC_ENA_SHIFT, 1, 0),
SOC_SINGLE("HPOUT2 SC Protect Switch", ARIZONA_HP2_SHORT_CIRCUIT_CTRL,
	   ARIZONA_HP2_SC_ENA_SHIFT, 1, 0),
SOC_SINGLE("HPOUT3 SC Protect Switch", ARIZONA_HP3_SHORT_CIRCUIT_CTRL,
	   ARIZONA_HP3_SC_ENA_SHIFT, 1, 0),

SOC_SINGLE("SPKDAT1 High Performance Switch", ARIZONA_OUTPUT_PATH_CONFIG_5L,
	   ARIZONA_OUT5_OSR_SHIFT, 1, 0),

SOC_DOUBLE_R("HPOUT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_1L,
	     ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT2 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_2L,
	     ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("HPOUT3 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_3L,
	     ARIZONA_DAC_DIGITAL_VOLUME_3R, ARIZONA_OUT3L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("SPKDAT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_5L,
	     ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("HPOUT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_1L,
		 ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT2 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_2L,
		 ARIZONA_DAC_DIGITAL_VOLUME_2R, ARIZONA_OUT2L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("HPOUT3 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_3L,
		 ARIZONA_DAC_DIGITAL_VOLUME_3R, ARIZONA_OUT3L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("SPKDAT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_5L,
		 ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),

SOC_DOUBLE("SPKDAT1 Switch", ARIZONA_PDM_SPK1_CTRL_1, ARIZONA_SPK1L_MUTE_SHIFT,
	   ARIZONA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_EXT("HPOUT1 DRE Switch", ARIZONA_DRE_ENABLE,
	   VEGAS_DRE1L_ENA_SHIFT, VEGAS_DRE1R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, clearwater_put_dre),
SOC_DOUBLE_EXT("HPOUT2 DRE Switch", ARIZONA_DRE_ENABLE,
	   VEGAS_DRE2L_ENA_SHIFT, VEGAS_DRE2R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, clearwater_put_dre),
SOC_DOUBLE_EXT("HPOUT3 DRE Switch", ARIZONA_DRE_ENABLE,
	   VEGAS_DRE3L_ENA_SHIFT, VEGAS_DRE3R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, clearwater_put_dre),

SOC_DOUBLE("HPOUT1 EDRE Switch", CLEARWATER_EDRE_ENABLE,
	   CLEARWATER_EDRE_OUT1L_THR1_ENA_SHIFT,
	   CLEARWATER_EDRE_OUT1R_THR1_ENA_SHIFT, 1, 0),
SOC_DOUBLE("HPOUT2 EDRE Switch", CLEARWATER_EDRE_ENABLE,
	   CLEARWATER_EDRE_OUT2L_THR1_ENA_SHIFT,
	   CLEARWATER_EDRE_OUT2R_THR1_ENA_SHIFT, 1, 0),
SOC_DOUBLE("HPOUT3 EDRE Switch", CLEARWATER_EDRE_ENABLE,
	   CLEARWATER_EDRE_OUT3L_THR1_ENA_SHIFT,
	   CLEARWATER_EDRE_OUT3R_THR1_ENA_SHIFT, 1, 0),

SOC_ENUM("Output Ramp Up", arizona_out_vi_ramp),
SOC_ENUM("Output Ramp Down", arizona_out_vd_ramp),

MOON_RATE_ENUM("SPDIF Rate", arizona_spdif_rate),

SOC_SINGLE("Noise Gate Switch", ARIZONA_NOISE_GATE_CONTROL,
	   ARIZONA_NGATE_ENA_SHIFT, 1, 0),
SOC_SINGLE_TLV("Noise Gate Threshold Volume", ARIZONA_NOISE_GATE_CONTROL,
	       ARIZONA_NGATE_THR_SHIFT, 7, 1, ng_tlv),
SOC_ENUM("Noise Gate Hold", arizona_ng_hold),

MOON_RATE_ENUM("Output Rate 1", arizona_output_rate),

SOC_ENUM_EXT("IN1L Rate", moon_input_rate[0],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN1R Rate", moon_input_rate[1],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN2L Rate", moon_input_rate[2],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN2R Rate", moon_input_rate[3],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN3L Rate", moon_input_rate[4],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN3R Rate", moon_input_rate[5],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN4L Rate", moon_input_rate[6],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN4R Rate", moon_input_rate[7],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN5L Rate", moon_input_rate[8],
	snd_soc_get_enum_double, moon_in_rate_put),
SOC_ENUM_EXT("IN5R Rate", moon_input_rate[9],
	snd_soc_get_enum_double, moon_in_rate_put),

SOC_ENUM_EXT("DFC1RX Width", moon_dfc_width[0],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC1RX Type", moon_dfc_type[0],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC1TX Width", moon_dfc_width[1],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC1TX Type", moon_dfc_type[1],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC2RX Width", moon_dfc_width[2],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC2RX Type", moon_dfc_type[2],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC2TX Width", moon_dfc_width[3],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC2TX Type", moon_dfc_type[3],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC3RX Width", moon_dfc_width[4],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC3RX Type", moon_dfc_type[4],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC3TX Width", moon_dfc_width[5],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC3TX Type", moon_dfc_type[5],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC4RX Width", moon_dfc_width[6],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC4RX Type", moon_dfc_type[6],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC4TX Width", moon_dfc_width[7],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC4TX Type", moon_dfc_type[7],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC5RX Width", moon_dfc_width[8],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC5RX Type", moon_dfc_type[8],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC5TX Width", moon_dfc_width[9],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC5TX Type", moon_dfc_type[9],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC6RX Width", moon_dfc_width[10],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC6RX Type", moon_dfc_type[10],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC6TX Width", moon_dfc_width[11],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC6TX Type", moon_dfc_type[11],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC7RX Width", moon_dfc_width[12],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC7RX Type", moon_dfc_type[12],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC7TX Width", moon_dfc_width[13],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC7TX Type", moon_dfc_type[13],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC8RX Width", moon_dfc_width[14],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC8RX Type", moon_dfc_type[14],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC8TX Width", moon_dfc_width[15],
	snd_soc_get_enum_double, moon_dfc_put),
SOC_ENUM_EXT("DFC8TX Type", moon_dfc_type[15],
	snd_soc_get_enum_double, moon_dfc_put),

MOON_NG_SRC("HPOUT1L", ARIZONA_NOISE_GATE_SELECT_1L),
MOON_NG_SRC("HPOUT1R", ARIZONA_NOISE_GATE_SELECT_1R),
MOON_NG_SRC("HPOUT2L", ARIZONA_NOISE_GATE_SELECT_2L),
MOON_NG_SRC("HPOUT2R", ARIZONA_NOISE_GATE_SELECT_2R),
MOON_NG_SRC("HPOUT3L", ARIZONA_NOISE_GATE_SELECT_3L),
MOON_NG_SRC("HPOUT3R", ARIZONA_NOISE_GATE_SELECT_3R),
MOON_NG_SRC("SPKDAT1L", ARIZONA_NOISE_GATE_SELECT_5L),
MOON_NG_SRC("SPKDAT1R", ARIZONA_NOISE_GATE_SELECT_5R),

ARIZONA_MIXER_CONTROLS("AIF1TX1", ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX2", ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX3", ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX4", ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX5", ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX6", ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX7", ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX8", ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF2TX1", ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX2", ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX3", ARIZONA_AIF2TX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX4", ARIZONA_AIF2TX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX5", ARIZONA_AIF2TX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX6", ARIZONA_AIF2TX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX7", ARIZONA_AIF2TX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX8", ARIZONA_AIF2TX8MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF3TX1", ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF3TX2", ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF4TX1", ARIZONA_AIF4TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF4TX2", ARIZONA_AIF4TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("SLIMTX1", ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX2", ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX3", ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX4", ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX5", ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX6", ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX7", ARIZONA_SLIMTX7MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX8", ARIZONA_SLIMTX8MIX_INPUT_1_SOURCE),

ARIZONA_GAINMUX_CONTROLS("SPDIFTX1", ARIZONA_SPDIFTX1MIX_INPUT_1_SOURCE),
ARIZONA_GAINMUX_CONTROLS("SPDIFTX2", ARIZONA_SPDIFTX2MIX_INPUT_1_SOURCE),
};

CLEARWATER_MIXER_ENUMS(EQ1, ARIZONA_EQ1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(EQ2, ARIZONA_EQ2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(EQ3, ARIZONA_EQ3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(EQ4, ARIZONA_EQ4MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DRC1L, ARIZONA_DRC1LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DRC1R, ARIZONA_DRC1RMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DRC2L, ARIZONA_DRC2LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DRC2R, ARIZONA_DRC2RMIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(LHPF1, ARIZONA_HPLP1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(LHPF2, ARIZONA_HPLP2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(LHPF3, ARIZONA_HPLP3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(LHPF4, ARIZONA_HPLP4MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP1L, ARIZONA_DSP1LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP1R, ARIZONA_DSP1RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP1, ARIZONA_DSP1AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP2L, ARIZONA_DSP2LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP2R, ARIZONA_DSP2RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP2, ARIZONA_DSP2AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP3L, ARIZONA_DSP3LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP3R, ARIZONA_DSP3RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP3, ARIZONA_DSP3AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP4L, ARIZONA_DSP4LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP4R, ARIZONA_DSP4RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP4, ARIZONA_DSP4AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP5L, CLEARWATER_DSP5LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP5R, CLEARWATER_DSP5RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP5, CLEARWATER_DSP5AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP6L, CLEARWATER_DSP6LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP6R, CLEARWATER_DSP6RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP6, CLEARWATER_DSP6AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(DSP7L, CLEARWATER_DSP7LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(DSP7R, CLEARWATER_DSP7RMIX_INPUT_1_SOURCE);
CLEARWATER_DSP_AUX_ENUMS(DSP7, CLEARWATER_DSP7AUX1MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(PWM1, ARIZONA_PWM1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(PWM2, ARIZONA_PWM2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(OUT1L, ARIZONA_OUT1LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT1R, ARIZONA_OUT1RMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT2L, ARIZONA_OUT2LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT2R, ARIZONA_OUT2RMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT3L, ARIZONA_OUT3LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT3R, ARIZONA_OUT3RMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SPKDAT1L, ARIZONA_OUT5LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SPKDAT1R, ARIZONA_OUT5RMIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF1TX1, ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX2, ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX3, ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX4, ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX5, ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX6, ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX7, ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX8, ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF2TX1, ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX2, ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX3, ARIZONA_AIF2TX3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX4, ARIZONA_AIF2TX4MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX5, ARIZONA_AIF2TX5MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX6, ARIZONA_AIF2TX6MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX7, ARIZONA_AIF2TX7MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX8, ARIZONA_AIF2TX8MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF3TX1, ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF3TX2, ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF4TX1, ARIZONA_AIF4TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF4TX2, ARIZONA_AIF4TX2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(SLIMTX1, ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX2, ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX3, ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX4, ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX5, ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX6, ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX7, ARIZONA_SLIMTX7MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX8, ARIZONA_SLIMTX8MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(SPD1TX1, ARIZONA_SPDIFTX1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(SPD1TX2, ARIZONA_SPDIFTX2MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ASRC1IN1L, CLEARWATER_ASRC1_1LMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC1IN1R, CLEARWATER_ASRC1_1RMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC1IN2L, CLEARWATER_ASRC1_2LMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC1IN2R, CLEARWATER_ASRC1_2RMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC2IN1L, CLEARWATER_ASRC2_1LMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC2IN1R, CLEARWATER_ASRC2_1RMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC2IN2L, CLEARWATER_ASRC2_2LMIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ASRC2IN2R, CLEARWATER_ASRC2_2RMIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC1INT1, ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1INT2, ARIZONA_ISRC1INT2MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1INT3, ARIZONA_ISRC1INT3MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1INT4, ARIZONA_ISRC1INT4MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC1DEC1, ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1DEC2, ARIZONA_ISRC1DEC2MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1DEC3, ARIZONA_ISRC1DEC3MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC1DEC4, ARIZONA_ISRC1DEC4MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC2INT1, ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2INT2, ARIZONA_ISRC2INT2MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2INT3, ARIZONA_ISRC2INT3MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2INT4, ARIZONA_ISRC2INT4MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC2DEC1, ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2DEC2, ARIZONA_ISRC2DEC2MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2DEC3, ARIZONA_ISRC2DEC3MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC2DEC4, ARIZONA_ISRC2DEC4MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC3INT1, ARIZONA_ISRC3INT1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC3INT2, ARIZONA_ISRC3INT2MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC3DEC1, ARIZONA_ISRC3DEC1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC3DEC2, ARIZONA_ISRC3DEC2MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC4INT1, ARIZONA_ISRC4INT1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC4INT2, ARIZONA_ISRC4INT2MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(ISRC4DEC1, ARIZONA_ISRC4DEC1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(ISRC4DEC2, ARIZONA_ISRC4DEC2MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(DFC1, MOON_DFC1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC2, MOON_DFC2MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC3, MOON_DFC3MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC4, MOON_DFC4MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC5, MOON_DFC5MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC6, MOON_DFC6MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC7, MOON_DFC7MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(DFC8, MOON_DFC8MIX_INPUT_1_SOURCE);

static const char * const moon_dsp_output_texts[] = {
	"None",
	"DSP6",
};

static const struct soc_enum moon_dsp_output_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(moon_dsp_output_texts),
			moon_dsp_output_texts);

static const struct snd_kcontrol_new moon_dsp_output_mux[] = {
	SOC_DAPM_ENUM("DSP Virtual Output Mux", moon_dsp_output_enum),
};

static const char * const moon_memory_mux_texts[] = {
	"None",
	"Shared Memory",
};

static const struct soc_enum moon_memory_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(moon_memory_mux_texts),
			moon_memory_mux_texts);

static const struct snd_kcontrol_new moon_memory_mux[] = {
	SOC_DAPM_ENUM("DSP2 Virtual Input", moon_memory_enum),
	SOC_DAPM_ENUM("DSP3 Virtual Input", moon_memory_enum),
};

static const char * const moon_aec_loopback_texts[] = {
	"HPOUT1L", "HPOUT1R", "HPOUT2L", "HPOUT2R", "HPOUT3L", "HPOUT3R",
	"SPKDAT1L", "SPKDAT1R",
};

static const unsigned int moon_aec_loopback_values[] = {
	0, 1, 2, 3, 4, 5, 8, 9,
};

static const struct soc_enum moon_aec_loopback =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DAC_AEC_CONTROL_1,
			      ARIZONA_AEC_LOOPBACK_SRC_SHIFT, 0xf,
			      ARRAY_SIZE(moon_aec_loopback_texts),
			      moon_aec_loopback_texts,
			      moon_aec_loopback_values);

static const struct snd_kcontrol_new moon_aec_loopback_mux =
	SOC_DAPM_ENUM("AEC Loopback", moon_aec_loopback);

static const struct snd_kcontrol_new moon_anc_input_mux[] = {
	SOC_DAPM_ENUM_EXT("RXANCL Input", clearwater_anc_input_src[0],
			  arizona_get_anc_input, arizona_put_anc_input),
	SOC_DAPM_ENUM_EXT("RXANCR Input", clearwater_anc_input_src[1],
			  arizona_get_anc_input, arizona_put_anc_input),
};

static const struct snd_kcontrol_new moon_output_anc_src[] = {
	SOC_DAPM_ENUM("HPOUT1L ANC Source", arizona_output_anc_src[0]),
	SOC_DAPM_ENUM("HPOUT1R ANC Source", arizona_output_anc_src[1]),
	SOC_DAPM_ENUM("HPOUT2L ANC Source", arizona_output_anc_src[2]),
	SOC_DAPM_ENUM("HPOUT2R ANC Source", arizona_output_anc_src[3]),
	SOC_DAPM_ENUM("HPOUT3L ANC Source", arizona_output_anc_src[4]),
	SOC_DAPM_ENUM("HPOUT3R ANC Source", clearwater_output_anc_src_defs[0]),
	SOC_DAPM_ENUM("SPKDAT1L ANC Source", arizona_output_anc_src[8]),
	SOC_DAPM_ENUM("SPKDAT1R ANC Source", arizona_output_anc_src[9]),
};

static const struct snd_soc_dapm_widget moon_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", ARIZONA_SYSTEM_CLOCK_1, ARIZONA_SYSCLK_ENA_SHIFT,
		    0, moon_sysclk_ev,
		    SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_SUPPLY("ASYNCCLK", ARIZONA_ASYNC_CLOCK_1,
		    ARIZONA_ASYNC_CLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("OPCLK", ARIZONA_OUTPUT_SYSTEM_CLOCK,
		    ARIZONA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("ASYNCOPCLK", ARIZONA_OUTPUT_ASYNC_CLOCK,
		    ARIZONA_OPCLK_ASYNC_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("DSPCLK", CLEARWATER_DSP_CLOCK_1, 6,
		    0, NULL, 0),


SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD2", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD3", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD4", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("MICVDD", 0, SND_SOC_DAPM_REGULATOR_BYPASS),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),
SND_SOC_DAPM_SIGGEN("HAPTICS"),

SND_SOC_DAPM_INPUT("IN1AL"),
SND_SOC_DAPM_INPUT("IN1BL"),
SND_SOC_DAPM_INPUT("IN1AR"),
SND_SOC_DAPM_INPUT("IN1BR"),
SND_SOC_DAPM_INPUT("IN2AL"),
SND_SOC_DAPM_INPUT("IN2BL"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),
SND_SOC_DAPM_INPUT("IN4L"),
SND_SOC_DAPM_INPUT("IN4R"),
SND_SOC_DAPM_INPUT("IN5L"),
SND_SOC_DAPM_INPUT("IN5R"),

SND_SOC_DAPM_MUX("IN1L Mux", SND_SOC_NOPM, 0, 0, &moon_in1mux[0]),
SND_SOC_DAPM_MUX("IN1R Mux", SND_SOC_NOPM, 0, 0, &moon_in1mux[1]),
SND_SOC_DAPM_MUX("IN2L Mux", SND_SOC_NOPM, 0, 0, &moon_in2mux),

SND_SOC_DAPM_OUTPUT("DRC1 Signal Activity"),
SND_SOC_DAPM_OUTPUT("DRC2 Signal Activity"),

SND_SOC_DAPM_OUTPUT("DSP Virtual Output"),

SND_SOC_DAPM_PGA_E("IN1L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN1R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN1R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN2R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN2R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN3R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN3R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN4L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN4L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN4R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN4R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN5L PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN5L_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("IN5R PGA", ARIZONA_INPUT_ENABLES, ARIZONA_IN5R_ENA_SHIFT,
		   0, NULL, 0, arizona_in_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_SUPPLY("MICBIAS1", ARIZONA_MIC_BIAS_CTRL_1,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2", ARIZONA_MIC_BIAS_CTRL_2,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS1A", ARIZONA_MIC_BIAS_CTRL_5,
			ARIZONA_MICB1A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1B", ARIZONA_MIC_BIAS_CTRL_5,
			ARIZONA_MICB1B_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1C", ARIZONA_MIC_BIAS_CTRL_5,
			ARIZONA_MICB1C_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1D", ARIZONA_MIC_BIAS_CTRL_5,
			ARIZONA_MICB1D_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS2A", ARIZONA_MIC_BIAS_CTRL_6,
			ARIZONA_MICB2A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2B", ARIZONA_MIC_BIAS_CTRL_6,
			ARIZONA_MICB2B_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2C", ARIZONA_MIC_BIAS_CTRL_6,
			ARIZONA_MICB2C_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2D", ARIZONA_MIC_BIAS_CTRL_6,
			ARIZONA_MICB2D_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Noise Generator", CLEARWATER_COMFORT_NOISE_GENERATOR,
		 CLEARWATER_NOISE_GEN_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("Tone Generator 1", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("Tone Generator 2", ARIZONA_TONE_GENERATOR_1,
		 ARIZONA_TONE2_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("EQ1", ARIZONA_EQ1_1, ARIZONA_EQ1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ2", ARIZONA_EQ2_1, ARIZONA_EQ2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ3", ARIZONA_EQ3_1, ARIZONA_EQ3_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("EQ4", ARIZONA_EQ4_1, ARIZONA_EQ4_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DRC1L", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC1R", ARIZONA_DRC1_CTRL1, ARIZONA_DRC1R_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC2L", CLEARWATER_DRC2_CTRL1, ARIZONA_DRC2L_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("DRC2R", CLEARWATER_DRC2_CTRL1, ARIZONA_DRC2R_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("LHPF1", ARIZONA_HPLPF1_1, ARIZONA_LHPF1_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF2", ARIZONA_HPLPF2_1, ARIZONA_LHPF2_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF3", ARIZONA_HPLPF3_1, ARIZONA_LHPF3_ENA_SHIFT, 0,
		 NULL, 0),
SND_SOC_DAPM_PGA("LHPF4", ARIZONA_HPLPF4_1, ARIZONA_LHPF4_ENA_SHIFT, 0,
		 NULL, 0),

SND_SOC_DAPM_PGA("PWM1 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM1_ENA_SHIFT,
		 0, NULL, 0),
SND_SOC_DAPM_PGA("PWM2 Driver", ARIZONA_PWM_DRIVE_1, ARIZONA_PWM2_ENA_SHIFT,
		 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC1IN1L", CLEARWATER_ASRC1_ENABLE,
		CLEARWATER_ASRC1_IN1L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN1R", CLEARWATER_ASRC1_ENABLE,
		CLEARWATER_ASRC1_IN1R_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN2L", CLEARWATER_ASRC1_ENABLE,
		CLEARWATER_ASRC1_IN2L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC1IN2R", CLEARWATER_ASRC1_ENABLE,
		CLEARWATER_ASRC1_IN2R_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ASRC2IN1L", CLEARWATER_ASRC2_ENABLE,
		CLEARWATER_ASRC2_IN1L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN1R", CLEARWATER_ASRC2_ENABLE,
		CLEARWATER_ASRC2_IN1R_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN2L", CLEARWATER_ASRC2_ENABLE,
		CLEARWATER_ASRC2_IN2L_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ASRC2IN2R", CLEARWATER_ASRC2_ENABLE,
		CLEARWATER_ASRC2_IN2R_ENA_SHIFT, 0, NULL, 0),

WM_ADSP2("DSP1", 0, moon_adsp_power_ev),
WM_ADSP2("DSP2", 1, moon_adsp_power_ev),
WM_ADSP2("DSP3", 2, moon_adsp_power_ev),
WM_ADSP2("DSP4", 3, moon_adsp_power_ev),
WM_ADSP2("DSP5", 4, moon_adsp_power_ev),
WM_ADSP2("DSP6", 5, moon_adsp_power_ev),
WM_ADSP2("DSP7", 6, moon_adsp_power_ev),

SND_SOC_DAPM_PGA("ISRC1INT1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1INT4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_INT3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC1DEC1", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC2", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC3", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC1DEC4", ARIZONA_ISRC_1_CTRL_3,
		 ARIZONA_ISRC1_DEC3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2INT1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT3", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2INT4", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_INT3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC2DEC1", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC2", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC3", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC2_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC2DEC4", ARIZONA_ISRC_2_CTRL_3,
		 ARIZONA_ISRC2_DEC3_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC3INT1", ARIZONA_ISRC_3_CTRL_3,
		 ARIZONA_ISRC3_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC3INT2", ARIZONA_ISRC_3_CTRL_3,
		 ARIZONA_ISRC3_INT1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC3DEC1", ARIZONA_ISRC_3_CTRL_3,
		 ARIZONA_ISRC3_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC3DEC2", ARIZONA_ISRC_3_CTRL_3,
		 ARIZONA_ISRC3_DEC1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC4INT1", ARIZONA_ISRC_4_CTRL_3,
		 ARIZONA_ISRC4_INT0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC4INT2", ARIZONA_ISRC_4_CTRL_3,
		 ARIZONA_ISRC4_INT1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("ISRC4DEC1", ARIZONA_ISRC_4_CTRL_3,
		 ARIZONA_ISRC4_DEC0_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("ISRC4DEC2", ARIZONA_ISRC_4_CTRL_3,
		 ARIZONA_ISRC4_DEC1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_MUX("AEC Loopback", ARIZONA_DAC_AEC_CONTROL_1,
		       ARIZONA_AEC_LOOPBACK_ENA_SHIFT, 0,
		       &moon_aec_loopback_mux),

SND_SOC_DAPM_MUX("RXANCL Input", SND_SOC_NOPM, 0, 0, &moon_anc_input_mux[0]),
SND_SOC_DAPM_MUX("RXANCR Input", SND_SOC_NOPM, 0, 0, &moon_anc_input_mux[1]),

SND_SOC_DAPM_PGA_E("RXANCL", SND_SOC_NOPM, ARIZONA_CLK_L_ENA_SET_SHIFT,
		   0, NULL, 0, arizona_anc_ev,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_PGA_E("RXANCR", SND_SOC_NOPM, ARIZONA_CLK_R_ENA_SET_SHIFT,
		   0, NULL, 0, arizona_anc_ev,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_MUX("HPOUT1L ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[0]),
SND_SOC_DAPM_MUX("HPOUT1R ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[1]),
SND_SOC_DAPM_MUX("HPOUT2L ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[2]),
SND_SOC_DAPM_MUX("HPOUT2R ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[3]),
SND_SOC_DAPM_MUX("HPOUT3L ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[4]),
SND_SOC_DAPM_MUX("HPOUT3R ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[5]),
SND_SOC_DAPM_MUX("SPKDAT1L ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[6]),
SND_SOC_DAPM_MUX("SPKDAT1R ANC Source", SND_SOC_NOPM, 0, 0,
		 &moon_output_anc_src[7]),

SND_SOC_DAPM_AIF_OUT("AIF1TX1", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX2", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX3", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX4", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX5", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX6", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX7", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF1TX8", NULL, 0,
		     ARIZONA_AIF1_TX_ENABLES, ARIZONA_AIF1TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF1RX1", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX2", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX3", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX4", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX5", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX6", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX7", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF1RX8", NULL, 0,
		    ARIZONA_AIF1_RX_ENABLES, ARIZONA_AIF1RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX3", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX4", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX5", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX6", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX7", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX8", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX3", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX4", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX5", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX6", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX7", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX8", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("SLIMRX1", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX2", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX3", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX4", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX5", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX6", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX7", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("SLIMRX8", NULL, 0,
		    ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE,
		    ARIZONA_SLIMRX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("SLIMTX1", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX2", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX2_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX3", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX3_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX4", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX4_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX5", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX5_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX6", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX6_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX7", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX7_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("SLIMTX8", NULL, 0,
		     ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE,
		     ARIZONA_SLIMTX8_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_OUT("AIF4TX1", NULL, 0,
		     ARIZONA_AIF4_TX_ENABLES, ARIZONA_AIF4TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF4TX2", NULL, 0,
		     ARIZONA_AIF4_TX_ENABLES, ARIZONA_AIF4TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF4RX1", NULL, 0,
		    ARIZONA_AIF4_RX_ENABLES, ARIZONA_AIF4RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF4RX2", NULL, 0,
		    ARIZONA_AIF4_RX_ENABLES, ARIZONA_AIF4RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_PGA_E("OUT1L", SND_SOC_NOPM,
		   ARIZONA_OUT1L_ENA_SHIFT, 0, NULL, 0, moon_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", SND_SOC_NOPM,
		   ARIZONA_OUT1R_ENA_SHIFT, 0, NULL, 0, moon_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2L_ENA_SHIFT, 0, NULL, 0, moon_analog_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT2R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT2R_ENA_SHIFT, 0, NULL, 0, moon_analog_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT3L_ENA_SHIFT, 0, NULL, 0, moon_analog_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT3R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT3R_ENA_SHIFT, 0, NULL, 0, moon_analog_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5L", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5L_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT5R", ARIZONA_OUTPUT_ENABLES_1,
		   ARIZONA_OUT5R_ENA_SHIFT, 0, NULL, 0, arizona_out_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA("SPD1TX1", ARIZONA_SPD1_TX_CONTROL,
		   ARIZONA_SPD1_VAL1_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("SPD1TX2", ARIZONA_SPD1_TX_CONTROL,
		   ARIZONA_SPD1_VAL2_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_OUT_DRV("SPD1", ARIZONA_SPD1_TX_CONTROL,
		     ARIZONA_SPD1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_PGA("DFC1", MOON_DFC1_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC2", MOON_DFC2_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC3", MOON_DFC3_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC4", MOON_DFC4_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC5", MOON_DFC5_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC6", MOON_DFC6_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC7", MOON_DFC7_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_PGA("DFC8", MOON_DFC8_CTRL,
		 MOON_DFC1_ENA_SHIFT, 0, NULL, 0),

ARIZONA_MIXER_WIDGETS(EQ1, "EQ1"),
ARIZONA_MIXER_WIDGETS(EQ2, "EQ2"),
ARIZONA_MIXER_WIDGETS(EQ3, "EQ3"),
ARIZONA_MIXER_WIDGETS(EQ4, "EQ4"),

ARIZONA_MIXER_WIDGETS(DRC1L, "DRC1L"),
ARIZONA_MIXER_WIDGETS(DRC1R, "DRC1R"),
ARIZONA_MIXER_WIDGETS(DRC2L, "DRC2L"),
ARIZONA_MIXER_WIDGETS(DRC2R, "DRC2R"),

ARIZONA_MIXER_WIDGETS(LHPF1, "LHPF1"),
ARIZONA_MIXER_WIDGETS(LHPF2, "LHPF2"),
ARIZONA_MIXER_WIDGETS(LHPF3, "LHPF3"),
ARIZONA_MIXER_WIDGETS(LHPF4, "LHPF4"),

ARIZONA_MIXER_WIDGETS(PWM1, "PWM1"),
ARIZONA_MIXER_WIDGETS(PWM2, "PWM2"),

ARIZONA_MIXER_WIDGETS(OUT1L, "HPOUT1L"),
ARIZONA_MIXER_WIDGETS(OUT1R, "HPOUT1R"),
ARIZONA_MIXER_WIDGETS(OUT2L, "HPOUT2L"),
ARIZONA_MIXER_WIDGETS(OUT2R, "HPOUT2R"),
ARIZONA_MIXER_WIDGETS(OUT3L, "HPOUT3L"),
ARIZONA_MIXER_WIDGETS(OUT3R, "HPOUT3R"),
ARIZONA_MIXER_WIDGETS(SPKDAT1L, "SPKDAT1L"),
ARIZONA_MIXER_WIDGETS(SPKDAT1R, "SPKDAT1R"),

ARIZONA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
ARIZONA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),
ARIZONA_MIXER_WIDGETS(AIF1TX3, "AIF1TX3"),
ARIZONA_MIXER_WIDGETS(AIF1TX4, "AIF1TX4"),
ARIZONA_MIXER_WIDGETS(AIF1TX5, "AIF1TX5"),
ARIZONA_MIXER_WIDGETS(AIF1TX6, "AIF1TX6"),
ARIZONA_MIXER_WIDGETS(AIF1TX7, "AIF1TX7"),
ARIZONA_MIXER_WIDGETS(AIF1TX8, "AIF1TX8"),

ARIZONA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
ARIZONA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),
ARIZONA_MIXER_WIDGETS(AIF2TX3, "AIF2TX3"),
ARIZONA_MIXER_WIDGETS(AIF2TX4, "AIF2TX4"),
ARIZONA_MIXER_WIDGETS(AIF2TX5, "AIF2TX5"),
ARIZONA_MIXER_WIDGETS(AIF2TX6, "AIF2TX6"),
ARIZONA_MIXER_WIDGETS(AIF2TX7, "AIF2TX7"),
ARIZONA_MIXER_WIDGETS(AIF2TX8, "AIF2TX8"),

ARIZONA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
ARIZONA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

ARIZONA_MIXER_WIDGETS(AIF4TX1, "AIF4TX1"),
ARIZONA_MIXER_WIDGETS(AIF4TX2, "AIF4TX2"),

ARIZONA_MIXER_WIDGETS(SLIMTX1, "SLIMTX1"),
ARIZONA_MIXER_WIDGETS(SLIMTX2, "SLIMTX2"),
ARIZONA_MIXER_WIDGETS(SLIMTX3, "SLIMTX3"),
ARIZONA_MIXER_WIDGETS(SLIMTX4, "SLIMTX4"),
ARIZONA_MIXER_WIDGETS(SLIMTX5, "SLIMTX5"),
ARIZONA_MIXER_WIDGETS(SLIMTX6, "SLIMTX6"),
ARIZONA_MIXER_WIDGETS(SLIMTX7, "SLIMTX7"),
ARIZONA_MIXER_WIDGETS(SLIMTX8, "SLIMTX8"),

ARIZONA_MUX_WIDGETS(SPD1TX1, "SPDIFTX1"),
ARIZONA_MUX_WIDGETS(SPD1TX2, "SPDIFTX2"),

ARIZONA_MUX_WIDGETS(ASRC1IN1L, "ASRC1IN1L"),
ARIZONA_MUX_WIDGETS(ASRC1IN1R, "ASRC1IN1R"),
ARIZONA_MUX_WIDGETS(ASRC1IN2L, "ASRC1IN2L"),
ARIZONA_MUX_WIDGETS(ASRC1IN2R, "ASRC1IN2R"),
ARIZONA_MUX_WIDGETS(ASRC2IN1L, "ASRC2IN1L"),
ARIZONA_MUX_WIDGETS(ASRC2IN1R, "ASRC2IN1R"),
ARIZONA_MUX_WIDGETS(ASRC2IN2L, "ASRC2IN2L"),
ARIZONA_MUX_WIDGETS(ASRC2IN2R, "ASRC2IN2R"),


ARIZONA_DSP_WIDGETS(DSP1, "DSP1"),
ARIZONA_DSP_WIDGETS(DSP2, "DSP2"),
ARIZONA_DSP_WIDGETS(DSP3, "DSP3"),
ARIZONA_DSP_WIDGETS(DSP4, "DSP4"),
ARIZONA_DSP_WIDGETS(DSP5, "DSP5"),
ARIZONA_DSP_WIDGETS(DSP6, "DSP6"),
ARIZONA_DSP_WIDGETS(DSP7, "DSP7"),

SND_SOC_DAPM_MUX("DSP2 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &moon_memory_mux[0]),
SND_SOC_DAPM_MUX("DSP3 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &moon_memory_mux[1]),

SND_SOC_DAPM_MUX("DSP Virtual Output Mux", SND_SOC_NOPM, 0, 0,
		      &moon_dsp_output_mux[0]),

ARIZONA_MUX_WIDGETS(ISRC1DEC1, "ISRC1DEC1"),
ARIZONA_MUX_WIDGETS(ISRC1DEC2, "ISRC1DEC2"),
ARIZONA_MUX_WIDGETS(ISRC1DEC3, "ISRC1DEC3"),
ARIZONA_MUX_WIDGETS(ISRC1DEC4, "ISRC1DEC4"),

ARIZONA_MUX_WIDGETS(ISRC1INT1, "ISRC1INT1"),
ARIZONA_MUX_WIDGETS(ISRC1INT2, "ISRC1INT2"),
ARIZONA_MUX_WIDGETS(ISRC1INT3, "ISRC1INT3"),
ARIZONA_MUX_WIDGETS(ISRC1INT4, "ISRC1INT4"),

ARIZONA_MUX_WIDGETS(ISRC2DEC1, "ISRC2DEC1"),
ARIZONA_MUX_WIDGETS(ISRC2DEC2, "ISRC2DEC2"),
ARIZONA_MUX_WIDGETS(ISRC2DEC3, "ISRC2DEC3"),
ARIZONA_MUX_WIDGETS(ISRC2DEC4, "ISRC2DEC4"),

ARIZONA_MUX_WIDGETS(ISRC2INT1, "ISRC2INT1"),
ARIZONA_MUX_WIDGETS(ISRC2INT2, "ISRC2INT2"),
ARIZONA_MUX_WIDGETS(ISRC2INT3, "ISRC2INT3"),
ARIZONA_MUX_WIDGETS(ISRC2INT4, "ISRC2INT4"),

ARIZONA_MUX_WIDGETS(ISRC3DEC1, "ISRC3DEC1"),
ARIZONA_MUX_WIDGETS(ISRC3DEC2, "ISRC3DEC2"),

ARIZONA_MUX_WIDGETS(ISRC3INT1, "ISRC3INT1"),
ARIZONA_MUX_WIDGETS(ISRC3INT2, "ISRC3INT2"),

ARIZONA_MUX_WIDGETS(ISRC4DEC1, "ISRC4DEC1"),
ARIZONA_MUX_WIDGETS(ISRC4DEC2, "ISRC4DEC2"),

ARIZONA_MUX_WIDGETS(ISRC4INT1, "ISRC4INT1"),
ARIZONA_MUX_WIDGETS(ISRC4INT2, "ISRC4INT2"),

ARIZONA_MUX_WIDGETS(DFC1, "DFC1"),
ARIZONA_MUX_WIDGETS(DFC2, "DFC2"),
ARIZONA_MUX_WIDGETS(DFC3, "DFC3"),
ARIZONA_MUX_WIDGETS(DFC4, "DFC4"),
ARIZONA_MUX_WIDGETS(DFC5, "DFC5"),
ARIZONA_MUX_WIDGETS(DFC6, "DFC6"),
ARIZONA_MUX_WIDGETS(DFC7, "DFC7"),
ARIZONA_MUX_WIDGETS(DFC8, "DFC8"),

SND_SOC_DAPM_OUTPUT("HPOUT1L"),
SND_SOC_DAPM_OUTPUT("HPOUT1R"),
SND_SOC_DAPM_OUTPUT("HPOUT2L"),
SND_SOC_DAPM_OUTPUT("HPOUT2R"),
SND_SOC_DAPM_OUTPUT("HPOUT3L"),
SND_SOC_DAPM_OUTPUT("HPOUT3R"),
SND_SOC_DAPM_OUTPUT("SPKDAT1L"),
SND_SOC_DAPM_OUTPUT("SPKDAT1R"),
SND_SOC_DAPM_OUTPUT("SPDIF"),

SND_SOC_DAPM_OUTPUT("MICSUPP"),
};

#define ARIZONA_MIXER_INPUT_ROUTES(name)	\
	{ name, "Noise Generator", "Noise Generator" }, \
	{ name, "Tone Generator 1", "Tone Generator 1" }, \
	{ name, "Tone Generator 2", "Tone Generator 2" }, \
	{ name, "Haptics", "HAPTICS" }, \
	{ name, "AEC", "AEC Loopback" }, \
	{ name, "IN1L", "IN1L PGA" }, \
	{ name, "IN1R", "IN1R PGA" }, \
	{ name, "IN2L", "IN2L PGA" }, \
	{ name, "IN2R", "IN2R PGA" }, \
	{ name, "IN3L", "IN3L PGA" }, \
	{ name, "IN3R", "IN3R PGA" }, \
	{ name, "IN4L", "IN4L PGA" }, \
	{ name, "IN4R", "IN4R PGA" }, \
	{ name, "IN5L", "IN5L PGA" }, \
	{ name, "IN5R", "IN5R PGA" }, \
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF1RX3", "AIF1RX3" }, \
	{ name, "AIF1RX4", "AIF1RX4" }, \
	{ name, "AIF1RX5", "AIF1RX5" }, \
	{ name, "AIF1RX6", "AIF1RX6" }, \
	{ name, "AIF1RX7", "AIF1RX7" }, \
	{ name, "AIF1RX8", "AIF1RX8" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF2RX3", "AIF2RX3" }, \
	{ name, "AIF2RX4", "AIF2RX4" }, \
	{ name, "AIF2RX5", "AIF2RX5" }, \
	{ name, "AIF2RX6", "AIF2RX6" }, \
	{ name, "AIF2RX7", "AIF2RX7" }, \
	{ name, "AIF2RX8", "AIF2RX8" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "AIF4RX1", "AIF4RX1" }, \
	{ name, "AIF4RX2", "AIF4RX2" }, \
	{ name, "SLIMRX1", "SLIMRX1" }, \
	{ name, "SLIMRX2", "SLIMRX2" }, \
	{ name, "SLIMRX3", "SLIMRX3" }, \
	{ name, "SLIMRX4", "SLIMRX4" }, \
	{ name, "SLIMRX5", "SLIMRX5" }, \
	{ name, "SLIMRX6", "SLIMRX6" }, \
	{ name, "SLIMRX7", "SLIMRX7" }, \
	{ name, "SLIMRX8", "SLIMRX8" }, \
	{ name, "EQ1", "EQ1" }, \
	{ name, "EQ2", "EQ2" }, \
	{ name, "EQ3", "EQ3" }, \
	{ name, "EQ4", "EQ4" }, \
	{ name, "DRC1L", "DRC1L" }, \
	{ name, "DRC1R", "DRC1R" }, \
	{ name, "DRC2L", "DRC2L" }, \
	{ name, "DRC2R", "DRC2R" }, \
	{ name, "LHPF1", "LHPF1" }, \
	{ name, "LHPF2", "LHPF2" }, \
	{ name, "LHPF3", "LHPF3" }, \
	{ name, "LHPF4", "LHPF4" }, \
	{ name, "ASRC1IN1L", "ASRC1IN1L" }, \
	{ name, "ASRC1IN1R", "ASRC1IN1R" }, \
	{ name, "ASRC1IN2L", "ASRC1IN2L" }, \
	{ name, "ASRC1IN2R", "ASRC1IN2R" }, \
	{ name, "ASRC2IN1L", "ASRC2IN1L" }, \
	{ name, "ASRC2IN1R", "ASRC2IN1R" }, \
	{ name, "ASRC2IN2L", "ASRC2IN2L" }, \
	{ name, "ASRC2IN2R", "ASRC2IN2R" }, \
	{ name, "ISRC1DEC1", "ISRC1DEC1" }, \
	{ name, "ISRC1DEC2", "ISRC1DEC2" }, \
	{ name, "ISRC1DEC3", "ISRC1DEC3" }, \
	{ name, "ISRC1DEC4", "ISRC1DEC4" }, \
	{ name, "ISRC1INT1", "ISRC1INT1" }, \
	{ name, "ISRC1INT2", "ISRC1INT2" }, \
	{ name, "ISRC1INT3", "ISRC1INT3" }, \
	{ name, "ISRC1INT4", "ISRC1INT4" }, \
	{ name, "ISRC2DEC1", "ISRC2DEC1" }, \
	{ name, "ISRC2DEC2", "ISRC2DEC2" }, \
	{ name, "ISRC2DEC3", "ISRC2DEC3" }, \
	{ name, "ISRC2DEC4", "ISRC2DEC4" }, \
	{ name, "ISRC2INT1", "ISRC2INT1" }, \
	{ name, "ISRC2INT2", "ISRC2INT2" }, \
	{ name, "ISRC2INT3", "ISRC2INT3" }, \
	{ name, "ISRC2INT4", "ISRC2INT4" }, \
	{ name, "ISRC3DEC1", "ISRC3DEC1" }, \
	{ name, "ISRC3DEC2", "ISRC3DEC2" }, \
	{ name, "ISRC3INT1", "ISRC3INT1" }, \
	{ name, "ISRC3INT2", "ISRC3INT2" }, \
	{ name, "ISRC4DEC1", "ISRC4DEC1" }, \
	{ name, "ISRC4DEC2", "ISRC4DEC2" }, \
	{ name, "ISRC4INT1", "ISRC4INT1" }, \
	{ name, "ISRC4INT2", "ISRC4INT2" }, \
	{ name, "DSP1.1", "DSP1" }, \
	{ name, "DSP1.2", "DSP1" }, \
	{ name, "DSP1.3", "DSP1" }, \
	{ name, "DSP1.4", "DSP1" }, \
	{ name, "DSP1.5", "DSP1" }, \
	{ name, "DSP1.6", "DSP1" }, \
	{ name, "DSP2.1", "DSP2" }, \
	{ name, "DSP2.2", "DSP2" }, \
	{ name, "DSP2.3", "DSP2" }, \
	{ name, "DSP2.4", "DSP2" }, \
	{ name, "DSP2.5", "DSP2" }, \
	{ name, "DSP2.6", "DSP2" }, \
	{ name, "DSP3.1", "DSP3" }, \
	{ name, "DSP3.2", "DSP3" }, \
	{ name, "DSP3.3", "DSP3" }, \
	{ name, "DSP3.4", "DSP3" }, \
	{ name, "DSP3.5", "DSP3" }, \
	{ name, "DSP3.6", "DSP3" }, \
	{ name, "DSP4.1", "DSP4" }, \
	{ name, "DSP4.2", "DSP4" }, \
	{ name, "DSP4.3", "DSP4" }, \
	{ name, "DSP4.4", "DSP4" }, \
	{ name, "DSP4.5", "DSP4" }, \
	{ name, "DSP4.6", "DSP4" }, \
	{ name, "DSP5.1", "DSP5" }, \
	{ name, "DSP5.2", "DSP5" }, \
	{ name, "DSP5.3", "DSP5" }, \
	{ name, "DSP5.4", "DSP5" }, \
	{ name, "DSP5.5", "DSP5" }, \
	{ name, "DSP5.6", "DSP5" }, \
	{ name, "DSP6.1", "DSP6" }, \
	{ name, "DSP6.2", "DSP6" }, \
	{ name, "DSP6.3", "DSP6" }, \
	{ name, "DSP6.4", "DSP6" }, \
	{ name, "DSP6.5", "DSP6" }, \
	{ name, "DSP6.6", "DSP6" }, \
	{ name, "DSP7.1", "DSP7" }, \
	{ name, "DSP7.2", "DSP7" }, \
	{ name, "DSP7.3", "DSP7" }, \
	{ name, "DSP7.4", "DSP7" }, \
	{ name, "DSP7.5", "DSP7" }, \
	{ name, "DSP7.6", "DSP7" }, \
	{ name, "DFC1", "DFC1" }, \
	{ name, "DFC2", "DFC2" }, \
	{ name, "DFC3", "DFC3" }, \
	{ name, "DFC4", "DFC4" }, \
	{ name, "DFC5", "DFC5" }, \
	{ name, "DFC6", "DFC6" }, \
	{ name, "DFC7", "DFC7" }, \
	{ name, "DFC8", "DFC8" }

static const struct snd_soc_dapm_route moon_dapm_routes[] = {
	{ "AIF2 Capture", NULL, "DBVDD2" },
	{ "AIF2 Playback", NULL, "DBVDD2" },

	{ "AIF3 Capture", NULL, "DBVDD3" },
	{ "AIF3 Playback", NULL, "DBVDD3" },

	{ "AIF4 Capture", NULL, "DBVDD3" },
	{ "AIF4 Playback", NULL, "DBVDD3" },

	{ "OUT1L", NULL, "CPVDD" },
	{ "OUT1R", NULL, "CPVDD" },
	{ "OUT2L", NULL, "CPVDD" },
	{ "OUT2R", NULL, "CPVDD" },
	{ "OUT3L", NULL, "CPVDD" },
	{ "OUT3R", NULL, "CPVDD" },

	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
	{ "OUT2L", NULL, "SYSCLK" },
	{ "OUT2R", NULL, "SYSCLK" },
	{ "OUT3L", NULL, "SYSCLK" },
	{ "OUT5L", NULL, "SYSCLK" },
	{ "OUT5R", NULL, "SYSCLK" },

	{ "SPD1", NULL, "SYSCLK" },
	{ "SPD1", NULL, "SPD1TX1" },
	{ "SPD1", NULL, "SPD1TX2" },

	{ "IN1AL", NULL, "SYSCLK" },
	{ "IN1BL", NULL, "SYSCLK" },
	{ "IN1AR", NULL, "SYSCLK" },
	{ "IN1BR", NULL, "SYSCLK" },
	{ "IN2AL", NULL, "SYSCLK" },
	{ "IN2BL", NULL, "SYSCLK" },
	{ "IN2R", NULL, "SYSCLK" },
	{ "IN3L", NULL, "SYSCLK" },
	{ "IN3R", NULL, "SYSCLK" },
	{ "IN4L", NULL, "SYSCLK" },
	{ "IN4R", NULL, "SYSCLK" },
	{ "IN5L", NULL, "SYSCLK" },
	{ "IN5R", NULL, "SYSCLK" },

	{ "IN3L", NULL, "DBVDD4" },
	{ "IN3R", NULL, "DBVDD4" },
	{ "IN4L", NULL, "DBVDD4" },
	{ "IN4R", NULL, "DBVDD4" },
	{ "IN5L", NULL, "DBVDD4" },
	{ "IN5R", NULL, "DBVDD4" },

	{ "DSP1", NULL, "DSPCLK"},
	{ "DSP2", NULL, "DSPCLK"},
	{ "DSP3", NULL, "DSPCLK"},
	{ "DSP4", NULL, "DSPCLK"},
	{ "DSP5", NULL, "DSPCLK"},
	{ "DSP6", NULL, "DSPCLK"},
	{ "DSP7", NULL, "DSPCLK"},

	{ "MICBIAS1", NULL, "MICVDD" },
	{ "MICBIAS2", NULL, "MICVDD" },

	{ "MICBIAS1A", NULL, "MICBIAS1" },
	{ "MICBIAS1B", NULL, "MICBIAS1" },
	{ "MICBIAS1C", NULL, "MICBIAS1" },
	{ "MICBIAS1D", NULL, "MICBIAS1" },

	{ "MICBIAS2A", NULL, "MICBIAS2" },
	{ "MICBIAS2B", NULL, "MICBIAS2" },
	{ "MICBIAS2C", NULL, "MICBIAS2" },
	{ "MICBIAS2D", NULL, "MICBIAS2" },

	{ "Noise Generator", NULL, "SYSCLK" },
	{ "Tone Generator 1", NULL, "SYSCLK" },
	{ "Tone Generator 2", NULL, "SYSCLK" },

	{ "Noise Generator", NULL, "NOISE" },
	{ "Tone Generator 1", NULL, "TONE" },
	{ "Tone Generator 2", NULL, "TONE" },

	{ "AIF1 Capture", NULL, "AIF1TX1" },
	{ "AIF1 Capture", NULL, "AIF1TX2" },
	{ "AIF1 Capture", NULL, "AIF1TX3" },
	{ "AIF1 Capture", NULL, "AIF1TX4" },
	{ "AIF1 Capture", NULL, "AIF1TX5" },
	{ "AIF1 Capture", NULL, "AIF1TX6" },
	{ "AIF1 Capture", NULL, "AIF1TX7" },
	{ "AIF1 Capture", NULL, "AIF1TX8" },

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },
	{ "AIF1RX3", NULL, "AIF1 Playback" },
	{ "AIF1RX4", NULL, "AIF1 Playback" },
	{ "AIF1RX5", NULL, "AIF1 Playback" },
	{ "AIF1RX6", NULL, "AIF1 Playback" },
	{ "AIF1RX7", NULL, "AIF1 Playback" },
	{ "AIF1RX8", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },
	{ "AIF2 Capture", NULL, "AIF2TX3" },
	{ "AIF2 Capture", NULL, "AIF2TX4" },
	{ "AIF2 Capture", NULL, "AIF2TX5" },
	{ "AIF2 Capture", NULL, "AIF2TX6" },
	{ "AIF2 Capture", NULL, "AIF2TX7" },
	{ "AIF2 Capture", NULL, "AIF2TX8" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },
	{ "AIF2RX3", NULL, "AIF2 Playback" },
	{ "AIF2RX4", NULL, "AIF2 Playback" },
	{ "AIF2RX5", NULL, "AIF2 Playback" },
	{ "AIF2RX6", NULL, "AIF2 Playback" },
	{ "AIF2RX7", NULL, "AIF2 Playback" },
	{ "AIF2RX8", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

	{ "AIF4 Capture", NULL, "AIF4TX1" },
	{ "AIF4 Capture", NULL, "AIF4TX2" },

	{ "AIF4RX1", NULL, "AIF4 Playback" },
	{ "AIF4RX2", NULL, "AIF4 Playback" },

	{ "Slim1 Capture", NULL, "SLIMTX1" },
	{ "Slim1 Capture", NULL, "SLIMTX2" },
	{ "Slim1 Capture", NULL, "SLIMTX3" },
	{ "Slim1 Capture", NULL, "SLIMTX4" },

	{ "SLIMRX1", NULL, "Slim1 Playback" },
	{ "SLIMRX2", NULL, "Slim1 Playback" },
	{ "SLIMRX3", NULL, "Slim1 Playback" },
	{ "SLIMRX4", NULL, "Slim1 Playback" },

	{ "Slim2 Capture", NULL, "SLIMTX5" },
	{ "Slim2 Capture", NULL, "SLIMTX6" },

	{ "SLIMRX5", NULL, "Slim2 Playback" },
	{ "SLIMRX6", NULL, "Slim2 Playback" },

	{ "Slim3 Capture", NULL, "SLIMTX7" },
	{ "Slim3 Capture", NULL, "SLIMTX8" },

	{ "SLIMRX7", NULL, "Slim3 Playback" },
	{ "SLIMRX8", NULL, "Slim3 Playback" },

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },
	{ "AIF4 Playback", NULL, "SYSCLK" },
	{ "Slim1 Playback", NULL, "SYSCLK" },
	{ "Slim2 Playback", NULL, "SYSCLK" },
	{ "Slim3 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },
	{ "AIF4 Capture", NULL, "SYSCLK" },
	{ "Slim1 Capture", NULL, "SYSCLK" },
	{ "Slim2 Capture", NULL, "SYSCLK" },
	{ "Slim3 Capture", NULL, "SYSCLK" },

	{ "Voice Control CPU", NULL, "Voice Control DSP" },
	{ "Voice Control DSP", NULL, "DSP6" },
	{ "Voice Control CPU", NULL, "SYSCLK" },
	{ "Voice Control DSP", NULL, "SYSCLK" },

	{ "Trace CPU", NULL, "Trace DSP" },
	{ "Trace DSP", NULL, "DSP1" },
	{ "Trace CPU", NULL, "SYSCLK" },
	{ "Trace DSP", NULL, "SYSCLK" },

	{ "IN1L Mux", "A", "IN1AL" },
	{ "IN1L Mux", "B", "IN1BL" },
	{ "IN1R Mux", "A", "IN1AR" },
	{ "IN1R Mux", "B", "IN1BR" },

	{ "IN2L Mux", "A", "IN2AL" },
	{ "IN2L Mux", "B", "IN2BL" },

	{ "IN1L PGA", NULL, "IN1L Mux" },
	{ "IN1R PGA", NULL, "IN1R Mux" },

	{ "IN2L PGA", NULL, "IN2L Mux" },
	{ "IN2R PGA", NULL, "IN2R" },

	{ "IN3L PGA", NULL, "IN3L" },
	{ "IN3R PGA", NULL, "IN3R" },

	{ "IN4L PGA", NULL, "IN4L" },
	{ "IN4R PGA", NULL, "IN4R" },

	{ "IN5L PGA", NULL, "IN5L" },
	{ "IN5R PGA", NULL, "IN5R" },

	ARIZONA_MIXER_ROUTES("OUT1L", "HPOUT1L"),
	ARIZONA_MIXER_ROUTES("OUT1R", "HPOUT1R"),
	ARIZONA_MIXER_ROUTES("OUT2L", "HPOUT2L"),
	ARIZONA_MIXER_ROUTES("OUT2R", "HPOUT2R"),
	ARIZONA_MIXER_ROUTES("OUT3L", "HPOUT3L"),
	ARIZONA_MIXER_ROUTES("OUT3R", "HPOUT3R"),

	ARIZONA_MIXER_ROUTES("OUT5L", "SPKDAT1L"),
	ARIZONA_MIXER_ROUTES("OUT5R", "SPKDAT1R"),

	ARIZONA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	ARIZONA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	ARIZONA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	ARIZONA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),
	ARIZONA_MIXER_ROUTES("AIF1TX3", "AIF1TX3"),
	ARIZONA_MIXER_ROUTES("AIF1TX4", "AIF1TX4"),
	ARIZONA_MIXER_ROUTES("AIF1TX5", "AIF1TX5"),
	ARIZONA_MIXER_ROUTES("AIF1TX6", "AIF1TX6"),
	ARIZONA_MIXER_ROUTES("AIF1TX7", "AIF1TX7"),
	ARIZONA_MIXER_ROUTES("AIF1TX8", "AIF1TX8"),

	ARIZONA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	ARIZONA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),
	ARIZONA_MIXER_ROUTES("AIF2TX3", "AIF2TX3"),
	ARIZONA_MIXER_ROUTES("AIF2TX4", "AIF2TX4"),
	ARIZONA_MIXER_ROUTES("AIF2TX5", "AIF2TX5"),
	ARIZONA_MIXER_ROUTES("AIF2TX6", "AIF2TX6"),
	ARIZONA_MIXER_ROUTES("AIF2TX7", "AIF2TX7"),
	ARIZONA_MIXER_ROUTES("AIF2TX8", "AIF2TX8"),

	ARIZONA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	ARIZONA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	ARIZONA_MIXER_ROUTES("AIF4TX1", "AIF4TX1"),
	ARIZONA_MIXER_ROUTES("AIF4TX2", "AIF4TX2"),

	ARIZONA_MIXER_ROUTES("SLIMTX1", "SLIMTX1"),
	ARIZONA_MIXER_ROUTES("SLIMTX2", "SLIMTX2"),
	ARIZONA_MIXER_ROUTES("SLIMTX3", "SLIMTX3"),
	ARIZONA_MIXER_ROUTES("SLIMTX4", "SLIMTX4"),
	ARIZONA_MIXER_ROUTES("SLIMTX5", "SLIMTX5"),
	ARIZONA_MIXER_ROUTES("SLIMTX6", "SLIMTX6"),
	ARIZONA_MIXER_ROUTES("SLIMTX7", "SLIMTX7"),
	ARIZONA_MIXER_ROUTES("SLIMTX8", "SLIMTX8"),

	ARIZONA_MUX_ROUTES("SPD1TX1", "SPDIFTX1"),
	ARIZONA_MUX_ROUTES("SPD1TX2", "SPDIFTX2"),

	ARIZONA_MIXER_ROUTES("EQ1", "EQ1"),
	ARIZONA_MIXER_ROUTES("EQ2", "EQ2"),
	ARIZONA_MIXER_ROUTES("EQ3", "EQ3"),
	ARIZONA_MIXER_ROUTES("EQ4", "EQ4"),

	ARIZONA_MIXER_ROUTES("DRC1L", "DRC1L"),
	ARIZONA_MIXER_ROUTES("DRC1R", "DRC1R"),
	ARIZONA_MIXER_ROUTES("DRC2L", "DRC2L"),
	ARIZONA_MIXER_ROUTES("DRC2R", "DRC2R"),

	ARIZONA_MIXER_ROUTES("LHPF1", "LHPF1"),
	ARIZONA_MIXER_ROUTES("LHPF2", "LHPF2"),
	ARIZONA_MIXER_ROUTES("LHPF3", "LHPF3"),
	ARIZONA_MIXER_ROUTES("LHPF4", "LHPF4"),

	ARIZONA_MUX_ROUTES("ASRC1IN1L", "ASRC1IN1L"),
	ARIZONA_MUX_ROUTES("ASRC1IN1R", "ASRC1IN1R"),
	ARIZONA_MUX_ROUTES("ASRC1IN2L", "ASRC1IN2L"),
	ARIZONA_MUX_ROUTES("ASRC1IN2R", "ASRC1IN2R"),
	ARIZONA_MUX_ROUTES("ASRC2IN1L", "ASRC2IN1L"),
	ARIZONA_MUX_ROUTES("ASRC2IN1R", "ASRC2IN1R"),
	ARIZONA_MUX_ROUTES("ASRC2IN2L", "ASRC2IN2L"),
	ARIZONA_MUX_ROUTES("ASRC2IN2R", "ASRC2IN2R"),

	ARIZONA_DSP_ROUTES("DSP1"),
	ARIZONA_DSP_ROUTES("DSP2"),
	ARIZONA_DSP_ROUTES("DSP3"),
	ARIZONA_DSP_ROUTES("DSP4"),
	ARIZONA_DSP_ROUTES("DSP5"),
	ARIZONA_DSP_ROUTES("DSP6"),
	ARIZONA_DSP_ROUTES("DSP7"),

	{ "DSP2 Preloader",  NULL, "DSP2 Virtual Input" },
	{ "DSP2 Virtual Input", "Shared Memory", "DSP3" },
	{ "DSP3 Preloader", NULL, "DSP3 Virtual Input" },
	{ "DSP3 Virtual Input", "Shared Memory", "DSP2" },

	{ "DSP Virtual Output", NULL, "DSP Virtual Output Mux" },
	{ "DSP Virtual Output Mux", "DSP6", "DSP6" },
	{ "DSP Virtual Output", NULL, "SYSCLK" },

	ARIZONA_MUX_ROUTES("ISRC1INT1", "ISRC1INT1"),
	ARIZONA_MUX_ROUTES("ISRC1INT2", "ISRC1INT2"),
	ARIZONA_MUX_ROUTES("ISRC1INT3", "ISRC1INT3"),
	ARIZONA_MUX_ROUTES("ISRC1INT4", "ISRC1INT4"),

	ARIZONA_MUX_ROUTES("ISRC1DEC1", "ISRC1DEC1"),
	ARIZONA_MUX_ROUTES("ISRC1DEC2", "ISRC1DEC2"),
	ARIZONA_MUX_ROUTES("ISRC1DEC3", "ISRC1DEC3"),
	ARIZONA_MUX_ROUTES("ISRC1DEC4", "ISRC1DEC4"),

	ARIZONA_MUX_ROUTES("ISRC2INT1", "ISRC2INT1"),
	ARIZONA_MUX_ROUTES("ISRC2INT2", "ISRC2INT2"),
	ARIZONA_MUX_ROUTES("ISRC2INT3", "ISRC2INT3"),
	ARIZONA_MUX_ROUTES("ISRC2INT4", "ISRC2INT4"),

	ARIZONA_MUX_ROUTES("ISRC2DEC1", "ISRC2DEC1"),
	ARIZONA_MUX_ROUTES("ISRC2DEC2", "ISRC2DEC2"),
	ARIZONA_MUX_ROUTES("ISRC2DEC3", "ISRC2DEC3"),
	ARIZONA_MUX_ROUTES("ISRC2DEC4", "ISRC2DEC4"),

	ARIZONA_MUX_ROUTES("ISRC3INT1", "ISRC3INT1"),
	ARIZONA_MUX_ROUTES("ISRC3INT2", "ISRC3INT2"),

	ARIZONA_MUX_ROUTES("ISRC3DEC1", "ISRC3DEC1"),
	ARIZONA_MUX_ROUTES("ISRC3DEC2", "ISRC3DEC2"),

	ARIZONA_MUX_ROUTES("ISRC4INT1", "ISRC4INT1"),
	ARIZONA_MUX_ROUTES("ISRC4INT2", "ISRC4INT2"),

	ARIZONA_MUX_ROUTES("ISRC4DEC1", "ISRC4DEC1"),
	ARIZONA_MUX_ROUTES("ISRC4DEC2", "ISRC4DEC2"),

	{ "AEC Loopback", "HPOUT1L", "OUT1L" },
	{ "AEC Loopback", "HPOUT1R", "OUT1R" },
	{ "HPOUT1L", NULL, "OUT1L" },
	{ "HPOUT1R", NULL, "OUT1R" },

	{ "AEC Loopback", "HPOUT2L", "OUT2L" },
	{ "AEC Loopback", "HPOUT2R", "OUT2R" },
	{ "HPOUT2L", NULL, "OUT2L" },
	{ "HPOUT2R", NULL, "OUT2R" },

	{ "AEC Loopback", "HPOUT3L", "OUT3L" },
	{ "AEC Loopback", "HPOUT3R", "OUT3R" },
	{ "HPOUT3L", NULL, "OUT3L" },
	{ "HPOUT3R", NULL, "OUT3R" },

	{ "AEC Loopback", "SPKDAT1L", "OUT5L" },
	{ "AEC Loopback", "SPKDAT1R", "OUT5R" },
	{ "SPKDAT1L", NULL, "OUT5L" },
	{ "SPKDAT1R", NULL, "OUT5R" },

	MOON_RXANC_INPUT_ROUTES("RXANCL", "RXANCL"),
	MOON_RXANC_INPUT_ROUTES("RXANCR", "RXANCR"),

	MOON_RXANC_OUTPUT_ROUTES("OUT1L", "HPOUT1L"),
	MOON_RXANC_OUTPUT_ROUTES("OUT1R", "HPOUT1R"),
	MOON_RXANC_OUTPUT_ROUTES("OUT2L", "HPOUT2L"),
	MOON_RXANC_OUTPUT_ROUTES("OUT2R", "HPOUT2R"),
	MOON_RXANC_OUTPUT_ROUTES("OUT3L", "HPOUT3L"),
	MOON_RXANC_OUTPUT_ROUTES("OUT3R", "HPOUT3R"),
	MOON_RXANC_OUTPUT_ROUTES("OUT5L", "SPKDAT1L"),
	MOON_RXANC_OUTPUT_ROUTES("OUT5R", "SPKDAT1R"),

	{ "SPDIF", NULL, "SPD1" },

	{ "MICSUPP", NULL, "SYSCLK" },

	{ "DRC1 Signal Activity", NULL, "DRC1L" },
	{ "DRC1 Signal Activity", NULL, "DRC1R" },
	{ "DRC2 Signal Activity", NULL, "DRC2L" },
	{ "DRC2 Signal Activity", NULL, "DRC2R" },

	ARIZONA_MUX_ROUTES("DFC1", "DFC1"),
	ARIZONA_MUX_ROUTES("DFC2", "DFC2"),
	ARIZONA_MUX_ROUTES("DFC3", "DFC3"),
	ARIZONA_MUX_ROUTES("DFC4", "DFC4"),
	ARIZONA_MUX_ROUTES("DFC5", "DFC5"),
	ARIZONA_MUX_ROUTES("DFC6", "DFC6"),
	ARIZONA_MUX_ROUTES("DFC7", "DFC7"),
	ARIZONA_MUX_ROUTES("DFC8", "DFC8"),
};

static int moon_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int Fref, unsigned int Fout)
{
	struct moon_priv *moon = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case MOON_FLL1:
		return arizona_set_fll(&moon->fll[0], source, Fref, Fout);
	case MOON_FLL2:
		return arizona_set_fll(&moon->fll[1], source, Fref, Fout);
	case MOON_FLLAO:
		return arizona_set_fll_ao(&moon->fll[2], source, Fref, Fout);
	case MOON_FLL1_REFCLK:
		return arizona_set_fll_refclk(&moon->fll[0], source, Fref,
					      Fout);
	case MOON_FLL2_REFCLK:
		return arizona_set_fll_refclk(&moon->fll[1], source, Fref,
					      Fout);
	default:
		return -EINVAL;
	}
}

#define MOON_RATES SNDRV_PCM_RATE_8000_192000

#define MOON_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver moon_dai[] = {
	{
		.name = "moon-aif1",
		.id = 1,
		.base = ARIZONA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "moon-aif2",
		.id = 2,
		.base = ARIZONA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 8,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "moon-aif3",
		.id = 3,
		.base = ARIZONA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "moon-aif4",
		.id = 4,
		.base = ARIZONA_AIF4_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF4 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF4 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "moon-slim1",
		.id = 5,
		.playback = {
			.stream_name = "Slim1 Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim1 Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "moon-slim2",
		.id = 6,
		.playback = {
			.stream_name = "Slim2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "moon-slim3",
		.id = 7,
		.playback = {
			.stream_name = "Slim3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MOON_RATES,
			 .formats = MOON_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "moon-cpu6-voicectrl",
		.capture = {
			.stream_name = "Voice Control CPU",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.compress_dai = 1,
	},
	{
		.name = "moon-dsp6-voicectrl",
		.capture = {
			.stream_name = "Voice Control DSP",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
	},
	{
		.name = "moon-cpu-trace",
		.capture = {
			.stream_name = "Trace CPU",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
		.compress_dai = 1,
	},
	{
		.name = "moon-dsp-trace",
		.capture = {
			.stream_name = "Trace DSP",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MOON_RATES,
			.formats = MOON_FORMATS,
		},
	},
};

static irqreturn_t adsp2_irq(int irq, void *data)
{
	struct moon_priv *moon = data;
	int ret, avail;

	mutex_lock(&moon->compr_info.lock);

	if (!moon->compr_info.trig &&
	    moon->core.adsp[5].fw_id == 0x9000d &&
	    moon->core.adsp[5].running) {
		if (moon->core.arizona->pdata.ez2ctrl_trigger)
			moon->core.arizona->pdata.ez2ctrl_trigger();
		moon->compr_info.trig = true;
	}

	if (!moon->compr_info.allocated)
		goto out;

	ret = wm_adsp_stream_handle_irq(moon->compr_info.adsp);
	if (ret < 0) {
		dev_err(moon->core.arizona->dev,
			"Failed to capture DSP data: %d\n",
			ret);
		goto out;
	}

	moon->compr_info.total_copied += ret;

	avail = wm_adsp_stream_avail(moon->compr_info.adsp);
	if (avail > MOON_DEFAULT_FRAGMENT_SIZE)
		snd_compr_fragment_elapsed(moon->compr_info.stream);

out:
	mutex_unlock(&moon->compr_info.lock);

	return IRQ_HANDLED;
}

static irqreturn_t moon_adsp_bus_error(int irq, void *data)
{
	struct wm_adsp *adsp = (struct wm_adsp *)data;
	return wm_adsp2_bus_error(adsp);
}

static int moon_open(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);
	struct arizona *arizona = moon->core.arizona;
	int n_adsp, ret = 0;

	mutex_lock(&moon->compr_info.lock);

	if (moon->compr_info.stream) {
		ret = -EBUSY;
		goto out;
	}

	if (strcmp(rtd->codec_dai->name, "moon-dsp6-voicectrl") == 0) {
		n_adsp = 5;
	} else if (strcmp(rtd->codec_dai->name, "moon-dsp-trace") == 0) {
		n_adsp = 0;
	} else {
		dev_err(arizona->dev,
			"No suitable compressed stream for dai '%s'\n",
			rtd->codec_dai->name);
		ret = -EINVAL;
		goto out;
	}

	if (!wm_adsp_compress_supported(&moon->core.adsp[n_adsp], stream)) {
		dev_err(arizona->dev,
			"No suitable firmware for compressed stream\n");
		ret = -EINVAL;
		goto out;
	}

	moon->compr_info.adsp = &moon->core.adsp[n_adsp];
	moon->compr_info.stream = stream;
out:
	mutex_unlock(&moon->compr_info.lock);

	return ret;
}

static int moon_free(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);

	mutex_lock(&moon->compr_info.lock);

	moon->compr_info.allocated = false;
	moon->compr_info.stream = NULL;
	moon->compr_info.total_copied = 0;

	wm_adsp_stream_free(moon->compr_info.adsp);

	mutex_unlock(&moon->compr_info.lock);

	return 0;
}

static int moon_set_params(struct snd_compr_stream *stream,
			     struct snd_compr_params *params)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);
	struct arizona *arizona = moon->core.arizona;
	struct moon_compr *compr = &moon->compr_info;
	int ret = 0;

	mutex_lock(&compr->lock);

	if (!wm_adsp_format_supported(compr->adsp, stream, params)) {
		dev_err(arizona->dev,
			"Invalid params: id:%u, chan:%u,%u, rate:%u format:%u\n",
			params->codec.id, params->codec.ch_in,
			params->codec.ch_out, params->codec.sample_rate,
			params->codec.format);
		ret = -EINVAL;
		goto out;
	}

	ret = wm_adsp_stream_alloc(compr->adsp, params);
	if (ret == 0)
		compr->allocated = true;

out:
	mutex_unlock(&compr->lock);

	return ret;
}

static int moon_get_params(struct snd_compr_stream *stream,
			     struct snd_codec *params)
{
	return 0;
}

static int moon_trigger(struct snd_compr_stream *stream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);
	struct arizona *arizona = moon->core.arizona;
	int ret = 0;
	bool pending = false;

	mutex_lock(&moon->compr_info.lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = wm_adsp_stream_start(moon->compr_info.adsp);

		/**
		 * If the stream has already triggered before the stream
		 * opened better process any outstanding data
		 */
		if (moon->compr_info.trig)
			pending = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&moon->compr_info.lock);

	/*
	* Stream has already trigerred, force irq handler to run
	* by generating interrupt.
	*/
	if (pending)
		regmap_write(arizona->regmap, CLEARWATER_ADSP2_IRQ0, 0x01);


	return ret;
}

static int moon_pointer(struct snd_compr_stream *stream,
			  struct snd_compr_tstamp *tstamp)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);

	mutex_lock(&moon->compr_info.lock);
	tstamp->byte_offset = 0;
	tstamp->copied_total = moon->compr_info.total_copied;
	mutex_unlock(&moon->compr_info.lock);

	return 0;
}

static int moon_copy(struct snd_compr_stream *stream, char __user *buf,
		       size_t count)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);
	int ret;

	mutex_lock(&moon->compr_info.lock);

	if (stream->direction == SND_COMPRESS_PLAYBACK)
		ret = -EINVAL;
	else
		ret = wm_adsp_stream_read(moon->compr_info.adsp, buf, count);

	mutex_unlock(&moon->compr_info.lock);

	return ret;
}

static int moon_get_caps(struct snd_compr_stream *stream,
			   struct snd_compr_caps *caps)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct moon_priv *moon = snd_soc_codec_get_drvdata(rtd->codec);

	mutex_lock(&moon->compr_info.lock);

	memset(caps, 0, sizeof(*caps));

	caps->direction = stream->direction;
	caps->min_fragment_size = MOON_DEFAULT_FRAGMENT_SIZE;
	caps->max_fragment_size = MOON_DEFAULT_FRAGMENT_SIZE;
	caps->min_fragments = MOON_DEFAULT_FRAGMENTS;
	caps->max_fragments = MOON_DEFAULT_FRAGMENTS;

	wm_adsp_get_caps(moon->compr_info.adsp, stream, caps);

	mutex_unlock(&moon->compr_info.lock);

	return 0;
}

static int moon_get_codec_caps(struct snd_compr_stream *stream,
				 struct snd_compr_codec_caps *codec)
{
	return 0;
}

static int moon_codec_probe(struct snd_soc_codec *codec)
{
	struct moon_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;
	int ret, i, j;

	for (i = 0; i < MOON_NUM_ADSP; i++)
		wm_adsp_init_debugfs(&priv->core.adsp[i], codec);

	codec->control_data = priv->core.arizona->regmap;
	priv->core.arizona->dapm = &codec->dapm;

	arizona_init_gpio(codec);
	arizona_init_mono(codec);
	arizona_init_input(codec);

	/* Update Sample Rate 1 to 48kHz for cases when no AIF1 hw_params */
	regmap_update_bits(arizona->regmap, ARIZONA_SAMPLE_RATE_1,
			   ARIZONA_SAMPLE_RATE_1_MASK, 0x03);

	ret = snd_soc_add_codec_controls(codec, wm_adsp2v2_fw_controls, 14);
	if (ret != 0)
		return ret;

	snd_soc_dapm_disable_pin(&codec->dapm, "HAPTICS");

	priv->core.arizona->dapm = &codec->dapm;

	ret = arizona_request_irq(arizona, ARIZONA_IRQ_DSP_IRQ1,
				  "ADSP2 interrupt 1", adsp2_irq, priv);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request DSP IRQ: %d\n", ret);
		return ret;
	}

	ret = irq_set_irq_wake(arizona->irq, 1);
	if (ret)
		dev_err(arizona->dev,
			"Failed to set DSP IRQ to wake source: %d\n",
			ret);

	for (i = 0; i < MOON_NUM_ADSP; i++) {
		ret = arizona_request_irq(arizona,
				moon_adsp_bus_error_irqs[i],
				"ADSP2 bus error",
				moon_adsp_bus_error,
				&priv->core.adsp[i]);
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to request DSP Lock region IRQ: %d\n",
				ret);
			for (j = 0; j < i; j++)
				arizona_free_irq(arizona,
					moon_adsp_bus_error_irqs[j],
					&priv->core.adsp[j]);
			return ret;
		}
	}

	snd_soc_dapm_enable_pin(&codec->dapm, "DRC2 Signal Activity");

	ret = regmap_update_bits(arizona->regmap, CLEARWATER_IRQ2_MASK_9,
				 CLEARWATER_DRC2_SIG_DET_EINT2,
				 0);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to unmask DRC2 IRQ for DSP: %d\n",
			ret);
		goto err_drc;
	}

	return 0;

err_drc:
	for (i = 0; i < MOON_NUM_ADSP; i++)
		arizona_free_irq(arizona, moon_adsp_bus_error_irqs[i],
				 &priv->core.adsp[i]);

	return ret;
}

static int moon_codec_remove(struct snd_soc_codec *codec)
{
	int i;
	struct moon_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;

	for (i = 0; i < MOON_NUM_ADSP; i++)
		wm_adsp_cleanup_debugfs(&priv->core.adsp[i]);

	irq_set_irq_wake(arizona->irq, 0);
	arizona_free_irq(arizona, ARIZONA_IRQ_DSP_IRQ1, priv);
	for (i = 0; i < MOON_NUM_ADSP; i++)
		arizona_free_irq(arizona, moon_adsp_bus_error_irqs[i],
				 &priv->core.adsp[i]);
	regmap_update_bits(arizona->regmap, CLEARWATER_IRQ2_MASK_9,
			   CLEARWATER_DRC2_SIG_DET_EINT2,
			   CLEARWATER_DRC2_SIG_DET_EINT2);
	priv->core.arizona->dapm = NULL;

	return 0;
}

#define MOON_DIG_VU 0x0200

static unsigned int moon_digital_vu[] = {
	ARIZONA_DAC_DIGITAL_VOLUME_1L,
	ARIZONA_DAC_DIGITAL_VOLUME_1R,
	ARIZONA_DAC_DIGITAL_VOLUME_2L,
	ARIZONA_DAC_DIGITAL_VOLUME_2R,
	ARIZONA_DAC_DIGITAL_VOLUME_3L,
	ARIZONA_DAC_DIGITAL_VOLUME_3R,
	ARIZONA_DAC_DIGITAL_VOLUME_5L,
	ARIZONA_DAC_DIGITAL_VOLUME_5R,
};

static struct regmap *moon_get_regmap(struct device *dev)
{
	struct moon_priv *priv = dev_get_drvdata(dev);

	return priv->core.arizona->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_moon = {
	.probe = moon_codec_probe,
	.remove = moon_codec_remove,
	.get_regmap = moon_get_regmap,

	.idle_bias_off = true,

	.set_sysclk = arizona_set_sysclk,
	.set_pll = moon_set_fll,

	.controls = moon_snd_controls,
	.num_controls = ARRAY_SIZE(moon_snd_controls),
	.dapm_widgets = moon_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(moon_dapm_widgets),
	.dapm_routes = moon_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(moon_dapm_routes),
};

static struct snd_compr_ops moon_compr_ops = {
	.open = moon_open,
	.free = moon_free,
	.set_params = moon_set_params,
	.get_params = moon_get_params,
	.trigger = moon_trigger,
	.pointer = moon_pointer,
	.copy = moon_copy,
	.get_caps = moon_get_caps,
	.get_codec_caps = moon_get_codec_caps,
};

static struct snd_soc_platform_driver moon_compr_platform = {
	.compr_ops = &moon_compr_ops,
};

static int moon_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct moon_priv *moon;
	int i, ret;

	BUILD_BUG_ON(ARRAY_SIZE(moon_dai) > ARIZONA_MAX_DAI);

	moon = devm_kzalloc(&pdev->dev, sizeof(struct moon_priv),
			      GFP_KERNEL);
	if (moon == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, moon);

	/* Set of_node to parent from the SPI device to allow DAPM to
	 * locate regulator supplies */
	pdev->dev.of_node = arizona->dev->of_node;

	mutex_init(&moon->compr_info.lock);
	mutex_init(&moon->fw_lock);

	moon->core.arizona = arizona;
	moon->core.num_inputs = 10;

	for (i = 0; i < MOON_NUM_ADSP; i++) {
		moon->core.adsp[i].part = "moon";
		if (arizona->pdata.rev_specific_fw)
			moon->core.adsp[i].part_rev = 'a' + arizona->rev;
		moon->core.adsp[i].num = i + 1;
		moon->core.adsp[i].type = WMFW_ADSP2;
		moon->core.adsp[i].rev = 2;
		moon->core.adsp[i].dev = arizona->dev;
		moon->core.adsp[i].regmap = arizona->regmap_32bit;

		moon->core.adsp[i].base = wm_adsp2_control_bases[i];
		moon->core.adsp[i].mem = moon_dsp_regions[i];
		moon->core.adsp[i].num_mems
			= ARRAY_SIZE(moon_dsp1_regions);

		if (arizona->pdata.num_fw_defs[i]) {
			moon->core.adsp[i].firmwares
				= arizona->pdata.fw_defs[i];

			moon->core.adsp[i].num_firmwares
				= arizona->pdata.num_fw_defs[i];
		}

		moon->core.adsp[i].rate_put_cb =
					moon_adsp_rate_put_cb;

		moon->core.adsp[i].lock_regions = WM_ADSP2_REGION_1_9;

		ret = wm_adsp2_init(&moon->core.adsp[i], &moon->fw_lock);
		if (ret != 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(moon->fll); i++) {
		moon->fll[i].vco_mult = 3;
		moon->fll[i].min_outdiv = 3;
		moon->fll[i].max_outdiv = 3;
	}

	arizona_init_fll(arizona, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &moon->fll[0]);
	arizona_init_fll(arizona, 2, ARIZONA_FLL2_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL2_LOCK, ARIZONA_IRQ_FLL2_CLOCK_OK,
			 &moon->fll[1]);
	arizona_init_fll(arizona, 4, MOON_FLLAO_CONTROL_1 - 1,
			 MOON_IRQ_FLLAO_CLOCK_OK, MOON_IRQ_FLLAO_CLOCK_OK,
			 &moon->fll[2]);

	for (i = 0; i < ARRAY_SIZE(moon_dai); i++)
		arizona_init_dai(&moon->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(moon_digital_vu); i++)
		regmap_update_bits(arizona->regmap, moon_digital_vu[i],
				   MOON_DIG_VU, MOON_DIG_VU);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	ret = snd_soc_register_platform(&pdev->dev, &moon_compr_platform);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to register platform: %d\n",
			ret);
		goto error;
	}

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_moon,
				      moon_dai, ARRAY_SIZE(moon_dai));
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to register codec: %d\n",
			ret);
		snd_soc_unregister_platform(&pdev->dev);
		goto error;
	}

	return ret;

error:
	mutex_destroy(&moon->compr_info.lock);
	mutex_destroy(&moon->fw_lock);

	return ret;
}

static int moon_remove(struct platform_device *pdev)
{
	struct moon_priv *moon = platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	mutex_destroy(&moon->compr_info.lock);
	mutex_destroy(&moon->fw_lock);

	return 0;
}

static struct platform_driver moon_codec_driver = {
	.driver = {
		.name = "moon-codec",
		.owner = THIS_MODULE,
	},
	.probe = moon_probe,
	.remove = moon_remove,
};

module_platform_driver(moon_codec_driver);

MODULE_DESCRIPTION("ASoC MOON driver");
MODULE_AUTHOR("Nikesh Oswal <nikesh@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:moon-codec");
