/*
 *  max77836_charger.c
 *  Samsung MAX77836 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#include <linux/platform_data/android_battery.h>

#define ENABLE 1
#define DISABLE 0
#define BIT_MBCICHWRCL	BIT (4)

#define CHARGING_CURRENT_MAX 475000

static enum power_supply_property max77836_chg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static void max77836_chg_set_command(struct max77836_chg_data *charger,
		int reg, int value)
{
	int ret;
	u8 data = 0;

	ret = max77836_read_reg(charger->client, (u8)reg, &data);
	if (ret >= 0) {
		if (data != value) {
			data = value;
			max77836_write_reg(charger->client,
					(u8)reg, data);
		}
	}
}

static int max77836_chg_get_charging_status(struct max77836_chg_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 data = 0;

	max77836_read_reg(charger->client, MAX77836_CHG_REG_STATUS3, &data);
	dev_info(&charger->client->dev,
			"%s : charging status (0x%02x)\n", __func__, data);

	if (data & 0x01)
		status = POWER_SUPPLY_STATUS_FULL;
	else if (data & 0x02)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else if (data & 0x04)
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	return (int)status;
}

static int max77836_chg_get_charging_health(struct max77836_chg_data *charger)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 data = 0;

	max77836_read_reg(charger->client, MAX77836_CHG_REG_STATUS3, &data);
	dev_info(&charger->client->dev,
			"%s : health status (0x%02x)\n", __func__, data);

	if (data & 0x04)
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;

	return (int)health;
}

static u8 max77836_chg_get_float_voltage_data(
		int uV)
{
	u8 data;

	data = uV < 4000000 ? 1 :
		uV < 4200000 ? (uV - 4000000)/20000 + 1 :
		uV < 4220000 ? 0 :
		uV < 4280000 ? (uV - 4220000)/20000 + 11 :
		uV < 4350000 ? 14 :
		uV == 4350000 ? 15 : 0;

	return data;
}

static void max77836_chg_set_float_voltage(
		struct max77836_chg_data *charger, int uV)
{
	u8 data = 0;
	data = max77836_chg_get_float_voltage_data(uV);
	dev_info(&charger->client->dev, "%s : float voltage (%dmV), data(0x%x)\n",
			__func__, uV / 1000, data);
	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL3, data);
}

static u8 max77836_chg_get_eoc_current_value(
		int uA)
{
	u8 data;

	data = ((uA < 7500) ? 1 :
			(uA < 10000) ? 0 :
			(uA < 60000) ? ((uA - 10000)/5000 + 2) :
			(uA < 100000) ? ((uA - 60000)/10000 + 11) : 15);

	return data;
}

static void max77836_chg_set_eoc_current(
		struct max77836_chg_data *charger, int uA)
{
	int data = 0;

	data = max77836_chg_get_eoc_current_value(uA);
	dev_info(&charger->client->dev,
			"%s : EOC (%dmA), data(0x%x)\n", __func__,
			uA / 1000, data);

	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL5, data);
}

static u8 max77836_chg_get_charging_current_value(
		int uA)
{
	u8 data = 0;

	data = ((uA < 100000) ? 0 :
			(uA < 225000) ? ((uA - 100000)/25000) :
			(uA < 250000) ? 9 :
			(uA < 325000) ? ((uA - 250000)/25000 + 6) :
			(uA < 350000) ? 5 :
			(uA < 475000) ? ((uA - 350000)/25000 + 10) : 15);

	return data;
}

static void max77836_chg_set_charging_current(
		struct max77836_chg_data *charger, int uA)
{
	u8 data = 0;

	/* Fast Battery Charge Current */
	if (uA >= 100000)
		data = max77836_chg_get_charging_current_value(uA) | BIT_MBCICHWRCL;
	else
		data = 0;

	dev_info(&charger->client->dev, "%s : charging current (%dmA), data(0x%x)\n",
			__func__, uA / 1000, data);

	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL4, data);
}

static void max77836_chg_set_configuration(struct max77836_chg_data *charger)
{
	u8 data;

	/* Battery Fast-Charge Timer disabled [6:4] = 0b111 */
	data = 0x70;
	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL1, data);

	/* Battery-Charger Constant Voltage(CV) Mode, float voltage */
	max77836_chg_set_float_voltage(charger, charger->pdata->chg_float_voltage);

	/* Fast Battery Charge Current */
	max77836_chg_set_charging_current(charger, charger->charging_current);

	/* End-of-Charge Current Setting */
	max77836_chg_set_eoc_current(charger, charger->pdata->
			charging_current[charger->cable_type].full_check_current);

	/* Auto Charging Stop disabled [5] = 0 */
	data = 0;
	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL6, data);

	/* Overvoltage-Protection Threshold 6.5V [1:0] = 0b10 */
	data = 0x01;
	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL7, data);
}

static void max77836_chg_set_enable(struct max77836_chg_data *charger)
{
	u8 data;

	if(charger->is_charging) {
		/* turn on charger */
		data = 0xc0;
		max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL2, data);
	} else {
		/* turn off charger */
		data = 0x00;
		max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL2, data);
	}
}

static int max77836_chg_get_charging_current_now(
		struct max77836_chg_data *charger)
{
	u8 data;
	int uA;

	max77836_read_reg(charger->client,
			MAX77836_CHG_REG_CHG_CTRL4, &data);

	/* Fast Battery Charge Current */
	if (data & 0x10)
		uA = ((data & 0x0f) * 25 + 100) * 1000;
	else
		uA = 0;

	return uA;
}

