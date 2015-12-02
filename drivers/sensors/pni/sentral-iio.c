/*
 * COPYRIGHT (C) 2015 PNI SENSOR CORPORATION
 *
 * LICENSED UNDER THE APACHE LICENSE, VERSION 2.0 (THE "LICENSE");
 * YOU MAY NOT USE THIS FILE EXCEPT IN COMPLIANCE WITH THE LICENSE.
 * YOU MAY OBTAIN A COPY OF THE LICENSE AT
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY PNI SENSOR CORPORATION "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PNI SENSOR CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>

#include <linux/sentral-iio.h>

static int sentral_fifo_flush(struct sentral_device *sentral, u8 sensor_id);

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
	u8 buf[1 + count];
	struct i2c_msg msg[] = {
		{
			.addr = sentral->client->addr,
			.flags = 0,
			.len = sizeof(buf),
			.buf = buf,
		},
	};
	int rc;

	if (!count)
		return count;

	buf[0] = reg;
	memcpy(&buf[1], buffer, count);

	rc = i2c_transfer(sentral->client->adapter, msg, 1);

	if (rc != 1) {
		LOGE(&sentral->client->dev, "error (%d) writing data\n", rc);
		return rc;
	}

	return 0;
}

static int sentral_read_block(struct sentral_device *sentral, u8 reg,
		void *buffer, size_t count)
{
	struct i2c_msg msg[] = {
		{
			.addr = sentral->client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = sentral->client->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buffer,
		},
	};
	int rc;

	if (!count)
		return count;

	rc = i2c_transfer(sentral->client->adapter, msg, 2);

	if (rc != 2) {
		LOGE(&sentral->client->dev, "error (%d) reading data\n", rc);
		return rc;
	}

	return 0;
}

// misc

static u64 sentral_get_boottime_ns(void)
{
	struct timespec t;
	t.tv_sec = t.tv_nsec = 0;
	get_monotonic_boottime(&t);
	return (u64)(t.tv_sec) * NSEC_PER_SEC + t.tv_nsec;
}

static int sentral_parameter_read(struct sentral_device *sentral,
		u8 page_number, u8 param_number, void *param, size_t size)
{
	int rc;
	int i;

#ifdef SEN_DBG_PIO
	u8 dbuf[size * 5 + 1];
	size_t dcount = 0;
	u8 *dparam = (u8 *)param;
#endif /* SEN_DBG_PIO */

	if (size > PARAM_READ_SIZE_MAX)
		return -EINVAL;

#ifdef SEN_DBG_PIO
	LOGD(&sentral->client->dev, "[PIO RD] page: %u, number: %u, count: %zu\n",
			page_number, param_number, size);
#endif /* SEN_DBG_PIO */

	if (size < PARAM_READ_SIZE_MAX)
		page_number = (size << 4) | (page_number & 0x0F);

	mutex_lock(&sentral->lock_special);

	// select page
	rc = sentral_write_byte(sentral, SR_PARAM_PAGE, page_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"[PIO RD] error (%d) selecting parameter page: %u\n", rc,
				page_number);

		goto exit_error_page;
	}

	// select param number
	rc = sentral_write_byte(sentral, SR_PARAM_REQ, param_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"[PIO RD] error (%d) selecting parameter number: %u\n", rc,
				param_number);

		goto exit_error_param;
	}

	// wait for ack
	for (i = 0; i < PARAM_MAX_RETRY; i++) {
		usleep_range(8000, 10000);
		rc = sentral_read_byte(sentral, SR_PARAM_ACK);
		if (rc < 0) {
			LOGE(&sentral->client->dev,
					"[PIO RD] error (%d) reading parameter ack\n", rc);

			goto exit;
		}

		if (rc == param_number)
			goto acked;

#ifdef SEN_DBG_PIO
		LOGD(&sentral->client->dev,
				"[PIO RD] ack: 0x%02X, expected: 0x%02X\n", rc, param_number);
#endif /* SEN_DBG_PIO */
	}
	LOGE(&sentral->client->dev, "[PIO RD] parameter ack retries (%d) exhausted\n",
			PARAM_MAX_RETRY);

	rc = -EIO;
	goto exit;

acked:
	// read values
	rc = sentral_read_block(sentral, SR_PARAM_SAVE, param, size);
	if (rc < 0) {
		LOGE(&sentral->client->dev,
				"[PIO RD] error (%d) reading parameter data\n", rc);
		goto exit;
	}
	rc = 0;
#ifdef SEN_DBG_PIO
	for (i = 0; i < size; i++) {
		dcount += scnprintf(dbuf + dcount, PAGE_SIZE - dcount, "0x%02X ",
				*(dparam + i));
	}
	LOGD(&sentral->client->dev, "[PIO RD] bytes: %s\n", dbuf);
#endif /* SEN_DBG_PIO */
exit:
	(void)sentral_write_byte(sentral, SR_PARAM_PAGE, 0);
exit_error_param:
	(void)sentral_write_byte(sentral, SR_PARAM_REQ, 0);
exit_error_page:
	mutex_unlock(&sentral->lock_special);
	return rc;
}

static int sentral_parameter_write(struct sentral_device *sentral,
		u8 page_number, u8 param_number, void *param, size_t size)
{
	int rc;
	int i;

#ifdef SEN_DBG_PIO
	u8 dbuf[size * 5 + 1];
	size_t dcount = 0;
	u8 *dparam = (u8 *)param;
#endif /* SEN_DBG_PIO */

	if (size > PARAM_WRITE_SIZE_MAX)
		return -EINVAL;

#ifdef SEN_DBG_PIO
	LOGD(&sentral->client->dev,
			"[PIO WR] page: %u, number: %u, count: %zu\n",
			page_number, param_number, size);

	for (i = 0; i < size; i++) {
		dcount += scnprintf(dbuf + dcount, PAGE_SIZE - dcount, "0x%02X ",
				*(dparam + i));
	}
	LOGD(&sentral->client->dev, "[PIO WR] bytes: %s\n", dbuf);
#endif /* SEN_DBG_PIO */

	if (size < PARAM_WRITE_SIZE_MAX)
		page_number = (size << 4) | (page_number & 0x0F);

	mutex_lock(&sentral->lock_special);

	// select page
	rc = sentral_write_byte(sentral, SR_PARAM_PAGE, page_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"[PIO WR] error (%d) selecting parameter page: %u\n", rc,
				page_number);

		goto exit_error_page;
	}

	// write values
	rc = sentral_write_block(sentral, SR_PARAM_LOAD, param, size);
	if (rc < 0) {
		LOGE(&sentral->client->dev,
				"[PIO WR] error (%d) writing parameter data\n", rc);
		goto exit_error_page;
	}

	// select param number
	param_number |= 0x80;
	rc = sentral_write_byte(sentral, SR_PARAM_REQ, param_number);
	if (rc) {
		LOGE(&sentral->client->dev,
				"[PIO WR] error (%d) selecting parameter number: %u\n", rc,
				param_number);

		goto exit_error_param;
	}

	LOGD(&sentral->client->dev, "[PIO WR] Page: 0x%02X, Param: 0x%02X\n",
			page_number, param_number);

	// wait for ack
	for (i = 0; i < PARAM_MAX_RETRY; i++) {
		usleep_range(8000, 10000);
		rc = sentral_read_byte(sentral, SR_PARAM_ACK);

#ifdef SEN_DBG_PIO
		LOGD(&sentral->client->dev,
				"[PIO WR] ack: 0x%02X, expected: 0x%02X\n", rc, param_number);
#endif /* SEN_DBG_PIO */

		if (rc < 0) {
			LOGE(&sentral->client->dev,
					"[PIO WR] error (%d) reading parameter ack\n", rc);

			goto exit;
		}

		if (rc == param_number)
			goto acked;

	}
	LOGE(&sentral->client->dev,
			"[PIO WR] parameter ack retries (%d) exhausted\n", PARAM_MAX_RETRY);

	rc = -EIO;
	goto exit;

acked:
	rc = 0;

exit:
	(void)sentral_write_byte(sentral, SR_PARAM_PAGE, 0);
