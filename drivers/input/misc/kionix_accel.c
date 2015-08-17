/* drivers/input/misc/kionix_accel.c - Kionix accelerometer driver
 *
 * Copyright (C) 2012-2015 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input/kionix_accel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sensors.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

/* Debug Message Flags */
#define KIONIX_KMSG_ERR	1	/* Print kernel debug message for error */
#define KIONIX_KMSG_INF	1	/* Print kernel debug message for info */

#if KIONIX_KMSG_ERR
#define KMSGERR(format, ...)	\
		dev_err(format, ## __VA_ARGS__)
#else
#define KMSGERR(format, ...)
#endif

#if KIONIX_KMSG_INF
#define KMSGINF(format, ...)	\
		dev_info(format, ## __VA_ARGS__)
#else
#define KMSGINF(format, ...)
#endif

/******************************************************************************
 * Accelerometer WHO_AM_I return value
 *****************************************************************************/
#define KIONIX_ACCEL_WHO_AM_I_KXTE9			0x00
#define KIONIX_ACCEL_WHO_AM_I_KXTF9			0x01
#define KIONIX_ACCEL_WHO_AM_I_KXTI9_1001	0x04
#define KIONIX_ACCEL_WHO_AM_I_KXTIK_1004	0x05
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005	0x07
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007	0x08
#define KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008	0x0A
#define KIONIX_ACCEL_WHO_AM_I_KXTJ2_1029 0x09
#define KIONIX_ACCEL_WHO_AM_I_KXCJK_1013	0x11



/******************************************************************************
 * Accelerometer Grouping
 *****************************************************************************/
#define KIONIX_ACCEL_GRP1	1	/* KXTE9 */
#define KIONIX_ACCEL_GRP2	2	/* KXTF9/I9-1001/J9-1005 */
#define KIONIX_ACCEL_GRP3	3	/* KXTIK-1004 */
#define KIONIX_ACCEL_GRP4	4	/* KXTJ9-1007/KXCJ9-1008 */
#define KIONIX_ACCEL_GRP5	5	/* KXTJ2-1009  KXTJ2-1029   */
#define KIONIX_ACCEL_GRP6	6	/* KXCJK-1013 */

/******************************************************************************
 * Registers for Accelerometer Group 1 & 2 & 3
 *****************************************************************************/
#define ACCEL_WHO_AM_I		0x0F

/*****************************************************************************/
/* Registers for Accelerometer Group 1 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP1_XOUT			0x12
/* Control Registers */
#define ACCEL_GRP1_CTRL_REG1	0x1B
/* CTRL_REG1 */
#define ACCEL_GRP1_PC1_OFF		0x7F
#define ACCEL_GRP1_PC1_ON		(1 << 7)
#define ACCEL_GRP1_ODR40		(3 << 3)
#define ACCEL_GRP1_ODR10		(2 << 3)
#define ACCEL_GRP1_ODR3			(1 << 3)
#define ACCEL_GRP1_ODR1			(0 << 3)
#define ACCEL_GRP1_ODR_MASK		(3 << 3)

/*****************************************************************************/
/* Registers for Accelerometer Group 2 & 3 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP2_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP2_INT_REL		0x1A
#define ACCEL_GRP2_CTRL_REG1	0x1B
#define ACCEL_GRP2_INT_CTRL1	0x1E
#define ACCEL_GRP2_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP2_PC1_OFF		0x7F
#define ACCEL_GRP2_PC1_ON		(1 << 7)
#define ACCEL_GRP2_DRDYE		(1 << 5)
#define ACCEL_GRP2_G_8G			(2 << 3)
#define ACCEL_GRP2_G_4G			(1 << 3)
#define ACCEL_GRP2_G_2G			(0 << 3)
#define ACCEL_GRP2_G_MASK		(3 << 3)
#define ACCEL_GRP2_RES_8BIT		(0 << 6)
#define ACCEL_GRP2_RES_12BIT	(1 << 6)
#define ACCEL_GRP2_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP2_IEA			(1 << 4)
#define ACCEL_GRP2_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP2_ODR12_5		0x00
#define ACCEL_GRP2_ODR25		0x01
#define ACCEL_GRP2_ODR50		0x02
#define ACCEL_GRP2_ODR100		0x03
#define ACCEL_GRP2_ODR200		0x04
#define ACCEL_GRP2_ODR400		0x05
#define ACCEL_GRP2_ODR800		0x06
/*****************************************************************************/

/*****************************************************************************/
/* Registers for Accelerometer Group 4 & 5 & 6 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP4_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP4_INT_REL		0x1A
#define ACCEL_GRP4_CTRL_REG1	0x1B
#define ACCEL_GRP4_INT_CTRL1	0x1E
#define ACCEL_GRP4_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP4_PC1_OFF		0x7F
#define ACCEL_GRP4_PC1_ON		(1 << 7)
#define ACCEL_GRP4_DRDYE		(1 << 5)
#define ACCEL_GRP4_G_8G			(2 << 3)
#define ACCEL_GRP4_G_4G			(1 << 3)
#define ACCEL_GRP4_G_2G			(0 << 3)
#define ACCEL_GRP4_G_MASK		(3 << 3)
#define ACCEL_GRP4_RES_8BIT		(0 << 6)
#define ACCEL_GRP4_RES_12BIT	(1 << 6)
#define ACCEL_GRP4_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP4_IEA			(1 << 4)
#define ACCEL_GRP4_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP4_ODR0_781		0x08
#define ACCEL_GRP4_ODR1_563		0x09
#define ACCEL_GRP4_ODR3_125		0x0A
#define ACCEL_GRP4_ODR6_25		0x0B
#define ACCEL_GRP4_ODR12_5		0x00
#define ACCEL_GRP4_ODR25		0x01
#define ACCEL_GRP4_ODR50		0x02
#define ACCEL_GRP4_ODR100		0x03
#define ACCEL_GRP4_ODR200		0x04
#define ACCEL_GRP4_ODR400		0x05
#define ACCEL_GRP4_ODR800		0x06
#define ACCEL_GRP4_ODR1600		0x07
/*****************************************************************************/
/* Input Event Constants */
#define ACCEL_G_MAX			8096
#define ACCEL_FUZZ			3
#define ACCEL_FLAT			3
/* I2C Retry Constants */
/* Number of times to retry i2c */
#define KIONIX_I2C_RETRY_COUNT		10
/* Timeout between retry (miliseconds) */
#define KIONIX_I2C_RETRY_TIMEOUT	1

/* Earlysuspend Contants */
/* Timeout (miliseconds) */
#define KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT	5000

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate (ODR).
 */
static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp1_odr_table[] = {
	{
	100, ACCEL_GRP1_ODR40}, {
	334, ACCEL_GRP1_ODR10}, {
	1000, ACCEL_GRP1_ODR3}, {
0, ACCEL_GRP1_ODR1},};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp2_odr_table[] = {
	{
	3, ACCEL_GRP2_ODR800}, {
	5, ACCEL_GRP2_ODR400}, {
	10, ACCEL_GRP2_ODR200}, {
	20, ACCEL_GRP2_ODR100}, {
	40, ACCEL_GRP2_ODR50}, {
	80, ACCEL_GRP2_ODR25}, {
0, ACCEL_GRP2_ODR12_5},};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp4_odr_table[] = {
	{
	2, ACCEL_GRP4_ODR1600}, {
	3, ACCEL_GRP4_ODR800}, {
	5, ACCEL_GRP4_ODR400}, {
	10, ACCEL_GRP4_ODR200}, {
	20, ACCEL_GRP4_ODR100}, {
	40, ACCEL_GRP4_ODR50}, {
	80, ACCEL_GRP4_ODR25}, {
	160, ACCEL_GRP4_ODR12_5}, {
	320, ACCEL_GRP4_ODR6_25}, {
	640, ACCEL_GRP4_ODR3_125}, {
	1280, ACCEL_GRP4_ODR1_563}, {
0, ACCEL_GRP4_ODR0_781},};

enum {
	accel_grp1_ctrl_reg1 = 0,
	accel_grp1_regs_count,
};

enum {
	accel_grp2_ctrl_reg1 = 0,
	accel_grp2_data_ctrl,
	accel_grp2_int_ctrl,
	accel_grp2_regs_count,
};

enum {
	accel_grp4_ctrl_reg1 = 0,
	accel_grp4_data_ctrl,
	accel_grp4_int_ctrl,
	accel_grp4_regs_count,
};

struct kionix_accel_driver {
	/* regulator data */
	bool power_on;
	struct regulator *vdd;
	struct regulator *vio;

	struct i2c_client *client;
	struct sensors_classdev cdev;
	struct kionix_accel_platform_data accel_pdata;
	struct input_dev *input_dev;
	struct delayed_work accel_work;
	struct workqueue_struct *accel_workqueue;
	wait_queue_head_t wqh_suspend;

	int accel_data[3];
	int accel_cali[3];
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	bool negate_x;
	bool negate_y;
	bool negate_z;
	u8 shift;

	atomic_t delay;

	unsigned int poll_interval;
	unsigned int poll_delay;
	unsigned int accel_group;
	u8 *accel_registers;

	atomic_t accel_suspended;
	atomic_t accel_suspend_continue;
	atomic_t accel_enabled;
	atomic_t accel_input_event;
	atomic_t accel_enable_resume;
	struct mutex mutex_earlysuspend;
	struct mutex mutex_resume;
	rwlock_t rwlock_accel_data;

	bool accel_drdy;

	/* Function callback */
	void (*kionix_accel_report_accel_data)(struct kionix_accel_driver
						*acceld);
	int (*kionix_accel_update_odr)(struct kionix_accel_driver *acceld,
					unsigned int poll_interval);
	int (*kionix_accel_power_on_init)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_operate)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_standby)(struct kionix_accel_driver *acceld);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif				/* CONFIG_HAS_EARLYSUSPEND */
};

