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

#ifdef WLAN_FEATURE_VOWIFI_11R
/**=========================================================================

  \brief implementation for PE 11r VoWiFi FT Protocol

  ========================================================================*/

/* $Header$ */


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <limSendMessages.h>
#include <limTypes.h>
#include <limFT.h>
#include <limFTDefs.h>
#include <limUtils.h>
#include <limPropExtsUtils.h>
#include <limAssocUtils.h>
#include <limSession.h>
#include <limSessionUtils.h>
#include <limAdmitControl.h>
#include "wmmApsd.h"

extern void limSendSetStaKeyReq( tpAniSirGlobal pMac,
    tLimMlmSetKeysReq *pMlmSetKeysReq,
    tANI_U16 staIdx,
    tANI_U8 defWEPIdx,
    tpPESession sessionEntry,
    tANI_BOOLEAN sendRsp);

/*--------------------------------------------------------------------------
  Initialize the FT variables.
  ------------------------------------------------------------------------*/
void limFTOpen(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
   if (psessionEntry)
      vos_mem_set(&psessionEntry->ftPEContext, sizeof(tftPEContext), 0);
}

/*--------------------------------------------------------------------------
  Clean up FT variables.
  ------------------------------------------------------------------------*/
void limFTCleanupPreAuthInfo(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
   tpPESession pReAssocSessionEntry = NULL;
   tANI_U8 sessionId = 0;
   if (!psessionEntry) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, "%s: psessionEntry is NULL", __func__);)
#endif
      return;
   }

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
      return;
   }

   if (psessionEntry->ftPEContext.pFTPreAuthReq) {
      pReAssocSessionEntry =
         peFindSessionByBssid(pMac,
               psessionEntry->ftPEContext.pFTPreAuthReq->preAuthbssId,
               &sessionId);

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOG1(limLog( pMac, LOG1, FL("Freeing pFTPreAuthReq= %p"),
             psessionEntry->ftPEContext.pFTPreAuthReq);)
#endif
      if (psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription) {
         vos_mem_free(
            psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription);
         psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription = NULL;
      }
      vos_mem_free(psessionEntry->ftPEContext.pFTPreAuthReq);
      psessionEntry->ftPEContext.pFTPreAuthReq = NULL;
   }

   if (psessionEntry->ftPEContext.pAddBssReq) {
      vos_mem_free(psessionEntry->ftPEContext.pAddBssReq);
      psessionEntry->ftPEContext.pAddBssReq = NULL;
   }

   if (psessionEntry->ftPEContext.pAddStaReq) {
      vos_mem_free(psessionEntry->ftPEContext.pAddStaReq);
      psessionEntry->ftPEContext.pAddStaReq = NULL;
   }

   /* The session is being deleted, cleanup the contents */
   vos_mem_set(&psessionEntry->ftPEContext, sizeof(tftPEContext), 0);

   /* Delete the session created while handling pre-auth response */
   if (pReAssocSessionEntry) {
      /* If we have successful pre-auth response, then we would have
       * created a session on which reassoc request will be sent
       */
      if (pReAssocSessionEntry->valid &&
            pReAssocSessionEntry->limSmeState == eLIM_SME_WT_REASSOC_STATE) {
         limLog( pMac, LOG1, FL("Deleting Preauth Session %d"),
                pReAssocSessionEntry->peSessionId);
         peDeleteSession(pMac, pReAssocSessionEntry);
      }
   }
}

void limFTCleanupAllFTSessions(tpAniSirGlobal pMac)
{
   /* Wrapper function to cleanup all FT sessions */
   int i;

   for (i = 0; i < pMac->lim.maxBssId; i++) {
      if (VOS_TRUE == pMac->lim.gpSession[i].valid) {
         /* The session is valid, may have FT data */
         limFTCleanup(pMac, &pMac->lim.gpSession[i]);
      }
   }
}

void limFTCleanup(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
   if (NULL == psessionEntry) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is NULL"));)
#endif
      return;
   }

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
      return;
   }

   if (NULL != psessionEntry->ftPEContext.pFTPreAuthReq) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOG1(limLog( pMac, LOG1, FL("Freeing pFTPreAuthReq= %p"),
             psessionEntry->ftPEContext.pFTPreAuthReq);)
#endif
      if (NULL != psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription) {
         vos_mem_free(
               psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription);
         psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription = NULL;
      }
      vos_mem_free(psessionEntry->ftPEContext.pFTPreAuthReq);
      psessionEntry->ftPEContext.pFTPreAuthReq = NULL;
   }

   if (psessionEntry->ftPEContext.pAddBssReq) {
      vos_mem_free(psessionEntry->ftPEContext.pAddBssReq);
      psessionEntry->ftPEContext.pAddBssReq = NULL;
   }

   if (psessionEntry->ftPEContext.pAddStaReq) {
      vos_mem_free(psessionEntry->ftPEContext.pAddStaReq);
      psessionEntry->ftPEContext.pAddStaReq = NULL;
   }

   /* The session is being deleted, cleanup the contents */
   vos_mem_set(&psessionEntry->ftPEContext, sizeof(tftPEContext), 0);
}

/*------------------------------------------------------------------
 *
 * This is the handler after suspending the link.
 * We suspend the link and then now proceed to switch channel.
 *
 *------------------------------------------------------------------*/
void static
limFTPreAuthSuspendLinkHandler(tpAniSirGlobal pMac, eHalStatus status,
                                 tANI_U32 *data)
{
    tpPESession psessionEntry = (tpPESession)data;

    /* The link is suspended of not */
    if (NULL == psessionEntry ||
        NULL == psessionEntry->ftPEContext.pFTPreAuthReq ||
        status != eHAL_STATUS_SUCCESS) {
        PELOGE(limLog( pMac, LOGE,
               FL("preAuth error, status = %d"), status);)
        limPostFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
        return;
    }

    /* Suspended, now move to a different channel.
     * Perform some sanity check before proceeding
     */
    if (psessionEntry->ftPEContext.pFTPreAuthReq) {
       limChangeChannelWithCallback(pMac,
             psessionEntry->ftPEContext.pFTPreAuthReq->preAuthchannelNum,
             limPerformFTPreAuth, NULL, psessionEntry);
       return;
    }
}


/*--------------------------------------------------------------------------
  In this function, we process the FT Pre Auth Req.
  We receive Pre-Auth
  Suspend link
  Register a call back
  In the call back, we will need to accept frames from the new bssid
  Send out the auth req to new AP.
  Start timer and when the timer is done or if we receive the Auth response
  We change channel
  Resume link
  ------------------------------------------------------------------------*/
int limProcessFTPreAuthReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    int bufConsumed = FALSE;
    tpPESession psessionEntry;
    tANI_U8 sessionId;
    tpSirFTPreAuthReq ftPreAuthReq =
            (tSirFTPreAuthReq *)pMsg->bodyptr;

    if (NULL == ftPreAuthReq) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog(pMac, LOGE, FL("tSirFTPreAuthReq is NULL"));)
#endif
        return bufConsumed;
    }

    /* Get the current session entry */
    psessionEntry =
         peFindSessionByBssid(pMac, ftPreAuthReq->currbssId, &sessionId);
    if (psessionEntry == NULL) {
        PELOGE(limLog( pMac, LOGE,
               FL("Unable to find session for the following bssid"));)
        limPrintMacAddr( pMac, ftPreAuthReq->currbssId, LOGE );

        /* Post the FT Pre Auth Response to SME */
        limPostFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
        /* return FALSE, since the Pre-Auth Req will be freed in
         * limPostFTPreAuthRsp on failure
         */
        return bufConsumed;
    }

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       bufConsumed = TRUE;
       return bufConsumed;
    }

    /* Can set it only after sending auth */
    psessionEntry->ftPEContext.ftPreAuthStatus = eSIR_FAILURE;
    psessionEntry->ftPEContext.ftPreAuthSession = VOS_TRUE;

    /* Indicate that this is the session on which preauth is being done */
    if (psessionEntry->ftPEContext.pFTPreAuthReq) {
       if (psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription)
       {
          vos_mem_free(
               psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription);
          psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription = NULL;
       }
       vos_mem_free(psessionEntry->ftPEContext.pFTPreAuthReq);
       psessionEntry->ftPEContext.pFTPreAuthReq = NULL;
    }

    /* We need information from the Pre-Auth Req. Lets save that */
    psessionEntry->ftPEContext.pFTPreAuthReq = ftPreAuthReq;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog( pMac, LOG1, FL("PRE Auth ft_ies_length=%02x%02x%02x"),
        psessionEntry->ftPEContext.pFTPreAuthReq->ft_ies[0],
        psessionEntry->ftPEContext.pFTPreAuthReq->ft_ies[1],
        psessionEntry->ftPEContext.pFTPreAuthReq->ft_ies[2]);)
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT,
                       psessionEntry, 0, 0);
#endif

    /* Dont need to suspend if APs are in same channel and DUT is not in MCC state*/
    if ((psessionEntry->currentOperChannel !=
        psessionEntry->ftPEContext.pFTPreAuthReq->preAuthchannelNum)
        || limIsInMCC(pMac)) {
       /* Need to suspend link only if the channels are different */
       PELOG2(limLog(pMac, LOG2, FL("Performing pre-auth on different"
               " channel (session %p)"), psessionEntry);)
       limSuspendLink(pMac, eSIR_CHECK_ROAMING_SCAN,
                      limFTPreAuthSuspendLinkHandler,
                      (tANI_U32 *)psessionEntry);
    } else {
       PELOG2(limLog(pMac, LOG2, FL("Performing pre-auth on same"
                " channel (session %p)"), psessionEntry);)
        /* We are in the same channel. Perform pre-auth */
       limPerformFTPreAuth(pMac, eHAL_STATUS_SUCCESS, NULL, psessionEntry);
    }

    return bufConsumed;
}

