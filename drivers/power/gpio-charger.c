/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *  Driver for chargers which report their online status through a GPIO pin
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>

#include <linux/power/gpio-charger.h>

struct gpio_charger {
	const struct gpio_charger_platform_data *pdata;
	unsigned int irq;

	struct power_supply charger;

	struct switch_dev *sdev;
	struct work_struct work;
};

static inline struct gpio_charger *psy_to_gpio_charger(struct power_supply *psy)
{
	return container_of(psy, struct gpio_charger, charger);
}

static inline struct gpio_charger *work_to_gpio_charger(struct work_struct *wk)
{
	return container_of(wk, struct gpio_charger, work);
}

static inline int gpio_state(const struct gpio_charger_platform_data *pdata)
{
	return !!gpio_get_value_cansleep(pdata->gpio) ^
	       !!pdata->gpio_active_low;
}

static irqreturn_t gpio_charger_irq(int irq, void *devid)
{
	struct power_supply *charger = devid;
	struct gpio_charger *gpio_charger = psy_to_gpio_charger(charger);

	if (gpio_charger->sdev)
		schedule_work(&gpio_charger->work);

	power_supply_changed(charger);

	return IRQ_HANDLED;
}

static int gpio_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct gpio_charger *gpio_charger = psy_to_gpio_charger(psy);
	const struct gpio_charger_platform_data *pdata = gpio_charger->pdata;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = gpio_state(pdata);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void gpio_charger_work(struct work_struct *work)
{
	struct gpio_charger *gpio_charger = work_to_gpio_charger(work);
	const struct gpio_charger_platform_data *pdata = gpio_charger->pdata;

	switch_set_state(gpio_charger->sdev, gpio_state(pdata));
}

static enum power_supply_property gpio_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};


static struct gpio_charger_platform_data *
of_get_gpio_charger_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct gpio_charger_platform_data *pdata;
	const char *string;
	int i, count, size;
	u32 val;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Failed to alloc platform data\n");
		return NULL;
	}

	of_property_read_string(np, "charger-name", &pdata->name);

	if (of_property_read_u32(np, "charger-type", &val)) {
		dev_err(dev, "No 'charger-type' property\n");
		return NULL;
	}
	pdata->type = val;

	pdata->gpio = of_get_named_gpio(np, "gpio", 0);
	if (pdata->gpio < 0) {
		dev_err(dev, "No invalid 'gpio' property\n");
		return NULL;
	}

	pdata->gpio_active_low = of_property_read_bool(np, "gpio_active_low");

	count = of_property_count_strings(np, "supplied_to");
	if (count > 0) {
		size = count * sizeof(*pdata->supplied_to);
		pdata->supplied_to = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!pdata->supplied_to) {
			dev_err(dev, "Failed to alloc supplied_to\n");
			return NULL;
		}

		/* Make copies of the DT strings for const-correctness */
		for (i = 0; i < count; i++) {
			if (of_property_read_string_index(np, "supplied_to", i,
							  &string)) {
				dev_err(dev, "Failed to read supplied_to"
					" supplied_to[%d]\n", i);
				for (i--; i >= 0; i--)
					kfree(pdata->supplied_to[i]);
				return NULL;
			}
			pdata->supplied_to[i] = kstrdup(string,  GFP_KERNEL);
			if (!pdata->supplied_to[i]) {
				dev_err(dev, "Failed to alloc space for"
					" supplied_to[%d]\n", i);
				return NULL;
			}
		}
		pdata->num_supplicants = count;
	}

	of_property_read_string(np, "switch-name", &pdata->switch_name);

	pdata->wakeup = of_property_read_bool(np, "wakeup");

	return pdata;
}

