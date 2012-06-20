/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * Airgo Networks, Inc proprietary. All rights reserved
 * sysEntryFunc.cc - This file has all the system level entry functions
 *                   for all the defined threads at system level.
 * Author:    V. K. Kandarpa
 * Date:      01/16/2002
 * History:-
 * Date       Modified by            Modification Information
 * --------------------------------------------------------------------------
 *
 */
/* Standard include files */

/* Application Specific include files */
#include "sirCommon.h"
#include "aniGlobal.h"


#include "limApi.h"
#include "schApi.h"
#include "utilsApi.h"
#include "pmmApi.h"

#include "sysDebug.h"
#include "sysDef.h"
#include "sysEntryFunc.h"
#include "sysStartup.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halMacSecurityApi.h"
#endif
#include "limTrace.h"
#include "wlan_qct_wda.h"

#ifndef WLAN_FTM_STUB
tSirRetStatus
postPTTMsgApi(tpAniSirGlobal pMac, tSirMsgQ *pMsg);
#endif

#ifdef VOSS_ENABLED
#include "vos_types.h"
#include "vos_packet.h"
#endif

// ---------------------------------------------------------------------------
/**
 * sysInitGlobals
 *
 * FUNCTION:
 *    Initializes system level global parameters
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param tpAniSirGlobal Sirius software parameter struct pointer
 * @return None
 */

tSirRetStatus
sysInitGlobals(tpAniSirGlobal pMac)
{

    palZeroMemory(pMac->hHdd, (tANI_U8 *) &pMac->sys, sizeof(pMac->sys));

#if defined(ANI_DEBUG)
    //FIXME : right now we want the reset to happen even in diag debug build.
    // later on we need to set this to true.
    //pMac->sys.debugOnReset = true;
    pMac->sys.debugOnReset = false;
#else
    pMac->sys.debugOnReset = false;
#endif

    pMac->sys.gSysEnableScanMode        = 1;
    pMac->sys.gSysEnableLinkMonitorMode = 0;
    pMac->sys.fTestRadar                = false;
    pMac->sys.radarDetected             = false;
    pMac->sys.gSysdropLimPkts           = false;
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
    if(eHAL_STATUS_SUCCESS != halGlobalInit(pMac))
        return eSIR_FAILURE;
#endif
    schInitGlobals(pMac);

    return eSIR_SUCCESS;
}

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC


// ---------------------------------------------------------------------------
/**
 * sysIsLearnScanModeFrame
 *
 * FUNCTION:
 * Determine whether the received frame was received in learn/scan mode
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pFrame
 * @return true if frame was received in learn/scan mode
 *         false otherwise
 */

static inline tANI_U8
sysIsLearnScanModeFrame(tpHalBufDesc pBd)
{
    if( SIR_MAC_BD_TO_SCAN_LEARN(pBd) )
         return 1;
    else
        return 0;
}
#endif
// ---------------------------------------------------------------------------
/**
 * sysBbtProcessMessageCore
 *
 * FUNCTION:
 * Process BBT messages
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param tpAniSirGlobal A pointer to MAC params instance
 * @param pMsg message pointer
 * @param tANI_U32 type
 * @param tANI_U32 sub type
 * @return None
 */
