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
	u32 irq_status;
	struct stm401_data *ps_stm401 = container_of(work,
			struct stm401_data, irq_work);
	unsigned char cmdbuff[STM401_MAXDATA_LENGTH];
	unsigned char readbuff[STM401_MAXDATA_LENGTH];

	dev_dbg(&ps_stm401->client->dev, "stm401_irq_work_func\n");
	mutex_lock(&ps_stm401->lock);

	stm401_wake(ps_stm401);

	if (ps_stm401->mode == BOOTMODE)
		goto EXIT;

	if (ps_stm401->is_suspended)
		goto EXIT;

	/* read interrupt mask register */
	cmdbuff[0] = INTERRUPT_STATUS;
	err = stm401_i2c_write_read(
		ps_stm401,
		cmdbuff,
		readbuff,
		1, 3);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev,
			"Reading from stm401 failed\n");
		goto EXIT;
	}
	irq_status = (readbuff[IRQ_NOWAKE_HI] << 16) |
		(readbuff[IRQ_NOWAKE_MED] << 8) |
		readbuff[IRQ_NOWAKE_LO];

	if (irq_status & M_ACCEL) {
		/* read accelerometer values from STM401 */
		cmdbuff[0] = ACCEL_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Accel from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_ACCEL,
			readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X, readbuff),
			STM16_TO_HOST(ACCEL_RD_Y, readbuff),
			STM16_TO_HOST(ACCEL_RD_Z, readbuff));
	}
	if (irq_status & M_LIN_ACCEL) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_LIN_ACCEL bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read linear accelerometer values from STM401 */
		cmdbuff[0] = LIN_ACCEL_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Linear Accel from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_LIN_ACCEL,
					readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending lin_acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X, readbuff),
			STM16_TO_HOST(ACCEL_RD_Y, readbuff),
			STM16_TO_HOST(ACCEL_RD_Z, readbuff));
	}
	if (irq_status & M_ECOMPASS) {
		unsigned char status;
		/*Read orientation values */
		cmdbuff[0] = MAG_HX;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 13);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading Ecompass failed\n");
			goto EXIT;
		}
		status = readbuff[COMPASS_STATUS];
		stm401_as_data_buffer_write(ps_stm401, DT_MAG,
			readbuff, 6, status);

		dev_dbg(&ps_stm401->client->dev,
			"Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(MAG_X, readbuff),
			STM16_TO_HOST(MAG_Y, readbuff),
			STM16_TO_HOST(MAG_Z, readbuff));

		stm401_as_data_buffer_write(ps_stm401, DT_ORIENT,
					    readbuff + 6, 6, status);

		dev_dbg(&ps_stm401->client->dev,
			"Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ORIENT_X, readbuff),
			STM16_TO_HOST(ORIENT_Y, readbuff),
			STM16_TO_HOST(ORIENT_Z, readbuff));
	}
	if (irq_status & M_GYRO) {
		cmdbuff[0] = GYRO_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_GYRO,
			readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GYRO_RD_X, readbuff),
			STM16_TO_HOST(GYRO_RD_Y, readbuff),
			STM16_TO_HOST(GYRO_RD_Z, readbuff));
	}
	/*MODIFIED UNCALIBRATED GYRO*/
	if (irq_status & M_UNCALIB_GYRO) {
		cmdbuff[0] = UNCALIB_GYRO_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 12);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_UNCALIB_GYRO,
			readbuff, 12, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(GYRO_RD_X, readbuff),
			STM16_TO_HOST(GYRO_RD_Y, readbuff),
			STM16_TO_HOST(GYRO_RD_Z, readbuff),
			STM16_TO_HOST(GYRO_UNCALIB_X, readbuff),
			STM16_TO_HOST(GYRO_UNCALIB_Y, readbuff),
			STM16_TO_HOST(GYRO_UNCALIB_Z, readbuff));
	}
	if (irq_status & M_UNCALIB_MAG) {
		cmdbuff[0] = UNCALIB_MAG_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 12);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_UNCALIB_MAG,
			readbuff, 12, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(MAG_X, readbuff),
			STM16_TO_HOST(MAG_Y, readbuff),
			STM16_TO_HOST(MAG_Z, readbuff),
			STM16_TO_HOST(MAG_UNCALIB_X, readbuff),
			STM16_TO_HOST(MAG_UNCALIB_Y, readbuff),
			STM16_TO_HOST(MAG_UNCALIB_Z, readbuff));
	}
	if (irq_status & (M_QUAT_6AXIS | M_QUAT_9AXIS)) {
		cmdbuff[0] = QUATERNION_DATA;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 16);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading quaternions failed\n");
			goto EXIT;
		}

		if (irq_status & M_QUAT_6AXIS) {
			stm401_as_data_buffer_write(
				ps_stm401,
				DT_QUAT_6AXIS,
				readbuff,
				8,
				0
			);

			dev_dbg(
				&ps_stm401->client->dev,
				"Sending 6-axis quat values:%d,%d,%d,%d\n",
				STM16_TO_HOST(QUAT_6AXIS_A, readbuff),
				STM16_TO_HOST(QUAT_6AXIS_B, readbuff),
				STM16_TO_HOST(QUAT_6AXIS_C, readbuff),
				STM16_TO_HOST(QUAT_6AXIS_W, readbuff)
			);
		}

		if (irq_status & M_QUAT_9AXIS) {
			stm401_as_data_buffer_write(
				ps_stm401,
				DT_QUAT_9AXIS,
				readbuff+8,
				8,
				0
			);

			dev_dbg(
				&ps_stm401->client->dev,
				"Sending 9-axis quat values:%d,%d,%d,%d\n",
				STM16_TO_HOST(QUAT_9AXIS_A, readbuff),
				STM16_TO_HOST(QUAT_9AXIS_B, readbuff),
				STM16_TO_HOST(QUAT_9AXIS_C, readbuff),
				STM16_TO_HOST(QUAT_9AXIS_W, readbuff)
			);
		}
	}
	if (irq_status & M_STEP_COUNTER) {
		cmdbuff[0] = STEP_COUNTER;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 8);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading step counter failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_STEP_COUNTER,
			readbuff, 8, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending step counter %X %X %X %X\n",
			STM16_TO_HOST(STEP64_DATA, readbuff),
			STM16_TO_HOST(STEP32_DATA, readbuff),
			STM16_TO_HOST(STEP16_DATA, readbuff),
			STM16_TO_HOST(STEP8_DATA, readbuff));
	}
	if (irq_status & M_STEP_DETECTOR) {
		unsigned short detected_steps = 0;
		cmdbuff[0] = STEP_DETECTOR;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading step detector  failed\n");
			goto EXIT;
		}
		detected_steps = readbuff[STEP_DETECT];
		while (detected_steps-- != 0) {
			stm401_as_data_buffer_write(ps_stm401, DT_STEP_DETECTOR,
				readbuff, 1, 0);

			dev_dbg(&ps_stm401->client->dev,
				"Sending step detector\n");
		}
	}
	if (irq_status & M_ALS) {
		cmdbuff[0] = ALS_LUX;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading ALS from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_ALS,
			readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending ALS %d\n",
			STM16_TO_HOST(ALS_VALUE, readbuff));
	}
	if (irq_status & M_TEMPERATURE) {
		/*Read temperature value */
		cmdbuff[0] = TEMPERATURE_DATA;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Temperature failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_TEMP,
					    readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending temp(x)value:%d\n",
			STM16_TO_HOST(TEMP_VALUE, readbuff));
	}
	if (irq_status & M_PRESSURE) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_PRESSURE bit set. irq_status = 0x%06x\n",
			irq_status);

		/*Read pressure value */
		cmdbuff[0] = CURRENT_PRESSURE;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 4);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Pressure failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_PRESSURE,
			readbuff, 4, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending pressure %d\n",
			STM32_TO_HOST(PRESSURE_VALUE, readbuff));
	}
	if (irq_status & M_GRAVITY) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_GRAVITY bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read gravity values from STM401 */
		cmdbuff[0] = GRAVITY_X;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gravity from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_GRAVITY,
			readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gravity(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GRAV_X, readbuff),
			STM16_TO_HOST(GRAV_Y, readbuff),
			STM16_TO_HOST(GRAV_Z, readbuff));
	}
	if (irq_status & M_DISP_ROTATE) {
		/*Read Display Rotate value */
		cmdbuff[0] = DISP_ROTATE_DATA;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading disp_rotate failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_DISP_ROTATE,
			readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending disp_rotate(x)value: %d\n",
			readbuff[DISP_VALUE]);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		cmdbuff[0] = DISPLAY_BRIGHTNESS;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Display Brightness failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_DISP_BRIGHT,
			readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Display Brightness %d\n",
			readbuff[DISP_VALUE]);
	}
	if (irq_status & M_IR_GESTURE) {
		err = stm401_process_ir_gesture(ps_stm401);
		if (err < 0)
			goto EXIT;
	}
	if (irq_status & M_IR_RAW) {
		cmdbuff[0] = IR_RAW;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1,
			STM401_IR_SZ_RAW);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading IR data failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_IR_RAW,
			readbuff, STM401_IR_SZ_RAW, 0);
		dev_dbg(&ps_stm401->client->dev, "Sending raw IR data\n");
	}
	if (irq_status & M_IR_OBJECT) {
		cmdbuff[0] = IR_STATE;
		err = stm401_i2c_write_read(
			ps_stm401,
			cmdbuff,
			readbuff,
			1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading IR object state failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_IR_OBJECT,
			readbuff, 1, 0);
		dev_dbg(&ps_stm401->client->dev, "Sending IR object state: 0x%x\n",
			readbuff[IR_STATE_STATE]);
	}

EXIT:
	stm401_sleep(ps_stm401);
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_stm401->lock);
}

int stm401_process_ir_gesture(struct stm401_data *ps_stm401)
{
	int i, err;
	unsigned char cmdbuff[1];
	unsigned char readbuff[STM401_MAXDATA_LENGTH];

	cmdbuff[0] = IR_GESTURE;
	err = stm401_i2c_write_read(
		ps_stm401,
		cmdbuff,
		readbuff,
		1,
		STM401_IR_SZ_GESTURE * STM401_IR_GESTURE_CNT);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading IR gesture failed\n");
		return err;
	}
	for (i = 0; i < STM401_IR_GESTURE_CNT; i++) {
		int ofs = i * STM401_IR_SZ_GESTURE;
		if (readbuff[ofs + IR_GESTURE_EVENT] == 0)
			continue;
		stm401_as_data_buffer_write(ps_stm401, DT_IR_GESTURE,
					readbuff + ofs,
					STM401_IR_SZ_GESTURE, 0);
		dev_dbg(&ps_stm401->client->dev, "Send IR Gesture %d\n",
			readbuff[ofs + IR_GESTURE_ID]);
	}
	return 0;
}
