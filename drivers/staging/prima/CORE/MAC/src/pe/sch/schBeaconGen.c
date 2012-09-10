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
 * This file schBeaconGen.cc contains beacon generation related
 * functions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
 
#include "palTypes.h"
#include "wniCfgAp.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"

#include "limUtils.h"
#include "limApi.h"

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halCommonApi.h"
#include "halDataStruct.h"
#endif

#include "halMsgApi.h"
#include "cfgApi.h"
#include "pmmApi.h"
#include "schApi.h"

#include "parserApi.h"

#include "schDebug.h"

//
// March 15, 2006
// Temporarily (maybe for all of Alpha-1), assuming TIM = 0
//

const tANI_U8 P2pOui[] = {0x50, 0x6F, 0x9A, 0x9};

#ifdef ANI_PRODUCT_TYPE_AP

static void
specialBeaconProcessing(tpAniSirGlobal pMac, tANI_U32 beaconSize);
#endif

#if defined(WLAN_SOFTAP_FEATURE) && defined(WLAN_FEATURE_P2P)
tSirRetStatus schGetP2pIeOffset(tANI_U8 *pExtraIe, tANI_U32 extraIeLen, tANI_U16 *pP2pIeOffset)
{
    tSirRetStatus status = eSIR_FAILURE;   
    *pP2pIeOffset = 0;

    // Extra IE is not present
    if(0 == extraIeLen)
    {
        return status;
    }

    // Calculate the P2P IE Offset
    do
    {
        if(*pExtraIe == 0xDD)
        {
            if(palEqualMemory(NULL, (void *)(pExtraIe+2), &P2pOui, sizeof(P2pOui)))
            {
                (*pP2pIeOffset)++;
                status = eSIR_SUCCESS;
                break;
            }
        }

        (*pP2pIeOffset)++;
        pExtraIe++;
     }while(--extraIeLen > 0); 

     return status;
}
#endif

