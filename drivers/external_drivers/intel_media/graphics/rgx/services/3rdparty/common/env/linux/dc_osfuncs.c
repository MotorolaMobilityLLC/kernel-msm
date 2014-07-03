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

#if defined(LINUX)

#include <linux/version.h>
#include <linux/bug.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <asm/page.h>

#if defined(__i386__) || defined(__x86_64__)
#include <asm/i387.h>
#endif

#include "srvkm.h"
#include "dc_osfuncs.h"

#define MAX_DRVNAME 128
static IMG_CHAR g_szDrvName[MAX_DRVNAME + 1];

typedef struct WORK_ITEM_TAG
{
	struct work_struct	sWork;
	PFN_WORK_PROCESSOR	pfnProcessor;
	IMG_VOID		*pvData;
} WORK_ITEM;

IMG_VOID DC_OSSetDrvName(const IMG_CHAR *pszDrvName)
{
	strncpy(g_szDrvName, pszDrvName, MAX_DRVNAME);
	g_szDrvName[MAX_DRVNAME] = '\0'; /* ensure null term (not guaranteed by strncpy) */
}

IMG_VOID DC_OSAbort(const IMG_CHAR *pszFile, IMG_UINT32 ui32Line)
{
	printk(KERN_EMERG "%s - Assertion failed at %s:%d\n", g_szDrvName, pszFile, ui32Line);
	BUG();
}

IMG_VOID DC_OSDebugPrintf(DC_OS_DEBUG_LEVEL eDebugLevel, const IMG_CHAR *pszFormat, ...)
{
	va_list args;

	switch (eDebugLevel)
	{
		case DBGLVL_FATAL:
		{
			printk(KERN_EMERG " %s", g_szDrvName);
			break;
		}
		case DBGLVL_ALERT:
		{
			printk(KERN_ALERT " %s", g_szDrvName);
			break;
		}
		case DBGLVL_ERROR:
		{
			printk(KERN_ERR " %s", g_szDrvName);
			break;
		}
		case DBGLVL_WARNING:
		{
			printk(KERN_WARNING " %s", g_szDrvName);
			break;
		}
		case DBGLVL_NOTICE:
		{
			printk(KERN_NOTICE " %s", g_szDrvName);
			break;
		}
		case DBGLVL_INFO:
		{
			printk(KERN_INFO " %s", g_szDrvName);
			break;
		}
		case DBGLVL_DEBUG:
		{
			printk(KERN_DEBUG " %s", g_szDrvName);
			break;
		}
	}

	va_start(args, pszFormat);
	vprintk(pszFormat, args);
	va_end(args);
}

IMG_CHAR *DC_OSStringNCopy(IMG_CHAR *pszDest, const IMG_CHAR *pszSrc, IMG_SIZE_T uiLength)
{
	return strncpy(pszDest, pszSrc, uiLength);
}

IMG_INT64 DC_OSClockns(IMG_VOID)
{
	struct timespec now;
	ktime_get_ts(&now);
	return now.tv_sec * 1000000000ULL + now.tv_nsec;
}

IMG_UINT32 DC_OSGetPageSize(IMG_VOID)
{
	return PAGE_SIZE;
}

IMG_UINT32 DC_OSGetPageShift(IMG_VOID)
{
	return PAGE_SHIFT;
}

IMG_UINT32 DC_OSGetPageMask(IMG_VOID)
{
	return (IMG_UINT32)PAGE_MASK;
}

IMG_VOID *DC_OSAllocMem(IMG_SIZE_T uiSize)
{
	return kmalloc(uiSize, GFP_KERNEL);
}

IMG_VOID *DC_OSCallocMem(IMG_SIZE_T uiSize)
{
	return kzalloc(uiSize, GFP_KERNEL);
}

IMG_VOID DC_OSFreeMem(IMG_VOID *pvMem)
{
	kfree(pvMem);
}

IMG_VOID DC_OSMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_SIZE_T uiSize)
{
	memset(pvDest, ui8Value, uiSize);
}

IMG_UINT32 DC_OSAddrRangeStart(IMG_VOID *pvDevice, IMG_UINT8 ui8BaseNum)
{
	return pci_resource_start((struct pci_dev *)pvDevice, ui8BaseNum);
}

IMG_VOID *DC_OSRequestAddrRegion(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size, IMG_CHAR *pszRequestorName)
{
	return request_mem_region(sCpuPAddr.uiAddr, ui32Size, pszRequestorName);
}

IMG_VOID DC_OSReleaseAddrRegion(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size)
{
	release_mem_region(sCpuPAddr.uiAddr, ui32Size);
}

