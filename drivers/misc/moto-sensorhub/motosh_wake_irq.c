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
#include <linux/string.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

#define SPURIOUS_INT_DELAY 800 /* ms */
#define MAX_NUM_LOGS_PER_INT  25

static int process_log_message(
	struct motosh_data *ps_motosh,
	char *logmsg,
	u8 log_msg_len)
{
	char *token = NULL;

	if (ps_motosh == NULL || logmsg == NULL || log_msg_len == 0
			|| log_msg_len > MAX_LOG_MSG_LEN)
		return -EINVAL;

	/* make sure the given string ends in null */
	logmsg[log_msg_len] = '\0';

	/* loop over the message buffer and print separate
	 * dev_info logs for each \n found from the hub */
	do {
		token = strsep(&logmsg, "\n\0");
		if (token != NULL && strlen(token) > 0) {
			dev_info(&ps_motosh->client->dev,
				"sensorhub %s\n", token);
		}
	} while (token != NULL);

	return 0;
}

irqreturn_t motosh_wake_isr(int irq, void *dev)
{
	struct motosh_data *ps_motosh = dev;

	if (motosh_irq_disable)
		return IRQ_HANDLED;

	wake_lock_timeout(&ps_motosh->wakelock, HZ);

	if (ps_motosh->mode == NORMALMODE) {
		/* No delay if boot completed */
		ps_motosh->wake_work_delay = 0;
	} else if (ps_motosh->wake_work_delay == 0) {
		/* First interrupt prior to boot complete */
		ps_motosh->wake_work_delay = 1;
	} else {
		/* Increase delay, set within allowed min/max range */
		ps_motosh->wake_work_delay =
			clamp(ps_motosh->wake_work_delay * 2,
				100U, 600000U);
	}

	queue_delayed_work(ps_motosh->irq_work_queue, &ps_motosh->irq_wake_work,
		msecs_to_jiffies(ps_motosh->wake_work_delay));

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
	bool pending_reset = false;
	u8 pending_reset_reason;
	unsigned char cmdbuff[MOTOSH_MAXDATA_LENGTH];
	unsigned char readbuff[MOTOSH_MAXDATA_LENGTH];
	struct motosh_data *ps_motosh = container_of(
			(struct delayed_work *)work,
			struct motosh_data, irq_wake_work);
	int log_msg_ctr = 0;
	struct motosh_platform_data *pdata;
	pdata = ps_motosh->pdata;

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
	cmdbuff[0] = WAKESENSOR_STATUS;
	if (motosh_misc_data->in_reset_and_init) {
		/* only apply delay if issue observed before */
		if (spurious_det)
			msleep(SPURIOUS_INT_DELAY);

		err = motosh_i2c_write_read_no_reset(ps_motosh,
						     cmdbuff, readbuff, 1, 3);
		if (err < 0) {
			spurious_det = 1;
			dev_err(&ps_motosh->client->dev,
				"Spurious interrupt? Retry... [err: %d]\n",
				err);
			motosh_reset_and_init(START_RESET);
			goto EXIT;
		}
	} else {
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 3);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading wake sensor status failed [err: %d]\n",
				err);
			goto EXIT;
		}
	}

	irq_status = (readbuff[IRQ_WAKE_HI] << 16)
			| (readbuff[IRQ_WAKE_MED] << 8) | readbuff[IRQ_WAKE_LO];

	if (ps_motosh->qw_irq_status) {
		irq_status |= ps_motosh->qw_irq_status;
		ps_motosh->qw_irq_status = 0;
	}

	/* Check if we are coming out of normal reset and/or
	   the part has self-reset */
	if (irq_status & M_INIT_COMPLETE) {
		dev_info(&ps_motosh->client->dev,
			"sensorhub reports reset [%d]", spurious_det);
		motosh_reset_and_init(COMPLETE_INIT);
	}
	if (irq_status & M_TOUCH) {
		if (motosh_display_handle_touch_locked(ps_motosh) == -EIO)
			goto EXIT;
	}
	if (irq_status & M_QUICKPEEK) {
		if (motosh_display_handle_quickpeek_locked(ps_motosh) == -EIO)
			goto EXIT;
	}

	/* read wake queue length */
	cmdbuff[0] = WAKE_MSG_QUEUE_LEN;
	err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1, 1);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading wake queue length failed [err: %d]\n", err);
		goto EXIT;
	}
	queue_length = readbuff[0];

	dev_dbg(&ps_motosh->client->dev,
			"wake queue_length: %d\n", queue_length);

	valid_queue_len = ((queue_length > 0) &&
			   (queue_length <= MOTOSH_MAX_EVENT_QUEUE_SIZE));
	if (!valid_queue_len) {
		dev_dbg(&ps_motosh->client->dev,
			"Invalid wake queue_length: %d\n", queue_length);
		goto PROCESS_LOGS;
	}

	/* read wake queue */
	cmdbuff[0] = WAKE_MSG_QUEUE;
	err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
		1, queue_length);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading wake queue failed [len: %d] [err: %d]\n",
			queue_length,
			err);
		goto EXIT;
	}

	/* process each event from the queue */
	/* NOTE: the readbuff should not be modified while the event
	   queue is being processed */
	while (queue_index < queue_length) {
		unsigned char *data;
		unsigned char message_id = readbuff[queue_index];

		queue_index += MOTOSH_EVENT_QUEUE_MSG_ID_LEN;
		data = &readbuff[queue_index];

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
			}
			break;
		case RESET_REASON:
			pending_reset = true;
			pending_reset_reason = data[0];
			queue_index += 1;
			break;
		case DOCKED_DATA:
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
		case PROXIMITY_DATA:
			motosh_as_data_buffer_write(ps_motosh, DT_PROX,
				data, 1, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Proximity distance %d\n",
				data[PROX_DISTANCE]);
			queue_index += 1;
			break;
		case COVER_DATA:
			if ((pdata->cover_detect_polarity
				& data[COVER_STATE]) != MOTOSH_HALL_NO_DETECT)
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

			/* On folio changes touch configuration will
			 * usually need to be changed */
			motosh_check_touch_config_locked(NORMAL_CHECK);

			dev_info(&ps_motosh->client->dev, "Cover status: %d\n",
				state);
			queue_index += 1;
			break;
		case STOWED_DATA:
			/* Just pass the first byte for stowed status
				Rest is for logging */
			motosh_as_data_buffer_write(ps_motosh, DT_STOWED,
				data, 1, 0, false);

			dev_info(&ps_motosh->client->dev,
				"Sending Stowed status %d, als %d, prox %d\n",
				data[STOWED_STATUS],
				STM16_TO_HOST(&data[STOWED_ALS], ALS_VALUE),
				data[STOWED_PROX]);
			queue_index += 4;
			break;
		case CAMERA_GESTURE:
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
		case NFC_DATA:
			motosh_as_data_buffer_write(ps_motosh, DT_NFC,
					data, 1, 0, false);

			dev_dbg(&ps_motosh->client->dev,
				"Sending NFC value: %d\n", data[0]);
			queue_index += 1;
			break;
		case SIM_DATA:
			motosh_as_data_buffer_write(ps_motosh, DT_SIM,
					data, 2, 0, false);

			/* This is one shot sensor */
			motosh_g_wake_sensor_state &= (~M_SIM);

			dev_dbg(&ps_motosh->client->dev, "Sending SIM Value=%d\n",
				STM16_TO_HOST(data, SIM_OFFSET));
			queue_index += 2;
			break;
		case CHOPCHOP_DATA:
			motosh_as_data_buffer_write(ps_motosh, DT_CHOPCHOP,
							data, 2, 0, false);

			dev_dbg(&ps_motosh->client->dev, "ChopChop triggered. Gyro aborts=%d\n",
					STM16_TO_HOST(data,
						CHOPCHOP_DATA_OFFSET));
			queue_index += 2;
			break;
		case LIFT_DATA:
			motosh_as_data_buffer_write(ps_motosh, DT_LIFT,
							data, 12, 0, false);

			dev_dbg(&ps_motosh->client->dev, "Lift triggered. Dist=%d. ZRot=%d. GravDiff=%d.\n",
					STM32_TO_HOST(data, LIFT_DISTANCE),
					STM32_TO_HOST(data, LIFT_ROTATION),
					STM32_TO_HOST(data, LIFT_GRAV_DIFF));
			queue_index += 12;
			break;
		case GLANCE_REG:
			motosh_as_data_buffer_write(ps_motosh, DT_GLANCE,
							data, 2, 0, false);

			dev_dbg(&ps_motosh->client->dev, "Glance Gesture=%d\n",
					STM16_TO_HOST(data, 0));
			queue_index += 2;
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
		case ALGO_EVT_ORIENT:
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
			dev_dbg(&ps_motosh->client->dev,
				"Sending accum mvmt event\n");
			queue_index += 7;
		}
			break;
		case IR_GESTURE:
			queue_index += motosh_process_ir_gesture(ps_motosh,
								 data);
			break;
		case GENERIC_INT:
			motosh_ms_data_buffer_write(ps_motosh, DT_GENERIC_INT,
				data, 1, false);
			dev_dbg(&ps_motosh->client->dev,
				"Sending generic interrupt event:%d\n",
				data[0]);
			queue_index += 1;
			break;
		case GYRO_CAL:
			dev_dbg(&ps_motosh->client->dev,
				"Store gyro calibration\n");
			motosh_as_data_buffer_write(ps_motosh, DT_GYRO_CAL,
				NULL, 0, 0, false);
			break;
		default:
			/* ERROR...unknown message
			   Need to drop the remaining data in this operation. */
			dev_err(&ps_motosh->client->dev,
				"ERROR: unknown wake msg: 0x%02X\n",
				message_id);
			/* a write to the work queue length causes
			   it to be reset */
			cmdbuff[0] = WAKE_MSG_QUEUE_LEN;
			cmdbuff[1] = 0x00;
			motosh_i2c_write(ps_motosh, cmdbuff, 2);
			/* exit wake queue loop */
			queue_length = 0;
		};
	}

