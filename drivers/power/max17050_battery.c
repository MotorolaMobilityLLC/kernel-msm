/*
 * Fuel gauge driver for Maxim 17050
 *
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on max17042_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/power/max17050_battery.h>
#include <linux/of.h>

/* Status register bits */
#define STATUS_POR_BIT		(1 << 1)
#define STATUS_BI_BIT		(1 << 11)
#define STATUS_BR_BIT		(1 << 15)

/* Interrupt config/status bits */
#define CFG_ALRT_BIT_ENBL	(1 << 2)
#define CFG_EXT_TEMP_BIT	(1 << 8)
#define STATUS_INTR_SOCMIN_BIT	(1 << 10)
#define STATUS_INTR_SOCMAX_BIT	(1 << 14)

/* External battery power supply poll times */
#define EXT_BATT_FAST_PERIOD	100
#define EXT_BATT_SLOW_PERIOD	10000

/* Modelgauge M3 save/restore values */
struct max17050_learned_params {
	u16 rcomp0;
	u16 tempco;
	u16 fullcap;
	u16 fullcapnom;
	u16 iavg_empty;
	u16 qrtable00;
	u16 qrtable10;
	u16 qrtable20;
	u16 qrtable30;
	u16 cycles;
	u16 param_version;
};

struct max17050_chip {
	struct i2c_client *client;
	struct power_supply battery;
	struct power_supply *ext_battery;
	bool power_supply_registered;
	struct max17050_platform_data *pdata;
	struct work_struct work;
	struct delayed_work ext_batt_work;
	struct mutex mutex;
	struct timespec next_update_time;
	bool suspended;
	bool use_ext_temp;
	int ext_temp;
	bool init_done;
	bool power_on_reset;

	/* values read from chip */
	u16 status;
	u16 vcell;
	u16 repcap;
	u16 repsoc;
	u16 temp;
	u16 current_now;
	u16 tte;

	struct max17050_learned_params learned;
};

static int max17050_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	dev_dbg(&client->dev, "max17050 write: [%x]=%x", reg, value);

	return ret;
}

static int max17050_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	dev_dbg(&client->dev, "max17050 read: [%x]=%x", reg, ret);

	return ret;
}