tSirRetStatus
sysBbtProcessMessageCore(tpAniSirGlobal pMac, tpSirMsgQ pMsg, tANI_U32 type,
                         tANI_U32 subType)
{
    tSirRetStatus ret;
    void*         pBd;
    tMgmtFrmDropReason dropReason;
    vos_pkt_t  *pVosPkt = (vos_pkt_t *)pMsg->bodyptr;
    VOS_STATUS  vosStatus =
              WDA_DS_PeekRxPacketInfo( pVosPkt, (v_PVOID_t *)&pBd, VOS_FALSE );
    pMac->sys.gSysBbtReceived++;

    if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
    {
        goto fail;
    }

    PELOGW(sysLog(pMac, LOGW, FL("Rx Mgmt Frame Subtype: %d\n"), subType);
    sirDumpBuf(pMac, SIR_SYS_MODULE_ID, LOGW, (tANI_U8 *)WDA_GET_RX_MAC_HEADER(pBd), WDA_GET_RX_MPDU_LEN(pBd));
    sirDumpBuf(pMac, SIR_SYS_MODULE_ID, LOGW, WDA_GET_RX_MPDU_DATA(pBd), WDA_GET_RX_PAYLOAD_LEN(pBd));)

    pMac->sys.gSysFrameCount[type][subType]++;

    if(type == SIR_MAC_MGMT_FRAME)
    {

            if( (dropReason = limIsPktCandidateForDrop(pMac, pBd, subType)) != eMGMT_DROP_NO_DROP)
            {
                PELOG1(sysLog(pMac, LOG1, FL("Mgmt Frame %d being dropped, reason: %d\n"), subType, dropReason);)
                MTRACE(macTrace(pMac,   TRACE_CODE_RX_MGMT_DROP, 0, dropReason);)
                goto fail;
            }
            //Post the message to PE Queue
            ret = (tSirRetStatus) limPostMsgApi(pMac, pMsg);
            if (ret != eSIR_SUCCESS)
            {
                PELOGE(sysLog(pMac, LOGE, FL("posting to LIM2 failed, ret %d\n"), ret);)
                goto fail;
            }
            pMac->sys.gSysBbtPostedToLim++;
    }
#ifdef FEATURE_WLAN_CCX
    else if (type == SIR_MAC_DATA_FRAME)
    {
        PELOGW(sysLog(pMac, LOGW, FL("IAPP Frame...\n")););
        //Post the message to PE Queue
        ret = (tSirRetStatus) limPostMsgApi(pMac, pMsg);
        if (ret != eSIR_SUCCESS)
        {
            PELOGE(sysLog(pMac, LOGE, FL("posting to LIM2 failed, ret %d\n"), ret);)
            goto fail;
        }
        pMac->sys.gSysBbtPostedToLim++;
    }
#endif
    else
    {
        PELOGE(sysLog(pMac, LOGE, "BBT received Invalid type %d subType %d "
                   "LIM state %X. BD dump is:\n",
                   type, subType, limGetSmeState(pMac));
        sirDumpBuf(pMac, SIR_SYS_MODULE_ID, LOGE,
                       (tANI_U8 *) pBd, WLANHAL_RX_BD_HEADER_SIZE);)

        goto fail;
    }

    return eSIR_SUCCESS;

fail:

    pMac->sys.gSysBbtDropped++;
    return eSIR_FAILURE;
}


void sysLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...)
{
    // Verify against current log level
    if ( loglevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( SIR_SYS_MODULE_ID )] )
        return;
    else
    {
        va_list marker;

        va_start( marker, pString );     /* Initialize variable arguments. */

        logDebug(pMac, SIR_SYS_MODULE_ID, loglevel, pString, marker);

        va_end( marker );              /* Reset variable arguments.      */
    }
}



#if defined( ANI_OS_TYPE_WINDOWS )
// ---------------------------------------------------------------------------
/**
 * sysBbtProcessMessage
 *
 * FUNCTION:
 *    Process BBT messages
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pBD Buffer descriptor pointer
 * @return None
 */
