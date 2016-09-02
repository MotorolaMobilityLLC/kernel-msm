/*
 * madera-ldo1.c  --  Driver for the LDO1 regulator on Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#include <linux/regulator/madera-ldo1.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/mfd/madera/registers.h>

struct madera_ldo1 {
	struct regulator_dev *regulator;

	struct regulator_consumer_supply supply;
	struct regulator_init_data init_data;
};

static const struct regulator_ops madera_ldo1_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

static const struct regulator_desc madera_ldo1 = {
	.name = "LDO1",
	.supply_name = "LDOVDD",
	.type = REGULATOR_VOLTAGE,
	.ops = &madera_ldo1_ops,

	.vsel_reg = MADERA_LDO1_CONTROL_1,
	.vsel_mask = MADERA_LDO1_VSEL_MASK,
	.min_uV = 900000,
	.uV_step = 25000,
	.n_voltages = 13,
	.enable_time = 3000,

	.owner = THIS_MODULE,
};

static const struct regulator_init_data madera_ldo1_default = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 1200000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
};

static int madera_ldo1_of_get_pdata(struct madera *madera,
				    struct regulator_config *config,
				    const struct regulator_desc *desc)
{
	struct madera_pdata *pdata = &madera->pdata;
	struct madera_ldo1 *ldo1 = config->driver_data;
	struct device_node *init_node, *dcvdd_node;
	struct regulator_init_data *init_data;

	init_node = of_get_child_by_name(madera->dev->of_node, "ldo1");
	dcvdd_node = of_parse_phandle(madera->dev->of_node, "DCVDD-supply", 0);

	if (init_node) {
		config->of_node = init_node;

		init_data = of_get_regulator_init_data(madera->dev, init_node,
						       desc);

		if (init_data) {
			init_data->consumer_supplies = &ldo1->supply;
			init_data->num_consumer_supplies = 1;

			if (dcvdd_node && dcvdd_node != init_node)
				madera->internal_dcvdd = false;

			pdata->ldo1.init_data = init_data;
		}
	} else if (dcvdd_node) {
		madera->internal_dcvdd = false;
	}

	if (madera->internal_dcvdd) {
		pdata->ldo1.ldoena = of_get_named_gpio(madera->dev->of_node,
							"cirrus,ldoena-gpios",
							0);
		if (pdata->ldo1.ldoena < 0) {
			if (pdata->ldo1.ldoena != -ENOENT)
				dev_warn(madera->dev,
					 "Malformed cirrus,ldoena-gpios ignored: %d\n",
					 pdata->ldo1.ldoena);

			pdata->ldo1.ldoena = 0;
		}
	}

	of_node_put(dcvdd_node);

	return 0;
}

static int madera_ldo1_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	const struct regulator_desc *desc;
	struct regulator_config config = { };
	struct madera_ldo1 *ldo1;
	int ret;

	madera->internal_dcvdd = true;

	ldo1 = devm_kzalloc(&pdev->dev, sizeof(*ldo1), GFP_KERNEL);
	if (!ldo1)
		return -ENOMEM;

	/*
	 * Since the chip usually supplies itself we provide some
	 * default init_data for it.  This will be overridden with
	 * platform data if provided.
	 */
	desc = &madera_ldo1;
	ldo1->init_data = madera_ldo1_default;

	ldo1->init_data.consumer_supplies = &ldo1->supply;
	ldo1->supply.supply = "DCVDD";
	ldo1->supply.dev_name = dev_name(madera->dev);

	config.dev = madera->dev;
	config.driver_data = ldo1;
	config.regmap = madera->regmap;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!dev_get_platdata(madera->dev)) {
			ret = madera_ldo1_of_get_pdata(madera, &config, desc);
			if (ret < 0)
				return ret;
		}
	}

	/* pdata entries default to 0 if not explicitly set. If the
	 * value is 0 this means 'undefined'
	 */
	if (madera->pdata.ldo1.ldoena == 0)
		madera->pdata.ldo1.ldoena = -1;

	if (madera->pdata.ldo1.init_data)
		ldo1->init_data = *madera->pdata.ldo1.init_data;

	config.ena_gpio = madera->pdata.ldo1.ldoena;
	config.ena_gpio_flags = GPIOF_OUT_INIT_LOW;

	config.init_data = &ldo1->init_data;

	/*
	 * LDO1 can only be used to supply DCVDD so if it has no
	 * consumers then DCVDD is supplied externally.
	 */
	if (config.init_data->num_consumer_supplies == 0)
		madera->internal_dcvdd = false;

	ldo1->regulator = devm_regulator_register(&pdev->dev, desc, &config);

	of_node_put(config.of_node);

	if (IS_ERR(ldo1->regulator)) {
		ret = PTR_ERR(ldo1->regulator);
		dev_err(madera->dev, "Failed to register LDO1 supply: %d\n",
			ret);
		return ret;
	}

	platform_set_drvdata(pdev, ldo1);

	return 0;
}

static struct platform_driver madera_ldo1_driver = {
	.probe = madera_ldo1_probe,
	.driver		= {
		.name	= "madera-ldo1",
	},
};

module_platform_driver(madera_ldo1_driver);

MODULE_DESCRIPTION("Madera LDO1 driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:madera-ldo1");
