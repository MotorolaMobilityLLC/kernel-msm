/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Emulator system-specific declarations and macros
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

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__

#define EMU_RGX_CLOCK_FREQ		(400000000)
#define RESERVE_DC_MEM_SIZE		(16 * 1024 * 1024)

#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (500)

static IMG_UINT32 EmuRGXClockFreq(IMG_HANDLE hSysData);

static IMG_VOID EmuCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
										IMG_DEV_PHYADDR *psDevPAddr,
										IMG_CPU_PHYADDR *psCpuPAddr);

static IMG_VOID EmuDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
										IMG_CPU_PHYADDR *psCpuPAddr,
										IMG_DEV_PHYADDR *psDevPAddr);

static RGX_TIMING_INFORMATION sRGXTimingInfo = 
{
	.ui32CoreClockSpeed		= RGX_CORE_CLOCK_SPEED_DEFAULT,
	.bEnableActivePM		= IMG_TRUE,
#ifdef CONFIG_MOOREFIELD
	.bEnableRDPowIsland		= IMG_TRUE,
#else
	.bEnableRDPowIsland		= IMG_FALSE,
#endif
	/* ui32ActivePMLatencyms */
	.ui32ActivePMLatencyms		= RGX_APM_LATENCY_DEFAULT
};

static RGX_DATA sRGXData = { &sRGXTimingInfo };

static PVRSRV_DEVICE_CONFIG sDevices[] = {
	/* RGX device */
	{
		/* uiFlags */
		0,
		/* pszName */
		"RGX",
		/* eDeviceType */
		PVRSRV_DEVICE_TYPE_RGX,
		/* sRegsCpuPBase */
		{ 0 },
		/* ui32RegsSize */
		0,
		/* ui32IRQ */
		0,
		/* bIRQIsShared */
		IMG_TRUE,
		/* hDevData */
		&sRGXData,
		/* hSysData */
		IMG_NULL,
		/* ui32PhysHeapID */
		{ 0, 0 }, /* Use physical heap 0 only */
		/* No power management on no HW system */
		/* pfnPrePowerState */
		IMG_NULL,
		/* pfnPostPowerState */
		IMG_NULL,
		/* pfnClockFreqGet */
		EmuRGXClockFreq,
		/* pfnInterruptHandled */
		IMG_NULL,
		/* pfnCheckMemAllocSize */
		SysCheckMemAllocSize,
		/* eBPDM */
		RGXFWIF_DM_TA, /* initialised to a valid DM but no breakpoint is set yet */
		/* bBPSet */
		IMG_FALSE,
	}
};

static PHYS_HEAP_FUNCTIONS gsPhysHeapFuncs = { EmuCpuPAddrToDevPAddr, EmuDevPAddrToCpuPAddr };

#if defined(LMA)
static PHYS_HEAP_CONFIG	gsPhysHeapConfig[] = {
	{	/* Heap 0 is used for RGX */
		/* ui32PhysHeapID */
		0,
		/* eType */
		PHYS_HEAP_TYPE_LMA,
		/* sStartAddr */
		{ 0 },
		/* uiSize */
		0,
		/* pszPDumpMemspaceName	*/
		"LMA",
		/* psMemFuncs */
		&gsPhysHeapFuncs,
		/* hPrivData */
		IMG_NULL,
	},
	{	/* Heap 1 is used for DC memory */
		/* ui32PhysHeapID */
		1,
		/* eType */
		PHYS_HEAP_TYPE_LMA,
		/* sStartAddr */
		{ 0 },
		/* uiSize */
		0,
		/* pszPDumpMemspaceName	*/
		"LMA",
		/* psMemFuncs */
		&gsPhysHeapFuncs,
		/* hPrivData */
		IMG_NULL,
	}
};
#else
static PHYS_HEAP_CONFIG	gsPhysHeapConfig = {
	/* ui32PhysHeapID */
	0,
	/* eType */
	PHYS_HEAP_TYPE_UMA,
	/* sStartAddr */
	{ 0 },
	/* uiSize */
	0,
	/* pszPDumpMemspaceName	*/
	"SYSMEM",
	/* psMemFuncs */
	&gsPhysHeapFuncs,
	/* hPrivData */
	IMG_NULL,
};
#endif

/* default BIF tiling heap x-stride configurations. */
static IMG_UINT32 gauiBIFTilingHeapXStrides[RGXFWIF_NUM_BIF_TILING_CONFIGS] =
{
	0, /* BIF tiling heap 1 x-stride */
	1, /* BIF tiling heap 2 x-stride */
	2, /* BIF tiling heap 3 x-stride */
	3  /* BIF tiling heap 4 x-stride */
};

static PVRSRV_SYSTEM_CONFIG sSysConfig = {
	/* uiSysFlags */
	0,
	/* pszSystemName */
	"Emu with Rogue",
	/* uiDeviceCount */
	sizeof(sDevices)/sizeof(PVRSRV_DEVICE_CONFIG),
	/* pasDevices */
	&sDevices[0],
	/* pfnSysPrePowerState */
	IMG_NULL,
	/* pfnSysPostPowerState */
	IMG_NULL,
	/* eCacheSnoopingMode */
	PVRSRV_SYSTEM_SNOOP_NONE,
#if defined(LMA)
	/* pasPhysHeaps */
	&gsPhysHeapConfig[0],
#else
	&gsPhysHeapConfig,
#endif
	/* Physcial memory heaps */
	/* ui32PhysHeapCount */
	sizeof(gsPhysHeapConfig) / sizeof(PHYS_HEAP_CONFIG),

	/* BIF tiling heap config */
	gauiBIFTilingHeapXStrides,
	IMG_ARR_NUM_ELEMS(gauiBIFTilingHeapXStrides),
};

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/
 
#endif	/* __SYSCCONFIG_H__ */
