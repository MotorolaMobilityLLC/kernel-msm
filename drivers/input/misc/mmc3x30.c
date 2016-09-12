/*
 * Copyright (c) 2014, 2016 Linux Foundation. All rights reserved.
 * Linux Foundation chooses to take subject only to the GPLv2 license
 * terms, and distributes only under these terms.
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/sensors.h>
#include <asm/uaccess.h>

#include <linux/i2c/mmc3x30.h>

#define MMC3X30_DELAY_TM_MS	10

#define MMC3X30_DELAY_SET_MS	75
#define MMC3X30_DELAY_RESET_MS	75

#define MMC3X30_RETRY_COUNT	10
#define MMC3X30_DEFAULT_INTERVAL_MS	100
#define MMC3X30_TIMEOUT_SET_MS	15000

#define MMC3X30_PRODUCT_ID	0x09

/* POWER SUPPLY VOLTAGE RANGE */
#define MMC3X30_VDD_MIN_UV	2000000
#define MMC3X30_VDD_MAX_UV	3300000
#define MMC3X30_VIO_MIN_UV	1750000
#define MMC3X30_VIO_MAX_UV	1950000

enum {
	OBVERSE_X_AXIS_FORWARD = 0,
	OBVERSE_X_AXIS_RIGHTWARD,
	OBVERSE_X_AXIS_BACKWARD,
	OBVERSE_X_AXIS_LEFTWARD,
	REVERSE_X_AXIS_FORWARD,
	REVERSE_X_AXIS_RIGHTWARD,
	REVERSE_X_AXIS_BACKWARD,
	REVERSE_X_AXIS_LEFTWARD,
	MMC3X30_DIR_COUNT,
};

static char *mmc3x30_dir[MMC3X30_DIR_COUNT] = {
	[OBVERSE_X_AXIS_FORWARD] = "obverse-x-axis-forward",
	[OBVERSE_X_AXIS_RIGHTWARD] = "obverse-x-axis-rightward",
	[OBVERSE_X_AXIS_BACKWARD] = "obverse-x-axis-backward",
	[OBVERSE_X_AXIS_LEFTWARD] = "obverse-x-axis-leftward",
	[REVERSE_X_AXIS_FORWARD] = "reverse-x-axis-forward",
	[REVERSE_X_AXIS_RIGHTWARD] = "reverse-x-axis-rightward",
	[REVERSE_X_AXIS_BACKWARD] = "reverse-x-axis-backward",
	[REVERSE_X_AXIS_LEFTWARD] = "reverse-x-axis-leftward",
};

static s8 mmc3x30_rotation_matrix[MMC3X30_DIR_COUNT][9] = {
	[OBVERSE_X_AXIS_FORWARD] = {0, -1, 0, 1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, 1, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_BACKWARD] = {0, 1, 0, -1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, -1, 0, 0, 0, 1},
	[REVERSE_X_AXIS_FORWARD] = {0, 1, 0, 1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, -1, 0, 0, 0, -1},
	[REVERSE_X_AXIS_BACKWARD] = {0, -1, 0, -1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, 1, 0, 0, 0, -1},
};

struct mmc3x30_vec {
	int x;
	int y;
	int z;
};

struct mmc3x30_data {
	struct mutex		ecompass_lock;
	struct mutex		ops_lock;
	struct delayed_work	dwork;
	struct sensors_classdev	cdev;
	struct mmc3x30_vec	last;

	struct i2c_client	*i2c;
	struct input_dev	*idev;
	struct regulator	*vdd;
	struct regulator	*vio;
	struct regmap		*regmap;

	int			dir;
	int			auto_report;
	int			enable;
	int			poll_interval;
	int			power_enabled;
	unsigned long		timeout;
	unsigned int device_id;
};

static struct sensors_classdev sensors_cdev = {
	.name = "mmc3x30-mag",
	.vendor = "MEMSIC, Inc",
	.version = 1,
	.handle = SENSORS_MAGNETIC_FIELD_HANDLE,
	.type = SENSOR_TYPE_MAGNETIC_FIELD,
	.max_range = "1228.8",
	.resolution = "0.09765625", /* 1024 */
	.sensor_power = "0.35",
	.min_delay = 20000,
	.max_delay = 100000, /* 100ms */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = MMC3X30_DEFAULT_INTERVAL_MS,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct mmc3x30_data *mmc3x30_data_struct;

static int mmc3x30_read_xyz(struct mmc3x30_data *memsic,
		struct mmc3x30_vec *vec)
{
	//int count = 0;
	unsigned char data[6];
	//unsigned int status;
	struct mmc3x30_vec tmp;
	int rc = 0;

