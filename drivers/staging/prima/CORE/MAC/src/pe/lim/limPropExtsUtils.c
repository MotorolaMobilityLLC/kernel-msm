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
 * This file limPropExtsUtils.cc contains the utility functions
 * to populate, parse proprietary extensions required to
 * support ANI feature set.
 *
 * Author:        Chandra Modumudi
 * Date:          11/27/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "aniGlobal.h"
#ifdef ANI_PRODUCT_TYPE_AP
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#endif
#include "sirCommon.h"
#include "sirDebug.h"
#include "utilsApi.h"
#include "cfgApi.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halCommonApi.h"
#endif
#include "limApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"
#include "limTrace.h"
#include "limSession.h"
#define LIM_GET_NOISE_MAX_TRY 5
#if (defined(ANI_PRODUCT_TYPE_AP) || defined(ANI_PRODUCT_TYPE_AP_SDK))
/**
 * limGetCurrentLearnChannel()
 *
 *FUNCTION:
 * This function is called in various places to get current channel
 * number being 'learned'.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return Channel number
 */
tANI_U8
limGetCurrentLearnChannel(tpAniSirGlobal pMac)
{
    tANI_U8 *pChanNum = pMac->lim.gpLimMeasReq->channelList.channelNumber;
    return (*(pChanNum + pMac->lim.gLimMeasParams.nextLearnChannelId));
} /*** end limGetCurrentLearnChannel() ***/
#endif //#if (defined(ANI_PRODUCT_TYPE_AP) || defined(ANI_PRODUCT_TYPE_AP_SDK))
/**
 * limExtractApCapability()
 *
 *FUNCTION:
 * This function is called to extract AP's HCF/WME/WSM capability
 * from the IEs received from it in Beacon/Probe Response frames
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param   pMac      Pointer to Global MAC structure
 * @param   pIE       Pointer to starting IE in Beacon/Probe Response
 * @param   ieLen     Length of all IEs combined
 * @param   qosCap    Bits are set according to capabilities
 * @return  0 - If AP does not assert HCF capability & 1 - otherwise
 */
void
limExtractApCapability(tpAniSirGlobal pMac, tANI_U8 *pIE, tANI_U16 ieLen,
                       tANI_U8 *qosCap, tANI_U16 *propCap, tANI_U8 *uapsd, 
                       tPowerdBm *localConstraint
                       )
{
    tSirProbeRespBeacon beaconStruct;
#if !defined WLAN_FEATURE_VOWIFI
    tANI_U32            localPowerConstraints = 0;
#endif
    palZeroMemory( pMac->hHdd, (tANI_U8 *) &beaconStruct, sizeof(beaconStruct));
    *qosCap = 0;
    *propCap = 0;
    *uapsd = 0;
    PELOG3(limLog( pMac, LOG3,
        FL("In limExtractApCapability: The IE's being received are:\n"));
    sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOG3, pIE, ieLen );)
    if (sirParseBeaconIE(pMac, &beaconStruct, pIE, (tANI_U32)ieLen) == eSIR_SUCCESS)
    {
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
        if (beaconStruct.propIEinfo.hcfEnabled)
            LIM_BSS_CAPS_SET(HCF, *qosCap);
#endif
        if (beaconStruct.wmeInfoPresent || beaconStruct.wmeEdcaPresent)
            LIM_BSS_CAPS_SET(WME, *qosCap);
        if (LIM_BSS_CAPS_GET(WME, *qosCap) && beaconStruct.wsmCapablePresent)
            LIM_BSS_CAPS_SET(WSM, *qosCap);
        if (beaconStruct.propIEinfo.aniIndicator &&
            beaconStruct.propIEinfo.capabilityPresent)
            *propCap = beaconStruct.propIEinfo.capability;
        if (beaconStruct.HTCaps.present)
            pMac->lim.htCapabilityPresentInBeacon = 1;
        else
            pMac->lim.htCapabilityPresentInBeacon = 0;

#ifdef WLAN_FEATURE_11AC
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO_MED,
            "***beacon.VHTCaps.present*****=%d\n",beaconStruct.VHTCaps.present);

        if ( beaconStruct.VHTCaps.present && beaconStruct.VHTOperation.present)
        {
            pMac->lim.vhtCapabilityPresentInBeacon = 1;
            pMac->lim.apCenterChan = beaconStruct.VHTOperation.chanCenterFreqSeg1;
            pMac->lim.apChanWidth = beaconStruct.VHTOperation.chanWidth;
        }
        else
        {
            pMac->lim.vhtCapabilityPresentInBeacon = 0;
        }
#endif
        // Extract the UAPSD flag from WMM Parameter element
        if (beaconStruct.wmeEdcaPresent)
            *uapsd = beaconStruct.edcaParams.qosInfo.uapsd;
#if defined FEATURE_WLAN_CCX
        /* If there is Power Constraint Element specifically,
         * adapt to it. Hence there is else condition check
         * for this if statement.
         */
        if ( beaconStruct.ccxTxPwr.present)
        {
            *localConstraint = beaconStruct.ccxTxPwr.power_limit;
        }
#endif
        if (beaconStruct.powerConstraintPresent)
#if 0
        //Remove this check. This function is expected to return localPowerConsraints
        //and it should just do that. Check for 11h enabled or not can be done at the caller
