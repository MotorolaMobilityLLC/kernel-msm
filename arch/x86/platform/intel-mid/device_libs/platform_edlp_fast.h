/*
 * platform_edlp_fast.h: hsi EDLP platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_EDLP_FAST_H_
#define _PLATFORM_EDLP_FAST_H_

extern void *edlp_fast_platform_data(void *data) __attribute__((weak));

extern void sfi_handle_edlp_fast_dev(struct sfi_device_table_entry *pentry,
							struct devs_id *dev);
#endif
