/**
 * \file
 *
 * \authors   Joshua Gleaton <joshua.gleaton@treadlighttech.com>
 *
 * \brief     Linux kernel module specific data types and definitions
 *
 * \copyright (C) 2013-2014 EM Microelectronic – US, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _EM7180_H_
#define _EM7180_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include <linux/em718x_portable.h>
#include <linux/em718x_registers.h>
#include <linux/em718x_core.h>


#define SENSOR_PATH_MAX 128
#define SENSOR_NUM_ATTR 2

/** low-hz sensors track timestamps with host time, rather than device time,
	to avoid issues with timestamp multiple-wrapping */
#define SENSOR_FLAG_LOWHZ            0x00000001
/** update any listeners with the current value of this sensor on enable */
#define SENSOR_FLAG_UPDATE_ON_ENABLE 0x00000002

int em718x_init_status = -1; // add em718x init i2c driver success

struct em718x_sensor {
	struct em718x           *dev;
	struct input_dev        *input;

	/** input->phys, eg "em7180:accel" */
	char                    path[SENSOR_PATH_MAX];

	/** Short name, eg, "accel" */
	const char              *name;

	/** long description, eg EM7180 Accelerometer Sensor */
	const char              *desc;

	DI_SENSOR_TYPE_T        type;

	/** pointer to sensor_info in the DI_INSTANCE for this sensor */
	DI_SENSOR_INFO_T        *info;

	/** The rate requested via the sysfs "rate" attribute.
	 *  The actual device rate may be different than this, in which case
	 *  the dt counter will decimate the
	 *  actual rate to the requested value */
	int                     rate;

	/** samples will be dropped while dt>0. This is updated from the current @rate */
	s64                     dt;

	/** last sample timestamp (host absolute time) */
	u64                     timestamp;

	/** bitmask of SENSOR_FLAG_* */
	u32                     flags;


	/** the attributes exposed via sysfs */
	struct device_attribute attr_rate;
	struct device_attribute attr_enable;
	struct attribute*       attrs[SENSOR_NUM_ATTR+1];
	struct attribute_group  attrs_group;

	void (*notify) (struct em718x_sensor*, DI_SENSOR_INT_DATA_T* data );
};


struct em718x {
	struct device        *dev;
	struct mutex         mutex;
	int                  irq;
	/** driver_core instance */
	struct DI_INSTANCE   *di;

	u64                  timebase;

	struct em718x_sensor sensors[DST_NUM_SENSOR_TYPES];
};



#endif
