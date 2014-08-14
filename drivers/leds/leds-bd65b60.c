/*
* Simple driver for ROHM Semiconductor BD65B60GWL Backlight driver chip
* Copyright (C) 2014 ROHM Semiconductor.com
* Copyright (C) 2014 MMI
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/platform_data/leds-bd65b60.h>
#include <linux/of.h>

#define BD65B60_NAME			"bd65b60"
#define BD65B60_MAX_BRIGHTNESS		255
#define BD65B60_DEFAULT_BRIGHTNESS	255
#define BD65B60_DEFAULT_TRIGGER		"bkl-trigger"
#define BD65B60_DEFAULT_OVP_VAL		BD65B60_35V_OVP
#define	BD65B60_DEFAULT_PWM_PERIOD	0

#define REG_SFTRST		0x00
#define REG_COMSET1		0x01
#define REG_COMSET2		0x02
#define REG_LEDSEL		0x03
#define REG_ILED		0x05
#define REG_CTRLSET		0x07
#define REG_SLEWSET		0x08
#define	REG_PON			0x0E
#define	REG_MAX			REG_PON

#define	PWMEN_MASK		0x20
#define	OVP_MASK		0x18
#define	LEDSEL_MASK		0x05

#define INT_DEBOUNCE_MSEC	10

struct bd65b60_chip {
	struct bd65b60_platform_data *pdata;
	struct regmap *regmap;
	struct device *dev;
	struct workqueue_struct *ledwq;
	struct work_struct ledwork;
	struct led_classdev cdev;
};

static const struct regmap_config bd65b60_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

/* i2c access */
static int bd65b60_read(struct bd65b60_chip *pchip, unsigned int reg)
{
	int rval;
	unsigned int reg_val;

	rval = regmap_read(pchip->regmap, reg, &reg_val);
	if (rval < 0)
		return rval;
	return reg_val & 0xFF;
}

static int bd65b60_write(struct bd65b60_chip *pchip,
			 unsigned int reg, unsigned int data)
{
	int rc;
	rc = regmap_write(pchip->regmap, reg, data);
	if (rc < 0)
		dev_err(pchip->dev, "i2c failed to write reg %#x", reg);
	return rc;
}

static int bd65b60_update(struct bd65b60_chip *pchip,
			  unsigned int reg, unsigned int mask,
			  unsigned int data)
{
	int rc;
	rc = regmap_update_bits(pchip->regmap, reg, mask, data);
	if (rc < 0)
		dev_err(pchip->dev, "i2c failed to update reg %#x", reg);
	return rc;
}

/* initialize chip */
static int bd65b60_chip_init(struct i2c_client *client)
{
	int rval = 0;
	struct bd65b60_chip *pchip = i2c_get_clientdata(client);
	struct bd65b60_platform_data *pdata = pchip->pdata;

	if (!pdata->no_reset)
		rval |= bd65b60_write(pchip, REG_SFTRST, 0x01);
	/* set common settings/OVP register */
	rval |= bd65b60_update(pchip, REG_COMSET1, OVP_MASK, pdata->ovp_val);
	/* set control */
	rval |= bd65b60_update(pchip, REG_LEDSEL, LEDSEL_MASK, pdata->led_sel);
	/* turn on LED Driver */
	rval |= bd65b60_write(pchip, REG_PON, 0x01);
	if (rval < 0)
		dev_err(&client->dev, "i2c failed to access register");
	return rval;
}

/* set brightness */
static void bd65b60_brightness_set(struct work_struct *work)
{
	struct bd65b60_chip *pchip = container_of(work,
		struct bd65b60_chip, ledwork);
	unsigned int level = pchip->cdev.brightness;
	static int old_level = -1;
	struct bd65b60_platform_data *pdata = pchip->pdata;

	/* set configure pwm input on first brightness command */
	if (old_level == -1 && pdata->pwm_on) {
		dev_info(pchip->dev, "Enabling CABC");
		bd65b60_update(pchip, REG_CTRLSET, PWMEN_MASK, pdata->pwm_ctrl);
	}

	if (level != old_level && old_level == 0) {
		dev_info(pchip->dev, "backlight on");
		bd65b60_write(pchip, REG_PON, 0x01);
	} else if (level == 0 && old_level != 0)
		dev_info(pchip->dev, "backlight off");
	old_level = level;

	bd65b60_write(pchip, REG_ILED, level);
	if (!level)
		/* turn off LED because 0 in REG_ILED = 1/256 * Imax */
		bd65b60_write(pchip, REG_PON, 0x00);

	return;
}

static enum led_brightness bd65b60_led_get(struct led_classdev *led_cdev)
{
	struct bd65b60_chip *pchip;
	int rc;

