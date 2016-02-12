/*
 * Copyright (C) 2010-2016 Motorola Mobility LLC
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
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

#define LOWPOWER_ALLOWED   0

#define I2C_RETRIES			5
#define RESET_RETRIES			2
#define MOTOSH_DELAY_USEC		10
#define G_MAX				0x7FFF

#define MOTOSH_BUSY_SLEEP_USEC    10000
#define MOTOSH_BUSY_RESUME_COUNT  14
#define MOTOSH_BUSY_SUSPEND_COUNT 6
#define MOTOSH_I2C_FAIL_LIMIT     10

long motosh_time_delta;
unsigned int motosh_irq_disable;

module_param_named(irq_disable, motosh_irq_disable, uint, 0644);

unsigned short motosh_i2c_retry_delay = 13;

/* Remember the sensor state */
unsigned short motosh_g_acc_delay;
unsigned short motosh_g_mag_delay;
unsigned short motosh_g_gyro_delay;
uint8_t motosh_g_rv_6axis_delay = 40;
uint8_t motosh_g_rv_9axis_delay = 40;
uint8_t motosh_g_gravity_delay = 40;
uint8_t motosh_g_linear_accel_delay = 40;
unsigned short motosh_g_baro_delay;
unsigned short motosh_g_als_delay;
unsigned short motosh_g_step_counter_delay;
unsigned short motosh_g_ir_gesture_delay;
unsigned short motosh_g_ir_raw_delay;
unsigned long motosh_g_nonwake_sensor_state;
unsigned long motosh_g_wake_sensor_state;
unsigned short motosh_g_algo_state;
unsigned char motosh_g_motion_dur;
unsigned char motosh_g_zmotion_dur;
unsigned char motosh_g_control_reg[MOTOSH_CONTROL_REG_SIZE];
unsigned char motosh_g_mag_cal[MOTOSH_MAG_CAL_SIZE];
unsigned char motosh_g_gyro_cal[MOTOSH_GYRO_CAL_SIZE];
unsigned short motosh_g_control_reg_restore;
unsigned char motosh_g_ir_config_reg[MOTOSH_IR_CONFIG_REG_SIZE];
bool motosh_g_ir_config_reg_restore;
bool motosh_g_booted;

#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
unsigned char motosh_g_antcap_cal[MOTOSH_ANTCAP_CAL_BUFF_SIZE];
unsigned char motosh_g_antcap_cfg[MOTOSH_ANTCAP_CFG_BUFF_SIZE];
unsigned char motosh_g_conn_state;
unsigned char motosh_g_antcap_enabled = ANTCAP_CHECK_CAL;
unsigned char motosh_g_antcap_hw_ready;
unsigned char motosh_g_antcap_sw_ready;
#endif /* CONFIG_CYPRESS_CAPSENSE_HSSP */

struct motosh_algo_requst_t motosh_g_algo_requst[MOTOSH_NUM_ALGOS];

/* per algo config, request, and event registers */
const struct motosh_algo_info_t motosh_algo_info[MOTOSH_NUM_ALGOS] = {
	{ M_ALGO_MODALITY, 0, ALGO_REQ_MODALITY,
	  ALGO_EVT_MODALITY, MOTOSH_EVT_SZ_TRANSITION },
	{ M_ALGO_ORIENTATION, 0, ALGO_REQ_ORIENT,
	  ALGO_EVT_ORIENT, MOTOSH_EVT_SZ_TRANSITION },
	{ M_ALGO_STOWED, 0, ALGO_REQ_STOWED,
	  ALGO_EVT_STOWED, MOTOSH_EVT_SZ_TRANSITION },
	{ M_ALGO_ACCUM_MODALITY, ALGO_ACCUM_ALL_MODALITY,
	   ALGO_REQ_ACCUM_MODALITY, ALGO_EVT_ACCUM_MODALITY,
	   MOTOSH_EVT_SZ_ACCUM_STATE },
	{ M_ALGO_ACCUM_MVMT, 0, ALGO_REQ_ACCUM_MVMT,
	  ALGO_EVT_ACCUM_MVMT, MOTOSH_EVT_SZ_ACCUM_MVMT }
};

struct motosh_data *motosh_misc_data;

/* MOTOSH sysfs functions/attributes */

/* Attribute: timestamp_time_ns (RO) */
static ssize_t timestamp_time_ns_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	*((uint64_t *)buf) = motosh_timestamp_ns();
	return sizeof(uint64_t);
}

/* Attribute: rv_6axis_update_rate (RW) */
static ssize_t rv_6axis_update_rate_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	*((uint8_t *)buf) = motosh_g_rv_6axis_delay;
	return sizeof(uint8_t);
}
static ssize_t rv_6axis_update_rate_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int err = 0;
	if (count < 1)
		return -EINVAL;
	err = motosh_set_rv_6axis_update_rate(
		motosh_misc_data,
		*((uint8_t *)buf));
	if (err)
		return err;
	else
		return sizeof(uint8_t);
}

/* Attribute: rv_9axis_update_rate (RW) */
static ssize_t rv_9axis_update_rate_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	*((uint8_t *)buf) = motosh_g_rv_9axis_delay;
	return sizeof(uint8_t);
}
static ssize_t rv_9axis_update_rate_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int err = 0;
	if (count < 1)
		return -EINVAL;
	err = motosh_set_rv_9axis_update_rate(
		motosh_misc_data,
		*((uint8_t *)buf));
	if (err)
		return err;
	else
		return sizeof(uint8_t);
}

/* Attribute: gravity_update_rate */
static ssize_t gravity_update_rate_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	*((uint8_t *)buf) = motosh_g_gravity_delay;
	return sizeof(uint8_t);
}
static ssize_t gravity_update_rate_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int err = 0;
	if (count < 1)
		return -EINVAL;
	err = motosh_set_gravity_update_rate(
		motosh_misc_data,
		*((uint8_t *)buf));
	if (err)
		return err;
	else
		return sizeof(uint8_t);
}

/* Attribute: linear_accel_update_rate */
static ssize_t linear_accel_update_rate_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	*((uint8_t *)buf) = motosh_g_linear_accel_delay;
	return sizeof(uint8_t);
}
static ssize_t linear_accel_update_rate_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int err = 0;
	if (count < 1)
		return -EINVAL;
	err = motosh_set_linear_accel_update_rate(
		motosh_misc_data,
		*((uint8_t *)buf));
	if (err)
		return err;
	else
		return sizeof(uint8_t);
}

static struct device_attribute motosh_attributes[] = {
	__ATTR_RO(timestamp_time_ns),
	__ATTR(
		rv_6axis_update_rate,
		0664,
		rv_6axis_update_rate_show,
		rv_6axis_update_rate_store),
	__ATTR(
		rv_9axis_update_rate,
		0664,
		rv_9axis_update_rate_show,
		rv_9axis_update_rate_store),
	__ATTR(
		gravity_update_rate,
		0664,
		gravity_update_rate_show,
		gravity_update_rate_store),
	__ATTR(
		linear_accel_update_rate,
		0664,
		linear_accel_update_rate_show,
		linear_accel_update_rate_store),
	__ATTR_NULL
};

