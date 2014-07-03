/*************************************************************************/ /*!
@File
@Title          Server bridge for srvcore
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for srvcore
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

#include "srvcore.h"
#include "pvrsrv.h"


#include "common_srvcore_bridge.h"

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
ReleaseGlobalEventObjectResManProxy(IMG_HANDLE hResmanItem)
{
	PVRSRV_ERROR eError;

	eError = ResManFreeResByPtr(hResmanItem);

	/* Freeing a resource should never fail... */
	PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));

	return eError;
}

static PVRSRV_ERROR
EventObjectCloseResManProxy(IMG_HANDLE hResmanItem)
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
PVRSRVBridgeConnect(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_CONNECT *psConnectIN,
					 PVRSRV_BRIDGE_OUT_CONNECT *psConnectOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_CONNECT);

	PVR_UNREFERENCED_PARAMETER(psConnection);




	psConnectOUT->eError =
		PVRSRVConnectKM(
					psConnectIN->ui32Flags,
					psConnectIN->ui32ClientBuildOptions,
					psConnectIN->ui32ClientDDKVersion,
					psConnectIN->ui32ClientDDKBuild);




	return 0;
}

static IMG_INT
PVRSRVBridgeDisconnect(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DISCONNECT *psDisconnectIN,
					 PVRSRV_BRIDGE_OUT_DISCONNECT *psDisconnectOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_DISCONNECT);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psDisconnectIN);




	psDisconnectOUT->eError =
		PVRSRVDisconnectKM(
					);




	return 0;
}

#ifndef CONFIG_COMPAT
static IMG_INT
PVRSRVBridgeEnumerateDevices(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_ENUMERATEDEVICES *psEnumerateDevicesIN,
					 PVRSRV_BRIDGE_OUT_ENUMERATEDEVICES *psEnumerateDevicesOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_DEVICE_IDENTIFIER *psDeviceIdentifierInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_ENUMERATEDEVICES);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psEnumerateDevicesIN);

	psEnumerateDevicesOUT->psDeviceIdentifier = psEnumerateDevicesIN->psDeviceIdentifier;



	{
		psDeviceIdentifierInt = OSAllocMem(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_IDENTIFIER));
		if (!psDeviceIdentifierInt)
		{
			psEnumerateDevicesOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto EnumerateDevices_exit;
		}
	}


	psEnumerateDevicesOUT->eError =
		PVRSRVEnumerateDevicesKM(
					&psEnumerateDevicesOUT->ui32NumDevices,
					psDeviceIdentifierInt);

	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*) psEnumerateDevicesOUT->psDeviceIdentifier, (PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_IDENTIFIER)))
		|| (OSCopyToUser(NULL, psEnumerateDevicesOUT->psDeviceIdentifier, psDeviceIdentifierInt,
		(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_IDENTIFIER))) != PVRSRV_OK) )
	{
		psEnumerateDevicesOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto EnumerateDevices_exit;
	}


EnumerateDevices_exit:
	if (psDeviceIdentifierInt)
		OSFreeMem(psDeviceIdentifierInt);

	return 0;
}
#endif

static IMG_INT
PVRSRVBridgeAcquireDeviceData(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_ACQUIREDEVICEDATA *psAcquireDeviceDataIN,
					 PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA *psAcquireDeviceDataOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevCookieInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_ACQUIREDEVICEDATA);





	psAcquireDeviceDataOUT->eError =
		PVRSRVAcquireDeviceDataKM(
					psAcquireDeviceDataIN->ui32DevIndex,
					psAcquireDeviceDataIN->eDeviceType,
					&hDevCookieInt);
	/* Exit early if bridged call fails */
	if(psAcquireDeviceDataOUT->eError != PVRSRV_OK)
	{
		goto AcquireDeviceData_exit;
	}

	psAcquireDeviceDataOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psAcquireDeviceDataOUT->hDevCookie,
							(IMG_HANDLE) hDevCookieInt,
							PVRSRV_HANDLE_TYPE_DEV_NODE,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							);
	if (psAcquireDeviceDataOUT->eError != PVRSRV_OK)
	{
		goto AcquireDeviceData_exit;
	}


