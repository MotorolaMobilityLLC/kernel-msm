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
	unsigned short irq_status;
	u32 irq2_status;
	uint8_t irq3_status;
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

	/* read interrupt mask register */
	motosh_cmdbuff[0] = WAKESENSOR_STATUS;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "Reading from motosh failed\n");
		goto EXIT;
	}
	irq_status = (motosh_readbuff[IRQ_WAKE_MED] << 8)
				| motosh_readbuff[IRQ_WAKE_LO];

	/* read algorithm interrupt status register */
	motosh_cmdbuff[0] = ALGO_INT_STATUS;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 3);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "Reading from motosh failed\n");
		goto EXIT;
	}
	irq2_status = (motosh_readbuff[IRQ_WAKE_HI] << 16) |
		(motosh_readbuff[IRQ_WAKE_MED] << 8) |
		motosh_readbuff[IRQ_WAKE_LO];

	/* read generic interrupt register */
	motosh_cmdbuff[0] = GENERIC_INT_STATUS;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "Reading from stm failed\n");
		goto EXIT;
	}
	irq3_status = motosh_readbuff[0];

	if (ps_motosh->qw_irq_status) {
		irq_status |= ps_motosh->qw_irq_status;
		ps_motosh->qw_irq_status = 0;
	}

	/* First, check for error messages */
	if (irq_status & M_LOG_MSG) {
		motosh_cmdbuff[0] = ERROR_STATUS;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
					    1, ESR_SIZE);

		if (err >= 0) {
			memcpy(stat_string, motosh_readbuff, ESR_SIZE);
			stat_string[ESR_SIZE] = 0;
			dev_err(&ps_motosh->client->dev,
				"MOTOSH Error: %s\n", stat_string);
		} else
			dev_err(&ps_motosh->client->dev,
				"Failed to read error message %d\n", err);
	}

	/* Second, check for a reset request */
	if (irq_status & M_HUB_RESET) {
		unsigned char status;

		if (strnstr(stat_string, "modality", ESR_SIZE))
			status = 0x01;
		else if (strnstr(stat_string, "Algo", ESR_SIZE))
			status = 0x02;
		else if (strnstr(stat_string, "Watchdog", ESR_SIZE))
			status = 0x03;
		else
			status = 0x04;

		motosh_as_data_buffer_write(ps_motosh, DT_RESET, &status, 1, 0);

		motosh_reset_and_init();
		dev_err(&ps_motosh->client->dev, "MOTOSH requested a reset\n");
		goto EXIT;
	}

	/* Check all other status bits */
	if (irq_status & M_DOCK) {
		int state;

		dev_err(&ps_motosh->client->dev,
			"Invalid M_DOCK bit set. irq_status = 0x%06x\n",
			irq_status);

		motosh_cmdbuff[0] = DOCK_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Dock state failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_DOCK,
			motosh_readbuff, 1, 0);
		state = motosh_readbuff[DOCK_STATE];
		if (ps_motosh->dsdev.dev != NULL)
			switch_set_state(&ps_motosh->dsdev, state);
		if (ps_motosh->edsdev.dev != NULL)
			switch_set_state(&ps_motosh->edsdev, state);

		dev_dbg(&ps_motosh->client->dev, "Dock status:%d\n", state);
	}
	if (irq_status & M_PROXIMITY) {
		motosh_cmdbuff[0] = PROXIMITY;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading prox from motosh failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_PROX,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Proximity distance %d\n",
			motosh_readbuff[PROX_DISTANCE]);
	}
	if (irq_status & M_TOUCH) {
		if (motosh_display_handle_touch_locked(ps_motosh) < 0)
			goto EXIT;
	}
	if (irq_status & M_QUICKPEEK) {
		if (motosh_display_handle_quickpeek_locked(ps_motosh,
			irq_status == M_QUICKPEEK) < 0)
			goto EXIT;
	}
	if (irq_status & M_COVER) {
		int state;
		motosh_cmdbuff[0] = COVER_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Cover state failed\n");
			goto EXIT;
		}

		if (motosh_readbuff[COVER_STATE] == MOTOSH_HALL_NORTH)
			state = 1;
		else
			state = 0;

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
		/* notify subscribers of cover state change */
		mmi_hall_notify(MMI_HALL_FOLIO, state);
