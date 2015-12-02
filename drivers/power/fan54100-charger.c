/*
 * Copyright (C) 2015 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/printk.h>
#include <linux/interrupt.h>

#define FAN54100_CHRG_DRV_NAME "fan54100-charger"
#define FAN54100_CHRG_PSY_NAME "charger-switch"

/* Mask/Bit helpers */
#define _FAN_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define FAN_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_FAN_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))

/* Base Registers */
#define FAN54100_REG_ID			0x00

#define FAN54100_REG_CONTROL0		0x01
#define FAN54100_BIT_SW_ENABLE		BIT(0)
#define FAN54100_MSK_OCP		FAN_MASK(2, 1)
#define FAN54100_SHIFT_OCP		1
#define FAN54100_MSK_SOVP		FAN_MASK(6, 3)
#define FAN54100_SHIFT_SOVP		3
#define FAN54100_BIT_TEST_ENABLE	BIT(7)

#define FAN54100_REG_CONTROL1		0x02
#define FAN54100_MSK_FOVP		FAN_MASK(1, 0)
#define FAN54100_SHIFT_FOVP		0
#define FAN54100_MSK_SOTP		FAN_MASK(3, 2)
#define FAN54100_SHIFT_SOTP		2
#define FAN54100_BIT_INT_N_CTRL		BIT(4)
#define FAN54100_BIT_F0RCE_INT_N	BIT(5)

#define FAN54100_REG_STATUS		0x03
#define FAN54100_BIT_SW_STAT		BIT(0)
#define FAN54100_BIT_INT_STAT		BIT(1)
#define FAN54100_BIT_PWR_SRC		BIT(2)

#define FAN54100_REG_PROTECT		0x04
#define FAN54100_REG_FDS		0x05
#define FAN54100_REG_IRQ		0x06
#define FAN54100_REG_IRQ_MASK		0x07
#define FAN54100_BIT_UVLO		BIT(0)
#define FAN54100_BIT_SOTP		BIT(1)
#define FAN54100_BIT_RCB		BIT(2)
#define FAN54100_BIT_CDP		BIT(3)
#define FAN54100_BIT_OCP		BIT(4)
#define FAN54100_BIT_FOVP		BIT(5)
#define FAN54100_BIT_SOVP		BIT(6)
#define FAN54100_BIT_OVLO		BIT(7)

#define OCP_MIN_MA			4600
#define OCP_MAX_MA			6900
#define OCP_STEP_MA			800
#define OCP_NUM_STEP			4

#define SOVP_MIN_MV			4200
#define SOVP_MAX_MV			4575
#define SOVP_STEP_MV			25
#define SOVP_NUM_STEP			8

#define FOVP_MIN_MV			4700
#define FOVP_MAX_MV			4850
#define FOVP_STEP_MV			50
#define FOVP_NUM_STEP			4

#define SOTP_MIN_C			120
#define SOTP_MAX_C			150
#define SOTP_STEP_C			10
#define SOTP_NUM_STEP			4

struct fan54100_chrg_chip {
	struct i2c_client *client;
	struct device *dev;

	struct gpio fan54100_int_n;

	struct power_supply sw_psy;
	struct power_supply *batt_psy;
	struct dentry *debug_root;
	u32 peek_poke_address;
	int fovp_limit_mv;
	int ocp_limit_ma;
	int sotp_limit_c;
	int health;
	bool factory_mode;
};

