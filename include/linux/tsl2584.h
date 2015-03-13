/*
 * Copyright (C) 2015 Motorola Mobility
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef _LINUX_TSL2584_H__
#define _LINUX_TSL2584_H__

#include <linux/ioctl.h>

#ifdef __KERNEL__

#include <linux/types.h>

#define LD_TSL2584_NAME "tsl2584"

#define TSL2584_REGULATOR_NAME_LENGTH 10

struct tsl2584_platform_data {
	u16	irq;
	u8	regulator_name[TSL2584_REGULATOR_NAME_LENGTH];
	u8	ink_type;
	int gpio_irq;
} __packed;

#endif	/* __KERNEL__ */

#endif	/* _LINUX_TSL2584_H__ */
