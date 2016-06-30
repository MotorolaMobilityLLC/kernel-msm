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
#include <linux/list.h>
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
#include <linux/dropbox.h>
#include <linux/string.h>

#ifdef CONFIG_SENSORS_SH_AK09912
#include <linux/stml0xx_akm.h>
#endif

#include <linux/stml0xx.h>

#define NAME                "stml0xx"

#define STML0XX_DELAY_USEC	10
#define G_MAX				0x7FFF

#define STML0XX_BUSY_SLEEP_USEC    10000
#define STML0XX_BUSY_RESUME_COUNT  14
#define STML0XX_BUSY_SUSPEND_COUNT 6
#define STML0XX_SPI_FAIL_LIMIT     10

#define DROPBOX_SENSORHUB_POWERON_ISSUE "sensorhub_poweron_issue"

long stml0xx_time_delta;
unsigned int stml0xx_irq_disable;
module_param_named(irq_disable, stml0xx_irq_disable, uint, 0644);

unsigned short stml0xx_spi_retry_delay = 10;

/* Remember the sensor state */
unsigned short stml0xx_g_acc_delay;
unsigned short stml0xx_g_acc2_delay;
unsigned short stml0xx_g_mag_delay;
unsigned short stml0xx_g_gyro_delay;
unsigned short stml0xx_g_baro_delay;
unsigned short stml0xx_g_als_delay;
unsigned short stml0xx_g_step_counter_delay;
unsigned long stml0xx_g_nonwake_sensor_state;
unsigned long stml0xx_g_wake_sensor_state;
unsigned short stml0xx_g_algo_state;
unsigned char stml0xx_g_motion_dur;
unsigned char stml0xx_g_zmotion_dur;
unsigned char stml0xx_g_control_reg[STML0XX_CONTROL_REG_SIZE];
unsigned char stml0xx_g_mag_cal[STML0XX_MAG_CAL_SIZE];
unsigned char stml0xx_g_gyro_cal[STML0XX_GYRO_CAL_SIZE];
unsigned char stml0xx_g_accel_cal[STML0XX_ACCEL_CAL_SIZE];
unsigned short stml0xx_g_control_reg_restore;
bool stml0xx_g_booted;

/* Store log message */
unsigned char stat_string[LOG_MSG_SIZE + 1];

struct stml0xx_algo_request_t stml0xx_g_algo_request[STML0XX_NUM_ALGOS];

unsigned char *stml0xx_boot_cmdbuff;
unsigned char *stml0xx_boot_readbuff;

/* per algo config, request, and event registers */
const struct stml0xx_algo_info_t stml0xx_algo_info[STML0XX_NUM_ALGOS] = {
	{M_ALGO_MODALITY, 0, ALGO_REQ_MODALITY,
	 ALGO_EVT_MODALITY, STML0XX_EVT_SZ_TRANSITION},
	{M_ALGO_ORIENTATION, 0, ALGO_REQ_ORIENT,
	 ALGO_EVT_ORIENT, STML0XX_EVT_SZ_TRANSITION},
	{M_ALGO_STOWED, 0, ALGO_REQ_STOWED,
	 ALGO_EVT_STOWED, STML0XX_EVT_SZ_TRANSITION},
	{M_ALGO_ACCUM_MODALITY, ALGO_ACCUM_ALL_MODALITY,
	 ALGO_REQ_ACCUM_MODALITY, ALGO_EVT_ACCUM_MODALITY,
	 STML0XX_EVT_SZ_ACCUM_STATE},
	{M_ALGO_ACCUM_MVMT, 0, ALGO_REQ_ACCUM_MVMT,
	 ALGO_EVT_ACCUM_MVMT, STML0XX_EVT_SZ_ACCUM_MVMT}
};

struct stml0xx_data *stml0xx_misc_data;

void stml0xx_wake(struct stml0xx_data *ps_stml0xx)
{
	if (ps_stml0xx != NULL && ps_stml0xx->pdata != NULL) {
		mutex_lock(&ps_stml0xx->sh_wakeup_lock);

		if (ps_stml0xx->sh_wakeup_count == 0 &&
				ps_stml0xx->sh_lowpower_enabled)
			gpio_set_value(ps_stml0xx->pdata->gpio_sh_wake, 1);

		ps_stml0xx->sh_wakeup_count++;

		mutex_unlock(&ps_stml0xx->sh_wakeup_lock);
	}
}

void stml0xx_sleep(struct stml0xx_data *ps_stml0xx)
{
	if (ps_stml0xx != NULL && ps_stml0xx->pdata != NULL) {
		mutex_lock(&ps_stml0xx->sh_wakeup_lock);

		if (ps_stml0xx->sh_wakeup_count > 0) {
			ps_stml0xx->sh_wakeup_count--;
			if (ps_stml0xx->sh_wakeup_count == 0 &&
					ps_stml0xx->sh_lowpower_enabled) {
				gpio_set_value(ps_stml0xx->pdata->gpio_sh_wake,
					       0);
			}
		} else {
			dev_err(&stml0xx_misc_data->spi->dev,
				"stml0xx_sleep called too many times: %d",
				ps_stml0xx->sh_wakeup_count);
		}

		mutex_unlock(&ps_stml0xx->sh_wakeup_lock);
	}
}

