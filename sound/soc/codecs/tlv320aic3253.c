/*
 * Copyright (C) 2012-2013 Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <sound/tlv320aic3253.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#ifdef CONFIG_SND_SOC_TAS2552
#include "tas2552-core.h"
#endif
#include "tlv320aic3253.h"


#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		5
#define AIC3253_RATES	SNDRV_PCM_RATE_8000_48000
#define AIC3253_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

struct aic3253_priv {
	u32 sysclk;
	u8 page_no;
	struct i2c_client *control_data;
	u32 power_cfg;
	int rst_gpio;
	struct regulator *s4;
	int mclk_gpio;
};

/* Clock Dividers Structure */
struct aic3253_rate_divs {
	u32 mclk;
	u32 rate;
	u16 dosr;
	u8 ndac;
	u8 mdac;
	u8 aosr;
	u8 nadc;
	u8 madc;
};

/* 0dB min, 1dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_1, 0, 100, 0);
/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);

static const struct snd_kcontrol_new aic3253_snd_controls[] = {
	SOC_SINGLE_TLV("AIC PCM Playback VolumeL", AIC3253_LDACVOL,
			0, 0x30, 0, tlv_step_0_5),
	SOC_SINGLE_TLV("AIC PCM Playback VolumeR", AIC3253_RDACVOL,
			0, 0x30, 0, tlv_step_0_5),
	SOC_SINGLE_TLV("AIC Driver Gain VolumeL", AIC3253_HPLGAIN,
			0, 0x1D, 0, tlv_step_1),
	SOC_SINGLE_TLV("AIC Driver Gain VolumeR", AIC3253_HPRGAIN,
			0, 0x1D, 0, tlv_step_1),
	SOC_SINGLE("AIC DAC Playback SwitchL", AIC3253_HPLGAIN,
			6, 0x01, 0),
	SOC_SINGLE("AIC DAC Playback SwitchR", AIC3253_HPRGAIN,
			6, 0x01, 0),
	SOC_SINGLE("AIC Playback Powertune CTLL", AIC3253_LPLYCFG1,
			2, 0x02, 0),
	SOC_SINGLE("AIC Playback Powertune CTLR", AIC3253_RPLYCFG2,
			2, 0x02, 0),
	SOC_SINGLE("AIC Auto-mute Switch", AIC3253_DACMUTE, 4, 7, 0),
};

/* set up the clock dividers, rate = mclk/(ndac * mdac * dosr) */
static const struct aic3253_rate_divs aic3253_divs[] = {
	/* 16k rate */
	{AIC3253_SYSCLK_FREQ, 16000, 384, 1, 2, 128, 2, 4},
	/* 48k rate */
	{AIC3253_SYSCLK_FREQ, 48000, 128, 1, 2, 128, 2, 4},
};

static const struct snd_kcontrol_new hpl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("L_DAC Switch", AIC3253_HPLROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_L Switch", AIC3253_HPLROUTE, 2, 1, 0),
};

static const struct snd_kcontrol_new hpr_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("R_DAC Switch", AIC3253_HPRROUTE, 3, 1, 0),
	SOC_DAPM_SINGLE("IN1_R Switch", AIC3253_HPRROUTE, 2, 1, 0),
};

