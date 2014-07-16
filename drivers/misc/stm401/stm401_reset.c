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


int stm401_load_brightness_table(struct stm401_data *ps_stm401,
		unsigned char *cmdbuff)
{
	int err = -ENOTTY;
	int index = 0;
	cmdbuff[0] = LUX_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[(2 * index) + 1]
			= ps_stm401->pdata->lux_table[index] >> 8;
		cmdbuff[(2 * index) + 2]
			= ps_stm401->pdata->lux_table[index] & 0xFF;
	}
	err = stm401_i2c_write_no_reset(ps_stm401, cmdbuff,
				(2 * LIGHTING_TABLE_SIZE) + 1);
	if (err)
		return err;

	cmdbuff[0] = BRIGHTNESS_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[index + 1]
				= ps_stm401->pdata->brightness_table[index];
	}
	err = stm401_i2c_write_no_reset(ps_stm401, cmdbuff,
			LIGHTING_TABLE_SIZE + 1);
	dev_dbg(&ps_stm401->client->dev, "Brightness tables loaded\n");
	return err;
}

void stm401_reset(struct stm401_platform_data *pdata, unsigned char *cmdbuff)
{
	dev_err(&stm401_misc_data->client->dev, "stm401_reset\n");
	msleep(stm401_i2c_retry_delay);
	gpio_set_value(pdata->gpio_reset, 0);
	msleep(STM401_RESET_DELAY);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(STM401_RESET_DELAY);
	stm401_detect_lowpower_mode(cmdbuff);
}