tSirRetStatus schAppendAddnIE(tpAniSirGlobal pMac, tpPESession psessionEntry,
                                     tANI_U8 *pFrame, tANI_U32 maxBeaconSize,
                                     tANI_U32 *nBytes)
{
    tSirRetStatus status = eSIR_FAILURE;
    tANI_U32 present, len;
    tANI_U8 addIE[WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN];
    
     if((status = wlan_cfgGetInt(pMac, WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG,
                                 &present)) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG"));
        return status;
    }

    if(present)
    {
        if((status = wlan_cfgGetStrLen(pMac, WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA,
                                       &len)) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP,
                FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA length"));
            return status;
        }

        if(len <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN && len && 
          ((len + *nBytes) <= maxBeaconSize))
        {
            if((status = wlan_cfgGetStr(pMac, 
                          WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA, &addIE[0], &len))
                          == eSIR_SUCCESS)
            {
#ifdef WLAN_FEATURE_P2P
                tANI_U8* pP2pIe = limGetP2pIEPtr(pMac, &addIE[0], len);
                if(pP2pIe != NULL)
                {
                    tANI_U8 noaLen = 0;
                    tANI_U8 noaStream[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
                    //get NoA attribute stream P2P IE
                    noaLen = limGetNoaAttrStream(pMac, noaStream, psessionEntry);
                    if(noaLen)
                    {
                        if(noaLen + len <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN)
                        {
                            vos_mem_copy(&addIE[len], noaStream, noaLen);
                            len += noaLen;
                            /* Update IE Len */
                            pP2pIe[1] += noaLen;
                        }
                        else
                        {
                            limLog(pMac, LOGE,
                               FL("Not able to insert NoA because of length constraint"));
                        }
                    }
                }
#endif
                vos_mem_copy(pFrame, &addIE[0], len);
                *nBytes = *nBytes + len;
            }
        }
    }

    return status;
}

// --------------------------------------------------------------------
/**
 * schSetFixedBeaconFields
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

tSirRetStatus schSetFixedBeaconFields(tpAniSirGlobal pMac,tpPESession psessionEntry)
{
    tpAniBeaconStruct pBeacon = (tpAniBeaconStruct)
                                   pMac->sch.schObject.gSchBeaconFrameBegin;
    tpSirMacMgmtHdr mac;
    tANI_U16        offset;
    tANI_U8        *ptr;
    tDot11fBeacon1  bcn1;
    tDot11fBeacon2  bcn2;
    tANI_U32        i, nStatus, nBytes;
    tANI_U32        wpsApEnable=0, tmp;
#ifdef WLAN_SOFTAP_FEATURE
    tDot11fIEWscProbeRes      WscProbeRes;
#ifdef WLAN_FEATURE_P2P
    tANI_U8  *pExtraIe = NULL;
    tANI_U32 extraIeLen =0;
    tANI_U16 extraIeOffset = 0;
    tANI_U16 p2pIeOffset = 0;
    tSirRetStatus status = eSIR_SUCCESS;
#endif
#endif

    PELOG1(schLog(pMac, LOG1, FL("Setting fixed beacon fields\n"));)

    /*
     * First set the fixed fields
     */

    // set the TFP headers

    // set the mac header
    palZeroMemory( pMac->hHdd, ( tANI_U8*) &pBeacon->macHdr, sizeof( tSirMacMgmtHdr ) );
    mac = (tpSirMacMgmtHdr) &pBeacon->macHdr;
    mac->fc.type = SIR_MAC_MGMT_FRAME;
    mac->fc.subType = SIR_MAC_MGMT_BEACON;

    for (i=0; i<6; i++)
        mac->da[i] = 0xff;
    
    /* Knocking out Global pMac update */
    /* limGetMyMacAddr(pMac, mac->sa); */
    /* limGetBssid(pMac, mac->bssId); */

    palCopyMemory(pMac->hHdd, mac->sa, psessionEntry->selfMacAddr, sizeof(psessionEntry->selfMacAddr));
    palCopyMemory(pMac->hHdd, mac->bssId, psessionEntry->bssId, sizeof (psessionEntry->bssId));

    mac->fc.fromDS = 0;
    mac->fc.toDS = 0;

    /*
     * Now set the beacon body
     */

    palZeroMemory( pMac->hHdd, ( tANI_U8*) &bcn1, sizeof( bcn1 ) );

    // Skip over the timestamp (it'll be updated later).

    bcn1.BeaconInterval.interval = pMac->sch.schObject.gSchBeaconInterval;
    PopulateDot11fCapabilities( pMac, &bcn1.Capabilities, psessionEntry );
    if (psessionEntry->ssidHidden)
    {
       bcn1.SSID.present = 1; //rest of the fileds are 0 for hidden ssid
    }
    else
    {
       PopulateDot11fSSID( pMac, &psessionEntry->ssId, &bcn1.SSID );
    }

    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &bcn1.SuppRates,psessionEntry);
    PopulateDot11fDSParams( pMac, &bcn1.DSParams, psessionEntry->currentOperChannel, psessionEntry);
    PopulateDot11fIBSSParams( pMac, &bcn1.IBSSParams,psessionEntry);

    offset = sizeof( tAniBeaconStruct );
    ptr    = pMac->sch.schObject.gSchBeaconFrameBegin + offset;

#ifdef WLAN_SOFTAP_FEATURE
    if((psessionEntry->limSystemRole == eLIM_AP_ROLE) 
        && (psessionEntry->proxyProbeRspEn))
    {
        /* Initialize the default IE bitmap to zero */
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&(psessionEntry->DefProbeRspIeBitmap), (sizeof( tANI_U32 ) * 8));

        /* Initialize the default IE bitmap to zero */
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&(psessionEntry->probeRespFrame), sizeof(psessionEntry->probeRespFrame));

        /* Can be efficiently updated whenever new IE added  in Probe response in future */
        limUpdateProbeRspTemplateIeBitmapBeacon1(pMac,&bcn1,&psessionEntry->DefProbeRspIeBitmap[0],
                                                &psessionEntry->probeRespFrame);
    }
#endif

    nStatus = dot11fPackBeacon1( pMac, &bcn1, ptr,
                                 SCH_MAX_BEACON_SIZE - offset,
                                 &nBytes );
    if ( DOT11F_FAILED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("Failed to packed a tDot11fBeacon1 (0x%0"
                             "8x.).\n"), nStatus );
      return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("There were warnings while packing a tDo"
                             "t11fBeacon1 (0x%08x.).\n"), nStatus );
    }
    /*changed  to correct beacon corruption */
    palZeroMemory( pMac->hHdd, ( tANI_U8*) &bcn2, sizeof( bcn2 ) );
    pMac->sch.schObject.gSchBeaconOffsetBegin = offset + ( tANI_U16 )nBytes;
    schLog( pMac, LOG1, FL("Initialized beacon begin, offset %d\n"), offset );

    /*
     * Initialize the 'new' fields at the end of the beacon
     */

    
    PopulateDot11fCountry( pMac, &bcn2.Country, psessionEntry);
    if(bcn1.Capabilities.qos)
    {
        PopulateDot11fEDCAParamSet( pMac, &bcn2.EDCAParamSet, psessionEntry);
    }

    if(psessionEntry->lim11hEnable)
    {
      PopulateDot11fPowerConstraints( pMac, &bcn2.PowerConstraints );
      PopulateDot11fTPCReport( pMac, &bcn2.TPCReport, psessionEntry);
    }