#if defined WLAN_FEATURE_VOWIFI
          && ( pMac->lim.gLim11hEnable
           || pMac->rrm.rrmPEContext.rrmEnable
#endif
#endif
        {
#if defined WLAN_FEATURE_VOWIFI 
           *localConstraint -= beaconStruct.localPowerConstraint.localPowerConstraints;
#else
           localPowerConstraints = (tANI_U32)beaconStruct.localPowerConstraint.localPowerConstraints;
#endif
        }
#if !defined WLAN_FEATURE_VOWIFI
        if (cfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, localPowerConstraints) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Could not update local power constraint to cfg.\n"));
        }
#endif
    }
    return;
} /****** end limExtractApCapability() ******/

#if (defined(ANI_PRODUCT_TYPE_AP) || defined(ANI_PRODUCT_TYPE_AP_SDK))
/**
 * limQuietBss()
 *
 *FUNCTION:
 * This function is called to quiet the BSS
 * while entering into Learn mode
 *
 *LOGIC:
 * Data frame to self with duration value passed is
 * transmitted
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * If all associated STAs are 11h compliant, Quiet IE
 * need to be sent in Beacon/Probe Response frames.
 *
 * @param   pMac      Pointer to Global MAC structure
 * @param   duration  Specifies quiet duration in millisec
 * @return  None
 */
void
limQuietBss(tpAniSirGlobal pMac, tANI_U32 duration)
{
    // Temporarily not quieting BSS
    (void) pMac; (void) duration;
    return;
} /****** end limQuietBss() ******/

/**
 * limIsMatrixNodePresent()
 *
 *FUNCTION:
 * This function is called to determine if measurements are
 * already made on the current learn channel
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param   pMac  - Pointer to Global MAC structure
 * @return  pNode - Pointer to Matrix node if found. Else NULL
 */
static tpLimMeasMatrixNode
limIsMatrixNodePresent(tpAniSirGlobal pMac)
{
    tANI_U8   i, chanNum = limGetCurrentLearnChannel(pMac);
    tpLimMeasMatrixNode pNode = pMac->lim.gpLimMeasData->pMeasMatrixInfo;
    if (!pNode)
        return NULL;
    for (i = 0; i < pMac->lim.gpLimMeasReq->channelList.numChannels; i++)
    {
        if (pNode->matrix.channelNumber == chanNum)
        {
            return pNode;
        }
        else
        {
            if (pNode->next)
                pNode = pNode->next;
            else
                break;
        }
    }
    return NULL;
} /****** end limIsMatrixNodePresent() ******/

/**
 * limGetMatrixNode()
 *
 *FUNCTION:
 * This function is called to get a metrics node associated
 * with the current measurement channel. If no node is found
 * for this channel, one is added at the beginning of the list.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param   pMac      Pointer to Global MAC structure
 * @return  None
 */
static tpLimMeasMatrixNode
limGetMatrixNode(tpAniSirGlobal pMac)
{
    tpLimMeasMatrixNode       pNewMatrix;
    eHalStatus          status;
    pNewMatrix = limIsMatrixNodePresent(pMac);
    if (!pNewMatrix)
    {
        // Making first time measurements on this channel.
        // Add matrix node for this at the front.
        status = palAllocateMemory( pMac->hHdd, (void **)&pNewMatrix, sizeof(*pNewMatrix));        
        if (status != eHAL_STATUS_SUCCESS)
        {
            /// Could not allocate buffer for new measMatrix Node
            // Log error
            limLog(pMac, LOGP,
               FL("palAllocateMemory failed for new measMatrix Node\n"));
            return NULL;
        }
        status = palZeroMemory( pMac->hHdd, (void *)pNewMatrix, sizeof(*pNewMatrix));
        if (status != eHAL_STATUS_SUCCESS)
        {
            /// Could not allocate buffer for new measMatrix Node
            // Log error
            limLog(pMac, LOGP,
               FL("palZeroMemory failed for new measMatrix Node\n"));
            return NULL;
        }
        
        pNewMatrix->matrix.channelNumber =
                                   limGetCurrentLearnChannel(pMac);
        pNewMatrix->avgRssi              = 0;
       PELOG3(limLog(pMac, LOG3, FL("Adding new Matrix info:channel#=%d\n"),
               pNewMatrix->matrix.channelNumber);)
        pNewMatrix->next = pMac->lim.gpLimMeasData->pMeasMatrixInfo;
        pMac->lim.gpLimMeasData->pMeasMatrixInfo = pNewMatrix;
        pMac->lim.gpLimMeasData->numMatrixNodes++;
    }
    return pNewMatrix;
} /****** end limGetMatrixNode() ******/

/**
 * limComputeAvg()
 *
 *FUNCTION:
 * This function is called to compute exponential average
 * of RSSI, channel utilization etc.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param   pMac      Pointer to Global MAC structure
 * @param   oldVal    Previous averaged value
 * @param   newVal    New averaged value
 * @return  None
 */
tANI_U32
limComputeAvg(tpAniSirGlobal pMac, tANI_U32 oldVal, tANI_U32 newVal)
{
    return (halExpAvg(newVal,
                      oldVal,
                      pMac->lim.gLimMeasParams.rssiAlpha));
} /****** end limComputeAvg() ******/

/**
 * limCollectRSSI()
 *
 *FUNCTION:
 * This function is called to collect RSSI measurements on
 * the current learn channel
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param   pMac      Pointer to Global MAC structure
 * @return  None
 */