IMG_CPU_VIRTADDR DC_OSMapPhysAddr(IMG_CPU_PHYADDR sCpuPAddr, IMG_UINT32 ui32Size)
{
	return ioremap_nocache(sCpuPAddr.uiAddr, ui32Size);
}

IMG_VOID DC_OSUnmapPhysAddr(IMG_CPU_VIRTADDR pvCpuVAddr, IMG_UINT32 ui32Size)
{
	PVR_UNREFERENCED_PARAMETER(ui32Size);
	iounmap(pvCpuVAddr);
}

IMG_UINT32 DC_OSReadReg32(IMG_CPU_VIRTADDR pvRegCpuVBase, IMG_UINT32 ui32Offset)
{
	return ioread32(pvRegCpuVBase + ui32Offset);
}

IMG_VOID DC_OSWriteReg32(IMG_CPU_VIRTADDR pvRegCpuVBase, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Value)
{
	iowrite32(ui32Value, (pvRegCpuVBase + ui32Offset));
}

IMG_UINT32 DC_OSDiv64(IMG_UINT64 ui64Numerator, IMG_UINT32 ui32Denominator)
{
	(void)do_div(ui64Numerator, ui32Denominator);

	return (IMG_UINT32)ui64Numerator;
}

IMG_VOID DC_OSFloatingPointBegin(IMG_VOID)
{
#if defined(__i386__) || defined(__x86_64__)
	kernel_fpu_begin();
#else
	DC_OSDebugPrintf(DBGLVL_WARNING, " - %s: Unsupported architecture\n", __FUNCTION__);
#endif
}

IMG_VOID DC_OSFloatingPointEnd(IMG_VOID)
{
#if defined(__i386__) || defined(__x86_64__)
	kernel_fpu_end();
#else
	DC_OSDebugPrintf(DBGLVL_WARNING, " - %s: Unsupported architecture\n", __FUNCTION__);
#endif
}

PVRSRV_ERROR DC_OSMutexCreate(IMG_VOID **ppvMutex)
{
	struct mutex *psLock;

	psLock = DC_OSAllocMem(sizeof *psLock);
	if (psLock == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}
	mutex_init(psLock);

	*ppvMutex = (IMG_VOID *)psLock;

	return PVRSRV_OK;
}

IMG_VOID DC_OSMutexDestroy(IMG_VOID *pvMutex)
{
	DC_OSFreeMem(pvMutex);
}

IMG_VOID DC_OSMutexLock(IMG_VOID *pvMutex)
{
	mutex_lock((struct mutex *)pvMutex);
}

IMG_VOID DC_OSMutexUnlock(IMG_VOID *pvMutex)
{
	mutex_unlock((struct mutex *)pvMutex);
}

IMG_VOID DC_OSDelayus(IMG_UINT32 ui32Timeus)
{
	udelay(ui32Timeus);
}

PVRSRV_ERROR DC_OSPVRServicesConnectionOpen(IMG_HANDLE *phPVRServicesConnection)
{
	*phPVRServicesConnection = (IMG_HANDLE)IMG_NULL;

	return PVRSRV_OK;
}

IMG_VOID DC_OSPVRServicesConnectionClose(IMG_HANDLE hPVRServicesConnection)
{
	PVR_UNREFERENCED_PARAMETER(hPVRServicesConnection);

	DC_ASSERT(hPVRServicesConnection == (IMG_HANDLE)IMG_NULL);
}