PROCESS_LOGS:
	/* Log messages are stored in a separate queue.
	   Read at most MAX_NUM_LOGS_PER_INT, or if pending_reset is set allow
	   5 times that many. */
	log_msg_ctr = (pending_reset ? 5 * MAX_NUM_LOGS_PER_INT :
				       MAX_NUM_LOGS_PER_INT);
	for (; log_msg_ctr > 0; log_msg_ctr--) {
		u8 log_msg_len;

		cmdbuff[0] = LOG_MSG_LEN;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading log msg length failed [err: %d]\n",
				err);
			goto PROCESS_RESET;
		}
		log_msg_len = readbuff[0];

		dev_dbg(&ps_motosh->client->dev,
				"log msg length: %d\n", log_msg_len);

		if (log_msg_len > MAX_LOG_MSG_LEN) {
			dev_err(&ps_motosh->client->dev,
				"ERROR: invalid log msg length: 0x%02X\n",
				log_msg_len);
			/* a write to the log queue length causes
			   it to be reset */
			cmdbuff[0] = LOG_MSG_LEN;
			cmdbuff[1] = 0x00;
			motosh_i2c_write(ps_motosh, cmdbuff, 2);
			goto PROCESS_RESET;
		} else if (log_msg_len > 0) {
			cmdbuff[0] = LOG_MSG;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, log_msg_len);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading log queue failed [len: %d] [err: %d]\n",
					log_msg_len,
					err);
				goto PROCESS_RESET;
			} else {
				process_log_message(ps_motosh,
						readbuff,
						log_msg_len);
			}
		} else
			break; /* no more log messages to process */
	}

PROCESS_RESET:
	/* process a reset request after dumping any last logs */
	if (pending_reset) {
		motosh_as_data_buffer_write(ps_motosh, DT_RESET,
					    &pending_reset_reason,
					    1, 0, false);

		motosh_reset_and_init(START_RESET);
		dev_info(&ps_motosh->client->dev, "sensorhub requested a reset\n");
		goto EXIT;
	}

EXIT:
	/* if there were no surfaced events (only quickpeek/logging),
	   release the wakelock */
	if (irq_status == M_QUICKPEEK && !valid_queue_len)
		wake_unlock(&ps_motosh->wakelock);

	motosh_sleep(ps_motosh);
EXIT_NO_WAKE:
	mutex_unlock(&ps_motosh->lock);
}
