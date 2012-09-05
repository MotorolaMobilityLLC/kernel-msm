/* Lge_touch_core.c
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/jiffies.h>
#include <linux/sysdev.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/version.h>

#include <asm/atomic.h>
#include <linux/gpio.h>

#include <linux/input/lge_touch_core.h>

struct touch_device_driver*     touch_device_func;
struct workqueue_struct*        touch_wq;

struct lge_touch_attribute {
	struct attribute attr;
	ssize_t (*show)(struct lge_touch_data *ts, char *buf);
	ssize_t (*store)(struct lge_touch_data *ts,
				const char *buf, size_t count);
};

static int is_pressure;
static int is_width_major;
static int is_width_minor;

#define LGE_TOUCH_ATTR(_name, _mode, _show, _store)               \
	struct lge_touch_attribute lge_touch_attr_##_name =       \
	__ATTR(_name, _mode, _show, _store)

/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/debug_mask
 */
u32 touch_debug_mask = DEBUG_BASE_INFO;
module_param_named(debug_mask, touch_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

#ifdef LGE_TOUCH_TIME_DEBUG
/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/lge_touch_core/parameters/time_debug_mask
 */
u32 touch_time_debug_mask = DEBUG_TIME_PROFILE_NONE;
module_param_named(time_debug_mask, touch_time_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

#define get_time_interval(a,b) ((a)>=(b) ? (a)-(b) : 1000000+(a)-(b))
struct timeval t_debug[TIME_PROFILE_MAX];
#endif

#define MAX_RETRY_COUNT         3

#ifdef LGE_TOUCH_POINT_DEBUG
#define MAX_TRACE	500
struct pointer_trace {
	int	x;
	int	y;
	s64	time;
};

static struct pointer_trace tr_data[MAX_TRACE];
static int tr_last_index;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h);
static void touch_late_resume(struct early_suspend *h);
#endif

/* set_touch_handle / get_touch_handle
 *
 * Developer can save their object using 'set_touch_handle'.
 * Also, they can restore that using 'get_touch_handle'.
 */
void set_touch_handle(struct i2c_client *client, void *h_touch)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	ts->h_touch = h_touch;
}

void* get_touch_handle(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);
	return ts->h_touch;
}

/* touch_i2c_read / touch_i2c_write
 *
 * Developer can use these fuctions to communicate with touch_device through I2C.
 */
int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else {
		return 0;
	}
}

int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf)
{
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}

int touch_i2c_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
	unsigned char send_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	send_buf[1] = (unsigned char)data;

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else {
		return 0;
	}
}