exit_error_param:
	(void)sentral_write_byte(sentral, SR_PARAM_REQ, 0);
exit_error_page:
	mutex_unlock(&sentral->lock_special);
	return rc;
}

static int sentral_stime_current_get(struct sentral_device *sentral, u32 *ts)
{
	struct sentral_param_timestamp stime = {{ 0 }};
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_HOST_IRQ_TS,
			(void *)&stime, sizeof(stime));
	if (rc)
		return rc;

	*ts = stime.ts.current_stime;

	return 0;
}

static int sentral_step_count_init_get(struct sentral_device *sentral,
		u16 *step)
{
	return sentral_parameter_read(sentral, SPP_ASUS, SP_ASUS_STEP_COUNT_INIT,
			(void *)step, sizeof(*step));
}

static int sentral_step_count_init_set(struct sentral_device *sentral,
		u16 step)
{
	LOGI(&sentral->client->dev, "setting step count init to: %u steps\n",
			step);

	return sentral_parameter_write(sentral, SPP_ASUS,
			SP_ASUS_STEP_COUNT_INIT, (void *)&step, sizeof(step));
}

static int sentral_step_count_set(struct sentral_device *sentral, u64 step)
{
	sentral->step_count.curr = (step & 0xFFFF);
	sentral->step_count.prev = (step & 0xFFFF);
	sentral->step_count.base = (step & 0xFFFFFFFFFFFF0000);
	sentral->step_count.total = step;

	return sentral_step_count_init_set(sentral, step);
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
			if (!test_bit(i, &sentral->enabled_mask))
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

	rc = sentral_step_count_init_set(sentral, sentral->step_count.curr);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) restoring step count\n", rc);
		return rc;
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

static int sentral_meta_event_ctrl_set(struct sentral_device *sentral,
		u8 id, bool evt_enable, bool int_enable)
{
	u64 meta_event_ctrl = 0;
	int bit_evt, bit_int;
	int rc;

	if ((id < SEN_META_FIRST) || (id >= SEN_META_MAX))
		return -EINVAL;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_META_EVENT_CONTROL,
			(void *)&meta_event_ctrl, sizeof(meta_event_ctrl));
	if (rc)
		return rc;

	bit_evt   = 1 << (((id - 1) * 2) + 1);
	bit_int   = 1 << (((id - 1) * 2));

	meta_event_ctrl &= ~(bit_evt);
	if (evt_enable)
		meta_event_ctrl |= bit_evt;

	meta_event_ctrl &= ~(bit_int);
	if (int_enable)
		meta_event_ctrl |= bit_int;

	return sentral_parameter_write(sentral, SPP_SYS, SP_SYS_META_EVENT_CONTROL,
			(void *)&meta_event_ctrl, sizeof(meta_event_ctrl));
}

static int sentral_sensor_id_is_valid(u8 id)
{
	return ((id >= SST_FIRST) && (id < SST_MAX));
}

static int sentral_inactivity_timeout_get(struct sentral_device *sentral,
		u16 *timeout_s)
{
	return sentral_parameter_read(sentral, SPP_ASUS,
			SP_ASUS_INACTIVITY_TIMEOUT, (void *)timeout_s, sizeof(timeout_s));
}

static int sentral_inactivity_timeout_set(struct sentral_device *sentral,
		u16 timeout_s)
{
	LOGI(&sentral->client->dev, "setting inactivity timeout to: %u seconds\n",
			timeout_s);

	return sentral_parameter_write(sentral, SPP_ASUS,
			SP_ASUS_INACTIVITY_TIMEOUT, (void *)&timeout_s, sizeof(timeout_s));
}

static int sentral_rate_to_fitness_id(struct sentral_device *sentral,
		u16 rate)
{
	int fitness_id = 0;
	int i;

	for (i = 1; i < ARRAY_SIZE(sentral_fitness_id_rates); i++) {
		if (rate >= sentral_fitness_id_rates[i])
			fitness_id = i;
	}

	return fitness_id;
}

static int sentral_coach_fitness_id_get(struct sentral_device *sentral,
		u8 *fitness_id)
{
	int rc = sentral_read_byte(sentral, SR_FITNESS_ID);
	if (rc < 0)
		return rc;

	*fitness_id = (u8)rc;
	return 0;
}

static int sentral_coach_fitness_id_set(struct sentral_device *sentral,
		u8 fitness_id)
{
	LOGI(&sentral->client->dev, "setting coach fitness id to : %u\n",
			fitness_id);

	return sentral_write_byte(sentral, SR_FITNESS_ID, fitness_id);
}

static int sentral_step_report_delay_get(struct sentral_device *sentral,
		u16 *report_delay_ms)
{
	return sentral_read_block(sentral, SR_STEP_REPORT, (void *)report_delay_ms,
			sizeof(*report_delay_ms));
}

static int sentral_error_get(struct sentral_device *sentral)
{
	return sentral_read_byte(sentral, SR_ERROR);
}

static int sentral_crc_get(struct sentral_device *sentral, u32 *crc)
{
	return sentral_read_block(sentral, SR_CRC_HOST, (void *)crc, sizeof(*crc));
}

static int sentral_vibrator_enable(struct sentral_device *sentral,
		u8 enable)
{
	return sentral_write_byte(sentral, SR_VIBRATOR_EN, !!enable);
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

	LOGI(&sentral->client->dev, "batch set id: %u, rate: %u, timeout: %u\n", id,
			rate, timeout_ms);

	if (!sentral_sensor_id_is_valid(id))
		return -EINVAL;

	clear_bit(id, &sentral->sensor_warmup_mask);
	if (!config.sample_rate)
		set_bit(id, &sentral->sensor_warmup_mask);

	// inactivity sensor uses rate param to set timeout value
	if (id == SST_INACTIVITY_ALARM) {
		rc = sentral_inactivity_timeout_set(sentral, rate);
		if (rc)
			return rc;

		// change rate param to a real rate
		if (rate)
			config.sample_rate = SENTRAL_INACTIVITY_RATE_HZ;
	}

	// coach sensor uses rate param to set fitness id
	if (id == SST_COACH) {
		int fitness_id = 0;

		rc = sentral_rate_to_fitness_id(sentral, rate);
		if (rc < 0)
			return rc;

		fitness_id = rc;
		rc = sentral_coach_fitness_id_set(sentral, fitness_id);
		if (rc)
			return rc;

		// change rate param to a real rate
		if (rate)
			config.sample_rate = SENTRAL_COACH_RATE_HZ;
	}

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
		set_bit(id, &sentral->enabled_mask);
	} else {
		clear_bit(id, &sentral->enabled_mask);
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

static int sentral_sensor_ref_time_get(struct sentral_device *sentral,
		struct sentral_sensor_ref_time *ref_time)
{
	int rc;

	cancel_delayed_work_sync(&sentral->work_ts_ref_reset);

	ref_time->system_ns = sentral_get_boottime_ns();

	rc = sentral_stime_current_get(sentral, &ref_time->hub_stime);
	if (rc)
		return rc;

	queue_delayed_work(sentral->sentral_wq, &sentral->work_ts_ref_reset,
			(SENTRAL_TS_REF_RESET_WORK_SECS * HZ));

	return 0;
}

static void sentral_wq_wake_flush_complete(struct sentral_device *sentral)
{
	LOGD(&sentral->client->dev, "Flush pending: %s\n",
			TFSTR(sentral->flush_pending));

	sentral->flush_complete = true;

	if (sentral->flush_pending)
		wake_up_interruptible(&sentral->wq_flush);

	sentral->flush_pending = false;
}

static void sentral_crash_reset(struct sentral_device *sentral)
{
	LOGI(&sentral->client->dev, "[CRASH] Probable crash %u detected, restarting device ...\n",
			++sentral->crash_count);

	// turn on warm restart flag
	sentral->warm_reset = true;

	// queue reset
	queue_work(sentral->sentral_wq, &sentral->work_reset);
}

// IIO

static int sentral_iio_buffer_push(struct sentral_device *sentral,
		u16 sensor_id, void *data, u64 event_ntime, size_t bytes)
{
	u8 buffer[24] = { 0 };
	int rc;
	char dstr[sizeof(buffer) * 5 + 1];
	size_t dstr_len = 0;
	int i;

	// sensor id 0-1
	memcpy(&buffer[0], &sensor_id, sizeof(sensor_id));

	// data 2-15
	if (bytes > 0)
		memcpy(&buffer[2], data, bytes);

	// timestamp 16-23
	memcpy(&buffer[16], &event_ntime, sizeof(event_ntime));

	// iio data debug
	for (i = 0, dstr_len = 0; i < sizeof(buffer); i++) {
		dstr_len += scnprintf(dstr + dstr_len, PAGE_SIZE - dstr_len,
				" 0x%02X", buffer[i]);
	}
	LOGD(&sentral->client->dev, "iio buffer bytes: %s\n", dstr);

	rc = iio_push_to_buffers(sentral->indio_dev, buffer);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) pushing to IIO buffers", rc);
		sentral_crash_reset(sentral);
	}

	return rc;
}

