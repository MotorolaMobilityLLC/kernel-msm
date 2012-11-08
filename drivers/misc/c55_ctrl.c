/*
 * Copyright (C) 2012 Motorola, Inc.
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

#include <linux/c55_ctrl.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

struct c55_ctrl_data {
	struct c55_ctrl_platform_data *pdata;
	int int_gpio;
	struct wake_lock wake_lock;
};

static irqreturn_t c55_ctrl_isr(int irq, void *data)
{
	struct c55_ctrl_data *cdata = data;

	pr_debug("%s: value=%d\n", __func__, gpio_get_value(cdata->int_gpio));

	/* Interrupt is active low */
	if (gpio_get_value(cdata->int_gpio) == 0)
		wake_lock(&cdata->wake_lock);
	else
		wake_unlock(&cdata->wake_lock);

	return IRQ_HANDLED;
}

static void c55_ctrl_int_setup(struct c55_ctrl_data *cdata, int gpio)
{
	int ret;
	int irq = gpio_to_irq(gpio);
	unsigned int flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	if (cdata->int_gpio) {
		/* Interrupt already registered */
		return;
	}

	pr_debug("%s: Requesting IRQ %d\n", __func__, irq);

	/* Interrupt is shared with user space */
	flags |= IRQF_SHARED;

	ret = request_threaded_irq(irq, NULL, c55_ctrl_isr,
				   flags, "c55_ctrl", cdata);
	if (ret) {
		pr_err("%s: IRQ request failed: %d\n", __func__, ret);
		return;
	}

	enable_irq_wake(irq);
	cdata->int_gpio = gpio;
}

static int c55_ctrl_gpio_setup(struct c55_ctrl_data *cdata, struct device *dev)
{
	int i;
	int ret;
	struct gpio *gpios = cdata->pdata->gpios;
	int num_gpios = cdata->pdata->num_gpios;

	ret = gpio_request_array(gpios, num_gpios);
	if (ret) {
		pr_err("%s: GPIO requests failed: %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < num_gpios; ++i) {
		pr_debug("%s: Exporting %s (GPIO %d)\n",
			 __func__, gpios[i].label, gpios[i].gpio);

		gpio_export(gpios[i].gpio, false);
		gpio_export_link(dev, gpios[i].label, gpios[i].gpio);

		if ((gpios[i].flags & GPIOF_IN) == GPIOF_IN)
			c55_ctrl_int_setup(cdata, gpios[i].gpio);
	}

	return 0;
}

static int __devinit c55_ctrl_probe(struct platform_device *pdev)
{
	struct c55_ctrl_data *cdata;
	struct c55_ctrl_platform_data *pdata;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		pr_err("%s: Missing platform data!\n", __func__);
		return -EINVAL;
	}

	cdata = kzalloc(sizeof(*cdata), GFP_KERNEL);
	if (cdata == NULL) {
		pr_err("%s: No memory!\n", __func__);
		return -ENOMEM;
	}

	cdata->pdata = pdata;
	wake_lock_init(&cdata->wake_lock, WAKE_LOCK_SUSPEND, "c55_ctrl");

	ret = c55_ctrl_gpio_setup(cdata, &pdev->dev);
	if (ret)
		goto err;

	if (pdata->adc_clk_en)
		pdata->adc_clk_en(1);

	platform_set_drvdata(pdev, cdata);
	return 0;

err:
	wake_lock_destroy(&cdata->wake_lock);
	kfree(cdata);
	return ret;
}

static int __devexit c55_ctrl_remove(struct platform_device *pdev)
{
	struct c55_ctrl_data *cdata = platform_get_drvdata(pdev);

	if (cdata->pdata->adc_clk_en)
		cdata->pdata->adc_clk_en(0);

	if (cdata->int_gpio)
		free_irq(gpio_to_irq(cdata->int_gpio), cdata);

	gpio_free_array(cdata->pdata->gpios, cdata->pdata->num_gpios);
	wake_lock_destroy(&cdata->wake_lock);
	kfree(cdata);

	return 0;
}

static struct platform_driver c55_ctrl_driver = {
	.driver = {
		.name = "c55_ctrl",
	},
	.probe = c55_ctrl_probe,
	.remove = __devexit_p(c55_ctrl_remove),
};

static int __init c55_ctrl_init(void)
{
	return platform_driver_register(&c55_ctrl_driver);
}

static void __exit c55_ctrl_exit(void)
{
	platform_driver_unregister(&c55_ctrl_driver);
}

module_init(c55_ctrl_init);
module_exit(c55_ctrl_exit);

MODULE_ALIAS("platform:c55_ctrl");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("TI C55 control driver");