AcquireDeviceData_exit:
	if (psAcquireDeviceDataOUT->eError != PVRSRV_OK)
	{
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeReleaseDeviceData(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA *psReleaseDeviceDataIN,
					 PVRSRV_BRIDGE_OUT_RELEASEDEVICEDATA *psReleaseDeviceDataOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevCookieInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_RELEASEDEVICEDATA);





				{
					/* Look up the address from the handle */
					psReleaseDeviceDataOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevCookieInt,
											psReleaseDeviceDataIN->hDevCookie,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psReleaseDeviceDataOUT->eError != PVRSRV_OK)
					{
						goto ReleaseDeviceData_exit;
					}

				}

	psReleaseDeviceDataOUT->eError =
		PVRSRVReleaseDeviceDataKM(
					hDevCookieInt);
	/* Exit early if bridged call fails */
	if(psReleaseDeviceDataOUT->eError != PVRSRV_OK)
	{
		goto ReleaseDeviceData_exit;
	}

	psReleaseDeviceDataOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psReleaseDeviceDataIN->hDevCookie,
					PVRSRV_HANDLE_TYPE_DEV_NODE);


ReleaseDeviceData_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeAcquireGlobalEventObject(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_ACQUIREGLOBALEVENTOBJECT *psAcquireGlobalEventObjectIN,
					 PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT *psAcquireGlobalEventObjectOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hGlobalEventObjectInt = IMG_NULL;
	IMG_HANDLE hGlobalEventObjectInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_ACQUIREGLOBALEVENTOBJECT);

	PVR_UNREFERENCED_PARAMETER(psAcquireGlobalEventObjectIN);




	psAcquireGlobalEventObjectOUT->eError =
		AcquireGlobalEventObjectServer(
					&hGlobalEventObjectInt);
	/* Exit early if bridged call fails */
	if(psAcquireGlobalEventObjectOUT->eError != PVRSRV_OK)
	{
		goto AcquireGlobalEventObject_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hGlobalEventObjectInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_SHARED_EVENT_OBJECT,
												hGlobalEventObjectInt,
												(RESMAN_FREE_FN)&ReleaseGlobalEventObjectServer);
	if (hGlobalEventObjectInt2 == IMG_NULL)
	{
		psAcquireGlobalEventObjectOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto AcquireGlobalEventObject_exit;
	}
	psAcquireGlobalEventObjectOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psAcquireGlobalEventObjectOUT->hGlobalEventObject,
							(IMG_HANDLE) hGlobalEventObjectInt2,
							PVRSRV_HANDLE_TYPE_SHARED_EVENT_OBJECT,
							PVRSRV_HANDLE_ALLOC_FLAG_SHARED
							);
	if (psAcquireGlobalEventObjectOUT->eError != PVRSRV_OK)
	{
		goto AcquireGlobalEventObject_exit;
	}


AcquireGlobalEventObject_exit:
	if (psAcquireGlobalEventObjectOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hGlobalEventObjectInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hGlobalEventObjectInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (hGlobalEventObjectInt)
		{
			ReleaseGlobalEventObjectServer(hGlobalEventObjectInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeReleaseGlobalEventObject(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT *psReleaseGlobalEventObjectIN,
					 PVRSRV_BRIDGE_OUT_RELEASEGLOBALEVENTOBJECT *psReleaseGlobalEventObjectOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hGlobalEventObjectInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_RELEASEGLOBALEVENTOBJECT);





				{
					/* Look up the address from the handle */
					psReleaseGlobalEventObjectOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hGlobalEventObjectInt2,
											psReleaseGlobalEventObjectIN->hGlobalEventObject,
											PVRSRV_HANDLE_TYPE_SHARED_EVENT_OBJECT);
					if(psReleaseGlobalEventObjectOUT->eError != PVRSRV_OK)
					{
						goto ReleaseGlobalEventObject_exit;
					}

				}

	psReleaseGlobalEventObjectOUT->eError = ReleaseGlobalEventObjectResManProxy(hGlobalEventObjectInt2);
	/* Exit early if bridged call fails */
	if(psReleaseGlobalEventObjectOUT->eError != PVRSRV_OK)
	{
		goto ReleaseGlobalEventObject_exit;
	}

	psReleaseGlobalEventObjectOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psReleaseGlobalEventObjectIN->hGlobalEventObject,
					PVRSRV_HANDLE_TYPE_SHARED_EVENT_OBJECT);


ReleaseGlobalEventObject_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeEventObjectOpen(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN *psEventObjectOpenIN,
					 PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN *psEventObjectOpenOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hEventObjectInt = IMG_NULL;
	IMG_HANDLE hEventObjectInt2 = IMG_NULL;
	IMG_HANDLE hOSEventInt = IMG_NULL;
	IMG_HANDLE hOSEventInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTOPEN);





				{
					/* Look up the address from the handle */
					psEventObjectOpenOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hEventObjectInt2,
											psEventObjectOpenIN->hEventObject,
											PVRSRV_HANDLE_TYPE_SHARED_EVENT_OBJECT);
					if(psEventObjectOpenOUT->eError != PVRSRV_OK)
					{
						goto EventObjectOpen_exit;
					}

					/* Look up the data from the resman address */
					psEventObjectOpenOUT->eError = ResManFindPrivateDataByPtr(hEventObjectInt2, (IMG_VOID **) &hEventObjectInt);

					if(psEventObjectOpenOUT->eError != PVRSRV_OK)
					{
						goto EventObjectOpen_exit;
					}
				}

	psEventObjectOpenOUT->eError =
		OSEventObjectOpen(
					hEventObjectInt,
					&hOSEventInt);
	/* Exit early if bridged call fails */
	if(psEventObjectOpenOUT->eError != PVRSRV_OK)
	{
		goto EventObjectOpen_exit;
	}

	/* Create a resman item and overwrite the handle with it */
	hOSEventInt2 = ResManRegisterRes(psConnection->hResManContext,
												RESMAN_TYPE_EVENT_OBJECT,
												hOSEventInt,
												(RESMAN_FREE_FN)&OSEventObjectClose);
	if (hOSEventInt2 == IMG_NULL)
	{
		psEventObjectOpenOUT->eError = PVRSRV_ERROR_UNABLE_TO_REGISTER_RESOURCE;
		goto EventObjectOpen_exit;
	}
	psEventObjectOpenOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psEventObjectOpenOUT->hOSEvent,
							(IMG_HANDLE) hOSEventInt2,
							PVRSRV_HANDLE_TYPE_EVENT_OBJECT_CONNECT,
							PVRSRV_HANDLE_ALLOC_FLAG_MULTI
							);
	if (psEventObjectOpenOUT->eError != PVRSRV_OK)
	{
		goto EventObjectOpen_exit;
	}


