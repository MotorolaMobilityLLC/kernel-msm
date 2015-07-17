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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/tsl2584.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#define TSL2584_I2C_RETRIES	2
#define TSL2584_I2C_RETRY_DELAY	10

#define TSL2584_COMMAND_SELECT			0x80
#define TSL2584_COMMAND_AUTO_INCREMENT		0x20
#define TSL2584_COMMAND_SPECIAL_FUNCTION	0x60

#define TSL2584_COMMAND_ALS_INT_CLEAR		0x01

/* TSL2584 Registers */
#define TSL2584_CONTROL		0x00
#define TSL2584_CONTROL_ADC_INTR	(1<<5)
#define TSL2584_CONTROL_ADC_VALID	(1<<4)
#define TSL2584_CONTROL_ADC_EN		(1<<1)
#define TSL2584_CONTROL_POWER		(1<<0)

#define TSL2584_ITIME		0x01

#define TSL2584_INTR		0x02
#define TSL2584_INTR_STOP		(1<<6)
#define TSL2584_INTR_LEVEL		(1<<4)
#define TSL2584_INTR_INTR_MASK		0x30
#define TSL2584_INTR_PERSIST_MASK	0x0F


#define TSL2584_THLLOW		0x03
#define TSL2584_THLHIGH		0x04
#define TSL2584_THHLOW		0x05
#define TSL2584_THHHIGH		0x06

#define TSL2584_ANALOG		0x07
#define TSL2584_ANALOG_AGAIN_1X		0x00
#define TSL2584_ANALOG_AGAIN_8X		0x01
#define TSL2584_ANALOG_AGAIN_16X	0x02
#define TSL2584_ANALOG_AGAIN_111X	0x03

#define TSL2584_ID		0x12
#define TSL2584_ID_PARTNO_MASK		0xF0
#define TSL2584_ID_REVNO_MASK		0x0F

#define TSL2584_DATA0LOW	0x14
#define TSL2584_DATA0HIGH	0x15
#define TSL2584_DATA1LOW	0x16
#define TSL2584_DATA1HIGH	0x17

#define TSL2584_TIMERLOW	0x18
#define TSL2584_TIMERHIGH	0x19

#define TSL2584_ID2		0x1E
#define TSL2584_ID2_MASK		0x80

#define TSL2584_C0DATA_MAX	0xFFFF

#define TSL2584_ALS_LOW_TO_HIGH_THRESHOLD	200	/* 200 lux */
#define TSL2584_ALS_HIGH_TO_LOW_THRESHOLD	100	/* 100 lux */

#define TSL2584_ALS_IRQ_DELTA_PERCENT	5

#define TSL2584_ALS_LUX1_CH0_COEFF_DEFAULT	1000
#define TSL2584_ALS_LUX1_CH1_COEFF_DEFAULT	2005
#define TSL2584_ALS_LUX2_CH0_COEFF_DEFAULT	604
#define TSL2584_ALS_LUX2_CH1_COEFF_DEFAULT	1090

#define TSL2584_ALS_HIGH_LUX_DEN	128
#define TSL2584_ALS_LOW_LUX_DEN		(TSL2584_ALS_HIGH_LUX_DEN * 8)

static s32 lux1ch0_coeff = TSL2584_ALS_LUX1_CH0_COEFF_DEFAULT;
static s32 lux1ch1_coeff = TSL2584_ALS_LUX1_CH1_COEFF_DEFAULT;
static s32 lux2ch0_coeff = TSL2584_ALS_LUX2_CH0_COEFF_DEFAULT;
static s32 lux2ch1_coeff = TSL2584_ALS_LUX2_CH1_COEFF_DEFAULT;

enum tsl2584_als_mode {
	TSL2584_ALS_MODE_LOW_LUX,
	TSL2584_ALS_MODE_HIGH_LUX,
};

struct tsl2584_data {
	struct input_dev *dev;
	struct i2c_client *client;
	struct regulator *regulator;
	struct delayed_work dwork;
	struct workqueue_struct *workqueue;

	struct tsl2584_platform_data *pdata;
	struct miscdevice miscdevice;
	struct notifier_block pm_notifier;
	struct mutex mutex;
	struct wake_lock wl_to;
	struct wake_lock wl_isr;
	/* state flags */
	atomic_t power_on;
	atomic_t als_enabled;
	enum tsl2584_als_mode als_mode;
	/* numeric values */
	unsigned int als_apers;
	unsigned int als_low_threshold;
	unsigned int als_high_threshold;
	u8 ink_type;
};