/*
 * Global data
 */
static struct kionix_accel_driver *kionix_data;

static struct sensors_classdev sensors_cdev = {
	.name = "kionix-accel",
	.vendor = "kionix",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",	/* 16g */
	.resolution = "0.156",	/* 15.63mg */
	.sensor_power = "0.13",	/* typical value */
	.min_delay = POLL_INTERVAL_MIN_MS * 1000,	/* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = POLL_DEFAULT_INTERVAL_MS,	/* in millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int kionix_i2c_read(struct i2c_client *client,
			   u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
		 .addr = client->addr,
		 .flags = client->flags,
		 .len = 1,
		 .buf = &addr,
		 },
		{
		 .addr = client->addr,
		 .flags = client->flags | I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	return i2c_transfer(client->adapter, msgs, 2);
}

static int kionix_strtok(const char *buf,
			 size_t count, char **token, const int token_nr)
{
	char *buf2 = kzalloc((count + 1) * sizeof(char), GFP_KERNEL);
	char **token2 = token;
	unsigned int num_ptr = 0, num_nr = 0, num_neg = 0;
	int i = 0, start = 0, end = (int)count;

	strlcpy(buf2, buf, sizeof(buf2));

	while ((start < end) && (i < token_nr)) {
		/* We found a negative sign */
		if (*(buf2 + start) == '-') {
			/* Previous char(s) are numeric, so
			we store their value first before proceed */
			if (num_nr > 0) {
				/* If there is a pending negative sign,
				we adjust the variables to account for it */
				if (num_neg) {
					num_ptr--;
					num_nr++;
				}
				*token2 =
				    kzalloc((num_nr + 2) * sizeof(char),
						    GFP_KERNEL);
				strlcpy(*token2, (const char *)(buf2 + num_ptr),
					(size_t) num_nr);
				*(*token2 + num_nr) = '\n';
				i++;
				token2++;
				/* Reset */
				num_ptr = num_nr = 0;
			}
			/* This indicates that there is a pending
			negative sign in the string */
			num_neg = 1;
		}
		/* We found a numeric */
		else if ((*(buf2 + start) >= '0') && (*(buf2 + start) <= '9')) {
			/* If the previous char(s) are not numeric,
			set num_ptr to current char */
			if (num_nr < 1)
				num_ptr = start;
			num_nr++;
		}
		/* We found an unwanted character */
		else {
			/* Previous char(s) are numeric, so we
			store their value first before proceed */
			if (num_nr > 0) {
				if (num_neg) {
					num_ptr--;
					num_nr++;
				}
				*token2 =
				    kzalloc((num_nr + 2) * sizeof(char),
						    GFP_KERNEL);
				strlcpy(*token2, (const char *)(buf2 + num_ptr),
					(size_t) num_nr);
				*(*token2 + num_nr) = '\n';
				i++;
				token2++;
			}
			/* Reset all the variables to start afresh */
			num_ptr = num_nr = num_neg = 0;
		}
		start++;
	}

	kfree(buf2);

	return (i == token_nr) ? token_nr : -1;
}

static int kionix_accel_grp1_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP1_CTRL_REG1,
						acceld->accel_registers
						[accel_grp1_ctrl_reg1] |
						ACCEL_GRP1_PC1_ON);
		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP1_CTRL_REG1,
						acceld->accel_registers
						[accel_grp1_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp1_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1,
					acceld->accel_registers
					[accel_grp2_ctrl_reg1] |
					ACCEL_GRP1_PC1_ON);
	if (err < 0)
		return err;

	queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return 0;
}

static int kionix_accel_grp1_standby(struct kionix_accel_driver *acceld)
{
	int err;

	cancel_delayed_work_sync(&acceld->accel_work);

	err =
	    i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp1_report_accel_data(struct kionix_accel_driver
						*acceld)
{
	u8 accel_data[3];
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop = KIONIX_I2C_RETRY_COUNT;

	if (atomic_read(&acceld->accel_enabled) > 0) {
		if (atomic_read(&acceld->accel_enable_resume) > 0) {
			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
				    kionix_i2c_read(acceld->client,
						    ACCEL_GRP1_XOUT, accel_data,
						    6);
				mutex_unlock(&input_dev->mutex);
				if (err < 0) {
					loop--;
					msleep(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev,
					"%s: read data output error = %d\n",
					__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);

				x = ((s16)
				     le16_to_cpu(((s16)
						  (accel_data
						   [acceld->axis_map_x] >> 2)) -
						 32)) << 6;
				y = ((s16)
				     le16_to_cpu(((s16)
						  (accel_data
						   [acceld->axis_map_y] >> 2)) -
						 32)) << 6;
				z = ((s16)
				     le16_to_cpu(((s16)
						  (accel_data
						   [acceld->axis_map_z] >> 2)) -
						 32)) << 6;

				acceld->accel_data[acceld->axis_map_x] =
				    (acceld->negate_x ? -x : x) +
				    acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
				    (acceld->negate_y ? -y : y) +
				    acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
				    (acceld->negate_z ? -z : z) +
				    acceld->accel_cali[acceld->axis_map_z];

				if (atomic_read(&acceld->accel_input_event)
					> 0) {
					input_report_abs(acceld->input_dev,
							 ABS_X,
							 acceld->accel_data
							 [acceld->axis_map_x]);
					input_report_abs(acceld->input_dev,
							 ABS_Y,
							 acceld->accel_data
							 [acceld->axis_map_y]);
					input_report_abs(acceld->input_dev,
							 ABS_Z,
							 acceld->accel_data
							 [acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}

				write_unlock(&acceld->rwlock_accel_data);
			}
		} else
			atomic_inc(&acceld->accel_enable_resume);
	}
}

static int kionix_accel_grp1_update_odr(struct kionix_accel_driver *acceld,
					unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp1_odr_table); i++) {
		odr = kionix_accel_grp1_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp1_odr_table[i].cutoff)
			break;
	}

/* Do not need to update CTRL_REG1 register if the ODR is not changed */
	if ((acceld->accel_registers[accel_grp1_ctrl_reg1] &
	     ACCEL_GRP1_ODR_MASK) == odr)
		return 0;
	else {
		acceld->accel_registers[accel_grp1_ctrl_reg1] &=
		    ~ACCEL_GRP1_ODR_MASK;
		acceld->accel_registers[accel_grp1_ctrl_reg1] |= odr;
	}

/* Do not need to update CTRL_REG1 register if
the sensor is not currently turn on */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP1_CTRL_REG1,
					      acceld->accel_registers
					      [accel_grp1_ctrl_reg1] |
					      ACCEL_GRP1_PC1_ON);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP2_CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP2_DATA_CTRL,
					acceld->accel_registers
					[accel_grp2_data_ctrl]);
	if (err < 0)
		return err;