static int create_device_attributes(
	struct device *dev,
	struct device_attribute *attrs)
{
	int i;
	int err = 0;

	for (i = 0; attrs[i].attr.name != NULL; ++i) {
		err = device_create_file(dev, &attrs[i]);
		if (err)
			break;
	}

	if (err) {
		for (--i; i >= 0; --i)
			device_remove_file(dev, &attrs[i]);
	}

	return err;
}

static void remove_device_attributes(
	struct device *dev,
	struct device_attribute *attrs)
{
	int i;

	for (i = 0; attrs[i].attr.name != NULL; ++i)
		device_remove_file(dev, &attrs[i]);
}

static int create_sysfs_interfaces(struct motosh_data *ps_motosh)
{
	int err = 0;

	if (!ps_motosh)
		return -EINVAL;

	err = create_device_attributes(
		ps_motosh->motosh_class_dev,
		motosh_attributes);

	if (err < 0)
		remove_device_attributes(
			ps_motosh->motosh_class_dev,
			motosh_attributes);
	return err;
}

/* END: MOTOSH sysfs functions/attributes */

int64_t motosh_timestamp_ns(void)
{
	struct timespec ts;
	get_monotonic_boottime(&ts);
	return ts.tv_sec*1000000000LL + ts.tv_nsec;
}

void motosh_wake(struct motosh_data *ps_motosh)
{
	if (ps_motosh != NULL && ps_motosh->pdata != NULL) {
		if (!(ps_motosh->sh_lowpower_enabled))
			return;

		mutex_lock(&ps_motosh->sh_wakeup_lock);

		if (ps_motosh->sh_wakeup_count == 0) {
			int timeout = 5000;

			/* wake up the sensorhub and wait up to 5ms
			 * for it to respond */
			gpio_set_value(ps_motosh->pdata->gpio_sh_wake, 1);
			while (!gpio_get_value
			       (ps_motosh->pdata->gpio_sh_wake_resp)
			       && timeout--) {
				udelay(1);
			}

			if (timeout <= 0)
				dev_err(&ps_motosh->client->dev,
					"sensorhub wakeup timeout\n");
		}

		ps_motosh->sh_wakeup_count++;

		mutex_unlock(&ps_motosh->sh_wakeup_lock);
	}
}

void motosh_sleep(struct motosh_data *ps_motosh)
{
	if (ps_motosh != NULL && ps_motosh->pdata != NULL) {
		if (!(ps_motosh->sh_lowpower_enabled))
			return;

		mutex_lock(&ps_motosh->sh_wakeup_lock);

		if (ps_motosh->sh_wakeup_count > 0) {
			ps_motosh->sh_wakeup_count--;
			if (ps_motosh->sh_wakeup_count == 0) {
				gpio_set_value(ps_motosh->pdata->gpio_sh_wake,
					       0);
				udelay(1);
			}
		} else {
			dev_err(&ps_motosh->client->dev,
				"motosh_sleep called too many times: %d",
				ps_motosh->sh_wakeup_count);
		}

		mutex_unlock(&ps_motosh->sh_wakeup_lock);
	}
}

void motosh_detect_lowpower_mode(unsigned char *cmdbuff)
{
	int err;
	bool factory;
	struct device_node *np = of_find_node_by_path("/chosen");
	unsigned char readbuff[2];

	if (np) {
		/* detect factory cable and disable lowpower mode
		 * because TCMDs will talk directly to the motosh
		 * over i2c */
		factory = of_property_read_bool(np, "mmi,factory-cable");

		if (factory) {
			/* lowpower mode disabled for factory testing */
			dev_dbg(&motosh_misc_data->client->dev,
				"factory cable...lowpower disabled");
			motosh_misc_data->sh_lowpower_enabled = 0;
			return;
		}
	}

	if ((motosh_misc_data->pdata->gpio_sh_wake >= 0)
	    && (motosh_misc_data->pdata->gpio_sh_wake_resp >= 0)) {
		/* wait up to 5ms for the sensorhub to respond/power up */
		int timeout = 5000;
		mutex_lock(&motosh_misc_data->sh_wakeup_lock);

		/* hold sensorhub awake, it might try to sleep
		 * after we tell it the kernel supports low power */
		gpio_set_value(motosh_misc_data->pdata->gpio_sh_wake, 1);
		while (!gpio_get_value
		       (motosh_misc_data->pdata->gpio_sh_wake_resp)
		       && timeout--) {
			udelay(1);
		}

		/* detect whether lowpower mode is supported */
		dev_dbg(&motosh_misc_data->client->dev,
			"lowpower supported: ");

		cmdbuff[0] = LOWPOWER_REG;
		err =
		    motosh_i2c_write_read_no_reset(
			motosh_misc_data,
			cmdbuff,
			readbuff,
			1, 2);
		if (err >= 0) {
			if ((int)readbuff[1] == 1)
				motosh_misc_data->sh_lowpower_enabled = 1;
			else
				motosh_misc_data->sh_lowpower_enabled = 0;

			dev_dbg(&motosh_misc_data->client->dev,
				"lowpower supported: %d",
				motosh_misc_data->sh_lowpower_enabled);

			if (motosh_misc_data->sh_lowpower_enabled) {
				/* send back to the hub the kernel
				 * supports low power mode */
				cmdbuff[1] =
				    motosh_misc_data->sh_lowpower_enabled;
				err =
				    motosh_i2c_write_no_reset(motosh_misc_data,
							      cmdbuff,
							      2);

				if (err < 0) {
					/* if we failed to let the sensorhub
					 * know we support lowpower mode
					 * disable it */
					motosh_misc_data->sh_lowpower_enabled =
					    0;
				}
			}
		} else {
			dev_err(&motosh_misc_data->client->dev,
				"error reading lowpower supported %d",
				err);
				/* if we failed to read the sensorhub
				 * disable lowpower mode */
				motosh_misc_data->sh_lowpower_enabled =
				    0;
		}

		mutex_unlock(&motosh_misc_data->sh_wakeup_lock);
	}
}

int motosh_i2c_write_read_no_reset(
	struct motosh_data *ps_motosh,
	u8 *writebuff,
	u8 *readbuff,
	int writelen,
	int readlen)
{
	int tries, err = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = ps_motosh->client->addr,
			.flags = ps_motosh->client->flags,
			.len = writelen,
			.buf = writebuff,
		},
		{
			.addr = ps_motosh->client->addr,
			.flags = ps_motosh->client->flags | I2C_M_RD,
			.len = readlen,
			.buf = readbuff,
		},
	};

	if (writebuff == NULL ||
	    readbuff == NULL ||
	    writelen == 0 ||
	    readlen == 0)
		return -EFAULT;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	tries = 0;
	do {
		err = i2c_transfer(ps_motosh->client->adapter, msgs, 2);

		if (err != 2)
			msleep(motosh_i2c_retry_delay);
	} while ((err != 2) && (++tries < I2C_RETRIES));
	if (err != 2) {
		dev_err(&ps_motosh->client->dev, "Read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
		dev_dbg(&ps_motosh->client->dev, "Read from MOTOSH: ");
		for (tries = 0; tries < readlen; tries++)
			dev_dbg(&ps_motosh->client->dev, "%02x",
				readbuff[tries]);
	}

	return err;
}