static struct tsl2584_data *tsl2584_misc_data;
static struct tsl2584_reg {
	const char *name;
	u8 reg;
} tsl2584_regs[] = {
	{ "CONTROL",	TSL2584_CONTROL },	/* same as enable */
	{ "ITIME",	TSL2584_ITIME },	/* same as ATIME */
	{ "INTR",	TSL2584_INTR },
	{ "THLLOW",	TSL2584_THLLOW },
	{ "THLHIGH",	TSL2584_THLHIGH },
	{ "THHLOW",	TSL2584_THHLOW },
	{ "THHHIGH",	TSL2584_THHHIGH },
	{ "ANALOG",	TSL2584_ANALOG },
	{ "ID",		TSL2584_ID },
	{ "DATA0LOW",	TSL2584_DATA0LOW },
	{ "DATA0HIGH",	TSL2584_DATA0HIGH },
	{ "DATA1LOW",	TSL2584_DATA1LOW },
	{ "DATA1HIGH",	TSL2584_DATA1HIGH },
	{ "TIMERLOW",	TSL2584_TIMERLOW },
	{ "TIMERHIGH",	TSL2584_TIMERHIGH },
	{ "ID2",	TSL2584_ID2 },
};

#define TSL2584_DBG_INPUT		0x00000001
#define TSL2584_DBG_POWER_ON_OFF	0x00000002
#define TSL2584_DBG_ENABLE_DISABLE	0x00000004
#define TSL2584_DBG_IOCTL		0x00000008
#define TSL2584_DBG_SUSPEND_RESUME	0x00000010
static u32 tsl2584_debug = 0x00000000;
module_param_named(debug_mask, tsl2584_debug, uint, 0644);

static int tsl2584_i2c_read(struct tsl2584_data *tsl, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = tsl->client->addr,
			.flags = tsl->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = tsl->client->addr,
			.flags = (tsl->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	buf[0] |= TSL2584_COMMAND_SELECT;
	do {
		err = i2c_transfer(tsl->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(TSL2584_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < TSL2584_I2C_RETRIES));

	if (err != 2) {
		dev_err(&tsl->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tsl2584_i2c_write(struct tsl2584_data *tsl, u8 *buf, int len)
{
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = tsl->client->addr,
			.flags = tsl->client->flags & I2C_M_TEN,
			.len = len,
			.buf = buf,
		},
	};

	buf[0] |= TSL2584_COMMAND_SELECT;
	do {
		err = i2c_transfer(tsl->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(TSL2584_I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < TSL2584_I2C_RETRIES));

	if (err != 1) {
		dev_err(&tsl->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


static int tsl2584_device_power(struct tsl2584_data *tsl, bool on)
{
	int error;
	u8 reg_data[2] = {0x00, 0x00};

	reg_data[0] = TSL2584_CONTROL;
	if (on)
		reg_data[1] = TSL2584_CONTROL_POWER;
	else
		reg_data[1] = 0x00;
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error) {
		dev_err(&tsl->client->dev,
			"Error: control power write failed: %d\n", error);
		return error;
	}

	if (on)
		usleep_range(3000, 3100);

	if (tsl2584_debug & TSL2584_DBG_ENABLE_DISABLE)
		dev_info(&tsl->client->dev, "writing CONTROL=0x%02x\n",
			 reg_data[1]);

	return error;
}

static void tsl2584_write_als_thresholds(struct tsl2584_data *tsl)
{
	u8 reg_data[5] = {0};
	unsigned int ailt = tsl->als_low_threshold;
	unsigned int aiht = tsl->als_high_threshold;
	int error;

	reg_data[0] = (TSL2584_THLLOW | TSL2584_COMMAND_AUTO_INCREMENT);
	reg_data[1] = (ailt & 0xFF);
	reg_data[2] = ((ailt >> 8) & 0xFF);
	reg_data[3] = (aiht & 0xFF);
	reg_data[4] = ((aiht >> 8) & 0xFF);

	error = tsl2584_i2c_write(tsl, reg_data, 5);
	if (error < 0)
		dev_err(&tsl->client->dev,
			"Error: Error writing new ALS thresholds: %d\n", error);
}

static void tsl2584_als_mode_low_lux(struct tsl2584_data *tsl)
{
	int error;
	u8 reg_data[2] = {0};

	/* write ALS gain = 8 */
	reg_data[0] = TSL2584_ANALOG;
	reg_data[1] = TSL2584_ANALOG_AGAIN_8X;
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev, "error writing ALS gain: %d\n",
			error);
		return;
	}

	/* write ALS integration time = ~49 ms */
	reg_data[0] = TSL2584_ITIME;
	reg_data[1] = 0xEE;
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev, "error writing ATIME: %d\n", error);
		return;
	}

	tsl->als_mode = TSL2584_ALS_MODE_LOW_LUX;
	tsl->als_low_threshold = TSL2584_C0DATA_MAX - 1;
	tsl->als_high_threshold = TSL2584_C0DATA_MAX;
	tsl2584_write_als_thresholds(tsl);
}

static void tsl2584_als_mode_high_lux(struct tsl2584_data *tsl)
{
	int error;
	u8 reg_data[2] = {0};

	/* write ALS gain = 1 */
	reg_data[0] = TSL2584_ANALOG;
	reg_data[1] = TSL2584_ANALOG_AGAIN_1X;
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev,
			"error writing ALS gain: %d\n", error);
		return;
	}

	/* write ALS integration time = ~49 ms */
	reg_data[0] = TSL2584_ITIME;
	reg_data[1] = 0xEE;
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev, "error writing ATIME: %d\n", error);
		return;
	}

	tsl->als_mode = TSL2584_ALS_MODE_HIGH_LUX;
	tsl->als_low_threshold = TSL2584_C0DATA_MAX - 1;
	tsl->als_high_threshold = TSL2584_C0DATA_MAX;
	tsl2584_write_als_thresholds(tsl);
}