EventObjectOpen_exit:
	if (psEventObjectOpenOUT->eError != PVRSRV_OK)
	{
		/* If we have a valid resman item we should undo the bridge function by freeing the resman item */
		if (hOSEventInt2)
		{
			PVRSRV_ERROR eError = ResManFreeResByPtr(hOSEventInt2);

			/* Freeing a resource should never fail... */
			PVR_ASSERT((eError == PVRSRV_OK) || (eError == PVRSRV_ERROR_RETRY));
		}
		else if (hOSEventInt)
		{
			OSEventObjectClose(hOSEventInt);
		}
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeEventObjectClose(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE *psEventObjectCloseIN,
					 PVRSRV_BRIDGE_OUT_EVENTOBJECTCLOSE *psEventObjectCloseOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hOSEventKMInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTCLOSE);





				{
					/* Look up the address from the handle */
					psEventObjectCloseOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hOSEventKMInt2,
											psEventObjectCloseIN->hOSEventKM,
											PVRSRV_HANDLE_TYPE_EVENT_OBJECT_CONNECT);
					if(psEventObjectCloseOUT->eError != PVRSRV_OK)
					{
						goto EventObjectClose_exit;
					}

				}

	psEventObjectCloseOUT->eError = EventObjectCloseResManProxy(hOSEventKMInt2);
	/* Exit early if bridged call fails */
	if(psEventObjectCloseOUT->eError != PVRSRV_OK)
	{
		goto EventObjectClose_exit;
	}

	psEventObjectCloseOUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psEventObjectCloseIN->hOSEventKM,
					PVRSRV_HANDLE_TYPE_EVENT_OBJECT_CONNECT);


EventObjectClose_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeInitSrvDisconnect(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_INITSRVDISCONNECT *psInitSrvDisconnectIN,
					 PVRSRV_BRIDGE_OUT_INITSRVDISCONNECT *psInitSrvDisconnectOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_INITSRVDISCONNECT);





	psInitSrvDisconnectOUT->eError =
		PVRSRVInitSrvDisconnectKM(psConnection,
					psInitSrvDisconnectIN->bInitSuccesful,
					psInitSrvDisconnectIN->ui32ClientBuildOptions);




	return 0;
}

static IMG_INT
PVRSRVBridgeEventObjectWait(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT *psEventObjectWaitIN,
					 PVRSRV_BRIDGE_OUT_EVENTOBJECTWAIT *psEventObjectWaitOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hOSEventKMInt = IMG_NULL;
	IMG_HANDLE hOSEventKMInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTWAIT);





				{
					/* Look up the address from the handle */
					psEventObjectWaitOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hOSEventKMInt2,
											psEventObjectWaitIN->hOSEventKM,
											PVRSRV_HANDLE_TYPE_EVENT_OBJECT_CONNECT);
					if(psEventObjectWaitOUT->eError != PVRSRV_OK)
					{
						goto EventObjectWait_exit;
					}

					/* Look up the data from the resman address */
					psEventObjectWaitOUT->eError = ResManFindPrivateDataByPtr(hOSEventKMInt2, (IMG_VOID **) &hOSEventKMInt);

					if(psEventObjectWaitOUT->eError != PVRSRV_OK)
					{
						goto EventObjectWait_exit;
					}
				}

	psEventObjectWaitOUT->eError =
		OSEventObjectWait(
					hOSEventKMInt);



