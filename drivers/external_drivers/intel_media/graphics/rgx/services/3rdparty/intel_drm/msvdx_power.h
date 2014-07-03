/*
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: binglin.chen@intel.com>
 *
 */

#ifndef MSVDX_POWER_H_
#define MSVDX_POWER_H_

#include "device.h"
#include "power.h"

/* function define */
PVRSRV_ERROR MSVDXRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode);
PVRSRV_ERROR MSVDXUnregisterDevice (PVRSRV_DEVICE_NODE *psDeviceNode);

#endif /* !MSVDX_POWER_H_ */
