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
#include <linux/math64.h>
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
#include <linux/motosh_vmm_defines.h>

static void motosh_irq_vr_handler(struct kthread_work *work);
static void motosh_process_vr_data(struct motosh_data *ps_motosh,
	struct vr_data *vr_data);
static void motosh_irq_handler(struct kthread_work *work);

irqreturn_t motosh_isr(int irq, void *dev)
{
	struct motosh_data *ps_motosh = dev;

	if (motosh_irq_disable || ps_motosh->is_suspended)
		return IRQ_HANDLED;

	queue_kthread_work(&ps_motosh->irq_worker,
		&ps_motosh->irq_work);

	return IRQ_HANDLED;
}

void motosh_irq_thread_func(struct kthread_work *work)
{
	if (motosh_misc_data->vr_mode)
		motosh_irq_vr_handler(work);
	else
		motosh_irq_handler(work);
}

static void motosh_irq_vr_handler(struct kthread_work *work)
{
	static int64_t last_sensor_ts;
	int err;
	int num_samples;
	int sample_count;
	unsigned char cmdbuff;
	struct vr_data read_vr_data;
	struct motosh_data *ps_motosh = container_of(work,
			struct motosh_data, irq_work);

	if (ps_motosh->is_suspended)
		goto EXIT;

	if (ps_motosh->mode <= BOOTMODE)
		goto EXIT;

	if (motosh_misc_data->in_reset_and_init)
		goto EXIT;

	if (ps_motosh->resume_cleanup) {
		ps_motosh->resume_cleanup = false;

		/* If we are just coming back from suspend we will
		   discard any non-batchable sensor data that may have
		   entered the buffer prior to last reporting interval
		   before the AP went to suspend */
		motosh_time_sync();
	}

	/* If we detect that we have fallen behind try and read out the
	 * extra samples that will have been buffered in the sensorhub */
	num_samples = div_s64((motosh_timestamp_ns() - last_sensor_ts),
			MAX_VR_RATE_NS);

	if (num_samples < 1)
		num_samples = 1;
	if (num_samples > VR_BUFFERED_SAMPLES)
		num_samples = VR_BUFFERED_SAMPLES;

	for (sample_count = 0; sample_count < num_samples; ++sample_count) {
		cmdbuff = VR_DATA;
		memset(&read_vr_data, 0, sizeof(read_vr_data));
		err = motosh_i2c_write_read(ps_motosh,
				&cmdbuff, (void *)&read_vr_data,
				sizeof(cmdbuff),
				sizeof(struct vr_data));

		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading vr_data failed [err: %d]\n", err);
			continue;
		}

		if ((read_vr_data.status & VR_READY) != VR_READY)
			continue;

		motosh_process_vr_data(ps_motosh, &read_vr_data);

		if (read_vr_data.status != 0) {
			last_sensor_ts = motosh_time_recover(
				(read_vr_data.timestamp[0] << 16) |
				(read_vr_data.timestamp[1] <<  8) |
				(read_vr_data.timestamp[2]),
				motosh_timestamp_ns());
		}
	}

EXIT:
	/* Latest queue purged from hub. Reply to each pending flush bit
	   that is pending. Currently only accel is batched, others are
	   directly handled at IOCTL
	*/
	if (ps_motosh->nwake_flush_req & FLUSH_ACCEL_REQ) {
		uint32_t handle = ID_A;

		motosh_as_data_buffer_write(ps_motosh, DT_FLUSH,
					    (char *)&handle, 4, 0, NULL);
		dev_dbg(&ps_motosh->client->dev,
			 "flushed accel");
	}
	/* clear any flush requests pending */
	ps_motosh->nwake_flush_req = 0;

	/* last of batched accel processed */
	if (motosh_g_acc_cfg.timeout == 0)
		ps_motosh->is_batching &= (~ACCEL_BATCHING);

	return;
}

static void motosh_process_vr_data(struct motosh_data *ps_motosh,
	struct vr_data *vr_data)
{
	static uint8_t mag_calibrated_status;

