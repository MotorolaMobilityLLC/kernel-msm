/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

/*===========================================================================
                        L I M _ P 2 P . C

  OVERVIEW:

  This software unit holds the implementation of the WLAN Protocol Engine for
  P2P.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2011-05-02    djindal Corrected file indentation and changed remain on channel
                      handling for concurrency.
===========================================================================*/


#include "limUtils.h"
#include "limSessionUtils.h"
#include "wlan_qct_wda.h"

#define   PROBE_RSP_IE_OFFSET    36
#define   BSSID_OFFSET           16
#define   ADDR2_OFFSET           10
#define   ACTION_OFFSET          24

/* A DFS channel can be ACTIVE for max 9000 msec, from the last
   received Beacon/Prpbe Resp. */
#define   MAX_TIME_TO_BE_ACTIVE_CHANNEL 9000



void limRemainOnChnlSuspendLinkHdlr(tpAniSirGlobal pMac, eHalStatus status,
                                       tANI_U32 *data);
void limRemainOnChnlSetLinkStat(tpAniSirGlobal pMac, eHalStatus status,
                                tANI_U32 *data, tpPESession psessionEntry);
void limExitRemainOnChannel(tpAniSirGlobal pMac, eHalStatus status,
                         tANI_U32 *data, tpPESession psessionEntry);
extern tSirRetStatus limSetLinkState(
                         tpAniSirGlobal pMac, tSirLinkState state,
                         tSirMacAddr bssId, tSirMacAddr selfMacAddr,
                         tpSetLinkStateCallback callback, void *callbackArg);

static tSirRetStatus limCreateSessionForRemainOnChn(tpAniSirGlobal pMac, tPESession **ppP2pSession);
eHalStatus limP2PActionCnf(tpAniSirGlobal pMac, tANI_U32 txCompleteSuccess);
/*------------------------------------------------------------------
 *
 * Below function is callback function, it is called when
 * WDA_SET_LINK_STATE_RSP is received from WDI. callback function for
 * P2P of limSetLinkState
 *
 *------------------------------------------------------------------*/
void limSetLinkStateP2PCallback(tpAniSirGlobal pMac, void *callbackArg,
            bool status)
{
    //Send Ready on channel indication to SME
    if(pMac->lim.gpLimRemainOnChanReq)
    {
        limSendSmeRsp(pMac, eWNI_SME_REMAIN_ON_CHN_RDY_IND, eHAL_STATUS_SUCCESS,
                     pMac->lim.gpLimRemainOnChanReq->sessionId, 0);
    }
    else
    {
        //This is possible in case remain on channel is aborted
        limLog( pMac, LOGE, FL(" NULL pointer of gpLimRemainOnChanReq") );
    }
}

/*------------------------------------------------------------------
 *
 * Below function is called if pMac->fP2pListenOffload enabled and hdd
 * requests a remain on channel.
 *
 *------------------------------------------------------------------*/
