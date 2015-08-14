/*
 * rf_detect.c
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/delay.h>

#define DRV_NAME "RF_DEVICE"
#define TAG "RF_GPIO: "

struct rf_platform_data {
	unsigned int gpio_main;
	unsigned int gpio_aux;
	int debounce_interval;
};

struct rf_device {
	struct device *dev;
	struct class *class;
	dev_t devno;
	const struct rf_platform_data *pdata;

	struct delayed_work dwork;

	unsigned int irq_main;
	unsigned int irq_aux;

	unsigned int prev_state;
	unsigned int curr_state;

	bool gpio_main_avail;
	bool gpio_aux_avail;
};

static int rf_get_state(const struct rf_device* rfdev)
{
	unsigned int main_gpio = rfdev->gpio_main_avail ?
		!!gpio_get_value(rfdev->pdata->gpio_main) : 0;

	unsigned int aux_gpio = rfdev->gpio_aux_avail ?
		!!gpio_get_value(rfdev->pdata->gpio_aux) : 0;

	if (rfdev->gpio_main_avail)
		pr_debug(TAG "gpio_main = %d\n", main_gpio);

	if (rfdev->gpio_aux_avail)
		pr_debug(TAG "gpio_aux = %d\n", aux_gpio);

	/* any one in high */
	return (main_gpio | aux_gpio);
}

static void rf_report_event(struct rf_device *rfdev)
{
	char *uevent_envp[2];
	char buf[50];

	rfdev->curr_state = rf_get_state(rfdev);

	/* Report event if previous state is not equal to current state */
	if (rfdev->prev_state != rfdev->curr_state) {
		uevent_envp[1] = NULL;

		sprintf(buf, "GPIO_STATUS=%d", rfdev->curr_state);
		uevent_envp[0] = buf;
		pr_info(TAG "event = %s\n", uevent_envp[0]);
		/* notify */
		kobject_uevent_env(&rfdev->dev->kobj, KOBJ_CHANGE, uevent_envp);
		/* save the current state */
		rfdev->prev_state = rfdev->curr_state;
	}
}

static void rf_detect_work_func(struct work_struct *work)
{
	struct rf_device *rfdev =
		container_of(work, struct rf_device, dwork.work);

	rf_report_event(rfdev);
}

