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

#ifndef USB3813_H__
#define USB3813_H__

#include <linux/mods/usb_ext_bridge.h>

#ifdef CONFIG_USB3813
extern int usb3813_enable_hub(struct i2c_client *client,
			bool enable,
			enum usb_ext_path path);
#else
static inline int usb3813_enable_hub(struct i2c_client *client,
			bool enable,
			enum usb_ext_path path)
{ return 0; }
#endif

#endif
