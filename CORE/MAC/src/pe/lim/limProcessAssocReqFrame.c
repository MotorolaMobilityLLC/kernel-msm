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
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limProcessAssocReqFrame.cc contains the code
 * for processing Re/Association Request Frame.
 * Author:        Chandra Modumudi
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 05/26/10       js             WPA handling in (Re)Assoc frames
 *
 */
#include "palTypes.h"
#include "aniGlobal.h"
#include "wniCfgAp.h"
#include "sirApi.h"
#include "cfgApi.h"

#include "schApi.h"
#include "pmmApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limStaHashApi.h"
#include "limAdmitControl.h"
#include "palApi.h"


#include "vos_types.h"
/**
 * limConvertSupportedChannels
 *
 *FUNCTION:
 * This function is called by limProcessAssocReqFrame() to
 * parse the channel support IE in the Assoc/Reassoc Request
 * frame, and send relevant information in the SME_ASSOC_IND
 *
 *NOTE:
 *
 * @param  pMac         - A pointer to Global MAC structure
 * @param  pMlmAssocInd - A pointer to SME ASSOC/REASSOC IND
 * @param  assocReq     - A pointer to ASSOC/REASSOC Request frame
 *
 * @return None
 */
static void
limConvertSupportedChannels(tpAniSirGlobal pMac,
                            tpLimMlmAssocInd pMlmAssocInd,
                            tSirAssocReq *assocReq)
{

    tANI_U16   i, j, index=0;
    tANI_U8    firstChannelNumber;
    tANI_U8    numberOfChannel;
    tANI_U8    nextChannelNumber;

    if(assocReq->supportedChannels.length >= SIR_MAX_SUPPORTED_CHANNEL_LIST)
    {
        limLog(pMac, LOG1, FL("Number of supported channels:%d is more than MAX\n"),
                              assocReq->supportedChannels.length);
        pMlmAssocInd->supportedChannels.numChnl = 0;
        return;
    }

    for(i=0; i < (assocReq->supportedChannels.length); i++)
    {
        // Get First Channel Number
        firstChannelNumber = assocReq->supportedChannels.supportedChannels[i];
        pMlmAssocInd->supportedChannels.channelList[index] = firstChannelNumber;
        i++;
        index++;
        if (index >= SIR_MAX_SUPPORTED_CHANNEL_LIST)
        {
            pMlmAssocInd->supportedChannels.numChnl = 0;
            return;
        }
        // Get Number of Channels in a Subband
        numberOfChannel = assocReq->supportedChannels.supportedChannels[i];
        PELOG2(limLog(pMac, LOG2, FL("Rcv AssocReq: chnl=%d, numOfChnl=%d \n"),
                              firstChannelNumber, numberOfChannel);)

        if (numberOfChannel > 1)
        {
            nextChannelNumber = firstChannelNumber;
            if(SIR_BAND_5_GHZ == limGetRFBand(firstChannelNumber))
            {
                for (j=1; j < numberOfChannel; j++)
                {
                    nextChannelNumber += SIR_11A_FREQUENCY_OFFSET;
                    pMlmAssocInd->supportedChannels.channelList[index] = nextChannelNumber;
                    index++;
                    if (index >= SIR_MAX_SUPPORTED_CHANNEL_LIST)
                    {
                        pMlmAssocInd->supportedChannels.numChnl = 0;
                        return;
                    }
                }
            }
            else if(SIR_BAND_2_4_GHZ == limGetRFBand(firstChannelNumber))
            {
                for (j=1; j < numberOfChannel; j++)
                {
                    nextChannelNumber += SIR_11B_FREQUENCY_OFFSET;
                    pMlmAssocInd->supportedChannels.channelList[index] = nextChannelNumber;
                    index++;
                    if (index >= SIR_MAX_SUPPORTED_CHANNEL_LIST)
                    {
                        pMlmAssocInd->supportedChannels.numChnl = 0;
                        return;
                    }
                }
            }
        }
    }

    pMlmAssocInd->supportedChannels.numChnl = (tANI_U8) index;
   PELOG2(limLog(pMac, LOG2,
        FL("Send AssocInd to WSM: spectrum ON, minPwr %d, maxPwr %d, numChnl %d\n"),
        pMlmAssocInd->powerCap.minTxPower,
        pMlmAssocInd->powerCap.maxTxPower,
        pMlmAssocInd->supportedChannels.numChnl);)
}


/**---------------------------------------------------------------
\fn     limProcessAssocReqFrame
\brief  This function is called by limProcessMessageQueue()
\       upon Re/Association Request frame reception in 
\       BTAMP AP or Soft AP role.
\
\param pMac
\param *pRxPacketInfo    - A pointer to Buffer descriptor + associated PDUs
\param subType - Indicates whether it is Association Request(=0) 
\                or Reassociation Request(=1) frame
\return None
------------------------------------------------------------------*/
void
limProcessAssocReqFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,
                        tANI_U8 subType, tpPESession psessionEntry)
{
    tANI_U8                 updateContext;
    tANI_U8                 *pBody;
    tANI_U16                aid, temp;
    tANI_U32                val;
    tANI_S32                framelen;
    tSirRetStatus           status;
    tpSirMacMgmtHdr         pHdr;
    struct tLimPreAuthNode  *pStaPreAuthContext;
    tAniAuthType            authType;
    tSirMacCapabilityInfo   localCapabilities;
    tpDphHashNode           pStaDs = NULL;
    tpSirAssocReq           pAssocReq;
#ifdef WLAN_SOFTAP_FEATURE
    tLimMlmStates           mlmPrevState;
    tDot11fIERSN            Dot11fIERSN;
    tDot11fIEWPA            Dot11fIEWPA;
#endif
    tANI_U32 phyMode;
    tHalBitVal qosMode;
    tHalBitVal wsmMode, wmeMode;
    tANI_U8    *wpsIe = NULL;
    tSirMacRateSet  basicRates;
    tANI_U8 i = 0, j = 0;

    limGetPhyMode(pMac, &phyMode, psessionEntry);

    limGetQosMode(psessionEntry, &qosMode);

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    framelen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

   if (psessionEntry->limSystemRole == eLIM_STA_ROLE || psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE )
   {
        limLog(pMac, LOGE, FL("received unexpected ASSOC REQ subType=%d for role=%d, radioId=%d from \n"),
                                            subType, psessionEntry->limSystemRole, pMac->sys.gSirRadioId);
        limPrintMacAddr(pMac, pHdr->sa, LOGE);
        sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3,
        WDA_GET_RX_MPDU_DATA(pRxPacketInfo), framelen);
        return;
    }

    // Get pointer to Re/Association Request frame body
    pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

    if (limIsGroupAddr(pHdr->sa))
    {
        // Received Re/Assoc Req frame from a BC/MC address
        // Log error and ignore it
        if (subType == LIM_ASSOC)
            limLog(pMac, LOG1, FL("received Assoc frame from a BC/MC address\n"));
        else
            limLog(pMac, LOG1, FL("received ReAssoc frame from a BC/MC address\n"));
        limPrintMacAddr(pMac, pHdr->sa, LOG1);
        return;
    }
    limLog(pMac, LOG2, FL("Received AssocReq Frame: "));
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2, (tANI_U8 *) pBody, framelen);

#ifdef WLAN_SOFTAP_FEATURE
    // If TKIP counter measures active send Assoc Rsp frame to station with eSIR_MAC_MIC_FAILURE_REASON
    if ((psessionEntry->bTkipCntrMeasActive) && (psessionEntry->limSystemRole == eLIM_AP_ROLE))
    {
        limSendAssocRspMgmtFrame(pMac,
                                    eSIR_MAC_MIC_FAILURE_REASON,
                                    1,
                                    pHdr->sa,
                                    subType, 0, psessionEntry);
        return;
    }