#ifdef LGE_TOUCH_TIME_DEBUG
static void time_profile_result(struct lge_touch_data *ts)
{
	if (touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
				&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
			t_debug[TIME_INT_INTERVAL].tv_sec =
						t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec =
						t_debug[TIME_ISR_START].tv_usec;
		} else {
			TOUCH_INFO_MSG("Interval [%6luus], Total [%6luus], "
				"IRQ -> Thread IRQ [%6luus] -> work [%6luus]"
				" -> report [%6luus]\n",
				get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec),
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));

			t_debug[TIME_INT_INTERVAL].tv_sec =
						t_debug[TIME_ISR_START].tv_sec;
			t_debug[TIME_INT_INTERVAL].tv_usec =
						t_debug[TIME_ISR_START].tv_usec;
		}
	} else {
		if (touch_time_debug_mask & DEBUG_TIME_INT_INTERVAL) {
			if (t_debug[TIME_INT_INTERVAL].tv_sec == 0
				&& t_debug[TIME_INT_INTERVAL].tv_usec == 0) {
				t_debug[TIME_INT_INTERVAL].tv_sec =
						t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec =
						t_debug[TIME_ISR_START].tv_usec;
			} else {
				TOUCH_INFO_MSG("Interrupt interval: %6luus\n",
					get_time_interval(t_debug[TIME_ISR_START].tv_usec, t_debug[TIME_INT_INTERVAL].tv_usec));

				t_debug[TIME_INT_INTERVAL].tv_sec =
						t_debug[TIME_ISR_START].tv_sec;
				t_debug[TIME_INT_INTERVAL].tv_usec =
						t_debug[TIME_ISR_START].tv_usec;
			}
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_IRQ_DELAY) {
			TOUCH_INFO_MSG("IRQ -> Thread IRQ : %6luus\n",
				get_time_interval(t_debug[TIME_THREAD_ISR_START].tv_usec, t_debug[TIME_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_INT_THREAD_IRQ_DELAY) {
			TOUCH_INFO_MSG("Thread IRQ -> work: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_START].tv_usec, t_debug[TIME_THREAD_ISR_START].tv_usec));
		}

		if (touch_time_debug_mask & DEBUG_TIME_DATA_HANDLE) {
			TOUCH_INFO_MSG("work -> report: %6luus\n",
				get_time_interval(t_debug[TIME_WORKQUEUE_END].tv_usec, t_debug[TIME_WORKQUEUE_START].tv_usec));
		}
	}

	if (!ts->ts_data.total_num)
		memset(t_debug, 0x0, sizeof(t_debug));
}
#endif

/* release_all_ts_event
 *
 * When system enters suspend-state,
 * if user press touch-panel, release them automatically.
 */
static void release_all_ts_event(struct lge_touch_data *ts)
{
	int id;

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		if (!ts->ts_data.curr_data[id].state)
			continue;

		input_mt_slot(ts->input_dev, id);
		input_mt_report_slot_state(ts->input_dev,
				ts->ts_data.curr_data[id].tool_type, 0);
		ts->ts_data.curr_data[id].state = 0;
	}

	input_sync(ts->input_dev);
}

/* touch_power_cntl
 *
 * 1. POWER_ON
 * 2. POWER_OFF
 * 3. POWER_SLEEP
 * 4. POWER_WAKE
 */
static int touch_power_cntl(struct lge_touch_data *ts, int onoff)
{
	int ret = 0;

	if (touch_device_func->power == NULL) {
		TOUCH_INFO_MSG("There is no specific power control function\n");
		return -1;
	}

	switch (onoff) {
	case POWER_ON:
		ret = touch_device_func->power(ts->client, POWER_ON);
		if (ret < 0)
			TOUCH_ERR_MSG("power on failed\n");
		else
			ts->curr_pwr_state = POWER_ON;
		break;
	case POWER_OFF:
		ret = touch_device_func->power(ts->client, POWER_OFF);
		if (ret < 0)
			TOUCH_ERR_MSG("power off failed\n");
		else
			ts->curr_pwr_state = POWER_OFF;

		msleep(ts->pdata->role->reset_delay);

		atomic_set(&ts->device_init, 0);
		break;
	case POWER_SLEEP:
		ret = touch_device_func->power(ts->client, POWER_SLEEP);
		if (ret < 0)
			TOUCH_ERR_MSG("power sleep failed\n");
		else
			ts->curr_pwr_state = POWER_SLEEP;
		break;
	case POWER_WAKE:
		ret = touch_device_func->power(ts->client, POWER_WAKE);
		if (ret < 0)
			TOUCH_ERR_MSG("power wake failed\n");
		else
			ts->curr_pwr_state = POWER_WAKE;
		break;
	default:
		break;
	}

	if (unlikely(touch_debug_mask & DEBUG_POWER))
		if (ret >= 0)
			TOUCH_INFO_MSG("%s: power_state[%d]",
					__FUNCTION__, ts->curr_pwr_state);

	return ret;
}

/* safety_reset
 *
 * 1. disable irq/timer.
 * 2. turn off the power.
 * 3. turn on the power.
 * 4. sleep (booting_delay)ms, usually 400ms(synaptics).
 * 5. enable irq/timer.
 *
 * After 'safety_reset', we should call 'touch_init'.
 */
static void safety_reset(struct lge_touch_data *ts)
{
	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		disable_irq(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	release_all_ts_event(ts);

	touch_power_cntl(ts, POWER_OFF);
	touch_power_cntl(ts, POWER_ON);
	msleep(ts->pdata->role->booting_delay);

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer,
				ktime_set(0, ts->pdata->role->report_period),
				HRTIMER_MODE_REL);
}

/* touch_ic_init
 *
 * initialize the device_IC and variables.
 */
static int touch_ic_init(struct lge_touch_data *ts)
{
	int int_pin = 0;
	int next_work = 0;

	if (unlikely(ts->ic_init_err_cnt >= MAX_RETRY_COUNT)) {
		TOUCH_ERR_MSG("Init Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

	atomic_set(&ts->next_work, 0);
	atomic_set(&ts->device_init, 1);

	if (touch_device_func->init == NULL) {
		TOUCH_INFO_MSG("There is no specific IC init function\n");
		goto err_out_critical;
	}

	if (touch_device_func->init(ts->client, &ts->fw_info) < 0) {
		TOUCH_ERR_MSG("specific device initialization fail\n");
		goto err_out_retry;
	}

	/* Interrupt pin check after IC init - avoid Touch lockup */
	if (ts->pdata->role->operation_mode == INTERRUPT_MODE) {
		int_pin = gpio_get_value(ts->pdata->int_pin);
		next_work = atomic_read(&ts->next_work);

		if (unlikely(int_pin != 1 && next_work <= 0)) {
			TOUCH_INFO_MSG("WARN: Interrupt pin is low"
					" - next_work: %d, try_count: %d]\n",
					next_work, ts->ic_init_err_cnt);
			goto err_out_retry;
		}
	}

	ts->gf_ctrl.count = 0;
	ts->gf_ctrl.ghost_check_count = 0;
	memset(&ts->ts_data, 0, sizeof(ts->ts_data));
	memset(&ts->fw_upgrade, 0, sizeof(ts->fw_upgrade));
	ts->ic_init_err_cnt = 0;

	ts->jitter_filter.id_mask = 0;
	memset(ts->jitter_filter.his_data,
				0, sizeof(ts->jitter_filter.his_data));
	memset(&ts->accuracy_filter.his_data,
				0, sizeof(ts->accuracy_filter.his_data));

	return 0;

err_out_retry:
	ts->ic_init_err_cnt++;
	safety_reset(ts);
	queue_delayed_work(touch_wq, &ts->work_init, msecs_to_jiffies(10));

	return 0;

err_out_critical:
	ts->ic_init_err_cnt = 0;

	return -1;
}

/* Jitter Filter
 *
 */
#define jitter_abs(x)           ((x) > 0 ? (x) : -(x))
#define jitter_sub(x, y)        ((x) > (y) ? (x) - (y) : (y) - (x))

static u16 check_boundary(int x, int max)
{
	if (x < 0)
		return 0;
	else if (x > max)
		return (u16)max;
	else
		return (u16)x;
}

static int check_direction(int x)
{
	if (x > 0)
		return 1;
	else if (x < 0)
		return -1;
	else
		return 0;
}

static int accuracy_filter_func(struct lge_touch_data *ts)
{
	int delta_x = 0;
	int delta_y = 0;

	/* finish the accuracy_filter */
	if (ts->accuracy_filter.finish_filter == 1 &&
			(ts->accuracy_filter.his_data.count >
				ts->accuracy_filter.touch_max_count ||
			ts->ts_data.total_num != 1)) {
		ts->accuracy_filter.finish_filter = 0;
		ts->accuracy_filter.his_data.count = 0;
	}

	if (ts->accuracy_filter.finish_filter) {
		delta_x = (int)ts->accuracy_filter.his_data.x -
				(int)ts->ts_data.curr_data[0].x_position;
		delta_y = (int)ts->accuracy_filter.his_data.y -
				(int)ts->ts_data.curr_data[0].y_position;
		if (delta_x || delta_y) {
			ts->accuracy_filter.his_data.axis_x +=
						check_direction(delta_x);
			ts->accuracy_filter.his_data.axis_y +=
						check_direction(delta_y);
			ts->accuracy_filter.his_data.count++;
		}

		if (ts->accuracy_filter.his_data.count == 1 ||
			((jitter_sub(ts->ts_data.curr_data[0].pressure,
			ts->accuracy_filter.his_data.pressure) >
				ts->accuracy_filter.ignore_pressure_gap ||
			ts->ts_data.curr_data[0].pressure >
				ts->accuracy_filter.max_pressure) &&
			!((ts->accuracy_filter.his_data.count >
				ts->accuracy_filter.time_to_max_pressure &&
			(jitter_abs(ts->accuracy_filter.his_data.axis_x) ==
					ts->accuracy_filter.his_data.count ||
			jitter_abs(ts->accuracy_filter.his_data.axis_y) ==
					ts->accuracy_filter.his_data.count)) ||
			(jitter_abs(ts->accuracy_filter.his_data.axis_x) >
					ts->accuracy_filter.direction_count ||
			jitter_abs(ts->accuracy_filter.his_data.axis_y) >
				ts->accuracy_filter.direction_count)))) {
			ts->accuracy_filter.his_data.mod_x += delta_x;
			ts->accuracy_filter.his_data.mod_y += delta_y;
		}
	}

	/* if 'delta' > delta_max or id != 0, remove the modify-value. */
	if ((ts->accuracy_filter.his_data.count != 1 &&
		(jitter_abs(delta_x) > ts->accuracy_filter.delta_max ||
		jitter_abs(delta_y) > ts->accuracy_filter.delta_max))) {
		ts->accuracy_filter.his_data.mod_x = 0;
		ts->accuracy_filter.his_data.mod_y = 0;
	}

	/* start the accuracy_filter */
	if (ts->accuracy_filter.finish_filter == 0
			&& ts->accuracy_filter.his_data.count == 0
			&& ts->ts_data.total_num == 1
			&& ts->accuracy_filter.his_data.prev_total_num == 0) {
		ts->accuracy_filter.finish_filter = 1;
		memset(&ts->accuracy_filter.his_data, 0,
					sizeof(ts->accuracy_filter.his_data));
	}

	if (unlikely(touch_debug_mask & DEBUG_ACCURACY)) {
		TOUCH_INFO_MSG("AccuracyFilter: <0> pos[%4d,%4d] "
			"new[%4d,%4d] his[%4d,%4d] delta[%3d,%3d] "
			"mod[%3d,%3d] p[%d,%3d,%3d] axis[%2d,%2d] "
			"count[%2d/%2d] total_num[%d,%d] finish[%d]\n",
			ts->ts_data.curr_data[0].x_position,
			ts->ts_data.curr_data[0].y_position,
			check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max),
			check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max),
			ts->accuracy_filter.his_data.x,
			ts->accuracy_filter.his_data.y,
			delta_x, delta_y,
			ts->accuracy_filter.his_data.mod_x,
			ts->accuracy_filter.his_data.mod_y,
			jitter_sub(ts->ts_data.curr_data[0].pressure,
			ts->accuracy_filter.his_data.pressure) >
					ts->accuracy_filter.ignore_pressure_gap,
			ts->ts_data.curr_data[0].pressure,
			ts->accuracy_filter.his_data.pressure,
			ts->accuracy_filter.his_data.axis_x,
			ts->accuracy_filter.his_data.axis_y,
			ts->accuracy_filter.his_data.count,
			ts->accuracy_filter.touch_max_count,
			ts->accuracy_filter.his_data.prev_total_num,
			ts->ts_data.total_num, ts->accuracy_filter.finish_filter);
	}

	ts->accuracy_filter.his_data.x = ts->ts_data.curr_data[0].x_position;
	ts->accuracy_filter.his_data.y = ts->ts_data.curr_data[0].y_position;
	ts->accuracy_filter.his_data.pressure = ts->ts_data.curr_data[0].pressure;
	ts->accuracy_filter.his_data.prev_total_num = ts->ts_data.total_num;

	if (ts->ts_data.total_num) {
		ts->ts_data.curr_data[0].x_position =
			check_boundary((int)ts->ts_data.curr_data[0].x_position + ts->accuracy_filter.his_data.mod_x, ts->pdata->caps->x_max);
		ts->ts_data.curr_data[0].y_position =
			check_boundary((int)ts->ts_data.curr_data[0].y_position + ts->accuracy_filter.his_data.mod_y, ts->pdata->caps->y_max);
	}

	return 0;
}

static int jitter_filter_func(struct lge_touch_data *ts)
{
	int id;
	int jitter_count = 0;
	u16 new_id_mask = 0;
	u16 bit_mask = 0;
	u16 bit_id = 1;
	int curr_ratio = ts->pdata->role->jitter_curr_ratio;

	if (ts->accuracy_filter.finish_filter)
		return 0;

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		u16 width;

		if (!ts->ts_data.curr_data[id].state)
			continue;

		width = ts->ts_data.curr_data[id].width_major;
		new_id_mask |= (1 << id);

		if (ts->jitter_filter.id_mask & (1 << id)) {
			int delta_x, delta_y;
			int f_jitter = curr_ratio*width;
			int adjust_x, adjust_y;

			if (ts->jitter_filter.adjust_margin > 0) {
				adjust_x = (int)ts->ts_data.curr_data[id].x_position - (int)ts->jitter_filter.his_data[id].x;
				adjust_y = (int)ts->ts_data.curr_data[id].y_position - (int)ts->jitter_filter.his_data[id].y;

				if (jitter_abs(adjust_x) > ts->jitter_filter.adjust_margin) {
					adjust_x = (int)ts->ts_data.curr_data[id].x_position + (adjust_x >> 2);
					ts->ts_data.curr_data[id].x_position =
						check_boundary(adjust_x,
							ts->pdata->caps->x_max);
				}

				if (jitter_abs(adjust_y) > ts->jitter_filter.adjust_margin) {
					adjust_y = (int)ts->ts_data.curr_data[id].y_position + (adjust_y >> 2);
					ts->ts_data.curr_data[id].y_position =
						check_boundary(adjust_y,
							ts->pdata->caps->y_max);
				}
			}

			ts->ts_data.curr_data[id].x_position =
				(ts->ts_data.curr_data[id].x_position +
					ts->jitter_filter.his_data[id].x) >> 1;
			ts->ts_data.curr_data[id].y_position =
				(ts->ts_data.curr_data[id].y_position +
					ts->jitter_filter.his_data[id].y) >> 1;

			delta_x = (int)ts->ts_data.curr_data[id].x_position -
					(int)ts->jitter_filter.his_data[id].x;
			delta_y = (int)ts->ts_data.curr_data[id].y_position -
					(int)ts->jitter_filter.his_data[id].y;

			ts->jitter_filter.his_data[id].delta_x = delta_x * curr_ratio + ((ts->jitter_filter.his_data[id].delta_x * (128 - curr_ratio)) >> 7);
			ts->jitter_filter.his_data[id].delta_y = delta_y * curr_ratio + ((ts->jitter_filter.his_data[id].delta_y * (128 - curr_ratio)) >> 7);

			if (unlikely(touch_debug_mask & DEBUG_JITTER))
				TOUCH_INFO_MSG("JitterFilter: <%d> pos[%d,%d] "
					"h_pos[%d,%d] delta[%d,%d] "
					"h_delta[%d,%d] j_fil[%d]\n",
					id, ts->ts_data.curr_data[id].x_position,
					ts->ts_data.curr_data[id].y_position,
					ts->jitter_filter.his_data[id].x,
					ts->jitter_filter.his_data[id].y,
					delta_x, delta_y,
					ts->jitter_filter.his_data[id].delta_x,
					ts->jitter_filter.his_data[id].delta_y,
					f_jitter);

			if (jitter_abs(ts->jitter_filter.his_data[id].delta_x) <= f_jitter && jitter_abs(ts->jitter_filter.his_data[id].delta_y) <= f_jitter)
				jitter_count++;
		}
	}

	bit_mask = ts->jitter_filter.id_mask ^ new_id_mask;

	for (id = 0, bit_id = 1; id < ts->pdata->caps->max_id; id++) {
		if ((ts->jitter_filter.id_mask & bit_id) &&
						!(new_id_mask & bit_id)) {
			if (unlikely(touch_debug_mask & DEBUG_JITTER))
				TOUCH_INFO_MSG("JitterFilter: released - "
					"id[%d] mask[0x%x]\n",
					bit_id, ts->jitter_filter.id_mask);
			memset(&ts->jitter_filter.his_data[id], 0,
					sizeof(ts->jitter_filter.his_data[id]));
		}
		bit_id = bit_id << 1;
	}

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		if (!ts->ts_data.curr_data[id].state)
			continue;

		ts->jitter_filter.his_data[id].pressure =
					ts->ts_data.curr_data[id].pressure;
	}

	if (!bit_mask && ts->ts_data.total_num &&
				ts->ts_data.total_num == jitter_count) {
		if (unlikely(touch_debug_mask & DEBUG_JITTER))
			TOUCH_INFO_MSG("JitterFilter: ignored - "
					"jitter_count[%d] total_num[%d] "						"bitmask[0x%x]\n",
					jitter_count, ts->ts_data.total_num,
					bit_mask);
		return -1;
	}

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		if (!ts->ts_data.curr_data[id].state)
			continue;

		ts->jitter_filter.his_data[id].x =
					ts->ts_data.curr_data[id].x_position;
		ts->jitter_filter.his_data[id].y =
					ts->ts_data.curr_data[id].y_position;
	}

	ts->jitter_filter.id_mask = new_id_mask;

	return 0;
}

