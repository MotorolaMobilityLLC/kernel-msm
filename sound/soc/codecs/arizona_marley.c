/*
 * arizona.c - Wolfson Arizona class device shared support
 *
 * Copyright 2014 Cirrus Logic
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/gcd.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/slimbus/slimbus.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona_marley.h"

#define ARIZONA_AIF_BCLK_CTRL                   0x00
#define ARIZONA_AIF_TX_PIN_CTRL                 0x01
#define ARIZONA_AIF_RX_PIN_CTRL                 0x02
#define ARIZONA_AIF_RATE_CTRL                   0x03
#define ARIZONA_AIF_FORMAT                      0x04
#define ARIZONA_AIF_TX_BCLK_RATE                0x05
#define ARIZONA_AIF_RX_BCLK_RATE                0x06
#define ARIZONA_AIF_FRAME_CTRL_1                0x07
#define ARIZONA_AIF_FRAME_CTRL_2                0x08
#define ARIZONA_AIF_FRAME_CTRL_3                0x09
#define ARIZONA_AIF_FRAME_CTRL_4                0x0A
#define ARIZONA_AIF_FRAME_CTRL_5                0x0B
#define ARIZONA_AIF_FRAME_CTRL_6                0x0C
#define ARIZONA_AIF_FRAME_CTRL_7                0x0D
#define ARIZONA_AIF_FRAME_CTRL_8                0x0E
#define ARIZONA_AIF_FRAME_CTRL_9                0x0F
#define ARIZONA_AIF_FRAME_CTRL_10               0x10
#define ARIZONA_AIF_FRAME_CTRL_11               0x11
#define ARIZONA_AIF_FRAME_CTRL_12               0x12
#define ARIZONA_AIF_FRAME_CTRL_13               0x13
#define ARIZONA_AIF_FRAME_CTRL_14               0x14
#define ARIZONA_AIF_FRAME_CTRL_15               0x15
#define ARIZONA_AIF_FRAME_CTRL_16               0x16
#define ARIZONA_AIF_FRAME_CTRL_17               0x17
#define ARIZONA_AIF_FRAME_CTRL_18               0x18
#define ARIZONA_AIF_TX_ENABLES                  0x19
#define ARIZONA_AIF_RX_ENABLES                  0x1A
#define ARIZONA_AIF_FORCE_WRITE                 0x1B

#define ARIZONA_FLL_VCO_CORNER 141900000
#define ARIZONA_FLL_MAX_FREF   13500000
#define ARIZONA_FLL_MAX_N      1023
#define ARIZONA_FLLAO_MAX_FREF 12288000
#define ARIZONA_FLLAO_MIN_N    4
#define ARIZONA_FLLAO_MAX_N    1023
#define ARIZONA_FLLAO_MAX_FBDIV 254
#define ARIZONA_FLL_MIN_FVCO   90000000
#define ARIZONA_FLL_MAX_FRATIO 16
#define ARIZONA_FLL_MAX_REFDIV 8
#define ARIZONA_FLL_MIN_OUTDIV 2
#define ARIZONA_FLL_MAX_OUTDIV 7
#define ARIZONA_FLL_SYNC_OFFSET 0x10

#define ARIZONA_FMT_DSP_MODE_A          0
#define ARIZONA_FMT_DSP_MODE_B          1
#define ARIZONA_FMT_I2S_MODE            2
#define ARIZONA_FMT_LEFT_JUSTIFIED_MODE 3

#define arizona_fll_err(_fll, fmt, ...) \
	dev_err(_fll->arizona->dev, "FLL%d: " fmt, _fll->id, ##__VA_ARGS__)
#define arizona_fll_warn(_fll, fmt, ...) \
	dev_warn(_fll->arizona->dev, "FLL%d: " fmt, _fll->id, ##__VA_ARGS__)
#define arizona_fll_dbg(_fll, fmt, ...) \
	dev_dbg(_fll->arizona->dev, "FLL%d: " fmt, _fll->id, ##__VA_ARGS__)

#define arizona_aif_err(_dai, fmt, ...) \
	dev_err(_dai->dev, "AIF%d: " fmt, _dai->id, ##__VA_ARGS__)
#define arizona_aif_warn(_dai, fmt, ...) \
	dev_warn(_dai->dev, "AIF%d: " fmt, _dai->id, ##__VA_ARGS__)
#define arizona_aif_dbg(_dai, fmt, ...) \
	dev_dbg(_dai->dev, "AIF%d: " fmt, _dai->id, ##__VA_ARGS__)

static struct mutex slim_tx_lock;
static struct mutex slim_rx_lock;

static struct slim_device *slim_audio_dev;
static u8 slim_logic_addr;


static const int arizona_aif1_inputs[32] = {
	ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX1MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX1MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX1MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX2MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX2MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX2MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX3MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX3MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX3MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX4MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX4MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX4MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX5MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX5MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX5MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX6MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX6MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX6MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX7MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX7MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX7MIX_INPUT_4_SOURCE,
	ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE,
	ARIZONA_AIF1TX8MIX_INPUT_2_SOURCE,
	ARIZONA_AIF1TX8MIX_INPUT_3_SOURCE,
	ARIZONA_AIF1TX8MIX_INPUT_4_SOURCE,
};

static const int arizona_aif2_inputs[32] = {
	ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX1MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX1MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX1MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX2MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX2MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX2MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX3MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX3MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX3MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX3MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX4MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX4MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX4MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX4MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX5MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX5MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX5MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX5MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX6MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX6MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX6MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX6MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX7MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX7MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX7MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX7MIX_INPUT_4_SOURCE,
	ARIZONA_AIF2TX8MIX_INPUT_1_SOURCE,
	ARIZONA_AIF2TX8MIX_INPUT_2_SOURCE,
	ARIZONA_AIF2TX8MIX_INPUT_3_SOURCE,
	ARIZONA_AIF2TX8MIX_INPUT_4_SOURCE,
};

static const int arizona_aif3_inputs[8] = {
	ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE,
	ARIZONA_AIF3TX1MIX_INPUT_2_SOURCE,
	ARIZONA_AIF3TX1MIX_INPUT_3_SOURCE,
	ARIZONA_AIF3TX1MIX_INPUT_4_SOURCE,
	ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE,
	ARIZONA_AIF3TX2MIX_INPUT_2_SOURCE,
	ARIZONA_AIF3TX2MIX_INPUT_3_SOURCE,
	ARIZONA_AIF3TX2MIX_INPUT_4_SOURCE,
};

static const int arizona_aif4_inputs[8] = {
	ARIZONA_AIF4TX1MIX_INPUT_1_SOURCE,
	ARIZONA_AIF4TX1MIX_INPUT_2_SOURCE,
	ARIZONA_AIF4TX1MIX_INPUT_3_SOURCE,
	ARIZONA_AIF4TX1MIX_INPUT_4_SOURCE,
	ARIZONA_AIF4TX2MIX_INPUT_1_SOURCE,
	ARIZONA_AIF4TX2MIX_INPUT_2_SOURCE,
	ARIZONA_AIF4TX2MIX_INPUT_3_SOURCE,
	ARIZONA_AIF4TX2MIX_INPUT_4_SOURCE,
};

static unsigned int arizona_aif_sources_cache[ARRAY_SIZE(arizona_aif1_inputs)];

struct fllao_patch {
	unsigned int fin;
	unsigned int fout;
	struct reg_default *patch;
	unsigned int patch_size;
};

static struct reg_default fll_ao_32K_49M_patch[] = {
	{ MOON_FLLAO_CONTROL_2,  0x02EE },
	{ MOON_FLLAO_CONTROL_3,  0x0000 },
	{ MOON_FLLAO_CONTROL_4,  0x0001 },
	{ MOON_FLLAO_CONTROL_5,  0x0002 },
	{ MOON_FLLAO_CONTROL_6,  0x8001 },
	{ MOON_FLLAO_CONTROL_7,  0x0004 },
	{ MOON_FLLAO_CONTROL_8,  0x0077 },
	{ MOON_FLLAO_CONTROL_10, 0x06D8 },
	{ MOON_FLLAO_CONTROL_11, 0x0085 },
	{ MOON_FLLAO_CONTROL_2,  0x82EE },
};

static struct reg_default fll_ao_32K_45M_patch[] = {
	{ MOON_FLLAO_CONTROL_2,  0x02B1 },
	{ MOON_FLLAO_CONTROL_3,  0x0001 },
	{ MOON_FLLAO_CONTROL_4,  0x0010 },
	{ MOON_FLLAO_CONTROL_5,  0x0002 },
	{ MOON_FLLAO_CONTROL_6,  0x8001 },
	{ MOON_FLLAO_CONTROL_7,  0x0004 },
	{ MOON_FLLAO_CONTROL_8,  0x0077 },
	{ MOON_FLLAO_CONTROL_10, 0x06D8 },
	{ MOON_FLLAO_CONTROL_11, 0x0005 },
	{ MOON_FLLAO_CONTROL_2,  0x82B1 },
};

static const struct fllao_patch fllao_settings[] = {
	{
		.fin = 32768,
		.fout = 49152000,
		.patch = fll_ao_32K_49M_patch,
		.patch_size = ARRAY_SIZE(fll_ao_32K_49M_patch),

	},
	{
		.fin = 32768,
		.fout = 45158400,
		.patch = fll_ao_32K_45M_patch,
		.patch_size = ARRAY_SIZE(fll_ao_32K_45M_patch),
	},
};

static int arizona_get_sources(struct arizona *arizona,
			       struct snd_soc_dai *dai,
			       const int **source, int *lim)
{
	int ret = 0;

	*lim = dai->driver->playback.channels_max * 4;

	switch (dai->driver->base) {
	case ARIZONA_AIF1_BCLK_CTRL:
		*source = arizona_aif1_inputs;
		break;
	case ARIZONA_AIF2_BCLK_CTRL:
		*source = arizona_aif2_inputs;
		break;
	case ARIZONA_AIF3_BCLK_CTRL:
		*source = arizona_aif3_inputs;
		break;
	case ARIZONA_AIF4_BCLK_CTRL:
		*source = arizona_aif4_inputs;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int arizona_cache_and_clear_sources(struct arizona *arizona,
				    const int *sources,
				    unsigned int *cache,
				    int lim)
{
	int ret = 0;
	int i;

	for (i = 0; i < lim; i++)
		cache[i] = 0;

	for (i = 0; i < lim; i++) {
		ret = regmap_read(arizona->regmap,
				  sources[i],
				  &cache[i]);

		dev_dbg(arizona->dev,
			"%s addr: 0x%04x value: 0x%04x\n",
			__func__, sources[i], cache[i]);

		if (ret != 0) {
			dev_err(arizona->dev,
				"%s Failed to cache AIF:0x%04x inputs: %d\n",
				__func__, sources[i], ret);
			break;
		}

		ret = regmap_write(arizona->regmap,
				   sources[i],
				   0);

		if (ret != 0) {
			dev_err(arizona->dev,
				"%s Failed to clear AIF:0x%04x inputs: %d\n",
				__func__, sources[i], ret);
			break;
		}

	}

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_cache_and_clear_sources);

void clearwater_spin_sysclk(struct arizona *arizona)
{
	unsigned int val;
	int ret, i;

	/* Skip this if the chip is down */
	if (pm_runtime_suspended(arizona->dev))
		return;

	/*
	 * Just read a register a few times to ensure the internal
	 * oscillator sends out a few clocks.
	 */
	for (i = 0; i < 4; i++) {
		ret = regmap_read(arizona->regmap,
				  ARIZONA_SOFTWARE_RESET,
				  &val);
		if (ret != 0)
			dev_err(arizona->dev,
				"%s Failed to read register: %d (%d)\n",
				__func__, ret, i);
	}
}
EXPORT_SYMBOL_GPL(clearwater_spin_sysclk);

int arizona_restore_sources(struct arizona *arizona,
			    const int *sources,
			    unsigned int *cache,
			    int lim)
{
	int ret = 0;
	int i;

	for (i = 0; i < lim; i++) {

		dev_dbg(arizona->dev,
			"%s addr: 0x%04x value: 0x%04x\n",
			__func__, sources[i], cache[i]);

		ret = regmap_write(arizona->regmap,
				   sources[i],
				   cache[i]);

		if (ret != 0) {
			dev_err(arizona->dev,
				"%s Failed to restore AIF:0x%04x inputs: %d\n",
				__func__, sources[i], ret);
			break;
		}

	}

	return ret;

}
EXPORT_SYMBOL_GPL(arizona_restore_sources);

static int arizona_check_speaker_overheat(struct arizona *arizona,
					  bool *warn, bool *shutdown)
{
	unsigned int reg, mask_warn, mask_shutdown, val;
	int ret;

	switch (arizona->type) {
	case WM8997:
	case WM5102:
	case WM8280:
	case WM5110:
	case WM8998:
	case WM1814:
	case CS47L24:
	case WM1831:
		reg = ARIZONA_INTERRUPT_RAW_STATUS_3;
		mask_warn = ARIZONA_SPK_OVERHEAT_WARN_STS;
		mask_shutdown = ARIZONA_SPK_OVERHEAT_STS;
		break;
	default:
		reg = CLEARWATER_IRQ1_RAW_STATUS_15;
		mask_warn = CLEARWATER_SPK_OVERHEAT_WARN_STS1;
		mask_shutdown = CLEARWATER_SPK_OVERHEAT_STS1;
		break;
	}

	ret = regmap_read(arizona->regmap, reg, &val);
	if (ret) {
		dev_err(arizona->dev, "Failed to read thermal status: %d\n",
			ret);
		return ret;
	}

	*warn = val & mask_warn ? true : false;
	*shutdown = val & mask_shutdown ? true : false;
	return 0;
}

static int vegas_spk_pre_enable(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int mute_reg, mute_mask, thr2_mask;

	switch (w->shift) {
	case ARIZONA_OUT4L_ENA_SHIFT:
		mute_reg = ARIZONA_DAC_DIGITAL_VOLUME_4L;
		mute_mask = ARIZONA_OUT4L_MUTE_MASK;
		thr2_mask = CLEARWATER_EDRE_OUT4L_THR2_ENA_MASK;
		break;
	case ARIZONA_OUT4R_ENA_SHIFT:
		mute_reg = ARIZONA_DAC_DIGITAL_VOLUME_4R;
		mute_mask = ARIZONA_OUT4R_MUTE_MASK;
		thr2_mask = CLEARWATER_EDRE_OUT4R_THR2_ENA_MASK;
		break;
	default:
		return 0;
	}

	/* mute to prevent pops */
	priv->spk_mute_cache &= ~mute_mask;
	priv->spk_mute_cache |= snd_soc_read(codec, mute_reg) & mute_mask;
	snd_soc_update_bits(codec, mute_reg, mute_mask, mute_mask);

	/* disable thr2 while we enable */
	priv->spk_thr2_cache &=  ~thr2_mask;
	priv->spk_thr2_cache |=
		snd_soc_read(codec, CLEARWATER_EDRE_ENABLE) & thr2_mask;
	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE, thr2_mask,
			    0);

	return 0;
}

static int vegas_spk_post_enable(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int mute_reg, mute_mask, thr1_mask, thr2_mask, val;

	switch (w->shift) {
	case ARIZONA_OUT4L_ENA_SHIFT:
		mute_reg = ARIZONA_DAC_DIGITAL_VOLUME_4L;
		mute_mask = ARIZONA_OUT4L_MUTE_MASK;
		thr1_mask = CLEARWATER_EDRE_OUT4L_THR1_ENA_MASK;
		thr2_mask = CLEARWATER_EDRE_OUT4L_THR2_ENA_MASK;
		break;
	case ARIZONA_OUT4R_ENA_SHIFT:
		mute_reg = ARIZONA_DAC_DIGITAL_VOLUME_4R;
		mute_mask = ARIZONA_OUT4R_MUTE_MASK;
		thr1_mask = CLEARWATER_EDRE_OUT4R_THR1_ENA_MASK;
		thr2_mask = CLEARWATER_EDRE_OUT4R_THR2_ENA_MASK;
		break;
	default:
		return 0;
	}

	/* write sequencer sets OUT4R_THR2_ENA - update cache */
	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE,
			    thr2_mask, thr2_mask);

	/* restore THR2 to what it was at the start of the sequence */
	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE, thr2_mask,
			    priv->spk_thr2_cache);

	/* disable THR2 if THR1 disabled */
	val = snd_soc_read(codec, CLEARWATER_EDRE_ENABLE);
	if ((val & thr1_mask) == 0)
		snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE,
				    thr2_mask, 0);

	/* restore mute state */
	snd_soc_update_bits(codec, mute_reg, mute_mask, priv->spk_mute_cache);

	return 0;
}

static int vegas_spk_post_disable(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol,
				int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int thr2_mask;

	switch (w->shift) {
	case ARIZONA_OUT4L_ENA_SHIFT:
		thr2_mask = CLEARWATER_EDRE_OUT4L_THR2_ENA_MASK;
		break;
	case ARIZONA_OUT4R_ENA_SHIFT:
		thr2_mask = CLEARWATER_EDRE_OUT4R_THR2_ENA_MASK;
		break;
	default:
		return 0;
	}

	/* Read the current value of THR2 in to the cache so we can restore
	 * it after the write sequencer has executed
	 */
	priv->spk_thr2_cache &= ~thr2_mask;
	priv->spk_thr2_cache |=
			snd_soc_read(codec, CLEARWATER_EDRE_ENABLE) & thr2_mask;

	/* write sequencer clears OUT4R_THR2_ENA - update cache */
	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE, thr2_mask, 0);

	/* Restore the previous value after the write sequencer update */
	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE, thr2_mask,
			    priv->spk_thr2_cache);

	return 0;
}

static int arizona_spk_ev(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	bool warn, shutdown;
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (arizona->type) {
		case WM8998:
		case WM1814:
			vegas_spk_pre_enable(w, kcontrol, event);
			break;
		default:
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		ret = arizona_check_speaker_overheat(arizona, &warn, &shutdown);
		if (ret)
			return ret;

		if (shutdown) {
			dev_crit(arizona->dev,
				 "Speaker not enabled due to temperature\n");
			return -EBUSY;
		}

		regmap_update_bits_async(arizona->regmap,
					 ARIZONA_OUTPUT_ENABLES_1,
					 1 << w->shift, 1 << w->shift);

		switch (arizona->type) {
		case WM8280:
		case WM5110:
		case WM1831:
		case CS47L24:
			usleep_range(10000, 10001);
			break;
		case WM8998:
		case WM1814:
			usleep_range(10000, 10001); /* wait for wseq to end */
			vegas_spk_post_enable(w, kcontrol, event);
			break;
		default:
			break;
		};

		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_update_bits_async(arizona->regmap,
					 ARIZONA_OUTPUT_ENABLES_1,
					 1 << w->shift, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		switch (arizona->type) {
		case WM8998:
		case WM1814:
			usleep_range(5000, 5001); /* wait for wseq to end */
			vegas_spk_post_disable(w, kcontrol, event);
			break;
		default:
			break;
		}
		break;
	}

	return 0;
}

static irqreturn_t arizona_thermal_warn(int irq, void *data)
{
	struct arizona *arizona = data;
	bool warn, shutdown;
	int ret;

	ret = arizona_check_speaker_overheat(arizona, &warn, &shutdown);
	if ((ret == 0) && warn)
		dev_crit(arizona->dev, "Thermal warning\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_thermal_shutdown(int irq, void *data)
{
	struct arizona *arizona = data;
	bool warn, shutdown;
	int ret;

	ret = arizona_check_speaker_overheat(arizona, &warn, &shutdown);
	if ((ret == 0) && shutdown) {
		dev_crit(arizona->dev, "Thermal shutdown\n");
		ret = regmap_update_bits(arizona->regmap,
					 ARIZONA_OUTPUT_ENABLES_1,
					 ARIZONA_OUT4L_ENA |
					 ARIZONA_OUT4R_ENA, 0);
		if (ret != 0)
			dev_crit(arizona->dev,
				 "Failed to disable speaker outputs: %d\n",
				 ret);
	}

	return IRQ_HANDLED;
}

static const struct snd_soc_dapm_widget arizona_spkl =
	SND_SOC_DAPM_PGA_E("OUT4L", SND_SOC_NOPM,
			   ARIZONA_OUT4L_ENA_SHIFT, 0, NULL, 0, arizona_spk_ev,
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU);

static const struct snd_soc_dapm_widget arizona_spkr =
	SND_SOC_DAPM_PGA_E("OUT4R", SND_SOC_NOPM,
			   ARIZONA_OUT4R_ENA_SHIFT, 0, NULL, 0, arizona_spk_ev,
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU);

int arizona_init_spk(struct snd_soc_codec *codec)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret;

	ret = snd_soc_dapm_new_controls(&codec->dapm, &arizona_spkl, 1);
	if (ret != 0)
		return ret;

	switch (arizona->type) {
	case WM8997:
	case WM1831:
	case CS47L24:
	case CS47L35:
		break;
	default:
		ret = snd_soc_dapm_new_controls(&codec->dapm,
						&arizona_spkr, 1);
		if (ret != 0)
			return ret;
		break;
	}

	ret = arizona_request_irq(arizona, ARIZONA_IRQ_SPK_OVERHEAT_WARN,
				  "Thermal warning", arizona_thermal_warn,
				  arizona);
	if (ret != 0)
		dev_err(arizona->dev,
			"Failed to get thermal warning IRQ: %d\n",
			ret);

	ret = arizona_request_irq(arizona, ARIZONA_IRQ_SPK_OVERHEAT,
				  "Thermal shutdown", arizona_thermal_shutdown,
				  arizona);
	if (ret != 0)
		dev_err(arizona->dev,
			"Failed to get thermal shutdown IRQ: %d\n",
			ret);

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_spk);

int arizona_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct arizona_enum *arz_enum = (struct arizona_enum *)kcontrol->private_value;

	ucontrol->value.enumerated.item[0] = arz_enum->val;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_mux_get);

int arizona_mux_put(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct arizona_enum *arz_enum = (struct arizona_enum *)kcontrol->private_value;
	struct soc_enum *e = &(arz_enum->mixer_enum);
	struct snd_soc_dapm_update update;
	int mux;

	mux = ucontrol->value.enumerated.item[0];

	if (arz_enum->val == mux)
		return 0;

	arz_enum->val = mux;
	update.kcontrol = kcontrol;
	update.reg = e->reg;
	update.mask = e->mask;
	update.val = snd_soc_enum_item_to_val(e, mux);

	return snd_soc_dapm_mux_update_power(dapm, kcontrol, mux, e, &update);
}
EXPORT_SYMBOL_GPL(arizona_mux_put);

int arizona_mux_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct snd_soc_dapm_update *update = w->dapm->card->update;
	struct arizona_enum *arz_enum;
	struct soc_enum *e;
	unsigned int val, mask;
	int ret;

	arz_enum = (struct arizona_enum *)w->kcontrols[0]->private_value;
	e = &(arz_enum->mixer_enum);
	mask = e->mask << e->shift_l;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		val = snd_soc_enum_item_to_val(e, arz_enum->val);
		val <<= e->shift_l;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		val  = 0;
		break;
	case SND_SOC_DAPM_PRE_REG:
		ret = regmap_read(arizona->regmap, e->reg, &val);
		if (ret)
			return ret;
		if (val == 0)
			update->val = 0;
		mutex_lock(&arizona->rate_lock);
		return 0;
	case SND_SOC_DAPM_POST_REG:
		mutex_unlock(&arizona->rate_lock);
		return 0;
	default:
		return -EINVAL;
	}

	mutex_lock(&arizona->rate_lock);
	ret = regmap_update_bits(arizona->regmap, e->reg, mask, val);
	mutex_unlock(&arizona->rate_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_mux_event);

