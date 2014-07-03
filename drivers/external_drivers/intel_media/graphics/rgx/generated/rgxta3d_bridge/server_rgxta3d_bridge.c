/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxta3d
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxta3d
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

#include "rgxta3d.h"


#include "common_rgxta3d_bridge.h"

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
RGXDestroyHWRTDataResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
RGXDestroyRenderTargetResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
RGXDestroyZSBufferResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
RGXUnpopulateZSBufferResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
RGXDestroyFreeListResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
RGXDestroyRenderContextResManProxy(IMG_HANDLE hResmanItem)
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
PVRSRVBridgeRGXCreateHWRTData(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA *psRGXCreateHWRTDataIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA *psRGXCreateHWRTDataOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	RGX_FREELIST * *psapsFreeListsInt = IMG_NULL;
	IMG_HANDLE *hapsFreeListsInt2 = IMG_NULL;
	RGX_RTDATA_CLEANUP_DATA * psCleanupCookieInt = IMG_NULL;
	IMG_HANDLE hCleanupCookieInt2 = IMG_NULL;
	DEVMEM_MEMDESC * psRTACtlMemDescInt = IMG_NULL;
	DEVMEM_MEMDESC * pssHWRTDataMemDescInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATA);



	psRGXCreateHWRTDataOUT->hCleanupCookie = IMG_NULL;


	{
		psapsFreeListsInt = OSAllocMem(RGXFW_MAX_FREELISTS * sizeof(RGX_FREELIST *));
		if (!psapsFreeListsInt)
		{
			psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXCreateHWRTData_exit;
		}
		hapsFreeListsInt2 = OSAllocMem(RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE));
		if (!hapsFreeListsInt2)
		{
			psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXCreateHWRTData_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXCreateHWRTDataIN->phapsFreeLists, RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hapsFreeListsInt2, psRGXCreateHWRTDataIN->phapsFreeLists,
				RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateHWRTData_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXCreateHWRTDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateHWRTDataIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateHWRTData_exit;
					}

				}

	{
		IMG_UINT32 i;

		for (i=0;i<RGXFW_MAX_FREELISTS;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXCreateHWRTDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hapsFreeListsInt2[i],
											psRGXCreateHWRTDataIN->phapsFreeLists[i],
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
					if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateHWRTData_exit;
					}

					/* Look up the data from the resman address */
					psRGXCreateHWRTDataOUT->eError = ResManFindPrivateDataByPtr(hapsFreeListsInt2[i], (IMG_VOID **) &psapsFreeListsInt[i]);

					if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateHWRTData_exit;
					}
				}
		}
	}

	psRGXCreateHWRTDataOUT->eError =
		RGXCreateHWRTData(
					hDevNodeInt,
					psRGXCreateHWRTDataIN->ui32RenderTarget,
					psRGXCreateHWRTDataIN->sPMMlistDevVAddr,
					psRGXCreateHWRTDataIN->sVFPPageTableAddr,
					psapsFreeListsInt,
					&psCleanupCookieInt,
					&psRTACtlMemDescInt,
					psRGXCreateHWRTDataIN->ui32PPPScreen,
					psRGXCreateHWRTDataIN->ui32PPPGridOffset,
					psRGXCreateHWRTDataIN->ui64PPPMultiSampleCtl,
					psRGXCreateHWRTDataIN->ui32TPCStride,
					psRGXCreateHWRTDataIN->sTailPtrsDevVAddr,
					psRGXCreateHWRTDataIN->ui32TPCSize,
					psRGXCreateHWRTDataIN->ui32TEScreen,
					psRGXCreateHWRTDataIN->ui32TEAA,
					psRGXCreateHWRTDataIN->ui32TEMTILE1,
					psRGXCreateHWRTDataIN->ui32TEMTILE2,
					psRGXCreateHWRTDataIN->ui32MTileStride,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeLowerX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeLowerY,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeUpperX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeUpperY,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeScaleX,
					psRGXCreateHWRTDataIN->ui32ui32ISPMergeScaleY,
					psRGXCreateHWRTDataIN->ui16MaxRTs,
					&pssHWRTDataMemDescInt,
					&psRGXCreateHWRTDataOUT->ui32FWHWRTData);
	/* Exit early if bridged call fails */
	if(psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hCleanupCookieInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_FWIF_HWRTDATA,
												psCleanupCookieInt,
												(RESMAN_FREE_FN)&RGXDestroyHWRTData);
	if (hCleanupCookieInt2 == IMG_NULL)
	{
		psRGXCreateHWRTDataOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateHWRTData_exit;
	}
	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateHWRTDataOUT->hCleanupCookie,
							(IMG_HANDLE) hCleanupCookieInt2,
							PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}
	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,
							&psRGXCreateHWRTDataOUT->hRTACtlMemDesc,
							(IMG_HANDLE) psRTACtlMemDescInt,
							PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,psRGXCreateHWRTDataOUT->hCleanupCookie);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}
	psRGXCreateHWRTDataOUT->eError = PVRSRVAllocSubHandle(psConnection->psHandleBase,
							&psRGXCreateHWRTDataOUT->hsHWRTDataMemDesc,
							(IMG_HANDLE) pssHWRTDataMemDescInt,
							PVRSRV_HANDLE_TYPE_RGX_FW_MEMDESC,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							,psRGXCreateHWRTDataOUT->hCleanupCookie);
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateHWRTData_exit;
	}


RGXCreateHWRTData_exit:
	if (psRGXCreateHWRTDataOUT->eError != PVRSRV_OK)
	{
		if (psRGXCreateHWRTDataOUT->hCleanupCookie)
		{
			PVRSRVReleaseHandle(psConnection->psHandleBase,
						(IMG_HANDLE) psRGXCreateHWRTDataOUT->hCleanupCookie,
						PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);
		}

		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hCleanupCookieInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hCleanupCookieInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psCleanupCookieInt)
		{
			RGXDestroyHWRTData(psCleanupCookieInt);
		}
	}

	if (psapsFreeListsInt)
		OSFreeMem(psapsFreeListsInt);
	if (hapsFreeListsInt2)
		OSFreeMem(hapsFreeListsInt2);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyHWRTData(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hCleanupCookieInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATA);





				{
					/* Look up the address from the handle */
					psRGXDestroyHWRTDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hCleanupCookieInt2,
											psRGXDestroyHWRTDataIN->hCleanupCookie,
											PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);
					if(psRGXDestroyHWRTDataOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyHWRTData_exit;
					}

				}

	psRGXDestroyHWRTDataOUT->eError = RGXDestroyHWRTDataResManProxy(hCleanupCookieInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyHWRTDataOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyHWRTData_exit;
	}

	psRGXDestroyHWRTDataOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyHWRTDataIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);


RGXDestroyHWRTData_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateRenderTarget(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET *psRGXCreateRenderTargetIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET *psRGXCreateRenderTargetOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	RGX_RT_CLEANUP_DATA * pssRenderTargetMemDescInt = IMG_NULL;
	IMG_HANDLE hsRenderTargetMemDescInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERTARGET);





				{
					/* Look up the address from the handle */
					psRGXCreateRenderTargetOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateRenderTargetIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateRenderTarget_exit;
					}

				}

	psRGXCreateRenderTargetOUT->eError =
		RGXCreateRenderTarget(
					hDevNodeInt,
					psRGXCreateRenderTargetIN->spsVHeapTableDevVAddr,
					&pssRenderTargetMemDescInt,
					&psRGXCreateRenderTargetOUT->ui32sRenderTargetFWDevVAddr);
	/* Exit early if bridged call fails */
	if(psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderTarget_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hsRenderTargetMemDescInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_FWIF_RENDERTARGET,
												pssRenderTargetMemDescInt,
												(RESMAN_FREE_FN)&RGXDestroyRenderTarget);
	if (hsRenderTargetMemDescInt2 == IMG_NULL)
	{
		psRGXCreateRenderTargetOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateRenderTarget_exit;
	}
	psRGXCreateRenderTargetOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateRenderTargetOUT->hsRenderTargetMemDesc,
							(IMG_HANDLE) hsRenderTargetMemDescInt2,
							PVRSRV_HANDLE_TYPE_RGX_FWIF_RENDERTARGET,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderTarget_exit;
	}


RGXCreateRenderTarget_exit:
	if (psRGXCreateRenderTargetOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hsRenderTargetMemDescInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hsRenderTargetMemDescInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (pssRenderTargetMemDescInt)
		{
			RGXDestroyRenderTarget(pssRenderTargetMemDescInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyRenderTarget(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsRenderTargetMemDescInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERTARGET);





				{
					/* Look up the address from the handle */
					psRGXDestroyRenderTargetOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsRenderTargetMemDescInt2,
											psRGXDestroyRenderTargetIN->hsRenderTargetMemDesc,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_RENDERTARGET);
					if(psRGXDestroyRenderTargetOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyRenderTarget_exit;
					}

				}

	psRGXDestroyRenderTargetOUT->eError = RGXDestroyRenderTargetResManProxy(hsRenderTargetMemDescInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyRenderTargetOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyRenderTarget_exit;
	}

	psRGXDestroyRenderTargetOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyRenderTargetIN->hsRenderTargetMemDesc,
					PVRSRV_HANDLE_TYPE_RGX_FWIF_RENDERTARGET);


RGXDestroyRenderTarget_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateZSBuffer(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER *psRGXCreateZSBufferIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER *psRGXCreateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	DEVMEMINT_RESERVATION * psReservationInt = IMG_NULL;
	IMG_HANDLE hReservationInt2 = IMG_NULL;
	PMR * psPMRInt = IMG_NULL;
	IMG_HANDLE hPMRInt2 = IMG_NULL;
	RGX_ZSBUFFER_DATA * pssZSBufferKMInt = IMG_NULL;
	IMG_HANDLE hsZSBufferKMInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER);





				{
					/* Look up the address from the handle */
					psRGXCreateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateZSBufferIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXCreateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hReservationInt2,
											psRGXCreateZSBufferIN->hReservation,
											PVRSRV_HANDLE_TYPE_DEVMEMINT_RESERVATION);
					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}

					/* Look up the data from the resman address */
					psRGXCreateZSBufferOUT->eError = ResManFindPrivateDataByPtr(hReservationInt2, (IMG_VOID **) &psReservationInt);

					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}
				}

				{
					/* Look up the address from the handle */
					psRGXCreateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hPMRInt2,
											psRGXCreateZSBufferIN->hPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}

					/* Look up the data from the resman address */
					psRGXCreateZSBufferOUT->eError = ResManFindPrivateDataByPtr(hPMRInt2, (IMG_VOID **) &psPMRInt);

					if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateZSBuffer_exit;
					}
				}

	psRGXCreateZSBufferOUT->eError =
		RGXCreateZSBufferKM(
					hDevNodeInt,
					psReservationInt,
					psPMRInt,
					psRGXCreateZSBufferIN->uiMapFlags,
					&pssZSBufferKMInt,
					&psRGXCreateZSBufferOUT->ui32sZSBufferFWDevVAddr);
	/* Exit early if bridged call fails */
	if(psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateZSBuffer_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hsZSBufferKMInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_FWIF_ZSBUFFER,
												pssZSBufferKMInt,
												(RESMAN_FREE_FN)&RGXDestroyZSBufferKM);
	if (hsZSBufferKMInt2 == IMG_NULL)
	{
		psRGXCreateZSBufferOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateZSBuffer_exit;
	}
	psRGXCreateZSBufferOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateZSBufferOUT->hsZSBufferKM,
							(IMG_HANDLE) hsZSBufferKMInt2,
							PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateZSBuffer_exit;
	}


