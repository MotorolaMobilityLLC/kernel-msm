/*
 * Copyright (C) 2012 Motorola Mobility, LLC.
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

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <sound/tfa9890.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "tfa9890-core.h"


#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		5
#define PLL_SYNC_RETRIES		10

#define TFA9890_RATES	SNDRV_PCM_RATE_8000_48000
#define TFA9890_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)


struct tfa9890_priv {
	struct i2c_client *control_data;
	struct regulator *vdd;
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa9890_wq;
	struct work_struct work;
	struct mutex dsp_init_lock;
	struct mutex i2c_rw_lock;
	int dsp_init;
	int speaker_imp;
	int sysclk;
};

static const struct tfa9890_regs tfa9890_reg_defaults[] = {
{
	.reg = TFA9890_BATT_CTL_REG,
	.value = 0x5382,
},
{
	.reg = TFA9890_VOL_CTL_REG,
	.value = 0x002f,
},
{
	.reg = TFA9890_SPK_CTL_REG,
	.value = 0x4800,
},
{
	.reg = TFA9890_DC2DC_CTL_REG,
	.value = 0x8FFE,
},
{
	.reg = TFA9890_SYS_CTL1_REG,
	.value =  0x82D5,
},
{
	.reg = TFA9890_SYS_CTL2_REG,
	.value = 0x38E5,
},
{
	.reg = TFA9890_RESERVED_REG,
	.value = 0x7F,
},
{
	.reg = TFA9890_PWM_CTL_REG,
	.value = 0x0308,
},
{
	.reg = TFA9890_CURRT_SNS_REG,
	.value = 0x0340,
},
{
	.reg = TFA9890_CURRC_SNS_REG,
	.value = 0xAD93,
},
{
	.reg = TFA9890_CURRT_CTL_REG,
	.value = 0x4000,
},
{
	.reg = TFA9890_DEM_CTL_REG,
	.value = 0x00,
},
};

/* table used by ALSA core while creating codec register
 * access debug fs.
 */
static u8 tfa9890_reg_readable[TFA9890_REG_CACHE_SIZE] = {
[TFA9890_SYS_STATUS_REG] = 1,
[TFA9890_BATT_STATUS_REG] = 1,
[TFA9890_TEMP_STATUS_REG] = 1,
[TFA9890_REV_ID] = 1,
[TFA9890_I2S_CTL_REG] = 1,
[TFA9890_BATT_CTL_REG] = 1,
[TFA9890_VOL_CTL_REG] = 1,
[TFA9890_DC2DC_CTL_REG] = 1,
[TFA9890_SPK_CTL_REG] = 1,
[TFA9890_SYS_CTL1_REG] = 1,
[TFA9890_SYS_CTL2_REG] = 1,
[TFA9890_MTP_KEY_REG] = 1,
[TFA9890_RESERVED_REG] = 1,
[TFA9890_PWM_CTL_REG] = 1,
[TFA9890_CURRT_SNS_REG] = 1,
[TFA9890_CURRC_SNS_REG] = 1,
[TFA9890_CURRT_CTL_REG] = 1,
[TFA9890_DEM_CTL_REG] = 1,
[TFA9890_MTP_COPY_REG] = 1,
[TFA9890_CF_CONTROLS] = 1,
[TFA9890_CF_MAD] = 1,
[TFA9890_CF_MEM] = 0,
[TFA9890_CF_STATUS] = 1,
[TFA9890_MTP_REG] = 1,
};


/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);

static const struct snd_kcontrol_new tfa9890_snd_controls[] = {
	SOC_SINGLE_TLV("NXP Volume", TFA9890_VOL_CTL_REG,
			8, 0xff, 0, tlv_step_0_5),
};

/* bit 6,7 : 01 - DSP bypassed, 10 - DSP used */
static const struct snd_kcontrol_new tfa9890_mixer_controls[] = {
	SOC_DAPM_SINGLE("DSP Switch", TFA9890_I2S_CTL_REG, 6, 3, 0),
};

static const struct snd_soc_dapm_widget tfa9890_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("I2S1"),
	SND_SOC_DAPM_MIXER("NXP Output Mixer", SND_SOC_NOPM, 0, 0,
			   &tfa9890_mixer_controls[0],
			   ARRAY_SIZE(tfa9890_mixer_controls)),
};

