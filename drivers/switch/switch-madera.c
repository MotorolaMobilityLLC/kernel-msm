/*
 * switch-madera.c - Switch driver for Cirrus Logic Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/switch.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/math64.h>

#include <sound/soc.h>

#include <linux/extcon/extcon-madera.h>
#include <linux/extcon/extcon-madera-pdata.h>
#include <dt-bindings/extcon/madera.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/mfd/madera/registers.h>

#define MADERA_MAX_MICD_RANGE		8

#define MADERA_MICD_CLAMP_MODE_JD1L	0x4
#define MADERA_MICD_CLAMP_MODE_JD1H	0x5
#define MADERA_MICD_CLAMP_MODE_JD1L_JD2L 0x8
#define MADERA_MICD_CLAMP_MODE_JD1L_JD2H 0x9
#define MADERA_MICD_CLAMP_MODE_JD1H_JD2H 0xb

#define MADERA_MICD_WRONG_POLARITY	30

#define MADERA_HPDET_LINEOUT		500000 /* 5,000 ohms */
#define MADERA_HPDET_MAX		10000
#define MADERA_HP_SHORT_IMPEDANCE_MIN	4

#define MADERA_HPDET_DEBOUNCE_MS	500
#define MADERA_DEFAULT_MICD_TIMEOUT_MS	2000

#define MADERA_MICROPHONE_MIN_OHM	1258
#define MADERA_MICROPHONE_MAX_OHM	30000

#define MADERA_MIC_MUTE			1
#define MADERA_MIC_UNMUTE		0

#define MADERA_HP_TUNING_INVALID	-1

/* Conversion between ohms and hundredths of an ohm. */
#define HOHM_TO_OHM(X)	(((X) == INT_MAX || (X) == MADERA_HP_Z_OPEN) ? \
			 (X) : ((X) + 50) / 100)
#define OHM_TO_HOHM(X)	((X) >= (INT_MAX / 100) ? INT_MAX : (X) * 100)

struct madera_micd_bias {
	unsigned int bias;
	bool enabled;
};

struct madera_hpdet_trims {
	int off_x4;
	int grad_x4;
};

struct madera_extcon_info {
	struct device *dev;
	struct madera *madera;
	const struct madera_accdet_pdata *pdata;
	struct mutex lock;
	struct regulator *micvdd;
	struct input_dev *input;
	struct switch_dev edev;

	u16 last_jackdet;
	int hp_tuning_level;

	const struct madera_hpdet_calibration_data *hpdet_ranges;
	int num_hpdet_ranges;
	const struct madera_hpdet_trims *hpdet_trims;

	int micd_mode;
	const struct madera_micd_config *micd_modes;
	int micd_num_modes;

	const struct madera_micd_range *micd_ranges;
	int num_micd_ranges;

	int micd_res_old;
	int micd_debounce;
	int micd_count;

	struct completion manual_mic_completion;

	struct delayed_work micd_detect_work;

	bool have_mic;
	bool detecting;
	int jack_flips;

	const struct madera_jd_state *state;
	const struct madera_jd_state *old_state;
	struct delayed_work state_timeout_work;

	struct wakeup_source detection_wake_lock;

	struct madera_micd_bias micd_bias;
};

static const struct madera_micd_config cs47l85_micd_default_modes[] = {
	{ MADERA_ACCD_SENSE_MICDET2, 0, MADERA_ACCD_BIAS_SRC_MICBIAS1, 0, 0 },
	{ MADERA_ACCD_SENSE_MICDET1, 0, MADERA_ACCD_BIAS_SRC_MICBIAS2, 1, 0 },
};

static const struct madera_micd_config madera_micd_default_modes[] = {
	{ MADERA_MICD1_SENSE_MICDET1, MADERA_MICD1_GND_MICDET2,
	  MADERA_MICD_BIAS_SRC_MICBIAS1A, 0, MADERA_HPD_GND_HPOUTFB2 },
	{ MADERA_MICD1_SENSE_MICDET2, MADERA_MICD1_GND_MICDET1,
	  MADERA_MICD_BIAS_SRC_MICBIAS1B, 1, MADERA_HPD_GND_HPOUTFB1 },
};

static const unsigned int madera_default_hpd_pins[4] = {
	[0] = MADERA_HPD_OUT_OUT1L,
	[1] = MADERA_HPD_SENSE_HPDET1,
	[2] = MADERA_HPD_OUT_OUT1R,
	[3] = MADERA_HPD_SENSE_HPDET1,
};

static struct madera_micd_range madera_micd_default_ranges[] = {
	{ .max =  11, .key = BTN_0 },
	{ .max =  28, .key = BTN_1 },
	{ .max =  54, .key = BTN_2 },
	{ .max = 100, .key = BTN_3 },
	{ .max = 186, .key = BTN_4 },
	{ .max = 430, .key = BTN_5 },
	{ .max = -1, .key = -1 },
	{ .max = -1, .key = -1 },
};

/* The number of levels in madera_micd_levels valid for button thresholds */
#define MADERA_NUM_MICD_BUTTON_LEVELS 64

static const int madera_micd_levels[] = {
	3, 6, 8, 11, 13, 16, 18, 21, 23, 26, 28, 31, 34, 36, 39, 41, 44, 46,
	49, 52, 54, 57, 60, 62, 65, 67, 70, 73, 75, 78, 81, 83, 89, 94, 100,
	105, 111, 116, 122, 127, 139, 150, 161, 173, 186, 196, 209, 220, 245,
	270, 295, 321, 348, 375, 402, 430, 489, 550, 614, 681, 752, 903, 1071,
	1257, 30000,
};

#define MADERA_JACK_SWITCH_TYPES 3

static int arizona_switch_types[MADERA_JACK_SWITCH_TYPES] = {
	SW_HEADPHONE_INSERT,
	SW_MICROPHONE_INSERT,
	SW_LINEOUT_INSERT
};

/* These values are copied from Android WiredAccessoryObserver */
enum headset_state {
	BIT_NO_HEADSET = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADSET_NO_MIC = (1 << 1),
	BIT_LINEOUT = (1 << 5),
};

struct madera_hpdet_calibration_data {
	int	min;
	int	max;
	s64	C0;		/* value * 1000000 */
	s64	C1;		/* value * 10000 */
	s64	C2;		/* not multiplied */
	s64	C3;		/* value * 1000000 */
	s64	C4_x_C3;	/* value * 1000000 */
	s64	C5;		/* value * 1000000 */
	s64	dacval_adjust;
};

static const struct madera_hpdet_calibration_data cs47l85_hpdet_ranges[] = {
	{ 4, 30, 1007000, -7200, 4003, 69300000, 381150, 250000, 500000},
	{ 8, 100, 1007000,   -7200,   7975, 69600000, 382800, 250000, 500000},
	{ 100,  1000,  9696000,   -79500,  7300, 62900000, 345950, 250000,
	500000},
	{ 1000, 10000, 100684000, -949400, 7300, 63200000, 347600, 250000,
	500000},
};

static const struct madera_hpdet_calibration_data madera_hpdet_ranges[] = {
	{ 4, 30, 1014000, -4300, 3950, 69300000, 381150, 700000, 500000},
	{ 8, 100, 1014000, -8600, 7975, 69600000, 382800, 700000, 500000},
	{ 100, 1000, 9744000, -79500,  7300, 62900000, 345950, 700000, 500000},
	{ 1000, 10000, 101158000, -949400, 7300, 63200000, 347600, 700000,
	500000},
};

struct madera_hp_tuning {
	int max_ohm;
	const struct reg_sequence *patch;
	int patch_len;
};

static const struct reg_sequence cs47l35_low_impedance_patch[] = {
	{ 0x460, 0x0C40 },
	{ 0x461, 0xCD1A },
	{ 0x462, 0x0C40 },
	{ 0x463, 0xB53B },
	{ 0x464, 0x0C41 },
	{ 0x465, 0x4826 },
	{ 0x466, 0x0C41 },
	{ 0x467, 0x2EDA },
	{ 0x468, 0x0C41 },
	{ 0x469, 0x203A },
	{ 0x46A, 0x0841 },
	{ 0x46B, 0x121F },
	{ 0x46C, 0x0446 },
	{ 0x46D, 0x0B6F },
	{ 0x46E, 0x0446 },
	{ 0x46F, 0x0818 },
	{ 0x470, 0x04C6 },
	{ 0x471, 0x05BB },
	{ 0x472, 0x04C6 },
	{ 0x473, 0x040F },
	{ 0x474, 0x04CE },
	{ 0x475, 0x0339 },
	{ 0x476, 0x05DF },
	{ 0x477, 0x028F },
	{ 0x478, 0x05DF },
	{ 0x479, 0x0209 },
	{ 0x47A, 0x05DF },
	{ 0x47B, 0x00CF },
	{ 0x47C, 0x05DF },
	{ 0x47D, 0x0001 },
	{ 0x47E, 0x07FF },
};

static const struct reg_sequence cs47l35_normal_impedance_patch[] = {
	{ 0x460, 0x0C40 },
	{ 0x461, 0xCD1A },
	{ 0x462, 0x0C40 },
	{ 0x463, 0xB53B },
	{ 0x464, 0x0C40 },
	{ 0x465, 0x7503 },
	{ 0x466, 0x0C40 },
	{ 0x467, 0x4A41 },
	{ 0x468, 0x0041 },
	{ 0x469, 0x3491 },
	{ 0x46A, 0x0841 },
	{ 0x46B, 0x1F50 },
	{ 0x46C, 0x0446 },
	{ 0x46D, 0x14ED },
	{ 0x46E, 0x0446 },
	{ 0x46F, 0x1455 },
	{ 0x470, 0x04C6 },
	{ 0x471, 0x1220 },
	{ 0x472, 0x04C6 },
	{ 0x473, 0x040F },
	{ 0x474, 0x04CE },
	{ 0x475, 0x0339 },
	{ 0x476, 0x05DF },
	{ 0x477, 0x028F },
	{ 0x478, 0x05DF },
	{ 0x479, 0x0209 },
	{ 0x47A, 0x05DF },
	{ 0x47B, 0x00CF },
	{ 0x47C, 0x05DF },
	{ 0x47D, 0x0001 },
	{ 0x47E, 0x07FF },
};

static const struct madera_hp_tuning cs47l35_hp_tuning[] = {
	{
		13,
		cs47l35_low_impedance_patch,
		ARRAY_SIZE(cs47l35_low_impedance_patch),
	},
	{
		MADERA_HPDET_MAX,
		cs47l35_normal_impedance_patch,
		ARRAY_SIZE(cs47l35_normal_impedance_patch),
	},
};

static const struct reg_sequence cs47l85_low_impedance_patch[] = {
	{ 0x465, 0x4C6D },
	{ 0x467, 0x3950 },
	{ 0x469, 0x2D86 },
	{ 0x46B, 0x1E6D },
	{ 0x46D, 0x199A },
	{ 0x46F, 0x1456 },
	{ 0x483, 0x0826 },
};

static const struct reg_sequence cs47l85_normal_impedance_patch[] = {
	{ 0x465, 0x8A43 },
	{ 0x467, 0x7259 },
	{ 0x469, 0x65EA },
	{ 0x46B, 0x50F4 },
	{ 0x46D, 0x41CD },
	{ 0x46F, 0x199A },
	{ 0x483, 0x0023 },
};

static const struct madera_hp_tuning cs47l85_hp_tuning[] = {
	{
		13,
		cs47l85_low_impedance_patch,
		ARRAY_SIZE(cs47l85_low_impedance_patch),
	},
	{
		MADERA_HPDET_MAX,
		cs47l85_normal_impedance_patch,
		ARRAY_SIZE(cs47l85_normal_impedance_patch),
	},
};

static const struct reg_sequence cs47l90_low_impedance_patch[] = {
	{ 0x460, 0x0C21 },
	{ 0x461, 0xB53C },
	{ 0x462, 0x0C21 },
	{ 0x463, 0xA186 },
	{ 0x464, 0x0C21 },
	{ 0x465, 0x8FF6 },
	{ 0x466, 0x0C24 },
	{ 0x467, 0x804E },
	{ 0x468, 0x0C24 },
	{ 0x469, 0x725A },
	{ 0x46A, 0x0C24 },
	{ 0x46B, 0x5AD5 },
	{ 0x46C, 0x0C28 },
	{ 0x46D, 0x50F4 },
	{ 0x46E, 0x0C2C },
	{ 0x46F, 0x4827 },
	{ 0x470, 0x0C31 },
	{ 0x471, 0x404E },
	{ 0x472, 0x0020 },
	{ 0x473, 0x3950 },
	{ 0x474, 0x0028 },
	{ 0x475, 0x3314 },
	{ 0x476, 0x0030 },
	{ 0x477, 0x2893 },
	{ 0x478, 0x003F },
	{ 0x479, 0x2429 },
	{ 0x47A, 0x0830 },
	{ 0x47B, 0x203A },
	{ 0x47C, 0x0420 },
	{ 0x47D, 0x1027 },
	{ 0x47E, 0x0430 },
};

