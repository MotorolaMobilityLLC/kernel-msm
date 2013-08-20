/*
 * Copyright (C) 2011-2013 Motorola Mobility LLC
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
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>

#include <linux/l3g4200d.h>

/** Register map */
#define L3G4200D_WHO_AM_I		0x0f
#define L3G4200D_CTRL_REG1		0x20
#define L3G4200D_CTRL_REG2		0x21
#define L3G4200D_CTRL_REG3		0x22
#define L3G4200D_CTRL_REG4		0x23
#define L3G4200D_CTRL_REG5		0x24

#define L3G4200D_REF_DATA_CAP		0x25
#define L3G4200D_OUT_TEMP		0x26
#define L3G4200D_STATUS_REG		0x27

#define L3G4200D_OUT_X_L		0x28
#define L3G4200D_OUT_X_H		0x29
#define L3G4200D_OUT_Y_L		0x2a
#define L3G4200D_OUT_Y_H		0x2b
#define L3G4200D_OUT_Z_L		0x2c
#define L3G4200D_OUT_Z_H		0x2d

#define L3G4200D_FIFO_CTRL		0x2e
#define L3G4200D_FIFO_SRC		0x2f

#define L3G4200D_INTERRUPT_CFG		0x30
#define L3G4200D_INTERRUPT_SRC		0x31
#define L3G4200D_INTERRUPT_THRESH_X_H	0x32
#define L3G4200D_INTERRUPT_THRESH_X_L	0x33
#define L3G4200D_INTERRUPT_THRESH_Y_H	0x34
#define L3G4200D_INTERRUPT_THRESH_Y_L	0x35
#define L3G4200D_INTERRUPT_THRESH_Z_H	0x36
#define L3G4200D_INTERRUPT_THRESH_Z_L	0x37
#define L3G4200D_INTERRUPT_DURATION	0x38

#define PM_MASK                         0x08
#define ENABLE_ALL_AXES			0x07
#define ODR_MASK	0xF0
#define ODR100_BW25	0x10
#define ODR200_BW70	0x70
#define ODR400_BW110	0xB0
#define ODR800_BW110	0xF0

#define I2C_RETRY_DELAY			5
#define I2C_RETRIES			5
#define AUTO_INCREMENT			0x80
#define L3G4200D_PU_DELAY               320


struct l3g4200d_data {
	struct i2c_client *client;
	struct l3g4200d_platform_data *pdata;
	struct input_dev *input_dev;

	bool hw_initialized;
	bool enabled;
	struct regulator *regulator;
	struct delayed_work enable_work;
	struct mutex mutex;  /* used for all functions */
	struct notifier_block pm_notifier;
};

struct gyro_val {
	s16 x;
	s16 y;
	s16 z;
};

static const struct {
	u8 odr_mask;
	u32 delay_us;
} gyro_odr_table[] = {
	{  ODR800_BW110, 1250  },
	{  ODR400_BW110, 2500  },
	{  ODR200_BW70,  5000  },
	{  ODR100_BW25,  10000 },
};

static uint32_t l3g4200d_debug;
module_param_named(gyro_debug, l3g4200d_debug, uint, 0644);

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct l3g4200d_data *l3g4200d_misc_data;

