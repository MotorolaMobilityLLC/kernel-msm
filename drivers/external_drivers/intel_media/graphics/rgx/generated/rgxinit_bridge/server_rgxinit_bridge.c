/*************************************************************************/ /*!
@File
@Title          Server bridge for rgxinit
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for rgxinit
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

#include "rgxinit.h"


#include "common_rgxinit_bridge.h"

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
PVRSRVBridgeRGXInitAllocFWImgMem(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXINITALLOCFWIMGMEM *psRGXInitAllocFWImgMemIN,
					 PVRSRV_BRIDGE_OUT_RGXINITALLOCFWIMGMEM *psRGXInitAllocFWImgMemOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWCodeAllocServerExportCookieInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWDataAllocServerExportCookieInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWCorememAllocServerExportCookieInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXINIT_RGXINITALLOCFWIMGMEM);





				{
					/* Look up the address from the handle */
					psRGXInitAllocFWImgMemOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXInitAllocFWImgMemIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
					{
						goto RGXInitAllocFWImgMem_exit;
					}

				}

	psRGXInitAllocFWImgMemOUT->eError =
		PVRSRVRGXInitAllocFWImgMemKM(
					hDevNodeInt,
					psRGXInitAllocFWImgMemIN->uiFWCodeLen,
					psRGXInitAllocFWImgMemIN->uiFWDataLen,
					psRGXInitAllocFWImgMemIN->uiFWCoremem,
					&psFWCodeAllocServerExportCookieInt,
					&psRGXInitAllocFWImgMemOUT->sFWCodeDevVAddrBase,
					&psFWDataAllocServerExportCookieInt,
					&psRGXInitAllocFWImgMemOUT->sFWDataDevVAddrBase,
					&psFWCorememAllocServerExportCookieInt,
					&psRGXInitAllocFWImgMemOUT->sFWCorememDevVAddrBase,
					&psRGXInitAllocFWImgMemOUT->sFWCorememMetaVAddrBase);
	/* Exit early if bridged call fails */
	if(psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
	{
		goto RGXInitAllocFWImgMem_exit;
	}

	psRGXInitAllocFWImgMemOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXInitAllocFWImgMemOUT->hFWCodeAllocServerExportCookie,
							(IMG_HANDLE) psFWCodeAllocServerExportCookieInt,
							PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
	{
		goto RGXInitAllocFWImgMem_exit;
	}
	psRGXInitAllocFWImgMemOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXInitAllocFWImgMemOUT->hFWDataAllocServerExportCookie,
							(IMG_HANDLE) psFWDataAllocServerExportCookieInt,
							PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
	{
		goto RGXInitAllocFWImgMem_exit;
	}
	psRGXInitAllocFWImgMemOUT->eError = PVRSRVAllocHandle(psConnection->psHandleBase,
							&psRGXInitAllocFWImgMemOUT->hFWCorememAllocServerExportCookie,
							(IMG_HANDLE) psFWCorememAllocServerExportCookieInt,
							PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE,
							PVRSRV_HANDLE_ALLOC_FLAG_NONE
							);
	if (psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
	{
		goto RGXInitAllocFWImgMem_exit;
	}


RGXInitAllocFWImgMem_exit:
	if (psRGXInitAllocFWImgMemOUT->eError != PVRSRV_OK)
	{
	}


	return 0;
}

static IMG_INT
PVRSRVBridgeRGXInitFirmware(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXINITFIRMWARE *psRGXInitFirmwareIN,
					 PVRSRV_BRIDGE_OUT_RGXINITFIRMWARE *psRGXInitFirmwareOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	IMG_UINT32 *ui32RGXFWAlignChecksInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXINIT_RGXINITFIRMWARE);




	if (psRGXInitFirmwareIN->ui32RGXFWAlignChecksSize != 0)
	{
		ui32RGXFWAlignChecksInt = OSAllocMem(psRGXInitFirmwareIN->ui32RGXFWAlignChecksSize * sizeof(IMG_UINT32));
		if (!ui32RGXFWAlignChecksInt)
		{
			psRGXInitFirmwareOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto RGXInitFirmware_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXInitFirmwareIN->pui32RGXFWAlignChecks, psRGXInitFirmwareIN->ui32RGXFWAlignChecksSize * sizeof(IMG_UINT32))
				|| (OSCopyFromUser(NULL, ui32RGXFWAlignChecksInt, psRGXInitFirmwareIN->pui32RGXFWAlignChecks,
				psRGXInitFirmwareIN->ui32RGXFWAlignChecksSize * sizeof(IMG_UINT32)) != PVRSRV_OK) )
			{
				psRGXInitFirmwareOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXInitFirmware_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXInitFirmwareOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXInitFirmwareIN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXInitFirmwareOUT->eError != PVRSRV_OK)
					{
						goto RGXInitFirmware_exit;
					}

				}

	psRGXInitFirmwareOUT->eError =
		PVRSRVRGXInitFirmwareKM(
					hDevNodeInt,
					&psRGXInitFirmwareOUT->spsRGXFwInit,
					psRGXInitFirmwareIN->bEnableSignatureChecks,
					psRGXInitFirmwareIN->ui32SignatureChecksBufSize,
					psRGXInitFirmwareIN->ui32HWPerfFWBufSizeKB,
					psRGXInitFirmwareIN->ui64HWPerfFilter,
					psRGXInitFirmwareIN->ui32RGXFWAlignChecksSize,
					ui32RGXFWAlignChecksInt,
					psRGXInitFirmwareIN->ui32ConfigFlags,
					psRGXInitFirmwareIN->ui32LogType,
					psRGXInitFirmwareIN->ui32FilterFlags,
					&psRGXInitFirmwareIN->sClientBVNC,
					psRGXInitFirmwareIN->ui32APMLatency,
					psRGXInitFirmwareIN->ui32CoreClockSpeed);



