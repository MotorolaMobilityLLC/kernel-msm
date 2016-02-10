/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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
 *
 * This file limProcessAssocRspFrame.cc contains the code
 * for processing Re/Association Response Frame.
 * Author:        Chandra Modumudi
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniApi.h"
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "cfgApi.h"

#include "utilsApi.h"
#include "pmmApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limStaHashApi.h"
#include "limSendMessages.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#endif

extern tSirRetStatus schBeaconEdcaProcess(tpAniSirGlobal pMac, tSirMacEdcaParamSetIE *edca, tpPESession psessionEntry);


/**
 * @function : limUpdateAssocStaDatas
 *
 * @brief :  This function is called to Update the Station Descriptor (dph) Details from
 *                  Association / ReAssociation Response Frame
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pStaDs   - Station Descriptor in DPH
 * @param  pAssocRsp    - Pointer to Association Response Structure
 *
 * @return None
 */
void limUpdateAssocStaDatas(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpSirAssocRsp pAssocRsp,tpPESession psessionEntry)
{
    tANI_U32        phyMode;
    tANI_BOOLEAN    qosMode;
    tANI_U16        rxHighestRate = 0;
    uint32_t        shortgi_20mhz_support;
    uint32_t        shortgi_40mhz_support;

    limGetPhyMode(pMac, &phyMode, psessionEntry);

    pStaDs->staType= STA_ENTRY_SELF;

    limGetQosMode(psessionEntry, &qosMode);

       pStaDs->mlmStaContext.authType = psessionEntry->limCurrentAuthType;

       // Add capabilities information, rates and AID
       pStaDs->mlmStaContext.capabilityInfo = pAssocRsp->capabilityInfo;
       pStaDs->shortPreambleEnabled= (tANI_U8)pAssocRsp->capabilityInfo.shortPreamble;

       //Update HT Capabilites only when the self mode supports HT
       if(IS_DOT11_MODE_HT(psessionEntry->dot11mode)) {
           pStaDs->mlmStaContext.htCapability = pAssocRsp->HTCaps.present;

           if ( pAssocRsp->HTCaps.present ) {
               pStaDs->htGreenfield = ( tANI_U8 ) pAssocRsp->HTCaps.greenField;
               if (psessionEntry->htSupportedChannelWidthSet) {
                   pStaDs->htSupportedChannelWidthSet =
                           (tANI_U8)(pAssocRsp->HTCaps.supportedChannelWidthSet ?
                           pAssocRsp->HTInfo.recommendedTxWidthSet :
                           pAssocRsp->HTCaps.supportedChannelWidthSet);
               }
               else
                   pStaDs->htSupportedChannelWidthSet = eHT_CHANNEL_WIDTH_20MHZ;

                   pStaDs->htLsigTXOPProtection = ( tANI_U8 ) pAssocRsp->HTCaps.lsigTXOPProtection;
                   pStaDs->htMIMOPSState =  (tSirMacHTMIMOPowerSaveState)pAssocRsp->HTCaps.mimoPowerSave;
                   pStaDs->htMaxAmsduLength = ( tANI_U8 ) pAssocRsp->HTCaps.maximalAMSDUsize;
                   pStaDs->htAMpduDensity =             pAssocRsp->HTCaps.mpduDensity;
                   pStaDs->htDsssCckRate40MHzSupport = (tANI_U8)pAssocRsp->HTCaps.dsssCckMode40MHz;
                   pStaDs->htMaxRxAMpduFactor = pAssocRsp->HTCaps.maxRxAMPDUFactor;
                   limFillRxHighestSupportedRate(pMac, &rxHighestRate, pAssocRsp->HTCaps.supportedMCSSet);
                   pStaDs->supportedRates.rxHighestDataRate = rxHighestRate;
                   /* This is for AP as peer STA and we are INFRA STA. We will put APs offset in dph node which is peer STA */
                   pStaDs->htSecondaryChannelOffset = (tANI_U8)pAssocRsp->HTInfo.secondaryChannelOffset;

                   //FIXME_AMPDU
                   // In the future, may need to check for "assoc.HTCaps.delayedBA"
                   // For now, it is IMMEDIATE BA only on ALL TID's
                   pStaDs->baPolicyFlag = 0xFF;

                   /*
                    * Check if we have support for gShortGI20Mhz and
                    * gShortGI40Mhz from ini file.
                    */
                   if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac,
                                          WNI_CFG_SHORT_GI_20MHZ,
                                          &shortgi_20mhz_support))) {
                       if (VOS_TRUE == shortgi_20mhz_support)
                           pStaDs->htShortGI20Mhz =
                                  (tANI_U8)pAssocRsp->HTCaps.shortGI20MHz;
                       else
                           pStaDs->htShortGI20Mhz = VOS_FALSE;
                   } else {
                       limLog(pMac, LOGE,
                              FL("could not retrieve shortGI 20Mhz CFG, setting value to default"));
                       pStaDs->htShortGI20Mhz = WNI_CFG_SHORT_GI_20MHZ_STADEF;
                   }

                   if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac,
                                          WNI_CFG_SHORT_GI_40MHZ,
                                          &shortgi_40mhz_support))) {
                       if (VOS_TRUE == shortgi_40mhz_support)
                           pStaDs->htShortGI40Mhz =
                                   (tANI_U8)pAssocRsp->HTCaps.shortGI40MHz;
                       else
                           pStaDs->htShortGI40Mhz = VOS_FALSE;
                   } else {
                       limLog(pMac, LOGE,
                              FL("could not retrieve shortGI 40Mhz CFG,setting value to default"));
                       pStaDs->htShortGI40Mhz = WNI_CFG_SHORT_GI_40MHZ_STADEF;
                   }
           }
       }

