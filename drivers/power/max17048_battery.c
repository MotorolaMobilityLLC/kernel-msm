/*
 *  max17048_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2012 Nvidia Cooperation
 *  Chandler Zhang <chazhang@nvidia.com>
 *  Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 *  Copyright (C) 2013 LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/power/max17048_battery.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define MAX17048_MODE		0x06

#define MAX17048_VCELL		0x02
#define MAX17048_SOC		0x04
#define MAX17048_VER		0x08
#define MAX17048_HIBRT		0x0A
#define MAX17048_CONFIG		0x0C
#define MAX17048_OCV		0x0E
#define MAX17048_VLRT		0x14
#define MAX17048_VRESET		0x18
#define MAX17048_STATUS		0x1A
#define MAX17048_UNLOCK		0x3E
#define MAX17048_TABLE		0x40
#define MAX17048_RCOMPSEG1	0x80
#define MAX17048_RCOMPSEG2	0x90
#define MAX17048_CMD		0xFF
#define MAX17048_UNLOCK_VALUE	0x4a57
#define MAX17048_RESET_VALUE	0x5400
#define MAX17048_POLLING_PERIOD	50000
#define MAX17048_BATTERY_FULL	100
#define MAX17048_BATTERY_LOW	15
#define MAX17048_VERSION_11	0x11
#define MAX17048_VERSION_12	0x12

struct max17048_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply batt_psy;
	struct power_supply *ac_psy;
	struct max17048_platform_data	*pdata;
	int vcell;
	int soc;
	int capacity_level;
	int lasttime_vcell;
	int lasttime_soc;
	int lasttime_status;
	struct work_struct	alert_work;
	struct wake_lock	alert_lock;
	uint16_t config;
	uint16_t status;
	int alert_gpio;
	int rcomp;
	int alert_threshold;
	int max_mvolt;
	int min_mvolt;
	int batt_tech;
	int fcc_mah;
	int voltage;
	int lasttime_voltage;
	int lasttime_capacity_level;
	int state;
	int chg_state;
	int batt_temp;
	int batt_health;
};

static struct max17048_chip *ref;

static int max17048_write_word(struct i2c_client *client, int reg, u16 value)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, swab16(value));

	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register"
					"0x%02x err %d\n", __func__, reg, ret);

	return ret;
}

static int max17048_read_word(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in reading register"
					"0x%02x err %d\n", __func__, reg, ret);
		return ret;
	} else {
		ret = (int)swab16((uint16_t)(ret & 0x0000ffff));
		return ret;
	}
}

static int max17048_get_config(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int config;

	config = max17048_read_word(client, MAX17048_CONFIG);
	if (config < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, config);
		return config;
	} else {
		pr_info("%s: config = 0x%x\n", __func__, config);
		chip->config = config;
		return 0;
	}
}

static int max17048_get_status(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int status;

	status = max17048_read_word(client, MAX17048_STATUS);
	if (status < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, status);
		return status;
	} else {
		pr_info("%s: status = 0x%x\n", __func__, status);
		chip->status = status;
		return 0;
	}
}

/* Using Quickstart instead of reset for Power Test
*  DO NOT USE THIS COMMAND ANOTHER SCENE.
*/
static int max17048_set_reset(struct i2c_client *client)
{
	max17048_write_word(client, MAX17048_MODE, 0x4000);
	dev_info(&client->dev, "MAX17048 Reset(Quickstart)\n");
	return 0;
}

/* Calculate MAX17048 custom model SOC */
static int max17048_get_capacity_from_soc(void)
{
	u8 buf[2];
	int batt_soc = 0;

	if (ref == NULL)
		return 100;

	buf[0] = (ref->soc & 0x0000FF00) >> 8;
	buf[1] = (ref->soc & 0x000000FF);

	pr_debug("%s: SOC raw = 0x%x%x\n", __func__, buf[0], buf[1]);

	batt_soc = (((int)buf[0]*256)+buf[1])*19531; /* 0.001953125 */
	batt_soc /= 10000000;

	if (batt_soc > 100)
		batt_soc = 100;

	return batt_soc;
}

