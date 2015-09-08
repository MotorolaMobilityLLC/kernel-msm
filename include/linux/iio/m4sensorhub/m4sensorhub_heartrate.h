/*
 *  Copyright (C) 2014 Motorola, Inc.
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
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#ifndef _M4SENSORHUB_HEARTRATE_IIO_H
#define _M4SENSORHUB_HEARTRATE_IIO_H

enum m4sensorhub_heartrate_iio_type {
	HEARTRATE_TYPE_EVENT_DATA = 0,
	HEARTRATE_TYPE_EVENT_FLUSH = 1,
};

struct m4sensorhub_heartrate_event_data {
	uint16_t        heartrate;
	uint8_t         confidence;
	uint8_t         predictor;
} __packed;

struct m4sensorhub_heartrate_event_flush {
	uint32_t        handle;
} __packed;

struct m4sensorhub_heartrate_iio_data {
	uint8_t         type;
	union {
		struct m4sensorhub_heartrate_event_data  event_data;
		struct m4sensorhub_heartrate_event_flush event_flush;
	};
	long long       timestamp;
} __packed;

#define M4HRT_DRIVER_NAME           "m4sensorhub_heartrate"
#define M4HRT_DATA_STRUCT_SIZE_BITS \
	(sizeof(struct m4sensorhub_heartrate_iio_data) * 8)

#endif /* _M4SENSORHUB_HEARTRATE_IIO_H */
