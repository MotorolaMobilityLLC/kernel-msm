/*
 *  stk3x1x.c - Linux kernel modules for sensortek stk301x, stk321x and stk331x 
 *  proximity/ambient light sensor
 *
 *  Copyright (C) 2012~2013 Lex Hsieh / sensortek <lex_hsieh@sensortek.com.tw>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Changes to the original Sensortek code are
 * Copyright (C) 2014 Motorola Mobility LLC. All Rights Reserved
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include   <linux/fs.h>   
#include  <asm/uaccess.h> 
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif


#define DRIVER_VERSION  "3.5.1.1nk"


/* Driver Settings */
#define STK_INT_PS_MODE_NONE		0
#define STK_INT_PS_MODE_HIGH		5
#define STK_INT_PS_MODE_LOW		6
#define STK_INT_PS_MODE_BOTH		7

//#define SPREADTRUM_PLATFORM
//#define STK_ALS_FIR
//#define STK_IRS

#ifdef SPREADTRUM_PLATFORM
	#include "stk3x1x.h"
#else
	#include "linux/stk3x1x_moto.h"
#endif
/* Define Register Map */
#define STK_STATE_REG 			0x00
#define STK_PSCTRL_REG			0x01
#define STK_ALSCTRL_REG			0x02
#define STK_LEDCTRL_REG			0x03
#define STK_INT_REG			0x04
#define STK_WAIT_REG 			0x05
#define STK_THDH1_PS_REG 		0x06
#define STK_THDH2_PS_REG 		0x07
#define STK_THDL1_PS_REG 		0x08
#define STK_THDL2_PS_REG 		0x09
#define STK_THDH1_ALS_REG 		0x0A
#define STK_THDH2_ALS_REG 		0x0B
#define STK_THDL1_ALS_REG 		0x0C
#define STK_THDL2_ALS_REG 		0x0D
#define STK_FLAG_REG 			0x10
#define STK_DATA1_PS_REG	 	0x11
#define STK_DATA2_PS_REG 		0x12
#define STK_DATA1_ALS_REG 		0x13
#define STK_DATA2_ALS_REG 		0x14
#define STK_DATA1_OFFSET_REG		0x15
#define STK_DATA2_OFFSET_REG		0x16
#define STK_DATA1_IR_REG 		0x17
#define STK_DATA2_IR_REG 		0x18
#define STK_PDT_ID_REG 			0x3E
#define STK_RSRVD_REG 			0x3F
#define STK_SW_RESET_REG		0x80


/* Define state reg */
#define STK_STATE_EN_IRS_SHIFT  	7
#define STK_STATE_EN_AK_SHIFT		6
#define STK_STATE_EN_ASO_SHIFT  	5
#define STK_STATE_EN_IRO_SHIFT  	4
#define STK_STATE_EN_WAIT_SHIFT  	2
#define STK_STATE_EN_ALS_SHIFT  	1
#define STK_STATE_EN_PS_SHIFT		0

#define STK_STATE_EN_IRS_MASK		0x80
#define STK_STATE_EN_AK_MASK		0x40
#define STK_STATE_EN_ASO_MASK		0x20
#define STK_STATE_EN_IRO_MASK		0x10
#define STK_STATE_EN_WAIT_MASK		0x04
#define STK_STATE_EN_ALS_MASK		0x02
#define STK_STATE_EN_PS_MASK		0x01

/* Define PS ctrl reg */
#define STK_PS_PRS_SHIFT  		6
#define STK_PS_GAIN_SHIFT  		4
#define STK_PS_IT_SHIFT			0

#define STK_PS_PRS_MASK			0xC0
#define STK_PS_GAIN_MASK		0x30
#define STK_PS_IT_MASK			0x0F

/* Define ALS ctrl reg */
#define STK_ALS_PRS_SHIFT  		6
#define STK_ALS_GAIN_SHIFT  		4
#define STK_ALS_IT_SHIFT		0

#define STK_ALS_PRS_MASK		0xC0
#define STK_ALS_GAIN_MASK		0x30
#define STK_ALS_IT_MASK			0x0F
	
/* Define LED ctrl reg */
#define STK_LED_IRDR_SHIFT  		6
#define STK_LED_DT_SHIFT  		0

#define STK_LED_IRDR_MASK		0xC0
#define STK_LED_DT_MASK			0x3F
	
/* Define interrupt reg */
#define STK_INT_CTRL_SHIFT  		7
#define STK_INT_OUI_SHIFT  		4
#define STK_INT_ALS_SHIFT  		3
#define STK_INT_PS_SHIFT		0

#define STK_INT_CTRL_MASK		0x80
#define STK_INT_OUI_MASK		0x10
#define STK_INT_ALS_MASK		0x08
#define STK_INT_PS_MASK			0x07

#define STK_INT_ALS			0x08

/* Define flag reg */
#define STK_FLG_ALSDR_SHIFT  		7
#define STK_FLG_PSDR_SHIFT  		6
#define STK_FLG_ALSINT_SHIFT  		5
#define STK_FLG_PSINT_SHIFT  		4
#define STK_FLG_OUI_SHIFT  		2
#define STK_FLG_IR_RDY_SHIFT  		1
#define STK_FLG_NF_SHIFT  		0

#define STK_FLG_ALSDR_MASK		0x80
#define STK_FLG_PSDR_MASK		0x40
#define STK_FLG_ALSINT_MASK		0x20
#define STK_FLG_PSINT_MASK		0x10
#define STK_FLG_OUI_MASK		0x04
#define STK_FLG_IR_RDY_MASK		0x02
#define STK_FLG_NF_MASK			0x01
	
/* misc define */
#define MIN_ALS_POLL_DELAY_NS		110000000

#define STK_ALS_CORRECT_FACTOR		1300

#define STK2213_PID			0x23
#define STK2213I_PID			0x22
#define STK3010_PID			0x33
#define STK3210_STK3310_PID		0x13
#define STK3211_STK3311_PID		0x12

#define STK_IRC_MAX_ALS_CODE		20000
#define STK_IRC_MIN_ALS_CODE		25
#define STK_IRC_MIN_IR_CODE		50
#define STK_IRC_ALS_DENOMI		2		
#define STK_IRC_ALS_NUMERA		5
#define STK_IRC_ALS_CORREC		748

#define STK_PROX_SENSOR_COVERED		0
#define STK_PROX_SENSOR_UNCOVERED	1

#define DEVICE_NAME		"stk_ps"
#define ALS_NAME "lightsensor-level"
#define PS_NAME "proximity"

#ifdef SPREADTRUM_PLATFORM
extern int sprd_3rdparty_gpio_pls_irq;

static struct stk3x1x_platform_data stk3x1x_pfdata = {
	.state_reg = 0x0,    /* disable all */
	.psctrl_reg = 0x71,  /* ps_persistance=4, ps_gain=64X, PS_IT=0.391ms */
	.alsctrl_reg = 0x38, /* als_persistance=1, als_gain=64X, ALS_IT=50ms */
	.ledctrl_reg = 0xFF, /* 100mA IRDR, 64/64 LED duty */
	.wait_reg = 0x07,    /* 50 ms */
	.int_pin = sprd_3rdparty_gpio_pls_irq,
	.als_thresh_pct = 5;
	.covered_thresh = 0x0096;
	.uncovered_thresh = 0x006E;
	.recal_thresh = 0x0046;
	.max_noise_fl = 0x0F00;
}; 
#endif

#ifdef STK_ALS_FIR
struct data_filter {
	u16 raw[8];
	int sum;
	int number;
	int idx;
};
#endif

enum prox_state {
	CALIBRATING,
	UNCOVERED,
	COVERED
};

struct stk3x1x_data {
	struct i2c_client *client;
	int32_t irq;
	struct work_struct stk_work;
	struct workqueue_struct *stk_wq;	
	uint16_t ir_code;
	uint16_t als_correct_factor;
	uint8_t alsctrl_reg;
	uint8_t psctrl_reg;
	int		int_pin;
	uint8_t wait_reg;
	uint8_t int_reg;
	struct mutex io_lock;
	struct input_dev *ps_input_dev;
	bool ps_enabled;
	struct wake_lock ps_wakelock;	
	struct input_dev *als_input_dev;
	int32_t als_lux_last;
	uint32_t als_transmittance;	
	bool als_enabled;
	bool re_enable_als;
#ifdef STK_ALS_FIR
	struct data_filter fir;
#endif
	uint16_t als_threshold_percent;
	uint16_t covered_threshold;
	uint16_t uncovered_threshold;
	uint16_t recal_threshold;
	uint32_t noise_count;
	uint16_t noise_floor;
	uint16_t max_noise_floor;
	enum prox_state prox_mode;
	bool suspended;
	bool delayed_work;
};