#ifdef WLAN_FEATURE_11AC
       if(IS_DOT11_MODE_VHT(psessionEntry->dot11mode))
       {
           pStaDs->mlmStaContext.vhtCapability = pAssocRsp->VHTCaps.present;
           if (pAssocRsp->VHTCaps.present &&
               psessionEntry->htSupportedChannelWidthSet)
               pStaDs->vhtSupportedChannelWidthSet =
                                   pAssocRsp->VHTOperation.chanWidth;
       }

       // If 11ac is supported and if the peer is sending VHT capabilities,
       // then htMaxRxAMpduFactor should be overloaded with VHT maxAMPDULenExp
       if (pAssocRsp->VHTCaps.present)
       {
          pStaDs->htMaxRxAMpduFactor = pAssocRsp->VHTCaps.maxAMPDULenExp;
       }

       if (limPopulatePeerRateSet(pMac, &pStaDs->supportedRates,
                                pAssocRsp->HTCaps.supportedMCSSet,
                                false,psessionEntry , &pAssocRsp->VHTCaps) != eSIR_SUCCESS)
#else
       if (limPopulatePeerRateSet(pMac, &pStaDs->supportedRates, pAssocRsp->HTCaps.supportedMCSSet, false,psessionEntry) != eSIR_SUCCESS)
#endif
       {
           limLog(pMac, LOGP, FL("could not get rateset and extended rate set"));
           return;
       }

#ifdef WLAN_FEATURE_11AC
        pStaDs->vhtSupportedRxNss = ((pStaDs->supportedRates.vhtRxMCSMap & MCSMAPMASK2x2)
                                                                == MCSMAPMASK2x2) ? 1 : 2;
#endif
       //If one of the rates is 11g rates, set the ERP mode.
       if ((phyMode == WNI_CFG_PHY_MODE_11G) && sirIsArate(pStaDs->supportedRates.llaRates[0] & 0x7f))
           pStaDs->erpEnabled = eHAL_SET;


       pStaDs->qosMode    = 0;
       pStaDs->lleEnabled = 0;

       // update TSID to UP mapping
       if (qosMode) {
           if (pAssocRsp->edcaPresent) {
               tSirRetStatus status;
               status = schBeaconEdcaProcess(pMac,&pAssocRsp->edca, psessionEntry);
              PELOG2(limLog(pMac, LOG2, "Edca set update based on AssocRsp: status %d",
                      status);)
               if (status != eSIR_SUCCESS) {
                   PELOGE(limLog(pMac, LOGE, FL("Edca error in AssocResp "));)
               } else { // update default tidmap based on ACM
                   pStaDs->qosMode    = 1;
                   pStaDs->lleEnabled = 1;
               }
           }
       }

       pStaDs->wmeEnabled = 0;
       pStaDs->wsmEnabled = 0;
       if (psessionEntry->limWmeEnabled && pAssocRsp->wmeEdcaPresent)
       {
           tSirRetStatus status;
           status = schBeaconEdcaProcess(pMac,&pAssocRsp->edca, psessionEntry);
           PELOGW(limLog(pMac, LOGW, "WME Edca set update based on AssocRsp: status %d", status);)

           if (status != eSIR_SUCCESS)
               PELOGE(limLog(pMac, LOGE, FL("WME Edca error in AssocResp - ignoring"));)
           else { // update default tidmap based on HashACM
               pStaDs->qosMode    = 1;
               pStaDs->wmeEnabled = 1;
           }
       }
       else {
           /* We received assoc rsp from a legacy AP. So fill in the default
            * local EDCA params. This is needed (refer to bug #14989) as we'll
            * be passing the gLimEdcaParams to HAL in limProcessStaMlmAddBssRsp().
            */
           schSetDefaultEdcaParams(pMac, psessionEntry);
       }

       if(qosMode && (!pStaDs->qosMode) && pStaDs->mlmStaContext.htCapability)
       {
           // Enable QOS for all HT AP's even though WMM or 802.11E IE is not present
           pStaDs->qosMode    = 1;
           pStaDs->wmeEnabled = 1;
       }

#ifdef WLAN_FEATURE_11W
       if(psessionEntry->limRmfEnabled)
       {
           pStaDs->rmfEnabled = 1;
       }
#endif
}

