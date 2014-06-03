/*
 *  bq27421_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/power/bq27421_battery.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#ifdef BQ27421_DEBUG
#define bq27421_dbg(x, ...) pr_info("bq27421: " x, ##__VA_ARGS__)
#endif

struct bq27421_chip {
	struct i2c_client *client;
	struct power_supply battery;
	struct bq27421_platform_data *pdata;
	struct mutex mutex;
	bool suspended;
	bool power_supply_registered;
	int status;
	int vcell;
	int last_vcell;
	int soc;
	int last_soc;
	int capacity_level;
	int average_current;
	int remaining_capacity;
	int full_charge_capacity;
	int temperature;
	int soh;
	struct delayed_work socint_work;
#ifdef BQ27421_DEBUG
	int PresentDOD;
	int OCVCurrent;
	int OCVVoltage;
	int RemainingCapacityUnfiltered;
	int FullChargeCapacityUnfiltered;
	int DOD0;
	int PassedCharge;
	int Qstart;
#endif
};

static int bq27421_read_word(struct i2c_client *client, int reg)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);
	int ret;

	if (!client->adapter)
		return -ENODEV;

	mutex_lock(&chip->mutex);
	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev,
			"%s(): Failed in reading register 0x%02x err %d\n",
			__func__, reg, ret);
	}

	mutex_unlock(&chip->mutex);
	return ret;
}

static int bq27421_read(struct i2c_client *client, u8 reg, int *rt_value, int b_single, int b_dataflash)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	u8 data[2];
	int err;

	if (!client->adapter)
		return -ENODEV;

	mutex_lock(&chip->mutex);

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;

	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0) {
		if (!b_single)
			msg->len = 2;
		else
			msg->len = 1;

		msg->flags = I2C_M_RD;

		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0) {
			if (!b_single) {
				if (unlikely(b_dataflash))
					*rt_value = get_unaligned_be16(data);
				else
					*rt_value = get_unaligned_le16(data);
			}
			else {
				*rt_value = data[0];
			}

			mutex_unlock(&chip->mutex);
			return 0;
		}
		else {
			pr_err("%s: err = %d\n", __func__, err);
		}
	}

	mutex_unlock(&chip->mutex);
	return err;
}

static int bq27421_i2c_txsubcmd(u8 reg, u16 subcmd,
		struct i2c_client *client)
{
	struct i2c_msg msg;
	u8 data[3];
	int ret;

	if (!client)
		return -ENODEV;

	memset(data, 0, sizeof(data));
	data[0] = reg;
	data[1] = subcmd & 0x00FF;
	data[2] = (subcmd & 0xFF00) >> 8;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		return -EIO;

	return 0;
}

static void bq27421_cntl_cmd(struct i2c_client *client, int subcmd)
{
	int ret;

	ret = bq27421_i2c_txsubcmd(BQ27421_REG_CONTROL, subcmd, client);

	if (ret < 0)
		pr_err("%s: err %d\n", __func__, ret);
}

static int bq27421_get_status(struct bq27421_chip *chip)
{
	struct i2c_client *client = chip->client;
	int ret;

	bq27421_cntl_cmd(client, BQ27421_SUBCMD_CTNL_STATUS);
	udelay(66);

	ret = bq27421_read(client, BQ27421_REG_CONTROL, &chip->status, 0, 0);

	if (ret < 0) {
		pr_err("%s: err %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void bq27421_get_value(struct bq27421_chip *chip, int reg)
{
	struct i2c_client *client = chip->client;
	int value;

	if (chip->suspended)
		return;

	value = bq27421_read_word(client, reg);
	if (value < 0) {
		pr_err("%s: error reading values (%d)\n", __func__, value);
		return;
	}

	switch(reg) {
		case BQ27421_REG_TEMP:
			chip->temperature = value;
			chip->temperature = (chip->temperature * 10) - 27315;
			break;
		case BQ27421_REG_VOLT:
			chip->vcell = value;
			break;
		case BQ27421_REG_RC:
			chip->remaining_capacity = value;
			break;
		case BQ27421_REG_FCC:
			chip->full_charge_capacity = value;
			break;
		case BQ27421_REG_AC:
			chip->average_current = value;
			break;
		case BQ27421_REG_SOC:
			chip->soc = value;
			break;
		case BQ27421_REG_SOH:
			chip->soh = value;
			break;
#ifdef BQ27421_DEBUG
		case BQ27421_REG_P_DOD:
			chip->PresentDOD = value;
			break;
		case BQ27421_REG_OCV_I:
			chip->OCVCurrent = value;
			break;
		case BQ27421_REG_OCV_V:
			chip->OCVVoltage = value;
			break;
		case BQ27421_REG_REMCAP_UNF:
			chip->RemainingCapacityUnfiltered = value;
			break;
		case BQ27421_REG_FULLCAP_UNF:
			chip->FullChargeCapacityUnfiltered = value;
			break;
		case BQ27421_REG_DOD0:
			chip->DOD0 = value;
			break;
		case BQ27421_REG_PASSEDCHG:
			chip->PassedCharge = value;
			break;
		case BQ27421_REG_QSTART:
			chip->Qstart = value;
			break;
#endif
		default:
			return;
	}
}

static enum power_supply_property bq27421_battery_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int bq27421_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bq27421_chip *chip = container_of(psy,
				struct bq27421_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		bq27421_get_value(chip, BQ27421_REG_VOLT);
		val->intval = chip->vcell;
		val->intval *= 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		bq27421_get_value(chip, BQ27421_REG_SOC);
		val->intval = chip->soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		bq27421_get_value(chip, BQ27421_REG_TEMP);
		val->intval = chip->temperature;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		bq27421_get_value(chip, BQ27421_REG_AC);
		val->intval = (s16)chip->average_current;
		val->intval *= -1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t bq27421_interrupt_handler(int irq, void *data)
{
	struct bq27421_chip *chip = data;

	pr_debug("%s : interupt occured\n", __func__);

	power_supply_changed(&chip->battery);
	return IRQ_HANDLED;
}

static struct bq27421_platform_data *
bq27421_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct bq27421_platform_data *pdata;
	u32 prop;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->ext_batt_psy = of_property_read_bool(np, "bq27421,ext_batt_psy");

	if (of_property_read_u32(np, "bq27421,full_design", &prop) == 0)
		pdata->full_design = prop;

	return pdata;
}

static void bq27421_socint_work(struct work_struct *work)
{
	struct bq27421_chip *chip = container_of(work, struct bq27421_chip, socint_work.work);
	static struct power_supply *batt_psy;
	union power_supply_propval batt_temp = {0, };
	int ret;
	int flags;

	/* Update recently VCELL, SOC and CAPACITY */
	bq27421_get_value(chip, BQ27421_REG_TEMP);
	bq27421_get_value(chip, BQ27421_REG_VOLT);
	bq27421_get_value(chip, BQ27421_REG_RC);
	bq27421_get_value(chip, BQ27421_REG_FCC);
	bq27421_get_value(chip, BQ27421_REG_AC);
	bq27421_get_value(chip, BQ27421_REG_SOC);
	bq27421_get_value(chip, BQ27421_REG_SOH);

