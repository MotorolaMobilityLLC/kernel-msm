/*
 * gpio-madera.c  --  GPIO support for Cirrus Logic Madera codecs
 *
 * Copyright 2015 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/mfd/madera/registers.h>

struct madera_gpio {
	struct madera *madera;
	struct gpio_chip gpio_chip;
};

static int mmi_is_factory_mode(void)
{
	static int is_factory_mode = -1;

	struct device_node *np = of_find_node_by_path("/chosen");

	if (is_factory_mode != -1)
		return is_factory_mode;

	if (np)
		is_factory_mode = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return is_factory_mode;
}

static inline struct madera_gpio *to_madera_gpio(struct gpio_chip *chip)
{
	return container_of(chip, struct madera_gpio, gpio_chip);
}

static int madera_gpio_direction_in(struct gpio_chip *chip, unsigned int offset)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;

	return regmap_update_bits(madera->regmap,
				  MADERA_GPIO1_CTRL_2 + (2 * offset),
				  MADERA_GP1_DIR_MASK, MADERA_GP1_DIR);
}

static int madera_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;
	unsigned int val;
	int ret;

	ret = regmap_read(madera->regmap,
			  MADERA_GPIO1_CTRL_1 + (2 * offset), &val);
	if (ret < 0)
		return ret;

	if (val & MADERA_GP1_LVL_MASK)
		return 1;
	else
		return 0;
}

static int madera_gpio_direction_out(struct gpio_chip *chip,
				     unsigned int offset, int value)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;
	int ret;

	if (value)
		value = MADERA_GP1_LVL;

	ret = regmap_update_bits(madera->regmap,
				 MADERA_GPIO1_CTRL_2 + (2 * offset),
				 MADERA_GP1_DIR_MASK, 0);
	if (ret < 0)
		return ret;

	return regmap_update_bits(madera->regmap,
				  MADERA_GPIO1_CTRL_1 + (2 * offset),
				  MADERA_GP1_LVL_MASK, value);
}

static void madera_gpio_set(struct gpio_chip *chip, unsigned int offset,
			    int value)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;

	if (value)
		value = MADERA_GP1_LVL;

	regmap_update_bits(madera->regmap,
			   MADERA_GPIO1_CTRL_1 + (2 * offset),
			   MADERA_GP1_LVL_MASK, value);
}

static int madera_gpio_direction_in_factorymode(struct gpio_chip *chip,
					unsigned int offset)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;
	int ret;

	ret = regmap_update_bits(madera->regmap,
				  MADERA_GPIO1_CTRL_1 + (2 * offset),
				  MADERA_GP1_FN_MASK,
				  MADERA_GP1_FN_GPIO);
	if (ret) {
		pr_err("%s regmap_update_bits error %d\n", __func__, ret);
		return ret;
	}

	return regmap_update_bits(madera->regmap,
				  MADERA_GPIO1_CTRL_2 + (2 * offset),
				  MADERA_GP1_DIR_MASK, MADERA_GP1_DIR);
}

static int madera_gpio_direction_out_factorymode(struct gpio_chip *chip,
				     unsigned int offset, int value)
{
	struct madera_gpio *madera_gpio = to_madera_gpio(chip);
	struct madera *madera = madera_gpio->madera;
	int ret;

	if (value)
		value = MADERA_GP1_LVL;

	ret = regmap_update_bits(madera->regmap,
				 MADERA_GPIO1_CTRL_2 + (2 * offset),
				 MADERA_GP1_DIR_MASK, 0);
	if (ret < 0) {
		pr_err("%s regmap_update_bits error %d\n", __func__, ret);
		return ret;
	}

	value |= MADERA_GP1_FN_GPIO;

	return regmap_update_bits(madera->regmap,
				  MADERA_GPIO1_CTRL_1 + (2 * offset),
				  (MADERA_GP1_LVL_MASK | MADERA_GP1_FN_MASK),
				  value);
}

static struct gpio_chip template_chip = {
	.label			= "madera",
	.owner			= THIS_MODULE,
	.direction_input	= madera_gpio_direction_in,
	.get			= madera_gpio_get,
	.direction_output	= madera_gpio_direction_out,
	.set			= madera_gpio_set,
	.can_sleep		= true,
};

static int madera_gpio_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	struct madera_pdata *pdata = dev_get_platdata(madera->dev);
	struct madera_gpio *madera_gpio;
	int ret;

	madera_gpio = devm_kzalloc(&pdev->dev, sizeof(*madera_gpio),
				   GFP_KERNEL);
	if (!madera_gpio)
		return -ENOMEM;

	if (mmi_is_factory_mode() == 1) {
		pr_info("%s fset gpiochip function to the factorymode ones\n",
			__func__);
		template_chip.direction_input = madera_gpio_direction_in_factorymode;
		template_chip.direction_output = madera_gpio_direction_out_factorymode;
	}

	madera_gpio->madera = madera;
	madera_gpio->gpio_chip = template_chip;
	madera_gpio->gpio_chip.dev = &pdev->dev;
#ifdef CONFIG_OF_GPIO
	madera_gpio->gpio_chip.of_node = madera->dev->of_node;
#endif

	switch (madera->type) {
	case CS47L35:
		madera_gpio->gpio_chip.ngpio = CS47L35_NUM_GPIOS;
		break;
	case CS47L85:
	case WM1840:
		madera_gpio->gpio_chip.ngpio = CS47L85_NUM_GPIOS;
		break;
	case CS47L90:
	case CS47L91:
		madera_gpio->gpio_chip.ngpio = CS47L90_NUM_GPIOS;
		break;
	default:
		dev_err(&pdev->dev, "Unknown chip variant %d\n",
			madera->type);
		return -EINVAL;
	}

	if (pdata && pdata->gpio_base)
		madera_gpio->gpio_chip.base = pdata->gpio_base;
	else
		madera_gpio->gpio_chip.base = -1;

	ret = gpiochip_add(&madera_gpio->gpio_chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip, %d\n",
			ret);
		return ret;
	}

	platform_set_drvdata(pdev, madera_gpio);

	return ret;
}

static int madera_gpio_remove(struct platform_device *pdev)
{
	struct madera_gpio *madera_gpio = platform_get_drvdata(pdev);

	gpiochip_remove(&madera_gpio->gpio_chip);
	return 0;
}

static struct platform_driver madera_gpio_driver = {
	.driver.name	= "madera-gpio",
	.driver.owner	= THIS_MODULE,
	.probe		= madera_gpio_probe,
	.remove		= madera_gpio_remove,
};

module_platform_driver(madera_gpio_driver);

MODULE_DESCRIPTION("GPIO interface for Madera devices");
MODULE_AUTHOR("Nariman Poushin <nariman@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:madera-gpio");