static eHalStatus limSendHalReqRemainOnChanOffload(tpAniSirGlobal pMac,
                                      tSirRemainOnChnReq *pRemOnChnReq)
{
    tSirScanOffloadReq *pScanOffloadReq;
    tSirMsgQ msg;
    tSirRetStatus rc = eSIR_SUCCESS;
    tANI_U8 bssid[SIR_MAC_ADDR_LENGTH] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    pScanOffloadReq = vos_mem_malloc(sizeof(tSirScanOffloadReq));
    if (NULL == pScanOffloadReq)
    {
        limLog(pMac, LOGE,
                FL("Memory allocation failed for pScanOffloadReq"));
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_zero(pScanOffloadReq, sizeof(tSirScanOffloadReq));

    msg.type = WDA_START_SCAN_OFFLOAD_REQ;
    msg.bodyptr = pScanOffloadReq;
    msg.bodyval = 0;

    vos_mem_copy((tANI_U8 *) pScanOffloadReq->selfMacAddr,
                 (tANI_U8 *) pRemOnChnReq->selfMacAddr,
                 sizeof(tSirMacAddr));

    vos_mem_copy((tANI_U8 *) pScanOffloadReq->bssId,
                 (tANI_U8 *) bssid,
                 sizeof(tSirMacAddr));
    pScanOffloadReq->scanType = eSIR_PASSIVE_SCAN;
    pScanOffloadReq->p2pScanType = P2P_SCAN_TYPE_LISTEN;
    pScanOffloadReq->minChannelTime = pRemOnChnReq->duration;
    pScanOffloadReq->maxChannelTime = pRemOnChnReq->duration;
    pScanOffloadReq->sessionId = pRemOnChnReq->sessionId;
    pScanOffloadReq->channelList.numChannels = 1;
    pScanOffloadReq->channelList.channelNumber[0] = pRemOnChnReq->chnNum;

    limLog(pMac, LOG1,
            FL("Req-rem-on-channel: duration %u, session %hu, chan %hu"),
            pRemOnChnReq->duration, pRemOnChnReq->sessionId,
            pRemOnChnReq->chnNum);

    rc = wdaPostCtrlMsg(pMac, &msg);
    if (rc != eSIR_SUCCESS)
    {
        limLog(pMac, LOGE, FL("wdaPostCtrlMsg() return failure %u"),
                rc);
        vos_mem_free(pScanOffloadReq);
        return eHAL_STATUS_FAILURE;
    }

    pMac->lim.fOffloadScanPending = 1;
    pMac->lim.fOffloadScanP2PListen = 1;

    return eHAL_STATUS_SUCCESS;
}

/*------------------------------------------------------------------
 *
 * Remain on channel req handler. Initiate the INIT_SCAN, CHN_CHANGE
 * and SET_LINK Request from SME, chnNum and duration to remain on channel.
 *
 *------------------------------------------------------------------*/
int limProcessRemainOnChnlReq(tpAniSirGlobal pMac, tANI_U32 *pMsg)
{

    /* CONC_OPER_AND_LISTEN_CHNL_SAME_OPTIMIZE - Currently removed the special optimization when a concurrent session
     * exists with operating channel same as P2P listen channel since it was causing issues in P2P search. The reason was
     * STA-AP link entering BMPS when returning to home channel causing P2P search to miss Probe Reqs and hence not
     * respond with Probe Rsp causing peer device to NOT find us.
     * If we need this optimization, we need to find a way to keep the STA-AP link awake (no BMPS) on home channel when in listen state
     */

    tSirRemainOnChnReq *MsgBuff = (tSirRemainOnChnReq *)pMsg;
    pMac->lim.gpLimRemainOnChanReq = MsgBuff;

    if (pMac->fP2pListenOffload)
    {
        eHalStatus status;

        status = limSendHalReqRemainOnChanOffload(pMac, MsgBuff);
        if (status != eHAL_STATUS_SUCCESS)
        {
            /* Post the meessage to Sme */
            limSendSmeRsp(pMac, eWNI_SME_REMAIN_ON_CHN_RSP, status,
                    MsgBuff->sessionId, 0);
            vos_mem_free(pMac->lim.gpLimRemainOnChanReq);
            pMac->lim.gpLimRemainOnChanReq = NULL;
        }
        return FALSE;
    }

    pMac->lim.gLimPrevMlmState = pMac->lim.gLimMlmState;
    pMac->lim.gLimMlmState     = eLIM_MLM_P2P_LISTEN_STATE;

    pMac->lim.gTotalScanDuration = MsgBuff->duration;

    /* 1st we need to suspend link with callback to initiate change channel */
    limSuspendLink(pMac, eSIR_CHECK_LINK_TRAFFIC_BEFORE_SCAN,
                   limRemainOnChnlSuspendLinkHdlr, NULL);
    return FALSE;

}


tSirRetStatus limCreateSessionForRemainOnChn(tpAniSirGlobal pMac, tPESession **ppP2pSession)
{
    tSirRetStatus nSirStatus = eSIR_FAILURE;
    tpPESession psessionEntry;
    tANI_U8 sessionId;
    tANI_U32 val;

    if(pMac->lim.gpLimRemainOnChanReq && ppP2pSession)
    {
        if((psessionEntry = peCreateSession(pMac,
           pMac->lim.gpLimRemainOnChanReq->selfMacAddr,
           &sessionId, 1, eSIR_INFRA_AP_MODE)) == NULL)
        {
            limLog(pMac, LOGE, FL("Session Can not be created "));
            /* send remain on chn failure */
            return nSirStatus;
        }
        /* Store PE sessionId in session Table  */
        psessionEntry->peSessionId = sessionId;
        psessionEntry->smeSessionId = pMac->lim.gpLimRemainOnChanReq->sessionId;

        psessionEntry->limSystemRole = eLIM_P2P_DEVICE_ROLE;
        CFG_GET_STR( nSirStatus, pMac,  WNI_CFG_SUPPORTED_RATES_11A,
               psessionEntry->rateSet.rate, val , SIR_MAC_MAX_NUMBER_OF_RATES );
        psessionEntry->rateSet.numRates = val;

        CFG_GET_STR( nSirStatus, pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                     psessionEntry->extRateSet.rate, val,
                     WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN );
        psessionEntry->extRateSet.numRates = val;

        sirCopyMacAddr(psessionEntry->selfMacAddr,
                       pMac->lim.gpLimRemainOnChanReq->selfMacAddr);

        psessionEntry->currentOperChannel = pMac->lim.gpLimRemainOnChanReq->chnNum;
        nSirStatus = eSIR_SUCCESS;
        *ppP2pSession = psessionEntry;
    }

    return nSirStatus;
}


/*------------------------------------------------------------------
 *
 * limSuspenLink callback, on success link suspend, trigger change chn
 *
 *
 *------------------------------------------------------------------*/

tSirRetStatus limRemainOnChnlChangeChnReq(tpAniSirGlobal pMac,
                                          eHalStatus status, tANI_U32 *data)
{
    tpPESession psessionEntry;
    tANI_U8 sessionId = 0;
    tSirRetStatus nSirStatus = eSIR_FAILURE;

    if( NULL == pMac->lim.gpLimRemainOnChanReq )
    {
        //RemainOnChannel may have aborted
        PELOGE(limLog( pMac, LOGE, FL(" gpLimRemainOnChanReq is NULL") );)
        return nSirStatus;
    }

     /* The link is not suspended */
    if (status != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog( pMac, LOGE, FL(" Suspend link Failure ") );)
        goto error;
    }


    if((psessionEntry = peFindSessionByBssid(
        pMac,pMac->lim.gpLimRemainOnChanReq->selfMacAddr, &sessionId)) != NULL)
    {
        goto change_channel;
    }
    else /* Session Entry does not exist for given BSSId */
    {
        /* Try to Create a new session */
        if(eSIR_SUCCESS != limCreateSessionForRemainOnChn(pMac, &psessionEntry))
        {
            limLog(pMac, LOGE, FL("Session Can not be created "));
            /* send remain on chn failure */
            goto error;
        }
    }

change_channel:
    /* change channel to the requested by RemainOn Chn*/
    limChangeChannelWithCallback(pMac,
                              pMac->lim.gpLimRemainOnChanReq->chnNum,
                              limRemainOnChnlSetLinkStat, NULL, psessionEntry);
     return eSIR_SUCCESS;

error:
     limRemainOnChnRsp(pMac,eHAL_STATUS_FAILURE, NULL);
     return eSIR_FAILURE;
}

