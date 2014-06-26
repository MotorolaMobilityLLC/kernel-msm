/*
 * platform_max7315.h: max7315 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MAX7315_H_
#define _PLATFORM_MAX7315_H_

/* we have multiple max7315 on the board ... */
#define MAX7315_NUM 2

extern void __init *max7315_platform_data(void *info) __attribute__((weak));
#endif