static const struct snd_soc_dapm_route tfa9890_dapm_routes[] = {
	{"NXP Output Mixer", "DSP Switch", "I2S1"},
};

/*
 * I2C Read/Write Functions
 */

static int tfa9890_i2c_read(struct i2c_client *tfa9890_client,
				u8 reg, u8 *value, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = tfa9890_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = tfa9890_client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(tfa9890_client->adapter, msgs,
							ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tfa9890_client->dev, "read transfer error %d\n"
				, err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tfa9890_i2c_write(struct i2c_client *tfa9890_client,
				u8 *value, u8 len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = tfa9890_client->addr,
		 .flags = 0,
		 .len = len,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(tfa9890_client->adapter, msgs,
					ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tfa9890_client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tfa9890_bulk_write(struct snd_soc_codec *codec, unsigned int reg,
				const void *data, size_t len)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u8 *data_buf = (u8 *)data;
	int ret;

	mutex_lock(&tfa9890->i2c_rw_lock);
	ret = tfa9890_i2c_write(codec->control_data, data_buf, len);
	mutex_unlock(&tfa9890->i2c_rw_lock);
	return ret;
}

static int tfa9890_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int val)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u8 buf[3] = {0, 0, 0};
	int ret;
	buf[0] = (reg & 0xff);
	buf[1] = (val >> 8) & 0xff;
	buf[2] = (val & 0xff);

	mutex_lock(&tfa9890->i2c_rw_lock);
	ret = tfa9890_i2c_write(codec->control_data, buf, ARRAY_SIZE(buf));
	mutex_unlock(&tfa9890->i2c_rw_lock);
	return ret;
}

static unsigned int tfa9890_read(struct snd_soc_codec *codec, unsigned int reg)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u8 buf[3];
	int len;
	int val = -EIO;

	mutex_lock(&tfa9890->i2c_rw_lock);
	/* check if we are reading from Device memory whose word size is
	 * 2 bytes or DSP memory whose word size is 3 bytes.
	 */
	if (reg == TFA9890_CF_MEM)
		len = 3;
	else
		len = 2;

	if (tfa9890_i2c_read(codec->control_data, reg & 0xff, buf, len) == 0) {
		if (len == 3)
			val = (buf[0] << 16 | buf[1] << 8 | buf[2]);
		else if (len == 2)
			val = (buf[0] << 8 | buf[1]);
	}
	mutex_unlock(&tfa9890->i2c_rw_lock);
	return val;
}

/*
 * DSP Read/Write/Setup Functions
 */

/* RPC Protocol to Access DSP Memory is implemented in tfa9890_dsp_transfer func
 *- The host writes into a certain DSP memory location the data for the RPC
 *- Module and Id of the function
 *- Parameters for the function (in case of SetParam), maximum 145 parameters
 *- The host triggers the DSP that there is valid RPC data
 *	- The DSP executes the RPC and stores the result
 *		-An error code
 *		-Return parameters (in case of GetParam)
 *	- The host reads the result
 */

