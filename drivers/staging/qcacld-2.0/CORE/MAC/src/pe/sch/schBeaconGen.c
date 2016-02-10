/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"

#include "limUtils.h"
#include "limApi.h"


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
            if ( vos_mem_compare ( (void *)(pExtraIe+2), &P2pOui, sizeof(P2pOui) ) )
            {
                status = eSIR_SUCCESS;
                break;
            }
        }

        (*pP2pIeOffset)++;
        pExtraIe++;
     }while(--extraIeLen > 0);

     return status;
}

tSirRetStatus schAppendAddnIE(tpAniSirGlobal pMac, tpPESession psessionEntry,
                                     tANI_U8 *pFrame, tANI_U32 maxBeaconSize,
                                     tANI_U32 *nBytes, uint8_t *addn_ie,
                                     uint16_t addn_ielen)
{
    tSirRetStatus status = eSIR_FAILURE;
    tANI_U8 addIE[WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN];

    if(addn_ielen <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN && addn_ielen &&
      ((addn_ielen + *nBytes) <= maxBeaconSize))
    {

        vos_mem_copy(&addIE[0], addn_ie, addn_ielen);

        {
            tANI_U8* pP2pIe = limGetP2pIEPtr(pMac, &addIE[0], addn_ielen);
            if ((pP2pIe != NULL) && !pMac->beacon_offload)
            {
                tANI_U8 noaLen = 0;
                tANI_U8 noaStream[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
                //get NoA attribute stream P2P IE
                noaLen = limGetNoaAttrStream(pMac, noaStream, psessionEntry);
                if(noaLen)
                {
                    if ((noaLen + addn_ielen) <=
                                    WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN) {
                        vos_mem_copy(&addIE[addn_ielen], noaStream, noaLen);
                        addn_ielen += noaLen;
                        /* Update IE Len */
                        pP2pIe[1] += noaLen;
                    }
                    else
                    {
                        schLog(pMac, LOGE,
                           FL("Not able to insert NoA because of addn_ielength constraint"));
                    }
                }
            }
            if(addn_ielen <= WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN)
            {
                vos_mem_copy(pFrame, &addIE[0], addn_ielen);
                *nBytes = *nBytes + addn_ielen;
            }
            else
            {
                schLog(pMac, LOGW, FL("Not able to insert because of"
                    " addn_ielength constraint %d"), addn_ielen);
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
                          psessionEntry->pSchBeaconFrameBegin;
    tpSirMacMgmtHdr mac;
    tANI_U16        offset;
    tANI_U8        *ptr;
    tDot11fBeacon1 *pBcn1;
    tDot11fBeacon2 *pBcn2;
    tANI_U32        i, nStatus, nBytes;
    tANI_U32        wpsApEnable=0, tmp;
    tDot11fIEWscProbeRes      *pWscProbeRes;
    tANI_U8  *pExtraIe = NULL;
    tANI_U32 extraIeLen =0;
    tANI_U16 extraIeOffset = 0;
    tANI_U16 p2pIeOffset = 0;
    tSirRetStatus status = eSIR_SUCCESS;
    tANI_BOOLEAN  isVHTEnabled = eANI_BOOLEAN_FALSE;
    uint16_t addn_ielen = 0;
    uint8_t *addn_ie = NULL;
    tDot11fIEExtCap extracted_extcap;
    bool extcap_present = true, addnie_present = false;

    pBcn1 = vos_mem_malloc(sizeof(tDot11fBeacon1));
    if ( NULL == pBcn1 )
    {
        schLog(pMac, LOGE, FL("Failed to allocate memory") );
        return eSIR_FAILURE;
    }

    pBcn2 = vos_mem_malloc(sizeof(tDot11fBeacon2));
    if ( NULL == pBcn2 )
    {
        schLog(pMac, LOGE, FL("Failed to allocate memory") );
        vos_mem_free(pBcn1);
        return eSIR_FAILURE;
    }

    pWscProbeRes = vos_mem_malloc(sizeof(tDot11fIEWscProbeRes));
    if ( NULL == pWscProbeRes )
    {
        schLog(pMac, LOGE, FL("Failed to allocate memory") );
        vos_mem_free(pBcn1);
        vos_mem_free(pBcn2);
        return eSIR_FAILURE;
    }

    PELOG1(schLog(pMac, LOG1, FL("Setting fixed beacon fields"));)

    /*
     * First set the fixed fields
     */

    // set the TFP headers

    // set the mac header
    vos_mem_set(( tANI_U8*) &pBeacon->macHdr, sizeof( tSirMacMgmtHdr ),0);
    mac = (tpSirMacMgmtHdr) &pBeacon->macHdr;
    mac->fc.type = SIR_MAC_MGMT_FRAME;
    mac->fc.subType = SIR_MAC_MGMT_BEACON;

    for (i=0; i<6; i++)
        mac->da[i] = 0xff;

    vos_mem_copy(mac->sa, psessionEntry->selfMacAddr, sizeof(psessionEntry->selfMacAddr));
    vos_mem_copy(mac->bssId, psessionEntry->bssId, sizeof (psessionEntry->bssId));

    mac->fc.fromDS = 0;
    mac->fc.toDS = 0;

    /*
     * Now set the beacon body
     */

    vos_mem_set(( tANI_U8*) pBcn1, sizeof( tDot11fBeacon1 ), 0);

    /* Skip over the time stamp (it'll be updated later). */

    pBcn1->BeaconInterval.interval = pMac->sch.schObject.gSchBeaconInterval;
    PopulateDot11fCapabilities( pMac, &pBcn1->Capabilities, psessionEntry );
    if (psessionEntry->ssidHidden)
    {
       pBcn1->SSID.present = 1; /* Rest of the fields are 0 for hidden ssid */
       if((psessionEntry->ssId.length) &&
          (psessionEntry->ssidHidden == eHIDDEN_SSID_ZERO_CONTENTS))
          pBcn1->SSID.num_ssid = psessionEntry->ssId.length;
    }
    else
    {
       PopulateDot11fSSID( pMac, &psessionEntry->ssId, &pBcn1->SSID );
    }


    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &pBcn1->SuppRates,psessionEntry);
    PopulateDot11fDSParams( pMac, &pBcn1->DSParams, psessionEntry->currentOperChannel, psessionEntry);
    PopulateDot11fIBSSParams( pMac, &pBcn1->IBSSParams,psessionEntry);

    offset = sizeof( tAniBeaconStruct );
    ptr    = psessionEntry->pSchBeaconFrameBegin + offset;

    if (LIM_IS_AP_ROLE(psessionEntry)) {
        /* Initialize the default IE bitmap to zero */
        vos_mem_set(( tANI_U8* )&(psessionEntry->DefProbeRspIeBitmap), (sizeof( tANI_U32 ) * 8), 0);

        /* Initialize the default IE bitmap to zero */
        vos_mem_set(( tANI_U8* )&(psessionEntry->probeRespFrame),
                    sizeof(psessionEntry->probeRespFrame), 0);

        /* Can be efficiently updated whenever new IE added
         * in Probe response in future
         */
        if (limUpdateProbeRspTemplateIeBitmapBeacon1(pMac, pBcn1,
                psessionEntry) != eSIR_SUCCESS) {
                    schLog(pMac, LOGE,
                        FL("Failed to build ProbeRsp template"));
        }
    }

    nStatus = dot11fPackBeacon1( pMac, pBcn1, ptr,
                                 SCH_MAX_BEACON_SIZE - offset,
                                 &nBytes );
    if ( DOT11F_FAILED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("Failed to packed a tDot11fBeacon1 (0x%0"
                             "8x.)."), nStatus );
      vos_mem_free(pBcn1);
      vos_mem_free(pBcn2);
      vos_mem_free(pWscProbeRes);
      return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("There were warnings while packing a tDo"
                             "t11fBeacon1 (0x%08x.)."), nStatus );
    }
    /*changed  to correct beacon corruption */
    vos_mem_set(( tANI_U8*) pBcn2, sizeof( tDot11fBeacon2 ), 0);
    psessionEntry->schBeaconOffsetBegin = offset + ( tANI_U16 )nBytes;
    schLog( pMac, LOG1, FL("Initialized beacon begin, offset %d"), offset );

    /*
     * Initialize the 'new' fields at the end of the beacon
     */

    if ((psessionEntry->limSystemRole == eLIM_AP_ROLE) &&
       psessionEntry->dfsIncludeChanSwIe == VOS_TRUE) {
           populate_dot_11_f_ext_chann_switch_ann(pMac,
                           &pBcn2->ext_chan_switch_ann,
                           psessionEntry);
    }

    populate_dot11_supp_operating_classes(pMac, &pBcn2->SuppOperatingClasses,
                                          psessionEntry);
    PopulateDot11fCountry( pMac, &pBcn2->Country, psessionEntry);
    if(pBcn1->Capabilities.qos)
    {
        PopulateDot11fEDCAParamSet( pMac, &pBcn2->EDCAParamSet, psessionEntry);
    }

    if(psessionEntry->lim11hEnable)
    {
      PopulateDot11fPowerConstraints( pMac, &pBcn2->PowerConstraints );
      PopulateDot11fTPCReport( pMac, &pBcn2->TPCReport, psessionEntry);

      /* Need to insert channel switch announcement here */
      if ((LIM_IS_AP_ROLE(psessionEntry) ||
           LIM_IS_P2P_DEVICE_GO(psessionEntry)) &&
           psessionEntry->dfsIncludeChanSwIe == VOS_TRUE) {
         /* Channel switch announcement only if radar is detected
          * and SAP has instructed to announce channel switch IEs
          * in beacon and probe responses
          */
         PopulateDot11fChanSwitchAnn(pMac, &pBcn2->ChanSwitchAnn,
                                     psessionEntry);
#ifdef WLAN_FEATURE_11AC
         /* TODO: If in 11AC mode, wider bw channel switch announcement needs
          * to be called
          */
         /*PopulateDot11fWiderBWChanSwitchAnn(pMac, &pBcn2->WiderBWChanSwitchAnn,
                                            psessionEntry);*/
#endif
         /*
          * Populate the Channel Switch Wrapper Element if
          * SAP operates in 40/80 Mhz Channel Width.
          */
         if (VOS_TRUE == psessionEntry->dfsIncludeChanWrapperIe)
         {
            PopulateDot11fChanSwitchWrapper(pMac,
                                     &pBcn2->ChannelSwitchWrapper,
                                     psessionEntry);
         }
      }
    }

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
    /* populate proprietary IE for MDM device operating in AP-MCC */
    populate_dot11f_avoid_channel_ie(pMac, &pBcn2->QComVendorIE, psessionEntry);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

    if (psessionEntry->dot11mode != WNI_CFG_DOT11_MODE_11B)
        PopulateDot11fERPInfo( pMac, &pBcn2->ERPInfo, psessionEntry );

    if(psessionEntry->htCapability)
    {
        PopulateDot11fHTCaps( pMac,psessionEntry, &pBcn2->HTCaps );
        PopulateDot11fHTInfo( pMac, &pBcn2->HTInfo, psessionEntry );
    }
#ifdef WLAN_FEATURE_11AC
    if(psessionEntry->vhtCapability)
    {
        schLog( pMac, LOGW, FL("Populate VHT IEs in Beacon"));
        PopulateDot11fVHTCaps( pMac, psessionEntry, &pBcn2->VHTCaps );
        PopulateDot11fVHTOperation( pMac, psessionEntry, &pBcn2->VHTOperation);
        isVHTEnabled = eANI_BOOLEAN_TRUE;
        // we do not support multi users yet
        //PopulateDot11fVHTExtBssLoad( pMac, &bcn2.VHTExtBssLoad);
        if(psessionEntry->gLimOperatingMode.present)
            PopulateDot11fOperatingMode( pMac, &pBcn2->OperatingMode, psessionEntry );
    }
#endif

    PopulateDot11fExtCap(pMac, isVHTEnabled, &pBcn2->ExtCap, psessionEntry);

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &pBcn2->ExtSuppRates, psessionEntry );

    if( psessionEntry->pLimStartBssReq != NULL )
    {
          PopulateDot11fWPA( pMac, &psessionEntry->pLimStartBssReq->rsnIE,
                       &pBcn2->WPA );
          PopulateDot11fRSNOpaque( pMac, &psessionEntry->pLimStartBssReq->rsnIE,
                       &pBcn2->RSNOpaque );
#ifdef SAP_AUTH_OFFLOAD
        /* Software AP Authentication Offload feature
         * only support WPA2-PSK AES and we
         * need to update RSNIE for beacon
         */
        sap_auth_offload_update_rsn_ie(pMac, &pBcn2->RSNOpaque);
#endif
    }

    if(psessionEntry->limWmeEnabled)
    {
        PopulateDot11fWMM( pMac, &pBcn2->WMMInfoAp, &pBcn2->WMMParams, &pBcn2->WMMCaps, psessionEntry);
    }
    if (LIM_IS_AP_ROLE(psessionEntry)) {
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            PopulateDot11fBeaconWPSIEs( pMac, &pBcn2->WscBeacon, psessionEntry);
        }
    }
    else
    {
        if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_ENABLE, &tmp) != eSIR_SUCCESS)
            schLog(pMac, LOGP,"Failed to cfg get id %d", WNI_CFG_WPS_ENABLE );

        wpsApEnable = tmp & WNI_CFG_WPS_ENABLE_AP;

        if (wpsApEnable)
        {
            PopulateDot11fWsc(pMac, &pBcn2->WscBeacon);
        }

        if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_BEGIN)
        {
            PopulateDot11fWscRegistrarInfo(pMac, &pBcn2->WscBeacon);
            pMac->lim.wscIeInfo.wscEnrollmentState = eLIM_WSC_ENROLL_IN_PROGRESS;
        }

        if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_END)
        {
            DePopulateDot11fWscRegistrarInfo(pMac, &pBcn2->WscBeacon);
            pMac->lim.wscIeInfo.wscEnrollmentState = eLIM_WSC_ENROLL_NOOP;
        }
    }

    if (LIM_IS_AP_ROLE(psessionEntry)) {
        /* Can be efficiently updated whenever new IE added  in Probe response in future */
        limUpdateProbeRspTemplateIeBitmapBeacon2(pMac,pBcn2,&psessionEntry->DefProbeRspIeBitmap[0],
                                                &psessionEntry->probeRespFrame);

        /* update probe response WPS IE instead of beacon WPS IE
        * */
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            if(psessionEntry->APWPSIEs.SirWPSProbeRspIE.FieldPresent)
            {
                PopulateDot11fProbeResWPSIEs(pMac, pWscProbeRes, psessionEntry);
            }
            else
            {
                pWscProbeRes->present = 0;
            }
            if(pWscProbeRes->present)
            {
                SetProbeRspIeBitmap(&psessionEntry->DefProbeRspIeBitmap[0],SIR_MAC_WPA_EID);
                vos_mem_copy((void *)&psessionEntry->probeRespFrame.WscProbeRes,
                             (void *)pWscProbeRes,
                             sizeof(tDot11fIEWscProbeRes));
            }
        }

    }

    addnie_present = (psessionEntry->addIeParams.probeRespBCNDataLen != 0);
    if (addnie_present) {
        addn_ielen = psessionEntry->addIeParams.probeRespBCNDataLen;
        addn_ie = vos_mem_malloc(addn_ielen);
        if (!addn_ie) {
             schLog(pMac, LOGE, FL("addn_ie malloc failed"));
             vos_mem_free(pBcn1);
             vos_mem_free(pBcn2);
             vos_mem_free(pWscProbeRes);
             return eSIR_MEM_ALLOC_FAILED;
        }
        vos_mem_copy(addn_ie, psessionEntry->addIeParams.probeRespBCNData_buff,
                     addn_ielen);

        vos_mem_set((uint8_t *)&extracted_extcap, sizeof(tDot11fIEExtCap), 0);
        status = lim_strip_extcap_update_struct(pMac, addn_ie, &addn_ielen,
                                                &extracted_extcap);
        if (eSIR_SUCCESS != status) {
            extcap_present = false;
            schLog(pMac, LOG1, FL("extcap not extracted"));
        }
        /* merge extcap IE */
        if (extcap_present)
            lim_merge_extcap_struct(&pBcn2->ExtCap, &extracted_extcap);

    }

    nStatus = dot11fPackBeacon2( pMac, pBcn2,
                                 psessionEntry->pSchBeaconFrameEnd,
                                 SCH_MAX_BEACON_SIZE, &nBytes );
    if ( DOT11F_FAILED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("Failed to packed a tDot11fBeacon2 (0x%0"
                             "8x.)."), nStatus );
      vos_mem_free(pBcn1);
      vos_mem_free(pBcn2);
      vos_mem_free(pWscProbeRes);
      vos_mem_free(addn_ie);
      return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
      schLog( pMac, LOGE, FL("There were warnings while packing a tDo"
                             "t11fBeacon2 (0x%08x.)."), nStatus );
    }

    pExtraIe = psessionEntry->pSchBeaconFrameEnd + nBytes;
    extraIeOffset = nBytes;

    if (addn_ielen > 0)
        schAppendAddnIE(pMac, psessionEntry,
                    psessionEntry->pSchBeaconFrameEnd + nBytes,
                    SCH_MAX_BEACON_SIZE, &nBytes, addn_ie, addn_ielen);

    psessionEntry->schBeaconOffsetEnd = ( tANI_U16 )nBytes;

    extraIeLen = nBytes - extraIeOffset;

    //Get the p2p Ie Offset
    status = schGetP2pIeOffset(pExtraIe, extraIeLen, &p2pIeOffset);

    if(eSIR_SUCCESS == status)
    {
       //Update the P2P Ie Offset
       pMac->sch.schObject.p2pIeOffset =
                    psessionEntry->schBeaconOffsetBegin + TIM_IE_SIZE +
                    extraIeOffset + p2pIeOffset;
    }
    else
    {
       pMac->sch.schObject.p2pIeOffset = 0;
    }

    schLog( pMac, LOG1, FL("Initialized beacon end, offset %d"),
            psessionEntry->schBeaconOffsetEnd );

    pMac->sch.schObject.fBeaconChanged = 1;
    vos_mem_free(pBcn1);
    vos_mem_free(pBcn2);
    vos_mem_free(pWscProbeRes);
    vos_mem_free(addn_ie);
    return eSIR_SUCCESS;
}

