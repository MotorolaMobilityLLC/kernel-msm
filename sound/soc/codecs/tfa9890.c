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

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
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
#define MTPB_RETRIES		5

#define TFA9890_RATES	SNDRV_PCM_RATE_8000_48000
#define TFA9890_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE)

#define FIRMWARE_NAME_SIZE     128
#define TFA9890_STATUS_UP_MASK	(TFA9890_STATUS_PLLS | \
					TFA9890_STATUS_CLKS | \
					TFA9890_STATUS_VDDS | \
					TFA9890_STATUS_ARFS)
#define SYS_CLK_DEFAULT 1536000
struct tfa9890_priv {
	struct i2c_client *control_data;
	struct regulator *vdd;
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa9890_wq;
	struct work_struct init_work;
	struct work_struct calib_work;
	struct work_struct mode_work;
	struct work_struct load_preset;
	struct delayed_work delay_work;
	struct mutex dsp_init_lock;
	struct mutex i2c_rw_lock;
	int dsp_init;
	int speaker_imp;
	int sysclk;
	int rst_gpio;
	int max_vol_steps;
	int mode;
	int mode_switched;
	int curr_mode;
	int cfg_mode;
	int vol_idx;
	int curr_vol_idx;
	int ic_version;
	char const *tfa_dev;
	char const *fw_path;
	char const *fw_name;
	int is_spkr_prot_en;
	int update_eq;
	int update_cfg;
};

static DEFINE_MUTEX(lr_lock);
static int stereo_mode;
static struct snd_soc_codec *left_codec;
static struct snd_soc_codec *right_codec;

static const struct tfa9890_regs tfa9890_reg_defaults[] = {
{
	.reg = TFA9890_BATT_CTL_REG,
	.value = 0x13A2,
},
{
	.reg = TFA9890_I2S_CTL_REG,
	.value = 0x889b,
},
{
	.reg = TFA9890_VOL_CTL_REG,
	.value = 0x002f,
},
{
	.reg = TFA9890_SPK_CTL_REG,
	.value = 0x3832,
},
{
	.reg = TFA9890_DC2DC_CTL_REG,
	.value = 0x8FE6,
},
{
	.reg = TFA9890_SYS_CTL1_REG,
	.value =  0x827D,
},
{
	.reg = TFA9890_SYS_CTL2_REG,
	.value = 0x38D2,
},
{
	.reg = TFA9890_PWM_CTL_REG,
	.value = 0x0308,
},
{
	.reg = TFA9890_CURRT_SNS1_REG,
	.value = 0x7be1,
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
{
	.reg = TFA9890_CURRT_SNS2_REG,
	.value = 0x340,
},
};

/* presets tables per volume step for different modes */

static char const *tfa9890_preset_tables[] = {
	"music_table.preset",
	"voice_table.preset",
	"ringtone_table.preset",
};

static char const *tfa9890_config_tables[] = {
	"music.config",
	"voice.config",
	"ringtone.config",
};

static char const *tfa9890_eq_tables[] = {
	"music.eq",
	"voice.eq",
	"ringtone.eq",
};

static char const *tfa9890_mode[] = {
	"tfa9890_music",
	"tfa9890_voice",
	"tfa9890_ringtone",
};

static const struct firmware *left_fw_pst_table[ARRAY_SIZE(tfa9890_mode)];
static const struct firmware *right_fw_pst_table[ARRAY_SIZE(tfa9890_mode)];

/* firmware files for cfg and eq per mode*/
static const struct firmware *left_fw_config[ARRAY_SIZE(tfa9890_mode)];
static const struct firmware *right_fw_config[ARRAY_SIZE(tfa9890_mode)];
static const struct firmware *left_fw_eq[ARRAY_SIZE(tfa9890_mode)];
static const struct firmware *right_fw_eq[ARRAY_SIZE(tfa9890_mode)];

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
[TFA9890_LDO_REG] = 1,
[TFA9890_PWM_CTL_REG] = 1,
[TFA9890_CURRT_SNS1_REG] = 1,
[TFA9890_CURRT_SNS2_REG] = 1,
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
	u8 chunk_buf[TFA9890_MAX_I2C_SIZE + 1];
	int offset = 0;
	int ret = 0;
	/* first byte is mem address */
	int remaining_bytes = len - 1;
	int chunk_size = TFA9890_MAX_I2C_SIZE;

	chunk_buf[0] = reg & 0xff;
	mutex_lock(&tfa9890->i2c_rw_lock);
	while ((remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;

		memcpy(chunk_buf + 1, data + 1 + offset, chunk_size);
		ret = tfa9890_i2c_write(codec->control_data, chunk_buf,
					chunk_size + 1);
		offset = offset + chunk_size;
		remaining_bytes = remaining_bytes - chunk_size;
	}

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
	u8 buf[3] = {0, 0, 0};
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

static int tfa9890_bulk_read(struct snd_soc_codec *codec, u8 reg,
				u8 *data, int len)
{
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int offset = 0;
	int remaining_bytes = len;
	int chunk_size = TFA9890_MAX_I2C_SIZE;

	mutex_lock(&tfa9890->i2c_rw_lock);
	while ((remaining_bytes > 0)) {
		if (remaining_bytes < chunk_size)
			chunk_size = remaining_bytes;
		ret = tfa9890_i2c_read(codec->control_data, reg, data + offset,
				chunk_size);
		offset = offset + chunk_size;
		remaining_bytes = remaining_bytes - chunk_size;
	}
	mutex_unlock(&tfa9890->i2c_rw_lock);

	return ret;
}

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
			int len, int type, u8 *read)
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
		tfa9890_bulk_read(codec, TFA9890_CF_MEM,
				read, len);
	}

	return 0;
}

/*
 * ASOC controls
 */

/* 0dB min, 0.5dB steps */
static DECLARE_TLV_DB_SCALE(tlv_step_0_5, 0, 50, 0);

static int tfa9890_get_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa9890->curr_mode;
	ucontrol->value.integer.value[1] = tfa9890->vol_idx;

	return 0;
}

static int tfa9890_set_config_right(struct tfa9890_priv *tfa9890)
{
	int ret;

	if (tfa9890->dsp_init == TFA9890_DSP_INIT_FAIL) {
		pr_err("%s: firmware loading failed!!", __func__);
		return -EIO;
	}

	if (!right_fw_config[tfa9890->cfg_mode] ||
				((right_fw_config[tfa9890->cfg_mode])->size !=
				TFA9890_CFG_FW_SIZE)) {
			pr_err("tfa9890: Data size check failed cfg file\n");
			return -EIO;
	}

	ret = tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_SET_CONFIG,
				right_fw_config[tfa9890->cfg_mode]->data,
				right_fw_config[tfa9890->cfg_mode]->size,
				TFA9890_DSP_WRITE, 0);
	if (ret < 0) {
		pr_err("tfa9890: Failed to load cfg for mode %d\n",
			tfa9890->cfg_mode);
		return ret;
	}

	tfa9890->update_cfg = 0;
	return ret;

}

static int tfa9890_set_config_left(struct tfa9890_priv *tfa9890)
{
	int ret;
	if (tfa9890->dsp_init == TFA9890_DSP_INIT_FAIL) {
		pr_err("%s: firmware loading failed!!", __func__);
		return -EIO;
	}

	if (!left_fw_config[tfa9890->cfg_mode] ||
				((left_fw_config[tfa9890->cfg_mode])->size !=
				TFA9890_CFG_FW_SIZE)) {
			pr_err("tfa9890: Data size check failed cfg file\n");
			return -EIO;
	}

	ret = tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_SET_CONFIG,
				left_fw_config[tfa9890->cfg_mode]->data,
				left_fw_config[tfa9890->cfg_mode]->size,
				TFA9890_DSP_WRITE, 0);
	if (ret < 0) {
		pr_err("tfa9890: Failed to load cfg for mode %d\n",
			tfa9890->cfg_mode);
		return ret;
	}
	tfa9890->update_cfg = 0;
	return ret;
}


