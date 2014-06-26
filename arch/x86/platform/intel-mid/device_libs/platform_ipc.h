/*
 * platform_ipc.h: IPC platform library header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_IPC_H_
#define _PLATFORM_IPC_H_

extern void ipc_device_handler(struct sfi_device_table_entry *pentry,
			struct devs_id *dev) __attribute__((weak));
#endif
