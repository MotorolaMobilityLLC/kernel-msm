/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011, 2013, 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __LINUX_USB_DIAG_BRIDGE_H__
#define __LINUX_USB_DIAG_BRIDGE_H__

struct diag_bridge_ops {
	void *ctxt;
	void (*read_complete_cb)(void *ctxt, char *buf,
			int buf_size, int actual);
	void (*write_complete_cb)(void *ctxt, char *buf,
			int buf_size, int actual);
	int (*suspend)(void *ctxt);
	void (*resume)(void *ctxt);
};

#if IS_ENABLED(CONFIG_USB_QCOM_DIAG_BRIDGE)

extern int diag_bridge_read(int id, char *data, int size);
extern int diag_bridge_write(int id, char *data, int size);
extern int diag_bridge_open(int id, struct diag_bridge_ops *ops);
extern void diag_bridge_close(int id);

#else

static int __maybe_unused diag_bridge_read(int id, char *data, int size)
{
	return -ENODEV;
}

static int __maybe_unused diag_bridge_write(int id, char *data, int size)
{
	return -ENODEV;
}

static int __maybe_unused diag_bridge_open(int id, struct diag_bridge_ops *ops)
{
	return -ENODEV;
}

static void __maybe_unused diag_bridge_close(int id) { }

#endif

#endif
