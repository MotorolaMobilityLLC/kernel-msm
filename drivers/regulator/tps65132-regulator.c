/*
 * Copyright (C) 2013 Motorola Mobility LLC
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
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define TPS65132_NAME "tps65132"

struct tps65132_data {
	struct regulator_dev *rdev;
	int enable_p_gpio;
	int enable_n_gpio;
	u32 bias_delay_us;
	bool enabled;
};

/* volt postive/negative output level reg */
#define	VPOS_REG		0x00
#define VNEG_REG		0x01
#define ACTIVE_DIS_REG		0x03
#define APPS_REG		0x03

#define VPOS_MASK		0x1f
#define VNEG_MASK		0x1f
#define ACTIVE_DIS_REG_MASK	0x03
#define APPS_MASK		0x40

#define I2C_RETRY_DELAY		10	/* 10ms */
#define I2C_MAX_RW_RETRIES	10	/* retry 10 times max */

static int tps65132_read_reg(struct i2c_client *client, uint8_t reg,
			uint8_t *value)
{
	int error;
	int i = 0;

	do {
		error = i2c_smbus_read_i2c_block_data(client,
			reg, 1, value);
		if (error < 0) {
			dev_err(&client->dev, "%s: i2c_smbus_read_i2c_block_data: %d\n",
				__func__, error);
			msleep(I2C_RETRY_DELAY);
		}
	} while ((error < 0) && ((++i) < I2C_MAX_RW_RETRIES));

	if (error < 0)
		dev_err(&client->dev, "%s: i2c_smbus_read_i2c_block_data [0x%02X] failed:%d\n",
			__func__, reg, error);

	return error;
}

static int tps65132_write_reg(struct i2c_client *client, uint8_t reg,
			uint8_t value)
{
	int i = 0;
	int error;

	do {
		error = i2c_smbus_write_byte_data(client,
			 reg, value);
		if (error < 0) {
			dev_err(&client->dev, "%s: i2c_smbus_write_byte_data: %d\n",
				__func__, error);
			msleep(I2C_RETRY_DELAY);
		}
	} while ((error < 0) && ((++i) < I2C_MAX_RW_RETRIES));

	if (error < 0)
		dev_err(&client->dev, "%s: i2c_smbus_write_byte_data [0x%02X] failed:%d\n",
			__func__, reg, error);

	return error;
}

/* TPS65132 init register */
static int tps65132_init_registers_dt(struct i2c_client *client,
			struct device_node *node)
{
	bool active_dis;
	bool app_tablet;

	uint8_t value = 0;

	active_dis = of_property_read_bool(node, "active-dis");
	app_tablet = of_property_read_bool(node, "app-tablet");

	if (tps65132_read_reg(client, ACTIVE_DIS_REG, &value) < 0) {
		dev_err(&client->dev, "%s: Register ACTIVE_DIS_REG read failed\n",
			__func__);
		return -ENODEV;
	}

	value = (value & ~ACTIVE_DIS_REG_MASK) |
			(active_dis ? ACTIVE_DIS_REG_MASK : 0);
	if (tps65132_write_reg(client, ACTIVE_DIS_REG, value)) {
		dev_err(&client->dev, "%s: Register ACTIVE_DIS_REG initialization failed\n",
			__func__);
		return -EINVAL;
	}

	if (tps65132_read_reg(client, APPS_REG, &value) < 0) {
		dev_err(&client->dev, "%s: Register APPS_REG read failed\n",
			__func__);
		return -ENODEV;
	}

