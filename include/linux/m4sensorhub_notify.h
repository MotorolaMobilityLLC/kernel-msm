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

#ifndef M4SENSORHUB_NOTIFY_H
#define M4SENSORHUB_NOTIFY_H

#ifdef __KERNEL__

#include <linux/notifier.h>

/* some peripherals are hooked up on lines that M4 bootloader
is polling to see any activity. If any of these lines show
any kind of activity, M4 bootloader gets confused and can't be
recovered. So.. when we are trying to bring up M4, disable
these peripherals, and reenable them once M4 is up and running*/

enum m4sensor_request {
	DISABLE_PERIPHERAL,
	ENABLE_PERIPHERAL
};


extern void m4sensorhub_register_notify(struct notifier_block *nb);
extern void m4sensorhub_unregister_notify(struct notifier_block *nb);
extern void m4sensorhub_notify_subscriber(unsigned long event);
#endif /* __KERNEL__ */

#endif /* M4SENSORHUB_NOTIFY_H */
