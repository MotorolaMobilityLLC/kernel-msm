/*
 * Copyright (c) 2015 LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>

struct dw8768_regulator {
	struct device			*dev;
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	struct regulator_init_data	*init_data;
	struct regmap 			*i2c_regmap;
	struct device_node		*reg_node;
	int				en_gpio;
	bool				is_enabled;
};

#define DW8768_VOLTAGE_MIN		4000000
#define DW8768_VOLTAGE_MAX		6000000
#define DW8768_VOLTAGE_STEP		100000
#define DW8768_VOLTAGE_LEVELS	\
		((DW8768_VOLTAGE_MAX - DW8768_VOLTAGE_MIN) \
		 / DW8768_VOLTAGE_STEP + 1)

static struct of_regulator_match dw8768_reg_matches[] = {
	{ .name = "dw8768-dsv", .driver_data = (void *)0 },
};

static int dw8768_regulator_enable(struct regulator_dev *rdev)
{
	struct dw8768_regulator *reg_data =
					rdev_get_drvdata(rdev);
	int rc;

	if (gpio_is_valid(reg_data->en_gpio)) {
		gpio_set_value(reg_data->en_gpio, 1);
		rc = regmap_write(rdev->regmap, 0x05, 0x0F);
		reg_data->is_enabled = true;
	} else {
		pr_err("enable gpio is not configured\n");
		return -EINVAL;
	}

	pr_debug("enabled\n");

	return 0;
}

static int dw8768_regulator_disable(struct regulator_dev *rdev)
{
	struct dw8768_regulator *reg_data = rdev_get_drvdata(rdev);
	int rc;

	if (gpio_is_valid(reg_data->en_gpio)) {
		gpio_set_value(reg_data->en_gpio, 0);
		rc = regmap_write(rdev->regmap, 0x05, 0x07);
		reg_data->is_enabled = false;
	} else {
		pr_err("enable gpio is not configured\n");
		return -EINVAL;
	}

	pr_debug("disabled\n");

	return 0;
}

static int dw8768_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct dw8768_regulator *reg_data = rdev_get_drvdata(rdev);

	return reg_data->is_enabled ? 1 : 0;
}

static int dw8768_regulator_set_voltage(struct regulator_dev *rdev,
			int min_uV, int max_uV, unsigned *selector)
{
	return 0;
}

static int dw8768_regulator_get_voltage(struct regulator_dev *rdev)
{
	return 0;
}

static int dw8768_regulator_list_voltage(struct regulator_dev *rdev,
			unsigned selector)
{
	if (selector >= DW8768_VOLTAGE_LEVELS)
		return 0;
	return selector * DW8768_VOLTAGE_STEP + DW8768_VOLTAGE_MIN;
}

static struct regulator_ops dw8768_ops = {
	.set_voltage = dw8768_regulator_set_voltage,
	.get_voltage = dw8768_regulator_get_voltage,
	.list_voltage = dw8768_regulator_list_voltage,
	.enable = dw8768_regulator_enable,
	.disable = dw8768_regulator_disable,
	.is_enabled = dw8768_regulator_is_enabled,
};

static int dw8768_parse_dt(struct dw8768_regulator *reg_data,
					struct i2c_client *client)
{
	struct device_node *node;
	struct regulation_constraints *constraints;
	int rc;

	if (!client->dev.of_node) {
		pr_err("Can't find device tree\n");
		return -ENOTSUPP;
	}
	node = of_find_node_by_name(client->dev.of_node,
						"regulators");
	if (!node) {
		pr_err("Can't find regulator node\n");
		return -EINVAL;
	}

	rc = of_regulator_match(&client->dev, node,
					dw8768_reg_matches,
					ARRAY_SIZE(dw8768_reg_matches));
	if (IS_ERR_VALUE(rc)) {
		pr_err("Can't match regulator. rc=%d\n", rc);
		return rc;
	}

	if (!dw8768_reg_matches[0].init_data) {
		pr_err("Failed to match regulator\n");
		return -EINVAL;
	}
	constraints = &dw8768_reg_matches[0].init_data->constraints;
	if (!constraints) {
		pr_err("Failed to init constraints\n");
		return -EINVAL;
	}
	constraints->input_uV = constraints->max_uV;
	constraints->valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS;

	reg_data->init_data = dw8768_reg_matches[0].init_data;
	reg_data->reg_node = dw8768_reg_matches[0].of_node;
	reg_data->rdesc.name = constraints->name;

	reg_data->en_gpio = of_get_named_gpio(dw8768_reg_matches[0].of_node,
						"dw,enable-gpio", 0);
	if (!gpio_is_valid(reg_data->en_gpio)) {
		pr_err("enable-gpio not specified. rd=%d\n",
						reg_data->en_gpio);
		return -EINVAL;
	}

	rc = of_property_read_u32(dw8768_reg_matches[0].of_node,
					"dw,power-on-delay-us",
					&reg_data->rdesc.enable_time);
	if (IS_ERR_VALUE(rc)) {
		pr_err("Can't read power on delay. rc=%d\n", rc);
		reg_data->rdesc.enable_time = 3700;
	}
	return rc;
}

static struct regmap_config dw8768_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int dw8768_regulator_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct dw8768_regulator *reg_data;
	struct regulator_config config = {};
	struct regulator_desc *rdesc;
	int rc;

	reg_data = devm_kzalloc(&client->dev,
				sizeof(struct dw8768_regulator),
				GFP_KERNEL);
	if (!reg_data) {
		pr_err("Can't allocate dw8768_regulator memory\n");
		return -ENOMEM;
	}

	rc = dw8768_parse_dt(reg_data, client);
	if (rc) {
		pr_err("Failed to parse device tree. rc=%d\n", rc);
		goto out;
	}

	rc = gpio_request_one(reg_data->en_gpio,
				GPIOF_OUT_INIT_HIGH, "dw8768_enable");
	if (rc) {
		pr_err("Failed to request enanble gpio. rc=%d\n", rc);
		goto out;
	}

	reg_data->i2c_regmap = devm_regmap_init_i2c(client,
						&dw8768_i2c_regmap);
	if (IS_ERR(reg_data->i2c_regmap)) {
		rc = PTR_ERR(reg_data->i2c_regmap);
		pr_err("Failed to init i2c. rc=%d\n", rc);
		goto error;
	}
	i2c_set_clientdata(client, reg_data);

	reg_data->dev = &client->dev;

	config.dev = &client->dev;
	config.init_data = reg_data->init_data;
	config.driver_data = reg_data;
	config.of_node = reg_data->reg_node;
	config.regmap = reg_data->i2c_regmap;

	rdesc = &reg_data->rdesc;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->owner = THIS_MODULE;
	rdesc->n_voltages = DW8768_VOLTAGE_LEVELS;
	rdesc->ops = &dw8768_ops;

	reg_data->rdev = regulator_register(rdesc, &config);
	if (IS_ERR(reg_data->rdev)) {
		rc = PTR_ERR(reg_data->rdev);
		pr_err("Failed to register regulator. rc=%d\n", rc);
		goto error;
	}

	return 0;

error:
	if(reg_data->en_gpio)
		gpio_free(reg_data->en_gpio);
out:
	return rc;
}

static int dw8768_regulator_remove(struct i2c_client *client)
{
	struct dw8768_regulator *reg_data =
				i2c_get_clientdata(client);

	regulator_unregister(reg_data->rdev);
	if(reg_data->en_gpio)
		gpio_free(reg_data->en_gpio);

	return 0;
}

static struct of_device_id dw8768_match_table[] = {
	{ .compatible = "dw,dw8768", },
	{},
};

static const struct i2c_device_id dw8768_id[] = {
	{ "dw8768", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, dw8768_id);

static struct i2c_driver dw8768_regulator_driver = {
	.driver = {
		.name = "dw8768",
		.owner = THIS_MODULE,
		.of_match_table = dw8768_match_table,
	},
	.probe = dw8768_regulator_probe,
	.remove = dw8768_regulator_remove,
	.id_table = dw8768_id,
};

static int __init dw8768_init(void)
{
	return i2c_add_driver(&dw8768_regulator_driver);
}
subsys_initcall(dw8768_init);

static void __exit dw8768_exit(void)
{
	i2c_del_driver(&dw8768_regulator_driver);
}
module_exit(dw8768_exit);

MODULE_DESCRIPTION("DONGWOON DW8768 regulator driver");
MODULE_LICENSE("GPL v2");