#endif

    // Allocate memory for the Assoc Request frame
    if ( palAllocateMemory(pMac->hHdd, (void **)&pAssocReq, sizeof(*pAssocReq)) != eHAL_STATUS_SUCCESS) 
    {
        limLog(pMac, LOGP, FL("PAL Allocate Memory failed in AssocReq\n"));
        return;
    }
    palZeroMemory( pMac->hHdd, (void *)pAssocReq , sizeof(*pAssocReq));
   
    // Parse Assoc Request frame
    if (subType == LIM_ASSOC)
        status = sirConvertAssocReqFrame2Struct(pMac, pBody, framelen, pAssocReq);
    else
        status = sirConvertReassocReqFrame2Struct(pMac, pBody, framelen, pAssocReq);

    if (status != eSIR_SUCCESS)
    {
        limLog(pMac, LOGW, FL("Parse error AssocRequest, length=%d from \n"),framelen);
        limPrintMacAddr(pMac, pHdr->sa, LOGW);
        limSendAssocRspMgmtFrame(pMac, eSIR_MAC_UNSPEC_FAILURE_STATUS, 1, pHdr->sa, subType, 0, psessionEntry);
        goto error;
    }

    if ( palAllocateMemory(pMac->hHdd, (void **)&pAssocReq->assocReqFrame, framelen) != eHAL_STATUS_SUCCESS) 
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory for the assoc req, length=%d from \n"),framelen);
        goto error;
    }
    
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pAssocReq->assocReqFrame,
                  (tANI_U8 *) pBody, framelen);
    pAssocReq->assocReqFrameLength = framelen;    

    if (cfgGetCapabilityInfo(pMac, &temp,psessionEntry) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("could not retrieve Capabilities\n"));
        goto error;
    }
#if defined(ANI_PRODUCT_TYPE_AP) && defined(ANI_LITTLE_BYTE_ENDIAN)
    *(tANI_U16*)&localCapabilities=(tANI_U16)(temp);
#else
    limCopyU16((tANI_U8 *) &localCapabilities, temp);
#endif

    if (limCompareCapabilities(pMac,
                               pAssocReq,
                               &localCapabilities,psessionEntry) == false)
    {
        /**
         * Capabilities of requesting STA does not match with
         * local capabilities. Respond with 'unsupported capabilities'
         * status code.
         */
        limSendAssocRspMgmtFrame(
                        pMac,
                        eSIR_MAC_CAPABILITIES_NOT_SUPPORTED_STATUS,
                        1,
                        pHdr->sa,
                        subType, 0,psessionEntry);

        limLog(pMac, LOG1, FL("local caps 0x%x received 0x%x\n"), localCapabilities, pAssocReq->capabilityInfo);

        // Log error
        if (subType == LIM_ASSOC)
            limLog(pMac, LOG1,
               FL("received Assoc req with unsupported capabilities from\n"));
        else
            limLog(pMac, LOG1,
               FL("received ReAssoc req with unsupported capabilities from\n"));
        limPrintMacAddr(pMac, pHdr->sa, LOG1);
        goto error;
    }

    updateContext = false;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    // Check if multiple SSID feature is not enabled
    if (psessionEntry->pLimStartBssReq->ssId.length)
    {
        if (limCmpSSid(pMac, &pAssocReq->ssId, psessionEntry) == false)
        {
            /*Temp hack for UPF35 - skip SSID check in order to be able to interop 
             with Marvel - they send their own SSID instead of ours*/
            if ( 0 != vos_get_skip_ssid_check())
            {
                limLog(pMac, LOG1, FL("Received unmatched SSID but cfg to suppress - continuing\n"));
            }
            else
            {
              /**
              * Received Re/Association Request with either
              * Broadcast SSID OR with SSID that does not
              * match with local one.
              * Respond with unspecified status code.
              */
              limSendAssocRspMgmtFrame(pMac,
                               eSIR_MAC_UNSPEC_FAILURE_STATUS,
                               1,
                               pHdr->sa,
                               subType, 0,psessionEntry);
  
              // Log error
              if (subType == LIM_ASSOC)
                  limLog(pMac, LOGW, FL("received Assoc req with unmatched SSID from \n"));
              else
                  limLog(pMac, LOGW, FL("received ReAssoc req with unmatched SSID from \n"));
              limPrintMacAddr(pMac, pHdr->sa, LOGW);
              goto error;
          }
      }
    }
    else
        limLog(pMac, LOG1, FL("Suppressed SSID, App is going to check SSID\n"));
#else
    if (limCmpSSid(pMac, &pAssocReq->ssId, psessionEntry) == false)
    {
        /**
         * Received Re/Association Request with either
         * Broadcast SSID OR with SSID that does not
         * match with local one.
         * Respond with unspecified status code.
         */
        limSendAssocRspMgmtFrame(pMac,
                             eSIR_MAC_UNSPEC_FAILURE_STATUS,
                             1,
                             pHdr->sa,
                             subType, 0,psessionEntry);

        // Log error
        if (subType == LIM_ASSOC)
            limLog(pMac, LOGW,
                   FL("received Assoc req with unmatched SSID from \n"));
        else
            limLog(pMac, LOGW,
                   FL("received ReAssoc req with unmatched SSID from \n"));
        limPrintMacAddr(pMac, pHdr->sa, LOGW);
        goto error;
    }
#endif

    /***************************************************************
      ** Verify if the requested rates are available in supported rate
      ** set or Extended rate set. Some APs are adding basic rates in
      ** Extended rateset IE
      ***************************************************************/
    basicRates.numRates = 0;

    for(i = 0; i < pAssocReq->supportedRates.numRates; i++)
    {
        basicRates.rate[i] = pAssocReq->supportedRates.rate[i];
        basicRates.numRates++;
    }

    for(j = 0; (j < pAssocReq->extendedRates.numRates) && (i < SIR_MAC_RATESET_EID_MAX); i++,j++)
    {
        basicRates.rate[i] = pAssocReq->extendedRates.rate[j];
        basicRates.numRates++;
    }
    if (limCheckRxBasicRates(pMac, basicRates, psessionEntry) == false)
    {
        /**
         * Requesting STA does not support ALL BSS basic
         * rates. Respond with 'basic rates not supported'
         * status code.
         */
        limSendAssocRspMgmtFrame(
                    pMac,
                    eSIR_MAC_BASIC_RATES_NOT_SUPPORTED_STATUS,
                    1,
                    pHdr->sa,
                    subType, 0,psessionEntry);

        // Log error
        if (subType == LIM_ASSOC)
            limLog(pMac, LOGW,
               FL("received Assoc req with unsupported rates from \n"));
        else
            limLog(pMac, LOGW,
               FL("received ReAssoc req with unsupported rates from\n"));
        limPrintMacAddr(pMac, pHdr->sa, LOGW);
        goto error;
    }

#ifdef WLAN_SOFTAP_FEATURE

    if((psessionEntry->limSystemRole == eLIM_AP_ROLE ) &&
       (psessionEntry->dot11mode == WNI_CFG_DOT11_MODE_11G_ONLY) &&
       ((!pAssocReq->extendedRatesPresent ) || (pAssocReq->HTCaps.present)))
    {
        limSendAssocRspMgmtFrame( pMac, eSIR_MAC_CAPABILITIES_NOT_SUPPORTED_STATUS, 
                                  1, pHdr->sa, subType, 0, psessionEntry );
        limLog(pMac, LOGE, FL("SOFTAP was in 11G only mode, rejecting legacy STA's\n"));
        goto error;

    }//end if phyMode == 11G_only
 
    if((psessionEntry->limSystemRole == eLIM_AP_ROLE) && 
       (psessionEntry->dot11mode == WNI_CFG_DOT11_MODE_11N_ONLY) && 
       (!pAssocReq->HTCaps.present))
    {
        limSendAssocRspMgmtFrame( pMac, eSIR_MAC_CAPABILITIES_NOT_SUPPORTED_STATUS, 
                                  1, pHdr->sa, subType, 0, psessionEntry );
        limLog(pMac, LOGE, FL("SOFTAP was in 11N only mode, rejecting legacy STA's\n"));
        goto error;
    }//end if PhyMode == 11N_only

