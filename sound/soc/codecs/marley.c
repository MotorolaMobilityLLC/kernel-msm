/*
 * marley.c  --  ALSA SoC Audio driver for Marley class devices
 *
 * Copyright 2015 Cirrus Logic
 *
 * Author: Piotr Stankiewicz <piotrs@opensource.wolfsonmicro.com>
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
#include "marley.h"

#define MARLEY_NUM_ADSP 3

/* Number of compressed DAI hookups, each pair of DSP and dummy CPU
 * are counted as one DAI
 */
#define MARLEY_NUM_COMPR_DAI 2

#define MARLEY_FRF_COEFFICIENT_LEN 4

#define MARLEY_FLL_COUNT 1

static int marley_frf_bytes_put(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol);

#define MARLEY_FRF_BYTES(xname, xbase, xregs)			\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,	\
	.info = snd_soc_bytes_info, .get = snd_soc_bytes_get,	\
	.put = marley_frf_bytes_put, .private_value =		\
	((unsigned long)&(struct soc_bytes)			\
		{.base = xbase, .num_regs = xregs }) }

/* 2 mixer inputs with a stride of n in the register address */
#define MARLEY_MIXER_INPUTS_2_N(_reg, n)	\
	(_reg),					\
	(_reg) + (1 * (n))

/* 4 mixer inputs with a stride of n in the register address */
#define MARLEY_MIXER_INPUTS_4_N(_reg, n)		\
	MARLEY_MIXER_INPUTS_2_N(_reg, n),		\
	MARLEY_MIXER_INPUTS_2_N(_reg + (2 * n), n)

#define MARLEY_DSP_MIXER_INPUTS(_reg) \
	MARLEY_MIXER_INPUTS_4_N(_reg, 2),		\
	MARLEY_MIXER_INPUTS_4_N(_reg + 8, 2),	\
	MARLEY_MIXER_INPUTS_4_N(_reg + 16, 8),	\
	MARLEY_MIXER_INPUTS_2_N(_reg + 48, 8)

static const int marley_fx_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_EQ1MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_EQ2MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_EQ3MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_EQ4MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_DRC1LMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_DRC1RMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_DRC2LMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_DRC2RMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_HPLP1MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_HPLP2MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_HPLP3MIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_HPLP4MIX_INPUT_1_SOURCE, 2),
};

static const int marley_isrc1_fsl_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE, 8),
};

static const int marley_isrc1_fsh_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int marley_isrc2_fsl_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE, 8),
};

static const int marley_isrc2_fsh_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE, 8),
};

static const int marley_out_inputs[] = {
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_OUT1LMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_OUT1RMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_OUT4LMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_OUT5LMIX_INPUT_1_SOURCE, 2),
	MARLEY_MIXER_INPUTS_4_N(ARIZONA_OUT5RMIX_INPUT_1_SOURCE, 2),
};

static const int marley_spd1_inputs[] = {
	MARLEY_MIXER_INPUTS_2_N(ARIZONA_SPDIFTX1MIX_INPUT_1_SOURCE, 8),
};

static const int marley_dsp1_inputs[] = {
	MARLEY_DSP_MIXER_INPUTS(ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
};

static const int marley_dsp2_inputs[] = {
	MARLEY_DSP_MIXER_INPUTS(ARIZONA_DSP2LMIX_INPUT_1_SOURCE),
};

static const int marley_dsp3_inputs[] = {
	MARLEY_DSP_MIXER_INPUTS(ARIZONA_DSP3LMIX_INPUT_1_SOURCE),
};

static int marley_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

#define MARLEY_RATE_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,\
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_get_enum_double, .put = marley_rate_put, \
	.private_value = (unsigned long)&xenum }

struct marley_priv;

struct marley_compr {
#if 0
	struct wm_adsp_compr adsp_compr;
#endif
	const char *dai_name;
	bool trig;
	struct mutex trig_lock;
	struct marley_priv *priv;
};

struct marley_priv {
	struct arizona_priv core;
	struct arizona_fll fll[MARLEY_FLL_COUNT];
	struct marley_compr compr_info[MARLEY_NUM_COMPR_DAI];

	struct mutex fw_lock;
};

static const struct {
	const char *dai_name;
	int adsp_num;
} compr_dai_mapping[MARLEY_NUM_COMPR_DAI] = {
	{
		.dai_name = "marley-dsp-voicectrl",
		.adsp_num = 2,
	},
	{
		.dai_name = "marley-dsp-trace",
		.adsp_num = 0,
	},
};

static const struct wm_adsp_region marley_dsp1_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x080000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x0e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x0a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x0c0000 },
};

static const struct wm_adsp_region marley_dsp2_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x100000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x160000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x120000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x140000 },
};

static const struct wm_adsp_region marley_dsp3_regions[] = {
	{ .type = WMFW_ADSP2_PM, .base = 0x180000 },
	{ .type = WMFW_ADSP2_ZM, .base = 0x1e0000 },
	{ .type = WMFW_ADSP2_XM, .base = 0x1a0000 },
	{ .type = WMFW_ADSP2_YM, .base = 0x1c0000 },
};

static const struct wm_adsp_region *marley_dsp_regions[] = {
	marley_dsp1_regions,
	marley_dsp2_regions,
	marley_dsp3_regions,
};

static const int wm_adsp2_control_bases[] = {
	CLEARWATER_DSP1_CONFIG,
	CLEARWATER_DSP2_CONFIG,
	CLEARWATER_DSP3_CONFIG,
};

static const char * const marley_inmux_texts[] = {
	"A",
	"B",
};

static SOC_ENUM_SINGLE_DECL(marley_in1muxl_enum,
			    ARIZONA_ADC_DIGITAL_VOLUME_1L,
			    ARIZONA_IN1L_SRC_SHIFT,
			    marley_inmux_texts);

static SOC_ENUM_SINGLE_DECL(marley_in1muxr_enum,
			    ARIZONA_ADC_DIGITAL_VOLUME_1R,
			    ARIZONA_IN1R_SRC_SHIFT,
			    marley_inmux_texts);

static const struct snd_kcontrol_new marley_in1mux[2] = {
	SOC_DAPM_ENUM("IN1L Mux", marley_in1muxl_enum),
	SOC_DAPM_ENUM("IN1R Mux", marley_in1muxr_enum),
};

static const char * const marley_outdemux_texts[] = {
	"HPOUT",
	"EPOUT",
};