void limRemainOnChnlSuspendLinkHdlr(tpAniSirGlobal pMac, eHalStatus status,
                                       tANI_U32 *data)
{
    limRemainOnChnlChangeChnReq(pMac, status, data);
    return;
}

/*------------------------------------------------------------------
 *
 * Set the LINK state to LISTEN to allow only PROBE_REQ and Action frames
 *
 *------------------------------------------------------------------*/
void limRemainOnChnlSetLinkStat(tpAniSirGlobal pMac, eHalStatus status,
                                tANI_U32 *data, tpPESession psessionEntry)
{
    tANI_U32 val;
    tSirRemainOnChnReq *MsgRemainonChannel = pMac->lim.gpLimRemainOnChanReq;
    tSirMacAddr             nullBssid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if (status != eHAL_STATUS_SUCCESS)
    {
        limLog( pMac, LOGE, FL("Change channel not successful"));
        goto error1;
    }

    // Start timer here to come back to operating channel.
    pMac->lim.limTimers.gLimRemainOnChannelTimer.sessionId =
                                                psessionEntry->peSessionId;
    pMac->lim.p2pRemOnChanTimeStamp = vos_timer_get_system_time();
    pMac->lim.gTotalScanDuration = MsgRemainonChannel->duration;

      /* get the duration from the request */
    val = SYS_MS_TO_TICKS(MsgRemainonChannel->duration);

    limLog( pMac, LOG2, "Start listen duration = %d", val);
    if (tx_timer_change(&pMac->lim.limTimers.gLimRemainOnChannelTimer,
                                                val, 0) != TX_SUCCESS)
    {
       /**
        * Could not change Remain on channel Timer. Log error.
        */
        limLog(pMac, LOGP,
               FL("Unable to change remain on channel Timer val"));
        goto error;
    }

    if(TX_SUCCESS !=
       tx_timer_activate(&pMac->lim.limTimers.gLimRemainOnChannelTimer))
    {
        limLog( pMac, LOGE,
                  "%s: remain on channel Timer Start Failed", __func__);
        goto error;
    }

    if ((limSetLinkState(pMac, MsgRemainonChannel->isProbeRequestAllowed?
                         eSIR_LINK_LISTEN_STATE:eSIR_LINK_SEND_ACTION_STATE,nullBssid,
                         pMac->lim.gSelfMacAddr, limSetLinkStateP2PCallback,
                         NULL)) != eSIR_SUCCESS)
    {
        limLog( pMac, LOGE, "Unable to change link state");
        goto error;
    }

    return;
error:
    limDeactivateAndChangeTimer(pMac, eLIM_REMAIN_CHN_TIMER);
error1:
    limRemainOnChnRsp(pMac,eHAL_STATUS_FAILURE, NULL);
    return;
}

/*------------------------------------------------------------------
 *
 * lim Insert NOA timer timeout callback - when timer fires, deactivate it and send
 * scan rsp to csr/hdd
 *
 *------------------------------------------------------------------*/
void limProcessInsertSingleShotNOATimeout(tpAniSirGlobal pMac)
{
    /* timeout means start NOA did not arrive; we need to deactivate and change the timer for
     * future activations
     */
    limDeactivateAndChangeTimer(pMac, eLIM_INSERT_SINGLESHOT_NOA_TIMER);

    /* Even if insert NOA timedout, go ahead and process/send stored SME request */
    limProcessRegdDefdSmeReqAfterNOAStart(pMac);

    return;
}

/*-----------------------------------------------------------------
 * lim Insert Timer callback function to check active DFS channels
 * and convert them to passive channels if there was no
 * beacon/proberesp for MAX_TIME_TO_BE_ACTIVE_CHANNEL time
 *------------------------------------------------------------------*/
void limConvertActiveChannelToPassiveChannel(tpAniSirGlobal pMac )
{
    tANI_U32 currentTime;
    tANI_U32 lastTime = 0;
    tANI_U32 timeDiff;
    tANI_U8 i;
    currentTime = vos_timer_get_system_time();
    for (i = 1; i < SIR_MAX_24G_5G_CHANNEL_RANGE ; i++)
    {
        if ((pMac->lim.dfschannelList.timeStamp[i]) != 0)
        {
            lastTime = pMac->lim.dfschannelList.timeStamp[i];
            if (currentTime >= lastTime)
            {
                timeDiff = (currentTime - lastTime);
            }
            else
            {
                timeDiff = (0xFFFFFFFF - lastTime) + currentTime;
            }

            if (timeDiff >= MAX_TIME_TO_BE_ACTIVE_CHANNEL)
            {
                limCovertChannelScanType( pMac, i,FALSE);
                pMac->lim.dfschannelList.timeStamp[i] = 0;
            }
        }
    }
    /* lastTime is zero if there is no DFS active channels in the list.
     * If this is non zero then we have active DFS channels so restart the timer.
    */
    if (lastTime != 0)
    {
        if (tx_timer_activate(
                         &pMac->lim.limTimers.gLimActiveToPassiveChannelTimer)
                          != TX_SUCCESS)
        {
            limLog(pMac, LOGE, FL("Could not activate Active to Passive Channel  timer"));
        }
    }

    return;

}