	mutex_lock(&memsic->ecompass_lock);
#if 0
	/* mmc3x30 need to be set periodly to avoid overflow */
	if (time_after(jiffies, memsic->timeout)) {

		if (memsic->device_id == MMC3524_DEVICE_ID || memsic->device_id == MMC3530_DEVICE_ID)
		{
			rc = regmap_write(memsic->regmap, MMC35XX_REG_CTRL0, MMC35XX_SET);
			if (rc) {
				dev_err(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
						          MMC35XX_REG_CTRL0, __LINE__, rc);
				goto exit;
			}

			/* Wait time to complete SET/RESET */
			usleep_range(1000, 1500);
			memsic->timeout = jiffies + msecs_to_jiffies(MMC3X30_TIMEOUT_SET_MS);

			dev_dbg(&memsic->i2c->dev, "mmc3x30 reset is done\n");

		} else if (memsic->device_id == MMC3630_DEVICE_ID)
		{
			rc = regmap_write(memsic->regmap, MMC36XX_REG_CTRL0, MMC36XX_SET);
			if (rc) {
				dev_err(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
						          MMC36XX_REG_CTRL0, __LINE__, rc);
				goto exit;
			}

			/* Wait time to complete SET/RESET */
			usleep_range(1000, 1500);
			memsic->timeout = jiffies + msecs_to_jiffies(MMC3X30_TIMEOUT_SET_MS);

			dev_dbg(&memsic->i2c->dev, "mmc3x30 reset is done\n");
		}

	}
#endif
	/* read xyz raw data */
	rc = regmap_bulk_read(memsic->regmap, MMC3X30_REG_DATA, data, 6);
	if (rc) {
		dev_err(&memsic->i2c->dev, "read reg %d failed at %d.(%d)\n",
				                       MMC3X30_REG_DATA, __LINE__, rc);
		goto exit;
	}


	tmp.x = (((u8)data[1]) << 8 | (u8)data[0]) - 32768;
	tmp.y = (((u8)data[3]) << 8 | (u8)data[2]) - (((u8)data[5]) << 8 | (u8)data[4]) ;
	tmp.z = (((u8)data[3]) << 8 | (u8)data[2]) +  (((u8)data[5]) << 8 | (u8)data[4]) - 32768 - 32768;

	vec->x = tmp.x;
	vec->y = tmp.y;
	vec->z = -tmp.z;

exit:
	/* send TM cmd before read */
	if (memsic->device_id == MMC3524_DEVICE_ID || memsic->device_id == MMC3530_DEVICE_ID)
	{
		if (regmap_write(memsic->regmap, MMC35XX_REG_CTRL0, MMC35XX_SINGLE_TM)) {

		    dev_warn(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
				         MMC35XX_REG_CTRL0, __LINE__, rc);
		}

	} else if (memsic->device_id == MMC3630_DEVICE_ID) {
		if (regmap_write(memsic->regmap, MMC36XX_REG_CTRL0, MMC36XX_SINGLE_TM)) {

		    dev_warn(&memsic->i2c->dev, "write reg %d failed at %d.(%d)\n",
				         MMC36XX_REG_CTRL0, __LINE__, rc);
		}

	}

	mutex_unlock(&memsic->ecompass_lock);
	return rc;
}

static void mmc3x30_poll(struct work_struct *work)
{
	int ret;
	s8 *tmp;
	struct mmc3x30_vec vec;
	struct mmc3x30_vec report;
	struct mmc3x30_data *memsic = container_of((struct delayed_work *)work,
			struct mmc3x30_data, dwork);

	vec.x = vec.y = vec.z = 0;

	ret = mmc3x30_read_xyz(memsic, &vec);
	if (ret) {
		dev_warn(&memsic->i2c->dev, "read xyz failed\n");
		goto exit;
	}

	tmp = &mmc3x30_rotation_matrix[memsic->dir][0];
	report.x = tmp[0] * vec.x + tmp[1] * vec.y + tmp[2] * vec.z;
	report.y = tmp[3] * vec.x + tmp[4] * vec.y + tmp[5] * vec.z;
	report.z = tmp[6] * vec.x + tmp[7] * vec.y + tmp[8] * vec.z;

	input_report_abs(memsic->idev, ABS_X, report.x);
	input_report_abs(memsic->idev, ABS_Y, report.y);
	input_report_abs(memsic->idev, ABS_Z, report.z);
	input_sync(memsic->idev);

exit:
	schedule_delayed_work(&memsic->dwork,
			msecs_to_jiffies(memsic->poll_interval));
}

static struct input_dev *mmc3x30_init_input(struct i2c_client *client)
{
	int status;
	struct input_dev *input = NULL;

	input = devm_input_allocate_device(&client->dev);
	if (!input)
		return NULL;

	input->name = "compass";
	input->phys = "mmc3x30/input0";
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_X, -1024*30, 1024*30, 0, 0);
	input_set_abs_params(input, ABS_Y, -1024*30, 1024*30, 0, 0);
	input_set_abs_params(input, ABS_Z, -1024*30, 1024*30, 0, 0);