/**
 * @function : limUpdateReAssocGlobals
 *
 * @brief :  This function is called to Update the Globals (LIM) during ReAssoc.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pAssocRsp    - Pointer to Association Response Structure
 *
 * @return None
 */

void limUpdateReAssocGlobals(tpAniSirGlobal pMac, tpSirAssocRsp pAssocRsp,tpPESession psessionEntry)
{
    /**
     * Update the status for PMM module
     */
    pmmResetPmmState(pMac);

    // Update the current Bss Information
    vos_mem_copy(psessionEntry->bssId,
                 psessionEntry->limReAssocbssId, sizeof(tSirMacAddr));
    psessionEntry->currentOperChannel = psessionEntry->limReassocChannelId;
    psessionEntry->htSecondaryChannelOffset = psessionEntry->reAssocHtSupportedChannelWidthSet;
    psessionEntry->htRecommendedTxWidthSet = psessionEntry->reAssocHtRecommendedTxWidthSet;
    psessionEntry->htSecondaryChannelOffset = psessionEntry->reAssocHtSecondaryChannelOffset;
    psessionEntry->limCurrentBssCaps   = psessionEntry->limReassocBssCaps;
    psessionEntry->limCurrentBssQosCaps = psessionEntry->limReassocBssQosCaps;
    psessionEntry->limCurrentBssPropCap = psessionEntry->limReassocBssPropCap;

    vos_mem_copy((tANI_U8 *) &psessionEntry->ssId,
                 (tANI_U8 *) &psessionEntry->limReassocSSID,
                  psessionEntry->limReassocSSID.length+1);

    // Store assigned AID for TIM processing
    psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;
    /** Set the State Back to ReAssoc Rsp*/
    psessionEntry->limMlmState = eLIM_MLM_WT_REASSOC_RSP_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));


}

/**
 * @function : limProcessAssocRspFrame
 *
 * @brief :  This function is called by limProcessMessageQueue() upon
 *              Re/Association Response frame reception.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  *pRxPacketInfo    - A pointer to Rx packet info structure
 * @param  subType - Indicates whether it is Association Response (=0) or
 *                   Reassociation Response (=1) frame
 *
 * @return None
 */

void
limProcessAssocRspFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tANI_U8 subType,tpPESession psessionEntry)
{
    tANI_U8               *pBody;
    tANI_U16              caps;
    tANI_U32              frameLen;
    tSirMacAddr           currentBssId;
    tpSirMacMgmtHdr       pHdr = NULL;
    tSirMacCapabilityInfo localCapabilities;
    tpDphHashNode         pStaDs;
    tpSirAssocRsp         pAssocRsp;
    tLimMlmAssocCnf       mlmAssocCnf;
    tSchBeaconStruct      *pBeaconStruct;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    tANI_U8               smeSessionId = 0;
#endif

    //Initialize status code to success.
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (psessionEntry->bRoamSynchInProgress)
        pHdr = (tpSirMacMgmtHdr)pMac->roam.pReassocResp;
    else
#endif
    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    smeSessionId = psessionEntry->smeSessionId;
#endif

    mlmAssocCnf.resultCode = eSIR_SME_SUCCESS;
    /* Update PE session Id*/
    mlmAssocCnf.sessionId = psessionEntry->peSessionId;
    if (pHdr == NULL) {
        limLog(pMac, LOGE,
               FL("LFR3: Reassoc response packet header is NULL"));
        return;
    } else if ( pHdr->sa == NULL) {
        limLog(pMac, LOGE,
               FL("LFR3: Reassoc response packet source address is NULL"));
        return;
    }

    limLog(pMac, LOG1,
              FL("received Re/Assoc(%d) resp on sessionid: %d with systemrole: %d "
              "and mlmstate: %d RSSI %d from "MAC_ADDRESS_STR),subType,
              psessionEntry->peSessionId,
              GET_LIM_SYSTEM_ROLE(psessionEntry), psessionEntry->limMlmState,
              (uint)abs((tANI_S8)WDA_GET_RX_RSSI_NORMALIZED(pRxPacketInfo)),
              MAC_ADDR_ARRAY(pHdr->sa));

    pBeaconStruct = vos_mem_malloc(sizeof(tSchBeaconStruct));
    if (NULL == pBeaconStruct)
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory") );
        return;
    }


    if (LIM_IS_AP_ROLE(psessionEntry) || LIM_IS_BT_AMP_AP_ROLE(psessionEntry)) {
        // Should not have received Re/Association Response
        // frame on AP. Log error
        limLog(pMac, LOGE,
               FL("Should not recieved Re/Assoc Response in role %d"),
               GET_LIM_SYSTEM_ROLE(psessionEntry));

        vos_mem_free(pBeaconStruct);
        return;
    }

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (psessionEntry->bRoamSynchInProgress)
    {
        pHdr = (tpSirMacMgmtHdr)pMac->roam.pReassocResp;
        frameLen = pMac->roam.reassocRespLen - SIR_MAC_HDR_LEN_3A;
    }
    else
    {
#endif
        pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    }