static int tfa9890_dsp_transfer(struct snd_soc_codec *codec,
			int module_id, int param_id, const u8 *bytes,
			int len, int type, int *read)
{
	u8 buffer[8];
	/* DSP mem access control */
	u16 cf_ctrl = TFA9890_CF_CTL_MEM_REQ;
	/* memory address to be accessed (0 : Status, 1 : ID, 2 : parameters) */
	u16 cf_mad = TFA9890_CF_MAD_ID;
	int err;
	int rpc_status;
	int cf_status;

	/* first the data for CF_CONTROLS */
	buffer[0] = TFA9890_CF_CONTROLS;
	buffer[1] = ((cf_ctrl >> 8) & 0xFF);
	buffer[2] = (cf_ctrl & 0xFF);
	/* write the contents of CF_MAD which is the subaddress
	 * following CF_CONTROLS.
	 */
	buffer[3] = ((cf_mad >> 8) & 0xFF);
	buffer[4] = (cf_mad & 0xFF);
	/* write the module and RPC id into CF_MEM, which follows CF_MAD */
	buffer[5] = 0;
	buffer[6] = module_id + 128;
	buffer[7] = param_id;
	err = codec->bulk_write_raw(codec, TFA9890_CF_CONTROLS, buffer,
						ARRAY_SIZE(buffer));
	if (err < 0) {
		pr_err("tfa9890: Failed to Write DSP controls req %d:", err);
		return err;
	}
	/* check transfer type */
	if (type == TFA9890_DSP_WRITE) {
		/* write data to DSP memory*/
		err = codec->bulk_write_raw(codec, TFA9890_CF_MEM, bytes, len);
		if (err < 0)
			return err;
	}

	/* wake up the DSP to process the data */
	cf_ctrl |= TFA9890_CF_ACK_REQ | TFA9890_CF_INT_REQ;
	snd_soc_write(codec, TFA9890_CF_CONTROLS, cf_ctrl);

	/* wait for ~1 msec (max per spec) and check for DSP status */
	msleep_interruptible(1);
	cf_status = snd_soc_read(codec, TFA9890_CF_STATUS);
	if ((cf_status & TFA9890_STATUS_CF) == 0) {
		pr_err("tfa9890: Failed to ack DSP CTL req %d:", err);
		return -EIO;
	}
	/* read the RPC Status */
	cf_ctrl = TFA9890_CF_CTL_MEM_REQ;
	buffer[0] = TFA9890_CF_CONTROLS;
	buffer[1] = (unsigned char)((cf_ctrl >> 8) & 0xFF);
	buffer[2] = (unsigned char)(cf_ctrl & 0xFF);
	cf_mad = TFA9890_CF_MAD_STATUS;
	buffer[3] = (unsigned char)((cf_mad >> 8) & 0xFF);
	buffer[4] = (unsigned char)(cf_mad & 0xFF);
	err = codec->bulk_write_raw(codec, TFA9890_CF_CONTROLS, buffer, 5);
	if (err < 0) {
		pr_err("tfa9890: Failed to Write NXP DSP CTL reg for rpc check%d",
				err);
		return err;
	}

	rpc_status = snd_soc_read(codec, TFA9890_CF_MEM);
	if (rpc_status != TFA9890_STATUS_OK) {
		pr_err("tfa9890: RPC status check failed %d",
				err);
		return -EIO;
	}
	if (type == TFA9890_DSP_READ) {
		cf_mad = TFA9890_CF_MAD_PARAM;
		snd_soc_write(codec, TFA9890_CF_MAD, cf_mad);
		*read = snd_soc_read(codec, TFA9890_CF_MEM);
	}

	return 0;
}

static int tfa9890_coldboot(struct snd_soc_codec *codec)
{
	u8 coldpatch[8] = {TFA9890_CF_CONTROLS, 0x00, 0x07, 0x81, 0x00,
				0x00, 0x00, 0x01};
	int ret;
	u16 val;

	/* This will cold boot the DSP */
	ret = codec->bulk_write_raw(codec, TFA9890_CF_CONTROLS, coldpatch,
				ARRAY_SIZE(coldpatch));
	if (ret < 0)
		return ret;

	val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);

	/* Check if cold started sucessfully */
	if (TFA9890_STATUS_ACS & val)
		ret = 0;
	else
		ret = -EINVAL;

	return ret;
}

static void tfa9890_power(struct snd_soc_codec *codec, int on)
{
	u16 val;

	val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
	if (on) {
		/* set up device in follower mode and power up device,
		 * boost will be put in active mode in hw param callback.
		 */
		val = val & ~(TFA9890_STATUS_DC2DC);
		val = val & ~(0x1 << 7);
		val = val & ~(0x1);
	} else
		val = val | (TFA9890_POWER_DOWN);

	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);
}

static void tfa9890_calibaration(struct tfa9890_priv *tfa9890)
{
	u16 val;
	struct snd_soc_codec *codec = tfa9890->codec;

	/* set PWM slope to 0000 while calibarating */
	val = snd_soc_read(codec, TFA9890_VOL_CTL_REG);
	val = val & ~(TFA9890_PWM_MASK);
	snd_soc_write(codec, TFA9890_VOL_CTL_REG, val);

	/* unlock write access MTP memory*/
	snd_soc_write(codec, TFA9890_MTP_KEY_REG, TFA9890_MTK_KEY);

	val = snd_soc_read(codec, TFA9890_MTP_REG);
	/* set MTPOTC = 1 and MTPEX = 0 */
	val = val | TFA9890_MTPOTC;
	val = val & (~(TFA9890_STATUS_MTPEX));
	snd_soc_write(codec, TFA9890_MTP_REG, val);
	/* set CIMTB to initiate copy of calib values */
	val = snd_soc_read(codec, TFA9890_MTP_COPY_REG);
	val = val | TFA9890_STATUS_CIMTP;
	snd_soc_write(codec, TFA9890_MTP_COPY_REG, val);

	tfa9890_coldboot(codec);

	/* set pwm slope back to init val */
	val = snd_soc_read(codec, TFA9890_VOL_CTL_REG);
	val = val | (TFA9890_PWM_VAL);
	snd_soc_write(codec, TFA9890_VOL_CTL_REG, val);
}

