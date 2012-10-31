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
 * Airgo Networks, Inc proprietary. All rights reserved. 
 * 
 * This file limSerDesUtils.cc contains the serializer/deserializer
 * utility functions LIM uses while communicating with upper layer
 * software entities
 * Author:        Chandra Modumudi
 * Date:          10/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "aniSystemDefs.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSerDesUtils.h"



/**
 * limCheckRemainingLength()
 *
 *FUNCTION:
 * This function is called while de-serializing received SME_REQ
 * message.
 *
 *LOGIC:
 * Remaining message length is checked for > 0.
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  len     - Remaining message length
 * @return retCode - eSIR_SUCCESS if len > 0, else eSIR_FAILURE
 */

static inline tSirRetStatus
limCheckRemainingLength(tpAniSirGlobal pMac, tANI_S16 len)
{
    if (len > 0)
        return eSIR_SUCCESS;
    else
    {
        limLog(pMac, LOGW,
           FL("Received SME message with invalid rem length=%d\n"),
           len);
        return eSIR_FAILURE;
    }
} /*** end limCheckRemainingLength(pMac, ) ***/

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
#else
/**
 * limGetBssDescription()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * BSS description from a tANI_U8* buffer pointer to tSirBssDescription
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pBssDescription  Pointer to the BssDescription to be copied
 * @param  *pBuf            Pointer to the source buffer
 * @param  rLen             Remaining message length being extracted
 * @return retCode          Indicates whether message is successfully
 *                          de-serialized (eSIR_SUCCESS) or
 *                          failure (eSIR_FAILURE).
 */

static tSirRetStatus
limGetBssDescription( tpAniSirGlobal pMac, tSirBssDescription *pBssDescription,
                     tANI_S16 rLen, tANI_S16 *lenUsed, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    pBssDescription->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len   = pBssDescription->length;

    if (rLen < (tANI_S16) (len + sizeof(tANI_U16)))
        return eSIR_FAILURE;

    *lenUsed = len + sizeof(tANI_U16);

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pBssDescription->bssId,
                  pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract timer
    palCopyMemory( pMac->hHdd, (tANI_U8 *) (&pBssDescription->scanSysTimeMsec),
                  pBuf, sizeof(v_TIME_t));
    pBuf += sizeof(v_TIME_t);
    len  -= sizeof(v_TIME_t);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract timeStamp
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pBssDescription->timeStamp,
                  pBuf, sizeof(tSirMacTimeStamp));
    pBuf += sizeof(tSirMacTimeStamp);
    len  -= sizeof(tSirMacTimeStamp);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract beaconInterval
    pBssDescription->beaconInterval = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract capabilityInfo
    pBssDescription->capabilityInfo = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract nwType
    pBssDescription->nwType = (tSirNwType) limGetU32(pBuf);
    pBuf += sizeof(tSirNwType);
    len  -= sizeof(tSirNwType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract aniIndicator
    pBssDescription->aniIndicator = *pBuf++;
    len --;

    // Extract rssi
    pBssDescription->rssi = (tANI_S8) *pBuf++;
    len --;

    // Extract sinr
    pBssDescription->sinr = (tANI_S8) *pBuf++;
    len --;

    // Extract channelId
    pBssDescription->channelId = *pBuf++;
    len --;

    // Extract channelIdSelf
    pBssDescription->channelIdSelf = *pBuf++;
    len --;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
 
    // Extract reserved bssDescription
    pBuf += sizeof(pBssDescription->sSirBssDescriptionRsvd);
    len -= sizeof(pBssDescription->sSirBssDescriptionRsvd);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //pass the timestamp
    pBssDescription->nReceivedTime = limGetU32( pBuf );
    pBuf += sizeof(tANI_TIMESTAMP);
    len -= sizeof(tANI_TIMESTAMP);

#if defined WLAN_FEATURE_VOWIFI
    //TSF when the beacon received (parent TSF)
    pBssDescription->parentTSF = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);

    //start TSF of scan during which this BSS desc was updated.
    pBssDescription->startTSF[0] = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);

    //start TSF of scan during which this BSS desc was updated.
    pBssDescription->startTSF[1] = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    // MobilityDomain
    pBssDescription->mdiePresent = *pBuf++;
    len --;
    pBssDescription->mdie[0] = *pBuf++;
    len --;
    pBssDescription->mdie[1] = *pBuf++;
    len --;
    pBssDescription->mdie[2] = *pBuf++;
    len --;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog(pMac, LOG1, FL("mdie=%02x %02x %02x\n"), 
        pBssDescription->mdie[0],
        pBssDescription->mdie[1],
        pBssDescription->mdie[2]);)
#endif
#endif

#ifdef FEATURE_WLAN_CCX
    pBssDescription->QBSSLoad_present = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract QBSSLoad_avail
    pBssDescription->QBSSLoad_avail = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif
    pBssDescription->fProbeRsp = *pBuf++;
    len  -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    /* 3 reserved bytes for padding */
    pBuf += (3 * sizeof(tANI_U8));
    len  -= 3;

    pBssDescription->WscIeLen = limGetU32( pBuf );
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    if (WSCIE_PROBE_RSP_LEN < len)
    {
        /* Do not copy with WscIeLen
         * if WscIeLen is not set properly, memory overwrite happen
         * Ended up with memory corruption and crash
         * Copy with Fixed size */
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pBssDescription->WscIeProbeRsp,
                       pBuf,
                       WSCIE_PROBE_RSP_LEN);

    }
    else
    {
        limLog(pMac, LOGE,
                     FL("remaining bytes len %d is less than WSCIE_PROBE_RSP_LEN\n"),
                     pBssDescription->WscIeLen);
        return eSIR_FAILURE;
    }

    /* 1 reserved byte padding */
    pBuf += (WSCIE_PROBE_RSP_LEN + 1);
    len -= (WSCIE_PROBE_RSP_LEN + 1);

    if (len > 0)
    {
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pBssDescription->ieFields,
                       pBuf,
                       len);
    }
    else if (len < 0)
    {
        limLog(pMac, LOGE, 
                     FL("remaining length is negative. len = %d, actual length = %d\n"), 
                     len, pBssDescription->length);
        return eSIR_FAILURE;
    }    

    return eSIR_SUCCESS;
} /*** end limGetBssDescription() ***/
#endif



/**
 * limCopyBssDescription()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * BSS description to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *pBuf            Pointer to the destination buffer
 * @param  pBssDescription  Pointer to the BssDescription being copied
 * @return                  Length of BSSdescription written
 */

tANI_U16
limCopyBssDescription(tpAniSirGlobal pMac, tANI_U8 *pBuf, tSirBssDescription *pBssDescription)
{
    tANI_U16 len = 0;

    limCopyU16(pBuf, pBssDescription->length);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    palCopyMemory( pMac->hHdd, pBuf,
                  (tANI_U8 *) pBssDescription->bssId,
                  sizeof(tSirMacAddr));
    pBuf       += sizeof(tSirMacAddr);
    len        += sizeof(tSirMacAddr);

   PELOG3(limLog(pMac, LOG3,
       FL("Copying BSSdescr:channel is %d, aniInd is %d, bssId is "),
       pBssDescription->channelId, pBssDescription->aniIndicator);
    limPrintMacAddr(pMac, pBssDescription->bssId, LOG3);)

    palCopyMemory( pMac->hHdd, pBuf,
                  (tANI_U8 *) (&pBssDescription->scanSysTimeMsec),
                  sizeof(v_TIME_t));
    pBuf       += sizeof(v_TIME_t);
    len        += sizeof(v_TIME_t);

    limCopyU32(pBuf, pBssDescription->timeStamp[0]);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    limCopyU32(pBuf, pBssDescription->timeStamp[1]);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    limCopyU16(pBuf, pBssDescription->beaconInterval);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    limCopyU16(pBuf, pBssDescription->capabilityInfo);
    pBuf       += sizeof(tANI_U16);
    len        += sizeof(tANI_U16);

    limCopyU32(pBuf, pBssDescription->nwType);
    pBuf       += sizeof(tANI_U32);
    len        += sizeof(tANI_U32);

    *pBuf++ = pBssDescription->aniIndicator;
    len++;

    *pBuf++ = pBssDescription->rssi;
    len++;

    *pBuf++ = pBssDescription->sinr;
    len++;

    *pBuf++ = pBssDescription->channelId;
    len++;

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssDescription->ieFields),
                  limGetIElenFromBssDescription(pBssDescription));

    return (len + sizeof(tANI_U16));
} /*** end limCopyBssDescription() ***/



#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
/**
 * limCopyLoad()
 *
 *fUNCTION:
 * This function is called by various LIM functions to copy
 * load information into a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pBuf     Pointer to the destination buffer
 * @param load      load to be copied to destination buffer
 *
 * @return None
 */

static void
limCopyLoad(tANI_U8 *pBuf, tSirLoad load)
{
    limCopyU16(pBuf, load.numStas);
    pBuf += sizeof(tANI_U16);

    limCopyU16(pBuf, load.channelUtilization);
    pBuf += sizeof(tANI_U16);
} /*** end limCopyLoad() ***/



/**
 * limGetLoad()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * load information into a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pLoad    Pointer to load being copied to
 * @param *pBuf     Pointer to the source buffer
 *
 * @return None
 */

static void
limGetLoad(tSirLoad *pLoad, tANI_U8 *pBuf)
{
    pLoad->numStas = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    pLoad->channelUtilization = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
} /*** end limGetLoad() ***/



#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/**
 * limCopyWdsInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * WdsInfo information into a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *pBuf     Pointer to the destination buffer
 * @param  wdsInfo   WdsInfo to be copied to destination buffer
 *
 * @return Length of WDS info copied
 */

static tANI_U16
limCopyWdsInfo(tpAniSirGlobal pMac, tANI_U8 *pBuf, tpSirWdsInfo pWdsInfo)
{
    limCopyU16(pBuf, pWdsInfo->wdsLength);
    pBuf += sizeof(tANI_U16);

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) pWdsInfo->wdsBytes, pWdsInfo->wdsLength);
    pBuf += pWdsInfo->wdsLength;

    return ((tANI_U16) (sizeof(tANI_U16) + pWdsInfo->wdsLength));
} /*** end limCopyWdsInfo() ***/



/**
 * limGetWdsInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * WdsInfo information from a tANI_U8* buffer pointer to tSirWdsInfo
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param wdsInfo   Pointer to the WdsInfo to be copied
 * @param *pBuf     Pointer to the source buffer
 *
 * @return true when WDS info extracted successfully else false
 */

static tANI_U8
limGetWdsInfo(tpAniSirGlobal pMac, tpSirWdsInfo pWdsInfo, tANI_U8 *pBuf)
{
    pWdsInfo->wdsLength = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (pWdsInfo->wdsLength > ANI_WDS_INFO_MAX_LENGTH)
        return false;

    palCopyMemory( pMac->hHdd, pWdsInfo->wdsBytes, pBuf, pWdsInfo->wdsLength);

    return true;
} /*** end limGetWdsInfo() ***/



/**
 * limGetAlternateRadioList()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * alternateRadio information from a tANI_U8* buffer pointer to
 * tSirAlternateRadioList
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pRadioList   Pointer to the radioList to be copied
 * @param  *pBuf        Pointer to the source buffer
 *
 * @return len          Length of copied alternateRadioList
 */