static int sentral_iio_buffer_push_std(struct sentral_device *sentral,
		u8 sensor_id, void *data, u32 event_stime, size_t bytes)
{
	u64 ts = 0;
	s64 dt_stime = 0;
	s64 dt_nanos = 0;
	u64 now = 0;
	int rc;

	if (sensor_id != SST_META_EVENT) {
		if (test_bit(sensor_id, &sentral->sensor_warmup_mask)) {
			LOGD(&sentral->client->dev,
					"[IIO] pending enable for sensor id %u, dropping warm-up sample ...\n",
					sensor_id);
			return 0;
		}

		if (!test_bit(sensor_id, &sentral->enabled_mask)) {
			LOGD(&sentral->client->dev,
					"[IIO] dropping sample from disabled sensor: %d\n", sensor_id);
			return 0;
		}
	}

	if (sensor_id < SST_MAX) {
		mutex_lock(&sentral->lock_ts);

		if (test_bit(sensor_id, &sentral->ts_ref_reset_mask)) {
			rc = sentral_sensor_ref_time_get(sentral, &sentral->ts_sensor_ref[sensor_id]);
			if (rc) {
				LOGE(&sentral->client->dev,
						"[TS] error (%d) retrieving sensor reference time\n", rc);
				mutex_unlock(&sentral->lock_ts);
				return rc;
			}
			LOGD(&sentral->client->dev,
					"[TS] ref time sync { sensor_id: %u, system_ns: %llu, hub_stime: %u }\n",
					sensor_id, sentral->ts_sensor_ref[sensor_id].system_ns,
					sentral->ts_sensor_ref[sensor_id].hub_stime);

			clear_bit(sensor_id, &sentral->ts_ref_reset_mask);
		}
		now = sentral_get_boottime_ns();

		dt_stime = (s64)(event_stime)
				- (s64)(sentral->ts_sensor_ref[sensor_id].hub_stime);

		if (sentral->ts_msw_offset[sensor_id])
			dt_stime += sentral->ts_msw_offset[sensor_id] * 65536;

		dt_nanos = dt_stime * sentral->stime_scale;
		ts = sentral->ts_sensor_ref[sensor_id].system_ns + dt_nanos;

		LOGD(&sentral->client->dev,
				"[TS] %u,%llu,%llu,%u,%u,%u,%u,%lld,%lld,%lld,%lld\n",
				sensor_id, now, sentral->ts_sensor_ref[sensor_id].system_ns,
				sentral->ts_sensor_ref[sensor_id].hub_stime,
				event_stime, sentral->ts_msw_offset[sensor_id],
				sentral->stime_scale, dt_stime, dt_nanos, ts, now - ts);

		mutex_unlock(&sentral->lock_ts);
	}

	return sentral_iio_buffer_push(sentral, sensor_id, data, ts, bytes);
}

static int sentral_iio_buffer_push_wrist_tilt(struct sentral_device *sentral)
{
	LOGI(&sentral->client->dev, "sending wake-up wrist-tilt event\n");
	return sentral_iio_buffer_push(sentral, SST_WRIST_TILT_GESTURE, NULL,
			sentral_get_boottime_ns(), 0);
}

static int sentral_iio_buffer_push_sig_motion(struct sentral_device *sentral)
{
	LOGI(&sentral->client->dev, "sending wake-up sig-motion event\n");
	return sentral_iio_buffer_push(sentral, SST_SIGNIFICANT_MOTION, NULL,
			sentral_get_boottime_ns(), 0);
}

// FIFO

static int sentral_fifo_ctrl_get(struct sentral_device *sentral,
		struct sentral_param_fifo_control *fifo_ctrl)
{
	return sentral_parameter_read(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)fifo_ctrl, sizeof(struct sentral_param_fifo_control));
}

static int sentral_fifo_ctrl_set(struct sentral_device *sentral,
		struct sentral_param_fifo_control *fifo_ctrl)
{
	return sentral_parameter_write(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)fifo_ctrl, sizeof(struct sentral_param_fifo_control));
}

static int sentral_fifo_watermark_set(struct sentral_device *sentral,
		u16 watermark)
{
	struct sentral_param_fifo_control fifo_ctrl = {{ 0 }};
	int rc;

	rc = sentral_fifo_ctrl_get(sentral, &fifo_ctrl);
	if (rc)
		return rc;

	fifo_ctrl.fifo.watermark = watermark;

	return sentral_fifo_ctrl_set(sentral, &fifo_ctrl);
}

static int sentral_fifo_watermark_enable(struct sentral_device *sentral,
		bool enable)
{
	int rc;

	if (enable)
		rc = sentral_fifo_watermark_set(sentral, sentral->fifo_watermark);
	else
		rc = sentral_fifo_watermark_set(sentral, 0);
	return rc;
}

static int sentral_fifo_watermark_autoset(struct sentral_device *sentral)
{
	struct sentral_param_fifo_control fifo_ctrl = {{ 0 }};
	int rc;

	// get FIFO size
	rc = sentral_fifo_ctrl_get(sentral, &fifo_ctrl);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) getting FIFO control\n", rc);
		return rc;
	}

	// set the watermark below FIFO size
	sentral->fifo_watermark = 0;
	if (fifo_ctrl.fifo.size > SENTRAL_FIFO_WATERMARK_BUFFER) {
		sentral->fifo_watermark = fifo_ctrl.fifo.size
				- SENTRAL_FIFO_WATERMARK_BUFFER;
	}

	rc = sentral_meta_event_ctrl_set(sentral, SEN_META_FIFO_WATERMARK,
			true, false);
	if (rc)
		return rc;

	if (sentral->fifo_watermark)
		return sentral_fifo_watermark_enable(sentral, true);

	return 0;
}

static int sentral_handle_special_wakeup(struct sentral_device *sentral,
		struct sentral_wake_src_count curr)
{
	struct sentral_wake_src_count prev = sentral->wake_src_prev;
	int rc;

	LOGD(&sentral->client->dev, "curr: %u, prev: %u\n", curr.byte,
			prev.byte);

	// wrist tilt
	if (curr.bits.wrist_tilt != prev.bits.wrist_tilt) {
		sentral->wake_src_prev.bits.wrist_tilt = curr.bits.wrist_tilt;
		rc = sentral_iio_buffer_push_wrist_tilt(sentral);
		if (rc)
			return rc;
	}

	// significant motion
	if (curr.bits.sig_motion != prev.bits.sig_motion) {
		sentral->wake_src_prev.bits.sig_motion = curr.bits.sig_motion;
		rc = sentral_iio_buffer_push_sig_motion(sentral);
		if (rc)
			return rc;
	}

	sentral->wake_src_prev = curr;
	return 0;
}

