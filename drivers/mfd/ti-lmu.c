/*
 * TI LMU(Lighting Management Unit) Core Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LMU MFD supports LM3532, LM3631, LM3633, LM3695 and LM3697.
 *
 *   LM3532: backlight + light effect
 *   LM3631: backlight + light effect + regulators
 *   LM3633: backlight + light effect + LED indicators
 *   LM3695: backlight
 *   LM3697: backlight + light effect
 *
 * Those devices have common features as below.
 *
 *   - I2C interface for accessing device registers
 *   - Hardware enable pin control
 *   - Backlight brightness control with current settings
 *   - Light effect driver for backlight and LED patterns
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-effect.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

#define LMU_IMAX_OFFSET		6

static const struct resource lm3532_effect_resources[] = {
	{
		.name  = LM3532_EFFECT_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3532_EFFECT_REGISTER(RAMPUP),
	},
	{
		.name  = LM3532_EFFECT_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3532_EFFECT_REGISTER(RAMPDN),
	},
};

static struct mfd_cell lm3532_devices[] = {
	{
		.name          = "lm3532-backlight",
		.of_compatible = "ti,lmu-backlight",
	},
	{
		.name          = "lmu-effect",
		.id            = LM3532,
		.resources     = lm3532_effect_resources,
		.num_resources = ARRAY_SIZE(lm3532_effect_resources),
	},
};

static const struct resource lm3631_effect_resources[] = {
	{
		.name  = LM3631_EFFECT_SLOPE,
		.flags = IORESOURCE_REG,
		.start = LM3631_EFFECT_REGISTER(SLOPE),
	},
};

#define LM3631_REGULATOR(_id)			\
{						\
	.name          = "lm3631-regulator",	\
	.id            = _id,			\
	.of_compatible = "ti,lm3631-regulator",	\
}						\

static struct mfd_cell lm3631_devices[] = {
	LM3631_REGULATOR(0),
	LM3631_REGULATOR(1),
	LM3631_REGULATOR(2),
	LM3631_REGULATOR(3),
	LM3631_REGULATOR(4),
	{
		.name          = "lm3631-backlight",
		.of_compatible = "ti,lmu-backlight",
	},
	{
		.name          = "lmu-effect",
		.id            = LM3631,
		.resources     = lm3631_effect_resources,
		.num_resources = ARRAY_SIZE(lm3631_effect_resources),
	},
};

static const struct resource lm3633_effect_resources[] = {
	{
		.name  = LM3633_EFFECT_BL0_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(BL0_RAMPUP),
	},
	{
		.name  = LM3633_EFFECT_BL0_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(BL0_RAMPDN),
	},
	{
		.name  = LM3633_EFFECT_BL1_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(BL1_RAMPUP),
	},
	{
		.name  = LM3633_EFFECT_BL1_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(BL1_RAMPDN),
	},
	{
		.name  = LM3633_EFFECT_PTN_DELAY,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(DELAY),
	},
	{
		.name  = LM3633_EFFECT_PTN_HIGHTIME,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(HIGHTIME),
	},
	{
		.name  = LM3633_EFFECT_PTN_LOWTIME,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(LOWTIME),
	},
	{
		.name  = LM3633_EFFECT_PTN0_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(PTN0_RAMPUP),
	},
	{
		.name  = LM3633_EFFECT_PTN0_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(PTN0_RAMPDN),
	},
	{
		.name  = LM3633_EFFECT_PTN1_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(PTN1_RAMPUP),
	},
	{
		.name  = LM3633_EFFECT_PTN1_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(PTN1_RAMPDN),
	},
	{
		.name  = LM3633_EFFECT_PTN_LOWBRT,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(LOWBRT),
	},
	{
		.name  = LM3633_EFFECT_PTN_HIGHBRT,
		.flags = IORESOURCE_REG,
		.start = LM3633_EFFECT_REGISTER(HIGHBRT),
	},
};

static struct mfd_cell lm3633_devices[] = {
	{
		.name          = "lm3633-backlight",
		.of_compatible = "ti,lmu-backlight",
	},
	{
		.name          = "lm3633-leds",
		.of_compatible = "ti,lm3633-leds",
	},
	{
		.name          = "lmu-effect",
		.id            = LM3633,
		.resources     = lm3633_effect_resources,
		.num_resources = ARRAY_SIZE(lm3633_effect_resources),
	},
};

static struct mfd_cell lm3695_devices[] = {
	{
		.name          = "lm3695-backlight",
		.of_compatible = "ti,lmu-backlight",
	},
};

static const struct resource lm3697_effect_resources[] = {
	{
		.name  = LM3697_EFFECT_BL0_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3697_EFFECT_REGISTER(BL0_RAMPUP),
	},
	{
		.name  = LM3697_EFFECT_BL0_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3697_EFFECT_REGISTER(BL0_RAMPDN),
	},
	{
		.name  = LM3697_EFFECT_BL1_RAMPUP,
		.flags = IORESOURCE_REG,
		.start = LM3697_EFFECT_REGISTER(BL1_RAMPUP),
	},
	{
		.name  = LM3697_EFFECT_BL1_RAMPDOWN,
		.flags = IORESOURCE_REG,
		.start = LM3697_EFFECT_REGISTER(BL1_RAMPDN),
	},
};

static struct mfd_cell lm3697_devices[] = {
	{
		.name          = "lm3697-backlight",
		.of_compatible = "ti,lmu-backlight",
	},
	{
		.name          = "lmu-effect",
		.id            = LM3697,
		.resources     = lm3697_effect_resources,
		.num_resources = ARRAY_SIZE(lm3697_effect_resources),
	},
};

static struct mfd_cell *ti_lmu_cell[] = {
	lm3532_devices,
	lm3631_devices,
	lm3633_devices,
	lm3695_devices,
	lm3697_devices,
};

static const int ti_lmu_num_cells[] = {
	ARRAY_SIZE(lm3532_devices),
	ARRAY_SIZE(lm3631_devices),
	ARRAY_SIZE(lm3633_devices),
	ARRAY_SIZE(lm3695_devices),
	ARRAY_SIZE(lm3697_devices),
};

int ti_lmu_read_byte(struct ti_lmu *lmu, u8 reg, u8 *read)
{
	int ret;
	unsigned int val;

	ret = regmap_read(lmu->regmap, reg, &val);
	if (ret < 0)
		return ret;

	*read = (u8)val;
	return 0;
}
EXPORT_SYMBOL_GPL(ti_lmu_read_byte);

int ti_lmu_write_byte(struct ti_lmu *lmu, u8 reg, u8 data)
{
	return regmap_write(lmu->regmap, reg, data);
}
EXPORT_SYMBOL_GPL(ti_lmu_write_byte);

int ti_lmu_update_bits(struct ti_lmu *lmu, u8 reg, u8 mask, u8 data)
{
	if (mask == LMU_FULL_MASKBIT)
		return regmap_write(lmu->regmap, reg, data);
	else
		return regmap_update_bits(lmu->regmap, reg, mask, data);
}
EXPORT_SYMBOL_GPL(ti_lmu_update_bits);

enum ti_lmu_max_current ti_lmu_get_current_code(u8 imax_mA)
{
	const enum ti_lmu_max_current imax_table[] = {
		LMU_IMAX_6mA,  LMU_IMAX_7mA,  LMU_IMAX_8mA,  LMU_IMAX_9mA,
		LMU_IMAX_10mA, LMU_IMAX_11mA, LMU_IMAX_12mA, LMU_IMAX_13mA,
		LMU_IMAX_14mA, LMU_IMAX_15mA, LMU_IMAX_16mA, LMU_IMAX_17mA,
		LMU_IMAX_18mA, LMU_IMAX_19mA, LMU_IMAX_20mA, LMU_IMAX_21mA,
		LMU_IMAX_22mA, LMU_IMAX_23mA, LMU_IMAX_24mA, LMU_IMAX_25mA,
		LMU_IMAX_26mA, LMU_IMAX_27mA, LMU_IMAX_28mA, LMU_IMAX_29mA,
	};

	/*
	 * Convert milliampere to appropriate enum code value.
	 * Input range : 5 ~ 30mA
	 */

	if (imax_mA <= 5)
		return LMU_IMAX_5mA;

	if (imax_mA >= 30)
		return LMU_IMAX_30mA;

	return imax_table[imax_mA - LMU_IMAX_OFFSET];
}
EXPORT_SYMBOL_GPL(ti_lmu_get_current_code);