static int tsl2584_device_init(struct tsl2584_data *tsl)
{
	int error = 0;
	u8 reg_data[2] = {0};

	/* write ALS integration time */
	reg_data[0] = TSL2584_ITIME;
	reg_data[1] = 0xEE; /* 49 ms */
	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev, "Error: %d\n", error);
		return error;
	}

	/* write ALS interrupt persistence */
	reg_data[0] = TSL2584_INTR;
	if (tsl->pdata->irq)
		reg_data[1] = TSL2584_INTR_LEVEL | tsl->als_apers;
	else
	reg_data[1] = tsl->als_apers;

	error = tsl2584_i2c_write(tsl, reg_data, 2);
	if (error < 0) {
		dev_err(&tsl->client->dev, "I2C write error: %d\n", error);
		return error;
	}

	tsl->als_mode = TSL2584_ALS_MODE_LOW_LUX;
	tsl2584_als_mode_low_lux(tsl);

	return 0;
}

static void tsl2584_power_off(struct tsl2584_data *tsl)
{
	if (atomic_cmpxchg(&tsl->power_on, 1, 0)) {
		dev_dbg(&tsl->client->dev, "%s:\n", __func__);
		tsl2584_device_power(tsl, 0);
		if (!IS_ERR_OR_NULL(tsl->regulator))
			regulator_disable(tsl->regulator);
	}
}

static int tsl2584_power_on(struct tsl2584_data *tsl)
{
	int error;

	if (!atomic_cmpxchg(&tsl->power_on, 0, 1)) {
		dev_dbg(&tsl->client->dev, "%s:\n", __func__);

		if (!IS_ERR_OR_NULL(tsl->regulator)) {
			error = regulator_enable(tsl->regulator);
			if (error) {
				dev_err(&tsl->client->dev,
					"regulator_enable failed: %d\n", error);
				atomic_set(&tsl->power_on, 0);
				return error;
			}
		}

		usleep(5000);

		error = tsl2584_device_power(tsl, 1);
		if (error) {
			atomic_set(&tsl->power_on, 0);
			return error;
		}
	}

	return 0;
}

static void tsl2584_check_als_range(struct tsl2584_data *ct, unsigned int lux)
{
	if (ct->als_mode == TSL2584_ALS_MODE_LOW_LUX) {
		if (lux >= TSL2584_ALS_LOW_TO_HIGH_THRESHOLD)
			tsl2584_als_mode_high_lux(ct);
	} else if (ct->als_mode == TSL2584_ALS_MODE_HIGH_LUX) {
		if (lux < TSL2584_ALS_HIGH_TO_LOW_THRESHOLD)
			tsl2584_als_mode_low_lux(ct);
	}
}

static int tsl2584_als_enable(struct tsl2584_data *tsl)
{
	int error;
	u8 reg_data[2] = {0};

	if (!atomic_cmpxchg(&tsl->als_enabled, 0, 1)) {
		if (tsl2584_power_on(tsl) != 0) {
			dev_err(&tsl->client->dev, "Power on failure\n");
			goto clear_state;
		}
		if (tsl2584_device_init(tsl) != 0) {
			dev_err(&tsl->client->dev, "Deveice init failure\n");
			goto power_off;
		}
		/* write ALS interrupt persistence */
		reg_data[0] = TSL2584_INTR;
		if (tsl->pdata->irq)
			reg_data[1] =  TSL2584_INTR_LEVEL | tsl->als_apers;
		else
			reg_data[1] =  tsl->als_apers;
		error = tsl2584_i2c_write(tsl, reg_data, 2);
		if (error < 0) {
			dev_err(&tsl->client->dev, "I2C write error: %d\n",
				error);
			goto power_off;
		}

		/* ADC Enable */
		reg_data[0] = TSL2584_CONTROL;
		reg_data[1] = TSL2584_CONTROL_ADC_EN | TSL2584_CONTROL_POWER;
		error = tsl2584_i2c_write(tsl, reg_data, 2);
		if (error < 0) {
			dev_err(&tsl->client->dev, "I2C write error  %d\n",
				error);
			goto power_off;
		}

		if (!tsl->pdata->irq)
			queue_delayed_work(tsl->workqueue, &tsl->dwork, 0);

	}
	return 0;
power_off:
	tsl2584_power_off(tsl);
clear_state:
	atomic_set(&tsl->als_enabled, 0);
	return error;
}