static int l3g4200d_i2c_read(struct l3g4200d_data *gyro, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = gyro->client->addr,
			.flags = gyro->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = gyro->client->addr,
			.flags = (gyro->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(gyro->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&gyro->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int l3g4200d_i2c_write(struct l3g4200d_data *gyro, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = gyro->client->addr,
			.flags = gyro->client->flags & I2C_M_TEN,
			.len = len + 1,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(gyro->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&gyro->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}
static int l3g4200d_hw_init(struct l3g4200d_data *gyro)
{
	int err = -1;
	u8 buf[8];

	buf[0] = (AUTO_INCREMENT | L3G4200D_CTRL_REG1);
	buf[1] = gyro->pdata->ctrl_reg1 & ~PM_MASK;
	buf[2] = gyro->pdata->ctrl_reg2;
	buf[3] = gyro->pdata->ctrl_reg3;
	buf[4] = gyro->pdata->ctrl_reg4;
	buf[5] = gyro->pdata->ctrl_reg5;
	buf[6] = gyro->pdata->reference;
	err = l3g4200d_i2c_write(gyro, buf, 6);
	if (err < 0)
		return err;

	buf[0] = (L3G4200D_FIFO_CTRL);
	buf[1] = gyro->pdata->fifo_ctrl_reg;
	err = l3g4200d_i2c_write(gyro, buf, 1);
	if (err < 0)
		return err;

	buf[0] = (L3G4200D_INTERRUPT_CFG);
	buf[1] = gyro->pdata->int1_cfg;
	err = l3g4200d_i2c_write(gyro, buf, 1);
	if (err < 0)
		return err;

	buf[0] = (AUTO_INCREMENT | L3G4200D_INTERRUPT_THRESH_X_H);
	buf[1] = gyro->pdata->int1_tsh_xh;
	buf[2] = gyro->pdata->int1_tsh_xl;
	buf[3] = gyro->pdata->int1_tsh_yh;
	buf[4] = gyro->pdata->int1_tsh_yl;
	buf[5] = gyro->pdata->int1_tsh_zh;
	buf[6] = gyro->pdata->int1_tsh_zl;
	buf[7] = gyro->pdata->int1_duration;
	err = l3g4200d_i2c_write(gyro, buf, 7);
	if (err < 0)
		return err;

	gyro->hw_initialized = true;

	return 0;
}

static void l3g4200d_device_power_off(struct l3g4200d_data *gyro)
{
	int err;
	u8 buf[2] = {L3G4200D_CTRL_REG1, 0};

	err = l3g4200d_i2c_read(gyro, buf, 1);
	if (err < 0) {
		dev_err(&gyro->client->dev, "read register control_1 failed\n");
		buf[0] = gyro->pdata->ctrl_reg1;
	}
	buf[1] = buf[0] & ~PM_MASK;
	buf[0] = L3G4200D_CTRL_REG1;

	err = l3g4200d_i2c_write(gyro, buf, 1);
	if (err < 0)
		dev_err(&gyro->client->dev, "soft power off failed\n");
}

static int l3g4200d_device_power_on(struct l3g4200d_data *gyro)
{
	int err;
	u8 buf[2] = {L3G4200D_CTRL_REG1, 0};

	if (!gyro->hw_initialized) {
		err = l3g4200d_hw_init(gyro);
		if (err < 0) {
			l3g4200d_device_power_off(gyro);
			return err;
		}
	}

	err = l3g4200d_i2c_read(gyro, buf, 1);
	if (err < 0) {
		dev_err(&gyro->client->dev, "read register control_1 failed\n");
		buf[0] = gyro->pdata->ctrl_reg1;
	}

	buf[1] = buf[0] | PM_MASK;
	buf[0] = L3G4200D_CTRL_REG1;
	err = l3g4200d_i2c_write(gyro, buf, 1);
	if (err < 0)
		dev_err(&gyro->client->dev, "soft power on failed\n");

	return 0;
}

static void l3g4200d_device_suspend(struct l3g4200d_data *gyro, int suspend)
{
	int err;
	u8 buf[2] = {L3G4200D_CTRL_REG1, 0};

	err = l3g4200d_i2c_read(gyro, buf, 1);
	if (err < 0) {
		dev_err(&gyro->client->dev, "read register control_1 failed\n");
		return;
	}

	if (suspend)
		buf[1] = buf[0] & ~ENABLE_ALL_AXES;
	else
		buf[1] = buf[0] | ENABLE_ALL_AXES;

	buf[0] = L3G4200D_CTRL_REG1;

	err = l3g4200d_i2c_write(gyro, buf, 1);
	if (err < 0)
		dev_err(&gyro->client->dev, "suspend %d failed\n", suspend);
}

static int l3g4200d_get_gyro_data(struct l3g4200d_data *gyro,
					 struct gyro_val *data)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 gyro_data[6] = {0, 0, 0, 0, 0, 0};

	gyro_data[0] = (AUTO_INCREMENT | L3G4200D_OUT_X_L);
	err = l3g4200d_i2c_read(gyro, gyro_data, 6);
	if (err < 0)
		return err;

	data->x = (gyro_data[1] << 8) | gyro_data[0];
	data->y = (gyro_data[3] << 8) | gyro_data[2];
	data->z = (gyro_data[5] << 8) | gyro_data[4];

	return 0;
}

static void l3g4200d_report_values(struct l3g4200d_data *gyro,
					 struct gyro_val *data)
{
	input_report_rel(gyro->input_dev, REL_RX, data->x);
	input_report_rel(gyro->input_dev, REL_RY, data->y);
	input_report_rel(gyro->input_dev, REL_RZ, data->z);
	input_sync(gyro->input_dev);

	if (l3g4200d_debug)
		pr_info("l3g4200d: x: %3d, y: %3d, z: %3d\n",
			 data->x, data->y, data->z);
}

static irqreturn_t gyro_irq_thread(int irq, void *dev)
{
	struct l3g4200d_data *gyro = dev;
	struct gyro_val data;
	int err;

	mutex_lock(&gyro->mutex);

	err = l3g4200d_get_gyro_data(gyro, &data);
	if (err < 0)
		dev_err(&gyro->client->dev, "get_gyro_data failed\n");
	else
		l3g4200d_report_values(gyro, &data);

	mutex_unlock(&gyro->mutex);

	return IRQ_HANDLED;
}

static int l3g4200d_flush_gyro_data(struct l3g4200d_data *gyro)
{
	struct gyro_val data;
	int i;

	for (i = 0; i < 5; i++) {
		if (gpio_get_value(gyro->pdata->gpio_drdy))
			l3g4200d_get_gyro_data(gyro, &data);
		else
			return 0;
	}
	return -EIO;
}

static void l3g4200d_enable_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct l3g4200d_data *gyro =
		container_of(dwork, struct l3g4200d_data, enable_work);

	if (l3g4200d_debug)
		pr_info("%s\n", __func__);
	l3g4200d_flush_gyro_data(gyro);
	enable_irq(gyro->client->irq);
}

static int l3g4200d_enable(struct l3g4200d_data *gyro)
{
	int err;

	if (!gyro->enabled) {
		if (l3g4200d_debug)
			pr_info("%s\n", __func__);
		if (gyro->regulator) {
			err = regulator_enable(gyro->regulator);
			if (err < 0)
				goto err0;
		}

		err = l3g4200d_device_power_on(gyro);
		if (err < 0) {
			dev_err(&gyro->client->dev,
				" l3g4200d_device_power_on failed\n");
			if (gyro->regulator) {
				regulator_disable(gyro->regulator);
				gyro->hw_initialized = false;
			}
			goto err0;
		}

		/* do not report noise at IC power-up
		 * flush data before enabling irq */
		schedule_delayed_work(&gyro->enable_work,
			msecs_to_jiffies(L3G4200D_PU_DELAY));
		gyro->enabled = true;
	}
	return 0;
err0:
	return err;
}

static int l3g4200d_disable(struct l3g4200d_data *gyro)
{
	if (gyro->enabled) {
		if (l3g4200d_debug)
			pr_info("%s\n", __func__);
		if (!cancel_delayed_work_sync(&gyro->enable_work))
			disable_irq_nosync(gyro->client->irq);
		l3g4200d_device_power_off(gyro);
		if (gyro->regulator) {
			regulator_disable(gyro->regulator);
			gyro->hw_initialized = false;
		}
		gyro->enabled = false;
	}
	return 0;
}

static int l3g4200d_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = l3g4200d_misc_data;

	return 0;
}

static int l3g4200d_set_delay(struct l3g4200d_data *gyro, u32 delay_ms)
{
	int odr_value = ODR100_BW25;
	int err = -1;
	int i;
	u32 delay_us;
	u8 buf[2] = {L3G4200D_CTRL_REG1, 0};

	/* do not report noise during ODR update */
	disable_irq_nosync(gyro->client->irq);

	delay_us = delay_ms * USEC_PER_MSEC;
	for (i = 0; i < ARRAY_SIZE(gyro_odr_table); i++)
		if (delay_us <= gyro_odr_table[i].delay_us) {
			odr_value = gyro_odr_table[i].odr_mask;
			delay_us = gyro_odr_table[i].delay_us;
			break;
		}

	if (delay_us >= gyro_odr_table[3].delay_us) {
		odr_value = gyro_odr_table[3].odr_mask;
		delay_us = gyro_odr_table[3].delay_us;
	}

	if (odr_value != (gyro->pdata->ctrl_reg1 & ODR_MASK)) {
		buf[1] = (gyro->pdata->ctrl_reg1 & ~ODR_MASK);
		buf[1] |= odr_value;
		gyro->pdata->ctrl_reg1 = buf[1];
		err = l3g4200d_i2c_write(gyro, buf, 1);
		if (err < 0)
			goto error;
	}

	gyro->pdata->poll_interval = delay_us / USEC_PER_MSEC;
	/* noisy data upto 6/ODR */
	msleep((delay_us * 6) / USEC_PER_MSEC);

	l3g4200d_flush_gyro_data(gyro);
	enable_irq(gyro->client->irq);

	return 0;

error:
	dev_err(&gyro->client->dev, "update odr failed 0x%x,0x%x: %d\n",
		buf[0], buf[1], err);

	return err;
}

static long l3g4200d_misc_ioctl_locked(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int interval;
	int err = -1;
	struct l3g4200d_data *gyro = file->private_data;

	if (l3g4200d_debug)
		pr_info("%s cmd=0x%x\n", __func__, cmd);
	switch (cmd) {
	case L3G4200D_IOCTL_GET_DELAY:
		interval = gyro->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case L3G4200D_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		err = l3g4200d_set_delay(gyro, interval);
		if (err < 0)
			return err;
		break;

	case L3G4200D_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval > 1)
			return -EINVAL;

		if (interval)
			l3g4200d_enable(gyro);
		else
			l3g4200d_disable(gyro);

		break;

	case L3G4200D_IOCTL_GET_ENABLE:
		interval = gyro->enabled;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static long l3g4200d_misc_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret;
	struct l3g4200d_data *gyro = file->private_data;

	mutex_lock(&gyro->mutex);

	ret = l3g4200d_misc_ioctl_locked(file, cmd, arg);

	mutex_unlock(&gyro->mutex);

	return ret;
}

static const struct file_operations l3g4200d_misc_fops = {
	.owner = THIS_MODULE,
	.open = l3g4200d_misc_open,
	.unlocked_ioctl = l3g4200d_misc_ioctl,
};

static struct miscdevice l3g4200d_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = L3G4200D_NAME,
	.fops = &l3g4200d_misc_fops,
};

