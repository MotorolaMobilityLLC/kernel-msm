/*************************************************************************/ /*!
@File
@Title          Device specific power routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
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

#include "rgxpower.h"
#include "rgx_fwif_km.h"
#include "rgxutils.h"
#include "rgxfwutils.h"
#include "pdump_km.h"
#include "rgxdefs_km.h"
#include "pvrsrv.h"
#include "pvr_debug.h"
#include "osfunc.h"
#include "rgxdebug.h"
#include "rgx_meta.h"
#include "devicemem_pdump.h"
#include "dfrgx_interface.h"
#include "rgxpowermon.h"
#include <linux/kernel.h>
#include <linux/workqueue.h>

extern IMG_UINT32 g_ui32HostSampleIRQCount;

struct workqueue_struct *rgxPowerRequestWq;
struct work_struct rgxPowerRequestWork;
IMG_HANDLE rgxPowerRequestData;

#if ! defined(FIX_HW_BRN_37453)
/*!
*******************************************************************************

 @Function	RGXEnableClocks

 @Description Enable RGX Clocks

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXEnableClocks(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGX clock: use default (automatic clock gating)");
}
#endif


/*!
*******************************************************************************

 @Function	RGXInitBIF

 @Description Initialise RGX BIF

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXInitBIF(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PVRSRV_ERROR	eError;
	IMG_DEV_PHYADDR sPCAddr;

	/*
		Acquire the address of the Kernel Page Catalogue.
	*/
	eError = MMU_AcquireBaseAddr(psDevInfo->psKernelMMUCtx, &sPCAddr);
	PVR_ASSERT(eError == PVRSRV_OK);

	/* Sanity check Cat-Base address */
	PVR_ASSERT((((sPCAddr.uiAddr
			>> psDevInfo->ui32KernelCatBaseAlignShift)
			<< psDevInfo->ui32KernelCatBaseShift)
			& ~psDevInfo->ui64KernelCatBaseMask) == 0x0UL);

	/*
		Write the kernel catalogue base.
	*/
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGX firmware MMU Page Catalogue");

	if (psDevInfo->ui32KernelCatBaseIdReg != -1)
	{
		/* Set the mapping index */
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseIdReg,
						psDevInfo->ui32KernelCatBaseId);

		/* pdump mapping context */
		PDUMPREG32(RGX_PDUMPREG_NAME,
							psDevInfo->ui32KernelCatBaseIdReg,
							psDevInfo->ui32KernelCatBaseId,
							PDUMP_FLAGS_POWERTRANS);
	}

	if (psDevInfo->ui32KernelCatBaseWordSize == 8)
	{
		/* Write the cat-base address */
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseReg,
						((sPCAddr.uiAddr
							>> psDevInfo->ui32KernelCatBaseAlignShift)
							<< psDevInfo->ui32KernelCatBaseShift)
							& psDevInfo->ui64KernelCatBaseMask);
	}
	else
	{
		/* Write the cat-base address */
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM,
						psDevInfo->ui32KernelCatBaseReg,
						(IMG_UINT32)(((sPCAddr.uiAddr
							>> psDevInfo->ui32KernelCatBaseAlignShift)
							<< psDevInfo->ui32KernelCatBaseShift)
							& psDevInfo->ui64KernelCatBaseMask));
	}

	/* pdump catbase address */
	MMU_PDumpWritePageCatBase(psDevInfo->psKernelMMUCtx,
							  RGX_PDUMPREG_NAME,
							  psDevInfo->ui32KernelCatBaseReg,
							  psDevInfo->ui32KernelCatBaseWordSize,
							  psDevInfo->ui32KernelCatBaseAlignShift,
							  psDevInfo->ui32KernelCatBaseShift,
							  PDUMP_FLAGS_POWERTRANS);

	/*
	 * Trusted META boot
	 */
#if defined(SUPPORT_TRUSTED_DEVICE)
	#if defined(TRUSTED_DEVICE_DEFAULT_ENABLED)
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Trusted Device enabled");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_BIF_TRUST, RGX_CR_BIF_TRUST_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_SYS_BUS_SECURE, RGX_CR_SYS_BUS_SECURE_ENABLE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	#else /* ! defined(TRUSTED_DEVICE_DEFAULT_ENABLED) */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Trusted Device disabled");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_TRUST, 0);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_BIF_TRUST, 0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_SYS_BUS_SECURE, 0);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_SYS_BUS_SECURE, 0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	#endif /* TRUSTED_DEVICE_DEFAULT_ENABLED */
#endif

}

