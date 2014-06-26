/*
 *  merr_dpcm_wm8958.c - ASoc DPCM Machine driver for Intel Merrfield MID platform
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Vinod Koul <vinod.koul@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/platform_mrfld_audio.h>
#include <asm/intel_sst_mrfld.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input.h>
#include <asm/intel-mid.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include "../../codecs/wm8994.h"

/* Codec PLL output clk rate */
#define CODEC_SYSCLK_RATE			24576000
/* Input clock to codec at MCLK1 PIN */
#define CODEC_IN_MCLK1_RATE			19200000
/* Input clock to codec at MCLK2 PIN */
#define CODEC_IN_MCLK2_RATE			32768
/*  define to select between MCLK1 and MCLK2 input to codec as its clock */
#define CODEC_IN_MCLK1				1
#define CODEC_IN_MCLK2				2

/* Register address for OSC Clock */
#define MERR_OSC_CLKOUT_CTRL0_REG_ADDR  0xFF00BC04
/* Size of osc clock register */
#define MERR_OSC_CLKOUT_CTRL0_REG_SIZE  4

struct mrfld_8958_mc_private {
	struct snd_soc_jack jack;
	int jack_retry;
	u8 pmic_id;
	void __iomem    *osc_clk0_reg;
};


/* set_osc_clk0-	enable/disables the osc clock0
 * addr:		address of the register to write to
 * enable:		bool to enable or disable the clock
 */
static inline void set_soc_osc_clk0(void __iomem *addr, bool enable)
{
	u32 osc_clk_ctrl;

	osc_clk_ctrl = readl(addr);
	if (enable)
		osc_clk_ctrl |= BIT(31);
	else
		osc_clk_ctrl &= ~(BIT(31));

	pr_debug("%s: enable:%d val 0x%x\n", __func__, enable, osc_clk_ctrl);

	writel(osc_clk_ctrl, addr);
}

static inline struct snd_soc_codec *mrfld_8958_get_codec(struct snd_soc_card *card)
{
	bool found = false;
	struct snd_soc_codec *codec;

	list_for_each_entry(codec, &card->codec_dev_list, card_list) {
		if (!strstr(codec->name, "wm8994-codec")) {
			pr_debug("codec was %s", codec->name);
			continue;
		} else {
			found = true;
			break;
		}
	}
	if (found == false) {
		pr_warn("%s: cant find codec", __func__);
		return NULL;
	}
	return codec;
}

/* TODO: find better way of doing this */
static struct snd_soc_dai *find_codec_dai(struct snd_soc_card *card, const char *dai_name)
{
	int i;
	for (i = 0; i < card->num_rtd; i++) {
		if (!strcmp(card->rtd[i].codec_dai->name, dai_name))
			return card->rtd[i].codec_dai;
	}
	pr_err("%s: unable to find codec dai\n", __func__);
	/* this should never occur */
	WARN_ON(1);
	return NULL;
}

/* Function to switch the input clock for codec,  When audio is in
 * progress input clock to codec will be through MCLK1 which is 19.2MHz
 * while in off state input clock to codec will be through 32KHz through
 * MCLK2
 * card	: Sound card structure
 * src	: Input clock source to codec
 */
