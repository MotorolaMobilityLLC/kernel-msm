/*
* Simple driver for Intersil Semiconductor ISL98611GWL Backlight driver chip
* Copyright (C) 2015 MMI
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
#include <linux/pwm.h>
#include <linux/platform_data/leds-isl98611.h>
#include <linux/of.h>

#define ISL98611_NAME				"isl98611"
#define ISL98611_MAX_BRIGHTNESS			255
#define ISL98611_DEFAULT_BRIGHTNESS		255
#define ISL98611_DEFAULT_TRIGGER		"bkl-trigger"
#define ISL98611_DEFAULT_VN_LEVEL		20
#define ISL98611_DEFAULT_VP_LEVEL		20

#define REG_STATUS		0x01
#define REG_ENABLE		0x02
#define REG_VPVNHEAD		0x03
#define REG_VPLEVEL		0x04
#define REG_VNLEVEL		0x05
#define REG_BOOSTCTRL		0x06
#define REG_VNPFM		0x09
#define REG_BRGHT_MSB		0x10
#define REG_BRGHT_LSB		0x11
#define REG_CURRENT		0x12
#define REG_DIMMCTRL		0x13
#define REG_PWMCTRL		0x14
#define REG_VLEDFREQ		0x16
#define REG_VLEDCONFIG		0x17
#define REG_MAX			0x17

#define VPLEVEL_MASK		0x1F
#define VNLEVEL_MASK		0x1F
#define RESET_MASK		0x80
#define VPEN_MASK		0x04
#define VNEN_MASK		0x02
#define CABC_MASK		0xF0
#define PWMRES_MASK		0x80
#define CURRENT_MASK		0xC0

#define VPON_VAL		0x04
#define VNON_VAL		0x02
#define VPOFF_VAL		0x00
#define VNOFF_VAL		0x00
#define RESET_VAL		0x00
#define CABC_VAL		0x80
#define PWMRES_VAL		0x80

struct isl98611_chip {
	struct isl98611_platform_data *pdata;
	struct regmap *regmap;
	struct pwm_device *pwmd;
	struct device *dev;
	struct workqueue_struct *ledwq;
	struct work_struct ledwork;
	struct led_classdev cdev;
};

static const struct regmap_config isl98611_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

/* i2c access */
static int isl98611_read(struct isl98611_chip *pchip, unsigned int reg)
{
	int rval;
	unsigned int reg_val;

	rval = regmap_read(pchip->regmap, reg, &reg_val);
	if (rval < 0)
		return rval;
	return reg_val & 0xFF;
}

static int isl98611_write(struct isl98611_chip *pchip,
			 unsigned int reg, unsigned int data)
{
	int rc;
	rc = regmap_write(pchip->regmap, reg, data);
	if (rc < 0)
		dev_err(pchip->dev, "i2c failed to write reg %#x", reg);
	return rc;
}