	value = (value & ~APPS_MASK) | (app_tablet ? APPS_MASK : 0);
	if (tps65132_write_reg(client, APPS_REG, value)) {
		dev_err(&client->dev, "%s: Register APPS_REG initialization failed\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

/* Index maps to dac_value to store into register */
static int tps65132_voltages[] =
{	/* voltage	index/dac_value */
	4000000,	/* 0x00 */
	4100000,	/* 0x01 */
	4200000,	/* 0x02 */
	4300000,	/* 0x03 */
	4400000,	/* 0x04 */
	4500000,	/* 0x05 */
	4600000,	/* 0x06 */
	4700000,	/* 0x07 */
	4800000,	/* 0x08 */
	4900000,	/* 0x09 */
	5000000,	/* 0x0a */
	5100000,	/* 0x0b */
	5200000,	/* 0x0c */
	5300000,	/* 0x0d */
	5400000,	/* 0x0e */
	5500000,	/* 0x0f */
	5600000,	/* 0x10 */
	5700000,	/* 0x11 */
	5800000,	/* 0x12 */
	5900000,	/* 0x13 */
	6000000,	/* 0x14 */
};

static int tps65132_regulator_enable(struct regulator_dev *rdev)
{
	struct tps65132_data *tps65132 = rdev_get_drvdata(rdev);

	if (gpio_is_valid(tps65132->enable_p_gpio))
		gpio_set_value(tps65132->enable_p_gpio, 1);

	if (tps65132->bias_delay_us)
		usleep_range(tps65132->bias_delay_us,
			tps65132->bias_delay_us + 100);

	if (gpio_is_valid(tps65132->enable_n_gpio))
		gpio_set_value(tps65132->enable_n_gpio, 1);

	tps65132->enabled = true;

	return 0;
}

static int tps65132_regulator_disable(struct regulator_dev *rdev)
{
	struct tps65132_data *tps65132 = rdev_get_drvdata(rdev);

	if (gpio_is_valid(tps65132->enable_n_gpio))
		gpio_set_value(tps65132->enable_n_gpio, 0);

	if (tps65132->bias_delay_us)
		usleep_range(tps65132->bias_delay_us,
			tps65132->bias_delay_us + 100);

	if (gpio_is_valid(tps65132->enable_p_gpio))
		gpio_set_value(tps65132->enable_p_gpio, 0);

	tps65132->enabled = false;

	return 0;
}

static int tps65132_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct tps65132_data *tps65132 = rdev_get_drvdata(rdev);

	return tps65132->enabled;
}

static int tps65132_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	uint8_t value;

	if (tps65132_read_reg(client, VPOS_REG, &value) < 0)
		return -EINVAL;

	value &= VPOS_MASK;

	if (value >= ARRAY_SIZE(tps65132_voltages))
		return -EINVAL;

	return value;
}

static int tps65132_regulator_set_voltage_sel(struct regulator_dev *rdev,
			unsigned selector)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	uint8_t value;

	value = selector & VPOS_MASK;

	if (tps65132_write_reg(client, VPOS_REG, value) < 0)
		return -EINVAL;

	value = selector & VNEG_MASK;

	if (tps65132_write_reg(client, VNEG_REG, value) < 0)
		return -EINVAL;

	return 0;
}

static int tps65132_regulator_list_voltage(struct regulator_dev *rdev,
			unsigned selector)
{
	if (selector >= ARRAY_SIZE(tps65132_voltages))
		return -EINVAL;

	return tps65132_voltages[selector];
}

static struct regulator_ops tps65132_regulator_ops = {
	.enable		= tps65132_regulator_enable,
	.disable	= tps65132_regulator_disable,
	.is_enabled	= tps65132_regulator_is_enabled,
	.get_voltage_sel = tps65132_regulator_get_voltage_sel,
	.set_voltage_sel = tps65132_regulator_set_voltage_sel,
	.list_voltage	= tps65132_regulator_list_voltage,
};

static struct regulator_desc tps65132_regulator_desc = {
	.name		= "tps65132",
	.ops		= &tps65132_regulator_ops,
	.type		= REGULATOR_VOLTAGE,
	.id			= 0,
	.owner		= THIS_MODULE,
	.n_voltages	= ARRAY_SIZE(tps65132_voltages),
};

