/*
 * Copyright (C) 2016 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MODBUS_EXT_H__
#define MODBUS_EXT_H__

enum modbus_ext_protocol {
	MODBUS_PROTO_UNKNOWN = 0,
	MODBUS_PROTO_I2S,
	MODBUS_PROTO_MPHY,
};

struct modbus_ext_status {
	bool active;
	enum modbus_ext_protocol proto;
};

typedef void (*modbus_ext_cb)(struct modbus_ext_status *status);

#ifdef CONFIG_MODS_MODBUS_EXT
extern int modbus_ext_register_notifier(struct notifier_block *nb,
		modbus_ext_cb cb);
extern int modbus_ext_unregister_notifier(struct notifier_block *nb);
extern void modbus_ext_get_state(struct modbus_ext_status *status);
extern void modbus_ext_set_state(const struct modbus_ext_status *status);
#else
static inline int modbus_ext_register_notifier(struct notifier_block *nb,
		modbus_ext_cb cb) {
	return 0;
}

static inline int modbus_ext_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline void modbus_ext_get_state(struct modbus_ext_status *status) {}

static inline void modbus_ext_set_state(const struct modbus_ext_status *status)
{}
#endif

#endif
