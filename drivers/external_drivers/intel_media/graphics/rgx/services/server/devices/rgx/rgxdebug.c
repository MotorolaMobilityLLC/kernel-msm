/*************************************************************************/ /*!
@File
@Title          Rgx debug information
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX debugging functions
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
//#define PVR_DPF_FUNCTION_TRACE_ON 1
#undef PVR_DPF_FUNCTION_TRACE_ON

#include "rgxdefs_km.h"
#include "rgxdevice.h"
#include "rgxmem.h"
#include "allocmem.h"
#include "osfunc.h"

#include "lists.h"

#include "rgxdebug.h"
#include "pvrversion.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "rgxutils.h"
#include "tlstream.h"
#include "rgxfwutils.h"
#include "pvrsrv.h"

#include "devicemem_pdump.h"

#include "rgx_fwif.h"
#include "pvrsrv.h"

#if defined(PVRSRV_ENABLE_FW_TRACE_DEBUGFS)
#include "rgx_fwif_sf.h"
#include "rgxfw_log_helper.h"
#endif

#include "rgxta3d.h"
#include "rgxcompute.h"
#include "rgxtransfer.h"
#if defined(RGX_FEATURE_RAY_TRACING)
#include "rgxray.h"
#endif


#define RGX_DEBUG_STR_SIZE	(150)

#define RGX_CR_BIF_CAT_BASE0                              (0x1200U)
#define RGX_CR_BIF_CAT_BASE1                              (0x1208U)

#define RGX_CR_BIF_CAT_BASEN(n) \
	RGX_CR_BIF_CAT_BASE0 + \
	((RGX_CR_BIF_CAT_BASE1 - RGX_CR_BIF_CAT_BASE0) * n)

IMG_CHAR* pszPowStateName [] = {
#define X(NAME)	#NAME,
	RGXFWIF_POW_STATES
#undef X
};


extern IMG_UINT32 g_ui32HostSampleIRQCount;


/*
 *  This macro is used as a convenient way to switch between PVR_LOG and 
 *  a custom supplied printf function. 
 */
#define PVR_DUMPDEBUG_LOG(x)  do { if (pfnDumpDebugPrintf) {pfnDumpDebugPrintf x;} else {PVR_LOG(x);} } while(0)


/*!
*******************************************************************************

 @Function	_RGXDecodePMPC

 @Description

 Return the name for the PM managed Page Catalogues

 @Input ui32PC	 - Page Catalogue number

 @Return   IMG_VOID

******************************************************************************/
static IMG_CHAR* _RGXDecodePMPC(IMG_UINT32 ui32PC)
{
	IMG_CHAR* pszPMPC = " (-)";

	switch (ui32PC)
	{
		case 0x8: pszPMPC = " (PM-VCE0)"; break;
		case 0x9: pszPMPC = " (PM-TE0)"; break;
		case 0xA: pszPMPC = " (PM-ZLS0)"; break;
		case 0xB: pszPMPC = " (PM-ALIST0)"; break;
		case 0xC: pszPMPC = " (PM-VCE1)"; break;
		case 0xD: pszPMPC = " (PM-TE1)"; break;
		case 0xE: pszPMPC = " (PM-ZLS1)"; break;
		case 0xF: pszPMPC = " (PM-ALIST1)"; break;
	}

	return pszPMPC;
}