static int aic3253_spkramp_event(struct snd_soc_dapm_widget *wgt,
	struct snd_kcontrol *ctl, int event)
{
	pr_info("Enter:%s event %d", __func__, event);
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(wgt->name, "Ext Spk", 7)) {
#ifdef CONFIG_SND_SOC_TAS2552
			tas2552_ext_amp_on(1);
#endif
		} else {
			return -EINVAL;
		}
	} else {
		if (!strncmp(wgt->name, "Ext Spk", 7)) {
#ifdef CONFIG_SND_SOC_TAS2552
			tas2552_ext_amp_on(0);
#endif
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

static const struct snd_soc_dapm_widget aic3253_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("Left DAC", "Playback" , AIC3253_DACSETUP, 7, 0),
	SND_SOC_DAPM_MIXER("HPL AIC Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_output_mixer_controls[0],
			   ARRAY_SIZE(hpl_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPL Power", AIC3253_OUTPWRCTL, 5, 0, NULL, 0),

	SND_SOC_DAPM_DAC("Right DAC", "Playback", AIC3253_DACSETUP, 6, 0),
	SND_SOC_DAPM_MIXER("HPR AIC Output Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_output_mixer_controls[0],
			   ARRAY_SIZE(hpr_output_mixer_controls)),
	SND_SOC_DAPM_PGA("HPR Power", AIC3253_OUTPWRCTL, 4, 0, NULL, 0),
	SND_SOC_DAPM_SPK("Ext Spk", aic3253_spkramp_event),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_INPUT("IN1_L"),
	SND_SOC_DAPM_INPUT("IN1_R"),

};

static const struct snd_soc_dapm_route aic3253_dapm_routes[] = {
	/* Left Output */
	{"HPL AIC Output Mixer", "L_DAC Switch", "Left DAC"},
	{"HPL AIC Output Mixer", "IN1_L Switch", "IN1_L"},

	{"HPL Power", NULL, "HPL AIC Output Mixer"},
	{"HPL", NULL, "HPL Power"},

	/* Right Output */
	{"HPR AIC Output Mixer", "R_DAC Switch", "Right DAC"},
	{"HPR AIC Output Mixer", "IN1_R Switch", "IN1_R"},

	{"HPR Power", NULL, "HPR AIC Output Mixer"},
	{"HPR", NULL, "HPR Power"},
	/* Speaker */
	{"Ext Spk", NULL, "HPL"},
	{"Ext Spk", NULL, "HPR"},
};

/* I2C Read/Write Functions */
static int tlv320aic3253_i2c_read(struct i2c_client *tlv320aic3253_client,
				u8 reg, u8 *value)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = tlv320aic3253_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = tlv320aic3253_client->addr,
		 .flags = I2C_M_RD,
		 .len = 1,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(tlv320aic3253_client->adapter, msgs,
							ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tlv320aic3253_client->dev, "read transfer error %d\n"
				, err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tlv320aic3253_i2c_write(struct i2c_client *tlv320aic3253_client,
				u8 *value, u8 len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = tlv320aic3253_client->addr,
		 .flags = 0,
		 .len = len,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(tlv320aic3253_client->adapter, msgs,
					ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tlv320aic3253_client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static inline int aic3253_change_page(struct snd_soc_codec *codec,
				unsigned int new_page)
{
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	int ret;
	u8 buf[2] = {0, 0};
	buf[1] = (new_page & 0xff);

	ret = tlv320aic3253_i2c_write(codec->control_data, buf, ARRAY_SIZE(buf));
	if (ret == 0) {
		aic3253->page_no = new_page;
		return 0;
	} else {
		return ret;
	}
}

static int aic3253_bulk_write(struct snd_soc_codec *codec, unsigned int reg,
				const void *data, size_t len)
{
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	u8 *data_buf = (u8 *)data;
	u8 page = data_buf[AIC3253_PAGE_INDX];
	int ret;

	if (aic3253->page_no != page) {
		ret = aic3253_change_page(codec, page);
		if (ret != 0)
			return ret;
	}
	/* AIC3253_PAYLOAD_START index in received data buf specifies
		start address in the page memory */
	if (tlv320aic3253_i2c_write(codec->control_data,
			(data_buf + AIC3253_PAYLOAD_START), len) == 0)
		return 0;
	else
		return -EIO;
}

static int aic3253_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int val)
{
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	/* Calculate the page number from the register value, Register
		values are coded incrementally as per page size.
		EX: Page 1 reg 2 is coded as 130. below code extracts the
		page no and the register index on the page */
	unsigned int page = reg / (AIC3253_PAGE_SIZE);
	unsigned int fixed_reg = reg % (AIC3253_PAGE_SIZE);
	int ret;
	u8 buf[2] = {0, 0};
	buf[0] = (fixed_reg & 0xff);
	buf[1] = (val & 0xff);
	/* A write to aic3253_PSEL is really a non-explicit page change */
	if (reg == AIC3253_PSEL)
		return aic3253_change_page(codec, val);

	if (aic3253->page_no != page) {
		ret = aic3253_change_page(codec, page);
		if (ret != 0)
			return ret;
	}

	if (tlv320aic3253_i2c_write(codec->control_data, buf, ARRAY_SIZE(buf)) == 0)
		return 0;
	else
		return -EIO;
}

static unsigned int aic3253_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	unsigned int page = reg / (AIC3253_PAGE_SIZE);
	unsigned int fixed_reg = reg % (AIC3253_PAGE_SIZE);
	int ret;
	u8 val;

	if (aic3253->page_no != page) {
		ret = aic3253_change_page(codec, page);
		if (ret != 0)
			return ret;
	}

	if (tlv320aic3253_i2c_read(codec->control_data, fixed_reg & 0xff, &val) == 0)
		return val;
	else
		return -EIO;
}

static inline int aic3253_get_divs(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic3253_divs); i++) {
		if ((aic3253_divs[i].rate == rate)
		    && (aic3253_divs[i].mclk == mclk)) {
			return i;
		}
	}
	pr_err("aic3253: mclk: %d and sample rate: %d not supported",
			mclk, rate);
	return -EINVAL;
}

static int aic3253_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(&codec->dapm, aic3253_dapm_widgets,
				  ARRAY_SIZE(aic3253_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, aic3253_dapm_routes,
				ARRAY_SIZE(aic3253_dapm_routes));

	snd_soc_dapm_new_widgets(&codec->dapm);
	snd_soc_dapm_enable_pin(&codec->dapm, "Ext Spk");
	snd_soc_dapm_sync(&codec->dapm);
	return 0;
}

static int aic3253_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	if (freq != AIC3253_SYSCLK_FREQ)  {
		pr_err("aic3253: invalid frequency to set DAI system clock\n");
		return -EINVAL;
	}
	aic3253->sysclk = freq;
	return 0;
}

static int aic3253_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iface_reg_1;
	u8 iface_reg_2;
	u8 iface_reg_3;

	iface_reg_1 = snd_soc_read(codec, AIC3253_IFACE1);
	iface_reg_1 = iface_reg_1 & ~(3 << 6 | 3 << 2);
	iface_reg_2 = snd_soc_read(codec, AIC3253_IFACE2);
	iface_reg_2 = 0;
	iface_reg_3 = snd_soc_read(codec, AIC3253_IFACE3);
	iface_reg_3 = iface_reg_3 & ~(1 << 3);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg_1 |= AIC3253_BCLKMASTER | AIC3253_WCLKMASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		pr_err("aic3253: invalid DAI master/slave interface\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_reg_1 |= (AIC3253_DSP_MODE << AIC3253_PLLJ_SHIFT);
		iface_reg_3 |= (1 << 3); /* invert bit clock */
		iface_reg_2 = 0x01; /* add offset 1 */
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg_1 |= (AIC3253_DSP_MODE << AIC3253_PLLJ_SHIFT);
		iface_reg_3 |= (1 << 3); /* invert bit clock */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg_1 |=
			(AIC3253_RIGHT_JUSTIFIED_MODE << AIC3253_PLLJ_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg_1 |=
			(AIC3253_LEFT_JUSTIFIED_MODE << AIC3253_PLLJ_SHIFT);
		break;
	default:
		pr_err("aic3253: invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_write(codec, AIC3253_IFACE1, iface_reg_1);
	snd_soc_write(codec, AIC3253_IFACE2, iface_reg_2);
	snd_soc_write(codec, AIC3253_IFACE3, iface_reg_3);
	return 0;
}

static int aic3253_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	u8 data;
	int div;

	div = aic3253_get_divs(aic3253->sysclk, params_rate(params));
	if (div < 0) {
		pr_err("aic3253: sampling rate not supported\n");
		return div;
	}

	/* Use Mini DSP D for processing */
	data = snd_soc_read(codec, AIC3253_DACSPB);
	data &= ~(0x1f);
	snd_soc_write(codec, AIC3253_DACSPB, data | AIC3253_DSPDEN);

	/* Set Mini DSP Interpolation filterRatio */
	snd_soc_write(codec, AIC3253_DSPINT, 8);
	/* Mini DSP Decimation ratio to 4 */
	snd_soc_write(codec, AIC3253_DSPDEC, 4);

	data = snd_soc_read(codec, AIC3253_DACSPB);
	data &= ~(0x1f);

	/* enable Dac Adaptive filter Mode */
	snd_soc_write(codec, AIC3253_DACFLT, 4);

	/* NDAC divider value */
	data = snd_soc_read(codec, AIC3253_NDAC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC3253_NDAC, data | aic3253_divs[div].ndac);

	/* MDAC divider value */
	data = snd_soc_read(codec, AIC3253_MDAC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC3253_MDAC, data | aic3253_divs[div].mdac);

	/* DOSR MSB & LSB values */
	snd_soc_write(codec, AIC3253_DOSRMSB, aic3253_divs[div].dosr >> 8);
	snd_soc_write(codec, AIC3253_DOSRLSB,
		      (aic3253_divs[div].dosr & 0xff));

	/* NADC divider value */
	data = snd_soc_read(codec, AIC3253_NADC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC3253_NADC, data | aic3253_divs[div].nadc);

	/* MADC divider value */
	data = snd_soc_read(codec, AIC3253_MADC);
	data &= ~(0x7f);
	snd_soc_write(codec, AIC3253_MADC, data | aic3253_divs[div].madc);

	/* AOSR value */
	snd_soc_write(codec, AIC3253_AOSR, aic3253_divs[div].aosr);

	/* set up ADC for PDM data input and clock for
		feedback processing */
	snd_soc_write(codec, AIC3253_ADCSETUP, AIC3253_ADCSETUP_PDM);

	data = snd_soc_read(codec, AIC3253_IFACE1);
	data = data & ~(3 << 4);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC3253_WORD_LEN_20BITS << AIC3253_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (AIC3253_WORD_LEN_24BITS << AIC3253_DOSRMSB_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC3253_WORD_LEN_32BITS << AIC3253_DOSRMSB_SHIFT);
		break;
	}
	snd_soc_write(codec, AIC3253_IFACE1, data);

	return 0;
}

static int aic3253_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 dac_reg;

	dac_reg = snd_soc_read(codec, AIC3253_DACMUTE) & ~AIC3253_MUTEON;
	if (mute)
		snd_soc_write(codec, AIC3253_DACMUTE, dac_reg | AIC3253_MUTEON);
	else
		snd_soc_write(codec, AIC3253_DACMUTE, dac_reg);
	return 0;
}

