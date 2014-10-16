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

#include <linux/stm401.h>


irqreturn_t stm401_wake_isr(int irq, void *dev)
{
	struct stm401_data *ps_stm401 = dev;

	if (stm401_irq_disable) {
		return IRQ_HANDLED;
	}

	wake_lock_timeout(&ps_stm401->wakelock, HZ);

	queue_work(ps_stm401->irq_work_queue, &ps_stm401->irq_wake_work);
	return IRQ_HANDLED;
}

void stm401_irq_wake_work_func(struct work_struct *work)
{
	int err;
	unsigned short irq_status;
	u32 irq2_status;
	uint8_t irq3_status;
	struct stm401_data *ps_stm401 = container_of(work,
			struct stm401_data, irq_wake_work);

	dev_dbg(&ps_stm401->client->dev, "stm401_irq_wake_work_func\n");
	mutex_lock(&ps_stm401->lock);

	if (ps_stm401->mode == BOOTMODE)
		goto EXIT_NO_WAKE;

	/* This is to handle the case of receiving an interrupt after
	   suspend_late, but before interrupts were globally disabled. If this
	   is the case, interrupts might be disabled now, so we cannot handle
	   this at this time. suspend_noirq will return BUSY if this happens
	   so that we can handle these interrupts. */
	if (ps_stm401->ignore_wakeable_interrupts) {
		dev_info(&ps_stm401->client->dev,
			"Deferring interrupt work\n");
		ps_stm401->ignored_interrupts++;
		goto EXIT_NO_WAKE;
	}

	stm401_wake(ps_stm401);

	/* read interrupt mask register */
	stm401_cmdbuff[0] = WAKESENSOR_STATUS;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading from stm401 failed\n");
		goto EXIT;
	}
	irq_status = (stm401_readbuff[IRQ_WAKE_MED] << 8)
				| stm401_readbuff[IRQ_WAKE_LO];

	/* read algorithm interrupt status register */
	stm401_cmdbuff[0] = ALGO_INT_STATUS;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 3);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading from stm401 failed\n");
		goto EXIT;
	}
	irq2_status = (stm401_readbuff[IRQ_WAKE_HI] << 16) |
		(stm401_readbuff[IRQ_WAKE_MED] << 8) |
		stm401_readbuff[IRQ_WAKE_LO];

	/* read generic interrupt register */
	stm401_cmdbuff[0] = GENERIC_INT_STATUS;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading from stm failed\n");
		goto EXIT;
	}
	irq3_status = stm401_readbuff[0];

	if (ps_stm401->qw_irq_status) {
		irq_status |= ps_stm401->qw_irq_status;
		ps_stm401->qw_irq_status = 0;
	}

	/* First, check for error messages */
	if (irq_status & M_LOG_MSG) {
		stm401_cmdbuff[0] = ERROR_STATUS;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff,
			1, ESR_SIZE);
		if (err >= 0) {
			memcpy(stat_string, stm401_readbuff, ESR_SIZE);
			stat_string[ESR_SIZE] = 0;
			dev_err(&ps_stm401->client->dev,
				"STM401 Error: %s\n", stat_string);
		} else
			dev_err(&ps_stm401->client->dev,
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

		stm401_as_data_buffer_write(ps_stm401, DT_RESET, &status, 1, 0);

		stm401_reset_and_init();
		dev_err(&ps_stm401->client->dev, "STM401 requested a reset\n");
		goto EXIT;
	}

	/* Check all other status bits */
	if (irq_status & M_DOCK) {
		int state;

		dev_err(&ps_stm401->client->dev,
			"Invalid M_DOCK bit set. irq_status = 0x%06x\n",
			irq_status);

		stm401_cmdbuff[0] = DOCK_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Dock state failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_DOCK,
			stm401_readbuff, 1, 0);
		state = stm401_readbuff[DOCK_STATE];
		if (ps_stm401->dsdev.dev != NULL)
			switch_set_state(&ps_stm401->dsdev, state);
		if (ps_stm401->edsdev.dev != NULL)
			switch_set_state(&ps_stm401->edsdev, state);

		dev_dbg(&ps_stm401->client->dev, "Dock status:%d\n", state);
	}
	if (irq_status & M_PROXIMITY) {
		stm401_cmdbuff[0] = PROXIMITY;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading prox from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_PROX,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Proximity distance %d\n",
			stm401_readbuff[PROX_DISTANCE]);
	}
	if (irq_status & M_TOUCH) {
		if (stm401_display_handle_touch_locked(ps_stm401) < 0)
			goto EXIT;
	}
	if (irq_status & M_COVER) {
		int state;
		stm401_cmdbuff[0] = COVER_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Cover state failed\n");
			goto EXIT;
		}

		state = stm401_readbuff[COVER_STATE];
		if (state > 0)
			state = 1;

		input_report_switch(ps_stm401->input_dev, SW_LID, state);
		input_sync(ps_stm401->input_dev);

		dev_dbg(&ps_stm401->client->dev, "Cover status: %d\n", state);
	}
	if (irq_status & M_QUICKPEEK) {
		if (stm401_display_handle_quickpeek_locked(ps_stm401,
			irq_status == M_QUICKPEEK) < 0)
			goto EXIT;
	}
	if (irq_status & M_FLATUP) {
		stm401_cmdbuff[0] = FLAT_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading flat data from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_FLAT_UP,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending Flat up %d\n",
			stm401_readbuff[FLAT_UP]);
	}
	if (irq_status & M_FLATDOWN) {
		stm401_cmdbuff[0] = FLAT_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading flat data from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_FLAT_DOWN,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending Flat down %d\n",
			stm401_readbuff[FLAT_DOWN]);
	}
	if (irq_status & M_STOWED) {
		stm401_cmdbuff[0] = STOWED;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading stowed from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_STOWED,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Stowed status %d\n", stm401_readbuff[STOWED]);
	}
	if (irq_status & M_CAMERA_ACT) {
		stm401_cmdbuff[0] = CAMERA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading camera data from stm failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_CAMERA_ACT,
			stm401_readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Camera: %d\n", STM16_TO_HOST(CAMERA_VALUE));

		input_report_key(ps_stm401->input_dev, KEY_CAMERA, 1);
		input_report_key(ps_stm401->input_dev, KEY_CAMERA, 0);
		input_sync(ps_stm401->input_dev);
		dev_dbg(&ps_stm401->client->dev,
			"Report camkey toggle\n");
	}
	if (irq_status & M_NFC) {
		stm401_cmdbuff[0] = NFC;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading nfc data from stm failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_NFC,
				stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending NFC value: %d\n", stm401_readbuff[NFC_VALUE]);

	}
	if (irq_status & M_SIM) {
		stm401_cmdbuff[0] = SIM;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading sig_motion data from stm failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_SIM,
				stm401_readbuff, 2, 0);

		/* This is one shot sensor */
		stm401_g_wake_sensor_state &= (~M_SIM);

		dev_dbg(&ps_stm401->client->dev, "Sending SIM Value=%d\n",
					STM16_TO_HOST(SIM_DATA));
	}
	if (irq_status & M_CHOPCHOP) {
		stm401_cmdbuff[0] = CHOPCHOP;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading chopchop data from stm failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_CHOPCHOP,
						stm401_readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev, "ChopChop triggered. Gyro aborts=%d\n",
				STM16_TO_HOST(CHOPCHOP_DATA));
	}
	if (irq2_status & M_MMOVEME) {
		unsigned char status;
		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STM401_CLIENT_MASK) | M_MMOVEME;
		stm401_ms_data_buffer_write(ps_stm401, DT_MMMOVE, &status, 1);

		dev_dbg(&ps_stm401->client->dev,
			"Sending meaningful movement event\n");
	}
	if (irq2_status & M_NOMMOVE) {
		unsigned char status;
		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STM401_CLIENT_MASK) | M_NOMMOVE;
		stm401_ms_data_buffer_write(ps_stm401, DT_NOMOVE, &status, 1);

		dev_dbg(&ps_stm401->client->dev,
			"Sending no meaningful movement event\n");
	}
	if (irq2_status & M_ALGO_MODALITY) {
		stm401_cmdbuff[0] =
			stm401_algo_info[STM401_IDX_MODALITY].evt_register;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading modality event failed\n");
			goto EXIT;
		}
		stm401_readbuff[ALGO_TYPE] = STM401_IDX_MODALITY;
		stm401_ms_data_buffer_write(ps_stm401, DT_ALGO_EVT,
			stm401_readbuff, 8);
		dev_dbg(&ps_stm401->client->dev, "Sending modality event\n");
	}
	if (irq2_status & M_ALGO_ORIENTATION) {
		stm401_cmdbuff[0] =
			stm401_algo_info[STM401_IDX_ORIENTATION].evt_register;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading orientation event failed\n");
			goto EXIT;
		}
		stm401_readbuff[ALGO_TYPE] = STM401_IDX_ORIENTATION;
		stm401_ms_data_buffer_write(ps_stm401, DT_ALGO_EVT,
			stm401_readbuff, 8);
		dev_dbg(&ps_stm401->client->dev, "Sending orientation event\n");
	}
	if (irq2_status & M_ALGO_STOWED) {
		stm401_cmdbuff[0] =
			stm401_algo_info[STM401_IDX_STOWED].evt_register;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading stowed event failed\n");
			goto EXIT;
		}
		stm401_readbuff[ALGO_TYPE] = STM401_IDX_STOWED;
		stm401_ms_data_buffer_write(ps_stm401, DT_ALGO_EVT,
			stm401_readbuff, 8);
		dev_dbg(&ps_stm401->client->dev, "Sending stowed event\n");
	}
	if (irq2_status & M_ALGO_ACCUM_MODALITY) {
		stm401_cmdbuff[0] =
			stm401_algo_info[STM401_IDX_ACCUM_MODALITY]
				.evt_register;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_EVT_SZ_ACCUM_STATE);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading accum modality event failed\n");
			goto EXIT;
		}
		stm401_readbuff[ALGO_TYPE] = STM401_IDX_ACCUM_MODALITY;
		stm401_ms_data_buffer_write(ps_stm401, DT_ALGO_EVT,
			stm401_readbuff, 8);
		dev_dbg(&ps_stm401->client->dev, "Sending accum modality event\n");
	}
	if (irq2_status & M_ALGO_ACCUM_MVMT) {
		stm401_cmdbuff[0] =
			stm401_algo_info[STM401_IDX_ACCUM_MVMT].evt_register;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_EVT_SZ_ACCUM_MVMT);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading accum mvmt event failed\n");
			goto EXIT;
		}
		stm401_readbuff[ALGO_TYPE] = STM401_IDX_ACCUM_MVMT;
		stm401_ms_data_buffer_write(ps_stm401, DT_ALGO_EVT,
			stm401_readbuff, 8);
		dev_dbg(&ps_stm401->client->dev, "Sending accum mvmt event\n");
	}
	if (irq2_status & M_IR_WAKE_GESTURE) {
		err = stm401_process_ir_gesture(ps_stm401);
		if (err < 0)
			goto EXIT;
	}
	if (irq3_status & M_GENERIC_INTRPT) {

		dev_err(&ps_stm401->client->dev,
			"Invalid M_GENERIC_INTRPT bit set. irq_status = 0x%06x\n",
			irq_status);

		/* x (data1) : irq3_status */
		stm401_ms_data_buffer_write(ps_stm401, DT_GENERIC_INT,
			&irq3_status, 1);
		dev_dbg(&ps_stm401->client->dev,
			"Sending generic interrupt event:%d\n", irq3_status);
	}

EXIT:
	stm401_sleep(ps_stm401);
EXIT_NO_WAKE:
	mutex_unlock(&ps_stm401->lock);
}