static void tfa9890_dsp_init(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv, work);
	struct snd_soc_codec *codec = tfa9890->codec;
	const struct firmware *fw_speaker;
	const struct firmware *fw_config;
	const struct firmware *fw_preset;
	const struct firmware *fw_coeff;
	u16 val;
	int read;
	int ret;
	int tries = 0;

	pr_info("tfa9890: Initializing DSP");
	mutex_lock(&tfa9890->dsp_init_lock);
	/* check if DSP pll is synced, It should be sync'ed at this point */
	do {
		val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) && (val & TFA9890_STATUS_CLKS))
			break;
		msleep_interruptible(1);
	} while ((++tries < PLL_SYNC_RETRIES));

	if (tries == PLL_SYNC_RETRIES) {
		pr_err("tfa9890:DSP pll sync failed!!");
		goto out;
	}

	/* cold boot device before loading firmware and parameters */
	ret = tfa9890_coldboot(codec);
	if (ret < 0) {
		pr_err("tfa9890: cold boot failed!!");
		goto out;
	}
	/* Load DSP config and firmware files */
	ret = request_firmware(&fw_speaker, "tfa9890.speaker", codec->dev);
	if (ret) {
		pr_err("tfa9890: Failed to locate speaker model!!");
		goto out;
	}
	if (fw_speaker->size != TFA9890_SPK_FW_SIZE) {
		pr_err("tfa9890: Data size check failed for spkr model");
		goto out;
	}
	ret = tfa9890_dsp_transfer(codec, TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_SET_LSMODEL, fw_speaker->data,
			fw_speaker->size, TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		goto out;

	ret = request_firmware(&fw_config, "tfa9890.config", codec->dev);
	if (ret) {
		pr_err("tfa9890: Failed to locate dsp config!!");
		goto out;
	}
	if (fw_config->size != TFA9890_CFG_FW_SIZE) {
		pr_err("%s: Data size check failed for config file", __func__);
		goto out;
	}
	ret = tfa9890_dsp_transfer(codec, TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_SET_CONFIG, fw_config->data,
			fw_config->size, TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		goto out;

	ret = request_firmware(&fw_preset, "tfa9890.preset", codec->dev);
	if (ret) {
		pr_err("tfa9890: Failed to locate DSP preset file");
		goto out;
	}
	if (fw_preset->size != TFA9890_PST_FW_SIZE) {
		pr_err("tfa9890: Data size check failed preset file");
		goto out;
	}
	ret = tfa9890_dsp_transfer(codec, TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_SET_PRESET, fw_preset->data,
			fw_preset->size, TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		goto out;

	ret = request_firmware(&fw_coeff, "tfa9890.eq", codec->dev);
	if (ret) {
		pr_err("tfa9890: Failed to locate DSP coefficients");
		goto out;
	}
	if (fw_coeff->size != TFA9890_COEFF_FW_SIZE) {
		pr_err("tfa9890: Data size check failed coefficients");

		goto out;
	}
	ret = tfa9890_dsp_transfer(codec, TFA9890_DSP_MOD_BIQUADFILTERBANK,
			0, fw_coeff->data, fw_coeff->size,
			TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		goto out;

	/* set all dsp config loaded */
	val = (u16)snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
	val = val | TFA9890_SYSCTRL_CONFIGURED;
	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);

	/* check if calibaration completed, Calibaration is one time event.
	 * will be done only once when device boots up for the first time.Once
	 * calibarated info is stored in non-volatile memory of the device.
	 * It will be part of the factory test to validate speaker imp.
	 */
	val = snd_soc_read(codec, TFA9890_MTP_REG);
	if (!(val & TFA9890_STATUS_MTPEX)) {
		pr_info("tfa9890:Calib not completed initiating ..");
		tfa9890_calibaration(tfa9890);
	} else {
		/* speaker impedence available to read*/
		tfa9890_dsp_transfer(codec, TFA9890_DSP_MOD_SPEAKERBOOST,
		TFA9890_PARAM_GET_RE0, 0, 3, TFA9890_DSP_READ, &read);
		tfa9890->speaker_imp = read/(1 << (23 - TFA9890_SPKR_IMP_EXP));
	}

	tfa9890->dsp_init = TFA9890_DSP_INIT_DONE;
	mutex_unlock(&tfa9890->dsp_init_lock);
	return;
out:
	tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
	mutex_unlock(&tfa9890->dsp_init_lock);
}

/*
 * ASOC OPS
*/

static int tfa9890_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct tfa9890_priv *tfa9890 =
				snd_soc_codec_get_drvdata(codec_dai->codec);
	tfa9890->sysclk = freq;
	return 0;
}

static int tfa9890_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 val;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* default value */
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	default:
		/* only supports Slave mode */
		pr_err("tfa9890: invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* default value */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val = val & ~(TFA9890_FORMAT_MASK);
		val = val | TFA9890_FORMAT_LSB;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val = val & ~(TFA9890_FORMAT_MASK);
		val = val | TFA9890_FORMAT_MSB;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	default:
		pr_err("tfa9890: invalid DAI interface format\n");
		return -EINVAL;
	}

	return 0;
}

