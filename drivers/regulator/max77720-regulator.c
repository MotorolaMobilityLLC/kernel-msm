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
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#if IS_ENABLED(CONFIG_REGULATOR_DEBUG_CONTROL)
#include <linux/regulator/debug-regulator.h>
#endif

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
#define MAX77720_BITS_IPK_BST			GENMASK(2, 1)
#define MAX77720_BIT_ADE_BST			BIT(0)

/* CNFG_DCDC1 */
#define MAX77720_BIT_VOUT_IBB			BIT(8)

/* CNFG_DLY */
#define MAX77720_BITS_UP_DLY_IBB		GENMASK(7, 4)
#define MAX77720_BITS_DN_DLY_IBB		GENMASK(3, 0)
#define MAX77720_VOUT_MASK			0x1FF
#define MAX77720_IBB_N_VOLTAGE			0x1D2
#define MAX77720_VOUT_SEL_OFFSET		(0x1FF - 0x1D2)


#define MAX77720_MASK_CHIP_REV			(0x7 << 0)
#define MAX77720_MASK_VERSION			(0xF << 3)

#define MAX77720_MASK_INT_OCP			(0x1 << 0)
#define MAX77720_MASK_INT_OVP			(0x1 << 1)
#define MAX77720_MASK_INT_POK			(0x1 << 2)
#define MAX77720_MASK_INT_THM			(0x1 << 3)

#define MAX77720_MASK_ST			(0xF)
#define MAX77720_MASK_ST_OCP			(0x1 << 0)
#define MAX77720_MASK_ST_OVP			(0x1 << 1)
#define MAX77720_MASK_ST_POK			(0x1 << 2)
#define MAX77720_MASK_ST_TSHDN			(0x1 << 3)

#define MAX77720_MASK_BB_EN			(0x1 << 6)
#define MAX77720_MASK_PD_EN			(0x1 << 5)
#define MAX77720_MASK_POK_POL			(0x1 << 4)

#define MAX77720_MASK_FPWM			(0x1 << 0)
#define MAX77720_MASK_AD			(0x1 << 0)
#define MAX77720_MASK_OVP_TH			(0x3 << 2)
#define MAX77720_MASK_RD_SR			(0x1 << 4)
#define MAX77720_MASK_RU_SR			(0x1 << 5)
#define MAX77720_MASK_ILIM			(0x3 << 6)

#define MAX77720_MASK_GPIO_CFG			(0x07)
#define MAX77720_MASK_VOUT			(0x7F)

#define MAX77720_VOUT_MIN_UV			11000000
#define MAX77720_VOUT_STEP_UV			20000

#define MAX77720_AD_DISABLE			0


/* MAX77720 Registers */
enum {
	MAX77720_REG_DEVICE_ID,
	MAX77720_REG_STATUS,
	MAX77720_REG_CONFIG1,
	MAX77720_REG_CONFIG2,
	MAX77720_REG_VOUT,
	MAX77720_REG_INT_MASK,
	MAX77720_REG_INT,
};

/* MAX77720 Chip Variation */
enum {
	MAX77720_SUB_A_F = 1,
	MAX77720_SUB_B,
	MAX77720_SUB_C,
	MAX77720_SUB_D,
	MAX77720_SUB_E,
};

/* MAX77720 Regulator Option */
enum {
	MAX77720_ID_VOUT,
	MAX77720_ID_VOUT_H,
	MAX77720_MAX_REGULATORS,
};

#define MAX77720_MASK_CHIP_REV		(0x7 << 0)
#define MAX77720_MASK_VERSION		(0xF << 3)

#define MAX77720_MASK_INT_OCP		(0x1 << 0)
#define MAX77720_MASK_INT_OVP		(0x1 << 1)
#define MAX77720_MASK_INT_POK		(0x1 << 2)
#define MAX77720_MASK_INT_THM		(0x1 << 3)

#define MAX77720_MASK_ST		(0xF)
#define MAX77720_MASK_ST_OCP		(0x1 << 0)
#define MAX77720_MASK_ST_OVP		(0x1 << 1)
#define MAX77720_MASK_ST_POK		(0x1 << 2)
#define MAX77720_MASK_ST_TSHDN		(0x1 << 3)

#define MAX77720_MASK_PD_EN		(0x1 << 5)
#define MAX77720_MASK_POK_POL		(0x1 << 4)