static int aic3253_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* Switch on NDAC Divider */
		snd_soc_update_bits(codec, AIC3253_NDAC,
				    AIC3253_NDACEN, AIC3253_NDACEN);

		/* Switch on MDAC Divider */
		snd_soc_update_bits(codec, AIC3253_MDAC,
				    AIC3253_MDACEN, AIC3253_MDACEN);

		/* Switch on NADC Divider */
		snd_soc_update_bits(codec, AIC3253_NADC,
				    AIC3253_NADCEN, AIC3253_NADCEN);

		/* Switch on MADC Divider */
		snd_soc_update_bits(codec, AIC3253_MADC,
				    AIC3253_MADCEN, AIC3253_MADCEN);

		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* Switch off NDAC Divider */
		snd_soc_update_bits(codec, AIC3253_NDAC,
				    AIC3253_NDACEN, 0);

		/* Switch off MDAC Divider */
		snd_soc_update_bits(codec, AIC3253_MDAC,
				    AIC3253_MDACEN, 0);

		/* Switch off NADC Divider */
		snd_soc_update_bits(codec, AIC3253_NADC,
				    AIC3253_NADCEN, 0);

		/* Switch off MADC Divider */
		snd_soc_update_bits(codec, AIC3253_MADC,
				    AIC3253_MADCEN, 0);

		break;
	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static const struct snd_soc_dai_ops aic3253_ops = {
	.hw_params = aic3253_hw_params,
	.digital_mute = aic3253_mute,
	.set_fmt = aic3253_set_dai_fmt,
	.set_sysclk = aic3253_set_dai_sysclk,
};