/* touch_init_func
 *
 * In order to reduce the booting-time,
 * we used delayed_work_queue instead of msleep or mdelay.
 */
static void touch_init_func(struct work_struct *work_init)
{
	struct lge_touch_data *ts =
			container_of(to_delayed_work(work_init),
					struct lge_touch_data, work_init);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device initialization */
	touch_ic_init(ts);
}

#ifdef LGE_TOUCH_POINT_DEBUG
static void dump_pointer_trace(void)
{
	int	i;

	printk("Single Touch Trace: Total Points %d in %dms (%dHz)\n",
		tr_last_index,
		tr_last_index > 1
			? (int)(tr_data[tr_last_index-1].time - tr_data[0].time)
			: 0,
		tr_last_index > 1
			? (tr_last_index * 1000) / (int)(tr_data[tr_last_index-1].time - tr_data[0].time)
			: 0);

	for (i = 0; i < tr_last_index; i++) {
		printk("(%x,%x,%d)",
			tr_data[i].x, tr_data[i].y,
			(i == 0) ? 0 : (int)(tr_data[i].time - tr_data[i-1].time));

		if ((i % 4) == 3)
			printk("\n");
	}

	printk("\n");

	tr_last_index = 0;
}
#endif

static void touch_input_report(struct lge_touch_data *ts)
{
	int	id;

	for (id = 0; id < ts->pdata->caps->max_id; id++) {
		if (!ts->ts_data.curr_data[id].state)
			continue;

		input_mt_slot(ts->input_dev, id);
		input_mt_report_slot_state(ts->input_dev,
				ts->ts_data.curr_data[id].tool_type,
				ts->ts_data.curr_data[id].state != ABS_RELEASE);

		if (ts->ts_data.curr_data[id].state != ABS_RELEASE) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					ts->ts_data.curr_data[id].x_position);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					ts->ts_data.curr_data[id].y_position);
			if (is_pressure)
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					ts->ts_data.curr_data[id].pressure);

			/* Only support circle region */
			if (is_width_major)
				input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR,
					ts->ts_data.curr_data[id].width_major);

			if (is_width_minor)
				input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MINOR,
					ts->ts_data.curr_data[id].width_minor);
#ifdef LGE_TOUCH_POINT_DEBUG
			if (id == 0 && tr_last_index < MAX_TRACE) {
				tr_data[tr_last_index].x = ts->ts_data.curr_data[id].x_position;
				tr_data[tr_last_index].y = ts->ts_data.curr_data[id].y_position;
				tr_data[tr_last_index++].time = ktime_to_ms(ktime_get());
			}
