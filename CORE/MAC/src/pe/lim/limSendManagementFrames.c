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

/**
 * \file limSendManagementFrames.c
 *
 * \brief Code for preparing and sending 802.11 Management frames
 *
 * Copyright (C) 2005-2007 Airgo Networks, Incorporated
 *
 */

#include "sirApi.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halDataStruct.h"
#endif
#include "cfgApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "dot11f.h"
#include "limStaHashApi.h"
#include "schApi.h"
#include "limSendMessages.h"
#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif

#ifdef FEATURE_WLAN_CCX
#include <limCcxparserApi.h>
#endif
#include "wlan_qct_wda.h"
#ifdef WLAN_FEATURE_11W
#include "dot11fdefs.h"
#endif


////////////////////////////////////////////////////////////////////////

/// Get an integral configuration item & check return status; if it
/// fails, return.
#define CFG_LIM_GET_INT_NO_STATUS(nStatus, pMac, nItem, cfg )      \
    (nStatus) = wlan_cfgGetInt( (pMac), (nItem), & (cfg) );             \
    if ( eSIR_SUCCESS != (nStatus) )                               \
    {                                                              \
        limLog( (pMac), LOGP, FL("Failed to retrieve "             \
                                 #nItem " from CFG (%d).\n"),      \
                (nStatus) );                                       \
        return;                                                    \
    }

/// Get an text configuration item & check return status; if it fails,
/// return.
#define CFG_LIM_GET_STR_NO_STATUS(nStatus, pMac, nItem, cfg, nCfg, \
                              nMaxCfg)                             \
    (nCfg) = (nMaxCfg);                                            \
    (nStatus) = wlan_cfgGetStr( (pMac), (nItem), (cfg), & (nCfg) );     \
    if ( eSIR_SUCCESS != (nStatus) )                               \
    {                                                              \
        limLog( (pMac), LOGP, FL("Failed to retrieve "             \
                                 #nItem " from CFG (%d).\n"),      \
                (nStatus) );                                       \
        return;                                                    \
    }

/**
 *
 * \brief This function is called by various LIM modules to prepare the
 * 802.11 frame MAC header
 *
 *
 * \param  pMac Pointer to Global MAC structure
 *
 * \param pBD Pointer to the frame buffer that needs to be populate
 *
 * \param type Type of the frame
 *
 * \param subType Subtype of the frame
 *
 * \return eHalStatus
 *
 *
 * The pFrameBuf argument points to the beginning of the frame buffer to
 * which - a) The 802.11 MAC header is set b) Following this MAC header
 * will be the MGMT frame payload The payload itself is populated by the
 * caller API
 *
 *
 */

tSirRetStatus limPopulateMacHeader( tpAniSirGlobal pMac,
                             tANI_U8* pBD,
                             tANI_U8 type,
                             tANI_U8 subType,
                             tSirMacAddr peerAddr ,tSirMacAddr selfMacAddr)
{
    tSirRetStatus   statusCode = eSIR_SUCCESS;
    tpSirMacMgmtHdr pMacHdr;
    
    /// Prepare MAC management header
    pMacHdr = (tpSirMacMgmtHdr) (pBD);

    // Prepare FC
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = type;
    pMacHdr->fc.subType = subType;

    // Prepare Address 1
    palCopyMemory( pMac->hHdd,
                   (tANI_U8 *) pMacHdr->da,
                   (tANI_U8 *) peerAddr,
                   sizeof( tSirMacAddr ));

    // Prepare Address 2
    #if 0
    if ((statusCode = wlan_cfgGetStr(pMac, WNI_CFG_STA_ID, (tANI_U8 *) pMacHdr->sa,
                                &cfgLen)) != eSIR_SUCCESS)
    {
        // Could not get STA_ID from CFG. Log error.
        limLog( pMac, LOGP,
                FL("Failed to retrive STA_ID\n"));
        return statusCode;
    }
    #endif// TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->sa,selfMacAddr);

    // Prepare Address 3
    palCopyMemory( pMac->hHdd,
                   (tANI_U8 *) pMacHdr->bssId,
                   (tANI_U8 *) peerAddr,
                   sizeof( tSirMacAddr ));
    return statusCode;
} /*** end limPopulateMacHeader() ***/

/**
 * \brief limSendProbeReqMgmtFrame
 *
 *
 * \param  pMac Pointer to Global MAC structure
 *
 * \param  pSsid SSID to be sent in Probe Request frame
 *
 * \param  bssid BSSID to be sent in Probe Request frame
 *
 * \param  nProbeDelay probe delay to be used before sending Probe Request
 * frame
 *
 * \param nChannelNum Channel # on which the Probe Request is going out
 *
 * \param nAdditionalIELen if non-zero, include pAdditionalIE in the Probe Request frame
 *
 * \param pAdditionalIE if nAdditionalIELen is non zero, include this field in the Probe Request frame
 *
 * This function is called by various LIM modules to send Probe Request frame
 * during active scan/learn phase.
 * Probe request is sent out in the following scenarios:
 * --heartbeat failure:  session needed
 * --join req:           session needed
 * --foreground scan:    no session
 * --background scan:    no session
 * --schBeaconProcessing:  to get EDCA parameters:  session needed
 *
 *
 */
tSirRetStatus
limSendProbeReqMgmtFrame(tpAniSirGlobal pMac,
                         tSirMacSSid   *pSsid,
                         tSirMacAddr    bssid,
                         tANI_U8        nChannelNum,
                         tSirMacAddr    SelfMacAddr,
                         tANI_U32 dot11mode,
                         tANI_U32 nAdditionalIELen, 
                         tANI_U8 *pAdditionalIE)
{
    tDot11fProbeRequest pr;
    tANI_U32            nStatus, nBytes, nPayload;
    tSirRetStatus       nSirStatus;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    tpPESession         psessionEntry;
    tANI_U8             sessionId;
#ifdef WLAN_FEATURE_P2P
    tANI_U8             *p2pIe = NULL;
#endif
    tANI_U8             txFlag = 0;

#ifndef GEN4_SCAN
    return eSIR_FAILURE;
#endif

#if defined ( ANI_DVT_DEBUG )
    return eSIR_FAILURE;
#endif

    /*
    * session context may or may not be present, when probe request needs to be sent out.
    * following cases exist:
        * --heartbeat failure:  session needed
    * --join req:           session needed
    * --foreground scan:    no session
    * --background scan:    no session
    * --schBeaconProcessing:  to get EDCA parameters:  session needed
    * If session context does not exist, some IEs will be populated from CFGs, 
    * e.g. Supported and Extended rate set IEs
    */
    psessionEntry = peFindSessionByBssid(pMac,bssid,&sessionId);

    // The scheme here is to fill out a 'tDot11fProbeRequest' structure
    // and then hand it off to 'dot11fPackProbeRequest' (for
    // serialization).  We start by zero-initializing the structure:
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&pr, sizeof( pr ) );

    // & delegating to assorted helpers:
    PopulateDot11fSSID( pMac, pSsid, &pr.SSID );

#ifdef WLAN_FEATURE_P2P
    if( nAdditionalIELen && pAdditionalIE )
    {
        p2pIe = limGetP2pIEPtr(pMac, pAdditionalIE, nAdditionalIELen);
    }
    if( p2pIe != NULL)
    {
        /* In the below API pass channel number > 14, do that it fills only
         * 11a rates in supported rates */
        PopulateDot11fSuppRates( pMac, 15, &pr.SuppRates,psessionEntry);
    }
    else
    {
#endif
        PopulateDot11fSuppRates( pMac, nChannelNum, 
                                               &pr.SuppRates,psessionEntry);

        if ( WNI_CFG_DOT11_MODE_11B != dot11mode )
        {
            PopulateDot11fExtSuppRates1( pMac, nChannelNum, &pr.ExtSuppRates );
        }
#ifdef WLAN_FEATURE_P2P
    }
#endif

#if defined WLAN_FEATURE_VOWIFI
    //Table 7-14 in IEEE Std. 802.11k-2008 says
    //DS params "can" be present in RRM is disabled and "is" present if 
    //RRM is enabled. It should be ok even if we add it into probe req when 
    //RRM is not enabled. 
    PopulateDot11fDSParams( pMac, &pr.DSParams, nChannelNum, psessionEntry );
    //Call RRM module to get the tx power for management used.
    {
       tANI_U8 txPower = (tANI_U8) rrmGetMgmtTxPower( pMac, psessionEntry );
       PopulateDot11fWFATPC( pMac, &pr.WFATPC, txPower, 0 );
    }
#endif
    pMac->lim.htCapability = IS_DOT11_MODE_HT(dot11mode);

    if (psessionEntry != NULL ) {
       psessionEntry->htCapabality = IS_DOT11_MODE_HT(dot11mode);
       //Include HT Capability IE
       if (psessionEntry->htCapabality)
       {
           PopulateDot11fHTCaps( pMac, &pr.HTCaps );
       }
    } else {
           if (pMac->lim.htCapability)
           {
               PopulateDot11fHTCaps( pMac, &pr.HTCaps );
           }
    }

    // That's it-- now we pack it.  First, how much space are we going to
    // need?
    nStatus = dot11fGetPackedProbeRequestSize( pMac, &pr, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Probe Request (0x%08x).\n"), nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Probe Request ("
                               "0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAdditionalIELen;
  
    // Ok-- try to allocate some memory:
    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Pro"
                               "be Request.\n"), nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_REQ, bssid ,SelfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return nSirStatus;      // allocated!
    }

    // That done, pack the Probe Request:
    nStatus = dot11fPackProbeRequest( pMac, &pr, pFrame +
                                      sizeof( tSirMacMgmtHdr ),
                                      nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Probe Request (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a P"
                               "robe Request (0x%08x).\n") );
    }

    // Append any AddIE if present.
    if( nAdditionalIELen )
    {
        palCopyMemory( pMac->hHdd, pFrame+sizeof(tSirMacMgmtHdr)+nPayload, 
                                                    pAdditionalIE, nAdditionalIELen );
        nPayload += nAdditionalIELen;
    }

    /* If this probe request is sent during P2P Search State, then we need 
     * to send it at OFDM rate. 
     */
    if( ( SIR_BAND_5_GHZ == limGetRFBand(nChannelNum))
#ifdef WLAN_FEATURE_P2P
      || (( pMac->lim.gpLimMlmScanReq != NULL) &&
          pMac->lim.gpLimMlmScanReq->p2pSearch )
#endif
      ) 
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME; 
    }


    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) sizeof(tSirMacMgmtHdr) + nPayload,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("could not send Probe Request frame!\n" ));
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;
} // End limSendProbeReqMgmtFrame.

#ifdef WLAN_FEATURE_P2P
tSirRetStatus limGetAddnIeForProbeResp(tpAniSirGlobal pMac,
                              tANI_U8* addIE, tANI_U16 *addnIELen,
                              tANI_U8 probeReqP2pIe)
{
    /* If Probe request doesn't have P2P IE, then take out P2P IE
       from additional IE */
    if(!probeReqP2pIe)
    {
        tANI_U8* tempbuf = NULL;
        tANI_U16 tempLen = 0;
        int left = *addnIELen;
        v_U8_t *ptr = addIE;
        v_U8_t elem_id, elem_len;

        if(NULL == addIE)
        {
           PELOGE(limLog(pMac, LOGE,
                 FL(" NULL addIE pointer"));)
            return eSIR_FAILURE;
        }

        if( (palAllocateMemory(pMac->hHdd, (void**)&tempbuf,
             left)) != eHAL_STATUS_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE,
                 FL("Unable to allocate memory to store addn IE"));)
            return eSIR_MEM_ALLOC_FAILED;
        }

        while(left >= 2)
        {
            elem_id  = ptr[0];
            elem_len = ptr[1];
            left -= 2;
            if(elem_len > left)
            {
                limLog( pMac, LOGE,
                   FL("****Invalid IEs eid = %d elem_len=%d left=%d*****\n"),
                                                   elem_id,elem_len,left);
                palFreeMemory(pMac->hHdd, tempbuf);
                return eSIR_FAILURE;
            }
            if ( !( (SIR_MAC_EID_VENDOR == elem_id) &&
                   (memcmp(&ptr[2], SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE)==0) ) )
            {
                palCopyMemory ( pMac->hHdd, tempbuf + tempLen, &ptr[0], elem_len + 2);
                tempLen += (elem_len + 2);
            }
            left -= elem_len;
            ptr += (elem_len + 2);
       }
       palCopyMemory ( pMac->hHdd, addIE, tempbuf, tempLen);
       *addnIELen = tempLen;
       palFreeMemory(pMac->hHdd, tempbuf);
    }
    return eSIR_SUCCESS;
}
#endif

void
limSendProbeRspMgmtFrame(tpAniSirGlobal pMac,
                         tSirMacAddr    peerMacAddr,
                         tpAniSSID      pSsid,
                         short          nStaId,
                         tANI_U8        nKeepAlive,
                         tpPESession psessionEntry,
                         tANI_U8        probeReqP2pIe)
{
    tDot11fProbeResponse frm;
    tSirRetStatus        nSirStatus;
    tANI_U32             cfg, nPayload, nBytes, nStatus;
    tpSirMacMgmtHdr      pMacHdr;
    tANI_U8             *pFrame;
    void                *pPacket;
    eHalStatus           halstatus;
    tANI_U32             addnIEPresent;
    tANI_U32             addnIE1Len=0;
    tANI_U32             addnIE2Len=0;
    tANI_U32             addnIE3Len=0;
    tANI_U16             totalAddnIeLen = 0;
    tANI_U32             wpsApEnable=0, tmp;
    tANI_U8              txFlag = 0;
    tANI_U8              *addIE = NULL;
#ifdef WLAN_FEATURE_P2P
    tANI_U8             *pP2pIe = NULL;
    tANI_U8              noaLen = 0;
    tANI_U8              total_noaLen = 0;
    tANI_U8              noaStream[SIR_MAX_NOA_ATTR_LEN 
                                           + SIR_P2P_IE_HEADER_LEN];
    tANI_U8              noaIe[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
#endif
  
    if(pMac->gDriverType == eDRIVER_TYPE_MFG)         // We don't answer requests
    {
        return;                     // in this case.
    }

    if(NULL == psessionEntry)
    {
        return;
    }
    
    // Fill out 'frm', after which we'll just hand the struct off to
    // 'dot11fPackProbeResponse'.
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    // Timestamp to be updated by TFP, below.

    // Beacon Interval:
#ifdef WLAN_SOFTAP_FEATURE
    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        frm.BeaconInterval.interval = pMac->sch.schObject.gSchBeaconInterval;        
    }
    else
    {
#endif
    CFG_LIM_GET_INT_NO_STATUS( nSirStatus, pMac,
                               WNI_CFG_BEACON_INTERVAL, cfg );
    frm.BeaconInterval.interval = ( tANI_U16 ) cfg;
#ifdef WLAN_SOFTAP_FEATURE
    }
#endif


    PopulateDot11fCapabilities( pMac, &frm.Capabilities, psessionEntry );
    PopulateDot11fSSID( pMac, ( tSirMacSSid* )pSsid, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                             &frm.SuppRates,psessionEntry);

    PopulateDot11fDSParams( pMac, &frm.DSParams, psessionEntry->currentOperChannel, psessionEntry);
    PopulateDot11fIBSSParams( pMac, &frm.IBSSParams, psessionEntry );

#ifdef ANI_PRODUCT_TYPE_AP
    PopulateDot11fCFParams( pMac, &frm.Capabilities, &frm.CFParams );
#endif // AP Image

#ifdef WLAN_SOFTAP_FEATURE
    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if(psessionEntry->wps_state != SAP_WPS_DISABLED)
        {
            PopulateDot11fProbeResWPSIEs(pMac, &frm.WscProbeRes, psessionEntry);
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
        PopulateDot11fWscInProbeRes(pMac, &frm.WscProbeRes);
    }

    if (pMac->lim.wscIeInfo.probeRespWscEnrollmentState == eLIM_WSC_ENROLL_BEGIN)
    {
        PopulateDot11fWscRegistrarInfoInProbeRes(pMac, &frm.WscProbeRes);
        pMac->lim.wscIeInfo.probeRespWscEnrollmentState = eLIM_WSC_ENROLL_IN_PROGRESS;
    }

    if (pMac->lim.wscIeInfo.wscEnrollmentState == eLIM_WSC_ENROLL_END)
    {
        DePopulateDot11fWscRegistrarInfoInProbeRes(pMac, &frm.WscProbeRes);
        pMac->lim.wscIeInfo.probeRespWscEnrollmentState = eLIM_WSC_ENROLL_NOOP;
    }
#ifdef WLAN_SOFTAP_FEATURE
    }