#ifdef ANI_PRODUCT_TYPE_AP
    if( psessionEntry->lim11hEnable && (eLIM_QUIET_RUNNING == psessionEntry->gLimSpecMgmt.quietState))
    {
      PopulateDot11fQuiet( pMac, &bcn2.Quiet );
    }

    /* If 11h is enabled, and AP is in the state of changing either the
     * primary channel, or both primary & secondary channel, and the
     * channel switch count is still being decremented, then AP shall
     * populate the 802.11h channel switch IE in its Beacons and Probe
     * Responses.
     */
    if ( (psessionEntry->lim11hEnable) &&
         (psessionEntry->gLimChannelSwitch.switchCount != 0) &&
         (psessionEntry->gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING))

    {
      PopulateDot11fChanSwitchAnn( pMac, &bcn2.ChanSwitchAnn, psessionEntry );
      PopulateDot11fExtChanSwitchAnn(pMac, &bcn2.ExtChanSwitchAnn, psessionEntry );
    }
#endif

    if (psessionEntry->dot11mode != WNI_CFG_DOT11_MODE_11B)
        PopulateDot11fERPInfo( pMac, &bcn2.ERPInfo, psessionEntry );

    if(psessionEntry->htCapability)
    {
        PopulateDot11fHTCaps( pMac, psessionEntry, &bcn2.HTCaps );
#ifdef WLAN_SOFTAP_FEATURE
        PopulateDot11fHTInfo( pMac, &bcn2.HTInfo, psessionEntry );
#else
        PopulateDot11fHTInfo( pMac, &bcn2.HTInfo );
#endif
    }
#ifdef WLAN_FEATURE_11AC
    if(psessionEntry->vhtCapability)
    {        
        limLog( pMac, LOGW, FL("Populate VHT IEs in Beacon\n"));
        PopulateDot11fVHTCaps( pMac, &bcn2.VHTCaps );
        PopulateDot11fVHTOperation( pMac, &bcn2.VHTOperation);
        // we do not support multi users yet
        //PopulateDot11fVHTExtBssLoad( pMac, &bcn2.VHTExtBssLoad);
    }
#endif

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &bcn2.ExtSuppRates, psessionEntry );
 
    if( psessionEntry->pLimStartBssReq != NULL )
    {
          PopulateDot11fWPA( pMac, &psessionEntry->pLimStartBssReq->rsnIE,
                       &bcn2.WPA );
          PopulateDot11fRSN( pMac, &psessionEntry->pLimStartBssReq->rsnIE,
                       &bcn2.RSN );
    }

    if(psessionEntry->limWmeEnabled)
    {
        PopulateDot11fWMM( pMac, &bcn2.WMMInfoAp, &bcn2.WMMParams, &bcn2.WMMCaps, psessionEntry);
    }
#ifdef WLAN_SOFTAP_FEATURE
    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            PopulateDot11fBeaconWPSIEs( pMac, &bcn2.WscBeacon, psessionEntry);            
        }
    }
    else
    {
#endif
    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_ENABLE, &tmp) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d\n", WNI_CFG_WPS_ENABLE );
    
    wpsApEnable = tmp & WNI_CFG_WPS_ENABLE_AP;
    
    if (wpsApEnable)
    {
        PopulateDot11fWsc(pMac, &bcn2.WscBeacon);
    }

    if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_BEGIN)
    {
        PopulateDot11fWscRegistrarInfo(pMac, &bcn2.WscBeacon);
        pMac->lim.wscIeInfo.wscEnrollmentState = eLIM_WSC_ENROLL_IN_PROGRESS;
    }

    if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_END)
    {
        DePopulateDot11fWscRegistrarInfo(pMac, &bcn2.WscBeacon);
        pMac->lim.wscIeInfo.wscEnrollmentState = eLIM_WSC_ENROLL_NOOP;
    }
