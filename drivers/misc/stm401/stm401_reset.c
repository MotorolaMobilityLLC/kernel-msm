/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
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

#include <linux/cdev.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
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

#include <linux/stm401.h>


int stm401_load_brightness_table(struct stm401_data *ps_stm401)
{
	int err = -ENOTTY;
	int index = 0;
	stm401_cmdbuff[0] = LUX_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		stm401_cmdbuff[(2 * index) + 1]
			= ps_stm401->pdata->lux_table[index] >> 8;
		stm401_cmdbuff[(2 * index) + 2]
			= ps_stm401->pdata->lux_table[index] & 0xFF;
	}
	err = stm401_i2c_write_no_reset(ps_stm401, stm401_cmdbuff,
				(2 * LIGHTING_TABLE_SIZE) + 1);
	if (err)
		return err;

	stm401_cmdbuff[0] = BRIGHTNESS_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		stm401_cmdbuff[index + 1]
				= ps_stm401->pdata->brightness_table[index];
	}
	err = stm401_i2c_write_no_reset(ps_stm401, stm401_cmdbuff,
			LIGHTING_TABLE_SIZE + 1);
	dev_dbg(&ps_stm401->client->dev, "Brightness tables loaded\n");
	return err;
}

void stm401_reset(struct stm401_platform_data *pdata)
{
	dev_err(&stm401_misc_data->client->dev, "stm401_reset\n");
	msleep_interruptible(stm401_i2c_retry_delay);
	gpio_set_value(pdata->gpio_reset, 0);
	msleep_interruptible(stm401_i2c_retry_delay);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep_interruptible(STM401_RESET_DELAY);
}

int stm401_reset_and_init(void)
{
	struct stm401_platform_data *pdata;
	struct timespec current_time;
	int stm401_req_gpio;
	int stm401_req_value;
	unsigned int i;
	int err, ret_err = 0;

	pdata = stm401_misc_data->pdata;

	if (stm401_misc_data->ap_stm401_handoff_ctrl) {
		stm401_req_gpio = pdata->gpio_mipi_req;
		stm401_req_value = gpio_get_value(stm401_req_gpio);
		if (stm401_req_value)
			gpio_set_value(stm401_req_gpio, 0);
	} else {
		stm401_req_value = 0;
	}

	stm401_reset(pdata);
	stm401_i2c_retry_delay = 200;

	stm401_cmdbuff[0] = ACCEL_UPDATE_RATE;
	stm401_cmdbuff[1] = stm401_g_acc_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_i2c_retry_delay = 10;

	stm401_cmdbuff[0] = MAG_UPDATE_RATE;
	stm401_cmdbuff[1] = stm401_g_mag_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = GYRO_UPDATE_RATE;
	stm401_cmdbuff[1] = stm401_g_gyro_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = PRESSURE_UPDATE_RATE;
	stm401_cmdbuff[1] = stm401_g_baro_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = NONWAKESENSOR_CONFIG;
	stm401_cmdbuff[1] = stm401_g_nonwake_sensor_state & 0xFF;
	stm401_cmdbuff[2] = stm401_g_nonwake_sensor_state >> 8;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = WAKESENSOR_CONFIG;
	stm401_cmdbuff[1] = stm401_g_wake_sensor_state & 0xFF;
	stm401_cmdbuff[2] = stm401_g_wake_sensor_state >> 8;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = ALGO_CONFIG;
	stm401_cmdbuff[1] = stm401_g_algo_state & 0xFF;
	stm401_cmdbuff[2] = stm401_g_algo_state >> 8;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = MOTION_DUR;
	stm401_cmdbuff[1] = stm401_g_motion_dur;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_cmdbuff[0] = ZRMOTION_DUR;
	stm401_cmdbuff[1] = stm401_g_zmotion_dur;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	for (i = 0; i < STM401_NUM_ALGOS; i++) {
		if (stm401_g_algo_requst[i].size > 0) {
			stm401_cmdbuff[0] = stm401_algo_info[i].req_register;
			memcpy(&stm401_cmdbuff[1],
				stm401_g_algo_requst[i].data,
				stm401_g_algo_requst[i].size);
			err = stm401_i2c_write_no_reset(stm401_misc_data,
				stm401_cmdbuff,
				stm401_g_algo_requst[i].size + 1);
		}
	}

	stm401_cmdbuff[0] = PROX_SETTINGS;
	stm401_cmdbuff[1]
		= (pdata->ct406_detect_threshold >> 8) & 0xff;
	stm401_cmdbuff[2]
		= pdata->ct406_detect_threshold & 0xff;
	stm401_cmdbuff[3] = (pdata->ct406_undetect_threshold >> 8) & 0xff;
	stm401_cmdbuff[4] = pdata->ct406_undetect_threshold & 0xff;
	stm401_cmdbuff[5]
		= (pdata->ct406_recalibrate_threshold >> 8) & 0xff;
	stm401_cmdbuff[6] = pdata->ct406_recalibrate_threshold & 0xff;
	stm401_cmdbuff[7] = pdata->ct406_pulse_count & 0xff;
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff, 8);
	if (err < 0) {
		dev_err(&stm401_misc_data->client->dev,
			"unable to write proximity settings %d\n", err);
		ret_err = err;
	}

	err = stm401_load_brightness_table(stm401_misc_data);
	if (err < 0)
		ret_err = err;

	if (stm401_req_value) {
		getnstimeofday(&current_time);
		current_time.tv_sec += stm401_time_delta;

		stm401_cmdbuff[0] = AP_POSIX_TIME;
		stm401_cmdbuff[1] = (unsigned char)(current_time.tv_sec >> 24);
		stm401_cmdbuff[2] = (unsigned char)((current_time.tv_sec >> 16)
			& 0xff);
		stm401_cmdbuff[3] = (unsigned char)((current_time.tv_sec >> 8)
			& 0xff);
		stm401_cmdbuff[4] = (unsigned char)((current_time.tv_sec)
			& 0xff);
		err = stm401_i2c_write_no_reset(stm401_misc_data,
						stm401_cmdbuff, 5);
		if (err < 0)
			ret_err = err;

		stm401_cmdbuff[0] = STM401_CONTROL_REG;
		memcpy(&stm401_cmdbuff[1], stm401_g_control_reg,
			STM401_CONTROL_REG_SIZE);
		err = stm401_i2c_write_no_reset(stm401_misc_data,
			stm401_cmdbuff,
			STM401_CONTROL_REG_SIZE);
		if (err < 0)
			ret_err = err;

		gpio_set_value(stm401_req_gpio, 1);
	}

	stm401_cmdbuff[0] = MAG_CAL;
	memcpy(&stm401_cmdbuff[1], stm401_g_mag_cal, STM401_MAG_CAL_SIZE);
	err = stm401_i2c_write_no_reset(stm401_misc_data, stm401_cmdbuff,
		STM401_MAG_CAL_SIZE);
	if (err < 0)
		ret_err = err;

	return ret_err;
}
