/*
 * Copyright (c) 2014, ASUStek All rights reserved.
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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <linux/tty.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/sensors.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
struct notifier_block bmd101_fb_notif;
#endif

#define bmd101_dev			"/dev/ttyHSL1"
#define bmd101_regulator		"8226_l28"
//Debug Masks +++
#define NO_DEBUG       0
#define DEBUG_POWER     1
#define DEBUG_INFO  2
#define DEBUG_VERBOSE 5
#define DEBUG_RAW      8
#define DEBUG_TRACE   10

static int debug = DEBUG_INFO;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activate debugging output");

#define sensor_debug(level, ...) \
		if (debug >= (level)) \
			pr_info(__VA_ARGS__);
//Debug Masks ---

#define BMD101_CS_GPIO		53
#define BMD101_RST_GPIO		55

static int bmd101_enable;
static struct regulator *pm8921_l28;
static struct sensors_classdev bmd101_cdev = {
	.name = "bmd101",
	.vendor = "NeuroSky",
	.version = 1,
	.enabled = 0,
	.sensors_enable = NULL,
	.status = 0,
};
struct bmd101_data {
	struct sensors_classdev cdev;
};
struct bmd101_data *sensor_data;

static int bmd101_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	int rc;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: sensor currently %s, turning sensor %s\n", __func__, bmd101_enable ? "on":"off", enable ? "on":"off");

	if (enable && !bmd101_enable) {
		rc = gpio_request(BMD101_RST_GPIO, "BMD101_RST");
		if (rc) {
			pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_RST_GPIO);
			goto gpio_rst_fail;
		}
		rc = gpio_request(BMD101_CS_GPIO, "BMD101_CS");
		if (rc) {
			pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_CS_GPIO);
			goto gpio_cs_fail;
		}
		rc = regulator_enable(pm8921_l28);
    		if (rc) {
			pr_err("[bmd101] %s: regulator_enable of 8921_l28 failed(%d)\n", __func__, rc);
			goto reg_put_LDO28;
    		}
		msleep(100);

		gpio_direction_output(BMD101_CS_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_CS_GPIO, 1);
		msleep(100);

		gpio_direction_output(BMD101_RST_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_RST_GPIO, 1);
		msleep(100);

		bmd101_enable = 1;
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: gpio %d and %d pulled hig, bmd101_enable(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, bmd101_enable);
	}
	else if (!enable && bmd101_enable) {	
		rc = regulator_disable(pm8921_l28);
    		if (rc) {
			pr_err("%s: regulator_enable of 8921_l28 failed(%d)\n", __func__, rc);
			goto reg_put_LDO28;
    		}
		gpio_direction_output(BMD101_CS_GPIO, 0);
		gpio_direction_output(BMD101_RST_GPIO, 0);
		gpio_free(BMD101_CS_GPIO);
		gpio_free(BMD101_RST_GPIO);
		bmd101_enable = 0;

		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: gpio %d and %d pulled low, bmd101_enable(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, bmd101_enable);
	}
	else
		pr_err("[bmd101] %s : sensor is already %s\n", __func__, enable ? "enabled":"disabled");

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);

	return 0;

	gpio_rst_fail:
		gpio_free(BMD101_RST_GPIO);
	gpio_cs_fail:
		gpio_free(BMD101_CS_GPIO);
	reg_put_LDO28:
		regulator_put(pm8921_l28);

	return rc;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void bmd101_early_suspend(struct early_suspend *h)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 0);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);
}

static void bmd101_late_resume(struct early_suspend *h)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 1);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);
}

struct early_suspend bmd101_early_suspend_handler = {
    .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN, 
    .suspend = bmd101_early_suspend,
    .resume = bmd101_late_resume,
};
#elif defined(CONFIG_FB)
static void bmd101_early_suspend(void)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 0);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);
}

static void bmd101_late_resume(void)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 1);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);
}

static int bmd101_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	static int blank_old = 0;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			if (blank_old == FB_BLANK_POWERDOWN) {
				blank_old = FB_BLANK_UNBLANK;
				bmd101_late_resume();
			}
		} else if (*blank == FB_BLANK_POWERDOWN) {
			if (blank_old == 0 || blank_old == FB_BLANK_UNBLANK) {
				blank_old = FB_BLANK_POWERDOWN;
				bmd101_early_suspend();
			}
		}
	}

	return 0;
}
#endif

static int bmd101_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);

	sensor_data = kzalloc(sizeof(struct bmd101_data),GFP_KERNEL);
	if(!sensor_data)
	{
		printk(KERN_ERR "%s: failed to allocate bmd101_data\n", __func__);
		return -ENOMEM;
	}
	sensor_data->cdev = bmd101_cdev;
	sensor_data->cdev.sensors_enable = bmd101_enable_set;
	ret = sensors_classdev_register(&pdev->dev, &sensor_data->cdev);
	if (ret) {
		pr_err("[BMD101] class device create failed: %d\n", ret);
		goto classdev_register_fail;
	}
	
	#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&bmd101_early_suspend_handler);
	#elif defined(CONFIG_FB)
	bmd101_fb_notif.notifier_call = bmd101_fb_notifier_callback;
	ret= fb_register_client(&bmd101_fb_notif);
	if (ret)
		dev_err(&pdev->dev, "Unable to register fb_notifier: %d\n", ret);
	#endif

	pm8921_l28 = regulator_get(&pdev->dev, bmd101_regulator);
	if (IS_ERR(pm8921_l28)) {
        	pr_err("%s: regulator get of 8921_l28 failed (%ld)\n", __func__, PTR_ERR(pm8921_l28));
		ret = PTR_ERR(pm8921_l28);
		goto regulator_get_fail;
	}
	
    	ret = regulator_set_voltage(pm8921_l28, 2850000, 2850000);
    	if (ret) {
		pr_err("%s: regulator_set_voltage of 8921_l28 failed(%d)\n", __func__, ret);
		goto reg_put_LDO28;
    	}

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: enable sensor +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 1);
	
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;

	reg_put_LDO28:
	regulator_put(pm8921_l28);
	regulator_get_fail:
	sensors_classdev_unregister(&sensor_data->cdev);
	classdev_register_fail:
	kfree(sensor_data);

	return ret;

}

static int bmd101_remove(struct platform_device *pdev)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	return 0;
}

static struct of_device_id bmd101_match_table[] = {
	{ .compatible = "neurosky, bmd101",},
	{},
};

static struct platform_driver bmd101_platform_driver = {
	.probe = bmd101_probe,
	.remove = bmd101_remove,
	.driver = {
		.name = "bmd101",
		.owner = THIS_MODULE,
		.of_match_table = bmd101_match_table,
	},
};

static int __init bmd101_init(void)
{
	return platform_driver_register(&bmd101_platform_driver);
}

static void __exit bmd101_exit(void)
{
	return platform_driver_unregister(&bmd101_platform_driver);
}

module_init(bmd101_init);
module_exit(bmd101_exit);

MODULE_DESCRIPTION("Driver for ECG sensor");
MODULE_LICENSE("GPL v2");