static int32_t stk3x1x_enable_ps(struct stk3x1x_data *ps_data,
	uint8_t enable);
static int32_t stk3x1x_enable_als(struct stk3x1x_data *ps_data,
	uint8_t enable);
static int32_t stk3x1x_set_ps_thd_l(struct stk3x1x_data *ps_data,
	uint16_t thd_l);
static int32_t stk3x1x_set_ps_thd_h(struct stk3x1x_data *ps_data,
	uint16_t thd_h);
static int32_t stk3x1x_set_als_thd_l(struct stk3x1x_data *ps_data,
	uint16_t thd_l);
static int32_t stk3x1x_set_als_thd_h(struct stk3x1x_data *ps_data,
	uint16_t thd_h);
static int32_t stk3x1x_get_ir_reading(struct stk3x1x_data *ps_data);
static void stk3x1x_set_wait_reg(struct stk3x1x_data *ps_data,
	uint8_t wait_reg);


static int stk3x1x_i2c_read_data(struct i2c_client *client,
	unsigned char command, int length, unsigned char *values)
{
	uint8_t retry;	
	int err;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &command,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = values,
		},
	};
	
	for (retry = 0; retry < 5; retry++) {
		err = i2c_transfer(client->adapter, msgs, 2);
		if (err == 2)
			break;
		else
			mdelay(5);
	}
	
	if (retry >= 5) {
		dev_err(&client->dev, "%s: I2C read fail, err=%d\n",
			__func__, err);
		return -EIO;
	} 
	return 0;		
}

static int stk3x1x_i2c_write_data(struct i2c_client *client,
	unsigned char command, int length, unsigned char *values)
{
	int retry;
	int err;	
	unsigned char data[11];
	struct i2c_msg msg;
	int index;

	if (!client)
		return -EINVAL;
	else if (length >= 10) {
		dev_err(&client->dev, "%s: length %d exceeds 10\n",
			__func__, length);
		return -EINVAL;
	}
	
	data[0] = command;
	for (index=1;index<=length;index++)
		data[index] = values[index-1];	
	
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = length+1;
	msg.buf = data;
	
	for (retry = 0; retry < 5; retry++) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		else
			mdelay(5);
	}
	
	if (retry >= 5) {
		dev_err(&client->dev, "%s: I2C write fail, err=%d\n",
			__func__, err);
		return -EIO;
	}
	return 0;
}

static int stk3x1x_i2c_smbus_read_byte_data(struct i2c_client *client,
	unsigned char command)
{
	unsigned char value;
	int err;
	err = stk3x1x_i2c_read_data(client, command, 1, &value);
	if(err < 0)
		return err;
	return value;
}

static int stk3x1x_i2c_smbus_write_byte_data(struct i2c_client *client,
	unsigned char command, unsigned char value)
{
	int err;
	err = stk3x1x_i2c_write_data(client, command, 1, &value);
	return err;
}

inline uint32_t stk_alscode2lux(struct stk3x1x_data *ps_data, uint32_t alscode)
{
	alscode += ((alscode << 7) + (alscode << 3) + (alscode >> 1));
	alscode <<= 3;
	alscode /= ps_data->als_transmittance;
	return alscode;
}

inline void stk_als_set_new_thd(struct stk3x1x_data *ps_data, uint16_t alscode)
{
    int32_t high_thd,low_thd;
	uint32_t delta;

	delta = (uint32_t)alscode * (uint32_t)ps_data->als_threshold_percent /
		100;
	if (delta == 0)
		delta = 1;

	high_thd = alscode + delta;
	low_thd = alscode - delta;

	if (high_thd >= (1 << 16))
		high_thd = (1 << 16) - 1;
	if (low_thd < 0)
		low_thd = 0;

	stk3x1x_set_als_thd_h(ps_data, (uint16_t)high_thd);
	stk3x1x_set_als_thd_l(ps_data, (uint16_t)low_thd);
}

static int32_t stk3x1x_init_all_reg(struct stk3x1x_data *ps_data,
	struct stk3x1x_platform_data *plat_data)
{
	int32_t ret;
	uint8_t w_reg;
	
	w_reg = plat_data->state_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_STATE_REG, w_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}

	ps_data->psctrl_reg = plat_data->psctrl_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_PSCTRL_REG, ps_data->psctrl_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}
	ps_data->alsctrl_reg = plat_data->alsctrl_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_ALSCTRL_REG, ps_data->alsctrl_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}
	w_reg = plat_data->ledctrl_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_LEDCTRL_REG, w_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}
	ps_data->wait_reg = plat_data->wait_reg;
	
	if (ps_data->wait_reg < 2) {
		dev_err(&ps_data->client->dev,
			"%s: wait_reg should be larger than 2, force to 2\n",
			__func__);
		ps_data->wait_reg = 2;
	} else if (ps_data->wait_reg > 0xFF) {
		dev_err(&ps_data->client->dev,
			"%s: wait_reg should be less than 0xFF, "
			"force to 0xFF\n", __func__);
		ps_data->wait_reg = 0xFF;		
	}
	w_reg = plat_data->wait_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_WAIT_REG, w_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}
	w_reg = STK_INT_PS_MODE_NONE;
	w_reg |= STK_INT_ALS;
	ps_data->int_reg = w_reg;
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_INT_REG,
		ps_data->int_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;
	}		
	return 0;	
}
	
static int32_t stk3x1x_check_pid(struct stk3x1x_data *ps_data)
{
	unsigned char value[2], pid_msb;
	int err;
	
	err = stk3x1x_i2c_read_data(ps_data->client, STK_PDT_ID_REG, 2,
		&value[0]);
	if (err < 0) {
		dev_err(&ps_data->client->dev, "%s: fail, ret=%d\n",
		__func__, err);
		return err;
	}
	
	dev_dbg(&ps_data->client->dev,
		"%s: PID=0x%x, RID=0x%x\n", __func__, value[0], value[1]);
	
	if (value[1] == 0xC0)
		dev_dbg(&ps_data->client->dev, "%s: RID=0xC0\n", __func__);

	if (value[0] == 0) {
		dev_err(&ps_data->client->dev,
			"%s: PID=0x0, please make sure the chip is stk3x1x!\n",
			__func__);
		return -EIO;
	}
	
	pid_msb = value[0] & 0xF0;
	switch (pid_msb) {
	case 0x10:
	case 0x20:
	case 0x30:
		return 0;
	default:
		dev_err(&ps_data->client->dev, "%s: invalid PID(%#x)\n",
			__func__, value[0]);
		return -EIO;
	}
	return 0;	
}

static int32_t stk3x1x_software_reset(struct stk3x1x_data *ps_data)
{
	int32_t r;
	uint8_t w_reg;
	
	w_reg = 0x7F;
	r = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_WAIT_REG,
		w_reg);
	if (r < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error, ret=%d\n", __func__, r);
		return r;
	}
	r = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_WAIT_REG);
	if (w_reg != r) {
		dev_err(&ps_data->client->dev,
			"%s: read-back value is not the same\n", __func__);
		return -EIO;
	}
	
	r = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_SW_RESET_REG, 0);
	if (r < 0) {
		dev_err(&ps_data->client->dev, "%s: write error after reset\n",
			__func__);
		return r;
	}
	msleep(1);
	return 0;
}

static int32_t stk3x1x_set_als_thd_l(struct stk3x1x_data *ps_data,
	uint16_t thd_l)
{
	unsigned char val[2];
	dev_dbg(&ps_data->client->dev, "%s: Low ALS threshold = 0x%04x\n",
		__func__, (uint16_t)thd_l);
	val[0] = (thd_l & 0xFF00) >> 8;
	val[1] = thd_l & 0x00FF;
	return stk3x1x_i2c_write_data(ps_data->client, STK_THDL1_ALS_REG,
		2, val);
}

static int32_t stk3x1x_set_als_thd_h(struct stk3x1x_data *ps_data,
	uint16_t thd_h)
{
	unsigned char val[2];
	dev_dbg(&ps_data->client->dev, "%s: High ALS threshold = 0x%04x\n",
		__func__, (uint16_t)thd_h);
	val[0] = (thd_h & 0xFF00) >> 8;
	val[1] = thd_h & 0x00FF;
	return stk3x1x_i2c_write_data(ps_data->client, STK_THDH1_ALS_REG,
		2, val);
}