int motosh_i2c_read_no_reset(struct motosh_data *ps_motosh,
			u8 *buf, int len)
{
	int tries, err = 0;

	if (buf == NULL || len == 0)
		return -EFAULT;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	tries = 0;
	do {
		err = i2c_master_recv(ps_motosh->client, buf, len);
		if (err < 0)
			msleep(motosh_i2c_retry_delay);
	} while ((err < 0) && (++tries < I2C_RETRIES));
	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "i2c read transfer error\n");
		err = -EIO;
	} else {
		dev_dbg(&ps_motosh->client->dev, "i2c read was successsful:\n");
		for (tries = 0; tries < err; tries++)
			dev_dbg(&ps_motosh->client->dev, "%02x", buf[tries]);
	}

	return err;
}

int motosh_i2c_write_no_reset(struct motosh_data *ps_motosh,
			u8 *buf, int len)
{
	int err = 0;
	int tries = 0;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	do {
		err = i2c_master_send(ps_motosh->client, buf, len);
		if (err < 0)
			msleep(motosh_i2c_retry_delay);
	} while ((err < 0) && (++tries < I2C_RETRIES));

	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"i2c write error - 0x%x - 0x%x\n", buf[0], -err);
		err = -EIO;
	} else {
		dev_dbg(&ps_motosh->client->dev,
			"i2c write successful\n");
		err = 0;
	}

	return err;
}

static int motosh_hw_init(struct motosh_data *ps_motosh)
{
	int err = 0;
	dev_dbg(&ps_motosh->client->dev, "motosh_hw_init\n");
	ps_motosh->hw_initialized = 1;
	return err;
}

static void motosh_device_power_off(struct motosh_data *ps_motosh)
{
	dev_dbg(&ps_motosh->client->dev,
		"motosh_device_power_off\n");
	if (ps_motosh->hw_initialized == 1) {
		if (ps_motosh->pdata->power_off)
			ps_motosh->pdata->power_off();
		ps_motosh->hw_initialized = 0;
	}
}

static int motosh_device_power_on(struct motosh_data *ps_motosh)
{
	int err = 0;
	dev_dbg(&ps_motosh->client->dev, "motosh_device_power_on\n");
	if (ps_motosh->pdata->power_on) {
		err = ps_motosh->pdata->power_on();
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"power_on failed: %d\n", err);
			return err;
		}
	}
	if (!ps_motosh->hw_initialized) {
		err = motosh_hw_init(ps_motosh);
		if (err < 0) {
			motosh_device_power_off(ps_motosh);
			return err;
		}
	}
	return err;
}

int motosh_i2c_write_read(
	struct motosh_data *ps_motosh,
	u8 *writebuff,
	u8 *readbuff,
	int writelen,
	int readlen)
{
	int err = 0;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	if (writebuff == NULL ||
	    readbuff == NULL ||
	    writelen == 0 ||
	    readlen == 0)
		return -EFAULT;

	err = motosh_i2c_write_read_no_reset(ps_motosh,
		writebuff, readbuff, writelen, readlen);
	if (err < 0)
		motosh_reset_and_init(START_RESET);

	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "write_read error\n");
		err = -EIO;
	} else {
		dev_dbg(&ps_motosh->client->dev, "write_read successsful:\n");
	}

	return err;
}

int motosh_i2c_read(struct motosh_data *ps_motosh, u8 *buf, int len)
{
	int err = 0;

	if (buf == NULL || len == 0)
		return -EFAULT;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	err = motosh_i2c_read_no_reset(ps_motosh, buf, len);
	if (err < 0)
		motosh_reset_and_init(START_RESET);

	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "read error\n");
		err = -EIO;
	} else {
		dev_dbg(&ps_motosh->client->dev, "read successsful:\n");
	}
	return err;
}

int motosh_i2c_write(struct motosh_data *ps_motosh, u8 *buf, int len)
{
	int err = 0;

	if (ps_motosh->mode == BOOTMODE)
		return -EFAULT;

	err = motosh_i2c_write_no_reset(ps_motosh, buf, len);
	if (err < 0)
		motosh_reset_and_init(START_RESET);

	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "write error - %x\n", buf[0]);
		err = -EIO;
	} else {
		dev_dbg(&ps_motosh->client->dev,
			"write successful\n");
		err = 0;
	}
	return err;
}

static ssize_t dock_print_name(struct switch_dev *switch_dev, char *buf)
{
	switch (switch_get_state(switch_dev)) {
	case NO_DOCK:
		return snprintf(buf, 5, "None\n");
	case DESK_DOCK:
		return snprintf(buf, 5, "DESK\n");
	case CAR_DOCK:
		return snprintf(buf, 4, "CAR\n");
	}

	return -EINVAL;
}

int motosh_enable(struct motosh_data *ps_motosh)
{
	int err = 0;

	dev_dbg(&ps_motosh->client->dev, "motosh_enable\n");
	if (!atomic_cmpxchg(&ps_motosh->enabled, 0, 1)) {
		err = motosh_device_power_on(ps_motosh);
		if (err < 0) {
			atomic_set(&ps_motosh->enabled, 0);
			dev_err(&ps_motosh->client->dev,
				"motosh_enable returned with %d\n", err);
			return err;
		}
	}

	return err;
}

struct miscdevice motosh_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &motosh_misc_fops,
};