#if defined(RGX_FEATURE_AXI_ACELITE)
/*!
*******************************************************************************

 @Function	RGXAXIACELiteInit

 @Description Initialise AXI-ACE Lite interface

 @Input psDevInfo - device info structure

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID RGXAXIACELiteInit(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_UINT32 ui32RegAddr;
	IMG_UINT64 ui64RegVal;

	ui32RegAddr = RGX_CR_AXI_ACE_LITE_CONFIGURATION;

	/* Setup AXI-ACE config. Set everything to outer cache */
	ui64RegVal =   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_NON_SNOOPING_SHIFT) |
				   (3U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_NON_SNOOPING_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_CACHE_MAINTENANCE_SHIFT)  |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWDOMAIN_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARDOMAIN_COHERENT_SHIFT) |
				   (((IMG_UINT64) 1) << RGX_CR_AXI_ACE_LITE_CONFIGURATION_DISABLE_COHERENT_WRITELINEUNIQUE_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_AWCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_COHERENT_SHIFT) |
				   (2U << RGX_CR_AXI_ACE_LITE_CONFIGURATION_ARCACHE_CACHE_MAINTENANCE_SHIFT);

	OSWriteHWReg64(psDevInfo->pvRegsBaseKM,
				   ui32RegAddr,
				   ui64RegVal);
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "Init AXI-ACE interface");
	PDUMPREG64(RGX_PDUMPREG_NAME, ui32RegAddr, ui64RegVal, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
}
#endif

/*!
*******************************************************************************

 @Function	RGXStart

 @Description

 (client invoked) chip-reset and initialisation

 @Input psDevInfo - device info structure

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStart(PVRSRV_RGXDEV_INFO	*psDevInfo, PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	RGXFWIF_INIT	*psRGXFWInit;

#if defined(FIX_HW_BRN_37453)
	/* Force all clocks on*/
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: force all clocks on");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_ON);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_ON, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
#endif

	/* Set RGX in soft-reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: soft reset everything");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL & (~RGX_CR_SOFT_RESET_SLC_EN));
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Take Rascal and Dust out of reset */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Rascal and Dust out of reset");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, (RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN) & (~RGX_CR_SOFT_RESET_SLC_EN));
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_MASKFULL ^ RGX_CR_SOFT_RESET_RASCALDUSTS_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Read soft-reset to fence previos write in order to clear the SOCIF pipeline */
	(IMG_VOID) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	/* Take everything out of reset but META */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Take everything out of reset but META");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, RGX_CR_SOFT_RESET_GARTEN_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

#if ! defined(FIX_HW_BRN_37453)
	/*
	 * Enable clocks.
	 */
	RGXEnableClocks(psDevInfo);
#endif

#if !defined(SUPPORT_META_SLAVE_BOOT)
	/* Configure META to Master boot */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: META Master boot");
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN);
	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_META_BOOT, RGX_CR_META_BOOT_MODE_EN, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
#endif

	/* Set Garten IDLE to META idle and Set the Garten Wrapper BIF Fence address */
	{
		IMG_UINT64 ui32BIFFenceAddr = RGXFW_BOOTLDR_DEVV_ADDR | RGX_CR_MTS_GARTEN_WRAPPER_CONFIG_IDLE_CTRL_META;

		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Configure META wrapper");
		OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui32BIFFenceAddr);
		PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_MTS_GARTEN_WRAPPER_CONFIG, ui32BIFFenceAddr, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	}

#if defined(RGX_FEATURE_AXI_ACELITE)
	/*
		We must init the AXI-ACE interface before 1st BIF transaction
	*/
	RGXAXIACELiteInit(psDevInfo);