#endif

    PopulateDot11fCountry( pMac, &frm.Country, psessionEntry);
    PopulateDot11fEDCAParamSet( pMac, &frm.EDCAParamSet, psessionEntry);

#ifdef ANI_PRODUCT_TYPE_AP
    if( pMac->lim.gLim11hEnable )
    {
        PopulateDot11fPowerConstraints( pMac, &frm.PowerConstraints );
        PopulateDot11fTPCReport( pMac, &frm.TPCReport, psessionEntry);

        // If .11h isenabled & channel switching is not already started and
        // we're in either PRIMARY_ONLY or PRIMARY_AND_SECONDARY state, then
        // populate 802.11h channel switch IE
        if (( pMac->lim.gLimChannelSwitch.switchCount != 0 ) &&
             ( pMac->lim.gLimChannelSwitch.state ==
               eLIM_CHANNEL_SWITCH_PRIMARY_ONLY ||
               pMac->lim.gLimChannelSwitch.state ==
               eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY ) )
        {
            PopulateDot11fChanSwitchAnn( pMac, &frm.ChanSwitchAnn );
            PopulateDot11fExtChanSwitchAnn(pMac, &frm.ExtChanSwitchAnn);
        }
    }
#endif

    if (psessionEntry->dot11mode != WNI_CFG_DOT11_MODE_11B)
        PopulateDot11fERPInfo( pMac, &frm.ERPInfo, psessionEntry);


    // N.B. In earlier implementations, the RSN IE would be placed in
    // the frame here, before the WPA IE, if 'RSN_BEFORE_WPA' was defined.
    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &frm.ExtSuppRates, psessionEntry );

    //Populate HT IEs, when operating in 11n or Taurus modes.
    if ( psessionEntry->htCapabality )
    {
        PopulateDot11fHTCaps( pMac, &frm.HTCaps );
#ifdef WLAN_SOFTAP_FEATURE
        PopulateDot11fHTInfo( pMac, &frm.HTInfo, psessionEntry );
#else
        PopulateDot11fHTInfo( pMac, &frm.HTInfo );
#endif
    }

    if ( psessionEntry->pLimStartBssReq ) 
    {
      PopulateDot11fWPA( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &frm.WPA );
      PopulateDot11fRSN( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &frm.RSN );
    }

    PopulateDot11fWMM( pMac, &frm.WMMInfoAp, &frm.WMMParams, &frm.WMMCaps, psessionEntry );

#if defined(FEATURE_WLAN_WAPI)
    if( psessionEntry->pLimStartBssReq ) 
    {
      PopulateDot11fWAPI( pMac, &( psessionEntry->pLimStartBssReq->rsnIE ),
          &frm.WAPI );
    }

#endif // defined(FEATURE_WLAN_WAPI)


    nStatus = dot11fGetPackedProbeResponseSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Probe Response (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeResponse );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Probe Response "
                               "(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    addnIEPresent = false;
    
#ifdef WLAN_FEATURE_P2P
    if( pMac->lim.gpLimRemainOnChanReq )
    {
        nBytes += (pMac->lim.gpLimRemainOnChanReq->length - sizeof( tSirRemainOnChnReq ) );
    }
    //Only use CFG for non-listen mode. This CFG is not working for concurrency
    //In listening mode, probe rsp IEs is passed in the message from SME to PE
    else
#endif
    {

        if (wlan_cfgGetInt(pMac, WNI_CFG_PROBE_RSP_ADDNIE_FLAG,
                           &addnIEPresent) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_FLAG"));
            return;
        }
    }

    if (addnIEPresent)
    {
        if( (palAllocateMemory(pMac->hHdd, (void**)&addIE, 
             WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN*3 )) != eHAL_STATUS_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE,
                 FL("Unable to allocate memory to store addn IE"));)
            return;
        }
        
        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA1, &addnIE1Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA1 length"));
            palFreeMemory(pMac->hHdd, addIE);
            return;
        }
        if (addnIE1Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN && addnIE1Len &&
                     (nBytes + addnIE1Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA1, &addIE[0],
                                     &addnIE1Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA1 String"));
                palFreeMemory(pMac->hHdd, addIE);
                return;
            }
        }

        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA2, &addnIE2Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA2 length"));
            palFreeMemory(pMac->hHdd, addIE);
            return;
        }
        if (addnIE2Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA2_LEN && addnIE2Len &&
                     (nBytes + addnIE2Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA2, &addIE[addnIE1Len],
                                     &addnIE2Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA2 String"));
                palFreeMemory(pMac->hHdd, addIE);
                return;
            }
        }

        //Probe rsp IE available
        if ( eSIR_SUCCESS != wlan_cfgGetStrLen(pMac,
                                  WNI_CFG_PROBE_RSP_ADDNIE_DATA3, &addnIE3Len) )
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA3 length"));
            palFreeMemory(pMac->hHdd, addIE);
            return;
        }
        if (addnIE3Len <= WNI_CFG_PROBE_RSP_ADDNIE_DATA3_LEN && addnIE3Len &&
                     (nBytes + addnIE3Len) <= SIR_MAX_PACKET_SIZE)
        {
            if ( eSIR_SUCCESS != wlan_cfgGetStr(pMac,
                                     WNI_CFG_PROBE_RSP_ADDNIE_DATA3,
                                     &addIE[addnIE1Len + addnIE2Len],
                                     &addnIE3Len) )
            {
                limLog(pMac, LOGP,
                     FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA3 String"));
                palFreeMemory(pMac->hHdd, addIE);
                return;
            }
        }
        totalAddnIeLen = addnIE1Len + addnIE2Len + addnIE3Len;

#ifdef WLAN_FEATURE_P2P
        if(eSIR_SUCCESS != limGetAddnIeForProbeResp(pMac, addIE, &totalAddnIeLen, probeReqP2pIe))
        {
            limLog(pMac, LOGP,
                 FL("Unable to get final Additional IE for Probe Req"));
            palFreeMemory(pMac->hHdd, addIE);
            return;
        }
        nBytes = nBytes + totalAddnIeLen;

        if (probeReqP2pIe)
        {
            pP2pIe = limGetP2pIEPtr(pMac, &addIE[0], totalAddnIeLen);
            if (pP2pIe != NULL)
            {
                //get NoA attribute stream P2P IE
                noaLen = limGetNoaAttrStream(pMac, noaStream, psessionEntry);
                if (noaLen != 0)
                {
                    total_noaLen = limBuildP2pIe(pMac, &noaIe[0], 
                                            &noaStream[0], noaLen); 
                    nBytes = nBytes + total_noaLen;
                }
            }
        }
#endif
    }

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Pro"
                               "be Response.\n"), nBytes );
        if ( addIE != NULL )
        {
            palFreeMemory(pMac->hHdd, addIE);
        }
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_RSP, peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Response (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        if ( addIE != NULL )
        {
            palFreeMemory(pMac->hHdd, addIE);
        }
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
  
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    // That done, pack the Probe Response:
    nStatus = dot11fPackProbeResponse( pMac, &frm, pFrame + sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Probe Response (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        if ( addIE != NULL )
        {
            palFreeMemory(pMac->hHdd, addIE);
        }
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a P"
                               "robe Response (0x%08x).\n") );
    }

    PELOG3(limLog( pMac, LOG3, FL("Sending Probe Response frame to ") );
    limPrintMacAddr( pMac, peerMacAddr, LOG3 );)

    pMac->sys.probeRespond++;

#ifdef WLAN_FEATURE_P2P
    if( pMac->lim.gpLimRemainOnChanReq )
    {
        palCopyMemory ( pMac->hHdd, pFrame+sizeof(tSirMacMgmtHdr)+nPayload,
          pMac->lim.gpLimRemainOnChanReq->probeRspIe, (pMac->lim.gpLimRemainOnChanReq->length - sizeof( tSirRemainOnChnReq )) );
    }
#endif

    if ( addnIEPresent )
    {
        if (palCopyMemory ( pMac->hHdd, pFrame+sizeof(tSirMacMgmtHdr)+nPayload,
             &addIE[0], totalAddnIeLen) != eHAL_STATUS_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Additional Probe Rp IE request failed while Appending: %x"),halstatus);
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                            ( void* ) pFrame, ( void* ) pPacket );
            if ( addIE != NULL )
            {
                palFreeMemory(pMac->hHdd, addIE);
            }
            return;
        }
    }
#ifdef WLAN_FEATURE_P2P
    if (noaLen != 0)
    {
        if (palCopyMemory ( pMac->hHdd, &pFrame[nBytes - (total_noaLen)],
                            &noaIe[0], total_noaLen) != eHAL_STATUS_SUCCESS)
        {
            limLog(pMac, LOGE,
                  FL("Not able to insert NoA because of length constraint"));
        }
    }
#endif

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue Probe Response frame in high priority WQ
    halstatus = halTxFrame( ( tHalHandle ) pMac, pPacket,
                            ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_LOW,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Could not send Probe Response.\n") );
        //Pkt will be freed up by the callback
    }

    if ( addIE != NULL )
    {
        palFreeMemory(pMac->hHdd, addIE);
    }

} // End limSendProbeRspMgmtFrame.

void
limSendAddtsReqActionFrame(tpAniSirGlobal    pMac,
                           tSirMacAddr       peerMacAddr,
                           tSirAddtsReqInfo *pAddTS,
                           tpPESession       psessionEntry)
{
    tANI_U16               i;
    tANI_U8               *pFrame;
    tSirRetStatus          nSirStatus;
    tDot11fAddTSRequest    AddTSReq;
    tDot11fWMMAddTSRequest WMMAddTSReq;
    tANI_U32               nPayload, nBytes, nStatus;
    tpSirMacMgmtHdr        pMacHdr;
    void                  *pPacket;
#ifdef FEATURE_WLAN_CCX
    tANI_U32               phyMode;
#endif
    eHalStatus             halstatus;
    tANI_U8                txFlag = 0;

    if(NULL == psessionEntry)
    {
           return;
    }

    if ( ! pAddTS->wmeTspecPresent )
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&AddTSReq, sizeof( AddTSReq ) );

        AddTSReq.Action.action     = SIR_MAC_QOS_ADD_TS_REQ;
        AddTSReq.DialogToken.token = pAddTS->dialogToken;
        AddTSReq.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        if ( pAddTS->lleTspecPresent )
        {
            PopulateDot11fTSPEC( &pAddTS->tspec, &AddTSReq.TSPEC );
        }
        else
        {
            PopulateDot11fWMMTSPEC( &pAddTS->tspec, &AddTSReq.WMMTSPEC );
        }

        if ( pAddTS->lleTspecPresent )
        {
            AddTSReq.num_WMMTCLAS = 0;
            AddTSReq.num_TCLAS = pAddTS->numTclas;
            for ( i = 0; i < pAddTS->numTclas; ++i)
            {
                PopulateDot11fTCLAS( pMac, &pAddTS->tclasInfo[i],
                                     &AddTSReq.TCLAS[i] );
            }
        }
        else
        {
            AddTSReq.num_TCLAS = 0;
            AddTSReq.num_WMMTCLAS = pAddTS->numTclas;
            for ( i = 0; i < pAddTS->numTclas; ++i)
            {
                PopulateDot11fWMMTCLAS( pMac, &pAddTS->tclasInfo[i],
                                        &AddTSReq.WMMTCLAS[i] );
            }
        }

        if ( pAddTS->tclasProcPresent )
        {
            if ( pAddTS->lleTspecPresent )
            {
                AddTSReq.TCLASSPROC.processing = pAddTS->tclasProc;
                AddTSReq.TCLASSPROC.present    = 1;
            }
            else
            {
                AddTSReq.WMMTCLASPROC.version    = 1;
                AddTSReq.WMMTCLASPROC.processing = pAddTS->tclasProc;
                AddTSReq.WMMTCLASPROC.present    = 1;
            }
        }

        nStatus = dot11fGetPackedAddTSRequestSize( pMac, &AddTSReq, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                                   "or an Add TS Request (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSRequest );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calculating"
                                   "the packed size for an Add TS Request"
                                   " (0x%08x).\n"), nStatus );
        }
    }
    else
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&WMMAddTSReq, sizeof( WMMAddTSReq ) );

        WMMAddTSReq.Action.action     = SIR_MAC_QOS_ADD_TS_REQ;
        WMMAddTSReq.DialogToken.token = pAddTS->dialogToken;
        WMMAddTSReq.Category.category = SIR_MAC_ACTION_WME;

        // WMM spec 2.2.10 - status code is only filled in for ADDTS response
        WMMAddTSReq.StatusCode.statusCode = 0;

        PopulateDot11fWMMTSPEC( &pAddTS->tspec, &WMMAddTSReq.WMMTSPEC );
#ifdef FEATURE_WLAN_CCX
        limGetPhyMode(pMac, &phyMode, psessionEntry);

        if( phyMode == WNI_CFG_PHY_MODE_11G || phyMode == WNI_CFG_PHY_MODE_11A)
        {
            pAddTS->tsrsIE.rates[0] = TSRS_11AG_RATE_6MBPS;
        }
        else 
        {
            pAddTS->tsrsIE.rates[0] = TSRS_11B_RATE_5_5MBPS;
        }
        PopulateDot11TSRSIE(pMac,&pAddTS->tsrsIE, &WMMAddTSReq.CCXTrafStrmRateSet,sizeof(tANI_U8));
#endif
        // fillWmeTspecIE

        nStatus = dot11fGetPackedWMMAddTSRequestSize( pMac, &WMMAddTSReq, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                                   "or a WMM Add TS Request (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSRequest );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calculating"
                                   "the packed size for a WMM Add TS Requ"
                                   "est (0x%08x).\n"), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Request.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    #if 0
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Request.\n") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }
    #endif //TO SUPPORT BT-AMP
    
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    // That done, pack the struct:
    if ( ! pAddTS->wmeTspecPresent )
    {
        nStatus = dot11fPackAddTSRequest( pMac, &AddTSReq,
                                          pFrame + sizeof(tSirMacMgmtHdr),
                                          nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack an Add TS Request "
                                   "(0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "an Add TS Request (0x%08x).\n") );
        }
    }
    else
    {
        nStatus = dot11fPackWMMAddTSRequest( pMac, &WMMAddTSReq,
                                             pFrame + sizeof(tSirMacMgmtHdr),
                                             nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Add TS Reque"
                                   "st (0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;            // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "a WMM Add TS Request (0x%08x).\n") );
        }
    }

    PELOG3(limLog( pMac, LOG3, FL("Sending an Add TS Request frame to ") );
    limPrintMacAddr( pMac, peerMacAddr, LOG3 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue Addts Response frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL( "*** Could not send an Add TS Request"
                                " (%X) ***\n" ), halstatus );
        //Pkt will be freed up by the callback
    }

} // End limSendAddtsReqActionFrame.

/* Added ANI_PRODUCT_TYPE_CLIENT for BT-AMP Support */
#ifdef ANI_PRODUCT_TYPE_AP

