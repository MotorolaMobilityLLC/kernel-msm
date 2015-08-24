/*
 * max98925.c -- ALSA SoC Stereo MAX98925 driver
 *
 * Copyright 2013 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/max98925.h>
#include "max98925.h"
#define SUPPORT_DEVICE_TREE
#ifdef SUPPORT_DEVICE_TREE
#include <linux/regulator/consumer.h>
#endif
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define USE_DSM_MISC_DEV

#undef MAXDSM_MAX_DEVICES
#undef DSM_ALWAYS_ON

#ifdef USE_DSM_MISC_DEV
extern int afe_dsm_param_ctrl(uint32_t dir, uint32_t size, uint8_t *payload);
static int param_array[384];
static uint8_t *payload = (uint8_t *)&param_array[100];
#endif

#define DSM_MAX_SIZE_RW          (284 * sizeof(int))
#define GPIO_PULL_UP             1
#define GPIO_PULL_DOWN           0
#define DEFAULT_SWITCH_NONEED    0

static DEFINE_MUTEX(dsm_lock);

static struct reg_default max98925_reg[] = {
	{ 0x00, 0x00 }, /* Battery Voltage Data */
	{ 0x01, 0x00 }, /* Boost Voltage Data */
	{ 0x02, 0x00 }, /* Live Status0 */
	{ 0x03, 0x00 }, /* Live Status1 */
	{ 0x04, 0x00 }, /* Live Status2 */
	{ 0x05, 0x00 }, /* State0 */
	{ 0x06, 0x00 }, /* State1 */
	{ 0x07, 0x00 }, /* State2 */
	{ 0x08, 0x00 }, /* Flag0 */
	{ 0x09, 0x00 }, /* Flag1 */
	{ 0x0A, 0x00 }, /* Flag2 */
	{ 0x0B, 0x00 }, /* IRQ Enable0 */
	{ 0x0C, 0x00 }, /* IRQ Enable1 */
	{ 0x0D, 0x00 }, /* IRQ Enable2 */
	{ 0x0E, 0x00 }, /* IRQ Clear0 */
	{ 0x0F, 0x00 }, /* IRQ Clear1 */
	{ 0x10, 0x00 }, /* IRQ Clear2 */
	{ 0x11, 0xC0 }, /* Map0 */
	{ 0x12, 0x00 }, /* Map1 */
	{ 0x13, 0x00 }, /* Map2 */
	{ 0x14, 0xF0 }, /* Map3 */
	{ 0x15, 0x00 }, /* Map4 */
	{ 0x16, 0xAB }, /* Map5 */
	{ 0x17, 0x89 }, /* Map6 */
	{ 0x18, 0x00 }, /* Map7 */
	{ 0x19, 0x00 }, /* Map8 */
	{ 0x1A, 0x06 }, /* DAI Clock Mode 1 */
	{ 0x1B, 0xC0 }, /* DAI Clock Mode 2 */
	{ 0x1C, 0x00 }, /* DAI Clock Divider Denominator MSBs */
	{ 0x1D, 0x00 }, /* DAI Clock Divider Denominator LSBs */
	{ 0x1E, 0xF0 }, /* DAI Clock Divider Numerator MSBs */
	{ 0x1F, 0x00 }, /* DAI Clock Divider Numerator LSBs */
	{ 0x20, 0x50 }, /* Format */
	{ 0x21, 0x00 }, /* TDM Slot Select */
	{ 0x22, 0x00 }, /* DOUT Configuration VMON */
	{ 0x23, 0x00 }, /* DOUT Configuration IMON */
	{ 0x24, 0x00 }, /* DOUT Configuration VBAT */
	{ 0x25, 0x00 }, /* DOUT Configuration VBST */
	{ 0x26, 0x00 }, /* DOUT Configuration FLAG */
	{ 0x27, 0xFF }, /* DOUT HiZ Configuration 1 */
	{ 0x28, 0xFF }, /* DOUT HiZ Configuration 2 */
	{ 0x29, 0xFF }, /* DOUT HiZ Configuration 3 */
	{ 0x2A, 0xFF }, /* DOUT HiZ Configuration 4 */
	{ 0x2B, 0x02 }, /* DOUT Drive Strength */
	{ 0x2C, 0x90 }, /* Filters */
	{ 0x2D, 0x00 }, /* Gain */
	{ 0x2E, 0x02 }, /* Gain Ramping */
	{ 0x2F, 0x00 }, /* Speaker Amplifier */
	{ 0x30, 0x0A }, /* Threshold */
	{ 0x31, 0x00 }, /* ALC Attack */
	{ 0x32, 0x80 }, /* ALC Atten and Release */
	{ 0x33, 0x00 }, /* ALC Infinite Hold Release */
	{ 0x34, 0x92 }, /* ALC Configuration */
	{ 0x35, 0x01 }, /* Boost Converter */
	{ 0x36, 0x00 }, /* Block Enable */
	{ 0x37, 0x00 }, /* Configuration */
	{ 0x38, 0x00 }, /* Global Enable */
	{ 0x3A, 0x00 }, /* Boost Limiter */
	{ 0xFF, 0x50 }, /* Revision ID */
};

static bool pa_enable = false;

static bool max98925_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98925_R000_VBAT_DATA:
	case MAX98925_R001_VBST_DATA:
	case MAX98925_R002_LIVE_STATUS0:
	case MAX98925_R003_LIVE_STATUS1:
	case MAX98925_R004_LIVE_STATUS2:
	case MAX98925_R005_STATE0:
	case MAX98925_R006_STATE1:
	case MAX98925_R007_STATE2:
	case MAX98925_R008_FLAG0:
	case MAX98925_R009_FLAG1:
	case MAX98925_R00A_FLAG2:
	case MAX98925_R0FF_VERSION:
		return true;
	default:
		return false;
	}
}

static bool max98925_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98925_R00E_IRQ_CLEAR0:
	case MAX98925_R00F_IRQ_CLEAR1:
	case MAX98925_R010_IRQ_CLEAR2:
	case MAX98925_R033_ALC_HOLD_RLS:
		return false;
	default:
		return true;
	}
};