#endif

    /* Spectrum Management (11h) specific checks */
    if (localCapabilities.spectrumMgt)
    {
        tSirRetStatus status = eSIR_SUCCESS;

        /* If station is 11h capable, then it SHOULD send all mandatory 
         * IEs in assoc request frame. Let us verify that
         */
        if (pAssocReq->capabilityInfo.spectrumMgt)
        {
            if (!((pAssocReq->powerCapabilityPresent) && (pAssocReq->supportedChannelsPresent)))
            {
                /* One or more required information elements are missing, log the peers error */
                if (!pAssocReq->powerCapabilityPresent)
                {
                    if(subType == LIM_ASSOC)
                       limLog(pMac, LOG1, FL("LIM Info: Missing Power capability IE in assoc request\n"));
                    else
                       limLog(pMac, LOG1, FL("LIM Info: Missing Power capability IE in Reassoc request\n"));
                }
                if (!pAssocReq->supportedChannelsPresent)
                {
                    if(subType == LIM_ASSOC)
                        limLog(pMac, LOG1, FL("LIM Info: Missing Supported channel IE in assoc request\n"));
                    else
                        limLog(pMac, LOG1, FL("LIM Info: Missing Supported channel IE in Reassoc request\n"));
                }
                limPrintMacAddr(pMac, pHdr->sa, LOG1);
            }
            else
            {
                /* Assoc request has mandatory fields */
                status = limIsDot11hPowerCapabilitiesInRange(pMac, pAssocReq, psessionEntry);
                if (eSIR_SUCCESS != status)
                {
                    if (subType == LIM_ASSOC)
                        limLog(pMac, LOGW, FL("LIM Info: Association MinTxPower(STA) > MaxTxPower(AP)\n"));
                    else
                        limLog(pMac, LOGW, FL("LIM Info: Reassociation MinTxPower(STA) > MaxTxPower(AP)\n"));
                    limPrintMacAddr(pMac, pHdr->sa, LOGW);
                }
                status = limIsDot11hSupportedChannelsValid(pMac, pAssocReq);
                if (eSIR_SUCCESS != status)
                {
                    if (subType == LIM_ASSOC)
                        limLog(pMac, LOGW, FL("LIM Info: Association wrong supported channels (STA)\n"));
                    else
                        limLog(pMac, LOGW, FL("LIM Info: Reassociation wrong supported channels (STA)\n"));
                    limPrintMacAddr(pMac, pHdr->sa, LOGW);
                }
                /* IEs are valid, use them if needed */
            }
        } //if(assoc.capabilityInfo.spectrumMgt)
        else
        {
            /* As per the capabiities, the spectrum management is not enabled on the station
             * The AP may allow the associations to happen even if spectrum management is not
             * allowed, if the transmit power of station is below the regulatory maximum
             */

            /* TODO: presently, this is not handled. In the current implemetation, the AP would
             * allow the station to associate even if it doesn't support spectrum management.
             */
        }
    }// end of spectrum management related processing

    if ( (pAssocReq->HTCaps.present) && (limCheckMCSSet(pMac, pAssocReq->HTCaps.supportedMCSSet) == false))
    {
        /**
         * Requesting STA does not support ALL BSS MCS basic Rate set rates.
         * Spec does not define any status code for this scenario.
         */
        limSendAssocRspMgmtFrame(
                    pMac,
                    eSIR_MAC_OUTSIDE_SCOPE_OF_SPEC_STATUS,
                    1,
                    pHdr->sa,
                    subType, 0,psessionEntry);

        // Log error
        if (subType == LIM_ASSOC)
            limLog(pMac, LOGW,
               FL("received Assoc req with unsupported MCS Rate Set from \n"));
        else
            limLog(pMac, LOGW,
               FL("received ReAssoc req with unsupported MCS Rate Set from\n"));
        limPrintMacAddr(pMac, pHdr->sa, LOGW);
        goto error;
    }

    //if (pMac->dph.gDphPhyMode == WNI_CFG_PHY_MODE_11G)
    if (phyMode == WNI_CFG_PHY_MODE_11G)
    {

        if (wlan_cfgGetInt(pMac, WNI_CFG_11G_ONLY_POLICY, &val) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("could not retrieve 11g-only flag\n"));
            goto error;
        }

        if (!pAssocReq->extendedRatesPresent && val)
        {
            /**
             * Received Re/Association Request from
             * 11b STA when 11g only policy option
             * is set.
             * Reject with unspecified status code.
             */
            limSendAssocRspMgmtFrame(
                           pMac,
                           eSIR_MAC_BASIC_RATES_NOT_SUPPORTED_STATUS,
                           1,
                           pHdr->sa,
                           subType, 0,psessionEntry);

            limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from 11b STA: "));
            limPrintMacAddr(pMac, pHdr->sa, LOGW);
            
#ifdef WLAN_DEBUG    
            pMac->lim.gLim11bStaAssocRejectCount++;
#endif
            goto error;
        }
    }

#ifdef WMM_APSD
    // Save the QOS info element in assoc request..
    limGetWmeMode(pMac, &wmeMode);
    if (wmeMode == eHAL_SET)
    {
        tpQosInfoSta qInfo;

        qInfo = (tpQosInfoSta) (pAssocReq->qosCapability.qosInfo);

        if ((pMac->lim.gWmmApsd.apsdEnable == 0) && (qInfo->ac_be || qInfo->ac_bk || qInfo->ac_vo || qInfo->ac_vi))
        {

            /**
             * Received Re/Association Request from
             * 11b STA when 11g only policy option
             * is set.
             * Reject with unspecified status code.
             */
            limSendAssocRspMgmtFrame(
                           pMac,
                           eSIR_MAC_WME_REFUSED_STATUS,
                           1,
                           pHdr->sa,
                           subType, 0,psessionEntry);

            limLog(pMac, LOGW,
                   FL("Rejecting Re/Assoc req from STA: "));
            limPrintMacAddr(pMac, pHdr->sa, LOGW);
            limLog(pMac, LOGE, FL("APSD not enabled, qosInfo - 0x%x\n"), *qInfo);
            goto error;
        }
    }
#endif

    // Check for 802.11n HT caps compatibility; are HT Capabilities
    // turned on in lim?
    if ( psessionEntry->htCapabality )
    {
        // There are; are they turned on in the STA?
        if ( pAssocReq->HTCaps.present )
        {
            // The station *does* support 802.11n HT capability...

            limLog( pMac, LOG1, FL( "AdvCodingCap:%d ChaWidthSet:%d "
                                    "PowerSave:%d greenField:%d "
                                    "shortGI20:%d shortGI40:%d\n"
                                    "txSTBC:%d rxSTBC:%d delayBA:%d"
                                    "maxAMSDUsize:%d DSSS/CCK:%d "
                                    "PSMP:%d stbcCntl:%d lsigTXProt:%d\n"),
                    pAssocReq->HTCaps.advCodingCap,
                    pAssocReq->HTCaps.supportedChannelWidthSet,
                    pAssocReq->HTCaps.mimoPowerSave,
                    pAssocReq->HTCaps.greenField,
                    pAssocReq->HTCaps.shortGI20MHz,
                    pAssocReq->HTCaps.shortGI40MHz,
                    pAssocReq->HTCaps.txSTBC,
                    pAssocReq->HTCaps.rxSTBC,
                    pAssocReq->HTCaps.delayedBA,
                    pAssocReq->HTCaps.maximalAMSDUsize,
                    pAssocReq->HTCaps.dsssCckMode40MHz,
                    pAssocReq->HTCaps.psmp,
                    pAssocReq->HTCaps.stbcControlFrame,
                    pAssocReq->HTCaps.lsigTXOPProtection );

                // Make sure the STA's caps are compatible with our own:
                //11.15.2 Support of DSSS/CCK in 40 MHz
                //the AP shall refuse association requests from an HT STA that has the DSSS/CCK 
                //Mode in 40 MHz subfield set to 1;

            //FIXME_BTAMP_AP : Need to be enabled 
            /*
            if ( !pMac->lim.gHTDsssCckRate40MHzSupport && pAssocReq->HTCaps.dsssCckMode40MHz )
            {
                statusCode = eSIR_MAC_DSSS_CCK_RATE_NOT_SUPPORT_STATUS;
                limLog( pMac, LOGW, FL( "AP DSSS/CCK is disabled; "
                                        "STA rejected.\n" ) );
                // Reject association
                limSendAssocRspMgmtFrame( pMac, statusCode, 1, pHdr->sa, subType, 0,psessionEntry);
                goto error;
            }
            */
        }
    } // End if on HT caps turned on in lim.

