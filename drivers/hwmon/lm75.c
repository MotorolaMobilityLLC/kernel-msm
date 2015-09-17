/*
 * lm75.c - Part of lm_sensors, Linux kernel modules for hardware
 *	 monitoring
 * Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "lm75.h"


/*
 * This driver handles the LM75 and compatible digital temperature sensors.
 */

enum lm75_type {		/* keep sorted in alphabetical order */
	adt75,
	ds1775,
	ds75,
	ds7505,
	g751,
	lm75,
	lm75a,
	max6625,
	max6626,
	mcp980x,
	stds75,
	tcn75,
	tmp100,
	tmp101,
	tmp105,
	tmp108,
	tmp112,
	tmp175,
	tmp275,
	tmp75,
};

/* Addresses scanned */
static const unsigned short normal_i2c[] = { 0x48, 0x49, 0x4a, 0x4b, 0x4c,
					0x4d, 0x4e, 0x4f, I2C_CLIENT_END };


/* The LM75 registers */
#define LM75_REG_CONF		0x01
static const u8 LM75_REG_TEMP[3] = {
	0x00,		/* input */
	0x03,		/* max */
	0x02,		/* hyst */
};

#define TMP108_MODE_MASK 0x0300
#define TMP108_SHUTDOWN  (0 << 8)
#define TMP108_ONESHOT   (1 << 8)
#define TMP108_CONT      (2 << 8)

/* Each client has this additional data */
struct lm75_data {
	struct i2c_client	*client;
	enum lm75_type          sensor;
	int                     irq_gpio;
	struct device		*hwmon_dev;
	struct thermal_zone_device	*tz;
	struct mutex		update_lock;
	u16			orig_conf;
	u8			resolution;	/* In bits, between 9 and 12 */
	u8			resolution_limits;
	char			valid;		/* !=0 if registers are valid */
	unsigned long		last_updated;	/* In jiffies */
	unsigned long		sample_time;	/* In jiffies */
	s16			temp[3];	/* Register values,
						   0 = input
						   1 = max
						   2 = hyst */
};

static int lm75_read_value(struct i2c_client *client, u8 reg);
static int lm75_write_value(struct i2c_client *client, u8 reg, u16 value);
static struct lm75_data *lm75_update_device(struct device *dev);


/*-----------------------------------------------------------------------*/

static inline long lm75_reg_to_mc(s16 temp, u8 resolution)
{
	return ((temp >> (16 - resolution)) * 1000) >> (resolution - 8);
}

/* sysfs attributes for hwmon */

static int lm75_read_temp(void *dev, long *temp)
{
	struct lm75_data *data = lm75_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	*temp = lm75_reg_to_mc(data->temp[0], data->resolution);

	return 0;
}

static ssize_t show_temp(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct lm75_data *data = lm75_update_device(dev);

	if (IS_ERR(data))
		return PTR_ERR(data);

	return sprintf(buf, "%ld\n", lm75_reg_to_mc(data->temp[attr->index],
						    data->resolution));
}

static ssize_t set_temp(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct lm75_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int nr = attr->index;
	long temp;
	int error;
	u8 resolution;

	error = kstrtol(buf, 10, &temp);
	if (error)
		return error;

	/*
	 * Resolution of limit registers is assumed to be the same as the
	 * temperature input register resolution unless given explicitly.
	 */
	if (attr->index && data->resolution_limits)
		resolution = data->resolution_limits;
	else
		resolution = data->resolution;

	mutex_lock(&data->update_lock);
	temp = clamp_val(temp, LM75_TEMP_MIN, LM75_TEMP_MAX);
	data->temp[nr] = DIV_ROUND_CLOSEST(temp  << (resolution - 8),
					   1000) << (16 - resolution);
	lm75_write_value(client, LM75_REG_TEMP[nr], data->temp[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_alarm(struct device *dev,
			  struct device_attribute *da, char *buf)
{
	struct lm75_data *data = dev_get_drvdata(dev);
	int value = gpio_get_value(data->irq_gpio);
	return sprintf(buf, "%d\n", value);
}

static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO,
			show_temp, set_temp, 1);
static SENSOR_DEVICE_ATTR(temp1_max_hyst, S_IWUSR | S_IRUGO,
			show_temp, set_temp, 2);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_temp, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_alarm, S_IRUGO, show_alarm, NULL, 0);

static struct attribute *lm75_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,

	NULL
};
ATTRIBUTE_GROUPS(lm75);

static const struct thermal_zone_of_device_ops lm75_of_thermal_ops = {
	.get_temp = lm75_read_temp,
};

#ifdef CONFIG_OF
static int lm75_of_init(struct lm75_data *data, struct i2c_client *client)
{
	struct device_node *np;

	np = client->dev.of_node;
	data->irq_gpio = of_get_gpio(np, 0);
	return ((data->irq_gpio < 0) ? data->irq_gpio : 0);
}
#else
static inline int lm75_of_init(struct lm75_data *data,
			       struct i2c_client *client)
{
	return -EINVAL;
}
#endif

