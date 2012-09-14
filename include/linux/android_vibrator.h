/*
  * Copyright (C) 2011, 2012 LGE, Inc.
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

#ifndef __LINUX_ANDROID_VIBRATOR_H
#define __LINUX_ANDROID_VIBRATOR_H

/* android vibrator platform data */
struct android_vibrator_platform_data {
	int enable_status;
	int amp;
	int vibe_n_value;
	int vibe_warmup_delay; /* in ms */
	int (*power_set)(int enable); /* LDO Power Set Function */
	int (*pwm_set)(int enable, int gain, int n_value); /* PWM Set Function */
	int (*ic_enable_set)(int enable); /* Motor IC Set Function */
	int (*vibrator_init)(void);
};

#endif

