/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _NANOHUB_MAIN_H
#define _NANOHUB_MAIN_H

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/wakelock.h>

#include "comms.h"
#include "bl.h"

#define NANOHUB_NAME "nanohub"

struct nanohub_buf {
	struct list_head list;
	uint8_t buffer[255];
	uint8_t length;
};

struct nanohub_data {
	struct class *sensor_class;
	struct device *sensor_dev;
	struct device *comms_dev;
	struct iio_dev *iio_dev;
	struct cdev cdev;
	struct cdev cdev_comms;
	struct nanohub_comms comms;
	struct nanohub_bl bl;
	const struct nanohub_platform_data *pdata;
	int irq1;
	int irq2;

	atomic_t kthread_run;
	wait_queue_head_t kthread_wait;

	wait_queue_head_t read_wait;
	struct list_head read_data;
	atomic_t read_cnt;
	spinlock_t read_lock;
	struct wake_lock wakelock_read;

	struct list_head read_free;
	atomic_t read_free_cnt;
	spinlock_t read_free_lock;

	atomic_t wakeup_cnt;
	spinlock_t wakeup_lock;
	atomic_t wakeup_aquired;
	wait_queue_head_t wakeup_wait;

	uint32_t interrupts[8];

	int err_cnt;

	wait_queue_head_t hal_read_wait;
	struct list_head hal_read_data;
	atomic_t hal_read_cnt;
	spinlock_t hal_read_lock;
};

int request_wakeup(struct nanohub_data *data);
int request_wakeup_timeout(struct nanohub_data *data, long timeout);
void release_wakeup(struct nanohub_data *data);
struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev);
int nanohub_reset(struct nanohub_data *data);
int nanohub_remove(struct iio_dev *iio_dev);
int nanohub_suspend(struct iio_dev *iio_dev);
int nanohub_resume(struct iio_dev *iio_dev);

#endif