static int marley_put_demux(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_codec *codec = snd_soc_dapm_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int ep_sel, mux, change;
	unsigned int mask;
	int ret;

	if (ucontrol->value.enumerated.item[0] > e->items - 1)
		return -EINVAL;
	mux = ucontrol->value.enumerated.item[0];
	ep_sel = mux << e->shift_l;
	mask = e->mask << e->shift_l;

	mutex_lock_nested(&card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	change = snd_soc_test_bits(codec, e->reg, mask, ep_sel);
	if (change) {
		/* if HP detection clamp is applied while switching to HPOUT
		 * disable OUT1 and set EDRE Manual */
		if (!ep_sel && (arizona->hpdet_clamp || (arizona->hp_impedance
				<= arizona->pdata.hpdet_short_circuit_imp))) {
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_OUTPUT_ENABLES_1,
						 ARIZONA_OUT1L_ENA |
						 ARIZONA_OUT1R_ENA, 0);
			if (ret)
				dev_warn(arizona->dev,
					 "Failed to disable headphone outputs"
					 ": %d\n", ret);
		}
		if (!ep_sel && arizona->hpdet_clamp) {
			ret = regmap_write(arizona->regmap,
					   CLEARWATER_EDRE_MANUAL, 0x3);
			if (ret)
				dev_warn(arizona->dev,
					 "Failed to set EDRE Manual: %d\n",
					 ret);
		}

		ret = regmap_update_bits(arizona->regmap,
					 ARIZONA_OUTPUT_ENABLES_1,
					 ARIZONA_EP_SEL, ep_sel);
		if (ret)
			dev_err(arizona->dev, "Failed to set OUT1 demux: %d\n",
					ret);

		/* provided the switch back to EPOUT succeeded make sure OUT1
		 * is restored to a desired value (retained by arizona->hp_ena)
		 * and EDRE Manual is set to the proper value
		 * */
		if (ep_sel && !ret) {
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_OUTPUT_ENABLES_1,
						 ARIZONA_OUT1L_ENA |
						 ARIZONA_OUT1R_ENA,
						 arizona->hp_ena);
			if (ret)
				dev_warn(arizona->dev,
					 "Failed to restore earpiece outputs:"
					 " %d\n", ret);
			ret = regmap_write(arizona->regmap,
					   CLEARWATER_EDRE_MANUAL, 0);
			if (ret)
				dev_warn(arizona->dev,
					 "Failed to restore EDRE Manual: %d\n",
					 ret);
		}

	}

	mutex_unlock(&card->dapm_mutex);

	return snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, NULL);
}

static SOC_ENUM_SINGLE_DECL(marley_outdemux_enum,
			    ARIZONA_OUTPUT_ENABLES_1,
			    ARIZONA_EP_SEL_SHIFT,
			    marley_outdemux_texts);

static const struct snd_kcontrol_new marley_outdemux =
	SOC_DAPM_ENUM_EXT("OUT1 Demux", marley_outdemux_enum,
			snd_soc_dapm_get_enum_double, marley_put_demux);

static int marley_frf_bytes_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_bytes *params = (void *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret, len;
	void *data;

	len = params->num_regs * codec->component.val_bytes;

	data = kmemdup(ucontrol->value.bytes.data, len, GFP_KERNEL | GFP_DMA);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}

	mutex_lock(&arizona->reg_setting_lock);
	regmap_write(arizona->regmap, 0x80, 0x3);

	ret = regmap_raw_write(arizona->regmap, params->base, data, len);

	regmap_write(arizona->regmap, 0x80, 0x0);
	mutex_unlock(&arizona->reg_setting_lock);

out:
	kfree(data);
	return ret;
}

/* Allow the worst case number of sources (FX Rate currently) */
static unsigned int mixer_sources_cache[ARRAY_SIZE(marley_fx_inputs)];

static int marley_get_sources(unsigned int reg, const int **cur_sources,
		int *lim)
{
	int ret = 0;

	switch (reg) {
	case ARIZONA_FX_CTRL1:
		*cur_sources = marley_fx_inputs;
		*lim = ARRAY_SIZE(marley_fx_inputs);
		break;
	case ARIZONA_ISRC_1_CTRL_1:
		*cur_sources = marley_isrc1_fsh_inputs;
		*lim = ARRAY_SIZE(marley_isrc1_fsh_inputs);
		break;
	case ARIZONA_ISRC_1_CTRL_2:
		*cur_sources = marley_isrc1_fsl_inputs;
		*lim = ARRAY_SIZE(marley_isrc1_fsl_inputs);
		break;
	case ARIZONA_ISRC_2_CTRL_1:
		*cur_sources = marley_isrc2_fsh_inputs;
		*lim = ARRAY_SIZE(marley_isrc2_fsh_inputs);
		break;
	case ARIZONA_ISRC_2_CTRL_2:
		*cur_sources = marley_isrc2_fsl_inputs;
		*lim = ARRAY_SIZE(marley_isrc2_fsl_inputs);
		break;
	case ARIZONA_OUTPUT_RATE_1:
		*cur_sources = marley_out_inputs;
		*lim = ARRAY_SIZE(marley_out_inputs);
		break;
	case ARIZONA_SPD1_TX_CONTROL:
		*cur_sources = marley_spd1_inputs;
		*lim = ARRAY_SIZE(marley_spd1_inputs);
		break;
	case CLEARWATER_DSP1_CONFIG:
		*cur_sources = marley_dsp1_inputs;
		*lim = ARRAY_SIZE(marley_dsp1_inputs);
		break;
	case CLEARWATER_DSP2_CONFIG:
		*cur_sources = marley_dsp2_inputs;
		*lim = ARRAY_SIZE(marley_dsp2_inputs);
		break;
	case CLEARWATER_DSP3_CONFIG:
		*cur_sources = marley_dsp3_inputs;
		*lim = ARRAY_SIZE(marley_dsp3_inputs);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int marley_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int ret, err;
	int lim;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	struct marley_priv *marley = snd_soc_codec_get_drvdata(codec);
	struct arizona_priv *priv = &marley->core;
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

	ret = marley_get_sources((int)e->reg, &cur_sources, &lim);
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

static int marley_adsp_rate_put_cb(struct wm_adsp *adsp, unsigned int mask,
				   unsigned int val)
{
	int ret, err;
	int lim;
	const int *cur_sources;
	struct arizona *arizona = dev_get_drvdata(adsp->dev);
	unsigned int cur;

	ret = regmap_read(adsp->regmap,  adsp->base, &cur);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to read current: %d\n", ret);
		return ret;
	}

	if ((val & mask) == (cur & mask))
		return 0;

	ret = marley_get_sources(adsp->base, &cur_sources, &lim);
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

static int marley_adsp_power_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct marley_priv *marley = snd_soc_codec_get_drvdata(codec);
	struct arizona_priv *priv = &marley->core;
	struct arizona *arizona = priv->arizona;
	unsigned int freq;
	int ret;
#if 0
	int i, ret;
#endif

	ret = regmap_read(arizona->regmap, CLEARWATER_DSP_CLOCK_1, &freq);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to read CLEARWATER_DSP_CLOCK_1: %d\n", ret);
		return ret;
	}

	freq &= CLEARWATER_DSP_CLK_FREQ_LEGACY_MASK;
	freq >>= CLEARWATER_DSP_CLK_FREQ_LEGACY_SHIFT;

#if 0
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		for (i = 0; i < ARRAY_SIZE(marley->compr_info); ++i) {
			if (marley->compr_info[i].adsp_compr.dsp->num !=
			    w->shift + 1)
				continue;

			mutex_lock(&marley->compr_info[i].trig_lock);
			marley->compr_info[i].trig = false;
			mutex_unlock(&marley->compr_info[i].trig_lock);
		}
		break;
	default:
		break;
	}
#endif

	return wm_adsp2_early_event(w, kcontrol, event, freq);
}

static DECLARE_TLV_DB_SCALE(ana_tlv, 0, 100, 0);
static DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);
static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
static DECLARE_TLV_DB_SCALE(noise_tlv, -13200, 600, 0);
static DECLARE_TLV_DB_SCALE(ng_tlv, -12000, 600, 0);

#define MARLEY_NG_SRC(name, base) \
	SOC_SINGLE(name " NG OUT1L Switch",  base,  0, 1, 0), \
	SOC_SINGLE(name " NG OUT1R Switch",  base,  1, 1, 0), \
	SOC_SINGLE(name " NG SPKOUT Switch",  base,  6, 1, 0), \
	SOC_SINGLE(name " NG SPKDATL Switch", base,  8, 1, 0), \
	SOC_SINGLE(name " NG SPKDATR Switch", base,  9, 1, 0)

static int marley_cp_mode_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	unsigned int val;

	regmap_read(arizona->regmap, CLEARWATER_CP_MODE, &val);
	if (val == 0x400)
		ucontrol->value.enumerated.item[0] = 0;
	else
		ucontrol->value.enumerated.item[0] = 1;

	return 0;
}