/*------------------------------------------------------------------
 *
 * limchannelchange callback, on success channel change, set the
 * link_state to LISTEN
 *
 *------------------------------------------------------------------*/

void limProcessRemainOnChnTimeout(tpAniSirGlobal pMac)
{
    tpPESession psessionEntry;
    tSirMacAddr             nullBssid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    //Timer might get extended while Sending Action Frame
    //In that case don't process Channel Timeout
    if (tx_timer_running(&pMac->lim.limTimers.gLimRemainOnChannelTimer))
    {
        limLog( pMac, LOGE,
                "still timer is running already and not processing limProcessRemainOnChnTimeout");
        return;
    }

    limDeactivateAndChangeTimer(pMac, eLIM_REMAIN_CHN_TIMER);

    if (NULL == pMac->lim.gpLimRemainOnChanReq)
    {
        limLog( pMac, LOGE, "No Remain on channel pending");
        return;
    }

    /* get the previous valid LINK state */
    if (limSetLinkState(pMac, eSIR_LINK_IDLE_STATE, nullBssid,
        pMac->lim.gSelfMacAddr, NULL, NULL) != eSIR_SUCCESS)
    {
        limLog( pMac, LOGE, "Unable to change link state");
        return;
    }

    if (pMac->lim.gLimMlmState  != eLIM_MLM_P2P_LISTEN_STATE )
    {
        limRemainOnChnRsp(pMac,eHAL_STATUS_SUCCESS, NULL);
    }
    else
    {
        /* get the session */
        if((psessionEntry = peFindSessionBySessionId(pMac,
            pMac->lim.limTimers.gLimRemainOnChannelTimer.sessionId))== NULL)
        {
            limLog(pMac, LOGE,
                  FL("Session Does not exist for given sessionID"));
            goto error;
        }

        limExitRemainOnChannel(pMac, eHAL_STATUS_SUCCESS, NULL, psessionEntry);
        return;
error:
        limRemainOnChnRsp(pMac,eHAL_STATUS_FAILURE, NULL);
    }
    return;
}


/*------------------------------------------------------------------
 *
 * limchannelchange callback, on success channel change, set the link_state
 * to LISTEN
 *
 *------------------------------------------------------------------*/

void limExitRemainOnChannel(tpAniSirGlobal pMac, eHalStatus status,
                         tANI_U32 *data, tpPESession psessionEntry)
{

    if (status != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog( pMac, LOGE, "Remain on Channel Failed");)
        goto error;
    }
    //Set the resume channel to Any valid channel (invalid).
    //This will instruct HAL to set it to any previous valid channel.
    peSetResumeChannel(pMac, 0, 0);
    limResumeLink(pMac, limRemainOnChnRsp, NULL);
    return;
error:
    limRemainOnChnRsp(pMac,eHAL_STATUS_FAILURE, NULL);
    return;
}

/*------------------------------------------------------------------
 *
 * Send remain on channel respone: Success/ Failure
 *
 *------------------------------------------------------------------*/
void limRemainOnChnRsp(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data)
{
    tpPESession psessionEntry;
    tANI_U8             sessionId;
    tSirRemainOnChnReq *MsgRemainonChannel = pMac->lim.gpLimRemainOnChanReq;
    tSirMacAddr             nullBssid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if ( NULL == MsgRemainonChannel )
    {
        PELOGE(limLog( pMac, LOGP,
             "%s: No Pointer for Remain on Channel Req", __func__);)
        return;
    }

    /* Incase of the Remain on Channel Failure Case
       Clean up Everything */
    if(eHAL_STATUS_FAILURE == status)
    {
       //Deactivate Remain on Channel Timer
       limDeactivateAndChangeTimer(pMac, eLIM_REMAIN_CHN_TIMER);

       //Set the Link State to Idle
       /* get the previous valid LINK state */
       if (limSetLinkState(pMac, eSIR_LINK_IDLE_STATE, nullBssid,
           pMac->lim.gSelfMacAddr, NULL, NULL) != eSIR_SUCCESS)
       {
           limLog( pMac, LOGE, "Unable to change link state");
       }

       pMac->lim.gLimSystemInScanLearnMode = 0;
       pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;
       SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    }

    /* delete the session */
    if((psessionEntry = peFindSessionByBssid(pMac,
                 MsgRemainonChannel->selfMacAddr,&sessionId)) != NULL)
    {
        if (LIM_IS_P2P_DEVICE_ROLE(psessionEntry)) {
            peDeleteSession( pMac, psessionEntry);
        }
    }

    /* Post the meessage to Sme */
    limSendSmeRsp(pMac, eWNI_SME_REMAIN_ON_CHN_RSP, status,
                  MsgRemainonChannel->sessionId, 0);

    vos_mem_free(pMac->lim.gpLimRemainOnChanReq);
    pMac->lim.gpLimRemainOnChanReq = NULL;

    pMac->lim.gLimMlmState = pMac->lim.gLimPrevMlmState;

    /* If remain on channel timer expired and action frame is pending then
     * indicaiton confirmation with status failure */
    if (pMac->lim.mgmtFrameSessionId != 0xff)
    {
       limP2PActionCnf(pMac, 0);
    }

    return;
}

