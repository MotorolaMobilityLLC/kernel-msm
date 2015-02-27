/*
 * TI LM3631 Regulator Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/err.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

#define ENABLE_TIME_USEC		1000

enum lm3631_regulator_id {
	LM3631_BOOST,		/* Boost output */
	LM3631_LDO_CONT,	/* Display panel controller */
	LM3631_LDO_OREF,	/* Gamma reference */
	LM3631_LDO_POS,		/* Positive display bias output */
	LM3631_LDO_NEG,		/* Negative display bias output */
};

struct lm3631_regulator {
	struct ti_lmu *lmu;
	struct regulator_desc *desc;
	struct regulator_dev *regulator;
};

static const int lm3631_boost_vtbl[] = {
	4500000, 4550000, 4600000, 4650000, 4700000, 4750000, 4800000, 4850000,
	4900000, 4950000, 5000000, 5050000, 5100000, 5150000, 5200000, 5250000,
	5300000, 5350000, 5400000, 5450000, 5500000, 5550000, 5600000, 5650000,
	5700000, 5750000, 5800000, 5850000, 5900000, 5950000, 6000000, 6050000,
	6100000, 6150000, 6200000, 6250000, 6300000, 6350000,
};

static const int lm3631_ldo_cont_vtbl[] = {
	1800000, 2300000, 2800000, 3300000,
};

static const int lm3631_ldo_target_vtbl[] = {
	4000000, 4050000, 4100000, 4150000, 4200000, 4250000, 4300000, 4350000,
	4400000, 4450000, 4500000, 4550000, 4600000, 4650000, 4700000, 4750000,
	4800000, 4850000, 4900000, 4950000, 5000000, 5050000, 5100000, 5150000,
	5200000, 5250000, 5300000, 5350000, 5400000, 5450000, 5500000, 5550000,
	5600000, 5650000, 5700000, 5750000, 5800000, 5850000, 5900000, 5950000,
	6000000,
};

const int ldo_cont_enable_time[] = {
	0, 2000, 5000, 10000, 20000, 50000, 100000, 200000,
};

static int lm3631_regulator_enable_time(struct regulator_dev *rdev)
{
	struct lm3631_regulator *lm3631_regulator = rdev_get_drvdata(rdev);
	enum lm3631_regulator_id id = rdev_get_id(rdev);
	u8 val, addr, mask;

	switch (id) {
	case LM3631_LDO_CONT:
		addr = LM3631_REG_ENTIME_VCONT;
		mask = LM3631_ENTIME_CONT_MASK;
		break;
	case LM3631_LDO_OREF:
		addr = LM3631_REG_ENTIME_VOREF;
		mask = LM3631_ENTIME_MASK;
		break;
	case LM3631_LDO_POS:
		addr = LM3631_REG_ENTIME_VPOS;
		mask = LM3631_ENTIME_MASK;
		break;
	case LM3631_LDO_NEG:
		addr = LM3631_REG_ENTIME_VNEG;
		mask = LM3631_ENTIME_MASK;
		break;
	default:
		return -EINVAL;
	}

	if (ti_lmu_read_byte(lm3631_regulator->lmu, addr, &val))
		return -EINVAL;

	val = (val & mask) >> LM3631_ENTIME_SHIFT;

	if (id == LM3631_LDO_CONT)
		return ldo_cont_enable_time[val];
	else
		return ENABLE_TIME_USEC * val;
}

static struct regulator_ops lm3631_boost_voltage_table_ops = {
	.list_voltage     = regulator_list_voltage_table,
	.set_voltage_sel  = regulator_set_voltage_sel_regmap,
	.get_voltage_sel  = regulator_get_voltage_sel_regmap,
};

static struct regulator_ops lm3631_regulator_voltage_table_ops = {
	.list_voltage     = regulator_list_voltage_table,
	.set_voltage_sel  = regulator_set_voltage_sel_regmap,
	.get_voltage_sel  = regulator_get_voltage_sel_regmap,
	.enable           = regulator_enable_regmap,
	.disable          = regulator_disable_regmap,
	.is_enabled       = regulator_is_enabled_regmap,
	.enable_time      = lm3631_regulator_enable_time,
};