static void max17048_get_vcell(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int vcell;

	vcell = max17048_read_word(client, MAX17048_VCELL);
	if (vcell < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, vcell);
	else
		chip->vcell = vcell >> 4;
	chip->voltage = (chip->vcell * 5) >> 2;
}

static void max17048_get_soc(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int soc;

	soc = max17048_read_word(client, MAX17048_SOC);
	if (soc < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, soc);
	else
		chip->soc = soc;
	chip->capacity_level = max17048_get_capacity_from_soc();
}

static uint16_t max17048_get_version(struct i2c_client *client)
{
	return swab16(i2c_smbus_read_word_data(client, MAX17048_VER));
}

static void max17048_work(struct work_struct *work)
{
	struct max17048_chip *chip;
	int ret = 0;

	pr_debug("%s.\n", __func__);

	chip = container_of(work, struct max17048_chip, work.work);
	if (chip == NULL) {
		pr_err("%s: Called before init.\n", __func__);
		return;
	}

	disable_irq(gpio_to_irq(chip->alert_gpio));
	ret = max17048_get_config(chip->client);
	if (ret < 0)
		pr_err("%s: error get config register.\n", __func__);

	ret = max17048_get_status(chip->client);
	if (ret < 0)
		pr_err("%s: error get status register.\n", __func__);

	/* Update recently VCELL, SOC and CAPACITY */
	max17048_get_vcell(chip->client);
	max17048_get_soc(chip->client);

	if (chip->voltage != chip->lasttime_voltage ||
		chip->capacity_level != chip->lasttime_capacity_level) {
		chip->lasttime_voltage = chip->voltage;
		chip->lasttime_soc = chip->soc;
		chip->lasttime_capacity_level = chip->capacity_level;

		power_supply_changed(&chip->batt_psy);
	}

	pr_info("%s: MAX17048 SOC : 0x%x / vcell : 0x%x\n",
			__func__, chip->soc, chip->vcell);
	pr_info("%s: MAX17048 Capacity : %d / voltage : %d\n",
			__func__, chip->capacity_level, chip->voltage);

	enable_irq(gpio_to_irq(chip->alert_gpio));
	return;
}

static irqreturn_t max17048_interrupt_handler(int irq, void *data)
{
	struct max17048_chip *chip = data;

	pr_debug("%s : interupt occured\n", __func__);

	if (chip == NULL) {
		pr_err("%s : called before init.\n", __func__);
		return IRQ_HANDLED;
	}

	if (chip->state)
		schedule_work(&chip->alert_work);

	return IRQ_HANDLED;
}