PVRSRV_ERROR DC_OSPVRServicesSetupFuncs(IMG_HANDLE hPVRServicesConnection, DC_SERVICES_FUNCS *psServicesFuncs)
{
	DC_ASSERT(hPVRServicesConnection == (IMG_HANDLE)IMG_NULL);

	if (psServicesFuncs == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psServicesFuncs->pfnDCRegisterDevice = DCRegisterDevice;
	psServicesFuncs->pfnDCUnregisterDevice = DCUnregisterDevice;
	psServicesFuncs->pfnDCDisplayConfigurationRetired = DCDisplayConfigurationRetired;
	psServicesFuncs->pfnDCImportBufferAcquire = DCImportBufferAcquire;
	psServicesFuncs->pfnDCImportBufferRelease = DCImportBufferRelease;

	psServicesFuncs->pfnPhysHeapAcquire = PhysHeapAcquire;
	psServicesFuncs->pfnPhysHeapRelease = PhysHeapRelease;
	psServicesFuncs->pfnPhysHeapGetType = PhysHeapGetType;
	psServicesFuncs->pfnPhysHeapGetSize = PhysHeapGetSize;
	psServicesFuncs->pfnPhysHeapGetAddress = PhysHeapGetAddress;
	psServicesFuncs->pfnPhysHeapCpuPAddrToDevPAddr = PhysHeapCpuPAddrToDevPAddr;

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
	psServicesFuncs->pfnSysInstallDeviceLISR = SysInstallDeviceLISR;
	psServicesFuncs->pfnSysUninstallDeviceLISR = SysUninstallDeviceLISR;
#endif

	psServicesFuncs->pfnCheckStatus = PVRSRVCheckStatus;
	psServicesFuncs->pfnGetErrorString = PVRSRVGetErrorStringKM;

	return PVRSRV_OK;
}

PVRSRV_ERROR DC_OSWorkQueueCreate(IMG_HANDLE *phQueue, IMG_UINT32 ui32Length)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	struct workqueue_struct *psQueue;

	PVR_UNREFERENCED_PARAMETER(ui32Length);

	if (phQueue == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psQueue = alloc_ordered_workqueue(g_szDrvName, WQ_FREEZABLE | WQ_MEM_RECLAIM);
	if (psQueue == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	*phQueue = (IMG_HANDLE)psQueue;

	return PVRSRV_OK;
#else
	PVR_UNREFERENCED_PARAMETER(phQueue);
	PVR_UNREFERENCED_PARAMETER(ui32Length);

	return PVRSRV_ERROR_NOT_SUPPORTED;
#endif
}

PVRSRV_ERROR DC_OSWorkQueueDestroy(IMG_HANDLE hQueue)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	struct workqueue_struct *psQueue = (struct workqueue_struct *)hQueue;

	DC_ASSERT(psQueue);

	destroy_workqueue(psQueue);

	return PVRSRV_OK;
#else
	PVR_UNREFERENCED_PARAMETER(hQueue);

	return PVRSRV_ERROR_NOT_SUPPORTED;
#endif
}

PVRSRV_ERROR DC_OSWorkQueueFlush(IMG_HANDLE hQueue)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	struct workqueue_struct *psQueue = (struct workqueue_struct *)hQueue;

	DC_ASSERT(psQueue);

	flush_workqueue(psQueue);

	return PVRSRV_OK;
#else
	PVR_UNREFERENCED_PARAMETER(hQueue);

	return PVRSRV_ERROR_NOT_SUPPORTED;
#endif
}

static void WorkQueueProcessorWrapper(struct work_struct *psWork)
{
	WORK_ITEM *psWorkItem = container_of(psWork, WORK_ITEM, sWork);

	DC_ASSERT(psWorkItem && psWorkItem->pfnProcessor);

	(IMG_VOID)psWorkItem->pfnProcessor(psWorkItem->pvData);
}

PVRSRV_ERROR DC_OSWorkQueueCreateWorkItem(IMG_HANDLE *phWorkItem, PFN_WORK_PROCESSOR pfnProcessor, IMG_VOID *pvProcessorData)
{
	WORK_ITEM *psWorkItem;

	if (phWorkItem == IMG_NULL || pfnProcessor == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psWorkItem = DC_OSAllocMem(sizeof *psWorkItem);
	if (psWorkItem == IMG_NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	INIT_WORK(&psWorkItem->sWork, WorkQueueProcessorWrapper);

	psWorkItem->pfnProcessor = pfnProcessor;
	psWorkItem->pvData = pvProcessorData;

	*phWorkItem = (IMG_HANDLE)psWorkItem;

	return PVRSRV_OK;
}

PVRSRV_ERROR DC_OSWorkQueueDestroyWorkItem(IMG_HANDLE phWorkItem)
{
	WORK_ITEM *psWorkItem = (WORK_ITEM *)phWorkItem;

	DC_ASSERT(psWorkItem);

	DC_OSFreeMem(psWorkItem);

	return PVRSRV_OK;
}

PVRSRV_ERROR DC_OSWorkQueueAddWorkItem(IMG_HANDLE hQueue, IMG_HANDLE hWorkItem)
{
	struct workqueue_struct *psQueue = (struct workqueue_struct *)hQueue;
	WORK_ITEM *psWorkItem = (WORK_ITEM *)hWorkItem;

	DC_ASSERT(psQueue && psWorkItem);

	if (!queue_work(psQueue, &psWorkItem->sWork))
	{
		printk(KERN_WARNING " %s - %s: Cannot queue work that's already queued\n",
		       g_szDrvName, __FUNCTION__);
		return PVRSRV_ERROR_UNABLE_TO_SCHEDULE_TASK;
	}

	return PVRSRV_OK;
}

#endif /* defined(LINUX) */

