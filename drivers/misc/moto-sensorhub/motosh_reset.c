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
#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
#include <linux/hssp_programmer.h>
#endif
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

#define MOTOSH_RESET_DELAY		5
#define CAPSENSE_RESET_DELAY		1000

int motosh_load_brightness_table(struct motosh_data *ps_motosh,
		unsigned char *cmdbuff)
{
	int err = -ENOTTY;
	int index = 0;
	cmdbuff[0] = LUX_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[(2 * index) + 1]
			= ps_motosh->pdata->lux_table[index] >> 8;
		cmdbuff[(2 * index) + 2]
			= ps_motosh->pdata->lux_table[index] & 0xFF;
	}
	err = motosh_i2c_write_no_reset(ps_motosh, cmdbuff,
				(2 * LIGHTING_TABLE_SIZE) + 1);
	if (err)
		return err;

	cmdbuff[0] = BRIGHTNESS_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[index + 1]
				= ps_motosh->pdata->brightness_table[index];
	}
	err = motosh_i2c_write_no_reset(ps_motosh, cmdbuff,
			LIGHTING_TABLE_SIZE + 1);
	dev_dbg(&ps_motosh->client->dev, "Brightness tables loaded\n");
	return err;
}

void motosh_reset(struct motosh_platform_data *pdata, unsigned char *cmdbuff)
{
	dev_err(&motosh_misc_data->client->dev, "motosh_reset\n");
	gpio_set_value(pdata->gpio_reset, 0);

	/* Reset the capsense in case it has gotten
	 * into a bad state. This should allow the sensorhub
	 * to recover from the scenario where capsense is preventing
	 * its initialization. */
#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
	if (cycapsense_reset() == -ENODEV)
		msleep(MOTOSH_RESET_DELAY);
	else
		msleep(CAPSENSE_RESET_DELAY);
#else
	msleep(MOTOSH_RESET_DELAY);
#endif

	gpio_set_value(pdata->gpio_reset, 1);
	msleep(MOTOSH_RESET_DELAY);
	motosh_detect_lowpower_mode(cmdbuff);

	if (!motosh_misc_data->in_reset_and_init) {
		/* sending reset to slpc hal */
		motosh_ms_data_buffer_write(motosh_misc_data, DT_RESET,
			NULL, 0, false);
	}
}

