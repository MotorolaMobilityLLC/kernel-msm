// SPDX-License-Identifier: GPL-2.0+
/*
 * wl2868c, Multi-Output Regulators
 * Copyright (C) 2019  Motorola Mobility LLC,
 *
 * Author: ChengLong, Motorola Mobility LLC,
 */

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include "wl2868c-regulator.h"

static int ldo_chipid = -1;
static int first_init_flag = 0;
static int ldo_driver_id = 0;

enum ldo_device_id {
	WL2868C_ID = 0,
	FAN53870_ID,
	ET5907_ID,
};

enum slg51000_regulators {
	WL2868C_REGULATOR_LDO1 = 0,
	WL2868C_REGULATOR_LDO2,
	WL2868C_REGULATOR_LDO3,
	WL2868C_REGULATOR_LDO4,
	WL2868C_REGULATOR_LDO5,
	WL2868C_REGULATOR_LDO6,
	WL2868C_REGULATOR_LDO7,
	WL2868C_MAX_REGULATORS,
};

struct wl2868c {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_desc *rdesc[WL2868C_MAX_REGULATORS];
	struct regulator_dev *rdev[WL2868C_MAX_REGULATORS];
	int chip_cs_pin;
};

/*
 * struct wl2868c_evt_sta {
 * 	unsigned int sreg;
 * };
 *
 * static const struct wl2868c_evt_sta wl2868c_status_reg = { WL2868C_LDO_EN };
 */

static unsigned int wl2868c_status_reg = WL2868C_LDO_EN;
static const struct regmap_range wl2868c_writeable_ranges[] = {
	regmap_reg_range(WL2868C_CURRENT_LIMITSEL, WL2868C_SEQ_STATUS),
	/* Do not let useless register writeable */
	// regmap_reg_range(WL2868C_CURRENT_LIMITSEL, WL2868C_CURRENT_LIMITSEL),
	// regmap_reg_range(WL2868C_LDO1_VOUT, WL2868C_LDO7_VOUT),
	// regmap_reg_range(WL2868C_LDO_EN, WL2868C_LDO_EN),
};

static const struct regmap_range wl2868c_readable_ranges[] = {
	regmap_reg_range(WL2868C_CHIP_REV, WL2868C_SEQ_STATUS),
};

static const struct regmap_range wl2868c_volatile_ranges[] = {
	regmap_reg_range(WL2868C_CURRENT_LIMITSEL, WL2868C_SEQ_STATUS),
};

static const struct regmap_access_table wl2868c_writeable_table = {
	.yes_ranges   = wl2868c_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl2868c_writeable_ranges),
};

static const struct regmap_access_table wl2868c_readable_table = {
	.yes_ranges   = wl2868c_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl2868c_readable_ranges),
};

static const struct regmap_access_table wl2868c_volatile_table = {
	.yes_ranges   = wl2868c_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl2868c_volatile_ranges),
};

static const struct regmap_config wl2868c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x0F,
	.wr_table = &wl2868c_writeable_table,
	.rd_table = &wl2868c_readable_table,
	.volatile_table = &wl2868c_volatile_table,
};

static int wl2868c_get_error_flags(struct regulator_dev *rdev, unsigned int *flags)
{
	struct wl2868c *chip = rdev_get_drvdata(rdev);
	uint8_t reg_dump[WL2868C_REG_NUM];
	uint8_t reg_idx;
	unsigned int val = 0;

	dev_err(chip->dev, "************ start dump wl2868c register ************\n");
	dev_err(chip->dev, "register 0x00:      chip version\n");
	dev_err(chip->dev, "register 0x01:      LDO CL\n");
	dev_err(chip->dev, "register 0x03~0x09: LDO1~LDO7 OUT Voltage\n");
	dev_err(chip->dev, "register 0x0e:      Bit[6:0] LDO7~LDO1 EN\n");

	for (reg_idx = 0; reg_idx < WL2868C_REG_NUM; reg_idx++) {
		regmap_read(chip->regmap, reg_idx, &val);
		reg_dump[reg_idx] = val;
		dev_err(chip->dev, "Reg[0x%02x] = 0x%x", reg_idx, reg_dump[reg_idx]);
	}
	dev_err(chip->dev, "************ end dump wl2868c register ************\n");

	if (flags != NULL) {
		*flags = 0;
	}

	return 0;
}