tSirRetStatus limUpdateProbeRspTemplateIeBitmapBeacon1(tpAniSirGlobal pMac,
                                              tDot11fBeacon1* beacon1,
                                              tpPESession psessionEntry)
{
    tANI_U32* DefProbeRspIeBitmap;
    tDot11fProbeResponse* prb_rsp;

    if (!psessionEntry) {
        schLog(pMac, LOGE, FL("PESession is null!"));
        return eSIR_FAILURE;
    }

    DefProbeRspIeBitmap = &psessionEntry->DefProbeRspIeBitmap[0];
    prb_rsp = &psessionEntry->probeRespFrame;

    prb_rsp->BeaconInterval = beacon1->BeaconInterval;
    vos_mem_copy((void *)&prb_rsp->Capabilities, (void *)&beacon1->Capabilities,
                 sizeof(beacon1->Capabilities));

    /* SSID */
    if(beacon1->SSID.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_SSID_EID);
        /* populating it, because probe response has to go with
         * SSID even in hidden case
         */
        PopulateDot11fSSID(pMac, &psessionEntry->ssId, &prb_rsp->SSID);
    }
    /* supported rates */
    if(beacon1->SuppRates.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_RATESET_EID);
        vos_mem_copy((void *)&prb_rsp->SuppRates, (void *)&beacon1->SuppRates,
                     sizeof(beacon1->SuppRates));

    }
    /* DS Parameter set */
    if(beacon1->DSParams.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_DS_PARAM_SET_EID);
        vos_mem_copy((void *)&prb_rsp->DSParams, (void *)&beacon1->DSParams,
                      sizeof(beacon1->DSParams));

    }

    /* IBSS params will not be present in the Beacons transmitted by AP */

    return eSIR_SUCCESS;
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
        vos_mem_copy((void *)&prb_rsp->Country, (void *)&beacon2->Country,
                     sizeof(beacon2->Country));

    }
    /* Power constraint */
    if(beacon2->PowerConstraints.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_PWR_CONSTRAINT_EID);
        vos_mem_copy((void *)&prb_rsp->PowerConstraints, (void *)&beacon2->PowerConstraints,
                     sizeof(beacon2->PowerConstraints));

    }
    /* Channel Switch Announcement SIR_MAC_CHNL_SWITCH_ANN_EID */
    if(beacon2->ChanSwitchAnn.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_CHNL_SWITCH_ANN_EID);
        vos_mem_copy((void *)&prb_rsp->ChanSwitchAnn, (void *)&beacon2->ChanSwitchAnn,
                     sizeof(beacon2->ChanSwitchAnn));

    }

    /* EXT Channel Switch Announcement CHNL_EXTENDED_SWITCH_ANN_EID*/
    if (beacon2->ext_chan_switch_ann.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,
               SIR_MAC_CHNL_EXTENDED_SWITCH_ANN_EID);
        vos_mem_copy((void *)&prb_rsp->ext_chan_switch_ann,
            (void *)&beacon2->ext_chan_switch_ann,
             sizeof(beacon2->ext_chan_switch_ann));
    }
    /* Supported operating class */
    if(beacon2->SuppOperatingClasses.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_OPERATING_CLASS_EID);
        vos_mem_copy((void *)&prb_rsp->SuppOperatingClasses,
                     (void *)&beacon2->SuppOperatingClasses,
                     sizeof(beacon2->SuppOperatingClasses));
    }

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
    if(beacon2->QComVendorIE.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap, SIR_MAC_QCOM_VENDOR_EID);
        vos_mem_copy((void *)&prb_rsp->QComVendorIE,
                     (void *)&beacon2->QComVendorIE,
                     sizeof(beacon2->QComVendorIE));
    }
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

    /* ERP information */
    if(beacon2->ERPInfo.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_ERP_INFO_EID);
        vos_mem_copy((void *)&prb_rsp->ERPInfo, (void *)&beacon2->ERPInfo,
                     sizeof(beacon2->ERPInfo));

    }
    /* Extended supported rates */
    if(beacon2->ExtSuppRates.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_EXTENDED_RATE_EID);
        vos_mem_copy((void *)&prb_rsp->ExtSuppRates, (void *)&beacon2->ExtSuppRates,
                     sizeof(beacon2->ExtSuppRates));

    }

    /* WPA */
    if(beacon2->WPA.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        vos_mem_copy((void *)&prb_rsp->WPA, (void *)&beacon2->WPA,
                     sizeof(beacon2->WPA));

    }

    /* RSN */
    if(beacon2->RSNOpaque.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_RSN_EID);
        vos_mem_copy((void *)&prb_rsp->RSNOpaque, (void *)&beacon2->RSNOpaque,
                     sizeof(beacon2->RSNOpaque));
    }

    /* EDCA Parameter set */
    if(beacon2->EDCAParamSet.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_EDCA_PARAM_SET_EID);
        vos_mem_copy((void *)&prb_rsp->EDCAParamSet, (void *)&beacon2->EDCAParamSet,
                     sizeof(beacon2->EDCAParamSet));

    }
    /* Vendor specific - currently no vendor specific IEs added */
    /* Requested IEs - currently we are not processing this will be added later */
    //HT capability IE
    if(beacon2->HTCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_HT_CAPABILITIES_EID);
        vos_mem_copy((void *)&prb_rsp->HTCaps, (void *)&beacon2->HTCaps,
                     sizeof(beacon2->HTCaps));
    }
    // HT Info IE
    if(beacon2->HTInfo.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_HT_INFO_EID);
        vos_mem_copy((void *)&prb_rsp->HTInfo, (void *)&beacon2->HTInfo,
                     sizeof(beacon2->HTInfo));
    }

