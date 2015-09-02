/*
 * Copyright (C) 2015 Motorola Mobility LLC
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
 */
#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

#include <linux/hssp_programmer.h>
#include <linux/power_supply.h>
#include <linux/usb.h>
#include <linux/firmware.h>


static unsigned char motosh_antcap_i2c_rbuf[80];

/**********************************************************/
/* boot-safe and ioctl-protected I2C read/write functions */
/**********************************************************/

static int motosh_antcap_i2c_write(unsigned char *buf,
				   unsigned char len,
				   unsigned char getmutex)
{
	int                err = 0;
	struct motosh_data *ps_motosh = motosh_misc_data;

	if (getmutex)
		if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
			return -EINTR;

	if (motosh_g_booted && motosh_g_antcap_hw_ready
			&& (ps_motosh->mode > BOOTMODE)) {
		motosh_wake(ps_motosh);
		err = motosh_i2c_write(ps_motosh, buf, len);
		motosh_sleep(ps_motosh);
	} else
		err = 0x8000 |
			(motosh_g_booted << 8) | motosh_g_antcap_hw_ready;

	if (getmutex)
		mutex_unlock(&ps_motosh->lock);

	return err;
}

static int motosh_antcap_i2c_write_read(unsigned char *buf,
					unsigned char wlen,
					unsigned char rlen,
					unsigned char getmutex)
{
	int                err = 0;
	struct motosh_data *ps_motosh = motosh_misc_data;

	if (getmutex)
		if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
			return -EINTR;

	if (motosh_g_booted && motosh_g_antcap_hw_ready
			&& (ps_motosh->mode > BOOTMODE)) {
		motosh_wake(ps_motosh);
		err = motosh_i2c_write_read(ps_motosh, buf,
						motosh_antcap_i2c_rbuf,
						wlen, rlen);
		motosh_sleep(ps_motosh);
	} else
		err = 0x8000
			| (motosh_g_booted << 8)
			| motosh_g_antcap_hw_ready;

	if (getmutex)
		mutex_unlock(&ps_motosh->lock);

	return err;
}

/******************************************************/
/* functions which set/get/flash/check Cypress config */
/******************************************************/

int motosh_antcap_i2c_send_enable(unsigned char getmutex)
{
	int           err = -1;
	unsigned char i2cwbuf[3];

	if (motosh_g_antcap_hw_ready && motosh_g_antcap_sw_ready) {
		i2cwbuf[0] = ANTCAP_CTRL;
		if (motosh_g_antcap_enabled == ANTCAP_ENABLED)
			i2cwbuf[1] = ANTCAP_CTRL0_ENABLE;
		else
			i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
		i2cwbuf[2] = motosh_g_conn_state;

		err = motosh_antcap_i2c_write(i2cwbuf, 3, getmutex);
	}

	return err;
}

int motosh_antcap_i2c_getver_poll(unsigned char getmutex)
{
	int           err;
	int           i, j;
	unsigned char i2cwbuf[3];

	/********************************/
	/* ensure SH driver is disabled */
	/********************************/
	i2cwbuf[0] = ANTCAP_CTRL;
	i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
	err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
	if (err)
		return 0x100;

	/************************************************/
	/* mark reply header so we know when it changes */
	/************************************************/
	i2cwbuf[0] = ANTCAP_DEBUG;
	i2cwbuf[1] = 0xfe;
	err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
	if (err)
		return 0x200;

	for (i = 0; i < 5; i++) {

		/********************/
		/* put getcal index */
		/********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
		i2cwbuf[2] = ANTCAP_CTRL1_GET_VERS;
		err = motosh_antcap_i2c_write(i2cwbuf, 3, getmutex);
		if (err)
			return 0x300 + (i<<4);

		/***********************/
		/* send getcal message */
		/***********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_GETCFG;
		err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
		if (err)
			return 0x400 + (i<<4);

		/***********************************/
		/* poll for getcal response (0x85) */
		/***********************************/
		for (j = 0; j < 3; j++) {
			msleep_interruptible(200);

			i2cwbuf[0] = ANTCAP_DEBUG;
			err = motosh_antcap_i2c_write_read(i2cwbuf, 1, 32,
								getmutex);

			dev_dbg(&motosh_misc_data->client->dev,
				"motosh_antcap_i2c_getver_poll: %d-%d:  %02x %02x %02x %02x %02x %02x %02x\n",
				i, j,
				motosh_antcap_i2c_rbuf[0],
				motosh_antcap_i2c_rbuf[1],
				motosh_antcap_i2c_rbuf[2],
				motosh_antcap_i2c_rbuf[3],
				motosh_antcap_i2c_rbuf[4],
				motosh_antcap_i2c_rbuf[6],
				motosh_antcap_i2c_rbuf[7]);

			if (err)
				return 0x500 + (i<<4) + j;
			else {
				if ((motosh_antcap_i2c_rbuf[0] == 0x85) &&
					(motosh_antcap_i2c_rbuf[3] == 0x00)) {

					err = 0;
					goto getver_done;
				} else
					err = 1;
			}
		}

	}
getver_done:
	return err;
}