RGXInitFirmware_exit:
	if (ui32RGXFWAlignChecksInt)
		OSFreeMem(ui32RGXFWAlignChecksInt);

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXInitLoadFWImage(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXINITLOADFWIMAGE *psRGXInitLoadFWImageIN,
					 PVRSRV_BRIDGE_OUT_RGXINITLOADFWIMAGE *psRGXInitLoadFWImageOUT,
					 CONNECTION_DATA *psConnection)
{
	PMR * psImgDestImportInt = IMG_NULL;
	IMG_HANDLE hImgDestImportInt2 = IMG_NULL;
	PMR * psImgSrcImportInt = IMG_NULL;
	IMG_HANDLE hImgSrcImportInt2 = IMG_NULL;
	PMR * psSigImportInt = IMG_NULL;
	IMG_HANDLE hSigImportInt2 = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXINIT_RGXINITLOADFWIMAGE);





				{
					/* Look up the address from the handle */
					psRGXInitLoadFWImageOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hImgDestImportInt2,
											psRGXInitLoadFWImageIN->hImgDestImport,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}

					/* Look up the data from the resman address */
					psRGXInitLoadFWImageOUT->eError = ResManFindPrivateDataByPtr(hImgDestImportInt2, (IMG_VOID **) &psImgDestImportInt);

					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}
				}

				{
					/* Look up the address from the handle */
					psRGXInitLoadFWImageOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hImgSrcImportInt2,
											psRGXInitLoadFWImageIN->hImgSrcImport,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}

					/* Look up the data from the resman address */
					psRGXInitLoadFWImageOUT->eError = ResManFindPrivateDataByPtr(hImgSrcImportInt2, (IMG_VOID **) &psImgSrcImportInt);

					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}
				}

				{
					/* Look up the address from the handle */
					psRGXInitLoadFWImageOUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hSigImportInt2,
											psRGXInitLoadFWImageIN->hSigImport,
											PVRSRV_HANDLE_TYPE_PHYSMEM_PMR);
					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}

					/* Look up the data from the resman address */
					psRGXInitLoadFWImageOUT->eError = ResManFindPrivateDataByPtr(hSigImportInt2, (IMG_VOID **) &psSigImportInt);

					if(psRGXInitLoadFWImageOUT->eError != PVRSRV_OK)
					{
						goto RGXInitLoadFWImage_exit;
					}
				}

	psRGXInitLoadFWImageOUT->eError =
		PVRSRVRGXInitLoadFWImageKM(
					psImgDestImportInt,
					psImgSrcImportInt,
					psRGXInitLoadFWImageIN->ui64ImgLen,
					psSigImportInt,
					psRGXInitLoadFWImageIN->ui64SigLen);



