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

#define FORCE_SW_MASK	0x05
#define FORCE_ON	0x05
#define FORCE_OFF	0x01

/**
 * @led_classdev - led class device
 * @spmi_device - spmi class device
 * @addr - spmi address for control register
 */
struct atc_led_data {
	struct led_classdev	cdev;
	struct spmi_device	*spmi_dev;
	u32			addr;
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
	val = (led->cdev.brightness) ? 1 : 0;
	spmi_masked_write(led, led->addr, FORCE_SW_MASK, val ? FORCE_ON : FORCE_OFF);
	led->cdev.brightness = value;
}

static enum led_brightness atc_led_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int atc_leds_probe(struct spmi_device *spmi)
{
	struct atc_led_data *led;
	struct resource *led_resource;
	struct device_node *node;
	u32 offset;
	int rc;

	node = spmi->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	led = devm_kzalloc(&spmi->dev, sizeof(struct atc_led_data), GFP_KERNEL);
	if (!led) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->spmi_dev = spmi;

	led_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!led_resource) {
		dev_err(&spmi->dev, "Unable to get LED base address\n");
		return -ENXIO;
	}

	rc = of_property_read_u32(node, "qcom,ctrl-reg", &offset);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading ctrl offset, rc = %d\n", rc);
		return -ENODEV;
	}

	led->addr = led_resource->start + offset;

	rc = of_property_read_string(node, "linux,name", &led->cdev.name);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Failure reading led name, rc = %d\n", rc);
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
	dev_info(&spmi->dev, "%s success\n", __func__);
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