#ifdef SUPPORT_DEVICE_TREE
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int max98925_regulator_config(struct i2c_client *i2c, bool pullup, bool on)
{
	struct regulator *vcc_i2c = NULL, *vdd = NULL;
	int rc = 0;
	#define VCC_I2C_MIN_UV	1800000
	#define VCC_I2C_MAX_UV	1800000
	#define I2C_LOAD_UA		300000

	pr_debug("%s: enter\n", __func__);

	vdd = regulator_get(&i2c->dev, "vdd");
	if (IS_ERR(vdd)) {
		pr_err("%s: regulator get failed rc=%d\n", __func__, rc);
		return PTR_ERR(vdd);
	}

	if (regulator_count_voltages(vdd) > 0) {
		rc = regulator_set_voltage(vdd, VCC_I2C_MIN_UV, VCC_I2C_MAX_UV);
		if (rc) {
			pr_err("%s: regulator set_vtg failed rc=%d\n", __func__, rc);
			goto error_set_vtg_vdd;
		}
	}

	if (pullup) {
		vcc_i2c = regulator_get(&i2c->dev, "vcc_i2c");
		if (IS_ERR(vcc_i2c)) {
			rc = PTR_ERR(vcc_i2c);
			pr_err("%s: regulator get failed rc=%d\n", __func__, rc);
			goto error_get_vtg_i2c;
		}

		if (regulator_count_voltages(vcc_i2c) > 0) {
			rc = regulator_set_voltage(vcc_i2c, VCC_I2C_MIN_UV, VCC_I2C_MAX_UV);
			if (rc) {
				pr_err("%s: regulator set_vtg failed rc=%d\n", __func__, rc);
				goto error_set_vtg_i2c;
			}
		}
	}

	if (on) {
		rc = reg_set_optimum_mode_check(vdd, I2C_LOAD_UA);
		if (rc < 0) {
			pr_err("%s: regulator vcc_i2c set_opt failed rc=%d\n", __func__, rc);
			goto error_reg_opt_vdd;
		}

		rc = regulator_enable(vdd);
		if (rc) {
			pr_err("%s: regulator vcc_i2c enable failed rc=%d\n", __func__, rc);
			goto error_reg_en_vdd;
		}

		if (vcc_i2c) {
			rc = reg_set_optimum_mode_check(vcc_i2c, I2C_LOAD_UA);
			if (rc < 0) {
				pr_err("%s: regulator vcc_i2c set_opt failed rc=%d\n", __func__, rc);
				goto error_reg_opt_i2c;
			}

			rc = regulator_enable(vcc_i2c);
			if (rc) {
				pr_err("%s: regulator vcc_i2c enable failed rc=%d\n", __func__, rc);
				goto error_reg_en_vcc_i2c;
			}
		}
	} else {
		if (vcc_i2c) {
			reg_set_optimum_mode_check(vcc_i2c, 0);
			regulator_disable(vcc_i2c);
		}

		reg_set_optimum_mode_check(vdd, 0);
		regulator_disable(vdd);
	}

	return rc;

	error_set_vtg_i2c:
		regulator_put(vcc_i2c);
	error_get_vtg_i2c:
		if (regulator_count_voltages(vcc_i2c) > 0)
			regulator_set_voltage(vcc_i2c, 0, VCC_I2C_MAX_UV);
	error_set_vtg_vdd:
		regulator_put(vdd);
	error_reg_en_vcc_i2c:
		reg_set_optimum_mode_check(vcc_i2c, 0);
	error_reg_opt_i2c:
		regulator_disable(vcc_i2c);
	error_reg_en_vdd:
		reg_set_optimum_mode_check(vdd, 0);
	error_reg_opt_vdd:
		regulator_disable(vdd);

	return rc;
}
#endif

void reg_dump(struct max98925_priv *max98925)
{
	int val_l;
	int val_r;
	int i, j;

	static const struct {
		int start;
		int count;
	} reg_table[] = {
		{ 0x02, 0x03 },
		{ 0x1A, 0x1F },
		{ 0x3A, 0x01 },
		{ 0x00, 0x00 }
	};

	i = 0;
	while (reg_table[i].count != 0) {
		for(j = 0; j < reg_table[i].count; j++) {
			int addr = j + reg_table[i].start;
			regmap_read(max98925->regmapL, addr, &val_l);
			regmap_read(max98925->regmapR, addr, &val_r);
			pr_debug("%s: reg 0x%02X, val_l 0x%02X, val_r 0x%02X\n",
					__func__, addr, val_l, val_r);
		}
		i++;
	}
}

#ifdef USE_DSM_MISC_DEV
static int maxdsm_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t maxdsm_read(struct file *filep, char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc;

	mutex_lock(&dsm_lock);

	afe_dsm_param_ctrl(0, 0, payload);

	if (count > DSM_MAX_SIZE_RW)
		count = DSM_MAX_SIZE_RW;

	rc = copy_to_user(buf, payload, count);
	if (rc != 0) {
		pr_err("%s: copy_to_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return 0;
	}

	mutex_unlock(&dsm_lock);

	return rc;
}

static ssize_t maxdsm_write(struct file *filep, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc;

	mutex_lock(&dsm_lock);

	if (count > DSM_MAX_SIZE_RW)
		count =  DSM_MAX_SIZE_RW;

	rc = copy_from_user(payload, buf, count);
	if (rc != 0) {
		pr_err("%s: copy_from_user failed - %d\n", __func__, rc);
		mutex_unlock(&dsm_lock);
		return rc;
	}

	/* set params from the algorithm to application */
	afe_dsm_param_ctrl(1, count, payload);
	mutex_unlock(&dsm_lock);

	return rc;
}

static const struct file_operations dsm_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= maxdsm_open,
	.release	= NULL,
	.read		= maxdsm_read,
	.write		= maxdsm_write,
	.mmap		= NULL,
	.poll		= NULL,
	.fasync		= NULL,
	.llseek		= NULL,
};

static struct miscdevice dsm_ctrl_miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "dsm_ctrl_dev",
	.fops =     &dsm_ctrl_fops
};

static int maxdsm_init(void)
{
    int result;

    result = misc_register(&dsm_ctrl_miscdev);

    return result;
}

static int maxdsm_deinit(void)
{
	int result;

	result = misc_deregister(&dsm_ctrl_miscdev);

	return result;
}
#endif