static const struct reg_sequence cs47l90_normal_impedance_patch[] = {
	{ 0x460, 0x0C21 },
	{ 0x461, 0xB53C },
	{ 0x462, 0x0C25 },
	{ 0x463, 0xA186 },
	{ 0x464, 0x0C26 },
	{ 0x465, 0x8FF6 },
	{ 0x466, 0x0C28 },
	{ 0x467, 0x804E },
	{ 0x468, 0x0C30 },
	{ 0x469, 0x725A },
	{ 0x46A, 0x0C30 },
	{ 0x46B, 0x65EA },
	{ 0x46C, 0x0028 },
	{ 0x46D, 0x5AD5 },
	{ 0x46E, 0x0028 },
	{ 0x46F, 0x50F4 },
	{ 0x470, 0x0030 },
	{ 0x471, 0x4827 },
	{ 0x472, 0x0030 },
	{ 0x473, 0x404E },
	{ 0x474, 0x003F },
	{ 0x475, 0x3950 },
	{ 0x476, 0x0830 },
	{ 0x477, 0x3314 },
	{ 0x478, 0x0420 },
	{ 0x479, 0x2D86 },
	{ 0x47A, 0x0428 },
	{ 0x47B, 0x2893 },
	{ 0x47C, 0x0428 },
	{ 0x47D, 0x203A },
	{ 0x47E, 0x0428 },
};

static const struct reg_sequence cs47l90_high_impedance_patch[] = {
	{ 0x460, 0x0C21 },
	{ 0x461, 0xB53C },
	{ 0x462, 0x0C26 },
	{ 0x463, 0xA186 },
	{ 0x464, 0x0C28 },
	{ 0x465, 0x8FF6 },
	{ 0x466, 0x0C2A },
	{ 0x467, 0x804E },
	{ 0x468, 0x0025 },
	{ 0x469, 0x725A },
	{ 0x46A, 0x0030 },
	{ 0x46B, 0x65EA },
	{ 0x46C, 0x0030 },
	{ 0x46D, 0x5AD5 },
	{ 0x46E, 0x003F },
	{ 0x46F, 0x50F4 },
	{ 0x470, 0x003F },
	{ 0x471, 0x4827 },
	{ 0x472, 0x0830 },
	{ 0x473, 0x404E },
	{ 0x474, 0x083F },
	{ 0x475, 0x3950 },
	{ 0x476, 0x0420 },
	{ 0x477, 0x3314 },
	{ 0x478, 0x0430 },
	{ 0x479, 0x2D86 },
	{ 0x47A, 0x0430 },
	{ 0x47B, 0x2893 },
	{ 0x47C, 0x0430 },
	{ 0x47D, 0x203A },
	{ 0x47E, 0x0430 },
};

static const struct madera_hp_tuning cs47l90_hp_tuning[] = {
	{
		14,
		cs47l90_low_impedance_patch,
		ARRAY_SIZE(cs47l90_low_impedance_patch),
	},
	{	24,
		cs47l90_normal_impedance_patch,
		ARRAY_SIZE(cs47l90_normal_impedance_patch),
	},
	{	MADERA_HPDET_MAX,
		cs47l90_high_impedance_patch,
		ARRAY_SIZE(cs47l90_high_impedance_patch),
	},
};

static ssize_t madera_extcon_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct madera_extcon_info *info = platform_get_drvdata(pdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 info->madera->hp_impedance_x100[0]);
}

static DEVICE_ATTR(hp1_impedance, S_IRUGO, madera_extcon_show, NULL);

static inline bool madera_is_lineout(struct madera_extcon_info *info)
{
	return info->madera->hp_impedance_x100[0] >= MADERA_HPDET_LINEOUT;
}

inline void madera_extcon_report(struct madera_extcon_info *info, int state)
{
	dev_dbg(info->madera->dev, "Switch Report: %d\n", state);
	switch_set_state(&info->edev, state);

	if (!info->pdata->report_to_input)
		return;

	if (state == BIT_LINEOUT)
		input_report_switch(info->input, SW_LINEOUT_INSERT, 1);
	else
		input_report_switch(info->input, SW_LINEOUT_INSERT, 0);

	if (state == BIT_HEADSET)
		input_report_switch(info->input, SW_MICROPHONE_INSERT, 1);
	else
		input_report_switch(info->input, SW_MICROPHONE_INSERT, 0);

	if ((state == BIT_HEADSET) || (state == BIT_HEADSET_NO_MIC))
		input_report_switch(info->input, SW_HEADPHONE_INSERT, 1);
	else
		input_report_switch(info->input, SW_HEADPHONE_INSERT, 0);

	input_sync(info->input);
}
EXPORT_SYMBOL_GPL(madera_extcon_report);

static
enum madera_accdet_mode madera_jds_get_mode(struct madera_extcon_info *info)
{
	if (info->state)
		return info->state->mode;
	else
		return MADERA_ACCDET_MODE_INVALID;
}

int madera_jds_set_state(struct madera_extcon_info *info,
			 const struct madera_jd_state *new_state)
{
	int ret = 0;

	if (new_state != info->state) {
		if (info->state)
			info->state->stop(info);

		info->state = new_state;

		if (info->state) {
			ret = info->state->start(info);
			if (ret < 0)
				info->state = NULL;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(madera_jds_set_state);

static void madera_jds_reading(struct madera_extcon_info *info, int val)
{
	int ret;

	ret = info->state->reading(info, val);

	if (ret == -EAGAIN && info->state->restart)
		info->state->restart(info);
}

static inline bool madera_jds_cancel_timeout(struct madera_extcon_info *info)
{
	return cancel_delayed_work_sync(&info->state_timeout_work);
}

static void madera_jds_start_timeout(struct madera_extcon_info *info)
{
	const struct madera_jd_state *state = info->state;

	if (!state)
		return;

	if (state->timeout_ms && state->timeout) {
		int ms = state->timeout_ms(info);

		schedule_delayed_work(&info->state_timeout_work,
				      msecs_to_jiffies(ms));
	}
}

static void madera_jds_timeout_work(struct work_struct *work)
{
	struct madera_extcon_info *info =
		container_of(work, struct madera_extcon_info,
			     state_timeout_work.work);

	mutex_lock(&info->lock);

	if (!info->state) {
		dev_warn(info->madera->dev, "Spurious timeout in idle state\n");
	} else if (!info->state->timeout) {
		dev_warn(info->madera->dev, "Spurious timeout state.mode=%d\n",
			 info->state->mode);
	} else {
		info->state->timeout(info);
		madera_jds_start_timeout(info);
	}

	mutex_unlock(&info->lock);
}

static void madera_extcon_hp_clamp(struct madera_extcon_info *info,
				   bool clamp)
{
	struct madera *madera = info->madera;
	unsigned int mask, val = 0;
	unsigned int edre_reg = 0, edre_val = 0;
	unsigned int ep_sel = 0;
	int ret;

	snd_soc_dapm_mutex_lock(madera->dapm);

	switch (madera->type) {
	case CS47L35:
		/* check whether audio is routed to EPOUT, do not disable OUT1
		 * in that case.
		 */
		regmap_read(madera->regmap, MADERA_OUTPUT_ENABLES_1, &ep_sel);
		ep_sel &= MADERA_EP_SEL_MASK;
		/* fall through to next step to set common variables */
	case CS47L85:
	case WM1840:
		edre_reg = MADERA_EDRE_MANUAL;
		mask = MADERA_HP1L_SHRTO | MADERA_HP1L_FLWR | MADERA_HP1L_SHRTI;
		if (clamp) {
			val = MADERA_HP1L_SHRTO;
			edre_val = 0x3;
		} else {
			val = MADERA_HP1L_FLWR | MADERA_HP1L_SHRTI;
			edre_val = 0;
		}
		break;
	default:
		mask = MADERA_HPD_OVD_ENA_SEL_MASK;
		if (clamp)
			val = MADERA_HPD_OVD_ENA_SEL_MASK;
		else
			val = 0;
		break;
	}

	madera->hpdet_clamp[0] = clamp;

	/* Keep the HP output stages disabled while doing the clamp */
	if (clamp && !ep_sel) {
		ret = regmap_update_bits(madera->regmap,
					 MADERA_OUTPUT_ENABLES_1,
					 MADERA_OUT1L_ENA |
					 MADERA_OUT1R_ENA, 0);
		if (ret)
			dev_warn(madera->dev,
				"Failed to disable headphone outputs: %d\n",
				 ret);
	}

	if (edre_reg && !ep_sel) {
		ret = regmap_write(madera->regmap, edre_reg, edre_val);
		if (ret)
			dev_warn(madera->dev,
				"Failed to set EDRE Manual: %d\n", ret);
	}

	dev_dbg(madera->dev, "%sing clamp mask=0x%x val=0x%x\n",
		clamp ? "Set" : "Clear", mask, val);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		ret = regmap_update_bits(madera->regmap,
					 MADERA_HP_CTRL_1L, mask, val);
		if (ret)
			dev_warn(madera->dev, "Failed to do clamp: %d\n", ret);

		ret = regmap_update_bits(madera->regmap,
					 MADERA_HP_CTRL_1R, mask, val);
		if (ret)
			dev_warn(madera->dev, "Failed to do clamp: %d\n", ret);
		break;
	default:
		ret = regmap_update_bits(madera->regmap,
					 MADERA_HEADPHONE_DETECT_0,
					 MADERA_HPD_OVD_ENA_SEL_MASK,
					 val);
		if (ret)
			dev_warn(madera->dev, "Failed to do clamp: %d\n", ret);
		break;
	}

	/* Restore the desired state while not doing the clamp */
	if (!clamp && (HOHM_TO_OHM(madera->hp_impedance_x100[0]) >
			info->pdata->hpdet_short_circuit_imp) && !ep_sel) {
		ret = regmap_update_bits(madera->regmap,
					 MADERA_OUTPUT_ENABLES_1,
					 MADERA_OUT1L_ENA | MADERA_OUT1R_ENA,
					 madera->hp_ena);
		if (ret)
			dev_warn(madera->dev,
				 "Failed to restore headphone outputs: %d\n",
				 ret);
	}

	snd_soc_dapm_mutex_unlock(madera->dapm);
}

static const char *madera_ext_get_micbias_src(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int bias = info->micd_modes[info->micd_mode].bias;

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		return NULL;
	default:
		switch (bias) {
		case 0:
		case 1:
		case 2:
		case 3:
			return "MICBIAS1";
		case 4:
		case 5:
		case 6:
		case 7:
			return "MICBIAS2";
		default:
			return "MICVDD";
		}
		break;
	}
}

static const char *madera_extcon_get_micbias(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int bias = info->micd_modes[info->micd_mode].bias;

	switch (madera->type) {
	case CS47L35:
		switch (bias) {
		case 1:
			return "MICBIAS1A";
		case 2:
			return "MICBIAS1B";
		case 3:
			return "MICBIAS2A";
		default:
			return "MICVDD";
		}
	case CS47L85:
	case WM1840:
		switch (bias) {
		case 1:
			return "MICBIAS1";
		case 2:
			return "MICBIAS2";
		case 3:
			return "MICBIAS3";
		case 4:
			return "MICBIAS4";
		default:
			return "MICVDD";
		}
	default:
		switch (bias) {
		case 0:
			return "MICBIAS1A";
		case 1:
			return "MICBIAS1B";
		case 2:
			return "MICBIAS1C";
		case 3:
			return "MICBIAS1D";
		case 4:
			return "MICBIAS2A";
		case 5:
			return "MICBIAS2B";
		case 6:
			return "MICBIAS2C";
		case 7:
			return "MICBIAS2D";
		default:
			return "MICVDD";
		}
	}
}

static void madera_extcon_enable_micbias_pin(struct madera_extcon_info *info,
					     const char *widget)
{
	struct madera *madera = info->madera;
	struct snd_soc_dapm_context *dapm = madera->dapm;
	int ret;

	ret = snd_soc_dapm_force_enable_pin(dapm, widget);
	if (ret)
		dev_warn(madera->dev, "Failed to enable %s: %d\n", widget, ret);

	snd_soc_dapm_sync(dapm);

	dev_dbg(madera->dev, "Enabled %s\n", widget);
}

static void madera_extcon_disable_micbias_pin(struct madera_extcon_info *info,
					      const char *widget)
{
	struct madera *madera = info->madera;
	struct snd_soc_dapm_context *dapm = madera->dapm;
	int ret;

	ret = snd_soc_dapm_disable_pin(dapm, widget);
	if (ret)
		dev_warn(madera->dev, "Failed to enable %s: %d\n", widget, ret);

	snd_soc_dapm_sync(dapm);

	dev_dbg(madera->dev, "Disabled %s\n", widget);
}

static void madera_extcon_set_micd_bias(struct madera_extcon_info *info,
					bool enable)
{
	int old_bias = info->micd_bias.bias;
	int new_bias = info->micd_modes[info->micd_mode].bias;
	const char *widget;

	info->micd_bias.bias = new_bias;

	if ((new_bias == old_bias) && (info->micd_bias.enabled == enable))
		return;

	widget = madera_ext_get_micbias_src(info);
	WARN_ON(!widget);

	if (info->micd_bias.enabled)
		madera_extcon_disable_micbias_pin(info, widget);

	if (enable)
		madera_extcon_enable_micbias_pin(info, widget);

	info->micd_bias.enabled = enable;
}

static void madera_extcon_enable_micbias(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	const char *widget = madera_extcon_get_micbias(info);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		break;
	default:
		madera_extcon_set_micd_bias(info, true);
		break;
	}

	/* If forced we must manually control the pin state, otherwise
	 * the codec will manage this automatically
	 */
	if (info->pdata->micd_force_micbias)
		madera_extcon_enable_micbias_pin(info, widget);
}