EventObjectWait_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeDumpDebugInfo(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_DUMPDEBUGINFO *psDumpDebugInfoIN,
					 PVRSRV_BRIDGE_OUT_DUMPDEBUGINFO *psDumpDebugInfoOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_DUMPDEBUGINFO);

	PVR_UNREFERENCED_PARAMETER(psConnection);




	psDumpDebugInfoOUT->eError =
		PVRSRVDumpDebugInfoKM(
					psDumpDebugInfoIN->ui32ui32VerbLevel);




	return 0;
}

static IMG_INT
PVRSRVBridgeInitSrvConnect(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_INITSRVCONNECT *psInitSrvConnectIN,
					 PVRSRV_BRIDGE_OUT_INITSRVCONNECT *psInitSrvConnectOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_INITSRVCONNECT);

	PVR_UNREFERENCED_PARAMETER(psInitSrvConnectIN);




	psInitSrvConnectOUT->eError =
		PVRSRVInitSrvConnectKM(psConnection
					);




	return 0;
}

static IMG_INT
PVRSRVBridgeGetDevClockSpeed(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED *psGetDevClockSpeedIN,
					 PVRSRV_BRIDGE_OUT_GETDEVCLOCKSPEED *psGetDevClockSpeedOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_GETDEVCLOCKSPEED);





				{
					/* Look up the address from the handle */
					psGetDevClockSpeedOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psGetDevClockSpeedIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psGetDevClockSpeedOUT->eError != PVRSRV_OK)
					{
						goto GetDevClockSpeed_exit;
					}

				}

	psGetDevClockSpeedOUT->eError =
		PVRSRVGetDevClockSpeedKM(
					hDevNodeInt,
					&psGetDevClockSpeedOUT->ui32ui32RGXClockSpeed);



GetDevClockSpeed_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeHWOpTimeout(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_HWOPTIMEOUT *psHWOpTimeoutIN,
					 PVRSRV_BRIDGE_OUT_HWOPTIMEOUT *psHWOpTimeoutOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_HWOPTIMEOUT);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psHWOpTimeoutIN);




	psHWOpTimeoutOUT->eError =
		PVRSRVHWOpTimeoutKM(
					);




	return 0;
}

static IMG_INT
PVRSRVBridgeKickDevices(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_KICKDEVICES *psKickDevicesIN,
					 PVRSRV_BRIDGE_OUT_KICKDEVICES *psKickDevicesOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_KICKDEVICES);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psKickDevicesIN);




	psKickDevicesOUT->eError =
		PVRSRVKickDevicesKM(
					);




	return 0;
}

static IMG_INT
PVRSRVBridgeResetHWRLogs(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RESETHWRLOGS *psResetHWRLogsIN,
					 PVRSRV_BRIDGE_OUT_RESETHWRLOGS *psResetHWRLogsOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_RESETHWRLOGS);





				{
					/* Look up the address from the handle */
					psResetHWRLogsOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psResetHWRLogsIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psResetHWRLogsOUT->eError != PVRSRV_OK)
					{
						goto ResetHWRLogs_exit;
					}

				}

	psResetHWRLogsOUT->eError =
		PVRSRVResetHWRLogsKM(
					hDevNodeInt);



ResetHWRLogs_exit:

	return 0;
}

#ifdef CONFIG_COMPAT

/* ***************************************************************************
 * Compat Layer Server-side bridge entry points
 */

typedef struct _compat_PVRSRV_DEVICE_IDENTIFIER_
{
	PVRSRV_DEVICE_TYPE      eDeviceType;        /*!< Identifies the type of the device */
	PVRSRV_DEVICE_CLASS     eDeviceClass;       /*!< Identifies general class of device */
	IMG_UINT32              ui32DeviceIndex;    /*!< Index of the device within the system */
	/* IMG_CHAR                *pszPDumpDevName;   !< Pdump memory bank name */
	IMG_UINT32                pszPDumpDevName;   /*!< Pdump memory bank name */
	/* IMG_CHAR                *pszPDumpRegName;   !< Pdump register bank name */
	IMG_UINT32                pszPDumpRegName;   /*!< Pdump memory bank name */
} compat_PVRSRV_DEVICE_IDENTIFIER;

/* Bridge in structure for EnumerateDevices */
typedef struct compat_PVRSRV_BRIDGE_IN_ENUMERATEDEVICES_TAG
{
	/* Output pointer psDeviceIdentifier is also an implied input */
	/* compat_PVRSRV_DEVICE_IDENTIFIER * psDeviceIdentifier; */
	IMG_UINT32 psDeviceIdentifier;
} compat_PVRSRV_BRIDGE_IN_ENUMERATEDEVICES;

