/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense TOF sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/platform_device.h>
/*
 * power specific includes
 */
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
/*
 * API includes
 */
#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "vl6180x_i2c.h"
#include "stmvl6180-i2c.h"
#include "stmvl6180-cci.h"
#include "stmvl6180.h"
#ifndef CAMERA_CCI
#define VL6180_I2C_ADDRESS  (0x52>>1)

/*
 * Global data
 */
static int stmvl6180_parse_vdd(struct device *dev, struct i2c_data *data);

/*
 * QCOM specific functions
 */
static int stmvl6180_parse_vdd(struct device *dev, struct i2c_data *data)
{
	int ret = 0;
	vl6180_dbgmsg("Enter\n");

	if (dev->of_node) {
		data->vana = regulator_get(dev, "vdd");
		if (IS_ERR(data->vana)) {
			vl6180_errmsg("vdd supply is not provided\n");
			ret = -1;
		}
	}
	vl6180_dbgmsg("End\n");

	return ret;
}

static int stmvl6180_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int rc = 0;
	struct stmvl6180_data *vl6180_data = NULL;
	struct i2c_data *data = NULL;

	vl6180_dbgmsg("Enter\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
		rc = -EIO;
		return rc;
	}

	vl6180_data = stmvl6180_getobject();
	if (vl6180_data)
		data = &(vl6180_data->client_object);
	data->client = client;

	/* setup regulator */
	stmvl6180_parse_vdd(&data->client->dev, data);

	/* setup client data */
	i2c_set_clientdata(client, vl6180_data);

	/* setup platform i2c client */
	i2c_setclient((void *)client);

	vl6180_dbgmsg("End\n");
	return rc;
}

static int stmvl6180_remove(struct i2c_client *client)
{
	struct stmvl6180_data *data = stmvl6180_getobject();
	vl6180_dbgmsg("Enter\n");

	/* Power down the device */
	stmvl6180_power_down_i2c((void *)&data->client_object);

	vl6180_dbgmsg("End\n");
	return 0;
}

#ifdef CONFIG_PM
static int stmvl6180_set_enable(struct i2c_client *client, unsigned int enable)
{
	return 0;
}
static int stmvl6180_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return stmvl6180_set_enable(client, 0);
}

static int stmvl6180_resume(struct i2c_client *client)
{
	return stmvl6180_set_enable(client, 1);
}

#else

#define stmvl6180_suspend	NULL
#define stmvl6180_resume	NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id stmvl6180_id[] = {
	{ STMVL6180_DRV_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, stmvl6180_id);

static const struct of_device_id st_stmv16180_dt_match[] = {
	{ .compatible = "st,stmvl6180", },
	{ },
};

static struct i2c_driver stmvl6180_driver = {
	.driver = {
		.name	= STMVL6180_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = st_stmv16180_dt_match,
	},
	.suspend = stmvl6180_suspend,
	.resume	= stmvl6180_resume,
	.probe	= stmvl6180_probe,
	.remove	= stmvl6180_remove,
	.id_table = stmvl6180_id,

};

int stmvl6180_power_up_i2c(void *i2c_object, unsigned int *preset_flag)
{
	int ret = 0;
	struct i2c_data *data = (struct i2c_data *)i2c_object;

	vl6180_dbgmsg("Enter\n");

	/* actual power on */
#ifndef STM_TEST
	ret = regulator_set_voltage(data->vana,	2700000, 2700000);
	if (ret < 0) {
		vl6180_errmsg("set_vol(%p) fail %d\n", data->vana , ret);
		return ret;
	}
	ret = regulator_enable(data->vana);
	msleep(3);
	if (ret < 0) {
		vl6180_errmsg("reg enable(%p) failed.rc=%d\n", data->vana, ret);
		return ret;
	}
	data->power_up = 1;
	*preset_flag = 1;
#endif

	vl6180_dbgmsg("End\n");
	return ret;
}

int stmvl6180_power_down_i2c(void *i2c_object)
{
	int ret = 0;
	struct i2c_data *data = (struct i2c_data *)i2c_object;

	vl6180_dbgmsg("Enter\n");
#ifndef STM_TEST
	msleep(3);
	ret = regulator_disable(data->vana);
	if (ret < 0) {
		vl6180_errmsg("reg disable(%p) failed.rc=%d\n", data->vana, ret);
	}
	data->power_up = 0;
#endif

	vl6180_dbgmsg("End\n");
	return ret;
}

int stmvl6180_init_i2c(void)
{
	int ret = 0;

#ifdef STM_TEST
	struct i2c_client *client = NULL;
	struct i2c_adapter *adapter;
	struct i2c_board_info info = {
		.type = "stmvl6180",
		.addr = VL6180_I2C_ADDRESS,
	};
#endif

	vl6180_dbgmsg("Enter\n");

	/* register as a i2c client device */
	ret = i2c_add_driver(&stmvl6180_driver);
	if (ret)
		vl6180_errmsg("%d erro ret:%d\n", __LINE__, ret);

#ifdef STM_TEST
	if (!ret) {
		adapter = i2c_get_adapter(4);
		if (!adapter)
			ret = -EINVAL;
		else
			client = i2c_new_device(adapter, &info);
		if (!client)
			ret = -EINVAL;
	}
#endif

	vl6180_dbgmsg("End with rc:%d\n", ret);

	return ret;
}

void stmvl6180_exit_i2c(void *i2c_object)
{
	vl6180_dbgmsg("Enter\n");
	i2c_del_driver(&stmvl6180_driver);

	vl6180_dbgmsg("End\n");
}

#endif /* end of NOT CAMERA_CCI */
