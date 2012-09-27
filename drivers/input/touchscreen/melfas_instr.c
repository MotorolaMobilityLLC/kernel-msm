/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
//#include <linux/gfp.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/melfas100_ts.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/types.h>

#include <linux/input/touch_platform.h>


#define MELFAS_SLOW_TOUCH 2000
#define MELFAS_LATENCY_SAMPLES 64
#define MELFAS_LATENCY_SHIFT 6
#define MELFAS_ALL_LATENCY_SIZE 1024
#define MELFAS_SLOW_SIZE 256

static u32 melfas_latency_times[MELFAS_LATENCY_SAMPLES];
static struct timespec melfas_int_time;
static u32 melfas_int_count;
static u32 melfas_all_int_count;
static u32 melfas_slow_int_count;
static u32 melfas_high_latency;
static u32 melfas_avg_latency;
static u64 *melfas_slow_timestamp;
static u32 *melfas_all_latency;
static u8 melfas_latency_debug;

static void touch_recalc_avg_latency(void)
{
	u64 total_time = 0;
	u8 i;

	for (i = 0; i < MELFAS_LATENCY_SAMPLES; i++)
		total_time += melfas_latency_times[i];
	melfas_avg_latency = total_time >> MELFAS_LATENCY_SHIFT;

	return;
}

void touch_calculate_latency(void)
{
	struct timespec end_time;
	struct timespec delta_timespec;
	s64 delta_time;

	ktime_get_ts(&end_time);
	if (melfas_int_count == 0)
		melfas_slow_int_count = 0;
	delta_timespec = timespec_sub(end_time, melfas_int_time);
	delta_time = timespec_to_ns(&delta_timespec);
	do_div(delta_time, NSEC_PER_USEC);
	melfas_latency_times[melfas_int_count & \
				(MELFAS_LATENCY_SAMPLES - 1)] = delta_time;
	if ((melfas_latency_debug >= 2) && (melfas_all_latency == NULL)) {
		melfas_all_latency = kmalloc(MELFAS_ALL_LATENCY_SIZE * \
						sizeof(u32), GFP_ATOMIC);
		memset(melfas_all_latency, 0, MELFAS_ALL_LATENCY_SIZE * \
						sizeof(u32));
	}
	if (melfas_all_latency > 0)
		melfas_all_latency[melfas_all_int_count++ & \
			(MELFAS_ALL_LATENCY_SIZE - 1)] = delta_time;
	if (delta_time > MELFAS_SLOW_TOUCH) {
		if ((melfas_latency_debug >= 2) &&
			(melfas_slow_timestamp == NULL)) {
			melfas_slow_timestamp = kmalloc(MELFAS_SLOW_SIZE * \
						sizeof(u64), GFP_ATOMIC);
			memset(melfas_slow_timestamp, 0, MELFAS_SLOW_SIZE * \
						sizeof(u64));
		}
		if (melfas_slow_timestamp > 0)
			melfas_slow_timestamp[melfas_slow_int_count++ & \
				(MELFAS_SLOW_SIZE - 1)] = \
					timespec_to_ns(&end_time);
	}
	if (delta_time > melfas_high_latency)
		melfas_high_latency = delta_time;
	if (!(melfas_int_count & (MELFAS_LATENCY_SAMPLES - 1)))
		touch_recalc_avg_latency();
	melfas_int_count++;

	return;
}
EXPORT_SYMBOL(touch_calculate_latency);

void touch_set_int_time(void)
{
	ktime_get_ts(&melfas_int_time);

	return;
}
EXPORT_SYMBOL(touch_set_int_time);

u32 touch_get_avg_latency(void)
{
	return melfas_avg_latency;
}
EXPORT_SYMBOL(touch_get_avg_latency);

u32 touch_get_high_latency(void)
{
	return melfas_high_latency;
}
EXPORT_SYMBOL(touch_get_high_latency);

u32 touch_get_slow_int_count(void)
{
	return melfas_slow_int_count;
}
EXPORT_SYMBOL(touch_get_slow_int_count);

u32 touch_get_int_count(void)
{
	return melfas_int_count;
}
EXPORT_SYMBOL(touch_get_int_count);

void touch_set_latency_debug_level(u8 debug_level)
{
	melfas_latency_debug = debug_level;

	return;
}
EXPORT_SYMBOL(touch_set_latency_debug_level);

u8 touch_get_latency_debug_level(void)
{
	return melfas_latency_debug;
}
EXPORT_SYMBOL(touch_get_latency_debug_level);

u32 touch_get_timestamp_ptr(u64 **timestamp)
{
	*timestamp = melfas_slow_timestamp;

	return MELFAS_SLOW_SIZE * sizeof(u64);
}
EXPORT_SYMBOL(touch_get_timestamp_ptr);

u32 touch_get_latency_ptr(u32 **latency)
{
	*latency = melfas_all_latency;

	return MELFAS_ALL_LATENCY_SIZE * sizeof(u32);
}
EXPORT_SYMBOL(touch_get_latency_ptr);
