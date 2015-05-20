#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>

#include <linux/sentral-iio.h>

// I2C
static int sentral_read_byte(struct sentral_device *sentral, u8 reg)
{
	int rc;

	LOGD(&sentral->client->dev, "read byte: reg: 0x%02X\n", reg);

	rc = i2c_smbus_read_byte_data(sentral->client, reg);
	return rc;
}

static int sentral_write_byte(struct sentral_device *sentral, u8 reg, u8 value)
{
	int rc;

	LOGD(&sentral->client->dev, "write byte: reg: 0x%02X, value: 0x%02X\n",
			reg, value);

	rc = i2c_smbus_write_byte_data(sentral->client, reg, value);
	return rc;
}

static int sentral_write_block(struct sentral_device *sentral, u8 reg,
		void *buffer, size_t count)
{
	char dstr[I2C_BLOCK_SIZE_MAX * 5 + 1];
	size_t dstr_len = 0;
	int i;
	int rc;
	int total = 0;

	u8 xfer_count;

	while (count) {
		xfer_count = MIN(I2C_BLOCK_SIZE_MAX, count);
		rc = i2c_smbus_write_i2c_block_data(sentral->client, reg, xfer_count,
				(u8 *)buffer + total);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "write block error: %d\n", rc);
			return rc;
		}

		LOGD(&sentral->client->dev,
				"write block: reg: 0x%02X, count: %zu, rc: %d\n", reg, count,
				rc);

		for (i = 0, dstr_len = 0; i < xfer_count; i++) {
			dstr_len += scnprintf(dstr + dstr_len, PAGE_SIZE - dstr_len,
					" 0x%02X", *((u8 *)(buffer + total + i)));
		}
		LOGD(&sentral->client->dev, "write block bytes:%s\n", dstr);

		reg += xfer_count;
		count -= xfer_count;
		total += xfer_count;

	}
	return total;
}

static int sentral_read_block(struct sentral_device *sentral, u8 reg,
		void *buffer, size_t count)
{
	int rc = 0;
	int total = 0;
	char dstr[I2C_BLOCK_SIZE_MAX * 5 + 1];
	size_t dstr_len = 0;
	int i;
	u8 xfer_count;

	while (count) {
		xfer_count = MIN(I2C_BLOCK_SIZE_MAX, count);
		rc = i2c_smbus_read_i2c_block_data(sentral->client, reg, xfer_count,
				(u8 *)buffer + total);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "read block error: %d\n", rc);
			return rc;
		}

		LOGD(&sentral->client->dev,
				"read block: reg: 0x%02X, count: %zu, rc: %d\n", reg, count,
				rc);

		for (i = 0, dstr_len = 0; i < xfer_count; i++) {
			dstr_len += scnprintf(dstr + dstr_len, PAGE_SIZE - dstr_len,
					" 0x%02X", *((u8 *)(buffer + total + i)));
		}
		LOGD(&sentral->client->dev, "read block bytes:%s\n", dstr);

		reg += rc;
		count -= rc;
		total += rc;

	}
	return total;
}

// misc

static u64 sentral_get_ktime_us(void)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return (u64)(tv.tv_sec) * USEC_PER_SEC + tv.tv_usec;
}

static int sentral_parameter_read(struct sentral_device *sentral,
		u8 page_number, u8 param_number, void *param, size_t size)
{
	int rc;
	int i;

	if (size > PARAM_READ_SIZE_MAX)
		return -EINVAL;

	// select page
	rc = sentral_write_byte(sentral, SR_PARAM_PAGE, page_number);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) selecting parameter page: %u\n",
				rc, page_number);

		goto exit_error_page;
	}

	// select param number
	rc = sentral_write_byte(sentral, SR_PARAM_REQ, param_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) selecting parameter number: %u\n", rc,
				param_number);

		goto exit_error_param;
	}

	// wait for ack
	for (i = 0; i < PARAM_MAX_RETRY; i++) {
		usleep_range(8000, 10000);
		rc = sentral_read_byte(sentral, SR_PARAM_ACK);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) reading parameter ack\n",
					rc);

			goto exit;
		}

		if (rc == param_number)
			goto acked;
	}
	LOGE(&sentral->client->dev, "parameter ack retries (%d) exhausted\n",
			PARAM_MAX_RETRY);

	goto exit;

acked:
	// read values
	rc = sentral_read_block(sentral, SR_PARAM_SAVE, param, size);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading parameter data\n", rc);
		goto exit;
	}
	rc = 0;

exit:
	(void)sentral_write_byte(sentral, SR_PARAM_PAGE, 0);
exit_error_param:
	(void)sentral_write_byte(sentral, SR_PARAM_REQ, 0);
exit_error_page:
	return rc;
}

static int sentral_parameter_write(struct sentral_device *sentral,
		u8 page_number, u8 param_number, void *param, size_t size)
{
	int rc;
	int i;

	if (size > PARAM_WRITE_SIZE_MAX)
		return -EINVAL;

	// select page
	rc = sentral_write_byte(sentral, SR_PARAM_PAGE, page_number);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) selecting parameter page: %u\n",
			rc, page_number);

		goto exit_error_page;
	}

	// write values
	rc = sentral_write_block(sentral, SR_PARAM_LOAD, param, size);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) writing parameter data\n", rc);
		goto exit_error_page;
	}

	// select param number
	param_number |= 0x80;
	rc = sentral_write_byte(sentral, SR_PARAM_REQ, param_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) selecting parameter number: %u\n", rc,
				param_number);

		goto exit_error_param;
	}

	// wait for ack
	for (i = 0; i < PARAM_MAX_RETRY; i++) {
		usleep_range(8000, 10000);
		rc = sentral_read_byte(sentral, SR_PARAM_ACK);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) reading parameter ack\n",
					rc);

			goto exit;
		}

		if (rc == param_number)
			goto acked;
	}
	LOGE(&sentral->client->dev, "parameter ack retries (%d) exhausted\n",
			PARAM_MAX_RETRY);

	goto exit;

acked:
	rc = 0;

exit:
	(void)sentral_write_byte(sentral, SR_PARAM_PAGE, 0);
exit_error_param:
	(void)sentral_write_byte(sentral, SR_PARAM_REQ, 0);
exit_error_page:
	return rc;
}

// Sensors

static int sentral_sensor_config_read(struct sentral_device *sentral, u8 id,
		struct sentral_param_sensor_config *config)
{
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SENSORS,
			id + PARAM_SENSORS_ACTUAL_OFFSET,
			(void *)config, sizeof(struct sentral_param_sensor_config));

	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) reading sensor parameter for sensor id: %u\n",
				rc, id);

		return rc;
	}

	return 0;
}

static int sentral_sensor_config_write(struct sentral_device *sentral, u8 id,
		struct sentral_param_sensor_config *config)
{
	int rc;

	rc = sentral_parameter_write(sentral, SPP_SENSORS,
			id + PARAM_SENSORS_ACTUAL_OFFSET,
			(void *)config, sizeof(struct sentral_param_sensor_config));

	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) writing sensor parameter for sensor id: %u\n",
				rc, id);

		return rc;
	}

	// update current sensor config
	memcpy(&sentral->sensor_config[id], config,
			sizeof(struct sentral_param_sensor_config));

	return 0;
}

static int sentral_sensor_config_restore(struct sentral_device *sentral)
{
	int i;
	int rc = 0;

	if (sentral->enabled_mask) {
		for (i = 0; i < SST_MAX; i++) {
			if (!(sentral->enabled_mask & (1LL << i)))
				continue;

			LOGI(&sentral->client->dev, "restoring state for sensor id: %d\n",
					i);

			rc = sentral_sensor_config_write(sentral, i,
					&sentral->sensor_config[i]);

			if (rc) {
				LOGE(&sentral->client->dev,
						"error (%d) restoring sensor config for sensor id: %d\n",
						rc, i);
				return rc;
			}
		}
	}

	return 0;
}

static int sentral_sensor_info_read(struct sentral_device *sentral, u8 id,
		struct sentral_param_sensor_info *info)
{
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SENSORS, id, (void *)info,
			sizeof(struct sentral_param_sensor_info));

	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) reading sensor parameter for sensor id: %u\n",
				rc, id);

		return rc;
	}

	return 0;
}

