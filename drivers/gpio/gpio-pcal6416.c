/*
** =============================================================================
** Copyright (c) 2017  Motorola Mobility LLC.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** File:
**     gpio-pcal6416.c
**
** Description:
**     pcal6416 chip driver
**
** =============================================================================
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/export.h>
#include <linux/bitops.h>
#include <linux/pcal6416.h>
#include <soc/qcom/bootinfo.h>
#include "gpiolib.h"
static struct regmap_config pcal6416_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = PCAL6416_REG_OUTPUT_CONTROL,
	.cache_type = REGCACHE_NONE,
};

static int pcal6416_reg_read(struct pcal6416_data *data, unsigned char reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(data->mpRegmap, reg, &val);
	if (ret < 0) {
		dev_err(data->dev,
			"%s reg=0x%x error %d\n", __func__, reg, ret);
		return ret;
	} else
		return val;
}

static int pcal6416_reg_write(struct pcal6416_data *data,
		  unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(data->mpRegmap, reg, val);

	if (ret < 0) {
		dev_err(data->dev,
			"%s reg=0x%x, value=0%x error %d\n",
			__func__, reg, val, ret);
	}

	return ret;
}

static int pcal6416_reg_read_one_io(struct pcal6416_data *data,
		  unsigned char reg_base, unsigned char offset)
{
	int reg = IO_REGISTER(reg_base, offset);
	int reg_val = -1;
	int io = FIND_IO_BIT(offset);

	reg_val = pcal6416_reg_read(data, reg);
	if (reg_val < 0) {
		dev_err(data->dev, "%s reg read error %d\n",
			__func__, reg_val);
		return reg_val;
	}

	return !!(reg_val & BIT_MASK(io));
}

static int pcal6416_reg_write_one_io(struct pcal6416_data *data,
		  unsigned char reg_base, unsigned char offset, unsigned char value)
{
	int reg = IO_REGISTER(reg_base, offset);
	int reg_val = -1;
	int io = FIND_IO_BIT(offset);

	value = !!value;
	reg_val = pcal6416_reg_read(data, reg);
	if (reg_val < 0) {
		dev_err(data->dev, "%s reg read error %d\n", __func__, reg_val);
		return reg_val;
	}

	reg_val = (reg_val & ~(BIT_MASK(io))) | (value << io);

	return pcal6416_reg_write(data, reg, reg_val);
}

static int pcal6416_pin_get(struct gpio_chip *gpio_chip, unsigned offset)
{
	struct pcal6416_data *data = dev_get_drvdata(gpio_chip->dev);
	int val;

	if (data->all_gpio_config[offset] == GPIO_IN)
		val = pcal6416_reg_read_one_io(data, PCAL6416_REG_INPUT_VAL, offset);
	else
		val = pcal6416_reg_read_one_io(data, PCAL6416_REG_OUTPUT_VAL, offset);

	return val;
}

static void pcal6416_pin_set(struct gpio_chip *gpio_chip,
		unsigned offset, int value)
{
	struct pcal6416_data *data = dev_get_drvdata(gpio_chip->dev);

	pcal6416_reg_write_one_io(data, PCAL6416_REG_OUTPUT_VAL, offset, value);
}

static int pcal6416_pin_direction_input(struct gpio_chip *gpio_chip,
		unsigned offset)
{
	int rc = 0;

	struct pcal6416_data *data = dev_get_drvdata(gpio_chip->dev);

	rc = pcal6416_reg_write_one_io(data, PCAL6416_REG_CONFIG, offset, GPIO_IN);

	if (!rc)
		data->all_gpio_config[offset] = GPIO_IN;

	return rc;
}

static int pcal6416_pin_direction_output(struct gpio_chip *gpio_chip,
		unsigned offset,
		int val)
{
	int rc = 0;

	struct pcal6416_data *data = dev_get_drvdata(gpio_chip->dev);

	if (val >= 0)
		pcal6416_pin_set(gpio_chip, offset, val);

	rc = pcal6416_reg_write_one_io(data, PCAL6416_REG_CONFIG, offset, GPIO_OUT);
	if (!rc)
		data->all_gpio_config[offset] = GPIO_OUT;

	return rc;
}

static int pcal6416_pin_of_gpio_xlate(struct gpio_chip *gpio_chip,
				   const struct of_phandle_args *gpio_spec,
				   u32 *flags)
{
	pr_info("%s cunt %d, arg0 %d, arg1 %d\n", __func__,
			gpio_spec->args_count, gpio_spec->args[0], gpio_spec->args[1]);
	if (flags)
		*flags = gpio_spec->args[1];
	return gpio_spec->args[0];
}

static int pcal6416_get_all_gpio_config(struct pcal6416_data *data)
{
	int i;
	int reg_val;

	reg_val = pcal6416_reg_read(data, PCAL6416_REG_CONFIG);
	for (i = 0; i < REGISTER_BIT_NUM; i++)
		data->all_gpio_config[i] = (reg_val & BIT_MASK(i)) ? GPIO_IN : GPIO_OUT;

	reg_val = pcal6416_reg_read(data, PCAL6416_REG_CONFIG + 1);
	for (i = 0; i < REGISTER_BIT_NUM; i++)
		data->all_gpio_config[REGISTER_BIT_NUM + i] = (reg_val & BIT_MASK(i)) ? GPIO_IN : GPIO_OUT;


	return 0;
}

static int pcal6416_of_init(struct i2c_client *client, struct pcal6416_data *data)
{
	struct device_node *node = client->dev.of_node;
	int rc = 0;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "%s: of_node do not find\n", __func__);
		rc = -1;
		goto end;
	}

	rc = of_property_read_u32(node, "pcal,pin-num", &data->gpio_num);
	if (rc) {
		dev_err(&client->dev, "%s: unable to get qcom,pin-num property\n",
							__func__);
		goto end;
	}

	data->reset_gpio = of_get_named_gpio(node, "pcal,reset-gpio", 0);
	if (data->reset_gpio < 0) {
		pr_err("failed to get pcal,reset-gpio.\n");
		rc = data->reset_gpio;
		goto end;
	}

	if (gpio_is_valid(data->reset_gpio)) {
		rc = gpio_request(data->reset_gpio, PCAL_DEVICE_NAME "NRST");
		if (rc == 0) {
			gpio_direction_output(data->reset_gpio, 0);
			udelay(1);
			gpio_direction_output(data->reset_gpio, 1);
			udelay(1);
		} else if (rc == -EBUSY) {
			struct gpio_desc *gpio_desc = gpio_to_desc(data->reset_gpio);
			if (gpio_desc != NULL && gpio_desc->label != NULL &&
				strncmp(gpio_desc->label, PCAL_DEVICE_NAME, strlen(PCAL_DEVICE_NAME)) == 0) {
				dev_info(data->dev, "%s: GPIO %d has been requested\n",
				__func__, data->reset_gpio);
				rc = 0;
			} else
				dev_err(data->dev, "%s: GPIO %d request error\n",
					__func__, data->reset_gpio);
			}
	}
end:
	return rc;
}

static int pcal6416_gpiochip_init(struct i2c_client *client)
{
	int rc = 0;
	struct pcal6416_data *data = client->dev.driver_data;

	if (!data) {
		dev_err(&client->dev, "%s: Can't add gpio chip, rc = %d\n",
								__func__, rc);
		return -ENODEV;
	}

	data->gpio_chip.base = -1;
	data->gpio_chip.ngpio = data->gpio_num;
	data->gpio_chip.label = "pcal_pin";
	data->gpio_chip.direction_input = pcal6416_pin_direction_input;
	data->gpio_chip.direction_output = pcal6416_pin_direction_output;
	data->gpio_chip.get = pcal6416_pin_get;
	data->gpio_chip.set = pcal6416_pin_set;
	data->gpio_chip.dev = &client->dev;
	data->gpio_chip.of_xlate = pcal6416_pin_of_gpio_xlate;
	data->gpio_chip.of_gpio_n_cells = 2;
	data->gpio_chip.can_sleep = 0;

	rc = gpiochip_add(&data->gpio_chip);
	if (rc) {
		dev_err(&client->dev, "%s: Can't add gpio chip, rc = %d\n",
								__func__, rc);
		rc = -EIO;
	}

	return rc;
}

static int pcal6416_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct pcal6416_data *data;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s:I2C check failed\n", __func__);
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct pcal6416_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "%s:no memory\n", __func__);
		return -ENOMEM;
	}

	data->i2c_client = client;
	data->dev = &client->dev;
	data->mpRegmap = devm_regmap_init_i2c(client, &pcal6416_i2c_regmap);
	if (IS_ERR(data->mpRegmap)) {
		err = PTR_ERR(data->mpRegmap);
		dev_err(data->dev,
			"%s:Failed to allocate register map: %d\n",
			__func__, err);
		return err;
	}

	if (pcal6416_of_init(client, data)) {
		dev_err(data->dev, "pcal6416_of_init failed\n");
		return err;
	}

	data->all_gpio_config = devm_kzalloc(&client->dev, data->gpio_num, GFP_KERNEL);
	if (IS_ERR(data->all_gpio_config)) {
		err = PTR_ERR(data->all_gpio_config);
		dev_err(data->dev,
			"%s:Failed to allocate gpio config: %d\n",
			__func__, err);
		return err;
	}

	i2c_set_clientdata(client, data);

	err = pcal6416_gpiochip_init(client);
	if (err) {
		dev_err(data->dev, "pcal6416 init gpiochip failed (%d)\n", err);
		return err;
	}

	data->chip_registered = true;
	pcal6416_get_all_gpio_config(data);

	dev_info(data->dev, "pcal6416 probe succeeded\n");

	return 0;
}

static int pcal6416_free_gpiochip(struct pcal6416_data *data)
{
	int rc = 0;

	if (data->chip_registered)
		gpiochip_remove(&data->gpio_chip);

	kfree(data->all_gpio_config);
	kfree(data);

	return rc;
}

static int pcal6416_remove(struct i2c_client *client)
{
	struct pcal6416_data *data = i2c_get_clientdata(client);

	pcal6416_free_gpiochip(data);
	dev_err(data->dev, "%s remove driver\n", __func__);

	return 0;
}

static struct of_device_id pcal6416_match_table[] = {
	{	.compatible = "pcal,pcal-pin",
	},
	{}
};


static struct i2c_device_id pcal6416_id_table[] = {
	{PCAL_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pcal6416_id_table);

static struct i2c_driver pcal6416_driver = {
	.driver = {
		   .name = PCAL_DEVICE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(pcal6416_match_table),
		   },
	.id_table = pcal6416_id_table,
	.probe = pcal6416_probe,
	.remove = pcal6416_remove,
};

static int __init pcal6416_init(void)
{
	return i2c_add_driver(&pcal6416_driver);
}

static void __exit pcal6416_exit(void)
{
	i2c_del_driver(&pcal6416_driver);
}

fs_initcall(pcal6416_init);
module_exit(pcal6416_exit);

MODULE_AUTHOR("Motorola Mobility LLC.");
MODULE_DESCRIPTION("Driver for " PCAL_DEVICE_NAME);