void
limSendAssocRspMgmtFrame(tpAniSirGlobal pMac,
                         tANI_U16       statusCode,
                         tANI_U16       aid,
                         tSirMacAddr    peerMacAddr,
                         tANI_U8        subType,
                         tpDphHashNode  pSta,
                         tpPESession psessionEntry)
{
    tDot11fAssocResponse frm;
    tANI_U8             *pFrame, *macAddr;
    tpSirMacMgmtHdr      pMacHdr;
    tSirRetStatus        nSirStatus;
    tANI_U8              lleMode = 0, fAddTS, edcaInclude = 0;
    tHalBitVal           qosMode, wmeMode;
    tANI_U32             nPayload, nBytes, nStatus, cfgLen;
    void                *pPacket;
    eHalStatus           halstatus;
    tUpdateBeaconParams beaconParams;
    tANI_U32             wpsApEnable=0, tmp;
    tANI_U8              txFlag = 0;

    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    limGetQosMode(pMac, &qosMode);
    limGetWmeMode(pMac, &wmeMode);

    // An Add TS IE is added only if the AP supports it and the requesting
    // STA sent a traffic spec.
    fAddTS = ( qosMode && pSta && pSta->qos.addtsPresent ) ? 1 : 0;

    PopulateDot11fCapabilities( pMac, &frm.Capabilities, psessionEntry);

    frm.Status.status = statusCode;

    frm.AID.associd = aid | LIM_AID_MASK;

    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &frm.SuppRates );

    if (wlan_cfgGetInt(pMac, (tANI_U16) WNI_CFG_WPS_ENABLE, &tmp) != eSIR_SUCCESS)
        limLog(pMac, LOGP,"Failed to cfg get id %d\n", WNI_CFG_WPS_ENABLE );
    
    wpsApEnable = tmp & WNI_CFG_WPS_ENABLE_AP;

    if (wpsApEnable)
    {
        PopulateDot11fWscInAssocRes(pMac, &frm.WscAssocRes);
    }

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &frm.ExtSuppRates, psessionEntry );

    if ( NULL != pSta )
    {
        if ( eHAL_SET == qosMode )
        {
            if ( pSta->lleEnabled )
            {
                lleMode = 1;
                if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) )
                {
                    PopulateDot11fEDCAParamSet( pMac, &frm.EDCAParamSet, psessionEntry);

//                     FramesToDo:...
//                     if ( fAddTS )
//                     {
//                         tANI_U8 *pAf = pBody;
//                         *pAf++ = SIR_MAC_QOS_ACTION_EID;
//                         tANI_U32 tlen;
//                         status = sirAddtsRspFill(pMac, pAf, statusCode, &pSta->qos.addts, NULL,
//                                                  &tlen, bufLen - frameLen);
//                     } // End if on Add TS.
                }
            } // End if on .11e enabled in 'pSta'.
        } // End if on QOS Mode on.

        if ( ( ! lleMode ) && ( eHAL_SET == wmeMode ) && pSta->wmeEnabled )
        {
            if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {
                PopulateDot11fWMMParams( pMac, &frm.WMMParams );

                if ( pSta->wsmEnabled )
                {
                    PopulateDot11fWMMCaps(&frm.WMMCaps );
                }
            }
        }

        if ( pSta->aniPeer )
        {
            if ( ( lleMode && PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) ||
                 ( pSta->wmeEnabled && PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {
                edcaInclude = 1;
            }

        } // End if on Airgo peer.

        if ( pSta->mlmStaContext.htCapability  && 
             pMac->lim.htCapability )
        {
            PopulateDot11fHTCaps( pMac, &frm.HTCaps );
            PopulateDot11fHTInfo( pMac, &frm.HTInfo );
        }
    } // End if on non-NULL 'pSta'.


    if(pMac->lim.gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideApProtection(pMac, peerMacAddr, &beaconParams);
    limUpdateShortPreamble(pMac, peerMacAddr, &beaconParams);
    limUpdateShortSlotTime(pMac, peerMacAddr, &beaconParams);

    //Send message to HAL about beacon parameter change.
    if(beaconParams.paramChangeBitmap)
    {
        schSetFixedBeaconFields(pMac,psessionEntry);
        limSendBeaconParams(pMac, &beaconParams, psessionEntry );
    }

    // Allocate a buffer for this frame:
    nStatus = dot11fGetPackedAssocResponseSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to calculate the packed size f"
                               "or an Association Response (0x%08x).\n"),
                nStatus );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for an Association Re"
                               "sponse (0x%08x).\n"), nStatus );
    }

    nBytes = sizeof( tSirMacMgmtHdr ) + nPayload;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGP, FL("Call to bufAlloc failed for RE/ASSOC RSP.\n"));
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac,
                                pFrame,
                                SIR_MAC_MGMT_FRAME,
                                ( LIM_ASSOC == subType ) ?
                                    SIR_MAC_MGMT_ASSOC_RSP :
                                    SIR_MAC_MGMT_REASSOC_RSP,
                                    peerMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Association Response (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Association Response.\n") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
  
    nStatus = dot11fPackAssocResponse( pMac, &frm,
                                       pFrame + sizeof( tSirMacMgmtHdr ),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack an Association Response (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing an "
                               "Association Response (0x%08x).\n") );
    }

    macAddr = pMacHdr->da;

    if (subType == LIM_ASSOC)
        limLog(pMac, LOG1,
               FL("*** Sending Assoc Resp status %d aid %d to "),
               statusCode, aid);
    else
        limLog(pMac, LOG1,
               FL("*** Sending ReAssoc Resp status %d aid %d to "),
               statusCode, aid);
    limPrintMacAddr(pMac, pMacHdr->da, LOG1);

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    /// Queue Association Response frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("*** Could not Send Re/AssocRsp, retCode=%X ***\n"),
               nSirStatus);

        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                  (void *) pFrame, (void *) pPacket );
    }

    // update the ANI peer station count
    //FIXME_PROTECTION : take care of different type of station
    // counter inside this function.
    limUtilCountStaAdd(pMac, pSta, psessionEntry);

} // End limSendAssocRspMgmtFrame.


#endif  // ANI_PRODUCT_TYPE_AP


void
limSendAssocRspMgmtFrame(tpAniSirGlobal pMac,
                         tANI_U16       statusCode,
                         tANI_U16       aid,
                         tSirMacAddr    peerMacAddr,
                         tANI_U8        subType,
                         tpDphHashNode  pSta,tpPESession psessionEntry)
{
    static tDot11fAssocResponse frm;
    tANI_U8             *pFrame, *macAddr;
    tpSirMacMgmtHdr      pMacHdr;
    tSirRetStatus        nSirStatus;
    tANI_U8              lleMode = 0, fAddTS, edcaInclude = 0;
    tHalBitVal           qosMode, wmeMode;
    tANI_U32             nPayload, nBytes, nStatus;
    void                *pPacket;
    eHalStatus           halstatus;
    tUpdateBeaconParams beaconParams;
    tANI_U8              txFlag = 0;
    tANI_U32             addnIEPresent = false;
    tANI_U32             addnIELen=0;
    tANI_U8              addIE[WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN];
    tpSirAssocReq        pAssocReq = NULL; 

    if(NULL == psessionEntry)
    {
        return;
    }

    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    limGetQosMode(psessionEntry, &qosMode);
    limGetWmeMode(psessionEntry, &wmeMode);

    // An Add TS IE is added only if the AP supports it and the requesting
    // STA sent a traffic spec.
    fAddTS = ( qosMode && pSta && pSta->qos.addtsPresent ) ? 1 : 0;

    PopulateDot11fCapabilities( pMac, &frm.Capabilities, psessionEntry );

    frm.Status.status = statusCode;

    frm.AID.associd = aid | LIM_AID_MASK;

    if ( NULL == pSta )
    {
       PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &frm.SuppRates,psessionEntry);
       PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL, &frm.ExtSuppRates, psessionEntry );
    }
    else
    {
       PopulateDot11fAssocRspRates( pMac, &frm.SuppRates, &frm.ExtSuppRates,
                      pSta->supportedRates.llbRates, pSta->supportedRates.llaRates );
    }

#ifdef WLAN_SOFTAP_FEATURE
    if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if( pSta != NULL && eSIR_SUCCESS == statusCode )
        {
            pAssocReq = 
                (tpSirAssocReq) psessionEntry->parsedAssocReq[pSta->assocId];
#ifdef WLAN_FEATURE_P2P
            /* populate P2P IE in AssocRsp when assocReq from the peer includes P2P IE */
            if( pAssocReq != NULL && pAssocReq->addIEPresent ) {
                PopulateDot11AssocResP2PIE(pMac, &frm.P2PAssocRes, pAssocReq);
            }
#endif
        }
    }
#endif

    if ( NULL != pSta )
    {
        if ( eHAL_SET == qosMode )
        {
            if ( pSta->lleEnabled )
            {
                lleMode = 1;
                if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) )
                {
                    PopulateDot11fEDCAParamSet( pMac, &frm.EDCAParamSet, psessionEntry);

//                     FramesToDo:...
//                     if ( fAddTS )
//                     {
//                         tANI_U8 *pAf = pBody;
//                         *pAf++ = SIR_MAC_QOS_ACTION_EID;
//                         tANI_U32 tlen;
//                         status = sirAddtsRspFill(pMac, pAf, statusCode, &pSta->qos.addts, NULL,
//                                                  &tlen, bufLen - frameLen);
//                     } // End if on Add TS.
                }
            } // End if on .11e enabled in 'pSta'.
        } // End if on QOS Mode on.

        if ( ( ! lleMode ) && ( eHAL_SET == wmeMode ) && pSta->wmeEnabled )
        {
            if ( ( ! pSta->aniPeer ) || ( ! PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {

#ifdef WLAN_SOFTAP_FEATURE
                PopulateDot11fWMMParams( pMac, &frm.WMMParams, psessionEntry);
#else
                PopulateDot11fWMMParams( pMac, &frm.WMMParams );
#endif

                if ( pSta->wsmEnabled )
                {
                    PopulateDot11fWMMCaps(&frm.WMMCaps );
                }
            }
        }

        if ( pSta->aniPeer )
        {
            if ( ( lleMode && PROP_CAPABILITY_GET( 11EQOS, pSta->propCapability ) ) ||
                 ( pSta->wmeEnabled && PROP_CAPABILITY_GET( WME, pSta->propCapability ) ) )
            {
                edcaInclude = 1;
            }

        } // End if on Airgo peer.

        if ( pSta->mlmStaContext.htCapability  && 
             psessionEntry->htCapabality )
        {
            PopulateDot11fHTCaps( pMac, &frm.HTCaps );
#ifdef WLAN_SOFTAP_FEATURE
            PopulateDot11fHTInfo( pMac, &frm.HTInfo, psessionEntry );
#else
            PopulateDot11fHTInfo( pMac, &frm.HTInfo );
#endif
        }
    } // End if on non-NULL 'pSta'.


   palZeroMemory( pMac->hHdd, ( tANI_U8* )&beaconParams, sizeof( tUpdateBeaconParams) );

#ifdef WLAN_SOFTAP_FEATURE
    if( psessionEntry->limSystemRole == eLIM_AP_ROLE ){
        if(psessionEntry->gLimProtectionControl != WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
        limDecideApProtection(pMac, peerMacAddr, &beaconParams,psessionEntry);
    }
#endif

    limUpdateShortPreamble(pMac, peerMacAddr, &beaconParams, psessionEntry);
    limUpdateShortSlotTime(pMac, peerMacAddr, &beaconParams, psessionEntry);

    beaconParams.bssIdx = psessionEntry->bssIdx;

    //Send message to HAL about beacon parameter change.
    if(beaconParams.paramChangeBitmap)
    {
        schSetFixedBeaconFields(pMac,psessionEntry);
        limSendBeaconParams(pMac, &beaconParams, psessionEntry );
    }

    // Allocate a buffer for this frame:
    nStatus = dot11fGetPackedAssocResponseSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to calculate the packed size f"
                               "or an Association Response (0x%08x).\n"),
                nStatus );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for an Association Re"
                               "sponse (0x%08x).\n"), nStatus );
    }

    nBytes = sizeof( tSirMacMgmtHdr ) + nPayload;

    if ( pAssocReq != NULL ) 
    {
        if (wlan_cfgGetInt(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_FLAG, 
                    &addnIEPresent) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_ASSOC_RSP_ADDNIE_FLAG"));
            return;
        }

        if (addnIEPresent)
        {
            //Assoc rsp IE available
            if (wlan_cfgGetStrLen(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_DATA,
                        &addnIELen) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP, FL("Unable to get WNI_CFG_ASSOC_RSP_ADDNIE_DATA length"));
                return;
            }

            if (addnIELen <= WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN && addnIELen &&
                    (nBytes + addnIELen) <= SIR_MAX_PACKET_SIZE)
            {
                if (wlan_cfgGetStr(pMac, WNI_CFG_ASSOC_RSP_ADDNIE_DATA,
                            &addIE[0], &addnIELen) == eSIR_SUCCESS)
                {
                    nBytes = nBytes + addnIELen;
                }
            }
        }
    }

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGP, FL("Call to bufAlloc failed for RE/ASSOC RSP.\n"));
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac,
                                pFrame,
                                SIR_MAC_MGMT_FRAME,
                                ( LIM_ASSOC == subType ) ?
                                    SIR_MAC_MGMT_ASSOC_RSP :
                                    SIR_MAC_MGMT_REASSOC_RSP,
                                peerMacAddr,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Association Response (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    #if 0
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Association Response.\n") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    nStatus = dot11fPackAssocResponse( pMac, &frm,
                                       pFrame + sizeof( tSirMacMgmtHdr ),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack an Association Response (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing an "
                               "Association Response (0x%08x).\n") );
    }

    macAddr = pMacHdr->da;

    if (subType == LIM_ASSOC)
    {
        PELOG1(limLog(pMac, LOG1,
               FL("*** Sending Assoc Resp status %d aid %d to "),
               statusCode, aid);)
    }    
    else{
        PELOG1(limLog(pMac, LOG1,
               FL("*** Sending ReAssoc Resp status %d aid %d to "),
               statusCode, aid);)
    }
    PELOG1(limPrintMacAddr(pMac, pMacHdr->da, LOG1);)

    if ( addnIEPresent )
    {
        if (palCopyMemory ( pMac->hHdd, pFrame+sizeof(tSirMacMgmtHdr)+nPayload,
                           &addIE[0], addnIELen ) != eHAL_STATUS_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Additional Assoc IEs request failed while Appending: %x\n"),halstatus);
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                       ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    /// Queue Association Response frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("*** Could not Send Re/AssocRsp, retCode=%X ***\n"),
               nSirStatus);

        //Pkt will be freed up by the callback
    }

    // update the ANI peer station count
    //FIXME_PROTECTION : take care of different type of station
    // counter inside this function.
    limUtilCountStaAdd(pMac, pSta, psessionEntry);

} // End limSendAssocRspMgmtFrame.


 
void
limSendAddtsRspActionFrame(tpAniSirGlobal     pMac,
                           tSirMacAddr        peer,
                           tANI_U16           nStatusCode,
                           tSirAddtsReqInfo  *pAddTS,
                           tSirMacScheduleIE *pSchedule,
                           tpPESession        psessionEntry)
{
    tANI_U8                *pFrame;
    tpSirMacMgmtHdr         pMacHdr;
    tDot11fAddTSResponse    AddTSRsp;
    tDot11fWMMAddTSResponse WMMAddTSRsp;
    tSirRetStatus           nSirStatus;
    tANI_U32                i, nBytes, nPayload, nStatus;
    void                   *pPacket;
    eHalStatus              halstatus;
    tANI_U8                 txFlag = 0;

    if(NULL == psessionEntry)
    {
              return;
    }

    if ( ! pAddTS->wmeTspecPresent )
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&AddTSRsp, sizeof( AddTSRsp ) );

        AddTSRsp.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        AddTSRsp.Action.action     = SIR_MAC_QOS_ADD_TS_RSP;
        AddTSRsp.DialogToken.token = pAddTS->dialogToken;
        AddTSRsp.Status.status     = nStatusCode;

        // The TsDelay information element is only filled in for a specific
        // status code:
        if ( eSIR_MAC_TS_NOT_CREATED_STATUS == nStatusCode )
        {
            if ( pAddTS->wsmTspecPresent )
            {
                AddTSRsp.WMMTSDelay.version = 1;
                AddTSRsp.WMMTSDelay.delay   = 10;
                AddTSRsp.WMMTSDelay.present = 1;
            }
            else
            {
                AddTSRsp.TSDelay.delay   = 10;
                AddTSRsp.TSDelay.present = 1;
            }
        }

        if ( pAddTS->wsmTspecPresent )
        {
            PopulateDot11fWMMTSPEC( &pAddTS->tspec, &AddTSRsp.WMMTSPEC );
        }
        else
        {
            PopulateDot11fTSPEC( &pAddTS->tspec, &AddTSRsp.TSPEC );
        }

        if ( pAddTS->wsmTspecPresent )
        {
            AddTSRsp.num_WMMTCLAS = 0;
            AddTSRsp.num_TCLAS = pAddTS->numTclas;
            for ( i = 0; i < AddTSRsp.num_TCLAS; ++i)
            {
                PopulateDot11fTCLAS( pMac, &pAddTS->tclasInfo[i],
                                     &AddTSRsp.TCLAS[i] );
            }
        }
        else
        {
            AddTSRsp.num_TCLAS = 0;
            AddTSRsp.num_WMMTCLAS = pAddTS->numTclas;
            for ( i = 0; i < AddTSRsp.num_WMMTCLAS; ++i)
            {
                PopulateDot11fWMMTCLAS( pMac, &pAddTS->tclasInfo[i],
                                        &AddTSRsp.WMMTCLAS[i] );
            }
        }

        if ( pAddTS->tclasProcPresent )
        {
            if ( pAddTS->wsmTspecPresent )
            {
                AddTSRsp.WMMTCLASPROC.version    = 1;
                AddTSRsp.WMMTCLASPROC.processing = pAddTS->tclasProc;
                AddTSRsp.WMMTCLASPROC.present    = 1;
            }
            else
            {
                AddTSRsp.TCLASSPROC.processing = pAddTS->tclasProc;
                AddTSRsp.TCLASSPROC.present    = 1;
            }
        }

        // schedule element is included only if requested in the tspec and we are
        // using hcca (or both edca and hcca)
        // 11e-D8.0 is inconsistent on whether the schedule element is included
        // based on tspec schedule bit or not. Sec 7.4.2.2. says one thing but
        // pg 46, line 17-18 says something else. So just include it and let the
        // sta figure it out
        if ((pSchedule != NULL) &&
            ((pAddTS->tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA) ||
             (pAddTS->tspec.tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_BOTH)))
        {
            if ( pAddTS->wsmTspecPresent )
            {
                PopulateDot11fWMMSchedule( pSchedule, &AddTSRsp.WMMSchedule );
            }
            else
            {
                PopulateDot11fSchedule( pSchedule, &AddTSRsp.Schedule );
            }
        }

        nStatus = dot11fGetPackedAddTSResponseSize( pMac, &AddTSRsp, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for an Add TS Response (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fAddTSResponse );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "tingthe packed size for an Add TS"
                                   " Response (0x%08x).\n"), nStatus );
        }
    }
    else
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&WMMAddTSRsp, sizeof( WMMAddTSRsp ) );

        WMMAddTSRsp.Category.category = SIR_MAC_ACTION_WME;
        WMMAddTSRsp.Action.action     = SIR_MAC_QOS_ADD_TS_RSP;
        WMMAddTSRsp.DialogToken.token = pAddTS->dialogToken;
        WMMAddTSRsp.StatusCode.statusCode = (tANI_U8)nStatusCode;

        PopulateDot11fWMMTSPEC( &pAddTS->tspec, &WMMAddTSRsp.WMMTSPEC );

        nStatus = dot11fGetPackedWMMAddTSResponseSize( pMac, &WMMAddTSRsp, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a WMM Add TS Response (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fWMMAddTSResponse );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "tingthe packed size for a WMM Add"
                                   "TS Response (0x%08x).\n"), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Response.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Response (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

     
    #if 0
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Response.\n") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    // That done, pack the struct:
    if ( ! pAddTS->wmeTspecPresent )
    {
        nStatus = dot11fPackAddTSResponse( pMac, &AddTSRsp,
                                           pFrame + sizeof( tSirMacMgmtHdr ),
                                           nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack an Add TS Response "
                                   "(0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "an Add TS Response (0x%08x).\n") );
        }
    }
    else
    {
        nStatus = dot11fPackWMMAddTSResponse( pMac, &WMMAddTSRsp,
                                              pFrame + sizeof( tSirMacMgmtHdr ),
                                              nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Add TS Response "
                                   "(0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "a WMM Add TS Response (0x%08x).\n") );
        }
    }

    PELOG1(limLog( pMac, LOG1, FL("Sending an Add TS Response (status %d) to "),
            nStatusCode );
    limPrintMacAddr( pMac, pMacHdr->da, LOG1 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue the frame in high priority WQ:
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Add TS Response (%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
    }

} // End limSendAddtsRspActionFrame.

void
limSendDeltsReqActionFrame(tpAniSirGlobal  pMac,
                           tSirMacAddr  peer,
                           tANI_U8  wmmTspecPresent,
                           tSirMacTSInfo  *pTsinfo,
                           tSirMacTspecIE  *pTspecIe,
                           tpPESession psessionEntry)
{
    tANI_U8         *pFrame;
    tpSirMacMgmtHdr  pMacHdr;
    tDot11fDelTS     DelTS;
    tDot11fWMMDelTS  WMMDelTS;
    tSirRetStatus    nSirStatus;
    tANI_U32         nBytes, nPayload, nStatus;
    void            *pPacket;
    eHalStatus       halstatus;
    tANI_U8          txFlag = 0;

    if(NULL == psessionEntry)
    {
              return;
    }

    if ( ! wmmTspecPresent )
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&DelTS, sizeof( DelTS ) );

        DelTS.Category.category = SIR_MAC_ACTION_QOS_MGMT;
        DelTS.Action.action     = SIR_MAC_QOS_DEL_TS_REQ;
        PopulateDot11fTSInfo( pTsinfo, &DelTS.TSInfo );

        nStatus = dot11fGetPackedDelTSSize( pMac, &DelTS, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a Del TS (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fDelTS );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for a Del TS"
                                   " (0x%08x).\n"), nStatus );
        }
    }
    else
    {
        palZeroMemory( pMac->hHdd, ( tANI_U8* )&WMMDelTS, sizeof( WMMDelTS ) );

        WMMDelTS.Category.category = SIR_MAC_ACTION_WME;
        WMMDelTS.Action.action     = SIR_MAC_QOS_DEL_TS_REQ;
        WMMDelTS.DialogToken.token = 0;
        WMMDelTS.StatusCode.statusCode = 0;
        PopulateDot11fWMMTSPEC( pTspecIe, &WMMDelTS.WMMTSPEC );
        nStatus = dot11fGetPackedWMMDelTSSize( pMac, &WMMDelTS, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGP, FL("Failed to calculate the packed si"
                                   "ze for a WMM Del TS (0x%08x).\n"),
                    nStatus );
            // We'll fall back on the worst case scenario:
            nPayload = sizeof( tDot11fDelTS );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while calcula"
                                   "ting the packed size for a WMM De"
                                   "l TS (0x%08x).\n"), nStatus );
        }
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an Ad"
                               "d TS Response.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer,
                                psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Add TS Response (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    #if 0

    cfgLen = SIR_MAC_ADDR_LENGTH;
    if ( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID,
                                    ( tANI_U8* )pMacHdr->bssId, &cfgLen ) )
    {
        limLog( pMac, LOGP, FL("Failed to retrieve WNI_CFG_BSSID whil"
                               "e sending an Add TS Response.\n") );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId, psessionEntry->bssId);
    
    // That done, pack the struct:
    if ( !wmmTspecPresent )
    {
        nStatus = dot11fPackDelTS( pMac, &DelTS,
                                   pFrame + sizeof( tSirMacMgmtHdr ),
                                   nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a Del TS frame (0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "a Del TS frame (0x%08x).\n") );
        }
    }
    else
    {
        nStatus = dot11fPackWMMDelTS( pMac, &WMMDelTS,
                                      pFrame + sizeof( tSirMacMgmtHdr ),
                                      nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
            limLog( pMac, LOGE, FL("Failed to pack a WMM Del TS frame (0x%08x).\n"),
                    nStatus );
            palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
            return;             // allocated!
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
            limLog( pMac, LOGW, FL("There were warnings while packing"
                                   "a WMM Del TS frame (0x%08x).\n") );
        }
    }

    PELOG1(limLog(pMac, LOG1, FL("Sending DELTS REQ (size %d) to "), nBytes);
    limPrintMacAddr(pMac, pMacHdr->da, LOG1);)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Del TS (%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
    }

} // End limSendDeltsReqActionFrame.