void
limCollectRSSI(tpAniSirGlobal pMac)
{
    tpLimMeasMatrixNode  pNewMatrix = limGetMatrixNode(pMac);
    tANI_U32 i, noise;
    for (i = 0; i < LIM_GET_NOISE_MAX_TRY; i++)
        if ((noise = halGetNoise(pMac)) != HAL_NOISE_INVALID)
        {
            pNewMatrix->avgRssi = limComputeAvg(pMac,
                                                pNewMatrix->avgRssi,
                                                noise);
            break;
        }
} /****** end limCollectRSSI() ******/

/**----------------------------------------------------------------------------
\fn        limGetNeighbourBssNode
\brief    returns neighbour bss node if it is already present in the list.
\param pMac
\param bssid - Bssid of new beacon or data packet.
\param pSsId - Pointer to SSID of new packet.
\param nwType - 11b/g/a
\param chanId - Channel in which we received the packet.
\return tpLimNeighborBssWdsNode or NULL
-------------------------------------------------------------------------------*/
static tpLimNeighborBssWdsNode
limGetNeighbourBssNode(tpAniSirGlobal pMac, tANI_U8 *bssId, tANI_U8 chanId,
                                            tSirNwType nwType, tpAniSSID pSsId, tANI_U8 type)
{
    tpLimNeighborBssWdsNode pNode = pMac->lim.gpLimMeasData->pNeighborWdsInfo;
    while (pNode)
    {
        //Do we need to check for ssId also ?
        if (palEqualMemory(pMac->hHdd, pNode->info.neighborBssInfo.bssId,
                    bssId, sizeof(tSirMacAddr)) &&
                (pNode->info.neighborBssInfo.channelId == chanId))
        {
#if 0
/** Commented out assuming that comparing bssId and ChanId would be enough to
uniquely identify a particular BSS, Uncomment if strict checking is needed,
eventhough not possible if packet type is DATA. */
            if (type == eSIR_MAC_MGMT_FRAME)
            {
                /** Beacon Frame */
                if (palEqualMemory(pMac->hHdd, &pNode->info.neighborBssInfo.ssId,
                            pSsId, pSsId->length+1) &&
                            (pNode->info.neighborBssInfo.nwType == nwType)) 
                {
                    return pNode;
                }
            }
            else
#endif
                return pNode;
        }
        if (!pNode->next)
            break;
        else
            pNode = pNode->next;
    }
    return NULL;
}

/**
 * limCollectMeasurementData()
 *
 *FUNCTION:
 * This function is called upon receiving Beacon/Probe Response
 * or Data frames to collect measurement related data.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac     - Pointer to Global MAC structure
 * @param  pRxPacketInfo      - Pointer to received BD+payload
 * @param  pBeacon  - Pointer to parsed BSS info
 * @return None
 */
