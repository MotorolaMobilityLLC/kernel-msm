/* include/linux/platform_data/hds_max1462x.h
 *
 * Copyright (C) 2012 LG Electronics Inc.
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

#ifndef __HDS_MAX1462X_H__
#define __HDS_MAX1462X_H__

struct max1462x_platform_data {
	const char *switch_name;	/* switch device name */
	const char *keypad_name;	/* keypad device name */
	unsigned int key_code;		/* key code for hook, volume up, volume down */
	unsigned int gpio_mic_en;	/* MODE : high, low, high-z */
	unsigned int gpio_key;		/* SWD : to detect 3 or 4 pole | to detect keys */
	unsigned int gpio_detect;	/* DET : to detect jack inserted or not */

	/* callback function which is initialized while probing */
	void (*set_headset_mic_bias)(int enable);

	unsigned int external_ldo_mic_bias;  /* External LDO control */
	/* latency for pole (3 or 4)detection (in ms) */
	unsigned int latency_for_detection;
	unsigned int latency_for_key;
};

#endif /* __HDS_MAX1462X_H__ */