#ifdef WLAN_SOFTAP_FEATURE
    /* Clear the buffers so that frame parser knows that there isn't a previously decoded IE in these buffers */
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&Dot11fIERSN, sizeof( Dot11fIERSN ) );
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&Dot11fIEWPA, sizeof( Dot11fIEWPA ) );

    /* if additional IE is present, check if it has WscIE */
    if( pAssocReq->addIEPresent && pAssocReq->addIE.length )
        wpsIe = limGetWscIEPtr(pMac, pAssocReq->addIE.addIEdata, pAssocReq->addIE.length);
    /* when wpsIe is present, RSN/WPA IE is ignored */
    if( wpsIe == NULL )
    {
        /** check whether as RSN IE is present */
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE 
            && psessionEntry->pLimStartBssReq->privacy 
            && psessionEntry->pLimStartBssReq->rsnIE.length)
        {
            limLog(pMac, LOGE,
                   FL("AP supports RSN enabled authentication\n"));

            if(pAssocReq->rsnPresent)
            {
                if(pAssocReq->rsn.length)
                {
                    // Unpack the RSN IE 
                    dot11fUnpackIeRSN(pMac, 
                                        &pAssocReq->rsn.info[0], 
                                        pAssocReq->rsn.length, 
                                        &Dot11fIERSN);

                    /* Check RSN version is supported or not */
                    if(SIR_MAC_OUI_VERSION_1 == Dot11fIERSN.version)
                    {
                        /* check the groupwise and pairwise cipher suites */
                        if(eSIR_SUCCESS != (status = limCheckRxRSNIeMatch(pMac, Dot11fIERSN, psessionEntry, pAssocReq->HTCaps.present) ) )
                        {
                            /* some IE is not properly sent */
                            /* received Association req frame with RSN IE but length is 0 */
                            limSendAssocRspMgmtFrame(
                                           pMac,
                                           status,
                                           1,
                                           pHdr->sa,
                                           subType, 0,psessionEntry);

                            limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from STA: "));
                            limPrintMacAddr(pMac, pHdr->sa, LOGW);
                            goto error;

                        }
                    }
                    else
                    {
                        /* received Association req frame with RSN IE version wrong */
                        limSendAssocRspMgmtFrame(
                                       pMac,
                                       eSIR_MAC_UNSUPPORTED_RSN_IE_VERSION_STATUS,
                                       1,
                                       pHdr->sa,
                                       subType, 0,psessionEntry);

                        limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from STA: "));
                        limPrintMacAddr(pMac, pHdr->sa, LOGW);
                        goto error;

                    }
                }
                else
                {
                    /* received Association req frame with RSN IE but length is 0 */
                    limSendAssocRspMgmtFrame(
                                   pMac,
                                   eSIR_MAC_INVALID_INFORMATION_ELEMENT_STATUS,
                                   1,
                                   pHdr->sa,
                                   subType, 0,psessionEntry);

                    limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from STA: "));
                    limPrintMacAddr(pMac, pHdr->sa, LOGW);
                    goto error;
                    
                }
            } /* end - if(pAssocReq->rsnPresent) */
            if((!pAssocReq->rsnPresent) && pAssocReq->wpaPresent)
            {
                // Unpack the WPA IE 
                if(pAssocReq->wpa.length)
                {
                    dot11fUnpackIeWPA(pMac, 
                                        &pAssocReq->wpa.info[4], //OUI is not taken care
                                        pAssocReq->wpa.length, 
                                        &Dot11fIEWPA);
                    /* check the groupwise and pairwise cipher suites */
                    if(eSIR_SUCCESS != (status = limCheckRxWPAIeMatch(pMac, Dot11fIEWPA, psessionEntry, pAssocReq->HTCaps.present)))
                    {
                        /* received Association req frame with WPA IE but mismatch */
                        limSendAssocRspMgmtFrame(
                                       pMac,
                                       status,
                                       1,
                                       pHdr->sa,
                                       subType, 0,psessionEntry);

                        limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from STA: "));
                        limPrintMacAddr(pMac, pHdr->sa, LOGW);
                        goto error;

                    }
                }
                else
                {
                    /* received Association req frame with invalid WPA IE */
                    limSendAssocRspMgmtFrame(
                                   pMac,
                                   eSIR_MAC_INVALID_INFORMATION_ELEMENT_STATUS,
                                   1,
                                   pHdr->sa,
                                   subType, 0,psessionEntry);

                    limLog(pMac, LOGW, FL("Rejecting Re/Assoc req from STA: "));
                    limPrintMacAddr(pMac, pHdr->sa, LOGW);
                    goto error;
                }/* end - if(pAssocReq->wpa.length) */
            } /* end - if(pAssocReq->wpaPresent) */
        } /* end of if(psessionEntry->pLimStartBssReq->privacy 
            && psessionEntry->pLimStartBssReq->rsnIE->length) */

    } /* end of     if( ! pAssocReq->wscInfo.present ) */
