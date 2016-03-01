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

/*
 * This file limLinkMonitoringAlgo.cc contains the code for
 * Link monitoring algorithm on AP and heart beat failure
 * handling on STA.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "aniGlobal.h"
#include "wniCfgSta.h"
#include "cfgApi.h"


#include "schApi.h"
#include "pmmApi.h"
#include "utilsApi.h"
#include "limAssocUtils.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limPropExtsUtils.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
#include "vos_diag_core_log.h"
#endif //FEATURE_WLAN_DIAG_SUPPORT
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "limFTDefs.h"
#endif
#include "limSession.h"
#include "limSerDesUtils.h"


/**
 * limSendKeepAliveToPeer()
 *
 *FUNCTION:
 * This function is called to send Keep alive message to peer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac        - Pointer to Global MAC structure
 * @return None
 */

void
limSendKeepAliveToPeer(tpAniSirGlobal pMac)
{

} /*** limSendKeepAliveToPeer() ***/


/** ---------------------------------------------------------
\fn      limDeleteStaContext
\brief   This function handles the message from HAL:
\        WDA_DELETE_STA_CONTEXT_IND. This function
\        validates that the given station id exist, and if so,
\        deletes the station by calling limTriggerSTAdeletion.
\param   tpAniSirGlobal pMac
\param   tpSirMsgQ      limMsg
\return  none
  -----------------------------------------------------------*/
