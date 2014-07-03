/*************************************************************************/ /*!
@File
@Title          Server bridge for dc
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for dc
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

#include <stddef.h>
#include <asm/uaccess.h>

#include "img_defs.h"

#include "dc_server.h"


#include "common_dc_bridge.h"

#include "allocmem.h"
#include "pvr_debug.h"
#include "connection_server.h"
#include "pvr_bridge.h"
#include "rgx_bridge.h"
#include "srvcore.h"
#include "handle.h"

#if defined (SUPPORT_AUTH)
#include "osauth.h"
#endif

#include <linux/slab.h>

/* ***************************************************************************
 * Bridge proxy functions
 */

static PVRSRV_ERROR
DCDeviceReleaseResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCSystemBufferReleaseResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCDisplayContextDestroyResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCBufferFreeResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCBufferUnimportResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCBufferUnpinResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
DCBufferReleaseResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}



/* ***************************************************************************
 * Server-side bridge entry points
 */

static IMG_INT
PVRSRVBridgeDCDevicesQueryCount(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDEVICESQUERYCOUNT *psDCDevicesQueryCountIN,
					 PVRSRV_BRIDGE_OUT_DCDEVICESQUERYCOUNT *psDCDevicesQueryCountOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDEVICESQUERYCOUNT);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psDCDevicesQueryCountIN);




	psDCDevicesQueryCountOUT->eError =
		DCDevicesQueryCount(
					&psDCDevicesQueryCountOUT->ui32DeviceCount);




	return 0;
}

static IMG_INT
PVRSRVBridgeDCDevicesEnumerate(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE *psDCDevicesEnumerateIN,
					 PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE *psDCDevicesEnumerateOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_UINT32 *pui32DeviceIndexInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDEVICESENUMERATE);

	PVR_UNREFERENCED_PARAMETER(psConnection);

	psDCDevicesEnumerateOUT->pui32DeviceIndex = psDCDevicesEnumerateIN->pui32DeviceIndex;


	if (psDCDevicesEnumerateIN->ui32DeviceArraySize != 0)
	{
		pui32DeviceIndexInt = OSAllocMem(psDCDevicesEnumerateIN->ui32DeviceArraySize * sizeof(IMG_UINT32));
		if (!pui32DeviceIndexInt)
		{
			psDCDevicesEnumerateOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDevicesEnumerate_exit;
		}
	}


	psDCDevicesEnumerateOUT->eError =
		DCDevicesEnumerate(
					psDCDevicesEnumerateIN->ui32DeviceArraySize,
					&psDCDevicesEnumerateOUT->ui32DeviceCount,
					pui32DeviceIndexInt);


	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*) psDCDevicesEnumerateOUT->pui32DeviceIndex, (psDCDevicesEnumerateOUT->ui32DeviceCount * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psDCDevicesEnumerateOUT->pui32DeviceIndex, pui32DeviceIndexInt,
		(psDCDevicesEnumerateOUT->ui32DeviceCount * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psDCDevicesEnumerateOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCDevicesEnumerate_exit;
	}


DCDevicesEnumerate_exit:
	if (pui32DeviceIndexInt)
		OSFreeMem(pui32DeviceIndexInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDeviceAcquire(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDEVICEACQUIRE *psDCDeviceAcquireIN,
					 PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE *psDCDeviceAcquireOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDEVICEACQUIRE);





	psDCDeviceAcquireOUT->eError =
		DCDeviceAcquire(
					psDCDeviceAcquireIN->ui32DeviceIndex,
					&psDeviceInt);
	/* Exit early if bridged call fails */
	if(psDCDeviceAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCDeviceAcquire_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hDeviceInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_DEVICE,
												psDeviceInt,
												(RESMAN_FREE_FN)&DCDeviceRelease);
	if (hDeviceInt2 == IMG_NULL)
	{
		psDCDeviceAcquireOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCDeviceAcquire_exit;
	}
	psDCDeviceAcquireOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCDeviceAcquireOUT->hDevice,
							(IMG_HANDLE) hDeviceInt2,
							PVRSRV_HANDLE_TYPE_DC_DEVICE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCDeviceAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCDeviceAcquire_exit;
	}


DCDeviceAcquire_exit:
	if (psDCDeviceAcquireOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hDeviceInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hDeviceInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psDeviceInt)
		{
			DCDeviceRelease(psDeviceInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCDeviceRelease(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDEVICERELEASE *psDCDeviceReleaseIN,
					 PVRSRV_BRIDGE_OUT_DCDEVICERELEASE *psDCDeviceReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDEVICERELEASE);





				{
					/* Look up the address from the handle */
					psDCDeviceReleaseOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCDeviceReleaseIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCDeviceReleaseOUT->eError != PVRSRV_OK)
					{
						goto DCDeviceRelease_exit;
					}

				}

	psDCDeviceReleaseOUT->eError = DCDeviceReleaseResManProxy(hDeviceInt2);
	/* Exit early if bridged call fails */
	if(psDCDeviceReleaseOUT->eError != PVRSRV_OK)
	{
		goto DCDeviceRelease_exit;
	}

	psDCDeviceReleaseOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCDeviceReleaseIN->hDevice,
					PVRSRV_HANDLE_TYPE_DC_DEVICE);


DCDeviceRelease_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCGetInfo(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCGETINFO *psDCGetInfoIN,
					 PVRSRV_BRIDGE_OUT_DCGETINFO *psDCGetInfoOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCGETINFO);





				{
					/* Look up the address from the handle */
					psDCGetInfoOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCGetInfoIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCGetInfoOUT->eError != PVRSRV_OK)
					{
						goto DCGetInfo_exit;
					}

					/* Look up the data from the resman address */
					psDCGetInfoOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCGetInfoOUT->eError != PVRSRV_OK)
					{
						goto DCGetInfo_exit;
					}
				}

	psDCGetInfoOUT->eError =
		DCGetInfo(
					psDeviceInt,
					&psDCGetInfoOUT->sDisplayInfo);



DCGetInfo_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCPanelQueryCount(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT *psDCPanelQueryCountIN,
					 PVRSRV_BRIDGE_OUT_DCPANELQUERYCOUNT *psDCPanelQueryCountOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCPANELQUERYCOUNT);





				{
					/* Look up the address from the handle */
					psDCPanelQueryCountOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCPanelQueryCountIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCPanelQueryCountOUT->eError != PVRSRV_OK)
					{
						goto DCPanelQueryCount_exit;
					}

					/* Look up the data from the resman address */
					psDCPanelQueryCountOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCPanelQueryCountOUT->eError != PVRSRV_OK)
					{
						goto DCPanelQueryCount_exit;
					}
				}

	psDCPanelQueryCountOUT->eError =
		DCPanelQueryCount(
					psDeviceInt,
					&psDCPanelQueryCountOUT->ui32NumPanels);



DCPanelQueryCount_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCPanelQuery(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCPANELQUERY *psDCPanelQueryIN,
					 PVRSRV_BRIDGE_OUT_DCPANELQUERY *psDCPanelQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;
	PVRSRV_PANEL_INFO *psPanelInfoInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCPANELQUERY);


	psDCPanelQueryOUT->psPanelInfo = psDCPanelQueryIN->psPanelInfo;


	if (psDCPanelQueryIN->ui32PanelsArraySize != 0)
	{
		psPanelInfoInt = OSAllocMem(psDCPanelQueryIN->ui32PanelsArraySize * sizeof(PVRSRV_PANEL_INFO));
		if (!psPanelInfoInt)
		{
			psDCPanelQueryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCPanelQuery_exit;
		}
	}


				{
					/* Look up the address from the handle */
					psDCPanelQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCPanelQueryIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCPanelQueryOUT->eError != PVRSRV_OK)
					{
						goto DCPanelQuery_exit;
					}

					/* Look up the data from the resman address */
					psDCPanelQueryOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCPanelQueryOUT->eError != PVRSRV_OK)
					{
						goto DCPanelQuery_exit;
					}
				}

	psDCPanelQueryOUT->eError =
		DCPanelQuery(
					psDeviceInt,
					psDCPanelQueryIN->ui32PanelsArraySize,
					&psDCPanelQueryOUT->ui32NumPanels,
					psPanelInfoInt);


	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*) psDCPanelQueryOUT->psPanelInfo, (psDCPanelQueryOUT->ui32NumPanels * sizeof(PVRSRV_PANEL_INFO)))
		|| (OSCopyToUser(NULL, psDCPanelQueryOUT->psPanelInfo, psPanelInfoInt,
		(psDCPanelQueryOUT->ui32NumPanels * sizeof(PVRSRV_PANEL_INFO))) != PVRSRV_OK) )
	{
		psDCPanelQueryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCPanelQuery_exit;
	}