#endif
    if (((subType == LIM_ASSOC) &&
         (psessionEntry->limMlmState != eLIM_MLM_WT_ASSOC_RSP_STATE)) ||
        ((subType == LIM_REASSOC) &&
         ((psessionEntry->limMlmState != eLIM_MLM_WT_REASSOC_RSP_STATE)
#if defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
         && (psessionEntry->limMlmState != eLIM_MLM_WT_FT_REASSOC_RSP_STATE)
#endif
         )))
    {
        /// Received unexpected Re/Association Response frame

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOG1(limLog(pMac, LOG1,  FL("Recieved Re/Assoc rsp in unexpected "
            "state %d on session=%d"),
            psessionEntry->limMlmState, psessionEntry->peSessionId);)
#endif
        // Log error
        if (!pHdr->fc.retry) {
            if (!(pMac->lim.retry_packet_cnt & 0xf)) {
                limLog(pMac, LOGE,
                   FL("received Re/Assoc rsp frame is not a retry frame"));
                limPrintMlmState(pMac, LOGE, psessionEntry->limMlmState);
            } else {
                pMac->lim.retry_packet_cnt++;
            }
        }
        vos_mem_free(pBeaconStruct);
        return;
    }
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    if (subType == LIM_ASSOC)
    {
        if (!vos_mem_compare(pHdr->sa, currentBssId, sizeof(tSirMacAddr)))
        {
            /**
             * Received Association Response frame from an entity
             * other than one to which request was initiated.
             * Ignore this and wait until Association Failure Timeout.
             */

            // Log error
            PELOGW(limLog(pMac, LOGW,
                   FL("received AssocRsp frame from unexpected peer "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pHdr->sa));)
            vos_mem_free(pBeaconStruct);
            return;
        }
    }
    else
    {
        if (!vos_mem_compare(pHdr->sa, psessionEntry->limReAssocbssId, sizeof(tSirMacAddr)))
        {
            /**
             * Received Reassociation Response frame from an entity
             * other than one to which request was initiated.
             * Ignore this and wait until Reassociation Failure Timeout.
             */

            // Log error
            PELOGW(limLog(pMac, LOGW,
                   FL("received ReassocRsp frame from unexpected peer "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pHdr->sa));)
            vos_mem_free(pBeaconStruct);

            return;
        }
    }

   pAssocRsp = vos_mem_malloc(sizeof(*pAssocRsp));
   if (NULL == pAssocRsp)
   {
        limLog(pMac, LOGP, FL("Allocate Memory failed in AssocRsp"));
        vos_mem_free(pBeaconStruct);

        return;
    }

    // Get pointer to Re/Association Response frame body
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (psessionEntry->bRoamSynchInProgress)
        pBody = pMac->roam.pReassocResp + SIR_MAC_HDR_LEN_3A;
    else
#endif
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

    // parse Re/Association Response frame.
    if (sirConvertAssocRespFrame2Struct(
                        pMac, pBody, frameLen, pAssocRsp) == eSIR_FAILURE)
    {
        vos_mem_free(pAssocRsp);
        PELOGE(limLog(pMac, LOGE, FL("Parse error Assoc resp subtype %d,"
                     "length=%d"), frameLen,subType);)
        vos_mem_free(pBeaconStruct);

        return;
    }

    if(!pAssocRsp->suppRatesPresent)
    {
        PELOGE(limLog(pMac, LOGE, FL("assoc response does not have supported rate set"));)
        vos_mem_copy(&pAssocRsp->supportedRates,
                      &psessionEntry->rateSet, sizeof(tSirMacRateSet));
    }

    mlmAssocCnf.protStatusCode = pAssocRsp->statusCode;

    if( psessionEntry->assocRsp != NULL )
    {
        limLog(pMac, LOGW, FL("psessionEntry->assocRsp is not NULL freeing it "
        "and setting NULL"));
        vos_mem_free(psessionEntry->assocRsp);
        psessionEntry->assocRsp = NULL;
        psessionEntry->assocRspLen = 0;
    }

    psessionEntry->assocRsp = vos_mem_malloc(frameLen);
    if (NULL == psessionEntry->assocRsp)
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc response, len = %d"), frameLen);)
    }
    else
    {
        //Store the Assoc response. This is sent to csr/hdd in join cnf response.
        vos_mem_copy(psessionEntry->assocRsp, pBody, frameLen);
        psessionEntry->assocRspLen = frameLen;
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->ricData != NULL)
    {
        vos_mem_free(psessionEntry->ricData);
        psessionEntry->ricData = NULL;
        psessionEntry->RICDataLen = 0;
    }
    if(pAssocRsp->ricPresent) {
        psessionEntry->RICDataLen =
                pAssocRsp->num_RICData * sizeof(tDot11fIERICDataDesc);
        if (psessionEntry->RICDataLen) {
            psessionEntry->ricData = vos_mem_malloc(psessionEntry->RICDataLen);
            if (NULL == psessionEntry->ricData) {
               limLog(pMac, LOGE, FL("Unable to alloc mem for RIC data"));
               psessionEntry->RICDataLen = 0;
            } else {
               vos_mem_copy(psessionEntry->ricData,
                         &pAssocRsp->RICData[0], psessionEntry->RICDataLen);
            }
        } else {
               limLog(pMac, LOGE, FL("RIC Data not present"));
        }
    } else {
        limLog(pMac, LOG1, FL("RIC is not present"));
        psessionEntry->RICDataLen = 0;
        psessionEntry->ricData = NULL;
    }
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (pAssocRsp->FTInfo.R0KH_ID.present)
    {
        pMac->roam.roamSession[smeSessionId].ftSmeContext.r0kh_id_len =
                                    pAssocRsp->FTInfo.R0KH_ID.num_PMK_R0_ID;
        vos_mem_copy(pMac->roam.roamSession[smeSessionId].ftSmeContext.r0kh_id,
                pAssocRsp->FTInfo.R0KH_ID.PMK_R0_ID,
                pMac->roam.roamSession[smeSessionId].ftSmeContext.r0kh_id_len);
    }
    else
    {
       pMac->roam.roamSession[smeSessionId].ftSmeContext.r0kh_id_len = 0;
       vos_mem_zero(pMac->roam.roamSession[smeSessionId].ftSmeContext.r0kh_id,
                    SIR_ROAM_R0KH_ID_MAX_LEN);
    }