int arizona_adsp_power_ev(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	unsigned int v ;
	int ret;

	ret = regmap_read(arizona->regmap, ARIZONA_SYSTEM_CLOCK_1, &v);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to read SYSCLK state: %d\n", ret);
		return -EIO;
	}

	v = (v & ARIZONA_SYSCLK_FREQ_MASK) >> ARIZONA_SYSCLK_FREQ_SHIFT;

	return wm_adsp2_early_event(w, kcontrol, event, v);
}
EXPORT_SYMBOL_GPL(arizona_adsp_power_ev);

static const struct snd_soc_dapm_route arizona_mono_routes[] = {
	{ "OUT1R", NULL, "OUT1L" },
	{ "OUT2R", NULL, "OUT2L" },
	{ "OUT3R", NULL, "OUT3L" },
	{ "OUT4R", NULL, "OUT4L" },
	{ "OUT5R", NULL, "OUT5L" },
	{ "OUT6R", NULL, "OUT6L" },
};

int arizona_init_mono(struct snd_soc_codec *codec)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int i;

	for (i = 0; i < ARIZONA_MAX_OUTPUT; ++i) {
		if (arizona->pdata.out_mono[i])
			snd_soc_dapm_add_routes(&codec->dapm,
						&arizona_mono_routes[i], 1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_mono);

static const char * const arizona_dmic_refs[] = {
	"MICVDD",
	"MICBIAS1",
	"MICBIAS2",
	"MICBIAS3",
};

static const char * const marley_dmic_refs[] = {
	"MICVDD",
	"MICBIAS1B",
	"MICBIAS2A",
	"MICBIAS2B",
};

static const char * const arizona_dmic_inputs[] = {
	"IN1L",
	"IN1R",
	"IN2L",
	"IN2R",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
};

static const char * const clearwater_dmic_inputs[] = {
	"IN1L Mux",
	"IN1R",
	"IN2L Mux",
	"IN2R Mux",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
	"IN5L",
	"IN5R",
	"IN6L",
	"IN6R",
};

static const char * const marley_dmic_inputs[] = {
	"IN1L Mux",
	"IN1R Mux",
	"IN2L",
	"IN2R",
};

static const char * const moon_dmic_inputs[] = {
	"IN1L Mux",
	"IN1R Mux",
	"IN2L Mux",
	"IN2R",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
	"IN5L",
	"IN5R",
};

int arizona_init_input(struct snd_soc_codec *codec)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct arizona_pdata *pdata = &arizona->pdata;
	int i, ret;
	struct snd_soc_dapm_route routes[2];

	memset(&routes, 0, sizeof(routes));

	for (i = 0; i < priv->num_inputs / 2; ++i) {
		switch (arizona->type) {
		case CS47L35:
			routes[0].source = marley_dmic_refs[pdata->dmic_ref[i]];
			routes[1].source = marley_dmic_refs[pdata->dmic_ref[i]];
			break;
		default:
			routes[0].source =
				arizona_dmic_refs[pdata->dmic_ref[i]];
			routes[1].source =
				arizona_dmic_refs[pdata->dmic_ref[i]];
			break;
		}

		switch (arizona->type) {
		case WM5102:
		case WM5110:
		case WM8997:
		case WM8280:
		case WM8998:
		case WM1814:
		case WM1831:
		case CS47L24:
			routes[0].sink = arizona_dmic_inputs[i * 2];
			routes[1].sink = arizona_dmic_inputs[(i * 2) + 1];
			break;
		case WM8285:
		case WM1840:
			routes[0].sink = clearwater_dmic_inputs[i * 2];
			routes[1].sink = clearwater_dmic_inputs[(i * 2) + 1];
			break;
		case CS47L35:
			routes[0].sink = marley_dmic_inputs[i * 2];
			routes[1].sink = marley_dmic_inputs[(i * 2) + 1];
			break;
		default:
			routes[0].sink = moon_dmic_inputs[i * 2];
			routes[1].sink = moon_dmic_inputs[(i * 2) + 1];
			break;
		}

		ret = snd_soc_dapm_add_routes(&codec->dapm, routes, 2);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_input);

int arizona_init_gpio(struct snd_soc_codec *codec)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int i;

	switch (arizona->type) {
	case WM8280:
	case WM5110:
	case WM1831:
	case CS47L24:
		snd_soc_dapm_disable_pin(&codec->dapm, "DRC2 Signal Activity");
		break;
	default:
		break;
	}

	snd_soc_dapm_disable_pin(&codec->dapm, "DRC1 Signal Activity");

	for (i = 0; i < ARRAY_SIZE(arizona->pdata.gpio_defaults); i++) {
		switch (arizona->pdata.gpio_defaults[i] & ARIZONA_GPN_FN_MASK) {
		case ARIZONA_GP_FN_DRC1_SIGNAL_DETECT:
			snd_soc_dapm_enable_pin(&codec->dapm,
						"DRC1 Signal Activity");
			break;
		case ARIZONA_GP_FN_DRC2_SIGNAL_DETECT:
			snd_soc_dapm_enable_pin(&codec->dapm,
						"DRC2 Signal Activity");
			break;
		default:
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_gpio);

const char * const arizona_mixer_texts[ARIZONA_NUM_MIXER_INPUTS] = {
	"None",
	"Tone Generator 1",
	"Tone Generator 2",
	"Haptics",
	"AEC",
	"AEC2",
	"Mic Mute Mixer",
	"Noise Generator",
	"IN1L",
	"IN1R",
	"IN2L",
	"IN2R",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
	"IN5L",
	"IN5R",
	"IN6L",
	"IN6R",
	"AIF1RX1",
	"AIF1RX2",
	"AIF1RX3",
	"AIF1RX4",
	"AIF1RX5",
	"AIF1RX6",
	"AIF1RX7",
	"AIF1RX8",
	"AIF2RX1",
	"AIF2RX2",
	"AIF2RX3",
	"AIF2RX4",
	"AIF2RX5",
	"AIF2RX6",
	"AIF2RX7",
	"AIF2RX8",
	"AIF3RX1",
	"AIF3RX2",
	"AIF4RX1",
	"AIF4RX2",
	"SLIMRX1",
	"SLIMRX2",
	"SLIMRX3",
	"SLIMRX4",
	"SLIMRX5",
	"SLIMRX6",
	"SLIMRX7",
	"SLIMRX8",
	"EQ1",
	"EQ2",
	"EQ3",
	"EQ4",
	"DRC1L",
	"DRC1R",
	"DRC2L",
	"DRC2R",
	"LHPF1",
	"LHPF2",
	"LHPF3",
	"LHPF4",
	"DSP1.1",
	"DSP1.2",
	"DSP1.3",
	"DSP1.4",
	"DSP1.5",
	"DSP1.6",
	"DSP2.1",
	"DSP2.2",
	"DSP2.3",
	"DSP2.4",
	"DSP2.5",
	"DSP2.6",
	"DSP3.1",
	"DSP3.2",
	"DSP3.3",
	"DSP3.4",
	"DSP3.5",
	"DSP3.6",
	"DSP4.1",
	"DSP4.2",
	"DSP4.3",
	"DSP4.4",
	"DSP4.5",
	"DSP4.6",
	"DSP5.1",
	"DSP5.2",
	"DSP5.3",
	"DSP5.4",
	"DSP5.5",
	"DSP5.6",
	"DSP6.1",
	"DSP6.2",
	"DSP6.3",
	"DSP6.4",
	"DSP6.5",
	"DSP6.6",
	"DSP7.1",
	"DSP7.2",
	"DSP7.3",
	"DSP7.4",
	"DSP7.5",
	"DSP7.6",
	"ASRC1L",
	"ASRC1R",
	"ASRC2L",
	"ASRC2R",
	"ISRC1INT1",
	"ISRC1INT2",
	"ISRC1INT3",
	"ISRC1INT4",
	"ISRC1DEC1",
	"ISRC1DEC2",
	"ISRC1DEC3",
	"ISRC1DEC4",
	"ISRC2INT1",
	"ISRC2INT2",
	"ISRC2INT3",
	"ISRC2INT4",
	"ISRC2DEC1",
	"ISRC2DEC2",
	"ISRC2DEC3",
	"ISRC2DEC4",
	"ISRC3INT1",
	"ISRC3INT2",
	"ISRC3INT3",
	"ISRC3INT4",
	"ISRC3DEC1",
	"ISRC3DEC2",
	"ISRC3DEC3",
	"ISRC3DEC4",
	"ISRC4INT1",
	"ISRC4INT2",
	"ISRC4DEC1",
	"ISRC4DEC2",
};
EXPORT_SYMBOL_GPL(arizona_mixer_texts);

unsigned int arizona_mixer_values[ARIZONA_NUM_MIXER_INPUTS] = {
	0x00,  /* None */
	0x04,  /* Tone */
	0x05,
	0x06,  /* Haptics */
	0x08,  /* AEC */
	0x09,  /* AEC2 */
	0x0c,  /* Noise mixer */
	0x0d,  /* Comfort noise */
	0x10,  /* IN1L */
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,
	0x17,
	0x18,
	0x19,
	0x1A,
	0x1B,
	0x20,  /* AIF1RX1 */
	0x21,
	0x22,
	0x23,
	0x24,
	0x25,
	0x26,
	0x27,
	0x28,  /* AIF2RX1 */
	0x29,
	0x2a,
	0x2b,
	0x2c,
	0x2d,
	0x2e,
	0x2f,
	0x30,  /* AIF3RX1 */
	0x31,
	0x34,  /* AIF4RX1 */
	0x35,
	0x38,  /* SLIMRX1 */
	0x39,
	0x3a,
	0x3b,
	0x3c,
	0x3d,
	0x3e,
	0x3f,
	0x50,  /* EQ1 */
	0x51,
	0x52,
	0x53,
	0x58,  /* DRC1L */
	0x59,
	0x5a,
	0x5b,
	0x60,  /* LHPF1 */
	0x61,
	0x62,
	0x63,
	0x68,  /* DSP1.1 */
	0x69,
	0x6a,
	0x6b,
	0x6c,
	0x6d,
	0x70,  /* DSP2.1 */
	0x71,
	0x72,
	0x73,
	0x74,
	0x75,
	0x78,  /* DSP3.1 */
	0x79,
	0x7a,
	0x7b,
	0x7c,
	0x7d,
	0x80,  /* DSP4.1 */
	0x81,
	0x82,
	0x83,
	0x84,
	0x85,
	0x88,  /* DSP5.1 */
	0x89,
	0x8a,
	0x8b,
	0x8c,
	0x8d,
	0xc0,  /* DSP6.1 */
	0xc1,
	0xc2,
	0xc3,
	0xc4,
	0xc5,
	0xc8,  /* DSP7.1 */
	0xc9,
	0xca,
	0xcb,
	0xcc,
	0xcd,
	0x90,  /* ASRC1L */
	0x91,
	0x92,
	0x93,
	0xa0,  /* ISRC1INT1 */
	0xa1,
	0xa2,
	0xa3,
	0xa4,  /* ISRC1DEC1 */
	0xa5,
	0xa6,
	0xa7,
	0xa8,  /* ISRC2DEC1 */
	0xa9,
	0xaa,
	0xab,
	0xac,  /* ISRC2INT1 */
	0xad,
	0xae,
	0xaf,
	0xb0,  /* ISRC3DEC1 */
	0xb1,
	0xb2,
	0xb3,
	0xb4,  /* ISRC3INT1 */
	0xb5,
	0xb6,
	0xb7,
	0xb8,  /* ISRC4INT1 */
	0xb9,
	0xbc,  /* ISRC4DEC1 */
	0xbd,
};
EXPORT_SYMBOL_GPL(arizona_mixer_values);

const char * const arizona_v2_mixer_texts[ARIZONA_V2_NUM_MIXER_INPUTS] = {
	"None",
	"Tone Generator 1",
	"Tone Generator 2",
	"Haptics",
	"AEC",
	"AEC2",
	"Mic Mute Mixer",
	"Noise Generator",
	"IN1L",
	"IN1R",
	"IN2L",
	"IN2R",
	"IN3L",
	"IN3R",
	"IN4L",
	"IN4R",
	"IN5L",
	"IN5R",
	"IN6L",
	"IN6R",
	"AIF1RX1",
	"AIF1RX2",
	"AIF1RX3",
	"AIF1RX4",
	"AIF1RX5",
	"AIF1RX6",
	"AIF1RX7",
	"AIF1RX8",
	"AIF2RX1",
	"AIF2RX2",
	"AIF2RX3",
	"AIF2RX4",
	"AIF2RX5",
	"AIF2RX6",
	"AIF2RX7",
	"AIF2RX8",
	"AIF3RX1",
	"AIF3RX2",
	"AIF4RX1",
	"AIF4RX2",
	"SLIMRX1",
	"SLIMRX2",
	"SLIMRX3",
	"SLIMRX4",
	"SLIMRX5",
	"SLIMRX6",
	"SLIMRX7",
	"SLIMRX8",
	"EQ1",
	"EQ2",
	"EQ3",
	"EQ4",
	"DRC1L",
	"DRC1R",
	"DRC2L",
	"DRC2R",
	"LHPF1",
	"LHPF2",
	"LHPF3",
	"LHPF4",
	"DSP1.1",
	"DSP1.2",
	"DSP1.3",
	"DSP1.4",
	"DSP1.5",
	"DSP1.6",
	"DSP2.1",
	"DSP2.2",
	"DSP2.3",
	"DSP2.4",
	"DSP2.5",
	"DSP2.6",
	"DSP3.1",
	"DSP3.2",
	"DSP3.3",
	"DSP3.4",
	"DSP3.5",
	"DSP3.6",
	"DSP4.1",
	"DSP4.2",
	"DSP4.3",
	"DSP4.4",
	"DSP4.5",
	"DSP4.6",
	"DSP5.1",
	"DSP5.2",
	"DSP5.3",
	"DSP5.4",
	"DSP5.5",
	"DSP5.6",
	"DSP6.1",
	"DSP6.2",
	"DSP6.3",
	"DSP6.4",
	"DSP6.5",
	"DSP6.6",
	"DSP7.1",
	"DSP7.2",
	"DSP7.3",
	"DSP7.4",
	"DSP7.5",
	"DSP7.6",
	"ASRC1IN1L",
	"ASRC1IN1R",
	"ASRC1IN2L",
	"ASRC1IN2R",
	"ASRC2IN1L",
	"ASRC2IN1R",
	"ASRC2IN2L",
	"ASRC2IN2R",
	"ISRC1INT1",
	"ISRC1INT2",
	"ISRC1INT3",
	"ISRC1INT4",
	"ISRC1DEC1",
	"ISRC1DEC2",
	"ISRC1DEC3",
	"ISRC1DEC4",
	"ISRC2INT1",
	"ISRC2INT2",
	"ISRC2INT3",
	"ISRC2INT4",
	"ISRC2DEC1",
	"ISRC2DEC2",
	"ISRC2DEC3",
	"ISRC2DEC4",
	"ISRC3INT1",
	"ISRC3INT2",
	"ISRC3INT3",
	"ISRC3INT4",
	"ISRC3DEC1",
	"ISRC3DEC2",
	"ISRC3DEC3",
	"ISRC3DEC4",
	"ISRC4INT1",
	"ISRC4INT2",
	"ISRC4DEC1",
	"ISRC4DEC2",
	"DFC1",
	"DFC2",
	"DFC3",
	"DFC4",
	"DFC5",
	"DFC6",
	"DFC7",
	"DFC8",
};
EXPORT_SYMBOL_GPL(arizona_v2_mixer_texts);

unsigned int arizona_v2_mixer_values[ARIZONA_V2_NUM_MIXER_INPUTS] = {
	0x00,  /* None */
	0x04,  /* Tone */
	0x05,
	0x06,  /* Haptics */
	0x08,  /* AEC */
	0x09,  /* AEC2 */
	0x0c,  /* Noise mixer */
	0x0d,  /* Comfort noise */
	0x10,  /* IN1L */
	0x11,
	0x12,
	0x13,
	0x14,
	0x15,
	0x16,
	0x17,
	0x18,
	0x19,
	0x1A,
	0x1B,
	0x20,  /* AIF1RX1 */
	0x21,
	0x22,
	0x23,
	0x24,
	0x25,
	0x26,
	0x27,
	0x28,  /* AIF2RX1 */
	0x29,
	0x2a,
	0x2b,
	0x2c,
	0x2d,
	0x2e,
	0x2f,
	0x30,  /* AIF3RX1 */
	0x31,
	0x34,  /* AIF4RX1 */
	0x35,
	0x38,  /* SLIMRX1 */
	0x39,
	0x3a,
	0x3b,
	0x3c,
	0x3d,
	0x3e,
	0x3f,
	0x50,  /* EQ1 */
	0x51,
	0x52,
	0x53,
	0x58,  /* DRC1L */
	0x59,
	0x5a,
	0x5b,
	0x60,  /* LHPF1 */
	0x61,
	0x62,
	0x63,
	0x68,  /* DSP1.1 */
	0x69,
	0x6a,
	0x6b,
	0x6c,
	0x6d,
	0x70,  /* DSP2.1 */
	0x71,
	0x72,
	0x73,
	0x74,
	0x75,
	0x78,  /* DSP3.1 */
	0x79,
	0x7a,
	0x7b,
	0x7c,
	0x7d,
	0x80,  /* DSP4.1 */
	0x81,
	0x82,
	0x83,
	0x84,
	0x85,
	0x88,  /* DSP5.1 */
	0x89,
	0x8a,
	0x8b,
	0x8c,
	0x8d,
	0xc0,  /* DSP6.1 */
	0xc1,
	0xc2,
	0xc3,
	0xc4,
	0xc5,
	0xc8,  /* DSP7.1 */
	0xc9,
	0xca,
	0xcb,
	0xcc,
	0xcd,
	0x90,  /* ASRC1IN1L */
	0x91,
	0x92,
	0x93,
	0x94,  /* ASRC2IN1L */
	0x95,
	0x96,
	0x97,
	0xa0,  /* ISRC1INT1 */
	0xa1,
	0xa2,
	0xa3,
	0xa4,  /* ISRC1DEC1 */
	0xa5,
	0xa6,
	0xa7,
	0xa8,  /* ISRC2DEC1 */
	0xa9,
	0xaa,
	0xab,
	0xac,  /* ISRC2INT1 */
	0xad,
	0xae,
	0xaf,
	0xb0,  /* ISRC3DEC1 */
	0xb1,
	0xb2,
	0xb3,
	0xb4,  /* ISRC3INT1 */
	0xb5,
	0xb6,
	0xb7,
	0xb8,  /* ISRC4INT1 */
	0xb9,
	0xbc,  /* ISRC4DEC1 */
	0xbd,
	0xf8, /* DFC1 */
	0xf9,
	0xfa,
	0xfb,
	0xfc,
	0xfd,
	0xfe,
	0xff, /* DFC8 */
};
EXPORT_SYMBOL_GPL(arizona_v2_mixer_values);

const DECLARE_TLV_DB_SCALE(arizona_mixer_tlv, -3200, 100, 0);
EXPORT_SYMBOL_GPL(arizona_mixer_tlv);

const char * const arizona_sample_rate_text[ARIZONA_SAMPLE_RATE_ENUM_SIZE] = {
	"12kHz", "24kHz", "48kHz", "96kHz", "192kHz",
	"11.025kHz", "22.05kHz", "44.1kHz", "88.2kHz", "176.4kHz",
	"4kHz", "8kHz", "16kHz", "32kHz",
};
EXPORT_SYMBOL_GPL(arizona_sample_rate_text);

const unsigned int arizona_sample_rate_val[ARIZONA_SAMPLE_RATE_ENUM_SIZE] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
	0x10, 0x11, 0x12, 0x13,
};
EXPORT_SYMBOL_GPL(arizona_sample_rate_val);

const char *arizona_sample_rate_val_to_name(unsigned int rate_val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(arizona_sample_rate_val); ++i) {
		if (arizona_sample_rate_val[i] == rate_val)
			return arizona_sample_rate_text[i];
	}

	return "Illegal";
}
EXPORT_SYMBOL_GPL(arizona_sample_rate_val_to_name);

