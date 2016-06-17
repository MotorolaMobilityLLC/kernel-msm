/*
 * Copyright (C) 2010-2014 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
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

#include <linux/stml0xx.h>

void stml0xx_reset(struct stml0xx_platform_data *pdata)
{
	dev_err(&stml0xx_misc_data->spi->dev, "stml0xx_reset");
	stml0xx_g_booted = 0;

	if (pdata->reset_hw_type != 0)
		gpio_direction_output(pdata->gpio_reset, 1);

	msleep(stml0xx_spi_retry_delay);
	gpio_set_value(pdata->gpio_reset, 0);
	msleep(stml0xx_spi_retry_delay);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(STML0XX_RESET_DELAY);

	if (pdata->reset_hw_type != 0)
		gpio_direction_input(pdata->gpio_reset);
}

void stml0xx_initialize_work_func(struct work_struct *work)
{
	struct stml0xx_data *ps_stml0xx = container_of(work,
		struct stml0xx_data, initialize_work);

	struct stml0xx_platform_data *pdata;
	unsigned char buf[SPI_MSG_SIZE];
	unsigned int i;
	int err;
	int ret_err = 0;

	pdata = ps_stml0xx->pdata;

	wake_lock(&ps_stml0xx->reset_wakelock);

	stml0xx_spi_retry_delay = 200;

	stml0xx_wake(ps_stml0xx);

	stml0xx_detect_lowpower_mode();

	if ((pdata->accel_orientation_1 > 0) ||
				(pdata->accel_orientation_2 > 0)) {
		buf[0] = pdata->accel_orientation_1 & 0xff;
		buf[1] = pdata->accel_orientation_2 & 0xff;
		err = stml0xx_spi_send_write_reg_reset(ACCEL_ORIENTATION, buf,
				2, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
				"Unable to write accel orientation value");
			ret_err = err;
		}
	}

	/* Only send accel swap value if different from the default. */
	if (pdata->accel_swap != 0) {
		buf[0] = pdata->accel_swap & 0xff;
		err = stml0xx_spi_send_write_reg_reset(ACCEL_SWAP, buf,
			1, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
				"Unable to write accel swap value");
			ret_err = err;
		}
	}

	buf[0] = stml0xx_g_acc_delay;
	err = stml0xx_spi_send_write_reg_reset(ACCEL_UPDATE_RATE, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_acc2_delay;
	err = stml0xx_spi_send_write_reg_reset(ACCEL2_UPDATE_RATE, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_gyro_delay;
	err = stml0xx_spi_send_write_reg_reset(GYRO_UPDATE_RATE, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_mag_delay;
	err = stml0xx_spi_send_write_reg_reset(MAG_UPDATE_RATE, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	stml0xx_spi_retry_delay = 10;

	buf[0] = stml0xx_g_als_delay >> 8;
	buf[1] = stml0xx_g_als_delay & 0xFF;
	err = stml0xx_spi_send_write_reg_reset(ALS_UPDATE_RATE, buf,
			2, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_step_counter_delay >> 8;
	buf[1] = stml0xx_g_step_counter_delay & 0xFF;
	err = stml0xx_spi_send_write_reg_reset(STEP_COUNTER_INFO, buf,
			2, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = (pdata->ct406_detect_threshold >> 8) & 0xff;
	buf[1] = pdata->ct406_detect_threshold & 0xff;
	buf[2] = (pdata->ct406_undetect_threshold >> 8) & 0xff;
	buf[3] = pdata->ct406_undetect_threshold & 0xff;
	buf[4] = (pdata->ct406_recalibrate_threshold >> 8) & 0xff;
	buf[5] = pdata->ct406_recalibrate_threshold & 0xff;
	buf[6] = pdata->ct406_pulse_count & 0xff;
	buf[7] = pdata->ct406_prox_gain & 0xff;
	buf[8] = (pdata->ct406_als_lux1_c0_mult >> 8) & 0xff;
	buf[9] = pdata->ct406_als_lux1_c0_mult & 0xff;
	buf[10] = (pdata->ct406_als_lux1_c1_mult >> 8) & 0xff;
	buf[11] = pdata->ct406_als_lux1_c1_mult & 0xff;
	buf[12] = (pdata->ct406_als_lux1_div >> 8) & 0xff;
	buf[13] = pdata->ct406_als_lux1_div & 0xff;
	buf[14] = (pdata->ct406_als_lux2_c0_mult >> 8) & 0xff;
	buf[15] = pdata->ct406_als_lux2_c0_mult & 0xff;
	buf[16] = (pdata->ct406_als_lux2_c1_mult >> 8) & 0xff;
	buf[17] = pdata->ct406_als_lux2_c1_mult & 0xff;
	buf[18] = (pdata->ct406_als_lux2_div >> 8) & 0xff;
	buf[19] = pdata->ct406_als_lux2_div & 0xff;
	err = stml0xx_spi_send_write_reg_reset(PROX_ALS_SETTINGS, buf,
			20, RESET_NOT_ALLOWED);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"unable to write proximity settings %d", err);
		ret_err = err;
	}

	buf[0] = pdata->dsp_iface_enable & 0xff;
	err = stml0xx_spi_send_write_reg_reset(DSP_CONTROL, buf,
		1, RESET_NOT_ALLOWED);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"Unable to write dsp interface enable");
		ret_err = err;
	}

	buf[0] = (pdata->headset_insertion_debounce >> 8) & 0xff;
	buf[1] = pdata->headset_insertion_debounce & 0xff;
	buf[2] = (pdata->headset_removal_debounce >> 8) & 0xff;
	buf[3] = pdata->headset_removal_debounce & 0xff;
	buf[4] = (pdata->headset_button_down_debounce >> 8) & 0xff;
	buf[5] = pdata->headset_button_down_debounce & 0xff;
	buf[6] = (pdata->headset_button_up_debounce >> 8) & 0xff;
	buf[7] = pdata->headset_button_up_debounce & 0xff;
	buf[8] = (pdata->headset_button_0_1_threshold >> 8) & 0xff;
	buf[9] = pdata->headset_button_0_1_threshold & 0xff;
	buf[10] = (pdata->headset_button_1_2_threshold >> 8) & 0xff;
	buf[11] = pdata->headset_button_1_2_threshold & 0xff;
	buf[12] = (pdata->headset_button_2_3_threshold >> 8) & 0xff;
	buf[13] = pdata->headset_button_2_3_threshold & 0xff;
	buf[14] = (pdata->headset_button_3_upper_threshold >> 8) & 0xff;
	buf[15] = pdata->headset_button_3_upper_threshold & 0xff;
	err = stml0xx_spi_send_write_reg_reset(HEADSET_SETTINGS, buf,
			16, RESET_NOT_ALLOWED);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"unable to write headset settings %d", err);
		ret_err = err;
	} else {
		buf[0] = pdata->headset_hw_version & 0xFF;
		err = stml0xx_spi_send_write_reg_reset(HEADSET_HW_VER, buf,
			1, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
				"Unable to write headset hw version");
			ret_err = err;
		}
	}
	if (err >= 0) {
		buf[0] = pdata->headset_detect_enable & 0xFF;
		err = stml0xx_spi_send_write_reg_reset(HEADSET_CONTROL, buf,
			1, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
				"Unable to write headset detect enable");
			ret_err = err;
		}
	}
	if (err >= 0) {
		memcpy(buf, stml0xx_g_gyro_cal, STML0XX_GYRO_CAL_FIRST);
		err = stml0xx_spi_send_write_reg_reset(GYRO_CAL, buf,
				STML0XX_GYRO_CAL_FIRST, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
					"Unable to write gyro calibration");
			ret_err = err;
		}
		memcpy(buf, stml0xx_g_gyro_cal + STML0XX_GYRO_CAL_FIRST,
				STML0XX_GYRO_CAL_SECOND);
		err = stml0xx_spi_send_write_reg_reset(GYRO_CAL_2, buf,
				STML0XX_GYRO_CAL_SECOND, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
					"Unable to write gyro calibration");
			ret_err = err;
		}
	}
	if (err >= 0) {
		memcpy(buf, stml0xx_g_accel_cal, STML0XX_ACCEL_CAL_SIZE);
		err = stml0xx_spi_send_write_reg_reset(ACCEL_CAL, buf,
				STML0XX_ACCEL_CAL_SIZE, RESET_NOT_ALLOWED);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
					"Unable to write accel calibration");
			ret_err = err;
		}
	}
