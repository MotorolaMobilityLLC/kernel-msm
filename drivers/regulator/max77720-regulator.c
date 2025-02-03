/*
 * MAX77720 voltage regulator Driver
 *
 * Copyright (c) 2024 Analog Devices, Inc.
 *
 *
 * MAX77720 PMIC Linux Driver is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * MAX77720 PMIC Linux Driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MAX77720 PMIC Linux Driver. If not, see http://www.gnu.org/licenses/.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/util_macros.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define MAX77720_REG_STAT_GLBL			0x3
#define MAX77720_REG_CNFG_GLBL			0x5
#define MAX77720_REG_CNFG_DCDC0			0x30
#define MAX77720_REG_CNFG_DCDC1			0x31
#define MAX77720_REG_CNFG_DCDC2			0x32
#define MAX77720_REG_CNFG_DLY0			0x40
#define MAX77720_REG_CNFG_DLY1			0x41

/* CNFG_GLBL*/
#define MAX77720_BIT_EN_BIAS			BIT(3)
#define MAX77720_BIT_FRC_IBB_ON			BIT(2)
#define MAX77720_BIT_FRC_BST_ON			BIT(1)
#define MAX77720_BIT_FRC_DIS			BIT(0)
#define MAX77720_IBB_EN_MASK			0x0E

/* CNFG_DCDC0 */
#define MAX77720_BIT_RNG_IBB			BIT(6)
#define MAX77720_BIT_SS_IBB			BIT(5)
#define MAX77720_BIT_ADE_IBB			BIT(4)
#define MAX77720_BITS_IPK_BST	        GENMASK(2, 1)
#define MAX77720_BIT_ADE_BST			BIT(0)

/* CNFG_DCDC1 */
#define MAX77720_BIT_VOUT_IBB			BIT(8)

/* CNFG_DLY */
#define MAX77720_BITS_UP_DLY_IBB        GENMASK(7, 4)
#define MAX77720_BITS_DN_DLY_IBB        GENMASK(3, 0)
#define MAX77720_VOUT_MASK			0x1FF
#define MAX77720_IBB_N_VOLTAGE			0x1D2
#define MAX77720_VOUT_SEL_OFFSET		(0x1FF - 0x1D2)

/* max77720 data */
struct max77720_data {
	struct device *dev;
	struct regulator_init_data *reg_init_data;
	struct regulator_desc desc;
	bool enable_external_control;
	unsigned int cnfg_glbl_flags;
};

static bool is_volatile_reg(struct device *dev, unsigned int reg)
{

	switch (reg) {
	case 0x01:
	case 0x03:
	case 0x04:
		return true;
	default:
		return false;
	}
}

static bool is_read_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x01 ... 0x05:
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x40:
	case 0x41:
		return true;
	default:
		return false;
	}
}

static bool is_write_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x01:
	case 0x03:
	case 0x04:
		return false;
	default:
		return true;
	}
}

static const struct regmap_config max77720_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg	= is_write_reg,
	.readable_reg	= is_read_reg,
	.volatile_reg	= is_volatile_reg,
	.max_register = 0x42,
	.cache_type = REGCACHE_RBTREE,
};

static int max77720_get_voltage_sel(struct regulator_dev *rdev)
{
	unsigned int data;
	int ret;

	ret = regmap_read(rdev->regmap, MAX77720_REG_CNFG_DCDC1, &data);
	if (ret < 0)
		return ret;

	if (data & 0x1) {
		ret = regmap_read(rdev->regmap, MAX77720_REG_CNFG_DCDC2, &data);
		data |= MAX77720_BIT_VOUT_IBB;
	} else {
		ret = regmap_read(rdev->regmap, MAX77720_REG_CNFG_DCDC2, &data);
	}
	if (ret < 0)
		return ret;

	data -= MAX77720_VOUT_SEL_OFFSET;
	data = MAX77720_IBB_N_VOLTAGE - data;
	return data & MAX77720_VOUT_MASK;
}

