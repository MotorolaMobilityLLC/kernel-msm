/*
 * TI LM3695 Backlight Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LM3695 backlight driver consists of two parts
 *
 *   1) LM3695 chip specific part: this file
 *      Define device specific operations
 *      Register LMU backlight driver
 *
 *   2) LMU backlight common driver: ti-lmu-backlight
 *      General backlight subsystem control
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "ti-lmu-backlight.h"

#define LM3695_FULL_STRINGS		(LMU_HVLED1 | LMU_HVLED2)
#define LM3695_MAX_BRIGHTNESS		2047

static int lm3695_bl_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	int ret;

	ret = ti_lmu_update_bits(lmu_bl->chip->lmu, LM3695_REG_GP,
				 LM3695_BL_EN_MASK, enable);
	if (ret)
		return ret;

	/* Wait time for brightness register wake up */
	usleep_range(600, 700);

	return 0;
}

static int lm3695_bl_set_brightness(struct ti_lmu_bl *lmu_bl, int brightness)
{
	u8 data;
	int ret;

	data = brightness & LM3695_BRT_LSB_MASK;
	ret = ti_lmu_update_bits(lmu_bl->chip->lmu, LM3695_REG_BRT_LSB,
				 LM3695_BRT_LSB_MASK, data);
	if (ret)
		return ret;

	data = (brightness >> LM3695_BRT_MSB_SHIFT) & 0xFF;
	return ti_lmu_write_byte(lmu_bl->chip->lmu, LM3695_REG_BRT_MSB,
				 data);
}

static int lm3695_bl_init(struct ti_lmu_bl_chip *chip)
{
	return ti_lmu_update_bits(chip->lmu, LM3695_REG_GP,
				  LM3695_BRT_RW_MASK, LM3695_BRT_SET_RW);
}

static int lm3695_bl_configure(struct ti_lmu_bl *lmu_bl)
{
	u8 val;

	if (lmu_bl->bl_pdata->bl_string == LM3695_FULL_STRINGS)
		val = LM3695_BL_TWO_STRINGS;
	else
		val = LM3695_BL_ONE_STRING;

	return ti_lmu_update_bits(lmu_bl->chip->lmu, LM3695_REG_GP,
				  LM3695_BL_STRING_MASK, val);
}

static const struct ti_lmu_bl_ops lm3695_lmu_ops = {
	.init              = lm3695_bl_init,
	.configure         = lm3695_bl_configure,
	.update_brightness = lm3695_bl_set_brightness,
	.bl_enable         = lm3695_bl_enable,
	.max_brightness    = LM3695_MAX_BRIGHTNESS,
};

static int lm3695_bl_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct ti_lmu_bl *lmu_bl;
	struct ti_lmu_bl_chip *chip;

	chip = ti_lmu_backlight_init_device(&pdev->dev, lmu, &lm3695_lmu_ops);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	lmu_bl = ti_lmu_backlight_register(chip, lmu->pdata->bl_pdata, 1);
	if (IS_ERR(lmu_bl))
		return PTR_ERR(lmu_bl);

	platform_set_drvdata(pdev, lmu_bl);

	return 0;
}

static int lm3695_bl_remove(struct platform_device *pdev)
{
	struct ti_lmu_bl *lmu_bl = platform_get_drvdata(pdev);

	return ti_lmu_backlight_unregister(lmu_bl);
}

#ifdef CONFIG_OF
static const struct of_device_id lm3695_bl_of_match[] = {
	{ .compatible = "ti,lm3695-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3695_bl_of_match);
#endif

static struct platform_driver lm3695_bl_driver = {
	.probe = lm3695_bl_probe,
	.remove = lm3695_bl_remove,
	.driver = {
		.name = "lm3695-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3695_bl_of_match),
	},
};
module_platform_driver(lm3695_bl_driver);

MODULE_DESCRIPTION("TI LM3695 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3695-backlight");
