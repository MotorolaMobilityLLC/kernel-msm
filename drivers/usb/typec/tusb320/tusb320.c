/*
 * tusb320.c - Driver for TUSB320 USB Type-C connector chip
 *
 * Copyright (C) 2015 HUAWEI, Inc.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Author: HUAWEI, Inc.
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <asm/irq.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/usb/typec.h>
#include "tusb320.h"

static enum typec_current_mode tusb320_current_mode_detect(void);
static enum typec_current_mode tusb320_current_advertise_get(void);
static int tusb320_current_advertise_set(enum typec_current_mode current_mode);
static enum typec_attached_state tusb320_attatched_state_detect(void);
static enum typec_port_mode tusb320_port_mode_get(void);
static int tusb320_port_mode_set(enum typec_port_mode port_mode);
static ssize_t tusb320_dump_regs(char *buf);

static struct tusb320_device_info *g_tusb320_dev;

struct typec_device_ops tusb320_ops = {
	.current_detect = tusb320_current_mode_detect,
	.attached_state_detect = tusb320_attatched_state_detect,
	.current_advertise_get = tusb320_current_advertise_get,
	.current_advertise_set = tusb320_current_advertise_set,
	.port_mode_get = tusb320_port_mode_get,
	.port_mode_set = tusb320_port_mode_set,
	.dump_regs = tusb320_dump_regs
};

static int tusb320_i2c_read(struct tusb320_device_info *di,
			    char *rxData, int length)
{
	int ret = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = di->client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	ret = i2c_transfer(di->client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		pr_err("%s: transfer error %d\n", __func__, ret);

	return ret;
}

static int tusb320_i2c_write(struct tusb320_device_info *di,
			     char *txData, int length)
{
	int ret = 0;

	struct i2c_msg msg[] = {
		{
		 .addr = di->client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	ret = i2c_transfer(di->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		pr_err("%s: transfer error %d\n", __func__, ret);

	return ret;
}

static int tusb320_read_reg(u8 reg, u8 * val)
{
	int ret;
	u8 buf[1];
	struct tusb320_device_info *di = g_tusb320_dev;

	buf[0] = reg;
	ret = tusb320_i2c_write(di, buf, sizeof(buf));
	if (ret < 0) {
		pr_err("%s: tusb320_i2c_write error %d\n", __func__, ret);
		return ret;
	}
	ret = tusb320_i2c_read(di, val, 1);
	if (ret < 0) {
		pr_err("%s: tusb320_i2c_read error %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int tusb320_write_reg(u8 reg, u8 val)
{
	u8 buf[2];
	struct tusb320_device_info *di = g_tusb320_dev;

	buf[0] = reg;
	buf[1] = val;
	return tusb320_i2c_write(di, buf, sizeof(buf));
}

static int tusb320_int_clear(void)
{
	u8 reg_val;
	int ret;

	ret = tusb320_read_reg(TUSB320_REG_ATTACH_STATUS, &reg_val);
	if (ret < 0) {
		pr_err("%s: read error\n", __func__);
		return ret;
	}

	reg_val |= TUSB320_REG_STATUS_INT;

	ret = tusb320_write_reg(TUSB320_REG_ATTACH_STATUS, reg_val);
	if (ret < 0) {
		pr_err("%s: write error\n", __func__);
		return ret;
	}

	return ret;
}

static enum typec_current_mode tusb320_current_mode_detect(void)
{
	u8 reg_val, mask_val;
	int ret;
	enum typec_current_mode current_mode = TYPEC_CURRENT_MODE_DEFAULT;

	ret = tusb320_read_reg(TUSB320_REG_CURRENT_MODE, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_CURRENT_MODE error\n", __func__);
		return current_mode;
	}

	pr_info("%s: REG_CURRENT_MODE 08H is 0x%x\n", __func__, reg_val);

	mask_val = reg_val & TUSB320_REG_CUR_MODE_DETECT_MASK;
	switch (mask_val) {
	case TUSB320_REG_CUR_MODE_DETECT_DEFAULT:
		current_mode = TYPEC_CURRENT_MODE_DEFAULT;
		break;
	case TUSB320_REG_CUR_MODE_DETECT_MID:
		current_mode = TYPEC_CURRENT_MODE_MID;
		break;
	case TUSB320_REG_CUR_MODE_DETECT_HIGH:
		current_mode = TYPEC_CURRENT_MODE_HIGH;
		break;
	default:
		current_mode = TYPEC_CURRENT_MODE_UNSPPORTED;
	}

	pr_info("%s: current mode is %d\n", __func__, current_mode);

	return current_mode;
}

static enum typec_current_mode tusb320_current_advertise_get(void)
{
	u8 reg_val, mask_val;
	int ret;
	enum typec_current_mode current_mode = TYPEC_CURRENT_MODE_DEFAULT;

	ret = tusb320_read_reg(TUSB320_REG_CURRENT_MODE, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_CURRENT_MODE error\n", __func__);
		return current_mode;
	}

	pr_info("%s: REG_CURRENT_MODE 08H is 0x%x\n", __func__, reg_val);

	mask_val = reg_val & TUSB320_REG_CUR_MODE_ADVERTISE_MASK;
	switch (mask_val) {
	case TUSB320_REG_CUR_MODE_ADVERTISE_DEFAULT:
		current_mode = TYPEC_CURRENT_MODE_DEFAULT;
		break;
	case TUSB320_REG_CUR_MODE_ADVERTISE_MID:
		current_mode = TYPEC_CURRENT_MODE_MID;
		break;
	case TUSB320_REG_CUR_MODE_ADVERTISE_HIGH:
		current_mode = TYPEC_CURRENT_MODE_HIGH;
		break;
	default:
		current_mode = TYPEC_CURRENT_MODE_UNSPPORTED;
	}

	pr_info("%s: current advertise is %d\n", __func__, current_mode);

	return current_mode;
}

static int tusb320_current_advertise_set(enum typec_current_mode current_mode)
{
	u8 reg_val, mask_val;
	int ret;

	ret = tusb320_read_reg(TUSB320_REG_CURRENT_MODE, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_CURRENT_MODE error\n", __func__);
		return -1;
	}

	pr_info("%s: REG_CURRENT_MODE 08H is 0x%x\n", __func__, reg_val);

	switch (current_mode) {
	case TYPEC_CURRENT_MODE_MID:
		mask_val = TUSB320_REG_CUR_MODE_ADVERTISE_MID;
		break;
	case TYPEC_CURRENT_MODE_HIGH:
		mask_val = TUSB320_REG_CUR_MODE_ADVERTISE_HIGH;
		break;
	default:
		mask_val = TUSB320_REG_CUR_MODE_ADVERTISE_DEFAULT;
	}

	if (mask_val == (reg_val & TUSB320_REG_CUR_MODE_ADVERTISE_MASK)) {
		pr_info("%s: current advertise is %d already\n", __func__,
			current_mode);
		return 0;
	}

	reg_val &= ~TUSB320_REG_CUR_MODE_ADVERTISE_MASK;
	reg_val |= mask_val;
	ret = tusb320_write_reg(TUSB320_REG_CURRENT_MODE, reg_val);
	if (ret < 0) {
		pr_err("%s: write REG_CURRENT_MODE error\n", __func__);
		return -1;
	}

	pr_info("%s: current advertise set to %d\n", __func__, current_mode);
	return 0;
}

static enum typec_attached_state tusb320_attatched_state_detect(void)
{
	u8 reg_val, mask_val;
	int ret;
	enum typec_attached_state attached_state = TYPEC_NOT_ATTACHED;

	ret = tusb320_read_reg(TUSB320_REG_ATTACH_STATUS, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_ATTACH_STATUS error\n", __func__);
		return attached_state;
	}

	pr_info("%s: REG_ATTACH_STATUS 09H is 0x%x\n", __func__, reg_val);

	mask_val = reg_val & TUSB320_REG_STATUS_MODE;
	switch (mask_val) {
	case TUSB320_REG_STATUS_AS_DFP:
		attached_state = TYPEC_ATTACHED_AS_DFP;
		break;
	case TUSB320_REG_STATUS_AS_UFP:
		attached_state = TYPEC_ATTACHED_AS_UFP;
		break;
	case TYPEC_ATTACHED_TO_ACCESSORY:
		attached_state = TYPEC_ATTACHED_TO_ACCESSORY;
		break;
	default:
		attached_state = TYPEC_NOT_ATTACHED;
	}

	pr_info("%s: attached state is %d\n", __func__, attached_state);

	return attached_state;
}

static enum typec_port_mode tusb320_port_mode_get(void)
{
	u8 reg_val, mask_val;
	int ret;
	enum typec_port_mode port_mode = TYPEC_MODE_ACCORDING_TO_PROT;

	ret = tusb320_read_reg(TUSB320_REG_MODE_SET, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_MODE_SET error\n", __func__);
		return port_mode;
	}

	pr_info("%s: REG_MODE_SET is 0x%x\n", __func__, reg_val);

	mask_val = reg_val & TUSB320_REG_SET_MODE;
	switch (mask_val) {
	case TUSB320_REG_SET_UFP:
		port_mode = TYPEC_UFP_MODE;
		break;
	case TUSB320_REG_SET_DFP:
		port_mode = TYPEC_DFP_MODE;
		break;
	case TUSB320_REG_SET_DRP:
		port_mode = TYPEC_DRP_MODE;
		break;
	default:
		port_mode = TYPEC_MODE_ACCORDING_TO_PROT;
		break;
	}

	pr_info("%s: port mode is %d\n", __func__, port_mode);

	return port_mode;
}

static int tusb320_port_mode_set(enum typec_port_mode port_mode)
{
	u8 reg_val, mask_val;
	int ret;

	ret = tusb320_read_reg(TUSB320_REG_MODE_SET, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_MODE_SET error\n", __func__);
		return -1;
	}

	pr_info("%s: REG_MODE_SET 0aH is 0x%x\n", __func__, reg_val);

	switch (port_mode) {
	case TYPEC_UFP_MODE:
		mask_val = TUSB320_REG_SET_UFP;
		break;
	case TYPEC_DFP_MODE:
		mask_val = TUSB320_REG_SET_DFP;
		break;
	case TYPEC_DRP_MODE:
		mask_val = TUSB320_REG_SET_DRP;
		break;
	default:
		mask_val = TUSB320_REG_SET_BY_PORT;
		break;
	}

	if (mask_val == (reg_val & TUSB320_REG_SET_MODE)) {
		pr_info("%s: port mode is %d already\n", __func__, port_mode);
		return 0;
	}

	reg_val &= ~TUSB320_REG_SET_MODE;
	reg_val |= mask_val;
	ret = tusb320_write_reg(TUSB320_REG_MODE_SET, reg_val);
	if (ret < 0) {
		pr_err("%s: write REG_MODE_SET error\n", __func__);
		return -1;
	}

	pr_info("%s: port mode set to %d\n", __func__, port_mode);
	return 0;
}

static ssize_t tusb320_dump_regs(char *buf)
{
	int i;
	char reg[REGISTER_NUM] = { 0 };

	for (i = 8; i < 11; i++) {
		tusb320_read_reg(i, &reg[i]);
	}

	return scnprintf(buf, PAGE_SIZE,
			 "0x%02X,0x%02X,0x%02X\n", reg[8], reg[9], reg[10]);
}

static void tusb320_intb_work(struct work_struct *work)
{
	int ret;
	enum typec_current_mode current_mode;
	enum typec_attached_state attached_state;

	current_mode = tusb320_current_mode_detect();
	attached_state = tusb320_attatched_state_detect();

	ret = tusb320_int_clear();
	if (ret < 0) {
		pr_err("%s: clean interrupt mask error\n", __func__);
	}

	pr_info("%s: ------end\n", __func__);
}

static irqreturn_t tusb320_irq_handler(int irq, void *dev_id)
{
	int gpio_value_intb = 0;
	struct tusb320_device_info *di = dev_id;

	pr_info("%s: ------entry\n", __func__);
	gpio_value_intb = gpio_get_value(di->gpio_intb);
	if (1 == gpio_value_intb) {
		pr_err("%s: intb high when interrupt occured!\n", __func__);
	}

	schedule_work(&di->g_intb_work);

	return IRQ_HANDLED;
}

static int tusb320_device_check(struct tusb320_device_info *di)
{
	u8 reg_val;
	int ret;

	ret = tusb320_read_reg(TUSB320_REG_DEVICE_ID, &reg_val);
	if (ret < 0) {
		pr_err("%s: read REG_DEVICE_ID error\n", __func__);
		return ret;
	}

	return 0;
}

static int tusb320_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret = 0;
	int gpio_value_intb;
	struct tusb320_device_info *di = NULL;
	struct device_node *node;

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di) {
		pr_err("tusb320_device_info is NULL!\n");
		return -ENOMEM;
	}

	pr_info("%s: ------entry\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c_check_functionality error!\n", __func__);
		ret = -ENODEV;
		goto err_i2c_check_functionality_0;
	}

	g_tusb320_dev = di;
	di->dev = &client->dev;
	di->client = client;
	node = di->dev->of_node;
	i2c_set_clientdata(client, di);

	ret = tusb320_device_check(di);
	if (ret < 0) {
		pr_err("%s: check device error, the chip is not TUSB320!\n",
		       __func__);
		goto err_i2c_check_functionality_0;
	}

	di->gpio_enb = of_get_named_gpio(node, "tusb320_typec,gpio_enb", 0);
	if (!gpio_is_valid(di->gpio_enb)) {
		pr_err("%s: of get gpio_enb error\n", __func__);
		ret = -EINVAL;
		goto err_i2c_check_functionality_0;
	}

	di->gpio_intb = of_get_named_gpio(node, "tusb320_typec,gpio_intb", 0);
	if (!gpio_is_valid(di->gpio_intb)) {
		pr_err("%s: of get gpio_intb error\n", __func__);
		ret = -EINVAL;
		goto err_i2c_check_functionality_0;
	}

	ret = gpio_request(di->gpio_enb, "tusb320_en");
	if (ret < 0) {
		pr_err("%s: gpio_request error, ret=%d, gpio=%d\n", __func__,
		       ret, di->gpio_enb);
		ret = -ENOMEM;
		goto err_gpio_enb_request_1;
	}

	ret = gpio_direction_output(di->gpio_enb, 0);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input error, ret=%d, gpio=%d\n",
		       __func__, ret, di->gpio_enb);
		goto err_gpio_enb_request_1;
	}

	ret = gpio_request(di->gpio_intb, "tusb320_int");
	if (ret < 0) {
		pr_err("%s: gpio_request error, ret=%d, gpio_intb=%d\n",
		       __func__, ret, di->gpio_intb);
		ret = -ENOMEM;
		goto err_gpio_intb_request_2;
	}

	di->irq_intb = gpio_to_irq(di->gpio_intb);
	if (di->irq_intb < 0) {
		pr_err("%s: gpio_to_irq error, gpio_intb=%d\n", __func__,
		       di->gpio_intb);
		goto err_gpio_intb_request_2;
	}

	ret = gpio_direction_input(di->gpio_intb);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input error, ret=%d, gpio_intb=%d\n",
		       __func__, ret, di->gpio_intb);
		goto err_gpio_intb_request_2;
	}

	INIT_WORK(&di->g_intb_work, tusb320_intb_work);

	ret = request_irq(di->irq_intb,
			  tusb320_irq_handler,
			  IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING,
			  "tusb320_int", di);
	if (ret) {
		pr_err("%s: request_irq error, ret=%d\n", __func__, ret);
		di->irq_intb = -1;
		goto err_irq_request_3;
	}

	ret = tusb320_int_clear();
	if (ret < 0) {
		pr_err("%s: clean interrupt mask error %d\n", __func__, ret);
	}

	gpio_value_intb = gpio_get_value(di->gpio_intb);
	if (0 == gpio_value_intb) {
		pr_info("%s: gpio_intb is low\n", __func__);
		schedule_work(&di->g_intb_work);
	}

	tusb320_attatched_state_detect();
	tusb320_current_mode_detect();

	ret = add_typec_device(di->dev, &tusb320_ops);
	if (ret < 0) {
		pr_err("%s: add_typec_device fail\n", __func__);
	}

	pr_info("%s: ------end\n", __func__);
	return ret;

err_irq_request_3:
	free_irq(di->gpio_intb, di);
err_gpio_intb_request_2:
	gpio_free(di->gpio_intb);
err_gpio_enb_request_1:
	gpio_free(di->gpio_enb);
err_i2c_check_functionality_0:
	devm_kfree(&client->dev, di);
	g_tusb320_dev = NULL;

	pr_err("%s: ------FAIL!!! end. ret = %d\n", __func__, ret);
	return ret;
}

static int tusb320_remove(struct i2c_client *client)
{
	struct tusb320_device_info *di = i2c_get_clientdata(client);

	free_irq(di->irq_intb, di);
	gpio_set_value(di->gpio_enb, 1);
	gpio_free(di->gpio_enb);
	gpio_free(di->gpio_intb);

	return 0;
}

static const struct of_device_id typec_tusb320_ids[] = {
	{.compatible = "huawei,tusb320"},
	{},
};

MODULE_DEVICE_TABLE(of, typec_tusb320_ids);

static const struct i2c_device_id tusb320_i2c_id[] = {
	{"tusb320", 0},
	{}
};

static struct i2c_driver tusb320_i2c_driver = {
	.driver = {
		   .name = "tusb320",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(typec_tusb320_ids),
		   },
	.probe = tusb320_probe,
	.remove = tusb320_remove,
	.id_table = tusb320_i2c_id,
};

static __init int tusb320_i2c_init(void)
{
	int ret = 0;
	pr_info("%s: ------entry\n", __func__);

	ret = i2c_add_driver(&tusb320_i2c_driver);
	if (ret) {
		pr_err("%s: i2c_add_driver error!!!\n", __func__);
	}

	pr_info("%s: ------end\n", __func__);
	return ret;
}

static __exit void tusb320_i2c_exit(void)
{
	i2c_del_driver(&tusb320_i2c_driver);
}

module_init(tusb320_i2c_init);
module_exit(tusb320_i2c_exit);

MODULE_AUTHOR("HUAWEI");
MODULE_DESCRIPTION("I2C bus driver for tusb320 Type-C connector");
MODULE_LICENSE("GPL v2");