RGXCreateZSBuffer_exit:
	if (psRGXCreateZSBufferOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hsZSBufferKMInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hsZSBufferKMInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (pssZSBufferKMInt)
		{
			RGXDestroyZSBufferKM(pssZSBufferKMInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyZSBuffer(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsZSBufferMemDescInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER);





				{
					/* Look up the address from the handle */
					psRGXDestroyZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsZSBufferMemDescInt2,
											psRGXDestroyZSBufferIN->hsZSBufferMemDesc,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
					if(psRGXDestroyZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyZSBuffer_exit;
					}

				}

	psRGXDestroyZSBufferOUT->eError = RGXDestroyZSBufferResManProxy(hsZSBufferMemDescInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyZSBuffer_exit;
	}

	psRGXDestroyZSBufferOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyZSBufferIN->hsZSBufferMemDesc,
					PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);


RGXDestroyZSBuffer_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXPopulateZSBuffer(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferIN,
					 PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_ZSBUFFER_DATA * pssZSBufferKMInt = IMG_NULL;
	IMG_HANDLE hsZSBufferKMInt2 = IMG_NULL;
	RGX_POPULATION * pssPopulationInt = IMG_NULL;
	IMG_HANDLE hsPopulationInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER);





				{
					/* Look up the address from the handle */
					psRGXPopulateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsZSBufferKMInt2,
											psRGXPopulateZSBufferIN->hsZSBufferKM,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
					if(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXPopulateZSBuffer_exit;
					}

					/* Look up the data from the resman address */
					psRGXPopulateZSBufferOUT->eError = ResManFindPrivateDataByPtr(hsZSBufferKMInt2, (IMG_VOID **) &pssZSBufferKMInt);

					if(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXPopulateZSBuffer_exit;
					}
				}

	psRGXPopulateZSBufferOUT->eError =
		RGXPopulateZSBufferKM(
					pssZSBufferKMInt,
					&pssPopulationInt);
	/* Exit early if bridged call fails */
	if(psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXPopulateZSBuffer_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hsPopulationInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_POPULATION,
												pssPopulationInt,
												(RESMAN_FREE_FN)&RGXUnpopulateZSBufferKM);
	if (hsPopulationInt2 == IMG_NULL)
	{
		psRGXPopulateZSBufferOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXPopulateZSBuffer_exit;
	}
	psRGXPopulateZSBufferOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXPopulateZSBufferOUT->hsPopulation,
							(IMG_HANDLE) hsPopulationInt2,
							PVRSRV_HANDLE_TYPE_RGX_POPULATION,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXPopulateZSBuffer_exit;
	}


RGXPopulateZSBuffer_exit:
	if (psRGXPopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hsPopulationInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hsPopulationInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (pssPopulationInt)
		{
			RGXUnpopulateZSBufferKM(pssPopulationInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeRGXUnpopulateZSBuffer(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferIN,
					 PVRSRV_BRIDGE_OUT_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hsPopulationInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER);





				{
					/* Look up the address from the handle */
					psRGXUnpopulateZSBufferOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsPopulationInt2,
											psRGXUnpopulateZSBufferIN->hsPopulation,
											PVRSRV_HANDLE_TYPE_RGX_POPULATION);
					if(psRGXUnpopulateZSBufferOUT->eError != PVRSRV_OK)
					{
						goto RGXUnpopulateZSBuffer_exit;
					}

				}

	psRGXUnpopulateZSBufferOUT->eError = RGXUnpopulateZSBufferResManProxy(hsPopulationInt2);
	/* Exit early if bridged call fails */
	if(psRGXUnpopulateZSBufferOUT->eError != PVRSRV_OK)
	{
		goto RGXUnpopulateZSBuffer_exit;
	}

	psRGXUnpopulateZSBufferOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXUnpopulateZSBufferIN->hsPopulation,
					PVRSRV_HANDLE_TYPE_RGX_POPULATION);


RGXUnpopulateZSBuffer_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateFreeList(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATEFREELIST *psRGXCreateFreeListIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST *psRGXCreateFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	PMR * pssFreeListPMRInt = IMG_NULL;
	IMG_HANDLE hsFreeListPMRInt2 = IMG_NULL;
	RGX_FREELIST * psCleanupCookieInt = IMG_NULL;
	IMG_HANDLE hCleanupCookieInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST);





				{
					/* Look up the address from the handle */
					psRGXCreateFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateFreeListIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateFreeList_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXCreateFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsFreeListPMRInt2,
											psRGXCreateFreeListIN->hsFreeListPMR,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateFreeList_exit;
					}

					/* Look up the data from the resman address */
					psRGXCreateFreeListOUT->eError = ResManFindPrivateDataByPtr(hsFreeListPMRInt2, (IMG_VOID **) &pssFreeListPMRInt);

					if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateFreeList_exit;
					}
				}

	psRGXCreateFreeListOUT->eError =
		RGXCreateFreeList(
					hDevNodeInt,
					psRGXCreateFreeListIN->ui32ui32MaxFLPages,
					psRGXCreateFreeListIN->ui32ui32InitFLPages,
					psRGXCreateFreeListIN->ui32ui32GrowFLPages,
					psRGXCreateFreeListIN->bbFreeListCheck,
					psRGXCreateFreeListIN->spsFreeListDevVAddr,
					pssFreeListPMRInt,
					psRGXCreateFreeListIN->uiPMROffset,
					&psCleanupCookieInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateFreeList_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hCleanupCookieInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_FWIF_FREELIST,
												psCleanupCookieInt,
												(RESMAN_FREE_FN)&RGXDestroyFreeList);
	if (hCleanupCookieInt2 == IMG_NULL)
	{
		psRGXCreateFreeListOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateFreeList_exit;
	}
	psRGXCreateFreeListOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateFreeListOUT->hCleanupCookie,
							(IMG_HANDLE) hCleanupCookieInt2,
							PVRSRV_HANDLE_TYPE_RGX_FREELIST,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateFreeList_exit;
	}


RGXCreateFreeList_exit:
	if (psRGXCreateFreeListOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hCleanupCookieInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hCleanupCookieInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psCleanupCookieInt)
		{
			RGXDestroyFreeList(psCleanupCookieInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyFreeList(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST *psRGXDestroyFreeListIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYFREELIST *psRGXDestroyFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hCleanupCookieInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST);





				{
					/* Look up the address from the handle */
					psRGXDestroyFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hCleanupCookieInt2,
											psRGXDestroyFreeListIN->hCleanupCookie,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
					if(psRGXDestroyFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyFreeList_exit;
					}

				}

	psRGXDestroyFreeListOUT->eError = RGXDestroyFreeListResManProxy(hCleanupCookieInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyFreeListOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyFreeList_exit;
	}

	psRGXDestroyFreeListOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyFreeListIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_FREELIST);


RGXDestroyFreeList_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXAddBlockToFreeList(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListIN,
					 PVRSRV_BRIDGE_OUT_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_FREELIST * pssFreeListInt = IMG_NULL;
	IMG_HANDLE hsFreeListInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXADDBLOCKTOFREELIST);





				{
					/* Look up the address from the handle */
					psRGXAddBlockToFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsFreeListInt2,
											psRGXAddBlockToFreeListIN->hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
					if(psRGXAddBlockToFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXAddBlockToFreeList_exit;
					}

					/* Look up the data from the resman address */
					psRGXAddBlockToFreeListOUT->eError = ResManFindPrivateDataByPtr(hsFreeListInt2, (IMG_VOID **) &pssFreeListInt);

					if(psRGXAddBlockToFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXAddBlockToFreeList_exit;
					}
				}

	psRGXAddBlockToFreeListOUT->eError =
		RGXAddBlockToFreeListKM(
					pssFreeListInt,
					psRGXAddBlockToFreeListIN->ui3232NumPages);



RGXAddBlockToFreeList_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXRemoveBlockFromFreeList(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListIN,
					 PVRSRV_BRIDGE_OUT_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_FREELIST * pssFreeListInt = IMG_NULL;
	IMG_HANDLE hsFreeListInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXREMOVEBLOCKFROMFREELIST);





				{
					/* Look up the address from the handle */
					psRGXRemoveBlockFromFreeListOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hsFreeListInt2,
											psRGXRemoveBlockFromFreeListIN->hsFreeList,
											PVRSRV_HANDLE_TYPE_RGX_FREELIST);
					if(psRGXRemoveBlockFromFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXRemoveBlockFromFreeList_exit;
					}

					/* Look up the data from the resman address */
					psRGXRemoveBlockFromFreeListOUT->eError = ResManFindPrivateDataByPtr(hsFreeListInt2, (IMG_VOID **) &pssFreeListInt);

					if(psRGXRemoveBlockFromFreeListOUT->eError != PVRSRV_OK)
					{
						goto RGXRemoveBlockFromFreeList_exit;
					}
				}

	psRGXRemoveBlockFromFreeListOUT->eError =
		RGXRemoveBlockFromFreeListKM(
					pssFreeListInt);