#ifdef WLAN_SOFTAP_FEATURE    
    }
#endif

#ifdef WLAN_SOFTAP_FEATURE
    if((psessionEntry->limSystemRole == eLIM_AP_ROLE) 
        && (psessionEntry->proxyProbeRspEn))
    {
        /* Can be efficiently updated whenever new IE added  in Probe response in future */
        limUpdateProbeRspTemplateIeBitmapBeacon2(pMac,&bcn2,&psessionEntry->DefProbeRspIeBitmap[0],
                                                &psessionEntry->probeRespFrame);

        /* update probe response WPS IE instead of beacon WPS IE
        * */
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            if(psessionEntry->APWPSIEs.SirWPSProbeRspIE.FieldPresent)
            {
                PopulateDot11fProbeResWPSIEs(pMac, &WscProbeRes, psessionEntry);
            }
            else
            {
                WscProbeRes.present = 0;
            }
            if(WscProbeRes.present)
            {
                SetProbeRspIeBitmap(&psessionEntry->DefProbeRspIeBitmap[0],SIR_MAC_WPA_EID);
                palCopyMemory(pMac->hHdd,
                            (void *)&psessionEntry->probeRespFrame.WscProbeRes,
                            (void *)&WscProbeRes,
                            sizeof(WscProbeRes));
            }
        }

    }
#endif

    nStatus = dot11fPackBeacon2( pMac, &bcn2,
                                 pMac->sch.schObject.gSchBeaconFrameEnd,
                                 SCH_MAX_BEACON_SIZE, &nBytes );
    if ( DOT11F_FAILED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("Failed to packed a tDot11fBeacon2 (0x%0"
                             "8x.).\n"), nStatus );
      return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("There were warnings while packing a tDo"
                             "t11fBeacon2 (0x%08x.).\n"), nStatus );
    }

#if defined(WLAN_SOFTAP_FEATURE) && defined(WLAN_FEATURE_P2P)
    pExtraIe = pMac->sch.schObject.gSchBeaconFrameEnd + nBytes;
    extraIeOffset = nBytes;
#endif

    //TODO: Append additional IE here.
    schAppendAddnIE(pMac, psessionEntry, 
                    pMac->sch.schObject.gSchBeaconFrameEnd + nBytes,
                    SCH_MAX_BEACON_SIZE, &nBytes);

    pMac->sch.schObject.gSchBeaconOffsetEnd = ( tANI_U16 )nBytes;

#if defined(WLAN_SOFTAP_FEATURE) && defined(WLAN_FEATURE_P2P)
    extraIeLen = nBytes - extraIeOffset;

    //Get the p2p Ie Offset
    status = schGetP2pIeOffset(pExtraIe, extraIeLen, &p2pIeOffset);

    if(eSIR_SUCCESS == status)
    {
       //Update the P2P Ie Offset
       pMac->sch.schObject.p2pIeOffset = 
                    pMac->sch.schObject.gSchBeaconOffsetBegin + TIM_IE_SIZE +
                    extraIeOffset + p2pIeOffset;
    }
    else
    {
       pMac->sch.schObject.p2pIeOffset = 0;
    }
#endif

    schLog( pMac, LOG1, FL("Initialized beacon end, offset %d\n"),
            pMac->sch.schObject.gSchBeaconOffsetEnd );

    pMac->sch.schObject.fBeaconChanged = 1;

    return eSIR_SUCCESS;
}

#ifdef WLAN_SOFTAP_FEATURE
void limUpdateProbeRspTemplateIeBitmapBeacon1(tpAniSirGlobal pMac,
                                              tDot11fBeacon1* beacon1,
                                              tANI_U32* DefProbeRspIeBitmap,
                                              tDot11fProbeResponse* prb_rsp)
{
    prb_rsp->BeaconInterval = beacon1->BeaconInterval;
    palCopyMemory(pMac->hHdd,(void *)&prb_rsp->Capabilities,
                            (void *)&beacon1->Capabilities,
                            sizeof(beacon1->Capabilities));

    /* SSID */
    if(beacon1->SSID.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_SSID_EID);
        /* populating it , because probe response has to go with SSID even in hidden case */
        PopulateDot11fSSID2( pMac, &prb_rsp->SSID );
    }
    /* supported rates */
    if(beacon1->SuppRates.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_RATESET_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->SuppRates,
                            (void *)&beacon1->SuppRates,
                            sizeof(beacon1->SuppRates));

    }
    /* DS Parameter set */
    if(beacon1->DSParams.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_DS_PARAM_SET_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->DSParams,
                            (void *)&beacon1->DSParams,
                            sizeof(beacon1->DSParams));

    }

    /* IBSS params will not be present in the Beacons transmitted by AP */
}

