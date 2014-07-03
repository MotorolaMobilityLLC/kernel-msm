/*************************************************************************/ /*!
@File
@Title          PVR Bridge Module (kernel side)
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Receives calls from the user portion of services and
                despatches them to functions in the kernel portion.
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
#include "img_defs.h"
#include "pvr_bridge.h"
#include "connection_server.h"
#include "mutex.h"
#include "syscommon.h"
#include "pvr_debug.h"
#include "pvr_debugfs.h"
#include "private_data.h"
#include "linkage.h"

#if defined(SUPPORT_DRM)
#include <drm/drmP.h>
#include "pvr_drm.h"
#if defined(PVR_DRM_SECURE_AUTH_EXPORT)
#include "env_connection.h"
#endif
#endif /* defined(SUPPORT_DRM) */

/* RGX: */
#if defined(SUPPORT_RGX)
#include "rgx_bridge.h"
#endif

#include "srvcore.h"
#include "common_srvcore_bridge.h"
#include "cache_defines.h"

#if defined(MODULE_TEST)
/************************************************************************/
// additional includes for services testing
/************************************************************************/
#include "pvr_test_bridge.h"
#include "kern_test.h"
/************************************************************************/
// end of additional includes
/************************************************************************/
#endif


#if defined(SUPPORT_DRM)
#define	PRIVATE_DATA(pFile) ((pFile)->driver_priv)
#else
#define	PRIVATE_DATA(pFile) ((pFile)->private_data)
#endif

#if defined(DEBUG_BRIDGE_KM)
static struct dentry *gpsPVRDebugFSBridgeStatsEntry = NULL;
static struct seq_operations gsBridgeStatsReadOps;
#endif

extern PVRSRV_LINUX_MUTEX gPVRSRVLock;

PVRSRV_ERROR RegisterPDUMPFunctions(IMG_VOID);
#if defined(SUPPORT_DISPLAY_CLASS)
PVRSRV_ERROR RegisterDCFunctions(IMG_VOID);
#endif
PVRSRV_ERROR RegisterMMFunctions(IMG_VOID);
PVRSRV_ERROR RegisterCMMFunctions(IMG_VOID);
PVRSRV_ERROR RegisterPDUMPMMFunctions(IMG_VOID);
PVRSRV_ERROR RegisterPDUMPCMMFunctions(IMG_VOID);
PVRSRV_ERROR RegisterSRVCOREFunctions(IMG_VOID);
PVRSRV_ERROR RegisterSYNCFunctions(IMG_VOID);
#if defined(SUPPORT_INSECURE_EXPORT)
PVRSRV_ERROR RegisterSYNCEXPORTFunctions(IMG_VOID);
#endif
#if defined(SUPPORT_SECURE_EXPORT)
PVRSRV_ERROR RegisterSYNCSEXPORTFunctions(IMG_VOID);
#endif
#if defined (SUPPORT_RGX)
PVRSRV_ERROR RegisterRGXINITFunctions(IMG_VOID);
PVRSRV_ERROR RegisterRGXTA3DFunctions(IMG_VOID);
PVRSRV_ERROR RegisterRGXTQFunctions(IMG_VOID);
PVRSRV_ERROR RegisterRGXCMPFunctions(IMG_VOID);
PVRSRV_ERROR RegisterBREAKPOINTFunctions(IMG_VOID);
PVRSRV_ERROR RegisterDEBUGMISCFunctions(IMG_VOID);
PVRSRV_ERROR RegisterRGXPDUMPFunctions(IMG_VOID);
PVRSRV_ERROR RegisterRGXHWPERFFunctions(IMG_VOID);
#if defined(RGX_FEATURE_RAY_TRACING)
PVRSRV_ERROR RegisterRGXRAYFunctions(IMG_VOID);
#endif /* RGX_FEATURE_RAY_TRACING */
PVRSRV_ERROR RegisterREGCONFIGFunctions(IMG_VOID);
#endif /* SUPPORT_RGX */
#if (CACHEFLUSH_TYPE == CACHEFLUSH_GENERIC)
PVRSRV_ERROR RegisterCACHEGENERICFunctions(IMG_VOID);
#endif
#if defined(SUPPORT_SECURE_EXPORT)
PVRSRV_ERROR RegisterSMMFunctions(IMG_VOID);
#endif
#if defined(SUPPORT_PMMIF)
PVRSRV_ERROR RegisterPMMIFFunctions(IMG_VOID);
#endif
PVRSRV_ERROR RegisterPVRTLFunctions(IMG_VOID);
#if defined(PVR_RI_DEBUG)
PVRSRV_ERROR RegisterRIFunctions(IMG_VOID);
#endif
#if defined(SUPPORT_ION)
PVRSRV_ERROR RegisterIONFunctions(IMG_VOID);
#endif