static int sentral_sensor_id_is_valid(u8 id)
{
	return ((id >= SST_FIRST) && (id < SST_MAX));
}

static int sentral_sensor_batch_set(struct sentral_device *sentral, u8 id,
		u16 rate, u16 timeout_ms)
{
	int rc;
	struct sentral_param_sensor_config config = {
		.sample_rate = rate,
		.max_report_latency = timeout_ms,
		.change_sensitivity = 0,
		.dynamic_range = 0,
	};

	LOGD(&sentral->client->dev, "batch set id: %u, rate: %u, timeout: %u\n", id,
			rate, timeout_ms);

	if (!sentral_sensor_id_is_valid(id))
		return -EINVAL;

	// update config
	rc = sentral_sensor_config_write(sentral, id, &config);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) writing sensor config for sensor id: %u\n",
				rc, id);
		return rc;
	}

	return 0;
}

static int sentral_sensor_enable_set(struct sentral_device *sentral, u8 id,
		bool enable)
{
	LOGD(&sentral->client->dev, "enable set id: %u, enable: %u\n", id, enable);

	if (enable) {
		sentral->enabled_mask |= 1LL << id;
	} else {
		sentral->enabled_mask &= ~(1LL << id);
		return sentral_sensor_batch_set(sentral, id, 0, 0);
	}

	return 0;
}

static int sentral_request_reset(struct sentral_device *sentral)
{
	int rc;

	LOGI(&sentral->client->dev, "reset request\n");
	rc = sentral_write_byte(sentral, SR_RESET_REQ, 1);
	return rc;
}

static int sentral_log_meta_event(struct sentral_device *sentral,
		struct sentral_data_meta *data)
{
	if (data->event_id >= SEN_META_MAX) {
		LOGE(&sentral->client->dev, "Invalid meta event received: 0x%02X\n",
				data->event_id);

		return -EINVAL;
	}

	LOGI(&sentral->client->dev, "Meta Event: %s { 0x%02X, 0x%02X }\n",
			sentral_meta_event_strings[data->event_id], data->byte_1,
			data->byte_2);

	return 0;
}

// IIO

static int sentral_iio_buffer_push(struct sentral_device *sentral,
		u8 sensor_id, void *data, size_t bytes)
{
	u8 buffer[24] = { 0 };
	int rc;
	int i;
	char dstr[sizeof(buffer) * 5 + 1];
	size_t dstr_len = 0;
	u16 sensor_id_u16 = (u16)sensor_id;

	// sensor id 0-1
	memcpy(&buffer[0], &sensor_id_u16, sizeof(sensor_id_u16));

	// data 2-15
	if (bytes > 0)
		memcpy(&buffer[2], data, bytes);

	// timestamp 16-23
	memcpy(&buffer[16], &sentral->ts_sensor_utime, sizeof(u64));

	for (i = 0, dstr_len = 0; i < sizeof(buffer); i++) {
		dstr_len += scnprintf(dstr + dstr_len, PAGE_SIZE - dstr_len,
				" 0x%02X", buffer[i]);
	}
	LOGD(&sentral->client->dev, "iio buffer bytes: %s\n", dstr);

	if (sensor_id == SST_ACCELEROMETER)
		memcpy(&sentral->latest_accel_buffer, buffer, sizeof(buffer));

	rc = iio_push_to_buffers(sentral->indio_dev, buffer);
	if (rc)
		LOGE(&sentral->client->dev, "error (%d) pushing to IIO buffers", rc);

	return rc;
}

// FIFO

static int sentral_fifo_flush(struct sentral_device *sentral, u8 sensor_id)
{
	int rc;

	LOGI(&sentral->client->dev, "FIFO flush sensor ID: 0x%02X\n", sensor_id);
	mutex_lock(&sentral->lock_flush);
	rc = sentral_write_byte(sentral, SR_FIFO_FLUSH, sensor_id);
	return rc;
}

static int sentral_fifo_get_bytes_remaining(struct sentral_device *sentral)
{
	int rc;
	u16 bytes;

	rc = sentral_read_block(sentral, SR_FIFO_BYTES, (void *)&bytes,
			sizeof(bytes));

	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading FIFO bytes remaining\n",
				rc);

		return rc;
	}

	return bytes;
}

static int sentral_fifo_parse(struct sentral_device *sentral, u8 *buffer,
		size_t bytes)
{
	u8 sensor_id;
	size_t data_size;

	while (bytes) {
		// get sensor id
		sensor_id = *buffer++;
		bytes--;
		data_size = 0;

		switch (sensor_id) {

		case SST_TIMESTAMP_MSW:
			{
				u16 ts = *(u16 *)buffer;
				u16 *ts_ptr = (u16 *)&sentral->ts_sensor_stime;
				ts_ptr[0] = ts;

				buffer += sizeof(u16);
				bytes -= sizeof(u16);
				continue;
			}
			break;

		case SST_TIMESTAMP_LSW:
			{
				u16 ts = *(u16 *)buffer;
				u32 dt_stime;
				u64 dt_utime;

				u16 *ts_ptr = (u16 *)&sentral->ts_sensor_stime;
				ts_ptr[1] = ts;

				sentral->ts_irq_stime = sentral->ts_sensor_stime;
				dt_stime = sentral->ts_sensor_stime - sentral->ts_irq_stime;
				dt_utime = ((u64)dt_stime * SENTRAL_SENSOR_TIMESTAMP_SCALE_NS)
						/ 1000;

				sentral->ts_sensor_utime = sentral->ts_irq_utime + dt_utime;

				buffer += sizeof(u16);
				bytes -= sizeof(u16);
				continue;
			}
			break;

		case SST_NOP:
			continue;

		case SST_SIGNIFICANT_MOTION:
		case SST_STEP_DETECTOR:
		case SST_TILT_DETECTOR:
		case SST_WAKE_GESTURE:
		case SST_GLANCE_GESTURE:
		case SST_PICK_UP_GESTURE:
		case SST_WRIST_TILT_GESTURE:
		case SST_INACTIVITY_ALARM:
			if (sensor_id == SST_WRIST_TILT_GESTURE)
				LOGI(&sentral->client->dev, "get a WRIST_TILT_GESTURE event");
			data_size = 0;
			break;

		case SST_HEART_RATE:
			data_size = 1;
			break;

		case SST_LIGHT:
		case SST_PROXIMITY:
		case SST_RELATIVE_HUMIDITY:
		case SST_STEP_COUNTER:
		case SST_TEMPERATURE:
		case SST_AMBIENT_TEMPERATURE:
		case SST_ACTIVITY:
			data_size = 2;
			break;

		case SST_PRESSURE:
		case SST_COACH:
			data_size = 3;
			break;

		case SST_ACCELEROMETER:
		case SST_GEOMAGNETIC_FIELD:
		case SST_ORIENTATION:
		case SST_GYROSCOPE:
		case SST_GRAVITY:
		case SST_LINEAR_ACCELERATION:
			data_size = 7;
			break;

		case SST_ROTATION_VECTOR:
		case SST_GAME_ROTATION_VECTOR:
		case SST_GEOMAGNETIC_ROTATION_VECTOR:
			data_size = 10;
			break;

		case SST_MAGNETIC_FIELD_UNCALIBRATED:
		case SST_GYROSCOPE_UNCALIBRATED:
			data_size = 13;
			break;

		case SST_META_EVENT:
			{
				struct sentral_data_meta *meta_data =
						(struct sentral_data_meta *)buffer;

				if (sentral_log_meta_event(sentral, meta_data))
					LOGE(&sentral->client->dev, "error parsing meta event\n");

				// push flush complete events
				if (meta_data->event_id == SEN_META_FLUSH_COMPLETE) {
					mutex_unlock(&sentral->lock_flush);
					sentral_iio_buffer_push(sentral, sensor_id,
							(void *)&meta_data->byte_1,
							sizeof(meta_data->byte_1));
				}

				// restore sensors on init
				if (meta_data->event_id == SEN_META_INITIALIZED)
					(void)sentral_sensor_config_restore(sentral);

				buffer += 3;
				bytes -= 3;
				continue;
			}
			break;

		default:
			LOGE(&sentral->client->dev, "invalid sensor type: %u\n", sensor_id);
			return -EINVAL;
		}

		sentral_iio_buffer_push(sentral, sensor_id, (void *)buffer, data_size);

		buffer += data_size;
		bytes -= data_size;

	}
	return 0;
}