/* This routine is now used in 2 stages, it is first called to
   start the reset, and subsequently to complete the init when
   the part acknowledges that it is up
*/
int motosh_reset_and_init(enum reset_mode mode)
{
	struct motosh_platform_data *pdata;
	struct timespec current_time;
	unsigned int i;
	int err = 0, ret_err = 0;
	unsigned char *rst_cmdbuff;
	int mutex_locked = 0;

	dev_dbg(&motosh_misc_data->client->dev,
		 "motosh_reset_and_init %d\n", (int)mode);
	pdata = motosh_misc_data->pdata;
	motosh_misc_data->in_reset_and_init = true;
	wake_lock(&motosh_misc_data->reset_wakelock);

	if (mode == START_RESET) {
		/* Mode will transition to NORMALMODE after
		   hub reports its init is complete */
		motosh_misc_data->mode = UNINITIALIZED;
		motosh_reset(pdata, rst_cmdbuff);
		wake_unlock(&motosh_misc_data->reset_wakelock);
		return ret_err;
	}

	/* INIT */
	rst_cmdbuff = kmalloc(512, GFP_KERNEL);

	if (rst_cmdbuff == NULL) {
		wake_unlock(&motosh_misc_data->reset_wakelock);
		return -ENOMEM;
	}

	motosh_wake(motosh_misc_data);

	/* Part is up and alive, switch to normal mode */
	motosh_misc_data->mode = NORMALMODE;

	motosh_time_sync();

	rst_cmdbuff[0] = SENSOR_ORIENTATIONS;
	rst_cmdbuff[1] = pdata->accel_orient & 0xff;
	rst_cmdbuff[2] = pdata->gyro_orient & 0xff;
	rst_cmdbuff[3] = pdata->mag_orient & 0xff;
	rst_cmdbuff[4] = pdata->panel_type & 0xff;
	rst_cmdbuff[5] = pdata->IR_config & 0xff;
	rst_cmdbuff[6] = pdata->aod_touch_mode & 0xff;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 7);
	if (err < 0) {
		dev_err(&motosh_misc_data->client->dev,
			"Unable to write sensor orientation value");
		ret_err = err;
	}

	rst_cmdbuff[0] = ACCEL_UPDATE_RATE;
	rst_cmdbuff[1] = motosh_g_acc_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = MAG_UPDATE_RATE;
	rst_cmdbuff[1] = motosh_g_mag_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = GYRO_UPDATE_RATE;
	rst_cmdbuff[1] = motosh_g_gyro_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = PRESSURE_UPDATE_RATE;
	rst_cmdbuff[1] = motosh_g_baro_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = ALS_UPDATE_RATE;
	rst_cmdbuff[1] = motosh_g_als_delay >> 8;
	rst_cmdbuff[2] = motosh_g_als_delay  & 0xFF;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = IR_GESTURE_RATE;
	rst_cmdbuff[1] = motosh_g_ir_gesture_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = IR_RAW_RATE;
	rst_cmdbuff[1] = motosh_g_ir_raw_delay;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = ALGO_CONFIG;
	rst_cmdbuff[1] = motosh_g_algo_state & 0xFF;
	rst_cmdbuff[2] = motosh_g_algo_state >> 8;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 3);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = MOTION_DUR;
	rst_cmdbuff[1] = motosh_g_motion_dur;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = ZRMOTION_DUR;
	rst_cmdbuff[1] = motosh_g_zmotion_dur;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 2);
	if (err < 0)
		ret_err = err;

	for (i = 0; i < MOTOSH_NUM_ALGOS; i++) {
		if (motosh_g_algo_requst[i].size > 0) {
			rst_cmdbuff[0] =
				motosh_algo_info[i].req_register;
			memcpy(&rst_cmdbuff[1],
				motosh_g_algo_requst[i].data,
				motosh_g_algo_requst[i].size);
			err = motosh_i2c_write_no_reset(
				motosh_misc_data,
				rst_cmdbuff,
				motosh_g_algo_requst[i].size
				+ 1);
			if (err < 0)
				ret_err = err;
		}
	}

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
	rst_cmdbuff[8] = pdata->ct406_prox_gain & 0xff;
	rst_cmdbuff[9] = (pdata->ct406_als_lux1_c0_mult >> 8) & 0xff;
	rst_cmdbuff[10] = pdata->ct406_als_lux1_c0_mult & 0xff;
	rst_cmdbuff[11] = (pdata->ct406_als_lux1_c1_mult >> 8) & 0xff;
	rst_cmdbuff[12] = pdata->ct406_als_lux1_c1_mult & 0xff;
	rst_cmdbuff[13] = (pdata->ct406_als_lux1_div >> 8) & 0xff;
	rst_cmdbuff[14] = pdata->ct406_als_lux1_div & 0xff;
	rst_cmdbuff[15] = (pdata->ct406_als_lux2_c0_mult >> 8) & 0xff;
	rst_cmdbuff[16] = pdata->ct406_als_lux2_c0_mult & 0xff;
	rst_cmdbuff[17] = (pdata->ct406_als_lux2_c1_mult >> 8) & 0xff;
	rst_cmdbuff[18] = pdata->ct406_als_lux2_c1_mult & 0xff;
	rst_cmdbuff[19] = (pdata->ct406_als_lux2_div >> 8) & 0xff;
	rst_cmdbuff[20] = pdata->ct406_als_lux2_div & 0xff;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 21);
	if (err < 0) {
		dev_err(&motosh_misc_data->client->dev,
			"unable to write proximity settings %d\n", err);
		ret_err = err;
	}

	err = motosh_load_brightness_table(motosh_misc_data,
					   rst_cmdbuff);
	if (err < 0)
		ret_err = err;

	getnstimeofday(&current_time);
	current_time.tv_sec += motosh_time_delta;

	rst_cmdbuff[0] = AP_POSIX_TIME;
	rst_cmdbuff[1] = (unsigned char)(current_time.tv_sec >> 24);
	rst_cmdbuff[2] = (unsigned char)((current_time.tv_sec >> 16)
					& 0xff);
	rst_cmdbuff[3] = (unsigned char)((current_time.tv_sec >> 8)
					& 0xff);
	rst_cmdbuff[4] = (unsigned char)((current_time.tv_sec)
					& 0xff);
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 5);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = MAG_CAL;
	memcpy(&rst_cmdbuff[1], motosh_g_mag_cal,
		MOTOSH_MAG_CAL_SIZE);
	err = motosh_i2c_write_no_reset(motosh_misc_data, rst_cmdbuff,
					MOTOSH_MAG_CAL_SIZE + 1);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = GYRO_CAL_TABLE;
	memcpy(&rst_cmdbuff[1], motosh_g_gyro_cal,
		MOTOSH_GYRO_CAL_SIZE);
	err = motosh_i2c_write_no_reset(motosh_misc_data, rst_cmdbuff,
					MOTOSH_GYRO_CAL_SIZE + 1);
	if (err < 0)
		ret_err = err;

	if (motosh_g_ir_config_reg_restore) {
		rst_cmdbuff[0] = IR_CONFIG;
		memcpy(&rst_cmdbuff[1], motosh_g_ir_config_reg,
				motosh_g_ir_config_reg[0]);
		err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff,
					motosh_g_ir_config_reg[0] + 1);
		if (err < 0)
			ret_err = err;
	}

	/* Activate fast sensors last to reduce likelyhood of
	   early interrupts when completing reset initialization */
	rst_cmdbuff[0] = NONWAKESENSOR_CONFIG;
	rst_cmdbuff[1] = motosh_g_nonwake_sensor_state & 0xFF;
	rst_cmdbuff[2] = (motosh_g_nonwake_sensor_state >> 8) & 0xFF;
	rst_cmdbuff[3] = motosh_g_nonwake_sensor_state >> 16;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 4);
	if (err < 0)
		ret_err = err;

	rst_cmdbuff[0] = WAKESENSOR_CONFIG;
	rst_cmdbuff[1] = (motosh_g_wake_sensor_state) & 0xFF;
	rst_cmdbuff[2] = (motosh_g_wake_sensor_state >>  8) & 0xFF;
	rst_cmdbuff[3] = (motosh_g_wake_sensor_state >> 16) & 0xFF;
	err = motosh_i2c_write_no_reset(motosh_misc_data,
					rst_cmdbuff, 4);
	if (err < 0)
		ret_err = err;

