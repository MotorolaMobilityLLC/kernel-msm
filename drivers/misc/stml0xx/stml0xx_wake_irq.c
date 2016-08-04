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

enum headset_state_t {
	SH_HEADSET_REMOVED,
	SH_HEADPHONE_INSERTED,
	SH_HEADSET_INSERTED,
	SH_HEADSET_BUTTON_1,
	SH_HEADSET_BUTTON_2,
	SH_HEADSET_BUTTON_3,
	SH_HEADSET_BUTTON_4
};

enum cover_detect_states {
	STML0XX_HALL_NO_DETECT,
	STML0XX_HALL_SOUTH_DETECT,
	STML0XX_HALL_NORTH_DETECT,
	STML0XX_HALL_NORTH_OR_SOUTH_DETECT
};

enum headset_state_t Headset_State = SH_HEADSET_REMOVED;

irqreturn_t stml0xx_wake_isr(int irq, void *dev)
{
	struct timespec ts;
	struct stml0xx_delayed_work_struct *stm_ws;
	struct stml0xx_data *ps_stml0xx = dev;

	if (stml0xx_irq_disable)
		return IRQ_HANDLED;

	get_monotonic_boottime(&ts);

	wake_lock_timeout(&ps_stml0xx->wake_sensor_wakelock, HZ);
	stm_ws = kmalloc(
		sizeof(struct stml0xx_delayed_work_struct),
		GFP_ATOMIC);
	if (!stm_ws) {
		dev_err(&ps_stml0xx->spi->dev,
			"stml0xx_wake_isr: unable to allocate work struct");
		return IRQ_HANDLED;
	}

	INIT_DELAYED_WORK((struct delayed_work *)stm_ws,
				stml0xx_irq_wake_work_func);
	stm_ws->ts_ns = ts_to_ns(ts);

	if (stml0xx_g_booted) {
		/* No delay if boot completed */
		ps_stml0xx->irq_wake_work_delay = 0;
	} else if (ps_stml0xx->irq_wake_work_delay == 0) {
		/* First interrupt prior to boot complete */
		ps_stml0xx->irq_wake_work_delay = 1;
	} else {
		/* Increase delay, set within allowed min/max range */
		ps_stml0xx->irq_wake_work_delay =
			clamp(ps_stml0xx->irq_wake_work_delay * 2,
					100U, 600000U);
	}

	queue_delayed_work(ps_stml0xx->irq_work_queue,
			(struct delayed_work *)stm_ws,
			msecs_to_jiffies(ps_stml0xx->irq_wake_work_delay));

	return IRQ_HANDLED;
}

