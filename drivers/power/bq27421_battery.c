/*
* Fuel gauge driver for TI BQ27421
*
* Copyright(c) 2014, LGE Inc. All rights reserved.
* ChoongRyeol Lee <choongryeol.lee@lge.com>
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/power/bq27421_battery.h>

#define RETRY_CNT_EXIT_CFGUPDATE 100
#define RETRY_CNT_ENTER_CFGUPDATE 10

#define SOC_LOW_THRESHOLD  3

struct bq27421_chip {
	struct i2c_client *client;
	struct power_supply battery;
	struct bq27421_platform_data *pdata;
	struct mutex mutex;
	struct timespec next_update_time;
	bool suspended;
	bool power_supply_registered;
	bool irq_wake_enabled;
	int vcell;
	int soc;
	int current_now;
	int temperature;
};

static int bq27421_read_byte(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	pr_debug("%s: [%x]=%x\n", __func__, reg, ret);

	return ret;
}

static int bq27421_read_word(struct i2c_client *client, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	pr_debug("%s: [%x]=%x\n", __func__, reg, ret);

	return ret;
}

static int bq27421_write_byte(struct i2c_client *client, u8 reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	pr_debug("%s: [%x]=%x\n", __func__, reg, value);

	return ret;
}

static int bq27421_write_word(struct i2c_client *client, u8 reg, u16 value)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	pr_debug("%s: [%x]=%x\n", __func__, reg, value);

	return ret;
}

static int bq27421_get_ctrl_status(struct bq27421_chip *chip)
{
	struct i2c_client *client = chip->client;

	bq27421_write_word(client, BQ27421_CONTROL,
					BQ27421_SCMD_CTRL_STAT);
	udelay(66);

	return bq27421_read_word(client, BQ27421_CONTROL);
}

static bool bq27421_is_unsealed(struct bq27421_chip *chip)
{
	int status;

	status = bq27421_get_ctrl_status(chip);
	if (status < 0) {
		pr_err("%s: failed to get control status\n", __func__);
		return false;
	}

	return !(status & 0x2000);
}

static bool bq27421_ctrl_unsealed(struct bq27421_chip *chip)
{
	bq27421_write_word(chip->client, BQ27421_CONTROL, UNSEAL_KEY0);
	bq27421_write_word(chip->client, BQ27421_CONTROL, UNSEAL_KEY1);

	msleep(100);

	return bq27421_is_unsealed(chip);
}

static bool bq27421_ctrl_sealed(struct bq27421_chip *chip)
{
	bq27421_write_word(chip->client, BQ27421_CONTROL,
					BQ27421_SCMD_SEALED);

	msleep(100);

	return !bq27421_is_unsealed(chip);
}

static bool bq27421_is_cfgupdate(struct bq27421_chip *chip)
{
	int ret;

	ret = bq27421_read_word(chip->client, BQ27421_FLAGS);
	if (ret < 0) {
		pr_err("%s: failed to get flags\n", __func__);
		return false;
	}

	return !!(ret & 0x0010);
}

static bool bq27421_enter_cfgupdate(struct bq27421_chip *chip)
{
	int retry = 0;

	if (bq27421_is_cfgupdate(chip))
		return true;

	do {
		bq27421_write_word(chip->client, BQ27421_CONTROL,
						BQ27421_SCMD_CFGUPDATE);
		if (bq27421_is_cfgupdate(chip)) {
			return true;
		} else {
			retry++;
			msleep(100);
		}
	} while (retry < RETRY_CNT_ENTER_CFGUPDATE);

	return false;

}

static bool bq27421_exit_cfgupdate(struct bq27421_chip *chip)
{
	int retry = 0;

	if (!bq27421_is_cfgupdate(chip))
		return true;

	do {
		bq27421_write_word(chip->client, BQ27421_CONTROL,
					BQ27421_SCMD_EXIT_CFGUPDATE);
		if (!bq27421_is_cfgupdate(chip)) {
			return true;
		} else {
			retry++;
			msleep(100);
		}
	} while (retry < RETRY_CNT_EXIT_CFGUPDATE);

	return false;
}

/* Must call in config update mode */
static void bq27421_data_mem_write_byte(struct i2c_client *client,
					u8 dclass, u8 block, u8 offset,
					u8 mask, u8 value)
{
	int checksum;
	int read_block;
	u8 prev_value;

	bq27421_write_byte(client, BQ27421_ECMD_DATACLASS, dclass);
	bq27421_write_byte(client, BQ27421_ECMD_DATABLK, block);

	msleep(10);

	checksum = bq27421_read_byte(client, BQ27421_ECMD_BLKCHECKSUM);
	if (checksum < 0)
		return;

	read_block = bq27421_read_byte(client, BQ27421_ECMD_BLKDATA + offset);
	if (read_block < 0)
		return;

	prev_value = read_block;
	read_block &= ~mask;
	read_block |= value & mask;
	checksum = checksum + prev_value - read_block;

	bq27421_write_byte(client, BQ27421_ECMD_BLKDATA + offset,
							(u8)read_block);
	bq27421_write_byte(client, BQ27421_ECMD_BLKCHECKSUM, (u8)checksum);
}

