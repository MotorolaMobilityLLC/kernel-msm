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

#include <linux/himax_platform.h>

#define CONFIG_TOUCHSCREEN_HIMAX_DEBUG
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define D(x...) printk(KERN_DEBUG "[HXTP] " x)
#define I(x...) printk(KERN_INFO "[HXTP] " x)
#define W(x...) printk(KERN_WARNING "[HXTP][WARNING] " x)
#define E(x...) printk(KERN_ERR "[HXTP][ERROR] " x)
#endif

int irq_enable_count = 0;


#ifdef QCT
int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
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

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(10);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
	uint8_t buf[length + 1];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	buf[0] = command;
	memcpy(buf+1, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_read_command(struct i2c_client *client, uint8_t length, uint8_t *data, uint8_t *readlength, uint8_t toRetry)
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

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
		       __func__, toRetry);
		return -EIO;
	}
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command, uint8_t toRetry)
{
	return i2c_himax_write(client, command, NULL, 0, toRetry);
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
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

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(10);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
		       __func__, toRetry);
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
	I("irq_enable_count = %d", irq_enable_count);
}

void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

int himax_gpio_power_config(struct i2c_client *client,struct himax_i2c_platform_data *pdata)
{
	int error = 0;

	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");
		if (error < 0)
			E("%s: request 3v3_en pin failed\n", __func__);
		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en pin =%d\n", gpio_get_value(pdata->gpio_3v3_en));
	}

	pdata->vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
	if (IS_ERR(pdata->vcc_i2c)) {
		error = PTR_ERR(pdata->vcc_i2c);
		E("Regulator get failed rc=%d\n", error);
		//goto error_get_vtg_i2c;
	}
	if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
		error = regulator_set_voltage(pdata->vcc_i2c,
					1800000, 1800000);
		if (error) {
			E("regulator set_vtg failed rc=%d\n", error);
			goto reg_vcc_i2c_put;
		}
	}

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");
		if (error) {
			E("unable to request gpio [%d]\n",
						pdata->gpio_reset);
			goto free_power_gpio;
		}

		error = gpio_direction_output(pdata->gpio_reset, 1);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
				pdata->gpio_reset);
			printk("dengchoa unable to set direction for gpio [%d]\n",
				pdata->gpio_reset);
			goto free_reset_gpio;
		}
		//gpio_set_value_cansleep(pdata->gpio_reset, 1);
		msleep(20);
		gpio_set_value_cansleep(pdata->gpio_reset, 0);
		msleep(20);
		gpio_set_value_cansleep(pdata->gpio_reset, 1);

	}
	msleep(20);

return error;

reg_vcc_i2c_put:
	regulator_put(pdata->vcc_i2c);
free_reset_gpio:
	gpio_free(pdata->gpio_reset);
free_power_gpio:
	gpio_free(pdata->gpio_3v3_en);

return error;
}
#endif