/*------------------------------------------------------------------
 * Send the Auth1
 * Receive back Auth2
 *------------------------------------------------------------------*/
void limPerformFTPreAuth(tpAniSirGlobal pMac, eHalStatus status,
                         tANI_U32 *data, tpPESession psessionEntry)
{
    tSirMacAuthFrameBody authFrame;

    if (NULL == psessionEntry) {
        PELOGE(limLog(pMac, LOGE, FL("psessionEntry is NULL"));)
        return;
    }

    if (psessionEntry->is11Rconnection &&
        psessionEntry->ftPEContext.pFTPreAuthReq) {
        /* Only 11r assoc has FT IEs */
        if (psessionEntry->ftPEContext.pFTPreAuthReq->ft_ies == NULL) {
            PELOGE(limLog( pMac, LOGE,
                           "%s: FTIEs for Auth Req Seq 1 is absent",
                           __func__);)
            goto preauth_fail;
        }
    }

    if (status != eHAL_STATUS_SUCCESS) {
        PELOGE(limLog( pMac, LOGE,
                       "%s: Change channel not successful for FT pre-auth",
                       __func__);)
        goto preauth_fail;
    }

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       return;
    }

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOG2(limLog(pMac,LOG2,"Entered wait auth2 state for FT"
           " (old session %p)", psessionEntry);)
#endif

    if (psessionEntry->is11Rconnection) {
        /* Now we are on the right channel and need to send out Auth1 and
         * receive Auth2
         */
        authFrame.authAlgoNumber = eSIR_FT_AUTH;
    }
#if defined FEATURE_WLAN_ESE || defined FEATURE_WLAN_LFR
    else {
        /* Will need to make isESEconnection a enum may be for further
         * improvements to this to match this algorithm number
         */
        authFrame.authAlgoNumber = eSIR_OPEN_SYSTEM;
    }
#endif
    authFrame.authTransactionSeqNumber = SIR_MAC_AUTH_FRAME_1;
    authFrame.authStatusCode = 0;

    /* Start timer here to come back to operating channel */
    pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId =
                                       psessionEntry->peSessionId;
    if(TX_SUCCESS !=
         tx_timer_activate(&pMac->lim.limTimers.gLimFTPreAuthRspTimer)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOGE(limLog( pMac, LOGE, FL("FT Auth Rsp Timer Start Failed"));)
#endif
        goto preauth_fail;
    }
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, psessionEntry->peSessionId,
                    eLIM_FT_PREAUTH_RSP_TIMER));

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOG1(limLog( pMac, LOG1, FL("FT Auth Rsp Timer Started"));)
#endif
#ifdef FEATURE_WLAN_DIAG_SUPPORT
        limDiagEventReport(pMac, WLAN_PE_DIAG_ROAM_AUTH_START_EVENT,
                           pMac->lim.pSessionEntry, eSIR_SUCCESS, eSIR_SUCCESS);
#endif

    limSendAuthMgmtFrame(pMac, &authFrame,
        psessionEntry->ftPEContext.pFTPreAuthReq->preAuthbssId,
        LIM_NO_WEP_IN_FC, psessionEntry, eSIR_FALSE);
    return;

preauth_fail:
    limHandleFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
    return;
}

/*------------------------------------------------------------------
 *
 * Create the new Add Bss Req to the new AP.
 * This will be used when we are ready to FT to the new AP.
 * The newly created ft Session entry is passed to this function
 *
 *------------------------------------------------------------------*/
tSirRetStatus limFTPrepareAddBssReq( tpAniSirGlobal pMac,
    tANI_U8 updateEntry, tpPESession pftSessionEntry,
    tpSirBssDescription bssDescription )
{
    tpAddBssParams pAddBssParams = NULL;
    tANI_U8 i;
    tANI_U8 chanWidthSupp = 0;
    tSchBeaconStruct *pBeaconStruct;

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(pftSessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       return eSIR_FAILURE;
    }

    pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
    if (NULL == pBeaconStruct) {
       limLog(pMac, LOGE,
              FL("Unable to allocate memory for creating ADD_BSS") );
       return eSIR_MEM_ALLOC_FAILED;
    }

    // Package SIR_HAL_ADD_BSS_REQ message parameters
    pAddBssParams = vos_mem_malloc(sizeof( tAddBssParams ));
    if (NULL == pAddBssParams) {
        vos_mem_free(pBeaconStruct);
        limLog( pMac, LOGP,
                FL( "Unable to allocate memory for creating ADD_BSS" ));
        return (eSIR_MEM_ALLOC_FAILED);
    }

    vos_mem_set((tANI_U8 *) pAddBssParams, sizeof( tAddBssParams ), 0);

    limExtractApCapabilities( pMac,
        (tANI_U8 *) bssDescription->ieFields,
        limGetIElenFromBssDescription( bssDescription ), pBeaconStruct );

    if (pMac->lim.gLimProtectionControl !=
                                    WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideStaProtectionOnAssoc(pMac, pBeaconStruct, pftSessionEntry);

    vos_mem_copy(pAddBssParams->bssId, bssDescription->bssId,
                 sizeof(tSirMacAddr));

    // Fill in tAddBssParams selfMacAddr
    vos_mem_copy(pAddBssParams->selfMacAddr, pftSessionEntry->selfMacAddr,
                 sizeof(tSirMacAddr));

    pAddBssParams->bssType = pftSessionEntry->bssType;
    pAddBssParams->operMode = BSS_OPERATIONAL_MODE_STA;

    pAddBssParams->beaconInterval = bssDescription->beaconInterval;

    pAddBssParams->dtimPeriod = pBeaconStruct->tim.dtimPeriod;
    pAddBssParams->updateBss = updateEntry;

    pAddBssParams->reassocReq = true;

    pAddBssParams->cfParamSet.cfpCount = pBeaconStruct->cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod = pBeaconStruct->cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration =
                                      pBeaconStruct->cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining =
                                      pBeaconStruct->cfParamSet.cfpDurRemaining;


    pAddBssParams->rateSet.numRates = pBeaconStruct->supportedRates.numRates;
    vos_mem_copy(pAddBssParams->rateSet.rate,
                 pBeaconStruct->supportedRates.rate,
                 pBeaconStruct->supportedRates.numRates);

    pAddBssParams->nwType = bssDescription->nwType;

    pAddBssParams->shortSlotTimeSupported =
                           (tANI_U8)pBeaconStruct->capabilityInfo.shortSlotTime;
    pAddBssParams->llaCoexist =
                           (tANI_U8) pftSessionEntry->beaconParams.llaCoexist;
    pAddBssParams->llbCoexist =
                           (tANI_U8) pftSessionEntry->beaconParams.llbCoexist;
    pAddBssParams->llgCoexist =
                           (tANI_U8) pftSessionEntry->beaconParams.llgCoexist;
    pAddBssParams->ht20Coexist =
                           (tANI_U8) pftSessionEntry->beaconParams.ht20Coexist;
#ifdef WLAN_FEATURE_11W
    pAddBssParams->rmfEnabled = pftSessionEntry->limRmfEnabled;
#endif

    // Use the advertised capabilities from the received beacon/PR
    if (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode) &&
        ( pBeaconStruct->HTCaps.present ))
    {
        pAddBssParams->htCapable = pBeaconStruct->HTCaps.present;
        vos_mem_copy(&pAddBssParams->staContext.capab_info,
                     &pBeaconStruct->capabilityInfo,
                     sizeof(pAddBssParams->staContext.capab_info));
        vos_mem_copy(&pAddBssParams->staContext.ht_caps,
                     (tANI_U8 *)&pBeaconStruct->HTCaps + sizeof(tANI_U8),
                     sizeof(pAddBssParams->staContext.ht_caps));

        if ( pBeaconStruct->HTInfo.present )
        {
            pAddBssParams->htOperMode =
                          (tSirMacHTOperatingMode)pBeaconStruct->HTInfo.opMode;
            pAddBssParams->dualCTSProtection =
                          ( tANI_U8 ) pBeaconStruct->HTInfo.dualCTSProtection;

            chanWidthSupp = limGetHTCapability( pMac,
                                                eHT_SUPPORTED_CHANNEL_WIDTH_SET,
                                                pftSessionEntry);
            if( (pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp) )
            {
                pAddBssParams->txChannelWidthSet =
                  ( tANI_U8 ) pBeaconStruct->HTInfo.recommendedTxWidthSet;
                pAddBssParams->currentExtChannel =
                  pBeaconStruct->HTInfo.secondaryChannelOffset;
            }
            else
            {
                pAddBssParams->txChannelWidthSet =
                     WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                pAddBssParams->currentExtChannel = PHY_SINGLE_CHANNEL_CENTERED;
            }
            pAddBssParams->llnNonGFCoexist =
                (tANI_U8)pBeaconStruct->HTInfo.nonGFDevicesPresent;
            pAddBssParams->fLsigTXOPProtectionFullSupport =
                (tANI_U8)pBeaconStruct->HTInfo.lsigTXOPProtectionFullSupport;
            pAddBssParams->fRIFSMode = pBeaconStruct->HTInfo.rifsMode;
        }
    }

    pAddBssParams->currentOperChannel = bssDescription->channelId;
    pftSessionEntry->htSecondaryChannelOffset =
                pAddBssParams->currentExtChannel;

#ifdef WLAN_FEATURE_11AC
    if (pftSessionEntry->vhtCapability &&
        pftSessionEntry->vhtCapabilityPresentInBeacon)
    {
        pAddBssParams->vhtCapable = pBeaconStruct->VHTCaps.present;
        pAddBssParams->vhtTxChannelWidthSet =
                              pBeaconStruct->VHTOperation.chanWidth;
        pAddBssParams->currentExtChannel =
                   limGet11ACPhyCBState(pMac,
                                        pAddBssParams->currentOperChannel,
                                        pAddBssParams->currentExtChannel,
                                        pftSessionEntry->apCenterChan,
                                        pftSessionEntry);
        pAddBssParams->staContext.vht_caps =
             ((pBeaconStruct->VHTCaps.maxMPDULen <<
                                          SIR_MAC_VHT_CAP_MAX_MPDU_LEN) |
              (pBeaconStruct->VHTCaps.supportedChannelWidthSet <<
                                       SIR_MAC_VHT_CAP_SUPP_CH_WIDTH_SET) |
              (pBeaconStruct->VHTCaps.ldpcCodingCap <<
                                       SIR_MAC_VHT_CAP_LDPC_CODING_CAP) |
              (pBeaconStruct->VHTCaps.shortGI80MHz <<
                                       SIR_MAC_VHT_CAP_SHORTGI_80MHZ) |
              (pBeaconStruct->VHTCaps.shortGI160and80plus80MHz <<
                                       SIR_MAC_VHT_CAP_SHORTGI_160_80_80MHZ) |
              (pBeaconStruct->VHTCaps.txSTBC << SIR_MAC_VHT_CAP_TXSTBC) |
              (pBeaconStruct->VHTCaps.rxSTBC << SIR_MAC_VHT_CAP_RXSTBC) |
              (pBeaconStruct->VHTCaps.suBeamFormerCap <<
                                       SIR_MAC_VHT_CAP_SU_BEAMFORMER_CAP) |
              (pBeaconStruct->VHTCaps.suBeamformeeCap <<
                                       SIR_MAC_VHT_CAP_SU_BEAMFORMEE_CAP) |
              (pBeaconStruct->VHTCaps.csnofBeamformerAntSup <<
                                   SIR_MAC_VHT_CAP_CSN_BEAMORMER_ANT_SUP) |
              (pBeaconStruct->VHTCaps.numSoundingDim <<
                                        SIR_MAC_VHT_CAP_NUM_SOUNDING_DIM) |
              (pBeaconStruct->VHTCaps.muBeamformerCap <<
                                      SIR_MAC_VHT_CAP_NUM_BEAM_FORMER_CAP)|
              (pBeaconStruct->VHTCaps.muBeamformeeCap <<
                                     SIR_MAC_VHT_CAP_NUM_BEAM_FORMEE_CAP) |
              (pBeaconStruct->VHTCaps.vhtTXOPPS << SIR_MAC_VHT_CAP_TXOPPS) |
              (pBeaconStruct->VHTCaps.htcVHTCap << SIR_MAC_VHT_CAP_HTC_CAP) |
              (pBeaconStruct->VHTCaps.maxAMPDULenExp <<
                                 SIR_MAC_VHT_CAP_MAX_AMDU_LEN_EXPO) |
              (pBeaconStruct->VHTCaps.vhtLinkAdaptCap <<
                                    SIR_MAC_VHT_CAP_LINK_ADAPT_CAP) |
              (pBeaconStruct->VHTCaps.rxAntPattern <<
                                SIR_MAC_VHT_CAP_RX_ANTENNA_PATTERN) |
              (pBeaconStruct->VHTCaps.txAntPattern <<
                                SIR_MAC_VHT_CAP_TX_ANTENNA_PATTERN) |
              (pBeaconStruct->VHTCaps.reserved1 << SIR_MAC_VHT_CAP_RESERVED2));
    }
    else
    {
        pAddBssParams->vhtCapable = 0;
    }
#endif

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL( "SIR_HAL_ADD_BSS_REQ with channel = %d..." ),
        pAddBssParams->currentOperChannel);