#endif

	/*
	 * Initialise BIF.
	 */
	RGXInitBIF(psDevInfo);

	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: Take META out of reset");
	/* need to wait for at least 16 cycles before taking meta out of reset ... */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_POWERTRANS);
	
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET, 0x0);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, 0x0, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);

	(IMG_VOID) OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_SOFT_RESET);
	PDUMPREGREAD64(RGX_PDUMPREG_NAME, RGX_CR_SOFT_RESET, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
	
	/* ... and afterwards */
	PVRSRVSystemWaitCycles(psDevConfig, 32);
	PDUMPIDLWITHFLAGS(32, PDUMP_FLAGS_POWERTRANS);
#if defined(FIX_HW_BRN_37453)
	/* we rely on the 32 clk sleep from above */

	/* switch clocks back to auto */
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGXStart: set clocks back to auto");
	OSWriteHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_AUTO);
	PDUMPREG64(RGX_PDUMPREG_NAME, RGX_CR_CLK_CTRL, RGX_CR_CLK_CTRL_ALL_AUTO, PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWERTRANS);
#endif

	/*
	 * Start the firmware.
	 */
#if defined(SUPPORT_META_SLAVE_BOOT)
	RGXStartFirmware(psDevInfo);
#else
	PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS|PDUMP_FLAGS_CONTINUOUS, "RGXStart: RGX Firmware Master boot Start");
#endif
	
	OSMemoryBarrier();

	/* Check whether the FW has started by polling on bFirmwareStarted flag */
	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
									  (IMG_VOID **)&psRGXFWInit);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStart: Failed to acquire kernel fw if ctl (%u)", eError));
		return eError;
	}

	if (PVRSRVPollForValueKM((IMG_UINT32 *)&psRGXFWInit->bFirmwareStarted,
							 IMG_TRUE,
							 0xFFFFFFFF) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXStart: Polling for 'FW started' flag failed."));
		eError = PVRSRV_ERROR_TIMEOUT;
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
		return eError;
	}

#if defined(PDUMP)
	PDUMPCOMMENT("Wait for the Firmware to start.");
	eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfInitMemDesc,
											offsetof(RGXFWIF_INIT, bFirmwareStarted),
											IMG_TRUE,
											0xFFFFFFFFU,
											PDUMP_POLL_OPERATOR_EQUAL,
											PDUMP_FLAGS_POWERTRANS|PDUMP_FLAGS_CONTINUOUS);
	
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "RGXStart: problem pdumping POL for psRGXFWIfInitMemDesc (%d)", eError));
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
		return eError;
	}
#endif

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);

	return eError;
}


/*!
*******************************************************************************

 @Function	RGXStop

 @Description Stop RGX in preparation for power down

 @Input psDevInfo - RGX device info

 @Return   PVRSRV_ERROR

******************************************************************************/
static PVRSRV_ERROR RGXStop(PVRSRV_RGXDEV_INFO	*psDevInfo)

{
	PVRSRV_ERROR		eError; 


	eError = RGXRunScript(psDevInfo, psDevInfo->psScripts->asDeinitCommands, RGX_MAX_DEINIT_COMMANDS, PDUMP_FLAGS_POWERTRANS, IMG_NULL);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXStop: RGXRunScript failed (%d)", eError));
		panic("RGXStop() failed");
		return eError;
	}


	return PVRSRV_OK;
}

