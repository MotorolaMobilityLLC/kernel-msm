/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxhwperf
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxhwperf
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

#include "rgxhwperf.h"


#include "common_rgxhwperf_bridge.h"

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



/* ***************************************************************************
 * Server-side bridge entry points
 */

static IMG_INT
PVRSRVBridgeRGXCtrlHWPerf(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCTRLHWPERF *psRGXCtrlHWPerfIN,
					 PVRSRV_BRIDGE_OUT_RGXCTRLHWPERF *psRGXCtrlHWPerfOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF);





				{
					/* Look up the address from the handle */
					psRGXCtrlHWPerfOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCtrlHWPerfIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCtrlHWPerfOUT->eError != PVRSRV_OK)
					{
						goto RGXCtrlHWPerf_exit;
					}

				}

	psRGXCtrlHWPerfOUT->eError =
		PVRSRVRGXCtrlHWPerfKM(
					hDevNodeInt,
					psRGXCtrlHWPerfIN->bEnable,
					psRGXCtrlHWPerfIN->ui64Mask);



RGXCtrlHWPerf_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXConfigEnableHWPerfCounters(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersIN,
					 PVRSRV_BRIDGE_OUT_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	RGX_HWPERF_CONFIG_CNTBLK *psBlockConfigsInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS);




	if (psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen != 0)
	{
		psBlockConfigsInt = OSAllocMem(psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK));
		if (!psBlockConfigsInt)
		{
			psRGXConfigEnableHWPerfCountersOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXConfigEnableHWPerfCounters_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXConfigEnableHWPerfCountersIN->psBlockConfigs, psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK))
				|| (OSCopyFromUser(NULL, psBlockConfigsInt, psRGXConfigEnableHWPerfCountersIN->psBlockConfigs,
				psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen * sizeof(RGX_HWPERF_CONFIG_CNTBLK)) != PVRSRV_OK) )
			{
				psRGXConfigEnableHWPerfCountersOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXConfigEnableHWPerfCounters_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXConfigEnableHWPerfCountersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXConfigEnableHWPerfCountersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXConfigEnableHWPerfCountersOUT->eError != PVRSRV_OK)
					{
						goto RGXConfigEnableHWPerfCounters_exit;
					}

				}

	psRGXConfigEnableHWPerfCountersOUT->eError =
		PVRSRVRGXConfigEnableHWPerfCountersKM(
					hDevNodeInt,
					psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen,
					psBlockConfigsInt);



RGXConfigEnableHWPerfCounters_exit:
	if (psBlockConfigsInt)
		OSFreeMem(psBlockConfigsInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXCtrlHWPerfCounters(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersIN,
					 PVRSRV_BRIDGE_OUT_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_UINT8 *ui8BlockIDsInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS);




	if (psRGXCtrlHWPerfCountersIN->ui32ArrayLen != 0)
	{
		ui8BlockIDsInt = OSAllocMem(psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT8));
		if (!ui8BlockIDsInt)
		{
			psRGXCtrlHWPerfCountersOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXCtrlHWPerfCounters_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXCtrlHWPerfCountersIN->pui8BlockIDs, psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT8))
				|| (OSCopyFromUser(NULL, ui8BlockIDsInt, psRGXCtrlHWPerfCountersIN->pui8BlockIDs,
				psRGXCtrlHWPerfCountersIN->ui32ArrayLen * sizeof(IMG_UINT8)) != PVRSRV_OK) )
			{
				psRGXCtrlHWPerfCountersOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXCtrlHWPerfCounters_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXCtrlHWPerfCountersOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXCtrlHWPerfCountersIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXCtrlHWPerfCountersOUT->eError != PVRSRV_OK)
					{
						goto RGXCtrlHWPerfCounters_exit;
					}

				}

	psRGXCtrlHWPerfCountersOUT->eError =
		PVRSRVRGXCtrlHWPerfCountersKM(
					hDevNodeInt,
					psRGXCtrlHWPerfCountersIN->bEnable,
					psRGXCtrlHWPerfCountersIN->ui32ArrayLen,
					ui8BlockIDsInt);



RGXCtrlHWPerfCounters_exit:
	if (ui8BlockIDsInt)
		OSFreeMem(ui8BlockIDsInt);

	return 0;
}

#ifdef CONFIG_COMPAT
/* Bridge in structure for RGXCtrlHWPerf */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERF_TAG
{
	/* IMG_HANDLE hDevNode; */
	IMG_UINT32 hDevNode;
	IMG_BOOL bEnable;
	IMG_UINT64 ui64Mask __attribute__ ((__packed__));
} compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERF;


