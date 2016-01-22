/*
 * tusb320.c, TUSB320 USB TYPE-C Controller device driver
 *
 * Copyright (C) 2015 Longcheer Co.Ltd
 * Author: kun Pang <pangkun@longcheer.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "TUSB:%s: " fmt, __func__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include "tusb320.h"

#define REG_DEV_ID0			0x01
#define REG_DEV_ID1			0x02
#define REG_DEV_ID2			0x03
#define REG_DEV_ID3			0x04
#define REG_DEV_ID4			0x05
#define REG_DEV_ID5			0x06
#define REG_DEV_ID6			0x07

#define REG_CURRENT			0x08
#define REG_DETECT			0x09
#define REG_CONFIG			0x0A

/*    REG_TYPE (0x08)    */
#define TYPE_AUDIO_ACC			0x04
#define TYPE_AUDIO_CHG_TR_ACC			0x05
#define TYPE_DBG_ACC_DFP			0x06
#define TYPE_DBG_ACC_UFP			0x07

/*    REG_INT (0x09)    */
#define INT_DETACH_SHIFT		6
#define INT_DETACH			(0x00 << INT_DETACH_SHIFT)

enum tusb320_orient {
	TUSB320_ORIENT_NO_CONN = 0,
	TUSB320_ORIENT_CC1_CC,
	TUSB320_ORIENT_CC2_CC,
	TUSB320_ORIENT_FAULT
};

struct tusb320_info {
	struct i2c_client		*i2c;
	struct device *dev_t;
	struct mutex		mutex;
	struct class *tusb_class;
	int irq;
	enum tusb320_type tusb_type;
	enum tusb320_orient tusb_orient;
};

static int tusb320_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct tusb320_info *info = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&info->mutex);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&info->mutex);
	if (ret < 0) {
		pr_err("reg(0x%x), ret=%d\n", reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int tusb320_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct tusb320_info *info = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&info->mutex);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&info->mutex);
	if (ret < 0)
		pr_err("reg(0x%x), ret=%d\n", reg, ret);

	return ret;
}

static void tusb320_check_type(struct tusb320_info *info, u8 type)
{
	type = type & (0x07 << 1);

	if (type & TYPE_AUDIO_ACC)
		info->tusb_type = TUSB320_TYPE_AUDIO;
	else if (type & TYPE_AUDIO_CHG_TR_ACC)
		info->tusb_type = TUSB320_TYPE_POWER_ACC;
	else if (type & TYPE_DBG_ACC_DFP)
		info->tusb_type = TUSB320_TYPE_DEBUG_DFP;
	else if (type & TYPE_DBG_ACC_UFP)
		info->tusb_type = TUSB320_TYPE_DEBUG_UFP;
	else
		pr_err("No device type!\n");
}

static void tusb320_check_orient(struct tusb320_info *info, u8 status)
{
	u8 orient = ((status & (0x1 << 5)) >> 5);

	info->tusb_orient = orient;
}

static irqreturn_t tusb320_irq_thread(int irq, void *handle)
{
	u8 intr, rdata;
	struct tusb320_info *info = (struct tusb320_info *)handle;

	tusb320_read_reg(info->i2c, REG_DETECT, &intr);
	pr_err("\n type<%d> int(0x%02x)\n", info->tusb_type, intr);

	if (intr >> INT_DETACH_SHIFT != 0) {
		pr_err("Attach interrupt!\n");

		tusb320_read_reg(info->i2c, REG_CURRENT, &rdata);
		tusb320_check_type(info, rdata);
		pr_err("TYPE is %d, rdata is 0x%x\n", info->tusb_type, rdata);

		tusb320_read_reg(info->i2c, REG_DETECT, &rdata);
		tusb320_check_orient(info, rdata);
		pr_err("Orient is %d\n", info->tusb_orient);

	} else {
		/* Detach */
		pr_err("Detach interrupt! or fake irq\n");
		info->tusb_type = TUSB320_TYPE_NONE;
	}

	/* clear irq */
	tusb320_write_reg(info->i2c, REG_DETECT, 0x10);

	return IRQ_HANDLED;
}