static int tfa9890_set_mode_right(struct tfa9890_priv *tfa9890)
{
	int ret;

	if (tfa9890->dsp_init == TFA9890_DSP_INIT_FAIL) {
		pr_err("tfa9890: firmware loading failed!!");
		return -EIO;
	}

	if (!right_fw_pst_table[tfa9890->mode] ||
			(right_fw_pst_table[tfa9890->mode])->size !=
			tfa9890->max_vol_steps*TFA9890_PST_FW_SIZE) {
		pr_err("tfa9890: Data size check failed preset file");
		return -EIO;
	}

	pr_debug("tfa9890: switching to mode: %s vol idx: %d",
			tfa9890_mode[tfa9890->mode], tfa9890->vol_idx);

	ret = tfa9890_dsp_transfer(tfa9890->codec, TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_SET_PRESET,
			(right_fw_pst_table[tfa9890->mode])->data +
			((TFA9890_PST_FW_SIZE)*
				(tfa9890->max_vol_steps - tfa9890->vol_idx)),
			TFA9890_PST_FW_SIZE, TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		return ret;
	tfa9890->mode_switched = 0;
	tfa9890->curr_mode = tfa9890->mode;
	tfa9890->curr_vol_idx = tfa9890->vol_idx;

	if (tfa9890->update_eq) {
		if (!right_fw_eq[tfa9890->mode] ||
			((right_fw_eq[tfa9890->mode])->size !=
				TFA9890_COEFF_FW_SIZE)) {
			pr_err("tfa9890: Data size check failed eq file\n");
			return -EIO;
		}

		ret = tfa9890_dsp_transfer(tfa9890->codec,
			TFA9890_DSP_MOD_BIQUADFILTERBANK,
			0, right_fw_eq[tfa9890->mode]->data,
			right_fw_eq[tfa9890->mode]->size,
			TFA9890_DSP_WRITE, 0);
		if (ret < 0) {
			pr_err("tfa9890: Failed to load eq for mode %s\n",
				tfa9890_mode[tfa9890->mode]);
			return ret;
		}
		pr_debug("tfa9890: switching eq file to mode %s\n",
			tfa9890_mode[tfa9890->mode]);
	}

	tfa9890->update_eq = 0;
	return ret;
}

static int tfa9890_set_mode_left(struct tfa9890_priv *tfa9890)
{
	int ret;

	if (tfa9890->dsp_init == TFA9890_DSP_INIT_FAIL) {
		pr_err("tfa9890: firmware loading failed!!");
		return -EIO;
	}

	if (!left_fw_pst_table[tfa9890->mode] ||
			(left_fw_pst_table[tfa9890->mode])->size !=
			tfa9890->max_vol_steps*TFA9890_PST_FW_SIZE) {
		pr_err("tfa9890: Data size check failed preset file");
		return -EIO;
	}

	pr_debug("tfa9890: switching to mode: %s vol idx: %d",
			tfa9890_mode[tfa9890->mode], tfa9890->vol_idx);

	ret = tfa9890_dsp_transfer(tfa9890->codec, TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_SET_PRESET,
			(left_fw_pst_table[tfa9890->mode])->data +
			((TFA9890_PST_FW_SIZE)*
				(tfa9890->max_vol_steps - tfa9890->vol_idx)),
			TFA9890_PST_FW_SIZE, TFA9890_DSP_WRITE, 0);
	if (ret < 0)
		return ret;
	tfa9890->mode_switched = 0;
	tfa9890->curr_mode = tfa9890->mode;
	tfa9890->curr_vol_idx = tfa9890->vol_idx;

	if (tfa9890->update_eq) {
		if (!left_fw_eq[tfa9890->mode] ||
			((left_fw_eq[tfa9890->mode])->size
			!= TFA9890_COEFF_FW_SIZE)) {
			pr_err("tfa9890: Data size check failed eq file\n");
			return -EIO;
		}

		ret = tfa9890_dsp_transfer(tfa9890->codec,
			TFA9890_DSP_MOD_BIQUADFILTERBANK,
			0, left_fw_eq[tfa9890->mode]->data,
			left_fw_eq[tfa9890->mode]->size,
			TFA9890_DSP_WRITE, 0);
		if (ret < 0) {
			pr_err("tfa9890: Failed to load eq for mode %s\n",
				tfa9890_mode[tfa9890->mode]);
			return ret;
		}
		pr_debug("tfa9890: switching eq file to mode %s\n",
			tfa9890_mode[tfa9890->mode]);
	}

	tfa9890->update_eq = 0;
	return ret;
}

static int tfa9890_put_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u16 val;
	int mode_value = ucontrol->value.integer.value[0];
	int vol_value = ucontrol->value.integer.value[1];

	mutex_lock(&lr_lock);
	mutex_lock(&tfa9890->dsp_init_lock);
	/* check for boundary conditions */
	if ((vol_value > tfa9890->max_vol_steps || vol_value < 0) ||
			(mode_value > (ARRAY_SIZE(tfa9890_mode) - 1) ||
			mode_value < 0)) {
		pr_err("%s: invalid mode or vol index set", __func__);
		mutex_unlock(&tfa9890->dsp_init_lock);
		mutex_unlock(&lr_lock);
		return -EINVAL;
	}

	if (vol_value == 0)
		/* increment vol value to 1 for index 0, vol index 0 is not
		 * treated as mute for all streams in android, use
		 * nxp preset 1 for vol index 0.
		 */
		vol_value++;

	/* update config/eq based on mode change */
	if (tfa9890->curr_mode != mode_value)
		tfa9890->update_eq = 1;

	if (tfa9890->curr_mode != mode_value ||
			tfa9890->curr_vol_idx != vol_value) {
		tfa9890->mode_switched = 1;
		tfa9890->mode = mode_value;
		tfa9890->vol_idx = vol_value;
		val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);
		/* audio session active switch the preset realtime */
		if ((val & TFA9890_STATUS_UP_MASK) == TFA9890_STATUS_UP_MASK) {
			if (!strncmp("left", tfa9890->tfa_dev, 4))
				tfa9890_set_mode_left(tfa9890);
			else
				tfa9890_set_mode_right(tfa9890);
		}
	}
	mutex_unlock(&tfa9890->dsp_init_lock);
	mutex_unlock(&lr_lock);
	return 1;
}