static int32_t stk3x1x_set_ps_thd_l(struct stk3x1x_data *ps_data,
	uint16_t thd_l)
{
	unsigned char val[2];
	dev_dbg(&ps_data->client->dev, "%s: Low prox threshold = 0x%04x\n",
		__func__, (uint16_t)thd_l);
	val[0] = (thd_l & 0xFF00) >> 8;
	val[1] = thd_l & 0x00FF;
	return stk3x1x_i2c_write_data(ps_data->client, STK_THDL1_PS_REG,
		2, val);
}

static int32_t stk3x1x_set_ps_thd_h(struct stk3x1x_data *ps_data,
	uint16_t thd_h)
{	
	unsigned char val[2];
	dev_dbg(&ps_data->client->dev, "%s: High prox threshold = 0x%04x\n",
		__func__, (uint16_t)thd_h);
	val[0] = (thd_h & 0xFF00) >> 8;
	val[1] = thd_h & 0x00FF;
	return stk3x1x_i2c_write_data(ps_data->client, STK_THDH1_PS_REG,
		2, val);
}

static void stk_prox_mode_calibrating(struct stk3x1x_data *ps_data)
{
	ps_data->prox_mode = CALIBRATING;

	dev_err(&ps_data->client->dev,
		"%s: Prox sensor calibrating", __func__);

	ps_data->noise_count = 4;
	ps_data->noise_floor = 0;

	stk3x1x_set_ps_thd_l(ps_data, 0xFFFF);
	stk3x1x_set_ps_thd_h(ps_data, 0x0000);

	return;
}

static void stk_prox_mode_uncovered(struct stk3x1x_data *ps_data)
{
	int32_t thd_l;
	int32_t thd_h;

	ps_data->prox_mode = UNCOVERED;

	dev_err(&ps_data->client->dev,
		"%s: Prox sensor uncovered", __func__);

	thd_l = (int32_t)ps_data->noise_floor -
		(int32_t)ps_data->recal_threshold;
	if (thd_l < 0)
		thd_l = 0;
	stk3x1x_set_ps_thd_l(ps_data, (uint16_t)thd_l);

	thd_h = (int32_t)ps_data->noise_floor +
		(int32_t)ps_data->covered_threshold;
	if (thd_h > 0xFFFF)
		thd_h = 0xFFFF;
	stk3x1x_set_ps_thd_h(ps_data, (uint16_t)thd_h);

	input_report_abs(ps_data->ps_input_dev,
		ABS_DISTANCE, STK_PROX_SENSOR_UNCOVERED);
	input_sync(ps_data->ps_input_dev);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);

	return;
}

static void stk_prox_mode_covered(struct stk3x1x_data *ps_data)
{
	int32_t thd_l;

	ps_data->prox_mode = COVERED;

	dev_err(&ps_data->client->dev,
		"%s: Prox sensor covered", __func__);

	thd_l = (int32_t)ps_data->noise_floor +
		(int32_t)ps_data->uncovered_threshold;
	stk3x1x_set_ps_thd_l(ps_data, (uint16_t)thd_l);

	stk3x1x_set_ps_thd_h(ps_data, 0xFFFF);

	input_report_abs(ps_data->ps_input_dev,
		ABS_DISTANCE, STK_PROX_SENSOR_COVERED);
	input_sync(ps_data->ps_input_dev);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);

	return;
}

static inline uint32_t stk3x1x_get_ps_reading(struct stk3x1x_data *ps_data)
{	
	unsigned char value[2];
	int err;
	err = stk3x1x_i2c_read_data(ps_data->client, STK_DATA1_PS_REG, 2,
		&value[0]);
	if(err < 0)
		return err;
	return ((value[0]<<8) | value[1]);	
}

static int32_t stk3x1x_set_flag(struct stk3x1x_data *ps_data,
	uint8_t org_flag_reg, uint8_t clr)
{
	uint8_t w_flag;
	w_flag = org_flag_reg | (STK_FLG_ALSINT_MASK | STK_FLG_PSINT_MASK
		| STK_FLG_OUI_MASK | STK_FLG_IR_RDY_MASK);
	w_flag &= (~clr);
	dev_dbg(&ps_data->client->dev,
		"%s: org_flag_reg=0x%x, w_flag = 0x%x\n", __func__,
			org_flag_reg, w_flag);
	return stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_FLAG_REG,
		w_flag);
}

static int32_t stk3x1x_get_flag(struct stk3x1x_data *ps_data)
{	
	return stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_FLAG_REG);
}

static void stk3x1x_set_wait_reg(struct stk3x1x_data *ps_data, uint8_t wait_reg)
{
	int32_t ret;

	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_WAIT_REG, wait_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error\n", __func__);
	}
}

static int32_t stk3x1x_enable_ps(struct stk3x1x_data *ps_data, uint8_t enable)
{
	int32_t ret;
	uint8_t w_state_reg;
	uint8_t curr_ps_enable;

	curr_ps_enable = ps_data->ps_enabled ? 1 : 0;
	if(curr_ps_enable == enable)
		return 0;
		
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error, ret=%d\n", __func__, ret);
		return ret;
	}	
	w_state_reg = ret;
	
	w_state_reg &= ~(STK_STATE_EN_PS_MASK | STK_STATE_EN_WAIT_MASK
		| STK_STATE_EN_AK_MASK);
	if (enable) {
		w_state_reg |= STK_STATE_EN_PS_MASK;
		if (!(ps_data->als_enabled)) {
			w_state_reg |= STK_STATE_EN_WAIT_MASK;
			stk3x1x_set_wait_reg(ps_data, ps_data->wait_reg & 0x0F);
		}
	} else {
		if (ps_data->als_enabled) {
			w_state_reg |= STK_STATE_EN_WAIT_MASK;
			stk3x1x_set_wait_reg(ps_data, ps_data->wait_reg);
		}
	}
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG,
		w_state_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error, ret=%d\n", __func__, ret);
		return ret;
	}
		
	if (enable) {
		stk_prox_mode_calibrating(ps_data);

		ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
			STK_INT_REG);
		if (ret < 0) {
			dev_err(&ps_data->client->dev,
				"%s: read i2c error, ret=%d\n", __func__, ret);
			return ret;
		}
		w_state_reg = ret;
		w_state_reg &= ~STK_INT_PS_MASK;
		w_state_reg |= STK_INT_PS_MODE_BOTH;
		ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
			STK_INT_REG, w_state_reg);
		if (ret < 0) {
			dev_err(&ps_data->client->dev,
				"%s: write I2C error, ret=%d\n", __func__, ret);
			return ret;
		}

		if (!(ps_data->als_enabled))
			enable_irq(ps_data->irq);
		ps_data->ps_enabled = true;
	} else {
		if (!(ps_data->als_enabled))
			disable_irq(ps_data->irq);
		ps_data->ps_enabled = false;
	}
	return ret;
}

static int32_t stk3x1x_enable_als(struct stk3x1x_data *ps_data, uint8_t enable)
{
	int32_t ret;
	uint8_t w_state_reg;
	uint8_t curr_als_enable = (ps_data->als_enabled)?1:0;
	
	if(curr_als_enable == enable)
		return 0;
	
#ifdef STK_IRS
	if (enable && !(ps_data->ps_enabled)) {
		ret = stk3x1x_get_ir_reading(ps_data);
		if(ret > 0)
			ps_data->ir_code = ret;
	}		
#endif
	
	if (enable) {
		stk3x1x_set_als_thd_h(ps_data, 0x0000);
		stk3x1x_set_als_thd_l(ps_data, 0xFFFF);
	}
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error, ret=%d\n", __func__, ret);
		return ret;
	}
	w_state_reg = (uint8_t)(ret & (~(STK_STATE_EN_ALS_MASK
		| STK_STATE_EN_WAIT_MASK)));
	if (enable) {
		w_state_reg |= STK_STATE_EN_ALS_MASK;
		if (!(ps_data->ps_enabled)) {
			w_state_reg |= STK_STATE_EN_WAIT_MASK;
			stk3x1x_set_wait_reg(ps_data, ps_data->wait_reg);
		}
	} else {
		if (ps_data->ps_enabled) {
			w_state_reg |= STK_STATE_EN_WAIT_MASK;
			stk3x1x_set_wait_reg(ps_data, ps_data->wait_reg & 0x0F);
		}
	}

	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG,
		w_state_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error, ret=%d\n", __func__, ret);
		return ret;
	}
	
	if (enable) {
		ps_data->als_enabled = true;
		if(!(ps_data->ps_enabled))
			enable_irq(ps_data->irq);
	} else {
		ps_data->als_enabled = false;
		if(!(ps_data->ps_enabled))	
			disable_irq(ps_data->irq);		
	}
	return ret;
}