static int l3g4200d_input_init(struct l3g4200d_data *gyro)
{
	int err;

	gyro->input_dev = input_allocate_device();
	if (!gyro->input_dev) {
		err = -ENOMEM;
		dev_err(&gyro->client->dev, "input device allocate failed\n");
		goto err0;
	}

	input_set_drvdata(gyro->input_dev, gyro);

	input_set_capability(gyro->input_dev, EV_REL, REL_RX);
	input_set_capability(gyro->input_dev, EV_REL, REL_RY);
	input_set_capability(gyro->input_dev, EV_REL, REL_RZ);

	gyro->input_dev->name = "gyroscope";

	err = input_register_device(gyro->input_dev);
	if (err) {
		dev_err(&gyro->client->dev,
			"unable to register input polled device %s\n",
			gyro->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(gyro->input_dev);
err0:
	return err;
}

static void l3g4200d_input_cleanup(struct l3g4200d_data *gyro)
{
	input_unregister_device(gyro->input_dev);
}

static int l3g4200d_resume(struct l3g4200d_data *gyro)
{
	if (gyro->enabled) {
		l3g4200d_device_suspend(gyro, 0);
		l3g4200d_flush_gyro_data(gyro);
		enable_irq(gyro->client->irq);
	}
	return 0;
}

static int l3g4200d_suspend(struct l3g4200d_data *gyro)
{
	if (gyro->enabled) {
		disable_irq_nosync(gyro->client->irq);
		l3g4200d_device_suspend(gyro, 1);
	}
	return 0;
}

static int l3g4200d_pm_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	struct l3g4200d_data *gyro = container_of(this,
		struct l3g4200d_data, pm_notifier);

	mutex_lock(&gyro->mutex);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		if (l3g4200d_debug)
			pr_info("%s: suspend\n", __func__);
		l3g4200d_suspend(gyro);
		break;
	case PM_POST_SUSPEND:
		if (l3g4200d_debug)
			pr_info("%s: resume\n", __func__);
		l3g4200d_resume(gyro);
		break;
	}

	mutex_unlock(&gyro->mutex);

	return NOTIFY_DONE;
}