static const unsigned int max98925_spk_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	1, 31, TLV_DB_SCALE_ITEM(-600, 100, 0),
};

static int max98925_left_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	int data;

	regmap_read(max98925->regmapL, MAX98925_R02D_GAIN, &data);
	ucontrol->value.integer.value[0] =
			(data & M98925_SPK_GAIN_MASK) >> M98925_SPK_GAIN_SHIFT;

	pr_debug("%s: left_gain setting returned %d\n", __func__,
		(int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98925_left_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < ((1 << M98925_SPK_GAIN_WIDTH) - 1)) {
		regmap_update_bits(max98925->regmapL, MAX98925_R02D_GAIN,
			M98925_SPK_GAIN_MASK, sel << M98925_SPK_GAIN_SHIFT);

		max98925->left_gain = sel;

		pr_debug("%s: left_gain set to %d\n", __func__, sel);
	} else {
		pr_debug("%s: valid speaker gain settings: %d to %d\n",
			__func__, 0, ((1 << M98925_SPK_GAIN_WIDTH) - 1));
	}

	return 0;
}

static int max98925_right_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	int data;

	regmap_read(max98925->regmapR, MAX98925_R02D_GAIN, &data);
	ucontrol->value.integer.value[0] =
			(data & M98925_SPK_GAIN_MASK) >> M98925_SPK_GAIN_SHIFT;

	pr_debug("%s: right_gain setting returned %d\n", __func__,
		(int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98925_right_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (sel < ((1 << M98925_SPK_GAIN_WIDTH) - 1)) {
		regmap_update_bits(max98925->regmapR, MAX98925_R02D_GAIN,
			M98925_SPK_GAIN_MASK, sel << M98925_SPK_GAIN_SHIFT);

		max98925->right_gain = sel;
		pr_debug("%s: right_gain set to %d\n", __func__, sel);
	} else {
		pr_debug("%s: valid speaker gain settings: %d to %d\n",
			__func__, 0, ((1 << M98925_SPK_GAIN_WIDTH) - 1));
	}

	return 0;
}

static const char * max98925_amplifier_text[] = {"Off", "On"};

static const struct soc_enum max98925_amplifier_enum =
	SOC_ENUM_SINGLE(MAX98925_R038_GLOBAL_ENABLE, M98925_EN_SHIFT, 2,
			max98925_amplifier_text);

static int max98925_left_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
			(max98925->left_en & M98925_EN_MASK) >> M98925_EN_SHIFT;

	return 0;
}

static int max98925_left_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if ( DEFAULT_SWITCH_NONEED != max98925->switch_en_gpio && sel) {
		int nret = gpio_direction_output(max98925->switch_en_gpio,
			GPIO_PULL_DOWN);
		if ( nret )
			pr_err("%s: modify spk&recv to high failed nret = %d\n",
				__func__, nret);
	} else {
		pr_err("%s: spk&rcver switch gpio had pulled down", __func__);
	}

	max98925->left_en = sel << M98925_EN_SHIFT;

	pr_debug("%s: left speaker set to %d\n", __func__, sel);
	return 0;
}

static int max98925_right_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
			(max98925->right_en & M98925_EN_MASK) >> M98925_EN_SHIFT;

	return 0;
}

static int max98925_right_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	max98925->right_en = sel << M98925_EN_SHIFT;

	pr_debug("%s: right speaker set to %d\n", __func__, sel);
	return 0;
}

static int max98925_receiver_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);

	/*TBD: Read GPIO status here*/

	/*AND with GPIO value*/
	ucontrol->value.integer.value[0] =
			(max98925->left_en & M98925_EN_MASK) >> M98925_EN_SHIFT;

	return 0;
}

static int max98925_receiver_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	/*TBD: Enable GPIO to receiver mode here*/
	if( DEFAULT_SWITCH_NONEED != max98925->switch_en_gpio && sel){
		int nret = gpio_direction_output(max98925->switch_en_gpio,
			GPIO_PULL_UP);
		if( nret )
			pr_err("%s: modify spk&recv to high failed nret = %d\n",
				__func__,nret);
	}else{
		pr_err("%s: spk&rcver switch gpio had pulled up",__func__);
	}

	max98925->left_en = sel << M98925_EN_SHIFT;

	return 0;
}

static int max98925_reg_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	int data;

	regmap_read(max98925->regmapL, reg, &data);

	ucontrol->value.integer.value[0] = (data & mask) >> shift;

	return 0;
}

static int max98925_reg_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	regmap_update_bits(max98925->regmapL, reg, mask, sel << shift);
	regmap_update_bits(max98925->regmapR, reg, mask, sel << shift);

	pr_debug("%s: register 0x%02X, value 0x%02X\n", __func__, reg, sel);

	return 0;
}

static const struct soc_enum max98925_imon_enum =
	SOC_ENUM_SINGLE(MAX98925_R036_BLOCK_ENABLE, M98925_ADC_IMON_EN_SHIFT, 2,
			max98925_amplifier_text);

static int max98925_imon_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R036_BLOCK_ENABLE,
			M98925_ADC_IMON_EN_MASK, M98925_ADC_IMON_EN_SHIFT);
}

static int max98925_imon_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R036_BLOCK_ENABLE,
			M98925_ADC_IMON_EN_MASK, M98925_ADC_IMON_EN_SHIFT);
}

static const struct soc_enum max98925_vmon_enum =
	SOC_ENUM_SINGLE(MAX98925_R036_BLOCK_ENABLE, M98925_ADC_VMON_EN_SHIFT, 2,
			max98925_amplifier_text);


static int max98925_vmon_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R036_BLOCK_ENABLE,
			M98925_ADC_VMON_EN_MASK, M98925_ADC_VMON_EN_SHIFT);
}

static int max98925_vmon_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R036_BLOCK_ENABLE,
			M98925_ADC_VMON_EN_MASK, M98925_ADC_VMON_EN_SHIFT);
}

static const struct soc_enum max98925_classd_only_enum =
	SOC_ENUM_SINGLE(MAX98925_R02F_SPK_AMP, M98925_SPK_MODE_SHIFT, 2,
			max98925_amplifier_text);

static int max98925_classd_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R02F_SPK_AMP,
			M98925_SPK_MODE_MASK, M98925_SPK_MODE_SHIFT);
}