static int max17050_write_verify_reg(struct i2c_client *client, u8 reg,
								u16 value)
{
	int ret;
	int read;

	ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	read = i2c_smbus_read_word_data(client, reg);

	if (read < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	if (read != value) {
		dev_err(&client->dev, "%s: expected 0x%x, got 0x%x\n",
			__func__, value, read);
		return -EIO;
	}

	dev_dbg(&client->dev, "max17050 writev: [%x]=%x", reg, value);

	return ret;
}

static int max17050_write_verify_block(struct i2c_client *client, u8 reg,
							u16 *values, u8 len)
{
	int ret;
	int read;
	u8 i;

	for (i = 0; i < len; i++) {
		ret = i2c_smbus_write_word_data(client, reg + i, values[i]);

		if (ret < 0) {
			dev_err(&client->dev, "%s: write err %d\n", __func__,
									ret);
			return ret;
		}

		read = i2c_smbus_read_word_data(client, reg + i);

		if (read < 0) {
			dev_err(&client->dev, "%s: read err %d\n", __func__,
									read);
			return read;
		}

		if (read != values[i]) {
			dev_err(&client->dev,
				"%s: addr 0x%x expected 0x%x, got 0x%x\n",
				__func__, reg + i, values[i], read);
			return -EIO;
		}
	}

	return ret;
}

static void max17050_init_chip(struct max17050_chip *chip)
{
	struct i2c_client *client = chip->client;
	u16 vfsoc;
	u16 remcap;
	u16 repcap;
	u8 i;

	/* Wait 500ms after power up */
	msleep(500);

	/* Init config registers */
	max17050_write_reg(client, MAX17050_RELAXCFG, chip->pdata->relaxcfg);
	max17050_write_reg(client, MAX17050_CONFIG, chip->pdata->config);
	max17050_write_reg(client, MAX17050_FILTERCFG, chip->pdata->filtercfg);
	max17050_write_reg(client, MAX17050_LEARNCFG, chip->pdata->learncfg);
	max17050_write_reg(client, MAX17050_MISCCFG, chip->pdata->misccfg);
	max17050_write_reg(client, MAX17050_FULLSOCTHR, chip->pdata->fullsocthr);

	/* Unlock model using magic lock numbers from Maxim */
	max17050_write_reg(client, MAX17050_MODEL_LOCK1, 0x59);
	max17050_write_reg(client, MAX17050_MODEL_LOCK2, 0xc4);

	/* Write characterisation data */
	max17050_write_verify_block(client, MAX17050_MODEL_TABLE,
						chip->pdata->model, MODEL_SIZE);

	/* Lock the model */
	max17050_write_reg(client, MAX17050_MODEL_LOCK1, 0);
	max17050_write_reg(client, MAX17050_MODEL_LOCK2, 0);

	/* Verify that we can't read the model any more */
	for (i = 0; i < MODEL_SIZE; i++)
		if (max17050_read_reg(client, MAX17050_MODEL_TABLE + i))
			dev_err(&client->dev, "model is non-zero");

	/* Write custom parameters */
	max17050_write_verify_reg(client, MAX17050_RCOMP0,
						chip->pdata->rcomp0);
	max17050_write_verify_reg(client, MAX17050_TEMPCO,
						chip->pdata->tempco);
	max17050_write_verify_reg(client, MAX17050_TEMPNOM,
						chip->pdata->tempnom);
	max17050_write_verify_reg(client, MAX17050_TGAIN,
						chip->pdata->tgain);
	max17050_write_verify_reg(client, MAX17050_TOFF,
						chip->pdata->toff);
	max17050_write_verify_reg(client, MAX17050_ICHGTERM,
						chip->pdata->ichgterm);
	max17050_write_verify_reg(client, MAX17050_V_EMPTY,
						chip->pdata->vempty);
	max17050_write_verify_reg(client, MAX17050_QRTABLE00,
						chip->pdata->qrtable00);
	max17050_write_verify_reg(client, MAX17050_QRTABLE10,
						chip->pdata->qrtable10);
	max17050_write_verify_reg(client, MAX17050_QRTABLE20,
						chip->pdata->qrtable20);
	max17050_write_verify_reg(client, MAX17050_QRTABLE30,
						chip->pdata->qrtable30);

	/* Update full capacity registers */
	max17050_write_verify_reg(client, MAX17050_FULLCAP,
						chip->pdata->capacity);
	max17050_write_verify_reg(client, MAX17050_DESIGNCAP,
						chip->pdata->vf_fullcap);
	max17050_write_verify_reg(client, MAX17050_FULLCAPNOM,
						chip->pdata->vf_fullcap);

	/* Wait for VFSOC to be calculated */
	msleep(350);

	/* Write VFSOC value to VFSOC0, write QH to QH0 */
	vfsoc = max17050_read_reg(client, MAX17050_VFSOC);
	max17050_write_reg(client, MAX17050_VFSOC0_LOCK, 0x0080);
	max17050_write_verify_reg(client, MAX17050_VFSOC0, vfsoc);
	max17050_write_reg(client, MAX17050_VFSOC0_LOCK, 0);

	/* Write temperature */
	max17050_write_verify_reg(client, MAX17050_TEMP,
						chip->pdata->temperature);

	/* Load new capacity params */
	remcap = (vfsoc * chip->pdata->vf_fullcap) / 25600;
	max17050_write_verify_reg(client, MAX17050_REMCAP, remcap);
	repcap = remcap;
	max17050_write_verify_reg(client, MAX17050_REPCAP, repcap);

	max17050_write_verify_reg(client, MAX17050_DQACC,
						chip->pdata->vf_fullcap / 16);
	max17050_write_verify_reg(client, MAX17050_DPACC, chip->pdata->dpacc);
	max17050_write_verify_reg(client, MAX17050_FULLCAP,
						chip->pdata->capacity);
	max17050_write_reg(client, MAX17050_DESIGNCAP,
						chip->pdata->vf_fullcap);
	max17050_write_verify_reg(client, MAX17050_FULLCAPNOM,
						chip->pdata->vf_fullcap);

	/* Complete initialisation */
	chip->status = max17050_read_reg(client, MAX17050_STATUS);

	max17050_write_verify_reg(client, MAX17050_STATUS,
		chip->status & ~(STATUS_POR_BIT | STATUS_BR_BIT |
							STATUS_BI_BIT));

	/* Write custom parameter version */
	max17050_write_verify_reg(client, MAX17050_CUSTOMVER,
					chip->pdata->param_version);

	/* Set the parameter init status */
	if (chip->status & (STATUS_POR_BIT | STATUS_BI_BIT))
		chip->power_on_reset = true;
}

/* Must call with mutex locked */
static void max17050_save_learned_params(struct max17050_chip *chip)
{
	struct i2c_client *client = chip->client;

	chip->learned.rcomp0 = max17050_read_reg(client,
							MAX17050_RCOMP0);
	chip->learned.tempco = max17050_read_reg(client,
							MAX17050_TEMPCO);
	chip->learned.fullcap = max17050_read_reg(client,
							MAX17050_FULLCAP);
	chip->learned.cycles = max17050_read_reg(client,
							MAX17050_CYCLES);
	chip->learned.fullcapnom = max17050_read_reg(client,
							MAX17050_FULLCAPNOM);
	chip->learned.iavg_empty = max17050_read_reg(client,
							MAX17050_IAVG_EMPTY);
	chip->learned.qrtable00 = max17050_read_reg(client,
							MAX17050_QRTABLE00);
	chip->learned.qrtable10 = max17050_read_reg(client,
							MAX17050_QRTABLE10);
	chip->learned.qrtable20 = max17050_read_reg(client,
							MAX17050_QRTABLE20);
	chip->learned.qrtable30 = max17050_read_reg(client,
							MAX17050_QRTABLE30);
	chip->learned.param_version = max17050_read_reg(client,
							MAX17050_CUSTOMVER);
}

/* Must call with mutex locked */
static void max17050_restore_learned_params(struct max17050_chip *chip)
{
	struct i2c_client *client = chip->client;
	u16 remcap;

	/* Skip restore when custom param version has changed */
	if (chip->learned.param_version !=
			max17050_read_reg(client, MAX17050_CUSTOMVER))
		return;

	max17050_write_verify_reg(client, MAX17050_RCOMP0,
						chip->learned.rcomp0);
	max17050_write_verify_reg(client, MAX17050_TEMPCO,
						chip->learned.tempco);
	max17050_write_verify_reg(client, MAX17050_IAVG_EMPTY,
						chip->learned.iavg_empty);
	max17050_write_verify_reg(client, MAX17050_FULLCAPNOM,
						chip->learned.fullcapnom);
	max17050_write_verify_reg(client, MAX17050_QRTABLE00,
						chip->learned.qrtable00);
	max17050_write_verify_reg(client, MAX17050_QRTABLE10,
						chip->learned.qrtable10);
	max17050_write_verify_reg(client, MAX17050_QRTABLE20,
						chip->learned.qrtable20);
	max17050_write_verify_reg(client, MAX17050_QRTABLE30,
						chip->learned.qrtable30);

	/* Wait for VFSOC to be calculated */
	msleep(350);

	remcap = (max17050_read_reg(client, MAX17050_SOC) *
		max17050_read_reg(client, MAX17050_FULLCAP0)) / 25600;
	max17050_write_verify_reg(client, MAX17050_REMCAP, remcap);
	max17050_write_verify_reg(client, MAX17050_FULLCAP,
						chip->learned.fullcap);
	max17050_write_verify_reg(client, MAX17050_DQACC,
						chip->learned.fullcap / 16);
	max17050_write_verify_reg(client, MAX17050_DPACC, chip->pdata->dpacc);

	msleep(350);

	max17050_write_verify_reg(client, MAX17050_CYCLES,
							chip->learned.cycles);
	if (chip->learned.cycles > 0xff) {
		/* advance to learnstage 7 */
		max17050_write_verify_reg(client, MAX17050_LEARNCFG,
									0x0676);
	}
}

static ssize_t max17050_learned_read(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct max17050_chip *chip = dev_get_drvdata(dev);

	if (count == 0)
		return 0;
	else if (count < sizeof(struct max17050_learned_params))
		return -EINVAL;

	mutex_lock(&chip->mutex);
	max17050_save_learned_params(chip);
	memcpy(buf, &chip->learned, sizeof(struct max17050_learned_params));
	mutex_unlock(&chip->mutex);

	return count;
}

static ssize_t max17050_learned_write(struct file *filp,
				struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct max17050_chip *chip = dev_get_drvdata(dev);

	if (count == 0)
		return 0;
	else if (count < sizeof(struct max17050_learned_params))
		return -EINVAL;

	mutex_lock(&chip->mutex);
	memcpy(&chip->learned, buf, sizeof(struct max17050_learned_params));
	max17050_restore_learned_params(chip);
	mutex_unlock(&chip->mutex);

	return count;
}

static struct bin_attribute max17050_learned_attr = {
	.attr = {
		.name = "learned",
		.mode = S_IRUGO | S_IWUSR,
	},
	.size = sizeof(struct max17050_learned_params),
	.read = max17050_learned_read,
	.write = max17050_learned_write,
};

static ssize_t max17050_show_init_done(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->init_done);
}
static DEVICE_ATTR(init_done, S_IRUGO,
			max17050_show_init_done, NULL);