RGXRemoveBlockFromFreeList_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCreateRenderContext(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_BYTE *psFrameworkCmdInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = IMG_NULL;
	IMG_HANDLE hRenderContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT);




	if (psRGXCreateRenderContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt = OSAllocMem(psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE));
		if (!psFrameworkCmdInt)
		{
			psRGXCreateRenderContextOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXCreateRenderContext_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXCreateRenderContextIN->psFrameworkCmd, psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psFrameworkCmdInt, psRGXCreateRenderContextIN->psFrameworkCmd,
				psRGXCreateRenderContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXCreateRenderContextOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateRenderContext_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXCreateRenderContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateRenderContextIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateRenderContext_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXCreateRenderContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hPrivDataInt,
											psRGXCreateRenderContextIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateRenderContext_exit;
					}

				}

	psRGXCreateRenderContextOUT->eError =
		PVRSRVRGXCreateRenderContextKM(psConnection,
					hDevNodeInt,
					psRGXCreateRenderContextIN->ui32Priority,
					psRGXCreateRenderContextIN->sMCUFenceAddr,
					psRGXCreateRenderContextIN->sVDMCallStackAddr,
					psRGXCreateRenderContextIN->ui32FrameworkCmdize,
					psFrameworkCmdInt,
					hPrivDataInt,
					&psRenderContextInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderContext_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hRenderContextInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_SERVER_RENDER_CONTEXT,
												psRenderContextInt,
												(RESMAN_FREE_FN)&PVRSRVRGXDestroyRenderContextKM);
	if (hRenderContextInt2 == IMG_NULL)
	{
		psRGXCreateRenderContextOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateRenderContext_exit;
	}
	psRGXCreateRenderContextOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateRenderContextOUT->hRenderContext,
							(IMG_HANDLE) hRenderContextInt2,
							PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateRenderContext_exit;
	}


RGXCreateRenderContext_exit:
	if (psRGXCreateRenderContextOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hRenderContextInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hRenderContextInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psRenderContextInt)
		{
			PVRSRVRGXDestroyRenderContextKM(psRenderContextInt);
		}
	}

	if (psFrameworkCmdInt)
		OSFreeMem(psFrameworkCmdInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyRenderContext(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hCleanupCookieInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT);





				{
					/* Look up the address from the handle */
					psRGXDestroyRenderContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hCleanupCookieInt2,
											psRGXDestroyRenderContextIN->hCleanupCookie,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
					if(psRGXDestroyRenderContextOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyRenderContext_exit;
					}

				}

	psRGXDestroyRenderContextOUT->eError = RGXDestroyRenderContextResManProxy(hCleanupCookieInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyRenderContextOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyRenderContext_exit;
	}

	psRGXDestroyRenderContextOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyRenderContextIN->hCleanupCookie,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);


RGXDestroyRenderContext_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXKickTA3D(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXKICKTA3D *psRGXKickTA3DIN,
					 PVRSRV_BRIDGE_OUT_RGXKICKTA3D *psRGXKickTA3DOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = IMG_NULL;
	IMG_HANDLE hRenderContextInt2 = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClientTAFenceUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32ClientTAFenceValueInt = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClientTAUpdateUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32ClientTAUpdateValueInt = IMG_NULL;
	IMG_UINT32 *ui32ServerTASyncFlagsInt = IMG_NULL;
	SERVER_SYNC_PRIMITIVE * *psServerTASyncsInt = IMG_NULL;
	IMG_HANDLE *hServerTASyncsInt2 = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClient3DFenceUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32Client3DFenceValueInt = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClient3DUpdateUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32Client3DUpdateValueInt = IMG_NULL;
	IMG_UINT32 *ui32Server3DSyncFlagsInt = IMG_NULL;
	SERVER_SYNC_PRIMITIVE * *psServer3DSyncsInt = IMG_NULL;
	IMG_HANDLE *hServer3DSyncsInt2 = IMG_NULL;
	IMG_INT32 *i32FenceFdsInt = IMG_NULL;
	IMG_BYTE *psTACmdInt = IMG_NULL;
	IMG_BYTE *ps3DPRCmdInt = IMG_NULL;
	IMG_BYTE *ps3DCmdInt = IMG_NULL;
	RGX_RTDATA_CLEANUP_DATA * psRTDataCleanupInt = IMG_NULL;
	IMG_HANDLE hRTDataCleanupInt2 = IMG_NULL;
	RGX_ZSBUFFER_DATA * psZBufferInt = IMG_NULL;
	IMG_HANDLE hZBufferInt2 = IMG_NULL;
	RGX_ZSBUFFER_DATA * psSBufferInt = IMG_NULL;
	IMG_HANDLE hSBufferInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D);




	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		sClientTAFenceUFOAddressInt = OSAllocMem(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClientTAFenceUFOAddressInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->psClientTAFenceUFOAddress, psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClientTAFenceUFOAddressInt, psRGXKickTA3DIN->psClientTAFenceUFOAddress,
				psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAFenceCount != 0)
	{
		ui32ClientTAFenceValueInt = OSAllocMem(psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAFenceValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32ClientTAFenceValue, psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAFenceValueInt, psRGXKickTA3DIN->pui32ClientTAFenceValue,
				psRGXKickTA3DIN->ui32ClientTAFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		sClientTAUpdateUFOAddressInt = OSAllocMem(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClientTAUpdateUFOAddressInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->psClientTAUpdateUFOAddress, psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClientTAUpdateUFOAddressInt, psRGXKickTA3DIN->psClientTAUpdateUFOAddress,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ClientTAUpdateCount != 0)
	{
		ui32ClientTAUpdateValueInt = OSAllocMem(psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32));
		if (!ui32ClientTAUpdateValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32ClientTAUpdateValue, psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientTAUpdateValueInt, psRGXKickTA3DIN->pui32ClientTAUpdateValue,
				psRGXKickTA3DIN->ui32ClientTAUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		ui32ServerTASyncFlagsInt = OSAllocMem(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32));
		if (!ui32ServerTASyncFlagsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32ServerTASyncFlags, psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerTASyncFlagsInt, psRGXKickTA3DIN->pui32ServerTASyncFlags,
				psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
		psServerTASyncsInt = OSAllocMem(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServerTASyncsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
		hServerTASyncsInt2 = OSAllocMem(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE));
		if (!hServerTASyncsInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->phServerTASyncs, psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServerTASyncsInt2, psRGXKickTA3DIN->phServerTASyncs,
				psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		sClient3DFenceUFOAddressInt = OSAllocMem(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClient3DFenceUFOAddressInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->psClient3DFenceUFOAddress, psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClient3DFenceUFOAddressInt, psRGXKickTA3DIN->psClient3DFenceUFOAddress,
				psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DFenceCount != 0)
	{
		ui32Client3DFenceValueInt = OSAllocMem(psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32));
		if (!ui32Client3DFenceValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32Client3DFenceValue, psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DFenceValueInt, psRGXKickTA3DIN->pui32Client3DFenceValue,
				psRGXKickTA3DIN->ui32Client3DFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		sClient3DUpdateUFOAddressInt = OSAllocMem(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClient3DUpdateUFOAddressInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->psClient3DUpdateUFOAddress, psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClient3DUpdateUFOAddressInt, psRGXKickTA3DIN->psClient3DUpdateUFOAddress,
				psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Client3DUpdateCount != 0)
	{
		ui32Client3DUpdateValueInt = OSAllocMem(psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32));
		if (!ui32Client3DUpdateValueInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32Client3DUpdateValue, psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Client3DUpdateValueInt, psRGXKickTA3DIN->pui32Client3DUpdateValue,
				psRGXKickTA3DIN->ui32Client3DUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		ui32Server3DSyncFlagsInt = OSAllocMem(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32));
		if (!ui32Server3DSyncFlagsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pui32Server3DSyncFlags, psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32Server3DSyncFlagsInt, psRGXKickTA3DIN->pui32Server3DSyncFlags,
				psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
		psServer3DSyncsInt = OSAllocMem(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServer3DSyncsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
		hServer3DSyncsInt2 = OSAllocMem(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE));
		if (!hServer3DSyncsInt2)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->phServer3DSyncs, psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServer3DSyncsInt2, psRGXKickTA3DIN->phServer3DSyncs,
				psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32NumFenceFds != 0)
	{
		i32FenceFdsInt = OSAllocMem(psRGXKickTA3DIN->ui32NumFenceFds * sizeof(IMG_INT32));
		if (!i32FenceFdsInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->pi32FenceFds, psRGXKickTA3DIN->ui32NumFenceFds * sizeof(IMG_INT32))
				|| (OSCopyFromUser(NULL, i32FenceFdsInt, psRGXKickTA3DIN->pi32FenceFds,
				psRGXKickTA3DIN->ui32NumFenceFds * sizeof(IMG_INT32)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui32TACmdSize != 0)
	{
		psTACmdInt = OSAllocMem(psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE));
		if (!psTACmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->psTACmd, psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psTACmdInt, psRGXKickTA3DIN->psTACmd,
				psRGXKickTA3DIN->ui32TACmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui323DPRCmdSize != 0)
	{
		ps3DPRCmdInt = OSAllocMem(psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE));
		if (!ps3DPRCmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->ps3DPRCmd, psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, ps3DPRCmdInt, psRGXKickTA3DIN->ps3DPRCmd,
				psRGXKickTA3DIN->ui323DPRCmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}
	if (psRGXKickTA3DIN->ui323DCmdSize != 0)
	{
		ps3DCmdInt = OSAllocMem(psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE));
		if (!ps3DCmdInt)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->ps3DCmd, psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, ps3DCmdInt, psRGXKickTA3DIN->ps3DCmd,
				psRGXKickTA3DIN->ui323DCmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickTA3D_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hRenderContextInt2,
											psRGXKickTA3DIN->hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hRenderContextInt2, (IMG_VOID **) &psRenderContextInt);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}

	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ServerTASyncPrims;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hServerTASyncsInt2[i],
											psRGXKickTA3DIN->phServerTASyncs[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hServerTASyncsInt2[i], (IMG_VOID **) &psServerTASyncsInt[i]);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}

	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Server3DSyncPrims;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hServer3DSyncsInt2[i],
											psRGXKickTA3DIN->phServer3DSyncs[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hServer3DSyncsInt2[i], (IMG_VOID **) &psServer3DSyncsInt[i]);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}
		}
	}

				if (psRGXKickTA3DIN->hRTDataCleanup)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hRTDataCleanupInt2,
											psRGXKickTA3DIN->hRTDataCleanup,
											PVRSRV_HANDLE_TYPE_RGX_RTDATA_CLEANUP);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hRTDataCleanupInt2, (IMG_VOID **) &psRTDataCleanupInt);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}

				if (psRGXKickTA3DIN->hZBuffer)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hZBufferInt2,
											psRGXKickTA3DIN->hZBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hZBufferInt2, (IMG_VOID **) &psZBufferInt);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}

				if (psRGXKickTA3DIN->hSBuffer)
				{
					/* Look up the address from the handle */
					psRGXKickTA3DOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hSBufferInt2,
											psRGXKickTA3DIN->hSBuffer,
											PVRSRV_HANDLE_TYPE_RGX_FWIF_ZSBUFFER);
					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickTA3DOUT->eError = ResManFindPrivateDataByPtr(hSBufferInt2, (IMG_VOID **) &psSBufferInt);

					if(psRGXKickTA3DOUT->eError != PVRSRV_OK)
					{
						goto RGXKickTA3D_exit;
					}
				}

	psRGXKickTA3DOUT->eError =
		PVRSRVRGXKickTA3DKM(
					psRenderContextInt,
					psRGXKickTA3DIN->ui32ClientTAFenceCount,
					sClientTAFenceUFOAddressInt,
					ui32ClientTAFenceValueInt,
					psRGXKickTA3DIN->ui32ClientTAUpdateCount,
					sClientTAUpdateUFOAddressInt,
					ui32ClientTAUpdateValueInt,
					psRGXKickTA3DIN->ui32ServerTASyncPrims,
					ui32ServerTASyncFlagsInt,
					psServerTASyncsInt,
					psRGXKickTA3DIN->ui32Client3DFenceCount,
					sClient3DFenceUFOAddressInt,
					ui32Client3DFenceValueInt,
					psRGXKickTA3DIN->ui32Client3DUpdateCount,
					sClient3DUpdateUFOAddressInt,
					ui32Client3DUpdateValueInt,
					psRGXKickTA3DIN->ui32Server3DSyncPrims,
					ui32Server3DSyncFlagsInt,
					psServer3DSyncsInt,
					psRGXKickTA3DIN->sPRFenceUFOAddress,
					psRGXKickTA3DIN->ui32FRFenceValue,
					psRGXKickTA3DIN->ui32NumFenceFds,
					i32FenceFdsInt,
					psRGXKickTA3DIN->ui32TACmdSize,
					psTACmdInt,
					psRGXKickTA3DIN->ui323DPRCmdSize,
					ps3DPRCmdInt,
					psRGXKickTA3DIN->ui323DCmdSize,
					ps3DCmdInt,
					psRGXKickTA3DIN->ui32TAFrameNum,
					psRGXKickTA3DIN->ui32TARTData,
					psRGXKickTA3DIN->bbLastTAInScene,
					psRGXKickTA3DIN->bbKickTA,
					psRGXKickTA3DIN->bbKickPR,
					psRGXKickTA3DIN->bbKick3D,
					psRGXKickTA3DIN->bbAbort,
					psRGXKickTA3DIN->bbPDumpContinuous,
					psRTDataCleanupInt,
					psZBufferInt,
					psSBufferInt,
					psRGXKickTA3DIN->bbCommitRefCountsTA,
					psRGXKickTA3DIN->bbCommitRefCounts3D,
					&psRGXKickTA3DOUT->bbCommittedRefCountsTA,
					&psRGXKickTA3DOUT->bbCommittedRefCounts3D);