/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_INT_CTRL1,
						acceld->accel_registers
						[accel_grp2_int_ctrl]);
		if (err < 0)
			return err;
	}

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_CTRL_REG1,
						acceld->accel_registers
						[accel_grp2_ctrl_reg1] |
						ACCEL_GRP2_PC1_ON);
		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP2_CTRL_REG1,
						acceld->accel_registers
						[accel_grp2_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1,
					acceld->accel_registers
					[accel_grp2_ctrl_reg1] |
					ACCEL_GRP2_PC1_ON);
	if (err < 0)
		return err;

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
				   0);

	return 0;
}

static int kionix_accel_grp2_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if (acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err =
	    i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp2_report_accel_data(struct kionix_accel_driver
						*acceld)
{
	struct {
		union {
			s16 accel_data_s16[3];
			s8 accel_data_s8[6];
		};
	} accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;

	/* Only read the output registers if enabled */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		if (atomic_read(&acceld->accel_enable_resume) > 0) {
			loop = KIONIX_I2C_RETRY_COUNT;
			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
				    kionix_i2c_read(acceld->client,
						    ACCEL_GRP2_XOUT_L, (u8 *)
						    accel_data.accel_data_s16,
						    6);
				mutex_unlock(&input_dev->mutex);
				if (err < 0) {
					loop--;
					msleep(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev,
					"%s: read data output error = %d\n",
					__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);

				x = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_x])) >>
				    acceld->shift;
				y = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_y])) >>
				    acceld->shift;
				z = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_z])) >>
				    acceld->shift;

				acceld->accel_data[acceld->axis_map_x] =
				    (acceld->negate_x ? -x : x) +
				    acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
				    (acceld->negate_y ? -y : y) +
				    acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
				    (acceld->negate_z ? -z : z) +
				    acceld->accel_cali[acceld->axis_map_z];

				if (atomic_read(&acceld->accel_input_event)
					> 0) {
					input_report_abs(acceld->input_dev,
							 ABS_X,
							 acceld->accel_data
							 [acceld->axis_map_x]);
					input_report_abs(acceld->input_dev,
							 ABS_Y,
							 acceld->accel_data
							 [acceld->axis_map_y]);
					input_report_abs(acceld->input_dev,
							 ABS_Z,
							 acceld->accel_data
							 [acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}

				write_unlock(&acceld->rwlock_accel_data);
			}
		} else
			atomic_inc(&acceld->accel_enable_resume);
	}

	/* Clear the interrupt if using drdy */
	if (acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;
		while (loop) {
			err =
			    i2c_smbus_read_byte_data(acceld->client,
						     ACCEL_GRP2_INT_REL);
			if (err < 0) {
				loop--;
				msleep(KIONIX_I2C_RETRY_TIMEOUT);
			} else
				loop = 0;
		}
		if (err < 0)
			KMSGERR(&acceld->client->dev,
				"%s: clear interrupt error = %d\n", __func__,
				err);
	}
}

static void kionix_accel_grp2_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp2_ctrl_reg1] &= ~ACCEL_GRP2_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
	case KIONIX_ACCEL_G_8G:
	case KIONIX_ACCEL_G_6G:
		acceld->shift = 2;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
		    ACCEL_GRP2_G_8G;
		break;
	case KIONIX_ACCEL_G_4G:
		acceld->shift = 3;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
		    ACCEL_GRP2_G_4G;
		break;
	case KIONIX_ACCEL_G_2G:
	default:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
		    ACCEL_GRP2_G_2G;
		break;
	}

	return;
}

static int kionix_accel_grp2_update_odr(struct kionix_accel_driver *acceld,
					unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp2_odr_table); i++) {
		odr = kionix_accel_grp2_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp2_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG
	   register if the ODR is not changed */
	if (acceld->accel_registers[accel_grp2_data_ctrl] == odr)
		return 0;
	else
		acceld->accel_registers[accel_grp2_data_ctrl] = odr;

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP2_CTRL_REG1, 0);
		if (err < 0)
			return err;

		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP2_DATA_CTRL,
					      acceld->accel_registers
					      [accel_grp2_data_ctrl]);
		if (err < 0)
			return err;

		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP2_CTRL_REG1,
					      acceld->accel_registers
					      [accel_grp2_ctrl_reg1] |
					      ACCEL_GRP2_PC1_ON);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
					ACCEL_GRP4_DATA_CTRL,
					acceld->accel_registers
					[accel_grp4_data_ctrl]);
	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_INT_CTRL1,
						acceld->accel_registers
						[accel_grp4_int_ctrl]);
		if (err < 0)
			return err;
	}

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_CTRL_REG1,
						acceld->accel_registers
						[accel_grp4_ctrl_reg1] |
						ACCEL_GRP4_PC1_ON);
		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
						ACCEL_GRP4_CTRL_REG1,
						acceld->accel_registers
						[accel_grp4_ctrl_reg1]);
		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1,
					acceld->accel_registers
					[accel_grp4_ctrl_reg1] |
					ACCEL_GRP4_PC1_ON);
	if (err < 0)
		return err;

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
				   0);

	return 0;
}