/*!
*******************************************************************************

 @Function	_RGXDecodeBIFReqTags

 @Description

 Decode the BIF Tag ID and SideBand data fields from BIF_FAULT_BANK_REQ_STATUS regs

 @Input ui32TagID           - Tag ID value
 @Input ui32TagSB           - Tag Sideband data
 @Output ppszTagID          - Decoded string from the Tag ID
 @Output ppszTagSB          - Decoded string from the Tag SB
 @Output pszScratchBuf      - Buffer provided to the function to generate the debug strings
 @Input ui32ScratchBufSize  - Size of the provided buffer

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID _RGXDecodeBIFReqTags(IMG_UINT32		ui32TagID, 
									 IMG_UINT32		ui32TagSB, 
									 IMG_CHAR		**ppszTagID, 
									 IMG_CHAR		**ppszTagSB,
									 IMG_CHAR		*pszScratchBuf,
									 IMG_UINT32		ui32ScratchBufSize)
{
	/* default to unknown */
	IMG_CHAR *pszTagID = "-";
	IMG_CHAR *pszTagSB = "-";

	switch (ui32TagID)
	{
		case 0x0:
		{
			pszTagID = "MMU";
			break;
		}
		case 0x1:
		{
			pszTagID = "TLA";
			break;
		}
		case 0x2:
		{
			pszTagID = "HOST";
			break;
		}
		case 0x3:
		{
			pszTagID = "META";
			switch (ui32TagSB)
			{
				case 0x0: pszTagSB = "dcache"; break;
				case 0x1: pszTagSB = "icache"; break;
				case 0x2: pszTagSB = "jtag"; break;
				case 0x3: pszTagSB = "slave bus"; break;
			}
			break;
		}
		case 0x4:
		{
			pszTagID = "USC";
			break;
		}
		case 0x5:
		{
			pszTagID = "PBE";
			break;
		}
		case 0x6:
		{
			pszTagID = "ISP";
			break;
		}
		case 0x7:
		{
			pszTagID = "IPF";
			switch (ui32TagSB)
			{
				case 0x0: pszTagSB = "Macrotile Header"; break;
				case 0x1: pszTagSB = "Region Header"; break;
				case 0x2: pszTagSB = "DBSC"; break;
				case 0x3: pszTagSB = "CPF"; break;
				case 0x4: 
				case 0x6:
				case 0x8: pszTagSB = "Control Stream"; break;
				case 0x5: 
				case 0x7:
				case 0x9: pszTagSB = "Primitive Block"; break;
			}
			break;
		}
		case 0x8:
		{
			pszTagID = "CDM";
			switch (ui32TagSB)
			{
				case 0x0: pszTagSB = "Control Stream"; break;
				case 0x1: pszTagSB = "Indirect Data"; break;
				case 0x2: pszTagSB = "Event Write"; break;
				case 0x3: pszTagSB = "Context State"; break;
			}
			break;
		}
		case 0x9:
		{
			pszTagID = "VDM";
			switch (ui32TagSB)
			{
				case 0x0: pszTagSB = "Control Stream"; break;
				case 0x1: pszTagSB = "PPP State"; break;
				case 0x2: pszTagSB = "Index Data"; break;
				case 0x4: pszTagSB = "Call Stack"; break;
				case 0x8: pszTagSB = "Context State"; break;
			}
			break;
		}
		case 0xA:
		{
			pszTagID = "PM";
			switch (ui32TagSB)
			{
				case 0x0: pszTagSB = "PMA_TAFSTACK"; break;
				case 0x1: pszTagSB = "PMA_TAMLIST"; break;
				case 0x2: pszTagSB = "PMA_3DFSTACK"; break;
				case 0x3: pszTagSB = "PMA_3DMLIST"; break;
				case 0x4: pszTagSB = "PMA_PMCTX0"; break;
				case 0x5: pszTagSB = "PMA_PMCTX1"; break;
				case 0x6: pszTagSB = "PMA_MAVP"; break;
				case 0x7: pszTagSB = "PMA_UFSTACK"; break;
				case 0x8: pszTagSB = "PMD_TAFSTACK"; break;
				case 0x9: pszTagSB = "PMD_TAMLIST"; break;
				case 0xA: pszTagSB = "PMD_3DFSTACK"; break;
				case 0xB: pszTagSB = "PMD_3DMLIST"; break;
				case 0xC: pszTagSB = "PMD_PMCTX0"; break;
				case 0xD: pszTagSB = "PMD_PMCTX1"; break;
				case 0xF: pszTagSB = "PMD_UFSTACK"; break;
				case 0x10: pszTagSB = "PMA_TAMMUSTACK"; break;
				case 0x11: pszTagSB = "PMA_3DMMUSTACK"; break;
				case 0x12: pszTagSB = "PMD_TAMMUSTACK"; break;
				case 0x13: pszTagSB = "PMD_3DMMUSTACK"; break;
				case 0x14: pszTagSB = "PMA_TAUFSTACK"; break;
				case 0x15: pszTagSB = "PMA_3DUFSTACK"; break;
				case 0x16: pszTagSB = "PMD_TAUFSTACK"; break;
				case 0x17: pszTagSB = "PMD_3DUFSTACK"; break;
				case 0x18: pszTagSB = "PMA_TAVFP"; break;
				case 0x19: pszTagSB = "PMD_3DVFP"; break;
				case 0x1A: pszTagSB = "PMD_TAVFP"; break;
			}
			break;
		}
		case 0xB:
		{
			pszTagID = "TA";
			switch (ui32TagSB)
			{
				case 0x1: pszTagSB = "VCE"; break;
				case 0x2: pszTagSB = "TPC"; break;
				case 0x3: pszTagSB = "TE Control Stream"; break;
				case 0x4: pszTagSB = "TE Region Header"; break;
				case 0x5: pszTagSB = "TE Render Target Cache"; break;
				case 0x6: pszTagSB = "TEAC Render Target Cache"; break;
				case 0x7: pszTagSB = "VCE Render Target Cache"; break;
			}
			break;
		}
		case 0xC:
		{
			pszTagID = "TPF";
			break;
		}
		case 0xD:
		{
			pszTagID = "PDS";
			break;
		}
		case 0xE:
		{
			pszTagID = "MCU";
			{
				IMG_UINT32 ui32Burst = (ui32TagSB >> 5) & 0x7;
				IMG_UINT32 ui32GroupEnc = (ui32TagSB >> 2) & 0x7;
				IMG_UINT32 ui32Group = ui32TagSB & 0x2;

				IMG_CHAR* pszBurst = "";
				IMG_CHAR* pszGroupEnc = "";
				IMG_CHAR* pszGroup = "";

				switch (ui32Burst)
				{
					case 0x4: pszBurst = "Lower 256bits"; break;
					case 0x5: pszBurst = "Upper 256bits"; break;
					case 0x6: pszBurst = "512 bits"; break;
					default:  pszBurst = (ui32Burst & 0x2)?
								"128bit word within the Lower 256bits":
								"128bit word within the Upper 256bits"; break;
				}
				switch (ui32GroupEnc)
				{
					case 0x0: pszGroupEnc = "TPUA_USC"; break;
					case 0x1: pszGroupEnc = "TPUB_USC"; break;
					case 0x2: pszGroupEnc = "USCA_USC"; break;
					case 0x3: pszGroupEnc = "USCB_USC"; break;
					case 0x4: pszGroupEnc = "PDS_USC"; break;
#if (RGX_FEATURE_NUM_CLUSTERS < 6)
					case 0x5: pszGroupEnc = "PDSRW"; break;
#elif (RGX_FEATURE_NUM_CLUSTERS == 6)
					case 0x5: pszGroupEnc = "UPUC_USC"; break;
					case 0x6: pszGroupEnc = "TPUC_USC"; break;
					case 0x7: pszGroupEnc = "PDSRW"; break;
#endif
				}
				switch (ui32Group)
				{
					case 0x0: pszGroup = "Banks 0-3"; break;
					case 0x1: pszGroup = "Banks 4-7"; break;
					case 0x2: pszGroup = "Banks 8-11"; break;
					case 0x3: pszGroup = "Banks 12-15"; break;
				}

				OSSNPrintf(pszScratchBuf, ui32ScratchBufSize,
								"%s, %s, %s", pszBurst, pszGroupEnc, pszGroup);
				pszTagSB = pszScratchBuf;
			}
			break;
		}
		case 0xF:
		{
			pszTagID = "FB_CDC";
			{
				IMG_UINT32 ui32Req = (ui32TagSB >> 2) & 0x3;
				IMG_UINT32 ui32MCUSB = ui32TagSB & 0x3;

				IMG_CHAR* pszReqId = (ui32TagSB & 0x10)?"FBDC":"FBC";
				IMG_CHAR* pszOrig = "";

				switch (ui32Req)
				{
					case 0x0: pszOrig = "ZLS"; break;
					case 0x1: pszOrig = (ui32TagSB & 0x10)?"MCU":"PBE"; break;
					case 0x2: pszOrig = "Host"; break;
					case 0x3: pszOrig = "TLA"; break;
				}
				OSSNPrintf(pszScratchBuf, ui32ScratchBufSize,
							"%s Request, originator %s, MCU sideband 0x%X",
							pszReqId, pszOrig, ui32MCUSB);
				pszTagSB = pszScratchBuf;
			}
			break;
		}
	} /* switch(TagID) */

	*ppszTagID = pszTagID;
	*ppszTagSB = pszTagSB;
}