/* These and their friends above will go when full bridge gen comes in */
PVRSRV_ERROR
LinuxBridgeInit(IMG_VOID);
IMG_VOID
LinuxBridgeDeInit(IMG_VOID);

PVRSRV_ERROR
LinuxBridgeInit(IMG_VOID)
{
	PVRSRV_ERROR eError;
#if defined(DEBUG_BRIDGE_KM)
	IMG_INT iResult;

	iResult = PVRDebugFSCreateEntry("bridge_stats",
					NULL,
					&gsBridgeStatsReadOps,
					NULL,
					&g_BridgeDispatchTable[0],
					&gpsPVRDebugFSBridgeStatsEntry);
	if (iResult != 0)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
#endif

	eError = RegisterSRVCOREFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterSYNCFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_INSECURE_EXPORT)
	eError = RegisterSYNCEXPORTFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif
#if defined(SUPPORT_SECURE_EXPORT)
	eError = RegisterSYNCSEXPORTFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = RegisterPDUMPFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = RegisterMMFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = RegisterCMMFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = RegisterPDUMPMMFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	eError = RegisterPDUMPCMMFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SUPPORT_PMMIF)
	eError = RegisterPMMIFFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_ION)
	eError = RegisterIONFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_DISPLAY_CLASS)
	eError = RegisterDCFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if (CACHEFLUSH_TYPE == CACHEFLUSH_GENERIC)
	eError = RegisterCACHEGENERICFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

#if defined(SUPPORT_SECURE_EXPORT)
	eError = RegisterSMMFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif

	eError = RegisterPVRTLFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	#if defined(PVR_RI_DEBUG)
	eError = RegisterRIFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	#endif

	#if defined (SUPPORT_RGX)
	eError = RegisterRGXTQFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterRGXCMPFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterRGXINITFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterRGXTA3DFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterBREAKPOINTFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterDEBUGMISCFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	
	eError = RegisterRGXPDUMPFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = RegisterRGXHWPERFFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(RGX_FEATURE_RAY_TRACING)
	eError = RegisterRGXRAYFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
#endif /* RGX_FEATURE_RAY_TRACING */

	eError = RegisterREGCONFIGFunctions();
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#endif /* SUPPORT_RGX */

	return eError;
}

IMG_VOID
LinuxBridgeDeInit(IMG_VOID)
{
#if defined(DEBUG_BRIDGE_KM)
	PVRDebugFSRemoveEntry(gpsPVRDebugFSBridgeStatsEntry);
	gpsPVRDebugFSBridgeStatsEntry = NULL;
#endif
}

#if defined(DEBUG_BRIDGE_KM)
static void *BridgeStatsSeqStart(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psDispatchTable = (PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)psSeqFile->private;

	LinuxLockMutex(&gPVRSRVLock);

	if (psDispatchTable == NULL || (*puiPosition) > BRIDGE_DISPATCH_TABLE_ENTRY_COUNT)
	{
		return NULL;
	}

	if ((*puiPosition) == 0) 
	{
		return SEQ_START_TOKEN;
	}

	return &(psDispatchTable[(*puiPosition) - 1]);
}

static void BridgeStatsSeqStop(struct seq_file *psSeqFile, void *pvData)
{
	PVR_UNREFERENCED_PARAMETER(psSeqFile);
	PVR_UNREFERENCED_PARAMETER(pvData);

	LinuxUnLockMutex(&gPVRSRVLock);
}

static void *BridgeStatsSeqNext(struct seq_file *psSeqFile,
			       void *pvData,
			       loff_t *puiPosition)
{
	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psDispatchTable = (PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)psSeqFile->private;

	PVR_UNREFERENCED_PARAMETER(pvData);

	(*puiPosition)++;

	if ((*puiPosition) > BRIDGE_DISPATCH_TABLE_ENTRY_COUNT)
	{
		return NULL;
	}

	return &(psDispatchTable[(*puiPosition) - 1]);
}