DCPanelQuery_exit:
	if (psPanelInfoInt)
		OSFreeMem(psPanelInfoInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCFormatQuery(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCFORMATQUERY *psDCFormatQueryIN,
					 PVRSRV_BRIDGE_OUT_DCFORMATQUERY *psDCFormatQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;
	PVRSRV_SURFACE_FORMAT *psFormatInt = IMG_NULL;
	IMG_UINT32 *pui32SupportedInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCFORMATQUERY);


	psDCFormatQueryOUT->pui32Supported = psDCFormatQueryIN->pui32Supported;


	if (psDCFormatQueryIN->ui32NumFormats != 0)
	{
		psFormatInt = OSAllocMem(psDCFormatQueryIN->ui32NumFormats * sizeof(PVRSRV_SURFACE_FORMAT));
		if (!psFormatInt)
		{
			psDCFormatQueryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCFormatQuery_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCFormatQueryIN->psFormat, psDCFormatQueryIN->ui32NumFormats * sizeof(PVRSRV_SURFACE_FORMAT))
				|| (OSCopyFromUser(NULL, psFormatInt, psDCFormatQueryIN->psFormat,
				psDCFormatQueryIN->ui32NumFormats * sizeof(PVRSRV_SURFACE_FORMAT)) != PVRSRV_OK) )
			{
				psDCFormatQueryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCFormatQuery_exit;
			}
	if (psDCFormatQueryIN->ui32NumFormats != 0)
	{
		pui32SupportedInt = OSAllocMem(psDCFormatQueryIN->ui32NumFormats * sizeof(IMG_UINT32));
		if (!pui32SupportedInt)
		{
			psDCFormatQueryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCFormatQuery_exit;
		}
	}


				{
					/* Look up the address from the handle */
					psDCFormatQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCFormatQueryIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCFormatQueryOUT->eError != PVRSRV_OK)
					{
						goto DCFormatQuery_exit;
					}

					/* Look up the data from the resman address */
					psDCFormatQueryOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCFormatQueryOUT->eError != PVRSRV_OK)
					{
						goto DCFormatQuery_exit;
					}
				}

	psDCFormatQueryOUT->eError =
		DCFormatQuery(
					psDeviceInt,
					psDCFormatQueryIN->ui32NumFormats,
					psFormatInt,
					pui32SupportedInt);


	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*) psDCFormatQueryOUT->pui32Supported, (psDCFormatQueryIN->ui32NumFormats * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psDCFormatQueryOUT->pui32Supported, pui32SupportedInt,
		(psDCFormatQueryIN->ui32NumFormats * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psDCFormatQueryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCFormatQuery_exit;
	}


DCFormatQuery_exit:
	if (psFormatInt)
		OSFreeMem(psFormatInt);
	if (pui32SupportedInt)
		OSFreeMem(pui32SupportedInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDimQuery(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDIMQUERY *psDCDimQueryIN,
					 PVRSRV_BRIDGE_OUT_DCDIMQUERY *psDCDimQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;
	PVRSRV_SURFACE_DIMS *psDimInt = IMG_NULL;
	IMG_UINT32 *pui32SupportedInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDIMQUERY);


	psDCDimQueryOUT->pui32Supported = psDCDimQueryIN->pui32Supported;


	if (psDCDimQueryIN->ui32NumDims != 0)
	{
		psDimInt = OSAllocMem(psDCDimQueryIN->ui32NumDims * sizeof(PVRSRV_SURFACE_DIMS));
		if (!psDimInt)
		{
			psDCDimQueryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDimQuery_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDimQueryIN->psDim, psDCDimQueryIN->ui32NumDims * sizeof(PVRSRV_SURFACE_DIMS))
				|| (OSCopyFromUser(NULL, psDimInt, psDCDimQueryIN->psDim,
				psDCDimQueryIN->ui32NumDims * sizeof(PVRSRV_SURFACE_DIMS)) != PVRSRV_OK) )
			{
				psDCDimQueryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDimQuery_exit;
			}
	if (psDCDimQueryIN->ui32NumDims != 0)
	{
		pui32SupportedInt = OSAllocMem(psDCDimQueryIN->ui32NumDims * sizeof(IMG_UINT32));
		if (!pui32SupportedInt)
		{
			psDCDimQueryOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDimQuery_exit;
		}
	}


				{
					/* Look up the address from the handle */
					psDCDimQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCDimQueryIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCDimQueryOUT->eError != PVRSRV_OK)
					{
						goto DCDimQuery_exit;
					}

					/* Look up the data from the resman address */
					psDCDimQueryOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCDimQueryOUT->eError != PVRSRV_OK)
					{
						goto DCDimQuery_exit;
					}
				}

	psDCDimQueryOUT->eError =
		DCDimQuery(
					psDeviceInt,
					psDCDimQueryIN->ui32NumDims,
					psDimInt,
					pui32SupportedInt);


	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*) psDCDimQueryOUT->pui32Supported, (psDCDimQueryIN->ui32NumDims * sizeof(IMG_UINT32)))
		|| (OSCopyToUser(NULL, psDCDimQueryOUT->pui32Supported, pui32SupportedInt,
		(psDCDimQueryIN->ui32NumDims * sizeof(IMG_UINT32))) != PVRSRV_OK) )
	{
		psDCDimQueryOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCDimQuery_exit;
	}


DCDimQuery_exit:
	if (psDimInt)
		OSFreeMem(psDimInt);
	if (pui32SupportedInt)
		OSFreeMem(pui32SupportedInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCSetBlank(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCSETBLANK *psDCSetBlankIN,
					 PVRSRV_BRIDGE_OUT_DCSETBLANK *psDCSetBlankOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCSETBLANK);





				{
					/* Look up the address from the handle */
					psDCSetBlankOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCSetBlankIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCSetBlankOUT->eError != PVRSRV_OK)
					{
						goto DCSetBlank_exit;
					}

					/* Look up the data from the resman address */
					psDCSetBlankOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCSetBlankOUT->eError != PVRSRV_OK)
					{
						goto DCSetBlank_exit;
					}
				}

	psDCSetBlankOUT->eError =
		DCSetBlank(
					psDeviceInt,
					psDCSetBlankIN->bEnabled);



DCSetBlank_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCSetVSyncReporting(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING *psDCSetVSyncReportingIN,
					 PVRSRV_BRIDGE_OUT_DCSETVSYNCREPORTING *psDCSetVSyncReportingOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCSETVSYNCREPORTING);





				{
					/* Look up the address from the handle */
					psDCSetVSyncReportingOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCSetVSyncReportingIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCSetVSyncReportingOUT->eError != PVRSRV_OK)
					{
						goto DCSetVSyncReporting_exit;
					}

					/* Look up the data from the resman address */
					psDCSetVSyncReportingOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCSetVSyncReportingOUT->eError != PVRSRV_OK)
					{
						goto DCSetVSyncReporting_exit;
					}
				}

	psDCSetVSyncReportingOUT->eError =
		DCSetVSyncReporting(
					psDeviceInt,
					psDCSetVSyncReportingIN->bEnabled);



DCSetVSyncReporting_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCLastVSyncQuery(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY *psDCLastVSyncQueryIN,
					 PVRSRV_BRIDGE_OUT_DCLASTVSYNCQUERY *psDCLastVSyncQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCLASTVSYNCQUERY);





				{
					/* Look up the address from the handle */
					psDCLastVSyncQueryOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCLastVSyncQueryIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCLastVSyncQueryOUT->eError != PVRSRV_OK)
					{
						goto DCLastVSyncQuery_exit;
					}

					/* Look up the data from the resman address */
					psDCLastVSyncQueryOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCLastVSyncQueryOUT->eError != PVRSRV_OK)
					{
						goto DCLastVSyncQuery_exit;
					}
				}

	psDCLastVSyncQueryOUT->eError =
		DCLastVSyncQuery(
					psDeviceInt,
					&psDCLastVSyncQueryOUT->i64Timestamp);



DCLastVSyncQuery_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCSystemBufferAcquire(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE *psDCSystemBufferAcquireIN,
					 PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE *psDCSystemBufferAcquireOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;
	DC_BUFFER * psBufferInt = IMG_NULL;
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERACQUIRE);





				{
					/* Look up the address from the handle */
					psDCSystemBufferAcquireOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCSystemBufferAcquireIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCSystemBufferAcquireOUT->eError != PVRSRV_OK)
					{
						goto DCSystemBufferAcquire_exit;
					}

					/* Look up the data from the resman address */
					psDCSystemBufferAcquireOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCSystemBufferAcquireOUT->eError != PVRSRV_OK)
					{
						goto DCSystemBufferAcquire_exit;
					}
				}

	psDCSystemBufferAcquireOUT->eError =
		DCSystemBufferAcquire(
					psDeviceInt,
					&psDCSystemBufferAcquireOUT->ui32Stride,
					&psBufferInt);
	/* Exit early if bridged call fails */
	if(psDCSystemBufferAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCSystemBufferAcquire_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hBufferInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_BUFFER,
												psBufferInt,
												(RESMAN_FREE_FN)&DCSystemBufferRelease);
	if (hBufferInt2 == IMG_NULL)
	{
		psDCSystemBufferAcquireOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCSystemBufferAcquire_exit;
	}
	psDCSystemBufferAcquireOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCSystemBufferAcquireOUT->hBuffer,
							(IMG_HANDLE) hBufferInt2,
							PVRSRV_HANDLE_TYPE_DC_BUFFER,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							);
	if (psDCSystemBufferAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCSystemBufferAcquire_exit;
	}


DCSystemBufferAcquire_exit:
	if (psDCSystemBufferAcquireOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hBufferInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hBufferInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psBufferInt)
		{
			DCSystemBufferRelease(psBufferInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCSystemBufferRelease(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE *psDCSystemBufferReleaseIN,
					 PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERRELEASE *psDCSystemBufferReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERRELEASE);





				{
					/* Look up the address from the handle */
					psDCSystemBufferReleaseOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBufferInt2,
											psDCSystemBufferReleaseIN->hBuffer,
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCSystemBufferReleaseOUT->eError != PVRSRV_OK)
					{
						goto DCSystemBufferRelease_exit;
					}

				}

	psDCSystemBufferReleaseOUT->eError = DCSystemBufferReleaseResManProxy(hBufferInt2);
	/* Exit early if bridged call fails */
	if(psDCSystemBufferReleaseOUT->eError != PVRSRV_OK)
	{
		goto DCSystemBufferRelease_exit;
	}

	psDCSystemBufferReleaseOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCSystemBufferReleaseIN->hBuffer,
					PVRSRV_HANDLE_TYPE_DC_BUFFER);


DCSystemBufferRelease_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDisplayContextCreate(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE *psDCDisplayContextCreateIN,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE *psDCDisplayContextCreateOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DEVICE * psDeviceInt = IMG_NULL;
	IMG_HANDLE hDeviceInt2 = IMG_NULL;
	DC_DISPLAY_CONTEXT * psDisplayContextInt = IMG_NULL;
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCREATE);





				{
					/* Look up the address from the handle */
					psDCDisplayContextCreateOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDeviceInt2,
											psDCDisplayContextCreateIN->hDevice,
											PVRSRV_HANDLE_TYPE_DC_DEVICE);
					if(psDCDisplayContextCreateOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextCreate_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextCreateOUT->eError = ResManFindPrivateDataByPtr(hDeviceInt2, (IMG_VOID **) &psDeviceInt);

					if(psDCDisplayContextCreateOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextCreate_exit;
					}
				}

	psDCDisplayContextCreateOUT->eError =
		DCDisplayContextCreate(
					psDeviceInt,
					&psDisplayContextInt);
	/* Exit early if bridged call fails */
	if(psDCDisplayContextCreateOUT->eError != PVRSRV_OK)
	{
		goto DCDisplayContextCreate_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hDisplayContextInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_DISPLAY_CONTEXT,
												psDisplayContextInt,
												(RESMAN_FREE_FN)&DCDisplayContextDestroy);
	if (hDisplayContextInt2 == IMG_NULL)
	{
		psDCDisplayContextCreateOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCDisplayContextCreate_exit;
	}
	psDCDisplayContextCreateOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCDisplayContextCreateOUT->hDisplayContext,
							(IMG_HANDLE) hDisplayContextInt2,
							PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCDisplayContextCreateOUT->eError != PVRSRV_OK)
	{
		goto DCDisplayContextCreate_exit;
	}


