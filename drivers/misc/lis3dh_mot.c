/*
 * Copyright (C) 2012-2013 Motorola Mobility LLC
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/lis3dh_mot.h>
#include <linux/regulator/consumer.h>

#define NAME				"lis3dh"

/** Maximum polled-device-reported g value */
#define G_MAX			16000

#define SHIFT_ADJ_2G		4
#define SHIFT_ADJ_4G		3
#define SHIFT_ADJ_8G		2
#define SHIFT_ADJ_16G		1

#define AXISDATA_REG		0x28

/* ctrl 1: ODR3 ODR2 ODR1 ODR0 LPen Zen Yen Xen */
#define CTRL_REG1		0x20	/* power control reg */
#define CTRL_REG2		0x21	/* power control reg */

#define CTRL_REG3		0x22	/* power control reg */
/* ctrl 4: BDU BLE FS1 FS0 HR ST1 ST0 SIM */
#define CTRL_REG4		0x23	/* interrupt control reg */
#define CTRL_REG5		0x24	/* 4D detection interrupt */
#define INT1_CFG		0x30	/* interrupt 1 config reg */
#define INT1_SRC		0x31	/* interrupt 1 status reg */
#define INT1_THS		0x32	/* interrupt 1 threshold reg */
#define INT1_DURATION		0x33	/* interrupt 1 duration reg  */

#define PM_LOW          	0x08
#define PM_NORMAL       	0x00
#define ENABLE_ALL_AXES 	0x07

#define ODR_OFF		0x00
#define ODR1		0x10
#define ODR10		0x20
#define ODR25		0x30
#define ODR50		0x40
#define ODR100		0x50
#define ODR200		0x60
#define ODR400		0x70
#define ODR1250		0x90

#define ROTATE_PM_MODE	(PM_LOW|ODR100|ENABLE_ALL_AXES)

#define G_2G		0x00
#define G_4G		0x10
#define G_8G		0x20
#define G_16G		0x30
#define HIGH_RES	0x08
#define BLK_UPDATE	0x80

#define FUZZ			0
#define FLAT			0
#define I2C_RETRY_DELAY		5
#define I2C_RETRIES		5
#define AUTO_INCREMENT		0x80

#define ENABLE_AOI1		0x40
#define ENABLE_D4D_INT1		0x04

#define MODE_OFF		0
#define MODE_ACCEL		1
#define MODE_ROTATE		2
#define MODE_ALL		3

#define TYPE_UNKNOWN		-1
#define TYPE_PORTRAIT		0
#define TYPE_LANDSCAPE		1
#define TYPE_INV_PORTRAIT	2
#define TYPE_INV_LANDSCAPE	3

#define INT_XL_BIT		0x01
#define INT_XH_BIT		0x02
#define INT_YL_BIT		0x04
#define INT_YH_BIT		0x08
#define INT_ZL_BIT		0x10
#define INT_ZH_BIT		0x20
#define INT_4D_BITS		0x0f
#define INT_6D_BITS		0x3f

#define INT_CFG_XY		0x4f
#define INT_CFG_XZ		0x73
#define INT_CFG_YZ		0x7C
#define INT_CFG_XYZ             0x7F
#define INT_CFG_INIT_THRESHOLD	0x2D
#define INT_CFG_THRESHOLD	0x20
#define INT_CFG_DURATION	0x05

static struct {
	unsigned int cutoff;
	unsigned int mask;
} odr_table[] = {
	{
	3,    PM_NORMAL | ODR1250}, {
	5,    PM_NORMAL |  ODR400}, {
	10,   PM_NORMAL |  ODR200}, {
	20,   PM_NORMAL |  ODR100}, {
	40,   PM_NORMAL |  ODR50}, {
	100,  PM_NORMAL |  ODR25}, {
	1000, PM_NORMAL |  ODR10}, {
	0,    PM_NORMAL |  ODR1},
};

struct lis3dh_data {
	struct i2c_client *client;
	struct lis3dh_platform_data *pdata;

	struct mutex lock;

	struct work_struct irq_work;
	struct delayed_work input_work;
	struct input_dev *input_dev;

	struct regulator *vdd;
	int irq;
	int mode;
	int hw_initialized;
	atomic_t enabled;
	int on_before_suspend;
	u8 shift_adj;
	u8 resume_state[5];
	u8 irq_config[3];

	struct notifier_block pm_notifier;
};

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct lis3dh_data *lis3dh_misc_data;