#ifdef CONFIG_OF
static struct motosh_platform_data *
motosh_of_init(struct i2c_client *client)
{
	int len;
	int lsize, bsize, index;
	struct motosh_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	unsigned int lux_table[LIGHTING_TABLE_SIZE];
	unsigned int brightness_table[LIGHTING_TABLE_SIZE];
	const char *name;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&motosh_misc_data->client->dev,
			"pdata allocation failure\n");
		return NULL;
	}

	pdata->gpio_int = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);
	pdata->gpio_bslen = of_get_gpio(np, 2);
	pdata->gpio_wakeirq = of_get_gpio(np, 3);

	if (LOWPOWER_ALLOWED && of_gpio_count(np) >= 6) {
		pdata->gpio_sh_wake = of_get_gpio(np, 4);
		pdata->gpio_sh_wake_resp = of_get_gpio(np, 5);
		motosh_misc_data->sh_lowpower_enabled = 1;
	} else {
		pdata->gpio_sh_wake = -1;
		pdata->gpio_sh_wake_resp = -1;
		motosh_misc_data->sh_lowpower_enabled = 0;
	}
	motosh_misc_data->sh_wakeup_count = 0;

	if (of_get_property(np, "lux_table", &len) == NULL) {
		dev_err(&motosh_misc_data->client->dev,
			"lux_table len access failure\n");
		return NULL;
	}
	lsize = len / sizeof(u32);
	if ((lsize != 0) && (lsize < (LIGHTING_TABLE_SIZE - 1)) &&
		(!of_property_read_u32_array(np, "lux_table",
					(u32 *)(lux_table),
					lsize))) {
		for (index = 0; index < lsize; index++)
			pdata->lux_table[index] = ((u32 *)lux_table)[index];
	} else {
		dev_err(&motosh_misc_data->client->dev,
			"Lux table is missing\n");
		return NULL;
	}
	pdata->lux_table[lsize] = 0xFFFF;

	if (of_get_property(np, "brightness_table", &len) == NULL) {
		dev_err(&motosh_misc_data->client->dev,
			"Brightness_table len access failure\n");
		return NULL;
	}
	bsize = len / sizeof(u32);
	if ((bsize != 0) && (bsize < (LIGHTING_TABLE_SIZE)) &&
		!of_property_read_u32_array(np,
					"brightness_table",
					(u32 *)(brightness_table),
					bsize)) {

		for (index = 0; index < bsize; index++) {
			pdata->brightness_table[index]
				= ((u32 *)brightness_table)[index];
		}
	} else {
		dev_err(&motosh_misc_data->client->dev,
			"Brightness table is missing\n");
		return NULL;
	}

	if ((lsize + 1) != bsize) {
		dev_err(&motosh_misc_data->client->dev,
			"Lux and Brightness table sizes don't match\n");
		return NULL;
	}

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	if (of_property_read_u32(np, "qd_pm_qos_latency",
				&pdata->qd_pm_qos_latency)) {
		dev_warn(&motosh_misc_data->client->dev,
			"PM QoS latency not configured!\n");
		pdata->qd_pm_qos_latency = PM_QOS_DEFAULT_VALUE;
	}
	if (of_property_read_u32(np, "qd_pm_qos_timeout",
				&pdata->qd_pm_qos_timeout)) {
		dev_warn(&motosh_misc_data->client->dev,
			"PM QoS timeout not configured!\n");
		pdata->qd_pm_qos_timeout = 0;
	}

	pdata->aod_touch_mode = 0;
	of_property_read_u32(np, "aod_touch_mode",
			&pdata->aod_touch_mode);
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

	of_property_read_u32(np, "bslen_pin_active_value",
				&pdata->bslen_pin_active_value);

	of_get_property(np, "motosh_fw_version", &len);
	if (!of_property_read_string(np, "motosh_fw_version", &name))
		strlcpy(pdata->fw_version, name, FW_VERSION_SIZE);
	else
		dev_dbg(&motosh_misc_data->client->dev,
			"Not useing motosh_fw_version override\n");

	pdata->ct406_detect_threshold = 0x006E;
	pdata->ct406_undetect_threshold = 0x0050;
	pdata->ct406_recalibrate_threshold = 0x0064;
	pdata->ct406_pulse_count = 0x02;
	pdata->ct406_prox_gain = 0x02;
	of_property_read_u32(np, "ct406_detect_threshold",
				&pdata->ct406_detect_threshold);
	of_property_read_u32(np, "ct406_undetect_threshold",
				&pdata->ct406_undetect_threshold);
	of_property_read_u32(np, "ct406_recalibrate_threshold",
				&pdata->ct406_recalibrate_threshold);
	of_property_read_u32(np, "ct406_pulse_count",
				&pdata->ct406_pulse_count);
	of_property_read_u32(np, "ct406_prox_gain",
				&pdata->ct406_prox_gain);
	pdata->ct406_als_lux1_c0_mult = 0x2ED;
	pdata->ct406_als_lux1_c1_mult = 0x9CA;
	pdata->ct406_als_lux1_div = 0x46;
	pdata->ct406_als_lux2_c0_mult = 0x150;
	pdata->ct406_als_lux2_c1_mult = 0x237;
	pdata->ct406_als_lux2_div = 0x46;
	of_property_read_u32(np, "ct406_als_lux1_c0_mult",
				&pdata->ct406_als_lux1_c0_mult);
	of_property_read_u32(np, "ct406_als_lux1_c1_mult",
				&pdata->ct406_als_lux1_c1_mult);
	of_property_read_u32(np, "ct406_als_lux1_div",
				&pdata->ct406_als_lux1_div);
	of_property_read_u32(np, "ct406_als_lux2_c0_mult",
				&pdata->ct406_als_lux2_c0_mult);
	of_property_read_u32(np, "ct406_als_lux2_c1_mult",
				&pdata->ct406_als_lux2_c1_mult);
	of_property_read_u32(np, "ct406_als_lux2_div",
				&pdata->ct406_als_lux2_div);

#ifdef CONFIG_SENSORS_MOTOSH_HEADSET
	pdata->headset_detect_enable = 0;
	pdata->headset_insertion_debounce = 0x01F4;
	pdata->headset_removal_debounce = 0x01F4;
	pdata->headset_button_down_debounce = 0x0032;
	pdata->headset_button_up_debounce = 0x0032;
	pdata->headset_button_0_1_threshold = 0x0096;
	pdata->headset_button_1_2_threshold = 0x012C;
	pdata->headset_button_2_3_threshold = 0x01C2;
	pdata->headset_button_3_upper_threshold = 0x02EE;
	pdata->headset_button_1_keycode = 0xE2;
	pdata->headset_button_2_keycode = 0x0;
	pdata->headset_button_3_keycode = 0x0;
	pdata->headset_button_4_keycode = 0x0;
	of_property_read_u32(np, "headset_detect_enable",
			&pdata->headset_detect_enable);
	of_property_read_u32(np, "headset_insertion_debounce",
			&pdata->headset_insertion_debounce);
	of_property_read_u32(np, "headset_removal_debounce",
			&pdata->headset_removal_debounce);
	of_property_read_u32(np, "headset_button_down_debounce",
			&pdata->headset_button_down_debounce);
	of_property_read_u32(np, "headset_button_up_debounce",
			&pdata->headset_button_up_debounce);
	of_property_read_u32(np, "headset_button_0-1_threshold",
			&pdata->headset_button_0_1_threshold);
	of_property_read_u32(np, "headset_button_1-2_threshold",
			&pdata->headset_button_1_2_threshold);
	of_property_read_u32(np, "headset_button_2-3_threshold",
			&pdata->headset_button_2_3_threshold);
	of_property_read_u32(np, "headset_button_3-upper_threshold",
			&pdata->headset_button_3_upper_threshold);
	of_property_read_u32(np, "headset_button_1_keycode",
			&pdata->headset_button_1_keycode);
	of_property_read_u32(np, "headset_button_2_keycode",
			&pdata->headset_button_2_keycode);
	of_property_read_u32(np, "headset_button_3_keycode",
			&pdata->headset_button_3_keycode);
	of_property_read_u32(np, "headset_button_4_keycode",
			&pdata->headset_button_4_keycode);
