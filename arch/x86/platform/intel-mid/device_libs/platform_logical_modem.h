/*
 * platform_logical_modem.h: logical hsi platform data header file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_LOGICAL_MODEM_H_
#define _PLATFORM_LOGICAL_MODEM_H_

#define HSI_MID_MAX_CLIENTS    5

extern void *logical_platform_data(void *data) __attribute__((weak));
#endif