void sysBbtProcessMessage( tHalHandle hHal, void *pBD )
{
    tpAniSirGlobal  pMac = PMAC_STRUCT( hHal );
    tpSirMacMgmtHdr mHdr;
    tSirMsgQ        msg;

    //
    //  The MPDU header is now present at a certain "offset" in
    // the BD and is specified in the BD itself
    //
    mHdr = WDA_GET_RX_MAC_HEADER(pBD);

    // Dump received packet
    /*
    if(pBD->swBdType != SMAC_SWBD_TYPE_CTLMSG)
        sysLog( pMac, LOG3,
            FL( "%s: RX Mesg Type %d, subType %d, MPDU Len %d, RXP Flags 0x%x\n" ),
            __FUNCTION__,
            mHdr->fc.type,
            mHdr->fc.subType,
            pBD->mpduLength,
            pBD->rxpFlags );
    */
    //sirDumpBuf(pMac, SIR_SYS_MODULE_ID, LOGW, SIR_MAC_BD_TO_MPDUDATA(pBD), SIR_MAC_BD_TO_PAYLOAD_LEN(pBD));

    // Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG
    msg.type = SIR_BB_XPORT_MGMT_MSG;
    msg.bodyptr = pBD;
    msg.bodyval = 0;

    if( eSIR_SUCCESS != sysBbtProcessMessageCore( pMac,
                                                  &msg,
                                                  mHdr->fc.type,
                                                  mHdr->fc.subType ))
    {
        sysLog( pMac, LOGW,
                FL ( "sysBbtProcessMessageCore failed to process SIR_BB_XPORT_MGMT_MSG\n" ));

        // TODO - Will the caller (HDD) free the received packet?
    }
}
#endif // #if defined( ANI_OS_TYPE_WINDOWS )

#if defined(ANI_OS_TYPE_RTAI_LINUX)
#ifndef WLAN_FTM_STUB
#include "pttModuleApi.h"
#endif // eDRIVER_TYPE_MFG

// ---------------------------------------------------------------------
/**
 * sysMmhEntry
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy Dummy parameter
 * @return None
 */
void
sysMmhEntry(tANI_U32 dummy)
{
    tSirMbMsg  *pMbMsg;
    tSirMsgQ    msg;
    tpAniSirGlobal pMac;

    pMac = getPMac();

    sysLog(pMac, LOG4, "MMH task started\n");

    while (1)
    {
        // Blocks waiting for messages from HDD

        tx_queue_receive(&pMac->sys.gSirTxMsgQ, (void*)&pMbMsg,
                         TX_WAIT_FOREVER);
        // Compose inter-module message and send it off to the receiver
        msg.type = sirReadU16N((tANI_U8*)&(pMbMsg->type));
        msg.bodyptr = pMbMsg;
        msg.bodyval = 0;
        sysLog(pMac, LOG4, "<MMH> HDD message received id=0x%x\n", msg.type);
        if (pMac->sys.abort==1)
        {
            if (msg.type==eWNI_SME_STOP_BSS_REQ)
            {
                sirStoreU16N((tANI_U8*)(&(pMbMsg->type)), eWNI_SME_STOP_BSS_RSP);
                pMbMsg->data[0] = 0;
                halMmhPostMsgApi(pMac, &msg,  ePROT);
            }
            else
            {
                // we should free buffer, but only if it is an skbuff
            }
            continue;
        }

        switch (msg.type & 0xFF00)
        {
            case SIR_HAL_MSG_TYPES_BEGIN:
                if (halPostMsgApi(pMac, &msg) != eSIR_SUCCESS)
                    sysLog(pMac, LOGP, "sysMmhEntry: halPostMsgApi Failed!\n");
                else
                {
                    sysLog(pMac, LOG4, "<MMH> Message forwarded to HAL\n");
                }

                break;

            case SIR_LIM_MSG_TYPES_BEGIN:
                limPostMsgApi(pMac, &msg);
                break;

            case SIR_MNT_MSG_TYPES_BEGIN:

                if (halMntPostMsgApi(pMac, &msg) != eSIR_SUCCESS)
                    sysLog(pMac, LOGP, "sysMmhEntry: halMntPostMsgApi Failed!\n");
                else
                {
                    sysLog(pMac, LOG4, "<MMH> Message forwarded to MNT type (%X)\n",
                           msg.type);
                }

                break;

            case SIR_PMM_MSG_TYPES_BEGIN:

                // Shall have its API call here; Once API is added, remove the
                // following release memory call.
                break;

            case SIR_CFG_MSG_TYPES_BEGIN:

                if (halMntPostMsgApi(pMac, &msg) != eSIR_SUCCESS)
                    sysLog(pMac, LOGP,
                           "sysMmhEntry: cfg msg: halMntPostMsgApi Failed!\n");
                else
                {
                    sysLog(pMac, LOG4,
                           "sysMmhEntry: cfg msg: halMntPostMsgApi!\n");
                }

                break;

#ifndef WLAN_FTM_STUB
            case PTT_MSG_TYPES_BEGIN_30: /*PTT_MSG_TYPES_BEGIN:*/
            case PTT_MSG_TYPES_BEGIN_31:
            case PTT_MSG_TYPES_BEGIN_32:
                if (postPTTMsgApi(pMac, &msg) != eSIR_SUCCESS)
                    sysLog(pMac, LOGP,
                           "sysMmhEntry: RD msg: postPTTMsgApi Failed!\n");
                else
                {
                    sysLog(pMac, LOG4,
                           "sysMmhEntry: RD msg: postPTTMsgApi!\n");
                }
                break;
#endif

            default:
                sysLog(pMac, LOGW, "sysMmhEntry Unknown destination \n");
                // Unknown destination.  Just drop it
                palFreeMemory( pMac->hHdd, (void*)pMbMsg);
        }
    }

} /*** sysMmhEntry() ***/