#endif //WLAN_SOFTAP_FEATURE

    /**
     * Extract 'associated' context for STA, if any.
     * This is maintained by DPH and created by LIM.
     */
    pStaDs = dphLookupHashEntry(pMac, pHdr->sa, &aid, &psessionEntry->dph.dphHashTable);

    /// Extract pre-auth context for the STA, if any.
    pStaPreAuthContext = limSearchPreAuthList(pMac, pHdr->sa);

    if (pStaDs == NULL)
    {
        /// Requesting STA is not currently associated
        if (pMac->lim.gLimNumOfCurrentSTAs == pMac->lim.maxStation)
        {
            /**
             * Maximum number of STAs that AP can handle reached.
             * Send Association response to peer MAC entity
             */
            limRejectAssociation(pMac, pHdr->sa,
                                 subType, false,
                                 (tAniAuthType) 0, 0,
                                 false,
                                 (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

            goto error;
        }

        /// Check if STA is pre-authenticated.
        if ((pStaPreAuthContext == NULL) ||
            (pStaPreAuthContext &&
             (pStaPreAuthContext->mlmState !=
                              eLIM_MLM_AUTHENTICATED_STATE)))
        {
            /**
             * STA is not pre-authenticated yet requesting
             * Re/Association before Authentication.
             * OR STA is in the process of getting authenticated
             * and sent Re/Association request.
             * Send Deauthentication frame with 'prior
             * authentication required' reason code.
             */
            limSendDeauthMgmtFrame(
                     pMac,
                     eSIR_MAC_STA_NOT_PRE_AUTHENTICATED_REASON, //=9
                     pHdr->sa,psessionEntry);

            // Log error
            if (subType == LIM_ASSOC)
                limLog(pMac, LOG1,
                       FL("received Assoc req from STA that does not have pre-auth context, MAC addr is: \n"));
            else
                limLog(pMac, LOG1,
                       FL("received ReAssoc req from STA that does not have pre-auth context, MAC addr is: \n"));
            limPrintMacAddr(pMac, pHdr->sa, LOG1);
            goto error;
        }

        /// Delete 'pre-auth' context of STA
        authType = pStaPreAuthContext->authType;
        limDeletePreAuthNode(pMac, pHdr->sa);

        // All is well. Assign AID (after else part)

    } // if (pStaDs == NULL)
    else
    {
        // STA context does exist for this STA

        if (pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE)
        {
            /**
             * Requesting STA is in some 'transient' state?
             * Ignore the Re/Assoc Req frame by incrementing
             * debug counter & logging error.
             */
            if (subType == LIM_ASSOC)
            {
            
#ifdef WLAN_DEBUG    
                pMac->lim.gLimNumAssocReqDropInvldState++;
#endif
                limLog(pMac, LOG1, FL("received Assoc req in state %X from "), pStaDs->mlmStaContext.mlmState);
            }
            else
            {     
#ifdef WLAN_DEBUG    
                pMac->lim.gLimNumReassocReqDropInvldState++;
#endif
                limLog(pMac, LOG1, FL("received ReAssoc req in state %X from "), pStaDs->mlmStaContext.mlmState);
            }
            limPrintMacAddr(pMac, pHdr->sa, LOG1);
            limPrintMlmState(pMac, LOG1, (tLimMlmStates) pStaDs->mlmStaContext.mlmState);
            goto error;
        } // if (pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE)

        /**
         * STA sent Re/Association Request frame while already in
         * 'associated' state. Update STA capabilities and
         * send Association response frame with same AID
         */

        pStaDs->mlmStaContext.capabilityInfo = pAssocReq->capabilityInfo;

        if (pStaPreAuthContext &&
            (pStaPreAuthContext->mlmState ==
                                       eLIM_MLM_AUTHENTICATED_STATE))
        {
            /// STA has triggered pre-auth again
            authType = pStaPreAuthContext->authType;
            limDeletePreAuthNode(pMac, pHdr->sa);
        }
        else
            authType = pStaDs->mlmStaContext.authType;

        updateContext = true;

        if (dphInitStaState(pMac, pHdr->sa, aid, true, &psessionEntry->dph.dphHashTable) == NULL)   
        {
            limLog(pMac, LOGE, FL("could not Init STAid=%d\n"), aid);
            goto  error;
        }


        goto sendIndToSme;
    } // end if (lookup for STA in perStaDs fails)



    // check if sta is allowed per QoS AC rules
    //if (pMac->dph.gDphQosEnabled || pMac->dph.gDphWmeEnabled)
    limGetWmeMode(psessionEntry, &wmeMode);
    if ((qosMode == eHAL_SET) || (wmeMode == eHAL_SET))
    {
        // for a qsta, check if the requested Traffic spec
        // is admissible
        // for a non-qsta check if the sta can be admitted
        if (pAssocReq->addtsPresent)
        {
            tANI_U8 tspecIdx = 0; //index in the sch tspec table.
            if (limAdmitControlAddTS(pMac, pHdr->sa, &(pAssocReq->addtsReq),
                                     &(pAssocReq->qosCapability), 0, false, NULL, &tspecIdx, psessionEntry) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGW, FL("AdmitControl: TSPEC rejected\n"));
                limSendAssocRspMgmtFrame(
                               pMac,
                               eSIR_MAC_QAP_NO_BANDWIDTH_REASON,
                               1,
                               pHdr->sa,
                               subType, 0,psessionEntry);
#ifdef WLAN_DEBUG    
                pMac->lim.gLimNumAssocReqDropACRejectTS++;
#endif
                goto error;
            }
        }
        else if (limAdmitControlAddSta(pMac, pHdr->sa, false)
                                               != eSIR_SUCCESS)
        {
            limLog(pMac, LOGW, FL("AdmitControl: Sta rejected\n"));
            limSendAssocRspMgmtFrame(
                    pMac,
                    eSIR_MAC_QAP_NO_BANDWIDTH_REASON,
                    1,
                    pHdr->sa,
                    subType, 0,psessionEntry);
#ifdef WLAN_DEBUG    
            pMac->lim.gLimNumAssocReqDropACRejectSta++;
#endif
            goto error;
        }

        // else all ok
        limLog(pMac, LOG1, FL("AdmitControl: Sta OK!\n"));
    }

    /**
     * STA is Associated !
     */
    if (subType == LIM_ASSOC)
        limLog(pMac, LOG1, FL("received Assoc req successful from "));
    else
        limLog(pMac, LOG1, FL("received ReAssoc req successful from "));
    limPrintMacAddr(pMac, pHdr->sa, LOG1);

    /**
     * Assign unused/least recently used AID from perStaDs.
     * This will 12-bit STAid used by MAC HW.
     * NOTE: limAssignAID() assigns AID values ranging between 1 - 255
     */

    aid = limAssignAID(pMac);

    if (!aid)
    {
        // Could not assign AID
        // Reject association
        limRejectAssociation(pMac, pHdr->sa,
                             subType, true, authType,
                             aid, false,
                             (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

        goto error;
    }

    /**
     * Add an entry to hash table maintained by DPH module
     */

    pStaDs = dphAddHashEntry(pMac, pHdr->sa, aid, &psessionEntry->dph.dphHashTable);

    if (pStaDs == NULL)
    {
        // Could not add hash table entry at DPH
        limLog(pMac, LOGE,
           FL("could not add hash entry at DPH for aid=%d, MacAddr:\n"),
           aid);
        limPrintMacAddr(pMac, pHdr->sa, LOGE);

        // Release AID
        limReleaseAID(pMac, aid);

        limRejectAssociation(pMac, pHdr->sa,
                             subType, true, authType, aid, false,
                             (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

        goto error;
    }


sendIndToSme:

    psessionEntry->parsedAssocReq[pStaDs->assocId] = pAssocReq;

    pStaDs->mlmStaContext.htCapability = pAssocReq->HTCaps.present;
    pStaDs->qos.addtsPresent = (pAssocReq->addtsPresent==0) ? false : true;
    pStaDs->qos.addts        = pAssocReq->addtsReq;
    pStaDs->qos.capability   = pAssocReq->qosCapability;
    pStaDs->versionPresent   = 0;
    /* short slot and short preamble should be updated before doing limaddsta */
    pStaDs->shortPreambleEnabled = (tANI_U8)pAssocReq->capabilityInfo.shortPreamble;
    pStaDs->shortSlotTimeEnabled = (tANI_U8)pAssocReq->capabilityInfo.shortSlotTime;
 
    if (pAssocReq->propIEinfo.versionPresent) //update STA version info
    {
        pStaDs->versionPresent = 1;
        pStaDs->version = pAssocReq->propIEinfo.version;
    }
    pStaDs->propCapability = 0;
    if (pAssocReq->propIEinfo.capabilityPresent)
    {
        if (sirGetCfgPropCaps(pMac, &pStaDs->propCapability))
            pStaDs->propCapability &= pAssocReq->propIEinfo.capability;
    }

    pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_ASSOC_CNF_STATE;
    pStaDs->valid                  = 0;
    pStaDs->mlmStaContext.authType = authType;
    pStaDs->staType = STA_ENTRY_PEER;

    //TODO: If listen interval is more than certain limit, reject the association.
    //Need to check customer requirements and then implement.
    pStaDs->mlmStaContext.listenInterval = pAssocReq->listenInterval;
    pStaDs->mlmStaContext.capabilityInfo = pAssocReq->capabilityInfo;

    /* The following count will be used to knock-off the station if it doesn't
     * come back to receive the buffered data. The AP will wait for numTimSent number
     * of beacons after sending TIM information for the station, before assuming that 
     * the station is no more associated and disassociates it
     */

    /** timWaitCount is used by PMM for monitoring the STA's in PS for LINK*/
    pStaDs->timWaitCount = (tANI_U8)GET_TIM_WAIT_COUNT(pAssocReq->listenInterval);
    
    /** Initialise the Current successful MPDU's tranfered to this STA count as 0 */
    pStaDs->curTxMpduCnt = 0;

    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
      (pAssocReq->HTCaps.present))
    {
        pStaDs->htGreenfield = (tANI_U8)pAssocReq->HTCaps.greenField;
        pStaDs->htAMpduDensity = pAssocReq->HTCaps.mpduDensity;
        pStaDs->htDsssCckRate40MHzSupport = (tANI_U8)pAssocReq->HTCaps.dsssCckMode40MHz;
        pStaDs->htLsigTXOPProtection = (tANI_U8)pAssocReq->HTCaps.lsigTXOPProtection;
        pStaDs->htMaxAmsduLength = (tANI_U8)pAssocReq->HTCaps.maximalAMSDUsize;
        pStaDs->htMaxRxAMpduFactor = pAssocReq->HTCaps.maxRxAMPDUFactor;
        pStaDs->htMIMOPSState = pAssocReq->HTCaps.mimoPowerSave;
        pStaDs->htShortGI20Mhz = (tANI_U8)pAssocReq->HTCaps.shortGI20MHz;
        pStaDs->htShortGI40Mhz = (tANI_U8)pAssocReq->HTCaps.shortGI40MHz;
        pStaDs->htSupportedChannelWidthSet = (tANI_U8)pAssocReq->HTCaps.supportedChannelWidthSet;
        pStaDs->baPolicyFlag = 0xFF;
    }



    if (limPopulateMatchingRateSet(pMac,
                                   pStaDs,
                                   &(pAssocReq->supportedRates),
                                   &(pAssocReq->extendedRates),
                                   pAssocReq->HTCaps.supportedMCSSet,
                                   &(pAssocReq->propIEinfo.propRates), psessionEntry) != eSIR_SUCCESS)
    {
        // Could not update hash table entry at DPH with rateset
        limLog(pMac, LOGE,
           FL("could not update hash entry at DPH for aid=%d, MacAddr:\n"),
           aid);
        limPrintMacAddr(pMac, pHdr->sa, LOGE);

                // Release AID
        limReleaseAID(pMac, aid);


        limRejectAssociation(pMac, pHdr->sa,
                             subType, true, authType, aid, true,
                             (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

        /*return it from here rather than goto error statement.This is done as the memory is getting free twice*/
        return;
        //goto error;
    }

    palCopyMemory( pMac->hHdd, (tANI_U8 *) &pStaDs->mlmStaContext.propRateSet,
                  (tANI_U8 *) &(pAssocReq->propIEinfo.propRates),
                  pAssocReq->propIEinfo.propRates.numPropRates + 1);

    /// Add STA context at MAC HW (BMU, RHP & TFP)

    pStaDs->qosMode    = eANI_BOOLEAN_FALSE;
    pStaDs->lleEnabled = eANI_BOOLEAN_FALSE;
    if (pAssocReq->capabilityInfo.qos && (qosMode == eHAL_SET))
    {
        pStaDs->lleEnabled = eANI_BOOLEAN_TRUE;
        pStaDs->qosMode    = eANI_BOOLEAN_TRUE; 
    }

    pStaDs->wmeEnabled = eANI_BOOLEAN_FALSE;
    pStaDs->wsmEnabled = eANI_BOOLEAN_FALSE;
    limGetWmeMode(psessionEntry, &wmeMode);
    //if ((! pStaDs->lleEnabled) && assoc.wmeInfoPresent && pMac->dph.gDphWmeEnabled)
    if ((! pStaDs->lleEnabled) && pAssocReq->wmeInfoPresent && (wmeMode == eHAL_SET))
    {
        pStaDs->wmeEnabled = eANI_BOOLEAN_TRUE;
        pStaDs->qosMode = eANI_BOOLEAN_TRUE;
        limGetWsmMode(psessionEntry, &wsmMode);
        /* WMM_APSD - WMM_SA related processing should be separate; WMM_SA and WMM_APSD
         can coexist */
#ifdef WLAN_SOFTAP_FEATURE
        if( pAssocReq->WMMInfoStation.present)
        {
            /* check whether AP supports or not */
            if ((psessionEntry->limSystemRole == eLIM_AP_ROLE)
                 && (psessionEntry->apUapsdEnable == 0) && (pAssocReq->WMMInfoStation.acbe_uapsd 
                    || pAssocReq->WMMInfoStation.acbk_uapsd 
                    || pAssocReq->WMMInfoStation.acvo_uapsd 
                    || pAssocReq->WMMInfoStation.acvi_uapsd))
            {

                /**
                 * Received Re/Association Request from
                 * STA when UPASD is not supported.
                 */
                limLog( pMac, LOGE, FL( "AP do not support UPASD REASSOC Failed\n" ));
                limRejectAssociation(pMac, pHdr->sa,
                                     subType, true, authType, aid, true,
                                     (tSirResultCodes) eSIR_MAC_WME_REFUSED_STATUS, psessionEntry);


                /*return it from here rather than goto error statement.This is done as the memory is getting free twice in this uapsd scenario*/
                return;
                //goto error;
            }
            else
            {
                /* update UAPSD and send it to LIM to add STA */
                pStaDs->qos.capability.qosInfo.acbe_uapsd = pAssocReq->WMMInfoStation.acbe_uapsd;
                pStaDs->qos.capability.qosInfo.acbk_uapsd = pAssocReq->WMMInfoStation.acbk_uapsd;
                pStaDs->qos.capability.qosInfo.acvo_uapsd = pAssocReq->WMMInfoStation.acvo_uapsd;
                pStaDs->qos.capability.qosInfo.acvi_uapsd = pAssocReq->WMMInfoStation.acvi_uapsd;
                pStaDs->qos.capability.qosInfo.maxSpLen = pAssocReq->WMMInfoStation.max_sp_length;
            }
        }
#endif
        //if (assoc.wsmCapablePresent && pMac->dph.gDphWsmEnabled)
        if (pAssocReq->wsmCapablePresent && (wsmMode == eHAL_SET))
            pStaDs->wsmEnabled = eANI_BOOLEAN_TRUE;

    }

    // Re/Assoc Response frame to requesting STA
    pStaDs->mlmStaContext.subType = subType;

    if (pAssocReq->propIEinfo.aniIndicator)
        pStaDs->aniPeer = 1;

    // BTAMP: Storing the parsed assoc request in the psessionEntry array
    psessionEntry->parsedAssocReq[pStaDs->assocId] = pAssocReq;

    /* BTAMP: If STA context already exist (ie. updateContext = 1)  
     * for this STA, then we should delete the old one, and add 
     * the new STA. This is taken care of in the limDelSta() routine.   
     *
     * Prior to BTAMP, we were setting this flag so that when
     * PE receives SME_ASSOC_CNF, and if this flag is set, then 
     * PE shall delete the old station and then add. But now in
     * BTAMP, we're directly adding station before waiting for
     * SME_ASSOC_CNF, so we can do this now.  
     */
    if (!updateContext)
    {
        pStaDs->mlmStaContext.updateContext = 0;

        // BTAMP: Add STA context at HW - issue WDA_ADD_STA_REQ to HAL
        if (limAddSta(pMac, pStaDs,psessionEntry) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGE, FL("could not Add STA with assocId=%d\n"), pStaDs->assocId);
            limRejectAssociation( pMac, pStaDs->staAddr, pStaDs->mlmStaContext.subType,
                                  true, pStaDs->mlmStaContext.authType, pStaDs->assocId, true,
                                  (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

            /*return it from here rather than goto error statement.This is done as the memory is getting free twice*/
            return;
            //goto error;
        }
    }
    else
    {
        pStaDs->mlmStaContext.updateContext = 1;

#ifdef WLAN_SOFTAP_FEATURE
        mlmPrevState = pStaDs->mlmStaContext.mlmState;

        /* As per the HAL/FW needs the reassoc req need not be calling limDelSta */
        if(subType != LIM_REASSOC)
        {
            //we need to set the mlmState here in order differentiate in limDelSta.
            pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE;
            if(limDelSta(pMac, pStaDs, true, psessionEntry) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGE, FL("could not DEL STA with assocId=%d staId %d\n"), pStaDs->assocId, pStaDs->staIndex);
                limRejectAssociation( pMac, pStaDs->staAddr, pStaDs->mlmStaContext.subType, true, pStaDs->mlmStaContext.authType,
                                      pStaDs->assocId, true,(tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);

                //Restoring the state back.
                pStaDs->mlmStaContext.mlmState = mlmPrevState;
                goto error;
            }
        }
        else
        {
            /* mlmState is changed in limAddSta context */
            /* use the same AID, already allocated */
            if (limAddSta(pMac, pStaDs,psessionEntry) != eSIR_SUCCESS)
            {
                    limLog( pMac, LOGE, FL( "AP do not support UPASD REASSOC Failed\n" ));
                    limRejectAssociation( pMac, pStaDs->staAddr, pStaDs->mlmStaContext.subType, true, pStaDs->mlmStaContext.authType,
                                          pStaDs->assocId, true,(tSirResultCodes) eSIR_MAC_WME_REFUSED_STATUS, psessionEntry);

                    //Restoring the state back.
                    pStaDs->mlmStaContext.mlmState = mlmPrevState;
                    goto error;
            }

        }
#endif

    }

    return;

error:
    if (pAssocReq != NULL)
    {
        if ( pAssocReq->assocReqFrame ) 
        {
            palFreeMemory(pMac->hHdd, pAssocReq->assocReqFrame);
            pAssocReq->assocReqFrame = NULL;
        }

        if (palFreeMemory(pMac->hHdd, pAssocReq) != eHAL_STATUS_SUCCESS) 
        {
            limLog(pMac, LOGP, FL("PalFree Memory failed \n"));
            return;
        }
    }

    if(pStaDs!= NULL)
        psessionEntry->parsedAssocReq[pStaDs->assocId] = NULL;
    return;

} /*** end limProcessAssocReqFrame() ***/



/**---------------------------------------------------------------
\fn     limSendMlmAssocInd
\brief  This function sends either LIM_MLM_ASSOC_IND  
\       or LIM_MLM_REASSOC_IND to SME.
\
\param  pMac
\param  *pStaDs - Station DPH hash entry
\param  psessionEntry - PE session entry
\return None

 * ?????? How do I get 
 *  - subtype   =====> psessionEntry->parsedAssocReq.reassocRequest
 *  - aid       =====> pStaDs->assocId
 *  - pHdr->sa  =====> pStaDs->staAddr
 *  - authType
 *  - pHdr->seqControl  =====> no longer needed
 *  - pStaDs
------------------------------------------------------------------*/
void limSendMlmAssocInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry)
{
    tpLimMlmAssocInd        pMlmAssocInd = NULL;
    tpLimMlmReassocInd      pMlmReassocInd;
    tpSirAssocReq           pAssocReq;
    tANI_U16                temp;
    tANI_U32                phyMode;
    tANI_U8                 subType;
#ifdef WLAN_SOFTAP_FEATURE
    tANI_U8                 *wpsIe = NULL;
#endif
    tANI_U32                tmp;
//    tANI_U16                statusCode;    
    tANI_U16                i, j=0;

    // Get a copy of the already parsed Assoc Request
    pAssocReq = (tpSirAssocReq) psessionEntry->parsedAssocReq[pStaDs->assocId];

    // Get the phyMode
    limGetPhyMode(pMac, &phyMode, psessionEntry);
 
    // Extract pre-auth context for the peer BTAMP-STA, if any.
 
    // Determiine if its Assoc or ReAssoc Request
    if (pAssocReq->reassocRequest == 1)
        subType = LIM_REASSOC;
    else 
        subType = LIM_ASSOC;
#ifdef WLAN_SOFTAP_FEATURE
    if (subType == LIM_ASSOC || subType == LIM_REASSOC)
#else
    if (subType == LIM_ASSOC )
#endif
    {
        temp  = sizeof(tLimMlmAssocInd);
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)        
        temp += pAssocReq->propIEinfo.numBss * sizeof(tSirNeighborBssInfo);
#endif        

        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmAssocInd, temp))
        {
            limReleaseAID(pMac, pStaDs->assocId);
            limLog(pMac, LOGP, FL("palAllocateMemory failed for pMlmAssocInd\n"));
            return;
        }
        palZeroMemory( pMac->hHdd, pMlmAssocInd, temp);

        palCopyMemory( pMac->hHdd,(tANI_U8 *)pMlmAssocInd->peerMacAddr,(tANI_U8 *)pStaDs->staAddr,sizeof(tSirMacAddr));
 
        pMlmAssocInd->aid    = pStaDs->assocId;
        palCopyMemory( pMac->hHdd, (tANI_U8 *)&pMlmAssocInd->ssId,(tANI_U8 *)&(pAssocReq->ssId), pAssocReq->ssId.length + 1);
        pMlmAssocInd->sessionId = psessionEntry->peSessionId;
        pMlmAssocInd->authType =  pStaDs->mlmStaContext.authType;
 
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
        // Note for BTAMP: no need to fill in pMlmAssocInd->seqNum
        pMlmAssocInd->wniIndicator = (tAniBool) pAssocReq->propIEinfo.aniIndicator;
        pMlmAssocInd->bpIndicator  = (tAniBool) pAssocReq->propIEinfo.bpIndicator;
        pMlmAssocInd->bpType       = (tSirBpIndicatorType) pAssocReq->propIEinfo.bpType;
        if (pAssocReq->extendedRatesPresent)
        {
            pMlmAssocInd->nwType = eSIR_11G_NW_TYPE;
            limSetStaHashErpMode(pMac, pStaDs->assocId, eHAL_SET);
        }
        else
        {
            if (phyMode == WNI_CFG_PHY_MODE_11A)
                pMlmAssocInd->nwType = eSIR_11A_NW_TYPE;
            else
            {
                pMlmAssocInd->nwType = eSIR_11B_NW_TYPE;
                limSetStaHashErpMode(pMac, pStaDs->assocId, eHAL_CLEAR);
            }
        }
        pMlmAssocInd->assocType = (tSirAssocType)pAssocReq->propIEinfo.assocType;
        pMlmAssocInd->load.numStas = pMac->lim.gLimNumOfCurrentSTAs;
        pMlmAssocInd->load.channelUtilization =(pMac->lim.gpLimMeasData) ? pMac->lim.gpLimMeasData->avgChannelUtilization : 0;
        pMlmAssocInd->numBss = (tANI_U32) pAssocReq->propIEinfo.numBss;
        if (pAssocReq->propIEinfo.numBss)
        {
            palCopyMemory( pMac->hHdd,(tANI_U8 *) pMlmAssocInd->neighborList,(tANI_U8 *)pAssocReq->propIEinfo.pBssList,
                           (sizeof(tSirNeighborBssInfo) * pAssocReq->propIEinfo.numBss));
        } 
#endif
        pMlmAssocInd->capabilityInfo = pAssocReq->capabilityInfo;

        // Fill in RSN IE information
        pMlmAssocInd->rsnIE.length = 0;
#ifdef WLAN_SOFTAP_FEATURE
        // if WPS IE is present, ignore RSN IE
        if (pAssocReq->addIEPresent && pAssocReq->addIE.length ) {
            wpsIe = limGetWscIEPtr(pMac, pAssocReq->addIE.addIEdata, pAssocReq->addIE.length);
        }
        if (pAssocReq->rsnPresent && (NULL == wpsIe))
#else
        if (pAssocReq->rsnPresent)
#endif
        {
            limLog(pMac, LOG2, FL("Assoc Req RSN IE len = %d\n"), pAssocReq->rsn.length);
            pMlmAssocInd->rsnIE.length = 2 + pAssocReq->rsn.length;
            pMlmAssocInd->rsnIE.rsnIEdata[0] = SIR_MAC_RSN_EID;
            pMlmAssocInd->rsnIE.rsnIEdata[1] = pAssocReq->rsn.length;
            palCopyMemory( pMac->hHdd, 
                           &pMlmAssocInd->rsnIE.rsnIEdata[2],
                           pAssocReq->rsn.info,
                           pAssocReq->rsn.length);
        }

        //FIXME: we need to have the cb information seprated between HT and Titan later. 
        if(pAssocReq->HTCaps.present)
            limGetHtCbAdminState(pMac, pAssocReq->HTCaps, &pMlmAssocInd->titanHtCaps);

        // Fill in 802.11h related info
        if (pAssocReq->powerCapabilityPresent && pAssocReq->supportedChannelsPresent)
        {
            pMlmAssocInd->spectrumMgtIndicator = eSIR_TRUE;
            pMlmAssocInd->powerCap.minTxPower = pAssocReq->powerCapability.minTxPower;
            pMlmAssocInd->powerCap.maxTxPower = pAssocReq->powerCapability.maxTxPower;
            limConvertSupportedChannels(pMac, pMlmAssocInd, pAssocReq);
        }
        else
            pMlmAssocInd->spectrumMgtIndicator = eSIR_FALSE;


#ifdef WLAN_SOFTAP_FEATURE
        /* This check is to avoid extra Sec IEs present incase of WPS */
        if (pAssocReq->wpaPresent && (NULL == wpsIe))
#else
        if ((pAssocReq->wpaPresent) && (pMlmAssocInd->rsnIE.length < SIR_MAC_MAX_IE_LENGTH))
#endif
        {
            if((pMlmAssocInd->rsnIE.length + pAssocReq->wpa.length) >= SIR_MAC_MAX_IE_LENGTH)
            {
                PELOGE(limLog(pMac, LOGE, FL("rsnIEdata index out of bounds %d\n"), pMlmAssocInd->rsnIE.length);)
                return;
            }
            pMlmAssocInd->rsnIE.rsnIEdata[pMlmAssocInd->rsnIE.length] = SIR_MAC_WPA_EID;
            pMlmAssocInd->rsnIE.rsnIEdata[pMlmAssocInd->rsnIE.length + 1] = pAssocReq->wpa.length;
            palCopyMemory( pMac->hHdd,
                           &pMlmAssocInd->rsnIE.rsnIEdata[pMlmAssocInd->rsnIE.length + 2],
                           pAssocReq->wpa.info,
                           pAssocReq->wpa.length);
            pMlmAssocInd->rsnIE.length += 2 + pAssocReq->wpa.length;
        }


       pMlmAssocInd->addIE.length = 0;
       if (pAssocReq->addIEPresent)
       {
            palCopyMemory( pMac->hHdd,
                           &pMlmAssocInd->addIE.addIEdata,
                           pAssocReq->addIE.addIEdata,
                           pAssocReq->addIE.length);

            pMlmAssocInd->addIE.length = pAssocReq->addIE.length;
       }

#ifdef WLAN_SOFTAP_FEATURE
        if(pAssocReq->wmeInfoPresent)
        {

            if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WME_ENABLED, &tmp) != eSIR_SUCCESS)
                 limLog(pMac, LOGP, FL("wlan_cfgGetInt failed for id %d\n"), WNI_CFG_WME_ENABLED );

            /* check whether AP is enabled with WMM */
            if(tmp)
            {
                pMlmAssocInd->WmmStaInfoPresent = 1;
            }
            else
            {
                pMlmAssocInd->WmmStaInfoPresent= 0;
            }
            /* Note: we are not rejecting association here because IOT will fail */

        }
#endif

        // Required for indicating the frames to upper layer
        pMlmAssocInd->assocReqLength = pAssocReq->assocReqFrameLength;
        pMlmAssocInd->assocReqPtr = pAssocReq->assocReqFrame;
        
        pMlmAssocInd->beaconPtr = psessionEntry->beacon;
        pMlmAssocInd->beaconLength = psessionEntry->bcnLen;

        limPostSmeMessage(pMac, LIM_MLM_ASSOC_IND, (tANI_U32 *) pMlmAssocInd);
        palFreeMemory( pMac->hHdd, pMlmAssocInd);
    }
    else
    {
        // If its of Reassociation Request, then post LIM_MLM_REASSOC_IND 
        temp  = sizeof(tLimMlmReassocInd);

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
        temp += pAssocReq->propIEinfo.numBss * sizeof(tSirNeighborBssInfo);
#endif
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmReassocInd, temp))
        {
            limLog(pMac, LOGP, FL("call to palAllocateMemory failed for pMlmReassocInd\n"));
            limReleaseAID(pMac, pStaDs->assocId);
            return;
        }
        palZeroMemory( pMac->hHdd, pMlmReassocInd, temp);

        palCopyMemory( pMac->hHdd,(tANI_U8 *) pMlmReassocInd->peerMacAddr, (tANI_U8 *)pStaDs->staAddr, sizeof(tSirMacAddr));
        palCopyMemory( pMac->hHdd,(tANI_U8 *) pMlmReassocInd->currentApAddr, (tANI_U8 *)&(pAssocReq->currentApAddr), sizeof(tSirMacAddr));
        pMlmReassocInd->aid = pStaDs->assocId;
        pMlmReassocInd->authType = pStaDs->mlmStaContext.authType;
        palCopyMemory( pMac->hHdd,(tANI_U8 *)&pMlmReassocInd->ssId, (tANI_U8 *)&(pAssocReq->ssId), pAssocReq->ssId.length + 1);

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
        // Note for BTAMP: no need to fill in pMlmAssocInd->seqNum
        pMlmReassocInd->wniIndicator = (tAniBool) pAssocReq->propIEinfo.aniIndicator;
        pMlmReassocInd->bpIndicator  = (tAniBool) pAssocReq->propIEinfo.bpIndicator;
        pMlmReassocInd->bpType       = (tSirBpIndicatorType) pAssocReq->propIEinfo.bpType;
        if (pAssocReq->extendedRatesPresent)
        {
            pMlmReassocInd->nwType = eSIR_11G_NW_TYPE;
            limSetStaHashErpMode(pMac, pStaDs->assocId, eHAL_SET);
        }
        else
        {
            if (phyMode == WNI_CFG_PHY_MODE_11A)
                pMlmReassocInd->nwType = eSIR_11A_NW_TYPE;
            else
            {
                pMlmReassocInd->nwType = eSIR_11B_NW_TYPE;
                limSetStaHashErpMode(pMac, pStaDs->assocId, eHAL_CLEAR);
            }
        }

        pMlmReassocInd->reassocType  = (tSirAssocType)pAssocReq->propIEinfo.assocType;
        pMlmReassocInd->load.numStas = pMac->lim.gLimNumOfCurrentSTAs;
        pMlmReassocInd->load.channelUtilization = (pMac->lim.gpLimMeasData) ?
                                                  pMac->lim.gpLimMeasData->avgChannelUtilization : 0;
        pMlmReassocInd->numBss = (tANI_U32) pAssocReq->propIEinfo.numBss;
        if (pAssocReq->propIEinfo.numBss)
        {
            palCopyMemory( pMac->hHdd, 
                           (tANI_U8 *) pMlmReassocInd->neighborList,
                           (tANI_U8 *) pAssocReq->propIEinfo.pBssList,
                           (sizeof(tSirNeighborBssInfo) * pAssocReq->propIEinfo.numBss));
        }