void limUpdateProbeRspTemplateIeBitmapBeacon2(tpAniSirGlobal pMac,
                                              tDot11fBeacon2* beacon2,
                                              tANI_U32* DefProbeRspIeBitmap,
                                              tDot11fProbeResponse* prb_rsp)
{
    /* IBSS parameter set - will not be present in probe response tx by AP */
    /* country */
    if(beacon2->Country.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_COUNTRY_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->Country,
                            (void *)&beacon2->Country,
                            sizeof(beacon2->Country));

    }
    /* Power constraint */
    if(beacon2->PowerConstraints.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_PWR_CONSTRAINT_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->PowerConstraints,
                            (void *)&beacon2->PowerConstraints,
                            sizeof(beacon2->PowerConstraints));

    }
    /* Channel Switch Annoouncement SIR_MAC_CHNL_SWITCH_ANN_EID */
    if(beacon2->ChanSwitchAnn.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_CHNL_SWITCH_ANN_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->ChanSwitchAnn,
                            (void *)&beacon2->ChanSwitchAnn,
                            sizeof(beacon2->ChanSwitchAnn));

    }
    /* ERP information */
    if(beacon2->ERPInfo.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_ERP_INFO_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->ERPInfo,
                            (void *)&beacon2->ERPInfo,
                            sizeof(beacon2->ERPInfo));

    }
    /* Extended supported rates */
    if(beacon2->ExtSuppRates.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_EXTENDED_RATE_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->ExtSuppRates,
                            (void *)&beacon2->ExtSuppRates,
                            sizeof(beacon2->ExtSuppRates));

    }

    /* WPA */
    if(beacon2->WPA.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->WPA,
                            (void *)&beacon2->WPA,
                            sizeof(beacon2->WPA));

    }

    /* RSN */
    if(beacon2->RSN.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_RSN_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->RSN,
                            (void *)&beacon2->RSN,
                            sizeof(beacon2->RSN));

    }
/*
    // BSS load
    if(beacon2->QBSSLoad.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_QBSS_LOAD_EID);
    }
*/
    /* EDCA Parameter set */
    if(beacon2->EDCAParamSet.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_EDCA_PARAM_SET_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->EDCAParamSet,
                            (void *)&beacon2->EDCAParamSet,
                            sizeof(beacon2->EDCAParamSet));

    }
    /* Vendor specific - currently no vendor specific IEs added */
    /* Requested IEs - currently we are not processing this will be added later */
    //HT capability IE
    if(beacon2->HTCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_HT_CAPABILITIES_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->HTCaps,
                            (void *)&beacon2->HTCaps,
                            sizeof(beacon2->HTCaps));
    }
    // HT Info IE
    if(beacon2->HTInfo.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_HT_INFO_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->HTInfo,
                            (void *)&beacon2->HTInfo,
                            sizeof(beacon2->HTInfo));
    }

#ifdef WLAN_FEATURE_11AC
    if(beacon2->VHTCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_CAPABILITIES_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->VHTCaps,
                            (void *)&beacon2->VHTCaps,
                            sizeof(beacon2->VHTCaps));
    }
    if(beacon2->VHTOperation.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_OPERATION_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->VHTOperation,
                            (void *)&beacon2->VHTOperation,
                            sizeof(beacon2->VHTOperation));
    }
    if(beacon2->VHTExtBssLoad.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_EXT_BSS_LOAD_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->VHTExtBssLoad,
                            (void *)&beacon2->VHTExtBssLoad,
                            sizeof(beacon2->VHTExtBssLoad));
    }
#endif

    //WMM IE
    if(beacon2->WMMParams.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->WMMParams,
                            (void *)&beacon2->WMMParams,
                            sizeof(beacon2->WMMParams));
    }
    //WMM capability - most of the case won't be present
    if(beacon2->WMMCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        palCopyMemory(pMac->hHdd,(void *)&prb_rsp->WMMCaps,
                            (void *)&beacon2->WMMCaps,
                            sizeof(beacon2->WMMCaps));
    }

}