#endif
		}
		else {
			ts->ts_data.curr_data[id].state = 0;
#ifdef LGE_TOUCH_POINT_DEBUG
			dump_pointer_trace();
#endif
		}
	}

	input_sync(ts->input_dev);
}

/*
 * Touch work function
 */
static void touch_work_func(struct work_struct *work)
{
	struct lge_touch_data *ts =
			container_of(work, struct lge_touch_data, work);
	int int_pin = 0;
	int next_work = 0;
	int ret;

	atomic_dec(&ts->next_work);
	ts->ts_data.total_num = 0;

	if (unlikely(ts->work_sync_err_cnt >= MAX_RETRY_COUNT)) {
		TOUCH_ERR_MSG("Work Sync Failed: Irq-pin has some unknown problems\n");
		goto err_out_critical;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_START]);
#endif
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ret = touch_device_func->data(ts->client, ts->ts_data.curr_data,
		&ts->ts_data.curr_button, &ts->ts_data.total_num);
	if (ret < 0) {
		if (ret == -EINVAL) /* Ignore the error */
			return;
		goto err_out_critical;
	}

	if (likely(ts->pdata->role->operation_mode == INTERRUPT_MODE))
		int_pin = gpio_get_value(ts->pdata->int_pin);

	/* Accuracy Solution */
	if (likely(ts->pdata->role->accuracy_filter_enable)) {
		if (accuracy_filter_func(ts) < 0)
			goto out;
	}

	/* Jitter Solution */
	if (likely(ts->pdata->role->jitter_filter_enable)) {
		if (jitter_filter_func(ts) < 0)
			goto out;
	}

	touch_input_report(ts);

out:
	if (likely(ts->pdata->role->operation_mode == INTERRUPT_MODE)) {
		next_work = atomic_read(&ts->next_work);

		if (unlikely(int_pin != 1 && next_work <= 0)) {
			TOUCH_INFO_MSG("WARN: Interrupt pin is low - "
					"next_work: %d, try_count: %d]\n",
					next_work, ts->work_sync_err_cnt);
			goto err_out_retry;
		}
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_WORKQUEUE_END]);
	if (next_work)
		memset(t_debug, 0x0, sizeof(t_debug));
	time_profile_result(ts);
#endif
	ts->work_sync_err_cnt = 0;

	return;

err_out_retry:
	ts->work_sync_err_cnt++;
	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);

	return;

err_out_critical:
	ts->work_sync_err_cnt = 0;
	safety_reset(ts);
	touch_ic_init(ts);

	return;
}

/* touch_fw_upgrade_func
 *
 * it used to upgrade the firmware of touch IC.
 */
static void touch_fw_upgrade_func(struct work_struct *work_fw_upgrade)
{
	struct lge_touch_data *ts =
		container_of(work_fw_upgrade,
				struct lge_touch_data, work_fw_upgrade);
	u8 saved_state = ts->curr_pwr_state;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func->fw_upgrade == NULL) {
		TOUCH_INFO_MSG("There is no specific firmware upgrade function\n");
		goto out;
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("fw_rev[%d:%d] product_id[%s:%s] force_upgrade[%d]\n",
			ts->fw_info.fw_rev, ts->fw_info.fw_image_rev,
			ts->fw_info.product_id, ts->fw_info.fw_image_product_id,
			ts->fw_upgrade.fw_force_upgrade);

	ts->fw_upgrade.is_downloading = UNDER_DOWNLOADING;

	if (touch_device_func->fw_upgrade_check(ts) < 0)
		goto out;

	if (ts->curr_pwr_state == POWER_ON ||
				ts->curr_pwr_state == POWER_WAKE) {
		if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
			disable_irq(ts->client->irq);
		else
			hrtimer_cancel(&ts->timer);
	}

	if (ts->curr_pwr_state == POWER_OFF) {
		touch_power_cntl(ts, POWER_ON);
		msleep(ts->pdata->role->booting_delay);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE | DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Start\n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_START]);
#endif

	if (touch_device_func->fw_upgrade(ts->client, ts->fw_upgrade.fw_path) < 0) {
		TOUCH_ERR_MSG("Firmware upgrade was failed\n");

		if (ts->curr_resume_state)
			if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
				enable_irq(ts->client->irq);

		goto err_out;
	}

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);
#endif

	if (!ts->curr_resume_state) {
		touch_power_cntl(ts, POWER_OFF);
	}
	else {
		if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
			enable_irq(ts->client->irq);
		else
			hrtimer_start(&ts->timer,
				ktime_set(0, ts->pdata->role->report_period),
				HRTIMER_MODE_REL);

		touch_ic_init(ts);

		if (saved_state == POWER_WAKE || saved_state == POWER_SLEEP)
			touch_power_cntl(ts, saved_state);
	}

	if (likely(touch_debug_mask & (DEBUG_FW_UPGRADE |DEBUG_BASE_INFO)))
		TOUCH_INFO_MSG("F/W upgrade - Finish\n");

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_FW_UPGRADE_END]);

	if (touch_time_debug_mask & DEBUG_TIME_FW_UPGRADE ||
		touch_time_debug_mask & DEBUG_TIME_PROFILE_ALL) {
		TOUCH_INFO_MSG("FW upgrade time is under %3lu.%06lusec\n",
			get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_sec,
			t_debug[TIME_FW_UPGRADE_START].tv_sec),
			get_time_interval(t_debug[TIME_FW_UPGRADE_END].tv_usec,
			t_debug[TIME_FW_UPGRADE_START].tv_usec));
	}
#endif
	goto out;

err_out:
	safety_reset(ts);
	touch_ic_init(ts);

out:
	memset(&ts->fw_upgrade, 0, sizeof(ts->fw_upgrade));

	return;
}

/* touch_irq_handler
 *
 * When Interrupt occurs, it will be called before touch_thread_irq_handler.
 *
 * return
 * IRQ_HANDLED: touch_thread_irq_handler will not be called.
 * IRQ_WAKE_THREAD: touch_thread_irq_handler will be called.
 */
static irqreturn_t touch_irq_handler(int irq, void *dev_id)
{
	struct lge_touch_data *ts = (struct lge_touch_data *)dev_id;

	if (unlikely(atomic_read(&ts->device_init) != 1))
		return IRQ_HANDLED;

#ifdef LGE_TOUCH_TIME_DEBUG
	do_gettimeofday(&t_debug[TIME_ISR_START]);
#endif
	atomic_inc(&ts->next_work);

	queue_work(touch_wq, &ts->work);

	return IRQ_HANDLED;
}

/* touch_timer_handler
 *
 * it will be called when timer interrupt occurs.
 */
static enum hrtimer_restart touch_timer_handler(struct hrtimer *timer)
{
	struct lge_touch_data *ts =
			container_of(timer, struct lge_touch_data, timer);

	atomic_inc(&ts->next_work);
	queue_work(touch_wq, &ts->work);
	hrtimer_start(&ts->timer,
		ktime_set(0, ts->pdata->role->report_period), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

/* check_platform_data
 *
 * check-list
 * 1. Null Pointer
 * 2. lcd, touch screen size
 * 3. button support
 * 4. operation mode
 * 5. power module
 * 6. report period
 */
static int check_platform_data(struct touch_platform_data *pdata)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!pdata)
		return -1;

	if (!pdata->caps || !pdata->role || !pdata->pwr)
		return -1;

	if (!pdata->caps->lcd_x || !pdata->caps->lcd_y ||
			!pdata->caps->x_max || !pdata->caps->y_max) {
		TOUCH_ERR_MSG("lcd_x, lcd_y, x_max, y_max are should be defined\n");
		return -1;
	}

	if (pdata->caps->button_support) {
		if (!pdata->role->key_type) {
			TOUCH_ERR_MSG("button_support = 1, but key_type is not defined\n");
			return -1;
		}

		if (!pdata->caps->y_button_boundary) {
			if (pdata->role->key_type == TOUCH_HARD_KEY)
				pdata->caps->y_button_boundary = pdata->caps->y_max;
			else
				pdata->caps->y_button_boundary =
					(pdata->caps->lcd_y * pdata->caps->x_max) /
						pdata->caps->lcd_x;
		}

		if (pdata->caps->button_margin < 0 ||
					pdata->caps->button_margin > 49) {
			pdata->caps->button_margin = 10;
			TOUCH_ERR_MSG("0 < button_margin < 49, "
					"button_margin is set 10 by force\n");
		}
	}