#endif /* CONFIG_SENSORS_MOTOSH_HEADSET */

	pdata->cover_detect_polarity = MOTOSH_HALL_NO_DETECT;
	of_property_read_u32(np, "cover_detect_polarity",
				&pdata->cover_detect_polarity);

	pdata->accel_orient = 1;
	pdata->gyro_orient = 1;
	pdata->mag_orient = 1;
	pdata->mag_config = 0;
	pdata->panel_type = 1;
	pdata->IR_config = 1;
	of_property_read_u32(np, "accel_orient",
			&pdata->accel_orient);
	of_property_read_u32(np, "gyro_orient",
			&pdata->gyro_orient);
	of_property_read_u32(np, "mag_orient",
			&pdata->mag_orient);
	of_property_read_u32(np, "mag_config",
			&pdata->mag_config);
	of_property_read_u32(np, "panel_type",
			&pdata->panel_type);
	of_property_read_u32(np, "IR_config",
			&pdata->IR_config);

	return pdata;
}
#else
static inline struct motosh_platform_data *
motosh_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int motosh_gpio_init(struct motosh_platform_data *pdata,
				struct i2c_client *pdev)
{
	int err;

	err = gpio_request(pdata->gpio_int, "motosh int");
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"motosh int gpio_request failed: %d\n", err);
		return err;
	}
	gpio_direction_input(pdata->gpio_int);
	err = gpio_export(pdata->gpio_int, 0);
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"gpio_int gpio_export failed: %d\n", err);
		goto free_int;
	}
	err = gpio_export_link(&pdev->dev, "gpio_irq", pdata->gpio_int);
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"gpio_irq gpio_export_link failed: %d\n", err);
		goto free_int;
	}

	err = gpio_request(pdata->gpio_reset, "motosh reset");
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"motosh reset gpio_request failed: %d\n", err);
		goto free_int;
	}
	gpio_direction_output(pdata->gpio_reset, 1);

	/* Keep part in reset until end of probe sequence to avoid early
	   handling of irq related communication */
	gpio_set_value(pdata->gpio_reset, 0);

	err = gpio_export(pdata->gpio_reset, 0);
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"reset gpio_export failed: %d\n", err);
		goto free_reset;
	}

	err = gpio_request(pdata->gpio_bslen, "motosh bslen");
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"bslen gpio_request failed: %d\n", err);
		goto free_reset;
	}
	gpio_direction_output(pdata->gpio_bslen, 0);
	gpio_set_value(pdata->gpio_bslen, 0);
	err = gpio_export(pdata->gpio_bslen, 0);
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"bslen gpio_export failed: %d\n", err);
		goto free_bslen;
	}

	if ((pdata->gpio_sh_wake >= 0) && (pdata->gpio_sh_wake_resp >= 0)) {
		/* pin to pull the stm chip out of lowpower mode */
		err = gpio_request(pdata->gpio_sh_wake, "motosh sh_wake");
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"sh_wake gpio_request failed: %d\n", err);
			goto free_bslen;
		}
		gpio_direction_output(pdata->gpio_sh_wake, 0);
		gpio_set_value(pdata->gpio_sh_wake, 1);
		err = gpio_export(pdata->gpio_sh_wake, 0);
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"sh_wake gpio_export failed: %d\n", err);
			goto free_wake_sh;
		}

		/* pin for the response from stm that it is awake */
		err =
		    gpio_request(pdata->gpio_sh_wake_resp,
				 "motosh sh_wake_resp");
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"sh_wake_resp gpio_request failed: %d\n", err);
			goto free_wake_sh;
		}
		gpio_direction_input(pdata->gpio_sh_wake_resp);
		err = gpio_export(pdata->gpio_sh_wake_resp, 0);
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"sh_wake_resp gpio_export failed: %d\n", err);
			goto free_sh_wake_resp;
		}
		err =
		    gpio_export_link(&pdev->dev, "gpio_sh_wake_resp",
				     pdata->gpio_sh_wake_resp);
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"gpio_sh_wake_resp gpio_export_link failed: %d\n",
				err);
			goto free_sh_wake_resp;
		}
	} else {
		dev_err(&motosh_misc_data->client->dev,
			"%s: pins for stm lowpower mode not specified\n",
			__func__);
	}

	if (gpio_is_valid(pdata->gpio_wakeirq)) {
		err = gpio_request(pdata->gpio_wakeirq, "motosh wakeirq");
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"wakeirq gpio_request failed: %d\n", err);
			goto free_sh_wake_resp;
		}
		gpio_direction_input(pdata->gpio_wakeirq);
		err = gpio_export(pdata->gpio_wakeirq, 0);
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"wakeirq gpio_export failed: %d\n", err);
			goto free_wakeirq;
		}

		err = gpio_export_link(&pdev->dev, "wakeirq",
						pdata->gpio_wakeirq);
		if (err) {
			dev_err(&motosh_misc_data->client->dev,
				"wakeirq link failed: %d\n", err);
			goto free_wakeirq;
		}
	} else {
		pr_warn("%s: gpio wake irq not specified\n", __func__);
	}

	return 0;

free_wakeirq:
	gpio_free(pdata->gpio_wakeirq);
free_sh_wake_resp:
	if (pdata->gpio_sh_wake_resp >= 0)
		gpio_free(pdata->gpio_sh_wake_resp);
free_wake_sh:
	if (pdata->gpio_sh_wake >= 0)
		gpio_free(pdata->gpio_sh_wake);
free_bslen:
	gpio_free(pdata->gpio_bslen);
free_reset:
	gpio_free(pdata->gpio_reset);
free_int:
	gpio_free(pdata->gpio_int);
	return err;
}

static void motosh_gpio_free(struct motosh_platform_data *pdata)
{
	gpio_free(pdata->gpio_int);
	gpio_free(pdata->gpio_reset);
	gpio_free(pdata->gpio_bslen);
	gpio_free(pdata->gpio_wakeirq);
	if (pdata->gpio_sh_wake >= 0)
		gpio_free(pdata->gpio_sh_wake);
	if (pdata->gpio_sh_wake_resp >= 0)
		gpio_free(pdata->gpio_sh_wake_resp);
}

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
#if defined(CONFIG_FB)
static int motosh_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct motosh_data *ps_motosh = container_of(self, struct motosh_data,
		fb_notif);
	int blank;
	bool vote = false;

	dev_dbg(&ps_motosh->client->dev, "%s+\n", __func__);

	if ((event != FB_EVENT_BLANK && event != FB_EARLY_EVENT_BLANK) ||
		!evdata || !evdata->data || !evdata->info
			|| evdata->info->node != 0)
		goto exit;

	blank = *(int *)evdata->data;

	/* determine vote */
	switch (event) {
	case FB_EVENT_BLANK:
		if (blank == FB_BLANK_POWERDOWN)
			vote = true;
		else
			goto exit; /* not interested in this event */
		break;
	case FB_EARLY_EVENT_BLANK:
		if (blank != FB_BLANK_POWERDOWN)
			vote = false;
		else
			goto exit; /* not interested in these events */
		break;
	default:
		goto exit;
	}

	if (ps_motosh->in_reset_and_init || ps_motosh->mode <= BOOTMODE) {
		/* store the kernel's vote */
		motosh_store_vote_aod_enabled(ps_motosh,
				AOD_QP_ENABLED_VOTE_KERN, vote);
		dev_warn(&ps_motosh->client->dev, "motosh in reset or BOOTMODE...bailing\n");
		goto exit;
	} else {
		mutex_lock(&ps_motosh->lock);
		motosh_vote_aod_enabled_locked(ps_motosh,
			AOD_QP_ENABLED_VOTE_KERN, vote);
		motosh_resolve_aod_enabled_locked(ps_motosh);
		mutex_unlock(&ps_motosh->lock);
	}

