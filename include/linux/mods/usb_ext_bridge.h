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

#ifndef EXT_USB_BRIDGE_H__
#define EXT_USB_BRIDGE_H__

enum usb_ext_protocol {
	USB_EXT_PROTO_UNKNOWN = 0,
	USB_EXT_PROTO_2_0,
	USB_EXT_PROTO_3_1,
};

enum usb_ext_path {
	USB_EXT_PATH_UNKNOWN = 0,
	USB_EXT_PATH_ENTERPRISE,
	USB_EXT_PATH_BRIDGE,
};

enum usb_ext_remote_type {
	USB_EXT_REMOTE_UNKNOWN = 0,
	USB_EXT_REMOTE_DEVICE,
	USB_EXT_REMOTE_HOST,
};

struct usb_ext_status {
	bool active;
	enum usb_ext_protocol proto;
	enum usb_ext_path path;
	enum usb_ext_remote_type type;
};

typedef void (*usb_ext_cb)(struct usb_ext_status *status);

extern int usb_ext_register_notifier(struct notifier_block *nb,
		usb_ext_cb cb);
extern int usb_ext_unregister_notifier(struct notifier_block *nb);
extern void usb_ext_get_state(struct usb_ext_status *status);
extern void usb_ext_set_state(const struct usb_ext_status *status);

#endif