static inline int32_t stk3x1x_get_als_reading(struct stk3x1x_data *ps_data)
{
	int32_t word_data;
#ifdef STK_ALS_FIR
	int index;   
#endif	
	unsigned char value[2];
	int ret;
	
	ret = stk3x1x_i2c_read_data(ps_data->client, STK_DATA1_ALS_REG, 2,
		&value[0]);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: fail, ret=0x%x",
			__func__, ret);
		return ret;
	}
	word_data = (value[0]<<8) | value[1];	
	
#ifdef STK_ALS_FIR
	if (ps_data->fir.number < 8) {
		ps_data->fir.raw[ps_data->fir.number] = word_data;
		ps_data->fir.sum += word_data;
		ps_data->fir.number++;
		ps_data->fir.idx++;
	} else {
		index = ps_data->fir.idx % 8;
		ps_data->fir.sum -= ps_data->fir.raw[index];
		ps_data->fir.raw[index] = word_data;
		ps_data->fir.sum += word_data;
		ps_data->fir.idx++;
		word_data = ps_data->fir.sum/8;
	}	
#endif	
	return word_data;
}

static int32_t stk3x1x_set_irs_it_slp(struct stk3x1x_data *ps_data,
	uint16_t *slp_time)
{
	uint8_t irs_alsctrl;
	int32_t ret;
		
	irs_alsctrl = (ps_data->alsctrl_reg & 0x0F) - 2;		
	switch (irs_alsctrl) {
		case 6:
			*slp_time = 12;
			break;
		case 7:
			*slp_time = 24;			
			break;
		case 8:
			*slp_time = 48;			
			break;
		case 9:
			*slp_time = 96;			
			break;				
		default:
			dev_err(&ps_data->client->dev,
				"%s: unknown ALS IT=0x%x\n", __func__,
				irs_alsctrl);
			ret = -EINVAL;	
			return ret;
	}
	irs_alsctrl |= (ps_data->alsctrl_reg & 0xF0);
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_ALSCTRL_REG, irs_alsctrl);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		return ret;		
	}		
	return 0;
}

static int32_t stk3x1x_get_ir_reading(struct stk3x1x_data *ps_data)
{
	int32_t word_data, ret;
	uint8_t w_reg, retry = 0;	
	uint16_t irs_slp_time = 100;
	bool re_enable_ps = false;
	unsigned char value[2];
	
	if (ps_data->ps_enabled) {
		stk3x1x_enable_ps(ps_data, 0);
		re_enable_ps = true;
	}
	
	ret = stk3x1x_set_irs_it_slp(ps_data, &irs_slp_time);
	if(ret < 0)
		goto irs_err_i2c_rw;
	
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		goto irs_err_i2c_rw;
	}		
	
	w_reg = ret | STK_STATE_EN_IRS_MASK;		
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG,
		w_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		goto irs_err_i2c_rw;
	}	
	msleep(irs_slp_time);	
	
	do
	{
		msleep(3);
		ret = stk3x1x_get_flag(ps_data);
		if (ret < 0) {
			dev_err(&ps_data->client->dev, "%s: write I2C error\n",
				__func__);
			goto irs_err_i2c_rw;
		}	
		retry++;
	} while (retry < 10 && ((ret&STK_FLG_IR_RDY_MASK) == 0));
	
	if (retry == 10) {
		dev_err(&ps_data->client->dev,
			"%s: ir data is not ready for 300ms\n", __func__);
		ret = -EINVAL;
		goto irs_err_i2c_rw;
	}
	ret = stk3x1x_set_flag(ps_data, ret, STK_FLG_IR_RDY_MASK);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		goto irs_err_i2c_rw;
	}		
	
	ret = stk3x1x_i2c_read_data(ps_data->client, STK_DATA1_IR_REG, 2,
		&value[0]);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: fail, ret=0x%x",
			__func__, ret);
		goto irs_err_i2c_rw;
	}
	word_data = ((value[0]<<8) | value[1]);	

	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		STK_ALSCTRL_REG, ps_data->alsctrl_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev, "%s: write I2C error\n",
			__func__);
		goto irs_err_i2c_rw;
	}
	if(re_enable_ps)
		stk3x1x_enable_ps(ps_data, 1);			
	return word_data;

irs_err_i2c_rw:	
	if(re_enable_ps)
		stk3x1x_enable_ps(ps_data, 1);		
	return ret;
}

static ssize_t stk_als_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);		
	int32_t reading;
	
	reading = stk3x1x_get_als_reading(ps_data);
	return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}

static ssize_t stk_als_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int32_t enable, ret;
	
	mutex_lock(&ps_data->io_lock);
	enable = (ps_data->als_enabled)?1:0;
	mutex_unlock(&ps_data->io_lock);
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	ret = (ret & STK_STATE_EN_ALS_MASK) ? 1 : 0 ;
	
	if(enable != ret)
		dev_err(&ps_data->client->dev,
			"%s: driver and sensor mismatch! driver_enable=0x%x, "
			"sensor_enable=%x\n",
			__func__, enable, ret);
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);	
}

static ssize_t stk_als_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		dev_err(&ps_data->client->dev,
			"%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}	
	dev_dbg(&ps_data->client->dev,
		"%s: Enable ALS : %d\n", __func__, en);
	mutex_lock(&ps_data->io_lock);
	stk3x1x_enable_als(ps_data, en);
	mutex_unlock(&ps_data->io_lock);
	return size;
}

static ssize_t stk_als_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
	int32_t als_reading;
	uint32_t als_lux;
	als_reading = stk3x1x_get_als_reading(ps_data);
	als_lux = stk_alscode2lux(ps_data, als_reading);
	return scnprintf(buf, PAGE_SIZE, "%d lux\n", als_lux);
}

static ssize_t stk_als_lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);	
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 16, &value);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	
	}
	ps_data->als_lux_last = value;
	input_report_abs(ps_data->als_input_dev, ABS_MISC, value);
	input_sync(ps_data->als_input_dev);
	dev_dbg(&ps_data->client->dev,
		"%s: als input event %ld lux\n", __func__, value);

	return size;
}


static ssize_t stk_als_transmittance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int32_t transmittance;
	transmittance = ps_data->als_transmittance;
	return scnprintf(buf, PAGE_SIZE, "%d\n", transmittance);
}

static ssize_t stk_als_transmittance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	    
	}
	ps_data->als_transmittance = value;
	return size;
}

static ssize_t stk_als_ir_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int32_t reading;
	reading = stk3x1x_get_ir_reading(ps_data);
	return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}

static ssize_t stk_ps_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint32_t reading;
	reading = stk3x1x_get_ps_reading(ps_data);
	return scnprintf(buf, PAGE_SIZE, "%d\n", reading);
}

static ssize_t stk_ps_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t enable, ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	
	mutex_lock(&ps_data->io_lock);
	enable = (ps_data->ps_enabled)?1:0;
	mutex_unlock(&ps_data->io_lock);
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	ret = (ret & STK_STATE_EN_PS_MASK) ? 1 : 0;
	
	if(enable != ret)
		dev_err(&ps_data->client->dev,
			"%s: driver and sensor mismatch! "
			"driver_enable=0x%x, sensor_enable=%x\n",
			__func__, enable, ret);
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);	
}

static ssize_t stk_ps_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		dev_err(&ps_data->client->dev,
			"%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: Enable PS : %d\n", __func__, en);
	mutex_lock(&ps_data->io_lock);
	stk3x1x_enable_ps(ps_data, en);
	mutex_unlock(&ps_data->io_lock);
	return size;
}

static ssize_t stk_ps_enable_aso_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t ret;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	ret = (ret & STK_STATE_EN_ASO_MASK) ? 1 : 0;
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", ret);		
}

static ssize_t stk_ps_enable_aso_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	uint8_t en;
	int32_t ret;
	uint8_t w_state_reg;
	
	if (sysfs_streq(buf, "1"))
		en = 1;
	else if (sysfs_streq(buf, "0"))
		en = 0;
	else {
		dev_err(&ps_data->client->dev,
			"%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: Enable PS ASO : %d\n", __func__, en);
    
	ret = stk3x1x_i2c_smbus_read_byte_data(ps_data->client, STK_STATE_REG);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error\n", __func__);
		return ret;
	}
	w_state_reg = (uint8_t)(ret & (~STK_STATE_EN_ASO_MASK)); 
	if(en)	
		w_state_reg |= STK_STATE_EN_ASO_MASK;	
	
	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client, STK_STATE_REG,
		w_state_reg);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error\n", __func__);
		return ret;
	}	
	
	return size;	
}