exit:
	dev_dbg(&ps_motosh->client->dev, "%s-\n", __func__);

	return 0;
}
#endif
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

static int motosh_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct motosh_platform_data *pdata;
	struct motosh_data *ps_motosh;
	int err = -1;
	dev_info(&client->dev, "probe begun\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		return -ENODEV;
	}

	ps_motosh = kzalloc(sizeof(*ps_motosh), GFP_KERNEL);
	if (ps_motosh == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data: %d\n", err);
		return -ENOMEM;
	}
	ps_motosh->client = client;
	motosh_misc_data = ps_motosh;

	INIT_LIST_HEAD(&(ps_motosh->as_queue.list));
	spin_lock_init(&(ps_motosh->as_queue_lock));

	if (client->dev.of_node)
		pdata = motosh_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data is NULL, exiting\n");
		err = -ENODEV;
		goto err_pdata;
	}

	/* initialize regulators */
	ps_motosh->regulator_1 = regulator_get(&client->dev, "sensor1");
	if (IS_ERR(ps_motosh->regulator_1)) {
		dev_err(&client->dev, "Failed to get VIO regulator\n");
		goto err_regulator;
	}

	ps_motosh->regulator_2 = regulator_get(&client->dev, "sensor2");
	if (IS_ERR(ps_motosh->regulator_2)) {
		dev_err(&client->dev, "Failed to get VCC regulator\n");
		regulator_put(ps_motosh->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_motosh->regulator_1)) {
		dev_err(&client->dev, "Failed to enable Sensor 1 regulator\n");
		regulator_put(ps_motosh->regulator_2);
		regulator_put(ps_motosh->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_motosh->regulator_2)) {
		dev_err(&client->dev, "Failed to enable Sensor 2 regulator\n");
		regulator_disable(ps_motosh->regulator_1);
		regulator_put(ps_motosh->regulator_2);
		regulator_put(ps_motosh->regulator_1);
		goto err_regulator;
	}

	err = motosh_gpio_init(pdata, client);
	if (err) {
		dev_err(&client->dev, "motosh gpio init failed\n");
		goto err_gpio_init;
	}

	mutex_init(&ps_motosh->lock);
	mutex_init(&ps_motosh->sh_wakeup_lock);

	mutex_lock(&ps_motosh->lock);
	wake_lock_init(&ps_motosh->wakelock, WAKE_LOCK_SUSPEND, "motosh");
	wake_lock_init(&ps_motosh->reset_wakelock, WAKE_LOCK_SUSPEND,
		"motosh_reset");

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	mutex_init(&ps_motosh->aod_enabled.vote_lock);
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

	/* Set to passive mode by default */
	motosh_g_nonwake_sensor_state = 0;
	motosh_g_wake_sensor_state = 0;
	/* clear the interrupt mask */
	ps_motosh->intp_mask = 0x00;

	ps_motosh->wake_work_delay = 0;
	INIT_WORK(&ps_motosh->irq_work, motosh_irq_work_func);
	INIT_DELAYED_WORK(&ps_motosh->irq_wake_work, motosh_irq_wake_work_func);

	ps_motosh->irq_work_queue = create_singlethread_workqueue("motosh_wq");
	if (!ps_motosh->irq_work_queue) {
		err = -ENOMEM;
		dev_err(&client->dev, "cannot create work queue: %d\n", err);
		goto err1;
	}
	ps_motosh->pdata = pdata;
	i2c_set_clientdata(client, ps_motosh);
	ps_motosh->client->flags &= 0x00;

	if (ps_motosh->pdata->init) {
		err = ps_motosh->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err2;
		}
	}

	/*configure interrupt*/
	ps_motosh->irq = gpio_to_irq(ps_motosh->pdata->gpio_int);
	if (gpio_is_valid(ps_motosh->pdata->gpio_wakeirq))
		ps_motosh->irq_wake
			= gpio_to_irq(ps_motosh->pdata->gpio_wakeirq);
	else
		ps_motosh->irq_wake = -1;

	err = motosh_device_power_on(ps_motosh);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err4;
	}

	if (ps_motosh->irq_wake != -1)
		enable_irq_wake(ps_motosh->irq_wake);
	atomic_set(&ps_motosh->enabled, 1);

	err = misc_register(&motosh_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "misc register failed: %d\n", err);
		goto err6;
	}

	if (alloc_chrdev_region(&ps_motosh->motosh_dev_num, 0, 2, "motosh")
		< 0)
		dev_err(&client->dev,
			"alloc_chrdev_region failed\n");
	ps_motosh->motosh_class = class_create(THIS_MODULE, "motosh");

	cdev_init(&ps_motosh->as_cdev, &motosh_as_fops);
	ps_motosh->as_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_motosh->as_cdev, ps_motosh->motosh_dev_num, 1);
	if (err)
		dev_err(&client->dev,
			"cdev_add as failed: %d\n", err);

	ps_motosh->motosh_class_dev = device_create(
		ps_motosh->motosh_class, NULL,
		MKDEV(MAJOR(ps_motosh->motosh_dev_num), 0),
		ps_motosh, "motosh_as");

	err = create_sysfs_interfaces(ps_motosh);
	if (err)
		dev_err(&client->dev,
			"create_sysfs_interfaces failed: %d",
			err);

	cdev_init(&ps_motosh->ms_cdev, &motosh_ms_fops);
	ps_motosh->ms_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_motosh->ms_cdev, ps_motosh->motosh_dev_num + 1, 1);
	if (err)
		dev_err(&client->dev,
			"cdev_add ms failed: %d\n", err);

	device_create(ps_motosh->motosh_class, NULL,
		MKDEV(MAJOR(ps_motosh->motosh_dev_num), 1),
		ps_motosh, "motosh_ms");

	motosh_device_power_off(ps_motosh);

	atomic_set(&ps_motosh->enabled, 0);

	err = request_irq(ps_motosh->irq, motosh_isr,
				IRQF_TRIGGER_RISING,
				NAME, ps_motosh);
	if (err < 0) {
		dev_err(&client->dev, "request irq failed: %d\n", err);
		goto err7;
	}

	if (ps_motosh->irq_wake != -1) {
		err = request_irq(ps_motosh->irq_wake, motosh_wake_isr,
				IRQF_TRIGGER_RISING,
				NAME, ps_motosh);
		if (err < 0) {
			dev_err(&client->dev, "request wake irq failed: %d\n",
				err);
			goto err8;
		}
	}

	init_waitqueue_head(&ps_motosh->motosh_as_data_wq);
	init_waitqueue_head(&ps_motosh->motosh_ms_data_wq);

	ps_motosh->dsdev.name = "dock";
	ps_motosh->dsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_motosh->dsdev);
	if (err) {
		dev_err(&client->dev,
			"Couldn't register switch (%s) rc=%d\n",
			ps_motosh->dsdev.name, err);
		ps_motosh->dsdev.dev = NULL;
	}

	ps_motosh->edsdev.name = "extdock";
	ps_motosh->edsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_motosh->edsdev);
	if (err) {
		dev_err(&client->dev,
			"Couldn't register switch (%s) rc=%d\n",
			ps_motosh->edsdev.name, err);
		ps_motosh->edsdev.dev = NULL;
	}

	ps_motosh->input_dev = input_allocate_device();
	if (!ps_motosh->input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"input device allocate failed: %d\n", err);
		goto err8;
	}
	input_set_drvdata(ps_motosh->input_dev, ps_motosh);
	input_set_capability(ps_motosh->input_dev, EV_KEY, KEY_POWER);
	input_set_capability(ps_motosh->input_dev, EV_KEY, KEY_CAMERA);
	input_set_capability(ps_motosh->input_dev, EV_SW, SW_LID);