static int sentral_fifo_read_block(struct sentral_device *sentral, u8 *buffer,
		size_t bytes)
{
	int rc;
	size_t bytes_read = 0;
	size_t bytes_to_read = 0;
	u8 fifo_offset;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	while (bytes) {
		bytes_to_read = I2C_BLOCK_SIZE_MAX;
		if ((bytes_read % SENTRAL_FIFO_BLOCK_SIZE + I2C_BLOCK_SIZE_MAX)
					> SENTRAL_FIFO_BLOCK_SIZE) {
			bytes_to_read = SENTRAL_FIFO_BLOCK_SIZE - bytes_read
					% SENTRAL_FIFO_BLOCK_SIZE;
		}

		bytes_to_read = MIN(bytes, bytes_to_read);
		fifo_offset = bytes_read % SENTRAL_FIFO_BLOCK_SIZE;

		LOGD(&sentral->client->dev,
				"bytes: %zu, bytes_read: %zu, offset: %u, count: %zu\n", bytes,
				bytes_read, fifo_offset, bytes_to_read);

		rc = sentral_read_block(sentral, SR_FIFO_START + fifo_offset,
				buffer + bytes_read, bytes_to_read);

		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);
			return rc;
		}

		bytes -= rc;
		bytes_read += rc;

	}

	return bytes_read;
}

static int sentral_fifo_read(struct sentral_device *sentral, u8 *buffer)
{
	int rc;
	u16 bytes_remaining = 0;
	u32 crc;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	// get bytes remaining
	rc = sentral_fifo_get_bytes_remaining(sentral);
	if (rc < 0) {
		LOGE(&sentral->client->dev,
				"error (%d) reading FIFO bytes remaining\n", rc);

		return rc;
	}
	bytes_remaining = (u16)rc;

	// check for crash
	if (bytes_remaining == 0) {
		rc = sentral_read_block(sentral, SR_CRC_HOST, (void *)&crc,
				sizeof(crc));

		LOGD(&sentral->client->dev, "empty FIFO, crc: 0x%08X\n", crc);

		// probable crash, queue reset
		if (crc == 0xFFFFFFFF) {
			LOGE(&sentral->client->dev, "crash detected, resetting ...\n");
			queue_work(sentral->sentral_wq, &sentral->work_reset);
			return -EINVAL;
		}
	}

	// get interrupt status
	while (sentral_read_byte(sentral, SR_INT_STATUS) > 0) {

		LOGD(&sentral->client->dev, "%s int status > 0\n", __func__);

		// check buffer overflow
		if (bytes_remaining > DATA_BUFFER_SIZE) {
			LOGE(&sentral->client->dev, "FIFO read buffer overflow (%u > %u)\n",
					bytes_remaining, DATA_BUFFER_SIZE);

			return -EINVAL;
		}

		// read FIFO
		rc = sentral_fifo_read_block(sentral, buffer, bytes_remaining);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);
			return rc;
		}

		// parse buffer
		rc = sentral_fifo_parse(sentral, buffer, bytes_remaining);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) parsing FIFO\n", rc);
			return rc;
		}

		// get bytes remaining
		rc = sentral_fifo_get_bytes_remaining(sentral);
		if (rc < 0) {
			LOGE(&sentral->client->dev,
					"error (%d) reading FIFO bytes remaining\n", rc);

			return rc;
		}
		bytes_remaining = (u16)rc;

	}

	return 0;
}

static void sentral_do_work_fifo_read(struct work_struct *work)
{
	struct sentral_device *sentral = container_of(work, struct sentral_device,
			work_fifo_read);

	int rc;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	rc = sentral_fifo_read(sentral, (void *)sentral->data_buffer);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);
		return;
	}
	queue_delayed_work(sentral->sentral_wq, &sentral->work_watchdog,
			msecs_to_jiffies(SENTRAL_WATCHDOG_WORK_MSECS));
}

static int sentral_set_register_flag(struct sentral_device *sentral, u8 reg,
		u8 flag, bool enable)
{
	int rc;
	u8 value;

	LOGD(&sentral->client->dev, "setting register 0x%02X flag 0x%02X to %u\n",
			reg, flag, enable);

	rc = sentral_read_byte(sentral, reg);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading register 0x%02X\n", rc,
				reg);

		return rc;
	}

	value = rc & ~(flag);
	if (enable)
		value |= flag;

	if (value == rc)
		return 0;

	rc = sentral_write_byte(sentral, reg, value);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) setting register 0x%02X to 0x%02X\n", rc, reg,
				value);

		return rc;
	}

	return 0;
}

// chip control

static int sentral_set_chip_control_flag(struct sentral_device *sentral,
		u8 flag, bool enable)
{
	return sentral_set_register_flag(sentral, SR_CHIP_CONTROL, flag, enable);
}

static int sentral_set_cpu_run_enable(struct sentral_device *sentral,
		bool enable)
{
	LOGI(&sentral->client->dev, "setting CPU run to %s\n", ENSTR(enable));
	return sentral_set_chip_control_flag(sentral, SEN_CHIP_CTRL_CPU_RUN,
			enable);
}

static int sentral_set_host_upload_enable(struct sentral_device *sentral,
		bool enable)
{
	LOGI(&sentral->client->dev, "setting upload enable to %s\n", ENSTR(enable));
	return sentral_set_chip_control_flag(sentral, SEN_CHIP_CTRL_UPLOAD_ENABLE,
			enable);
}

// host iface control

static int sentral_set_host_iface_control_flag(struct sentral_device *sentral,
		u8 flag, bool enable)
{
	return sentral_set_register_flag(sentral, SR_HOST_CONTROL, flag, enable);
}

// ap suspend

static int sentral_set_host_ap_suspend_enable(struct sentral_device *sentral,
		bool enable)
{
	LOGI(&sentral->client->dev, "setting AP suspend to %s\n", ENSTR(enable));
	return sentral_set_host_iface_control_flag(sentral,
			SEN_HOST_CTRL_AP_SUSPENDED, enable);
}

static int sentral_firmware_load(struct sentral_device *sentral,
		const char *firmware_name)
{
	const struct firmware *fw;
	struct sentral_fw_header *fw_header;
	struct sentral_fw_cds *fw_cds;
	u32 *fw_data;
	size_t fw_data_size;
	u32 crc;

	int rc = 0;
	LOGI(&sentral->client->dev, "loading firmware: %s\n", firmware_name);

	// load fw from system
	rc = request_firmware(&fw, firmware_name, &sentral->client->dev);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) loading firmware: %s\n", rc,
				firmware_name);

		goto exit;
	}

	// check fw size too small
	if (fw->size < sizeof(*fw_header)) {
		LOGE(&sentral->client->dev, "invalid firmware image size\n");
		goto exit;
	}

	// check fw signature
	fw_header = (struct sentral_fw_header *)fw->data;
	if (fw_header->signature != FW_IMAGE_SIGNATURE) {
		LOGE(&sentral->client->dev, "invalid firmware signature\n");
		goto exit;
	}

	// check fw size too big
	if ((sizeof(*fw_header) + fw_header->text_length) > fw->size) {
		LOGE(&sentral->client->dev, "invalid firmware image size\n");
		goto exit;
	}

	fw_cds = (struct sentral_fw_cds *)(sizeof(*fw_header) + fw->data
			+ fw_header->text_length - sizeof(struct sentral_fw_cds));

	// send reset request
	rc = sentral_request_reset(sentral);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) requesting reset\n", rc);
		goto exit_release;
	}

	// enable host upload
	rc = sentral_set_host_upload_enable(sentral, true);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) enabling host upload\n", rc);
		goto exit_release;
	}

	fw_data = (u32 *)(((u8 *)fw->data) + sizeof(*fw_header));
	fw_data_size = fw->size - sizeof(*fw_header);

	while (fw_data_size) {
		u32 buf[MIN(RAM_BUF_LEN, I2C_BLOCK_SIZE_MAX) / sizeof(u32)];
		size_t ul_size = MIN(fw_data_size, sizeof(buf));
		int i;

		for (i = 0; i < ul_size / 4; i++)
			buf[i] = swab32(*fw_data++);

		rc = sentral_write_block(sentral, SR_UPLOAD_DATA, (void *)buf, ul_size);
		if (rc < 0) {
			LOGE(&sentral->client->dev, "error (%d) uploading data\n", rc);
			goto exit_release;
		}

		fw_data_size -= ul_size;
	}

	// disable host upload
	rc = sentral_set_host_upload_enable(sentral, false);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) disabling host upload\n", rc);
		goto exit_release;
	}

	// check CRC
	rc = sentral_read_block(sentral, SR_CRC_HOST, (void *)&crc, sizeof(crc));
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading host CRC\n", rc);
		goto exit_release;
	}

	LOGI(&sentral->client->dev, "host CRC: 0x%08X, fw CRC: 0x%08X\n", crc,
			fw_header->text_crc);

	if (crc != fw_header->text_crc) {
		LOGE(&sentral->client->dev,
				"invalid firmware CRC, expected 0x%08X got 0x%08X\n", crc,
				fw_header->text_crc);

		goto exit_release;
	}

	LOGI(&sentral->client->dev, "firmware CRC OK\n");

	return 0;