static struct snd_soc_dai_driver aic3253_dai = {
	.name = "tlv320aic3253_codec",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AIC3253_RATES,
		     .formats = AIC3253_FORMATS,},
	.ops = &aic3253_ops,
	.symmetric_rates = 1,
};

static int aic3253_suspend(struct snd_soc_codec *codec)
{
	aic3253_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aic3253_resume(struct snd_soc_codec *codec)
{
	aic3253_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

static void aic3253_hw_reset(int rst_gpio)
{
	gpio_set_value_cansleep(rst_gpio, 0);
	usleep(1);
	gpio_set_value_cansleep(rst_gpio, 1);
}

static int aic3253_probe(struct snd_soc_codec *codec)
{
	struct aic3253_priv *aic3253 = snd_soc_codec_get_drvdata(codec);
	u32 tmp_reg;
	/* Set codec Bulk write method, will be used for
		loading DSP coeffs/Instrs */
	codec->bulk_write_raw = aic3253_bulk_write;
	codec->control_data = aic3253->control_data;

	snd_soc_write(codec, AIC3253_RESET, 0x01);
	/* wait for 1ms after reset*/
	msleep_interruptible(1);

	/* Power platform configuration */
	if (aic3253->power_cfg & AIC3253_PWR_AVDD_DVDD_WEAK_DISABLE)
		snd_soc_write(codec, AIC3253_PWRCFG, AIC3253_AVDDWEAKDISABLE);

	tmp_reg = (aic3253->power_cfg & AIC3253_PWR_AIC3253_LDO_ENABLE) ?
			AIC3253_LDOCTLEN : 0;
	snd_soc_write(codec, AIC3253_LDOCTL, tmp_reg);

	/* Set up pins MFP4 and MFP1 for PDM data and clock */
	snd_soc_write(codec, AIC3253_PDMIN, AIC3253_PDMINEN);
	snd_soc_write(codec, AIC3253_PDMIN, AIC3253_PDMCLKEN);

	/* write mini dsp coefficinets and instructions for DAC */
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_coeffs1,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_coeffs2,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_coeffs3,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_coeffs4,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr1,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr2,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr3,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr4,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr5,
					AIC3253_PAGE_SIZE);
	snd_soc_bulk_write_raw(codec, 1, (const void *)aic3253_dac_instr6,
					AIC3253_PAGE_SIZE);

	aic3253_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	snd_soc_add_codec_controls(codec, aic3253_snd_controls,
			     ARRAY_SIZE(aic3253_snd_controls));
	aic3253_add_widgets(codec);
	pr_info("tlv320aic3253 codec registered");
	return 0;
}

static int aic3253_remove(struct snd_soc_codec *codec)
{
	aic3253_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_aic3253 = {
	.read = aic3253_read,
	.write = aic3253_write,
	.probe = aic3253_probe,
	.remove = aic3253_remove,
	.suspend = aic3253_suspend,
	.resume = aic3253_resume,
	.set_bias_level = aic3253_set_bias_level,
};

#ifdef CONFIG_OF
static struct aic3253_pdata *
aic3253_of_init(struct i2c_client *client)
{
	struct aic3253_pdata *pdata;
	struct device_node *np = client->dev.of_node;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s : pdata allocation failure\n", __func__);
		return NULL;
	}

	pdata->reset_gpio = of_get_gpio(np, 0);
	pdata->mclk_sel_gpio = of_get_gpio(np, 1);

	return pdata;
}
#else
static inline struct aic3253_pdata *
aic3253_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int aic3253_gpio_init(struct aic3253_pdata *pdata)
{
	int ret;

	ret = gpio_request(pdata->reset_gpio, "aic reset gpio");
	if (ret)
		return ret;

	ret = gpio_direction_output(pdata->reset_gpio, 0);
	if (ret)
		goto free_reset;

	ret = gpio_request(pdata->mclk_sel_gpio, "mclk sel gpio");
	if (ret)
		pr_info("%s : mclk gpio not assigned\n", __func__);
	else {
		ret = gpio_direction_output(pdata->mclk_sel_gpio, 1);
		if (ret)
			goto free_mclk;
	}

	return 0;

free_mclk:
	gpio_free(pdata->mclk_sel_gpio);
free_reset:
	gpio_free(pdata->reset_gpio);
	return ret;
}

static void aic3253_gpio_free(struct aic3253_priv *dev)
{
	gpio_free(dev->rst_gpio);
	gpio_free(dev->mclk_gpio);
}

static __devinit int aic3253_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct aic3253_pdata *pdata;
	struct aic3253_priv *aic3253;
	int ret;

	dev_dbg(&i2c->dev, "Probing aic3253 driver\n");

	if (i2c->dev.of_node)
		pdata = aic3253_of_init(i2c);
	else
		pdata = i2c->dev.platform_data;

	/* Check platform data */
	if (pdata == NULL) {
		dev_err(&i2c->dev, "no pdata found, aic3253 probe fail\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aic3253 = devm_kzalloc(&i2c->dev, sizeof(struct aic3253_priv),
			       GFP_KERNEL);
	if (aic3253 == NULL)
		return -ENOMEM;

	ret = aic3253_gpio_init(pdata);
	if (ret) {
		dev_err(&i2c->dev, "gpio init failed\n");
		return ret;
	}

	aic3253->control_data = i2c;
	i2c_set_clientdata(i2c, aic3253);
	aic3253->rst_gpio = pdata->reset_gpio;
	aic3253->mclk_gpio = pdata->mclk_sel_gpio;
	aic3253->power_cfg = AIC3253_PWR_AVDD_DVDD_WEAK_DISABLE |
					AIC3253_PWR_AIC3253_LDO_ENABLE;
	aic3253->page_no = 0;

	/* enable regulator */

	aic3253->s4 = regulator_get(&i2c->dev, "aic_vdd");
	if (IS_ERR(aic3253->s4)) {
		pr_err("%s: Error getting S4 regulator.\n", __func__);
		ret = PTR_ERR(aic3253->s4);
		goto reg_get;
	}

	regulator_set_voltage(aic3253->s4, 1800000, 1800000);

	ret = regulator_enable(aic3253->s4);
	if (ret < 0) {
		pr_err("%s: Error enabling s4 regulator.\n", __func__);
		goto reg_enable;
	}

	/* perform hw reset */
	aic3253_hw_reset(aic3253->rst_gpio);

	/* select MI2S MCLK as sys clk input */
	if (gpio_is_valid(aic3253->mclk_gpio))
		gpio_set_value_cansleep(aic3253->mclk_gpio, 1);

	/* register codec */
	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_aic3253, &aic3253_dai, 1);
	if (ret < 0) {
		pr_err("%s: Error registering aic3253 codec\n", __func__);
		goto codec_fail;
	}

	pr_info("tlv320aic3253 probed %d:", ret);
	return ret;

codec_fail:
reg_enable:
	regulator_disable(aic3253->s4);
	regulator_put(aic3253->s4);
reg_get:
	aic3253_gpio_free(aic3253);
	return ret;
}