static tANI_U16
limGetAlternateRadioList(tpAniSirGlobal pMac, tpSirMultipleAlternateRadioInfo pRadioList,
                         tANI_U8 *pBuf)
{
    tANI_U8 i;
    tANI_U16                        len = sizeof(tANI_U8);
    tpSirAlternateRadioInfo    pRadioInfo;

    pRadioList->numBss = *pBuf++;

    if (pRadioList->numBss > SIR_MAX_NUM_ALTERNATE_RADIOS)
        pRadioList->numBss = SIR_MAX_NUM_ALTERNATE_RADIOS;

    pRadioInfo = pRadioList->alternateRadio;
    for (i = 0; i < pRadioList->numBss; i++)
    {
        palCopyMemory( pMac->hHdd, pRadioInfo->bssId, pBuf, sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        pRadioInfo->channelId = *pBuf++;
       PELOG3(limLog(pMac, LOG3, FL("Alternate radio[%d] channelId=%d, BssId is \n"),
               i, pRadioInfo->channelId);
        limPrintMacAddr(pMac, pRadioInfo->bssId, LOG3);)

        pRadioInfo++;
        len += sizeof(tSirMacAddr) + sizeof(tANI_U8);
    }

    return len;
} /*** end limGetAlternateRadioList() ***/
#endif



/**
 * limCopyNeighborBssInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * Neighbor BSS info into a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *pBuf       Pointer to the destination buffer
 * @param  pBssInfo    Pointer to neighbor BSS info be copied
 *                     to destination buffer
 *
 * @return bssInfoLen  Length of bssInfo copied
 */

tANI_U32
limCopyNeighborBssInfo(tpAniSirGlobal pMac, tANI_U8 *pBuf, tpSirNeighborBssInfo pBssInfo)
{
    tANI_U32 bssInfoLen = 0;

    palCopyMemory( pMac->hHdd, pBuf,
                  (tANI_U8 *) pBssInfo->bssId,
                  sizeof(tSirMacAddr));
    pBuf       += sizeof(tSirMacAddr);
    bssInfoLen += sizeof(tSirMacAddr);
   PELOG3(limLog(pMac, LOG3,
       FL("Copying new NeighborWds node:channel is %d, wniIndicator is %d, bssType is %d, bssId is "),
       pBssInfo->channelId, pBssInfo->wniIndicator, pBssInfo->bssType);
    limPrintMacAddr(pMac, pBssInfo->bssId, LOG3);)

    *pBuf++ = pBssInfo->channelId;
    bssInfoLen++;

    limCopyU32(pBuf, pBssInfo->wniIndicator);
    pBuf       += sizeof(tANI_U32);
    bssInfoLen += sizeof(tANI_U32);

    limCopyU32(pBuf, pBssInfo->bssType);
    pBuf       += sizeof(tANI_U32);
    bssInfoLen += sizeof(tANI_U32);

    *pBuf++ = pBssInfo->sinr;
    bssInfoLen++;
    *pBuf++ = pBssInfo->rssi;
    bssInfoLen++;

    limCopyLoad(pBuf, pBssInfo->load);
    pBuf       += sizeof(tSirLoad);
    bssInfoLen += sizeof(tSirLoad);

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssInfo->ssId),
                  pBssInfo->ssId.length + 1);

    bssInfoLen += pBssInfo->ssId.length + 1;
    pBuf       += pBssInfo->ssId.length + 1;

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssInfo->apName),
                  pBssInfo->apName.length + 1);

    bssInfoLen += pBssInfo->apName.length + 1;
    pBuf       += pBssInfo->apName.length + 1;

    limCopyU16(pBuf, pBssInfo->rsnIE.length);
    pBuf       += sizeof(tANI_U16);
    bssInfoLen += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssInfo->rsnIE.rsnIEdata),
                  pBssInfo->rsnIE.length);

    bssInfoLen += pBssInfo->rsnIE.length;
    pBuf       += pBssInfo->rsnIE.length;

    limCopyU32(pBuf, pBssInfo->nwType);
    pBuf       += sizeof(tANI_U32);
    bssInfoLen += sizeof(tANI_U32);

    // not sure we need to do a sirSwapU16ifNeeded ???
    limCopyU16(pBuf, pBssInfo->capabilityInfo);
    pBuf       += sizeof(tANI_U16);
    bssInfoLen += sizeof(tANI_U16);

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssInfo->operationalRateSet),
                  pBssInfo->operationalRateSet.numRates + 1);
    bssInfoLen += pBssInfo->operationalRateSet.numRates + 1;
    pBuf       += pBssInfo->operationalRateSet.numRates + 1;

    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pBssInfo->extendedRateSet),
                  pBssInfo->extendedRateSet.numRates + 1);
    bssInfoLen += pBssInfo->extendedRateSet.numRates + 1;
    pBuf       += pBssInfo->extendedRateSet.numRates + 1;

    limCopyU16(pBuf, pBssInfo->beaconInterval);
    pBuf += sizeof(tANI_U16);
    bssInfoLen += sizeof(tANI_U16);
   
    *pBuf++ = pBssInfo->dtimPeriod;
    bssInfoLen++;
    *pBuf++ = pBssInfo->HTCapsPresent;
    bssInfoLen++;
    *pBuf++ = pBssInfo->HTInfoPresent;
    bssInfoLen++;
    *pBuf++ = pBssInfo->wmeInfoPresent;
    bssInfoLen++;
    *pBuf++ = pBssInfo->wmeEdcaPresent;
    bssInfoLen++;
    *pBuf++ = pBssInfo->wsmCapablePresent;
    bssInfoLen++;
    *pBuf++ = pBssInfo->hcfEnabled;
    bssInfoLen++;
    
    limCopyU16(pBuf, pBssInfo->propIECapability);
    pBuf += sizeof(tANI_U16);
    bssInfoLen += sizeof(tANI_U16);
    
    limCopyU32(pBuf, pBssInfo->localPowerConstraints);
    pBuf += sizeof(tANI_S32);
    bssInfoLen += sizeof(tANI_S32);
    
    limCopyU32(pBuf, pBssInfo->aggrRssi);
    pBuf += sizeof(tANI_S32);
    bssInfoLen += sizeof(tANI_S32);
    
    limCopyU32(pBuf, pBssInfo->dataCount);
    pBuf += sizeof(tANI_U32);
    bssInfoLen += sizeof(tANI_U32);
    
    limCopyU32(pBuf, pBssInfo->totalPackets);
    pBuf += sizeof(tANI_U32);
    bssInfoLen += sizeof(tANI_U32);
    
    return bssInfoLen;
} /*** end limCopyNeighborBssInfo() ***/


/**
 * limCopyNeighborList()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * Neighbor BSS list into a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pBuf           Pointer to the destination buffer
 * @param neighborList    Neighbor BSS list be copied to
 *                        destination buffer
 * @param numBss          Number of Neighbor BSS list be copied
 *
 * @return listLen        Length of Neighbor BSS list
 */

static tANI_U32
limCopyNeighborList(tpAniSirGlobal pMac, tANI_U8 *pBuf, tpSirNeighborBssInfo pBssInfo, tANI_U32 numBss)
{
    tANI_U32  bssInfoLen = 0, listLen = 0;
    tANI_U8  i;
    tANI_U8  *pTemp = (tANI_U8 *) pBssInfo;

    PELOG3(limLog(pMac, LOG3, FL("Going to copy BssInfoList:\n"));)
    PELOG3(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3,
               pTemp, numBss*sizeof(tSirNeighborBssInfo));)

    for (i = 0; i < numBss; i++, bssInfoLen = 0)
    {
        bssInfoLen = limCopyNeighborBssInfo(pMac,
                                   pBuf,
                                   (tpSirNeighborBssInfo) &pBssInfo[i]);

        pBuf     += bssInfoLen;
        listLen  += bssInfoLen;
    }

    return listLen;
} /*** end limCopyNeighborList(pMac, ) ***/


/**
 * limGetNeighborBssInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to get
 * Neighbor BSS info from a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pBssInfo    Pointer to neighbor BSS info being copied
 * @param  *pBuf       Pointer to the source buffer
 *
 * @return Size of NeighborBssInfo that is extracted
 */

tANI_U32
limGetNeighborBssInfo(tpAniSirGlobal pMac, tpSirNeighborBssInfo pBssInfo, tANI_U8 *pBuf)
{
    tANI_U32 len = 0;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pBssInfo->bssId,
                  pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  += sizeof(tSirMacAddr);

    pBssInfo->channelId = *pBuf++;
    len++;

    pBssInfo->wniIndicator = (tAniBool) limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len  += sizeof(tAniBool);

    pBssInfo->bssType = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tSirBssType);
    len  += sizeof(tSirBssType);

    pBssInfo->sinr = *pBuf++;
    len++;
    pBssInfo->rssi = *pBuf++;
    len++;

    limGetLoad(&pBssInfo->load, pBuf);
    pBuf += sizeof(tSirLoad);
    len  += sizeof(tSirLoad);

    pBssInfo->ssId.length = *pBuf++;
    palCopyMemory( pMac->hHdd, pBssInfo->ssId.ssId, pBuf, pBssInfo->ssId.length);
    pBuf += pBssInfo->ssId.length;
    len  += pBssInfo->ssId.length + 1;
    
    pBssInfo->apName.length = *pBuf++;
    palCopyMemory( pMac->hHdd, pBssInfo->apName.name, pBuf, pBssInfo->apName.length);
    pBuf += pBssInfo->apName.length;
    len  += pBssInfo->apName.length + 1;
    
    pBssInfo->rsnIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pBssInfo->rsnIE.rsnIEdata, pBuf,
                  pBssInfo->rsnIE.length);
    pBuf += pBssInfo->rsnIE.length;
    len  += pBssInfo->rsnIE.length + 2;

   PELOG2(limLog(pMac, LOG2, FL("BSS type %d channel %d wniInd %d RSN len %d\n"),
           pBssInfo->bssType, pBssInfo->channelId, pBssInfo->wniIndicator,
           pBssInfo->rsnIE.length);
    limPrintMacAddr(pMac, pBssInfo->bssId, LOG2);)


    pBssInfo->nwType = (tSirNwType) limGetU32(pBuf);
    pBuf += sizeof(tSirNwType);
    len  += sizeof(tSirNwType);
    
    pBssInfo->capabilityInfo = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  += sizeof(tANI_U16);

    pBssInfo->operationalRateSet.numRates = *pBuf++;
    palCopyMemory( pMac->hHdd, pBssInfo->operationalRateSet.rate, pBuf,
                  pBssInfo->operationalRateSet.numRates);
    pBuf += pBssInfo->operationalRateSet.numRates;
    len  += pBssInfo->operationalRateSet.numRates + 1;

    pBssInfo->extendedRateSet.numRates = *pBuf++;
    palCopyMemory( pMac->hHdd, pBssInfo->extendedRateSet.rate, pBuf,
                  pBssInfo->extendedRateSet.numRates);
    pBuf += pBssInfo->extendedRateSet.numRates;
    len  += pBssInfo->extendedRateSet.numRates + 1;

    pBssInfo->beaconInterval = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  += sizeof(tANI_U16);

    pBssInfo->dtimPeriod = *pBuf++;
    pBssInfo->HTCapsPresent = *pBuf++;
    pBssInfo->HTInfoPresent = *pBuf++;
    pBssInfo->wmeInfoPresent = *pBuf++;
    pBssInfo->wmeEdcaPresent = *pBuf++;
    pBssInfo->wsmCapablePresent = *pBuf++;
    pBssInfo->hcfEnabled = *pBuf++;
    pBssInfo->propIECapability = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    pBssInfo->localPowerConstraints = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len += 13;
    
   PELOG2(limLog(pMac, LOG2, FL("rsnIELen %d operRateLen %d extendRateLen %d total %d\n"),
        pBssInfo->rsnIE.length, pBssInfo->operationalRateSet.numRates,
        pBssInfo->extendedRateSet.numRates, len);)
    
    
    return len;
} /*** end limGetNeighborBssInfo() ***/



#if defined(ANI_PRODUCT_TYPE_AP)
/**
 * limGetNeighborBssList()
 *
 *FUNCTION:
 * This function is called by various LIM functions to get
 * Neighbor BSS list from a tANI_U8* buffer pointer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  *pNeighborList   Pointer to neighbor BSS list being copied to
 * @param  *pBuf            Pointer to the source buffer
 * @param  rLen             Remaining message length being extracted
 * @return retCode          Indicates whether message is successfully
 *                          de-serialized (eSIR_SUCCESS) or
 *                          failure (eSIR_FAILURE).
 */