exit_release:
	release_firmware(fw);
exit:
	return -EINVAL;
}

// SYSFS

// chip control

static ssize_t sentral_sysfs_chip_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	struct sentral_chip_control chip_control;

	mutex_lock(&sentral->lock);
	rc = sentral_read_byte(sentral, SR_CHIP_CONTROL);
	if (rc < 0) {
		LOGE(dev, "error (%d) reading chip control\n", rc);
		goto exit;
	}

	LOGD(dev, "read chip_control: %d\n", rc);
	chip_control.byte = rc;

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"CPU Run", (chip_control.bits.cpu_run ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"Upload Enable",
			(chip_control.bits.upload_enable ? "true" : "false"));

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(chip_control, S_IRUGO, sentral_sysfs_chip_control_show,
		NULL);

// host status

static ssize_t sentral_sysfs_host_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	struct sentral_host_status host_status;

	mutex_lock(&sentral->lock);
	rc = sentral_read_byte(sentral, SR_HOST_STATUS);
	if (rc < 0) {
		LOGE(dev, "error (%d) reading host status\n", rc);
		goto exit;
	}

	LOGD(dev, "read host_status: %d\n", rc);
	host_status.byte = rc;

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"CPU Reset", (host_status.bits.cpu_reset ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"Algo Standby", (host_status.bits.algo_standby ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %u\n",
			"Host Iface ID", (host_status.bits.host_iface_id >> 2) & 0x07);

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %u\n",
			"Algo ID", (host_status.bits.algo_id >> 5) & 0x07);

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(host_status, S_IRUGO, sentral_sysfs_host_status_show, NULL);

// chip status

static ssize_t sentral_sysfs_chip_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	const char bit_strings[][20] = { "EEPROM", "EEUploadDone", "EEUploadError",
			"Idle", "NoEEPROM" };

	int i;

	mutex_lock(&sentral->lock);
	rc = sentral_read_byte(sentral, SR_CHIP_STATUS);
	if (rc < 0) {
		LOGE(dev, "error (%d) reading chip status\n", rc);
		goto exit;
	}

	LOGD(dev, "read chip_status: %d\n", rc);
	for (i = 0; i < sizeof(bit_strings) / 20; i++) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
				bit_strings[i], (rc & (1 << i) ? "true" : "false"));
	}
	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(chip_status, S_IRUGO, sentral_sysfs_chip_status_show, NULL);

// registers

static ssize_t sentral_sysfs_registers_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = SR_MAX - SR_FIRST + 1;
	ssize_t used = 0;
	u8 regs[count];
	int rc;
	int i;

	mutex_lock(&sentral->lock);
	rc = sentral_read_block(sentral, SR_FIRST, (void *)&regs, count);
	if (rc < 0)
		goto exit;

	for (i = 0; i < count; i++)
		used += scnprintf(buf + used, PAGE_SIZE - used, "0x%02X: 0x%02X\n",
				SR_FIRST + i, regs[i]);

	rc = used;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static ssize_t sentral_sysfs_registers_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u32 addr;
	u32 value;
	int rc;
	if (2 != sscanf(buf, "%i,%i", &addr, &value))
		return -EINVAL;

	if ((addr > SR_MAX) || (addr < SR_FIRST))
		return -EINVAL;

	if (value > 0xFF)
		return -EINVAL;

	mutex_lock(&sentral->lock);
	rc = sentral_write_byte(sentral, (u8)addr, (u8)value);
	if (rc)
		goto exit;

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUGO, sentral_sysfs_registers_show, sentral_sysfs_registers_store);

// sensor info

static ssize_t sentral_sysfs_sensor_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	int i;
	struct sentral_param_sensor_info sensor_info;

	mutex_lock(&sentral->lock);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-33s,%4s,%6s,%6s,%4s,%8s,%6s,%6s\n", "SensorType",
			"Ver", "Power", "Range", "Res", "MaxRate", "FRes", "FMax");

	for (i = SST_FIRST; i < SST_MAX; i++) {
		rc = sentral_sensor_info_read(sentral, i, &sensor_info);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) reading sensor info for sensor id: %u\n",
					rc, i);

			goto exit;
		}

		if (!sensor_info.driver_id)
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"%-28s (%2d),%4u,%6u,%6u,%4u,%8u,%6u,%6u\n",
				sentral_sensor_type_strings[i],
				i,
				sensor_info.driver_version,
				sensor_info.power,
				sensor_info.max_range,
				sensor_info.resolution,
				sensor_info.max_rate,
				sensor_info.fifo_reserved,
				sensor_info.fifo_max);
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(sensor_info, S_IRUGO, sentral_sysfs_sensor_info_show, NULL);

// sensor config

static ssize_t sentral_sysfs_sensor_config_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	int i;
	struct sentral_param_sensor_config sensor_config;

	mutex_lock(&sentral->lock);
	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-33s,%6s,%11s,%12s,%13s\n", "SensorType", "Rate",
			"MaxLatency", "Sensitivity", "DynamicRange");

	for (i = SST_FIRST; i < SST_MAX; i++) {
		rc = sentral_sensor_config_read(sentral, i, &sensor_config);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) reading sensor config for sensor id: %d\n",
					rc, i);

			goto exit;
		}

		if (!sentral_sensor_type_strings[i])
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"%-28s (%2d),%6u,%11u,%12u,%13u\n",
				sentral_sensor_type_strings[i],
				i,
				sensor_config.sample_rate,
				sensor_config.max_report_latency,
				sensor_config.change_sensitivity,
				sensor_config.dynamic_range);
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(sensor_config, S_IRUGO, sentral_sysfs_sensor_config_show,
		NULL);

// sensor status

static ssize_t sentral_sysfs_sensor_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	int i, j;
	struct sentral_param_sensor_status sensor_status[16];

	mutex_lock(&sentral->lock);
	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%5s%10s%10s%10s%10s%10s%10s\n", "SID", "DataAvail", "I2CNACK",
			"DevIDErr", "TransErr", "DataLost", "PowerMode");

	for (i = 0; i < 2; i++) {
		rc = sentral_parameter_read(sentral, SPP_SYS,
				SP_SYS_SENSOR_STATUS_B0 + i, (void *)&sensor_status,
				sizeof(sensor_status));

		if (rc < 0) {
			LOGE(&sentral->client->dev,
					"error (%d) reading sensor status, bank: %d\n", rc, i);

			goto exit;
		}
		for (j = 0; j < 16; j++) {
			count += scnprintf(buf + count, PAGE_SIZE - count, "%5d",
					i * 16 + j + 1);

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10s",
					TFSTR(sensor_status[j].bits.data_available));

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10s",
					TFSTR(sensor_status[j].bits.i2c_nack));

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10s",
					TFSTR(sensor_status[j].bits.device_id_error));

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10s",
					TFSTR(sensor_status[j].bits.transient_error));

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10s",
					TFSTR(sensor_status[j].bits.data_lost));

			count += scnprintf(buf + count, PAGE_SIZE - count, "%10d",
					sensor_status[j].bits.power_mode);

			count += scnprintf(buf + count, PAGE_SIZE - count, "\n");
		}
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(sensor_status, S_IRUGO, sentral_sysfs_sensor_status_show,
		NULL);

