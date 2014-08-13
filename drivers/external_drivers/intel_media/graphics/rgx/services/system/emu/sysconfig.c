/*************************************************************************/ /*!
@File
@Title          Sysconfig layer for Emu
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the system layer for the emulator
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

#include "img_types.h"
#include "pvrsrv_device.h"
#include "sysinfo.h"
#include "syscommon.h"
#include "sysconfig.h"
#include "emu.h"
#include "allocmem.h"
#include "pvr_debug.h"
#include "osfunc.h"

#include "pci_support.h"

#include "emu_cr_defs.h"

#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif

/*!
 ******************************************************************************
 * System emulator parameters
 *****************************************************************************/
static IMG_UINT32 gui32SysEmuMemLatency = 0;

#if defined (LINUX)
#include <linux/module.h>
module_param_named(sys_emu_mem_latency, gui32SysEmuMemLatency, uint, S_IRUGO | S_IWUSR);
#endif

static IMG_UINT32 EmuMemLatencyGet(IMG_VOID)
{
	IMG_UINT32 ui32Latency;

	/* obtain the sys emu mem latency */
	ui32Latency = gui32SysEmuMemLatency;

	/* return it and make sure it does not overflow */
	return (ui32Latency & EMU_CR_MEMORY_LATENCY_MASKFULL);
}

typedef struct _PLAT_DATA_
{
	IMG_UINT32	uiRefCount;
	IMG_HANDLE	hRGXPCI;
	IMG_UINT32	uiICRCpuPAddr;
	IMG_UINT32	uiICRSize;
#if defined (LMA)
	IMG_UINT64	uiLMACpuPAddr;
	IMG_UINT64	uiLMASize;
#endif
} PLAT_DATA;

PLAT_DATA *gpsPlatData = IMG_NULL;


/*
	EmuReset
*/
static PVRSRV_ERROR EmuReset(IMG_CPU_PHYADDR	sRegsCpuPBase)
{
	IMG_CPU_PHYADDR	sWrapperRegsCpuPBase;
	IMG_VOID		*pvWrapperRegs;
	IMG_UINT32		ui32MemLatency;
	
	sWrapperRegsCpuPBase.uiAddr = sRegsCpuPBase.uiAddr + EMULATOR_RGX_REG_WRAPPER_OFFSET;
	
	/*
		Create a temporary mapping of the wrapper registers in order to reset
		the emulator design.
	*/
	pvWrapperRegs = OSMapPhysToLin(sWrapperRegsCpuPBase,
								   EMULATOR_RGX_REG_WRAPPER_SIZE,
								   0);
	if (pvWrapperRegs == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"EmuReset: Failed to create wrapper register mapping\n"));
		return PVRSRV_ERROR_BAD_MAPPING;
	}

	/*
		Set the memory latency. This needs to be done before the soft reset to ensure
		it applies to all aspects of the emulator.
	*/
	ui32MemLatency = EmuMemLatencyGet();
	if (ui32MemLatency != 0)
	{
		PVR_LOG(("EmuReset: Mem latency = 0x%X", ui32MemLatency));
	}
	OSWriteHWReg32(pvWrapperRegs, EMU_CR_MEMORY_LATENCY, ui32MemLatency);
	(void) OSReadHWReg32(pvWrapperRegs, EMU_CR_MEMORY_LATENCY);

	/*
		Emu reset.
	*/
	OSWriteHWReg32(pvWrapperRegs, EMU_CR_SOFT_RESET, EMU_CR_SOFT_RESET_SYS_EN|EMU_CR_SOFT_RESET_MEM_EN|EMU_CR_SOFT_RESET_CORE_EN);
	/* Flush register write */
	(void) OSReadHWReg32(pvWrapperRegs, EMU_CR_SOFT_RESET);
	OSWaitus(10);

	OSWriteHWReg32(pvWrapperRegs, EMU_CR_SOFT_RESET, 0x0);
	/* Flush register write */
	(void) OSReadHWReg32(pvWrapperRegs, EMU_CR_SOFT_RESET);
	OSWaitus(10);

#if !defined(LMA)
	/* If we're UMA then enable bus mastering */
	OSWriteHWReg32(pvWrapperRegs, EMU_CR_PCI_MASTER, EMU_CR_PCI_MASTER_MODE_EN);
#else
	/* otherwise disable it: the emu regbank is not resetable */
	OSWriteHWReg32(pvWrapperRegs, EMU_CR_PCI_MASTER, 0x0);
