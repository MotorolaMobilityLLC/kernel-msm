/*
 * Copyright (C) 2010-2012 Motorola, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/tps65132.h>


struct tps65132_data {
	struct i2c_client *client;
	struct tps65132_platform_data *pdata;
	struct regulator *regulator;  /*tps65132 input volt B+*/
};

/* volt postive/negative output level reg */
#define	REG_VPOS		0x00
#define REG_VNEG		0x01
/* select apps and discharge configure reg */
#define REG_FREQ_APPS_DIS	0x03

#define VPOS_MASK		0x1f
#define VNEG_MASK		0x1f
#define FREQ_APPS_DIS_MASK	0x43

#define VOLT_5P6		0x10
#define	FREQ_APPS_DIS_VALUE	0x03	/* smart phones, actively discharge */

#define I2C_RETRY_DELAY		10    /* 10ms */
#define I2C_MAX_RW_RETRIES	10    /* retry 10 times max */

#ifdef DEBUG
struct tps65132_reg {
	const char *name;
	uint8_t reg;
} lp8556_regs[] = {
	{ "VPOS", REG_VPOS},
	{ "VNEG", REG_VNEG},
	{ "FREQ_APPS_DIS", REG_FREQ_APPS_DIS},
};
#endif


static int tps65132_read_reg(struct tps65132_data *tps65132_data, uint8_t reg,
		   uint8_t *value)
{
	int error = 0;
	int i = 0;

	if (!value) {
		pr_err("%s: invalid value pointer\n", __func__);
		return -EINVAL;
	}

	do {
		error = i2c_smbus_read_i2c_block_data(tps65132_data->client,
			reg, 1, value);
		if (error < 0) {
			pr_err("%s: i2c_smbus_read_i2c_block_data: %d\n",
				__func__, error);
			msleep(I2C_RETRY_DELAY);
		}
	} while ((error < 0) &&	((++i) < I2C_MAX_RW_RETRIES));

	if (error < 0)
		pr_err("%s: i2c_smbus_read_i2c_block_data [0x%02X] failed:%d\n",
			__func__, reg, error);

	pr_debug("...%s: R[0x%02X]=0x%02X\n", __func__, reg, *value);

	return error;
}

static int tps65132_write_reg(struct tps65132_data *tps65132_data, uint8_t reg,
			uint8_t value)
{
	int i = 0;
	int error;

	pr_debug("...%s: W[0x%02X]=0x%02X\n", __func__, reg, value);

	do {
		error = i2c_smbus_write_byte_data(tps65132_data->client,
			 reg, value);
		if (error < 0) {
			pr_err("%s: i2c_smbus_write_byte_data: %d\n",
				__func__, error);
			msleep(I2C_RETRY_DELAY);
		}
	} while ((error < 0) && ((++i) < I2C_MAX_RW_RETRIES));

	if (error < 0)
		pr_err("%s: i2c_smbus_write_byte_data [0x%02X] failed:%d\n",
			__func__, reg, error);

	return error;
}

/* TPS65132 init register */
static int tps65132_init_registers(struct tps65132_data *tps65132_data)
{
	uint8_t value = 0;
	int error;

	pr_debug("...%s: +\n", __func__);

	error = tps65132_read_reg(tps65132_data, REG_VPOS, &value);
	if (error < 0) {
		pr_err("%s: Register VPOS read failed\n", __func__);
		return -ENODEV;
	}

	value = (value & ~VPOS_MASK) | VOLT_5P6;
	if (tps65132_write_reg(tps65132_data, REG_VPOS, value)) {
		pr_err("%s: Register VPOS initialization failed\n", __func__);
		return -EINVAL;
	}
	error = tps65132_read_reg(tps65132_data, REG_VNEG, &value);
	if (error < 0) {
		pr_err("%s: Register VNEG read failed\n", __func__);
		return -ENODEV;
	}

	value = (value & ~VNEG_MASK) | VOLT_5P6;
	if (tps65132_write_reg(tps65132_data, REG_VNEG, value)) {
		pr_err("%s: Register VNEG initialization failed\n", __func__);
		return -EINVAL;
	}

	error = tps65132_read_reg(tps65132_data, REG_FREQ_APPS_DIS, &value);
	if (error < 0) {
		pr_err("%s: Register VNEG read failed\n", __func__);
		return -ENODEV;
	}

	value = (value & ~FREQ_APPS_DIS_MASK) | FREQ_APPS_DIS_VALUE;
	if (tps65132_write_reg(tps65132_data, REG_FREQ_APPS_DIS, value)) {
		pr_err("%s: Register FREQ-APPS-DIS initialization failed\n",
			__func__);
		return -EINVAL;
	}

	pr_debug("...%s: -\n", __func__);

	return 0;
}


