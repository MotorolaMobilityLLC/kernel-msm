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
#ifndef _MOD_DISPLAY_OPS_H_
#define _MOD_DISPLAY_OPS_H_

#ifdef __KERNEL__

struct mod_display_ops {
	int (*handle_available)(void *data);
	int (*handle_unavailable)(void *data);
	int (*handle_connect)(void *data);
	int (*handle_disconnect)(void *data);
	void *data; /* arbitrary data passed back to user */
};

struct mod_display_impl_data {
	int mod_display_type;
	struct mod_display_ops *ops;
	struct list_head list;
};

#ifdef CONFIG_MOD_DISPLAY

int mod_display_register_impl(struct mod_display_impl_data *impl);

#else

static inline int mod_display_register_impl(struct mod_display_impl_data *impl)
	{ return 0; };

#endif /* CONFIG_MOD_DISPLAY */

#endif /* __KERNEL__ */

#endif /* _MOD_DISPLAY_OPS_H_ */
