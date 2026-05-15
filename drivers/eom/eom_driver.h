/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef EOM_DRIVER_H
#define EOM_DRIVER_H

#include <linux/delay.h>
#include <linux/eom_ioctl.h>
#include <linux/eventfd.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/phy_core.h>

#define EOM_MAX_LANES 32
#define POSITIVE_SEQUENCE 1
#define NEGATIVE_SEQUENCE 0
#define TIME_100MS_US 100000

struct eom_entry {
	int x;
	int y;
	int error_count;
};

struct eom_lane;

typedef int (*eom_sequence_fn)(struct eom_lane *lane);

struct eom_dev_sequence {
	/* Device name */
	char name[32];
	/* Type identifier (PCIE, USB, etc.) */
	u8 type;
	/* Function pointer to EOM sequence */
	eom_sequence_fn run_eom;
};

/* Per lane structure */
struct eom_lane {
	struct miscdevice miscdev;
	struct eom_buffer *buffer;
	struct eom_phy_device *phy_dev;
	struct eom_dev_sequence *seq;
	int lane_num;
	u32 dwell_time_us;
	atomic_t eom_seq_stop;
	struct eventfd_ctx *eventfd;
};

/* EOM Context */
struct eom_context {
	int index;
	u8 type;
	struct eom_phy_device *phy_dev;
	struct eom_lane *lanes[EOM_MAX_LANES];
	int num_lanes;
	/* global context list */
	struct list_head list;
	struct mutex lock;
};

static atomic_t g_eom_seq_stop = ATOMIC_INIT(0);

static inline int write_phy_reg(struct eom_phy_device *phy, u32 offset, u32 val)
{
	int ret = phy_write(phy, offset, val);

	if (ret < 0) {
		pr_err("EOM PHY Dev type %d [index %d] unable to write PHY register\n",
			phy->type, phy->index);
	}

	return ret;
}

static inline int read_phy_reg(struct eom_phy_device *phy, u32 offset, u32 *val)
{
	int ret = phy_read(phy, offset, val);

	if (ret < 0) {
		pr_err("EOM PHY Dev type %d [index %d] unable to read PHY register\n",
			phy->type, phy->index);
	}

	return ret;
}

/* EOM device sequence index enum */
enum eom_dev_sequence_index {
	EOM_DUMMY_INDEX = 0,
	EOM_PCIE_INDEX,
	EOM_USB_INDEX,
	/* Add more devices as needed */
	EOM_MAX_DEVICES
};

#endif /* EOM_DRIVER_H */