static int tfa9890_dsp_bypass_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u16 val;

	mutex_lock(&lr_lock);
	if (ucontrol->value.integer.value[0]) {
		/* Set CHSA to bypass DSP */
		val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
		val &= ~(TFA9890_I2SREG_CHSA_MSK);
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);

		/* Set DCDC compensation to off and set impedance as 8ohm */
		val = snd_soc_read(codec, TFA9890_SYS_CTL2_REG);
		val &= ~(TFA9890_SYS_CTRL2_REG_DCFG_MSK);
		val |= TFA9890_SYS_CTRL2_REG_SPKR_MSK;
		snd_soc_write(codec, TFA9890_SYS_CTL2_REG, val);

		/* Set DCDC to follower mode and disable coolflux  */
		val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
		val &= ~(TFA9890_SYS_CTRL_DCA_MSK);
		val &= ~(TFA9890_SYS_CTRL_CFE_MSK);
		snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);

		/* Bypass battery safeguard */
		val = snd_soc_read(codec, TFA9890_BATT_CTL_REG);
		val |= TFA9890_BAT_CTL_BSSBY_MSK;
		snd_soc_write(codec, TFA9890_BATT_CTL_REG, val);

		tfa9890->is_spkr_prot_en = 1;
		pr_debug("%s: codec dsp bypassed %d\n", codec->name,
			tfa9890->is_spkr_prot_en);
	} else {
		/* Set CHSA to enable DSP */
		val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
		val |= (TFA9890_I2SREG_CHSA_VAL);
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);

		/* Set DCDC compensation to default 100% and
		 * Set impedance to be defined by DSP
		 */
		val = snd_soc_read(codec, TFA9890_SYS_CTL2_REG);
		val |= (TFA9890_SYS_CTRL2_REG_DCFG_VAL);
		val &= ~TFA9890_SYS_CTRL2_REG_SPKR_MSK;
		snd_soc_write(codec, TFA9890_SYS_CTL2_REG, val);

		/* Set DCDC to active mode and enable Coolflux */
		val = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
		val |= (TFA9890_SYS_CTRL_DCA_MSK);
		val |= (TFA9890_SYS_CTRL_CFE_MSK);
		snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);

		/* Enable battery safeguard */
		val = snd_soc_read(codec, TFA9890_BATT_CTL_REG);
		val &= ~(TFA9890_BAT_CTL_BSSBY_MSK);
		snd_soc_write(codec, TFA9890_BATT_CTL_REG, val);

		tfa9890->is_spkr_prot_en = 0;
		pr_debug("%s: codec dsp unbypassed %d\n", codec->name,
			tfa9890->is_spkr_prot_en);
	}
	mutex_unlock(&lr_lock);
	return 1;
}

static int tfa9890_dsp_bypass_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa9890->is_spkr_prot_en;

	return 0;
}

static int tfa9890_put_ch_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u16 val;
	u16 ch_sel;

	mutex_lock(&lr_lock);
	pr_debug("%s: value %ld\n", __func__, ucontrol->value.integer.value[0]);

	val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
	ch_sel = (TFA9890_I2S_CHS12 & val) >> 3;

	if (ucontrol->value.integer.value[0] != 0x3)
		tfa9890->cfg_mode = 0;
	else
		tfa9890->cfg_mode = 1;

	/* check if config file need to switched */
	if (ch_sel != ucontrol->value.integer.value[0]) {
		val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_UP_MASK) == TFA9890_STATUS_UP_MASK) {
			if (!strncmp("left", tfa9890->tfa_dev, 4))
				tfa9890_set_config_left(tfa9890);
			else
				tfa9890_set_config_right(tfa9890);
		} else
			tfa9890->update_cfg = 1;

		val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
		val = val & (~TFA9890_I2S_CHS12);
		val = val | (ucontrol->value.integer.value[0] << 3);
		snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);
	}
	mutex_unlock(&lr_lock);
	return 0;
}

static int tfa9890_get_ch_sel(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u16 ch_sel;
	u16 val;

	mutex_lock(&lr_lock);
	val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
	ch_sel = TFA9890_I2S_CHS12 & val;
	ucontrol->value.integer.value[0] = ch_sel;
	mutex_unlock(&lr_lock);
	return 0;
}

static const struct soc_enum tfa9890_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, tfa9890_mode),
};

static const struct snd_kcontrol_new tfa9890_left_snd_controls[] = {
	SOC_SINGLE_TLV("BOOST VolumeL", TFA9890_VOL_CTL_REG,
			8, 0xff, 0, tlv_step_0_5),
	SOC_SINGLE_MULTI_EXT("BOOST ModeL", SND_SOC_NOPM, 0, 255,
				 0, 2, tfa9890_get_mode,
				 tfa9890_put_mode),
	/* val 1 for left channel, 2 for right and 3 for (l+r)/2 */
	SOC_SINGLE_EXT("BOOST Left Ch Select", TFA9890_I2S_CTL_REG,
			3, 0x3, 0, tfa9890_get_ch_sel,
					tfa9890_put_ch_sel),
	SOC_SINGLE_EXT("BOOST ENABLE Spkr Left Prot", 0 , 0, 1,
				 0, tfa9890_dsp_bypass_get,
					tfa9890_dsp_bypass_put),
};

/* bit 6,7 : 01 - DSP bypassed, 10 - DSP used */
static const struct snd_kcontrol_new tfa9890_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("DSP Bypass Left", TFA9890_I2S_CTL_REG, 6, 3, 0),
};

static const struct snd_soc_dapm_widget tfa9890_left_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("I2S1L"),
	SND_SOC_DAPM_MIXER("BOOST Output Mixer Left", SND_SOC_NOPM, 0, 0,
			   &tfa9890_left_mixer_controls[0],
			   ARRAY_SIZE(tfa9890_left_mixer_controls)),
	SND_SOC_DAPM_OUTPUT("BOOST Speaker Left"),
};

static const struct snd_soc_dapm_route tfa9890_left_dapm_routes[] = {
	{"BOOST Output Mixer Left", "DSP Bypass Left", "I2S1L"},
	{"BOOST Speaker Left", "Null", "BOOST Output Mixer Left"},
};

static const struct snd_kcontrol_new tfa9890_right_snd_controls[] = {
	SOC_SINGLE_TLV("BOOST VolumeR", TFA9890_VOL_CTL_REG,
			8, 0xff, 0, tlv_step_0_5),
	SOC_SINGLE_MULTI_EXT("BOOST ModeR", SND_SOC_NOPM, 0, 255,
				 0, 2, tfa9890_get_mode,
				 tfa9890_put_mode),
	/* val 1 for left channel, 2 for right and 3 for (l+r)/2 */
	SOC_SINGLE_EXT("BOOST Right Ch Select", TFA9890_I2S_CTL_REG,
			3, 0x3, 0, tfa9890_get_ch_sel,
						tfa9890_put_ch_sel),
	SOC_SINGLE_EXT("BOOST ENABLE Spkr Right Prot", 0, 0, 1,
				 0, tfa9890_dsp_bypass_get,
					tfa9890_dsp_bypass_put),
};

/* bit 6,7 : 01 - DSP bypassed, 10 - DSP used */
static const struct snd_kcontrol_new tfa9890_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("DSP Bypass Right", TFA9890_I2S_CTL_REG, 6, 3, 0),
};

static const struct snd_soc_dapm_widget tfa9890_right_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("I2S1R"),
	SND_SOC_DAPM_MIXER("BOOST Output Mixer Right", SND_SOC_NOPM, 0, 0,
			   &tfa9890_right_mixer_controls[0],
			   ARRAY_SIZE(tfa9890_right_mixer_controls)),
	SND_SOC_DAPM_OUTPUT("BOOST Speaker Right"),
};

static const struct snd_soc_dapm_route tfa9890_right_dapm_routes[] = {
	{"BOOST Output Mixer Right", "DSP Bypass Right", "I2S1R"},
	{"BOOST Speaker Right", "Null", "BOOST Output Mixer Right"},
};

/*
 * DSP Read/Write/Setup Functions
 */
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
	if (on)
		val = val & ~(TFA9890_POWER_DOWN);
	else
		val = val | (TFA9890_POWER_DOWN);

	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);
}

static void tfa9890_set_mute(struct snd_soc_codec *codec, int mute_state)
{
	u16 mute_val;
	u16 amp_en;

	/* read volume ctrl register */
	mute_val = snd_soc_read(codec, TFA9890_VOL_CTL_REG);
	/* read sys ctrl register */
	amp_en = snd_soc_read(codec, TFA9890_SYS_CTL1_REG);

	switch (mute_state) {
	case TFA9890_MUTE_OFF:
		/* unmute and enable amp */
		mute_val = mute_val & ~(TFA9890_STATUS_MUTE);
		amp_en = amp_en | (TFA9890_STATUS_AMPE);
		break;
	case TFA9890_DIGITAL_MUTE:
		/* mute amp and enable amp */
		mute_val = mute_val | (TFA9890_STATUS_MUTE);
		amp_en = amp_en | (TFA9890_STATUS_AMPE);
		break;
	case TFA9890_AMP_MUTE:
		/* turn off amp */
		mute_val = mute_val & ~(TFA9890_STATUS_MUTE);
		amp_en = amp_en & ~(TFA9890_STATUS_AMPE);
		break;
	default:
		break;
	}
	/* update register settings */
	snd_soc_write(codec, TFA9890_VOL_CTL_REG, mute_val);
	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, amp_en);
}