RGXInitLoadFWImage_exit:

	return 0;
}

static IMG_INT
PVRSRVBridgeRGXInitDevPart2(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_RGXINITDEVPART2 *psRGXInitDevPart2IN,
					 PVRSRV_BRIDGE_OUT_RGXINITDEVPART2 *psRGXInitDevPart2OUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_HANDLE hDevNodeInt = IMG_NULL;
	RGX_INIT_COMMAND *psInitScriptInt = IMG_NULL;
	RGX_INIT_COMMAND *psDbgScriptInt = IMG_NULL;
	RGX_INIT_COMMAND *psDbgBusScriptInt = IMG_NULL;
	RGX_INIT_COMMAND *psDeinitScriptInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWCodeAllocServerExportCookieInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWDataAllocServerExportCookieInt = IMG_NULL;
	DEVMEM_EXPORTCOOKIE * psFWCorememAllocServerExportCookieInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_RGXINIT_RGXINITDEVPART2);





	{
		psInitScriptInt = OSAllocMem(RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND));
		if (!psInitScriptInt)
		{
			psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXInitDevPart2_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXInitDevPart2IN->psInitScript, RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND))
				|| (OSCopyFromUser(NULL, psInitScriptInt, psRGXInitDevPart2IN->psInitScript,
				RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND)) != PVRSRV_OK) )
			{
				psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXInitDevPart2_exit;
			}

	{
		psDbgScriptInt = OSAllocMem(RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND));
		if (!psDbgScriptInt)
		{
			psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXInitDevPart2_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXInitDevPart2IN->psDbgScript, RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND))
				|| (OSCopyFromUser(NULL, psDbgScriptInt, psRGXInitDevPart2IN->psDbgScript,
				RGX_MAX_INIT_COMMANDS * sizeof(RGX_INIT_COMMAND)) != PVRSRV_OK) )
			{
				psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXInitDevPart2_exit;
			}

	{
		psDbgBusScriptInt = OSAllocMem(RGX_MAX_DBGBUS_COMMANDS * sizeof(RGX_INIT_COMMAND));
		if (!psDbgBusScriptInt)
		{
			psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXInitDevPart2_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXInitDevPart2IN->psDbgBusScript, RGX_MAX_DBGBUS_COMMANDS * sizeof(RGX_INIT_COMMAND))
				|| (OSCopyFromUser(NULL, psDbgBusScriptInt, psRGXInitDevPart2IN->psDbgBusScript,
				RGX_MAX_DBGBUS_COMMANDS * sizeof(RGX_INIT_COMMAND)) != PVRSRV_OK) )
			{
				psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXInitDevPart2_exit;
			}

	{
		psDeinitScriptInt = OSAllocMem(RGX_MAX_DEINIT_COMMANDS * sizeof(RGX_INIT_COMMAND));
		if (!psDeinitScriptInt)
		{
			psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;

			goto RGXInitDevPart2_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psRGXInitDevPart2IN->psDeinitScript, RGX_MAX_DEINIT_COMMANDS * sizeof(RGX_INIT_COMMAND))
				|| (OSCopyFromUser(NULL, psDeinitScriptInt, psRGXInitDevPart2IN->psDeinitScript,
				RGX_MAX_DEINIT_COMMANDS * sizeof(RGX_INIT_COMMAND)) != PVRSRV_OK) )
			{
				psRGXInitDevPart2OUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto RGXInitDevPart2_exit;
			}

				{
					/* Look up the address from the handle */
					psRGXInitDevPart2OUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &hDevNodeInt,
											psRGXInitDevPart2IN->hDevNode,
											PVRSRV_HANDLE_TYPE_DEV_NODE);
					if(psRGXInitDevPart2OUT->eError != PVRSRV_OK)
					{
						goto RGXInitDevPart2_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXInitDevPart2OUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &psFWCodeAllocServerExportCookieInt,
											psRGXInitDevPart2IN->hFWCodeAllocServerExportCookie,
											PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);
					if(psRGXInitDevPart2OUT->eError != PVRSRV_OK)
					{
						goto RGXInitDevPart2_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXInitDevPart2OUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &psFWDataAllocServerExportCookieInt,
											psRGXInitDevPart2IN->hFWDataAllocServerExportCookie,
											PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);
					if(psRGXInitDevPart2OUT->eError != PVRSRV_OK)
					{
						goto RGXInitDevPart2_exit;
					}

				}

				{
					/* Look up the address from the handle */
					psRGXInitDevPart2OUT->eError =
						PVRSRVLookupHandle(psConnection->psHandleBase,
											(IMG_HANDLE *) &psFWCorememAllocServerExportCookieInt,
											psRGXInitDevPart2IN->hFWCorememAllocServerExportCookie,
											PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);
					if(psRGXInitDevPart2OUT->eError != PVRSRV_OK)
					{
						goto RGXInitDevPart2_exit;
					}

				}

	psRGXInitDevPart2OUT->eError =
		PVRSRVRGXInitDevPart2KM(
					hDevNodeInt,
					psInitScriptInt,
					psDbgScriptInt,
					psDbgBusScriptInt,
					psDeinitScriptInt,
					psRGXInitDevPart2IN->ui32ui32KernelCatBaseIdReg,
					psRGXInitDevPart2IN->ui32KernelCatBaseId,
					psRGXInitDevPart2IN->ui32KernelCatBaseReg,
					psRGXInitDevPart2IN->ui32KernelCatBaseWordSize,
					psRGXInitDevPart2IN->ui32KernelCatBaseAlignShift,
					psRGXInitDevPart2IN->ui32KernelCatBaseShift,
					psRGXInitDevPart2IN->ui64KernelCatBaseMask,
					psRGXInitDevPart2IN->ui32DeviceFlags,
					psRGXInitDevPart2IN->ui32RGXActivePMConf,
					psFWCodeAllocServerExportCookieInt,
					psFWDataAllocServerExportCookieInt,
					psFWCorememAllocServerExportCookieInt);
	/* Exit early if bridged call fails */
	if(psRGXInitDevPart2OUT->eError != PVRSRV_OK)
	{
		goto RGXInitDevPart2_exit;
	}

	psRGXInitDevPart2OUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXInitDevPart2IN->hFWCodeAllocServerExportCookie,
					PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);
	psRGXInitDevPart2OUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXInitDevPart2IN->hFWDataAllocServerExportCookie,
					PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);
	psRGXInitDevPart2OUT->eError =
		PVRSRVReleaseHandle(psConnection->psHandleBase,
					(IMG_HANDLE) psRGXInitDevPart2IN->hFWCorememAllocServerExportCookie,
					PVRSRV_HANDLE_TYPE_SERVER_EXPORTCOOKIE);