static IMG_VOID _RGXFWCBEntryAdd(PVRSRV_DEVICE_NODE    *psDeviceNode, IMG_UINT64 ui64TimeStamp, IMG_UINT32 ui32Type)
{
       PVRSRV_RGXDEV_INFO              *psDevInfo = psDeviceNode->pvDevice;
       RGXFWIF_GPU_UTIL_FWCB   *psRGXFWIfGpuUtilFWCb = psDevInfo->psRGXFWIfGpuUtilFWCb;

       switch(ui32Type)
       {
               case RGXFWIF_GPU_UTIL_FWCB_TYPE_CRTIME:
                       {
                               RGX_GPU_DVFS_HIST               *psGpuDVFSHistory = psDevInfo->psGpuDVFSHistory;
                               RGX_DATA                                *psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;

                               /* Advance DVFS history ID */
                               psGpuDVFSHistory->ui32CurrentDVFSId++;
                               if (psGpuDVFSHistory->ui32CurrentDVFSId >= RGX_GPU_DVFS_HIST_SIZE)
                               {
                                       psGpuDVFSHistory->ui32CurrentDVFSId = 0;
                               }

                               /* Update DVFS history ID that is used by the Host to populate state changes CB */
                               psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId = psGpuDVFSHistory->ui32CurrentDVFSId;

                               /* Store new DVFS freq into DVFS history entry */
                               psGpuDVFSHistory->aui32DVFSClockCB[psGpuDVFSHistory->ui32CurrentDVFSId] = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
                       }
                       /* 'break;' missing on purpose */

               case RGXFWIF_GPU_UTIL_FWCB_TYPE_END_CRTIME:
                       /* The DVFS history ID in this case is the same as the last one set by the Firmware,
                        * no need to add an identical copy in the DVFS history buffer */

                       /* Populate DVFS history entry (the GPU state is the same as in the last FW entry in this CB) */
                       psRGXFWIfGpuUtilFWCb->aui64CB[psRGXFWIfGpuUtilFWCb->ui32WriteOffset] =
                                       ((ui64TimeStamp << RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_CR_TIMER_MASK) |
                                       (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32LastGpuUtilState << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_STATE_MASK) |
                                       (((IMG_UINT64)ui32Type << RGXFWIF_GPU_UTIL_FWCB_TYPE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_TYPE_MASK) |
                                       (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId << RGXFWIF_GPU_UTIL_FWCB_ID_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_ID_MASK);
                       break;

               case RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_ON:
               case RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_OFF:
                       /* Populate DVFS history entry (the GPU state is the same as in the last FW entry in this CB) */
                       psRGXFWIfGpuUtilFWCb->aui64CB[psRGXFWIfGpuUtilFWCb->ui32WriteOffset] =
                                       ((ui64TimeStamp << RGXFWIF_GPU_UTIL_FWCB_TIMER_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_OS_TIMER_MASK) |
                                       (((IMG_UINT64)psRGXFWIfGpuUtilFWCb->ui32LastGpuUtilState << RGXFWIF_GPU_UTIL_FWCB_STATE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_STATE_MASK) |
                                       (((IMG_UINT64)ui32Type << RGXFWIF_GPU_UTIL_FWCB_TYPE_SHIFT) & RGXFWIF_GPU_UTIL_FWCB_TYPE_MASK);
                       break;

               default:
                       PVR_DPF((PVR_DBG_ERROR,"RGXFWCBEntryAdd: Wrong entry type"));
                       break;
       }

       psRGXFWIfGpuUtilFWCb->ui32WriteOffset++;
       if(psRGXFWIfGpuUtilFWCb->ui32WriteOffset >= RGXFWIF_GPU_UTIL_FWCB_SIZE)
       {
               psRGXFWIfGpuUtilFWCb->ui32WriteOffset = 0;
       }
}

/*
	RGXPrePowerState
*/
PVRSRV_ERROR RGXPrePowerState (IMG_HANDLE				hDevHandle,
							   PVRSRV_DEV_POWER_STATE	eNewPowerState,
							   PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
							   IMG_BOOL					bForced)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if ((eNewPowerState != eCurrentPowerState) &&
		(eNewPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
		RGXFWIF_KCCB_CMD	sPowCmd;
		RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;
		IMG_UINT32			ui32DM;
		IMG_BOOL		bCanPowerOff = IMG_FALSE;
		IMG_UINT64		ui64CRTimeStamp, ui64OSTimeStamp;

		/* Can We power-off the device?*/
		bCanPowerOff = rgx_powermeter_poweroff();
		if (!bCanPowerOff) {
			/* Abort power-off*/
			rgx_powermeter_poweron();
			/* PUnit is using RGX or We timed out, either case don't go to D0i3*/
			eError = PVRSRV_ERROR_RETRY;
			return eError;
		}
		/* Send the Power off request to the FW */
		sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
		sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_OFF_REQ;
		sPowCmd.uCmdData.sPowData.uPoweReqData.bForced = bForced;

		SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

		/* Send one pow command to each DM to make sure we flush all the DMs pipelines */
		for (ui32DM = 0; ui32DM < RGXFWIF_DM_MAX; ui32DM++)
		{
			eError = RGXSendCommandRaw(psDevInfo,
					ui32DM,
					&sPowCmd,
					sizeof(sPowCmd),
					PDUMP_FLAGS_POWERTRANS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Failed to send Power off request for DM%d", ui32DM));
				return eError;
			}
		}

		/* Wait for the firmware to complete processing. It cannot use PVRSRVWaitForValueKM as it relies 
		   on the EventObject which is signalled in this MISR */
		eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

		/* Check the Power state after the answer */
		if (eError == PVRSRV_OK)	
		{
			/* Finally, de-initialise some registers. */
			if (psFWTraceBuf->ePowState == RGXFWIF_POW_OFF)
			{
#if !defined(NO_HARDWARE)
				/* Wait for the pending META to host interrupts to come back. */
				eError = PVRSRVPollForValueKM(&g_ui32HostSampleIRQCount,
									          psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount,
									          0xffffffff);
#endif /* NO_HARDWARE */

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Wait for pending interrupts failed. Host:%d, FW: %d",
					g_ui32HostSampleIRQCount,
					psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount));
				}
				else
				{
					psDevInfo->bIgnoreFurtherIRQs =
						IMG_TRUE;

					ui64CRTimeStamp =
						(OSReadHWReg64(psDevInfo->
							       pvRegsBaseKM,
							       RGX_CR_TIMER) &
						 ~RGX_CR_TIMER_VALUE_CLRMSK)
						>> RGX_CR_TIMER_VALUE_SHIFT;
					ui64OSTimeStamp = OSClockus64();

                                       /* Add two entries to the GPU utilisation FWCB (current CR timestamp and current OS timestamp)
                                        * so that RGXGetGpuUtilStats() can link a power-on period to a previous power-off period (this one) */
                                       _RGXFWCBEntryAdd(psDeviceNode, ui64CRTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_END_CRTIME);
                                       _RGXFWCBEntryAdd(psDeviceNode, ui64OSTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_OFF);

					eError = RGXStop(psDevInfo);
					if (eError != PVRSRV_OK)
					{
						PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: RGXStop failed (%s)", PVRSRVGetErrorStringKM(eError)));
						eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
					}
					
					/*Report dfrgx We have the device OFF*/
					dfrgx_interface_power_state_set(0);
				}
			}
			else
			{
				/* the sync was updated bu the pow state isn't off -> the FW denied the transition */
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
			}
		}
		else if (eError == PVRSRV_ERROR_TIMEOUT)
		{
			/* timeout waiting for the FW to ack the request: return timeout */
			PVR_DPF((PVR_DBG_WARNING,"RGXPrePowerState: Timeout waiting for powoff ack from the FW"));
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"RGXPrePowerState: Error waiting for powoff ack from the FW (%s)", PVRSRVGetErrorStringKM(eError)));
			eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
		}

	}

	return eError;
}


