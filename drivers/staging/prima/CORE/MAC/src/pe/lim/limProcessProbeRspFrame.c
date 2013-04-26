/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limProcessProbeRspFrame.cc contains the code
 * for processing Probe Response Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wniApi.h"
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "schApi.h"
#include "utilsApi.h"
#include "limApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"
#include "limSendMessages.h"

#include "parserApi.h"

tSirRetStatus
limValidateIEInformationInProbeRspFrame (tANI_U8 *pRxPacketInfo)
{
   tSirRetStatus       status = eSIR_SUCCESS;

   if (WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) < (SIR_MAC_B_PR_SSID_OFFSET + SIR_MAC_MIN_IE_LEN))
   {
      status = eSIR_FAILURE;
   }

   return status;
}

/**
 * limProcessProbeRspFrame
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Probe Response frame reception.
 *
 *LOGIC:
 * This function processes received Probe Response frame.
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * 1. Frames with out-of-order IEs are dropped.
 * 2. In case of IBSS, join 'success' makes MLM state machine
 *    transition into 'BSS started' state. This may have to change
 *    depending on supporting what kinda Authentication in IBSS.
 *
 * @param pMac   Pointer to Global MAC structure
 * @param  *pRxPacketInfo  A pointer to Buffer descriptor + associated PDUs
 * @return None
 */


