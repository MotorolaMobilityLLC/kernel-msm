/*
 * TI LM3697 Backlight Driver
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

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/lm3697_bl.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/slab.h>

/* Registers */
#define LM3697_REG_OUTPUT_CFG		0x10

#define LM3697_REG_BOOST_CTRL		0x1A

#define LM3697_REG_PWM_CFG		0x1C

#define LM3697_REG_IMAX_A		0x17
#define LM3697_REG_IMAX_B		0x18

#define LM3697_REG_BRT_A_LSB		0x20
#define LM3697_REG_BRT_A_MSB		0x21
#define LM3697_REG_BRT_B_LSB		0x22
#define LM3697_REG_BRT_B_MSB		0x23
#define LM3697_BRT_LSB_MASK		(BIT(0) | BIT(1) | BIT(2))
#define LM3697_BRT_MSB_SHIFT		3

#define LM3697_REG_ENABLE		0x24

/* Other definitions */
#define LM3697_PWM_ID			1
#define LM3697_MAX_REGISTERS		0xB4
#define LM3697_MAX_STRINGS		3
#define LM3697_MAX_BRIGHTNESS		2047
#define LM3697_IMAX_OFFSET		6
#define LM3697_DEFAULT_NAME		"lcd-backlight"
#define LM3697_DEFAULT_PWM		"lm3697-backlight"

enum lm3697_bl_ctrl_mode {
	BL_REGISTER_BASED,
	BL_PWM_BASED,
	BL_REGISTER_BASED_PWM,
};

/*
 * struct lm3697_bl_chip
 * @dev: Parent device structure
 * @regmap: Used for I2C register access
 * @pdata: LM3697 platform data
 */
struct lm3697_bl_chip {
	struct device *dev;
	struct regmap *regmap;
	struct lm3697_platform_data *pdata;
};

/*
 * struct lm3697_bl
 * @bank_id: Control bank ID. BANK A or BANK A and B
 * @bl_dev: Backlight device structure
 * @chip: LM3697 backlight chip structure for low level access
 * @bl_pdata: LM3697 backlight platform data
 * @mode: Backlight control mode
 * @pwm: PWM device structure. Only valid in PWM control mode
 * @pwm_name: PWM device name
 */
struct lm3697_bl {
	int bank_id;
	struct backlight_device *bl_dev;
	struct lm3697_bl_chip *chip;
	struct lm3697_backlight_platform_data *bl_pdata;
	enum lm3697_bl_ctrl_mode mode;
	struct pwm_device *pwm;
	char pwm_name[20];
};

static int lm3697_bl_write_byte(struct lm3697_bl_chip *chip, u8 reg, u8 data)
{
	return regmap_write(chip->regmap, reg, data);
}

static int lm3697_bl_update_bits(struct lm3697_bl_chip *chip, u8 reg, u8 mask,
				 u8 data)
{
	return regmap_update_bits(chip->regmap, reg, mask, data);
}

static int lm3697_bl_enable(struct lm3697_bl *lm3697_bl, int enable)
{
	return lm3697_bl_update_bits(lm3697_bl->chip, LM3697_REG_ENABLE,
				     BIT(lm3697_bl->bank_id),
				     enable << lm3697_bl->bank_id);
}

static void lm3697_bl_pwm_ctrl(struct lm3697_bl *lm3697_bl, int br, int max_br)
{
	struct pwm_device *pwm;
	unsigned int duty, period;

	/* Request a PWM device with the consumer name */
	if (!lm3697_bl->pwm) {
		pwm = pwm_request(LM3697_PWM_ID, lm3697_bl->pwm_name);
		if (IS_ERR(pwm)) {
			dev_err(lm3697_bl->chip->dev,
				"Can not get PWM device: %s\n",
				lm3697_bl->pwm_name);
			return;
		}
		lm3697_bl->pwm = pwm;
	}

	period = lm3697_bl->bl_pdata->pwm_period;
	duty = br * period / max_br;

	pwm_config(lm3697_bl->pwm, duty, period);
	if (duty)
		pwm_enable(lm3697_bl->pwm);
	else
		pwm_disable(lm3697_bl->pwm);
}