void stml0xx_detect_lowpower_mode(void)
{
	int err;
	uint8_t buf[STML0XX_POWER_REG_SIZE] = {0};

	if (stml0xx_misc_data->pdata->gpio_sh_wake >= 0) {
		mutex_lock(&stml0xx_misc_data->sh_wakeup_lock);

		/* hold sensorhub awake, it might try to sleep
		 * after we tell it the kernel supports low power */
		gpio_set_value(stml0xx_misc_data->pdata->gpio_sh_wake, 1);

		/* detect whether lowpower mode is supported */
		err = stml0xx_spi_send_read_reg_reset(LOWPOWER_REG, buf,
				STML0XX_POWER_REG_SIZE, RESET_NOT_ALLOWED);

		if (err >= 0) {
			if ((int)buf[1] == 1)
				stml0xx_misc_data->sh_lowpower_enabled = 1;
			else
				stml0xx_misc_data->sh_lowpower_enabled = 0;

			dev_dbg(&stml0xx_misc_data->spi->dev,
				"lowpower supported: %d",
				stml0xx_misc_data->sh_lowpower_enabled);

			if (stml0xx_misc_data->sh_lowpower_enabled) {
				/* send back to the hub the kernel
				 * supports low power mode */
				memset(buf, 0x01, STML0XX_POWER_REG_SIZE);
				err =
					stml0xx_spi_send_write_reg_reset(
							LOWPOWER_REG,
							buf,
							STML0XX_POWER_REG_SIZE,
							RESET_NOT_ALLOWED);
				if (err < 0) {
					/* if we failed to let the sensorhub
					 * know we support lowpower mode
					 * disable it */
					stml0xx_misc_data->sh_lowpower_enabled =
					    0;
				}
			}
		} else {
			dev_err(&stml0xx_misc_data->spi->dev,
				"error reading lowpower supported %d", err);
			/* if we failed to read the sensorhub
			 * disable lowpower mode */
			stml0xx_misc_data->sh_lowpower_enabled = 0;
		}

		mutex_unlock(&stml0xx_misc_data->sh_wakeup_lock);
	}
}

uint8_t stml0xx_set_lowpower_mode(enum lowpower_mode lp_type,
		enum reset_option reset)
{
	uint8_t lp_buf[STML0XX_POWER_REG_SIZE] = {0};
	uint8_t err = 0;

	err = stml0xx_spi_send_read_reg_reset(LOWPOWER_REG, lp_buf,
			STML0XX_POWER_REG_SIZE, reset);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"stml0xx_set_lowpower_mode: read_reg");
		err = -EFAULT;
		goto EXIT;
	}

	lp_buf[0] = lp_type;
	err = stml0xx_spi_send_write_reg_reset(LOWPOWER_REG, lp_buf,
			STML0XX_POWER_REG_SIZE, reset);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"stml0xx_set_lowpower_mode: write_reg");
		err = -EFAULT;
		goto EXIT;
	}
EXIT:
	return err;
}

static int stml0xx_hw_init(struct stml0xx_data *ps_stml0xx)
{
	int err = 0;
	dev_dbg(&ps_stml0xx->spi->dev, "stml0xx_hw_init");
	ps_stml0xx->hw_initialized = 1;
	return err;
}

static void stml0xx_device_power_off(struct stml0xx_data *ps_stml0xx)
{
	dev_dbg(&ps_stml0xx->spi->dev, "stml0xx_device_power_off");
	if (ps_stml0xx->hw_initialized == 1) {
		if (ps_stml0xx->pdata->power_off)
			ps_stml0xx->pdata->power_off();
		ps_stml0xx->hw_initialized = 0;
	}
}

static void stml0xx_device_power_on_fail_dropbox(void)
{
	char dropbox_entry[256];

	memset(dropbox_entry, 0, sizeof(dropbox_entry));
	snprintf(dropbox_entry, sizeof(dropbox_entry),
		"Sensor Hub Power on failure");
	dropbox_queue_event_text(DROPBOX_SENSORHUB_POWERON_ISSUE,
		dropbox_entry, strlen(dropbox_entry));
}

static int stml0xx_device_power_on(struct stml0xx_data *ps_stml0xx)
{
	int err = 0;
	dev_dbg(&ps_stml0xx->spi->dev, "stml0xx_device_power_on");
	if (ps_stml0xx->pdata->power_on) {
		err = ps_stml0xx->pdata->power_on();
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
				"power_on failed: %d", err);
			/* Send dropbox event to trigger bug2go report */
			stml0xx_device_power_on_fail_dropbox();
			return err;
		}
	}
	if (!ps_stml0xx->hw_initialized) {
		err = stml0xx_hw_init(ps_stml0xx);
		if (err < 0) {
			stml0xx_device_power_off(ps_stml0xx);
			/* Send dropbox event to trigger bug2go report */
			stml0xx_device_power_on_fail_dropbox();
			return err;
		}
	}
	return err;
}

static ssize_t dock_print_name(struct switch_dev *switch_dev, char *buf)
{
	switch (switch_get_state(switch_dev)) {
	case NO_DOCK:
		return snprintf(buf, 5, "None\n");
	case DESK_DOCK:
		return snprintf(buf, 5, "DESK\n");
	case CAR_DOCK:
		return snprintf(buf, 4, "CAR\n");
	}

	return -EINVAL;
}

int stml0xx_enable(void)
{
	int err = 0;

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_enable");
	if (!atomic_cmpxchg(&stml0xx_misc_data->enabled, 0, 1)) {
		err = stml0xx_device_power_on(stml0xx_misc_data);
		if (err < 0) {
			atomic_set(&stml0xx_misc_data->enabled, 0);
			dev_err(&stml0xx_misc_data->spi->dev,
				"stml0xx_enable returned with %d", err);
			return err;
		}
	}

	return err;
}

struct miscdevice stml0xx_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &stml0xx_misc_fops,
};

#ifdef CONFIG_OF
static struct stml0xx_platform_data *stml0xx_of_init(struct spi_device *spi)
{
	int len;
	int lsize, bsize, index;
	struct stml0xx_platform_data *pdata;
	struct device_node *np = spi->dev.of_node;
	unsigned int lux_table[LIGHTING_TABLE_SIZE];
	unsigned int brightness_table[LIGHTING_TABLE_SIZE];
	const char *name;

