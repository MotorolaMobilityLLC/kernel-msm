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
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

static int maxBufFill = -1;
static long long int as_queue_numremoved;
static long long int as_queue_numadded;
#define AS_QUEUE_SIZE_ESTIMATE (as_queue_numadded - as_queue_numremoved)
#define AS_QUEUE_MAX_SIZE (1024)

/**
 * motosh_as_data_buffer_write() - put a sensor event on the as queue
 *
 * @ps_motosh: the global motosh data struct
 * @type:      the data type (DT_*)
 * @data:      raw sensor data
 * @size:      the size in bytes of the data
 * @status:    the status of the sensor event
 *
 * NOTE: this function must be reentrant since kmalloc with
 *       GFP_KERNEL flags can block.
 *
 * Return: 1 on success, 0 on dropped event
 */
int motosh_as_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size,
	unsigned char status, bool timestamped)
{
	static bool full_reported;

	long long int queue_size;
	struct as_node *new_tail;
	struct motosh_android_sensor_data *buffer;
	int64_t now;

	/* Get current queue size */
	queue_size = AS_QUEUE_SIZE_ESTIMATE;

	if (queue_size > maxBufFill) {
		dev_dbg(
			&ps_motosh->client->dev,
			"maxBufFill = %d",
			maxBufFill);
		maxBufFill = queue_size;
	}

	/* Check for queue full */
	if (queue_size >= AS_QUEUE_MAX_SIZE) {
		if (!full_reported) {
			dev_err(&ps_motosh->client->dev, "as data buffer full\n");
			full_reported = true;
		}
		/* Some events (especially on-change events) are very
		 * important to know about if we have to drop them
		 */
		switch (type) {
		case DT_ALS:
			dev_err(
				&ps_motosh->client->dev,
				"dropped als event: %d",
				STM16_TO_HOST(data, ALS_VALUE)
			);
			break;
		case DT_PROX:
			dev_err(
				&ps_motosh->client->dev,
				"dropped prox event: %d",
				data[PROX_DISTANCE]
			);
			break;
		case DT_DISP_ROTATE:
			dev_err(
				&ps_motosh->client->dev,
				"dropped disp-rotate event: %d",
				data[DISP_VALUE]
			);
			break;
		default:
			break;
		}
		wake_up(&ps_motosh->motosh_as_data_wq);
		return 0;
	} /* END: queue full */

	/* Make a new node */
	new_tail = kmalloc(
		sizeof(struct as_node),
		GFP_KERNEL
	);
	if (!new_tail) {
		wake_up(&ps_motosh->motosh_as_data_wq);
		dev_err(
			&ps_motosh->client->dev,
			"as queue kmalloc error"
		);
		return 0;
	}

	/* Populate node data */
	buffer = &(new_tail->data);
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

	/* Add new_tail to end of queue */
	spin_lock(&(ps_motosh->as_queue_lock));

	/* Have to timestamp after lock to avoid
	 * out-of-order events
	 */
	now = motosh_timestamp_ns();
	if (timestamped) {
		bool streaming;

		/* if a timestamp is included in the event buffer,
		   it is the 3 bytes after the data */
		buffer->timestamp =
			motosh_time_recover((data[size] << 16) |
					    (data[size+1] <<  8) |
					    (data[size+2]),
					    now);
		if (type == DT_ACCEL &&
		   (ps_motosh->is_batching & ACCEL_BATCHING))
			streaming = false;
		else
			streaming = true;

		/* compensate for AP/Hub drift based on recovered sample
		   time and the current time */
		motosh_time_drift_comp(buffer->timestamp,
				       now,
				       streaming);

		/* check for erroneous future time */
		if (buffer->timestamp > now) {
			dev_dbg(&ps_motosh->client->dev,
				"future time, delta: %lld\n",
				buffer->timestamp - now);
			buffer->timestamp = now;
		}
	} else
		buffer->timestamp = now;

	list_add_tail(&(new_tail->list), &(ps_motosh->as_queue.list));
	++as_queue_numadded;
	spin_unlock(&(ps_motosh->as_queue_lock));

	wake_up(&ps_motosh->motosh_as_data_wq);

	/* If we got here, the queue is not full */
	if (full_reported) {
		dev_err(
			&ps_motosh->client->dev,
			"as data buffer not full\n"
		);
	}
	full_reported = false;
	return 1;
}

/**
 * motosh_as_data_buffer_read() - read/remove an item from the as queue
 * @ps_motosh: the global motosh data struct
 * @buff:      the output data item
 *
 * NOTE: this function is not reentrant, due to the fact that there is
 *       only one single-threaded reader
 * Return: 1 on success, 0 on empty queue
 */
int motosh_as_data_buffer_read(struct motosh_data *ps_motosh,
	struct motosh_android_sensor_data *buff)
{
	struct as_node *first;

	/* Get first item in queue */
	first = list_first_entry_or_null(
		&(ps_motosh->as_queue.list),
		struct as_node,
		list
	);
	if (first == NULL)
		return 0;

	/* Copy out the data */
	*buff = first->data;

	/* Delete the item */
	spin_lock(&(ps_motosh->as_queue_lock));
		list_del(&(first->list));
		++as_queue_numremoved;
	spin_unlock(&(ps_motosh->as_queue_lock));
	kfree(first);
	return 1;
}

int motosh_ms_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size, bool timestamped)
{
	int new_head;
	struct motosh_moto_sensor_data *buffer;
	static bool error_reported;
	int64_t now;

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

	now = motosh_timestamp_ns();
	if (timestamped) {
		/* if a timestamp is included in the event buffer,
		   it is the 3 bytes after the data */
		buffer->timestamp =
			motosh_time_recover((data[size] << 16) |
					    (data[size+1] <<  8) |
					    (data[size+2]),
					    now);
		/* check for erroneous future time */
		if (buffer->timestamp > now) {
			dev_dbg(&ps_motosh->client->dev,
				"future time, delta: %lld\n",
				buffer->timestamp - now);
			buffer->timestamp = now;
		}
	} else
		buffer->timestamp = now;

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
	if (!list_empty(&(ps_motosh->as_queue.list)))
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