	if ((vr_data->status & VR_ACCEL) &&
			(motosh_g_nonwake_sensor_state & M_ACCEL)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_ACCEL,
			(uint8_t *)&vr_data->accel,
			sizeof(vr_data->accel),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_GYRO) &&
			(motosh_g_nonwake_sensor_state & M_GYRO)) {

		struct vr_sensor calibrated_gyro;

		calibrated_gyro.x =
			vr_data->gyro.raw.x -
			vr_data->gyro.cal.x;
		calibrated_gyro.y =
			vr_data->gyro.raw.y -
			vr_data->gyro.cal.y;
		calibrated_gyro.z =
			vr_data->gyro.raw.z -
			vr_data->gyro.cal.z;

		calibrated_gyro.x =
			STM16_TO_HOST(&calibrated_gyro.x, 0);
		calibrated_gyro.y =
			STM16_TO_HOST(&calibrated_gyro.y, 0);
		calibrated_gyro.z =
			STM16_TO_HOST(&calibrated_gyro.z, 0);

		motosh_as_data_buffer_write(ps_motosh,
			DT_GYRO,
			(uint8_t *)&calibrated_gyro,
			sizeof(calibrated_gyro),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_GYRO) &&
			(motosh_g_nonwake_sensor_state & M_UNCALIB_GYRO)) {

		vr_data->gyro.raw.x =
			STM16_TO_HOST(&vr_data->gyro.raw.x, 0);
		vr_data->gyro.raw.y =
			STM16_TO_HOST(&vr_data->gyro.raw.y, 0);
		vr_data->gyro.raw.z =
			STM16_TO_HOST(&vr_data->gyro.raw.z, 0);

		vr_data->gyro.cal.x =
			STM16_TO_HOST(&vr_data->gyro.cal.x, 0);
		vr_data->gyro.cal.y =
			STM16_TO_HOST(&vr_data->gyro.cal.y, 0);
		vr_data->gyro.cal.z =
			STM16_TO_HOST(&vr_data->gyro.cal.z, 0);

		motosh_as_data_buffer_write(ps_motosh,
			DT_UNCALIB_GYRO,
			(uint8_t *)&vr_data->gyro,
			sizeof(vr_data->gyro),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_MAG) &&
			(motosh_g_nonwake_sensor_state & M_ECOMPASS)) {

		struct vr_sensor calibrated_mag;

		calibrated_mag.x =
			vr_data->mag_game_rv_union.mag.raw.x -
			vr_data->mag_game_rv_union.mag.cal.x;
		calibrated_mag.y =
			vr_data->mag_game_rv_union.mag.raw.y -
			vr_data->mag_game_rv_union.mag.cal.y;
		calibrated_mag.z =
			vr_data->mag_game_rv_union.mag.raw.z -
			vr_data->mag_game_rv_union.mag.cal.z;

		mag_calibrated_status = vr_data->
			mag_game_rv_union.mag.calibrated_status;

		calibrated_mag.x =
			STM16_TO_HOST(&calibrated_mag.x, 0);
		calibrated_mag.y =
			STM16_TO_HOST(&calibrated_mag.y, 0);
		calibrated_mag.z =
			STM16_TO_HOST(&calibrated_mag.z, 0);

		motosh_as_data_buffer_write(ps_motosh,
			DT_MAG,
			(uint8_t *)&calibrated_mag,
			sizeof(calibrated_mag),
			mag_calibrated_status,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_MAG) &&
			(motosh_g_nonwake_sensor_state & M_UNCALIB_MAG)) {

		vr_data->mag_game_rv_union.mag.raw.x =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.raw.x, 0);
		vr_data->mag_game_rv_union.mag.raw.y =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.raw.y, 0);
		vr_data->mag_game_rv_union.mag.raw.z =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.raw.z, 0);

		vr_data->mag_game_rv_union.mag.cal.x =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.cal.x, 0);
		vr_data->mag_game_rv_union.mag.cal.y =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.cal.y, 0);
		vr_data->mag_game_rv_union.mag.cal.z =
			STM16_TO_HOST(&vr_data->mag_game_rv_union.
			mag.cal.z, 0);

		motosh_as_data_buffer_write(ps_motosh,
			DT_UNCALIB_MAG,
			(uint8_t *)&vr_data->mag_game_rv_union.mag,
			sizeof(vr_data->mag_game_rv_union.mag),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_ALS) &&
			(motosh_g_nonwake_sensor_state & M_ALS)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_ALS,
			(uint8_t *)&vr_data->als,
			sizeof(vr_data->als),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_DISPLAY_ROTATE) &&
			(motosh_g_nonwake_sensor_state & M_DISP_ROTATE)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_DISP_ROTATE,
			(uint8_t *)&vr_data->display_rotate,
			sizeof(vr_data->display_rotate),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_GRAVITY) &&
			(motosh_g_nonwake_sensor_state & M_GRAVITY)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_GRAVITY,
			(uint8_t *)&vr_data->la_g_union.gravity,
			sizeof(vr_data->la_g_union.gravity),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_LINEAR_ACCEL) &&
			(motosh_g_nonwake_sensor_state & M_LIN_ACCEL)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_LIN_ACCEL,
			(uint8_t *)&vr_data->la_g_union.linear_accel,
			sizeof(vr_data->la_g_union.linear_accel),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_ROTV_9AXIS) &&
			(motosh_g_nonwake_sensor_state & M_QUAT_9AXIS)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_QUAT_9AXIS,
			(uint8_t *)&vr_data->rv_9axis,
			sizeof(vr_data->rv_9axis),
			mag_calibrated_status,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_ROTV_GAME) &&
			(motosh_g_nonwake_sensor_state & M_GAME_RV)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_GAME_RV,
			(uint8_t *)&vr_data->rv_game,
			sizeof(vr_data->rv_game),
			0,
			(uint8_t *)&vr_data->timestamp);
	}

	if ((vr_data->status & VR_ROTV_6AXIS) &&
			(motosh_g_nonwake_sensor_state & M_QUAT_6AXIS)) {

		motosh_as_data_buffer_write(ps_motosh,
			DT_QUAT_6AXIS,
			(uint8_t *)&vr_data->mag_game_rv_union.rv_6axis,
			sizeof(vr_data->mag_game_rv_union.rv_6axis),
			0,
			(uint8_t *)&vr_data->timestamp);
	}
}

