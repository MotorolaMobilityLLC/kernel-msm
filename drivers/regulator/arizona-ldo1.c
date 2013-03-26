/*
 * arizona-ldo1.c  --  LDO1 supply for Arizona devices
 *
 * Copyright 2012 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/pdata.h>
#include <linux/mfd/arizona/registers.h>


#define  MIN_UV 	900000
#define  UV_STEP 	50000
#define  HI_PWR_UV	1800000

struct arizona_ldo1 {
	struct regulator_dev *regulator;
	struct arizona *arizona;

	struct regulator_consumer_supply supply;
	struct regulator_init_data init_data;

	int ena;
	int ena_state;
};

static int arizona_ldo_reg_list_voltage_linear(struct regulator_dev *rdev,
					       unsigned int selector)
{
	if (selector >= rdev->desc->n_voltages)
		return -EINVAL;

	if (selector == rdev->desc->n_voltages - 1)
		return HI_PWR_UV;
	else
		return MIN_UV + (UV_STEP * selector);
}

static int arizona_ldo_reg_get_voltage_sel(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);

	ret = regmap_read(ldo1->arizona->regmap,
			  ARIZONA_LDO1_CONTROL_2,
			  &val);
	if (ret != 0)
		return ret;

	if (val & ARIZONA_LDO1_HI_PWR)
		return rdev->desc->n_voltages - 1;

	ret = regmap_read(ldo1->arizona->regmap,
			  ARIZONA_LDO1_CONTROL_1,
			  &val);
	if (ret != 0)
		return ret;

	val &= ARIZONA_LDO1_VSEL_MASK;
	val >>= ARIZONA_LDO1_VSEL_SHIFT;

	return val;
}

static int arizona_ldo_reg_set_voltage_sel(struct regulator_dev *rdev,
					   unsigned sel)
{
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	if (sel == rdev->desc->n_voltages - 1)
		val = ARIZONA_LDO1_HI_PWR;
	else
		val = 0;

	ret = regmap_update_bits(ldo1->arizona->regmap, ARIZONA_LDO1_CONTROL_2,
			         ARIZONA_LDO1_HI_PWR, val);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(ldo1->arizona->regmap,
				  ARIZONA_DYNAMIC_FREQUENCY_SCALING_1,
				  ARIZONA_SUBSYS_MAX_FREQ, val);
	if (ret < 0)
		return ret;

	if (val)
		return 0;

	sel <<= ARIZONA_LDO1_VSEL_SHIFT;

	return regmap_update_bits(ldo1->arizona->regmap,
				  ARIZONA_LDO1_CONTROL_1,
				  ARIZONA_LDO1_VSEL_MASK, sel);
}

static int arizona_ldo_enable_time(struct regulator_dev *rdev)
{
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);

	switch (ldo1->arizona->type) {
	case WM5102:
		return 1500;
	default:
		return 500;
	}
}

static int arizona_ldo_enable(struct regulator_dev *rdev)
{
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);

	if (!ldo1->ena)
		return -EINVAL;

	gpio_set_value_cansleep(ldo1->ena, 1);
	ldo1->ena_state = 1;

	return 0;
}

static int arizona_ldo_disable(struct regulator_dev *rdev)
{
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);

	if (!ldo1->ena)
		return -EINVAL;

	gpio_set_value_cansleep(ldo1->ena, 0);
	ldo1->ena_state = 0;

	return 0;
}

static int arizona_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct arizona_ldo1 *ldo1 = rdev_get_drvdata(rdev);

	if (!ldo1->ena)
		return -EINVAL;

	return ldo1->ena_state;
}

static struct regulator_ops arizona_ldo1_ops = {
	.enable = arizona_ldo_enable,
	.disable = arizona_ldo_disable,
	.is_enabled = arizona_ldo_is_enabled,
	.list_voltage = arizona_ldo_reg_list_voltage_linear,
	.get_voltage_sel = arizona_ldo_reg_get_voltage_sel,
	.set_voltage_sel = arizona_ldo_reg_set_voltage_sel,
	.enable_time = arizona_ldo_enable_time,
};

static struct regulator_desc arizona_ldo1 = {
	.name = "LDO1",
	.type = REGULATOR_VOLTAGE,
	.ops = &arizona_ldo1_ops,

	.n_voltages = 8,

	.owner = THIS_MODULE,
};

static const struct regulator_init_data arizona_ldo1_dvfs = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 1800000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
				  REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies = 1,
};

static const struct regulator_init_data arizona_ldo1_default = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
};

static __devinit int arizona_ldo1_probe(struct platform_device *pdev)
{
	struct arizona *arizona = dev_get_drvdata(pdev->dev.parent);
	struct arizona_ldo1 *ldo1;
	struct regulator_init_data *init_data;
	int ret;

	ldo1 = devm_kzalloc(&pdev->dev, sizeof(*ldo1), GFP_KERNEL);
	if (ldo1 == NULL) {
		dev_err(&pdev->dev, "Unable to allocate private data\n");
		return -ENOMEM;
	}

	if (arizona->pdata.ldoena) {
		ldo1->ena = arizona->pdata.ldoena;
		ret = gpio_request_one(ldo1->ena, GPIOF_OUT_INIT_LOW,
					"Arizona LDOENA");
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to request LDOENA: %d\n",
				ret);
			return ret;
		}
	}

	ldo1->arizona = arizona;

	/*
	 * Since the chip usually supplies itself we provide some
	 * default init_data for it.  This will be overridden with
	 * platform data if provided.
	 */
	switch (arizona->type) {
	case WM5102:
		ldo1->init_data = arizona_ldo1_dvfs;
		break;
	default:
		ldo1->init_data = arizona_ldo1_default;
		break;
	}

	ldo1->init_data.consumer_supplies = &ldo1->supply;
	ldo1->supply.supply = "DCVDD";
	ldo1->supply.dev_name = dev_name(arizona->dev);

	if (arizona->pdata.ldo1)
		init_data = arizona->pdata.ldo1;
	else
		init_data = &ldo1->init_data;

	ldo1->regulator = regulator_register(&arizona_ldo1,
					     arizona->dev, init_data,
					     ldo1, NULL);

	if (IS_ERR(ldo1->regulator)) {
		ret = PTR_ERR(ldo1->regulator);
		dev_err(arizona->dev, "Failed to register LDO1 supply: %d\n",
			ret);
		return ret;
	}

	platform_set_drvdata(pdev, ldo1);

	return 0;
}

static __devexit int arizona_ldo1_remove(struct platform_device *pdev)
{
	struct arizona_ldo1 *ldo1 = platform_get_drvdata(pdev);

	regulator_unregister(ldo1->regulator);

	return 0;
}

static struct platform_driver arizona_ldo1_driver = {
	.probe = arizona_ldo1_probe,
	.remove = __devexit_p(arizona_ldo1_remove),
	.driver		= {
		.name	= "arizona-ldo1",
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(arizona_ldo1_driver);

/* Module information */
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_DESCRIPTION("Arizona LDO1 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:arizona-ldo1");
