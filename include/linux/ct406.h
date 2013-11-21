/*
 * Copyright (C) 2011-2012 Motorola Mobility, Inc.
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

#ifndef _LINUX_CT406_H__
#define _LINUX_CT406_H__

#include <linux/ioctl.h>

#ifdef __KERNEL__

#include <linux/types.h>

#define LD_CT406_NAME "ct406"

#define CT406_REGULATOR_NAME_LENGTH 10

struct ct406_platform_data {
	u16	irq;
	u8	regulator_name[CT406_REGULATOR_NAME_LENGTH];

	u8	prox_samples_for_noise_floor;

	u16	ct406_prox_covered_offset;
	u16	ct406_prox_uncovered_offset;
	u16	ct406_prox_recalibrate_offset;
	u8	ct406_prox_pulse_count;
	u8	ct406_prox_offset;

	u8	ink_type;

	int gpio_irq;
} __packed;

#endif	/* __KERNEL__ */

#endif	/* _LINUX_CT406_H__ */