	//input_set_capability(input, EV_ABS, ABS_X);
	//input_set_capability(input, EV_ABS, ABS_Y);
	//input_set_capability(input, EV_ABS, ABS_Z);

	status = input_register_device(input);
	if (status) {
		dev_err(&client->dev,
			"error registering input device\n");
		return NULL;
	}

	return input;
}

static int mmc3x30_power_init(struct mmc3x30_data *data)
{
	int rc;

	data->vdd = devm_regulator_get(&data->i2c->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->i2c->dev,
				"Regualtor get failed vdd rc=%d\n", rc);
		return rc;
	}
	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd,
				MMC3X30_VDD_MIN_UV, MMC3X30_VDD_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
			goto exit;
		}
	}

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vdd failed rc=%d\n", rc);
		goto exit;
	}
	data->vio = devm_regulator_get(&data->i2c->dev, "vio");
	if (IS_ERR(data->vio)) {
		rc = PTR_ERR(data->vio);
		dev_err(&data->i2c->dev,
				"Regulator get failed vio rc=%d\n", rc);
		goto reg_vdd_set;
	}

	if (regulator_count_voltages(data->vio) > 0) {
		rc = regulator_set_voltage(data->vio,
				MMC3X30_VIO_MIN_UV, MMC3X30_VIO_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}
	}
	rc = regulator_enable(data->vio);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vio failed rc=%d\n", rc);
		goto reg_vdd_set;
	}

	 /* The minimum time to operate device after VDD valid is 10 ms. */
	usleep_range(15000, 20000);

	data->power_enabled = true;

	return 0;

reg_vdd_set:
	regulator_disable(data->vdd);
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, MMC3X30_VDD_MAX_UV);
exit:
	return rc;

}

static int mmc3x30_power_deinit(struct mmc3x30_data *data)
{
	if (!IS_ERR_OR_NULL(data->vio)) {
		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0,
					MMC3X30_VIO_MAX_UV);

		regulator_disable(data->vio);
	}

	if (!IS_ERR_OR_NULL(data->vdd)) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,
					MMC3X30_VDD_MAX_UV);

		regulator_disable(data->vdd);
	}

	data->power_enabled = false;

	return 0;
}

static int mmc3x30_power_set(struct mmc3x30_data *memsic, bool on)
{
	int rc = 0;

	if (!on && memsic->power_enabled) {
		mutex_lock(&memsic->ecompass_lock);

		rc = regulator_disable(memsic->vdd);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			goto err_vdd_disable;
		}

		rc = regulator_disable(memsic->vio);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			goto err_vio_disable;
		}
		memsic->power_enabled = false;

		mutex_unlock(&memsic->ecompass_lock);
		return rc;
	} else if (on && !memsic->power_enabled) {
		mutex_lock(&memsic->ecompass_lock);

		rc = regulator_enable(memsic->vdd);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			goto err_vdd_enable;
		}

		rc = regulator_enable(memsic->vio);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			goto err_vio_enable;
		}
		memsic->power_enabled = true;

		mutex_unlock(&memsic->ecompass_lock);

		/* The minimum time to operate after VDD valid is 10 ms */
		usleep_range(15000, 20000);

		return rc;
	} else {
		dev_warn(&memsic->i2c->dev,
				"Power on=%d. enabled=%d\n",
				on, memsic->power_enabled);
		return rc;
	}

err_vio_enable:
	regulator_disable(memsic->vio);
err_vdd_enable:
	mutex_unlock(&memsic->ecompass_lock);
	return rc;

err_vio_disable:
	if (regulator_enable(memsic->vdd))
		dev_warn(&memsic->i2c->dev, "Regulator vdd enable failed\n");
err_vdd_disable:
	mutex_unlock(&memsic->ecompass_lock);
	return rc;
}
static int mmc3x30_check_device(struct mmc3x30_data *memsic)
{
	unsigned int id[2] = {0, 0};
	unsigned int reg[2] = {MMC35XX_REG_ID, MMC36XX_REG_ID};
	int rc[2];
	int i;

	for (i = 0; i < 2; i++)
		rc[i] = regmap_read(memsic->regmap, reg[i], &id[i]);
	/* two id in the arrow */
	if (id[1] == MMC3630_DEVICE_ID)
		memsic->device_id = MMC3630_DEVICE_ID;
	else if (id[0] == MMC3530_DEVICE_ID)
		memsic->device_id = MMC3530_DEVICE_ID;
	else if (id[0] == MMC3524_DEVICE_ID)
		memsic->device_id = MMC3524_DEVICE_ID;
	else
		return -ENODEV;

	return 0;
}

static int mmc3x30_parse_dt(struct i2c_client *client,
		struct mmc3x30_data *memsic)
{
	struct device_node *np = client->dev.of_node;
	const char *tmp;
	int rc;
	int i;

	rc = of_property_read_string(np, "memsic,dir", &tmp);