	pdata = devm_kzalloc(&spi->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"pdata allocation failure");
		return NULL;
	}

	pdata->gpio_int = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);
	pdata->gpio_bslen = of_get_gpio(np, 2);
	pdata->gpio_wakeirq = of_get_gpio(np, 3);

	if (of_gpio_count(np) >= 6) {
		pdata->gpio_spi_ready_for_receive = of_get_gpio(np, 4);
		pdata->gpio_spi_data_ack = of_get_gpio(np, 5);
	} else {
		dev_err(&stml0xx_misc_data->spi->dev,
			"spi side band signals not defined");
		return NULL;
	}

	if (of_gpio_count(np) >= 7) {
		pdata->gpio_sh_wake = of_get_gpio(np, 6);
		stml0xx_misc_data->sh_lowpower_enabled = 1;
	} else {
		pdata->gpio_sh_wake = -1;
		stml0xx_misc_data->sh_lowpower_enabled = 0;
	}

	if (of_get_property(np, "lux_table", &len) == NULL) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"lux_table len access failure");
		return NULL;
	}
	lsize = len / sizeof(u32);
	if ((lsize != 0) && (lsize < (LIGHTING_TABLE_SIZE - 1)) &&
	    (!of_property_read_u32_array(np, "lux_table",
					 (u32 *) (lux_table), lsize))) {
		for (index = 0; index < lsize; index++)
			pdata->lux_table[index] = ((u32 *) lux_table)[index];
	} else {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Lux table is missing");
		return NULL;
	}
	pdata->lux_table[lsize] = 0xFFFF;

	if (of_get_property(np, "brightness_table", &len) == NULL) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Brightness_table len access failure");
		return NULL;
	}
	bsize = len / sizeof(u32);
	if ((bsize != 0) && (bsize < (LIGHTING_TABLE_SIZE)) &&
	    !of_property_read_u32_array(np,
					"brightness_table",
					(u32 *) (brightness_table), bsize)) {

		for (index = 0; index < bsize; index++) {
			pdata->brightness_table[index]
			    = ((u32 *) brightness_table)[index];
		}
	} else {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Brightness table is missing");
		return NULL;
	}

	if ((lsize + 1) != bsize) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Lux and Brightness table sizes don't match");
		return NULL;
	}

	of_property_read_u32(np, "bslen_pin_active_value",
			     &pdata->bslen_pin_active_value);

	pdata->reset_hw_type = 0;
	of_property_read_u32(np, "reset_hw_type",
		&pdata->reset_hw_type);

	of_get_property(np, "stml0xx_fw_version", &len);
	if (!of_property_read_string(np, "stml0xx_fw_version", &name))
		strlcpy(pdata->fw_version, name, FW_VERSION_SIZE);
	else
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Not using stml0xx_fw_version override");

	pdata->ct406_detect_threshold = 0x00C8;
	pdata->ct406_undetect_threshold = 0x00A5;
	pdata->ct406_recalibrate_threshold = 0x0064;
	pdata->ct406_pulse_count = 0x02;
	pdata->ct406_prox_gain = 0x02;
	of_property_read_u32(np, "ct406_detect_threshold",
			     &pdata->ct406_detect_threshold);
	of_property_read_u32(np, "ct406_undetect_threshold",
			     &pdata->ct406_undetect_threshold);
	of_property_read_u32(np, "ct406_recalibrate_threshold",
			     &pdata->ct406_recalibrate_threshold);
	of_property_read_u32(np, "ct406_pulse_count",
			     &pdata->ct406_pulse_count);
	of_property_read_u32(np, "ct406_prox_gain",
			     &pdata->ct406_prox_gain);
	pdata->ct406_als_lux1_c0_mult = 0x294;
	pdata->ct406_als_lux1_c1_mult = 0x55A;
	pdata->ct406_als_lux1_div = 0x64;
	pdata->ct406_als_lux2_c0_mult = 0xDA;
	pdata->ct406_als_lux2_c1_mult = 0x186;
	pdata->ct406_als_lux2_div = 0x64;
	of_property_read_u32(np, "ct406_als_lux1_c0_mult",
			     &pdata->ct406_als_lux1_c0_mult);
	of_property_read_u32(np, "ct406_als_lux1_c1_mult",
			     &pdata->ct406_als_lux1_c1_mult);
	of_property_read_u32(np, "ct406_als_lux1_div",
			     &pdata->ct406_als_lux1_div);
	of_property_read_u32(np, "ct406_als_lux2_c0_mult",
			     &pdata->ct406_als_lux2_c0_mult);
	of_property_read_u32(np, "ct406_als_lux2_c1_mult",
			     &pdata->ct406_als_lux2_c1_mult);
	of_property_read_u32(np, "ct406_als_lux2_div",
			     &pdata->ct406_als_lux2_div);

	pdata->dsp_iface_enable = 0;
	of_property_read_u32(np, "dsp_iface_enable",
				&pdata->dsp_iface_enable);

	pdata->headset_detect_enable = 0;
	pdata->headset_hw_version = 0;
	pdata->headset_insertion_debounce = 0x01F4;
	pdata->headset_removal_debounce = 0x01F4;
	pdata->headset_button_down_debounce = 0x0032;
	pdata->headset_button_up_debounce = 0x0032;
	pdata->headset_button_0_1_threshold = 0x0096;
	pdata->headset_button_1_2_threshold = 0x012C;
	pdata->headset_button_2_3_threshold = 0x01C2;
	pdata->headset_button_3_upper_threshold = 0x02EE;
	pdata->headset_button_1_keycode = 0xE2;
	pdata->headset_button_2_keycode = 0x0;
	pdata->headset_button_3_keycode = 0x0;
	pdata->headset_button_4_keycode = 0x0;
	of_property_read_u32(np, "headset_detect_enable",
			     &pdata->headset_detect_enable);
	of_property_read_u32(np, "headset_hw_version",
			     &pdata->headset_hw_version);
	of_property_read_u32(np, "headset_insertion_debounce",
			     &pdata->headset_insertion_debounce);
	of_property_read_u32(np, "headset_removal_debounce",
			     &pdata->headset_removal_debounce);
	of_property_read_u32(np, "headset_button_down_debounce",
			     &pdata->headset_button_down_debounce);
	of_property_read_u32(np, "headset_button_up_debounce",
			     &pdata->headset_button_up_debounce);
	of_property_read_u32(np, "headset_button_0-1_threshold",
			     &pdata->headset_button_0_1_threshold);
	of_property_read_u32(np, "headset_button_1-2_threshold",
			     &pdata->headset_button_1_2_threshold);
	of_property_read_u32(np, "headset_button_2-3_threshold",
			     &pdata->headset_button_2_3_threshold);
	of_property_read_u32(np, "headset_button_3-upper_threshold",
			     &pdata->headset_button_3_upper_threshold);
	of_property_read_u32(np, "headset_button_1_keycode",
			     &pdata->headset_button_1_keycode);
	of_property_read_u32(np, "headset_button_2_keycode",
			     &pdata->headset_button_2_keycode);
	of_property_read_u32(np, "headset_button_3_keycode",
			     &pdata->headset_button_3_keycode);
	of_property_read_u32(np, "headset_button_4_keycode",
			     &pdata->headset_button_4_keycode);

	pdata->cover_detect_polarity = 0;
	of_property_read_u32(np, "cover_detect_polarity",
			     &pdata->cover_detect_polarity);

	pdata->accel_orientation_1 = 0;
	pdata->accel_orientation_2 = 0;
	of_property_read_u32(np, "accel_orientation_1",
			     &pdata->accel_orientation_1);
	of_property_read_u32(np, "accel_orientation_2",
			     &pdata->accel_orientation_2);

	pdata->accel_swap = 0;
	of_property_read_u32(np, "accel_swap",
			     &pdata->accel_swap);

	pdata->mag_layout = 8;
	of_property_read_u32(np, "mag_layout",
			     &pdata->mag_layout);

	return pdata;
}
#else
static inline struct stml0xx_platform_data *stml0xx_of_init(struct spi_device
							    *spi)
{
	return NULL;
}
#endif

