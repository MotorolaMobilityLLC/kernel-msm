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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>

#include <linux/mods/usb_ext_bridge.h>

static DEFINE_MUTEX(usb_ext_mutex);
static BLOCKING_NOTIFIER_HEAD(usb_ext_notifier_list);
static struct usb_ext_status usb_ext_current_status;

/*
 * Add caller to receive notifications of changes in attach state.
 */
int usb_ext_register_notifier(struct notifier_block *nb,
		usb_ext_cb cb)
{
	int ret =  blocking_notifier_chain_register(&usb_ext_notifier_list, nb);
	if (cb && !ret) {
		struct usb_ext_status st;

		mutex_lock(&usb_ext_mutex);
		st = usb_ext_current_status;
		mutex_unlock(&usb_ext_mutex);

		cb(&st);
	}
	return ret;
}
EXPORT_SYMBOL(usb_ext_register_notifier);

/*
 * Remove caller from receiving notifications of changes in attach state
 */
int usb_ext_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&usb_ext_notifier_list, nb);
}
EXPORT_SYMBOL(usb_ext_unregister_notifier);

/*
 * Get the current attach state for the USB-EXT interface
 */
void usb_ext_get_state(struct usb_ext_status *status)
{
	mutex_lock(&usb_ext_mutex);
	*status = usb_ext_current_status;
	mutex_unlock(&usb_ext_mutex);
}
EXPORT_SYMBOL(usb_ext_get_state);

/*
 * Set the current attach state for the USB-EXT interface
 */
void usb_ext_set_state(const struct usb_ext_status *status)
{
	struct usb_ext_status st;

	mutex_lock(&usb_ext_mutex);
	usb_ext_current_status = *status;
	st = *status;
	mutex_unlock(&usb_ext_mutex);

	blocking_notifier_call_chain(&usb_ext_notifier_list, st.active, &st);
}
EXPORT_SYMBOL(usb_ext_set_state);