void SetProbeRspIeBitmap(tANI_U32* IeBitmap,tANI_U32 pos)
{
    tANI_U32 index,temp;

    index = pos >> 5;
    if(index >= 8 )
    {
        return;
    }
    temp = IeBitmap[index];

    temp |= 1 << (pos & 0x1F);

    IeBitmap[index] = temp;
}

#endif

#ifdef ANI_PRODUCT_TYPE_AP

//----------------------------
/**
  *     @function : schUpdateCfpParam
  *     @brief      : Generate the CFP Parameter Set
  *     
  *     @param    :  pMac - tpAniSirGlobal
  *                       ptr    - Pointer to the BeaconFrame
  *                       pbeaconSize - The beaconSize
  *
  *     @return    :  Return the Updated Ptrlocation
  */
static tANI_U8 *
__schUpdateCfpParam(tpAniSirGlobal pMac, tANI_U8 *ptr, tANI_U32 *pbeaconSize)
{  
    tANI_U32 val;

    *ptr++ = SIR_MAC_CF_PARAM_SET_EID;
    *ptr++ = SIR_MAC_CF_PARAM_SET_EID_MIN;

    wlan_cfgGetInt(pMac, WNI_CFG_CFP_PERIOD, &val);
    if (++pMac->sch.schObject.gSchCFPCount == val)
        pMac->sch.schObject.gSchCFPCount = 0;

    *ptr++ = pMac->sch.schObject.gSchCFPCount;
    *ptr++ = (tANI_U8)val;

    wlan_cfgGetInt(pMac, WNI_CFG_CFP_MAX_DURATION, &val);
    pMac->sch.schObject.gSchCFPMaxDuration = (tANI_U8)val;

    sirStoreU16(ptr, (tANI_U16)val);
    ptr += 2;

    if (pMac->sch.schObject.gSchCFPCount == 0)
        pMac->sch.schObject.gSchCFPDurRemaining = pMac->sch.schObject.gSchCFPMaxDuration;
    else if (pMac->sch.schObject.gSchCFPDurRemaining > pMac->sch.schObject.gSchBeaconInterval)
        pMac->sch.schObject.gSchCFPDurRemaining -= pMac->sch.schObject.gSchBeaconInterval;
    else
        pMac->sch.schObject.gSchCFPDurRemaining = 0;

    sirStoreU16(ptr, pMac->sch.schObject.gSchCFPDurRemaining);
    ptr += 2;

    (*pbeaconSize) += 2 + SIR_MAC_CF_PARAM_SET_EID_MIN;

    return ptr;
}

#endif

// --------------------------------------------------------------------
/**
 * writeBeaconToMemory
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @param size    Size of the beacon to write to memory
 * @param length Length field of the beacon to write to memory
 * @return None
 */

void writeBeaconToMemory(tpAniSirGlobal pMac, tANI_U16 size, tANI_U16 length, tpPESession psessionEntry)
{
    tANI_U16          i;
    tpAniBeaconStruct pBeacon;

    // copy end of beacon only if length > 0
    if (length > 0)
    {
        for (i=0; i < pMac->sch.schObject.gSchBeaconOffsetEnd; i++)
            pMac->sch.schObject.gSchBeaconFrameBegin[size++] = pMac->sch.schObject.gSchBeaconFrameEnd[i];
    }
    
    // Update the beacon length
    pBeacon = (tpAniBeaconStruct) pMac->sch.schObject.gSchBeaconFrameBegin;
    // Do not include the beaconLength indicator itself
    if (length == 0)
    {
        pBeacon->beaconLength = 0;
        // Dont copy entire beacon, Copy length field alone
        size = 4;
    }
    else
        pBeacon->beaconLength = (tANI_U32) size - sizeof( tANI_U32 );

    // write size bytes from gSchBeaconFrameBegin
    PELOG2(schLog(pMac, LOG2, FL("Beacon size - %d bytes\n"), size);)
    PELOG2(sirDumpBuf(pMac, SIR_SCH_MODULE_ID, LOG2, pMac->sch.schObject.gSchBeaconFrameBegin, size);)

    if (! pMac->sch.schObject.fBeaconChanged)
        return;

    pMac->sch.gSchGenBeacon = 1;
    if (pMac->sch.gSchGenBeacon)
    {
        pMac->sch.gSchBeaconsSent++;

        //
        // Copy beacon data to SoftMAC shared memory...
        // Do this by sending a message to HAL
        //

        size = (size + 3) & (~3);
        if( eSIR_SUCCESS != schSendBeaconReq( pMac, pMac->sch.schObject.gSchBeaconFrameBegin, size , psessionEntry))
            PELOGE(schLog(pMac, LOGE, FL("schSendBeaconReq() returned an error (zsize %d)\n"), size);)
        else
        {
            pMac->sch.gSchBeaconsWritten++;
        }
    }
    pMac->sch.schObject.fBeaconChanged = 0;
}