static int tfa9890_read_spkr_imp(struct tfa9890_priv *tfa9890)
{
	int imp;
	u8 buf[3] = {0, 0, 0};
	int ret;

	ret = tfa9890_dsp_transfer(tfa9890->codec,
			TFA9890_DSP_MOD_SPEAKERBOOST,
			TFA9890_PARAM_GET_RE0, 0, ARRAY_SIZE(buf),
			TFA9890_DSP_READ, buf);
	if (ret == 0) {
		imp = (buf[0] << 16 | buf[1] << 8 | buf[2]);
		imp = imp/(1 << (23 - TFA9890_SPKR_IMP_EXP));
	} else
		imp = 0;

	return imp;
}

static int tfa9890_get_ic_ver(struct snd_soc_codec *codec)
{
	u16 ver1;
	u16 ver2;
	u16 ver3;
	int ret = TFA9890_N1C2;

	/* read all the three IC version registers to determine tfa9890 type */
	ver1 = snd_soc_read(codec, 0x8b) & TFA9890_N1B12_VER1_MASK;
	ver2 = snd_soc_read(codec, 0x8c);
	ver3 = snd_soc_read(codec, 0x8d) & TFA9890_N1B12_VER3_MASK;

	if ((ver1 == TFA9890_N1B12_VER1_VAL) &&
			(ver2 == TFA9890_N1B12_VER2_VAL) &&
			(ver3 == TFA9890_N1B12_VER3_VAL)) {
		pr_debug("tfa9890: n1b12 version detected\n");
		ret = TFA9890_N1B12;
	} else if (!ver1 && !ver2 && !ver3) {
		pr_debug("tfa9890: n1b4 version detected\n");
		ret = TFA9890_N1B4;
	} else
		/* if it not above types then it must be N1C2 ver type */
		pr_debug("tfa9890: n1c2 version detected\n");

	return ret;
}

static int tfa9890_wait_pll_sync(struct tfa9890_priv *tfa9890)
{
	int ret = 0;
	int tries = 0;
	u16 val;
	struct snd_soc_codec *codec = tfa9890->codec;

	/* check if DSP pll is synced */
	do {
		val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_UP_MASK) == TFA9890_STATUS_UP_MASK)
			break;
		msleep_interruptible(1);
	} while ((++tries < PLL_SYNC_RETRIES));

	if (tries == PLL_SYNC_RETRIES) {
		pr_err("tfa9890:DSP pll sync failed!! %s", codec->name);
		ret = -EIO;
	}

	return ret;
}

static int tfa9887_load_dsp_patch(struct snd_soc_codec *codec, const u8 *fw,
				int fw_len)
{
	int index = 0;
	int length;
	int size = 0;
	const u8 *fw_data;
	int err = -EINVAL;

	length = fw_len - TFA9890_PATCH_HEADER;
	fw_data = fw + TFA9890_PATCH_HEADER;

	/* process the firmware */
	while (index < length) {
		/* extract length from first two bytes*/
		size = *(fw_data + index) + (*(fw_data + index + 1)) * 256;
		index += 2;
		if ((index + size) > length) {
			/* outside the buffer, error in the input data */
			pr_err("tfa9890: invalid length");
			goto out;
		}
		if ((size) > TFA9890_MAX_I2C_SIZE) {
			/* too big */
			pr_err("tfa9890: ivalid dsp patch");
			goto out;
		}
		err = codec->bulk_write_raw(codec, *(fw_data + index),
				(fw_data + index), size);
		if (err < 0) {
			pr_err("tfa9890: writing dsp patch failed");
			goto out;
		}
		index += size;
	}
	err = 0;

out:
	return err;
}

static int tfa9890_load_config(struct tfa9890_priv *tfa9890)
{
	struct snd_soc_codec *codec = tfa9890->codec;
	int ret = -EIO;
	const struct firmware *fw_speaker = NULL;
	const struct firmware *fw_config = NULL;
	const struct firmware *fw_coeff = NULL;
	const struct firmware *fw_patch = NULL;
	u16 val;
	char *fw_name;

	fw_name = kzalloc(FIRMWARE_NAME_SIZE, GFP_KERNEL);
	if (!fw_name) {
		pr_err("tfa9890: Failed to allocate fw_name\n");
		goto out;
	}

	/* Load DSP config and firmware files */

	/* check IC version to get correct firmware */
	tfa9890->ic_version = tfa9890_get_ic_ver(codec);
	if (tfa9890->ic_version == TFA9890_N1C2) {
		scnprintf(fw_name, FIRMWARE_NAME_SIZE,
				"%s/%s.%s_n1c2.patch",
				tfa9890->fw_path, tfa9890->tfa_dev,
				tfa9890->fw_name);
		ret = request_firmware(&fw_patch, fw_name, codec->dev);
		if (ret) {
			pr_err("tfa9890: Failed to locate tfa9890_n1c2.patch");
			goto out;
		}
	} else if (tfa9890->ic_version == TFA9890_N1B12) {
		scnprintf(fw_name, FIRMWARE_NAME_SIZE,
				"%s/%s.%s_n1b12.patch",
				tfa9890->fw_path, tfa9890->tfa_dev,
				tfa9890->fw_name);
		ret = request_firmware(&fw_patch, fw_name, codec->dev);
		if (ret) {
			pr_err("tfa9890: Failed to locate tfa9890_n1b12.patch");
			goto out;
		}
	} /* else nothing to do for n1b4's */

	if (fw_patch) {
		ret = tfa9887_load_dsp_patch(codec, fw_patch->data,
				fw_patch->size);
		if (ret) {
			pr_err("tfa9890: Failed to load dsp patch!!");
			goto out;
		}
	}

	scnprintf(fw_name, FIRMWARE_NAME_SIZE, "%s/%s.%s.speaker",
			tfa9890->fw_path, tfa9890->tfa_dev, tfa9890->fw_name);
	ret = request_firmware(&fw_speaker, fw_name, codec->dev);
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

	if (!strncmp("left", tfa9890->tfa_dev, 4)) {
		ret = tfa9890_set_mode_left(tfa9890);
		if (ret < 0)
			goto out;
		ret = tfa9890_set_config_left(tfa9890);
		if (ret < 0)
			goto out;
	} else {
		ret = tfa9890_set_mode_right(tfa9890);
		if (ret < 0)
			goto out;
		ret = tfa9890_set_config_right(tfa9890);
		if (ret < 0)
			goto out;
	}

	/* set all dsp config loaded */
	val = (u16)snd_soc_read(codec, TFA9890_SYS_CTL1_REG);
	val = val | TFA9890_SYSCTRL_CONFIGURED;
	snd_soc_write(codec, TFA9890_SYS_CTL1_REG, val);

	ret = 0;

out:
	kfree(fw_name);
	/* release firmware */
	release_firmware(fw_speaker);
	release_firmware(fw_coeff);
	release_firmware(fw_config);
	release_firmware(fw_patch);
	return ret;
}