static ssize_t max17050_show_power_on_reset(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct max17050_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->power_on_reset);
}
static DEVICE_ATTR(power_on_reset, S_IRUGO,
			max17050_show_power_on_reset, NULL);

static void max17050_update_ext_temp(struct work_struct *work)
{
	struct max17050_chip *chip =
		container_of(work, struct max17050_chip, ext_batt_work.work);
	union power_supply_propval propval = {0,};
	u16 u16_value;

	if (!chip->ext_battery)
		chip->ext_battery = power_supply_get_by_name("battery");
	if (!chip->ext_battery) {
		/* Power supply not ready - try again soon */
		schedule_delayed_work(&chip->ext_batt_work,
					msecs_to_jiffies(EXT_BATT_FAST_PERIOD));
		return;
	}

	/* Get the temperature from the external power supply */
	chip->ext_battery->get_property(chip->ext_battery,
					POWER_SUPPLY_PROP_TEMP, &propval);

	if (chip->ext_temp != propval.intval) {
		chip->ext_temp = propval.intval;

		/* Write the temperature to the MAX17050 */
		u16_value = (u16)(chip->ext_temp * 256 / 10);
		mutex_lock(&chip->mutex);
		max17050_write_verify_reg(chip->client, MAX17050_TEMP,
								u16_value);
		mutex_unlock(&chip->mutex);
	}

	/* Update again after some time */
	schedule_delayed_work(&chip->ext_batt_work,
					msecs_to_jiffies(EXT_BATT_SLOW_PERIOD));
}

