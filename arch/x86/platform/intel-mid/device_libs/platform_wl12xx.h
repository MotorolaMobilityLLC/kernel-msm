/*
 * platform_wl12xx.h: wl12xx platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_WL12XX_H_
#define _PLATFORM_WL12XX_H_

#define WL12XX_SFI_GPIO_IRQ_NAME "WLAN-interrupt"
#define WL12XX_SFI_GPIO_ENABLE_NAME "WLAN-enable"
#define ICDK_BOARD_REF_CLK 26000000
#define NCDK_BOARD_REF_CLK 38400000

extern void __init *wl12xx_platform_data(void *info) __attribute__((weak));
#endif