	if (pdata->role->operation_mode == POLLING_MODE) {
		if (!pdata->role->report_period) {
			TOUCH_ERR_MSG("polling mode needs report_period\n");
			return -1;
		}
	}

	if (pdata->role->suspend_pwr == POWER_OFF ||
			pdata->role->suspend_pwr == POWER_SLEEP) {
		if (pdata->role->suspend_pwr == POWER_OFF)
			pdata->role->resume_pwr = POWER_ON;
		else
			pdata->role->resume_pwr = POWER_WAKE;
	} else {
		TOUCH_ERR_MSG("suspend_pwr = POWER_OFF or POWER_SLEEP\n");
	}

	if (pdata->pwr->use_regulator) {
		if (!pdata->pwr->vdd[0] || !pdata->pwr->vio[0]) {
			TOUCH_ERR_MSG("you should assign the name of vdd and vio\n");
			return -1;
		}
	} else {
		if (!pdata->pwr->power) {
			TOUCH_ERR_MSG("you should assign the power-control-function\n");
			return -1;
		}
	}

	if (pdata->role->report_period == 0)
		pdata->role->report_period = 12500000;

	return 0;
}

/* get_section
 *
 * it calculates the area of touch-key, automatically.
 */
void get_section(struct section_info* sc, struct touch_platform_data *pdata)
{
	int i;

	sc->panel.left = 0;
	sc->panel.right = pdata->caps->x_max;
	sc->panel.top = 0;
	sc->panel.bottom = pdata->caps->y_button_boundary;

	if (pdata->caps->button_support) {
		sc->b_width  = pdata->caps->x_max /
					pdata->caps->number_of_button;
		sc->b_margin = sc->b_width * pdata->caps->button_margin / 100;
		sc->b_inner_width = sc->b_width - (2 * sc->b_margin);
		sc->b_height = pdata->caps->y_max -
					pdata->caps->y_button_boundary;
		sc->b_num = pdata->caps->number_of_button;

		for (i = 0; i < sc->b_num; i++) {
			sc->button[i].left = i * (pdata->caps->x_max /
				pdata->caps->number_of_button) + sc->b_margin;
			sc->button[i].right = sc->button[i].left +
							sc->b_inner_width;
			sc->button[i].top = pdata->caps->y_button_boundary + 1;
			sc->button[i].bottom = pdata->caps->y_max;

			sc->button_cancel[i].left = sc->button[i].left - (2 * sc->b_margin) >= 0 ?  sc->button[i].left - (2 * sc->b_margin) : 0;
			sc->button_cancel[i].right = sc->button[i].right +
				(2 * sc->b_margin) <= pdata->caps->x_max ?
				sc->button[i].right + (2 * sc->b_margin)
				: pdata->caps->x_max;
			sc->button_cancel[i].top = sc->button[i].top;
			sc->button_cancel[i].bottom = sc->button[i].bottom;

			sc->b_name[i] = pdata->caps->button_name[i];
		}
	}
}

/* Sysfs
 *
 * For debugging easily, we added some sysfs.
 */
static ssize_t show_platform_data(struct lge_touch_data *ts, char *buf)
{
	struct touch_platform_data *pdata = ts->pdata;
	int ret = 0;

	ret = sprintf(buf, "====== Platform data ======\n");
	ret += sprintf(buf+ret, "int_pin[%d] reset_pin[%d]\n",
			pdata->int_pin, pdata->reset_pin);
	ret += sprintf(buf+ret, "caps:\n");
	if (pdata->caps->button_support) {
		ret += sprintf(buf+ret, "\tbutton_support        = %d\n",
			pdata->caps->button_support);
		ret += sprintf(buf+ret, "\ty_button_boundary     = %d\n",
			pdata->caps->y_button_boundary);
		ret += sprintf(buf+ret, "\tbutton_margin         = %d\n",
			pdata->caps->button_margin);
		ret += sprintf(buf+ret, "\tnumber_of_button      = %d\n",
			pdata->caps->number_of_button);
		ret += sprintf(buf+ret, "\tbutton_name           = %d, %d, %d, %d\n",
			pdata->caps->button_name[0],
			pdata->caps->button_name[1],
			pdata->caps->button_name[2],
			pdata->caps->button_name[3]);
	}
	ret += sprintf(buf+ret, "\tis_width_major_supported    = %d\n",
			pdata->caps->is_width_major_supported);
	ret += sprintf(buf+ret, "\tis_width_minor_supported    = %d\n",
			pdata->caps->is_width_minor_supported);
	ret += sprintf(buf+ret, "\tis_pressure_supported = %d\n",
			pdata->caps->is_pressure_supported);
	ret += sprintf(buf+ret, "\tis_id_supported       = %d\n",
			pdata->caps->is_id_supported);
	ret += sprintf(buf+ret, "\tmax_width_major       = %d\n",
			pdata->caps->max_width_major);
	ret += sprintf(buf+ret, "\tmax_width_minor       = %d\n",
			pdata->caps->max_width_minor);
	ret += sprintf(buf+ret, "\tmax_pressure          = %d\n",
			pdata->caps->max_pressure);
	ret += sprintf(buf+ret, "\tmax_id                = %d\n",
			pdata->caps->max_id);
	ret += sprintf(buf+ret, "\tx_max                 = %d\n",
			pdata->caps->x_max);
	ret += sprintf(buf+ret, "\ty_max                 = %d\n",
			pdata->caps->y_max);
	ret += sprintf(buf+ret, "\tlcd_x                 = %d\n",
			pdata->caps->lcd_x);
	ret += sprintf(buf+ret, "\tlcd_y                 = %d\n",
			pdata->caps->lcd_y);
	ret += sprintf(buf+ret, "role:\n");
	ret += sprintf(buf+ret, "\toperation_mode        = %d\n",
			pdata->role->operation_mode);
	ret += sprintf(buf+ret, "\tkey_type              = %d\n",
			pdata->role->key_type);
	ret += sprintf(buf+ret, "\treport_mode           = %d\n",
			pdata->role->report_mode);
	ret += sprintf(buf+ret, "\tdelta_pos_threshold   = %d\n",
			pdata->role->delta_pos_threshold);
	ret += sprintf(buf+ret, "\torientation           = %d\n",
			pdata->role->orientation);
	ret += sprintf(buf+ret, "\treport_period         = %d\n",
			pdata->role->report_period);
	ret += sprintf(buf+ret, "\tbooting_delay         = %d\n",
			pdata->role->booting_delay);
	ret += sprintf(buf+ret, "\treset_delay           = %d\n",
			pdata->role->reset_delay);
	ret += sprintf(buf+ret, "\tsuspend_pwr           = %d\n",
			pdata->role->suspend_pwr);
	ret += sprintf(buf+ret, "\tresume_pwr            = %d\n",
			pdata->role->resume_pwr);
	ret += sprintf(buf+ret, "\tirqflags              = 0x%lx\n",
			pdata->role->irqflags);
	ret += sprintf(buf+ret, "\tshow_touches          = %d\n",
			pdata->role->show_touches);
	ret += sprintf(buf+ret, "\tpointer_location      = %d\n",
			pdata->role->pointer_location);
	ret += sprintf(buf+ret, "pwr:\n");
	ret += sprintf(buf+ret, "\tuse_regulator         = %d\n",
			pdata->pwr->use_regulator);
	ret += sprintf(buf+ret, "\tvdd                   = %s\n",
			pdata->pwr->vdd);
	ret += sprintf(buf+ret, "\tvdd_voltage           = %d\n",
			pdata->pwr->vdd_voltage);
	ret += sprintf(buf+ret, "\tvio                   = %s\n",
			pdata->pwr->vio);
	ret += sprintf(buf+ret, "\tvio_voltage           = %d\n",
			pdata->pwr->vio_voltage);
	ret += sprintf(buf+ret, "\tpower                 = %s\n",
			pdata->pwr->power ? "YES" : "NO");
	return ret;
}

