/*
 * siw_touch_bus_i2c.c - SiW touch bus i2c driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include <linux/platform_device.h>
#include <linux/i2c.h>

#include "siw_touch.h"
#include "siw_touch_bus.h"
#include "siw_touch_irq.h"

#if defined(CONFIG_I2C)

#define siwmon_submit_bus_i2c_read(_client, _data, _ret)	\
		siwmon_submit_bus(&_client->dev, "I2C_R", _data, _ret)

#define siwmon_submit_bus_i2c_write(_client, _data, _ret)	\
		siwmon_submit_bus(&_client->dev, "I2C_W", _data, _ret)


static void siw_touch_i2c_err_dump(struct i2c_client *client,
					struct i2c_msg *msgs, int num)
{
	struct i2c_msg *msg = msgs;
	int i;

	t_dev_err(&client->dev, "i2c transfer err : ");
	for (i = 0; i < num; i++) {
		t_dev_err(&client->dev,
			" - msgs[%d] : client 0x%04X, flags 0x%04X, len %d\n",
			i, msg->addr, msg->flags, msg->len);
		siw_touch_bus_err_dump_data(&client->dev,
			msg->buf, msg->len, i, "msgs");
		msg++;
	}
}

static int siw_touch_i2c_init(struct device *dev)
{
	return 0;
}

static int siw_touch_do_i2c_read(struct i2c_client *client,
					struct touch_bus_msg *msg)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = msg->tx_size,
			.buf = msg->tx_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = msg->rx_size,
			.buf = msg->rx_buf,
		},
	};
	int ret = 0;

	if ((msg->rx_size > SIW_TOUCH_MAX_BUF_SIZE) ||
		(msg->tx_size > SIW_TOUCH_MAX_BUF_SIZE)) {
		t_dev_err(&client->dev,
			"i2c rd: buffer overflow - rx %Xh, tx %Xh\n",
			msg->rx_size, msg->tx_size);
		return -EOVERFLOW;
	}

	/*
	 * Bus control can need to be modifyed up to main chipset spec.
	 */

	ret = i2c_transfer(client->adapter, msgs, 2);
	siwmon_submit_bus_i2c_read(client, msg, ret);
	if (ret < 0)
		siw_touch_i2c_err_dump(client, msgs, 2);

	return ret;
}

static int siw_touch_i2c_read(struct device *dev, void *msg)
{
	return siw_touch_do_i2c_read(to_i2c_client(dev),
		(struct touch_bus_msg *)msg);
}

int siw_touch_do_i2c_write(struct i2c_client *client,
						struct touch_bus_msg *msg)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = msg->tx_size,
			.buf = msg->tx_buf,
		},
	};
	int ret = 0;

	if (msg->tx_size > SIW_TOUCH_MAX_BUF_SIZE) {
		t_dev_err(&client->dev, "i2c wr: buffer overflow - tx %Xh\n",
			msg->tx_size);
		return -EOVERFLOW;
	}

	/*
	 * Bus control can need to be modifyed up to main chipset spec.
	 */

	ret = i2c_transfer(client->adapter, msgs, 1);
	siwmon_submit_bus_i2c_write(client, msg, ret);
	if (ret < 0)
		siw_touch_i2c_err_dump(client, msgs, 1);

	return ret;
}

static int siw_touch_i2c_write(struct device *dev, void *msg)
{
	return siw_touch_do_i2c_write(to_i2c_client(dev),
		(struct touch_bus_msg *)msg);
}

static int siw_touch_i2c_do_xfer(struct i2c_client *client,
	struct touch_xfer_msg *xfer)
{
	t_dev_info(&client->dev, "I2C xfer : not supported\n");
	return -EINVAL;
}

static int siw_touch_i2c_xfer(struct device *dev, void *xfer)
{
	return siw_touch_i2c_do_xfer(to_i2c_client(dev),
		(struct touch_xfer_msg *)xfer);
}

static struct siw_ts *siw_touch_i2c_alloc(
			struct i2c_client *i2c,
			struct siw_touch_bus_drv *bus_drv)
{
	struct device *dev = &i2c->dev;
	struct siw_ts *ts = NULL;
	struct siw_touch_pdata *pdata = NULL;
	int ret;

	ts = touch_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		t_dev_err(dev,
			"i2c alloc: fail to allocate memory for touch data\n");
		goto out;
	}

	ts->bus_dev = i2c;
	ts->addr = (size_t)i2c->addr;
	ts->dev = dev;
	ts->irq = i2c->irq;
	pdata = bus_drv->pdata;
	if (!pdata) {
		t_dev_err(dev, "i2c alloc: NULL pdata\n");
		goto out_pdata;
	}

	ret = siw_setup_params(ts, pdata);
	if (ret < 0)
		goto out_name;

	ts->pdata = pdata;

	siw_setup_operations(ts, bus_drv->pdata->ops);

	ts->bus_init = siw_touch_i2c_init;
	ts->bus_read = siw_touch_i2c_read;
	ts->bus_write = siw_touch_i2c_write;
	ts->bus_xfer = siw_touch_i2c_xfer;

	ret = siw_touch_bus_tr_data_init(ts);
	if (ret < 0)
		goto out_tr;

	i2c_set_clientdata(i2c, ts);

	return ts;

out_tr:

out_name:

out_pdata:
	touch_kfree(dev, ts);

out:
	return NULL;
}

static void siw_touch_i2c_free(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct siw_ts *ts = to_touch_core(dev);

	i2c_set_clientdata(i2c, NULL);

	siw_touch_bus_tr_data_free(ts);

	touch_kfree(dev, ts);
}

