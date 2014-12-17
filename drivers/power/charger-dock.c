/*
 * Copyright(c) 2014,2015, LGE Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/switch.h>

#define DOCK_TIMEOUT	2000

struct dock_device {
	struct power_supply psy;
	struct power_supply *usb_psy;
	struct switch_dev *sdev;
	struct wake_lock wakelock;
	int docked;
};

static void dock_external_power_changed(struct power_supply *psy)
{
	struct dock_device *dock_dev = container_of(psy, struct dock_device,
			psy);
	union power_supply_propval res = {0, };
	struct power_supply *usb_psy = NULL;

	if (!dock_dev->usb_psy)
		dock_dev->usb_psy = power_supply_get_by_name("usb");

	usb_psy = dock_dev->usb_psy;
	if (!usb_psy) {
		pr_debug("%s: no usb power supply\n", __func__);
		return;
	}
	usb_psy->get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &res);

	if (!(res.intval^dock_dev->docked))
		return;

	dock_dev->docked = res.intval;
	switch_set_state(dock_dev->sdev, !!dock_dev->docked);
	wake_lock_timeout(&dock_dev->wakelock, msecs_to_jiffies(DOCK_TIMEOUT));

	pr_info("%s: docked = %d\n", __func__, res.intval);
}

static int dock_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		 union power_supply_propval *val)
{
	return -EINVAL;
}

static int dock_probe(struct platform_device *pdev)
{
	struct dock_device *dock_dev;
	int ret = 0;

	dock_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct dock_device), GFP_KERNEL);
	if (!dock_dev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dock_dev);

	dock_dev->psy.name = "dock";
	dock_dev->psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
	dock_dev->psy.get_property = dock_get_property;
	dock_dev->psy.external_power_changed = dock_external_power_changed;

	ret = power_supply_register(&pdev->dev, &dock_dev->psy);
	if (ret) {
		pr_err("%s: failed power supply register\n", __func__);
		return ret;
	}

	dock_dev->sdev = devm_kzalloc(&pdev->dev, sizeof(*dock_dev->sdev),
			GFP_KERNEL);
	if (!dock_dev) {
		pr_err("%s: failed to alloc switch device\n", __func__);
		ret = -ENOMEM;
		goto err_switch;
	}

	dock_dev->sdev->name = "dock";
	ret = switch_dev_register(dock_dev->sdev);
	if (ret < 0) {
		pr_err("%s: failed to register switch device\n", __func__);
		goto err_switch;
	}

	wake_lock_init(&dock_dev->wakelock, WAKE_LOCK_SUSPEND,
			"docked-state-uevent");

	dock_external_power_changed(&dock_dev->psy);
	return 0;

err_switch:
	power_supply_unregister(&dock_dev->psy);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int dock_remove(struct platform_device *pdev)
{
	struct dock_device *dock_dev = platform_get_drvdata(pdev);

	wake_lock_destroy(&dock_dev->wakelock);
	switch_dev_unregister(dock_dev->sdev);
	power_supply_unregister(&dock_dev->psy);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id dock_match[] = {
	{.compatible = "charger-dock-detector", },
	{}
};

static struct platform_driver dock_driver = {
	.probe = dock_probe,
	.remove = dock_remove,
	.driver = {
		.name = "dock_detector",
		.owner = THIS_MODULE,
		.of_match_table = dock_match,
	},
};

static int __init dock_init(void)
{
	return platform_driver_register(&dock_driver);
}

static void __exit dock_exit(void)
{
	platform_driver_unregister(&dock_driver);
}

late_initcall(dock_init);
module_exit(dock_exit);

MODULE_DESCRIPTION("Charger dock detector Driver");
MODULE_AUTHOR("Dojip Kim <dojip.kim@lge.com>");
MODULE_LICENSE("GPL");
