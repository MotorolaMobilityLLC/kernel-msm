/*
 * Copyright (C) 2013 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/power-on.h>

struct usr_reset_warning_data {
	struct work_struct work;
	int gpio;
	int irq;
};

static void usr_reset_warning_work(struct work_struct *work)
{
	kernel_halt();
}
static irqreturn_t usr_reset_warning_isr(int irq, void *dev)
{
	struct usr_reset_warning_data *data = dev;

	disable_irq_nosync(irq);
	pr_warn("HFF: User initiated HW reset warning.\n");
	pr_warn("     2 sec to reset.Halt the kernel.\n");
	/* set HW reset power up reason */
	qpnp_pon_store_extra_reset_info(RESET_EXTRA_HW_RESET_REASON,
		RESET_EXTRA_HW_RESET_REASON);
	schedule_work(&data->work);

	return IRQ_HANDLED;
}
#ifdef CONFIG_OF
static int __devinit usr_reset_of_init(struct usr_reset_warning_data *data,
				struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	data->gpio = of_get_gpio(np, 0);
	return ((data->gpio < 0) ? -EINVAL : 0);
}
#else
static int __devinit usr_reset_of_init(struct usr_reset_warning_data *data,
				struct platform_device *pdev)
{
	return -EINVAL;
}
#endif
static int __devinit usr_reset_warning_probe(struct platform_device *pdev)
{
	struct usr_reset_warning_data *data;
	int ret = -EINVAL;

	data = devm_kzalloc(&pdev->dev,
			sizeof(struct usr_reset_warning_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	if (pdev->dev.of_node)
		ret = usr_reset_of_init(data, pdev);

	if (ret < 0) {
		pr_err("%s:unable to config IRQ: %d\n", __func__, ret);
		goto err;
	}
	ret = gpio_request(data->gpio, "usr_reset_warn");
	if (ret < 0) {
		pr_err("%s: failed to request usr reset warning gpio: %d\n",
			__func__, ret);
		goto err;
	}

	INIT_WORK(&data->work, usr_reset_warning_work);

	data->irq = gpio_to_irq(data->gpio);
	ret = request_irq(data->irq, usr_reset_warning_isr, IRQF_TRIGGER_LOW,
			"usr-reset-warning", data);
	if (ret) {
		pr_err("%s: usr-reset-warning request irq failed: %d\n",
			__func__, ret);
		goto irq_err;
	}

	enable_irq_wake(data->irq);
	platform_set_drvdata(pdev, data);

	return 0;
irq_err:
	gpio_free(data->gpio);
err:
	return ret;
}

static int __devexit usr_reset_warning_remove(struct platform_device *pdev)
{
	struct usr_reset_warning_data *data = platform_get_drvdata(pdev);

	gpio_free(data->gpio);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id usr_reset_warning_match_table[] = {
	{	.compatible = "mmi,usr-reset-warning",
	},
	{}
};
#endif

static struct platform_driver usr_reset_warning_driver = {
	.probe		= usr_reset_warning_probe,
	.remove		= __devexit_p(usr_reset_warning_remove),
	.driver		= {
		.name	= "usr-reset-warning",
		.of_match_table = of_match_ptr(usr_reset_warning_match_table),
		.owner	= THIS_MODULE,
	},
};

static int __init usr_reset_warning_init(void)
{
	return platform_driver_register(&usr_reset_warning_driver);
}

static void __exit usr_reset_warning_exit(void)
{
	platform_driver_unregister(&usr_reset_warning_driver);
}

module_init(usr_reset_warning_init);
module_exit(usr_reset_warning_exit);

MODULE_DESCRIPTION("User reset warning driver");
MODULE_LICENSE("GPL");