static int max98925_classd_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R02F_SPK_AMP,
			M98925_SPK_MODE_MASK, M98925_SPK_MODE_SHIFT);
}

static int max98925_spk_ramp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R02E_GAIN_RAMPING,
			M98925_SPK_RMP_EN_MASK, M98925_SPK_RMP_EN_SHIFT);
}

static int max98925_spk_ramp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R02E_GAIN_RAMPING,
			M98925_SPK_RMP_EN_MASK, M98925_SPK_RMP_EN_SHIFT);
}

static int max98925_spk_zcd_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R02E_GAIN_RAMPING,
			M98925_SPK_ZCD_EN_MASK, M98925_SPK_ZCD_EN_SHIFT);
}

static int max98925_spk_zcd_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R02E_GAIN_RAMPING,
			M98925_SPK_ZCD_EN_MASK, M98925_SPK_ZCD_EN_SHIFT);
}

static int max98925_alc_en_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R030_THRESHOLD,
			M98925_ALC_EN_MASK, M98925_ALC_EN_SHIFT);
}

static int max98925_alc_en_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R030_THRESHOLD,
			M98925_ALC_EN_MASK, M98925_ALC_EN_SHIFT);
}

static int max98925_alc_threshold_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R030_THRESHOLD,
			M98925_ALC_TH_MASK, M98925_ALC_TH_SHIFT);
}

static int max98925_alc_threshold_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R030_THRESHOLD,
			M98925_ALC_TH_MASK, M98925_ALC_TH_SHIFT);
}

static const char * max98925_boost_voltage_text[] = {"8.5V", "8.25V",
		"8.0V", "7.75V","7.5V","7.25V","7.0V","6.75V", "6.5V", "6.5V",
		"6.5V","6.5V","6.5V","6.5V","6.5V","6.5V"};

static const struct soc_enum max98925_boost_voltage_enum =
	SOC_ENUM_SINGLE(MAX98925_R037_CONFIGURATION, M98925_BST_VOUT_SHIFT, 15,
			max98925_boost_voltage_text);

static int max98925_boost_voltage_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_get(kcontrol, ucontrol, MAX98925_R037_CONFIGURATION,
			M98925_BST_VOUT_MASK, M98925_BST_VOUT_SHIFT);
}

static int max98925_boost_voltage_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98925_reg_put(kcontrol, ucontrol, MAX98925_R037_CONFIGURATION,
			M98925_BST_VOUT_MASK, M98925_BST_VOUT_SHIFT);
}

static const struct snd_kcontrol_new max98925_snd_controls[] = {

	SOC_SINGLE_EXT_TLV("Left Speaker Gain", MAX98925_R02D_GAIN,
		M98925_SPK_GAIN_SHIFT, (1<<M98925_SPK_GAIN_WIDTH)-1, 0,
		max98925_left_gain_get, max98925_left_gain_put, max98925_spk_tlv),

	SOC_SINGLE_EXT_TLV("Right Speaker Gain", MAX98925_R02D_GAIN,
		M98925_SPK_GAIN_SHIFT, (1<<M98925_SPK_GAIN_WIDTH)-1, 0,
		max98925_right_gain_get, max98925_right_gain_put, max98925_spk_tlv),

	SOC_ENUM_EXT("Left Speaker Enable", max98925_amplifier_enum,
		max98925_left_en_get, max98925_left_en_put),

	SOC_ENUM_EXT("Right Speaker Enable", max98925_amplifier_enum,
		max98925_right_en_get, max98925_right_en_put),

	SOC_ENUM_EXT("Receiver Enable", max98925_amplifier_enum,
		max98925_receiver_en_get, max98925_receiver_en_put),

	SOC_SINGLE_EXT("Speaker Ramp", 0, 0, 1, 0,
		max98925_spk_ramp_get, max98925_spk_ramp_put),

	SOC_SINGLE_EXT("Speaker ZCD", 0, 0, 1, 0,
		max98925_spk_zcd_get, max98925_spk_zcd_put),

	SOC_SINGLE_EXT("ALC Enable", 0, 0, 1, 0,
		max98925_alc_en_get, max98925_alc_en_put),

	SOC_SINGLE_EXT("ALC Threshold", 0, 0, (1<<M98925_ALC_TH_WIDTH)-1, 0,
		max98925_alc_threshold_get, max98925_alc_threshold_put),

	SOC_ENUM_EXT("Boost Output Voltage", max98925_boost_voltage_enum,
		max98925_boost_voltage_get, max98925_boost_voltage_put),

	SOC_ENUM_EXT("Imon Enable", max98925_imon_enum,
		max98925_imon_en_get, max98925_imon_en_put),

	SOC_ENUM_EXT("Vmon Enable", max98925_vmon_enum,
		max98925_vmon_en_get, max98925_vmon_en_put),

	SOC_ENUM_EXT("ClassD Only", max98925_classd_only_enum,
		max98925_classd_en_get, max98925_classd_en_put),
};

static int max98925_add_widgets(struct snd_soc_codec *codec)
{
	int ret;

	ret = snd_soc_add_codec_controls(codec, max98925_snd_controls,
		ARRAY_SIZE(max98925_snd_controls));

#if 0
	snd_soc_dapm_new_controls(dapm, max98925_dapm_widgets,
		ARRAY_SIZE(max98925_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, max98925_dapm_routes,
		ARRAY_SIZE(max98925_dapm_routes));
#endif

	return 0;
}

/* codec sample rate and n/m dividers parameter table */
static const struct {
	u32 rate;
	u8  sr;
	u32 divisors[3][2];
} rate_table[] = {
	{ 8000, 0, {{  1,   375}, {5, 1764}, {  1,   384}}},
	{11025,	1, {{147, 40000}, {1,  256}, {147, 40960}}},
	{12000, 2, {{  1,   250}, {5, 1176}, {  1,   256}}},
	{16000, 3, {{  2,   375}, {5,  882}, {  1,   192}}},
	{22050, 4, {{147, 20000}, {1,  128}, {147, 20480}}},
	{24000, 5, {{  1,   125}, {5,  588}, {  1,   128}}},
	{32000, 6, {{  4,   375}, {5,  441}, {  1,    96}}},
	{44100, 7, {{147, 10000}, {1,   64}, {147, 10240}}},
	{48000, 8, {{  2,   125}, {5,  294}, {  1,    64}}},
};

static inline int max98925_rate_value(int rate, int clock, u8 *value,
		int *n, int *m)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
		if (rate_table[i].rate >= rate) {
			*value = rate_table[i].sr;
			*n = rate_table[i].divisors[clock][0];
			*m = rate_table[i].divisors[clock][1];
			ret = 0;
			break;
		}
	}

	pr_debug("%s: sample rate is %d, returning %d\n", __func__,
		rate_table[i].rate, *value);

	return ret;
}