static int mrfld_8958_set_codec_clk(struct snd_soc_card *card, int src)
{
	struct snd_soc_dai *aif1_dai = find_codec_dai(card, "wm8994-aif1");
	int ret;

	if (!aif1_dai)
		return -ENODEV;

	switch (src) {
	case CODEC_IN_MCLK1:
		/* Turn ON the PLL to generate required sysclk rate
		 * from MCLK1 */
		ret = snd_soc_dai_set_pll(aif1_dai,
			WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
			CODEC_IN_MCLK1_RATE, CODEC_SYSCLK_RATE);
		if (ret < 0) {
			pr_err("Failed to start FLL: %d\n", ret);
			return ret;
		}
		/* Switch to MCLK1 input */
		ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_FLL1,
				CODEC_SYSCLK_RATE, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			pr_err("Failed to set codec sysclk configuration %d\n",
				 ret);
			return ret;
		}
		break;
	case CODEC_IN_MCLK2:
		/* Switch to MCLK2 */
		ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_MCLK2,
				32768, SND_SOC_CLOCK_IN);
		if (ret < 0) {
			pr_err("Failed to switch to MCLK2: %d", ret);
			return ret;
		}
		/* Turn off PLL for MCLK1 */
		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);
		if (ret < 0) {
			pr_err("Failed to stop the FLL: %d", ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mrfld_wm8958_set_clk_fmt(struct snd_soc_dai *codec_dai)
{
	unsigned int fmt;
	int ret = 0;
	struct snd_soc_card *card = codec_dai->card;
	struct mrfld_8958_mc_private *ctx = snd_soc_card_get_drvdata(card);

	/* Enable the osc clock at start so that it gets settling time */
	set_soc_osc_clk0(ctx->osc_clk0_reg, true);

	pr_err("setting snd_soc_dai_set_tdm_slot\n");
	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0, 0, 4, SNDRV_PCM_FORMAT_S24_LE);
	if (ret < 0) {
		pr_err("can't set codec pcm format %d\n", ret);
		return ret;
	}

	/* WM8958 slave Mode */
	fmt =   SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_IB_NF
		| SND_SOC_DAIFMT_CBS_CFS;
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	/* FIXME: move this to SYS_CLOCK event handler when codec driver
	 * dependency is clean.
	 */
	/* Switch to 19.2MHz MCLK1 input clock for codec */
	ret = mrfld_8958_set_codec_clk(card, CODEC_IN_MCLK1);

	return ret;
}

static int mrfld_8958_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	if (!strcmp(codec_dai->name, "wm8994-aif1"))
		return mrfld_wm8958_set_clk_fmt(codec_dai);
	return 0;
}

static int mrfld_wm8958_compr_set_params(struct snd_compr_stream *cstream)
{
	return 0;
}

static const struct snd_soc_pcm_stream mrfld_wm8958_dai_params = {
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static int merr_codec_fixup(struct snd_soc_pcm_runtime *rtd,
			    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("Invoked %s for dailink %s\n", __func__, rtd->dai_link->name);

	/* The DSP will covert the FE rate to 48k, stereo, 24bits */
	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	/* set SSP2 to 24-bit */
	snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
				    SNDRV_PCM_HW_PARAM_FIRST_MASK],
				    SNDRV_PCM_FORMAT_S24_LE);
	return 0;
}

static int mrfld_8958_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_dai *aif1_dai = find_codec_dai(card, "wm8994-aif1");
	int ret = 0;

	if (!aif1_dai)
		return -ENODEV;

	if (dapm->dev != aif1_dai->dev)
		return 0;
	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (card->dapm.bias_level == SND_SOC_BIAS_STANDBY)

			ret = mrfld_wm8958_set_clk_fmt(aif1_dai);
		break;
	default:
		break;
	}
	pr_debug("%s card(%s)->bias_level %u\n", __func__, card->name,
			card->dapm.bias_level);
	return ret;
}

static int mrfld_8958_set_bias_level_post(struct snd_soc_card *card,
		 struct snd_soc_dapm_context *dapm,
		 enum snd_soc_bias_level level)
{
	struct snd_soc_dai *aif1_dai = find_codec_dai(card, "wm8994-aif1");
	struct mrfld_8958_mc_private *ctx = snd_soc_card_get_drvdata(card);
	int ret = 0;

	if (!aif1_dai)
		return -ENODEV;

	if (dapm->dev != aif1_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		/* We are in stabdba down so */
		/* Switch to 32KHz MCLK2 input clock for codec
		 */
		ret = mrfld_8958_set_codec_clk(card, CODEC_IN_MCLK2);
		/* Turn off 19.2MHz soc osc clock */
		set_soc_osc_clk0(ctx->osc_clk0_reg, false);
		break;
	default:
		break;
	}
	card->dapm.bias_level = level;
	pr_debug("%s card(%s)->bias_level %u\n", __func__, card->name,
			card->dapm.bias_level);
	return ret;
}

#define PMIC_ID_ADDR		0x00
#define PMIC_CHIP_ID_A0_VAL	0xC0