void
limSendAssocReqMgmtFrame(tpAniSirGlobal   pMac,
                         tLimMlmAssocReq *pMlmAssocReq,
                         tpPESession psessionEntry)
{
    tDot11fAssocRequest frm;
    tANI_U16            caps;
    tANI_U8            *pFrame;
    tSirRetStatus       nSirStatus;
    tLimMlmAssocCnf     mlmAssocCnf;
    tANI_U32            nBytes, nPayload, nStatus;
    tANI_U8             fQosEnabled, fWmeEnabled, fWsmEnabled;
    void               *pPacket;
    eHalStatus          halstatus;
    tANI_U16            nAddIELen; 
    tANI_U8             *pAddIE;
    tANI_U8             *wpsIe = NULL;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8             PowerCapsPopulated = FALSE;
#endif
    tANI_U8             txFlag = 0;

    if(NULL == psessionEntry)
    {
        return;
    }

    if(NULL == psessionEntry->pLimJoinReq)
    {
        return;
    }
    
    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimJoinReq)
    {
        return;
    }
    nAddIELen = psessionEntry->pLimJoinReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimJoinReq->addIEAssoc.addIEdata;

    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    caps = pMlmAssocReq->capabilityInfo;
    if ( PROP_CAPABILITY_GET( 11EQOS, psessionEntry->limCurrentBssPropCap ) )
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
       According to WAPI standard:
       7.3.1.4 Capability Information field
       In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
       Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
       Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&frm.Capabilities );

    frm.ListenInterval.interval = pMlmAssocReq->listenInterval;
    PopulateDot11fSSID2( pMac, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled) &&
        SIR_MAC_GET_QOS( psessionEntry->limCurrentBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limCurrentBssQosCaps );

    // We prefer .11e asociations:
    if ( fQosEnabled ) fWmeEnabled = false;

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limCurrentBssQosCaps );

    if ( psessionEntry->lim11hEnable  &&
            psessionEntry->pLimJoinReq->spectrumMgtIndicator == eSIR_TRUE ) 
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;

        PopulateDot11fPowerCaps( pMac, &frm.PowerCaps, LIM_ASSOC,psessionEntry);
#endif
        PopulateDot11fSuppChannels( pMac, &frm.SuppChannels, LIM_ASSOC,psessionEntry);

    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &frm.PowerCaps, LIM_ASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
            ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limCurrentBssPropCap)))
        PopulateDot11fQOSCapsStation( pMac, &frm.QOSCapsStation );

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &frm.RRMEnabledCap, psessionEntry );       
    }
#endif
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if( nAddIELen && pAddIE )
    {
        wpsIe = limGetWscIEPtr (pMac, pAddIE, nAddIELen);
    }
    if ( NULL == wpsIe )
    {
        PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &frm.RSNOpaque );
        PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &frm.WPAOpaque );
#if defined(FEATURE_WLAN_WAPI)
        PopulateDot11fWAPIOpaque( pMac, &( psessionEntry->pLimJoinReq->rsnIE ),
                &frm.WAPIOpaque );
#endif // defined(FEATURE_WLAN_WAPI)
    }

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limCurrentBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &frm.WMMInfoStation );
        }

        if ( fWsmEnabled &&
                ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limCurrentBssPropCap )))
        {
            PopulateDot11fWMMCaps( &frm.WMMCaps );
        }
    }

    //Populate HT IEs, when operating in 11n or Taurus modes AND
    //when AP is also operating in 11n mode.
    if ( psessionEntry->htCapabality &&
            pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, &frm.HTCaps );
#ifdef DISABLE_GF_FOR_INTEROP

        /*
         * To resolve the interop problem with Broadcom AP, 
         * where TQ STA could not pass traffic with GF enabled,
         * TQ STA will do Greenfield only with TQ AP, for 
         * everybody else it will be turned off.
         */

        if( (psessionEntry->pLimJoinReq != NULL) && (!psessionEntry->pLimJoinReq->bssDescription.aniIndicator))
        {
                limLog( pMac, LOG1, FL("Sending Assoc Req to Non-TQ AP, Turning off Greenfield"));
            frm.HTCaps.greenField = WNI_CFG_GREENFIELD_CAPABILITY_DISABLE;
        }
#endif

    }

#if defined WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->pLimJoinReq->is11Rconnection)
    {
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
        limLog( pMac, LOGE, FL("mdie = %02x %02x %02x\n"), 
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[0],
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[1],
                (unsigned int)psessionEntry->pLimJoinReq->bssDescription.mdie[2]);
#endif
        PopulateMDIE( pMac, &frm.MobilityDomain, psessionEntry->pLimJoinReq->bssDescription.mdie); 
    }
    else 
    {
        // No 11r IEs dont send any MDIE
        limLog( pMac, LOGE, FL("mdie not present\n")); 
    }
#endif

#ifdef FEATURE_WLAN_CCX
    // For CCX Associations fill the CCX IEs
    if (psessionEntry->isCCXconnection)
    {
        PopulateDot11fCCXRadMgmtCap(&frm.CCXRadMgmtCap);
        PopulateDot11fCCXVersion(&frm.CCXVersion);
    }
#endif

    nStatus = dot11fGetPackedAssocRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                    "or an Association Request (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                    "the packed size for an Association Re "
                    "quest(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
            ( tANI_U16 )nBytes, ( void** ) &pFrame,
            ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for an As"
                    "sociation Request.\n"), nBytes );

        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));


        /* Update PE session id*/
        mlmAssocCnf.sessionId = psessionEntry->peSessionId;

        mlmAssocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;

        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                ( void* ) pFrame, ( void* ) pPacket );

        limPostSmeMessage( pMac, LIM_MLM_ASSOC_CNF,
                ( tANI_U32* ) &mlmAssocCnf);

        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
            SIR_MAC_MGMT_ASSOC_REQ, psessionEntry->bssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                    "tor for an Association Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;
    }


    // That done, pack the Probe Request:
    nStatus = dot11fPackAssocRequest( pMac, &frm, pFrame +
            sizeof(tSirMacMgmtHdr),
            nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Probe Response (0x%0"
                    "8x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                ( void* ) pFrame, ( void* ) pPacket );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a P"
                    "robe Response (0x%08x).\n") );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending Association Request length %d"
                    "to \n"),
                nBytes );)
        //   limPrintMacAddr( pMac, bssid, LOG1 );

        if( psessionEntry->assocReq != NULL )
        {
            palFreeMemory(pMac->hHdd, psessionEntry->assocReq);
            psessionEntry->assocReq = NULL;
        }

    if( nAddIELen )
    {
        palCopyMemory( pMac->hHdd, pFrame + sizeof(tSirMacMgmtHdr) + nPayload, 
                pAddIE, 
                nAddIELen );
        nPayload += nAddIELen;
    }

    if( (palAllocateMemory(pMac->hHdd, (void**)&psessionEntry->assocReq,
                           nPayload)) != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        palCopyMemory(pMac->hHdd, psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (sizeof(tSirMacMgmtHdr) + nPayload),
            HAL_TXRX_FRM_802_11_MGMT,
            ANI_TXDIR_TODS,
            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Association Request (%X)!\n"),
                halstatus );
        //Pkt will be freed up by the callback
        return;
    }

    // Free up buffer allocated for mlmAssocReq
    palFreeMemory( pMac->hHdd, ( tANI_U8* ) pMlmAssocReq );

} // End limSendAssocReqMgmtFrame


#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
/*------------------------------------------------------------------------------------
 *
 * Send Reassoc Req with FTIEs.
 *
 *-----------------------------------------------------------------------------------
 */
void
limSendReassocReqWithFTIEsMgmtFrame(tpAniSirGlobal     pMac,
                           tLimMlmReassocReq *pMlmReassocReq,tpPESession psessionEntry)
{
    static tDot11fReAssocRequest frm;
    tANI_U16              caps;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tANI_U32              nBytes, nPayload, nStatus;
    tANI_U8               fQosEnabled, fWmeEnabled, fWsmEnabled;
    void                 *pPacket;
    eHalStatus            halstatus;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8               PowerCapsPopulated = FALSE;
#endif
    tANI_U16              ft_ies_length = 0;
    tANI_U8               *pBody;
    tANI_U16              nAddIELen; 
    tANI_U8               *pAddIE;
#if defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
    tANI_U8               *wpsIe = NULL;
#endif
    tANI_U8               txFlag = 0;

    if (NULL == psessionEntry)
    {
        return;
    }

#if defined WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->is11Rconnection)
    {
        if (pMac->ft.ftSmeContext.reassoc_ft_ies_length == 0)
        {
            return;
        }
    }
#endif

    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimReAssocReq)
    {
        return;
    }
    nAddIELen = psessionEntry->pLimReAssocReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimReAssocReq->addIEAssoc.addIEdata;
    limLog( pMac, LOGE, FL("limSendReassocReqWithFTIEsMgmtFrame received in " 
                           "state (%d).\n"), psessionEntry->limMlmState);

    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    caps = pMlmReassocReq->capabilityInfo;
    if (PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap))
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
       According to WAPI standard:
       7.3.1.4 Capability Information field
       In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
       Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
       Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&frm.Capabilities );

    frm.ListenInterval.interval = pMlmReassocReq->listenInterval;

    // Get the old bssid of the older AP.
    palCopyMemory( pMac->hHdd, ( tANI_U8* )frm.CurrentAPAddress.mac,
            pMac->ft.ftPEContext.pFTPreAuthReq->currbssId, 6); 

    PopulateDot11fSSID2( pMac, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled) &&
        SIR_MAC_GET_QOS( psessionEntry->limReassocBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limReassocBssQosCaps );

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limReassocBssQosCaps );

    if ( psessionEntry->lim11hEnable  &&
            psessionEntry->pLimReAssocReq->spectrumMgtIndicator == eSIR_TRUE )
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;

        PopulateDot11fPowerCaps( pMac, &frm.PowerCaps, LIM_REASSOC,psessionEntry);
        PopulateDot11fSuppChannels( pMac, &frm.SuppChannels, LIM_REASSOC,psessionEntry);