/* Must call with mutex locked */
static void max17050_update(struct max17050_chip *chip)
{
	struct i2c_client *client = chip->client;

	/* Check for power-on-reset or battery insertion */
	chip->status = max17050_read_reg(client, MAX17050_STATUS);

	if (chip->status == (u16)-EIO) {
		return;
	}

	if (chip->status & (STATUS_POR_BIT | STATUS_BI_BIT))
		max17050_init_chip(chip);

	chip->repcap = max17050_read_reg(client, MAX17050_REPCAP);
	chip->repsoc = max17050_read_reg(client, MAX17050_REPSOC);
	chip->tte = max17050_read_reg(client, MAX17050_TTE);
	chip->current_now = max17050_read_reg(client, MAX17050_CURRENT);
	if (!chip->use_ext_temp || !chip->ext_battery)
		chip->temp = max17050_read_reg(client, MAX17050_TEMP);
	chip->vcell = max17050_read_reg(client, MAX17050_VCELL);
	chip->learned.cycles = max17050_read_reg(client, MAX17050_CYCLES);

	/* next update must be at least 1 second later */
	ktime_get_ts(&chip->next_update_time);
	monotonic_to_bootbased(&chip->next_update_time);
	chip->next_update_time.tv_sec++;
}

static int max17050_scale_clamp_soc(struct max17050_chip *chip)
{
	int adj_soc;

	if (chip->pdata->full_soc <= chip->pdata->empty_soc) {
		pr_err("%s: Invalid soc range for scaling\n", __func__);
		return chip->repsoc >> 8;
	}

	/*
	 * Scale repsoc to the range between full_soc and empty_soc. Use an
	 * additional 3 bits of repsoc (0.125% units) for more accuracy.
	 */
	adj_soc = chip->repsoc >> 5;
	adj_soc = (adj_soc - chip->pdata->empty_soc * 8) * 100 /
			((chip->pdata->full_soc - chip->pdata->empty_soc) * 8);

	if (adj_soc > 100)
		adj_soc = 100;
	else if (adj_soc < 0)
		adj_soc = 0;

	return adj_soc;
}

