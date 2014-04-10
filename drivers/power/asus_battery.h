/*
 * linux/drivers/power/asus_battery.h
 *
 * Battery Driver for TD series
 *
 * based on tosa_battery.c
 *
 * Copyright (C) 2011 Josh Liao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ASUS_BATTERY_H
#define __ASUS_BATTERY_H

/* @CFG_REF_LAST_BAT_INFO - Refer to last saved battery info when update battery
 * firstly from booting. Such as battery life, rtc time.
 */
//#define CFG_REF_LAST_BAT_INFO	1

/* @CFG_CHECK_BAT_LOW_DETECTOR - Enable to check if has battery low detector. If
 * not, it shall take a specific ocv value as battery low happend.
 */
//#define CFG_CHECK_BAT_LOW_DETECTOR	1

/* @CFG_VF_TUNING - Enable VF tuning for ocv
 */
//#define CFG_VF_TUNING	1

#endif /* __ASUS_BATTERY_H */
extern void AfterthawProcessResumeBatteryService(void);
extern void cancelBatCapQueueBeforeSuspend(void);