	pchip = container_of(led_cdev, struct bd65b60_chip, cdev);
	rc = bd65b60_read(pchip, REG_ILED);
	return led_cdev->brightness;
}

static void bd65b60_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct bd65b60_chip *pchip;

	pchip = container_of(led_cdev, struct bd65b60_chip, cdev);
	if (value < LED_OFF) {
		dev_err(pchip->dev, "Invalid brightness value");
		return;
	}

	if (value > led_cdev->max_brightness)
		value = led_cdev->max_brightness;

	led_cdev->brightness = value;
	schedule_work(&pchip->ledwork);
}


#ifdef CONFIG_OF
static int bd65b60_dt_init(struct i2c_client *client,
	struct bd65b60_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	int rc;

	rc = of_property_read_string(np, "linux,name", &pdata->name);
	if (rc) {
		dev_err(&client->dev, "No linux name provided");
		return rc;
	}

	pdata->trigger = BD65B60_DEFAULT_TRIGGER;
	of_property_read_string(np, "linux,default-trigger", &pdata->trigger);

	if (of_property_read_bool(np, "rohm,led1-used"))
		pdata->led_sel |= BD65B60_LED1SEL;
	if (of_property_read_bool(np, "rohm,led2-used"))
		pdata->led_sel |= BD65B60_LED2SEL;

	pdata->no_reset = of_property_read_bool(np, "rohm,no-reset");

	pdata->default_on = of_property_read_bool(np, "rohm,default-on");
	pdata->init_level = BD65B60_DEFAULT_BRIGHTNESS;
	if (pdata->default_on)
		of_property_read_u32(np, "rohm,init-level", &pdata->init_level);

	pdata->ovp_val = BD65B60_DEFAULT_OVP_VAL;
	of_property_read_u32(np, "rohm,ovp-val", &pdata->ovp_val);

	pdata->pwm_on = of_property_read_bool(np, "rohm,pwm-on");
	pdata->pwm_ctrl = (pdata->pwm_on)
		? BD65B60_PWM_ENABLE : BD65B60_PWM_DISABLE;

	return 0;

}

static const struct of_device_id of_bd65b60_leds_match[] = {
	{ .compatible = "rohm,bd65b60", },
	{},
};

#else
static int bd65b60_dt_init(struct i2c_client *client,
	struct bd65b60_platform_data *pdata)
{
	return ERR_PTR(-ENODEV);
}

static const struct of_device_id of_bd65b60_leds_match;

#endif

static int bd65b60_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct bd65b60_platform_data *pdata = dev_get_platdata(&client->dev);
	struct bd65b60_chip *pchip;
	int rc;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "fail : i2c functionality check");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct bd65b60_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	if (!pdata) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct bd65b60_platform_data), GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;

		rc = bd65b60_dt_init(client, pdata);
		if (rc)
			return rc;
	}

	i2c_set_clientdata(client, pchip);
	pchip->pdata = pdata;
	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &bd65b60_regmap);
	if (IS_ERR(pchip->regmap)) {
		rc = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "fail : allocate reg. map: %d", rc);
		return rc;
	}

	/* chip initialize */
	rc = bd65b60_chip_init(client);
	if (rc < 0) {
		dev_err(&client->dev, "fail : init chip");
		return rc;
	}

	/* led classdev register */
	pchip->cdev.brightness_set = bd65b60_led_set;
	pchip->cdev.brightness_get = bd65b60_led_get;
	pchip->cdev.max_brightness = BD65B60_MAX_BRIGHTNESS;
	pchip->cdev.name = pdata->name;
	pchip->cdev.default_trigger = pdata->trigger;
	INIT_WORK(&pchip->ledwork, bd65b60_brightness_set);

	rc = led_classdev_register(&client->dev, &pchip->cdev);
	if (rc) {
		dev_err(&client->dev, "unable to register led rc=%d", rc);
		return rc;
	}

	if (pdata->default_on)
		bd65b60_led_set(&pchip->cdev, pdata->init_level);

	return 0;
}

static int bd65b60_remove(struct i2c_client *client)
{
	struct bd65b60_chip *pchip = i2c_get_clientdata(client);

	bd65b60_write(pchip, REG_PON, 0);
	led_classdev_unregister(&pchip->cdev);

	return 0;
}

static const struct i2c_device_id bd65b60_id[] = {
	{BD65B60_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bd65b60_id);

static struct i2c_driver bd65b60_i2c_driver = {
	.driver = {
		.name = BD65B60_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_bd65b60_leds_match),
		  },
	.probe = bd65b60_probe,
	.remove = bd65b60_remove,
	.id_table = bd65b60_id,
};

module_i2c_driver(bd65b60_i2c_driver);

MODULE_DESCRIPTION("ROHM Semiconductor Backlight driver for bd65b60");
MODULE_LICENSE("GPL v2");