static void madera_extcon_disable_micbias(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	const char *widget = madera_extcon_get_micbias(info);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		break;
	default:
		madera_extcon_set_micd_bias(info, false);
		break;
	}

	/* If forced we must manually control the pin state, otherwise
	 * the codec will manage this automatically
	 */
	if (info->pdata->micd_force_micbias)
		madera_extcon_disable_micbias_pin(info, widget);
}

static void madera_extcon_set_mode(struct madera_extcon_info *info, int mode)
{
	struct madera *madera = info->madera;

	dev_dbg(madera->dev,
		"mic_mode[%d] src=0x%x gnd=0x%x bias=0x%x gpio=%d hp_gnd=%d\n",
		mode, info->micd_modes[mode].src, info->micd_modes[mode].gnd,
		info->micd_modes[mode].bias, info->micd_modes[mode].gpio,
		info->micd_modes[mode].hp_gnd);

	if (info->pdata->micd_pol_gpio[0] > 0)
		gpio_set_value_cansleep(info->pdata->micd_pol_gpio[0],
					info->micd_modes[mode].gpio);
	if (info->pdata->micd_pol_gpio[1] > 0)
		gpio_set_value_cansleep(info->pdata->micd_pol_gpio[1],
					!info->micd_modes[mode].gpio);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_BIAS_SRC_MASK,
				   info->micd_modes[mode].bias <<
				   MADERA_MICD_BIAS_SRC_SHIFT);
		regmap_update_bits(madera->regmap,
				   MADERA_ACCESSORY_DETECT_MODE_1,
				   MADERA_ACCDET_SRC,
				   info->micd_modes[mode].src <<
				   MADERA_ACCDET_SRC_SHIFT);
		break;
	default:
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_BIAS_SRC_MASK,
				   info->micd_modes[mode].bias <<
				   MADERA_MICD_BIAS_SRC_SHIFT);
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_0,
				   MADERA_MICD1_SENSE_MASK,
				   info->micd_modes[mode].src <<
				   MADERA_MICD1_SENSE_SHIFT);
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_0,
				   MADERA_MICD1_GND_MASK,
				   info->micd_modes[mode].gnd <<
				   MADERA_MICD1_GND_SHIFT);
		regmap_update_bits(madera->regmap,
				   MADERA_OUTPUT_PATH_CONFIG_1,
				   MADERA_HP1_GND_SEL_MASK,
				   info->micd_modes[mode].hp_gnd <<
				   MADERA_HP1_GND_SEL_SHIFT);
		break;
	}

	info->micd_mode = mode;
}

static void madera_extcon_next_mode(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int old_mode = info->micd_mode;
	int new_mode;
	bool change_bias = false;

	new_mode = (old_mode + 1) % info->micd_num_modes;

	dev_dbg(madera->dev, "change micd mode %d->%d (bias %d->%d)\n",
		old_mode, new_mode,
		info->micd_modes[old_mode].bias,
		info->micd_modes[new_mode].bias);

	if (info->micd_modes[old_mode].bias !=
	    info->micd_modes[new_mode].bias) {
		change_bias = true;

		madera_extcon_disable_micbias(info);
	}

	madera_extcon_set_mode(info, new_mode);

	if (change_bias)
		madera_extcon_enable_micbias(info);
}

static int madera_micd_adc_read(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int val = 0;
	int ret;

	/* Must disable MICD before we read the ADCVAL */
	ret = regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
				 MADERA_MICD_ENA, 0);
	if (ret) {
		dev_err(madera->dev, "Failed to disable MICD: %d\n", ret);
		return ret;
	}

	ret = regmap_read(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_4, &val);
	if (ret) {
		dev_err(madera->dev, "Failed to read MICDET_ADCVAL: %d\n", ret);
		return ret;
	}

	dev_dbg(madera->dev, "MICDET_ADCVAL: 0x%x\n", val);

	val &= MADERA_MICDET_ADCVAL_MASK;
	if (val < ARRAY_SIZE(madera_micd_levels))
		val = madera_micd_levels[val];
	else
		val = INT_MAX;

	return val;
}

static int madera_micd_read(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int val = 0;
	int ret, i;

	for (i = 0; i < 10 && !(val & MADERA_MICD_LVL_0_TO_8); i++) {
		ret = regmap_read(madera->regmap,
				  MADERA_MIC_DETECT_1_CONTROL_3, &val);
		if (ret) {
			dev_err(madera->dev,
				"Failed to read MICDET: %d\n", ret);
			return ret;
		}

		dev_dbg(madera->dev, "MICDET: 0x%x\n", val);

		if (!(val & MADERA_MICD_VALID)) {
			dev_warn(madera->dev,
				 "Microphone detection state invalid\n");
			return -EINVAL;
		}
	}

	if (i == 10 && !(val & MADERA_MICD_LVL_0_TO_8)) {
		dev_warn(madera->dev, "Failed to get valid MICDET value\n");
		return -EINVAL;
	}

	if (!(val & MADERA_MICD_STS)) {
		val = INT_MAX;
	} else if (!(val & MADERA_MICD_LVL_0_TO_7)) {
		val = madera_micd_levels[ARRAY_SIZE(madera_micd_levels) - 1];
	} else {
		int lvl;

		lvl = (val & MADERA_MICD_LVL_MASK) >> MADERA_MICD_LVL_SHIFT;
		lvl = ffs(lvl) - 1;

		if (lvl < info->num_micd_ranges) {
			val = info->micd_ranges[lvl].max;
		} else {
			i = ARRAY_SIZE(madera_micd_levels) - 2;
			val = madera_micd_levels[i];
		}
	}

	return val;
}

static void madera_extcon_notify_micd(const struct madera_extcon_info *info,
				      bool present,
				      unsigned int impedance)
{
	struct madera_micdet_notify_data data;

	data.present = present;
	data.impedance_x100 = OHM_TO_HOHM(impedance);
	data.out_num = 1;

	madera_call_notifiers(info->madera, MADERA_NOTIFY_MICDET, &data);
}

static int madera_hpdet_calc_calibration(const struct madera_extcon_info *info,
			int dacval, const struct madera_hpdet_trims *trims,
			const struct madera_hpdet_calibration_data *calib)
{
	int grad_x4 = trims->grad_x4;
	int off_x4 = trims->off_x4;
	s64 val = dacval;
	s64 n;

	val = (val * 1000000) + calib->dacval_adjust;
	val = div64_s64(val, calib->C2);

	n = div_s64(1000000000000LL, calib->C3 +
			((calib->C4_x_C3 * grad_x4) / 4));
	n = val - n;
	if (n <= 0)
		return MADERA_HPDET_MAX;

	val = calib->C0 + ((calib->C1 * off_x4) / 4);
	val *= 1000000;

	val = div_s64(val, n);
	val -= calib->C5;

	/* Round up */
	val += 500000;
	val = div_s64(val, 1000000);

	if (val < 0)
		return 0;
	else if (val > MADERA_HPDET_MAX)
		return MADERA_HPDET_MAX;

	return (int)val;
}

static int madera_hpdet_calibrate(struct madera_extcon_info *info,
				  unsigned int range, unsigned int *val)
{
	struct madera *madera = info->madera;
	unsigned int dacval, dacval_down;
	int ret;

	ret = regmap_read(madera->regmap, MADERA_HEADPHONE_DETECT_3, &dacval);
	if (ret) {
		dev_err(madera->dev, "Failed to read HP DACVAL: %d\n", ret);
		return -EAGAIN;
	}

	dacval = (dacval >> MADERA_HP_DACVAL_SHIFT) & MADERA_HP_DACVAL_MASK;

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		ret = regmap_read(madera->regmap, MADERA_HEADPHONE_DETECT_5,
				  &dacval_down);
		if (ret) {
			dev_err(madera->dev,
				"Failed to read HP DACVAL_down: %d\n", ret);
			return -EAGAIN;
		}

		dacval_down = (dacval_down >> MADERA_HP_DACVAL_DOWN_SHIFT) &
				MADERA_HP_DACVAL_DOWN_MASK;

		dacval = (dacval + dacval_down) / 2;
		break;
	default:
		break;
	}

	dev_dbg(info->madera->dev,
		"hpdet_d calib range %d dac %d\n", range, dacval);

	*val = madera_hpdet_calc_calibration(info, dacval,
					     &info->hpdet_trims[range],
					     &info->hpdet_ranges[range]);
	return 0;
}

static int madera_hpdet_read(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int val, range, sense_pin;
	int ret;
	bool is_jdx_micdetx_pin = false;

	dev_dbg(madera->dev, "HPDET read\n");

	ret = regmap_read(madera->regmap, MADERA_HEADPHONE_DETECT_2, &val);
	if (ret) {
		dev_err(madera->dev, "Failed to read HPDET status: %d\n", ret);
		return ret;
	}

	if (!(val & MADERA_HP_DONE_B)) {
		dev_warn(madera->dev, "HPDET did not complete: %x\n", val);
		return -EAGAIN;
	}

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		break;
	default:
		regmap_read(madera->regmap, MADERA_HEADPHONE_DETECT_0,
			    &sense_pin);
		sense_pin = (sense_pin & MADERA_HPD_SENSE_SEL_MASK) >>
			    MADERA_HPD_SENSE_SEL_SHIFT;

		switch (sense_pin) {
		case MADERA_HPD_SENSE_HPDET1:
		case MADERA_HPD_SENSE_HPDET2:
			is_jdx_micdetx_pin = false;
			break;
		default:
			dev_dbg(madera->dev, "is_jdx_micdetx_pin\n");
			is_jdx_micdetx_pin = true;
		}
		break;
	}

	val &= MADERA_HP_LVL_B_MASK;
	/* Convert to ohms, the value is in 0.5 ohm increments */
	val /= 2;

	if (is_jdx_micdetx_pin)
		goto done;

	regmap_read(madera->regmap, MADERA_HEADPHONE_DETECT_1, &range);
	range = (range & MADERA_HP_IMPEDANCE_RANGE_MASK) >>
		MADERA_HP_IMPEDANCE_RANGE_SHIFT;

	/* Skip up a range, or report? */
	if (range < info->num_hpdet_ranges - 1 &&
	    (val >= info->hpdet_ranges[range].max)) {
		range++;
		dev_dbg(madera->dev, "Moving to HPDET range %d-%d\n",
			info->hpdet_ranges[range].min,
			info->hpdet_ranges[range].max);

		regmap_update_bits(madera->regmap,
				   MADERA_HEADPHONE_DETECT_1,
				   MADERA_HP_IMPEDANCE_RANGE_MASK,
				   range <<
				   MADERA_HP_IMPEDANCE_RANGE_SHIFT);
		return -EAGAIN;
	}

	if (info->hpdet_trims) {
		/* Perform calibration */
		ret = madera_hpdet_calibrate(info, range, &val);
		if (ret)
			return ret;
	} else {
		/* Use uncalibrated reading */
		if (range && (val < info->hpdet_ranges[range].min)) {
			dev_dbg(madera->dev,
				"Reporting range boundary %d\n",
				info->hpdet_ranges[range].min);
			val = info->hpdet_ranges[range].min;
		}
	}

	if (info->pdata->hpdet_ext_res_x100) {
		if (info->pdata->hpdet_ext_res_x100 >= OHM_TO_HOHM(val)) {
			dev_dbg(madera->dev,
				"External resistor (%d.%02d) >= measurement (%d.00)\n",
				info->pdata->hpdet_ext_res_x100 / 100,
				info->pdata->hpdet_ext_res_x100 % 100,
				val);
			val = 0;	/* treat as a short */
		} else {
			dev_dbg(madera->dev,
				"Compensating for external %d.%02d ohm resistor\n",
				info->pdata->hpdet_ext_res_x100 / 100,
				info->pdata->hpdet_ext_res_x100 % 100);

			val -= HOHM_TO_OHM(info->pdata->hpdet_ext_res_x100);
		}
	}