#endif

	/* Flush register write */
	(void) OSReadHWReg32(pvWrapperRegs, EMU_CR_PCI_MASTER);

	/*
		Remove the temporary register mapping.
	*/
	OSUnMapPhysToLin(pvWrapperRegs, EMULATOR_RGX_REG_WRAPPER_SIZE, 0);

	return PVRSRV_OK;
}

static IMG_VOID Init_IRQ_CTRL(PLAT_DATA *psPlatData)
{
	IMG_CPU_PHYADDR	sICRCpuPBase;
	IMG_VOID *pvICRCpuVBase;
	IMG_UINT32 ui32Value;

	/* Map into CPU address space */
	sICRCpuPBase.uiAddr = IMG_CAST_TO_CPUPHYADDR_UINT(psPlatData->uiICRCpuPAddr);
	pvICRCpuVBase = OSMapPhysToLin(sICRCpuPBase,
							psPlatData->uiICRSize,
							0);

	/* Configure IRQ_CTRL: For Rogue, enable IRQ, set sense to active low */
	ui32Value = OSReadHWReg32(pvICRCpuVBase, EMULATOR_RGX_ICR_REG_IRQ_CTRL);
	ui32Value &= ~EMULATOR_RGX_ICR_REG_IRQ_CTRL_IRQ_HINLO;
	ui32Value |= EMULATOR_RGX_ICR_REG_IRQ_CTRL_IRQ_EN;
	OSWriteHWReg32(pvICRCpuVBase,EMULATOR_RGX_ICR_REG_IRQ_CTRL,	ui32Value);

	/* Flush register write */
	(void) OSReadHWReg32(pvICRCpuVBase, EMULATOR_RGX_ICR_REG_IRQ_CTRL);
	OSWaitus(10);

	PVR_TRACE(("Emulator FPGA image version (ICR_REG_CORE_REVISION): 0x%08x",
			  OSReadHWReg32(pvICRCpuVBase, EMULATOR_RGX_ICR_REG_CORE_REVISION)));

	/* Unmap from CPU address space */
	OSUnMapPhysToLin(pvICRCpuVBase, psPlatData->uiICRSize, 0);
}

/*
	PCIInitDev
*/
static PVRSRV_ERROR PCIInitDev(PLAT_DATA *psPlatData)
{
	PVRSRV_DEVICE_CONFIG *psDevice = &sSysConfig.pasDevices[0];
	PVRSRV_ERROR eError;
#if defined (LMA)
	IMG_UINT32 uiLMACpuPAddr;
	IMG_UINT32 uiLMASize;
#endif

	psPlatData->hRGXPCI = OSPCIAcquireDev(SYS_RGX_DEV_VENDOR_ID,
										  SYS_RGX_DEV_DEVICE_ID,
										  HOST_PCI_INIT_FLAG_BUS_MASTER | HOST_PCI_INIT_FLAG_MSI);

	if (!psPlatData->hRGXPCI)
	{
		psPlatData->hRGXPCI = OSPCIAcquireDev(SYS_RGX_DEV_VENDOR_ID,
											  SYS_RGX_DEV1_DEVICE_ID,
											  HOST_PCI_INIT_FLAG_BUS_MASTER | HOST_PCI_INIT_FLAG_MSI);
	}

	if (!psPlatData->hRGXPCI)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Failed to acquire PCI device"));
		eError =  PVRSRV_ERROR_PCI_DEVICE_NOT_FOUND;
		goto e0;
	}

	psPlatData->uiICRCpuPAddr = OSPCIAddrRangeStart(psPlatData->hRGXPCI, EMULATOR_RGX_ICR_PCI_BASENUM);
	psPlatData->uiICRSize = OSPCIAddrRangeLen(psPlatData->hRGXPCI, EMULATOR_RGX_ICR_PCI_BASENUM);

	/* Check the address range is large enough. */
	if (psPlatData->uiICRSize < EMULATOR_RGX_ICR_PCI_REGION_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region isn't big enough (Emu reg) (was 0x%08x, required 0x%08x)",
									psPlatData->uiICRSize, EMULATOR_RGX_ICR_PCI_REGION_SIZE));
		eError =  PVRSRV_ERROR_PCI_REGION_TOO_SMALL;
		goto e1;
	}

	/* Reserve the address range */
	if (OSPCIRequestAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_ICR_PCI_BASENUM)
								!= PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region not available (Emu reg)"));
		eError = PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;
		goto e1;
	}

	psDevice->sRegsCpuPBase.uiAddr = OSPCIAddrRangeStart(psPlatData->hRGXPCI, EMULATOR_RGX_REG_PCI_BASENUM);
	psDevice->ui32RegsSize = OSPCIAddrRangeLen(psPlatData->hRGXPCI, EMULATOR_RGX_REG_PCI_BASENUM);

	/* Check the address range is large enough. */
	if (psDevice->ui32RegsSize < EMULATOR_RGX_REG_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region isn't big enough (RGX reg) (was 0x%08x, required 0x%08x)",
									psDevice->ui32RegsSize, EMULATOR_RGX_REG_SIZE));
		eError = PVRSRV_ERROR_PCI_REGION_TOO_SMALL;
		goto e2;
	}

	/* Reserve the address range */
	if (OSPCIRequestAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_REG_PCI_BASENUM) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region not available (RGX reg) "));
		eError = PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;
		goto e2;
	}