int motosh_antcap_i2c_getcal_poll(unsigned char getmutex)
{
	int           err;
	int           i, j;
	unsigned char i2cwbuf[3];

	/********************************/
	/* ensure SH driver is disabled */
	/********************************/
	i2cwbuf[0] = ANTCAP_CTRL;
	i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
	err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
	if (err)
		return 0x100;

	/************************************************/
	/* mark reply header so we know when it changes */
	/************************************************/
	i2cwbuf[0] = ANTCAP_DEBUG;
	i2cwbuf[1] = 0xfe;
	err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
	if (err)
		return 0x200;

	for (i = 0; i < 5; i++) {

		/********************/
		/* put getcal index */
		/********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
		i2cwbuf[2] = ANTCAP_CTRL1_GET_CAL;
		err = motosh_antcap_i2c_write(i2cwbuf, 3, getmutex);
		if (err)
			return 0x300 + (i<<4);

		/***********************/
		/* send getcal message */
		/***********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_GETCFG;
		err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
		if (err)
			return 0x400 + (i<<4);

		/***********************************/
		/* poll for getcal response (0x85) */
		/***********************************/
		for (j = 0; j < 3; j++) {
			msleep_interruptible(200);

			i2cwbuf[0] = ANTCAP_DEBUG;
			err = motosh_antcap_i2c_write_read(i2cwbuf, 1, 32,
								getmutex);

			dev_dbg(&motosh_misc_data->client->dev,
				"motosh_antcap_i2c_getcal_poll: %d-%d:  %02x %02x %02x %02x\n",
				i, j,
				motosh_antcap_i2c_rbuf[0],
				motosh_antcap_i2c_rbuf[1],
				motosh_antcap_i2c_rbuf[2],
				motosh_antcap_i2c_rbuf[3]);

			if (err)
				return 0x500 + (i<<4) + j;
			else {
				if ((motosh_antcap_i2c_rbuf[0] == 0x85) &&
					(motosh_antcap_i2c_rbuf[3] == 0x00)) {
					err = 0;
					goto getcal_done;
				} else
					err = 1;
			}
		}

	}
getcal_done:
	return err;
}

int motosh_antcap_i2c_setcal_poll(unsigned char getmutex)
{
	int           err;
	int           i, j, l;
	unsigned char i2cwbuf[MOTOSH_ANTCAP_CAL_BUFF_SIZE+1];

	l = (motosh_g_antcap_cal[1] - 3);

	for (i = 0; i < 5; i++) {

		/*******************************************/
		/* copy calibration data into ANTCAP_DEBUG */
		/*******************************************/
		i2cwbuf[0] = ANTCAP_DEBUG;
		memcpy(&i2cwbuf[1], &motosh_g_antcap_cal[5], l);
		err = motosh_antcap_i2c_write(i2cwbuf, (l+1), getmutex);
		if (err)
			return 0x100;

		/********************/
		/* put setcal index */
		/********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_DISABLE;
		i2cwbuf[2] = ANTCAP_CTRL1_SET_CALFLASH;
		err = motosh_antcap_i2c_write(i2cwbuf, 3, getmutex);
		if (err)
			return 0x200 + (i<<4);

		/***********************/
		/* send setcal message */
		/***********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_SETCFG;
		err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
		if (err)
			return 0x300 + (i<<4);

		/***********************************/
		/* poll for setcal response (0x84) */
		/***********************************/
		for (j = 0; j < 10; j++) {
			msleep_interruptible(200);

			i2cwbuf[0] = ANTCAP_DEBUG;
			err = motosh_antcap_i2c_write_read(i2cwbuf, 1, 8,
								getmutex);

			dev_dbg(&motosh_misc_data->client->dev,
				"motosh_antcap_i2c_setcal_poll: %d-%d:  %02x %02x %02x %02x\n",
				i, j,
				motosh_antcap_i2c_rbuf[0],
				motosh_antcap_i2c_rbuf[1],
				motosh_antcap_i2c_rbuf[2],
				motosh_antcap_i2c_rbuf[3]);

			if (err)
				return 0x400 + (i<<4) + j;
			else {
				if ((motosh_antcap_i2c_rbuf[0] == 0x84) &&
					(motosh_antcap_i2c_rbuf[3] == 0x00)) {
					err = 0;
					goto setcal_done;
				} else
					err = 1;
			}
		}

	}
setcal_done:
	return err;
}