static int tsl2584_als_disable(struct tsl2584_data *tsl)
{
	int error = 0;
	u8 reg_data[2] = {0};

	cancel_delayed_work_sync(&tsl->dwork);
	if (atomic_cmpxchg(&tsl->als_enabled, 1, 0)) {
		/* ADC Disable */
		reg_data[0] = TSL2584_CONTROL;
		reg_data[1] = TSL2584_CONTROL_POWER;
		error = tsl2584_i2c_write(tsl, reg_data, 2);
		if (error < 0)
			dev_err(&tsl->client->dev, "I2C write error  %d\n",
				error);
		tsl2584_power_off(tsl);
	}
	if (wake_lock_active(&tsl->wl_isr)) {
		wake_unlock(&tsl->wl_isr);
		if (tsl->client->irq) {
			/* Need to re-enable irq if delayed work was canceled */
			if (tsl2584_debug & TSL2584_DBG_ENABLE_DISABLE)
				dev_info(&tsl->client->dev, "enable irq\n");
			enable_irq(tsl->client->irq);
		}
	}

	return error;
}

static void tsl2584_report_als(struct tsl2584_data *ct)
{
	int error;
	u8 reg_data[4] = {0};
	int c0data;
	int c1data;
	unsigned int ratio;
	int lux1 = 0;
	int lux2 = 0;
	unsigned int threshold_delta;
	struct timespec ts;

	reg_data[0] = (TSL2584_DATA0LOW | TSL2584_COMMAND_AUTO_INCREMENT);
	error = tsl2584_i2c_read(ct, reg_data, 4);
	if (error < 0)
		return;

	c0data = (reg_data[1] << 8) | reg_data[0];
	c1data = (reg_data[3] << 8) | reg_data[2];
	if (tsl2584_debug & TSL2584_DBG_INPUT)
		dev_info(&ct->client->dev, "C0DATA = %d, C1DATA = %d\n",
			 c0data, c1data);

	threshold_delta = c0data * TSL2584_ALS_IRQ_DELTA_PERCENT / 100;
	if (threshold_delta == 0)
		threshold_delta = 1;
	if (c0data > threshold_delta)
		ct->als_low_threshold = c0data - threshold_delta;
	else
		ct->als_low_threshold = 0;
	ct->als_high_threshold = c0data + threshold_delta;
	if (ct->als_high_threshold > TSL2584_C0DATA_MAX)
		ct->als_high_threshold = TSL2584_C0DATA_MAX;
	tsl2584_write_als_thresholds(ct);


	if (ct->ink_type == 0) {
		/* calculate lux using piecewise function from TAOS */
		if (c0data == 0)
			c0data = 1;
		ratio = ((100 * c1data) + c0data - 1) / c0data;
		switch (ct->als_mode) {
		case TSL2584_ALS_MODE_LOW_LUX:
			if (c0data == 0x4800 || c1data == 0x4800)
				lux1 = TSL2584_ALS_LOW_TO_HIGH_THRESHOLD;
			else if (ratio <= 51)
				lux1 = (121*c0data - 227*c1data) / 100;
			else if (ratio <= 58)
				lux1 = (40*c0data - 68*c1data) / 100;
			else
				lux1 = 0;
			break;
		case TSL2584_ALS_MODE_HIGH_LUX:
			if (c0data == 0x4800 || c1data == 0x4800)
				lux1 = 0xFFFF;
			else if (ratio <= 51)
				lux1 = (964*c0data - 1818*c1data) / 100;
			else if (ratio <= 58)
				lux1 = (317*c0data - 544*c1data) / 100;
			else
				lux1 = 0;
			break;
		default:
			dev_err(&ct->client->dev, "ALS mode is %d!\n",
				ct->als_mode);
		}
	} else {
		switch (ct->als_mode) {
		case TSL2584_ALS_MODE_LOW_LUX:
			lux1 = ((lux1ch0_coeff * c0data)
				- (lux1ch1_coeff * c1data))
				/ TSL2584_ALS_LOW_LUX_DEN;
			if (lux1 < 0)
				lux1 = 0;
			lux2 = ((lux2ch0_coeff * c0data)
				- (lux2ch1_coeff * c1data))
				/ TSL2584_ALS_LOW_LUX_DEN;
			if (lux2 < 0)
				lux2 = 0;
			if (lux1 < lux2)
				lux1 = lux2;
			break;
		case TSL2584_ALS_MODE_HIGH_LUX:
			lux1 = ((lux1ch0_coeff * c0data)
				- (lux1ch1_coeff * c1data))
				/ TSL2584_ALS_HIGH_LUX_DEN;
			if (lux1 < 0)
				lux1 = 0;
			lux2 = ((lux2ch0_coeff * c0data)
				- (lux2ch1_coeff * c1data))
				/ TSL2584_ALS_HIGH_LUX_DEN;
			if (lux2 < 0)
				lux2 = 0;
			if (lux1 < lux2)
				lux1 = lux2;
			break;
		default:
			dev_err(&ct->client->dev, "ALS mode is %d!\n",
				ct->als_mode);
		}
	}

	/* input.c filters consecutive LED_MISC values <=1. */
	lux1 = (lux1 >= 2) ? lux1 : 2;

	if (tsl2584_debug & TSL2584_DBG_INPUT)
		dev_info(&ct->client->dev, "LUX = %d\n", lux1);

	get_monotonic_boottime(&ts);

	input_event(ct->dev, EV_MSC, MSC_TIMESTAMP, ts.tv_sec);
	input_event(ct->dev, EV_MSC, MSC_TIMESTAMP, ts.tv_nsec);
	input_event(ct->dev, EV_MSC, MSC_RAW, lux1);
	input_sync(ct->dev);
	tsl2584_check_als_range(ct, lux1);

}