static enum power_supply_property max17050_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int max17050_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17050_chip *chip = container_of(psy,
				struct max17050_chip, battery);
	struct timespec now;
	s16 s16_value;

	if (chip->suspended)
		return -EAGAIN;

	ktime_get_ts(&now);
	monotonic_to_bootbased(&now);
	if (timespec_compare(&now, &chip->next_update_time) >= 0) {
		mutex_lock(&chip->mutex);
		max17050_update(chip);
		mutex_unlock(&chip->mutex);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if (chip->status & MAX17050_STATUS_BATTABSENT)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = chip->learned.cycles / 100;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell * 625 / 8;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max17050_scale_clamp_soc(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->use_ext_temp && chip->ext_battery) {
			val->intval = chip->ext_temp;
		} else {
			/* cast to s16 */
			s16_value = (s16)chip->temp;
			/* sign extend to s32 */
			val->intval = (s32)s16_value;
			/* units are 1/256, and temp is reported in C * 10 */
			val->intval = (val->intval * 10) / 256;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* cast to s16 */
		s16_value = (s16)chip->current_now;
		/* sign extend to s32 */
		val->intval = (s32)s16_value;
		val->intval *= 1562500 / chip->pdata->r_sns;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void max17050_set_soc_thresholds(struct max17050_chip *chip,
								s16 threshold)
{
	s16 soc_now;
	s16 soc_max;
	s16 soc_min;

	soc_now = max17050_read_reg(chip->client, MAX17050_REPSOC) >> 8;

	soc_max = soc_now + threshold;
	if (soc_max > 100)
		soc_max = 100;
	soc_min = soc_now - threshold;
	if (soc_min < 0)
		soc_min = 0;

	max17050_write_reg(chip->client, MAX17050_SALRT_TH,
		(u16)soc_min | ((u16)soc_max << 8));
}

static irqreturn_t max17050_interrupt(int id, void *dev)
{
	struct max17050_chip *chip = dev;
	struct i2c_client *client = chip->client;
	u16 val;

	pr_debug("%s: interrupt occured, ID = %d\n", __func__, id);

	val = max17050_read_reg(client, MAX17050_STATUS);

	/* Signal userspace when the capacity exceeds the limits */
	if ((val & STATUS_INTR_SOCMIN_BIT) || (val & STATUS_INTR_SOCMAX_BIT)) {
		/* Clear interrupt status bits */
		max17050_write_reg(client, MAX17050_STATUS, val &
			~(STATUS_INTR_SOCMIN_BIT | STATUS_INTR_SOCMAX_BIT));

		/* Reset capacity thresholds */
		max17050_set_soc_thresholds(chip, 5);

		power_supply_changed(&chip->battery);
	}

	return IRQ_HANDLED;
}

static void max17050_complete_init(struct max17050_chip *chip)
{
	struct i2c_client *client = chip->client;
	int val;

	if (sysfs_create_bin_file(&client->dev.kobj, &max17050_learned_attr))
		dev_err(&client->dev, "sysfs_create_bin_file failed");

	if (device_create_file(&client->dev, &dev_attr_init_done))
		dev_err(&client->dev, "device_create_file failed");

	if (device_create_file(&client->dev, &dev_attr_power_on_reset))
		dev_err(&client->dev, "device_create_file failed");

	if (power_supply_register(&client->dev, &chip->battery))
		dev_err(&client->dev, "failed: power supply register");
	else
		chip->power_supply_registered = true;

	if (client->irq) {
		/* Set capacity thresholds to +/- 5% of current capacity */
		max17050_set_soc_thresholds(chip, 5);

		/* Enable capacity interrupts */
		val = max17050_read_reg(client, MAX17050_CONFIG);
		max17050_write_reg(client, MAX17050_CONFIG,
						val | CFG_ALRT_BIT_ENBL);
	}

	if (chip->use_ext_temp && chip->pdata->ext_batt_psy)
		schedule_delayed_work(&chip->ext_batt_work, 0);

	chip->init_done = true;
}

static void max17050_init_worker(struct work_struct *work)
{
	struct max17050_chip *chip = container_of(work,
				struct max17050_chip, work);

	max17050_init_chip(chip);
	max17050_complete_init(chip);
}

static struct max17050_platform_data *
max17050_get_pdata(struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 prop;
	struct max17050_platform_data *pdata;
	const u8 *model;
	int len;

	if (!np)
		return dev->platform_data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	/*
	 * Require current sense resistor value to be specified for
	 * current-sense functionality to be enabled at all.
	 */
	if (of_property_read_u32(np, "maxim,rsns-microohm", &prop) == 0) {
		pdata->r_sns = prop;
		pdata->enable_current_sense = true;
	} else {
		pdata->r_sns = MAX17050_DEFAULT_SNS_RESISTOR;
	}

	pdata->ext_batt_psy = of_property_read_bool(np, "maxim,ext_batt_psy");

	if (of_property_read_u32(np, "maxim,relaxcfg", &prop) == 0)
		pdata->relaxcfg = prop;
	if (of_property_read_u32(np, "maxim,config", &prop) == 0)
		pdata->config = prop;
	if (of_property_read_u32(np, "maxim,filtercfg", &prop) == 0)
		pdata->filtercfg = prop;
	if (of_property_read_u32(np, "maxim,learncfg", &prop) == 0)
		pdata->learncfg = prop;
	if (of_property_read_u32(np, "maxim,misccfg", &prop) == 0)
		pdata->misccfg = prop;
	if (of_property_read_u32(np, "maxim,fullsocthr", &prop) == 0)
		pdata->fullsocthr = prop;
	if (of_property_read_u32(np, "maxim,rcomp0", &prop) == 0)
		pdata->rcomp0 = prop;
	if (of_property_read_u32(np, "maxim,tempco", &prop) == 0)
		pdata->tempco = prop;
	if (of_property_read_u32(np, "maxim,tempnom", &prop) == 0)
		pdata->tempnom = prop;
	if (of_property_read_u32(np, "maxim,tgain", &prop) == 0)
		pdata->tgain = prop;
	if (of_property_read_u32(np, "maxim,toff", &prop) == 0)
		pdata->toff = prop;
	if (of_property_read_u32(np, "maxim,ichgterm", &prop) == 0)
		pdata->ichgterm = prop;
	if (of_property_read_u32(np, "maxim,vempty", &prop) == 0)
		pdata->vempty = prop;
	if (of_property_read_u32(np, "maxim,qrtable00", &prop) == 0)
		pdata->qrtable00 = prop;
	if (of_property_read_u32(np, "maxim,qrtable10", &prop) == 0)
		pdata->qrtable10 = prop;
	if (of_property_read_u32(np, "maxim,qrtable20", &prop) == 0)
		pdata->qrtable20 = prop;
	if (of_property_read_u32(np, "maxim,qrtable30", &prop) == 0)
		pdata->qrtable30 = prop;
	if (of_property_read_u32(np, "maxim,capacity", &prop) == 0)
		pdata->capacity = prop;
	if (of_property_read_u32(np, "maxim,vf_fullcap", &prop) == 0)
		pdata->vf_fullcap = prop;
	if (of_property_read_u32(np, "maxim,temperature", &prop) == 0)
		pdata->temperature = prop;
	if (of_property_read_u32(np, "maxim,dpacc", &prop) == 0)
		pdata->dpacc = prop;
	if (of_property_read_u32(np, "maxim,empty-soc", &prop) == 0)
		pdata->empty_soc = prop;
	else
		pdata->empty_soc = 0;
	if (of_property_read_u32(np, "maxim,full-soc", &prop) == 0)
		pdata->full_soc = prop;
	else
		pdata->full_soc = 100;

	if (of_property_read_u32(np, "maxim,param-version", &prop) == 0)
		pdata->param_version = prop;

	model = of_get_property(np, "maxim,model", &len);
	if (model && ((len / 2) == MODEL_SIZE)) {
		int i;

		for (i = 0; i < MODEL_SIZE; i++) {
			pdata->model[i] = *model++ << 8;
			pdata->model[i] |= *model++;
		}
	}

	return pdata;
}

static int max17050_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17050_chip *chip;
	int ret = 0;
	bool new_custom_param = false;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = max17050_get_pdata(&client->dev);
	if (!chip->pdata) {
		dev_err(&client->dev, "no platform data provided\n");
		devm_kfree(&client->dev, chip);
		return -EINVAL;
	}

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

	chip->battery.get_property = max17050_get_property;
	chip->battery.properties = max17050_battery_props;
	chip->battery.num_properties = ARRAY_SIZE(max17050_battery_props);

	/*
	 * When current is not measured, CURRENT_NOW property should be
	 * invisible.
	 */
	if (!chip->pdata->enable_current_sense)
		chip->battery.num_properties -= 1;

	chip->use_ext_temp = !!(chip->pdata->config & CFG_EXT_TEMP_BIT);
	if (chip->use_ext_temp && chip->pdata->ext_batt_psy)
		INIT_DELAYED_WORK(&chip->ext_batt_work,
						max17050_update_ext_temp);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
					max17050_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					chip->battery.name, chip);
		if (ret) {
			dev_err(&client->dev, "cannot enable irq");
			return ret;
		} else {
			enable_irq_wake(client->irq);
		}
	}

	INIT_WORK(&chip->work, max17050_init_worker);
	mutex_init(&chip->mutex);

	/*
	 * If the version of custom parameters is changed, the init chip
	 * should be called even if POR doesn't happen.
	 * For checking parameter change, the reserved register, 0x20,
	 * is defined as custom version register.
	 */
	if (max17050_read_reg(client, MAX17050_CUSTOMVER) !=
					chip->pdata->param_version)
		new_custom_param = true;

	if ((max17050_read_reg(client, MAX17050_STATUS) &
				(STATUS_POR_BIT | STATUS_BI_BIT)) ||
							new_custom_param)
		schedule_work(&chip->work);
	else
		max17050_complete_init(chip);

	return ret;
}