/*
	RGXPostPowerState
*/
PVRSRV_ERROR RGXPostPowerState (IMG_HANDLE				hDevHandle,
								PVRSRV_DEV_POWER_STATE	eNewPowerState,
								PVRSRV_DEV_POWER_STATE	eCurrentPowerState,
								IMG_BOOL				bForced)
{
	if ((eNewPowerState != eCurrentPowerState) &&
		(eCurrentPowerState != PVRSRV_DEV_POWER_STATE_ON))
	{
		PVRSRV_ERROR		 eError;
		PVRSRV_DEVICE_NODE	 *psDeviceNode = hDevHandle;
		PVRSRV_RGXDEV_INFO	 *psDevInfo = psDeviceNode->pvDevice;
		PVRSRV_DEVICE_CONFIG *psDevConfig = psDeviceNode->psDevConfig;

		if (eCurrentPowerState == PVRSRV_DEV_POWER_STATE_OFF)
		{
                       IMG_UINT64 ui64CRTimeStamp = (OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_TIMER) & ~RGX_CR_TIMER_VALUE_CLRMSK) >> RGX_CR_TIMER_VALUE_SHIFT;
                       IMG_UINT64 ui64OSTimeStamp = OSClockus64();

                       /* Add two entries to the GPU utilisation FWCB (current OS timestamp and current CR timestamp)
                        * so that RGXGetGpuUtilStats() can link a power-on (this one) period to a  previous power-off period */
                       _RGXFWCBEntryAdd(psDeviceNode, ui64OSTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_POWER_ON);
                       _RGXFWCBEntryAdd(psDeviceNode, ui64CRTimeStamp, RGXFWIF_GPU_UTIL_FWCB_TYPE_CRTIME);

			psDevInfo->bIgnoreFurtherIRQs = IMG_TRUE;
			/*
				Run the RGX init script.
			*/
			eError = RGXStart(psDevInfo, psDevConfig);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPostPowerState: RGXStart failed"));
				panic("RGXStart() failed");

				return eError;
			}
			
			psDevInfo->bIgnoreFurtherIRQs = IMG_FALSE;

			/*Report dfrgx We have the device back ON*/
			dfrgx_interface_power_state_set(1);
			/*Report Punit that We have exited D0i3*/
			rgx_powermeter_poweron();

		}
	}

	PDUMPCOMMENT("RGXPostPowerState: Current state: %d, New state: %d", eCurrentPowerState, eNewPowerState);

	return PVRSRV_OK;
}


