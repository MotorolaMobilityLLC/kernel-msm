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

#ifndef _M4SENSORHUB_FUSION_IIO_H
#define _M4SENSORHUB_FUSION_IIO_H

enum m4sensorhub_fusion_iio_type {
	FUSION_TYPE_LINEAR_ACCELERATION = 0,
	FUSION_TYPE_GRAVITY = 1,
	FUSION_TYPE_ROTATION = 2,
	FUSION_TYPE_FLUSH = 3,
	/* FUSION_TYPE_NONE is needed for flush by HAL
	 * because HAL always expects M4FUS_NUM_FUSION_BUFFERS
	 * iio buffers to be sent. In case of flush we only
	 * set the first buffer to FUSION_TYPE_FLUSH and the
	 * rest are set to FUSION_TYPE_NONE
	 */
	FUSION_TYPE_NONE = 4,
};

struct m4sensorhub_fusion_iio_data {
	uint8_t         type;  /* NOTE: sizeof(enum) can vary but is often 4 */
	int32_t         values[5];  /* NOTE: this can be a maximum of 5 */
	long long       timestamp;
} __packed;

#define M4FUS_DRIVER_NAME           "m4sensorhub_fusion"
#define M4FUS_DATA_STRUCT_SIZE_BITS \
	(sizeof(struct m4sensorhub_fusion_iio_data) * 8)

#define M4FUS_NUM_FUSION_BUFFERS 3

#endif /* _M4SENSORHUB_FUSION_IIO_H */