void
limCollectMeasurementData(tpAniSirGlobal pMac,
                          tANI_U32 *pRxPacketInfo, tpSchBeaconStruct pBeacon)
{
    tANI_U16                allocLen=0, len=0, realLen=0, ieLen=0;
    tANI_U32                     i;
    tANI_U8                 found = eANI_BOOLEAN_TRUE;
    tSirMacFrameCtl         fc ;
    tSirMacAddr             bssIdRcv;
    tSirNwType              nwType;
    tAniSSID                ssId;
    tANI_U8                 chanId = 0;
    tpLimNeighborBssWdsNode pNode = NULL;
    tpLimMeasMatrixNode     pNewMatrix;
    eHalStatus status;
    tpSirMacMgmtHdr       pHdr;
   PELOG3(limLog(pMac, LOG3, FL("Collecting measurement data for RadioId %d\n"),
           pMac->sys.gSirRadioId);)
    tANI_U32 ignore = 0;
    limGetBssidFromBD(pMac, (tpHalBufDesc) pRxPacketInfo, bssIdRcv, &ignore);
    if (palEqualMemory( pMac->hHdd, bssIdRcv, pMac->lim.gLimBssid, sizeof(tSirMacAddr)))
    {
        // Packet received from our own BSS, dont take measurement.
        return;
    }
    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    fc = pHdr->fc;
    if (fc.type == SIR_MAC_DATA_FRAME)
    {
        PELOG2(limLog(pMac, LOG2, FL("Received DATA packet\n"));)
        ssId.length = 0;
        ieLen = 0;
    }
    else
    {
        /** Probe response or beaccon packet */
        palCopyMemory(pMac->hHdd, (void *)&ssId, (void *) &pBeacon->ssId,
                                             sizeof(ssId));
        chanId = limGetChannelFromBeacon(pMac, pBeacon);
        ieLen = pBeacon->wpa.length + pBeacon->propIEinfo.wdsLength;
    }
    if (chanId == 0)
    {
      /* If the channel Id is not retrieved from Beacon, extract the channel from BD */
      /* Unmapped the channel.This We have to do since we have done mapping in the hal to
         overcome  the limitation of RXBD of not able to accomodate the bigger channel number.*/
      chanId = WDA_GET_RX_CH(pRxPacketInfo);
      if(!( chanId = limUnmapChannel(chanId))) 
      {
           chanId = pMac->lim.gLimCurrentScanChannelId;
      }
    }
    /*
     * Now always returns nwType as 11G for data packets - FIXIT
     */
    nwType = limGetNwType(pMac, chanId, fc.type, pBeacon);
    pNewMatrix = limGetMatrixNode(pMac);
    /** LOGP would result in freeing all dynamicall allocated memories. So
      *  return from here if limGetMatrixNode returns NULL
      */
    if (!pNewMatrix)
        return;
    pNewMatrix->matrix.aggrRssi += WDA_GET_RX_RSSI_DB(pRxPacketInfo);
    pNewMatrix->matrix.totalPackets++;
    // Find if this neighbor is already 'learned'
    // If found, update its information.
    pNode = limGetNeighbourBssNode(pMac, bssIdRcv, chanId, nwType, &ssId, fc.type);
    if (!pNode)
    {
        realLen = sizeof(tSirNeighborBssWdsInfo);
        /** Newly discovered neighbor. Inform WSM of this
          * and add this BSS info at the beginning
          * Need to limit the number newly discovered BSS added
          * to the list.
          */
        len = LIM_MAX_BUF_SIZE - (sizeof(tSirSmeMeasurementInd) +
                (pMac->lim.gpLimMeasReq->channelList.numChannels *
                 sizeof(tSirMeasMatrixInfo)));
       PELOG2(limLog(pMac, LOG2, FL("Current BSS length %d, Real length %d\n"),
                pMac->lim.gpLimMeasData->totalBssSize, realLen);)
        /** Check if we have enough room for adding a new node.
          */
        if (pMac->lim.gpLimMeasData->totalBssSize + realLen < len)
        {
            pMac->lim.gpLimMeasData->numBssWds++;
            pMac->lim.gpLimMeasData->totalBssSize += realLen;
            PELOG2(limPrintMacAddr(pMac, bssIdRcv, LOG2);)
        }
        else
        {
            PELOG2(limLog(pMac, LOG2, FL("Dropping the measurement packets: No memory!\n"));)
            return;
    }
        /** Allocate max memory required even if the packet is of type DATA,
          * So that next time we receive a beacon, won't run out of memory to
          * update the information.
          */
        allocLen = sizeof(tLimNeighborBssWdsNode) + 4 + ieLen;
        status = palAllocateMemory( pMac->hHdd, (void **)&pNode, allocLen);
    if (status != eHAL_STATUS_SUCCESS)
    {
            limLog(pMac, LOGP, FL("palAllocateMemory failed for new NeighborBssWds Node\n"));
        return;
    }
        status = palZeroMemory(pMac->hHdd, pNode, allocLen);
        if (status != eHAL_STATUS_SUCCESS)
        {
            limLog(pMac, LOGP, FL("palAllocateMemory failed for new NeighborBssWds Node\n"));
            return;
        }
        pNode->next = pMac->lim.gpLimMeasData->pNeighborWdsInfo;
        pMac->lim.gpLimMeasData->pNeighborWdsInfo = pNode;
        found = eANI_BOOLEAN_FALSE;
    }
    pNode->info.neighborBssInfo.rssi = WDA_GET_RX_RSSI_DB(pRxPacketInfo);
    pNode->info.neighborBssInfo.aggrRssi += pNode->info.neighborBssInfo.rssi;
    if (fc.type == SIR_MAC_DATA_FRAME)
        pNode->info.neighborBssInfo.dataCount++;
    pNode->info.neighborBssInfo.totalPackets++;
    /** If node not found or previous learn was not from a beacon/probe rsp
      * then learn again.
      */
    if (!found || ((pNode->info.neighborBssInfo.ssId.length == 0) &&
                   (fc.type == SIR_MAC_MGMT_FRAME)))
    {
        palCopyMemory(pMac->hHdd, pNode->info.neighborBssInfo.bssId, bssIdRcv,
                  sizeof(tSirMacAddr));
        pNode->info.neighborBssInfo.channelId = chanId;
    if (fc.type == SIR_MAC_DATA_FRAME)
    {
        // Data frame received from other BSS.
        // Collect as much information as possible
            pNode->info.neighborBssInfo.wniIndicator = (tAniBool) 0;
        if (fc.toDS || fc.fromDS)
                pNode->info.neighborBssInfo.bssType = eSIR_INFRASTRUCTURE_MODE;
        else
                pNode->info.neighborBssInfo.bssType = eSIR_IBSS_MODE;
            pNode->info.neighborBssInfo.load.numStas = 0;
            pNode->info.neighborBssInfo.load.channelUtilization = 0;
            pNode->info.neighborBssInfo.ssId.length = 0;
            pNode->info.neighborBssInfo.apName.length = 0;
            pNode->info.neighborBssInfo.rsnIE.length = 0;
            pNode->info.wdsInfo.wdsLength = 0;
    }
    else
    {
        // This must be either Beacon frame or
        // Probe Response. Copy all relevant information.
            pNode->info.neighborBssInfo.wniIndicator = (tAniBool) pBeacon->propIEinfo.aniIndicator;
            pNode->info.neighborBssInfo.bssType = (tSirBssType) pBeacon->capabilityInfo.ibss;
            pNode->info.neighborBssInfo.load.numStas = pBeacon->propIEinfo.load.numStas;
            pNode->info.neighborBssInfo.load.channelUtilization = pBeacon->propIEinfo.load.channelUtilization;
            
            palCopyMemory( pMac->hHdd, (tANI_U8 *) &pNode->info.neighborBssInfo.ssId,
                                                    &pBeacon->ssId, pBeacon->ssId.length+1);
            pNode->info.neighborBssInfo.apName.length = pBeacon->propIEinfo.apName.length;
            palCopyMemory( pMac->hHdd, (tANI_U8 *) pNode->info.neighborBssInfo.apName.name,
                    pBeacon->propIEinfo.apName.name, pBeacon->propIEinfo.apName.length);
            pNode->info.neighborBssInfo.rsnIE.length = 0;
            // Add WPA2 information. Before that make sure that memory is available
            if (pBeacon->rsnPresent && (pBeacon->rsn.length < SIR_MAC_MAX_IE_LENGTH))
            {
                pNode->info.neighborBssInfo.rsnIE.length = 2 + pBeacon->rsn.length;
                pNode->info.neighborBssInfo.rsnIE.rsnIEdata[0] = SIR_MAC_RSN_EID;
                pNode->info.neighborBssInfo.rsnIE.rsnIEdata[1] = pBeacon->rsn.length;
                palCopyMemory( pMac->hHdd, (tANI_U8 *) &pNode->info.neighborBssInfo.rsnIE.rsnIEdata[2],
                        pBeacon->rsn.info, pBeacon->rsn.length);
               PELOG2(limLog(pMac, LOG2, FL("NeighborBss RSN IE, type=%x, length=%x\n"), 
                        pNode->info.neighborBssInfo.rsnIE.rsnIEdata[0],
                        pNode->info.neighborBssInfo.rsnIE.rsnIEdata[1]);)
            }
            // Add WPA information. Before that make sure that memory is available
            if (pBeacon->wpaPresent && ((pBeacon->rsn.length + pBeacon->wpa.length) < (SIR_MAC_MAX_IE_LENGTH-2)))
            {
                pNode->info.neighborBssInfo.rsnIE.rsnIEdata[pNode->info.neighborBssInfo.rsnIE.length] =
                    SIR_MAC_WPA_EID;
                pNode->info.neighborBssInfo.rsnIE.rsnIEdata[pNode->info.neighborBssInfo.rsnIE.length + 1] =
                    pBeacon->wpa.length;
                palCopyMemory( pMac->hHdd,
                        (tANI_U8 *) &pNode->info.neighborBssInfo.rsnIE.rsnIEdata[pNode->info.neighborBssInfo.rsnIE.length + 2],
                        pBeacon->wpa.info, pBeacon->wpa.length);
               PELOG2(limLog(pMac, LOG2, FL("NeighborBss WPA IE, type=%x, length=%x\n"),
                        pNode->info.neighborBssInfo.rsnIE.rsnIEdata[pNode->info.neighborBssInfo.rsnIE.length],
                        pNode->info.neighborBssInfo.rsnIE.rsnIEdata[pNode->info.neighborBssInfo.rsnIE.length + 1]);)
                pNode->info.neighborBssInfo.rsnIE.length += 2 + pBeacon->wpa.length;
            }
            pNode->info.wdsInfo.wdsLength = (tANI_U16) pBeacon->propIEinfo.wdsLength;
            palCopyMemory( pMac->hHdd, (tANI_U8 *) pNode->info.wdsInfo.wdsBytes,
                      pBeacon->propIEinfo.wdsData,
                      pBeacon->propIEinfo.wdsLength);
            pNode->info.neighborBssInfo.capabilityInfo = *((tANI_U16*)&pBeacon->capabilityInfo);
#if 0
            if (pBeacon->HTCaps.present)
                palCopyMemory( pMac->hHdd, (tANI_U8 *)&pNode->info.neighborBssInfo.HTCaps,
                        (tANI_U8 *)&pBeacon->HTCaps, HT_CAPABILITY_IE_SIZE);
        else
                pNode->info.neighborBssInfo.HTCaps.present = 0;
            if (pBeacon->HTInfo.present)
                palCopyMemory( pMac->hHdd, (tANI_U8 *)&pNode->info.neighborBssInfo.HTInfo,
                        (tANI_U8 *)&pBeacon->HTInfo, HT_INFO_IE_SIZE);
        else
                pNode->info.neighborBssInfo.HTInfo.present = 0;
#endif
            if (pBeacon->suppRatesPresent && (pBeacon->supportedRates.numRates <= 
                                              SIR_MAC_RATESET_EID_MAX))
            {
                pNode->info.neighborBssInfo.operationalRateSet.numRates = pBeacon->supportedRates.numRates;
               PELOG4(limLog(pMac, LOG4, FL("Supported Rates (%d) : "),
                        pNode->info.neighborBssInfo.operationalRateSet.numRates);)
                for (i=0; i<pBeacon->supportedRates.numRates; i++)
                {
                    pNode->info.neighborBssInfo.operationalRateSet.rate[i] = pBeacon->supportedRates.rate[i];
                    PELOG4(limLog(pMac, LOG4, FL("%d "), pBeacon->supportedRates.rate[i]);)
                }
                PELOG4(limLog(pMac, LOG4, FL("\n"));)
            }
            if (pBeacon->extendedRatesPresent && (pBeacon->extendedRates.numRates <= 
                                                  SIR_MAC_RATESET_EID_MAX))
            {
                pNode->info.neighborBssInfo.extendedRateSet.numRates = pBeacon->extendedRates.numRates;
               PELOG4(limLog(pMac, LOG4, FL("Extended Rates (%d) : "),
                        pNode->info.neighborBssInfo.extendedRateSet.numRates);)
                for (i=0; i<pBeacon->extendedRates.numRates; i++)
                {
                    pNode->info.neighborBssInfo.extendedRateSet.rate[i] = pBeacon->extendedRates.rate[i];
                    PELOG4(limLog(pMac, LOG4, FL("%d "), pBeacon->extendedRates.rate[i]);)
                }
            }
            else
            {
                pNode->info.neighborBssInfo.extendedRateSet.numRates = 0;
            }
            pNode->info.neighborBssInfo.nwType = nwType;
            pNode->info.neighborBssInfo.hcfEnabled = pBeacon->propIEinfo.hcfEnabled;
            pNode->info.neighborBssInfo.beaconInterval = pBeacon->beaconInterval;
            pNode->info.neighborBssInfo.wmeInfoPresent = pBeacon->wmeInfoPresent;
            pNode->info.neighborBssInfo.wmeEdcaPresent = pBeacon->wmeEdcaPresent;
            pNode->info.neighborBssInfo.wsmCapablePresent = pBeacon->wsmCapablePresent;
            pNode->info.neighborBssInfo.propIECapability = pBeacon->propIEinfo.capability;
            pNode->info.neighborBssInfo.localPowerConstraints = pBeacon->localPowerConstraint.localPowerConstraints;
            pNode->info.neighborBssInfo.dtimPeriod = pBeacon->tim.dtimPeriod;
            pNode->info.neighborBssInfo.HTCapsPresent = pBeacon->HTCaps.present;
            pNode->info.neighborBssInfo.HTInfoPresent = pBeacon->HTInfo.present;
        }
    }
} /****** end limCollectMeasurementData() ******/
/**
 * limCleanupMatrixNodes()
 *
 *FUNCTION:
 * This function is called from various places within LIM code
 * to cleanup channel info matrix collected by learn mode measurements.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
static void
limCleanupMatrixNodes(tpAniSirGlobal pMac)
{
    if (pMac->lim.gpLimMeasData->pMeasMatrixInfo)
    {
        tpLimMeasMatrixNode pNode = pMac->lim.gpLimMeasData->pMeasMatrixInfo;
        tpLimMeasMatrixNode pNext;
      while (pNode)
      {
            pNext = pNode->next;
            palFreeMemory( pMac->hHdd, pNode);
            if (pNext)
                pNode = pNext;
            else
              break;
          }
      }
    pMac->lim.gpLimMeasData->numMatrixNodes = 0;
         PELOG2(limLog(pMac, LOG2,
           FL("Cleaned up channel matrix nodes\n"));)
    pMac->lim.gpLimMeasData->pMeasMatrixInfo = NULL;
} /****** end limCleanupMatrixNodes() ******/
/**
 * limCleanupNeighborBssNodes()
 *
 *FUNCTION:
 * This function is called from various places within LIM code
 * to cleanup neighbor info collected by learn mode measurements.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
static void
limCleanupNeighborBssNodes(tpAniSirGlobal pMac)
{
    if (pMac->lim.gpLimMeasData->pNeighborWdsInfo)
    {
        tpLimNeighborBssWdsNode pNode =
                             pMac->lim.gpLimMeasData->pNeighborWdsInfo;
        tpLimNeighborBssWdsNode pNext;
        while (pNode)
        {
            pNext = pNode->next;
            pMac->lim.gpLimMeasData->numBssWds--;
            palFreeMemory( pMac->hHdd, pNode);
            if (pNext)
                pNode = pNext;
            else
                break;
        }
    }
   PELOG2(limLog(pMac, LOG2,
           FL("Cleaned up neighbor nodes\n"));)
    pMac->lim.gpLimMeasData->numBssWds        = 0;
    pMac->lim.gpLimMeasData->totalBssSize     = 0;
    pMac->lim.gpLimMeasData->pNeighborWdsInfo = NULL;
} /****** end limCleanupNeighborBssNodes() ******/