static void motosh_irq_handler(struct kthread_work *work)
{
	int err;
	u8 irq_status = 0;
	u16 queue_length = 0;
	u16 read_size;
	u16 queue_index = 0;
	struct motosh_data *ps_motosh = container_of(work,
			struct motosh_data, irq_work);
	/* Short buffers for command and status reads */
	unsigned char cmdbuff[8];
	static unsigned char readbuff[MOTOSH_MAX_NWAKE_EVENT_QUEUE_SIZE];
	u16 prev_message_id;
	u8 resuming = 0;

	mutex_lock(&ps_motosh->lock);

	if (ps_motosh->is_suspended)
		goto EXIT;

	if (ps_motosh->mode <= BOOTMODE)
		goto EXIT;

	if (motosh_misc_data->in_reset_and_init)
		goto EXIT;

	if (ps_motosh->resume_cleanup) {
		ps_motosh->resume_cleanup = false;

		/* If we are just coming back from suspend we will
		   discard any non-batchable sensor data that may have
		   entered the buffer prior to last reporting interval
		   before the AP went to suspend */
		resuming = 1;

		motosh_time_sync();
	}

	/* get nwake status (queue length and irq_status) */
	cmdbuff[0] = NWAKE_STATUS;
	err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1, 3);
	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading nwake status failed [err: %d]\n", err);
		goto EXIT;
	}
	queue_length = (readbuff[0] << 8) | (readbuff[1] & 0xff);
	irq_status = readbuff[2];

	if (queue_length == 0 && irq_status == 0) {
			dev_dbg(&ps_motosh->client->dev,
				"No status or queue\n");
			goto EXIT;
	}

	dev_dbg(&ps_motosh->client->dev,
			"motosh_irq_work_func irq_status:%0X\n", irq_status);

	/* process the irq status */
	if (irq_status & N_DISP_ROTATE) {
		/* read DROTATE */
		cmdbuff[0] = DROTATE;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 1);

		if (err >= 0) {
			motosh_as_data_buffer_write(ps_motosh, DT_DISP_ROTATE,
				readbuff, 1, 0, NULL);

			/* temporarily print this log to help debug any
			 * other display rotate issues */
			dev_info(&ps_motosh->client->dev,
				"Sending disp_rotate(x)value: %d\n",
				readbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Reading DROTATE failed [err: %d]\n",
				err);
		}
	}
	if (irq_status & N_ALS) {
		/* read ALS_LUX */
		cmdbuff[0] = ALS_LUX;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 2);

		if (err >= 0) {
			motosh_as_data_buffer_write(ps_motosh, DT_ALS,
				readbuff, 2, 0, NULL);

			dev_dbg(&ps_motosh->client->dev, "Sending ALS %d\n",
				STM16_TO_HOST(readbuff, ALS_VALUE));
		} else {
			dev_err(&ps_motosh->client->dev,
				"Reading ALS_LUX failed [err: %d]\n",
				err);
		}
	}
	if (irq_status & N_DISP_BRIGHTNESS) {
		/* read DISPLAY_BRIGHTNESS */
		cmdbuff[0] = DISP_BRIGHTNESS_DATA;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 1);

		if (err >= 0) {
			motosh_as_data_buffer_write(ps_motosh, DT_DISP_BRIGHT,
				readbuff, 1, 0, NULL);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Display Brightness %d\n",
				readbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Reading DISPLAY_BRIGHTNESS failed [err: %d]\n",
				err);
		}
	}
	if (irq_status & N_STEP_COUNTER) {

		/* read STEP_COUNTER_INFO, last 4 bytes contain counter */
		cmdbuff[0] = STEP_COUNTER_INFO;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
					    1, 6);
		if (err >= 0) {
			motosh_as_data_buffer_write(ps_motosh, DT_STEP_COUNTER,
						    &readbuff[2], 4, 0, NULL);

			dev_dbg(&ps_motosh->client->dev,
				"Sending step count %X %X %X %X\n",
				readbuff[2], readbuff[3],
				readbuff[4], readbuff[5]);
		}
	}
	if (irq_status & N_UPDATE_ACCEL_CAL) {
		motosh_as_data_buffer_write(
			ps_motosh,
			DT_ACCEL_CAL,
			NULL,
			0,
			0,
			NULL);

		dev_info(&ps_motosh->client->dev,
			"Save accel cal");
	}
	if (irq_status & N_MOTO_MOD_CURRENT_DRAIN) {
		cmdbuff[0] = MOTO_MOD_CURRENT_DRAIN;
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 4);

		if (err >= 0) {
			motosh_as_data_buffer_write(ps_motosh,
				DT_MOTO_MOD_CURRENT_DRAIN,
				readbuff, 4, 0, NULL);

			dev_dbg(&ps_motosh->client->dev, "Sending Moto Mod Current Drain 0x%04X\n",
				STM32_TO_HOST(readbuff, 0));
		} else {
			dev_err(&ps_motosh->client->dev,
				"Reading MOTO_MOD_CURRENT_DRAIN failed [err: %d]\n",
				err);
		}
	}

	if (queue_length > MOTOSH_MAX_NWAKE_EVENT_QUEUE_SIZE) {
		dev_err(&ps_motosh->client->dev,
			"Invalid nwake queue_length: %d\n",
			queue_length);
		goto EXIT;
	} else if (queue_length == 0) {
		goto EXIT;
	}

	/* read nwake queue, due to I2C DMA memory limit
	   this may take 2 block reads
	*/
	if (queue_length <= MOTOSH_MAX_NWAKE_EVENT_QUEUE_READ_SIZE)
		read_size = queue_length;
	else
		read_size = MOTOSH_MAX_NWAKE_EVENT_QUEUE_READ_SIZE;

	cmdbuff[0] = NWAKE_MSG_QUEUE;
	err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
		1, read_size);

	/* get one more packet */
	if (err >= 0 &&
	   queue_length > read_size)
		err = motosh_i2c_write_read(ps_motosh, cmdbuff,
					    &readbuff[read_size],
					    1,
					    queue_length - read_size);

	if (err < 0) {
		dev_err(&ps_motosh->client->dev,
			"Reading nwake queue failed [len: %d] [err: %d]\n",
			queue_length,
			err);
		goto EXIT;
	}

	/* process each event from the queue */
	/* NOTE: the readbuff should not be modified while the event
	   queue is being processed */
	prev_message_id = MOTOSH_UNKNOWN_MSG;
	while (queue_index < queue_length) {
		unsigned char *data;
		unsigned char message_id = readbuff[queue_index];

		queue_index += MOTOSH_EVENT_QUEUE_MSG_ID_LEN;
		data = &readbuff[queue_index];
		switch (message_id) {
		case ACCEL_DATA:
			/* Accel supports batching while AP suspended, send
			   events on resume only if we are batching */
			if (!resuming ||
			    (ps_motosh->is_batching & ACCEL_BATCHING))
				motosh_as_data_buffer_write(ps_motosh,
							DT_ACCEL,
							data, 6, 0, &data[6]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, ACCEL_RD_X),
				STM16_TO_HOST(data, ACCEL_RD_Y),
				STM16_TO_HOST(data, ACCEL_RD_Z));
			queue_index += 6 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case LINEAR_ACCEL:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							DT_LIN_ACCEL,
							data, 6, 0, &data[6]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending lin_acc(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, ACCEL_RD_X),
				STM16_TO_HOST(data, ACCEL_RD_Y),
				STM16_TO_HOST(data, ACCEL_RD_Z));
			queue_index += 6 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case MAG_DATA:
		{
			unsigned char status;
			status = *(data + COMPASS_STATUS);

			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh, DT_MAG,
				data, 6, status, &data[13]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, MAG_X),
				STM16_TO_HOST(data, MAG_Y),
				STM16_TO_HOST(data, MAG_Z));

			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							    DT_ORIENT,
							    &data[6], 6,
							    status, &data[13]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, ORIENT_X),
				STM16_TO_HOST(data, ORIENT_Y),
				STM16_TO_HOST(data, ORIENT_Z));
			queue_index += 13 + MOTOSH_EVENT_TIMESTAMP_LEN;
		}
			break;
		case GYRO_DATA:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh, DT_GYRO,
				data, 6, 0, &data[6]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, GYRO_RD_X),
				STM16_TO_HOST(data, GYRO_RD_Y),
				STM16_TO_HOST(data, GYRO_RD_Z));
			queue_index += 6 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case UNCALIB_GYRO_DATA:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							DT_UNCALIB_GYRO,
							data, 12, 0, &data[12]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
				STM16_TO_HOST(data, GYRO_RD_X),
				STM16_TO_HOST(data, GYRO_RD_Y),
				STM16_TO_HOST(data, GYRO_RD_Z),
				STM16_TO_HOST(data, GYRO_UNCALIB_X),
				STM16_TO_HOST(data, GYRO_UNCALIB_Y),
				STM16_TO_HOST(data, GYRO_UNCALIB_Z));
			queue_index += 12 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case UNCALIB_MAG_DATA:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							DT_UNCALIB_MAG,
							data, 12, 0, &data[12]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Gyro uncalib(x,y,z)values:%d,%d,%d;%d,%d,%d\n",
				STM16_TO_HOST(data, MAG_X),
				STM16_TO_HOST(data, MAG_Y),
				STM16_TO_HOST(data, MAG_Z),
				STM16_TO_HOST(data, MAG_UNCALIB_X),
				STM16_TO_HOST(data, MAG_UNCALIB_Y),
				STM16_TO_HOST(data, MAG_UNCALIB_Z));
			queue_index += 12 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case QUATERNION_6AXIS:
			if (!resuming)
				motosh_as_data_buffer_write(
							    ps_motosh,
							    DT_QUAT_6AXIS,
							    data,
							    8,
							    0, &data[8]
							    );

			dev_dbg(
				&ps_motosh->client->dev,
				"Sending 6-axis quat values:%d,%d,%d,%d\n",
				STM16_TO_HOST(data, QUAT_6AXIS_A),
				STM16_TO_HOST(data, QUAT_6AXIS_B),
				STM16_TO_HOST(data, QUAT_6AXIS_C),
				STM16_TO_HOST(data, QUAT_6AXIS_W)
			);
			queue_index += 8 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case QUATERNION_9AXIS:
		{
			unsigned char status = *(data + 8);
			if (!resuming)
				motosh_as_data_buffer_write(
							    ps_motosh,
							    DT_QUAT_9AXIS,
							    data,
							    9,
							    status, &data[9]
							    );

			dev_dbg(
				&ps_motosh->client->dev,
				"Sending 9-axis quat values:%d,%d,%d,%d,%d\n",
				STM16_TO_HOST(data, QUAT_9AXIS_A),
				STM16_TO_HOST(data, QUAT_9AXIS_B),
				STM16_TO_HOST(data, QUAT_9AXIS_C),
				STM16_TO_HOST(data, QUAT_9AXIS_W),
				status
			);
			queue_index += 9 + MOTOSH_EVENT_TIMESTAMP_LEN;
		}
			break;
		case GAME_RV_DATA:
			if (!resuming)
				motosh_as_data_buffer_write(
							    ps_motosh,
							    DT_GAME_RV,
							    data,
							    8,
							    0, &data[8]
							    );

			dev_dbg(
				&ps_motosh->client->dev,
				"Sending game rotation values:%d,%d,%d,%d\n",
				STM16_TO_HOST(data, GAME_RV_A),
				STM16_TO_HOST(data, GAME_RV_B),
				STM16_TO_HOST(data, GAME_RV_C),
				STM16_TO_HOST(data, GAME_RV_W)
			);
			queue_index += 8 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case STEP_DETECTOR:
			/* Step detector always sends a 1 */
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
					DT_STEP_DETECTOR, data, 1, 0, NULL);

			dev_dbg(&ps_motosh->client->dev,
				"Sending step detector, %d\n", data[0]);

			queue_index += 1;
			break;
		case CAMSENSORSYNC:
			/* Sensor Sync always sends a 1 */
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
					DT_SENSOR_SYNC, data, 1, 0, &data[1]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending Camera sync, %d\n", data[0]);

			queue_index += 1;
			break;
		case PRESSURE_DATA:
			dev_err(&ps_motosh->client->dev, "Invalid CURRENT_PRESSURE event\n");

			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							    DT_PRESSURE,
							    data, 4, 0, NULL);

			dev_dbg(&ps_motosh->client->dev, "Sending pressure %d\n",
				STM32_TO_HOST(data, PRESSURE_VALUE));
			queue_index += 4;
			break;
		case GRAVITY:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							DT_GRAVITY,
							data, 6, 0, &data[6]);

			dev_dbg(&ps_motosh->client->dev,
				"Sending gravity(x,y,z)values:x=%d,y=%d,z=%d\n",
				STM16_TO_HOST(data, GRAV_X),
				STM16_TO_HOST(data, GRAV_Y),
				STM16_TO_HOST(data, GRAV_Z));
			queue_index += 6 + MOTOSH_EVENT_TIMESTAMP_LEN;
			break;
		case IR_GESTURE:
			if (!resuming)
				motosh_process_ir_gesture(ps_motosh, data);

			queue_index += MOTOSH_IR_SZ_GESTURE *
				MOTOSH_IR_GESTURE_CNT;
			break;
		case IR_RAW:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							    DT_IR_RAW,
							    data,
							    MOTOSH_IR_SZ_RAW,
							    0, NULL);
			dev_dbg(&ps_motosh->client->dev, "Sending raw IR data\n");
			queue_index += MOTOSH_IR_SZ_RAW;
			break;
		case IR_STATE:
			if (!resuming)
				motosh_as_data_buffer_write(ps_motosh,
							    DT_IR_OBJECT,
							    data, 1, 0, NULL);
			dev_dbg(&ps_motosh->client->dev, "Sending IR object state: 0x%x\n",
				data[0]);
			queue_index += 1;
			break;
		default:
			/* ERROR...unknown message
			   Need to drop the remaining data in this operation. */
			dev_err(&ps_motosh->client->dev, "ERROR: unknown nwake msg: 0x%02X prev_msg: 0x%04X\n",
				message_id,
				prev_message_id);
			/* a write to the work queue length causes
			   it to be reset */
			cmdbuff[0] = NWAKE_STATUS;
			cmdbuff[1] = 0x00;
			motosh_i2c_write(ps_motosh, cmdbuff, 2);
			goto EXIT;
		};

		prev_message_id = message_id;
	}

