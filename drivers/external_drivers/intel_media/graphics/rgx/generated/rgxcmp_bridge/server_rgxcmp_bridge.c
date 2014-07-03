/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxcmp
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxcmp
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

#include "rgxcompute.h"


#include "common_rgxcmp_bridge.h"

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
RGXDestroyComputeContextResManProxy(IMG_HANDLE hResmanItem)
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
PVRSRVBridgeRGXCreateComputeContext(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextIN,
					 PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_BYTE *psFrameworkCmdInt = IMG_NULL;
	IMG_HANDLE hPrivDataInt = IMG_NULL;
	RGX_SERVER_COMPUTE_CONTEXT * psComputeContextInt = IMG_NULL;
	IMG_HANDLE hComputeContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT);




	if (psRGXCreateComputeContextIN->ui32FrameworkCmdize != 0)
	{
		psFrameworkCmdInt = OSAllocMem(psRGXCreateComputeContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE));
		if (!psFrameworkCmdInt)
		{
			psRGXCreateComputeContextOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXCreateComputeContext_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXCreateComputeContextIN->psFrameworkCmd, psRGXCreateComputeContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psFrameworkCmdInt, psRGXCreateComputeContextIN->psFrameworkCmd,
				psRGXCreateComputeContextIN->ui32FrameworkCmdize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXCreateComputeContextOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCreateComputeContext_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXCreateComputeContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCreateComputeContextIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateComputeContext_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXCreateComputeContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hPrivDataInt,
											psRGXCreateComputeContextIN->hPrivData,
											PVRSRV_HANDLE_TYPE_DEV_PRIV_DATA);
					if(psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
					{
						goto RGXCreateComputeContext_exit;
					}

				}

	psRGXCreateComputeContextOUT->eError =
		PVRSRVRGXCreateComputeContextKM(psConnection,
					hDevNodeInt,
					psRGXCreateComputeContextIN->ui32Priority,
					psRGXCreateComputeContextIN->sMCUFenceAddr,
					psRGXCreateComputeContextIN->ui32FrameworkCmdize,
					psFrameworkCmdInt,
					hPrivDataInt,
					&psComputeContextInt);
	/* Exit early if bridged call fails */
	if(psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateComputeContext_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hComputeContextInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
												psComputeContextInt,
												(RESMAN_FREE_FN)&PVRSRVRGXDestroyComputeContextKM);
	if (hComputeContextInt2 == IMG_NULL)
	{
		psRGXCreateComputeContextOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto RGXCreateComputeContext_exit;
	}
	psRGXCreateComputeContextOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXCreateComputeContextOUT->hComputeContext,
							(IMG_HANDLE) hComputeContextInt2,
							PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
	{
		goto RGXCreateComputeContext_exit;
	}


RGXCreateComputeContext_exit:
	if (psRGXCreateComputeContextOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hComputeContextInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hComputeContextInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (psComputeContextInt)
		{
			PVRSRVRGXDestroyComputeContextKM(psComputeContextInt);
		}
	}

	if (psFrameworkCmdInt)
		OSFreeMem(psFrameworkCmdInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXDestroyComputeContext(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextIN,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hComputeContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT);





				{
					/* Look up the address from the handle */
					psRGXDestroyComputeContextOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hComputeContextInt2,
											psRGXDestroyComputeContextIN->hComputeContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
					if(psRGXDestroyComputeContextOUT->eError != PVRSRV_OK)
					{
						goto RGXDestroyComputeContext_exit;
					}

				}

	psRGXDestroyComputeContextOUT->eError = RGXDestroyComputeContextResManProxy(hComputeContextInt2);
	/* Exit early if bridged call fails */
	if(psRGXDestroyComputeContextOUT->eError != PVRSRV_OK)
	{
		goto RGXDestroyComputeContext_exit;
	}

	psRGXDestroyComputeContextOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXDestroyComputeContextIN->hComputeContext,
					PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);