/*!
*******************************************************************************

 @Function	_RGXDumpRGXBIFBank

 @Description

 Dump BIF Bank state in human readable form.

 @Input psDevInfo				- RGX device info
 @Input ui32BankID	 			- BIF Bank identification number
 @Input ui64MMUStatus			- MMU Status register value
 @Input ui64ReqStatus			- BIF request Status register value
 @Input bBIFSummary				- Flag to check whether the function is called
 	 	 	 	 	 	 	 	  as a part of the debug dump summary or
								  as a part of a HWR log
 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID _RGXDumpRGXBIFBank(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                   PVRSRV_RGXDEV_INFO	*psDevInfo,
                                   IMG_UINT32			ui32BankID,
                                   IMG_UINT64			ui64MMUStatus,
                                   IMG_UINT64			ui64ReqStatus,
                                   IMG_BOOL				bBIFSummary)
{

	if (ui64MMUStatus == 0x0)
	{
		PVR_DUMPDEBUG_LOG(("BIF%d - OK", ui32BankID));
	}
	else
	{
		/* Bank 0 & 1 share the same fields */
		PVR_DUMPDEBUG_LOG(("%sBIF%d - FAULT:",
						  (bBIFSummary)?"":"    ",
						  ui32BankID));

		/* MMU Status */
		{
			IMG_UINT32 ui32PC = 
				(ui64MMUStatus & ~RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_CAT_BASE_CLRMSK) >>
					RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_CAT_BASE_SHIFT;

			IMG_UINT32 ui32PageSize = 
				(ui64MMUStatus & ~RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_PAGE_SIZE_CLRMSK) >>
					RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_PAGE_SIZE_SHIFT;

			IMG_UINT32 ui32MMUDataType = 
				(ui64MMUStatus & ~RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_DATA_TYPE_CLRMSK) >>
					RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_DATA_TYPE_SHIFT;

			IMG_BOOL bROFault = (ui64MMUStatus & RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_FAULT_RO_EN) != 0;
			IMG_BOOL bProtFault = (ui64MMUStatus & RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_FAULT_PM_META_RO_EN) != 0;

			PVR_DUMPDEBUG_LOG(("%s  * MMU status (0x%016llX): PC = %d%s, Page Size = %d, MMU data type = %d%s%s.",
			                  (bBIFSummary)?"":"    ",
							  ui64MMUStatus,
			                  ui32PC,
			                  (ui32PC < 0x8)?"":_RGXDecodePMPC(ui32PC),
			                  ui32PageSize,
			                  ui32MMUDataType,
			                  (bROFault)?", Read Only fault":"",
			                  (bProtFault)?", PM/META protection fault":""));
		}

		/* Req Status */
		{
			IMG_CHAR *pszTagID;
			IMG_CHAR *pszTagSB;
			IMG_CHAR aszScratch[RGX_DEBUG_STR_SIZE];

			IMG_BOOL bRead = (ui64ReqStatus & RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_RNW_EN) != 0;
			IMG_UINT32 ui32TagSB = 
				(ui64ReqStatus & ~RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_TAG_SB_CLRMSK) >>
					RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_TAG_SB_SHIFT;
			IMG_UINT32 ui32TagID = 
				(ui64ReqStatus & ~RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_TAG_ID_CLRMSK) >>
							RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_TAG_ID_SHIFT;
			IMG_UINT64 ui64Addr = (ui64ReqStatus & ~RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_ADDRESS_CLRMSK);

			_RGXDecodeBIFReqTags(ui32TagID, ui32TagSB, &pszTagID, &pszTagSB, &aszScratch[0], RGX_DEBUG_STR_SIZE);

			PVR_DUMPDEBUG_LOG(("%s  * Request (0x%016llX): %s (%s), %s 0x%010llX.",
							  (bBIFSummary)?"":"    ",
							  ui64ReqStatus,
			                  pszTagID,
			                  pszTagSB,
			                  (bRead)?"Reading from":"Writing to",
			                  ui64Addr));
		}

		/* Check if the host thinks this fault is valid */
		if(bBIFSummary)
		{
			IMG_UINT32 ui32PC = 
				(ui64MMUStatus & ~RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_CAT_BASE_CLRMSK) >>
					RGX_CR_BIF_FAULT_BANK0_MMU_STATUS_CAT_BASE_SHIFT;
			IMG_DEV_VIRTADDR sFaultDevVAddr;
			IMG_DEV_PHYADDR sPCDevPAddr;

			sPCDevPAddr.uiAddr = OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_CAT_BASEN(ui32PC));
			sFaultDevVAddr.uiAddr = (ui64ReqStatus & ~RGX_CR_BIF_FAULT_BANK0_REQ_STATUS_ADDRESS_CLRMSK);
			RGXCheckFaultAddress(psDevInfo, &sFaultDevVAddr, &sPCDevPAddr);
		}
		
	}

}


/*!
*******************************************************************************

 @Function	_RGXDumpFWAssert

 @Description

 Dump FW assert strings when a thread asserts.

 @Input psRGXFWIfTraceBufCtl	- RGX FW trace buffer

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID _RGXDumpFWAssert(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                 RGXFWIF_TRACEBUF *psRGXFWIfTraceBufCtl)
{
	IMG_CHAR    *pszTraceAssertPath;
	IMG_CHAR    *pszTraceAssertInfo;
	IMG_INT32   ui32TraceAssertLine;
	IMG_UINT32  i;

	for (i = 0; i < RGXFW_THREAD_NUM; i++)
	{
		pszTraceAssertPath = psRGXFWIfTraceBufCtl->sTraceBuf[i].sAssertBuf.szPath;
		pszTraceAssertInfo = psRGXFWIfTraceBufCtl->sTraceBuf[i].sAssertBuf.szInfo;
		ui32TraceAssertLine = psRGXFWIfTraceBufCtl->sTraceBuf[i].sAssertBuf.ui32LineNum;

		/* print non null assert strings */
		if (*pszTraceAssertInfo)
		{
			PVR_DUMPDEBUG_LOG(("FW-T%d Assert: %s (%s:%d)", 
			                  i, pszTraceAssertInfo, pszTraceAssertPath, ui32TraceAssertLine));
		}
	}
}

static IMG_VOID _RGXDumpFWPoll(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                               RGXFWIF_TRACEBUF *psRGXFWIfTraceBufCtl)
{
	IMG_UINT32 i;
	for (i = 0; i < RGXFW_THREAD_NUM; i++)
	{
		if (psRGXFWIfTraceBufCtl->aui32CrPollAddr[i])
		{
			PVR_DUMPDEBUG_LOG(("T%u polling %s (reg:0x%08X val:0x%08X)",
			                  i,
			                  ((psRGXFWIfTraceBufCtl->aui32CrPollAddr[i] & RGXFW_POLL_TYPE_SET)?("set"):("unset")), 
			                  psRGXFWIfTraceBufCtl->aui32CrPollAddr[i] & ~RGXFW_POLL_TYPE_SET, 
			                  psRGXFWIfTraceBufCtl->aui32CrPollValue[i]));
		}
	}

}