/* show_fw_info
 *
 * show only the firmware information
 */
static ssize_t show_fw_info(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "====== Firmware Info ======\n");
	ret += sprintf(buf+ret, "manufacturer_id  = %d\n",
			ts->fw_info.manufacturer_id);
	ret += sprintf(buf+ret, "product_id       = %s\n",
			ts->fw_info.product_id);
	ret += sprintf(buf+ret, "fw_version       = %s\n",
			ts->fw_info.fw_version);
	ret += sprintf(buf+ret, "fw_image_product_id = %s\n",
			ts->fw_info.fw_image_product_id);
	ret += sprintf(buf+ret, "fw_image_version = %s\n",
			ts->fw_info.fw_image_version);
	return ret;
}

/* store_fw_upgrade
 *
 * User can upgrade firmware, anytime, using this module.
 * Also, user can use both binary-img(SDcard) and header-file(Kernel image).
 */
static ssize_t store_fw_upgrade(struct lge_touch_data *ts,
					const char *buf, size_t count)
{
	int value = 0;
	int repeat = 0;
	char path[256] = {0};

	sscanf(buf, "%d %s", &value, path);

	printk(KERN_INFO "\n");
	TOUCH_INFO_MSG("Firmware image path: %s\n",
				path[0] != 0 ? path : "Internal");

	if (value) {
		for (repeat = 0; repeat < value; repeat++) {
			/* sync for n-th repeat test */
			while (ts->fw_upgrade.is_downloading);

			msleep(ts->pdata->role->booting_delay * 2);
			printk(KERN_INFO "\n");
			TOUCH_INFO_MSG("Firmware image upgrade: No.%d",
								repeat+1);

			/* for n-th repeat test
			 * because ts->fw_upgrade is setted 0 after
			 * FW upgrade */
			memcpy(ts->fw_upgrade.fw_path, path,
					sizeof(ts->fw_upgrade.fw_path)-1);

			/* set downloading flag for sync for n-th test */
			ts->fw_upgrade.is_downloading = UNDER_DOWNLOADING;
			ts->fw_upgrade.fw_force_upgrade = 1;

			queue_work(touch_wq, &ts->work_fw_upgrade);
		}

		/* sync for fw_upgrade test */
		while (ts->fw_upgrade.is_downloading);
	}

	return count;
}

/* show_fw_ver
 *
 * show only firmware version.
 * It will be used for AT-COMMAND
 */
static ssize_t show_fw_ver(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%s\n", ts->fw_info.fw_version);
	return ret;
}

/* store_ts_reset
 *
 * Reset the touch IC.
 */
static ssize_t store_ts_reset(struct lge_touch_data *ts,
					const char *buf, size_t count)
{
	unsigned char string[5];
	u8 saved_state = ts->curr_pwr_state;
	int ret = 0;

	sscanf(buf, "%s", string);

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		disable_irq_nosync(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE) {
		if (!strncmp(string, "soft", 4)) {
			if (touch_device_func->ic_ctrl)
				touch_device_func->ic_ctrl(ts->client,
							IC_CTRL_RESET_CMD, 0);
			else
				TOUCH_INFO_MSG("There is no specific IC control function\n");
		} else if (!strncmp(string, "pin", 3)) {
			if (ts->pdata->reset_pin > 0) {
				gpio_set_value(ts->pdata->reset_pin, 0);
				msleep(ts->pdata->role->reset_delay);
				gpio_set_value(ts->pdata->reset_pin, 1);
			} else {
				TOUCH_INFO_MSG("There is no reset pin\n");
			}
		} else if (!strncmp(string, "vdd", 3)) {
			touch_power_cntl(ts, POWER_OFF);
			touch_power_cntl(ts, POWER_ON);
		} else {
			TOUCH_INFO_MSG("Usage: echo [soft | pin | vdd] > ts_reset\n");
			TOUCH_INFO_MSG(" - soft : reset using IC register setting\n");
			TOUCH_INFO_MSG(" - soft : reset using reset pin\n");
			TOUCH_INFO_MSG(" - hard : reset using VDD\n");
		}

		if (ret < 0)
			TOUCH_ERR_MSG("reset fail\n");
		else
			atomic_set(&ts->device_init, 0);

		msleep(ts->pdata->role->booting_delay);

	} else {
		TOUCH_INFO_MSG("Touch is suspend state. Don't need reset\n");
	}

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer,
			ktime_set(0, ts->pdata->role->report_period),
					HRTIMER_MODE_REL);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE)
		touch_ic_init(ts);

	return count;
}

/* ic_register_ctrl
 *
 * User can see any register of touch_IC
 */
static ssize_t ic_register_ctrl(struct lge_touch_data *ts,
					const char *buf, size_t count)
{
	unsigned char string[6];
	int reg = 0;
	int value = 0;
	int ret = 0;
	u32 write_data;

	sscanf(buf, "%s %d %d", string, &reg, &value);

	if (touch_device_func->ic_ctrl) {
		if (ts->curr_pwr_state == POWER_ON ||
					ts->curr_pwr_state == POWER_WAKE) {
			if (!strncmp(string, "read", 4)) {
				do {
					ret = touch_device_func->ic_ctrl(ts->client, IC_CTRL_READ, reg);
					if (ret >= 0) {
						TOUCH_INFO_MSG("register[0x%x] = 0x%x\n", reg, ret);
					} else {
						TOUCH_INFO_MSG("cannot read register[0x%x]\n", reg);
					}
					reg++;
				} while (--value > 0);
			} else if (!strncmp(string, "write", 4)) {
				write_data = ((0xFF & reg) << 8) | (0xFF & value);
				ret = touch_device_func->ic_ctrl(ts->client,
						IC_CTRL_WRITE, write_data);
				if (ret >= 0) {
					TOUCH_INFO_MSG("register[0x%x] is set to 0x%x\n", reg, value);
				} else {
					TOUCH_INFO_MSG("cannot write register[0x%x]\n", reg);
				}
			} else {
				TOUCH_INFO_MSG("Usage: echo [read | write] "
						"reg_num value > ic_rw\n");
				TOUCH_INFO_MSG(" - reg_num : register address\n");
				TOUCH_INFO_MSG(" - value [read] : number of register "
						"starting form reg_num\n");
				TOUCH_INFO_MSG(" - value [write] : set value into reg_num\n");
			}
		} else {
			TOUCH_INFO_MSG("state=[suspend]. we cannot use I2C, now\n");
		}
	} else {
		TOUCH_INFO_MSG("There is no specific IC control function\n");
	}

	return count;
}

static ssize_t store_jitter_solution(struct lge_touch_data *ts,
						const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->jitter_filter, 0, sizeof(ts->jitter_filter));

	ret = sscanf(buf, "%d %d", &ts->pdata->role->jitter_filter_enable,
				&ts->jitter_filter.adjust_margin);

	return count;
}

static ssize_t store_accuracy_solution(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int ret = 0;

	memset(&ts->accuracy_filter, 0, sizeof(ts->accuracy_filter));

	ret = sscanf(buf, "%d %d %d %d %d %d %d",
				&ts->pdata->role->accuracy_filter_enable,
				&ts->accuracy_filter.ignore_pressure_gap,
				&ts->accuracy_filter.delta_max,
				&ts->accuracy_filter.touch_max_count,
				&ts->accuracy_filter.max_pressure,
				&ts->accuracy_filter.direction_count,
				&ts->accuracy_filter.time_to_max_pressure);

	return count;
}

/* show_show_touches
 *
 * User can check the information of show_touches, using this module.
 */
static ssize_t show_show_touches(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", ts->pdata->role->show_touches);

	return ret;
}

/* store_show_touches
 *
 * This function is related with show_touches in framework.
 */
static ssize_t store_show_touches(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	ts->pdata->role->show_touches = value;

	return count;
}