static int tfa9890_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u16 val;
	int bclk;
	int bclk_ws_ratio;
	int bclk_div;

	/* set boost in active mode */
	val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
	val = val | TFA9890_STATUS_DC2DC;
	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);

	/* validate and set params */
	if (params_format(params) != SNDRV_PCM_FORMAT_S16_LE) {
		pr_err("tfa9890: invalid pcm bit lenght\n");
		return -EINVAL;
	}

	val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
	/* clear Sample rate bits */
	val = val & ~(TFA887_SAMPLE_RATE);

	switch (params_rate(params)) {
	case 8000:
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 11025:
		val = val | TFA9890_SAMPLE_RATE_11k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 12000:
		val = val | TFA9890_SAMPLE_RATE_12k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 16000:
		val = val | TFA9890_SAMPLE_RATE_16k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 22050:
		val = val | TFA9890_SAMPLE_RATE_22k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 24000:
		val = val | TFA9890_SAMPLE_RATE_24k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 32000:
		val = val | TFA9890_SAMPLE_RATE_32k;
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
		break;
	case 44100:
		val = val | TFA9890_SAMPLE_RATE_44k;
		break;
	case 48000:
		val = val | TFA9890_SAMPLE_RATE_48k;
		break;
	default:
		pr_err("tfa9890: invalid sample rate\n");
		return -EINVAL;
	}
	snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
	/* calc bclk to ws freq ratio, tfa9890 supports only 32, 48, 64 */
	bclk_div = tfa9890->sysclk/(params_rate(params) * 16 * 2);
	bclk = tfa9890->sysclk/bclk_div;
	bclk_ws_ratio = bclk/params_rate(params);
	if (bclk_ws_ratio != 32 && bclk_ws_ratio != 48
			&& bclk_ws_ratio != 64) {
		pr_err("tfa9890: invalid bit clk to ws freq ratio %d:",
			bclk_ws_ratio);
		return -EINVAL;
	}

	return 0;
}

static int tfa9890_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 val;

	if (mute) {
		val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
		/* turn Off amp */
		val = val & ~(TFA9890_STATUS_AMPE);
		snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);
		/* mute amp */
		val = snd_soc_read(codec, TFA9890_VOL_CTL_REG);
		val = val | (TFA9890_STATUS_MUTE);
		snd_soc_write(codec,  TFA9890_VOL_CTL_REG, val);
	} else {
		val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
		/* turn ON amp */
		val = val | TFA9890_STATUS_AMPE;
		snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);
		/* unmute amp */
		val = snd_soc_read(codec, TFA9890_VOL_CTL_REG);
		val = val & ~(TFA9890_STATUS_MUTE);
		snd_soc_write(codec,  TFA9890_VOL_CTL_REG, val);
	}

	return 0;
}


static int tfa9890_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&tfa9890->dsp_init_lock);
	if (tfa9890->dsp_init != TFA9890_DSP_INIT_FAIL)
		tfa9890_power(codec, 1);
	mutex_unlock(&tfa9890->dsp_init_lock);
	return 0;
}

static void tfa9890_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	tfa9890_power(dai->codec, 0);
}

/* Trigger callback is atomic function, It gets called when pcm is started */