/*------------------------------------------------------------------
 *
 * Indicate the Mgmt Frame received to SME to HDD callback
 * handle Probe_req/Action frame currently
 *
 *------------------------------------------------------------------*/
void limSendSmeMgmtFrameInd(
                    tpAniSirGlobal pMac, tANI_U8 frameType,
                    tANI_U8  *frame, tANI_U32 frameLen, tANI_U16 sessionId,
                    tANI_U32 rxChannel, tpPESession psessionEntry,
                    tANI_S8 rxRssi)
{
    tpSirSmeMgmtFrameInd pSirSmeMgmtFrame = NULL;
    tANI_U16              length;

    length = sizeof(tSirSmeMgmtFrameInd) + frameLen;

    pSirSmeMgmtFrame = vos_mem_malloc(length);
    if (NULL == pSirSmeMgmtFrame)
    {
        limLog(pMac, LOGP,
               FL("AllocateMemory failed for eWNI_SME_LISTEN_RSP"));
        return;
    }
    vos_mem_set((void*)pSirSmeMgmtFrame, length, 0);

    pSirSmeMgmtFrame->frame_len = frameLen;
    pSirSmeMgmtFrame->sessionId = sessionId;
    pSirSmeMgmtFrame->frameType = frameType;
    pSirSmeMgmtFrame->rxRssi = rxRssi;

    if (pMac->fP2pListenOffload)
        goto send_frame;

    /*
     *  Work around to address LIM sending wrong channel to HDD for p2p action
     *  frames(In case of auto GO) recieved on 5GHz channel.
     *  As RXP has only 4bits to store the channel, we need some mechanism to
     *  to distinguish between 2.4Ghz/5GHz channel. if gLimRemainOnChannelTImer
     *  is not running and if we get a frame then pass the Go session
     *  operating channel to HDD. Some vendors create separate p2p interface
     *  after group formation. In that case LIM session entry will be NULL for
     *  p2p device address. So search for p2p go session and pass it's
     *  operating channel.
     *  Need to revisit this path in case of GO+CLIENT concurrency.
     */
    if( VOS_FALSE ==
        tx_timer_running(&pMac->lim.limTimers.gLimRemainOnChannelTimer) )
    {
        tpPESession pTempSessionEntry = psessionEntry;
        if( ( (NULL != pTempSessionEntry) ||
              (pTempSessionEntry = limIsApSessionActive(pMac)) ) &&
            (SIR_BAND_5_GHZ == limGetRFBand(pTempSessionEntry->currentOperChannel)) )
        {
            rxChannel = pTempSessionEntry->currentOperChannel;
        }
    }

    if(VOS_TRUE == tx_timer_running(&pMac->lim.limTimers.gLimRemainOnChannelTimer) &&
            ( (psessionEntry != NULL) && (psessionEntry->pePersona != VOS_P2P_GO_MODE)) &&
            (frameType == SIR_MAC_MGMT_ACTION))
    {
            unsigned int chanWaitTime, vStatus ;

            limLog( pMac, LOG1, FL("Rx: Extend the gLimRemainOnChannelTimer = %d "),
                                                              pMac->lim.gTotalScanDuration);

            pMac->lim.p2pRemOnChanTimeStamp = vos_timer_get_system_time();

            chanWaitTime = SYS_MS_TO_TICKS(pMac->lim.gTotalScanDuration);
            vStatus = tx_timer_deactivate(&pMac->lim.limTimers.gLimRemainOnChannelTimer);

            if (VOS_STATUS_SUCCESS != vStatus)
            {
                limLog( pMac, LOGE, FL("Rx: Extend the gLimRemainOnChannelTimer"));
            }

            if (tx_timer_change(&pMac->lim.limTimers.gLimRemainOnChannelTimer, chanWaitTime, 0) != TX_SUCCESS)
            {
                limLog( pMac, LOGE, FL("Unable to change the gLimRemainOnChannelTimer"));
            }

            if (tx_timer_activate(&pMac->lim.limTimers.gLimRemainOnChannelTimer) != 0)
            {
                limLog( pMac, LOGE, FL("Unable to active the gLimRemainOnChannelTimer"));
            }
    }
    else
    {
       if(frameType == SIR_MAC_MGMT_ACTION)
            limLog( pMac, LOGE, FL("Rx: NO REMAIN ON CHANNEL and recd action frame "));
    }

send_frame:
    pSirSmeMgmtFrame->rxChan = rxChannel;

    vos_mem_zero(pSirSmeMgmtFrame->frameBuf,frameLen);
    vos_mem_copy(pSirSmeMgmtFrame->frameBuf,frame,frameLen);

    if (pMac->mgmt_frame_ind_cb)
       pMac->mgmt_frame_ind_cb(pSirSmeMgmtFrame);
    else
       limLog(pMac, LOGW,
             FL("Management indication callback not registered!!"));
    vos_mem_free(pSirSmeMgmtFrame);

    return;
} /*** end limSendSmeListenRsp() ***/