/*
	RGXPreClockSpeedChange
*/
PVRSRV_ERROR RGXPreClockSpeedChange (IMG_HANDLE				hDevHandle,
									 IMG_BOOL				bIdleDevice,
									 PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_ERROR		eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	PVR_UNREFERENCED_PARAMETER(psRGXData);

	PVR_DPF((PVR_DBG_MESSAGE,"RGXPreClockSpeedChange: RGX clock speed was %uHz",
			psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
		&& (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF)) 
	{
		if (bIdleDevice)
		{
			RGXFWIF_KCCB_CMD	sPowCmd;

			/* Send the IDLE request to the FW */
			sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
			sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
			sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_FALSE;

			SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

			/* Send one forced IDLE command to GP */
			eError = RGXSendCommandRaw(psDevInfo,
					RGXFWIF_DM_GP,
					&sPowCmd,
					sizeof(sPowCmd),
					PDUMP_FLAGS_POWERTRANS);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Failed to send IDLE request for DM%d", RGXFWIF_DM_GP));
				return eError;
			}

			/* Wait for the firmware to complete processing. */
			eError = PVRSRVPollForValueKM(psDevInfo->psPowSyncPrim->pui32LinAddr, 0x1, 0xFFFFFFFF);

			/* Check the Power state after the answer */
			if (eError == PVRSRV_OK)	
			{
				if (psFWTraceBuf->ePowState != RGXFWIF_POW_FORCED_IDLE)
				{
					/* the sync was updated but the pow state isn't idle -> the FW denied the transition */
					eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED;
				}
			}
			else if (eError == PVRSRV_ERROR_TIMEOUT)
			{
				/* timeout waiting for the FW to ack the request: return timeout */
				PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Timeout waiting for idle ack from the FW"));
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR,"RGXPreClockSpeedChange: Error waiting for idle ack from the FW (%s)", PVRSRVGetErrorStringKM(eError)));
				eError = PVRSRV_ERROR_DEVICE_POWER_CHANGE_FAILURE;
			}

			if (eError != PVRSRV_OK)
			{
				RGXFWIF_KCCB_CMD	sPowCmd;
				PVRSRV_ERROR		eError2;

				/* Send the IDLE request to the FW */
				sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
				sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
				sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_TRUE;

				SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

				/* Send one forced IDLE command to GP */
				eError2 = RGXSendCommandRaw(psDevInfo,
					RGXFWIF_DM_GP,
					&sPowCmd,
					sizeof(sPowCmd),
					PDUMP_FLAGS_POWERTRANS);
				if (eError2 != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"RGXPostClockSpeedChange: Failed to send Cancel IDLE request for DM%d", RGXFWIF_DM_GP));
				}
			}
		}

		if (eError == PVRSRV_OK)
		{
			/* Advance DVFS history ID */
			psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId++;
			if (psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId >= RGX_GPU_DVFS_HIST_SIZE)
			{
				psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId = 0;
			}

			/* Update DVFS history ID that is used by the FW to populate state changes CB */
			psDevInfo->psRGXFWIfGpuUtilFWCb->ui32CurrentDVFSId = psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId;
			/* Populate DVFS history entry */
			psDevInfo->psGpuDVFSHistory->aui32DVFSClockCB[psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId] = 0;
		}
	} else {
		eError = PVRSRV_ERROR_UNKNOWN_POWER_STATE;
	}

	return eError;
}