DCDisplayContextCreate_exit:
	if (psDCDisplayContextCreateOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hDisplayContextInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hDisplayContextInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psDisplayContextInt)
		{
			DCDisplayContextDestroy(psDisplayContextInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCDisplayContextConfigureCheck(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK *psDCDisplayContextConfigureCheckIN,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURECHECK *psDCDisplayContextConfigureCheckOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DISPLAY_CONTEXT * psDisplayContextInt = IMG_NULL;
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;
	PVRSRV_SURFACE_CONFIG_INFO *psSurfInfoInt = IMG_NULL;
	DC_BUFFER * *psBuffersInt = IMG_NULL;
	IMG_HANDLE *hBuffersInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURECHECK);




	if (psDCDisplayContextConfigureCheckIN->ui32PipeCount != 0)
	{
		psSurfInfoInt = OSAllocMem(psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO));
		if (!psSurfInfoInt)
		{
			psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigureCheck_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureCheckIN->psSurfInfo, psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO))
				|| (OSCopyFromUser(NULL, psSurfInfoInt, psDCDisplayContextConfigureCheckIN->psSurfInfo,
				psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigureCheck_exit;
			}
	if (psDCDisplayContextConfigureCheckIN->ui32PipeCount != 0)
	{
		psBuffersInt = OSAllocMem(psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(DC_BUFFER *));
		if (!psBuffersInt)
		{
			psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigureCheck_exit;
		}
		hBuffersInt2 = OSAllocMem(psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(IMG_HANDLE));
		if (!hBuffersInt2)
		{
			psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigureCheck_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureCheckIN->phBuffers, psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hBuffersInt2, psDCDisplayContextConfigureCheckIN->phBuffers,
				psDCDisplayContextConfigureCheckIN->ui32PipeCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigureCheck_exit;
			}

				{
					/* Look up the address from the handle */
					psDCDisplayContextConfigureCheckOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDisplayContextInt2,
											psDCDisplayContextConfigureCheckIN->hDisplayContext,
											PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);
					if(psDCDisplayContextConfigureCheckOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigureCheck_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextConfigureCheckOUT->eError = ResManFindPrivateDataByPtr(hDisplayContextInt2, (IMG_VOID **) &psDisplayContextInt);

					if(psDCDisplayContextConfigureCheckOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigureCheck_exit;
					}
				}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureCheckIN->ui32PipeCount;i++)
		{
				{
					/* Look up the address from the handle */
					psDCDisplayContextConfigureCheckOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBuffersInt2[i],
											psDCDisplayContextConfigureCheckIN->phBuffers[i],
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCDisplayContextConfigureCheckOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigureCheck_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextConfigureCheckOUT->eError = ResManFindPrivateDataByPtr(hBuffersInt2[i], (IMG_VOID **) &psBuffersInt[i]);

					if(psDCDisplayContextConfigureCheckOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigureCheck_exit;
					}
				}
		}
	}

	psDCDisplayContextConfigureCheckOUT->eError =
		DCDisplayContextConfigureCheck(
					psDisplayContextInt,
					psDCDisplayContextConfigureCheckIN->ui32PipeCount,
					psSurfInfoInt,
					psBuffersInt);



DCDisplayContextConfigureCheck_exit:
	if (psSurfInfoInt)
		OSFreeMem(psSurfInfoInt);
	if (psBuffersInt)
		OSFreeMem(psBuffersInt);
	if (hBuffersInt2)
		OSFreeMem(hBuffersInt2);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDisplayContextConfigure(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE *psDCDisplayContextConfigureIN,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURE *psDCDisplayContextConfigureOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DISPLAY_CONTEXT * psDisplayContextInt = IMG_NULL;
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;
	PVRSRV_SURFACE_CONFIG_INFO *psSurfInfoInt = IMG_NULL;
	DC_BUFFER * *psBuffersInt = IMG_NULL;
	IMG_HANDLE *hBuffersInt2 = IMG_NULL;
	SERVER_SYNC_PRIMITIVE * *psSyncInt = IMG_NULL;
	IMG_HANDLE *hSyncInt2 = IMG_NULL;
	IMG_BOOL *bUpdateInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURE);




	if (psDCDisplayContextConfigureIN->ui32PipeCount != 0)
	{
		psSurfInfoInt = OSAllocMem(psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO));
		if (!psSurfInfoInt)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureIN->psSurfInfo, psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO))
				|| (OSCopyFromUser(NULL, psSurfInfoInt, psDCDisplayContextConfigureIN->psSurfInfo,
				psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(PVRSRV_SURFACE_CONFIG_INFO)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}
	if (psDCDisplayContextConfigureIN->ui32PipeCount != 0)
	{
		psBuffersInt = OSAllocMem(psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(DC_BUFFER *));
		if (!psBuffersInt)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
		hBuffersInt2 = OSAllocMem(psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(IMG_HANDLE));
		if (!hBuffersInt2)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureIN->phBuffers, psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hBuffersInt2, psDCDisplayContextConfigureIN->phBuffers,
				psDCDisplayContextConfigureIN->ui32PipeCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}
	if (psDCDisplayContextConfigureIN->ui32SyncCount != 0)
	{
		psSyncInt = OSAllocMem(psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psSyncInt)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
		hSyncInt2 = OSAllocMem(psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_HANDLE));
		if (!hSyncInt2)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureIN->phSync, psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hSyncInt2, psDCDisplayContextConfigureIN->phSync,
				psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}
	if (psDCDisplayContextConfigureIN->ui32SyncCount != 0)
	{
		bUpdateInt = OSAllocMem(psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_BOOL));
		if (!bUpdateInt)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCDisplayContextConfigureIN->pbUpdate, psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_BOOL))
				|| (OSCopyFromUser(NULL, bUpdateInt, psDCDisplayContextConfigureIN->pbUpdate,
				psDCDisplayContextConfigureIN->ui32SyncCount * sizeof(IMG_BOOL)) != PVRSRV_OK) )
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}

				{
					/* Look up the address from the handle */
					psDCDisplayContextConfigureOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDisplayContextInt2,
											psDCDisplayContextConfigureIN->hDisplayContext,
											PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);
					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextConfigureOUT->eError = ResManFindPrivateDataByPtr(hDisplayContextInt2, (IMG_VOID **) &psDisplayContextInt);

					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}
				}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureIN->ui32PipeCount;i++)
		{
				{
					/* Look up the address from the handle */
					psDCDisplayContextConfigureOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBuffersInt2[i],
											psDCDisplayContextConfigureIN->phBuffers[i],
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextConfigureOUT->eError = ResManFindPrivateDataByPtr(hBuffersInt2[i], (IMG_VOID **) &psBuffersInt[i]);

					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}
				}
		}
	}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureIN->ui32SyncCount;i++)
		{
				{
					/* Look up the address from the handle */
					psDCDisplayContextConfigureOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hSyncInt2[i],
											psDCDisplayContextConfigureIN->phSync[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}

					/* Look up the data from the resman address */
					psDCDisplayContextConfigureOUT->eError = ResManFindPrivateDataByPtr(hSyncInt2[i], (IMG_VOID **) &psSyncInt[i]);

					if(psDCDisplayContextConfigureOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextConfigure_exit;
					}
				}
		}
	}

	psDCDisplayContextConfigureOUT->eError =
		DCDisplayContextConfigure(
					psDisplayContextInt,
					psDCDisplayContextConfigureIN->ui32PipeCount,
					psSurfInfoInt,
					psBuffersInt,
					psDCDisplayContextConfigureIN->ui32SyncCount,
					psSyncInt,
					bUpdateInt,
					psDCDisplayContextConfigureIN->ui32DisplayPeriod,
					psDCDisplayContextConfigureIN->ui32MaxDepth,
					psDCDisplayContextConfigureIN->i32AcquireFd,
					&psDCDisplayContextConfigureOUT->i32ReleaseFd);



DCDisplayContextConfigure_exit:
	if (psSurfInfoInt)
		OSFreeMem(psSurfInfoInt);
	if (psBuffersInt)
		OSFreeMem(psBuffersInt);
	if (hBuffersInt2)
		OSFreeMem(hBuffersInt2);
	if (psSyncInt)
		OSFreeMem(psSyncInt);
	if (hSyncInt2)
		OSFreeMem(hSyncInt2);
	if (bUpdateInt)
		OSFreeMem(bUpdateInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDisplayContextFlush(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTFLUSH *psDCDisplayContextFlushIN,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTFLUSH *psDCDisplayContextFlushOUT,
					 CONNECTION_DATA *psConnection)

{
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTFLUSH);

	psDCDisplayContextFlushOUT->eError = DCDisplayContextFlush();

	/* Exit early if bridged call fails */
	if(psDCDisplayContextFlushOUT->eError != PVRSRV_OK)
	{
		goto DCDisplayContextFlush_exit;
	}

DCDisplayContextFlush_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCDisplayContextDestroy(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY *psDCDisplayContextDestroyIN,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTDESTROY *psDCDisplayContextDestroyOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTDESTROY);





				{
					/* Look up the address from the handle */
					psDCDisplayContextDestroyOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDisplayContextInt2,
											psDCDisplayContextDestroyIN->hDisplayContext,
											PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);
					if(psDCDisplayContextDestroyOUT->eError != PVRSRV_OK)
					{
						goto DCDisplayContextDestroy_exit;
					}

				}

	psDCDisplayContextDestroyOUT->eError = DCDisplayContextDestroyResManProxy(hDisplayContextInt2);
	/* Exit early if bridged call fails */
	if(psDCDisplayContextDestroyOUT->eError != PVRSRV_OK)
	{
		goto DCDisplayContextDestroy_exit;
	}

	psDCDisplayContextDestroyOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCDisplayContextDestroyIN->hDisplayContext,
					PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);


DCDisplayContextDestroy_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferAlloc(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERALLOC *psDCBufferAllocIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERALLOC *psDCBufferAllocOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DISPLAY_CONTEXT * psDisplayContextInt = IMG_NULL;
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;
	DC_BUFFER * psBufferInt = IMG_NULL;
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERALLOC);





				{
					/* Look up the address from the handle */
					psDCBufferAllocOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDisplayContextInt2,
											psDCBufferAllocIN->hDisplayContext,
											PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);
					if(psDCBufferAllocOUT->eError != PVRSRV_OK)
					{
						goto DCBufferAlloc_exit;
					}

					/* Look up the data from the resman address */
					psDCBufferAllocOUT->eError = ResManFindPrivateDataByPtr(hDisplayContextInt2, (IMG_VOID **) &psDisplayContextInt);

					if(psDCBufferAllocOUT->eError != PVRSRV_OK)
					{
						goto DCBufferAlloc_exit;
					}
				}

	psDCBufferAllocOUT->eError =
		DCBufferAlloc(
					psDisplayContextInt,
					&psDCBufferAllocIN->sSurfInfo,
					&psDCBufferAllocOUT->ui32Stride,
					&psBufferInt);
	/* Exit early if bridged call fails */
	if(psDCBufferAllocOUT->eError != PVRSRV_OK)
	{
		goto DCBufferAlloc_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hBufferInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_BUFFER,
												psBufferInt,
												(RESMAN_FREE_FN)&DCBufferFree);
	if (hBufferInt2 == IMG_NULL)
	{
		psDCBufferAllocOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCBufferAlloc_exit;
	}
	psDCBufferAllocOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCBufferAllocOUT->hBuffer,
							(IMG_HANDLE) hBufferInt2,
							PVRSRV_HANDLE_TYPE_DC_BUFFER,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCBufferAllocOUT->eError != PVRSRV_OK)
	{
		goto DCBufferAlloc_exit;
	}


