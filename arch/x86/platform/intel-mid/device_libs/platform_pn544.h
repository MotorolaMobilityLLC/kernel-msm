/*
 * platform_pn544.h: pn544 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_PN544_H_
#define _PLATFORM_PN544_H_

/* MFLD NFC controller (PN544) platform init */
#define NFC_HOST_INT_GPIO               "NFC-intr"
#define NFC_ENABLE_GPIO                 "NFC-enable"
#define NFC_FW_RESET_GPIO               "NFC-reset"
extern void *pn544_platform_data(void *info) __attribute__((weak));
#endif