static ssize_t stk_ps_offset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int32_t word_data;
	unsigned char value[2];
	int ret;
	
	ret = stk3x1x_i2c_read_data(ps_data->client, STK_DATA1_OFFSET_REG, 2,
		&value[0]);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: read I2C error, ret=0x%x", __func__, ret);
		return ret;
	}
	word_data = (value[0]<<8) | value[1];					
		
	return scnprintf(buf, PAGE_SIZE, "%d\n", word_data);
}
 
static ssize_t stk_ps_offset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long offset = 0;
	int ret;
	unsigned char val[2];
	
	ret = kstrtoul(buf, 10, &offset);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__,  ret);
		return ret;	
	}
	if (offset > 65535) {
		dev_err(&ps_data->client->dev,
			"%s: invalid value, offset=%ld\n",
			__func__, offset);
		return -EINVAL;
	}
	
	val[0] = (offset & 0xFF00) >> 8;
	val[1] = offset & 0x00FF;
	ret = stk3x1x_i2c_write_data(ps_data->client, STK_DATA1_OFFSET_REG,
		2, val);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: write I2C error\n", __func__);
		return ret;
	}
	
	return size;
}

static ssize_t stk_ps_distance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	int32_t dist = 1, ret;

	ret = stk3x1x_get_flag(ps_data);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: stk3x1x_get_flag failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}
	dist = (ret & STK_FLG_NF_MASK) ? 1 : 0;
	
	input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, dist);
	input_sync(ps_data->ps_input_dev);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
	dev_dbg(&ps_data->client->dev,
		"%s: ps input event %d cm\n", __func__, dist);
	return scnprintf(buf, PAGE_SIZE, "%d\n", dist);
}

static ssize_t stk_ps_distance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;
	ret = kstrtoul(buf, 10, &value);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	
	}
	input_report_abs(ps_data->ps_input_dev, ABS_DISTANCE, value);
	input_sync(ps_data->ps_input_dev);
	wake_lock_timeout(&ps_data->ps_wakelock, 3*HZ);
	dev_dbg(&ps_data->client->dev,
		"%s: ps input event %ld cm\n", __func__, value);
	return size;
}

static ssize_t stk_ps_code_thd_l_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t ps_thd_l1_reg, ps_thd_l2_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	ps_thd_l1_reg = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_THDL1_PS_REG);
	if (ps_thd_l1_reg < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, err=0x%x", __func__, ps_thd_l1_reg);
		return -EINVAL;		
	}
	ps_thd_l2_reg = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_THDL2_PS_REG);
	if (ps_thd_l2_reg < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, err=0x%x", __func__, ps_thd_l2_reg);
		return -EINVAL;		
	}
	ps_thd_l1_reg = ps_thd_l1_reg<<8 | ps_thd_l2_reg;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ps_thd_l1_reg);
}

static ssize_t stk_ps_code_thd_h_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t ps_thd_h1_reg, ps_thd_h2_reg;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	ps_thd_h1_reg = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_THDH1_PS_REG);
	if (ps_thd_h1_reg < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, err=0x%x", __func__, ps_thd_h1_reg);
		return -EINVAL;
	}
	ps_thd_h2_reg = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_THDH2_PS_REG);
	if (ps_thd_h2_reg < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, err=0x%x", __func__, ps_thd_h2_reg);
		return -EINVAL;
	}
	ps_thd_h1_reg = ps_thd_h1_reg<<8 | ps_thd_h2_reg;
	return scnprintf(buf, PAGE_SIZE, "%d\n", ps_thd_h1_reg);
}

static ssize_t stk_all_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t ps_reg[27];
	uint8_t cnt;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);
	for (cnt = 0; cnt < 25; cnt++) {
		ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
			(cnt));
		if (ps_reg[cnt] < 0) {
			dev_err(&ps_data->client->dev,
				"%s: fail, ret=%d", __func__, ps_reg[cnt]);
			return -EINVAL;
		} else {
			dev_dbg(&ps_data->client->dev,
				"%s: reg[0x%2X]=0x%2X\n", __func__,
				cnt, ps_reg[cnt]);
		}
	}
	ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_PDT_ID_REG);
	if (ps_reg[cnt] < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, ret=%d", __func__, ps_reg[cnt]);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: reg[0x%x]=0x%2X\n", __func__, STK_PDT_ID_REG,
		ps_reg[cnt]);
	cnt++;
	ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_RSRVD_REG);
	if (ps_reg[cnt] < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, ret=%d", __func__, ps_reg[cnt]);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: reg[0x%x]=0x%2X\n", __func__, STK_RSRVD_REG,
		ps_reg[cnt]);

	return scnprintf(buf, PAGE_SIZE,
	"[0]%2X [1]%2X [2]%2X [3]%2X [4]%2X [5]%2X [6/7 HTHD]%2X,%2X [8/9 LTHD]%2X,%2X [A]%2X [B]%2X [C]%2X [D]%2X [E/F Aoff]%2X,%2X, [10]%2X [11/12 PS]%2X,%2X [13]%2X [14]%2X [15/16 Foff]%2X,%2X [17]%2X [18]%2X [3E]%2X [3F]%2X\n",
	ps_reg[0], ps_reg[1], ps_reg[2], ps_reg[3], ps_reg[4], ps_reg[5],
	ps_reg[6], ps_reg[7], ps_reg[8], ps_reg[9], ps_reg[10], ps_reg[11],
	ps_reg[12], ps_reg[13], ps_reg[14], ps_reg[15], ps_reg[16], ps_reg[17],
	ps_reg[18], ps_reg[19], ps_reg[20], ps_reg[21], ps_reg[22], ps_reg[23],
	ps_reg[24], ps_reg[25], ps_reg[26]);
}


static ssize_t stk_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int32_t ps_reg[27];
	uint8_t cnt;
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);	
	for (cnt = 0; cnt < 25; cnt++) {
		ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
			(cnt));
		if (ps_reg[cnt] < 0) {
			dev_err(&ps_data->client->dev,
				"%s: fail, ret=%d", __func__,
				ps_reg[cnt]);
			return -EINVAL;
		} else {
			dev_dbg(&ps_data->client->dev,
				"%s: reg[0x%2X]=0x%2X\n", __func__,
				cnt, ps_reg[cnt]);
		}
	}
	ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_PDT_ID_REG);
	if (ps_reg[cnt] < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, ret=%d", __func__, ps_reg[cnt]);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: reg[0x%x]=0x%2X\n", __func__, STK_PDT_ID_REG,
		ps_reg[cnt]);
	cnt++;
	ps_reg[cnt] = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,
		STK_RSRVD_REG);
	if (ps_reg[cnt] < 0) {
		dev_err(&ps_data->client->dev,
			"%s: fail, ret=%d", __func__, ps_reg[cnt]);
		return -EINVAL;
	}
	dev_dbg(&ps_data->client->dev,
		"%s: reg[0x%x]=0x%2X\n", __func__, STK_RSRVD_REG,
		ps_reg[cnt]);

	return scnprintf(buf, PAGE_SIZE,
		"[PS=%2X] [ALS=%2X] [WAIT=0x%4Xms] [EN_ASO=%2X] [EN_AK=%2X] [NEAR/FAR=%2X] [FLAG_OUI=%2X] [FLAG_PSINT=%2X] [FLAG_ALSINT=%2X]\n",
		ps_reg[0] & 0x01, (ps_reg[0] & 0x02) >> 1,
		((ps_reg[0] & 0x04) >> 2) * ps_reg[5] * 6,
		(ps_reg[0] & 0x20) >> 5, (ps_reg[0] & 0x40) >> 6,
		ps_reg[16] & 0x01, (ps_reg[16] & 0x04) >> 2,
		(ps_reg[16] & 0x10) >> 4, (ps_reg[16] & 0x20) >> 5);
}

static ssize_t stk_recv_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t stk_recv_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	unsigned long value = 0;
	int ret;
	int32_t recv_data;	
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);	
	
	ret = kstrtoul(buf, 16, &value);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	
	}
	recv_data = stk3x1x_i2c_smbus_read_byte_data(ps_data->client,value);
	dev_dbg(&ps_data->client->dev,
		"%s: reg 0x%x=0x%x\n", __func__, (int)value, recv_data);
	return size;
}