#endif

    // Populate the STA-related parameters here
    // Note that the STA here refers to the AP
    {
        pAddBssParams->staContext.staType = STA_ENTRY_OTHER;

        vos_mem_copy(pAddBssParams->staContext.bssId,
                     bssDescription->bssId,
                     sizeof(tSirMacAddr));
        pAddBssParams->staContext.listenInterval =
                     bssDescription->beaconInterval;

        pAddBssParams->staContext.assocId = 0;
        pAddBssParams->staContext.uAPSD = 0;
        pAddBssParams->staContext.maxSPLen = 0;
        pAddBssParams->staContext.shortPreambleSupported =
                        (tANI_U8)pBeaconStruct->capabilityInfo.shortPreamble;
        pAddBssParams->staContext.updateSta = updateEntry;
        pAddBssParams->staContext.encryptType = pftSessionEntry->encryptType;
#ifdef WLAN_FEATURE_11W
        pAddBssParams->staContext.rmfEnabled = pftSessionEntry->limRmfEnabled;
#endif

        if (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode) &&
                                           ( pBeaconStruct->HTCaps.present )) {
            pAddBssParams->staContext.us32MaxAmpduDuration = 0;
            pAddBssParams->staContext.htCapable = 1;
            pAddBssParams->staContext.greenFieldCapable  =
                                   ( tANI_U8 ) pBeaconStruct->HTCaps.greenField;
            pAddBssParams->staContext.lsigTxopProtection =
                           ( tANI_U8 ) pBeaconStruct->HTCaps.lsigTXOPProtection;
            if ((pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp)) {
                pAddBssParams->staContext.txChannelWidthSet =
                        ( tANI_U8 )pBeaconStruct->HTInfo.recommendedTxWidthSet;
            }
            else {
                pAddBssParams->staContext.txChannelWidthSet =
                        WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }
#ifdef WLAN_FEATURE_11AC
            if (pftSessionEntry->vhtCapability &&
                IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps))
            {
                pAddBssParams->staContext.vhtCapable = 1;
                if ((pBeaconStruct->VHTCaps.suBeamFormerCap ||
                     pBeaconStruct->VHTCaps.muBeamformerCap) &&
                     pftSessionEntry->txBFIniFeatureEnabled)
                {
                    pAddBssParams->staContext.vhtTxBFCapable = 1;
                }
            }
#endif
            if ((pBeaconStruct->HTCaps.supportedChannelWidthSet) &&
                (chanWidthSupp))
            {
                pAddBssParams->staContext.txChannelWidthSet =
                       (tANI_U8)pBeaconStruct->HTInfo.recommendedTxWidthSet;
#ifdef WLAN_FEATURE_11AC
                if (pAddBssParams->staContext.vhtCapable)
                {
                    pAddBssParams->staContext.vhtTxChannelWidthSet =
                        pBeaconStruct->VHTOperation.chanWidth;
                }
#endif
            }
            else
            {
                pAddBssParams->staContext.txChannelWidthSet =
                  WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            }
            pAddBssParams->staContext.mimoPS             =
               (tSirMacHTMIMOPowerSaveState)pBeaconStruct->HTCaps.mimoPowerSave;
            pAddBssParams->staContext.delBASupport       =
               ( tANI_U8 ) pBeaconStruct->HTCaps.delayedBA;
            pAddBssParams->staContext.maxAmsduSize       =
               ( tANI_U8 ) pBeaconStruct->HTCaps.maximalAMSDUsize;
            pAddBssParams->staContext.maxAmpduDensity    =
               pBeaconStruct->HTCaps.mpduDensity;
            pAddBssParams->staContext.fDsssCckMode40Mhz =
               (tANI_U8)pBeaconStruct->HTCaps.dsssCckMode40MHz;
            pAddBssParams->staContext.fShortGI20Mhz =
               (tANI_U8)pBeaconStruct->HTCaps.shortGI20MHz;
            pAddBssParams->staContext.fShortGI40Mhz =
               (tANI_U8)pBeaconStruct->HTCaps.shortGI40MHz;
            pAddBssParams->staContext.maxAmpduSize =
               pBeaconStruct->HTCaps.maxRxAMPDUFactor;

            if( pBeaconStruct->HTInfo.present )
                pAddBssParams->staContext.rifsMode =
                pBeaconStruct->HTInfo.rifsMode;
        }

        if ((pftSessionEntry->limWmeEnabled && pBeaconStruct->wmeEdcaPresent) ||
                (pftSessionEntry->limQosEnabled && pBeaconStruct->edcaPresent))
            pAddBssParams->staContext.wmmEnabled = 1;
        else
            pAddBssParams->staContext.wmmEnabled = 0;

        pAddBssParams->staContext.wpa_rsn = pBeaconStruct->rsnPresent;
        /* For OSEN Connection AP does not advertise RSN or WPA IE
         * so from the IEs we get from supplicant we get this info
         * so for FW to transmit EAPOL message 4 we shall set
         * wpa_rsn
         */
        pAddBssParams->staContext.wpa_rsn |= (pBeaconStruct->wpaPresent << 1);
        if ((!pAddBssParams->staContext.wpa_rsn) &&
            (pftSessionEntry->isOSENConnection))
            pAddBssParams->staContext.wpa_rsn = 1;
        //Update the rates
#ifdef WLAN_FEATURE_11AC
        limPopulatePeerRateSet(pMac, &pAddBssParams->staContext.supportedRates,
                             pBeaconStruct->HTCaps.supportedMCSSet,
                             false,pftSessionEntry,&pBeaconStruct->VHTCaps);
