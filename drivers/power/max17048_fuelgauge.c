/*
 *  max17048_fuelgauge.c
 *  Samsung Mobile Fuel Gauge Driver
 *
 *  Copyright (C) 2014 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#include <linux/power/max17048_fuelgauge.h>

static enum power_supply_property max17048_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
};

static int max17048_fuelgauge_read_word(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17048_fuelgauge_get_vcell(struct i2c_client *client)
{
	u32 vcell;
	u16 w_data;
	u32 temp;

	temp = max17048_fuelgauge_read_word(client, MAX17048_VCELL_MSB);

	w_data = swab16(temp);

	temp = ((w_data & 0xFFF0) >> 4) * 1250;
	vcell = temp;

	dev_dbg(&client->dev,
		"%s : vcell (%d)\n", __func__, vcell);

	return vcell;
}

/* soc should be 0.01% unit */
static int max17048_fuelgauge_get_soc(struct i2c_client *client)
{
	struct max17048_fuelgauge_data *fuelgauge =
				i2c_get_clientdata(client);
	u8 data[2] = {0, 0};
	int temp, soc;
	u64 psoc64 = 0;
	u64 temp64;
	u32 divisor = 10000000;

	temp = max17048_fuelgauge_read_word(client, MAX17048_SOC_MSB);

	if (fuelgauge->pdata->is_using_model_data) {
		/* [ TempSOC = ((SOC1 * 256) + SOC2) * 0.001953125 ] */
		temp64 = swab16(temp);
		psoc64 = temp64 * 1953125;
		psoc64 = div_u64(psoc64, divisor);
		soc = psoc64 & 0xffff;
	} else {
		data[0] = temp & 0xff;
		data[1] = (temp & 0xff00) >> 8;

		soc = (data[0] * 100) + (data[1] * 100 / 256);
	}

	dev_dbg(&client->dev,
		"%s : raw capacity (%d), data(0x%04x)\n",
		__func__, soc, (data[0]<<8) | data[1]);

	return soc;
}

static u16 max17048_fuelgauge_get_rcomp(struct i2c_client *client)
{
	u16 w_data;
	int temp;

	temp = max17048_fuelgauge_read_word(client, MAX17048_RCOMP_MSB);

	w_data = swab16(temp);

	dev_dbg(&client->dev,
		"%s : current rcomp = 0x%04x\n",
		__func__, w_data);

	return w_data;
}

static void max17048_fuelgauge_set_rcomp(struct i2c_client *client, u16 new_rcomp)
{
	i2c_smbus_write_word_data(client,
		MAX17048_RCOMP_MSB, swab16(new_rcomp));
}

bool max17048_fuelgauge_fuelalert_init(struct i2c_client *client, int soc)
{
	u16 temp;
	u8 data;

	temp = max17048_fuelgauge_get_rcomp(client);
	data = 32 - soc; /* set soc for fuel alert */
	temp &= 0xff00;
	temp += data;

	dev_dbg(&client->dev,
		"%s : new rcomp = 0x%04x\n",
		__func__, temp);

	max17048_fuelgauge_set_rcomp(client, temp);

	return true;
}

static int max17048_fuelgauge_get_current(struct i2c_client *client)
{
	/* need to implement */
	return 0;
}

static void max17048_fuelgauge_get_version(struct i2c_client *client)
{
	u16 w_data;
	int temp;

	temp = max17048_fuelgauge_read_word(client, MAX17048_VER_MSB);

	w_data = swab16(temp);

	dev_info(&client->dev,
		"MAX17048 Fuel-Gauge Ver 0x%04x\n", w_data);
}