/**
 * limSendSmeMeasurementInd()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() to
 * send SME_MEASUREMENT_IND message to WSM.
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
limSendSmeMeasurementInd(tpAniSirGlobal pMac)
{
    tANI_U8          *pMeasInd;
    tANI_U16         len = 0;
    tSirMsgQ    mmhMsg;
#ifdef GEN6_TODO
    //fetch the sessionEntry based on the sessionId
    //priority - MEDIUM
    tpPESession sessionEntry;
    if((sessionEntry = peFindSessionBySessionId(pMac, pMac->lim.gLimMeasParams.measurementIndTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
#endif
    if (!pMac->sys.gSysEnableLearnMode ||
        (pMac->lim.gpLimMeasReq == NULL))
    {
        return;
    }
    len = sizeof(tSirSmeMeasurementInd) +
          (pMac->lim.gpLimMeasReq->channelList.numChannels *
           sizeof(tSirMeasMatrixInfo)) +
           pMac->lim.gpLimMeasData->totalBssSize;
    if (len > LIM_MAX_BUF_SIZE)
    {
        limLog(pMac, LOGP,
               FL("len %d numChannels %d numBssWds %d totalBssSize %d\n"),
               len,
               pMac->lim.gpLimMeasReq->channelList.numChannels,
               pMac->lim.gpLimMeasData->numBssWds,
               pMac->lim.gpLimMeasData->totalBssSize);
    }
    PELOG2(limLog(pMac, LOG2, FL("*****  Measurement IND size %d\n"), len);)
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMeasInd, len))
    {
        /// Buffer not available. Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for eWNI_SME_MEAS_IND\n"));
        return;
    }
   PELOG3(limLog(pMac, LOG3,
       FL("Sending eWNI_SME_MEAS_IND on Radio %d, requested len=%d\n"),
       pMac->sys.gSirRadioId, len);)
    limMeasurementIndSerDes(pMac, pMeasInd);
    mmhMsg.type = eWNI_SME_MEASUREMENT_IND;
    mmhMsg.bodyptr = pMeasInd;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
    // Cleanup neighbor information
    limCleanupNeighborBssNodes(pMac);
    limCleanupMatrixNodes(pMac);
} /*** end limSendSmeMeasurementInd() ***/