int motosh_antcap_i2c_flash_poll(unsigned char getmutex)
{
	int           err;
	int           i, j;
	unsigned char i2cwbuf[2];

	for (i = 0; i < 5; i++) {

		/**********************/
		/* send flash message */
		/**********************/
		i2cwbuf[0] = ANTCAP_CTRL;
		i2cwbuf[1] = ANTCAP_CTRL0_FLASH;
		err = motosh_antcap_i2c_write(i2cwbuf, 2, getmutex);
		if (err)
			return 0x300 + (i<<4);

		/**********************************/
		/* poll for flash response (0x89) */
		/**********************************/
		for (j = 0; j < 30; j++) {
			msleep_interruptible(200);

			i2cwbuf[0] = ANTCAP_DEBUG;
			err = motosh_antcap_i2c_write_read(i2cwbuf, 1, 8,
								getmutex);

			dev_dbg(&motosh_misc_data->client->dev,
				"motosh_antcap_i2c_flash_poll: %d-%d:  %02x %02x %02x %02x\n",
				i, j,
				motosh_antcap_i2c_rbuf[0],
				motosh_antcap_i2c_rbuf[1],
				motosh_antcap_i2c_rbuf[2],
				motosh_antcap_i2c_rbuf[3]);

			if (err)
				return 0x400 + (i<<4) + j;
			else {
				if ((motosh_antcap_i2c_rbuf[0] == 0x89) &&
					(motosh_antcap_i2c_rbuf[2] == 0x00)) {

					err = 0;
					goto flash_done;
				} else
					err = 1;
			}
		}

	}
flash_done:
	return err;
}

int motosh_antcap_check_cal(void)
{
	int           i, j, k, l, m;

/*                                   BUFFER FORMATS                           *
 * -------------------------------------------------------------------------  *
 * motosh_g_antcap_cal[] (factory cal)    | motosh_antcap_i2c_rbuf[] (I2C)    *
 *   [ 0] = 0x8a  : cal header            |   [ 0] = 0x85  : getcfg header    *
 *   [ 1] = 0x13  : length of payload     |   [ 1] = 0x12  : payload length   *
 *   [ 2] = 0 / 1 : calibration pass/fail |   [ 2] = 0x05  : cal_data reply   *
 *   [ 3] = 0x33  : hi-byte of cal target |   [ 3] = 0 / 1 : getcfg pass/fail *
 *   [ 4] = 0x33  : lo-byte of cal target |                                   *
 *   [ 5]         : cal data[00]          |   [ 4]         : cal data[00]     *
 *   ...                                  |   ...                             *
 *   [20]         : cal data[15]          |   [19]         : cal data[15]     *
 *   ...                                  |   ...                             */

	l = (motosh_g_antcap_cal[1] - 3);
	m = motosh_antcap_i2c_rbuf[3];
	for (i = 0, j = 5, k = 4; i < l; i++, j++, k++) {
		if (motosh_g_antcap_cal[j] != motosh_antcap_i2c_rbuf[k])
			m = 1;

		dev_dbg(&motosh_misc_data->client->dev,
			"motosh_antcap_check_cal: %02x %02x %01x\n",
			motosh_g_antcap_cal[j],
			motosh_antcap_i2c_rbuf[k], m);
	}

	return m;
}

/************************************************************/
/* synchronize Cypress boot and SH operation using notifier */
/************************************************************/

static int motosh_antcap_enable(struct notifier_block *self,
				unsigned long event,
				void *data)
{
	int           err = 0;

	if (event == HSSP_STOP) {
		motosh_g_antcap_enabled |=  ANTCAP_ENABLED;
		motosh_g_antcap_hw_ready = 1;
	} else {
		motosh_g_antcap_enabled &= ~ANTCAP_ENABLED;
		motosh_g_antcap_hw_ready = 0;
	}

	dev_dbg(&motosh_misc_data->client->dev,
		"motosh_antcap_enable: event=%d\n",
		((int) motosh_g_antcap_enabled));

	err = motosh_antcap_i2c_send_enable(1);

	dev_dbg(&motosh_misc_data->client->dev,
		"motosh_antcap_enable: err=%d en=%02x st=%02x\n",
		err, motosh_g_antcap_enabled, motosh_g_conn_state);

	return NOTIFY_OK;
}