static int gpio_charger_probe(struct platform_device *pdev)
{
	const struct gpio_charger_platform_data *pdata;
	struct gpio_charger *gpio_charger;
	struct power_supply *charger;
	int ret;
	int irq;

	if (pdev->dev.of_node)
		pdata = of_get_gpio_charger_pdata(&pdev->dev);
	else
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -EINVAL;
	}

	if (!gpio_is_valid(pdata->gpio)) {
		dev_err(&pdev->dev, "Invalid gpio pin\n");
		return -EINVAL;
	}

	gpio_charger = devm_kzalloc(&pdev->dev, sizeof(*gpio_charger),
					GFP_KERNEL);
	if (!gpio_charger) {
		dev_err(&pdev->dev, "Failed to alloc driver structure\n");
		return -ENOMEM;
	}

	charger = &gpio_charger->charger;

	charger->name = pdata->name ? pdata->name : "gpio-charger";
	charger->type = pdata->type;
	charger->properties = gpio_charger_properties;
	charger->num_properties = ARRAY_SIZE(gpio_charger_properties);
	charger->get_property = gpio_charger_get_property;
	charger->supplied_to = pdata->supplied_to;
	charger->num_supplicants = pdata->num_supplicants;

	ret = gpio_request(pdata->gpio, dev_name(&pdev->dev));
	if (ret) {
		dev_err(&pdev->dev, "Failed to request gpio pin: %d\n", ret);
		goto err_free;
	}
	ret = gpio_direction_input(pdata->gpio);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set gpio to input: %d\n", ret);
		goto err_gpio_free;
	}

	gpio_charger->pdata = pdata;

	ret = power_supply_register(&pdev->dev, charger);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register power supply: %d\n",
			ret);
		goto err_gpio_free;
	}

	irq = gpio_to_irq(pdata->gpio);
	if (irq > 0) {
		ret = request_any_context_irq(irq, gpio_charger_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				dev_name(&pdev->dev), charger);
		if (ret < 0) {
			dev_warn(&pdev->dev, "Failed to request irq: %d\n", ret);
		} else {
			gpio_charger->irq = irq;
			device_init_wakeup(&pdev->dev, pdata->wakeup);
		}
	}

	/* Switch needs valid irq since its state changes when irq goes off */
	if (pdata->switch_name && !gpio_charger->irq) {
		dev_warn(&pdev->dev, "Must have valid irq for the switch\n");
	} else if (pdata->switch_name && gpio_charger->irq) {
		gpio_charger->sdev = devm_kzalloc(&pdev->dev,
						  sizeof(*gpio_charger->sdev),
						  GFP_KERNEL);
		if (!gpio_charger->sdev) {
			dev_err(&pdev->dev, "Failed to alloc switch device\n");
			ret = -ENOMEM;
			goto err_irq_free;
		}

		gpio_charger->sdev->name = pdata->switch_name;
		ret = switch_dev_register(gpio_charger->sdev);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register switch device:"
				" %d\n", ret);
			goto err_irq_free;
		}
		INIT_WORK(&gpio_charger->work, gpio_charger_work);
	}

	platform_set_drvdata(pdev, gpio_charger);

	/* Set initial state */
	if (gpio_charger->sdev)
		schedule_work(&gpio_charger->work);

	return 0;

err_irq_free:
	if (gpio_charger->irq)
		free_irq(gpio_charger->irq, &gpio_charger->charger);
err_gpio_free:
	gpio_free(pdata->gpio);
err_free:
	return ret;
}

static int gpio_charger_remove(struct platform_device *pdev)
{
	struct gpio_charger *gpio_charger = platform_get_drvdata(pdev);
	int i;

	if (gpio_charger->sdev)
		switch_dev_unregister(gpio_charger->sdev);

	if (gpio_charger->irq)
		free_irq(gpio_charger->irq, &gpio_charger->charger);

	power_supply_unregister(&gpio_charger->charger);

	gpio_free(gpio_charger->pdata->gpio);

	/* Free memory allocated for the copies of the DT strings */
	if (pdev->dev.of_node && gpio_charger->pdata->supplied_to)
		for (i = 0; i < gpio_charger->pdata->num_supplicants; i++)
			kfree(gpio_charger->pdata->supplied_to[i]);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_charger_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_charger *gpio_charger = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(gpio_charger->irq);

	return 0;
}

static int gpio_charger_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_charger *gpio_charger = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev)) {
		disable_irq_wake(gpio_charger->irq);
	} else {
		/* Set correct state */
		power_supply_changed(&gpio_charger->charger);
		if (gpio_charger->sdev)
			schedule_work(&gpio_charger->work);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_charger_pm_ops, gpio_charger_suspend,
				gpio_charger_resume);

#if defined(CONFIG_OF)
static const struct of_device_id gpio_charger_of_match[] = {
	{ .compatible = "gpio-charger", },
	{ }
};
MODULE_DEVICE_TABLE(of, gpio_charger_of_match);
#endif

static struct platform_driver gpio_charger_driver = {
	.probe = gpio_charger_probe,
	.remove = gpio_charger_remove,
	.driver = {
		.name = "gpio-charger",
		.owner = THIS_MODULE,
		.pm = &gpio_charger_pm_ops,
		.of_match_table = gpio_charger_of_match,
	},
};

module_platform_driver(gpio_charger_driver);

MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_DESCRIPTION("Driver for chargers which report their online status through a GPIO");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-charger");