#ifdef CONFIG_SENSORS_MOTOSH_HEADSET
	if (pdata->headset_button_1_keycode > 0)
		input_set_capability(ps_motosh->input_dev, EV_KEY,
				pdata->headset_button_1_keycode);
	if (pdata->headset_button_2_keycode > 0)
		input_set_capability(ps_motosh->input_dev, EV_KEY,
				pdata->headset_button_2_keycode);
	if (pdata->headset_button_3_keycode > 0)
		input_set_capability(ps_motosh->input_dev, EV_KEY,
				pdata->headset_button_3_keycode);
	if (pdata->headset_button_4_keycode > 0)
		input_set_capability(ps_motosh->input_dev, EV_KEY,
				pdata->headset_button_4_keycode);
	input_set_capability(ps_motosh->input_dev, EV_SW,
			SW_HEADPHONE_INSERT);
	input_set_capability(ps_motosh->input_dev, EV_SW,
			SW_MICROPHONE_INSERT);
#endif /* CONFIG_SENSORS_MOTOSH_HEADSET */
	ps_motosh->input_dev->name = "sensorprocessor";

	err = input_register_device(ps_motosh->input_dev);
	if (err) {
		dev_err(&client->dev,
			"unable to register input polled device %s: %d\n",
			ps_motosh->input_dev->name, err);
		goto err9;
	}

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
#if defined(CONFIG_FB)
	ps_motosh->fb_notif.notifier_call = motosh_fb_notifier_callback;
	/* We must make sure we are the first callback to run, high priority */
	ps_motosh->fb_notif.priority = 1;
	err = fb_register_client(&ps_motosh->fb_notif);
	if (err) {
		dev_err(&client->dev,
			"Error registering fb_notifier: %d\n", err);
		goto err10;
	}
#endif
	ps_motosh->quickpeek_work_queue =
		create_singlethread_workqueue("motosh_quickpeek_wq");
	if (!ps_motosh->quickpeek_work_queue) {
		err = -ENOMEM;
		dev_err(&client->dev, "cannot create work queue: %d\n", err);
		goto err11;
	}
	INIT_WORK(&ps_motosh->quickpeek_work, motosh_quickpeek_work_func);
	wake_lock_init(&ps_motosh->quickpeek_wakelock, WAKE_LOCK_SUSPEND,
		"motosh_quickpeek");
	INIT_LIST_HEAD(&ps_motosh->quickpeek_command_list);
	atomic_set(&ps_motosh->qp_enabled, 0);
	ps_motosh->qp_in_progress = false;
	ps_motosh->qp_prepared = false;
	init_waitqueue_head(&ps_motosh->quickpeek_wait_queue);

	ps_motosh->ignore_wakeable_interrupts = false;
	ps_motosh->ignored_interrupts = 0;
	ps_motosh->quickpeek_occurred = false;
	mutex_init(&ps_motosh->qp_list_lock);

	motosh_quickwakeup_init(ps_motosh);

	pm_qos_add_request(&ps_motosh->pm_qos_req_dma,
			PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

	ps_motosh->is_suspended = false;
	ps_motosh->resume_cleanup = false;

	/* We could call switch_motosh_mode(NORMALMODE) at this point, but
	 * instead we will hold the part in reset and only go to NORMALMODE on a
	 * request to do so from the flasher.  The flasher must be present, and
	 * it must verify the firmware file is available before switching to
	 * NORMALMODE. This is to prevent a build that is missing firmware or
	 * flasher from behaving as a normal build (with factory firmware in the
	 * part).
	 */

	mutex_unlock(&ps_motosh->lock);

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
	ps_motosh->hall_data = mmi_hall_init();
#endif

#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
	if (client->dev.of_node)
		motosh_antcap_of_init(client);

	err = motosh_antcap_register();
	if (err)
		dev_err(&motosh_misc_data->client->dev,
			"switch_to_motosh motosh_register_antcap failed: %d\n",
			err);
#endif

	dev_info(&client->dev, "probe finished\n");

	return 0;

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
err11:
#if defined(CONFIG_FB)
	fb_unregister_client(&ps_motosh->fb_notif);
err10:
#endif
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */
err9:
	input_free_device(ps_motosh->input_dev);
err8:
	free_irq(ps_motosh->irq, ps_motosh);
err7:
	misc_deregister(&motosh_misc_device);
err6:
	motosh_device_power_off(ps_motosh);
err4:
	if (ps_motosh->pdata->exit)
		ps_motosh->pdata->exit();
err2:
	destroy_workqueue(ps_motosh->irq_work_queue);
err1:
	mutex_unlock(&ps_motosh->lock);
	mutex_destroy(&ps_motosh->lock);
	wake_unlock(&ps_motosh->wakelock);
	wake_lock_destroy(&ps_motosh->wakelock);
	wake_unlock(&ps_motosh->reset_wakelock);
	wake_lock_destroy(&ps_motosh->reset_wakelock);
	motosh_gpio_free(pdata);
err_gpio_init:
	regulator_disable(ps_motosh->regulator_2);
	regulator_disable(ps_motosh->regulator_1);
	regulator_put(ps_motosh->regulator_2);
	regulator_put(ps_motosh->regulator_1);
err_regulator:
err_pdata:
	kfree(ps_motosh);
	return err;
}

