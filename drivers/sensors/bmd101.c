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
};
struct bmd101_data {
	struct sensors_classdev cdev;
	struct workqueue_struct *bmd101_wq;
	int status;
};
struct bmd101_data *sensor_data;

//ASUS_BSP +++ Maggie_Lee "Support BMMI"
static ssize_t bmd101_show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sensor_data->status);
}

static ssize_t bmd101_store_status(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;

	sensor_debug(DEBUG_VERBOSE, "[BMD101] %s: (%s)\n", __func__, buf);
	
	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 1))
		return -EINVAL;
	
	sensor_data->status = val;

	return val;
}
static DEVICE_ATTR(status, S_IRUGO|S_IWUSR|S_IWGRP, bmd101_show_status, bmd101_store_status);
//ASUS_BSP +++ Maggie_Lee "Support BMMI"

static struct attribute *bmd101_attributes[] = {
	&dev_attr_status.attr,
	NULL
};

static struct attribute_group bmd101_attribute_group = {
	.attrs = bmd101_attributes
};

static int bmd101_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	int rc;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: regulator %s, sensor currently %s\n", __func__, enable ? "enable":"disable", bmd101_enable ? "enabled":"disabled");

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

static void sensor_enable_wq(struct work_struct *work)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 1);
	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);
}
static DECLARE_WORK(sensor_enable_work, sensor_enable_wq);

static int bmd101_resume(struct platform_device *pdev)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 1);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;
}

static int bmd101_suspend(struct platform_device *pdev, pm_message_t state)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	bmd101_enable_set(&sensor_data->cdev, 0);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;
}

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
	sensor_data->status = 0;
	sensor_data->cdev = bmd101_cdev;
	sensor_data->cdev.sensors_enable = bmd101_enable_set;
	ret = sensors_classdev_register(&pdev->dev, &sensor_data->cdev);
	if (ret) {
		pr_err("[BMD101] class device create failed: %d\n", ret);
		goto classdev_register_fail;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &bmd101_attribute_group);
	if (ret) {
		pr_err("[BMD101] sysfs create failed: %d\n", ret);
		goto create_sysfs_fail;
	}

	sensor_data->bmd101_wq = create_singlethread_workqueue("bmd101_wq");
	if (!sensor_data->bmd101_wq) {
		pr_err("[BMD101][error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto create_workqueue_fail;
	}

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
	queue_work(sensor_data->bmd101_wq, &sensor_enable_work);
	
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;


	reg_put_LDO28:
	regulator_put(pm8921_l28);
	regulator_get_fail:
	destroy_workqueue(sensor_data->bmd101_wq);
	create_workqueue_fail:
	sysfs_remove_group(&pdev->dev.kobj, &bmd101_attribute_group);
	create_sysfs_fail:
	sensors_classdev_unregister(&sensor_data->cdev);
	classdev_register_fail:
	kfree(sensor_data);

	return ret;

}

static int bmd101_remove(struct platform_device *pdev)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	destroy_workqueue(sensor_data->bmd101_wq);
	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);
	return 0;
}

static struct of_device_id bmd101_match_table[] = {
	{ .compatible = "neurosky, bmd101",},
	{},
};

static struct platform_driver bmd101_platform_driver = {
	.probe = bmd101_probe,
	.remove = bmd101_remove,
	.suspend = bmd101_suspend,
	.resume = bmd101_resume,
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