static int mrfld_8958_set_vflex_vsel(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
#define VFLEXCNT		0xAB
#define VFLEXVSEL_5V		0x01
#define VFLEXVSEL_B0_VSYS_PT	0x80	/* B0: Vsys pass-through */
#define VFLEXVSEL_A0_4P5V	0x41	/* A0: 4.5V */

	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mrfld_8958_mc_private *ctx = snd_soc_card_get_drvdata(card);

	u8 vflexvsel, pmic_id = ctx->pmic_id;
	int retval = 0;

	pr_debug("%s: ON? %d\n", __func__, SND_SOC_DAPM_EVENT_ON(event));

	vflexvsel = (pmic_id == PMIC_CHIP_ID_A0_VAL) ? VFLEXVSEL_A0_4P5V : VFLEXVSEL_B0_VSYS_PT;
	pr_debug("pmic_id %#x vflexvsel %#x\n", pmic_id,
		SND_SOC_DAPM_EVENT_ON(event) ? VFLEXVSEL_5V : vflexvsel);

	/*FIXME: seems to be issue with bypass mode in MOOR, for now
		force the bias off volate as VFLEXVSEL_5V */
	if ((INTEL_MID_BOARD(1, PHONE, MOFD)) ||
			(INTEL_MID_BOARD(1, TABLET, MOFD)))
		vflexvsel = VFLEXVSEL_5V;

	if (SND_SOC_DAPM_EVENT_ON(event))
		retval = intel_scu_ipc_iowrite8(VFLEXCNT, VFLEXVSEL_5V);
	else if (SND_SOC_DAPM_EVENT_OFF(event))
		retval = intel_scu_ipc_iowrite8(VFLEXCNT, vflexvsel);
	if (retval)
		pr_err("Error writing to VFLEXCNT register\n");

	return retval;
}

static const struct snd_soc_dapm_widget widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("DMIC", NULL),
	SND_SOC_DAPM_SUPPLY("VFLEXCNT", SND_SOC_NOPM, 0, 0,
			mrfld_8958_set_vflex_vsel,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route map[] = {
	{ "Headphones", NULL, "HPOUT1L" },
	{ "Headphones", NULL, "HPOUT1R" },

	/* saltbay uses 2 DMICs, other configs may use more so change below
	 * accordingly
	 */
	{ "DMIC1DAT", NULL, "DMIC" },
	{ "DMIC2DAT", NULL, "DMIC" },
	/*{ "DMIC3DAT", NULL, "DMIC" },*/
	/*{ "DMIC4DAT", NULL, "DMIC" },*/

	/* MICBIAS2 is connected as Bias for AMIC so we link it
	 * here. Also AMIC wires up to IN1LP pin.
	 * DMIC is externally connected to 1.8V rail, so no link rqd.
	 */
	{ "AMIC", NULL, "MICBIAS2" },
	{ "IN1LP", NULL, "AMIC" },

	/* SWM map link the SWM outs to codec AIF */
	{ "AIF1 Playback", NULL, "ssp2 Tx"},
	{ "ssp2 Tx", NULL, "codec_out0"},
	{ "ssp2 Tx", NULL, "codec_out1"},
	{ "codec_in0", NULL, "ssp2 Rx" },
	{ "codec_in1", NULL, "ssp2 Rx" },
	{ "ssp2 Rx", NULL, "AIF1 Capture"},

	{ "ssp0 Tx", NULL, "modem_out"},
	{ "modem_in", NULL, "ssp0 Rx" },

	{ "ssp1 Tx", NULL, "bt_fm_out"},
	{ "bt_fm_in", NULL, "ssp1 Rx" },

	{ "AIF1 Playback", NULL, "VFLEXCNT" },
	{ "AIF1 Capture", NULL, "VFLEXCNT" },
};

static const struct wm8958_micd_rate micdet_rates[] = {
	{ 32768,       true,  1, 4 },
	{ 32768,       false, 1, 1 },
	{ 44100 * 256, true,  7, 10 },
	{ 44100 * 256, false, 7, 10 },
};

static void wm8958_custom_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	struct wm8994 *control = dev_get_drvdata(codec->dev->parent);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates;
	int num_rates;

	idle = !wm8994->jack_mic;

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (control->pdata.micd_rates) {
		rates = control->pdata.micd_rates;
		num_rates = control->pdata.num_micd_rates;
	} else {
		rates = micdet_rates;
		num_rates = ARRAY_SIZE(micdet_rates);
	}

	best = 0;
	for (i = 0; i < num_rates; i++) {
		if (rates[i].idle != idle)
			continue;
		if (abs(rates[i].sysclk - sysclk) <
		    abs(rates[best].sysclk - sysclk))
			best = i;
		else if (rates[best].idle != idle)
			best = i;
	}

	val = rates[best].start << WM8958_MICD_BIAS_STARTTIME_SHIFT
		| rates[best].rate << WM8958_MICD_RATE_SHIFT;

	dev_dbg(codec->dev, "MICD rate %d,%d for %dHz %s\n",
		rates[best].start, rates[best].rate, sysclk,
		idle ? "idle" : "active");

	snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
			    WM8958_MICD_BIAS_STARTTIME_MASK |
			    WM8958_MICD_RATE_MASK, val);
}