RGXKickTA3D_exit:
	if (sClientTAFenceUFOAddressInt)
		OSFreeMem(sClientTAFenceUFOAddressInt);
	if (ui32ClientTAFenceValueInt)
		OSFreeMem(ui32ClientTAFenceValueInt);
	if (sClientTAUpdateUFOAddressInt)
		OSFreeMem(sClientTAUpdateUFOAddressInt);
	if (ui32ClientTAUpdateValueInt)
		OSFreeMem(ui32ClientTAUpdateValueInt);
	if (ui32ServerTASyncFlagsInt)
		OSFreeMem(ui32ServerTASyncFlagsInt);
	if (psServerTASyncsInt)
		OSFreeMem(psServerTASyncsInt);
	if (hServerTASyncsInt2)
		OSFreeMem(hServerTASyncsInt2);
	if (sClient3DFenceUFOAddressInt)
		OSFreeMem(sClient3DFenceUFOAddressInt);
	if (ui32Client3DFenceValueInt)
		OSFreeMem(ui32Client3DFenceValueInt);
	if (sClient3DUpdateUFOAddressInt)
		OSFreeMem(sClient3DUpdateUFOAddressInt);
	if (ui32Client3DUpdateValueInt)
		OSFreeMem(ui32Client3DUpdateValueInt);
	if (ui32Server3DSyncFlagsInt)
		OSFreeMem(ui32Server3DSyncFlagsInt);
	if (psServer3DSyncsInt)
		OSFreeMem(psServer3DSyncsInt);
	if (hServer3DSyncsInt2)
		OSFreeMem(hServer3DSyncsInt2);
	if (i32FenceFdsInt)
		OSFreeMem(i32FenceFdsInt);
	if (psTACmdInt)
		OSFreeMem(psTACmdInt);
	if (ps3DPRCmdInt)
		OSFreeMem(ps3DPRCmdInt);
	if (ps3DCmdInt)
		OSFreeMem(ps3DCmdInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXSetRenderContextPriority(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityIN,
					 PVRSRV_BRIDGE_OUT_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = IMG_NULL;
	IMG_HANDLE hRenderContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY);





				{
					/* Look up the address from the handle */
					psRGXSetRenderContextPriorityOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hRenderContextInt2,
											psRGXSetRenderContextPriorityIN->hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
					if(psRGXSetRenderContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetRenderContextPriority_exit;
					}

					/* Look up the data from the resman address */
					psRGXSetRenderContextPriorityOUT->eError = ResManFindPrivateDataByPtr(hRenderContextInt2, (IMG_VOID **) &psRenderContextInt);

					if(psRGXSetRenderContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetRenderContextPriority_exit;
					}
				}

	psRGXSetRenderContextPriorityOUT->eError =
		PVRSRVRGXSetRenderContextPriorityKM(psConnection,
					psRenderContextInt,
					psRGXSetRenderContextPriorityIN->ui32Priority);



RGXSetRenderContextPriority_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXGetLastRenderContextResetReason(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonIN,
					 PVRSRV_BRIDGE_OUT_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_RENDER_CONTEXT * psRenderContextInt = IMG_NULL;
	IMG_HANDLE hRenderContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON);





				{
					/* Look up the address from the handle */
					psRGXGetLastRenderContextResetReasonOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hRenderContextInt2,
											psRGXGetLastRenderContextResetReasonIN->hRenderContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_RENDER_CONTEXT);
					if(psRGXGetLastRenderContextResetReasonOUT->eError != PVRSRV_OK)
					{
						goto RGXGetLastRenderContextResetReason_exit;
					}

					/* Look up the data from the resman address */
					psRGXGetLastRenderContextResetReasonOUT->eError = ResManFindPrivateDataByPtr(hRenderContextInt2, (IMG_VOID **) &psRenderContextInt);

					if(psRGXGetLastRenderContextResetReasonOUT->eError != PVRSRV_OK)
					{
						goto RGXGetLastRenderContextResetReason_exit;
					}
				}

	psRGXGetLastRenderContextResetReasonOUT->eError =
		PVRSRVRGXGetLastRenderContextResetReasonKM(
					psRenderContextInt,
					&psRGXGetLastRenderContextResetReasonOUT->ui32LastResetReason);



RGXGetLastRenderContextResetReason_exit:

	return 0;
}

#ifdef CONFIG_COMPAT

#include <linux/compat.h>

/*******************************************
            RGXCreateHWRTData
 *******************************************/

/* Bridge in structure for RGXCreateHWRTData */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
	IMG_UINT32 ui32RenderTarget;
	IMG_DEV_VIRTADDR sPMMlistDevVAddr __attribute__ ((__packed__));
	IMG_DEV_VIRTADDR sVFPPageTableAddr __attribute__ ((__packed__));
	/*IMG_HANDLE * phapsFreeLists;*/
	IMG_UINT32 phapsFreeLists;
	IMG_UINT32 ui32PPPScreen;
	IMG_UINT32 ui32PPPGridOffset;
	IMG_UINT64 ui64PPPMultiSampleCtl __attribute__ ((__packed__));
	IMG_UINT32 ui32TPCStride;
	IMG_DEV_VIRTADDR sTailPtrsDevVAddr __attribute__ ((__packed__));
	IMG_UINT32 ui32TPCSize;
	IMG_UINT32 ui32TEScreen;
	IMG_UINT32 ui32TEAA;
	IMG_UINT32 ui32TEMTILE1;
	IMG_UINT32 ui32TEMTILE2;
	IMG_UINT32 ui32MTileStride;
	IMG_UINT32 ui32ui32ISPMergeLowerX;
	IMG_UINT32 ui32ui32ISPMergeLowerY;
	IMG_UINT32 ui32ui32ISPMergeUpperX;
	IMG_UINT32 ui32ui32ISPMergeUpperY;
	IMG_UINT32 ui32ui32ISPMergeScaleX;
	IMG_UINT32 ui32ui32ISPMergeScaleY;
	IMG_UINT16 ui16MaxRTs;
} compat_PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA;