#else
        limPopulatePeerRateSet(pMac, &pAddBssParams->staContext.supportedRates,
                   beaconStruct.HTCaps.supportedMCSSet, false,pftSessionEntry);
#endif
        if (pftSessionEntry->htCapability)
        {
           pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11n;
           if (pftSessionEntry->vhtCapability)
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11ac;
        }
        else
        {
           if (pftSessionEntry->limRFBand == SIR_BAND_5_GHZ)
           {
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11a;
           }
           else
           {
              pAddBssParams->staContext.supportedRates.opRateMode = eSTA_11bg;
           }
        }
    }

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
        pAddBssParams->staContext.staTCParams[i].txUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].rxUseBA    = eBA_DISABLE;
        pAddBssParams->staContext.staTCParams[i].txBApolicy =
                                                      eBA_POLICY_IMMEDIATE;
        pAddBssParams->staContext.staTCParams[i].rxBApolicy =
                                                      eBA_POLICY_IMMEDIATE;
    }

#if defined WLAN_FEATURE_VOWIFI
    pAddBssParams->maxTxPower = pftSessionEntry->maxTxPower;
#endif

#ifdef WLAN_FEATURE_11W
    if (pftSessionEntry->limRmfEnabled)
    {
        pAddBssParams->rmfEnabled = 1;
        pAddBssParams->staContext.rmfEnabled = 1;
    }
#endif

    pAddBssParams->status = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = true;

    pAddBssParams->staContext.sessionId = pftSessionEntry->peSessionId;
    pAddBssParams->staContext.smesessionId = pftSessionEntry->smeSessionId;
    pAddBssParams->sessionId = pftSessionEntry->peSessionId;

    // Set a new state for MLME

    pftSessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, pftSessionEntry->peSessionId,
                    eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE));
    pAddBssParams->halPersona=(tANI_U8)pftSessionEntry->pePersona;

    pftSessionEntry->ftPEContext.pAddBssReq = pAddBssParams;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOG1, FL( "Saving SIR_HAL_ADD_BSS_REQ for pre-auth ap..." ));
#endif

    vos_mem_free(pBeaconStruct);
    return 0;
}


/*------------------------------------------------------------------
 *
 * Setup the new session for the pre-auth AP.
 * Return the newly created session entry.
 *
 *------------------------------------------------------------------*/
void limFillFTSession(tpAniSirGlobal pMac,
    tpSirBssDescription  pbssDescription,
    tpPESession       pftSessionEntry,
    tpPESession psessionEntry)
{
   tANI_U8           currentBssUapsd;
   tPowerdBm         localPowerConstraint;
   tPowerdBm         regMax;
   tSchBeaconStruct  *pBeaconStruct;
   tANI_U32          selfDot11Mode;
   ePhyChanBondState cbEnabledMode;

   pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
   if (NULL == pBeaconStruct) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      limLog(pMac, LOGE,
             FL("Unable to allocate memory for creating limFillFTSession") );
#endif
      return;
   }

   /* Retrieve the session that has already been created and update the entry */
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
   limPrintMacAddr(pMac, pbssDescription->bssId, LOG1);
#endif
   pftSessionEntry->limWmeEnabled = psessionEntry->limWmeEnabled;
   pftSessionEntry->limQosEnabled = psessionEntry->limQosEnabled;
   pftSessionEntry->limWsmEnabled = psessionEntry->limWsmEnabled;
   pftSessionEntry->lim11hEnable = psessionEntry->lim11hEnable;
   pftSessionEntry->isOSENConnection = psessionEntry->isOSENConnection;

   // Fields to be filled later
   pftSessionEntry->pLimJoinReq = NULL;
   pftSessionEntry->smeSessionId = psessionEntry->smeSessionId;
   pftSessionEntry->transactionId = 0;

   limExtractApCapabilities( pMac,
         (tANI_U8 *) pbssDescription->ieFields,
         limGetIElenFromBssDescription( pbssDescription ),
         pBeaconStruct );

   pftSessionEntry->rateSet.numRates = pBeaconStruct->supportedRates.numRates;
   vos_mem_copy(pftSessionEntry->rateSet.rate,
    pBeaconStruct->supportedRates.rate, pBeaconStruct->supportedRates.numRates);

   pftSessionEntry->extRateSet.numRates = pBeaconStruct->extendedRates.numRates;
   vos_mem_copy(pftSessionEntry->extRateSet.rate,
    pBeaconStruct->extendedRates.rate, pftSessionEntry->extRateSet.numRates);

   pftSessionEntry->ssId.length = pBeaconStruct->ssId.length;
   vos_mem_copy(pftSessionEntry->ssId.ssId, pBeaconStruct->ssId.ssId,
         pftSessionEntry->ssId.length);

   wlan_cfgGetInt(pMac, WNI_CFG_DOT11_MODE, &selfDot11Mode);
   limLog(pMac, LOG1, FL("selfDot11Mode %d"),selfDot11Mode );
   pftSessionEntry->dot11mode = selfDot11Mode;
   pftSessionEntry->vhtCapability =
         (IS_DOT11_MODE_VHT(pftSessionEntry->dot11mode)
         && IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps));
   pftSessionEntry->htCapability = (IS_DOT11_MODE_HT(pftSessionEntry->dot11mode)
         && pBeaconStruct->HTCaps.present);
#ifdef WLAN_FEATURE_11AC
   if (IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps) && pBeaconStruct->VHTOperation.present)
   {
      pftSessionEntry->vhtCapabilityPresentInBeacon = 1;
      pftSessionEntry->apCenterChan =
         pBeaconStruct->VHTOperation.chanCenterFreqSeg1;
      pftSessionEntry->apChanWidth = pBeaconStruct->VHTOperation.chanWidth;
   }
   else
   {
      pftSessionEntry->vhtCapabilityPresentInBeacon = 0;
   }
#endif
   sirCopyMacAddr(pftSessionEntry->selfMacAddr, psessionEntry->selfMacAddr);
   sirCopyMacAddr(pftSessionEntry->limReAssocbssId, pbssDescription->bssId);
   sirCopyMacAddr(pftSessionEntry->prev_ap_bssid, psessionEntry->bssId);
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
   limPrintMacAddr(pMac, pftSessionEntry->limReAssocbssId, LOG1);
#endif

   /* Store beaconInterval */
   pftSessionEntry->beaconParams.beaconInterval =
         pbssDescription->beaconInterval;
   pftSessionEntry->bssType = psessionEntry->bssType;

   pftSessionEntry->statypeForBss = STA_ENTRY_PEER;
   pftSessionEntry->nwType = pbssDescription->nwType;

   /* Copy The channel Id to the session Table */
   pftSessionEntry->limReassocChannelId = pbssDescription->channelId;
   pftSessionEntry->currentOperChannel = pbssDescription->channelId;

   if (pftSessionEntry->bssType == eSIR_INFRASTRUCTURE_MODE)
   {
      pftSessionEntry->limSystemRole = eLIM_STA_ROLE;
   }
   else if(pftSessionEntry->bssType == eSIR_BTAMP_AP_MODE)
   {
      pftSessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
   }
   else
   {
      /* Throw an error and return and make sure to delete the session.*/
      limLog(pMac, LOGE, FL("Invalid bss type"));
   }

   pftSessionEntry->limCurrentBssCaps = pbssDescription->capabilityInfo;
   pftSessionEntry->limReassocBssCaps = pbssDescription->capabilityInfo;
   if( pMac->roam.configParam.shortSlotTime &&
         SIR_MAC_GET_SHORT_SLOT_TIME(pftSessionEntry->limReassocBssCaps))
   {
      pftSessionEntry->shortSlotTimeSupported = TRUE;
   }

   regMax = cfgGetRegulatoryMaxTransmitPower(pMac,
                                          pftSessionEntry->currentOperChannel );
   localPowerConstraint = regMax;
   limExtractApCapability( pMac, (tANI_U8 *) pbssDescription->ieFields,
         limGetIElenFromBssDescription(pbssDescription),
         &pftSessionEntry->limCurrentBssQosCaps,
         &pftSessionEntry->limCurrentBssPropCap,
         &currentBssUapsd , &localPowerConstraint, psessionEntry);

   pftSessionEntry->limReassocBssQosCaps =
      pftSessionEntry->limCurrentBssQosCaps;
   pftSessionEntry->limReassocBssPropCap =
      pftSessionEntry->limCurrentBssPropCap;

#ifdef WLAN_FEATURE_VOWIFI_11R
    pftSessionEntry->is11Rconnection = psessionEntry->is11Rconnection;
#endif
#ifdef FEATURE_WLAN_ESE
    pftSessionEntry->isESEconnection = psessionEntry->isESEconnection;
    pftSessionEntry->is_ese_version_ie_present =
                                 pBeaconStruct->is_ese_ver_ie_present;
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    pftSessionEntry->isFastTransitionEnabled =
                     psessionEntry->isFastTransitionEnabled;
#endif

#ifdef FEATURE_WLAN_LFR
    pftSessionEntry->isFastRoamIniFeatureEnabled =
                     psessionEntry->isFastRoamIniFeatureEnabled;
#endif

#ifdef FEATURE_WLAN_ESE
    pftSessionEntry->maxTxPower =
        limGetMaxTxPower(regMax, localPowerConstraint,
                         pMac->roam.configParam.nTxPowerCap);
#else
   pftSessionEntry->maxTxPower = VOS_MIN( regMax , (localPowerConstraint) );