done:
	dev_dbg(madera->dev, "HP impedance %d ohms\n", val);
	return (int)val;
}

static int madera_tune_headphone(struct madera_extcon_info *info, int reading)
{
	struct madera *madera = info->madera;
	const struct madera_hp_tuning *tuning;
	int n_tunings;
	int i, ret;

	switch (madera->type) {
	case CS47L35:
		tuning = cs47l35_hp_tuning;
		n_tunings = ARRAY_SIZE(cs47l35_hp_tuning);
		break;
	case CS47L85:
	case WM1840:
		tuning = cs47l85_hp_tuning;
		n_tunings = ARRAY_SIZE(cs47l85_hp_tuning);
		break;
	case CS47L90:
	case CS47L91:
		tuning = cs47l90_hp_tuning;
		n_tunings = ARRAY_SIZE(cs47l90_hp_tuning);
		break;
	default:
		return 0;
	}

	if (reading <= info->pdata->hpdet_short_circuit_imp) {
		/* Headphones are always off here so just mark them */
		dev_warn(madera->dev, "Possible HP short, disabling\n");
		return 0;
	}

	/* Check for tuning, we don't need to compare against the last
	 * tuning entry because we always select that if reading is not
	 * in range of the lower tunings
	 */
	for (i = 0; i < n_tunings - 1; ++i) {
		if (reading <= tuning[i].max_ohm)
			break;
	}

	if (info->hp_tuning_level != i) {
		dev_dbg(madera->dev, "New tuning level %d\n", i);

		info->hp_tuning_level = i;

		ret = regmap_multi_reg_write(madera->regmap,
					     tuning[i].patch,
					     tuning[i].patch_len);
		if (ret) {
			dev_err(madera->dev,
				"Failed to apply HP tuning %d\n", ret);
			return ret;
		}
	}

	return 0;
}

void madera_set_headphone_imp(struct madera_extcon_info *info, int imp)
{
	struct madera *madera = info->madera;
	struct madera_hpdet_notify_data data;

	madera->hp_impedance_x100[0] = imp;

	data.impedance_x100 = imp;
	madera_call_notifiers(madera, MADERA_NOTIFY_HPDET, &data);

	madera_tune_headphone(info, HOHM_TO_OHM(imp));
}
EXPORT_SYMBOL_GPL(madera_set_headphone_imp);

static void madera_hpdet_start_micd(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;

	regmap_update_bits(madera->regmap, MADERA_IRQ1_MASK_6,
			   MADERA_IM_MICDET1_EINT1_MASK,
			   MADERA_IM_MICDET1_EINT1);
	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_0,
			   MADERA_MICD1_ADC_MODE_MASK,
			   MADERA_MICD1_ADC_MODE_MASK);
	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_BIAS_STARTTIME_MASK |
			   MADERA_MICD_RATE_MASK |
			   MADERA_MICD_DBTIME_MASK |
			   MADERA_MICD_ENA, MADERA_MICD_ENA);
}

static void madera_hpdet_stop_micd(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	unsigned int start_time = 1, dbtime = 1, rate = 1;

	if (info->pdata->micd_bias_start_time)
		start_time = info->pdata->micd_bias_start_time;

	if (info->pdata->micd_rate)
		rate = info->pdata->micd_rate;

	if (info->pdata->micd_dbtime)
		dbtime = info->pdata->micd_dbtime;

	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_BIAS_STARTTIME_MASK |
			   MADERA_MICD_RATE_MASK |
			   MADERA_MICD_DBTIME_MASK |
			   MADERA_MICD_ENA,
			   start_time << MADERA_MICD_BIAS_STARTTIME_SHIFT |
			   rate << MADERA_MICD_RATE_SHIFT |
			   dbtime << MADERA_MICD_DBTIME_SHIFT);

	udelay(100);

	/* Clear any spurious IRQs that have happened */
	regmap_write(madera->regmap, MADERA_IRQ1_STATUS_6,
		     MADERA_MICDET1_EINT1);
	regmap_update_bits(madera->regmap, MADERA_IRQ1_MASK_6,
		     MADERA_IM_MICDET1_EINT1_MASK, 0);
}

int madera_hpdet_start(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int ret;
	unsigned int hpd_sense, hpd_clamp, val, hpd_gnd;

	dev_dbg(madera->dev, "Starting HPDET\n");

	/* If we specified to assume a fixed impedance skip HPDET */
	if (info->pdata->fixed_hpdet_imp_x100) {
		madera_set_headphone_imp(info,
					 info->pdata->fixed_hpdet_imp_x100);
		ret = -EEXIST;
		goto skip;
	}

	/* Make sure we keep the device enabled during the measurement */
	pm_runtime_get_sync(info->dev);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		madera_extcon_hp_clamp(info, true);
		ret = regmap_update_bits(madera->regmap,
					 MADERA_ACCESSORY_DETECT_MODE_1,
					 MADERA_ACCDET_MODE_MASK,
					 info->state->mode);
		if (ret) {
			dev_err(madera->dev, "Failed to set HPDET mode (%d): %d\n",
				info->state->mode, ret);
			goto err;
		}
		break;
	default:
		if (info->state->mode == MADERA_ACCDET_MODE_HPL) {
			hpd_clamp = info->pdata->hpd_pins[0];
			hpd_sense = info->pdata->hpd_pins[1];
		} else {
			hpd_clamp = info->pdata->hpd_pins[2];
			hpd_sense = info->pdata->hpd_pins[3];
		}

		hpd_gnd = info->micd_modes[info->micd_mode].gnd;

		val = (hpd_sense << MADERA_HPD_SENSE_SEL_SHIFT) |
				(hpd_clamp << MADERA_HPD_OUT_SEL_SHIFT) |
				(hpd_sense << MADERA_HPD_FRC_SEL_SHIFT) |
				(hpd_gnd << MADERA_HPD_GND_SEL_SHIFT);

		ret = regmap_update_bits(madera->regmap,
					 MADERA_HEADPHONE_DETECT_0,
					 MADERA_HPD_GND_SEL_MASK |
					 MADERA_HPD_SENSE_SEL_MASK |
					 MADERA_HPD_FRC_SEL_MASK |
					 MADERA_HPD_OUT_SEL_MASK,
				val);
		if (ret) {
			dev_err(madera->dev,
				"Failed to set HPDET sense: %d\n", ret);
			goto err;
		}
		madera_extcon_hp_clamp(info, true);
		madera_hpdet_start_micd(info);
		break;
	}

	ret = regmap_update_bits(madera->regmap, MADERA_HEADPHONE_DETECT_1,
				 MADERA_HP_POLL, MADERA_HP_POLL);
	if (ret) {
		dev_err(madera->dev, "Can't start HPDET measurement: %d\n",
			ret);
		goto err;
	}

	return 0;

err:
	madera_extcon_hp_clamp(info, false);

	pm_runtime_put_autosuspend(info->dev);

skip:
	return ret;
}
EXPORT_SYMBOL_GPL(madera_hpdet_start);

void madera_hpdet_restart(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;

	/* Reset back to starting range */
	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_ENA_MASK, 0);

	regmap_update_bits(madera->regmap, MADERA_HEADPHONE_DETECT_1,
			   MADERA_HP_IMPEDANCE_RANGE_MASK | MADERA_HP_POLL, 0);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		break;
	default:
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_ENA_MASK, MADERA_MICD_ENA);
		break;
	}

	regmap_update_bits(madera->regmap, MADERA_HEADPHONE_DETECT_1,
			   MADERA_HP_POLL, MADERA_HP_POLL);
}
EXPORT_SYMBOL_GPL(madera_hpdet_restart);

void madera_hpdet_stop(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;

	dev_dbg(madera->dev, "Stopping HPDET\n");

	/* Reset back to starting range */
	madera_hpdet_stop_micd(info);

	regmap_update_bits(madera->regmap, MADERA_HEADPHONE_DETECT_1,
			   MADERA_HP_IMPEDANCE_RANGE_MASK | MADERA_HP_POLL, 0);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		/* Reset to default mode */
		regmap_update_bits(madera->regmap,
				   MADERA_ACCESSORY_DETECT_MODE_1,
				   MADERA_ACCDET_MODE_MASK, 0);
		break;
	default:
		break;
	}

	madera_extcon_hp_clamp(info, false);

	pm_runtime_mark_last_busy(info->dev);
	pm_runtime_put_autosuspend(info->dev);
}
EXPORT_SYMBOL_GPL(madera_hpdet_stop);

int madera_hpdet_reading(struct madera_extcon_info *info, int val)
{
	dev_dbg(info->madera->dev, "Reading HPDET %d\n", val);

	if (val < 0)
		return val;

	madera_set_headphone_imp(info, val);

	if (info->have_mic) {
		madera_extcon_report(info, BIT_HEADSET);
		madera_jds_set_state(info, &madera_micd_button);
	} else {
		madera_extcon_report(info, BIT_HEADSET_NO_MIC);
		madera_extcon_report(info, madera_is_lineout(info) ?
				     BIT_LINEOUT : BIT_HEADSET_NO_MIC);
		madera_jds_set_state(info, NULL);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(madera_hpdet_reading);

int madera_micd_start(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int ret;
	unsigned int micd_mode;

	/* Microphone detection can't use idle mode */
	pm_runtime_get_sync(info->dev);

	dev_dbg(madera->dev, "Disabling MICD_OVD\n");
	regmap_update_bits(madera->regmap,
			   MADERA_MICD_CLAMP_CONTROL,
			   MADERA_MICD_CLAMP_OVD_MASK, 0);

	ret = regulator_enable(info->micvdd);
	if (ret)
		dev_err(madera->dev, "Failed to enable MICVDD: %d\n", ret);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		regmap_update_bits(madera->regmap,
				   MADERA_ACCESSORY_DETECT_MODE_1,
				   MADERA_ACCDET_MODE_MASK, info->state->mode);
		break;
	default:
		if (info->state->mode == MADERA_ACCDET_MODE_ADC)
			micd_mode = MADERA_MICD1_ADC_MODE_MASK;
		else
			micd_mode = 0;

		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_0,
				   MADERA_MICD1_ADC_MODE_MASK, micd_mode);
		break;
	}

	madera_extcon_enable_micbias(info);

	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_ENA, MADERA_MICD_ENA);

	return 0;
}
EXPORT_SYMBOL_GPL(madera_micd_start);

void madera_micd_stop(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;

	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_ENA, 0);

	madera_extcon_disable_micbias(info);

	switch (madera->type) {
	case CS47L35:
	case CS47L85:
	case WM1840:
		/* Reset to default mode */
		regmap_update_bits(madera->regmap,
				   MADERA_ACCESSORY_DETECT_MODE_1,
				   MADERA_ACCDET_MODE_MASK, 0);
		break;
	default:
		break;
	}

	regulator_disable(info->micvdd);

	dev_dbg(madera->dev, "Enabling MICD_OVD\n");
	regmap_update_bits(madera->regmap, MADERA_MICD_CLAMP_CONTROL,
			   MADERA_MICD_CLAMP_OVD_MASK, MADERA_MICD_CLAMP_OVD);

	pm_runtime_mark_last_busy(info->dev);
	pm_runtime_put_autosuspend(info->dev);
}
EXPORT_SYMBOL_GPL(madera_micd_stop);

static void madera_micd_restart(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;

	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_ENA, 0);
	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_1,
			   MADERA_MICD_ENA, MADERA_MICD_ENA);
}

static int madera_micd_button_debounce(struct madera_extcon_info *info, int val)
{
	struct madera *madera = info->madera;
	int debounce_lim = info->pdata->micd_manual_debounce;

	if (debounce_lim) {
		if (info->micd_debounce != val)
			info->micd_count = 0;

		info->micd_debounce = val;
		info->micd_count++;

		if (info->micd_count == debounce_lim) {
			info->micd_count = 0;
			if (val == info->micd_res_old)
				return 0;

			info->micd_res_old = val;
		} else {
			dev_dbg(madera->dev, "Software debounce: %d,%x\n",
				info->micd_count, val);
			madera_micd_restart(info);
			return -EAGAIN;
		}
	}

	return 0;
}