static irqreturn_t rf_irq(int irq, void *dev_id)
{
	struct rf_device *rfdev = dev_id;
	pr_info(TAG "%s: Interrupt occured\n", __func__);

	schedule_delayed_work(&rfdev->dwork,
			msecs_to_jiffies(rfdev->pdata->debounce_interval));

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static struct of_device_id rf_of_match[] = {
	{ .compatible = "rf-device", },
	{ },
};
MODULE_DEVICE_TABLE(of, rf_of_match);

static struct rf_platform_data *rf_parse_dt(struct device *dev)
{
	const struct of_device_id *of_id =
				of_match_device(rf_of_match, dev);
	struct device_node *np = dev->of_node;
	struct rf_platform_data *pdata;
	enum of_gpio_flags flags;

	if (!of_id || !np)
		return NULL;

	pdata = kzalloc(sizeof(struct rf_platform_data), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_main = of_get_gpio_flags(np, 0, &flags);
	pr_debug(TAG "gpio_main = %d\n", pdata->gpio_main);

	pdata->gpio_aux = of_get_gpio_flags(np, 1, &flags);
	pr_debug(TAG "gpio_aux = %d\n", pdata->gpio_aux);

	/* parse failed when all gpios get failed */
	if (!gpio_is_valid(pdata->gpio_main) &&
			!gpio_is_valid(pdata->gpio_aux)) {
		pr_err(TAG "Parse gpios failed\n");
		kfree(pdata);
		return NULL;
	}

	if (of_property_read_u32(np, "debounce-interval",
				&pdata->debounce_interval)) {
		/* default 5 us */
		pdata->debounce_interval = 5;
	}

	return pdata;
}
#else
static inline struct rf_platform_data *rf_parse_dt(struct device *dev)
{
	return NULL;
}
#endif

static int config_gpio(struct rf_device *rfdev, int *irq, int gpio)
{
	int err = -1;

	/* request the GPIOs */
	err = gpio_request_one(gpio, GPIOF_IN, "RF_DETECT");
	if (err) {
		pr_err(TAG "Unable to request gpio %d\n", gpio);
		return err;
	}

	/* request irq */
	*irq = gpio_to_irq(gpio);
	err = request_threaded_irq(*irq, NULL, &rf_irq,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			  DRV_NAME, rfdev);
	if (err) {
		gpio_free(gpio);
		pr_err(TAG "Unable to request IRQ %d, err = %d\n", *irq, err);
		return err;
	}

	pr_info(TAG "Request gpio_%s: %d, irq: %d success\n",
			gpio == rfdev->pdata->gpio_main ? "main" : "aux", gpio, *irq);

	return 0;
}

static ssize_t show_gpio_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rf_device *rfdev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", rf_get_state(rfdev));
}

static struct device_attribute dev_attr_gpio_status =
__ATTR(gpio_status, S_IRUSR | S_IRGRP, show_gpio_status, NULL);

static int rf_probe(struct platform_device *pdev)
{
	const struct rf_platform_data *pdata = NULL;
	struct rf_device *rfdev = NULL;
	int err = -1;

	pr_info(TAG "RF platform device detected\n");
	if (!pdata) {
		pdata = rf_parse_dt(&pdev->dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);

		if (!pdata) {
			pr_err(TAG "Missing platform data\n");
			return -EINVAL;
		}
	}
	pr_info(TAG "Parse device tree success\n");

	rfdev = kzalloc(sizeof(struct rf_device), GFP_KERNEL);
	if(!rfdev) {
		pr_err(TAG "Request memory failed\n");
		kfree(pdata);
		return -EINVAL;
	}

	rfdev->pdata = pdata;

	/* init delayed work */
	INIT_DELAYED_WORK(&rfdev->dwork, rf_detect_work_func);

	/* only config the available gpio */
	rfdev->gpio_main_avail = gpio_is_valid(pdata->gpio_main);
	rfdev->gpio_aux_avail = gpio_is_valid(pdata->gpio_aux);

	if (rfdev->gpio_main_avail) {
		err = config_gpio(rfdev, &rfdev->irq_main, rfdev->pdata->gpio_main);
		if (err) {
			pr_err(TAG "Config main gpio: %d failed\n", rfdev->pdata->gpio_main);
			rfdev->gpio_main_avail = false;
		} else {
			pr_info(TAG "Config main gpio: %d success\n", rfdev->pdata->gpio_main);
		}
	}

	if (rfdev->gpio_aux_avail) {
		err = config_gpio(rfdev, &rfdev->irq_aux, rfdev->pdata->gpio_aux);
		if (err) {
			pr_err(TAG "Config auxiliary gpio: %d failed\n", rfdev->pdata->gpio_aux);
			rfdev->gpio_aux_avail = false;
			if (!rfdev->gpio_main_avail) {
				pr_err(TAG "Config gpio failed, exit\n");
				goto exit_free_mem;
			}
		} else {
			pr_info(TAG "Config auxiliary gpio: %d success\n", rfdev->pdata->gpio_aux);
		}
	}

	platform_set_drvdata(pdev, rfdev);

	/*
	 * Create directory "/sys/class/sensors/status/"
	 * To get event, up layer will listen the directory
	 */
	rfdev->class = class_create(THIS_MODULE, "sensors");
	if (IS_ERR(rfdev->class)) {
		pr_err(TAG "class create failed\n");
		goto exit_free_mem;
	}

	err = alloc_chrdev_region(&rfdev->devno, 0, 1, DRV_NAME);
	if (err) {
		pr_err(TAG "alloc character device region failed\n");
		goto exit_class_destroy;
	}

	rfdev->dev = device_create(rfdev->class, &pdev->dev, rfdev->devno, rfdev, "status");
	if (IS_ERR(rfdev->dev)) {
		pr_err(TAG "device create failed\n");
		goto exit_class_destroy;
	}

	err = device_create_file(rfdev->dev, &dev_attr_gpio_status);
	if (err) {
		pr_err(TAG "create file failed\n");
		goto exit_device_destroy;
	}
	pr_info(TAG "RF platform device probe success\n");

	return 0;

exit_device_destroy:
	device_destroy(rfdev->class, rfdev->devno);
exit_class_destroy:
	class_destroy(rfdev->class);
exit_free_mem:
	kfree(pdata);
	kfree(rfdev);
	pr_info(TAG "RF platform device probe failed\n");

	return err;
}

static int rf_remove(struct platform_device *pdev)
{
	struct rf_device *rfdev = platform_get_drvdata(pdev);
	const struct rf_platform_data *pdata = rfdev->pdata;

	cancel_delayed_work_sync(&rfdev->dwork);

	device_remove_file(rfdev->dev, &dev_attr_gpio_status);
	device_destroy(rfdev->class, rfdev->devno);
	class_destroy(rfdev->class);

	if (rfdev->gpio_main_avail) {
		free_irq(rfdev->irq_main, rfdev);
		gpio_free(pdata->gpio_main);
	}

	if (rfdev->gpio_aux_avail) {
		free_irq(rfdev->irq_aux, rfdev);
		gpio_free(pdata->gpio_aux);
	}

	kfree(pdata);
	kfree(rfdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int rf_dev_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rf_device *rfdev = platform_get_drvdata(pdev);

	rf_report_event(rfdev);

	return 0;
}

static int rf_dev_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rf_device *rfdev = platform_get_drvdata(pdev);

	/*
	 * Because the gpios can't wake device,
	 * so we need to reset the varient "prev_state" to default 0.
	 */
	rfdev->prev_state = 0;
	cancel_delayed_work_sync(&rfdev->dwork);

	return 0;
}

/**
  The file Operation table for power
*/
static const struct dev_pm_ops rf_dev_pm_ops = {
	.suspend = rf_dev_suspend,
	.resume = rf_dev_resume,
};

static struct platform_driver rf_driver = {
	.probe		= rf_probe,
	.remove		= rf_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.pm		= &rf_dev_pm_ops,
		.of_match_table = of_match_ptr(rf_of_match),
	}
};
module_platform_driver(rf_driver);

MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DESCRIPTION("Adjusted RF by monitoring the change of gpio");
