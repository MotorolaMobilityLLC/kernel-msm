/* Copyright (c) 2016, Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _MOD_DISPLAY_H_
#define _MOD_DISPLAY_H_

#ifdef __KERNEL__

#include <linux/types.h>

#define MOD_DISPLAY_NAME	"mod_display"

enum mod_display_type {
	MOD_DISPLAY_TYPE_INVALID,
	MOD_DISPLAY_TYPE_DSI,
	MOD_DISPLAY_TYPE_DP,
};

enum mod_display_config_type {
	MOD_CONFIG_INVALID,
	MOD_CONFIG_EDID_1_3,
};

struct mod_display_panel_config {
	u8 display_type;
	u8 config_type;
	u32 edid_buf_size;
	char edid_buf[0];
};

enum mod_display_state {
	MOD_DISPLAY_OFF,
	MOD_DISPLAY_ON,
};

#ifdef CONFIG_MOD_DISPLAY

int mod_display_get_display_config(
	struct mod_display_panel_config **display_config);
int mod_display_set_display_config(u8 index);
int mod_display_get_display_state(u8 *state);
int mod_display_set_display_state(u8 state);

#else

static inline int mod_display_get_display_config(
	struct mod_display_panel_config **display_config)
	{ return 0; };
static inline int mod_display_set_display_config(u8 index)
	{ return 0; };
static inline int mod_display_get_display_state(u8 *state)
	{ return 0; };
static inline int mod_display_set_display_state(u8 state)
	{ return 0; };

#endif /* CONFIG_MOD_DISPLAY */

#endif /* __KERNEL__ */

#endif /* _MOD_DISPLAY_H_ */
