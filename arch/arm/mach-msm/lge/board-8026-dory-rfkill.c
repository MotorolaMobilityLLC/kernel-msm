/*
 * This software is used for bluetooth HW enable/disable control.
 *
 * Copyright (C) 2013 LGE Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define BTRFKILL_DBG

#ifdef BTRFKILL_DBG
#define BTRFKILLDBG(fmt, arg...) pr_info("rfkill: %s: " fmt "\n",\
				         __func__, ##arg)
#else
#define BTRFKILLDBG(fmt, arg...) do{ }while(0)
#endif

struct bluetooth_rfkill_platform_data {
	int gpio_reset;
	int gpio_on;
};

struct bluetooth_rfkill_device {
	struct rfkill *rfk;
	int gpio_bt_reset;
	int gpio_bt_on;
};

static int bluetooth_set_power(void *data, bool blocked)
{
	struct bluetooth_rfkill_device *bdev = data;

	BTRFKILLDBG("set blocked %d", blocked);

	if (!blocked) {
		gpio_set_value(bdev->gpio_bt_reset, 0);
		gpio_set_value(bdev->gpio_bt_on, 0);
		msleep(30);
		gpio_set_value(bdev->gpio_bt_on, 1);
		usleep(10*1000);
		gpio_set_value(bdev->gpio_bt_reset, 1);
		usleep(10*1000);
		BTRFKILLDBG("Bluetooth ON!!");
	} else {
		gpio_set_value(bdev->gpio_bt_reset, 0);
		gpio_set_value(bdev->gpio_bt_on, 0);
		BTRFKILLDBG("Bluetooth OFF!!");
	}
	return 0;
}

static int bluetooth_rfkill_parse_dt(struct device *dev,
		struct bluetooth_rfkill_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->gpio_reset = of_get_named_gpio_flags(np,
			"gpio-bt-reset", 0, NULL);
	if (pdata->gpio_reset < 0) {
		pr_err("%s: failed to get gpio-bt-reset\n", __func__);
		return pdata->gpio_reset;
	}

	pdata->gpio_on = of_get_named_gpio_flags(np,
			"gpio-bt-on", 0, NULL);
	if (pdata->gpio_on < 0) {
		pr_err("%s: failed to get gpio-bt-on\n", __func__);
		return pdata->gpio_on;
	}

	return 0;
}

static struct rfkill_ops bluetooth_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int bluetooth_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */
	struct bluetooth_rfkill_platform_data *pdata;
	struct bluetooth_rfkill_device *bdev;

	BTRFKILLDBG("entry");

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct bluetooth_rfkill_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		rc = bluetooth_rfkill_parse_dt(&pdev->dev, pdata);
		if (rc < 0) {
			pr_err("%s: failed to parse device tree\n", __func__);
			return rc;
		}
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	bdev = kzalloc(sizeof(struct bluetooth_rfkill_device), GFP_KERNEL);
	if (!bdev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	bdev->gpio_bt_on = pdata->gpio_on;
	bdev->gpio_bt_reset = pdata->gpio_reset;
	platform_set_drvdata(pdev, bdev);

	rc = gpio_request_one(bdev->gpio_bt_on, GPIOF_OUT_INIT_LOW, "bt_on");
	if (rc) {
		pr_err("%s: failed to request gpio(%d)\n", __func__,
				bdev->gpio_bt_on);
		goto err_gpio_on;
	}

	rc = gpio_request_one(bdev->gpio_bt_reset, GPIOF_OUT_INIT_LOW,
			"bt_reset");
	if (rc) {
		pr_err("%s: failed to request gpio(%d)\n", __func__,
				bdev->gpio_bt_reset);
		goto err_gpio_reset;
	}

	bluetooth_set_power(bdev, default_state);

	bdev->rfk = rfkill_alloc("brcm_Bluetooth_rfkill", &pdev->dev,
			RFKILL_TYPE_BLUETOOTH,
			&bluetooth_rfkill_ops, bdev);
	if (!bdev->rfk) {
		pr_err("%s: failed to alloc rfkill\n", __func__);
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(bdev->rfk, default_state, false);

	/* userspace cannot take exclusive control */
	rc = rfkill_register(bdev->rfk);
	if (rc) {
		pr_err("%s: failed to register rfkill\n", __func__);
		goto err_rfkill_reg;
	}

	return 0;


err_rfkill_reg:
	rfkill_destroy(bdev->rfk);
err_rfkill_alloc:
	gpio_free(bdev->gpio_bt_reset);
err_gpio_reset:
	gpio_free(bdev->gpio_bt_on);
err_gpio_on:
	kfree(bdev);
	return rc;
}

static int bluetooth_rfkill_remove(struct platform_device *pdev)
{
	struct bluetooth_rfkill_device *bdev = platform_get_drvdata(pdev);

	rfkill_unregister(bdev->rfk);
	rfkill_destroy(bdev->rfk);
	gpio_free(bdev->gpio_bt_reset);
	gpio_free(bdev->gpio_bt_on);
	platform_set_drvdata(pdev, NULL);
	kfree(bdev);
	return 0;
}

static struct of_device_id bluetooth_rfkil_match_table[] = {
	{ .compatible = "lge,bluetooth_rfkill", },
	{ },
};

static struct platform_driver bluetooth_rfkill_driver = {
	.probe = bluetooth_rfkill_probe,
	.remove = bluetooth_rfkill_remove,
	.driver = {
		.name = "bluetooth_rfkill",
		.owner = THIS_MODULE,
		.of_match_table = bluetooth_rfkil_match_table,
	},
};

static int __init bluetooth_rfkill_init(void)
{
	return platform_driver_register(&bluetooth_rfkill_driver);;
}

static void __exit bluetooth_rfkill_exit(void)
{
	platform_driver_unregister(&bluetooth_rfkill_driver);
}

module_init(bluetooth_rfkill_init);
module_exit(bluetooth_rfkill_exit);

MODULE_DESCRIPTION("bluetooth rfkill");
MODULE_AUTHOR("Devin Kim <dojip.kim@lge.com>");
MODULE_LICENSE("GPL");
