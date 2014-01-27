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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
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


irqreturn_t stm401_isr(int irq, void *dev)
{
	struct stm401_data *ps_stm401 = dev;

	if (stm401_irq_disable) {
		disable_irq_wake(ps_stm401->irq);
		return IRQ_HANDLED;
	}

	queue_work(ps_stm401->irq_work_queue, &ps_stm401->irq_work);
	if (ps_stm401->irq_wake == -1)
		queue_work(ps_stm401->irq_work_queue,
			&ps_stm401->irq_wake_work);
	return IRQ_HANDLED;
}

void stm401_irq_work_func(struct work_struct *work)
{
	int err;
	unsigned short irq_status;
	signed short x, y, z;
	struct stm401_data *ps_stm401 = container_of(work,
			struct stm401_data, irq_work);

	dev_dbg(&ps_stm401->client->dev, "stm401_irq_work_func\n");
	mutex_lock(&ps_stm401->lock);

	/* read interrupt mask register */
	stm401_cmdbuff[0] = INTERRUPT_STATUS;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev,
			"Reading from stm401 failed\n");
		goto EXIT;
	}
	irq_status = (stm401_readbuff[1] << 8) | stm401_readbuff[0];

	if (irq_status & M_ACCEL) {
		/* read accelerometer values from STM401 */
		stm401_cmdbuff[0] = ACCEL_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Accel from stm401 failed\n");
			goto EXIT;
		}

		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		z = (stm401_readbuff[4] << 8) | stm401_readbuff[5];
		stm401_as_data_buffer_write(ps_stm401, DT_ACCEL, x, y, z, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_LIN_ACCEL) {
		/* read linear accelerometer values from STM401 */
		stm401_cmdbuff[0] = LIN_ACCEL_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Linear Accel from stm401 failed\n");
			goto EXIT;
		}

		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		z = (stm401_readbuff[4] << 8) | stm401_readbuff[5];
		stm401_as_data_buffer_write(ps_stm401, DT_LIN_ACCEL,
			x, y, z, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending lin_acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_ECOMPASS) {
		/*Read orientation values */
		stm401_cmdbuff[0] = MAG_HX;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 13);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading Ecompass failed\n");
			goto EXIT;
		}

		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		z = (stm401_readbuff[4] << 8) | stm401_readbuff[5];
		stm401_as_data_buffer_write(ps_stm401, DT_MAG, x, y, z,
			stm401_readbuff[12]);

		dev_dbg(&ps_stm401->client->dev,
			"Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);

		x = (stm401_readbuff[6] << 8) | stm401_readbuff[7];
		y = (stm401_readbuff[8] << 8) | stm401_readbuff[9];
		/* roll value needs to be negated */
		z = -(stm401_readbuff[10] << 8) | stm401_readbuff[11];
		stm401_as_data_buffer_write(ps_stm401, DT_ORIENT, x, y, z,
			stm401_readbuff[12]);

		dev_dbg(&ps_stm401->client->dev,
			"Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
		       x, y, z);
	}
	if (irq_status & M_GYRO) {
		stm401_cmdbuff[0] = GYRO_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		z = (stm401_readbuff[4] << 8) | stm401_readbuff[5];
		stm401_as_data_buffer_write(ps_stm401, DT_GYRO, x, y, z, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_ALS) {
		stm401_cmdbuff[0] = ALS_LUX;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading ALS from stm401 failed\n");
			goto EXIT;
		}
		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		stm401_as_data_buffer_write(ps_stm401, DT_ALS, x, 0, 0, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending ALS %d\n", x);
	}
	if (irq_status & M_TEMPERATURE) {
		/*Read temperature value */
		stm401_cmdbuff[0] = TEMPERATURE_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Temperature failed\n");
			goto EXIT;
		}
		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		stm401_as_data_buffer_write(ps_stm401, DT_TEMP, x, 0, 0, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending temp(x)value: %d\n", x);
	}
	if (irq_status & M_PRESSURE) {
		/*Read pressure value */
		stm401_cmdbuff[0] = CURRENT_PRESSURE;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 4);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Pressure failed\n");
			goto EXIT;
		}
		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		stm401_as_data_buffer_write(ps_stm401, DT_PRESSURE, x, y, 0, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending pressure %d\n",
			(x << 16) | (y & 0xFFFF));
	}
	if (irq_status & M_GRAVITY) {
		/* read gravity values from STM401 */
		stm401_cmdbuff[0] = GRAVITY_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gravity from stm401 failed\n");
			goto EXIT;
		}

		x = (stm401_readbuff[0] << 8) | stm401_readbuff[1];
		y = (stm401_readbuff[2] << 8) | stm401_readbuff[3];
		z = (stm401_readbuff[4] << 8) | stm401_readbuff[5];
		stm401_as_data_buffer_write(ps_stm401, DT_GRAVITY,
			x, y, z, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gravity(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_DISP_ROTATE) {
		/*Read Display Rotate value */
		stm401_cmdbuff[0] = DISP_ROTATE_DATA;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading disp_rotate failed\n");
			goto EXIT;
		}
		x = stm401_readbuff[0];
		stm401_as_data_buffer_write(ps_stm401,
			DT_DISP_ROTATE, x, 0, 0, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending disp_rotate(x)value: %d\n", x);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		stm401_cmdbuff[0] = DISPLAY_BRIGHTNESS;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Display Brightness failed\n");
			goto EXIT;
		}
		x = stm401_readbuff[0];
		stm401_as_data_buffer_write(ps_stm401, DT_DISP_BRIGHT,
			x, 0, 0, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Display Brightness %d\n", x);
	}

EXIT:
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_stm401->lock);
}
