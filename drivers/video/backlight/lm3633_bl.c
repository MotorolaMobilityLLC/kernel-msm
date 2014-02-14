/*
 * TI LM3633 Backlight Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LM3633 backlight driver consists of two parts
 *
 *   1) LM3633 chip specific part: this file
 *      Define device specific operations
 *      Register LMU backlight driver
 *
 *   2) LMU backlight common driver: ti-lmu-backlight
 *      General backlight subsystem control
 *
 *   3) LMU effect driver
 *      Backlight slope time configuration
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-effect.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#include "ti-lmu-backlight.h"

#define LM3633_BOOST_OVP_40V		0x6
#define LM3633_BL_MAX_STRINGS		3
#define LM3633_BL_MAX_BRIGHTNESS	2047

static int lm3633_bl_init(struct ti_lmu_bl_chip *chip)
{
	/* Configure ramp selection for each bank */
	return ti_lmu_update_bits(chip->lmu, LM3633_REG_BL_RAMP_CONF,
				  LM3633_BL_RAMP_MASK, LM3633_BL_RAMP_EACH);
}

static int lm3633_bl_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3633_REG_ENABLE,
				  BIT(lmu_bl->bank_id),
				  enable << lmu_bl->bank_id);
}

static int lm3633_bl_set_brightness(struct ti_lmu_bl *lmu_bl, int brightness)
{
	int ret;
	u8 data;
	u8 reg_lsb[] = { LM3633_REG_BRT_HVLED_A_LSB,
			 LM3633_REG_BRT_HVLED_B_LSB, };
	u8 reg_msb[] = { LM3633_REG_BRT_HVLED_A_MSB,
			 LM3633_REG_BRT_HVLED_B_MSB, };

	if (lmu_bl->mode == BL_PWM_BASED) {
		/*
		 * PWM can start from any non-zero code and dim down to zero.
		 * So, brightness register should be updated even in PWM mode.
		 */
		if (brightness > 0)
			brightness = LM3633_BL_MAX_BRIGHTNESS;
		else
			brightness = 0;
	}

	data = brightness & LM3633_BRT_HVLED_LSB_MASK;
	ret = ti_lmu_update_bits(lmu_bl->chip->lmu, reg_lsb[lmu_bl->bank_id],
				 LM3633_BRT_HVLED_LSB_MASK, data);
	if (ret)
		return ret;

	data = (brightness >> LM3633_BRT_HVLED_MSB_SHIFT) & 0xFF;
	return ti_lmu_write_byte(lmu_bl->chip->lmu, reg_msb[lmu_bl->bank_id],
				 data);
}

static int lm3633_bl_boost_configure(struct ti_lmu_bl *lmu_bl)
{
	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3633_REG_BOOST_CFG,
				  LM3633_BOOST_OVP_MASK, LM3633_BOOST_OVP_40V);
}

static int lm3633_bl_set_ctrl_mode(struct ti_lmu_bl *lmu_bl)
{
	int bank_id = lmu_bl->bank_id;

	if (lmu_bl->mode == BL_PWM_BASED)
		return ti_lmu_update_bits(lmu_bl->chip->lmu,
					  LM3633_REG_PWM_CFG,
					  BIT(bank_id), 1 << bank_id);

	return 0;
}

static int lm3633_bl_string_configure(struct ti_lmu_bl *lmu_bl)
{
	struct ti_lmu *lmu = lmu_bl->chip->lmu;
	int is_detected = 0;
	int i, ret;

	/* Assign control bank from backlight string configuration */
	for (i = 0; i < LM3633_BL_MAX_STRINGS; i++) {
		if (test_bit(i, &lmu_bl->bl_pdata->bl_string)) {
			ret = ti_lmu_update_bits(lmu,
						 LM3633_REG_HVLED_OUTPUT_CFG,
						 BIT(i), lmu_bl->bank_id << i);
			if (ret)
				return ret;

			is_detected = 1;
		}
	}

	if (!is_detected) {
		dev_err(lmu_bl->chip->dev, "No backlight string found\n");
		return -EINVAL;
	}

	if (lmu_bl->mode == BL_PWM_BASED) {
		snprintf(lmu_bl->pwm_name, sizeof(lmu_bl->pwm_name),
			 "%s", LMU_BL_DEFAULT_PWM_NAME);
	}

	return 0;
}