static IMG_VOID _RGXDumpFWHWRInfo(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                  RGXFWIF_TRACEBUF *psRGXFWIfTraceBufCtl, PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_BOOL        	bAnyLocked = IMG_FALSE;
	IMG_UINT32      	dm, i;
	IMG_UINT32      	ui32LineSize;
	IMG_CHAR	    	*pszLine, *pszTemp;
	const IMG_CHAR 		*apszDmNames[RGXFWIF_DM_MAX + 1] = { "GP(", "2D(", "TA(", "3D(", "CDM(",
#if defined(RGX_FEATURE_RAY_TRACING)
								 "RTU(", "SHG(",
#endif /* RGX_FEATURE_RAY_TRACING */
								 NULL };

	const IMG_CHAR 		*pszMsgHeader = "Number of HWR: ";
	IMG_CHAR 			*pszLockupType = "";
	RGXFWIF_HWRINFOBUF 	*psHWInfoBuf = psDevInfo->psRGXFWIfHWRInfoBuf;
	RGX_HWRINFO 		*psHWRInfo;
	IMG_UINT32      	ui32MsgHeaderSize = OSStringLength(pszMsgHeader);
	IMG_UINT32			ui32HWRRecoveryFlags;
	IMG_UINT32			ui32ReadIndex;

	for (dm = 0; dm < RGXFWIF_DM_MAX; dm++)
	{
		if (psRGXFWIfTraceBufCtl->aui16HwrDmLockedUpCount[dm]  ||
		    psRGXFWIfTraceBufCtl->aui16HwrDmOverranCount[dm])
		{
			bAnyLocked = IMG_TRUE;
			break;					
		}
	}

	if (!bAnyLocked && (psRGXFWIfTraceBufCtl->ui32HWRStateFlags & RGXFWIF_HWR_HARDWARE_OK))
	{
		/* No HWR situation, print nothing */
		return;
	}

	ui32LineSize = sizeof(IMG_CHAR) * (	ui32MsgHeaderSize + 
			(RGXFWIF_DM_MAX*(	4/*DM name + left parenthesis*/ + 
								5/*UINT16 max num of digits*/ + 
								1/*slash*/ + 
								5/*UINT16 max num of digits*/ + 
								3/*right parenthesis + comma + space*/)) + 
			7 + (RGXFWIF_DM_MAX*6)/* FALSE() + (UINT16 max num + comma) per DM */ +
			1/* \0 */);

	pszLine = OSAllocMem(ui32LineSize);
	if (pszLine == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"_RGXDumpRGXDebugSummary: Out of mem allocating line string (size: %d)", ui32LineSize));
		return;
	}

	OSStringCopy(pszLine,pszMsgHeader);
	pszTemp = pszLine + ui32MsgHeaderSize;

	for (dm = 0; (dm < RGXFWIF_DM_MAX) && (apszDmNames[dm] != IMG_NULL); dm++)
	{
		OSStringCopy(pszTemp,apszDmNames[dm]);
		pszTemp += OSStringLength(apszDmNames[dm]);
		pszTemp += OSSNPrintf(pszTemp, 
				5 + 1 + 5 + 1 + 5 + 1 + 1 + 1 + 1 /* UINT16 + slash + UINT16 + plus + UINT16 + right parenthesis + comma + space + \0 */,
				"%u/%u+%u), ",
				psRGXFWIfTraceBufCtl->aui16HwrDmRecoveredCount[dm],
				psRGXFWIfTraceBufCtl->aui16HwrDmLockedUpCount[dm],
				psRGXFWIfTraceBufCtl->aui16HwrDmOverranCount[dm]);
	}

	OSStringCopy(pszTemp, "FALSE(");
	pszTemp += 6;

	for (dm = 0; (dm < RGXFWIF_DM_MAX) && (apszDmNames[dm] != IMG_NULL); dm++)
	{
		pszTemp += OSSNPrintf(pszTemp, 
				5 + 1 + 1 /* UINT16 max num + comma + \0 */,
				(dm < RGXFWIF_DM_MAX-1 ? "%u," : "%u)"),
				psRGXFWIfTraceBufCtl->aui16HwrDmFalseDetectCount[dm]);
	}

	PVR_DUMPDEBUG_LOG((pszLine));

	OSFreeMem(pszLine);

	/* Print out per HWR info */
	for (dm = 0; (dm < RGXFWIF_DM_MAX) && (apszDmNames[dm] != IMG_NULL); dm++)
	{
		if (dm == RGXFWIF_DM_GP)
		{
			PVR_DUMPDEBUG_LOG(("DM %d (GP)", dm));
		}
		else
		{
			PVR_DUMPDEBUG_LOG(("DM %d (HWRflags 0x%08x)", dm, psRGXFWIfTraceBufCtl->aui32HWRRecoveryFlags[dm]));
		}

		ui32ReadIndex = 0;
		for(i = 0 ; i < RGXFWIF_HWINFO_MAX ; i++)
		{
			psHWRInfo = &psHWInfoBuf->sHWRInfo[ui32ReadIndex];

			if((psHWRInfo->eDM == dm) && (psHWRInfo->ui32HWRNumber != 0))
			{
				ui32HWRRecoveryFlags = psHWRInfo->ui32HWRRecoveryFlags;
				if(ui32HWRRecoveryFlags & RGXFWIF_DM_STATE_GUILTY_LOCKUP) { pszLockupType = ", Guilty Lockup"; }
				else if (ui32HWRRecoveryFlags & RGXFWIF_DM_STATE_INNOCENT_LOCKUP) { pszLockupType = ", Innocent Lockup"; }
				else if (ui32HWRRecoveryFlags & RGXFWIF_DM_STATE_GUILTY_OVERRUNING) { pszLockupType = ", Guilty Overrun"; }
				else if (ui32HWRRecoveryFlags & RGXFWIF_DM_STATE_GUILTY_LOCKUP) { pszLockupType = ", Innocent Overrun"; }

				PVR_DUMPDEBUG_LOG(("  Recovery %d: PID = %d, frame = %d, HWRTData = 0x%08X, EventStatus = 0x%08X, CRTimer = 0x%012llX%s",
								  psHWRInfo->ui32HWRNumber,
								  psHWRInfo->ui32PID,
								  psHWRInfo->ui32FrameNum,
								  psHWRInfo->ui32ActiveHWRTData,
								  psHWRInfo->ui32EventStatus,
								  psHWRInfo->ui64CRTimer,
								  pszLockupType));

				switch(psHWRInfo->eHWRType)
				{
					case RGX_HWRTYPE_BIF0FAILURE:
					case RGX_HWRTYPE_BIF1FAILURE:
					{
						_RGXDumpRGXBIFBank(pfnDumpDebugPrintf, psDevInfo, RGXFWIF_HWRTYPE_BIF_BANK_GET(psHWRInfo->eHWRType),
										psHWRInfo->uHWRData.sBIFInfo.ui64BIFMMUStatus,
										psHWRInfo->uHWRData.sBIFInfo.ui64BIFReqStatus,
										IMG_FALSE);
					}
					break;

					case RGX_HWRTYPE_POLLFAILURE:
					{
						PVR_DUMPDEBUG_LOG(("    T%u polling %s (reg:0x%08X val:0x%08X)",
										  psHWRInfo->uHWRData.sPollInfo.ui32ThreadNum,
										  ((psHWRInfo->uHWRData.sPollInfo.ui32CrPollAddr & RGXFW_POLL_TYPE_SET)?("set"):("unset")),
										  psHWRInfo->uHWRData.sPollInfo.ui32CrPollAddr & ~RGXFW_POLL_TYPE_SET,
										  psHWRInfo->uHWRData.sPollInfo.ui32CrPollValue));
					}
					break;

					case RGX_HWRTYPE_OVERRUN:
					case RGX_HWRTYPE_UNKNOWNFAILURE:
					{
						/* Nothing to dump */
					}
					break;

					default:
					{
						PVR_ASSERT(IMG_FALSE);
					}
					break;
				}
			}

			if(ui32ReadIndex == RGXFWIF_HWINFO_MAX_FIRST - 1)
							ui32ReadIndex = psHWInfoBuf->ui32WriteIndex;
			else
				ui32ReadIndex = (ui32ReadIndex + 1) - (ui32ReadIndex / RGXFWIF_HWINFO_LAST_INDEX) * RGXFWIF_HWINFO_MAX_LAST;
		}
	}	
}