#endif // #if defined(ANI_OS_TYPE_RTAI_LINUX)

#if defined(ANI_OS_TYPE_LINUX) || defined(ANI_OS_TYPE_OSX)

// ---------------------------------------------------------------------------
/**
 * sysSchEntry
 *
 * FUNCTION:
 *         SCH thread entry function.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy At present there is no need of any entry input. Default=0
 * @return None
 */
void
sysSchEntry(tANI_U32 i)
{
    tpAniSirGlobal pMac;
    pMac = getPMac();
    while (1)
{
        schProcessMessageQueue(pMac);
    }
} // sysSchEntry()


// ---------------------------------------------------------------------------
/**
 * sysPmmEntry
 *
 * FUNCTION:
 *         PMM thread entry function.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy At present there is no need of any entry input. Default=0
 * @return None
 */
void
sysPmmEntry(tANI_U32 i)
{
    tpAniSirGlobal pMac;
    pMac = getPMac();
    while (1)
    {
        pmmProcessMessageQueue(pMac);
    }
} // sysPmmEntry()


// ---------------------------------------------------------------------
/**
 * sysLimEntry
 *
 * FUNCTION:
 *   LIM thread entry point
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy Dummy parameter
 * @return None
 */

void
sysLimEntry(tANI_U32 dummy)
{
    tpAniSirGlobal pMac;
    pMac = getPMac();
    limInitialize(pMac);

    while (1)
    {
        limProcessMessageQueue(pMac);
    } // while(1)
} // limEntry()



// ---------------------------------------------------------------------
/**
 * sysMntEntry
 *
 * FUNCTION:
 *    MNT thread entry point
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy Dummy parameter
 * @return None
 */

void
sysMntEntry(tANI_U32 dummy)
{
    tANI_U32      status;
    tSirMsgQ msg;
    tpAniSirGlobal pMac;
    pMac = getPMac();
    sysLog(pMac, LOG4, "<MNT> MNT task started\n");

#if defined(ANI_OS_TYPE_RTAI_LINUX)
    tANI_U32      interval;
    interval = SYS_MNT_INTERVAL * SYS_TICKS_PER_SECOND;
#endif

    while (1)
    {
#if defined(ANI_OS_TYPE_RTAI_LINUX)
        status = tx_queue_receive(&pMac->sys.gSirMntMsgQ, &msg, interval);
#else
        status = tx_queue_receive(&pMac->sys.gSirMntMsgQ, &msg,
                                  TX_WAIT_FOREVER);
#endif

        // this routine only dequeues the message from queue
        // processing is done by sysMntProcessMsg
        if (status == TX_SUCCESS)
        {
            if (halMntProcessMsgs(pMac, &msg) != eSIR_SUCCESS)
            {
                sysLog(pMac, LOGP, "sysMntEntry: halMntProcessMsgs call "
                       "failed!\n");
            }
        }
    }
} // sysMntEntry()

// ---------------------------------------------------------------------
/**
 * sysHalEntry
 *
 * FUNCTION:
 *   HAL thread entry point
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param dummy Dummy parameter
 * @return None
 */

