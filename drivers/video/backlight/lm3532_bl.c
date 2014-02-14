/*
 * TI LM3532 Backlight Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LM3532 backlight driver consists of three parts
 *
 *   1) LM3532 chip specific part: this file
 *      Define device specific operations
 *      Register LMU backlight driver
 *
 *   2) LMU backlight common driver: ti-lmu-backlight
 *      General backlight subsystem control
 *
 *   3) LMU effect driver
 *      Backlight ramp time configuration
 *
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

#define LM3532_PWM1			0
#define LM3532_PWM2			1
#define LM3532_BL_MAX_STRINGS		3
#define LM3532_MAX_ZONE_CFG		3
#define LM3532_MAX_BRIGHTNESS		255

static int lm3532_bl_init(struct ti_lmu_bl_chip *chip)
{
	int i, ret;
	u8 lm3532_regs[] = { LM3532_REG_ZONE_CFG_A, LM3532_REG_ZONE_CFG_B,
			     LM3532_REG_ZONE_CFG_C, };

	/*
	 * Assign zone targets as below.
	 *   Zone target 0 for control A
	 *   Zone target 1 for control B
	 *   Zone target 2 for control C
	 */

	for (i = 0; i < LM3532_MAX_ZONE_CFG; i++) {
		ret = ti_lmu_update_bits(chip->lmu, lm3532_regs[i],
					 LM3532_ZONE_CFG_MASK,
					 i << LM3532_ZONE_CFG_SHIFT);
		if (ret)
			return ret;
	}

	return 0;
}

static int lm3532_bl_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3532_REG_ENABLE,
				  BIT(lmu_bl->bank_id),
				  enable << lmu_bl->bank_id);
}

static int lm3532_bl_set_brightness(struct ti_lmu_bl *lmu_bl, int brightness)
{
	u8 reg[] = { LM3532_REG_BRT_A, LM3532_REG_BRT_B, LM3532_REG_BRT_C, };

	return ti_lmu_write_byte(lmu_bl->chip->lmu, reg[lmu_bl->bank_id],
				 brightness);
}

static int lm3532_bl_select_pwm_bank(struct ti_lmu_bl *lmu_bl, int bank_id)
{
	struct ti_lmu *lmu = lmu_bl->chip->lmu;
	static int num_pwms;
	u8 pwm_sel;
	u8 mask[]  = { LM3532_PWM_SEL_A_MASK, LM3532_PWM_SEL_B_MASK,
		       LM3532_PWM_SEL_C_MASK, };
	u8 shift[] = { LM3532_PWM_SEL_A_SHIFT, LM3532_PWM_SEL_B_SHIFT,
		       LM3532_PWM_SEL_C_SHIFT, };

	if (num_pwms > 0)
		pwm_sel = LM3532_PWM2;
	else
		pwm_sel = LM3532_PWM1;

	num_pwms++;

	snprintf(lmu_bl->pwm_name, sizeof(lmu_bl->pwm_name), "%s:%d",
		 LMU_BL_DEFAULT_PWM_NAME, pwm_sel);

	return ti_lmu_update_bits(lmu, LM3532_REG_PWM_CFG_BASE + bank_id,
				  mask[bank_id],
				  (1 << shift[bank_id]) | pwm_sel);
}

static int lm3532_bl_string_configure(struct ti_lmu_bl *lmu_bl)
{
	struct ti_lmu *lmu = lmu_bl->chip->lmu;
	int bank_id = lmu_bl->bank_id;
	int is_detected = 0;
	int i, ret;
	u8 mask[]  = { LM3532_ILED1_CFG_MASK, LM3532_ILED2_CFG_MASK,
		       LM3532_ILED3_CFG_MASK, };
	u8 shift[]  = { LM3532_ILED1_CFG_SHIFT, LM3532_ILED2_CFG_SHIFT,
			LM3532_ILED3_CFG_SHIFT, };

	/* Assign control bank from backlight string configuration */
	for (i = 0; i < LM3532_BL_MAX_STRINGS; i++) {
		if (test_bit(i, &lmu_bl->bl_pdata->bl_string)) {
			ret = ti_lmu_update_bits(lmu, LM3532_REG_OUTPUT_CFG,
						 mask[i], bank_id << shift[i]);
			if (ret)
				return ret;

			is_detected = 1;
		}
	}

	if (!is_detected) {
		dev_err(lmu_bl->chip->dev, "No backlight string found\n");
		return -EINVAL;
	}

	if (lmu_bl->mode == BL_PWM_BASED)
		return lm3532_bl_select_pwm_bank(lmu_bl, bank_id);

	return 0;
}

static int lm3532_bl_set_current(struct ti_lmu_bl *lmu_bl)
{
	u8 reg[] = { LM3532_REG_IMAX_A, LM3532_REG_IMAX_B, LM3532_REG_IMAX_C };

	return ti_lmu_write_byte(lmu_bl->chip->lmu, reg[lmu_bl->bank_id],
				 lmu_bl->bl_pdata->imax);
}

static void lm3532_bl_effect_request(struct ti_lmu_bl *lmu_bl)
{
	int i;
	const char *name[] = {
		[BL_EFFECT_RAMPUP] = LM3532_EFFECT_RAMPUP,
		[BL_EFFECT_RAMPDN] = LM3532_EFFECT_RAMPDOWN,
	};

	for (i = BL_EFFECT_RAMPUP; i <= BL_EFFECT_RAMPDN; i++)
		ti_lmu_effect_request(name[i],
				      ti_lmu_backlight_effect_callback,
				      i, lmu_bl);
}

static int lm3532_bl_configure(struct ti_lmu_bl *lmu_bl)
{
	int ret;

	ret = lm3532_bl_string_configure(lmu_bl);
	if (ret)
		return ret;

	ret = lm3532_bl_set_current(lmu_bl);
	if (ret)
		return ret;

	/* LMU effect is optional, checking return value is not required */
	lm3532_bl_effect_request(lmu_bl);

	return 0;
}

static const struct ti_lmu_bl_ops lm3532_lmu_ops = {
	.init              = lm3532_bl_init,
	.configure         = lm3532_bl_configure,
	.update_brightness = lm3532_bl_set_brightness,
	.bl_enable         = lm3532_bl_enable,
	.max_brightness    = LM3532_MAX_BRIGHTNESS,
};

static int lm3532_bl_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct ti_lmu_bl_chip *chip;
	struct ti_lmu_bl *lmu_bl;

	chip = ti_lmu_backlight_init_device(&pdev->dev, lmu, &lm3532_lmu_ops);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	lmu_bl = ti_lmu_backlight_register(chip, lmu->pdata->bl_pdata,
					   lmu->pdata->num_backlights);
	if (IS_ERR(lmu_bl))
		return PTR_ERR(lmu_bl);

	platform_set_drvdata(pdev, lmu_bl);

	return 0;
}

static int lm3532_bl_remove(struct platform_device *pdev)
{
	struct ti_lmu_bl *lmu_bl = platform_get_drvdata(pdev);

	return ti_lmu_backlight_unregister(lmu_bl);
}

#ifdef CONFIG_OF
static const struct of_device_id lm3532_bl_of_match[] = {
	{ .compatible = "ti,lm3532-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3532_bl_of_match);
#endif

static struct platform_driver lm3532_bl_driver = {
	.probe = lm3532_bl_probe,
	.remove = lm3532_bl_remove,
	.driver = {
		.name = "lm3532-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3532_bl_of_match),
	},
};
module_platform_driver(lm3532_bl_driver);

MODULE_DESCRIPTION("TI LM3532 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3532-backlight");