static int BridgeStatsSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	if (pvData == SEQ_START_TOKEN)
	{
		seq_printf(psSeqFile,
			   "Total ioctl call count = %u\n"
			   "Total number of bytes copied via copy_from_user = %u\n"
			   "Total number of bytes copied via copy_to_user = %u\n"
			   "Total number of bytes copied via copy_*_user = %u\n\n"
			   "%-60s | %-48s | %10s | %20s | %10s\n",
			   g_BridgeGlobalStats.ui32IOCTLCount,
			   g_BridgeGlobalStats.ui32TotalCopyFromUserBytes,
			   g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
			   g_BridgeGlobalStats.ui32TotalCopyFromUserBytes + g_BridgeGlobalStats.ui32TotalCopyToUserBytes,
			   "Bridge Name",
			   "Wrapper Function",
			   "Call Count",
			   "copy_from_user Bytes",
			   "copy_to_user Bytes");
	}
	else if (pvData != NULL)
	{
		PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *psEntry = (	PVRSRV_BRIDGE_DISPATCH_TABLE_ENTRY *)pvData;

		seq_printf(psSeqFile,
			   "%-60s   %-48s   %-10u   %-20u   %-10u\n",
			   psEntry->pszIOCName,
			   psEntry->pszFunctionName,
			   psEntry->ui32CallCount,
			   psEntry->ui32CopyFromUserTotalBytes,
			   psEntry->ui32CopyToUserTotalBytes);
	}

	return 0;
}

static struct seq_operations gsBridgeStatsReadOps =
{
	.start = BridgeStatsSeqStart,
	.stop = BridgeStatsSeqStop,
	.next = BridgeStatsSeqNext,
	.show = BridgeStatsSeqShow,
};
#endif /* defined(DEBUG_BRIDGE_KM) */


#if defined(SUPPORT_DRM)
int
PVRSRV_BridgeDispatchKM(struct drm_device *dev, void *arg, struct drm_file *pFile)
#else
long
PVRSRV_BridgeDispatchKM(struct file *pFile, unsigned int unref__ ioctlCmd, unsigned long arg)
#endif
{
#if !defined(SUPPORT_DRM)
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageUM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVRSRV_BRIDGE_PACKAGE sBridgePackageKM;
#endif
	PVRSRV_BRIDGE_PACKAGE *psBridgePackageKM;
	CONNECTION_DATA *psConnection = LinuxConnectionFromFile(pFile);
	IMG_INT err = -EFAULT;

	LinuxLockMutex(&gPVRSRVLock);

#if defined(SUPPORT_DRM)
	PVR_UNREFERENCED_PARAMETER(dev);

	psBridgePackageKM = (PVRSRV_BRIDGE_PACKAGE *)arg;
	PVR_ASSERT(psBridgePackageKM != IMG_NULL);
#else
	PVR_UNREFERENCED_PARAMETER(ioctlCmd);

	psBridgePackageKM = &sBridgePackageKM;

	if(!OSAccessOK(PVR_VERIFY_WRITE,
				   psBridgePackageUM,
				   sizeof(PVRSRV_BRIDGE_PACKAGE)))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Received invalid pointer to function arguments",
				 __FUNCTION__));

		goto unlock_and_return;
	}
	
	/* FIXME - Currently the CopyFromUserWrapper which collects stats about
	 * how much data is shifted to/from userspace isn't available to us
	 * here. */
	if(OSCopyFromUser(IMG_NULL,
					  psBridgePackageKM,
					  psBridgePackageUM,
					  sizeof(PVRSRV_BRIDGE_PACKAGE))
	  != PVRSRV_OK)
	{
		goto unlock_and_return;
	}
#endif

#if defined(DEBUG_BRIDGE_CALLS)
	{
		IMG_UINT32 mangledID;
		mangledID = psBridgePackageKM->ui32BridgeID;

		psBridgePackageKM->ui32BridgeID = PVRSRV_GET_BRIDGE_ID(psBridgePackageKM->ui32BridgeID);

		PVR_DPF((PVR_DBG_WARNING, "Bridge ID (x%8x) %8u (mangled: x%8x) ", psBridgePackageKM->ui32BridgeID, psBridgePackageKM->ui32BridgeID, mangledID));
	}
#else
		psBridgePackageKM->ui32BridgeID = PVRSRV_GET_BRIDGE_ID(psBridgePackageKM->ui32BridgeID);
#endif

	err = BridgedDispatchKM(psConnection, psBridgePackageKM);

#if !defined(SUPPORT_DRM)
unlock_and_return:
#endif
	LinuxUnLockMutex(&gPVRSRVLock);
	return err;
}