const struct soc_enum arizona_sample_rate[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_SAMPLE_RATE_2,
			      ARIZONA_SAMPLE_RATE_2_SHIFT, 0x1f,
			      ARIZONA_SAMPLE_RATE_ENUM_SIZE,
			      arizona_sample_rate_text,
			      arizona_sample_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_SAMPLE_RATE_3,
			      ARIZONA_SAMPLE_RATE_3_SHIFT, 0x1f,
			      ARIZONA_SAMPLE_RATE_ENUM_SIZE,
			      arizona_sample_rate_text,
			      arizona_sample_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ASYNC_SAMPLE_RATE_2,
			      ARIZONA_ASYNC_SAMPLE_RATE_2_SHIFT, 0x1f,
			      ARIZONA_SAMPLE_RATE_ENUM_SIZE,
			      arizona_sample_rate_text,
			      arizona_sample_rate_val),

};
EXPORT_SYMBOL_GPL(arizona_sample_rate);

const char * const arizona_rate_text[ARIZONA_RATE_ENUM_SIZE] = {
	"SYNCCLK rate 1", "SYNCCLK rate 2", "SYNCCLK rate 3",
	"ASYNCCLK rate", "ASYNCCLK rate 2",
};
EXPORT_SYMBOL_GPL(arizona_rate_text);

const struct soc_enum arizona_output_rate =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_OUTPUT_RATE_1,
			      ARIZONA_OUT_RATE_SHIFT,
			      0x0f,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text,
			      arizona_rate_val);
EXPORT_SYMBOL_GPL(arizona_output_rate);

const struct soc_enum arizona_input_rate =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_INPUT_RATE,
			      ARIZONA_IN_RATE_SHIFT,
			      0x0f,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text,
			      arizona_rate_val);
EXPORT_SYMBOL_GPL(arizona_input_rate);

const struct soc_enum moon_input_rate[] = {
	SOC_VALUE_ENUM_SINGLE(MOON_IN1L_RATE_CONTROL,
		MOON_IN1L_RATE_SHIFT,
		MOON_IN1L_RATE_MASK >> MOON_IN1L_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN1R_RATE_CONTROL,
		MOON_IN1R_RATE_SHIFT,
		MOON_IN1R_RATE_MASK >> MOON_IN1R_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN2L_RATE_CONTROL,
		MOON_IN2L_RATE_SHIFT,
		MOON_IN2L_RATE_MASK >> MOON_IN2L_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN2R_RATE_CONTROL,
		MOON_IN2R_RATE_SHIFT,
		MOON_IN2R_RATE_MASK >> MOON_IN2R_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN3L_RATE_CONTROL,
		MOON_IN3L_RATE_SHIFT,
		MOON_IN3L_RATE_MASK >> MOON_IN3L_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN3R_RATE_CONTROL,
		MOON_IN3R_RATE_SHIFT,
		MOON_IN3R_RATE_MASK >> MOON_IN3R_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN4L_RATE_CONTROL,
		MOON_IN4L_RATE_SHIFT,
		MOON_IN4L_RATE_MASK >> MOON_IN4L_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN4R_RATE_CONTROL,
		MOON_IN4R_RATE_SHIFT,
		MOON_IN4R_RATE_MASK >> MOON_IN4R_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN5L_RATE_CONTROL,
		MOON_IN5L_RATE_SHIFT,
		MOON_IN5L_RATE_MASK >> MOON_IN5L_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(MOON_IN5R_RATE_CONTROL,
		MOON_IN5R_RATE_SHIFT,
		MOON_IN5R_RATE_MASK >> MOON_IN5R_RATE_SHIFT,
		ARIZONA_SYNC_RATE_ENUM_SIZE,
		arizona_rate_text,
		arizona_rate_val),
};
EXPORT_SYMBOL_GPL(moon_input_rate);

const char * const moon_dfc_width_text[MOON_DFC_WIDTH_ENUM_SIZE] = {
	"8bit", "16bit", "20bit", "24bit", "32bit",
};
EXPORT_SYMBOL_GPL(moon_dfc_width_text);

const unsigned int moon_dfc_width_val[MOON_DFC_WIDTH_ENUM_SIZE] = {
	7, 15, 19, 23, 31,
};
EXPORT_SYMBOL_GPL(moon_dfc_width_val);

const char * const moon_dfc_type_text[MOON_DFC_TYPE_ENUM_SIZE] = {
	"Fixed", "Unsigned Fixed", "Single Precision Floating",
	"Half Precision Floating", "Arm Alternative Floating",
};
EXPORT_SYMBOL_GPL(moon_dfc_type_text);

const unsigned int moon_dfc_type_val[MOON_DFC_TYPE_ENUM_SIZE] = {
	0, 1, 2, 4, 5,
};
EXPORT_SYMBOL_GPL(moon_dfc_type_val);


const struct soc_enum moon_dfc_width[] = {
	SOC_VALUE_ENUM_SINGLE(MOON_DFC1_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC1_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC2_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC2_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC3_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC3_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC4_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC4_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC5_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC5_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC6_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC6_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC7_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC7_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC8_RX,
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		MOON_DFC1_RX_DATA_WIDTH_MASK >>
		MOON_DFC1_RX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC8_TX,
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		MOON_DFC1_TX_DATA_WIDTH_MASK >>
		MOON_DFC1_TX_DATA_WIDTH_SHIFT,
		ARRAY_SIZE(moon_dfc_width_text),
		moon_dfc_width_text,
		moon_dfc_width_val),
};
EXPORT_SYMBOL_GPL(moon_dfc_width);

const struct soc_enum moon_dfc_type[] = {
	SOC_VALUE_ENUM_SINGLE(MOON_DFC1_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC1_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC2_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC2_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC3_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC3_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC4_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC4_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC5_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC5_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC6_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC6_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC7_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC7_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC8_RX,
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		MOON_DFC1_RX_DATA_TYPE_MASK >>
		MOON_DFC1_RX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
	SOC_VALUE_ENUM_SINGLE(MOON_DFC8_TX,
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		MOON_DFC1_TX_DATA_TYPE_MASK >>
		MOON_DFC1_TX_DATA_TYPE_SHIFT,
		ARRAY_SIZE(moon_dfc_type_text),
		moon_dfc_type_text,
		moon_dfc_type_val),
};
EXPORT_SYMBOL_GPL(moon_dfc_type);

const struct soc_enum arizona_fx_rate =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_FX_CTRL1,
			      ARIZONA_FX_RATE_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val);
EXPORT_SYMBOL_GPL(arizona_fx_rate);

const struct soc_enum arizona_spdif_rate =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_SPD1_TX_CONTROL,
			      ARIZONA_SPD1_RATE_SHIFT,
			      0x0f,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text,
			      arizona_rate_val);
EXPORT_SYMBOL_GPL(arizona_spdif_rate);

const unsigned int arizona_rate_val[ARIZONA_RATE_ENUM_SIZE] = {
	0x0, 0x1, 0x2, 0x8, 0x9,
};
EXPORT_SYMBOL_GPL(arizona_rate_val);

const struct soc_enum arizona_isrc_fsh[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_1_CTRL_1,
			      ARIZONA_ISRC1_FSH_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_2_CTRL_1,
			      ARIZONA_ISRC2_FSH_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_3_CTRL_1,
			      ARIZONA_ISRC3_FSH_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_4_CTRL_1,
			      ARIZONA_ISRC4_FSH_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),

};
EXPORT_SYMBOL_GPL(arizona_isrc_fsh);

const struct soc_enum arizona_isrc_fsl[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_1_CTRL_2,
			      ARIZONA_ISRC1_FSL_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_2_CTRL_2,
			      ARIZONA_ISRC2_FSL_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_3_CTRL_2,
			      ARIZONA_ISRC3_FSL_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ISRC_4_CTRL_2,
			      ARIZONA_ISRC4_FSL_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),

};
EXPORT_SYMBOL_GPL(arizona_isrc_fsl);

const struct soc_enum arizona_asrc_rate1 =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ASRC_RATE1,
			      ARIZONA_ASRC_RATE1_SHIFT, 0xf,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val);
EXPORT_SYMBOL_GPL(arizona_asrc_rate1);

const struct soc_enum arizona_asrc_rate2 =
	SOC_VALUE_ENUM_SINGLE(ARIZONA_ASRC_RATE2,
			      ARIZONA_ASRC_RATE2_SHIFT, 0xf,
			      ARIZONA_ASYNC_RATE_ENUM_SIZE,
			      arizona_rate_text + ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_val + ARIZONA_SYNC_RATE_ENUM_SIZE);
EXPORT_SYMBOL_GPL(arizona_asrc_rate2);

const struct soc_enum clearwater_asrc1_rate[] = {
	SOC_VALUE_ENUM_SINGLE(CLEARWATER_ASRC1_RATE1,
			      CLEARWATER_ASRC1_RATE1_SHIFT, 0xf,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(CLEARWATER_ASRC1_RATE2,
			      CLEARWATER_ASRC1_RATE1_SHIFT, 0xf,
			      ARIZONA_ASYNC_RATE_ENUM_SIZE,
			      arizona_rate_text + ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_val + ARIZONA_SYNC_RATE_ENUM_SIZE),

};
EXPORT_SYMBOL_GPL(clearwater_asrc1_rate);

const struct soc_enum clearwater_asrc2_rate[] = {
	SOC_VALUE_ENUM_SINGLE(CLEARWATER_ASRC2_RATE1,
			      CLEARWATER_ASRC2_RATE1_SHIFT, 0xf,
			      ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(CLEARWATER_ASRC2_RATE2,
			      CLEARWATER_ASRC2_RATE2_SHIFT, 0xf,
			      ARIZONA_ASYNC_RATE_ENUM_SIZE,
			      arizona_rate_text + ARIZONA_SYNC_RATE_ENUM_SIZE,
			      arizona_rate_val + ARIZONA_SYNC_RATE_ENUM_SIZE),

};
EXPORT_SYMBOL_GPL(clearwater_asrc2_rate);

static const char * const arizona_vol_ramp_text[] = {
	"0ms/6dB", "0.5ms/6dB", "1ms/6dB", "2ms/6dB", "4ms/6dB", "8ms/6dB",
	"15ms/6dB", "30ms/6dB",
};

SOC_ENUM_SINGLE_DECL(arizona_in_vd_ramp,
		     ARIZONA_INPUT_VOLUME_RAMP,
		     ARIZONA_IN_VD_RAMP_SHIFT,
		     arizona_vol_ramp_text);
EXPORT_SYMBOL_GPL(arizona_in_vd_ramp);

SOC_ENUM_SINGLE_DECL(arizona_in_vi_ramp,
		     ARIZONA_INPUT_VOLUME_RAMP,
		     ARIZONA_IN_VI_RAMP_SHIFT,
		     arizona_vol_ramp_text);
EXPORT_SYMBOL_GPL(arizona_in_vi_ramp);

SOC_ENUM_SINGLE_DECL(arizona_out_vd_ramp,
		     ARIZONA_OUTPUT_VOLUME_RAMP,
		     ARIZONA_OUT_VD_RAMP_SHIFT,
		     arizona_vol_ramp_text);
EXPORT_SYMBOL_GPL(arizona_out_vd_ramp);

SOC_ENUM_SINGLE_DECL(arizona_out_vi_ramp,
		     ARIZONA_OUTPUT_VOLUME_RAMP,
		     ARIZONA_OUT_VI_RAMP_SHIFT,
		     arizona_vol_ramp_text);
EXPORT_SYMBOL_GPL(arizona_out_vi_ramp);

static const char * const arizona_lhpf_mode_text[] = {
	"Low-pass", "High-pass"
};

SOC_ENUM_SINGLE_DECL(arizona_lhpf1_mode,
		     ARIZONA_HPLPF1_1,
		     ARIZONA_LHPF1_MODE_SHIFT,
		     arizona_lhpf_mode_text);
EXPORT_SYMBOL_GPL(arizona_lhpf1_mode);

SOC_ENUM_SINGLE_DECL(arizona_lhpf2_mode,
		     ARIZONA_HPLPF2_1,
		     ARIZONA_LHPF2_MODE_SHIFT,
		     arizona_lhpf_mode_text);
EXPORT_SYMBOL_GPL(arizona_lhpf2_mode);

SOC_ENUM_SINGLE_DECL(arizona_lhpf3_mode,
		     ARIZONA_HPLPF3_1,
		     ARIZONA_LHPF3_MODE_SHIFT,
		     arizona_lhpf_mode_text);
EXPORT_SYMBOL_GPL(arizona_lhpf3_mode);

SOC_ENUM_SINGLE_DECL(arizona_lhpf4_mode,
		     ARIZONA_HPLPF4_1,
		     ARIZONA_LHPF4_MODE_SHIFT,
		     arizona_lhpf_mode_text);
EXPORT_SYMBOL_GPL(arizona_lhpf4_mode);

static const char * const arizona_ng_hold_text[] = {
	"30ms", "120ms", "250ms", "500ms",
};

SOC_ENUM_SINGLE_DECL(arizona_ng_hold,
		     ARIZONA_NOISE_GATE_CONTROL,
		     ARIZONA_NGATE_HOLD_SHIFT,
		     arizona_ng_hold_text);
EXPORT_SYMBOL_GPL(arizona_ng_hold);

static const char * const arizona_in_hpf_cut_text[] = {
	"2.5Hz", "5Hz", "10Hz", "20Hz", "40Hz"
};

SOC_ENUM_SINGLE_DECL(arizona_in_hpf_cut_enum,
		     ARIZONA_HPF_CONTROL,
		     ARIZONA_IN_HPF_CUT_SHIFT,
		     arizona_in_hpf_cut_text);
EXPORT_SYMBOL_GPL(arizona_in_hpf_cut_enum);

static const char * const arizona_in_dmic_osr_text[] = {
	"1.536MHz", "3.072MHz", "6.144MHz", "768kHz",
};

const struct soc_enum arizona_in_dmic_osr[] = {
	SOC_ENUM_SINGLE(ARIZONA_IN1L_CONTROL, ARIZONA_IN1_OSR_SHIFT,
			ARRAY_SIZE(arizona_in_dmic_osr_text),
			arizona_in_dmic_osr_text),
	SOC_ENUM_SINGLE(ARIZONA_IN2L_CONTROL, ARIZONA_IN2_OSR_SHIFT,
			ARRAY_SIZE(arizona_in_dmic_osr_text),
			arizona_in_dmic_osr_text),
	SOC_ENUM_SINGLE(ARIZONA_IN3L_CONTROL, ARIZONA_IN3_OSR_SHIFT,
			ARRAY_SIZE(arizona_in_dmic_osr_text),
			arizona_in_dmic_osr_text),
	SOC_ENUM_SINGLE(ARIZONA_IN4L_CONTROL, ARIZONA_IN4_OSR_SHIFT,
			ARRAY_SIZE(arizona_in_dmic_osr_text),
			arizona_in_dmic_osr_text),
};
EXPORT_SYMBOL_GPL(arizona_in_dmic_osr);

static const char * const clearwater_in_dmic_osr_text[CLEARWATER_OSR_ENUM_SIZE] = {
	"384kHz", "768kHz", "1.536MHz", "3.072MHz", "6.144MHz",
};

static const unsigned int clearwater_in_dmic_osr_val[CLEARWATER_OSR_ENUM_SIZE] = {
	2, 3, 4, 5, 6,
};

const struct soc_enum clearwater_in_dmic_osr[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC1L_CONTROL, CLEARWATER_IN1_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC2L_CONTROL, CLEARWATER_IN2_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC3L_CONTROL, CLEARWATER_IN3_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC4L_CONTROL, CLEARWATER_IN4_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC5L_CONTROL, CLEARWATER_IN5_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DMIC6L_CONTROL, CLEARWATER_IN6_OSR_SHIFT,
			      0x7, CLEARWATER_OSR_ENUM_SIZE,
			      clearwater_in_dmic_osr_text, clearwater_in_dmic_osr_val),
};
EXPORT_SYMBOL_GPL(clearwater_in_dmic_osr);

static const char * const arizona_anc_input_src_text[] = {
	"None", "IN1", "IN2", "IN3", "IN4", "IN5", "IN6",
};

static const char * const arizona_anc_channel_src_text[] = {
	"None", "Left", "Right", "Combine",
};

const struct soc_enum arizona_anc_input_src[] = {
	SOC_ENUM_SINGLE(ARIZONA_ANC_SRC,
			ARIZONA_IN_RXANCL_SEL_SHIFT,
			ARRAY_SIZE(arizona_anc_input_src_text),
			arizona_anc_input_src_text),
	SOC_ENUM_SINGLE(ARIZONA_FCL_ADC_REFORMATTER_CONTROL,
			ARIZONA_FCL_MIC_MODE_SEL,
			ARRAY_SIZE(arizona_anc_channel_src_text),
			arizona_anc_channel_src_text),
	SOC_ENUM_SINGLE(ARIZONA_ANC_SRC,
			ARIZONA_IN_RXANCR_SEL_SHIFT,
			ARRAY_SIZE(arizona_anc_input_src_text),
			arizona_anc_input_src_text),
	SOC_ENUM_SINGLE(ARIZONA_FCR_ADC_REFORMATTER_CONTROL,
			ARIZONA_FCR_MIC_MODE_SEL,
			ARRAY_SIZE(arizona_anc_channel_src_text),
			arizona_anc_channel_src_text),
};
EXPORT_SYMBOL_GPL(arizona_anc_input_src);

const struct soc_enum clearwater_anc_input_src[] = {
	SOC_ENUM_SINGLE(ARIZONA_ANC_SRC,
			ARIZONA_IN_RXANCL_SEL_SHIFT,
			ARRAY_SIZE(arizona_anc_input_src_text),
			arizona_anc_input_src_text),
	SOC_ENUM_SINGLE(ARIZONA_FCL_ADC_REFORMATTER_CONTROL,
			ARIZONA_FCL_MIC_MODE_SEL,
			ARRAY_SIZE(arizona_anc_channel_src_text),
			arizona_anc_channel_src_text),
	SOC_ENUM_SINGLE(ARIZONA_ANC_SRC,
			ARIZONA_IN_RXANCR_SEL_SHIFT,
			ARRAY_SIZE(arizona_anc_input_src_text),
			arizona_anc_input_src_text),
	SOC_ENUM_SINGLE(CLEARWATER_FCR_ADC_REFORMATTER_CONTROL,
			ARIZONA_FCR_MIC_MODE_SEL,
			ARRAY_SIZE(arizona_anc_channel_src_text),
			arizona_anc_channel_src_text),
};
EXPORT_SYMBOL_GPL(clearwater_anc_input_src);

static const char * const arizona_anc_ng_texts[] = {
	"None",
	"Internal",
	"External",
};

SOC_ENUM_SINGLE_DECL(arizona_anc_ng_enum, SND_SOC_NOPM, 0,
		     arizona_anc_ng_texts);
EXPORT_SYMBOL_GPL(arizona_anc_ng_enum);

static const char * const arizona_output_anc_src_text[] = {
	"None", "RXANCL", "RXANCR",
};

const struct soc_enum arizona_output_anc_src[] = {
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_1L,
			ARIZONA_OUT1L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_1R,
			ARIZONA_OUT1R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_2L,
			ARIZONA_OUT2L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_2R,
			ARIZONA_OUT2R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_3L,
			ARIZONA_OUT3L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_DAC_VOLUME_LIMIT_3R,
			ARIZONA_OUT3R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_4L,
			ARIZONA_OUT4L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_4R,
			ARIZONA_OUT4R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_5L,
			ARIZONA_OUT5L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_5R,
			ARIZONA_OUT5R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_6L,
			ARIZONA_OUT6L_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_6R,
			ARIZONA_OUT6R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
};
EXPORT_SYMBOL_GPL(arizona_output_anc_src);

const struct soc_enum clearwater_output_anc_src_defs[] = {
	SOC_ENUM_SINGLE(ARIZONA_OUTPUT_PATH_CONFIG_3R,
			ARIZONA_OUT3R_ANC_SRC_SHIFT,
			ARRAY_SIZE(arizona_output_anc_src_text),
			arizona_output_anc_src_text),
};
EXPORT_SYMBOL_GPL(clearwater_output_anc_src_defs);

static const char * const arizona_ip_mode_text[2] = {
	"Analog", "Digital",
};

const struct soc_enum arizona_ip_mode[] = {
	SOC_ENUM_SINGLE(ARIZONA_IN1L_CONTROL, ARIZONA_IN1_MODE_SHIFT,
		ARRAY_SIZE(arizona_ip_mode_text), arizona_ip_mode_text),
	SOC_ENUM_SINGLE(ARIZONA_IN2L_CONTROL, ARIZONA_IN2_MODE_SHIFT,
		ARRAY_SIZE(arizona_ip_mode_text), arizona_ip_mode_text),
	SOC_ENUM_SINGLE(ARIZONA_IN3L_CONTROL, ARIZONA_IN3_MODE_SHIFT,
		ARRAY_SIZE(arizona_ip_mode_text), arizona_ip_mode_text),
};
EXPORT_SYMBOL_GPL(arizona_ip_mode);

int arizona_ip_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg, ret = 0;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	/* Cannot change input mode on an active input*/
	reg = snd_soc_read(codec, ARIZONA_INPUT_ENABLES);

	switch (e->reg) {
	case ARIZONA_IN1L_CONTROL:
		if (reg & (ARIZONA_IN1L_ENA_MASK | ARIZONA_IN1R_ENA_MASK)) {
			ret = -EBUSY;
			goto exit;
		}
		break;
	case ARIZONA_IN2L_CONTROL:
		if (reg & (ARIZONA_IN2L_ENA_MASK | ARIZONA_IN2R_ENA_MASK)) {
			ret = -EBUSY;
			goto exit;
		}
		break;
	case ARIZONA_IN3L_CONTROL:
		if (reg & (ARIZONA_IN3L_ENA_MASK | ARIZONA_IN3R_ENA_MASK)) {
			ret = -EBUSY;
			goto exit;
		}
		break;
	default:
		ret = -EINVAL;
		goto exit;
		break;
	}

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);
exit:
	mutex_unlock(&codec->component.card->dapm_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_ip_mode_put);

