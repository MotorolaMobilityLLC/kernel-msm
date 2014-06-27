/*
 * TI LMU Backlight Common Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LMU backlight driver supports common features as below.
 *
 *   - Consistent device control flow by using ti_lmu_bl_ops
 *   - Control bank assignment from the platform data
 *   - Backlight subsystem control
 *   - PWM brightness control
 *   - Shared device tree node
 *
 * Sequence of LMU backlight control
 *
 *   (Chip dependent backlight driver)            (TI LMU Backlight Common)
 *
 *     Operation configuration
 *     ti_lmu_backlight_init_device()   --->
 *     Initialization                   <---        ops->init()
 *
 *     ti_lmu_backlight_register()      --->
 *     Backlight configuration          <---        ops->configure()
 *
 *                                                  Runtime brightness control
 *     Enable register control          <---        ops->bl_enable()
 *     Brightness register control      <---        ops->update_brightness()
 *
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/fb.h>

#include "ti-lmu-backlight.h"

#define DEFAULT_BL_NAME			"lcd-backlight"

static int ti_lmu_backlight_enable(struct ti_lmu_bl *lmu_bl, int enable)
{
	const struct ti_lmu_bl_ops *ops = lmu_bl->chip->ops;

	if (ops->bl_enable)
		return ops->bl_enable(lmu_bl, enable);

	return 0;
}

static void ti_lmu_backlight_pwm_ctrl(struct ti_lmu_bl *lmu_bl, int br,
				      int max_br)
{
	struct pwm_device *pwm;
	unsigned int duty, period;

	/* Request a PWM device with the consumer name */
	if (!lmu_bl->pwm) {
		pwm = devm_pwm_get(lmu_bl->chip->dev, lmu_bl->pwm_name);
		if (IS_ERR(pwm)) {
			dev_err(lmu_bl->chip->dev,
				"Can not get PWM device: %s\n",
				lmu_bl->pwm_name);
			return;
		}
		lmu_bl->pwm = pwm;
	}

	period = lmu_bl->bl_pdata->pwm_period;
	duty = br * period / max_br;

	pwm_config(lmu_bl->pwm, duty, period);
	if (duty)
		pwm_enable(lmu_bl->pwm);
	else
		pwm_disable(lmu_bl->pwm);
}

static int ti_lmu_backlight_update_status(struct backlight_device *bl_dev)
{
	struct ti_lmu_bl *lmu_bl = bl_get_data(bl_dev);
	const struct ti_lmu_bl_ops *ops = lmu_bl->chip->ops;
	int ret = 0;
	int brt;

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
		bl_dev->props.brightness = 0;

	brt = bl_dev->props.brightness;
	if (brt > 0)
		ret = ti_lmu_backlight_enable(lmu_bl, 1);
	else
		ret = ti_lmu_backlight_enable(lmu_bl, 0);

	if (ret)
		return ret;

	if (lmu_bl->mode == BL_PWM_BASED)
		ti_lmu_backlight_pwm_ctrl(lmu_bl, brt,
					  bl_dev->props.max_brightness);

	/*
	 * In some devices, additional handling is required after PWM control.
	 * So, just call device-specific brightness function.
	 */
	if (ops->update_brightness)
		return ops->update_brightness(lmu_bl, brt);

	return 0;
}

static int ti_lmu_backlight_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static int ti_lmu_check_fb(struct backlight_device *bd, struct fb_info *fi)
{
	return 0;
}

static const struct backlight_ops lmu_bl_common_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = ti_lmu_backlight_update_status,
	.get_brightness = ti_lmu_backlight_get_brightness,
	.check_fb = ti_lmu_check_fb,
};