#ifdef BQ27421_DEBUG
	bq27421_get_value(chip, BQ27421_REG_P_DOD);
	bq27421_get_value(chip, BQ27421_REG_OCV_I);
	bq27421_get_value(chip, BQ27421_REG_OCV_V);
	bq27421_get_value(chip, BQ27421_REG_REMCAP_UNF);
	bq27421_get_value(chip, BQ27421_REG_FULLCAP_UNF);
	bq27421_get_value(chip, BQ27421_REG_DOD0);
	bq27421_get_value(chip, BQ27421_REG_PASSEDCHG);
	bq27421_get_value(chip, BQ27421_REG_QSTART);
#endif

	ret = bq27421_read(chip->client, BQ27421_REG_FLAGS, &flags, 0, 0);
	if (ret < 0)
		pr_err("%s: error reading flags\n", __func__);

	ret = bq27421_get_status(chip);
	if (ret < 0)
		pr_err("%s: error get status register\n", __func__);

#ifdef BQ27421_DEBUG
	bq27421_dbg("Flags: 0x%04x, Status: 0x%04x T: %d, V: %d, RC: %d, FCC: %d, AC: %d, SOC: %d, SOH: %d\n", flags, chip->status, chip->temperature, chip->vcell,
		chip->remaining_capacity, chip->full_charge_capacity, (s16)chip->average_current, chip->soc, chip->soh);

	bq27421_dbg("PDOD: %d, OCVC: %d, OCVV:%d, RCU: %d, FCCU: %d, DOD0: %d, PassedCharge: %d, QS: %d\n",
		chip->PresentDOD, (s16)chip->OCVCurrent, chip->OCVVoltage, chip->RemainingCapacityUnfiltered, chip->FullChargeCapacityUnfiltered, chip->DOD0, (s16)chip->PassedCharge, chip->Qstart);