RGXDestroyComputeContext_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXKickCDM(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXKICKCDM *psRGXKickCDMIN,
					 PVRSRV_BRIDGE_OUT_RGXKICKCDM *psRGXKickCDMOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_COMPUTE_CONTEXT * psComputeContextInt = IMG_NULL;
	IMG_HANDLE hComputeContextInt2 = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClientFenceUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32ClientFenceValueInt = IMG_NULL;
	PRGXFWIF_UFO_ADDR *sClientUpdateUFOAddressInt = IMG_NULL;
	IMG_UINT32 *ui32ClientUpdateValueInt = IMG_NULL;
	IMG_UINT32 *ui32ServerSyncFlagsInt = IMG_NULL;
	SERVER_SYNC_PRIMITIVE * *psServerSyncsInt = IMG_NULL;
	IMG_HANDLE *hServerSyncsInt2 = IMG_NULL;
	IMG_BYTE *psDMCmdInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM);




	if (psRGXKickCDMIN->ui32ClientFenceCount != 0)
	{
		sClientFenceUFOAddressInt = OSAllocMem(psRGXKickCDMIN->ui32ClientFenceCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClientFenceUFOAddressInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->psClientFenceUFOAddress, psRGXKickCDMIN->ui32ClientFenceCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClientFenceUFOAddressInt, psRGXKickCDMIN->psClientFenceUFOAddress,
				psRGXKickCDMIN->ui32ClientFenceCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32ClientFenceCount != 0)
	{
		ui32ClientFenceValueInt = OSAllocMem(psRGXKickCDMIN->ui32ClientFenceCount * sizeof(IMG_UINT32));
		if (!ui32ClientFenceValueInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->pui32ClientFenceValue, psRGXKickCDMIN->ui32ClientFenceCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientFenceValueInt, psRGXKickCDMIN->pui32ClientFenceValue,
				psRGXKickCDMIN->ui32ClientFenceCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32ClientUpdateCount != 0)
	{
		sClientUpdateUFOAddressInt = OSAllocMem(psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(PRGXFWIF_UFO_ADDR));
		if (!sClientUpdateUFOAddressInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->psClientUpdateUFOAddress, psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(PRGXFWIF_UFO_ADDR))
				|| (OSCopyFromUser(NULL, sClientUpdateUFOAddressInt, psRGXKickCDMIN->psClientUpdateUFOAddress,
				psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(PRGXFWIF_UFO_ADDR)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32ClientUpdateCount != 0)
	{
		ui32ClientUpdateValueInt = OSAllocMem(psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32));
		if (!ui32ClientUpdateValueInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->pui32ClientUpdateValue, psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ClientUpdateValueInt, psRGXKickCDMIN->pui32ClientUpdateValue,
				psRGXKickCDMIN->ui32ClientUpdateCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32ServerSyncCount != 0)
	{
		ui32ServerSyncFlagsInt = OSAllocMem(psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32));
		if (!ui32ServerSyncFlagsInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->pui32ServerSyncFlags, psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32ServerSyncFlagsInt, psRGXKickCDMIN->pui32ServerSyncFlags,
				psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32ServerSyncCount != 0)
	{
		psServerSyncsInt = OSAllocMem(psRGXKickCDMIN->ui32ServerSyncCount * sizeof(SERVER_SYNC_PRIMITIVE *));
		if (!psServerSyncsInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
		hServerSyncsInt2 = OSAllocMem(psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_HANDLE));
		if (!hServerSyncsInt2)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->phServerSyncs, psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_HANDLE))
				|| (OSCopyFromUser(NULL, hServerSyncsInt2, psRGXKickCDMIN->phServerSyncs,
				psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_HANDLE)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}
	if (psRGXKickCDMIN->ui32CmdSize != 0)
	{
		psDMCmdInt = OSAllocMem(psRGXKickCDMIN->ui32CmdSize * sizeof(IMG_BYTE));
		if (!psDMCmdInt)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->psDMCmd, psRGXKickCDMIN->ui32CmdSize * sizeof(IMG_BYTE))
				|| (OSCopyFromUser(NULL, psDMCmdInt, psRGXKickCDMIN->psDMCmd,
				psRGXKickCDMIN->ui32CmdSize * sizeof(IMG_BYTE)) != PVRSRV_OK) )
			{
				psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXKickCDM_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXKickCDMOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hComputeContextInt2,
											psRGXKickCDMIN->hComputeContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
					if(psRGXKickCDMOUT->eError != PVRSRV_OK)
					{
						goto RGXKickCDM_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickCDMOUT->eError = ResManFindPrivateDataByPtr(hComputeContextInt2, (IMG_VOID **) &psComputeContextInt);

					if(psRGXKickCDMOUT->eError != PVRSRV_OK)
					{
						goto RGXKickCDM_exit;
					}
				}

	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickCDMIN->ui32ServerSyncCount;i++)
		{
				{
					/* Look up the address from the handle */
					psRGXKickCDMOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hServerSyncsInt2[i],
											psRGXKickCDMIN->phServerSyncs[i],
											PVRSRV_HANDLE_TYPE_SERVER_SYNC_PRIMITIVE);
					if(psRGXKickCDMOUT->eError != PVRSRV_OK)
					{
						goto RGXKickCDM_exit;
					}

					/* Look up the data from the resman address */
					psRGXKickCDMOUT->eError = ResManFindPrivateDataByPtr(hServerSyncsInt2[i], (IMG_VOID **) &psServerSyncsInt[i]);

					if(psRGXKickCDMOUT->eError != PVRSRV_OK)
					{
						goto RGXKickCDM_exit;
					}
				}
		}
	}

	psRGXKickCDMOUT->eError =
		PVRSRVRGXKickCDMKM(
					psComputeContextInt,
					psRGXKickCDMIN->ui32ClientFenceCount,
					sClientFenceUFOAddressInt,
					ui32ClientFenceValueInt,
					psRGXKickCDMIN->ui32ClientUpdateCount,
					sClientUpdateUFOAddressInt,
					ui32ClientUpdateValueInt,
					psRGXKickCDMIN->ui32ServerSyncCount,
					ui32ServerSyncFlagsInt,
					psServerSyncsInt,
					psRGXKickCDMIN->ui32CmdSize,
					psDMCmdInt,
					psRGXKickCDMIN->bbPDumpContinuous);