void
sysHalEntry(tANI_U32 dummy)
{
    tANI_U32 status;
    tSirMsgQ msg;
    tpAniSirGlobal pMac;

    pMac = getPMac();
    sysLog(pMac, LOG4, "<HAL> HAL task started\n");

    while (1)
    {

        status = tx_queue_receive(&pMac->sys.gSirHalMsgQ, &msg,
                                  TX_WAIT_FOREVER);
        // this routine only dequeues the message from queue
        if (status == TX_SUCCESS)
        {
            if (halProcessMsg(pMac, &msg) != eSIR_SUCCESS)
            {
                sysLog(pMac, LOGP, "sysHalEntry: halProcessMsgQ call failed!\n");
            }
        }
    } // while(1)
} // sysHalEntry

#ifndef WLAN_FTM_STUB
#include "pttModuleApi.h"
// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
/**
  * sysNimPttEntry
  *
  * FUNCTION:
  *   NIM PTT thread entry point
  * LOGIC:
  *
  * ASSUMPTIONS:
  *
  * NOTE:
  *
  * @param dummy Dummy parameter
  * @return None
  */

void
sysNimPttEntry(tANI_U32 dummy)
{
    tANI_U32 status;
    tSirMsgQ msg;
    tpAniSirGlobal pMac;
    pMac = getPMac();

    sysLog(pMac, LOGW, "<NIM> PTT task started\n");

    while (1)
    {
        status = tx_queue_receive(&pMac->sys.gSirNimRDMsgQ, &msg,
                                  TX_WAIT_FOREVER);

        // this routine only dequeues the message from queue
        if (status == TX_SUCCESS)
        {
            pttProcessMsg(pMac, (tPttMsgbuffer *)msg.bodyptr);
            //TODO: the resonse is now packaged in ((tPttMsgbuffer *)&msg->body)->msgResponse and needs to be sent back to the application
        }
     } // while(1)
} // sysNimPttEntry

// ---------------------------------------------------------------------

// -------------------------------------------------------------
/**
 * postPTTMsgApi
 *
 * FUNCTION:
 *     Posts NIM messages to gNIM thread
 *
 * LOGIC:
 *
 * ASSUMPTIONS:pl
 *
 *
 * NOTE:
 *
 * @param tpAniSirGlobal MAC parameters structure
 * @param pMsg pointer with message
 * @return Success or Failure
 */

tSirRetStatus
postPTTMsgApi(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
    tSirRetStatus rc = eSIR_SUCCESS;
    tPttMsgbuffer *pPttMsg;
    ePttMsgId msgId;
    uPttMsgs *msgBody;
    tANI_U8 *pReq = (tANI_U8*) pMsg->bodyptr;

    pPttMsg = (tPttMsgbuffer *)pReq;
#if (defined(ANI_OS_TYPE_RTAI_LINUX) && defined(ANI_LITTLE_BYTE_ENDIAN))
    pPttMsg->msgId = sirReadU16N((tANI_U8 *)&(pPttMsg->msgId));
#endif
    msgId = (ePttMsgId)(pPttMsg->msgId);
    msgBody = (uPttMsgs *)&(pPttMsg->msgBody);
    do
    {
#if defined ANI_OS_TYPE_LINUX || defined ANI_OS_TYPE_OSX
        // Posts message to the queue
        if (tx_queue_send(&pMac->sys.gSirHalMsgQ, pMsg,
                          TX_NO_WAIT) != TX_SUCCESS)
        {
            rc = eSIR_FAILURE;
            break;
        }
#else
        // For Windows based MAC, instead of posting message to different
        // queues, we will call the handler routines directly

        //pttProcessMsg(pMac, pMsg);
        rc = eSIR_SUCCESS;
#endif
    }
    while (0);

    return rc;
} // postPTTMsgApi()


#endif // eDRIVER_TYPE_MFG

#endif // #if defined ANI_OS_TYPE_LINUX || defined ANI_OS_TYPE_OSX