#endif

#ifdef FEATURE_WLAN_ESE
    if (psessionEntry->tspecIes != NULL)
    {
        vos_mem_free(psessionEntry->tspecIes);
        psessionEntry->tspecIes = NULL;
        psessionEntry->tspecLen = 0;
    }
    if(pAssocRsp->tspecPresent) {
        limLog(pMac, LOG1, FL("Tspec EID present in assoc rsp"));
        psessionEntry->tspecLen = pAssocRsp->num_tspecs * sizeof(tDot11fIEWMMTSPEC);
        if (psessionEntry->tspecLen) {
            psessionEntry->tspecIes = vos_mem_malloc(psessionEntry->tspecLen);
            if (NULL == psessionEntry->tspecIes) {
                limLog(pMac, LOGE, FL("Unable to alloc mem for TSPEC"));
                psessionEntry->tspecLen = 0;
            } else {
                 vos_mem_copy(psessionEntry->tspecIes,
                             &pAssocRsp->TSPECInfo[0], psessionEntry->tspecLen);
            }
        } else {
                 limLog(pMac, LOGE, FL("TSPEC has Zero length"));
        }
    } else {
        psessionEntry->tspecLen = 0;
        psessionEntry->tspecIes = NULL;
        limLog(pMac, LOG1, FL("Tspec EID *NOT* present in assoc rsp"));
    }
#endif

    if (pAssocRsp->capabilityInfo.ibss)
    {
        /**
         * Received Re/Association Response from peer
         * with IBSS capability set.
         * Ignore the frame and wait until Re/assoc
         * failure timeout.
         */

        // Log error
        limLog(pMac, LOGE,
               FL("received Re/AssocRsp frame with IBSS capability"));
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        return;
    }

    if (cfgGetCapabilityInfo(pMac, &caps,psessionEntry) != eSIR_SUCCESS)
    {
        /**
         * Could not get Capabilities value
         * from CFG. Log error.
         */
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        limLog(pMac, LOGP, FL("could not retrieve Capabilities value"));
        return;
    }
    limCopyU16((tANI_U8 *) &localCapabilities, caps);

    if (subType == LIM_ASSOC)        // Stop Association failure timer
        limDeactivateAndChangeTimer(pMac, eLIM_ASSOC_FAIL_TIMER);
    else        // Stop Reassociation failure timer
    {
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        pMac->lim.reAssocRetryAttempt = 0;
        if ((NULL != pMac->lim.pSessionEntry) && (NULL != pMac->lim.pSessionEntry->pLimMlmReassocRetryReq))
        {
            vos_mem_free(pMac->lim.pSessionEntry->pLimMlmReassocRetryReq);
            pMac->lim.pSessionEntry->pLimMlmReassocRetryReq = NULL;
        }
#endif
        limDeactivateAndChangeTimer(pMac, eLIM_REASSOC_FAIL_TIMER);
    }

    if (pAssocRsp->statusCode != eSIR_MAC_SUCCESS_STATUS
#ifdef WLAN_FEATURE_11W
      && (!psessionEntry->limRmfEnabled ||
          pAssocRsp->statusCode != eSIR_MAC_TRY_AGAIN_LATER)
#endif /* WLAN_FEATURE_11W */
      )
    {
        // Re/Association response was received
        // either with failure code.
        // Log error.
        PELOGE(limLog(pMac, LOGE, FL("received Re/AssocRsp frame failure code %d"), pAssocRsp->statusCode);)
        // Need to update 'association failure' error counter
        // along with STATUS CODE

        // Return Assoc confirm to SME with received failure code
        mlmAssocCnf.resultCode = eSIR_SME_ASSOC_REFUSED;

        // Delete Pre-auth context for the associated BSS
        if (limSearchPreAuthList(pMac, pHdr->sa))
            limDeletePreAuthNode(pMac, pHdr->sa);

        goto assocReject;
    }
    else if ((pAssocRsp->aid & 0x3FFF) > 2007)
    {
        // Re/Association response was received
        // with invalid AID value
        // Log error
        PELOGW(limLog(pMac, LOGE, FL("received Re/AssocRsp frame with"
                     "invalid aid %X"), pAssocRsp->aid);)
        mlmAssocCnf.resultCode = eSIR_SME_INVALID_ASSOC_RSP_RXED;
        mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

        // Send advisory Disassociation frame to AP
        limSendDisassocMgmtFrame(pMac, eSIR_MAC_UNSPEC_FAILURE_REASON,
                                 pHdr->sa, psessionEntry, FALSE);

        goto assocReject;
    }
    // Association Response received with success code
    /*
     * Set the link state to POSTASSOC now that we have received
     * assoc/reassoc response
     * NOTE: for BTAMP case, it is being handled in limProcessMlmAssocReq
     */