static ssize_t sentral_sysfs_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 reset;
	int rc;

	rc = kstrtou8(buf, 10, &reset);
	if (rc) {
		LOGE(dev, "error (%d) parsing value\n", rc);
		return rc;
	}

	mutex_lock(&sentral->lock);
	sentral->init_complete = false;
	queue_work(sentral->sentral_wq, &sentral->work_reset);
	mutex_unlock(&sentral->lock);

	return count;
}

static DEVICE_ATTR(reset, S_IWUGO, NULL, sentral_sysfs_reset);

// ANDROID sensor_poll_device_t method support

// activate

static ssize_t sentral_sysfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	u32 id;
	u32 enable;

	mutex_lock(&sentral->lock);
	if (2 != sscanf(buf, "%u %u", &id, &enable)) {
		rc = -EINVAL;
		goto exit;
	}

	if (!sentral_sensor_id_is_valid(id)) {
		rc = -EINVAL;
		goto exit;
	}

	LOGI(&sentral->client->dev, "setting sensor id: %u to %s\n", id,
			ENSTR(enable));

	rc = sentral_sensor_enable_set(sentral, id, enable);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) setting sensor id: %u to %s\n",
				rc, id, ENSTR(enable));

		goto exit;
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(enable, S_IWUGO, NULL, sentral_sysfs_enable_store);

// set_delay

static ssize_t sentral_sysfs_delay_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	u32 id;
	u32 delay_ms;
	u16 sample_rate = 0;

	mutex_lock(&sentral->lock);
	if (2 != sscanf(buf, "%u %u", &id, &delay_ms)) {
		LOGE(&sentral->client->dev, "invalid parameters\n");
		rc = -EINVAL;
		goto exit;
	}

	if (!sentral_sensor_id_is_valid(id)) {
		rc = -EINVAL;
		goto exit;
	}

	// convert millis to Hz
	if (delay_ms > 0) {
		delay_ms = MIN(1000, delay_ms);
		sample_rate = 1000 / delay_ms;
	}

	LOGI(&sentral->client->dev,
			"setting rate for sensor id: %u, delay_ms: %u, rate: %u Hz\n",
			id, delay_ms, sample_rate);

	rc = sentral_sensor_batch_set(sentral, id, sample_rate, 0);
	if (rc < 0)
		goto exit;

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(delay_ms, S_IWUGO, NULL, sentral_sysfs_delay_ms_store);

// batch

static ssize_t sentral_sysfs_batch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	u32 id;
	u32 flags;
	u32 delay_ms;
	u32 timeout_ms;

	u16 sample_rate = 1000/66; // default to 30Hz
	u16 inactive_time; // 5 mins

	mutex_lock(&sentral->lock);
	if (4 != sscanf(buf, "%u %u %u %u", &id, &flags, &delay_ms, &timeout_ms)) {
		rc = -EINVAL;
		goto exit;
	}

	if (!sentral_sensor_id_is_valid(id)) {
		rc = -EINVAL;
		goto exit;
	}

	// convert millis to Hz
	if (delay_ms >= 0) {
		if (id == SST_COACH) {
			// delay_ms -> coach mode setting
			switch (delay_ms) {
				case SEN_COACH_ACTIVITY_0:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)0);
					break;
				case SEN_COACH_ACTIVITY_4:
				case SEN_COACH_ACTIVITY_1: // push up
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)1);
					if (rc == 0) {
						LOGI(&sentral->client->dev,
							"setting coach success(push up): %u, delay_ms: %u",
							id, delay_ms);
						LOGI(&sentral->client->dev,
							"read back setting(push up): %d",
							sentral_read_byte(sentral,SR_COACH_SET));
					}
					break;
				case SEN_COACH_ACTIVITY_TEST1:
				case SEN_COACH_ACTIVITY_TEST2:
				case SEN_COACH_ACTIVITY_5:
				case SEN_COACH_ACTIVITY_2: // sit up
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)2);
					if (rc == 0) {
						LOGI(&sentral->client->dev,
							"setting coach success(sit up): %u, delay_ms: %u",
							id, delay_ms);
						LOGI(&sentral->client->dev,
							"read back setting(sit up): %d",
							sentral_read_byte(sentral,SR_COACH_SET));
					}
					break;
				case SEN_COACH_ACTIVITY_3:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)3);
					break;
				/*
				case SEN_COACH_ACTIVITY_4:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)4);
					break;
				case SEN_COACH_ACTIVITY_5:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)5);
					break;
				*/
				case SEN_COACH_ACTIVITY_6:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)6);
					break;
				case SEN_COACH_ACTIVITY_7:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)7);
					break;
				case SEN_COACH_ACTIVITY_8:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)8);
					break;
				case SEN_COACH_ACTIVITY_9:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)9);
					break;
				case SEN_COACH_ACTIVITY_10:
					rc = sentral_write_byte(sentral,SR_COACH_SET,(u8)10);
					break;
				default :
					rc = 1;
					goto exit;
			}
			if (rc)
				LOGE(&sentral->client->dev,
					"error (%d) setting coach ID: %u, with activity ID: %u\n",
					rc, id, delay_ms);
		} else if (id == SST_INACTIVITY_ALARM) {
			rc = sentral_parameter_read(sentral, SPP_CUSTOM_PARAM, 1, &inactive_time, 2);
			LOGI(&sentral->client->dev,
				"inactivity rate readback before setting, %d",
				inactive_time);
			// delay_ms -> inactivity alarm rate
			if (delay_ms > 9)
				inactive_time = delay_ms;
			else // similar to game mode, 0-9 set as 2-11 mins
				inactive_time = (delay_ms + 2) * 60;
			rc = sentral_parameter_write(sentral, SPP_CUSTOM_PARAM, 1, &inactive_time, 2);
			if (rc) {
				LOGE(&sentral->client->dev,
					"error (%d) inactivity alarm rate set error",
					rc);
			} else {
				LOGI(&sentral->client->dev,
					"success inactivity set at %d",
					inactive_time);
			}
			// just checking
			inactive_time = 0;
			sentral_parameter_read(sentral, SPP_CUSTOM_PARAM, 1, &inactive_time, 2);
			LOGI(&sentral->client->dev,
				"inactivity rate readback after setting, %d",
				inactive_time);
		} else {
			if (delay_ms != 0) {
				delay_ms = MIN(1000, delay_ms);
				sample_rate = 1000 / delay_ms;
			}
		}
	}

	rc = sentral_sensor_batch_set(sentral, id, sample_rate, timeout_ms);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) setting batch for sensor id: %u, rate: %u, timeout: %u\n",
				rc, id, sample_rate, timeout_ms);

		goto exit;
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(batch, S_IWUGO, NULL, sentral_sysfs_batch_store);

// flush

static ssize_t sentral_sysfs_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 sensor_id;
	int rc;

	rc = kstrtou8(buf, 10, &sensor_id);
	if (rc) {
		LOGE(dev, "error (%d) parsing value\n", rc);
		return rc;
	}

	if ((sensor_id < SST_FIRST) || (sensor_id >= SST_MAX))
		return -EINVAL;

	mutex_lock(&sentral->lock);

	LOGI(&sentral->client->dev, "flushing FIFO for sensor id: %u\n", sensor_id);

	rc = sentral_fifo_flush(sentral, sensor_id);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) writing FIFO flush\n", rc);
		goto exit;
	}

	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(flush, S_IWUGO, NULL, sentral_sysfs_flush_store);

// asus bmmi attributes