static struct notifier_block antcap_notif = {
	.notifier_call  = motosh_antcap_enable,
};
static int hssp_notifier_registered;

/***************************************************************/
/* get USB cable information from power_supply driver directly */
/***************************************************************/

static int motosh_antcap_usb_pres = -1;

static void motosh_antcap_external_power_changed(struct power_supply *psy)
{
	struct power_supply        *usb_psy;
	union power_supply_propval pval = {0, };
	int                        err = 0;

	/* struct motosh_data *d = container_of(psy,
	 *					struct motosh_data,
	 *					antcap_psy);
	 */

	usb_psy = power_supply_get_by_name("usb");

	if (usb_psy)
		if (usb_psy->get_property)
			usb_psy->get_property(usb_psy,
					      POWER_SUPPLY_PROP_PRESENT,
					      &pval);

	dev_dbg(&motosh_misc_data->client->dev,
		"motosh_antcap_external_power_changed: %d.\n",
		((int) pval.intval));

	if (motosh_antcap_usb_pres != pval.intval) {
		motosh_antcap_usb_pres = pval.intval;

		if (motosh_antcap_usb_pres)
			motosh_g_conn_state |=  ANTCAP_USB;
		else
			motosh_g_conn_state &= ~ANTCAP_USB;

		err = motosh_antcap_i2c_send_enable(1);

		dev_dbg(&motosh_misc_data->client->dev,
			"motosh_antcap_external_power_changed i2c err=%d en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);
	}

	/* schedule_delayed_work(&d->antcap_psy_work, msecs_to_jiffies(100)); */
}

static enum power_supply_property motosh_antcap_ps_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int motosh_antcap_ps_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	val->intval = 0;
	return 0;
}

static const char * const motosh_antcap_usb_supply[] = { "usb", };

static struct work_struct motosh_antcap_headset_work;

static void motosh_antcap_headset_update(struct work_struct *work)
{
	int err = 0;

	err = motosh_antcap_i2c_send_enable(1);

	dev_dbg(&motosh_misc_data->client->dev,
		"switch_to_motosh_report: err=%d en=%02x st=%02x\n",
		err, motosh_g_antcap_enabled, motosh_g_conn_state);
}

int motosh_antcap_of_init(struct i2c_client *client)
{
	int                len;
	unsigned int       ret;
	struct motosh_data *d = motosh_misc_data;
	struct device_node *np = client->dev.of_node;

	/*********************************************************/
	/* register for USB connector insertion via power_supply */
	/*********************************************************/
	d->antcap_psy.num_supplies = 1;
	d->antcap_psy.supplied_from = ((char **) motosh_antcap_usb_supply);
	d->antcap_psy.name = "MOTOSH-ANTCAP-PSY-DRIVER";
	d->antcap_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
	d->antcap_psy.properties = motosh_antcap_ps_props;
	d->antcap_psy.num_properties = ARRAY_SIZE(motosh_antcap_ps_props);
	d->antcap_psy.get_property = motosh_antcap_ps_get_property;
	d->antcap_psy.external_power_changed =
		motosh_antcap_external_power_changed;
	ret = power_supply_register(&client->dev, &d->antcap_psy);
	if (ret < 0) {
		dev_err(&(client->dev),
			"antcap_psy power_supply_register fail = %d\n",
			ret);
		goto fail;
	}

	/************************************/
	/* read antcap_cfg from device tree */
	/************************************/
	if (of_get_property(np, "antcap_cfg", &len) == NULL) {
		dev_err(&motosh_misc_data->client->dev,
			"antcap_cfg len access failure\n");
		ret = 1;
		goto fail;
	}

	if ((len > 0) && (len <= MOTOSH_ANTCAP_CFG_BUFF_SIZE)) {
		of_property_read_u8_array(np, "antcap_cfg",
						((u8 *) motosh_g_antcap_cfg),
						len);
		dev_dbg(&motosh_misc_data->client->dev,
			"antcap_cfg table: %02x%02x%02x%02x%02x%02x...\n",
			motosh_g_antcap_cfg[0], motosh_g_antcap_cfg[1],
			motosh_g_antcap_cfg[2], motosh_g_antcap_cfg[3],
			motosh_g_antcap_cfg[4], motosh_g_antcap_cfg[5]);
	} else {
		for (len = 0; len < MOTOSH_ANTCAP_CFG_BUFF_SIZE; len++)
			motosh_g_antcap_cfg[len] = 0xff;

		dev_err(&motosh_misc_data->client->dev,
			"antcap_cfg table is missing\n");
		ret = 1;
		goto fail;
	}

	/*****************************************************/
	/* create thread for I2C writes from headset handler */
	/*****************************************************/
	INIT_WORK(&motosh_antcap_headset_work, motosh_antcap_headset_update);

fail:
	return ret;
}