static int stml0xx_gpio_init(struct stml0xx_platform_data *pdata,
			     struct spi_device *pdev)
{
	int err;

	err = gpio_request(pdata->gpio_int, "stml0xx int");
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"stml0xx int gpio_request failed: %d", err);
		return err;
	}
	gpio_direction_input(pdata->gpio_int);
	err = gpio_export(pdata->gpio_int, 0);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"gpio_int gpio_export failed: %d", err);
		goto free_int;
	}
	err = gpio_export_link(&pdev->dev, "gpio_irq", pdata->gpio_int);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"gpio_irq gpio_export_link failed: %d", err);
		goto free_int;
	}

	err = gpio_request(pdata->gpio_reset, "stml0xx reset");
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"stml0xx reset gpio_request failed: %d", err);
		goto free_int;
	}
	gpio_direction_output(pdata->gpio_reset, 1);
	if (pdata->reset_hw_type == 0) {
		err = gpio_export(pdata->gpio_reset, 0);
	} else {
		err = gpio_export(pdata->gpio_reset, 1);
	}
	/* Keep the part in reset until the flasher uses NORMALMODE to tell us
	 * we're good to go. */
	gpio_set_value(pdata->gpio_reset, 0);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"reset gpio_export failed: %d", err);
		goto free_reset;
	}

	err = gpio_request(pdata->gpio_bslen, "stml0xx bslen");
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"bslen gpio_request failed: %d", err);
		goto free_reset;
	}
	gpio_direction_output(pdata->gpio_bslen, 0);
	gpio_set_value(pdata->gpio_bslen, 0);
	err = gpio_export(pdata->gpio_bslen, 0);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"bslen gpio_export failed: %d", err);
		goto free_bslen;
	}

	err = gpio_request(pdata->gpio_spi_ready_for_receive, "stml0xx rfr");
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"rfr gpio_request failed: %d", err);
		goto free_bslen;
	}
	gpio_direction_input(pdata->gpio_spi_ready_for_receive);
	err = gpio_export(pdata->gpio_spi_ready_for_receive, 0);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"rfr gpio_export failed: %d", err);
		goto free_rfr;
	}
	err =
	    gpio_export_link(&pdev->dev, "gpio_spi_ready_for_receive",
			     pdata->gpio_spi_ready_for_receive);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
		  "gpio_spi_ready_for_receive gpio_export_link failed: %d",
		  err);
		goto free_rfr;
	}

	err = gpio_request(pdata->gpio_spi_data_ack, "stml0xx data_ack");
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"data_ack gpio_request failed: %d", err);
		goto free_rfr;
	}
	gpio_direction_input(pdata->gpio_spi_data_ack);
	err = gpio_export(pdata->gpio_spi_data_ack, 0);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"data_ack gpio_export failed: %d", err);
		goto free_data_ack;
	}
	err =
	    gpio_export_link(&pdev->dev, "gpio_spi_data_ack",
			     pdata->gpio_spi_data_ack);
	if (err) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"gpio_spi_data_ack gpio_export_link failed: %d", err);
		goto free_data_ack;
	}

	if (pdata->gpio_sh_wake >= 0) {
		/* pin to pull the stm chip out of lowpower mode */
		err = gpio_request(pdata->gpio_sh_wake, "stml0xx sh_wake");
		if (err) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"sh_wake gpio_request failed: %d", err);
			goto free_data_ack;
		}
		gpio_direction_output(pdata->gpio_sh_wake, 0);
		gpio_set_value(pdata->gpio_sh_wake, 1);
		err = gpio_export(pdata->gpio_sh_wake, 0);
		if (err) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"sh_wake gpio_export failed: %d", err);
			goto free_wake_sh;
		}
	} else {
		dev_err(&stml0xx_misc_data->spi->dev,
			"%s: pin for stm lowpower mode not specified",
			__func__);
	}

	if (gpio_is_valid(pdata->gpio_wakeirq)) {
		err = gpio_request(pdata->gpio_wakeirq, "stml0xx wakeirq");
		if (err) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"wakeirq gpio_request failed: %d", err);
			goto free_wake_sh;
		}
		gpio_direction_input(pdata->gpio_wakeirq);
		err = gpio_export(pdata->gpio_wakeirq, 0);
		if (err) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"wakeirq gpio_export failed: %d", err);
			goto free_wakeirq;
		}

		err = gpio_export_link(&pdev->dev, "wakeirq",
				       pdata->gpio_wakeirq);
		if (err) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"wakeirq link failed: %d", err);
			goto free_wakeirq;
		}
	} else {
		dev_err(&stml0xx_misc_data->spi->dev,
			"%s: gpio wake irq not specified", __func__);
	}

	return 0;