static int fan54100_chrg_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
	int ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int fan54100_chrg_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int fan54100_chrg_masked_write_reg(struct i2c_client *client, u8 reg,
				   u8 mask,
				   u8 value)
{
	int curr_val = fan54100_chrg_read_reg(client, reg);
	int ret;

	if (curr_val < 0)
		return ret;

	curr_val &= ~mask;
	curr_val |= value & mask;

	ret = i2c_smbus_write_byte_data(client, reg, curr_val);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	struct fan54100_chrg_chip *chip = data;
	int temp;

	temp = fan54100_chrg_read_reg(chip->client, chip->peek_poke_address);

	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct fan54100_chrg_chip *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = fan54100_chrg_write_reg(chip->client,
				    chip->peek_poke_address, temp);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int find_protect_setting(int req, int min_val, int max_val,
				int step_size, char num_step)
{
	int i;

	if (req <= min_val) {
		pr_debug("Requested Setting: %d At or Below Minimum: %d\n",
			 req, min_val);
		return 0;
	}

	if (req >= max_val) {
		pr_debug("Requested Setting: %d At or Above Maximum: %d\n",
			 req, max_val);
		return (num_step - 1);
	}

	for (i = 1; i < num_step; i++)
		if (req < (min_val + (i * step_size)))
			break;

	if (i > 0)
		i--;

	return i;
}

static void fan54100_chrg_update_health(struct fan54100_chrg_chip *chip,
					int health)
{
	switch (chip->health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
	case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		break;
	case POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE:
	case POWER_SUPPLY_HEALTH_GOOD:
		chip->health = health;
		break;
	case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
		if (health != POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE)
			chip->health = health;
		break;
	default:
		break;
	}
}

static void fan54100_chrg_get_psys(struct fan54100_chrg_chip *chip)
{
	int i = 0;
	struct power_supply *psy;

	if (!chip) {
		pr_err_once("Chip not ready\n");
		return;
	}

	for (i = 0; i < chip->sw_psy.num_supplies; i++) {
		psy = power_supply_get_by_name(chip->sw_psy.supplied_from[i]);

		if (psy)
			switch (psy->type) {
			case POWER_SUPPLY_TYPE_BATTERY:
				chip->batt_psy = psy;
				break;
			default:
				break;
			}
	}

	if (!chip->batt_psy)
		pr_err("BATT PSY Not Found\n");
}

static enum power_supply_property fan54100_chrg_props[] = {
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_HEALTH,
};

static int fan54100_chrg_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct fan54100_chrg_chip *chip;
	u8 reg_val;

	if (!psy || !psy->dev || !val) {
		pr_err("fan54100_chrg_get_property NO dev!\n");
		return -ENODEV;
	}

	chip = dev_get_drvdata(psy->dev->parent);
	if (!chip) {
		pr_err("fan54100_chrg_get_property NO dev chip!\n");
		return -ENODEV;
	}

	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		reg_val = fan54100_chrg_read_reg(chip->client,
						 FAN54100_REG_STATUS);
		reg_val &= FAN54100_BIT_SW_ENABLE;
		val->intval = reg_val;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		reg_val = fan54100_chrg_read_reg(chip->client,
						 FAN54100_REG_CONTROL0);
		reg_val &= FAN54100_MSK_SOVP;
		reg_val = reg_val >> FAN54100_SHIFT_SOVP;
		if (reg_val > (SOVP_NUM_STEP - 1))
			reg_val = (SOVP_NUM_STEP - 1);
		val->intval = ((reg_val * SOVP_STEP_MV) + SOVP_MIN_MV) * 1000;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int fan54100_chrg_set_property(struct power_supply *psy,
				      enum power_supply_property prop,
				      const union power_supply_propval *val)
{
	int rc = 0;
	u8 protect_bits;
	struct fan54100_chrg_chip *chip;

	if (!psy || !psy->dev || !val) {
		pr_err("fan54100_chrg_set_property NO dev!\n");
		return -ENODEV;
	}

	chip = dev_get_drvdata(psy->dev->parent);
	if (!chip) {
		pr_err("fan54100_chrg_get_property NO dev chip!\n");
		return -ENODEV;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		protect_bits = val->intval ?  FAN54100_BIT_SW_ENABLE : 0;
		if (val->intval)
			chip->health = POWER_SUPPLY_HEALTH_GOOD;
		rc = fan54100_chrg_masked_write_reg(chip->client,
						    FAN54100_REG_CONTROL0,
						    FAN54100_BIT_SW_ENABLE,
						    protect_bits);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		protect_bits = find_protect_setting((val->intval / 1000),
						    SOVP_MIN_MV,
						    SOVP_MAX_MV,
						    SOVP_STEP_MV ,
						    SOVP_NUM_STEP);
		protect_bits = protect_bits << FAN54100_SHIFT_SOVP;

		rc = fan54100_chrg_masked_write_reg(chip->client,
						    FAN54100_REG_CONTROL0,
						    FAN54100_MSK_SOVP,
						    protect_bits);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int fan54100_chrg_is_writeable(struct power_supply *psy,
				      enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int fan54100_uvlo_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
	dev_dbg(chip->dev, "FAN54100 UVLO IRQ tripped!\n");
	return 0;
}

static int fan54100_sotp_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_OVERHEAT);
	dev_warn(chip->dev, "FAN54100 SOTP IRQ tripped!\n");
	return 0;
}

static int fan54100_rcb_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
	dev_warn(chip->dev, "FAN54100 RCB IRQ tripped!\n");
	return 0;
}