/***********************************************************/
/* get headset cable information from input switch handler */
/***********************************************************/

static void switch_to_motosh_report(unsigned long state)
{
	int           err = 0;
	unsigned char st = motosh_g_conn_state;
	unsigned char en = motosh_g_antcap_enabled;

	if ((state & (1 << SW_HEADPHONE_INSERT))
		|| (state & (1 << SW_LINEOUT_INSERT)))
		motosh_g_conn_state |=  ANTCAP_HEADSET;
	else
		motosh_g_conn_state &= ~ANTCAP_HEADSET;

	if ((st != motosh_g_conn_state)
		|| (en != motosh_g_antcap_enabled)) {

		dev_dbg(&motosh_misc_data->client->dev,
			"switch_to_motosh_report: state=%08x, st=%d, en=%d\n",
			((unsigned int) state),
			((int) motosh_g_conn_state),
			((int) motosh_g_antcap_enabled));

		err = schedule_work(&motosh_antcap_headset_work);

		dev_dbg(&motosh_misc_data->client->dev,
			"switch_to_motosh_report: err=%d en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);
	}
}

static int switch_to_motosh_connect(struct input_handler *handler,
					struct input_dev *dev,
					const struct input_device_id *id)
{
	int                 ret;
	struct input_handle *handle;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;
	handle->dev     = dev;
	handle->handler = handler;
	handle->name    = "switch_to_motosh";

	ret = input_register_handle(handle);
	if (ret)
		goto err_input_register_handle;

	ret = input_open_device(handle);
	if (ret)
		goto err_input_open_device;

	switch_to_motosh_report(dev->sw[0]);

	return 0;

err_input_open_device:
	input_unregister_handle(handle);
err_input_register_handle:
	kfree(handle);
	return ret;
}

static void switch_to_motosh_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static bool switch_to_motosh_filter(struct input_handle *handle,
					unsigned int type,
					unsigned int code,
					int value) {

	dev_dbg(&motosh_misc_data->client->dev,
		"switch_to_motosh_filter: code=%d, state=%08x\n",
		code,
		((unsigned int) handle->dev->sw[0]));

	if ((code == SW_HEADPHONE_INSERT)
		|| (code == SW_LINEOUT_INSERT)
		|| (code == SW_RFKILL_ALL))
		switch_to_motosh_report(handle->dev->sw[0]);

	return false;
}

static const struct input_device_id switch_to_motosh_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_SW) },
	},
	{ },
};

MODULE_DEVICE_TABLE(input, switch_to_motosh_ids);

static struct input_handler switch_to_motosh_handler = {
	.filter     = switch_to_motosh_filter,
	.connect    = switch_to_motosh_connect,
	.disconnect = switch_to_motosh_disconnect,
	.name       = "switch_to_motosh",
	.id_table   = switch_to_motosh_ids,
};
static int switch_to_motosh_registered;


/**************************************************************/
/* provide registration function to be called in motosh probe */
/**************************************************************/

int motosh_antcap_register(void)
{
	int err = 0;

	/* register captouch driver with cycapsense_hssp driver */
	if (!hssp_notifier_registered) {
		register_hssp_update_notify(&antcap_notif);
		hssp_notifier_registered = 1;
	}

	/* register captouch driver with kernel input handler */
	if (!switch_to_motosh_registered) {
		err = input_register_handler(&switch_to_motosh_handler);
		if (err)
			dev_err(&motosh_misc_data->client->dev,
				"switch_to_motosh input_register_handler failed: %d\n",
				err);

		switch_to_motosh_registered = 1;
	}

	return err;
}

#endif  /* CONFIG_CYPRESS_CAPSENSE_HSSP */
