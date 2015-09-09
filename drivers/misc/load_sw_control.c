/* load_sw_control.c
 *
 * Copyright (C) 2015 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/*
 * This is for reducing the power off leakage current.
 * Load switch changed from ON state to OFF state when off pin is triggered
 *     falling edge
 */

#define pr_fmt(fmt) "%s : " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

struct load_sw_device {
	int gpio;
	int need_to_control;
};

static ssize_t lsw_show_control(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct load_sw_device *lsw_dev = dev_get_drvdata(dev);

	if (!lsw_dev || !gpio_is_valid(lsw_dev->gpio))
		return -ENODEV;

	return sprintf(buf, "%d\n", lsw_dev->need_to_control);
}

static ssize_t lsw_store_control(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct load_sw_device *lsw_dev = dev_get_drvdata(dev);
	long r;
	int ret;

	if (!lsw_dev || !gpio_is_valid(lsw_dev->gpio))
		return -ENODEV;

	ret = kstrtol(buf, 10, &r);
	if (ret < 0) {
		pr_err("failed to store value\n");
		return ret;
	}

	lsw_dev->need_to_control = r;

	return count;
}

static DEVICE_ATTR(control, S_IRUGO|S_IWUSR,
		lsw_show_control, lsw_store_control);

static int load_sw_probe(struct platform_device *pdev)
{
	struct load_sw_device *lsw_dev;
	struct device_node *np = NULL;
	int ret = 0;

	lsw_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct load_sw_device), GFP_KERNEL);
	if (!lsw_dev) {
		pr_err("no memory\n");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		np = pdev->dev.of_node;
	} else {
		pr_err("device node is null\n");
		return -ENODEV;
	}

	/* Parse gpio dts */
	ret = of_get_named_gpio_flags(np, "load-sw-gpio", 0, NULL);
	if (ret < 0) {
		pr_err("failed to read GPIO from device tree");
		return -EINVAL;
	}
	lsw_dev->gpio = ret;

	/* Request gpio */
	ret = gpio_request_one(lsw_dev->gpio,
			GPIOF_OUT_INIT_LOW, "load-sw-gpio");
	if (ret) {
		pr_err("failed to request GPIO %d\n", lsw_dev->gpio);
		return ret;
	}

	platform_set_drvdata(pdev, lsw_dev);

	ret = device_create_file(&pdev->dev, &dev_attr_control);
	if (ret < 0) {
		pr_err("failed to create sysfs\n");
		goto err_device_create_file;
	}

	pr_info("probed\n");
	return 0;

err_device_create_file:
	if (gpio_is_valid(lsw_dev->gpio))
		gpio_free(lsw_dev->gpio);

	return ret;
}

static int load_sw_remove(struct platform_device *pdev)
{
	struct load_sw_device *lsw_dev = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_control);
	gpio_free(lsw_dev->gpio);

	return 0;
}

static void load_sw_shutdown(struct platform_device *pdev)
{
	struct load_sw_device *lsw_dev = platform_get_drvdata(pdev);

	if (SYSTEM_RESTART != system_state && lsw_dev->need_to_control) {
		gpio_set_value(lsw_dev->gpio, 1);
		printk("Load switch will be opened after shutdown!!\n");
	}
}

#ifdef CONFIG_OF
static struct of_device_id load_sw_match_table[] = {
	{ . compatible = "lge,load-sw-control", },
	{},
};
#endif

static struct platform_driver load_sw_driver = {
	.probe = load_sw_probe,
	.remove = load_sw_remove,
	.shutdown = load_sw_shutdown,
	.driver = {
		.name = "load-sw-control",
#ifdef CONFIG_OF
		.of_match_table = load_sw_match_table,
#endif
	},
};

static int __init load_sw_init(void)
{
	return platform_driver_register(&load_sw_driver);
}
module_init(load_sw_init);

static void __exit load_sw_exit(void)
{
	return platform_driver_unregister(&load_sw_driver);

}
module_exit(load_sw_exit);

MODULE_DESCRIPTION("Load Switch Control driver");
MODULE_LICENSE("GPL");