static ssize_t bmmi_chip_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;
	u8 err_EEUploadError = 0x04;

	mutex_lock(&sentral->lock);
	rc = sentral_read_byte(sentral, SR_CHIP_STATUS);
	if (rc < 0) {
		dev_err(dev, "error (%d) reading chip status\n", rc);
		rc = sprintf(buf, "0\n");
		goto exit;
	}

	dev_info(dev, "read chip_status: 0x%x\n", rc);

	if (rc & err_EEUploadError) {
		rc = sprintf(buf, "0\n");
		goto exit;
	}

	rc = sprintf(buf, "1\n");

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(bmmi_chip_status, S_IRUGO, bmmi_chip_status_show, NULL);

static int check_specific_sensor_status(struct sentral_device *sentral, int id)
{
	int rc;
	struct sentral_param_sensor_status sensor_status[16];
	u8 err = 0x01;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_SENSOR_STATUS_B0, (void *)&sensor_status, sizeof(sensor_status));
	if (rc < 0) {
		dev_err(&sentral->client->dev, "error (%d) reading sensor status, bank: 0\n", rc);
		return -EINVAL;
	}
	if (err & sensor_status[id].bits.i2c_nack ||
			err & sensor_status[id].bits.device_id_error ||
			err & sensor_status[id].bits.transient_error) {
		dev_err(&sentral->client->dev, "sensor[%d] acts abnormally, i2c: 0x%x, device_id: 0x%x, transient: 0x%x\n",
				id + 1,
				sensor_status[id].bits.i2c_nack,
				sensor_status[id].bits.device_id_error,
				sensor_status[id].bits.transient_error);
		return -EINVAL;
	}
	dev_info(&sentral->client->dev, "sensor[%d] acts well\n", id + 1);
	return 0;
}

static ssize_t bmmi_acc_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&sentral->lock);

	rc = check_specific_sensor_status(sentral, SST_ACCELEROMETER - 1);
	if (rc < 0) {
		rc = sprintf(buf, "0\n");
		goto exit;
	}
	rc = sprintf(buf, "1\n");

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(bmmi_acc_status, S_IRUGO, bmmi_acc_status_show, NULL);

static ssize_t bmmi_gyr_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&sentral->lock);

	rc = check_specific_sensor_status(sentral, SST_GYROSCOPE - 1);
	if (rc < 0) {
		rc = sprintf(buf, "0\n");
		goto exit;
	}
	rc = sprintf(buf, "1\n");

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(bmmi_gyr_status, S_IRUGO, bmmi_gyr_status_show, NULL);

static ssize_t bmmi_mag_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc;

	mutex_lock(&sentral->lock);

	rc = check_specific_sensor_status(sentral, SST_GEOMAGNETIC_FIELD - 1);
	if (rc < 0) {
		rc = sprintf(buf, "0\n");
		goto exit;
	}
	rc = sprintf(buf, "1\n");

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(bmmi_mag_status, S_IRUGO, bmmi_mag_status_show, NULL);

static int get_specific_sensor_buffer(struct sentral_device *sentral,
		int id, u8 *buffer)
{
	struct sentral_param_sensor_config config;
	int rc = 0;
	u16 original_rate;
	u16 original_latency;

	// read and store the sensor config
	rc = sentral_parameter_read(sentral, SPP_SENSORS,
			id + PARAM_SENSORS_ACTUAL_OFFSET, (void *)&config,
			sizeof(struct sentral_param_sensor_config));
	if (rc < 0) {
		dev_err(&sentral->client->dev, "error (%d) reading config: %d", rc, id);
		goto exit;
	}
	original_rate = config.sample_rate;
	original_latency = config.max_report_latency;
	dev_info(&sentral->client->dev,
			"(READ) config.sample_rate: %d, config.max_report_latency: %d",
			original_rate, original_latency);

	// set sample rate
	config.sample_rate = 200;
	config.max_report_latency = 0;
	rc = sentral_parameter_write(sentral, SPP_SENSORS,
			id + PARAM_SENSORS_ACTUAL_OFFSET, (void *)&config,
			sizeof(struct sentral_param_sensor_config));
	dev_info(&sentral->client->dev,
			"(WRITE) config.sample_rate: %d, config.max_report_latency: %d",
			config.sample_rate, config.max_report_latency);
	if (rc < 0) {
		dev_err(&sentral->client->dev, "error (%d) writing config: %d", rc, id);
		goto exit;
	}

	queue_work(sentral->sentral_wq, &sentral->work_fifo_read);
	msleep(20);

	// write back the stored sensor config
	config.sample_rate = original_rate;
	config.max_report_latency = original_latency;
	rc = sentral_parameter_write(sentral, SPP_SENSORS,
			id + PARAM_SENSORS_ACTUAL_OFFSET, (void *)&config,
			sizeof(struct sentral_param_sensor_config));
	dev_info(&sentral->client->dev,
			"(WRITE) config.sample_rate: %d, config.max_report_latency: %d",
			config.sample_rate, config.max_report_latency);
	if (rc < 0) {
		dev_err(&sentral->client->dev, "error (%d) writing config: %d", rc, id);
		goto exit;
	}

	memcpy(&buffer[0], sentral->latest_accel_buffer,
			sizeof(sentral->latest_accel_buffer));

exit:
	return rc;
}

// asus smmi attributes

static ssize_t smmi_acc_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 buffer[24];
	int x, y, z;
	int rc, i;

	for (i = 0; i < sizeof(buffer); i++)
		buffer[i] = 0;

	mutex_lock(&sentral->lock);
	rc = get_specific_sensor_buffer(sentral, SST_ACCELEROMETER, buffer);
	if (rc < 0) {
		dev_err(&sentral->client->dev, "%s, rc: %d", __func__, rc);
		rc = sprintf(buf, "0\n");
		goto exit;
	}
	x = (buffer[3] << 8) + buffer[2];
	y = (buffer[5] << 8) + buffer[4];
	z = (buffer[7] << 8) + buffer[6];
	(x >= 32768) ? (x = x - 65536) : (x = x);
	(y >= 32768) ? (y = y - 65536) : (y = y);
	(z >= 32768) ? (z = z - 65536) : (z = z);

	rc = sprintf(buf, "%d %d %d\n", x, y, z);

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(smmi_acc_rawdata, S_IRUGO, smmi_acc_rawdata_show, NULL);

static ssize_t smmi_acc_cali_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	s64 coeffs[3];
	const int COEFFS_ACCEL_OFFSET = 15;
	int rc, i;

	dev_info(&sentral->client->dev, "%s", __func__);
	mutex_lock(&sentral->lock);
	for (i = 0; i < 3; ++i) {
		rc = sentral_parameter_read(sentral, SPP_ALGO_WARM_START,
				COEFFS_ACCEL_OFFSET + i, (void *)&coeffs[i], sizeof(coeffs[i]));
		if (rc < 0) {
			dev_err(&sentral->client->dev,
					"error (%d) reading acc cal coefficients", rc);
			rc = sprintf(buf, "error (%d) reading acc cal coefficients\n", rc);
			goto exit;
		}
	}
	dev_info(&sentral->client->dev,
			"read cali coefficients: (0x%016llx,0x%016llx,0x%016llx)",
			coeffs[0], coeffs[1], coeffs[2]);
	rc = sprintf(buf, "0x%016llx 0x%016llx 0x%016llx\n",
			coeffs[0], coeffs[1], coeffs[2]);

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static ssize_t smmi_acc_cali_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	s64 coeffs[3];
	const int COEFFS_ACCEL_OFFSET = 15;
	int rc, i;

	dev_info(&sentral->client->dev, "%s", __func__);
	mutex_lock(&sentral->lock);
	if (3 != sscanf(buf, "%lld %lld %lld",
			&coeffs[0], &coeffs[1], &coeffs[2])) {
		rc = -EINVAL;
		goto exit;
	}
	for (i = 0; i < 3; ++i) {
		rc = sentral_parameter_write(sentral, SPP_ALGO_WARM_START,
				COEFFS_ACCEL_OFFSET + i, (void *)&coeffs[i], sizeof(coeffs[i]));
		if (rc < 0) {
			dev_err(&sentral->client->dev,
					"error (%d) writing acc cal coefficients", rc);
			goto exit;
		}
	}
	dev_info(&sentral->client->dev,
			"write cali coefficients: (0x%016llx,0x%016llx,0x%016llx)",
			coeffs[0], coeffs[1], coeffs[2]);
	rc = count;

exit:
	mutex_unlock(&sentral->lock);
	return rc;
}

