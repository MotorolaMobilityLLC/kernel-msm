/*
 * Copyright (C) 2015 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/notifier.h>
#include <linux/led-notify.h>

static BLOCKING_NOTIFIER_HEAD(brightness_notifier_list);

/**
 * brightness_register_notify - register a notifier callback for triggering backlight
 * @nb: pointer to the notifier block for the callback events.
 *
 */
void brightness_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&brightness_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(brightness_register_notify);

/**
 * brightness_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * brightness_register_notify() must have been previously called
 * for this function to work properly.
 */
void brightness_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&brightness_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(brightness_unregister_notify);

void brightness_notify_subscriber(unsigned long level)
{
	blocking_notifier_call_chain(&brightness_notifier_list, level, NULL);
}
EXPORT_SYMBOL_GPL(brightness_notify_subscriber);

