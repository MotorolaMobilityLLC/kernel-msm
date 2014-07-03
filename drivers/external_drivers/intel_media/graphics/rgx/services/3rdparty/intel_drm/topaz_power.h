/*
** topaz_power.h
** Login : <binglin.chen@intel.com>
** Started on  Mon Nov 16 13:31:42 2009 brady
**
** Copyright (C) 2009 brady
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef TOPAZ_POWER_H_
#define TOPAZ_POWER_H_

#include "device.h"
#include "power.h"

/* function define */
PVRSRV_ERROR TOPAZRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR TOPAZUnregisterDevice (PVRSRV_DEVICE_NODE *psDeviceNode);

#endif /* !TOPAZ_POWER_H_ */
