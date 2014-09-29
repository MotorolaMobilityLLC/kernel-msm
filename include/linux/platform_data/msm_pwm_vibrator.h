/*
 * Copyright (C) LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_PWM_VIBRATOR_H
#define __MSM_PWM_VIBRATOR_H

struct timed_vibrator_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	spinlock_t spinlock;
	struct mutex lock;
	int max_timeout;
	int min_timeout;
	int ms_time;            /* vibrator duration */
	int status;             /* vibe status */
	int gain;               /* default max gain(amp) */
	int pwm;                /* n-value */
	int braking_gain;
	int braking_ms;
	int clk_flag;
	int haptic_en_gpio;
	int motor_pwm_gpio;
	int motor_pwm_func;
	int warmup_ms;
	int driving_ms;
	ktime_t last_time;     /* time stamp */
	struct delayed_work work_vibrator_off;
	struct delayed_work work_vibrator_on;
	bool use_vdd_supply;
	struct regulator *vdd_reg;
	const char *clk_name;
};

#endif