static int marley_cp_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val = ucontrol->value.enumerated.item[0];

	if (val > e->items - 1)
		return -EINVAL;

	mutex_lock(&arizona->reg_setting_lock);
	if (val == 0) { /* Default */
		regmap_write(arizona->regmap, 0x80, 0x1);
		regmap_write(arizona->regmap, CLEARWATER_CP_MODE, 0x400);
		regmap_write(arizona->regmap, 0x80, 0x0);
	} else {/* Inverting */
		regmap_write(arizona->regmap, 0x80, 0x1);
		regmap_write(arizona->regmap, CLEARWATER_CP_MODE, 0x407);
		regmap_write(arizona->regmap, 0x80, 0x0);
	}
	mutex_unlock(&arizona->reg_setting_lock);

	return 0;
}

static const char * const marley_cp_mode_text[2] = {
	"Default", "Inverting",
};

static const struct soc_enum marley_cp_mode[] = {
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(marley_cp_mode_text),
		marley_cp_mode_text),
};

static const struct snd_kcontrol_new marley_snd_controls[] = {
SOC_ENUM("IN1 OSR", clearwater_in_dmic_osr[0]),
SOC_ENUM("IN2 OSR", clearwater_in_dmic_osr[1]),

SOC_SINGLE_RANGE_TLV("IN1L Volume", ARIZONA_IN1L_CONTROL,
		     ARIZONA_IN1L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN1R Volume", ARIZONA_IN1R_CONTROL,
		     ARIZONA_IN1R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2L Volume", ARIZONA_IN2L_CONTROL,
		     ARIZONA_IN2L_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),
SOC_SINGLE_RANGE_TLV("IN2R Volume", ARIZONA_IN2R_CONTROL,
		     ARIZONA_IN2R_PGA_VOL_SHIFT, 0x40, 0x5f, 0, ana_tlv),

SOC_ENUM("IN HPF Cutoff Frequency", arizona_in_hpf_cut_enum),

SOC_SINGLE("IN1L HPF Switch", ARIZONA_IN1L_CONTROL,
	   ARIZONA_IN1L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN1R HPF Switch", ARIZONA_IN1R_CONTROL,
	   ARIZONA_IN1R_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2L HPF Switch", ARIZONA_IN2L_CONTROL,
	   ARIZONA_IN2L_HPF_SHIFT, 1, 0),
SOC_SINGLE("IN2R HPF Switch", ARIZONA_IN2R_CONTROL,
	   ARIZONA_IN2R_HPF_SHIFT, 1, 0),

SOC_SINGLE_TLV("IN1L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1L,
	       ARIZONA_IN1L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN1R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_1R,
	       ARIZONA_IN1R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2L Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2L,
	       ARIZONA_IN2L_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("IN2R Digital Volume", ARIZONA_ADC_DIGITAL_VOLUME_2R,
	       ARIZONA_IN2R_DIG_VOL_SHIFT, 0xbf, 0, digital_tlv),

SOC_ENUM_EXT("CP Mode", marley_cp_mode[0],
			marley_cp_mode_get, marley_cp_mode_put),

SOC_ENUM("Input Ramp Up", arizona_in_vi_ramp),
SOC_ENUM("Input Ramp Down", arizona_in_vd_ramp),

MARLEY_FRF_BYTES("FRF COEFF 1L", CLEARWATER_FRF_COEFFICIENT_1L_1,
				 MARLEY_FRF_COEFFICIENT_LEN),
MARLEY_FRF_BYTES("FRF COEFF 1R", CLEARWATER_FRF_COEFFICIENT_1R_1,
				 MARLEY_FRF_COEFFICIENT_LEN),
MARLEY_FRF_BYTES("FRF COEFF 4L", MARLEY_FRF_COEFFICIENT_4L_1,
				 MARLEY_FRF_COEFFICIENT_LEN),
MARLEY_FRF_BYTES("FRF COEFF 5L", MARLEY_FRF_COEFFICIENT_5L_1,
				 MARLEY_FRF_COEFFICIENT_LEN),
MARLEY_FRF_BYTES("FRF COEFF 5R", MARLEY_FRF_COEFFICIENT_5R_1,
				 MARLEY_FRF_COEFFICIENT_LEN),

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

ARIZONA_LHPF_CONTROL("LHPF1 Coefficients", ARIZONA_HPLPF1_2),
ARIZONA_LHPF_CONTROL("LHPF2 Coefficients", ARIZONA_HPLPF2_2),
ARIZONA_LHPF_CONTROL("LHPF3 Coefficients", ARIZONA_HPLPF3_2),
ARIZONA_LHPF_CONTROL("LHPF4 Coefficients", ARIZONA_HPLPF4_2),

SOC_ENUM("LHPF1 Mode", arizona_lhpf1_mode),
SOC_ENUM("LHPF2 Mode", arizona_lhpf2_mode),
SOC_ENUM("LHPF3 Mode", arizona_lhpf3_mode),
SOC_ENUM("LHPF4 Mode", arizona_lhpf4_mode),

SOC_ENUM("Sample Rate 2", arizona_sample_rate[0]),
SOC_ENUM("Sample Rate 3", arizona_sample_rate[1]),

MARLEY_RATE_ENUM("FX Rate", arizona_fx_rate),

MARLEY_RATE_ENUM("ISRC1 FSL", arizona_isrc_fsl[0]),
MARLEY_RATE_ENUM("ISRC2 FSL", arizona_isrc_fsl[1]),
MARLEY_RATE_ENUM("ISRC1 FSH", arizona_isrc_fsh[0]),
MARLEY_RATE_ENUM("ISRC2 FSH", arizona_isrc_fsh[1]),

ARIZONA_MIXER_CONTROLS("DSP1L", ARIZONA_DSP1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP1R", ARIZONA_DSP1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP2L", ARIZONA_DSP2LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP2R", ARIZONA_DSP2RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP3L", ARIZONA_DSP3LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("DSP3R", ARIZONA_DSP3RMIX_INPUT_1_SOURCE),

SOC_SINGLE_TLV("Noise Generator Volume", CLEARWATER_COMFORT_NOISE_GENERATOR,
	       CLEARWATER_NOISE_GEN_GAIN_SHIFT, 0x16, 0, noise_tlv),

ARIZONA_MIXER_CONTROLS("OUT1L", ARIZONA_OUT1LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("OUT1R", ARIZONA_OUT1RMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKOUT", ARIZONA_OUT4LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDATL", ARIZONA_OUT5LMIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SPKDATR", ARIZONA_OUT5RMIX_INPUT_1_SOURCE),

SOC_SINGLE("OUT1 SC Protect Switch", ARIZONA_HP1_SHORT_CIRCUIT_CTRL,
	   ARIZONA_HP1_SC_ENA_SHIFT, 1, 0),

SOC_SINGLE("OUT1L ONEFLT Switch", ARIZONA_HP_TEST_CTRL_5,
				    ARIZONA_HP1L_ONEFLT_SHIFT, 1, 0),
SOC_SINGLE("OUT1R ONEFLT Switch", ARIZONA_HP_TEST_CTRL_6,
				    ARIZONA_HP1R_ONEFLT_SHIFT, 1, 0),

SOC_SINGLE("SPKDAT High Performance Switch", ARIZONA_OUTPUT_PATH_CONFIG_5L,
	   ARIZONA_OUT5_OSR_SHIFT, 1, 0),

SOC_DOUBLE_R("OUT1 Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_1L,
	     ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_MUTE_SHIFT, 1, 1),
SOC_SINGLE("Speaker Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_4L,
	   ARIZONA_OUT4L_MUTE_SHIFT, 1, 1),
SOC_DOUBLE_R("SPKDAT Digital Switch", ARIZONA_DAC_DIGITAL_VOLUME_5L,
	     ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_R_TLV("OUT1 Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_1L,
		 ARIZONA_DAC_DIGITAL_VOLUME_1R, ARIZONA_OUT1L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),
SOC_SINGLE_TLV("Speaker Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_4L,
	       ARIZONA_OUT4L_VOL_SHIFT, 0xbf, 0, digital_tlv),
SOC_DOUBLE_R_TLV("SPKDAT Digital Volume", ARIZONA_DAC_DIGITAL_VOLUME_5L,
		 ARIZONA_DAC_DIGITAL_VOLUME_5R, ARIZONA_OUT5L_VOL_SHIFT,
		 0xbf, 0, digital_tlv),

SOC_DOUBLE("SPKDAT Switch", ARIZONA_PDM_SPK1_CTRL_1, ARIZONA_SPK1L_MUTE_SHIFT,
	   ARIZONA_SPK1R_MUTE_SHIFT, 1, 1),

SOC_DOUBLE_EXT("OUT1 DRE Switch", ARIZONA_DRE_ENABLE,
	   ARIZONA_DRE1L_ENA_SHIFT, ARIZONA_DRE1R_ENA_SHIFT, 1, 0,
	   snd_soc_get_volsw, clearwater_put_dre),

SOC_DOUBLE("OUT1 EDRE Switch", CLEARWATER_EDRE_ENABLE,
	   CLEARWATER_EDRE_OUT1L_THR1_ENA_SHIFT,
	   CLEARWATER_EDRE_OUT1R_THR1_ENA_SHIFT, 1, 0),

SOC_SINGLE("Speaker THR1 EDRE Switch", CLEARWATER_EDRE_ENABLE,
	   CLEARWATER_EDRE_OUT4L_THR1_ENA_SHIFT, 1, 0),

SOC_ENUM("Output Ramp Up", arizona_out_vi_ramp),
SOC_ENUM("Output Ramp Down", arizona_out_vd_ramp),

MARLEY_RATE_ENUM("SPDIF Rate", arizona_spdif_rate),

SOC_SINGLE("Noise Gate Switch", ARIZONA_NOISE_GATE_CONTROL,
	   ARIZONA_NGATE_ENA_SHIFT, 1, 0),
SOC_SINGLE_TLV("Noise Gate Threshold Volume", ARIZONA_NOISE_GATE_CONTROL,
	       ARIZONA_NGATE_THR_SHIFT, 7, 1, ng_tlv),
SOC_ENUM("Noise Gate Hold", arizona_ng_hold),

MARLEY_RATE_ENUM("Output Rate 1", arizona_output_rate),
SOC_ENUM("In Rate", arizona_input_rate),

MARLEY_NG_SRC("OUT1L", ARIZONA_NOISE_GATE_SELECT_1L),
MARLEY_NG_SRC("OUT1R", ARIZONA_NOISE_GATE_SELECT_1R),
MARLEY_NG_SRC("SPKOUT", ARIZONA_NOISE_GATE_SELECT_4L),
MARLEY_NG_SRC("SPKDATL", ARIZONA_NOISE_GATE_SELECT_5L),
MARLEY_NG_SRC("SPKDATR", ARIZONA_NOISE_GATE_SELECT_5R),

ARIZONA_MIXER_CONTROLS("AIF1TX1", ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX2", ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX3", ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX4", ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX5", ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF1TX6", ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF2TX1", ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF2TX2", ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("AIF3TX1", ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("AIF3TX2", ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE),

ARIZONA_MIXER_CONTROLS("SLIMTX1", ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX2", ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX3", ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX4", ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX5", ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE),
ARIZONA_MIXER_CONTROLS("SLIMTX6", ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE),

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

CLEARWATER_MIXER_ENUMS(PWM1, ARIZONA_PWM1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(PWM2, ARIZONA_PWM2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(OUT1L, ARIZONA_OUT1LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(OUT1R, ARIZONA_OUT1RMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SPKOUT, ARIZONA_OUT4LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SPKDAT1L, ARIZONA_OUT5LMIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SPKDAT1R, ARIZONA_OUT5RMIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF1TX1, ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX2, ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX3, ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX4, ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX5, ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF1TX6, ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF2TX1, ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF2TX2, ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(AIF3TX1, ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(AIF3TX2, ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE);

CLEARWATER_MIXER_ENUMS(SLIMTX1, ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX2, ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX3, ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX4, ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX5, ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE);
CLEARWATER_MIXER_ENUMS(SLIMTX6, ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE);

CLEARWATER_MUX_ENUMS(SPD1TX1, ARIZONA_SPDIFTX1MIX_INPUT_1_SOURCE);
CLEARWATER_MUX_ENUMS(SPD1TX2, ARIZONA_SPDIFTX2MIX_INPUT_1_SOURCE);

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

static const char * const marley_dsp_output_texts[] = {
	"None",
	"DSP3",
};

static const struct soc_enum marley_dsp_output_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(marley_dsp_output_texts),
			marley_dsp_output_texts);

static const struct snd_kcontrol_new marley_dsp_output_mux[] = {
	SOC_DAPM_ENUM("DSP Virtual Output Mux", marley_dsp_output_enum),
};

static const char * const marley_memory_mux_texts[] = {
	"None",
	"Shared Memory",
};

static const struct soc_enum marley_memory_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(marley_memory_mux_texts),
			marley_memory_mux_texts);

static const struct snd_kcontrol_new marley_memory_mux[] = {
	SOC_DAPM_ENUM("DSP2 Virtual Input", marley_memory_enum),
	SOC_DAPM_ENUM("DSP3 Virtual Input", marley_memory_enum),
};

static const char * const marley_aec_loopback_texts[] = {
	"OUT1L", "OUT1R", "SPKOUT", "SPKDATL", "SPKDATR",
};

static const unsigned int marley_aec_loopback_values[] = {
	0, 1, 6, 8, 9,
};

static const struct soc_enum marley_aec_loopback =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DAC_AEC_CONTROL_1,
			      ARIZONA_AEC_LOOPBACK_SRC_SHIFT, 0xf,
			      ARRAY_SIZE(marley_aec_loopback_texts),
			      marley_aec_loopback_texts,
			      marley_aec_loopback_values);

static const struct snd_kcontrol_new marley_aec_loopback_mux =
	SOC_DAPM_ENUM("AEC Loopback", marley_aec_loopback);

static const struct snd_soc_dapm_widget marley_dapm_widgets[] = {
SND_SOC_DAPM_SUPPLY("SYSCLK", ARIZONA_SYSTEM_CLOCK_1, ARIZONA_SYSCLK_ENA_SHIFT,
		    0, NULL, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_SUPPLY("OPCLK", ARIZONA_OUTPUT_SYSTEM_CLOCK,
		    ARIZONA_OPCLK_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("DSPCLK", CLEARWATER_DSP_CLOCK_1, 6,
		    0, NULL, 0),


SND_SOC_DAPM_REGULATOR_SUPPLY("DBVDD2", 0, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("CPVDD", 20, 0),
SND_SOC_DAPM_REGULATOR_SUPPLY("MICVDD", 0, SND_SOC_DAPM_REGULATOR_BYPASS),
SND_SOC_DAPM_REGULATOR_SUPPLY("SPKVDD", 0, 0),

SND_SOC_DAPM_SIGGEN("TONE"),
SND_SOC_DAPM_SIGGEN("NOISE"),
SND_SOC_DAPM_MIC("HAPTICS", NULL),

SND_SOC_DAPM_INPUT("IN1AL"),
SND_SOC_DAPM_INPUT("IN1AR"),
SND_SOC_DAPM_INPUT("IN1BL"),
SND_SOC_DAPM_INPUT("IN1BR"),
SND_SOC_DAPM_INPUT("IN2L"),
SND_SOC_DAPM_INPUT("IN2R"),

SND_SOC_DAPM_MUX("IN1L Mux", SND_SOC_NOPM, 0, 0, &marley_in1mux[0]),
SND_SOC_DAPM_MUX("IN1R Mux", SND_SOC_NOPM, 0, 0, &marley_in1mux[1]),

SND_SOC_DAPM_DEMUX("OUT1 Demux", SND_SOC_NOPM, 0, 0, &marley_outdemux),

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

SND_SOC_DAPM_SUPPLY("MICBIAS1", ARIZONA_MIC_BIAS_CTRL_1,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2", ARIZONA_MIC_BIAS_CTRL_2,
		    ARIZONA_MICB1_ENA_SHIFT, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("MICBIAS1A", ARIZONA_MIC_BIAS_CTRL_5,
		ARIZONA_MICB1A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS1B", ARIZONA_MIC_BIAS_CTRL_5,
		ARIZONA_MICB1B_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2A", ARIZONA_MIC_BIAS_CTRL_6,
		ARIZONA_MICB2A_ENA_SHIFT, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("MICBIAS2B", ARIZONA_MIC_BIAS_CTRL_6,
		ARIZONA_MICB2B_ENA_SHIFT, 0, NULL, 0),

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

WM_ADSP2("DSP1", 0, marley_adsp_power_ev),
WM_ADSP2("DSP2", 1, marley_adsp_power_ev),
WM_ADSP2("DSP3", 2, marley_adsp_power_ev),

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

SND_SOC_DAPM_MUX("AEC Loopback", ARIZONA_DAC_AEC_CONTROL_1,
		       ARIZONA_AEC_LOOPBACK_ENA_SHIFT, 0,
		       &marley_aec_loopback_mux),

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

SND_SOC_DAPM_AIF_OUT("AIF2TX1", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF2TX2", NULL, 0,
		     ARIZONA_AIF2_TX_ENABLES, ARIZONA_AIF2TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF2RX1", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF2RX2", NULL, 0,
		    ARIZONA_AIF2_RX_ENABLES, ARIZONA_AIF2RX2_ENA_SHIFT, 0),

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

SND_SOC_DAPM_AIF_OUT("AIF3TX1", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_OUT("AIF3TX2", NULL, 0,
		     ARIZONA_AIF3_TX_ENABLES, ARIZONA_AIF3TX2_ENA_SHIFT, 0),

SND_SOC_DAPM_AIF_IN("AIF3RX1", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX1_ENA_SHIFT, 0),
SND_SOC_DAPM_AIF_IN("AIF3RX2", NULL, 0,
		    ARIZONA_AIF3_RX_ENABLES, ARIZONA_AIF3RX2_ENA_SHIFT, 0),

SND_SOC_DAPM_PGA_E("OUT1L", SND_SOC_NOPM,
		   ARIZONA_OUT1L_ENA_SHIFT, 0, NULL, 0, clearwater_hp_ev,
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("OUT1R", SND_SOC_NOPM,
		   ARIZONA_OUT1R_ENA_SHIFT, 0, NULL, 0, clearwater_hp_ev,
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

ARIZONA_MIXER_WIDGETS(OUT1L, "OUT1L"),
ARIZONA_MIXER_WIDGETS(OUT1R, "OUT1R"),
ARIZONA_MIXER_WIDGETS(SPKOUT, "SPKOUT"),
ARIZONA_MIXER_WIDGETS(SPKDAT1L, "SPKDATL"),
ARIZONA_MIXER_WIDGETS(SPKDAT1R, "SPKDATR"),

ARIZONA_MIXER_WIDGETS(AIF1TX1, "AIF1TX1"),
ARIZONA_MIXER_WIDGETS(AIF1TX2, "AIF1TX2"),
ARIZONA_MIXER_WIDGETS(AIF1TX3, "AIF1TX3"),
ARIZONA_MIXER_WIDGETS(AIF1TX4, "AIF1TX4"),
ARIZONA_MIXER_WIDGETS(AIF1TX5, "AIF1TX5"),
ARIZONA_MIXER_WIDGETS(AIF1TX6, "AIF1TX6"),

ARIZONA_MIXER_WIDGETS(AIF2TX1, "AIF2TX1"),
ARIZONA_MIXER_WIDGETS(AIF2TX2, "AIF2TX2"),

ARIZONA_MIXER_WIDGETS(AIF3TX1, "AIF3TX1"),
ARIZONA_MIXER_WIDGETS(AIF3TX2, "AIF3TX2"),

ARIZONA_MIXER_WIDGETS(SLIMTX1, "SLIMTX1"),
ARIZONA_MIXER_WIDGETS(SLIMTX2, "SLIMTX2"),
ARIZONA_MIXER_WIDGETS(SLIMTX3, "SLIMTX3"),
ARIZONA_MIXER_WIDGETS(SLIMTX4, "SLIMTX4"),
ARIZONA_MIXER_WIDGETS(SLIMTX5, "SLIMTX5"),
ARIZONA_MIXER_WIDGETS(SLIMTX6, "SLIMTX6"),

ARIZONA_MUX_WIDGETS(SPD1TX1, "SPDIFTX1"),
ARIZONA_MUX_WIDGETS(SPD1TX2, "SPDIFTX2"),

ARIZONA_DSP_WIDGETS(DSP1, "DSP1"),
ARIZONA_DSP_WIDGETS(DSP2, "DSP2"),
ARIZONA_DSP_WIDGETS(DSP3, "DSP3"),

SND_SOC_DAPM_MUX("DSP2 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &marley_memory_mux[0]),
SND_SOC_DAPM_MUX("DSP3 Virtual Input", SND_SOC_NOPM, 0, 0,
		      &marley_memory_mux[1]),

SND_SOC_DAPM_MUX("DSP Virtual Output Mux", SND_SOC_NOPM, 0, 0,
		      &marley_dsp_output_mux[0]),

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

SND_SOC_DAPM_OUTPUT("HPOUTL"),
SND_SOC_DAPM_OUTPUT("HPOUTR"),
SND_SOC_DAPM_OUTPUT("EPOUTP"),
SND_SOC_DAPM_OUTPUT("EPOUTN"),
SND_SOC_DAPM_OUTPUT("SPKOUTN"),
SND_SOC_DAPM_OUTPUT("SPKOUTP"),
SND_SOC_DAPM_OUTPUT("SPKDATL"),
SND_SOC_DAPM_OUTPUT("SPKDATR"),
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
	{ name, "AIF1RX1", "AIF1RX1" }, \
	{ name, "AIF1RX2", "AIF1RX2" }, \
	{ name, "AIF1RX3", "AIF1RX3" }, \
	{ name, "AIF1RX4", "AIF1RX4" }, \
	{ name, "AIF1RX5", "AIF1RX5" }, \
	{ name, "AIF1RX6", "AIF1RX6" }, \
	{ name, "AIF2RX1", "AIF2RX1" }, \
	{ name, "AIF2RX2", "AIF2RX2" }, \
	{ name, "AIF3RX1", "AIF3RX1" }, \
	{ name, "AIF3RX2", "AIF3RX2" }, \
	{ name, "SLIMRX1", "SLIMRX1" }, \
	{ name, "SLIMRX2", "SLIMRX2" }, \
	{ name, "SLIMRX3", "SLIMRX3" }, \
	{ name, "SLIMRX4", "SLIMRX4" }, \
	{ name, "SLIMRX5", "SLIMRX5" }, \
	{ name, "SLIMRX6", "SLIMRX6" }, \
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
	{ name, "DSP3.6", "DSP3" }

static const struct snd_soc_dapm_route marley_dapm_routes[] = {
	{ "AIF2 Capture", NULL, "DBVDD2" },
	{ "AIF2 Playback", NULL, "DBVDD2" },

	{ "AIF3 Capture", NULL, "DBVDD2" },
	{ "AIF3 Playback", NULL, "DBVDD2" },

	{ "OUT1L", NULL, "CPVDD" },
	{ "OUT1R", NULL, "CPVDD" },

	{ "OUT4L", NULL, "SPKVDD" },

	{ "OUT1L", NULL, "SYSCLK" },
	{ "OUT1R", NULL, "SYSCLK" },
	{ "OUT4L", NULL, "SYSCLK" },
	{ "OUT5L", NULL, "SYSCLK" },
	{ "OUT5R", NULL, "SYSCLK" },

	{ "SPD1", NULL, "SYSCLK" },
	{ "SPD1", NULL, "SPD1TX1" },
	{ "SPD1", NULL, "SPD1TX2" },

	{ "IN1AL", NULL, "SYSCLK" },
	{ "IN1AR", NULL, "SYSCLK" },
	{ "IN1BL", NULL, "SYSCLK" },
	{ "IN1BR", NULL, "SYSCLK" },
	{ "IN2L", NULL, "SYSCLK" },
	{ "IN2R", NULL, "SYSCLK" },

	{ "DSP1", NULL, "DSPCLK"},
	{ "DSP2", NULL, "DSPCLK"},
	{ "DSP3", NULL, "DSPCLK"},

	{ "MICBIAS1", NULL, "MICVDD" },
	{ "MICBIAS2", NULL, "MICVDD" },

	{ "MICBIAS1A", NULL, "MICBIAS1" },
	{ "MICBIAS1B", NULL, "MICBIAS1" },
	{ "MICBIAS2A", NULL, "MICBIAS2" },
	{ "MICBIAS2B", NULL, "MICBIAS2" },

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

	{ "AIF1RX1", NULL, "AIF1 Playback" },
	{ "AIF1RX2", NULL, "AIF1 Playback" },
	{ "AIF1RX3", NULL, "AIF1 Playback" },
	{ "AIF1RX4", NULL, "AIF1 Playback" },
	{ "AIF1RX5", NULL, "AIF1 Playback" },
	{ "AIF1RX6", NULL, "AIF1 Playback" },

	{ "AIF2 Capture", NULL, "AIF2TX1" },
	{ "AIF2 Capture", NULL, "AIF2TX2" },

	{ "AIF2RX1", NULL, "AIF2 Playback" },
	{ "AIF2RX2", NULL, "AIF2 Playback" },

	{ "AIF3 Capture", NULL, "AIF3TX1" },
	{ "AIF3 Capture", NULL, "AIF3TX2" },

	{ "AIF3RX1", NULL, "AIF3 Playback" },
	{ "AIF3RX2", NULL, "AIF3 Playback" },

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

	{ "AIF1 Playback", NULL, "SYSCLK" },
	{ "AIF2 Playback", NULL, "SYSCLK" },
	{ "AIF3 Playback", NULL, "SYSCLK" },
	{ "Slim1 Playback", NULL, "SYSCLK" },
	{ "Slim2 Playback", NULL, "SYSCLK" },

	{ "AIF1 Capture", NULL, "SYSCLK" },
	{ "AIF2 Capture", NULL, "SYSCLK" },
	{ "AIF3 Capture", NULL, "SYSCLK" },
	{ "Slim1 Capture", NULL, "SYSCLK" },
	{ "Slim2 Capture", NULL, "SYSCLK" },

	{ "Voice Control CPU", NULL, "Voice Control DSP" },
	{ "Voice Control DSP", NULL, "DSP3" },
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

	{ "IN1L PGA", NULL, "IN1L Mux" },
	{ "IN1R PGA", NULL, "IN1R Mux" },

	{ "IN2L PGA", NULL, "IN2L" },
	{ "IN2R PGA", NULL, "IN2R" },

	ARIZONA_MIXER_ROUTES("OUT1L", "OUT1L"),
	ARIZONA_MIXER_ROUTES("OUT1R", "OUT1R"),

	ARIZONA_MIXER_ROUTES("OUT4L", "SPKOUT"),

	ARIZONA_MIXER_ROUTES("OUT5L", "SPKDATL"),
	ARIZONA_MIXER_ROUTES("OUT5R", "SPKDATR"),

	ARIZONA_MIXER_ROUTES("PWM1 Driver", "PWM1"),
	ARIZONA_MIXER_ROUTES("PWM2 Driver", "PWM2"),

	ARIZONA_MIXER_ROUTES("AIF1TX1", "AIF1TX1"),
	ARIZONA_MIXER_ROUTES("AIF1TX2", "AIF1TX2"),
	ARIZONA_MIXER_ROUTES("AIF1TX3", "AIF1TX3"),
	ARIZONA_MIXER_ROUTES("AIF1TX4", "AIF1TX4"),
	ARIZONA_MIXER_ROUTES("AIF1TX5", "AIF1TX5"),
	ARIZONA_MIXER_ROUTES("AIF1TX6", "AIF1TX6"),

	ARIZONA_MIXER_ROUTES("AIF2TX1", "AIF2TX1"),
	ARIZONA_MIXER_ROUTES("AIF2TX2", "AIF2TX2"),

	ARIZONA_MIXER_ROUTES("AIF3TX1", "AIF3TX1"),
	ARIZONA_MIXER_ROUTES("AIF3TX2", "AIF3TX2"),

	ARIZONA_MIXER_ROUTES("SLIMTX1", "SLIMTX1"),
	ARIZONA_MIXER_ROUTES("SLIMTX2", "SLIMTX2"),
	ARIZONA_MIXER_ROUTES("SLIMTX3", "SLIMTX3"),
	ARIZONA_MIXER_ROUTES("SLIMTX4", "SLIMTX4"),
	ARIZONA_MIXER_ROUTES("SLIMTX5", "SLIMTX5"),
	ARIZONA_MIXER_ROUTES("SLIMTX6", "SLIMTX6"),

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

	ARIZONA_DSP_ROUTES("DSP1"),
	ARIZONA_DSP_ROUTES("DSP2"),
	ARIZONA_DSP_ROUTES("DSP3"),

	{ "DSP2 Preloader",  NULL, "DSP2 Virtual Input" },
	{ "DSP2 Virtual Input", "Shared Memory", "DSP3" },
	{ "DSP3 Preloader", NULL, "DSP3 Virtual Input" },
	{ "DSP3 Virtual Input", "Shared Memory", "DSP2" },

	{ "DSP Virtual Output", NULL, "DSP Virtual Output Mux" },
	{ "DSP Virtual Output Mux", "DSP3", "DSP3" },
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

	{ "AEC Loopback", "OUT1L", "OUT1L" },
	{ "AEC Loopback", "OUT1R", "OUT1R" },
	{ "OUT1 Demux", NULL, "OUT1L" },
	{ "OUT1 Demux", NULL, "OUT1R" },

	{ "AEC Loopback", "SPKOUT", "OUT4L" },
	{ "SPKOUTN", NULL, "OUT4L" },
	{ "SPKOUTP", NULL, "OUT4L" },

	{ "HPOUTL", "HPOUT", "OUT1 Demux" },
	{ "HPOUTR", "HPOUT", "OUT1 Demux" },
	{ "EPOUTP", "EPOUT", "OUT1 Demux" },
	{ "EPOUTN", "EPOUT", "OUT1 Demux" },

	{ "AEC Loopback", "SPKDATL", "OUT5L" },
	{ "AEC Loopback", "SPKDATR", "OUT5R" },
	{ "SPKDATL", NULL, "OUT5L" },
	{ "SPKDATR", NULL, "OUT5R" },

	{ "SPDIF", NULL, "SPD1" },

	{ "MICSUPP", NULL, "SYSCLK" },

	{ "DRC1 Signal Activity", NULL, "DRC1L" },
	{ "DRC1 Signal Activity", NULL, "DRC1R" },
	{ "DRC2 Signal Activity", NULL, "DRC2L" },
	{ "DRC2 Signal Activity", NULL, "DRC2R" },
};

static int marley_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int fref, unsigned int fout)
{
	struct marley_priv *marley = snd_soc_codec_get_drvdata(codec);

	switch (fll_id) {
	case MARLEY_FLL1:
		return arizona_set_fll(&marley->fll[0], source, fref, fout);
	case MARLEY_FLL1_REFCLK:
		return arizona_set_fll_refclk(&marley->fll[0], source, fref,
					      fout);
	default:
		return -EINVAL;
	}
}

#define MARLEY_RATES SNDRV_PCM_RATE_8000_192000

#define MARLEY_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver marley_dai[] = {
	{
		.name = "marley-aif1",
		.id = 1,
		.base = ARIZONA_AIF1_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 6,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF1 Capture",
			 .channels_min = 1,
			 .channels_max = 6,
			 .rates = MARLEY_RATES,
			 .formats = MARLEY_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "marley-aif2",
		.id = 2,
		.base = ARIZONA_AIF2_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MARLEY_RATES,
			 .formats = MARLEY_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "marley-aif3",
		.id = 3,
		.base = ARIZONA_AIF3_BCLK_CTRL,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF3 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MARLEY_RATES,
			 .formats = MARLEY_FORMATS,
		 },
		.ops = &arizona_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "marley-slim1",
		.id = 5,
		.playback = {
			.stream_name = "Slim1 Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim1 Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = MARLEY_RATES,
			 .formats = MARLEY_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "marley-slim2",
		.id = 6,
		.playback = {
			.stream_name = "Slim2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.capture = {
			 .stream_name = "Slim2 Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = MARLEY_RATES,
			 .formats = MARLEY_FORMATS,
		 },
		.ops = &arizona_simple_dai_ops,
	},
	{
		.name = "marley-cpu-voicectrl",
		.capture = {
			.stream_name = "Voice Control CPU",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.compress_dai = 1,
	},
	{
		.name = "marley-dsp-voicectrl",
		.capture = {
			.stream_name = "Voice Control DSP",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
	},
	{
		.name = "marley-cpu-trace",
		.capture = {
			.stream_name = "Trace CPU",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
		.compress_dai = 1,
	},
	{
		.name = "marley-dsp-trace",
		.capture = {
			.stream_name = "Trace DSP",
			.channels_min = 2,
			.channels_max = 8,
			.rates = MARLEY_RATES,
			.formats = MARLEY_FORMATS,
		},
	},
};

#if 0
static void marley_compr_irq(struct marley_priv *marley,
			     struct marley_compr *compr)
{
	struct arizona *arizona = marley->core.arizona;
	bool trigger;
	int ret;

	ret = wm_adsp_compr_irq(&compr->adsp_compr, &trigger);
	if (ret < 0)
		return;

	if (trigger && arizona->pdata.ez2ctrl_trigger) {
		mutex_lock(&compr->trig_lock);
		if (!compr->trig) {
			compr->trig = true;

			if (arizona->pdata.ez2ctrl_trigger &&
			    wm_adsp_fw_has_voice_trig(compr->adsp_compr.dsp))
				arizona->pdata.ez2ctrl_trigger();
		}
		mutex_unlock(&compr->trig_lock);
	}
}
#endif

static irqreturn_t marley_adsp2_irq(int irq, void *data)
{
#if 0
	struct marley_priv *marley = data;
	int i;

	for (i = 0; i < ARRAY_SIZE(marley->compr_info); ++i) {
		if (!marley->compr_info[i].adsp_compr.dsp->running)
			continue;

		marley_compr_irq(marley, &marley->compr_info[i]);
	}
#endif
	return IRQ_HANDLED;
}

#if 0
static struct marley_compr *marley_get_compr(struct snd_soc_pcm_runtime *rtd,
					     struct marley_priv *marley)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(marley->compr_info); ++i) {
		if (strcmp(rtd->codec_dai->name,
			   marley->compr_info[i].dai_name) == 0)
			return &marley->compr_info[i];
	}

	return NULL;
}

static int marley_compr_open(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct marley_priv *marley = snd_soc_codec_get_drvdata(rtd->codec);
	struct marley_compr *compr;

	compr = marley_get_compr(rtd, marley);
	if (!compr) {
		dev_err(marley->core.arizona->dev,
			"No compressed stream for dai '%s'\n",
			rtd->codec_dai->name);
		return -EINVAL;
	}

	return wm_adsp_compr_open(&compr->adsp_compr, stream);
}

static int marley_compr_trigger(struct snd_compr_stream *stream, int cmd)
{
	struct wm_adsp_compr *adsp_compr =
			(struct wm_adsp_compr *)stream->runtime->private_data;
	struct marley_compr *compr = container_of(adsp_compr,
						  struct marley_compr,
						  adsp_compr);
	struct arizona *arizona = compr->priv->core.arizona;
	int ret;

	ret = wm_adsp_compr_trigger(stream, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		if (compr->trig)
			/*
			 * If the firmware already triggered before the stream
			 * was opened trigger another interrupt so irq handler
			 * will run and process any outstanding data
			 */
			regmap_write(arizona->regmap,
				     CLEARWATER_ADSP2_IRQ0, 0x01);
		break;
	default:
		break;
	}

	return ret;
}
#endif

static int marley_codec_probe(struct snd_soc_codec *codec)
{
	struct marley_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;
	int i, ret;

	priv->core.arizona->dapm = &codec->dapm;

	arizona_init_spk(codec);
	arizona_init_gpio(codec);
	arizona_init_mono(codec);
	arizona_init_input(codec);

	/* Update Sample Rate 1 to 48kHz for cases when no AIF1 hw_params */
	regmap_update_bits(arizona->regmap, ARIZONA_SAMPLE_RATE_1,
			   ARIZONA_SAMPLE_RATE_1_MASK, 0x03);

	for (i = 0; i < MARLEY_NUM_ADSP; ++i) {
#if 0
		ret = wm_adsp2_codec_probe(&priv->core.adsp[i], codec);
		if (ret)
			return ret;
#endif
	}

	ret = snd_soc_add_codec_controls(codec, wm_adsp2v2_fw_controls, 6);
	if (ret != 0)
		return ret;

	snd_soc_dapm_disable_pin(&codec->dapm, "HAPTICS");

	priv->core.arizona->dapm = &codec->dapm;

	ret = arizona_request_irq(arizona, ARIZONA_IRQ_DSP_IRQ1,
				  "ADSP2 interrupt 1", marley_adsp2_irq, priv);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request DSP IRQ: %d\n", ret);
		return ret;
	}

	ret = irq_set_irq_wake(arizona->irq, 1);
	if (ret)
		dev_err(arizona->dev,
			"Failed to set DSP IRQ to wake source: %d\n",
			ret);

	snd_soc_dapm_enable_pin(&codec->dapm, "DRC2 Signal Activity");

	ret = regmap_update_bits(arizona->regmap, CLEARWATER_IRQ2_MASK_9,
				 CLEARWATER_DRC2_SIG_DET_EINT2,
				 0);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to unmask DRC2 IRQ for DSP: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static int marley_codec_remove(struct snd_soc_codec *codec)
{
	struct marley_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->core.arizona;
#if 0
	int i;
#endif

	irq_set_irq_wake(arizona->irq, 0);
	arizona_free_irq(arizona, ARIZONA_IRQ_DSP_IRQ1, priv);
	regmap_update_bits(arizona->regmap, CLEARWATER_IRQ2_MASK_9,
			   CLEARWATER_DRC2_SIG_DET_EINT2,
			   CLEARWATER_DRC2_SIG_DET_EINT2);

#if 0
	for (i = 0; i < MARLEY_NUM_ADSP; ++i)
		wm_adsp2_codec_remove(&priv->core.adsp[i], codec);
#endif

	priv->core.arizona->dapm = NULL;

	return 0;
}

#define MARLEY_DIG_VU 0x0200

static unsigned int marley_digital_vu[] = {
	ARIZONA_DAC_DIGITAL_VOLUME_1L,
	ARIZONA_DAC_DIGITAL_VOLUME_1R,
	ARIZONA_DAC_DIGITAL_VOLUME_4L,
	ARIZONA_DAC_DIGITAL_VOLUME_5L,
	ARIZONA_DAC_DIGITAL_VOLUME_5R,
};

static struct regmap *marley_get_regmap(struct device *dev)
{
	struct marley_priv *priv = dev_get_drvdata(dev);

	return priv->core.arizona->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_marley = {
	.probe = marley_codec_probe,
	.remove = marley_codec_remove,
	.get_regmap = marley_get_regmap,

	.idle_bias_off = true,

	.set_sysclk = arizona_set_sysclk,
	.set_pll = marley_set_fll,

	.controls = marley_snd_controls,
	.num_controls = ARRAY_SIZE(marley_snd_controls),
	.dapm_widgets = marley_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(marley_dapm_widgets),
	.dapm_routes = marley_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(marley_dapm_routes),
};


#if 0
static struct snd_compr_ops marley_compr_ops = {
	.open = marley_compr_open,
	.free = wm_adsp_compr_free,
	.set_params = wm_adsp_compr_set_params,
	.trigger = marley_compr_trigger,
	.pointer = wm_adsp_compr_pointer,
	.copy = wm_adsp_compr_copy,
	.get_caps = wm_adsp_compr_get_caps,
};

static struct snd_soc_platform_driver marley_compr_platform = {
	.compr_ops = &marley_compr_ops,
};

static void marley_init_compr_info(struct marley_priv *marley)
{
	struct wm_adsp *dsp;
	int i;

	BUILD_BUG_ON(ARRAY_SIZE(marley->compr_info) !=
		     ARRAY_SIZE(compr_dai_mapping));

	for (i = 0; i < ARRAY_SIZE(marley->compr_info); ++i) {
		marley->compr_info[i].priv = marley;
		marley->compr_info[i].dai_name =
			compr_dai_mapping[i].dai_name;

		dsp = &marley->core.adsp[compr_dai_mapping[i].adsp_num],
		wm_adsp_compr_init(dsp, &marley->compr_info[i].adsp_compr);

		mutex_init(&marley->compr_info[i].trig_lock);
	}
}

static void marley_destroy_compr_info(struct marley_priv *marley)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(marley->compr_info); ++i)
		wm_adsp_compr_destroy(&marley->compr_info[i].adsp_compr);
}
#endif

static int marley_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct marley_priv *marley;
	int i, ret;

	BUILD_BUG_ON(ARRAY_SIZE(marley_dai) > ARIZONA_MAX_DAI);

	marley = devm_kzalloc(&pdev->dev, sizeof(struct marley_priv),
			      GFP_KERNEL);
	if (marley == NULL)
		return -ENOMEM;
	platform_set_drvdata(pdev, marley);

	/* Set of_node to parent from the SPI device to allow DAPM to
	 * locate regulator supplies */
	pdev->dev.of_node = arizona->dev->of_node;

	mutex_init(&marley->fw_lock);

	marley->core.arizona = arizona;
	marley->core.num_inputs = 4;

	for (i = 0; i < MARLEY_NUM_ADSP; i++) {
		marley->core.adsp[i].part = "marley";
		if (arizona->pdata.rev_specific_fw)
			marley->core.adsp[i].part_rev = 'a' + arizona->rev;
		marley->core.adsp[i].num = i + 1;
		marley->core.adsp[i].type = WMFW_ADSP2;
		marley->core.adsp[i].rev = 1;
		marley->core.adsp[i].dev = arizona->dev;
		marley->core.adsp[i].regmap = arizona->regmap_32bit;

		marley->core.adsp[i].base = wm_adsp2_control_bases[i];
		marley->core.adsp[i].mem = marley_dsp_regions[i];
		marley->core.adsp[i].num_mems
			= ARRAY_SIZE(marley_dsp1_regions);

		if (arizona->pdata.num_fw_defs[i]) {
			marley->core.adsp[i].firmwares
				= arizona->pdata.fw_defs[i];

			marley->core.adsp[i].num_firmwares
				= arizona->pdata.num_fw_defs[i];
		}

		marley->core.adsp[i].rate_put_cb = marley_adsp_rate_put_cb;

		ret = wm_adsp2_init(&marley->core.adsp[i], &marley->fw_lock);
		if (ret != 0)
			return ret;
	}

#if 0
	marley_init_compr_info(marley);
#endif

	for (i = 0; i < ARRAY_SIZE(marley->fll); i++) {
		marley->fll[i].vco_mult = 3;
		marley->fll[i].min_outdiv = 3;
		marley->fll[i].max_outdiv = 3;
		marley->fll[i].sync_offset = 0xE;
	}

	arizona_init_fll(arizona, 1, ARIZONA_FLL1_CONTROL_1 - 1,
			 ARIZONA_IRQ_FLL1_LOCK, ARIZONA_IRQ_FLL1_CLOCK_OK,
			 &marley->fll[0]);

	for (i = 0; i < ARRAY_SIZE(marley_dai); i++)
		arizona_init_dai(&marley->core, i);

	/* Latch volume update bits */
	for (i = 0; i < ARRAY_SIZE(marley_digital_vu); i++)
		regmap_update_bits(arizona->regmap, marley_digital_vu[i],
				   MARLEY_DIG_VU, MARLEY_DIG_VU);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

#if 0
	ret = snd_soc_register_platform(&pdev->dev, &marley_compr_platform);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to register platform: %d\n",
			ret);
		goto error;
	}
#endif

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_marley,
				      marley_dai, ARRAY_SIZE(marley_dai));
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Failed to register codec: %d\n",
			ret);
		snd_soc_unregister_platform(&pdev->dev);
		goto error;
	}

	return ret;

error:
#if 0
	marley_destroy_compr_info(marley);
#endif
	mutex_destroy(&marley->fw_lock);

	return ret;
}

static int marley_remove(struct platform_device *pdev)
{
	struct marley_priv *marley = platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

#if 0
	marley_destroy_compr_info(marley);
#endif
	mutex_destroy(&marley->fw_lock);

	return 0;
}

static struct platform_driver marley_codec_driver = {
	.driver = {
		.name = "marley-codec",
		.owner = THIS_MODULE,
	},
	.probe = marley_probe,
	.remove = marley_remove,
};

module_platform_driver(marley_codec_driver);

MODULE_DESCRIPTION("ASoC Marley driver");
MODULE_AUTHOR("Piotr Stankiewicz <piotrs@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:marley-codec");