#ifdef CONFIG_SENSORHUB_DEBUG_LOGGING
	buf[0] = SH_LOG_DEBUG;
	err = stml0xx_spi_send_write_reg_reset(SH_LOG_LEVEL, buf,
		1, RESET_NOT_ALLOWED);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"Unable to write sh log level");
		ret_err = err;
	}
#endif
	/* Enable of algos and sensors should be the last things written */

	buf[0] = stml0xx_g_motion_dur;
	err = stml0xx_spi_send_write_reg_reset(MOTION_DUR, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_zmotion_dur;
	err = stml0xx_spi_send_write_reg_reset(ZMOTION_DUR, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_algo_state & 0xFF;
	buf[1] = stml0xx_g_algo_state >> 8;
	err = stml0xx_spi_send_write_reg_reset(ALGO_CONFIG, buf, 2,
			RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	for (i = 0; i < STML0XX_NUM_ALGOS; i++) {
		if (stml0xx_g_algo_request[i].size > 0) {
			memcpy(buf,
			       stml0xx_g_algo_request[i].data,
			       stml0xx_g_algo_request[i].size);
			err =
			    stml0xx_spi_send_write_reg_reset(
						       stml0xx_algo_info
						       [i].req_register, buf,
						       stml0xx_g_algo_request
						       [i].size,
						       RESET_NOT_ALLOWED);
			if (err < 0)
				ret_err = err;
		}
	}

	/* Enable sensors last after other initialization is done */
	buf[0] = stml0xx_g_nonwake_sensor_state & 0xFF;
	buf[1] = (stml0xx_g_nonwake_sensor_state >> 8) & 0xFF;
	buf[2] = stml0xx_g_nonwake_sensor_state >> 16;
	err = stml0xx_spi_send_write_reg_reset(NONWAKESENSOR_CONFIG, buf,
			3, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_wake_sensor_state & 0xFF;
	buf[1] = (stml0xx_g_wake_sensor_state >> 8) & 0xFF;
	buf[2] = (stml0xx_g_wake_sensor_state >> 16) & 0xFF;
	err = stml0xx_spi_send_write_reg_reset(WAKESENSOR_CONFIG, buf,
			3, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	/* sending reset to slpc hal */
	stml0xx_ms_data_buffer_write(ps_stml0xx, DT_RESET, NULL, 0);

	stml0xx_sleep(ps_stml0xx);
	wake_unlock(&ps_stml0xx->reset_wakelock);

	if (ret_err >= 0)
		dev_err(&ps_stml0xx->spi->dev,
				"Sensor Hub initialization successful");
	else
		dev_err(&ps_stml0xx->spi->dev,
				"Sensor Hub initialization failed");

}