static void max17048_fuelgauge_rcomp_update(struct i2c_client *client, int temp)
{
	struct max17048_fuelgauge_data *fuelgauge =
				i2c_get_clientdata(client);
	union power_supply_propval value;

	int starting_rcomp = 0;
	int new_rcomp = 0;
	int rcomp_current = 0;
	struct power_supply *psy;

	rcomp_current = max17048_fuelgauge_get_rcomp(client);

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: Fail to battery psy \n", __func__);
		return;
	} else {
		int ret;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
		if (ret < 0) {
			dev_err(&client->dev,
					"%s: Fail to get property(%d)\n", __func__, ret);
			return;
		}
	}

	if (value.intval == POWER_SUPPLY_STATUS_CHARGING) /* in the charging */
		starting_rcomp = fuelgauge->pdata->rcomp_charging;
	else
		starting_rcomp = fuelgauge->pdata->rcomp_discharging;

	if (temp > RCOMP_DISCHARGING_THRESHOLD)
		new_rcomp = starting_rcomp + ((temp - RCOMP_DISCHARGING_THRESHOLD) *
			fuelgauge->pdata->temp_cohot / 1000);
	else if (temp < RCOMP_DISCHARGING_THRESHOLD)
		new_rcomp = starting_rcomp + ((temp - RCOMP_DISCHARGING_THRESHOLD) *
			fuelgauge->pdata->temp_cocold / 1000);
	else
		new_rcomp = starting_rcomp;

	if (new_rcomp > 255)
		new_rcomp = 255;
	else if (new_rcomp < 0)
		new_rcomp = 0;

	new_rcomp <<= 8;
	new_rcomp &= 0xff00;
	/* not related to RCOMP */
	new_rcomp |= (rcomp_current & 0xff);

	if (rcomp_current != new_rcomp) {
		dev_dbg(&client->dev,
			"%s : RCOMP 0x%04x -> 0x%04x (0x%02x)\n",
			__func__, rcomp_current, new_rcomp,
			new_rcomp >> 8);
		max17048_fuelgauge_set_rcomp(client, new_rcomp);
	}
}

static int max17048_fuelgauge_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);

	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17048_fuelgauge_get_vcell(fuelgauge->client);
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Average Current (mA) */
		val->intval = max17048_fuelgauge_get_current(fuelgauge->client);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max17048_fuelgauge_get_soc(fuelgauge->client) / 10;
		/* capacity should be between 0% and 100%
		 * (0.1% degree)
		 */
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		/* get only integer part */
		val->intval /= 10;

		/* check whether doing the wake_unlock */
		if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
				fuelgauge->is_fuel_alerted) {
			wake_unlock(&fuelgauge->fuel_alert_wake_lock);
			max17048_fuelgauge_fuelalert_init(fuelgauge->client,
					fuelgauge->pdata->fuel_alert_soc);
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		return -ENODATA;
	default:
		return -EINVAL;
	}
	return 0;
}

bool max17048_fuelgauge_full_charged(struct i2c_client *client)
{
	return true;
}

static int max17048_fuelgauge_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			max17048_fuelgauge_full_charged(fuelgauge->client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* temperature is 0.1 degree, should be divide by 10 */
		max17048_fuelgauge_rcomp_update(fuelgauge->client, val->intval / 10);
		break;
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

bool max17048_fuelgauge_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

static void max17048_fuelgauge_isr_work(struct work_struct *work)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(work, struct max17048_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	max17048_fuelgauge_fuelalert_process(fuelgauge, fuelgauge->is_fuel_alerted);
}

bool max17048_fuelgauge_is_fuelalerted(struct i2c_client *client)
{
	u16 temp;

	temp = max17048_fuelgauge_get_rcomp(client);

	if (temp & 0x20)	/* ALRT is asserted */
		return true;

	return false;
}

static irqreturn_t max17048_fuelgauge_irq_thread(int irq, void *irq_data)
{
	struct max17048_fuelgauge_data *fuelgauge = irq_data;
	bool fuel_alerted;

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		fuel_alerted =
			max17048_fuelgauge_is_fuelalerted(fuelgauge->client);

		dev_info(&fuelgauge->client->dev,
			"%s: Fuel-alert %salerted!\n",
			__func__, fuel_alerted ? "" : "NOT ");

		if (fuel_alerted == fuelgauge->is_fuel_alerted) {
			if (!fuelgauge->pdata->repeated_fuelalert) {
				dev_dbg(&fuelgauge->client->dev,
					"%s: Fuel-alert Repeated (%d)\n",
					__func__, fuelgauge->is_fuel_alerted);
				return IRQ_HANDLED;
			}
		}

		if (fuel_alerted)
			wake_lock(&fuelgauge->fuel_alert_wake_lock);
		else
			wake_unlock(&fuelgauge->fuel_alert_wake_lock);

		schedule_delayed_work(&fuelgauge->isr_work, 0);

		fuelgauge->is_fuel_alerted = fuel_alerted;
	}

	return IRQ_HANDLED;
}