#ifdef WLAN_FEATURE_11W
    if (psessionEntry->limRmfEnabled &&
        pAssocRsp->statusCode == eSIR_MAC_TRY_AGAIN_LATER) {
        /* fetch timer value from IE */
        if (pAssocRsp->TimeoutInterval.present &&
            (pAssocRsp->TimeoutInterval.timeoutType ==
               SIR_MAC_TI_TYPE_ASSOC_COMEBACK) ) {
            tANI_U16 timeout_value = pAssocRsp->TimeoutInterval.timeoutValue;
            if (timeout_value < 10) {
                /*
                 * if this value is less than 10 then our timer will fail to
                 * start and due to this we will never re-attempt. Better
                 * modify the timer value here.
                 */
                timeout_value = 10;
            }
            PELOGE(limLog(pMac, LOG1,
                   FL("ASSOC response with eSIR_MAC_TRY_AGAIN_LATER recvd. "
                   "Starting timer to wait timeout=%d."),
                   timeout_value);)

            /* start timer with callback */
            if (VOS_STATUS_SUCCESS !=
                vos_timer_start(&psessionEntry->pmfComebackTimer,
                                timeout_value)) {
                PELOGE(limLog(pMac, LOGE,
                       FL("Failed to start comeback timer."));)
            }
        } else {
            PELOGE(limLog(pMac, LOG1,
                   FL("ASSOC response with eSIR_MAC_TRY_AGAIN_LATER recvd."
                      "But try again time interval IE is wrong."));)
        }
        /* callback will send Assoc again */
        /* DO NOT send ASSOC CNF to MLM state machine */
        vos_mem_free(pBeaconStruct);
        vos_mem_free(pAssocRsp);
        return;
    }
#endif /* WLAN_FEATURE_11W */

    if (!((psessionEntry->bssType == eSIR_BTAMP_STA_MODE) ||
          ((psessionEntry->bssType == eSIR_BTAMP_AP_MODE) &&
          LIM_IS_BT_AMP_STA_ROLE(psessionEntry)))) {
            if (limSetLinkState(pMac, eSIR_LINK_POSTASSOC_STATE, psessionEntry->bssId,
                                psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("Set link state to POSTASSOC failed"));)
                vos_mem_free(pBeaconStruct);
                vos_mem_free(pAssocRsp);
                return;
            }
    }

    if (subType == LIM_REASSOC)
    {
        // Log success
        PELOG1(limLog(pMac, LOG1, FL("Successfully Reassociated with BSS"));)
#ifdef FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_ROAM_ASSOC_COMP_EVENT,
                       psessionEntry, eSIR_SUCCESS, eSIR_SUCCESS);
#endif
#ifdef FEATURE_WLAN_ESE
        {
            tANI_U8 cnt = 0;
            if (pAssocRsp->tsmPresent)
            {
                limLog(pMac, LOGW, "TSM IE Present in Reassoc Rsp");
                // Start the TSM  timer only if the TSPEC Ie is present in the reassoc rsp
                if (pAssocRsp->tspecPresent) {
                    // Find the TSPEC IE with VO user priority
                    for (cnt=0; cnt<pAssocRsp->num_tspecs; cnt++) {
                        if ( upToAc(pAssocRsp->TSPECInfo[cnt].user_priority) == EDCA_AC_VO) {
                            psessionEntry->eseContext.tsm.tid = pAssocRsp->TSPECInfo[cnt].user_priority;
                            vos_mem_copy(&psessionEntry->eseContext.tsm.tsmInfo,
                                    &pAssocRsp->tsmIE, sizeof(tSirMacESETSMIE));
#ifdef FEATURE_WLAN_ESE_UPLOAD
                            limSendSmeTsmIEInd(pMac,
                                               psessionEntry,
                                               pAssocRsp->tsmIE.tsid,
                                               pAssocRsp->tsmIE.state,
                                               pAssocRsp->tsmIE.msmt_interval);
#else
                            limActivateTSMStatsTimer(pMac, psessionEntry);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
                            if(psessionEntry->eseContext.tsm.tsmInfo.state) {
                                psessionEntry->eseContext.tsm.tsmMetrics.RoamingCount++;
                            }
                            break;
                        }
                    }
                } else {
                    limLog(pMac, LOGE, "TSM present but TSPEC IE not present in Reassoc Rsp");
                }
            }
        }