static int kionix_accel_grp4_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if (acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err =
	    i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, 0);
	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp4_report_accel_data(struct kionix_accel_driver
						*acceld)
{
	struct {
		union {
			s16 accel_data_s16[3];
			s8 accel_data_s8[6];
		};
	} accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;

	/* Only read the output registers if enabled */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		if (atomic_read(&acceld->accel_enable_resume) > 0) {

			loop = KIONIX_I2C_RETRY_COUNT;
			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
				    kionix_i2c_read(acceld->client,
						    ACCEL_GRP4_XOUT_L, (u8 *)
						    accel_data.accel_data_s16,
						    6);
				mutex_unlock(&input_dev->mutex);
				if (err < 0) {
					loop--;
					msleep(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}
			if (err < 0) {
				KMSGERR(&acceld->client->dev,
					"%s: read data output error = %d\n",
					__func__, err);
			} else {

				write_lock(&acceld->rwlock_accel_data);

				x = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_x]));
				y = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_y]));
				z = ((s16)
				     le16_to_cpu(accel_data.accel_data_s16
						 [acceld->axis_map_z]));

				acceld->accel_data[acceld->axis_map_x] =
				    (acceld->negate_x ? -x : x) +
				    acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
				    (acceld->negate_y ? -y : y) +
				    acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
				    (acceld->negate_z ? -z : z) +
				    acceld->accel_cali[acceld->axis_map_z];

				if (atomic_read(&acceld->accel_input_event)
					> 0) {
					input_report_abs(acceld->input_dev,
							 ABS_X,
							 acceld->accel_data
							 [acceld->axis_map_x]);
					input_report_abs(acceld->input_dev,
							 ABS_Y,
							 acceld->accel_data
							 [acceld->axis_map_y]);
					input_report_abs(acceld->input_dev,
							 ABS_Z,
							 acceld->accel_data
							 [acceld->axis_map_z]);
					input_sync(acceld->input_dev);
				}
				write_unlock(&acceld->rwlock_accel_data);
			}
		} else {
			atomic_inc(&acceld->accel_enable_resume);
		}
	}

	/* Clear the interrupt if using drdy */
	if (acceld->accel_drdy == 1) {

		loop = KIONIX_I2C_RETRY_COUNT;
		while (loop) {
			err =
			    i2c_smbus_read_byte_data(acceld->client,
						     ACCEL_GRP4_INT_REL);
			if (err < 0) {
				loop--;
				msleep(KIONIX_I2C_RETRY_TIMEOUT);
			} else
				loop = 0;
		}
		if (err < 0)
			KMSGERR(&acceld->client->dev,
				"%s: clear interrupt error = %d\n", __func__,
				err);
	}
}

static void kionix_accel_grp4_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp4_ctrl_reg1] &= ~ACCEL_GRP4_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
	case KIONIX_ACCEL_G_8G:
	case KIONIX_ACCEL_G_6G:
		acceld->shift = 2;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
		    ACCEL_GRP4_G_8G;
		break;
	case KIONIX_ACCEL_G_4G:
		acceld->shift = 3;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
		    ACCEL_GRP4_G_4G;
		break;
	case KIONIX_ACCEL_G_2G:
	default:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
		    ACCEL_GRP4_G_2G;
		break;
	}

	return;
}

static int kionix_accel_grp4_update_odr(struct kionix_accel_driver *acceld,
					unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp4_odr_table); i++) {
		odr = kionix_accel_grp4_odr_table[i].mask;
		if (poll_interval < kionix_accel_grp4_odr_table[i].cutoff)
			break;
	}

	if (acceld->accel_registers[accel_grp4_data_ctrl] == odr)
		return 0;
	else
		acceld->accel_registers[accel_grp4_data_ctrl] = odr;

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP4_CTRL_REG1, 0);
		if (err < 0)
			return err;

		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP4_DATA_CTRL,
					      acceld->accel_registers
					      [accel_grp4_data_ctrl]);
		if (err < 0)
			return err;

		err =
		    i2c_smbus_write_byte_data(acceld->client,
					      ACCEL_GRP4_CTRL_REG1,
					      acceld->accel_registers
					      [accel_grp4_ctrl_reg1] |
					      ACCEL_GRP4_PC1_ON);
		if (err < 0)
			return err;

		err =
		    i2c_smbus_read_byte_data(acceld->client,
					     ACCEL_GRP4_DATA_CTRL);
		if (err < 0)
			return err;
		switch (err) {
		case ACCEL_GRP4_ODR0_781:
			dev_info(&acceld->client->dev, "ODR = 0.781 Hz\n");
			break;
		case ACCEL_GRP4_ODR1_563:
			dev_info(&acceld->client->dev, "ODR = 1.563 Hz\n");
			break;
		case ACCEL_GRP4_ODR3_125:
			dev_info(&acceld->client->dev, "ODR = 3.125 Hz\n");
			break;
		case ACCEL_GRP4_ODR6_25:
			dev_info(&acceld->client->dev, "ODR = 6.25 Hz\n");
			break;
		case ACCEL_GRP4_ODR12_5:
			dev_info(&acceld->client->dev, "ODR = 12.5 Hz\n");
			break;
		case ACCEL_GRP4_ODR25:
			dev_info(&acceld->client->dev, "ODR = 25 Hz\n");
			break;
		case ACCEL_GRP4_ODR50:
			dev_info(&acceld->client->dev, "ODR = 50 Hz\n");
			break;
		case ACCEL_GRP4_ODR100:
			dev_info(&acceld->client->dev, "ODR = 100 Hz\n");
			break;
		case ACCEL_GRP4_ODR200:
			dev_info(&acceld->client->dev, "ODR = 200 Hz\n");
			break;
		case ACCEL_GRP4_ODR400:
			dev_info(&acceld->client->dev, "ODR = 400 Hz\n");
			break;
		case ACCEL_GRP4_ODR800:
			dev_info(&acceld->client->dev, "ODR = 800 Hz\n");
			break;
		case ACCEL_GRP4_ODR1600:
			dev_info(&acceld->client->dev, "ODR = 1600 Hz\n");
			break;
		default:
			dev_info(&acceld->client->dev, "Unknown ODR\n");
			break;
		}
	}

	return 0;
}
/*
static int kionix_accel_power_on(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_on)
		return acceld->accel_pdata.power_on();

	return 0;
}
*/
static void kionix_accel_power_off(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_off)
		acceld->accel_pdata.power_off();
}

static irqreturn_t kionix_accel_isr(int irq, void *dev)
{
	struct kionix_accel_driver *acceld = dev;

	queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);

	return IRQ_HANDLED;
}

static void kionix_accel_work(struct work_struct *work)
{
	struct kionix_accel_driver *acceld =
	    container_of((struct delayed_work *)work,
			 struct kionix_accel_driver, accel_work);

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
				   msecs_to_jiffies(atomic_read
						    (&acceld->delay)));
	acceld->kionix_accel_report_accel_data(acceld);
}

static void kionix_accel_update_direction(struct kionix_accel_driver *acceld)
{
	unsigned int direction = acceld->accel_pdata.accel_direction;
	unsigned int accel_group = acceld->accel_group;

	write_lock(&acceld->rwlock_accel_data);
	acceld->axis_map_x = ((direction - 1) % 2);
	acceld->axis_map_y = (direction % 2);
	acceld->axis_map_z = 2;
	acceld->negate_z = ((direction - 1) / 4);
	switch (accel_group) {
	case KIONIX_ACCEL_GRP3:
	case KIONIX_ACCEL_GRP6:
		acceld->negate_x = (((direction + 2) / 2) % 2);
		acceld->negate_y = (((direction + 5) / 4) % 2);
		break;
	case KIONIX_ACCEL_GRP5:
		acceld->axis_map_x = (direction % 2);
		acceld->axis_map_y = ((direction - 1) % 2);
		acceld->negate_x = (((direction + 1) / 2) % 2);
		acceld->negate_y =
		    (((direction / 2) + ((direction - 1) / 4)) % 2);
		break;
	default:
		acceld->negate_x = ((direction / 2) % 2);
		acceld->negate_y = (((direction + 1) / 4) % 2);
		break;
	}
	write_unlock(&acceld->rwlock_accel_data);
	return;
}

