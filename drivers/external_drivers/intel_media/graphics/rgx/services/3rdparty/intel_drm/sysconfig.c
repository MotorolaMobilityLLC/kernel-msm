/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
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

#include <drm/drmP.h>
#include "img_types.h"
#include "pwr_mgmt.h"
#include "psb_irq.h"
#include "pvrsrv_device.h"
#include "syscommon.h"
#include "sysconfig.h"
#include "allocmem.h"
#include "pvr_debug.h"
#include "osfunc.h"

#include "pci_support.h"

typedef struct _PLAT_DATA_
{
	IMG_HANDLE	hRGXPCI;

	struct drm_device *psDRMDev;
} PLAT_DATA;

PLAT_DATA *gpsPlatData = IMG_NULL;
extern struct drm_device *gpsPVRDRMDev;

/*
	PCIInitDev
*/
static PVRSRV_ERROR PCIInitDev(PLAT_DATA *psPlatData)
{
	PVRSRV_DEVICE_CONFIG *psDevice = &sSysConfig.pasDevices[0];
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32MaxOffset;
	IMG_UINT32 ui32BaseAddr = 0;

	psPlatData->psDRMDev = gpsPVRDRMDev;
	if (!psPlatData->psDRMDev)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: DRM device not initialized"));
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if (!IS_MRFLD(psPlatData->psDRMDev))
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device 0x%08x not supported", psPlatData->psDRMDev->pci_device));
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	psPlatData->hRGXPCI = OSPCISetDev((IMG_VOID *)psPlatData->psDRMDev->pdev, 0);
	if (!psPlatData->hRGXPCI)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Failed to acquire PCI device"));
		return PVRSRV_ERROR_PCI_DEVICE_NOT_FOUND;
	}

	ui32MaxOffset = OSPCIAddrRangeLen(psPlatData->hRGXPCI, 0);
	if (ui32MaxOffset < (RGX_REG_OFFSET + RGX_REG_SIZE))
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region 0x%08x isn't big enough", ui32MaxOffset));
		return PVRSRV_ERROR_PCI_REGION_TOO_SMALL;
	}
	PVR_DPF((PVR_DBG_WARNING,"PCIInitDev: Device memory region len 0x%08x", ui32MaxOffset));

	/* Reserve the address range */
	if (OSPCIRequestAddrRange(psPlatData->hRGXPCI, 0) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region not available"));
		return PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;

	}

	ui32BaseAddr = OSPCIAddrRangeStart(psPlatData->hRGXPCI, 0);

	if (OSPCIIRQ(psPlatData->hRGXPCI, &psDevice->ui32IRQ) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Couldn't get IRQ"));
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		goto e4;
	}
	PVR_DPF((PVR_DBG_WARNING, "PCIInitDev: BaseAddr 0x%08x, EndAddr 0x%08x, IRQ %d",
			ui32BaseAddr, OSPCIAddrRangeEnd(psPlatData->hRGXPCI, 0), psDevice->ui32IRQ));

	psDevice->sRegsCpuPBase.uiAddr = ui32BaseAddr + RGX_REG_OFFSET;
	psDevice->ui32RegsSize = RGX_REG_SIZE;
	PVR_DPF((PVR_DBG_WARNING, "PCIInitDev: sRegsCpuPBase 0x%x, size 0x%x",
			psDevice->sRegsCpuPBase.uiAddr, psDevice->ui32RegsSize));

	return PVRSRV_OK;

e4:
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, 0);
	OSPCIReleaseDev(psPlatData->hRGXPCI);

	return eError;
}

/*!
******************************************************************************

 @Function		PCIDeInitDev

 @Description

 Uninitialise the PCI device when it is no loger required

 @Input		psSysData :	System data

 @Return	none

******************************************************************************/
static IMG_VOID PCIDeInitDev(PLAT_DATA *psPlatData)
{
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, 0);
	OSPCIReleaseDev(psPlatData->hRGXPCI);
}


PVRSRV_ERROR SysCreateConfigData(PVRSRV_SYSTEM_CONFIG **ppsSysConfig)
{
	PLAT_DATA *psPlatData;
	PVRSRV_ERROR eError;

	psPlatData = OSAllocMem(sizeof(PLAT_DATA));
	OSMemSet(psPlatData, 0, sizeof(PLAT_DATA));

	/* Query the Emu for reg and IRQ information */
	eError = PCIInitDev(psPlatData);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	/* Save data for this device */
	sSysConfig.pasDevices[0].hSysData = (IMG_HANDLE) psPlatData;

	/* Save private data for the physical memory heap */
	gsPhysHeapConfig[0].hPrivData = (IMG_HANDLE) psPlatData;

#if defined(TDMETACODE)
	#error "Not supported services/3rdparty/intel_drm/sysconfig.c"
	gsPhysHeapConfig[1].hPrivData = IMG_NULL;
#endif

	*ppsSysConfig = &sSysConfig;

	gpsPlatData = psPlatData;


	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	#error "Need to check this function call"
	IonInit(NULL);
#endif

	return PVRSRV_OK;
e0:
	return eError;
}

IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PLAT_DATA *psPlatData = gpsPlatData;

	PVR_UNREFERENCED_PARAMETER(psSysConfig);
	PCIDeInitDev(psPlatData);
	OSFreeMem(psPlatData);

#if defined(SUPPORT_ION)
	#error "Need to check this function call"
	IonDeinit(NULL);
#endif
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);



	return PVRSRV_OK;
}

static IMG_VOID SysCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
										IMG_DEV_PHYADDR *psDevPAddr,
										IMG_CPU_PHYADDR *psCpuPAddr)
{
	psDevPAddr->uiAddr = psCpuPAddr->uiAddr;
}

static IMG_VOID SysDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
										IMG_CPU_PHYADDR *psCpuPAddr,
										IMG_DEV_PHYADDR *psDevPAddr)
{
	psCpuPAddr->uiAddr = psDevPAddr->uiAddr;
}

static PVRSRV_ERROR SysDevicePrePowerState(
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState,
		IMG_BOOL bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
		(eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF)) {
		PVR_DPF((PVR_DBG_MESSAGE, "Remove SGX power"));
		if (!power_island_put(OSPM_GRAPHICS_ISLAND))
			return PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR SysDevicePostPowerState(
		PVRSRV_DEV_POWER_STATE eNewPowerState,
		PVRSRV_DEV_POWER_STATE eCurrentPowerState,
		IMG_BOOL bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
		(eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)) {
		PVR_DPF((PVR_DBG_MESSAGE, "Restore SGX power"));
		if (!power_island_get(OSPM_GRAPHICS_ISLAND))
			return PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
	}

	return PVRSRV_OK;
}


PVRSRV_ERROR SysInstallDeviceLISR(IMG_UINT32 ui32IRQ,
				  IMG_BOOL bShared,
				  IMG_CHAR *pszName,
				  PFN_LISR pfnLISR,
				  IMG_PVOID pvData,
				  IMG_HANDLE *phLISRData)
{
	register_rgx_irq_handler(pfnLISR, pvData);
	return PVRSRV_OK;

}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	register_rgx_irq_handler(IMG_NULL, IMG_NULL);
	return PVRSRV_OK;

}


/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