static void bq27421_set_pwr_off_data(struct bq27421_chip *chip)
{
	mutex_lock(&chip->mutex);

	if (!bq27421_ctrl_unsealed(chip)) {
		pr_err("%s: failed to set unseal\n", __func__);
		mutex_unlock(&chip->mutex);
		return;
	}

	if (!bq27421_enter_cfgupdate(chip)) {
		pr_err("%s: failed to enter config mode\n", __func__);
		mutex_unlock(&chip->mutex);
		return;
	}

	bq27421_write_byte(chip->client, BQ27421_ECMD_BLKCTRL, 0x00);
	/* Disable wake comparator in sleep to reduce leakage current */
	bq27421_data_mem_write_byte(chip->client, SUBCLASS_REGISTERS,
					0x00, OFFSET_OPCONFIG, 0x05, 0x00);
	/* Keep unseal state after exiting config mode */
	bq27421_data_mem_write_byte(chip->client, SUBCLASS_STATE,
					0x00, OFFSET_UPDATESTAT, 0x80, 0x00);

	if (!bq27421_exit_cfgupdate(chip))
		pr_err("%s: failed to exit config model\n", __func__);

	mutex_unlock(&chip->mutex);
}

/* Must call with mutex locked */
static void bq27421_update(struct bq27421_chip *chip)
{
	struct i2c_client *client = chip->client;
	int ret;

	ret = bq27421_read_word(client, BQ27421_TEMP);
	if (ret >= 0)
		chip->temperature = ret;

	ret = bq27421_read_word(client, BQ27421_VOLT);
	if (ret >= 0)
		chip->vcell = ret;

	ret = bq27421_read_word(client, BQ27421_SCURRENT);
	if (ret >= 0)
		chip->current_now = ret;

	ret = bq27421_read_word(client, BQ27421_SOC);
	if (ret >= 0)
		chip->soc = ret;

	/* next update must be at least 1 second later */
	ktime_get_ts(&chip->next_update_time);
	monotonic_to_bootbased(&chip->next_update_time);
	chip->next_update_time.tv_sec++;
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
	struct timespec now;

	if (chip->suspended)
		return -EAGAIN;

	ktime_get_ts(&now);
	monotonic_to_bootbased(&now);
	if (timespec_compare(&now, &chip->next_update_time) >= 0) {
		mutex_lock(&chip->mutex);
		bq27421_update(chip);
		mutex_unlock(&chip->mutex);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* Convert 0.1 Kelvin unit to 0.1 Celsius */
		val->intval = chip->temperature - 2731;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = (s16)chip->current_now;
		val->intval *= 1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t bq27421_interrupt(int irq, void *data)
{
	struct bq27421_chip *chip = data;

	pr_debug("%s : interupt occured\n", __func__);
	power_supply_changed(&chip->battery);

	return IRQ_HANDLED;
}

static struct bq27421_platform_data *bq27421_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct bq27421_platform_data *pdata;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->ext_batt_psy =
			of_property_read_bool(np, "ti,ext_batt_psy");

	return pdata;
}

static int bq27421_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq27421_chip *chip;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
						I2C_FUNC_SMBUS_BYTE_DATA |
						I2C_FUNC_SMBUS_I2C_BLOCK))
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

	if (bq27421_is_unsealed(chip))
		bq27421_ctrl_sealed(chip);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					bq27421_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (ret) {
			pr_err("%s: cannot enable irq\n", __func__);
			return ret;
		}
	}

	if (power_supply_register(&client->dev, &chip->battery))
		pr_err("%s: failed power supply register\n", __func__);
	else
		chip->power_supply_registered = true;

	return ret;
}

static int bq27421_remove(struct i2c_client *client)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	if (client->irq)
		disable_irq_wake(client->irq);

	if (chip->power_supply_registered)
		power_supply_unregister(&chip->battery);

	i2c_set_clientdata(client, NULL);
	devm_kfree(&client->dev, chip);

	return 0;
}

static void bq27421_shutdown(struct i2c_client *client)
{
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	/* Set params for device shutdown */
	bq27421_set_pwr_off_data(chip);
}

static int bq27421_pm_prepare(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	chip->suspended = true;

	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		if (chip->soc < SOC_LOW_THRESHOLD) {
			enable_irq_wake(chip->client->irq);
			chip->irq_wake_enabled = true;
		}
	}

	return 0;
}

static void bq27421_pm_complete(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq27421_chip *chip = i2c_get_clientdata(client);

	chip->suspended = false;

	if (chip->client->irq) {
		if (chip->irq_wake_enabled) {
			disable_irq_wake(chip->client->irq);
			chip->irq_wake_enabled = false;
		}
		enable_irq(chip->client->irq);
	}
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
	.probe  = bq27421_probe,
	.remove  = bq27421_remove,
	.id_table  = bq27421_id,
	.shutdown  = bq27421_shutdown,
};

module_i2c_driver(bq27421_i2c_driver);

MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("BQ27421 Fuel Gauge");
MODULE_LICENSE("GPL");
