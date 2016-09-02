/*
 * Interrupt support for Cirrus Logic Madera codecs
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/irqchip/irq-madera.h>
#include <linux/irqchip/irq-madera-pdata.h>
#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/mfd/madera/registers.h>

#include "irq-madera.h"

struct madera_irq_priv {
	struct device *dev;
	unsigned int irq_sem;
	int irq;
	unsigned int irq_flags;
	int irq_gpio;
	struct regmap_irq_chip_data *irq_data;
	struct irq_domain *domain;
	struct madera *madera;
};

int madera_request_irq(struct madera *madera, int irq, const char *name,
			irq_handler_t handler, void *data)
{
	struct madera_irq_priv *priv;

	if (irq < 0)
		return irq;

	if (!madera->irq_dev)
		return -EINVAL;

	priv = dev_get_drvdata(madera->irq_dev);

	return request_threaded_irq(regmap_irq_get_virq(priv->irq_data, irq),
				    NULL, handler, IRQF_ONESHOT, name, data);

}
EXPORT_SYMBOL_GPL(madera_request_irq);

void madera_free_irq(struct madera *madera, int irq, void *data)
{
	struct madera_irq_priv *priv;

	if (irq < 0)
		return;

	if (!madera->irq_dev)
		return;

	priv = dev_get_drvdata(madera->irq_dev);

	free_irq(regmap_irq_get_virq(priv->irq_data, irq), data);
}
EXPORT_SYMBOL_GPL(madera_free_irq);

int madera_set_irq_wake(struct madera *madera, int irq, int on)
{
	struct madera_irq_priv *priv;

	if (irq < 0)
		return irq;

	if (!madera->irq_dev)
		return -EINVAL;

	priv = dev_get_drvdata(madera->irq_dev);

	return irq_set_irq_wake(regmap_irq_get_virq(priv->irq_data, irq), on);
}
EXPORT_SYMBOL_GPL(madera_set_irq_wake);

static irqreturn_t madera_irq_thread(int irq, void *data)
{
	struct madera_irq_priv *priv = data;
	bool poll;
	int ret;

	dev_dbg(priv->dev, "irq_thread handler\n");

	/* The codec can generate IRQs while it is in low-power mode so
	 * we must do a runtime get before dispatching the IRQ
	 */
	ret = pm_runtime_get_sync(priv->madera->dev);
	if (ret < 0) {
		dev_err(priv->dev, "Failed to resume device: %d\n", ret);
		return IRQ_NONE;
	}

	do {
		poll = false;

		handle_nested_irq(irq_find_mapping(priv->domain, 0));

		/*
		 * Poll the IRQ pin status to see if we're really done
		 * if the interrupt controller can't do it for us.
		 */
		if (!gpio_is_valid(priv->irq_gpio)) {
			break;
		} else if (priv->irq_flags & IRQF_TRIGGER_RISING &&
			   gpio_get_value_cansleep(priv->irq_gpio)) {
			poll = true;
		} else if (priv->irq_flags & IRQF_TRIGGER_FALLING &&
			   !gpio_get_value_cansleep(priv->irq_gpio)) {
			poll = true;
		}
	} while (poll);

	pm_runtime_mark_last_busy(priv->madera->dev);
	pm_runtime_put_autosuspend(priv->madera->dev);

	return IRQ_HANDLED;
}

static void madera_irq_dummy(struct irq_data *data)
{
}

static int madera_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct madera_irq_priv *priv = irq_data_get_irq_chip_data(data);

	return irq_set_irq_wake(priv->irq, on);
}

static struct irq_chip madera_irq_chip = {
	.name		= "madera",
	.irq_disable	= madera_irq_dummy,
	.irq_enable	= madera_irq_dummy,
	.irq_ack	= madera_irq_dummy,
	.irq_mask	= madera_irq_dummy,
	.irq_unmask	= madera_irq_dummy,
	.irq_set_wake	= madera_irq_set_wake,
};

static struct lock_class_key madera_irq_lock_class;

