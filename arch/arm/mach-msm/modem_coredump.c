/* Copyright (c) 2011-2012, Motorola Mobility. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/sched.h>		/*wait.h needs THREAD_NORMAL*/
#include <asm-generic/poll.h>
#include <mach/msm_smsm.h>
#include "modem_coredump.h"

#define COREDUMP_WAIT_MSECS	120000

struct coredump_device {
	char name[256];

	unsigned int data_ready;
	unsigned int consumer_present;
	int coredump_status;

	struct completion coredump_complete;
	struct miscdevice device;

	wait_queue_head_t dump_wait_q;
};

static int coredump_open(struct inode *inode, struct file *filep)
{
	struct coredump_device *rd_dev = container_of(filep->private_data,
	struct coredump_device, device);
	rd_dev->consumer_present = 1;
	rd_dev->coredump_status = 0;
	return 0;
}

static int coredump_release(struct inode *inode, struct file *filep)
{
	struct coredump_device *rd_dev = container_of(filep->private_data,
				struct coredump_device, device);
	rd_dev->consumer_present = 0;
	rd_dev->data_ready = 0;
	complete(&rd_dev->coredump_complete);
	return 0;
}

#define MAX_IOREMAP_SIZE SZ_1M

static int coredump_read(struct file *filep, char __user *buf, size_t count,
			loff_t *pos)
{
	struct coredump_device *rd_dev = container_of(filep->private_data,
	struct coredump_device, device);
	char *device_mem = NULL;
	size_t copy_size = 0;
	int ret = 0;

	if (rd_dev->data_ready == 0) {
		pr_err("Coredump(%s): Read when there's no dump available!",
			rd_dev->name);
		return -EPIPE;
	}

	/* coredump is done after first read for now.*/
	if (*pos > 0) {
		pr_debug("Coredump(%s): Coredump complete. %lld bytes read.",
			rd_dev->name, *pos);
		rd_dev->coredump_status = 0;
		ret = 0;
		goto coredump_done;
	}

	/* get modem coredump from shared memory */
	device_mem = smem_get_entry(SMEM_ERR_CRASH_LOG, &copy_size);
	if (device_mem == 0) {
		pr_err("Coredump(%s): No crash log report\n", rd_dev->name);
		rd_dev->coredump_status = -1;
		ret = -EFAULT;
		goto coredump_done;
	}

	device_mem[copy_size - 1] = 0;
	pr_debug("Coredump(%s):copy_size=%d\n'%s'\n", rd_dev->name,
		copy_size, device_mem);

	copy_size = min(count, (size_t)MAX_IOREMAP_SIZE);
	copy_size = min(count, copy_size);

	if (copy_to_user(buf, device_mem, copy_size)) {
		pr_err("Coredump(%s): Couldn't copy all data to user.",
			rd_dev->name);
		rd_dev->coredump_status = -1;
		ret = -EFAULT;
		goto coredump_done;
	}

	*pos += copy_size;
	pr_debug("Coredump(%s): Read %d bytes.",
		rd_dev->name, copy_size);

	return copy_size;

coredump_done:
	rd_dev->data_ready = 0;
	*pos = 0;
	complete(&rd_dev->coredump_complete);
	return ret;
}

static unsigned int coredump_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	struct coredump_device *rd_dev = container_of(filep->private_data,
	struct coredump_device, device);
	unsigned int mask = 0;

	if (rd_dev->data_ready)
		mask |= (POLLIN | POLLRDNORM);

	poll_wait(filep, &rd_dev->dump_wait_q, wait);
	return mask;
}

const struct file_operations coredump_file_ops = {
	.open = coredump_open,
	.release = coredump_release,
	.read = coredump_read,
	.poll = coredump_poll
};

void *create_modem_coredump_device(const char *dev_name)
{
	int ret;
	struct coredump_device *rd_dev;

	if (!dev_name) {
		pr_err("%s: Invalid device name.\n", __func__);
		return NULL;
	}

	rd_dev = kzalloc(sizeof(struct coredump_device), GFP_KERNEL);

	if (!rd_dev) {
		pr_err("%s: Couldn't alloc space for coredump device!",
			__func__);
		return NULL;
	}

	snprintf(rd_dev->name, ARRAY_SIZE(rd_dev->name), "coredump_%s",
		 dev_name);

	init_completion(&rd_dev->coredump_complete);

	rd_dev->device.minor = MISC_DYNAMIC_MINOR;
	rd_dev->device.name = rd_dev->name;
	rd_dev->device.fops = &coredump_file_ops;

	init_waitqueue_head(&rd_dev->dump_wait_q);

	ret = misc_register(&rd_dev->device);

	if (ret) {
		pr_err("%s: misc_register failed for %s (%d)", __func__,
			dev_name, ret);
		kfree(rd_dev);
		return NULL;
	}

	return (void *)rd_dev;
}

int do_modem_coredump(void *handle)
{
	int ret;
	struct coredump_device *rd_dev = (struct coredump_device *)handle;

	if (!rd_dev->consumer_present) {
		pr_err("Coredump(%s): No consumers. Aborting..\n",
			rd_dev->name);
		return -EPIPE;
	}

	rd_dev->data_ready = 1;
	rd_dev->coredump_status = -1;

	INIT_COMPLETION(rd_dev->coredump_complete);

	/* Tell userspace that the data is ready */
	wake_up(&rd_dev->dump_wait_q);

	/* Wait (with a timeout) to let the coredump complete */
	ret = wait_for_completion_timeout(&rd_dev->coredump_complete,
			msecs_to_jiffies(COREDUMP_WAIT_MSECS));

	if (!ret) {
		pr_err("Coredump(%s): Timed out waiting for userspace.\n",
			rd_dev->name);
		ret = -EPIPE;
	} else
		ret = (rd_dev->coredump_status == 0) ? 0 : -EPIPE;

	rd_dev->data_ready = 0;
	return ret;
}