/*!
*******************************************************************************

 @Function	_RGXDumpRGXDebugSummary

 @Description

 Dump a summary in human readable form with the RGX state

 @Input psDevInfo	 - RGX device info

 @Return   IMG_VOID

******************************************************************************/
static IMG_VOID _RGXDumpRGXDebugSummary(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                        PVRSRV_RGXDEV_INFO *psDevInfo, IMG_BOOL bRGXPoweredON)
{
	IMG_CHAR *pszState;
	RGXFWIF_TRACEBUF *psRGXFWIfTraceBuf = psDevInfo->psRGXFWIfTraceBuf;

	if (bRGXPoweredON)
	{
		IMG_UINT64	ui64RegValMMUStatus, ui64RegValREQStatus;

		ui64RegValMMUStatus = OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_FAULT_BANK0_MMU_STATUS);
		ui64RegValREQStatus = OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_FAULT_BANK0_REQ_STATUS);

		_RGXDumpRGXBIFBank(pfnDumpDebugPrintf, psDevInfo, 0, ui64RegValMMUStatus, ui64RegValREQStatus, IMG_TRUE);

		ui64RegValMMUStatus = OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_FAULT_BANK1_MMU_STATUS);
		ui64RegValREQStatus = OSReadHWReg64(psDevInfo->pvRegsBaseKM, RGX_CR_BIF_FAULT_BANK1_REQ_STATUS);

		_RGXDumpRGXBIFBank(pfnDumpDebugPrintf, psDevInfo, 1, ui64RegValMMUStatus, ui64RegValREQStatus, IMG_TRUE);
	}

	/* Firmware state */
	switch (psDevInfo->psDeviceNode->eHealthStatus)
	{
		case PVRSRV_DEVICE_HEALTH_STATUS_OK:
		{
			pszState = "OK";
			break;
		}
		
		case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
		{
			pszState = "DEAD";
			break;
		}
		
		default:
		{
			pszState = "UNKNOWN";
			break;
		}
	}

	if (psRGXFWIfTraceBuf == IMG_NULL)
	{
		PVR_DUMPDEBUG_LOG(("RGX FW State: %s", pszState));

		/* can't dump any more information */
		return;
	}
	
	PVR_DUMPDEBUG_LOG(("RGX FW State: %s (HWRState 0x%08x)", pszState, psRGXFWIfTraceBuf->ui32HWRStateFlags));
	PVR_DUMPDEBUG_LOG(("RGX FW Power State: %s (APM %s: %d ok, %d denied, %d other, %d total)", 
	                  pszPowStateName[psRGXFWIfTraceBuf->ePowState],
	                  (psDevInfo->pfnActivePowerCheck)?"enabled":"disabled",
	                  psDevInfo->ui32ActivePMReqOk,
	                  psDevInfo->ui32ActivePMReqDenied,
	                  psDevInfo->ui32ActivePMReqTotal - psDevInfo->ui32ActivePMReqOk - psDevInfo->ui32ActivePMReqDenied,
	                  psDevInfo->ui32ActivePMReqTotal));


	_RGXDumpFWAssert(pfnDumpDebugPrintf, psRGXFWIfTraceBuf);

	_RGXDumpFWPoll(pfnDumpDebugPrintf, psRGXFWIfTraceBuf);

	_RGXDumpFWHWRInfo(pfnDumpDebugPrintf, psRGXFWIfTraceBuf, psDevInfo);

}

static IMG_VOID _RGXDumpMetaSPExtraDebugInfo(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                             PVRSRV_RGXDEV_INFO *psDevInfo)
{
/* List of extra META Slave Port debug registers */
#define RGX_META_SP_EXTRA_DEBUG \
			X(RGX_CR_META_SP_MSLVCTRL0) \
			X(RGX_CR_META_SP_MSLVCTRL1) \
			X(RGX_CR_META_SP_MSLVIRQSTATUS) \
			X(RGX_CR_META_SP_MSLVIRQENABLE) \
			X(RGX_CR_META_SP_MSLVIRQLEVEL)

	IMG_UINT32 ui32Idx, ui32RegIdx;
	IMG_UINT32 ui32RegVal;
	IMG_UINT32 ui32RegAddr;

	const IMG_UINT32 aui32DebugRegAddr [] = {
#define X(A) A,
		RGX_META_SP_EXTRA_DEBUG
#undef X
		};

	const IMG_CHAR* apszDebugRegName [] = {
#define X(A) #A,
	RGX_META_SP_EXTRA_DEBUG
#undef X
	};
	
	const IMG_UINT32 aui32Debug2RegAddr [] = {0xA28, 0x0A30, 0x0A38};

	PVR_DUMPDEBUG_LOG(("META Slave Port extra debug:"));

	/* dump first set of Slave Port debug registers */
	for (ui32Idx = 0; ui32Idx < sizeof(aui32DebugRegAddr)/sizeof(IMG_UINT32); ui32Idx++)
	{
		const IMG_CHAR* pszRegName = apszDebugRegName[ui32Idx];

		ui32RegAddr = aui32DebugRegAddr[ui32Idx];
		ui32RegVal = OSReadHWReg32(psDevInfo->pvRegsBaseKM, ui32RegAddr);
		PVR_DUMPDEBUG_LOG(("  * %s: 0x%8.8X", pszRegName, ui32RegVal));
	}

	/* dump second set of Slave Port debug registers */
	for (ui32Idx = 0; ui32Idx < 4; ui32Idx++)
	{
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, 0xA20, ui32Idx);
		ui32RegVal = OSReadHWReg32(psDevInfo->pvRegsBaseKM, 0xA20);
		PVR_DUMPDEBUG_LOG(("  * 0xA20[%d]: 0x%8.8X", ui32Idx, ui32RegVal));

	}

	for (ui32RegIdx = 0; ui32RegIdx < sizeof(aui32Debug2RegAddr)/sizeof(IMG_UINT32); ui32RegIdx++)
	{
		ui32RegAddr = aui32Debug2RegAddr[ui32RegIdx];
		for (ui32Idx = 0; ui32Idx < 2; ui32Idx++)
		{
			OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32RegAddr, ui32Idx);
			ui32RegVal = OSReadHWReg32(psDevInfo->pvRegsBaseKM, ui32RegAddr);
			PVR_DUMPDEBUG_LOG(("  * 0x%X[%d]: 0x%8.8X", ui32RegAddr, ui32Idx, ui32RegVal));
		}
	}

}

/*
	RGXDumpDebugInfo
*/
IMG_VOID RGXDumpDebugInfo(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                          PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	IMG_UINT32 i;
	PVRSRV_ERROR eError;
	eError = PVRSRVPowerLock();
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXDumpDebugInfo: Failed to acquire power lock"));
		return;
	}

	for(i=0;i<=DEBUG_REQUEST_VERBOSITY_MAX;i++)
	{
		RGXDebugRequestProcess(pfnDumpDebugPrintf, psDevInfo, i);
	}
	PVRSRVPowerUnlock();
}


#if defined(PVRSRV_ENABLE_FW_TRACE_DEBUGFS)
/*
 *  Array of all the Firmware Trace log IDs used to convert the trace data.
 */
typedef struct _TRACEBUF_LOG_ {
	RGXFW_LOG_SFids	 eSFId;
	IMG_CHAR		 *pszName;
	IMG_CHAR		 *pszFmt;
	IMG_UINT32		 ui32ArgNum;
} TRACEBUF_LOG;

TRACEBUF_LOG aLogDefinitions[] = {
#define X(a, b, c, d, e) {RGXFW_LOG_CREATESFID(a,b,e), #c, d, e},
	RGXFW_LOG_SFIDLIST 
#undef X
};