static irqreturn_t lm75_alert_irq_handler(int irq, void *dev_id)
{
	struct lm75_data *data = dev_id;

	data->valid = 0;
	lm75_update_device(data->hwmon_dev);

	sysfs_notify(&data->hwmon_dev->kobj, NULL, "temp1_alarm");

	return IRQ_HANDLED;
}

static int lm75_init_irq_gpio(struct lm75_data *data, struct i2c_client *client)
{
	int ret;
	int alert_irq;

	ret = devm_gpio_request_one(&client->dev, data->irq_gpio,
				    (GPIOF_IN | GPIOF_ACTIVE_LOW),
				    client->name);
	if (ret) {
		dev_err(&client->dev, "GPIO request failed: %d\n", ret);
		return ret;
	}

	alert_irq = gpio_to_irq(data->irq_gpio);
	if (alert_irq < 0) {
		dev_err(&client->dev, "GPIO to IRQ failed: %d\n", ret);
		return ret;
	}

	irq_set_irq_type(alert_irq, IRQ_TYPE_EDGE_BOTH);

	ret = devm_request_threaded_irq(&client->dev, alert_irq,
					NULL,
					lm75_alert_irq_handler,
					(IRQF_TRIGGER_FALLING |
					 IRQF_TRIGGER_RISING |
					 IRQF_ONESHOT),
					client->name, data);
	return ret;
}

/*-----------------------------------------------------------------------*/

/* device probe and removal */

static int
lm75_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct lm75_data *data;
	int status;
	u16 set_mask, clr_mask;
	int new;
	enum lm75_type kind = id->driver_data;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	data = devm_kzalloc(dev, sizeof(struct lm75_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);
	data->irq_gpio = -1;
	mutex_init(&data->update_lock);

	data->sensor = id->driver_data;

	/* Set to LM75 resolution (9 bits, 1/2 degree C) and range.
	 * Then tweak to be more precise when appropriate.
	 */
	set_mask = 0;
	clr_mask = LM75_SHUTDOWN;		/* continuous conversions */

	switch (kind) {
	case adt75:
		clr_mask |= 1 << 5;		/* not one-shot mode */
		data->resolution = 12;
		data->sample_time = HZ / 8;
		break;
	case ds1775:
	case ds75:
	case stds75:
		clr_mask |= 3 << 5;
		set_mask |= 2 << 5;		/* 11-bit mode */
		data->resolution = 11;
		data->sample_time = HZ;
		break;
	case ds7505:
		set_mask |= 3 << 5;		/* 12-bit mode */
		data->resolution = 12;
		data->sample_time = HZ / 4;
		break;
	case g751:
	case lm75:
	case lm75a:
		data->resolution = 9;
		data->sample_time = HZ / 2;
		break;
	case max6625:
		data->resolution = 9;
		data->sample_time = HZ / 4;
		break;
	case max6626:
		data->resolution = 12;
		data->resolution_limits = 9;
		data->sample_time = HZ / 4;
		break;
	case tcn75:
		data->resolution = 9;
		data->sample_time = HZ / 8;
		break;
	case mcp980x:
		data->resolution_limits = 9;
		/* fall through */
	case tmp100:
	case tmp101:
		set_mask |= 3 << 5;		/* 12-bit mode */
		data->resolution = 12;
		data->sample_time = HZ;
		clr_mask |= 1 << 7;		/* not one-shot mode */
		break;
	case tmp112:
		set_mask |= 3 << 5;		/* 12-bit mode */
		clr_mask |= 1 << 7;		/* not one-shot mode */
		data->resolution = 12;
		data->sample_time = HZ / 4;
		break;
	case tmp105:
	case tmp175:
	case tmp275:
	case tmp75:
		set_mask |= 3 << 5;		/* 12-bit mode */
		clr_mask |= 1 << 7;		/* not one-shot mode */
		data->resolution = 12;
		data->sample_time = HZ / 2;
		break;
	case tmp108:
		/* The tmp108 config register is word-sized */
		set_mask = 1 << 7;		/* active high */
		clr_mask = 1 << 10;		/* comparator mode */
		data->resolution = 12;
		data->sample_time = HZ;
		break;
	}

	/* configure as specified */
	status = lm75_read_value(client, LM75_REG_CONF);
	if (status < 0) {
		dev_dbg(dev, "Can't read config? %d\n", status);
		return status;
	}
	data->orig_conf = status;
	new = status & ~clr_mask;
	new |= set_mask;
	if (status != new)
		lm75_write_value(client, LM75_REG_CONF, new);
	dev_dbg(dev, "Config %02x\n", new);

	data->hwmon_dev = hwmon_device_register_with_groups(dev, client->name,
							    data, lm75_groups);
	if (IS_ERR(data->hwmon_dev))
		return PTR_ERR(data->hwmon_dev);

	if (client->dev.of_node) {
		status = lm75_of_init(data, client);
		if (!status)
			status = lm75_init_irq_gpio(data, client);
		if (status < 0)
			dev_err(&client->dev, "unable to config IRQ.\n");
	}

	data->tz = thermal_zone_of_sensor_register(data->hwmon_dev, 0,
						   data->hwmon_dev,
						   &lm75_of_thermal_ops);
	if (IS_ERR(data->tz))
		data->tz = NULL;

	dev_info(dev, "%s: sensor '%s'\n",
		 dev_name(data->hwmon_dev), client->name);

	return 0;
}