static int madera_micd_button_process(struct madera_extcon_info *info, int val)
{
	struct madera *madera = info->madera;
	int i, key;

	if (val < MADERA_MICROPHONE_MIN_OHM) {
		dev_dbg(madera->dev, "Mic button detected\n");

		for (i = 0; i < info->num_micd_ranges; i++)
			input_report_key(info->input,
					 info->micd_ranges[i].key, 0);

		for (i = 0; i < info->num_micd_ranges; i++) {
			if (val <= info->micd_ranges[i].max) {
				key = info->micd_ranges[i].key;
				dev_dbg(madera->dev, "Key %d down\n", key);
				input_report_key(info->input, key, 1);
				input_sync(info->input);
				break;
			}
		}

		if (i == info->num_micd_ranges)
			dev_warn(madera->dev,
				 "Button level %u out of range\n", val);
	} else {
		dev_dbg(madera->dev, "Mic button released\n");

		for (i = 0; i < info->num_micd_ranges; i++)
			input_report_key(info->input,
					 info->micd_ranges[i].key, 0);
		input_sync(info->input);
	}

	return 0;
}

int madera_micd_button_reading(struct madera_extcon_info *info, int val)
{
	int ret;

	if (val < 0)
		return val;

	val = HOHM_TO_OHM(val);

	ret = madera_micd_button_debounce(info, val);
	if (ret < 0)
		return ret;

	return madera_micd_button_process(info, val);
}
EXPORT_SYMBOL_GPL(madera_micd_button_reading);

int madera_micd_mic_start(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int ret;

	info->detecting = true;

	ret = regulator_allow_bypass(info->micvdd, false);
	if (ret)
		dev_err(madera->dev, "Failed to regulate MICVDD: %d\n", ret);

	return madera_micd_start(info);
}
EXPORT_SYMBOL_GPL(madera_micd_mic_start);

void madera_micd_mic_stop(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int ret;

	madera_micd_stop(info);

	ret = regulator_allow_bypass(info->micvdd, true);
	if (ret)
		dev_err(madera->dev, "Failed to bypass MICVDD: %d\n", ret);

	info->detecting = false;
}
EXPORT_SYMBOL_GPL(madera_micd_mic_stop);

int madera_micd_mic_reading(struct madera_extcon_info *info, int val)
{
	struct madera *madera = info->madera;
	int ret;

	if (val < 0)
		return val;

	val = HOHM_TO_OHM(val);

	/* Due to jack detect this should never happen */
	if (val > MADERA_MICROPHONE_MAX_OHM) {
		dev_warn(madera->dev, "Detected open circuit\n");
		info->have_mic = info->pdata->micd_open_circuit_declare;
		goto done;
	}

	/* If we got a high impedence we should have a headset, report it. */
	if (val >= MADERA_MICROPHONE_MIN_OHM) {
		dev_dbg(madera->dev, "Detected headset\n");
		info->have_mic = true;
		goto done;
	}

	/* If we detected a lower impedence during initial startup
	 * then we probably have the wrong polarity, flip it.  Don't
	 * do this for the lowest impedences to speed up detection of
	 * plain headphones.  If both polarities report a low
	 * impedence then give up and report headphones.
	 */
	if (((info->micd_ranges[0].max > MADERA_MICD_WRONG_POLARITY) ||
	    (val > info->micd_ranges[0].max)) &&
	    (info->micd_num_modes > 1)) {
		if (info->jack_flips >= info->micd_num_modes * 10) {
			dev_dbg(madera->dev, "Detected HP/line\n");
			goto done;
		} else {
			madera_extcon_next_mode(info);

			info->jack_flips++;

			return -EAGAIN;
		}
	}

	/*
	 * If we're still detecting and we detect a short then we've
	 * got a headphone.
	 */
	dev_dbg(madera->dev, "Headphone detected\n");

done:
	pm_runtime_mark_last_busy(info->dev);

	if (info->pdata->hpdet_channel)
		ret = madera_jds_set_state(info, &madera_hpdet_right);
	else
		ret = madera_jds_set_state(info, &madera_hpdet_left);
	if (ret < 0) {
		if (info->have_mic)
			madera_extcon_report(info, BIT_HEADSET);
		else
			madera_extcon_report(info, madera_is_lineout(info) ?
					     BIT_LINEOUT : BIT_HEADSET_NO_MIC);
	}

	madera_extcon_notify_micd(info, info->have_mic, val);

	return 0;
}
EXPORT_SYMBOL_GPL(madera_micd_mic_reading);

int madera_micd_mic_timeout_ms(struct madera_extcon_info *info)
{
	if (info->pdata->micd_timeout_ms)
		return info->pdata->micd_timeout_ms;
	else
		return MADERA_DEFAULT_MICD_TIMEOUT_MS;
}
EXPORT_SYMBOL_GPL(madera_micd_mic_timeout_ms);

void madera_micd_mic_timeout(struct madera_extcon_info *info)
{
	int ret;

	dev_dbg(info->madera->dev, "MICD timed out, reporting HP\n");

	if (info->pdata->hpdet_channel)
		ret = madera_jds_set_state(info, &madera_hpdet_right);
	else
		ret = madera_jds_set_state(info, &madera_hpdet_left);
	if (ret < 0)
		madera_extcon_report(info, madera_is_lineout(info) ?
				     BIT_LINEOUT : BIT_HEADSET_NO_MIC);
}
EXPORT_SYMBOL_GPL(madera_micd_mic_timeout);

static int madera_jack_present(struct madera_extcon_info *info,
				unsigned int *jack_val)
{
	struct madera *madera = info->madera;
	unsigned int present, val;
	int ret;

	ret = regmap_read(madera->regmap, MADERA_IRQ1_RAW_STATUS_7, &val);
	if (ret) {
		dev_err(madera->dev,
			"Failed to read jackdet status: %d\n", ret);
		return ret;
	}

	dev_dbg(madera->dev, "IRQ1_RAW_STATUS_7=0x%x\n", val);

	if (info->pdata->jd_use_jd2) {
		val &= MADERA_MICD_CLAMP_RISE_STS1;
		present = 0;
	} else if (info->pdata->jd_invert) {
		val &= MADERA_JD1_FALL_STS1_MASK;
		present = MADERA_JD1_FALL_STS1;
	} else {
		val &= MADERA_JD1_RISE_STS1_MASK;
		present = MADERA_JD1_RISE_STS1;
	}

	dev_dbg(madera->dev, "jackdet val=0x%x present=0x%x\n", val, present);

	if (jack_val)
		*jack_val = val;

	if (val == present)
		return 1;
	else
		return 0;
}

static irqreturn_t madera_hpdet_handler(int irq, void *data)
{
	struct madera_extcon_info *info = data;
	struct madera *madera = info->madera;
	int ret;

	dev_dbg(madera->dev, "HPDET handler\n");

	madera_jds_cancel_timeout(info);

	mutex_lock(&info->lock);

	switch (madera_jds_get_mode(info)) {
	case MADERA_ACCDET_MODE_HPL:
	case MADERA_ACCDET_MODE_HPR:
	case MADERA_ACCDET_MODE_HPM:
		/* Fall through to spurious if no jack present */
		if (madera_jack_present(info, NULL) > 0)
			break;
	default:
		dev_warn(madera->dev, "Spurious HPDET IRQ\n");
		madera_jds_start_timeout(info);
		mutex_unlock(&info->lock);
		return IRQ_NONE;
	}

	ret = madera_hpdet_read(info);
	if (ret == -EAGAIN)
		goto out;

	ret = OHM_TO_HOHM(ret);
	madera_jds_reading(info, ret);

out:
	madera_jds_start_timeout(info);

	pm_runtime_mark_last_busy(info->dev);

	mutex_unlock(&info->lock);

	return IRQ_HANDLED;
}

static void madera_micd_handler(struct work_struct *work)
{
	struct madera_extcon_info *info =
		container_of(work, struct madera_extcon_info,
			     micd_detect_work.work);
	struct madera *madera = info->madera;
	enum madera_accdet_mode mode;
	int ret;

	madera_jds_cancel_timeout(info);

	mutex_lock(&info->lock);

	/* Must check that we are in a micd state before accessing
	 * any codec registers
	 */
	mode = madera_jds_get_mode(info);
	switch (mode) {
	case MADERA_ACCDET_MODE_MIC:
	case MADERA_ACCDET_MODE_ADC:
		break;
	default:
		goto spurious;
	}

	if (madera_jack_present(info, NULL) <= 0)
		goto spurious;

	switch (mode) {
	case MADERA_ACCDET_MODE_MIC:
		ret = madera_micd_read(info);
		break;
	case MADERA_ACCDET_MODE_ADC:
		ret = madera_micd_adc_read(info);
		break;
	default:	/* we can't get here but compiler still warns */
		ret = 0;
		break;
	}

	if (ret == -EAGAIN)
		goto out;

	dev_dbg(madera->dev, "Mic impedance %d ohms\n", ret);

	madera_jds_reading(info, OHM_TO_HOHM(ret));

out:
	madera_jds_start_timeout(info);

	pm_runtime_mark_last_busy(info->dev);

	mutex_unlock(&info->lock);

	return;

spurious:
	dev_warn(madera->dev, "Spurious MICDET IRQ\n");
	madera_jds_start_timeout(info);
	mutex_unlock(&info->lock);
}

static irqreturn_t madera_micdet(int irq, void *data)
{
	struct madera_extcon_info *info = data;
	struct madera *madera = info->madera;
	int debounce = info->pdata->micd_detect_debounce_ms;

	dev_dbg(madera->dev, "micdet IRQ");

	cancel_delayed_work_sync(&info->micd_detect_work);

	mutex_lock(&info->lock);

	if (!info->detecting)
		debounce = 0;

	mutex_unlock(&info->lock);

	/* Defer to the workqueue to ensure serialization
	 * and prevent race conditions if an IRQ occurs while
	 * running the delayed work
	 */
	schedule_delayed_work(&info->micd_detect_work,
				msecs_to_jiffies(debounce));

	return IRQ_HANDLED;
}

const struct madera_jd_state madera_hpdet_left = {
	.mode = MADERA_ACCDET_MODE_HPL,
	.start = madera_hpdet_start,
	.reading = madera_hpdet_reading,
	.stop = madera_hpdet_stop,
};
EXPORT_SYMBOL_GPL(madera_hpdet_left);

const struct madera_jd_state madera_hpdet_right = {
	.mode = MADERA_ACCDET_MODE_HPR,
	.start = madera_hpdet_start,
	.reading = madera_hpdet_reading,
	.stop = madera_hpdet_stop,
};
EXPORT_SYMBOL_GPL(madera_hpdet_right);

const struct madera_jd_state madera_micd_button = {
	.mode = MADERA_ACCDET_MODE_MIC,
	.start = madera_micd_start,
	.reading = madera_micd_button_reading,
	.stop = madera_micd_stop,
};
EXPORT_SYMBOL_GPL(madera_micd_button);

const struct madera_jd_state madera_micd_adc_mic = {
	.mode = MADERA_ACCDET_MODE_ADC,
	.start = madera_micd_mic_start,
	.restart = madera_micd_restart,
	.reading = madera_micd_mic_reading,
	.stop = madera_micd_mic_stop,

	.timeout_ms = madera_micd_mic_timeout_ms,
	.timeout = madera_micd_mic_timeout,
};
EXPORT_SYMBOL_GPL(madera_micd_adc_mic);

const struct madera_jd_state madera_micd_microphone = {
	.mode = MADERA_ACCDET_MODE_MIC,
	.start = madera_micd_mic_start,
	.reading = madera_micd_mic_reading,
	.stop = madera_micd_mic_stop,

	.timeout_ms = madera_micd_mic_timeout_ms,
	.timeout = madera_micd_mic_timeout,
};
EXPORT_SYMBOL_GPL(madera_micd_microphone);