	/* does not have a value or the string is not null-terminated */
	if (rc && (rc != -EINVAL)) {
		dev_err(&client->dev, "Unable to read memsic,dir\n");
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(mmc3x30_dir); i++) {
		if (strcmp(mmc3x30_dir[i], tmp) == 0)
			break;
	}

	if (i >= ARRAY_SIZE(mmc3x30_dir)) {
		dev_err(&client->dev, "Invalid memsic,dir property");
		return -EINVAL;
	}

	/*PLEASE CONFIRM WITH SENSOR PROVIDER*/
	memsic->dir = 1;

	if (of_property_read_bool(np, "memsic,auto-report"))
		memsic->auto_report = 1;
	else
		memsic->auto_report = 0;

	return 0;
}

static int mmc3x30_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int rc = 0;
	struct mmc3x30_data *memsic = container_of(sensors_cdev,
			struct mmc3x30_data, cdev);

	mutex_lock(&memsic->ops_lock);

	if (enable && (!memsic->enable)) {
		rc = mmc3x30_power_set(memsic, true);
		if (rc) {
			dev_err(&memsic->i2c->dev, "Power up failed\n");
			goto exit;
		}

		/* send TM cmd before read */
		if (memsic->device_id == MMC3524_DEVICE_ID || memsic->device_id == MMC3530_DEVICE_ID)
		{
			rc = regmap_write(memsic->regmap, MMC35XX_REG_CTRL0,
				        MMC35XX_SINGLE_TM);
			if (rc) {
				dev_err(&memsic->i2c->dev, "write reg %d failed.(%d)\n",
						MMC35XX_REG_CTRL0, rc);
				goto exit;
			}
		} else if (memsic->device_id == MMC3630_DEVICE_ID) {
			rc = regmap_write(memsic->regmap, MMC36XX_REG_CTRL0,
				        MMC36XX_SINGLE_TM);
			if (rc) {
				dev_err(&memsic->i2c->dev, "write reg %d failed.(%d)\n",
						MMC36XX_REG_CTRL0, rc);
				goto exit;
			}
		}
		memsic->timeout = jiffies;
		if (memsic->auto_report)
			schedule_delayed_work(&memsic->dwork,
				msecs_to_jiffies(memsic->poll_interval));
	} else if ((!enable) && memsic->enable) {
		if (memsic->auto_report)
			cancel_delayed_work_sync(&memsic->dwork);

		if (mmc3x30_power_set(memsic, false))
			dev_warn(&memsic->i2c->dev, "Power off failed\n");
	} else {
		dev_warn(&memsic->i2c->dev,
				"ignore enable state change from %d to %d\n",
				memsic->enable, enable);
	}
	memsic->enable = enable;

exit:
	mutex_unlock(&memsic->ops_lock);
	return rc;
}

static int mmc3x30_set_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct mmc3x30_data *memsic = container_of(sensors_cdev,
			struct mmc3x30_data, cdev);

	mutex_lock(&memsic->ops_lock);
	if (memsic->poll_interval != delay_msec)
		memsic->poll_interval = delay_msec;

	if (memsic->auto_report)
		mod_delayed_work(system_wq, &memsic->dwork,
				msecs_to_jiffies(delay_msec));
	mutex_unlock(&memsic->ops_lock);

	return 0;
}

static struct regmap_config mmc3x30_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
#ifdef INIT_SET
static int mmc3x30_device_initial(struct mmc3x30_data *memsic)
{
	int rc = -1;

	// Do SET operation
	if (memsic->device_id == MMC3524_DEVICE_ID || memsic->device_id == MMC3530_DEVICE_ID)
	{
		rc = regmap_write(mmc3x30_data_struct->regmap, MMC35XX_REG_CTRL0,
				              MMC35XX_SET);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
					          MMC35XX_REG_CTRL0, __LINE__, rc);

		    return -EFAULT;
		}

	} else if (memsic->device_id == MMC3630_DEVICE_ID) {
		rc = regmap_write(mmc3x30_data_struct->regmap, MMC36XX_REG_CTRL0,
				              MMC36XX_SET);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
					          MMC36XX_REG_CTRL0, __LINE__, rc);

		    return -EFAULT;
		}
	}

	msleep(20);/* setting need 20ms */

	return 0;
}
#endif
static int mmc3x30_open(struct inode *inode, struct file *file)
{

	return nonseekable_open(inode, file);
}

static int mmc3x30_release(struct inode *inode, struct file *file)
{

	return 0;
}