static int lm75_remove(struct i2c_client *client)
{
	struct lm75_data *data = i2c_get_clientdata(client);

	thermal_zone_of_sensor_unregister(data->hwmon_dev, data->tz);
	gpio_free(data->irq_gpio);
	hwmon_device_unregister(data->hwmon_dev);
	lm75_write_value(client, LM75_REG_CONF, data->orig_conf);
	return 0;
}

static const struct i2c_device_id lm75_ids[] = {
	{ "adt75", adt75, },
	{ "ds1775", ds1775, },
	{ "ds75", ds75, },
	{ "ds7505", ds7505, },
	{ "g751", g751, },
	{ "lm75", lm75, },
	{ "lm75a", lm75a, },
	{ "max6625", max6625, },
	{ "max6626", max6626, },
	{ "mcp980x", mcp980x, },
	{ "stds75", stds75, },
	{ "tcn75", tcn75, },
	{ "tmp100", tmp100, },
	{ "tmp101", tmp101, },
	{ "tmp105", tmp105, },
	{ "tmp108", tmp108, },
	{ "tmp112", tmp112, },
	{ "tmp175", tmp175, },
	{ "tmp275", tmp275, },
	{ "tmp75", tmp75, },
	{ /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, lm75_ids);

#define LM75A_ID 0xA1

/* Return 0 if detection is successful, -ENODEV otherwise */
static int lm75_detect(struct i2c_client *new_client,
		       struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = new_client->adapter;
	int i;
	int conf, hyst, os;
	bool is_lm75a = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	/*
	 * Now, we do the remaining detection. There is no identification-
	 * dedicated register so we have to rely on several tricks:
	 * unused bits, registers cycling over 8-address boundaries,
	 * addresses 0x04-0x07 returning the last read value.
	 * The cycling+unused addresses combination is not tested,
	 * since it would significantly slow the detection down and would
	 * hardly add any value.
	 *
	 * The National Semiconductor LM75A is different than earlier
	 * LM75s.  It has an ID byte of 0xaX (where X is the chip
	 * revision, with 1 being the only revision in existence) in
	 * register 7, and unused registers return 0xff rather than the
	 * last read value.
	 *
	 * Note that this function only detects the original National
	 * Semiconductor LM75 and the LM75A. Clones from other vendors
	 * aren't detected, on purpose, because they are typically never
	 * found on PC hardware. They are found on embedded designs where
	 * they can be instantiated explicitly so detection is not needed.
	 * The absence of identification registers on all these clones
	 * would make their exhaustive detection very difficult and weak,
	 * and odds are that the driver would bind to unsupported devices.
	 */

	/* Unused bits */
	conf = i2c_smbus_read_byte_data(new_client, 1);
	if (conf & 0xe0)
		return -ENODEV;

	/* First check for LM75A */
	if (i2c_smbus_read_byte_data(new_client, 7) == LM75A_ID) {
		/* LM75A returns 0xff on unused registers so
		   just to be sure we check for that too. */
		if (i2c_smbus_read_byte_data(new_client, 4) != 0xff
		 || i2c_smbus_read_byte_data(new_client, 5) != 0xff
		 || i2c_smbus_read_byte_data(new_client, 6) != 0xff)
			return -ENODEV;
		is_lm75a = 1;
		hyst = i2c_smbus_read_byte_data(new_client, 2);
		os = i2c_smbus_read_byte_data(new_client, 3);
	} else { /* Traditional style LM75 detection */
		/* Unused addresses */
		hyst = i2c_smbus_read_byte_data(new_client, 2);
		if (i2c_smbus_read_byte_data(new_client, 4) != hyst
		 || i2c_smbus_read_byte_data(new_client, 5) != hyst
		 || i2c_smbus_read_byte_data(new_client, 6) != hyst
		 || i2c_smbus_read_byte_data(new_client, 7) != hyst)
			return -ENODEV;
		os = i2c_smbus_read_byte_data(new_client, 3);
		if (i2c_smbus_read_byte_data(new_client, 4) != os
		 || i2c_smbus_read_byte_data(new_client, 5) != os
		 || i2c_smbus_read_byte_data(new_client, 6) != os
		 || i2c_smbus_read_byte_data(new_client, 7) != os)
			return -ENODEV;
	}

	/* Addresses cycling */
	for (i = 8; i <= 248; i += 40) {
		if (i2c_smbus_read_byte_data(new_client, i + 1) != conf
		 || i2c_smbus_read_byte_data(new_client, i + 2) != hyst
		 || i2c_smbus_read_byte_data(new_client, i + 3) != os)
			return -ENODEV;
		if (is_lm75a && i2c_smbus_read_byte_data(new_client, i + 7)
				!= LM75A_ID)
			return -ENODEV;
	}

	strlcpy(info->type, is_lm75a ? "lm75a" : "lm75", I2C_NAME_SIZE);

	return 0;
}

#ifdef CONFIG_PM
static int lm75_suspend(struct device *dev)
{
	int status;
	struct i2c_client *client = to_i2c_client(dev);
	struct lm75_data *data = i2c_get_clientdata(client);

	status = lm75_read_value(client, LM75_REG_CONF);
	if (status < 0) {
		dev_dbg(&client->dev, "Can't read config? %d\n", status);
		return status;
	}
	if (data->sensor == tmp108)
		status = ((status & ~(TMP108_MODE_MASK)) | TMP108_SHUTDOWN);
	else
		status = status | LM75_SHUTDOWN;
	lm75_write_value(client, LM75_REG_CONF, status);
	return 0;
}

static int lm75_resume(struct device *dev)
{
	int status;
	struct i2c_client *client = to_i2c_client(dev);
	struct lm75_data *data = i2c_get_clientdata(client);

	status = lm75_read_value(client, LM75_REG_CONF);
	if (status < 0) {
		dev_dbg(&client->dev, "Can't read config? %d\n", status);
		return status;
	}
	if (data->sensor == tmp108)
		status = ((status & ~(TMP108_MODE_MASK)) | TMP108_CONT);
	else
		status = status & ~LM75_SHUTDOWN;
	lm75_write_value(client, LM75_REG_CONF, status);
	return 0;
}

static const struct dev_pm_ops lm75_dev_pm_ops = {
	.suspend	= lm75_suspend,
	.resume		= lm75_resume,
};
#define LM75_DEV_PM_OPS (&lm75_dev_pm_ops)
#else
#define LM75_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static struct i2c_driver lm75_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "lm75",
		.pm	= LM75_DEV_PM_OPS,
	},
	.probe		= lm75_probe,
	.remove		= lm75_remove,
	.id_table	= lm75_ids,
	.detect		= lm75_detect,
	.address_list	= normal_i2c,
};

