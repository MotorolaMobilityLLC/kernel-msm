/*
 * platform_cs42l73.h: cs42l73 platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:	Hevendra R <hevendrax.raja.reddy@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_CS42L73_H_
#define _PLATFORM_CS42L73_H_

struct cs42l73_pdata {
	int codec_rst;
};

extern void *cs42l73_platform_data(void *info) __attribute__((weak));
#endif
