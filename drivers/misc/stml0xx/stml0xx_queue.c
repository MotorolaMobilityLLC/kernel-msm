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

#include <linux/dropbox.h>
#include <linux/string.h>
#include <linux/stml0xx.h>

static int maxBufFill = -1;
static long long int as_queue_numremoved;
static long long int as_queue_numadded;
#define AS_QUEUE_SIZE_ESTIMATE (as_queue_numadded - as_queue_numremoved)
#define AS_QUEUE_MAX_SIZE (1024)

#define DROPBOX_QUEDROP_ISSUE "queuedrop_issue"
/**
 * stml0xx_as_data_buffer_write() - put a sensor event on the as queue
 *
 * @ps_stml0xx: the global stml0xx data struct
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
int stml0xx_as_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size, unsigned char status,
				 uint64_t timestamp_ns)
{
	static bool full_reported;

	long long int queue_size;
	struct as_node *new_tail;
	struct stml0xx_android_sensor_data *buffer;
	char dropbox_entry[256];

	/* Get current queue size */
	queue_size = AS_QUEUE_SIZE_ESTIMATE;

	if (queue_size > maxBufFill) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
				"maxBufFill = %d",
				maxBufFill);
		maxBufFill = queue_size;
	}

	/* Check for queue full */
	if (queue_size >= AS_QUEUE_MAX_SIZE) {
		if (!full_reported) {
			dev_err(&stml0xx_misc_data->spi->dev,
					"as data buffer full\n");
			full_reported = true;
			/*
			* Send buffer queue full dropbox,
			* avoid to send many bug2go event
			*/
			memset(dropbox_entry, 0, sizeof(dropbox_entry));
			snprintf(dropbox_entry, sizeof(dropbox_entry),
				"DT QUEUE drop for %d", type);
			dropbox_queue_event_text(DROPBOX_QUEDROP_ISSUE,
				dropbox_entry, strlen(dropbox_entry));
		}
		wake_up(&ps_stml0xx->stml0xx_as_data_wq);
		return 0;
	} /* END: queue full */

	/* Make a new node */
	new_tail = kmalloc(sizeof(struct as_node), GFP_KERNEL);
	if (!new_tail) {
		wake_up(&ps_stml0xx->stml0xx_as_data_wq);
		dev_err(&stml0xx_misc_data->spi->dev,
				"as queue kmalloc error");
		return 0;
	}

	/* Populate node data */
	buffer = &(new_tail->data);
	buffer->type = type;
	buffer->status = status;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"size %d exceeds as buffer", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	/* Add new_tail to end of queue */
	spin_lock(&(ps_stml0xx->as_queue_lock));
		/* Have to timestamp after lock to avoid
		 * out-of-order events
		 */
		buffer->timestamp = timestamp_ns;
		list_add_tail(&(new_tail->list), &(ps_stml0xx->as_queue.list));
		++as_queue_numadded;
	spin_unlock(&(ps_stml0xx->as_queue_lock));

	wake_up(&ps_stml0xx->stml0xx_as_data_wq);

	/* If we got here, the queue is not full */
	if (full_reported)
		dev_err(&stml0xx_misc_data->spi->dev,
				"as data buffer not full\n");
	full_reported = false;
	return 1;
}

/**
 * stml0xx_as_data_buffer_read() - read/remove an item from the as queue
 * @ps_stml0xx: the global stml0xx data struct
 * @buff:      the output data item
 *
 * NOTE: this function is not reentrant, due to the fact that there is
 *       only one single-threaded reader
 * Return: 1 on success, 0 on empty queue
 */
int stml0xx_as_data_buffer_read(struct stml0xx_data *ps_stml0xx,
				struct stml0xx_android_sensor_data *buff)
{
	struct as_node *first;

	/* Get first item in queue */
	first = list_first_entry_or_null(&(ps_stml0xx->as_queue.list),
			struct as_node, list);
	if (first == NULL)
		return 0;

	/* Copy out the data */
	*buff = first->data;

	/* Delete the item */
	spin_lock(&(ps_stml0xx->as_queue_lock));
		list_del(&(first->list));
		++as_queue_numremoved;
	spin_unlock(&(ps_stml0xx->as_queue_lock));
	kfree(first);

	return 1;
}