static int tps65132_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tps65132_data *tps65132;
	struct regulator_init_data *init_data;
	struct device_node *regulator_node = NULL;
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	if (!client->dev.of_node) {
		/* Platform data not currently supported */
		dev_err(&client->dev, "%s: of devtree data not found\n", __func__);
		return -EINVAL;
	}

	tps65132 = devm_kzalloc(&client->dev, sizeof(*tps65132), GFP_KERNEL);
	if (tps65132 == NULL) {
		dev_err(&client->dev, "%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	/* Use OF devtree. */
	of_property_read_u32(client->dev.of_node, "bias-delay-us",
			&tps65132->bias_delay_us);

	tps65132->enable_p_gpio = of_get_gpio(client->dev.of_node, 0);
	if (!gpio_is_valid(tps65132->enable_p_gpio))
		dev_warn(&client->dev, "%s: enable_p_gpio not found in of devtree\n",
				__func__);

	tps65132->enable_n_gpio = of_get_gpio(client->dev.of_node, 1);
	if (!gpio_is_valid(tps65132->enable_n_gpio))
		dev_warn(&client->dev, "%s: enable_n_gpio not found in of devtree\n",
				__func__);

	for_each_child_of_node(client->dev.of_node, regulator_node)
		if (of_node_cmp(regulator_node->name, "regulator") == 0)
			break;

	if (regulator_node == NULL) {
		dev_err(&client->dev,
				"%s: unable to find regulator node within device node\n",
				__func__);
		error = -ENOMEM;
		goto err_regulator;
	}

	init_data = of_get_regulator_init_data(&client->dev, regulator_node);
	if (!init_data) {
		dev_err(&client->dev, "%s: of_get_regulator_init_data failed\n",
				__func__);
		error = -EINVAL;
		goto err_regulator;
	}

	tps65132->rdev = regulator_register(&tps65132_regulator_desc, &client->dev,
		init_data, tps65132, regulator_node);

	if (IS_ERR(tps65132->rdev)) {
		dev_err(&client->dev, "failed to register regulator %s\n",
			tps65132_regulator_desc.name);
		error = PTR_ERR(tps65132->rdev);
		goto err_regulator;
	}

	error = tps65132_init_registers_dt(client, client->dev.of_node);
	if (error < 0) {
		dev_err(&client->dev, "%s: register init failed: %d\n",
			__func__, error);
		error = -ENODEV;
		goto err_regulator_register;
	}

	tps65132->enabled = tps65132->rdev->constraints->boot_on;

	if (gpio_is_valid(tps65132->enable_p_gpio)) {
		if (gpio_request(tps65132->enable_p_gpio, "tps56132-enable-p")) {
			dev_err(&client->dev, "%s: gpio_request failed on gpio %d\n",
					__func__, tps65132->enable_p_gpio);
			error = -EINVAL;
			goto err_regulator_register;
		}
		gpio_direction_output(tps65132->enable_p_gpio, tps65132->enabled);
	}

	if (gpio_is_valid(tps65132->enable_n_gpio)) {
		if (gpio_request(tps65132->enable_n_gpio, "tps56132-enable-n")) {
			dev_err(&client->dev, "%s: gpio_request failed on gpio %d\n",
					__func__, tps65132->enable_n_gpio);
			error = -EINVAL;
			goto err_gpio_request;
		}
		gpio_direction_output(tps65132->enable_n_gpio, tps65132->enabled);
	}

	return 0;

err_gpio_request:
	gpio_free(tps65132->enable_p_gpio);
err_regulator_register:
	regulator_unregister(tps65132->rdev);
err_regulator:
	i2c_set_clientdata(client, NULL);
	return error;
}

static int tps65132_remove(struct i2c_client *client)
{
	struct regulator_dev *rdev = dev_get_drvdata(&client->dev);
	struct tps65132_data *tps65132 = rdev_get_drvdata(rdev);

	gpio_free(tps65132->enable_p_gpio);
	gpio_free(tps65132->enable_n_gpio);

	regulator_unregister(rdev);
	return 0;
}

static const struct i2c_device_id tps65132_id[] = {
	{TPS65132_NAME, 0},
	{}
};


static struct of_device_id regulator_tps65132_match_table[] = {
	{ .compatible = "ti," TPS65132_NAME, },
	{}
};

static struct i2c_driver tps65132_i2c_driver = {
	.probe = tps65132_probe,
	.remove = tps65132_remove,
	.id_table = tps65132_id,
	.driver = {
		.name = TPS65132_NAME,
		.owner = THIS_MODULE,
		.of_match_table = regulator_tps65132_match_table,
		},
};

static int __init tps65132_init(void)
{
	return i2c_add_driver(&tps65132_i2c_driver);
}

static void __exit tps65132_exit(void)
{
	i2c_del_driver(&tps65132_i2c_driver);
}

module_init(tps65132_init);
module_exit(tps65132_exit);

MODULE_DESCRIPTION("Driver for TPS65132 dual output lcd bias");
MODULE_AUTHOR("Motorola Mobility LLC");