static int fan54100_cdp_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
	dev_warn(chip->dev, "FAN54100 CDP IRQ tripped!\n");
	return 0;
}

static int fan54100_ocp_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
	dev_warn(chip->dev, "FAN54100 OCP IRQ tripped!\n");
	return 0;
}

static int fan54100_fovp_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_OVERVOLTAGE);
	dev_warn(chip->dev, "FAN54100 FOVP IRQ tripped!\n");
	return 0;
}

static int fan54100_sovp_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip,
				    POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE);
	dev_dbg(chip->dev, "FAN54100 SOVP IRQ tripped!\n");
	return 0;
}

static int fan54100_ovlo_handler(struct fan54100_chrg_chip *chip)
{
	fan54100_chrg_update_health(chip, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
	dev_warn(chip->dev, "FAN54100 OVLO IRQ tripped!\n");
	return 0;
}

struct fan54100_irq_info {
	const char		*name;
	int			(*fan_irq)(struct fan54100_chrg_chip *chip);
};

static struct fan54100_irq_info handlers[] = {

	{
		.name		= "uvlo",
		.fan_irq	= fan54100_uvlo_handler,
	},
	{
		.name		= "sotp",
		.fan_irq	= fan54100_sotp_handler,
	},
	{
		.name		= "rcb",
		.fan_irq	= fan54100_rcb_handler,
	},
	{
		.name		= "cdp",
		.fan_irq	= fan54100_cdp_handler,
	},
	{
		.name		= "ocp",
		.fan_irq	= fan54100_ocp_handler,
	},
	{
		.name		= "fovp",
		.fan_irq	= fan54100_fovp_handler,
	},
	{
		.name		= "sovp",
		.fan_irq	= fan54100_sovp_handler,
	},
	{
		.name		= "ovlo",
		.fan_irq	= fan54100_ovlo_handler,
	},
};

static bool fan54100_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

static irqreturn_t fan54100_int_n_handler(int irq, void *dev_id)
{
	u8 int_mask;
	u8 int_irq;
	u8 int_cntrl;
	u8 irq_pres;
	int i;
	int rc;
	int int_lvl;
	struct fan54100_chrg_chip *chip = dev_id;
	struct i2c_client *client = chip->client;

	if (chip->factory_mode)
		return IRQ_HANDLED;

fan54100_irq_retry:
	int_lvl = gpio_get_value(chip->fan54100_int_n.gpio);
	if (!int_lvl) {
		int_mask = fan54100_chrg_read_reg(client,
						  FAN54100_REG_IRQ_MASK);
		int_irq = fan54100_chrg_read_reg(client,
						 FAN54100_REG_IRQ);
		for (i = 0; i < 8; i++) {
			irq_pres = 0x1 << i;
			if (int_mask & irq_pres)
				continue;
			if (int_irq & irq_pres) {
				handlers[i].fan_irq(chip);
				rc = fan54100_chrg_masked_write_reg(client,
							    FAN54100_REG_IRQ,
							    irq_pres,
							    irq_pres);
				if (rc < 0)
					dev_err(chip->dev,
						"Clear IRQ %s Fail rc = %d\n",
						handlers[i].name, rc);
			}
		}
		int_cntrl = fan54100_chrg_read_reg(client,
						   FAN54100_REG_CONTROL0);
		if (int_cntrl & FAN54100_BIT_SW_ENABLE) {
			dev_err(chip->dev, "IRQ still valid, retry\n");
			goto fan54100_irq_retry;
		}
		power_supply_changed(&chip->sw_psy);
	}

	dev_dbg(chip->dev, "fan54100_int_n_handler int_n =%d\n", int_lvl);

	return IRQ_HANDLED;
}

static int fan54100_chrg_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret = 0;
	int i;
	char **sf;
	enum of_gpio_flags flags;
	struct fan54100_chrg_chip *chip;
	struct device_node *np = client->dev.of_node;
	u8 protect_bits;
	int irq;

	if (!np) {
		dev_err(&client->dev, "No OF DT node found.\n");
		return -ENODEV;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;
	i2c_set_clientdata(client, chip);

	if (fan54100_chrg_read_reg(chip->client, FAN54100_REG_ID) < 0) {
		dev_info(&client->dev, "fan54100_charger absent\n");
		return -ENODEV;
	}

	chip->factory_mode = fan54100_mmi_factory();
	if (chip->factory_mode)
		dev_info(&client->dev, "fan54100: Factory Mode\n");

	chip->health = POWER_SUPPLY_HEALTH_GOOD;

	chip->fan54100_int_n.gpio = of_get_gpio_flags(np, 0, &flags);
	chip->fan54100_int_n.flags = flags;
	of_property_read_string_index(np, "gpio-names", 0,
				      &chip->fan54100_int_n.label);
	dev_dbg(&client->dev, "GPIO: %d  FLAGS: %ld  LABEL: %s\n",
		chip->fan54100_int_n.gpio, chip->fan54100_int_n.flags,
		chip->fan54100_int_n.label);

	ret = gpio_request_one(chip->fan54100_int_n.gpio,
			       chip->fan54100_int_n.flags,
			       chip->fan54100_int_n.label);
	if (ret) {
		dev_err(&client->dev, "failed to request GPIO\n");
		return -ENODEV;
	}

	if (chip->fan54100_int_n.flags & GPIOF_EXPORT) {
		ret = gpio_export_link(&client->dev,
			       chip->fan54100_int_n.label,
			       chip->fan54100_int_n.gpio);
		if (ret) {
			dev_err(&client->dev, "Failed to link GPIO %s: %d\n",
				chip->fan54100_int_n.label,
				chip->fan54100_int_n.gpio);
			goto fail_gpio;
		}
	}

	if (chip->fan54100_int_n.gpio) {
		irq = gpio_to_irq(chip->fan54100_int_n.gpio);
		if (irq < 0) {
			dev_err(&client->dev, " gpio_to_irq failed\n");
			ret = -ENODEV;
			goto fail_gpio;
		}
		ret = devm_request_threaded_irq(&client->dev, irq,
						NULL,
						fan54100_int_n_handler,
						(IRQF_TRIGGER_RISING
						 | IRQF_TRIGGER_FALLING
						 | IRQF_ONESHOT),
						"fan54100_irq_n",
						chip);
		if (ret < 0) {
			dev_err(&client->dev, "request irq failed\n");
			goto fail_gpio;
		}
	}

	ret = of_property_read_u32(np, "fairchild,sotp-limit-c",
				   &chip->sotp_limit_c);
	if (ret < 0)
		chip->sotp_limit_c = SOTP_MIN_C;

	ret = of_property_read_u32(np, "fairchild,fovp-limit-mv",
				   &chip->fovp_limit_mv);
	if (ret < 0)
		chip->fovp_limit_mv = FOVP_MIN_MV;

	ret = of_property_read_u32(np, "fairchild,ocp-limit-ma",
				   &chip->ocp_limit_ma);
	if (ret < 0)
		chip->ocp_limit_ma = OCP_MIN_MA;

	chip->sw_psy.num_supplies =
		of_property_count_strings(np, "supply-names");

	chip->sw_psy.supplied_from =
		devm_kzalloc(&client->dev,
			     sizeof(char *) *
			     chip->sw_psy.num_supplies,
			     GFP_KERNEL);

	if (!chip->sw_psy.supplied_from) {
		dev_err(&client->dev,
			"fan54100 fail create supplied_from\n");
		goto fail_gpio;
	}


	for (i = 0; i < chip->sw_psy.num_supplies; i++) {
		sf = &chip->sw_psy.supplied_from[i];
		of_property_read_string_index(np, "supply-names", i,
					      (const char **)sf);
		dev_info(&client->dev, "supply name: %s\n",
			chip->sw_psy.supplied_from[i]);
	}

	chip->sw_psy.name = FAN54100_CHRG_PSY_NAME;
	chip->sw_psy.type = POWER_SUPPLY_TYPE_SWITCH;
	chip->sw_psy.properties = fan54100_chrg_props;
	chip->sw_psy.num_properties = ARRAY_SIZE(fan54100_chrg_props);
	chip->sw_psy.get_property = fan54100_chrg_get_property;
	chip->sw_psy.set_property = fan54100_chrg_set_property;
	chip->sw_psy.property_is_writeable = fan54100_chrg_is_writeable;
	ret = power_supply_register(&client->dev, &chip->sw_psy);
	if (ret < 0) {
		dev_err(&client->dev,
			"power_supply_register fan54100 failed ret = %d\n",
			ret);
		goto fail_gpio;
	}

	fan54100_chrg_get_psys(chip);

	chip->debug_root = debugfs_create_dir("fan54100_chrg", NULL);
	if (!chip->debug_root)
		dev_err(&client->dev, "Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_x32("address",
					 S_IFREG | S_IWUSR | S_IRUGO,
					 chip->debug_root,
					 &(chip->peek_poke_address));
		if (!ent)
			dev_err(&client->dev,
				"Couldn't create address debug file\n");

		ent = debugfs_create_file("data",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent)
			dev_err(&client->dev,
				"Couldn't create data debug file\n");
	}

	/* Open Switch */
	fan54100_chrg_masked_write_reg(client, FAN54100_REG_CONTROL0,
				       FAN54100_BIT_SW_ENABLE, 0);

	/* Configure INT_N */
	protect_bits = FAN54100_BIT_INT_N_CTRL | FAN54100_BIT_F0RCE_INT_N;
	fan54100_chrg_masked_write_reg(client, FAN54100_REG_CONTROL1,
				       protect_bits,
				       0);

	/* Set Protect Settings */
	protect_bits = find_protect_setting(chip->ocp_limit_ma, OCP_MIN_MA,
					    OCP_MAX_MA, OCP_STEP_MA,
					    OCP_NUM_STEP);
	protect_bits = protect_bits << FAN54100_SHIFT_OCP;

	fan54100_chrg_masked_write_reg(client, FAN54100_REG_CONTROL0,
				       FAN54100_MSK_OCP,
				       protect_bits);

	protect_bits = find_protect_setting(chip->fovp_limit_mv, FOVP_MIN_MV,
					    FOVP_MAX_MV, FOVP_STEP_MV,
					    FOVP_NUM_STEP);
	protect_bits = protect_bits << FAN54100_SHIFT_FOVP;

	fan54100_chrg_masked_write_reg(client, FAN54100_REG_CONTROL1,
				       FAN54100_MSK_FOVP,
				       protect_bits);

	protect_bits = find_protect_setting(chip->sotp_limit_c, SOTP_MIN_C,
					    SOTP_MAX_C, SOTP_STEP_C,
					    SOTP_NUM_STEP);
	protect_bits = protect_bits << FAN54100_SHIFT_SOTP;

	fan54100_chrg_masked_write_reg(client, FAN54100_REG_CONTROL1,
				       FAN54100_MSK_SOTP,
				       protect_bits);
	/* Enable All Interrupts */
	protect_bits = (FAN54100_BIT_UVLO |
			FAN54100_BIT_SOTP |
			FAN54100_BIT_RCB |
			FAN54100_BIT_CDP |
			FAN54100_BIT_OCP |
			FAN54100_BIT_FOVP |
			FAN54100_BIT_SOVP |
			FAN54100_BIT_OVLO);
	fan54100_chrg_write_reg(client, FAN54100_REG_PROTECT,
				protect_bits);

	/* Unmask All Interrupts */
	protect_bits = ~protect_bits;
	fan54100_chrg_write_reg(client, FAN54100_REG_IRQ_MASK,
				protect_bits);
	dev_info(&client->dev, "Done probing fan54100_charger\n");
	return 0;

fail_gpio:
	gpio_free(chip->fan54100_int_n.gpio);
	return ret;
}

static int fan54100_chrg_remove(struct i2c_client *client)
{
	struct fan54100_chrg_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		gpio_free(chip->fan54100_int_n.gpio);
		power_supply_unregister(&chip->sw_psy);
	}

	return 0;
}

static const struct of_device_id fan54100_chrg_of_tbl[] = {
	{ .compatible = "fairchild,fan54100", .data = NULL},
	{},
};

static const struct i2c_device_id fan54100_charger_id[] = {
	{"fan54100-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fan54100_charger_id);

static struct i2c_driver fan54100_charger_driver = {
	.driver		= {
		.name		= FAN54100_CHRG_DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= fan54100_chrg_of_tbl,
	},
	.probe		= fan54100_chrg_probe,
	.remove		= fan54100_chrg_remove,
	.id_table	= fan54100_charger_id,
};
module_i2c_driver(fan54100_charger_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("fan54100-charger driver");
MODULE_ALIAS("i2c:fan54100-charger");