/*
	RGXPostClockSpeedChange
*/
PVRSRV_ERROR RGXPostClockSpeedChange (IMG_HANDLE				hDevHandle,
									  IMG_BOOL					bIdleDevice,
									  PVRSRV_DEV_POWER_STATE	eCurrentPowerState)
{
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	RGX_DATA			*psRGXData = (RGX_DATA*)psDeviceNode->psDevConfig->hDevData;
	PVRSRV_ERROR		eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD 	sCOREClkSpeedChangeCmd;
	RGXFWIF_TRACEBUF	*psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

    if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
		&& (psFWTraceBuf->ePowState != RGXFWIF_POW_OFF))
	{
		sCOREClkSpeedChangeCmd.eCmdType = RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE;
		sCOREClkSpeedChangeCmd.uCmdData.sCORECLKSPEEDCHANGEData.ui32NewClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
		/* Store new DVFS freq into DVFS history entry */
		psDevInfo->psGpuDVFSHistory->aui32DVFSClockCB[psDevInfo->psGpuDVFSHistory->ui32CurrentDVFSId] = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

		PDUMPCOMMENT("Scheduling CORE clock speed change command");
		eError = RGXSendCommandRaw(psDeviceNode->pvDevice,
											RGXFWIF_DM_GP,
											&sCOREClkSpeedChangeCmd,
											sizeof(sCOREClkSpeedChangeCmd),
											0);
	
		if (eError != PVRSRV_OK)
		{
			PDUMPCOMMENT("Scheduling CORE clock speed change command failed");
			PVR_DPF((PVR_DBG_ERROR, "RGXPostClockSpeedChange: Scheduling KCCB command failed. Error:%u", eError));
			return eError;
		}
 
		PVR_DPF((PVR_DBG_MESSAGE,"RGXPostClockSpeedChange: RGX clock speed changed to %uHz",
				psRGXData->psRGXTimingInfo->ui32CoreClockSpeed));
	}

       if ((eCurrentPowerState != PVRSRV_DEV_POWER_STATE_OFF) 
               && (psFWTraceBuf->ePowState == RGXFWIF_POW_FORCED_IDLE) 
               && bIdleDevice)
       {
               RGXFWIF_KCCB_CMD        sPowCmd;

               /* Send the IDLE request to the FW */
               sPowCmd.eCmdType = RGXFWIF_KCCB_CMD_POW;
               sPowCmd.uCmdData.sPowData.ePowType = RGXFWIF_POW_FORCED_IDLE_REQ;
               sPowCmd.uCmdData.sPowData.uPoweReqData.bCancelForcedIdle = IMG_TRUE;

               SyncPrimSet(psDevInfo->psPowSyncPrim, 0);

               /* Send one forced IDLE command to GP */
               eError = RGXSendCommandRaw(psDevInfo,
                               RGXFWIF_DM_GP,
                               &sPowCmd,
                               sizeof(sPowCmd),
                               PDUMP_FLAGS_POWERTRANS);

               if (eError != PVRSRV_OK)
               {
                       PVR_DPF((PVR_DBG_ERROR,"RGXPostClockSpeedChange: Failed to send Cancel IDLE request for DM%d", RGXFWIF_DM_GP));
                       return eError;
               }
       }

	return eError;
}