/**
 * limCleanupMeasData()
 *
 *FUNCTION:
 * This function is called from various places within LIM code
 * to cleanup measurement related data.
 *
 *LOGIC:
 * Buffers and associated timer resources will be deleted when
 * this function is called
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
void
limCleanupMeasData(tpAniSirGlobal pMac)
{
    if (pMac->lim.gpLimMeasReq)
        palFreeMemory( pMac->hHdd, pMac->lim.gpLimMeasReq);
    pMac->lim.gpLimMeasReq = NULL;
    if (!pMac->lim.gpLimMeasData)
        return;
    if (pMac->lim.gpLimMeasData->pMeasMatrixInfo)
    {
        // Retain current channel's data and flush remaining
        tpLimMeasMatrixNode pMatrix =
                        (pMac->lim.gpLimMeasData->pMeasMatrixInfo)->next;
        tpLimMeasMatrixNode pNext;
        while (pMatrix)
        {
            pNext = pMatrix->next;
            palFreeMemory( pMac->hHdd, pMatrix);
            if (pNext)
                pMatrix = pNext;
            else
                break;
        }
        pMac->lim.gpLimMeasData->pMeasMatrixInfo->next = NULL;
    }
    pMac->lim.gpLimMeasData->numMatrixNodes = 0;
   PELOG2(limLog(pMac, LOG2,
           FL("Cleaned up measurement metrics nodes\n"));)
    // Cleanup neighbor information
    limCleanupNeighborBssNodes(pMac);
} /****** end limCleanupMeasData() ******/
/**---------------------------------------------------------
\fn     limStopMeasTimers
\brief  Stops all measurement related timers.
\param  pMac
\return None
 ----------------------------------------------------------*/