static int max77836_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77836_chg_data *charger =
		container_of(psy, struct max77836_chg_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77836_chg_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77836_chg_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:	/* charging current */
		/* calculated charging current limit value */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			val->intval =
				max77836_chg_get_charging_current_now(charger);
		} else
			val->intval = 0;

		dev_info(&charger->client->dev,
			"%s : set-current(%dmA), current now(%dmA)\n",
			__func__, charger->charging_current/1000,
			val->intval/1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

extern int androidboot_mode_charger;
static int max77836_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct max77836_chg_data *charger =
		container_of(psy, struct max77836_chg_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		charger->bootdone = 1;
		break;

	/* val->intval : type */
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->cable_type = val->intval;
		pr_info("%s: bootdone(%d), poweroff_charging(%d)\n",
				__func__, charger->bootdone, androidboot_mode_charger);
		if (!charger->bootdone && !androidboot_mode_charger) {
			charger->charging_current =
				CHARGING_CURRENT_MAX;
		} else if (androidboot_mode_charger) {
			charger->charging_current =
				charger->pdata->charging_current[
				CHARGE_SOURCE_AC].fast_charging_current;
		} else {
			charger->charging_current =
				charger->pdata->charging_current[
				val->intval].fast_charging_current;
		}
		max77836_chg_set_configuration(charger);

		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->is_charging = val->intval;
		max77836_chg_set_enable(charger);
		break;

	/* to control charging current,
	 * use input current limit and set charging current as much as possible
	 * so we only control input current limit to control charge current
	 */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		break;

		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		max77836_chg_set_charging_current(charger, val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->charging_current = val->intval;
		max77836_chg_set_charging_current(charger, charger->charging_current);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_OF
static int max77836_chg_read_u32_index_dt(const struct device_node *np,
		const char *propname,
		u32 index, u32 *out_value)
{
	struct property *prop = of_find_property(np, propname, NULL);
	u32 len = (index + 1) * sizeof(*out_value);

	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;
	if (len > prop->length)
		return -EOVERFLOW;

	*out_value = be32_to_cpup(((__be32 *)prop->value) + index);

	return 0;
}

static int max77836_chg_parse_dt(struct max77836_chg_data *charger)
{
	struct device_node *np = of_find_node_by_name(NULL, "max77836-charger");
	int ret = 0;
	int len;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		int i;
		ret = of_property_read_u32(np, "charger,chg_float_voltage",
				&charger->pdata->chg_float_voltage);
		pr_info("%s: charger,chg_float_voltage:%d\n", __func__, charger->pdata->chg_float_voltage);

		of_get_property(np, "charger,fast_charging_current", &len);
		len = len / sizeof(u32);
		charger->pdata->charging_current = kzalloc(sizeof(max77836_chg_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = max77836_chg_read_u32_index_dt(np,
					"charger,fast_charging_current", i,
					&charger->pdata->charging_current[i].fast_charging_current);
			if (ret)
				pr_info("%s: charger,fast_charging_current is empty\n", __func__);
			ret = max77836_chg_read_u32_index_dt(np,
					"charger,full_check_current", i,
					&charger->pdata->charging_current[i].full_check_current);
			if (ret)
				pr_info("%s: charger,full_check_current is empty\n", __func__);
		}
	}
	return ret;
}
#endif

static int max77836_chg_probe(struct platform_device *pdev)
{
	struct max77836_dev *mfd_dev = dev_get_drvdata(pdev->dev.parent);
	struct max77836_chg_data *charger;
	int ret = 0;

	dev_info(&pdev->dev,
			"%s: MAX77836 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->pdata = kzalloc(sizeof(max77836_chg_platform_data_t), GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_free;
	}

	charger->client = mfd_dev->i2c;

#if defined(CONFIG_OF)
	ret = max77836_chg_parse_dt(charger);
	if (ret < 0) {
		pr_err("%s Not found charger DT! ret[%d]\n",
				__func__, ret);
		goto err_free;
	}
#endif

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name		= "max77836-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= max77836_chg_get_property;
	charger->psy_chg.set_property	= max77836_chg_set_property;
	charger->psy_chg.properties	= max77836_chg_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(max77836_chg_props);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&pdev->dev,
				"%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	pr_info("%s: Max77836 Charger Driver Loaded\n", __func__);
	return 0;

err_free:
	kfree(charger);

	return ret;
}

static int max77836_chg_remove(struct platform_device *pdev)
{
	struct max77836_chg_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77836_chg_suspend(struct device *dev)
{
	return 0;
}

static int max77836_chg_resume(struct device *dev)
{
	return 0;
}
#else
#define max77836_chg_suspend NULL
#define max77836_chg_resume NULL
#endif

static void max77836_chg_shutdown(struct device *dev)
{
	struct max77836_chg_data *charger = dev_get_drvdata(dev);
	u8 data;
	pr_info("%s: MAX77836 Charger driver shutdown\n", __func__);
	data = 0xc0;
	max77836_chg_set_command(charger, MAX77836_CHG_REG_CHG_CTRL2, data);
}

const struct dev_pm_ops max77836_chg_pm_ops = {
	.suspend = max77836_chg_suspend,
	.resume = max77836_chg_resume,
};

static struct platform_driver max77836_chg_driver = {
	.driver = {
		.name = "max77836-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77836_chg_pm_ops,
#endif
		.shutdown = max77836_chg_shutdown,
	},
	.probe = max77836_chg_probe,
	.remove = max77836_chg_remove,
};

static int __init max77836_chg_init(void)
{
	return platform_driver_register(&max77836_chg_driver);
}

static void __exit max77836_chg_exit(void)
{
	platform_driver_unregister(&max77836_chg_driver);
}

module_init(max77836_chg_init);
module_exit(max77836_chg_exit);

MODULE_DESCRIPTION("Samsung MAX77836 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
