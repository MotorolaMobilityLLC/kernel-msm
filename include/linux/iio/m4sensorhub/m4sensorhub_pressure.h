/*
 * Copyright (c) 2015, Motorola, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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


#ifndef __M4SENSORHUB_PRESSURE_H__
#define __M4SENSORHUB_PRESSURE_H__

#include <linux/types.h>

/* This needs to be thought through when we
add other sensors */

enum m4sensorhub_pressure_iio_type {
	PRESSURE_TYPE_EVENT_DATA = 0,
	PRESSURE_TYPE_EVENT_FLUSH = 1,
};

struct m4sensorhub_pressure_event_data {
	int32_t pressure;
	int32_t altitude;
} __packed;

struct m4sensorhub_pressure_iio_data {
	uint8_t   type;
	struct m4sensorhub_pressure_event_data event_data;
	long long timestamp;
} __packed;

#endif /* __M4SENSORHUB_PRESSURE_H__ */