#define MAX77720_MASK_FPWM		(0x1 << 0)
#define MAX77720_MASK_AD		(0x1 << 0)
#define MAX77720_MASK_OVP_TH		(0x3 << 2)
#define MAX77720_MASK_RD_SR		(0x1 << 4)
#define MAX77720_MASK_RU_SR		(0x1 << 5)
#define MAX77720_MASK_ILIM		(0x3 << 6)

#define MAX77720_MASK_GPIO_CFG		(0x07)
#define MAX77720_MASK_VOUT		(0x7F)

#define MAX77720_AD_DISABLE		0


/* max77720 data */
struct max77720_data {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_init_data *reg_init_data;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	u32 pwr_en_gpio;
	u32 driver_en_gpio;
	bool enable_external_control;
	unsigned int cnfg_glbl_flags;
};

static struct max77720_data *pdata_global;

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
	.max_register	= 0x42,
	.cache_type	= REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
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
	} else
		ret = regmap_read(rdev->regmap, MAX77720_REG_CNFG_DCDC2, &data);

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

	return ret;
}

static int max77720_get_status(struct regulator_dev *rdev)
{
	struct max77720_data *pdata = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(pdata->regmap, MAX77720_REG_STATUS, &data);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to read i2c: %d\n", ret);
		return ret;
	}
	ret = (data & MAX77720_MASK_ST);
	dev_dbg(pdata->dev, "get status: data: %d, ret: %d\n", data, ret);

	return ret;
}

static int max77720_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max77720_data *pdata = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(pdata->dev, "set mode: %d", mode);
	if (mode != REGULATOR_MODE_NORMAL) {
		ret = regmap_update_bits(pdata->regmap,
				MAX77720_REG_CONFIG1,
				MAX77720_MASK_FPWM, 0);
	} else
		ret = regmap_update_bits(pdata->regmap,
				MAX77720_REG_CONFIG1,
				MAX77720_MASK_FPWM, MAX77720_MASK_FPWM);
	if (ret < 0)
		dev_err(pdata->dev,
			"failed to update i2c: %d @ function %s\n", ret, __func__);

	return ret;
}

static int max77720_write_regmap(struct regmap *regmap, int addr, int data)
{
	int ret = 0;

	ret = regmap_write(regmap, addr, data);
	if (ret < 0) {
		dev_err(pdata_global->dev,
			"failed to write regmap: addr: 0x%x, data: 0x%x, ret:%d\n",
						addr, data, ret);
		return ret;
	}
	dev_dbg(pdata_global->dev, "success to write regmap: addr: 0x%x, data: 0x%x, ret:%d\n",
						addr, data, ret);
	ret = regmap_read(regmap, addr, &data);
	if (ret < 0)
		dev_err(pdata_global->dev,
			"failed to read regmap: addr: 0x%x, data: 0x%x, ret:%d\n",
						addr, data, ret);
	else
		dev_dbg(pdata_global->dev,
			"success to read regmap: addr: 0x%x, data: 0x%x, ret:%d\n",
						addr, data, ret);

	return ret;
}

int max77720_write_reg(int addr, int data)
{
	if (!pdata_global)
		return -EINVAL;

	return max77720_write_regmap(pdata_global->regmap, addr, data);
}
EXPORT_SYMBOL_GPL(max77720_write_reg);

static unsigned int max77720_get_mode(struct regulator_dev *rdev)
{
	struct max77720_data *pdata = rdev_get_drvdata(rdev);
	unsigned int rval;
	int ret;

	ret = regmap_read(pdata->regmap, MAX77720_REG_CONFIG1, &rval);
	if (ret < 0) {
		dev_err(pdata->dev,
			"failed to read i2c: %d @ function %s\n",
				ret, __func__);
		return ret;
	}

	ret = (rval & MAX77720_MASK_FPWM)
			? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
	dev_dbg(pdata->dev, "get mode: rval: %d, ret: %d\n", rval, ret);

	return ret;
}

static const struct regulator_ops max77720_ibb_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.get_voltage_sel = max77720_get_voltage_sel,
	.set_voltage_sel = max77720_set_voltage_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = max77720_get_status,
	.set_mode = max77720_set_mode,
	.get_mode = max77720_get_mode,
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
	.id = MAX77720_ID_VOUT,
	.ops = &max77720_ibb_ops,
	.type = REGULATOR_VOLTAGE,
	.enable_mask = MAX77720_MASK_BB_EN,
	.enable_reg = MAX77720_REG_CONFIG2,
	.vsel_reg = MAX77720_REG_VOUT,
	.vsel_mask = MAX77720_MASK_VOUT,
	.n_voltages = MAX77720_VOUT_MASK + 1,
	.min_uV = MAX77720_VOUT_MIN_UV,
	.uV_step = MAX77720_VOUT_STEP_UV,
	.active_discharge_reg = MAX77720_REG_CONFIG1,
	.active_discharge_mask = MAX77720_MASK_AD,
	.active_discharge_off = MAX77720_AD_DISABLE,
	.active_discharge_on = MAX77720_MASK_AD,
	.owner = THIS_MODULE,
};

