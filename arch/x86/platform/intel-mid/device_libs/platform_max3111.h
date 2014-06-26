/*
 * platform_max3111.h: max3111 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MAX3111_H_
#define _PLATFORM_MAX3111_H_

/* REVERT ME workaround[MRFL] for invalid bus number in IAFW .25 */
#define FORCE_SPI_BUS_NUM	5
#define FORCE_CHIP_SELECT	0

extern void *max3111_platform_data(void *info) __attribute__((weak));
#endif