static __devexit int aic3253_i2c_remove(struct i2c_client *client)
{
	struct aic3253_priv *aic3253 = i2c_get_clientdata(client);
	snd_soc_unregister_codec(&client->dev);
	regulator_disable(aic3253->s4);
	regulator_put(aic3253->s4);
	aic3253_gpio_free(aic3253);
	return 0;
}

static const struct i2c_device_id aic3253_i2c_id[] = {
	{ "tlv320aic3253", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, aic3253_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id aic3253_match_tbl[] = {
	{ .compatible = "ti,aic3253" },
	{ },
};
MODULE_DEVICE_TABLE(of, aic3253_match_tbl);
#endif

static struct i2c_driver aic3253_i2c_driver = {
	.driver = {
		.name = "tlv320aic3253",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aic3253_match_tbl),
	},
	.probe =    aic3253_i2c_probe,
	.remove =   __devexit_p(aic3253_i2c_remove),
	.id_table = aic3253_i2c_id,
};

static int __init aic3253_modinit(void)
{
	int ret = 0;

	ret = i2c_add_driver(&aic3253_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register aic3253 I2C driver: %d\n",
		       ret);
	}
	return ret;
}
module_init(aic3253_modinit);

static void __exit aic3253_exit(void)
{
	i2c_del_driver(&aic3253_i2c_driver);
}
module_exit(aic3253_exit);

MODULE_DESCRIPTION("ASoC tlv320aic3253 codec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility");