#define NARGS_MASK ~(0xF<<16)
static IMG_BOOL _FirmwareTraceIntegrityCheck(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
	TRACEBUF_LOG  *psLogDef    = &aLogDefinitions[0];
	IMG_BOOL      bIntegrityOk = IMG_TRUE;

	/*
	 *  For every log ID, check the format string and number of arguments is valid.
	 */
	while (psLogDef->eSFId != RGXFW_SF_LAST)
	{
		IMG_UINT32    ui32Count;
		IMG_CHAR      *pszString;
		TRACEBUF_LOG  *psLogDef2;

		/*
		 * Check the number of arguments matches the number of '%' in the string and
		 * check that no string uses %s which is not supported as it requires a
		 * pointer to memory that is not going to be valid.
		 */
		pszString = psLogDef->pszFmt;
		ui32Count = 0;
		
		while (*pszString != '\0')
		{
			if (*pszString++ == '%')
			{
				ui32Count++;
				if (*pszString == 's')
				{
					bIntegrityOk = IMG_FALSE;
					PVR_DUMPDEBUG_LOG(("Integrity Check FAIL: %s has an unsupported type not recognized (fmt: %%%c). Please fix.",
									  psLogDef->pszName, *pszString));
				}
				else if (*pszString == '%')
				{
					/* Double % is a printable % sign and not a format string... */
					ui32Count--;
				}
			}
		}
		
		if (ui32Count != psLogDef->ui32ArgNum)
		{
			bIntegrityOk = IMG_FALSE;
			PVR_DUMPDEBUG_LOG(("Integrity Check FAIL: %s has %d arguments but only %d are specified. Please fix.",
			                  psLogDef->pszName, ui32Count, psLogDef->ui32ArgNum));
		}

		/* RGXDumpFirmwareTrace() has a hardcoded limit of supporting up to 20 arguments... */
		if (ui32Count > 20)
		{
			bIntegrityOk = IMG_FALSE;
			PVR_DUMPDEBUG_LOG(("Integrity Check FAIL: %s has %d arguments but a maximum of 20 are supported. Please fix.",
			                  psLogDef->pszName, ui32Count));
		}

		/* Check the id number is unique (don't take into account the number of arguments) */
		ui32Count = 0;
		psLogDef2 = &aLogDefinitions[0];

		while (psLogDef2->eSFId != RGXFW_SF_LAST)
		{
			if ((psLogDef->eSFId & NARGS_MASK) == (psLogDef2->eSFId & NARGS_MASK))
			{
				ui32Count++;
			}
			psLogDef2++;
		}

		if (ui32Count != 1)
		{
			bIntegrityOk = IMG_FALSE;
			PVR_DUMPDEBUG_LOG(("Integrity Check FAIL: %s id %x is not unique, there are %d more. Please fix.",
			                  psLogDef->pszName, psLogDef->eSFId, ui32Count - 1));
		}

		/* Move to the next log ID... */
		psLogDef++;
	}

	return bIntegrityOk;
}

IMG_VOID RGXDumpFirmwareTrace(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                              PVRSRV_RGXDEV_INFO  *psDevInfo)
{
	RGXFWIF_TRACEBUF  *psRGXFWIfTraceBufCtl = psDevInfo->psRGXFWIfTraceBuf;
	static IMG_BOOL   bIntegrityCheckPassed = IMG_FALSE;

	/* Check that the firmware trace is correctly defined... */
	if (!bIntegrityCheckPassed)
	{
		bIntegrityCheckPassed = _FirmwareTraceIntegrityCheck(pfnDumpDebugPrintf);
		if (!bIntegrityCheckPassed)
		{
			return;
		}
	}

	/* Dump FW trace information... */
	if (psRGXFWIfTraceBufCtl != IMG_NULL)
	{
		IMG_CHAR    szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
		IMG_UINT32  tid;
		
		/* Print the log type settings... */
		if (psRGXFWIfTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_GROUP_MASK)
		{
			PVR_DUMPDEBUG_LOG(("Debug log type: %s ( " RGXFWIF_LOG_ENABLED_GROUPS_LIST_PFSPEC ")",
							  ((psRGXFWIfTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_TRACE)?("trace"):("tbi")),
							  RGXFWIF_LOG_ENABLED_GROUPS_LIST(psRGXFWIfTraceBufCtl->ui32LogType)
							  ));
		}
		else
		{
			PVR_DUMPDEBUG_LOG(("Debug log type: none"));
		}

		/* Print the decoded log for each thread... */
		for (tid = 0;  tid < RGXFW_THREAD_NUM;  tid++) 
		{
			IMG_UINT32  *ui32TraceBuf = psRGXFWIfTraceBufCtl->sTraceBuf[tid].aui32TraceBuffer;
			IMG_UINT32  ui32TracePtr  = psRGXFWIfTraceBufCtl->sTraceBuf[tid].ui32TracePointer;
			IMG_UINT32  ui32Count     = 0;

			while (ui32Count < RGXFW_TRACE_BUFFER_SIZE)
			{
				IMG_UINT32  ui32Data, ui32DataToId;
				
				/* Find the first valid log ID, skipping whitespace... */
				do
				{
					ui32Data     = ui32TraceBuf[ui32TracePtr];
					ui32DataToId = idToStringID(ui32Data);

					/* If an unrecognized id is found check if it is valid, if it is tracebuf needs updating. */ 
					if (ui32DataToId == RGXFW_SF_LAST  &&  RGXFW_LOG_VALIDID(ui32Data))
					{
						PVR_DUMPDEBUG_LOG(("ERROR: Unrecognized id (%x). From here on the trace might be wrong!", ui32Data));
						return;
					}

					/* Update the trace pointer... */
					ui32TracePtr = (ui32TracePtr + 1) % RGXFW_TRACE_BUFFER_SIZE;
					ui32Count++;
				} while ((RGXFW_SF_LAST == ui32DataToId  ||  ui32DataToId >= RGXFW_SF_FIRST)  &&
				         ui32Count < RGXFW_TRACE_BUFFER_SIZE);

				if (ui32Count < RGXFW_TRACE_BUFFER_SIZE)
				{
					IMG_UINT64  ui64RGXTimer;
					
					/* If we hit the ASSERT message then this is the end of the log... */
					if (ui32Data == RGXFW_SF_MAIN_ASSERT_FAILED)
					{
						PVR_DUMPDEBUG_LOG(("ASSERTION %s failed at %s:%u",
										  psRGXFWIfTraceBufCtl->sTraceBuf[tid].sAssertBuf.szInfo,
										  psRGXFWIfTraceBufCtl->sTraceBuf[tid].sAssertBuf.szPath,
										  psRGXFWIfTraceBufCtl->sTraceBuf[tid].sAssertBuf.ui32LineNum));
						break;
					}

					/*
					 *  Print the trace string and provide up to 20 arguments which
					 *  printf function will be able to use. We have already checked
					 *  that no string uses more than this.
					 */
					OSStringCopy(szBuffer, "%llu:T%u-%s> ");
					OSStringCopy(&szBuffer[OSStringLength(szBuffer)], SFs[ui32DataToId].name);
					szBuffer[OSStringLength(szBuffer)-1] = '\0';
					ui64RGXTimer = (IMG_UINT64)(ui32TraceBuf[(ui32TracePtr +  0) % RGXFW_TRACE_BUFFER_SIZE]) << 32 |
					               (IMG_UINT64)(ui32TraceBuf[(ui32TracePtr +  1) % RGXFW_TRACE_BUFFER_SIZE]);
					PVR_DUMPDEBUG_LOG((szBuffer, ui64RGXTimer, tid, groups[RGXFW_SF_GID(ui32Data)],
									  ui32TraceBuf[(ui32TracePtr +  2) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  3) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  4) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  5) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  6) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  7) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  8) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr +  9) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 10) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 11) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 12) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 13) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 14) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 15) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 16) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 17) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 18) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 19) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 20) % RGXFW_TRACE_BUFFER_SIZE],
									  ui32TraceBuf[(ui32TracePtr + 21) % RGXFW_TRACE_BUFFER_SIZE]));

					/* Update the trace pointer... */
					ui32TracePtr = (ui32TracePtr + 2 + RGXFW_SF_PARAMNUM(ui32Data)) % RGXFW_TRACE_BUFFER_SIZE;
					ui32Count    = (ui32Count    + 2 + RGXFW_SF_PARAMNUM(ui32Data));
				}
			}
		}
	}
}
#endif