#endif

		input_report_switch(ps_motosh->input_dev, SW_LID, state);
		input_sync(ps_motosh->input_dev);

		dev_err(&ps_motosh->client->dev, "Cover status: %d\n", state);
	}
	if (irq_status & M_FLATUP) {
		motosh_cmdbuff[0] = FLAT_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading flat data from motosh failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_FLAT_UP,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev, "Sending Flat up %d\n",
			motosh_readbuff[FLAT_UP]);
	}
	if (irq_status & M_FLATDOWN) {
		motosh_cmdbuff[0] = FLAT_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading flat data from motosh failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_FLAT_DOWN,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev, "Sending Flat down %d\n",
			motosh_readbuff[FLAT_DOWN]);
	}
	if (irq_status & M_STOWED) {
		motosh_cmdbuff[0] = STOWED;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading stowed from motosh failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_STOWED,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Stowed status %d\n", motosh_readbuff[STOWED]);
	}
	if (irq_status & M_CAMERA_ACT) {
		motosh_cmdbuff[0] = CAMERA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading camera data from stm failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_CAMERA_ACT,
			motosh_readbuff, 2, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Camera: %d\n", STM16_TO_HOST(CAMERA_VALUE));

		input_report_key(ps_motosh->input_dev, KEY_CAMERA, 1);
		input_report_key(ps_motosh->input_dev, KEY_CAMERA, 0);
		input_sync(ps_motosh->input_dev);
		dev_dbg(&ps_motosh->client->dev,
			"Report camkey toggle\n");
	}
	if (irq_status & M_NFC) {
		motosh_cmdbuff[0] = NFC;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading nfc data from stm failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_NFC,
				motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending NFC value: %d\n", motosh_readbuff[NFC_VALUE]);

	}
	if (irq_status & M_SIM) {
		motosh_cmdbuff[0] = SIM;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading sig_motion data from stm failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_SIM,
				motosh_readbuff, 2, 0);

		/* This is one shot sensor */
		motosh_g_wake_sensor_state &= (~M_SIM);

		dev_dbg(&ps_motosh->client->dev, "Sending SIM Value=%d\n",
					STM16_TO_HOST(SIM_DATA));
	}
	if (irq_status & M_CHOPCHOP) {
		motosh_cmdbuff[0] = CHOPCHOP;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading chopchop data from stm failed\n");
			goto EXIT;
		}

		motosh_as_data_buffer_write(ps_motosh, DT_CHOPCHOP,
						motosh_readbuff, 2, 0);

		dev_dbg(&ps_motosh->client->dev, "ChopChop triggered. Gyro aborts=%d\n",
				STM16_TO_HOST(CHOPCHOP_DATA));
	}
	if (irq2_status & M_MMOVEME) {
		unsigned char status;
		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & MOTOSH_CLIENT_MASK) | M_MMOVEME;
		motosh_ms_data_buffer_write(ps_motosh, DT_MMMOVE, &status, 1);

		dev_dbg(&ps_motosh->client->dev,
			"Sending meaningful movement event\n");
	}
	if (irq2_status & M_NOMMOVE) {
		unsigned char status;
		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & MOTOSH_CLIENT_MASK) | M_NOMMOVE;
		motosh_ms_data_buffer_write(ps_motosh, DT_NOMOVE, &status, 1);

		dev_dbg(&ps_motosh->client->dev,
			"Sending no meaningful movement event\n");
	}
	if (irq2_status & M_ALGO_MODALITY) {
		motosh_cmdbuff[0] =
			motosh_algo_info[MOTOSH_IDX_MODALITY].evt_register;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading modality event failed\n");
			goto EXIT;
		}
		motosh_readbuff[ALGO_TYPE] = MOTOSH_IDX_MODALITY;
		motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
			motosh_readbuff, 8);
		dev_dbg(&ps_motosh->client->dev, "Sending modality event\n");
	}
	if (irq2_status & M_ALGO_ORIENTATION) {
		motosh_cmdbuff[0] =
			motosh_algo_info[MOTOSH_IDX_ORIENTATION].evt_register;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading orientation event failed\n");
			goto EXIT;
		}
		motosh_readbuff[ALGO_TYPE] = MOTOSH_IDX_ORIENTATION;
		motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
			motosh_readbuff, 8);
		dev_dbg(&ps_motosh->client->dev, "Sending orientation event\n");
	}
	if (irq2_status & M_ALGO_STOWED) {
		motosh_cmdbuff[0] =
			motosh_algo_info[MOTOSH_IDX_STOWED].evt_register;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading stowed event failed\n");
			goto EXIT;
		}
		motosh_readbuff[ALGO_TYPE] = MOTOSH_IDX_STOWED;
		motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
			motosh_readbuff, 8);
		dev_dbg(&ps_motosh->client->dev, "Sending stowed event\n");
	}
	if (irq2_status & M_ALGO_ACCUM_MODALITY) {
		motosh_cmdbuff[0] =
			motosh_algo_info[MOTOSH_IDX_ACCUM_MODALITY]
				.evt_register;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_EVT_SZ_ACCUM_STATE);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading accum modality event failed\n");
			goto EXIT;
		}
		motosh_readbuff[ALGO_TYPE] = MOTOSH_IDX_ACCUM_MODALITY;
		motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
			motosh_readbuff, 8);
		dev_dbg(&ps_motosh->client->dev, "Sending accum modality event\n");
	}
	if (irq2_status & M_ALGO_ACCUM_MVMT) {
		motosh_cmdbuff[0] =
			motosh_algo_info[MOTOSH_IDX_ACCUM_MVMT].evt_register;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_EVT_SZ_ACCUM_MVMT);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading accum mvmt event failed\n");
			goto EXIT;
		}
		motosh_readbuff[ALGO_TYPE] = MOTOSH_IDX_ACCUM_MVMT;
		motosh_ms_data_buffer_write(ps_motosh, DT_ALGO_EVT,
			motosh_readbuff, 8);
		dev_dbg(&ps_motosh->client->dev, "Sending accum mvmt event\n");
	}
	if (irq2_status & M_IR_WAKE_GESTURE) {
		err = motosh_process_ir_gesture(ps_motosh);
		if (err < 0)
			goto EXIT;
	}
	if (irq3_status & M_GENERIC_INTRPT) {

		dev_err(&ps_motosh->client->dev,
			"Invalid M_GENERIC_INTRPT bit set. irq_status = 0x%06x\n",
			irq_status);

		/* x (data1) : irq3_status */
		motosh_ms_data_buffer_write(ps_motosh, DT_GENERIC_INT,
			&irq3_status, 1);
		dev_dbg(&ps_motosh->client->dev,
			"Sending generic interrupt event:%d\n", irq3_status);
	}

EXIT:
	motosh_sleep(ps_motosh);
EXIT_NO_WAKE:
	mutex_unlock(&ps_motosh->lock);
}