static void wm8958_custom_mic_id(void *data, u16 status)
{
	struct snd_soc_codec *codec = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "wm8958 custom mic id called with status %x\n",
		status);

	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		/* If nothing present then clear our statuses */
		dev_dbg(codec->dev, "Detected open circuit\n");

		schedule_delayed_work(&wm8994->open_circuit_work,
				      msecs_to_jiffies(2500));
		return;
	}

	schedule_delayed_work(&wm8994->micd_set_custom_rate_work,
		msecs_to_jiffies(wm8994->wm8994->pdata.micb_en_delay));

	/* If the measurement is showing a high impedence we've got a
	 * microphone.
	 */
	if (status & 0x600) {
		dev_dbg(codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;
		wm8994->headphone_detected = false;

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADSET,
				    SND_JACK_HEADSET);
	}


	if (status & 0xfc) {
		dev_dbg(codec->dev, "Detected headphone\n");

		/* Partial inserts of headsets with complete insert
		 * after an indeterminate amount of time require
		 * continouous micdetect enabled (until open circuit
		 * or headset is detected)
		 * */
		wm8994->mic_detecting = true;
		wm8994->jack_mic = false;
		wm8994->headphone_detected = true;

		snd_soc_jack_report(wm8994->micdet[0].jack, SND_JACK_HEADPHONE,
				    SND_JACK_HEADSET);
	}
}

static int mrfld_8958_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	unsigned int fmt;
	struct snd_soc_codec *codec;
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_dai *aif1_dai = find_codec_dai(card, "wm8994-aif1");
	struct mrfld_8958_mc_private *ctx = snd_soc_card_get_drvdata(card);

	if (!aif1_dai)
		return -ENODEV;

	pr_debug("Entry %s\n", __func__);

	ret = snd_soc_dai_set_tdm_slot(aif1_dai, 0, 0, 4, SNDRV_PCM_FORMAT_S24_LE);
	if (ret < 0) {
		pr_err("can't set codec pcm format %d\n", ret);
		return ret;
	}

	/* WM8958 slave Mode */
	fmt =   SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_IB_NF
		| SND_SOC_DAIFMT_CBS_CFS;
	ret = snd_soc_dai_set_fmt(aif1_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}

	mrfld_8958_set_bias_level(card, &card->dapm, SND_SOC_BIAS_OFF);
	card->dapm.idle_bias_off = true;

	/* these pins are not used in SB config so mark as nc
	 *
	 * LINEOUT1, 2
	 * IN1R
	 * DMICDAT2
	 */
	snd_soc_dapm_nc_pin(&card->dapm, "DMIC2DAT");
	snd_soc_dapm_nc_pin(&card->dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(&card->dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(&card->dapm, "LINEOUT2P");
	snd_soc_dapm_nc_pin(&card->dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(&card->dapm, "IN1RN");
	snd_soc_dapm_nc_pin(&card->dapm, "IN1RP");

	/* Force enable VMID to avoid cold latency constraints */
	snd_soc_dapm_force_enable_pin(&card->dapm, "VMID");
	snd_soc_dapm_sync(&card->dapm);

	codec = mrfld_8958_get_codec(card);
	if (!codec) {
		pr_err("%s: we didnt find the codec pointer!\n", __func__);
		return 0;
	}

	ctx->jack_retry = 0;
	ret = snd_soc_jack_new(codec, "Intel MID Audio Jack",
			       SND_JACK_HEADSET | SND_JACK_HEADPHONE |
				SND_JACK_BTN_0 | SND_JACK_BTN_1,
				&ctx->jack);
	if (ret) {
		pr_err("jack creation failed\n");
		return ret;
	}

	snd_jack_set_key(ctx->jack.jack, SND_JACK_BTN_1, KEY_MEDIA);
	snd_jack_set_key(ctx->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	snd_soc_update_bits(codec, WM8958_MICBIAS2, WM8958_MICB2_LVL_MASK,
				WM8958_MICB2_LVL_2P6V << WM8958_MICB2_LVL_SHIFT);

	wm8958_mic_detect(codec, &ctx->jack, NULL, NULL,
			  wm8958_custom_mic_id, codec);

	wm8958_micd_set_custom_rate(codec, wm8958_custom_micd_set_rate, codec);

	snd_soc_update_bits(codec, WM8994_AIF1_DAC1_FILTERS_1, WM8994_AIF1DAC1_MUTE, 0);
	snd_soc_update_bits(codec, WM8994_AIF1_DAC2_FILTERS_1, WM8994_AIF1DAC2_MUTE, 0);

	/* Micbias1 is always off, so for pm optimizations make sure the micbias1
	 * discharge bit is set to floating to avoid discharge in disable state
	 */
	snd_soc_update_bits(codec, WM8958_MICBIAS1, WM8958_MICB1_DISCH, 0);

	return 0;
}

static unsigned int rates_8000_16000[] = {
	8000,
	16000,
};

static struct snd_pcm_hw_constraint_list constraints_8000_16000 = {
	.count = ARRAY_SIZE(rates_8000_16000),
	.list  = rates_8000_16000,
};
static unsigned int rates_48000[] = {
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count = ARRAY_SIZE(rates_48000),
	.list  = rates_48000,
};
static int mrfld_8958_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_48000);
}

static struct snd_soc_ops mrfld_8958_ops = {
	.startup = mrfld_8958_startup,
};
static int mrfld_8958_8k_16k_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_8000_16000);
}

