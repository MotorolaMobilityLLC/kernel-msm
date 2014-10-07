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

enum headset_state_t {
	SH_HEADSET_REMOVED,
	SH_HEADPHONE_INSERTED,
	SH_HEADSET_INSERTED,
	SH_HEADSET_BUTTON_1,
	SH_HEADSET_BUTTON_2,
	SH_HEADSET_BUTTON_3,
	SH_HEADSET_BUTTON_4
};

enum headset_state_t Headset_State = SH_HEADSET_REMOVED;

irqreturn_t stml0xx_wake_isr(int irq, void *dev)
{
	struct stml0xx_data *ps_stml0xx = dev;

	if (stml0xx_irq_disable)
		return IRQ_HANDLED;

	wake_lock_timeout(&ps_stml0xx->wakelock, HZ);

	queue_work(ps_stml0xx->irq_work_queue, &ps_stml0xx->irq_wake_work);
	return IRQ_HANDLED;
}

void stml0xx_irq_wake_work_func(struct work_struct *work)
{
	int err;
	unsigned short irq_status;
	u32 irq2_status;
	uint8_t irq3_status;
	struct stml0xx_data *ps_stml0xx = stml0xx_misc_data;
	unsigned char buf[SPI_MSG_SIZE];

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_irq_wake_work_func");
	mutex_lock(&ps_stml0xx->lock);

	if (ps_stml0xx->mode == BOOTMODE)
		goto EXIT_NO_WAKE;

	if (ps_stml0xx->is_suspended) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"setting pending_wake_work [true]");
		ps_stml0xx->pending_wake_work = true;
		goto EXIT_NO_WAKE;
	}

	stml0xx_wake(ps_stml0xx);

	/* read interrupt mask register */
	err = stml0xx_spi_send_read_reg(WAKESENSOR_STATUS, buf, 2);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Reading from stml0xx failed");
		goto EXIT;
	}
	irq_status = (buf[IRQ_WAKE_MED] << 8)
	    | buf[IRQ_WAKE_LO];

	/* read algorithm interrupt status register */
	err = stml0xx_spi_send_read_reg(ALGO_INT_STATUS, buf, 3);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Reading from stml0xx failed");
		goto EXIT;
	}
	irq2_status = (buf[IRQ_WAKE_HI] << 16) |
	    (buf[IRQ_WAKE_MED] << 8) | buf[IRQ_WAKE_LO];

	/* read generic interrupt register */
	err = stml0xx_spi_send_read_reg(GENERIC_INT_STATUS, buf, 1);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Reading from stm failed");
		goto EXIT;
	}
	irq3_status = buf[0];

	/* First, check for error messages */
	if (irq_status & M_LOG_MSG) {
		err = stml0xx_spi_send_read_reg(ERROR_STATUS, buf, ESR_SIZE);
		if (err >= 0) {
			memcpy(stat_string, buf, ESR_SIZE);
			stat_string[ESR_SIZE] = 0;
			dev_err(&stml0xx_misc_data->spi->dev,
				"STML0XX Error: %s", stat_string);
		} else
			dev_err(&stml0xx_misc_data->spi->dev,
				"Failed to read error message %d", err);
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

		stml0xx_as_data_buffer_write(ps_stml0xx, DT_RESET, &status, 1,
					     0);

		stml0xx_reset(stml0xx_misc_data->pdata, stml0xx_cmdbuff);
		dev_err(&stml0xx_misc_data->spi->dev,
			"STML0XX requested a reset");
		goto EXIT;
	}

	/* Check all other status bits */
	if (irq_status & M_DOCK) {
		int state;

		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid M_DOCK bit set. irq_status = 0x%06x",
			irq_status);

		err = stml0xx_spi_send_read_reg(DOCK_DATA, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading Dock state failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_DOCK, buf, 1, 0);
		state = buf[DOCK_STATE];
		if (ps_stml0xx->dsdev.dev != NULL)
			switch_set_state(&ps_stml0xx->dsdev, state);
		if (ps_stml0xx->edsdev.dev != NULL)
			switch_set_state(&ps_stml0xx->edsdev, state);

		dev_dbg(&stml0xx_misc_data->spi->dev, "Dock status:%d", state);
	}
	if (irq_status & M_PROXIMITY) {
		err = stml0xx_spi_send_read_reg(PROXIMITY, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading prox from stml0xx failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_PROX, buf, 1, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Proximity distance %d", buf[PROX_DISTANCE]);
	}
	if (irq_status & M_COVER) {
		int state;
		err = stml0xx_spi_send_read_reg(COVER_DATA, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading Cover state failed");
			goto EXIT;
		}

		state = buf[COVER_STATE];
		if (state > 0)
			state = 1;

		input_report_switch(ps_stml0xx->input_dev, SW_LID, state);
		input_sync(ps_stml0xx->input_dev);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Cover status: %d", state);
	}
	if (irq_status & M_HEADSET) {
		uint8_t new_state;

		err = stml0xx_spi_send_read_reg(HEADSET_DATA, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading Headset state failed");
			goto EXIT;
		}
		new_state = buf[HEADSET_STATE];

		switch (Headset_State) {
		case SH_HEADSET_BUTTON_1:
			if (!(new_state & SH_HEADSET_BUTTON_1_DOWN)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 1 released");
				Headset_State = SH_HEADSET_INSERTED;
			}
			break;
		case SH_HEADSET_BUTTON_2:
			if (!(new_state & SH_HEADSET_BUTTON_2_DOWN)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 2 released");
				Headset_State = SH_HEADSET_INSERTED;
			}
			break;
		case SH_HEADSET_BUTTON_3:
			if (!(new_state & SH_HEADSET_BUTTON_3_DOWN)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 3 released");
				Headset_State = SH_HEADSET_INSERTED;
			}
			break;
		case SH_HEADSET_BUTTON_4:
			if (!(new_state & SH_HEADSET_BUTTON_4_DOWN)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 4 released");
				Headset_State = SH_HEADSET_INSERTED;
			}
			break;
		default:
			break;
		}
		if (Headset_State == SH_HEADPHONE_INSERTED) {
			if (!(new_state & SH_HEADPHONE_DETECTED)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headphone removed");
				Headset_State = SH_HEADSET_REMOVED;
			}
		} else if (Headset_State ==  SH_HEADSET_INSERTED) {
			if (!(new_state & SH_HEADSET_DETECTED)) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset removed");
				Headset_State = SH_HEADSET_REMOVED;
			}
		}
		if (Headset_State == SH_HEADSET_REMOVED) {
			if (new_state & SH_HEADPHONE_DETECTED) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headphone inserted");
				Headset_State = SH_HEADPHONE_INSERTED;
			} else if (new_state & SH_HEADSET_DETECTED) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset inserted");
				Headset_State = SH_HEADSET_INSERTED;
			}
		}
		if (Headset_State == SH_HEADSET_INSERTED) {
			if (new_state & SH_HEADSET_BUTTON_1_DOWN) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 1 pressed");
				Headset_State = SH_HEADSET_BUTTON_1;
			} else if (new_state & SH_HEADSET_BUTTON_2_DOWN) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 2 pressed");
				Headset_State = SH_HEADSET_BUTTON_2;
			} else if (new_state & SH_HEADSET_BUTTON_3_DOWN) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 3 pressed");
				Headset_State = SH_HEADSET_BUTTON_3;
			} else if (new_state & SH_HEADSET_BUTTON_4_DOWN) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Headset button 4 pressed");
				Headset_State = SH_HEADSET_BUTTON_4;
			}
		}
	}
	if (irq_status & M_INIT_COMPLETE) {
		/* set the init complete register, */
		/* to let the hub know it was received */
		buf[0] = 0x01;
		err = stml0xx_spi_send_write_reg(INIT_COMPLETE_REG, buf, 1);

		queue_work(ps_stml0xx->irq_work_queue,
			&ps_stml0xx->initialize_work);
		dev_err(&stml0xx_misc_data->spi->dev,
			"Sensor Hub reports reset");
		stml0xx_g_booted = 1;
	}
	if (irq_status & M_FLATUP) {
		err = stml0xx_spi_send_read_reg(FLAT_DATA, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading flat data from stml0xx failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_FLAT_UP, buf, 1, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Flat up %d", buf[FLAT_UP]);
	}
	if (irq_status & M_FLATDOWN) {
		err = stml0xx_spi_send_read_reg(FLAT_DATA, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading flat data from stml0xx failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_FLAT_DOWN,
					     buf, 1, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Flat down %d", buf[FLAT_DOWN]);
	}
	if (irq_status & M_STOWED) {
		err = stml0xx_spi_send_read_reg(STOWED, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading stowed from stml0xx failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_STOWED, buf, 1, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Stowed status %d", buf[STOWED_STATUS]);
	}
	if (irq_status & M_CAMERA_ACT) {
		err = stml0xx_spi_send_read_reg(CAMERA, buf, 2);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading camera data from stm failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_CAMERA_ACT,
					     buf, 2, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Camera: %d", STM16_TO_HOST(CAMERA_VALUE));

		input_report_key(ps_stml0xx->input_dev, KEY_CAMERA, 1);
		input_report_key(ps_stml0xx->input_dev, KEY_CAMERA, 0);
		input_sync(ps_stml0xx->input_dev);
		dev_dbg(&stml0xx_misc_data->spi->dev, "Report camkey toggle");
	}
	if (irq_status & M_NFC) {
		err = stml0xx_spi_send_read_reg(NFC, buf, 1);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading nfc data from stm failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_NFC, buf, 1, 0);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending NFC value: %d", buf[NFC_VALUE]);

	}
	if (irq_status & M_SIM) {
		err = stml0xx_spi_send_read_reg(SIM, buf, 2);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading sig_motion data from stm failed");
			goto EXIT;
		}
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_SIM, buf, 2, 0);

		/* This is one shot sensor */
		stml0xx_g_wake_sensor_state &= (~M_SIM);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending SIM Value=%d", STM16_TO_HOST(SIM_DATA));
	}
	if (irq2_status & M_MMOVEME) {
		unsigned char status;

		/* read motion data reg to clear movement interrupt */
		err = stml0xx_spi_send_read_reg(MOTION_DATA, buf, 2);

		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STML0XX_CLIENT_MASK) | M_MMOVEME;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_MMMOVE, &status, 1);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending meaningful movement event");
	}
	if (irq2_status & M_NOMMOVE) {
		unsigned char status;

		/* read motion data reg to clear movement interrupt */
		err = stml0xx_spi_send_read_reg(MOTION_DATA, buf, 2);

		/* Client recieving action will be upper 2 most significant */
		/* bits of the least significant byte of status. */
		status = (irq2_status & STML0XX_CLIENT_MASK) | M_NOMMOVE;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_NOMOVE, &status, 1);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending no meaningful movement event");
	}
	if (irq2_status & M_ALGO_MODALITY) {
		err =
		    stml0xx_spi_send_read_reg(stml0xx_algo_info
				[STML0XX_IDX_MODALITY].evt_register,
				buf, STML0XX_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading modality event failed");
			goto EXIT;
		}
		buf[ALGO_TYPE] = STML0XX_IDX_MODALITY;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT, buf, 8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending modality event");
	}
	if (irq2_status & M_ALGO_ORIENTATION) {
		err =
		    stml0xx_spi_send_read_reg(stml0xx_algo_info
				[STML0XX_IDX_ORIENTATION].evt_register,
				buf, STML0XX_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading orientation event failed");
			goto EXIT;
		}
		buf[ALGO_TYPE] = STML0XX_IDX_ORIENTATION;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT, buf, 8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending orientation event");
	}
	if (irq2_status & M_ALGO_STOWED) {
		err =
		    stml0xx_spi_send_read_reg(stml0xx_algo_info
				[STML0XX_IDX_STOWED].evt_register,
				buf, STML0XX_EVT_SZ_TRANSITION);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading stowed event failed");
			goto EXIT;
		}
		buf[ALGO_TYPE] = STML0XX_IDX_STOWED;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT, buf, 8);
		dev_dbg(&stml0xx_misc_data->spi->dev, "Sending stowed event");
	}
	if (irq2_status & M_ALGO_ACCUM_MODALITY) {
		err =
		    stml0xx_spi_send_read_reg(stml0xx_algo_info
				[STML0XX_IDX_ACCUM_MODALITY].evt_register,
				buf, STML0XX_EVT_SZ_ACCUM_STATE);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading accum modality event failed");
			goto EXIT;
		}
		buf[ALGO_TYPE] = STML0XX_IDX_ACCUM_MODALITY;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT, buf, 8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending accum modality event");
	}
	if (irq2_status & M_ALGO_ACCUM_MVMT) {
		err =
		    stml0xx_spi_send_read_reg(stml0xx_algo_info
				[STML0XX_IDX_ACCUM_MVMT].evt_register,
				buf, STML0XX_EVT_SZ_ACCUM_MVMT);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Reading accum mvmt event failed");
			goto EXIT;
		}
		buf[ALGO_TYPE] = STML0XX_IDX_ACCUM_MVMT;
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_ALGO_EVT, buf, 8);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending accum mvmt event");
	}
	if (irq3_status & M_GENERIC_INTRPT) {

		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid M_GENERIC_INTRPT bit set. irq_status = 0x%06x"
			, irq_status);

		/* x (data1) : irq3_status */
		stml0xx_ms_data_buffer_write(ps_stml0xx, DT_GENERIC_INT,
					     &irq3_status, 1);
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending generic interrupt event:%d", irq3_status);
	}

EXIT:
	stml0xx_sleep(ps_stml0xx);
EXIT_NO_WAKE:
	mutex_unlock(&ps_stml0xx->lock);
}
