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
#include <linux/workqueue.h>

#include <linux/stml0xx.h>

irqreturn_t stml0xx_isr(int irq, void *dev)
{
	static struct timespec ts;
	static struct stml0xx_work_struct *stm_ws;
	struct stml0xx_data *ps_stml0xx = dev;
	get_monotonic_boottime(&ts);

	if (stml0xx_irq_disable)
		return IRQ_HANDLED;

	stm_ws = kmalloc(
		sizeof(struct stml0xx_work_struct),
		GFP_ATOMIC);
	if (!stm_ws) {
		dev_err(&ps_stml0xx->spi->dev,
			"stml0xx_isr: unable to allocate work struct");
		return IRQ_HANDLED;
	}

	INIT_WORK((struct work_struct *)stm_ws, stml0xx_irq_work_func);
	stm_ws->ts_ns = ts_to_ns(ts);

	queue_work(ps_stml0xx->irq_work_queue, (struct work_struct *)stm_ws);
	return IRQ_HANDLED;
}

void stml0xx_irq_work_func(struct work_struct *work)
{
	int err;
	u32 irq_status;
	struct stml0xx_work_struct *stm_ws = (struct stml0xx_work_struct *)work;
	struct stml0xx_data *ps_stml0xx = stml0xx_misc_data;
	unsigned char buf[SPI_MSG_SIZE];

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_irq_work_func");
	mutex_lock(&ps_stml0xx->lock);

	stml0xx_wake(ps_stml0xx);

	if (!stml0xx_g_booted)
		goto EXIT;

	if (ps_stml0xx->is_suspended)
		goto EXIT;

	err = stml0xx_spi_read_msg_data(SPI_MSG_TYPE_READ_IRQ_DATA,
					buf,
					SPI_RX_PAYLOAD_LEN,
					RESET_ALLOWED);

	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Reading from stml0xx failed");
		goto EXIT;
	}

	irq_status = (buf[IRQ_IDX_STATUS_HI] << 16) |
	    (buf[IRQ_IDX_STATUS_MED] << 8) | buf[IRQ_IDX_STATUS_LO];

	if (irq_status & M_ACCEL) {
		int head = 0;
		int tail = 0;
		int first_accel_sample = 0;
		uint64_t ts_ns = 0;
		unsigned char *data_buf = 0;
		unsigned char *accel_buf = 0;
		uint8_t delay_ms = 0;

		data_buf = &buf[IRQ_IDX_ACCEL1];

		ts_ns = stm_ws->ts_ns;
		head = data_buf[28];
		tail = data_buf[32];
		for (
			first_accel_sample = 1, accel_buf = data_buf + 7*tail;
			tail != head;
			tail = (tail + 1) % 4, accel_buf = data_buf + 7*tail) {

			delay_ms = accel_buf[6];
			if (first_accel_sample)
				first_accel_sample = 0;
			else
				ts_ns += delay_ms * 1000000LL;

			stml0xx_as_data_buffer_write(
				ps_stml0xx,
				DT_ACCEL,
				accel_buf,
				6,
				0,
				ts_ns);

			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Sending acc(x,y,z)values:x=%d,y=%d,z=%d",
				STM16_TO_HOST(ACCEL_RD_X, accel_buf),
				STM16_TO_HOST(ACCEL_RD_Y, accel_buf),
				STM16_TO_HOST(ACCEL_RD_Z, accel_buf));
		}
	}
	if (irq_status & M_ACCEL2) {
		stml0xx_as_data_buffer_write(ps_stml0xx,
			DT_ACCEL2,
			&buf[IRQ_IDX_ACCEL2],
			6,
			0,
			stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending acc2(x,y,z)values:x=%d,y=%d,z=%d",
			STM16_TO_HOST(ACCEL_RD_X, &buf[IRQ_IDX_ACCEL2]),
			STM16_TO_HOST(ACCEL_RD_Y, &buf[IRQ_IDX_ACCEL2]),
			STM16_TO_HOST(ACCEL_RD_Z, &buf[IRQ_IDX_ACCEL2]));
	}
	if (irq_status & M_ALS) {
		stml0xx_as_data_buffer_write(
			ps_stml0xx,
			DT_ALS,
			&buf[IRQ_IDX_ALS],
			2,
			0,
			stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending ALS %d", STM16_TO_HOST(ALS_VALUE,
					&buf[IRQ_IDX_ALS]));
	}
	if (irq_status & M_DISP_ROTATE) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_DISP_ROTATE,
					&buf[IRQ_IDX_DISP_ROTATE],
					1, 0, stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending disp_rotate(x)value: %d",
			buf[IRQ_IDX_DISP_ROTATE]);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		stml0xx_as_data_buffer_write(ps_stml0xx, DT_DISP_BRIGHT,
						&buf[IRQ_IDX_DISP_BRIGHTNESS],
						 1, 0, stm_ws->ts_ns);

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending Display Brightness %d",
			buf[IRQ_IDX_DISP_BRIGHTNESS]);
	}
EXIT:
	kfree((void *)stm_ws);
	stml0xx_sleep(ps_stml0xx);
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_stml0xx->lock);
}