#endif

	if (chip->soc != chip->last_soc &&
		chip->vcell != chip->last_vcell) {
		chip->last_vcell = chip->vcell;
		chip->last_soc = chip->soc;

		if (!batt_psy)
			batt_psy = power_supply_get_by_name("battery");

		if (batt_psy) {
			batt_psy->get_property(batt_psy,
				  POWER_SUPPLY_PROP_TEMP, &batt_temp);
		}

		power_supply_changed(&chip->battery);
		pr_info("%s: battery l=%d v=%d t=%d.%d c=%d\n", __func__, chip->soc, chip->vcell, batt_temp.intval / 10, batt_temp.intval % 10, (s16)chip->average_current * -1);
	}

	schedule_delayed_work(&chip->socint_work, msecs_to_jiffies(SOC_WORK_PERIOD));

	return;
}

#ifdef BQ27421_DEBUG
static int bq27421_ext_cmd(struct i2c_client *client, u8 extcmd, u8 data)
{
	struct i2c_msg msg;
	u8 i2c_data[2];
	int ret;

	if (!client)
		return -ENODEV;

	memset(i2c_data, 0, sizeof(i2c_data));
	i2c_data[0] = extcmd;
	i2c_data[1] = data;

	msg.addr = client->addr;

	msg.flags = 0;
	msg.len = 2;
	msg.buf = i2c_data;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("%s: i2c_transfer fails.\n", __func__);

	return ret;
}

static bool bq27421_is_device_unsealed(struct i2c_client *client)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);
	int ret;
	bool is_unsealed = false;

	ret = bq27421_get_status(chip);
	if (ret < 0)
		pr_err("%s: error get status register\n", __func__);

	if ((chip->status & 0x2000) == 0)
		is_unsealed = true;

	return is_unsealed;
}

static bool bq27421_make_device_unsealed(struct i2c_client *client)
{
	bool is_unsealed;

	bq27421_cntl_cmd(client, UNSEAL_KEY0);
	bq27421_cntl_cmd(client, UNSEAL_KEY1);
	mdelay(100);

	is_unsealed = bq27421_is_device_unsealed(client);

	return is_unsealed;
}

static bool bq27421_make_device_sealed(struct i2c_client *client)
{
	bool is_sealed;

	bq27421_cntl_cmd(client, BQ27421_SUBCMD_SEALED);
	mdelay(100);

	is_sealed = !bq27421_is_device_unsealed(client);

	return is_sealed;
}

static int bq27421_blockdata_read(struct i2c_client *client, char *read_buf, int count)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	int err;
	int i;

	if (!client->adapter)
		return -ENODEV;

	mutex_lock(&chip->mutex);

	msg->addr = client->addr;
	msg->flags = I2C_M_RD;
	msg->len = count;
	msg->buf = read_buf;

	read_buf[0] = BQ27421_EXTCMD_BLOCKDATA;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err < 0)
		pr_err("%s: i2c_transfer fails. err = %d\n", __func__, err);
	else
		for (i = 0; i < count; i++)
			bq27421_dbg("%02d, 0x%02x\n", i, read_buf[i]);

	mutex_unlock(&chip->mutex);
	return err;
}

static void bq27421_check_blockdata(struct i2c_client *client)
{
	u8 DataRAM_RB[32] = {0, };

	if (bq27421_make_device_unsealed(client) == false) {
		pr_err("%s: unseal fails\n", __func__);
		return;
	}

	bq27421_ext_cmd(client, BQ27421_EXTCMD_BLOCKDATACONTROL, 0x00);

	bq27421_dbg("reading SubClass 64...\n");
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATACLASS, 64);
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATABLOCK, 0);
	bq27421_blockdata_read(client, DataRAM_RB, 32);

	bq27421_dbg("reading SubClass 82...\n");
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATACLASS, 82);
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATABLOCK, 0);
	bq27421_blockdata_read(client, DataRAM_RB, 32);

	bq27421_dbg("reading SubClass 82-1...\n");
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATACLASS, 82);
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATABLOCK, 1);
	bq27421_blockdata_read(client, DataRAM_RB, 32);

	bq27421_dbg("reading SubClass 89...\n");
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATACLASS, 89);
	bq27421_ext_cmd(client, BQ27421_EXTCMD_DATABLOCK, 0);
	bq27421_blockdata_read(client, DataRAM_RB, 32);

	if (bq27421_make_device_sealed(client) == false) {
		pr_err("%s: seal fails\n", __func__);
		return;
	}
}
#endif