static tSirRetStatus
limGetNeighborBssList(tpAniSirGlobal pMac,
                      tSirNeighborBssList *pNeighborList,
                      tANI_S16 rLen, tANI_S16 *lenUsed, tANI_U8 *pBuf)
{
    tANI_U8     *pBssInfo = (tANI_U8 *) pNeighborList->bssList;
    tANI_U32    i, bssInfoLen = 0, len = 0, numBss, numBssList;

    numBssList = numBss = limGetU32(pBuf);

    if (!numBss)
    {
        PELOG1(limLog(pMac, LOG1, FL("No Neighbor BSS present in Neighbor list\n"));)

        return eSIR_FAILURE;
    }
   PELOG2(limLog(pMac, LOG2,
           FL("Num of Neighbor BSS present in Neighbor list is %d\n"),
           numBss);)

    pBuf += sizeof(tANI_U32);
    len  += sizeof(tANI_U32);
    rLen -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, rLen) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // First neighborInfo is the one we're attempting to join/reassoc
    bssInfoLen = limGetNeighborBssInfo(pMac, (tSirNeighborBssInfo *) pBssInfo,
                                       pBuf);
    PELOG1(limLog(pMac, LOG1,
           FL("BSSinfo len to be joined is %d rem %d, numBss = %d\n"),
           bssInfoLen, rLen, numBss - 1);)
    pBuf  += bssInfoLen;
    len   += bssInfoLen;
    rLen  -= (tANI_S16) bssInfoLen;
    numBss--;
    numBssList--;

    if (numBss > 0)
    {
        // Store remaining neighbor BSS info
        if (numBss > SIR_MAX_NUM_NEIGHBOR_BSS)
        {
            // Store only MAX number of Neighbor BSS
          PELOG2(limLog(pMac, LOG2,
                  FL("Pruning number of neighbors to %d from %d\n"),
                  SIR_MAX_NUM_NEIGHBOR_BSS, numBss);)
            numBss = SIR_MAX_NUM_NEIGHBOR_BSS;
        }

        pBssInfo = (tANI_U8 *) pMac->lim.gLimNeighborBssList.bssList;
        for (i = numBss; i > 0; i--)
        {
            PELOG3(limLog(pMac, LOG3, FL("pMac = %p, pBssInfo = %p, pBuf = %p\n"), pMac, pBssInfo, pBuf);)
            bssInfoLen = limGetNeighborBssInfo(pMac,
                                   (tSirNeighborBssInfo *) pBssInfo,
                                   pBuf);

            pBssInfo += sizeof(tSirNeighborBssInfo);
            pBuf     += bssInfoLen;
            len      += bssInfoLen;
            rLen     -= (tANI_S16) bssInfoLen;
            numBssList--;

            PELOG1(limLog(pMac, LOG1, FL("BSS info[%d] len %d rem %d\n"),
                   i, bssInfoLen, rLen);)
        }

        while (numBssList > 0)
        {
            tSirNeighborBssInfo pTemp;
            bssInfoLen = limGetNeighborBssInfo(pMac, &pTemp, pBuf);
            pBuf     += bssInfoLen;
            len      += bssInfoLen;
            rLen     -= (tANI_S16) bssInfoLen;
            numBssList--;
            PELOG1(limLog(pMac, LOG1, FL("BSS info[%d] len %d rem %d\n"),
                   numBssList, bssInfoLen, rLen);)
        }
    }
    *lenUsed = len;

    pMac->lim.gLimNeighborBssList.numBss = pNeighborList->numBss
                                         = numBss;

    return eSIR_SUCCESS;
} /*** end limGetNeighborBssList(pMac, ) ***/



/**
 * limCopyNeighborWdsInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * detected neighborBssWds info to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pBuf           Pointer to the destination buffer
 * @param *pInfo          Pointer to the NeighborWdsInfo being copied
 * @return Length of Matrix information copied
 */

static tANI_U16
limCopyNeighborWdsInfo(tpAniSirGlobal pMac, tANI_U8 *pBuf, tpSirNeighborBssWdsInfo pInfo)
{
    tANI_U16    len = 0;

    len   = (tANI_U16) limCopyNeighborBssInfo(pMac, pBuf, &pInfo->neighborBssInfo);
    pBuf += len;
    len  += limCopyWdsInfo(pMac, pBuf, &pInfo->wdsInfo);

    return len;
} /*** end limCopyNeighborWdsInfo() ***/



/**
 * limCopyMeasMatrixInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * collected Measurement matrix info to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pBuf           Pointer to the destination buffer
 * @param *pInfo          Pointer to the matrix info being copied
 * @return Length of Matrix information copied
 */

static tANI_U16
limCopyMeasMatrixInfo(tANI_U8 *pBuf, tpLimMeasMatrixNode pInfo)
{
    tANI_U16 len = 0;
    
    *pBuf++ = pInfo->matrix.channelNumber;
    len++;
    *pBuf++ = pInfo->matrix.compositeRssi;
    len++;
    limCopyU32(pBuf, pInfo->matrix.aggrRssi);
    pBuf += sizeof(tANI_S32);
    len += sizeof(tANI_S32);
    limCopyU32(pBuf, pInfo->matrix.totalPackets);
    len += sizeof(tANI_U32);

    return len;
} /*** end limCopyMeasMatrixInfo() ***/



/**
 * limCopyMeasMatrixList()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * collected Measurement matrix List to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @param  *pBuf - Pointer to the destination buffer
 * @return Length of matrix list copied
 */

static tANI_U16
limCopyMeasMatrixList(tpAniSirGlobal pMac, tANI_U8 *pBuf)
{
    tANI_U16    len = 0, nodeLen = 0;
    tANI_U8     numNodes = pMac->lim.gpLimMeasData->numMatrixNodes;

    *pBuf++ = numNodes;
    len++;

    if (numNodes)
    {
        tpLimMeasMatrixNode pInfo =
                         pMac->lim.gpLimMeasData->pMeasMatrixInfo;
        while (numNodes-- && pInfo) //Safety measure, checking both
        {
            nodeLen = limCopyMeasMatrixInfo(pBuf, pInfo);
            pBuf   += nodeLen;
            len    += nodeLen;

            if (pInfo->next)
                pInfo = pInfo->next;
            else
                break;
        }

        return len;
    }
    else
    {
        *pBuf = 0;
        return 1;
    }
} /*** end limCopyMeasMatrixList() ***/



/**
 * limCopyNeighborWdsList()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * detected neighborBssWds List to a tANI_U8 buffer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac  Pointer to Global MAC structure
 * @param  *pBuf Pointer to the destination buffer
 * @return Length of matrix list copied
 */

static tANI_U16
limCopyNeighborWdsList(tpAniSirGlobal pMac, tANI_U8 *pBuf)
{
    tANI_U16    len = 0, nodeLen = 0;

    if (pMac->lim.gpLimMeasData->numBssWds)
    {
        limCopyU32(pBuf, pMac->lim.gpLimMeasData->numBssWds);
        pBuf += sizeof(tANI_U32);
        len  += sizeof(tANI_U32);

        tpLimNeighborBssWdsNode pInfo =
                        pMac->lim.gpLimMeasData->pNeighborWdsInfo;
        while (pInfo)
        {
            nodeLen = limCopyNeighborWdsInfo(pMac, pBuf, &pInfo->info);
            pBuf   += nodeLen;
            len    += nodeLen;

            if (pInfo->next)
                pInfo = pInfo->next;
            else
                break;
        }

        return len;
    }
    else
    {
        limCopyU32(pBuf, 0);
        return (sizeof(tANI_U32));
    }
} /*** end limCopyNeighborWdsList() ***/
#endif
#endif


/**
 * limGetKeysInfo()
 *
 *FUNCTION:
 * This function is called by various LIM functions to copy
 * key information from a tANI_U8* buffer pointer to tSirKeys
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param *pKeyInfo   Pointer to the keyInfo to be copied
 * @param *pBuf       Pointer to the source buffer
 *
 * @return Length of key info extracted
 */

static tANI_U32
limGetKeysInfo(tpAniSirGlobal pMac, tpSirKeys pKeyInfo, tANI_U8 *pBuf)
{
    tANI_U32 len = 0;

    pKeyInfo->keyId        = *pBuf++;
    len++;
    pKeyInfo->unicast      = *pBuf++;
    len++;
    pKeyInfo->keyDirection = (tAniKeyDirection) limGetU32(pBuf);
    len  += sizeof(tAniKeyDirection);
    pBuf += sizeof(tAniKeyDirection);

    palCopyMemory( pMac->hHdd, pKeyInfo->keyRsc, pBuf, WLAN_MAX_KEY_RSC_LEN);
    pBuf += WLAN_MAX_KEY_RSC_LEN;
    len  += WLAN_MAX_KEY_RSC_LEN;

    pKeyInfo->paeRole      = *pBuf++;
    len++;

    pKeyInfo->keyLength    = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pKeyInfo->key, pBuf, pKeyInfo->keyLength);
    pBuf += pKeyInfo->keyLength;
    len  += pKeyInfo->keyLength;

   PELOG3(limLog(pMac, LOG3,
           FL("Extracted keyId=%d, keyLength=%d, Key is :\n"),
           pKeyInfo->keyId, pKeyInfo->keyLength);
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3,
               pKeyInfo->key, pKeyInfo->keyLength);)

    return len;
} /*** end limGetKeysInfo() ***/



/**
 * limStartBssReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_START_BSS_REQ from host
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
 * @param  pStartBssReq  Pointer to tSirSmeStartBssReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limStartBssReqSerDes(tpAniSirGlobal pMac, tpSirSmeStartBssReq pStartBssReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    tANI_U8 i;
#endif

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pStartBssReq || !pBuf)
        return eSIR_FAILURE;

    pStartBssReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pStartBssReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_START_BSS_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pStartBssReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pStartBssReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pStartBssReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract selfMacAddr
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pStartBssReq->selfMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract beaconInterval
    pStartBssReq->beaconInterval = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract dot11Mode
    pStartBssReq->dot11mode = *pBuf++;
    len --;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssType
    pStartBssReq->bssType = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract ssId
    if (*pBuf > SIR_MAC_MAX_SSID_LENGTH)
    {
        // SSID length is more than max allowed 32 bytes
        PELOGW(limLog(pMac, LOGW, FL("Invalid SSID length, len=%d\n"), *pBuf);)
        return eSIR_FAILURE;
    }

    pStartBssReq->ssId.length = *pBuf++;
    len--;
    if (len < pStartBssReq->ssId.length)
    {
        limLog(pMac, LOGW,
           FL("SSID length is longer that the remaining length. SSID len=%d, remaining len=%d\n"),
           pStartBssReq->ssId.length, len);
        return eSIR_FAILURE;
    }

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pStartBssReq->ssId.ssId,
                  pBuf,
                  pStartBssReq->ssId.length);
    pBuf += pStartBssReq->ssId.length;
    len  -= pStartBssReq->ssId.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract channelId
    pStartBssReq->channelId = *pBuf++;
    len--;

    // Extract CB secondary channel info
    pStartBssReq->cbMode = (ePhyChanBondState)limGetU32( pBuf );
    pBuf += sizeof( tANI_U32 );
    len -= sizeof( tANI_U32 );

    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    tANI_U16 paramLen = 0;

    // Extract alternateRadioList
    pStartBssReq->alternateRadioList.numBss = *pBuf;
    paramLen = limGetAlternateRadioList(pMac,
                              &pMac->lim.gLimAlternateRadioList,
                              pBuf);
    pBuf += paramLen;
    len  -= paramLen;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract powerLevel
    pStartBssReq->powerLevel = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract wdsInfo
    if (limGetWdsInfo(pMac, &pStartBssReq->wdsInfo, pBuf) == false)
    {
        limLog(pMac, LOGW, FL("Invalid WDS length %d in SME_START_BSS_REQ\n"),
               pStartBssReq->wdsInfo.wdsLength);
        return eSIR_FAILURE;
    }

    pBuf += sizeof(tANI_U16) + pStartBssReq->wdsInfo.wdsLength;
    len  -= (sizeof(tANI_U16) + pStartBssReq->wdsInfo.wdsLength);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif

#ifdef WLAN_SOFTAP_FEATURE
    // Extract privacy setting
    pStartBssReq->privacy = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //Extract Uapsd Enable 
    pStartBssReq->apUapsdEnable = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    //Extract SSID hidden
    pStartBssReq->ssidHidden = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pStartBssReq->fwdWPSPBCProbeReq = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    //Extract HT Protection Enable 
    pStartBssReq->protEnabled = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;

    //Extract OBSS Protection Enable 
    pStartBssReq->obssProtEnabled = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
       return eSIR_FAILURE;

    pStartBssReq->ht_capab = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract AuthType
    pStartBssReq->authType = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract dtimPeriod
    pStartBssReq->dtimPeriod = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract wps state
    pStartBssReq->wps_state = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#endif
    // Extract bssPersona
    pStartBssReq->bssPersona = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract rsnIe
    pStartBssReq->rsnIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    // Check for RSN IE length (that includes length of type & length
    if (pStartBssReq->rsnIE.length > SIR_MAC_MAX_IE_LENGTH + 2)
    {
        limLog(pMac, LOGW,
               FL("Invalid RSN IE length %d in SME_START_BSS_REQ\n"),
               pStartBssReq->rsnIE.length);
        return eSIR_FAILURE;
    }

    palCopyMemory( pMac->hHdd, pStartBssReq->rsnIE.rsnIEdata,
                  pBuf, pStartBssReq->rsnIE.length);

    len  -= (sizeof(tANI_U16) + pStartBssReq->rsnIE.length);
    pBuf += pStartBssReq->rsnIE.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract nwType
    pStartBssReq->nwType = (tSirNwType) limGetU32(pBuf);
    pBuf += sizeof(tSirNwType);
    len  -= sizeof(tSirNwType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract operationalRateSet
    pStartBssReq->operationalRateSet.numRates = *pBuf++;

    // Check for number of rates
    if (pStartBssReq->operationalRateSet.numRates >
        SIR_MAC_MAX_NUMBER_OF_RATES)
    {
        limLog(pMac, LOGW, FL("Invalid numRates %d in SME_START_BSS_REQ\n"),
               pStartBssReq->operationalRateSet.numRates);
        return eSIR_FAILURE;
    }

    len--;
    if (len < pStartBssReq->operationalRateSet.numRates)
        return eSIR_FAILURE;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pStartBssReq->operationalRateSet.rate,
                  pBuf,
                  pStartBssReq->operationalRateSet.numRates);
    pBuf += pStartBssReq->operationalRateSet.numRates;
    len  -= pStartBssReq->operationalRateSet.numRates;

    // Extract extendedRateSet
    if ((pStartBssReq->nwType == eSIR_11G_NW_TYPE) ||
        (pStartBssReq->nwType == eSIR_11N_NW_TYPE ))
    {
        pStartBssReq->extendedRateSet.numRates = *pBuf++;
        len--;
        palCopyMemory( pMac->hHdd, pStartBssReq->extendedRateSet.rate,
                       pBuf, pStartBssReq->extendedRateSet.numRates);
        pBuf += pStartBssReq->extendedRateSet.numRates;
        len  -= pStartBssReq->extendedRateSet.numRates;
    }

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

        limLog(pMac,
               LOGW,
               FL("Going to parse numSSID  in the START_BSS_REQ, len=%d\n"), len);
    if (len < 2)
    {
        // No numSSID parameter in the START_BSS_REQ
        limLog(pMac,
               LOGW,
               FL("No numSSID  in the START_BSS_REQ, len=%d\n"), len);
        return eSIR_FAILURE;
    }

    // Extract numSSID
    pStartBssReq->numSSID = *pBuf++;
    len--;

    // Extract ssIdList[]
    for (i = 0; (i < pStartBssReq->numSSID) && len; i++)
    {
        pStartBssReq->ssIdList[i].length = *pBuf++;
        len--;
        if (len < pStartBssReq->ssIdList[i].length)
        {
            limLog(pMac, LOGW,
               FL("SSID length[%d] is more than the rem length[%d]\n"),
               pStartBssReq->ssIdList[i].length, len);
            return eSIR_FAILURE;
        }

        if (pStartBssReq->ssIdList[i].length > SIR_MAC_MAX_SSID_LENGTH)
        {
            // SSID length is more than max allowed 32 bytes
            limLog(pMac,
                   LOGW,
                   FL("Invalid SSID length in the list, len=%d\n"),
                   pStartBssReq->ssIdList[i].length);
            return eSIR_FAILURE;
        }

        palCopyMemory( pMac->hHdd, (tANI_U8 *) pStartBssReq->ssIdList[i].ssId,
                      pBuf,
                      pStartBssReq->ssIdList[i].length);
        pBuf += pStartBssReq->ssIdList[i].length;
        len  -= pStartBssReq->ssIdList[i].length;
    }
#endif

    if (len)
    {
        limLog(pMac, LOGW, FL("Extra bytes left in SME_START_BSS_REQ, len=%d\n"), len);
    }

    return eSIR_SUCCESS;
} /*** end limStartBssReqSerDes() ***/



