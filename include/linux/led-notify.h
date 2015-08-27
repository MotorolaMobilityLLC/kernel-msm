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

#ifndef LED_NOTIFY_H
#define LED_NOTIFY_H

#ifdef __KERNEL__

#define BRIGHTNESS_OFF		0
#define BRIGHTNESS_DIM		1
#define BRIGHTNESS_MAX		255

extern void brightness_register_notify(struct notifier_block *nb);
extern void brightness_unregister_notify(struct notifier_block *nb);
extern void brightness_notify_subscriber(unsigned long event);
#endif /* __KERNEL__ */

#endif /* LED_NOTIFY_H */