static int max77720_set_voltage_sel(struct regulator_dev *rdev,
		unsigned int vsel)
{
	int ret;

	vsel = MAX77720_IBB_N_VOLTAGE - vsel; //invert value
	vsel += MAX77720_VOUT_SEL_OFFSET;
	ret = regmap_write(rdev->regmap, MAX77720_REG_CNFG_DCDC1,
			((vsel & MAX77720_BIT_VOUT_IBB) >> 8));
	if (ret < 0)
		return ret;

	ret = regmap_write(rdev->regmap, MAX77720_REG_CNFG_DCDC2, vsel & 0xff);
	if (ret < 0)
		return ret;
	return 0;
}

static const struct regulator_ops max77720_ibb_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = max77720_get_voltage_sel,
	.set_voltage_sel = max77720_set_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_active_discharge = regulator_set_active_discharge_regmap,
};

static const struct regulator_ops max77720_base_ibb_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = max77720_get_voltage_sel,
	.set_voltage_sel = max77720_set_voltage_sel,
	.set_active_discharge = regulator_set_active_discharge_regmap,
};

static const struct regulator_desc max77720_low_regulators_desc = {
	.name = "max77720",
	.ops = &max77720_ibb_ops,
	.type = REGULATOR_VOLTAGE,
	.vsel_reg = MAX77720_REG_CNFG_DCDC2,
	.vsel_mask = MAX77720_VOUT_MASK,
	.n_voltages = MAX77720_IBB_N_VOLTAGE,
	.min_uV = 17010000,
	.uV_step = 15000,
	.active_discharge_reg = MAX77720_REG_CNFG_DCDC0,
	.active_discharge_mask = MAX77720_BIT_ADE_IBB,
	.active_discharge_off = 0x0,
	.active_discharge_on = 0x1,
	.owner = THIS_MODULE,
};