/* show_pointer_location
 *
 * User can check the information of pointer_location, using this module.
 */
static ssize_t show_pointer_location(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", ts->pdata->role->pointer_location);

	return ret;
}

/* store_pointer_location
 *
 * This function is related with pointer_location in framework.
 */
static ssize_t store_pointer_location(struct lge_touch_data *ts, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d", &value);

	ts->pdata->role->pointer_location = value;

	return count;
}

/* show_charger
 *
 * Show the current charger status
 */
static ssize_t show_charger(struct lge_touch_data *ts, char *buf)
{
	int ret = 0;

	switch (ts->charger_type) {
	case 0:
		ret = sprintf(buf, "%s\n", "NO CHARGER");
		break;
	case 1:
		ret = sprintf(buf, "%s\n", "WIRELESS");
		break;
	case 2:
		ret = sprintf(buf, "%s\n", "USB");
		break;
	case 3:
		ret = sprintf(buf, "%s\n", "AC");
		break;
	}

	return ret;
}

/* store_charger
 *
 * Store the charger status
 * Syntax: <type> <1:0>  type: 0: NO Charger, 1: WIRELESS, 2: USB, 3: AC
 */
static ssize_t store_charger(struct lge_touch_data *ts, const char *buf, size_t count)
{
	long type;

	if (!kstrtol(buf, 10, &type)) {
		if (type >= 0 && type <= 3) {
			/* regardless of charger type */
			ts->charger_type = type;
			touch_device_func->ic_ctrl(ts->client,
				IC_CTRL_CHARGER, ts->charger_type);
		}
	}

	return count;
}

static LGE_TOUCH_ATTR(platform_data, S_IRUGO | S_IWUSR, show_platform_data, NULL);
static LGE_TOUCH_ATTR(firmware, S_IRUGO | S_IWUSR, show_fw_info, store_fw_upgrade);
static LGE_TOUCH_ATTR(fw_ver, S_IRUGO | S_IWUSR, show_fw_ver, NULL);
static LGE_TOUCH_ATTR(reset, S_IRUGO | S_IWUSR, NULL, store_ts_reset);
static LGE_TOUCH_ATTR(ic_rw, S_IRUGO | S_IWUSR, NULL, ic_register_ctrl);
static LGE_TOUCH_ATTR(jitter, S_IRUGO | S_IWUSR, NULL, store_jitter_solution);
static LGE_TOUCH_ATTR(accuracy, S_IRUGO | S_IWUSR, NULL, store_accuracy_solution);
static LGE_TOUCH_ATTR(show_touches, S_IRUGO | S_IWUSR, show_show_touches, store_show_touches);
static LGE_TOUCH_ATTR(pointer_location, S_IRUGO | S_IWUSR, show_pointer_location,
					store_pointer_location);
static LGE_TOUCH_ATTR(charger, S_IRUGO | S_IWUSR, show_charger, store_charger);

static struct attribute *lge_touch_attribute_list[] = {
	&lge_touch_attr_platform_data.attr,
	&lge_touch_attr_firmware.attr,
	&lge_touch_attr_fw_ver.attr,
	&lge_touch_attr_reset.attr,
	&lge_touch_attr_ic_rw.attr,
	&lge_touch_attr_jitter.attr,
	&lge_touch_attr_accuracy.attr,
	&lge_touch_attr_show_touches.attr,
	&lge_touch_attr_pointer_location.attr,
	&lge_touch_attr_charger.attr,
	NULL,
};

/* lge_touch_attr_show / lge_touch_attr_store
 *
 * sysfs bindings for lge_touch
 */
static ssize_t lge_touch_attr_show(struct kobject *lge_touch_kobj, struct attribute *attr,
			     char *buf)
{
	struct lge_touch_data *ts = container_of(lge_touch_kobj,
					struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->show)
		ret = lge_touch_priv->show(ts, buf);

	return ret;
}

static ssize_t lge_touch_attr_store(struct kobject *lge_touch_kobj,
			struct attribute *attr, const char *buf, size_t count)
{
	struct lge_touch_data *ts = container_of(lge_touch_kobj,
				struct lge_touch_data, lge_touch_kobj);
	struct lge_touch_attribute *lge_touch_priv =
		container_of(attr, struct lge_touch_attribute, attr);
	ssize_t ret = 0;

	if (lge_touch_priv->store)
		ret = lge_touch_priv->store(ts, buf, count);

	return ret;
}

static const struct sysfs_ops lge_touch_sysfs_ops = {
	.show	= lge_touch_attr_show,
	.store	= lge_touch_attr_store,
};

static struct kobj_type lge_touch_kobj_type = {
	.sysfs_ops	= &lge_touch_sysfs_ops,
	.default_attrs 	= lge_touch_attribute_list,
};

static struct sysdev_class lge_touch_sys_class = {
	.name = LGE_TOUCH_NAME,
};

static struct sys_device lge_touch_sys_device = {
	.id	= 0,
	.cls	= &lge_touch_sys_class,
};

