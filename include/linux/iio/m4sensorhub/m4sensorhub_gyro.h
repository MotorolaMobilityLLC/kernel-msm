/*
 *  Copyright (C) 2015 Motorola, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _M4SENSORHUB_GYRO_IIO_H
#define _M4SENSORHUB_GYRO_IIO_H

enum m4sensorhub_gyro_iio_type {
	GYRO_TYPE_EVENT_DATA  = 0,
	GYRO_TYPE_EVENT_FLUSH = 1,
};

struct m4sensorhub_gyro_event_data {
	int32_t x;
	int32_t y;
	int32_t z;
} __packed;

struct m4sensorhub_gyro_iio_data {
	uint8_t         type;
	struct m4sensorhub_gyro_event_data event_data;
	uint32_t        seq_num;
	long long       timestamp;
} __packed;

#define M4GYR_DRIVER_NAME           "m4sensorhub_gyro"
#define M4GYR_DATA_STRUCT_SIZE_BITS \
	(sizeof(struct m4sensorhub_gyro_iio_data) * 8)

#endif /* _M4SENSORHUB_GYRO_IIO_H */