static irqreturn_t madera_jackdet(int irq, void *data)
{
	struct madera_extcon_info *info = data;
	struct madera *madera = info->madera;
	unsigned int val, mask;
	bool cancelled_state;
	int i, present;

	dev_dbg(madera->dev, "jackdet IRQ");

	cancelled_state = madera_jds_cancel_timeout(info);

	pm_runtime_get_sync(info->dev);

	mutex_lock(&info->lock);

	val = 0;
	present = madera_jack_present(info, &val);
	if (present < 0) {
		mutex_unlock(&info->lock);
		pm_runtime_put_autosuspend(info->dev);
		return IRQ_NONE;
	}

	if (val == info->last_jackdet) {
		dev_dbg(madera->dev, "Suppressing duplicate JACKDET\n");
		if (cancelled_state)
			madera_jds_start_timeout(info);

		goto out;
	}
	info->last_jackdet = val;

	mask = MADERA_MICD_CLAMP_DB | MADERA_JD1_DB;

	if (info->pdata->jd_use_jd2)
		mask |= MADERA_JD2_DB;

	if (present) {
		dev_dbg(madera->dev, "Detected jack\n");

		if (info->pdata->jd_wake_time)
			__pm_wakeup_event(&info->detection_wake_lock,
					  info->pdata->jd_wake_time);

		info->have_mic = false;
		info->jack_flips = 0;

		if (info->pdata->custom_jd)
			madera_jds_set_state(info, info->pdata->custom_jd);
		else if (info->pdata->micd_software_compare)
			madera_jds_set_state(info, &madera_micd_adc_mic);
		else
			madera_jds_set_state(info, &madera_micd_microphone);

		madera_jds_start_timeout(info);

		regmap_update_bits(madera->regmap, MADERA_INTERRUPT_DEBOUNCE_7,
				   mask, 0);
	} else {
		dev_dbg(madera->dev, "Detected jack removal\n");

		info->have_mic = false;
		info->micd_res_old = 0;
		info->micd_debounce = 0;
		info->micd_count = 0;
		madera_jds_set_state(info, NULL);

		for (i = 0; i < info->num_micd_ranges; i++)
			input_report_key(info->input,
					 info->micd_ranges[i].key, 0);
		input_sync(info->input);

		madera_extcon_report(info, BIT_NO_HEADSET);

		regmap_update_bits(madera->regmap, MADERA_INTERRUPT_DEBOUNCE_7,
				   mask, mask);

		madera_set_headphone_imp(info, MADERA_HP_Z_OPEN);

		madera_extcon_notify_micd(info, false, 0);
	}

out:
	mutex_unlock(&info->lock);

	pm_runtime_mark_last_busy(info->dev);
	pm_runtime_put_autosuspend(info->dev);

	return IRQ_HANDLED;
}

/* Map a level onto a slot in the register bank */
static void madera_micd_set_level(struct madera *madera, int index,
				  unsigned int level)
{
	int reg;
	unsigned int mask;

	reg = MADERA_MIC_DETECT_1_LEVEL_4 - (index / 2);

	if (!(index % 2)) {
		mask = 0x3f00;
		level <<= 8;
	} else {
		mask = 0x3f;
	}

	/* Program the level itself */
	regmap_update_bits(madera->regmap, reg, mask, level);
}

#ifdef CONFIG_OF
static void madera_extcon_of_get_int(struct device_node *node, const char *prop,
				     int *value)
{
	u32 v;

	if (of_property_read_u32(node, prop, &v) == 0)
		*value = v;
}

static int madera_extcon_of_get_u32_num_groups(struct madera *madera,
						struct device_node *node,
						const char *prop,
						int group_size)
{
	int len_prop, num_groups;

	if (!of_get_property(node, prop, &len_prop))
		return -EINVAL;

	num_groups =  len_prop / (group_size * sizeof(u32));

	if (num_groups * group_size * sizeof(u32) != len_prop) {
		dev_err(madera->dev,
			"DT property %s is malformed: %d\n", prop, -EOVERFLOW);
		return -EOVERFLOW;
	}

	return num_groups;
}