static void tfa9890_calibaration(struct tfa9890_priv *tfa9890)
{
	u16 val;
	struct snd_soc_codec *codec = tfa9890->codec;

	/* Ensure no audio playback while calibarating but leave
	 * amp enabled*/
	tfa9890_set_mute(codec, TFA9890_DIGITAL_MUTE);

	/* unlock write access MTP memory*/
	snd_soc_write(codec, TFA9890_MTP_KEY_REG, TFA9890_MTK_KEY);

	val = snd_soc_read(codec, TFA9890_MTP_REG);
	/* set MTPOTC = 1 & MTPEX = 0*/
	val = val | TFA9890_MTPOTC;
	val = val & (~(TFA9890_STATUS_MTPEX));
	snd_soc_write(codec, TFA9890_MTP_REG, val);

	/* set CIMTB to initiate copy of calib values */
	val = snd_soc_read(codec, TFA9890_MTP_COPY_REG);
	val = val | TFA9890_STATUS_CIMTP;
	snd_soc_write(codec, TFA9890_MTP_COPY_REG, val);
}

static void tfa9890_work_read_imp(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv, calib_work);
	u16 val;

	mutex_lock(&lr_lock);
	val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
	if ((val & TFA9890_STATUS_PLLS) && (val & TFA9890_STATUS_CLKS))
		tfa9890->speaker_imp = tfa9890_read_spkr_imp(tfa9890);
	mutex_unlock(&lr_lock);
}

static void tfa9890_work_mode(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv, mode_work);
	int ret;

	mutex_lock(&lr_lock);
	mutex_lock(&tfa9890->dsp_init_lock);
	/* check if DSP pll is synced, It should be sync'ed at this point */
	ret = tfa9890_wait_pll_sync(tfa9890);
	if (ret < 0)
		goto out;

	if (!strncmp("left", tfa9890->tfa_dev, 4)) {
		if (tfa9890->mode_switched)
			tfa9890_set_mode_left(tfa9890);
		if (tfa9890->update_cfg)
			tfa9890_set_config_left(tfa9890);
	} else {
		if (tfa9890->mode_switched)
			tfa9890_set_mode_right(tfa9890);
		if (tfa9890->update_cfg)
			tfa9890_set_config_right(tfa9890);
	}
out:
	mutex_unlock(&tfa9890->dsp_init_lock);
	mutex_unlock(&lr_lock);
}