static ssize_t stk_send_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t stk_send_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	int addr, cmd;
	int32_t ret, i;
	char *token[10];
	struct stk3x1x_data *ps_data =  dev_get_drvdata(dev);	
	
	for (i = 0; i < 2; i++)
		token[i] = strsep((char **)&buf, " ");
	ret = kstrtoul(token[0], 16, (unsigned long *)&(addr));
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	
	}
	ret = kstrtoul(token[1], 16, (unsigned long *)&(cmd));
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;	
	}
	dev_dbg(&ps_data->client->dev,
		"%s: write reg 0x%x=0x%x\n", __func__, addr, cmd);

	ret = stk3x1x_i2c_smbus_write_byte_data(ps_data->client,
		(unsigned char)addr, (unsigned char)cmd);
	if (0 != ret) {
		dev_err(&ps_data->client->dev,
			"%s: stk3x1x_i2c_smbus_write_byte_data fail\n",
			__func__);
		return ret;
	}
	
	return size;
}

static struct device_attribute als_enable_attribute =
	__ATTR(enable, 0664, stk_als_enable_show, stk_als_enable_store);
static struct device_attribute als_lux_attribute =
	__ATTR(lux, 0664, stk_als_lux_show, stk_als_lux_store);
static struct device_attribute als_code_attribute =
	__ATTR(code, 0444, stk_als_code_show, NULL);
static struct device_attribute als_transmittance_attribute =
	__ATTR(transmittance, 0664, stk_als_transmittance_show,
	stk_als_transmittance_store);
static struct device_attribute als_ir_code_attribute =
	__ATTR(ircode, 0444, stk_als_ir_code_show, NULL);

static struct attribute *stk_als_attrs [] =
{
	&als_enable_attribute.attr,
	&als_lux_attribute.attr,
	&als_code_attribute.attr,
	&als_transmittance_attribute.attr,
	&als_ir_code_attribute.attr,
	NULL
};

static struct attribute_group stk_als_attribute_group = {
	.name = "driver",
	.attrs = stk_als_attrs,
};

static struct device_attribute ps_enable_attribute =
	__ATTR(enable, 0664, stk_ps_enable_show, stk_ps_enable_store);
static struct device_attribute ps_enable_aso_attribute =
	__ATTR(enableaso, 0664, stk_ps_enable_aso_show,
	stk_ps_enable_aso_store);
static struct device_attribute ps_distance_attribute =
	__ATTR(distance, 0664, stk_ps_distance_show, stk_ps_distance_store);
static struct device_attribute ps_offset_attribute =
	__ATTR(offset, 0664, stk_ps_offset_show, stk_ps_offset_store);
static struct device_attribute ps_code_attribute =
	__ATTR(code, 0444, stk_ps_code_show, NULL);
static struct device_attribute ps_code_thd_l_attribute =
	__ATTR(codethdl, 0664, stk_ps_code_thd_l_show, NULL);
static struct device_attribute ps_code_thd_h_attribute =
	__ATTR(codethdh, 0664, stk_ps_code_thd_h_show, NULL);
static struct device_attribute recv_attribute =
	__ATTR(recv, 0664, stk_recv_show, stk_recv_store);
static struct device_attribute send_attribute =
	__ATTR(send, 0664, stk_send_show, stk_send_store);
static struct device_attribute all_reg_attribute =
	__ATTR(allreg, 0444, stk_all_reg_show, NULL);
static struct device_attribute status_attribute =
	__ATTR(status, 0444, stk_status_show, NULL);

static struct attribute *stk_ps_attrs [] =
{
	&ps_enable_attribute.attr,
	&ps_enable_aso_attribute.attr,
	&ps_distance_attribute.attr,
	&ps_offset_attribute.attr,
	&ps_code_attribute.attr,
	&ps_code_thd_l_attribute.attr,
	&ps_code_thd_h_attribute.attr,	
	&recv_attribute.attr,
	&send_attribute.attr,	
	&all_reg_attribute.attr,
	&status_attribute.attr,
	NULL
};

static struct attribute_group stk_ps_attribute_group = {
	.name = "driver",	
	.attrs = stk_ps_attrs,
};


static void stk_work_func(struct work_struct *work)
{
	uint32_t reading;
	int32_t ret;
	uint8_t disable_flag = 0;
	uint8_t org_flag_reg;
	struct stk3x1x_data *ps_data = container_of(work,
	struct stk3x1x_data, stk_work);
	int32_t als_comperator;
	int32_t thd_l;
	int32_t thd_h;
	
	/* mode 0x01 or 0x04 */	
	org_flag_reg = stk3x1x_get_flag(ps_data);
	if (org_flag_reg < 0) {
		dev_err(&ps_data->client->dev,
			"%s: stk3x1x_get_flag fail, org_flag_reg=%d\n",
			__func__, org_flag_reg);
		goto err_i2c_rw;	
	}	
	
	if (org_flag_reg & STK_FLG_ALSINT_MASK) {
		disable_flag |= STK_FLG_ALSINT_MASK;
		reading = stk3x1x_get_als_reading(ps_data);
		if (reading < 0) {
			dev_err(&ps_data->client->dev,
				"%s: stk3x1x_get_als_reading fail, ret=%d",
				__func__, reading);
			goto err_i2c_rw;
		}
		stk_als_set_new_thd(ps_data, reading);

		if (ps_data->ir_code) {
			if (reading < STK_IRC_MAX_ALS_CODE &&
				reading > STK_IRC_MIN_ALS_CODE &&
				ps_data->ir_code > STK_IRC_MIN_IR_CODE) {
				als_comperator = reading * STK_IRC_ALS_NUMERA /
					STK_IRC_ALS_DENOMI;
				if(ps_data->ir_code > als_comperator)
					ps_data->als_correct_factor =
						STK_IRC_ALS_CORREC;
				else
					ps_data->als_correct_factor =
						STK_ALS_CORRECT_FACTOR;
			}
			dev_dbg(&ps_data->client->dev,
				"%s: als=%d, ir=%d, als_correct_factor=%d",
				__func__, reading, ps_data->ir_code,
				ps_data->als_correct_factor);
			ps_data->ir_code = 0;
		}	

		reading = reading * ps_data->als_correct_factor / 1000;

		ps_data->als_lux_last = stk_alscode2lux(ps_data, reading);
		input_report_abs(ps_data->als_input_dev, ABS_MISC,
			ps_data->als_lux_last);
		input_sync(ps_data->als_input_dev);
		dev_dbg(&ps_data->client->dev, "%s: als input event %d lux\n",
			__func__, ps_data->als_lux_last);
	}

	if (org_flag_reg & STK_FLG_PSINT_MASK) {
		reading = stk3x1x_get_ps_reading(ps_data);
		if (reading < 0) {
			dev_err(&ps_data->client->dev,
				"%s: stk3x1x_get_als_reading fail, ret=%d",
				 __func__, reading);
			goto err_i2c_rw;
		}
		dev_dbg(&ps_data->client->dev, "%s: Prox reading 0x%04x\n",
			__func__, reading);

		if (ps_data->prox_mode == CALIBRATING) {
			ps_data->noise_count--;
			ps_data->noise_floor += reading;

			if (ps_data->noise_count == 0) {
				ps_data->noise_floor >>= 2;
				dev_dbg(&ps_data->client->dev,
					"%s: Noise floor = 0x%04x\n",
					__func__, ps_data->noise_floor);

				if (ps_data->noise_floor <
					ps_data->max_noise_floor) {
					stk_prox_mode_uncovered(ps_data);
				} else {
					dev_dbg(&ps_data->client->dev,
						"%s: Noise floor too high\n",
						__func__);
					ps_data->noise_floor =
						ps_data->max_noise_floor -
						ps_data->uncovered_threshold -
						200;
					stk_prox_mode_covered(ps_data);
				}
			}
		} else if (ps_data->prox_mode == UNCOVERED) {
			thd_l = (int32_t)ps_data->noise_floor -
				(int32_t)ps_data->recal_threshold;
			if (thd_l < 0)
				thd_l = 0;

			thd_h = (int32_t)ps_data->noise_floor +
				(int32_t)ps_data->covered_threshold;
			if (thd_h > 0x0000FFFF)
				thd_h = 0x0000FFFF;

			if (reading < thd_l)
				stk_prox_mode_calibrating(ps_data);
			else if (reading > thd_h)
				stk_prox_mode_covered(ps_data);

		} else if (ps_data->prox_mode == COVERED) {
			thd_l = (int32_t)ps_data->noise_floor +
				(int32_t)ps_data->uncovered_threshold;
			if (thd_l > ps_data->max_noise_floor)
				thd_l = ps_data->max_noise_floor;

			if (reading < thd_l)
				stk_prox_mode_uncovered(ps_data);
		} else {
			dev_err(&ps_data->client->dev,
				"%s: Invalid prox mode\n", __func__);
			stk_prox_mode_calibrating(ps_data);
		}

		disable_flag |= STK_FLG_PSINT_MASK;
	}
	
	ret = stk3x1x_set_flag(ps_data, org_flag_reg, disable_flag);
	if (ret < 0) {
		dev_err(&ps_data->client->dev,
			"%s: reset_int_flag fail, ret=%d\n", __func__, ret);
		goto err_i2c_rw;
	}		

	msleep(1);
	enable_irq(ps_data->irq);
	return;

err_i2c_rw:
	msleep(30);
	enable_irq(ps_data->irq);
	return;	
}