int moon_in_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg, mask;
	int ret = 0;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	/* Cannot change rate on an active input */
	reg = snd_soc_read(codec, ARIZONA_INPUT_ENABLES);
	mask = (e->reg - ARIZONA_IN1L_CONTROL) / 4;
	mask ^= 0x1; /* Flip bottom bit for channel order */

	if ((reg) & (1 << mask)) {
		ret = -EBUSY;
		goto exit;
	}

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);
exit:
	mutex_unlock(&codec->component.card->dapm_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(moon_in_rate_put);

int moon_dfc_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int val;
	int ret = 0;

	reg = ((reg / 6) * 6) - 2;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	/* Cannot change dfc settings when its on */
	val = snd_soc_read(codec, reg);
	if (val & MOON_DFC1_ENA) {
		ret = -EBUSY;
		goto exit;
	}

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);
exit:
	mutex_unlock(&codec->component.card->dapm_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(moon_dfc_put);

int moon_osr_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int mode;

	/* for analog mode osr is fixed */
	mode = snd_soc_read(codec, e->reg - 2);
	if (!(mode & ARIZONA_IN1_MODE_MASK)) {
		dev_err(codec->dev,
			"OSR is fixed to 3.072MHz in analog mode\n");
		return -EINVAL;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}
EXPORT_SYMBOL_GPL(moon_osr_put);

int moon_lp_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	unsigned int reg, mask;
	int ret;


	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	/* Cannot change lp mode on an active input */
	reg = snd_soc_read(codec, ARIZONA_INPUT_ENABLES);
	mask = (mc->reg - ARIZONA_ADC_DIGITAL_VOLUME_1L) / 4;
	mask ^= 0x1; /* Flip bottom bit for channel order */

	if ((reg) & (1 << mask)) {
		ret = -EBUSY;
		dev_err(codec->dev,
			"Can't change lp mode on an active input\n");
		goto exit;
	}

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

exit:
	mutex_unlock(&codec->component.card->dapm_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(moon_lp_mode_put);

static void arizona_in_set_vu(struct snd_soc_codec *codec, int ena)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int val;
	int i;

	if (ena)
		val = ARIZONA_IN_VU;
	else
		val = 0;

	for (i = 0; i < priv->num_inputs; i++)
		snd_soc_update_bits(codec,
				    ARIZONA_ADC_DIGITAL_VOLUME_1L + (i * 4),
				    ARIZONA_IN_VU, val);
}

static int arizona_update_input(struct arizona *arizona, bool enable)
{
	unsigned int val;

	switch (arizona->type) {
	case WM8280:
	case WM5110:
		if (arizona->rev >= 6)
			return 0;
		break;
	default:
		return 0;
	}

	mutex_lock(&arizona->reg_setting_lock);
	regmap_write(arizona->regmap, 0x80,  0x3);

	if (enable) {
		arizona_florida_mute_analog(arizona, ARIZONA_IN1L_MUTE);
		usleep_range(10000, 10001);

		regmap_write(arizona->regmap, 0x3A6, 0x5555);
		regmap_write(arizona->regmap, 0x3A5, 0x3);
	} else {
		regmap_read(arizona->regmap, 0x3A5, &val);
		if (val) {
			usleep_range(10000, 10001);
			regmap_write(arizona->regmap, 0x3A5, 0x0);
			regmap_write(arizona->regmap, 0x3A6, 0x0);
			usleep_range(5000, 5001);
		}

		arizona_florida_mute_analog(arizona, 0);
	}

	regmap_write(arizona->regmap, 0x80,  0x0);
	mutex_unlock(&arizona->reg_setting_lock);

	return 0;
}

int arizona_in_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		  int event)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int ctrl;
	unsigned int reg;

	if (w->shift % 2) {
		reg = ARIZONA_ADC_DIGITAL_VOLUME_1L + ((w->shift / 2) * 8);
		ctrl = reg - 1;
	} else {
		reg = ARIZONA_ADC_DIGITAL_VOLUME_1R + ((w->shift / 2) * 8);
		ctrl = reg - 5;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		priv->in_pending++;

		/* Check for analogue input */
		if ((snd_soc_read(w->codec, ctrl) & 0x0400) == 0)
			arizona_update_input(priv->arizona, true);

		break;
	case SND_SOC_DAPM_POST_PMU:
		priv->in_pending--;

		if (priv->in_pending == 0)
			arizona_update_input(priv->arizona, false);

		snd_soc_update_bits(w->codec, reg, ARIZONA_IN1L_MUTE, 0);

		/* If this is the last input pending then allow VU */
		if (priv->in_pending == 0) {
			usleep_range(1000, 1001);
			arizona_in_set_vu(w->codec, 1);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, reg,
				    ARIZONA_IN1L_MUTE | ARIZONA_IN_VU,
				    ARIZONA_IN1L_MUTE | ARIZONA_IN_VU);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable volume updates if no inputs are enabled */
		reg = snd_soc_read(w->codec, ARIZONA_INPUT_ENABLES);
		if (reg == 0)
			arizona_in_set_vu(w->codec, 0);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_in_ev);

static const struct reg_default florida_no_dre_left_enable[] = {
	{ 0x3024, 0xE410 },
	{ 0x3025, 0x0056 },
	{ 0x301B, 0x0224 },
	{ 0x301F, 0x4263 },
	{ 0x3021, 0x5291 },
	{ 0x3030, 0xE410 },
	{ 0x3031, 0x3066 },
	{ 0x3032, 0xE410 },
	{ 0x3033, 0x3070 },
	{ 0x3034, 0xE410 },
	{ 0x3035, 0x3078 },
	{ 0x3036, 0xE410 },
	{ 0x3037, 0x3080 },
	{ 0x3038, 0xE410 },
	{ 0x3039, 0x3080 },
};

static const struct reg_default florida_dre_left_enable[] = {
	{ 0x3024, 0x0231 },
	{ 0x3025, 0x0B00 },
	{ 0x301B, 0x0227 },
	{ 0x301F, 0x4266 },
	{ 0x3021, 0x5294 },
	{ 0x3030, 0xE231 },
	{ 0x3031, 0x0266 },
	{ 0x3032, 0x8231 },
	{ 0x3033, 0x4B15 },
	{ 0x3034, 0x8231 },
	{ 0x3035, 0x0B15 },
	{ 0x3036, 0xE231 },
	{ 0x3037, 0x5294 },
	{ 0x3038, 0x0231 },
	{ 0x3039, 0x0B00 },
};

static const struct reg_default florida_no_dre_right_enable[] = {
	{ 0x3074, 0xE414 },
	{ 0x3075, 0x0056 },
	{ 0x306B, 0x0224 },
	{ 0x306F, 0x4263 },
	{ 0x3071, 0x5291 },
	{ 0x3080, 0xE414 },
	{ 0x3081, 0x3066 },
	{ 0x3082, 0xE414 },
	{ 0x3083, 0x3070 },
	{ 0x3084, 0xE414 },
	{ 0x3085, 0x3078 },
	{ 0x3086, 0xE414 },
	{ 0x3087, 0x3080 },
	{ 0x3088, 0xE414 },
	{ 0x3089, 0x3080 },
};

static const struct reg_default florida_dre_right_enable[] = {
	{ 0x3074, 0x0231 },
	{ 0x3075, 0x0B00 },
	{ 0x306B, 0x0227 },
	{ 0x306F, 0x4266 },
	{ 0x3071, 0x5294 },
	{ 0x3080, 0xE231 },
	{ 0x3081, 0x0266 },
	{ 0x3082, 0x8231 },
	{ 0x3083, 0x4B17 },
	{ 0x3084, 0x8231 },
	{ 0x3085, 0x0B17 },
	{ 0x3086, 0xE231 },
	{ 0x3087, 0x5294 },
	{ 0x3088, 0x0231 },
	{ 0x3089, 0x0B00 },
};

static int florida_hp_pre_enable(struct snd_soc_dapm_widget *w)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int val = snd_soc_read(w->codec, ARIZONA_DRE_ENABLE);

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
		if (val & ARIZONA_DRE1L_ENA_MASK) {
			regmap_multi_reg_write(priv->arizona->regmap,
					       florida_dre_left_enable,
					       ARRAY_SIZE(florida_dre_left_enable));
		} else {
			regmap_multi_reg_write(priv->arizona->regmap,
					       florida_no_dre_left_enable,
					       ARRAY_SIZE(florida_no_dre_left_enable));
		}
		udelay(1000);
		break;
	case ARIZONA_OUT1R_ENA_SHIFT:
		if (val & ARIZONA_DRE1R_ENA_MASK) {
			regmap_multi_reg_write(priv->arizona->regmap,
					       florida_dre_right_enable,
					       ARRAY_SIZE(florida_dre_right_enable));
		} else {
			regmap_multi_reg_write(priv->arizona->regmap,
					       florida_no_dre_right_enable,
					       ARRAY_SIZE(florida_no_dre_right_enable));
		}
		udelay(1000);
		break;

	default:
		break;
	}

	return 0;
}

static int florida_hp_post_enable(struct snd_soc_dapm_widget *w)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int val = snd_soc_read(w->codec, ARIZONA_DRE_ENABLE);

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1L_ENA_MASK))
			priv->out_up_delay += 10;
		break;
	case ARIZONA_OUT1R_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1R_ENA_MASK))
			priv->out_up_delay += 10;
		break;

	default:
		break;
	}

	return 0;
}

static int florida_hp_pre_disable(struct snd_soc_dapm_widget *w)
{
	unsigned int val = snd_soc_read(w->codec, ARIZONA_DRE_ENABLE);

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1L_ENA_MASK)) {
			snd_soc_write(w->codec,
				      ARIZONA_WRITE_SEQUENCER_CTRL_0,
				      ARIZONA_WSEQ_ENA | ARIZONA_WSEQ_START |
				      0x138);
			usleep_range(10000, 10001);
		}
		break;
	case ARIZONA_OUT1R_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1R_ENA_MASK)) {
			snd_soc_write(w->codec,
				      ARIZONA_WRITE_SEQUENCER_CTRL_0,
				      ARIZONA_WSEQ_ENA | ARIZONA_WSEQ_START |
				      0x13d);
			usleep_range(10000, 10001);
		}
		break;

	default:
		break;
	}

	return 0;
}

static int florida_hp_post_disable(struct snd_soc_dapm_widget *w)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	unsigned int val = snd_soc_read(w->codec, ARIZONA_DRE_ENABLE);

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1L_ENA_MASK))
			priv->out_down_delay += 17;
		break;
	case ARIZONA_OUT1R_ENA_SHIFT:
		if (!(val & ARIZONA_DRE1R_ENA_MASK))
			priv->out_down_delay += 17;
		break;

	default:
		break;
	}

	return 0;
}

static void clearwater_hp_post_enable(struct snd_soc_dapm_widget *w)
{
	unsigned int val;

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
	case ARIZONA_OUT1R_ENA_SHIFT:
		val = snd_soc_read(w->codec, ARIZONA_OUTPUT_ENABLES_1);
		val &= (ARIZONA_OUT1L_ENA | ARIZONA_OUT1R_ENA);

		if (val == (ARIZONA_OUT1L_ENA | ARIZONA_OUT1R_ENA))
			snd_soc_update_bits(w->codec,
				    CLEARWATER_EDRE_HP_STEREO_CONTROL,
				    ARIZONA_HP1_EDRE_STEREO_MASK,
				    ARIZONA_HP1_EDRE_STEREO);
		break;

	default:
		break;
	}
}

static void clearwater_hp_post_disable(struct snd_soc_dapm_widget *w)
{
	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
		snd_soc_write(w->codec,
			      ARIZONA_DCS_HP1L_CONTROL,
			      0x2006);
		break;
	case ARIZONA_OUT1R_ENA_SHIFT:
		snd_soc_write(w->codec,
			      ARIZONA_DCS_HP1R_CONTROL,
			      0x2006);
		break;
	default:
		return;
	}

	/* Only get to here for OUT1L and OUT1R */
	snd_soc_update_bits(w->codec,
			    CLEARWATER_EDRE_HP_STEREO_CONTROL,
			    ARIZONA_HP1_EDRE_STEREO_MASK,
			    0);
}

static void moon_analog_post_enable(struct snd_soc_dapm_widget *w)
{
	unsigned int mask, val;

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
	case ARIZONA_OUT1R_ENA_SHIFT:
		mask = ARIZONA_HP1_EDRE_STEREO_MASK;
		val = ARIZONA_HP1_EDRE_STEREO;
		break;
	case ARIZONA_OUT2L_ENA_SHIFT:
	case ARIZONA_OUT2R_ENA_SHIFT:
		mask = ARIZONA_HP2_EDRE_STEREO_MASK;
		val = ARIZONA_HP2_EDRE_STEREO;
		break;
	case ARIZONA_OUT3L_ENA_SHIFT:
	case ARIZONA_OUT3R_ENA_SHIFT:
		mask = ARIZONA_HP3_EDRE_STEREO_MASK;
		val = ARIZONA_HP3_EDRE_STEREO;
		break;
	default:
		return;
	}

	snd_soc_update_bits(w->codec,
		CLEARWATER_EDRE_HP_STEREO_CONTROL,
		mask, val);
}

static void moon_analog_post_disable(struct snd_soc_dapm_widget *w)
{
	unsigned int mask;

	switch (w->shift) {
	case ARIZONA_OUT1L_ENA_SHIFT:
	case ARIZONA_OUT1R_ENA_SHIFT:
		mask = ARIZONA_HP1_EDRE_STEREO_MASK;
		break;
	case ARIZONA_OUT2L_ENA_SHIFT:
	case ARIZONA_OUT2R_ENA_SHIFT:
		mask = ARIZONA_HP2_EDRE_STEREO_MASK;
		break;
	case ARIZONA_OUT3L_ENA_SHIFT:
	case ARIZONA_OUT3R_ENA_SHIFT:
		mask = ARIZONA_HP3_EDRE_STEREO_MASK;
		break;
	default:
		return;
	}

	snd_soc_update_bits(w->codec,
		CLEARWATER_EDRE_HP_STEREO_CONTROL,
		mask, 0);
}

static int florida_set_dre(struct arizona *arizona, unsigned int shift,
			   bool enable)
{
	unsigned int pga = ARIZONA_OUTPUT_PATH_CONFIG_1L + shift * 4;
	unsigned int mask = 1 << shift;
	unsigned int val = 0;
	const struct reg_default *wseq;
	int nregs;
	bool change;

	if (enable) {
		regmap_update_bits_check(arizona->regmap, ARIZONA_DRE_ENABLE,
					 mask, mask, &change);
		if (!change)
			return 0;

		/* Force reset of PGA Vol */
		regmap_update_bits(arizona->regmap, pga,
				   ARIZONA_OUT1L_PGA_VOL_MASK, 0x7F);
		regmap_update_bits(arizona->regmap, pga,
				   ARIZONA_OUT1L_PGA_VOL_MASK, 0x80);

		switch (shift) {
		case ARIZONA_DRE1L_ENA_SHIFT:
			mask = ARIZONA_OUT1L_ENA;
			wseq = florida_dre_left_enable;
			nregs = ARRAY_SIZE(florida_dre_left_enable);
			break;
		case ARIZONA_DRE1R_ENA_SHIFT:
			mask = ARIZONA_OUT1R_ENA;
			wseq = florida_dre_right_enable;
			nregs = ARRAY_SIZE(florida_dre_right_enable);
			break;
		default:
			return 0;
		}
	} else {
		regmap_update_bits_check(arizona->regmap, ARIZONA_DRE_ENABLE,
					 mask, 0, &change);
		if (!change)
			return 0;

		switch (shift) {
		case ARIZONA_DRE1L_ENA_SHIFT:
			mask = ARIZONA_OUT1L_ENA;
			wseq = florida_no_dre_left_enable;
			nregs = ARRAY_SIZE(florida_no_dre_left_enable);
			break;
		case ARIZONA_DRE1R_ENA_SHIFT:
			mask = ARIZONA_OUT1R_ENA;
			wseq = florida_no_dre_right_enable;
			nregs = ARRAY_SIZE(florida_no_dre_right_enable);
			break;
		default:
			return 0;
		}
	}

	/* If the output is on we need to update the disable sequence */
	regmap_read(arizona->regmap, ARIZONA_OUTPUT_ENABLES_1, &val);

	if (val & mask) {
		regmap_multi_reg_write(arizona->regmap, wseq, nregs);
		udelay(1000);
	}

	return 0;
}

