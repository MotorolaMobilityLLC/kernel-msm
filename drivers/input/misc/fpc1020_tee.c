/*
 * FPC1020 Fingerprint sensor device driver
 *
 * Copyright (c) 2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>

struct FPS_data {
	unsigned int enabled;
	unsigned int state;
	struct blocking_notifier_head nhead;
} *fpsData;

struct FPS_data *FPS_init(struct device *dev)
{
	struct FPS_data *mdata = devm_kzalloc(dev,
			sizeof(struct FPS_data), GFP_KERNEL);
	if (mdata) {
		BLOCKING_INIT_NOTIFIER_HEAD(&mdata->nhead);
		pr_debug("%s: FPS notifier data structure init-ed\n", __func__);
	}
	return mdata;
}

int FPS_register_notifier(struct notifier_block *nb,
	unsigned long stype, bool report)
{
	int error;
	struct FPS_data *mdata = fpsData;

	if (!mdata)
		return -ENODEV;

	mdata->enabled = (unsigned int)stype;
	pr_info("%s: FPS sensor %lu notifier enabled\n", __func__, stype);

	error = blocking_notifier_chain_register(&mdata->nhead, nb);
	if (!error && report) {
		int state = mdata->state;
		/* send current FPS state on register request */
		blocking_notifier_call_chain(&mdata->nhead,
				stype, (void *)&state);
		pr_debug("%s: FPS reported state %d\n", __func__, state);
	}
	return error;
}
EXPORT_SYMBOL_GPL(FPS_register_notifier);

int FPS_unregister_notifier(struct notifier_block *nb,
		unsigned long stype)
{
	int error;
	struct FPS_data *mdata = fpsData;

	if (!mdata)
		return -ENODEV;

	error = blocking_notifier_chain_unregister(&mdata->nhead, nb);
	pr_debug("%s: FPS sensor %lu notifier unregister\n", __func__, stype);

	if (!mdata->nhead.head) {
		mdata->enabled = 0;
		pr_info("%s: FPS sensor %lu no clients\n", __func__, stype);
	}

	return error;
}
EXPORT_SYMBOL_GPL(FPS_unregister_notifier);

void FPS_notify(unsigned long stype, int state)
{
	struct FPS_data *mdata = fpsData;

	pr_debug("%s: Enter", __func__);

	if (!mdata) {
		pr_err("%s: FPS notifier not initialized yet\n", __func__);
		return;
	} else if (!mdata->enabled) {
		pr_debug("%s: !mdata->enabled", __func__);
		return;
	}

	pr_debug("%s: FPS current state %d -> (0x%x)\n", __func__,
	       mdata->state, state);

	if (mdata->state != state) {
		mdata->state = state;
		blocking_notifier_call_chain(&mdata->nhead,
					     stype, (void *)&state);
		pr_debug("%s: FPS notification sent\n", __func__);
	} else
		pr_warn("%s: mdata->state==state", __func__);
}

struct fpc1020_data {
	struct device *dev;
	struct platform_device *pdev;
	struct wake_lock wlock;
	struct notifier_block nb;
	int irq_gpio;
	int irq_num;
	unsigned int irq_cnt;
};

static ssize_t dev_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);

	int state = (*buf == '1') ? 1 : 0;

	FPS_notify(0xbeef, state);
	dev_dbg(fpc1020->dev, "%s state = %d\n", __func__, state);
	return 1;
}
static DEVICE_ATTR(dev_enable, S_IWUSR | S_IWGRP, NULL, dev_enable_set);

static ssize_t irq_get(struct device *device,
		       struct device_attribute *attribute,
		       char *buffer)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc1020->irq_gpio);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}
static DEVICE_ATTR(irq, S_IRUSR | S_IRGRP, irq_get, NULL);

static ssize_t irq_cnt_get(struct device *device,
		       struct device_attribute *attribute,
		       char *buffer)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);

	return scnprintf(buffer, PAGE_SIZE, "%u\n", fpc1020->irq_cnt);
}
static DEVICE_ATTR(irq_cnt, S_IRUSR, irq_cnt_get, NULL);