static int lm3697_bl_set_brightness(struct lm3697_bl *lm3697_bl, int brightness)
{
	int ret;
	u8 data;
	u8 reg_lsb[] = { LM3697_REG_BRT_A_LSB, LM3697_REG_BRT_B_LSB, };
	u8 reg_msb[] = { LM3697_REG_BRT_A_MSB, LM3697_REG_BRT_B_MSB, };

	/* Two registers(LSB and MSB) are updated for 11-bit dimming */

	data = brightness & LM3697_BRT_LSB_MASK;
	ret = lm3697_bl_update_bits(lm3697_bl->chip,
				    reg_lsb[lm3697_bl->bank_id],
				    LM3697_BRT_LSB_MASK, data);
	if (ret)
		return ret;

	data = (brightness >> LM3697_BRT_MSB_SHIFT) & 0xFF;

	return lm3697_bl_write_byte(lm3697_bl->chip,
				    reg_msb[lm3697_bl->bank_id], data);
}

static int lm3697_bl_update_status(struct backlight_device *bl_dev)
{
	struct lm3697_bl *lm3697_bl = bl_get_data(bl_dev);
	int ret = 0;
	int brt;

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
		bl_dev->props.brightness = 0;

	brt = bl_dev->props.brightness;
	if (brt > 0)
		ret = lm3697_bl_enable(lm3697_bl, 1);
	else
		ret = lm3697_bl_enable(lm3697_bl, 0);

	if (ret)
		return ret;

	if (lm3697_bl->mode == BL_PWM_BASED)
		lm3697_bl_pwm_ctrl(lm3697_bl, brt,
				   bl_dev->props.max_brightness);

	/*
	 * Brightness register should always be written
	 * not only register based mode but also in PWM mode.
	 */
	return lm3697_bl_set_brightness(lm3697_bl, brt);
}

static int lm3697_bl_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static const struct backlight_ops lm3697_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3697_bl_update_status,
	.get_brightness = lm3697_bl_get_brightness,
};

static int lm3697_bl_set_ctrl_mode(struct lm3697_bl *lm3697_bl)
{
	struct lm3697_backlight_platform_data *pdata = lm3697_bl->bl_pdata;
	int bank_id = lm3697_bl->bank_id;

	if (pdata->pwm_period > 0)
		lm3697_bl->mode = BL_PWM_BASED;
	else if (pdata->pwm_enable)
		lm3697_bl->mode = BL_REGISTER_BASED_PWM;
	else
		lm3697_bl->mode = BL_REGISTER_BASED;

	/* Control bank assignment for PWM control */
	if (lm3697_bl->mode == BL_PWM_BASED) {
		snprintf(lm3697_bl->pwm_name, sizeof(lm3697_bl->pwm_name),
			 "%s", LM3697_DEFAULT_PWM);

		return lm3697_bl_update_bits(lm3697_bl->chip,
					     LM3697_REG_PWM_CFG, BIT(bank_id),
					     1 << bank_id);
	}
	else if(lm3697_bl->mode == BL_REGISTER_BASED_PWM) {
		return lm3697_bl_update_bits(lm3697_bl->chip,
					     LM3697_REG_PWM_CFG, BIT(bank_id),
					     1 << bank_id);
	}

	return 0;
}

static int lm3697_bl_string_configure(struct lm3697_bl *lm3697_bl)
{
	int bank_id = lm3697_bl->bank_id;
	int is_detected = 0;
	int i, ret;

	/* Control bank assignment for backlight string configuration */
	for (i = 0; i < LM3697_MAX_STRINGS; i++) {
		if (test_bit(i, &lm3697_bl->bl_pdata->bl_string)) {
			ret = lm3697_bl_update_bits(lm3697_bl->chip,
						    LM3697_REG_OUTPUT_CFG,
						    BIT(i), bank_id << i);
			if (ret)
				return ret;

			is_detected = 1;
		}
	}

	if (!is_detected) {
		dev_err(lm3697_bl->chip->dev, "No backlight string found\n");
		return -EINVAL;
	}

	return 0;
}

static int lm3697_bl_set_current(struct lm3697_bl *lm3697_bl)
{
	u8 reg[] = { LM3697_REG_IMAX_A, LM3697_REG_IMAX_B, };

	return lm3697_bl_write_byte(lm3697_bl->chip, reg[lm3697_bl->bank_id],
				    lm3697_bl->bl_pdata->imax);
}


static int lm3697_bl_set_boost(struct lm3697_bl *lm3697_bl)
{
	return lm3697_bl_write_byte(lm3697_bl->chip, LM3697_REG_BOOST_CTRL, 0x02);
}
static int lm3697_bl_configure(struct lm3697_bl *lm3697_bl)
{
	int ret;

	ret = lm3697_bl_set_ctrl_mode(lm3697_bl);
	if (ret)
		return ret;

	ret = lm3697_bl_string_configure(lm3697_bl);
	if (ret)
		return ret;

	ret = lm3697_bl_set_boost(lm3697_bl);
	if (ret)
		return ret;

	return lm3697_bl_set_current(lm3697_bl);
}