free_wakeirq:
	gpio_free(pdata->gpio_wakeirq);
free_wake_sh:
	if (pdata->gpio_sh_wake >= 0)
		gpio_free(pdata->gpio_sh_wake);
free_data_ack:
	gpio_free(pdata->gpio_spi_data_ack);
free_rfr:
	gpio_free(pdata->gpio_spi_ready_for_receive);
free_bslen:
	gpio_free(pdata->gpio_bslen);
free_reset:
	gpio_free(pdata->gpio_reset);
free_int:
	gpio_free(pdata->gpio_int);
	return err;
}

static void stml0xx_gpio_free(struct stml0xx_platform_data *pdata)
{
	gpio_free(pdata->gpio_int);
	gpio_free(pdata->gpio_reset);
	gpio_free(pdata->gpio_bslen);
	gpio_free(pdata->gpio_wakeirq);
	gpio_free(pdata->gpio_spi_data_ack);
	gpio_free(pdata->gpio_spi_ready_for_receive);
	if (pdata->gpio_sh_wake >= 0)
		gpio_free(pdata->gpio_sh_wake);
}

static int stml0xx_probe(struct spi_device *spi)
{
	struct stml0xx_platform_data *pdata;
	struct stml0xx_data *ps_stml0xx;
	int err = -1;

	dev_dbg(&spi->dev, "probe begun");

	ps_stml0xx = devm_kzalloc(&spi->dev, sizeof(*ps_stml0xx), GFP_KERNEL);
	if (ps_stml0xx == NULL) {
		dev_err(&spi->dev,
			"failed to allocate memory for module data: %d", err);
		return -ENOMEM;
	}
	ps_stml0xx->spi = spi;
	if (!stml0xx_misc_data)
		stml0xx_misc_data = ps_stml0xx;

	INIT_LIST_HEAD(&(ps_stml0xx->as_queue.list));
	spin_lock_init(&(ps_stml0xx->as_queue_lock));

	/* SPI setup */

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev, "%s: Half duplex not supported by host",
			__func__);
		err = -EIO;
		goto err_other;
	}

	spi->mode = (SPI_MODE_0);
	spi->bits_per_word = 8;

	err = spi_setup(spi);
	if (err < 0) {
		dev_err(&spi->dev, "%s: Failed to perform SPI setup",
			__func__);
		goto err_other;
	}

	if (spi->dev.of_node)
		pdata = stml0xx_of_init(spi);
	else
		pdata = spi->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&spi->dev, "platform data is NULL, exiting");
		err = -ENODEV;
		goto err_pdata;
	}

	/* Allocate SPI buffers */
	ps_stml0xx->spi_tx_buf =
		devm_kzalloc(&spi->dev, SPI_BUFF_SIZE, GFP_KERNEL);
	ps_stml0xx->spi_rx_buf =
		devm_kzalloc(&spi->dev, SPI_BUFF_SIZE, GFP_KERNEL);
	if (!ps_stml0xx->spi_tx_buf || !ps_stml0xx->spi_rx_buf)
		goto err_nomem;

	/* global buffers used exclusively in bootloader mode */
	stml0xx_boot_cmdbuff = ps_stml0xx->spi_tx_buf;
	stml0xx_boot_readbuff = ps_stml0xx->spi_rx_buf;

	/* initialize regulators */
	ps_stml0xx->regulator_1 = regulator_get(&spi->dev, "sensor1");
	if (IS_ERR(ps_stml0xx->regulator_1)) {
		dev_err(&spi->dev, "Failed to get VIO regulator");
		goto err_regulator;
	}

	ps_stml0xx->regulator_2 = regulator_get(&spi->dev, "sensor2");
	if (IS_ERR(ps_stml0xx->regulator_2)) {
		dev_err(&spi->dev, "Failed to get VCC regulator");
		regulator_put(ps_stml0xx->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_stml0xx->regulator_1)) {
		dev_err(&spi->dev, "Failed to enable Sensor 1 regulator");
		regulator_put(ps_stml0xx->regulator_2);
		regulator_put(ps_stml0xx->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_stml0xx->regulator_2)) {
		dev_err(&spi->dev, "Failed to enable Sensor 2 regulator");
		regulator_disable(ps_stml0xx->regulator_1);
		regulator_put(ps_stml0xx->regulator_2);
		regulator_put(ps_stml0xx->regulator_1);
		goto err_regulator;
	}

	/* not all products have 3 sensor supplies */
	ps_stml0xx->regulator_3 = regulator_get(&spi->dev, "sensor3");

	if (!IS_ERR(ps_stml0xx->regulator_3)) {
		if (regulator_enable(ps_stml0xx->regulator_3)) {
			dev_err(&spi->dev, "Failed to enable Sensor 3 regulator");
			regulator_disable(ps_stml0xx->regulator_2);
			regulator_disable(ps_stml0xx->regulator_1);
			regulator_put(ps_stml0xx->regulator_3);
			regulator_put(ps_stml0xx->regulator_2);
			regulator_put(ps_stml0xx->regulator_1);
			goto err_regulator;
		}
	}

	err = stml0xx_gpio_init(pdata, spi);
	if (err) {
		dev_err(&spi->dev, "stml0xx gpio init failed");
		goto err_gpio_init;
	}

	mutex_init(&ps_stml0xx->lock);
	mutex_init(&ps_stml0xx->sh_wakeup_lock);
	mutex_init(&ps_stml0xx->spi_lock);

	mutex_lock(&ps_stml0xx->lock);
	wake_lock_init(&ps_stml0xx->wakelock, WAKE_LOCK_SUSPEND, "stml0xx");
	wake_lock_init(&ps_stml0xx->wake_sensor_wakelock, WAKE_LOCK_SUSPEND,
		       "stml0xx_wake_sensor");
	wake_lock_init(&ps_stml0xx->reset_wakelock, WAKE_LOCK_SUSPEND,
		       "stml0xx_reset");

	/* Set to passive mode by default */
	stml0xx_g_nonwake_sensor_state = 0;
	stml0xx_g_wake_sensor_state = 0;
	/* clear the interrupt mask */
	ps_stml0xx->intp_mask = 0x00;

	INIT_WORK(&ps_stml0xx->initialize_work, stml0xx_initialize_work_func);

	ps_stml0xx->irq_work_queue =
	    create_singlethread_workqueue("stml0xx_wq");
	if (!ps_stml0xx->irq_work_queue) {
		err = -ENOMEM;
		dev_err(&spi->dev, "cannot create work queue: %d", err);
		goto err1;
	}
	ps_stml0xx->ioctl_work_queue =
		create_singlethread_workqueue("stml0xx_ioctl_wq");
	if (!ps_stml0xx->ioctl_work_queue) {
		err = -ENOMEM;
		dev_err(
			&spi->dev,
			"cannot create ioctl work queue: %d\n",
			err
		);
		goto err2;
	}
	ps_stml0xx->pdata = pdata;
	spi_set_drvdata(spi, ps_stml0xx);

	if (ps_stml0xx->pdata->init) {
		err = ps_stml0xx->pdata->init();
		if (err < 0) {
			dev_err(&spi->dev, "init failed: %d", err);
			goto err_pdata_init;
		}
	}

	/* configure interrupt */
	ps_stml0xx->irq = gpio_to_irq(ps_stml0xx->pdata->gpio_int);
	if (gpio_is_valid(ps_stml0xx->pdata->gpio_wakeirq))
		ps_stml0xx->irq_wake
		    = gpio_to_irq(ps_stml0xx->pdata->gpio_wakeirq);
	else
		ps_stml0xx->irq_wake = -1;

	err = stml0xx_device_power_on(ps_stml0xx);
	if (err < 0) {
		dev_err(&spi->dev, "power on failed: %d", err);
		goto err4;
	}

	if (ps_stml0xx->irq_wake != -1)
		enable_irq_wake(ps_stml0xx->irq_wake);
	atomic_set(&ps_stml0xx->enabled, 1);

	err = misc_register(&stml0xx_misc_device);
	if (err < 0) {
		dev_err(&spi->dev, "misc register failed: %d", err);
		goto err6;
	}

	if (alloc_chrdev_region(&ps_stml0xx->stml0xx_dev_num, 0, 2, "stml0xx")
	    < 0) {
		dev_err(&spi->dev, "alloc_chrdev_region failed");
	}
	ps_stml0xx->stml0xx_class = class_create(THIS_MODULE, "stml0xx");

	cdev_init(&ps_stml0xx->as_cdev, &stml0xx_as_fops);
	ps_stml0xx->as_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_stml0xx->as_cdev, ps_stml0xx->stml0xx_dev_num, 1);
	if (err)
		dev_err(&spi->dev, "cdev_add as failed: %d", err);

	device_create(ps_stml0xx->stml0xx_class, NULL,
		      MKDEV(MAJOR(ps_stml0xx->stml0xx_dev_num), 0),
		      ps_stml0xx, "stml0xx_as");

	cdev_init(&ps_stml0xx->ms_cdev, &stml0xx_ms_fops);
	ps_stml0xx->ms_cdev.owner = THIS_MODULE;
	err =
	    cdev_add(&ps_stml0xx->ms_cdev, ps_stml0xx->stml0xx_dev_num + 1, 1);
	if (err)
		dev_err(&spi->dev, "cdev_add ms failed: %d", err);

	device_create(ps_stml0xx->stml0xx_class, NULL,
		      MKDEV(MAJOR(ps_stml0xx->stml0xx_dev_num), 1),
		      ps_stml0xx, "stml0xx_ms");

	stml0xx_device_power_off(ps_stml0xx);

	atomic_set(&ps_stml0xx->enabled, 0);

	err = request_irq(ps_stml0xx->irq, stml0xx_isr,
			  IRQF_TRIGGER_RISING, NAME, ps_stml0xx);
	if (err < 0) {
		dev_err(&spi->dev, "request irq failed: %d", err);
		goto err7;
	}

	if (ps_stml0xx->irq_wake != -1) {
		err = request_irq(ps_stml0xx->irq_wake, stml0xx_wake_isr,
				  IRQF_TRIGGER_RISING, NAME, ps_stml0xx);
		if (err < 0) {
			dev_err(&spi->dev, "request wake irq failed: %d",
				err);
			goto err8;
		}
	}

	init_waitqueue_head(&ps_stml0xx->stml0xx_as_data_wq);
	init_waitqueue_head(&ps_stml0xx->stml0xx_ms_data_wq);

	ps_stml0xx->dsdev.name = "dock";
	ps_stml0xx->dsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_stml0xx->dsdev);
	if (err) {
		dev_err(&spi->dev, "Couldn't register switch (%s) rc=%d",
			ps_stml0xx->dsdev.name, err);
		ps_stml0xx->dsdev.dev = NULL;
	}

	ps_stml0xx->edsdev.name = "extdock";
	ps_stml0xx->edsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_stml0xx->edsdev);
	if (err) {
		dev_err(&spi->dev, "Couldn't register switch (%s) rc=%d",
			ps_stml0xx->edsdev.name, err);
		ps_stml0xx->edsdev.dev = NULL;
	}

	ps_stml0xx->input_dev = input_allocate_device();
	if (!ps_stml0xx->input_dev) {
		err = -ENOMEM;
		dev_err(&spi->dev, "input device allocate failed: %d", err);
		goto err8;
	}
	input_set_drvdata(ps_stml0xx->input_dev, ps_stml0xx);
	input_set_capability(ps_stml0xx->input_dev, EV_KEY, KEY_POWER);
	input_set_capability(ps_stml0xx->input_dev, EV_SW, SW_LID);
	if (pdata->headset_button_1_keycode > 0)
		input_set_capability(ps_stml0xx->input_dev, EV_KEY, pdata->headset_button_1_keycode);
	if (pdata->headset_button_2_keycode > 0)
		input_set_capability(ps_stml0xx->input_dev, EV_KEY, pdata->headset_button_2_keycode);
	if (pdata->headset_button_3_keycode > 0)
		input_set_capability(ps_stml0xx->input_dev, EV_KEY, pdata->headset_button_3_keycode);
	if (pdata->headset_button_4_keycode > 0)
		input_set_capability(ps_stml0xx->input_dev, EV_KEY, pdata->headset_button_4_keycode);
	input_set_capability(ps_stml0xx->input_dev, EV_SW, SW_LID);
	input_set_capability(ps_stml0xx->input_dev, EV_SW, SW_HEADPHONE_INSERT);
	input_set_capability(ps_stml0xx->input_dev, EV_SW, SW_MICROPHONE_INSERT);
	ps_stml0xx->input_dev->name = "sensorprocessor";

	err = input_register_device(ps_stml0xx->input_dev);
	if (err) {
		dev_err(&spi->dev,
			"unable to register input polled device %s: %d",
			ps_stml0xx->input_dev->name, err);
		goto err9;
	}

	ps_stml0xx->is_suspended = false;
	ps_stml0xx->irq_wake_work_pending = false;
	ps_stml0xx->discard_sensor_queue = false;

	/* We could call switch_stml0xx_mode(NORMALMODE) at this point, but
	 * instead we will hold the part in reset and only go to NORMALMODE on a
	 * request to do so from the flasher.  The flasher must be present, and
	 * it must verify the firmware file is available before switching to
	 * NORMALMODE. This is to prevent a build that is missing firmware or
	 * flasher from behaving as a normal build (with factory firmware in the
	 * part).
	 */

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
	ps_stml0xx->hall_data = mmi_hall_init();