static long mmc3x30_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned char data[16] = {0};
	unsigned char reg_addr;
	unsigned char reg_num;
	unsigned int reg_value;
	int rc = -1;

	switch (cmd) {
		case MMC3X30_IOC_SET:
			if (mmc3x30_data_struct->device_id == MMC3524_DEVICE_ID || mmc3x30_data_struct->device_id == MMC3530_DEVICE_ID)
			{
				rc = regmap_write(mmc3x30_data_struct->regmap, MMC35XX_REG_CTRL0,
							              MMC35XX_SET);
				if (rc) {
					dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
								           MMC35XX_REG_CTRL0, __LINE__, rc);
					return -EFAULT;
				}

			} else if (mmc3x30_data_struct->device_id == MMC3630_DEVICE_ID) {
				rc = regmap_write(mmc3x30_data_struct->regmap, MMC36XX_REG_CTRL0,
							              MMC36XX_SET);
				if (rc) {
					dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
								           MMC36XX_REG_CTRL0, __LINE__, rc);
					return -EFAULT;
				}

			}
			msleep(20);/* setting need 20ms */
			break;

		case MMC3X30_IOC_RESET:
			if (mmc3x30_data_struct->device_id == MMC3524_DEVICE_ID || mmc3x30_data_struct->device_id == MMC3530_DEVICE_ID)
			{
				rc = regmap_write(mmc3x30_data_struct->regmap, MMC35XX_REG_CTRL0,
							              MMC35XX_RESET);
				if (rc) {
					dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
								           MMC35XX_REG_CTRL0, __LINE__, rc);
					return -EFAULT;
				}

			} else if (mmc3x30_data_struct->device_id == MMC3630_DEVICE_ID) {
				rc = regmap_write(mmc3x30_data_struct->regmap, MMC36XX_REG_CTRL0,
							              MMC36XX_RESET);
				if (rc) {
					dev_err(&mmc3x30_data_struct->i2c->dev, "write reg %d failed at %d.(%d)\n",
								           MMC36XX_REG_CTRL0, __LINE__, rc);
					return -EFAULT;
				}

			}
			msleep(20);/* setting need 20ms */
			break;

	 	case MMC3X30_IOC_READ_REG:
			if (copy_from_user(&reg_addr, pa, sizeof(reg_addr)))
				return -EFAULT;
			rc = regmap_read(mmc3x30_data_struct->regmap, reg_addr, &reg_value);
			if (rc) {
				dev_err(&mmc3x30_data_struct->i2c->dev,
					 "read reg %d failed at %d.(%d)\n",
						      reg_addr, __LINE__, rc);
			}
			pr_info("mmc3x30 Read register 0x%0x\n", reg_addr);
			if (copy_to_user(pa, &reg_value, sizeof(reg_value))) {
				return -EFAULT;
			}
			break;

	 	case MMC3X30_IOC_WRITE_REG:
			if (copy_from_user(&data, pa, sizeof(data)))
				return -EFAULT;

			rc = regmap_write(mmc3x30_data_struct->regmap,
				data[1], data[0]);
			if (rc) {
				dev_err(&mmc3x30_data_struct->i2c->dev,
					"write reg %d failed at %d.(%d)\n",
					data[1], __LINE__, rc);
				return -EFAULT;
			}
			pr_info("mmc3x30 Write '0x%0x' to register 0x%0x\n",
				 data[0], data[1]);
			break;

	 	case MMC3X30_IOC_READ_REGS:
			if (copy_from_user(&data, pa, sizeof(data)))
				return -EFAULT;
			reg_addr = data[0];
			reg_num = data[1];

			rc = regmap_bulk_read(mmc3x30_data_struct->regmap,
					 reg_addr, data, reg_num);
			if (rc) {
				dev_err(&mmc3x30_data_struct->i2c->dev,
						"read reg %d failed at %d.(%d)\n",
						reg_addr, __LINE__, rc);
			}
			pr_info("mmc3x30 data: %x %x %x \n%x %x %x\n",
				data[0], data[1], data[2],
				data[3], data[4], data[5]);
			if (copy_to_user(pa, data, sizeof(data))) {
				return -EFAULT;
			}
			break;

		default:
			dev_dbg(&mmc3x30_data_struct->i2c->dev, "no ctl cmd !\n");
			return -EFAULT;
			//break;
	}

	return 0;

}
#ifdef CONFIG_COMPAT
static long mmc3x30_compat_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	void __user *arg64 = compat_ptr(arg);
        int ret =-1;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
	{
		dev_err(&mmc3x30_data_struct->i2c->dev,
			 "file->f_op OR file->f_op->unlocked_ioctl is null!\n");
		return -EFAULT;
	}
	switch(cmd)
	{
		case COMPAT_MMC3X30_IOC_SET:
			ret = file->f_op->unlocked_ioctl(file,
				 MMC3X30_IOC_SET, (unsigned long)arg64);
			if (ret < 0)
			{
				dev_err(&mmc3x30_data_struct->i2c->dev,
				 "COMPAT_MMC3X30_IOC_SET is failed!\n");
				return -EFAULT;
			}
			break;
		case COMPAT_MMC3X30_IOC_RESET:
			ret = file->f_op->unlocked_ioctl(file,
				 MMC3X30_IOC_RESET, (unsigned long)arg64);
			if (ret < 0)
			{
				dev_err(&mmc3x30_data_struct->i2c->dev,
				 "COMPAT_MMC3X30_IOC_RESET is failed!\n");
				return -EFAULT;
			}
			break;
		case COMPAT_MMC3X30_IOC_READ_REG:
			ret = file->f_op->unlocked_ioctl(file,
				COMPAT_MMC3X30_IOC_READ_REG,
				 (unsigned long)arg64);
			if (ret < 0)
			{
				dev_err(&mmc3x30_data_struct->i2c->dev,
				 "COMPAT_MMC3X30_IOC_READ_REG is failed!\n");
				return -EFAULT;
			}
			break;
		case COMPAT_MMC3X30_IOC_WRITE_REG:
			ret = file->f_op->unlocked_ioctl(file,
				 COMPAT_MMC3X30_IOC_WRITE_REG,
				 (unsigned long)arg64);
			if (ret < 0)
			{
				dev_err(&mmc3x30_data_struct->i2c->dev,
				 "COMPAT_MMC3X30_IOC_WRITE_REG is failed!\n");
				return -EFAULT;
			}
			break;
		case COMPAT_MMC3X30_IOC_READ_REGS:
			ret = file->f_op->unlocked_ioctl(file,
				 COMPAT_MMC3X30_IOC_READ_REGS,
				 (unsigned long)arg64);
			if (ret < 0)
			{
				dev_err(&mmc3x30_data_struct->i2c->dev,
				 "COMPAT_MMC3X30_IOC_READ_REGS is failed!\n");
				return -EFAULT;
			}
			break;
		default:
			dev_dbg(&mmc3x30_data_struct->i2c->dev,
				 "no compat ctl cmd !\n");
			return -EFAULT;
			//break;
	}
	return 0;

}

