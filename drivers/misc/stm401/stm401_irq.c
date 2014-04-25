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

	dev_dbg(&ps_stm401->client->dev, "stm401_irq_work_func\n");
	mutex_lock(&ps_stm401->lock);

	stm401_wake(ps_stm401);

	if (ps_stm401->mode == BOOTMODE)
		goto EXIT;

	if (ps_stm401->is_suspended)
		goto EXIT;

	/* read interrupt mask register */
	stm401_cmdbuff[0] = INTERRUPT_STATUS;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 3);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev,
			"Reading from stm401 failed\n");
		goto EXIT;
	}
	irq_status = (stm401_readbuff[IRQ_NOWAKE_HI] << 16) |
		(stm401_readbuff[IRQ_NOWAKE_MED] << 8) |
		stm401_readbuff[IRQ_NOWAKE_LO];

	if (irq_status & M_ACCEL) {
		/* read accelerometer values from STM401 */
		stm401_cmdbuff[0] = ACCEL_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Accel from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_ACCEL,
			stm401_readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X), STM16_TO_HOST(ACCEL_RD_Y),
			STM16_TO_HOST(ACCEL_RD_Z));
	}
	if (irq_status & M_LIN_ACCEL) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_LIN_ACCEL bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read linear accelerometer values from STM401 */
		stm401_cmdbuff[0] = LIN_ACCEL_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Linear Accel from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_LIN_ACCEL,
					stm401_readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending lin_acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ACCEL_RD_X), STM16_TO_HOST(ACCEL_RD_Y),
			STM16_TO_HOST(ACCEL_RD_Z));
	}
	if (irq_status & M_ECOMPASS) {
		unsigned char status;
		/*Read orientation values */
		stm401_cmdbuff[0] = MAG_HX;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 13);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading Ecompass failed\n");
			goto EXIT;
		}
		status = stm401_readbuff[COMPASS_STATUS];
		stm401_as_data_buffer_write(ps_stm401, DT_MAG,
			stm401_readbuff, 6, status);

		dev_dbg(&ps_stm401->client->dev,
			"Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(MAG_X), STM16_TO_HOST(MAG_Y),
			STM16_TO_HOST(MAG_Z));

		stm401_as_data_buffer_write(ps_stm401, DT_ORIENT,
					    stm401_readbuff + 6, 6, status);

		dev_dbg(&ps_stm401->client->dev,
			"Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(ORIENT_X), STM16_TO_HOST(ORIENT_Y),
			STM16_TO_HOST(ORIENT_Z));
	}
	if (irq_status & M_GYRO) {
		stm401_cmdbuff[0] = GYRO_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_GYRO,
			stm401_readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GYRO_RD_X), STM16_TO_HOST(GYRO_RD_Y),
			STM16_TO_HOST(GYRO_RD_Z));
	}
	/*MODIFIED UNCALIBRATED GYRO*/
	if (irq_status & M_UNCALIB_GYRO) {
		stm401_cmdbuff[0] = UNCALIB_GYRO_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 12);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_UNCALIB_GYRO,
			stm401_readbuff, 12, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(GYRO_RD_X), STM16_TO_HOST(GYRO_RD_Y),
			STM16_TO_HOST(GYRO_RD_Z), STM16_TO_HOST(GYRO_UNCALIB_X),
			STM16_TO_HOST(GYRO_UNCALIB_Y),
			STM16_TO_HOST(GYRO_UNCALIB_Z));
	}
	if (irq_status & M_UNCALIB_MAG) {
		stm401_cmdbuff[0] = UNCALIB_MAG_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 12);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gyroscope failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_UNCALIB_MAG,
			stm401_readbuff, 12, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
			STM16_TO_HOST(MAG_X), STM16_TO_HOST(MAG_Y),
			STM16_TO_HOST(MAG_Z), STM16_TO_HOST(MAG_UNCALIB_X),
			STM16_TO_HOST(MAG_UNCALIB_Y),
			STM16_TO_HOST(MAG_UNCALIB_Z));
	}
	if (irq_status & M_STEP_COUNTER) {
		stm401_cmdbuff[0] = STEP_COUNTER;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 8);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading step counter failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_STEP_COUNTER,
			stm401_readbuff, 8, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending step counter %X %X %X %X\n",
			STM16_TO_HOST(STEP64_DATA), STM16_TO_HOST(STEP32_DATA),
			STM16_TO_HOST(STEP16_DATA), STM16_TO_HOST(STEP8_DATA));
	}
	if (irq_status & M_STEP_DETECTOR) {
		unsigned short detected_steps = 0;
		stm401_cmdbuff[0] = STEP_DETECTOR;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading step detector  failed\n");
			goto EXIT;
		}
		detected_steps = stm401_readbuff[STEP_DETECT];
		while (detected_steps-- != 0) {
			stm401_as_data_buffer_write(ps_stm401, DT_STEP_DETECTOR,
				stm401_readbuff, 1, 0);

			dev_dbg(&ps_stm401->client->dev,
				"Sending step detector\n");
		}
	}
	if (irq_status & M_ALS) {
		stm401_cmdbuff[0] = ALS_LUX;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading ALS from stm401 failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_ALS,
			stm401_readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending ALS %d\n",
			STM16_TO_HOST(ALS_VALUE));
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
		stm401_as_data_buffer_write(ps_stm401, DT_TEMP,
					    stm401_readbuff, 2, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending temp(x)value:%d\n", STM16_TO_HOST(TEMP_VALUE));
	}
	if (irq_status & M_PRESSURE) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_PRESSURE bit set. irq_status = 0x%06x\n",
			irq_status);

		/*Read pressure value */
		stm401_cmdbuff[0] = CURRENT_PRESSURE;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 4);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Pressure failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_PRESSURE,
			stm401_readbuff, 4, 0);

		dev_dbg(&ps_stm401->client->dev, "Sending pressure %d\n",
			STM32_TO_HOST(PRESSURE_VALUE));
	}
	if (irq_status & M_GRAVITY) {
		dev_err(&ps_stm401->client->dev,
			"Invalid M_GRAVITY bit set. irq_status = 0x%06x\n",
			irq_status);

		/* read gravity values from STM401 */
		stm401_cmdbuff[0] = GRAVITY_X;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 6);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Gravity from stm401 failed\n");
			goto EXIT;
		}

		stm401_as_data_buffer_write(ps_stm401, DT_GRAVITY,
			stm401_readbuff, 6, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending gravity(x,y,z)values:x=%d,y=%d,z=%d\n",
			STM16_TO_HOST(GRAV_X), STM16_TO_HOST(GRAV_Y),
			STM16_TO_HOST(GRAV_Z));
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
		stm401_as_data_buffer_write(ps_stm401, DT_DISP_ROTATE,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending disp_rotate(x)value: %d\n",
			stm401_readbuff[DISP_VALUE]);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		stm401_cmdbuff[0] = DISPLAY_BRIGHTNESS;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading Display Brightness failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_DISP_BRIGHT,
			stm401_readbuff, 1, 0);

		dev_dbg(&ps_stm401->client->dev,
			"Sending Display Brightness %d\n",
			stm401_readbuff[DISP_VALUE]);
	}
	if (irq_status & M_IR_GESTURE) {
		err = stm401_process_ir_gesture(ps_stm401);
		if (err < 0)
			goto EXIT;
	}
	if (irq_status & M_IR_RAW) {
		stm401_cmdbuff[0] = IR_RAW;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
			STM401_IR_SZ_RAW);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading IR data failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_IR_RAW,
			stm401_readbuff, STM401_IR_SZ_RAW, 0);
		dev_dbg(&ps_stm401->client->dev, "Sending raw IR data\n");
	}
	if (irq_status & M_IR_OBJECT) {
		stm401_cmdbuff[0] = IR_STATE;
		err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&ps_stm401->client->dev, "Reading IR object state failed\n");
			goto EXIT;
		}
		stm401_as_data_buffer_write(ps_stm401, DT_IR_OBJECT,
			stm401_readbuff, 1, 0);
		dev_dbg(&ps_stm401->client->dev, "Sending IR object state: 0x%x\n",
			stm401_readbuff[IR_STATE_STATE]);
	}

EXIT:
	stm401_sleep(ps_stm401);
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_stm401->lock);
}

int stm401_process_ir_gesture(struct stm401_data *ps_stm401)
{
	int i, err;

	stm401_cmdbuff[0] = IR_GESTURE;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1,
				STM401_IR_SZ_GESTURE * STM401_IR_GESTURE_CNT);
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading IR gesture failed\n");
		return err;
	}
	for (i = 0; i < STM401_IR_GESTURE_CNT; i++) {
		int ofs = i * STM401_IR_SZ_GESTURE;
		if (stm401_readbuff[ofs + IR_GESTURE_EVENT] == 0)
			continue;
		stm401_as_data_buffer_write(ps_stm401, DT_IR_GESTURE,
					stm401_readbuff + ofs,
					STM401_IR_SZ_GESTURE, 0);
		dev_dbg(&ps_stm401->client->dev, "Send IR Gesture %d\n",
			stm401_readbuff[ofs + IR_GESTURE_ID]);
	}
	return 0;
}