#if defined (LMA)
	uiLMACpuPAddr = OSPCIAddrRangeStart(psPlatData->hRGXPCI, EMULATOR_RGX_MEM_PCI_BASENUM);
	uiLMASize = OSPCIAddrRangeLen(psPlatData->hRGXPCI, EMULATOR_RGX_MEM_PCI_BASENUM);

	/* Setup the RGX heap */
	gsPhysHeapConfig[0].sStartAddr.uiAddr = uiLMACpuPAddr;
	gsPhysHeapConfig[0].uiSize = uiLMASize - RESERVE_DC_MEM_SIZE;

	/* Setup the DC heap */
	gsPhysHeapConfig[1].sStartAddr.uiAddr = uiLMACpuPAddr + gsPhysHeapConfig[0].uiSize;
	gsPhysHeapConfig[1].uiSize = RESERVE_DC_MEM_SIZE;

	/* Check the address range is large enough. */
	if (uiLMASize < EMULATOR_RGX_MEM_REGION_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region isn't big enough (was 0x%08x, required 0x%08x)",
									uiLMASize, EMULATOR_RGX_MEM_REGION_SIZE));
		eError = PVRSRV_ERROR_PCI_REGION_TOO_SMALL;
		goto e3;
	}

	/* Reserve the address range */
	if (OSPCIRequestAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_MEM_PCI_BASENUM) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Device memory region not available"));
		eError = PVRSRV_ERROR_PCI_REGION_UNAVAILABLE;
		goto e3;
	}
#endif

	if (OSPCIIRQ(psPlatData->hRGXPCI, &psDevice->ui32IRQ) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Couldn't get IRQ"));
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		goto e4;
	}

#if defined (LMA)
	/* Save this info for later use */
	psPlatData->uiLMACpuPAddr = uiLMACpuPAddr;
	psPlatData->uiLMASize = uiLMASize;
#endif

	/*
		Reset the emulator design
	*/
	eError = EmuReset(psDevice->sRegsCpuPBase);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"PCIInitDev: Couldn't reset emulator"));
		goto e4;
	}	

	/*
	 	 Init IRC IRQ_CTRL register
	 */
	Init_IRQ_CTRL(psPlatData);


	return PVRSRV_OK;

e4:
#if defined (LMA)
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_MEM_PCI_BASENUM);
e3:
#endif
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_REG_PCI_BASENUM);
e2:
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_ICR_PCI_BASENUM);
e1:
	OSPCIReleaseDev(psPlatData->hRGXPCI);
e0:
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
#if defined (LMA)
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_MEM_PCI_BASENUM);
#endif
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_REG_PCI_BASENUM);
	OSPCIReleaseAddrRange(psPlatData->hRGXPCI, EMULATOR_RGX_ICR_PCI_BASENUM);
	OSPCIReleaseDev(psPlatData->hRGXPCI);
}

static IMG_VOID EmuCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
										IMG_DEV_PHYADDR *psDevPAddr,
										IMG_CPU_PHYADDR *psCpuPAddr)
{
	PLAT_DATA *psPlatData = (PLAT_DATA *) hPrivData;

#if defined(LMA)
	psDevPAddr->uiAddr = psCpuPAddr->uiAddr - psPlatData->uiLMACpuPAddr;
#else
	PVR_UNREFERENCED_PARAMETER(psPlatData);
	psDevPAddr->uiAddr = psCpuPAddr->uiAddr;
#endif
}