static struct snd_soc_ops mrfld_8958_8k_16k_ops = {
	.startup = mrfld_8958_8k_16k_startup,
	.hw_params = mrfld_8958_hw_params,
};

static struct snd_soc_ops mrfld_8958_be_ssp2_ops = {
	.hw_params = mrfld_8958_hw_params,
};
static struct snd_soc_compr_ops mrfld_compr_ops = {
	.set_params = mrfld_wm8958_compr_set_params,
};

struct snd_soc_dai_link mrfld_8958_msic_dailink[] = {
	[MERR_DPCM_AUDIO] = {
		.name = "Merrifield Audio Port",
		.stream_name = "Saltbay Audio",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "sst-platform",
		.init = mrfld_8958_init,
		.ignore_suspend = 1,
		.dynamic = 1,
		.ops = &mrfld_8958_ops,
	},
	[MERR_DPCM_DB] = {
		.name = "Merrifield DB Audio Port",
		.stream_name = "Deep Buffer Audio",
		.cpu_dai_name = "Deepbuffer-cpu-dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "sst-platform",
		.init = mrfld_8958_init,
		.ignore_suspend = 1,
		.dynamic = 1,
		.ops = &mrfld_8958_ops,
	},
	[MERR_DPCM_LL] = {
		.name = "Merrifield LL Audio Port",
		.stream_name = "Low Latency Audio",
		.cpu_dai_name = "Lowlatency-cpu-dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.platform_name = "sst-platform",
		.init = mrfld_8958_init,
		.ignore_suspend = 1,
		.dynamic = 1,
		.ops = &mrfld_8958_ops,
	},
	[MERR_DPCM_COMPR] = {
		.name = "Merrifield Compress Port",
		.stream_name = "Saltbay Compress",
		.platform_name = "sst-platform",
		.cpu_dai_name = "Compress-cpu-dai",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.init = mrfld_8958_init,
		.compr_ops = &mrfld_compr_ops,
	},
	[MERR_DPCM_VOIP] = {
		.name = "Merrifield VOIP Port",
		.stream_name = "Saltbay Voip",
		.cpu_dai_name = "Voip-cpu-dai",
		.platform_name = "sst-platform",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.init = NULL,
		.ignore_suspend = 1,
		.ops = &mrfld_8958_8k_16k_ops,
		.dynamic = 1,
	},
	[MERR_DPCM_PROBE] = {
		.name = "Merrifield Probe Port",
		.stream_name = "Saltbay Probe",
		.cpu_dai_name = "Probe-cpu-dai",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.platform_name = "sst-platform",
		.playback_count = 8,
		.capture_count = 8,
	},
	/* CODEC<->CODEC link */
	{
		.name = "Merrifield Codec-Loop Port",
		.stream_name = "Saltbay Codec-Loop",
		.cpu_dai_name = "ssp2-port",
		.platform_name = "sst-platform",
		.codec_dai_name = "wm8994-aif1",
		.codec_name = "wm8994-codec",
		.dai_fmt = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_IB_NF
						| SND_SOC_DAIFMT_CBS_CFS,
		.params = &mrfld_wm8958_dai_params,
		.dsp_loopback = true,
	},
	{
		.name = "Merrifield Modem-Loop Port",
		.stream_name = "Saltbay Modem-Loop",
		.cpu_dai_name = "ssp0-port",
		.platform_name = "sst-platform",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.params = &mrfld_wm8958_dai_params,
		.dsp_loopback = true,
	},
	{
		.name = "Merrifield BTFM-Loop Port",
		.stream_name = "Saltbay BTFM-Loop",
		.cpu_dai_name = "ssp1-port",
		.platform_name = "sst-platform",
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.params = &mrfld_wm8958_dai_params,
		.dsp_loopback = true,
	},