static int tfa9890_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(dai->codec);
	int ret = 0;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* To initialize dsp all the I2S signals should be bought up,
		 * so that the DSP's internal PLL can sync up and memory becomes
		 * accessible. Trigger callback is called when pcm write starts,
		 * so this should be the place where DSP is initialized
		 */
		if (tfa9890->dsp_init == TFA9890_DSP_INIT_PENDING)
			queue_work(tfa9890->tfa9890_wq, &tfa9890->work);
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*
 * SysFS support
 */

static ssize_t tfa9890_show_spkr_imp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tfa9890_priv *tfa9890 = i2c_get_clientdata(to_i2c_client(dev));
	u16 val;
	int read_imp = 0;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			/* if I2S CLKS are ON read from DSP mem, otherwise print
			 * stored value as DSP mem cannot be accessed.
			 */
			tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_GET_RE0,
				0, 3, TFA9890_DSP_READ, &read_imp);
			/* convert to fixed point decimal*/
			tfa9890->speaker_imp =
				read_imp/(1 << (23 - TFA9890_SPKR_IMP_EXP));
			return snprintf(buf, 8, "%u\n", tfa9890->speaker_imp);
		}
		return snprintf(buf, 8, "%u\n", tfa9890->speaker_imp);
	}
	return snprintf(buf, 8, "%u\n", 0);
}

static ssize_t tfa9890_force_calibaration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tfa9890_priv *tfa9890 = i2c_get_clientdata(client);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0 || val > 1)
		return -EINVAL;

	tfa9890_calibaration(tfa9890);

	return count;
}

static DEVICE_ATTR(spkr_imp, S_IRUGO,
		   tfa9890_show_spkr_imp, NULL);

static DEVICE_ATTR(force_calib, S_IWUSR,
		   NULL, tfa9890_force_calibaration);

static struct attribute *tfa9890_attributes[] = {
	&dev_attr_spkr_imp.attr,
	&dev_attr_force_calib.attr,
	NULL
};

static const struct attribute_group tfa9890_attr_group = {
	.attrs = tfa9890_attributes,
};


static const struct snd_soc_dai_ops tfa9890_ops = {
	.hw_params = tfa9890_hw_params,
	.digital_mute = tfa9890_mute,
	.set_fmt = tfa9890_set_dai_fmt,
	.set_sysclk = tfa9890_set_dai_sysclk,
	.startup	= tfa9890_startup,
	.shutdown	= tfa9890_shutdown,
	.trigger = tfa9890_trigger,
};

static struct snd_soc_dai_driver tfa9890_dai = {
	.name = "tfa9890_codec",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = TFA9890_RATES,
		     .formats = TFA9890_FORMATS,},
	.ops = &tfa9890_ops,
	.symmetric_rates = 1,
};

static int tfa9890_probe(struct snd_soc_codec *codec)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	int i;

	/* set codec Bulk write method, will be used for
	 * loading DSP firmware and config files.
	 */
	codec->bulk_write_raw = tfa9890_bulk_write;
	codec->control_data = tfa9890->control_data;
	tfa9890->codec = codec;
	/* reset registers to Default values */
	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, 0x0002);

	/* initialize I2C registers */
	for (i = 0; i < ARRAY_SIZE(tfa9890_reg_defaults); i++) {
		snd_soc_write(codec, tfa9890_reg_defaults[i].reg,
						tfa9890_reg_defaults[i].value);
	}
	/* add controls and didgets */
	snd_soc_add_codec_controls(codec, tfa9890_snd_controls,
			     ARRAY_SIZE(tfa9890_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, tfa9890_dapm_widgets,
				  ARRAY_SIZE(tfa9890_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, tfa9890_dapm_routes,
				ARRAY_SIZE(tfa9890_dapm_routes));

	snd_soc_dapm_new_widgets(&codec->dapm);
	snd_soc_dapm_sync(&codec->dapm);

	pr_info("tfa9890 codec registered");
	return 0;

}

static int tfa9890_remove(struct snd_soc_codec *codec)
{
	tfa9890_power(codec, 0);
	return 0;
}

static int tfa9890_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	return tfa9890_reg_readable[reg];
}