static int max17050_remove(struct i2c_client *client)
{
	struct max17050_chip *chip = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_power_on_reset);
	device_remove_file(&client->dev, &dev_attr_init_done);
	sysfs_remove_bin_file(&client->dev.kobj, &max17050_learned_attr);

	if (client->irq)
		disable_irq_wake(client->irq);

	if (chip->power_supply_registered)
		power_supply_unregister(&chip->battery);

	i2c_set_clientdata(client, NULL);
	devm_kfree(&client->dev, chip);

	return 0;
}

static int max17050_pm_prepare(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17050_chip *chip = i2c_get_clientdata(client);

	/* Cancel any pending work, if necessary */
	if (chip->use_ext_temp && chip->pdata->ext_batt_psy)
		cancel_delayed_work_sync(&chip->ext_batt_work);

	chip->suspended = true;
	if (chip->client->irq) {
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}
	return 0;
}

static void max17050_pm_complete(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17050_chip *chip = i2c_get_clientdata(client);

	chip->suspended = false;
	if (chip->client->irq) {
		disable_irq_wake(chip->client->irq);
		enable_irq(chip->client->irq);
	}

	/* Schedule a temperature update, if needed */
	if (chip->use_ext_temp && chip->pdata->ext_batt_psy)
		schedule_delayed_work(&chip->ext_batt_work, 0);
}

static const struct dev_pm_ops max17050_pm_ops = {
	.prepare = max17050_pm_prepare,
	.complete = max17050_pm_complete,
};

static const struct of_device_id max17050_dt_match[] = {
	{ .compatible = "maxim,max17050" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17050_dt_match);

static const struct i2c_device_id max17050_id[] = {
	{ "max17050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17050_id);

static struct i2c_driver max17050_i2c_driver = {
	.driver	= {
		.name = "max17050",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max17050_dt_match),
		.pm = &max17050_pm_ops,
	},
	.probe = max17050_probe,
	.remove = max17050_remove,
	.id_table = max17050_id,
};
module_i2c_driver(max17050_i2c_driver);

MODULE_AUTHOR("Simon Wilson <simonwilson@google.com>");
MODULE_DESCRIPTION("MAX17050 Fuel Gauge");
MODULE_LICENSE("GPL");