static int bq27421_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq27421_chip *chip;
	int ret;

	pr_info("%s: START\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = bq27421_get_pdata(&client->dev);
	if (!chip->pdata) {
		pr_err("%s: no platform data provided\n", __func__);
		devm_kfree(&client->dev, chip);
		return -EINVAL;
	}

	mutex_init(&chip->mutex);
	i2c_set_clientdata(client, chip);

#ifdef BQ27421_DEBUG
	bq27421_check_blockdata(chip->client);
#endif

	/*
	 * If ext_batt_psy is true, then an external device publishes
	 * a POWER_SUPPLY_TYPE_BATTERY, so this driver will publish its
	 * data via the POWER_SUPPLY_TYPE_BMS type.
	 */
	if (chip->pdata->ext_batt_psy) {
		chip->battery.name = "bms";
		chip->battery.type = POWER_SUPPLY_TYPE_BMS;
	} else {
		chip->battery.name = "battery";
		chip->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	}

	chip->battery.get_property = bq27421_get_property;
	chip->battery.properties = bq27421_battery_props;
	chip->battery.num_properties = ARRAY_SIZE(bq27421_battery_props);

	/* check if BQ27421 Initialization is completed */
	ret = bq27421_get_status(chip);
	if (ret < 0) {
		pr_err("%s: bq27421_get_status fails\n", __func__);
		return -EIO;
	}

	if ((chip->status & 0x0080) == 0) {
		pr_err("%s: init not completed yet\n", __func__);
		return -EIO;
	}

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					bq27421_interrupt_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (ret) {
			pr_err("%s: cannot enable irq\n", __func__);
			return ret;
		} else {
			enable_irq_wake(client->irq);
		}
	}

	if (power_supply_register(&client->dev, &chip->battery))
		pr_err("%s: failed power supply register\n", __func__);
	else
		chip->power_supply_registered = true;

	INIT_DELAYED_WORK(&chip->socint_work, bq27421_socint_work);
	schedule_delayed_work(&chip->socint_work, msecs_to_jiffies(SOC_WORK_PERIOD));

	pr_info("%s: DONE\n", __func__);

	return ret;
}

static int bq27421_remove(struct i2c_client *client)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->socint_work);

	if (client->irq)
		disable_irq_wake(client->irq);

	if (chip->power_supply_registered)
		power_supply_unregister(&chip->battery);

	i2c_set_clientdata(client, NULL);
	devm_kfree(&client->dev, chip);

	return 0;
}

static int bq27421_pm_prepare(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->socint_work);

	chip->suspended = true;

	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}

	return 0;
}

static void bq27421_pm_complete(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	chip->suspended = false;

	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
	}

	schedule_delayed_work(&chip->socint_work, msecs_to_jiffies(200));
}

static const struct dev_pm_ops bq27421_pm_ops = {
	.prepare = bq27421_pm_prepare,
	.complete = bq27421_pm_complete,
};

static struct of_device_id bq27421_match_table[] = {
	{ .compatible = "ti,bq27421", },
	{ },
};
MODULE_DEVICE_TABLE(of, bq27421_match_table);

static const struct i2c_device_id bq27421_id[] = {
	{ "bq27421", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq27421_id);

static struct i2c_driver bq27421_i2c_driver = {
	.driver	= {
		.name = "bq27421",
		.owner = THIS_MODULE,
		.of_match_table = bq27421_match_table,
		.pm = &bq27421_pm_ops,
	},
	.probe		= bq27421_probe,
	.remove		= bq27421_remove,
	.id_table	= bq27421_id,
};

module_i2c_driver(bq27421_i2c_driver);

MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("BQ27421 Fuel Gauge");
MODULE_LICENSE("GPL");