static enum lm3697_max_current lm3697_get_current_code(u8 imax_mA)
{
	const enum lm3697_max_current imax_table[] = {
		LM3697_IMAX_6mA,  LM3697_IMAX_7mA,  LM3697_IMAX_8mA,
		LM3697_IMAX_9mA,  LM3697_IMAX_10mA, LM3697_IMAX_11mA,
		LM3697_IMAX_12mA, LM3697_IMAX_13mA, LM3697_IMAX_14mA,
		LM3697_IMAX_15mA, LM3697_IMAX_16mA, LM3697_IMAX_17mA,
		LM3697_IMAX_18mA, LM3697_IMAX_19mA, LM3697_IMAX_20mA,
		LM3697_IMAX_21mA, LM3697_IMAX_22mA, LM3697_IMAX_23mA,
		LM3697_IMAX_24mA, LM3697_IMAX_25mA, LM3697_IMAX_26mA,
		LM3697_IMAX_27mA, LM3697_IMAX_28mA, LM3697_IMAX_29mA,
	};

	/*
	 * Convert milliampere to appropriate enum code value.
	 * Input range : 5 ~ 30mA
	 */

	if (imax_mA <= 5)
		return LM3697_IMAX_5mA;

	if (imax_mA >= 30)
		return LM3697_IMAX_30mA;

	return imax_table[imax_mA - LM3697_IMAX_OFFSET];
}

/* This helper funcion is moved from linux-v3.9 */
static inline int _of_get_child_count(const struct device_node *np)
{
	struct device_node *child;
	int num = 0;

	for_each_child_of_node(np, child)
		num++;

	return num;
}