void stml0xx_irq_wake_work_func(struct work_struct *work)
{
	int err;
	unsigned long irq_status;
	u32 irq2_status;
	struct stml0xx_delayed_work_struct *stm_ws =
			(struct stml0xx_delayed_work_struct *)work;
	struct stml0xx_data *ps_stml0xx = stml0xx_misc_data;
	unsigned char buf[SPI_MSG_SIZE];
#ifdef CONFIG_SENSORS_SH_AK09912
	static bool akm_init;
#endif

	struct stml0xx_platform_data *pdata;
	pdata = ps_stml0xx->pdata;

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_irq_wake_work_func");

	if (ps_stml0xx->is_suspended) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Deferring irq wake work until resume");
		ps_stml0xx->irq_wake_work_pending = true;
		ps_stml0xx->pending_wake_irq_ts_ns = stm_ws->ts_ns;
		return;
	}

	mutex_lock(&ps_stml0xx->lock);

	if (ps_stml0xx->mode == BOOTMODE)
		goto EXIT_NO_WAKE;

	stml0xx_wake(ps_stml0xx);

	err = stml0xx_spi_read_msg_data(SPI_MSG_TYPE_READ_WAKE_IRQ_DATA,
					buf,
					SPI_RX_PAYLOAD_LEN,
					RESET_ALLOWED);

	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Reading from stml0xx failed");
		goto EXIT;
	}

	/* read interrupt mask register */
	irq_status = (buf[WAKE_IRQ_IDX_STATUS_HI] << 16) |
	    (buf[WAKE_IRQ_IDX_STATUS_MED] << 8) |
	    buf[WAKE_IRQ_IDX_STATUS_LO];

	/* read algorithm interrupt status register */
	irq2_status = (buf[WAKE_IRQ_IDX_ALGO_STATUS_HI] << 16) |
	    (buf[WAKE_IRQ_IDX_ALGO_STATUS_MED] << 8) |
		buf[WAKE_IRQ_IDX_ALGO_STATUS_LO];

	/* If SH not fully booted, filter on relevant msgs only */
	if (!stml0xx_g_booted) {
		irq_status &= (M_LOG_MSG | M_INIT_COMPLETE | M_HUB_RESET);
		irq2_status = 0;
	}

	/* Check all other status bits */
	if (irq_status & M_PROXIMITY) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_PROX,
			&buf[WAKE_IRQ_IDX_PROX],
			1,
			0,
			stm_ws->ts_ns);

		dev_info(&stml0xx_misc_data->spi->dev,
			"Sending Proximity distance %d als %d stowed %d raw_prox %d noise_floor %d recal_thresh %d low_thresh %d high_thresh %d\n",
			buf[WAKE_IRQ_IDX_PROX],
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_ALS),
			buf[WAKE_IRQ_IDX_STOWED],
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_RAW),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_NOISE),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_RECAL),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_LTHRESH),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_HTHRESH));
	}
	if (irq_status & M_COVER) {
		int state = 0;
		if ((pdata->cover_detect_polarity
			& buf[WAKE_IRQ_IDX_COVER]) != 0)
				state = 1;
#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
		/* notify subscribers of cover state change */
		mmi_hall_notify(MMI_HALL_FOLIO, state);
#endif
		input_report_switch(ps_stml0xx->input_dev, SW_LID, state);
		input_sync(ps_stml0xx->input_dev);

		dev_info(&stml0xx_misc_data->spi->dev,
			"Cover status: %d", state);
	}
	if (irq_status & M_HEADSET) {
		uint8_t new_state;

		new_state = buf[WAKE_IRQ_IDX_HEADSET];

		switch (Headset_State) {
		case SH_HEADSET_BUTTON_1:
			if (!(new_state & SH_HEADSET_BUTTON_1_DOWN)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 1 released");
				Headset_State = SH_HEADSET_INSERTED;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_1_keycode,
					0);
				input_sync(ps_stml0xx->input_dev);
			}
			break;
		case SH_HEADSET_BUTTON_2:
			if (!(new_state & SH_HEADSET_BUTTON_2_DOWN)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 2 released");
				Headset_State = SH_HEADSET_INSERTED;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_2_keycode,
					0);
				input_sync(ps_stml0xx->input_dev);
			}
			break;
		case SH_HEADSET_BUTTON_3:
			if (!(new_state & SH_HEADSET_BUTTON_3_DOWN)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 3 released");
				Headset_State = SH_HEADSET_INSERTED;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_3_keycode,
					0);
				input_sync(ps_stml0xx->input_dev);
			}
			break;
		case SH_HEADSET_BUTTON_4:
			if (!(new_state & SH_HEADSET_BUTTON_4_DOWN)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 4 released");
				Headset_State = SH_HEADSET_INSERTED;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_4_keycode,
					0);
				input_sync(ps_stml0xx->input_dev);
			}
			break;
		default:
			break;
		}
		if (Headset_State == SH_HEADPHONE_INSERTED) {
			if (!(new_state & SH_HEADPHONE_DETECTED)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headphone removed");
				Headset_State = SH_HEADSET_REMOVED;
				input_report_switch(ps_stml0xx->input_dev,
						SW_HEADPHONE_INSERT, 0);
				input_sync(ps_stml0xx->input_dev);
			}
		} else if (Headset_State ==  SH_HEADSET_INSERTED) {
			if (!(new_state & SH_HEADSET_DETECTED)) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset removed");
				Headset_State = SH_HEADSET_REMOVED;
				input_report_switch(ps_stml0xx->input_dev,
						SW_HEADPHONE_INSERT,  0);
				input_report_switch(ps_stml0xx->input_dev,
						SW_MICROPHONE_INSERT, 0);
				input_sync(ps_stml0xx->input_dev);
			}
		}
		if (Headset_State == SH_HEADSET_REMOVED) {
			if (new_state & SH_HEADPHONE_DETECTED) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headphone inserted");
				Headset_State = SH_HEADPHONE_INSERTED;
				input_report_switch(ps_stml0xx->input_dev,
						SW_HEADPHONE_INSERT, 1);
				input_sync(ps_stml0xx->input_dev);
			} else if (new_state & SH_HEADSET_DETECTED) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset inserted");
				Headset_State = SH_HEADSET_INSERTED;
				input_report_switch(ps_stml0xx->input_dev,
						SW_HEADPHONE_INSERT,  1);
				input_report_switch(ps_stml0xx->input_dev,
						SW_MICROPHONE_INSERT, 1);
				input_sync(ps_stml0xx->input_dev);
			}
		}
		if (Headset_State == SH_HEADSET_INSERTED) {
			if (new_state & SH_HEADSET_BUTTON_1_DOWN) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 1 pressed");
				Headset_State = SH_HEADSET_BUTTON_1;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_1_keycode,
					1);
				input_sync(ps_stml0xx->input_dev);
			} else if (new_state & SH_HEADSET_BUTTON_2_DOWN) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 2 pressed");
				Headset_State = SH_HEADSET_BUTTON_2;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_2_keycode,
					1);
				input_sync(ps_stml0xx->input_dev);
			} else if (new_state & SH_HEADSET_BUTTON_3_DOWN) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 3 pressed");
				Headset_State = SH_HEADSET_BUTTON_3;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_3_keycode,
					1);
				input_sync(ps_stml0xx->input_dev);
			} else if (new_state & SH_HEADSET_BUTTON_4_DOWN) {
				dev_info(&stml0xx_misc_data->spi->dev,
					"Headset button 4 pressed");
				Headset_State = SH_HEADSET_BUTTON_4;
				input_report_key(ps_stml0xx->input_dev,
					stml0xx_misc_data->pdata->headset_button_4_keycode,
					1);
				input_sync(ps_stml0xx->input_dev);
			}
		}
	}
	if (irq_status & M_FLATUP) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_FLAT_UP,
			&buf[WAKE_IRQ_IDX_FLAT],
			1,
			0,
			stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Flat up %d", buf[WAKE_IRQ_IDX_FLAT]);
	}
	if (irq_status & M_FLATDOWN) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_FLAT_DOWN,
						&buf[WAKE_IRQ_IDX_FLAT],
						1, 0, stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Flat down %d", buf[WAKE_IRQ_IDX_FLAT]);
	}
	if (irq_status & M_STOWED) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_STOWED,
			&buf[WAKE_IRQ_IDX_STOWED],
			1,
			0,
			stm_ws->ts_ns);

		dev_info(&stml0xx_misc_data->spi->dev,
			"Sending Stowed status %d als %d prox %d raw_prox %d noise_floor %d recal_thresh %d low_thresh %d high_thresh %d\n",
			buf[WAKE_IRQ_IDX_STOWED],
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_ALS),
			buf[WAKE_IRQ_IDX_PROX],
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_RAW),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_NOISE),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_RECAL),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_LTHRESH),
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_HTHRESH));
	}
	if (irq_status & M_CAMERA_GESTURE) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_CAMERA_ACT,
						&buf[WAKE_IRQ_IDX_CAMERA],
						2, 0, stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Camera: %d",
			SH_TO_H16(buf + WAKE_IRQ_IDX_CAMERA));

		dev_dbg(&stml0xx_misc_data->spi->dev, "Report camkey toggle");
	}
	if (irq_status & M_CHOPCHOP) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_CHOPCHOP,
						NULL,
						0, 0, stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev, "Sending Chopchop");
	}
	if (irq_status & M_SIM) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_SIM,
			&buf[WAKE_IRQ_IDX_SIM],
			2,
			0,
			stm_ws->ts_ns);

		/* This is one shot sensor */
		stml0xx_g_wake_sensor_state &= (~M_SIM);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending SIM Value=%d",
			SH_TO_H16(buf + WAKE_IRQ_IDX_SIM));
	}
	if (irq_status & M_LIFT) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_LIFT,
			&buf[WAKE_IRQ_IDX_LIFT],
			12,
			0,
			stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Lift triggered. Dist=%d. ZRot=%d. GravDiff=%d.\n",
			SH_TO_H32(buf + WAKE_IRQ_IDX_LIFT + LIFT_DISTANCE),
			SH_TO_H32(buf + WAKE_IRQ_IDX_LIFT + LIFT_ROTATION),
			SH_TO_H32(buf + WAKE_IRQ_IDX_LIFT + LIFT_GRAV_DIFF));
	}
	if (irq_status & M_UPDATE_GYRO_CAL) {
		dev_dbg(&stml0xx_misc_data->spi->dev, "Save Gyro Cal Table");
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_GYRO_CAL,
				NULL, 0, 0, stm_ws->ts_ns);
	}
	if (irq_status & M_GLANCE) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_GLANCE,
			&buf[WAKE_IRQ_IDX_GLANCE],
			2,
			0,
			stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev, "Glance Gesture=%d\n",
				SH_TO_H16(buf + WAKE_IRQ_IDX_GLANCE));
	}
	if (irq_status & M_MOTION_DETECT) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_MOTION_DETECT,
				NULL, 0, 0, stm_ws->ts_ns);
		dev_dbg(&stml0xx_misc_data->spi->dev, "Motion Detect");
	}
	if (irq_status & M_STATIONARY_DETECT) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_STATIONARY_DETECT,
				NULL, 0, 0, stm_ws->ts_ns);
		dev_dbg(&stml0xx_misc_data->spi->dev, "Stationary Detect");
	}
	if (irq2_status & M_MMOVEME) {
		unsigned char status;

		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STML0XX_CLIENT_MASK) | M_MMOVEME;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_MMMOVE, &status, 1);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending meaningful movement event");
	}
	if (irq2_status & M_NOMMOVE) {
		unsigned char status;

		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STML0XX_CLIENT_MASK) | M_NOMMOVE;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_NOMOVE, &status, 1);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending no meaningful movement event");
	}
	if (irq2_status & M_ALGO_MODALITY) {
		buf[WAKE_IRQ_IDX_MODALITY + ALGO_TYPE] = STML0XX_IDX_MODALITY;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT,
						&buf[WAKE_IRQ_IDX_MODALITY], 8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending algo modality event");
	}
	if (irq2_status & M_ALGO_ORIENTATION) {
		buf[WAKE_IRQ_IDX_MODALITY_ORIENT + ALGO_TYPE] =
				STML0XX_IDX_ORIENTATION;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT,
				&buf[WAKE_IRQ_IDX_MODALITY_ORIENT],
				8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending algo orientation event");
	}
	if (irq2_status & M_ALGO_STOWED) {
		buf[WAKE_IRQ_IDX_MODALITY_STOWED + ALGO_TYPE] =
				STML0XX_IDX_STOWED;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT,
				&buf[WAKE_IRQ_IDX_MODALITY_STOWED],
				8);
		dev_info(&stml0xx_misc_data->spi->dev,
			"Sending algo stowed event %d, als %d, prox %d",
			buf[WAKE_IRQ_IDX_MODALITY_STOWED + 3],
			SH_TO_UH16(buf + WAKE_IRQ_IDX_PROX_ALS),
			buf[WAKE_IRQ_IDX_PROX]);
	}
	if (irq2_status & M_ALGO_ACCUM_MODALITY) {
		buf[WAKE_IRQ_IDX_MODALITY_ACCUM + ALGO_TYPE] =
				STML0XX_IDX_ACCUM_MODALITY;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT,
				&buf[WAKE_IRQ_IDX_MODALITY_ACCUM],
				8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending accum modality event");
	}
	if (irq2_status & M_ALGO_ACCUM_MVMT) {
		buf[WAKE_IRQ_IDX_MODALITY_ACCUM_MVMT + ALGO_TYPE] =
				STML0XX_IDX_ACCUM_MVMT;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT,
				&buf[WAKE_IRQ_IDX_MODALITY_ACCUM_MVMT],
				8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending accum mvmt event");
	}
	/* check for log messages */
	if (irq_status & M_LOG_MSG) {
		memcpy(stat_string, &buf[WAKE_IRQ_IDX_LOG_MSG], LOG_MSG_SIZE);
		stat_string[LOG_MSG_SIZE] = 0;
		dev_info(&stml0xx_misc_data->spi->dev,
			"sensorhub : %s", stat_string);
	}
	if (irq_status & M_INIT_COMPLETE) {
		/* set the init complete register, */
		/* to let the hub know it was received */
		buf[0] = 0x01;
		err = stml0xx_spi_send_write_reg(INIT_COMPLETE, buf, 1);

		queue_work(ps_stml0xx->irq_work_queue,
			&ps_stml0xx->initialize_work);
		dev_info(&stml0xx_misc_data->spi->dev,
			"Sensor Hub reports reset");
		stml0xx_g_booted = 1;
#ifdef CONFIG_SENSORS_SH_AK09912
		if (akm_init == false) {
			err = akm09912_i2c_check_device(ps_stml0xx);
			if (err < 0) {
				dev_err(&stml0xx_misc_data->spi->dev,
					"akm09912 device check failed");
			} else
				akm_init = true;
		}
#endif
	}
	/* check for a reset request */
	if (irq_status & M_HUB_RESET) {
		err = stml0xx_spi_send_read_reg(RESET_REASON, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Unable to read reset reason %d",
				err);
			/* Unknown error reason since the read failed. */
			buf[0] = RESET_REASON_UNKNOWN;
		}

		stml0xx_as_data_buffer_write(ps_stml0xx, DT_RESET, buf, 1,
					     0, stm_ws->ts_ns);

		dev_err(&stml0xx_misc_data->spi->dev,
			"Sensor Hub requested a reset %d", buf[0]);

		stml0xx_reset(stml0xx_misc_data->pdata);
	}

EXIT:
	stml0xx_sleep(ps_stml0xx);
EXIT_NO_WAKE:
	mutex_unlock(&ps_stml0xx->lock);
	kfree(stm_ws);
}
