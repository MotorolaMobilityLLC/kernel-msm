// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/init.h>

/**
 * struct genoa_st - platform device data for genoa
 *              extcon driver.
 * @dev: pointer to the genoa device.
 * @extcon: pointer to the extcon device.
 * @vbus_det_gpio: variable to monitor VBUS_DET GPIO which is used for
 *		vbus notifications. If
 *		0: device mode cable is disconnected so msm can run
 *		in host mode to activate Genoa use case.
 *		1: device mode cable to windows host PC is
 *		connected as VBUS_DET GPIO is connected to VBUS.
 * @usb_id_gpio:   variable to drive USB_ID GPIO which is the id
 *		pin for USB port,If
 *		1: device mode cable is disconnected so msm will
 *		be in host mode.
 *		0:  device mode cable is connected so msm will
 *		be in peripheral mode.
 * @vbus_det_gpio_irq:  variable used as an irq line to trigger interrupt
 *                 based on VBUS_DET GPIO rising/falling events.
 * @genoa_usb_oe_n_gpio: variable to drive USB_OE_N GPIO if device mode cable is
 *                  connected to genoa/Windows Host PC or disconnected.
 *                  drive it
 *                  HIGH: switch will be disabled.
 *                  LOW:  switch will be enabled.
 */
struct genoa_st {
	struct extcon_dev	*extcon;
	unsigned int		vbus_det_gpio;
	unsigned int		usb_id_gpio;
	unsigned int		vbus_det_gpio_irq;
	unsigned int		genoa_usb_oe_n_gpio;
};

/* USB external connector */
static const unsigned int genoa_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
};

static struct genoa_st *gpst;

static void genoa_peripheral_mode_init(void)
{
	gpio_set_value_cansleep(gpst->genoa_usb_oe_n_gpio, 0);
	gpio_set_value_cansleep(gpst->usb_id_gpio, 0);
	extcon_set_state_sync(gpst->extcon, EXTCON_USB_HOST, false);
	extcon_set_state_sync(gpst->extcon, EXTCON_USB, true);
}

static void genoa_host_mode_init(void)
{
	gpio_set_value_cansleep(gpst->genoa_usb_oe_n_gpio, 0);
	gpio_set_value_cansleep(gpst->usb_id_gpio, 1);
	extcon_set_state_sync(gpst->extcon, EXTCON_USB, false);
	extcon_set_state_sync(gpst->extcon, EXTCON_USB_HOST, true);
}

/*
 * genoa_vbus_det_gpio_isr will be called when user manually
 * connects/disconnects device mode cable and set peripheral
 * or host mode accordingly based on VBUS_DET GPIO.
 */
static irqreturn_t genoa_vbus_det_gpio_isr(int irq, void *dev_id)
{
	if (gpio_get_value(gpst->vbus_det_gpio))
		genoa_peripheral_mode_init();
	else
		genoa_host_mode_init();

	return IRQ_HANDLED;
}

static int genoa_populate_dt_info(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

	ret = of_get_named_gpio(np, "genoa_vbus_det", 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't find genoa vbus det gpio %d\n",
				ret);
		return -ENODEV;
	}
	gpst->vbus_det_gpio = ret;

	ret = of_get_named_gpio(np, "genoa_usb_id", 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't find genoa usb id gpio %d\n",
				ret);
		return -ENODEV;
	}
	gpst->usb_id_gpio = ret;

	ret = of_get_named_gpio(np, "genoa_usb_oe_n", 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't find genoa usb_oe_n gpio %d\n",
				ret);
		return -ENODEV;
	}
	gpst->genoa_usb_oe_n_gpio = ret;

	dev_info(&pdev->dev, "genoa vbus_det_gpio:%d usb_id_gpio:%d genoa_usb_oe_n_gpio:%d gpio\n",
			gpst->vbus_det_gpio, gpst->usb_id_gpio,
			gpst->genoa_usb_oe_n_gpio);
	return 0;
}