DCBufferAlloc_exit:
	if (psDCBufferAllocOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hBufferInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hBufferInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psBufferInt)
		{
			DCBufferFree(psBufferInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferImport(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERIMPORT *psDCBufferImportIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT *psDCBufferImportOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_DISPLAY_CONTEXT * psDisplayContextInt = IMG_NULL;
	IMG_HANDLE hDisplayContextInt2 = IMG_NULL;
	PMR * *psImportInt = IMG_NULL;
	IMG_HANDLE *hImportInt2 = IMG_NULL;
	DC_BUFFER * psBufferInt = IMG_NULL;
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERIMPORT);




	if (psDCBufferImportIN->ui32NumPlanes != 0)
	{
		psImportInt = OSAllocMem(psDCBufferImportIN->ui32NumPlanes * sizeof(PMR *));
		if (!psImportInt)
		{
			psDCBufferImportOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCBufferImport_exit;
		}
		hImportInt2 = OSAllocMem(psDCBufferImportIN->ui32NumPlanes * sizeof(IMG_HANDLE));
		if (!hImportInt2)
		{
			psDCBufferImportOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCBufferImport_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psDCBufferImportIN->phImport, psDCBufferImportIN->ui32NumPlanes * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hImportInt2, psDCBufferImportIN->phImport,
				psDCBufferImportIN->ui32NumPlanes * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psDCBufferImportOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCBufferImport_exit;
			}

				{
					/* Look up the address from the handle */
					psDCBufferImportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDisplayContextInt2,
											psDCBufferImportIN->hDisplayContext,
											PVRSRV_HANDLE_TYPE_DC_DISPLAY_CONTEXT);
					if(psDCBufferImportOUT->eError != PVRSRV_OK)
					{
						goto DCBufferImport_exit;
					}

					/* Look up the data from the resman address */
					psDCBufferImportOUT->eError = ResManFindPrivateDataByPtr(hDisplayContextInt2, (IMG_VOID **) &psDisplayContextInt);

					if(psDCBufferImportOUT->eError != PVRSRV_OK)
					{
						goto DCBufferImport_exit;
					}
				}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCBufferImportIN->ui32NumPlanes;i++)
		{
				{
					/* Look up the address from the handle */
					psDCBufferImportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hImportInt2[i],
											psDCBufferImportIN->phImport[i],
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psDCBufferImportOUT->eError != PVRSRV_OK)
					{
						goto DCBufferImport_exit;
					}

					/* Look up the data from the resman address */
					psDCBufferImportOUT->eError = ResManFindPrivateDataByPtr(hImportInt2[i], (IMG_VOID **) &psImportInt[i]);

					if(psDCBufferImportOUT->eError != PVRSRV_OK)
					{
						goto DCBufferImport_exit;
					}
				}
		}
	}

	psDCBufferImportOUT->eError =
		DCBufferImport(
					psDisplayContextInt,
					psDCBufferImportIN->ui32NumPlanes,
					psImportInt,
					&psDCBufferImportIN->sSurfAttrib,
					&psBufferInt);
	/* Exit early if bridged call fails */
	if(psDCBufferImportOUT->eError != PVRSRV_OK)
	{
		goto DCBufferImport_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hBufferInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_BUFFER,
												psBufferInt,
												(RESMAN_FREE_FN)&DCBufferFree);
	if (hBufferInt2 == IMG_NULL)
	{
		psDCBufferImportOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCBufferImport_exit;
	}
	psDCBufferImportOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCBufferImportOUT->hBuffer,
							(IMG_HANDLE) hBufferInt2,
							PVRSRV_HANDLE_TYPE_DC_BUFFER,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCBufferImportOUT->eError != PVRSRV_OK)
	{
		goto DCBufferImport_exit;
	}


DCBufferImport_exit:
	if (psDCBufferImportOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hBufferInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hBufferInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psBufferInt)
		{
			DCBufferFree(psBufferInt);
		}
	}

	if (psImportInt)
		OSFreeMem(psImportInt);
	if (hImportInt2)
		OSFreeMem(hImportInt2);

	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferFree(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERFREE *psDCBufferFreeIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERFREE *psDCBufferFreeOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERFREE);





				{
					/* Look up the address from the handle */
					psDCBufferFreeOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBufferInt2,
											psDCBufferFreeIN->hBuffer,
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCBufferFreeOUT->eError != PVRSRV_OK)
					{
						goto DCBufferFree_exit;
					}

				}

	psDCBufferFreeOUT->eError = DCBufferFreeResManProxy(hBufferInt2);
	/* Exit early if bridged call fails */
	if(psDCBufferFreeOUT->eError != PVRSRV_OK)
	{
		goto DCBufferFree_exit;
	}

	psDCBufferFreeOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCBufferFreeIN->hBuffer,
					PVRSRV_HANDLE_TYPE_DC_BUFFER);


DCBufferFree_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferUnimport(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT *psDCBufferUnimportIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERUNIMPORT *psDCBufferUnimportOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERUNIMPORT);





				{
					/* Look up the address from the handle */
					psDCBufferUnimportOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBufferInt2,
											psDCBufferUnimportIN->hBuffer,
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCBufferUnimportOUT->eError != PVRSRV_OK)
					{
						goto DCBufferUnimport_exit;
					}

				}

	psDCBufferUnimportOUT->eError = DCBufferUnimportResManProxy(hBufferInt2);
	/* Exit early if bridged call fails */
	if(psDCBufferUnimportOUT->eError != PVRSRV_OK)
	{
		goto DCBufferUnimport_exit;
	}

	psDCBufferUnimportOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCBufferUnimportIN->hBuffer,
					PVRSRV_HANDLE_TYPE_DC_BUFFER);


DCBufferUnimport_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferPin(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERPIN *psDCBufferPinIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERPIN *psDCBufferPinOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_BUFFER * psBufferInt = IMG_NULL;
	IMG_HANDLE hBufferInt2 = IMG_NULL;
	DC_PIN_HANDLE hPinHandleInt = IMG_NULL;
	IMG_HANDLE hPinHandleInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERPIN);





				{
					/* Look up the address from the handle */
					psDCBufferPinOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBufferInt2,
											psDCBufferPinIN->hBuffer,
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCBufferPinOUT->eError != PVRSRV_OK)
					{
						goto DCBufferPin_exit;
					}

					/* Look up the data from the resman address */
					psDCBufferPinOUT->eError = ResManFindPrivateDataByPtr(hBufferInt2, (IMG_VOID **) &psBufferInt);

					if(psDCBufferPinOUT->eError != PVRSRV_OK)
					{
						goto DCBufferPin_exit;
					}
				}

	psDCBufferPinOUT->eError =
		DCBufferPin(
					psBufferInt,
					&hPinHandleInt);
	/* Exit early if bridged call fails */
	if(psDCBufferPinOUT->eError != PVRSRV_OK)
	{
		goto DCBufferPin_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hPinHandleInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_DC_PIN_HANDLE,
												hPinHandleInt,
												(RESMAN_FREE_FN)&DCBufferUnpin);
	if (hPinHandleInt2 == IMG_NULL)
	{
		psDCBufferPinOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCBufferPin_exit;
	}
	psDCBufferPinOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCBufferPinOUT->hPinHandle,
							(IMG_HANDLE) hPinHandleInt2,
							PVRSRV_HANDLE_TYPE_DC_PIN_HANDLE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCBufferPinOUT->eError != PVRSRV_OK)
	{
		goto DCBufferPin_exit;
	}


DCBufferPin_exit:
	if (psDCBufferPinOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hPinHandleInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hPinHandleInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (hPinHandleInt)
		{
			DCBufferUnpin(hPinHandleInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferUnpin(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERUNPIN *psDCBufferUnpinIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERUNPIN *psDCBufferUnpinOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hPinHandleInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERUNPIN);





				{
					/* Look up the address from the handle */
					psDCBufferUnpinOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hPinHandleInt2,
											psDCBufferUnpinIN->hPinHandle,
											PVRSRV_HANDLE_TYPE_DC_PIN_HANDLE);
					if(psDCBufferUnpinOUT->eError != PVRSRV_OK)
					{
						goto DCBufferUnpin_exit;
					}

				}

	psDCBufferUnpinOUT->eError = DCBufferUnpinResManProxy(hPinHandleInt2);
	/* Exit early if bridged call fails */
	if(psDCBufferUnpinOUT->eError != PVRSRV_OK)
	{
		goto DCBufferUnpin_exit;
	}

	psDCBufferUnpinOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCBufferUnpinIN->hPinHandle,
					PVRSRV_HANDLE_TYPE_DC_PIN_HANDLE);


DCBufferUnpin_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferAcquire(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE *psDCBufferAcquireIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE *psDCBufferAcquireOUT,
					 CONNECTION_DATA *psConnection)
{
	DC_BUFFER * psBufferInt = IMG_NULL;
	IMG_HANDLE hBufferInt2 = IMG_NULL;
	PMR * psExtMemInt = IMG_NULL;
	IMG_HANDLE hExtMemInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERACQUIRE);





				{
					/* Look up the address from the handle */
					psDCBufferAcquireOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hBufferInt2,
											psDCBufferAcquireIN->hBuffer,
											PVRSRV_HANDLE_TYPE_DC_BUFFER);
					if(psDCBufferAcquireOUT->eError != PVRSRV_OK)
					{
						goto DCBufferAcquire_exit;
					}

					/* Look up the data from the resman address */
					psDCBufferAcquireOUT->eError = ResManFindPrivateDataByPtr(hBufferInt2, (IMG_VOID **) &psBufferInt);

					if(psDCBufferAcquireOUT->eError != PVRSRV_OK)
					{
						goto DCBufferAcquire_exit;
					}
				}

	psDCBufferAcquireOUT->eError =
		DCBufferAcquire(
					psBufferInt,
					&psExtMemInt);
	/* Exit early if bridged call fails */
	if(psDCBufferAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCBufferAcquire_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hExtMemInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_PMR,
												psExtMemInt,
												(RESMAN_FREE_FN)&DCBufferRelease);
	if (hExtMemInt2 == IMG_NULL)
	{
		psDCBufferAcquireOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto DCBufferAcquire_exit;
	}
	psDCBufferAcquireOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psDCBufferAcquireOUT->hExtMem,
							(IMG_HANDLE) hExtMemInt2,
							PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psDCBufferAcquireOUT->eError != PVRSRV_OK)
	{
		goto DCBufferAcquire_exit;
	}


DCBufferAcquire_exit:
	if (psDCBufferAcquireOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hExtMemInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hExtMemInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psExtMemInt)
		{
			DCBufferRelease(psExtMemInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeDCBufferRelease(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCBUFFERRELEASE *psDCBufferReleaseIN,
					 PVRSRV_BRIDGE_OUT_DCBUFFERRELEASE *psDCBufferReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hExtMemInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_DC_DCBUFFERRELEASE);





				{
					/* Look up the address from the handle */
					psDCBufferReleaseOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hExtMemInt2,
											psDCBufferReleaseIN->hExtMem,
											PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);
					if(psDCBufferReleaseOUT->eError != PVRSRV_OK)
					{
						goto DCBufferRelease_exit;
					}

				}

	psDCBufferReleaseOUT->eError = DCBufferReleaseResManProxy(hExtMemInt2);
	/* Exit early if bridged call fails */
	if(psDCBufferReleaseOUT->eError != PVRSRV_OK)
	{
		goto DCBufferRelease_exit;
	}

	psDCBufferReleaseOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psDCBufferReleaseIN->hExtMem,
					PVRSRV_HANDLE_TYPE_DEVMEM_MEM_IMPORT);


DCBufferRelease_exit:

	return 0;
}

#ifdef CONFIG_COMPAT
#include <linux/compat.h>

/*******************************************
            DCDevicesEnumerate
 *******************************************/