#endif
        if (pAssocReq->propIEinfo.aniIndicator)
            pStaDs->aniPeer = 1;

        pMlmReassocInd->capabilityInfo = pAssocReq->capabilityInfo;
        pMlmReassocInd->rsnIE.length = 0;

#ifdef WLAN_SOFTAP_FEATURE
        if (pAssocReq->addIEPresent && pAssocReq->addIE.length )
            wpsIe = limGetWscIEPtr(pMac, pAssocReq->addIE.addIEdata, pAssocReq->addIE.length);

        if (pAssocReq->rsnPresent && (NULL == wpsIe))
#else
        if (pAssocReq->rsnPresent)
#endif
        {
            limLog(pMac, LOG2, FL("Assoc Req: RSN IE length = %d\n"), pAssocReq->rsn.length);
            pMlmReassocInd->rsnIE.length = 2 + pAssocReq->rsn.length;
            pMlmReassocInd->rsnIE.rsnIEdata[0] = SIR_MAC_RSN_EID;
            pMlmReassocInd->rsnIE.rsnIEdata[1] = pAssocReq->rsn.length;
            palCopyMemory( pMac->hHdd, &pMlmReassocInd->rsnIE.rsnIEdata[2], pAssocReq->rsn.info, pAssocReq->rsn.length);
        }

        if(pAssocReq->HTCaps.present)
              limGetHtCbAdminState(pMac, pAssocReq->HTCaps,  &pMlmReassocInd->titanHtCaps );

        // 802.11h support
        if (pAssocReq->powerCapabilityPresent && pAssocReq->supportedChannelsPresent)
        {
            pMlmReassocInd->spectrumMgtIndicator = eSIR_TRUE;
            pMlmReassocInd->powerCap.minTxPower = pAssocReq->powerCapability.minTxPower;
            pMlmReassocInd->powerCap.maxTxPower = pAssocReq->powerCapability.maxTxPower;
            pMlmReassocInd->supportedChannels.numChnl = (tANI_U8)(pAssocReq->supportedChannels.length / 2);

            limLog(pMac, LOG1,
                FL("Sending Reassoc Ind: spectrum ON, minPwr %d, maxPwr %d, numChnl %d\n"),
                pMlmReassocInd->powerCap.minTxPower,
                pMlmReassocInd->powerCap.maxTxPower,
                pMlmReassocInd->supportedChannels.numChnl);

            for(i=0; i < pMlmReassocInd->supportedChannels.numChnl; i++)
            {
                pMlmReassocInd->supportedChannels.channelList[i] = pAssocReq->supportedChannels.supportedChannels[j];
                limLog(pMac, LOG1, FL("Sending ReassocInd: chn[%d] = %d \n"),
                       i, pMlmReassocInd->supportedChannels.channelList[i]);
                j+=2;
            }
        }
        else
            pMlmReassocInd->spectrumMgtIndicator = eSIR_FALSE;