static int sentral_handle_meta_data(struct sentral_device *sentral,
		void *buffer)
{
	struct sentral_data_meta *data = (struct sentral_data_meta *)buffer;

	LOGI(&sentral->client->dev, "Meta Event: %s { 0x%02X, 0x%02X }\n",
			(data->event_id >= SEN_META_MAX)
					? "Unknown"
					: sentral_meta_event_strings[data->event_id],
			data->byte_1, data->byte_2);

	switch (data->event_id) {

	// push flush complete events
	case SEN_META_FLUSH_COMPLETE:
		sentral_iio_buffer_push_std(sentral, SST_META_EVENT, (void *)&data->byte_1,
				sentral->ts_sensor_stime, sizeof(data->byte_1));
		sentral_wq_wake_flush_complete(sentral);
		set_bit(data->byte_1, &sentral->ts_ref_reset_mask);
		break;

	// restore sensors on init
	case SEN_META_INITIALIZED:
		(void)sentral_fifo_watermark_autoset(sentral);
		(void)sentral_sensor_config_restore(sentral);
		sentral_wq_wake_flush_complete(sentral);
		sentral->overflow_count = 0;
		break;

	// clear enable pending filter
	case SEN_META_SAMPLE_RATE_CHANGED:
		if (data->byte_1 < SST_MAX) {
			sentral->ts_msw_meta_rate_change[data->byte_1] = sentral->ts_msw_last;
			sentral->ts_msw_offset_reset[data->byte_1] = 1;
			set_bit(data->byte_1, &sentral->ts_ref_reset_mask);
		}

		clear_bit(data->byte_1, &sentral->sensor_warmup_mask);
		break;

	// reset hub on too many overflows
	case SEN_META_FIFO_OVERFLOW:
		if (++sentral->overflow_count >= SENTRAL_FIFO_OVERFLOW_THRD) {
			LOGE(&sentral->client->dev, "FIFO overflow exceeds %d times",
					SENTRAL_FIFO_OVERFLOW_THRD);
			(void)sentral_crash_reset(sentral);
		}
		break;
	}

	return sizeof(*data);
}

static int sentral_handle_ts_msw(struct sentral_device *sentral, void *buffer)
{
	u16 ts = *(u16 *)buffer;

	mutex_lock(&sentral->lock_ts);

	sentral->ts_sensor_stime = ts << 16;

	if (ts < (sentral->ts_msw_last - 1)) {
		LOGI(&sentral->client->dev, "[TS] MSW wrapped\n");
		sentral->ts_ref_reset_mask = ULONG_MAX;
	}

	sentral->ts_msw_last = ts;

	mutex_unlock(&sentral->lock_ts);

	LOGD(&sentral->client->dev, "[TS] MSW: { ts: %5u 0x%04X }\n", ts, ts);

	return sizeof(ts);
}

static int sentral_handle_ts_lsw(struct sentral_device *sentral, void *buffer)
{
	u16 ts = *(u16 *)buffer;

	mutex_lock(&sentral->lock_ts);

	sentral->ts_sensor_stime &= 0xFFFF0000;
	sentral->ts_sensor_stime |= ts;
	sentral->ts_lsw_last = ts;

	mutex_unlock(&sentral->lock_ts);

	LOGD(&sentral->client->dev, "[TS] LSW: { ts: %5u 0x%04X }\n", ts, ts);

	return sizeof(ts);
}

static int sentral_handle_debug_data(struct sentral_device *sentral,
		void *buffer)
{
	struct sentral_data_debug *data = (struct sentral_data_debug *)buffer;
	char buf[60];
	size_t i;
	size_t count = 0;

	switch (data->attr.type) {
	case SEN_DEBUG_STRING:
		for (i = 0; (i < data->attr.length) && (i < sizeof(buf)); i++)
			count += scnprintf(buf + count, PAGE_SIZE - count, "%c",
					data->value[i]);
		break;

	case SEN_DEBUG_BINARY:
		for (i = 0; (i < data->attr.length) && (i < sizeof(buf)); i++)
			count += scnprintf(buf + count, PAGE_SIZE - count, "0x%02X ",
					data->value[i]);
		break;

	default:
		break;
	}
	dev_info(&sentral->client->dev, "Debug Event: %s\n", buf);

	return sizeof(*data);
}

static size_t sentral_handle_step_count(struct sentral_device *sentral,
		void *buffer)
{
	u16 *value = (u16 *)buffer;

	sentral->step_count.curr = *value;

	if (sentral->step_count.prev > sentral->step_count.curr)
		sentral->step_count.base += (1 << 16);

	sentral->step_count.total = sentral->step_count.base
			| sentral->step_count.curr;

	LOGI(&sentral->client->dev,
			"[SNS] Step Count { current: %u, previous: %u, base: %llu, total: %llu }\n",
			sentral->step_count.curr, sentral->step_count.prev,
			sentral->step_count.base, sentral->step_count.total);

	sentral->step_count.prev = sentral->step_count.curr;

	(void)sentral_iio_buffer_push_std(sentral, SST_STEP_COUNTER,
			(void *)&sentral->step_count.total, sentral->ts_sensor_stime,
			sizeof(sentral->step_count.total));

	return sizeof(*value);
}

static int sentral_fifo_flush(struct sentral_device *sentral, u8 sensor_id)
{
	bool prev_flush_pending;
	int rc;

	LOGI(&sentral->client->dev, "FIFO flush sensor ID: 0x%02X\n", sensor_id);

	if (sensor_id == SST_SIGNIFICANT_MOTION)
		return 0;

	mutex_lock(&sentral->lock_flush);

	prev_flush_pending = sentral->flush_pending;
	sentral->flush_pending = true;
	sentral->flush_complete = false;

	if (prev_flush_pending) {
		rc = wait_event_interruptible_timeout(sentral->wq_flush,
			sentral->flush_complete,
			msecs_to_jiffies(SENTRAL_WQ_FLUSH_TIMEOUT_MSECS));

		if (rc < 0)
			goto exit;

		if (rc == 0) {
			LOGE(&sentral->client->dev, "FIFO flush timeout\n");
			rc = -EBUSY;
			goto exit;
		}

		LOGD(&sentral->client->dev, "FIFO flush waited %d ms\n",
				SENTRAL_WQ_FLUSH_TIMEOUT_MSECS - jiffies_to_msecs(rc));
	}

	rc = sentral_write_byte(sentral, SR_FIFO_FLUSH, sensor_id);
exit:
	mutex_unlock(&sentral->lock_flush);
	return rc;
}

static int sentral_fifo_get_bytes_remaining(struct sentral_device *sentral,
		u16 *bytes)
{
	int rc = sentral_read_block(sentral, SR_FIFO_BYTES, (void *)bytes,
			sizeof(*bytes));

	LOGD(&sentral->client->dev, "FIFO bytes remaining: %u\n", *bytes);

	return rc;
}

