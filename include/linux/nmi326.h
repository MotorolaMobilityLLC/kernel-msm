/*
 * Copyright (C) 2013 Motorola Mobility LLC
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

#ifndef __NMI_H__
#define __NMI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define IOCTL_ISDBT_BASE			't'

#define IOCTL_ISDBT_POWER_ON			_IO(IOCTL_ISDBT_BASE, 0)
#define IOCTL_ISDBT_POWER_OFF			_IO(IOCTL_ISDBT_BASE, 1)
#define IOCTL_ISDBT_RST_DN			_IO(IOCTL_ISDBT_BASE, 2)
#define IOCTL_ISDBT_RST_UP			_IO(IOCTL_ISDBT_BASE, 3)
#define IOCTL_ISDBT_INTERRUPT_REGISTER		_IO(IOCTL_ISDBT_BASE, 4)
#define IOCTL_ISDBT_INTERRUPT_UNREGISTER	_IO(IOCTL_ISDBT_BASE, 5)
#define IOCTL_ISDBT_INTERRUPT_ENABLE		_IO(IOCTL_ISDBT_BASE, 6)
#define IOCTL_ISDBT_INTERRUPT_DISABLE		_IO(IOCTL_ISDBT_BASE, 7)
#define IOCTL_ISDBT_INTERRUPT_DONE		_IO(IOCTL_ISDBT_BASE, 8)

#ifdef __cplusplus
}
#endif

#endif /* __NMI_H__ */
