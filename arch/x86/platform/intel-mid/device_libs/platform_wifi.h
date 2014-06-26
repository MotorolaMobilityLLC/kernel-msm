/*
 * platform_wifi.h: WiFi platform data header file
 *
 * (C) Copyright 2011 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_WIFI_H_
#define _PLATFORM_WIFI_H_

#define WIFI_SFI_GPIO_IRQ_NAME "WLAN-interrupt"
#define WIFI_SFI_GPIO_ENABLE_NAME "WLAN-enable"

extern void __init *wifi_platform_data(void *info) __attribute__((weak));
extern void wifi_platform_data_fastirq(struct sfi_device_table_entry *pe,
				       struct devs_id *dev) __attribute__((weak));

#endif