/**
 * limStopBssReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_STOP_BSS_REQ from host
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
 * @param  pStopBssReq   Pointer to tSirSmeStopBssReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limStopBssReqSerDes(tpAniSirGlobal pMac, tpSirSmeStopBssReq pStopBssReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    pStopBssReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pStopBssReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_STOP_BSS_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pStopBssReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pStopBssReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16); 

    // Extract reasonCode
    pStopBssReq->reasonCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);
    len -= sizeof(tSirResultCodes);

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pStopBssReq->bssId, pBuf, sizeof(tSirMacAddr));
    len  -= sizeof(tSirMacAddr);
  
    if (len)
        return eSIR_FAILURE;
    else
        return eSIR_SUCCESS;

} /*** end limStopBssReqSerDes() ***/



/**
 * limJoinReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_JOIN_REQ from host
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
 * @param  pJoinReq  Pointer to tSirSmeJoinReq being extracted
 * @param  pBuf      Pointer to serialized buffer
 * @return retCode   Indicates whether message is successfully
 *                   de-serialized (eSIR_SUCCESS) or
 *                   not (eSIR_FAILURE)
 */

tSirRetStatus
limJoinReqSerDes(tpAniSirGlobal pMac, tpSirSmeJoinReq pJoinReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    tANI_S16 lenUsed = 0;

#ifdef PE_DEBUG_LOG1
     tANI_U8  *pTemp = pBuf;
#endif

    if (!pJoinReq || !pBuf)
    {
        PELOGE(limLog(pMac, LOGE, FL("NULL ptr received\n"));)
        return eSIR_FAILURE;
    }

    // Extract messageType
    pJoinReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    // Extract length
    len = pJoinReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (pJoinReq->messageType == eWNI_SME_JOIN_REQ)
        PELOG1(limLog(pMac, LOG3, FL("SME_JOIN_REQ length %d bytes is:\n"), len);)
    else
        PELOG1(limLog(pMac, LOG3, FL("SME_REASSOC_REQ length %d bytes is:\n"), len);)

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
    {
        PELOGE(limLog(pMac, LOGE, FL("len too short %d\n"), len);)
        return eSIR_FAILURE;
    }

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pJoinReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pJoinReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract ssId
    pJoinReq->ssId.length = *pBuf++;
    len--;
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->ssId.ssId, pBuf, pJoinReq->ssId.length);
    pBuf += pJoinReq->ssId.length;
    len -= pJoinReq->ssId.length;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract selfMacAddr
    palCopyMemory( pMac->hHdd, pJoinReq->selfMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bsstype
    pJoinReq->bsstype = (tSirBssType) limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract dot11mode
    pJoinReq->dot11mode= *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssPersona
    pJoinReq->staPersona = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract cbMode
    pJoinReq->cbMode = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract uapsdPerAcBitmask
    pJoinReq->uapsdPerAcBitmask = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    // Extract assocType
    pJoinReq->assocType = (tSirAssocType) limGetU32(pBuf);
    pBuf += sizeof(tSirAssocType);
    len  -= sizeof(tSirAssocType); 
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif

    // Extract operationalRateSet
    pJoinReq->operationalRateSet.numRates= *pBuf++;
    len--;
    if (pJoinReq->operationalRateSet.numRates)
    {
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->operationalRateSet.rate, pBuf, pJoinReq->operationalRateSet.numRates);
        pBuf += pJoinReq->operationalRateSet.numRates;
        len -= pJoinReq->operationalRateSet.numRates;
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }

    // Extract extendedRateSet
    pJoinReq->extendedRateSet.numRates = *pBuf++;
    len--;
    if (pJoinReq->extendedRateSet.numRates)
    {
        palCopyMemory( pMac->hHdd, pJoinReq->extendedRateSet.rate, pBuf, pJoinReq->extendedRateSet.numRates);
        pBuf += pJoinReq->extendedRateSet.numRates;
        len  -= pJoinReq->extendedRateSet.numRates;
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }

    // Extract RSN IE
    pJoinReq->rsnIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    if (pJoinReq->rsnIE.length)
    {
        // Check for RSN IE length (that includes length of type & length)
        if ((pJoinReq->rsnIE.length > SIR_MAC_MAX_IE_LENGTH + 2) ||
             (pJoinReq->rsnIE.length != 2 + *(pBuf + 1)))
        {
            limLog(pMac, LOGW,
                   FL("Invalid RSN IE length %d in SME_JOIN_REQ\n"),
                   pJoinReq->rsnIE.length);
            return eSIR_FAILURE;
        }
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->rsnIE.rsnIEdata,
                      pBuf, pJoinReq->rsnIE.length);
        pBuf += pJoinReq->rsnIE.length;
        len  -= pJoinReq->rsnIE.length; // skip RSN IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }

#ifdef FEATURE_WLAN_CCX
    // Extract CCKM IE
    pJoinReq->cckmIE.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);
    if (pJoinReq->cckmIE.length)
    {
        // Check for CCKM IE length (that includes length of type & length)
        if ((pJoinReq->cckmIE.length > SIR_MAC_MAX_IE_LENGTH) ||
             (pJoinReq->cckmIE.length != (2 + *(pBuf + 1))))
        {
            limLog(pMac, LOGW,
                   FL("Invalid CCKM IE length %d/%d in SME_JOIN/REASSOC_REQ\n"),
                   pJoinReq->cckmIE.length, 2 + *(pBuf + 1));
            return eSIR_FAILURE;
        }
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->cckmIE.cckmIEdata,
                      pBuf, pJoinReq->cckmIE.length);
        pBuf += pJoinReq->cckmIE.length;
        len  -= pJoinReq->cckmIE.length; // skip CCKM IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }
#endif

    // Extract Add IE for scan
    pJoinReq->addIEScan.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    if (pJoinReq->addIEScan.length)
    {
        // Check for IE length (that includes length of type & length)
        if (pJoinReq->addIEScan.length > SIR_MAC_MAX_IE_LENGTH + 2)
        {
            limLog(pMac, LOGE,
                   FL("Invalid addIE Scan length %d in SME_JOIN_REQ\n"),
                   pJoinReq->addIEScan.length);
            return eSIR_FAILURE;
        }
        // Check for P2P IE length (that includes length of type & length)
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->addIEScan.addIEdata,
                      pBuf, pJoinReq->addIEScan.length);
        pBuf += pJoinReq->addIEScan.length;
        len  -= pJoinReq->addIEScan.length; // skip add IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }

    pJoinReq->addIEAssoc.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    // Extract Add IE for assoc
    if (pJoinReq->addIEAssoc.length)
    {
        // Check for IE length (that includes length of type & length)
        if (pJoinReq->addIEAssoc.length > SIR_MAC_MAX_IE_LENGTH + 2)
        {
            limLog(pMac, LOGE,
                   FL("Invalid addIE Assoc length %d in SME_JOIN_REQ\n"),
                   pJoinReq->addIEAssoc.length);
            return eSIR_FAILURE;
        }
        // Check for P2P IE length (that includes length of type & length)
        palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->addIEAssoc.addIEdata,
                      pBuf, pJoinReq->addIEAssoc.length);
        pBuf += pJoinReq->addIEAssoc.length;
        len  -= pJoinReq->addIEAssoc.length; // skip add IE
        if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
            return eSIR_FAILURE;
    }

    pJoinReq->MCEncryptionType = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    
    
    pJoinReq->UCEncryptionType = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    
    
#ifdef WLAN_FEATURE_VOWIFI_11R
    //is11Rconnection;
    pJoinReq->is11Rconnection = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    
#endif

#ifdef FEATURE_WLAN_CCX
    //isCCXconnection;
    pJoinReq->isCCXconnection = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    

    // TSPEC information
    pJoinReq->ccxTspecInfo.numTspecs = *pBuf++;
    len -= sizeof(tANI_U8);
    palCopyMemory(pMac->hHdd, (void*)&pJoinReq->ccxTspecInfo.tspec[0], pBuf, (sizeof(tTspecInfo)* pJoinReq->ccxTspecInfo.numTspecs));
    pBuf += sizeof(tTspecInfo)*SIR_CCX_MAX_TSPEC_IES;
    len  -= sizeof(tTspecInfo)*SIR_CCX_MAX_TSPEC_IES;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif
    
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
    //isFastTransitionEnabled;
    pJoinReq->isFastTransitionEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    
#endif

#ifdef FEATURE_WLAN_LFR
    //isFastRoamIniFeatureEnabled;
    pJoinReq->isFastRoamIniFeatureEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;    
#endif

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    // Extract BP Indicator
    pJoinReq->bpIndicator = (tAniBool) limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len  -= sizeof(tAniBool); 
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract BP Type
    pJoinReq->bpType = (tSirBpIndicatorType) limGetU32(pBuf);
    pBuf += sizeof(tSirBpIndicatorType);
    len  -= sizeof(tSirBpIndicatorType); 
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract Neighbor BSS List
    if (limGetNeighborBssList(pMac, &pJoinReq->neighborBssList,
                              len, &lenUsed, pBuf) == eSIR_FAILURE)
    {
        PELOGE(limLog(pMac, LOGE, FL("get neighbor bss list failed\n"));)
        return eSIR_FAILURE;
    }
    pBuf += lenUsed;
    len -= lenUsed;
    PELOG1(limLog(pMac, LOG1, FL("Assoc Type %d RSN len %d bp %d type %d bss RSN len %d\n"),
           pJoinReq->assocType, pJoinReq->rsnIE.length, pJoinReq->bpIndicator,
           pJoinReq->bpType, pJoinReq->neighborBssList.bssList->rsnIE.length);)