int florida_put_dre(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int lshift = mc->shift;
	unsigned int rshift = mc->rshift;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	florida_set_dre(arizona, lshift, !!ucontrol->value.integer.value[0]);
	florida_set_dre(arizona, rshift, !!ucontrol->value.integer.value[1]);

	mutex_unlock(&codec->component.card->dapm_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(florida_put_dre);

int clearwater_put_dre(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int ret;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

	mutex_unlock(&codec->component.card->dapm_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(clearwater_put_dre);

int arizona_put_out4_edre(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int mask_l, mask_r, old_val, out_ena;
	unsigned int val_l = 0, val_r = 0;
	int ret = 0;

	switch (priv->arizona->type) {
	case WM1814:
	case WM8998:
		mask_l = CLEARWATER_EDRE_OUT4L_THR1_ENA |
			 CLEARWATER_EDRE_OUT4L_THR2_ENA;
		mask_r = CLEARWATER_EDRE_OUT4R_THR1_ENA |
			 CLEARWATER_EDRE_OUT4R_THR2_ENA;
		break;
	default:
		return 0;
	}

	if (ucontrol->value.integer.value[0])
		val_l = mask_l;

	if (ucontrol->value.integer.value[1])
		val_r = mask_r;

	mutex_lock_nested(&codec->component.card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);

	/* Check what will change so we know which output enables to test */
	old_val = snd_soc_read(codec, CLEARWATER_EDRE_ENABLE);
	if ((old_val & mask_l) == val_l)
		mask_l = 0;

	if ((old_val & mask_r) == val_r)
		mask_r = 0;

	if ((mask_l | mask_r) == 0)
		goto out;

	out_ena = snd_soc_read(codec, ARIZONA_OUTPUT_ENABLES_1);
	if ((mask_l && (out_ena & ARIZONA_OUT4L_ENA_MASK)) ||
	    (mask_r && (out_ena & ARIZONA_OUT4R_ENA_MASK))) {
		dev_warn(codec->dev, "Cannot change OUT4 eDRE with output on\n");
		ret = -EBUSY;
		goto out;
	}

	snd_soc_update_bits(codec, CLEARWATER_EDRE_ENABLE,
			    mask_l | mask_r, val_l | val_r);
out:
	mutex_unlock(&codec->component.card->dapm_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_put_out4_edre);

int arizona_out_ev(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol,
		   int event)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		switch (w->shift) {
		case ARIZONA_OUT1L_ENA_SHIFT:
		case ARIZONA_OUT1R_ENA_SHIFT:
		case ARIZONA_OUT2L_ENA_SHIFT:
		case ARIZONA_OUT2R_ENA_SHIFT:
		case ARIZONA_OUT3L_ENA_SHIFT:
		case ARIZONA_OUT3R_ENA_SHIFT:
			priv->out_up_pending++;
			priv->out_up_delay += 17;
			break;
		default:
			break;
		}
		break;

	case SND_SOC_DAPM_POST_PMU:
		switch (w->shift) {
		case ARIZONA_OUT1L_ENA_SHIFT:
		case ARIZONA_OUT1R_ENA_SHIFT:
		case ARIZONA_OUT2L_ENA_SHIFT:
		case ARIZONA_OUT2R_ENA_SHIFT:
		case ARIZONA_OUT3L_ENA_SHIFT:
		case ARIZONA_OUT3R_ENA_SHIFT:
			priv->out_up_pending--;
			if (!priv->out_up_pending) {
				usleep_range(priv->out_up_delay*1000,
					priv->out_up_delay*1000 + 1);
				priv->out_up_delay = 0;
			}
			break;

		default:
			break;
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		switch (w->shift) {
		case ARIZONA_OUT1L_ENA_SHIFT:
		case ARIZONA_OUT1R_ENA_SHIFT:
		case ARIZONA_OUT2L_ENA_SHIFT:
		case ARIZONA_OUT2R_ENA_SHIFT:
		case ARIZONA_OUT3L_ENA_SHIFT:
		case ARIZONA_OUT3R_ENA_SHIFT:
			priv->out_down_pending++;
			priv->out_down_delay++;
			break;
		default:
			break;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		switch (w->shift) {
		case ARIZONA_OUT1L_ENA_SHIFT:
		case ARIZONA_OUT1R_ENA_SHIFT:
		case ARIZONA_OUT2L_ENA_SHIFT:
		case ARIZONA_OUT2R_ENA_SHIFT:
		case ARIZONA_OUT3L_ENA_SHIFT:
		case ARIZONA_OUT3R_ENA_SHIFT:
			priv->out_down_pending--;
			if (!priv->out_down_pending) {
				usleep_range(priv->out_down_delay*1000,
					priv->out_down_delay*1000 + 1);
				priv->out_down_delay = 0;
			}
			break;
		default:
			break;
		}
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_out_ev);

int arizona_hp_ev(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol,
		   int event)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(w->codec);
	struct arizona *arizona = priv->arizona;
	unsigned int mask = 1 << w->shift;
	unsigned int val;
	unsigned int ep_sel = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		val = mask;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		val = 0;
		break;
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMD:
		return arizona_out_ev(w, kcontrol, event);
	default:
		return -EINVAL;
	}

	/* Store the desired state for the HP outputs */
	priv->arizona->hp_ena &= ~mask;
	priv->arizona->hp_ena |= val;

	/* in case of Marley check if OUT1 is routed to EPOUT, do not disable
	 * OUT1 in this case */
	switch (priv->arizona->type) {
	case CS47L35:
		regmap_read(priv->arizona->regmap, ARIZONA_OUTPUT_ENABLES_1,
				&ep_sel);
		ep_sel &= ARIZONA_EP_SEL_MASK;
		break;
	default:
		break;
	}

	/* Force off if HPDET clamp is active */
	if ((priv->arizona->hpdet_clamp ||
	     priv->arizona->hp_impedance_x100 <=
	     OHM_TO_HOHM(priv->arizona->pdata.hpdet_short_circuit_imp)) &&
			!ep_sel)
		val = 0;

	regmap_update_bits_async(arizona->regmap, ARIZONA_OUTPUT_ENABLES_1,
				 mask, val);

	return arizona_out_ev(w, kcontrol, event);
}
EXPORT_SYMBOL_GPL(arizona_hp_ev);

static int arizona_slim_get_la(struct slim_device *dev, u8 *la)
{
	static const u8 e_addr[] =  {0x00, 0x00, 0x60, 0x63, 0xfa, 0x01 };
	int ret;

	do {
		if (!slim_audio_dev) {
			dev_err(&dev->dev, "Waiting for probe...\n");
			usleep_range(10000, 10001);
			continue;
		}

		ret = slim_get_logical_addr(slim_audio_dev, e_addr,
				sizeof(e_addr), la);
		if (ret != 0) {
			dev_err(&dev->dev, "Waiting for enum...\n");
			usleep_range(10000, 10001);
		}
	} while (!la);

	dev_dbg(&dev->dev, "LA %d\n", *la);

	return 0;
}

#define TX_STREAM_1 134
#define TX_STREAM_2 132
#define TX_STREAM_3 130

#define RX_STREAM_1 128
#define RX_STREAM_2 143
#define RX_STREAM_3 152


static u32 rx_porth1[2], rx_porth2[2], rx_porth3[2], rx_porth1m[1];
static u32 tx_porth1[4], tx_porth2[2], tx_porth3[1];
static u16 rx_handles1[] = { RX_STREAM_1, RX_STREAM_1 + 1 };
static u16 rx_handles2[] = { RX_STREAM_2, RX_STREAM_2 + 1 };
static u16 rx_handles3[] = { RX_STREAM_3, RX_STREAM_3 + 1 };
static u16 tx_handles1[] = { TX_STREAM_1, TX_STREAM_1 + 1,
			     TX_STREAM_1 + 2, TX_STREAM_1 + 3 };
static u16 tx_handles2[] = { TX_STREAM_2, TX_STREAM_2 + 1 };
static u16 tx_handles3[] = { TX_STREAM_3 };
static u16 rx_group1, rx_group2, rx_group3;
static u16 tx_group1, tx_group2, tx_group3;
static u32 rx1_samplerate, rx1_sampleszbits;
static u32 rx2_samplerate, rx2_sampleszbits;

int arizona_slim_tx_ev(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol,
		       int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct slim_ch prop;
	int ret, i;
	u32 *porth;
	u16 *handles, *group;
	int chcnt;

	switch (w->shift) {
	case ARIZONA_SLIMTX1_ENA_SHIFT:
		dev_dbg(codec->dev, "TX1\n");
		mutex_lock(&slim_tx_lock);
		porth = tx_porth1;
		handles = tx_handles1;
		group = &tx_group1;
		chcnt = ARRAY_SIZE(tx_porth1);
		break;
	case ARIZONA_SLIMTX5_ENA_SHIFT:
		dev_dbg(codec->dev, "TX2\n");
		mutex_lock(&slim_tx_lock);
		porth = tx_porth2;
		handles = tx_handles2;
		group = &tx_group2;
		chcnt = ARRAY_SIZE(tx_porth2);
		break;
	case ARIZONA_SLIMTX7_ENA_SHIFT:
		dev_dbg(codec->dev, "TX3\n");
		mutex_lock(&slim_tx_lock);
		porth = tx_porth3;
		handles = tx_handles3;
		group = &tx_group3;
		chcnt = ARRAY_SIZE(tx_porth3);
		break;
	default:
		return 0;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (48000/4000);
	prop.sampleszbits = 16;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(arizona->dev, "Start slimbus TX\n");
		ret = slim_define_ch(slim_audio_dev, &prop, handles, chcnt,
				     true, group);
		if (ret != 0) {
			mutex_unlock(&slim_tx_lock);
			dev_err(arizona->dev, "slim_define_ch() failed: %d\n",
				ret);
			return ret;
		}

		for (i = 0; i < chcnt; i++) {
			ret = slim_connect_src(slim_audio_dev, porth[i],
					       handles[i]);
			if (ret != 0) {
				mutex_unlock(&slim_tx_lock);
				dev_err(arizona->dev, "src connect fail %d: %d\n",
					i, ret);
				return ret;
			}
		}

		ret = slim_control_ch(slim_audio_dev, *group,
					SLIM_CH_ACTIVATE, true);
		mutex_unlock(&slim_tx_lock);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to activate: %d\n", ret);
			return ret;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(arizona->dev, "Stop slimbus Tx\n");
		ret = slim_control_ch(slim_audio_dev, *group,
					SLIM_CH_REMOVE, true);
		if (ret != 0)
			dev_err(arizona->dev, "Failed to remove tx: %d\n", ret);

		mutex_unlock(&slim_tx_lock);
		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(arizona_slim_tx_ev);

int arizona_slim_rx_ev(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol,
		    int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct slim_ch prop;
	int ret, i;
	u32 *porth;
	u16 *handles, *group;
	int chcnt;
	u32 rx_sampleszbits = 16, rx_samplerate = 48000;

	/* BODGE: should do this per port */
	switch (w->shift) {
	case ARIZONA_SLIMRX1_ENA_SHIFT:
		dev_dbg(codec->dev, "RX1\n");
		mutex_lock(&slim_rx_lock);
		porth = rx_porth1;
		handles = rx_handles1;
		group = &rx_group1;
		chcnt = ARRAY_SIZE(rx_porth1);
		rx_sampleszbits = rx1_sampleszbits;
		rx_samplerate = rx1_samplerate;
		break;
	case ARIZONA_SLIMRX3_ENA_SHIFT:
		dev_dbg(codec->dev, "RX1M\n");
		mutex_lock(&slim_rx_lock);
		porth = rx_porth1m;
		handles = rx_handles1;
		group = &rx_group1;
		chcnt = ARRAY_SIZE(rx_porth1m);
		break;
	case ARIZONA_SLIMRX5_ENA_SHIFT:
		dev_dbg(codec->dev, "RX2\n");
		mutex_lock(&slim_rx_lock);
		porth = rx_porth2;
		handles = rx_handles2;
		group = &rx_group2;
		chcnt = ARRAY_SIZE(rx_porth2);
		rx_sampleszbits = rx2_sampleszbits;
		rx_samplerate = rx2_samplerate;
		break;
	case ARIZONA_SLIMRX7_ENA_SHIFT:
		dev_dbg(codec->dev, "RX3\n");
		mutex_lock(&slim_rx_lock);
		porth = rx_porth3;
		handles = rx_handles3;
		group = &rx_group3;
		chcnt = ARRAY_SIZE(rx_porth3);
		break;
	default:
		return 0;
	}

	prop.prot = SLIM_AUTO_ISO;
	prop.baser = SLIM_RATE_4000HZ;
	prop.dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop.auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop.ratem = (rx_samplerate/4000);
	prop.sampleszbits = rx_sampleszbits;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(arizona->dev, "Start slimbus RX, rate=%d, bit=%d\n",
			rx_samplerate, rx_sampleszbits);
		ret = slim_define_ch(slim_audio_dev, &prop, handles, chcnt,
				     true, group);
		if (ret != 0) {
			mutex_unlock(&slim_rx_lock);
			dev_err(arizona->dev, "slim_define_ch() failed: %d\n",
				ret);
			return ret;
		}

		for (i = 0; i < chcnt; i++) {
			ret = slim_connect_sink(slim_audio_dev, &porth[i], 1,
						handles[i]);
			if (ret != 0) {
				mutex_unlock(&slim_rx_lock);
				dev_err(arizona->dev, "sink connect fail %d: %d\n",
					i, ret);
				return ret;
			}
		}

		ret = slim_control_ch(slim_audio_dev, *group,
					SLIM_CH_ACTIVATE, true);
		mutex_unlock(&slim_rx_lock);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to activate: %d\n", ret);
			return ret;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(arizona->dev, "Stop slimbus Rx %x\n", *group);
		ret = slim_control_ch(slim_audio_dev, *group,
					SLIM_CH_REMOVE, true);
		mutex_unlock(&slim_rx_lock);
		if (ret != 0)
			dev_err(arizona->dev, "Failed to remove rx: %d\n", ret);

		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(arizona_slim_rx_ev);

static int arizona_get_channel_map(struct snd_soc_dai *dai,
				   unsigned int *tx_num, unsigned int *tx_slot,
				   unsigned int *rx_num, unsigned int *rx_slot)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(dai->codec);
	struct arizona *arizona = priv->arizona;
	int i, ret;
	u8 laddr;

dev_dbg(arizona->dev, "************\n");
dev_dbg(arizona->dev, "%s!!!!!\n", __func__);
dev_dbg(arizona->dev, "************\n");
	if (slim_logic_addr == 0)
		arizona_slim_get_la(slim_audio_dev, &laddr);
	else
		laddr = slim_logic_addr;

dev_dbg(arizona->dev, "%s logic addr %d\n", __func__, laddr);
	for (i = 0; i < ARRAY_SIZE(rx_porth1); i++) {
		slim_get_slaveport(laddr, i,
				   &rx_porth1[i], SLIM_SINK);
	}

	for (i = 0; i < ARRAY_SIZE(rx_porth1m); i++) {
		slim_get_slaveport(laddr, i + 2,
				   &rx_porth1m[i], SLIM_SINK);
	}

	for (i = 0; i < ARRAY_SIZE(rx_porth2); i++) {
		slim_get_slaveport(laddr, i + 4,
				   &rx_porth2[i], SLIM_SINK);
	}

	for (i = 0; i < ARRAY_SIZE(rx_porth3); i++) {
		slim_get_slaveport(laddr, i + 6,
				   &rx_porth3[i], SLIM_SINK);
	}

	for (i = 0; i < ARRAY_SIZE(tx_porth1); i++) {
		slim_get_slaveport(laddr, i + 8,
				   &tx_porth1[i], SLIM_SRC);
	}

	for (i = 0; i < ARRAY_SIZE(tx_porth2); i++) {
		slim_get_slaveport(laddr, i + 12,
				   &tx_porth2[i], SLIM_SRC);
	}

	for (i = 0; i < ARRAY_SIZE(tx_porth3); i++) {
		slim_get_slaveport(laddr, i + 14,
				   &tx_porth3[i], SLIM_SRC);
	}

	/* This actually allocates the channel or refcounts it if there... */
	for (i = 0; i < ARRAY_SIZE(rx_handles1); i++) {
		ret = slim_query_ch(slim_audio_dev, RX_STREAM_1 + i,
					&rx_handles1[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(rx_handles2); i++) {
		ret = slim_query_ch(slim_audio_dev, RX_STREAM_2 + i,
					&rx_handles2[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(rx_handles3); i++) {
		ret = slim_query_ch(slim_audio_dev, RX_STREAM_3 + i,
					&rx_handles3[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(tx_handles1); i++) {
		ret = slim_query_ch(slim_audio_dev, TX_STREAM_1 + i,
					&tx_handles1[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(tx_handles2); i++) {
		ret = slim_query_ch(slim_audio_dev, TX_STREAM_2 + i,
					&tx_handles2[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(tx_handles3); i++) {
		ret = slim_query_ch(slim_audio_dev, TX_STREAM_3 + i,
					&tx_handles3[i]);
		if (ret != 0) {
			dev_err(arizona->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}
	}

	/* Handle both the playback and capture substreams per DAI*/
	switch (dai->id) {
	case ARIZONA_SLIM1:
		*rx_num = 2;
		rx_slot[0] = RX_STREAM_1;
		rx_slot[1] = RX_STREAM_1 + 1;
		*tx_num = 4;
		tx_slot[0] = TX_STREAM_1;
		tx_slot[1] = TX_STREAM_1 + 1;
		tx_slot[2] = TX_STREAM_1 + 2;
		tx_slot[3] = TX_STREAM_1 + 3;
		break;
	case ARIZONA_SLIM2:
		*rx_num = 2;
		rx_slot[0] = RX_STREAM_2;
		rx_slot[1] = RX_STREAM_2 + 1;
		*tx_num = 2;
		tx_slot[0] = TX_STREAM_2;
		tx_slot[1] = TX_STREAM_2 + 1;
		break;
	case ARIZONA_SLIM3:
		*rx_num = 2;
		rx_slot[0] = RX_STREAM_3;
		rx_slot[1] = RX_STREAM_3 + 1;
		*tx_num = 1;
		tx_slot[0] = TX_STREAM_3;
		break;

	default:
		dev_err(arizona->dev, "get_channel_map unknown dai->id %d",
			dai->id);
		return -EINVAL;
	break;
	}

	return 0;
}

int florida_hp_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		  int event)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		florida_hp_pre_enable(w);
		break;
	case SND_SOC_DAPM_POST_PMU:
		florida_hp_post_enable(w);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		florida_hp_pre_disable(w);
		break;
	case SND_SOC_DAPM_POST_PMD:
		florida_hp_post_disable(w);
		break;
	default:
		return -EINVAL;
	}

	return arizona_hp_ev(w, kcontrol, event);
}
EXPORT_SYMBOL_GPL(florida_hp_ev);

int clearwater_hp_ev(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_PRE_PMD:
		return arizona_hp_ev(w, kcontrol, event);
	case SND_SOC_DAPM_POST_PMU:
		ret = arizona_hp_ev(w, kcontrol, event);
		if (ret < 0)
			return ret;

		clearwater_hp_post_enable(w);
		return 0;
	case SND_SOC_DAPM_POST_PMD:
		ret = arizona_hp_ev(w, kcontrol, event);
		clearwater_hp_post_disable(w);
		return ret;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(clearwater_hp_ev);

int moon_hp_ev(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_PRE_PMD:
		return arizona_hp_ev(w, kcontrol, event);
	case SND_SOC_DAPM_POST_PMU:
		ret = arizona_hp_ev(w, kcontrol, event);
		if (ret < 0)
			return ret;

		moon_analog_post_enable(w);
		return 0;
	case SND_SOC_DAPM_POST_PMD:
		ret = arizona_hp_ev(w, kcontrol, event);
		moon_analog_post_disable(w);
		return ret;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(moon_hp_ev);

int moon_analog_ev(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	int ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_PRE_PMD:
		return arizona_out_ev(w, kcontrol, event);
	case SND_SOC_DAPM_POST_PMU:
		ret = arizona_out_ev(w, kcontrol, event);
		if (ret < 0)
			return ret;

		moon_analog_post_enable(w);
		return 0;
	case SND_SOC_DAPM_POST_PMD:
		ret = arizona_out_ev(w, kcontrol, event);
		moon_analog_post_disable(w);
		return ret;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(moon_analog_ev);

int arizona_anc_ev(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol,
		   int event)
{
	unsigned int mask = 0x3 << w->shift;
	unsigned int val;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		val = 1 << w->shift;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		val = 1 << (w->shift + 1);
		break;
	default:
		return 0;
	}

	snd_soc_update_bits(w->codec, ARIZONA_CLOCK_CONTROL, mask, val);

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_anc_ev);

static int arizona_dvfs_enable(struct snd_soc_codec *codec)
{
	const struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret;

	ret = regulator_set_voltage(arizona->dcvdd, 1800000, 1800000);
	if (ret) {
		dev_err(codec->dev, "Failed to boost DCVDD: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(arizona->regmap,
				 ARIZONA_DYNAMIC_FREQUENCY_SCALING_1,
				 ARIZONA_SUBSYS_MAX_FREQ,
				 ARIZONA_SUBSYS_MAX_FREQ);
	if (ret) {
		dev_err(codec->dev, "Failed to enable subsys max: %d\n", ret);
		regulator_set_voltage(arizona->dcvdd, 1200000, 1800000);
		return ret;
	}

	return 0;
}

static int arizona_dvfs_disable(struct snd_soc_codec *codec)
{
	const struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret;

	ret = regmap_update_bits(arizona->regmap,
				 ARIZONA_DYNAMIC_FREQUENCY_SCALING_1,
				 ARIZONA_SUBSYS_MAX_FREQ, 0);
	if (ret) {
		dev_err(codec->dev, "Failed to disable subsys max: %d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(arizona->dcvdd, 1200000, 1800000);
	if (ret) {
		dev_err(codec->dev, "Failed to unboost DCVDD: %d\n", ret);
		return ret;
	}

	return 0;
}

int arizona_dvfs_up(struct snd_soc_codec *codec, unsigned int flags)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&priv->dvfs_lock);

	if (!priv->dvfs_cached && !priv->dvfs_reqs) {
		ret = arizona_dvfs_enable(codec);
		if (ret)
			goto err;
	}

	priv->dvfs_reqs |= flags;
err:
	mutex_unlock(&priv->dvfs_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dvfs_up);

int arizona_dvfs_down(struct snd_soc_codec *codec, unsigned int flags)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int old_reqs;
	int ret = 0;

	mutex_lock(&priv->dvfs_lock);

	old_reqs = priv->dvfs_reqs;
	priv->dvfs_reqs &= ~flags;

	if (!priv->dvfs_cached && old_reqs && !priv->dvfs_reqs)
		ret = arizona_dvfs_disable(codec);

	mutex_unlock(&priv->dvfs_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dvfs_down);

int arizona_dvfs_sysclk_ev(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&priv->dvfs_lock);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (priv->dvfs_reqs)
			ret = arizona_dvfs_enable(codec);

		priv->dvfs_cached = false;
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* We must ensure DVFS is disabled before the codec goes into
		 * suspend so that we are never in an illegal state of DVFS
		 * enabled without enough DCVDD
		 */
		priv->dvfs_cached = true;

		if (priv->dvfs_reqs)
			ret = arizona_dvfs_disable(codec);
		break;
	default:
		break;
	}

	mutex_unlock(&priv->dvfs_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dvfs_sysclk_ev);

void arizona_init_dvfs(struct arizona_priv *priv)
{
	mutex_init(&priv->dvfs_lock);
}
EXPORT_SYMBOL_GPL(arizona_init_dvfs);

static unsigned int arizona_opclk_ref_48k_rates[] = {
	6144000,
	12288000,
	24576000,
	49152000,
};

static unsigned int arizona_opclk_ref_44k1_rates[] = {
	5644800,
	11289600,
	22579200,
	45158400,
};

static int arizona_set_opclk(struct snd_soc_codec *codec, unsigned int clk,
			     unsigned int freq)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	unsigned int reg;
	unsigned int *rates;
	int ref, refclk;
	unsigned int div, div_incr;

	switch (priv->arizona->type) {
	case WM5102:
	case WM5110:
	case WM8280:
	case WM8998:
	case WM1814:
	case CS47L24:
	case WM1831:
	case WM8997:
		div_incr = 1;
		break;
	default:
		div_incr = 2;
		break;
	}

	switch (clk) {
	case ARIZONA_CLK_OPCLK:
		reg = ARIZONA_OUTPUT_SYSTEM_CLOCK;
		refclk = priv->sysclk;
		break;
	case ARIZONA_CLK_ASYNC_OPCLK:
		reg = ARIZONA_OUTPUT_ASYNC_CLOCK;
		refclk = priv->asyncclk;
		break;
	default:
		return -EINVAL;
	}

	if (refclk % 8000)
		rates = arizona_opclk_ref_44k1_rates;
	else
		rates = arizona_opclk_ref_48k_rates;

	for (ref = 0; ref < ARRAY_SIZE(arizona_opclk_ref_48k_rates) &&
		     rates[ref] <= refclk; ref++) {
		div = div_incr;
		while (rates[ref] / div >= freq && div < 32) {
			if (rates[ref] / div == freq) {
				dev_dbg(codec->dev, "Configured %dHz OPCLK\n",
					freq);
				snd_soc_update_bits(codec, reg,
						    ARIZONA_OPCLK_DIV_MASK |
						    ARIZONA_OPCLK_SEL_MASK,
						    (div <<
						     ARIZONA_OPCLK_DIV_SHIFT) |
						    ref);
				return 0;
			}
			div += div_incr;
		}
	}

	dev_err(codec->dev, "Unable to generate %dHz OPCLK\n", freq);
	return -EINVAL;
}

static int arizona_get_sysclk_setting(unsigned int freq)
{
	switch (freq) {
	case 0:
	case 5644800:
	case 6144000:
		return 0;
	case 11289600:
	case 12288000:
		return ARIZONA_CLK_12MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 22579200:
	case 24576000:
		return ARIZONA_CLK_24MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 45158400:
	case 49152000:
		return ARIZONA_CLK_49MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 67737600:
	case 73728000:
		return ARIZONA_CLK_73MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 90316800:
	case 98304000:
		return ARIZONA_CLK_98MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 135475200:
	case 147456000:
		return ARIZONA_CLK_147MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	default:
		return -EINVAL;
	}
}

static int clearwater_get_sysclk_setting(unsigned int freq)
{
	switch (freq) {
	case 0:
	case 5644800:
	case 6144000:
		return 0;
	case 11289600:
	case 12288000:
		return ARIZONA_CLK_12MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 22579200:
	case 24576000:
		return ARIZONA_CLK_24MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 45158400:
	case 49152000:
		return ARIZONA_CLK_49MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	case 90316800:
	case 98304000:
		return CLEARWATER_CLK_98MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	default:
		return -EINVAL;
	}
}

static int clearwater_get_dspclk_setting(unsigned int freq,
					 struct arizona *arizona,
					 int source)
{
	switch (freq) {
	case 0:
		return 0;
	case 45158400:
	case 49152000:
		switch (arizona->type) {
		case WM1840:
		case WM8285:
			if (arizona->rev >= 3 &&
			    source == CLEARWATER_CLK_SRC_FLL1_DIV6)
				return ARIZONA_CLK_49MHZ <<
				       ARIZONA_SYSCLK_FREQ_SHIFT;
			else
				return -EINVAL;
		default:
			return -EINVAL;
		}
	case 135475200:
	case 147456000:
		return CLEARWATER_DSP_CLK_147MHZ << ARIZONA_SYSCLK_FREQ_SHIFT;
	default:
		return -EINVAL;
	}
}

static void clearwater_get_dsp_reg_seq(unsigned int cur, unsigned int tar,
				       unsigned int reg, unsigned int mask,
				       struct reg_default *s)
{
	/* To transition DSPCLK to a new source and frequency we must:
	 * - Disable DSPCLK_ENA
	 * - Wait 34us
	 * - Write the new source, freq and enable in one write
	 */
	unsigned int tmp;

	mask |= CLEARWATER_DSP_CLK_ENA_MASK;

	s[0].reg = reg;
	s[0].def = (cur & ~CLEARWATER_DSP_CLK_ENA_MASK);

	/* Clear the fields we care about */
	tmp = (cur & ~mask);

	/* Update the fields */
	tmp |= tar & mask;

	/* Re-set the enable bit */
	tmp |= CLEARWATER_DSP_CLK_ENA_MASK;

	s[1].reg = reg;
	s[1].def = tmp;
}

static int moon_get_dspclk_setting(unsigned int freq, unsigned int *val)
{
	if (freq > 150000000)
		return -EINVAL;

	/* freq * (2^6) / (10^6) */
	*val = freq / 15625;

	return 0;
}

int arizona_set_sysclk(struct snd_soc_codec *codec, int clk_id,
		       int source, unsigned int freq, int dir)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int ret = 0;
	char *name;
	unsigned int reg;
	unsigned int reg2, val2;
	unsigned int mask = ARIZONA_SYSCLK_FREQ_MASK | ARIZONA_SYSCLK_SRC_MASK;
	unsigned int val = source << ARIZONA_SYSCLK_SRC_SHIFT;
	int clk_freq;
	int *clk;
	unsigned int cw_dspclk_change = 0;
	unsigned int cw_dspclk_val;
	struct reg_default cw_dspclk_seq[2];

	reg2 = val2 = 0;

	switch (arizona->type) {
	case WM8997:
	case WM8998:
	case WM1814:
	case WM5102:
	case WM8280:
	case WM5110:
	case WM1831:
	case CS47L24:
		switch (clk_id) {
		case ARIZONA_CLK_SYSCLK:
			name = "SYSCLK";
			reg = ARIZONA_SYSTEM_CLOCK_1;
			clk = &priv->sysclk;
			mask |= ARIZONA_SYSCLK_FRAC;
			clk_freq = arizona_get_sysclk_setting(freq);
			break;
		case ARIZONA_CLK_ASYNCCLK:
			name = "ASYNCCLK";
			reg = ARIZONA_ASYNC_CLOCK_1;
			clk = &priv->asyncclk;
			clk_freq = arizona_get_sysclk_setting(freq);
			break;
		case ARIZONA_CLK_OPCLK:
		case ARIZONA_CLK_ASYNC_OPCLK:
			return arizona_set_opclk(codec, clk_id, freq);
		default:
			return -EINVAL;
		}
		break;
	case CS47L35:
	case WM8285:
	case WM1840:
		switch (clk_id) {
		case ARIZONA_CLK_SYSCLK:
			name = "SYSCLK";
			reg = ARIZONA_SYSTEM_CLOCK_1;
			clk = &priv->sysclk;
			clk_freq = clearwater_get_sysclk_setting(freq);
			mask |= ARIZONA_SYSCLK_FRAC;
			break;
		case ARIZONA_CLK_ASYNCCLK:
			name = "ASYNCCLK";
			reg = ARIZONA_ASYNC_CLOCK_1;
			clk = &priv->asyncclk;
			clk_freq = clearwater_get_sysclk_setting(freq);
			break;
		case ARIZONA_CLK_OPCLK:
		case ARIZONA_CLK_ASYNC_OPCLK:
			return arizona_set_opclk(codec, clk_id, freq);
		case ARIZONA_CLK_DSPCLK:
			name = "DSPCLK";
			reg = CLEARWATER_DSP_CLOCK_1;
			clk = &priv->dspclk;
			clk_freq = clearwater_get_dspclk_setting(freq,
						arizona,
						source);
			switch (arizona->type) {
			case WM1840:
			case WM8285:
				cw_dspclk_change = 1;
				break;
			default:
				break;
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		switch (clk_id) {
		case ARIZONA_CLK_SYSCLK:
			name = "SYSCLK";
			reg = ARIZONA_SYSTEM_CLOCK_1;
			clk = &priv->sysclk;
			clk_freq = clearwater_get_sysclk_setting(freq);
			mask |= ARIZONA_SYSCLK_FRAC;
			break;
		case ARIZONA_CLK_ASYNCCLK:
			name = "ASYNCCLK";
			reg = ARIZONA_ASYNC_CLOCK_1;
			clk = &priv->asyncclk;
			clk_freq = clearwater_get_sysclk_setting(freq);
			break;
		case ARIZONA_CLK_OPCLK:
		case ARIZONA_CLK_ASYNC_OPCLK:
			return arizona_set_opclk(codec, clk_id, freq);
		case ARIZONA_CLK_DSPCLK:
			name = "DSPCLK";
			reg = CLEARWATER_DSP_CLOCK_1;
			mask = ARIZONA_SYSCLK_SRC_MASK;
			reg2 = CLEARWATER_DSP_CLOCK_2;
			clk = &priv->dspclk;
			ret = moon_get_dspclk_setting(freq, &val2);
			break;
		default:
			return -EINVAL;
		}
		break;
	}

	if (reg2) {
		if (ret < 0) {
			dev_err(arizona->dev, "Failed to get clk setting for %dHZ\n",
				freq);
			return ret;
		}
		ret = regmap_write(arizona->regmap,
				   reg2, val2);
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to set dsp freq to %d\n", val2);
			return ret;
		}
	} else {
		if (clk_freq < 0) {
			dev_err(arizona->dev, "Failed to get clk setting for %dHZ\n",
				freq);
			return ret;
		}

		val |= clk_freq;
	}

	if (freq == 0) {
		dev_dbg(arizona->dev, "%s cleared\n", name);
		*clk = freq;
		return 0;
	}

	*clk = freq;

	if (freq % 6144000)
		val |= ARIZONA_SYSCLK_FRAC;

	dev_dbg(arizona->dev, "%s set to %uHz", name, freq);

	/* For the cases where we are changing DSPCLK on the fly we need to
	 * make sure DSPCLK_ENA is disabled for at least 32uS before changing it
	 */
	if (cw_dspclk_change) {
		mutex_lock(&arizona->dspclk_ena_lock);

		/* Is DSPCLK_ENA on? */
		ret = regmap_read(arizona->regmap, reg, &cw_dspclk_val);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to read 0x%04x: %d\n",
				reg, ret);
			goto err;
		}

		if (cw_dspclk_val & CLEARWATER_DSP_CLK_ENA_MASK) {
			clearwater_get_dsp_reg_seq(cw_dspclk_val, val, reg, mask,
						    cw_dspclk_seq);

			ret = regmap_multi_reg_write(arizona->regmap,
						     &cw_dspclk_seq[0],
						     1);

			if (ret != 0) {
				dev_err(arizona->dev,
					"Failed to write dspclk_seq 1: %d\n",
					ret);
				goto err;
			}
			usleep_range(34, 35);
			ret = regmap_multi_reg_write(arizona->regmap,
						     &cw_dspclk_seq[1],
						     1);

			if (ret != 0) {
				dev_err(arizona->dev,
					"Failed to write dspclk_seq 2: %d\n",
					ret);
				goto err;
			}
		} else {
			ret = regmap_update_bits(arizona->regmap, reg, mask,
						 val);
		}

		mutex_unlock(&arizona->dspclk_ena_lock);
	} else {
		ret = regmap_update_bits(arizona->regmap, reg, mask, val);
	}

	return ret;

err:
	mutex_unlock(&arizona->dspclk_ena_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_set_sysclk);

static int arizona_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int lrclk, bclk, mode, base;
	unsigned int mask;

	base = dai->driver->base;

	lrclk = 0;
	bclk = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		mode = ARIZONA_FMT_DSP_MODE_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
				!= SND_SOC_DAIFMT_CBM_CFM) {
			arizona_aif_err(dai, "DSP_B not valid in slave mode\n");
			return -EINVAL;
		}
		mode = ARIZONA_FMT_DSP_MODE_B;
		break;
	case SND_SOC_DAIFMT_I2S:
		mode = ARIZONA_FMT_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
				!= SND_SOC_DAIFMT_CBM_CFM) {
			arizona_aif_err(dai, "LEFT_J not valid in slave mode\n");
			return -EINVAL;
		}
		mode = ARIZONA_FMT_LEFT_JUSTIFIED_MODE;
		break;
	default:
		arizona_aif_err(dai, "Unsupported DAI format %d\n",
				fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		lrclk |= ARIZONA_AIF1TX_LRCLK_MSTR;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		bclk |= ARIZONA_AIF1_BCLK_MSTR;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		bclk |= ARIZONA_AIF1_BCLK_MSTR;
		lrclk |= ARIZONA_AIF1TX_LRCLK_MSTR;
		break;
	default:
		arizona_aif_err(dai, "Unsupported master mode %d\n",
				fmt & SND_SOC_DAIFMT_MASTER_MASK);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		bclk |= ARIZONA_AIF1_BCLK_INV;
		lrclk |= ARIZONA_AIF1TX_LRCLK_INV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		bclk |= ARIZONA_AIF1_BCLK_INV;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		lrclk |= ARIZONA_AIF1TX_LRCLK_INV;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits_async(arizona->regmap, base + ARIZONA_AIF_BCLK_CTRL,
				 ARIZONA_AIF1_BCLK_INV |
				 ARIZONA_AIF1_BCLK_MSTR,
				 bclk);
	regmap_update_bits_async(arizona->regmap, base + ARIZONA_AIF_TX_PIN_CTRL,
				 ARIZONA_AIF1TX_LRCLK_INV |
				 ARIZONA_AIF1TX_LRCLK_MSTR, lrclk);

	mask = ARIZONA_AIF1RX_LRCLK_INV | ARIZONA_AIF1RX_LRCLK_MSTR;
	switch (arizona->type) {
	case CS47L90:
	case CS47L91:
		mask |= ARIZONA_AIF1RX_LRCLK_ADV;
		if (arizona->pdata.lrclk_adv[dai->id - 1] &&
			mode == SND_SOC_DAIFMT_DSP_A)
			lrclk |= ARIZONA_AIF1RX_LRCLK_ADV;
		break;
	default:
		break;
	}

	regmap_update_bits_async(arizona->regmap,
				 base + ARIZONA_AIF_RX_PIN_CTRL,
				 mask, lrclk);
	regmap_update_bits(arizona->regmap, base + ARIZONA_AIF_FORMAT,
			   ARIZONA_AIF1_FMT_MASK, mode);

	return 0;
}

static const int arizona_48k_bclk_rates[] = {
	-1,
	48000,
	64000,
	96000,
	128000,
	192000,
	256000,
	384000,
	512000,
	768000,
	1024000,
	1536000,
	2048000,
	3072000,
	4096000,
	6144000,
	8192000,
	12288000,
	24576000,
};

static const unsigned int arizona_48k_rates[] = {
	12000,
	24000,
	48000,
	96000,
	192000,
	384000,
	768000,
	4000,
	8000,
	16000,
	32000,
	64000,
	128000,
	256000,
	512000,
};

static const struct snd_pcm_hw_constraint_list arizona_48k_constraint = {
	.count	= ARRAY_SIZE(arizona_48k_rates),
	.list	= arizona_48k_rates,
};

static const int arizona_44k1_bclk_rates[] = {
	-1,
	44100,
	58800,
	88200,
	117600,
	177640,
	235200,
	352800,
	470400,
	705600,
	940800,
	1411200,
	1881600,
	2822400,
	3763200,
	5644800,
	7526400,
	11289600,
	22579200,
};

static const unsigned int arizona_44k1_rates[] = {
	11025,
	22050,
	44100,
	88200,
	176400,
	352800,
	705600,
};

static const struct snd_pcm_hw_constraint_list arizona_44k1_constraint = {
	.count	= ARRAY_SIZE(arizona_44k1_rates),
	.list	= arizona_44k1_rates,
};

static int arizona_sr_vals[] = {
	0,
	12000,
	24000,
	48000,
	96000,
	192000,
	384000,
	768000,
	0,
	11025,
	22050,
	44100,
	88200,
	176400,
	352800,
	705600,
	4000,
	8000,
	16000,
	32000,
	64000,
	128000,
	256000,
	512000,
};

int arizona_put_sample_rate_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned int flag;
	int ret;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);
	if (ret == 0)
		return 0;	/* register value wasn't changed */

	val = e->values[ucontrol->value.enumerated.item[0]];

	switch (e->reg) {
	case ARIZONA_SAMPLE_RATE_2:
		flag = ARIZONA_DVFS_SR2_RQ;
		break;

	case ARIZONA_SAMPLE_RATE_3:
		flag = ARIZONA_DVFS_SR3_RQ;
		break;

	case ARIZONA_ASYNC_SAMPLE_RATE_2:
		flag = ARIZONA_DVFS_ASR2_RQ;
		break;

	default:
		return ret;
	}

	if (arizona_sr_vals[val] >= 88200) {
		ret = arizona_dvfs_up(codec, flag);
		if (ret != 0)
			dev_err(codec->dev, "Failed to raise DVFS %d\n", ret);
	} else {
		ret = arizona_dvfs_down(codec, flag);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_put_sample_rate_enum);

static int arizona_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona_dai_priv *dai_priv = &priv->dai[dai->id - 1];
	const struct snd_pcm_hw_constraint_list *constraint;
	unsigned int base_rate;

	switch (dai_priv->clk) {
	case ARIZONA_CLK_SYSCLK:
	case ARIZONA_CLK_SYSCLK_2:
	case ARIZONA_CLK_SYSCLK_3:
		base_rate = priv->sysclk;
		break;
	case ARIZONA_CLK_ASYNCCLK:
	case ARIZONA_CLK_ASYNCCLK_2:
		base_rate = priv->asyncclk;
		break;
	default:
		return 0;
	}

	if (base_rate == 0)
		return 0;

	if (base_rate % 8000)
		constraint = &arizona_44k1_constraint;
	else
		constraint = &arizona_48k_constraint;

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
					  SNDRV_PCM_HW_PARAM_RATE,
					  constraint);
}

static void arizona_wm5102_set_dac_comp(struct snd_soc_codec *codec,
					unsigned int rate)
{
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct reg_default dac_comp[] = {
		{ 0x80, 0x3 },
		{ ARIZONA_DAC_COMP_1, 0 },
		{ ARIZONA_DAC_COMP_2, 0 },
		{ 0x80, 0x0 },
	};

	switch (arizona->type) {
	case WM5102:
		break;
	default:
		return;
	}

	mutex_lock(&codec->mutex);

	dac_comp[1].def = arizona->dac_comp_coeff;
	if (rate >= 176400)
		dac_comp[2].def = arizona->dac_comp_enabled;

	mutex_unlock(&codec->mutex);

	regmap_multi_reg_write(arizona->regmap,
			       dac_comp,
			       ARRAY_SIZE(dac_comp));
}

static int arizona_hw_params_rate(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	struct arizona_dai_priv *dai_priv = &priv->dai[dai->id - 1];
	int base = dai->driver->base;
	int ret = 0, err;
	int i, sr_val, lim = 0;
	const int *sources = NULL;
	unsigned int cur, tar;
	bool change_rate = true, slim_dai = false;
	u32 rx_sampleszbits, rx_samplerate;

	rx_sampleszbits = snd_pcm_format_width(params_format(params));
	if (rx_sampleszbits < 16)
		rx_sampleszbits = 16;

	/*
	 * We will need to be more flexible than this in future,
	 * currently we use a single sample rate for SYSCLK.
	 */
	for (i = 0; i < ARRAY_SIZE(arizona_sr_vals); i++)
		if (arizona_sr_vals[i] == params_rate(params)) {
			rx_samplerate = params_rate(params);
			break;
		}
	if (i == ARRAY_SIZE(arizona_sr_vals)) {
		arizona_aif_err(dai, "Unsupported sample rate %dHz\n",
				params_rate(params));
		return -EINVAL;
	}
	sr_val = i;

	switch (dai->id) {
	case ARIZONA_SLIM1:
		rx1_sampleszbits = rx_sampleszbits;
		rx1_samplerate = rx_samplerate;
		break;
	case ARIZONA_SLIM2:
		rx2_sampleszbits = rx_sampleszbits;
		rx2_samplerate = rx_samplerate;
		break;
	case ARIZONA_SLIM3:
	default:
		break;
	}


	switch (priv->arizona->type) {
	case WM5102:
		if (priv->arizona->pdata.ultrasonic_response) {
			mutex_lock(&arizona->reg_setting_lock);
			snd_soc_write(codec, 0x80, 0x3);
			if (params_rate(params) >= 176400)
				snd_soc_write(codec, 0x4dd, 0x1);
			else
				snd_soc_write(codec, 0x4dd, 0x0);
			snd_soc_write(codec, 0x80, 0x0);
			mutex_unlock(&arizona->reg_setting_lock);
		}
		break;

	default:
		break;
	}

	switch (priv->arizona->type) {
	case WM5102:
	case WM8997:
	case WM8998:
	case WM1814:
		if (arizona_sr_vals[sr_val] >= 88200)
			ret = arizona_dvfs_up(codec,
					      ARIZONA_DVFS_SR1_RQ);
		else
			ret = arizona_dvfs_down(codec,
						ARIZONA_DVFS_SR1_RQ);

		if (ret != 0) {
			arizona_aif_err(dai, "Failed to change DVFS %d\n", ret);
			return ret;
		}
		break;

	default:
		break;
	}

	switch (dai_priv->clk) {
	case ARIZONA_CLK_SYSCLK:
		tar = 0 << ARIZONA_AIF1_RATE_SHIFT;
		break;
	case ARIZONA_CLK_SYSCLK_2:
		tar = 1 << ARIZONA_AIF1_RATE_SHIFT;
		break;
	case ARIZONA_CLK_SYSCLK_3:
		tar = 2 << ARIZONA_AIF1_RATE_SHIFT;
		break;
	case ARIZONA_CLK_ASYNCCLK:
		tar = 8 << ARIZONA_AIF1_RATE_SHIFT;
		break;
	case ARIZONA_CLK_ASYNCCLK_2:
		tar = 9 << ARIZONA_AIF1_RATE_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	/* If a Slimbus DAI, then there is no need to AIF Rates */
	if (strnstr(dai->name, "-slim", strlen(dai->name)))
		slim_dai = true;

	if (!slim_dai) {
		ret = regmap_read(priv->arizona->regmap,
			  base + ARIZONA_AIF_RATE_CTRL, &cur);
		if (ret != 0) {
			arizona_aif_err(dai, "Failed to check rate: %d\n", ret);
			return ret;
		}
	}

	if ((cur & ARIZONA_AIF1_RATE_MASK) == (tar & ARIZONA_AIF1_RATE_MASK))
		change_rate = false;

	if (change_rate && !slim_dai) {
		ret = arizona_get_sources(priv->arizona,
					  dai,
					  &sources,
					  &lim);
		if (ret != 0) {
			arizona_aif_err(dai,
					"Failed to get aif sources %d\n",
					ret);
			return ret;
		}

		mutex_lock(&priv->arizona->rate_lock);

		ret = arizona_cache_and_clear_sources(priv->arizona, sources,
						      arizona_aif_sources_cache,
						      lim);
		if (ret != 0) {
			arizona_aif_err(dai,
				"Failed to cache and clear aif sources: %d\n",
				ret);
			goto out;
		}

		clearwater_spin_sysclk(priv->arizona);
		udelay(300);
	}

	switch (dai_priv->clk) {
	case ARIZONA_CLK_SYSCLK:
		arizona_wm5102_set_dac_comp(codec, params_rate(params));

		snd_soc_update_bits(codec, ARIZONA_SAMPLE_RATE_1,
				    ARIZONA_SAMPLE_RATE_1_MASK, sr_val);
		if (base && !slim_dai)
			snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
					    ARIZONA_AIF1_RATE_MASK,
					    0 << ARIZONA_AIF1_RATE_SHIFT);
		break;
	case ARIZONA_CLK_SYSCLK_2:
		arizona_wm5102_set_dac_comp(codec, params_rate(params));

		snd_soc_update_bits(codec, ARIZONA_SAMPLE_RATE_2,
				    ARIZONA_SAMPLE_RATE_2_MASK, sr_val);
		if (base && !slim_dai)
			snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
					    ARIZONA_AIF1_RATE_MASK,
					    1 << ARIZONA_AIF1_RATE_SHIFT);
		break;
	case ARIZONA_CLK_SYSCLK_3:
		arizona_wm5102_set_dac_comp(codec, params_rate(params));

		snd_soc_update_bits(codec, ARIZONA_SAMPLE_RATE_3,
				    ARIZONA_SAMPLE_RATE_3_MASK, sr_val);
		if (base && !slim_dai)
			snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
					    ARIZONA_AIF1_RATE_MASK,
					    2 << ARIZONA_AIF1_RATE_SHIFT);
		break;
	case ARIZONA_CLK_ASYNCCLK:
		snd_soc_update_bits(codec, ARIZONA_ASYNC_SAMPLE_RATE_1,
				    ARIZONA_ASYNC_SAMPLE_RATE_1_MASK, sr_val);
		if (base && !slim_dai)
			snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
					    ARIZONA_AIF1_RATE_MASK,
					    8 << ARIZONA_AIF1_RATE_SHIFT);
		break;
	case ARIZONA_CLK_ASYNCCLK_2:
		snd_soc_update_bits(codec, ARIZONA_ASYNC_SAMPLE_RATE_2,
				    ARIZONA_ASYNC_SAMPLE_RATE_2_MASK, sr_val);
		if (base && !slim_dai)
			snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
					    ARIZONA_AIF1_RATE_MASK,
					    9 << ARIZONA_AIF1_RATE_SHIFT);
		break;
	default:
		arizona_aif_err(dai, "Invalid clock %d\n", dai_priv->clk);
		ret = -EINVAL;
	}

	if (change_rate) {
		clearwater_spin_sysclk(priv->arizona);
		udelay(300);
	}

out:
	if (change_rate && !slim_dai) {
		err = arizona_restore_sources(priv->arizona, sources,
					      arizona_aif_sources_cache, lim);
		if (err != 0) {
			arizona_aif_err(dai,
					"Failed to restore sources: %d\n",
					err);
		}

		mutex_unlock(&priv->arizona->rate_lock);
	}
	return ret;
}

static bool arizona_aif_cfg_changed(struct snd_soc_codec *codec,
				    int base, int bclk, int lrclk, int frame)
{
	int val;

	val = snd_soc_read(codec, base + ARIZONA_AIF_BCLK_CTRL);
	if (bclk != (val & ARIZONA_AIF1_BCLK_FREQ_MASK))
		return true;

	val = snd_soc_read(codec, base + ARIZONA_AIF_TX_BCLK_RATE);
	if (lrclk != (val & ARIZONA_AIF1TX_BCPF_MASK))
		return true;

	val = snd_soc_read(codec, base + ARIZONA_AIF_FRAME_CTRL_1);
	if (frame != (val & (ARIZONA_AIF1TX_WL_MASK |
			     ARIZONA_AIF1TX_SLOT_LEN_MASK)))
		return true;

	return false;
}

static int arizona_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int base = dai->driver->base;
	const int *rates;
	int i, ret, val;
	int channels = params_channels(params);
	int chan_limit = arizona->pdata.max_channels_clocked[dai->id - 1];
	int tdm_width = arizona->tdm_width[dai->id - 1];
	int tdm_slots = arizona->tdm_slots[dai->id - 1];
	int bclk, lrclk, wl, frame, bclk_target;
	bool reconfig;
	unsigned int aif_tx_state = 0, aif_rx_state = 0;

	if (params_rate(params) % 4000)
		rates = &arizona_44k1_bclk_rates[0];
	else
		rates = &arizona_48k_bclk_rates[0];

	wl = snd_pcm_format_width(params_format(params));

	if (tdm_slots) {
		arizona_aif_dbg(dai, "Configuring for %d %d bit TDM slots\n",
				tdm_slots, tdm_width);
		bclk_target = tdm_slots * tdm_width * params_rate(params);
		channels = tdm_slots;
	} else {
		bclk_target = snd_soc_params_to_bclk(params);
		tdm_width = wl;
	}

	/* Force width to be 16 bit if params pass 8 bit */
	if (wl == 8) {
		wl *= 2;
		bclk_target *= 2;
		tdm_width = wl;
	}

	if (chan_limit && chan_limit < channels) {
		arizona_aif_dbg(dai, "Limiting to %d channels\n", chan_limit);
		bclk_target /= channels;
		bclk_target *= chan_limit;
	}

	/* Force multiple of 2 channels for I2S mode */
	val = snd_soc_read(codec, base + ARIZONA_AIF_FORMAT);
	val &= ARIZONA_AIF1_FMT_MASK;
	if ((channels & 1) && (val == ARIZONA_FMT_I2S_MODE)) {
		arizona_aif_dbg(dai, "Forcing stereo mode\n");
		bclk_target /= channels;
		bclk_target *= channels + 1;
	}

	for (i = 0; i < ARRAY_SIZE(arizona_44k1_bclk_rates); i++) {
		if (rates[i] >= bclk_target &&
		    rates[i] % params_rate(params) == 0) {
			bclk = i;
			break;
		}
	}
	if (i == ARRAY_SIZE(arizona_44k1_bclk_rates)) {
		arizona_aif_err(dai, "Unsupported sample rate %dHz\n",
				params_rate(params));
		return -EINVAL;
	}

	lrclk = rates[bclk] / params_rate(params);

	arizona_aif_dbg(dai, "BCLK %dHz LRCLK %dHz\n",
			rates[bclk], rates[bclk] / lrclk);

	frame = wl << ARIZONA_AIF1TX_WL_SHIFT | tdm_width;

	reconfig = arizona_aif_cfg_changed(codec, base, bclk, lrclk, frame);

	if (reconfig) {
		/* Save AIF TX/RX state */
		aif_tx_state = snd_soc_read(codec,
					    base + ARIZONA_AIF_TX_ENABLES);
		aif_rx_state = snd_soc_read(codec,
					    base + ARIZONA_AIF_RX_ENABLES);
		/* Disable AIF TX/RX before reconfiguring it */
		regmap_update_bits_async(arizona->regmap,
				    base + ARIZONA_AIF_TX_ENABLES, 0xff, 0x0);
		regmap_update_bits(arizona->regmap,
				    base + ARIZONA_AIF_RX_ENABLES, 0xff, 0x0);
	}

	ret = arizona_hw_params_rate(substream, params, dai);
	if (ret != 0)
		goto restore_aif;

	if (reconfig) {
		regmap_update_bits_async(arizona->regmap,
					 base + ARIZONA_AIF_BCLK_CTRL,
					 ARIZONA_AIF1_BCLK_FREQ_MASK, bclk);
		regmap_update_bits_async(arizona->regmap,
					 base + ARIZONA_AIF_TX_BCLK_RATE,
					 ARIZONA_AIF1TX_BCPF_MASK, lrclk);
		regmap_update_bits_async(arizona->regmap,
					 base + ARIZONA_AIF_RX_BCLK_RATE,
					 ARIZONA_AIF1RX_BCPF_MASK, lrclk);
		regmap_update_bits_async(arizona->regmap,
					 base + ARIZONA_AIF_FRAME_CTRL_1,
					 ARIZONA_AIF1TX_WL_MASK |
					 ARIZONA_AIF1TX_SLOT_LEN_MASK, frame);
		regmap_update_bits(arizona->regmap,
				   base + ARIZONA_AIF_FRAME_CTRL_2,
				   ARIZONA_AIF1RX_WL_MASK |
				   ARIZONA_AIF1RX_SLOT_LEN_MASK, frame);
	}

restore_aif:
	if (reconfig) {
		/* Restore AIF TX/RX state */
		regmap_update_bits_async(arizona->regmap,
					 base + ARIZONA_AIF_TX_ENABLES,
					 0xff, aif_tx_state);
		regmap_update_bits(arizona->regmap,
				   base + ARIZONA_AIF_RX_ENABLES,
				   0xff, aif_rx_state);
	}
	return ret;
}

static const char * const arizona_dai_clk_str(int clk_id)
{
	switch (clk_id) {
	case ARIZONA_CLK_SYSCLK:
	case ARIZONA_CLK_SYSCLK_2:
	case ARIZONA_CLK_SYSCLK_3:
		return "SYSCLK";
	case ARIZONA_CLK_ASYNCCLK:
	case ARIZONA_CLK_ASYNCCLK_2:
		return "ASYNCCLK";
	default:
		return "Unknown clock";
	}
}

static int arizona_dai_set_sysclk(struct snd_soc_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona_dai_priv *dai_priv = &priv->dai[dai->id - 1];
	struct snd_soc_dapm_route routes[2];

	switch (clk_id) {
	case ARIZONA_CLK_SYSCLK:
	case ARIZONA_CLK_SYSCLK_2:
	case ARIZONA_CLK_SYSCLK_3:
	case ARIZONA_CLK_ASYNCCLK:
	case ARIZONA_CLK_ASYNCCLK_2:
		break;
	default:
		return -EINVAL;
	}

	if (clk_id == dai_priv->clk)
		return 0;

	if (dai->active) {
		dev_err(codec->dev, "Can't change clock on active DAI %d\n",
			dai->id);
		return -EBUSY;
	}

	dev_dbg(codec->dev, "Setting AIF%d to %s\n", dai->id + 1,
		arizona_dai_clk_str(clk_id));

	memset(&routes, 0, sizeof(routes));
	routes[0].sink = dai->driver->capture.stream_name;
	routes[1].sink = dai->driver->playback.stream_name;

	switch (clk_id) {
	case ARIZONA_CLK_SYSCLK:
	case ARIZONA_CLK_SYSCLK_2:
	case ARIZONA_CLK_SYSCLK_3:
		routes[0].source = arizona_dai_clk_str(dai_priv->clk);
		routes[1].source = arizona_dai_clk_str(dai_priv->clk);
		snd_soc_dapm_del_routes(&codec->dapm, routes,
					ARRAY_SIZE(routes));
		break;
	default:
		break;
	}

	switch (clk_id) {
	case ARIZONA_CLK_ASYNCCLK:
	case ARIZONA_CLK_ASYNCCLK_2:
		routes[0].source = arizona_dai_clk_str(clk_id);
		routes[1].source = arizona_dai_clk_str(clk_id);
		snd_soc_dapm_add_routes(&codec->dapm, routes,
					ARRAY_SIZE(routes));
		break;
	default:
		break;
	}

	dai_priv->clk = clk_id;

	return snd_soc_dapm_sync(&codec->dapm);
}

static int arizona_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_codec *codec = dai->codec;
	int base = dai->driver->base;
	unsigned int reg;

	if (tristate)
		reg = ARIZONA_AIF1_TRI;
	else
		reg = 0;

	return snd_soc_update_bits(codec, base + ARIZONA_AIF_RATE_CTRL,
				   ARIZONA_AIF1_TRI, reg);
}

static void arizona_set_channels_to_mask(struct snd_soc_dai *dai,
					 unsigned int base,
					 int channels, unsigned int mask)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int slot, i;

	for (i = 0; i < channels; ++i) {
		slot = ffs(mask) - 1;
		if (slot < 0)
			return;

		regmap_write(arizona->regmap, base + i, slot);

		mask &= ~(1 << slot);
	}

	if (mask)
		arizona_aif_warn(dai, "Too many channels in TDM mask\n");
}

static int arizona_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	struct arizona_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct arizona *arizona = priv->arizona;
	int base = dai->driver->base;
	int rx_max_chan = dai->driver->playback.channels_max;
	int tx_max_chan = dai->driver->capture.channels_max;

	/* Only support TDM for the physical AIFs */
	if (dai->id > ARIZONA_MAX_AIF)
		return -ENOTSUPP;

	if (slots == 0) {
		tx_mask = (1 << tx_max_chan) - 1;
		rx_mask = (1 << rx_max_chan) - 1;
	}

	arizona_set_channels_to_mask(dai, base + ARIZONA_AIF_FRAME_CTRL_3,
				     tx_max_chan, tx_mask);
	arizona_set_channels_to_mask(dai, base + ARIZONA_AIF_FRAME_CTRL_11,
				     rx_max_chan, rx_mask);

	arizona->tdm_width[dai->id - 1] = slot_width;
	arizona->tdm_slots[dai->id - 1] = slots;

	return 0;
}

const struct snd_soc_dai_ops arizona_dai_ops = {
	.startup = arizona_startup,
	.set_fmt = arizona_set_fmt,
	.set_tdm_slot = arizona_set_tdm_slot,
	.hw_params = arizona_hw_params,
	.set_sysclk = arizona_dai_set_sysclk,
	.set_tristate = arizona_set_tristate,
};
EXPORT_SYMBOL_GPL(arizona_dai_ops);

const struct snd_soc_dai_ops arizona_simple_dai_ops = {
	.startup = arizona_startup,
	.hw_params = arizona_hw_params_rate,
	.set_sysclk = arizona_dai_set_sysclk,
};
EXPORT_SYMBOL_GPL(arizona_simple_dai_ops);

const struct snd_soc_dai_ops arizona_slim_dai_ops = {
	.hw_params = arizona_hw_params_rate,
	.get_channel_map = arizona_get_channel_map,
};
EXPORT_SYMBOL_GPL(arizona_slim_dai_ops);


int arizona_init_dai(struct arizona_priv *priv, int id)
{
	struct arizona_dai_priv *dai_priv = &priv->dai[id];

	dai_priv->clk = ARIZONA_CLK_SYSCLK;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_dai);

static struct {
	unsigned int min;
	unsigned int max;
	u16 fratio;
	int ratio;
} fll_fratios[] = {
	{       0,    64000, 4, 16 },
	{   64000,   128000, 3,  8 },
	{  128000,   256000, 2,  4 },
	{  256000,  1000000, 1,  2 },
	{ 1000000, 13500000, 0,  1 },
};

struct arizona_fll_gain {
	unsigned int min;
	unsigned int max;
	u16 gain;
};

static struct arizona_fll_gain fll_gains[] = {
	{       0,   256000, 0 },
	{  256000,  1000000, 2 },
	{ 1000000, 13500000, 4 },
};

static struct arizona_fll_gain fll_moon_gains[] = {
	{       0,   100000, 0 },
	{  100000,   375000, 2 },
	{  375000,  1500000, 3 },
	{ 1500000,  6000000, 4 },
	{ 6000000, 13500000, 5 },
};

static int arizona_validate_fll(struct arizona_fll *fll,
				unsigned int fin,
				unsigned int fvco)
{
	if (fll->fvco && fvco != fll->fvco) {
		arizona_fll_err(fll,
				"Can't change output on active FLL\n");
		return -EINVAL;
	}

	if (fin / ARIZONA_FLL_MAX_REFDIV > ARIZONA_FLL_MAX_FREF) {
		arizona_fll_err(fll,
				"Can't scale %dMHz in to <=13.5MHz\n",
				fin);
		return -EINVAL;
	}

	return 0;
}

static int arizona_find_fratio(struct arizona_fll *fll, unsigned int fref,
	unsigned int fvco, int *fratio, bool sync)
{
	int i, ratio;

	switch (fll->arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		break;
	case CS47L35:
		/* rev A0 is like Clearwater, so break */
		if (fll->arizona->rev == 0)
			break;
		/* rev A1 works similar to Moon, so fall through to default */
	default:
		if (!sync) {
			ratio = 1;
			while ((fvco / (ratio * fref)) > ARIZONA_FLL_MAX_N)
				ratio++;
			*fratio = ratio - 1;
			return ratio;
		}
		break;
	}

	/* Find an appropriate FLL_FRATIO */
	for (i = 0; i < ARRAY_SIZE(fll_fratios); i++) {
		if (fll_fratios[i].min <= fref && fref <= fll_fratios[i].max) {
			if (fratio)
				*fratio = fll_fratios[i].fratio;
			return fll_fratios[i].ratio;
		}
	}

	return -EINVAL;
}

static int arizona_calc_fratio(struct arizona_fll *fll,
			       struct arizona_fll_cfg *cfg,
			       unsigned int fvco,
			       unsigned int fin, bool sync)
{
	int init_ratio, ratio;
	int refdiv, div;
	unsigned int fref = fin;

	/* Fref must be <=13.5MHz, find initial refdiv */
	div = 1;
	cfg->refdiv = 0;
	while (fref > ARIZONA_FLL_MAX_FREF) {
		div *= 2;
		fref /= 2;
		cfg->refdiv++;

		if (div > ARIZONA_FLL_MAX_REFDIV)
			return -EINVAL;
	}

	/* Find an appropriate FLL_FRATIO */
	init_ratio = arizona_find_fratio(fll, fref, fvco, &cfg->fratio, sync);
	if (init_ratio < 0) {
		arizona_fll_err(fll, "Unable to find FRATIO for fref=%uHz\n",
				fref);
		return init_ratio;
	}

	switch (fll->arizona->type) {
	case WM5102:
	case WM8997:
		return init_ratio;
	case WM8280:
	case WM5110:
		if (fll->arizona->rev < 3 || sync)
			return init_ratio;
		break;
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		if (fref == 11289600 && fvco == 90316800) {
			if (!sync)
				cfg->fratio = init_ratio - 1;
			return init_ratio;
		}

		if (sync)
			return init_ratio;
		break;
	case CS47L35:
		if (fll->arizona->rev == 0) {
			if (fref == 11289600 && fvco == 90316800) {
				if (!sync)
					cfg->fratio = init_ratio - 1;
				return init_ratio;
			}

			if (sync)
				return init_ratio;
			break;
		}
		return init_ratio;
	default:
		return init_ratio;
	}

	cfg->fratio = init_ratio - 1;

	/* Adjust FRATIO/refdiv to avoid integer mode if possible */
	refdiv = cfg->refdiv;

	while (div <= ARIZONA_FLL_MAX_REFDIV) {
		for (ratio = init_ratio; ratio <= ARIZONA_FLL_MAX_FRATIO;
		     ratio++) {
			if ((ARIZONA_FLL_VCO_CORNER / 2) /
			    (fll->vco_mult * ratio) < fref)
				break;

			if (fvco % (ratio * fref)) {
				cfg->refdiv = refdiv;
				cfg->fratio = ratio - 1;
				return ratio;
			}
		}

		for (ratio = init_ratio - 1; ratio > 0; ratio--) {
			if (fvco % (ratio * fref)) {
				cfg->refdiv = refdiv;
				cfg->fratio = ratio - 1;
				return ratio;
			}
		}

		div *= 2;
		fref /= 2;
		refdiv++;
		init_ratio = arizona_find_fratio(fll, fref, fvco, NULL, sync);
	}

	arizona_fll_warn(fll, "Falling back to integer mode operation\n");
	return cfg->fratio + 1;
}

static int arizona_calc_fll(struct arizona_fll *fll,
			    struct arizona_fll_cfg *cfg,
			    unsigned int fin, bool sync)
{
	unsigned int fvco, gcd_fll;
	int i, ratio;
	unsigned int fref;
	struct arizona_fll_gain *fll_gain;
	unsigned int size_fll_gain;

	arizona_fll_dbg(fll, "fin=%u fout=%u\n", fin, fll->fout);

	cfg->outdiv = fll->outdiv;

	if (cfg->fin == fin && cfg->fvco == fll->fvco) {
		/* use the pre-computed fll configuration */
		return 0;
	}

	fvco = fll->fvco;

	arizona_fll_dbg(fll, "fvco=%dHz\n", fvco);

	/* Find an appropriate FLL_FRATIO and refdiv */
	ratio = arizona_calc_fratio(fll, cfg, fvco, fin, sync);
	if (ratio < 0)
		return ratio;

	/* Apply the division for our remaining calculations */
	fref = fin / (1 << cfg->refdiv);

	cfg->n = fvco / (ratio * fref);

	if (fvco % (ratio * fref)) {
		gcd_fll = gcd(fvco, ratio * fref);
		arizona_fll_dbg(fll, "GCD=%u\n", gcd_fll);

		cfg->theta = (fvco - (cfg->n * ratio * fref))
			/ gcd_fll;
		cfg->lambda = (ratio * fref) / gcd_fll;
	} else {
		cfg->theta = 0;
		cfg->lambda = 0;
	}

	/* Round down to 16bit range with cost of accuracy lost.
	 * Denominator must be bigger than numerator so we only
	 * take care of it.
	 */
	while (cfg->lambda >= (1 << 16)) {
		cfg->theta >>= 1;
		cfg->lambda >>= 1;
	}

	fll_gain = fll_gains;
	size_fll_gain = ARRAY_SIZE(fll_gains);

	switch (fll->arizona->type) {
	default:
		if (!sync) {
			cfg->intg_gain = fref > 768000 ? 3 : 2;
			fll_gain = fll_moon_gains;
			size_fll_gain = ARRAY_SIZE(fll_moon_gains);
		}
		/* fall-through */
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		for (i = 0; i < size_fll_gain; i++) {
			if (fll_gain[i].min <= fref &&
			    fref <= fll_gain[i].max) {
				cfg->gain = fll_gain[i].gain;
				break;
			}
		}
		if (i == size_fll_gain) {
			arizona_fll_err(fll, "Unable to find gain for fref=%uHz\n",
					fref);
			return -EINVAL;
		}
		break;
	}

	cfg->fin = fin;
	cfg->fvco = fll->fvco;

	arizona_fll_dbg(fll, "N=%x THETA=%x LAMBDA=%x\n",
			cfg->n, cfg->theta, cfg->lambda);
	arizona_fll_dbg(fll, "FRATIO=%x(%d) OUTDIV=%x REFCLK_DIV=%x\n",
			cfg->fratio, cfg->fratio, cfg->outdiv, cfg->refdiv);
	arizona_fll_dbg(fll, "GAIN=%d\n", cfg->gain);

	return 0;

}

static bool arizona_apply_fll(struct arizona *arizona, unsigned int base,
			      struct arizona_fll_cfg *cfg, int source,
			      int gain, bool sync)
{
	bool change, fll_change;

	fll_change = false;
	regmap_update_bits_check_async(arizona->regmap, base + 3,
			   ARIZONA_FLL1_THETA_MASK, cfg->theta, &change);
	fll_change |= change;
	regmap_update_bits_check_async(arizona->regmap, base + 4,
			   ARIZONA_FLL1_LAMBDA_MASK, cfg->lambda, &change);
	fll_change |= change;
	regmap_update_bits_check_async(arizona->regmap, base + 5,
			   ARIZONA_FLL1_FRATIO_MASK,
			   cfg->fratio << ARIZONA_FLL1_FRATIO_SHIFT, &change);
	fll_change |= change;
	regmap_update_bits_check_async(arizona->regmap, base + 6,
			   ARIZONA_FLL1_CLK_REF_DIV_MASK |
			   ARIZONA_FLL1_CLK_REF_SRC_MASK,
			   cfg->refdiv << ARIZONA_FLL1_CLK_REF_DIV_SHIFT |
			   source << ARIZONA_FLL1_CLK_REF_SRC_SHIFT, &change);
	fll_change |= change;

	if (sync) {
		regmap_update_bits_check_async(arizona->regmap, base + 0x7,
				   ARIZONA_FLL1_GAIN_MASK,
				   gain << ARIZONA_FLL1_GAIN_SHIFT, &change);
		fll_change |= change;
	} else {
		regmap_update_bits_async(arizona->regmap, base + 0x5,
				   ARIZONA_FLL1_OUTDIV_MASK,
				   cfg->outdiv << ARIZONA_FLL1_OUTDIV_SHIFT);

		regmap_update_bits_check_async(arizona->regmap, base + 0x9,
				   ARIZONA_FLL1_GAIN_MASK,
				   gain << ARIZONA_FLL1_GAIN_SHIFT, &change);
		fll_change |= change;
	}

	regmap_update_bits_check_async(arizona->regmap, base + 2,
			   ARIZONA_FLL1_CTRL_UPD | ARIZONA_FLL1_N_MASK,
			   ARIZONA_FLL1_CTRL_UPD | cfg->n, &change);
	fll_change |= change;

	return fll_change;
}

static int arizona_is_enabled_fll(struct arizona_fll *fll)
{
	struct arizona *arizona = fll->arizona;
	unsigned int reg;
	int ret;

	ret = regmap_read(arizona->regmap, fll->base + 1, &reg);
	if (ret != 0) {
		arizona_fll_err(fll, "Failed to read current state: %d\n",
				ret);
		return ret;
	}

	return reg & ARIZONA_FLL1_ENA;
}

static int arizona_wait_for_fll(struct arizona_fll *fll, bool requested)
{
	struct arizona *arizona = fll->arizona;
	unsigned int reg, mask;
	unsigned int val = 0;
	bool status;
	int i;

	arizona_fll_dbg(fll, "Waiting for FLL...\n");

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM1831:
	case CS47L24:
		reg = ARIZONA_INTERRUPT_RAW_STATUS_5;
		mask = ARIZONA_FLL1_CLOCK_OK_STS;
		break;
	default:
		reg = CLEARWATER_IRQ1_RAW_STATUS_2;
		mask = CLEARWATER_FLL1_LOCK_STS1;
		break;
	}

	for (i = 0; i < 25; i++) {
		regmap_read(arizona->regmap, reg, &val);
		status = val & (mask << (fll->id - 1));
		if (status == requested)
			return 0;
		usleep_range(10000, 10001);
	}

	arizona_fll_warn(fll, "Timed out waiting for lock\n");

	return -ETIMEDOUT;
}

static int arizona_enable_fll(struct arizona_fll *fll)
{
	struct arizona *arizona = fll->arizona;
	bool use_sync = false;
	int already_enabled = arizona_is_enabled_fll(fll);
	struct arizona_fll_cfg *ref_cfg = &(fll->ref_cfg);
	struct arizona_fll_cfg *sync_cfg = &(fll->sync_cfg);
	bool fll_change, change;
	unsigned int fsync_freq;
	int gain;

	if (already_enabled < 0)
		return already_enabled;

	arizona_fll_dbg(fll, "Enabling FLL, initially %s\n",
			already_enabled ? "enabled" : "disabled");

	if (already_enabled) {
		/* Facilitate smooth refclk across the transition */
		regmap_update_bits_async(fll->arizona->regmap, fll->base + 0x9,
					 ARIZONA_FLL1_GAIN_MASK, 0);
		regmap_update_bits(fll->arizona->regmap, fll->base + 1,
				   ARIZONA_FLL1_FREERUN, ARIZONA_FLL1_FREERUN);
		udelay(32);
	}

	/*
	 * If we have both REFCLK and SYNCCLK then enable both,
	 * otherwise apply the SYNCCLK settings to REFCLK.
	 */
	if (fll->ref_src >= 0 && fll->ref_freq &&
	    fll->ref_src != fll->sync_src) {
		arizona_calc_fll(fll, ref_cfg, fll->ref_freq, false);

		fll_change = arizona_apply_fll(arizona, fll->base,
				ref_cfg, fll->ref_src, ref_cfg->gain, false);
		if (fll->sync_src >= 0) {
			arizona_calc_fll(fll, sync_cfg, fll->sync_freq, true);
			fsync_freq = fll->sync_freq / (1 << sync_cfg->refdiv);
			fll_change |= arizona_apply_fll(arizona,
					fll->base + fll->sync_offset,
					sync_cfg, fll->sync_src,
					sync_cfg->gain, true);
			use_sync = true;
		}
	} else if (fll->sync_src >= 0) {
		arizona_calc_fll(fll, ref_cfg, fll->sync_freq, false);

		gain = ref_cfg->gain;

		switch (fll->arizona->type) {
		case WM5102:
		case WM5110:
		case WM8997:
		case WM8280:
		case WM8998:
		case WM1814:
		case WM8285:
		case WM1840:
		case WM1831:
		case CS47L24:
		case CS47L35:
			break;
		default:
			if (ref_cfg->theta == 0)
				gain = ref_cfg->intg_gain;
			break;
		}

		fll_change = arizona_apply_fll(arizona, fll->base, ref_cfg,
				fll->sync_src, gain, false);

		regmap_update_bits_async(arizona->regmap,
			fll->base + fll->sync_offset + 0x1,
			ARIZONA_FLL1_SYNC_ENA, 0);
	} else {
		arizona_fll_err(fll, "No clocks provided\n");
		return -EINVAL;
	}

	/*
	 * Increase the bandwidth if we're not using a low frequency
	 * sync source.
	 */
	if (use_sync && fsync_freq > 128000)
		regmap_update_bits_async(arizona->regmap,
			fll->base + fll->sync_offset + 0x7,
			ARIZONA_FLL1_SYNC_BW, 0);
	else
		regmap_update_bits_async(arizona->regmap,
			fll->base + fll->sync_offset + 0x7,
			ARIZONA_FLL1_SYNC_BW,
			ARIZONA_FLL1_SYNC_BW);

	switch (fll->arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		break;
	case CS47L35:
		if (fll->arizona->rev == 0)
			break;
		/* for rev A1 fall through */
	default:
		if ((!use_sync) && (ref_cfg->theta == 0))
			regmap_update_bits_check(arizona->regmap,
				fll->base + 0xA,
				ARIZONA_FLL1_PHASE_ENA_MASK |
				ARIZONA_FLL1_PHASE_GAIN_MASK,
				(1 << ARIZONA_FLL1_PHASE_ENA_SHIFT) |
				(2 << ARIZONA_FLL1_PHASE_GAIN_SHIFT),
				&change);
		else
			regmap_update_bits_check(arizona->regmap,
				fll->base + 0xA,
				ARIZONA_FLL1_PHASE_ENA_MASK |
				ARIZONA_FLL1_PHASE_GAIN_MASK,
				2 << ARIZONA_FLL1_PHASE_GAIN_SHIFT,
				&change);
		fll_change |= change;
		break;
	}

	if (!already_enabled)
		pm_runtime_get(arizona->dev);

	regmap_update_bits_async(arizona->regmap, fll->base + 1,
				 ARIZONA_FLL1_ENA, ARIZONA_FLL1_ENA);
	if (use_sync)
		regmap_update_bits_async(arizona->regmap,
			fll->base + fll->sync_offset + 0x1,
			ARIZONA_FLL1_SYNC_ENA,
			ARIZONA_FLL1_SYNC_ENA);

	if (already_enabled)
		regmap_update_bits_async(arizona->regmap, fll->base + 1,
					 ARIZONA_FLL1_FREERUN, 0);

	if (fll_change || !already_enabled)
		arizona_wait_for_fll(fll, true);

	return 0;
}

static void arizona_disable_fll(struct arizona_fll *fll)
{
	struct arizona *arizona = fll->arizona;
	bool change;

	arizona_fll_dbg(fll, "Disabling FLL\n");

	regmap_update_bits_async(arizona->regmap, fll->base + 1,
				 ARIZONA_FLL1_FREERUN, ARIZONA_FLL1_FREERUN);
	regmap_update_bits_check(arizona->regmap, fll->base + 1,
				 ARIZONA_FLL1_ENA, 0, &change);
	regmap_update_bits(arizona->regmap,
		fll->base + fll->sync_offset + 0x1,
		ARIZONA_FLL1_SYNC_ENA, 0);
	regmap_update_bits_async(arizona->regmap, fll->base + 1,
				 ARIZONA_FLL1_FREERUN, 0);

	arizona_wait_for_fll(fll, false);

	if (change)
		pm_runtime_put_autosuspend(arizona->dev);
}

int arizona_set_fll_refclk(struct arizona_fll *fll, int source,
			   unsigned int fin, unsigned int fout)
{
	int ret = 0;

	if (fll->ref_src == source && fll->ref_freq == fin)
		return 0;

	if (fll->fout && fin > 0) {
		ret = arizona_validate_fll(fll, fin, fll->fvco);
		if (ret != 0)
			return ret;
	}

	fll->ref_src = source;
	fll->ref_freq = fin;

	if (fll->fout && fin > 0) {
		ret = arizona_enable_fll(fll);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_set_fll_refclk);

int arizona_get_fll(struct arizona_fll *fll, int *source,
		    unsigned int *Fref, unsigned int *Fout)
{
	if (!fll || !source || !Fref || !Fout)
		return -EINVAL;

	mutex_lock(&fll->lock);
	*source = fll->sync_src;
	*Fref = fll->sync_freq;
	*Fout = fll->fout;
	mutex_unlock(&fll->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(arizona_get_fll);

int arizona_set_fll(struct arizona_fll *fll, int source,
		    unsigned int fin, unsigned int fout)
{
	unsigned int fvco = 0;
	int div = 0;
	int ret = 0;

	if (fll->sync_src == source &&
	    fll->sync_freq == fin && fll->fout == fout)
		return 0;

	if (fout) {
		div = fll->min_outdiv;
		while (fout * div < ARIZONA_FLL_MIN_FVCO * fll->vco_mult) {
			div++;
			if (div > fll->max_outdiv) {
				arizona_fll_err(fll,
						"No FLL_OUTDIV for Fout=%uHz\n",
						fout);
				return -EINVAL;
			}
		}
		fvco = fout * div / fll->vco_mult;

		if (fll->ref_src >= 0) {
			ret = arizona_validate_fll(fll, fll->ref_freq, fvco);
			if (ret != 0)
				return ret;
		}

		ret = arizona_validate_fll(fll, fin, fvco);
		if (ret != 0)
			return ret;
	}

	fll->sync_src = source;
	fll->sync_freq = fin;
	fll->fvco = fvco;
	fll->outdiv = div;
	fll->fout = fout;

	if (fout)
		ret = arizona_enable_fll(fll);
	else
		arizona_disable_fll(fll);

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_set_fll);

static int arizona_enable_fll_ao(struct arizona_fll *fll,
	struct reg_default *patch, unsigned int patch_size)
{
	struct arizona *arizona = fll->arizona;
	int already_enabled = arizona_is_enabled_fll(fll);
	unsigned int i;

	if (already_enabled < 0)
		return already_enabled;

	arizona_fll_dbg(fll, "Enabling FLL, initially %s\n",
			already_enabled ? "enabled" : "disabled");

	/* FLL_AO_HOLD must be set before configuring any registers */
	regmap_update_bits(fll->arizona->regmap, fll->base + 1,
		MOON_FLL_AO_HOLD, MOON_FLL_AO_HOLD);

	/* the default patch is for mclk2 as source,
	modify the patch to apply fll->ref_src */
	for (i = 0; i < patch_size; i++) {
		if (patch[i].reg == MOON_FLLAO_CONTROL_6) {
			patch[i].def &= ~MOON_FLL_AO_REFCLK_SRC_MASK;
			patch[i].def |=
				(fll->ref_src << MOON_FLL_AO_REFCLK_SRC_SHIFT)
				& MOON_FLL_AO_REFCLK_SRC_MASK;
		}
	}

	regmap_multi_reg_write(arizona->regmap, patch,
		patch_size);

	if (!already_enabled)
		pm_runtime_get(arizona->dev);

	regmap_update_bits(arizona->regmap, fll->base + 1,
			   MOON_FLL_AO_ENA, MOON_FLL_AO_ENA);

	/* Release the hold so that fll_ao locks to external frequency */
	regmap_update_bits(arizona->regmap, fll->base + 1,
		MOON_FLL_AO_HOLD, 0);

	if (!already_enabled)
		arizona_wait_for_fll(fll, true);

	return 0;
}

static int arizona_disable_fll_ao(struct arizona_fll *fll)
{
	struct arizona *arizona = fll->arizona;
	bool change;

	arizona_fll_dbg(fll, "Disabling FLL\n");

	regmap_update_bits(arizona->regmap, fll->base + 1,
			   MOON_FLL_AO_HOLD, MOON_FLL_AO_HOLD);
	regmap_update_bits_check(arizona->regmap, fll->base + 1,
			   MOON_FLL_AO_ENA, 0, &change);

	arizona_wait_for_fll(fll, false);

	/* ctrl_up gates the writes to all fll_ao register,
	setting it to 0 here ensures that after a runtime
	suspend/resume cycle when one enables the
	fllao then ctrl_up is the last bit that is configured
	by the fllao enable code rather than the cache
	sync operation which would have updated it
	much earlier before writing out all fllao registers */
	regmap_update_bits(arizona->regmap, fll->base + 2,
			   MOON_FLL_AO_CTRL_UPD_MASK, 0);

	if (change)
		pm_runtime_put_autosuspend(arizona->dev);

	return 0;
}

int arizona_set_fll_ao(struct arizona_fll *fll, int source,
		    unsigned int fin, unsigned int fout)
{
	int ret = 0;
	struct arizona_fll_cfg *cfg = &(fll->ref_cfg);
	unsigned int i;

	if (fll->ref_src == source &&
	    fll->ref_freq == fin && fll->fout == fout)
		return 0;

	if ((fout) && (cfg->fin != fin ||
	     cfg->fvco != fout)) {
		for (i = 0; i < ARRAY_SIZE(fllao_settings); i++) {
			if (fllao_settings[i].fin == fin &&
				fllao_settings[i].fout == fout)
				break;
		}

		if (i == ARRAY_SIZE(fllao_settings)) {
			arizona_fll_err(fll,
				"No matching configuration for FLL_AO\n");
			return -EINVAL;
		}

		cfg->patch = fllao_settings[i].patch;
		cfg->patch_size = fllao_settings[i].patch_size;
		cfg->fin = fin;
		cfg->fvco = fout;
	}

	fll->ref_src = source;
	fll->ref_freq = fin;
	fll->fout = fout;

	if (fout)
		ret = arizona_enable_fll_ao(fll, cfg->patch, cfg->patch_size);
	else
		arizona_disable_fll_ao(fll);

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_set_fll_ao);

int arizona_init_fll(struct arizona *arizona, int id, int base, int lock_irq,
		     int ok_irq, struct arizona_fll *fll)
{
	unsigned int val;

	fll->id = id;
	fll->base = base;
	fll->arizona = arizona;
	fll->sync_src = ARIZONA_FLL_SRC_NONE;

	if (!fll->min_outdiv)
		fll->min_outdiv = ARIZONA_FLL_MIN_OUTDIV;
	if (!fll->max_outdiv)
		fll->max_outdiv = ARIZONA_FLL_MAX_OUTDIV;
	if (!fll->sync_offset)
		fll->sync_offset = ARIZONA_FLL_SYNC_OFFSET;

	/* Configure default refclk to 32kHz if we have one */
	regmap_read(arizona->regmap, ARIZONA_CLOCK_32K_1, &val);
	switch (val & ARIZONA_CLK_32K_SRC_MASK) {
	case ARIZONA_CLK_SRC_MCLK1:
	case ARIZONA_CLK_SRC_MCLK2:
		fll->ref_src = val & ARIZONA_CLK_32K_SRC_MASK;
		break;
	default:
		fll->ref_src = ARIZONA_FLL_SRC_NONE;
	}
	fll->ref_freq = 32768;

	regmap_update_bits(arizona->regmap, fll->base + 1,
			   ARIZONA_FLL1_FREERUN, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_init_fll);

/**
 * arizona_set_output_mode - Set the mode of the specified output
 *
 * @codec: Device to configure
 * @output: Output number
 * @diff: True to set the output to differential mode
 *
 * Some systems use external analogue switches to connect more
 * analogue devices to the CODEC than are supported by the device.  In
 * some systems this requires changing the switched output from single
 * ended to differential mode dynamically at runtime, an operation
 * supported using this function.
 *
 * Most systems have a single static configuration and should use
 * platform data instead.
 */
int arizona_set_output_mode(struct snd_soc_codec *codec, int output, bool diff)
{
	unsigned int reg, val;

	if (output < 1 || output > 6)
		return -EINVAL;

	reg = ARIZONA_OUTPUT_PATH_CONFIG_1L + (output - 1) * 8;

	if (diff)
		val = ARIZONA_OUT1_MONO;
	else
		val = 0;

	return snd_soc_update_bits(codec, reg, ARIZONA_OUT1_MONO, val);
}
EXPORT_SYMBOL_GPL(arizona_set_output_mode);

int arizona_set_hpdet_cb(struct snd_soc_codec *codec,
			 void (*hpdet_cb)(unsigned int measurement))
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.hpdet_cb = hpdet_cb;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_hpdet_cb);

int arizona_set_micd_cb(struct snd_soc_codec *codec,
			 void (*micd_cb)(bool mic))
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.micd_cb = micd_cb;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_micd_cb);

int arizona_set_ez2ctrl_cb(struct snd_soc_codec *codec,
			   void (*ez2ctrl_trigger)(void))
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.ez2ctrl_trigger = ez2ctrl_trigger;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_ez2ctrl_cb);

int arizona_set_ez2panic_cb(struct snd_soc_codec *codec,
			    void (*ez2panic_trigger)(int dsp, u16 *msg))
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.ez2panic_trigger = ez2panic_trigger;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_ez2panic_cb);

int arizona_set_ez2text_cb(struct snd_soc_codec *codec,
			   void (*ez2text_trigger)(int dsp))
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.ez2text_trigger = ez2text_trigger;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_ez2text_cb);