/* Bridge in structure for DCDevicesEnumerate */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE_TAG
{
	IMG_UINT32 ui32DeviceArraySize;
	/* Output pointer pui32DeviceIndex is also an implied input */
	/*IMG_UINT32 * pui32DeviceIndex;*/
	IMG_UINT32 pui32DeviceIndex;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE;


/* Bridge out structure for DCDevicesEnumerate */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE_TAG
{
	IMG_UINT32 ui32DeviceCount;
	/*IMG_UINT32 * pui32DeviceIndex;*/
	IMG_UINT32 pui32DeviceIndex;
	PVRSRV_ERROR eError;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE;

static IMG_INT
compat_PVRSRVBridgeDCDevicesEnumerate(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE *psDCDevicesEnumerateIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE *psDCDevicesEnumerateOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCDEVICESENUMERATE sDCDevicesEnumerateIN;
	PVRSRV_BRIDGE_OUT_DCDEVICESENUMERATE sDCDevicesEnumerateOUT;

	sDCDevicesEnumerateIN.ui32DeviceArraySize = psDCDevicesEnumerateIN_32->ui32DeviceArraySize;
	sDCDevicesEnumerateIN.pui32DeviceIndex = (IMG_UINT32 *)(IMG_UINT64)psDCDevicesEnumerateIN_32->pui32DeviceIndex;

	ret = PVRSRVBridgeDCDevicesEnumerate(ui32BridgeID, &sDCDevicesEnumerateIN,
					&sDCDevicesEnumerateOUT, psConnection);

	psDCDevicesEnumerateOUT_32->eError = sDCDevicesEnumerateOUT.eError;
	psDCDevicesEnumerateOUT_32->ui32DeviceCount = sDCDevicesEnumerateOUT.ui32DeviceCount;
	PVR_ASSERT(!((IMG_UINT64)sDCDevicesEnumerateOUT.pui32DeviceIndex & 0xFFFFFFFF00000000ULL));
	psDCDevicesEnumerateOUT_32->pui32DeviceIndex = (IMG_UINT32)(IMG_UINT64)sDCDevicesEnumerateOUT.pui32DeviceIndex;

	return ret;
}

/*******************************************
            DCDeviceAcquire
 *******************************************/

/* Bridge out structure for DCDeviceAcquire */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
	PVRSRV_ERROR eError;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE;

static IMG_INT
compat_PVRSRVBridgeDCDeviceAcquire(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DCDEVICEACQUIRE *psDCDeviceAcquireIN,
					 compat_PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE *psDCDeviceAcquireOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_OUT_DCDEVICEACQUIRE sDCDeviceAcquireOUT;

	ret = PVRSRVBridgeDCDeviceAcquire(ui32BridgeID, psDCDeviceAcquireIN,
					&sDCDeviceAcquireOUT, psConnection);

	PVR_ASSERT(!((IMG_UINT64)sDCDeviceAcquireOUT.hDevice & 0xFFFFFFFF00000000ULL));
	psDCDeviceAcquireOUT_32->hDevice = (IMG_UINT32)(IMG_UINT64)sDCDeviceAcquireOUT.hDevice;
	psDCDeviceAcquireOUT_32->eError= sDCDeviceAcquireOUT.eError;

	return ret;
}

/*******************************************
            DCDeviceRelease
 *******************************************/

/* Bridge in structure for DCDeviceRelease */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDEVICERELEASE_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCDEVICERELEASE;

static IMG_INT
compat_PVRSRVBridgeDCDeviceRelease(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDEVICERELEASE *psDCDeviceReleaseIN_32,
					 PVRSRV_BRIDGE_OUT_DCDEVICERELEASE *psDCDeviceReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCDEVICERELEASE sDCDeviceReleaseIN;

	sDCDeviceReleaseIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCDeviceReleaseIN_32->hDevice;

	return PVRSRVBridgeDCDeviceRelease(ui32BridgeID, &sDCDeviceReleaseIN,
					psDCDeviceReleaseOUT, psConnection);
}

/*******************************************
            DCGetInfo
 *******************************************/

/* Bridge in structure for DCGetInfo */
typedef struct compat_PVRSRV_BRIDGE_IN_DCGETINFO_TAG
{
	/*IMG_HANDLE hDevice;*/
    IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCGETINFO;

static IMG_INT
compat_PVRSRVBridgeDCGetInfo(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCGETINFO *psDCGetInfoIN_32,
					 PVRSRV_BRIDGE_OUT_DCGETINFO *psDCGetInfoOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCGETINFO sDCGetInfoIN;

	sDCGetInfoIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCGetInfoIN_32->hDevice;

	return PVRSRVBridgeDCGetInfo(ui32BridgeID, &sDCGetInfoIN,
					psDCGetInfoOUT, psConnection);
}

/*******************************************
            DCPanelQueryCount
 *******************************************/

/* Bridge in structure for DCPanelQueryCount */
typedef struct compat_PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT_TAG
{
	/*IMG_HANDLE hDevice;*/
    IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT;

static IMG_INT
compat_PVRSRVBridgeDCPanelQueryCount(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT *psDCPanelQueryCountIN_32,
					 PVRSRV_BRIDGE_OUT_DCPANELQUERYCOUNT *psDCPanelQueryCountOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCPANELQUERYCOUNT sDCPanelQueryCountIN;

	sDCPanelQueryCountIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCPanelQueryCountIN_32->hDevice;

	return PVRSRVBridgeDCPanelQueryCount(ui32BridgeID, &sDCPanelQueryCountIN,
					psDCPanelQueryCountOUT, psConnection);
}

/*******************************************
            DCPanelQuery
 *******************************************/

/* Bridge in structure for DCPanelQuery */
typedef struct compat_PVRSRV_BRIDGE_IN_DCPANELQUERY_TAG
{
	/*IMG_HANDLE hDevice;*/
    IMG_UINT32 hDevice;
	IMG_UINT32 ui32PanelsArraySize;
	/* Output pointer psPanelInfo is also an implied input */
	/*PVRSRV_PANEL_INFO * psPanelInfo;*/
	IMG_UINT32 psPanelInfo;
} compat_PVRSRV_BRIDGE_IN_DCPANELQUERY;


/* Bridge out structure for DCPanelQuery */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCPANELQUERY_TAG
{
	IMG_UINT32 ui32NumPanels;
	/*PVRSRV_PANEL_INFO * psPanelInfo;*/
	IMG_UINT32 psPanelInfo;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCPANELQUERY;

static IMG_INT
compat_PVRSRVBridgeDCPanelQuery(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCPANELQUERY *psDCPanelQueryIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCPANELQUERY *psDCPanelQueryOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCPANELQUERY sDCPanelQueryIN;
	PVRSRV_BRIDGE_OUT_DCPANELQUERY sDCPanelQueryOUT;

	sDCPanelQueryIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCPanelQueryIN_32->hDevice;
	sDCPanelQueryIN.ui32PanelsArraySize = psDCPanelQueryIN_32->ui32PanelsArraySize;
	sDCPanelQueryIN.psPanelInfo = (PVRSRV_PANEL_INFO *)(IMG_UINT64)psDCPanelQueryIN_32->psPanelInfo;

	ret = PVRSRVBridgeDCPanelQuery(ui32BridgeID, &sDCPanelQueryIN,
					&sDCPanelQueryOUT, psConnection);

	psDCPanelQueryOUT_32->eError = sDCPanelQueryOUT.eError;
	psDCPanelQueryOUT_32->ui32NumPanels = sDCPanelQueryOUT.ui32NumPanels;
	PVR_ASSERT(!((IMG_UINT64)sDCPanelQueryOUT.psPanelInfo & 0xFFFFFFFF00000000ULL));
	psDCPanelQueryOUT_32->psPanelInfo = (IMG_UINT32)(IMG_UINT64)sDCPanelQueryOUT.psPanelInfo;

	return ret;
}

/*******************************************
            DCFormatQuery
 *******************************************/

/* Bridge in structure for DCFormatQuery */
typedef struct compat_PVRSRV_BRIDGE_IN_DCFORMATQUERY_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
	IMG_UINT32 ui32NumFormats;
	/*PVRSRV_SURFACE_FORMAT * psFormat;*/
	IMG_UINT32 psFormat;
	/* Output pointer pui32Supported is also an implied input */
	/*IMG_UINT32 * pui32Supported;*/
	IMG_UINT32 pui32Supported;
} compat_PVRSRV_BRIDGE_IN_DCFORMATQUERY;


/* Bridge out structure for DCFormatQuery */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCFORMATQUERY_TAG
{
	/*IMG_UINT32 * pui32Supported;*/
	IMG_UINT32 pui32Supported;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCFORMATQUERY;

static IMG_INT
compat_PVRSRVBridgeDCFormatQuery(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCFORMATQUERY *psDCFormatQueryIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCFORMATQUERY *psDCFormatQueryOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCFORMATQUERY sDCFormatQueryIN;
	PVRSRV_BRIDGE_OUT_DCFORMATQUERY sDCFormatQueryOUT;

	sDCFormatQueryIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCFormatQueryIN_32->hDevice;
	sDCFormatQueryIN.ui32NumFormats = psDCFormatQueryIN_32->ui32NumFormats;
	sDCFormatQueryIN.psFormat = (PVRSRV_SURFACE_FORMAT *)(IMG_UINT64)psDCFormatQueryIN_32->psFormat;
	sDCFormatQueryIN.pui32Supported = (IMG_UINT32 *)(IMG_UINT64)psDCFormatQueryIN_32->pui32Supported;

	ret = PVRSRVBridgeDCFormatQuery(ui32BridgeID, &sDCFormatQueryIN,
					&sDCFormatQueryOUT, psConnection);

	psDCFormatQueryOUT_32->eError = sDCFormatQueryOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCFormatQueryOUT.pui32Supported & 0xFFFFFFFF00000000ULL));
	psDCFormatQueryOUT_32->pui32Supported = (IMG_UINT32)(IMG_UINT64)sDCFormatQueryOUT.pui32Supported;

	return ret;
}

/*******************************************
            DCDimQuery
 *******************************************/

/* Bridge in structure for DCDimQuery */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDIMQUERY_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
	IMG_UINT32 ui32NumDims;
	/*PVRSRV_SURFACE_DIMS * psDim;*/
	IMG_UINT32 psDim;
	/* Output pointer pui32Supported is also an implied input */
	/*IMG_UINT32 * pui32Supported;*/
	IMG_UINT32 pui32Supported;
} compat_PVRSRV_BRIDGE_IN_DCDIMQUERY;


/* Bridge out structure for DCDimQuery */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCDIMQUERY_TAG
{
	/*IMG_UINT32 * pui32Supported;*/
	IMG_UINT32 pui32Supported;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCDIMQUERY;


static IMG_INT
compat_PVRSRVBridgeDCDimQuery(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDIMQUERY *psDCDimQueryIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCDIMQUERY *psDCDimQueryOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCDIMQUERY sDCDimQueryIN;
	PVRSRV_BRIDGE_OUT_DCDIMQUERY sDCDimQueryOUT;

	sDCDimQueryIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCDimQueryIN_32->hDevice;
	sDCDimQueryIN.ui32NumDims = psDCDimQueryIN_32->ui32NumDims;
	sDCDimQueryIN.psDim = (PVRSRV_SURFACE_DIMS *)(IMG_UINT64)psDCDimQueryIN_32->psDim;
	sDCDimQueryIN.pui32Supported = (IMG_UINT32 *)(IMG_UINT64)psDCDimQueryIN_32->pui32Supported;

	ret = PVRSRVBridgeDCDimQuery(ui32BridgeID, &sDCDimQueryIN,
					&sDCDimQueryOUT, psConnection);

	psDCDimQueryOUT_32->eError = sDCDimQueryOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCDimQueryOUT.pui32Supported & 0xFFFFFFFF00000000ULL));
	psDCDimQueryOUT_32->pui32Supported = (IMG_UINT32)(IMG_UINT64)sDCDimQueryOUT.pui32Supported;

	return ret;
}

/*******************************************
            DCSetBlank
 *******************************************/

/* Bridge in structure for DCSetBlank */
typedef struct compat_PVRSRV_BRIDGE_IN_DCSETBLANK_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
	IMG_BOOL bEnabled;
} compat_PVRSRV_BRIDGE_IN_DCSETBLANK;