#endif

    // Extract Titan CB Neighbor BSS info
    pJoinReq->cbNeighbors.cbBssFoundPri = *pBuf;
    pBuf++;
    pJoinReq->cbNeighbors.cbBssFoundSecUp = *pBuf;
    pBuf++;
    pJoinReq->cbNeighbors.cbBssFoundSecDown = *pBuf;
    pBuf++;
    len -= 3;

    // Extract Spectrum Mgt Indicator
    pJoinReq->spectrumMgtIndicator = (tAniBool) limGetU32(pBuf);
    pBuf += sizeof(tAniBool);       
    len -= sizeof(tAniBool);

    pJoinReq->powerCap.minTxPower = *pBuf++;
    pJoinReq->powerCap.maxTxPower = *pBuf++;
    len -=2;
    limLog(pMac, LOG1, FL("Power Caps: Min power = %d, Max power = %d\n"), pJoinReq->powerCap.minTxPower, pJoinReq->powerCap.maxTxPower);

    pJoinReq->supportedChannels.numChnl = *pBuf++;
    len--;
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pJoinReq->supportedChannels.channelList,
                      pBuf, pJoinReq->supportedChannels.numChnl);
    pBuf += pJoinReq->supportedChannels.numChnl;
    len-= pJoinReq->supportedChannels.numChnl;

    PELOG2(limLog(pMac, LOG2,
            FL("spectrumInd ON: minPower %d, maxPower %d , numChnls %d\n"),
            pJoinReq->powerCap.minTxPower,
            pJoinReq->powerCap.maxTxPower,
            pJoinReq->supportedChannels.numChnl);)

    // Extract uapsdPerAcBitmask
    pJoinReq->uapsdPerAcBitmask = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if (WNI_POLARIS_FW_PRODUCT == WLAN_STA)
    //
    // NOTE - tSirBssDescription is now moved to the end
    // of tSirSmeJoinReq structure. This is to accomodate
    // the variable length data member ieFields[1]
    //
    if (limGetBssDescription( pMac, &pJoinReq->bssDescription,
                             len, &lenUsed, pBuf) == eSIR_FAILURE)
    {
        PELOGE(limLog(pMac, LOGE, FL("get bss description failed\n"));)
        return eSIR_FAILURE;
    }
    PELOG3(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, (tANI_U8 *) &(pJoinReq->bssDescription), pJoinReq->bssDescription.length + 2);)
    pBuf += lenUsed;
    len -= lenUsed;
#endif

    return eSIR_SUCCESS;
} /*** end limJoinReqSerDes() ***/


/**---------------------------------------------------------------
\fn     limAssocIndSerDes
\brief  This function is called by limProcessMlmAssocInd() to 
\       populate the SME_ASSOC_IND message based on the received
\       MLM_ASSOC_IND.
\
\param pMac
\param pAssocInd - Pointer to the received tLimMlmAssocInd
\param pBuf - Pointer to serialized buffer
\param psessionEntry - pointer to PE session entry
\
\return None
------------------------------------------------------------------*/
void
limAssocIndSerDes(tpAniSirGlobal pMac, tpLimMlmAssocInd pAssocInd, tANI_U8 *pBuf, tpPESession psessionEntry)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    tANI_U32 len = 0;
#endif

    mLen   = sizeof(tANI_U32);
    mLen   += sizeof(tANI_U8);
    pBuf  += sizeof(tANI_U16);
    *pBuf = psessionEntry->smeSessionId;
    pBuf   += sizeof(tANI_U8);

     // Fill in peerMacAddr
    palCopyMemory( pMac->hHdd, pBuf, pAssocInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in aid
    limCopyU16(pBuf, pAssocInd->aid);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

   // Fill in bssId
    palCopyMemory( pMac->hHdd, pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in staId
    limCopyU16(pBuf, psessionEntry->staId);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    // Fill in authType
    limCopyU32(pBuf, pAssocInd->authType);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    // Fill in ssId
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pAssocInd->ssId), pAssocInd->ssId.length + 1);
    pBuf += (1 + pAssocInd->ssId.length);
    mLen += (1 + pAssocInd->ssId.length);

    // Fill in rsnIE
    limCopyU16(pBuf, pAssocInd->rsnIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pAssocInd->rsnIE.rsnIEdata),
                  pAssocInd->rsnIE.length);
    pBuf += pAssocInd->rsnIE.length;
    mLen += pAssocInd->rsnIE.length;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)

    limCopyU16(pBuf, pAssocInd->seqNum);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    limCopyU32(pBuf, pAssocInd->wniIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    limCopyU32(pBuf, pAssocInd->bpIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    limCopyU32(pBuf, pAssocInd->bpType);
    pBuf += sizeof(tSirBpIndicatorType);
    mLen += sizeof(tSirBpIndicatorType);

    limCopyU32(pBuf, pAssocInd->assocType);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    limCopyLoad(pBuf, pAssocInd->load);
    pBuf += sizeof(tSirLoad);
    mLen += sizeof(tSirLoad);

    limCopyU32(pBuf, pAssocInd->numBss);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    if (pAssocInd->numBss)
    {
        len   = limCopyNeighborList(pMac,
                           pBuf,
                           pAssocInd->neighborList, pAssocInd->numBss);
        pBuf += len;
        mLen += (tANI_U16) len;
    }

    // place holder to capability and nwType

    limCopyU16(pBuf, *(tANI_U16 *)&pAssocInd->capabilityInfo);
    pBuf += sizeof(tANI_U16); // capabilityInfo
    mLen += sizeof(tANI_U16);

    limCopyU32(pBuf, *(tANI_U32 *)&pAssocInd->nwType);
    pBuf += sizeof(tANI_U32); // nwType
    mLen += sizeof(tANI_U32);

#endif

    limCopyU32(pBuf, pAssocInd->spectrumMgtIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    if (pAssocInd->spectrumMgtIndicator == eSIR_TRUE)
    {
        *pBuf = pAssocInd->powerCap.minTxPower;
        pBuf++;
        *pBuf = pAssocInd->powerCap.maxTxPower;
        pBuf++;
        mLen += sizeof(tSirMacPowerCapInfo);

        *pBuf = pAssocInd->supportedChannels.numChnl;
        pBuf++;
        mLen++;

      palCopyMemory( pMac->hHdd, pBuf,
                     (tANI_U8 *) &(pAssocInd->supportedChannels.channelList),
                     pAssocInd->supportedChannels.numChnl);


        pBuf += pAssocInd->supportedChannels.numChnl;
        mLen += pAssocInd->supportedChannels.numChnl;
    }
#ifdef WLAN_SOFTAP_FEATURE
    limCopyU32(pBuf, pAssocInd->WmmStaInfoPresent);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);
#endif
     // Fill in length of SME_ASSOC_IND message
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_ASSOC_IND length %d bytes:\n"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limAssocIndSerDes() ***/



/**
 * limAssocCnfSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() when
 * SME_ASSOC_CNF or SME_REASSOC_CNF message is received from
 * upper layer software.
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
 * @param  pAssocCnf  Pointer to tSirSmeAssocCnf being extracted into
 * @param  pBuf       Pointer to serialized buffer
 * @return retCode    Indicates whether message is successfully
 *                    de-serialized (eSIR_SUCCESS) or
 *                    not (eSIR_FAILURE)
 */

tSirRetStatus
limAssocCnfSerDes(tpAniSirGlobal pMac, tpSirSmeAssocCnf pAssocCnf, tANI_U8 *pBuf)
{
#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pAssocCnf || !pBuf)
        return eSIR_FAILURE;

    pAssocCnf->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    pAssocCnf->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (pAssocCnf->messageType == eWNI_SME_ASSOC_CNF)
    {
        PELOG1(limLog(pMac, LOG1, FL("SME_ASSOC_CNF length %d bytes is:\n"), pAssocCnf->length);)
    }
    else
    {
        PELOG1(limLog(pMac, LOG1, FL("SME_REASSOC_CNF length %d bytes is:\n"), pAssocCnf->length);)
    }
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, pAssocCnf->length);)

    // status code
    pAssocCnf->statusCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);

    // bssId
    palCopyMemory( pMac->hHdd, pAssocCnf->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);

    // peerMacAddr
    palCopyMemory( pMac->hHdd, pAssocCnf->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);


    pAssocCnf->aid = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    // alternateBssId
    palCopyMemory( pMac->hHdd, pAssocCnf->alternateBssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);

    // alternateChannelId
    pAssocCnf->alternateChannelId = *pBuf;
    pBuf++;

    return eSIR_SUCCESS;
} /*** end limAssocCnfSerDes() ***/



/**
 * limDisassocCnfSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() when
 * SME_DISASSOC_CNF message is received from upper layer software.
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
 * @param  pDisassocCnf  Pointer to tSirSmeDisassocCnf being
 *                       extracted into
 * @param  pBuf          Pointer to serialized buffer
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limDisassocCnfSerDes(tpAniSirGlobal pMac, tpSirSmeDisassocCnf pDisassocCnf, tANI_U8 *pBuf)
{
#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDisassocCnf || !pBuf)
        return eSIR_FAILURE;

    pDisassocCnf->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    pDisassocCnf->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DISASSOC_CNF length %d bytes is:\n"), pDisassocCnf->length);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, pDisassocCnf->length);)

    pDisassocCnf->statusCode = (tSirResultCodes) limGetU32(pBuf);
    pBuf += sizeof(tSirResultCodes);

    palCopyMemory( pMac->hHdd, pDisassocCnf->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
 
    palCopyMemory( pMac->hHdd, pDisassocCnf->peerMacAddr, pBuf, sizeof(tSirMacAddr));

#if defined(ANI_PRODUCT_TYPE_AP)
    pBuf += sizeof(tSirMacAddr);
    pDisassocCnf->aid = limGetU16(pBuf);
#endif
    
    return eSIR_SUCCESS;
} /*** end limDisassocCnfSerDes() ***/



#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/**
 * limMeasurementReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() when
 * SME_MEASUREMENT_REQ message is received from upper layer software.
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
 * @param  pMeasReq  Pointer to tSirSmeMeasurement being
 *                   extracted into
 * @param  pBuf      Pointer to serialized buffer
 * @return retCode   Indicates whether message is successfully
 *                   de-serialized (eSIR_SUCCESS) or
 *                   not (eSIR_FAILURE)
 */

tSirRetStatus
limMeasurementReqSerDes(tpAniSirGlobal pMac, tpSirSmeMeasurementReq pMeasReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    PELOG3(tANI_U8  *pTemp = pBuf;)

    if (!pMeasReq || !pBuf)
        return eSIR_FAILURE;

    pMeasReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pMeasReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG3(limLog(pMac, LOG3, FL("SME_MEASUREMENT_REQ length %d bytes is:\n"), len);
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.periodicMeasEnabled = (tAniBool)
                                                limGetU32(pBuf);

    pBuf += sizeof(tAniBool);
    len  -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.involveSTAs = (tAniBool)
                                        limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len  -= sizeof(tAniBool);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.metricsType = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.scanType = (tSirScanType)
                                     limGetU32(pBuf);
    pBuf += sizeof(tSirScanType);
    len  -= sizeof(tSirScanType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.longChannelScanPeriodicity = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measControl.cb11hEnabled = (tAniBool)limGetU32(pBuf);
    pBuf += sizeof(tAniBool);
    len  -= sizeof(tAniBool);

    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measDuration.shortTermPeriod = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    pMeasReq->measDuration.averagingPeriod = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    pMeasReq->measDuration.shortChannelScanDuration = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    pMeasReq->measDuration.longChannelScanDuration = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    len  -= sizeof(tSirMeasDuration);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->measIndPeriod = limGetU32(pBuf);

    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pMeasReq->channelList.numChannels = *pBuf++;
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pMeasReq->channelList.channelNumber,
                  pBuf, pMeasReq->channelList.numChannels);

    return eSIR_SUCCESS;
} /*** end limMeasurementReqSerDes() ***/


/**
 * limWdsReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() when
 * SME_SET_WDS_INFO_REQ message is received from upper layer software.
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
 * @param  pWdsInfo  Pointer to tSirWdsInfo being extracted into
 * @param  pBuf      Pointer to serialized buffer
 * @return retCode   Indicates whether message is successfully
 *                   de-serialized (eSIR_SUCCESS) or
 *                   not (eSIR_FAILURE)
 */

tSirRetStatus
limWdsReqSerDes(tpAniSirGlobal pMac, tpSirSmeSetWdsInfoReq pWdsInfoReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    PELOG1(tANI_U8  *pTemp = pBuf;)

    if (!pWdsInfoReq || !pBuf)
        return eSIR_FAILURE;

    // Extract messageType
    pWdsInfoReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    // Extract length
    len = pWdsInfoReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_SET_WDS_INFO_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header

    // Extract transactionId
    pWdsInfoReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);

    // Extract wdsInfo
    pWdsInfoReq->wdsInfo.wdsLength = limGetU16(pBuf);
    len -= sizeof(tANI_U16);
    pBuf += sizeof(tANI_U16);

    if (pWdsInfoReq->wdsInfo.wdsLength > ANI_WDS_INFO_MAX_LENGTH)
        return eSIR_FAILURE;

    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pWdsInfoReq->wdsInfo.wdsBytes,
        pBuf,
        pWdsInfoReq->wdsInfo.wdsLength);

    return eSIR_SUCCESS;

} /*** end limWdsReqSerDes() ***/