/* Bridge out structure for RGXCreateHWRTData */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA_TAG
{
	/*IMG_HANDLE hCleanupCookie;*/
	/*IMG_HANDLE hRTACtlMemDesc;*/
	/*IMG_HANDLE hsHWRTDataMemDesc;*/
	IMG_UINT32 hCleanupCookie;
	IMG_UINT32 hRTACtlMemDesc;
	IMG_UINT32 hsHWRTDataMemDesc;
	IMG_UINT32 ui32FWHWRTData;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA;

static IMG_INT
compat_PVRSRVBridgeRGXCreateHWRTData(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA *psRGXCreateHWRTDataIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA *psRGXCreateHWRTDataOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	IMG_HANDLE __user *hapsFreeListsInt2 = IMG_NULL;
	IMG_UINT32 *hapsFreeListsInt3 = IMG_NULL;

	PVRSRV_BRIDGE_IN_RGXCREATEHWRTDATA sRGXCreateHWRTDataIN;
	PVRSRV_BRIDGE_OUT_RGXCREATEHWRTDATA sRGXCreateHWRTDataOUT;

	sRGXCreateHWRTDataOUT.hCleanupCookie = IMG_NULL;

	sRGXCreateHWRTDataIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateHWRTDataIN_32->hDevNode;
	sRGXCreateHWRTDataIN.ui32RenderTarget = psRGXCreateHWRTDataIN_32->ui32RenderTarget;
	sRGXCreateHWRTDataIN.sPMMlistDevVAddr.uiAddr = psRGXCreateHWRTDataIN_32->sPMMlistDevVAddr.uiAddr;
	sRGXCreateHWRTDataIN.sVFPPageTableAddr.uiAddr = psRGXCreateHWRTDataIN_32->sVFPPageTableAddr.uiAddr;
	sRGXCreateHWRTDataIN.phapsFreeLists = (IMG_HANDLE)(IMG_UINT64)psRGXCreateHWRTDataIN_32->phapsFreeLists;
	sRGXCreateHWRTDataIN.ui32PPPScreen = psRGXCreateHWRTDataIN_32->ui32PPPScreen;
	sRGXCreateHWRTDataIN.ui32PPPGridOffset = psRGXCreateHWRTDataIN_32->ui32PPPGridOffset;
	sRGXCreateHWRTDataIN.ui64PPPMultiSampleCtl = psRGXCreateHWRTDataIN_32->ui64PPPMultiSampleCtl;
	sRGXCreateHWRTDataIN.ui32TPCStride = psRGXCreateHWRTDataIN_32->ui32TPCStride;
	sRGXCreateHWRTDataIN.sTailPtrsDevVAddr.uiAddr = psRGXCreateHWRTDataIN_32->sTailPtrsDevVAddr.uiAddr;
	sRGXCreateHWRTDataIN.ui32TPCSize = psRGXCreateHWRTDataIN_32->ui32TPCSize;
	sRGXCreateHWRTDataIN.ui32TEScreen = psRGXCreateHWRTDataIN_32->ui32TEScreen;
	sRGXCreateHWRTDataIN.ui32TEAA = psRGXCreateHWRTDataIN_32->ui32TEAA;
	sRGXCreateHWRTDataIN.ui32TEMTILE1 = psRGXCreateHWRTDataIN_32->ui32TEMTILE1;
	sRGXCreateHWRTDataIN.ui32TEMTILE2 = psRGXCreateHWRTDataIN_32->ui32TEMTILE2;
	sRGXCreateHWRTDataIN.ui32MTileStride = psRGXCreateHWRTDataIN_32->ui32MTileStride;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeLowerX = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeLowerX;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeLowerY = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeLowerY;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeUpperX = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeUpperX;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeUpperY = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeUpperY;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeScaleX = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeScaleX;
	sRGXCreateHWRTDataIN.ui32ui32ISPMergeScaleY = psRGXCreateHWRTDataIN_32->ui32ui32ISPMergeScaleY;
	sRGXCreateHWRTDataIN.ui16MaxRTs = psRGXCreateHWRTDataIN_32->ui16MaxRTs;

	hapsFreeListsInt2 = compat_alloc_user_space(RGXFW_MAX_FREELISTS * sizeof(IMG_HANDLE));
	if (!hapsFreeListsInt2)
	{
		sRGXCreateHWRTDataOUT.eError = PVRSRV_ERROR_OUT_OF_MEMORY;

		goto compat_RGXCreateHWRTData_exit;
	}

	hapsFreeListsInt3 = OSAllocMem(RGXFW_MAX_FREELISTS * sizeof(IMG_UINT32));
	if (!hapsFreeListsInt3)
	{
		sRGXCreateHWRTDataOUT.eError = PVRSRV_ERROR_OUT_OF_MEMORY;

		goto compat_RGXCreateHWRTData_exit;
	}

	/* Copy the data over */
	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*)(IMG_UINT64) psRGXCreateHWRTDataIN_32->phapsFreeLists, RGXFW_MAX_FREELISTS * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hapsFreeListsInt3, (IMG_VOID*)(IMG_UINT64)psRGXCreateHWRTDataIN_32->phapsFreeLists,
		RGXFW_MAX_FREELISTS * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		sRGXCreateHWRTDataOUT.eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto compat_RGXCreateHWRTData_exit;
	}


	{
		IMG_UINT32 i;

		for (i=0;i<RGXFW_MAX_FREELISTS;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hapsFreeListsInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hapsFreeListsInt3[i], &hapsFreeListsInt2[i]))
			{
				sRGXCreateHWRTDataOUT.eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto compat_RGXCreateHWRTData_exit;
			}
		}
		sRGXCreateHWRTDataIN.phapsFreeLists = hapsFreeListsInt2;
	}

	ret = PVRSRVBridgeRGXCreateHWRTData(ui32BridgeID, &sRGXCreateHWRTDataIN,
						&sRGXCreateHWRTDataOUT, psConnection);

	PVR_ASSERT(!((IMG_UINT64)sRGXCreateHWRTDataOUT.hCleanupCookie & 0xFFFFFFFF00000000ULL));
	psRGXCreateHWRTDataOUT_32->hCleanupCookie = (IMG_UINT32)(IMG_UINT64)sRGXCreateHWRTDataOUT.hCleanupCookie;
	PVR_ASSERT(!((IMG_UINT64)sRGXCreateHWRTDataOUT.hRTACtlMemDesc & 0xFFFFFFFF00000000ULL));
	psRGXCreateHWRTDataOUT_32->hRTACtlMemDesc = (IMG_UINT32)(IMG_UINT64)sRGXCreateHWRTDataOUT.hRTACtlMemDesc;
	PVR_ASSERT(!((IMG_UINT64)sRGXCreateHWRTDataOUT.hsHWRTDataMemDesc & 0xFFFFFFFF00000000ULL));
	psRGXCreateHWRTDataOUT_32->hsHWRTDataMemDesc = (IMG_UINT32)(IMG_UINT64)sRGXCreateHWRTDataOUT.hsHWRTDataMemDesc;
	psRGXCreateHWRTDataOUT_32->ui32FWHWRTData = sRGXCreateHWRTDataOUT.ui32FWHWRTData;

compat_RGXCreateHWRTData_exit:

	psRGXCreateHWRTDataOUT_32->eError= sRGXCreateHWRTDataOUT.eError;

	if (hapsFreeListsInt3)
		OSFreeMem(hapsFreeListsInt3);

	return ret;
}


/*******************************************
            RGXDestroyHWRTData
 *******************************************/

/* Bridge in structure for RGXDestroyHWRTData */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA_TAG
{
	/*IMG_HANDLE hCleanupCookie;*/
	IMG_UINT32 hCleanupCookie;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA;


static IMG_INT
compat_PVRSRVBridgeRGXDestroyHWRTData(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYHWRTDATA *psRGXDestroyHWRTDataOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYHWRTDATA sRGXDestroyHWRTDataIN;

	sRGXDestroyHWRTDataIN.hCleanupCookie = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyHWRTDataIN_32->hCleanupCookie;

	return PVRSRVBridgeRGXDestroyHWRTData( ui32BridgeID, &sRGXDestroyHWRTDataIN,
					 psRGXDestroyHWRTDataOUT, psConnection);
}


/*******************************************
            RGXCreateRenderTarget
 *******************************************/

/* Bridge in structure for RGXCreateRenderTarget */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
	IMG_DEV_VIRTADDR spsVHeapTableDevVAddr __attribute__ ((__packed__));
} compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET;


/* Bridge out structure for RGXCreateRenderTarget */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET_TAG
{
	/*IMG_HANDLE hsRenderTargetMemDesc;*/
	IMG_UINT32 hsRenderTargetMemDesc;
	IMG_UINT32 ui32sRenderTargetFWDevVAddr;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET;

static IMG_INT
compat_PVRSRVBridgeRGXCreateRenderTarget(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET *psRGXCreateRenderTargetIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET *psRGXCreateRenderTargetOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXCREATERENDERTARGET sRGXCreateRenderTargetIN;
	PVRSRV_BRIDGE_OUT_RGXCREATERENDERTARGET sRGXCreateRenderTargetOUT;

	sRGXCreateRenderTargetIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateRenderTargetIN_32->hDevNode;
	sRGXCreateRenderTargetIN.spsVHeapTableDevVAddr.uiAddr = psRGXCreateRenderTargetIN_32->spsVHeapTableDevVAddr.uiAddr;

	ret = PVRSRVBridgeRGXCreateRenderTarget(ui32BridgeID, &sRGXCreateRenderTargetIN,
					&sRGXCreateRenderTargetOUT, psConnection);

	psRGXCreateRenderTargetOUT_32->eError = sRGXCreateRenderTargetOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sRGXCreateRenderTargetOUT.hsRenderTargetMemDesc & 0xFFFFFFFF00000000ULL));
	psRGXCreateRenderTargetOUT_32->hsRenderTargetMemDesc= (IMG_UINT32)(IMG_UINT64)sRGXCreateRenderTargetOUT.hsRenderTargetMemDesc;
	psRGXCreateRenderTargetOUT_32->ui32sRenderTargetFWDevVAddr = sRGXCreateRenderTargetOUT.ui32sRenderTargetFWDevVAddr;

	return ret;
}



/*******************************************
            RGXDestroyRenderTarget
 *******************************************/

/* Bridge in structure for RGXDestroyRenderTarget */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET_TAG
{
	/*IMG_HANDLE hsRenderTargetMemDesc;*/
	IMG_UINT32 hsRenderTargetMemDesc;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET;


static IMG_INT
compat_PVRSRVBridgeRGXDestroyRenderTarget(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERTARGET *psRGXDestroyRenderTargetOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYRENDERTARGET sRGXDestroyRenderTargetIN;

	sRGXDestroyRenderTargetIN.hsRenderTargetMemDesc = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyRenderTargetIN_32->hsRenderTargetMemDesc;

	return PVRSRVBridgeRGXDestroyRenderTarget(ui32BridgeID, &sRGXDestroyRenderTargetIN,
					psRGXDestroyRenderTargetOUT, psConnection);
}



/*******************************************
            RGXCreateZSBuffer
 *******************************************/

/* Bridge in structure for RGXCreateZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER_TAG
{
	/*IMG_HANDLE hDevNode;*/
	/*IMG_HANDLE hReservation;*/
	/*IMG_HANDLE hPMR;*/
	IMG_UINT32 hDevNode;
	IMG_UINT32 hReservation;
	IMG_UINT32 hPMR;
	PVRSRV_MEMALLOCFLAGS_T uiMapFlags;
} compat_PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER;


/* Bridge out structure for RGXCreateZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER_TAG
{
	/*IMG_HANDLE hsZSBufferKM;*/
	IMG_UINT32 hsZSBufferKM;
	IMG_UINT32 ui32sZSBufferFWDevVAddr;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER;

static IMG_INT
compat_PVRSRVBridgeRGXCreateZSBuffer(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER *psRGXCreateZSBufferIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER *psRGXCreateZSBufferOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXCREATEZSBUFFER sRGXCreateZSBufferIN;
	PVRSRV_BRIDGE_OUT_RGXCREATEZSBUFFER sRGXCreateZSBufferOUT;

	sRGXCreateZSBufferIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateZSBufferIN_32->hDevNode;
	sRGXCreateZSBufferIN.hReservation= (IMG_HANDLE)(IMG_UINT64)psRGXCreateZSBufferIN_32->hReservation;
	sRGXCreateZSBufferIN.hPMR= (IMG_HANDLE)(IMG_UINT64)psRGXCreateZSBufferIN_32->hPMR;
	sRGXCreateZSBufferIN.uiMapFlags= psRGXCreateZSBufferIN_32->uiMapFlags;

	ret = PVRSRVBridgeRGXCreateZSBuffer(ui32BridgeID, &sRGXCreateZSBufferIN,
	                     &sRGXCreateZSBufferOUT, psConnection);

	PVR_ASSERT(!((IMG_UINT64)sRGXCreateZSBufferOUT.hsZSBufferKM & 0xFFFFFFFF00000000ULL));
	psRGXCreateZSBufferOUT_32->hsZSBufferKM = (IMG_UINT32)(IMG_UINT64)sRGXCreateZSBufferOUT.hsZSBufferKM;
	psRGXCreateZSBufferOUT_32->ui32sZSBufferFWDevVAddr= sRGXCreateZSBufferOUT.ui32sZSBufferFWDevVAddr;
	psRGXCreateZSBufferOUT_32->eError= sRGXCreateZSBufferOUT.eError;

	return ret;
}



