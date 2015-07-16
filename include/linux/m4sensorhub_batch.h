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

#ifndef __M4SENSORHUB_BATCH_H__
#define __M4SENSORHUB_BATCH_H__

#ifdef __KERNEL__

enum m4sensorhub_batch_sensor_type {
	M4SH_BATCH_SENSOR_TYPE_ACCEL = 0,
	M4SH_BATCH_SENSOR_TYPE_GYRO,
	M4SH_BATCH_SENSOR_TYPE_MAX
};

/*
 * IMPORTANT: This enum should be in sync with m4
 */
enum m4sensorhub_batch_command {
	M4SH_BATCH_CMD_DATA,
	M4SH_BATCH_CMD_FLUSH,
	M4SH_BATCH_CMD_INT_HOST,
	M4SH_BATCH_CMD_MAX
};

/*
 * IMPORTANT: This struct should be in sync with m4
 */
struct m4sensorhub_batch_sample {
	u8 sensor_id;
	u8 cmd_id;
	u8 batch_enabled;
	u8 padding;
	s32 sensor_data[3];
	u32 ts_delta;
};

int m4sensorhub_batch_register_data_callback(u8 sensor_type, void *priv_data,
	void (*data_callback)(void *batch_event_data, void *priv_data,
		int64_t monobase, int num_events));

int m4sensorhub_batch_unregister_data_callback(u8 sensor_type,
	void (*data_callback)(void *batch_event_data, void *priv_data,
		int64_t monobase, int num_events));
#endif /* __KERNEL__ */
#endif  /* __M4SENSORHUB_BATCH_H__ */