static int max98925_set_tdm_slot(struct snd_soc_dai *codec_dai,
		unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	pr_debug("%s: tx_mask 0x%X, rx_mask 0x%X, slots %d, slot width %d\n",
			__func__, tx_mask, rx_mask, slots, slot_width);
	return 0;
}

static void max98925_set_slave(struct max98925_priv *max98925)
{
	pr_debug("%s: ENTER\n", __func__);

	/*
	 * 1. use BCLK instead of MCLK
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_DAI_CLK_SOURCE_MASK, M98925_DAI_CLK_SOURCE_MASK);
	regmap_update_bits(max98925->regmapR, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_DAI_CLK_SOURCE_MASK, M98925_DAI_CLK_SOURCE_MASK);
	/*
	 * 2. set DAI to slave mode
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_MAS_MASK, 0);
	regmap_update_bits(max98925->regmapR, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_MAS_MASK, 0);
	/*
	 * 3. set BLCKs to LRCLKs to 64
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_BSEL_MASK, M98925_DAI_BSEL_64);
	regmap_update_bits(max98925->regmapR, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_BSEL_MASK, M98925_DAI_BSEL_64);
	/*
	 * 4. set VMON slots
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R022_DOUT_CFG_VMON,
			M98925_DAI_VMON_EN_MASK, M98925_DAI_VMON_EN_MASK);
	regmap_update_bits(max98925->regmapR, MAX98925_R022_DOUT_CFG_VMON,
			M98925_DAI_VMON_EN_MASK, M98925_DAI_VMON_EN_MASK);
	regmap_update_bits(max98925->regmapL, MAX98925_R022_DOUT_CFG_VMON,
			M98925_DAI_VMON_SLOT_MASK, M98925_DAI_VMON_SLOT_02_03);
	regmap_update_bits(max98925->regmapR, MAX98925_R022_DOUT_CFG_VMON,
			M98925_DAI_VMON_SLOT_MASK, M98925_DAI_VMON_SLOT_06_07);
	/*
	 * 5. set IMON slots
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R023_DOUT_CFG_IMON,
			M98925_DAI_IMON_EN_MASK, M98925_DAI_IMON_EN_MASK);
	regmap_update_bits(max98925->regmapR, MAX98925_R023_DOUT_CFG_IMON,
			M98925_DAI_IMON_EN_MASK, M98925_DAI_IMON_EN_MASK);
	regmap_update_bits(max98925->regmapL, MAX98925_R023_DOUT_CFG_IMON,
			M98925_DAI_IMON_SLOT_MASK, M98925_DAI_VMON_SLOT_00_01);
	regmap_update_bits(max98925->regmapR, MAX98925_R023_DOUT_CFG_IMON,
			M98925_DAI_IMON_SLOT_MASK, M98925_DAI_VMON_SLOT_04_05);
}

static void max98925_set_master(struct max98925_priv *max98925)
{
	pr_debug("%s: ENTER\n", __func__);

	/*
	 * 1. use MCLK for Left channel, right channel always BCLK
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_DAI_CLK_SOURCE_MASK, 0);
	regmap_update_bits(max98925->regmapR, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_DAI_CLK_SOURCE_MASK, M98925_DAI_CLK_SOURCE_MASK);
	/*
	 * 2. set left channel DAI to master mode, right channel always slave
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_MAS_MASK, M98925_DAI_MAS_MASK);
	regmap_update_bits(max98925->regmapR, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_MAS_MASK, 0);
}

static int max98925_dai_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	struct max98925_cdata *cdata;
	unsigned int invert = 0;

	pr_debug("%s: fmt 0x%08X\n", __func__, fmt);

	cdata = &max98925->dai[0];

	cdata->fmt = fmt;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		max98925_set_slave(max98925);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		max98925_set_master(max98925);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "DAI clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		pr_debug("%s: set SND_SOC_DAIFMT_I2S\n", __func__);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		pr_debug("%s: set SND_SOC_DAIFMT_LEFT_J\n", __func__);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		pr_debug("%s: set SND_SOC_DAIFMT_DSP_A\n", __func__);
	default:
		dev_err(codec->dev, "DAI format unsupported, fmt:0x%x", fmt);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		invert = M98925_DAI_WCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = M98925_DAI_BCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		invert = M98925_DAI_BCI_MASK | M98925_DAI_WCI_MASK;
		break;
	default:
		dev_err(codec->dev, "DAI invert mode unsupported");
		return -EINVAL;
	}

	regmap_update_bits(max98925->regmapL, MAX98925_R020_FORMAT,
			M98925_DAI_BCI_MASK | M98925_DAI_BCI_MASK, invert);
	regmap_update_bits(max98925->regmapR, MAX98925_R020_FORMAT,
			M98925_DAI_BCI_MASK | M98925_DAI_BCI_MASK, invert);

	return 0;
}

static int max98925_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level)
{
	codec->dapm.bias_level = level;
	return 0;
}

static int max98925_set_clock(struct max98925_priv *max98925, unsigned int rate)
{
	unsigned int clock;
	unsigned int mdll;
	unsigned int n;
	unsigned int m;
	u8 dai_sr;

	switch (max98925->sysclk) {
	case 6000000:
		clock = 0;
		mdll  = M98925_MDLL_MULT_MCLKx16;
		break;
	case 11289600:
		clock = 1;
		mdll  = M98925_MDLL_MULT_MCLKx8;
		break;
	case 12000000:
		clock = 0;
		mdll  = M98925_MDLL_MULT_MCLKx8;
		break;
	case 12288000:
		clock = 2;
		mdll  = M98925_MDLL_MULT_MCLKx8;
		break;
	default:
		dev_err(max98925->codec->dev, "unsupported sysclk %d\n",
					max98925->sysclk);
		return -EINVAL;
	}

	if (max98925_rate_value(rate, clock, &dai_sr, &n, &m))
		return -EINVAL;

	/*
	 * 1. set DAI_SR to correct LRCLK frequency
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_SR_MASK, dai_sr << M98925_DAI_SR_SHIFT);
	regmap_update_bits(max98925->regmapR, MAX98925_R01B_DAI_CLK_MODE2,
			M98925_DAI_SR_MASK, dai_sr << M98925_DAI_SR_SHIFT);
	/*
	 * 2. set DAI m divider
	 */
	regmap_write(max98925->regmapL, MAX98925_R01C_DAI_CLK_DIV_M_MSBS,
			m >> 8);
	regmap_write(max98925->regmapL, MAX98925_R01D_DAI_CLK_DIV_M_LSBS,
			m & 0xFF);
	/*
	 * 3. set DAI n divider
	 */
	regmap_write(max98925->regmapL, MAX98925_R01E_DAI_CLK_DIV_N_MSBS,
			n >> 8);
	regmap_write(max98925->regmapL, MAX98925_R01F_DAI_CLK_DIV_N_LSBS,
			n & 0xFF);
	/*
	 * 4. set MDLL
	 */
	regmap_update_bits(max98925->regmapL, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_MDLL_MULT_MASK, mdll << M98925_MDLL_MULT_SHIFT);
	regmap_update_bits(max98925->regmapR, MAX98925_R01A_DAI_CLK_MODE1,
			M98925_MDLL_MULT_MASK, mdll << M98925_MDLL_MULT_SHIFT);

	return 0;
}