#ifdef WLAN_SOFTAP_FEATURE
        /* This check is to avoid extra Sec IEs present incase of WPS */
        if (pAssocReq->wpaPresent && (NULL == wpsIe))
#else
        if (pAssocReq->wpaPresent)
#endif
        {
            limLog(pMac, LOG2, FL("Received WPA IE length in Assoc Req is %d\n"), pAssocReq->wpa.length);
            pMlmReassocInd->rsnIE.rsnIEdata[pMlmReassocInd->rsnIE.length] = SIR_MAC_WPA_EID;
            pMlmReassocInd->rsnIE.rsnIEdata[pMlmReassocInd->rsnIE.length + 1] = pAssocReq->wpa.length;
            palCopyMemory( pMac->hHdd,
                           &pMlmReassocInd->rsnIE.rsnIEdata[pMlmReassocInd->rsnIE.length + 2],
                           pAssocReq->wpa.info,
                           pAssocReq->wpa.length);
            pMlmReassocInd->rsnIE.length += 2 + pAssocReq->wpa.length;
        }

       pMlmReassocInd->addIE.length = 0;
       if (pAssocReq->addIEPresent)
       {
            palCopyMemory( pMac->hHdd,
                           &pMlmReassocInd->addIE.addIEdata,
                           pAssocReq->addIE.addIEdata,
                           pAssocReq->addIE.length);

            pMlmReassocInd->addIE.length = pAssocReq->addIE.length;
       }