static int tsl2584_misc_open(struct inode *inode, struct file *file)
{
	int err;

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = tsl2584_misc_data;

	return 0;
}

static const struct file_operations tsl2584_misc_fops = {
	.owner = THIS_MODULE,
	.open = tsl2584_misc_open,
};

static int tsl2584_als_enable_param;

static int tsl2584_set_als_enable_param(const char *char_value,
	struct kernel_param *kp)
{
	unsigned long int_value;
	int  ret;
	struct tsl2584_data *tsl = tsl2584_misc_data;

	if (!char_value)
		return -EINVAL;

	if (!tsl2584_misc_data)
		return -EINVAL;

	ret = strict_strtoul(char_value, (unsigned int)0, &int_value);
	if (ret)
		return ret;

	*((int *)kp->arg) = int_value;

	mutex_lock(&tsl2584_misc_data->mutex);

	if (int_value != 0)
		ret = tsl2584_als_enable(tsl);
	else
		ret = tsl2584_als_disable(tsl);

	mutex_unlock(&tsl2584_misc_data->mutex);

	return ret;
}

static int tsl2584_get_als_enable_param(char *buffer, struct kernel_param *kp)
{
	int num_chars;
	struct tsl2584_data *tsl = tsl2584_misc_data;

	if (!buffer)
		return -EINVAL;

	if (!tsl) {
		scnprintf(buffer, PAGE_SIZE, "0\n");
		return 1;
	}

	num_chars = scnprintf(buffer, 2, "%d\n",
		atomic_read(&tsl->als_enabled));

	return num_chars;
}

module_param_call(als_enable, tsl2584_set_als_enable_param,
	tsl2584_get_als_enable_param, &tsl2584_als_enable_param, 0644);
MODULE_PARM_DESC(als_enable, "enable/disable the ALS.");

static ssize_t tsl2584_registers_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct tsl2584_data *ct = i2c_get_clientdata(client);
	int error = 0;
	unsigned int i, n, reg_count;
	u8 reg_data[1] = {0};

	reg_count = sizeof(tsl2584_regs) / sizeof(tsl2584_regs[0]);
	mutex_lock(&ct->mutex);
	for (i = 0, n = 0; i < reg_count; i++) {
		reg_data[0] = tsl2584_regs[i].reg;
		error = tsl2584_i2c_read(ct, reg_data, 1);
		if (error < 0) {
			dev_err(&ct->client->dev,
				"Unable to read %s register: %d\n",
				tsl2584_regs[i].name, error);
		}
		n += scnprintf(buf + n, PAGE_SIZE - n,
			"%-20s = 0x%02X\n",
			tsl2584_regs[i].name,
			reg_data[0]);
	}
	mutex_unlock(&ct->mutex);

	return n;
}