#ifdef DEBUG
static ssize_t ld_tps65132_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
			dev);
	struct tps65132_data *tps65132_data = i2c_get_clientdata(client);
	unsigned i, n, reg_count;
	uint8_t value = 0;

	reg_count = sizeof(tps65132_regs) / sizeof(tps65132_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		tps65132_read_reg(tps65132_data, tps65132_regs[i].reg, &value);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       tps65132_regs[i].name,
			       value);
	}

	return n;
}

static ssize_t ld_tps65132_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
						 dev);
	struct tps65132_data *tps65132_data = i2c_get_clientdata(client);
	unsigned i, reg_count, value;
	int error;
	char name[30];

	if (count >= 30) {
		pr_err("%s: input too long\n", __func__);
		return -ENINVAL;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s: unable to parse input\n", __func__);
		return -EINVAL;
	}

	reg_count = sizeof(tps65132_regs) / sizeof(tps65132_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strncmp(name, tps65132_regs[i].name, 30)) {
			error = tps65132_write_reg(tps65132_data,
				tps65132_regs[i].reg,
				value);
			if (error) {
				pr_err("%s: Failed to write register \"%s\"\n",
					__func__, name);
				return -EINVAL;
			}
			return count;
		}
	}

	pr_err("%s: no such register \"%s\"\n", __func__, name);
	return -EINVAL;
}
static DEVICE_ATTR(registers, 0644, ld_tps65132_registers_show,
		ld_tps65132_registers_store);
#endif


static int tps65132_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct tps65132_platform_data *pdata = client->dev.platform_data;
	struct tps65132_data *tps65132_data = NULL;
	int error = 0;

	pr_debug("...%s +\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	} else if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	tps65132_data = kzalloc(sizeof(struct tps65132_data), GFP_KERNEL);
	if (tps65132_data == NULL) {
		error = -ENOMEM;
		goto err_alloc_data_failed;
	}

	tps65132_data->client = client;
	tps65132_data->pdata = client->dev.platform_data;
	i2c_set_clientdata(client, tps65132_data);

	error = tps65132_init_registers(tps65132_data);
	if (error < 0) {
		pr_err("%s: Register Initialization failed: %d\n",
			__func__, error);
		error = -ENODEV;
		goto err_reg_init_failed;
	}

#ifdef DEBUG
	error = device_create_file(tps65132_data->led_dev.dev,
			&dev_attr_registers);
	if (error < 0)
		pr_err("%s: File device creation failed: %d\n",
			__func__, error);
#endif

	pr_debug("...%s -\n", __func__);
	return 0;

err_reg_init_failed:
	kfree(tps65132_data);
	i2c_set_clientdata(client, NULL);
err_alloc_data_failed:
	return error;
}

static int tps65132_remove(struct i2c_client *client)
{
	struct tps65132_data *tps65132_data = i2c_get_clientdata(client);

#ifdef DEBUG
	device_remove_file(tps65132_data->led_dev.dev, &dev_attr_registers);
#endif
	kfree(tps65132_data);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id tps65132_id[] = {
	{TPS65132_NAME, 0},
	{}
};

static struct i2c_driver tps65132_i2c_driver = {
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	.id_table = tps65132_id,
	.driver = {
		.name = TPS65132_NAME,
		.owner = THIS_MODULE,
		},
};

static int __init tps65132_init(void)
{
	pr_debug("...%s\n", __func__);
	return i2c_add_driver(&tps65132_i2c_driver);
}

static void __exit tps65132_exit(void)
{
	pr_debug("...%s\n", __func__);
	i2c_del_driver(&tps65132_i2c_driver);
}

module_init(tps65132_init);
module_exit(tps65132_exit);

MODULE_DESCRIPTION("Driver for TPS65132 dual output lcd bias");
MODULE_AUTHOR("e12499@motorola.com");
