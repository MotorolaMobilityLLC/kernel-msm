/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef EOM_IOCTL_H
#define EOM_IOCTL_H

#include <linux/types.h>

/* Ioctl commands */
#define EOM_IOCTL_SELECT_DEVICE _IOW('E', 1, struct eom_select_device)
#define EOM_IOCTL_START_EOM _IO('E', 2)
#define EOM_IOCTL_SET_EVENTFD _IOW('E', 3, int)
#define EOM_IOCTL_STOP_EOM _IO('E', 0x07)
#define EOM_IOCTL_CLEAN_EOM _IOW('E', 0x08, struct eom_select_device)

/* Enum for supported EOM device types */
enum {
	TYPE_DUMMY = 0,
	TYPE_PCIE  = 1,
	TYPE_USB,
	/* Always keep this last for array size */
	TYPE_MAX
};

/* Mapping of enum values to strings */
static const char *eom_device_names[TYPE_MAX] = {
	[TYPE_DUMMY] = "dummy",
	[TYPE_PCIE] = "pcie",
	[TYPE_USB] = "usb",
};

struct eom_select_device {
	/* PCIe or USB device name */
	char name[32];
	__u8 type;
	__u16 index;
	__u32 vendor_id;
	__u32 device_id;
	__u32 dwell_time_us;
};

#endif
