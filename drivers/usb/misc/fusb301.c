/*
 * fusb301.c, FUSB301 USB TYPE-C Controller device driver
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

#define pr_fmt(fmt) "FUSB:%s: " fmt, __func__

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
#include "fusb301.h"

/******************************************************************************
* Register addresses
******************************************************************************/
#define REG_DEV_ID		0x01
#define REG_MOD			0x02
#define REG_CON			0x03
#define REG_MAN			0x04
#define REG_RST			0x05
#define REG_MSK			0x10
#define REG_STAT		0x11
#define REG_TYPE		0x12
#define REG_INT			0x13

/******************************************************************************
* Register bits
******************************************************************************/
/*	  REG_DEV_ID (0x01)    */
#define ID_REV					0x0F
#define ID_VER_SHIFT			4
#define ID_VER					(0x0F << ID_VER_SHIFT)

/*    REG_MOD (0x02)    */
#define MOD_SRC				0x01
#define MOD_SRC_ACC_SHIFT		1
#define MOD_SRC_ACC			(0x01 << MOD_SRC_ACC_SHIFT)
#define MOD_SNK_SHIFT			2
#define MOD_SNK				(0x01 << MOD_SNK_SHIFT)
#define MOD_SNK_ACC_SHIFT		3
#define MOD_SNK_ACC			(0x01 << MOD_SNK_SHIFT)
#define MOD_DRP_SHIFT			4
#define MOD_DRP				(0x01 << MOD_DRP_SHIFT)
#define MOD_DRP_ACC_SHIFT		5
#define MOD_DRP_ACC			(0x01 << MOD_DRP_ACC_SHIFT)

/*    REG_CON (0x03)    */
#define CON_INT_MSK				0x01
#define CON_HOST_CUR_SHIFT		1
#define CON_HOST_CUR			(0x03 << CON_HOST_CUR_SHIFT)
#define CON_DRP_TGL_SHIFT		4
#define CON_DRP_TGL			(0x03 << CON_DRP_TGL_SHIFT)

/*    REG_MAN (0x04)    */
#define MAN_ERR_REC			0x01
#define MAN_DIS_SHIFT			1
#define MAN_DIS				(0x01 << MAN_DIS_SHIFT)
#define MAN_UNATT_SRC_SHIFT		2
#define MAN_UNATT_SRC			(0x01 << MAN_UNATT_SRC_SHIFT)
#define MAN_UNATT_SNK_SHIFT		3
#define MAN_UNATT_SNK			(0x01 << MAN_UNATT_SNK_SHIFT)

/*    REG_RST (0x05)    */
#define RST_SW				0x01

/*    REG_MSK (0x10)    */
#define MSK_ATTACH			0x01
#define MSK_DETACH_SHIFT		1
#define MSK_DETACH			(0x01 << MSK_DETACH_SHIFT)
#define MSK_BC_LVL_SHIFT		2
#define MSK_BC_LVL			(0x01 << MSK_BC_LVL_SHIFT)
#define MSK_ACC_CHG_SHIFT		3
#define MSK_ACC_CHG			(0x01 << MSK_ACC_CHG_SHIFT)

/*    REG_STAT (0x11)    */
#define STAT_ATTACH			0x01
#define STAT_BC_LVL_SHIFT		1
#define STAT_BC_LVL			(0x03 << STAT_BC_LVL_SHIFT)
#define STAT_VBUS_OK_SHIFT		3
#define STAT_VBUS_OK			(0x01 << STAT_VBUS_OK_SHIFT)
#define STAT_ORIENT_SHIFT		3
#define STAT_ORIENT			(0x03 << STAT_ORIENT_SHIFT)

/*    REG_TYPE (0x12)    */
#define TYPE_AUDIO_ACC			0x01
#define TYPE_DBG_ACC_SHIFT		1
#define TYPE_DBG_ACC			(0x01 << TYPE_DBG_ACC_SHIFT)
#define TYPE_PWR_ACC_SHIFT		2
#define TYPE_PWR_ACC			(0x01 << TYPE_PWR_ACC_SHIFT)
#define TYPE_SRC_SHIFT			3
#define TYPE_SRC			(0x01 << TYPE_SRC_SHIFT)
#define TYPE_SNK_SHIFT			4
#define TYPE_SNK			(0x01 << TYPE_SNK_SHIFT)

/*    REG_INT (0x13)    */
#define INT_ATTACH			0x01
#define INT_DETACH_SHIFT		1
#define INT_DETACH			(0x01 << INT_DETACH_SHIFT)
#define INT_BC_LVL_SHIFT		2
#define INT_BC_LVL			(0x01 << INT_BC_LVL_SHIFT)
#define INT_ACC_CHG_SHIFT		3
#define INT_ACC_CHG			(0x01 << INT_ACC_CHG_SHIFT)

/******************************************************************************/
enum fusb301_drp_toggle {
	FUSB301_TOGGLE_SNK35_SRC15 = 0,  /*default*/
	FUSB301_TOGGLE_SNK30_SRC20,
	FUSB301_TOGGLE_SNK25_SRC25,
	FUSB301_TOGGLE_SNK20_SRC30,
};