#endif

#ifdef CONFIG_SENSORS_SH_AK09912
	stml0xx_akm_init(ps_stml0xx);
#endif

	mutex_unlock(&ps_stml0xx->lock);

	dev_dbg(&spi->dev, "probed finished");

	return 0;
err9:
	input_free_device(ps_stml0xx->input_dev);
err8:
	free_irq(ps_stml0xx->irq, ps_stml0xx);
err7:
	misc_deregister(&stml0xx_misc_device);
err6:
	stml0xx_device_power_off(ps_stml0xx);
err4:
	if (ps_stml0xx->pdata->exit)
		ps_stml0xx->pdata->exit();
err_pdata_init:
	destroy_workqueue(ps_stml0xx->ioctl_work_queue);
err2:
	destroy_workqueue(ps_stml0xx->irq_work_queue);
err1:
	mutex_unlock(&ps_stml0xx->lock);
	mutex_destroy(&ps_stml0xx->lock);
	wake_unlock(&ps_stml0xx->wakelock);
	wake_lock_destroy(&ps_stml0xx->wakelock);
	wake_unlock(&ps_stml0xx->wake_sensor_wakelock);
	wake_lock_destroy(&ps_stml0xx->wake_sensor_wakelock);
	wake_unlock(&ps_stml0xx->reset_wakelock);
	wake_lock_destroy(&ps_stml0xx->reset_wakelock);
	stml0xx_gpio_free(pdata);
