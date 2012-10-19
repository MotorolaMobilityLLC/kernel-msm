/*
 * earjack debugger trigger
 *
 * Copyright (C) 2012 LGE, Inc.
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
#include <linux/workqueue.h>

#include <mach/gpiomux.h>

#include "board-mako.h"
#include "board-mako-earjack-debugger.h"

#define MAKO_UART_TX_GPIO	10
#define MAKO_UART_RX_GPIO	11

static struct gpiomux_setting gsbi4_uart = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi4_gpio = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static bool mako_uart_initialized = false;

struct earjack_debugger_device {
	int gpio;
	int irq;
	int saved_detect;
	struct delayed_work work;
	void (*set_uart_console)(int enable);
};

static void earjack_debugger_set_gpiomux_uart(int enable)
{
	if (enable) {
		msm_gpiomux_write(MAKO_UART_TX_GPIO, GPIOMUX_ACTIVE,
				&gsbi4_uart, NULL);
		msm_gpiomux_write(MAKO_UART_TX_GPIO, GPIOMUX_SUSPENDED,
				&gsbi4_uart, NULL);
		msm_gpiomux_write(MAKO_UART_RX_GPIO, GPIOMUX_ACTIVE,
				&gsbi4_uart, NULL);
		msm_gpiomux_write(MAKO_UART_RX_GPIO, GPIOMUX_SUSPENDED,
				&gsbi4_uart, NULL);
	} else {
		msm_gpiomux_write(MAKO_UART_TX_GPIO, GPIOMUX_ACTIVE,
				&gsbi4_gpio, NULL);
		msm_gpiomux_write(MAKO_UART_TX_GPIO, GPIOMUX_SUSPENDED,
				&gsbi4_gpio, NULL);
		msm_gpiomux_write(MAKO_UART_RX_GPIO, GPIOMUX_ACTIVE,
				&gsbi4_gpio, NULL);
		msm_gpiomux_write(MAKO_UART_RX_GPIO, GPIOMUX_SUSPENDED,
				&gsbi4_gpio, NULL);
	}
}

static int earjack_debugger_detected(void *dev)
{
	struct earjack_debugger_device *adev = dev;

	return !!gpio_get_value(adev->gpio);
}

static void earjack_debugger_set_console(void *dev)
{
	struct earjack_debugger_device *adev = dev;

	if (earjack_debugger_detected(adev)) {
		if (!mako_uart_initialized) {
			mako_uart_initialized = true;
			earjack_debugger_set_gpiomux_uart(1);
			mdelay(1);
		}
		adev->set_uart_console(1);
	} else {
		adev->set_uart_console(0);
	}
}

static irqreturn_t earjack_debugger_irq_handler(int irq, void *_dev)
{
	struct earjack_debugger_device *adev = _dev;

	earjack_debugger_set_console(adev);

	return IRQ_HANDLED;
}

static void set_console_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct earjack_debugger_device *adev = container_of(
			dwork, struct earjack_debugger_device, work);
	int detect = 0;

	detect = earjack_debugger_detected(adev);
	if (detect != adev->saved_detect)
		earjack_debugger_set_console(adev);
}

static int earjack_debugger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct earjack_debugger_device *adev;
	struct earjack_debugger_platform_data *pdata =
		pdev->dev.platform_data;

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
	adev->set_uart_console = pdata->set_uart_console;

	if (adev->set_uart_console)
		adev->set_uart_console(0);

	platform_set_drvdata(pdev, adev);

	ret = gpio_request_one(adev->gpio, GPIOF_IN,
			"gpio_earjack_debugger");
	if (ret < 0) {
		pr_err("%s: failed to request gpio %d\n", __func__,
				adev->gpio);
		goto err_gpio_request;
	}

	INIT_DELAYED_WORK(&adev->work, set_console_work);

	ret = request_threaded_irq(adev->irq, NULL, earjack_debugger_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			"earjack_debugger_trigger", adev);
	if (ret < 0) {
		pr_err("%s: failed to request irq\n", __func__);
		goto err_request_irq;
	}

	if (earjack_debugger_detected(adev)) {
		if (adev->set_uart_console) {
			mako_uart_initialized = true;
			earjack_debugger_set_gpiomux_uart(1);
			adev->set_uart_console(1);
		}
	} else {
		earjack_debugger_set_gpiomux_uart(0);
	}

	pr_info("earjack debugger probed\n");

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

static int earjack_debugger_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	disable_irq(adev->irq);
	adev->saved_detect = earjack_debugger_detected(adev);

	return 0;
}

static int earjack_debugger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct earjack_debugger_device *adev = platform_get_drvdata(pdev);

	schedule_delayed_work(&adev->work, msecs_to_jiffies(10));
	enable_irq(adev->irq);

	return 0;
}

static const struct dev_pm_ops earjack_debugger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(earjack_debugger_suspend,
			earjack_debugger_resume)
};

static struct platform_driver earjack_debugger_driver = {
	.probe = earjack_debugger_probe,
	.remove = earjack_debugger_remove,
	.shutdown = earjack_debugger_shutdown,
	.driver = {
		.name = "earjack-debugger-trigger",
		.pm = &earjack_debugger_pm_ops,
	},
};

static int __init earjack_debugger_init(void)
{
	return platform_driver_register(&earjack_debugger_driver);
}

subsys_initcall_sync(earjack_debugger_init);
