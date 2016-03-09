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


#ifndef __M4SENSORHUB_PPG_H__
#define __M4SENSORHUB_PPG_H__

#include <linux/types.h>

enum m4sensorhub_ppg_iio_type {
	PPG_TYPE_EVENT_DATA = 0,
	PPG_TYPE_EVENT_FLUSH = 1,
};

struct m4sensorhub_ppg_event_data {
	int32_t raw_data1;
	int32_t raw_data2;
	uint32_t calib;
	int32_t x;
	int32_t y;
	int32_t z;
} __packed;

struct m4sensorhub_ppg_iio_data {
	uint8_t type;
	struct m4sensorhub_ppg_event_data  event_data;
	long long timestamp;
} __packed;

#endif /*__M4SENSORHUB_PPG_H__ */