	/* back ends */
	{
		.name = "SSP2-Codec",
		.be_id = 1,
		.cpu_dai_name = "ssp2-port",
		.platform_name = "sst-platform",
		.no_pcm = 1,
		.codec_dai_name = "wm8994-aif1",
		.codec_name = "wm8994-codec",
		.be_hw_params_fixup = merr_codec_fixup,
		.ignore_suspend = 1,
		.ops = &mrfld_8958_be_ssp2_ops,
	},
	{
		.name = "SSP1-BTFM",
		.be_id = 2,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
	},
	{
		.name = "SSP0-Modem",
		.be_id = 3,
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.ignore_suspend = 1,
	},
};

#ifdef CONFIG_PM_SLEEP
static int snd_mrfld_8958_prepare(struct device *dev)
{
	struct snd_soc_card *card = dev_get_drvdata(dev);
	struct snd_soc_codec *codec;
	struct snd_soc_dapm_context *dapm;

	pr_debug("In %s\n", __func__);

	codec = mrfld_8958_get_codec(card);
	if (!codec) {
		pr_err("%s: couldn't find the codec pointer!\n", __func__);
		return -EAGAIN;
	}

	pr_debug("found codec %s\n", codec->name);
	dapm = &codec->dapm;

	snd_soc_dapm_disable_pin(dapm, "VMID");
	snd_soc_dapm_sync(dapm);

	snd_soc_suspend(dev);
	return 0;
}

static void snd_mrfld_8958_complete(struct device *dev)
{
	struct snd_soc_card *card = dev_get_drvdata(dev);
	struct snd_soc_codec *codec;
	struct snd_soc_dapm_context *dapm;

	pr_debug("In %s\n", __func__);

	codec = mrfld_8958_get_codec(card);
	if (!codec) {
		pr_err("%s: couldn't find the codec pointer!\n", __func__);
		return;
	}

	pr_debug("found codec %s\n", codec->name);
	dapm = &codec->dapm;

	snd_soc_dapm_force_enable_pin(dapm, "VMID");
	snd_soc_dapm_sync(dapm);

	snd_soc_resume(dev);
	return;
}

static int snd_mrfld_8958_poweroff(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_poweroff(dev);
	return 0;
}
#else
#define snd_mrfld_8958_prepare NULL
#define snd_mrfld_8958_complete NULL
#define snd_mrfld_8958_poweroff NULL
#endif

/* SoC card */
static struct snd_soc_card snd_soc_card_mrfld = {
	.name = "wm8958-audio",
	.dai_link = mrfld_8958_msic_dailink,
	.num_links = ARRAY_SIZE(mrfld_8958_msic_dailink),
	.set_bias_level = mrfld_8958_set_bias_level,
	.set_bias_level_post = mrfld_8958_set_bias_level_post,
	.dapm_widgets = widgets,
	.num_dapm_widgets = ARRAY_SIZE(widgets),
	.dapm_routes = map,
	.num_dapm_routes = ARRAY_SIZE(map),
};