static int max98925_dai_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	struct max98925_cdata *cdata;
	unsigned int rate;

	pr_debug("%s: enter\n", __func__);

	cdata = &max98925->dai[0];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		pr_debug("%s: set SNDRV_PCM_FORMAT_S16_LE\n", __func__);
		regmap_update_bits(max98925->regmapL, MAX98925_R020_FORMAT,
				M98925_DAI_CHANSZ_MASK, M98925_DAI_CHANSZ_16);
		regmap_update_bits(max98925->regmapR, MAX98925_R020_FORMAT,
				M98925_DAI_CHANSZ_MASK, M98925_DAI_CHANSZ_16);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		pr_debug("%s: set SNDRV_PCM_FORMAT_S32_LE\n", __func__);
		regmap_update_bits(max98925->regmapL, MAX98925_R020_FORMAT,
				M98925_DAI_CHANSZ_MASK, M98925_DAI_CHANSZ_32);
		regmap_update_bits(max98925->regmapR, MAX98925_R020_FORMAT,
				M98925_DAI_CHANSZ_MASK, M98925_DAI_CHANSZ_32);
		break;
	default:
		pr_debug("%s: format unsupported %d", __func__, params_format(params));
		return -EINVAL;
	}

	return max98925_set_clock(max98925, rate);
}

static int max98925_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: clk_id %d, freq %d, dir %d\n", __func__, clk_id, freq, dir);

	max98925->sysclk = freq;

	return 0;
}
#define STEREO_CHANNEL_MODE         (0x3000000>>24)
#define ALG_AUTO_MUTE               (0x68 | 0x3000000)
#define ALG_ENABLE_CMD              (1)

static int max98925_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec_dai->codec);

	int* param = (int*) payload;

	param[0] = STEREO_CHANNEL_MODE,
	param[1] = 1,
	param[2] = ALG_AUTO_MUTE;
	param[3] = ALG_ENABLE_CMD;

	if (mute) {
		if (pa_enable) {
			afe_dsm_param_ctrl(1, 32,(uint8_t*)param);
			msleep(35);
			regmap_update_bits(max98925->regmapL, MAX98925_R038_GLOBAL_ENABLE,
				M98925_EN_MASK, 0x0);
			regmap_update_bits(max98925->regmapR, MAX98925_R038_GLOBAL_ENABLE,
				M98925_EN_MASK, 0x0);
			msleep(20);
		}
		pa_enable = false;

		if (DEFAULT_SWITCH_NONEED != max98925->switch_en_gpio)
			gpio_direction_output(max98925->switch_en_gpio, GPIO_PULL_DOWN);
	} else {
		regmap_update_bits(max98925->regmapL, MAX98925_R02D_GAIN,
			M98925_SPK_GAIN_MASK, max98925->left_gain);
		regmap_update_bits(max98925->regmapR, MAX98925_R02D_GAIN,
			M98925_SPK_GAIN_MASK, max98925->right_gain);

		regmap_write(max98925->regmapL, MAX98925_R038_GLOBAL_ENABLE,
			max98925->left_en);
		regmap_write(max98925->regmapR, MAX98925_R038_GLOBAL_ENABLE,
			max98925->right_en);
		pa_enable = true;
	}

	pr_debug("%s: mute %d\n", __func__, mute);

	return 0;
}

#define MAX98925_RATES SNDRV_PCM_RATE_8000_48000
#define MAX98925_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops max98925_dai_ops = {
	.set_sysclk = max98925_dai_set_sysclk,
	.set_fmt = max98925_dai_set_fmt,
	.set_tdm_slot = max98925_set_tdm_slot,
	.hw_params = max98925_dai_hw_params,
	.digital_mute = max98925_dai_digital_mute,
};

static struct snd_soc_dai_driver max98925_dai[] = {
	{
		.name = "max98925-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = MAX98925_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = MAX98925_FORMATS,
		},
		.ops = &max98925_dai_ops,
	}
};

static void max98925_handle_pdata(struct snd_soc_codec *codec)
{
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	struct max98925_pdata *pdata = max98925->pdata;

	if (!pdata) {
		dev_dbg(codec->dev, "No platform data\n");
		return;
	}
}

#ifdef CONFIG_PM
static int max98925_suspend(struct snd_soc_codec *codec)
{
	pr_debug("%s: enter\n", __func__);

	return 0;
}

static int max98925_resume(struct snd_soc_codec *codec)
{
	pr_debug("%s: enter\n", __func__);

	return 0;
}
#else
#define max98925_suspend NULL
#define max98925_resume NULL
#endif