static int l3g4200d_gpio_init(struct l3g4200d_platform_data *pdata)
{
	int err;

	err = gpio_request(pdata->gpio_drdy, "gyro drdy");
	if (err) {
		pr_err("Fail to request l3g4200d drdy gpio, err = %d\n", err);
		return err;
	}
	gpio_direction_input(pdata->gpio_drdy);

	return 0;
}

static void l3g4200d_gpio_free(struct l3g4200d_platform_data *pdata)
{
	gpio_free(pdata->gpio_drdy);
}

#ifdef CONFIG_OF
static struct l3g4200d_platform_data *
l3g4200d_of_init(struct i2c_client *client)
{
	struct l3g4200d_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	u32 val;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure.\n");
		return NULL;
	}

	if (!of_property_read_u32(np, "stm,poll_interval", &val))
		pdata->poll_interval = (int) val;

	pdata->gpio_drdy = of_get_gpio(np, 0);

	/* According the spec to configure following register */
	pdata->ctrl_reg1 = 0x1F;
	pdata->ctrl_reg2 = 0x00;
	pdata->ctrl_reg3 = 0x08;
	pdata->ctrl_reg4 = 0xA0;
	pdata->ctrl_reg5 = 0x00;
	pdata->reference = 0x00;
	pdata->fifo_ctrl_reg = 0x00;
	pdata->int1_cfg = 0x00;
	pdata->int1_tsh_xh = 0x00;
	pdata->int1_tsh_xl = 0x00;
	pdata->int1_tsh_yh = 0x00;
	pdata->int1_tsh_yl = 0x00;
	pdata->int1_tsh_zh = 0x00;
	pdata->int1_tsh_zl = 0x00;
	pdata->int1_duration = 0x00;

	return pdata;
}
#else
static struct l3g4200d_platform_data *
l3g4200d_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int l3g4200d_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct l3g4200d_data *gyro;
	struct l3g4200d_platform_data *pdata;
	int err = -1;

	pr_err("%s:Enter\n", __func__);

	if (client->dev.of_node)
		pdata = l3g4200d_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "pdata is NULL. exiting.\n");
		err = -ENODEV;
		goto err0;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}

	err = l3g4200d_gpio_init(pdata);
	if (err) {
		dev_err(&client->dev, "l3g4200 gyro gpio init failed.\n");
		goto err0;
	}

	gyro = kzalloc(sizeof(*gyro), GFP_KERNEL);
	if (gyro == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err1;
	}

	gyro->client = client;
	gyro->pdata = pdata;

	/*
	 * Use drdy pin as interrupt pin. If we want to use the INT1,
	 * we need to ask verdor to give the new register values.
	 */
	gyro->client->irq = gpio_to_irq(gyro->pdata->gpio_drdy);

	gyro->regulator = regulator_get(&client->dev, "vdd");

	if (IS_ERR_OR_NULL(gyro->regulator)) {
		dev_err(&client->dev, "unable to get regulator\n");
		gyro->regulator = NULL;
	}

	i2c_set_clientdata(client, gyro);

	/* As default, do not report information */
	gyro->enabled = false;
	gyro->hw_initialized = false;
	INIT_DELAYED_WORK(&gyro->enable_work, l3g4200d_enable_work_func);

	err = l3g4200d_input_init(gyro);
	if (err < 0)
		goto err3;

	mutex_init(&gyro->mutex);

	l3g4200d_misc_data = gyro;

	err = misc_register(&l3g4200d_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "l3g4200d_device register failed\n");
		goto err4;
	}

	err = request_threaded_irq(gyro->client->irq, NULL,
		gyro_irq_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		L3G4200D_NAME, gyro);
	if (err != 0) {
		pr_err("%s: irq request failed: %d\n", __func__, err);
		err = -ENODEV;
		goto err5;
	}
	disable_irq(gyro->client->irq);

	gyro->pm_notifier.notifier_call = l3g4200d_pm_event;
	err = register_pm_notifier(&gyro->pm_notifier);
	if (err < 0) {
		pr_err("%s:Register_pm_notifier failed: %d\n", __func__, err);
		goto err6;
	}

	pr_info("%s:Gyro probed\n", __func__);
	return 0;