/*!
******************************************************************************

 @Function	RGXDustCountChange

 @Description

	Does change of number of DUSTs

 @Input	   hDevHandle : RGX Device Node
 @Input	   ui32NumberOfDusts : Number of DUSTs to make transition to

 @Return   PVRSRV_ERROR :

******************************************************************************/
PVRSRV_ERROR RGXDustCountChange(IMG_HANDLE				hDevHandle,
								IMG_UINT32				ui32NumberOfDusts)
{

	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;
	PVRSRV_ERROR		eError;
	RGXFWIF_KCCB_CMD 	sDustCountChange;
	IMG_UINT32			ui32MaxAvailableDusts = RGX_FEATURE_NUM_CLUSTERS / 2;

	PVR_ASSERT(ui32MaxAvailableDusts > 1);
	
	if ((ui32NumberOfDusts == 0) || (ui32NumberOfDusts > ui32MaxAvailableDusts))
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR, 
				"RGXDustCountChange: Invalid number of DUSTs (%u) while expecting value within <1,%u>. Error:%u", 
				ui32NumberOfDusts,
				ui32MaxAvailableDusts,
				eError));
		return eError;
	}

	sDustCountChange.eCmdType = RGXFWIF_KCCB_CMD_POW;
	sDustCountChange.uCmdData.sPowData.ePowType = RGXFWIF_POW_NUMDUST_CHANGE;
	sDustCountChange.uCmdData.sPowData.uPoweReqData.ui32NumOfDusts = ui32NumberOfDusts;

	PDUMPCOMMENT("Scheduling command to change Dust Count to %u", ui32NumberOfDusts);
	eError = RGXScheduleCommand(psDeviceNode->pvDevice,
				RGXFWIF_DM_GP,
				&sDustCountChange,
				sizeof(sDustCountChange),
				IMG_TRUE);
	
	if (eError != PVRSRV_OK)
	{
		PDUMPCOMMENT("Scheduling command to change Dust Count failed. Error:%u", eError);
		PVR_DPF((PVR_DBG_ERROR, "RGXDustCountChange: Scheduling KCCB to change Dust Count failed. Error:%u", eError));
		return eError;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR RGX_Do_ActivePowerRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_DEVICE_NODE	*psDeviceNode = hDevHandle;

	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_TRACEBUF *psFWTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	/* Powerlock to avoid further requests from racing with the FW hand-shake from now on
	   (previous kicks to this point are detected by the FW) */
	eError = PVRSRVPowerLock();
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGX_Do_ActivePowerRequest: Failed to acquire PowerLock (device index: %d, error: %s)",
					psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRVGetErrorStringKM(eError)));
		goto _RGXActivePowerRequest_PowerLock_failed;
	}

	/* Check again for IDLE once we have the power lock */
	if (psFWTraceBuf->ePowState == RGXFWIF_POW_IDLE)
	{

		psDevInfo->ui32ActivePMReqTotal++;

		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_POWERTRANS, "RGX_Do_ActivePowerRequest: FW set APM, handshake to power off");

		eError = 
			PVRSRVSetDevicePowerStateKM(psDeviceNode->sDevId.ui32DeviceIndex,
					PVRSRV_DEV_POWER_STATE_OFF,
					IMG_FALSE); /* forced */

		if (eError == PVRSRV_OK)
		{
			psDevInfo->ui32ActivePMReqOk++;
		}
		else if (eError == PVRSRV_ERROR_DEVICE_POWER_CHANGE_DENIED)
		{
			psDevInfo->ui32ActivePMReqDenied++;
		}

	}

	PVRSRVPowerUnlock();

_RGXActivePowerRequest_PowerLock_failed:

	return eError;
}

/*
	RGXActivePowerRequest
*/
PVRSRV_ERROR RGXActivePowerRequest(IMG_HANDLE hDevHandle)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PDUMPPOWCMDSTART();

	/* check if can acquire the bridge lock */
	if (OSTryAcquireBridgeLock()) {
		OSSetKeepPVRLock();

		eError = RGX_Do_ActivePowerRequest(hDevHandle);

		OSSetReleasePVRLock();
		OSReleaseBridgeLock();
	} else {
	/* failed to get the bridge lock, schedule the power request to work queue */
		rgxPowerRequestData = hDevHandle;
		queue_work(rgxPowerRequestWq, &rgxPowerRequestWork);
	}

	PDUMPPOWCMDEND();

	return eError;

}

IMG_VOID RGXActivePowerRequestEntry(struct work_struct *work)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PDUMPPOWCMDSTART();

	OSAcquireBridgeLock();
	OSSetKeepPVRLock();

	eError = RGX_Do_ActivePowerRequest(rgxPowerRequestData);

	OSSetReleasePVRLock();
	OSReleaseBridgeLock();

	PDUMPPOWCMDEND();

}


IMG_VOID RGXInitPowerRequestWQ (IMG_VOID)
{
	rgxPowerRequestWq = alloc_workqueue("rgxPowerRequestWq", WQ_UNBOUND, 1);
	INIT_WORK(&rgxPowerRequestWork, RGXActivePowerRequestEntry);
}


/******************************************************************************
 End of file (rgxpower.c)
******************************************************************************/