err_gpio_init:
	if (!IS_ERR(ps_stml0xx->regulator_3)) {
		regulator_disable(ps_stml0xx->regulator_3);
		regulator_put(ps_stml0xx->regulator_3);
	}
	regulator_disable(ps_stml0xx->regulator_2);
	regulator_disable(ps_stml0xx->regulator_1);
	regulator_put(ps_stml0xx->regulator_2);
	regulator_put(ps_stml0xx->regulator_1);
err_regulator:
err_nomem:
err_pdata:
err_other:
	return err;
}

static int stml0xx_remove(struct spi_device *spi)
{
	struct stml0xx_data *ps_stml0xx = spi_get_drvdata(spi);

	switch_dev_unregister(&ps_stml0xx->dsdev);
	switch_dev_unregister(&ps_stml0xx->edsdev);

	if (ps_stml0xx->irq_wake != -1)
		free_irq(ps_stml0xx->irq_wake, ps_stml0xx);

	free_irq(ps_stml0xx->irq, ps_stml0xx);
	misc_deregister(&stml0xx_misc_device);
	input_unregister_device(ps_stml0xx->input_dev);
	input_free_device(ps_stml0xx->input_dev);
	stml0xx_device_power_off(ps_stml0xx);

	if (ps_stml0xx->pdata->exit)
		ps_stml0xx->pdata->exit();

	stml0xx_gpio_free(ps_stml0xx->pdata);
	destroy_workqueue(ps_stml0xx->irq_work_queue);
	destroy_workqueue(ps_stml0xx->ioctl_work_queue);
	mutex_destroy(&ps_stml0xx->lock);
	wake_unlock(&ps_stml0xx->wakelock);
	wake_lock_destroy(&ps_stml0xx->wakelock);
	wake_unlock(&ps_stml0xx->wake_sensor_wakelock);
	wake_lock_destroy(&ps_stml0xx->wake_sensor_wakelock);
	wake_unlock(&ps_stml0xx->reset_wakelock);
	wake_lock_destroy(&ps_stml0xx->reset_wakelock);
	disable_irq_wake(ps_stml0xx->irq);

	if (!IS_ERR(ps_stml0xx->regulator_3)) {
		regulator_disable(ps_stml0xx->regulator_3);
		regulator_put(ps_stml0xx->regulator_3);
	}
	regulator_disable(ps_stml0xx->regulator_2);
	regulator_disable(ps_stml0xx->regulator_1);
	regulator_put(ps_stml0xx->regulator_2);
	regulator_put(ps_stml0xx->regulator_1);

	return 0;
}