#ifdef WLAN_FEATURE_11AC
    if(beacon2->VHTCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_CAPABILITIES_EID);
        vos_mem_copy((void *)&prb_rsp->VHTCaps, (void *)&beacon2->VHTCaps,
                     sizeof(beacon2->VHTCaps));
    }
    if(beacon2->VHTOperation.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_OPERATION_EID);
        vos_mem_copy((void *)&prb_rsp->VHTOperation, (void *)&beacon2->VHTOperation,
                     sizeof(beacon2->VHTOperation));
    }
    if(beacon2->VHTExtBssLoad.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_VHT_EXT_BSS_LOAD_EID);
        vos_mem_copy((void *)&prb_rsp->VHTExtBssLoad, (void *)&beacon2->VHTExtBssLoad,
                     sizeof(beacon2->VHTExtBssLoad));
    }
#endif

    //WMM IE
    if(beacon2->WMMParams.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        vos_mem_copy((void *)&prb_rsp->WMMParams, (void *)&beacon2->WMMParams,
                     sizeof(beacon2->WMMParams));
    }
    //WMM capability - most of the case won't be present
    if(beacon2->WMMCaps.present)
    {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap,SIR_MAC_WPA_EID);
        vos_mem_copy((void *)&prb_rsp->WMMCaps, (void *)&beacon2->WMMCaps,
                     sizeof(beacon2->WMMCaps));
    }

    /* Extended Capability */
    if (beacon2->ExtCap.present) {
        SetProbeRspIeBitmap(DefProbeRspIeBitmap, DOT11F_EID_EXTCAP);
        vos_mem_copy((void *)&prb_rsp->ExtCap, (void *)&beacon2->ExtCap,
                     sizeof(beacon2->ExtCap));
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
        for (i=0; i < psessionEntry->schBeaconOffsetEnd; i++)
            psessionEntry->pSchBeaconFrameBegin[size++] = psessionEntry->pSchBeaconFrameEnd[i];
    }

    // Update the beacon length
    pBeacon = (tpAniBeaconStruct) psessionEntry->pSchBeaconFrameBegin;
    // Do not include the beaconLength indicator itself
    if (length == 0)
    {
        pBeacon->beaconLength = 0;
        // Dont copy entire beacon, Copy length field alone
        size = 4;
    }
    else
        pBeacon->beaconLength = (tANI_U32) size - sizeof( tANI_U32 );

    // write size bytes from pSchBeaconFrameBegin
    PELOG2(schLog(pMac, LOG2, FL("Beacon size - %d bytes"), size);)
    PELOG2(sirDumpBuf(pMac, SIR_SCH_MODULE_ID, LOG2, psessionEntry->pSchBeaconFrameBegin, size);)

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
        if( eSIR_SUCCESS != schSendBeaconReq( pMac, psessionEntry->pSchBeaconFrameBegin,
                                              size, psessionEntry))
            PELOGE(schLog(pMac, LOGE, FL("schSendBeaconReq() returned an error (zsize %d)"), size);)
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
    tANI_U32 beaconSize;
    tpPESession psessionEntry;
    tANI_U8 sessionId;

    if((psessionEntry = peFindSessionByBssid(pMac,pMsg->bssId, &sessionId))== NULL)
    {
        PELOGE(schLog(pMac, LOGE, FL("session lookup fails"));)
        goto end;
    }


    beaconSize = psessionEntry->schBeaconOffsetBegin;

    // If SME is not in normal mode, no need to generate beacon
    if (psessionEntry->limSmeState  != eLIM_SME_NORMAL_STATE)
    {
        PELOGE(schLog(pMac, LOG1, FL("PreBeaconInd received in invalid state: %d"), psessionEntry->limSmeState);)
        goto end;
    }

    switch(GET_LIM_SYSTEM_ROLE(psessionEntry)) {
    case eLIM_STA_IN_IBSS_ROLE:
    case eLIM_BT_AMP_AP_ROLE:
    case eLIM_BT_AMP_STA_ROLE:
        /* Generate IBSS parameter set */
        if(psessionEntry->statypeForBss == STA_ENTRY_SELF)
            writeBeaconToMemory(pMac, (tANI_U16) beaconSize,
                               (tANI_U16)beaconSize, psessionEntry);
        else
            PELOGE(schLog(pMac, LOGE, FL("can not send beacon for PEER session entry"));)
        break;

    case eLIM_AP_ROLE: {
         tANI_U8 *ptr = &psessionEntry->pSchBeaconFrameBegin[psessionEntry->schBeaconOffsetBegin];
         tANI_U16 timLength = 0;

         if (psessionEntry->statypeForBss == STA_ENTRY_SELF) {
             pmmGenerateTIM(pMac, &ptr, &timLength, psessionEntry->dtimPeriod);
             beaconSize += 2 + timLength;
             writeBeaconToMemory(pMac, (tANI_U16) beaconSize,
                                (tANI_U16)beaconSize, psessionEntry);
         } else
             PELOGE(schLog(pMac, LOGE, FL("can not send beacon for PEER session entry"));)
         }
         break;

    default:
        PELOGE(schLog(pMac, LOGE,
               FL("Error-PE has Receive PreBeconGenIndication when System is in %d role"),
               GET_LIM_SYSTEM_ROLE(psessionEntry));)
    }

end:
      vos_mem_free(pMsg);

}
