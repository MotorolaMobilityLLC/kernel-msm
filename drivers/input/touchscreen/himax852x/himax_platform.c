/* Himax Android Driver Sample Code Ver 0.3 for Himax chipset
*
* Copyright (C) 2014 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#define pr_fmt(fmt) "HXTP %s: " fmt, __func__
#include "himax_platform.h"

int irq_enable_count = 0;

int i2c_himax_read(struct i2c_client *client, uint8_t command,
	uint8_t *data, uint8_t length, uint8_t retry_max)
{
	int retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &command,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		}
	};

	for (retry = 0; retry < retry_max; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(10);
	}
	if (retry == retry_max) {
		pr_err("i2c read reg 0x%x failed, by %pS\n",
			(u32)command, __builtin_return_address(0));
		return -EIO;
	}
	return 0;
}

int i2c_himax_write(struct i2c_client *client, uint8_t command,
	uint8_t *data, uint8_t length, uint8_t retry_max)
{
	int retry;
	uint8_t buf[length + 1];

	struct i2c_msg msg[] = {
		{
			.addr  = client->addr,
			.flags = 0,
			.len   = length + 1,
			.buf   = buf,
		}
	};

	buf[0] = command;
	memcpy(buf+1, data, length);

	for (retry = 0; retry < retry_max; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == retry_max) {
		pr_err("i2c write reg 0x%x failed, by %pS\n",
			(u32)command, __builtin_return_address(0));
		return -EIO;
	}
	return 0;
}

int i2c_himax_read_command(struct i2c_client *client, uint8_t length,
	uint8_t *data, uint8_t *readlength, uint8_t retry_max)
{
	int retry;
	struct i2c_msg msg[] = {
		{
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = length,
		.buf = data,
		}
	};

	for (retry = 0; retry < retry_max; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}
	if (retry == retry_max) {
 		pr_err("i2c read reg failed\n");
		return -EIO;
	}
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client,
	uint8_t command, uint8_t retry_max)
{
	return i2c_himax_write(client, command, NULL, 0, retry_max);
}

int i2c_himax_master_write(struct i2c_client *client,
	uint8_t *data, uint8_t length, uint8_t retry_max)
{
	int retry;
	uint8_t buf[length];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buf,
		}
	};

	memcpy(buf, data, length);

	for (retry = 0; retry < retry_max; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == retry_max) {
		pr_err("i2c_write_block retry over %d\n", retry_max);
		return -EIO;
	}
	return 0;
}

void himax_int_enable(int irqnum, int enable)
{
	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
	}
}

void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

int himax_gpio_power_config(struct i2c_client *client,
	struct himax_i2c_platform_data *pdata)
{
	int err = 0;

	if (pdata->gpio_pwr_en >= 0) {
		err = gpio_request(pdata->gpio_pwr_en, "himax-power");
		if (err < 0)
			pr_err("request 3v3_en pin failed\n");
		err = gpio_direction_output(pdata->gpio_pwr_en, 1);
		if (err) {
			pr_err("unable to set direction for gpio %d\n",
				pdata->gpio_pwr_en);
			goto free_power_gpio;
		}
	}
	msleep(20);

	pdata->vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
	if (IS_ERR(pdata->vcc_i2c)) {
		err = PTR_ERR(pdata->vcc_i2c);
		pr_err("Regulator get failed rc=%d\n", err);
		goto free_power_gpio;
	}
	if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
		err = regulator_set_voltage(pdata->vcc_i2c,
			1800000, 1800000);
		if (err) {
			pr_err("regulator set_vtg failed rc=%d\n", err);
			goto reg_vcc_i2c_put;
		}
	}
	err = regulator_enable(pdata->vcc_i2c);
	if (err) {
		pr_err("Regulator vcc_i2c enable failed rc=%d\n", err);
		goto reg_vcc_i2c_put;
	}
	msleep(20);

	if (gpio_is_valid(pdata->gpio_reset)) {
		err = gpio_request(pdata->gpio_reset, "himax-reset");
		if (err) {
			pr_err("unable to request gpio %d\n", pdata->gpio_reset);
			goto reg_vcc_i2c_disable;
		}

		err = gpio_direction_output(pdata->gpio_reset, 1);
		if (err) {
			pr_err("unable to set direction for gpio %d\n",
				pdata->gpio_reset);
			goto free_reset_gpio;
		}
		msleep(20);
		gpio_set_value_cansleep(pdata->gpio_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(pdata->gpio_reset, 1);
		
	}
	msleep(20);
	return err;

free_reset_gpio:
	gpio_free(pdata->gpio_reset);
reg_vcc_i2c_disable:
	regulator_disable(pdata->vcc_i2c);
reg_vcc_i2c_put:
	regulator_put(pdata->vcc_i2c);
free_power_gpio:
	gpio_free(pdata->gpio_pwr_en);

	return err;
}