static IMG_CHAR* _RGXGetDebugDevPowerStateString(PVRSRV_DEV_POWER_STATE ePowerState)
{
	switch(ePowerState)
	{
		case PVRSRV_DEV_POWER_STATE_DEFAULT: return "DEFAULT";
		case PVRSRV_DEV_POWER_STATE_OFF: return "OFF";
		case PVRSRV_DEV_POWER_STATE_ON: return "ON";
		default: return "UNKNOWN";
	}
}

IMG_VOID RGXDebugRequestProcess(DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                                PVRSRV_RGXDEV_INFO	*psDevInfo,
                                IMG_UINT32			ui32VerbLevel)
{

	switch (ui32VerbLevel)
	{
		case DEBUG_REQUEST_VERBOSITY_LOW :
		{
			PVRSRV_ERROR            eError;
			IMG_UINT32              ui32DeviceIndex;
			PVRSRV_DEV_POWER_STATE  ePowerState;
			IMG_BOOL                bRGXPoweredON;

			ui32DeviceIndex = psDevInfo->psDeviceNode->sDevId.ui32DeviceIndex;

			eError = PVRSRVGetDevicePowerState(ui32DeviceIndex, &ePowerState);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "RGXDebugRequestProcess: Error retrieving RGX power state. No debug info dumped."));
				return;
			}

			bRGXPoweredON = (ePowerState == PVRSRV_DEV_POWER_STATE_ON);

			PVR_DUMPDEBUG_LOG(("------[ RGX summary ]------"));
			PVR_DUMPDEBUG_LOG(("RGX BVNC: %s", RGX_BVNC_KM));
			PVR_DUMPDEBUG_LOG(("RGX Power State: %s", _RGXGetDebugDevPowerStateString(ePowerState)));

			_RGXDumpRGXDebugSummary(pfnDumpDebugPrintf, psDevInfo, bRGXPoweredON);

			if (bRGXPoweredON)
			{

				PVR_DUMPDEBUG_LOG(("------[ RGX registers ]------"));
				PVR_DUMPDEBUG_LOG(("RGX Register Base Address (Linear):   0x%p", psDevInfo->pvRegsBaseKM));
				PVR_DUMPDEBUG_LOG(("RGX Register Base Address (Physical): 0x%08lX", (unsigned long)psDevInfo->sRegsPhysBase.uiAddr));

				/* Forcing bit 6 of MslvCtrl1 to 0 to avoid internal reg read going though the core */
				OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVCTRL1, 0x0);

				eError = RGXRunScript(psDevInfo, psDevInfo->psScripts->asDbgCommands, RGX_MAX_INIT_COMMANDS, PDUMP_FLAGS_CONTINUOUS, pfnDumpDebugPrintf);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_WARNING,"RGXDebugRequestProcess: RGXRunScript failed (%d) - Retry", eError));

					/* use thread1 for slave port accesses */
					OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVCTRL1, 0x1 << RGX_CR_META_SP_MSLVCTRL1_THREAD_SHIFT);

					eError = RGXRunScript(psDevInfo, psDevInfo->psScripts->asDbgCommands, RGX_MAX_INIT_COMMANDS, PDUMP_FLAGS_CONTINUOUS, pfnDumpDebugPrintf);
					if (eError != PVRSRV_OK)
					{
						PVR_DPF((PVR_DBG_ERROR,"RGXDebugRequestProcess: RGXRunScript retry failed (%d) - Dump Slave Port debug information", eError));
						_RGXDumpMetaSPExtraDebugInfo(pfnDumpDebugPrintf, psDevInfo);
					}

					/* use thread0 again */
					OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVCTRL1, 0x0 << RGX_CR_META_SP_MSLVCTRL1_THREAD_SHIFT);
				}
			}
			else
			{
				PVR_DUMPDEBUG_LOG((" (!) RGX power is down. No registers dumped"));
			}

			{
				RGXFWIF_DM	eKCCBType;
				
				/*
		 			Dump out the kernel CCBs.
		 		*/
				for (eKCCBType = 0; eKCCBType < RGXFWIF_DM_MAX; eKCCBType++)
				{
					RGXFWIF_CCB_CTL	*psKCCBCtl = psDevInfo->apsKernelCCBCtl[eKCCBType];
		
					if (psKCCBCtl != IMG_NULL)
					{
						PVR_DUMPDEBUG_LOG(("RGX Kernel CCB %u WO:0x%X RO:0x%X",
						                  eKCCBType, psKCCBCtl->ui32WriteOffset, psKCCBCtl->ui32ReadOffset));
					}
				}
		 	}

		 	/* Dump the IRQ info */
			{
				PVR_DUMPDEBUG_LOG(("RGX FW IRQ count = %d, last sampled in MISR = %d",
				                  psDevInfo->psRGXFWIfTraceBuf->ui32InterruptCount,
				                  g_ui32HostSampleIRQCount));
			}

			/* Dump the FW config flags */
			{
				RGXFWIF_INIT		*psRGXFWInit;

				eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc,
						(IMG_VOID **)&psRGXFWInit);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR,"RGXDebugRequestProcess: Failed to acquire kernel fw if ctl (%u)",
								eError));
					return;
				}

				PVR_DUMPDEBUG_LOG(("RGX FW config flags = 0x%X", psRGXFWInit->ui32ConfigFlags));

				DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfInitMemDesc);
			}

			break;

		}
		case DEBUG_REQUEST_VERBOSITY_MEDIUM :
		{
			IMG_INT tid;
			/* Dump FW trace information */
			if (psDevInfo->psRGXFWIfTraceBuf != IMG_NULL)
			{
				RGXFWIF_TRACEBUF *psRGXFWIfTraceBufCtl = psDevInfo->psRGXFWIfTraceBuf;
		
				for ( tid = 0 ; tid < RGXFW_THREAD_NUM ; tid++) 
				{
					IMG_UINT32	i;
					IMG_BOOL	bPrevLineWasZero = IMG_FALSE;
					IMG_BOOL	bLineIsAllZeros = IMG_FALSE;
					IMG_UINT32	ui32CountLines = 0;
					IMG_UINT32	*pui32TraceBuffer;
					IMG_CHAR	*pszLine;
		
					pui32TraceBuffer = &psRGXFWIfTraceBufCtl->sTraceBuf[tid].aui32TraceBuffer[0];
		
					/* each element in the line is 8 characters plus a space.  The '+1' is because of the final trailing '\0'. */
					pszLine = OSAllocMem(9*RGXFW_TRACE_BUFFER_LINESIZE+1);
					if (pszLine == IMG_NULL)
					{
						PVR_DPF((PVR_DBG_ERROR,"RGXDebugRequestProcess: Out of mem allocating line string (size: %d)", 9*RGXFW_TRACE_BUFFER_LINESIZE));
						return;
					}
		
					/* Print the tracepointer */
					if (psRGXFWIfTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_GROUP_MASK)
					{
						PVR_DUMPDEBUG_LOG(("Debug log type: %s ( " RGXFWIF_LOG_ENABLED_GROUPS_LIST_PFSPEC ")",
						                  ((psRGXFWIfTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_TRACE)?("trace"):("tbi")),
						                  RGXFWIF_LOG_ENABLED_GROUPS_LIST(psRGXFWIfTraceBufCtl->ui32LogType)
						                  ));
					}
					else
					{
						PVR_DUMPDEBUG_LOG(("Debug log type: none"));
					}
					
					PVR_DUMPDEBUG_LOG(("------[ RGX FW thread %d trace START ]------", tid));
					PVR_DUMPDEBUG_LOG(("FWT[traceptr]: %X", psRGXFWIfTraceBufCtl->sTraceBuf[tid].ui32TracePointer));
		
					for (i = 0; i < RGXFW_TRACE_BUFFER_SIZE; i += RGXFW_TRACE_BUFFER_LINESIZE)
					{
						IMG_UINT32 k = 0;
						IMG_UINT32 ui32Line = 0x0;
						IMG_UINT32 ui32LineOffset = i*sizeof(IMG_UINT32);
						IMG_CHAR   *pszBuf = pszLine;
		
						for (k = 0; k < RGXFW_TRACE_BUFFER_LINESIZE; k++)
						{
							ui32Line |= pui32TraceBuffer[i + k];
		
							/* prepare the line to print it. The '+1' is because of the trailing '\0' added */
							OSSNPrintf(pszBuf, 9 + 1, " %08x", pui32TraceBuffer[i + k]);
							pszBuf += 9; /* write over the '\0' */
						}
		
						bLineIsAllZeros = (ui32Line == 0x0);
		
						if (bLineIsAllZeros && bPrevLineWasZero)
						{
							ui32CountLines++;
						}
						else if (bLineIsAllZeros && !bPrevLineWasZero)
						{
							bPrevLineWasZero = IMG_TRUE;
							ui32CountLines = 0;
							PVR_DUMPDEBUG_LOG(("FWT[%08x]: 00000000 ... 00000000", ui32LineOffset));
						}
						else
						{
							if (bPrevLineWasZero)
							{
								PVR_DUMPDEBUG_LOG(("FWT[%08x]: %d lines were all zero", ui32LineOffset, ui32CountLines));
							}
							else
							{
		
								PVR_DUMPDEBUG_LOG(("FWT[%08x]:%s", ui32LineOffset, pszLine));
							}
							bPrevLineWasZero = IMG_FALSE;
						}
		
					}
					if (bPrevLineWasZero)
					{
						PVR_DUMPDEBUG_LOG(("FWT[END]: %d lines were all zero", ui32CountLines));
					}
		
					PVR_DUMPDEBUG_LOG(("------[ RGX FW thread %d trace END ]------", tid));
		
					OSFreeMem(pszLine);
				}

				{
					PVR_DUMPDEBUG_LOG(("------[ Stalled FWCtxs ]------"));

					if (psDevInfo)
					{
						CheckForStalledTransferCtxt(psDevInfo);
						CheckForStalledRenderCtxt(psDevInfo);
#if !defined(UNDER_WDDM)
						CheckForStalledComputeCtxt(psDevInfo);
#endif
#if defined(RGX_FEATURE_RAY_TRACING)
						CheckForStalledRayCtxt(psDevInfo);
#endif
					}
				}

			}
			break;
		}
		case DEBUG_REQUEST_VERBOSITY_HIGH:
		{
			PVRSRV_ERROR            eError;
			IMG_UINT32              ui32DeviceIndex;
			PVRSRV_DEV_POWER_STATE  ePowerState;
			IMG_BOOL                bRGXPoweredON;

			ui32DeviceIndex = psDevInfo->psDeviceNode->sDevId.ui32DeviceIndex;

			eError = PVRSRVGetDevicePowerState(ui32DeviceIndex, &ePowerState);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "RGXDebugRequestProcess: Error retrieving RGX power state. No debug info dumped."));
				return;
			}

			bRGXPoweredON = (ePowerState == PVRSRV_DEV_POWER_STATE_ON);

			PVR_DUMPDEBUG_LOG(("------[ Debug bus ]------"));

			_RGXDumpRGXDebugSummary(pfnDumpDebugPrintf, psDevInfo, bRGXPoweredON);

			if (bRGXPoweredON)
			{
				eError = RGXRunScript(psDevInfo, psDevInfo->psScripts->asDbgBusCommands, RGX_MAX_DBGBUS_COMMANDS, PDUMP_FLAGS_CONTINUOUS, pfnDumpDebugPrintf);
				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_WARNING,"RGXDebugRequestProcess: RGXRunScript failed (%s)", PVRSRVGetErrorStringKM(eError)));
				}
				break;
			}
		}
		default:
			break;
	}
}

