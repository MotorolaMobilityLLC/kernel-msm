/*************************************************************************/ /*!
@File
@Title		Common PDump functions
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

/* 
    This file has a mixture of top-level exported API functions, and
    low level functions used by services.  

    FIXME:
    This makes enforcement of a strict hierarchy impossible, and nasty 
    circular references are inevitable.
    Separate out these functions into two levels.
*/

   
#if defined(PDUMP)
#include <stdarg.h>

#include "allocmem.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "pvr_debug.h"
#include "srvkm.h"
#include "pdump_physmem.h"
#include "hash.h"
#include "connection_server.h"
#include "sync_server.h"

/* pdump headers */
#include "pdump_osfunc.h"
#include "pdump_km.h"
#include "pdump_int.h"

/* Allow temporary buffer size override */
#if !defined(PDUMP_TEMP_BUFFER_SIZE)
#define PDUMP_TEMP_BUFFER_SIZE (64 * 1024U)
#endif

/* DEBUG */
#if 0
#define PDUMP_DBG(a)   PDumpOSDebugPrintf (a)
#else
#define PDUMP_DBG(a)
#endif


#define	PTR_PLUS(t, p, x) ((t)(((IMG_CHAR *)(p)) + (x)))
#define	VPTR_PLUS(p, x) PTR_PLUS(IMG_VOID *, p, x)
#define	VPTR_INC(p, x) ((p) = VPTR_PLUS(p, x))
#define MAX_PDUMP_MMU_CONTEXTS	(32)
static IMG_VOID *gpvTempBuffer = IMG_NULL;

#define PERSISTANT_MAGIC ((IMG_UINTPTR_T) 0xe33ee33e)

#define PDUMP_PERSISTENT_HASH_SIZE 10
static HASH_TABLE *g_psPersistentHash = IMG_NULL;

#if defined(PDUMP_DEBUG_OUTFILES)
/* counter increments each time debug write is called */
IMG_UINT32 g_ui32EveryLineCounter = 1U;
#endif

#if defined(PDUMP_DEBUG) || defined(REFCOUNT_DEBUG)
#define PDUMP_REFCOUNT_PRINT(fmt, ...) PVRSRVDebugPrintf(PVR_DBG_WARNING, __FILE__, __LINE__, fmt, __VA_ARGS__)
#else
#define PDUMP_REFCOUNT_PRINT(fmt, ...)
#endif

struct _PDUMP_CONNECTION_DATA_ {
	IMG_UINT32				ui32RefCount;
	POS_LOCK				hLock;
	DLLIST_NODE				sListHead;
	IMG_BOOL				bLastInto;
	IMG_UINT32				ui32LastSetFrameNumber;
	IMG_BOOL				bWasInCaptureRange;
	IMG_BOOL				bIsInCaptureRange;
	IMG_BOOL				bLastTransitionFailed;
	SYNC_CONNECTION_DATA	*psSyncConnectionData;
};

static PDUMP_CONNECTION_DATA * _PDumpConnectionAcquire(PDUMP_CONNECTION_DATA *psPDumpConnectionData)
{
	IMG_UINT32 ui32RefCount;

	OSLockAcquire(psPDumpConnectionData->hLock);
	ui32RefCount = ++psPDumpConnectionData->ui32RefCount;
	OSLockRelease(psPDumpConnectionData->hLock);

	PDUMP_REFCOUNT_PRINT("%s: PDump connection %p, refcount = %d",
						 __FUNCTION__, psPDumpConnectionData, ui32RefCount);

	return psPDumpConnectionData;
}

static IMG_VOID _PDumpConnectionRelease(PDUMP_CONNECTION_DATA *psPDumpConnectionData)
{
	IMG_UINT32 ui32RefCount;

	OSLockAcquire(psPDumpConnectionData->hLock);
	ui32RefCount = --psPDumpConnectionData->ui32RefCount;
	OSLockRelease(psPDumpConnectionData->hLock);

	if (ui32RefCount == 0)
	{
		OSLockDestroy(psPDumpConnectionData->hLock);
		PVR_ASSERT(dllist_is_empty(&psPDumpConnectionData->sListHead));
		OSFreeMem(psPDumpConnectionData);
	}

	PDUMP_REFCOUNT_PRINT("%s: PDump connection %p, refcount = %d",
						 __FUNCTION__, psPDumpConnectionData, ui32RefCount);
}

#ifdef INLINE_IS_PRAGMA
#pragma inline(PDumpIsPersistent)
#endif

IMG_BOOL PDumpIsPersistent(IMG_VOID)
{
	IMG_PID uiPID = OSGetCurrentProcessIDKM();
	IMG_UINTPTR_T puiRetrieve;

	puiRetrieve = HASH_Retrieve(g_psPersistentHash, uiPID);
	if (puiRetrieve != 0)
	{
		PVR_ASSERT(puiRetrieve == PERSISTANT_MAGIC);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}


/**************************************************************************
 * Function Name  : GetTempBuffer
 * Inputs         : None
 * Outputs        : None
 * Returns        : Temporary buffer address, or IMG_NULL
 * Description    : Get temporary buffer address.
**************************************************************************/
static IMG_VOID *GetTempBuffer(IMG_VOID)
{
	/*
	 * Allocate the temporary buffer, it it hasn't been allocated already.
	 * Return the address of the temporary buffer, or IMG_NULL if it
	 * couldn't be allocated.
	 * It is expected that the buffer will be allocated once, at driver
	 * load time, and left in place until the driver unloads.
	 */

	if (gpvTempBuffer == IMG_NULL)
	{
		gpvTempBuffer = OSAllocMem(PDUMP_TEMP_BUFFER_SIZE);
		if (gpvTempBuffer == IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "GetTempBuffer: OSAllocMem failed"));
		}
	}

	return gpvTempBuffer;
}

static IMG_VOID FreeTempBuffer(IMG_VOID)
{

	if (gpvTempBuffer != IMG_NULL)
	{
		OSFreeMem(gpvTempBuffer);
		gpvTempBuffer = IMG_NULL;
	}
}