eHalStatus limP2PActionCnf(tpAniSirGlobal pMac, tANI_U32 txCompleteSuccess)
{
    if (pMac->lim.mgmtFrameSessionId != 0xff)
    {
        /* The session entry might be invalid(0xff) action confirmation received after
         * remain on channel timer expired */
        limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF,
                (txCompleteSuccess ? eSIR_SME_SUCCESS : eSIR_SME_SEND_ACTION_FAIL),
                pMac->lim.mgmtFrameSessionId, 0);
        pMac->lim.mgmtFrameSessionId = 0xff;
    }

    return eHAL_STATUS_SUCCESS;
}


void limSendP2PActionFrame(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    tSirMbMsgP2p *pMbMsg = (tSirMbMsgP2p *)pMsg->bodyptr;
    tANI_U32            nBytes;
    tANI_U8             *pFrame;
    void                *pPacket;
    eHalStatus          halstatus;
    tANI_U8             txFlag = 0;
    tpSirMacFrameCtl    pFc = (tpSirMacFrameCtl ) pMbMsg->data;
    tANI_U8             noaLen = 0;
    tANI_U8             noaStream[SIR_MAX_NOA_ATTR_LEN + (2*SIR_P2P_IE_HEADER_LEN)];
    tANI_U8             origLen = 0;
    tANI_U8             sessionId = 0;
    v_U8_t              *pP2PIe = NULL;
    tpPESession         psessionEntry = NULL;
    v_U8_t              *pPresenceRspNoaAttr = NULL;
    v_U8_t              *pNewP2PIe = NULL;
    v_U16_t             remainLen = 0;
    tANI_U8             smeSessionId = 0;
#ifdef WLAN_FEATURE_11W
    tpSirMacMgmtHdr        pMacHdr;
    tpSirMacActionFrameHdr pActionHdr;
#endif

    nBytes = pMbMsg->msgLen - sizeof(tSirMbMsg);

    limLog( pMac, LOG1, FL("sending pFc->type=%d pFc->subType=%d"),
                            pFc->type, pFc->subType);
    if (pMac->fP2pListenOffload)
    {
        if ((!pMac->lim.gpLimRemainOnChanReq) && (0 != pMbMsg->wait))
        {
            limLog(pMac, LOGE,
                    FL("Remain on channel is not running"));
            limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF,
                    eHAL_STATUS_FAILURE, pMbMsg->sessionId, 0);
            return;
        }
        smeSessionId = pMbMsg->sessionId;
        goto send_action_frame;
    }

    psessionEntry = peFindSessionByBssid(pMac,
                   (tANI_U8*)pMbMsg->data + BSSID_OFFSET, &sessionId);

    /* Check for session corresponding to ADDR2 As Supplicant is filling
       ADDR2  with BSSID */
    if( NULL == psessionEntry )
    {
        psessionEntry = peFindSessionByBssid(pMac,
                   (tANI_U8*)pMbMsg->data + ADDR2_OFFSET, &sessionId);
    }

    if( NULL == psessionEntry )
    {
        tANI_U8             isSessionActive = 0;
        tANI_U8             i;

        /* If we are not able to find psessionEntry entry, then try to find
           active session, if found any active sessions then send the
           action frame, If no active sessions found then drop the frame */
        for (i =0; i < pMac->lim.maxBssId;i++)
        {
            psessionEntry = peFindSessionBySessionId(pMac,i);
            if ( NULL != psessionEntry)
            {
                isSessionActive = 1;
                break;
            }
        }
        if( !isSessionActive )
        {
            limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF,
                          eHAL_STATUS_FAILURE, pMbMsg->sessionId, 0);
            return;
        }
    }

    smeSessionId = psessionEntry->smeSessionId;

