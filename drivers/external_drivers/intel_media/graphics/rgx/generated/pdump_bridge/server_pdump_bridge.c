/*************************************************************************/ /*!
@File
@Title          Server bridge for pdump
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Implements the server side of the bridge for pdump
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

#include "pdump_km.h"


#include "common_pdump_bridge.h"

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
PVRSRVBridgePVRSRVPDumpIsCapturing(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPISCAPTURING *psPVRSRVPDumpIsCapturingIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISCAPTURING *psPVRSRVPDumpIsCapturingOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPISCAPTURING);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpIsCapturingIN);




	psPVRSRVPDumpIsCapturingOUT->eError =
		PDumpIsCaptureFrameKM(
					&psPVRSRVPDumpIsCapturingOUT->bIsCapturing);




	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpComment(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT *psPVRSRVPDumpCommentIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT *psPVRSRVPDumpCommentOUT,
					 CONNECTION_DATA *psConnection)
{
	IMG_CHAR *uiCommentInt = IMG_NULL;

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPCOMMENT);

	PVR_UNREFERENCED_PARAMETER(psConnection);



	
	{
		uiCommentInt = OSAllocMem(PVRSRV_PDUMP_MAX_COMMENT_SIZE * sizeof(IMG_CHAR));
		if (!uiCommentInt)
		{
			psPVRSRVPDumpCommentOUT->eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	
			goto PVRSRVPDumpComment_exit;
		}
	}

			/* Copy the data over */
			if ( !OSAccessOK(PVR_VERIFY_READ, (IMG_VOID*) psPVRSRVPDumpCommentIN->puiComment, PVRSRV_PDUMP_MAX_COMMENT_SIZE * sizeof(IMG_CHAR))
				|| (OSCopyFromUser(NULL, uiCommentInt, psPVRSRVPDumpCommentIN->puiComment,
				PVRSRV_PDUMP_MAX_COMMENT_SIZE * sizeof(IMG_CHAR)) != PVRSRV_OK) )
			{
				psPVRSRVPDumpCommentOUT->eError = PVRSRV_ERROR_INVALID_PARAMS;

				goto PVRSRVPDumpComment_exit;
			}

	psPVRSRVPDumpCommentOUT->eError =
		PDumpCommentKM(
					uiCommentInt,
					psPVRSRVPDumpCommentIN->ui32Flags);



PVRSRVPDumpComment_exit:
	if (uiCommentInt)
		OSFreeMem(uiCommentInt);

	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpSetFrame(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPSETFRAME *psPVRSRVPDumpSetFrameIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSETFRAME *psPVRSRVPDumpSetFrameOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSETFRAME);





	psPVRSRVPDumpSetFrameOUT->eError =
		PDumpSetFrameKM(psConnection,
					psPVRSRVPDumpSetFrameIN->ui32Frame);




	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpIsLastCaptureFrame(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPISLASTCAPTUREFRAME *psPVRSRVPDumpIsLastCaptureFrameIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPISLASTCAPTUREFRAME *psPVRSRVPDumpIsLastCaptureFrameOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPISLASTCAPTUREFRAME);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpIsLastCaptureFrameIN);




	psPVRSRVPDumpIsLastCaptureFrameOUT->eError =
		PDumpIsLastCaptureFrameKM(
					);




	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpStartInitPhase(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTARTINITPHASE *psPVRSRVPDumpStartInitPhaseIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTARTINITPHASE *psPVRSRVPDumpStartInitPhaseOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSTARTINITPHASE);

	PVR_UNREFERENCED_PARAMETER(psConnection);
	PVR_UNREFERENCED_PARAMETER(psPVRSRVPDumpStartInitPhaseIN);




	psPVRSRVPDumpStartInitPhaseOUT->eError =
		PDumpStartInitPhaseKM(
					);




	return 0;
}

static IMG_INT
PVRSRVBridgePVRSRVPDumpStopInitPhase(IMG_UINT32 ui32BridgeID,
					 PVRSRV_BRIDGE_IN_PVRSRVPDUMPSTOPINITPHASE *psPVRSRVPDumpStopInitPhaseIN,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPSTOPINITPHASE *psPVRSRVPDumpStopInitPhaseOUT,
					 CONNECTION_DATA *psConnection)
{

	PVRSRV_BRIDGE_ASSERT_CMD(ui32BridgeID, PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSTOPINITPHASE);

	PVR_UNREFERENCED_PARAMETER(psConnection);




	psPVRSRVPDumpStopInitPhaseOUT->eError =
		PDumpStopInitPhaseKM(
					psPVRSRVPDumpStopInitPhaseIN->eModuleID);




	return 0;
}
#ifdef CONFIG_COMPAT
/* Bridge in structure for PVRSRVPDumpComment */
typedef struct compat_PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT_TAG
{
	/* IMG_CHAR * puiComment; */
	IMG_UINT32 puiComment;
	IMG_UINT32 ui32Flags;
} compat_PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT;
static IMG_INT
compat_PVRSRVBridgePVRSRVPDumpComment(IMG_UINT32 ui32BridgeID,
					 compat_PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT *psPVRSRVPDumpCommentIN_32,
					 PVRSRV_BRIDGE_OUT_PVRSRVPDUMPCOMMENT *psPVRSRVPDumpCommentOUT,
					 CONNECTION_DATA *psConnection)
{
	PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT sPVRSRVPDumpCommentIN;
	PVRSRV_BRIDGE_IN_PVRSRVPDUMPCOMMENT *psPVRSRVPDumpCommentIN=&sPVRSRVPDumpCommentIN;

	psPVRSRVPDumpCommentIN->puiComment = (IMG_CHAR*)(IMG_UINT64)psPVRSRVPDumpCommentIN_32->puiComment;
	psPVRSRVPDumpCommentIN->ui32Flags = psPVRSRVPDumpCommentIN_32->ui32Flags;

	return PVRSRVBridgePVRSRVPDumpComment(ui32BridgeID,
					psPVRSRVPDumpCommentIN,
					psPVRSRVPDumpCommentOUT,
					psConnection);
}
#endif

/* *************************************************************************** 
 * Server bridge dispatch related glue 
 */
 
PVRSRV_ERROR RegisterPDUMPFunctions(IMG_VOID);
IMG_VOID UnregisterPDUMPFunctions(IMG_VOID);

/*
 * Register all PDUMP functions with services
 */
PVRSRV_ERROR RegisterPDUMPFunctions(IMG_VOID)
{
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPISCAPTURING, PVRSRVBridgePVRSRVPDumpIsCapturing);
#ifdef CONFIG_COMPAT
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPCOMMENT, compat_PVRSRVBridgePVRSRVPDumpComment);
#else
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPCOMMENT, PVRSRVBridgePVRSRVPDumpComment);
#endif
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSETFRAME, PVRSRVBridgePVRSRVPDumpSetFrame);
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPISLASTCAPTUREFRAME, PVRSRVBridgePVRSRVPDumpIsLastCaptureFrame);
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSTARTINITPHASE, PVRSRVBridgePVRSRVPDumpStartInitPhase);
	SetDispatchTableEntry(PVRSRV_BRIDGE_PDUMP_PVRSRVPDUMPSTOPINITPHASE, PVRSRVBridgePVRSRVPDumpStopInitPhase);
	return PVRSRV_OK;
}

/*
 * Unregister all pdump functions with services
 */
IMG_VOID UnregisterPDUMPFunctions(IMG_VOID)
{
}
