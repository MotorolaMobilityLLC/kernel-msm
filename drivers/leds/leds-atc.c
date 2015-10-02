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

#define FORCE_ON_MASK	0x03

/**
 * @led_classdev - led class device
 * @spmi_device - spmi class device
 * @addr - spmi address for control register
 * @on_data - ctrl reg pattern to turn on the LED
 * @off_data - ctrl reg patterm to turn off the LED
 */
struct atc_led_data {
	struct led_classdev	cdev;
	struct spmi_device	*spmi_dev;
	u16			addr;
	u8			on_data;
	u8			off_data;
	u8			mask;
};

static int
spmi_masked_write(struct atc_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Unable to read from addr=%#x, rc(%d)\n", addr, rc);
	}

	reg &= ~mask;
	reg |= val;

	rc = spmi_ext_register_writel(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc)
		dev_err(&led->spmi_dev->dev,
			"Unable to write to addr=%#x, rc(%d)\n", addr, rc);
	return rc;
}

static void atc_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	u8 val;
	struct atc_led_data *led;

	led = container_of(led_cdev, struct atc_led_data, cdev);
	val = (led->cdev.brightness) ? led->on_data : led->off_data;
	spmi_masked_write(led, led->addr, led->mask, val);
}

static enum led_brightness atc_led_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int atc_leds_probe(struct spmi_device *spmi)
{
	struct atc_led_data *led;
	struct device_node *node;
	int rc;
	u8 save;

	node = spmi->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	led = devm_kzalloc(&spmi->dev, sizeof(struct atc_led_data), GFP_KERNEL);
	if (!led) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->spmi_dev = spmi;

	rc = of_property_read_u16(node, "qcom,ctrl-reg", &led->addr);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading ctrl offset, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = of_property_read_u8(node, "qcom,on-data", &led->on_data);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading ctrl on_data, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = of_property_read_u8(node, "qcom,off-data", &led->off_data);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading ctrl off_data, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = of_property_read_u8(node, "qcom,mask", &led->mask);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading ctrl mask, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = of_property_read_string(node, "linux,name", &led->cdev.name);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading led name, rc = %d\n", rc);
		return -ENODEV;
	}

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl, led->spmi_dev->sid,
		led->addr, &save, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Unable to read from addr=%#x, rc(%d)\n", led->addr, rc);
		return -ENODEV;
	}

	led->cdev.brightness_set = atc_led_set;
	led->cdev.brightness_get = atc_led_get;
	led->cdev.brightness = LED_OFF;

	rc = led_classdev_register(&spmi->dev, &led->cdev);
	if (rc) {
		dev_err(&spmi->dev, "unable to ATC led rc=%d\n", rc);
		return -ENODEV;
	}

	dev_set_drvdata(&spmi->dev, led);
	dev_info(&spmi->dev, "Probe success register value %#x\n", save);
	return 0;
}

static int atc_leds_remove(struct spmi_device *spmi)
{
	struct atc_led_data *led  = dev_get_drvdata(&spmi->dev);

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

static struct spmi_driver atc_leds_driver = {
	.driver		= {
		.name	= "qcom,leds-atc",
		.of_match_table = spmi_match_table,
	},
	.probe		= atc_leds_probe,
	.remove		= atc_leds_remove,
};

static int __init atc_led_init(void)
{
	return spmi_driver_register(&atc_leds_driver);
}
module_init(atc_led_init);

static void __exit atc_led_exit(void)
{
	spmi_driver_unregister(&atc_leds_driver);
}
module_exit(atc_led_exit);

MODULE_DESCRIPTION("ATC LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-atc");