send_action_frame:
    if ((SIR_MAC_MGMT_FRAME == pFc->type)&&
        ((SIR_MAC_MGMT_PROBE_RSP == pFc->subType)||
        (SIR_MAC_MGMT_ACTION == pFc->subType)))
    {
        //if this is a probe RSP being sent from wpa_supplicant
        if (SIR_MAC_MGMT_PROBE_RSP == pFc->subType)
        {
            //get proper offset for Probe RSP
            pP2PIe = limGetP2pIEPtr(pMac,
                          (tANI_U8*)pMbMsg->data + PROBE_RSP_IE_OFFSET,
                          nBytes - PROBE_RSP_IE_OFFSET);
            while ((NULL != pP2PIe) && (SIR_MAC_MAX_IE_LENGTH == pP2PIe[1]))
            {
                remainLen = nBytes - (pP2PIe - (tANI_U8*)pMbMsg->data);
                if (remainLen > 2)
                {
                     pNewP2PIe = limGetP2pIEPtr(pMac,
                                pP2PIe+SIR_MAC_MAX_IE_LENGTH + 2, remainLen);
                }
                if (pNewP2PIe)
                {
                    pP2PIe = pNewP2PIe;
                    pNewP2PIe = NULL;
                }
                else
                {
                    break;
                }
            } //end of while
        }
        else
        {
            if (SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY ==
                *((v_U8_t *)pMbMsg->data+ACTION_OFFSET))
            {
                tpSirMacP2PActionFrameHdr pActionHdr =
                    (tpSirMacP2PActionFrameHdr)((v_U8_t *)pMbMsg->data +
                                                        ACTION_OFFSET);
                if (vos_mem_compare( pActionHdr->Oui,
                    SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE ) &&
                    (SIR_MAC_ACTION_P2P_SUBTYPE_PRESENCE_RSP ==
                    pActionHdr->OuiSubType))
                { //In case of Presence RSP response
                    pP2PIe = limGetP2pIEPtr(pMac,
                                 (v_U8_t *)pMbMsg->data + ACTION_OFFSET +
                                 sizeof(tSirMacP2PActionFrameHdr),
                                 (nBytes - ACTION_OFFSET -
                                 sizeof(tSirMacP2PActionFrameHdr)));
                    if( NULL != pP2PIe )
                    {
                        //extract the presence of NoA attribute inside P2P IE
                        pPresenceRspNoaAttr =
                        limGetIEPtr(pMac,pP2PIe + SIR_P2P_IE_HEADER_LEN,
                                    pP2PIe[1], SIR_P2P_NOA_ATTR,TWO_BYTE);
                     }
                }
            }
        }

        if (pP2PIe != NULL)
        {
            //get NoA attribute stream P2P IE
            noaLen = limGetNoaAttrStream(pMac, noaStream, psessionEntry);
            //need to append NoA attribute in P2P IE
            if (noaLen > 0)
            {
                origLen = pP2PIe[1];
               //if Presence Rsp has NoAttr
                if (pPresenceRspNoaAttr)
                {
                    v_U16_t noaAttrLen = pPresenceRspNoaAttr[1] |
                                        (pPresenceRspNoaAttr[2]<<8);
                    /*One byte for attribute, 2bytes for length*/
                    origLen -= (noaAttrLen + 1 + 2);
                    //remove those bytes to copy
                    nBytes  -= (noaAttrLen + 1 + 2);
                    //remove NoA from original Len
                    pP2PIe[1] = origLen;
                }
                if ((pP2PIe[1] + (tANI_U16)noaLen)> SIR_MAC_MAX_IE_LENGTH)
                {
                    //Form the new NoA Byte array in multiple P2P IEs
                    noaLen = limGetNoaAttrStreamInMultP2pIes(pMac, noaStream,
                                 noaLen,((pP2PIe[1] + (tANI_U16)noaLen)-
                                 SIR_MAC_MAX_IE_LENGTH));
                    pP2PIe[1] = SIR_MAC_MAX_IE_LENGTH;
                }
                else
                {
                    pP2PIe[1] += noaLen; //increment the length of P2P IE
                }
                nBytes += noaLen;
                limLog( pMac, LOGE,
                        FL("noaLen=%d origLen=%d pP2PIe=%p"
                        " nBytes=%d nBytesToCopy=%zu "),
                                   noaLen,origLen,pP2PIe,nBytes,
                   ((pP2PIe + origLen + 2) - (v_U8_t *)pMbMsg->data));
            }
        }

        if (SIR_MAC_MGMT_PROBE_RSP == pFc->subType)
        {
            lim_set_ht_caps(pMac, psessionEntry,
                            (tANI_U8*)pMbMsg->data + PROBE_RSP_IE_OFFSET,
                            nBytes - PROBE_RSP_IE_OFFSET);
        }

        if (pMac->fP2pListenOffload)
            goto send_frame1;

        /* The minimum wait for any action frame should be atleast 100 ms.
         * If supplicant sends action frame at the end of already running remain on channel time
         * Then there is a chance to miss the response of the frame. So increase the remain on channel
         * time for all action frame to make sure that we receive the response frame */
        if ((SIR_MAC_MGMT_ACTION == pFc->subType) &&
                (0 != pMbMsg->wait))
        {
            if (tx_timer_running(&pMac->lim.limTimers.gLimRemainOnChannelTimer))
            {
                tANI_U32 val = 0;
                tx_timer_deactivate(&pMac->lim.limTimers.gLimRemainOnChannelTimer);
                /* get the duration from the request */
                pMac->lim.p2pRemOnChanTimeStamp = vos_timer_get_system_time();
                pMac->lim.gTotalScanDuration = pMbMsg->wait;

                val = SYS_MS_TO_TICKS(pMbMsg->wait);

                limLog(pMac, LOG1,
                        FL("Tx: Extending the gLimRemainOnChannelTimer"));
                if (tx_timer_change(
                            &pMac->lim.limTimers.gLimRemainOnChannelTimer, val, 0)
                        != TX_SUCCESS)
                {
                    limLog(pMac, LOGP,
                            FL("Unable to change remain on channel Timer val"));
                    return;
                }
                else if(TX_SUCCESS != tx_timer_activate(
                            &pMac->lim.limTimers.gLimRemainOnChannelTimer))
                {
                    limLog(pMac, LOGP,
                            FL("Unable to activate remain on channel Timer"));
                    limDeactivateAndChangeTimer(pMac, eLIM_REMAIN_CHN_TIMER);
                    return;
                }
            }
            else
            {
                limLog(pMac, LOGE,
                            FL("Failed to Send Action frame"));
                limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF,
                        eHAL_STATUS_FAILURE, pMbMsg->sessionId, 0);
                return;
            }
        }
    }

send_frame1:
    // Ok-- try to allocate some memory:
    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                      (tANI_U16)nBytes, ( void** ) &pFrame, (void**) &pPacket);
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to allocate %d bytes for a Probe"
          " Request."), nBytes );
        return;
    }

    // Paranoia:
    vos_mem_set(pFrame, nBytes, 0);

    /* Add sequence number to action frames */
    /* Frames are handed over in .11 format by supplicant already */
    limPopulateP2pMacHeader(pMac, (tANI_U8*)pMbMsg->data);

    if ((noaLen > 0) && (noaLen<(SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN)))
    {
        // Add 2 bytes for length and Arribute field
        v_U32_t nBytesToCopy = ((pP2PIe + origLen + 2 ) -
                                (v_U8_t *)pMbMsg->data);
        vos_mem_copy(pFrame, pMbMsg->data, nBytesToCopy);
        vos_mem_copy((pFrame + nBytesToCopy), noaStream, noaLen);
        vos_mem_copy((pFrame + nBytesToCopy + noaLen),
        pMbMsg->data + nBytesToCopy, nBytes - nBytesToCopy - noaLen);

    }
    else
    {
        vos_mem_copy(pFrame, pMbMsg->data, nBytes);
    }