static int lis3dh_i2c_read(struct lis3dh_data *lis, u8 * buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = lis->client->addr,
		 .flags = lis->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = lis->client->addr,
		 .flags = (lis->client->flags & I2C_M_TEN) | I2C_M_RD,
		 .len = len,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(lis->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&lis->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_i2c_write(struct lis3dh_data *lis, u8 * buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
		 .addr = lis->client->addr,
		 .flags = lis->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(lis->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&lis->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_read_int_data(struct lis3dh_data *lis)
{
	int err = -1;
	u8 data[1];

	data[0] =  INT1_SRC;
	err = lis3dh_i2c_read(lis, data, 1);

	return err ? 0 : (int)data[0];
}

static int lis3dh_hw_init(struct lis3dh_data *lis)
{
	int err = -1;
	u8 buf[6];

	buf[0] = (AUTO_INCREMENT | CTRL_REG1);
	buf[1] = lis->resume_state[0];
	buf[2] = lis->resume_state[1];
	buf[3] = lis->resume_state[2];
	buf[4] = lis->resume_state[3];
	buf[5] = lis->resume_state[4];
	err = lis3dh_i2c_write(lis, buf, 5);
	if (err < 0)
		return err;

	buf[0] = INT1_CFG;
	buf[1] = lis->irq_config[0];
	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		return err;
	buf[0] = INT1_THS;
	buf[1] = INT_CFG_INIT_THRESHOLD;
	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		return err;
	buf[0] = INT1_DURATION;
	buf[1] = lis->irq_config[2];
	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		return err;
	lis->hw_initialized = 1;

	return 0;
}

static void lis3dh_device_power_off(struct lis3dh_data *lis)
{
	int err=0;
	u8 buf[2] = { CTRL_REG1, ODR_OFF };

	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		dev_err(&lis->client->dev, "soft power off failed\n");

	if (lis->pdata->power_off) {
		lis->pdata->power_off();
		lis->hw_initialized = 0;
	}
}

static int lis3dh_device_power_on(struct lis3dh_data *lis)
{
	int err=0;

	if (lis->pdata->power_on) {
		err = lis->pdata->power_on();
		if (err < 0)
			return err;
	}

	if (!lis->hw_initialized) {
		err = lis3dh_hw_init(lis);
		if (err < 0) {
			lis3dh_device_power_off(lis);
			return err;
		}
	}

	return 0;
}

int lis3dh_update_g_range(struct lis3dh_data *lis, int new_g_range)
{
	int err;
	u8 shift;
	u8 buf[2];

	switch (new_g_range) {
	case 2:
		buf[1] = G_2G | HIGH_RES;
		shift = SHIFT_ADJ_2G;
		break;
	case 4:
		buf[1] = G_4G | HIGH_RES;
		shift = SHIFT_ADJ_4G;
		break;
	case 8:
		buf[1] = G_8G | HIGH_RES;
		shift = SHIFT_ADJ_8G;
		break;
	case 16:
		buf[1] = G_16G | HIGH_RES;
		shift = SHIFT_ADJ_16G;
		break;
	default:
		return -EINVAL;
	}

	if (atomic_read(&lis->enabled)) {
		/* Set configuration register 4, which contains g range setting
		 *  NOTE: this is a straight overwrite because this driver does
		 *  not use any of the other configuration bits in this
		 *  register.  Should this become untrue, we will have to read
		 *  out the value and only change the relevant bits --XX----
		 *  (marked by X) */
		buf[0] = CTRL_REG4;
		err = lis3dh_i2c_write(lis, buf, 1);
		if (err < 0)
			return err;
	}

	lis->resume_state[3] = buf[1];
	lis->shift_adj = shift;

	return 0;
}

int lis3dh_update_odr(struct lis3dh_data *lis, int poll_interval)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching */
	for (i = 0; i < ARRAY_SIZE(odr_table); i++) {
		config[1] = odr_table[i].mask;
		if (poll_interval < odr_table[i].cutoff)
			break;
	}

	config[1] |= ENABLE_ALL_AXES;

	/* Overwrite CTRL_REG1 to set low power mode for ROTATE sensor */
	if (lis->mode == MODE_ROTATE)
		config[1] = ROTATE_PM_MODE;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&lis->enabled)) {
		config[0] = CTRL_REG1;
		err = lis3dh_i2c_write(lis, config, 1);
		if (err < 0)
			return err;
	}

	lis->resume_state[0] = config[1];

	return 0;
}

static int lis3dh_get_acceleration_data(struct lis3dh_data *lis, int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];

	acc_data[0] = (AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_i2c_read(lis, acc_data, 6);
	if (err < 0)
		return err;

	xyz[0] = (int) (((acc_data[1]) << 8) | acc_data[0]);
	xyz[1] = (int) (((acc_data[3]) << 8) | acc_data[2]);
	xyz[2] = (int) (((acc_data[5]) << 8) | acc_data[4]);

	xyz[0] = (xyz[0] & 0x8000) ? (xyz[0] | 0xFFFF0000) : (xyz[0]);
	xyz[1] = (xyz[1] & 0x8000) ? (xyz[1] | 0xFFFF0000) : (xyz[1]);
	xyz[2] = (xyz[2] & 0x8000) ? (xyz[2] | 0xFFFF0000) : (xyz[2]);

	xyz[0] >>= lis->shift_adj;
	xyz[1] >>= lis->shift_adj;
	xyz[2] >>= lis->shift_adj;

	return err;
}

static void lis3dh_report_values(struct lis3dh_data *lis, int *xyz)
{
	input_report_abs(lis->input_dev, ABS_X, xyz[0]);
	input_report_abs(lis->input_dev, ABS_Y, xyz[1]);
	input_report_abs(lis->input_dev, ABS_Z, xyz[2]);
	input_sync(lis->input_dev);
}

static void lis3dh_report_rotate(struct lis3dh_data *lis, int flat)
{
	input_event(lis->input_dev, EV_MSC, MSC_RAW, flat);
	input_sync(lis->input_dev);
}

static int lis3dh_enable_pull(struct lis3dh_data *lis)
{
	schedule_delayed_work(&lis->input_work,
				msecs_to_jiffies(lis->pdata->poll_interval));
	return 0;
}

static int lis3dh_set_threshold(struct lis3dh_data *lis, u8 config,
		u8 threshold)
{
	int err = -1;
	u8 buf[6];

	buf[0] = INT1_CFG;
	buf[1] = config;
	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		return err;
	buf[0] = INT1_THS;
	buf[1] = threshold;
	err = lis3dh_i2c_write(lis, buf, 1);
	if (err < 0)
		return err;

	return 0;
}

static int lis3dh_enable_irq(struct lis3dh_data *lis)
{
	int err;
	u8 buf[2];

	if (atomic_read(&lis->enabled)) {
		buf[0] = CTRL_REG4;
		err = lis3dh_i2c_read(lis, buf, 1);
		if (err < 0)
			return err;
		buf[1] = (buf[0] & ~G_16G) & BLK_UPDATE;
		buf[0] = CTRL_REG4;
		err = lis3dh_i2c_write(lis, buf, 1);
		if (err < 0)
			return err;

		buf[0] = CTRL_REG5;
		err = lis3dh_i2c_read(lis, buf, 1);
		if (err < 0)
			return err;
		buf[1] = (buf[0] | 0x4);
		buf[0] = CTRL_REG5;
		err = lis3dh_i2c_write(lis, buf, 1);
		if (err < 0)
			return err;

		err = lis3dh_set_threshold(lis, lis->irq_config[0],
				INT_CFG_INIT_THRESHOLD);
		if (err < 0)
			return err;

		lis->resume_state[3] = buf[1];
		lis->shift_adj = SHIFT_ADJ_2G;
	}

	enable_irq(lis->irq);

	return 0;
}

static int lis3dh_enable(struct lis3dh_data *lis)
{
	int err;

	if (!atomic_cmpxchg(&lis->enabled, 0, 1)) {

		err = lis3dh_device_power_on(lis);
		if (err < 0) {
			atomic_set(&lis->enabled, 0);
			return err;
		}
	}

	return 0;
}

static int lis3dh_disable_irq(struct lis3dh_data *lis)
{
	int err;

	disable_irq_nosync(lis->irq);
	err = lis3dh_update_g_range(lis, lis->pdata->g_range);

	return err;
}

static int lis3dh_disable_pull(struct lis3dh_data *lis)
{
	cancel_delayed_work_sync(&lis->input_work);
	return 0;
}

static int lis3dh_disable(struct lis3dh_data *lis)
{
	if (atomic_cmpxchg(&lis->enabled, 1, 0)) {
		lis3dh_device_power_off(lis);
	}

	return 0;
}

static int lis3dh_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = lis3dh_misc_data;

	return 0;
}