static int max98925_probe(struct snd_soc_codec *codec)
{
	struct max98925_priv *max98925 = snd_soc_codec_get_drvdata(codec);
	struct max98925_cdata *cdata;
	int ret = 0;
	int reg = 0;

	dev_dbg(codec->dev, "STEREO - built on %s at %s\n",
		__DATE__, __TIME__);
	dev_dbg(codec->dev, "build number %s\n", MAX98925_REVISION);

	max98925->codec = codec;
	codec->control_data = max98925->regmapL;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* sysclk is hard-coded here but should be set by the machine driver */
	max98925->sysclk = 12288000;
	max98925->left_gain = 0x14;
	max98925->right_gain = 0x14;

	cdata = &max98925->dai[0];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;

	reg = 0;
	ret = regmap_read(max98925->regmapL, MAX98925_R0FF_VERSION, &reg);
	if ((ret < 0) || ((reg < MAX98925_VERSION) && (reg < MAX98925_VERSION1))) {
		dev_err(codec->dev, "L device initialization error (%d 0x%02X)\n",
			ret, reg);
		goto err_access;
	}
	dev_info(codec->dev, "L device version 0x%02X\n", reg);

	if (max98925->regmapR != NULL) {
		reg = 0;
		ret = regmap_read(max98925->regmapR, MAX98925_R0FF_VERSION, &reg);
		if ((ret < 0) || ((reg < MAX98925_VERSION) && (reg < MAX98925_VERSION1))) {
			dev_err(codec->dev,
				"R device initialization error (%d 0x%02X)\n", ret, reg);
			goto err_access;
		}
		dev_info(codec->dev, "R device version 0x%02X\n", reg);
	}

#if 0
	/* FOR DEBUGGING ONLY */
	regcache_cache_bypass(max98925->regmapL, true);
	regcache_cache_bypass(max98925->regmapR, true);
	dev_info(codec->dev, "regmap cache bypass ENABLED!\n");
	/**********************/
#endif

	regmap_write(max98925->regmapL, MAX98925_R038_GLOBAL_ENABLE, 0x00);
	regmap_write(max98925->regmapR, MAX98925_R038_GLOBAL_ENABLE, 0x00);
	pa_enable = false;

	/* It's not the default but we need to set DAI_DLY */
	regmap_write(max98925->regmapL, MAX98925_R020_FORMAT, M98925_DAI_EXTBCLK_HIZ_MASK | M98925_DAI_DLY_MASK);
	regmap_write(max98925->regmapR, MAX98925_R020_FORMAT, M98925_DAI_EXTBCLK_HIZ_MASK | M98925_DAI_DLY_MASK);

	regmap_write(max98925->regmapL, MAX98925_R021_TDM_SLOT_SELECT, 0xC0);
	regmap_write(max98925->regmapR, MAX98925_R021_TDM_SLOT_SELECT, 0xC0);

	regmap_write(max98925->regmapL, MAX98925_R027_DOUT_HIZ_CFG1, 0xFF);
	regmap_write(max98925->regmapL, MAX98925_R028_DOUT_HIZ_CFG2, 0xFF);
	regmap_write(max98925->regmapL, MAX98925_R029_DOUT_HIZ_CFG3, 0xFF);
	regmap_write(max98925->regmapL, MAX98925_R02A_DOUT_HIZ_CFG4, 0xF0);
	regmap_write(max98925->regmapR, MAX98925_R027_DOUT_HIZ_CFG1, 0xFF);
	regmap_write(max98925->regmapR, MAX98925_R028_DOUT_HIZ_CFG2, 0xFF);
	regmap_write(max98925->regmapR, MAX98925_R029_DOUT_HIZ_CFG3, 0xFF);
	regmap_write(max98925->regmapR, MAX98925_R02A_DOUT_HIZ_CFG4, 0x0F);

	regmap_write(max98925->regmapL, MAX98925_R02C_FILTERS, 0xD9);
	regmap_write(max98925->regmapR, MAX98925_R02C_FILTERS, 0xD9);

	regmap_write(max98925->regmapL, MAX98925_R034_ALC_CONFIGURATION, 0x12);
	regmap_write(max98925->regmapR, MAX98925_R034_ALC_CONFIGURATION, 0x12);

	regmap_write(max98925->regmapL, MAX98925_R037_CONFIGURATION, 0x00);
	regmap_write(max98925->regmapR, MAX98925_R037_CONFIGURATION, 0x00);

	// Disable ALC muting
	regmap_write(max98925->regmapL, MAX98925_R03A_BOOST_LIMITER, 0xF8);
	regmap_write(max98925->regmapR, MAX98925_R03A_BOOST_LIMITER, 0xF8);

	/* set drv strength to 3.25mA */
	regmap_write(max98925->regmapL, MAX98925_R02B_DOUT_DRV_STRENGTH, 0x00);
	regmap_write(max98925->regmapR, MAX98925_R02B_DOUT_DRV_STRENGTH, 0x00);

	regmap_write(max98925->regmapL, MAX98925_R035_BOOST_CONVERTER, 0x01);
	regmap_write(max98925->regmapR, MAX98925_R035_BOOST_CONVERTER, 0x21);

	regmap_update_bits(max98925->regmapL, MAX98925_R02D_GAIN,
			M98925_DAC_IN_SEL_MASK, M98925_DAC_IN_SEL_LEFT_DAI);
	regmap_update_bits(max98925->regmapR, MAX98925_R02D_GAIN,
			M98925_DAC_IN_SEL_MASK, M98925_DAC_IN_SEL_RIGHT_DAI);

	regmap_update_bits(max98925->regmapL, MAX98925_R036_BLOCK_ENABLE,
			M98925_BST_EN_MASK | M98925_SPK_EN_MASK |
			M98925_ADC_IMON_EN_MASK | M98925_ADC_VMON_EN_MASK,
			M98925_BST_EN_MASK | M98925_SPK_EN_MASK |
			M98925_ADC_IMON_EN_MASK | M98925_ADC_VMON_EN_MASK);
	regmap_update_bits(max98925->regmapR, MAX98925_R036_BLOCK_ENABLE,
			M98925_BST_EN_MASK | M98925_SPK_EN_MASK |
			M98925_ADC_IMON_EN_MASK | M98925_ADC_VMON_EN_MASK,
			M98925_BST_EN_MASK | M98925_SPK_EN_MASK |
			M98925_ADC_IMON_EN_MASK | M98925_ADC_VMON_EN_MASK);

	max98925_handle_pdata(codec);
	max98925_add_widgets(codec);

err_access:
	pr_debug("%s: exit %d\n", __func__, ret);

	return ret;
}

