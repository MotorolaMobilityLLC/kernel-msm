/*
 * Copyright (C) 2013 Motorola Mobility LLC
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>

struct c55_ctrl_data {
	int int_gpio;
	struct wake_lock wake_lock;
	struct clk *mclk;
};

#define NUM_GPIOS 4

const char *gpio_labels[NUM_GPIOS] = {
	"gpio_core",
	"gpio_reset",
	"gpio_ap_int",
	"gpio_c55_int"
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
	flags |= IRQF_SHARED | IRQF_ONESHOT;

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

	if (of_gpio_count(dev->of_node) != NUM_GPIOS) {
		dev_err(dev, "%s: gpio count is not %d.\n", __func__, NUM_GPIOS);
		return -EINVAL;
	}

	for (i = 0; i < NUM_GPIOS; i++) {
		enum of_gpio_flags flags;
		int gpio;

		gpio = of_get_gpio_flags(dev->of_node, i, &flags);
		if (gpio < 0) {
			pr_err("%s: of_get_gpio failed: %d\n", __func__, gpio);
			return gpio;
		}

		gpio_request(gpio, gpio_labels[i]);

		gpio_export(gpio, false);
		gpio_export_link(dev, gpio_labels[i], gpio);

		if ((flags & GPIOF_IN) == GPIOF_IN) {
			gpio_direction_input(gpio);
			c55_ctrl_int_setup(cdata, gpio);
		} else {
			if ((flags & GPIOF_OUT_INIT_HIGH) == GPIOF_OUT_INIT_HIGH)
				gpio_direction_output(gpio, 1);
			else
				gpio_direction_output(gpio, 0);
		}
	}

	return 0;
}

static int c55_ctrl_probe(struct platform_device *pdev)
{
	struct c55_ctrl_data *cdata;
	int ret;

	if (!pdev->dev.of_node) {
		/* Platform data not currently supported */
		dev_err(&pdev->dev, "%s: of devtree data not found\n", __func__);
		return -EINVAL;
	}

	cdata = devm_kzalloc(&pdev->dev, sizeof(*cdata), GFP_KERNEL);
	if (cdata == NULL) {
		dev_err(&pdev->dev, "%s: devm_kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	cdata->mclk = devm_clk_get(&pdev->dev, "adc_mclk");

	if (IS_ERR(cdata->mclk)) {
		dev_err(&pdev->dev, "%s: devm_clk_get failed.\n", __func__);
		return PTR_ERR(cdata->mclk);
	}

	clk_prepare_enable(cdata->mclk);

	ret = c55_ctrl_gpio_setup(cdata, &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev, "%s: c55_ctrl_gpio_setup failed.\n", __func__);
		return ret;
	}

	wake_lock_init(&cdata->wake_lock, WAKE_LOCK_SUSPEND, "c55_ctrl");

	platform_set_drvdata(pdev, cdata);
	return 0;
}

static int c55_ctrl_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id c55_ctrl_match[] = {
	{.compatible = "ti,c55-ctrl",},
	{},
};
MODULE_DEVICE_TABLE(of, c55_ctrl_match);

static const struct platform_device_id c55_ctrl_id_table[] = {
	{"c55_ctrl", 0},
	{},
};
MODULE_DEVICE_TABLE(of, c55_ctrl_id_table);

static struct platform_driver c55_ctrl_driver = {
	.driver = {
		.name = "c55_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = c55_ctrl_match,
	},
	.probe = c55_ctrl_probe,
	.remove = c55_ctrl_remove,
	.id_table = c55_ctrl_id_table,
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