int stm401_reset_and_init(void)
{
	struct stm401_platform_data *pdata;
	struct timespec current_time;
	unsigned int i;
	int err, ret_err = 0;
	int mutex_locked = 0;
	unsigned char *rst_cmdbuff = kmalloc(512, GFP_KERNEL);
	dev_dbg(&stm401_misc_data->client->dev, "stm401_reset_and_init\n");

	if (rst_cmdbuff == NULL)
		return -ENOMEM;

	wake_lock(&stm401_misc_data->reset_wakelock);

	pdata = stm401_misc_data->pdata;

	stm401_reset(pdata, rst_cmdbuff);
	stm401_i2c_retry_delay = 200;

	stm401_wake(stm401_misc_data);

	rst_cmdbuff[0] = ACCEL_UPDATE_RATE;
	rst_cmdbuff[1] = stm401_g_acc_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	stm401_i2c_retry_delay = 13;

	rst_cmdbuff[0] = MAG_UPDATE_RATE;
	rst_cmdbuff[1] = stm401_g_mag_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = GYRO_UPDATE_RATE;
	rst_cmdbuff[1] = stm401_g_gyro_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = PRESSURE_UPDATE_RATE;
	rst_cmdbuff[1] = stm401_g_baro_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = IR_GESTURE_RATE;
	rst_cmdbuff[1] = stm401_g_ir_gesture_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = IR_RAW_RATE;
	rst_cmdbuff[1] = stm401_g_ir_raw_delay;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = NONWAKESENSOR_CONFIG;
	rst_cmdbuff[1] = stm401_g_nonwake_sensor_state & 0xFF;
	rst_cmdbuff[2] = (stm401_g_nonwake_sensor_state >> 8) & 0xFF;
	rst_cmdbuff[3] = stm401_g_nonwake_sensor_state >> 16;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 4);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = WAKESENSOR_CONFIG;
	rst_cmdbuff[1] = stm401_g_wake_sensor_state & 0xFF;
	rst_cmdbuff[2] = stm401_g_wake_sensor_state >> 8;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = ALGO_CONFIG;
	rst_cmdbuff[1] = stm401_g_algo_state & 0xFF;
	rst_cmdbuff[2] = stm401_g_algo_state >> 8;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = MOTION_DUR;
	rst_cmdbuff[1] = stm401_g_motion_dur;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = ZRMOTION_DUR;
	rst_cmdbuff[1] = stm401_g_zmotion_dur;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	for (i = 0; i < STM401_NUM_ALGOS; i++) {
		if (stm401_g_algo_requst[i].size > 0) {
			rst_cmdbuff[0] = stm401_algo_info[i].req_register;
			memcpy(&rst_cmdbuff[1],
				stm401_g_algo_requst[i].data,
				stm401_g_algo_requst[i].size);
			err = stm401_i2c_write_no_reset(stm401_misc_data,
				rst_cmdbuff,
				stm401_g_algo_requst[i].size + 1);
			if (err < 0)
				ret_err = err;
		}
	}
	rst_cmdbuff[0] = INTERRUPT_STATUS;
	stm401_i2c_write_read_no_reset(stm401_misc_data, rst_cmdbuff, 1, 3);
	rst_cmdbuff[0] = WAKESENSOR_STATUS;
	stm401_i2c_write_read_no_reset(stm401_misc_data, rst_cmdbuff, 1, 2);

	rst_cmdbuff[0] = PROX_SETTINGS;
	rst_cmdbuff[1]
		= (pdata->ct406_detect_threshold >> 8) & 0xff;
	rst_cmdbuff[2]
		= pdata->ct406_detect_threshold & 0xff;
	rst_cmdbuff[3] = (pdata->ct406_undetect_threshold >> 8) & 0xff;
	rst_cmdbuff[4] = pdata->ct406_undetect_threshold & 0xff;
	rst_cmdbuff[5]
		= (pdata->ct406_recalibrate_threshold >> 8) & 0xff;
	rst_cmdbuff[6] = pdata->ct406_recalibrate_threshold & 0xff;
	rst_cmdbuff[7] = pdata->ct406_pulse_count & 0xff;
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff, 8);
	if (err < 0) {
		dev_err(&stm401_misc_data->client->dev,
			"unable to write proximity settings %d\n", err);
		ret_err = err;
	}

	err = stm401_load_brightness_table(stm401_misc_data, rst_cmdbuff);
	if (err < 0)
		ret_err = err;

	getnstimeofday(&current_time);
	current_time.tv_sec += stm401_time_delta;

	rst_cmdbuff[0] = AP_POSIX_TIME;
	rst_cmdbuff[1] = (unsigned char)(current_time.tv_sec >> 24);
	rst_cmdbuff[2] = (unsigned char)((current_time.tv_sec >> 16)
		& 0xff);
	rst_cmdbuff[3] = (unsigned char)((current_time.tv_sec >> 8)
		& 0xff);
	rst_cmdbuff[4] = (unsigned char)((current_time.tv_sec)
		& 0xff);
	err = stm401_i2c_write_no_reset(stm401_misc_data,
					rst_cmdbuff, 5);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = MAG_CAL;
	memcpy(&rst_cmdbuff[1], stm401_g_mag_cal, STM401_MAG_CAL_SIZE);
	err = stm401_i2c_write_no_reset(stm401_misc_data, rst_cmdbuff,
		STM401_MAG_CAL_SIZE);
	if (err < 0)
		ret_err = err;

	if (stm401_g_ir_config_reg_restore) {
		rst_cmdbuff[0] = IR_CONFIG;
		memcpy(&rst_cmdbuff[1], stm401_g_ir_config_reg,
			stm401_g_ir_config_reg[0]);
		err = stm401_i2c_write_no_reset(stm401_misc_data,
						rst_cmdbuff,
						stm401_g_ir_config_reg[0] + 1);
		if (err < 0)
			ret_err = err;
	}

	/* sending reset to slpc hal */
	stm401_ms_data_buffer_write(stm401_misc_data, DT_RESET,
		NULL, 0);

	mutex_locked = mutex_trylock(&stm401_misc_data->lock);
	stm401_quickpeek_reset_locked(stm401_misc_data);
	if (mutex_locked)
		mutex_unlock(&stm401_misc_data->lock);

	kfree(rst_cmdbuff);
	stm401_sleep(stm401_misc_data);
	wake_unlock(&stm401_misc_data->reset_wakelock);
	return ret_err;
}