void
limDeleteStaContext(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpDeleteStaContext  pMsg = (tpDeleteStaContext)limMsg->bodyptr;
    tpDphHashNode       pStaDs;
    tpPESession psessionEntry ;

    if(NULL == pMsg)
    {
        PELOGE(limLog(pMac, LOGE,FL("Invalid body pointer in message"));)
        return;
    }
    psessionEntry = pe_find_session_by_sme_session_id(pMac, pMsg->vdev_id);
    if(NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("session not found for given sme session"));
        vos_mem_free(pMsg);
        return;
    }

    switch(pMsg->reasonCode)
    {
        case HAL_DEL_STA_REASON_CODE_KEEP_ALIVE:
             pStaDs = dphLookupAssocId(pMac, pMsg->staId, &pMsg->assocId,
                            &psessionEntry->dph.dphHashTable);

             if (!pStaDs) {
                 PELOGE(limLog(pMac, LOGE,
                               FL("Skip STA deletion (invalid STA) limSystemRole=%d"),
                               GET_LIM_SYSTEM_ROLE(psessionEntry));)
                 vos_mem_free(pMsg);
                 return;
             }

             /*
              * check and see if same staId. This is to avoid the scenario
              * where we're trying to delete a staId we just added.
              */
             if (pStaDs->staIndex != pMsg->staId) {
                 PELOGE(limLog(pMac, LOGE,
                               FL("staid mismatch: %d vs %d "), pStaDs->staIndex, pMsg->staId);)
                 vos_mem_free(pMsg);
                 return;
             }

             /*
              * Check if Deauth/Disassoc is triggered from Host.
              * If mlmState is in some transient state then
              * don't trigger STA deletion to avoid the race
              * condition.
              */
             if ((pStaDs &&
                 ((pStaDs->mlmStaContext.mlmState !=
                   eLIM_MLM_LINK_ESTABLISHED_STATE) &&
                  (pStaDs->mlmStaContext.mlmState !=
                   eLIM_MLM_WT_ASSOC_CNF_STATE) &&
                  (pStaDs->mlmStaContext.mlmState !=
                   eLIM_MLM_ASSOCIATED_STATE)))) {
                   PELOGE(limLog(pMac, LOGE,
                              FL("received Del STA context in some transit state(staId: %d, assocId: %d)"),
                              pMsg->staId, pMsg->assocId);)
                vos_mem_free(pMsg);
                return;
             }

             if (LIM_IS_STA_ROLE(psessionEntry) && !pMsg->is_tdls) {
                 /*
                  * If roaming is in progress, then ignore the STA kick out
                  * and let the connection happen. The roaming_in_progress
                  * flag is set whenever a candidate found indication is
                  * received. It is enabled on the PE session for which
                  * the indication is received. There is really no need to
                  * re-set the flag, since the PE session on which it was
                  * set will be deleted, even if roaming is success or failure.
                  * When roaming is a success, the PE session for AP1 is
                  * deleted. When we get a candidate indication, it would be
                  * on the PE session of the AP1. AP2 to which we are about to
                  * roam will have a new PE session ID.If roaming fails for
                  * any reason, then it will anyways delete the PE session of
                  * of the AP1.
                  */
                 if (psessionEntry->roaming_in_progress ||
                      limIsReassocInProgress(pMac, psessionEntry)) {
                     limLog(pMac, LOGE,
                        FL("roam_progress=%d, reassoc=%d. Not disconnecting"),
                           psessionEntry->roaming_in_progress,
                           limIsReassocInProgress(pMac, psessionEntry));
                     vos_mem_free(pMsg);
                     return;
                 }
                 pStaDs = dphGetHashEntry(pMac,
                                          DPH_STA_HASH_INDEX_PEER,
                                          &psessionEntry->dph.dphHashTable);
                 if (NULL == pStaDs) {
                     limLog(pMac, LOGE, FL("Dph entry not found."));
                     vos_mem_free(pMsg);
                     return;
                 }
                 pStaDs->del_sta_ctx_rssi = pMsg->rssi;
                 limSendDeauthMgmtFrame(pMac,
                                   eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON,
                                   pMsg->addr2, psessionEntry, FALSE);
                 limTearDownLinkWithAp(pMac, psessionEntry->peSessionId,
                                 eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON);
                 /* only break for STA role (non TDLS) */
                 break;
             }

             limLog(pMac, LOGE, FL("Deleting sta: staId %d, reasonCode %d"),
                             pMsg->staId, pMsg->reasonCode);
             if (LIM_IS_IBSS_ROLE(psessionEntry)) {
                 vos_mem_free(pMsg);
                 return;
             }
             if (LIM_IS_BT_AMP_AP_ROLE(psessionEntry) ||
                 LIM_IS_AP_ROLE(psessionEntry)) {
                 PELOG1(limLog(pMac, LOG1, FL("SAP:lim Delete Station Context (staId: %d, assocId: %d) "),
                             pMsg->staId, pMsg->assocId);)
                 limSendDisassocMgmtFrame(pMac,
                                     eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON,
                                     pStaDs->staAddr, psessionEntry, FALSE);
                 limTriggerSTAdeletion(pMac, pStaDs, psessionEntry);
             }
#ifdef FEATURE_WLAN_TDLS
             else
             {
                if (LIM_IS_STA_ROLE(psessionEntry) &&
                    STA_ENTRY_TDLS_PEER == pStaDs->staType) {
                    //TeardownLink with PEER
                    //Reason code HAL_DEL_STA_REASON_CODE_KEEP_ALIVE means
                    //eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE
                    limSendSmeTDLSDelStaInd(pMac, pStaDs, psessionEntry,
                                       eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE);
                }
             }
#endif
             break;

        case HAL_DEL_STA_REASON_CODE_UNKNOWN_A2:
             PELOGE(limLog(pMac, LOGE, FL(" Deleting Unknown station "));)
             limPrintMacAddr(pMac, pMsg->addr2, LOGE);
             limSendDeauthMgmtFrame( pMac, eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON, pMsg->addr2, psessionEntry, FALSE);
             break;

        default:
             PELOGE(limLog(pMac, LOGE, FL(" Unknown reason code "));)
             break;

    }
    vos_mem_free(pMsg);
    return;
}


/**
 * limTriggerSTAdeletion()
 *
 *FUNCTION:
 * This function is called to trigger STA context deletion
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac   - Pointer to global MAC structure
 * @param  pStaDs - Pointer to internal STA Datastructure
 * @return None
 */