static long lis3dh_misc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int err;
	int interval;
	struct lis3dh_data *lis = file->private_data;

	switch (cmd) {
	case LIS3DH_IOCTL_GET_DELAY:
		interval = lis->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case LIS3DH_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 200)
			return -EINVAL;

		if (interval > lis->pdata->min_interval)
			lis->pdata->poll_interval = interval;
		else
			lis->pdata->poll_interval = lis->pdata->min_interval;
		err = lis3dh_update_odr(lis, lis->pdata->poll_interval);
		if (err < 0)
			return err;

		break;

	case LIS3DH_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < MODE_OFF || interval > MODE_ALL)
			return -EINVAL;

		switch (interval) {
		case MODE_OFF:  /* ALL OFF */
			if (lis->mode & MODE_ROTATE)
					lis3dh_disable_irq(lis);
			if (lis->mode & MODE_ACCEL)
					lis3dh_disable_pull(lis);
			lis3dh_disable(lis);
			break;

		case MODE_ACCEL: /* ACCEL ON & ROTATE OFF */

			if (!atomic_read(&lis->enabled)) {
				lis3dh_enable(lis);
			} else
				if (lis->mode & MODE_ROTATE)
						lis3dh_disable_irq(lis);
			if (!(lis->mode & MODE_ACCEL))
				lis3dh_enable_pull(lis);
			break;
		case MODE_ROTATE: /* ACCEL OFF & ROTATE 0N */

			if (!atomic_read(&lis->enabled)) {
				lis3dh_enable(lis);
			} else
				if (lis->mode & MODE_ACCEL)
					lis3dh_disable_pull(lis);
			if (!(lis->mode & MODE_ROTATE))
				lis3dh_enable_irq(lis);

			break;
		case MODE_ALL: /* ACCEL ON & ROTATE 0N */
			lis3dh_enable(lis);
			if (!(lis->mode & MODE_ROTATE))
				lis3dh_enable_irq(lis);
			if (!(lis->mode & MODE_ACCEL))
				lis3dh_enable_pull(lis);
			break;
		}
			lis->mode = interval;
		break;

	case LIS3DH_IOCTL_GET_ENABLE:
		interval = atomic_read(&lis->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations lis3dh_misc_fops = {
	.owner = THIS_MODULE,
	.open = lis3dh_misc_open,
	.unlocked_ioctl = lis3dh_misc_ioctl,
};

static struct miscdevice lis3dh_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &lis3dh_misc_fops,
};