static int tusb320_initialization(struct tusb320_info *info)
{
	u8 id;

	info->tusb_type = TUSB320_TYPE_NONE;

	tusb320_write_reg(info->i2c, 0x0a, 0x08);

	tusb320_read_reg(info->i2c, 0x01, &id);
	if (0 == id) {
		pr_err("Can't get tusb320L ID\n");
		return -ENODEV;
	}

	return 0;
}

static int parse_device_info(struct device *dev, struct tusb320_info *info)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	info->irq = of_get_named_gpio(np, "qcom,fusb-irq", 0);
	if ((!gpio_is_valid(info->irq))) {
		pr_err("Error reading info->irq =%d\n", info->irq);
		ret = -EINVAL;
		goto err_get;
	} else
		pr_err("info->irq=%d\n", info->irq);

	ret = gpio_request(info->irq, "tusb-irq-pin");
	if (ret) {
		pr_err("unable to request gpio [%d]\n", info->irq);
		goto err_request;
	}

	ret = gpio_direction_input(info->irq);
	if (ret) {
		pr_err("unable to set gpio [%d]\n", info->irq);
		goto err_set;
	}

	return ret;

err_set:
	gpio_free(info->irq);
err_request:
err_get:
	ret = -EINVAL;
	return ret;
}

static int tusb320_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int irq_pin;
	struct tusb320_info *info;

	info = kzalloc(sizeof(struct tusb320_info), GFP_KERNEL);
	info->i2c = client;

	pr_err("Enter into probe\n");
	ret = parse_device_info(&client->dev, info);
	if (ret < 0) {
		pr_err("invalid eint number - %d\n", info->irq);
		return -EINVAL;
	}

	i2c_set_clientdata(client, info);

	mutex_init(&info->mutex);

	info->tusb_class = class_create(THIS_MODULE, "type-c");
	info->dev_t = device_create(info->tusb_class, NULL, 0, NULL, "tusb320");
	dev_set_drvdata(info->dev_t, info);

	ret = tusb320_initialization(info);
	if (ret < 0)
		goto init_ic_failed;

	irq_pin = gpio_to_irq(info->irq);
	ret = request_threaded_irq(irq_pin, NULL, tusb320_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "tusb320_irq", info);
	if (ret) {
		dev_err(&client->dev, "failed to request IRQ\n");
		goto request_irq_failed;
	}

	ret = enable_irq_wake(irq_pin);
	if (ret < 0) {
		dev_err(&client->dev, "failed to enable wakeup src %d\n", ret);
		goto enable_irq_failed;
	}

	return 0;

enable_irq_failed:
	free_irq(info->irq, NULL);
request_irq_failed:
init_ic_failed:
	gpio_free(info->irq);
	device_destroy(info->tusb_class, 0);
	class_destroy(info->tusb_class);
	mutex_destroy(&info->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(info);

	return ret;
}

static int tusb320_remove(struct i2c_client *client)
{
	struct tusb320_info *info = i2c_get_clientdata(client);

	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, info);
	}
	device_destroy(info->tusb_class, 0);
	class_destroy(info->tusb_class);
	mutex_destroy(&info->mutex);
	i2c_set_clientdata(client, NULL);

	kfree(info);

	return 0;
}

static int  tusb320_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int  tusb320_resume(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id tusb320_id[] = {
		{.compatible = "tusb320"},
		{},
};
MODULE_DEVICE_TABLE(of, tusb320_id);

static const struct i2c_device_id tusb320_i2c_id[] = {
	{ "tusb320", 0 },
	{ }
};

static struct i2c_driver tusb320_i2c_driver = {
	.driver = {
		.name = "tusb320",
		.of_match_table = of_match_ptr(tusb320_id),
	},
	.probe    = tusb320_probe,
	.remove   = tusb320_remove,
	.suspend  = tusb320_suspend,
	.resume	  = tusb320_resume,
	.id_table = tusb320_i2c_id,
};

static __init int tusb320_i2c_init(void)
{
	return i2c_add_driver(&tusb320_i2c_driver);
}

static __exit void tusb320_i2c_exit(void)
{
	i2c_del_driver(&tusb320_i2c_driver);
}

module_init(tusb320_i2c_init);
module_exit(tusb320_i2c_exit);

MODULE_AUTHOR("pangkun@longcheer.net");
MODULE_DESCRIPTION("I2C bus driver for TUSB320 USB Type-C");
MODULE_LICENSE("GPL v2");
