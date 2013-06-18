/*
 * earjack debugger trigger
 *
 * Copyright (C) 2012, 2013 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>

#include <mach/msm_serial_hs_lite.h>

struct earjack_debugger_device {
	int gpio;
	int irq;
	int saved_detect;
	void (*set_uart_console)(bool enable);
};

struct earjack_debugger_platform_data {
	int gpio_trigger;
};

static int earjack_debugger_detected(void *dev)
{
	struct earjack_debugger_device *adev = dev;

	return !!gpio_get_value(adev->gpio);
}

static irqreturn_t earjack_debugger_irq_handler(int irq, void *_dev)
{
	struct earjack_debugger_device *adev = _dev;
	int detect;

	/*
	 * add debounce time to avoid earjack popup noise
	 */
	msleep(400);

	detect = earjack_debugger_detected(adev);

	if (detect) {
		pr_debug("%s: in\n", __func__);
		adev->set_uart_console(true);
	} else {
		pr_debug("%s: out\n", __func__);
		adev->set_uart_console(false);
	}

	return IRQ_HANDLED;
}

static void earjack_debugger_parse_dt(struct device *dev,
		struct earjack_debugger_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->gpio_trigger = of_get_named_gpio_flags(np,
			"serial,irq-gpio", 0, NULL);
}

static int earjack_debugger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct earjack_debugger_device *adev;
	struct earjack_debugger_platform_data *pdata;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct earjack_debugger_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		earjack_debugger_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
	}

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	adev = kzalloc(sizeof(struct earjack_debugger_device), GFP_KERNEL);
	if (!adev) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	adev->gpio = pdata->gpio_trigger;
	adev->irq = gpio_to_irq(pdata->gpio_trigger);
	adev->set_uart_console = msm_console_set_enable;

	platform_set_drvdata(pdev, adev);

	ret = gpio_request_one(adev->gpio, GPIOF_IN, "gpio_earjack_debugger");
	if (ret < 0) {
		pr_err("%s: failed to request gpio %d\n", __func__,
				adev->gpio);
		goto err_gpio_request;
	}

	ret = request_threaded_irq(adev->irq, NULL,
			earjack_debugger_irq_handler,
			IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT,
			"earjack_debugger_trigger", adev);
	if (ret < 0) {
		pr_err("%s: failed to request irq\n", __func__);
		goto err_request_irq;
	}

	enable_irq_wake(adev->irq);

	if (earjack_debugger_detected(adev))
		adev->set_uart_console(true);

	pr_info("%s: earjack debugger probed\n", __func__);

	return ret;

err_request_irq:
	gpio_free(adev->gpio);
err_gpio_request:
	kfree(adev);

	return ret;
}

static int earjack_debugger_remove(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	free_irq(adev->irq, adev);
	gpio_free(adev->gpio);
	kfree(adev);

	return 0;
}

static void earjack_debugger_shutdown(struct platform_device *pdev)
{
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	disable_irq(adev->irq);
}

static struct of_device_id earjack_debugger_match_table[] = {
	{ .compatible = "serial,earjack-debugger", },
	{ },
};

static struct platform_driver earjack_debugger_driver = {
	.probe = earjack_debugger_probe,
	.remove = __devexit_p(earjack_debugger_remove),
	.shutdown = earjack_debugger_shutdown,
	.driver = {
		.name = "earjack-debugger",
		.of_match_table = earjack_debugger_match_table,
	},
};

static int __init earjack_debugger_init(void)
{
	return platform_driver_register(&earjack_debugger_driver);
}

arch_initcall(earjack_debugger_init);