static int max17048_clear_interrupt(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int ret;

	pr_debug("%s.\n", __func__);
	if (chip == NULL)
		return -ENODEV;

	/* Clear low battery alert config register */
	if (chip->config & 0x0020) {
		chip->config &= 0xffdf;
		ret = max17048_write_word(chip->client,
				MAX17048_CONFIG, chip->config);
		if (ret < 0)
			return ret;
	}

	/* Clear All setted status register */
	if (chip->status & 0xFF00) {
		chip->status &= 0x00FF;
		ret = max17048_write_word(chip->client,
				MAX17048_STATUS, chip->status);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int max17048_set_athd_alert(struct i2c_client *client, int level)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int ret;

	pr_debug("%s.\n", __func__);
	if (chip == NULL)
		return -ENODEV;

	if (level > 32)
		level = 32;
	else if (level < 1)
		level = 1;

	level = 32 - level;

	/* If same LEVEL set previous value */
	if (level == (chip->config & 0x1F))
		return level;

	/* New LEVEL set and Set alert threshold flag */

	chip->config = ((chip->config & 0xffe0) | level);

	ret = max17048_write_word(chip->client,
			MAX17048_CONFIG, chip->config);
	if (ret < 0)
		return ret;

	return level;
}

static int max17048_set_alsc_alert(struct i2c_client *client,
	int enable)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int ret;

	pr_debug("%s. with %d \n", __func__,enable);
	if (chip == NULL)
		return -ENODEV;

	if (enable) {
		/* Enable SOC change alert */
		if (!(chip->config & 0x0040)) {
			chip->config |= 0x0040;
			ret = max17048_write_word(chip->client,
					MAX17048_CONFIG, chip->config);
			if (ret < 0)
				return ret;
		}
	} else {
		/* Disable SOC change alert */
		if (chip->config & 0x0040) {
			chip->config &= 0xFFBF;
			ret = max17048_write_word(chip->client,
					MAX17048_CONFIG, chip->config);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static void max17048_alert_work(struct work_struct *work)
{
	struct max17048_chip *chip = container_of(work,
			struct max17048_chip, alert_work);
	int ret;

	wake_lock(&chip->alert_lock);

	pr_debug("%s.\n", __func__);

	if (chip == NULL) {
		pr_err("%s : Called before init.\n", __func__);
		goto error;
	}

	/* MAX17048 update register */
	max17048_work(&(chip->work.work));

	/* Clear Interrupt status */
	ret = max17048_clear_interrupt(chip->client);
	if (ret < 0)
		pr_err("%s : error clear alert interrupt register.\n",
				__func__);

error:
	wake_unlock(&chip->alert_lock);
	return;
}

ssize_t max17048_show_voltage(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int level;

	if (ref == NULL)
		return snprintf(buf, PAGE_SIZE, "ERROR\n");

	level = ref->voltage;
	return snprintf(buf, PAGE_SIZE, "%d\n", level);
}
DEVICE_ATTR(voltage, 0444, max17048_show_voltage, NULL);

ssize_t max17048_show_capacity(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int level;

	if (ref == NULL)
		return snprintf(buf, PAGE_SIZE, "ERROR\n");

	level = ref->capacity_level;

	if (level > 100)
		level = 100;
	else if (level < 0)
		level = 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", level);
}
DEVICE_ATTR(capacity, 0444, max17048_show_capacity, NULL);

ssize_t max17048_store_status(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	if (ref == NULL)
		return -ENODEV;

	if (strncmp(buf, "reset", 5) == 0) {
		cancel_delayed_work(&ref->work);
		max17048_set_reset(ref->client);
		schedule_delayed_work(&ref->work, HZ);
	} else {
		return -EINVAL;
	}
	return count;
}
DEVICE_ATTR(fuelrst, 0664, NULL, max17048_store_status);

int max17048_get_voltage(void)
{
	if (ref == NULL)	/* if fuel gauge is not initialized, */
		return 4350;	/* return Dummy Value */
	return ref->voltage;
}
EXPORT_SYMBOL(max17048_get_voltage);

int max17048_get_capacity(void)
{
	if (ref == NULL)	/* if fuel gauge is not initialized, */
		return 100;	/* return Dummy Value */
	return ref->capacity_level;
}
EXPORT_SYMBOL(max17048_get_capacity);

#ifdef CONFIG_OF
static int max17048_parse_dt(struct device *dev,
		struct max17048_chip *chip)
{
	struct device_node *dev_node = dev->of_node;
	int ret = 0;

	chip->alert_gpio = of_get_named_gpio(dev_node,
			"max17048,alert_gpio", 0);
	if (chip->alert_gpio < 0) {
		pr_err("failed to get stat-gpio.\n");
		ret = chip->alert_gpio;
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,rcomp",
				&chip->rcomp);
	if (ret) {
		pr_err("%s: failed to read rcomp\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,alert_threshold",
				&chip->alert_threshold);
	if (ret) {
		pr_err("%s: failed to read alert_threshold\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,max-mvolt",
				   &chip->max_mvolt);
	if (ret) {
		pr_err("%s: failed to read max voltage\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,min-mvolt",
				   &chip->min_mvolt);
	if (ret) {
		pr_err("%s: failed to read min voltage\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,batt-tech",
				   &chip->batt_tech);
	if (ret) {
		pr_err("%s: failed to read batt technology\n", __func__);
		goto out;
	}

	ret = of_property_read_u32(dev_node, "max17048,fcc-mah",
				   &chip->fcc_mah);
	if (ret) {
		pr_err("%s: failed to read batt fcc\n", __func__);
		goto out;
	}

	pr_info("%s: rcomp = %d alert_thres = 0x%x"\
		"max_mv = %d min-mv =%d fcc = %d\n",
		__func__, chip->rcomp, chip->alert_threshold,
		chip->max_mvolt, chip->min_mvolt, chip->fcc_mah);

out:
	return ret;
}
#endif

static int max17048_get_prop_status(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	chip->ac_psy->get_property(chip->ac_psy, POWER_SUPPLY_PROP_STATUS,
				&ret);
	return ret.intval;
}

static int max17048_get_prop_health(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	chip->ac_psy->get_property(chip->ac_psy, POWER_SUPPLY_PROP_HEALTH,
				&ret);
	return ret.intval;
}

static int max17048_get_prop_vbatt_uv(struct max17048_chip *chip)
{
	max17048_get_vcell(chip->client);
	return chip->voltage * 1000;
}

static int max17048_get_prop_present(struct max17048_chip *chip)
{
	/*FIXME - need to implement */
	return true;
}

static int max17048_get_prop_temp(struct max17048_chip *chip)
{
	/* FIXME - need to implement */
	chip->batt_temp = 250;

	return chip->batt_temp;
}

static enum power_supply_property max17048_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int max17048_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17048_chip *chip = container_of(psy,
				struct max17048_chip, batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max17048_get_prop_status(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max17048_get_prop_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max17048_get_prop_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->batt_tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->max_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = chip->min_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17048_get_prop_vbatt_uv(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = max17048_get_prop_temp(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->fcc_mah;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max17048_external_power_changed(struct power_supply *psy)
{

	struct max17048_chip *chip = container_of(psy, struct max17048_chip,
						batt_psy);
	int chg_state;
	int batt_health;

	chg_state = max17048_get_prop_status(chip);
	batt_health = max17048_get_prop_health(chip);

	if ((chip->chg_state ^ chg_state)
			||(chip->batt_health ^ batt_health)) {
		chip->chg_state = chg_state;
		chip->batt_health = batt_health;
		pr_info("%s: power supply changed state = %d health = %d",
					__func__, chg_state, batt_health);
		power_supply_changed(psy);
	}
}


static int max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17048_chip *chip;
	int ret;
	uint16_t version;

	pr_info("%s: start\n", __func__);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c_check_functionality fail\n", __func__);
		return -EIO;
	}

	chip->client = client;

	chip->ac_psy = power_supply_get_by_name("ac");
	if (!chip->ac_psy) {
		pr_err("ac supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

#ifdef CONFIG_OF
	if (&client->dev.of_node) {
		ret = max17048_parse_dt(&client->dev, chip);
		if (ret) {
			pr_err("%s: failed to parse dt\n", __func__);
			goto  error;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}
#else
	chip->pdata = client->dev.platform_data;
#endif

	i2c_set_clientdata(client, chip);

	version = max17048_get_version(client);
	dev_info(&client->dev, "MAX17048 Fuel-Gauge Ver 0x%x\n", version);
	if (version != MAX17048_VERSION_11 &&
	    version != MAX17048_VERSION_12) {
		pr_err("%s: Not supported version: 0x%x\n", __func__,
				version);
		ret = -ENODEV;
		goto error;
	}

	ref = chip;

	chip->batt_psy.name		= "battery";
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= max17048_get_property;
	chip->batt_psy.properties	= max17048_battery_props;
	chip->batt_psy.num_properties	= ARRAY_SIZE(max17048_battery_props);
	chip->batt_psy.external_power_changed =
				max17048_external_power_changed;

	ret = power_supply_register(&client->dev, &chip->batt_psy);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error;
	}

	if (chip->alert_gpio) {
		ret = gpio_request(chip->alert_gpio,
				"max17048_alert");
		if (ret < 0) {
			pr_err("%s: GPIO Request Failed : return %d\n",
					__func__, ret);
			goto err_gpio_request_failed;
		}

		gpio_direction_input(chip->alert_gpio);

		ret = request_irq(gpio_to_irq(chip->alert_gpio),
				max17048_interrupt_handler,
				IRQF_TRIGGER_FALLING,
				"MAX17048_Alert", chip);
		if (ret < 0) {
			pr_err("%s: IRQ Request Failed : return %d\n",
					__func__, ret);
			goto err_request_irq_failed;
		}

		ret = enable_irq_wake(gpio_to_irq(chip->alert_gpio));
		if (ret < 0) {
			pr_err("%s: set irq to wakeup source failed.\n", __func__);
			goto err_request_wakeup_irq_failed;
		}
		disable_irq(gpio_to_irq(chip->alert_gpio));
	}

	ret = device_create_file(&client->dev, &dev_attr_voltage);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_voltage_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_capacity);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_capacity_failed;
	}
	ret = device_create_file(&client->dev, &dev_attr_fuelrst);
	if (ret < 0) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_file_fuelrst_failed;
	}

	INIT_DELAYED_WORK(&chip->work, max17048_work);
	INIT_WORK(&chip->alert_work, max17048_alert_work);
	wake_lock_init(&chip->alert_lock, WAKE_LOCK_SUSPEND, "max17048_alert");

	/* Update recently status register & clears the reset indicator */
	max17048_get_status(client);
	ret = max17048_write_word(client,
			MAX17048_STATUS, chip->status & 0xFEFF);
	if (ret < 0)
		return ret;

	/* Update recently config register */
	max17048_get_config(client);
	/* Set low battery alert threshold */
	max17048_set_athd_alert(client, chip->alert_threshold);

	/* Set register following each mode define */
	max17048_set_alsc_alert(client, 1);
	chip->state = 1;
	max17048_work(&(chip->work.work));

	/* Clear CONFIG.ATHD and Any setted STATUS register flag */
	max17048_clear_interrupt(client);
	enable_irq(gpio_to_irq(chip->alert_gpio));

	pr_info("%s: done\n", __func__);
	return 0;

err_create_file_fuelrst_failed:
	device_remove_file(&client->dev, &dev_attr_capacity);
err_create_file_capacity_failed:
	device_remove_file(&client->dev, &dev_attr_voltage);
err_create_file_voltage_failed:
	disable_irq_wake(gpio_to_irq(chip->alert_gpio));
err_request_wakeup_irq_failed:
	free_irq(gpio_to_irq(chip->alert_gpio), NULL);
err_request_irq_failed:
	gpio_free(chip->alert_gpio);
err_gpio_request_failed:
	power_supply_unregister(&chip->batt_psy);
error:
	kfree(chip);
	return ret;
}

static int max17048_remove(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_fuelrst);
	device_remove_file(&client->dev, &dev_attr_capacity);
	device_remove_file(&client->dev, &dev_attr_voltage);
	free_irq(gpio_to_irq(chip->alert_gpio), NULL);
	gpio_free(chip->alert_gpio);
	power_supply_unregister(&chip->batt_psy);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int max17048_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);

	/* Disable ALSC(1% alert) */
	max17048_set_alsc_alert(client, 0);
	chip->state = 0;
	return 0;
}

static int max17048_resume(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);

	max17048_work(&(chip->work.work));
	chip->state = 1;
	/* Enable ALSC(1% alert) */
	max17048_set_alsc_alert(client, 1);
	return 0;
}

#else

#define max17048_suspend NULL
#define max17048_resume NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id max17048_match_table[] = {
	{ .compatible = "maxim,max17048", },
	{ },
};
#endif

static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17048_id);

static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name	= "max17048",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max17048_match_table,
#endif
	},
	.probe = max17048_probe,
	.remove = max17048_remove,
	.suspend = max17048_suspend,
	.resume = max17048_resume,
	.id_table = max17048_id,
};

static int __init max17048_init(void)
{
	return i2c_add_driver(&max17048_i2c_driver);
}
module_init(max17048_init);

static void __exit max17048_exit(void)
{
	i2c_del_driver(&max17048_i2c_driver);
}
module_exit(max17048_exit);

MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");