static int kionix_accel_enable(struct kionix_accel_driver *acceld)
{
	int err = 0;
	long remaining;
	struct kionix_accel_platform_data pdata = acceld->accel_pdata;

	if (pdata.acc_power_on)
		pdata.acc_power_on(true);

	mutex_lock(&acceld->mutex_earlysuspend);

	atomic_set(&acceld->accel_suspend_continue, 0);

	if (atomic_read(&acceld->accel_suspended) == 1) {
		KMSGINF(&acceld->client->dev, "%s: waiting for resume\n",
			__func__);
		remaining =
		    wait_event_interruptible_timeout(acceld->wqh_suspend,
			atomic_read(&acceld->accel_suspended) == 0,
			msecs_to_jiffies
			(KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));

		if (atomic_read(&acceld->accel_suspended) == 1) {
			KMSGERR(&acceld->client->dev,
				"%s: timeout waiting for resume\n", __func__);
			err = -ETIME;
			goto exit;
		}
	}

	err = acceld->kionix_accel_operate(acceld);

	if (err < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: kionix_accel_operate returned err = %d\n",
			__func__, err);
		goto exit;
	}

	/* atomic_inc(&acceld->accel_enabled); */

exit:
	mutex_unlock(&acceld->mutex_earlysuspend);
	pr_info("%s: ++enable_sensor\n", __func__);
	return err;
}

static int kionix_accel_disable(struct kionix_accel_driver *acceld)
{
	int err = 0;
	struct kionix_accel_platform_data pdata = acceld->accel_pdata;

	mutex_lock(&acceld->mutex_resume);

	atomic_set(&acceld->accel_suspend_continue, 1);

	if (atomic_read(&acceld->accel_enabled) > 0) {
		if (atomic_dec_and_test(&acceld->accel_enabled)) {
			if (atomic_read(&acceld->accel_enable_resume) > 0)
				atomic_set(&acceld->accel_enable_resume, 0);
			err = acceld->kionix_accel_standby(acceld);
			if (err < 0) {
				KMSGERR(&acceld->client->dev,
					"%s: kionix_accel_standby returned err = %d\n",
					__func__, err);
				goto exit;
			}
			wake_up_interruptible(&acceld->wqh_suspend);
		}
	}

	/* Vote off  regulators */
	if (pdata.acc_power_on)
		pdata.acc_power_on(false);

exit:
	mutex_unlock(&acceld->mutex_resume);

	return err;
}

static int kionix_accel_input_open(struct input_dev *input)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(input);

	atomic_inc(&acceld->accel_input_event);

	return 0;
}

static void kionix_accel_input_close(struct input_dev *dev)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(dev);

	atomic_dec(&acceld->accel_input_event);
}

static void kionix_accel_init_input_device(struct kionix_accel_driver *acceld,
					   struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -ACCEL_G_MAX, ACCEL_G_MAX,
			     ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Y, -ACCEL_G_MAX, ACCEL_G_MAX,
			     ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Z, -ACCEL_G_MAX, ACCEL_G_MAX,
			     ACCEL_FUZZ, ACCEL_FLAT);

	input_dev->name = KIONIX_ACCEL_NAME;
	input_dev->id.bustype = BUS_I2C;
	/*input_dev->dev.parent = &acceld->client->dev; */
}

static int kionix_accel_setup_input_device(struct kionix_accel_driver *acceld)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		KMSGERR(&acceld->client->dev, "input_allocate_device failed\n");
		return -ENOMEM;
	}

	acceld->input_dev = input_dev;

	input_dev->open = kionix_accel_input_open;
	input_dev->close = kionix_accel_input_close;
	input_set_drvdata(input_dev, acceld);

	kionix_accel_init_input_device(acceld, input_dev);

	err = input_register_device(acceld->input_dev);
	if (err) {
		KMSGERR(&acceld->client->dev,
			"%s: input_register_device returned err = %d\n",
			__func__, err);
		input_free_device(acceld->input_dev);
		return err;
	}

	return 0;
}

/* Returns the enable state of device */
static ssize_t kionix_accel_get_enable(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		       atomic_read(&acceld->accel_enabled) > 0 ? 1 : 0);
}

/* Allow users to enable/disable the device */
static ssize_t kionix_accel_set_enable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int enable_count = 1;
	unsigned long enable;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (kionix_strtok(buf, count, &buf2, enable_count) < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: No enable data being read.\n", __func__);
	} else {
		/* Removes any leading negative sign */
		while (*buf2 == '-')
			buf2++;

		err =
		    kstrtouint((const char *)buf2, 10, (unsigned int *)&enable);
		if (err < 0) {
			KMSGERR(&acceld->client->dev,
				"%s: kstrtouint returned err = %d\n", __func__,
				err);
			goto exit;
		}

		if (enable)
			err = kionix_accel_enable(acceld);
		else
			err = kionix_accel_disable(acceld);

	}

exit:
	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

/* Returns currently selected poll interval (in ms) */
static ssize_t kionix_accel_get_delay(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", acceld->poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kionix_accel_set_delay(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int delay_count = 1;
	unsigned long interval;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (kionix_strtok(buf, count, &buf2, delay_count) < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: No delay data being read.", __func__);
	}

	else {
		/* Removes any leading negative sign */
		while (*buf2 == '-')
			buf2++;

		err =
		    kstrtouint((const char *)buf2, 10,
			       (unsigned int *)&interval);
		if (err < 0) {
			KMSGERR(&acceld->client->dev,
				"%s: kstrtouint returned err = %d\n", __func__,
				err);
			goto exit;
		}

		if (acceld->accel_drdy == 1)
			disable_irq(client->irq);

		/*
		 * Set current interval to the greater of
		 * the minimum interval or
		 * the requested interval
		 */
		acceld->poll_interval =
		    max((unsigned int)interval,
			acceld->accel_pdata.min_interval);
		acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval);

		err =
		    acceld->kionix_accel_update_odr(acceld,
						    acceld->poll_interval);

		if (acceld->accel_drdy == 1)
			enable_irq(client->irq);
	}

exit:
	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

/* Returns the direction of device */
static ssize_t kionix_accel_get_direct(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		acceld->accel_pdata.accel_direction);
}

/* Allow users to change the direction the device */
static ssize_t kionix_accel_set_direct(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int direct_count = 1;
	unsigned long direction;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (kionix_strtok(buf, count, &buf2, direct_count) < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: No direction data being read.\n", __func__);
	}

	else {
		/* Removes any leading negative sign */
		while (*buf2 == '-')
			buf2++;

		err =
		    kstrtouint((const char *)buf2, 10,
			       (unsigned int *)&direction);
		if (err < 0) {
			KMSGERR(&acceld->client->dev,
				"%s: kstrtouint returned err = %d\n", __func__,
				err);
			goto exit;
		}

		if (direction < 1 || direction > 8)
			KMSGERR(&acceld->client->dev,
				"%s: invalid direction = %d\n", __func__,
				(unsigned int)direction);

		else {
			acceld->accel_pdata.accel_direction = (u8) direction;
			kionix_accel_update_direction(acceld);
		}
	}

exit:
	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

/* Returns the data output of device */
static ssize_t kionix_accel_get_data(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int x, y, z;

	read_lock(&acceld->rwlock_accel_data);

	x = acceld->accel_data[acceld->axis_map_x];
	y = acceld->accel_data[acceld->axis_map_y];
	z = acceld->accel_data[acceld->axis_map_z];

	read_unlock(&acceld->rwlock_accel_data);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", x, y, z);
}

/* Returns the calibration value of the device */
static ssize_t kionix_accel_get_cali(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int calibration[3];

	read_lock(&acceld->rwlock_accel_data);

	calibration[0] = acceld->accel_cali[acceld->axis_map_x];
	calibration[1] = acceld->accel_cali[acceld->axis_map_y];
	calibration[2] = acceld->accel_cali[acceld->axis_map_z];

	read_unlock(&acceld->rwlock_accel_data);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", calibration[0],
			calibration[1],
		       calibration[2]);
}