static int madera_extcon_of_read_part_array(struct madera *madera,
					    struct device_node *node,
					    const char *prop,
					    int index, u32 *out, int num)
{
	int ret;

	for (; num > 0; --num) {
		ret = of_property_read_u32_index(node, prop, index++, out++);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int madera_extcon_of_get_micd_ranges(struct madera *madera,
					    struct device_node *node,
					    struct madera_accdet_pdata *pdata)
{
	struct madera_micd_range *micd_ranges;
	u32 values[2];
	int nranges, i;
	int ret = 0;

	nranges = madera_extcon_of_get_u32_num_groups(madera, node,
						      "cirrus,micd-ranges", 2);
	if (nranges < 0)
		return nranges;

	micd_ranges = devm_kcalloc(madera->dev,
				   nranges,
				   sizeof(struct madera_micd_range),
				   GFP_KERNEL);

	for (i = 0; i < nranges; ++i) {
		ret = madera_extcon_of_read_part_array(madera, node,
							"cirrus,micd-ranges",
							i * 2, values, 2);
		if (ret < 0)
			goto error;

		micd_ranges[i].max = values[0];
		micd_ranges[i].key = values[1];
	}

	pdata->micd_ranges = micd_ranges;
	pdata->num_micd_ranges = nranges;

	return ret;

error:
	devm_kfree(madera->dev, micd_ranges);
	dev_err(madera->dev,
		"DT property cirrus,micd-ranges is malformed: %d\n", ret);
	return ret;
}

static int madera_extcon_of_get_micd_configs(struct madera *madera,
					     struct device_node *node,
					     struct madera_accdet_pdata *pdata)
{
	struct madera_micd_config *micd_configs;
	u32 values[5];
	int nconfigs, i;
	int ret = 0;

	nconfigs = madera_extcon_of_get_u32_num_groups(madera, node,
							"cirrus,micd-configs",
							ARRAY_SIZE(values));
	if (nconfigs < 0)
		return nconfigs;

	micd_configs = devm_kcalloc(madera->dev,
				    nconfigs,
				    sizeof(struct madera_micd_config),
				    GFP_KERNEL);

	for (i = 0; i < nconfigs; ++i) {
		ret = madera_extcon_of_read_part_array(madera, node,
							"cirrus,micd-configs",
							i * ARRAY_SIZE(values),
							values,
							ARRAY_SIZE(values));
		if (ret < 0)
			goto error;

		micd_configs[i].src = values[0];
		micd_configs[i].gnd = values[1];
		micd_configs[i].bias = values[2];
		micd_configs[i].gpio = values[3];
		micd_configs[i].hp_gnd = values[4];
	}

	pdata->micd_configs = micd_configs;
	pdata->num_micd_configs = nconfigs;

	return ret;

error:
	devm_kfree(madera->dev, micd_configs);
	dev_err(madera->dev,
		"DT property cirrus,micd-configs is malformed: %d\n", ret);

	return ret;
}

static void madera_extcon_of_get_hpd_pins(struct madera *madera,
					  struct device_node *node,
					  struct madera_accdet_pdata *pdata)
{
	u32 values[ARRAY_SIZE(pdata->hpd_pins)];
	int i, ret;

	BUILD_BUG_ON(ARRAY_SIZE(pdata->hpd_pins) !=
		     ARRAY_SIZE(madera_default_hpd_pins));

	memcpy(pdata->hpd_pins, madera_default_hpd_pins,
		sizeof(pdata->hpd_pins));

	ret = of_property_read_u32_array(node, "cirrus,hpd-pins",
					 values, ARRAY_SIZE(values));
	if (ret) {
		if (ret != -EINVAL)
			dev_err(madera->dev,
				 "Malformed cirrus,hpd-pins: %d\n", ret);
		return;
	}

	/* copy values, supplying defaults where requested */
	for (i = 0; i < ARRAY_SIZE(values); ++i) {
		if (values[i] > 0xFFFF)
			pdata->hpd_pins[i] = madera_default_hpd_pins[i];
		else
			pdata->hpd_pins[i] = values[i];
	}
}

static void madera_extcon_of_process(struct madera *madera,
				     struct device_node *node)
{
	struct madera_accdet_pdata *pdata;
	u32 out_num;
	int ret;

	ret = of_property_read_u32_index(node, "reg", 0, &out_num);
	if (ret != 0) {
		dev_err(madera->dev,
			"failed to read reg property from %s (%d)\n",
			node->name, ret);
		return;
	}

	if ((out_num == 0) || (out_num > ARRAY_SIZE(madera->pdata.accdet))) {
		dev_warn(madera->dev, "accdet node illegal reg %u\n", out_num);
		return;
	}

	dev_dbg(madera->dev, "processing: %s reg=%u\n", node->name, out_num);

	pdata = &madera->pdata.accdet[out_num - 1];
	pdata->enabled = true;	/* implied by presence of DT node */

	madera_extcon_of_get_int(node, "cirrus,micd-detect-debounce-ms",
				 &pdata->micd_detect_debounce_ms);

	madera_extcon_of_get_int(node, "cirrus,micd-manual-debounce",
				 &pdata->micd_manual_debounce);

	pdata->micd_pol_gpio[0] = of_get_named_gpio(node,
						 "cirrus,micd-pol-gpios", 0);
	if (pdata->micd_pol_gpio[0] < 0) {
		if (pdata->micd_pol_gpio[0] != -ENOENT)
			dev_warn(madera->dev,
				 "Malformed cirrus,micd-pol-gpios ignored: %d\n",
				pdata->micd_pol_gpio[0]);

		pdata->micd_pol_gpio[0] = 0;
	}
	pdata->micd_pol_gpio[1] = of_get_named_gpio(node,
						 "cirrus,micd-pol-gpios", 1);
	if (pdata->micd_pol_gpio[1] < 0) {
		if (pdata->micd_pol_gpio[1] != -ENOENT)
			dev_warn(madera->dev,
				 "Second cirrus,micd-pol-gpios is not configured\n");

		pdata->micd_pol_gpio[1] = 0;
	}
	madera_extcon_of_get_micd_ranges(madera, node, pdata);
	madera_extcon_of_get_micd_configs(madera, node, pdata);

	madera_extcon_of_get_int(node, "cirrus,micd-bias-start-time",
				 &pdata->micd_bias_start_time);

	madera_extcon_of_get_int(node, "cirrus,micd-rate",
				 &pdata->micd_rate);

	madera_extcon_of_get_int(node, "cirrus,micd-dbtime",
				 &pdata->micd_dbtime);

	madera_extcon_of_get_int(node, "cirrus,micd-timeout-ms",
				 &pdata->micd_timeout_ms);

	pdata->micd_force_micbias =
		of_property_read_bool(node, "cirrus,micd-force-micbias");

	pdata->micd_software_compare =
		of_property_read_bool(node, "cirrus,micd-software-compare");

	pdata->micd_open_circuit_declare =
		of_property_read_bool(node, "cirrus,micd-open-circuit-declare");

	pdata->jd_use_jd2 = of_property_read_bool(node, "cirrus,jd-use-jd2");

	pdata->jd_invert = of_property_read_bool(node, "cirrus,jd-invert");

	madera_extcon_of_get_int(node, "cirrus,fixed-hpdet-imp",
				 &pdata->fixed_hpdet_imp_x100);

	madera_extcon_of_get_int(node, "cirrus,hpdet-short-circuit-imp",
				 &pdata->hpdet_short_circuit_imp);

	madera_extcon_of_get_int(node, "cirrus,hpdet-channel",
				 &pdata->hpdet_channel);

	madera_extcon_of_get_int(node, "cirrus,jd-wake-time",
				 &pdata->jd_wake_time);

	madera_extcon_of_get_int(node, "cirrus,micd-clamp-mode",
				 &pdata->micd_clamp_mode);

	madera_extcon_of_get_hpd_pins(madera, node, pdata);

	madera_extcon_of_get_int(node, "cirrus,hpdet-ext-res",
				 &pdata->hpdet_ext_res_x100);

	pdata->report_to_input = of_property_read_bool(node,
						       "cirrus,report-to-input");
}

static int madera_extcon_of_get_pdata(struct madera *madera)
{
	struct device_node *parent, *child;

	/* a GPSW is not necessarily exclusive to a single accessory detect
	 * channel so this is stored in the global device pdata
	 */
	madera_of_read_uint_array(madera, "cirrus,gpsw", false,
				  madera->pdata.gpsw, 0,
				  ARRAY_SIZE(madera->pdata.gpsw));

	parent = of_get_child_by_name(madera->dev->of_node, "cirrus,accdet");
	if (!parent) {
		dev_dbg(madera->dev, "No DT nodes\n");
		return 0;
	}

	for_each_child_of_node(parent, child) {
		madera_extcon_of_process(madera, child);
	}

	of_node_put(parent);

	return 0;
}
#else
static inline int madera_extcon_of_get_pdata(struct madera *madera)
{
	return 0;
}
#endif

#ifdef DEBUG
#define MADERA_EXTCON_DUMP(x, f) \
	dev_dbg(madera->dev, "\t" #x ": " f "\n", pdata->x)

static void madera_extcon_dump_pdata(struct madera *madera)
{
	const struct madera_accdet_pdata *pdata;
	int i, j;

	dev_dbg(madera->dev, "extcon pdata gpsw=[0x%x 0x%x]\n",
		madera->pdata.gpsw[0], madera->pdata.gpsw[1]);

	for (i = 0; i < ARRAY_SIZE(madera->pdata.accdet); ++i) {
		pdata = &madera->pdata.accdet[i];

		dev_dbg(madera->dev, "extcon pdata OUT%u\n", i + 1);
		MADERA_EXTCON_DUMP(enabled, "%u");
		MADERA_EXTCON_DUMP(jd_wake_time, "%d");
		MADERA_EXTCON_DUMP(jd_use_jd2, "%u");
		MADERA_EXTCON_DUMP(jd_invert, "%u");
		MADERA_EXTCON_DUMP(fixed_hpdet_imp_x100, "%d");
		MADERA_EXTCON_DUMP(hpdet_ext_res_x100, "%d");
		MADERA_EXTCON_DUMP(hpdet_short_circuit_imp, "%d");
		MADERA_EXTCON_DUMP(hpdet_channel, "%d");
		MADERA_EXTCON_DUMP(micd_detect_debounce_ms, "%d");
		MADERA_EXTCON_DUMP(hpdet_short_circuit_imp, "%d");
		MADERA_EXTCON_DUMP(hpdet_channel, "%d");
		MADERA_EXTCON_DUMP(micd_detect_debounce_ms, "%d");
		MADERA_EXTCON_DUMP(micd_manual_debounce, "%d");
		MADERA_EXTCON_DUMP(micd_pol_gpio[0], "%d");
		MADERA_EXTCON_DUMP(micd_pol_gpio[1], "%d");
		MADERA_EXTCON_DUMP(micd_bias_start_time, "%d");
		MADERA_EXTCON_DUMP(micd_rate, "%d");
		MADERA_EXTCON_DUMP(micd_dbtime, "%d");
		MADERA_EXTCON_DUMP(micd_timeout_ms, "%d");
		MADERA_EXTCON_DUMP(micd_clamp_mode, "%u");
		MADERA_EXTCON_DUMP(micd_force_micbias, "%u");
		MADERA_EXTCON_DUMP(micd_open_circuit_declare, "%u");
		MADERA_EXTCON_DUMP(micd_software_compare, "%u");

		dev_dbg(madera->dev, "\tmicd_ranges {\n");
		for (j = 0; j < pdata->num_micd_ranges; ++j)
			dev_dbg(madera->dev, "\t\tmax: %d key: %d\n",
				pdata->micd_ranges[j].max,
				pdata->micd_ranges[j].key);
		dev_dbg(madera->dev, "\t}\n");

		dev_dbg(madera->dev, "\tmicd_configs {\n");
		for (j = 0; j < pdata->num_micd_configs; ++j)
			dev_dbg(madera->dev,
				"\t\tsrc: 0x%x gnd: 0x%x bias: %u gpio: %u hp_gnd: %d\n",
				pdata->micd_configs[j].src,
				pdata->micd_configs[j].gnd,
				pdata->micd_configs[j].bias,
				pdata->micd_configs[j].gpio,
				pdata->micd_configs[j].hp_gnd);
		dev_dbg(madera->dev, "\t}\n");

		dev_dbg(madera->dev, "\thpd_pins: %u %u %u %u\n",
			pdata->hpd_pins[0], pdata->hpd_pins[1],
			pdata->hpd_pins[2], pdata->hpd_pins[3]);
	}
}
#else
static inline void madera_extcon_dump_pdata(struct madera *madera)
{
}
#endif

static int madera_extcon_read_calibration(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	struct madera_hpdet_trims *trims;
	int ret = -EIO;
	unsigned int offset, gradient, interim_val;
	unsigned int otp_hpdet_calib_1, otp_hpdet_calib_2;

	switch (madera->type) {
	case CS47L35:
		otp_hpdet_calib_1 = CS47L35_OTP_HPDET_CAL_1;
		otp_hpdet_calib_2 = CS47L35_OTP_HPDET_CAL_2;
		break;
	case CS47L85:
	case WM1840:
		otp_hpdet_calib_1 = CS47L85_OTP_HPDET_CAL_1;
		otp_hpdet_calib_2 = CS47L85_OTP_HPDET_CAL_2;
		break;
	default:
		otp_hpdet_calib_1 = MADERA_OTP_HPDET_CAL_1;
		otp_hpdet_calib_2 = MADERA_OTP_HPDET_CAL_2;
		break;
	}

	ret = regmap_read(madera->regmap_32bit, otp_hpdet_calib_1, &offset);
	if (ret) {
		dev_err(madera->dev,
			"Failed to read HP CALIB OFFSET value: %d\n", ret);
		return ret;
	}

	ret = regmap_read(madera->regmap_32bit, otp_hpdet_calib_2, &gradient);
	if (ret) {
		dev_err(madera->dev,
			"Failed to read HP CALIB OFFSET value: %d\n", ret);
		return ret;
	}

	if (((offset == 0) && (gradient == 0)) ||
	    ((offset == 0xFFFFFFFF) && (gradient == 0xFFFFFFFF))) {
		dev_warn(madera->dev, "No HP trims\n");
		return 0;
	}

	trims = devm_kcalloc(info->dev, 4, sizeof(struct madera_hpdet_trims),
			     GFP_KERNEL);

	interim_val = (offset & MADERA_OTP_HPDET_CALIB_OFFSET_00_MASK) >>
		       MADERA_OTP_HPDET_CALIB_OFFSET_00_SHIFT;
	trims[0].off_x4 = 128 - interim_val;

	interim_val = (gradient & MADERA_OTP_HPDET_GRADIENT_0X_MASK) >>
		       MADERA_OTP_HPDET_GRADIENT_0X_SHIFT;
	trims[0].grad_x4 = 128 - interim_val;

	interim_val = (offset & MADERA_OTP_HPDET_CALIB_OFFSET_01_MASK) >>
			MADERA_OTP_HPDET_CALIB_OFFSET_01_SHIFT;
	trims[1].off_x4 = 128 - interim_val;

	trims[1].grad_x4 = trims[0].grad_x4;

	interim_val = (offset & MADERA_OTP_HPDET_CALIB_OFFSET_10_MASK) >>
		       MADERA_OTP_HPDET_CALIB_OFFSET_10_SHIFT;
	trims[2].off_x4 = 128 - interim_val;

	interim_val = (gradient & MADERA_OTP_HPDET_GRADIENT_1X_MASK) >>
		       MADERA_OTP_HPDET_GRADIENT_1X_SHIFT;
	trims[2].grad_x4 = 128 - interim_val;

	interim_val = (offset & MADERA_OTP_HPDET_CALIB_OFFSET_11_MASK) >>
		       MADERA_OTP_HPDET_CALIB_OFFSET_11_SHIFT;
	trims[3].off_x4 = 128 - interim_val;

	trims[3].grad_x4 = trims[2].grad_x4;

	info->hpdet_trims = trims;

	dev_dbg(madera->dev,
		"trims_x_4: %u,%u %u,%u %u,%u %u,%u\n",
		trims[0].off_x4, trims[0].grad_x4,
		trims[1].off_x4, trims[1].grad_x4,
		trims[2].off_x4, trims[2].grad_x4,
		trims[3].off_x4, trims[3].grad_x4);

	return 0;
}

static void madera_extcon_set_micd_clamp_mode(struct madera_extcon_info *info)
{
	unsigned int clamp_ctrl_val;

	/* If the user has supplied a micd_clamp_mode, assume they know
	 * what they are doing and just write it out
	 */
	if (info->pdata->micd_clamp_mode) {
		clamp_ctrl_val = info->pdata->micd_clamp_mode;
	} else if (info->pdata->jd_use_jd2) {
		if (info->pdata->jd_invert)
			clamp_ctrl_val = MADERA_MICD_CLAMP_MODE_JD1H_JD2H;
		else
			clamp_ctrl_val = MADERA_MICD_CLAMP_MODE_JD1L_JD2L;
	} else {
		if (info->pdata->jd_invert)
			clamp_ctrl_val = MADERA_MICD_CLAMP_MODE_JD1H;
		else
			clamp_ctrl_val = MADERA_MICD_CLAMP_MODE_JD1L;
	}

	regmap_update_bits(info->madera->regmap,
			   MADERA_MICD_CLAMP_CONTROL,
			   MADERA_MICD_CLAMP_MODE_MASK,
			   clamp_ctrl_val);

	regmap_update_bits(info->madera->regmap,
			   MADERA_INTERRUPT_DEBOUNCE_7,
			   MADERA_MICD_CLAMP_DB,
			   MADERA_MICD_CLAMP_DB);
}

static int madera_extcon_add_micd_levels(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	int i, j;
	int ret = 0;

	BUILD_BUG_ON(ARRAY_SIZE(madera_micd_levels) <
		     MADERA_NUM_MICD_BUTTON_LEVELS);

	/* Disable all buttons by default */
	regmap_update_bits(madera->regmap, MADERA_MIC_DETECT_1_CONTROL_2,
			   MADERA_MICD_LVL_SEL_MASK, 0x81);

	/* Set up all the buttons the user specified */
	for (i = 0; i < info->num_micd_ranges; i++) {
		for (j = 0; j < MADERA_NUM_MICD_BUTTON_LEVELS; j++)
			if (madera_micd_levels[j] >= info->micd_ranges[i].max)
				break;

		if (j == MADERA_NUM_MICD_BUTTON_LEVELS) {
			dev_err(madera->dev, "Unsupported MICD level %d\n",
				info->micd_ranges[i].max);
			ret = -EINVAL;
			goto err_input;
		}

		dev_dbg(madera->dev, "%d ohms for MICD threshold %d\n",
			madera_micd_levels[j], i);

		madera_micd_set_level(madera, i, j);
		if (info->micd_ranges[i].key > 0)
			input_set_capability(info->input, EV_KEY,
					     info->micd_ranges[i].key);

		/* Enable reporting of that range */
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_2,
				   1 << i, 1 << i);
	}

	/* Set all the remaining keys to a maximum */
	for (; i < MADERA_MAX_MICD_RANGE; i++)
		madera_micd_set_level(madera, i, 0x3f);

err_input:
	return ret;
}

static int madera_extcon_init_micd_ranges(struct madera_extcon_info *info)
{
	struct madera *madera = info->madera;
	const struct madera_accdet_pdata *pdata = info->pdata;
	struct madera_micd_range *ranges;
	int i;

	if (pdata->num_micd_ranges == 0) {
		info->micd_ranges = madera_micd_default_ranges;
		info->num_micd_ranges =
			ARRAY_SIZE(madera_micd_default_ranges) - 2;
		return 0;
	}

	if (pdata->num_micd_ranges > MADERA_MAX_MICD_RANGE) {
		dev_err(madera->dev, "Too many MICD ranges: %d\n",
			pdata->num_micd_ranges);
		return -EINVAL;
	}

	ranges = devm_kmalloc_array(madera->dev,
				    pdata->num_micd_ranges,
				    sizeof(struct madera_micd_range),
				    GFP_KERNEL);
	if (!ranges) {
		dev_err(madera->dev, "Failed to kalloc micd ranges\n");
		return -ENOMEM;
	}

	memcpy(ranges, pdata->micd_ranges,
	       sizeof(struct madera_micd_range) * pdata->num_micd_ranges);
	info->micd_ranges = ranges;
	info->num_micd_ranges = pdata->num_micd_ranges;

	for (i = 0; i < info->num_micd_ranges - 1; i++) {
		if (info->micd_ranges[i].max > info->micd_ranges[i + 1].max) {
			dev_err(madera->dev, "MICD ranges must be sorted\n");
			goto err_free;
		}
	}

	return 0;

err_free:
	devm_kfree(madera->dev, ranges);
	return -EINVAL;
}

static void madera_extcon_xlate_pdata(struct madera_accdet_pdata *pdata)
{
	int i;

	BUILD_BUG_ON(ARRAY_SIZE(pdata->hpd_pins) !=
		     ARRAY_SIZE(madera_default_hpd_pins));

	/* translate from pdata format where 0=default and >0xFFFF means 0 */
	for (i = 0; i < ARRAY_SIZE(pdata->hpd_pins); ++i) {
		if (pdata->hpd_pins[i] == 0)
			pdata->hpd_pins[i] = madera_default_hpd_pins[i];
		else if (pdata->hpd_pins[i] > 0xFFFF)
			pdata->hpd_pins[i] = 0;
	}
}

static int madera_extcon_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	struct madera_accdet_pdata *pdata = &madera->pdata.accdet[0];
	struct madera_extcon_info *info;
	unsigned int debounce_val, analog_val;
	int jack_irq_fall, jack_irq_rise;
	int ret, mode, i;

	/* quick exit if Madera irqchip driver hasn't completed probe */
	if (!madera->irq_dev) {
		dev_dbg(madera->dev, "irqchip driver not ready\n");
		return -EPROBE_DEFER;
	}

	if (!madera->dapm || !madera->dapm->card)
		return -EPROBE_DEFER;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pdata = pdata;
	info->madera = madera;
	info->dev = &pdev->dev;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!dev_get_platdata(madera->dev)) {
			ret = madera_extcon_of_get_pdata(madera);
			if (ret < 0)
				return ret;
		}
	} else {
		madera_extcon_xlate_pdata(pdata);
	}

	madera_extcon_dump_pdata(madera);

	if (pdata->hpdet_short_circuit_imp < MADERA_HP_SHORT_IMPEDANCE_MIN)
		pdata->hpdet_short_circuit_imp = MADERA_HP_SHORT_IMPEDANCE_MIN;

	switch (madera->type) {
	case CS47L35:
		pdata->micd_force_micbias = true;
		info->hpdet_ranges = cs47l85_hpdet_ranges;
		info->num_hpdet_ranges = ARRAY_SIZE(cs47l85_hpdet_ranges);
		break;
	case CS47L85:
	case WM1840:
		info->hpdet_ranges = cs47l85_hpdet_ranges;
		info->num_hpdet_ranges = ARRAY_SIZE(cs47l85_hpdet_ranges);
		break;
	default:
		info->hpdet_ranges = madera_hpdet_ranges;
		info->num_hpdet_ranges = ARRAY_SIZE(madera_hpdet_ranges);
		break;
	}

	/* Set of_node to parent from the SPI device to allow
	 * location regulator supplies.
	 */
	pdev->dev.of_node = madera->dev->of_node;

	info->micvdd = devm_regulator_get(&pdev->dev, "MICVDD");
	if (IS_ERR(info->micvdd)) {
		ret = PTR_ERR(info->micvdd);
		dev_err(madera->dev, "Failed to get MICVDD: %d\n", ret);
		return ret;
	}

	mutex_init(&info->lock);
	init_completion(&info->manual_mic_completion);
	wakeup_source_init(&info->detection_wake_lock, "madera-jack-detection");
	INIT_DELAYED_WORK(&info->micd_detect_work, madera_micd_handler);
	INIT_DELAYED_WORK(&info->state_timeout_work, madera_jds_timeout_work);
	platform_set_drvdata(pdev, info);
	madera->extcon_info = info;

	if (pdata->jd_invert)
		info->last_jackdet =
			~(MADERA_MICD_CLAMP_RISE_STS1 | MADERA_JD1_FALL_STS1);
	else
		info->last_jackdet =
			~(MADERA_MICD_CLAMP_RISE_STS1 | MADERA_JD1_RISE_STS1);

	info->edev.name = "h2w";
	ret = switch_dev_register(&info->edev);
	if (ret < 0) {
		dev_err(madera->dev,
			"extcon_dev_register() failed: %d\n", ret);
		goto err_wakelock;
	}

	info->input = devm_input_allocate_device(&pdev->dev);
	if (!info->input) {
		dev_err(madera->dev, "Can't allocate input dev\n");
		ret = -ENOMEM;
		goto err_register;
	}

	info->input->name = "Headset";
	info->input->phys = "madera/switch";
	info->input->dev.parent = &pdev->dev;

	if (pdata->num_micd_configs) {
		info->micd_modes = pdata->micd_configs;
		info->micd_num_modes = pdata->num_micd_configs;
	} else {
		switch (madera->type) {
		case CS47L35:
		case CS47L85:
		case WM1840:
			info->micd_modes = cs47l85_micd_default_modes;
			info->micd_num_modes =
				ARRAY_SIZE(cs47l85_micd_default_modes);
			break;
		default:
			info->micd_modes = madera_micd_default_modes;
			info->micd_num_modes =
				ARRAY_SIZE(madera_micd_default_modes);
			break;
		}
	}

	if (madera->pdata.gpsw[0] > 0)
		regmap_update_bits(madera->regmap,
				   MADERA_GP_SWITCH_1,
				   MADERA_SW1_MODE_MASK,
				   madera->pdata.gpsw[0] <<
				   MADERA_SW1_MODE_SHIFT);
	switch (madera->type) {
	case CS47L90:
	case CS47L91:
		if (madera->pdata.gpsw[1] > 0)
			regmap_update_bits(madera->regmap,
					   MADERA_GP_SWITCH_1,
					   MADERA_SW2_MODE_MASK,
					   madera->pdata.gpsw[1] <<
					   MADERA_SW2_MODE_SHIFT);
		break;
	default:
		break;
	}

	if (info->pdata->micd_pol_gpio[0] > 0) {
		if (info->micd_modes[0].gpio)
			mode = GPIOF_OUT_INIT_HIGH;
		else
			mode = GPIOF_OUT_INIT_LOW;

		ret = devm_gpio_request_one(&pdev->dev,
					    info->pdata->micd_pol_gpio[0],
					    mode,
					    "MICD polarity");
		if (ret) {
			dev_err(madera->dev, "Failed to request GPIO%d: %d\n",
				info->pdata->micd_pol_gpio[0], ret);
			goto err_register;
		}
	}
	if (info->pdata->micd_pol_gpio[1] > 0) {
		if (info->micd_modes[0].gpio)
			mode = GPIOF_OUT_INIT_LOW;
		else
			mode = GPIOF_OUT_INIT_HIGH;

		ret = devm_gpio_request_one(&pdev->dev,
					    info->pdata->micd_pol_gpio[1],
					    mode,
					    "MICD polarity");
		if (ret) {
			dev_err(madera->dev, "Failed to request GPIO%d: %d\n",
				info->pdata->micd_pol_gpio[1], ret);
			goto err_register;
		}
	}

	if (info->pdata->micd_bias_start_time)
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_BIAS_STARTTIME_MASK,
				   info->pdata->micd_bias_start_time
				   << MADERA_MICD_BIAS_STARTTIME_SHIFT);

	if (info->pdata->micd_rate)
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_RATE_MASK,
				   info->pdata->micd_rate
				   << MADERA_MICD_RATE_SHIFT);

	if (info->pdata->micd_dbtime)
		regmap_update_bits(madera->regmap,
				   MADERA_MIC_DETECT_1_CONTROL_1,
				   MADERA_MICD_DBTIME_MASK,
				   info->pdata->micd_dbtime
				   << MADERA_MICD_DBTIME_SHIFT);

	ret = madera_extcon_init_micd_ranges(info);
	if (ret)
		goto err_input;

	ret = madera_extcon_add_micd_levels(info);
	if (ret)
		goto err_input;

	if (pdata->report_to_input) {
		for (i = 0; i < MADERA_JACK_SWITCH_TYPES; i++)
			input_set_capability(info->input, EV_SW,
					     arizona_switch_types[i]);
	}

	madera_extcon_set_micd_clamp_mode(info);

	madera_extcon_set_mode(info, 0);

	/* Invalidate the tuning level so that the first detection
	 * will always apply a tuning
	 */
	info->hp_tuning_level = MADERA_HP_TUNING_INVALID;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_idle(&pdev->dev);

	pm_runtime_get_sync(&pdev->dev);

	madera_extcon_read_calibration(info);
	if (info->hpdet_trims) {
		switch (madera->type) {
		case CS47L35:
		case CS47L85:
		case WM1840:
			/* set for accurate HP impedance detection */
			regmap_update_bits(madera->regmap,
				MADERA_ACCESSORY_DETECT_MODE_1,
				MADERA_ACCDET_POLARITY_INV_ENA_MASK,
				1 << MADERA_ACCDET_POLARITY_INV_ENA_SHIFT);
			break;
		default:
			break;
		}
	}

	if (info->pdata->jd_use_jd2) {
		jack_irq_rise = MADERA_IRQ_MICD_CLAMP_RISE;
		jack_irq_fall = MADERA_IRQ_MICD_CLAMP_FALL;
	} else {
		jack_irq_rise = MADERA_IRQ_JD1_RISE;
		jack_irq_fall = MADERA_IRQ_JD1_FALL;
	}

	ret = madera_request_irq(madera, jack_irq_rise,
				 "JACKDET rise", madera_jackdet, info);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to get JACKDET rise IRQ: %d\n", ret);
		goto err_input;
	}

	ret = madera_set_irq_wake(madera, jack_irq_rise, 1);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to set JD rise IRQ wake: %d\n",	ret);
		goto err_rise;
	}

	ret = madera_request_irq(madera, jack_irq_fall,
				 "JACKDET fall", madera_jackdet, info);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get JD fall IRQ: %d\n", ret);
		goto err_rise_wake;
	}

	ret = madera_set_irq_wake(madera, jack_irq_fall, 1);
	if (ret) {
		dev_err(&pdev->dev,
			"Failed to set JD fall IRQ wake: %d\n", ret);
		goto err_fall;
	}

	ret = madera_request_irq(madera, MADERA_IRQ_MICDET1,
				 "MICDET", madera_micdet, info);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get MICDET IRQ: %d\n", ret);
		goto err_fall_wake;
	}

	ret = madera_request_irq(madera, MADERA_IRQ_HPDET,
				 "HPDET", madera_hpdet_handler, info);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get HPDET IRQ: %d\n", ret);
		goto err_micdet;
	}

	if (info->pdata->jd_use_jd2) {
		debounce_val = MADERA_JD1_DB | MADERA_JD2_DB;
		analog_val = MADERA_JD1_ENA | MADERA_JD2_ENA;
	} else {
		debounce_val = MADERA_JD1_DB;
		analog_val = MADERA_JD1_ENA;
	}

	regmap_update_bits(madera->regmap, MADERA_INTERRUPT_DEBOUNCE_7,
			   debounce_val, debounce_val);
	regmap_update_bits(madera->regmap, MADERA_JACK_DETECT_ANALOGUE,
			   analog_val, analog_val);

	ret = regulator_allow_bypass(info->micvdd, true);
	if (ret)
		dev_warn(madera->dev,
			"Failed to set MICVDD to bypass: %d\n", ret);

	pm_runtime_put(&pdev->dev);

	ret = input_register_device(info->input);
	if (ret) {
		dev_err(&pdev->dev, "Can't register input device: %d\n", ret);
		goto err_hpdet;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_hp1_impedance);
	if (ret)
		dev_warn(&pdev->dev,
			"Failed to create sysfs node for hp_impedance %d\n",
			ret);

	return 0;

