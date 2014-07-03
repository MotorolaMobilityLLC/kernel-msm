/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#if !defined(__DC_OSFUNCS_H__)
#define __DC_OSFUNCS_H__

#include "img_types.h"
#include "physheap.h"
#include "kerneldisplay.h"
#include "pvrsrv.h"

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
#include "syscommon.h"
#endif

#if defined(DEBUG)
#define DC_ASSERT(EXPR)							\
	do								\
	{								\
		if (!(EXPR))						\
		{							\
			DC_OSAbort(__FILE__, __LINE__);			\
		}							\
	} while (0)
#else
#define DC_ASSERT(EXPR) (IMG_VOID)(EXPR) /* Null implementation of ASSERT (does nothing) */
#endif

#define DC_ALIGN(value, alignment) (((value) + ((alignment) - 1)) & ~((alignment) - 1))


/*******************************************************************************
 * DC OS functions
 ******************************************************************************/

/* Services display class function pointers */
typedef PVRSRV_ERROR (*PFN_DC_REGISTER_DEVICE)(DC_DEVICE_FUNCTIONS *psFuncTable, IMG_UINT32 ui32MaxConfigsInFlight, IMG_HANDLE hDeviceData, IMG_HANDLE *phSrvHandle);
typedef IMG_VOID (*PFN_DC_UNREGISTER_DEVICE)(IMG_HANDLE hSrvHandle);
typedef IMG_VOID (*PFN_DC_DISPLAY_CONFIGURATION_RETIRED)(IMG_HANDLE hConfigData);
typedef PVRSRV_ERROR (*PFN_DC_IMPORT_BUFFER_ACQUIRE)(IMG_HANDLE hImport, IMG_DEVMEM_LOG2ALIGN_T uiLog2PageSize, IMG_UINT32 *pui32PageCount, IMG_DEV_PHYADDR **ppasDevPAddr);
typedef IMG_VOID (*PFN_DC_IMPORT_BUFFER_RELEASE)(IMG_HANDLE hImport, IMG_DEV_PHYADDR *pasDevPAddr);

/* Services physical heap function pointers */
typedef PVRSRV_ERROR (*PFN_PHYS_HEAP_ACQUIRE)(IMG_UINT32 ui32PhysHeapID, PHYS_HEAP **ppsPhysHeap);
typedef IMG_VOID (*PFN_PHYS_HEAP_RELEASE)(PHYS_HEAP *psPhysHeap);
typedef PHYS_HEAP_TYPE (*PFN_PHYS_HEAP_GET_TYPE)(PHYS_HEAP *psPhysHeap);
typedef PVRSRV_ERROR (*PFN_PHYS_HEAP_GET_SIZE)(PHYS_HEAP *psPhysHeap, IMG_UINT64 *puiSize);
typedef PVRSRV_ERROR (*PFN_PHYS_HEAP_GET_ADDRESS)(PHYS_HEAP *psPhysHeap, IMG_CPU_PHYADDR *psCpuPAddr);
typedef IMG_VOID (*PFN_PHYS_HEAP_CPU_PADDR_TO_DEV_PADDR)(PHYS_HEAP *psPhysHeap, IMG_DEV_PHYADDR *psDevPAddr, IMG_CPU_PHYADDR *psCpuPAddr);

/* Services system functions */
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
typedef PVRSRV_ERROR (*PFN_SYS_INSTALL_DEVICE_LISR)(IMG_UINT32 ui32IRQ, IMG_BOOL bShared, IMG_CHAR *pszName, PFN_LISR pfnLISR, IMG_PVOID pvData, IMG_HANDLE *phLISRData);
typedef PVRSRV_ERROR (*PFN_SYS_UNINSTALL_DEVICE_LISR)(IMG_HANDLE hLISRData);
#endif

/* Other service related functions */
typedef IMG_VOID (*PFN_CHECK_STATUS)(IMG_PVOID hCmdCompCallerHandle);
typedef const IMG_CHAR *(*PFN_GET_ERROR_STRING)(PVRSRV_ERROR eError);

#define DC_OS_BYTES_TO_PAGES(range)	(((range) + (DC_OSGetPageSize() - 1)) >> DC_OSGetPageShift())

typedef struct DC_SERVICES_FUNCS_TAG
{
	/* Display class functions */
	PFN_DC_REGISTER_DEVICE			pfnDCRegisterDevice;
	PFN_DC_UNREGISTER_DEVICE		pfnDCUnregisterDevice;
	PFN_DC_DISPLAY_CONFIGURATION_RETIRED	pfnDCDisplayConfigurationRetired;
	PFN_DC_IMPORT_BUFFER_ACQUIRE		pfnDCImportBufferAcquire;
	PFN_DC_IMPORT_BUFFER_RELEASE		pfnDCImportBufferRelease;

	/* Physical heap functions */
	PFN_PHYS_HEAP_ACQUIRE			pfnPhysHeapAcquire;
	PFN_PHYS_HEAP_RELEASE			pfnPhysHeapRelease;
	PFN_PHYS_HEAP_GET_TYPE			pfnPhysHeapGetType;
	PFN_PHYS_HEAP_GET_SIZE			pfnPhysHeapGetSize;
	PFN_PHYS_HEAP_GET_ADDRESS		pfnPhysHeapGetAddress;
	PFN_PHYS_HEAP_CPU_PADDR_TO_DEV_PADDR	pfnPhysHeapCpuPAddrToDevPAddr;

	/* System functions */
#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	PFN_SYS_INSTALL_DEVICE_LISR		pfnSysInstallDeviceLISR;
	PFN_SYS_UNINSTALL_DEVICE_LISR		pfnSysUninstallDeviceLISR;
#endif
	/* Other service related functions */
	PFN_CHECK_STATUS		        pfnCheckStatus;
	PFN_GET_ERROR_STRING			pfnGetErrorString;
} DC_SERVICES_FUNCS;