static IMG_INT
compat_PVRSRVBridgeDCSetBlank(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCSETBLANK *psDCSetBlankIN_32,
					 PVRSRV_BRIDGE_OUT_DCSETBLANK *psDCSetBlankOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCSETBLANK sDCSetBlankIN;

	sDCSetBlankIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCSetBlankIN_32->hDevice;
	sDCSetBlankIN.bEnabled = psDCSetBlankIN_32->bEnabled;

	return PVRSRVBridgeDCSetBlank(ui32BridgeID, &sDCSetBlankIN,
					psDCSetBlankOUT, psConnection);
}

/*******************************************
            DCSetVSyncReporting
 *******************************************/

/* Bridge in structure for DCSetVSyncReporting */
typedef struct compat_PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
	IMG_BOOL bEnabled;
} compat_PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING;

static IMG_INT
compat_PVRSRVBridgeDCSetVSyncReporting(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING *psDCSetVSyncReportingIN_32,
					 PVRSRV_BRIDGE_OUT_DCSETVSYNCREPORTING *psDCSetVSyncReportingOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCSETVSYNCREPORTING sDCSetVSyncReportingIN;

	sDCSetVSyncReportingIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCSetVSyncReportingIN_32->hDevice;
	sDCSetVSyncReportingIN.bEnabled = psDCSetVSyncReportingIN_32->bEnabled;

	return PVRSRVBridgeDCSetVSyncReporting(ui32BridgeID, &sDCSetVSyncReportingIN,
					psDCSetVSyncReportingOUT, psConnection);
}

/*******************************************
            DCLastVSyncQuery
 *******************************************/

/* Bridge in structure for DCLastVSyncQuery */
typedef struct compat_PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY;

static IMG_INT
compat_PVRSRVBridgeDCLastVSyncQuery(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY *psDCLastVSyncQueryIN_32,
					 PVRSRV_BRIDGE_OUT_DCLASTVSYNCQUERY *psDCLastVSyncQueryOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCLASTVSYNCQUERY sDCLastVSyncQueryIN;

	sDCLastVSyncQueryIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCLastVSyncQueryIN_32->hDevice;

	return PVRSRVBridgeDCLastVSyncQuery(ui32BridgeID, &sDCLastVSyncQueryIN,
					psDCLastVSyncQueryOUT, psConnection);
}

/*******************************************
            DCSystemBufferAcquire
 *******************************************/

/* Bridge in structure for DCSystemBufferAcquire */
typedef struct compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE;


/* Bridge out structure for DCSystemBufferAcquire */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE_TAG
{
	IMG_UINT32 ui32Stride;
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE;

static IMG_INT
compat_PVRSRVBridgeDCSystemBufferAcquire(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE *psDCSystemBufferAcquireIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE *psDCSystemBufferAcquireOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERACQUIRE sDCSystemBufferAcquireIN;
	PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERACQUIRE sDCSystemBufferAcquireOUT;

	sDCSystemBufferAcquireIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCSystemBufferAcquireIN_32->hDevice;

	ret = PVRSRVBridgeDCSystemBufferAcquire(ui32BridgeID, &sDCSystemBufferAcquireIN,
					&sDCSystemBufferAcquireOUT, psConnection);

	psDCSystemBufferAcquireOUT_32->eError = sDCSystemBufferAcquireOUT.eError;
	psDCSystemBufferAcquireOUT_32->ui32Stride = sDCSystemBufferAcquireOUT.ui32Stride;
	PVR_ASSERT(!((IMG_UINT64)sDCSystemBufferAcquireOUT.hBuffer & 0xFFFFFFFF00000000ULL));
	psDCSystemBufferAcquireOUT_32->hBuffer = (IMG_UINT32)(IMG_UINT64)sDCSystemBufferAcquireOUT.hBuffer;

	return ret;
}

/*******************************************
            DCSystemBufferRelease
 *******************************************/

/* Bridge in structure for DCSystemBufferRelease */
typedef struct compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE_TAG
{
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
} compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE;

static IMG_INT
compat_PVRSRVBridgeDCSystemBufferRelease(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE *psDCSystemBufferReleaseIN_32,
					 PVRSRV_BRIDGE_OUT_DCSYSTEMBUFFERRELEASE *psDCSystemBufferReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCSYSTEMBUFFERRELEASE sDCSystemBufferReleaseIN;

	sDCSystemBufferReleaseIN.hBuffer = (IMG_HANDLE)(IMG_UINT64)psDCSystemBufferReleaseIN_32->hBuffer;

	return PVRSRVBridgeDCSystemBufferRelease(ui32BridgeID, &sDCSystemBufferReleaseIN,
					psDCSystemBufferReleaseOUT, psConnection);
}

/*******************************************
            DCDisplayContextCreate
 *******************************************/

/* Bridge in structure for DCDisplayContextCreate */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE_TAG
{
	/*IMG_HANDLE hDevice;*/
	IMG_UINT32 hDevice;
} compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE;


/* Bridge out structure for DCDisplayContextCreate */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
	IMG_UINT32 hDisplayContext;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE;

static IMG_INT
compat_PVRSRVBridgeDCDisplayContextCreate(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE *psDCDisplayContextCreateIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE *psDCDisplayContextCreateOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCREATE sDCDisplayContextCreateIN;
	PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCREATE sDCDisplayContextCreateOUT;

	sDCDisplayContextCreateIN.hDevice = (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextCreateIN_32->hDevice;

	ret = PVRSRVBridgeDCDisplayContextCreate(ui32BridgeID, &sDCDisplayContextCreateIN,
					&sDCDisplayContextCreateOUT, psConnection);

	psDCDisplayContextCreateOUT_32->eError = sDCDisplayContextCreateOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCDisplayContextCreateOUT.hDisplayContext & 0xFFFFFFFF00000000ULL));
	psDCDisplayContextCreateOUT_32->hDisplayContext = (IMG_UINT32)(IMG_UINT64)sDCDisplayContextCreateOUT.hDisplayContext;

	return ret;
}

/*******************************************
            DCDisplayContextConfigureCheck
 *******************************************/

/* Bridge in structure for DCDisplayContextConfigureCheck */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
	IMG_UINT32 hDisplayContext;
	IMG_UINT32 ui32PipeCount;
	/*PVRSRV_SURFACE_CONFIG_INFO * psSurfInfo;*/
	IMG_UINT32 psSurfInfo;
	/*IMG_HANDLE * phBuffers;*/
	IMG_UINT32 phBuffers;
} compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK;

static IMG_INT
compat_PVRSRVBridgeDCDisplayContextConfigureCheck(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK *psDCDisplayContextConfigureCheckIN_32,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURECHECK *psDCDisplayContextConfigureCheckOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret = 0;
	IMG_HANDLE __user *hBuffersInt2 = IMG_NULL;
	IMG_UINT32 *hBuffersInt3 = IMG_NULL;
	PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURECHECK sDCDisplayContextConfigureCheckIN;

	sDCDisplayContextConfigureCheckIN.hDisplayContext = (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextConfigureCheckIN_32->hDisplayContext;
	sDCDisplayContextConfigureCheckIN.ui32PipeCount = psDCDisplayContextConfigureCheckIN_32->ui32PipeCount;
	sDCDisplayContextConfigureCheckIN.psSurfInfo = (PVRSRV_SURFACE_CONFIG_INFO *)(IMG_UINT64)psDCDisplayContextConfigureCheckIN_32->psSurfInfo;
	sDCDisplayContextConfigureCheckIN.phBuffers = (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextConfigureCheckIN_32->phBuffers;

	if (psDCDisplayContextConfigureCheckIN_32->ui32PipeCount != 0)
	{
		hBuffersInt2 = compat_alloc_user_space(psDCDisplayContextConfigureCheckIN_32->ui32PipeCount * sizeof(IMG_HANDLE));
		if (!hBuffersInt2)
		{
			psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigureCheck_exit;
		}
		hBuffersInt3 = OSAllocMem(psDCDisplayContextConfigureCheckIN_32->ui32PipeCount * sizeof(IMG_UINT32));
		if (!hBuffersInt3)
		{
			psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigureCheck_exit;
		}
	}

	/* Copy the data over */
	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*)(IMG_UINT64) psDCDisplayContextConfigureCheckIN_32->phBuffers, psDCDisplayContextConfigureCheckIN_32->ui32PipeCount * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hBuffersInt3, (IMG_HANDLE *)(IMG_UINT64)psDCDisplayContextConfigureCheckIN_32->phBuffers,
		psDCDisplayContextConfigureCheckIN_32->ui32PipeCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCDisplayContextConfigureCheck_exit;
	}


	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureCheckIN_32->ui32PipeCount;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hBuffersInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hBuffersInt3[i], &hBuffersInt2[i]))
			{
				psDCDisplayContextConfigureCheckOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigureCheck_exit;
			}
		}
		sDCDisplayContextConfigureCheckIN.phBuffers = hBuffersInt2;
	}

	ret = PVRSRVBridgeDCDisplayContextConfigureCheck(ui32BridgeID, &sDCDisplayContextConfigureCheckIN,
					psDCDisplayContextConfigureCheckOUT, psConnection);


DCDisplayContextConfigureCheck_exit:
	if (hBuffersInt3)
		OSFreeMem(hBuffersInt3);

	return ret;
}

/*******************************************
            DCDisplayContextConfigure
 *******************************************/

/* Bridge in structure for DCDisplayContextConfigure */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
	IMG_UINT32 hDisplayContext;
	IMG_UINT32 ui32PipeCount;
	/*PVRSRV_SURFACE_CONFIG_INFO * psSurfInfo;*/
	IMG_UINT32 psSurfInfo;
	/*IMG_HANDLE * phBuffers;*/
	IMG_UINT32 phBuffers;
	IMG_UINT32 ui32SyncCount;
	/*IMG_HANDLE * phSync;*/
	IMG_UINT32 phSync;
	/*IMG_BOOL * pbUpdate;*/
	IMG_UINT32 pbUpdate;
	IMG_UINT32 ui32DisplayPeriod;
	IMG_UINT32 ui32MaxDepth;
	IMG_INT32 i32AcquireFd;
} compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE;