static int max77720_parse_dt(struct max77720_data *pdata)
{
	struct device_node *node = pdata->dev->of_node;

	if (!node) {
		dev_err(pdata->dev, "device tree info missing\n");
		return -EINVAL;
	}

	return 0;
}

static int max77720_init_regulator(struct max77720_data *pdata,
			struct device_node *node)
{
	int ret;
	unsigned int rval;
	struct regulator_config config = { };

	config.regmap = pdata->regmap;
	config.driver_data = pdata;
	config.dev = pdata->dev;
	config.of_node = pdata->dev->of_node;

	ret = regmap_read(pdata->regmap, MAX77720_REG_STATUS, &rval);
	if (ret < 0) {
		dev_err(pdata->dev,
			"failed to read i2c: %d @ function %s\n", ret, __func__);
		return ret;
	}

	switch (rval & MAX77720_MASK_GPIO_CFG) {
	case MAX77720_SUB_A_F:
	case MAX77720_SUB_B:
	case MAX77720_SUB_D:
	case MAX77720_SUB_E:
		dev_dbg(pdata->dev, "MAX77720_SUB_E function [%s] version: %d\n", __func__,
			(rval & MAX77720_MASK_GPIO_CFG));
		pdata->regulator_desc = max77720_low_regulators_desc;
		config.init_data = of_get_regulator_init_data(pdata->dev,
				pdata->dev->of_node,
				&max77720_low_regulators_desc);
		pdata->regulator
			= devm_regulator_register(pdata->dev,
				&max77720_low_regulators_desc, &config);
#if IS_ENABLED(CONFIG_REGULATOR_DEBUG_CONTROL)
		ret = devm_regulator_debug_register(pdata->dev, pdata->regulator);
		if (ret)
			dev_err(pdata->dev,
				"failed to register debug regulator for bob rc=%d\n", ret);
#endif
	break;
	case MAX77720_SUB_C:
		dev_err(pdata->dev, "MAX77720_SUB_C not supported %s\n", __func__);
	break;
	default:
		dev_err(pdata->dev, "MAX77720_default function %s\n", __func__);
	break;
	}

	if (IS_ERR(pdata->regulator)) {
		ret = PTR_ERR(pdata->regulator);
		return ret;
	}

	return 0;
}

static int max77720_regulator_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device_node *node = client->dev.of_node;
	struct device *dev = &client->dev;
	struct max77720_data *pdata;
	int ret = 0;

	pdata_global = NULL;

	pdata = devm_kzalloc(dev, sizeof(struct max77720_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	i2c_set_clientdata(client, pdata);
	pdata->dev = dev;

	ret = max77720_parse_dt(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to parse device tree, ret:%d\n", ret);
		goto fail_parse_dt;
	}

	pdata->regmap = devm_regmap_init_i2c(client, &max77720_regmap_config);
	if (IS_ERR(pdata->regmap)) {
		ret = PTR_ERR(pdata->regmap);
		dev_err(pdata->dev, "failed to initialize regmap: %d\n", ret);
		return ret;
	}

	ret = max77720_init_regulator(pdata, node);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to register regulator: %d\n", ret);
		return ret;
	} else {
		dev_dbg(pdata->dev, "success to register max77720 regulator: %d\n", ret);
		ret = max77720_write_regmap(pdata->regmap, 0x05, 0x0);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x02, 0xFF);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x30, 0x31);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x31, 0x01);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x32, 0x37);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x40, 0x0);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
		ret = max77720_write_regmap(pdata->regmap, 0x41, 0x0);
		if (ret < 0) {
			dev_err(pdata->dev, "failed to write reg map %d\n", ret);
			return ret;
		}
	}

	pdata_global = pdata;

	return 0;

fail_parse_dt:
	i2c_set_clientdata(client, NULL);
	dev_set_drvdata(&client->dev, NULL);

	return ret;
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
	.probe = max77720_regulator_probe,
	.id_table = max77720_regulator_id,
};

module_i2c_driver(max77720_regulator_driver);

MODULE_DESCRIPTION("MAX77720 regulator driver");
MODULE_LICENSE("GPL");