#endif
    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &frm.PowerCaps, LIM_REASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
            ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap ) ))
    {
        PopulateDot11fQOSCapsStation( pMac, &frm.QOSCapsStation );
    }

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
            &frm.ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
            SIR_MAC_GET_RRM( psessionEntry->limReassocBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &frm.RRMEnabledCap, psessionEntry );       
    }
#endif

    // Ideally this should be enabled for 11r also. But 11r does
    // not follow the usual norm of using the Opaque object
    // for rsnie and fties. Instead we just add
    // the rsnie and fties at the end of the pack routine for 11r.
    // This should ideally! be fixed.
#if defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
    //
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if (!psessionEntry->is11Rconnection)
    {
        if( nAddIELen && pAddIE )
        {
            wpsIe = limGetWscIEPtr(pMac, pAddIE, nAddIELen);
        }
        if ( NULL == wpsIe )
        {
            PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                    &frm.RSNOpaque );
            PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                    &frm.WPAOpaque );
        }

#ifdef FEATURE_WLAN_CCX
        if(psessionEntry->pLimReAssocReq->cckmIE.length)
        {
            PopulateDot11fCCXCckmOpaque( pMac, &( psessionEntry->pLimReAssocReq->cckmIE ),
                    &frm.CCXCckmOpaque );
        }
#endif //FEATURE_WLAN_CCX
    }

#ifdef FEATURE_WLAN_CCX
    // For CCX Associations fill the CCX IEs
    if (psessionEntry->isCCXconnection)
    {
        PopulateDot11fCCXRadMgmtCap(&frm.CCXRadMgmtCap);
        PopulateDot11fCCXVersion(&frm.CCXVersion);
    }
#endif //FEATURE_WLAN_CCX 
#endif //FEATURE_WLAN_CCX || FEATURE_WLAN_LFR

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limReassocBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &frm.WMMInfoStation );
        }

        if ( fWsmEnabled &&
                ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limReassocBssPropCap )))
        {
            PopulateDot11fWMMCaps( &frm.WMMCaps );
        }
#ifdef FEATURE_WLAN_CCX
        if (psessionEntry->isCCXconnection) 
        {
            PopulateDot11fReAssocTspec(pMac, &frm, psessionEntry);

            // Populate the TSRS IE if TSPEC is included in the reassoc request
            if (psessionEntry->pLimReAssocReq->ccxTspecInfo.numTspecs) 
            {
                tANI_U32 phyMode;
                tSirMacCCXTSRSIE    tsrsIE; 
                limGetPhyMode(pMac, &phyMode, psessionEntry);

                tsrsIE.tsid = 0;
                if( phyMode == WNI_CFG_PHY_MODE_11G || phyMode == WNI_CFG_PHY_MODE_11A)
                {
                    tsrsIE.rates[0] = TSRS_11AG_RATE_6MBPS;
                }
                else 
                {
                    tsrsIE.rates[0] = TSRS_11B_RATE_5_5MBPS;
                }
                PopulateDot11TSRSIE(pMac,&tsrsIE, &frm.CCXTrafStrmRateSet, sizeof(tANI_U8));
            }
        }
#endif    
    }

    if ( psessionEntry->htCapabality &&
            pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, &frm.HTCaps );
    }

    nStatus = dot11fGetPackedReAssocRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                    "or a Re-Association Request (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fReAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                    "the packed size for a Re-Association Re "
                    "quest(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen; ;

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    limLog( pMac, LOGE, FL("FT IE Reassoc Req (%d).\n"), 
            pMac->ft.ftSmeContext.reassoc_ft_ies_length);
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
    if (psessionEntry->is11Rconnection)
    {
        ft_ies_length = pMac->ft.ftSmeContext.reassoc_ft_ies_length;
    }
#endif

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
            ( tANI_U16 )nBytes+ft_ies_length, ( void** ) &pFrame,
            ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Re-As"
                    "sociation Request.\n"), nBytes );
        goto end;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes + ft_ies_length);

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
    limPrintMacAddr(pMac, psessionEntry->limReAssocbssId, LOGE);
#endif
    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
            SIR_MAC_MGMT_REASSOC_REQ,
            psessionEntry->limReAssocbssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                    "tor for an Association Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }


    // That done, pack the ReAssoc Request:
    nStatus = dot11fPackReAssocRequest( pMac, &frm, pFrame +
            sizeof(tSirMacMgmtHdr),
            nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Re-Association Reque"
                    "st (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a R"
                    "e-Association Request (0x%08x).\n") );
    }

    PELOG3(limLog( pMac, LOG3, 
            FL("*** Sending Re-Association Request length %d %d to \n"),
            nBytes, nPayload );)
    if( psessionEntry->assocReq != NULL )
    {
        palFreeMemory(pMac->hHdd, psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    if( nAddIELen )
    {
        palCopyMemory( pMac->hHdd, pFrame + sizeof(tSirMacMgmtHdr) + nPayload, 
                       pAddIE, 
                       nAddIELen );
        nPayload += nAddIELen;
    }

    if( (palAllocateMemory(pMac->hHdd, (void**)&psessionEntry->assocReq,
                           nPayload)) != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        palCopyMemory(pMac->hHdd, psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if (psessionEntry->is11Rconnection)
    {
        {
            int i = 0;

            pBody = pFrame + nBytes;
            for (i=0; i<ft_ies_length; i++)
            {
                *pBody = pMac->ft.ftSmeContext.reassoc_ft_ies[i];
                pBody++;
            }
        }
    }

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    PELOGE(limLog(pMac, LOGE, FL("Re-assoc Req Frame is: "));
            sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOGE,
                (tANI_U8 *)pFrame,
                (nBytes + ft_ies_length));)
#endif


    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }
 
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (nBytes + ft_ies_length),
            HAL_TXRX_FRM_802_11_MGMT,
            ANI_TXDIR_TODS,
            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Re-Association Request"
                    "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        goto end;
    }

end:
    // Free up buffer allocated for mlmAssocReq
    palFreeMemory( pMac->hHdd, ( tANI_U8* ) pMlmReassocReq );
    psessionEntry->pLimMlmReassocReq = NULL;

}
#endif /* WLAN_FEATURE_VOWIFI_11R */


void
limSendReassocReqMgmtFrame(tpAniSirGlobal     pMac,
                           tLimMlmReassocReq *pMlmReassocReq,tpPESession psessionEntry)
{
    static tDot11fReAssocRequest frm;
    tANI_U16              caps;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tANI_U32              nBytes, nPayload, nStatus;
    tANI_U8               fQosEnabled, fWmeEnabled, fWsmEnabled;
    void                 *pPacket;
    eHalStatus            halstatus;
    tANI_U16              nAddIELen; 
    tANI_U8               *pAddIE;
    tANI_U8               *wpsIe = NULL;
    tANI_U8               txFlag = 0;
#if defined WLAN_FEATURE_VOWIFI
    tANI_U8               PowerCapsPopulated = FALSE;
#endif

    if(NULL == psessionEntry)
    {
        return;
    }

    /* check this early to avoid unncessary operation */
    if(NULL == psessionEntry->pLimReAssocReq)
    {
        return;
    }
    nAddIELen = psessionEntry->pLimReAssocReq->addIEAssoc.length; 
    pAddIE = psessionEntry->pLimReAssocReq->addIEAssoc.addIEdata;
    
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    caps = pMlmReassocReq->capabilityInfo;
    if (PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap))
        ((tSirMacCapabilityInfo *) &caps)->qos = 0;
#if defined(FEATURE_WLAN_WAPI)
    /* CR: 262463 : 
    According to WAPI standard:
    7.3.1.4 Capability Information field
    In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in transmitted 
    Association or Reassociation management frames. APs ignore the Privacy subfield within received Association and 
    Reassociation management frames. */
    if ( psessionEntry->encryptType == eSIR_ED_WPI)
        ((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
    swapBitField16(caps, ( tANI_U16* )&frm.Capabilities );

    frm.ListenInterval.interval = pMlmReassocReq->listenInterval;

    palCopyMemory( pMac->hHdd, ( tANI_U8* )frm.CurrentAPAddress.mac,
                   ( tANI_U8* )psessionEntry->bssId, 6 );

    PopulateDot11fSSID2( pMac, &frm.SSID );
    PopulateDot11fSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                             &frm.SuppRates,psessionEntry);

    fQosEnabled = ( psessionEntry->limQosEnabled ) &&
        SIR_MAC_GET_QOS( psessionEntry->limReassocBssCaps );

    fWmeEnabled = ( psessionEntry->limWmeEnabled ) &&
        LIM_BSS_CAPS_GET( WME, psessionEntry->limReassocBssQosCaps );

    fWsmEnabled = ( psessionEntry->limWsmEnabled ) && fWmeEnabled &&
        LIM_BSS_CAPS_GET( WSM, psessionEntry->limReassocBssQosCaps );


    if ( psessionEntry->lim11hEnable  &&
         psessionEntry->pLimReAssocReq->spectrumMgtIndicator == eSIR_TRUE )
    {
#if defined WLAN_FEATURE_VOWIFI
        PowerCapsPopulated = TRUE;
        PopulateDot11fPowerCaps( pMac, &frm.PowerCaps, LIM_REASSOC,psessionEntry);
        PopulateDot11fSuppChannels( pMac, &frm.SuppChannels, LIM_REASSOC,psessionEntry);
#endif
    }

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
        SIR_MAC_GET_RRM( psessionEntry->limCurrentBssCaps ) )
    {
        if (PowerCapsPopulated == FALSE) 
        {
            PowerCapsPopulated = TRUE;
            PopulateDot11fPowerCaps(pMac, &frm.PowerCaps, LIM_REASSOC, psessionEntry);
        }
    }
#endif

    if ( fQosEnabled &&
         ( ! PROP_CAPABILITY_GET(11EQOS, psessionEntry->limReassocBssPropCap ) ))
    {
        PopulateDot11fQOSCapsStation( pMac, &frm.QOSCapsStation );
    }

    PopulateDot11fExtSuppRates( pMac, POPULATE_DOT11F_RATES_OPERATIONAL,
                                &frm.ExtSuppRates, psessionEntry );

#if defined WLAN_FEATURE_VOWIFI
    if( pMac->rrm.rrmPEContext.rrmEnable &&
        SIR_MAC_GET_RRM( psessionEntry->limReassocBssCaps ) )
    {
        PopulateDot11fRRMIe( pMac, &frm.RRMEnabledCap, psessionEntry );       
    }
#endif
    // The join request *should* contain zero or one of the WPA and RSN
    // IEs.  The payload send along with the request is a
    // 'tSirSmeJoinReq'; the IE portion is held inside a 'tSirRSNie':

    //     typedef struct sSirRSNie
    //     {
    //         tANI_U16       length;
    //         tANI_U8        rsnIEdata[SIR_MAC_MAX_IE_LENGTH+2];
    //     } tSirRSNie, *tpSirRSNie;

    // So, we should be able to make the following two calls harmlessly,
    // since they do nothing if they don't find the given IE in the
    // bytestream with which they're provided.

    // The net effect of this will be to faithfully transmit whatever
    // security IE is in the join request.

    // *However*, if we're associating for the purpose of WPS
    // enrollment, and we've been configured to indicate that by
    // eliding the WPA or RSN IE, we just skip this:
    if( nAddIELen && pAddIE )
    {
        wpsIe = limGetWscIEPtr(pMac, pAddIE, nAddIELen);
    }
    if ( NULL == wpsIe )
    {
        PopulateDot11fRSNOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.RSNOpaque );
        PopulateDot11fWPAOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.WPAOpaque );
#if defined(FEATURE_WLAN_WAPI)
        PopulateDot11fWAPIOpaque( pMac, &( psessionEntry->pLimReAssocReq->rsnIE ),
                                 &frm.WAPIOpaque );
#endif // defined(FEATURE_WLAN_WAPI)
    }

    // include WME EDCA IE as well
    if ( fWmeEnabled )
    {
        if ( ! PROP_CAPABILITY_GET( WME, psessionEntry->limReassocBssPropCap ) )
        {
            PopulateDot11fWMMInfoStation( pMac, &frm.WMMInfoStation );
        }

        if ( fWsmEnabled &&
             ( ! PROP_CAPABILITY_GET(WSM, psessionEntry->limReassocBssPropCap )))
        {
            PopulateDot11fWMMCaps( &frm.WMMCaps );
        }
    }

    if ( psessionEntry->htCapabality &&
          pMac->lim.htCapabilityPresentInBeacon)
    {
        PopulateDot11fHTCaps( pMac, &frm.HTCaps );
    }

    nStatus = dot11fGetPackedReAssocRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Re-Association Request (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fReAssocRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Re-Association Re "
                               "quest(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + nAddIELen;

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Re-As"
                               "sociation Request.\n"), nBytes );
        goto end;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_REASSOC_REQ,
                                psessionEntry->limReAssocbssId,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for an Association Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }


    // That done, pack the Probe Request:
    nStatus = dot11fPackReAssocRequest( pMac, &frm, pFrame +
                                        sizeof(tSirMacMgmtHdr),
                                        nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Re-Association Reque"
                               "st (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        goto end;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a R"
                               "e-Association Request (0x%08x).\n") );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending Re-Association Request length %d"
                           "to \n"),
            nBytes );)

    if( psessionEntry->assocReq != NULL )
    {
        palFreeMemory(pMac->hHdd, psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    if( nAddIELen )
    {
        palCopyMemory( pMac->hHdd, pFrame + sizeof(tSirMacMgmtHdr) + nPayload, 
                       pAddIE, 
                       nAddIELen );
        nPayload += nAddIELen;
    }

    if( (palAllocateMemory(pMac->hHdd, (void**)&psessionEntry->assocReq,
                           nPayload)) != eHAL_STATUS_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("Unable to allocate memory to store assoc request"));)
    }
    else
    {
        //Store the Assoc request. This is sent to csr/hdd in join cnf response. 
        palCopyMemory(pMac->hHdd, psessionEntry->assocReq, pFrame + sizeof(tSirMacMgmtHdr), nPayload);
        psessionEntry->assocReqLen = nPayload;
    }

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) (sizeof(tSirMacMgmtHdr) + nPayload),
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Re-Association Request"
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        goto end;
    }

end:
    // Free up buffer allocated for mlmAssocReq
    palFreeMemory( pMac->hHdd, ( tANI_U8* ) pMlmReassocReq );
    psessionEntry->pLimMlmReassocReq = NULL;

} // limSendReassocReqMgmtFrame

/**
 * \brief Send an Authentication frame
 *
 *
 * \param pMac Pointer to Global MAC structure
 *
 * \param pAuthFrameBody Pointer to Authentication frame structure that need
 * to be sent
 *
 * \param peerMacAddr MAC address of the peer entity to which Authentication
 * frame is destined
 *
 * \param wepBit Indicates whether wep bit to be set in FC while sending
 * Authentication frame3
 *
 *
 * This function is called by limProcessMlmMessages().  Authentication frame
 * is formatted and sent when this function is called.
 *
 *
 */