static DEVICE_ATTR(smmi_acc_cali, S_IRUGO|S_IWUGO, smmi_acc_cali_show,
		smmi_acc_cali_store);

static struct attribute *sentral_attributes[] = {
	&dev_attr_chip_control.attr,
	&dev_attr_host_status.attr,
	&dev_attr_chip_status.attr,
	&dev_attr_registers.attr,
	&dev_attr_reset.attr,
	&dev_attr_sensor_info.attr,
	&dev_attr_sensor_config.attr,
	&dev_attr_sensor_status.attr,
	&dev_attr_enable.attr,
	&dev_attr_delay_ms.attr,
	&dev_attr_batch.attr,
	&dev_attr_flush.attr,
	&dev_attr_bmmi_chip_status.attr,
	&dev_attr_bmmi_acc_status.attr,
	&dev_attr_bmmi_gyr_status.attr,
	&dev_attr_bmmi_mag_status.attr,
	&dev_attr_smmi_acc_rawdata.attr,
	&dev_attr_smmi_acc_cali.attr,
	NULL
};

static const struct attribute_group sentral_attribute_group = {
	.attrs = sentral_attributes
};

static int sentral_class_create(struct sentral_device *sentral)
{
	int rc = 0;

	// custom sensor hub class
	sentral->hub_class = class_create(THIS_MODULE, SENTRAL_HUB_CLASS_NAME);
	if (IS_ERR(sentral->hub_class)) {
		rc = PTR_ERR(sentral->hub_class);
		LOGE(&sentral->client->dev, "error creating hub class: %d\n", rc);
		goto exit;
	}

	// custom sensor hub device
	sentral->hub_device = device_create(sentral->hub_class, NULL, 0,
			"%s", SENTRAL_HUB_DEVICE_NAME);
	if (IS_ERR(sentral->hub_device)) {
		rc = PTR_ERR(sentral->hub_device);
		LOGE(&sentral->client->dev, "error creating hub device: %d\n", rc);
		goto exit_class_created;
	}

	// set device data
	rc = dev_set_drvdata(sentral->hub_device, sentral);
	if (rc) {
		LOGE(&sentral->client->dev, "error setting device data: %d\n", rc);
		goto exit_device_created;
	}

	return 0;

exit_device_created:
	device_unregister(sentral->hub_device);
exit_class_created:
	class_destroy(sentral->hub_class);
exit:
	return rc;
}

static void sentral_class_destroy(struct sentral_device *sentral)
{
	device_unregister(sentral->hub_device);
	class_destroy(sentral->hub_class);
}

// SYSFS

static int sentral_sysfs_create(struct sentral_device *sentral)
{
	int rc = 0;

	// link iio device
	rc = sysfs_create_link(&sentral->hub_device->kobj,
			&sentral->indio_dev->dev.kobj, "iio");
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) creating device iio link\n",
				rc);

		return rc;
	}

	// create root nodes
	rc = sysfs_create_group(&sentral->hub_device->kobj,
			&sentral_attribute_group);

	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) creating device root nodes\n",
				rc);

		return rc;
	}

	return 0;
}

static void sentral_sysfs_destroy(struct sentral_device *sentral)
{

	// remove iio device link
	sysfs_remove_link(&sentral->hub_device->kobj, "iio");

	// remove group
	sysfs_remove_group(&sentral->hub_device->kobj, &sentral_attribute_group);
}

static irqreturn_t sentral_irq_handler(int irq, void *dev_id)
{
	struct sentral_device *sentral = dev_id;

	LOGD(&sentral->client->dev, "IRQ received\n");

	if (sentral->init_complete) {
		// cancel any delayed watchdog work
		if (cancel_delayed_work(&sentral->work_watchdog) == 0)
			flush_workqueue(sentral->sentral_wq);

		sentral->ts_irq_utime = sentral_get_ktime_us();
		queue_work(sentral->sentral_wq, &sentral->work_fifo_read);
	}

	return IRQ_HANDLED;
}

static void sentral_do_work_watchdog(struct work_struct *work)
{
	struct sentral_device *sentral = container_of((struct delayed_work *)work,
			struct sentral_device, work_watchdog);

	int rc;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	rc = sentral_fifo_read(sentral, (void *)sentral->data_buffer);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);
		return;
	}
}

static void sentral_do_work_reset(struct work_struct *work)
{
	struct sentral_device *sentral = container_of(work, struct sentral_device,
			work_reset);

	int rc = 0;

	mutex_lock(&sentral->lock_reset);

	// load firmware
	rc = sentral_firmware_load(sentral, sentral->platform_data.firmware);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) loading firmware\n", rc);
		return;
	}

	mdelay(100);

	// enable host run
	rc = sentral_set_cpu_run_enable(sentral, true);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) enabling cpu run\n", rc);
		return;
	}

	sentral->init_complete = true;
	mutex_unlock(&sentral->lock_reset);

	// queue a FIFO read
	queue_work(sentral->sentral_wq, &sentral->work_fifo_read);
}

// IIO

static const struct iio_buffer_setup_ops sentral_iio_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
};

static int sentral_iio_buffer_create(struct iio_dev *indio_dev)
{
	struct sentral_device *sentral = iio_priv(indio_dev);
	int rc = 0;

	indio_dev->buffer = iio_kfifo_allocate(indio_dev);
	if (!indio_dev->buffer) {
		LOGE(&sentral->client->dev, "error allocating IIO kfifo buffer\n");
		return -ENOMEM;
	}

	indio_dev->buffer->scan_timestamp = true;
	indio_dev->setup_ops = &sentral_iio_buffer_setup_ops;

	rc = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);

	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) registering IIO buffer", rc);
		goto exit_free;
	}

	iio_scan_mask_set(indio_dev, indio_dev->buffer, SEN_SCAN_U32_1);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, SEN_SCAN_U32_2);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, SEN_SCAN_U32_3);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, SEN_SCAN_U32_4);

	return rc;

exit_free:
	iio_kfifo_free(indio_dev->buffer);
	return rc;
}

static void sentral_iio_buffer_destroy(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

#define SENTRAL_IIO_CHANNEL(i) \
{\
	.type = IIO_ACCEL,\
	.indexed = 1,\
	.channel = i,\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),\
	.scan_index = i,\
	.scan_type = IIO_ST('u', 32, 32, 0),\
}

static const struct iio_chan_spec sentral_iio_channels[] = {
	SENTRAL_IIO_CHANNEL(SEN_SCAN_U32_1),
	SENTRAL_IIO_CHANNEL(SEN_SCAN_U32_2),
	SENTRAL_IIO_CHANNEL(SEN_SCAN_U32_3),
	SENTRAL_IIO_CHANNEL(SEN_SCAN_U32_4),
	IIO_CHAN_SOFT_TIMESTAMP(SEN_SCAN_TS),
};

static const struct iio_info sentral_iio_info = {
	.driver_module = THIS_MODULE,
};

static int sentral_suspend_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct sentral_device *sentral = container_of(nb, struct sentral_device,
			nb);

	int rc;

	LOGD(&sentral->client->dev, "suspend nb: %lu\n", event);

	switch (event) {

	case PM_SUSPEND_PREPARE:
		LOGI(&sentral->client->dev, "preparing to suspend ...\n");

		// notify sentral of suspend
		rc = sentral_set_host_ap_suspend_enable(sentral, true);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) setting AP suspend to true\n", rc);
		}

		disable_irq(sentral->irq);

		// flush fifo
		rc = sentral_fifo_flush(sentral, SST_ALL);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) flushing FIFO, sensor: %d\n", rc, SST_ALL);
		}

		// empty fifo
		rc = sentral_fifo_read(sentral, (void *)sentral->data_buffer);
		if (rc)
			LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);

		break;

	case PM_POST_SUSPEND:
		LOGI(&sentral->client->dev, "post suspend ...\n");

		enable_irq(sentral->irq);

		// notify sentral of wakeup
		rc = sentral_set_host_ap_suspend_enable(sentral, false);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) setting AP suspend to false\n", rc);
		}

		// queue fifo work
		queue_work(sentral->sentral_wq, &sentral->work_fifo_read);

		break;
	}

	return NOTIFY_DONE;
}