static int madera_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct madera_irq_priv *priv = h->host_data;

	irq_set_chip_data(virq, priv);
	irq_set_lockdep_class(virq, &madera_irq_lock_class);
	irq_set_chip_and_handler(virq, &madera_irq_chip, handle_simple_irq);
	irq_set_nested_thread(virq, true);

	irq_set_noprobe(virq);

	return 0;
}

static const struct irq_domain_ops madera_domain_ops = {
	.map	= madera_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

#ifdef CONFIG_PM_SLEEP
static int madera_suspend_noirq(struct device *dev)
{
	struct madera_irq_priv *priv = dev_get_drvdata(dev);

	dev_dbg(priv->dev, "Late suspend, reenabling IRQ\n");

	if (priv->irq_sem) {
		enable_irq(priv->irq);
		priv->irq_sem = 0;
	}

	return 0;
}

static int madera_suspend(struct device *dev)
{
	struct madera_irq_priv *priv = dev_get_drvdata(dev);

	dev_dbg(priv->dev, "Early suspend, disabling IRQ\n");

	disable_irq(priv->irq);
	priv->irq_sem = 1;

	return 0;
}

static int madera_resume_noirq(struct device *dev)
{
	struct madera_irq_priv *priv = dev_get_drvdata(dev);

	dev_dbg(priv->dev, "Early resume, disabling IRQ\n");
	disable_irq(priv->irq);

	priv->irq_sem = 1;

	return 0;
}

static int madera_resume(struct device *dev)
{
	struct madera_irq_priv *priv = dev_get_drvdata(dev);

	dev_dbg(priv->dev, "Late resume, reenabling IRQ\n");
	if (priv->irq_sem) {
		enable_irq(priv->irq);
		priv->irq_sem = 0;
	}

	return 0;
}
#endif

static const struct dev_pm_ops madera_irq_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(madera_suspend, madera_resume)
#ifdef CONFIG_PM_SLEEP
	.suspend_noirq = madera_suspend_noirq,
	.resume_noirq = madera_resume_noirq,
#endif
};

static int madera_irq_of_get(struct madera_irq_priv *priv)
{
	/* all our settings are under the parent DT node */
	struct device_node *np = priv->madera->dev->of_node;
	u32 value;
	int ret;

	priv->irq = irq_of_parse_and_map(np, 0);
	if (priv->irq < 0)
		return priv->irq;

	ret = of_property_read_u32(np, "cirrus,irq_flags", &value);
	if (ret == 0)
		priv->irq_flags = value;

	priv->irq_gpio = of_get_named_gpio(np, "cirrus,irq-gpios", 0);

	return 0;
}