static ssize_t tsl2584_registers_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct tsl2584_data *ct = i2c_get_clientdata(client);
	unsigned int i, reg_count;
	unsigned int value;
	u8 reg_data[2] = {0};
	int error;
	char name[30];

	if (count >= 30) {
		dev_err(&ct->client->dev, "input too long\n");
		return -EMSGSIZE;
	}

	if (sscanf(buf, "%30s %x", name, &value) != 2) {
		dev_err(&ct->client->dev, "unable to parse input\n");
		return -EINVAL;
	}

	reg_count = sizeof(tsl2584_regs) / sizeof(tsl2584_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, tsl2584_regs[i].name)) {
			mutex_lock(&ct->mutex);
			error = tsl2584_i2c_write(ct, reg_data, 2);
			mutex_unlock(&ct->mutex);
			if (error) {
				dev_err(&ct->client->dev,
					"Failed to write register %s\n", name);
				return error;
			}
			return count;
		}
	}

	dev_err(&ct->client->dev, "no such register %s\n", name);
	return -EINVAL;
}

static DEVICE_ATTR(registers, 0644, tsl2584_registers_show,
		   tsl2584_registers_store);


static irqreturn_t tsl2584_irq_handler(int irq, void *dev)
{
	struct tsl2584_data *tsl = dev;

	disable_irq_nosync(tsl->client->irq);

	wake_lock(&tsl->wl_isr);
	queue_delayed_work(tsl->workqueue, &tsl->dwork, 0);

	return IRQ_HANDLED;
}

static void tsl2584_work_func(struct work_struct *work)
{
	int error;
	u8 reg_data[2] = {0};
	struct delayed_work *dwork = to_delayed_work(work);
	struct tsl2584_data *tsl =
		container_of(dwork, struct tsl2584_data, dwork);

	mutex_lock(&tsl->mutex);

	reg_data[0] = TSL2584_CONTROL;
	error = tsl2584_i2c_read(tsl, reg_data, 1);
	if (error < 0) {
		dev_err(&tsl->client->dev,
			"Unable to read interrupt register: %d\n", error);
		if (wake_lock_active(&tsl->wl_isr))
			wake_unlock(&tsl->wl_isr);
		return;
	}


	if (tsl->pdata->irq)
		reg_data[0] &= TSL2584_CONTROL_ADC_INTR;
	else
		reg_data[0] &= TSL2584_CONTROL_ADC_VALID;

	if (reg_data[0]) {

		tsl2584_report_als(tsl);

		/* ADC Disable */
		reg_data[0] = TSL2584_CONTROL;
		reg_data[1] = TSL2584_CONTROL_POWER;
		error = tsl2584_i2c_write(tsl, reg_data, 2);
		if (error < 0) {
			dev_err(&tsl->client->dev,
				"I2C write error  %d\n", error);
		}

		reg_data[0] = TSL2584_COMMAND_SELECT | TSL2584_COMMAND_SPECIAL_FUNCTION
				| TSL2584_COMMAND_ALS_INT_CLEAR;

		tsl2584_i2c_write(tsl, reg_data, 1);

		/* ADC Enable */
		reg_data[0] = TSL2584_CONTROL;
		reg_data[1] = TSL2584_CONTROL_ADC_EN | TSL2584_CONTROL_POWER;
		error = tsl2584_i2c_write(tsl, reg_data, 2);
		if (error < 0) {
			dev_err(&tsl->client->dev,
				"I2C write error  %d\n", error);
		}
	}

	if (tsl->client->irq) {
		if (tsl2584_debug & TSL2584_DBG_ENABLE_DISABLE)
			dev_info(&tsl->client->dev,
				 "enable irq\n");
		enable_irq(tsl->client->irq);
	}
	else
		queue_delayed_work(tsl->workqueue,
				   &tsl->dwork, msecs_to_jiffies(280));

	mutex_unlock(&tsl->mutex);

	wake_lock_timeout(&tsl->wl_to, 0.5 * HZ);
	if (wake_lock_active(&tsl->wl_isr))
		wake_unlock(&tsl->wl_isr);
}

static int tsl2584_suspend(struct tsl2584_data *tsl)
{
	if (tsl2584_debug & TSL2584_DBG_SUSPEND_RESUME)
		dev_info(&tsl->client->dev, "%s\n", __func__);

	if (atomic_read(&tsl->power_on)) {
		dev_err(&tsl->client->dev,
			"Error: can't suspend with device enabled\n");
		return -EBUSY;
	}

	return 0;
}

static int tsl2584_resume(struct tsl2584_data *tsl)
{
	if (tsl2584_debug & TSL2584_DBG_SUSPEND_RESUME)
		dev_info(&tsl->client->dev, "%s\n", __func__);

	return 0;
}