#ifdef WLAN_SOFTAP_FEATURE
        if(pAssocReq->wmeInfoPresent)
        {

            if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WME_ENABLED, &tmp) != eSIR_SUCCESS)
                 limLog(pMac, LOGP, FL("wlan_cfgGetInt failed for id %d\n"), WNI_CFG_WME_ENABLED );

            /* check whether AP is enabled with WMM */
            if(tmp)
            {
                pMlmReassocInd->WmmStaInfoPresent = 1;
            }
            else
            {
                pMlmReassocInd->WmmStaInfoPresent = 0;
            }
            /* Note: we are not rejecting Re-association here because IOT will fail */

        }
#endif

        // Required for indicating the frames to upper layer
        pMlmReassocInd->assocReqLength = pAssocReq->assocReqFrameLength;
        pMlmReassocInd->assocReqPtr = pAssocReq->assocReqFrame;

        pMlmReassocInd->beaconPtr = psessionEntry->beacon;
        pMlmReassocInd->beaconLength = psessionEntry->bcnLen;

        limPostSmeMessage(pMac, LIM_MLM_REASSOC_IND, (tANI_U32 *) pMlmReassocInd);
        palFreeMemory( pMac->hHdd, pMlmReassocInd);
    }

    return;

} /*** end limSendMlmAssocInd() ***/
