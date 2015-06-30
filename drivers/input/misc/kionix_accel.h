/* include/linux/input/kionix_accel.h - Kionix accelerometer driver
 *
 * Copyright (C) 2012-2015 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef __KIONIX_ACCEL_H__
#define __KIONIX_ACCEL_H__

/* POWER SUPPLY VOLTAGE RANGE */
#define KIONIX_VDD_MIN_UV  2000000
#define KIONIX_VDD_MAX_UV  3300000
#define KIONIX_VIO_MIN_UV  1750000
#define KIONIX_VIO_MAX_UV  1950000
/* Polling delay in msecs */
#define POLL_INTERVAL_MIN_MS	1
#define POLL_INTERVAL_MAX_MS	10000
#define POLL_DEFAULT_INTERVAL_MS 200
#define KIONIX_ACCEL_MAX_DELAY 1000
#define KIONIX_ACCEL_MIN_DELAY 1

#define KIONIX_ACCEL_I2C_ADDR		0xe
#define KIONIX_ACCEL_NAME			"accelerometer"
#define KIONIX_ACCEL_IRQ			"kionix-irq"

struct kionix_accel_platform_data {
	int (*acc_power)(unsigned char onoff);
	int (*acc_init)(void);
	void (*acc_exit)(void);
	int (*acc_power_on)(bool);
	/* Although the accelerometer can perform at high ODR,
	 * there is a need to keep the maximum ODR to a lower
	 * value due to power consumption or other concern.
	 * Use this variable to set the minimum allowable
	 * interval for data to be reported from the
	 * accelerometer. Unit is measured in milli-
	 * seconds. Recommended value is 5ms. */
	unsigned int min_interval;
	/* Use this variable to set the default interval for
	 * data to be reported from the accelerometer. This
	 * value will be used during driver setup process,
	 * but can be changed by the system during runtime via
	 * sysfs control. Recommended value is 200ms.*/
	unsigned int poll_interval;

	/* This variable controls the corresponding direction
	 * of the accelerometer that is mounted on the board
	 * of the device. Refer to the porting guide for
	 * details. Valid value is 1 to 8. */
	u8 accel_direction;

	/* Use this variable to choose whether or not to use
	 * DRDY hardware interrupt mode to trigger a data
	 * report event instead of using software polling.
	 * Note that for those accelerometer model that does
	 * not support DRDY hardware interrupt, the driver
	 * will revert to software polling mode automatically.
	 * Valid value is 0 or 1.*/
	bool accel_irq_use_drdy;

	/* Use this variable to control the number of
	 * effective bits of the accelerometer output.
	 * Use the macro definition to select the desired
	 * number of effective bits. */
	#define KIONIX_ACCEL_RES_12BIT	0
	#define KIONIX_ACCEL_RES_8BIT	1
	#define KIONIX_ACCEL_RES_6BIT	2
	u8 accel_res;

	/* Use this variable to control the G range of
	 * the accelerometer output. Use the macro definition
	 * to select the desired G range.*/
	#define KIONIX_ACCEL_G_2G		0
	#define KIONIX_ACCEL_G_4G		1
	#define KIONIX_ACCEL_G_6G		2
	#define KIONIX_ACCEL_G_8G		3
	u8 accel_g_range;

	/* Optional callback functions that can be implemented
	 * on per product basis. If these callbacks are defined,
	 * they will be called by the driver. */
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};
#endif  /* __KIONIX_ACCEL_H__ */