static IMG_INT
compat_PVRSRVBridgeDCDisplayContextConfigure(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE *psDCDisplayContextConfigureIN_32,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTCONFIGURE *psDCDisplayContextConfigureOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret = 0;
	PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTCONFIGURE sDCDisplayContextConfigureIN;

	IMG_HANDLE __user *hBuffersInt2 = IMG_NULL;
	IMG_UINT32 *hBuffersInt3 = IMG_NULL;
	IMG_HANDLE __user *hSyncInt2 = IMG_NULL;
	IMG_UINT32 *hSyncInt3 = IMG_NULL;
	IMG_UINT64 alloc_user = 0;

	sDCDisplayContextConfigureIN.hDisplayContext =   (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextConfigureIN_32->hDisplayContext;
	sDCDisplayContextConfigureIN.ui32PipeCount =     psDCDisplayContextConfigureIN_32->ui32PipeCount;
	sDCDisplayContextConfigureIN.psSurfInfo =        (PVRSRV_SURFACE_CONFIG_INFO *)(IMG_UINT64)psDCDisplayContextConfigureIN_32->psSurfInfo;
	sDCDisplayContextConfigureIN.phBuffers =         (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextConfigureIN_32->phBuffers;
	sDCDisplayContextConfigureIN.ui32SyncCount =     psDCDisplayContextConfigureIN_32->ui32SyncCount;
	sDCDisplayContextConfigureIN.phSync =            (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextConfigureIN_32->phSync;
	sDCDisplayContextConfigureIN.pbUpdate =          (IMG_BOOL *)(IMG_UINT64)psDCDisplayContextConfigureIN_32->pbUpdate;
	sDCDisplayContextConfigureIN.ui32DisplayPeriod = psDCDisplayContextConfigureIN_32->ui32DisplayPeriod;
	sDCDisplayContextConfigureIN.ui32MaxDepth =      psDCDisplayContextConfigureIN_32->ui32MaxDepth;
	sDCDisplayContextConfigureIN.i32AcquireFd =      psDCDisplayContextConfigureIN_32->i32AcquireFd;

	if (psDCDisplayContextConfigureIN_32->ui32PipeCount != 0)
	{
	    alloc_user = psDCDisplayContextConfigureIN_32->ui32PipeCount * sizeof(IMG_HANDLE);
		hBuffersInt3 = OSAllocMem(psDCDisplayContextConfigureIN_32->ui32PipeCount * sizeof(IMG_UINT32));
		if (!hBuffersInt3)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*)(IMG_UINT64) psDCDisplayContextConfigureIN_32->phBuffers, psDCDisplayContextConfigureIN_32->ui32PipeCount * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hBuffersInt3, (IMG_HANDLE *)(IMG_UINT64)psDCDisplayContextConfigureIN_32->phBuffers,
		psDCDisplayContextConfigureIN_32->ui32PipeCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCDisplayContextConfigure_exit;
	}

	if (psDCDisplayContextConfigureIN_32->ui32SyncCount != 0)
	{
	    alloc_user += psDCDisplayContextConfigureIN_32->ui32SyncCount * sizeof(IMG_HANDLE);
		hSyncInt3 = OSAllocMem(psDCDisplayContextConfigureIN_32->ui32SyncCount * sizeof(IMG_UINT32));
		if (!hSyncInt3)
		{
			psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCDisplayContextConfigure_exit;
		}
	}

	hBuffersInt2 = compat_alloc_user_space(alloc_user);
	if (!hBuffersInt2)
	{
	    psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

	    goto DCDisplayContextConfigure_exit;
	}
	hSyncInt2 = hBuffersInt2 + psDCDisplayContextConfigureIN_32->ui32PipeCount;

	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*)(IMG_UINT64) psDCDisplayContextConfigureIN_32->phSync, psDCDisplayContextConfigureIN_32->ui32SyncCount * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hSyncInt3, (IMG_HANDLE *)(IMG_UINT64)psDCDisplayContextConfigureIN_32->phSync,
		psDCDisplayContextConfigureIN_32->ui32SyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCDisplayContextConfigure_exit;
	}


	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureIN_32->ui32PipeCount;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hBuffersInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hBuffersInt3[i], &hBuffersInt2[i]))
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}
		}
		sDCDisplayContextConfigureIN.phBuffers = hBuffersInt2;
	}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCDisplayContextConfigureIN_32->ui32SyncCount;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hSyncInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hSyncInt3[i], &hSyncInt2[i]))
			{
				psDCDisplayContextConfigureOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCDisplayContextConfigure_exit;
			}
		}
		sDCDisplayContextConfigureIN.phSync = hSyncInt2;
	}

	ret = PVRSRVBridgeDCDisplayContextConfigure(ui32BridgeID, &sDCDisplayContextConfigureIN,
						psDCDisplayContextConfigureOUT, psConnection);

DCDisplayContextConfigure_exit:
	if (hBuffersInt3)
		OSFreeMem(hBuffersInt3);
	if (hSyncInt3)
		OSFreeMem(hSyncInt3);

	return ret;
}

/*******************************************
            DCDisplayContextDestroy
 *******************************************/

/* Bridge in structure for DCDisplayContextDestroy */
typedef struct compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
	IMG_UINT32 hDisplayContext;
} compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY;

static IMG_INT
compat_PVRSRVBridgeDCDisplayContextDestroy(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY *psDCDisplayContextDestroyIN_32,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTDESTROY *psDCDisplayContextDestroyOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTDESTROY sDCDisplayContextDestroyIN;

	sDCDisplayContextDestroyIN.hDisplayContext = (IMG_HANDLE)(IMG_UINT64)psDCDisplayContextDestroyIN_32->hDisplayContext;

	return PVRSRVBridgeDCDisplayContextDestroy(ui32BridgeID, &sDCDisplayContextDestroyIN,
					psDCDisplayContextDestroyOUT, psConnection);
}

typedef struct compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTFLUSH_TAG
{
	IMG_UINT32 ui32Empty;
} compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTFLUSH;

static IMG_INT
compat_PVRSRVBridgeDCDisplayContextFlush(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTFLUSH *psDCDisplayContextFlushIN_32,
					 PVRSRV_BRIDGE_OUT_DCDISPLAYCONTEXTFLUSH *psDCDisplayContextFlushOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCDISPLAYCONTEXTFLUSH sDCDisplayContextFlushIN;

	return PVRSRVBridgeDCDisplayContextFlush(ui32BridgeID, &sDCDisplayContextFlushIN, psDCDisplayContextFlushOUT, psConnection);
}

/*******************************************
            DCBufferAlloc
 *******************************************/

/* Bridge in structure for DCBufferAlloc */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERALLOC_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
	IMG_UINT32 hDisplayContext;
	DC_BUFFER_CREATE_INFO sSurfInfo;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERALLOC;


/* Bridge out structure for DCBufferAlloc */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCBUFFERALLOC_TAG
{
	IMG_UINT32 ui32Stride;
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCBUFFERALLOC;

static IMG_INT
compat_PVRSRVBridgeDCBufferAlloc(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERALLOC *psDCBufferAllocIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCBUFFERALLOC *psDCBufferAllocOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCBUFFERALLOC sDCBufferAllocIN;
	PVRSRV_BRIDGE_OUT_DCBUFFERALLOC sDCBufferAllocOUT;

	sDCBufferAllocIN.hDisplayContext = (IMG_HANDLE)(IMG_UINT64)psDCBufferAllocIN_32->hDisplayContext;
	    memcpy((void *)&sDCBufferAllocIN.sSurfInfo, (const void *)&psDCBufferAllocIN_32->sSurfInfo, sizeof(DC_BUFFER_CREATE_INFO));

	ret = PVRSRVBridgeDCBufferAlloc(ui32BridgeID, &sDCBufferAllocIN,
					&sDCBufferAllocOUT, psConnection);

	psDCBufferAllocOUT_32->eError = sDCBufferAllocOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCBufferAllocOUT.hBuffer & 0xFFFFFFFF00000000ULL));
	psDCBufferAllocOUT_32->hBuffer = (IMG_UINT32)(IMG_UINT64)sDCBufferAllocOUT.hBuffer;
	psDCBufferAllocOUT_32->ui32Stride = sDCBufferAllocOUT.ui32Stride;

	return ret;
}

/*******************************************
            DCBufferImport
 *******************************************/

/* Bridge in structure for DCBufferImport */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERIMPORT_TAG
{
	/*IMG_HANDLE hDisplayContext;*/
    IMG_UINT32 hDisplayContext;
	IMG_UINT32 ui32NumPlanes;
	/*IMG_HANDLE * phImport;*/
	IMG_UINT32 phImport;
	DC_BUFFER_IMPORT_INFO sSurfAttrib;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERIMPORT;


/* Bridge out structure for DCBufferImport */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT_TAG
{
	/*IMG_HANDLE hBuffer;*/
    IMG_UINT32 hBuffer;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT;

static IMG_INT
compat_PVRSRVBridgeDCBufferImport(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERIMPORT *psDCBufferImportIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT *psDCBufferImportOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret = 0;
	PVRSRV_BRIDGE_IN_DCBUFFERIMPORT sDCBufferImportIN;
	PVRSRV_BRIDGE_OUT_DCBUFFERIMPORT sDCBufferImportOUT;

	IMG_HANDLE __user *hImportInt2 = IMG_NULL;
	IMG_UINT32 *hImportInt3 = IMG_NULL;

	sDCBufferImportIN.hDisplayContext = (IMG_HANDLE)(IMG_UINT64)psDCBufferImportIN_32->hDisplayContext;
	sDCBufferImportIN.ui32NumPlanes = psDCBufferImportIN_32->ui32NumPlanes;
	sDCBufferImportIN.phImport = (IMG_HANDLE)(IMG_UINT64)psDCBufferImportIN_32->phImport;
	memcpy((void *)&sDCBufferImportIN.sSurfAttrib, (const void *)&psDCBufferImportIN_32->sSurfAttrib, sizeof(DC_BUFFER_IMPORT_INFO));

	if (psDCBufferImportIN_32->ui32NumPlanes != 0)
	{
		hImportInt2 = compat_alloc_user_space(psDCBufferImportIN_32->ui32NumPlanes * sizeof(IMG_HANDLE));
		if (!hImportInt2)
		{
			psDCBufferImportOUT_32->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCBufferImport_exit;
		}
		hImportInt3 = OSAllocMem(psDCBufferImportIN_32->ui32NumPlanes * sizeof(IMG_UINT32));
		if (!hImportInt3)
		{
			psDCBufferImportOUT_32->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto DCBufferImport_exit;
		}
	}

	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*)(IMG_UINT64) psDCBufferImportIN_32->phImport, psDCBufferImportIN_32->ui32NumPlanes * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hImportInt3, (IMG_HANDLE *)(IMG_UINT64)psDCBufferImportIN_32->phImport,
		psDCBufferImportIN_32->ui32NumPlanes * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psDCBufferImportOUT_32->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto DCBufferImport_exit;
	}

	{
		IMG_UINT32 i;

		for (i=0;i<psDCBufferImportIN_32->ui32NumPlanes;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hImportInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hImportInt3[i], &hImportInt2[i]))
			{
				psDCBufferImportOUT_32->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto DCBufferImport_exit;
			}
		}
		sDCBufferImportIN.phImport = hImportInt2;
	}

	ret = PVRSRVBridgeDCBufferImport(ui32BridgeID, &sDCBufferImportIN,
				&sDCBufferImportOUT, psConnection);

	PVR_ASSERT(!((IMG_UINT64)sDCBufferImportOUT.hBuffer & 0xFFFFFFFF00000000ULL));
	psDCBufferImportOUT_32->hBuffer = (IMG_UINT32)(IMG_UINT64)sDCBufferImportOUT.hBuffer;
	psDCBufferImportOUT_32->eError= sDCBufferImportOUT.eError;

