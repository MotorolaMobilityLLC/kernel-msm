/* Copyright (c) 2013 LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/setup.h>
#include <linux/of_gpio.h>
#include <linux/spmi.h>


/* enable /disable BOOST_BYP_BYP pin  when resume / suspend for BoostIC */
#define FORCED_BYPASS	0	/* gpio low	*/
#define AUTO_BYPASS	1	/* gpio high	*/

struct boost_gpio_drvdata {
	int bb_gpio;
};

struct boost_gpio_platform_data {
	struct boost_gpio_drvdata *ddata;
	char *bb_boost_gpio_name;	/* device name */
	int bb_gpio;
};

static void boost_gpio_bypass(struct boost_gpio_drvdata *ddata,
				int bypass)
{
	if (unlikely(ddata == NULL)) {
		pr_err("data is null\n");
		return;
	}

	if (gpio_get_value(ddata->bb_gpio) != bypass)
		gpio_set_value(ddata->bb_gpio, bypass);
}

static int boost_gpio_get_devtree_pdata(struct device *dev,
		struct boost_gpio_platform_data *pdata)
{
	struct device_node *node;

	node = dev->of_node;

	pdata->bb_boost_gpio_name = (char *)of_get_property(node,
					"pm8941,bb_boost_gpio_name", NULL);
	pdata->bb_gpio =
		of_get_named_gpio_flags(node, "pm8941,bb_gpio", 0, NULL);

	return 0;
}

static struct of_device_id boost_gpio_of_match[] = {
	{ .compatible = "pm8941,boost_gpio", },
	{ },
};

static int boost_gpio_suspend(struct device *dev)
{
	struct boost_gpio_drvdata *ddata = dev_get_drvdata(dev);

	pr_debug("%s\n", __func__);

	boost_gpio_bypass(ddata, FORCED_BYPASS);

	return 0;
}

static int boost_gpio_resume(struct device *dev)
{
	struct boost_gpio_drvdata *ddata = dev_get_drvdata(dev);

	pr_debug("%s\n", __func__);

	boost_gpio_bypass(ddata, AUTO_BYPASS);

	return 0;
}

static int boost_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

static int boost_gpio_probe(struct platform_device *pdev)
{
	struct boost_gpio_platform_data *pdata;
	struct boost_gpio_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct boost_gpio_platform_data alt_pdata;
	int err;

	pr_info("%s\n", __func__);

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(struct  boost_gpio_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: no memory\n", __func__);
			return -ENOMEM;
		}

		err = boost_gpio_get_devtree_pdata(dev, &alt_pdata);
		if (err) {
			pr_err("%s: failed to get dt\n", __func__);
			return err;
		}
		pdata = &alt_pdata;
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			pr_err("%s: no pdata\n", __func__);
			return -ENODEV;
		}
	}

	ddata = kzalloc(sizeof(struct boost_gpio_drvdata), GFP_KERNEL);
	if (!ddata) {
		pr_err("%s: failed to allocate state\n", __func__);
		err = -ENOMEM;
		goto fail_kzalloc;
	}

	ddata->bb_gpio = pdata->bb_gpio;

	platform_set_drvdata(pdev, ddata);

	err = gpio_request_one(ddata->bb_gpio,
				GPIOF_OUT_INIT_HIGH,
				pdata->bb_boost_gpio_name);
	if (err < 0) {
		pr_err("%s: failed to requset gpio", __func__);
		goto fail_bb_gpio;
	}

	return 0;

fail_bb_gpio:
	platform_set_drvdata(pdev, NULL);
	kfree(ddata);

fail_kzalloc:
	return err;
}

static SIMPLE_DEV_PM_OPS(boost_gpio_pm_ops,
		boost_gpio_suspend, boost_gpio_resume);

static struct platform_driver boost_gpio_device_driver = {
	.probe		= boost_gpio_probe,
	.remove		= boost_gpio_remove,
	.driver		= {
		.name	= "boost_gpio",
		.pm	= &boost_gpio_pm_ops,
		.of_match_table	= boost_gpio_of_match,
	}
};

static int __init boost_gpio_late_init(void)
{
	return platform_driver_register(&boost_gpio_device_driver);
}

late_initcall(boost_gpio_late_init);