static irqreturn_t stk_oss_irq_handler(int irq, void *data)
{
	struct stk3x1x_data *pData = data;
	disable_irq_nosync(irq);
	if (pData->suspended)
		pData->delayed_work = true;
	else
		queue_work(pData->stk_wq, &pData->stk_work);
	return IRQ_HANDLED;
}

static int32_t stk3x1x_init_all_setting(struct i2c_client *client,
	struct stk3x1x_platform_data *plat_data)
{
	int32_t ret;
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);		
	
	ps_data->als_enabled = false;
	ps_data->ps_enabled = false;
	
	ret = stk3x1x_software_reset(ps_data); 
	if(ret < 0)
		return ret;
	
	ret = stk3x1x_check_pid(ps_data);
	if(ret < 0)
		return ret;
	
	ret = stk3x1x_init_all_reg(ps_data, plat_data);
	if(ret < 0)
		return ret;	
	ps_data->suspended = false;
	ps_data->delayed_work = false;
	ps_data->re_enable_als = false;
	ps_data->ir_code = 0;
	ps_data->als_correct_factor = STK_ALS_CORRECT_FACTOR;
#ifdef STK_ALS_FIR
	memset(&ps_data->fir, 0x00, sizeof(ps_data->fir));  
#endif
	return 0;
}

static int stk3x1x_setup_irq(struct i2c_client *client)
{		
	int irq, err = -EIO;
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);

#ifdef SPREADTRUM_PLATFORM	
	irq = sprd_alloc_gpio_irq(ps_data->int_pin);	
#else	
	irq = gpio_to_irq(ps_data->int_pin);
#endif	
	dev_dbg(&ps_data->client->dev,
		"%s: int pin #=%d, irq=%d\n", __func__, ps_data->int_pin, irq);
	if (irq <= 0) {
		dev_err(&ps_data->client->dev,
			"%s: irq number is not specified, irq # = %d, "
			"int pin=%d\n",
			__func__, irq, ps_data->int_pin);
		return irq;
	}
	ps_data->irq = irq;	
	err = gpio_request(ps_data->int_pin,"stk-int");        
	if (err < 0) {
		dev_err(&ps_data->client->dev,
			"%s: gpio_request, err=%d", __func__, err);
		return err;
	}
	err = gpio_direction_input(ps_data->int_pin);
	if (err < 0) {
		dev_err(&ps_data->client->dev,
			"%s: gpio_direction_input, err=%d", __func__, err);
		return err;
	}		
	err = request_any_context_irq(irq, stk_oss_irq_handler,
		IRQF_TRIGGER_LOW, DEVICE_NAME, ps_data);
	if (err < 0) {
		dev_err(&ps_data->client->dev,
			"%s: request_any_context_irq(%d) failed for (%d)\n",
			__func__, irq, err);
		goto err_request_any_context_irq;
	}
	disable_irq(irq);
	
	return 0;
err_request_any_context_irq:	
#ifdef SPREADTRUM_PLATFORM
	sprd_free_gpio_irq(ps_data->int_pin);	
#else	
	gpio_free(ps_data->int_pin);		
#endif	
	return err;
}

static int stk3x1x_suspend(struct device *dev)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
	struct i2c_client *client = to_i2c_client(dev);
	int err;

	dev_dbg(&ps_data->client->dev,
		"%s: suspend\n", __func__);
	mutex_lock(&ps_data->io_lock);  		
	ps_data->suspended = true;
	if (ps_data->als_enabled) {
		stk3x1x_enable_als(ps_data, 0);		
		ps_data->re_enable_als = true;
	}  	
	if (ps_data->ps_enabled) {
		if (device_may_wakeup(&client->dev)) {
			err = enable_irq_wake(ps_data->irq);	
			if (err)
				dev_err(&ps_data->client->dev,
					"%s: set_irq_wake(%d) failed, "
					"err=(%d)\n",
					__func__, ps_data->irq, err);
		} else {
			dev_err(&ps_data->client->dev,
				"%s: not support wakeup source", __func__);
		}		
	}
	mutex_unlock(&ps_data->io_lock);		
	return 0;	
}

static int stk3x1x_resume(struct device *dev)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);	
	struct i2c_client *client = to_i2c_client(dev);
	int err;
	
	dev_dbg(&ps_data->client->dev,
		"%s: resume\n", __func__);
	mutex_lock(&ps_data->io_lock); 		
	if (ps_data->re_enable_als) {
		stk3x1x_enable_als(ps_data, 1);		
		ps_data->re_enable_als = false;		
	}
	if (ps_data->ps_enabled) {
		if (device_may_wakeup(&client->dev)) {
			err = disable_irq_wake(ps_data->irq);	
			if (err)		
				dev_err(&ps_data->client->dev,
					"%s: disable_irq_wake(%d) "
					"failed, err=(%d)\n",
					__func__, ps_data->irq, err);
		}		
	}
	if (ps_data->delayed_work == true) {
		queue_work(ps_data->stk_wq, &ps_data->stk_work);
		ps_data->delayed_work = false;
	}
	ps_data->suspended = false;
	mutex_unlock(&ps_data->io_lock);
	return 0;	
}

static const struct dev_pm_ops stk3x1x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stk3x1x_suspend, stk3x1x_resume)
};

#ifdef CONFIG_OF
static int stk3x1x_parse_dt(struct device *dev,
			struct stk3x1x_platform_data *pdata)
{
	struct stk3x1x_data *ps_data = dev_get_drvdata(dev);
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	pdata->int_pin = of_get_named_gpio_flags(np, "stk,irq-gpio",
				0, &pdata->int_flags);
	if (pdata->int_pin < 0) {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read irq-gpio\n", __func__);
		return pdata->int_pin;
	}

	rc = of_property_read_u32(np, "stk,transmittance", &temp_val);
	if (!rc)
		pdata->transmittance = temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read transmittance\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,state-reg", &temp_val);
	if (!rc)
		pdata->state_reg = temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read state-reg\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,psctrl-reg", &temp_val);
	if (!rc)
		pdata->psctrl_reg = (u8)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read psctrl-reg\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,alsctrl-reg", &temp_val);
	if (!rc)
		pdata->alsctrl_reg = (u8)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read alsctrl-reg\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,ledctrl-reg", &temp_val);
	if (!rc)
		pdata->ledctrl_reg = (u8)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read ledctrl-reg\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,wait-reg", &temp_val);
	if (!rc)
		pdata->wait_reg = (u8)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read wait-reg\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,als_thresh_pct", &temp_val);
	if (!rc)
		pdata->als_thresh_pct = (u16)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read als_thresh_pct\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,covered_thresh", &temp_val);
	if (!rc)
		pdata->covered_thresh = (u16)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read covered_thresh\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,uncovered_thresh", &temp_val);
	if (!rc)
		pdata->uncovered_thresh = (u16)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read uncovered_thresh\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,recal_thresh", &temp_val);
	if (!rc)
		pdata->recal_thresh = (u16)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read recal_thresh\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(np, "stk,max_noise_fl", &temp_val);
	if (!rc)
		pdata->max_noise_fl = (u16)temp_val;
	else {
		dev_err(&ps_data->client->dev,
			"%s: Unable to read max_noise_fl\n", __func__);
		return rc;
	}

	return 0;
}
#else
static int stk3x1x_parse_dt(struct device *dev,
			struct stk3x1x_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

static int stk3x1x_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
	int err = -ENODEV;
	struct stk3x1x_data *ps_data;
	struct stk3x1x_platform_data *plat_data;
	dev_dbg(&client->dev, "%s: driver version = %s\n", __func__,
		DRIVER_VERSION);
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: No Support for I2C_FUNC_I2C\n",
			__func__);
	    return -ENODEV;
	}

	ps_data = kzalloc(sizeof(struct stk3x1x_data),GFP_KERNEL);
	if (!ps_data) {
		dev_err(&client->dev, "%s: failed to allocate stk3x1x_data\n",
			__func__);
		return -ENOMEM;
	}
	ps_data->client = client;
	i2c_set_clientdata(client, ps_data);
	mutex_init(&ps_data->io_lock);
	wake_lock_init(&ps_data->ps_wakelock, WAKE_LOCK_SUSPEND,
		"stk_input_wakelock");