static int tsl2584_pm_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct tsl2584_data *tsl = container_of(this,
		struct tsl2584_data, pm_notifier);

	mutex_lock(&tsl->mutex);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		tsl2584_suspend(tsl);
		break;
	case PM_POST_SUSPEND:
		tsl2584_resume(tsl);
		break;
	}

	mutex_unlock(&tsl->mutex);

	return NOTIFY_DONE;
}

#ifdef CONFIG_OF
static struct tsl2584_platform_data *
tsl2584_of_init(struct i2c_client *client)
{
	struct tsl2584_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	u32 val;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	if (!of_property_read_u32(np, "ams,ink_type", &val))
		pdata->ink_type = (u8)val;

	if (!of_property_read_u32(np, "ams,lux1ch0_coeff", &val))
		lux1ch0_coeff = (s32)val;

	if (!of_property_read_u32(np, "ams,lux1ch1_coeff", &val))
		lux1ch1_coeff = (s32)val;

	if (!of_property_read_u32(np, "ams,lux2ch0_coeff", &val))
		lux2ch0_coeff = (s32)val;

	if (!of_property_read_u32(np, "ams,lux2ch1_coeff", &val))
		lux2ch1_coeff = (s32)val;

	pdata->gpio_irq = of_get_gpio(np, 0);
	if (!gpio_is_valid(pdata->gpio_irq)) {
		dev_err(&client->dev, "tsl2584 irq gpio invalid\n");
		return pdata;
	}
	if (gpio_request(pdata->gpio_irq, "tsl2584_irq")) {
		dev_err(&client->dev, "failed to export tsl2584 irq gpio\n");
		kfree(pdata);
		return NULL;
	}

	gpio_export(pdata->gpio_irq, 1);
	gpio_export_link(&client->dev, "tsl2584_irq", pdata->gpio_irq);

	pdata->irq = gpio_to_irq(pdata->gpio_irq);

	return pdata;
}
#else
static inline struct tsl2584_platform_data *
tsl2584_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int tsl2584_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct tsl2584_platform_data *pdata;
	struct tsl2584_data *tsl;
	int error = 0;

	if (client->dev.of_node)
		pdata = tsl2584_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev,
			"Error: platform data required\n");
		return -ENODEV;
	}

	client->irq = pdata->irq;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Error: I2C_FUNC_I2C not supported\n");
		goto i2c_check_fail;
	}

	tsl = kzalloc(sizeof(struct tsl2584_data), GFP_KERNEL);
	if (tsl == NULL) {
		error = -ENOMEM;
		goto error_alloc_data_failed;
	}

	tsl->client = client;
	tsl->pdata = pdata;

	tsl->regulator = regulator_get(&client->dev,"vdd");
	if (IS_ERR(tsl->regulator)) {
		dev_info(&client->dev, "Cannot acquire regulator \n");
		tsl->regulator = NULL;
	}

	tsl->dev = input_allocate_device();
	if (!tsl->dev) {
		error = -ENOMEM;
		dev_err(&tsl->client->dev,
			"Error: input device allocate failed: %d\n", error);
		goto error_input_allocate_failed;
	}

	tsl->dev->name = "light";
	input_set_capability(tsl->dev, EV_MSC, MSC_TIMESTAMP);
	input_set_capability(tsl->dev, EV_MSC, MSC_RAW);

	tsl2584_misc_data = tsl;
	tsl->miscdevice.minor = MISC_DYNAMIC_MINOR;
	tsl->miscdevice.name = LD_TSL2584_NAME;
	tsl->miscdevice.fops = &tsl2584_misc_fops;
	error = misc_register(&tsl->miscdevice);
	if (error < 0) {
		dev_err(&tsl->client->dev, "Error: misc_register failed\n");
		goto error_misc_register_failed;
	}

	/* flags */
	atomic_set(&tsl->als_enabled, 0);
	atomic_set(&tsl->power_on, 0);


	tsl->als_apers = 5;
	tsl->als_mode = TSL2584_ALS_MODE_LOW_LUX;
	tsl->ink_type = tsl->pdata->ink_type;

	tsl->workqueue = create_singlethread_workqueue("als_wq");
	if (!tsl->workqueue) {
		dev_err(&tsl->client->dev, "Cannot create work queue\n");
		error = -ENOMEM;
		goto error_create_wq_failed;
	}
	INIT_DELAYED_WORK(&tsl->dwork, tsl2584_work_func);

	mutex_init(&tsl->mutex);
	wake_lock_init(&tsl->wl_to, WAKE_LOCK_SUSPEND, "tsl2584_wake_to");
	wake_lock_init(&tsl->wl_isr, WAKE_LOCK_SUSPEND, "tsl2584_wake_isr");

	if (pdata->irq) {
		error = request_irq(client->irq, tsl2584_irq_handler,
			IRQF_TRIGGER_LOW, LD_TSL2584_NAME, tsl);
		if (error != 0) {
			dev_err(&tsl->client->dev,
				"Error: irq request failed: %d\n", error);
			error = -ENODEV;
			goto error_req_irq_failed;
		}
	}
	i2c_set_clientdata(client, tsl);
	error = input_register_device(tsl->dev);
	if (error) {
		dev_err(&tsl->client->dev,
			"Error: input device register failed:%d\n", error);
		goto error_input_register_failed;
	}

	error = device_create_file(&client->dev, &dev_attr_registers);
	if (error < 0) {
		dev_err(&tsl->client->dev,
			"Error: File device creation failed: %d\n", error);
		error = -ENODEV;
		goto error_create_registers_file_failed;
	}

	tsl->pm_notifier.notifier_call = tsl2584_pm_event;
	error = register_pm_notifier(&tsl->pm_notifier);
	if (error < 0) {
		dev_err(&tsl->client->dev,
			"Error: Register_pm_notifier failed: %d\n", error);
		goto error_revision_read_failed;
	}

	return 0;