#endif

static struct file_operations mmc3x30_fops = {
	.owner		= THIS_MODULE,
	.open		= mmc3x30_open,
	.release	= mmc3x30_release,
	.unlocked_ioctl = mmc3x30_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmc3x30_compat_ioctl,
#endif
};

static struct miscdevice mmc3x30_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MMC3X30_I2C_NAME,
	.fops = &mmc3x30_fops,
};
static ssize_t mmc3x30_otp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char data[10] = {0};
	unsigned char reg_addr;
	unsigned char reg_num;
	int rc = -1;

	if (mmc3x30_data_struct->device_id == MMC3524_DEVICE_ID
		 || mmc3x30_data_struct->device_id == MMC3530_DEVICE_ID)
	{
		reg_addr = MMC35XX_REG_OTP;
		reg_num = 4;
		rc = regmap_bulk_read(mmc3x30_data_struct->regmap,
					 reg_addr, data, reg_num);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "read reg %d failed at %d.(%d)\n",
				reg_addr, __LINE__, rc);
		}
		return snprintf(buf, PAGE_SIZE, "%x,%x,%x,%x\n",
				 data[0], data[1], data[2], data[3]);

	} else if (mmc3x30_data_struct->device_id == MMC3630_DEVICE_ID) {
		reg_addr = MMC36XX_REG_OTP_GATE0;
		data[0] = MMC36XX_OTP_OPEN0;
		rc = regmap_write(mmc3x30_data_struct->regmap,
				 reg_addr, data[0]);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				 reg_addr, __LINE__, rc);
		}
		reg_addr = MMC36XX_REG_OTP_GATE1;
		data[0] = MMC36XX_OTP_OPEN1;
		rc = regmap_write(mmc3x30_data_struct->regmap,
				 reg_addr, data[0]);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				 reg_addr, __LINE__, rc);
		}
		reg_addr = MMC36XX_REG_OTP_GATE2;
		data[0] = MMC36XX_OTP_OPEN2;
		rc = regmap_write(mmc3x30_data_struct->regmap,
				 reg_addr, data[0]);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				 reg_addr, __LINE__, rc);
		}
		reg_addr = MMC36XX_REG_CTRL2;
		data[0] = MMC36XX_OTP_OPEN2;
		rc = regmap_write(mmc3x30_data_struct->regmap,
				 reg_addr, data[0]);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				 reg_addr, __LINE__, rc);
		}

		reg_addr = MMC36XX_REG_OTP;
		reg_num = 2;
		rc = regmap_bulk_read(mmc3x30_data_struct->regmap,
				 reg_addr, data, reg_num);
		if (rc) {
			dev_err(&mmc3x30_data_struct->i2c->dev,
				 "read reg %d failed at %d.(%d)\n",
				 reg_addr, __LINE__, rc);
		}
		return snprintf(buf, PAGE_SIZE, "%x,%x\n", data[0], data[1]);
	}
	else
	{
		dev_err(&mmc3x30_data_struct->i2c->dev, "No otp !\n");
	    return -EFAULT;
	}
}