/*******************************************
            RGXDestroyZSBuffer
 *******************************************/

/* Bridge in structure for RGXDestroyZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER_TAG
{
	/*IMG_HANDLE hsZSBufferMemDesc;*/
	IMG_UINT32 hsZSBufferMemDesc;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER;

static IMG_INT
compat_PVRSRVBridgeRGXDestroyZSBuffer(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYZSBUFFER *psRGXDestroyZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYZSBUFFER sRGXDestroyZSBufferIN;

	sRGXDestroyZSBufferIN.hsZSBufferMemDesc = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyZSBufferIN_32->hsZSBufferMemDesc;

	return PVRSRVBridgeRGXDestroyZSBuffer(ui32BridgeID, &sRGXDestroyZSBufferIN,
					psRGXDestroyZSBufferOUT, psConnection);
}


/*******************************************
            RGXPopulateZSBuffer
 *******************************************/

/* Bridge in structure for RGXPopulateZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER_TAG
{
	/*IMG_HANDLE hsZSBufferKM;*/
	IMG_UINT32 hsZSBufferKM;
} compat_PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER;


/* Bridge out structure for RGXPopulateZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER_TAG
{
	/*IMG_HANDLE hsPopulation;*/
	IMG_UINT32 hsPopulation;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER;

static IMG_INT
compat_PVRSRVBridgeRGXPopulateZSBuffer(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER *psRGXPopulateZSBufferOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXPOPULATEZSBUFFER sRGXPopulateZSBufferIN;
	PVRSRV_BRIDGE_OUT_RGXPOPULATEZSBUFFER sRGXPopulateZSBufferOUT;


	sRGXPopulateZSBufferIN.hsZSBufferKM = (IMG_HANDLE)(IMG_UINT64)psRGXPopulateZSBufferIN_32->hsZSBufferKM;

	ret = PVRSRVBridgeRGXPopulateZSBuffer(ui32BridgeID, &sRGXPopulateZSBufferIN,
					&sRGXPopulateZSBufferOUT, psConnection);

	psRGXPopulateZSBufferOUT_32->eError = sRGXPopulateZSBufferOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sRGXPopulateZSBufferOUT.hsPopulation & 0xFFFFFFFF00000000ULL));
	psRGXPopulateZSBufferOUT_32->hsPopulation= (IMG_UINT32)(IMG_UINT64)sRGXPopulateZSBufferOUT.hsPopulation;

	return ret;
}



/*******************************************
            RGXUnpopulateZSBuffer
 *******************************************/

/* Bridge in structure for RGXUnpopulateZSBuffer */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER_TAG
{
	/*IMG_HANDLE hsPopulation;*/
	IMG_UINT32 hsPopulation;
} compat_PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER;

static IMG_INT
compat_PVRSRVBridgeRGXUnpopulateZSBuffer(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferIN_32,
					 PVRSRV_BRIDGE_OUT_RGXUNPOPULATEZSBUFFER *psRGXUnpopulateZSBufferOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXUNPOPULATEZSBUFFER sRGXUnpopulateZSBufferIN;

	sRGXUnpopulateZSBufferIN.hsPopulation = (IMG_HANDLE)(IMG_UINT64)psRGXUnpopulateZSBufferIN_32->hsPopulation;

	return PVRSRVBridgeRGXUnpopulateZSBuffer(ui32BridgeID, &sRGXUnpopulateZSBufferIN,
					psRGXUnpopulateZSBufferOUT, psConnection);
}


/*******************************************
            RGXCreateFreeList
 *******************************************/

/* Bridge in structure for RGXCreateFreeList */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATEFREELIST_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
	IMG_UINT32 ui32ui32MaxFLPages;
	IMG_UINT32 ui32ui32InitFLPages;
	IMG_UINT32 ui32ui32GrowFLPages;
	IMG_BOOL bbFreeListCheck;
	IMG_DEV_VIRTADDR spsFreeListDevVAddr __attribute__ ((__packed__));
	/*IMG_HANDLE hsFreeListPMR;*/
	IMG_UINT32 hsFreeListPMR;
	IMG_DEVMEM_OFFSET_T uiPMROffset __attribute__ ((__packed__));
} compat_PVRSRV_BRIDGE_IN_RGXCREATEFREELIST;


/* Bridge out structure for RGXCreateFreeList */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST_TAG
{
	/*IMG_HANDLE hCleanupCookie;*/
	IMG_UINT32 hCleanupCookie;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST;

static IMG_INT
compat_PVRSRVBridgeRGXCreateFreeList(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATEFREELIST *psRGXCreateFreeListIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST *psRGXCreateFreeListOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXCREATEFREELIST sRGXCreateFreeListIN;
	PVRSRV_BRIDGE_OUT_RGXCREATEFREELIST sRGXCreateFreeListOUT;

	sRGXCreateFreeListIN.bbFreeListCheck = psRGXCreateFreeListIN_32->bbFreeListCheck;
	sRGXCreateFreeListIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateFreeListIN_32->hDevNode;
	sRGXCreateFreeListIN.hsFreeListPMR = (IMG_HANDLE)(IMG_UINT64)psRGXCreateFreeListIN_32->hsFreeListPMR;
	sRGXCreateFreeListIN.ui32ui32GrowFLPages = psRGXCreateFreeListIN_32->ui32ui32GrowFLPages;
	sRGXCreateFreeListIN.ui32ui32InitFLPages = psRGXCreateFreeListIN_32->ui32ui32InitFLPages;
	sRGXCreateFreeListIN.ui32ui32MaxFLPages = psRGXCreateFreeListIN_32->ui32ui32MaxFLPages;
	sRGXCreateFreeListIN.spsFreeListDevVAddr.uiAddr = psRGXCreateFreeListIN_32->spsFreeListDevVAddr.uiAddr;
	sRGXCreateFreeListIN.uiPMROffset = psRGXCreateFreeListIN_32->uiPMROffset;

	ret = PVRSRVBridgeRGXCreateFreeList(ui32BridgeID, &sRGXCreateFreeListIN,
					&sRGXCreateFreeListOUT, psConnection);

	psRGXCreateFreeListOUT_32->eError = sRGXCreateFreeListOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sRGXCreateFreeListOUT.hCleanupCookie & 0xFFFFFFFF00000000ULL));
	psRGXCreateFreeListOUT_32->hCleanupCookie = (IMG_UINT32)(IMG_UINT64)sRGXCreateFreeListOUT.hCleanupCookie;

	return ret;
}


/*******************************************
            RGXDestroyFreeList
 *******************************************/

/* Bridge in structure for RGXDestroyFreeList */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST_TAG
{
	/*IMG_HANDLE hCleanupCookie;*/
	IMG_UINT32 hCleanupCookie;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST;


static IMG_INT
compat_PVRSRVBridgeRGXDestroyFreeList(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST *psRGXDestroyFreeListIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYFREELIST *psRGXDestroyFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYFREELIST sRGXDestroyFreeListIN;

	sRGXDestroyFreeListIN.hCleanupCookie = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyFreeListIN_32->hCleanupCookie;

	return PVRSRVBridgeRGXDestroyFreeList(ui32BridgeID, &sRGXDestroyFreeListIN,
					psRGXDestroyFreeListOUT, psConnection);
}



/*******************************************
            RGXAddBlockToFreeList
 *******************************************/

/* Bridge in structure for RGXAddBlockToFreeList */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST_TAG
{
	/*IMG_HANDLE hsFreeList;*/
	IMG_UINT32 hsFreeList;
	IMG_UINT32 ui3232NumPages;
} compat_PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST;


static IMG_INT
compat_PVRSRVBridgeRGXAddBlockToFreeList(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListIN_32,
					 PVRSRV_BRIDGE_OUT_RGXADDBLOCKTOFREELIST *psRGXAddBlockToFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXADDBLOCKTOFREELIST sRGXAddBlockToFreeListIN;

	sRGXAddBlockToFreeListIN.hsFreeList = (IMG_HANDLE)(IMG_UINT64)psRGXAddBlockToFreeListIN_32->hsFreeList;
	sRGXAddBlockToFreeListIN.ui3232NumPages = psRGXAddBlockToFreeListIN_32->ui3232NumPages;

	return PVRSRVBridgeRGXAddBlockToFreeList(ui32BridgeID, &sRGXAddBlockToFreeListIN,
					psRGXAddBlockToFreeListOUT, psConnection);
}


/*******************************************
            RGXRemoveBlockFromFreeList
 *******************************************/

/* Bridge in structure for RGXRemoveBlockFromFreeList */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST_TAG
{
	/*IMG_HANDLE hsFreeList;*/
	IMG_UINT32 hsFreeList;
} compat_PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST;


static IMG_INT
compat_PVRSRVBridgeRGXRemoveBlockFromFreeList(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListIN_32,
					 PVRSRV_BRIDGE_OUT_RGXREMOVEBLOCKFROMFREELIST *psRGXRemoveBlockFromFreeListOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXREMOVEBLOCKFROMFREELIST sRGXRemoveBlockFromFreeListIN;

	sRGXRemoveBlockFromFreeListIN.hsFreeList = (IMG_HANDLE)(IMG_UINT64)psRGXRemoveBlockFromFreeListIN_32->hsFreeList;

	return PVRSRVBridgeRGXRemoveBlockFromFreeList(ui32BridgeID, &sRGXRemoveBlockFromFreeListIN,
					psRGXRemoveBlockFromFreeListOUT, psConnection);
}


/*******************************************
            RGXCreateRenderContext
 *******************************************/

/* Bridge in structure for RGXCreateRenderContext */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
	IMG_UINT32 ui32Priority;
	IMG_DEV_VIRTADDR sMCUFenceAddr __attribute__ ((__packed__));
	IMG_DEV_VIRTADDR sVDMCallStackAddr __attribute__ ((__packed__));
	IMG_UINT32 ui32FrameworkCmdize;
	/*IMG_BYTE * psFrameworkCmd;*/
	IMG_UINT32 psFrameworkCmd;
	/*IMG_HANDLE hPrivData;*/
	IMG_UINT32 hPrivData;
} compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT;