static int isl98611_update(struct isl98611_chip *pchip,
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
static int isl98611_chip_init(struct i2c_client *client)
{
	int rval = 0;
	struct isl98611_chip *pchip = i2c_get_clientdata(client);
	struct isl98611_platform_data *pdata = pchip->pdata;

	if (!pdata->no_reset)
		rval |= isl98611_update(pchip, REG_ENABLE,
			RESET_MASK, RESET_VAL);

	if (pdata->cabc_off)
		isl98611_update(pchip, REG_DIMMCTRL, CABC_MASK, 0x00);

	/* set VN/VP power supply levels */
	if (pdata->vp_level != 0) {
		rval |= isl98611_update(pchip, REG_VPLEVEL,
			VPLEVEL_MASK, pdata->vp_level);
		rval |= isl98611_update(pchip, REG_ENABLE, VPEN_MASK, VPON_VAL);
	} else
		rval |= isl98611_update(pchip, REG_ENABLE,
			VPEN_MASK, VPOFF_VAL);

	if (pdata->vn_level != 0) {
		rval |= isl98611_update(pchip, REG_VNLEVEL,
			VNLEVEL_MASK, pdata->vn_level);
		rval |= isl98611_update(pchip, REG_ENABLE, VNEN_MASK, VNON_VAL);
	} else
		rval |= isl98611_update(pchip, REG_ENABLE,
			VNEN_MASK, VNOFF_VAL);

	return rval;
}

/* set brightness */
static void isl98611_brightness_set(struct work_struct *work)
{
	struct isl98611_chip *pchip = container_of(work,
		struct isl98611_chip, ledwork);
	unsigned int level = pchip->cdev.brightness;
	static int old_level = -1;
	struct isl98611_platform_data *pdata = pchip->pdata;

	/* set configure pwm input on first brightness command */
	if (old_level == -1 && !pdata->cabc_off) {
		dev_info(pchip->dev, "Enabling CABC");
		isl98611_update(pchip, REG_PWMCTRL, PWMRES_MASK, PWMRES_VAL);
		isl98611_update(pchip, REG_DIMMCTRL, CABC_MASK, CABC_VAL);
	}

	if (level != old_level && old_level == 0)
		dev_info(pchip->dev, "backlight on");
	 else if (level == 0 && old_level != 0)
		dev_info(pchip->dev, "backlight off");
	old_level = level;

	isl98611_write(pchip, REG_BRGHT_MSB, level);

	return;
}

static enum led_brightness isl98611_led_get(struct led_classdev *led_cdev)
{
	struct isl98611_chip *pchip;
	int rc;

	pchip = container_of(led_cdev, struct isl98611_chip, cdev);
	rc = isl98611_read(pchip, REG_BRGHT_MSB);
	return led_cdev->brightness;
}

static void isl98611_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct isl98611_chip *pchip;

	pchip = container_of(led_cdev, struct isl98611_chip, cdev);
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
static int isl98611_dt_init(struct i2c_client *client,
	struct isl98611_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	int rc;

	rc = of_property_read_string(np, "linux,name", &pdata->name);
	if (rc) {
		dev_err(&client->dev, "No linux name provided");
		return rc;
	}

	pdata->trigger = ISL98611_DEFAULT_TRIGGER;
	of_property_read_string(np, "linux,default-trigger", &pdata->trigger);

	pdata->no_reset = of_property_read_bool(np, "intersil,no-reset");

	pdata->cabc_off = of_property_read_bool(np, "intersil,cabc-disable");

	pdata->default_on = of_property_read_bool(np, "intersil,default-on");
	pdata->init_level = ISL98611_DEFAULT_BRIGHTNESS;
	if (pdata->default_on)
		of_property_read_u32(np, "intersil,init-level",
			&pdata->init_level);

	pdata->vp_level = ISL98611_DEFAULT_VP_LEVEL;
	of_property_read_u32(np, "intersil,vp", &pdata->vp_level);

	pdata->vn_level = ISL98611_DEFAULT_VN_LEVEL;
	of_property_read_u32(np, "intersil,vn", &pdata->vn_level);

	pdata->led_current = ISL98611_20MA;
	of_property_read_u32(np, "intersil,led-current", &pdata->led_current);

	return 0;

}

static const struct of_device_id of_isl98611_leds_match[] = {
	{ .compatible = "intersil,isl98611", },
	{},
};

#else
static int isl98611_dt_init(struct i2c_client *client,
	struct isl98611_platform_data *pdata)
{
	return ERR_PTR(-ENODEV);
}

static const struct of_device_id of_isl98611_leds_match;

#endif

static int isl98611_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct isl98611_platform_data *pdata = dev_get_platdata(&client->dev);
	struct isl98611_chip *pchip;
	int rc;
	unsigned int status;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "fail : i2c functionality check");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct isl98611_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	if (!pdata) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct isl98611_platform_data), GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;

		rc = isl98611_dt_init(client, pdata);
		if (rc)
			return rc;
	}

	i2c_set_clientdata(client, pchip);
	pchip->pdata = pdata;
	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &isl98611_regmap);
	if (IS_ERR(pchip->regmap)) {
		rc = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "fail : allocate reg. map: %d", rc);
		return rc;
	}

	/* chip initialize */
	rc = isl98611_chip_init(client);
	if (rc < 0) {
		dev_err(&client->dev, "fail : init chip");
		return rc;
	}

	/* led classdev register */
	pchip->cdev.brightness_set = isl98611_led_set;
	pchip->cdev.brightness_get = isl98611_led_get;
	pchip->cdev.max_brightness = ISL98611_MAX_BRIGHTNESS;
	pchip->cdev.name = pdata->name;
	pchip->cdev.default_trigger = pdata->trigger;
	INIT_WORK(&pchip->ledwork, isl98611_brightness_set);

	rc = led_classdev_register(&client->dev, &pchip->cdev);
	if (rc) {
		dev_err(&client->dev, "unable to register led rc=%d", rc);
		return rc;
	}

	if (pdata->default_on)
		isl98611_led_set(&pchip->cdev, pdata->init_level);

	status = isl98611_read(pchip, REG_STATUS);
	dev_info(pchip->dev, "probe success chip hw status %#x", status);

	return 0;
}

static int isl98611_remove(struct i2c_client *client)
{
	struct isl98611_chip *pchip = i2c_get_clientdata(client);

	led_classdev_unregister(&pchip->cdev);

	return 0;
}

static const struct i2c_device_id isl98611_id[] = {
	{ISL98611_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, isl98611_id);

static struct i2c_driver isl98611_i2c_driver = {
	.driver = {
		.name = ISL98611_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_isl98611_leds_match),
		  },
	.probe = isl98611_probe,
	.remove = isl98611_remove,
	.id_table = isl98611_id,
};

module_i2c_driver(isl98611_i2c_driver);

MODULE_DESCRIPTION("Intersil  Backlight driver for ISL98611");
MODULE_LICENSE("GPL v2");