RGXKickCDM_exit:
	if (sClientFenceUFOAddressInt)
		OSFreeMem(sClientFenceUFOAddressInt);
	if (ui32ClientFenceValueInt)
		OSFreeMem(ui32ClientFenceValueInt);
	if (sClientUpdateUFOAddressInt)
		OSFreeMem(sClientUpdateUFOAddressInt);
	if (ui32ClientUpdateValueInt)
		OSFreeMem(ui32ClientUpdateValueInt);
	if (ui32ServerSyncFlagsInt)
		OSFreeMem(ui32ServerSyncFlagsInt);
	if (psServerSyncsInt)
		OSFreeMem(psServerSyncsInt);
	if (hServerSyncsInt2)
		OSFreeMem(hServerSyncsInt2);
	if (psDMCmdInt)
		OSFreeMem(psDMCmdInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXFlushComputeData(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataIN,
					 PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_COMPUTE_CONTEXT * psComputeContextInt = IMG_NULL;
	IMG_HANDLE hComputeContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA);





				{
					/* Look up the address from the handle */
					psRGXFlushComputeDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hComputeContextInt2,
											psRGXFlushComputeDataIN->hComputeContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
					if(psRGXFlushComputeDataOUT->eError != PVRSRV_OK)
					{
						goto RGXFlushComputeData_exit;
					}

					/* Look up the data from the resman address */
					psRGXFlushComputeDataOUT->eError = ResManFindPrivateDataByPtr(hComputeContextInt2, (IMG_VOID **) &psComputeContextInt);

					if(psRGXFlushComputeDataOUT->eError != PVRSRV_OK)
					{
						goto RGXFlushComputeData_exit;
					}
				}

	psRGXFlushComputeDataOUT->eError =
		PVRSRVRGXFlushComputeDataKM(
					psComputeContextInt);



RGXFlushComputeData_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXSetComputeContextPriority(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY *psRGXSetComputeContextPriorityIN,
					 PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY *psRGXSetComputeContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	RGX_SERVER_COMPUTE_CONTEXT * psComputeContextInt = IMG_NULL;
	IMG_HANDLE hComputeContextInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY);





				{
					/* Look up the address from the handle */
					psRGXSetComputeContextPriorityOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hComputeContextInt2,
											psRGXSetComputeContextPriorityIN->hComputeContext,
											PVRSRV_HANDLE_TYPE_RGX_SERVER_COMPUTE_CONTEXT);
					if(psRGXSetComputeContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetComputeContextPriority_exit;
					}

					/* Look up the data from the resman address */
					psRGXSetComputeContextPriorityOUT->eError = ResManFindPrivateDataByPtr(hComputeContextInt2, (IMG_VOID **) &psComputeContextInt);

					if(psRGXSetComputeContextPriorityOUT->eError != PVRSRV_OK)
					{
						goto RGXSetComputeContextPriority_exit;
					}
				}

	psRGXSetComputeContextPriorityOUT->eError =
		PVRSRVRGXSetComputeContextPriorityKM(psConnection,
					psComputeContextInt,
					psRGXSetComputeContextPriorityIN->ui32Priority);



