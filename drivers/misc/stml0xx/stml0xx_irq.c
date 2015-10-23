/*
 * Copyright (C) 2010-2015 Motorola Mobility LLC
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

#define SH_LPTIM_TICKS_TO_NS(ticks)  ((7813335 * (uint64_t)ticks) >> 8)

/* Timestamp of the last sample in the streaming sensor queue */
static uint64_t last_ts_ns;

irqreturn_t stml0xx_isr(int irq, void *dev)
{
	struct timespec ts;
	struct stml0xx_work_struct *stm_ws;
	struct stml0xx_data *ps_stml0xx = dev;

	if (stml0xx_irq_disable)
		return IRQ_HANDLED;

	get_monotonic_boottime(&ts);

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

	if (last_ts_ns > stm_ws->ts_ns)
		dev_dbg(&ps_stml0xx->spi->dev,
			"warning: timestamp out of order");

	queue_work(ps_stml0xx->irq_work_queue, (struct work_struct *)stm_ws);
	return IRQ_HANDLED;
}

/**
 * stml0xx_process_stream_sensor_queue() - Process sensor data from the
 *		streaming sensor data queue
 * @buf:	SPI buffer containing the streaming sensor data queue
 * @ts_ns:	Timestamp applied to the first sample in the queue
 *
 * Streaming sensor data queue structure (queue_buf[]):
 *
 *   This is a circular buffer, where insert_idx == remove_idx means empty,
 *   and insert_idx ==(remove_idx + N-1) % N means full,
 *   where N = STREAM_SENSOR_QUEUE_DEPTH
 *   (so only N-1 of the N slots are ever filled).
 *
 *   Byte  0   - slot 0: sensor type
 *   Bytes 1~2 - slot 0: number of ticks elapsed since previous sample
 *   Bytes 3~8 - slot 0: 6-byte data
 *   ...
 *   Bytes [n*9]               - slot n: sensor type
 *   Bytes [n*9 + 1]~[n*9 + 2] - slot n: number of ticks elapsed since
 *                                       previous sample
 *   Bytes [n*9 + 3]~[n*9 + 8] - slot n: 6-byte data
 *
 *   Bytes [N*9]               - Insert index (0,1,2,3..N-1)
 *   Bytes [N*9 + 1]~[N*9 + 3] - Nothing (buffer to ensure insert and remove
 *                                        indices are in separate words)
 *   Bytes [N*9 + 4]           - Remove index (0,1,2,3..N-1)
 *
 *   where n = slot (0..N-1), N = STREAM_SENSOR_QUEUE_DEPTH
*/
void stml0xx_process_stream_sensor_queue(char *buf, uint64_t ts_ns)
{
	int i = 0;
	int insert_idx = 0;
	int remove_idx = 0;
	int first_sample = 1;
	unsigned char *sample_buf;
	uint16_t delta_ticks = 0;
	uint8_t sensor_type = 0;
	uint8_t num_samples = 0;
	struct stml0xx_data *ps_stml0xx = stml0xx_misc_data;
	uint32_t irq_status = (buf[IRQ_IDX_STATUS_HI] << 16) |
		(buf[IRQ_IDX_STATUS_MED] << 8) | buf[IRQ_IDX_STATUS_LO];
	unsigned char *queue_buf = &buf[IRQ_IDX_STREAM_SENSOR_QUEUE];

	insert_idx = queue_buf[STREAM_SENSOR_QUEUE_INSERT_IDX];
	remove_idx = queue_buf[STREAM_SENSOR_QUEUE_REMOVE_IDX];

	if (insert_idx >= remove_idx)
		num_samples = insert_idx - remove_idx;
	else
		num_samples = insert_idx + STREAM_SENSOR_QUEUE_DEPTH
				- remove_idx;

	if (num_samples >= 2)
		dev_dbg(&stml0xx_misc_data->spi->dev, "Samples in Queue: %d",
			num_samples);

#if ENABLE_VERBOSE_LOGGING
	for (i = 0; i < STREAM_SENSOR_QUEUE_DEPTH *
			STREAM_SENSOR_QUEUE_ENTRY_SIZE; i++) {
		if (i % STREAM_SENSOR_QUEUE_ENTRY_SIZE == 0) {
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Queue buf[%d] - 0x%02x - slot [%d]",
				i, queue_buf[i],
				i / STREAM_SENSOR_QUEUE_ENTRY_SIZE);
		} else {
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Queue buf[%d] - 0x%02x", i, queue_buf[i]);
		}
	}
	dev_dbg(&stml0xx_misc_data->spi->dev,
		"Insert [%d], Remove [%d]", insert_idx, remove_idx);
#else
	uninitialized_var(i);