int madera_irq_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	struct madera_irq_priv *priv;
	const struct regmap_irq_chip *irq = NULL;
	struct irq_data *irq_data;
	int flags = IRQF_ONESHOT;
	int ret;

	dev_dbg(&pdev->dev, "probe\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->madera = madera;

	switch (madera->type) {
	case CS47L35:
		if (IS_ENABLED(CONFIG_MADERA_IRQ_CS47L35))
			irq = &cs47l35_irq;
		break;
	case CS47L85:
	case WM1840:
		if (IS_ENABLED(CONFIG_MADERA_IRQ_CS47L85))
			irq = &cs47l85_irq;
		break;
	case CS47L90:
	case CS47L91:
		if (IS_ENABLED(CONFIG_MADERA_IRQ_CS47L90))
			irq = &cs47l90_irq;
		break;
	default:
		dev_err(madera->dev, "Unsupported Madera device type %d\n",
			madera->type);
		return -EINVAL;
	}

	if (!irq) {
		dev_err(madera->dev, "No support for %s\n",
			madera_name_from_type(madera->type));
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_OF)) {
		if (!dev_get_platdata(priv->dev)) {
			ret = madera_irq_of_get(priv);
			if (ret < 0)
				return ret;
		} else {
			priv->irq = madera->irq;
			priv->irq_flags = madera->pdata.irqchip.irq_flags;
			priv->irq_gpio = madera->pdata.irqchip.irq_gpio;

			/* pdata uses 0 to mean undefined, convert to an
			 * invalid GPIO number
			 */
			if (priv->irq_gpio == 0)
				priv->irq_gpio = -1;
		}
	}


	/* Read the flags from the interrupt controller if not specified */
	if (!priv->irq_flags) {
		irq_data = irq_get_irq_data(priv->irq);
		if (!irq_data) {
			dev_err(priv->dev, "Invalid IRQ: %d\n", priv->irq);
			return -EINVAL;
		}

		priv->irq_flags = irqd_get_trigger_type(irq_data);
		switch (priv->irq_flags) {
		case IRQF_TRIGGER_LOW:
		case IRQF_TRIGGER_HIGH:
		case IRQF_TRIGGER_RISING:
		case IRQF_TRIGGER_FALLING:
			break;

		case IRQ_TYPE_NONE:
		default:
			/* Device default */
			priv->irq_flags = IRQF_TRIGGER_LOW;
			break;
		}
	}

	flags |= priv->irq_flags;

	if (flags & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_RISING)) {
		ret = regmap_update_bits(madera->regmap, MADERA_IRQ1_CTRL,
					 MADERA_IRQ_POL_MASK, 0);
		if (ret) {
			dev_err(priv->dev,
				"Couldn't set IRQ polarity: %d\n", ret);
			return ret;
		}
	}


	if (flags & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		/* A GPIO is recommended to emulate edge trigger */
		if (gpio_is_valid(priv->irq_gpio)) {
			if (gpio_to_irq(priv->irq_gpio) != priv->irq) {
				dev_warn(priv->dev,
					 "IRQ %d is not GPIO %d (%d)\n",
					 priv->irq, priv->irq_gpio,
					 gpio_to_irq(priv->irq_gpio));

				priv->irq = gpio_to_irq(priv->irq_gpio);
			}

			ret = devm_gpio_request_one(priv->dev, priv->irq_gpio,
						    GPIOF_IN, "madera IRQ");
			if (ret) {
				dev_err(priv->dev,
					"Failed to request IRQ GPIO %d: %d\n",
					priv->irq_gpio, ret);
				return ret;
			}
		}

		dev_dbg(priv->dev, "edge-trigger mode irq_gpio=%d\n",
			priv->irq_gpio);
	} else {
		dev_dbg(priv->dev, "level-trigger mode\n");
	}

	priv->domain = irq_domain_add_linear(NULL, 1, &madera_domain_ops, priv);

	ret = regmap_add_irq_chip(madera->regmap,
				  irq_create_mapping(priv->domain, 0),
				  IRQF_ONESHOT, 0, irq,
				  &priv->irq_data);
	if (ret) {
		dev_err(priv->dev, "add_irq_chip failed: %d\n", ret);
		return ret;
	}

	ret = request_threaded_irq(priv->irq, NULL, madera_irq_thread,
				   flags, "madera", priv);
	if (ret) {
		dev_err(priv->dev,
			"Failed to request threaded irq %d: %d\n",
			priv->irq, ret);
		regmap_del_irq_chip(priv->irq, priv->irq_data);
		return ret;
	}

	platform_set_drvdata(pdev, priv);
	madera->irq_dev = priv->dev;

	return 0;
}

int madera_irq_remove(struct platform_device *pdev)
{
	struct madera_irq_priv *priv = platform_get_drvdata(pdev);

	priv->madera->irq_dev = NULL;

	regmap_del_irq_chip(priv->irq, priv->irq_data);
	free_irq(priv->irq, priv);

	return 0;
}

static struct platform_driver madera_irq_driver = {
	.probe = madera_irq_probe,
	.remove = madera_irq_remove,
	.driver = {
		.name	= "madera-irq",
		.pm = &madera_irq_pm_ops,
	}
};

module_platform_driver(madera_irq_driver);

MODULE_DESCRIPTION("Madera IRQ driver");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
