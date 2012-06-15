/*
  * Copyright (C) 2011 LGE, Inc.
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
	int (*power_set)(int enable); /* LDO Power Set Function */
	int (*pwm_set)(int enable, int gain, int n_value); /* PWM Set Function */
	int (*ic_enable_set)(int enable); /* Motor IC Set Function */
	int (*vibrator_init)(void);
};


/* Debug Mask setting */
#define VIBRATOR_DEBUG_PRINT (1)
#define VIBRATOR_ERROR_PRINT (1)
#define VIBRATOR_INFO_PRINT  (0)

#if (VIBRATOR_INFO_PRINT)
#define INFO_MSG(fmt, args...) \
			printk(KERN_INFO "[%s] " \
				fmt, __FUNCTION__, ##args);
#else
#define INFO_MSG(fmt, args...)
#endif

#if (VIBRATOR_DEBUG_PRINT)
#define DEBUG_MSG(fmt, args...) \
			printk(KERN_INFO "[%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define DEBUG_MSG(fmt, args...)
#endif

#if (VIBRATOR_ERROR_PRINT)
#define ERR_MSG(fmt, args...) \
			printk(KERN_ERR "[%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args);
#else
#define ERR_MSG(fmt, args...)
#endif


#endif

