/*
 * leds-atc - Simple driver to control ATC led on a Qualcomm PMIC
 * Copyright (c) 2015, MMI. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/regmap.h>

#define LED_CFG_MASK	0x06
#define LED_CFG_SHIFT   1
#define LED_ON		0x03
#define LED_OFF		0x00

/**
 * @led_classdev - led class device
 * @spmi_device - spmi class device
 * @addr - spmi address for control register
 */
struct atc_led_data {
	struct led_classdev	cdev;
	struct platform_device	*pdev;
	struct regmap		*regmap;
	u32			addr;
};

static int
atc_led_masked_write(struct atc_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;

	rc = regmap_update_bits(led->regmap, addr, mask, val);
	if (rc)
		dev_err(&led->pdev->dev,
			"Unable to regmap_update_bits to addr=%x, rc(%d)\n",
			addr, rc);
	return rc;
}

static void atc_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	u8 val;
	struct atc_led_data *led;

	led = container_of(led_cdev, struct atc_led_data, cdev);

	if (value > LED_ON)
		value = LED_ON;

	val = value << LED_CFG_SHIFT;
	atc_led_masked_write(led, led->addr, LED_CFG_MASK, val);
	led->cdev.brightness = value;
}

static enum led_brightness atc_led_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int atc_leds_probe(struct platform_device *pdev)
{
	struct atc_led_data *led;
	struct device_node *node;
	int rc;
	uint reg;

	node = pdev->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	led = devm_kzalloc(&pdev->dev, sizeof(struct atc_led_data), GFP_KERNEL);
	if (!led) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->pdev = pdev;

	led->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!led->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "qcom,ctrl-reg", &led->addr);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Failure reading ctrl offset, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = of_property_read_string(node, "linux,name", &led->cdev.name);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Failure reading led name, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = regmap_read(led->regmap, led->addr, &reg);
	if (rc) {
		dev_err(&pdev->dev, "read reg failed(%d)\n", rc);
		return rc;
	}

	led->cdev.brightness_set = atc_led_set;
	led->cdev.brightness_get = atc_led_get;
	led->cdev.brightness = (reg & LED_CFG_MASK) >> LED_CFG_SHIFT;
	led->cdev.max_brightness = LED_ON;

	rc = led_classdev_register(&pdev->dev, &led->cdev);
	if (rc) {
		dev_err(&pdev->dev, "unable to ATC led rc=%d\n", rc);
		return -ENODEV;
	}

	dev_set_drvdata(&pdev->dev, led);
	dev_info(&pdev->dev, "%s success\n", __func__);
	return 0;
}

static int atc_leds_remove(struct platform_device *pdev)
{
	struct atc_led_data *led  = dev_get_drvdata(&pdev->dev);

	if (NULL != led)
		led_classdev_unregister(&led->cdev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,leds-atc",},
	{ },
};
#else
#define spmi_match_table NULL
#endif

static struct platform_driver atc_leds_driver = {
	.driver		= {
		.name	= "qcom,leds-atc",
		.of_match_table = spmi_match_table,
	},
	.probe		= atc_leds_probe,
	.remove		= atc_leds_remove,
};

static int __init atc_led_init(void)
{
	return platform_driver_register(&atc_leds_driver);
}
module_init(atc_led_init);

static void __exit atc_led_exit(void)
{
	platform_driver_unregister(&atc_leds_driver);
}
module_exit(atc_led_exit);

MODULE_DESCRIPTION("ATC LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-atc");