static int wl2868c_get_status(struct regulator_dev *rdev)
{
	struct wl2868c *chip = rdev_get_drvdata(rdev);
	int ret, id = rdev_get_id(rdev);
	unsigned int status = 0;

	ret = regulator_is_enabled_regmap(rdev);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read enable register(%d)\n", ret);
		return ret;
	}

	if (!ret)
		return REGULATOR_STATUS_OFF;

	wl2868c_get_error_flags(rdev, NULL);
	if ((ldo_chipid == 0x01) || (ldo_chipid == 0x00))
	{
		wl2868c_status_reg = fan53870_LDO_EN;
	}
	ret = regmap_read(chip->regmap, wl2868c_status_reg, &status);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read status register(%d)\n", ret);
		return ret;
	}

	if (status & (0x01ul << id)) {
		return REGULATOR_STATUS_ON;
	} else {
		return REGULATOR_STATUS_OFF;
	}
}

static const struct regulator_ops wl2868c_regl_ops = {
	.enable  = regulator_enable_regmap,
	.disable = regulator_disable_regmap,

	.is_enabled = regulator_is_enabled_regmap,

	.list_voltage = regulator_list_voltage_linear,
	.map_voltage  = regulator_map_voltage_linear,

	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,

	.get_status = wl2868c_get_status,
	.get_error_flags = wl2868c_get_error_flags,
};

static int wl2868c_of_parse_cb(struct device_node *np, const struct regulator_desc *desc, struct regulator_config *config)
{
	int ena_gpio;

	ena_gpio = of_get_named_gpio(np, "enable-gpios", 0);
	if (gpio_is_valid(ena_gpio))
		config->ena_gpiod = gpio_to_desc(ena_gpio);

	return 0;
}