bool max17048_fuelgauge_initialization(struct i2c_client *client)
{
	max17048_fuelgauge_get_version(client);

	return true;
}

#ifdef CONFIG_OF
static int max17048_fuelgauge_parse_dt(struct device *dev,
			struct max17048_fuelgauge_data *fuelgauge)
{
	struct device_node *np = dev->of_node;
	struct max17048_fuelgauge_platform_data *pdata = fuelgauge->pdata;
	int ret;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,rcomp_charging",
				&pdata->rcomp_charging);
		if (ret < 0)
			pr_err("%s error reading pdata->rcomp_charging %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,rcomp_discharging",
				&pdata->rcomp_discharging);
		if (ret < 0)
			pr_err("%s error reading pdata->rcomp_discharging %d\n",
					__func__, ret);

		ret = of_get_named_gpio(np, "fuelgauge,fg_irq", 0);
		if (ret > 0) {
			pdata->fg_irq = ret;
			pr_info("%s reading fg_irq = %d\n", __func__, ret);
		}

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		pdata->repeated_fuelalert = of_property_read_bool(np, "fuelgauge,repeated_fuelalert");

		ret = of_property_read_u32(np, "fuelgauge,temp_cohot",
				&pdata->temp_cohot);
		if (ret < 0)
			pr_err("%s error reading pdata->temp_cohot %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,temp_cocold",
				&pdata->temp_cocold);
		if (ret < 0)
			pr_err("%s error reading pdata->temp_cocold %d\n",
					__func__, ret);

		pdata->is_using_model_data = of_property_read_bool(np, "fuelgauge,is_using_model_data");

		pr_info("%s: rcomp_charging : %d, rcomp_discharging: %d, fg_irq: %d, "
				"fuel_alert_soc: %d,\n"
				"is_using_model_data: %d\n",
				__func__, pdata->rcomp_charging , pdata->rcomp_discharging, pdata->fg_irq,
				pdata->fuel_alert_soc, pdata->is_using_model_data
				);
	}
	return 0;
}
#else
static int max17048_fuelgauge_parse_dt(struct device *dev,
			struct synaptics_rmi4_power_data *pdata)
{
	return -ENODEV;
}
#endif