DCBufferImport_exit:

	if (hImportInt3)
		OSFreeMem(hImportInt3);

	return ret;
}



/*******************************************
            DCBufferFree
 *******************************************/

/* Bridge in structure for DCBufferFree */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERFREE_TAG
{
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERFREE;

static IMG_INT
compat_PVRSRVBridgeDCBufferFree(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERFREE *psDCBufferFreeIN_32,
					 PVRSRV_BRIDGE_OUT_DCBUFFERFREE *psDCBufferFreeOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCBUFFERFREE sDCBufferFreeIN;

	sDCBufferFreeIN.hBuffer = (IMG_HANDLE)(IMG_UINT64)psDCBufferFreeIN_32->hBuffer;

	return PVRSRVBridgeDCBufferFree(ui32BridgeID, &sDCBufferFreeIN,
					psDCBufferFreeOUT, psConnection);
}


/*******************************************
            DCBufferUnimport
 *******************************************/

/* Bridge in structure for DCBufferUnimport */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT_TAG
{
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT;

static IMG_INT
compat_PVRSRVBridgeDCBufferUnimport(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT *psDCBufferUnimportIN_32,
					 PVRSRV_BRIDGE_OUT_DCBUFFERUNIMPORT *psDCBufferUnimportOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCBUFFERUNIMPORT sDCBufferUnimportIN;

	sDCBufferUnimportIN.hBuffer = (IMG_HANDLE)(IMG_UINT64)psDCBufferUnimportIN_32->hBuffer;

	return PVRSRVBridgeDCBufferUnimport(ui32BridgeID, &sDCBufferUnimportIN,
					psDCBufferUnimportOUT, psConnection);
}



/*******************************************
            DCBufferPin
 *******************************************/

/* Bridge in structure for DCBufferPin */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERPIN_TAG
{
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERPIN;


/* Bridge out structure for DCBufferPin */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCBUFFERPIN_TAG
{
	/*IMG_HANDLE hPinHandle;*/
	IMG_UINT32 hPinHandle;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCBUFFERPIN;

static IMG_INT
compat_PVRSRVBridgeDCBufferPin(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERPIN *psDCBufferPinIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCBUFFERPIN *psDCBufferPinOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCBUFFERPIN sDCBufferPinIN;
	PVRSRV_BRIDGE_OUT_DCBUFFERPIN sDCBufferPinOUT;

	sDCBufferPinIN.hBuffer = (IMG_HANDLE)(IMG_UINT64)psDCBufferPinIN_32->hBuffer;

	ret = PVRSRVBridgeDCBufferPin(ui32BridgeID, &sDCBufferPinIN,
					&sDCBufferPinOUT, psConnection);

	psDCBufferPinOUT_32->eError = sDCBufferPinOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCBufferPinOUT.hPinHandle & 0xFFFFFFFF00000000ULL));
	psDCBufferPinOUT_32->hPinHandle = (IMG_UINT32)(IMG_UINT64)sDCBufferPinOUT.hPinHandle;

	return ret;
}



/*******************************************
            DCBufferUnpin
 *******************************************/

/* Bridge in structure for DCBufferUnpin */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERUNPIN_TAG
{
	/*IMG_HANDLE hPinHandle;*/
	IMG_UINT32 hPinHandle;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERUNPIN;

static IMG_INT
compat_PVRSRVBridgeDCBufferUnpin(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERUNPIN *psDCBufferUnpinIN_32,
					 PVRSRV_BRIDGE_OUT_DCBUFFERUNPIN *psDCBufferUnpinOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCBUFFERUNPIN sDCBufferUnpinIN;

	sDCBufferUnpinIN.hPinHandle = (IMG_HANDLE)(IMG_UINT64)psDCBufferUnpinIN_32->hPinHandle;

	return PVRSRVBridgeDCBufferUnpin(ui32BridgeID, &sDCBufferUnpinIN,
					psDCBufferUnpinOUT, psConnection);
}


/*******************************************
            DCBufferAcquire
 *******************************************/

/* Bridge in structure for DCBufferAcquire */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE_TAG
{
	/*IMG_HANDLE hBuffer;*/
	IMG_UINT32 hBuffer;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE;


/* Bridge out structure for DCBufferAcquire */
typedef struct compat_PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE_TAG
{
	/*IMG_HANDLE hExtMem;*/
	IMG_UINT32 hExtMem;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE;

static IMG_INT
compat_PVRSRVBridgeDCBufferAcquire(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE *psDCBufferAcquireIN_32,
					 compat_PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE *psDCBufferAcquireOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_DCBUFFERACQUIRE sDCBufferAcquireIN;
	PVRSRV_BRIDGE_OUT_DCBUFFERACQUIRE sDCBufferAcquireOUT;

	sDCBufferAcquireIN.hBuffer = (IMG_HANDLE)(IMG_UINT64)psDCBufferAcquireIN_32->hBuffer;

	ret = PVRSRVBridgeDCBufferAcquire(ui32BridgeID, &sDCBufferAcquireIN,
					&sDCBufferAcquireOUT, psConnection);

	psDCBufferAcquireOUT_32->eError = sDCBufferAcquireOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sDCBufferAcquireOUT.hExtMem & 0xFFFFFFFF00000000ULL));
	psDCBufferAcquireOUT_32->hExtMem = (IMG_UINT32)(IMG_UINT64)sDCBufferAcquireOUT.hExtMem;

	return ret;
}


/*******************************************
            DCBufferRelease
 *******************************************/

/* Bridge in structure for DCBufferRelease */
typedef struct compat_PVRSRV_BRIDGE_IN_DCBUFFERRELEASE_TAG
{
	/*IMG_HANDLE hExtMem;*/
	IMG_UINT32 hExtMem;
} compat_PVRSRV_BRIDGE_IN_DCBUFFERRELEASE;

static IMG_INT
compat_PVRSRVBridgeDCBufferRelease(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_DCBUFFERRELEASE *psDCBufferReleaseIN_32,
					 PVRSRV_BRIDGE_OUT_DCBUFFERRELEASE *psDCBufferReleaseOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_DCBUFFERRELEASE sDCBufferReleaseIN;

	sDCBufferReleaseIN.hExtMem = (IMG_HANDLE)(IMG_UINT64)psDCBufferReleaseIN_32->hExtMem;

	return PVRSRVBridgeDCBufferRelease(ui32BridgeID, &sDCBufferReleaseIN,
					 psDCBufferReleaseOUT, psConnection);
}



#endif

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterDCFunctions(IMG_VOID);
IMG_VOID UnregisterDCFunctions(IMG_VOID);

/*
 * Register all DC functions with services
 */
PVRSRV_ERROR RegisterDCFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICESQUERYCOUNT, PVRSRVBridgeDCDevicesQueryCount);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICESENUMERATE, compat_PVRSRVBridgeDCDevicesEnumerate);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICEACQUIRE, compat_PVRSRVBridgeDCDeviceAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICERELEASE, compat_PVRSRVBridgeDCDeviceRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCGETINFO, compat_PVRSRVBridgeDCGetInfo);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCPANELQUERYCOUNT, compat_PVRSRVBridgeDCPanelQueryCount);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCPANELQUERY, compat_PVRSRVBridgeDCPanelQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCFORMATQUERY, compat_PVRSRVBridgeDCFormatQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDIMQUERY, compat_PVRSRVBridgeDCDimQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSETBLANK, compat_PVRSRVBridgeDCSetBlank);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSETVSYNCREPORTING, compat_PVRSRVBridgeDCSetVSyncReporting);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCLASTVSYNCQUERY, compat_PVRSRVBridgeDCLastVSyncQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERACQUIRE, compat_PVRSRVBridgeDCSystemBufferAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERRELEASE, compat_PVRSRVBridgeDCSystemBufferRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCREATE, compat_PVRSRVBridgeDCDisplayContextCreate);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURECHECK, compat_PVRSRVBridgeDCDisplayContextConfigureCheck);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURE, compat_PVRSRVBridgeDCDisplayContextConfigure);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTDESTROY, compat_PVRSRVBridgeDCDisplayContextDestroy);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERALLOC, compat_PVRSRVBridgeDCBufferAlloc);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERIMPORT, compat_PVRSRVBridgeDCBufferImport);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERFREE, compat_PVRSRVBridgeDCBufferFree);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERUNIMPORT, compat_PVRSRVBridgeDCBufferUnimport);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERPIN, compat_PVRSRVBridgeDCBufferPin);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERUNPIN, compat_PVRSRVBridgeDCBufferUnpin);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERACQUIRE, compat_PVRSRVBridgeDCBufferAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERRELEASE, compat_PVRSRVBridgeDCBufferRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTFLUSH, compat_PVRSRVBridgeDCDisplayContextFlush);
#else
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICESQUERYCOUNT, PVRSRVBridgeDCDevicesQueryCount);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICESENUMERATE, PVRSRVBridgeDCDevicesEnumerate);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICEACQUIRE, PVRSRVBridgeDCDeviceAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDEVICERELEASE, PVRSRVBridgeDCDeviceRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCGETINFO, PVRSRVBridgeDCGetInfo);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCPANELQUERYCOUNT, PVRSRVBridgeDCPanelQueryCount);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCPANELQUERY, PVRSRVBridgeDCPanelQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCFORMATQUERY, PVRSRVBridgeDCFormatQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDIMQUERY, PVRSRVBridgeDCDimQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSETBLANK, PVRSRVBridgeDCSetBlank);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSETVSYNCREPORTING, PVRSRVBridgeDCSetVSyncReporting);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCLASTVSYNCQUERY, PVRSRVBridgeDCLastVSyncQuery);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERACQUIRE, PVRSRVBridgeDCSystemBufferAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCSYSTEMBUFFERRELEASE, PVRSRVBridgeDCSystemBufferRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCREATE, PVRSRVBridgeDCDisplayContextCreate);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURECHECK, PVRSRVBridgeDCDisplayContextConfigureCheck);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTCONFIGURE, PVRSRVBridgeDCDisplayContextConfigure);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTDESTROY, PVRSRVBridgeDCDisplayContextDestroy);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERALLOC, PVRSRVBridgeDCBufferAlloc);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERIMPORT, PVRSRVBridgeDCBufferImport);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERFREE, PVRSRVBridgeDCBufferFree);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERUNIMPORT, PVRSRVBridgeDCBufferUnimport);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERPIN, PVRSRVBridgeDCBufferPin);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERUNPIN, PVRSRVBridgeDCBufferUnpin);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERACQUIRE, PVRSRVBridgeDCBufferAcquire);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCBUFFERRELEASE, PVRSRVBridgeDCBufferRelease);
	SetDispatchTableEntry(PVRSRV_BRIDGE_DC_DCDISPLAYCONTEXTFLUSH, PVRSRVBridgeDCDisplayContextFlush);
#endif
	return PVRSRV_OK;
}

/*
 * Unregister all dc functions with services
 */
IMG_VOID UnregisterDCFunctions(IMG_VOID)
{
}