void
limSendAuthMgmtFrame(tpAniSirGlobal pMac,
                     tpSirMacAuthFrameBody pAuthFrameBody,
                     tSirMacAddr           peerMacAddr,
                     tANI_U8               wepBit,
                     tpPESession           psessionEntry 
                                                       )
{
    tANI_U8            *pFrame, *pBody;
    tANI_U32            frameLen = 0, bodyLen = 0;
    tpSirMacMgmtHdr     pMacHdr;
    tANI_U16            i;
    void               *pPacket;
    eHalStatus          halstatus;
    tANI_U8             txFlag = 0;

    if(NULL == psessionEntry)
    {
        return;
    }
      
    if (wepBit == LIM_WEP_IN_FC)
    {
        /// Auth frame3 to be sent with encrypted framebody
        /**
         * Allocate buffer for Authenticaton frame of size equal
         * to management frame header length plus 2 bytes each for
         * auth algorithm number, transaction number, status code,
         * 128 bytes for challenge text and 4 bytes each for
         * IV & ICV.
         */

        frameLen = sizeof(tSirMacMgmtHdr) + LIM_ENCR_AUTH_BODY_LEN;

        bodyLen = LIM_ENCR_AUTH_BODY_LEN;
    } // if (wepBit == LIM_WEP_IN_FC)
    else
    {
        switch (pAuthFrameBody->authTransactionSeqNumber)
        {
            case SIR_MAC_AUTH_FRAME_1:
                /**
                 * Allocate buffer for Authenticaton frame of size
                 * equal to management frame header length plus 2 bytes
                 * each for auth algorithm number, transaction number
                 * and status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

#if defined WLAN_FEATURE_VOWIFI_11R
                if (pAuthFrameBody->authAlgoNumber == eSIR_FT_AUTH)
                {
                    if (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies) 
                    {
                        frameLen += pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length;
                        limLog(pMac, LOG3, FL("Auth frame, FTIES length added=%d\n"), 
                        pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length);
                    }
                    else 
                        limLog(pMac, LOG3, FL("Auth frame, Does not contain FTIES!!!\n"));
                }
#endif
                break;

            case SIR_MAC_AUTH_FRAME_2:
                if ((pAuthFrameBody->authAlgoNumber == eSIR_OPEN_SYSTEM) ||
                    ((pAuthFrameBody->authAlgoNumber == eSIR_SHARED_KEY) &&
                     (pAuthFrameBody->authStatusCode != eSIR_MAC_SUCCESS_STATUS)))
                {
                    /**
                     * Allocate buffer for Authenticaton frame of size
                     * equal to management frame header length plus
                     * 2 bytes each for auth algorithm number,
                     * transaction number and status code.
                     */

                    frameLen = sizeof(tSirMacMgmtHdr) +
                               SIR_MAC_AUTH_CHALLENGE_OFFSET;
                    bodyLen = SIR_MAC_AUTH_CHALLENGE_OFFSET;
                }
                else
                {
                    // Shared Key algorithm with challenge text
                    // to be sent
                    /**
                     * Allocate buffer for Authenticaton frame of size
                     * equal to management frame header length plus
                     * 2 bytes each for auth algorithm number,
                     * transaction number, status code and 128 bytes
                     * for challenge text.
                     */

                    frameLen = sizeof(tSirMacMgmtHdr) +
                               sizeof(tSirMacAuthFrame);
                    bodyLen  = sizeof(tSirMacAuthFrameBody);
                }

                break;

            case SIR_MAC_AUTH_FRAME_3:
                /// Auth frame3 to be sent without encrypted framebody
                /**
                 * Allocate buffer for Authenticaton frame of size equal
                 * to management frame header length plus 2 bytes each
                 * for auth algorithm number, transaction number and
                 * status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

                break;

            case SIR_MAC_AUTH_FRAME_4:
                /**
                 * Allocate buffer for Authenticaton frame of size equal
                 * to management frame header length plus 2 bytes each
                 * for auth algorithm number, transaction number and
                 * status code.
                 */

                frameLen = sizeof(tSirMacMgmtHdr) +
                           SIR_MAC_AUTH_CHALLENGE_OFFSET;
                bodyLen  = SIR_MAC_AUTH_CHALLENGE_OFFSET;

                break;
        } // switch (pAuthFrameBody->authTransactionSeqNumber)
    } // end if (wepBit == LIM_WEP_IN_FC)


    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )frameLen, ( void** ) &pFrame, ( void** ) &pPacket );

    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        // Log error
        limLog(pMac, LOGP, FL("call to bufAlloc failed for AUTH frame\n"));

        return;
    }

    for (i = 0; i < frameLen; i++)
        pFrame[i] = 0;

    // Prepare BD
    if (limPopulateMacHeader(pMac, pFrame, SIR_MAC_MGMT_FRAME,
                      SIR_MAC_MGMT_AUTH, peerMacAddr,psessionEntry->selfMacAddr) != eSIR_SUCCESS)
    {
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
    pMacHdr->fc.wep = wepBit;

    // Prepare BSSId
    if(  (psessionEntry->limSystemRole == eLIM_AP_ROLE)|| (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE) )
    {
        palCopyMemory( pMac->hHdd,(tANI_U8 *) pMacHdr->bssId,
                       (tANI_U8 *) psessionEntry->bssId,
                       sizeof( tSirMacAddr ));
    }

    /// Prepare Authentication frame body
    pBody    = pFrame + sizeof(tSirMacMgmtHdr);

    if (wepBit == LIM_WEP_IN_FC)
    {
        palCopyMemory( pMac->hHdd, pBody, (tANI_U8 *) pAuthFrameBody, bodyLen);

        PELOG1(limLog(pMac, LOG1,
           FL("*** Sending Auth seq# 3 status %d (%d) to\n"),
           pAuthFrameBody->authStatusCode,
           (pAuthFrameBody->authStatusCode == eSIR_MAC_SUCCESS_STATUS));

        limPrintMacAddr(pMac, pMacHdr->da, LOG1);)
    }
    else
    {
        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authAlgoNumber);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);

        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authTransactionSeqNumber);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);

        *((tANI_U16 *)(pBody)) = sirSwapU16ifNeeded(pAuthFrameBody->authStatusCode);
        pBody   += sizeof(tANI_U16);
        bodyLen -= sizeof(tANI_U16);

        palCopyMemory( pMac->hHdd, pBody, (tANI_U8 *) &pAuthFrameBody->type, bodyLen);

#if defined WLAN_FEATURE_VOWIFI_11R
        if ((pAuthFrameBody->authAlgoNumber == eSIR_FT_AUTH) && 
                (pAuthFrameBody->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_1))
        {

            {
                int i = 0;
#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
                if (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length) 
                {
                    PELOGE(limLog(pMac, LOGE, FL("Auth1 Frame FTIE is: "));
                        sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOGE,
                            (tANI_U8 *)pBody,
                            (pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length));)
                }
#endif
                for (i=0; i<pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies_length; i++)
                {
                    *pBody = pMac->ft.ftPEContext.pFTPreAuthReq->ft_ies[i];
                    pBody++;
                }
            }
        }
#endif

        PELOG1(limLog(pMac, LOG1,
           FL("*** Sending Auth seq# %d status %d (%d) to "),
           pAuthFrameBody->authTransactionSeqNumber,
           pAuthFrameBody->authStatusCode,
           (pAuthFrameBody->authStatusCode == eSIR_MAC_SUCCESS_STATUS));

        limPrintMacAddr(pMac, pMacHdr->da, LOG1);)
    }
    PELOG2(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2, pFrame, frameLen);)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    /// Queue Authentication frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) frameLen,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("*** Could not send Auth frame, retCode=%X ***\n"),
               halstatus);

        //Pkt will be freed up by the callback
    }

    return;
} /*** end limSendAuthMgmtFrame() ***/

/**
 * \brief This function is called to send Disassociate frame.
 *
 *
 * \param pMac Pointer to Global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in
 * Disassociation frame
 *
 * \param peerMacAddr MAC address of the STA to which Disassociation frame is
 * sent
 *
 *
 */

void
limSendDisassocMgmtFrame(tpAniSirGlobal pMac,
                         tANI_U16       nReason,
                         tSirMacAddr    peer,tpPESession psessionEntry)
{
    tDot11fDisassociation frm;
    tANI_U8              *pFrame;
    tSirRetStatus         nSirStatus;
    tpSirMacMgmtHdr       pMacHdr;
    tANI_U32              nBytes, nPayload, nStatus;
    void                 *pPacket;
    eHalStatus            halstatus;
    tANI_U8               txFlag = 0;

    if(NULL == psessionEntry)
    {
        return;
    }
    
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    frm.Reason.code = nReason;

    nStatus = dot11fGetPackedDisassociationSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Disassociation (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDisassociation );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Disassociation "
                               "(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Dis"
                               "association.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_DISASSOC, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Disassociation (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    // Prepare the BSSID
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);
    
    nStatus = dot11fPackDisassociation( pMac, &frm, pFrame +
                                        sizeof(tSirMacMgmtHdr),
                                        nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Disassociation (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a D"
                               "isassociation (0x%08x).\n") );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending Disassociation frame with rea"
                           "son %d to\n"), nReason );
    limPrintMacAddr( pMac, pMacHdr->da, LOG1 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue Disassociation frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send Disassociation "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return;
    }

} // End limSendDisassocMgmtFrame.

/**
 * \brief This function is called to send a Deauthenticate frame
 *
 *
 * \param pMac Pointer to global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in the
 * Deauthenticate frame
 *
 * \param peeer address of the STA to which the frame is to be sent
 *
 *
 */

void
limSendDeauthMgmtFrame(tpAniSirGlobal pMac,
                       tANI_U16       nReason,
                       tSirMacAddr    peer,tpPESession psessionEntry)
{
    tDot11fDeAuth    frm;
    tANI_U8         *pFrame;
    tSirRetStatus    nSirStatus;
    tpSirMacMgmtHdr  pMacHdr;
    tANI_U32         nBytes, nPayload, nStatus;
    void            *pPacket;
    eHalStatus       halstatus;
    tANI_U8          txFlag = 0;

    if(NULL == psessionEntry)
    {
        return;
    }
    
    palZeroMemory( pMac->hHdd, ( tANI_U8* ) &frm, sizeof( frm ) );

    frm.Reason.code = nReason;

    nStatus = dot11fGetPackedDeAuthSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a De-Authentication (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDeAuth );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a De-Authentication "
                               "(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a De-"
                               "Authentication.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_DEAUTH, peer,psessionEntry->selfMacAddr);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a De-Authentication (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    // Prepare the BSSID
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    nStatus = dot11fPackDeAuth( pMac, &frm, pFrame +
                                sizeof(tSirMacMgmtHdr),
                                nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a DeAuthentication (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                    ( void* ) pFrame, ( void* ) pPacket );
        return;
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a D"
                               "e-Authentication (0x%08x).\n") );
    }

    PELOG1(limLog( pMac, LOG1, FL("*** Sending De-Authentication frame with rea"
                           "son %d to\n"), nReason );
    limPrintMacAddr( pMac, pMacHdr->da, LOG1 );)

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    // Queue Disassociation frame in high priority WQ
    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, txFlag );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send De-Authentication "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return;
    }

} // End limSendDeauthMgmtFrame.


#ifdef ANI_SUPPORT_11H
/**
 * \brief Send a Measurement Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pMeasReqFrame Address of a tSirMacMeasReqActionFrame
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendMeasReportFrame(tpAniSirGlobal             pMac,
                       tpSirMacMeasReqActionFrame pMeasReqFrame,
                       tSirMacAddr                peer)
{
    tDot11fMeasurementReport frm;
    tANI_U8                      *pFrame;
    tSirRetStatus            nSirStatus;
    tpSirMacMgmtHdr          pMacHdr;
    tANI_U32                      nBytes, nPayload, nStatus, nCfg;
    void               *pPacket;
    eHalStatus          halstatus;
   
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    frm.Category.category = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action     = SIR_MAC_ACTION_MEASURE_REPORT_ID;
    frm.DialogToken.token = pMeasReqFrame->actionHeader.dialogToken;

    switch ( pMeasReqFrame->measReqIE.measType )
    {
    case SIR_MAC_BASIC_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport0( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    case SIR_MAC_CCA_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport1( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    case SIR_MAC_RPI_MEASUREMENT_TYPE:
        nSirStatus =
            PopulateDot11fMeasurementReport2( pMac, pMeasReqFrame,
                                               &frm.MeasurementReport );
        break;
    default:
        limLog( pMac, LOGE, FL("Unknown measurement type %d in limSen"
                               "dMeasReportFrame.\n"),
                pMeasReqFrame->measReqIE.measType );
        return eSIR_FAILURE;
    }

    if ( eSIR_SUCCESS != nSirStatus ) return eSIR_FAILURE;

    nStatus = dot11fGetPackedMeasurementReportSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Measurement Report (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fMeasurementReport );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Measurement Rep"
                               "ort (0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a De-"
                               "Authentication.\n"), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Measurement Report (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    nStatus = dot11fPackMeasurementReport( pMac, &frm, pFrame +
                                           sizeof(tSirMacMgmtHdr),
                                           nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Measurement Report (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a M"
                               "easurement Report (0x%08x).\n") );
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Measurement Report  "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;    // just allocated...
    }

    return eSIR_SUCCESS;

} // End limSendMeasReportFrame.


/**
 * \brief Send a TPC Request Action frame
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which the frame should be sent
 *
 *
 */

void
limSendTpcRequestFrame(tpAniSirGlobal pMac,
                       tSirMacAddr    peer)
{
    tDot11fTPCRequest  frm;
    tANI_U8                *pFrame;
    tSirRetStatus      nSirStatus;
    tpSirMacMgmtHdr    pMacHdr;
    tANI_U32                nBytes, nPayload, nStatus, nCfg;
    void               *pPacket;
    eHalStatus          halstatus;
   
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    frm.Category.category  = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action      = SIR_MAC_ACTION_TPC_REQUEST_ID;
    frm.DialogToken.token  = 1;
    frm.TPCRequest.present = 1;

    nStatus = dot11fGetPackedTPCRequestSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a TPC Request (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fTPCRequest );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a TPC Request (0x"
                               "%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Request.\n"), nBytes );
        return;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a TPC Request (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // just allocated...
    }

    nStatus = dot11fPackTPCRequest( pMac, &frm, pFrame +
                                    sizeof(tSirMacMgmtHdr),
                                    nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a TPC Request (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a T"
                               "PC Request (0x%08x).\n") );
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a TPC Request "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return;
    }

} // End limSendTpcRequestFrame.