static int max17048_fuelgauge_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17048_fuelgauge_data *fuelgauge;
	struct max17048_fuelgauge_platform_data *pdata = NULL;
	int ret = 0;

	dev_info(&client->dev,
		"%s: MAX17048 Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->client = client;

	if (client->dev.of_node) {
		int error;
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct max17048_fuelgauge_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_free;
		}

		fuelgauge->pdata = pdata;
		error = max17048_fuelgauge_parse_dt(&client->dev, fuelgauge);
		if (error) {
			dev_err(&client->dev,
				"%s: Failed to get fuel_int\n", __func__);
		}
	} else	{
		dev_err(&client->dev,
			"%s: Failed to get of_node\n", __func__);
		fuelgauge->pdata = client->dev.platform_data;
	}
	i2c_set_clientdata(client, fuelgauge);

	if (!max17048_fuelgauge_initialization(fuelgauge->client)) {
		dev_err(&client->dev,
			"%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_devm_free;
	}

	fuelgauge->psy_fg.name		= "max17048-fuelgauge";
	fuelgauge->psy_fg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property	= max17048_fuelgauge_get_property;
	fuelgauge->psy_fg.set_property	= max17048_fuelgauge_set_property;
	fuelgauge->psy_fg.properties	= max17048_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
		ARRAY_SIZE(max17048_fuelgauge_props);

	ret = power_supply_register(&client->dev, &fuelgauge->psy_fg);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_fg\n", __func__);
		goto err_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (max17048_fuelgauge_fuelalert_init(fuelgauge->client,
			fuelgauge->pdata->fuel_alert_soc))
			wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
				WAKE_LOCK_SUSPEND, "fuel_alerted");
		else {
			dev_err(&client->dev,
				"%s: Failed to Initialize Fuel-alert\n",
				__func__);
			goto err_irq;
		}
	}

	if (fuelgauge->pdata->fg_irq > 0) {
		INIT_DELAYED_WORK(
			&fuelgauge->isr_work, max17048_fuelgauge_isr_work);

		fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
		dev_info(&client->dev,
			"%s: fg_irq = %d\n", __func__, fuelgauge->fg_irq);
		if (fuelgauge->fg_irq > 0) {
			ret = request_threaded_irq(fuelgauge->fg_irq,
					NULL, max17048_fuelgauge_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING
					 | IRQF_ONESHOT,
					"fuelgauge-irq", fuelgauge);
			if (ret) {
				dev_err(&client->dev,
					"%s: Failed to Reqeust IRQ\n", __func__);
				goto err_supply_unreg;
			}

			ret = enable_irq_wake(fuelgauge->fg_irq);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
		} else {
			dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n",
				__func__, fuelgauge->fg_irq);
			goto err_supply_unreg;
		}
	}

	dev_info(&client->dev,
		"%s: MAX17048 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_irq:
	if (fuelgauge->fg_irq > 0)
		free_irq(fuelgauge->fg_irq, fuelgauge);
	wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);
err_supply_unreg:
	power_supply_unregister(&fuelgauge->psy_fg);
err_devm_free:
	if (pdata)
		devm_kfree(&client->dev, pdata);
err_free:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	dev_info(&client->dev, "%s: Fuel gauge probe failed\n", __func__);
	return ret;
}

static int max17048_fuelgauge_remove(
						struct i2c_client *client)
{
	struct max17048_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

static int max17048_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int max17048_fuelgauge_resume(struct device *dev)
{
	return 0;
}

static void max17048_fuelgauge_shutdown(struct i2c_client *client)
{
	u16 data;

	data = 0xFFFF;
	i2c_smbus_write_word_data(client,
			MAX17048_HIBERNATE, swab16(data));
}

static const struct i2c_device_id max17048_fuelgauge_id[] = {
	{"max17048-fuelgauge", 0},
	{}
};

static const struct dev_pm_ops max17048_fuelgauge_pm_ops = {
	.suspend = max17048_fuelgauge_suspend,
	.resume  = max17048_fuelgauge_resume,
};

MODULE_DEVICE_TABLE(i2c, max17048_fuelgauge_id);

static struct of_device_id fuelgauge_i2c_match_table[] = {
	{ .compatible = "maxim,max17048", },
	{ },
};
MODULE_DEVICE_TABLE(i2c, fuelgauge_i2c_match_table);

static struct i2c_driver max17048_fuelgauge_driver = {
	.driver = {
		   .name = "max17048-fuelgauge",
		   .owner = THIS_MODULE,
		   .of_match_table = fuelgauge_i2c_match_table,
#ifdef CONFIG_PM
		   .pm = &max17048_fuelgauge_pm_ops,
#endif
	},
	.probe	= max17048_fuelgauge_probe,
	.remove	= max17048_fuelgauge_remove,
	.shutdown   = max17048_fuelgauge_shutdown,
	.id_table   = max17048_fuelgauge_id,
};

static int __init max17048_fuelgauge_init(void)
{
	return i2c_add_driver(&max17048_fuelgauge_driver);
}

static void __exit max17048_fuelgauge_exit(void)
{
	i2c_del_driver(&max17048_fuelgauge_driver);
}

module_init(max17048_fuelgauge_init);
module_exit(max17048_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