error_revision_read_failed:
	device_remove_file(&client->dev, &dev_attr_registers);
error_create_registers_file_failed:
	input_unregister_device(tsl->dev);
	tsl->dev = NULL;
error_input_register_failed:
	i2c_set_clientdata(client, NULL);
	if (pdata->irq)
		free_irq(tsl->client->irq, tsl);
error_req_irq_failed:
	mutex_destroy(&tsl->mutex);
	wake_lock_destroy(&tsl->wl_to);
	wake_lock_destroy(&tsl->wl_isr);
	destroy_workqueue(tsl->workqueue);
error_create_wq_failed:
	misc_deregister(&tsl->miscdevice);
error_misc_register_failed:
	tsl2584_misc_data = NULL;
	input_free_device(tsl->dev);
error_input_allocate_failed:
	if (tsl->regulator)
		regulator_put(tsl->regulator);
	kfree(tsl);
error_alloc_data_failed:
i2c_check_fail:
	if (pdata->gpio_irq)
		gpio_free(pdata->gpio_irq);
	if (client->dev.of_node)
		kfree(pdata);
	return error;
}

static int tsl2584_remove(struct i2c_client *client)
{
	struct tsl2584_data *tsl = i2c_get_clientdata(client);

	unregister_pm_notifier(&tsl->pm_notifier);

	device_remove_file(&client->dev, &dev_attr_registers);

	tsl2584_power_off(tsl);

	input_unregister_device(tsl->dev);

	if (wake_lock_active(&tsl->wl_to))
		wake_unlock(&tsl->wl_to);

	if (wake_lock_active(&tsl->wl_isr))
		wake_unlock(&tsl->wl_isr);

	wake_lock_destroy(&tsl->wl_isr);
	wake_lock_destroy(&tsl->wl_to);

	mutex_destroy(&tsl->mutex);
	i2c_set_clientdata(client, NULL);

	if (tsl->client->irq)
		free_irq(tsl->client->irq, tsl);

	destroy_workqueue(tsl->workqueue);

	misc_deregister(&tsl->miscdevice);
	if (tsl->regulator)
		regulator_put(tsl->regulator);

	if (tsl->pdata->gpio_irq)
		gpio_free(tsl->pdata->gpio_irq);

	kfree(tsl->pdata);
	kfree(tsl);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tsl2584_match_tbl[] = {
	{ .compatible = "ams,tsl2584" },
	{ },
};
MODULE_DEVICE_TABLE(of, tsl2584_match_tbl);
#endif

static const struct i2c_device_id tsl2584_id[] = {
	{LD_TSL2584_NAME, 0},
	{}
};

static struct i2c_driver tsl2584_i2c_driver = {
	.probe		= tsl2584_probe,
	.remove		= tsl2584_remove,
	.id_table	= tsl2584_id,
	.driver = {
		.name = LD_TSL2584_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tsl2584_match_tbl),
	},
};

static int __init tsl2584_init(void)
{
	return i2c_add_driver(&tsl2584_i2c_driver);
}

static void __exit tsl2584_exit(void)
{
	i2c_del_driver(&tsl2584_i2c_driver);
}

module_init(tsl2584_init);
module_exit(tsl2584_exit);

MODULE_DESCRIPTION("ALS driver for TSL2584");
MODULE_AUTHOR("Motorola Mobility");
MODULE_LICENSE("GPL");
