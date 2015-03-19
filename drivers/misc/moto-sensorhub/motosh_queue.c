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


int motosh_as_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size,
	unsigned char status)
{
	int new_head;
	struct motosh_android_sensor_data *buffer;
	static bool error_reported;

	new_head = (ps_motosh->motosh_as_data_buffer_head + 1)
		& MOTOSH_AS_DATA_QUEUE_MASK;
	/* Check for buffer full */
	if (new_head == ps_motosh->motosh_as_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&ps_motosh->client->dev, "as data buffer full\n");
			error_reported = true;
		}
		/* Some events (especially on-change events) are very
		 * important to know about if we have to drop them
		 */
		switch (type) {
		case DT_ALS:
			dev_err(
				&ps_motosh->client->dev,
				"dropped als event: %d",
				STM16_TO_HOST(ALS_VALUE)
			);
			break;
		case DT_PROX:
			dev_err(
				&ps_motosh->client->dev,
				"dropped prox event: %d",
				motosh_readbuff[PROX_DISTANCE]
			);
			break;
		case DT_DISP_ROTATE:
			dev_err(
				&ps_motosh->client->dev,
				"dropped disp-rotate event: %d",
				motosh_readbuff[DISP_VALUE]
			);
			break;
		default:
			break;
		}
		wake_up(&ps_motosh->motosh_as_data_wq);
		return 0;
	}
	buffer = &(ps_motosh->motosh_as_data_buffer[new_head]);
	buffer->type = type;
	buffer->status = status;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&ps_motosh->client->dev,
				"size %d exceeds as buffer\n", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	buffer->timestamp = motosh_timestamp_ns();

	ps_motosh->motosh_as_data_buffer_head = new_head;
	wake_up(&ps_motosh->motosh_as_data_wq);

	error_reported = false;
	return 1;
}

int motosh_as_data_buffer_read(struct motosh_data *ps_motosh,
	struct motosh_android_sensor_data *buff)
{
	int new_tail;

	if (ps_motosh->motosh_as_data_buffer_tail
		== ps_motosh->motosh_as_data_buffer_head)
		return 0;

	new_tail = (ps_motosh->motosh_as_data_buffer_tail + 1)
		& MOTOSH_AS_DATA_QUEUE_MASK;

	*buff = ps_motosh->motosh_as_data_buffer[new_tail];

	ps_motosh->motosh_as_data_buffer_tail = new_tail;

	return 1;
}

int motosh_ms_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size)
{
	int new_head;
	struct motosh_moto_sensor_data *buffer;
	static bool error_reported;

	new_head = (ps_motosh->motosh_ms_data_buffer_head + 1)
		& MOTOSH_MS_DATA_QUEUE_MASK;
	if (new_head == ps_motosh->motosh_ms_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&ps_motosh->client->dev, "ms data buffer full\n");
			error_reported = true;
		}
		wake_up(&ps_motosh->motosh_ms_data_wq);
		return 0;
	}
	buffer = &(ps_motosh->motosh_ms_data_buffer[new_head]);
	buffer->type = type;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&ps_motosh->client->dev,
				"size %d exceeds ms buffer\n", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	buffer->timestamp = motosh_timestamp_ns();

	ps_motosh->motosh_ms_data_buffer_head = new_head;
	wake_up(&ps_motosh->motosh_ms_data_wq);

	error_reported = false;
	return 1;
}

int motosh_ms_data_buffer_read(struct motosh_data *ps_motosh,
	struct motosh_moto_sensor_data *buff)
{
	int new_tail;

	if (ps_motosh->motosh_ms_data_buffer_tail
		== ps_motosh->motosh_ms_data_buffer_head)
		return 0;

	new_tail = (ps_motosh->motosh_ms_data_buffer_tail + 1)
		& MOTOSH_MS_DATA_QUEUE_MASK;

	*buff = ps_motosh->motosh_ms_data_buffer[new_tail];

	ps_motosh->motosh_ms_data_buffer_tail = new_tail;

	return 1;
}

static int motosh_as_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&motosh_misc_data->client->dev, "motosh_as_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = motosh_misc_data;

	return err;
}

static ssize_t motosh_as_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct motosh_android_sensor_data tmp_buff;
	struct motosh_data *ps_motosh = file->private_data;

	ret = motosh_as_data_buffer_read(ps_motosh, &tmp_buff);
	if (ret == 0)
		return 0;
	ret = copy_to_user(buffer, &tmp_buff,
		sizeof(struct motosh_android_sensor_data));
	if (ret != 0) {
		dev_err(&ps_motosh->client->dev, "Copy error\n");
		return 0;
	}

	return sizeof(struct motosh_android_sensor_data);
}

static unsigned int motosh_as_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct motosh_data *ps_motosh = file->private_data;

	poll_wait(file, &ps_motosh->motosh_as_data_wq, wait);
	if (ps_motosh->motosh_as_data_buffer_head
		!= ps_motosh->motosh_as_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

const struct file_operations motosh_as_fops = {
	.owner = THIS_MODULE,
	.open = motosh_as_open,
	.read = motosh_as_read,
	.poll = motosh_as_poll,
};

static int motosh_ms_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&motosh_misc_data->client->dev, "motosh_ms_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = motosh_misc_data;

	return err;
}

static ssize_t motosh_ms_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct motosh_moto_sensor_data tmp_buff;
	struct motosh_data *ps_motosh = file->private_data;

	ret = motosh_ms_data_buffer_read(ps_motosh, &tmp_buff);
	if (copy_to_user(buffer, &tmp_buff,
		sizeof(struct motosh_moto_sensor_data))
		!= 0) {
		dev_err(&ps_motosh->client->dev, "Copy error\n");
		ret = 0;
	}

	return ret;
}

static unsigned int motosh_ms_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct motosh_data *ps_motosh = file->private_data;
	poll_wait(file, &ps_motosh->motosh_ms_data_wq, wait);
	if (ps_motosh->motosh_ms_data_buffer_head
		!= ps_motosh->motosh_ms_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

const struct file_operations motosh_ms_fops = {
	.owner = THIS_MODULE,
	.open = motosh_ms_open,
	.read = motosh_ms_read,
	.poll = motosh_ms_poll,
};