static struct attribute *attributes[] = {
	&dev_attr_dev_enable.attr,
	&dev_attr_irq.attr,
	&dev_attr_irq_cnt.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static irqreturn_t fpc1020_irq_handler(int irq, void *handle)
{
	struct fpc1020_data *fpc1020 = handle;

	wake_lock_timeout(&fpc1020->wlock, msecs_to_jiffies(1000));
	dev_dbg(fpc1020->dev, "%s\n", __func__);
	fpc1020->irq_cnt++;
	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);
	return IRQ_HANDLED;
}

static int fpc1020_request_named_gpio(struct fpc1020_data *fpc1020,
		const char *label, int *gpio)
{
	struct device *dev = fpc1020->dev;
	struct device_node *np = dev->of_node;
	int rc;

	*gpio = of_get_named_gpio(np, label, 0);
	rc = devm_gpio_request(dev, *gpio, label);
	if (rc) {
		dev_err(dev, "failed to request gpio %d\n", *gpio);
		return rc;
	}
	dev_dbg(dev, "%s %d\n", label, *gpio);
	return 0;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;
	int irqf;
	struct device_node *np = dev->of_node;
	struct fpc1020_data *fpc1020 = devm_kzalloc(dev, sizeof(*fpc1020),
			GFP_KERNEL);
	if (!fpc1020) {
		rc = -ENOMEM;
		goto exit;
	}

	fpsData = FPS_init(dev);

	fpc1020->dev = dev;
	dev_set_drvdata(dev, fpc1020);
	fpc1020->pdev = pdev;

	if (!np) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	rc = fpc1020_request_named_gpio(fpc1020, "irq",
			&fpc1020->irq_gpio);
	gpio_direction_input(fpc1020->irq_gpio);
	if (rc)
		goto exit;

	wake_lock_init(&fpc1020->wlock, WAKE_LOCK_SUSPEND, "fpc1020");

	fpc1020->irq_cnt = 0;
	irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	rc = devm_request_threaded_irq(dev, gpio_to_irq(fpc1020->irq_gpio),
			NULL, fpc1020_irq_handler, irqf,
			dev_name(dev), fpc1020);
	if (rc) {
		dev_err(dev, "could not request irq %d\n",
				gpio_to_irq(fpc1020->irq_gpio));
		goto exit;
	}
	dev_dbg(dev, "requested irq %d\n", gpio_to_irq(fpc1020->irq_gpio));

	/* Request that the interrupt should be wakeable */
	enable_irq_wake(gpio_to_irq(fpc1020->irq_gpio));


	rc = sysfs_create_group(&dev->kobj, &attribute_group);
	if (rc) {
		dev_err(dev, "could not create sysfs\n");
		goto exit;
	}

	dev_info(dev, "%s: ok\n", __func__);
exit:
	return rc;
}

static int fpc1020_remove(struct platform_device *pdev)
{
	struct  fpc1020_data *fpc1020 = dev_get_drvdata(&pdev->dev);

	sysfs_remove_group(&pdev->dev.kobj, &attribute_group);

	wake_lock_destroy(&fpc1020->wlock);
	dev_info(&pdev->dev, "%s\n", __func__);
	return 0;
}

static int fpc1020_suspend(struct device *dev)
{
	return 0;
}

static int fpc1020_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops fpc1020_pm_ops = {
	.suspend = fpc1020_suspend,
	.resume = fpc1020_resume,
};

static const struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc1020_of_match);

static struct platform_driver fpc1020_driver = {
	.driver = {
		.name	= "fpc1020",
		.owner	= THIS_MODULE,
		.of_match_table = fpc1020_of_match,
#if defined(CONFIG_PM)
		.pm = &fpc1020_pm_ops,
#endif
	},
	.probe		= fpc1020_probe,
	.remove		= fpc1020_remove,
};

static int __init fpc1020_init(void)
{
	int rc = platform_driver_register(&fpc1020_driver);

	if (!rc)
		pr_debug("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);
	return rc;
}

static void __exit fpc1020_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&fpc1020_driver);
}

module_init(fpc1020_init);
module_exit(fpc1020_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aleksej Makarov");
MODULE_AUTHOR("Henrik Tillman <henrik.tillman@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 Fingerprint sensor device driver.");