#endif

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   limLog(pMac, LOG1,
         FL("Reg max = %d, local power = %d, ini tx power = %d, max tx = %d"),
         regMax, localPowerConstraint, pMac->roam.configParam.nTxPowerCap,
         pftSessionEntry->maxTxPower);
#endif

   pftSessionEntry->limRFBand =
         limGetRFBand(pftSessionEntry->currentOperChannel);

   pftSessionEntry->limPrevSmeState = pftSessionEntry->limSmeState;
   pftSessionEntry->limSmeState = eLIM_SME_WT_REASSOC_STATE;
   MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, pftSessionEntry->peSessionId,
          pftSessionEntry->limSmeState));

   pftSessionEntry->encryptType = psessionEntry->encryptType;
#ifdef WLAN_FEATURE_11W
   pftSessionEntry->limRmfEnabled = psessionEntry->limRmfEnabled;
#endif

   if (pftSessionEntry->limRFBand == SIR_BAND_2_4_GHZ)
   {
      cbEnabledMode = pMac->roam.configParam.channelBondingMode24GHz;
   }
   else
   {
      cbEnabledMode = pMac->roam.configParam.channelBondingMode5GHz;
   }
   pftSessionEntry->htSupportedChannelWidthSet =
      (pBeaconStruct->HTInfo.present)?
      (cbEnabledMode && pBeaconStruct->HTInfo.recommendedTxWidthSet):0;
   pftSessionEntry->htRecommendedTxWidthSet =
      pftSessionEntry->htSupportedChannelWidthSet;

   vos_mem_free(pBeaconStruct);
}

/*------------------------------------------------------------------
 *
 * Setup the session and the add bss req for the pre-auth AP.
 *
 *------------------------------------------------------------------*/
tSirRetStatus limFTSetupAuthSession(tpAniSirGlobal pMac,
                                    tpPESession psessionEntry)
{
   tpPESession pftSessionEntry = NULL;
   tANI_U8 sessionId = 0;

   pftSessionEntry =
      peFindSessionByBssid(pMac, psessionEntry->limReAssocbssId, &sessionId);
   if (pftSessionEntry == NULL) {
      PELOGE(limLog(pMac, LOGE,
               FL("Unable to find session for the following bssid"));)
         limPrintMacAddr(pMac, psessionEntry->limReAssocbssId, LOGE);
      return eSIR_FAILURE;
   }

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
      return eSIR_FAILURE;
   }

   if (psessionEntry->ftPEContext.pFTPreAuthReq &&
       psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription) {
      limFillFTSession(pMac,
            psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription,
            pftSessionEntry,
            psessionEntry);

      limFTPrepareAddBssReq( pMac, FALSE, pftSessionEntry,
            psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription );
   }

   return eSIR_SUCCESS;
}

/*------------------------------------------------------------------
 * Resume Link Call Back
 *------------------------------------------------------------------*/
void limFTProcessPreAuthResult(tpAniSirGlobal pMac, eHalStatus status,
                               tANI_U32 *data)
{
   tpPESession psessionEntry = (tpPESession)data;

   if (NULL == psessionEntry ||
       NULL == psessionEntry->ftPEContext.pFTPreAuthReq)
      return;

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
         return;
   }

   if (psessionEntry->ftPEContext.ftPreAuthStatus == eSIR_SUCCESS) {
      psessionEntry->ftPEContext.ftPreAuthStatus =
         limFTSetupAuthSession(pMac, psessionEntry);
   }

   // Post the FT Pre Auth Response to SME
   limPostFTPreAuthRsp(pMac, psessionEntry->ftPEContext.ftPreAuthStatus,
         psessionEntry->ftPEContext.saved_auth_rsp,
         psessionEntry->ftPEContext.saved_auth_rsp_length, psessionEntry);
}

/*------------------------------------------------------------------
 * Resume Link Call Back
 *------------------------------------------------------------------*/
void limPerformPostFTPreAuthAndChannelChange(tpAniSirGlobal pMac,
                                          eHalStatus status,
                                          tANI_U32 *data,
                                          tpPESession psessionEntry)
{
    /* Set the resume channel to Any valid channel (invalid)
     * This will instruct HAL to set it to any previous valid channel.
     */
    peSetResumeChannel(pMac, 0, 0);
    limResumeLink(pMac, limFTProcessPreAuthResult, (tANI_U32 *)psessionEntry);
}

/*------------------------------------------------------------------
 *
 *  Will post pre auth response to SME.
 *
 *------------------------------------------------------------------*/
void limPostFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
    tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
    tpPESession psessionEntry)
{
   tpSirFTPreAuthRsp pFTPreAuthRsp;
   tSirMsgQ          mmhMsg;
   tANI_U16 rspLen = sizeof(tSirFTPreAuthRsp);

   pFTPreAuthRsp = (tpSirFTPreAuthRsp)vos_mem_malloc(rspLen);
   if (NULL == pFTPreAuthRsp) {
      PELOGE(limLog( pMac, LOGE, "Failed to allocate memory");)
         VOS_ASSERT(pFTPreAuthRsp != NULL);
      return;
   }
   vos_mem_zero( pFTPreAuthRsp, rspLen);

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   PELOGE(limLog( pMac, LOG1, FL("Auth Rsp = %p"), pFTPreAuthRsp);)
#endif

   if (psessionEntry) {
       /* Nothing to be done if the session is not in STA mode */
       if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
           PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
           vos_mem_free(pFTPreAuthRsp);
           return;
       }
       pFTPreAuthRsp->smeSessionId = psessionEntry->smeSessionId;

       /* The bssid of the AP we are sending Auth1 to. */
       if (psessionEntry->ftPEContext.pFTPreAuthReq)
          sirCopyMacAddr(pFTPreAuthRsp->preAuthbssId,
                psessionEntry->ftPEContext.pFTPreAuthReq->preAuthbssId);
   }

   pFTPreAuthRsp->messageType = eWNI_SME_FT_PRE_AUTH_RSP;
   pFTPreAuthRsp->length = (tANI_U16) rspLen;
   pFTPreAuthRsp->status = status;

   /* Attach the auth response now back to SME */
   pFTPreAuthRsp->ft_ies_length = 0;
   if ((auth_rsp != NULL) && (auth_rsp_length < MAX_FTIE_SIZE)) {
      /* Only 11r assoc has FT IEs */
      vos_mem_copy(pFTPreAuthRsp->ft_ies, auth_rsp, auth_rsp_length);
      pFTPreAuthRsp->ft_ies_length = auth_rsp_length;
   }

   if (status != eSIR_SUCCESS) {
       /* Ensure that on Pre-Auth failure the cached Pre-Auth Req and
        * other allocated memory is freed up before returning.
        */
       limLog(pMac, LOG1, "Pre-Auth Failed, Cleanup!");
       limFTCleanup(pMac, psessionEntry);
   }

   mmhMsg.type = pFTPreAuthRsp->messageType;
   mmhMsg.bodyptr = pFTPreAuthRsp;
   mmhMsg.bodyval = 0;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
   PELOGE(limLog( pMac, LOG1,
          "Posted Auth Rsp to SME with status of 0x%x", status);)
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
      if (status == eSIR_SUCCESS)
         limDiagEventReport(pMac, WLAN_PE_DIAG_PREAUTH_DONE, psessionEntry,
               status, 0);
#endif
   limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
}

/*------------------------------------------------------------------
 *
 * Send the FT Pre Auth Response to SME whenever we have a status
 * ready to be sent to SME
 *
 * SME will be the one to send it up to the supplicant to receive
 * FTIEs which will be required for Reassoc Req.
 *
 *------------------------------------------------------------------*/
void limHandleFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
    tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
    tpPESession psessionEntry)
{
    tpPESession      pftSessionEntry = NULL;
    tANI_U8 sessionId = 0;
    tpSirBssDescription  pbssDescription = NULL;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
   limDiagEventReport(pMac, WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT,
         psessionEntry, (tANI_U16)status, 0);
#endif

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
      return;
   }

   /* Save the status of pre-auth */
   psessionEntry->ftPEContext.ftPreAuthStatus = status;

   /* Save the auth rsp, so we can send it to
    * SME once we resume link
    */
   psessionEntry->ftPEContext.saved_auth_rsp_length = 0;
   if ((auth_rsp != NULL) && (auth_rsp_length < MAX_FTIE_SIZE)) {
      vos_mem_copy(psessionEntry->ftPEContext.saved_auth_rsp,
            auth_rsp, auth_rsp_length);
      psessionEntry->ftPEContext.saved_auth_rsp_length =
         auth_rsp_length;
   }

   if (!psessionEntry->ftPEContext.pFTPreAuthReq ||
       !psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription) {
       limLog(pMac, LOGE,
               FL("pFTPreAuthReq or pbssDescription is NULL"));
       return;
   }

   /* Create FT session for the re-association at this point */
   if (psessionEntry->ftPEContext.ftPreAuthStatus == eSIR_SUCCESS) {
      pbssDescription =
         psessionEntry->ftPEContext.pFTPreAuthReq->pbssDescription;
        limPrintMacAddr(pMac, pbssDescription->bssId, LOG1);
      if((pftSessionEntry =
               peCreateSession(pMac, pbssDescription->bssId,
                  &sessionId, pMac->lim.maxStation,
                  psessionEntry->bssType)) == NULL) {
         limLog(pMac, LOGE,
               FL("Session Can not be created for pre-auth 11R AP"));
         status = eSIR_FAILURE;
         psessionEntry->ftPEContext.ftPreAuthStatus = status;
         goto send_rsp;
      }

      pftSessionEntry->peSessionId = sessionId;
      pftSessionEntry->smeSessionId = psessionEntry->smeSessionId;
      sirCopyMacAddr(pftSessionEntry->selfMacAddr, psessionEntry->selfMacAddr);
      sirCopyMacAddr(pftSessionEntry->limReAssocbssId, pbssDescription->bssId);
      pftSessionEntry->bssType = psessionEntry->bssType;

      if (pftSessionEntry->bssType == eSIR_INFRASTRUCTURE_MODE) {
         pftSessionEntry->limSystemRole = eLIM_STA_ROLE;
      }
      else if(pftSessionEntry->bssType == eSIR_BTAMP_AP_MODE) {
         pftSessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
      }
      else {
         limLog(pMac, LOGE, FL("Invalid bss type"));
      }
      pftSessionEntry->limPrevSmeState = pftSessionEntry->limSmeState;
      vos_mem_copy(&(pftSessionEntry->htConfig), &(psessionEntry->htConfig),
            sizeof(psessionEntry->htConfig));
      pftSessionEntry->limSmeState = eLIM_SME_WT_REASSOC_STATE;

      PELOGE(limLog(pMac, LOG1, "%s:created session (%p) with id = %d",
               __func__, pftSessionEntry, pftSessionEntry->peSessionId);)

      /* Update the ReAssoc BSSID of the current session */
      sirCopyMacAddr(psessionEntry->limReAssocbssId, pbssDescription->bssId);
      limPrintMacAddr(pMac, psessionEntry->limReAssocbssId, LOG1);
   }