/*-----------------------------------------------------------------------*/

/* register access */

/*
 * All registers are word-sized, except for the configuration register.
 * LM75 uses a high-byte first convention, which is exactly opposite to
 * the SMBus standard. On tmp108, the configuratoin register is also
 * word-sized.
 */
static int lm75_read_value(struct i2c_client *client, u8 reg)
{
	struct lm75_data *data = i2c_get_clientdata(client);

	if ((reg == LM75_REG_CONF) && (!data->sensor == tmp108))
		return i2c_smbus_read_byte_data(client, reg);
	else
		return i2c_smbus_read_word_swapped(client, reg);
}

static int lm75_write_value(struct i2c_client *client, u8 reg, u16 value)
{
	struct lm75_data *data = i2c_get_clientdata(client);

	if ((reg == LM75_REG_CONF) && (!data->sensor == tmp108))
		return i2c_smbus_write_byte_data(client, reg, value);
	else
		return i2c_smbus_write_word_swapped(client, reg, value);
}

static struct lm75_data *lm75_update_device(struct device *dev)
{
	struct lm75_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct lm75_data *ret = data;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + data->sample_time)
	    || !data->valid) {
		int i;
		dev_dbg(&client->dev, "Starting lm75 update\n");

		for (i = 0; i < ARRAY_SIZE(data->temp); i++) {
			int status;

			status = lm75_read_value(client, LM75_REG_TEMP[i]);
			if (unlikely(status < 0)) {
				dev_dbg(dev,
					"LM75: Failed to read value: reg %d, error %d\n",
					LM75_REG_TEMP[i], status);
				ret = ERR_PTR(status);
				data->valid = 0;
				goto abort;
			}
			data->temp[i] = status;
		}
		data->last_updated = jiffies;
		data->valid = 1;
	}

abort:
	mutex_unlock(&data->update_lock);
	return ret;
}

module_i2c_driver(lm75_driver);

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>");
MODULE_DESCRIPTION("LM75 driver");
MODULE_LICENSE("GPL");
