/*************************************************************************/ /*!
@File
@Title          Device specific utility routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#include <linux/module.h>
#include <drm/drmP.h>
#include "rgxdf.h"
#include "device.h"
#include "rgxdevice.h"
#include "pvrsrv.h"
#include "rgx_fwif_km.h"
#include "pdump_km.h"
#include "osfunc.h"
#include "allocmem.h"
#include "pvr_debug.h"
#include "power.h"
#include "sync_internal.h"
#include "rgxfwutils.h"
#include "img_types.h"

extern struct drm_device *gpsPVRDRMDev;

static PVRSRV_DEVICE_NODE* pDevNode = IMG_NULL;

static PVRSRV_DEVICE_NODE* RGXGetDeviceNode()
{
	if(pDevNode == IMG_NULL)
	{
		PVRSRV_DEVICE_IDENTIFIER *pDeviceList = IMG_NULL;
		IMG_HANDLE hDevCookie = IMG_NULL;
		IMG_UINT32 numDevices = 0;
		IMG_UINT32 i = 0;
		IMG_UINT32 rgxIndex = IMG_UINT32_MAX;
		IMG_UINT32 error = 0;

		pDeviceList = OSAllocMem(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_IDENTIFIER));
		if (pDeviceList)
		{

			/* Enumerate active devices */
			error = PVRSRVEnumerateDevicesKM(&numDevices, pDeviceList);
			if(!error && pDeviceList){
				for(i =0; i < numDevices; i++){
					if(  pDeviceList[i].eDeviceType == PVRSRV_DEVICE_TYPE_RGX){
					rgxIndex = i;
					}
				}

				if(rgxIndex != IMG_UINT32_MAX){
					/* Now we have to acquire the node to work with, RGX device required*/
					error = PVRSRVAcquireDeviceDataKM (rgxIndex, PVRSRV_DEVICE_TYPE_RGX, &hDevCookie);

					if(!error)
						pDevNode = (PVRSRV_DEVICE_NODE*)hDevCookie;
				}

			}

			OSFreeMem(pDeviceList);
		}
	}

	return pDevNode;
}

unsigned int RGXGetDRMDeviceID(void)
{
	if (gpsPVRDRMDev != NULL)
		return gpsPVRDRMDev->pci_device;

	return 0;
}
EXPORT_SYMBOL(RGXGetDRMDeviceID);

int rgx_is_device_powered(void)
{

	PVRSRV_DEVICE_NODE* psDeviceNode = RGXGetDeviceNode();

	int isPowered = IMG_FALSE;

	if(psDeviceNode)
		isPowered = PVRSRVIsDevicePowered(psDeviceNode->sDevId.ui32DeviceIndex);

	return isPowered;
}
EXPORT_SYMBOL(rgx_is_device_powered);

unsigned int RGXUpdateClockSpeed(unsigned int ui32ClockSpeed)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE* psDeviceNode = RGXGetDeviceNode();
	RGX_DATA	*psRGXData = IMG_NULL;

	if(!psDeviceNode){
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		goto out;
	}

	psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;

	psRGXData->psRGXTimingInfo->ui32CoreClockSpeed = ui32ClockSpeed;
out:
	return eError;
}
EXPORT_SYMBOL(RGXUpdateClockSpeed);

unsigned int RGXPreClockSpeed(void){

	PVRSRV_ERROR	eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE* psDeviceNode = RGXGetDeviceNode();

	if(!psDeviceNode){
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		goto out;
	}

	eError = PVRSRVDevicePreClockSpeedChange(psDeviceNode->sDevId.ui32DeviceIndex, IMG_FALSE, IMG_NULL);
out:
	return eError;
}
EXPORT_SYMBOL(RGXPreClockSpeed);

unsigned int RGXPostClockSpeed(void){

	PVRSRV_ERROR	eError = PVRSRV_OK;
	RGX_DATA	*psRGXData = IMG_NULL;
	PVRSRV_DEVICE_NODE* psDeviceNode = RGXGetDeviceNode();

	if(!psDeviceNode){
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		goto out;
	}


	PVRSRVDevicePostClockSpeedChange(psDeviceNode->sDevId.ui32DeviceIndex, IMG_FALSE, IMG_NULL);

out:
	return eError;
}
EXPORT_SYMBOL(RGXPostClockSpeed);