static int sentral_fifo_parse(struct sentral_device *sentral, u8 *buffer,
		size_t bytes)
{
	u8 sensor_id;
	size_t data_size;

	while (bytes > 0) {
		// get sensor id
		sensor_id = *buffer++;
		bytes--;
		data_size = 0;

		switch (sensor_id) {

		case SST_TIMESTAMP_MSW:
			data_size = sentral_handle_ts_msw(sentral, buffer);
			buffer += data_size;
			bytes -= data_size;
			continue;

		case SST_TIMESTAMP_LSW:
			data_size = sentral_handle_ts_lsw(sentral, buffer);
			buffer += data_size;
			bytes -= data_size;
			continue;

		case SST_NOP:
			continue;

		case SST_STEP_DETECTOR:
		case SST_TILT_DETECTOR:
		case SST_WAKE_GESTURE:
		case SST_GLANCE_GESTURE:
		case SST_PICK_UP_GESTURE:
		case SST_INACTIVITY_ALARM:
			data_size = 0;
			break;

		// handled by special wake-up register
		case SST_SIGNIFICANT_MOTION:
		case SST_WRIST_TILT_GESTURE:
			data_size = 0;
			continue;

		case SST_HEART_RATE:
		case SST_SLEEP:
			data_size = 1;
			break;

		case SST_LIGHT:
		case SST_PROXIMITY:
		case SST_RELATIVE_HUMIDITY:
		case SST_TEMPERATURE:
		case SST_AMBIENT_TEMPERATURE:
		case SST_ACTIVITY:
			data_size = 2;
			break;

		case SST_STEP_COUNTER:
			data_size = sentral_handle_step_count(sentral, buffer);
			buffer += data_size;
			bytes -= data_size;
			continue;

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
			data_size = sentral_handle_meta_data(sentral, buffer);
			buffer += data_size;
			bytes -= data_size;
			continue;

		case SST_DEBUG:
			data_size = sentral_handle_debug_data(sentral, buffer);
			buffer += data_size;
			bytes -= data_size;
			continue;

		default:
			LOGE(&sentral->client->dev, "invalid sensor type: %u\n", sensor_id);
			// invalid sensor indicates FIFO corruption so we drop parsing the rest; we need to
			// wake up any waiting wq in case there was a flush complete event that was dropped
			(void)sentral_wq_wake_flush_complete(sentral);
			return -EINVAL;
		}

		if ((sensor_id < SST_MAX)
				&& (sentral->ts_msw_offset_reset[sensor_id] == 1)) {
			if (sentral->ts_msw_meta_rate_change[sensor_id] >= sentral->ts_msw_last) {
				sentral->ts_msw_offset[sensor_id]
						= sentral->ts_msw_meta_rate_change[sensor_id]
						- sentral->ts_msw_last;

				LOGD(&sentral->client->dev, "msw offset for sensor id %d %u\n",
						sensor_id, sentral->ts_msw_offset[sensor_id]);

				sentral->ts_msw_offset_reset[sensor_id] = 0;
			}
		}

		sentral_iio_buffer_push_std(sentral, sensor_id, (void *)buffer,
				sentral->ts_sensor_stime, data_size);

		buffer += data_size;
		bytes -= data_size;

	}
	return 0;
}

static int sentral_fifo_read_block(struct sentral_device *sentral, u8 *buffer,
		size_t bytes)
{
	int rc;

	mutex_lock(&sentral->lock_special);
	rc = sentral_read_block(sentral, SR_FIFO_START, buffer, bytes);
	mutex_unlock(&sentral->lock_special);

	return rc;
}

static int sentral_fifo_read(struct sentral_device *sentral, u8 *buffer)
{
	u16 bytes_remaining = 0;
	int rc;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	// get bytes remaining
	rc = sentral_fifo_get_bytes_remaining(sentral, &bytes_remaining);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) reading FIFO bytes remaining\n", rc);
		goto exit;
	}

	LOGD(&sentral->client->dev, "FIFO bytes remaining: %u\n", bytes_remaining);

	// check buffer overflow
	if (bytes_remaining > DATA_BUFFER_SIZE) {
		LOGE(&sentral->client->dev, "FIFO read buffer overflow (%u > %u)\n",
				bytes_remaining, DATA_BUFFER_SIZE);
		rc = -ENOMEM;
		goto exit;
	}

	// read FIFO
	rc = sentral_fifo_read_block(sentral, buffer, bytes_remaining);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading FIFO\n", rc);
		goto exit;
	}

	// parse buffer
	rc = sentral_fifo_parse(sentral, buffer, bytes_remaining);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) parsing FIFO\n", rc);
		goto exit;
	}

	rc = 0;

exit:
	atomic_set(&sentral->fifo_pending, 0);
	wake_up_interruptible(&sentral->wq_fifo);
	return rc;
}

static void sentral_do_work_fifo_read(struct work_struct *work)
{
	struct sentral_device *sentral = container_of(work, struct sentral_device,
			work_fifo_read);

	int rc;

	LOGD(&sentral->client->dev, "%s\n", __func__);

	rc = sentral_fifo_read(sentral, (void *)sentral->data_buffer);

	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) reading FIFO in fifo read workqueue\n", rc);
		sentral_crash_reset(sentral);
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

// timer
static void step_counter_timer_set(unsigned long handler)
{
	struct sentral_device *sentral = (struct sentral_device *)handler;

	// set flag on
	sentral->step_counter_reset_rate_en = true;

	// set another timer
	sentral->sc_timer.expires = jiffies + SENTRAL_STEP_COUNTER_TIMER;
	sentral->sc_timer.data = (unsigned long)sentral;
	sentral->sc_timer.function = step_counter_timer_set;
	add_timer(&(sentral->sc_timer));

	LOGI(&sentral->client->dev, "Timer added for step counter");
}

// firmware load
static int sentral_firmware_load(struct sentral_device *sentral,
		const char *firmware_name)
{
	const struct firmware *fw;
	struct sentral_fw_header *fw_header;
	struct sentral_fw_cds *fw_cds;
	u32 *fw_data;
	size_t fw_data_size;

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

	while (fw_data_size > 0) {
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
	rc = sentral_crc_get(sentral, &sentral->fw_crc);
	if (rc < 0) {
		LOGE(&sentral->client->dev, "error (%d) reading host CRC\n", rc);
		goto exit_release;
	}

	LOGI(&sentral->client->dev, "host CRC: 0x%08X, fw CRC: 0x%08X\n",
			sentral->fw_crc, fw_header->text_crc);

	if (sentral->fw_crc != fw_header->text_crc) {
		LOGE(&sentral->client->dev,
				"invalid firmware CRC, expected 0x%08X got 0x%08X\n",
				sentral->fw_crc, fw_header->text_crc);

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

	rc = sentral_read_byte(sentral, SR_CHIP_CONTROL);
	if (rc < 0)
		return rc;

	chip_control.byte = rc;

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"CPU Run", (chip_control.bits.cpu_run ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"Upload Enable",
			(chip_control.bits.upload_enable ? "true" : "false"));

	return count;
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

	rc = sentral_read_byte(sentral, SR_HOST_STATUS);
	if (rc < 0)
		return rc;

	host_status.byte = rc;

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"CPU Reset", (host_status.bits.cpu_reset ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
			"Algo Standby", (host_status.bits.algo_standby ? "true" : "false"));

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %u\n",
			"Host Iface ID", (host_status.bits.host_iface_id >> 2) & 0x07);

	count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %u\n",
			"Algo ID", (host_status.bits.algo_id >> 5) & 0x07);

	return count;
}

static DEVICE_ATTR(host_status, S_IRUGO, sentral_sysfs_host_status_show, NULL);

// chip status

static ssize_t sentral_sysfs_chip_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	const char bit_strings[][20] = {
		"EEPROM", "EEUploadDone", "EEUploadError", "Idle", "NoEEPROM",
	};
	ssize_t count = 0;
	int rc;
	int i;

	rc = sentral_read_byte(sentral, SR_CHIP_STATUS);
	if (rc < 0)
		return rc;

	for (i = 0; i < sizeof(bit_strings) / 20; i++) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n",
				bit_strings[i], (rc & (1 << i) ? "true" : "false"));
	}

	return count;
}

static DEVICE_ATTR(chip_status, S_IRUGO, sentral_sysfs_chip_status_show, NULL);

// registers

static ssize_t sentral_sysfs_registers_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t len = SR_MAX - SR_FIRST + 1;
	ssize_t count = 0;
	u8 regs[len];
	int rc;
	int i;

	rc = sentral_read_block(sentral, SR_FIRST, (void *)&regs, len);
	if (rc < 0)
		return rc;

	for (i = 0; i < len; i++)
		count += scnprintf(buf + count, PAGE_SIZE - count, "0x%02X: 0x%02X\n",
				SR_FIRST + i, regs[i]);

	return count;
}

static ssize_t sentral_sysfs_registers_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int addr;
	unsigned int value;
	int rc;

	if (2 != sscanf(buf, "%u,%u", &addr, &value))
		return -EINVAL;

	if ((addr > SR_MAX) || (addr < SR_FIRST))
		return -EINVAL;

	I("[SYSFS] registers { addr: 0x%02X, value: 0x%02X }\n", addr, value);

	rc = sentral_write_byte(sentral, (u8)addr, (u8)value);
	if (rc < 0)
		return rc;

	return count;
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

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-33s,%4s,%6s,%6s,%4s,%8s,%6s,%6s\n", "SensorType",
			"Ver", "Power", "Range", "Res", "MaxRate", "FRes", "FMax");

	for (i = SST_FIRST; i < SST_MAX; i++) {
		rc = sentral_sensor_info_read(sentral, i, &sensor_info);
		if (rc)
			return rc;

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
	return count;
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

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-33s,%6s,%11s,%12s,%13s\n", "SensorType", "Rate",
			"MaxLatency", "Sensitivity", "DynamicRange");

	for (i = SST_FIRST; i < SST_MAX; i++) {
		rc = sentral_sensor_config_read(sentral, i, &sensor_config);
		if (rc)
			return rc;

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

	return count;
}