RGXSetComputeContextPriority_exit:

	return 0;
}

#ifdef CONFIG_COMPAT

#include <linux/compat.h>

/* Bridge in structure for RGXCreateComputeContext */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT_TAG
{
	/* IMG_HANDLE hDevNode; */
	IMG_UINT32 hDevNode;
	IMG_UINT32 ui32Priority;
	IMG_DEV_VIRTADDR sMCUFenceAddr __attribute__ ((__packed__));
	IMG_UINT32 ui32FrameworkCmdize;
	/* IMG_BYTE * psFrameworkCmd; */
	IMG_UINT32 psFrameworkCmd;
	/* IMG_HANDLE hPrivData; */
	IMG_UINT32 hPrivData;
} compat_PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT;


/* Bridge out structure for RGXCreateComputeContext */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT_TAG
{
	/* IMG_HANDLE hComputeContext; */
	IMG_UINT32 hComputeContext;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT;

static IMG_INT
compat_PVRSRVBridgeRGXCreateComputeContext(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextIN_32,
					 compat_PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT sRGXCreateComputeContextIN;
	PVRSRV_BRIDGE_IN_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextIN = &sRGXCreateComputeContextIN;
	PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT sRGXCreateComputeContextOUT;
	PVRSRV_BRIDGE_OUT_RGXCREATECOMPUTECONTEXT *psRGXCreateComputeContextOUT = &sRGXCreateComputeContextOUT;

	psRGXCreateComputeContextIN->hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXCreateComputeContextIN_32->hDevNode;
	psRGXCreateComputeContextIN->ui32Priority = psRGXCreateComputeContextIN_32->ui32Priority;
	psRGXCreateComputeContextIN->sMCUFenceAddr = psRGXCreateComputeContextIN_32->sMCUFenceAddr;
	psRGXCreateComputeContextIN->ui32FrameworkCmdize = psRGXCreateComputeContextIN_32->ui32FrameworkCmdize;
	psRGXCreateComputeContextIN->psFrameworkCmd = (IMG_BYTE*)(IMG_UINT64)psRGXCreateComputeContextIN_32->psFrameworkCmd;
	psRGXCreateComputeContextIN->hPrivData = (IMG_HANDLE)(IMG_UINT64)psRGXCreateComputeContextIN_32->hPrivData;

	ret = PVRSRVBridgeRGXCreateComputeContext(ui32BridgeID,
					 psRGXCreateComputeContextIN,
					 psRGXCreateComputeContextOUT,
					 psConnection);

	PVR_ASSERT(!((IMG_UINT64)psRGXCreateComputeContextOUT->hComputeContext & 0xFFFFFFFF00000000ULL));
	psRGXCreateComputeContextOUT_32->hComputeContext = (IMG_UINT32)(IMG_UINT64)psRGXCreateComputeContextOUT->hComputeContext;
	psRGXCreateComputeContextOUT_32->eError = psRGXCreateComputeContextOUT->eError;

	return ret;
}

/* Bridge in structure for RGXDestroyComputeContext */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT_TAG
{
	/* IMG_HANDLE hComputeContext; */
	IMG_UINT32 hComputeContext;
} compat_PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT;

static IMG_INT
compat_PVRSRVBridgeRGXDestroyComputeContext(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextIN_32,
					 PVRSRV_BRIDGE_OUT_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT sRGXDestroyComputeContextIN;
	PVRSRV_BRIDGE_IN_RGXDESTROYCOMPUTECONTEXT *psRGXDestroyComputeContextIN = &sRGXDestroyComputeContextIN;

	psRGXDestroyComputeContextIN->hComputeContext = (IMG_HANDLE)(IMG_UINT64)psRGXDestroyComputeContextIN_32->hComputeContext;

	return PVRSRVBridgeRGXDestroyComputeContext(ui32BridgeID,
					psRGXDestroyComputeContextIN,
					psRGXDestroyComputeContextOUT,
					psConnection);

}

/* Bridge in structure for RGXKickCDM */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXKICKCDM_TAG
{
	/* IMG_HANDLE hComputeContext; */
	IMG_UINT32 hComputeContext;
	IMG_UINT32 ui32ClientFenceCount;
	/* PRGXFWIF_UFO_ADDR * psClientFenceUFOAddress; */
	IMG_UINT32 psClientFenceUFOAddress;
	/* IMG_UINT32 * pui32ClientFenceValue; */
	IMG_UINT32 pui32ClientFenceValue;
	IMG_UINT32 ui32ClientUpdateCount;
	/* PRGXFWIF_UFO_ADDR * psClientUpdateUFOAddress; */
	IMG_UINT32 psClientUpdateUFOAddress;
	/* IMG_UINT32 * pui32ClientUpdateValue; */
	IMG_UINT32 pui32ClientUpdateValue;
	IMG_UINT32 ui32ServerSyncCount;
	/* IMG_UINT32 * pui32ServerSyncFlags; */
	IMG_UINT32 pui32ServerSyncFlags;
	/* IMG_HANDLE * phServerSyncs; */
	IMG_UINT32 phServerSyncs;
	IMG_UINT32 ui32CmdSize;
	/* IMG_BYTE * psDMCmd; */
	IMG_UINT32 psDMCmd;
	IMG_BOOL bbPDumpContinuous;
} compat_PVRSRV_BRIDGE_IN_RGXKICKCDM;

static IMG_INT
compat_PVRSRVBridgeRGXKickCDM(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXKICKCDM *psRGXKickCDMIN_32,
					 PVRSRV_BRIDGE_OUT_RGXKICKCDM *psRGXKickCDMOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE *hServerSyncsInt2 = IMG_NULL;
	IMG_UINT32 *hServerSyncsInt3 = IMG_NULL;

	PVRSRV_BRIDGE_IN_RGXKICKCDM sRGXKickCDMIN;
	PVRSRV_BRIDGE_IN_RGXKICKCDM *psRGXKickCDMIN = &sRGXKickCDMIN;

	psRGXKickCDMIN->hComputeContext = (IMG_HANDLE)(IMG_UINT64)psRGXKickCDMIN_32->hComputeContext;
	psRGXKickCDMIN->ui32ClientFenceCount = psRGXKickCDMIN_32->ui32ClientFenceCount;
	psRGXKickCDMIN->psClientFenceUFOAddress = (PRGXFWIF_UFO_ADDR*)(IMG_UINT64)psRGXKickCDMIN_32->psClientFenceUFOAddress;
	psRGXKickCDMIN->pui32ClientFenceValue = (IMG_UINT32*)(IMG_UINT64)psRGXKickCDMIN_32->pui32ClientFenceValue;
	psRGXKickCDMIN->ui32ClientUpdateCount = psRGXKickCDMIN_32->ui32ClientUpdateCount;
	psRGXKickCDMIN->psClientUpdateUFOAddress = (PRGXFWIF_UFO_ADDR*)(IMG_UINT64)psRGXKickCDMIN_32->psClientUpdateUFOAddress;
	psRGXKickCDMIN->pui32ClientUpdateValue = (IMG_UINT32*)(IMG_UINT64)psRGXKickCDMIN_32->pui32ClientUpdateValue;
	psRGXKickCDMIN->ui32ServerSyncCount = psRGXKickCDMIN_32->ui32ServerSyncCount;
	psRGXKickCDMIN->pui32ServerSyncFlags = (IMG_UINT32*)(IMG_UINT64)psRGXKickCDMIN_32->pui32ServerSyncFlags;
	psRGXKickCDMIN->phServerSyncs = (IMG_HANDLE*)(IMG_UINT64)psRGXKickCDMIN_32->phServerSyncs;
	psRGXKickCDMIN->ui32CmdSize = psRGXKickCDMIN_32->ui32CmdSize;
	psRGXKickCDMIN->psDMCmd = (IMG_BYTE*)(IMG_UINT64)psRGXKickCDMIN_32->psDMCmd;
	psRGXKickCDMIN->bbPDumpContinuous = psRGXKickCDMIN_32->bbPDumpContinuous;

	if (psRGXKickCDMIN->ui32ServerSyncCount != 0)
	{
		hServerSyncsInt2 = compat_alloc_user_space(psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_HANDLE));
		if (!hServerSyncsInt2)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
		hServerSyncsInt3 = OSAllocMem(psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32));
		if (!hServerSyncsInt3)
		{
			psRGXKickCDMOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXKickCDM_exit;
		}
	}

	if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXKickCDMIN->phServerSyncs, psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32))
		|| (OSCopyFromUser(NULL, hServerSyncsInt3, psRGXKickCDMIN->phServerSyncs,
		psRGXKickCDMIN->ui32ServerSyncCount * sizeof(IMG_UINT32)) != PVRSRV_OK) )
	{
		psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

		goto RGXKickCDM_exit;
	}


	{
		IMG_UINT32 i;

		for (i=0;i<psRGXKickCDMIN->ui32ServerSyncCount;i++)
		{
            if (!access_ok(VERIFY_WRITE, &hServerSyncsInt2[i], sizeof(IMG_HANDLE))
                || __put_user((unsigned long)hServerSyncsInt3[i], &hServerSyncsInt2[i])) {
                psRGXKickCDMOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
                goto RGXKickCDM_exit;
            }
		}
        psRGXKickCDMIN->phServerSyncs = hServerSyncsInt2;
	}

    PVRSRVBridgeRGXKickCDM(ui32BridgeID,
					 psRGXKickCDMIN,
					 psRGXKickCDMOUT,
					 psConnection);


