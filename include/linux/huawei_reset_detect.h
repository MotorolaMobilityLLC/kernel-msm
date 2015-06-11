/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __HUAWEI_RESET_DETECT_H__
#define __HUAWEI_RESET_DETECT_H__

#define RESET_DETECT_TAG "[reset_detect]"

/* reset magic number */
#define RESET_MAGIC_APANIC       0x504E4943	/* 'P' 'N' 'I' 'C' */
#define RESET_MAGIC_WDT_BARK     0x5742524B	/* 'W' 'B' 'R' 'K' */
#define RESET_MAGIC_THERMAL      0x54484D4C	/* 'T' 'H' 'M' 'L' */
#define RESET_MAGIC_REBOOT       0x51424F54	/* 'R' 'B' 'O' 'T' */

void set_reset_magic(int magic_number);
#endif
