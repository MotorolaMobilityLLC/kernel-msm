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
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/stml0xx.h>

int stml0xx_as_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size, unsigned char status)
{
	int new_head;
	struct stml0xx_android_sensor_data *buffer;
	struct timespec ts;
	static bool error_reported;

	new_head = (ps_stml0xx->stml0xx_as_data_buffer_head + 1)
	    & STML0XX_AS_DATA_QUEUE_MASK;
	if (new_head == ps_stml0xx->stml0xx_as_data_buffer_tail) {
		if (!error_reported) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"as data buffer full");
			error_reported = true;
		}
		wake_up(&ps_stml0xx->stml0xx_as_data_wq);
		return 0;
	}
	buffer = &(ps_stml0xx->stml0xx_as_data_buffer[new_head]);
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

	ktime_get_ts(&ts);
	buffer->timestamp = ts.tv_sec * 1000000000LL + ts.tv_nsec;

	ps_stml0xx->stml0xx_as_data_buffer_head = new_head;
	wake_up(&ps_stml0xx->stml0xx_as_data_wq);

	error_reported = false;
	return 1;
}

int stml0xx_as_data_buffer_read(struct stml0xx_data *ps_stml0xx,
				struct stml0xx_android_sensor_data *buff)
{
	int new_tail;

	if (ps_stml0xx->stml0xx_as_data_buffer_tail
	    == ps_stml0xx->stml0xx_as_data_buffer_head)
		return 0;

	new_tail = (ps_stml0xx->stml0xx_as_data_buffer_tail + 1)
	    & STML0XX_AS_DATA_QUEUE_MASK;

	*buff = ps_stml0xx->stml0xx_as_data_buffer[new_tail];

	ps_stml0xx->stml0xx_as_data_buffer_tail = new_tail;

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

	ktime_get_ts(&ts);
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
	if (ps_stml0xx->stml0xx_as_data_buffer_head
	    != ps_stml0xx->stml0xx_as_data_buffer_tail)
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