static DEVICE_ATTR(sensor_config, S_IRUGO, sentral_sysfs_sensor_config_show,
		NULL);

static ssize_t sentral_sysfs_phys_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	struct sentral_param_phys_sensor_status_page phys = {{ 0 }};
	ssize_t count = 0;
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_PHYS_SENSOR_STATUS,
			(void *)&phys, sizeof(phys));
	if (rc)
		return rc;

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-15s, %5s, %5s, %5s, %10s\n", "Sensor", "Rate", "Range", "Int",
			"PowerMode");

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-15s, %5u, %5u, %5u, %10u\n", "Accelerometer",
			phys.accel.sample_rate, phys.accel.dynamic_range,
			phys.accel.flags.bits.int_enable, phys.accel.flags.bits.power_mode);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-15s, %5u, %5u, %5u, %10u\n", "Gyroscope",
			phys.gyro.sample_rate, phys.gyro.dynamic_range,
			phys.gyro.flags.bits.int_enable, phys.gyro.flags.bits.power_mode);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%-15s, %5u, %5u, %5u, %10u\n", "Magnetometer",
			phys.mag.sample_rate, phys.mag.dynamic_range,
			phys.mag.flags.bits.int_enable, phys.mag.flags.bits.power_mode);

	return count;
}

static DEVICE_ATTR(phys_status, S_IRUGO, sentral_sysfs_phys_status_show, NULL);

// sensor status

static ssize_t sentral_sysfs_sensor_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	ssize_t count = 0;
	int rc;
	int i, j;
	struct sentral_param_sensor_status sensor_status[16];

	count += scnprintf(buf + count, PAGE_SIZE - count,
			"%5s%10s%10s%10s%10s%10s%10s\n", "SID", "DataAvail", "I2CNACK",
			"DevIDErr", "TransErr", "DataLost", "PowerMode");

	for (i = 0; i < 2; i++) {
		rc = sentral_parameter_read(sentral, SPP_SYS,
				SP_SYS_SENSOR_STATUS_B0 + i, (void *)&sensor_status,
				sizeof(sensor_status));

		if (rc < 0)
			return rc;

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

	return count;
}

static DEVICE_ATTR(sensor_status, S_IRUGO, sentral_sysfs_sensor_status_show,
		NULL);

static ssize_t sentral_sysfs_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);

	// turn on warm restart flag for testing
	sentral->warm_reset = true;

	queue_work(sentral->sentral_wq, &sentral->work_reset);

	return count;
}

static DEVICE_ATTR(reset, S_IWUGO, NULL, sentral_sysfs_reset);

// inactivity timeout

static ssize_t sentral_sysfs_inactivity_timeout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u16 timeout_s = 0;
	int rc;

	rc = sentral_inactivity_timeout_get(sentral, &timeout_s);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", timeout_s);
}

static ssize_t sentral_sysfs_inactivity_timeout_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u16 timeout_s = 0;
	int rc;

	rc = kstrtou16(buf, 10, &timeout_s);
	if (rc)
		return rc;

	I("[SYSFS] inactivity_timeout { timeout_s: %u }\n", timeout_s);

	rc = sentral_inactivity_timeout_set(sentral, timeout_s);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(inactivity_timeout, S_IRUGO | S_IWUGO,
		sentral_sysfs_inactivity_timeout_show,
		sentral_sysfs_inactivity_timeout_store);

// step counter init setting
static ssize_t sentral_sysfs_step_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u16 step = 0;
	int rc;

	rc = sentral_step_count_init_get(sentral, &step);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", step);
}

static ssize_t sentral_sysfs_step_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u64 step = 0;
	int rc;

	rc = kstrtou64(buf, 10, &step);
	if (rc)
		return rc;

	I("[SYSFS] step count { total step count: %llu }\n", step);

	rc = sentral_step_count_set(sentral, step);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(step_count, S_IRUGO | S_IWUGO, sentral_sysfs_step_count_show,
		sentral_sysfs_step_count_store);

// coach fitness id

static ssize_t sentral_sysfs_coach_fitness_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 fitness_id = 0;
	int rc;

	rc = sentral_coach_fitness_id_get(sentral, &fitness_id);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fitness_id);
}

static ssize_t sentral_sysfs_coach_fitness_id_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 fitness_id = 0;
	int rc;

	rc = kstrtou8(buf, 10, &fitness_id);
	if (rc)
		return rc;

	I("[SYSFS] coach_fitness_id { fitness_id: %u }\n", fitness_id);

	rc = sentral_coach_fitness_id_set(sentral, fitness_id);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(coach_fitness_id, S_IRUGO | S_IWUGO,
		sentral_sysfs_coach_fitness_id_show,
		sentral_sysfs_coach_fitness_id_store);

static ssize_t sentral_sysfs_step_report_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u16 report_delay_ms = 0;
	int rc;

	rc = sentral_step_report_delay_get(sentral, &report_delay_ms);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", report_delay_ms);
}

static DEVICE_ATTR(step_report_delay, S_IRUGO,
		sentral_sysfs_step_report_delay_show, NULL);

static ssize_t sentral_sysfs_ts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u32 stime = 0;
	u64 ktime = sentral_get_boottime_ns();
	int rc;

	rc = sentral_stime_current_get(sentral, &stime);
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u,%llu\n", stime, ktime);
}

static DEVICE_ATTR(ts, S_IRUGO, sentral_sysfs_ts_show, NULL);

static ssize_t sentral_sysfs_cal_ts_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u\n", sentral->stime_scale);
}

static ssize_t sentral_sysfs_cal_ts_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int value = 0;
	int rc;

	rc = kstrtouint(buf, 10, &value);
	if (rc)
		return rc;

	I("[SYSFS] cal_ts_data { value: %u }\n", value);

	sentral->stime_scale = value;

	return count;
}

static DEVICE_ATTR(cal_ts_data, S_IRUGO | S_IWUGO, sentral_sysfs_cal_ts_data_show,
		sentral_sysfs_cal_ts_data_store);

static ssize_t sentral_sysfs_vibrator_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	int rc = sentral_read_byte(sentral, SR_VIBRATOR_EN);

	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", !!rc);
}

static ssize_t sentral_sysfs_vibrator_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int value = 0;
	int rc;

	rc = kstrtouint(buf, 10, &value);
	if (rc)
		return rc;

	I("[SYSFS] vibrator_en { value: %u }\n", value);

	rc = sentral_vibrator_enable(sentral, !!value);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(vibrator_en, S_IRUGO | S_IWUGO,
		sentral_sysfs_vibrator_en_show, sentral_sysfs_vibrator_en_store);

// ANDROID sensor_poll_device_t method support

// activate

static ssize_t sentral_sysfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sentral->enabled_mask);
}

static ssize_t sentral_sysfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int id;
	unsigned int enable;
	int rc;

	if (2 != sscanf(buf, "%u %u", &id, &enable))
		return -EINVAL;

	if (!sentral_sensor_id_is_valid(id))
		return -EINVAL;

	I("[SYSFS] enable { id: %u, enable: %u }\n", id, enable);

	rc = sentral_sensor_enable_set(sentral, id, enable);
	if (rc)
		return rc;

	if (!enable) {
		rc = sentral_fifo_flush(sentral, id);
		if (rc)
			return rc;
	}

	return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO, sentral_sysfs_enable_show,
		sentral_sysfs_enable_store);

// set_delay