send_rsp:
   if (psessionEntry->currentOperChannel !=
         psessionEntry->ftPEContext.pFTPreAuthReq->preAuthchannelNum) {
      /* Need to move to the original AP channel */
      limChangeChannelWithCallback(pMac, psessionEntry->currentOperChannel,
            limPerformPostFTPreAuthAndChannelChange,
            NULL, psessionEntry);
   }
   else {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog( pMac, LOG1,
             "Pre auth on same channel as connected AP channel %d",
             psessionEntry->ftPEContext.pFTPreAuthReq->preAuthchannelNum);)
#endif
         limFTProcessPreAuthResult(pMac, status, (tANI_U32 *)psessionEntry);
   }
}

/**
 * lim_ft_reassoc_set_link_state_callback()- registered callback to perform post
 *		peer creation operations
 *
 * @mac: pointer to global mac structure
 * @callback_arg: registered callback argument
 * @status: peer creation status
 *
 * this is registered callback function during ft reassoc scenario to perform
 * post peer creation operation based on the peer creation status
 *
 * Return: none
 */
void lim_ft_reassoc_set_link_state_callback(tpAniSirGlobal mac,
		 void  *callback_arg, bool status)
{
	tpPESession session_entry;
	tSirMsgQ msg_q;
	tLimMlmReassocReq *mlm_reassoc_req = (tLimMlmReassocReq *) callback_arg;
	tSirRetStatus ret_code = eSIR_SME_RESOURCES_UNAVAILABLE;
	tLimMlmReassocCnf mlm_reassoc_cnf = {0};
	session_entry = peFindSessionBySessionId(mac,
				mlm_reassoc_req->sessionId);
	if (!status || !session_entry) {
		limLog(mac, LOGE, FL("Failed: session:%p for session id:%d status:%d"),
			session_entry, mlm_reassoc_req->sessionId, status);
		goto failure;
	}

	/*
	 * we need to defer the message until we get the
	 * response back from HAL
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac, false);

	msg_q.type = SIR_HAL_ADD_BSS_REQ;
	msg_q.reserved = 0;
	msg_q.bodyptr = session_entry->ftPEContext.pAddBssReq;
	msg_q.bodyval = 0;

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
	limLog(mac, LOG1, FL("Sending SIR_HAL_ADD_BSS_REQ..."));
#endif
	MTRACE(macTraceMsgTx(mac, session_entry->peSessionId, msg_q.type));
	ret_code = wdaPostCtrlMsg(mac, &msg_q);
	if(eSIR_SUCCESS != ret_code) {
		vos_mem_free(session_entry->ftPEContext.pAddBssReq);
		limLog(mac, LOGE, FL("Post ADD_BSS_REQ failed reason=%X"),
			ret_code);
		session_entry->ftPEContext.pAddBssReq = NULL;
		goto failure;
	}

	session_entry->pLimMlmReassocReq = mlm_reassoc_req;
	session_entry->ftPEContext.pAddBssReq = NULL;
	return;

failure:
	vos_mem_free(mlm_reassoc_req);
	mlm_reassoc_cnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
	mlm_reassoc_cnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
	/* Update PE session Id*/
        if (session_entry)
	    mlm_reassoc_cnf.sessionId = session_entry->peSessionId;
	limPostSmeMessage(mac, LIM_MLM_REASSOC_CNF,
				(tANI_U32 *) &mlm_reassoc_cnf);
}

/*------------------------------------------------------------------
 *
 *  This function handles the 11R Reassoc Req from SME
 *
 *------------------------------------------------------------------*/
void limProcessMlmFTReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf,
    tpPESession psessionEntry)
{
    tANI_U8 smeSessionId = 0;
    tANI_U16 transactionId = 0;
    tANI_U8 chanNum = 0;
    tLimMlmReassocReq  *pMlmReassocReq;
    tANI_U16 caps;
    tANI_U32 val;
    tANI_U32 teleBcnEn = 0;
    tLimMlmReassocCnf mlm_reassoc_cnf = {0};

    chanNum = psessionEntry->currentOperChannel;
    limGetSessionInfo(pMac,(tANI_U8*)pMsgBuf, &smeSessionId, &transactionId);
    psessionEntry->smeSessionId = smeSessionId;
    psessionEntry->transactionId = transactionId;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_REASSOCIATING, psessionEntry, 0, 0);
#endif

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
      return;
   }

    if (NULL == psessionEntry->ftPEContext.pAddBssReq) {
        limLog(pMac, LOGE, FL("pAddBssReq is NULL"));
        goto end;
    }
    pMlmReassocReq = vos_mem_malloc(sizeof(tLimMlmReassocReq));
    if (NULL == pMlmReassocReq) {
        limLog(pMac, LOGE,
               FL("call to AllocateMemory failed for mlmReassocReq"));
        goto end;
    }

    vos_mem_copy(pMlmReassocReq->peerMacAddr,
                 psessionEntry->bssId,
                 sizeof(tSirMacAddr));

    if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                  (tANI_U32 *) &pMlmReassocReq->reassocFailureTimeout)
                           != eSIR_SUCCESS) {
        /**
         * Could not get ReassocFailureTimeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGE,
               FL("could not retrieve ReassocFailureTimeout value"));
        vos_mem_free(pMlmReassocReq);
        goto end;
    }

    if (cfgGetCapabilityInfo(pMac, &caps,psessionEntry) != eSIR_SUCCESS) {
        /**
         * Could not get Capabilities value
         * from CFG. Log error.
         */
        limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
        vos_mem_free(pMlmReassocReq);
        goto end;
    }
    pMlmReassocReq->capabilityInfo = caps;

    /* Update PE sessionId*/
    pMlmReassocReq->sessionId = psessionEntry->peSessionId;

    /* If telescopic beaconing is enabled, set listen interval
       to WNI_CFG_TELE_BCN_MAX_LI
     */
    if (wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_WAKEUP_EN, &teleBcnEn) !=
       eSIR_SUCCESS) {
       limLog(pMac, LOGP, FL("Couldn't get WNI_CFG_TELE_BCN_WAKEUP_EN"));
       vos_mem_free(pMlmReassocReq);
       goto end;
    }

    if (teleBcnEn) {
       if (wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_MAX_LI, &val) != eSIR_SUCCESS)
       {
          /**
            * Could not get ListenInterval value
            * from CFG. Log error.
          */
          limLog(pMac, LOGE, FL("could not retrieve ListenInterval"));
          vos_mem_free(pMlmReassocReq);
          goto end;
       }
    }
    else {
      if (wlan_cfgGetInt(pMac, WNI_CFG_LISTEN_INTERVAL, &val) != eSIR_SUCCESS) {
          /**
            * Could not get ListenInterval value
            * from CFG. Log error.
            */
         limLog(pMac, LOGE, FL("could not retrieve ListenInterval"));
         vos_mem_free(pMlmReassocReq);
         goto end;
      }
    }

    pMlmReassocReq->listenInterval = (tANI_U16) val;
    if (limSetLinkState(pMac, eSIR_LINK_PREASSOC_STATE, psessionEntry->bssId,
                        psessionEntry->selfMacAddr,
                        lim_ft_reassoc_set_link_state_callback,
                        pMlmReassocReq) != eSIR_SUCCESS) {
        vos_mem_free(pMlmReassocReq);
        goto end;
    }
    return;

end:
    mlm_reassoc_cnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
    mlm_reassoc_cnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
    /* Update PE session Id*/
    mlm_reassoc_cnf.sessionId = psessionEntry->peSessionId;
    limPostSmeMessage(pMac, LIM_MLM_REASSOC_CNF,
                       (tANI_U32 *) &mlm_reassoc_cnf);
}

/*------------------------------------------------------------------
 *
 * This function is called if preauth response is not received from the AP
 * within this timeout while FT in progress
 *
 *------------------------------------------------------------------*/