enum fusb301_host_cur {
	FUSB301_HOST_CUR_NO = 0,  /*no current*/
	FUSB301_HOST_CUR_80,  /*default USB*/
	FUSB301_HOST_CUR_180,  /*1.5A*/
	FUSB301_HOST_CUR_330,  /*3A*/
};

enum fusb301_orient {
	FUSB301_ORIENT_NO_CONN = 0,
	FUSB301_ORIENT_CC1_CC,
	FUSB301_ORIENT_CC2_CC,
	FUSB301_ORIENT_FAULT
};

enum fusb301_config_modes {
	FUSB301_MODE_SRC = 0,
	FUSB301_MODE_SRC_ACC,
	FUSB301_MODE_SNK,
	FUSB301_MODE_SNK_ACC,
	FUSB301_MODE_DRP,
	FUSB301_MODE_DRP_ACC
};

struct fusb301_info {
	struct i2c_client		*i2c;
	struct device *dev_t;
	struct mutex		mutex;
	struct class *fusb_class;
	int irq;
	enum fusb301_type fusb_type;
	enum fusb301_orient fusb_orient;
};


static int fusb301_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct fusb301_info *info = i2c_get_clientdata(i2c);
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

static int fusb301_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct fusb301_info *info = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&info->mutex);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&info->mutex);
	if (ret < 0)
		pr_err("reg(0x%x), ret=%d\n", reg, ret);

	return ret;
}

static int fusb301_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct fusb301_info *info = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&info->mutex);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));

		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&info->mutex);
	return ret;
}

static ssize_t show_current_type(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct fusb301_info *info = dev_get_drvdata(dev);

	switch (info->fusb_type) {
	case FUSB301_TYPE_AUDIO:
		return snprintf(buf, 19, "FUSB301_TYPE_AUDIO\n");
	case FUSB301_TYPE_DEBUG:
		return snprintf(buf, 19, "FUSB301_TYPE_DEBUG\n");
	case FUSB301_TYPE_POWER_ACC:
		return snprintf(buf, 23, "FUSB301_TYPE_POWER_ACC\n");
	case FUSB301_TYPE_SOURCE:
		return snprintf(buf, 15, "FUSB301_SOURCE\n");
	case FUSB301_TYPE_SINK:
		return snprintf(buf, 18, "FUSB301_TYPE_SINK\n");
	default:
		return snprintf(buf, 11, "TYPE ERROR\n");
	}
}

static ssize_t config_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int error;
	unsigned long data;
	u8 rdata;
	struct fusb301_info *info = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == FUSB301_MODE_SRC)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_SRC);
	else if (data == FUSB301_MODE_SRC_ACC)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_SRC_ACC);
	else if (data == FUSB301_MODE_SNK)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_SNK);
	else if (data == FUSB301_MODE_SNK_ACC)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_SNK_ACC);
	else if (data == FUSB301_MODE_DRP)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_DRP);
	else if (data == FUSB301_MODE_DRP_ACC)
		fusb301_write_reg(info->i2c, REG_MOD, MOD_DRP_ACC);
	else
		pr_info("Argument is not match!!\n");

	fusb301_read_reg(info->i2c, REG_MOD, &rdata);
	pr_info("REG_MOD(0x%02x)\n", rdata);

	return count;
}

static DEVICE_ATTR(type, S_IRUGO, show_current_type, NULL);
static DEVICE_ATTR(mode, S_IWUSR, NULL, config_mode);

static void fusb301_check_type(struct fusb301_info *info, u8 type)
{
	if (type & TYPE_AUDIO_ACC)
		info->fusb_type = FUSB301_TYPE_AUDIO;
	else if (type & TYPE_DBG_ACC)
		info->fusb_type = FUSB301_TYPE_DEBUG;
	else if (type & TYPE_PWR_ACC)
		info->fusb_type = FUSB301_TYPE_POWER_ACC;
	else if (type & TYPE_SRC)
		info->fusb_type = FUSB301_TYPE_SOURCE;
	else if (type & TYPE_SNK)
		info->fusb_type = FUSB301_TYPE_SINK;
	else
		pr_err("No device type!\n");
}

static void fusb301_check_orient(struct fusb301_info *info, u8 status)
{
	u8 orient = ((status & STAT_ORIENT)>>STAT_ORIENT_SHIFT);

	info->fusb_orient = orient;
}