static ssize_t sentral_sysfs_delay_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int id;
	unsigned int delay_ms;
	unsigned int sample_rate = 0;
	int rc;

	if (2 != sscanf(buf, "%u %u", &id, &delay_ms))
		return -EINVAL;

	I("[SYSFS] delay_ms { id: %u, delay_ms: %u }\n", id, delay_ms);

	if (!sentral_sensor_id_is_valid(id))
		return -EINVAL;

	switch (id) {
	// do not convert delay_ms to Hz for these sensors
	case SST_INACTIVITY_ALARM:
		sample_rate = delay_ms / 1000;
		break;
	case SST_COACH:
		sample_rate = delay_ms / 10000;
		break;
	case SST_STEP_COUNTER:
		sentral->step_counter_rate = (u16)delay_ms;
	case SST_SLEEP:
		sample_rate = (u16)delay_ms;
		break;

	// convert millis to Hz
	default:
		if (delay_ms > 0) {
			delay_ms = MIN(1000, delay_ms);
			sample_rate = 1000 / delay_ms;
		}
	}

	rc = sentral_sensor_batch_set(sentral, id, sample_rate, 0);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(delay_ms, S_IWUGO, NULL, sentral_sysfs_delay_ms_store);

// batch

static ssize_t sentral_sysfs_batch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	unsigned int id;
	unsigned int flags;
	unsigned int delay_ms;
	unsigned int timeout_ms;
	u16 sample_rate = 0;
	int rc;

	if (4 != sscanf(buf, "%u %u %u %u", &id, &flags, &delay_ms, &timeout_ms))
		return -EINVAL;

	I("[SYSFS] batch { id: %u, flags: %u, delay_ms: %u, timeout_ms: %u }\n",
			id, flags, delay_ms, timeout_ms);

	if (!sentral_sensor_id_is_valid(id))
		return -EINVAL;

	if (timeout_ms > USHRT_MAX)
		timeout_ms = USHRT_MAX;

	switch (id) {
	// do not convert delay_ms to Hz for these sensors
	case SST_INACTIVITY_ALARM:
		sample_rate = delay_ms / 1000;
		break;
	case SST_COACH:
		sample_rate = delay_ms / 10000;
		break;
	case SST_STEP_COUNTER:
		sentral->step_counter_rate = (u16)delay_ms;
	case SST_SLEEP:
		sample_rate = (u16)delay_ms;
		break;

	// convert millis to Hz
	default:
		if (delay_ms > 0) {
			delay_ms = MIN(1000, delay_ms);
			sample_rate = 1000 / delay_ms;
		}
	}

	if (sample_rate)
		mdelay(20);

	rc = sentral_sensor_batch_set(sentral, id, sample_rate, timeout_ms);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(batch, S_IWUGO, NULL, sentral_sysfs_batch_store);

// flush

static ssize_t sentral_sysfs_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	u8 id;
	int rc;

	rc = kstrtou8(buf, 10, &id);
	if (rc)
		return rc;

	I("[SYSFS] fifo_flush { id: %u }\n", id);

	if (((id < SST_FIRST) || (id >= SST_MAX)) && (id != SST_ALL))
		return -EINVAL;

	rc = sentral_fifo_flush(sentral, id);
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(flush, S_IWUGO, NULL, sentral_sysfs_flush_store);

// fifo control

static ssize_t sentral_sysfs_fifo_watermark_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	struct sentral_param_fifo_control fifo_ctrl = {{ 0 }};
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)&fifo_ctrl, sizeof(fifo_ctrl));
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fifo_ctrl.fifo.watermark);
}

static ssize_t sentral_sysfs_fifo_watermark_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	struct sentral_param_fifo_control fifo_ctrl = {{ 0 }};
	u16 watermark;
	int rc;

	rc = kstrtou16(buf, 10, &watermark);
	if (rc)
		return rc;

	I("[SYSFS] fifo_watermark { watermark: %u }\n", watermark);

	// read current fifo control param
	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)&fifo_ctrl, sizeof(fifo_ctrl));
	if (rc)
		return rc;

	// update watermark portion
	fifo_ctrl.fifo.watermark = watermark;

	rc = sentral_parameter_write(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)&fifo_ctrl, sizeof(fifo_ctrl));
	if (rc)
		return rc;

	return count;
}

static DEVICE_ATTR(fifo_watermark, S_IRUGO | S_IWUGO,
		sentral_sysfs_fifo_watermark_show, sentral_sysfs_fifo_watermark_store);

static ssize_t sentral_sysfs_fifo_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	struct sentral_param_fifo_control fifo_ctrl = {{ 0 }};
	int rc;

	rc = sentral_parameter_read(sentral, SPP_SYS, SP_SYS_FIFO_CONTROL,
			(void *)&fifo_ctrl, sizeof(fifo_ctrl));
	if (rc)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fifo_ctrl.fifo.size);
}

static DEVICE_ATTR(fifo_size, S_IRUGO, sentral_sysfs_fifo_size_show, NULL);

static ssize_t sentral_sysfs_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sentral_device *sentral = dev_get_drvdata(dev);
	size_t count = 0;

	int locked_special = mutex_is_locked(&sentral->lock_special);
	int locked_flush = mutex_is_locked(&sentral->lock_flush);
	int locked_reset = mutex_is_locked(&sentral->lock_reset);
	int locked_ts = mutex_is_locked(&sentral->lock_ts);

	bool wlocked_irq = sentral->wlock_irq.active;
	int wlocked_reset = wake_lock_active(&sentral->w_lock_reset);

	int fifo_pending = atomic_read(&sentral->fifo_pending);

	LOGE(&sentral->client->dev,
		"locked_special: %d, locked_flush: %d, locked_reset: %d, locked_ts: %d\n",
		locked_special, locked_flush, locked_reset, locked_ts);
	LOGE(&sentral->client->dev,
		"wlocked_irq: %d, wlocked_reset: %d, flush_pending: %d, fifo_pending: %d\n",
		wlocked_irq, wlocked_reset, sentral->flush_pending, fifo_pending);

	count += scnprintf(buf + count, PAGE_SIZE - count,
		"locked_special: %d, locked_flush: %d, locked_reset: %d, locked_ts: %d\n",
		locked_special, locked_flush, locked_reset, locked_ts);

	count += scnprintf(buf + count, PAGE_SIZE - count,
		"wlocked_irq: %d, wlocked_reset: %d, flush_pending: %d, fifo_pending: %d\n",
		wlocked_irq, wlocked_reset, sentral->flush_pending, fifo_pending);

	return count;
}

static DEVICE_ATTR(dbg, S_IRUGO, sentral_sysfs_dbg_show, NULL);

static ssize_t sentral_sysfs_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s.%s.%s.%s (%s)\n", SEN_DRV_PROJECT_ID,
		SEN_DRV_SUBPROJECT_ID, SEN_DRV_VERSION, SEN_DRV_BUILD, SEN_DRV_DATE);
}

static DEVICE_ATTR(version, S_IRUGO, sentral_sysfs_version_show, NULL);

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
	&dev_attr_inactivity_timeout.attr,
	&dev_attr_coach_fitness_id.attr,
	&dev_attr_step_report_delay.attr,
	&dev_attr_ts.attr,
	&dev_attr_sensor_info.attr,
	&dev_attr_sensor_config.attr,
	&dev_attr_sensor_status.attr,
	&dev_attr_phys_status.attr,
	&dev_attr_enable.attr,
	&dev_attr_delay_ms.attr,
	&dev_attr_batch.attr,
	&dev_attr_flush.attr,
	&dev_attr_fifo_watermark.attr,
	&dev_attr_fifo_size.attr,
	&dev_attr_cal_ts_data.attr,
	&dev_attr_dbg.attr,
	&dev_attr_version.attr,
	&dev_attr_vibrator_en.attr,
	&dev_attr_step_count.attr,
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
	struct sentral_wake_src_count wake_src_count;
	int rc;

	LOGD(&sentral->client->dev, "IRQ received\n");

	atomic_set(&sentral->fifo_pending, 1);

	if (sentral->init_complete) {
		// cancel any delayed watchdog work
		if (cancel_delayed_work(&sentral->work_watchdog) == 0)
			flush_workqueue(sentral->sentral_wq);

		// check for special wakeup source
		rc = sentral_read_block(sentral, SR_WAKE_SRC, (void *)&wake_src_count,
				sizeof(wake_src_count));
		if (rc)
			LOGE(&sentral->client->dev, "error (%d) reading wake source\n", rc);

		// handle wakeup source
		if (wake_src_count.byte != sentral->wake_src_prev.byte) {
			__pm_wakeup_event(&sentral->wlock_irq, 250);
			rc = sentral_handle_special_wakeup(sentral, wake_src_count);
			if (rc)
				LOGE(&sentral->client->dev, "error (%d) handling wake source\n", rc);
		}

		queue_work(sentral->sentral_wq, &sentral->work_fifo_read);
	}

	return IRQ_HANDLED;
}

