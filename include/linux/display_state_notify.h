/*
 * Copyright (C) 2014 Motorola Mobility LLC.
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

#ifndef DISPLAY_STATEE_NOTIFY_H
#define DISPLAY_STATE_NOTIFY_H

#ifdef __KERNEL__

enum display_wakeup_request {
	DISPLAY_STATE_OFF,
	DISPLAY_STATE_LP, /* Low Power mode */
	DISPLAY_STATE_ON
};


extern void display_state_register_notify(struct notifier_block *nb);
extern void display_state_unregister_notify(struct notifier_block *nb);
extern void display_state_notify_subscriber(unsigned long event);
#endif /* __KERNEL__ */

#endif /* DISPLAY_STATE_NOTIFY_H */
