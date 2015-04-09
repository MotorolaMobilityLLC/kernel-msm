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
#include <linux/gfp.h>
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

#include <linux/motosh.h>

#define SPURIOUS_INT_DELAY 600 /* ms */

irqreturn_t motosh_wake_isr(int irq, void *dev)
{
	struct motosh_data *ps_motosh = dev;

	if (motosh_irq_disable)
		return IRQ_HANDLED;

	wake_lock_timeout(&ps_motosh->wakelock, HZ);

	queue_work(ps_motosh->irq_work_queue, &ps_motosh->irq_wake_work);
	return IRQ_HANDLED;
}

void motosh_irq_wake_work_func(struct work_struct *work)
{
	int err;
	unsigned long irq_status;
	static int spurious_det;
	u8 queue_length = 0;
	u8 queue_index = 0;
	u8 state = 0;
	bool valid_queue_len;
	bool pending_log_msg = false;
	bool pending_reset = false;
	u8 pending_reset_reason;

	struct motosh_data *ps_motosh = container_of(work,
			struct motosh_data, irq_wake_work);

	dev_dbg(&ps_motosh->client->dev, "motosh_irq_wake_work_func\n");
	mutex_lock(&ps_motosh->lock);

	if (ps_motosh->mode == BOOTMODE)
		goto EXIT_NO_WAKE;

	/* This is to handle the case of receiving an interrupt after
	   suspend_late, but before interrupts were globally disabled. If this
	   is the case, interrupts might be disabled now, so we cannot handle
	   this at this time. suspend_noirq will return BUSY if this happens
	   so that we can handle these interrupts. */
	if (ps_motosh->ignore_wakeable_interrupts) {
		dev_dbg(&ps_motosh->client->dev,
			"Deferring interrupt work\n");
		ps_motosh->ignored_interrupts++;
		goto EXIT_NO_WAKE;
	}

	motosh_wake(ps_motosh);

	/* prioritize AoD
	   [these events operates independent of the wake event queueing] */
	motosh_cmdbuff[0] = WAKESENSOR_STATUS;
	if (motosh_misc_data->in_reset_and_init) {
		/* only apply delay if issue observed before */
		if (spurious_det)
			msleep(SPURIOUS_INT_DELAY);

		err = motosh_i2c_write_read_no_reset(ps_motosh,
						     motosh_cmdbuff, 1, 3);
		if (err < 0) {
			spurious_det = 1;
			dev_err(&ps_motosh->client->dev,
				"Spurious interrupt? Retry... [err: %d]\n",
				err);
			motosh_reset_and_init(START_RESET);
			goto EXIT;
		}
	} else {
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 3);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading wake sensor status failed [err: %d]\n",
				err);
			goto EXIT;
		}
	}

	irq_status = (motosh_readbuff[IRQ_WAKE_HI] << 16)
			| (motosh_readbuff[IRQ_WAKE_MED] << 8)
			| motosh_readbuff[IRQ_WAKE_LO];

	if (ps_motosh->qw_irq_status) {
		irq_status |= ps_motosh->qw_irq_status;
		ps_motosh->qw_irq_status = 0;
	}

	/* read wake queue length */
	motosh_cmdbuff[0] = WAKE_MSG_QUEUE_LEN;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading wake queue length failed [err: %d]\n", err);
		goto EXIT;
	}
	queue_length = motosh_readbuff[0];

	dev_dbg(&ps_motosh->client->dev,
			"wake queue_length: %d\n", queue_length);

	valid_queue_len = ((queue_length > 0) &&
			   (queue_length <= MOTOSH_MAX_EVENT_QUEUE_SIZE));

	/* Check if we are coming out of normal reset and/or
	   the part has self-reset */
	if (irq_status & M_INIT_COMPLETE) {
		dev_err(&ps_motosh->client->dev,
			"sensorhub reports reset [%d]", spurious_det);
		motosh_reset_and_init(COMPLETE_INIT);
	}
	if (irq_status & M_TOUCH) {
		if (motosh_display_handle_touch_locked(ps_motosh) < 0)
			goto EXIT;
	}
	if (irq_status & M_QUICKPEEK) {
		if (motosh_display_handle_quickpeek_locked(ps_motosh,
			irq_status == M_QUICKPEEK && !valid_queue_len) < 0)
			goto EXIT;
	}

	if (!valid_queue_len) {
		dev_dbg(&ps_motosh->client->dev,
			"Invalid wake queue_length: %d\n", queue_length);
		goto EXIT;
	}

	/* read wake queue */
	motosh_cmdbuff[0] = WAKE_MSG_QUEUE;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, queue_length);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading wake queue failed [len: %d] [err: %d]\n",
			queue_length,
			err);
		goto EXIT;
	}

	/* process each event from the queue */
	/* NOTE: the motosh_readbuff should not be modified while the event
	   queue is being processed */
	while (queue_index < queue_length) {
		unsigned char *data;
		unsigned char message_id = motosh_readbuff[queue_index];

		queue_index += MOTOSH_EVENT_QUEUE_MSG_ID_LEN;
		data = &motosh_readbuff[queue_index];

		dev_dbg(&ps_motosh->client->dev,
			"wake parsing event: 0x%02X\n",
			message_id);
		switch (message_id) {
		/* some events (for example, those that hold no data) are
		   signalled using a WAKESENSOR_STATUS */
		case WAKESENSOR_STATUS:
			irq_status = (data[IRQ_WAKE_HI] << 16)
					| (data[IRQ_WAKE_MED] << 8)
					| data[IRQ_WAKE_LO];
			queue_index += 3;
			data += 3;
			if (irq_status & M_FLATUP) {
				motosh_as_data_buffer_write(ps_motosh,
					DT_FLAT_UP, data, 1, 0, false);

				dev_dbg(&ps_motosh->client->dev, "Sending Flat up %d\n",
					data[0]);
				queue_index += 1;
			} else if (irq_status & M_FLATDOWN) {
				motosh_as_data_buffer_write(ps_motosh,
					DT_FLAT_DOWN, data, 1, 0, false);

				dev_dbg(&ps_motosh->client->dev, "Sending Flat down %d\n",
					data[0]);
				queue_index += 1;
			} else if (irq_status & M_LOG_MSG) {
				pending_log_msg = true;
			};
			break;
		case RESET_REQUEST:
			pending_reset = true;
			pending_reset_reason = data[0];
			queue_index += 1;
			break;
		case DOCK_DATA:
			dev_err(&ps_motosh->client->dev,
				"Invalid DOCK_DATA event [0x%02X]\n",
				data[0]);

			motosh_as_data_buffer_write(ps_motosh, DT_DOCK,
				data, 1, 0, false);
			state = data[DOCK_STATE];
			if (ps_motosh->dsdev.dev != NULL)
				switch_set_state(&ps_motosh->dsdev, state);
			if (ps_motosh->edsdev.dev != NULL)
				switch_set_state(&ps_motosh->edsdev, state);

			dev_dbg(&ps_motosh->client->dev, "Dock status:%d\n",
				state);
			queue_index += 1;
			break;
		case PROXIMITY:
			motosh_as_data_buffer_write(ps_motosh, DT_PROX,
				data, 1, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Proximity distance %d\n",
				data[PROX_DISTANCE]);
			queue_index += 1;
			break;
		case COVER_DATA:
			if (data[COVER_STATE] == MOTOSH_HALL_NORTH)
				state = 1;
			else
				state = 0;

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
			/* notify subscribers of cover state change */
			mmi_hall_notify(MMI_HALL_FOLIO, state);
#endif

			input_report_switch(ps_motosh->input_dev, SW_LID,
					    state);
			input_sync(ps_motosh->input_dev);

			dev_err(&ps_motosh->client->dev, "Cover status: %d\n",
				state);
			queue_index += 1;
			break;
		case STOWED:
			motosh_as_data_buffer_write(ps_motosh, DT_STOWED,
				data, 1, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Stowed status %d\n",  data[STOWED]);
			queue_index += 1;
			break;
		case CAMERA:
			motosh_as_data_buffer_write(ps_motosh, DT_CAMERA_ACT,
				data, 2, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Camera: %d\n", STM16_TO_HOST(data,
				CAMERA_VALUE));

			input_report_key(ps_motosh->input_dev, KEY_CAMERA, 1);
			input_report_key(ps_motosh->input_dev, KEY_CAMERA, 0);
			input_sync(ps_motosh->input_dev);
			dev_dbg(&ps_motosh->client->dev,
				"Report camkey toggle\n");
			queue_index += 2;
			break;
		case NFC:
			motosh_as_data_buffer_write(ps_motosh, DT_NFC,
					data, 1, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending NFC value: %d\n", data[0]);
			queue_index += 1;
			break;
		case SIM:
			motosh_as_data_buffer_write(ps_motosh, DT_SIM,
					data, 2, 0, false);

			/* This is one shot sensor */
			motosh_g_wake_sensor_state &= (~M_SIM);

			dev_dbg(&ps_motosh->client->dev, "Sending SIM Value=%d\n",
						STM16_TO_HOST(data, SIM_DATA));
			queue_index += 2;
			break;
		case CHOPCHOP:
			motosh_as_data_buffer_write(ps_motosh, DT_CHOPCHOP,
							data, 2, 0, false);

			dev_dbg(&ps_motosh->client->dev, "ChopChop triggered. Gyro aborts=%d\n",
					STM16_TO_HOST(data, CHOPCHOP_DATA));
			queue_index += 2;
			break;
		case LIFT:
			motosh_as_data_buffer_write(ps_motosh, DT_LIFT,
							data, 12, 0, false);

			dev_dbg(&ps_motosh->client->dev, "Lift triggered. Dist=%d. ZRot=%d. GravDiff=%d.\n",
					STM32_TO_HOST(data, LIFT_DISTANCE),
					STM32_TO_HOST(data, LIFT_ROTATION),
					STM32_TO_HOST(data, LIFT_GRAV_DIFF));
			queue_index += 12;
			break;
		case MOTION_DATA:
			if (data[0] & M_MMOVEME) {
				motosh_ms_data_buffer_write(ps_motosh,
						DT_MMMOVE, data, 1, false);
				dev_dbg(&ps_motosh->client->dev,
					"Sending meaningful movement event\n");
			} else {
				motosh_ms_data_buffer_write(ps_motosh,
						DT_NOMOVE, data, 1, false);
				dev_dbg(&ps_motosh->client->dev,
					"Sending no meaningful movement event\n");
			}
			queue_index += 1;
			break;
		case ALGO_EVT_MODALITY:
		{
			u8 algo_transition[8];
			memcpy(algo_transition, data, 7);
			algo_transition[ALGO_TYPE] = MOTOSH_IDX_MODALITY;
			motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
				algo_transition, 8, false);
			dev_dbg(&ps_motosh->client->dev, "Sending modality event\n");
			queue_index += 7;
		}
			break;
		case ALGO_EVT_ORIENTATION:
		{
			u8 algo_transition[8];
			memcpy(algo_transition, data, 7);
			algo_transition[ALGO_TYPE] = MOTOSH_IDX_ORIENTATION;
			motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
				algo_transition, 8, false);
			dev_dbg(&ps_motosh->client->dev, "Sending orientation event\n");
			queue_index += 7;
		}
			break;
		case ALGO_EVT_STOWED:
		{
			u8 algo_transition[8];
			memcpy(algo_transition, data, 7);
			algo_transition[ALGO_TYPE] = MOTOSH_IDX_STOWED;
			motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
				algo_transition, 8, false);
			dev_dbg(&ps_motosh->client->dev, "Sending stowed event\n");
			queue_index += 7;
		}
			break;
		case ALGO_EVT_ACCUM_MODALITY:
		{
			u8 algo_transition[8];
			memcpy(algo_transition, data, 7);
			algo_transition[ALGO_TYPE] = MOTOSH_IDX_ACCUM_MODALITY;
			motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
				algo_transition, 8, false);
			dev_dbg(&ps_motosh->client->dev, "Sending accum modality event\n");
			queue_index += 7;
		}
			break;
		case ALGO_EVT_ACCUM_MVMT:
		{
			u8 algo_transition[8];
			memcpy(algo_transition, data, 7);
			algo_transition[ALGO_TYPE] = MOTOSH_IDX_ACCUM_MVMT;
			motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
				algo_transition, 8, false);
			dev_dbg(&ps_motosh->client->dev, "Sending accum mvmt event\n");
			queue_index += 7;
		}
			break;
		case IR_GESTURE:
			queue_index += motosh_process_ir_gesture(ps_motosh,
								 data);
			break;
		case GENERIC_INT_STATUS:
			dev_err(&ps_motosh->client->dev,
				"Invalid GENERIC_INT_STATUS event [0x%02X]\n",
				data[0]);

			motosh_ms_data_buffer_write(ps_motosh, DT_GENERIC_INT,
				data, 1, false);
			dev_dbg(&ps_motosh->client->dev,
				"Sending generic interrupt event:%d\n",
				data[0]);
			queue_index += 1;
			break;
		default:
			/* ERROR...unknown message
			   Need to drop the remaining data in this operation. */
			dev_err(&ps_motosh->client->dev, "ERROR: unknown wake msg: 0x%02X\n",
				message_id);
			/* a write to the work queue length causes
			   it to be reset */
			motosh_cmdbuff[0] = WAKE_MSG_QUEUE_LEN;
			motosh_cmdbuff[1] = 0x00;
			motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
			goto EXIT;
		};
	}

	/* log messages are stored in a separate register, must read
	   after queue is processed */
	if (pending_log_msg) {
		motosh_cmdbuff[0] = ERROR_STATUS;
		err = motosh_i2c_write_read(ps_motosh,
					    motosh_cmdbuff,
					    1, ESR_SIZE);

		if (err >= 0) {
			memcpy(stat_string, motosh_readbuff,
			       ESR_SIZE);
			stat_string[ESR_SIZE] = 0;
			dev_err(&ps_motosh->client->dev,
				"sensorhub: %s\n",
				stat_string);
		} else
			dev_err(&ps_motosh->client->dev,
				"Failed to read error message [err: %d]\n",
				err);
	}

	/* process a reset request after dumping any last logs */
	if (pending_reset) {
		motosh_as_data_buffer_write(ps_motosh, DT_RESET,
					    &pending_reset_reason,
					    1, 0, false);

		motosh_reset_and_init(START_RESET);
		dev_err(&ps_motosh->client->dev, "sensorhub requested a reset\n");
		goto EXIT;
	}

EXIT:
	motosh_sleep(ps_motosh);
EXIT_NO_WAKE:
	mutex_unlock(&ps_motosh->lock);
}