static int stml0xx_resume(struct device *dev)
{
	static struct timespec ts;
	static struct stml0xx_work_struct *stm_ws;
	static struct stml0xx_delayed_work_struct *stm_dws;
	struct stml0xx_data *ps_stml0xx = spi_get_drvdata(to_spi_device(dev));

	get_monotonic_boottime(&ts);
	dev_dbg(&stml0xx_misc_data->spi->dev, "%s", __func__);

	mutex_lock(&ps_stml0xx->lock);
	ps_stml0xx->is_suspended = false;
	ps_stml0xx->discard_sensor_queue = true;
	enable_irq(ps_stml0xx->irq);

	/* Schedule work for wake IRQ, if pending */
	if (ps_stml0xx->irq_wake_work_pending) {
		stm_dws = kmalloc(sizeof(struct stml0xx_delayed_work_struct),
					GFP_ATOMIC);
		if (!stm_dws) {
			dev_err(&ps_stml0xx->spi->dev,
				"stml0xx_resume: unable to allocate delayed work struct");
			mutex_unlock(&ps_stml0xx->lock);
			return 0;
		}
		INIT_DELAYED_WORK((struct delayed_work *)stm_dws,
				stml0xx_irq_wake_work_func);
		stm_dws->ts_ns = ps_stml0xx->pending_wake_irq_ts_ns;
		queue_delayed_work(ps_stml0xx->irq_work_queue,
			(struct delayed_work *)stm_dws,
			msecs_to_jiffies(ps_stml0xx->irq_wake_work_delay));
		ps_stml0xx->irq_wake_work_pending = false;
	}

	/* Schedule work for non-wake IRQ to discard any streaming sensor data
	   that was queued at the sensorhub during suspend. Process other
	   non-wake sensor data that may have updated. */
	stm_ws = kmalloc(
		sizeof(struct stml0xx_work_struct),
		GFP_ATOMIC);
	if (!stm_ws) {
		dev_err(&ps_stml0xx->spi->dev,
			"stml0xx_resume: unable to allocate work struct");
		mutex_unlock(&ps_stml0xx->lock);
		return 0;
	}
	INIT_WORK((struct work_struct *)stm_ws, stml0xx_irq_work_func);
	stm_ws->ts_ns = ts_to_ns(ts);
	queue_work(ps_stml0xx->irq_work_queue, (struct work_struct *)stm_ws);

	mutex_unlock(&ps_stml0xx->lock);

	return 0;
}

static int stml0xx_suspend(struct device *dev)
{
	int mutex_locked = 0;
	int mutex_timeout = 50;
	struct stml0xx_data *ps_stml0xx = spi_get_drvdata(to_spi_device(dev));
	dev_dbg(&stml0xx_misc_data->spi->dev, "%s", __func__);

	disable_irq(ps_stml0xx->irq);
	ps_stml0xx->is_suspended = true;

	/* wait for irq handlers to finish */
	do {
		mutex_locked = mutex_trylock(&ps_stml0xx->lock);
		if (!mutex_locked)
			usleep_range(500, 1000);
	} while (!mutex_locked && mutex_timeout-- >= 0);

	if (mutex_locked)
		mutex_unlock(&ps_stml0xx->lock);

	return 0;
}

static const struct dev_pm_ops stml0xx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stml0xx_suspend, stml0xx_resume)
};

static const struct spi_device_id stml0xx_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(spi, stml0xx_id);

#ifdef CONFIG_OF
static struct of_device_id stml0xx_match_tbl[] = {
	{.compatible = "stm,stml0xx"},
	{},
};

MODULE_DEVICE_TABLE(of, stml0xx_match_tbl);
#endif

static struct spi_driver stml0xx_driver = {
	.driver = {
		   .name = NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stml0xx_match_tbl),
#ifdef CONFIG_PM
		   .pm = &stml0xx_pm_ops,
#endif
		   },
	.probe = stml0xx_probe,
	.remove = stml0xx_remove,
	.id_table = stml0xx_id,
};

static int __init stml0xx_init(void)
{
	return spi_register_driver(&stml0xx_driver);
}

static void __exit stml0xx_exit(void)
{
	spi_unregister_driver(&stml0xx_driver);
	return;
}

module_init(stml0xx_init);
module_exit(stml0xx_exit);

MODULE_DESCRIPTION("STML0XX sensor processor");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
