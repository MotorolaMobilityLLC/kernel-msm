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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
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

void stml0xx_reset(struct stml0xx_platform_data *pdata, unsigned char *cmdbuff)
{
	dev_err(&stml0xx_misc_data->spi->dev, "stml0xx_reset");
	stml0xx_g_booted = 0;
	msleep(stml0xx_spi_retry_delay);
	gpio_set_value(pdata->gpio_reset, 0);
	msleep(stml0xx_spi_retry_delay);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(STML0XX_RESET_DELAY);
}

void stml0xx_initialize_work_func(struct work_struct *work)
{
	struct stml0xx_data *ps_stml0xx = container_of(work,
		struct stml0xx_data, initialize_work);

	struct stml0xx_platform_data *pdata;
	unsigned char *rst_cmdbuff;
	unsigned char buf[SPI_MSG_SIZE];
	unsigned int i;
	int err;
	int ret_err = 0;

	pdata = ps_stml0xx->pdata;
	rst_cmdbuff = ps_stml0xx->spi_tx_buf;

	if (rst_cmdbuff == NULL)
		return;

	wake_lock(&ps_stml0xx->reset_wakelock);

	stml0xx_spi_retry_delay = 200;

	stml0xx_wake(ps_stml0xx);

	stml0xx_detect_lowpower_mode();

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

	stml0xx_spi_retry_delay = 10;

	buf[0] = stml0xx_g_nonwake_sensor_state & 0xFF;
	buf[1] = (stml0xx_g_nonwake_sensor_state >> 8) & 0xFF;
	buf[2] = stml0xx_g_nonwake_sensor_state >> 16;
	err = stml0xx_spi_send_write_reg_reset(NONWAKESENSOR_CONFIG, buf,
			3, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_wake_sensor_state & 0xFF;
	buf[1] = stml0xx_g_wake_sensor_state >> 8;
	err = stml0xx_spi_send_write_reg_reset(WAKESENSOR_CONFIG, buf,
			2, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_algo_state & 0xFF;
	buf[1] = stml0xx_g_algo_state >> 8;
	err = stml0xx_spi_send_write_reg_reset(ALGO_CONFIG, buf, 2,
			RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_motion_dur;
	err = stml0xx_spi_send_write_reg_reset(MOTION_DUR, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	buf[0] = stml0xx_g_zmotion_dur;
	err = stml0xx_spi_send_write_reg_reset(ZRMOTION_DUR, buf,
			1, RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err = err;

	for (i = 0; i < STML0XX_NUM_ALGOS; i++) {
		if (stml0xx_g_algo_requst[i].size > 0) {
			memcpy(buf,
			       stml0xx_g_algo_requst[i].data,
			       stml0xx_g_algo_requst[i].size);
			err =
			    stml0xx_spi_send_write_reg_reset(
						       stml0xx_algo_info
						       [i].req_register, buf,
						       stml0xx_g_algo_requst
						       [i].size,
						       RESET_NOT_ALLOWED);
			if (err < 0)
				ret_err = err;
		}
	}

	buf[0] = (pdata->ct406_detect_threshold >> 8) & 0xff;
	buf[1] = pdata->ct406_detect_threshold & 0xff;
	buf[2] = (pdata->ct406_undetect_threshold >> 8) & 0xff;
	buf[3] = pdata->ct406_undetect_threshold & 0xff;
	buf[4] = (pdata->ct406_recalibrate_threshold >> 8) & 0xff;
	buf[5] = pdata->ct406_recalibrate_threshold & 0xff;
	buf[6] = pdata->ct406_pulse_count & 0xff;
	err = stml0xx_spi_send_write_reg_reset(PROX_SETTINGS, buf,
			7, RESET_NOT_ALLOWED);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"unable to write proximity settings %d", err);
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
	}
	err = stml0xx_led_set_reset(&ps_stml0xx->led_cdev,
			RESET_NOT_ALLOWED);
	if (err < 0)
		ret_err =  err;

	/* sending reset to slpc hal */
	stml0xx_ms_data_buffer_write(ps_stml0xx, DT_RESET, NULL, 0);

	stml0xx_sleep(ps_stml0xx);
	wake_unlock(&ps_stml0xx->reset_wakelock);

	if (ret_err >= 0)
		dev_err(&ps_stml0xx->spi->dev, "Sensor Hub initialization successful");
	else
		dev_err(&ps_stml0xx->spi->dev, "Sensor Hub initialization failed");

}