static int ti_lmu_enable_hw(struct ti_lmu *lmu)
{
	int ret;

	ret = devm_gpio_request_one(lmu->dev, lmu->pdata->en_gpio,
				    GPIOF_OUT_INIT_HIGH, "lmu_hwen");
	if (ret)
		return ret;

	/*
	 * LM3631 Powerup Sequence
	 *
	 * 1) Enable nRST pin : GPIO control
	 * 2) Delay about 1ms : bias delay 200us + EPROM read time 700us
	 * 3) Set LCD_EN bit to 1
	 */

	if (lmu->id == LM3631) {
		usleep_range(1000, 1500);

		return ti_lmu_update_bits(lmu, LM3631_REG_DEVCTRL,
					  LM3631_LCD_EN_MASK,
					  1 << LM3631_LCD_EN_SHIFT);
	}

	return 0;
}

static void ti_lmu_disable_hw(struct ti_lmu *lmu)
{
	gpio_set_value(lmu->pdata->en_gpio, 0);
}

static struct regmap_config lmu_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int ti_lmu_parse_dt(struct device *dev, struct ti_lmu *lmu)
{
	struct device_node *node = dev->of_node;
	struct ti_lmu_platform_data *pdata;
	int ret;

	if (!node)
		return -EINVAL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->en_gpio = of_get_named_gpio(node, "ti,enable-gpio", 0);
	if (pdata->en_gpio < 0)
		return pdata->en_gpio;