/* Allow users to change the calibration value of the device */
static ssize_t kionix_accel_set_cali(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
/* How many calibration that we expect to get from the string */
	const int cali_count = 3;
	char **buf2;
	long calibration[cali_count];
	int err = 0, i = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	buf2 = kzalloc(cali_count * sizeof(char *), GFP_KERNEL);

	if (kionix_strtok(buf, count, buf2, cali_count) < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: Not enough calibration data being read.\n",
			__func__);
	} else {
		/* Convert string to integers  */
		for (i = 0; i < cali_count; i++) {

			err =
			    kstrtoint((const char *)*(buf2 + i), 10,
				      (int *)&calibration[i]);
			if (err < 0) {
				KMSGERR(&acceld->client->dev,
					"%s: No calibration data will be updated.\n",
					__func__);
				goto exit;
			}
		}

		write_lock(&acceld->rwlock_accel_data);

		acceld->accel_cali[acceld->axis_map_x] = (int)calibration[0];
		acceld->accel_cali[acceld->axis_map_y] = (int)calibration[1];
		acceld->accel_cali[acceld->axis_map_z] = (int)calibration[2];

		write_unlock(&acceld->rwlock_accel_data);
	}

exit:
	for (i = 0; i < cali_count; i++)
		kfree(*(buf2 + i));

	kfree(buf2);

	mutex_unlock(&input_dev->mutex);

	return (err < 0) ? err : count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
		   kionix_accel_get_enable, kionix_accel_set_enable);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
		   kionix_accel_get_delay, kionix_accel_set_delay);
static DEVICE_ATTR(direct, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
		   kionix_accel_get_direct, kionix_accel_set_direct);
static DEVICE_ATTR(data, S_IRUGO, kionix_accel_get_data, NULL);
static DEVICE_ATTR(cali, S_IRUGO | S_IWUSR | S_IWGRP | S_IWOTH,
		   kionix_accel_get_cali, kionix_accel_set_cali);

static struct attribute *kionix_accel_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_direct.attr,
	&dev_attr_data.attr,
	&dev_attr_cali.attr,
	NULL
};

static struct attribute_group kionix_accel_attribute_group = {
	.attrs = kionix_accel_attributes
};

static int kionix_verify(struct kionix_accel_driver *acceld)
{
	int retval = i2c_smbus_read_byte_data(acceld->client, ACCEL_WHO_AM_I);
	pr_info("%s: devices id is %d\n", __func__, retval);
#if KIONIX_KMSG_INF
	switch (retval) {
	case KIONIX_ACCEL_WHO_AM_I_KXTE9:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTE9.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTF9:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTF9.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTI9-1001.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTIK-1004.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTJ9-1005.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTJ9-1007.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXCJ9-1008.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1029:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXTJ2-1029.\n");
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
		KMSGINF(&acceld->client->dev,
			"this accelerometer is a KXCJK-1013.\n");
		break;
	default:
		break;
	}
#endif

	return retval;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void kionix_accel_earlysuspend_suspend(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld =
	    container_of(h, struct kionix_accel_driver, early_suspend);
	long remaining;

	mutex_lock(&acceld->mutex_earlysuspend);

	/* Only continue to suspend if enable did not intervene */
	if (atomic_read(&acceld->accel_suspend_continue) > 0) {
		if (atomic_read(&acceld->accel_enabled) > 0) {
			KMSGINF(&acceld->client->dev,
				"%s: waiting for disable\n", __func__);
			remaining =
			    wait_event_interruptible_timeout
			    (acceld->wqh_suspend,
			     atomic_read(&acceld->accel_enabled) < 1,
			     msecs_to_jiffies
			     (KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT));

			if (atomic_read(&acceld->accel_enabled) > 0)
				KMSGERR(&acceld->client->dev,
					"%s: timeout waiting for disable\n",
					__func__);
		}

		kionix_accel_power_off(acceld);

		atomic_set(&acceld->accel_suspended, 1);
	}

	mutex_unlock(&acceld->mutex_earlysuspend);

	return;
}

void kionix_accel_earlysuspend_resume(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld =
	    container_of(h, struct kionix_accel_driver, early_suspend);
	int err;

	mutex_lock(&acceld->mutex_resume);

	if (atomic_read(&acceld->accel_suspended) == 1) {
		err = kionix_accel_power_on(acceld);
		if (err < 0) {
			KMSGERR(&acceld->client->dev,
				"%s: kionix_accel_power_on returned err = %d\n",
				__func__, err);
			goto exit;
		}

		if (err > 0) {
			err = acceld->kionix_accel_power_on_init(acceld);
			if (err) {
				KMSGERR(&acceld->client->dev,
					"%s: kionix_accel_power_on_init returned err = %d\n",
					__func__, err);
				goto exit;
			}
		}

		atomic_set(&acceld->accel_suspended, 0);
	}

	wake_up_interruptible(&acceld->wqh_suspend);

exit:
	mutex_unlock(&acceld->mutex_resume);

	return;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int kionix_accel_regulator_configure(
	struct kionix_accel_driver *data, bool on)
{
	int rc;

	if (!on) {

		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,
				KIONIX_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0,
				KIONIX_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd,
				KIONIX_VDD_MIN_UV, KIONIX_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio,
				KIONIX_VIO_MIN_UV, KIONIX_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,
				"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, KIONIX_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int kionix_accel_regulator_power_on(
	struct kionix_accel_driver *data, bool on)
{
	int rc = 0;

	if (!on) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			dev_err(&data->client->dev,
					"Regulator vio re-enabled rc=%d\n", rc);
			/*
			 * Successfully re-enable regulator.
			 * Enter poweron delay and returns error.
			 */
			if (!rc) {
				rc = -EBUSY;
				goto enable_delay;
			}
		}
		return rc;
	} else {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
			return rc;
		}
	}

enable_delay:
	msleep(130);
	dev_dbg(&data->client->dev,
		"Sensor regulator power on =%d\n", on);
	return rc;
}

static int kionix_accel_platform_hw_power_on(bool on)
{
	struct kionix_accel_driver *data;
	int err = 0;

	if (kionix_data == NULL)
		return -ENODEV;

	data = kionix_data;
	if (data->power_on != on) {
		err = kionix_accel_regulator_power_on(data, on);
		if (err)
			dev_err(&data->client->dev,
					"Can't configure regulator!\n");
		else
			data->power_on = on;
	}

	return err;
}

static int kionix_accel_platform_hw_init(void)
{
	struct i2c_client *client;
	struct kionix_accel_driver *data;
	int error;

	if (kionix_data == NULL)
		return -ENODEV;

	data = kionix_data;
	client = data->client;

	error = kionix_accel_regulator_configure(data, true);
	if (error < 0) {
		dev_err(&client->dev, "unable to configure regulator\n");
		return error;
	}

	return 0;
}

static void kionix_accel_platform_hw_exit(void)
{
	struct kionix_accel_driver *data = kionix_data;

	if (data == NULL)
		return;

	kionix_accel_regulator_configure(data, false);

}