static int ti_lmu_backlight_parse_dt(struct device *dev, struct ti_lmu *lmu)
{
	struct ti_lmu_backlight_platform_data *pdata;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	int num_backlights;
	int i = 0;
	u8 imax_mA;

	if (!node) {
		dev_err(dev, "No device node exists\n");
		return -ENODEV;
	}

	num_backlights = of_get_child_count(node);
	if (num_backlights == 0) {
		dev_err(dev, "No backlight strings\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata) * num_backlights, GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	for_each_child_of_node(node, child) {
		of_property_read_string(child, "bl-name", &pdata[i].name);

		/* Make backlight strings */
		pdata[i].bl_string = 0;
		if (of_find_property(child, "hvled1-used", NULL))
			pdata[i].bl_string |= LMU_HVLED1;
		if (of_find_property(child, "hvled2-used", NULL))
			pdata[i].bl_string |= LMU_HVLED2;
		if (of_find_property(child, "hvled3-used", NULL))
			pdata[i].bl_string |= LMU_HVLED3;

		of_property_read_u8(child, "max-current-milliamp", &imax_mA);
		pdata[i].imax = ti_lmu_get_current_code(imax_mA);

		if (!of_property_read_u8(child, "initial-brightness",
				    &pdata[i].init_brightness))
			pdata[i].default_on = true;

		/* Light effect */
		of_property_read_u32(child, "ramp-up", &pdata[i].ramp_up_ms);
		of_property_read_u32(child, "ramp-down",
				     &pdata[i].ramp_down_ms);

		/* PWM mode */
		of_property_read_u32(child, "pwm-period", &pdata[i].pwm_period);

		i++;
	}

	lmu->pdata->bl_pdata = pdata;
	lmu->pdata->num_backlights = num_backlights;

	return 0;
}

struct ti_lmu_bl_chip *
ti_lmu_backlight_init_device(struct device *dev, struct ti_lmu *lmu,
			     const struct ti_lmu_bl_ops *ops)
{
	struct ti_lmu_bl_chip *chip;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return ERR_PTR(-ENOMEM);

	chip->dev = dev;
	chip->lmu = lmu;
	chip->ops = ops;

	if (!lmu->pdata->bl_pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = ti_lmu_backlight_parse_dt(dev, lmu);
		else
			return ERR_PTR(-ENODEV);

		if (ret)
			return ERR_PTR(ret);
	}

	if (chip->ops->init) {
		ret = chip->ops->init(chip);
		if (ret)
			return ERR_PTR(ret);
	}

	return chip;
}
EXPORT_SYMBOL_GPL(ti_lmu_backlight_init_device);

static void ti_lmu_backlight_set_ctrl_mode(struct ti_lmu_bl *lmu_bl)
{
	struct ti_lmu_backlight_platform_data *pdata = lmu_bl->bl_pdata;

	if (pdata->pwm_period > 0)
		lmu_bl->mode = BL_PWM_BASED;
	else
		lmu_bl->mode = BL_REGISTER_BASED;
}

static int ti_lmu_backlight_configure(struct ti_lmu_bl *lmu_bl)
{
	const struct ti_lmu_bl_ops *ops = lmu_bl->chip->ops;

	if (ops->configure)
		return ops->configure(lmu_bl);

	return 0;
}

static int ti_lmu_backlight_add_device(struct ti_lmu_bl *lmu_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct ti_lmu_backlight_platform_data *pdata = lmu_bl->bl_pdata;
	int max_brightness = lmu_bl->chip->ops->max_brightness;
	char name[20];

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = pdata ? pdata->init_brightness : 0;
	props.max_brightness = max_brightness;

	/* Backlight device name */
	if (!pdata->name)
		snprintf(name, sizeof(name), "%s:%d", DEFAULT_BL_NAME,
			 lmu_bl->bank_id);
	else
		snprintf(name, sizeof(name), "%s", pdata->name);

	bl_dev = backlight_device_register(name, lmu_bl->chip->dev, lmu_bl,
					   &lmu_bl_common_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lmu_bl->bl_dev = bl_dev;

	return 0;
}

struct ti_lmu_bl *
ti_lmu_backlight_register(struct ti_lmu_bl_chip *chip,
			  struct ti_lmu_backlight_platform_data *pdata,
			  int num_backlights)
{
	struct ti_lmu_bl *lmu_bl, *each;
	int i, ret;

	lmu_bl = devm_kzalloc(chip->dev, sizeof(*lmu_bl) * num_backlights,
			      GFP_KERNEL);
	if (!lmu_bl)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_backlights; i++) {
		each = lmu_bl + i;
		each->bank_id = i;
		each->chip = chip;
		each->bl_pdata = pdata + i;

		ti_lmu_backlight_set_ctrl_mode(lmu_bl);

		ret = ti_lmu_backlight_configure(each);
		if (ret) {
			dev_err(chip->dev, "Backlight config err: %d\n", ret);
			goto err;
		}

		ret = ti_lmu_backlight_add_device(each);
		if (ret) {
			dev_err(chip->dev, "Backlight device err: %d\n", ret);
			goto cleanup_backlights;
		}

		if (pdata->default_on)
			backlight_update_status(each->bl_dev);
	}

	chip->num_backlights = num_backlights;

	return lmu_bl;

cleanup_backlights:
	while (--i >= 0) {
		each = lmu_bl + i;
		backlight_device_unregister(each->bl_dev);
	}
err:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(ti_lmu_backlight_register);

int ti_lmu_backlight_unregister(struct ti_lmu_bl *lmu_bl)
{
	struct ti_lmu_bl *each;
	struct backlight_device *bl_dev;
	int num_backlights = lmu_bl->chip->num_backlights;
	int i;

	for (i = 0; i < num_backlights; i++) {
		each = lmu_bl + i;

		bl_dev = each->bl_dev;
		bl_dev->props.brightness = 0;
		backlight_update_status(bl_dev);
		backlight_device_unregister(bl_dev);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ti_lmu_backlight_unregister);

/*
 * This callback function is invoked in case the LMU effect driver is
 * requested successfully.
 */
void ti_lmu_backlight_effect_callback(struct ti_lmu_effect *lmu_effect,
				      int req_id, void *data)
{
	struct ti_lmu_bl *lmu_bl = data;
	unsigned int ramp_time;

	if (req_id == BL_EFFECT_RAMPUP)
		ramp_time = lmu_bl->bl_pdata->ramp_up_ms;
	else if (req_id == BL_EFFECT_RAMPDN)
		ramp_time = lmu_bl->bl_pdata->ramp_down_ms;
	else
		return;

	ti_lmu_effect_set_ramp(lmu_effect, ramp_time);
}
EXPORT_SYMBOL_GPL(ti_lmu_backlight_effect_callback);

MODULE_DESCRIPTION("TI LMU Backlight Common Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