#endif
        if (psessionEntry->pLimMlmJoinReq)
        {
            vos_mem_free(psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }

        psessionEntry->limAssocResponseData  = (void *) pAssocRsp; /** Store the ReAssocRsp Frame in DphTable to be used
                                                        during processing DelSta nd DelBss to send AddBss again*/
        pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

        if(!pStaDs)
        {
            PELOGE(limLog(pMac, LOGE, FL("could not get hash entry at DPH for"));)
            limPrintMacAddr(pMac, pHdr->sa, LOGE);
            mlmAssocCnf.resultCode = eSIR_SME_INVALID_ASSOC_RSP_RXED;
            mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

            // Send advisory Disassociation frame to AP
            limSendDisassocMgmtFrame(pMac, eSIR_MAC_UNSPEC_FAILURE_REASON,
                                     pHdr->sa, psessionEntry, FALSE);

            goto assocReject;
        }

#if defined(WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        if (psessionEntry->limMlmState == eLIM_MLM_WT_FT_REASSOC_RSP_STATE)
        {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
            PELOGE(limLog(pMac, LOG1, FL("Sending self sta"));)
#endif
            pmmResetPmmState(pMac);

            limUpdateAssocStaDatas(pMac, pStaDs, pAssocRsp,psessionEntry);

            // Store assigned AID for TIM processing
            psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;

            // Downgrade the EDCA parameters if needed
            limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

            // Send the active EDCA parameters to HAL
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            if (!psessionEntry->bRoamSynchInProgress)
            {
#endif
              limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive,
                                    pStaDs->bssId);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            }
#endif
            limAddFTStaSelf(pMac, (pAssocRsp->aid & 0x3FFF), psessionEntry);
            vos_mem_free(pBeaconStruct);

            return;
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */

        /* If we're re-associating to the same BSS, we don't want to invoke delete
         * STA, delete BSS, as that would remove the already established TSPEC.
         * Just go ahead and re-add the BSS, STA with new capability information.
         * However, if we're re-associating to a different BSS, then follow thru
         * with del STA, del BSS, add BSS, add STA.
         */
        if (sirCompareMacAddr( psessionEntry->bssId, psessionEntry->limReAssocbssId))
            limHandleAddBssInReAssocContext(pMac, pStaDs, psessionEntry);
        else
        {
            if(!pMac->psOffloadEnabled)
            {
                /*
                 * reset the uapsd mask settings
                 *  since we're re-associating to new AP
                 */
                pMac->lim.gUapsdPerAcDeliveryEnableMask = 0;
                pMac->lim.gUapsdPerAcTriggerEnableMask = 0;
            }
            else
            {
                /*
                 * reset the uapsd mask settings since
                 * we're re-associating to new AP
                 */
                psessionEntry->gUapsdPerAcDeliveryEnableMask = 0;
                psessionEntry->gUapsdPerAcTriggerEnableMask = 0;
            }

            if (limCleanupRxPath(pMac, pStaDs,psessionEntry) != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("Could not cleanup the rx path"));)
                goto assocReject;
            }
        }
        vos_mem_free(pBeaconStruct);

        return;
    }

    // Log success
    PELOG1(limLog(pMac, LOG1, FL("Successfully Associated with BSS "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(pHdr->sa));)
#ifdef FEATURE_WLAN_ESE
    if(psessionEntry->eseContext.tsm.tsmInfo.state)
    {
        psessionEntry->eseContext.tsm.tsmMetrics.RoamingCount = 0;
    }
#endif
    /**
     * Update the status for PMM module
     */
    pmmResetPmmState(pMac);

    // Store assigned AID for TIM processing
    psessionEntry->limAID = pAssocRsp->aid & 0x3FFF;


    //STA entry was created during pre-assoc state.
    if ((pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable)) == NULL)
    {
        // Could not add hash table entry
        PELOGE(limLog(pMac, LOGE, FL("could not get hash entry at DPH for "));)
        limPrintMacAddr(pMac, pHdr->sa, LOGE);

        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmAssocCnf.protStatusCode = eSIR_SME_SUCCESS;


        limPostSmeMessage(pMac, LIM_MLM_ASSOC_CNF,
                              (tANI_U32 *) &mlmAssocCnf);
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);

        return;
    }

     // Delete Pre-auth context for the associated BSS
    if (limSearchPreAuthList(pMac, pHdr->sa))
        limDeletePreAuthNode(pMac, pHdr->sa);

    limUpdateAssocStaDatas(pMac, pStaDs, pAssocRsp,psessionEntry);
    // Extract the AP capabilities from the beacon that was received earlier
    // TODO - Watch out for an error response!
    limExtractApCapabilities( pMac,
                            (tANI_U8 *) psessionEntry->pLimJoinReq->bssDescription.ieFields,
                            limGetIElenFromBssDescription( &psessionEntry->pLimJoinReq->bssDescription ),
                            pBeaconStruct );

    if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideStaProtectionOnAssoc(pMac, pBeaconStruct, psessionEntry);

    if(pBeaconStruct->erpPresent) {
        if (pBeaconStruct->erpIEInfo.barkerPreambleMode)
            psessionEntry->beaconParams.fShortPreamble = false;
        else
            psessionEntry->beaconParams.fShortPreamble = true;
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT
    limDiagEventReport(pMac, WLAN_PE_DIAG_CONNECTED, psessionEntry,
                       eSIR_SUCCESS, eSIR_SUCCESS);
#endif
    if( pAssocRsp->QosMapSet.present )
    {
        vos_mem_copy(&psessionEntry->QosMapSet,
                     &pAssocRsp->QosMapSet,
                     sizeof(tSirQosMapSet));
    }
    else
    {
       vos_mem_zero(&psessionEntry->QosMapSet, sizeof(tSirQosMapSet));
    }

    if (pAssocRsp->ExtCap.present)
    {
        struct s_ext_cap *ext_cap = (struct s_ext_cap *)
                                     pAssocRsp->ExtCap.bytes;

        lim_set_stads_rtt_cap(pStaDs, ext_cap);
#ifdef FEATURE_WLAN_TDLS
        psessionEntry->tdls_prohibited =
                ext_cap->TDLSProhibited;
        psessionEntry->tdls_chan_swit_prohibited =
                ext_cap->TDLSChanSwitProhibited;

        PELOG1(limLog(pMac, LOG1,
               FL("ExtCap: tdls_prohibited: %d, tdls_chan_swit_prohibited: %d"),
               ext_cap->TDLSProhibited,
               ext_cap->TDLSChanSwitProhibited);)
#endif
    }
    else
    {
        pStaDs->timingMeasCap = 0;
#ifdef FEATURE_WLAN_TDLS
        psessionEntry->tdls_prohibited = false;
        psessionEntry->tdls_chan_swit_prohibited = false;
#endif
        PELOG1(limLog(pMac, LOG1, FL("ExtCap not present"));)
    }

     //Update the BSS Entry, this entry was added during preassoc.
    if( eSIR_SUCCESS == limStaSendAddBss( pMac, pAssocRsp,  pBeaconStruct,
                   &psessionEntry->pLimJoinReq->bssDescription, true, psessionEntry))
    {
        vos_mem_free(pAssocRsp);
        vos_mem_free(pBeaconStruct);
        return;
    }
    else
    {
        PELOGE(limLog(pMac, LOGE, FL("could not update the bss entry"));)
        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
    }



assocReject:
    if ((subType == LIM_ASSOC)
#ifdef WLAN_FEATURE_VOWIFI_11R
                    || ((subType == LIM_REASSOC) && (psessionEntry->limMlmState == eLIM_MLM_WT_FT_REASSOC_RSP_STATE))
#endif
       ) {
        PELOGE(limLog(pMac, LOGE,  FL("Assoc Rejected by the peer. "
                    "mlmestate: %d sessionid %d Reason: %d MACADDR:"
                    MAC_ADDRESS_STR), psessionEntry->limMlmState,
                    psessionEntry->peSessionId, mlmAssocCnf.resultCode,
                    MAC_ADDR_ARRAY(pHdr->sa));)
        psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, psessionEntry->peSessionId, psessionEntry->limMlmState));

        if (psessionEntry->pLimMlmJoinReq)
        {
            vos_mem_free(psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }

        if (subType == LIM_ASSOC)
        {
           limPostSmeMessage(pMac, LIM_MLM_ASSOC_CNF, (tANI_U32 *) &mlmAssocCnf);
        }
#ifdef WLAN_FEATURE_VOWIFI_11R
        else
        {
                mlmAssocCnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
                limPostSmeMessage(pMac, LIM_MLM_REASSOC_CNF, (tANI_U32 *) &mlmAssocCnf);
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */
    } else {
        limRestorePreReassocState( pMac,
                  eSIR_SME_REASSOC_REFUSED, mlmAssocCnf.protStatusCode,psessionEntry);
    }

    /* CR: vos packet memory is leaked when assoc rsp timeouted/failed. */
    /* notify TL that association is failed so that TL can flush the cached frame  */
    PELOG1(limLog(pMac, LOG1,  FL("notify TL that association is failed"));)
    WLANTL_AssocFailed (psessionEntry->staId);

    vos_mem_free(pBeaconStruct);
    vos_mem_free(pAssocRsp);
    return;
} /*** end limProcessAssocRspFrame() ***/