static int motosh_remove(struct i2c_client *client)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(client);
	dev_err(&ps_motosh->client->dev, "motosh_remove\n");
	switch_dev_unregister(&ps_motosh->dsdev);
	switch_dev_unregister(&ps_motosh->edsdev);
	if (ps_motosh->irq_wake != -1)
		free_irq(ps_motosh->irq_wake, ps_motosh);
	free_irq(ps_motosh->irq, ps_motosh);
	misc_deregister(&motosh_misc_device);
	input_unregister_device(ps_motosh->input_dev);
	input_free_device(ps_motosh->input_dev);
	motosh_device_power_off(ps_motosh);
	if (ps_motosh->pdata->exit)
		ps_motosh->pdata->exit();
	motosh_gpio_free(ps_motosh->pdata);
	destroy_workqueue(ps_motosh->irq_work_queue);
	mutex_destroy(&ps_motosh->lock);
	wake_unlock(&ps_motosh->wakelock);
	wake_lock_destroy(&ps_motosh->wakelock);
	wake_unlock(&ps_motosh->reset_wakelock);
	wake_lock_destroy(&ps_motosh->reset_wakelock);
	disable_irq_wake(ps_motosh->irq);
#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	pm_qos_remove_request(&ps_motosh->pm_qos_req_dma);
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

	regulator_disable(ps_motosh->regulator_2);
	regulator_disable(ps_motosh->regulator_1);
	regulator_put(ps_motosh->regulator_2);
	regulator_put(ps_motosh->regulator_1);
#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
#if defined(CONFIG_FB)
	fb_unregister_client(&ps_motosh->fb_notif);
#endif
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */
	kfree(ps_motosh);

	return 0;
}

static void motosh_process_ignored_interrupts_locked(
						struct motosh_data *ps_motosh)
{
	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	ps_motosh->ignore_wakeable_interrupts = false;

	if (ps_motosh->ignored_interrupts) {
		ps_motosh->ignored_interrupts = 0;
		wake_lock_timeout(&ps_motosh->wakelock, HZ);
		queue_delayed_work(ps_motosh->irq_work_queue,
			&ps_motosh->irq_wake_work, 0);
	}
}

static int motosh_resume(struct device *dev)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(to_i2c_client(dev));
	dev_dbg(dev, "%s\n", __func__);

	ps_motosh->resume_cleanup = true;
	ps_motosh->is_suspended = false;

	mutex_lock(&ps_motosh->lock);

	/* During a quickwake interrupts will be set as ignored at the end of
	   quickpeek_work_func while the system is being re-suspended.  It is
	   possible for the quickwake suspend process to fail and the system to
	   be immediately resumed.  Since resume_early was already called before
	   the quickwake started (and therefore before quickpeek_work_func
	   disabled interrupts) we must re-enable interrupts here. */
	motosh_process_ignored_interrupts_locked(ps_motosh);

	if (ps_motosh->pending_wake_work) {
		queue_delayed_work(ps_motosh->irq_work_queue,
			&ps_motosh->irq_wake_work,
			msecs_to_jiffies(ps_motosh->wake_work_delay));
		ps_motosh->pending_wake_work = false;
	}

	/* Run the irq_work on resume to update any
	 * non-wakeable onchanged events. (display rotate / light sensor / etc.)
	 * Streaming data is now flushed in irq_work */
	if (motosh_irq_disable == 0)
		queue_work(ps_motosh->irq_work_queue,
			&ps_motosh->irq_work);

	mutex_unlock(&ps_motosh->lock);

	return 0;
}

static int motosh_resume_early(struct device *dev)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(to_i2c_client(dev));
	dev_dbg(dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->lock);

	/* If we received wakeable interrupts between suspend_late and
	   suspend_noirq, we need to reschedule the irq work to be handled now
	   that interrupts have been re-enabled. */
	motosh_process_ignored_interrupts_locked(ps_motosh);

	mutex_unlock(&ps_motosh->lock);

	return 0;
}

static int motosh_suspend(struct device *dev)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(to_i2c_client(dev));
	dev_dbg(dev, "%s\n", __func__);

	/* Stop processing non-wake irqs. Afterwards, no more non-wake
	 * irq work functions are scheduled. */
	ps_motosh->is_suspended = true;

	/* Acquire/release this mutex so that we are sure that any in-progress
	 * irq work function has completed before we allow suspend.
	 */
	mutex_lock(&ps_motosh->lock);
	mutex_unlock(&ps_motosh->lock);

	return 0;
}

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
static int motosh_suspend_late(struct device *dev)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(to_i2c_client(dev));
	dev_dbg(dev, "%s\n", __func__);

	if (!wait_event_timeout(ps_motosh->quickpeek_wait_queue,
		motosh_quickpeek_disable_when_idle(ps_motosh),
		msecs_to_jiffies(MOTOSH_LATE_SUSPEND_TIMEOUT)))
		return -EBUSY;

	return 0;
}
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

static int motosh_suspend_noirq(struct device *dev)
{
	struct motosh_data *ps_motosh = i2c_get_clientdata(to_i2c_client(dev));
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->lock);

	/* If we received wakeable interrupts between finishing a quickwake and
	   now, return an error so we will resume to process it instead of
	   dropping into suspend */
	if (ps_motosh->ignored_interrupts) {
		dev_dbg(dev,
			"Force system resume to handle deferred interrupts [%d]\n",
			ps_motosh->ignored_interrupts);
		ret = -EBUSY;
	}

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	ps_motosh->quickpeek_occurred = false;
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

	mutex_unlock(&ps_motosh->lock);

	return ret;
}

static const struct dev_pm_ops motosh_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(motosh_suspend, motosh_resume)
#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	.suspend_late = motosh_suspend_late,
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */
	.suspend_noirq = motosh_suspend_noirq,
	.resume_early = motosh_resume_early,
};

static const struct i2c_device_id motosh_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, motosh_id);

#ifdef CONFIG_OF
static struct of_device_id motosh_match_tbl[] = {
	{ .compatible = "stm,motosh" },
	{ },
};
MODULE_DEVICE_TABLE(of, motosh_match_tbl);
#endif

static struct i2c_driver motosh_driver = {
	.driver = {
		   .name = NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(motosh_match_tbl),
#ifdef CONFIG_PM
		   .pm = &motosh_pm_ops,
#endif
	},
	.probe = motosh_probe,
	.remove = motosh_remove,
	.id_table = motosh_id,
};

static int __init motosh_init(void)
{
	return i2c_add_driver(&motosh_driver);
}

static void __exit motosh_exit(void)
{
	i2c_del_driver(&motosh_driver);
	return;
}

module_init(motosh_init);
module_exit(motosh_exit);

MODULE_DESCRIPTION("MOTOSH sensor processor");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