#ifdef CONFIG_OF
static int kionix_accel_parse_dt(struct device *dev,
			  struct kionix_accel_platform_data *accel_pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	/* set functions of platform data */
	accel_pdata->acc_init = kionix_accel_platform_hw_init;
	accel_pdata->acc_exit = kionix_accel_platform_hw_exit;
	accel_pdata->acc_power_on = kionix_accel_platform_hw_power_on;

	rc = of_property_read_u32(np, "kionix,min_interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read min-interval\n");
		return rc;
	} else {
		accel_pdata->min_interval = temp_val;
	}

	rc = of_property_read_u32(np, "kionix,poll_interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read poll-interval\n");
		return rc;
	} else {
		accel_pdata->poll_interval = temp_val;
	}

	rc = of_property_read_u32(np, "kionix,accel_direction", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read accel_direction\n");
		return rc;
	} else {
		accel_pdata->accel_direction = (u8) temp_val;
	}

	rc = of_property_read_u32(np, "kionix,accel_irq_use_drdy", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read accel_irq_use_drdy\n");
		return rc;
	} else {
		accel_pdata->accel_irq_use_drdy = temp_val;
	}

	rc = of_property_read_u32(np, "kionix,accel_res", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read accel_res\n");
		return rc;
	} else {
		accel_pdata->accel_res = (u8) temp_val;
	}

	rc = of_property_read_u32(np, "kionix,accel_g_range", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read g-range\n");
		return rc;
	} else {
		switch (temp_val) {
		case 2:
			accel_pdata->accel_g_range = KIONIX_ACCEL_G_2G;
			break;
		case 4:
			accel_pdata->accel_g_range = KIONIX_ACCEL_G_4G;
			break;
		case 6:
			accel_pdata->accel_g_range = KIONIX_ACCEL_G_6G;
			break;
		case 8:
			accel_pdata->accel_g_range = KIONIX_ACCEL_G_8G;
			break;
		default:
			accel_pdata->accel_g_range = KIONIX_ACCEL_G_2G;
			break;
		}
	}

	return 0;
}
#else
static int kionix_accel_parse_dt(struct device *dev,
			  struct kionix_accel_platform_data *accel_pdata)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

static int kionix_accel_cdev_enable(struct sensors_classdev *sensors_cdev,
				    unsigned int enable)
{
	struct kionix_accel_driver *acceld = container_of(sensors_cdev,
							  struct
							  kionix_accel_driver,
							  cdev);
	int pre_enable = atomic_read(&acceld->accel_enabled);

	if (enable) {
		if (pre_enable == 0) {
			kionix_accel_enable(acceld);
			atomic_set(&acceld->accel_enabled, 1);
		}
	} else {
		if (pre_enable == 1) {
			kionix_accel_disable(acceld);
			atomic_set(&acceld->accel_enabled, 0);
		}
	}
	return 0;
}

static int kionix_accel_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
					unsigned int delay_ms)
{
	struct kionix_accel_driver *acceld = container_of(sensors_cdev,
							  struct
							  kionix_accel_driver,
							  cdev);

	if (delay_ms < KIONIX_ACCEL_MIN_DELAY)
		delay_ms = KIONIX_ACCEL_MIN_DELAY;
	if (delay_ms > KIONIX_ACCEL_MAX_DELAY)
		delay_ms = KIONIX_ACCEL_MAX_DELAY;
	atomic_set(&acceld->delay, (unsigned int)delay_ms);
	acceld->kionix_accel_update_odr(acceld, atomic_read(&acceld->delay));
	return 0;
}