/* Bridge out structure for EnumerateDevices */
typedef struct compat_PVRSRV_BRIDGE_OUT_ENUMERATEDEVICES_TAG
{
	IMG_UINT32 ui32NumDevices;
	/* compat_PVRSRV_DEVICE_IDENTIFIER * psDeviceIdentifier; */
	IMG_UINT32 psDeviceIdentifier;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_ENUMERATEDEVICES;

static IMG_INT
compat_PVRSRVBridgeEnumerateDevices(IMG_UINT32 ui32BridgeID,
				compat_PVRSRV_BRIDGE_IN_ENUMERATEDEVICES *psEnumerateDevicesIN,
				compat_PVRSRV_BRIDGE_OUT_ENUMERATEDEVICES *psEnumerateDevicesOUT,
				CONNECTION_DATA *psConnection)
{
	compat_PVRSRV_DEVICE_IDENTIFIER *psDeviceIdentifierInt32 = IMG_NULL;
	PVRSRV_DEVICE_IDENTIFIER *psDeviceIdentifierInt64 = IMG_NULL;
	IMG_UINT32 i;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_SRVCORE_ENUMERATEDEVICES);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psEnumerateDevicesIN);

	psEnumerateDevicesOUT->psDeviceIdentifier = psEnumerateDevicesIN->psDeviceIdentifier;
	{
		psDeviceIdentifierInt32 = OSAllocMem(PVRSRV_MAX_DEVICES * sizeof(compat_PVRSRV_DEVICE_IDENTIFIER));
		if (!psDeviceIdentifierInt32)
		{
			psEnumerateDevicesOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto EnumerateDevices_exit;
		}
		psDeviceIdentifierInt64 = OSAllocMem(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_IDENTIFIER));
		if (!psDeviceIdentifierInt64)
		{
			psEnumerateDevicesOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto EnumerateDevices_exit;
		}
	}

	psEnumerateDevicesOUT->eError =
		PVRSRVEnumerateDevicesKM(
				&psEnumerateDevicesOUT->ui32NumDevices,
				psDeviceIdentifierInt64);

	/* Convert PVRSRV_DEVICE_IDENTIFIER to compat_PVRSRV_DEVICE_IDENTIFIER */
	for (i=0; i<PVRSRV_MAX_DEVICES; i++)
	{
		psDeviceIdentifierInt32[i].eDeviceType = psDeviceIdentifierInt64[i].eDeviceType;
		psDeviceIdentifierInt32[i].eDeviceClass = psDeviceIdentifierInt64[i].eDeviceClass;
		psDeviceIdentifierInt32[i].ui32DeviceIndex= psDeviceIdentifierInt64[i].ui32DeviceIndex;
		psDeviceIdentifierInt32[i].pszPDumpDevName= (IMG_UINT32)(IMG_UINT64)psDeviceIdentifierInt64[i].pszPDumpDevName;
		psDeviceIdentifierInt32[i].pszPDumpRegName= (IMG_UINT32)(IMG_UINT64)psDeviceIdentifierInt64[i].pszPDumpRegName;
	}

	if ( !OSAccessOK(PVR_VERIFY_WRITE, (IMG_VOID*)(IMG_UINT64) psEnumerateDevicesOUT->psDeviceIdentifier, (PVRSRV_MAX_DEVICES * sizeof(compat_PVRSRV_DEVICE_IDENTIFIER)))
		|| (OSCopyToUser(NULL, (compat_PVRSRV_DEVICE_IDENTIFIER *)(IMG_UINT64)psEnumerateDevicesOUT->psDeviceIdentifier, psDeviceIdentifierInt32,
		(PVRSRV_MAX_DEVICES * sizeof(compat_PVRSRV_DEVICE_IDENTIFIER))) != PVRSRV_OK) )
	{
		psEnumerateDevicesOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto EnumerateDevices_exit;
	}

EnumerateDevices_exit:
	if (psDeviceIdentifierInt32)
		OSFreeMem(psDeviceIdentifierInt32);
	if (psDeviceIdentifierInt64)
		OSFreeMem(psDeviceIdentifierInt64);

	return 0;
}