static IMG_VOID EmuDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
										IMG_CPU_PHYADDR *psCpuPAddr,
										IMG_DEV_PHYADDR *psDevPAddr)
{
	PLAT_DATA *psPlatData = (PLAT_DATA *) hPrivData;

#if defined(LMA)
	psCpuPAddr->uiAddr = psDevPAddr->uiAddr + psPlatData->uiLMACpuPAddr;
#else
	PVR_UNREFERENCED_PARAMETER(psPlatData);
	psCpuPAddr->uiAddr = psDevPAddr->uiAddr;
#endif
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

#if defined(LMA)
	/* Save private data for the physical memory heaps */
	gsPhysHeapConfig[0].hPrivData = (IMG_HANDLE) psPlatData;
	gsPhysHeapConfig[1].hPrivData = (IMG_HANDLE) psPlatData;
#endif
	*ppsSysConfig = &sSysConfig;

	/* Ion is only supported on UMA builds */
#if (defined(SUPPORT_ION) && (!defined(LMA)))
	IonInit(NULL);
#endif

	gpsPlatData = psPlatData;

	(void)SysAcquireSystemData((IMG_HANDLE)psPlatData);

	return PVRSRV_OK;
e0:
	return eError;
}

IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PLAT_DATA *psPlatData = gpsPlatData;

	PVR_UNREFERENCED_PARAMETER(psSysConfig);

#if (defined(SUPPORT_ION) && (!defined(LMA)))
	IonDeinit();
#endif

	PCIDeInitDev(psPlatData);

	(void)SysReleaseSystemData((IMG_HANDLE)psPlatData);
}

PVRSRV_ERROR SysAcquireSystemData(IMG_HANDLE hSysData)
{
	PLAT_DATA *psPlatData = (PLAT_DATA *)hSysData;

	if (psPlatData == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psPlatData == gpsPlatData);
	psPlatData->uiRefCount++;

	return PVRSRV_OK;
}

PVRSRV_ERROR SysReleaseSystemData(IMG_HANDLE hSysData)
{
	PLAT_DATA *psPlatData = (PLAT_DATA *)hSysData;

	if (psPlatData == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_ASSERT(psPlatData == gpsPlatData);
	PVR_ASSERT(psPlatData->uiRefCount != 0);
	psPlatData->uiRefCount--;

	if (psPlatData->uiRefCount == 0)
	{
		OSFreeMem(psPlatData);
		gpsPlatData = IMG_NULL;
	}

	return PVRSRV_OK;
}

static IMG_UINT32 EmuRGXClockFreq(IMG_HANDLE hSysData)
{
	PVR_UNREFERENCED_PARAMETER(hSysData);
	return EMU_RGX_CLOCK_FREQ;
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
	PVRSRV_DEVICE_CONFIG	*psDevice;
	IMG_CPU_PHYADDR			sWrapperRegsCpuPBase;
	IMG_VOID				*pvWrapperRegs;

	psDevice = &sSysConfig.pasDevices[0];
	sWrapperRegsCpuPBase.uiAddr = psDevice->sRegsCpuPBase.uiAddr + EMULATOR_RGX_REG_WRAPPER_OFFSET;

	/* map emu registers */
	pvWrapperRegs = OSMapPhysToLin(sWrapperRegsCpuPBase,
								   EMULATOR_RGX_REG_WRAPPER_SIZE,
								   0);
	if (pvWrapperRegs == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"SysDebugDump: Failed to create wrapper register mapping\n"));
		return PVRSRV_ERROR_BAD_MAPPING;
	}

	PVR_DUMPDEBUG_LOG(("------[ System Debug ]------"));

#define SYS_EMU_DBG_R32(R)	PVR_DUMPDEBUG_LOG(("%-25s   0x%08X", #R ":", OSReadHWReg32(pvWrapperRegs, R)))
#define SYS_EMU_DBG_R64(R)	PVR_DUMPDEBUG_LOG(("%-25s 0x%010llX", #R ":", OSReadHWReg64(pvWrapperRegs, R)))

	SYS_EMU_DBG_R32(EMU_CR_PCI_MASTER);

	SYS_EMU_DBG_R64(EMU_CR_WRAPPER_ERROR);

	SYS_EMU_DBG_R32(EMU_CR_BANK_OUTSTANDING0);
	SYS_EMU_DBG_R32(EMU_CR_BANK_OUTSTANDING1);
	SYS_EMU_DBG_R32(EMU_CR_BANK_OUTSTANDING2);
	SYS_EMU_DBG_R32(EMU_CR_BANK_OUTSTANDING3);
	
	SYS_EMU_DBG_R32(EMU_CR_MEMORY_LATENCY);

	/* remove mapping */
	OSUnMapPhysToLin(pvWrapperRegs, EMULATOR_RGX_REG_WRAPPER_SIZE, 0);

	return PVRSRV_OK;

}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