static int kionix_accel_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct kionix_accel_platform_data *accel_pdata;
	struct kionix_accel_driver *acceld;
	int err;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		KMSGERR(&client->dev, "client is not i2c capable. Abort.\n");
		return -ENXIO;
	}
	accel_pdata = kzalloc(sizeof(*accel_pdata), GFP_KERNEL);
	if (accel_pdata == NULL) {
		KMSGERR(&client->dev,
			"failed to allocate memory for module data. Abort.\n");
		return -ENOMEM;
	}
	if (client->dev.of_node) {
		memset(accel_pdata, 0,
		       sizeof(struct kionix_accel_platform_data));
		err = kionix_accel_parse_dt(&client->dev, accel_pdata);
		if (err) {
			dev_err(&client->dev,
				"Unable to parse platfrom data err=%d\n", err);
			goto err_free_pdata;
		}
	} else {
		if (client->dev.platform_data) {
			accel_pdata =
			    (struct kionix_accel_platform_data *)client->
			    dev.platform_data;
		} else {
			KMSGERR(&client->dev,
				"platform data is NULL. Abortk.\n");
			return -EINVAL;
		}
	}
	acceld = kzalloc(sizeof(*acceld), GFP_KERNEL);
	if (acceld == NULL) {
		KMSGERR(&client->dev,
			"failed to allocate memory for module data. Abort.\n");
		goto err_free_mem;
	}
	kionix_data = acceld;
	acceld->client = client;
	acceld->accel_pdata = *accel_pdata;

	i2c_set_clientdata(client, acceld);

	/* h/w initialization */
	if (accel_pdata->acc_init)
		err = accel_pdata->acc_init();

	if (accel_pdata->acc_power_on)
		err = accel_pdata->acc_power_on(true);

	err = kionix_verify(acceld);
	if (err < 0) {
		KMSGERR(&acceld->client->dev,
			"%s: kionix_verify returned err = %d. Abort.\n",
			__func__, err);
		goto err_accel_pdata_power_off;
	}

	/* Setup group specific configuration and function callback */
	switch (err) {
	case KIONIX_ACCEL_WHO_AM_I_KXTE9:
		acceld->accel_group = KIONIX_ACCEL_GRP1;
		acceld->accel_registers =
		    kzalloc(sizeof(u8) * accel_grp1_regs_count, GFP_KERNEL);
		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
				"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_power_off;
		}
		acceld->accel_drdy = 0;
		acceld->kionix_accel_report_accel_data =
		    kionix_accel_grp1_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp1_update_odr;
		acceld->kionix_accel_power_on_init =
		    kionix_accel_grp1_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp1_operate;
		acceld->kionix_accel_standby = kionix_accel_grp1_standby;
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTF9:
	case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
	case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
		if (err == KIONIX_ACCEL_WHO_AM_I_KXTIK_1004)
			acceld->accel_group = KIONIX_ACCEL_GRP3;
		else
			acceld->accel_group = KIONIX_ACCEL_GRP2;
		acceld->accel_registers =
		    kzalloc(sizeof(u8) * accel_grp2_regs_count, GFP_KERNEL);
		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
				"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_power_off;
		}
		switch (acceld->accel_pdata.accel_res) {
		case KIONIX_ACCEL_RES_6BIT:
		case KIONIX_ACCEL_RES_8BIT:
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			    ACCEL_GRP2_RES_8BIT;
			break;
		case KIONIX_ACCEL_RES_12BIT:
		default:
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			    ACCEL_GRP2_RES_12BIT;
			break;
		}
		if (acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			acceld->accel_registers[accel_grp2_int_ctrl] |=
			    ACCEL_GRP2_IEN | ACCEL_GRP2_IEA;
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			    ACCEL_GRP2_DRDYE;
			acceld->accel_drdy = 1;
		} else
			acceld->accel_drdy = 0;
		kionix_accel_grp2_update_g_range(acceld);
		acceld->kionix_accel_report_accel_data =
		    kionix_accel_grp2_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp2_update_odr;
		acceld->kionix_accel_power_on_init =
		    kionix_accel_grp2_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp2_operate;
		acceld->kionix_accel_standby = kionix_accel_grp2_standby;
		break;
	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
	case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
	case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1029:
	case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
		if (err == KIONIX_ACCEL_WHO_AM_I_KXTJ2_1029)
			acceld->accel_group = KIONIX_ACCEL_GRP5;
		else if (err == KIONIX_ACCEL_WHO_AM_I_KXCJK_1013)
			acceld->accel_group = KIONIX_ACCEL_GRP6;
		else
			acceld->accel_group = KIONIX_ACCEL_GRP4;
		acceld->accel_registers =
		    kzalloc(sizeof(u8) * accel_grp4_regs_count, GFP_KERNEL);
		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
				"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_power_off;
		}
		switch (acceld->accel_pdata.accel_res) {
		case KIONIX_ACCEL_RES_6BIT:
		case KIONIX_ACCEL_RES_8BIT:
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			    ACCEL_GRP4_RES_8BIT;
			break;
		case KIONIX_ACCEL_RES_12BIT:
		default:
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			    ACCEL_GRP4_RES_12BIT;
			break;
		}
		if (acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			acceld->accel_registers[accel_grp4_int_ctrl] |=
			    ACCEL_GRP4_IEN | ACCEL_GRP4_IEA;
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			    ACCEL_GRP4_DRDYE;
			acceld->accel_drdy = 1;
		} else
			acceld->accel_drdy = 0;
		kionix_accel_grp4_update_g_range(acceld);
		acceld->kionix_accel_report_accel_data =
		    kionix_accel_grp4_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp4_update_odr;
		acceld->kionix_accel_power_on_init =
		    kionix_accel_grp4_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp4_operate;
		acceld->kionix_accel_standby = kionix_accel_grp4_standby;
		break;
	default:
		KMSGERR(&acceld->client->dev,
			"%s: unsupported device, who am i = %d. Abort.\n",
			__func__, err);
		goto err_accel_pdata_power_off;
	}

	err = kionix_accel_setup_input_device(acceld);
	if (err)
		goto err_free_accel_registers;

	atomic_set(&acceld->accel_suspended, 0);
	atomic_set(&acceld->accel_suspend_continue, 1);
	atomic_set(&acceld->accel_enabled, 0);
	/*atomic_set(&acceld->accel_input_event, 0); */
	atomic_set(&acceld->accel_input_event, 1);
	atomic_set(&acceld->accel_enable_resume, 0);
	atomic_set(&acceld->delay, accel_pdata->poll_interval);

	mutex_init(&acceld->mutex_earlysuspend);
	mutex_init(&acceld->mutex_resume);
	rwlock_init(&acceld->rwlock_accel_data);

	acceld->poll_delay = msecs_to_jiffies(100);
	acceld->kionix_accel_update_odr(acceld, 100);
	kionix_accel_update_direction(acceld);

	acceld->accel_workqueue = create_workqueue("Kionix Accel Workqueue");
	INIT_DELAYED_WORK(&acceld->accel_work, kionix_accel_work);
	init_waitqueue_head(&acceld->wqh_suspend);

	if (acceld->accel_drdy) {
		err = request_threaded_irq(client->irq, NULL, kionix_accel_isr,
					   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					   KIONIX_ACCEL_IRQ, acceld);
		if (err) {
			KMSGERR(&acceld->client->dev,
				"%s: request_threaded_irq returned err = %d\n",
				__func__, err);
			KMSGERR(&acceld->client->dev,
				"%s: running in software polling mode instead\n",
				__func__);
			acceld->accel_drdy = 0;
		}
		KMSGINF(&acceld->client->dev,
			"running in hardware interrupt mode\n");
	} else {
		KMSGINF(&acceld->client->dev,
			"running in software polling mode\n");
	}

	err = acceld->kionix_accel_power_on_init(acceld);
	if (err) {
		KMSGERR(&acceld->client->dev,
			"%s: kionix_accel_power_on_init returned err = %d. Abort.\n",
			__func__, err);
		goto err_free_irq;
	}

	err =
	    sysfs_create_group(&acceld->input_dev->dev.kobj,
			       &kionix_accel_attribute_group);
	if (err) {
		KMSGERR(&acceld->client->dev,
			"%s: sysfs_create_group returned err = %d. Abort.\n",
			__func__, err);
		goto err_free_irq;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	acceld->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 50;
	acceld->early_suspend.suspend = kionix_accel_earlysuspend_suspend;
	acceld->early_suspend.resume = kionix_accel_earlysuspend_resume;
	register_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	/* Register to sensors class */
	acceld->cdev = sensors_cdev;
	acceld->cdev.sensors_enable = kionix_accel_cdev_enable;
	acceld->cdev.sensors_poll_delay = kionix_accel_cdev_poll_delay;
	err = sensors_classdev_register(&client->dev, &acceld->cdev);
	if (err) {
		dev_err(&client->dev, "create class device file failed\n");
		err = -EINVAL;
		goto exit_remove_sysfs_group;
	}

	return 0;

/*exit_unregister_acc_class:
	sensors_classdev_unregister(&acceld->cdev);*/
exit_remove_sysfs_group:
	sysfs_remove_group(&acceld->input_dev->dev.kobj,
		&kionix_accel_attribute_group);
err_free_irq:
	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);
	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
err_free_accel_registers:
	kfree(acceld->accel_registers);
err_accel_pdata_power_off:
	if (accel_pdata->acc_power_on)
		accel_pdata->acc_power_on(false);
	if (accel_pdata->acc_exit)
		accel_pdata->acc_exit();
err_free_mem:
	kionix_data = NULL;
	kfree(acceld);
err_free_pdata:
kfree(accel_pdata);
	return err;
}

static int kionix_accel_remove(struct i2c_client *client)
{
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct kionix_accel_platform_data accel_pdata = acceld->accel_pdata;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	sysfs_remove_group(&acceld->input_dev->dev.kobj,
			   &kionix_accel_attribute_group);
	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);
	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
	kfree(acceld->accel_registers);
	if (acceld->accel_pdata.exit)
		acceld->accel_pdata.exit();
	kionix_accel_power_off(acceld);

	if (accel_pdata.acc_power_on)
		accel_pdata.acc_power_on(false);
	if (accel_pdata.acc_exit)
		accel_pdata.acc_exit();
	kionix_data = NULL;
	kfree(acceld);

	return 0;
}

#ifdef CONFIG_PM
static int kionix_accel_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	kionix_accel_cdev_enable(&acceld->cdev, 0);

	return 0;
}

static int kionix_accel_resume(struct i2c_client *client)
{
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	kionix_accel_cdev_enable(&acceld->cdev, 1);

	return 0;
}

#else

#define kionix_accel_suspend      NULL
#define kionix_accel_resume       NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id kionix_accel_id[] = {
	{KIONIX_ACCEL_NAME, 0},
	{},
};

static struct of_device_id kionix_accel_match_table[] = {
	{.compatible = "kionix,kxtj2-1029",},
	{},
};

MODULE_DEVICE_TABLE(i2c, kionix_accel_id);

static struct i2c_driver kionix_accel_driver = {
	.driver = {
		   .name = KIONIX_ACCEL_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = kionix_accel_match_table,
		   },
	.suspend    = kionix_accel_suspend,
	.resume     = kionix_accel_resume,
	.probe = kionix_accel_probe,
	.remove = kionix_accel_remove,
	.id_table = kionix_accel_id,
};

static int __init kionix_accel_init(void)
{
	return i2c_add_driver(&kionix_accel_driver);
}

module_init(kionix_accel_init);

static void __exit kionix_accel_exit(void)
{
	i2c_del_driver(&kionix_accel_driver);
}

module_exit(kionix_accel_exit);

MODULE_DESCRIPTION("Kionix accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.3.0");