void
limTriggerSTAdeletion(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry)
{
    tLimMlmDisassocInd mlmDisassocInd;

    if (!pStaDs)
    {
        PELOGW(limLog(pMac, LOGW, FL("Skip STA deletion (invalid STA)"));)
        return;
    }

    if ((pStaDs->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
        (pStaDs->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_BSS_RSP_STATE)) {
        /* Already in the process of deleting context for the peer */
        PELOGE(limLog(pMac, LOGE,
                FL("Deletion is in progress for peer:%p"), pStaDs->staAddr);)
        return;
    }

    pStaDs->mlmStaContext.disassocReason =
             eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON;
    pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DISASSOC;
    vos_mem_copy(&mlmDisassocInd.peerMacAddr, pStaDs->staAddr,
                 sizeof(tSirMacAddr));
    mlmDisassocInd.reasonCode = eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON;
    mlmDisassocInd.disassocTrigger = eLIM_LINK_MONITORING_DISASSOC;

    /* Update PE session Id  */
    mlmDisassocInd.sessionId = psessionEntry->peSessionId;
    limPostSmeMessage(pMac, LIM_MLM_DISASSOC_IND,
                      (tANI_U32 *) &mlmDisassocInd);
    // Issue Disassoc Indication to SME.
    limSendSmeDisassocInd(pMac, pStaDs, psessionEntry);
} /*** end limTriggerSTAdeletion() ***/



/**
 * limTearDownLinkWithAp()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limTearDownLinkWithAp(tpAniSirGlobal pMac, tANI_U8 sessionId, tSirMacReasonCodes reasonCode)
{
    tpDphHashNode pStaDs = NULL;

    //tear down the following sessionEntry
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, sessionId))== NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID"));
        return;
    }
    /**
     * Heart beat failed for upto threshold value
     * and AP did not respond for Probe request.
     * Trigger link tear down.
     */
    if(pMac->psOffloadEnabled)
        psessionEntry->pmmOffloadInfo.bcnmiss = FALSE;
    else
        pMac->pmm.inMissedBeaconScenario = FALSE;

    limLog(pMac, LOGW,
       FL("No ProbeRsp from AP after HB failure. Tearing down link"));

    // Deactivate heartbeat timer
    limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);

    // Announce loss of link to Roaming algorithm
    // and cleanup by sending SME_DISASSOC_REQ to SME

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);


    if (pStaDs != NULL)
    {
        tLimMlmDeauthInd  mlmDeauthInd;

#ifdef FEATURE_WLAN_TDLS
        /* Delete all TDLS peers connected before leaving BSS*/
        limDeleteTDLSPeers(pMac, psessionEntry);
#endif

        pStaDs->mlmStaContext.disassocReason = reasonCode;
        pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DEAUTH;

        /// Issue Deauth Indication to SME.
        vos_mem_copy((tANI_U8 *) &mlmDeauthInd.peerMacAddr,
                      pStaDs->staAddr,
                      sizeof(tSirMacAddr));

        /*
         * if sendDeauthBeforeCon is enabled and reasoncode is
         * Beacon Missed Store the MAC of AP in the flip flop
         * buffer. This MAC will be used to send Deauth before
         * connection, if we connect to same AP after HB failure.
        */
        if (pMac->roam.configParam.sendDeauthBeforeCon &&
                          eSIR_BEACON_MISSED == reasonCode)
        {
            int apCount = pMac->lim.gLimHeartBeatApMacIndex;

            if (pMac->lim.gLimHeartBeatApMacIndex)
                pMac->lim.gLimHeartBeatApMacIndex = 0;
            else
                pMac->lim.gLimHeartBeatApMacIndex = 1;

            limLog(pMac, LOGE, FL("HB Failure on MAC "
                   MAC_ADDRESS_STR" Store it on Index %d"),
                   MAC_ADDR_ARRAY(pStaDs->staAddr),apCount);

            sirCopyMacAddr(pMac->lim.gLimHeartBeatApMac[apCount],
                                                    pStaDs->staAddr);
        }

        mlmDeauthInd.reasonCode    = (tANI_U8) pStaDs->mlmStaContext.disassocReason;
        mlmDeauthInd.deauthTrigger =  pStaDs->mlmStaContext.cleanupTrigger;

        limPostSmeMessage(pMac, LIM_MLM_DEAUTH_IND, (tANI_U32 *) &mlmDeauthInd);

        limSendSmeDeauthInd(pMac, pStaDs, psessionEntry);
        limReInitScanResults(pMac);
    }
} /*** limTearDownLinkWithAp() ***/