void limProcessFTPreauthRspTimeout(tpAniSirGlobal pMac)
{
   tpPESession psessionEntry;

   /* We have failed pre auth. We need to resume link and get back on
    * home channel
    */
   limLog(pMac, LOGE, FL("FT Pre-Auth Time Out!!!!"));

   if ((psessionEntry =
            peFindSessionBySessionId(pMac,
               pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId)) == NULL) {
      limLog(pMac, LOGE, FL("Session Does not exist for given sessionID"));
      return;
   }

   /* Nothing to be done if the session is not in STA mode */
   if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
      PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
         return;
   }

   /* Reset the flag to indicate preauth request session */
   psessionEntry->ftPEContext.ftPreAuthSession = VOS_FALSE;

   if (NULL ==
         psessionEntry->ftPEContext.pFTPreAuthReq) {
      limLog(pMac,LOGE,FL("pFTPreAuthReq is NULL"));
      return;
   }

   if (psessionEntry->ftPEContext.pFTPreAuthReq == NULL) {
       limLog(pMac, LOGE, FL("Auth Rsp might already be posted to SME and "
              "ftcleanup done! sessionId:%d"),
              pMac->lim.limTimers.gLimFTPreAuthRspTimer.sessionId);
       return;
   }

   /* To handle the race condition where we recieve preauth rsp after
    * timer has expired.
    */
   if (eANI_BOOLEAN_TRUE ==
         psessionEntry->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed) {
      limLog(pMac,LOGE,FL("Auth rsp already posted to SME"
               " (session %p)"), psessionEntry);
      return;
   }
   else {
      /* Here we are sending preauth rsp with failure state
       * and which is forwarded to SME. Now, if we receive an preauth
       * resp from AP with success it would create a FT pesession, but
       * will be dropped in SME leaving behind the pesession.
       * Mark Preauth rsp processed so that any rsp from AP is dropped in
       * limProcessAuthFrameNoSession.
       */
      limLog(pMac,LOG1,FL("Auth rsp not yet posted to SME"
               " (session %p)"), psessionEntry);
      psessionEntry->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed =
         eANI_BOOLEAN_TRUE;
   }

   /* Attempted at Pre-Auth and failed. If we are off channel. We need
    * to get back to home channel
    */
   limHandleFTPreAuthRsp(pMac, eSIR_FAILURE, NULL, 0, psessionEntry);
}


/*------------------------------------------------------------------
 *
 * This function is called to process the update key request from SME
 *
 *------------------------------------------------------------------*/
tANI_BOOLEAN limProcessFTUpdateKey(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf )
{
    tAddBssParams *pAddBssParams;
    tSirFTUpdateKeyInfo *pKeyInfo;
    tANI_U32 val = 0;
    tpPESession psessionEntry;
    tANI_U8 sessionId;

    /* Sanity Check */
    if( pMac == NULL || pMsgBuf == NULL )
    {
        return VOS_FALSE;
    }

    pKeyInfo = (tSirFTUpdateKeyInfo *)pMsgBuf;

    psessionEntry =
         peFindSessionByBssid(pMac, pKeyInfo->bssId, &sessionId);
    if (NULL == psessionEntry)
    {
        PELOGE(limLog( pMac, LOGE,
               "%s: Unable to find session for the following bssid",
               __func__);)
        limPrintMacAddr( pMac, pKeyInfo->bssId, LOGE );
        return VOS_FALSE;
    }

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       return VOS_FALSE;
    }

    if (NULL == psessionEntry->ftPEContext.pAddBssReq)
    {
        // AddBss Req is NULL, save the keys to configure them later.
        tpLimMlmSetKeysReq pMlmSetKeysReq =
            &psessionEntry->ftPEContext.PreAuthKeyInfo.extSetStaKeyParam;

        vos_mem_zero(pMlmSetKeysReq, sizeof(tLimMlmSetKeysReq));
        vos_mem_copy(pMlmSetKeysReq->peerMacAddr, pKeyInfo->bssId,
                     sizeof(tSirMacAddr));
        pMlmSetKeysReq->sessionId = psessionEntry->peSessionId;
        pMlmSetKeysReq->smesessionId = psessionEntry->smeSessionId;
        pMlmSetKeysReq->edType = pKeyInfo->keyMaterial.edType;
        pMlmSetKeysReq->numKeys = pKeyInfo->keyMaterial.numKeys;
        vos_mem_copy((tANI_U8 *) &pMlmSetKeysReq->key,
            (tANI_U8 *) &pKeyInfo->keyMaterial.key, sizeof(tSirKeys));

        psessionEntry->ftPEContext.PreAuthKeyInfo.extSetStaKeyParamValid = TRUE;

        limLog( pMac, LOGE, FL( "pAddBssReq is NULL" ));

        if (psessionEntry->ftPEContext.pAddStaReq == NULL)
        {
            limLog( pMac, LOGE, FL( "pAddStaReq is NULL" ));
            limSendSetStaKeyReq(pMac, pMlmSetKeysReq, 0, 0, psessionEntry,
                                FALSE);
            psessionEntry->ftPEContext.PreAuthKeyInfo.extSetStaKeyParamValid =
                                                                          FALSE;
        }
    }
    else
    {
        pAddBssParams = psessionEntry->ftPEContext.pAddBssReq;

        /* Store the key information in the ADD BSS parameters */
        pAddBssParams->extSetStaKeyParamValid = 1;
        pAddBssParams->extSetStaKeyParam.encType = pKeyInfo->keyMaterial.edType;
        vos_mem_copy((tANI_U8 *) &pAddBssParams->extSetStaKeyParam.key,
                     (tANI_U8 *) &pKeyInfo->keyMaterial.key, sizeof(tSirKeys));
        if(eSIR_SUCCESS != wlan_cfgGetInt(pMac, WNI_CFG_SINGLE_TID_RC, &val))
        {
            limLog( pMac, LOGP, FL( "Unable to read WNI_CFG_SINGLE_TID_RC" ));
        }

        pAddBssParams->extSetStaKeyParam.singleTidRc = val;
        PELOG1(limLog(pMac, LOG1, FL("Key valid %d"),
                    pAddBssParams->extSetStaKeyParamValid,
                    pAddBssParams->extSetStaKeyParam.key[0].keyLength);)

        pAddBssParams->extSetStaKeyParam.staIdx = 0;

        PELOG1(limLog(pMac, LOG1,
               FL("BSSID = "MAC_ADDRESS_STR), MAC_ADDR_ARRAY(pKeyInfo->bssId));)

        sirCopyMacAddr(pAddBssParams->extSetStaKeyParam.peerMacAddr,
                       pKeyInfo->bssId);

        pAddBssParams->extSetStaKeyParam.sendRsp = FALSE;

        if(pAddBssParams->extSetStaKeyParam.key[0].keyLength == 16)
        {
            PELOG1(limLog(pMac, LOG1,
            FL("BSS key = %02X-%02X-%02X-%02X-%02X-%02X-%02X- "
            "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X"),
            pAddBssParams->extSetStaKeyParam.key[0].key[0],
            pAddBssParams->extSetStaKeyParam.key[0].key[1],
            pAddBssParams->extSetStaKeyParam.key[0].key[2],
            pAddBssParams->extSetStaKeyParam.key[0].key[3],
            pAddBssParams->extSetStaKeyParam.key[0].key[4],
            pAddBssParams->extSetStaKeyParam.key[0].key[5],
            pAddBssParams->extSetStaKeyParam.key[0].key[6],
            pAddBssParams->extSetStaKeyParam.key[0].key[7],
            pAddBssParams->extSetStaKeyParam.key[0].key[8],
            pAddBssParams->extSetStaKeyParam.key[0].key[9],
            pAddBssParams->extSetStaKeyParam.key[0].key[10],
            pAddBssParams->extSetStaKeyParam.key[0].key[11],
            pAddBssParams->extSetStaKeyParam.key[0].key[12],
            pAddBssParams->extSetStaKeyParam.key[0].key[13],
            pAddBssParams->extSetStaKeyParam.key[0].key[14],
            pAddBssParams->extSetStaKeyParam.key[0].key[15]);)
        }
    }
    return TRUE;
}

void
limFTSendAggrQosRsp(tpAniSirGlobal pMac, tANI_U8 rspReqd,
                    tpAggrAddTsParams aggrQosRsp, tANI_U8 smesessionId)
{
    tpSirAggrQosRsp  rsp;
    int i = 0;

    if (! rspReqd)
    {
        return;
    }

    rsp = vos_mem_malloc(sizeof(tSirAggrQosRsp));
    if (NULL == rsp)
    {
        limLog(pMac, LOGP, FL("AllocateMemory failed for tSirAggrQosRsp"));
        return;
    }

    vos_mem_set((tANI_U8 *) rsp, sizeof(*rsp), 0);
    rsp->messageType = eWNI_SME_FT_AGGR_QOS_RSP;
    rsp->sessionId = smesessionId;
    rsp->length = sizeof(*rsp);
    rsp->aggrInfo.tspecIdx = aggrQosRsp->tspecIdx;

    for( i = 0; i < SIR_QOS_NUM_AC_MAX; i++ )
    {
        if( (1 << i) & aggrQosRsp->tspecIdx )
        {
            rsp->aggrInfo.aggrRsp[i].status = aggrQosRsp->status[i];
            rsp->aggrInfo.aggrRsp[i].tspec = aggrQosRsp->tspec[i];
        }
    }

    limSendSmeAggrQosRsp(pMac, rsp, smesessionId);
    return;
}

void limProcessFTAggrQoSRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpAggrAddTsParams pAggrQosRspMsg = NULL;
    tAddTsParams     addTsParam = {0};
    tpDphHashNode  pSta = NULL;
    tANI_U16  assocId =0;
    tSirMacAddr  peerMacAddr;
    tANI_U8   rspReqd = 1;
    tpPESession  psessionEntry = NULL;
    int i = 0;

    PELOG1(limLog(pMac, LOG1, FL(" Received AGGR_QOS_RSP from HAL"));)

    /* Need to process all the deferred messages enqueued since sending the
       SIR_HAL_AGGR_ADD_TS_REQ */
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

    pAggrQosRspMsg = (tpAggrAddTsParams) (limMsg->bodyptr);
    if (NULL == pAggrQosRspMsg)
    {
        PELOGE(limLog(pMac, LOGE, FL("NULL pAggrQosRspMsg"));)
        return;
    }

    psessionEntry = peFindSessionBySessionId(pMac, pAggrQosRspMsg->sessionId);
    if (NULL == psessionEntry)
    {
        PELOGE(limLog(pMac, LOGE,
               FL("Cant find session entry for %s"), __func__);)
        if( pAggrQosRspMsg != NULL )
        {
            vos_mem_free(pAggrQosRspMsg);
        }
        return;
    }

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       return;
    }

    for( i = 0; i < HAL_QOS_NUM_AC_MAX; i++ )
    {
        if((((1 << i) & pAggrQosRspMsg->tspecIdx)) &&
                (pAggrQosRspMsg->status[i] != eHAL_STATUS_SUCCESS))
        {
            /* send DELTS to the station */
            sirCopyMacAddr(peerMacAddr,psessionEntry->bssId);

            addTsParam.staIdx = pAggrQosRspMsg->staIdx;
            addTsParam.sessionId = pAggrQosRspMsg->sessionId;
            addTsParam.tspec = pAggrQosRspMsg->tspec[i];
            addTsParam.tspecIdx = pAggrQosRspMsg->tspecIdx;

            limSendDeltsReqActionFrame(pMac, peerMacAddr, rspReqd,
                    &addTsParam.tspec.tsinfo,
                    &addTsParam.tspec, psessionEntry);

            pSta = dphLookupAssocId(pMac, addTsParam.staIdx, &assocId,
                    &psessionEntry->dph.dphHashTable);
            if (pSta != NULL)
            {
                limAdmitControlDeleteTS(pMac, assocId, &addTsParam.tspec.tsinfo,
                        NULL, (tANI_U8 *)&addTsParam.tspecIdx);
            }
        }
    }

    /* Send the Aggr QoS response to SME */
    limFTSendAggrQosRsp(pMac, rspReqd, pAggrQosRspMsg,
            psessionEntry->smeSessionId);
    if( pAggrQosRspMsg != NULL )
    {
        vos_mem_free(pAggrQosRspMsg);
    }
    return;
}

tSirRetStatus
limProcessFTAggrQosReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf )
{
    tSirMsgQ msg;
    tSirAggrQosReq * aggrQosReq = (tSirAggrQosReq *)pMsgBuf;
    tpAggrAddTsParams pAggrAddTsParam;
    tpPESession  psessionEntry = NULL;
    tpLimTspecInfo   tspecInfo;
    tANI_U8          ac;
    tpDphHashNode    pSta;
    tANI_U16         aid;
    tANI_U8 sessionId;
    int i;

    pAggrAddTsParam = vos_mem_malloc(sizeof(tAggrAddTsParams));
    if (NULL == pAggrAddTsParam)
    {
        PELOGE(limLog(pMac, LOGE, FL("AllocateMemory() failed"));)
        return eSIR_MEM_ALLOC_FAILED;
    }

    psessionEntry = peFindSessionByBssid(pMac, aggrQosReq->bssId, &sessionId);

    if (psessionEntry == NULL) {
        PELOGE(limLog(pMac, LOGE, FL("psession Entry Null for sessionId = %d"),
                      aggrQosReq->sessionId);)
        vos_mem_free(pAggrAddTsParam);
        return eSIR_FAILURE;
    }

    /* Nothing to be done if the session is not in STA mode */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
       PELOGE(limLog(pMac, LOGE, FL("psessionEntry is not in STA mode"));)
#endif
       vos_mem_free(pAggrAddTsParam);
       return eSIR_FAILURE;
    }

    pSta = dphLookupHashEntry(pMac, aggrQosReq->bssId, &aid,
                              &psessionEntry->dph.dphHashTable);
    if (pSta == NULL)
    {
        PELOGE(limLog(pMac, LOGE,
               FL("Station context not found - ignoring AddTsRsp"));)
        vos_mem_free(pAggrAddTsParam);
        return eSIR_FAILURE;
    }

    vos_mem_set((tANI_U8 *)pAggrAddTsParam,
                 sizeof(tAggrAddTsParams), 0);
    pAggrAddTsParam->staIdx = psessionEntry->staId;
    // Fill in the sessionId specific to PE
    pAggrAddTsParam->sessionId = sessionId;
    pAggrAddTsParam->tspecIdx = aggrQosReq->aggrInfo.tspecIdx;

    for( i = 0; i < HAL_QOS_NUM_AC_MAX; i++ )
    {
       if (aggrQosReq->aggrInfo.tspecIdx & (1<<i))
       {
          tSirMacTspecIE *pTspec =
             &aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
          /* Since AddTS response was successful, check for the PSB flag
           * and directional flag inside the TS Info field.
           * An AC is trigger enabled AC if the PSB subfield is set to 1
           * in the uplink direction.
           * An AC is delivery enabled AC if the PSB subfield is set to 1
           * in the downlink direction.
           * An AC is trigger and delivery enabled AC if the PSB subfield
           * is set to 1 in the bi-direction field.
           */
          if(!pMac->psOffloadEnabled)
          {
             if (pTspec->tsinfo.traffic.psb == 1)
             {
                limSetTspecUapsdMask(pMac, &pTspec->tsinfo, SET_UAPSD_MASK);
             }
             else
             {
                limSetTspecUapsdMask(pMac, &pTspec->tsinfo,
                      CLEAR_UAPSD_MASK);
             }
             /*
              * ADDTS success, so AC is now admitted.
              * We shall now use the default
              * EDCA parameters as advertised by AP and
              * send the updated EDCA params
              * to HAL.
              */
             ac = upToAc(pTspec->tsinfo.traffic.userPrio);
             if(pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK)
             {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
                   (1 << ac);
             }
             else if(pTspec->tsinfo.traffic.direction ==
                   SIR_MAC_DIRECTION_DNLINK)
             {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
                   (1 << ac);
             }
             else if(pTspec->tsinfo.traffic.direction ==
                   SIR_MAC_DIRECTION_BIDIR)
             {
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
                   (1 << ac);
                pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
                   (1 << ac);
             }
          }
          else
          {
             if (pTspec->tsinfo.traffic.psb == 1)
             {
                limSetTspecUapsdMaskPerSession(pMac, psessionEntry,
                      &pTspec->tsinfo,
                      SET_UAPSD_MASK);
             }
             else
             {
                limSetTspecUapsdMaskPerSession(pMac, psessionEntry,
                      &pTspec->tsinfo,
                      CLEAR_UAPSD_MASK);
             }
             /*
              * ADDTS success, so AC is now admitted.
              * We shall now use the default
              * EDCA parameters as advertised by AP and
              * send the updated EDCA params
              * to HAL.
              */
             ac = upToAc(pTspec->tsinfo.traffic.userPrio);
             if(pTspec->tsinfo.traffic.direction == SIR_MAC_DIRECTION_UPLINK)
             {
                psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
                                                      (1 << ac);
             }
             else if(pTspec->tsinfo.traffic.direction ==
                                                      SIR_MAC_DIRECTION_DNLINK)
             {
                psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
                                                      (1 << ac);
             }
             else if(pTspec->tsinfo.traffic.direction ==
                                                      SIR_MAC_DIRECTION_BIDIR)
             {
                psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
                                                      (1 << ac);
                psessionEntry->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
                                                      (1 << ac);
             }
          }

          limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams,
                psessionEntry);

          limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive,
                            pSta->bssId);

          if(eSIR_SUCCESS != limTspecAdd(pMac, pSta->staAddr, pSta->assocId,
                   pTspec,  0, &tspecInfo))
          {
             PELOGE(limLog(pMac, LOGE,
                      FL("Adding entry in lim Tspec Table failed "));)
                pMac->lim.gLimAddtsSent = false;
             vos_mem_free(pAggrAddTsParam);
             return eSIR_FAILURE;
          }

          pAggrAddTsParam->tspec[i] =
                    aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
       }
    }

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (!pMac->roam.configParam.isRoamOffloadEnabled ||
       (pMac->roam.configParam.isRoamOffloadEnabled &&
        !psessionEntry->is11Rconnection))
#endif
    {
        msg.type = WDA_AGGR_QOS_REQ;
        msg.bodyptr = pAggrAddTsParam;
        msg.bodyval = 0;

        /* We need to defer any incoming messages until we get a
         * WDA_AGGR_QOS_RSP from HAL.
         */
        SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
        MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msg.type));

        if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
        {
            PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg() failed"));)
            SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
            vos_mem_free(pAggrAddTsParam);
            return eSIR_FAILURE;
        }
    }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    else
    {
        /* Implies it is a LFR3.0 based 11r connection
         * so donot send add ts request to fimware since it
         * already has the RIC IEs */

        /* Send the Aggr QoS response to SME */
        limFTSendAggrQosRsp(pMac, true, pAggrAddTsParam,
                          psessionEntry->smeSessionId);
        if( pAggrAddTsParam != NULL )
        {
            vos_mem_free(pAggrAddTsParam);
        }
    }
#endif

    return eSIR_SUCCESS;
}

#endif /* WLAN_FEATURE_VOWIFI_11R */