EXIT:
	/* Latest queue purged from hub. Reply to each pending flush bit
	   that is pending. Currently only accel is batched, others are
	   directly handled at IOCTL
	*/
	if (ps_motosh->nwake_flush_req & FLUSH_ACCEL_REQ) {
		uint32_t handle = ID_A;

		motosh_as_data_buffer_write(ps_motosh, DT_FLUSH,
					    (char *)&handle, 4, 0, NULL);
		dev_dbg(&ps_motosh->client->dev,
			 "flushed accel");
	}
	/* clear any flush requests pending */
	ps_motosh->nwake_flush_req = 0;

	/* last of batched accel processed */
	if (motosh_g_acc_cfg.timeout == 0)
		ps_motosh->is_batching &= (~ACCEL_BATCHING);

	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_motosh->lock);

}

int motosh_process_ir_gesture(struct motosh_data *ps_motosh,
			      unsigned char *data)
{
	int i;

	for (i = 0; i < MOTOSH_IR_GESTURE_CNT; i++) {
		int ofs = i * MOTOSH_IR_SZ_GESTURE;
		if (data[ofs + IR_GESTURE_EVENT] == 0)
			continue;
		motosh_as_data_buffer_write(ps_motosh, DT_IR_GESTURE,
					data + ofs,
					MOTOSH_IR_SZ_GESTURE, 0, NULL);
		dev_dbg(&ps_motosh->client->dev, "Send IR Gesture %d\n",
			data[ofs + IR_GESTURE_ID]);
	}
	return MOTOSH_IR_SZ_GESTURE * MOTOSH_IR_GESTURE_CNT;
}