static irqreturn_t fusb301_irq_thread(int irq, void *handle)
{
	u8 intr, rdata;
	int bc_lvl;
	struct fusb301_info *info = (struct fusb301_info *)handle;

	fusb301_read_reg(info->i2c, REG_INT, &intr);
	pr_debug("\n type<%d> int(0x%02x)\n", info->fusb_type, intr);

	if (intr & INT_ATTACH) {
		pr_debug("Attach interrupt!\n");
		fusb301_read_reg(info->i2c, REG_TYPE, &rdata);
		fusb301_check_type(info, rdata);
		pr_info("TYPE is %d\n", info->fusb_type);
		fusb301_read_reg(info->i2c, REG_STAT, &rdata);

		fusb301_check_orient(info, rdata);
		pr_info("Orient is %d\n", info->fusb_orient);

		if (info->fusb_type == FUSB301_TYPE_SOURCE) {
			/*SOURCE*/
			fusb301_read_reg(info->i2c, REG_STAT, &rdata);
			bc_lvl = (rdata & STAT_BC_LVL) >> STAT_BC_LVL_SHIFT;
		} else if (info->fusb_type == FUSB301_TYPE_SINK) {
			/*SINK
			 *Please deal with sink
			 */
		} else
			pr_err("not sink and source\n");
	} else if (intr & INT_DETACH) {
		/* Detach */
		pr_debug("Detach interrupt!\n");
		info->fusb_type = FUSB301_TYPE_NONE;
	} else if (intr & INT_BC_LVL)
		pr_info("BC_LVL interrupt!\n");
	else if (intr & INT_ACC_CHG)
		pr_info("Accessory change interrupt!\n");
	else
		pr_info("weird interrupt!\n");

	return IRQ_HANDLED;
}

static int fusb301_initialization(struct fusb301_info *info)
{
	u8 id = 0;

	info->fusb_type = FUSB301_TYPE_NONE;

	fusb301_read_reg(info->i2c, 0x01, &id);
	if (0 == id) {
		pr_err("Can't get fusb301a ID\n");
		return -ENODEV;
	}

	fusb301_write_reg(info->i2c, REG_MOD, MOD_DRP);
	/*unmask global interrupts*/
	fusb301_update_reg(info->i2c, REG_CON, 0, CON_INT_MSK);

	return 0;
}

static int parse_device_info(struct device *dev, struct fusb301_info *info)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	info->irq = of_get_named_gpio(np, "qcom,fusb-irq", 0);
	if ((!gpio_is_valid(info->irq))) {
		pr_err("Error reading info->irq =%d\n", info->irq);
		ret = -EINVAL;
		goto err_get;
	} else
		pr_debug("info->irq=%d\n", info->irq);

	ret = gpio_request(info->irq, "fusb-irq-pin");
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

static int fusb301_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int irq_pin;
	struct fusb301_info *info;

	info = kzalloc(sizeof(struct fusb301_info), GFP_KERNEL);
	info->i2c = client;

	pr_info("Enter into probe\n");
	ret = parse_device_info(&client->dev, info);
	if (ret < 0) {
		pr_err("invalid eint number - %d\n", info->irq);
		return -EINVAL;
	}

	i2c_set_clientdata(client, info);

	mutex_init(&info->mutex);

	info->fusb_class = class_create(THIS_MODULE, "type-c");
	info->dev_t = device_create(info->fusb_class, NULL, 0, NULL, "fusb301");
	device_create_file(info->dev_t, &dev_attr_type);
	device_create_file(info->dev_t, &dev_attr_mode);
	dev_set_drvdata(info->dev_t, info);

	ret = fusb301_initialization(info);
	if (ret < 0)
		goto init_ic_failed;

	irq_pin = gpio_to_irq(info->irq);
	ret = request_threaded_irq(irq_pin, NULL, fusb301_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "fusb301_irq", info);
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
	device_remove_file(info->dev_t, &dev_attr_type);
	device_destroy(info->fusb_class, 0);
	class_destroy(info->fusb_class);
	mutex_destroy(&info->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(info);

	return ret;
}

static int fusb301_remove(struct i2c_client *client)
{
	struct fusb301_info *info = i2c_get_clientdata(client);

	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, info);
	}
	device_remove_file(info->dev_t, &dev_attr_type);
	device_destroy(info->fusb_class, 0);
	class_destroy(info->fusb_class);
	mutex_destroy(&info->mutex);
	i2c_set_clientdata(client, NULL);

	kfree(info);

	return 0;
}

static int  fusb301_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int  fusb301_resume(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id fusb301_id[] = {
		{.compatible = "fusb301"},
		{},
};
MODULE_DEVICE_TABLE(of, fusb301_id);

static const struct i2c_device_id fusb301_i2c_id[] = {
	{ "fusb301", 0 },
	{ }
};

static struct i2c_driver fusb301_i2c_driver = {
	.driver = {
		.name = "fusb301",
		.of_match_table = of_match_ptr(fusb301_id),
	},
	.probe    = fusb301_probe,
	.remove   = fusb301_remove,
	.suspend  = fusb301_suspend,
	.resume	  = fusb301_resume,
	.id_table = fusb301_i2c_id,
};

static __init int fusb301_i2c_init(void)
{
	return i2c_add_driver(&fusb301_i2c_driver);
}

static __exit void fusb301_i2c_exit(void)
{
	i2c_del_driver(&fusb301_i2c_driver);
}

module_init(fusb301_i2c_init);
module_exit(fusb301_i2c_exit);

MODULE_AUTHOR("pangkun@longcheer.net");
MODULE_DESCRIPTION("I2C bus driver for FUSB301 USB Type-C");
MODULE_LICENSE("GPL v2");
