/*
 * TI LM3631 Backlight Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LM3631 backlight driver consists of three parts
 *
 *   1) LM3631 chip specific part: this file
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

#define LM3631_FULL_STRINGS		(LMU_HVLED1 | LMU_HVLED2)
#define LM3631_DEFAULT_MODE		LM3631_MODE_I2C
#define LM3631_MAX_BRIGHTNESS		255

static int lm3631_bl_init(struct ti_lmu_bl_chip *chip)
{
	int ret;

	/* Set OVP to 29V by default */
	ret = ti_lmu_update_bits(chip->lmu, LM3631_REG_BL_BOOST,
				 LM3631_BOOST_OVP_MASK, LM3631_BOOST_OVP_29V);
	if (ret)
		return ret;

	/* Set the brightness mode to 'i2c only' by default */
	return ti_lmu_update_bits(chip->lmu, LM3631_REG_BRT_MODE,
				  LM3631_MODE_MASK, LM3631_DEFAULT_MODE);
}

static int lm3631_bl_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3631_REG_DEVCTRL,
				  LM3631_BL_EN_MASK,
				  enable << LM3631_BL_EN_SHIFT);
}

static int lm3631_bl_set_brightness(struct ti_lmu_bl *lmu_bl, int brightness)
{
	u8 data;

	if (lmu_bl->mode == BL_PWM_BASED)
		return 0;

	data = brightness & 0xFF;
	return ti_lmu_write_byte(lmu_bl->chip->lmu, LM3631_REG_BRT_MSB,
				 data);
}

static int lm3631_bl_string_configure(struct ti_lmu_bl *lmu_bl)
{
	u8 val;

	if (lmu_bl->bl_pdata->bl_string == LM3631_FULL_STRINGS)
		val = LM3631_BL_TWO_STRINGS;
	else
		val = LM3631_BL_ONE_STRING;

	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3631_REG_BL_CFG,
				  LM3631_BL_STRING_MASK, val);
}

static void lm3631_bl_effect_request(struct ti_lmu_bl *lmu_bl)
{
	/* Set exponential mapping */
	ti_lmu_update_bits(lmu_bl->chip->lmu, LM3631_REG_BL_CFG,
			   LM3631_MAP_MASK, LM3631_LINEAR_MAP);

	/* Enable slope bit before updating slope time value */
	ti_lmu_update_bits(lmu_bl->chip->lmu, LM3631_REG_BRT_MODE,
			   LM3631_EN_SLOPE_MASK, LM3631_EN_SLOPE_MASK);

	ti_lmu_effect_request(LM3631_EFFECT_SLOPE,
			      ti_lmu_backlight_effect_callback,
			      BL_EFFECT_RAMPUP, lmu_bl);
}

static int lm3631_bl_configure(struct ti_lmu_bl *lmu_bl)
{
	int ret;

	ret = lm3631_bl_string_configure(lmu_bl);
	if (ret)
		return ret;

	if (lmu_bl->mode == BL_PWM_BASED) {
		snprintf(lmu_bl->pwm_name, sizeof(lmu_bl->pwm_name),
			 "%s", LMU_BL_DEFAULT_PWM_NAME);
	}

	/* LMU effect is optional, checking return value is not required */
	lm3631_bl_effect_request(lmu_bl);

	return 0;
}

static const struct ti_lmu_bl_ops lm3631_lmu_ops = {
	.init              = lm3631_bl_init,
	.configure         = lm3631_bl_configure,
	.update_brightness = lm3631_bl_set_brightness,
	.bl_enable         = lm3631_bl_enable,
	.max_brightness    = LM3631_MAX_BRIGHTNESS,
};

static int lm3631_bl_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct ti_lmu_bl *lmu_bl;
	struct ti_lmu_bl_chip *chip;

	chip = ti_lmu_backlight_init_device(&pdev->dev, lmu, &lm3631_lmu_ops);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	lmu_bl = ti_lmu_backlight_register(chip, lmu->pdata->bl_pdata, 1);
	if (IS_ERR(lmu_bl))
		return PTR_ERR(lmu_bl);

	platform_set_drvdata(pdev, lmu_bl);

	return 0;
}

static int lm3631_bl_remove(struct platform_device *pdev)
{
	struct ti_lmu_bl *lmu_bl = platform_get_drvdata(pdev);

	return ti_lmu_backlight_unregister(lmu_bl);
}

#ifdef CONFIG_OF
static const struct of_device_id lm3631_bl_of_match[] = {
	{ .compatible = "ti,lm3631-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3631_bl_of_match);
#endif

static struct platform_driver lm3631_bl_driver = {
	.probe = lm3631_bl_probe,
	.remove = lm3631_bl_remove,
	.driver = {
		.name = "lm3631-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3631_bl_of_match),
	},
};
module_platform_driver(lm3631_bl_driver);

MODULE_DESCRIPTION("TI LM3631 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3631-backlight");
