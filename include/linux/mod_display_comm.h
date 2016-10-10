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
#ifndef _MOD_DISPLAY_COMM_H_
#define _MOD_DISPLAY_COMM_H_

#ifdef __KERNEL__

#include <linux/mod_display.h>

enum mod_display_notification {
	MOD_NOTIFY_INVALID,
	MOD_NOTIFY_FAILURE,
	MOD_NOTIFY_AVAILABLE,
	MOD_NOTIFY_UNAVAILABLE,
	MOD_NOTIFY_CONNECT,
	MOD_NOTIFY_DISCONNECT,
};

struct mod_display_comm_ops {
	int (*host_ready)(void *data);
	int (*get_display_config)(void *data,
		struct mod_display_panel_config **display_config);
	int (*set_display_config)(void *data, u8 index);
	int (*get_display_state)(void *data, u8 *state);
	int (*set_display_state)(void *data, u8 state);
	void *data; /* arbitrary data passed back to user */
};

struct mod_display_comm_data {
	struct mod_display_comm_ops *ops;
};

#ifdef CONFIG_MOD_DISPLAY

int mod_display_notification(enum mod_display_notification event);
int mod_display_register_comm(struct mod_display_comm_data *comm);
int mod_display_unregister_comm(struct mod_display_comm_data *comm);

#else

static inline int mod_display_notification(enum mod_display_notification event)
	{ return 0; };
static inline int mod_display_register_comm(struct mod_display_comm_data *comm)
	{ return 0; };
static inline int mod_display_unregister_comm(
	struct mod_display_comm_data *comm)
	{ return 0; };

#endif /* CONFIG_MOD_DISPLAY */

#endif /* __KERNEL__ */

#endif /* _MOD_DISPLAY_COMM_H_ */