static void tfa9890_load_preset(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv, load_preset);
	struct snd_soc_codec *codec = tfa9890->codec;
	int ret;
	int i;
	char *preset_name;
	char *cfg_name;
	char *eq_name;

	mutex_lock(&lr_lock);
	pr_info("%s: loading presets,eq and config files\n", codec->name);
	preset_name = kzalloc(FIRMWARE_NAME_SIZE, GFP_KERNEL);
	cfg_name = kzalloc(FIRMWARE_NAME_SIZE, GFP_KERNEL);
	eq_name = kzalloc(FIRMWARE_NAME_SIZE, GFP_KERNEL);

	if (!preset_name || !cfg_name || !eq_name) {
		tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
		pr_err("tfa9890 : Load preset allocation failure\n");
		if (preset_name)
			kfree(preset_name);
		if (cfg_name)
			kfree(cfg_name);
		if (eq_name)
			kfree(eq_name);
		mutex_unlock(&lr_lock);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(tfa9890_mode); i++) {
		scnprintf(preset_name, FIRMWARE_NAME_SIZE, "%s/%s.%s_%s",
				tfa9890->fw_path, tfa9890->tfa_dev,
				tfa9890->fw_name,
				tfa9890_preset_tables[i]);
		if (!strncmp("left", tfa9890->tfa_dev, 4)) {
			ret = request_firmware(&left_fw_pst_table[i],
					preset_name,
					codec->dev);
			if (ret || (left_fw_pst_table[i]->size !=
				tfa9890->max_vol_steps*TFA9890_PST_FW_SIZE)) {
				pr_err("%s: Failed to locate DSP preset table %s",
						codec->name, preset_name);
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		} else {
			ret = request_firmware(&right_fw_pst_table[i],
					preset_name,
					codec->dev);
			if (ret || (right_fw_pst_table[i]->size !=
				tfa9890->max_vol_steps*TFA9890_PST_FW_SIZE)) {
				pr_err("%s: Failed to locate DSP preset table %s",
						codec->name, preset_name);
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(tfa9890_mode); i++) {
		scnprintf(cfg_name, FIRMWARE_NAME_SIZE, "%s/%s.%s.%s",
			tfa9890->fw_path, tfa9890->tfa_dev, tfa9890->fw_name,
			tfa9890_config_tables[i]);
		if (!strncmp("left", tfa9890->tfa_dev, 4)) {
			ret = request_firmware(&left_fw_config[i],
					cfg_name, codec->dev);
			if (ret || left_fw_config[i]->size
				!= TFA9890_CFG_FW_SIZE) {
				pr_err("tfa9890: Failed to load dsp config!!");
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		} else {
			ret = request_firmware(&right_fw_config[i],
					cfg_name, codec->dev);
			if (ret || right_fw_config[i]->size !=
					TFA9890_CFG_FW_SIZE) {
				pr_err("tfa9890: Failed to load dsp config!!");
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		}

		pr_info("%s: looading cfg file %s\n", __func__, cfg_name);
	}

	for (i = 0; i < ARRAY_SIZE(tfa9890_mode); i++) {
		scnprintf(eq_name, FIRMWARE_NAME_SIZE, "%s/%s.%s.%s",
			tfa9890->fw_path, tfa9890->tfa_dev, tfa9890->fw_name,
			tfa9890_eq_tables[i]);
		if (!strncmp("left", tfa9890->tfa_dev, 4)) {
			ret = request_firmware(&left_fw_eq[i], eq_name,
					codec->dev);
			if (ret ||
				left_fw_eq[i]->size != TFA9890_COEFF_FW_SIZE) {
				pr_err("tfa9890: Failed to load eq!!");
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		} else {
			ret = request_firmware(&right_fw_eq[i],
						eq_name, codec->dev);
			if (ret || right_fw_eq[i]->size !=
						TFA9890_COEFF_FW_SIZE) {
				pr_err("tfa9890: Failed to load eq!!");
				tfa9890->dsp_init = TFA9890_DSP_INIT_FAIL;
			}
		}

		pr_info("%s: looading eq file %s\n", __func__, eq_name);
	}

	kfree(preset_name);
	kfree(cfg_name);
	kfree(eq_name);
	mutex_unlock(&lr_lock);
}

static void tfa9890_monitor(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv,
				delay_work.work);
	u16 val;

	mutex_lock(&lr_lock);
	mutex_lock(&tfa9890->dsp_init_lock);
	val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
	pr_debug("%s: status:0x%x dev:%s\n", __func__, val, tfa9890->tfa_dev);
	/* check IC status bits: cold start, amp switching, speaker error
	 * and DSP watch dog bit to re init */
	if (((TFA9890_STATUS_ACS & val) || (TFA9890_STATUS_WDS & val) ||
		(TFA9890_STATUS_SPKS & val) ||
		!(TFA9890_STATUS_AMP_SWS & val)) &&
		!(tfa9890->is_spkr_prot_en)) {
		tfa9890->dsp_init = TFA9890_DSP_INIT_PENDING;
		/* schedule init now if the clocks are up and stable */
		if ((val & TFA9890_STATUS_UP_MASK) == TFA9890_STATUS_UP_MASK)
			queue_work(tfa9890->tfa9890_wq, &tfa9890->init_work);
	} /* else just reschedule */

	queue_delayed_work(tfa9890->tfa9890_wq, &tfa9890->delay_work,
				5*HZ);
	mutex_unlock(&tfa9890->dsp_init_lock);
	mutex_unlock(&lr_lock);
}

static void tfa9890_dsp_init(struct work_struct *work)
{
	struct tfa9890_priv *tfa9890 =
			container_of(work, struct tfa9890_priv, init_work);
	struct snd_soc_codec *codec = tfa9890->codec;
	u16 val;
	int ret;

	mutex_lock(&lr_lock);
	mutex_lock(&tfa9890->dsp_init_lock);
	/* check if DSP pll is synced, It should be sync'ed at this point */
	ret = tfa9890_wait_pll_sync(tfa9890);
	if (ret < 0)
		goto out;

	val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);

	pr_info("tfa9890: Initializing DSP, status:0x%x", val);

	/* cold boot device before loading firmware and parameters */
	ret = tfa9890_coldboot(codec);
	if (ret < 0) {
		pr_err("tfa9890: cold boot failed!!");
		goto out;
	}

	ret = tfa9890_load_config(tfa9890);
	if (ret < 0) {
		pr_err("tfa9890: load dsp config failed!!");
		goto out;
	}

	val = snd_soc_read(codec, TFA9890_MTP_REG);
	/* check if calibaration completed, Calibaration is one time event.
	 * Will be done only once when device boots up for the first time.Once
	 * calibarated info is stored in non-volatile memory of the device.
	 * It will be part of the factory test to validate spkr imp.
	 * The MTPEX will be set to 1 always after calibration, on subsequent
	 * power down/up as well.
	 */
	if (!(val & TFA9890_STATUS_MTPEX)) {
		pr_info("tfa9890:Calib not completed initiating ..");
		tfa9890_calibaration(tfa9890);
	} else
		/* speaker impedence available to read */
		tfa9890->speaker_imp = tfa9890_read_spkr_imp(tfa9890);

	tfa9890->dsp_init = TFA9890_DSP_INIT_DONE;
	mutex_unlock(&tfa9890->dsp_init_lock);
	mutex_unlock(&lr_lock);
	return;
out:
	/* retry firmware load failure, according to NXP
	 * due to PLL instability firmware loading could corrupt
	 * DSP memory, putting the dsp in reset and adding additional
	 * delay should avoid this situation, but its safe retry loading
	 * all fimrware file again if we detect failure here.
	 */
	tfa9890->dsp_init = TFA9890_DSP_INIT_PENDING;
	mutex_unlock(&tfa9890->dsp_init_lock);
	mutex_unlock(&lr_lock);
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
	int bclk_ws_ratio;

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
		break;
	case 11025:
		val = val | TFA9890_SAMPLE_RATE_11k;
		break;
	case 12000:
		val = val | TFA9890_SAMPLE_RATE_12k;
		break;
	case 16000:
		val = val | TFA9890_SAMPLE_RATE_16k;
		break;
	case 22050:
		val = val | TFA9890_SAMPLE_RATE_22k;
		break;
	case 24000:
		val = val | TFA9890_SAMPLE_RATE_24k;
		break;
	case 32000:
		val = val | TFA9890_SAMPLE_RATE_32k;
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
	bclk_ws_ratio = tfa9890->sysclk/params_rate(params);
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
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(codec);
	u16 val;
	u16 tries = 0;

	if (mute)
		cancel_delayed_work_sync(&tfa9890->delay_work);

	mutex_lock(&tfa9890->dsp_init_lock);
	if (mute) {
		tfa9890_set_mute(codec, TFA9890_AMP_MUTE);
		do {
			/* need to wait for amp to stop switching, to minimize
			 * pop, else I2S clk is going away too soon interrupting
			 * the dsp from smothering the amp pop while turning it
			 * off, It shouldn't take more than 50 ms for the amp
			 * switching to stop.
			 */
			val = snd_soc_read(codec, TFA9890_SYS_STATUS_REG);
			if (!(val & TFA9890_STATUS_AMP_SWS))
				break;
			usleep_range(10000, 10001);
		} while ((++tries < 20));
	} else {
		if (tfa9890->dsp_init == TFA9890_DSP_INIT_PENDING) {
			/* if the initialzation is pending treat it as cold
			 * startcase, need to put the dsp in reset and and
			 * power it up after additional delay to make sure
			 * the pll is stable. once the init is done this step
			 * is not needed for warm start as the dsp firmware
			 * patch configures the PLL for stable startup.
			 */
			snd_soc_write(codec, TFA9890_CF_CONTROLS, 0x1);
			/* power up IC */
			tfa9890_power(codec, 1);
			tfa9890_wait_pll_sync(tfa9890);

			/* wait additional 3msec for PLL to be stable */
			usleep_range(3000, 3001);

			/* take DSP out of reset */
			snd_soc_write(codec, TFA9890_CF_CONTROLS, 0x0);
		}

		tfa9890_set_mute(codec, TFA9890_MUTE_OFF);
		/* start monitor thread to check IC status bit 5secs, and
		 * re-init IC to recover.
		 */
		queue_delayed_work(tfa9890->tfa9890_wq, &tfa9890->delay_work,
			HZ);
	}
	mutex_unlock(&tfa9890->dsp_init_lock);
	return 0;
}


static int tfa9890_startup(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9890_priv *tfa9890 = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: enter\n", __func__);
	/* power up IC here only on warm start, if the initialization
	 * is still pending the DSP will be put in reset and powered
	 * up ater firmware load in the mute function where clock is up.
	 */
	if (tfa9890->dsp_init == TFA9890_DSP_INIT_DONE)
		tfa9890_power(codec, 1);

	return 0;
}

static void tfa9890_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	pr_debug("%s: enter\n", __func__);
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
		/* if in bypass mode dont't do anything */
		if (tfa9890->is_spkr_prot_en)
			break;

		/* To initialize dsp all the I2S signals should be bought up,
		 * so that the DSP's internal PLL can sync up and memory becomes
		 * accessible. Trigger callback is called when pcm write starts,
		 * so this should be the place where DSP is initialized
		 */
		if (tfa9890->dsp_init == TFA9890_DSP_INIT_PENDING)
			queue_work(tfa9890->tfa9890_wq, &tfa9890->init_work);

		/* will need to read speaker impedence here if its not read yet
		 * to complete the calibartion process. This step will enable
		 * device to calibrate if its not calibrated/validated in the
		 * factory. When the factory process is in place speaker imp
		 * will be read from sysfs and validated.
		 */
		else if (tfa9890->dsp_init == TFA9890_DSP_INIT_DONE) {
			if ((tfa9890->mode_switched == 1) ||
					(tfa9890->update_cfg == 1))
				queue_work(tfa9890->tfa9890_wq,
					&tfa9890->mode_work);
			if (tfa9890->speaker_imp == 0)
				queue_work(tfa9890->tfa9890_wq,
					&tfa9890->calib_work);

		}
		/* else nothing to do */
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

/* Called from tfa9890 stereo codec stub driver to set mute
 * on the individual codecs, this is needed to smother pops
 * in stereo configuration.
*/

int tfa9890_stereo_sync_set_mute(int mute)
{
	struct tfa9890_priv *tfa9890_left = snd_soc_codec_get_drvdata(left_codec);
	struct tfa9890_priv *tfa9890_right = snd_soc_codec_get_drvdata(right_codec);
	u16 left_val;
	u16 right_val;
	u16 tries = 0;

	if (!left_codec || !right_codec) {
		pr_err("%s : codec instance variables not intialized\n",
				__func__);
		return 0;
	}

	if (mute) {
		cancel_delayed_work_sync(&tfa9890_left->delay_work);
		cancel_delayed_work_sync(&tfa9890_right->delay_work);
	}

	mutex_lock(&lr_lock);
	if (mute) {
		tfa9890_set_mute(left_codec, TFA9890_AMP_MUTE);
		tfa9890_set_mute(right_codec, TFA9890_AMP_MUTE);
		do {
			/* need to wait for amp to stop switching, to minimize
			 * pop, else I2S clk is going away too soon interrupting
			 * the dsp from smothering the amp pop while turning it
			 * off, It shouldn't take more than 50 ms for the amp
			 * switching to stop.
			 */
			left_val = snd_soc_read(left_codec,
							TFA9890_SYS_STATUS_REG);
			right_val = snd_soc_read(right_codec,
							TFA9890_SYS_STATUS_REG);
			if (!(left_val & TFA9890_STATUS_AMP_SWS) &&
					!(right_val & TFA9890_STATUS_AMP_SWS))
				break;
			usleep_range(10000, 10001);
		} while ((++tries < 20));
	}
	mutex_unlock(&lr_lock);
	return 0;
}

/*
 * SysFS support
 */

static ssize_t tfa9890_show_spkr_imp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tfa9890_priv *tfa9890 =
				i2c_get_clientdata(to_i2c_client(dev));
	u16 val;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS))
			/* if I2S CLKS are ON read from DSP mem, otherwise print
			 * stored value as DSP mem cannot be accessed.
			 */
			tfa9890->speaker_imp =
				tfa9890_read_spkr_imp(tfa9890);
	}
	return scnprintf(buf, PAGE_SIZE, "%u\n", tfa9890->speaker_imp);
}

