// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/delay.h>
#include <linux/eom_ioctl.h>
#include <linux/eventfd.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/phy_core.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "buffer_manager.h"
#include "eom_driver.h"

/* Global list of active contexts */
static LIST_HEAD(eom_context_list);
static DEFINE_MUTEX(eom_context_list_lock);

struct miscdevice eom_global_miscdev;

static long eom_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static long eom_lane_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t eom_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

static int dummy_eom_sequence(struct eom_lane *lane)
{
	int x, y, error_count;

	pr_debug("Running Dummy EOM for %s\n", lane->seq->name);
	/* Dummy EOM sequence for testing */
	for (x = 0; x < 128; x++) {
		for (y = 0; y < 64; y++) {
			error_count = x + y;
			struct eom_entry entry = { x, y, error_count };

			eom_buffer_write(lane->buffer, (char *)&entry, sizeof(entry));
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USB_MSM_EOM)
static int usb_eom_sequence(struct eom_lane *lane)
{
	pr_debug("Running USB EOM for %s\n", lane->seq->name);
	/* Add USB EOM sequence logic */
	return 0;
}
#endif

/**
 * phy_pcie_eom_sequence - Weak fallback for PCIe EOM sequence
 * @lane: EOM lane structure
 *
 * This weak symbol provides a fallback implementation when no architecture-specific
 * PCIe EOM implementation is compiled. It will be overridden by strong symbols
 * from pcie_eom_<target>_phy.c when they are compiled.
 *
 * Return: -EOPNOTSUPP indicating no implementation available
 */
int __weak phy_pcie_eom_sequence(struct eom_lane *lane)
{
	pr_err("PCIe EOM sequence not implemented for this architecture\n");
	pr_err("Please enable relevant CONFIG_ARCH\n");
	return -EOPNOTSUPP;
}

static struct eom_dev_sequence eom_devices[] = {
	[EOM_DUMMY_INDEX] = { .name = "dummy",
			      .type = TYPE_DUMMY,
			      .run_eom = dummy_eom_sequence },
#if IS_ENABLED(CONFIG_PCI_MSM_EOM)
	[EOM_PCIE_INDEX] = { .name = "pcie",
			     .type = TYPE_PCIE,
			     .run_eom = phy_pcie_eom_sequence },
#endif
#if IS_ENABLED(CONFIG_USB_MSM_EOM)
	[EOM_USB_INDEX] = { .name = "usb",
			    .type = TYPE_USB,
			    .run_eom = usb_eom_sequence },
#endif
	/*
	 * Add more devices as needed make sure to update enum eom_dev_sequence_index
	 * also make sure the name is unique
	 */
};

#define NUM_EOM_DEVICES ARRAY_SIZE(eom_devices)

static int get_seq_index(int type, const char *name)
{
	int i;

	if (type >= TYPE_MAX || !name)
		return -EINVAL;

	for (i = 0; i < NUM_EOM_DEVICES; i++) {
		if (eom_devices[i].type == type &&
		    strstr(eom_devices[i].name, name) != NULL &&
		    strlen(eom_devices[i].name) == strlen(name)) {
			return i;
		}
	}
	return -ENOENT;
}

/* Start EOM kernel thread */
static int eom_run(void *arg)
{
	struct eom_lane *lane = arg;
	struct eom_dev_sequence *eom_dev = lane->seq;
	int ret = -ENODEV;

	pr_debug("Starting EOM sequence for device: %s\n", eom_dev->name);
	if (eom_dev->run_eom)
		ret = eom_dev->run_eom(lane);

	if (lane->eventfd) {
		pr_debug("Send EOM completion event for device: %s\n", eom_dev->name);
		eventfd_signal(lane->eventfd, 1);
	}

	return ret;
}

/* Ioctl handler */
static long eom_lane_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct miscdevice *misc_dev = file->private_data;
	struct eom_lane *lane = container_of(misc_dev, struct eom_lane, miscdev);
	struct eventfd_ctx *eventfd;
	struct task_struct *task;
	int fd;

	switch (cmd) {
	case EOM_IOCTL_SET_EVENTFD:
		if (copy_from_user(&fd, (void __user *)arg, sizeof(fd)))
			return -EFAULT;

		eventfd = eventfd_ctx_fdget(fd);
		if (IS_ERR(eventfd))
			return PTR_ERR(eventfd);

		lane->eventfd = eventfd;
		break;

	case EOM_IOCTL_START_EOM:
		atomic_set(&g_eom_seq_stop, 0);
		atomic_set(&lane->eom_seq_stop, 0);
		/* clean up old pages if any and reset the buffer counter and read pointer */
		eom_buffer_free(lane->buffer);
		task = kthread_run(eom_run, lane, "eom_%s_lane_%d",
				   lane->seq->name, lane->lane_num);
		if (IS_ERR(task))
			return PTR_ERR(task);

		break;

	case EOM_IOCTL_STOP_EOM:
		if (!lane)
			return -EINVAL;

		atomic_set(&g_eom_seq_stop, 1);
		atomic_set(&lane->eom_seq_stop, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Read function for lane device */
static ssize_t eom_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	struct miscdevice *misc_dev = file->private_data;
	struct eom_lane *lane =
		container_of(misc_dev, struct eom_lane, miscdev);

	return eom_buffer_read(lane->buffer, buf, count);
}

static const struct file_operations eom_lane_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = eom_lane_ioctl,
	.compat_ioctl = eom_lane_ioctl,
	.read = eom_read,
};

/* Ioctl handler */
static long eom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct eom_context *eom_ctx = NULL;
	struct eom_context *ctx, *tmp_ctx;
	struct eom_phy_device *phy_dev;
	struct eom_select_device dev;
	int i, seq_index = 0, dwell_time = TIME_100MS_US;

	switch (cmd) {
	case EOM_IOCTL_SELECT_DEVICE:
		if (copy_from_user(&dev, (void __user *)arg, sizeof(dev)))
			return -EFAULT;

		if (dev.type >= TYPE_MAX) {
			pr_err("EOM: unknown device type\n");
			return -EINVAL;
		}

		mutex_lock(&eom_context_list_lock);
		list_for_each_entry_safe(ctx, tmp_ctx, &eom_context_list, list) {
			if (ctx->index == dev.index && ctx->type == dev.type) {
				mutex_unlock(&eom_context_list_lock);
				pr_err("EOM: Device already selected\n");
				return -EBUSY;
			}
		}

		mutex_unlock(&eom_context_list_lock);
		/* Find PHY device */
		phy_dev = get_eom_phy_device(dev.type, dev.index, dev.vendor_id, dev.device_id);
		if (!phy_dev)
			return -ENODEV;

		if (phy_dev->ops->get_caps) {
			if (phy_dev->ops->get_caps(phy_dev->priv) < 0) {
				pr_err("EOM: Phy not present for RC %d\n", dev.index);
				return -ENODEV;
			}
		}

		seq_index = get_seq_index(dev.type, dev.name);
		if (seq_index < 0) {
			pr_err("EOM: Unsupported device type/name %s\n", dev.name);
			return seq_index;
		}

		eom_ctx = kzalloc(sizeof(*eom_ctx), GFP_KERNEL);
		if (!eom_ctx)
			return -ENOMEM;

		eom_ctx->index = dev.index;
		/* Assume this is an integer corresponding to our enum */
		eom_ctx->type = dev.type;
		mutex_init(&eom_ctx->lock);
		eom_ctx->phy_dev = phy_dev;
		eom_ctx->num_lanes = phy_dev->lanes;
		if (dev.dwell_time_us > 0)
			dwell_time = dev.dwell_time_us;

		/* Create misc devices for each lane */
		for (i = 0; i < phy_dev->lanes; i++) {
			struct eom_lane *lane = kzalloc(sizeof(*lane), GFP_KERNEL);
			char dev_name[32];
			int ret = 0;

			if (!lane)
				continue;

			atomic_set(&lane->eom_seq_stop, 0);
			lane->phy_dev = phy_dev;
			lane->lane_num = i;
			lane->seq = &eom_devices[seq_index];
			snprintf(dev_name, sizeof(dev_name), "eom_%s%d_lane%d",
				 eom_device_names[dev.type], dev.index, i);
			lane->buffer = kzalloc(sizeof(struct eom_buffer), GFP_KERNEL);
			lane->dwell_time_us = dwell_time;
			lane->miscdev.name = dev_name;
			lane->miscdev.minor = MISC_DYNAMIC_MINOR;
			lane->miscdev.fops = &eom_lane_fops;
			eom_buffer_init(lane->buffer);
			eom_ctx->lanes[i] = lane;
			pr_debug("EOM: Register lane device %s\n", dev_name);

			ret = misc_register(&lane->miscdev);
			if (ret < 0) {
				pr_err("EOM: Failed to register lane device %s: err: %d\n",
					dev_name, ret);
				eom_buffer_free(lane->buffer);
				kfree(lane->buffer);
				kfree(lane);
				eom_ctx->lanes[i] = NULL;
				continue;
			}

		}

		mutex_lock(&eom_context_list_lock);
		list_add_tail(&eom_ctx->list, &eom_context_list);
		mutex_unlock(&eom_context_list_lock);
		break;

	case EOM_IOCTL_STOP_EOM:
		atomic_set(&g_eom_seq_stop, 1);
		break;

	case EOM_IOCTL_CLEAN_EOM:
		atomic_set(&g_eom_seq_stop, 1);
		if (copy_from_user(&dev, (void __user *)arg, sizeof(dev)))
			return -EFAULT;

		/* Find PHY device */
		phy_dev = get_eom_phy_device(dev.type, dev.index, dev.vendor_id, dev.device_id);
		if (!phy_dev)
			return -ENODEV;

		mutex_lock(&eom_context_list_lock);
		list_for_each_entry_safe(ctx, tmp_ctx, &eom_context_list, list) {
			mutex_lock(&ctx->lock);
			if (ctx->phy_dev != phy_dev) {
				mutex_unlock(&ctx->lock);
				continue;
			}

			for (i = 0; i < ctx->num_lanes; i++) {
				if (ctx->lanes[i]) {
					if (ctx->lanes[i]->buffer) {
						eom_buffer_free(
							ctx->lanes[i]->buffer);
						kfree(ctx->lanes[i]->buffer);
					}
					misc_deregister(
						&ctx->lanes[i]->miscdev);
					kfree(ctx->lanes[i]);
				}
			}
			mutex_unlock(&ctx->lock);
		}
		mutex_unlock(&eom_context_list_lock);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* File operations for lane device */
static const struct file_operations eom_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = eom_ioctl,
	.compat_ioctl = eom_ioctl,
};

/* Initialization */
static int __init eom_init(void)
{
	INIT_LIST_HEAD(&eom_context_list);
	mutex_init(&eom_context_list_lock);
	eom_global_miscdev.minor = MISC_DYNAMIC_MINOR;
	eom_global_miscdev.name = "eom";
	eom_global_miscdev.fops = &eom_fops;

	return misc_register(&eom_global_miscdev);
}

/* Cleanup */
static void __exit eom_exit(void)
{
	struct eom_context *ctx, *tmp_ctx;
	int i = 0;

	misc_deregister(&eom_global_miscdev);
	mutex_lock(&eom_context_list_lock);
	list_for_each_entry_safe(ctx, tmp_ctx, &eom_context_list, list) {
		mutex_lock(&ctx->lock);
		for (i = 0; i < ctx->num_lanes; i++) {
			if (ctx->lanes[i]) {
				if (ctx->lanes[i]->buffer) {
					eom_buffer_free(ctx->lanes[i]->buffer);
					kfree(ctx->lanes[i]->buffer);
				}
				misc_deregister(&ctx->lanes[i]->miscdev);
				kfree(ctx->lanes[i]);
			}
		}
		mutex_unlock(&ctx->lock);
		list_del(&ctx->list);
		kfree(ctx);
	}
	mutex_unlock(&eom_context_list_lock);
	phy_core_exit();
}

module_init(eom_init);
module_exit(eom_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EOM Kernel Module");