static struct regulator_desc lm3631_regulator_desc[] = {
	{
		.name           = "vboost",
		.id             = LM3631_BOOST,
		.ops            = &lm3631_boost_voltage_table_ops,
		.n_voltages     = ARRAY_SIZE(lm3631_boost_vtbl),
		.volt_table     = lm3631_boost_vtbl,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_BOOST,
		.vsel_mask      = LM3631_VOUT_MASK,
	},
	{
		.name           = "ldo_cont",
		.id             = LM3631_LDO_CONT,
		.ops            = &lm3631_regulator_voltage_table_ops,
		.n_voltages     = ARRAY_SIZE(lm3631_ldo_cont_vtbl),
		.volt_table     = lm3631_ldo_cont_vtbl,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_CONT,
		.vsel_mask      = LM3631_VOUT_CONT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL2,
		.enable_mask    = LM3631_EN_CONT_MASK,
	},
	{
		.name           = "ldo_oref",
		.id             = LM3631_LDO_OREF,
		.ops            = &lm3631_regulator_voltage_table_ops,
		.n_voltages     = ARRAY_SIZE(lm3631_ldo_target_vtbl),
		.volt_table     = lm3631_ldo_target_vtbl,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_OREF,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_OREF_MASK,
	},
	{
		.name           = "ldo_vpos",
		.id             = LM3631_LDO_POS,
		.ops            = &lm3631_regulator_voltage_table_ops,
		.n_voltages     = ARRAY_SIZE(lm3631_ldo_target_vtbl),
		.volt_table     = lm3631_ldo_target_vtbl,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_POS,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_VPOS_MASK,
	},
	{
		.name           = "ldo_vneg",
		.id             = LM3631_LDO_NEG,
		.ops            = &lm3631_regulator_voltage_table_ops,
		.n_voltages     = ARRAY_SIZE(lm3631_ldo_target_vtbl),
		.volt_table     = lm3631_ldo_target_vtbl,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_NEG,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_VNEG_MASK,
	},
};

	/* Names in the match table have to match child node names in the
	 regulator lm3631 entry. First match will be taken for each name.*/
static struct of_regulator_match lm3631_regulator_matches[] = {
	{ .name = "vboost", .driver_data = (void *)LM3631_BOOST, },
	{ .name = "vcont",  .driver_data = (void *)LM3631_LDO_CONT, },
	{ .name = "voref",  .driver_data = (void *)LM3631_LDO_OREF, },
	{ .name = "vpos",   .driver_data = (void *)LM3631_LDO_POS,  },
	{ .name = "vneg",   .driver_data = (void *)LM3631_LDO_NEG,  },
};

static struct regulator_init_data *
lm3631_regulator_parse_dt(struct device *dev,
			  struct lm3631_regulator *lm3631_regulator, int id)
{
	struct device_node *node = dev->of_node;
	int count;

	count = of_regulator_match(dev, node, &lm3631_regulator_matches[id], 1);
	if (count <= 0)
		return ERR_PTR(-ENODEV);

	return lm3631_regulator_matches[id].init_data;
}

static int lm3631_regulator_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct lm3631_regulator *lm3631_regulator;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = { };
	struct regulator_dev *rdev;
	int id = pdev->id;
	int ret;

	lm3631_regulator = devm_kzalloc(&pdev->dev, sizeof(*lm3631_regulator),
					GFP_KERNEL);
	if (!lm3631_regulator)
		return -ENOMEM;

	lm3631_regulator->lmu = lmu;

	init_data = lmu->pdata->regulator_data[id];
	if (!init_data) {
		if (IS_ENABLED(CONFIG_OF)) {
			init_data = lm3631_regulator_parse_dt(&pdev->dev,
							      lm3631_regulator,
							      id);
			if (IS_ERR(init_data))
				return PTR_ERR(init_data);
		}
	}

	/* Use name from device tree. Constraints are filled in parse dt*/
	lm3631_regulator_desc[id].name = init_data->constraints.name;
	cfg.dev = pdev->dev.parent;
	cfg.init_data = init_data;
	cfg.driver_data = lm3631_regulator;
	cfg.regmap = lmu->regmap;

	rdev = regulator_register(&lm3631_regulator_desc[id], &cfg);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(&pdev->dev, "[%d] regulator register err: %d\n",
			id + 1, ret);
		return ret;
	}

	lm3631_regulator->regulator = rdev;
	platform_set_drvdata(pdev, lm3631_regulator);
	dev_info(&pdev->dev, "%s success for %s\n",
		__func__, lm3631_regulator_desc[id].name);

	return 0;
}

static int lm3631_regulator_remove(struct platform_device *pdev)
{
	struct lm3631_regulator *lm3631_regulator = platform_get_drvdata(pdev);

	regulator_unregister(lm3631_regulator->regulator);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3631_regulator_of_match[] = {
	{ .compatible = "ti,lm3631-regulator", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3631_regulator_of_match);
#endif

static struct platform_driver lm3631_regulator_driver = {
	.probe = lm3631_regulator_probe,
	.remove = lm3631_regulator_remove,
	.driver = {
		.name = "lm3631-regulator",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3631_regulator_of_match),
	},
};

module_platform_driver(lm3631_regulator_driver);

MODULE_DESCRIPTION("TI LM3631 Regulator Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3631-regulator");