/* Bridge out structure for RGXCreateRenderContext */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT_TAG
{
	/*IMG_HANDLE hRenderContext;*/
	IMG_UINT32 hRenderContext;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT;

static IMG_INT
compat_PVRSRVBridgeRGXCreateRenderContext(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT *psRGXCreateRenderContextOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXCREATERENDERCONTEXT sRGXCreateRenderContextIN;
	PVRSRV_BRIDGE_OUT_RGXCREATERENDERCONTEXT sRGXCreateRenderContextOUT;

	sRGXCreateRenderContextIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateRenderContextIN_32->hDevNode;
	sRGXCreateRenderContextIN.ui32Priority = psRGXCreateRenderContextIN_32->ui32Priority;
	sRGXCreateRenderContextIN.sMCUFenceAddr.uiAddr = psRGXCreateRenderContextIN_32->sMCUFenceAddr.uiAddr;
	sRGXCreateRenderContextIN.sVDMCallStackAddr.uiAddr = psRGXCreateRenderContextIN_32->sVDMCallStackAddr.uiAddr;
	sRGXCreateRenderContextIN.ui32FrameworkCmdize = psRGXCreateRenderContextIN_32->ui32FrameworkCmdize;
	sRGXCreateRenderContextIN.psFrameworkCmd = (IMG_BYTE *)(IMG_UINT64)psRGXCreateRenderContextIN_32->psFrameworkCmd;
	sRGXCreateRenderContextIN.hPrivData = (IMG_HANDLE)(IMG_UINT64)psRGXCreateRenderContextIN_32->hPrivData;

	ret = PVRSRVBridgeRGXCreateRenderContext(ui32BridgeID, &sRGXCreateRenderContextIN,
					&sRGXCreateRenderContextOUT, psConnection);

	psRGXCreateRenderContextOUT_32->eError = sRGXCreateRenderContextOUT.eError;
	PVR_ASSERT(!((IMG_UINT64)sRGXCreateRenderContextOUT.hRenderContext & 0xFFFFFFFF00000000ULL));
	psRGXCreateRenderContextOUT_32->hRenderContext = (IMG_UINT32)(IMG_UINT64)sRGXCreateRenderContextOUT.hRenderContext;

	return ret;
}


/*******************************************
            RGXDestroyRenderContext
 *******************************************/

/* Bridge in structure for RGXDestroyRenderContext */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT_TAG
{
	/*IMG_HANDLE hCleanupCookie;*/
	IMG_UINT32 hCleanupCookie;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT;


static IMG_INT
compat_PVRSRVBridgeRGXDestroyRenderContext(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYRENDERCONTEXT *psRGXDestroyRenderContextOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYRENDERCONTEXT sRGXDestroyRenderContextIN;

	sRGXDestroyRenderContextIN.hCleanupCookie = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyRenderContextIN_32->hCleanupCookie;

	return PVRSRVBridgeRGXDestroyRenderContext(ui32BridgeID, &sRGXDestroyRenderContextIN,
					psRGXDestroyRenderContextOUT, psConnection);
}


/*******************************************
            RGXKickTA3D
 *******************************************/

/* Bridge in structure for RGXKickTA3D */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXKICKTA3D_TAG
{
	/*IMG_HANDLE hRenderContext;*/
	IMG_UINT32 hRenderContext;
	IMG_UINT32 ui32ClientTAFenceCount;
	/*PRGXFWIF_UFO_ADDR * psClientTAFenceUFOAddress;*/
	IMG_UINT32 psClientTAFenceUFOAddress;
	/*IMG_UINT32 * pui32ClientTAFenceValue;*/
	IMG_UINT32 pui32ClientTAFenceValue;
	IMG_UINT32 ui32ClientTAUpdateCount;
	/*PRGXFWIF_UFO_ADDR * psClientTAUpdateUFOAddress;*/
	IMG_UINT32 psClientTAUpdateUFOAddress;
	/*IMG_UINT32 * pui32ClientTAUpdateValue;*/
	IMG_UINT32 pui32ClientTAUpdateValue;
	IMG_UINT32 ui32ServerTASyncPrims;
	/*IMG_UINT32 * pui32ServerTASyncFlags;*/
	IMG_UINT32 pui32ServerTASyncFlags;
	/*IMG_HANDLE * phServerTASyncs;*/
	IMG_UINT32 phServerTASyncs;
	IMG_UINT32 ui32Client3DFenceCount;
	/*PRGXFWIF_UFO_ADDR * psClient3DFenceUFOAddress;*/
	IMG_UINT32 psClient3DFenceUFOAddress;
	/*IMG_UINT32 * pui32Client3DFenceValue;*/
	IMG_UINT32 pui32Client3DFenceValue;
	IMG_UINT32 ui32Client3DUpdateCount;
	/*PRGXFWIF_UFO_ADDR * psClient3DUpdateUFOAddress;*/
	IMG_UINT32 psClient3DUpdateUFOAddress;
	/*IMG_UINT32 * pui32Client3DUpdateValue;*/
	IMG_UINT32 pui32Client3DUpdateValue;
	IMG_UINT32 ui32Server3DSyncPrims;
	/*IMG_UINT32 * pui32Server3DSyncFlags;*/
	IMG_UINT32 pui32Server3DSyncFlags;
	/*IMG_HANDLE * phServer3DSyncs;*/
	IMG_UINT32 phServer3DSyncs;
	PRGXFWIF_UFO_ADDR sPRFenceUFOAddress;
	IMG_UINT32 ui32FRFenceValue;
	IMG_UINT32 ui32NumFenceFds;
	/*IMG_INT32 * pi32FenceFds;*/
	IMG_UINT32 pi32FenceFds;
	IMG_UINT32 ui32TACmdSize;
	/*IMG_BYTE * psTACmd;*/
	IMG_UINT32 psTACmd;
	IMG_UINT32 ui323DPRCmdSize;
	/*IMG_BYTE * ps3DPRCmd;*/
	IMG_UINT32 ps3DPRCmd;
	IMG_UINT32 ui323DCmdSize;
	/*IMG_BYTE * ps3DCmd;*/
	IMG_UINT32 ps3DCmd;
	IMG_UINT32 ui32TAFrameNum;
	IMG_UINT32 ui32TARTData;
	IMG_BOOL bbLastTAInScene;
	IMG_BOOL bbKickTA;
	IMG_BOOL bbKickPR;
	IMG_BOOL bbKick3D;
	IMG_BOOL bbAbort;
	IMG_BOOL bbPDumpContinuous;
	/*IMG_HANDLE hRTDataCleanup;*/
	IMG_UINT32 hRTDataCleanup;
	/*IMG_HANDLE hZBuffer;*/
	IMG_UINT32 hZBuffer;
	/*IMG_HANDLE hSBuffer;*/
	IMG_UINT32 hSBuffer;
	IMG_BOOL bbCommitRefCountsTA;
	IMG_BOOL bbCommitRefCounts3D;
} compat_PVRSRV_BRIDGE_IN_RGXKICKTA3D;

static IMG_INT
compat_PVRSRVBridgeRGXKickTA3D(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXKICKTA3D *psRGXKickTA3DIN_32,
					 PVRSRV_BRIDGE_OUT_RGXKICKTA3D *psRGXKickTA3DOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret = 0;
	IMG_HANDLE __user *hServerTASyncsInt2 = IMG_NULL;
	IMG_UINT32 *hServerTASyncsInt3 = IMG_NULL;
	IMG_HANDLE __user *hServer3DSyncsInt2 = IMG_NULL;
	IMG_UINT32 *hServer3DSyncsInt3 = IMG_NULL;
	PVRSRV_BRIDGE_IN_RGXKICKTA3D sRGXKickTA3DIN;
	PVRSRV_BRIDGE_IN_RGXKICKTA3D *psRGXKickTA3DIN = &sRGXKickTA3DIN;
	IMG_UINT64 alloc_user = 0;

	psRGXKickTA3DIN->hRenderContext = (IMG_HANDLE)(IMG_UINT64)psRGXKickTA3DIN_32->hRenderContext;
	psRGXKickTA3DIN->ui32ClientTAFenceCount = psRGXKickTA3DIN_32->ui32ClientTAFenceCount;
	psRGXKickTA3DIN->psClientTAFenceUFOAddress = (PRGXFWIF_UFO_ADDR *)(IMG_UINT64)psRGXKickTA3DIN_32->psClientTAFenceUFOAddress;
	psRGXKickTA3DIN->pui32ClientTAFenceValue = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32ClientTAFenceValue;
	psRGXKickTA3DIN->ui32ClientTAUpdateCount = psRGXKickTA3DIN_32->ui32ClientTAUpdateCount;
	psRGXKickTA3DIN->psClientTAUpdateUFOAddress = (PRGXFWIF_UFO_ADDR *)(IMG_UINT64)psRGXKickTA3DIN_32->psClientTAUpdateUFOAddress;
	psRGXKickTA3DIN->pui32ClientTAUpdateValue = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32ClientTAUpdateValue;
	psRGXKickTA3DIN->ui32ServerTASyncPrims = psRGXKickTA3DIN_32->ui32ServerTASyncPrims;
	psRGXKickTA3DIN->pui32ServerTASyncFlags = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32ServerTASyncFlags;
	psRGXKickTA3DIN->phServerTASyncs = (IMG_HANDLE *)(IMG_UINT64)psRGXKickTA3DIN_32->phServerTASyncs;
	psRGXKickTA3DIN->ui32Client3DFenceCount = psRGXKickTA3DIN_32->ui32Client3DFenceCount;
	psRGXKickTA3DIN->psClient3DFenceUFOAddress = (PRGXFWIF_UFO_ADDR *)(IMG_UINT64)psRGXKickTA3DIN_32->psClient3DFenceUFOAddress;
	psRGXKickTA3DIN->pui32Client3DFenceValue = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32Client3DFenceValue;
	psRGXKickTA3DIN->ui32Client3DUpdateCount = psRGXKickTA3DIN_32->ui32Client3DUpdateCount;
	psRGXKickTA3DIN->psClient3DUpdateUFOAddress = (PRGXFWIF_UFO_ADDR *)(IMG_UINT64)psRGXKickTA3DIN_32->psClient3DUpdateUFOAddress;
	psRGXKickTA3DIN->pui32Client3DUpdateValue = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32Client3DUpdateValue;
	psRGXKickTA3DIN->ui32Server3DSyncPrims = psRGXKickTA3DIN_32->ui32Server3DSyncPrims;
	psRGXKickTA3DIN->pui32Server3DSyncFlags = (IMG_UINT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pui32Server3DSyncFlags;
	psRGXKickTA3DIN->phServer3DSyncs = (IMG_HANDLE *)(IMG_UINT64)psRGXKickTA3DIN_32->phServer3DSyncs;
	psRGXKickTA3DIN->sPRFenceUFOAddress.ui32Addr = psRGXKickTA3DIN_32->sPRFenceUFOAddress.ui32Addr;
	psRGXKickTA3DIN->ui32FRFenceValue = psRGXKickTA3DIN_32->ui32FRFenceValue;
	psRGXKickTA3DIN->ui32NumFenceFds = psRGXKickTA3DIN_32->ui32NumFenceFds;
	psRGXKickTA3DIN->pi32FenceFds = (IMG_INT32 *)(IMG_UINT64)psRGXKickTA3DIN_32->pi32FenceFds;
	psRGXKickTA3DIN->ui32TACmdSize = psRGXKickTA3DIN_32->ui32TACmdSize;
	psRGXKickTA3DIN->psTACmd = (IMG_BYTE *)(IMG_UINT64)psRGXKickTA3DIN_32->psTACmd;
	psRGXKickTA3DIN->ui323DPRCmdSize = psRGXKickTA3DIN_32->ui323DPRCmdSize;
	psRGXKickTA3DIN->ps3DPRCmd = (IMG_BYTE *)(IMG_UINT64)psRGXKickTA3DIN_32->ps3DPRCmd;
	psRGXKickTA3DIN->ui323DCmdSize = psRGXKickTA3DIN_32->ui323DCmdSize;
	psRGXKickTA3DIN->ps3DCmd = (IMG_BYTE *)(IMG_UINT64)psRGXKickTA3DIN_32->ps3DCmd;
	psRGXKickTA3DIN->ui32TAFrameNum = psRGXKickTA3DIN_32->ui32TAFrameNum;
	psRGXKickTA3DIN->ui32TARTData = psRGXKickTA3DIN_32->ui32TARTData;
	psRGXKickTA3DIN->bbLastTAInScene = psRGXKickTA3DIN_32->bbLastTAInScene;
	psRGXKickTA3DIN->bbKickTA = psRGXKickTA3DIN_32->bbKickTA;
	psRGXKickTA3DIN->bbKickPR = psRGXKickTA3DIN_32->bbKickPR;
	psRGXKickTA3DIN->bbKick3D = psRGXKickTA3DIN_32->bbKick3D;
	psRGXKickTA3DIN->bbAbort = psRGXKickTA3DIN_32->bbAbort;
	psRGXKickTA3DIN->bbPDumpContinuous = psRGXKickTA3DIN_32->bbPDumpContinuous;
	psRGXKickTA3DIN->hRTDataCleanup = (IMG_HANDLE)(IMG_UINT64)psRGXKickTA3DIN_32->hRTDataCleanup;
	psRGXKickTA3DIN->hZBuffer = (IMG_HANDLE)(IMG_UINT64)psRGXKickTA3DIN_32->hZBuffer;
	psRGXKickTA3DIN->hSBuffer = (IMG_HANDLE)(IMG_UINT64)psRGXKickTA3DIN_32->hSBuffer;
	psRGXKickTA3DIN->bbCommitRefCountsTA = psRGXKickTA3DIN_32->bbCommitRefCountsTA;
	psRGXKickTA3DIN->bbCommitRefCounts3D = psRGXKickTA3DIN_32->bbCommitRefCounts3D;

	if (psRGXKickTA3DIN->ui32ServerTASyncPrims != 0)
	{
        alloc_user = psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_HANDLE);
		hServerTASyncsInt3 = OSAllocMem(psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32));
		if (!hServerTASyncsInt3)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

	/* Copy the data over */
	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->phServerTASyncs, psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hServerTASyncsInt3, psRGXKickTA3DIN->phServerTASyncs,
		psRGXKickTA3DIN->ui32ServerTASyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto RGXKickTA3D_exit;
	}

	if (psRGXKickTA3DIN->ui32Server3DSyncPrims != 0)
	{
        alloc_user += psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_HANDLE);
		hServer3DSyncsInt3 = OSAllocMem(psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32));
		if (!hServer3DSyncsInt3)
		{
			psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickTA3D_exit;
		}
	}

	hServerTASyncsInt2 = compat_alloc_user_space(alloc_user);
	if (!hServerTASyncsInt2)
	{
	    psRGXKickTA3DOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

	    goto RGXKickTA3D_exit;
	}
	hServer3DSyncsInt2 = hServerTASyncsInt2 + psRGXKickTA3DIN->ui32ServerTASyncPrims;

	/* Copy the data over */
	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickTA3DIN->phServer3DSyncs, psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hServer3DSyncsInt3, psRGXKickTA3DIN->phServer3DSyncs,
		psRGXKickTA3DIN->ui32Server3DSyncPrims * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto RGXKickTA3D_exit;
	}


	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32ServerTASyncPrims;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hServerTASyncsInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hServerTASyncsInt3[i], &hServerTASyncsInt2[i]))
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto RGXKickTA3D_exit;
			}
		}
        psRGXKickTA3DIN->phServerTASyncs = hServerTASyncsInt2;
	}

	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickTA3DIN->ui32Server3DSyncPrims;i++)
		{
			if (!OSAccessOK(PVR_VERIFY_WRITE, &hServer3DSyncsInt2[i], sizeof(IMG_HANDLE))
				|| __put_user((unsigned long)hServer3DSyncsInt3[i], &hServer3DSyncsInt2[i]))
			{
				psRGXKickTA3DOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto RGXKickTA3D_exit;
			}
		}
        psRGXKickTA3DIN->phServer3DSyncs = hServer3DSyncsInt2;
	}

	ret = PVRSRVBridgeRGXKickTA3D(ui32BridgeID, psRGXKickTA3DIN,
					psRGXKickTA3DOUT, psConnection);