static ssize_t mmc3x30_value_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	int ret;
	struct mmc3x30_vec vec;
	struct mmc3x30_data *memsic = dev_get_drvdata(dev);

	vec.x = vec.y = vec.z = 0;

	ret = mmc3x30_read_xyz(memsic, &vec);
	if (ret) {
		dev_warn(&memsic->i2c->dev, "read xyz failed\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", vec.x, vec.y, vec.z);
}
static ssize_t mmc3x30_set_store(struct device *dev,
		 struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int rc = -1;
	struct mmc3x30_data *memsic = dev_get_drvdata(dev);

	if (memsic->device_id == MMC3524_DEVICE_ID
	 || memsic->device_id == MMC3530_DEVICE_ID)
	{
		rc = regmap_write(memsic->regmap, MMC35XX_REG_CTRL0,
					MMC35XX_SET);
		if (rc) {
			dev_err(&memsic->i2c->dev,
			 "write reg %d failed at %d.(%d)\n",
			 MMC35XX_REG_CTRL0, __LINE__, rc);
			return -EFAULT;
		}

	} else if (memsic->device_id == MMC3630_DEVICE_ID) {
		rc = regmap_write(memsic->regmap, MMC36XX_REG_CTRL0,
					MMC36XX_SET);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				 MMC36XX_REG_CTRL0, __LINE__, rc);
			return -EFAULT;
		}

	}
	msleep(20);/* setting time */
	return count;/* nothing return */
}

static ssize_t mmc3x30_reset_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{

	int rc = -1;
	struct mmc3x30_data *memsic = dev_get_drvdata(dev);

	if (memsic->device_id == MMC3524_DEVICE_ID
	 || memsic->device_id == MMC3530_DEVICE_ID)
	{
		rc = regmap_write(memsic->regmap, MMC35XX_REG_CTRL0,
					 MMC35XX_RESET);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				MMC35XX_REG_CTRL0, __LINE__, rc);
			return -EFAULT;
		}

	} else if (memsic->device_id == MMC3630_DEVICE_ID) {
		rc = regmap_write(memsic->regmap, MMC36XX_REG_CTRL0,
					              MMC36XX_RESET);
		if (rc) {
			dev_err(&memsic->i2c->dev,
				 "write reg %d failed at %d.(%d)\n",
				MMC36XX_REG_CTRL0, __LINE__, rc);
			return -EFAULT;
		}

	}
	msleep(20);/* setting time */
	return count;/* nothing return */
}

/* add to create sysfs file node */
static DEVICE_ATTR(otp, S_IRUSR|S_IRGRP, mmc3x30_otp_show, NULL);
static DEVICE_ATTR(value, S_IRUSR|S_IRGRP, mmc3x30_value_show, NULL);
static DEVICE_ATTR(set, S_IWUSR, NULL, mmc3x30_set_store);
static DEVICE_ATTR(reset, S_IWUSR, NULL, mmc3x30_reset_store);

static struct attribute *mmc3x30_attributes[] = {
		&dev_attr_otp.attr,
		&dev_attr_value.attr,
		&dev_attr_set.attr,
		&dev_attr_reset.attr,
		NULL,
};

static const struct attribute_group mmc3x30_attr_group = {
		.attrs = mmc3x30_attributes,
};


static int mmc3x30_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int res = 0;
	struct mmc3x30_data *memsic;

	dev_info(&client->dev, "probing mmc3x30\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("mmc3x30 i2c functionality check failed.\n");
		res = -ENODEV;
		goto out;
	}

	memsic = devm_kzalloc(&client->dev, sizeof(struct mmc3x30_data),
			GFP_KERNEL);
	if (!memsic) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out;
	}

	mmc3x30_data_struct = memsic;

	if (client->dev.of_node) {
		res = mmc3x30_parse_dt(client, memsic);
		if (res) {
			dev_err(&client->dev,
				"Unable to parse platform data.(%d)", res);
			goto out;
		}
	} else {
		memsic->dir = 1;
		memsic->auto_report = 1;
	}

	memsic->i2c = client;
	dev_set_drvdata(&client->dev, memsic);

	mutex_init(&memsic->ecompass_lock);
	mutex_init(&memsic->ops_lock);

	memsic->regmap = devm_regmap_init_i2c(client, &mmc3x30_regmap_config);
	if (IS_ERR(memsic->regmap)) {
		dev_err(&client->dev, "Init regmap failed.(%ld)",
				PTR_ERR(memsic->regmap));
		res = PTR_ERR(memsic->regmap);
		goto out;
	}

	res = mmc3x30_power_init(memsic);
	if (res) {
		dev_err(&client->dev, "Power up mmc3x30 failed\n");
		goto out;
	}

	res = mmc3x30_check_device(memsic);
	if (res) {
		dev_err(&client->dev, "Check device failed\n");
		goto out_check_device;
	}

	memsic->idev = mmc3x30_init_input(client);
	if (!memsic->idev) {
		dev_err(&client->dev, "init input device failed\n");
		res = -ENODEV;
		goto out_init_input;
	}


	if (memsic->auto_report) {
		dev_info(&client->dev, "auto report is enabled\n");
		INIT_DELAYED_WORK(&memsic->dwork, mmc3x30_poll);
	}

	memsic->cdev = sensors_cdev;
	memsic->cdev.sensors_enable = mmc3x30_set_enable;
	memsic->cdev.sensors_poll_delay = mmc3x30_set_poll_delay;
	res = sensors_classdev_register(&client->dev, &memsic->cdev);
	if (res) {
		dev_err(&client->dev, "sensors class register failed.\n");
		goto out_register_classdev;
	}