void
limStopMeasTimers(tpAniSirGlobal pMac)
{
    if (pMac->lim.gpLimMeasReq == NULL)
        return;
    if (pMac->lim.gpLimMeasReq->measControl.periodicMeasEnabled)
    {
        if (tx_timer_deactivate(&pMac->lim.gLimMeasParams.measurementIndTimer) != TX_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE, FL("Cannot stop measurement Ind timer\n"));)
        }
    }
    pMac->lim.gLimMeasParams.isMeasIndTimerActive = 0;
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, NO_SESSION, eLIM_LEARN_INTERVAL_TIMER));
    if (tx_timer_deactivate(&pMac->lim.gLimMeasParams.learnIntervalTimer) != TX_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("Cannot stop learn interval timer\n"));)
    }
    if (pMac->lim.gLimSpecMgmt.fQuietEnabled)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, NO_SESSION, eLIM_LEARN_DURATION_TIMER));
        if (tx_timer_deactivate(&pMac->lim.gLimMeasParams.learnDurationTimer) != TX_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE, FL("Cannot stop learn duration timer\n"));)
        }
    }
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, NO_SESSION, eLIM_LEARN_DURATION_TIMER));
    if (tx_timer_deactivate(&pMac->lim.gLimMeasParams.learnDurationTimer) != TX_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("Cannot stop learn duration timer\n"));)
    }
}
/**
 * limDeleteMeasTimers()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() upon
 * receiving SME_MEASUREMENT_REQ. This function deletes
 * timers associated with Measurements.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
void
limDeleteMeasTimers(tpAniSirGlobal pMac)
{
    if (pMac->lim.gpLimMeasReq->measControl.periodicMeasEnabled)
    tx_timer_delete(&pMac->lim.gLimMeasParams.measurementIndTimer);
    tx_timer_delete(&pMac->lim.gLimMeasParams.learnIntervalTimer);
    tx_timer_delete(&pMac->lim.gLimMeasParams.learnDurationTimer);
} /*** end limDeleteMeasTimers() ***/

/**
 * limCleanupMeasResources()
 *
 *FUNCTION:
 * This function is called from various places within LIM code
 * to cleanup measurement related data and resources.
 *
 *LOGIC:
 * Buffers and associated timer resources will be deleted when
 * this function is called
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
void
limCleanupMeasResources(tpAniSirGlobal pMac)
{
   PELOG1( limLog(pMac, LOG1,
           FL("Cleaning up Learn mode Measurement resources\n"));)
    if (pMac->lim.gpLimMeasReq == NULL)
        return;
    limDeleteMeasTimers(pMac);
    if (pMac->lim.gpLimMeasData)
    {
        limCleanupMeasData(pMac);
        if (pMac->lim.gpLimMeasData->pMeasMatrixInfo)
            palFreeMemory( pMac->hHdd, pMac->lim.gpLimMeasData->pMeasMatrixInfo);
        palFreeMemory( pMac->hHdd, pMac->lim.gpLimMeasData);
        pMac->lim.gpLimMeasData = NULL;
    }
} /****** end limCleanupMeasResources() ******/