static int touch_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct lge_touch_data *ts;
	int ret = 0;
	int one_sec = 0;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TOUCH_ERR_MSG("i2c functionality check error\n");
		ret = -EPERM;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct lge_touch_data), GFP_KERNEL);
	if (ts == NULL) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->pdata = client->dev.platform_data;
	ret = check_platform_data(ts->pdata);
	if (ret < 0) {
		TOUCH_ERR_MSG("Can not read platform data\n");
		ret = -EINVAL;
		goto err_assign_platform_data;
	}

	one_sec = 1000000 / (ts->pdata->role->report_period / 1000);
	ts->ic_init_err_cnt = 0;
	ts->work_sync_err_cnt = 0;
	ts->gf_ctrl.min_count = 75000 / (ts->pdata->role->report_period / 1000);
	ts->gf_ctrl.max_count = one_sec;
	get_section(&ts->st_info, ts->pdata);

	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->fw_info.fw_force_rework = false;

	/* Specific device probe */
	if (touch_device_func->probe) {
		ret = touch_device_func->probe(client);
		if (ret < 0) {
			TOUCH_ERR_MSG("specific device probe fail\n");
			goto err_assign_platform_data;
		}
	}

	/* reset pin setting */
	if (ts->pdata->reset_pin > 0) {
		ret = gpio_request(ts->pdata->reset_pin, "touch_reset");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_reset gpio_request\n");
			goto err_assign_platform_data;
		}
		gpio_direction_output(ts->pdata->reset_pin, 0);
	}

	atomic_set(&ts->device_init, 0);
	ts->curr_resume_state = 1;

	/* Power on */
	if (touch_power_cntl(ts, POWER_ON) < 0)
		goto err_power_failed;

	msleep(ts->pdata->role->booting_delay);

	/* init work_queue */
	INIT_WORK(&ts->work, touch_work_func);

	INIT_DELAYED_WORK(&ts->work_init, touch_init_func);
	INIT_WORK(&ts->work_fw_upgrade, touch_fw_upgrade_func);

	/* input dev setting */
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		TOUCH_ERR_MSG("Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "touch_dev";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0))
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	if (ts->pdata->caps->button_support) {
		set_bit(EV_KEY, ts->input_dev->evbit);
		for (ret = 0; ret < ts->pdata->caps->number_of_button; ret++) {
			set_bit(ts->pdata->caps->button_name[ret], ts->input_dev->keybit);
		}
	}

	if (ts->pdata->caps->max_id > MAX_FINGER) {
		ts->pdata->caps->max_id = MAX_FINGER;
	}

	input_mt_init_slots(ts->input_dev, ts->pdata->caps->max_id);
	input_set_abs_params(ts->input_dev,
			ABS_MT_POSITION_X, 0, ts->pdata->caps->x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
			ts->pdata->caps->y_button_boundary
			? ts->pdata->caps->y_button_boundary
			: ts->pdata->caps->y_max,
			0, 0);

	/* Copy for efficient handling */
	is_pressure = ts->pdata->caps->is_pressure_supported;
	is_width_major = ts->pdata->caps->is_width_major_supported;
	is_width_minor = ts->pdata->caps->is_width_minor_supported;

	if (is_pressure)
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
				0, ts->pdata->caps->max_pressure, 0, 0);

	if (is_width_major)
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
				0, ts->pdata->caps->max_width_major, 0, 0);

	if (is_width_minor)
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR,
				0, ts->pdata->caps->max_width_minor, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret < 0) {
		TOUCH_ERR_MSG("Unable to register %s input device\n",
				ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE) {
		ret = gpio_request(ts->pdata->int_pin, "touch_int");
		if (ret < 0) {
			TOUCH_ERR_MSG("FAIL: touch_int gpio_request\n");
			goto err_interrupt_failed;
		}
		gpio_direction_input(ts->pdata->int_pin);

		ret = request_threaded_irq(client->irq, touch_irq_handler,
				NULL,
				ts->pdata->role->irqflags | IRQF_ONESHOT,
				client->name, ts);

		if (ret < 0) {
			TOUCH_ERR_MSG("request_irq failed. use polling mode\n");
			gpio_free(ts->pdata->int_pin);
			goto err_interrupt_failed;
		}
	} else {	/* polling mode */
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = touch_timer_handler;
		hrtimer_start(&ts->timer,
			ktime_set(0, ts->pdata->role->report_period),
					HRTIMER_MODE_REL);
	}

	/* Specific device initialization */
	touch_ic_init(ts);

	/* Firmware Upgrade Check - use thread for booting time reduction */
	if (touch_device_func->fw_upgrade) {
		queue_work(touch_wq, &ts->work_fw_upgrade);
	}

	/* jitter solution */
	if (ts->pdata->role->jitter_filter_enable) {
		ts->jitter_filter.adjust_margin = 100;
	}

	/* accuracy solution */
	if (ts->pdata->role->accuracy_filter_enable) {
		ts->accuracy_filter.ignore_pressure_gap = 5;
		ts->accuracy_filter.delta_max = 100;
		ts->accuracy_filter.max_pressure = 255;
		ts->accuracy_filter.time_to_max_pressure = one_sec / 20;
		ts->accuracy_filter.direction_count = one_sec / 6;
		ts->accuracy_filter.touch_max_count = one_sec / 2;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = touch_early_suspend;
	ts->early_suspend.resume = touch_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	/* Register sysfs for making fixed communication path to framework layer */
	ret = sysdev_class_register(&lge_touch_sys_class);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_class_register is failed\n");
		goto err_lge_touch_sys_class_register;
	}

	ret = sysdev_register(&lge_touch_sys_device);
	if (ret < 0) {
		TOUCH_ERR_MSG("sysdev_register is failed\n");
		goto err_lge_touch_sys_dev_register;
	}

	ret = kobject_init_and_add(&ts->lge_touch_kobj, &lge_touch_kobj_type,
			ts->input_dev->dev.kobj.parent,
			"%s", LGE_TOUCH_NAME);
	if (ret < 0) {
		TOUCH_ERR_MSG("kobject_init_and_add is failed\n");
		goto err_lge_touch_sysfs_init_and_add;
	}

	if (likely(touch_debug_mask & DEBUG_BASE_INFO))
		TOUCH_INFO_MSG("Touch driver is initialized\n");

	return 0;

err_lge_touch_sysfs_init_and_add:
	sysdev_unregister(&lge_touch_sys_device);
err_lge_touch_sys_dev_register:
	sysdev_class_unregister(&lge_touch_sys_class);
err_lge_touch_sys_class_register:
	unregister_early_suspend(&ts->early_suspend);
	if (ts->pdata->role->operation_mode == INTERRUPT_MODE) {
		gpio_free(ts->pdata->int_pin);
		free_irq(ts->client->irq, ts);
	}
err_interrupt_failed:
	input_unregister_device(ts->input_dev);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	touch_power_cntl(ts, POWER_OFF);
err_power_failed:
err_assign_platform_data:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int touch_remove(struct i2c_client *client)
{
	struct lge_touch_data *ts = i2c_get_clientdata(client);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	/* Specific device remove */
	if (touch_device_func->remove)
		touch_device_func->remove(ts->client);

	/* Power off */
	touch_power_cntl(ts, POWER_OFF);

	kobject_del(&ts->lge_touch_kobj);
	sysdev_unregister(&lge_touch_sys_device);
	sysdev_class_unregister(&lge_touch_sys_class);

	unregister_early_suspend(&ts->early_suspend);

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE) {
		gpio_free(ts->pdata->int_pin);
		free_irq(client->irq, ts);
	}
	else {
		hrtimer_cancel(&ts->timer);
	}

	input_unregister_device(ts->input_dev);
	input_free_device(ts->input_dev);
	kfree(ts);

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void touch_early_suspend(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ts->curr_resume_state = 0;

	if (ts->fw_upgrade.is_downloading == UNDER_DOWNLOADING) {
		TOUCH_INFO_MSG("early_suspend is not executed\n");
		return;
	}

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		disable_irq(ts->client->irq);
	else
		hrtimer_cancel(&ts->timer);

	cancel_work_sync(&ts->work);
	cancel_delayed_work_sync(&ts->work_init);
	if (ts->pdata->role->key_type == TOUCH_HARD_KEY)
		cancel_delayed_work_sync(&ts->work_touch_lock);

	release_all_ts_event(ts);

	touch_power_cntl(ts, ts->pdata->role->suspend_pwr);
}

static void touch_late_resume(struct early_suspend *h)
{
	struct lge_touch_data *ts =
			container_of(h, struct lge_touch_data, early_suspend);

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	ts->curr_resume_state = 1;

	if (ts->fw_upgrade.is_downloading == UNDER_DOWNLOADING) {
		TOUCH_INFO_MSG("late_resume is not executed\n");
		return;
	}

	touch_power_cntl(ts, ts->pdata->role->resume_pwr);

	if (ts->pdata->role->operation_mode == INTERRUPT_MODE)
		enable_irq(ts->client->irq);
	else
		hrtimer_start(&ts->timer,
			ktime_set(0, ts->pdata->role->report_period),
					HRTIMER_MODE_REL);

	if (ts->pdata->role->resume_pwr == POWER_ON)
		queue_delayed_work(touch_wq, &ts->work_init,
			msecs_to_jiffies(ts->pdata->role->booting_delay));
	else
		queue_delayed_work(touch_wq, &ts->work_init, 0);
}
#endif

#if defined(CONFIG_PM)
static int touch_suspend(struct device *device)
{
	return 0;
}

static int touch_resume(struct device *device)
{
	return 0;
}
#endif

static struct i2c_device_id lge_ts_id[] = {
	{LGE_TOUCH_NAME, 0 },
};

#if defined(CONFIG_PM)
static struct dev_pm_ops touch_pm_ops = {
	.suspend = touch_suspend,
	.resume = touch_resume,
};
#endif

static struct i2c_driver lge_touch_driver = {
	.probe = touch_probe,
	.remove = touch_remove,
	.id_table = lge_ts_id,
	.driver = {
		.name = LGE_TOUCH_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm = &touch_pm_ops,
#endif
	},
};

int touch_driver_register(struct touch_device_driver* driver)
{
	int ret = 0;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (touch_device_func != NULL) {
		TOUCH_ERR_MSG("CANNOT add new touch-driver\n");
		ret = -EMLINK;
		goto err_touch_driver_register;
	}

	touch_device_func = driver;

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		TOUCH_ERR_MSG("CANNOT create new workqueue\n");
		ret = -ENOMEM;
		goto err_work_queue;
	}

	ret = i2c_add_driver(&lge_touch_driver);
	if (ret < 0) {
		TOUCH_ERR_MSG("FAIL: i2c_add_driver\n");
		goto err_i2c_add_driver;
	}

	return 0;

err_i2c_add_driver:
	destroy_workqueue(touch_wq);
err_work_queue:
err_touch_driver_register:
	return ret;
}

void touch_driver_unregister(void)
{
	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	i2c_del_driver(&lge_touch_driver);
	touch_device_func = NULL;

	if (touch_wq)
		destroy_workqueue(touch_wq);
}