static struct snd_soc_codec_driver soc_codec_dev_tfa9890 = {
	.read = tfa9890_read,
	.write = tfa9890_write,
	.probe = tfa9890_probe,
	.remove = tfa9890_remove,
	.readable_register = tfa9890_readable,
	.reg_cache_size = TFA9890_REG_CACHE_SIZE,
	.reg_cache_default = tfa9890_reg_defaults,
	.reg_word_size = 2,
};

static __devinit int tfa9890_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct tfa9890_pdata *pdata = i2c->dev.platform_data;
	struct tfa9890_priv *tfa9890;
	int ret;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	/* check platform data */
	if (i2c->dev.platform_data == NULL) {
		dev_err(&i2c->dev,
			"platform data is NULL\n");
		return -EINVAL;
	}

	tfa9890 = devm_kzalloc(&i2c->dev, sizeof(struct tfa9890_priv),
			       GFP_KERNEL);
	if (tfa9890 == NULL)
		return -ENOMEM;

	tfa9890->control_data = i2c;
	tfa9890->dsp_init = TFA9890_DSP_INIT_PENDING;
	i2c_set_clientdata(i2c, tfa9890);
	mutex_init(&tfa9890->dsp_init_lock);
	mutex_init(&tfa9890->i2c_rw_lock);

	/* enable regulator */
	tfa9890->vdd = regulator_get(&i2c->dev, "tfa_vdd");
	if (IS_ERR(tfa9890->vdd)) {
		pr_err("%s: Error getting vdd regulator.\n", __func__);
		ret = PTR_ERR(tfa9890->vdd);
		goto reg_get_fail;
	}

	regulator_set_voltage(tfa9890->vdd, 1800000, 1800000);

	ret = regulator_enable(tfa9890->vdd);
	if (ret < 0) {
		pr_err("%s: Error enabling vdd regulator %d:", __func__, ret);
		goto reg_enable_fail;
	}

	/* register sysfs hooks */
	ret = sysfs_create_group(&i2c->dev.kobj, &tfa9890_attr_group);
	if (ret)
		pr_err("%s: Error registering tfa9890 sysfs\n", __func__);

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa9890->tfa9890_wq =
			create_singlethread_workqueue("tfa9890");
	if (tfa9890->tfa9890_wq == NULL) {
		ret = -ENOMEM;
		goto wq_fail;
	}

	INIT_WORK(&tfa9890->work, tfa9890_dsp_init);

	ret = gpio_request(pdata->reset_gpio, "tfa reset gpio");
	if (ret < 0) {
		pr_err("%s: tfa reset gpio_request failed: %d\n",
			__func__, ret);
		goto gpio_fail;
	}
	/* take IC out of reset, device registers are reset in codec probe
	 * through register write.
	 */
	gpio_direction_output(pdata->reset_gpio, 0);

	/* register codec */
	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_tfa9890, &tfa9890_dai, 1);
	if (ret < 0) {
		pr_err("%s: Error registering tfa9890 codec", __func__);
		goto codec_fail;
	}

	pr_info("tfa9890 probed successfully!");

	return ret;

codec_fail:
gpio_fail:
	destroy_workqueue(tfa9890->tfa9890_wq);
wq_fail:
	snd_soc_unregister_codec(&i2c->dev);
reg_enable_fail:
	regulator_disable(tfa9890->vdd);
	regulator_put(tfa9890->vdd);
reg_get_fail:

	return ret;
}

static __devexit int tfa9890_i2c_remove(struct i2c_client *client)
{
	struct tfa9890_priv *tfa9890 = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	regulator_disable(tfa9890->vdd);
	regulator_put(tfa9890->vdd);
	destroy_workqueue(tfa9890->tfa9890_wq);
	return 0;
}

static const struct i2c_device_id tfa9890_i2c_id[] = {
	{ "tfa9890", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa9890_i2c_id);

static struct i2c_driver tfa9890_i2c_driver = {
	.driver = {
		.name = "tfa9890",
		.owner = THIS_MODULE,
	},
	.probe =    tfa9890_i2c_probe,
	.remove =   __devexit_p(tfa9890_i2c_remove),
	.id_table = tfa9890_i2c_id,
};

static int __init tfa9890_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&tfa9890_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register tfa9890 I2C driver: %d\n",
		       ret);
	}
	return ret;
}
module_init(tfa9890_modinit);

static void __exit tfa9890_exit(void)
{
	i2c_del_driver(&tfa9890_i2c_driver);
}
module_exit(tfa9890_exit);

MODULE_DESCRIPTION("ASoC tfa9890 codec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility");