RGXKickTA3D_exit:
	if (hServerTASyncsInt3)
		OSFreeMem(hServerTASyncsInt3);
	if (hServer3DSyncsInt3)
		OSFreeMem(hServer3DSyncsInt3);

	return ret;
}

/*******************************************
            RGXSetRenderContextPriority
 *******************************************/

/* Bridge in structure for RGXSetRenderContextPriority */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY_TAG
{
	/*IMG_HANDLE hRenderContext;*/
	IMG_UINT32 hRenderContext;
	IMG_UINT32 ui32Priority;
} compat_PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY;

static IMG_INT
compat_PVRSRVBridgeRGXSetRenderContextPriority(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityIN_32,
					 PVRSRV_BRIDGE_OUT_RGXSETRENDERCONTEXTPRIORITY *psRGXSetRenderContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXSETRENDERCONTEXTPRIORITY sRGXSetRenderContextPriorityIN;

	sRGXSetRenderContextPriorityIN.hRenderContext = (IMG_HANDLE)(IMG_UINT64)psRGXSetRenderContextPriorityIN_32->hRenderContext;
	sRGXSetRenderContextPriorityIN.ui32Priority = psRGXSetRenderContextPriorityIN_32->ui32Priority;

	return PVRSRVBridgeRGXSetRenderContextPriority(ui32BridgeID, &sRGXSetRenderContextPriorityIN,
					psRGXSetRenderContextPriorityOUT, psConnection);
}

/* Bridge in structure for RGXGetLastRenderContextResetReason */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON_TAG
{
	/*IMG_HANDLE hRenderContext;*/
	IMG_UINT32 hRenderContext;
} compat_PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON;

static IMG_INT
compat_PVRSRVBridgeRGXGetLastRenderContextResetReason(IMG_UINT32 ui32BridgeID,
                                         compat_PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonIN,
                                         PVRSRV_BRIDGE_OUT_RGXGETLASTRENDERCONTEXTRESETREASON *psRGXGetLastRenderContextResetReasonOUT,
                                         CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXGETLASTRENDERCONTEXTRESETREASON sRGXGetLastRenderContextResetReasonIN64;

	sRGXGetLastRenderContextResetReasonIN64.hRenderContext = (IMG_HANDLE)(IMG_UINT64)psRGXGetLastRenderContextResetReasonIN->hRenderContext;

	return PVRSRVBridgeRGXGetLastRenderContextResetReason(ui32BridgeID, &sRGXGetLastRenderContextResetReasonIN64,
					psRGXGetLastRenderContextResetReasonOUT, psConnection);
}

#endif

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterRGXTA3DFunctions(IMG_VOID);
IMG_VOID UnregisterRGXTA3DFunctions(IMG_VOID);

/*
 * Register all RGXTA3D functions with services
 */
PVRSRV_ERROR RegisterRGXTA3DFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATA, compat_PVRSRVBridgeRGXCreateHWRTData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATA, compat_PVRSRVBridgeRGXDestroyHWRTData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERTARGET, compat_PVRSRVBridgeRGXCreateRenderTarget);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERTARGET, compat_PVRSRVBridgeRGXDestroyRenderTarget);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER, compat_PVRSRVBridgeRGXCreateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER, compat_PVRSRVBridgeRGXDestroyZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER, compat_PVRSRVBridgeRGXPopulateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER, compat_PVRSRVBridgeRGXUnpopulateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST, compat_PVRSRVBridgeRGXCreateFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST, compat_PVRSRVBridgeRGXDestroyFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXADDBLOCKTOFREELIST, compat_PVRSRVBridgeRGXAddBlockToFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXREMOVEBLOCKFROMFREELIST, compat_PVRSRVBridgeRGXRemoveBlockFromFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT, compat_PVRSRVBridgeRGXCreateRenderContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT, compat_PVRSRVBridgeRGXDestroyRenderContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D, compat_PVRSRVBridgeRGXKickTA3D);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY, compat_PVRSRVBridgeRGXSetRenderContextPriority);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON, compat_PVRSRVBridgeRGXGetLastRenderContextResetReason);
#else
	/* WPAT FINDME */
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEHWRTDATA, PVRSRVBridgeRGXCreateHWRTData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYHWRTDATA, PVRSRVBridgeRGXDestroyHWRTData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERTARGET, PVRSRVBridgeRGXCreateRenderTarget);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERTARGET, PVRSRVBridgeRGXDestroyRenderTarget);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEZSBUFFER, PVRSRVBridgeRGXCreateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYZSBUFFER, PVRSRVBridgeRGXDestroyZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXPOPULATEZSBUFFER, PVRSRVBridgeRGXPopulateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXUNPOPULATEZSBUFFER, PVRSRVBridgeRGXUnpopulateZSBuffer);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATEFREELIST, PVRSRVBridgeRGXCreateFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYFREELIST, PVRSRVBridgeRGXDestroyFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXADDBLOCKTOFREELIST, PVRSRVBridgeRGXAddBlockToFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXREMOVEBLOCKFROMFREELIST, PVRSRVBridgeRGXRemoveBlockFromFreeList);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXCREATERENDERCONTEXT, PVRSRVBridgeRGXCreateRenderContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXDESTROYRENDERCONTEXT, PVRSRVBridgeRGXDestroyRenderContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXKICKTA3D, PVRSRVBridgeRGXKickTA3D);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXSETRENDERCONTEXTPRIORITY, PVRSRVBridgeRGXSetRenderContextPriority);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXTA3D_RGXGETLASTRENDERCONTEXTRESETREASON, PVRSRVBridgeRGXGetLastRenderContextResetReason);
#endif

	return PVRSRV_OK;
}

/*
 * Unregister all rgxta3d functions with services
 */
IMG_VOID UnregisterRGXTA3DFunctions(IMG_VOID)
{
}