/**
 * \brief Send a TPC Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param pTpcReqFrame Pointer to the received TPC Request
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendTpcReportFrame(tpAniSirGlobal            pMac,
                      tpSirMacTpcReqActionFrame pTpcReqFrame,
                      tSirMacAddr               peer)
{
    tDot11fTPCReport frm;
    tANI_U8              *pFrame;
    tSirRetStatus    nSirStatus;
    tpSirMacMgmtHdr  pMacHdr;
    tANI_U32              nBytes, nPayload, nStatus, nCfg;
    void               *pPacket;
    eHalStatus          halstatus;
   
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    frm.Category.category  = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action      = SIR_MAC_ACTION_TPC_REPORT_ID;
    frm.DialogToken.token  = pTpcReqFrame->actionHeader.dialogToken;

    // FramesToDo: On the Gen4_TVM branch, there was a comment:
    // "misplaced this function, need to replace:
    // txPower = halGetRateToPwrValue(pMac, staid,
    //     pMac->lim.gLimCurrentChannelId, 0);
    frm.TPCReport.tx_power    = 0;
    frm.TPCReport.link_margin = 0;
    frm.TPCReport.present     = 1;

    nStatus = dot11fGetPackedTPCReportSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a TPC Report (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fTPCReport );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a TPC Report (0x"
                               "%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Report.\n"), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a TPC Report (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    nStatus = dot11fPackTPCReport( pMac, &frm, pFrame +
                                   sizeof(tSirMacMgmtHdr),
                                   nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a TPC Report (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a T"
                               "PC Report (0x%08x).\n") );
    }


    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a TPC Report "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;    // just allocated...
    }

    return eSIR_SUCCESS;

} // End limSendTpcReportFrame.
#endif  //ANI_SUPPORT_11H


#ifdef ANI_PRODUCT_TYPE_AP
/**
 * \brief Send a Channel Switch Announcement
 *
 *
 * \param pMac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which this frame will be sent
 *
 * \param nMode
 *
 * \param nNewChannel
 *
 * \param nCount
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendChannelSwitchMgmtFrame(tpAniSirGlobal pMac,
                              tSirMacAddr    peer,
                              tANI_U8             nMode,
                              tANI_U8             nNewChannel,
                              tANI_U8             nCount)
{
    tDot11fChannelSwitch frm;
    tANI_U8                  *pFrame;
    tSirRetStatus        nSirStatus;
    tpSirMacMgmtHdr      pMacHdr;
    tANI_U32                  nBytes, nPayload, nStatus, nCfg;
    void               *pPacket;
    eHalStatus          halstatus;
    
    palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

    frm.Category.category     = SIR_MAC_ACTION_SPECTRUM_MGMT;
    frm.Action.action         = SIR_MAC_ACTION_CHANNEL_SWITCH_ID;
    frm.ChanSwitchAnn.switchMode    = nMode;
    frm.ChanSwitchAnn.newChannel    = nNewChannel;
    frm.ChanSwitchAnn.switchCount   = nCount;
    frm.ChanSwitchAnn.present = 1;

    nStatus = dot11fGetPackedChannelSwitchSize( pMac, &frm, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
                               "or a Channel Switch (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fChannelSwitch );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while calculating"
                               "the packed size for a Channel Switch (0x"
                               "%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a TPC"
                               " Report.\n"), nBytes );
        return eSIR_FAILURE;
    }

    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer);
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Channel Switch (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

    nCfg = 6;
    nSirStatus = wlan_cfgGetStr( pMac, WNI_CFG_BSSID, pMacHdr->bssId, &nCfg );
    if ( eSIR_SUCCESS != nSirStatus )
    {
        limLog( pMac, LOGE, FL("Failed to retrieve WNI_CFG_BSSID from"
                               " CFG (%d).\n"),
                nSirStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // just allocated...
    }

    nStatus = dot11fPackChannelSwitch( pMac, &frm, pFrame +
                                       sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to pack a Channel Switch (0x%08x).\n"),
                nStatus );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;    // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        limLog( pMac, LOGW, FL("There were warnings while packing a C"
                               "hannel Switch (0x%08x).\n") );
    }

    halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame, 0 );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog( pMac, LOGE, FL("Failed to send a Channel Switch "
                               "(%X)!\n"),
                nSirStatus );
        //Pkt will be freed up by the callback
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

} // End limSendChannelSwitchMgmtFrame.

#endif // (ANI_PRODUCT_TYPE_AP)


/**
 * \brief Send an ADDBA Req Action Frame to peer
 *
 * \sa limSendAddBAReq
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMlmAddBAReq A pointer to tLimMlmAddBAReq. This contains
 * the necessary parameters reqd by PE send the ADDBA Req Action
 * Frame to the peer
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendAddBAReq( tpAniSirGlobal pMac,
    tpLimMlmAddBAReq pMlmAddBAReq ,tpPESession psessionEntry)
{
    tDot11fAddBAReq frmAddBAReq;
    tANI_U8 *pAddBAReqBuffer = NULL;
    tpSirMacMgmtHdr pMacHdr;
    tANI_U32 frameLen = 0, nStatus, nPayload;
    tSirRetStatus statusCode;
    eHalStatus halStatus;
    void *pPacket;
    tANI_U8 txFlag = 0;

     if(NULL == psessionEntry)
    {
        return eSIR_FAILURE;
    }

    palZeroMemory( pMac->hHdd, (void *) &frmAddBAReq, sizeof( frmAddBAReq ));

    // Category - 3 (BA)
    frmAddBAReq.Category.category = SIR_MAC_ACTION_BLKACK;
    
    // Action - 0 (ADDBA Req)
    frmAddBAReq.Action.action = SIR_MAC_BLKACK_ADD_REQ;

    // FIXME - Dialog Token, generalize this...
    frmAddBAReq.DialogToken.token = pMlmAddBAReq->baDialogToken;

    // Fill the ADDBA Parameter Set
    frmAddBAReq.AddBAParameterSet.tid = pMlmAddBAReq->baTID;
    frmAddBAReq.AddBAParameterSet.policy = pMlmAddBAReq->baPolicy;
    frmAddBAReq.AddBAParameterSet.bufferSize = pMlmAddBAReq->baBufferSize;

    // BA timeout
    // 0 - indicates no BA timeout
    frmAddBAReq.BATimeout.timeout = pMlmAddBAReq->baTimeout;

  // BA Starting Sequence Number
  // Fragment number will always be zero
  if (pMlmAddBAReq->baSSN < LIM_TX_FRAMES_THRESHOLD_ON_CHIP) {
      pMlmAddBAReq->baSSN = LIM_TX_FRAMES_THRESHOLD_ON_CHIP;
  }
  
  frmAddBAReq.BAStartingSequenceControl.ssn = 
                pMlmAddBAReq->baSSN - LIM_TX_FRAMES_THRESHOLD_ON_CHIP;

    nStatus = dot11fGetPackedAddBAReqSize( pMac, &frmAddBAReq, &nPayload );

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGW,
        FL( "Failed to calculate the packed size for "
          "an ADDBA Request (0x%08x).\n"),
        nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAddBAReq );
    }
    else if( DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
        FL( "There were warnings while calculating"
          "the packed size for an ADDBA Req (0x%08x).\n"),
        nStatus );
    }

    // Add the MGMT header to frame length
    frameLen = nPayload + sizeof( tSirMacMgmtHdr );

    // Need to allocate a buffer for ADDBA AF
    if( eHAL_STATUS_SUCCESS !=
      (halStatus = palPktAlloc( pMac->hHdd,
                                HAL_TXRX_FRM_802_11_MGMT,
                                (tANI_U16) frameLen,
                                (void **) &pAddBAReqBuffer,
                                (void **) &pPacket )))
    {
        // Log error
        limLog( pMac, LOGP,
        FL("palPktAlloc FAILED! Length [%d], Status [%d]\n"),
        frameLen,
        halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
    }

    palZeroMemory( pMac->hHdd, (void *) pAddBAReqBuffer, frameLen );

    // Copy necessary info to BD
    if( eSIR_SUCCESS !=
      (statusCode = limPopulateMacHeader( pMac,
                                   pAddBAReqBuffer,
                                   SIR_MAC_MGMT_FRAME,
                                   SIR_MAC_MGMT_ACTION,
                                   pMlmAddBAReq->peerMacAddr,psessionEntry->selfMacAddr)))
    goto returnAfterError;

    // Update A3 with the BSSID
    pMacHdr = ( tpSirMacMgmtHdr ) pAddBAReqBuffer;
    
    #if 0
    cfgLen = SIR_MAC_ADDR_LENGTH;
    if( eSIR_SUCCESS != cfgGetStr( pMac,
        WNI_CFG_BSSID,
        (tANI_U8 *) pMacHdr->bssId,
        &cfgLen ))
    {
        limLog( pMac, LOGP,
        FL( "Failed to retrieve WNI_CFG_BSSID while"
          "sending an ACTION Frame\n" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
    }
    #endif//TO SUPPORT BT-AMP
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    // Now, we're ready to "pack" the frames
    nStatus = dot11fPackAddBAReq( pMac,
      &frmAddBAReq,
      pAddBAReqBuffer + sizeof( tSirMacMgmtHdr ),
      nPayload,
      &nPayload );

    if( DOT11F_FAILED( nStatus ))
    {
        limLog( pMac, LOGE,
        FL( "Failed to pack an ADDBA Req (0x%08x).\n" ),
        nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
    }
    else if( DOT11F_WARNED( nStatus ))
    {
        limLog( pMac, LOGW,
        FL( "There were warnings while packing an ADDBA Req (0x%08x).\n" ));
    }

    limLog( pMac, LOGW,
      FL( "Sending an ADDBA REQ to \n" ));
    limPrintMacAddr( pMac, pMlmAddBAReq->peerMacAddr, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

    if( eHAL_STATUS_SUCCESS !=
      (halStatus = halTxFrame( pMac,
                               pPacket,
                               (tANI_U16) frameLen,
                               HAL_TXRX_FRM_802_11_MGMT,
                               ANI_TXDIR_TODS,
                               7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                               limTxComplete,
                               pAddBAReqBuffer, txFlag )))
    {
        limLog( pMac, LOGE,
        FL( "halTxFrame FAILED! Status [%d]\n"),
        halStatus );

    // FIXME - Need to convert eHalStatus to tSirRetStatus
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

returnAfterError:

  // Release buffer, if allocated
  if( NULL != pAddBAReqBuffer )
    palPktFree( pMac->hHdd,
        HAL_TXRX_FRM_802_11_MGMT,
        (void *) pAddBAReqBuffer,
        (void *) pPacket );

  return statusCode;
}

/**
 * \brief Send an ADDBA Rsp Action Frame to peer
 *
 * \sa limSendAddBARsp
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMlmAddBARsp A pointer to tLimMlmAddBARsp. This contains
 * the necessary parameters reqd by PE send the ADDBA Rsp Action
 * Frame to the peer
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendAddBARsp( tpAniSirGlobal pMac,
    tpLimMlmAddBARsp pMlmAddBARsp,
    tpPESession      psessionEntry)
{
    tDot11fAddBARsp frmAddBARsp;
    tANI_U8 *pAddBARspBuffer = NULL;
    tpSirMacMgmtHdr pMacHdr;
    tANI_U32 frameLen = 0, nStatus, nPayload;
    tSirRetStatus statusCode;
    eHalStatus halStatus;
    void *pPacket;
    tANI_U8 txFlag = 0;

     if(NULL == psessionEntry)
    {
        PELOGE(limLog(pMac, LOGE, FL("Session entry is NULL!!!\n"));)
        return eSIR_FAILURE;
    }

      palZeroMemory( pMac->hHdd, (void *) &frmAddBARsp, sizeof( frmAddBARsp ));

      // Category - 3 (BA)
      frmAddBARsp.Category.category = SIR_MAC_ACTION_BLKACK;
      // Action - 1 (ADDBA Rsp)
      frmAddBARsp.Action.action = SIR_MAC_BLKACK_ADD_RSP;

      // Should be same as the one we received in the ADDBA Req
      frmAddBARsp.DialogToken.token = pMlmAddBARsp->baDialogToken;

      // ADDBA Req status
      frmAddBARsp.Status.status = pMlmAddBARsp->addBAResultCode;

      // Fill the ADDBA Parameter Set as provided by caller
      frmAddBARsp.AddBAParameterSet.tid = pMlmAddBARsp->baTID;
      frmAddBARsp.AddBAParameterSet.policy = pMlmAddBARsp->baPolicy;
      frmAddBARsp.AddBAParameterSet.bufferSize = pMlmAddBARsp->baBufferSize;

      // BA timeout
      // 0 - indicates no BA timeout
      frmAddBARsp.BATimeout.timeout = pMlmAddBARsp->baTimeout;

      nStatus = dot11fGetPackedAddBARspSize( pMac, &frmAddBARsp, &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "Failed to calculate the packed size for "
              "an ADDBA Response (0x%08x).\n"),
            nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fAddBARsp );
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while calculating"
              "the packed size for an ADDBA Rsp (0x%08x).\n"),
            nStatus );
      }

      // Need to allocate a buffer for ADDBA AF
      frameLen = nPayload + sizeof( tSirMacMgmtHdr );

      // Allocate shared memory
      if( eHAL_STATUS_SUCCESS !=
          (halStatus = palPktAlloc( pMac->hHdd,
                                    HAL_TXRX_FRM_802_11_MGMT,
                                    (tANI_U16) frameLen,
                                    (void **) &pAddBARspBuffer,
                                    (void **) &pPacket )))
      {
        // Log error
        limLog( pMac, LOGP,
            FL("palPktAlloc FAILED! Length [%d], Status [%d]\n"),
            frameLen,
            halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
      }

      palZeroMemory( pMac->hHdd, (void *) pAddBARspBuffer, frameLen );

      // Copy necessary info to BD
      if( eSIR_SUCCESS !=
          (statusCode = limPopulateMacHeader( pMac,
                                       pAddBARspBuffer,
                                       SIR_MAC_MGMT_FRAME,
                                       SIR_MAC_MGMT_ACTION,
                                       pMlmAddBARsp->peerMacAddr,psessionEntry->selfMacAddr)))
        goto returnAfterError;

      // Update A3 with the BSSID
      
      pMacHdr = ( tpSirMacMgmtHdr ) pAddBARspBuffer;
      
      #if 0
      cfgLen = SIR_MAC_ADDR_LENGTH;
      if( eSIR_SUCCESS != wlan_cfgGetStr( pMac,
            WNI_CFG_BSSID,
            (tANI_U8 *) pMacHdr->bssId,
            &cfgLen ))
      {
        limLog( pMac, LOGP,
            FL( "Failed to retrieve WNI_CFG_BSSID while"
              "sending an ACTION Frame\n" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      #endif // TO SUPPORT BT-AMP
      sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

      // Now, we're ready to "pack" the frames
      nStatus = dot11fPackAddBARsp( pMac,
          &frmAddBARsp,
          pAddBARspBuffer + sizeof( tSirMacMgmtHdr ),
          nPayload,
          &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGE,
            FL( "Failed to pack an ADDBA Rsp (0x%08x).\n" ),
            nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while packing an ADDBA Rsp (0x%08x).\n" ));
      }

      limLog( pMac, LOGW,
          FL( "Sending an ADDBA RSP to \n" ));
      limPrintMacAddr( pMac, pMlmAddBARsp->peerMacAddr, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

  if( eHAL_STATUS_SUCCESS !=
      (halStatus = halTxFrame( pMac,
                               pPacket,
                               (tANI_U16) frameLen,
                               HAL_TXRX_FRM_802_11_MGMT,
                               ANI_TXDIR_TODS,
                               7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                               limTxComplete,
                               pAddBARspBuffer, txFlag )))
  {
    limLog( pMac, LOGE,
        FL( "halTxFrame FAILED! Status [%d]\n" ),
        halStatus );

    // FIXME - HAL error codes are different from PE error
    // codes!! And, this routine is returning tSirRetStatus
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

    returnAfterError:

      // Release buffer, if allocated
      if( NULL != pAddBARspBuffer )
        palPktFree( pMac->hHdd,
            HAL_TXRX_FRM_802_11_MGMT,
            (void *) pAddBARspBuffer,
            (void *) pPacket );

      return statusCode;
}

/**
 * \brief Send a DELBA Indication Action Frame to peer
 *
 * \sa limSendDelBAInd
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param peerMacAddr MAC Address of peer
 *
 * \param reasonCode Reason for the DELBA notification
 *
 * \param pBAParameterSet The DELBA Parameter Set.
 * This identifies the TID for which the BA session is
 * being deleted.
 *
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */
tSirRetStatus limSendDelBAInd( tpAniSirGlobal pMac,
    tpLimMlmDelBAReq pMlmDelBAReq,tpPESession psessionEntry)
{
    tDot11fDelBAInd frmDelBAInd;
    tANI_U8 *pDelBAIndBuffer = NULL;
    //tANI_U32 val;
    tpSirMacMgmtHdr pMacHdr;
    tANI_U32 frameLen = 0, nStatus, nPayload;
    tSirRetStatus statusCode;
    eHalStatus halStatus;
    void *pPacket;
    tANI_U8 txFlag = 0;

     if(NULL == psessionEntry)
    {
        return eSIR_FAILURE;
    }

    palZeroMemory( pMac->hHdd, (void *) &frmDelBAInd, sizeof( frmDelBAInd ));

      // Category - 3 (BA)
      frmDelBAInd.Category.category = SIR_MAC_ACTION_BLKACK;
      // Action - 2 (DELBA)
      frmDelBAInd.Action.action = SIR_MAC_BLKACK_DEL;

      // Fill the DELBA Parameter Set as provided by caller
      frmDelBAInd.DelBAParameterSet.tid = pMlmDelBAReq->baTID;
      frmDelBAInd.DelBAParameterSet.initiator = pMlmDelBAReq->baDirection;

      // BA Starting Sequence Number
      // Fragment number will always be zero
      frmDelBAInd.Reason.code = pMlmDelBAReq->delBAReasonCode;

      nStatus = dot11fGetPackedDelBAIndSize( pMac, &frmDelBAInd, &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "Failed to calculate the packed size for "
              "an DELBA Indication (0x%08x).\n"),
            nStatus );

        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fDelBAInd );
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while calculating"
              "the packed size for an DELBA Ind (0x%08x).\n"),
            nStatus );
      }

      // Add the MGMT header to frame length
      frameLen = nPayload + sizeof( tSirMacMgmtHdr );

      // Allocate shared memory
      if( eHAL_STATUS_SUCCESS !=
          (halStatus = palPktAlloc( pMac->hHdd,
                                    HAL_TXRX_FRM_802_11_MGMT,
                                    (tANI_U16) frameLen,
                                    (void **) &pDelBAIndBuffer,
                                    (void **) &pPacket )))
      {
        // Log error
        limLog( pMac, LOGP,
            FL("palPktAlloc FAILED! Length [%d], Status [%d]\n"),
            frameLen,
            halStatus );

        statusCode = eSIR_MEM_ALLOC_FAILED;
        goto returnAfterError;
      }

      palZeroMemory( pMac->hHdd, (void *) pDelBAIndBuffer, frameLen );

      // Copy necessary info to BD
      if( eSIR_SUCCESS !=
          (statusCode = limPopulateMacHeader( pMac,
                                       pDelBAIndBuffer,
                                       SIR_MAC_MGMT_FRAME,
                                       SIR_MAC_MGMT_ACTION,
                                       pMlmDelBAReq->peerMacAddr,psessionEntry->selfMacAddr)))
        goto returnAfterError;

      // Update A3 with the BSSID
      pMacHdr = ( tpSirMacMgmtHdr ) pDelBAIndBuffer;
      
      #if 0
      cfgLen = SIR_MAC_ADDR_LENGTH;
      if( eSIR_SUCCESS != cfgGetStr( pMac,
            WNI_CFG_BSSID,
            (tANI_U8 *) pMacHdr->bssId,
            &cfgLen ))
      {
        limLog( pMac, LOGP,
            FL( "Failed to retrieve WNI_CFG_BSSID while"
              "sending an ACTION Frame\n" ));

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      #endif //TO SUPPORT BT-AMP
      sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

      // Now, we're ready to "pack" the frames
      nStatus = dot11fPackDelBAInd( pMac,
          &frmDelBAInd,
          pDelBAIndBuffer + sizeof( tSirMacMgmtHdr ),
          nPayload,
          &nPayload );

      if( DOT11F_FAILED( nStatus ))
      {
        limLog( pMac, LOGE,
            FL( "Failed to pack an DELBA Ind (0x%08x).\n" ),
            nStatus );

        // FIXME - Need to convert to tSirRetStatus
        statusCode = eSIR_FAILURE;
        goto returnAfterError;
      }
      else if( DOT11F_WARNED( nStatus ))
      {
        limLog( pMac, LOGW,
            FL( "There were warnings while packing an DELBA Ind (0x%08x).\n" ));
      }

      limLog( pMac, LOGW,
          FL( "Sending a DELBA IND to \n" ));
      limPrintMacAddr( pMac, pMlmDelBAReq->peerMacAddr, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

  if( eHAL_STATUS_SUCCESS !=
      (halStatus = halTxFrame( pMac,
                               pPacket,
                               (tANI_U16) frameLen,
                               HAL_TXRX_FRM_802_11_MGMT,
                               ANI_TXDIR_TODS,
                               7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                               limTxComplete,
                               pDelBAIndBuffer, txFlag )))
  {
    PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]\n" ), halStatus );)
    statusCode = eSIR_FAILURE;
    //Pkt will be freed up by the callback
    return statusCode;
  }
  else
    return eSIR_SUCCESS;

    returnAfterError:

      // Release buffer, if allocated
      if( NULL != pDelBAIndBuffer )
        palPktFree( pMac->hHdd,
            HAL_TXRX_FRM_802_11_MGMT,
            (void *) pDelBAIndBuffer,
            (void *) pPacket );

      return statusCode;
}