err6:
	free_irq(gyro->client->irq, gyro);
err5:
	misc_deregister(&l3g4200d_misc_device);
err4:
	mutex_destroy(&gyro->mutex);
	l3g4200d_input_cleanup(gyro);
err3:
	if (gyro->regulator)
		regulator_put(gyro->regulator);
	kfree(gyro);
err1:
	l3g4200d_gpio_free(pdata);
err0:
	return err;
}

static int __devexit l3g4200d_remove(struct i2c_client *client)
{
	struct l3g4200d_data *gyro = i2c_get_clientdata(client);

	misc_deregister(&l3g4200d_misc_device);
	mutex_destroy(&gyro->mutex);
	l3g4200d_input_cleanup(gyro);
	l3g4200d_disable(gyro);
	if (gyro->regulator)
		regulator_put(gyro->regulator);
	unregister_pm_notifier(&gyro->pm_notifier);
	l3g4200d_gpio_free(gyro->pdata);
	kfree(gyro->pdata);
	kfree(gyro);

	return 0;
}

static const struct i2c_device_id l3g4200d_id[] = {
	{L3G4200D_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, l3g4200d_id);

#ifdef CONFIG_OF
static struct of_device_id l3g4200d_match_tbl[] = {
		{.compatible = "stm,l3g4200d"},
		{},
};
MODULE_DEVICE_TABLE(of, l3g4200d_match_tbl);
#endif

static struct i2c_driver l3g4200d_driver = {
	.driver = {
		.name = L3G4200D_NAME,
		.of_match_table = of_match_ptr(l3g4200d_match_tbl),
		   },
	.probe = l3g4200d_probe,
	.remove = __devexit_p(l3g4200d_remove),
	.id_table = l3g4200d_id,
};

static int __init l3g4200d_init(void)
{
	pr_info("L3G4200D gyroscope driver\n");
	return i2c_add_driver(&l3g4200d_driver);
}

static void __exit l3g4200d_exit(void)
{
	i2c_del_driver(&l3g4200d_driver);
	return;
}

module_init(l3g4200d_init);
module_exit(l3g4200d_exit);

MODULE_DESCRIPTION("l3g4200d gyroscope driver");
MODULE_AUTHOR("Motorola Mobility");
MODULE_LICENSE("GPL");