static ssize_t mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	if (gpio_get_value(gpst->vbus_det_gpio))
		return scnprintf(buf, PAGE_SIZE, "peripheral mode\n");

	return scnprintf(buf, PAGE_SIZE, "host mode\n");
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (sysfs_streq(buf, "peripheral"))
		genoa_peripheral_mode_init();
	else if (sysfs_streq(buf, "host"))
		genoa_host_mode_init();
	else
		return -EINVAL;

	return count;
}

static DEVICE_ATTR_RW(mode);

static int genoa_probe(struct platform_device *pdev)
{
	int ret;

	if (pdev->dev.of_node) {
		gpst = devm_kzalloc(&pdev->dev, sizeof(*gpst), GFP_KERNEL);
		if (!gpst)
			return -ENOMEM;

		ret = genoa_populate_dt_info(pdev);
		if (ret < 0) {
			dev_err(&pdev->dev,
					"couldn't populate dt info err: %d\n",
					ret);
			goto err;
		}
	} else {
		dev_err(&pdev->dev,
				"couldn't find of node for genoa device\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, gpst);

	/* configure vbus det gpio as input*/
	ret = devm_gpio_request_one(&pdev->dev, gpst->vbus_det_gpio, GPIOF_IN,
			"genoa_vbus_det_gpio");
	if (ret < 0) {
		dev_err(&pdev->dev,
				"failed to configure input direction for gpio %d err:%d\n",
				gpst->vbus_det_gpio, ret);
		goto err;
	}

	/* configure usb id gpio as output*/
	ret = devm_gpio_request_one(&pdev->dev, gpst->usb_id_gpio,
			GPIOF_DIR_OUT, "genoa_usb_id_gpio");
	if (ret < 0) {
		dev_err(&pdev->dev,
				"failed to configure input direction for gpio %d err:%d\n",
				gpst->vbus_det_gpio, ret);
		goto err;
	}

	/* configure usb_oe_n gpio as output*/
	ret = devm_gpio_request_one(&pdev->dev, gpst->genoa_usb_oe_n_gpio,
			GPIOF_OUT_INIT_HIGH, "genoa_usb_oe_n_gpio");
	if (ret < 0) {
		dev_err(&pdev->dev,
				"failed to configure input direction for gpio %d err:%d\n",
				gpst->vbus_det_gpio, ret);
		goto err;
	}

	ret = gpio_to_irq(gpst->vbus_det_gpio);
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't find genoa vbus det irq err:%d\n",
				ret);
		goto err;
	}
	gpst->vbus_det_gpio_irq = ret;

	gpst->extcon = devm_extcon_dev_allocate(&pdev->dev, genoa_extcon_cable);
	if (IS_ERR(gpst->extcon)) {
		dev_err(&pdev->dev, "%s: failed to allocate extcon device\n",
					__func__);
		return PTR_ERR(gpst->extcon);
	}

	ret = devm_extcon_dev_register(&pdev->dev, gpst->extcon);
	if (ret) {
		dev_err(&pdev->dev, "%s: failed to register extcon device %d\n",
					__func__, ret);
		goto err;
	}

	ret = devm_request_irq(&pdev->dev, gpst->vbus_det_gpio_irq,
			genoa_vbus_det_gpio_isr,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"vbus-det-irq-trigger", NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "couldn't acquire genoa vbus det irq %d\n",
				ret);
		goto err;
	}

	/*setting the state while bootup*/
	if (gpio_get_value(gpst->vbus_det_gpio))
		genoa_peripheral_mode_init();
	else
		genoa_host_mode_init();

	device_create_file(&pdev->dev, &dev_attr_mode);

	return 0;

err:
	return ret;
}

static int genoa_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_mode);
	return 0;
}


static const struct of_device_id genoa_dt_match[] = {
	{.compatible = "qcom,genoa-extcon"},
	{},
};
MODULE_DEVICE_TABLE(of, genoa_dt_match);

static struct platform_driver genoa_driver = {
	.probe		= genoa_probe,
	.remove		= genoa_remove,
	.driver		= {
		.name		= "genoa-msm-vbus-det",
		.of_match_table = genoa_dt_match,
	},
};

module_platform_driver(genoa_driver);

MODULE_DESCRIPTION("QTI genoa vbus detect driver");
MODULE_LICENSE("GPL v2");