#ifdef SPREADTRUM_PLATFORM
	if (&stk3x1x_pfdata != NULL) {
		plat_data = &stk3x1x_pfdata;
		ps_data->als_transmittance = plat_data->transmittance;
		ps_data->int_pin = plat_data->int_pin;
		if(ps_data->als_transmittance == 0)
		{
			dev_err(&client->dev,
				"%s: Please set als_transmittance in "
				"platform data\n", __func__);
			goto err_als_input_allocate;
		}
	} else {
		dev_err(&client->dev, "%s: no stk3x1x platform data!\n",
			__func__);
		goto err_als_input_allocate;
	}		
#else
	if (client->dev.of_node) {
		plat_data = devm_kzalloc(&client->dev,
			sizeof(struct stk3x1x_platform_data), GFP_KERNEL);
		if (!plat_data) {
			dev_err(&client->dev, "%s: Failed to allocate memory\n",
				__func__);
			return -ENOMEM;
		}

		err = stk3x1x_parse_dt(&client->dev, plat_data);
		if (err) {
			dev_err(&client->dev,
				"%s: stk3x1x_parse_dt failed ret=%d\n",
				__func__, err);
			return err;
		}
	} else
		plat_data = client->dev.platform_data;

	if (!plat_data) {
		dev_err(&client->dev,
			"%s: no stk3x1x platform data!\n", __func__);
		goto err_als_input_allocate;
	}
	ps_data->int_pin = plat_data->int_pin;
	ps_data->als_transmittance = plat_data->transmittance;
	if (ps_data->als_transmittance == 0) {
		dev_err(&client->dev,
			"%s: Please set als_transmittance\n", __func__);
		goto err_als_input_allocate;
	}
#endif
	ps_data->als_threshold_percent = plat_data->als_thresh_pct;
	ps_data->covered_threshold = plat_data->covered_thresh;
	ps_data->uncovered_threshold = plat_data->uncovered_thresh;
	ps_data->recal_threshold = plat_data->recal_thresh;
	ps_data->max_noise_floor = plat_data->max_noise_fl;

	ps_data->als_input_dev = input_allocate_device();
	if (ps_data->als_input_dev == NULL) {
		dev_err(&client->dev, "%s: could not allocate als device\n",
			__func__);
		err = -ENOMEM;
		goto err_als_input_allocate;
	}
	ps_data->ps_input_dev = input_allocate_device();
	if (ps_data->ps_input_dev == NULL) {
		dev_err(&client->dev, "%s: could not allocate ps device\n",
			__func__);
		err = -ENOMEM;
		goto err_ps_input_allocate;		
	}
	ps_data->als_input_dev->name = ALS_NAME;
	ps_data->ps_input_dev->name = PS_NAME;
	set_bit(EV_ABS, ps_data->als_input_dev->evbit);
	set_bit(EV_ABS, ps_data->ps_input_dev->evbit);
	input_set_abs_params(ps_data->als_input_dev, ABS_MISC, 0,
		stk_alscode2lux(ps_data, (1<<16)-1), 0, 0);
	input_set_abs_params(ps_data->ps_input_dev, ABS_DISTANCE, 0,1, 0, 0);
	err = input_register_device(ps_data->als_input_dev);
	if (err < 0) {
		dev_err(&client->dev, "%s: can not register als input device\n",
			__func__);
		goto err_als_input_register;
	}
	err = input_register_device(ps_data->ps_input_dev);	
	if (err < 0) {
		dev_err(&client->dev, "%s: can not register ps input device\n",
			__func__);
		goto err_ps_input_register;
	}

	err = sysfs_create_group(&ps_data->als_input_dev->dev.kobj,
		&stk_als_attribute_group);
	if (err < 0) {
		dev_err(&client->dev,
			"%s:could not create sysfs group for als\n", __func__);
		goto err_als_sysfs_create_group;
	}
	err = sysfs_create_group(&ps_data->ps_input_dev->dev.kobj,
		&stk_ps_attribute_group);
	if (err < 0) {
		dev_err(&client->dev,
			"%s:could not create sysfs group for ps\n", __func__);
		goto err_ps_sysfs_create_group;
	}
	input_set_drvdata(ps_data->als_input_dev, ps_data);
	input_set_drvdata(ps_data->ps_input_dev, ps_data);	
	
	ps_data->stk_wq = create_singlethread_workqueue("stk_wq");
	INIT_WORK(&ps_data->stk_work, stk_work_func);

	err = stk3x1x_setup_irq(client);
	if(err < 0)
		goto err_stk3x1x_setup_irq;

	device_init_wakeup(&client->dev, true);
	err = stk3x1x_init_all_setting(client, plat_data);
	if(err < 0)
		goto err_init_all_setting;			

	dev_dbg(&client->dev, "%s: probe successfully", __func__);
	return 0;

err_init_all_setting:	
	device_init_wakeup(&client->dev, false);
	free_irq(ps_data->irq, ps_data);
	#ifdef SPREADTRUM_PLATFORM	
		sprd_free_gpio_irq(ps_data->int_pin);		
	#else	
		gpio_free(ps_data->int_pin);	
	#endif	
err_stk3x1x_setup_irq:
	destroy_workqueue(ps_data->stk_wq);	
	sysfs_remove_group(&ps_data->ps_input_dev->dev.kobj,
		&stk_ps_attribute_group);
err_ps_sysfs_create_group:
	sysfs_remove_group(&ps_data->als_input_dev->dev.kobj,
		&stk_als_attribute_group);
err_als_sysfs_create_group:	
	input_unregister_device(ps_data->ps_input_dev);		
err_ps_input_register:
	input_unregister_device(ps_data->als_input_dev);	
err_als_input_register:	
	input_free_device(ps_data->ps_input_dev);	
err_ps_input_allocate:	
	input_free_device(ps_data->als_input_dev);	
err_als_input_allocate:
	wake_lock_destroy(&ps_data->ps_wakelock);
	mutex_destroy(&ps_data->io_lock);
	kfree(ps_data);
	return err;
}


static int stk3x1x_remove(struct i2c_client *client)
{
	struct stk3x1x_data *ps_data = i2c_get_clientdata(client);
	device_init_wakeup(&client->dev, false);
	free_irq(ps_data->irq, ps_data);
	#ifdef SPREADTRUM_PLATFORM	
		sprd_free_gpio_irq(ps_data->int_pin);		
	#else	
		gpio_free(ps_data->int_pin);	
	#endif	
	destroy_workqueue(ps_data->stk_wq);	
	sysfs_remove_group(&ps_data->ps_input_dev->dev.kobj,
		&stk_ps_attribute_group);
	sysfs_remove_group(&ps_data->als_input_dev->dev.kobj,
		&stk_als_attribute_group);
	input_unregister_device(ps_data->ps_input_dev);		
	input_unregister_device(ps_data->als_input_dev);	
	input_free_device(ps_data->ps_input_dev);	
	input_free_device(ps_data->als_input_dev);	
	wake_lock_destroy(&ps_data->ps_wakelock);	
	mutex_destroy(&ps_data->io_lock);
	kfree(ps_data);
	
    return 0;
}

static const struct i2c_device_id stk_ps_id[] =
{
	{ "stk_ps", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, stk_ps_id);

static struct of_device_id stk_match_table[] = {
	{ .compatible = "stk,stk3x1x", },
	{ },
};

static struct i2c_driver stk_ps_driver =
{
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stk_match_table,
		.pm = &stk3x1x_pm_ops,		
	},
	.probe = stk3x1x_probe,
	.remove = stk3x1x_remove,
	.id_table = stk_ps_id,
};


static int __init stk3x1x_init(void)
{
	int ret;
	ret = i2c_add_driver(&stk_ps_driver);
	if (ret)
	{
		i2c_del_driver(&stk_ps_driver);
		return ret;
	}
	return 0;
}

static void __exit stk3x1x_exit(void)
{
	i2c_del_driver(&stk_ps_driver);
}

module_init(stk3x1x_init);
module_exit(stk3x1x_exit);
MODULE_AUTHOR("Lex Hsieh <lex_hsieh@sensortek.com.tw>");
MODULE_DESCRIPTION("Sensortek stk3x1x Proximity Sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