static int lm3697_bl_parse_dt(struct device *dev, struct lm3697_bl_chip *chip)
{
	struct lm3697_platform_data *pdata;
	struct lm3697_backlight_platform_data *bl_pdata;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	int num_backlights;
	int i = 0;
	unsigned int imax_mA;

	if (!node) {
		dev_err(dev, "No device node exists\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->en_gpio = of_get_named_gpio(node, "ti,enable-gpio", 0);
	if (pdata->en_gpio < 0)
		return pdata->en_gpio;

	num_backlights = _of_get_child_count(node);
	if (num_backlights == 0) {
		dev_err(dev, "No backlight strings\n");
		return -EINVAL;
	}

	bl_pdata = devm_kzalloc(dev, sizeof(*bl_pdata) * num_backlights,
				GFP_KERNEL);
	if (!bl_pdata)
		return -ENOMEM;

	for_each_child_of_node(node, child) {
		of_property_read_string(child, "backlight-name",
					&bl_pdata[i].name);

		/* Make backlight strings */
		bl_pdata[i].bl_string = 0;
		if (of_property_read_bool(child, "hvled1-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED1;
		if (of_property_read_bool(child, "hvled2-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED2;
		if (of_property_read_bool(child, "hvled3-used"))
			bl_pdata[i].bl_string |= LM3697_HVLED3;

		imax_mA = 0;
		of_property_read_u32(child, "max-current-milliamp", &imax_mA);
		bl_pdata[i].imax = lm3697_get_current_code(imax_mA);

		of_property_read_u32(child, "initial-brightness",
				     &bl_pdata[i].init_brightness);

		/* PWM mode */
		of_property_read_u32(child, "pwm-period",
				     &bl_pdata[i].pwm_period);

		bl_pdata[i].pwm_enable = 0;
		bl_pdata[i].pwm_enable = of_property_read_bool(child, "pwm-enable");

		i++;
	}

	pdata->bl_pdata = bl_pdata;
	pdata->num_backlights = num_backlights;
	chip->pdata = pdata;

	return 0;
}

static int lm3697_bl_enable_hw(struct lm3697_bl_chip *chip)
{
	return gpio_request_one(chip->pdata->en_gpio, GPIOF_OUT_INIT_HIGH,
				"lm3697_hwen");
}

static void lm3697_bl_disable_hw(struct lm3697_bl_chip *chip)
{
	if (chip->pdata->en_gpio) {
		gpio_set_value(chip->pdata->en_gpio, 0);
		gpio_free(chip->pdata->en_gpio);
	}
}

static int lm3697_bl_add_device(struct lm3697_bl *lm3697_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct lm3697_backlight_platform_data *pdata = lm3697_bl->bl_pdata;
	char name[20];

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = pdata ? pdata->init_brightness : 0;
	props.max_brightness = LM3697_MAX_BRIGHTNESS;

	/* Backlight device name */
	if (!pdata->name)
		snprintf(name, sizeof(name), "%s:%d", LM3697_DEFAULT_NAME,
			 lm3697_bl->bank_id);
	else
		snprintf(name, sizeof(name), "%s", pdata->name);

	bl_dev = backlight_device_register(name, lm3697_bl->chip->dev,
					   lm3697_bl, &lm3697_bl_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lm3697_bl->bl_dev = bl_dev;

	return 0;
}

static struct lm3697_bl *lm3697_bl_register(struct lm3697_bl_chip *chip)
{
	struct lm3697_backlight_platform_data *pdata = chip->pdata->bl_pdata;
	struct lm3697_bl *lm3697_bl, *each;
	int num_backlights = chip->pdata->num_backlights;
	int i, ret;

	lm3697_bl = devm_kzalloc(chip->dev, sizeof(*lm3697_bl) * num_backlights,
				 GFP_KERNEL);
	if (!lm3697_bl)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_backlights; i++) {
		each = lm3697_bl + i;
		each->bank_id = i;
		each->chip = chip;
		each->bl_pdata = pdata + i;

		ret = lm3697_bl_configure(lm3697_bl);
		if (ret) {
			dev_err(chip->dev, "Backlight config err: %d\n", ret);
			goto err;
		}

		ret = lm3697_bl_add_device(each);
		if (ret) {
			dev_err(chip->dev, "Backlight device err: %d\n", ret);
			goto cleanup_backlights;
		}

		backlight_update_status(each->bl_dev);
	}

	return lm3697_bl;

cleanup_backlights:
	while (--i >= 0) {
		each = lm3697_bl + i;
		backlight_device_unregister(each->bl_dev);
	}
err:
	return ERR_PTR(ret);
}

static int lm3697_bl_unregister(struct lm3697_bl *lm3697_bl)
{
	struct lm3697_bl *each;
	struct backlight_device *bl_dev;
	int num_backlights = lm3697_bl->chip->pdata->num_backlights;
	int i;

	for (i = 0; i < num_backlights; i++) {
		each = lm3697_bl + i;

		bl_dev = each->bl_dev;
		bl_dev->props.brightness = 0;
		backlight_update_status(bl_dev);
		backlight_device_unregister(bl_dev);
	}

	return 0;
}

static struct regmap_config lm3697_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = LM3697_MAX_REGISTERS,
};

static int lm3697_bl_probe(struct i2c_client *cl,
			   const struct i2c_device_id *id)
{
	struct device *dev = &cl->dev;
	struct lm3697_platform_data *pdata = dev_get_platdata(dev);
	struct lm3697_bl_chip *chip;
	struct lm3697_bl *lm3697_bl;
	int ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	chip->pdata = pdata;

	chip->regmap = devm_regmap_init_i2c(cl, &lm3697_regmap_config);
	if (IS_ERR(chip->regmap))
		return PTR_ERR(chip->regmap);

	if (!chip->pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = lm3697_bl_parse_dt(dev, chip);
		else
			return -ENODEV;

		if (ret)
			return ret;
	}

	ret = lm3697_bl_enable_hw(chip);
	if (ret)
		return ret;

	lm3697_bl = lm3697_bl_register(chip);
	if (IS_ERR(lm3697_bl))
		return PTR_ERR(lm3697_bl);

	i2c_set_clientdata(cl, lm3697_bl);

	return 0;
}

static int lm3697_bl_remove(struct i2c_client *cl)
{
	struct lm3697_bl *lm3697_bl = i2c_get_clientdata(cl);

	lm3697_bl_unregister(lm3697_bl);
	lm3697_bl_disable_hw(lm3697_bl->chip);

	return 0;
}

static const struct i2c_device_id lm3697_bl_ids[] = {
	{ "lm3697", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3697_bl_ids);

#ifdef CONFIG_OF
static const struct of_device_id lm3697_bl_of_match[] = {
	{ .compatible = "ti,lm3697", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3697_bl_of_match);
#endif

static struct i2c_driver lm3697_bl_driver = {
	.probe = lm3697_bl_probe,
	.remove = lm3697_bl_remove,
	.driver = {
		.name = "lm3697",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3697_bl_of_match),
	},
	.id_table = lm3697_bl_ids,
};
module_i2c_driver(lm3697_bl_driver);

MODULE_DESCRIPTION("TI LM3697 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