// --------------------------------------------------------------------
/**
 * @function: SchProcessPreBeaconInd
 *
 * @brief : Process the PreBeacon Indication from the Lim
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param : pMac - tpAniSirGlobal
 *
 * @return None
 */

void
schProcessPreBeaconInd(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpBeaconGenParams  pMsg = (tpBeaconGenParams)limMsg->bodyptr;
    tANI_U32 beaconSize = pMac->sch.schObject.gSchBeaconOffsetBegin;
    tpPESession psessionEntry;
    tANI_U8 sessionId;

    if((psessionEntry = peFindSessionByBssid(pMac,pMsg->bssId, &sessionId))== NULL)
    {
        PELOGE(schLog(pMac, LOGE, FL("session lookup fails\n"));)
        goto end;
    } 
           


    // If SME is not in normal mode, no need to generate beacon
    if (psessionEntry->limSmeState  != eLIM_SME_NORMAL_STATE)
    {
        PELOGE(schLog(pMac, LOG1, FL("PreBeaconInd received in invalid state: %d\n"), psessionEntry->limSmeState);)
        goto end;
    }

    switch(psessionEntry->limSystemRole){

    case eLIM_STA_IN_IBSS_ROLE:
    case eLIM_BT_AMP_AP_ROLE:
    case eLIM_BT_AMP_STA_ROLE:
        // generate IBSS parameter set
        if(psessionEntry->statypeForBss == STA_ENTRY_SELF)
            writeBeaconToMemory(pMac, (tANI_U16) beaconSize, (tANI_U16)beaconSize, psessionEntry);
    else
        PELOGE(schLog(pMac, LOGE, FL("can not send beacon for PEER session entry\n"));)
        break;

#ifdef WLAN_SOFTAP_FEATURE
    case eLIM_AP_ROLE:{
         tANI_U8 *ptr = &pMac->sch.schObject.gSchBeaconFrameBegin[pMac->sch.schObject.gSchBeaconOffsetBegin];
         tANI_U16 timLength = 0;
         if(psessionEntry->statypeForBss == STA_ENTRY_SELF){
             pmmGenerateTIM(pMac, &ptr, &timLength, psessionEntry->dtimPeriod);
         beaconSize += 2 + timLength;
         writeBeaconToMemory(pMac, (tANI_U16) beaconSize, (tANI_U16)beaconSize, psessionEntry);
     }
     else
         PELOGE(schLog(pMac, LOGE, FL("can not send beacon for PEER session entry\n"));)
         }
     break;
#endif

#ifdef ANI_PRODUCT_TYPE_AP
    case eLIM_AP_ROLE: 
    {
        tANI_U8 *ptr = &pMac->sch.schObject.gSchBeaconFrameBegin[pMac->sch.schObject.gSchBeaconOffsetBegin];
        tANI_U16 timLength = 0;

        if (pMac->sch.schObject.gSchCFPEnabled)
          ptr = __schUpdateCfpParam( pMac, ptr, &beaconSize);

        // generate TIM
        pmmGenerateTIM(pMac, &ptr, &timLength);
        beaconSize += 2 + timLength;

        /**
        * Safe to call this each time.
        * Based on the requirement for updating the
        * fixed beacon fields, this routine will
        * appropriately update the fixed fields
        */
        specialBeaconProcessing(pMac, beaconSize);
        writeBeaconToMemory(pMac, beaconSize, beaconSize, psessionEntry);
        pmmHandleTimBasedDisassociation( pMac, psessionEntry );
    }
    break;
#endif

    default:
        PELOGE(schLog(pMac, LOGE, FL("Error-PE has Receive PreBeconGenIndication when System is in %d role"),
               psessionEntry->limSystemRole);)
    }

end:
    palFreeMemory(pMac->hHdd, (void*)pMsg);

}

