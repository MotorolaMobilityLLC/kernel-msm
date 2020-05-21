/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "tusb1064.h"

struct tusb1064 {
	struct device *dev;
	struct device_node *host_node;
	u8 i2c_addr;
	u32 dp_3v3_en;
	struct i2c_client *i2c_client;
	bool power_on;
};

static struct tusb1064 *pdata;
static bool standalone_mode;

static int tusb1064_read(struct i2c_client *client, u8 reg, char *buf, u32 size)
{
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = buf,
		}
	};

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		pr_err("%s i2c read failed\n", __func__);
		return -EIO;
	}
	pr_debug("%s, reg:%x buf[0]:%x\n", __func__, reg, buf[0]);

	return 0;
}

static int tusb1064_write(struct i2c_client *client, u8 reg, u8 val)
{
	u8 buf[2] = {reg, val};
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

	pr_debug("%s, reg:%x, val:%x\n", __func__, reg, val);
	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		pr_err("i2c write failed\n");
		return -EIO;
	}
	return 0;
}

void tusb1064_usb_event(bool flip)
{
	if (pdata) {
		if (flip) {
			if (standalone_mode)
				tusb1064_write(pdata->i2c_client, 0x0A, 0x05);
			else
				tusb1064_write(pdata->i2c_client, 0x0A, 0x06);
		} else {
			if (standalone_mode)
				tusb1064_write(pdata->i2c_client, 0x0A, 0x01);
			else
				tusb1064_write(pdata->i2c_client, 0x0A, 0x02);
		}
	}
}
EXPORT_SYMBOL(tusb1064_usb_event);

static int tusb1064_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	char buf[2];

	if (!client || !client->dev.of_node) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s device doesn't support I2C\n", __func__);
		return -ENODEV;
	}

	pdata = devm_kzalloc(&client->dev,
		sizeof(struct tusb1064), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = &client->dev;
	pdata->i2c_client = client;
	pr_debug("%s I2C address is %x\n", __func__, client->addr);

	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);

	tusb1064_read(pdata->i2c_client, 0x0A, buf, 2);
	tusb1064_read(pdata->i2c_client, 0x13, buf, 2);
	/*Enable 4-lane DP with FLip and enable EQ_OVERRIDe*/
	/*tusb1064_write(pdata, 0x0A, 0x13); */
	/*written usb sideEnable 4-lane DP with FLip and enable EQ_OVERRIDe */
	/*tusb1064_write(pdata, 0x0A, 0x12); */

	if (standalone_mode) {
		/*Enable 3.1 USB, no DP */
		if (tusb1064_write(pdata->i2c_client, 0x0A, 0x01) < 0)
			goto fail;
	} else {
		/*Enable 4-lane DP with Flip and enable EQ_OVERRIDe */
		if (tusb1064_write(pdata->i2c_client, 0x0A, 0x02) < 0)
			goto fail;

		pr_debug("%s setting SBU1 to AUXN, SBU2 to AUXP\n", __func__);
	}
	tusb1064_read(pdata->i2c_client, 0x0A, buf, 2);
	tusb1064_read(pdata->i2c_client, 0x13, buf, 2);
	tusb1064_read(pdata->i2c_client, 0x10, buf, 2);
	tusb1064_read(pdata->i2c_client, 0x11, buf, 2);

	pr_debug("%s probe successfully\n", __func__);
	return 0;
fail:
	devm_kfree(&client->dev, pdata);
	return -EINVAL;
}

static int tusb1064_remove(struct i2c_client *client)
{
	struct tusb1064 *pdata = i2c_get_clientdata(client);

	if (pdata)
		devm_kfree(&client->dev, pdata);
	return 0;
}

static void tusb1064_shutdown(struct i2c_client *client)
{
	dev_info(&(client->dev), "shutdown");
}

static int tusb1064_suspend(struct device *dev, pm_message_t state)
{
	dev_info(dev, "suspend");
	return 0;
}

static int tusb1064_resume(struct device *dev)
{
	dev_info(dev, "resume");
	return 0;
}

static const struct i2c_device_id tusb1064_id_table[] = {
	{"tusb1064", 0},
	{}
};

static struct i2c_driver tusb1064_i2c_driver = {
	.probe = tusb1064_probe,
	.remove = tusb1064_remove,
	.shutdown = tusb1064_shutdown,
	.driver = {
		.name = "tusb1064",
		.owner = THIS_MODULE,
		.suspend = tusb1064_suspend,
		.resume = tusb1064_resume,
	},
	.id_table = tusb1064_id_table,
};

static int __init tusb1064_init(void)
{
	char *cmdline;

	cmdline = strnstr(boot_command_line,
			"msm_drm.dsi_display0=dsi_sim_vid_display",
				strlen(boot_command_line));
	if (cmdline) {
		pr_debug("%s tethered mode cmdline:%s\n",
				__func__, cmdline);
		standalone_mode = false;
	} else {
		pr_debug("%s standalone mode cmdline:%s\n",
				__func__, cmdline);
		standalone_mode = true;
	}

	return 0;
}

device_initcall(tusb1064_init);
module_i2c_driver(tusb1064_i2c_driver);
MODULE_DEVICE_TABLE(i2c, tusb1064_id_table);
MODULE_DESCRIPTION("TUSB1064 USB Bridge");