RGXInitDevPart2_exit:
	if (psInitScriptInt)
		OSFreeMem(psInitScriptInt);
	if (psDbgScriptInt)
		OSFreeMem(psDbgScriptInt);
	if (psDbgBusScriptInt)
		OSFreeMem(psDbgBusScriptInt);
	if (psDeinitScriptInt)
		OSFreeMem(psDeinitScriptInt);

	return 0;
}


#ifdef CONFIG_COMPAT

/* Bridge in structure for RGXInitAllocFWImgMem */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXINITALLOCFWIMGMEM_TAG
{
    /*IMG_HANDLE hDevNode;*/
    IMG_UINT32 hDevNode;
    IMG_DEVMEM_SIZE_T uiFWCodeLen;
    IMG_DEVMEM_SIZE_T uiFWDataLen;
    IMG_DEVMEM_SIZE_T uiFWCoremem;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXINITALLOCFWIMGMEM;


/* Bridge out structure for RGXInitAllocFWImgMem */
typedef struct compat_PVRSRV_BRIDGE_OUT_RGXINITALLOCFWIMGMEM_TAG
{
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWCodeAllocServerExportCookie;*/
	IMG_UINT32 hFWCodeAllocServerExportCookie;
	IMG_DEV_VIRTADDR sFWCodeDevVAddrBase;
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWDataAllocServerExportCookie;*/
	IMG_UINT32 hFWDataAllocServerExportCookie;
	IMG_DEV_VIRTADDR sFWDataDevVAddrBase;
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWCorememAllocServerExportCookie;*/
	IMG_UINT32 hFWCorememAllocServerExportCookie;
	IMG_DEV_VIRTADDR sFWCorememDevVAddrBase;
	RGXFWIF_DEV_VIRTADDR sFWCorememMetaVAddrBase;
	PVRSRV_ERROR eError;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_OUT_RGXINITALLOCFWIMGMEM;


static IMG_INT
compat_PVRSRVBridgeRGXInitAllocFWImgMem(IMG_UINT32 ui32BridgeID,
                     compat_PVRSRV_BRIDGE_IN_RGXINITALLOCFWIMGMEM *psRGXInitAllocFWImgMemIN_32,
                     compat_PVRSRV_BRIDGE_OUT_RGXINITALLOCFWIMGMEM *psRGXInitAllocFWImgMemOUT_32,
                     CONNECTION_DATA *psConnection)
{
	IMG_INT ret;
	PVRSRV_BRIDGE_IN_RGXINITALLOCFWIMGMEM sRGXInitAllocFWImgMemIN;
	PVRSRV_BRIDGE_OUT_RGXINITALLOCFWIMGMEM sRGXInitAllocFWImgMemOUT;

	sRGXInitAllocFWImgMemIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXInitAllocFWImgMemIN_32->hDevNode;
	sRGXInitAllocFWImgMemIN.uiFWCodeLen = psRGXInitAllocFWImgMemIN_32->uiFWCodeLen;
	sRGXInitAllocFWImgMemIN.uiFWDataLen = psRGXInitAllocFWImgMemIN_32->uiFWDataLen;
	sRGXInitAllocFWImgMemIN.uiFWCoremem = psRGXInitAllocFWImgMemIN_32->uiFWCoremem;

	ret = PVRSRVBridgeRGXInitAllocFWImgMem(ui32BridgeID,
							&sRGXInitAllocFWImgMemIN,
							&sRGXInitAllocFWImgMemOUT,
							psConnection);

	PVR_ASSERT(!((IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWCodeAllocServerExportCookie & 0xFFFFFFFF00000000ULL));
	psRGXInitAllocFWImgMemOUT_32->hFWCodeAllocServerExportCookie = (IMG_UINT32)(IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWCodeAllocServerExportCookie;
	psRGXInitAllocFWImgMemOUT_32->sFWCodeDevVAddrBase.uiAddr= sRGXInitAllocFWImgMemOUT.sFWCodeDevVAddrBase.uiAddr;
	PVR_ASSERT(!((IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWDataAllocServerExportCookie & 0xFFFFFFFF00000000ULL));
	psRGXInitAllocFWImgMemOUT_32->hFWDataAllocServerExportCookie = (IMG_UINT32)(IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWDataAllocServerExportCookie;
	psRGXInitAllocFWImgMemOUT_32->sFWDataDevVAddrBase.uiAddr= sRGXInitAllocFWImgMemOUT.sFWDataDevVAddrBase.uiAddr;
	PVR_ASSERT(!((IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWCorememAllocServerExportCookie & 0xFFFFFFFF00000000ULL));
	psRGXInitAllocFWImgMemOUT_32->hFWCorememAllocServerExportCookie = (IMG_UINT32)(IMG_UINT64)sRGXInitAllocFWImgMemOUT.hFWCorememAllocServerExportCookie;
	psRGXInitAllocFWImgMemOUT_32->sFWCorememDevVAddrBase.uiAddr= sRGXInitAllocFWImgMemOUT.sFWCorememDevVAddrBase.uiAddr;
	psRGXInitAllocFWImgMemOUT_32->sFWCorememMetaVAddrBase.ui32Addr= sRGXInitAllocFWImgMemOUT.sFWCorememMetaVAddrBase.ui32Addr;
	psRGXInitAllocFWImgMemOUT_32->eError = sRGXInitAllocFWImgMemOUT.eError;

	return ret;
}

/* Bridge in structure for RGXInitFirmware */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXINITFIRMWARE_TAG
{
	/*IMG_HANDLE hDevNode;*/
	IMG_UINT32 hDevNode;
	IMG_BOOL bEnableSignatureChecks;
	IMG_UINT32 ui32SignatureChecksBufSize;
	IMG_UINT32 ui32HWPerfFWBufSizeKB;
	IMG_UINT64 ui64HWPerfFilter;
	IMG_UINT32 ui32RGXFWAlignChecksSize;
	IMG_UINT32 pui32RGXFWAlignChecks;
	IMG_UINT32 ui32ConfigFlags;
	IMG_UINT32 ui32LogType;
	IMG_UINT32 ui32FilterFlags;
	RGXFWIF_COMPCHECKS_BVNC sClientBVNC __attribute__ ((__packed__));
	IMG_UINT32 ui32APMLatency;
	IMG_UINT32 ui32CoreClockSpeed;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXINITFIRMWARE;


static IMG_INT
compat_PVRSRVBridgeRGXInitFirmware(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXINITFIRMWARE *psRGXInitFirmwareIN_32,
					 PVRSRV_BRIDGE_OUT_RGXINITFIRMWARE *psRGXInitFirmwareOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXINITFIRMWARE sRGXInitFirmwareIN;

	sRGXInitFirmwareIN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXInitFirmwareIN_32->hDevNode;
	sRGXInitFirmwareIN.bEnableSignatureChecks = psRGXInitFirmwareIN_32->bEnableSignatureChecks;
	sRGXInitFirmwareIN.ui32SignatureChecksBufSize =  psRGXInitFirmwareIN_32->ui32SignatureChecksBufSize;
	sRGXInitFirmwareIN.ui32HWPerfFWBufSizeKB = psRGXInitFirmwareIN_32->ui32HWPerfFWBufSizeKB;
	sRGXInitFirmwareIN.ui64HWPerfFilter = psRGXInitFirmwareIN_32->ui64HWPerfFilter;
	sRGXInitFirmwareIN.ui32RGXFWAlignChecksSize = psRGXInitFirmwareIN_32->ui32RGXFWAlignChecksSize;
	sRGXInitFirmwareIN.pui32RGXFWAlignChecks = (IMG_UINT32 *)(IMG_UINT64)psRGXInitFirmwareIN_32->pui32RGXFWAlignChecks;
	sRGXInitFirmwareIN.ui32ConfigFlags = psRGXInitFirmwareIN_32->ui32ConfigFlags;
	sRGXInitFirmwareIN.ui32LogType = psRGXInitFirmwareIN_32->ui32LogType;
	sRGXInitFirmwareIN.ui32FilterFlags = psRGXInitFirmwareIN_32->ui32FilterFlags;
	sRGXInitFirmwareIN.ui32APMLatency = psRGXInitFirmwareIN_32->ui32APMLatency;
	sRGXInitFirmwareIN.ui32CoreClockSpeed = psRGXInitFirmwareIN_32->ui32CoreClockSpeed;
	memcpy((void *)&sRGXInitFirmwareIN.sClientBVNC, (const void *)&psRGXInitFirmwareIN_32->sClientBVNC, sizeof(RGXFWIF_COMPCHECKS_BVNC));

	return PVRSRVBridgeRGXInitFirmware(ui32BridgeID,
					 &sRGXInitFirmwareIN,
					 psRGXInitFirmwareOUT,
					 psConnection);
}


/* Bridge in structure for RGXInitLoadFWImage */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXINITLOADFWIMAGE_TAG
{
/*	IMG_HANDLE hImgDestImport;*/
	IMG_UINT32 hImgDestImport;
/*	IMG_HANDLE hImgSrcImport;*/
	IMG_UINT32 hImgSrcImport;
	IMG_UINT64 ui64ImgLen;
/*	IMG_HANDLE hSigImport;*/
	IMG_UINT32 hSigImport;
	IMG_UINT64 ui64SigLen;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXINITLOADFWIMAGE;

static IMG_INT
compat_PVRSRVBridgeRGXInitLoadFWImage(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXINITLOADFWIMAGE *psRGXInitLoadFWImageIN_32,
					 PVRSRV_BRIDGE_OUT_RGXINITLOADFWIMAGE *psRGXInitLoadFWImageOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXINITLOADFWIMAGE sRGXInitLoadFWImageIN;

	sRGXInitLoadFWImageIN.hImgDestImport = (IMG_HANDLE)(IMG_UINT64)psRGXInitLoadFWImageIN_32->hImgDestImport;
	sRGXInitLoadFWImageIN.hImgSrcImport = (IMG_HANDLE)(IMG_UINT64)psRGXInitLoadFWImageIN_32->hImgSrcImport;
	sRGXInitLoadFWImageIN.ui64ImgLen = psRGXInitLoadFWImageIN_32->ui64ImgLen;
	sRGXInitLoadFWImageIN.ui64SigLen = psRGXInitLoadFWImageIN_32->ui64SigLen;
	sRGXInitLoadFWImageIN.hSigImport = (IMG_HANDLE)(IMG_UINT64)psRGXInitLoadFWImageIN_32->hSigImport;

	return PVRSRVBridgeRGXInitLoadFWImage(ui32BridgeID,
					&sRGXInitLoadFWImageIN,
					psRGXInitLoadFWImageOUT,
					psConnection);
}

/* Bridge in structure for RGXInitDevPart2 */
typedef struct compat_PVRSRV_BRIDGE_IN_RGXINITDEVPART2_TAG
{
	/*IMG_HANDLE hDevNode;
	RGX_INIT_COMMAND * psInitScript;
	RGX_INIT_COMMAND * psDbgScript;
	RGX_INIT_COMMAND * psDbgBusScript;
	RGX_INIT_COMMAND * psDeinitScript;*/
	IMG_UINT32 hDevNode;
	IMG_UINT32 psInitScript;
	IMG_UINT32 psDbgScript;
	IMG_UINT32 psDbgBusScript;
	IMG_UINT32 psDeinitScript;
	IMG_UINT32 ui32ui32KernelCatBaseIdReg;
	IMG_UINT32 ui32KernelCatBaseId;
	IMG_UINT32 ui32KernelCatBaseReg;
	IMG_UINT32 ui32KernelCatBaseWordSize;
	IMG_UINT32 ui32KernelCatBaseAlignShift;
	IMG_UINT32 ui32KernelCatBaseShift;
	IMG_UINT64 ui64KernelCatBaseMask;
	IMG_UINT32 ui32DeviceFlags;
	IMG_UINT32 ui32RGXActivePMConf;
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWCodeAllocServerExportCookie;*/
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWDataAllocServerExportCookie;*/
	/*DEVMEM_SERVER_EXPORTCOOKIE hFWCorememAllocServerExportCookie;*/
	IMG_UINT32 hFWCodeAllocServerExportCookie;
	IMG_UINT32 hFWDataAllocServerExportCookie;
	IMG_UINT32 hFWCorememAllocServerExportCookie;
} __attribute__ ((__packed__)) compat_PVRSRV_BRIDGE_IN_RGXINITDEVPART2;

static IMG_INT
compat_PVRSRVBridgeRGXInitDevPart2(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_RGXINITDEVPART2 *psRGXInitDevPart2IN_32,
					 PVRSRV_BRIDGE_OUT_RGXINITDEVPART2 *psRGXInitDevPart2OUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_RGXINITDEVPART2 sRGXInitDevPart2IN;

	sRGXInitDevPart2IN.hDevNode = (IMG_HANDLE)(IMG_UINT64)psRGXInitDevPart2IN_32->hDevNode;
	sRGXInitDevPart2IN.psInitScript = (RGX_INIT_COMMAND *)(IMG_UINT64)psRGXInitDevPart2IN_32->psInitScript;
	sRGXInitDevPart2IN.psDbgScript =  (RGX_INIT_COMMAND *)(IMG_UINT64)psRGXInitDevPart2IN_32->psDbgScript;
	sRGXInitDevPart2IN.psDbgBusScript = (RGX_INIT_COMMAND *)(IMG_UINT64)psRGXInitDevPart2IN_32->psDbgBusScript;
	sRGXInitDevPart2IN.psDeinitScript = (RGX_INIT_COMMAND *)(IMG_UINT64)psRGXInitDevPart2IN_32->psDeinitScript;
	sRGXInitDevPart2IN.ui32ui32KernelCatBaseIdReg = psRGXInitDevPart2IN_32->ui32ui32KernelCatBaseIdReg;
	sRGXInitDevPart2IN.ui32KernelCatBaseId = psRGXInitDevPart2IN_32->ui32KernelCatBaseId;
	sRGXInitDevPart2IN.ui32KernelCatBaseReg = psRGXInitDevPart2IN_32->ui32KernelCatBaseReg;
	sRGXInitDevPart2IN.ui32KernelCatBaseWordSize = psRGXInitDevPart2IN_32->ui32KernelCatBaseWordSize;
	sRGXInitDevPart2IN.ui32KernelCatBaseAlignShift = psRGXInitDevPart2IN_32->ui32KernelCatBaseAlignShift;
	sRGXInitDevPart2IN.ui32KernelCatBaseShift = psRGXInitDevPart2IN_32->ui32KernelCatBaseShift;
	sRGXInitDevPart2IN.ui64KernelCatBaseMask = psRGXInitDevPart2IN_32->ui64KernelCatBaseMask;
	sRGXInitDevPart2IN.ui32DeviceFlags = psRGXInitDevPart2IN_32->ui32DeviceFlags;
	sRGXInitDevPart2IN.ui32RGXActivePMConf = psRGXInitDevPart2IN_32->ui32RGXActivePMConf;
	sRGXInitDevPart2IN.hFWCodeAllocServerExportCookie = (DEVMEM_SERVER_EXPORTCOOKIE)(IMG_UINT64)psRGXInitDevPart2IN_32->hFWCodeAllocServerExportCookie;
	sRGXInitDevPart2IN.hFWDataAllocServerExportCookie = (DEVMEM_SERVER_EXPORTCOOKIE)(IMG_UINT64)psRGXInitDevPart2IN_32->hFWDataAllocServerExportCookie;
	sRGXInitDevPart2IN.hFWCorememAllocServerExportCookie = (DEVMEM_SERVER_EXPORTCOOKIE)(IMG_UINT64)psRGXInitDevPart2IN_32->hFWCorememAllocServerExportCookie;

	return PVRSRVBridgeRGXInitDevPart2(ui32BridgeID,
					 &sRGXInitDevPart2IN,
					 psRGXInitDevPart2OUT,
					 psConnection);
}

#endif /* CONFIG_COMPAT */

/* ***************************************************************************
 * Server bridge dispatch related glue
 */

PVRSRV_ERROR RegisterRGXINITFunctions(IMG_VOID);
IMG_VOID UnregisterRGXINITFunctions(IMG_VOID);

/*
 * Register all RGXINIT functions with services
 */
PVRSRV_ERROR RegisterRGXINITFunctions(IMG_VOID)
{
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITALLOCFWIMGMEM, compat_PVRSRVBridgeRGXInitAllocFWImgMem);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITFIRMWARE, compat_PVRSRVBridgeRGXInitFirmware);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITLOADFWIMAGE, compat_PVRSRVBridgeRGXInitLoadFWImage);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITDEVPART2, compat_PVRSRVBridgeRGXInitDevPart2);
#else
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITALLOCFWIMGMEM, PVRSRVBridgeRGXInitAllocFWImgMem);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITFIRMWARE, PVRSRVBridgeRGXInitFirmware);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITLOADFWIMAGE, PVRSRVBridgeRGXInitLoadFWImage);
	SetDispatchTableEntry(PVRSRV_BRIDGE_RGXINIT_RGXINITDEVPART2, PVRSRVBridgeRGXInitDevPart2);
#endif
	return PVRSRV_OK;
}

/*
 * Unregister all rgxinit functions with services
 */
IMG_VOID UnregisterRGXINITFunctions(IMG_VOID)
{
}