#if defined WLAN_FEATURE_VOWIFI

/**
 * \brief Send a Neighbor Report Request Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pNeighborReq Address of a tSirMacNeighborReportReq
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendNeighborReportRequestFrame(tpAniSirGlobal        pMac,
                       tpSirMacNeighborReportReq pNeighborReq,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus statusCode = eSIR_SUCCESS;
   tDot11fNeighborReportRequest frm;
   tANI_U8                      *pFrame;
   tpSirMacMgmtHdr          pMacHdr;
   tANI_U32                      nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus          halstatus;
   tANI_U8             txFlag = 0;

   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Neighbor Report request action frame\n") );
      return eSIR_FAILURE;
   }
   palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

   frm.Category.category = SIR_MAC_ACTION_RRM;
   frm.Action.action     = SIR_MAC_RRM_NEIGHBOR_REQ;
   frm.DialogToken.token = pNeighborReq->dialogToken;


   if( pNeighborReq->ssid_present )
   {
      PopulateDot11fSSID( pMac, &pNeighborReq->ssid, &frm.SSID );
   }

   nStatus = dot11fGetPackedNeighborReportRequestSize( pMac, &frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Neighbor Report Request(0x%08x).\n"),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fNeighborReportRequest );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating"
               "the packed size for a Neighbor Rep"
               "ort Request(0x%08x).\n"), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Neighbor "
               "Report Request.\n"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   palZeroMemory( pMac->hHdd, pFrame, nBytes );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackNeighborReportRequest( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Neighbor Report Request (0x%08x).\n" ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
            FL( "There were warnings while packing Neighbor Report Request (0x%08x).\n" ));
   }

   limLog( pMac, LOGW,
         FL( "Sending a Neighbor Report Request to \n" ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   if( eHAL_STATUS_SUCCESS !=
         (halstatus = halTxFrame( pMac,
                                  pPacket,
                                  (tANI_U16) nBytes,
                                  HAL_TXRX_FRM_802_11_MGMT,
                                  ANI_TXDIR_TODS,
                                  7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                                  limTxComplete,
                                  pFrame, txFlag )))
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]\n" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return statusCode;
   }
   else
      return eSIR_SUCCESS;

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );

   return statusCode;
} // End limSendNeighborReportRequestFrame.

/**
 * \brief Send a Link Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param pLinkReport Address of a tSirMacLinkReport
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendLinkReportActionFrame(tpAniSirGlobal        pMac,
                       tpSirMacLinkReport pLinkReport,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus statusCode = eSIR_SUCCESS;
   tDot11fLinkMeasurementReport frm;
   tANI_U8                      *pFrame;
   tpSirMacMgmtHdr          pMacHdr;
   tANI_U32                      nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus          halstatus;
   tANI_U8             txFlag = 0;


   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Link Report action frame\n") );
      return eSIR_FAILURE;
   }

   palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

   frm.Category.category = SIR_MAC_ACTION_RRM;
   frm.Action.action     = SIR_MAC_RRM_LINK_MEASUREMENT_RPT;
   frm.DialogToken.token = pLinkReport->dialogToken;


   //IEEE Std. 802.11 7.3.2.18. for the report element.
   //Even though TPC report an IE, it is represented using fixed fields since it is positioned
   //in the middle of other fixed fields in the link report frame(IEEE Std. 802.11k section7.4.6.4
   //and frame parser always expects IEs to come after all fixed fields. It is easier to handle 
   //such case this way than changing the frame parser.
   frm.TPCEleID.TPCId = SIR_MAC_TPC_RPT_EID; 
   frm.TPCEleLen.TPCLen = 2;
   frm.TxPower.txPower = pLinkReport->txPower;
   frm.LinkMargin.linkMargin = 0;

   frm.RxAntennaId.antennaId = pLinkReport->rxAntenna;
   frm.TxAntennaId.antennaId = pLinkReport->txAntenna;
   frm.RCPI.rcpi = pLinkReport->rcpi;
   frm.RSNI.rsni = pLinkReport->rsni;

   nStatus = dot11fGetPackedLinkMeasurementReportSize( pMac, &frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Link Report (0x%08x).\n"),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fLinkMeasurementReport );
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating"
               "the packed size for a Link Rep"
               "ort (0x%08x).\n"), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Link "
               "Report.\n"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   palZeroMemory( pMac->hHdd, pFrame, nBytes );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackLinkMeasurementReport( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Link Report (0x%08x).\n" ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
            FL( "There were warnings while packing Link Report (0x%08x).\n" ));
   }

   limLog( pMac, LOGW,
         FL( "Sending a Link Report to \n" ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   if( eHAL_STATUS_SUCCESS !=
         (halstatus = halTxFrame( pMac,
                                  pPacket,
                                  (tANI_U16) nBytes,
                                  HAL_TXRX_FRM_802_11_MGMT,
                                  ANI_TXDIR_TODS,
                                  7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                                  limTxComplete,
                                  pFrame, txFlag )))
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]\n" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return statusCode;
   }
   else
      return eSIR_SUCCESS;

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );

   return statusCode;
} // End limSendLinkReportActionFrame.

/**
 * \brief Send a Beacon Report Action frame
 *
 *
 * \param pMac Pointer to the global MAC structure
 *
 * \param dialog_token dialog token to be used in the action frame.
 *
 * \param num_report number of reports in pRRMReport.
 *
 * \param pRRMReport Address of a tSirMacRadioMeasureReport.
 *
 * \param peer mac address of peer station.
 *
 * \param psessionEntry address of session entry.
 *
 * \return eSIR_SUCCESS on success, eSIR_FAILURE else
 *
 *
 */

tSirRetStatus
limSendRadioMeasureReportActionFrame(tpAniSirGlobal        pMac,
                       tANI_U8              dialog_token,
                       tANI_U8              num_report,
                       tpSirMacRadioMeasureReport pRRMReport,
                       tSirMacAddr                peer,
                       tpPESession psessionEntry
                       )
{
   tSirRetStatus statusCode = eSIR_SUCCESS;
   tDot11fRadioMeasurementReport frm;
   tANI_U8                      *pFrame;
   tpSirMacMgmtHdr          pMacHdr;
   tANI_U32                      nBytes, nPayload, nStatus;
   void               *pPacket;
   eHalStatus          halstatus;
   tANI_U8             i;
   tANI_U8             txFlag = 0;

   if ( psessionEntry == NULL )
   {
      limLog( pMac, LOGE, FL("(psession == NULL) in Request to send Beacon Report action frame\n") );
      return eSIR_FAILURE;
   }
   palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );

   frm.Category.category = SIR_MAC_ACTION_RRM;
   frm.Action.action     = SIR_MAC_RRM_RADIO_MEASURE_RPT;
   frm.DialogToken.token = dialog_token;

   frm.num_MeasurementReport = (num_report > RADIO_REPORTS_MAX_IN_A_FRAME ) ? RADIO_REPORTS_MAX_IN_A_FRAME  : num_report;

   for( i = 0 ; i < frm.num_MeasurementReport ; i++ )
   {
      frm.MeasurementReport[i].type = pRRMReport[i].type;
      frm.MeasurementReport[i].token = pRRMReport[i].token;
      frm.MeasurementReport[i].late = 0; //IEEE 802.11k section 7.3.22. (always zero in rrm)
      switch( pRRMReport[i].type )
      {
         case SIR_MAC_RRM_BEACON_TYPE:
            PopulateDot11fBeaconReport( pMac, &frm.MeasurementReport[i], &pRRMReport[i].report.beaconReport );
            frm.MeasurementReport[i].incapable = pRRMReport[i].incapable;
            frm.MeasurementReport[i].refused = pRRMReport[i].refused;
            frm.MeasurementReport[i].present = 1;
            break;
         default:
            frm.MeasurementReport[i].present = 1;
            break;
      }
   }

   nStatus = dot11fGetPackedRadioMeasurementReportSize( pMac, &frm, &nPayload );
   if ( DOT11F_FAILED( nStatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to calculate the packed size f"
               "or a Radio Measure Report (0x%08x).\n"),
            nStatus );
      // We'll fall back on the worst case scenario:
      nPayload = sizeof( tDot11fLinkMeasurementReport );
      return eSIR_FAILURE;
   }
   else if ( DOT11F_WARNED( nStatus ) )
   {
      limLog( pMac, LOGW, FL("There were warnings while calculating"
               "the packed size for a Radio Measure Rep"
               "ort (0x%08x).\n"), nStatus );
   }

   nBytes = nPayload + sizeof( tSirMacMgmtHdr );

   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( tANI_U16 )nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a Radio Measure "
               "Report.\n"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   palZeroMemory( pMac->hHdd, pFrame, nBytes );

   // Copy necessary info to BD
   if( eSIR_SUCCESS !=
         (statusCode = limPopulateMacHeader( pMac,
                                      pFrame,
                                      SIR_MAC_MGMT_FRAME,
                                      SIR_MAC_MGMT_ACTION,
                                      peer, psessionEntry->selfMacAddr)))
      goto returnAfterError;

   // Update A3 with the BSSID
   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   sirCopyMacAddr( pMacHdr->bssId, psessionEntry->bssId );

   // Now, we're ready to "pack" the frames
   nStatus = dot11fPackRadioMeasurementReport( pMac,
         &frm,
         pFrame + sizeof( tSirMacMgmtHdr ),
         nPayload,
         &nPayload );

   if( DOT11F_FAILED( nStatus ))
   {
      limLog( pMac, LOGE,
            FL( "Failed to pack an Radio Measure Report (0x%08x).\n" ),
            nStatus );

      // FIXME - Need to convert to tSirRetStatus
      statusCode = eSIR_FAILURE;
      goto returnAfterError;
   }
   else if( DOT11F_WARNED( nStatus ))
   {
      limLog( pMac, LOGW,
            FL( "There were warnings while packing Radio Measure Report (0x%08x).\n" ));
   }

   limLog( pMac, LOGW,
         FL( "Sending a Radio Measure Report to \n" ));
   limPrintMacAddr( pMac, peer, LOGW );

    if( ( SIR_BAND_5_GHZ == limGetRFBand(psessionEntry->currentOperChannel))
#ifdef WLAN_FEATURE_P2P
       || ( psessionEntry->pePersona == VOS_P2P_CLIENT_MODE ) ||
         ( psessionEntry->pePersona == VOS_P2P_GO_MODE)
#endif
         )
    {
        txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
    }

   if( eHAL_STATUS_SUCCESS !=
         (halstatus = halTxFrame( pMac,
                                  pPacket,
                                  (tANI_U16) nBytes,
                                  HAL_TXRX_FRM_802_11_MGMT,
                                  ANI_TXDIR_TODS,
                                  7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                                  limTxComplete,
                                  pFrame, txFlag )))
   {
      PELOGE(limLog( pMac, LOGE, FL( "halTxFrame FAILED! Status [%d]\n" ), halstatus );)
         statusCode = eSIR_FAILURE;
      //Pkt will be freed up by the callback
      return statusCode;
   }
   else
      return eSIR_SUCCESS;

returnAfterError:
   palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );

   return statusCode;
} // End limSendBeaconReportActionFrame.

#endif

#ifdef WLAN_FEATURE_11W
/**
 * \brief Send SA query response action frame to peer 
 *
 * \sa limSendSaQueryResponseFrame
 * 
 *
 * \param pMac    The global tpAniSirGlobal object
 *
 * \param peer    The Mac address of the AP to which this action frame is 
addressed
 *
 * \param transId Transaction identifier received in SA query request action 
frame 
 * 
 * \return eSIR_SUCCESS if setup completes successfully
 *         eSIR_FAILURE is some problem is encountered
 */

tSirRetStatus limSendSaQueryResponseFrame( tpAniSirGlobal pMac, tANI_U16 transId,
tSirMacAddr peer,tpPESession psessionEntry)
{

   tDot11wSaQueryRsp  frm; // SA query reponse action frame
   tANI_U8            *pFrame;
   tSirRetStatus      nSirStatus;
   tpSirMacMgmtHdr    pMacHdr;
   tANI_U32           nBytes, nPayload;
   void               *pPacket;
   eHalStatus         halstatus;
   // Local variables used to dump prepared SA query response frame
   tANI_U8            *pDump; 
   tANI_U16           dumpCount;
   tANI_U8             txFlag = 0;
   //tANI_U16 nBytes
   
   palZeroMemory( pMac->hHdd, ( tANI_U8* )&frm, sizeof( frm ) );
   frm.category  = SIR_MAC_ACTION_SA_QUERY;
   /*11w action  fiedl is :
    action: 0 --> SA query request action frame
    action: 1 --> SA query response action frame */ 
   frm.action    = 1;
   /*11w Draft9.0 SA query response transId is same as
     SA query request transId*/
   frm.transId   = transId;

   nPayload = sizeof(tDot11wSaQueryRsp);
   nBytes = nPayload + sizeof( tSirMacMgmtHdr );
   halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,  nBytes, ( void** ) &pFrame, ( void** ) &pPacket );
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGP, FL("Failed to allocate %d bytes for a SA query response"
                               " action frame\n"), nBytes );
      return eSIR_FAILURE;
   }

   // Paranoia:
   palZeroMemory( pMac->hHdd, pFrame, nBytes );

   // Next, we fill out the buffer descriptor:
   nSirStatus = limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_ACTION, peer,psessionEntry->selfMacAddr );
   if ( eSIR_SUCCESS != nSirStatus )
   {
      limLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                              "tor for a TPC Report (%d).\n"),
               nSirStatus );
      palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT, ( void* ) pFrame, ( void* ) pPacket );
      return eSIR_FAILURE;    // just allocated...
   }

   pMacHdr = ( tpSirMacMgmtHdr ) pFrame;

   // Pack 11w SA query response frame
   DOT11F_MEMCPY(pMac, (tANI_U8 *)(pFrame + sizeof(tSirMacMgmtHdr)),(tANI_U8 *)&frm, nPayload);
   pDump = (tANI_U8 *) pFrame;

   halstatus = halTxFrame( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_TODS,
                            7,//SMAC_SWBD_TX_TID_MGMT_HIGH,
                            limTxComplete, pFrame,txFlag);
   if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
   {
      limLog( pMac, LOGE, FL("Failed to send a SA Query resp frame "
                             "(%X)!\n"),halstatus );
        //Pkt will be freed up by the callback
      return eSIR_FAILURE;    // just allocated...
   }

   return eSIR_SUCCESS;
}
#endif