typedef enum
{
	DBGLVL_FATAL = 0,
	DBGLVL_ALERT,
	DBGLVL_ERROR,
	DBGLVL_WARNING,
	DBGLVL_NOTICE,
	DBGLVL_INFO,
	DBGLVL_DEBUG,
} DC_OS_DEBUG_LEVEL;

/* Logging & other misc stuff */
IMG_VOID DC_OSSetDrvName(const IMG_CHAR *pszDrvName);
IMG_VOID DC_OSAbort(const IMG_CHAR *pszFile, IMG_UINT32 ui32Line);

IMG_VOID DC_OSDebugPrintf(DC_OS_DEBUG_LEVEL eDebugLevel, const IMG_CHAR *pszFormat, ...);

IMG_CHAR *DC_OSStringNCopy(IMG_CHAR *pszDest, const IMG_CHAR *pszSrc, IMG_SIZE_T uiLength);

IMG_INT64 DC_OSClockns(IMG_VOID);

/* Memory management */
IMG_UINT32 DC_OSGetPageSize(IMG_VOID);
IMG_UINT32 DC_OSGetPageShift(IMG_VOID);
IMG_UINT32 DC_OSGetPageMask(IMG_VOID);

IMG_VOID *DC_OSAllocMem(IMG_SIZE_T uiSize);
IMG_VOID *DC_OSCallocMem(IMG_SIZE_T uiSize);
IMG_VOID DC_OSFreeMem(IMG_VOID *pvMem);
IMG_VOID DC_OSMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_SIZE_T uiSize);

IMG_UINT32 DC_OSAddrRangeStart(IMG_VOID *pvDevice, IMG_UINT8 ui8BaseNum);
IMG_VOID *DC_OSRequestAddrRegion(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size, IMG_CHAR *pszRequestorName);
IMG_VOID DC_OSReleaseAddrRegion(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size);

IMG_CPU_VIRTADDR DC_OSMapPhysAddr(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size);
IMG_VOID DC_OSUnmapPhysAddr(IMG_CPU_VIRTADDR pvCpuVAddr, IMG_UINT32 ui32Size);

/* Register access */
IMG_UINT32 DC_OSReadReg32(IMG_CPU_VIRTADDR pvRegCpuVBase, IMG_UINT32 ui32Offset);
IMG_VOID DC_OSWriteReg32(IMG_CPU_VIRTADDR pvRegCpuVBase, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Value);

/* Floating-point support */
IMG_UINT32 DC_OSDiv64(IMG_UINT64 ui64Numerator, IMG_UINT32 ui32Denominator);
IMG_VOID DC_OSFloatingPointBegin(IMG_VOID);
IMG_VOID DC_OSFloatingPointEnd(IMG_VOID);

/* Inter-thread synchronisation */
PVRSRV_ERROR DC_OSMutexCreate(IMG_VOID **ppvMutex);
IMG_VOID DC_OSMutexDestroy(IMG_VOID *pvMutex);
IMG_VOID DC_OSMutexLock(IMG_VOID *pvMutex);
IMG_VOID DC_OSMutexUnlock(IMG_VOID *pvMutex);

IMG_VOID DC_OSDelayus(IMG_UINT32 ui32Timeus);

/* Services access */
PVRSRV_ERROR DC_OSPVRServicesConnectionOpen(IMG_HANDLE *phPVRServicesConnection);
IMG_VOID DC_OSPVRServicesConnectionClose(IMG_HANDLE hPVRServicesConnection);
PVRSRV_ERROR DC_OSPVRServicesSetupFuncs(IMG_HANDLE hPVRServicesConnection, DC_SERVICES_FUNCS *psServicesFuncs);

/* Workqueue functionality */
/* OS work queue interface function pointer */
typedef PVRSRV_ERROR (*PFN_WORK_PROCESSOR)(IMG_VOID *pvData);

PVRSRV_ERROR DC_OSWorkQueueCreate(IMG_HANDLE *phQueue, IMG_UINT32 ui32Length);
PVRSRV_ERROR DC_OSWorkQueueDestroy(IMG_HANDLE hQueue);
PVRSRV_ERROR DC_OSWorkQueueFlush(IMG_HANDLE hQueue);

PVRSRV_ERROR DC_OSWorkQueueCreateWorkItem(IMG_HANDLE *phWorkItem, PFN_WORK_PROCESSOR pfnProcessor, IMG_VOID *pvProcessorData);

PVRSRV_ERROR DC_OSWorkQueueDestroyWorkItem(IMG_HANDLE phWorkItem);
PVRSRV_ERROR DC_OSWorkQueueAddWorkItem(IMG_HANDLE hQueue, IMG_HANDLE hWorkItem);


#endif /* !defined(__DC_OSFUNCS_H__) */