int stml0xx_ms_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size)
{
	int new_head;
	struct stml0xx_moto_sensor_data *buffer;
	struct timespec ts;
	static bool error_reported;

	new_head = (ps_stml0xx->stml0xx_ms_data_buffer_head + 1)
	    & STML0XX_MS_DATA_QUEUE_MASK;
	if (new_head == ps_stml0xx->stml0xx_ms_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"ms data buffer full");
			error_reported = true;
		}
		wake_up(&ps_stml0xx->stml0xx_ms_data_wq);
		return 0;
	}
	buffer = &(ps_stml0xx->stml0xx_ms_data_buffer[new_head]);
	buffer->type = type;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"size %d exceeds ms buffer", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	get_monotonic_boottime(&ts);
	buffer->timestamp = ts.tv_sec * 1000000000LL + ts.tv_nsec;

	ps_stml0xx->stml0xx_ms_data_buffer_head = new_head;
	wake_up(&ps_stml0xx->stml0xx_ms_data_wq);

	error_reported = false;
	return 1;
}

int stml0xx_ms_data_buffer_read(struct stml0xx_data *ps_stml0xx,
				struct stml0xx_moto_sensor_data *buff)
{
	int new_tail;

	if (ps_stml0xx->stml0xx_ms_data_buffer_tail
	    == ps_stml0xx->stml0xx_ms_data_buffer_head)
		return 0;

	new_tail = (ps_stml0xx->stml0xx_ms_data_buffer_tail + 1)
	    & STML0XX_MS_DATA_QUEUE_MASK;

	*buff = ps_stml0xx->stml0xx_ms_data_buffer[new_tail];

	ps_stml0xx->stml0xx_ms_data_buffer_tail = new_tail;

	return 1;
}

static int stml0xx_as_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_as_open");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stml0xx_misc_data;

	return err;
}

static ssize_t stml0xx_as_read(struct file *file, char __user *buffer,
			       size_t size, loff_t *ppos)
{
	int ret;
	struct stml0xx_android_sensor_data tmp_buff;
	struct stml0xx_data *ps_stml0xx = file->private_data;

	ret = stml0xx_as_data_buffer_read(ps_stml0xx, &tmp_buff);
	if (ret == 0)
		return 0;

	ret = copy_to_user(buffer, &tmp_buff,
			   sizeof(struct stml0xx_android_sensor_data));
	if (ret != 0) {
		dev_err(&stml0xx_misc_data->spi->dev, "Copy error");
		return 0;
	}

	return sizeof(struct stml0xx_android_sensor_data);
}

static unsigned int stml0xx_as_poll(struct file *file,
				    struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct stml0xx_data *ps_stml0xx = file->private_data;

	poll_wait(file, &ps_stml0xx->stml0xx_as_data_wq, wait);
	if (!list_empty(&(ps_stml0xx->as_queue.list)))
		mask = POLLIN | POLLRDNORM;

	return mask;
}

const struct file_operations stml0xx_as_fops = {
	.owner = THIS_MODULE,
	.open = stml0xx_as_open,
	.read = stml0xx_as_read,
	.poll = stml0xx_as_poll,
};

static int stml0xx_ms_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_ms_open");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stml0xx_misc_data;

	return err;
}

static ssize_t stml0xx_ms_read(struct file *file, char __user *buffer,
			       size_t size, loff_t *ppos)
{
	int ret;
	struct stml0xx_moto_sensor_data tmp_buff;
	struct stml0xx_data *ps_stml0xx = file->private_data;

	ret = stml0xx_ms_data_buffer_read(ps_stml0xx, &tmp_buff);
	if (copy_to_user(buffer, &tmp_buff,
			 sizeof(struct stml0xx_moto_sensor_data))
	    != 0) {
		dev_err(&stml0xx_misc_data->spi->dev, "Copy error");
		ret = 0;
	}

	return ret;
}

static unsigned int stml0xx_ms_poll(struct file *file,
				    struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct stml0xx_data *ps_stml0xx = file->private_data;
	poll_wait(file, &ps_stml0xx->stml0xx_ms_data_wq, wait);
	if (ps_stml0xx->stml0xx_ms_data_buffer_head
	    != ps_stml0xx->stml0xx_ms_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

const struct file_operations stml0xx_ms_fops = {
	.owner = THIS_MODULE,
	.open = stml0xx_ms_open,
	.read = stml0xx_ms_read,
	.poll = stml0xx_ms_poll,
};