int arizona_set_custom_jd(struct snd_soc_codec *codec,
			   const struct arizona_jd_state *custom_jd)
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	arizona->pdata.custom_jd = custom_jd;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_set_custom_jd);

struct regmap *arizona_get_regmap_dsp(struct snd_soc_codec *codec)
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM1831:
	case CS47L24:
		return arizona->regmap;
	default:
		return arizona->regmap_32bit;
	}
}
EXPORT_SYMBOL_GPL(arizona_get_regmap_dsp);

struct arizona_extcon_info *
arizona_get_extcon_info(struct snd_soc_codec *codec)
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);

	return arizona->extcon_info;
}
EXPORT_SYMBOL_GPL(arizona_get_extcon_info);

static int arizona_set_force_bypass(struct snd_soc_codec *codec,
	bool set_bypass)
{
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct arizona_micbias *micbias = arizona->pdata.micbias;
	unsigned int i, cp_bypass = 0, micbias_bypass = 0;
	unsigned int num_micbiases;

	if (set_bypass) {
		cp_bypass = ARIZONA_CPMIC_BYPASS;
		micbias_bypass = ARIZONA_MICB1_BYPASS;
	}

	if (arizona->micvdd_regulated) {
		if (set_bypass)
			snd_soc_dapm_disable_pin(arizona->dapm,
				"MICSUPP");
		else
			snd_soc_dapm_force_enable_pin(arizona->dapm,
				"MICSUPP");

		snd_soc_dapm_sync(arizona->dapm);

		regmap_update_bits(arizona->regmap,
			ARIZONA_MIC_CHARGE_PUMP_1,
			ARIZONA_CPMIC_BYPASS, cp_bypass);
	}

	arizona_get_num_micbias(arizona, &num_micbiases, NULL);

	for (i = 0; i < num_micbiases; i++) {
		if ((set_bypass) ||
			(!micbias[i].bypass && micbias[i].mV))
			regmap_update_bits(arizona->regmap,
				ARIZONA_MIC_BIAS_CTRL_1 + i,
				ARIZONA_MICB1_BYPASS,
				micbias_bypass);
	}

	return 0;
}