/**
 * limMeasurementIndSerDes()
 *
 *FUNCTION:
 * This function is called by limSendMeasurementInd() while
 * sending SME_MEASUREMENT_IND message to WSM.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac     - Pointer to Global MAC structure
 * @param  pMeasInd - Pointer to prepared MEASUREMENT_IND message
 * @return None
 */

void
limMeasurementIndSerDes(tpAniSirGlobal pMac, tANI_U8 *pBuf)
{
    tANI_U8          *pLen;
    tANI_U16         len = 0, infoLen = 0;
    tSirLoad    load;
    PELOG3(tANI_U8  *pTemp = pBuf;)

    limCopyU16(pBuf, eWNI_SME_MEASUREMENT_IND);
    pBuf += sizeof(tANI_U16);

    pLen  = pBuf;
    pBuf += sizeof(tANI_U16);

    limCopyU32(pBuf, pMac->lim.gpLimMeasData->duration);
    pBuf += sizeof(tANI_U32);
    len += sizeof(tANI_U32);

    load.numStas            = pMac->lim.gLimNumOfCurrentSTAs;
    load.channelUtilization =
                        pMac->lim.gpLimMeasData->avgChannelUtilization;
    limCopyLoad(pBuf, load);
    pBuf += sizeof(tSirLoad);
    len  += sizeof(tSirLoad);

    infoLen = limCopyMeasMatrixList(pMac, pBuf);
    pBuf   += infoLen;
    len    += infoLen;

    infoLen = limCopyNeighborWdsList(pMac, pBuf);
    pBuf   += infoLen;
    len    += infoLen;

    limCopyU16(pLen, len+4);

    PELOG3(limLog(pMac, LOG3, FL("Sending MEAS_IND length %d bytes:\n"), len);
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)
} /*** end limMeasurementIndSerDes() ***/
#endif


/**---------------------------------------------------------------
\fn     limReassocIndSerDes
\brief  This function is called by limProcessMlmReassocInd() to 
\       populate the SME_REASSOC_IND message based on the received
\       MLM_REASSOC_IND.
\
\param pMac
\param pReassocInd - Pointer to the received tLimMlmReassocInd
\param pBuf - Pointer to serialized buffer
\param psessionEntry - pointer to PE session entry
\
\return None
------------------------------------------------------------------*/
void
limReassocIndSerDes(tpAniSirGlobal pMac, tpLimMlmReassocInd pReassocInd, tANI_U8 *pBuf, tpPESession psessionEntry)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    tANI_U32 len = 0;
#endif

    mLen   = sizeof(tANI_U32);
    pBuf  += sizeof(tANI_U16);
    *pBuf++ = psessionEntry->smeSessionId;
    mLen += sizeof(tANI_U8);

    // Fill in peerMacAddr
    palCopyMemory( pMac->hHdd, pBuf, pReassocInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in oldMacAddr
    palCopyMemory( pMac->hHdd, pBuf, pReassocInd->currentApAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in aid
    limCopyU16(pBuf, pReassocInd->aid);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
 
    // Fill in bssId
    palCopyMemory( pMac->hHdd, pBuf, psessionEntry->bssId, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    // Fill in staId
    limCopyU16(pBuf, psessionEntry->staId);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    // Fill in authType
    limCopyU32(pBuf, pReassocInd->authType);
    pBuf += sizeof(tAniAuthType);
    mLen += sizeof(tAniAuthType);

    // Fill in ssId
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pReassocInd->ssId),
                  pReassocInd->ssId.length + 1);
    pBuf += 1 + pReassocInd->ssId.length;
    mLen += pReassocInd->ssId.length + 1;

    // Fill in rsnIE
    limCopyU16(pBuf, pReassocInd->rsnIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *) &(pReassocInd->rsnIE.rsnIEdata),
                  pReassocInd->rsnIE.length);
    pBuf += pReassocInd->rsnIE.length;
    mLen += pReassocInd->rsnIE.length;

    // Fill in addIE
    limCopyU16(pBuf, pReassocInd->addIE.length);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8*) &(pReassocInd->addIE.addIEdata),
                   pReassocInd->addIE.length);
    pBuf += pReassocInd->addIE.length;
    mLen += pReassocInd->addIE.length;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)

    limCopyU16(pBuf, pReassocInd->seqNum);
    pBuf += sizeof(tANI_U16);
    mLen += sizeof(tANI_U16);

    limCopyU32(pBuf, pReassocInd->wniIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    limCopyU32(pBuf, pReassocInd->bpIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    limCopyU32(pBuf, pReassocInd->bpType);
    pBuf += sizeof(tSirBpIndicatorType);
    mLen += sizeof(tSirBpIndicatorType);

    limCopyU32(pBuf, pReassocInd->reassocType);
    pBuf += sizeof(tSirAssocType);
    mLen += sizeof(tSirAssocType);

    limCopyLoad(pBuf, pReassocInd->load);
    pBuf += sizeof(tSirLoad);
    mLen += sizeof(tSirLoad);

    limCopyU32(pBuf, pReassocInd->numBss);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);

    if (pReassocInd->numBss)
    {
        len   = limCopyNeighborList(pMac,
                       pBuf,
                       pReassocInd->neighborList, pReassocInd->numBss);
        pBuf += len;
        mLen += (tANI_U16) len;
    }

    // place holder to capability and nwType
    limCopyU16(pBuf, *(tANI_U16 *)&pReassocInd->capabilityInfo);
    pBuf += sizeof(tANI_U16); // capabilityInfo
    mLen += sizeof(tANI_U16);

    limCopyU32(pBuf, *(tANI_U32 *)&pReassocInd->nwType);
    pBuf += sizeof(tANI_U32); // nwType
    mLen += sizeof(tANI_U32);
#endif

    limCopyU32(pBuf, pReassocInd->spectrumMgtIndicator);
    pBuf += sizeof(tAniBool);
    mLen += sizeof(tAniBool);

    if (pReassocInd->spectrumMgtIndicator == eSIR_TRUE)
    {
        *pBuf = pReassocInd->powerCap.minTxPower;
        pBuf++;
        *pBuf = pReassocInd->powerCap.maxTxPower;
        pBuf++;
        mLen += sizeof(tSirMacPowerCapInfo);

        *pBuf = pReassocInd->supportedChannels.numChnl;
        pBuf++;
        mLen++;

        palCopyMemory( pMac->hHdd, pBuf,
                       (tANI_U8 *) &(pReassocInd->supportedChannels.channelList),
                       pReassocInd->supportedChannels.numChnl);

        pBuf += pReassocInd->supportedChannels.numChnl;
        mLen += pReassocInd->supportedChannels.numChnl;
    }
#ifdef WLAN_SOFTAP_FEATURE
    limCopyU32(pBuf, pReassocInd->WmmStaInfoPresent);
    pBuf += sizeof(tANI_U32);
    mLen += sizeof(tANI_U32);
#endif

    // Fill in length of SME_REASSOC_IND message
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_REASSOC_IND length %d bytes:\n"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limReassocIndSerDes() ***/


/**
 * limAuthIndSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessMlmAuthInd() while sending
 * SME_AUTH_IND to host
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
 * @param  pAuthInd          Pointer to tSirSmeAuthInd being sent
 * @param  pBuf         Pointer to serialized buffer
 *
 * @return None
 */

void
limAuthIndSerDes(tpAniSirGlobal pMac, tpLimMlmAuthInd pAuthInd, tANI_U8 *pBuf)
{
    tANI_U8  *pLen  = pBuf;
    tANI_U16 mLen = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    mLen   = sizeof(tANI_U32);
    pBuf  += sizeof(tANI_U16);
    *pBuf++ = pAuthInd->sessionId;
    mLen += sizeof(tANI_U8);

    // BTAMP TODO:  Fill in bssId
    palZeroMemory(pMac->hHdd, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    palCopyMemory( pMac->hHdd, pBuf, pAuthInd->peerMacAddr, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    mLen += sizeof(tSirMacAddr);

    limCopyU32(pBuf, pAuthInd->authType);
    pBuf += sizeof(tAniAuthType);
    mLen += sizeof(tAniAuthType);
  
    limCopyU16(pLen, mLen);

    PELOG1(limLog(pMac, LOG1, FL("Sending SME_AUTH_IND length %d bytes:\n"), mLen);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, mLen);)
} /*** end limAuthIndSerDes() ***/



/**
 * limSetContextReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_SETCONTEXT_REQ from host
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
 * @param  pSetContextReq  Pointer to tSirSmeSetContextReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limSetContextReqSerDes(tpAniSirGlobal pMac, tpSirSmeSetContextReq pSetContextReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
    tANI_U16 totalKeySize = sizeof(tANI_U8); // initialized to sizeof numKeys
    tANI_U8  numKeys;
    tANI_U8 *pKeys;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif
    if (!pSetContextReq || !pBuf)
        return eSIR_FAILURE;

    pSetContextReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pSetContextReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_SETCONTEXT_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG3, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pSetContextReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pSetContextReq->transactionId = sirReadU16N(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pSetContextReq->peerMacAddr,
                   pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    palCopyMemory( pMac->hHdd, pSetContextReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if defined(ANI_PRODUCT_TYPE_AP)
    pSetContextReq->aid = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif

//    pSetContextReq->qosInfoPresent = limGetU32(pBuf);
//    pBuf += sizeof(tAniBool);

//    if (pSetContextReq->qosInfoPresent)
//    {
//        len   = limGetQosInfo(&pSetContextReq->qos, pBuf);
//        pBuf += len;
//    }

    pSetContextReq->keyMaterial.length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pSetContextReq->keyMaterial.edType = (tAniEdType) limGetU32(pBuf);
    pBuf += sizeof(tAniEdType);
    len  -= sizeof(tAniEdType);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    numKeys = pSetContextReq->keyMaterial.numKeys = *pBuf++;
    len  -= sizeof(numKeys);

    if (numKeys > SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)
        return eSIR_FAILURE;

    /** Initialize the Default Keys if no Keys are being sent from the upper layer*/
    if( limCheckRemainingLength(pMac, len) == eSIR_FAILURE) {
        tpSirKeys pKeyinfo = pSetContextReq->keyMaterial.key;

        pKeyinfo->keyId = 0;
        pKeyinfo->keyDirection = eSIR_TX_RX; 
        pKeyinfo->keyLength = 0;
            
        if (!limIsAddrBC(pSetContextReq->peerMacAddr))
            pKeyinfo->unicast = 1;
        else
            pKeyinfo->unicast = 0;             
    }else {
        pKeys  = (tANI_U8 *) pSetContextReq->keyMaterial.key;
        do {
            tANI_U32 keySize   = limGetKeysInfo(pMac, (tpSirKeys) pKeys,
                                       pBuf);
            pBuf         += keySize;
            pKeys        += sizeof(tSirKeys);
            totalKeySize += (tANI_U16) keySize;
            if (numKeys == 0)
                break;
            numKeys--;            
        }while (numKeys);
    }
    return eSIR_SUCCESS;
} /*** end limSetContextReqSerDes() ***/

/**
 * limRemoveKeyReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_REMOVEKEY_REQ from host
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
 * @param  pRemoveKeyReq  Pointer to tSirSmeRemoveKeyReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limRemoveKeyReqSerDes(tpAniSirGlobal pMac, tpSirSmeRemoveKeyReq pRemoveKeyReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef    PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif
    if (!pRemoveKeyReq || !pBuf)
        return eSIR_FAILURE;

    pRemoveKeyReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pRemoveKeyReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_REMOVEKEY_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) pRemoveKeyReq->peerMacAddr,
                  pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if defined(ANI_PRODUCT_TYPE_AP)
    pRemoveKeyReq->aid = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
#endif

    pRemoveKeyReq->edType = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->wepType = *pBuf;

    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->keyId = *pBuf;
    
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pRemoveKeyReq->unicast = *pBuf;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, pRemoveKeyReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pRemoveKeyReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pRemoveKeyReq->transactionId = sirReadU16N(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    
    return eSIR_SUCCESS;
} /*** end limRemoveKeyReqSerDes() ***/



/**
 * limDisassocReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_DISASSOC_REQ from host
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
 * @param  pDisassocReq  Pointer to tSirSmeDisassocReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 *
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */

tSirRetStatus
limDisassocReqSerDes(tpAniSirGlobal pMac, tSirSmeDisassocReq *pDisassocReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;
#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDisassocReq || !pBuf)
        return eSIR_FAILURE;

    pDisassocReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pDisassocReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DISASSOC_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionID
    pDisassocReq->sessionId = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionid
    pDisassocReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pDisassocReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract peerMacAddr
    palCopyMemory( pMac->hHdd, pDisassocReq->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract reasonCode
    pDisassocReq->reasonCode = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    pDisassocReq->doNotSendOverTheAir = *pBuf;
    pBuf += sizeof(tANI_U8);
    len -= sizeof(tANI_U8);

#if defined(ANI_PRODUCT_TYPE_AP)
    pDisassocReq->aid = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    pDisassocReq->seqNum = limGetU16(pBuf);
#endif
#endif

    return eSIR_SUCCESS;
} /*** end limDisassocReqSerDes() ***/



/**
 * limDeauthReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_DEAUTH_REQ from host
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
 * @param  pDeauthReq           Pointer to tSirSmeDeauthReq being extracted
 * @param  pBuf          Pointer to serialized buffer
 *
 * @return retCode       Indicates whether message is successfully
 *                       de-serialized (eSIR_SUCCESS) or
 *                       not (eSIR_FAILURE)
 */
tSirRetStatus
limDeauthReqSerDes(tpAniSirGlobal pMac, tSirSmeDeauthReq *pDeauthReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    if (!pDeauthReq || !pBuf)
        return eSIR_FAILURE;

    pDeauthReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pDeauthReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_DEAUTH_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pDeauthReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pDeauthReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, pDeauthReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract peerMacAddr
    palCopyMemory( pMac->hHdd, pDeauthReq->peerMacAddr, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract reasonCode
    pDeauthReq->reasonCode = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);

#if defined(ANI_PRODUCT_TYPE_AP)
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
    pDeauthReq->aid = limGetU16(pBuf);
#endif
    
    return eSIR_SUCCESS;
} /*** end limDisassocReqSerDes() ***/




#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/**
 * limCopyNeighborInfoToCfg()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * SME_JOIN_REQ/SME_REASSOC_REQ from WSM to convert neighborBssInfo
 * that is being joined to bssDescription used by MLM
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
 * @param  neighborBssInfo  Neighbor BSS info to be converted
 *
 * @return None
 */

void
limCopyNeighborInfoToCfg(tpAniSirGlobal pMac, tSirNeighborBssInfo neighborBssInfo, tpPESession psessionEntry)
{
    tANI_U32    localPowerConstraints = 0;
    tANI_U16    caps;
    tANI_U8     qosCap = 0;

    if(psessionEntry)
    {
        psessionEntry->gLimPhyMode = neighborBssInfo.nwType;
    }
    else
    {
        pMac->lim.gLimPhyMode = neighborBssInfo.nwType;
    }

    cfgSetCapabilityInfo(pMac, neighborBssInfo.capabilityInfo);

    if (cfgSetStr(pMac, WNI_CFG_SSID, (tANI_U8 *) &neighborBssInfo.ssId.ssId,
                  neighborBssInfo.ssId.length) != eSIR_SUCCESS)
    {
        /// Could not update SSID at CFG. Log error.
        limLog(pMac, LOGP, FL("could not update SSID at CFG\n"));
    }

    #if 0
    if (cfgSetStr(
           pMac, WNI_CFG_OPERATIONAL_RATE_SET,
           (tANI_U8 *) &neighborBssInfo.operationalRateSet.rate,
           neighborBssInfo.operationalRateSet.numRates) != eSIR_SUCCESS)
    {
        /// Could not update Operational Rateset at CFG. Log error.
        limLog(pMac, LOGP,
               FL("could not update Operational Rateset at CFG\n"));
    }
    #endif // TO SUPPORT BT-AMP

    if (neighborBssInfo.nwType == WNI_CFG_PHY_MODE_11G)
    {
        if (cfgSetStr(
            pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
            (tANI_U8 *) &neighborBssInfo.extendedRateSet.rate,
            neighborBssInfo.extendedRateSet.numRates) != eSIR_SUCCESS)
        {
            /// Could not update Extended Rateset at CFG. Log error.
            limLog(pMac, LOGP,
                   FL("could not update Extended Rateset at CFG\n"));
        }
    }
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    if (neighborBssInfo.hcfEnabled)
        LIM_BSS_CAPS_SET(HCF, qosCap);
#endif
    if (neighborBssInfo.wmeInfoPresent || neighborBssInfo.wmeEdcaPresent)
        LIM_BSS_CAPS_SET(WME, qosCap);
    if (LIM_BSS_CAPS_GET(WME, qosCap) && neighborBssInfo.wsmCapablePresent)
        LIM_BSS_CAPS_SET(WSM, qosCap);
    
    pMac->lim.gLimCurrentBssQosCaps = qosCap;
    
    if (neighborBssInfo.wniIndicator)
        pMac->lim.gLimCurrentBssPropCap = neighborBssInfo.propIECapability;
    else
        pMac->lim.gLimCurrentBssPropCap = 0;

    if (neighborBssInfo.HTCapsPresent)
        pMac->lim.htCapabilityPresentInBeacon = 1;
    else
        pMac->lim.htCapabilityPresentInBeacon = 0;
    if (neighborBssInfo.localPowerConstraints && pSessionEntry->lim11hEnable)
    {
        localPowerConstraints = neighborBssInfo.localPowerConstraints;
    }
    if (cfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, localPowerConstraints) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Could not update local power constraint to cfg.\n"));
    }
} /*** end limCopyNeighborInfoToCfg() ***/
#endif


/**
 * limStatSerDes()
 *
 *FUNCTION:
 * This function is called by limSendSmeDisassocNtf() while sending
 * SME_DISASSOC_IND/eWNI_SME_DISASSOC_RSP to host
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
 * @param  pAssocInd  Pointer to tpAniStaStatStruct being sent
 * @param  pBuf       Pointer to serialized buffer
 *
 * @return None
 */

void
limStatSerDes(tpAniSirGlobal pMac, tpAniStaStatStruct pStat, tANI_U8 *pBuf)
{
#ifdef  PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    limCopyU32(pBuf, pStat->sentAesBlksUcastHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->sentAesBlksUcastLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->recvAesBlksUcastHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->recvAesBlksUcastLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesFormatErrorUcastCnts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesReplaysUcast);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->aesDecryptErrUcast);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->singleRetryPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->failedTxPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->ackTimeouts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->multiRetryPkts);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->fragTxCntsHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->fragTxCntsLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->transmittedPktsHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->transmittedPktsLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->phyStatHi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->phyStatLo);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkRssi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkSinr);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->uplinkRate);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkRssi);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkSinr);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->downlinkRate);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->nRcvBytes);
    pBuf += sizeof(tANI_U32);

    limCopyU32(pBuf, pStat->nXmitBytes);
    pBuf += sizeof(tANI_U32);

    PELOG1(limLog(pMac, LOG1, FL("STAT: length %d bytes is:\n"), sizeof(tAniStaStatStruct));)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp,  sizeof(tAniStaStatStruct));)

} /*** end limStatSerDes() ***/


