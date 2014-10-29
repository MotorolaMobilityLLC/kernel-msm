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
#include <linux/android_alarm.h>
#include <linux/ktime.h>

#include <linux/stm401.h>


int stm401_as_data_buffer_write(struct stm401_data *ps_stm401,
	unsigned char type, unsigned char *data, int size,
	unsigned char status)
{
	int new_head;
	struct stm401_android_sensor_data *buffer;
	static bool error_reported;

	new_head = (ps_stm401->stm401_as_data_buffer_head + 1)
		& STM401_AS_DATA_QUEUE_MASK;
	if (new_head == ps_stm401->stm401_as_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&ps_stm401->client->dev, "as data buffer full\n");
			error_reported = true;
		}
		wake_up(&ps_stm401->stm401_as_data_wq);
		return 0;
	}
	buffer = &(ps_stm401->stm401_as_data_buffer[new_head]);
	buffer->type = type;
	buffer->status = status;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&ps_stm401->client->dev,
				"size %d exceeds as buffer\n", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	/* The timestamp needs to be on the same timebase as    */
	/* SystemClock.elapsedRealtimeNanos(), which is calling */
	/* alarm_get_elapsed_realtime().                        */
	buffer->timestamp = ktime_to_ns(alarm_get_elapsed_realtime());

	ps_stm401->stm401_as_data_buffer_head = new_head;
	wake_up(&ps_stm401->stm401_as_data_wq);

	error_reported = false;
	return 1;
}

int stm401_as_data_buffer_read(struct stm401_data *ps_stm401,
	struct stm401_android_sensor_data *buff)
{
	int new_tail;

	if (ps_stm401->stm401_as_data_buffer_tail
		== ps_stm401->stm401_as_data_buffer_head)
		return 0;

	new_tail = (ps_stm401->stm401_as_data_buffer_tail + 1)
		& STM401_AS_DATA_QUEUE_MASK;

	*buff = ps_stm401->stm401_as_data_buffer[new_tail];

	ps_stm401->stm401_as_data_buffer_tail = new_tail;

	return 1;
}

int stm401_ms_data_buffer_write(struct stm401_data *ps_stm401,
	unsigned char type, unsigned char *data, int size)
{
	int new_head;
	struct stm401_moto_sensor_data *buffer;
	struct timespec ts;
	static bool error_reported;

	new_head = (ps_stm401->stm401_ms_data_buffer_head + 1)
		& STM401_MS_DATA_QUEUE_MASK;
	if (new_head == ps_stm401->stm401_ms_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&ps_stm401->client->dev, "ms data buffer full\n");
			error_reported = true;
		}
		wake_up(&ps_stm401->stm401_ms_data_wq);
		return 0;
	}
	buffer = &(ps_stm401->stm401_ms_data_buffer[new_head]);
	buffer->type = type;
	if (data != NULL && size > 0) {
		if (size > sizeof(buffer->data)) {
			dev_err(&ps_stm401->client->dev,
				"size %d exceeds ms buffer\n", size);
			return 0;
		}
		memcpy(buffer->data, data, size);
	}
	buffer->size = size;

	ktime_get_ts(&ts);
	buffer->timestamp
		= ts.tv_sec*1000000000LL + ts.tv_nsec;

	ps_stm401->stm401_ms_data_buffer_head = new_head;
	wake_up(&ps_stm401->stm401_ms_data_wq);

	error_reported = false;
	return 1;
}

int stm401_ms_data_buffer_read(struct stm401_data *ps_stm401,
	struct stm401_moto_sensor_data *buff)
{
	int new_tail;

	if (ps_stm401->stm401_ms_data_buffer_tail
		== ps_stm401->stm401_ms_data_buffer_head)
		return 0;

	new_tail = (ps_stm401->stm401_ms_data_buffer_tail + 1)
		& STM401_MS_DATA_QUEUE_MASK;

	*buff = ps_stm401->stm401_ms_data_buffer[new_tail];

	ps_stm401->stm401_ms_data_buffer_tail = new_tail;

	return 1;
}

static int stm401_as_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&stm401_misc_data->client->dev, "stm401_as_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stm401_misc_data;

	return err;
}

static ssize_t stm401_as_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct stm401_android_sensor_data tmp_buff;
	struct stm401_data *ps_stm401 = file->private_data;

	ret = stm401_as_data_buffer_read(ps_stm401, &tmp_buff);
	if (ret == 0)
		return 0;
	ret = copy_to_user(buffer, &tmp_buff,
		sizeof(struct stm401_android_sensor_data));
	if (ret != 0) {
		dev_err(&ps_stm401->client->dev, "Copy error\n");
		return 0;
	}

	return sizeof(struct stm401_android_sensor_data);
}

static unsigned int stm401_as_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct stm401_data *ps_stm401 = file->private_data;

	poll_wait(file, &ps_stm401->stm401_as_data_wq, wait);
	if (ps_stm401->stm401_as_data_buffer_head
		!= ps_stm401->stm401_as_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

const struct file_operations stm401_as_fops = {
	.owner = THIS_MODULE,
	.open = stm401_as_open,
	.read = stm401_as_read,
	.poll = stm401_as_poll,
};

static int stm401_ms_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&stm401_misc_data->client->dev, "stm401_ms_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stm401_misc_data;

	return err;
}

static ssize_t stm401_ms_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct stm401_moto_sensor_data tmp_buff;
	struct stm401_data *ps_stm401 = file->private_data;

	ret = stm401_ms_data_buffer_read(ps_stm401, &tmp_buff);
	if (copy_to_user(buffer, &tmp_buff,
		sizeof(struct stm401_moto_sensor_data))
		!= 0) {
		dev_err(&ps_stm401->client->dev, "Copy error\n");
		ret = 0;
	}

	return ret;
}

static unsigned int stm401_ms_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct stm401_data *ps_stm401 = file->private_data;
	poll_wait(file, &ps_stm401->stm401_ms_data_wq, wait);
	if (ps_stm401->stm401_ms_data_buffer_head
		!= ps_stm401->stm401_ms_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

const struct file_operations stm401_ms_fops = {
	.owner = THIS_MODULE,
	.open = stm401_ms_open,
	.read = stm401_ms_read,
	.poll = stm401_ms_poll,
};