static int siw_touch_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	struct siw_touch_bus_drv *bus_drv = NULL;
	struct siw_ts *ts = NULL;
	struct device *dev = &i2c->dev;
	int ret = 0;

	t_dev_info_bus_parent(dev);

	bus_drv = container_of(to_i2c_driver(dev->driver),
					struct siw_touch_bus_drv, bus.i2c_drv);
	if (bus_drv == NULL) {
		t_dev_err(dev, "NULL bus_drv info\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		t_dev_err(dev, "i2c func not Supported\n");
		return -EIO;
	}

	ts = siw_touch_i2c_alloc(i2c, bus_drv);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = siw_touch_probe(ts);
	if (ret)
		goto out_plat;

	return 0;

out_plat:

out:
	return ret;
}

static int siw_touch_i2c_remove(struct i2c_client *i2c)
{
	struct siw_ts *ts = to_touch_core(&i2c->dev);

	siw_touch_remove(ts);

	siw_touch_i2c_free(i2c);

	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int siw_touch_i2c_pm_suspend(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_suspend(dev, 0);

	return ret;
}

static int siw_touch_i2c_pm_resume(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_resume(dev, 0);

	return ret;
}

#if defined(__SIW_CONFIG_FASTBOOT)
static int siw_touch_i2c_pm_freeze(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_suspend(dev, 1);

	return ret;
}

static int siw_touch_i2c_pm_thaw(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_resume(dev, 1);

	return ret;
}
#endif

static const struct dev_pm_ops siw_touch_i2c_pm_ops = {
	.suspend		= siw_touch_i2c_pm_suspend,
	.resume			= siw_touch_i2c_pm_resume,
#if defined(__SIW_CONFIG_FASTBOOT)
	.freezer		= siw_touch_i2c_pm_freeze,
	.thaw			= siw_touch_i2c_pm_thaw,
	.poweroff		= siw_touch_i2c_pm_freeze,
	.restore		= siw_touch_i2c_pm_thaw,
#endif
};
#define DEV_PM_OPS	(&siw_touch_i2c_pm_ops)
#else	/* CONFIG_PM_SLEEP */
#define DEV_PM_OPS	NULL
#endif	/* CONFIG_PM_SLEEP */

static struct i2c_device_id siw_touch_i2c_id[] = {
	{ SIW_TOUCH_NAME, 0 },
	{ }
};

int siw_touch_i2c_add_driver(void *data)
{
	struct siw_touch_chip_data *chip_data = data;
	struct siw_touch_bus_drv *bus_drv = NULL;
	struct siw_touch_pdata *pdata = NULL;
	struct i2c_driver *i2c_drv = NULL;
	char *drv_name = NULL;
	int bus_type;
	int ret = 0;

	if (chip_data == NULL) {
		t_pr_err("NULL chip data\n");
		return -EINVAL;
	}

	if (chip_data->pdata == NULL) {
		t_pr_err("NULL chip pdata\n");
		return -EINVAL;
	}

	bus_type = pdata_bus_type((struct siw_touch_pdata *)chip_data->pdata);

	bus_drv = siw_touch_bus_create_bus_drv(bus_type);
	if (!bus_drv) {
		ret = -ENOMEM;
		goto out_drv;
	}

	pdata = siw_touch_bus_create_bus_pdata(bus_type);
	if (!pdata) {
		ret = -ENOMEM;
		goto out_pdata;
	}

	memcpy(pdata, chip_data->pdata, sizeof(*pdata));

	drv_name = pdata_drv_name(pdata);

	bus_drv->pdata = pdata;

	i2c_drv = &bus_drv->bus.i2c_drv;
	i2c_drv->driver.name = drv_name;
	i2c_drv->driver.owner = pdata->owner;
#if defined(__SIW_CONFIG_OF)
	i2c_drv->driver.of_match_table = pdata->of_match_table;
#endif
	i2c_drv->driver.pm = DEV_PM_OPS;

	i2c_drv->probe = siw_touch_i2c_probe;
	i2c_drv->remove = siw_touch_i2c_remove;
	i2c_drv->id_table = siw_touch_i2c_id;
	if (drv_name) {
		memset((void *)siw_touch_i2c_id[0].name, 0, I2C_NAME_SIZE);
		snprintf((char *)siw_touch_i2c_id[0].name,
				I2C_NAME_SIZE,
				"%s",
				drv_name);
	}

	ret = i2c_add_driver(i2c_drv);
	if (ret) {
		t_pr_err("i2c_register_driver[%s] failed, %d\n",
				drv_name, ret);
		goto out_client;
	}

	chip_data->bus_drv = bus_drv;

	return 0;

out_client:
	siw_touch_bus_free_bus_pdata(pdata);

out_pdata:
	siw_touch_bus_free_bus_drv(bus_drv);

out_drv:

	return ret;
}

int siw_touch_i2c_del_driver(void *data)
{
	struct siw_touch_chip_data *chip_data = data;
	struct siw_touch_bus_drv *bus_drv = NULL;

	if (chip_data == NULL) {
		t_pr_err("NULL touch chip data\n");
		return -ENODEV;
	}

	bus_drv = chip_data->bus_drv;
	if (bus_drv) {
		i2c_del_driver(&bus_drv->bus.i2c_drv);

		if (bus_drv->pdata)
			siw_touch_bus_free_bus_pdata(bus_drv->pdata);

		siw_touch_bus_free_bus_drv(bus_drv);
		chip_data->bus_drv = NULL;
	}

	return 0;
}
#else	/* CONFIG_I2C */
int siw_touch_i2c_add_driver(void *data)
{
	t_pr_err("I2C : not supported in this system\n");
	return -ENODEV;
}

int siw_touch_i2c_del_driver(void *data)
{
	t_pr_err("I2C : not supported in this system\n");
	return -ENODEV;
}

#endif	/* CONFIG_I2C */