PVRSRV_ERROR PDumpInitCommon(IMG_VOID)
{
	PVRSRV_ERROR eError;

	/* Allocate temporary buffer for copying from user space */
	(IMG_VOID) GetTempBuffer();

	g_psPersistentHash = HASH_Create(PDUMP_PERSISTENT_HASH_SIZE);
	if (g_psPersistentHash == IMG_NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* create the global PDump lock */
	eError = PDumpCreateLockKM();

	if(eError != PVRSRV_OK)
	{
		return eError;
	}

	/* Call environment specific PDump initialisation */
	PDumpInit();

	return PVRSRV_OK;
}

IMG_VOID PDumpDeInitCommon(IMG_VOID)
{
	/* Free temporary buffer */
	FreeTempBuffer();

	/* Call environment specific PDump Deinitialisation */
	PDumpDeInit();

	/* take down the global PDump lock */
	PDumpDestroyLockKM();
}

PVRSRV_ERROR PDumpAddPersistantProcess(IMG_VOID)
{
	IMG_PID uiPID = OSGetCurrentProcessIDKM();
	IMG_UINTPTR_T puiRetrieve;
	PVRSRV_ERROR eError = PVRSRV_OK;

	puiRetrieve = HASH_Retrieve(g_psPersistentHash, uiPID);
	if (puiRetrieve == 0)
	{
		if (!HASH_Insert(g_psPersistentHash, uiPID, PERSISTANT_MAGIC))
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}
	else
	{
		PVR_ASSERT(puiRetrieve == PERSISTANT_MAGIC);
	}

	return eError;
}

IMG_BOOL PDumpIsSuspended(IMG_VOID)
{
	return PDumpOSIsSuspended();
}

typedef struct _PDUMP_Transition_DATA_ {
	PFN_PDUMP_TRANSITION	pfnCallback;
	IMG_PVOID				hPrivData;
	PDUMP_CONNECTION_DATA	*psPDumpConnectionData;
	DLLIST_NODE				sNode;
} PDUMP_Transition_DATA;

PVRSRV_ERROR PDumpRegisterTransitionCallback(PDUMP_CONNECTION_DATA *psPDumpConnectionData,
											  PFN_PDUMP_TRANSITION pfnCallback,
											  IMG_PVOID hPrivData,
											  IMG_PVOID *ppvHandle)
{
	PDUMP_Transition_DATA *psData;
	PVRSRV_ERROR eError;

	psData = OSAllocMem(sizeof(*psData));
	if (psData == IMG_NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc;
	}

	/* Setup the callback and add it to the list for this process */
	psData->pfnCallback = pfnCallback;
	psData->hPrivData = hPrivData;
	dllist_add_to_head(&psPDumpConnectionData->sListHead, &psData->sNode);

	/* Take a reference on the connection so it doesn't get freed too early */
	psData->psPDumpConnectionData =_PDumpConnectionAcquire(psPDumpConnectionData);
	*ppvHandle = psData;

	return PVRSRV_OK;

fail_alloc:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

IMG_VOID PDumpUnregisterTransitionCallback(IMG_PVOID pvHandle)
{
	PDUMP_Transition_DATA *psData = pvHandle;

	dllist_remove_node(&psData->sNode);
	_PDumpConnectionRelease(psData->psPDumpConnectionData);
	OSFreeMem(psData);
}

typedef struct _PTCB_DATA_ {
	IMG_BOOL bInto;
	IMG_BOOL bContinuous;
	PVRSRV_ERROR eError;
} PTCB_DATA;

static IMG_BOOL _PDumpTransition(DLLIST_NODE *psNode, IMG_PVOID hData)
{
	PDUMP_Transition_DATA *psData = IMG_CONTAINER_OF(psNode, PDUMP_Transition_DATA, sNode);
	PTCB_DATA *psPTCBData = (PTCB_DATA *) hData;

	psPTCBData->eError = psData->pfnCallback(psData->hPrivData, psPTCBData->bInto, psPTCBData->bContinuous);
	if (psPTCBData->eError != PVRSRV_OK)
	{
		/* Got an error, break out of the loop */
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

PVRSRV_ERROR PDumpTransition(PDUMP_CONNECTION_DATA *psPDumpConnectionData, IMG_BOOL bInto, IMG_BOOL bContinuous)
{
	PTCB_DATA sPTCBData;

	/* Only call the callbacks if we've really done a Transition */
	if (bInto != psPDumpConnectionData->bLastInto)
	{
		/* We're Transitioning either into or out of capture range */
		sPTCBData.bInto = bInto;
		sPTCBData.bContinuous = bContinuous;
		sPTCBData.eError = PVRSRV_OK;
		dllist_foreach_node(&psPDumpConnectionData->sListHead, _PDumpTransition, &sPTCBData);
		if (sPTCBData.eError != PVRSRV_OK)
		{
			/* We failed so bail out leaving the state as it is ready for the retry */
			return sPTCBData.eError;
		}

		if (bInto)
		{
			SyncConnectionPDumpSyncBlocks(psPDumpConnectionData->psSyncConnectionData);
		}
		psPDumpConnectionData->bLastInto = bInto;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpIsCaptureFrameKM(IMG_BOOL *bIsCapturing)
{
	/* FIXME:
	 * WDDM has an extended init phase to work around RA suballocs not
     * having a PDump MALLOC.
     * As a consequence, the WDDM init phase contains some FW command submissions.
     * These commands were discarded due to being outside a capture range. In order
	 * to correctly PDump these commands the init phase is considered to be a
	 * capture range.
	 * Note that PDumpOSIsInitPhaseKM and DBGDrivIsInitPhase were invented for
	 * this purpose. They are not otherwise required.
	 */
	if(PDumpOSIsInitPhaseKM())
	{
		*bIsCapturing = IMG_TRUE;
	}
	else
	{
		*bIsCapturing = PDumpOSIsCaptureFrameKM();
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR _PDumpSetFrameKM(CONNECTION_DATA *psConnection, IMG_UINT32 ui32Frame)
{
	PDUMP_CONNECTION_DATA *psPDumpConnectionData = psConnection->psPDumpConnectionData;
	IMG_BOOL bWasInCaptureRange;
	IMG_BOOL bIsInCaptureRange;
	PVRSRV_ERROR eError;

	/*
		Note:
		As we can't test to see if the new frame will be in capture range
		before we set the frame number and we don't want to roll back
		the frame number if we fail then we have to save the "transient"
		data which decides if we're entering or exiting capture range
		along with a failure boolean so we know what to do on a retry
	*/
	if (psPDumpConnectionData->ui32LastSetFrameNumber != ui32Frame)
	{
		PDumpIsCaptureFrameKM(&bWasInCaptureRange);
		PDumpOSSetFrameKM(ui32Frame);
		PDumpIsCaptureFrameKM(&bIsInCaptureRange);
		psPDumpConnectionData->ui32LastSetFrameNumber = ui32Frame;

		/* Save the Transition data incase we fail the Transition */
		psPDumpConnectionData->bWasInCaptureRange = bWasInCaptureRange;
		psPDumpConnectionData->bIsInCaptureRange = bIsInCaptureRange;
	}
	else if (psPDumpConnectionData->bLastTransitionFailed)
	{
		/* Load the Transition data so we can try again */
		bWasInCaptureRange = psPDumpConnectionData->bWasInCaptureRange;
		bIsInCaptureRange = psPDumpConnectionData->bIsInCaptureRange;
	}

	if (!bWasInCaptureRange && bIsInCaptureRange)
	{
		eError = PDumpTransition(psPDumpConnectionData, IMG_TRUE, IMG_FALSE);
		if (eError != PVRSRV_OK)
		{
			goto fail_Transition;
		}
	}
	else if (bWasInCaptureRange && !bIsInCaptureRange)
	{
		eError = PDumpTransition(psPDumpConnectionData, IMG_FALSE, IMG_FALSE);
		if (eError != PVRSRV_OK)
		{
			goto fail_Transition;
		}
	}

	psPDumpConnectionData->bLastTransitionFailed = IMG_FALSE;
	return PVRSRV_OK;

fail_Transition:
	psPDumpConnectionData->bLastTransitionFailed = IMG_TRUE;
	return eError;
}

PVRSRV_ERROR PDumpSetFrameKM(CONNECTION_DATA *psConnection, IMG_UINT32 ui32Frame)
{
	PVRSRV_ERROR eError = PVRSRV_OK;	
	
	PDUMPCOMMENT("Set pdump frame %u (pre)", ui32Frame);

	eError = _PDumpSetFrameKM(psConnection, ui32Frame);

	PDUMPCOMMENT("Set pdump frame %u (post)", ui32Frame);
	
	return eError;
}

/**************************************************************************
 * Function Name  : PDumpReg32
 * Inputs         : pszPDumpDevName, Register offset, and value to write
 * Outputs        : None
 * Returns        : PVRSRV_ERROR
 * Description    : Create a PDUMP string, which represents a register write
**************************************************************************/
PVRSRV_ERROR PDumpReg32(IMG_CHAR	*pszPDumpRegName,
						IMG_UINT32	ui32Reg,
						IMG_UINT32	ui32Data,
						IMG_UINT32	ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpReg32"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "WRW :%s:0x%08X 0x%08X", pszPDumpRegName, ui32Reg, ui32Data);

	if (eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}


/**************************************************************************
 * Function Name  : PDumpReg64
 * Inputs         : pszPDumpDevName, Register offset, and value to write
 * Outputs        : None
 * Returns        : PVRSRV_ERROR
 * Description    : Create a PDUMP string, which represents a register write
**************************************************************************/
PVRSRV_ERROR PDumpReg64(IMG_CHAR	*pszPDumpRegName,
						IMG_UINT32	ui32Reg,
						IMG_UINT64	ui64Data,
						IMG_UINT32	ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpRegKM"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "WRW64 :%s:0x%08X 0x%010llX", pszPDumpRegName, ui32Reg, ui64Data);

	if (eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}


/**************************************************************************
 * Function Name  : PDumpLDW
 * Inputs         : pcBuffer -- buffer to send to register bank
 *                  ui32NumLoadBytes -- number of bytes in pcBuffer
 *                  pszDevSpaceName -- devspace for register bank
 *                  ui32Offset -- value of offset control register
 *                  ui32PDumpFlags -- flags to pass to PDumpOSWriteString
 * Outputs        : None
 * Returns        : PVRSRV_ERROR
 * Description    : Dumps the contents of pcBuffer to a .prm file and
 *                  writes an LDW directive to the pdump output.
 *                  NB: ui32NumLoadBytes must be divisible by 4
**************************************************************************/
PVRSRV_ERROR PDumpLDW(IMG_CHAR      *pcBuffer,
                      IMG_CHAR      *pszDevSpaceName,
                      IMG_UINT32    ui32OffsetBytes,
                      IMG_UINT32    ui32NumLoadBytes,
                      PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError;
	IMG_CHAR aszParamStreamFilename[PMR_MAX_PARAMSTREAM_FILENAME_LENGTH_DEFAULT];
	IMG_UINT32 ui32ParamStreamFileOffset;

	PDUMP_GET_SCRIPT_STRING()

	eError = PDumpWriteBuffer(pcBuffer,
	                          ui32NumLoadBytes,
	                          uiPDumpFlags,
	                          &aszParamStreamFilename[0],
	                          sizeof(aszParamStreamFilename),
	                          &ui32ParamStreamFileOffset);
	PVR_ASSERT(eError == PVRSRV_OK);


	uiPDumpFlags |= (PDumpIsPersistent()) ? PDUMP_FLAGS_PERSISTENT : 0;

	eError = PDumpOSBufprintf(hScript,
	                          ui32MaxLen,
	                          "LDW :%s:0x%x 0x%x 0x%x %s\n",
	                          pszDevSpaceName,
	                          ui32OffsetBytes,
	                          ui32NumLoadBytes / (IMG_UINT32)sizeof(IMG_UINT32),
	                          ui32ParamStreamFileOffset,
	                          aszParamStreamFilename);

	if(eError != PVRSRV_OK)
	{
		return eError;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, uiPDumpFlags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}

/**************************************************************************
 * Function Name  : PDumpSAW
 * Inputs         : pszDevSpaceName -- device space from which to output
 *                  ui32Offset -- offset value from register base
 *                  ui32NumSaveBytes -- number of bytes to output
 *                  pszOutfileName -- name of file to output to
 *                  ui32OutfileOffsetByte -- offset into output file to write
 *                  uiPDumpFlags -- flags to pass to PDumpOSWriteString
 * Outputs        : None
 * Returns        : PVRSRV_ERROR
 * Description    : Dumps the contents of a register bank into a file
 *                  NB: ui32NumSaveBytes must be divisible by 4
**************************************************************************/
PVRSRV_ERROR PDumpSAW(IMG_CHAR      *pszDevSpaceName,
                      IMG_UINT32    ui32HPOffsetBytes,
                      IMG_UINT32    ui32NumSaveBytes,
                      IMG_CHAR      *pszOutfileName,
                      IMG_UINT32    ui32OutfileOffsetByte,
                      PDUMP_FLAGS_T uiPDumpFlags)
{
	PVRSRV_ERROR eError;

	PDUMP_GET_SCRIPT_STRING()

	PVR_DPF((PVR_DBG_ERROR, "PDumpSAW\n"));

	uiPDumpFlags |= (PDumpIsPersistent()) ? PDUMP_FLAGS_PERSISTENT : 0;

	eError = PDumpOSBufprintf(hScript,
	                          ui32MaxLen,
	                          "SAW :%s:0x%x 0x%x 0x%x %s\n",
	                          pszDevSpaceName,
	                          ui32HPOffsetBytes,
	                          ui32NumSaveBytes / (IMG_UINT32)sizeof(IMG_UINT32),
	                          ui32OutfileOffsetByte,
	                          pszOutfileName);

	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpSAW PDumpOSBufprintf failed: eError=%u\n", eError));
		return eError;
	}

	PDUMP_LOCK();
	if(! PDumpOSWriteString2(hScript, uiPDumpFlags))
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpSAW PDumpOSWriteString2 failed!\n"));
	}
	PDUMP_UNLOCK();

	return PVRSRV_OK;
	
}


/**************************************************************************
 * Function Name  : PDumpRegPolKM
 * Inputs         : Description of what this register read is trying to do
 *					pszPDumpDevName
 *					Register offset
 *					expected value
 *					mask for that value
 * Outputs        : None
 * Returns        : None
 * Description    : Create a PDUMP string which represents a register read
 *					with the expected value
**************************************************************************/
PVRSRV_ERROR PDumpRegPolKM(IMG_CHAR				*pszPDumpRegName,
						   IMG_UINT32			ui32RegAddr, 
						   IMG_UINT32			ui32RegValue, 
						   IMG_UINT32			ui32Mask,
						   IMG_UINT32			ui32Flags,
						   PDUMP_POLL_OPERATOR	eOperator)
{
	/* Timings correct for linux and XP */
	/* FIXME: Timings should be passed in */
	#define POLL_DELAY			1000U
	#define POLL_COUNT_LONG		(2000000000U / POLL_DELAY)
	#define POLL_COUNT_SHORT	(1000000U / POLL_DELAY)

	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32PollCount;

	PDUMP_GET_SCRIPT_STRING();
	PDUMP_DBG(("PDumpRegPolKM"));
	if ( PDumpIsPersistent() )
	{
		/* Don't pdump-poll if the process is persistent */
		return PVRSRV_OK;
	}

	ui32PollCount = POLL_COUNT_LONG;

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "POL :%s:0x%08X 0x%08X 0x%08X %d %u %d",
							pszPDumpRegName, ui32RegAddr, ui32RegValue,
							ui32Mask, eOperator, ui32PollCount, POLL_DELAY);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}


/**************************************************************************
 * Function Name  : PDumpCommentKM
 * Inputs         : pszComment, ui32Flags
 * Outputs        : None
 * Returns        : None
 * Description    : Dumps a comment
**************************************************************************/
PVRSRV_ERROR PDumpCommentKM(IMG_CHAR *pszComment, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	IMG_CHAR pszCommentPrefix[] = "-- "; /* prefix for comments */
#if defined(PDUMP_DEBUG_OUTFILES)
	IMG_CHAR pszTemp[256];
#endif
	IMG_UINT32 ui32LenCommentPrefix;
	PDUMP_GET_SCRIPT_STRING();
	PDUMP_DBG(("PDumpCommentKM"));
#if defined(PDUMP_DEBUG_OUTFILES)
	/* include comments in the "extended" init phase.
	 * default is to ignore them.
	 */
	ui32Flags |= ( PDumpIsPersistent() ) ? PDUMP_FLAGS_PERSISTENT : 0;
#endif

	if((pszComment == IMG_NULL) || (PDumpOSBuflen(pszComment, ui32MaxLen) == 0))
	{
		/* PDumpOSVerifyLineEnding silently fails if pszComment is too short to
		   actually hold the line endings that it's trying to enforce, so
		   short circuit it and force safety */
		pszComment = "\n";
	}
	else
	{
		/* Put line ending sequence at the end if it isn't already there */
		PDumpOSVerifyLineEnding(pszComment, ui32MaxLen);
	}

	/* Length of string excluding terminating NULL character */
	ui32LenCommentPrefix = PDumpOSBuflen(pszCommentPrefix, sizeof(pszCommentPrefix));

	PDUMP_LOCK();

	/* Ensure output file is available for writing */
	if (!PDumpOSWriteString(PDumpOSGetStream(PDUMP_STREAM_SCRIPT2),
			  (IMG_UINT8*)pszCommentPrefix,
			  ui32LenCommentPrefix,
			  ui32Flags))
	{
		if(ui32Flags & PDUMP_FLAGS_CONTINUOUS)
		{
			eErr = PVRSRV_ERROR_PDUMP_BUFFER_FULL;
			goto ErrUnlock;
		}
		else
		{
			eErr = PVRSRV_ERROR_CMD_NOT_PROCESSED;
			goto ErrUnlock;
		}
	}
#if defined(PDUMP_DEBUG_OUTFILES)
	/* Prefix comment with PID and line number */
	eErr = PDumpOSSprintf(pszTemp, 256, "%u %u:%lu %s: %s",
		g_ui32EveryLineCounter,
		OSGetCurrentProcessIDKM(),
		(unsigned long)OSGetCurrentThreadIDKM(),
		OSGetCurrentProcessNameKM(),
		pszComment);

	/* Append the comment to the script stream */
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "%s",
		pszTemp);
#else
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "%s",
		pszComment);
#endif
	if( (eErr != PVRSRV_OK) &&
		(eErr != PVRSRV_ERROR_PDUMP_BUF_OVERFLOW))
	{
		goto ErrUnlock;
	}
	PDumpOSWriteString2(hScript, ui32Flags);

ErrUnlock:
	PDUMP_UNLOCK();
	return eErr;
}

/**************************************************************************
 * Function Name  : PDumpCommentWithFlags
 * Inputs         : psPDev - PDev for PDump device
 *				  : pszFormat - format string for comment
 *				  : ... - args for format string
 * Outputs        : None
 * Returns        : None
 * Description    : PDumps a comments
**************************************************************************/
PVRSRV_ERROR PDumpCommentWithFlags(IMG_UINT32 ui32Flags, IMG_CHAR * pszFormat, ...)
{
	PVRSRV_ERROR eErr;
	PDUMP_va_list ap;
	PDUMP_GET_MSG_STRING();

	/* Construct the string */
	PDUMP_va_start(ap, pszFormat);
	eErr = PDumpOSVSprintf(pszMsg, ui32MaxLen, pszFormat, ap);
	PDUMP_va_end(ap);

	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	return PDumpCommentKM(pszMsg, ui32Flags);
}

/**************************************************************************
 * Function Name  : PDumpComment
 * Inputs         : psPDev - PDev for PDump device
 *				  : pszFormat - format string for comment
 *				  : ... - args for format string
 * Outputs        : None
 * Returns        : None
 * Description    : PDumps a comments
**************************************************************************/
PVRSRV_ERROR PDumpComment(IMG_CHAR *pszFormat, ...)
{
	PVRSRV_ERROR eErr;
	PDUMP_va_list ap;
	PDUMP_GET_MSG_STRING();

	/* Construct the string */
	PDUMP_va_start(ap, pszFormat);
	eErr = PDumpOSVSprintf(pszMsg, ui32MaxLen, pszFormat, ap);
	PDUMP_va_end(ap);

	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	return PDumpCommentKM(pszMsg, PDUMP_FLAGS_CONTINUOUS);
}

/*************************************************************************/ /*!
 * Function Name  : PDumpPanic
 * Inputs         : ui32PanicNo - Unique number for panic condition
 *				  : pszPanicMsg - Panic reason message limited to ~90 chars
 *				  : pszPPFunc   - Function name string where panic occurred
 *				  : ui32PPline  - Source line number where panic occurred
 * Outputs        : None
 * Returns        : PVRSRV_ERROR
 * Description    : PDumps a panic assertion. Used when the host driver
 *                : detects a condition that will lead to an invalid PDump
 *                : script that cannot be played back off-line.
 */ /*************************************************************************/
PVRSRV_ERROR PDumpPanic(IMG_UINT32      ui32PanicNo,
						IMG_CHAR*       pszPanicMsg,
						const IMG_CHAR* pszPPFunc,
						IMG_UINT32      ui32PPline)
{
	PVRSRV_ERROR   eError = PVRSRV_OK;
	PDUMP_FLAGS_T  uiPDumpFlags = PDUMP_FLAGS_CONTINUOUS;
	IMG_CHAR       pszConsoleMsg[] =
"COM ***************************************************************************\n"
"COM Script invalid and not compatible with off-line playback. Check test \n"
"COM parameters and driver configuration, stop imminent.\n"
"COM ***************************************************************************\n";
	PDUMP_GET_SCRIPT_STRING();

	/* Log the panic condition to the live kern.log in both REL and DEB mode 
	 * to aid user PDump trouble shooting. */
	PVR_LOG(("PDUMP PANIC %08x: %s", ui32PanicNo, pszPanicMsg));
	PVR_DPF((PVR_DBG_MESSAGE, "PDUMP PANIC start %s:%d", pszPPFunc, ui32PPline));

	/* Check the supplied panic reason string is within length limits */
	PVR_ASSERT(OSStringLength(pszPanicMsg)+sizeof("PANIC   ") < PVRSRV_PDUMP_MAX_COMMENT_SIZE-1);

	/* Add persistent flag if required and obtain lock to keep the multi-line
	 * panic statement together in a single atomic write */
	uiPDumpFlags |= (PDumpIsPersistent()) ? PDUMP_FLAGS_PERSISTENT : 0;
	PDUMP_LOCK();

	/* Write -- Panic start (Function:line) */
	eError = PDumpOSBufprintf(hScript, ui32MaxLen, "-- Panic start (%s:%d)", pszPPFunc, ui32PPline);
	PVR_LOGG_IF_ERROR(eError, "PDumpOSBufprintf", e1);
	(IMG_VOID)PDumpOSWriteString2(hScript, uiPDumpFlags);

	/* Write COM <message> x4 */
	eError = PDumpOSBufprintf(hScript, ui32MaxLen, pszConsoleMsg);
	PVR_LOGG_IF_ERROR(eError, "PDumpOSBufprintf", e1);
	(IMG_VOID)PDumpOSWriteString2(hScript, uiPDumpFlags);

	/* Write PANIC no msg command */
	eError = PDumpOSBufprintf(hScript, ui32MaxLen, "PANIC %08x %s", ui32PanicNo, pszPanicMsg);
	PVR_LOGG_IF_ERROR(eError, "PDumpOSBufprintf", e1);
	(IMG_VOID)PDumpOSWriteString2(hScript, uiPDumpFlags);

	/* Write -- Panic end */
	eError = PDumpOSBufprintf(hScript, ui32MaxLen, "-- Panic end");
	PVR_LOGG_IF_ERROR(eError, "PDumpOSBufprintf", e1);
	(IMG_VOID)PDumpOSWriteString2(hScript, uiPDumpFlags);

e1:
	PDUMP_UNLOCK();

	return eError;
}

/*!
******************************************************************************

 @Function	PDumpBitmapKM

 @Description

 Dumps a bitmap from device memory to a file

 @Input    psDevId
 @Input    pszFileName
 @Input    ui32FileOffset
 @Input    ui32Width
 @Input    ui32Height
 @Input    ui32StrideInBytes
 @Input    sDevBaseAddr
 @Input    ui32Size
 @Input    ePixelFormat
 @Input    eMemFormat
 @Input    ui32PDumpFlags

 @Return   PVRSRV_ERROR			:

******************************************************************************/
PVRSRV_ERROR PDumpBitmapKM(	PVRSRV_DEVICE_NODE *psDeviceNode,
							IMG_CHAR *pszFileName,
							IMG_UINT32 ui32FileOffset,
							IMG_UINT32 ui32Width,
							IMG_UINT32 ui32Height,
							IMG_UINT32 ui32StrideInBytes,
							IMG_DEV_VIRTADDR sDevBaseAddr,
							IMG_UINT32 ui32MMUContextID,
							IMG_UINT32 ui32Size,
							PDUMP_PIXEL_FORMAT ePixelFormat,
							IMG_UINT32 ui32AddrMode,
							IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_DEVICE_IDENTIFIER *psDevId = &psDeviceNode->sDevId;
	PVRSRV_ERROR eErr=0;
	PDUMP_GET_SCRIPT_STRING();

	if ( PDumpIsPersistent() )
	{
		return PVRSRV_OK;
	}
	
	PDumpCommentWithFlags(ui32PDumpFlags, "Dump bitmap of render.");
	
	switch (ePixelFormat)
	{
		case PVRSRV_PDUMP_PIXEL_FORMAT_YUV8:
		{
			PDumpCommentWithFlags(ui32PDumpFlags, "YUV data. Switching from SII to SAB. Width=0x%08X Height=0x%08X Stride=0x%08X",
							 						ui32Width, ui32Height, ui32StrideInBytes);
							 						
			eErr = PDumpOSBufprintf(hScript,
									ui32MaxLen,
									"SAB :%s:v%x:0x%010llX 0x%08X 0x%08X %s.bin\n",
									psDevId->pszPDumpDevName,
									ui32MMUContextID,
									sDevBaseAddr.uiAddr,
									ui32Size,
									ui32FileOffset,
									pszFileName);
			
			if (eErr != PVRSRV_OK)
			{
				return eErr;
			}
			
			PDUMP_LOCK();
			PDumpOSWriteString2( hScript, ui32PDumpFlags);
			PDUMP_UNLOCK();		
			break;
		}
		case PVRSRV_PDUMP_PIXEL_FORMAT_420PL12YUV8: // YUV420 2 planes
		{
			const IMG_UINT32 ui32Plane0Size = ui32StrideInBytes*ui32Height;
			const IMG_UINT32 ui32Plane1Size = ui32Plane0Size>>1; // YUV420
			const IMG_UINT32 ui32Plane1FileOffset = ui32FileOffset + ui32Plane0Size;
			const IMG_UINT32 ui32Plane1MemOffset = ui32Plane0Size;
			
			PDumpCommentWithFlags(ui32PDumpFlags, "YUV420 2-plane. Width=0x%08X Height=0x%08X Stride=0x%08X",
							 						ui32Width, ui32Height, ui32StrideInBytes);
			eErr = PDumpOSBufprintf(hScript,
						ui32MaxLen,
						"SII %s %s.bin :%s:v%x:0x%010llX 0x%08X 0x%08X :%s:v%x:0x%010llX 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
						pszFileName,
						pszFileName,
						
						// Plane 0 (Y)
						psDevId->pszPDumpDevName,	// memsp
						ui32MMUContextID,			// Context id
						sDevBaseAddr.uiAddr,		// virtaddr
						ui32Plane0Size,				// size
						ui32FileOffset,				// fileoffset
						
						// Plane 1 (UV)
						psDevId->pszPDumpDevName,	// memsp
						ui32MMUContextID,			// Context id
						sDevBaseAddr.uiAddr+ui32Plane1MemOffset,	// virtaddr
						ui32Plane1Size,				// size
						ui32Plane1FileOffset,		// fileoffset
						
						ePixelFormat,
						ui32Width,
						ui32Height,
						ui32StrideInBytes,
						ui32AddrMode);
						
			if (eErr != PVRSRV_OK)
			{
				return eErr;
			}
			
			PDUMP_LOCK();
			PDumpOSWriteString2( hScript, ui32PDumpFlags);
			PDUMP_UNLOCK();
			break;
		}
		
		case PVRSRV_PDUMP_PIXEL_FORMAT_YUV_YV12: // YUV420 3 planes
		{
			const IMG_UINT32 ui32Plane0Size = ui32StrideInBytes*ui32Height;
			const IMG_UINT32 ui32Plane1Size = ui32Plane0Size>>2; // YUV420
			const IMG_UINT32 ui32Plane2Size = ui32Plane1Size;
			const IMG_UINT32 ui32Plane1FileOffset = ui32FileOffset + ui32Plane0Size;
			const IMG_UINT32 ui32Plane2FileOffset = ui32Plane1FileOffset + ui32Plane1Size;
			const IMG_UINT32 ui32Plane1MemOffset = ui32Plane0Size;
			const IMG_UINT32 ui32Plane2MemOffset = ui32Plane0Size+ui32Plane1Size;
	
			PDumpCommentWithFlags(ui32PDumpFlags, "YUV420 3-plane. Width=0x%08X Height=0x%08X Stride=0x%08X",
							 						ui32Width, ui32Height, ui32StrideInBytes);
			eErr = PDumpOSBufprintf(hScript,
						ui32MaxLen,
						"SII %s %s.bin :%s:v%x:0x%010llX 0x%08X 0x%08X :%s:v%x:0x%010llX 0x%08X 0x%08X :%s:v%x:0x%010llX 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
						pszFileName,
						pszFileName,
						
						// Plane 0 (Y)
						psDevId->pszPDumpDevName,	// memsp
						ui32MMUContextID,			// MMU context id
						sDevBaseAddr.uiAddr,		// virtaddr
						ui32Plane0Size,				// size
						ui32FileOffset,				// fileoffset
						
						// Plane 1 (U)
						psDevId->pszPDumpDevName,	// memsp
						ui32MMUContextID,			// MMU context id
						sDevBaseAddr.uiAddr+ui32Plane1MemOffset,	// virtaddr
						ui32Plane1Size,				// size
						ui32Plane1FileOffset,		// fileoffset
						
						// Plane 2 (V)
						psDevId->pszPDumpDevName,	// memsp
						ui32MMUContextID,			// MMU context id
						sDevBaseAddr.uiAddr+ui32Plane2MemOffset,	// virtaddr
						ui32Plane2Size,				// size
						ui32Plane2FileOffset,		// fileoffset
						
						ePixelFormat,
						ui32Width,
						ui32Height,
						ui32StrideInBytes,
						ui32AddrMode);
						
			if (eErr != PVRSRV_OK)
			{
				return eErr;
			}
			
			PDUMP_LOCK();
			PDumpOSWriteString2( hScript, ui32PDumpFlags);
			PDUMP_UNLOCK();
			break;
		}
		
		default: // Single plane formats
		{
			eErr = PDumpOSBufprintf(hScript,
						ui32MaxLen,
						"SII %s %s.bin :%s:v%x:0x%010llX 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X",
						pszFileName,
						pszFileName,
						psDevId->pszPDumpDevName,
						ui32MMUContextID,
						sDevBaseAddr.uiAddr,
						ui32Size,
						ui32FileOffset,
						ePixelFormat,
						ui32Width,
						ui32Height,
						ui32StrideInBytes,
						ui32AddrMode);
						
			if (eErr != PVRSRV_OK)
			{
				return eErr;
			}

			PDUMP_LOCK();
			PDumpOSWriteString2( hScript, ui32PDumpFlags);
			PDUMP_UNLOCK();
			break;
		}
	}

	return PVRSRV_OK;
}

/*!
******************************************************************************

 @Function	PDumpReadRegKM

 @Description

 Dumps a read from a device register to a file

 @Input    psConnection 		: connection info
 @Input    pszFileName
 @Input    ui32FileOffset
 @Input    ui32Address
 @Input    ui32Size
 @Input    ui32PDumpFlags

 @Return   PVRSRV_ERROR			:

******************************************************************************/
PVRSRV_ERROR PDumpReadRegKM		(	IMG_CHAR *pszPDumpRegName,
									IMG_CHAR *pszFileName,
									IMG_UINT32 ui32FileOffset,
									IMG_UINT32 ui32Address,
									IMG_UINT32 ui32Size,
									IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	PVR_UNREFERENCED_PARAMETER(ui32Size);

	eErr = PDumpOSBufprintf(hScript,
			ui32MaxLen,
			"SAB :%s:0x%08X 0x%08X %s",
			pszPDumpRegName,
			ui32Address,
			ui32FileOffset,
			pszFileName);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2( hScript, ui32PDumpFlags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}


/*****************************************************************************
 @name		PDumpRegRead32
 @brief		Dump 32-bit register read to script
 @param		pszPDumpDevName - pdump device name
 @param		ui32RegOffset - register offset
 @param		ui32Flags - pdump flags
 @return	Error
*****************************************************************************/
PVRSRV_ERROR PDumpRegRead32(IMG_CHAR *pszPDumpRegName,
							const IMG_UINT32 ui32RegOffset,
							IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "RDW :%s:0x%X",
							pszPDumpRegName, 
							ui32RegOffset);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();
	return PVRSRV_OK;
}

/*****************************************************************************
 @name		PDumpRegRead64
 @brief		Dump 64-bit register read to script
 @param		pszPDumpDevName - pdump device name
 @param		ui32RegOffset - register offset
 @param		ui32Flags - pdump flags
 @return	Error
*****************************************************************************/
PVRSRV_ERROR PDumpRegRead64(IMG_CHAR *pszPDumpRegName,
							const IMG_UINT32 ui32RegOffset,
							IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "RDW64 :%s:0x%X",
							pszPDumpRegName, 
							ui32RegOffset);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();
	return PVRSRV_OK;
}


/*****************************************************************************
 FUNCTION	: PDumpWriteShiftedMaskedValue

 PURPOSE	: Emits the PDump commands for writing a masked shifted address
              into another location

 PARAMETERS	: PDump symbolic name and offset of target word
              PDump symbolic name and offset of source address
              right shift amount
              left shift amount
              mask

 RETURNS	: None
*****************************************************************************/
PVRSRV_ERROR
PDumpWriteShiftedMaskedValue(const IMG_CHAR *pszDestRegspaceName,
                             const IMG_CHAR *pszDestSymbolicName,
                             IMG_DEVMEM_OFFSET_T uiDestOffset,
                             const IMG_CHAR *pszRefRegspaceName,
                             const IMG_CHAR *pszRefSymbolicName,
                             IMG_DEVMEM_OFFSET_T uiRefOffset,
                             IMG_UINT32 uiSHRAmount,
                             IMG_UINT32 uiSHLAmount,
                             IMG_UINT32 uiMask,
                             IMG_DEVMEM_SIZE_T uiWordSize,
                             IMG_UINT32 uiPDumpFlags)
{
	PVRSRV_ERROR         eError;

    /* Suffix of WRW command in PDump (i.e. WRW or WRW64) */
    const IMG_CHAR       *pszWrwSuffix;

    /* Internal PDump register used for interim calculation */
    const IMG_CHAR       *pszPDumpIntRegSpace;
    IMG_UINT32           uiPDumpIntRegNum;

	PDUMP_GET_SCRIPT_STRING();

    if ((uiWordSize != 4) && (uiWordSize != 8))
    {
        return PVRSRV_ERROR_NOT_SUPPORTED;
    }

    pszWrwSuffix = (uiWordSize == 8) ? "64" : "";

    /* FIXME: "Acquire" a pdump register */
    pszPDumpIntRegSpace = pszDestRegspaceName;
    uiPDumpIntRegNum = 1;
        
    eError = PDumpOSBufprintf(hScript,
                              ui32MaxLen,
                              /* NB: should be "MOV" -- FIXME! */
                              "WRW :%s:$%d :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC "\n",
                              /* dest */
                              pszPDumpIntRegSpace,
                              uiPDumpIntRegNum,
                              /* src */
                              pszRefRegspaceName,
                              pszRefSymbolicName,
                              uiRefOffset);
    if (eError != PVRSRV_OK)
    {
        goto ErrOut; /* FIXME: proper error handling */
    }

    PDUMP_LOCK();
    PDumpOSWriteString2(hScript, uiPDumpFlags);

    if (uiSHRAmount > 0)
    {
        eError = PDumpOSBufprintf(hScript,
                                  ui32MaxLen,
                                  "SHR :%s:$%d :%s:$%d 0x%X\n",
                                  /* dest */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src A */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src B */
                                  uiSHRAmount);
        if (eError != PVRSRV_OK)
        {
            goto ErrUnlock; /* FIXME: proper error handling */
        }
        PDumpOSWriteString2(hScript, uiPDumpFlags);
    }
    
    if (uiSHLAmount > 0)
    {
        eError = PDumpOSBufprintf(hScript,
                                  ui32MaxLen,
                                  "SHL :%s:$%d :%s:$%d 0x%X\n",
                                  /* dest */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src A */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src B */
                                  uiSHLAmount);
        if (eError != PVRSRV_OK)
        {
            goto ErrUnlock; /* FIXME: proper error handling */
        }
        PDumpOSWriteString2(hScript, uiPDumpFlags);
    }
    
    if (uiMask != (1ULL << (8*uiWordSize))-1)
    {
        eError = PDumpOSBufprintf(hScript,
                                  ui32MaxLen,
                                  "AND :%s:$%d :%s:$%d 0x%X\n",
                                  /* dest */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src A */
                                  pszPDumpIntRegSpace,
                                  uiPDumpIntRegNum,
                                  /* src B */
                                  uiMask);
        if (eError != PVRSRV_OK)
        {
            goto ErrUnlock; /* FIXME: proper error handling */
        }
        PDumpOSWriteString2(hScript, uiPDumpFlags);
    }

    eError = PDumpOSBufprintf(hScript,
                              ui32MaxLen,
                              "WRW%s :%s:%s:" IMG_DEVMEM_OFFSET_FMTSPEC " :%s:$%d\n",
                              pszWrwSuffix,
                              /* dest */
                              pszDestRegspaceName,
                              pszDestSymbolicName,
                              uiDestOffset,
                              /* src */
                              pszPDumpIntRegSpace,
                              uiPDumpIntRegNum);
    if(eError != PVRSRV_OK)
    {
        goto ErrUnlock; /* FIXME: proper error handling */
    }
    PDumpOSWriteString2(hScript, uiPDumpFlags);

ErrUnlock:
	PDUMP_UNLOCK();
ErrOut:
	return eError;
}


PVRSRV_ERROR
PDumpWriteSymbAddress(const IMG_CHAR *pszDestSpaceName,
                      IMG_DEVMEM_OFFSET_T uiDestOffset,
                      const IMG_CHAR *pszRefSymbolicName,
                      IMG_DEVMEM_OFFSET_T uiRefOffset,
                      const IMG_CHAR *pszPDumpDevName,
                      IMG_UINT32 ui32WordSize,
                      IMG_UINT32 ui32AlignShift,
                      IMG_UINT32 ui32Shift,
                      IMG_UINT32 uiPDumpFlags)
{
    const IMG_CHAR       *pszWrwSuffix = "";
	PVRSRV_ERROR         eError = PVRSRV_OK;

	PDUMP_GET_SCRIPT_STRING();

    if (ui32WordSize == 8)
    {
        pszWrwSuffix = "64";
    }

    PDUMP_LOCK();

    if (ui32AlignShift != ui32Shift)
    {
    	/* Write physical address into a variable */
    	eError = PDumpOSBufprintf(hScript,
    							ui32MaxLen,
    							"WRW%s :%s:$1 %s:" IMG_DEVMEM_OFFSET_FMTSPEC "\n",
    							pszWrwSuffix,
    							/* dest */
    							pszPDumpDevName,
    							/* src */
    							pszRefSymbolicName,
    							uiRefOffset);
		if (eError != PVRSRV_OK)
		{
			goto symbAddress_error;
		}
    	PDumpOSWriteString2(hScript, uiPDumpFlags);

    	/* apply address alignment  */
    	eError = PDumpOSBufprintf(hScript,
    							ui32MaxLen,
    							"SHR :%s:$1 :%s:$1 0x%X",
    							/* dest */
    							pszPDumpDevName,
    							/* src A */
    							pszPDumpDevName,
    							/* src B */
    							ui32AlignShift);
		if (eError != PVRSRV_OK)
		{
			goto symbAddress_error;
		}
    	PDumpOSWriteString2(hScript, uiPDumpFlags);

    	/* apply address shift  */
    	eError = PDumpOSBufprintf(hScript,
    							ui32MaxLen,
    							"SHL :%s:$1 :%s:$1 0x%X",
    							/* dest */
    							pszPDumpDevName,
    							/* src A */
    							pszPDumpDevName,
    							/* src B */
    							ui32Shift);
		if (eError != PVRSRV_OK)
		{
			goto symbAddress_error;
		}
    	PDumpOSWriteString2(hScript, uiPDumpFlags);


    	/* write result to register */
    	eError = PDumpOSBufprintf(hScript,
    							ui32MaxLen,
    							"WRW%s :%s:0x%08X :%s:$1",
    							pszWrwSuffix,
    							pszDestSpaceName,
    							(IMG_UINT32)uiDestOffset,
    							pszPDumpDevName);
		if (eError != PVRSRV_OK)
		{
			goto symbAddress_error;
		}
    	PDumpOSWriteString2(hScript, uiPDumpFlags);
    }
    else
    {
		eError = PDumpOSBufprintf(hScript,
								  ui32MaxLen,
								  "WRW%s :%s:" IMG_DEVMEM_OFFSET_FMTSPEC " %s:" IMG_DEVMEM_OFFSET_FMTSPEC "\n",
								  pszWrwSuffix,
								  /* dest */
								  pszDestSpaceName,
								  uiDestOffset,
								  /* src */
								  pszRefSymbolicName,
								  uiRefOffset);
		if (eError != PVRSRV_OK)
		{
			goto symbAddress_error;
		}
	    PDumpOSWriteString2(hScript, uiPDumpFlags);
    }

symbAddress_error:

    PDUMP_UNLOCK();

	return eError;
}

/**************************************************************************
 * Function Name  : PDumpIDLWithFlags
 * Inputs         : Idle time in clocks
 * Outputs        : None
 * Returns        : Error
 * Description    : Dump IDL command to script
**************************************************************************/
PVRSRV_ERROR PDumpIDLWithFlags(IMG_UINT32 ui32Clocks, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();
	PDUMP_DBG(("PDumpIDLWithFlags"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "IDL %u", ui32Clocks);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();
	return PVRSRV_OK;
}


/**************************************************************************
 * Function Name  : PDumpIDL
 * Inputs         : Idle time in clocks
 * Outputs        : None
 * Returns        : Error
 * Description    : Dump IDL command to script
**************************************************************************/
PVRSRV_ERROR PDumpIDL(IMG_UINT32 ui32Clocks)
{
	return PDumpIDLWithFlags(ui32Clocks, PDUMP_FLAGS_CONTINUOUS);
}

/*****************************************************************************
 FUNCTION	: PDumpRegBasedCBP
    
 PURPOSE	: Dump CBP command to script

 PARAMETERS	:
			  
 RETURNS	: None
*****************************************************************************/
PVRSRV_ERROR PDumpRegBasedCBP(IMG_CHAR		*pszPDumpRegName,
							  IMG_UINT32	ui32RegOffset,
							  IMG_UINT32	ui32WPosVal,
							  IMG_UINT32	ui32PacketSize,
							  IMG_UINT32	ui32BufferSize,
							  IMG_UINT32	ui32Flags)
{
	PDUMP_GET_SCRIPT_STRING();

	PDumpOSBufprintf(hScript,
			 ui32MaxLen,
			 "CBP :%s:0x%08X 0x%08X 0x%08X 0x%08X",
			 pszPDumpRegName,
			 ui32RegOffset,
			 ui32WPosVal,
			 ui32PacketSize,
			 ui32BufferSize);
	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();
	
	return PVRSRV_OK;		
}

PVRSRV_ERROR PDumpTRG(IMG_CHAR *pszMemSpace,
                      IMG_UINT32 ui32MMUCtxID,
                      IMG_UINT32 ui32RegionID,
                      IMG_BOOL bEnable,
                      IMG_UINT64 ui64VAddr,
                      IMG_UINT64 ui64LenBytes,
                      IMG_UINT32 ui32XStride,
                      IMG_UINT32 ui32Flags)
{
	PDUMP_GET_SCRIPT_STRING();

	if(bEnable)
	{
		PDumpOSBufprintf(hScript, ui32MaxLen,
		                 "TRG :%s:v%u %u 0x%08llX 0x%08llX %u",
		                 pszMemSpace, ui32MMUCtxID, ui32RegionID,
		                 ui64VAddr, ui64LenBytes, ui32XStride);
	}
	else
	{
		PDumpOSBufprintf(hScript, ui32MaxLen,
		                 "TRG :%s:v%u %u",
		                 pszMemSpace, ui32MMUCtxID, ui32RegionID);

	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, ui32Flags);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}

/**************************************************************************
 * Function Name  : PDumpConnectionNotify
 * Description    : Called by the debugdrv to tell Services that pdump has
 * 					connected
 **************************************************************************/
IMG_EXPORT IMG_VOID PDumpConnectionNotify(IMG_CHAR* pszStreamName)
{
	PVRSRV_DATA			*psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_NODE	*psThis;

	/* Check the stream name so we only perform the notify actions once per
	 * pdump client connect, not twice as before.
	 */
	if ((pszStreamName == IMG_NULL) ||
		(OSStringCompare(pszStreamName, PDUMP_SCRIPT_STREAM_NAME) != 0))
	{
		return;
	}
	/* else do stuff needed on PDump client connection to the script stream */

	PVR_DPF((PVR_DBG_WARNING, "PDump has connected."));
	
	/* Loop over all known devices */
	psThis = psPVRSRVData->psDeviceNodeList;
	while (psThis)
	{
		if (psThis->pfnPDumpInitDevice)
		{
			/* Reset pdump according to connected device */
			psThis->pfnPDumpInitDevice(psThis);
		}
		psThis = psThis->psNext;
	}
}

/**************************************************************************
 * Function Name  : PDumpIfKM
 * Inputs         : pszPDumpCond - string for condition
 * Outputs        : None
 * Returns        : None
 * Description    : Create a PDUMP string which represents IF command 
					with condition.
**************************************************************************/
PVRSRV_ERROR PDumpIfKM(IMG_CHAR		*pszPDumpCond)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpIfKM"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "IF %s\n", pszPDumpCond);

	if (eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}

/**************************************************************************
 * Function Name  : PDumpElseKM
 * Inputs         : pszPDumpCond - string for condition
 * Outputs        : None
 * Returns        : None
 * Description    : Create a PDUMP string which represents ELSE command 
					with condition.
**************************************************************************/
PVRSRV_ERROR PDumpElseKM(IMG_CHAR		*pszPDumpCond)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpElseKM"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "ELSE %s\n", pszPDumpCond);

	if (eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}

/**************************************************************************
 * Function Name  : PDumpFiKM
 * Inputs         : pszPDumpCond - string for condition
 * Outputs        : None
 * Returns        : None
 * Description    : Create a PDUMP string which represents FI command 
					with condition.
**************************************************************************/
PVRSRV_ERROR PDumpFiKM(IMG_CHAR		*pszPDumpCond)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpFiKM"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "FI %s\n", pszPDumpCond);

	if (eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDUMP_LOCK();
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
	PDUMP_UNLOCK();

	return PVRSRV_OK;
}

/*****************************************************************************
 * Function Name  : DbgWrite
 * Inputs         : psStream - debug stream to write to
 					pui8Data - buffer
 					ui32BCount - buffer length
 					ui32Flags - flags, e.g. continuous, LF
 * Outputs        : None
 * Returns        : Error
 * Description    : Write a block of data to a debug stream
 *****************************************************************************/
IMG_UINT32 DbgWrite(PDBG_STREAM psStream, IMG_UINT8 *pui8Data, IMG_UINT32 ui32BCount, IMG_UINT32 ui32Flags)
{
	IMG_UINT32	ui32BytesWritten = 0;
	IMG_UINT32	ui32Off = 0;

	/* Return immediately if marked as "never" */
	if ((ui32Flags & PDUMP_FLAGS_NEVER) != 0)
	{
		return ui32BCount;
	}

	/* Send persistent data first ...
	 * If we're still initialising the params will be captured to the
	 * init stream in the call to pfnDBGDrivWrite2 below.
	 */
	if ( ((ui32Flags & PDUMP_FLAGS_PERSISTENT) != 0) && PDumpOSGetCtrlState(psStream, DBG_GET_STATE_INIT_PHASE_COMPLETE))
	{
		while (ui32BCount > 0)
		{
			/*
				Params marked as persistent should be appended to the init phase.
				For example window system mem mapping of the primary surface.
			*/
				ui32BytesWritten = PDumpOSDebugDriverWrite(	psStream,
															PDUMP_WRITE_MODE_PERSISTENT,
															&pui8Data[ui32Off], ui32BCount, DEBUG_LEVEL_0, 0);

			if (ui32BytesWritten == 0)
			{
				PDumpOSReleaseExecution();
			}

			if (ui32BytesWritten != 0xFFFFFFFFU)
			{
				ui32Off += ui32BytesWritten;
				ui32BCount -= ui32BytesWritten;
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR, "DbgWrite: Failed to send persistent data"));
				if( PDumpOSGetCtrlState(psStream, DBG_GET_STATE_FLAG_IS_READONLY) )
				{
					/* suspend pdump to prevent flooding kernel log buffer */
					PDumpSuspendKM();
				}
				return 0xFFFFFFFFU;
			}
		}

		/* reset buffer counters */
		ui32BCount = ui32Off; ui32Off = 0; ui32BytesWritten = 0;
	}

	while (((IMG_UINT32) ui32BCount > 0) && (ui32BytesWritten != 0xFFFFFFFFU))
	{
		if ((ui32Flags & PDUMP_FLAGS_CONTINUOUS) != 0)
		{
			/*
				If pdump client (or its equivalent) isn't running then throw continuous data away.
			*/
			if ( PDumpOSGetCtrlState(psStream, DBG_GET_STATE_THROW_DATA_AWAY) )
			{
				ui32BytesWritten = ui32BCount;
			}
			else
			{
				ui32BytesWritten = PDumpOSDebugDriverWrite(	psStream, 
															PDUMP_WRITE_MODE_CONTINUOUS,
															&pui8Data[ui32Off], ui32BCount, DEBUG_LEVEL_0, 0);
			}
		}
		else
		{
			if (ui32Flags & PDUMP_FLAGS_LASTFRAME)
			{
				ui32BytesWritten = PDumpOSDebugDriverWrite(	psStream,
															PDUMP_WRITE_MODE_LASTFRAME,
															&pui8Data[ui32Off], ui32BCount, DEBUG_LEVEL_0, 0);
			}
			else
			{
				ui32BytesWritten = PDumpOSDebugDriverWrite(	psStream, 
															PDUMP_WRITE_MODE_BINCM,
															&pui8Data[ui32Off], ui32BCount, DEBUG_LEVEL_0, 0);
			}
		}

		/*
			If the debug driver's buffers are full so no data could be written then yield
			execution so pdump can run and empty them.
		*/
		if (ui32BytesWritten == 0)
		{
			PDumpOSReleaseExecution();
		}

		if (ui32BytesWritten != 0xFFFFFFFFU)
		{
			ui32Off += ui32BytesWritten;
			ui32BCount -= ui32BytesWritten;
		}

		/* loop exits when i) all data is written, or ii) an unrecoverable error occurs */
	}


	/* FIXME: translate ui32BytesWritten to error here */
	return ui32BytesWritten;
}

PVRSRV_ERROR PDumpCreateLockKM(IMG_VOID)
{
	return PDumpOSCreateLock();
}

IMG_VOID PDumpDestroyLockKM(IMG_VOID)
{
	PDumpOSDestroyLock();
}

IMG_VOID PDumpLockKM(IMG_VOID)
{
	PDumpOSLock();
}

IMG_VOID PDumpUnlockKM(IMG_VOID)
{
	PDumpOSUnlock();
}

#if defined(PVR_TESTING_UTILS)
extern IMG_VOID PDumpOSDumpState(IMG_VOID);

#if !defined(LINUX)
IMG_VOID PDumpOSDumpState(IMG_VOID)
{
}
#endif

IMG_VOID PDumpCommonDumpState(IMG_VOID);
IMG_VOID PDumpCommonDumpState(IMG_VOID)
{
	IMG_UINT32* ui32HashData = (IMG_UINT32*)g_psPersistentHash;

	PVR_LOG(("--- PDUMP COMMON: isSuspended ( %d )",
			PDumpIsSuspended() ));

	PVR_LOG(("--- PDUMP COMMON: g_psPersistentHash( %p ) uSize( %d ) uCount( %d )",
			g_psPersistentHash, ui32HashData[0], ui32HashData[1]) );

	PDumpOSDumpState();
}
#endif


PVRSRV_ERROR PDumpRegisterConnection(SYNC_CONNECTION_DATA *psSyncConnectionData,
									 PDUMP_CONNECTION_DATA **ppsPDumpConnectionData)
{
	PDUMP_CONNECTION_DATA *psPDumpConnectionData;
	PVRSRV_ERROR eError;

	PVR_ASSERT(ppsPDumpConnectionData != IMG_NULL);

	psPDumpConnectionData = OSAllocMem(sizeof(*psPDumpConnectionData));
	if (psPDumpConnectionData == IMG_NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc;
	}

	eError = OSLockCreate(&psPDumpConnectionData->hLock, LOCK_TYPE_PASSIVE);
	if (eError != PVRSRV_OK)
	{
		goto fail_lockcreate;
	}

	dllist_init(&psPDumpConnectionData->sListHead);
	psPDumpConnectionData->ui32RefCount = 1;
	psPDumpConnectionData->bLastInto = IMG_FALSE;
	psPDumpConnectionData->ui32LastSetFrameNumber = 0;
	psPDumpConnectionData->bLastTransitionFailed = IMG_FALSE;
	/*
		Although we don't take a refcount here resman will ensure that
		any resource which might trigger use to do a Transition will
		have been freed before the sync blocks which are keeping the
		sync connection data alive
	*/
	psPDumpConnectionData->psSyncConnectionData = psSyncConnectionData;
	*ppsPDumpConnectionData = psPDumpConnectionData;

	return PVRSRV_OK;

fail_lockcreate:
	OSFreeMem(psPDumpConnectionData);
fail_alloc:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

IMG_VOID PDumpUnregisterConnection(PDUMP_CONNECTION_DATA *psPDumpConnectionData)
{
	_PDumpConnectionRelease(psPDumpConnectionData);
}

#else	/* defined(PDUMP) */
/* disable warning about empty module */
#ifdef	_WIN32
#pragma warning (disable:4206)
#endif
#endif	/* defined(PDUMP) */
/*****************************************************************************
 End of file (pdump_common.c)
*****************************************************************************/