static void lis3dh_input_work_func(struct work_struct *work)
{
	struct lis3dh_data *lis = container_of((struct delayed_work *)work,
						  struct lis3dh_data,
						  input_work);
	int xyz[3] = { 0 };
	int err;

	mutex_lock(&lis->lock);
	err = lis3dh_get_acceleration_data(lis, xyz);
	if (err < 0)
		dev_err(&lis->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_report_values(lis, xyz);

	schedule_delayed_work(&lis->input_work,
			      msecs_to_jiffies(lis->pdata->poll_interval));
	mutex_unlock(&lis->lock);
}

#ifdef LIS3DH_OPEN_ENABLE
int lis3dh_input_open(struct input_dev *input)
{
	struct lis3dh_data *lis = input_get_drvdata(input);

	return lis3dh_enable(lis);
}

void lis3dh_input_close(struct input_dev *dev)
{
	struct lis3dh_data *lis = input_get_drvdata(dev);

	lis3dh_disable(lis);
}
#endif

static void lis3dh_irq_work(struct work_struct *work)
{
	int irq_data = 0, flat = TYPE_UNKNOWN;
	struct lis3dh_data *lis = container_of(work, struct lis3dh_data,
						  irq_work);
	mutex_lock(&lis->lock);
	irq_data = lis3dh_read_int_data(lis);
	switch (irq_data & INT_4D_BITS) {
	case INT_XL_BIT:
		flat = TYPE_PORTRAIT;
		break;
	case INT_XH_BIT:
		flat = TYPE_INV_PORTRAIT;
		break;
	case INT_YL_BIT:
		flat = TYPE_LANDSCAPE;
		break;
	case INT_YH_BIT:
		flat = TYPE_INV_LANDSCAPE;
		break;
	default:
		flat = TYPE_UNKNOWN;
	}

	lis3dh_set_threshold(lis, INT_CFG_XYZ, lis->irq_config[1]);
	lis3dh_report_rotate(lis, flat);
	mutex_unlock(&lis->lock);

}

static irqreturn_t lis3dh_isr(int irq, void *data)
{
	struct lis3dh_data *lis = data;
	schedule_work(&lis->irq_work);
	return IRQ_HANDLED;
}

static int lis3dh_validate_pdata(struct lis3dh_data *lis)
{
	/* Enforce minimum polling interval */
	if (lis->pdata->poll_interval < lis->pdata->min_interval) {
		dev_err(&lis->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dh_input_init(struct lis3dh_data *lis)
{
	int err;

	INIT_DELAYED_WORK(&lis->input_work, lis3dh_input_work_func);

	lis->input_dev = input_allocate_device();
	if (!lis->input_dev) {
		err = -ENOMEM;
		dev_err(&lis->client->dev, "input device allocate failed\n");
		goto err0;
	}

#ifdef LIS3DH_OPEN_ENABLE
	lis->input_dev->open = lis3dh_input_open;
	lis->input_dev->close = lis3dh_input_close;
#endif

	input_set_drvdata(lis->input_dev, lis);

	set_bit(EV_ABS, lis->input_dev->evbit);

	input_set_abs_params(lis->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(lis->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(lis->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_capability(lis->input_dev, EV_MSC, MSC_RAW);

	lis->input_dev->name = "accelerometer";

	err = input_register_device(lis->input_dev);
	if (err) {
		dev_err(&lis->client->dev,
			"unable to register input polled device %s\n",
			lis->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(lis->input_dev);
err0:
	return err;
}

static void lis3dh_input_cleanup(struct lis3dh_data *lis)
{
	input_unregister_device(lis->input_dev);
	input_free_device(lis->input_dev);
}

static int lis3dh_resume(struct lis3dh_data *lis)
{
	int err;

	if (lis->on_before_suspend) {
		err = lis3dh_enable(lis);
		if (err < 0)
			dev_err(&lis->client->dev,
				"resume failure\n");
	}
	return 0;
}

static int lis3dh_suspend(struct lis3dh_data *lis)
{
	int err;

	lis->on_before_suspend =
		atomic_read(&lis->enabled);
	if (lis->on_before_suspend) {
		err = lis3dh_disable(lis);
		if (err < 0)
			dev_err(&lis->client->dev, "suspend failure\n");
	}

	return 0;
}

static int lis3dh_pm_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct lis3dh_data *lis = container_of(this,
		struct lis3dh_data, pm_notifier);

	mutex_lock(&lis->lock);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		lis3dh_suspend(lis);
		break;
	case PM_POST_SUSPEND:
		lis3dh_resume(lis);
		break;
	}

	mutex_unlock(&lis->lock);

	return NOTIFY_DONE;
}

#ifdef CONFIG_OF
static struct lis3dh_platform_data *
lis3dh_of_init(struct i2c_client *client)
{
	struct lis3dh_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	u32 val;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	pdata->irq_gpio = of_get_gpio(np, 0);
	if (!of_property_read_u32(np, "stm,poll-interval", &val))
		pdata->poll_interval = (int)val;
	if (!of_property_read_u32(np, "stm,min-interval", &val))
		pdata->min_interval = (int)val;
	if (!of_property_read_u32(np, "stm,g-range", &val))
		pdata->g_range = (int)val;
	if (!of_property_read_u32(np, "stm,threshold", &val))
		pdata->threshold = (int)val;
	else
		pdata->threshold = INT_CFG_THRESHOLD;
	if (!of_property_read_u32(np, "stm,duration", &val))
		pdata->duration = (int)val;
	else
		pdata->duration = INT_CFG_DURATION;

	return pdata;
}
#else
static inline struct lis3dh_platform_data *
lis3dh_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int lis3dh_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct lis3dh_platform_data *pdata;
	struct lis3dh_data *lis;
	int err = 0;

	if (client->dev.of_node)
		pdata = lis3dh_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err0;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}

	lis = kzalloc(sizeof(*lis), GFP_KERNEL);
	if (lis == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err0;
	}

	mutex_init(&lis->lock);
	mutex_lock(&lis->lock);
	lis->client = client;

	lis->pdata = pdata;

	err = lis3dh_validate_pdata(lis);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err1;
	}

	i2c_set_clientdata(client, lis);

	if (lis->pdata->init) {
		err = lis->pdata->init();
		if (err < 0)
			goto err1;
	}

	lis->vdd = regulator_get(&client->dev, "vdd");
	if (IS_ERR(lis->vdd)) {
		dev_info(&client->dev, "vdd regulator control absent\n");
		lis->vdd = NULL;
	}

	if (lis->vdd != NULL) {
		err = regulator_enable(lis->vdd);
		if (err)
			dev_info(&client->dev, "regulator enable fail\n");
		else
			usleep_range(200, 400);
	}


	memset(lis->resume_state, 0, ARRAY_SIZE(lis->resume_state));

	lis->resume_state[0] = ODR_OFF | ENABLE_ALL_AXES;
	lis->resume_state[1] = 0;
	lis->resume_state[2] = ENABLE_AOI1;
	lis->resume_state[3] = 0;
	lis->resume_state[4] = ENABLE_D4D_INT1;

	lis->irq_config[0] = INT_CFG_XY;
	lis->irq_config[1] = lis->pdata->threshold;
	lis->irq_config[2] = lis->pdata->duration;

	err = lis3dh_device_power_on(lis);
	if (err < 0)
		goto err2;

	atomic_set(&lis->enabled, 1);

	err = lis3dh_update_g_range(lis, lis->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto err2;
	}

	err = lis3dh_update_odr(lis, lis->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err2;
	}

	err = lis3dh_input_init(lis);
	if (err < 0)
		goto err3;

	err = devm_gpio_request(&client->dev,
				lis->pdata->irq_gpio, "lis3dh_irq1");
	if (err < 0) {
		pr_err("%s: failed to request lis3dh irq gpio: %d\n",
			 __func__, err);
		goto err3;
	}

	INIT_WORK(&lis->irq_work, lis3dh_irq_work);

	lis->irq = gpio_to_irq(lis->pdata->irq_gpio);
	err = request_irq(lis->irq, lis3dh_isr, IRQF_TRIGGER_RISING,
			 "lis3dh_irq1", lis);
	if (err) {
		pr_err("%s: lis3dh request irq failed: %d\n",
			__func__, err);
		goto err3;
	}

	disable_irq(lis->irq);
	lis3dh_misc_data = lis;

	err = misc_register(&lis3dh_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "lisd_device register failed\n");
		goto err4;
	}

	lis3dh_device_power_off(lis);

	lis->pm_notifier.notifier_call = lis3dh_pm_event;
	err = register_pm_notifier(&lis->pm_notifier);
	if (err < 0) {
		pr_err("%s:Register_pm_notifier failed: %d\n", __func__, err);
		goto err5;
	}

	/* As default, do not report information */
	atomic_set(&lis->enabled, 0);

	mutex_unlock(&lis->lock);

	dev_info(&client->dev, "lis3dh probed\n");

	return 0;

err5:
	misc_deregister(&lis3dh_misc_device);
err4:
	lis3dh_input_cleanup(lis);
err3:
	lis3dh_device_power_off(lis);
err2:
	if (lis->vdd != NULL) {
		regulator_disable(lis->vdd);
		regulator_put(lis->vdd);
	}

	if (lis->pdata->exit)
		lis->pdata->exit();
err1:
	mutex_unlock(&lis->lock);
	kfree(lis);
err0:
	return err;
}

static int __devexit lis3dh_remove(struct i2c_client *client)
{
	struct lis3dh_data *lis = i2c_get_clientdata(client);
	if (lis->vdd != NULL) {
		regulator_disable(lis->vdd);
		regulator_put(lis->vdd);
	}

	unregister_pm_notifier(&lis->pm_notifier);
	misc_deregister(&lis3dh_misc_device);
	lis3dh_input_cleanup(lis);
	lis3dh_device_power_off(lis);
	if (lis->pdata->exit)
		lis->pdata->exit();
	kfree(lis);

	return 0;
}

static const struct i2c_device_id lis3dh_id[] = {
	{NAME, 0},
	{},
};

#ifdef CONFIG_OF
static struct of_device_id lis3dh_match_tbl[] = {
	{ .compatible = "stm,lis3dh" },
	{ },
};
MODULE_DEVICE_TABLE(of, lis3dh_match_tbl);
#endif
MODULE_DEVICE_TABLE(i2c, lis3dh_id);

static struct i2c_driver lis3dh_driver = {
	.driver = {
		.name = NAME,
		.of_match_table = of_match_ptr(lis3dh_match_tbl),
	},
	.probe = lis3dh_probe,
	.remove = __devexit_p(lis3dh_remove),
	.id_table = lis3dh_id,
};

static int __init lis3dh_init(void)
{
	pr_info("LIS3DH accelerometer driver\n");
	return i2c_add_driver(&lis3dh_driver);
}

static void __exit lis3dh_exit(void)
{
	i2c_del_driver(&lis3dh_driver);
	return;
}

module_init(lis3dh_init);
module_exit(lis3dh_exit);

MODULE_DESCRIPTION("lis3dh accelerometer driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
