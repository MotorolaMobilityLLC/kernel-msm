/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *
    \file csrCmdProcess.c

    Implementation for processing various commands.
========================================================================== */



#include "aniGlobal.h"

#include "palApi.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "smsDebug.h"
#include "macTrace.h"



eHalStatus csrMsgProcessor( tpAniSirGlobal pMac,  void *pMsgBuf )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeRsp *pSmeRsp = (tSirSmeRsp *)pMsgBuf;
#ifdef FEATURE_WLAN_SCAN_PNO
    tSirMbMsg *pMsg = (tSirMbMsg *)pMsgBuf;
    tCsrRoamSession *pSession;
#endif

    smsLog(pMac, LOG2, FL("Message %d[0x%04X] received in curState %s"
           " and substate %s sessionId (%d)"),
           pSmeRsp->messageType, pSmeRsp->messageType,
           macTraceGetcsrRoamState(pMac->roam.curState[pSmeRsp->sessionId]),
           macTraceGetcsrRoamSubState(
           pMac->roam.curSubState[pSmeRsp->sessionId]), pSmeRsp->sessionId);

#ifdef FEATURE_WLAN_SCAN_PNO
    /*
     * PNO scan responses have to be handled irrespective of CSR roam state.
     * Check if PNO has been started and only then process the PNO scan results.
     * Also note that normal scan is not allowed when PNO scan is in progress
     * and so the scan responses reaching here when PNO is started must be
     * PNO responses. For normal scan, the PNO started flag will be FALSE and
     * it will be processed as usual based on the current CSR roam state.
     */
    pSession = CSR_GET_SESSION(pMac, pSmeRsp->sessionId);
    if(!pSession)
    {
        smsLog(pMac, LOGE, FL(" session %d not found, msgType : %d"), pSmeRsp->sessionId,
               pMsg->type);
        return eHAL_STATUS_FAILURE;
    }

    if((eWNI_SME_SCAN_RSP == pMsg->type) && (TRUE == pSession->pnoStarted))
    {
        status = csrScanningStateMsgProcessor(pMac, pMsgBuf);
        if (eHAL_STATUS_SUCCESS != status)
        {
            smsLog(pMac, LOGE, FL(" handling PNO scan resp msg 0x%X CSR state is %d"),
                   pSmeRsp->messageType, pMac->roam.curState[pSmeRsp->sessionId]);
        }
        return (status);
    }
#endif

    // Process the message based on the state of the roaming states...

#if defined( ANI_RTT_DEBUG )
    if(!pAdapter->fRttModeEnabled)
    {
#endif//RTT
        switch (pMac->roam.curState[pSmeRsp->sessionId])
        {
        case eCSR_ROAMING_STATE_SCANNING:
        {
            //Are we in scan state
#if defined( ANI_EMUL_ASSOC )
            emulScanningStateMsgProcessor( pAdapter, pMBBufHdr );
#else
            status = csrScanningStateMsgProcessor(pMac, pMsgBuf);
#endif
            break;
        }

        case eCSR_ROAMING_STATE_JOINED:
        {
            //are we in joined state
            csrRoamJoinedStateMsgProcessor( pMac, pMsgBuf );
            break;
        }

        case eCSR_ROAMING_STATE_JOINING:
        {
            //are we in roaming states
#if defined( ANI_EMUL_ASSOC )
            emulRoamingStateMsgProcessor( pAdapter, pMBBufHdr );
#endif
            csrRoamingStateMsgProcessor( pMac, pMsgBuf );
            break;
        }

        //For all other messages, we ignore it
        default:
        {
            /*To work-around an issue where checking for set/remove key base on connection state is no longer
            * workable due to failure or finding the condition meets both SAP and infra/IBSS requirement.
            */
            if( (eWNI_SME_SETCONTEXT_RSP == pSmeRsp->messageType) ||
                (eWNI_SME_REMOVEKEY_RSP == pSmeRsp->messageType) )
            {
                smsLog(pMac, LOGW, FL(" handling msg 0x%X CSR state is %d"), pSmeRsp->messageType, pMac->roam.curState[pSmeRsp->sessionId]);
                csrRoamCheckForLinkStatusChange(pMac, pSmeRsp);
            }
            else if(eWNI_SME_GET_RSSI_REQ == pSmeRsp->messageType)
            {
                tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq*)pMsgBuf;
                if(NULL != pGetRssiReq->rssiCallback)
                {
                    smsLog(pMac,
                           LOGW,
                           FL("Message eWNI_SME_GET_RSSI_REQ is not handled"
                           " by CSR in state %d. calling RSSI callback"),
                           pMac->roam.curState[pSmeRsp->sessionId]);
                    ((tCsrRssiCallback)(pGetRssiReq->rssiCallback))(pGetRssiReq->lastRSSI,
                                                                    pGetRssiReq->staId,
                                                                    pGetRssiReq->pDevContext);
                }
                else
                {
                     smsLog(pMac, LOGE, FL("pGetRssiReq->rssiCallback is NULL"));
                }
            }
            else
            {
               smsLog(pMac, LOGE, "Message 0x%04X is not handled by CSR "
                  " CSR state is %d session Id %d", pSmeRsp->messageType,
                   pMac->roam.curState[pSmeRsp->sessionId], pSmeRsp->sessionId);

                if (eWNI_SME_FT_PRE_AUTH_RSP == pSmeRsp->messageType) {
                    smsLog(pMac, LOGE, "Dequeue eSmeCommandRoam command"
                       " with reason eCsrPerformPreauth");
                    csrDequeueRoamCommand(pMac, eCsrPerformPreauth);
                }
                else if (eWNI_SME_REASSOC_RSP == pSmeRsp->messageType) {
                    smsLog(pMac, LOGE, "Dequeue eSmeCommandRoam command"
                       " with reason eCsrSmeIssuedFTReassoc");
                    csrDequeueRoamCommand(pMac, eCsrSmeIssuedFTReassoc);
                }
            }
            break;
        }

        }//switch

#if defined( ANI_RTT_DEBUG )
    }
#endif//RTT

    return (status);
}



tANI_BOOLEAN csrCheckPSReady(void *pv)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pv );

    if (pMac->roam.sPendingCommands < 0)
    {
       VOS_ASSERT( pMac->roam.sPendingCommands >= 0 );
       return 0;
    }
    return (pMac->roam.sPendingCommands == 0);
}

tANI_BOOLEAN csrCheckPSOffloadReady(void *pv, tANI_U32 sessionId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(pv);

    VOS_ASSERT(pMac->roam.sPendingCommands >= 0);
    return (pMac->roam.sPendingCommands == 0);
}

void csrFullPowerCallback(void *pv, eHalStatus status)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pv );
    tListElem *pEntry;
    tSmeCmd *pCommand;

    (void)status;

    while( NULL != ( pEntry = csrLLRemoveHead( &pMac->roam.roamCmdPendingList, eANI_BOOLEAN_TRUE ) ) )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        smePushCommand( pMac, pCommand, eANI_BOOLEAN_FALSE );
    }

}

void csrFullPowerOffloadCallback(void *pv, tANI_U32 sessionId, eHalStatus status)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pv );
    tListElem *pEntry;
    tSmeCmd *pCommand;

    (void)status;

    while(NULL != (pEntry = csrLLRemoveHead(&pMac->roam.roamCmdPendingList,
                                            eANI_BOOLEAN_TRUE)))
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        smePushCommand(pMac, pCommand, eANI_BOOLEAN_FALSE);
    }

}