static int max77720_regulator_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct regulator_init_data *ridata;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct max77720_data *pdata;
	struct device_node *np = dev->of_node;
	struct gpio_desc *gpiod;
	enum gpiod_flags gflags;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check fail.\n");
		return -EOPNOTSUPP;
	}

	if (!np) {
		dev_err(&client->dev, "No Platform data");
		return -EIO;
	}

	pdata = devm_kzalloc(&client->dev,
			sizeof(struct max77720_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = dev;
	config.dev = dev;
	config.of_node = dev->of_node;
	config.regmap = devm_regmap_init_i2c(client, &max77720_regmap_config);
	if (IS_ERR(config.regmap)) {
		dev_err(dev, "Failed to init regmap");
		return PTR_ERR(config.regmap);
	}

	pdata->reg_init_data = of_get_regulator_init_data(&client->dev,
			client->dev.of_node, &pdata->desc);

	ridata = pdata->reg_init_data;

	pdata->enable_external_control = of_property_read_bool(np,
			"adi,externally-enable");

	if (of_property_read_bool(np, "adi,bias-force-en"))
		pdata->cnfg_glbl_flags  |= MAX77720_BIT_EN_BIAS;

	if (of_property_read_bool(np, "adi,force-ibb-en"))
		pdata->cnfg_glbl_flags  |= MAX77720_BIT_FRC_IBB_ON;

	if (of_property_read_bool(np, "adi,force-bst-en"))
		pdata->cnfg_glbl_flags  |= MAX77720_BIT_FRC_BST_ON;


	config.init_data = pdata->reg_init_data;
	config.driver_data = pdata;

	i2c_set_clientdata(client, pdata);
	pdata->desc.name = "max77720";
	pdata->desc.id = 0;
	pdata->desc.type = REGULATOR_VOLTAGE;
	pdata->desc.owner = THIS_MODULE;
	pdata->desc.vsel_reg = MAX77720_REG_CNFG_DCDC2,
	pdata->desc.vsel_mask = MAX77720_VOUT_MASK,
	pdata->desc.n_voltages = MAX77720_IBB_N_VOLTAGE,
	pdata->desc.min_uV = max77720_low_regulators_desc.min_uV;
	pdata->desc.uV_step = max77720_low_regulators_desc.uV_step;
	pdata->desc.n_voltages = max77720_low_regulators_desc.n_voltages;
	pdata->desc.active_discharge_reg = MAX77720_REG_CNFG_DCDC0;
	pdata->desc.active_discharge_mask = MAX77720_BIT_ADE_IBB | MAX77720_BIT_ADE_BST;
	pdata->desc.active_discharge_off = 0x0;
	pdata->desc.active_discharge_on = MAX77720_BIT_ADE_BST | MAX77720_BIT_ADE_BST;

	if (ridata && (ridata->constraints.boot_on || ridata->constraints.always_on))
		gflags = GPIOD_OUT_HIGH;
	else
		gflags = GPIOD_OUT_LOW;

	gflags |= GPIOD_FLAGS_BIT_NONEXCLUSIVE;
	gpiod = devm_gpiod_get_optional(&client->dev, "adi,enable-gpio", gflags);
	if (!IS_ERR(gpiod)) {
		if (gpiod)
			config.ena_gpiod = gpiod;
	}

	if (config.ena_gpiod) {
		if (!pdata->enable_external_control) {
			//no need external gpio control so put it low
			gpiod_set_value_cansleep(config.ena_gpiod, 0);
			config.ena_gpiod = NULL;
		} else

			/*
			 * Register the regulators
			 * Turn the GPIO descriptor over to the regulator core for
			 * lifecycle management if we pass an ena_gpiod.
			 */
			devm_gpiod_unhinge(&client->dev, config.ena_gpiod);
	} else {
		pdata->enable_external_control = false;  //no gpio resource
	}
	if (pdata->enable_external_control) {
		pdata->desc.ops = &max77720_base_ibb_ops;

		if (ridata && (ridata->constraints.always_on))
			config.ena_gpiod = NULL;// always_on no need pass to regulator core
	} else {
		if (ridata && (ridata->constraints.boot_on || ridata->constraints.always_on))
			pdata->cnfg_glbl_flags = MAX77720_IBB_EN_MASK;
		if (ridata && ridata->constraints.always_on)
			pdata->desc.ops = &max77720_base_ibb_ops;
		else {
			pdata->desc.ops = &max77720_ibb_ops;
			pdata->desc.enable_reg = MAX77720_REG_CNFG_GLBL;
			pdata->desc.enable_mask = MAX77720_IBB_EN_MASK;
			pdata->desc.enable_val = MAX77720_IBB_EN_MASK;
			pdata->desc.disable_val = 0;
		}
	}

	//by EN or force enable
	ret = regmap_write(config.regmap, MAX77720_REG_CNFG_GLBL, pdata->cnfg_glbl_flags);
	if (ret < 0) {
		dev_err(dev, "register %d write failed, err = %d",
				MAX77720_REG_CNFG_GLBL, ret);
		return ret;
	}
	rdev = devm_regulator_register(dev, &pdata->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(dev, "Failed to register regulator MAX77720");
		return PTR_ERR(rdev);
	}

	return 0;
}

static const struct of_device_id __maybe_unused max77720_of_match[] = {
	{ .compatible = "adi,max77720" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, max77720_of_match);

static const struct i2c_device_id max77720_regulator_id[] = {
	{"max77720-regulator"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(i2c, max77720_regulator_id);

static struct i2c_driver max77720_regulator_driver = {
	.driver = {
		.name = "max77720",
		.of_match_table = of_match_ptr(max77720_of_match),
	},
	.probe_new = max77720_regulator_probe,
	.id_table = max77720_regulator_id,
};

module_i2c_driver(max77720_regulator_driver);

MODULE_DESCRIPTION("MAX77720 regulator driver");
MODULE_LICENSE("GPL");