static int max98925_remove(struct snd_soc_codec *codec)
{
	pr_debug("%s: enter\n", __func__);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max98925 = {
	.probe            = max98925_probe,
	.remove           = max98925_remove,
	.set_bias_level   = max98925_set_bias_level,
	.suspend          = max98925_suspend,
	.resume           = max98925_resume,
};

static const struct regmap_config max98925_regmap = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = MAX98925_R0FF_VERSION,
	.reg_defaults     = max98925_reg,
	.num_reg_defaults = ARRAY_SIZE(max98925_reg),
	.volatile_reg     = max98925_volatile_register,
	.readable_reg     = max98925_readable_register,
	.cache_type       = REGCACHE_RBTREE,
};

/* There should be a second MAX98925 on the board */
static struct i2c_board_info max98925_i2c_right[] = {
	{
		I2C_BOARD_INFO("max98925r", 0x32),
	}
};

static int max98925_i2c_probe(struct i2c_client *i2c_l,
			     const struct i2c_device_id *id)
{
	struct max98925_priv *max98925;
	struct i2c_client *i2c_r;
	int ret;
	const __be32 *paddr;
	int len;

	pr_debug("%s: enter, device '%s'\n", __func__, id->name);

	max98925_regulator_config(i2c_l,
		of_property_read_bool(i2c_l->dev.of_node, "i2c-pull-up"), 1);

	max98925 = kzalloc(sizeof(struct max98925_priv), GFP_KERNEL);
	if (max98925 == NULL)
		return -ENOMEM;

	max98925->devtype = id->driver_data;
	i2c_set_clientdata(i2c_l, max98925);
	max98925->control_data = i2c_l;
	max98925->pdata = i2c_l->dev.platform_data;

	max98925->regmapL = regmap_init_i2c(i2c_l, &max98925_regmap);
	if ( IS_ERR(max98925->regmapL) ) {
		ret = PTR_ERR(max98925->regmapL);
		dev_err(&i2c_l->dev, "Failed to allocate regmapL: %d\n", ret);
		goto err_out;
	}

	/* Check for second MAX98925, we assume they are on the same bus*/
	paddr = of_get_property(i2c_l->dev.of_node, "right-reg", &len);
	if (paddr && (len >= sizeof(int))) {
		max98925_i2c_right->addr = be32_to_cpup(paddr);
		pr_warn("right channel getted reg value 0x%x\n", max98925_i2c_right->addr);
	}
	i2c_r = i2c_new_device(i2c_l->adapter, max98925_i2c_right);

	if (IS_ERR(i2c_r)) {
		dev_err(&i2c_l->dev, "second MAX98925 not found\n");
		ret = -ENODEV;
		goto err_out;
	} else {
		max98925->regmapR = regmap_init_i2c(i2c_r, &max98925_regmap);
		if (IS_ERR(max98925->regmapR)) {
			ret = PTR_ERR(max98925->regmapR);
			dev_err(&i2c_r->dev, "Failed to allocate regmapR: %d\n", ret);
			goto err_out;
		}
	}

	ret = snd_soc_register_codec(&i2c_l->dev, &soc_codec_dev_max98925,
			max98925_dai, ARRAY_SIZE(max98925_dai));

#ifdef USE_DSM_MISC_DEV
	pr_debug("%s: maxdsm_init() returned %d\n", __func__, maxdsm_init());
#endif

	ret = of_get_named_gpio(i2c_l->dev.of_node, "speaker_receiver_switch", 0);
	if (ret < 0) {
		pr_err("%s:Unable to read gpio pin number\n",__func__);
		max98925->switch_en_gpio = DEFAULT_SWITCH_NONEED;
	} else {
		pr_err("%s: read hac gpio  %d\n", __func__,ret);
		max98925->switch_en_gpio = ret;
		/* request var for gpio with receiver */
		ret = gpio_request(max98925->switch_en_gpio, "receiver_switch_gpio");
		if (ret) {
			pr_err("%s: Failed to configure spk&receiver switch "
					"gpio %u\n", __func__, max98925->switch_en_gpio);
			max98925->switch_en_gpio = DEFAULT_SWITCH_NONEED;
		} else {
			gpio_direction_output(max98925->switch_en_gpio, GPIO_PULL_DOWN);
		}
	}

	pr_debug("%s: ret %d\n", __func__, ret);

err_out:

	if (ret < 0) {
		if (max98925->regmapL)
			regmap_exit(max98925->regmapL);
		if (max98925->regmapR)
			regmap_exit(max98925->regmapR);
		kfree(max98925);
	}

	return ret;
}

static int max98925_i2c_remove(struct i2c_client *client)
{
	struct max98925_priv *max98925 = dev_get_drvdata(&client->dev);

	snd_soc_unregister_codec(&client->dev);
	if (max98925->regmapL)
		regmap_exit(max98925->regmapL);
	if (max98925->regmapR)
		regmap_exit(max98925->regmapL);
	kfree(i2c_get_clientdata(client));

#ifdef USE_DSM_MISC_DEV
	maxdsm_deinit();
#endif

	pr_debug("%s: exit\n", __func__);

	return 0;
}

static struct i2c_device_id max98925_id_table[] = {
	{"max98925", MAX98925},
	{}
};

static const struct of_device_id max98925_of_match[]  = {
	{ .compatible = "maxim,max98925", },
	{},
};

MODULE_DEVICE_TABLE(i2c, max98925_id_table);

static struct i2c_driver max98925_i2c_driver = {
	.driver = {
		.name = "max98925",
		.owner = THIS_MODULE,
		.of_match_table = max98925_of_match,
	},
	.id_table = max98925_id_table,
	.probe  = max98925_i2c_probe,
	.remove = max98925_i2c_remove,
};

module_i2c_driver(max98925_i2c_driver);

MODULE_DESCRIPTION("ALSA SoC MAX98925 Stereo driver");
MODULE_AUTHOR("Ralph Birt <rdbirt@gmail.com>");
MODULE_LICENSE("GPL");