err_hpdet:
	madera_free_irq(madera, MADERA_IRQ_HPDET, info);
err_micdet:
	madera_free_irq(madera, MADERA_IRQ_MICDET1, info);
err_fall_wake:
	madera_set_irq_wake(madera, jack_irq_fall, 0);
err_fall:
	madera_free_irq(madera, jack_irq_fall, info);
err_rise_wake:
	madera_set_irq_wake(madera, jack_irq_rise, 0);
err_rise:
	madera_free_irq(madera, jack_irq_rise, info);
err_input:
err_register:
	pm_runtime_disable(&pdev->dev);
	switch_dev_unregister(&info->edev);
err_wakelock:
	wakeup_source_trash(&info->detection_wake_lock);
	return ret;
}

static int madera_extcon_remove(struct platform_device *pdev)
{
	struct madera_extcon_info *info = platform_get_drvdata(pdev);
	struct madera *madera = info->madera;
	int jack_irq_rise, jack_irq_fall;

	pm_runtime_disable(&pdev->dev);

	regmap_update_bits(madera->regmap, MADERA_MICD_CLAMP_CONTROL,
			   MADERA_MICD_CLAMP_MODE_MASK, 0);

	if (info->pdata->jd_use_jd2) {
		jack_irq_rise = MADERA_IRQ_MICD_CLAMP_RISE;
		jack_irq_fall = MADERA_IRQ_MICD_CLAMP_FALL;
	} else {
		jack_irq_rise = MADERA_IRQ_JD1_RISE;
		jack_irq_fall = MADERA_IRQ_JD1_FALL;
	}

	madera_set_irq_wake(madera, jack_irq_rise, 0);
	madera_set_irq_wake(madera, jack_irq_fall, 0);
	madera_free_irq(madera, MADERA_IRQ_HPDET, info);
	madera_free_irq(madera, MADERA_IRQ_MICDET1, info);
	madera_free_irq(madera, jack_irq_rise, info);
	madera_free_irq(madera, jack_irq_fall, info);
	regmap_update_bits(madera->regmap, MADERA_JACK_DETECT_ANALOGUE,
			   MADERA_JD1_ENA | MADERA_JD2_ENA, 0);

	device_remove_file(&pdev->dev, &dev_attr_hp1_impedance);
	switch_dev_unregister(&info->edev);
	wakeup_source_trash(&info->detection_wake_lock);
	kfree(info->hpdet_trims);

	return 0;
}

static struct platform_driver madera_extcon_driver = {
	.driver		= {
		.name	= "madera-extcon",
	},
	.probe		= madera_extcon_probe,
	.remove		= madera_extcon_remove,
};

module_platform_driver(madera_extcon_driver);

MODULE_DESCRIPTION("Madera switch driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:switch-madera");
