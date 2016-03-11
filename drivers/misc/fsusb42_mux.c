/*
 * Copyright (C) 2016 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/fsusb42.h>

struct fsusb42_info {
	struct gpio sel_gpio;
	struct regulator *vdd;
	bool vdd_enabled;
	enum fsusb42_state state;
	struct mutex lock;
	struct device *dev;
};

static struct fsusb42_info *the_info;

static int fsusb42_vdd_enable(struct fsusb42_info *info, bool enable)
{
	int rc = 0;

	dev_dbg(info->dev, "reg (%s)\n", enable ? "HPM" : "LPM");

	if ((enable && info->vdd_enabled) || (!enable && !info->vdd_enabled))
		return 0;

	if (enable) {
		rc = regulator_enable(info->vdd);
	} else {
		rc = regulator_disable(info->vdd);
		if (rc)
			dev_err(info->dev, "Unable to enable vdd\n");
	}

	if (rc)
		dev_err(info->dev, "Unable to %sable vdd\n",
			enable ? "en" : "dis");
	else
		info->vdd_enabled = enable;

	return rc;
}

enum fsusb42_state fsusb42_get_state(void)
{
	struct fsusb42_info *info = the_info;
	enum fsusb42_state state = FSUSB_OFF;

	if (!info)
		return state;

	mutex_lock(&info->lock);
	state = info->state;
	mutex_unlock(&info->lock);

	return state;
}
EXPORT_SYMBOL(fsusb42_get_state);

void fsusb42_set_state(enum fsusb42_state state)
{
	struct fsusb42_info *info = the_info;

	if (!info)
		return;

	dev_dbg(info->dev, "Set State to %d\n", state);

	mutex_lock(&info->lock);

	if (state == FSUSB_OFF) {
		gpio_set_value(info->sel_gpio.gpio, 0);
		fsusb42_vdd_enable(info, false);
	} else {
		fsusb42_vdd_enable(info, true);

		if (state == FSUSB_STATE_USB)
			gpio_set_value(info->sel_gpio.gpio, 0);
		else
			gpio_set_value(info->sel_gpio.gpio, 1);
	}

	info->state = state;
	mutex_unlock(&info->lock);
}
EXPORT_SYMBOL(fsusb42_set_state);

static int fsusb42_probe(struct platform_device *pdev)
{
	struct fsusb42_info *info;
	int rc;
	enum of_gpio_flags flags;
	struct device_node *node = pdev->dev.of_node;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	mutex_init(&info->lock);
	info->sel_gpio.gpio = of_get_gpio_flags(node, 0, &flags);
	info->sel_gpio.flags = flags;
	of_property_read_string_index(node, "gpio-names", 0,
			&info->sel_gpio.label);

	rc = gpio_request_one(info->sel_gpio.gpio,
			info->sel_gpio.flags,
			info->sel_gpio.label);
	if (rc) {
		dev_err(info->dev, "failed to request sel GPIO\n");
		goto fail;
	}

	rc = gpio_export(info->sel_gpio.gpio, 1);
	if (rc)
		dev_err(info->dev, "Failed to export sel GPIO %s: %d\n",
			info->sel_gpio.label, info->sel_gpio.gpio);

	rc = gpio_export_link(info->dev, info->sel_gpio.label,
			info->sel_gpio.gpio);
	if (rc)
		dev_err(info->dev, "Failed to link sel GPIO %s: %d\n",
				info->sel_gpio.label, info->sel_gpio.gpio);

	info->vdd = devm_regulator_get(&pdev->dev, "vdd-fsusb42");
	if (IS_ERR(info->vdd)) {
		dev_err(info->dev, "unable to get vdd supply\n");
		goto fail_gpio;
	}

	the_info = info;
	return 0;

fail_gpio:
	gpio_free(info->sel_gpio.gpio);
fail:
	devm_kfree(&pdev->dev, info);
	return rc;
}

static int fsusb42_remove(struct platform_device *pdev)
{
	struct fsusb42_info *info = the_info;

	if (info) {
		gpio_free(info->sel_gpio.gpio);
		fsusb42_vdd_enable(info, false);
		devm_regulator_put(info->vdd);
		devm_kfree(&pdev->dev, info);
		the_info = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fsusb42_of_tbl[] = {
	{	.compatible = "fairchild,fsusb42",
	},
	{},
};
#endif

static struct platform_driver fsusb42_driver = {
	.probe = fsusb42_probe,
	.remove = fsusb42_remove,
	.driver = {
		.name = "fsusb42",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fsusb42_of_tbl),
	},
};

static int __init fsusb42_init(void)
{
	return platform_driver_register(&fsusb42_driver);
}

static void __exit fsusb42_exit(void)
{
	platform_driver_unregister(&fsusb42_driver);
}

module_init(fsusb42_init);
module_exit(fsusb42_exit);

MODULE_ALIAS("platform:fsusb42");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility USB Mux");
MODULE_LICENSE("GPL");
