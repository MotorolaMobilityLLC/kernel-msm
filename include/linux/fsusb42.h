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

#ifndef FSUSB42_H__
#define FSUSB42_H__

enum fsusb42_state {
	FSUSB_OFF = 0,
	FSUSB_STATE_USB,
	FSUSB_STATE_EXT,
};

#ifdef CONFIG_FSUSB42_MUX
extern enum fsusb42_state fsusb42_get_state(void);
extern void fsusb42_set_state(enum fsusb42_state state);
#else
static inline enum fsusb42_state fsusb42_get_state(void)
{
	return FSUSB_OFF;
}

static inline void fsusb42_set_state(enum fsusb42_state state) {}
#endif

#endif