static int lm3633_bl_set_current(struct ti_lmu_bl *lmu_bl)
{
	u8 reg[] = { LM3633_REG_IMAX_HVLED_A, LM3633_REG_IMAX_HVLED_B, };

	return ti_lmu_write_byte(lmu_bl->chip->lmu, reg[lmu_bl->bank_id],
				 lmu_bl->bl_pdata->imax);
}

static void lm3633_bl_effect_request(enum lmu_effect_request_id id,
				     struct ti_lmu_bl *lmu_bl)
{
	const char *name[][2] = {
		{ LM3633_EFFECT_BL0_RAMPUP, LM3633_EFFECT_BL0_RAMPDOWN },
		{ LM3633_EFFECT_BL1_RAMPUP, LM3633_EFFECT_BL1_RAMPDOWN },
	};

	ti_lmu_effect_request(name[lmu_bl->bank_id][id],
			      ti_lmu_backlight_effect_callback, id, lmu_bl);
}

static int lm3633_bl_configure(struct ti_lmu_bl *lmu_bl)
{
	int ret;

	ret = lm3633_bl_boost_configure(lmu_bl);
	if (ret)
		return ret;

	ret = lm3633_bl_set_ctrl_mode(lmu_bl);
	if (ret)
		return ret;

	ret = lm3633_bl_string_configure(lmu_bl);
	if (ret)
		return ret;

	ret = lm3633_bl_set_current(lmu_bl);
	if (ret)
		return ret;

	/* LMU effect is optional, checking return value is not required */
	if (lmu_bl->bl_pdata->ramp_up_ms)
		lm3633_bl_effect_request(BL_EFFECT_RAMPUP, lmu_bl);

	if (lmu_bl->bl_pdata->ramp_down_ms)
		lm3633_bl_effect_request(BL_EFFECT_RAMPDN, lmu_bl);

	return 0;
}

static const struct ti_lmu_bl_ops lm3633_lmu_ops = {
	.init              = lm3633_bl_init,
	.configure         = lm3633_bl_configure,
	.update_brightness = lm3633_bl_set_brightness,
	.bl_enable         = lm3633_bl_enable,
	.max_brightness    = LM3633_BL_MAX_BRIGHTNESS,
};

static int lm3633_bl_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct ti_lmu_bl *lmu_bl;
	struct ti_lmu_bl_chip *chip;

	chip = ti_lmu_backlight_init_device(&pdev->dev, lmu, &lm3633_lmu_ops);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	lmu_bl = ti_lmu_backlight_register(chip, lmu->pdata->bl_pdata,
					   lmu->pdata->num_backlights);
	if (IS_ERR(lmu_bl))
		return PTR_ERR(lmu_bl);

	platform_set_drvdata(pdev, lmu_bl);

	return 0;
}

static int lm3633_bl_remove(struct platform_device *pdev)
{
	struct ti_lmu_bl *lmu_bl = platform_get_drvdata(pdev);

	return ti_lmu_backlight_unregister(lmu_bl);
}

#ifdef CONFIG_OF
static const struct of_device_id lm3633_bl_of_match[] = {
	{ .compatible = "ti,lm3633-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3633_bl_of_match);
#endif

static struct platform_driver lm3633_bl_driver = {
	.probe = lm3633_bl_probe,
	.remove = lm3633_bl_remove,
	.driver = {
		.name = "lm3633-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3633_bl_of_match),
	},
};
module_platform_driver(lm3633_bl_driver);

MODULE_DESCRIPTION("TI LM3633 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3633-backlight");