#define WL2868C_REGL_DESC(_id, _name, _s_name, _min, _step)       \
	[WL2868C_REGULATOR_##_id] = {                             \
		.name = #_name,                                    \
		.supply_name = _s_name,                            \
		.id = WL2868C_REGULATOR_##_id,                    \
		.of_match = of_match_ptr(#_name),                  \
		.of_parse_cb = wl2868c_of_parse_cb,               \
		.ops = &wl2868c_regl_ops,                         \
		.regulators_node = of_match_ptr("regulators"),     \
		.n_voltages = 256,                                 \
		.min_uV = _min,                                    \
		.uV_step = _step,                                  \
		.linear_min_sel = 0,                               \
		.vsel_mask = WL2868C_VSEL_MASK,               \
		.vsel_reg = WL2868C_##_id##_VSEL,                 \
		.enable_reg = WL2868C_LDO_EN,       \
		.enable_mask = BIT(WL2868C_REGULATOR_##_id),     \
		.type = REGULATOR_VOLTAGE,                         \
		.owner = THIS_MODULE,                              \
	}

static struct regulator_desc wl2868c_regls_desc[WL2868C_MAX_REGULATORS] = {
	WL2868C_REGL_DESC(LDO1, ldo1, "vin1", 496000,  8000),
	WL2868C_REGL_DESC(LDO2, ldo2, "vin1", 496000,  8000),
	WL2868C_REGL_DESC(LDO3, ldo3, "vin2", 1504000, 8000),
	WL2868C_REGL_DESC(LDO4, ldo4, "vin2", 1504000, 8000),
	WL2868C_REGL_DESC(LDO5, ldo5, "vin2", 1504000,  8000),
	WL2868C_REGL_DESC(LDO6, ldo6, "vin2", 1504000,  8000),
	WL2868C_REGL_DESC(LDO7, ldo7, "vin2", 1504000, 8000),
};

static int wl2868c_regulator_init(struct wl2868c *chip)
{
	struct regulator_config config = { };
	struct regulator_desc *rdesc;
	u8 vsel_range[1];
	int id, ret = 0;

	unsigned int ldo_regs[WL2868C_MAX_REGULATORS] = {
		WL2868C_LDO1_VOUT,
		WL2868C_LDO2_VOUT,
		WL2868C_LDO3_VOUT,
		WL2868C_LDO4_VOUT,
		WL2868C_LDO5_VOUT,
		WL2868C_LDO6_VOUT,
		WL2868C_LDO7_VOUT,
	};

	unsigned int initial_voltage[LDO_DEVICE_MAX][WL2868C_MAX_REGULATORS] = {
		{0x24,//LDO1 1.05V
		0x5D,//LDO2 1.24V
		0x80,//LDO3 2.8V
		0x80,//LDO4 2.8V
		0x80,//LDO5 2.8V
		0xA2,//LDO6 3.0V
		0x30,//LDO7 1.8V
		},//wl2868c init voltage
		{0x24,//LDO1 1.05V
		0x9A,//LDO2 1.24V
		0x80,//LDO3 2.8V
		0x80,//LDO4 2.8V
		0x80,//LDO5 2.8V
		0xB2,//LDO6 3.0V
		0x30,//LDO7 1.8V
		},//fan53870 init voltage
		{0x24,//LDO1 1.05V
		0x6A,//LDO2 1.24V
		0x80,//LDO3 2.8V
		0x80,//LDO4 2.8V
		0x80,//LDO5 2.8V
		0xA0,//LDO6 3.0V
		0x30,//LDO7 1.8V
		},//et5907 init voltage
	};
	/*Disable all ldo output by default*/
	if (first_init_flag == 1) {
		if (ldo_chipid == 0x33)
		{
			ret = regmap_write(chip->regmap, WL2868C_LDO_EN, 0x80);
		}
		else if((ldo_chipid == 0x01) ||(ldo_chipid == 0x00))
		{
			ret = regmap_write(chip->regmap, fan53870_LDO_EN, 0);
		}

		if (ret < 0) {
			dev_err(chip->dev, "Disable all LDO output failed!!!\n");
			return ret;
		}
	}

	//ET5907 chipid is 0x00,Fan53870 chipid is 0x01.for change min_uV and step by chip
	if (0x01 == ldo_chipid)
	{
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO1].min_uV = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO1].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO2].min_uV = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO2].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO3].min_uV = 1372000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO3].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO4].min_uV = 1372000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO4].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO5].min_uV = 1372000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO5].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO6].min_uV = 1372000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO6].uV_step = 8000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO7].min_uV = 1372000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO7].uV_step = 8000;
	}
	else if (0x00 == ldo_chipid)
	{
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO1].min_uV = 600000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO1].uV_step = 6000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO2].min_uV = 600000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO2].uV_step = 6000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO3].min_uV = 1200000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO3].uV_step = 10000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO4].min_uV = 1200000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO4].uV_step = 10000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO5].min_uV = 1200000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO5].uV_step = 10000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO6].min_uV = 1200000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO6].uV_step = 10000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO7].min_uV = 1200000;
		wl2868c_regls_desc[WL2868C_REGULATOR_LDO7].uV_step = 10000;
	}

	for (id = 0; id < WL2868C_MAX_REGULATORS; id++)
	{
		if ((0x01 == ldo_chipid) || (0x00 == ldo_chipid))
		{
			wl2868c_regls_desc[id].enable_reg  = fan53870_LDO_EN;
			wl2868c_regls_desc[id].enable_mask = BIT(id);
			wl2868c_regls_desc[id].vsel_reg   += fan53870_LDO_OFFSET;
		}
		chip->rdesc[id] = &wl2868c_regls_desc[id];
		rdesc              = chip->rdesc[id];
		config.regmap      = chip->regmap;
		config.dev         = chip->dev;
		config.driver_data = chip;

		ret = regmap_bulk_read(chip->regmap, ldo_regs[id], vsel_range, 1);
		pr_err("wl2868c_regulator_init: LDO%d, ldo_regs=0x%x default value:0x%x", (id+1),ldo_regs[id],vsel_range[0]);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to read the ldo register\n");
			return ret;
		}

		pr_err("wl2868c_regulator_init: enable_reg0x%x, enable_mask:0x%x", wl2868c_regls_desc[id].enable_reg, wl2868c_regls_desc[id].enable_mask);

		ret = regmap_write(chip->regmap, ldo_regs[id], initial_voltage[ldo_driver_id][id]);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to write inital voltage register\n");
			return ret;
		}
		pr_err("wl2868c_regulator_init: ldo_driver_id=%d LDO%d, initial value:0x%x", ldo_driver_id, (id+1), initial_voltage[ldo_driver_id][id]);

		chip->rdev[id] = devm_regulator_register(chip->dev, rdesc, &config);
		if (IS_ERR(chip->rdev[id])) {
			ret = PTR_ERR(chip->rdev[id]);
			dev_err(chip->dev, "Failed to register regulator(%s):%d\n", chip->rdesc[id]->name, ret);
			return ret;
		}
	}

	return 0;
}