/**
 * limDeleteCurrentBssWdsNode()
 *
 *FUNCTION:
 * This function is called when Loss of link
 * is detected with AP and its BssWds info node
 * to be deleted
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  None
 * @return None
 */
void limDeleteCurrentBssWdsNode(tpAniSirGlobal pMac)
{
    tANI_U32                 cfg = sizeof(tSirMacAddr);
    tSirMacAddr         currentBssId;
#if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
#endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,sessionEntry->bssId);
    if (!pMac->lim.gpLimMeasData)
        return;
    if (pMac->lim.gpLimMeasData->pNeighborWdsInfo)
    {
        tpLimNeighborBssWdsNode pNode =
                             pMac->lim.gpLimMeasData->pNeighborWdsInfo;
        tpLimNeighborBssWdsNode pPrev = pNode;
        while (pNode)
        {
            if (palEqualMemory( pMac->hHdd,pNode->info.neighborBssInfo.bssId,
                          currentBssId,
                          sizeof(tSirMacAddr)))
            {
                pMac->lim.gpLimMeasData->numBssWds--;
                pPrev->next = pNode->next;
                palFreeMemory( pMac->hHdd, pNode);
                break;
            }
            pPrev = pNode;
            if (pNode->next)
                pNode = pNode->next;
            else
                break;
        }
        if (!pMac->lim.gpLimMeasData->numBssWds)
            pMac->lim.gpLimMeasData->pNeighborWdsInfo = NULL;
    }
} /****** end limDeleteCurrentBssWdsNode() ******/

/**
 * limRestorePreLearnState()
 *
 *FUNCTION:
 * This function is called when Learn duration timer expires
 * to restore pre-Learn mode state
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */
void
limRestorePreLearnState(tpAniSirGlobal pMac)
{
   PELOG4(limLog(pMac, LOG4,
           FL("Restoring from Learn mode on RadioId %d\n"),
           pMac->sys.gSirRadioId);)
    pMac->lim.gLimSystemInScanLearnMode = 0;
    // Go back to previous state.
    pMac->lim.gLimSmeState = pMac->lim.gLimPrevSmeState;
    pMac->lim.gLimMlmState = pMac->lim.gLimPrevMlmState;
    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, NO_SESSION, pMac->lim.gLimSmeState));
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, NO_SESSION, pMac->lim.gLimMlmState));
   PELOG4(limLog(pMac, LOG4,
           FL("Restored from Learn mode on RadioId %d\n"),
           pMac->sys.gSirRadioId);)
} /****** end limRestorePreLearnState() ******/
#endif //#if (defined(ANI_PRODUCT_TYPE_AP) || (ANI_PRODUCT_TYPE_AP_SDK))
/**
 * limGetHTCBState
 *
 *FUNCTION:
 * This routing provides the translation of Airgo Enum to HT enum for determining 
 * secondary channel offset.
 * Airgo Enum is required for backward compatibility purposes.
 *
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return The corresponding HT enumeration
 */
ePhyChanBondState  limGetHTCBState(ePhyChanBondState aniCBMode) 
{
    switch ( aniCBMode )
    {
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
#endif
        case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
        return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
#endif
        case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
        return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
           return PHY_SINGLE_CHANNEL_CENTERED;
#endif
        default :
           return PHY_SINGLE_CHANNEL_CENTERED;
     }
}

 /*
 * limGetStaPeerType
 *
 *FUNCTION:
 * Based on a combination of the following -
 * 1) tDphHashNode.aniPeer
 * 2) tDphHashNode.propCapability
 * this API determines if a given STA is an ANI peer or not
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pStaDs - Pointer to the tpDphHashNode of the STA
 *         under consideration
 * @return tStaRateMode
 */
tStaRateMode limGetStaPeerType( tpAniSirGlobal pMac,
    tpDphHashNode pStaDs,
    tpPESession   psessionEntry)
{
tStaRateMode staPeerType = eSTA_11b;
  // Determine the peer-STA type
  if( pStaDs->aniPeer )
  {
    if(PROP_CAPABILITY_GET( TAURUS, pStaDs->propCapability ))
        staPeerType = eSTA_TAURUS;
    else if( PROP_CAPABILITY_GET( TITAN, pStaDs->propCapability ))
        staPeerType = eSTA_TITAN;
    else
        staPeerType = eSTA_POLARIS;
  }
#ifdef WLAN_FEATURE_11AC
  else if(pStaDs->mlmStaContext.vhtCapability)
      staPeerType = eSTA_11ac;
#endif
  else if(pStaDs->mlmStaContext.htCapability)
        staPeerType = eSTA_11n;
  else if(pStaDs->erpEnabled)
        staPeerType = eSTA_11bg;
  else if(psessionEntry->limRFBand == SIR_BAND_5_GHZ)
        staPeerType = eSTA_11a;
  return staPeerType;
}