#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
	/* send antcap configuration data */
	memcpy(&rst_cmdbuff[1], motosh_g_antcap_cfg,
	       MOTOSH_ANTCAP_CFG_BUFF_SIZE);
	rst_cmdbuff[0] = ANTCAP_CONFIG;
	err = motosh_i2c_write_no_reset(motosh_misc_data, rst_cmdbuff,
					(MOTOSH_ANTCAP_CFG_BUFF_SIZE + 1));
	if (err) {
		dev_err(&motosh_misc_data->client->dev,
			"unable to write antcap cfg %04x\n", err);
		ret_err = err;
	}

	/* send antcap enable */
	err = motosh_antcap_i2c_send_enable(0);
	dev_info(&motosh_misc_data->client->dev,
		"motosh_antcap enable R (err=%04x): en=%02x st=%02x\n",
		err, motosh_g_antcap_enabled, motosh_g_conn_state);
	if (err)
		ret_err = err;
#endif

	/* sending reset to slpc hal */
	motosh_ms_data_buffer_write(motosh_misc_data, DT_RESET, NULL, 0, false);

	mutex_locked = mutex_trylock(&motosh_misc_data->lock);
	motosh_quickpeek_reset_locked(motosh_misc_data);
	if (mutex_locked)
		mutex_unlock(&motosh_misc_data->lock);

	kfree(rst_cmdbuff);
	motosh_sleep(motosh_misc_data);
	motosh_misc_data->in_reset_and_init = false;
	wake_unlock(&motosh_misc_data->reset_wakelock);

	return ret_err;
}