static int snd_mrfld_8958_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct mrfld_8958_mc_private *drv;

	pr_debug("Entry %s\n", __func__);

	drv = kzalloc(sizeof(*drv), GFP_ATOMIC);
	if (!drv) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}

	/* ioremap the register */
	drv->osc_clk0_reg = devm_ioremap_nocache(&pdev->dev,
					MERR_OSC_CLKOUT_CTRL0_REG_ADDR,
					MERR_OSC_CLKOUT_CTRL0_REG_SIZE);
	if (!drv->osc_clk0_reg) {
		pr_err("osc clk0 ctrl ioremap failed\n");
		ret_val = -1;
		goto unalloc;
	}

	ret_val = intel_scu_ipc_ioread8(PMIC_ID_ADDR, &drv->pmic_id);
	if (ret_val) {
		pr_err("Error reading PMIC ID register\n");
		goto unalloc;
	}

	/* register the soc card */
	snd_soc_card_mrfld.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_mrfld, drv);
	ret_val = snd_soc_register_card(&snd_soc_card_mrfld);
	if (ret_val) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		goto unalloc;
	}
	platform_set_drvdata(pdev, &snd_soc_card_mrfld);
	pr_info("%s successful\n", __func__);
	return ret_val;

unalloc:
	kfree(drv);
	return ret_val;
}

static int snd_mrfld_8958_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct mrfld_8958_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("In %s\n", __func__);
	kfree(drv);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

const struct dev_pm_ops snd_mrfld_8958_mc_pm_ops = {
	.prepare = snd_mrfld_8958_prepare,
	.complete = snd_mrfld_8958_complete,
	.poweroff = snd_mrfld_8958_poweroff,
};

static struct platform_driver snd_mrfld_8958_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mrfld_wm8958",
		.pm = &snd_mrfld_8958_mc_pm_ops,
	},
	.probe = snd_mrfld_8958_mc_probe,
	.remove = snd_mrfld_8958_mc_remove,
};

static int snd_mrfld_8958_driver_init(void)
{
	pr_info("Merrifield Machine Driver mrfld_wm8958 registerd\n");
	return platform_driver_register(&snd_mrfld_8958_mc_driver);
}

static void snd_mrfld_8958_driver_exit(void)
{
	pr_debug("In %s\n", __func__);
	platform_driver_unregister(&snd_mrfld_8958_mc_driver);
}

static int snd_mrfld_8958_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed snd_mrfld wm8958 rpmsg device\n");

	ret = snd_mrfld_8958_driver_init();

out:
	return ret;
}

static void snd_mrfld_8958_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	snd_mrfld_8958_driver_exit();
	dev_info(&rpdev->dev, "Removed snd_mrfld wm8958 rpmsg device\n");
}

static void snd_mrfld_8958_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
				int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
			data, len,  true);
}

static struct rpmsg_device_id snd_mrfld_8958_rpmsg_id_table[] = {
	{ .name = "rpmsg_mrfld_wm8958_audio" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, snd_mrfld_8958_rpmsg_id_table);

static struct rpmsg_driver snd_mrfld_8958_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= snd_mrfld_8958_rpmsg_id_table,
	.probe		= snd_mrfld_8958_rpmsg_probe,
	.callback	= snd_mrfld_8958_rpmsg_cb,
	.remove		= snd_mrfld_8958_rpmsg_remove,
};

static int __init snd_mrfld_8958_rpmsg_init(void)
{
	return register_rpmsg_driver(&snd_mrfld_8958_rpmsg);
}
late_initcall(snd_mrfld_8958_rpmsg_init);

static void __exit snd_mrfld_8958_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&snd_mrfld_8958_rpmsg);
}
module_exit(snd_mrfld_8958_rpmsg_exit);

MODULE_DESCRIPTION("ASoC Intel(R) Merrifield MID Machine driver");
MODULE_AUTHOR("Vinod Koul <vinod.koul@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mrfld_wm8958");