/**-------------------------------------------------------------
 \fn       specialBeaconProcessing
 \brief   To add/update channel switch IE/ Quiet IE in beacons.
          And also to resume transmission and measurement after
          switching the channel.
      
 \param   pMac
 \param   beaconSize  Size of the beacon
 \return   NONE
 --------------------------------------------------------------*/
#ifdef ANI_PRODUCT_TYPE_AP

static void
specialBeaconProcessing( tpAniSirGlobal pMac, tANI_U32 beaconSize)
{

    tpPESession psessionEntry = &pMac->lim.gpSession[0]; //TBD-RAJESH HOW TO GET sessionEntry?????
    tANI_BOOLEAN fBeaconChanged = eANI_BOOLEAN_FALSE;

    fBeaconChanged = limUpdateQuietIEInBeacons( pMac );

    if((pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_BEGIN) ||
       (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_END))
    {
        fBeaconChanged = eANI_BOOLEAN_TRUE;
    }


    /*******************************
     * Processing Channel Switch IE
     *******************************/
    if (pMac->lim.gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING)
    {
        fBeaconChanged = eANI_BOOLEAN_TRUE;

#if 0
        // If the station doesn't support 11h or have link monitoring enabled,
        // AP has to send disassoc frame to indicate station before going
        // to new channel. Otherwise station wont connect to AP in new channel.
        if (pMac->lim.gLimChannelSwitch.switchCount == 1)
        {
            if((pMac->lim.gLimChannelSwitch.state
                == eLIM_CHANNEL_SWITCH_PRIMARY_ONLY) ||
                (pMac->lim.gLimChannelSwitch.state
                == eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY))
            {
                tSirMacAddr   bcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

                limSendDisassocMgmtFrame(pMac,
                                   eSIR_MAC_DISASSOC_LEAVING_BSS_REASON,
                                   bcAddr);
            }
        }
#endif
        if (pMac->lim.gLimChannelSwitch.switchCount == 0)
        {
            
            /* length is set to 0, so that no beacon is transmitted without channel switch IE
                       * before switching  to new channel */
            pMac->sch.schObject.fBeaconChanged = 1;
            writeBeaconToMemory(pMac, beaconSize, 0, psessionEntry);
            schSetFixedBeaconFields(pMac,psessionEntry);

           PELOG3(limLog(pMac, LOG3, FL("Channel switch state = %d\n"), pMac->lim.gLimChannelSwitch.state);)
            switch(pMac->lim.gLimChannelSwitch.state)
            {
                case eLIM_CHANNEL_SWITCH_PRIMARY_ONLY:
                    limSwitchPrimaryChannel(pMac, pMac->lim.gLimChannelSwitch.primaryChannel);
                    break;
                case eLIM_CHANNEL_SWITCH_SECONDARY_ONLY:
                    limSwitchPrimarySecondaryChannel(pMac, psessionEntry,
                                             psessionEntry->currentOperChannel,
                                             pMac->lim.gLimChannelSwitch.secondarySubBand);
                    break;
                case eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY:
                    limSwitchPrimarySecondaryChannel(pMac, psessionEntry,
                                             pMac->lim.gLimChannelSwitch.primaryChannel,
                                             pMac->lim.gLimChannelSwitch.secondarySubBand);
                    break;
                case eLIM_CHANNEL_SWITCH_IDLE:
                    PELOGE(schLog(pMac, LOGE, FL("incorrect state - CHANNEL_SWITCH_IDLE\n"));)
                    break;

                default:
                    break;
            }
            pMac->lim.gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_IDLE;

            limSendSmeRsp(pMac, eWNI_SME_SWITCH_CHL_RSP, eSIR_SME_SUCCESS);

            limFrameTransmissionControl(pMac, eLIM_TX_BSS_BUT_BEACON, eLIM_RESUME_TX);
            /* Flag to indicate 11h channel switch is done. */
            pMac->lim.gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_INIT;
            pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
            LIM_SET_RADAR_DETECTED(pMac, eANI_BOOLEAN_FALSE);
            
            if (pMac->lim.gpLimMeasReq)
                limReEnableLearnMode(pMac);

            return;
        }
    }

    if (fBeaconChanged)
    {
        schSetFixedBeaconFields(pMac,psessionEntry);

        if (pMac->lim.gLimChannelSwitch.switchCount > 0)
            pMac->lim.gLimChannelSwitch.switchCount--;
    }
}
#endif