void
limProcessProbeRspFrame(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo,tpPESession psessionEntry)
{
    tANI_U8                 *pBody;
    tANI_U32                frameLen = 0;
    tSirMacAddr             currentBssId;
    tpSirMacMgmtHdr         pHdr;
    tSirProbeRespBeacon    *pProbeRsp;
    tANI_U8 qosEnabled =    false;
    tANI_U8 wmeEnabled =    false;

    if(eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd, 
                                                (void **)&pProbeRsp, sizeof(tSirProbeRespBeacon)))
    {
        limLog(pMac, LOGE, FL("Unable to PAL allocate memory in limProcessProbeRspFrame\n") );
        return;
    }

    pProbeRsp->ssId.length              = 0;
    pProbeRsp->wpa.length               = 0;
    pProbeRsp->propIEinfo.apName.length = 0;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);


   PELOG2(limLog(pMac, LOG2,
             FL("Received Probe Response frame with length=%d from "),
             WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);)

   if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
   {
       palFreeMemory(pMac->hHdd, pProbeRsp);    
       return;
   }
   // Validate IE information before processing Probe Response Frame
   if (limValidateIEInformationInProbeRspFrame(pRxPacketInfo) != eSIR_SUCCESS)
   {
       PELOG1(limLog(pMac, LOG1,
                 FL("Parse error ProbeResponse, length=%d"), frameLen);)
       palFreeMemory(pMac->hHdd, pProbeRsp);
       return;
   }

    /**
     * Expect Probe Response only when
     * 1. STA is in scan mode waiting for Beacon/Probe response or
     * 2. STA is waiting for Beacon/Probe Response to announce
     *    join success or
     * 3. STA is in IBSS mode in BSS started state or
     * 4. STA/AP is in learn mode
     * 5. STA in link established state. In this state, the probe response is
     *     expected for two scenarios:
     *     -- As part of heart beat mechanism, probe req is sent out
     *     -- If QoS Info IE in beacon has a different count for EDCA Params,
     *         and EDCA IE is not present in beacon,
     *         then probe req is sent out to get the EDCA params.
     *
     * Ignore Probe Response frame in all other states
     */
        // TO SUPPORT BT-AMP
    if (((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||   //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) ||     //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE) ||            //mlm state check should be global - 18th oct 
        (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE) ||
        (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) )||
        ((GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_STA_IN_IBSS_ROLE) &&
         (psessionEntry->limMlmState == eLIM_MLM_BSS_STARTED_STATE)))
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp) == eSIR_FAILURE ||
            !pProbeRsp->ssidPresent) // Enforce Mandatory IEs
        {
            PELOG1(limLog(pMac, LOG1,
               FL("Parse error ProbeResponse, length=%d"),
               frameLen);)
            palFreeMemory(pMac->hHdd, pProbeRsp);
            return;
        }
                                                                            //To Support BT-AMP                    
        if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||    //mlm state check should be global - 18th oct
            (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE))
            limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo, 
               ((pMac->lim.gLimHalScanState == eLIM_HAL_SCANNING_STATE) ? eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE), eANI_BOOLEAN_TRUE);
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)           //mlm state check should be global - 18th oct
        {
        }
        else if (psessionEntry->limMlmState ==
                                     eLIM_MLM_WT_JOIN_BEACON_STATE)
        {
            if( psessionEntry->beacon != NULL )//Either Beacon/probe response is required. Hence store it in same buffer.
            {
                palFreeMemory(pMac->hHdd, psessionEntry->beacon);
                psessionEntry->beacon = NULL;
            }
            psessionEntry->bcnLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
            if ((palAllocateMemory(pMac->hHdd, (void**)&psessionEntry->beacon,
                                   psessionEntry->bcnLen))
                != eHAL_STATUS_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE,
                              FL("Unable to allocate memory to store beacon"));)
            }
            else
            {
                //Store the Beacon/ProbeRsp. This is sent to csr/hdd in join cnf response. 
                palCopyMemory(pMac->hHdd, psessionEntry->beacon,
                              WDA_GET_RX_MPDU_DATA(pRxPacketInfo),
                              psessionEntry->bcnLen);
            }

            // STA in WT_JOIN_BEACON_STATE
            limCheckAndAnnounceJoinSuccess(pMac, pProbeRsp, pHdr, psessionEntry);
        }
        else if(psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)
        {
            tpDphHashNode pStaDs = NULL;
            /**
             * Check if this Probe Response is for
            * our Probe Request sent upon reaching
            * heart beat threshold
            */
            #if 0
            if (wlan_cfgGetStr(pMac,
                          WNI_CFG_BSSID,
                          currentBssId,
                          &cfg) != eSIR_SUCCESS)
            {
                /// Could not get BSSID from CFG. Log error.
                limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
            }
            #endif //TO SUPPORT BT-AMP
            sirCopyMacAddr(currentBssId,psessionEntry->bssId);

            if ( !palEqualMemory( pMac->hHdd,currentBssId, pHdr->bssId, sizeof(tSirMacAddr)) )
            {
                palFreeMemory(pMac->hHdd, pProbeRsp);    
                return;
            }

            if (!LIM_IS_CONNECTION_ACTIVE(psessionEntry))
            {
                limLog(pMac, LOGW,
                    FL("Received Probe Resp from AP. So it is alive!!\n"));

                if (pProbeRsp->HTInfo.present)
                    limReceivedHBHandler(pMac, (tANI_U8)pProbeRsp->HTInfo.primaryChannel, psessionEntry);
                else
                    limReceivedHBHandler(pMac, (tANI_U8)pProbeRsp->channelNumber, psessionEntry);
            }

            
            if (psessionEntry->limSystemRole == eLIM_STA_ROLE)
            {
                if (pProbeRsp->channelSwitchPresent ||
                    pProbeRsp->propIEinfo.propChannelSwitchPresent)
                {
                    limUpdateChannelSwitch(pMac, pProbeRsp, psessionEntry);
                }
                else if (psessionEntry->gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING)
                {
                    limCancelDot11hChannelSwitch(pMac, psessionEntry);
                }
            }
        
            
            /**
            * Now Process EDCA Parameters, if EDCAParamSet count is different.
            *     -- While processing beacons in link established state if it is determined that
            *         QoS Info IE has a different count for EDCA Params,
            *         and EDCA IE is not present in beacon,
            *         then probe req is sent out to get the EDCA params.
            */

            pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

            limGetQosMode(psessionEntry, &qosEnabled);
            limGetWmeMode(psessionEntry, &wmeEnabled);
           PELOG2(limLog(pMac, LOG2,
                    FL("wmeEdcaPresent: %d wmeEnabled: %d, edcaPresent: %d, qosEnabled: %d,  edcaParams.qosInfo.count: %d schObject.gLimEdcaParamSetCount: %d\n"),
                          pProbeRsp->wmeEdcaPresent, wmeEnabled, pProbeRsp->edcaPresent, qosEnabled,
                          pProbeRsp->edcaParams.qosInfo.count, psessionEntry->gLimEdcaParamSetCount);)
            if (((pProbeRsp->wmeEdcaPresent && wmeEnabled) ||
                (pProbeRsp->edcaPresent && qosEnabled)) &&
                (pProbeRsp->edcaParams.qosInfo.count != psessionEntry->gLimEdcaParamSetCount))
            {
                if (schBeaconEdcaProcess(pMac, &pProbeRsp->edcaParams, psessionEntry) != eSIR_SUCCESS)
                    PELOGE(limLog(pMac, LOGE, FL("EDCA parameter processing error\n"));)
                else if (pStaDs != NULL)
                {
                    // If needed, downgrade the EDCA parameters
                    limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry); 

                    if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE)
                        limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
                    else
                        limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
                }
                else
                    PELOGE(limLog(pMac, LOGE, FL("Self Entry missing in Hash Table\n"));)

            }

           if (psessionEntry->fWaitForProbeRsp == true)
           {
               limLog(pMac, LOGW, FL("Checking probe response for capability change\n") );
               limDetectChangeInApCapabilities(pMac, pProbeRsp, psessionEntry);
           }
        }
        else if ((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) &&
                 (psessionEntry->limMlmState == eLIM_MLM_BSS_STARTED_STATE))
                limHandleIBSScoalescing(pMac, pProbeRsp, pRxPacketInfo,psessionEntry);
    } // if ((pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) || ...

    palFreeMemory(pMac->hHdd, pProbeRsp);
    // Ignore Probe Response frame in all other states
    return;
} /*** end limProcessProbeRspFrame() ***/