/*
	RGXPanic
*/
IMG_VOID RGXPanic(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	PVR_LOG(("RGX panic"));
	PVRSRVDebugRequest(DEBUG_REQUEST_VERBOSITY_MAX);
	OSPanic();
}

/*
	RGXQueryDMState
*/
PVRSRV_ERROR RGXQueryDMState(PVRSRV_RGXDEV_INFO *psDevInfo, RGXFWIF_DM eDM, RGXFWIF_DM_STATE *peState, RGXFWIF_DEV_VIRTADDR *psCommonContextDevVAddr)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	RGXFWIF_TRACEBUF *psRGXFWIfTraceBufCtl = psDevInfo->psRGXFWIfTraceBuf;

	if (eDM >= RGXFWIF_DM_MAX)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,"RGXQueryDMState: eDM parameter is out of range (%u)",eError));
		return eError;
	}

	if (peState == IMG_NULL)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,"RGXQueryDMState: peState is NULL (%u)",eError));
		return eError;
	}

	if (psCommonContextDevVAddr == IMG_NULL)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		PVR_DPF((PVR_DBG_ERROR,"RGXQueryDMState: psCommonContextDevVAddr is NULL (%u)",eError));
		return eError;
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"RGXQueryDMState: Failed (%d) to acquire address for trace buffer", eError));
		return eError;
	}

	if (psRGXFWIfTraceBufCtl->apsHwrDmFWCommonContext[eDM].ui32Addr)
	{
		*peState = RGXFWIF_DM_STATE_LOCKEDUP;
	}
	else
	{
		*peState = RGXFWIF_DM_STATE_NORMAL;
	}
	
	*psCommonContextDevVAddr = psRGXFWIfTraceBufCtl->apsHwrDmFWCommonContext[eDM];

	return eError;
}


/******************************************************************************
 End of file (rgxdebug.c)
******************************************************************************/