static int sentral_dt_parse(struct device *dev,
		struct sentral_platform_data *platform_data)
{
	int rc = 0;

	// IRQ
	rc = of_get_named_gpio_flags(dev->of_node, "pni,gpio-irq", 0, NULL);
	if (rc < 0)
		return rc;

	platform_data->gpio_irq = rc;

	// FW name
	rc = of_property_read_string(dev->of_node, "pni,firmware",
			&platform_data->firmware);

	if (rc)
		return rc;

	dev_info(dev, "platform_data->gpio_irq = %d\n", platform_data->gpio_irq);
	dev_info(dev, "platform_data->firmware = %s\n", platform_data->firmware);
	return 0;
}

static int sentral_probe_id(struct i2c_client *client)
{
	int rc;
	struct sentral_hw_id hw_id;

	rc = i2c_smbus_read_i2c_block_data(client, SR_PRODUCT_ID, sizeof(hw_id),
			(u8 *)&hw_id);

	if (rc < 0)
		return rc;

	LOGI(&client->dev, "Product ID: 0x%02X, Revision ID: 0x%02X\n",
			hw_id.product_id, hw_id.revision_id);

	return 0;
}

static int sentral_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct sentral_device *sentral;
	struct device *dev = &client->dev;
	struct iio_dev *indio_dev;
	int rc;

	// check i2c capabilities
	rc = i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);

	if (!rc) {
		LOGE(dev, "i2c_check_functionality error\n");
		return -ENODEV;
	}

	// probe for product id
	rc = sentral_probe_id(client);
	if (rc) {
		LOGE(dev, "error (%d) reading hardware id\n", rc);
		return -ENODEV;
	}

	// allocate iio device
	indio_dev = iio_device_alloc(sizeof(*sentral));
	if (!indio_dev) {
		LOGE(dev, "couldn't allocate IIO device\n");
		return -ENOMEM;
	}

	// set sentral data
	sentral = iio_priv(indio_dev);
	sentral->client = client;
	sentral->device_id = id;
	sentral->indio_dev = indio_dev;

	// alloc fifo data buffer
	sentral->data_buffer = devm_kzalloc(&client->dev, DATA_BUFFER_SIZE,
			GFP_KERNEL);

	if (!sentral->data_buffer) {
		LOGE(&client->dev, "error allocating data buffer\n");
		rc = -ENOMEM;
		goto error_free;
	}

	// check platform data
	if (!client->dev.of_node) {
		LOGE(&client->dev, "error loading platform data\n");
		rc = -ENODEV;
		goto error_free;
	}

	// parse device tree
	rc = sentral_dt_parse(&client->dev, &sentral->platform_data);
	if (rc) {
		LOGE(&client->dev, "error parsing device tree\n");
		rc = -ENODEV;
		goto error_free;
	}

	// request GPIO
	if (gpio_is_valid(sentral->platform_data.gpio_irq)) {
		rc = gpio_request_one(sentral->platform_data.gpio_irq, GPIOF_DIR_IN,
				"sentral-gpio-irq");

		if (rc) {
			LOGE(&client->dev, "error requesting GPIO\n");
			rc = -ENODEV;
			goto error_free;
		}
	}

	sentral->irq = client->irq = gpio_to_irq(sentral->platform_data.gpio_irq);

	// set i2c client data
	i2c_set_clientdata(client, indio_dev);

	// set iio data
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = SENTRAL_NAME;
	indio_dev->info = &sentral_iio_info;
	indio_dev->modes = INDIO_BUFFER_HARDWARE;
	indio_dev->channels = sentral_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(sentral_iio_channels);

	// iio buffer
	if (sentral_iio_buffer_create(indio_dev)) {
		LOGE(&client->dev, "IIO buffer create failed\n");
		goto error_iio_buffer;
	}

	// iio device
	if (iio_device_register(indio_dev)) {
		LOGE(&client->dev, "IIO device register failed\n");
		goto error_iio_device;
	}

	// init work callbacks
	sentral->sentral_wq = create_singlethread_workqueue(SENTRAL_WORKQUEUE_NAME);

	INIT_WORK(&sentral->work_reset, sentral_do_work_reset);
	INIT_WORK(&sentral->work_fifo_read, sentral_do_work_fifo_read);
	INIT_DELAYED_WORK(&sentral->work_watchdog, &sentral_do_work_watchdog);

	// init mutex, wakelock
	mutex_init(&sentral->lock);
	mutex_init(&sentral->lock_reset);
	mutex_init(&sentral->lock_flush);
	wake_lock_init(&sentral->w_lock, WAKE_LOCK_SUSPEND, dev_name(dev));

	// setup irq handler
	LOGI(&sentral->client->dev, "requesting IRQ: %d, GPIO: %u\n", sentral->irq,
			sentral->platform_data.gpio_irq);

	rc = devm_request_threaded_irq(&sentral->client->dev, sentral->irq, NULL,
			sentral_irq_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			dev_name(&sentral->client->dev), sentral);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) requesting irq handler\n", rc);
		return rc;
	}

	// init pm
	device_init_wakeup(dev, 1);
	sentral->nb.notifier_call = sentral_suspend_notifier;
	register_pm_notifier(&sentral->nb);

	// create custom class
	rc = sentral_class_create(sentral);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) creating sensorhub class\n",
				rc);

		goto error_class;
	}

	// create sysfs nodes
	rc = sentral_sysfs_create(sentral);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) creating sysfs objects\n", rc);
		goto error_sysfs;
	}

	// startup
	schedule_work(&sentral->work_reset);
	return 0;

error_sysfs:
	sentral_class_destroy(sentral);
error_class:
	iio_device_unregister(indio_dev);
error_iio_device:
	sentral_iio_buffer_destroy(indio_dev);
error_iio_buffer:
error_free:
	if (sentral->data_buffer)
		devm_kfree(&client->dev, sentral->data_buffer);

	iio_device_free(indio_dev);
	return -EIO;
}

static int sentral_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct sentral_device *sentral = iio_priv(indio_dev);

	disable_irq(sentral->irq);
	if (gpio_is_valid(sentral->platform_data.gpio_irq))
		gpio_free(sentral->platform_data.gpio_irq);

	cancel_work_sync(&sentral->work_fifo_read);
	cancel_delayed_work_sync(&sentral->work_watchdog);

	destroy_workqueue(sentral->sentral_wq);

	sentral_sysfs_destroy(sentral);
	sentral_class_destroy(sentral);

	iio_device_unregister(indio_dev);
	sentral_iio_buffer_destroy(indio_dev);

	if (sentral->data_buffer)
		devm_kfree(&client->dev, sentral->data_buffer);

	iio_device_free(indio_dev);
	return 0;
}

static int sentral_suspend(struct i2c_client *client, pm_message_t pmesg)
{
	LOGI(&client->dev, "entered suspend");

	if (device_may_wakeup(&client->dev)) {
		LOGI(&client->dev, "enable wakeup irq: %d", client->irq);
		enable_irq_wake(client->irq);
	}

	return 0;
}

static int sentral_resume(struct i2c_client *client)
{
	LOGI(&client->dev, "entered resume");

	if (device_may_wakeup(&client->dev)) {
		LOGI(&client->dev, "disable wakeup irq: %d", client->irq);
		disable_irq_wake(client->irq);
	}

	return 0;
}

static const struct i2c_device_id sentral_i2c_id_table[] = {
	{"em7184", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, sentral_i2c_id_table);

static const struct of_device_id sentral_of_id_table[] = {
	{.compatible = "pni,em7184"},
	{},
};
MODULE_DEVICE_TABLE(of, sentral_of_id_table);

static struct i2c_driver sentral_driver = {
	.probe = sentral_probe,
	.remove = sentral_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sentral-iio",
		.of_match_table = sentral_of_id_table,
	},
	.suspend = sentral_suspend,
	.resume = sentral_resume,
	.id_table = sentral_i2c_id_table,
};
module_i2c_driver(sentral_driver);

MODULE_AUTHOR("Jeremiah Mattison <jmattison@pnicorp.com>");
MODULE_DESCRIPTION("SENtral Sensor Hub Driver");
MODULE_LICENSE("GPL");