void
limProcessProbeRspFrameNoSession(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo)
{
    tANI_U8                 *pBody;
    tANI_U32                frameLen = 0;
    tpSirMacMgmtHdr         pHdr;
    tSirProbeRespBeacon    *pProbeRsp;

    if(eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd, 
                                                (void **)&pProbeRsp, sizeof(tSirProbeRespBeacon)))
    {
        limLog(pMac, LOGE, FL("Unable to PAL allocate memory in limProcessProbeRspFrameNoSession\n") );
        return;
    }

    pProbeRsp->ssId.length              = 0;
    pProbeRsp->wpa.length               = 0;
    pProbeRsp->propIEinfo.apName.length = 0;


    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);


    limLog(pMac, LOG2,
             FL("Received Probe Response frame with length=%d from "),
             WDA_GET_RX_MPDU_LEN(pRxPacketInfo));
    limPrintMacAddr(pMac, pHdr->sa, LOG2);

    if (limDeactivateMinChannelTimerDuringScan(pMac) != eSIR_SUCCESS)
    {
        palFreeMemory(pMac->hHdd, pProbeRsp);
        return;
    }
     // Validate IE information before processing Probe Response Frame
    if (limValidateIEInformationInProbeRspFrame(pRxPacketInfo) != eSIR_SUCCESS)
    {
       PELOG1(limLog(pMac, LOG1,FL("Parse error ProbeResponse, length=%d"),
              frameLen);)
       palFreeMemory(pMac->hHdd, pProbeRsp);
       return;
    }
    /*  Since there is no psessionEntry, PE cannot be in the following states:
     *   - eLIM_MLM_WT_JOIN_BEACON_STATE
     *   - eLIM_MLM_LINK_ESTABLISHED_STATE
     *   - eLIM_MLM_BSS_STARTED_STATE
     *  Hence, expect Probe Response only when
     *   1. STA is in scan mode waiting for Beacon/Probe response 
     *  
     *  Ignore Probe Response frame in all other states
     */
    if( (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
        (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE)  ||     //mlm state check should be global - 18th oct
        (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE) )
    {
        frameLen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

        // Get pointer to Probe Response frame body
        pBody = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

        if (sirConvertProbeFrame2Struct(pMac, pBody, frameLen, pProbeRsp) == eSIR_FAILURE)
        {
            limLog(pMac, LOG1, FL("Parse error ProbeResponse, length=%d\n"), frameLen);
            palFreeMemory(pMac->hHdd, pProbeRsp);
            return;
        }

        if( (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE) ||
             (pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE) )
            limCheckAndAddBssDescription(pMac, pProbeRsp, pRxPacketInfo, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_TRUE);
        else if (pMac->lim.gLimMlmState == eLIM_MLM_LEARN_STATE)
        {
        }
    } 
    palFreeMemory(pMac->hHdd, pProbeRsp);
    return;
} /*** end limProcessProbeRspFrameNew() ***/
