/*
 * platform_modem_ctrl.h: modem control platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef __PLATFORM_MODEM_CTRL_H__
#define __PLATFORM_MODEM_CTRL_H__

#include <linux/mdm_ctrl_board.h>
#include <linux/mdm_ctrl.h>
#include <linux/sfi.h>

extern void *modem_platform_data(void *data) __attribute__((weak));

extern void sfi_handle_mdm(struct sfi_device_table_entry *pentry,
				struct devs_id *dev);
#endif