#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/**
 * limSmeWmStatusChangeNtfSerDes()
 *
 *FUNCTION:
 * This function is called by limSendSmeWmStatusChangeNtf()
 * to serialize the header fields of SME WM Status Change
 * Notification
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  statusChangeCode     Status Change code
 * @param  pBuf                 Pointer to serialized buffer
 * @param  len                  Pointer to length actually used
 * @param  buflen               Buffer length remaining
 * @return retCode              Indicates whether message is successfully
 *                              serialized (eSIR_SUCCESS) or not (eSIR_FAILURE)
 */

tSirRetStatus
limSmeWmStatusChangeHeaderSerDes(tpAniSirGlobal pMac,
                                 tSirSmeStatusChangeCode statusChangeCode,
                                 tANI_U8 *pBuf,
                                 tANI_U16 *len,
                                 tANI_U32 buflen,
                                 tANI_U8 sessionId)
{
    if (buflen < ((sizeof(tANI_U16) * 2) + sizeof(tANI_U32)))
    {
        limLog(pMac, LOGE,
               FL("limSmeWmStatusChangeHeaderSerDes: no enough space (buf %d) \n"), buflen);
        return eSIR_FAILURE;
    }

    *len = 0;

    limCopyU16(pBuf, eWNI_SME_WM_STATUS_CHANGE_NTF);
    pBuf += sizeof(tANI_U16);
    *len += sizeof(tANI_U16);

    // Actual length value shall be filled later
    pBuf += sizeof(tANI_U16);
    *len += sizeof(tANI_U16);
    
    *pBuf++ = sessionId;
    *len += sizeof(tANI_U8);

    limCopyU32(pBuf, statusChangeCode);
    pBuf += sizeof(tANI_U32);
    *len += sizeof(tANI_U32);

    return eSIR_SUCCESS;
} /*** end limSmeWmStatusChangeHeaderSerDes() ***/



/**
 * limRadioInfoSerDes()
 *
 *FUNCTION:
 * This function is called by limSendSmeWmStatusChangeNtf()
 * to serialize the radarInfo message
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pRadarInfo    Pointer to tSirRadarInfo
 * @param  pBuf          Pointer to serialized buffer
 * @param  len           Pointer to length actually used by tSirRadarInfo
 * @param  buflen        Length remaining
 *
 * @return retCode       Indicates whether message is successfully
 *                       serialized (eSIR_SUCCESS) or not (eSIR_FAILURE)
 */

tSirRetStatus
limRadioInfoSerDes(tpAniSirGlobal pMac, tpSirRadarInfo pRadarInfo,
                              tANI_U8 *pBuf, tANI_U16 *len, tANI_U32 buflen)
{

    if (buflen < sizeof(tSirRadarInfo))
    {
        limLog(pMac, LOGE,
               FL("no enough space (buf %d) \n"), buflen);
        return eSIR_FAILURE;
    }

    *pBuf = pRadarInfo->channelNumber;
    pBuf += sizeof(tANI_U8);
    *len += sizeof(tANI_U8);

    limCopyU16(pBuf, pRadarInfo->radarPulseWidth);
    pBuf += sizeof(tANI_U16);
    *len += sizeof(tANI_U16);

    limCopyU16(pBuf, pRadarInfo->numRadarPulse);
    pBuf += sizeof(tANI_U16);
    *len += sizeof(tANI_U16);

    return eSIR_SUCCESS;
} /*** end limRadioInfoSerDes() ***/
#endif


/**
 * limPackBkgndScanFailNotify()
 *
 *FUNCTION:
 * This function is called by limSendSmeWmStatusChangeNtf()
 * to pack the tSirBackgroundScanInfo message
 *
 */
void
limPackBkgndScanFailNotify(tpAniSirGlobal pMac,
                           tSirSmeStatusChangeCode statusChangeCode,
                           tpSirBackgroundScanInfo pScanInfo,
                           tSirSmeWmStatusChangeNtf *pSmeNtf,
                           tANI_U8 sessionId)
{

    tANI_U16    length = (sizeof(tANI_U16) * 2) + sizeof(tANI_U8) +
                    sizeof(tSirSmeStatusChangeCode) +
                    sizeof(tSirBackgroundScanInfo);

#if defined (ANI_PRODUCT_TYPE_AP) && defined(ANI_LITTLE_BYTE_ENDIAN)
        sirStoreU16N((tANI_U8*)&pSmeNtf->messageType, eWNI_SME_WM_STATUS_CHANGE_NTF );
        sirStoreU16N((tANI_U8*)&pSmeNtf->length, length);
        pSmeNtf->sessionId = sessionId;
        sirStoreU32N((tANI_U8*)&pSmeNtf->statusChangeCode, statusChangeCode);

        sirStoreU32N((tANI_U8*)&pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanSuccess,
                     pScanInfo->numOfScanSuccess);
        sirStoreU32N((tANI_U8*)&pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanFailure,
                     pScanInfo->numOfScanFailure);
        sirStoreU32N((tANI_U8*)&pSmeNtf->statusChangeInfo.bkgndScanInfo.reserved,
                     pScanInfo->reserved);
#else
        pSmeNtf->messageType = eWNI_SME_WM_STATUS_CHANGE_NTF;
        pSmeNtf->statusChangeCode = statusChangeCode;
        pSmeNtf->length = length;
        pSmeNtf->sessionId = sessionId;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanSuccess = pScanInfo->numOfScanSuccess;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.numOfScanFailure = pScanInfo->numOfScanFailure;
        pSmeNtf->statusChangeInfo.bkgndScanInfo.reserved = pScanInfo->reserved;
#endif
}

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)

/**
 * nonTitanBssFoundSerDes()
 *
 *FUNCTION:
 * This function is called by limSendSmeWmStatusChangeNtf()
 * to serialize the following message
 * Mesg = eWNI_SME_WM_STATUS_CHANGE_NTF
 * Change code = eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac         Pointer to the global pMAC structure
 * @param  pNewBssInfo  Pointer to tpSirNeighborBssWdsInfo
 * @param  pBuf         Send data buffer
 * @param  len          Length of message
 * @param  buflen       Length remaining
 *
 * @return retCode      Indicates whether message is successfully
 *                      serialized (eSIR_SUCCESS) or not (eSIR_FAILURE)
 */