RGXKickCDM_exit:
	if (hServerSyncsInt3)
		OSFreeMem(hServerSyncsInt3);

	return 0;
}

/* Bridge in structure for RGXFlushComputeData */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA_TAG
{
	/* IMG_HANDLE hComputeContext; */
	IMG_UINT32 hComputeContext;
} compat_PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA;

static IMG_INT
compat_PVRSRVBridgeRGXFlushComputeData(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataIN_32,
					 PVRSRV_BRIDGE_OUT_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA sRGXFlushComputeDataIN;
	PVRSRV_BRIDGE_IN_RGXFLUSHCOMPUTEDATA *psRGXFlushComputeDataIN = &sRGXFlushComputeDataIN;

	psRGXFlushComputeDataIN->hComputeContext =(IMG_HANDLE)(IMG_UINT64) psRGXFlushComputeDataIN_32->hComputeContext;

	return PVRSRVBridgeRGXFlushComputeData(ui32BridgeID,
					psRGXFlushComputeDataIN,
					psRGXFlushComputeDataOUT,
					psConnection);

}

/* Bridge in structure for RGXSetComputeContextPriority */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY_TAG
{
	/* IMG_HANDLE hComputeContext; */
	IMG_UINT32 hComputeContext;
	IMG_UINT32 ui32Priority;
} compat_PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY;