static ssize_t tfa9890_show_dev_state_info(struct file *filp,
					struct kobject *kobj,
				  struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct tfa9890_priv *tfa9890 =
				i2c_get_clientdata(kobj_to_i2c_client(kobj));
	u16 val;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_GET_STATE,
				0, attr->size, TFA9890_DSP_READ, buf);

			return count;
		}
	}
	return 0;
}

static ssize_t tfa9890_show_spkr_imp_model(struct file *filp,
					struct kobject *kobj,
				  struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct tfa9890_priv *tfa9890 =
				i2c_get_clientdata(kobj_to_i2c_client(kobj));
	u16 val;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_GET_LSMODEL,
				0, attr->size, TFA9890_DSP_READ, buf);

			return count;
		}
	}
	return 0;
}

static ssize_t tfa9890_show_spkr_exc_model(struct file *filp,
					struct kobject *kobj,
				  struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct tfa9890_priv *tfa9890 =
				i2c_get_clientdata(kobj_to_i2c_client(kobj));
	u16 val;

	if (off >= attr->size)
		return 0;

	if (off + count > attr->size)
		count = attr->size - off;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_GET_LSMODELW,
				0, attr->size, TFA9890_DSP_READ, buf);

			return count;
		}
	}
	return 0;
}

static ssize_t tfa9890_show_config_preset(struct file *filp,
					struct kobject *kobj,
				  struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct tfa9890_priv *tfa9890 =
				i2c_get_clientdata(kobj_to_i2c_client(kobj));
	u16 val;

	if (off >= attr->size)
		return 0;

	if (off + count > (attr->size))
		count = (attr->size) - off;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			tfa9890_dsp_transfer(tfa9890->codec,
				TFA9890_DSP_MOD_SPEAKERBOOST,
				TFA9890_PARAM_GET_CFGPST,
				0, (attr->size), TFA9890_DSP_READ, buf);

			return count;
		}
	}
	return 0;
}