int arizona_enable_force_bypass(struct snd_soc_codec *codec)
{
	return arizona_set_force_bypass(codec, true);
}
EXPORT_SYMBOL_GPL(arizona_enable_force_bypass);

int arizona_disable_force_bypass(struct snd_soc_codec *codec)
{
	return arizona_set_force_bypass(codec, false);
}
EXPORT_SYMBOL_GPL(arizona_disable_force_bypass);

static bool arizona_eq_filter_unstable(bool mode, __be16 _a, __be16 _b)
{
	s16 a = be16_to_cpu(_a);
	s16 b = be16_to_cpu(_b);

	if (!mode) {
		return abs(a) >= 4096;
	} else {
		if (abs(b) >= 4096)
			return true;

		return (abs((a << 16) / (4096 - b)) >= 4096 << 4);
	}
}

int arizona_eq_coeff_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	struct soc_bytes *params = (void *)kcontrol->private_value;
	unsigned int val;
	__be16 *data;
	int len;
	int ret;

	len = params->num_regs * regmap_get_val_bytes(arizona->regmap);

	data = kmemdup(ucontrol->value.bytes.data, len, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	data[0] &= cpu_to_be16(ARIZONA_EQ1_B1_MODE);

	if (arizona_eq_filter_unstable(!!data[0], data[1], data[2]) ||
	    arizona_eq_filter_unstable(true, data[4], data[5]) ||
	    arizona_eq_filter_unstable(true, data[8], data[9]) ||
	    arizona_eq_filter_unstable(true, data[12], data[13]) ||
	    arizona_eq_filter_unstable(false, data[16], data[17])) {
		dev_err(arizona->dev, "Rejecting unstable EQ coefficients\n");
		ret = -EINVAL;
		goto out;
	}

	ret = regmap_read(arizona->regmap, params->base, &val);
	if (ret != 0)
		goto out;

	val &= ~ARIZONA_EQ1_B1_MODE;
	data[0] |= cpu_to_be16(val);

	ret = regmap_raw_write(arizona->regmap, params->base, data, len);

out:
	kfree(data);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_eq_coeff_put);