	/* get regulator  to power the I2C switch */
	pdata->supply_name = NULL;
	ret = of_property_read_string(node, "ti,switch-supply",
		&pdata->supply_name);
	dev_info(dev, "I2C switch supply: %s\n",
		(ret ? "not provided" : pdata->supply_name));

	lmu->pdata = pdata;

	return 0;
}

static int ti_lmu_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct device *dev = &cl->dev;
	struct ti_lmu_platform_data *pdata = dev_get_platdata(dev);
	struct ti_lmu *lmu;
	int ret;

	lmu = devm_kzalloc(dev, sizeof(*lmu), GFP_KERNEL);
	if (!lmu)
		return -ENOMEM;

	lmu->pdata = pdata;
	if (!lmu->pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = ti_lmu_parse_dt(dev, lmu);
		else
			ret = -ENODEV;

		if (ret)
			return ret;
	}

	/* if configured request/enable the regulator for the I2C switch */
	if (lmu->pdata->supply_name) {
		lmu->pdata->vreg = devm_regulator_get(dev,
			lmu->pdata->supply_name);
		if (IS_ERR(lmu->pdata->vreg)) {
			if (PTR_ERR(lmu->pdata->vreg) == -EPROBE_DEFER)
				return -EPROBE_DEFER;
			else
				return PTR_ERR(lmu->pdata->vreg);
		}
		ret = regulator_enable(lmu->pdata->vreg);
		if (ret) {
			dev_err(dev, "regulator enable failed %d\n", ret);
			return ret;
		}
	}

	lmu->id = id->driver_data;
	switch (lmu->id) {
	case LM3532:
		lmu_regmap_config.max_register = LM3532_MAX_REGISTERS;
		break;
	case LM3631:
		lmu_regmap_config.max_register = LM3631_MAX_REGISTERS;
		break;
	case LM3633:
		lmu_regmap_config.max_register = LM3633_MAX_REGISTERS;
		break;
	case LM3695:
		lmu_regmap_config.max_register = LM3695_MAX_REGISTERS;
		break;
	case LM3697:
		lmu_regmap_config.max_register = LM3697_MAX_REGISTERS;
		break;
	default:
		break;
	}

	lmu_regmap_config.name = id->name;
	lmu->regmap = devm_regmap_init_i2c(cl, &lmu_regmap_config);
	if (IS_ERR(lmu->regmap))
		return PTR_ERR(lmu->regmap);

	lmu->dev = &cl->dev;
	i2c_set_clientdata(cl, lmu);

	ret = ti_lmu_enable_hw(lmu);
	if (ret)
		return ret;

	return mfd_add_devices(lmu->dev, 0, ti_lmu_cell[lmu->id],
			       ti_lmu_num_cells[lmu->id], NULL, 0, NULL);
}

static int ti_lmu_remove(struct i2c_client *cl)
{
	struct ti_lmu *lmu = i2c_get_clientdata(cl);

	ti_lmu_disable_hw(lmu);
	mfd_remove_devices(lmu->dev);
	return 0;
}

static const struct i2c_device_id ti_lmu_ids[] = {
	{ "lm3532", LM3532 },
	{ "lm3631", LM3631 },
	{ "lm3633", LM3633 },
	{ "lm3695", LM3695 },
	{ "lm3697", LM3697 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ti_lmu_ids);

#ifdef CONFIG_OF
static const struct of_device_id ti_lmu_of_match[] = {
	{ .compatible = "ti,lm3532", },
	{ .compatible = "ti,lm3631", },
	{ .compatible = "ti,lm3633", },
	{ .compatible = "ti,lm3695", },
	{ .compatible = "ti,lm3697", },
	{ }
};
MODULE_DEVICE_TABLE(of, ti_lmu_of_match);
#endif

static struct i2c_driver ti_lmu_driver = {
	.probe = ti_lmu_probe,
	.remove = ti_lmu_remove,
	.driver = {
		.name = "ti-lmu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ti_lmu_of_match),
	},
	.id_table = ti_lmu_ids,
};
module_i2c_driver(ti_lmu_driver);

MODULE_DESCRIPTION("TI LMU MFD Core Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