static int wl2868c_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct wl2868c *chip;
	int error, cs_gpio, ret;
	chip = devm_kzalloc(dev, sizeof(struct wl2868c), GFP_KERNEL);
	if (!chip) {
		dev_err(chip->dev, "wl2868c_i2c_probe Memory error...\n");
		return -ENOMEM;
	}

	dev_info(chip->dev, "wl2868c_i2c_probe Enter...\n");

	cs_gpio = of_get_named_gpio(dev->of_node, "semi,cs-gpios", 0);
	if (cs_gpio > 0) {
		if (!gpio_is_valid(cs_gpio)) {
			dev_err(dev, "Invalid chip select pin\n");
			return -EPERM;
		}

		ret = devm_gpio_request_one(dev, cs_gpio, GPIOF_OUT_INIT_HIGH, "wl2868c_cs_pin");
		if (ret) {
			dev_err(dev, "GPIO(%d) request failed(%d)\n", cs_gpio, ret);
			return ret;
		}

		chip->chip_cs_pin = cs_gpio;
	}

	dev_info(chip->dev, "wl2868c_i2c_probe cs_gpio:%d...\n", cs_gpio);

	mdelay(10);

	i2c_set_clientdata(client, chip);
	chip->dev    = dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl2868c_regmap_config);

	regmap_read(chip->regmap, WL2868C_CHIP_REV, &ldo_chipid);
	dev_info(chip->dev, "IIC addr= 0x%x chipid=:0x%x\n", client->addr,ldo_chipid);
	//if regmap fail use a new iic addr try again.
	if ((ldo_chipid != 0x33)&&(ldo_chipid != 0x01)&& (ldo_chipid != 0x00)) {
		error = PTR_ERR(chip->regmap);
		dev_info(chip->dev, "retry iic init\n");
		client->addr = fan53870_ET5907_ADDR;
		i2c_set_clientdata(client, chip);
		chip->dev    = dev;
		chip->regmap = devm_regmap_init_i2c(client, &wl2868c_regmap_config);
		regmap_read(chip->regmap, WL2868C_CHIP_REV, &ldo_chipid);
		if (IS_ERR(chip->regmap)) {
			error = PTR_ERR(chip->regmap);
			dev_err(dev, "Failed to allocate register map: %d\n",
			error);
			return error;
		}
	}

	ret = regmap_read(chip->regmap, WL2868C_CHIP_REV, &ldo_chipid);
	if (0x33 == ldo_chipid)
	{
		ldo_driver_id = WL2868C_ID;
		dev_info(chip->dev, "wl2868c chipid=:0x%x\n", ldo_chipid);
	}
	else if (0x01 == ldo_chipid)
	{
		ldo_driver_id = FAN53870_ID;
		dev_info(chip->dev, "fan53870 chipid=:0x%x\n", ldo_chipid);
	}
	else if (0x00 == ldo_chipid)
	{
		ldo_driver_id = ET5907_ID;
		dev_info(chip->dev, "et5907 chipid=:0x%x\n", ldo_chipid);
	}
	else
	{
		dev_err(chip->dev, "ldo read chipid error...chipid=:0x%x\n",ldo_chipid);
		return -ENOMEM;
	}

	{
		int chip_rev = 0;
		ret = regmap_bulk_read(chip->regmap, 0x00, &chip_rev, 1);

		printk("wl2868c chip rev: %02x\n", chip_rev);
	}

	ret = wl2868c_regulator_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to init regulator(%d)\n", ret);
		return ret;
	}
	dev_info(chip->dev, "wl2868c_i2c_probe Exit...\n");
	first_init_flag = 1;

	return ret;
}

static int wl2868c_i2c_remove(struct i2c_client *client)
{
	struct wl2868c *chip = i2c_get_clientdata(client);
	struct gpio_desc *desc;
	int ret = 0;

	if (chip->chip_cs_pin > 0) {
		desc = gpio_to_desc(chip->chip_cs_pin);
		ret = gpiod_direction_output_raw(desc, GPIOF_INIT_LOW);
	}

	return ret;
}

static const struct i2c_device_id wl2868c_i2c_id[] = {
	{"wl2868c", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, wl2868c_i2c_id);

static struct i2c_driver wl2868c_regulator_driver = {
	.driver = {
		.name = "wl2868c-regulator",
	},
	.probe = wl2868c_i2c_probe,
	.remove = wl2868c_i2c_remove,
	.id_table = wl2868c_i2c_id,
};

module_i2c_driver(wl2868c_regulator_driver);

MODULE_AUTHOR("ChengLong <chengl1@motorola.com>");
MODULE_DESCRIPTION("wl2868c regulator driver");
MODULE_LICENSE("GPL");