static IMG_INT
compat_PVRSRVBridgeRGXCtrlHWPerf(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERF *psRGXCtrlHWPerfIN_32,
					 PVRSRV_BRIDGE_OUT_RGXCTRLHWPERF *psRGXCtrlHWPerfOUT,
					 CONNECTION_DATA *psConnection)
{
		PVRSRV_BRIDGE_IN_RGXCTRLHWPERF sRGXCtrlHWPerfIN;
		PVRSRV_BRIDGE_IN_RGXCTRLHWPERF *psRGXCtrlHWPerfIN = &sRGXCtrlHWPerfIN;

		psRGXCtrlHWPerfIN->hDevNode= (IMG_HANDLE)(IMG_UINT64)psRGXCtrlHWPerfIN_32->hDevNode;
		psRGXCtrlHWPerfIN->bEnable= psRGXCtrlHWPerfIN_32->bEnable;
		psRGXCtrlHWPerfIN->ui64Mask = psRGXCtrlHWPerfIN_32->ui64Mask ;

		return PVRSRVBridgeRGXCtrlHWPerf(ui32BridgeID,
					 psRGXCtrlHWPerfIN,
					 psRGXCtrlHWPerfOUT,
					 psConnection);

}

/* Bridge in structure for RGXConfigEnableHWPerfCounters */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS_TAG
{
	/* IMG_HANDLE hDevNode; */
	IMG_UINT32 hDevNode;
	IMG_UINT32 ui32ArrayLen;
	/* RGX_HWPERF_CONFIG_CNTBLK * psBlockConfigs; */
	IMG_UINT32 psBlockConfigs;
}__attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS;

static IMG_INT
compat_PVRSRVBridgeRGXConfigEnableHWPerfCounters(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersIN_32,
					 PVRSRV_BRIDGE_OUT_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS sRGXConfigEnableHWPerfCountersIN;
	PVRSRV_BRIDGE_IN_RGXCONFIGENABLEHWPERFCOUNTERS *psRGXConfigEnableHWPerfCountersIN = &sRGXConfigEnableHWPerfCountersIN;

    psRGXConfigEnableHWPerfCountersIN->hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXConfigEnableHWPerfCountersIN_32->hDevNode;
    psRGXConfigEnableHWPerfCountersIN->ui32ArrayLen = psRGXConfigEnableHWPerfCountersIN_32->ui32ArrayLen;
    psRGXConfigEnableHWPerfCountersIN->psBlockConfigs = (RGX_HWPERF_CONFIG_CNTBLK *)(IMG_UINT64)psRGXConfigEnableHWPerfCountersIN_32->psBlockConfigs;

    return PVRSRVBridgeRGXConfigEnableHWPerfCounters(ui32BridgeID,
					psRGXConfigEnableHWPerfCountersIN,
					psRGXConfigEnableHWPerfCountersOUT,
					psConnection);

}

/* Bridge in structure for RGXCtrlHWPerfCounters */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS_TAG
{
	/* IMG_HANDLE hDevNode; */
	IMG_UINT32 hDevNode;
	IMG_BOOL bEnable;
	IMG_UINT32 ui32ArrayLen;
	/* IMG_UINT8 * pui8BlockIDs; */
	IMG_UINT32 pui8BlockIDs;
}__attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS;

static IMG_INT
compat_PVRSRVBridgeRGXCtrlHWPerfCounters(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersIN_32,
					 PVRSRV_BRIDGE_OUT_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS sRGXCtrlHWPerfCountersIN;
	PVRSRV_BRIDGE_IN_RGXCTRLHWPERFCOUNTERS *psRGXCtrlHWPerfCountersIN = &sRGXCtrlHWPerfCountersIN;

	psRGXCtrlHWPerfCountersIN->hDevNode = (IMG_HANDLE)(unsigned long)psRGXCtrlHWPerfCountersIN_32->hDevNode;
	psRGXCtrlHWPerfCountersIN->bEnable = psRGXCtrlHWPerfCountersIN_32->bEnable;
	psRGXCtrlHWPerfCountersIN->ui32ArrayLen = psRGXCtrlHWPerfCountersIN_32->ui32ArrayLen;
	psRGXCtrlHWPerfCountersIN->pui8BlockIDs = (IMG_UINT8 *)(unsigned long)psRGXCtrlHWPerfCountersIN_32->pui8BlockIDs;

    return PVRSRVBridgeRGXCtrlHWPerfCounters(ui32BridgeID,
                         psRGXCtrlHWPerfCountersIN,
                         psRGXCtrlHWPerfCountersOUT,
                         psConnection);

}


#endif

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterRGXHWPERFFunctions(IMG_VOID);
IMG_VOID UnregisterRGXHWPERFFunctions(IMG_VOID);

/*
 * Register all RGXHWPERF functions with services
 */
PVRSRV_ERROR RegisterRGXHWPERFFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF, compat_PVRSRVBridgeRGXCtrlHWPerf);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS, compat_PVRSRVBridgeRGXConfigEnableHWPerfCounters);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS, compat_PVRSRVBridgeRGXCtrlHWPerfCounters);
#else
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERF, PVRSRVBridgeRGXCtrlHWPerf);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCONFIGENABLEHWPERFCOUNTERS, PVRSRVBridgeRGXConfigEnableHWPerfCounters);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXHWPERF_RGXCTRLHWPERFCOUNTERS, PVRSRVBridgeRGXCtrlHWPerfCounters);

#endif
	return PVRSRV_OK;
}

/*
 * Unregister all rgxhwperf functions with services
 */
IMG_VOID UnregisterRGXHWPERFFunctions(IMG_VOID)
{
}