static void sentral_do_work_watchdog(struct work_struct *work)
{
	struct sentral_device *sentral = container_of((struct delayed_work *)work,
			struct sentral_device, work_watchdog);
	u8 err = 0;
	u32 crc = 0;
	int rc;

	LOGD(&sentral->client->dev, "[WD] Watchdog bite\n");

	// check error register
	err = sentral_error_get(sentral);
	if (err > 0)
		LOGE(&sentral->client->dev, "[WD] Error register: 0x%02X\n", err);

	switch (err) {
	case SEN_ERR_MATH:
	case SEN_ERR_MEM:
	case SEN_ERR_STACK_OVERFLOW:
		I("[WD] Fatal error: %u\n", err);
		sentral_crash_reset(sentral);
		return;
	}

	// check CRC for memory corruption
	rc = sentral_crc_get(sentral, &crc);
	if (rc) {
		LOGE(&sentral->client->dev, "[WD] error (%d) reading CRC\n", rc);
		return;
	}

	if (crc && (crc != sentral->fw_crc)) {
		I("[WD] CRC error { fw: %u, read: %u }\n", sentral->fw_crc, crc);
		sentral_crash_reset(sentral);
		return;
	}

	rc = sentral_fifo_read(sentral, (void *)sentral->data_buffer);
	if (rc) {
		LOGE(&sentral->client->dev,
				"error (%d) reading FIFO in watchdog workqueue\n", rc);
		sentral_crash_reset(sentral);
		return;
	}
}

static void sentral_do_work_ts_ref_reset(struct work_struct *work)
{
	struct sentral_device *sentral = container_of((struct delayed_work *)work,
			struct sentral_device, work_ts_ref_reset);

	LOGI(&sentral->client->dev, "[TS] Queuing timestamp ref sync\n");

	sentral->ts_ref_reset_mask = ULONG_MAX;

}

static void sentral_do_work_reset(struct work_struct *work)
{
	struct sentral_device *sentral = container_of(work, struct sentral_device,
			work_reset);

	int rc = 0;

	sentral->init_complete = false;

	mutex_lock(&sentral->lock_reset);
	wake_lock(&sentral->w_lock_reset);

	// load firmware
	rc = sentral_firmware_load(sentral, sentral->platform_data.firmware);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) loading firmware\n", rc);
		goto exit;
	}

	mdelay(100);

	// enable host run
	rc = sentral_set_cpu_run_enable(sentral, true);
	if (rc) {
		LOGE(&sentral->client->dev, "error (%d) enabling cpu run\n", rc);
		goto exit;
	}

	sentral->init_complete = true;

	mdelay(100);

	// queue a FIFO read
	queue_work(sentral->sentral_wq, &sentral->work_fifo_read);
exit:
	wake_unlock(&sentral->w_lock_reset);
	mutex_unlock(&sentral->lock_reset);
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

		cancel_delayed_work_sync(&sentral->work_ts_ref_reset);
		cancel_delayed_work_sync(&sentral->work_watchdog);

		if (atomic_read(&sentral->fifo_pending)) {
			LOGI(&sentral->client->dev, "[PM] waiting for FIFO\n");
			rc = wait_event_interruptible_timeout(sentral->wq_fifo,
					atomic_read(&sentral->fifo_pending) == 0,
					msecs_to_jiffies(SENTRAL_WQ_FIFO_TIMEOUT_MSECS));

			LOGD(&sentral->client->dev, "[PM] FIFO complete: %d\n", rc);

			if (rc == 1)
				return NOTIFY_DONE;

			LOGE(&sentral->client->dev,
					"timeout waiting for pending FIFO read, rc: %d\n", rc);
		}

		break;

	case PM_POST_SUSPEND:
		LOGI(&sentral->client->dev, "post suspend ...\n");

		// notify sentral of wakeup
		rc = sentral_set_host_ap_suspend_enable(sentral, false);
		if (rc) {
			LOGE(&sentral->client->dev,
					"error (%d) setting AP suspend to false\n", rc);
		}

		// check to set step counter rate
		if (sentral->step_counter_reset_rate_en) {
			LOGI(&sentral->client->dev, "Timer set step counter rate : %u\n", sentral->step_counter_rate);
			rc = sentral_sensor_batch_set(sentral, SST_STEP_COUNTER, sentral->step_counter_rate, 0);
			if (rc)
				return rc;
			sentral->step_counter_reset_rate_en = false;
		}

		// reset overflow counter
		sentral->overflow_count = 0;

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

	rc = gpio_to_irq(sentral->platform_data.gpio_irq);
	if (rc < 0) {
		LOGE(&client->dev, "error in GPIO to IRQ\n");
		rc = -ENODEV;
		goto error_free;
	}
	sentral->irq = rc;

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
	INIT_DELAYED_WORK(&sentral->work_ts_ref_reset, &sentral_do_work_ts_ref_reset);

	// init mutex, wakelock
	mutex_init(&sentral->lock);
	mutex_init(&sentral->lock_special);
	mutex_init(&sentral->lock_flush);
	mutex_init(&sentral->lock_reset);
	mutex_init(&sentral->lock_ts);
	init_waitqueue_head(&sentral->wq_flush);

	init_waitqueue_head(&sentral->wq_fifo);
	atomic_set(&sentral->fifo_pending, 0);

	wakeup_source_init(&sentral->wlock_irq, dev_name(dev));
	wake_lock_init(&sentral->w_lock_reset, WAKE_LOCK_SUSPEND, dev_name(dev));

	// zero wake source counters
	sentral->fifo_watermark = 0;
	sentral->wake_src_prev.byte = 0;

	// zero step count values
	memset(&sentral->step_count, 0, sizeof(sentral->step_count));

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

	// set ts defaults
	memset(sentral->ts_sensor_ref, 0, sizeof(sentral->ts_sensor_ref));
	sentral->ts_ref_reset_mask = ULONG_MAX;
	sentral->stime_scale = SENTRAL_SENSOR_TIMESTAMP_SCALE_NS;

	// init pm
	device_init_wakeup(dev, 1);
	if (device_may_wakeup(dev))
		enable_irq_wake(sentral->irq);
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

	// mark as cold start
	sentral->warm_reset = false;

	// init timer for step counter
	sentral->step_counter_rate = 1000;
	sentral->step_counter_reset_rate_en = false;
	init_timer(&(sentral->sc_timer));
	sentral->sc_timer.expires = jiffies + SENTRAL_STEP_COUNTER_TIMER;
	sentral->sc_timer.data = (unsigned long)sentral;
	sentral->sc_timer.function = step_counter_timer_set;
	add_timer(&(sentral->sc_timer));
	LOGI(&sentral->client->dev,"Step Counter Timer Init");

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
	wakeup_source_destroy(&sentral->wlock_irq);

	sentral_sysfs_destroy(sentral);
	sentral_class_destroy(sentral);

	iio_device_unregister(indio_dev);
	sentral_iio_buffer_destroy(indio_dev);

	if (sentral->data_buffer)
		devm_kfree(&client->dev, sentral->data_buffer);

	iio_device_free(indio_dev);
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
	.id_table = sentral_i2c_id_table,
};
module_i2c_driver(sentral_driver);

MODULE_AUTHOR("Jeremiah Mattison <jmattison@pnicorp.com>");
MODULE_DESCRIPTION("SENtral Sensor Hub Driver");
MODULE_LICENSE("GPL");
