/*
 * Copyright (c) 2015, LGE Inc. All rights reserved.
 *
 * TUSB320 USB TYPE-C Configration Controller driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TUSB320_NOTIFY_H
#define __TUSB320_NOTIFY_H

#include <linux/notifier.h>

/* Events from the Configure Channel Controller */
#define TUBS320_ATTACH_STATE_EVENT      0x0001
#define TUBS320_UFP_POWER_UPDATE_EVENT  0x0002

/**
 * Configure Channel Controller Attach State
 *
 */
#define TUBS320_NOT_ATTACH           0
#define TUBS320_DFP_ATTACH_UFP       1
#define TUBS320_UFP_ATTACH_DFP       2
#define TUBS320_ATTACH_ACC           3
#define TUBS320_ACC_AUDIO_DFP        4
#define TUBS320_ACC_AUDIO_UFP        5
#define TUBS320_ACC_DEBUG_DFP        6
#define TUBS320_ACC_DEBUG_UFP        7

/**
 * Configure Channel Controller UFP mode Power
 *
 * TUBS320_UFP_POWER_DEFAULT	500/900mA
 * TUBS320_UFP_POWER_MEDIUM	1.5A
 * TUBS320_UFP_POWER_ACC 	Charge through accessory (500mA)
 * TUBS320_UFP_POWER_HIGH	3A
 */

#define TUBS320_UFP_POWER_DEFAULT    0
#define TUBS320_UFP_POWER_MEDIUM     1
#define TUBS320_UFP_POWER_ACC        2
#define TUBS320_UFP_POWER_HIGH       3

struct tusb320_notify_param {
	u8	state;
	u8	ufp_power;
};

extern int tusb320_register_notifier(struct notifier_block *nb);
extern int tusb320_unregister_notifier(struct notifier_block *nb);

#endif
