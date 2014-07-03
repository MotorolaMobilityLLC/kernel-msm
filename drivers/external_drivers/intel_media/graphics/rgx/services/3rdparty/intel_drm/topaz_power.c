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
 * Author:binglin.chen@intel.com
 */

#include "topaz_power.h"
#include "pvr_debug.h"

PVRSRV_ERROR TOPAZRegisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	/*********************
	 * Device node setup *
	 *********************/
	/* Setup static data and callbacks on the device agnostic device node */
	psDeviceNode->sDevId.eDeviceType		= PVRSRV_DEVICE_TYPE_TOPAZ;
	psDeviceNode->sDevId.eDeviceClass		= PVRSRV_DEVICE_CLASS_VIDEO;

	/* Configure MMU specific stuff */
	/* ... */

	psDeviceNode->pfnMMUCacheInvalidate = IMG_NULL;

	psDeviceNode->pfnInitDeviceCompatCheck	= IMG_NULL;

	/* Register callbacks for creation of device memory contexts */
	psDeviceNode->pfnRegisterMemoryContext = IMG_NULL;
	psDeviceNode->pfnUnregisterMemoryContext = IMG_NULL;

	/* Register callbacks for Unified Fence Objects */
	psDeviceNode->pfnAllocUFOBlock = IMG_NULL;
	psDeviceNode->pfnFreeUFOBlock = IMG_NULL;

	/*********************
	 * Device info setup *
	 *********************/
	psDeviceNode->pvDevice = IMG_NULL;

	return PVRSRV_OK;
}

PVRSRV_ERROR TOPAZUnregisterDevice(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	return PVRSRV_OK;
}