static ssize_t tfa9890_show_ic_temp(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tfa9890_priv *tfa9890 = i2c_get_clientdata(to_i2c_client(dev));
	u16 val;
	int temp = 0;

	if (tfa9890->codec) {
		val = snd_soc_read(tfa9890->codec, TFA9890_SYS_STATUS_REG);
		if ((val & TFA9890_STATUS_PLLS) &&
				(val & TFA9890_STATUS_CLKS)) {
			/* calibaration should take place when the IC temp is
			 * between 0 and 50C, factory test command will verify
			 * factory test command will verify the temp along
			 * with impdedence to pass the test.
			 */
			val = snd_soc_read(tfa9890->codec,
						TFA9890_TEMP_STATUS_REG);
			temp = val & TFA9890_STATUS_TEMP;
		}
	}
	return scnprintf(buf, PAGE_SIZE, "%u\n", temp);
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

static const struct bin_attribute tf9890_raw_bin_attr[] = {
{
	.attr = {.name = "device_state", .mode = S_IRUGO},
	.read = tfa9890_show_dev_state_info,
	.size = TFA9890_DEV_STATE_SIZE,
},
{
	.attr = {.name = "spkr_imp_model", .mode = S_IRUGO},
	.read = tfa9890_show_spkr_imp_model,
	.size = TFA9890_SPK_FW_SIZE - 1,
},
{
	.attr = {.name = "config_preset", .mode = S_IRUGO},
	.read = tfa9890_show_config_preset,
	.size = TFA9890_CFG_FW_SIZE + TFA9890_PST_FW_SIZE - 2,
},
{
	.attr = {.name = "spkr_exc_model", .mode = S_IRUGO},
	.read = tfa9890_show_spkr_exc_model,
	.size = TFA9890_SPK_EX_FW_SIZE - 1,
},
};

static DEVICE_ATTR(ic_temp, S_IRUGO,
		   tfa9890_show_ic_temp, NULL);

static DEVICE_ATTR(spkr_imp, S_IRUGO,
		   tfa9890_show_spkr_imp, NULL);

static DEVICE_ATTR(force_calib, S_IWUSR,
		   NULL, tfa9890_force_calibaration);

static struct attribute *tfa9890_attributes[] = {
	&dev_attr_spkr_imp.attr,
	&dev_attr_force_calib.attr,
	&dev_attr_ic_temp.attr,
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

static struct snd_soc_dai_driver tfa9890_left_dai = {
	.name = "tfa9890_codec_left",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = TFA9890_RATES,
		     .formats = TFA9890_FORMATS,},
	.ops = &tfa9890_ops,
	.symmetric_rates = 1,
};

static struct snd_soc_dai_driver tfa9890_right_dai = {
	.name = "tfa9890_codec_right",
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
	u16 val;

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
	/* add controls and widgets */
	if (!strncmp("left", tfa9890->tfa_dev, 4)) {
		snd_soc_add_codec_controls(codec, tfa9890_left_snd_controls,
			     ARRAY_SIZE(tfa9890_left_snd_controls));
		snd_soc_dapm_new_controls(&codec->dapm,
				tfa9890_left_dapm_widgets,
				ARRAY_SIZE(tfa9890_left_dapm_widgets));

		snd_soc_dapm_add_routes(&codec->dapm, tfa9890_left_dapm_routes,
				ARRAY_SIZE(tfa9890_left_dapm_routes));
	} else if (!strncmp("right", tfa9890->tfa_dev, 5)) {
		snd_soc_add_codec_controls(codec, tfa9890_right_snd_controls,
			     ARRAY_SIZE(tfa9890_right_snd_controls));
		snd_soc_dapm_new_controls(&codec->dapm,
				tfa9890_right_dapm_widgets,
				ARRAY_SIZE(tfa9890_right_dapm_widgets));
		snd_soc_dapm_add_routes(&codec->dapm, tfa9890_right_dapm_routes,
				ARRAY_SIZE(tfa9890_right_dapm_routes));
	}

	if (stereo_mode) {
		val = snd_soc_read(codec, TFA9890_I2S_CTL_REG);
		pr_info("%s : val 0x%x\n", tfa9890->tfa_dev, val);
		/* select right/left channel input for I2S and Gain*/
		if (!strncmp(tfa9890->tfa_dev, "left", 4)) {
			val = val & (~TFA9890_I2S_CHS12);
			val = val | TFA9890_I2S_LEFT_IN;
			snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);

			/* set datao right channel to send gain info */
			val = snd_soc_read(codec, TFA9890_SYS_CTL2_REG);
			val = val & (~TFA9890_DORS_DATAO);
			val = val | TFA9890_DORS_GAIN;
			snd_soc_write(codec, TFA9890_SYS_CTL2_REG, val);
			left_codec = codec;
		} else if (!strncmp(tfa9890->tfa_dev, "right", 5)) {
			val = val & (~TFA9890_I2S_CHS12);
			val = val | TFA9890_I2S_RIGHT_IN;
			val = val | TFA9890_GAIN_IN;
			snd_soc_write(codec, TFA9890_I2S_CTL_REG, val);

			/* set datao left channel to send gain info */
			val = snd_soc_read(codec, TFA9890_SYS_CTL2_REG);
			val = val & (~TFA9890_DOLS_DATAO);
			val = val | TFA9890_DOLS_GAIN;
			snd_soc_write(codec, TFA9890_SYS_CTL2_REG, val);
			right_codec = codec;
		}
	}
	snd_soc_dapm_new_widgets(&codec->dapm);
	snd_soc_dapm_sync(&codec->dapm);

	/* load preset tables */
	queue_work(tfa9890->tfa9890_wq, &tfa9890->load_preset);
	pr_info("%s codec registered", codec->name);
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

#ifdef CONFIG_OF
static struct tfa9890_pdata *
tfa9890_of_init(struct i2c_client *client)
{
	struct tfa9890_pdata *pdata;
	struct device_node *np = client->dev.of_node;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s : pdata allocation failure\n", __func__);
		return NULL;
	}
	if (of_property_read_string(np, "nxp,tfa9890_bin_path",
				&pdata->fw_path))
		pdata->fw_path = ".";

	of_property_read_u32(np, "nxp,tfa_max-vol-steps",
				&pdata->max_vol_steps);
	pdata->reset_gpio = of_get_gpio(np, 0);

	if (of_property_read_string(np, "nxp,tfa-dev", &pdata->tfa_dev))
		pdata->tfa_dev = "left";

	if (of_property_read_string(np, "nxp,tfa-firmware-part-name",
					&pdata->fw_name))
		pdata->fw_name = "tfa9890";

	return pdata;
}
#else
static inline struct tfa9890_pdata *
tfa9890_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int tfa9890_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct tfa9890_pdata *pdata;
	struct tfa9890_priv *tfa9890;
	int ret;
	int i;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	if (i2c->dev.of_node)
		pdata = tfa9890_of_init(i2c);
	else
		pdata = i2c->dev.platform_data;

	/* check platform data */
	if (pdata == NULL) {
		dev_err(&i2c->dev,
			"platform data is NULL\n");
		return -EINVAL;
	}

	tfa9890 = devm_kzalloc(&i2c->dev, sizeof(struct tfa9890_priv),
			       GFP_KERNEL);
	if (tfa9890 == NULL)
		return -ENOMEM;

	tfa9890->rst_gpio = pdata->reset_gpio;
	tfa9890->max_vol_steps = pdata->max_vol_steps;
	tfa9890->control_data = i2c;
	tfa9890->dsp_init = TFA9890_DSP_INIT_PENDING;
	tfa9890->vol_idx = pdata->max_vol_steps;
	tfa9890->curr_vol_idx = pdata->max_vol_steps;
	tfa9890->tfa_dev = pdata->tfa_dev;
	tfa9890->sysclk = SYS_CLK_DEFAULT;
	tfa9890->fw_path = pdata->fw_path;
	tfa9890->fw_name = pdata->fw_name;
	tfa9890->update_eq = 1;
	i2c_set_clientdata(i2c, tfa9890);
	mutex_init(&tfa9890->dsp_init_lock);
	mutex_init(&tfa9890->i2c_rw_lock);

	/* register sysfs hooks */
	ret = sysfs_create_group(&i2c->dev.kobj, &tfa9890_attr_group);
	if (ret)
		pr_err("%s: Error registering tfa9890 sysfs\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tf9890_raw_bin_attr); i++) {
		ret = sysfs_create_bin_file(&i2c->dev.kobj,
						&tf9890_raw_bin_attr[i]);
		if (ret)
			pr_warn("%s: Error creating tfa9890 sysfs bin attr\n",
				__func__);
	}

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa9890->tfa9890_wq =
			create_singlethread_workqueue("tfa9890");
	if (tfa9890->tfa9890_wq == NULL) {
		ret = -ENOMEM;
		goto wq_fail;
	}

	INIT_WORK(&tfa9890->init_work, tfa9890_dsp_init);
	INIT_WORK(&tfa9890->calib_work, tfa9890_work_read_imp);
	INIT_WORK(&tfa9890->mode_work, tfa9890_work_mode);
	INIT_WORK(&tfa9890->load_preset, tfa9890_load_preset);
	INIT_DELAYED_WORK(&tfa9890->delay_work, tfa9890_monitor);

	/* gpio is optional in devtree, if its not specified dont fail
	 * Its not specified for right IC if the gpio is same for both
	 * IC's.
	 */
	if (gpio_is_valid(tfa9890->rst_gpio)) {
		ret = gpio_request(tfa9890->rst_gpio, "tfa reset gpio");
		if (ret < 0) {
			pr_err("%s: tfa reset gpio_request failed: %d\n",
				__func__, ret);
			goto gpio_fail;
		}
		gpio_direction_output(tfa9890->rst_gpio, 0);
	}

	tfa9890->vdd = regulator_get(&i2c->dev, "tfa_vdd");
	if (IS_ERR(tfa9890->vdd)) {
		pr_err("%s: Error getting vdd regulator.\n", __func__);
		ret = PTR_ERR(tfa9890->vdd);
		goto reg_get_fail;
	}

	regulator_set_voltage(tfa9890->vdd, 1800000, 1800000);

	ret = regulator_enable(tfa9890->vdd);
	if (ret < 0) {
		pr_err("%s: Error enabling vdd regulator %d:",
			__func__, ret);
		goto reg_enable_fail;
	}

	if (!strncmp("left", tfa9890->tfa_dev, 4)) {
		/* register codec */
		ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_tfa9890,
				&tfa9890_left_dai, 1);
		if (ret < 0) {
			pr_err("%s: Error registering tfa9890 left codec",
				__func__);
			goto codec_fail;
		}
	} else if (!strncmp("right", tfa9890->tfa_dev, 5)) {
		/* register codec */
		ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_tfa9890,
				&tfa9890_right_dai, 1);
		if (ret < 0) {
			pr_err("%s: Error registering tfa9890 right codec",
				__func__);
			goto codec_fail;
		}
		stereo_mode = 1;
	}

	pr_info("tfa9890 %s probed successfully!", tfa9890->tfa_dev);

	return ret;

codec_fail:
	regulator_disable(tfa9890->vdd);
reg_enable_fail:
	regulator_put(tfa9890->vdd);
reg_get_fail:
	if (gpio_is_valid(tfa9890->rst_gpio))
		gpio_free(tfa9890->rst_gpio);
gpio_fail:
	destroy_workqueue(tfa9890->tfa9890_wq);
wq_fail:

	return ret;
}

static int tfa9890_i2c_remove(struct i2c_client *client)
{
	struct tfa9890_priv *tfa9890 = i2c_get_clientdata(client);

	snd_soc_unregister_codec(&client->dev);
	regulator_disable(tfa9890->vdd);
	if (gpio_is_valid(tfa9890->rst_gpio))
		gpio_free(tfa9890->rst_gpio);
	regulator_put(tfa9890->vdd);
	destroy_workqueue(tfa9890->tfa9890_wq);
	return 0;
}

static const struct i2c_device_id tfa9890_i2c_id[] = {
	{ "tfa9890", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa9890_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id tfa9890_match_tbl[] = {
	{ .compatible = "nxp,tfa9890" },
	{ },
};
MODULE_DEVICE_TABLE(of, aic3253_match_tbl);
#endif

static struct i2c_driver tfa9890_i2c_driver = {
	.driver = {
		.name = "tfa9890",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa9890_match_tbl),
	},
	.probe =    tfa9890_i2c_probe,
	.remove =   tfa9890_i2c_remove,
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