int arizona_lhpf_coeff_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	__be16 *data = (__be16 *)ucontrol->value.bytes.data;
	s16 val = be16_to_cpu(*data);

	if (abs(val) >= 4096) {
		dev_err(arizona->dev, "Rejecting unstable LHPF coefficients\n");
		return -EINVAL;
	}

	return snd_soc_bytes_put(kcontrol, ucontrol);
}
EXPORT_SYMBOL_GPL(arizona_lhpf_coeff_put);

static int arizona_slim_audio_probe(struct slim_device *slim)
{
	dev_crit(&slim->dev, "Probed\n");

	slim_audio_dev = slim;
	mutex_init(&slim_tx_lock);
	mutex_init(&slim_rx_lock);

	return 0;
}

#ifdef CONFIG_SND_SOC_FLORIDA
static const struct slim_device_id arizona_slim_id[] = {
	{ "florida-slim-audio", 0 },
	{ },
};
#else
static const struct slim_device_id arizona_slim_id[] = {
	{ "marley-slim-audio", 0 },
	{ },
};
#endif

static struct slim_driver arizona_slim_audio = {
	.driver = {
		.name = "arizona-slim-audio",
		.owner = THIS_MODULE,
	},

	.probe = arizona_slim_audio_probe,
	.id_table = arizona_slim_id,
};

int __init arizona_asoc_init(void)
{
	return slim_driver_register(&arizona_slim_audio);
}
module_init(arizona_asoc_init);


MODULE_DESCRIPTION("ASoC Wolfson Arizona class device support");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
