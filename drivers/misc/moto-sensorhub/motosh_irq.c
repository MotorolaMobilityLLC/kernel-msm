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

irqreturn_t motosh_isr(int irq, void *dev)
{
	struct motosh_data *ps_motosh = dev;

	if (motosh_irq_disable)
		return IRQ_HANDLED;

	queue_work(ps_motosh->irq_work_queue, &ps_motosh->irq_work);
	if (ps_motosh->irq_wake == -1)
		queue_work(ps_motosh->irq_work_queue,
			&ps_motosh->irq_wake_work);
	return IRQ_HANDLED;
}

void motosh_irq_work_func(struct work_struct *work)
{
	int err;
	u32 irq_status;
	struct motosh_data *ps_motosh = container_of(work,
			struct motosh_data, irq_work);

	dev_dbg(&ps_motosh->client->dev, "motosh_irq_work_func\n");
	mutex_lock(&ps_motosh->lock);

	motosh_wake(ps_motosh);

	if (ps_motosh->mode == BOOTMODE)
		goto EXIT;

	if (ps_motosh->is_suspended)
		goto EXIT;

	/* read interrupt mask register */
	motosh_cmdbuff[0] = INTERRUPT_STATUS;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 3);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading from motosh failed\n");
		goto EXIT;
	}
	irq_status = (motosh_readbuff[IRQ_NOWAKE_HI] << 16) |
		(motosh_readbuff[IRQ_NOWAKE_MED] << 8) |
		motosh_readbuff[IRQ_NOWAKE_LO];

	if (irq_status & M_ACCEL) {
		/* read accelerometer values from MOTOSH */
		motosh_cmdbuff[0] = ACCEL_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Accel from motosh failed\n");
			goto EXIT;
		}

		motosh_as_data_buffer_write(ps_motosh, DT_ACCEL,
			motosh_readbuff, 6, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X), STM16_TO_HOST(ACCEL_RD_Y),
			STM16_TO_HOST(ACCEL_RD_Z));
	}
	if (irq_status & M_LIN_ACCEL) {
		dev_err(&ps_motosh->client->dev,
			"Invalid M_LIN_ACCEL bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read linear accelerometer values from MOTOSH */
		motosh_cmdbuff[0] = LIN_ACCEL_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Linear Accel from motosh failed\n");
			goto EXIT;
		}

		motosh_as_data_buffer_write(ps_motosh, DT_LIN_ACCEL,
					motosh_readbuff, 6, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending lin_acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X), STM16_TO_HOST(ACCEL_RD_Y),
			STM16_TO_HOST(ACCEL_RD_Z));
	}
	if (irq_status & M_ECOMPASS) {
		unsigned char status;
		/*Read orientation values */
		motosh_cmdbuff[0] = MAG_HX;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 13);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev, "Reading Ecompass failed\n");
			goto EXIT;
		}
		status = motosh_readbuff[COMPASS_STATUS];
		motosh_as_data_buffer_write(ps_motosh, DT_MAG,
			motosh_readbuff, 6, status);

		dev_dbg(&ps_motosh->client->dev,
			"Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(MAG_X), STM16_TO_HOST(MAG_Y),
			STM16_TO_HOST(MAG_Z));

		motosh_as_data_buffer_write(ps_motosh, DT_ORIENT,
					    motosh_readbuff + 6, 6, status);

		dev_dbg(&ps_motosh->client->dev,
			"Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ORIENT_X), STM16_TO_HOST(ORIENT_Y),
			STM16_TO_HOST(ORIENT_Z));
	}
	if (irq_status & M_GYRO) {
		motosh_cmdbuff[0] = GYRO_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_GYRO,
			motosh_readbuff, 6, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GYRO_RD_X), STM16_TO_HOST(GYRO_RD_Y),
			STM16_TO_HOST(GYRO_RD_Z));
	}
	/*MODIFIED UNCALIBRATED GYRO*/
	if (irq_status & M_UNCALIB_GYRO) {
		motosh_cmdbuff[0] = UNCALIB_GYRO_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 12);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_UNCALIB_GYRO,
			motosh_readbuff, 12, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(GYRO_RD_X), STM16_TO_HOST(GYRO_RD_Y),
			STM16_TO_HOST(GYRO_RD_Z), STM16_TO_HOST(GYRO_UNCALIB_X),
			STM16_TO_HOST(GYRO_UNCALIB_Y),
			STM16_TO_HOST(GYRO_UNCALIB_Z));
	}
	if (irq_status & M_UNCALIB_MAG) {
		motosh_cmdbuff[0] = UNCALIB_MAG_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 12);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}

		motosh_as_data_buffer_write(ps_motosh, DT_UNCALIB_MAG,
			motosh_readbuff, 12, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(MAG_X), STM16_TO_HOST(MAG_Y),
			STM16_TO_HOST(MAG_Z), STM16_TO_HOST(MAG_UNCALIB_X),
			STM16_TO_HOST(MAG_UNCALIB_Y),
			STM16_TO_HOST(MAG_UNCALIB_Z));
	}
	if (irq_status & M_STEP_COUNTER) {
		motosh_cmdbuff[0] = STEP_COUNTER;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 8);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading step counter failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_STEP_COUNTER,
			motosh_readbuff, 8, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending step counter %X %X %X %X\n",
			STM16_TO_HOST(STEP64_DATA), STM16_TO_HOST(STEP32_DATA),
			STM16_TO_HOST(STEP16_DATA), STM16_TO_HOST(STEP8_DATA));
	}
	if (irq_status & M_STEP_DETECTOR) {
		unsigned short detected_steps = 0;
		motosh_cmdbuff[0] = STEP_DETECTOR;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading step detector  failed\n");
			goto EXIT;
		}
		detected_steps = motosh_readbuff[STEP_DETECT];
		while (detected_steps-- != 0) {
			motosh_as_data_buffer_write(ps_motosh, DT_STEP_DETECTOR,
				motosh_readbuff, 1, 0);

			dev_dbg(&ps_motosh->client->dev,
				"Sending step detector\n");
		}
	}
	if (irq_status & M_ALS) {
		motosh_cmdbuff[0] = ALS_LUX;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading ALS from motosh failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_ALS,
			motosh_readbuff, 2, 0);

		dev_dbg(&ps_motosh->client->dev, "Sending ALS %d\n",
			STM16_TO_HOST(ALS_VALUE));
	}
	if (irq_status & M_TEMPERATURE) {
		/*Read temperature value */
		motosh_cmdbuff[0] = TEMPERATURE_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Temperature failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_TEMP,
					    motosh_readbuff, 2, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending temp(x)value:%d\n", STM16_TO_HOST(TEMP_VALUE));
	}
	if (irq_status & M_PRESSURE) {
		dev_err(&ps_motosh->client->dev,
			"Invalid M_PRESSURE bit set. irq_status = 0x%06x\n",
			irq_status);

		/*Read pressure value */
		motosh_cmdbuff[0] = CURRENT_PRESSURE;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 4);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Pressure failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_PRESSURE,
			motosh_readbuff, 4, 0);

		dev_dbg(&ps_motosh->client->dev, "Sending pressure %d\n",
			STM32_TO_HOST(PRESSURE_VALUE));
	}
	if (irq_status & M_GRAVITY) {
		dev_err(&ps_motosh->client->dev,
			"Invalid M_GRAVITY bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read gravity values from MOTOSH */
		motosh_cmdbuff[0] = GRAVITY_X;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Gravity from motosh failed\n");
			goto EXIT;
		}

		motosh_as_data_buffer_write(ps_motosh, DT_GRAVITY,
			motosh_readbuff, 6, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending gravity(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GRAV_X), STM16_TO_HOST(GRAV_Y),
			STM16_TO_HOST(GRAV_Z));
	}
	if (irq_status & M_DISP_ROTATE) {
		/*Read Display Rotate value */
		motosh_cmdbuff[0] = DISP_ROTATE_DATA;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading disp_rotate failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_DISP_ROTATE,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending disp_rotate(x)value: %d\n",
			motosh_readbuff[DISP_VALUE]);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		motosh_cmdbuff[0] = DISPLAY_BRIGHTNESS;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading Display Brightness failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_DISP_BRIGHT,
			motosh_readbuff, 1, 0);

		dev_dbg(&ps_motosh->client->dev,
			"Sending Display Brightness %d\n",
			motosh_readbuff[DISP_VALUE]);
	}
	if (irq_status & M_IR_GESTURE) {
		err = motosh_process_ir_gesture(ps_motosh);
		if (err < 0)
			goto EXIT;
	}
	if (irq_status & M_IR_RAW) {
		motosh_cmdbuff[0] = IR_RAW;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			MOTOSH_IR_SZ_RAW);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev, "Reading IR data failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_IR_RAW,
			motosh_readbuff, MOTOSH_IR_SZ_RAW, 0);
		dev_dbg(&ps_motosh->client->dev, "Sending raw IR data\n");
	}
	if (irq_status & M_IR_OBJECT) {
		motosh_cmdbuff[0] = IR_STATE;
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev, "Reading IR object state failed\n");
			goto EXIT;
		}
		motosh_as_data_buffer_write(ps_motosh, DT_IR_OBJECT,
			motosh_readbuff, 1, 0);
		dev_dbg(&ps_motosh->client->dev, "Sending IR object state: 0x%x\n",
			motosh_readbuff[IR_STATE_STATE]);
	}

EXIT:
	motosh_sleep(ps_motosh);
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_motosh->lock);
}

int motosh_process_ir_gesture(struct motosh_data *ps_motosh)
{
	int i, err;

	motosh_cmdbuff[0] = IR_GESTURE;
	err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
				MOTOSH_IR_SZ_GESTURE * MOTOSH_IR_GESTURE_CNT);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev, "Reading IR gesture failed\n");
		return err;
	}
	for (i = 0; i < MOTOSH_IR_GESTURE_CNT; i++) {
		int ofs = i * MOTOSH_IR_SZ_GESTURE;
		if (motosh_readbuff[ofs + IR_GESTURE_EVENT] == 0)
			continue;
		motosh_as_data_buffer_write(ps_motosh, DT_IR_GESTURE,
					motosh_readbuff + ofs,
					MOTOSH_IR_SZ_GESTURE, 0);
		dev_dbg(&ps_motosh->client->dev, "Send IR Gesture %d\n",
			motosh_readbuff[ofs + IR_GESTURE_ID]);
	}
	return 0;
}
