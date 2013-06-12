/*
 * Copyright (c) 2008-2010, Motorola, All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __LIS3DH_H__
#define __LIS3DH_H__

#include <linux/ioctl.h>

#define LIS3DH_IOCTL_BASE		71
/* The following define the IOCTL command values via the ioctl macros */
#define LIS3DH_IOCTL_SET_DELAY		_IOW(LIS3DH_IOCTL_BASE, 0, int)
#define LIS3DH_IOCTL_GET_DELAY		_IOR(LIS3DH_IOCTL_BASE, 1, int)
#define LIS3DH_IOCTL_SET_ENABLE		_IOW(LIS3DH_IOCTL_BASE, 2, int)
#define LIS3DH_IOCTL_GET_ENABLE		_IOR(LIS3DH_IOCTL_BASE, 3, int)

#ifdef __KERNEL__
struct lis3dh_platform_data {
	int poll_interval;
	int min_interval;
	int g_range;
	int irq_gpio;
	int threshold;
	int duration;
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

};
#endif /* __KERNEL__ */

#endif  /* __LIS3DH_H__ */