tSirRetStatus nonTitanBssFoundSerDes( tpAniSirGlobal pMac,
    tpSirNeighborBssWdsInfo pNewBssInfo,
    tANI_U8 *pBuf,
    tANI_U16 *len,
    tANI_U8 sessionId )
{
tANI_U8 *pTemp = pBuf;
tANI_U16 length = 0;
tANI_U32 bufLen = sizeof( tSirSmeWmStatusChangeNtf );

  // Serialize the header (length to be filled later)
  if( eSIR_SUCCESS != limSmeWmStatusChangeHeaderSerDes(
        pMac,
        eSIR_SME_CB_LEGACY_BSS_FOUND_BY_AP,
        pBuf,
        &length,
        bufLen,
        sessionId))
  {
    limLog( pMac, LOGE,
        FL("Header SerDes failed \n"));
    return eSIR_FAILURE;
  }
  pBuf += length;
  bufLen -= length;

  // Serialize tSirNeighborBssWdsInfo
  length = limCopyNeighborWdsInfo( pMac,
      pBuf,
      pNewBssInfo );
  pBuf += length;
  bufLen -= length;

  // Update tSirSmeWmStatusChangeNtf.length
  pBuf = pTemp;
  pBuf += sizeof(tANI_U16);
  limCopyU16(pBuf, length);

  // Update the total length to be returned
  *len = length;

  return eSIR_SUCCESS;
} /*** end nonTitanBssFoundSerDes() ***/

/**
 * limIsSmeSwitchChannelReqValid()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() upon
 * receiving SME_SWITCH_CHL_REQ message from application.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pBuf    - Pointer to a serialized SME_SWITCH_CHL_REQ message
 * @param  pSmeMsg - Pointer to a tSirsmeSwitchChannelReq structure
 * @return true if SME_SWITCH_CHL_REQ message is formatted correctly
 *                false otherwise
 */

tANI_BOOLEAN
limIsSmeSwitchChannelReqValid(tpAniSirGlobal pMac, 
                              tANI_U8 *pBuf, 
                              tpSirSmeSwitchChannelReq pSmeMsg)
{
    pSmeMsg->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);  

    pSmeMsg->length = limGetU16(pBuf); 
    pBuf += sizeof(tANI_U16);

    pSmeMsg->transactionId = limGetU16(pBuf); 
    pBuf += sizeof(tANI_U16);

    pSmeMsg->channelId = *pBuf;
    pBuf++;
  
    pSmeMsg->cbMode = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    pSmeMsg->dtimFactor = limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);

    if (pSmeMsg->length != SIR_SME_CHANNEL_SWITCH_SIZE)
    {
        PELOGE(limLog(pMac, LOGE, FL("Invalid length %d \n"), pSmeMsg->length);)
        return eANI_BOOLEAN_FALSE;
    }

    // dtimFactor should not be too large
    if (pSmeMsg->dtimFactor > SIR_MAX_DTIM_FACTOR)
    {
        PELOGE(limLog(pMac, LOGE, FL("Invalid dtimFactor %d \n"), pSmeMsg->dtimFactor);)
        return eANI_BOOLEAN_FALSE;
    }

    return eANI_BOOLEAN_TRUE;
}

#endif

#ifdef WLAN_SOFTAP_FEATURE
/**
 * limIsSmeGetAssocSTAsReqValid()
 *
 *FUNCTION:
 * This function is called by limProcessSmeReqMessages() upon
 * receiving SME_GET_ASSOC_STAS_REQ message from application.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pBuf    - Pointer to a serialized SME_GET_ASSOC_STAS_REQ message
 * @param  pSmeMsg - Pointer to a tSirSmeGetAssocSTAsReq structure
 * @return true if SME_GET_ASSOC_STAS_REQ message is formatted correctly
 *                false otherwise
 */
tANI_BOOLEAN
limIsSmeGetAssocSTAsReqValid(tpAniSirGlobal pMac, tpSirSmeGetAssocSTAsReq pGetAssocSTAsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    pGetAssocSTAsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pGetAssocSTAsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pGetAssocSTAsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract modId
    pGetAssocSTAsReq->modId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len  -= sizeof(tANI_U16);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract pUsrContext
    pGetAssocSTAsReq->pUsrContext = (void *)limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract pSapEventCallback
    pGetAssocSTAsReq->pSapEventCallback = (void *)limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract pAssocStasArray
    pGetAssocSTAsReq->pAssocStasArray = (void *)limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);

    PELOG1(limLog(pMac, LOG1, FL("SME_GET_ASSOC_STAS_REQ length consumed %d bytes \n"), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_ASSOC_STAS_REQ invalid length\n"));)
        return eANI_BOOLEAN_FALSE;
    }

    return eANI_BOOLEAN_TRUE;
}

/**
 * limTkipCntrMeasReqSerDes()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon receiving
 * eWNI_SME_TKIP_CNTR_MEAS_REQ from host HDD
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
 * @param  tpSirSmeTkipCntrMeasReq  Pointer to tSirSmeTkipCntrMeasReq being extracted
 * @param  pBuf                     Pointer to serialized buffer
 * @return retCode                  Indicates whether message is successfully
 *                                  de-serialized (eSIR_SUCCESS) or
 *                                  not (eSIR_FAILURE)
 */
tSirRetStatus
limTkipCntrMeasReqSerDes(tpAniSirGlobal pMac, tpSirSmeTkipCntrMeasReq  pTkipCntrMeasReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

#ifdef PE_DEBUG_LOG1
    tANI_U8  *pTemp = pBuf;
#endif

    pTkipCntrMeasReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pTkipCntrMeasReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    PELOG1(limLog(pMac, LOG1, FL("SME_TKIP_CNTR_MEAS_REQ length %d bytes is:\n"), len);)
    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pTemp, len);)

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pTkipCntrMeasReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pTkipCntrMeasReq->transactionId = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);
    len -= sizeof(tANI_U16); 
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pTkipCntrMeasReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bEnable
    pTkipCntrMeasReq->bEnable = *pBuf++;
    len --;

    PELOG1(limLog(pMac, LOG1, FL("SME_TKIP_CNTR_MEAS_REQ length consumed %d bytes \n"), len);)
    
    if (len)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_TKIP_CNTR_MEAS_REQ invalid \n"));)
        return eSIR_FAILURE;
    }
    else
        return eSIR_SUCCESS;
}

/**
 * limIsSmeGetWPSPBCSessionsReqValid()
 *
 *FUNCTION:
 * This function is called by limProcessSmeGetWPSPBCSessions() upon
 * receiving query WPS PBC overlap information message from application.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pBuf    - Pointer to a serialized SME_GET_WPSPBC_SESSION_REQ message
 * @param  pGetWPSPBCSessionsReq - Pointer to a tSirSmeGetWPSPBCSessionsReq structure
 * @return true if SME_GET_WPSPBC_SESSION_REQ message is formatted correctly
 *                false otherwise
 */
 
tSirRetStatus
limIsSmeGetWPSPBCSessionsReqValid(tpAniSirGlobal pMac, tSirSmeGetWPSPBCSessionsReq *pGetWPSPBCSessionsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirSmeGetWPSPBCSessionsReq));)
    
    pGetWPSPBCSessionsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pGetWPSPBCSessionsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

   // Extract pUsrContext
    pGetWPSPBCSessionsReq->pUsrContext = (void *)limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract pSapEventCallback
    pGetWPSPBCSessionsReq->pSapEventCallback = (void *)limGetU32(pBuf);
    pBuf += sizeof(tANI_U32);
    len  -= sizeof(tANI_U32);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pGetWPSPBCSessionsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
 
    // Extract MAC address of Station to be removed
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pGetWPSPBCSessionsReq->pRemoveMac, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    
    PELOG1(limLog(pMac, LOG1, FL("SME_GET_ASSOC_STAS_REQ length consumed %d bytes \n"), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_WPSPBC_SESSION_REQ invalid length\n"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
}

#endif

/**---------------------------------------------------------------
\fn     limGetSessionInfo
\brief  This function returns the sessionId and transactionId
\       of a message. This assumes that the message structure 
\       is of format:
\          tANI_U16   messageType
\          tANI_U16   messageLength
\          tANI_U8    sessionId
\          tANI_U16   transactionId
\param  pMac          - pMac global structure
\param  *pBuf         - pointer to the message buffer
\param  sessionId     - returned session id value
\param  transactionId - returned transaction ID value
\return None
------------------------------------------------------------------*/
void
limGetSessionInfo(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U8 *sessionId, tANI_U16 *transactionId)
{
    if (!pBuf)
    {
        limLog(pMac, LOGE, FL("NULL ptr received. \n"));
        return;
    }

    pBuf += sizeof(tANI_U16);   // skip message type 
    pBuf += sizeof(tANI_U16);   // skip message length

    *sessionId = *pBuf;            // get sessionId
    pBuf++;    
    *transactionId = limGetU16(pBuf);  // get transactionId

    return;
}

#ifdef WLAN_SOFTAP_FEATURE

/**
 * limUpdateAPWPSIEsReqSerDes()
 *
 *FUNCTION:
 * This function is to deserialize UpdateAPWPSIEs message
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
 * @param  pUpdateAPWPSIEsReq  Pointer to tSirUpdateAPWPSIEsReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limUpdateAPWPSIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPSIEsReq pUpdateAPWPSIEsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirUpdateAPWPSIEsReq));)

    if (!pUpdateAPWPSIEsReq || !pBuf)
        return eSIR_FAILURE;

    pUpdateAPWPSIEsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pUpdateAPWPSIEsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;
        
    // Extract transactionId
    pUpdateAPWPSIEsReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof( tANI_U16 );
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pUpdateAPWPSIEsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pUpdateAPWPSIEsReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract APWPSIEs
    palCopyMemory( pMac->hHdd, (tSirAPWPSIEs *) &pUpdateAPWPSIEsReq->APWPSIEs, pBuf, sizeof(tSirAPWPSIEs));
    pBuf += sizeof(tSirAPWPSIEs);
    len  -= sizeof(tSirAPWPSIEs);

    PELOG1(limLog(pMac, LOG1, FL("SME_UPDATE_APWPSIE_REQ length consumed %d bytes \n"), len);)

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_UPDATE_APWPSIE_REQ invalid length\n"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} /*** end limSetContextReqSerDes() ***/

/**
 * limUpdateAPWPARSNIEsReqSerDes ()
 *
 *FUNCTION:
 * This function is to deserialize UpdateAPWPSIEs message
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
 * @param  pUpdateAPWPARSNIEsReq  Pointer to tpSirUpdateAPWPARSNIEsReq being
 *                         extracted
 * @param  pBuf            Pointer to serialized buffer
 *
 * @return retCode         Indicates whether message is successfully
 *                         de-serialized (eSIR_SUCCESS) or
 *                         not (eSIR_FAILURE)
 */

tSirRetStatus
limUpdateAPWPARSNIEsReqSerDes(tpAniSirGlobal pMac, tpSirUpdateAPWPARSNIEsReq pUpdateAPWPARSNIEsReq, tANI_U8 *pBuf)
{
    tANI_S16 len = 0;

    PELOG1(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1, pBuf, sizeof(tSirUpdateAPWPARSNIEsReq));)

    if (!pUpdateAPWPARSNIEsReq || !pBuf)
        return eSIR_FAILURE;

    pUpdateAPWPARSNIEsReq->messageType = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    len = pUpdateAPWPARSNIEsReq->length = limGetU16(pBuf);
    pBuf += sizeof(tANI_U16);

    if (len < (tANI_S16) sizeof(tANI_U32))
        return eSIR_FAILURE;

    len -= sizeof(tANI_U32); // skip message header
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract transactionId
    pUpdateAPWPARSNIEsReq->transactionId = limGetU16( pBuf );
    pBuf += sizeof(tANI_U16);
    len -= sizeof( tANI_U16 );
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract bssId
    palCopyMemory( pMac->hHdd, (tANI_U8 *) pUpdateAPWPARSNIEsReq->bssId, pBuf, sizeof(tSirMacAddr));
    pBuf += sizeof(tSirMacAddr);
    len  -= sizeof(tSirMacAddr);
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract sessionId
    pUpdateAPWPARSNIEsReq->sessionId = *pBuf++;
    len--;
    if (limCheckRemainingLength(pMac, len) == eSIR_FAILURE)
        return eSIR_FAILURE;

    // Extract APWPARSNIEs
    palCopyMemory( pMac->hHdd, (tSirRSNie *) &pUpdateAPWPARSNIEsReq->APWPARSNIEs, pBuf, sizeof(tSirRSNie));
    pBuf += sizeof(tSirRSNie);
    len  -= sizeof(tSirRSNie);

    if (len < 0)
    {
        PELOGE(limLog(pMac, LOGE, FL("SME_GET_WPSPBC_SESSION_REQ invalid length\n"));)
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} /*** end limUpdateAPWPARSNIEsReqSerDes() ***/

#endif