#ifdef WLAN_FEATURE_11W
    pActionHdr = (tpSirMacActionFrameHdr) (pFrame + sizeof(tSirMacMgmtHdr));
    pMacHdr = (tpSirMacMgmtHdr)pFrame;
    /*
     * Setting Protected bit only for Robust Action Frames
     * This has to be based on the current Connection with the station
     * limSetProtectedBit API will set the protected bit if connection is PMF
     */
    psessionEntry = peFindSessionByBssid(pMac,
         (tANI_U8*)pMbMsg->data + BSSID_OFFSET, &sessionId);

    /*
     * Check for session corresponding to ADDR2 ss supplicant is filling
     *  ADDR2  with BSSID
     */
    if (NULL == psessionEntry)
       psessionEntry = peFindSessionByBssid(pMac,
               (tANI_U8*)pMbMsg->data + ADDR2_OFFSET, &sessionId);

    if (psessionEntry && (SIR_MAC_MGMT_ACTION == pFc->subType) &&
        psessionEntry->limRmfEnabled && (!limIsGroupAddr(pMacHdr->da)) &&
        lim_is_robust_mgmt_action_frame(pActionHdr->category)) {
        limSetProtectedBit(pMac, psessionEntry, pMacHdr->da, pMacHdr);
    }
#endif

    /* Use BD rate 2 for all P2P related frames. As these frames need to go
     * at OFDM rates. And BD rate2 we configured at 6Mbps.
     */
    txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

    if ( (SIR_MAC_MGMT_PROBE_RSP == pFc->subType) ||
         (pMbMsg->noack)
        )
    {
        halstatus = halTxFrame( pMac, pPacket, (tANI_U16)nBytes,
                        HAL_TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
                        7, limTxComplete, pFrame,
                        txFlag, smeSessionId );

        if (!pMbMsg->noack)
        {
           limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF,
               halstatus, pMbMsg->sessionId, 0);
        }
        pMac->lim.mgmtFrameSessionId = 0xff;
    }
    else
    {
        pMac->lim.mgmtFrameSessionId = pMbMsg->sessionId;
        halstatus = halTxFrameWithTxComplete( pMac, pPacket, (tANI_U16)nBytes,
                        HAL_TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
                        7, limTxComplete, pFrame,
                        limP2PActionCnf, txFlag, smeSessionId, false );

        if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
        {
             limLog( pMac, LOGE, FL("could not send action frame!" ));
             limSendSmeRsp(pMac, eWNI_SME_ACTION_FRAME_SEND_CNF, halstatus,
                pMbMsg->sessionId, 0);
             pMac->lim.mgmtFrameSessionId = 0xff;
        }
        else
        {
             pMac->lim.mgmtFrameSessionId = pMbMsg->sessionId;
             limLog( pMac, LOG2, FL("lim.actionFrameSessionId = %u" ),
                     pMac->lim.mgmtFrameSessionId);

        }
    }

    return;
}


void limAbortRemainOnChan(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    if (pMac->fP2pListenOffload)
    {
        limProcessAbortScanInd(pMac, sessionId);
        return;
    }

    if(VOS_TRUE == tx_timer_running(
                &pMac->lim.limTimers.gLimRemainOnChannelTimer))
    {
        //TODO check for state and take appropriate actions
        limDeactivateAndChangeTimer(pMac, eLIM_REMAIN_CHN_TIMER);
        limProcessRemainOnChnTimeout(pMac);
    }
    return;
}

/* Power Save Related Functions */
tSirRetStatus __limProcessSmeNoAUpdate(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpP2pPsConfig pNoA;
    tpP2pPsParams pMsgNoA;
    tSirMsgQ msg;

    pNoA = (tpP2pPsConfig) pMsgBuf;

    pMsgNoA = vos_mem_malloc(sizeof( tP2pPsConfig ));
    if (NULL == pMsgNoA)
    {
        limLog( pMac, LOGE,
                     FL( "Unable to allocate memory during NoA Update" ));
        return eSIR_MEM_ALLOC_FAILED;
    }

    vos_mem_set((tANI_U8 *)pMsgNoA, sizeof(tP2pPsConfig), 0);
    pMsgNoA->opp_ps = pNoA->opp_ps;
    pMsgNoA->ctWindow = pNoA->ctWindow;
    pMsgNoA->duration = pNoA->duration;
    pMsgNoA->interval = pNoA->interval;
    pMsgNoA->count = pNoA->count;
    pMsgNoA->single_noa_duration = pNoA->single_noa_duration;
    pMsgNoA->psSelection = pNoA->psSelection;
    pMsgNoA->sessionId = pNoA->sessionid;

    msg.type = WDA_SET_P2P_GO_NOA_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMsgNoA;
    msg.bodyval = 0;

    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
        limLog(pMac, LOGE, FL("halPostMsgApi failed"));
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} /*** end __limProcessSmeGoNegReq() ***/