#endif /* ENABLE_VERBOSE_LOGGING */

	while (remove_idx != insert_idx) {
		sample_buf = queue_buf +
			(STREAM_SENSOR_QUEUE_ENTRY_SIZE * remove_idx);
		sensor_type = sample_buf[SENSOR_TYPE_IDX];
		delta_ticks = SH_TO_H16(sample_buf + DELTA_TICKS_IDX);
		if (first_sample)
			first_sample = 0;
		else
			ts_ns += SH_LPTIM_TICKS_TO_NS(delta_ticks);

		last_ts_ns = ts_ns;

		switch (sensor_type) {
		case STREAM_SENSOR_TYPE_ACCEL1:
			stml0xx_as_data_buffer_write(ps_stml0xx,
				DT_ACCEL, &sample_buf[SENSOR_X_IDX],
				SENSOR_DATA_SIZE, 0, ts_ns);
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Sending acc(x,y,z)values:x=%d,y=%d,z=%d",
				SH_TO_H16(sample_buf + SENSOR_X_IDX),
				SH_TO_H16(sample_buf + SENSOR_Y_IDX),
				SH_TO_H16(sample_buf + SENSOR_Z_IDX));
			break;
		case STREAM_SENSOR_TYPE_ACCEL2:
			stml0xx_as_data_buffer_write(ps_stml0xx,
				DT_ACCEL2, &sample_buf[SENSOR_X_IDX],
				SENSOR_DATA_SIZE, 0, ts_ns);
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Sending acc2(x,y,z)values:x=%d,y=%d,z=%d",
				SH_TO_H16(sample_buf + SENSOR_X_IDX),
				SH_TO_H16(sample_buf + SENSOR_Y_IDX),
				SH_TO_H16(sample_buf + SENSOR_Z_IDX));
			break;
		case STREAM_SENSOR_TYPE_UNCAL_GYRO:
			if (irq_status & M_UNCALIB_GYRO) {
				char uncal_gyro_buf[UNCALIB_GYRO_DATA_SIZE];

				memcpy(uncal_gyro_buf,
					sample_buf + SENSOR_X_IDX,
					SENSOR_DATA_SIZE);
				memcpy(uncal_gyro_buf + SENSOR_DATA_SIZE,
					buf + IRQ_IDX_GYRO_CAL_X,
					SENSOR_DATA_SIZE);
				stml0xx_as_data_buffer_write(ps_stml0xx,
					DT_UNCALIB_GYRO,
					uncal_gyro_buf,
					UNCALIB_GYRO_DATA_SIZE, 0, ts_ns);
				dev_dbg(&stml0xx_misc_data->spi->dev,
				  "Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d",
					SH_TO_H16(sample_buf + SENSOR_X_IDX),
					SH_TO_H16(sample_buf + SENSOR_Y_IDX),
					SH_TO_H16(sample_buf + SENSOR_Z_IDX),
					SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_X),
					SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_Y),
					SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_Z));
			}
			if  (irq_status & M_GYRO) {
				int16_t gyro_x, gyro_y, gyro_z;

				/* Apply bias to uncal gyro data */
				gyro_x = SH_TO_H16(sample_buf + SENSOR_X_IDX)
					- SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_X);
				gyro_y = SH_TO_H16(sample_buf + SENSOR_Y_IDX)
					- SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_Y);
				gyro_z = SH_TO_H16(sample_buf + SENSOR_Z_IDX)
					- SH_TO_H16(buf + IRQ_IDX_GYRO_CAL_Z);

				/* Copy results back into buf */
				sample_buf[SENSOR_X_IDX]   = gyro_x >> 8;
				sample_buf[SENSOR_X_IDX+1] = gyro_x & 0x00ff;
				sample_buf[SENSOR_Y_IDX]   = gyro_y >> 8;
				sample_buf[SENSOR_Y_IDX+1] = gyro_y & 0x00ff;
				sample_buf[SENSOR_Z_IDX]   = gyro_z >> 8;
				sample_buf[SENSOR_Z_IDX+1] = gyro_z & 0x00ff;
				stml0xx_as_data_buffer_write(ps_stml0xx,
					DT_GYRO, &sample_buf[SENSOR_X_IDX],
					SENSOR_DATA_SIZE, 0, ts_ns);
				dev_dbg(&stml0xx_misc_data->spi->dev,
				   "Sending gyro(x,y,z)values:x=%d,y=%d,z=%d",
					SH_TO_H16(sample_buf + SENSOR_X_IDX),
					SH_TO_H16(sample_buf + SENSOR_Y_IDX),
					SH_TO_H16(sample_buf + SENSOR_Z_IDX));
			}
			break;
		default:
			dev_err(&stml0xx_misc_data->spi->dev,
				"Unknown sensor type [%d]", sensor_type);
			break;
		}

		remove_idx = (remove_idx + 1) % STREAM_SENSOR_QUEUE_DEPTH;
	}
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

	/* Check for streaming sensors */
	if (irq_status & (M_ACCEL | M_ACCEL2 | M_GYRO | M_GYRO)) {
		stml0xx_process_stream_sensor_queue(buf, stm_ws->ts_ns);
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
			"Sending ALS %d", SH_TO_H16(buf + IRQ_IDX_ALS));
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