#ifdef INIT_SET
	res = mmc3x30_device_initial(memsic);
	if (res) {
		pr_err("mmc3x30_device initial failed\n");
	}
	else{
		pr_info("mmc3x30_device initial OK\n");
	}
#endif

	res = misc_register(&mmc3x30_device);
	if (res) {
		pr_err("%s: mmc3x30_device register failed\n", __FUNCTION__);
		goto out_deregister;
	}


	res = mmc3x30_power_set(memsic, false);
	if (res) {
		dev_err(&client->dev, "Power off failed\n");
		goto out_power_set;
	}

	memsic->poll_interval = MMC3X30_DEFAULT_INTERVAL_MS;

	/* create sysfs group */
	res = sysfs_create_group(&client->dev.kobj, &mmc3x30_attr_group);
	if (res) {
		res = -EROFS;
		dev_err(&client->dev,"Unable to creat sysfs group\n");
	}

	dev_info(&client->dev, "mmc3x30 successfully probed\n");

	return 0;

out_power_set:
	sensors_classdev_unregister(&memsic->cdev);
out_deregister:
	misc_deregister(&mmc3x30_device);
out_register_classdev:
	input_unregister_device(memsic->idev);
out_init_input:
out_check_device:
	mmc3x30_power_deinit(memsic);
out:
	return res;
}

static int mmc3x30_remove(struct i2c_client *client)
{
	struct mmc3x30_data *memsic = dev_get_drvdata(&client->dev);

	sensors_classdev_unregister(&memsic->cdev);
	misc_deregister(&mmc3x30_device);
	mmc3x30_power_deinit(memsic);

	if (memsic->idev)
		input_unregister_device(memsic->idev);

	sysfs_remove_group(&client->dev.kobj, &mmc3x30_attr_group);

	return 0;
}

static int mmc3x30_suspend(struct device *dev)
{
	int res = 0;
	struct mmc3x30_data *memsic = dev_get_drvdata(dev);

	dev_dbg(dev, "suspended\n");

	if (memsic->enable) {
		if (memsic->auto_report)
			cancel_delayed_work_sync(&memsic->dwork);

		res = mmc3x30_power_set(memsic, false);
		if (res) {
			dev_err(dev, "failed to suspend mmc3x30\n");
			goto exit;
		}
	}
exit:
	return res;
}

static int mmc3x30_resume(struct device *dev)
{
	int res = 0;
	struct mmc3x30_data *memsic = dev_get_drvdata(dev);

	dev_dbg(dev, "resumed\n");

	if (!memsic->enable) {
		res = mmc3x30_power_set(memsic, true);
		if (res) {
			dev_err(&memsic->i2c->dev, "Power enable failed\n");
			goto exit;
		}

		if (memsic->auto_report)
			schedule_delayed_work(&memsic->dwork,
				msecs_to_jiffies(memsic->poll_interval));
	}

exit:
	return res;
}

static const struct i2c_device_id mmc3x30_id[] = {
	{ MMC3X30_I2C_NAME, 0 },
	{ }
};

static struct of_device_id mmc3x30_match_table[] = {
	{ .compatible = "memsic,mmc3x30", },
	{ },
};

static const struct dev_pm_ops mmc3x30_pm_ops = {
	.suspend = mmc3x30_suspend,
	.resume = mmc3x30_resume,
};

static struct i2c_driver mmc3x30_driver = {
	.probe 		= mmc3x30_probe,
	.remove 	= mmc3x30_remove,
	.id_table	= mmc3x30_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MMC3X30_I2C_NAME,
		.of_match_table = mmc3x30_match_table,
		.pm = &mmc3x30_pm_ops,
	},
};

static int __init mmc3x30_init(void)
{
	return i2c_add_driver(&mmc3x30_driver);
}

static void __exit mmc3x30_exit(void)
{
	i2c_del_driver(&mmc3x30_driver);
}

//module_i2c_driver(mmc3x30_driver);
late_initcall(mmc3x30_init);
module_exit(mmc3x30_exit);

MODULE_DESCRIPTION("MEMSIC MMC3X30 Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