static IMG_INT
compat_PVRSRVBridgeRGXSetComputeContextPriority(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY *psRGXSetComputeContextPriorityIN_32,
					 PVRSRV_BRIDGE_OUT_RGXSETCOMPUTECONTEXTPRIORITY *psRGXSetComputeContextPriorityOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY sRGXSetComputeContextPriorityIN;
	PVRSRV_BRIDGE_IN_RGXSETCOMPUTECONTEXTPRIORITY *psRGXSetComputeContextPriorityIN = &sRGXSetComputeContextPriorityIN;

	psRGXSetComputeContextPriorityIN->hComputeContext=(IMG_HANDLE)(IMG_UINT64)psRGXSetComputeContextPriorityIN_32->hComputeContext;
	psRGXSetComputeContextPriorityIN->ui32Priority = psRGXSetComputeContextPriorityIN_32->ui32Priority;

	return PVRSRVBridgeRGXSetComputeContextPriority(ui32BridgeID,
					 psRGXSetComputeContextPriorityIN,
					 psRGXSetComputeContextPriorityOUT,
					 psConnection);

}
#endif

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterRGXCMPFunctions(IMG_VOID);
IMG_VOID UnregisterRGXCMPFunctions(IMG_VOID);

/*
 * Register all RGXCMP functions with services
 */
PVRSRV_ERROR RegisterRGXCMPFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT, compat_PVRSRVBridgeRGXCreateComputeContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT, compat_PVRSRVBridgeRGXDestroyComputeContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM, compat_PVRSRVBridgeRGXKickCDM);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA, compat_PVRSRVBridgeRGXFlushComputeData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY, compat_PVRSRVBridgeRGXSetComputeContextPriority);
#else
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXCREATECOMPUTECONTEXT, PVRSRVBridgeRGXCreateComputeContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXDESTROYCOMPUTECONTEXT, PVRSRVBridgeRGXDestroyComputeContext);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXKICKCDM, PVRSRVBridgeRGXKickCDM);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXFLUSHCOMPUTEDATA, PVRSRVBridgeRGXFlushComputeData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXCMP_RGXSETCOMPUTECONTEXTPRIORITY, PVRSRVBridgeRGXSetComputeContextPriority);

#endif
	return PVRSRV_OK;
}

/*
 * Unregister all rgxcmp functions with services
 */
IMG_VOID UnregisterRGXCMPFunctions(IMG_VOID)
{
}