/**
 * limHandleHeartBeatFailure()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void limHandleHeartBeatFailure(tpAniSirGlobal pMac,tpPESession psessionEntry)
{

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    vos_log_beacon_update_pkt_type *log_ptr = NULL;
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* If gLimHeartBeatTimer fires between the interval of sending WDA_ENTER_BMPS_REQUEST
     * to the HAL and receiving WDA_ENTER_BMPS_RSP from the HAL, then LIM (PE) tries to Process the
     * SIR_LIM_HEAR_BEAT_TIMEOUT message but The PE state is ePMM_STATE_BMPS_SLEEP so PE dont
     * want to handle heartbeat timeout in the BMPS, because Firmware handles it in BMPS.
     * So just return from heartbeatfailure handler
     */
    if(!pMac->psOffloadEnabled)
    {
        if(!IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE && (!limIsSystemInActiveState(pMac)))
           return;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_beacon_update_pkt_type, LOG_WLAN_BEACON_UPDATE_C);
    if(log_ptr)
        log_ptr->bcn_rx_cnt = psessionEntry->LimRxedBeaconCntDuringHB;
    WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* Ensure HB Status for the session has been reseted */
    psessionEntry->LimHBFailureStatus = eANI_BOOLEAN_FALSE;

    if ((LIM_IS_STA_ROLE(psessionEntry) ||
         LIM_IS_BT_AMP_STA_ROLE(psessionEntry)) &&
         (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) &&
         (psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
         (psessionEntry->limSmeState != eLIM_SME_WT_DEAUTH_STATE))
    {
        if (!pMac->sys.gSysEnableLinkMonitorMode)
            return;

        /**
         * Beacon frame not received within heartbeat timeout.
         */
        PELOGW(limLog(pMac, LOGW, FL("Heartbeat Failure"));)
        pMac->lim.gLimHBfailureCntInLinkEstState++;

        /**
         * Check if connected on the DFS channel, if not connected on
         * DFS channel then only send the probe request otherwise tear down the link
         */
        if(!limIsconnectedOnDFSChannel(psessionEntry->currentOperChannel))
        {
            /*** Detected continuous Beacon Misses ***/
            psessionEntry->LimHBFailureStatus= eANI_BOOLEAN_TRUE;

            /*Reset the HB packet count before sending probe*/
            limResetHBPktCount(psessionEntry);
            /**
             * Send Probe Request frame to AP to see if
             * it is still around. Wait until certain
             * timeout for Probe Response from AP.
             */
            PELOGW(limLog(pMac, LOGW, FL("Heart Beat missed from AP. Sending Probe Req"));)
            /* for searching AP, we don't include any additional IE */
            limSendProbeReqMgmtFrame(pMac, &psessionEntry->ssId, psessionEntry->bssId,
                                      psessionEntry->currentOperChannel,psessionEntry->selfMacAddr,
                                      psessionEntry->dot11mode, 0, NULL);
        }
        else
        {
            PELOGW(limLog(pMac, LOGW,
              FL("Heart Beat missed from AP on DFS chanel moving to passive"));)
            if (psessionEntry->currentOperChannel < SIR_MAX_24G_5G_CHANNEL_RANGE){
               limCovertChannelScanType(pMac, psessionEntry->currentOperChannel, false);
               pMac->lim.dfschannelList.timeStamp[psessionEntry->currentOperChannel] = 0;
            }
            /* Connected on DFS channel so should not send the probe request
            * tear down the link directly */
            limTearDownLinkWithAp(pMac, psessionEntry->peSessionId,
                                                       eSIR_BEACON_MISSED);
        }
    }
    else
    {
        /**
             * Heartbeat timer may have timed out
            * while we're doing background scanning/learning
            * or in states other than link-established state.
            * Log error.
            */
        PELOG1(limLog(pMac, LOG1, FL("received heartbeat timeout in state %X"),
               psessionEntry->limMlmState);)
        limPrintMlmState(pMac, LOG1, psessionEntry->limMlmState);
        pMac->lim.gLimHBfailureCntInOtherStates++;
        limReactivateHeartBeatTimer(pMac, psessionEntry);
    }
} /*** limHandleHeartBeatFailure() ***/