/* Bridge out structure for AcquireDeviceData */
typedef struct compat_PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA_TAG
{
	/* IMG_HANDLE hDevCookie; */
	IMG_UINT32 hDevCookie;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA;

static IMG_INT
compat_PVRSRVBridgeAcquireDeviceData(IMG_UINT32 ui32BridgeID,
					PVRSRV_BRIDGE_IN_ACQUIREDEVICEDATA *psAcquireDeviceDataIN,
					compat_PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA *psAcquireDeviceDataOUT_32,
					CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA sAcquireDeviceDataOUT;
	PVRSRV_BRIDGE_OUT_ACQUIREDEVICEDATA *psAcquireDeviceDataOUT = &sAcquireDeviceDataOUT;

	ret = PVRSRVBridgeAcquireDeviceData(ui32BridgeID, psAcquireDeviceDataIN,
					psAcquireDeviceDataOUT, psConnection);

	psAcquireDeviceDataOUT_32->eError = psAcquireDeviceDataOUT->eError;
	PVR_ASSERT(!((IMG_UINT64)psAcquireDeviceDataOUT->hDevCookie & 0xFFFFFFFF00000000ULL));
	psAcquireDeviceDataOUT_32->hDevCookie = (IMG_UINT32)(IMG_UINT64)psAcquireDeviceDataOUT->hDevCookie;

	return ret;
}

/*******************************************
            ReleaseDeviceData
 *******************************************/
/* Bridge in structure for ReleaseDeviceData */
typedef struct compat_PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA_TAG
{
        /*IMG_HANDLE hDevCookie;*/
        IMG_UINT32 hDevCookie;
} compat_PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA;

static IMG_INT
compat_PVRSRVBridgeReleaseDeviceData(IMG_UINT32 ui32BridgeID,
                                         compat_PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA *psReleaseDeviceDataIN_32,
                                         PVRSRV_BRIDGE_OUT_RELEASEDEVICEDATA *psReleaseDeviceDataOUT,
                                         CONNECTION_DATA *psConnection)
{
        PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA sReleaseDeviceDataIN;
        PVRSRV_BRIDGE_IN_RELEASEDEVICEDATA *psReleaseDeviceDataIN = &sReleaseDeviceDataIN;

        psReleaseDeviceDataIN->hDevCookie = (IMG_HANDLE)(IMG_UINT64)psReleaseDeviceDataIN_32->hDevCookie;

        return PVRSRVBridgeReleaseDeviceData(ui32BridgeID, psReleaseDeviceDataIN,
					psReleaseDeviceDataOUT, psConnection);
}


/* Bridge out structure for AcquireGlobalEventObject */
typedef struct compat_PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT_TAG
{
	/* IMG_HANDLE hGlobalEventObject; */
	IMG_UINT32 hGlobalEventObject;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT;

static IMG_INT
compat_PVRSRVBridgeAcquireGlobalEventObject(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_ACQUIREGLOBALEVENTOBJECT *psAcquireGlobalEventObjectIN,
					 compat_PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT *psAcquireGlobalEventObjectOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT sAcquireGlobalEventObjectOUT;
	PVRSRV_BRIDGE_OUT_ACQUIREGLOBALEVENTOBJECT *psAcquireGlobalEventObjectOUT = &sAcquireGlobalEventObjectOUT;

	ret = PVRSRVBridgeAcquireGlobalEventObject(ui32BridgeID, psAcquireGlobalEventObjectIN,
					psAcquireGlobalEventObjectOUT, psConnection);

	psAcquireGlobalEventObjectOUT_32->eError = psAcquireGlobalEventObjectOUT->eError;
	PVR_ASSERT(!((IMG_UINT64)psAcquireGlobalEventObjectOUT->hGlobalEventObject & 0xFFFFFFFF00000000ULL));
	psAcquireGlobalEventObjectOUT_32->hGlobalEventObject= (IMG_UINT32)(IMG_UINT64)psAcquireGlobalEventObjectOUT->hGlobalEventObject;

	return ret;
}

/* Bridge in structure for ReleaseGlobalEventObject */
typedef struct compat_PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT_TAG
{
	/* IMG_HANDLE hGlobalEventObject; */
	IMG_UINT32 hGlobalEventObject;
} compat_PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT;

static IMG_INT
compat_PVRSRVBridgeReleaseGlobalEventObject(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT *psReleaseGlobalEventObjectIN_32,
					 PVRSRV_BRIDGE_OUT_RELEASEGLOBALEVENTOBJECT *psReleaseGlobalEventObjectOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT sReleaseGlobalEventObjectIN;
	PVRSRV_BRIDGE_IN_RELEASEGLOBALEVENTOBJECT *psReleaseGlobalEventObjectIN = &sReleaseGlobalEventObjectIN;

	psReleaseGlobalEventObjectIN->hGlobalEventObject = (IMG_HANDLE)(IMG_UINT64)psReleaseGlobalEventObjectIN_32->hGlobalEventObject;

	return PVRSRVBridgeReleaseGlobalEventObject(ui32BridgeID, psReleaseGlobalEventObjectIN,
					psReleaseGlobalEventObjectOUT, psConnection);
}

/* Bridge in structure for EventObjectOpen */
typedef struct compat_PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN_TAG
{
	/* IMG_HANDLE hEventObject; */
	IMG_UINT32 hEventObject;
} compat_PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN;


/* Bridge out structure for EventObjectOpen */
typedef struct compat_PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN_TAG
{
	/* IMG_HANDLE hOSEvent; */
	IMG_UINT32 hOSEvent;
	PVRSRV_ERROR eError;
} compat_PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN;

static IMG_INT
compat_PVRSRVBridgeEventObjectOpen(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN *psEventObjectOpenIN_32,
					 compat_PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN *psEventObjectOpenOUT_32,
					 CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN sEventObjectOpenIN;
	PVRSRV_BRIDGE_IN_EVENTOBJECTOPEN *psEventObjectOpenIN = &sEventObjectOpenIN;
	PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN sEventObjectOpenOUT;
	PVRSRV_BRIDGE_OUT_EVENTOBJECTOPEN *psEventObjectOpenOUT = &sEventObjectOpenOUT;

	psEventObjectOpenIN->hEventObject = (IMG_HANDLE)(IMG_UINT64)(psEventObjectOpenIN_32->hEventObject);

	ret = PVRSRVBridgeEventObjectOpen(ui32BridgeID, psEventObjectOpenIN,
					 psEventObjectOpenOUT, psConnection);

	psEventObjectOpenOUT_32->eError = psEventObjectOpenOUT->eError;
	PVR_ASSERT(!((IMG_UINT64)psEventObjectOpenOUT->hOSEvent & 0xFFFFFFFF00000000ULL));
	psEventObjectOpenOUT_32->hOSEvent = (IMG_UINT32)(IMG_UINT64)psEventObjectOpenOUT->hOSEvent;

	return ret;
}

/* Bridge in structure for EventObjectWait */
typedef struct compat_PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT_TAG
{
	/* IMG_HANDLE hOSEventKM; */
	IMG_UINT32 hOSEventKM;
} compat_PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT;

static IMG_INT
compat_PVRSRVBridgeEventObjectWait(IMG_UINT32 ui32BridgeID,
					PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT *psEventObjectWaitIN_32,
					PVRSRV_BRIDGE_OUT_EVENTOBJECTWAIT *psEventObjectWaitOUT,
					CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT sEventObjectWaitIN;
	PVRSRV_BRIDGE_IN_EVENTOBJECTWAIT *psEventObjectWaitIN = &sEventObjectWaitIN;

	psEventObjectWaitIN->hOSEventKM = (IMG_HANDLE)(IMG_UINT64)psEventObjectWaitIN_32->hOSEventKM;

	return PVRSRVBridgeEventObjectWait(ui32BridgeID, psEventObjectWaitIN,
					psEventObjectWaitOUT, psConnection);
}

/* Bridge in structure for EventObjectClose */
typedef struct compat_PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE_TAG
{
	/* IMG_HANDLE hOSEventKM; */
	IMG_UINT32 hOSEventKM;
} compat_PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE;

static IMG_INT
compat_PVRSRVBridgeEventObjectClose(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE *psEventObjectCloseIN_32,
					 PVRSRV_BRIDGE_OUT_EVENTOBJECTCLOSE *psEventObjectCloseOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE sEventObjectCloseIN;
	PVRSRV_BRIDGE_IN_EVENTOBJECTCLOSE *psEventObjectCloseIN = &sEventObjectCloseIN;

	psEventObjectCloseIN->hOSEventKM = (IMG_HANDLE)(IMG_UINT64)(psEventObjectCloseIN_32->hOSEventKM);

	return PVRSRVBridgeEventObjectClose(ui32BridgeID, psEventObjectCloseIN,
					psEventObjectCloseOUT, psConnection);
}

/* Bridge in structure for GetDevClockSpeed */
typedef struct compat_PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED_TAG
{
	/* IMG_HANDLE hDevNode; */
	IMG_UINT32 hDevNode;
} compat_PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED;

static IMG_INT
compat_PVRSRVBridgeGetDevClockSpeed(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED *psGetDevClockSpeedIN_32,
					 PVRSRV_BRIDGE_OUT_GETDEVCLOCKSPEED *psGetDevClockSpeedOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED sGetDevClockSpeedIN;
	PVRSRV_BRIDGE_IN_GETDEVCLOCKSPEED *psGetDevClockSpeedIN = &sGetDevClockSpeedIN;

	psGetDevClockSpeedIN->hDevNode = (IMG_HANDLE)(IMG_UINT64)(psGetDevClockSpeedIN_32->hDevNode);

	return PVRSRVBridgeGetDevClockSpeed(ui32BridgeID, psGetDevClockSpeedIN,
					psGetDevClockSpeedOUT, psConnection);

}

/*******************************************
            ResetHWRLogs
 *******************************************/

/* Bridge in structure for ResetHWRLogs */
typedef struct compat_PVRSRV_BRIDGE_IN_RESETHWRLOGS_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
} compat_PVRSRV_BRIDGE_IN_RESETHWRLOGS;

static IMG_INT
compat_PVRSRVBridgeResetHWRLogs(IMG_UINT32 ui32BridgeID,
					compat_PVRSRV_BRIDGE_IN_RESETHWRLOGS *psResetHWRLogsIN_32,
					PVRSRV_BRIDGE_OUT_RESETHWRLOGS *psResetHWRLogsOUT,
					CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RESETHWRLOGS sResetHWRLogsIN;
	PVRSRV_BRIDGE_IN_RESETHWRLOGS *psResetHWRLogsIN = &sResetHWRLogsIN;

	psResetHWRLogsIN->hDevNode = (IMG_HANDLE)(IMG_UINT64)psResetHWRLogsIN_32->hDevNode;

	return PVRSRVBridgeResetHWRLogs(ui32BridgeID, psResetHWRLogsIN,
					psResetHWRLogsOUT, psConnection);
}

#endif /* CONFIG_COMPAT */

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterSRVCOREFunctions(IMG_VOID);
IMG_VOID UnregisterSRVCOREFunctions(IMG_VOID);

/*
 * Register all SRVCORE functions with services
 */
PVRSRV_ERROR RegisterSRVCOREFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_CONNECT, PVRSRVBridgeConnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_DISCONNECT, PVRSRVBridgeDisconnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ENUMERATEDEVICES, compat_PVRSRVBridgeEnumerateDevices);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ACQUIREDEVICEDATA, compat_PVRSRVBridgeAcquireDeviceData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RELEASEDEVICEDATA, compat_PVRSRVBridgeReleaseDeviceData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_INITSRVCONNECT, PVRSRVBridgeInitSrvConnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_INITSRVDISCONNECT, PVRSRVBridgeInitSrvDisconnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ACQUIREGLOBALEVENTOBJECT, compat_PVRSRVBridgeAcquireGlobalEventObject);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RELEASEGLOBALEVENTOBJECT, compat_PVRSRVBridgeReleaseGlobalEventObject);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTOPEN, compat_PVRSRVBridgeEventObjectOpen);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTWAIT, compat_PVRSRVBridgeEventObjectWait);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTCLOSE, compat_PVRSRVBridgeEventObjectClose);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_DUMPDEBUGINFO, PVRSRVBridgeDumpDebugInfo);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_GETDEVCLOCKSPEED, compat_PVRSRVBridgeGetDevClockSpeed);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_HWOPTIMEOUT, PVRSRVBridgeHWOpTimeout);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_KICKDEVICES, PVRSRVBridgeKickDevices);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RESETHWRLOGS, compat_PVRSRVBridgeResetHWRLogs);
#else
	/*WPAT FINDME*/
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_CONNECT, PVRSRVBridgeConnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_DISCONNECT, PVRSRVBridgeDisconnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ENUMERATEDEVICES, PVRSRVBridgeEnumerateDevices);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ACQUIREDEVICEDATA, PVRSRVBridgeAcquireDeviceData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RELEASEDEVICEDATA, PVRSRVBridgeReleaseDeviceData);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_INITSRVCONNECT, PVRSRVBridgeInitSrvConnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_INITSRVDISCONNECT, PVRSRVBridgeInitSrvDisconnect);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_ACQUIREGLOBALEVENTOBJECT, PVRSRVBridgeAcquireGlobalEventObject);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RELEASEGLOBALEVENTOBJECT, PVRSRVBridgeReleaseGlobalEventObject);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTOPEN, PVRSRVBridgeEventObjectOpen);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTWAIT, PVRSRVBridgeEventObjectWait);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_EVENTOBJECTCLOSE, PVRSRVBridgeEventObjectClose);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_DUMPDEBUGINFO, PVRSRVBridgeDumpDebugInfo);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_GETDEVCLOCKSPEED, PVRSRVBridgeGetDevClockSpeed);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_HWOPTIMEOUT, PVRSRVBridgeHWOpTimeout);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_KICKDEVICES, PVRSRVBridgeKickDevices);
	SetDispatchTableEntry(PVRSRV_BRIDGE_SRVCORE_RESETHWRLOGS, PVRSRVBridgeResetHWRLogs);
#endif

	return PVRSRV_OK;
}

/*
 * Unregister all srvcore functions with services
 */
IMG_VOID UnregisterSRVCOREFunctions(IMG_VOID)
{
}
