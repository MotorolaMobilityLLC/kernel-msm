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

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file csrApiRoam.c

    Implementation for the Common Roaming interfaces.
========================================================================== */
/*===========================================================================
                      EDIT HISTORY FOR FILE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  when            who                 what, where, why
----------       ---                --------------------------------------------------------
06/03/10     js                     Added support to hostapd driven
 *                                  deauth/disassoc/mic failure
===========================================================================*/
#include "aniGlobal.h" //for tpAniSirGlobal
#include "wlan_qct_wda.h"
#include "halMsgApi.h" //for HAL_STA_INVALID_IDX.
#include "limUtils.h"
#include "palApi.h"
#include "csrInsideApi.h"
#include "smsDebug.h"
#include "logDump.h"
#include "smeQosInternal.h"
#include "wlan_qct_tl.h"
#include "smeInside.h"
#include "vos_diag_core_event.h"
#include "vos_diag_core_log.h"
#include "csrApi.h"
#include "pmc.h"
#include "vos_nvitem.h"
#include "macTrace.h"
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#include "csrNeighborRoam.h"
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "csrEse.h"
#endif /* FEATURE_WLAN_ESE && !FEATURE_WLAN_ESE_UPLOAD */
#include "regdomain_common.h"
#include "vos_utils.h"

#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2


#define CSR_NUM_IBSS_START_CHANNELS_50      4
#define CSR_NUM_IBSS_START_CHANNELS_24      3
/* 15 seconds, for WPA, WPA2, CCKM */
#define CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD         (15 * VOS_TIMER_TO_SEC_UNIT)
/* 120 seconds, for WPS */
#define CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD     (120 * VOS_TIMER_TO_SEC_UNIT)
/*---------------------------------------------------------------------------
  OBIWAN recommends [8 10]% : pick 9%
---------------------------------------------------------------------------*/
#define CSR_VCC_UL_MAC_LOSS_THRESHOLD 9
/*---------------------------------------------------------------------------
  OBIWAN recommends -85dBm
---------------------------------------------------------------------------*/
#define CSR_VCC_RSSI_THRESHOLD 80
#define CSR_MIN_GLOBAL_STAT_QUERY_PERIOD   500 //ms
#define CSR_MIN_GLOBAL_STAT_QUERY_PERIOD_IN_BMPS 2000 //ms
#define CSR_MIN_TL_STAT_QUERY_PERIOD       500 //ms

//Flag to send/do not send disassoc frame over the air
#define CSR_DONT_SEND_DISASSOC_OVER_THE_AIR 1
#define RSSI_HACK_BMPS (-40)
#define MAX_CB_VALUE_IN_INI (2)

#define MAX_SOCIAL_CHANNELS  3

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
static tANI_BOOLEAN bRoamScanOffloadStarted = VOS_FALSE;
#endif

/*--------------------------------------------------------------------------
  Static Type declarations
  ------------------------------------------------------------------------*/
static tCsrRoamSession csrRoamRoamSession[CSR_ROAM_SESSION_MAX];

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
int diagAuthTypeFromCSRType(eCsrAuthType authType)
{
    int n = AUTH_OPEN;
    switch(authType)
    {
    case eCSR_AUTH_TYPE_SHARED_KEY:
        n = AUTH_SHARED;
        break;
    case eCSR_AUTH_TYPE_WPA:
        n = AUTH_WPA_EAP;
        break;
    case eCSR_AUTH_TYPE_WPA_PSK:
        n = AUTH_WPA_PSK;
        break;
    case eCSR_AUTH_TYPE_RSN:
#ifdef WLAN_FEATURE_11W
    case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
#endif
        n = AUTH_WPA2_EAP;
        break;
    case eCSR_AUTH_TYPE_RSN_PSK:
#ifdef WLAN_FEATURE_11W
    case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
#endif
        n = AUTH_WPA2_PSK;
        break;
#ifdef FEATURE_WLAN_WAPI
    case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
        n = AUTH_WAPI_CERT;
        break;
    case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
        n = AUTH_WAPI_PSK;
        break;
#endif /* FEATURE_WLAN_WAPI */
    default:
        break;
    }
    return (n);
}
int diagEncTypeFromCSRType(eCsrEncryptionType encType)
{
    int n = ENC_MODE_OPEN;
    switch(encType)
    {
    case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
    case eCSR_ENCRYPT_TYPE_WEP40:
        n = ENC_MODE_WEP40;
        break;
    case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
    case eCSR_ENCRYPT_TYPE_WEP104:
        n = ENC_MODE_WEP104;
        break;
    case eCSR_ENCRYPT_TYPE_TKIP:
        n = ENC_MODE_TKIP;
        break;
    case eCSR_ENCRYPT_TYPE_AES:
        n = ENC_MODE_AES;
        break;
#ifdef FEATURE_WLAN_WAPI
    case eCSR_ENCRYPT_TYPE_WPI:
        n = ENC_MODE_SMS4;
        break;
#endif /* FEATURE_WLAN_WAPI */
    default:
        break;
    }
    return (n);
}
#endif //#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static const tANI_U8 csrStartIbssChannels50[ CSR_NUM_IBSS_START_CHANNELS_50 ] = { 36, 40,  44,  48};
static const tANI_U8 csrStartIbssChannels24[ CSR_NUM_IBSS_START_CHANNELS_24 ] = { 1, 6, 11 };
static void initConfigParam(tpAniSirGlobal pMac);
static tANI_BOOLEAN csrRoamProcessResults( tpAniSirGlobal pMac, tSmeCmd *pCommand,
                                       eCsrRoamCompleteResult Result, void *Context );
static eHalStatus csrRoamStartIbss( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    tCsrRoamProfile *pProfile,
                                    tANI_BOOLEAN *pfSameIbss );
static void csrRoamUpdateConnectedProfileFromNewBss( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirSmeNewBssInfo *pNewBss );
static void csrRoamPrepareBssParams(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                     tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig, tDot11fBeaconIEs *pIes);
static ePhyChanBondState csrGetCBModeFromIes(tpAniSirGlobal pMac, tANI_U8 primaryChn, tDot11fBeaconIEs *pIes);
eHalStatus csrInitGetChannels(tpAniSirGlobal pMac);
static void csrRoamingStateConfigCnfProcessor( tpAniSirGlobal pMac, tANI_U32 result );
eHalStatus csrRoamOpen(tpAniSirGlobal pMac);
eHalStatus csrRoamClose(tpAniSirGlobal pMac);
void csrRoamMICErrorTimerHandler(void *pv);
void csrRoamTKIPCounterMeasureTimerHandler(void *pv);
tANI_BOOLEAN csrRoamIsSameProfileKeys(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pConnProfile, tCsrRoamProfile *pProfile2);

static eHalStatus csrRoamStartRoamingTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval);
static eHalStatus csrRoamStopRoamingTimer(tpAniSirGlobal pMac, tANI_U32 sessionId);
static void csrRoamRoamingTimerHandler(void *pv);
eHalStatus csrRoamStartWaitForKeyTimer(tpAniSirGlobal pMac, tANI_U32 interval);
eHalStatus csrRoamStopWaitForKeyTimer(tpAniSirGlobal pMac);
static void csrRoamWaitForKeyTimeOutHandler(void *pv);
static eHalStatus CsrInit11dInfo(tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo);
static eHalStatus csrInitChannelPowerList( tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo);
static eHalStatus csrRoamFreeConnectedInfo( tpAniSirGlobal pMac, tCsrRoamConnectedInfo *pConnectedInfo );
eHalStatus csrSendMBSetContextReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId,
           tSirMacAddr peerMacAddr, tANI_U8 numKeys, tAniEdType edType,
           tANI_BOOLEAN fUnicast, tAniKeyDirection aniKeyDirection,
           tANI_U8 keyId, tANI_U8 keyLength, tANI_U8 *pKey, tANI_U8 paeRole,
           tANI_U8 *pKeyRsc );
static eHalStatus csrRoamIssueReassociate( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes,
                                    tCsrRoamProfile *pProfile );
void csrRoamStatisticsTimerHandler(void *pv);
void csrRoamStatsGlobalClassDTimerHandler(void *pv);
static void csrRoamLinkUp(tpAniSirGlobal pMac, tCsrBssid bssid);
VOS_STATUS csrRoamVccTriggerRssiIndCallback(tHalHandle hHal,
                                            v_U8_t  rssiNotification,
                                            void * context);
static void csrRoamLinkDown(tpAniSirGlobal pMac, tANI_U32 sessionId);
void csrRoamVccTrigger(tpAniSirGlobal pMac);
eHalStatus csrSendMBStatsReqMsg( tpAniSirGlobal pMac, tANI_U32 statsMask,
                                 tANI_U8 staId, tANI_U8 sessionId);
/*
    pStaEntry is no longer invalid upon the return of this function.
*/
static void csrRoamRemoveStatListEntry(tpAniSirGlobal pMac, tListElem *pEntry);
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,tANI_U8 operationChn, eCsrBand *pBand );
static eHalStatus csrRoamGetQosInfoFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBssDesc);
tCsrStatsClientReqInfo * csrRoamInsertEntryIntoList( tpAniSirGlobal pMac,
                                                     tDblLinkList *pStaList,
                                                     tCsrStatsClientReqInfo *pStaEntry);
void csrRoamStatsClientTimerHandler(void *pv);
tCsrPeStatsReqInfo *  csrRoamCheckPeStatsReqList(tpAniSirGlobal pMac, tANI_U32  statsMask,
                                                 tANI_U32 periodicity,
                                                 tANI_BOOLEAN *pFound,
                                                 tANI_U8 staId,
                                                 tANI_U8 sessionId);
void csrRoamReportStatistics(tpAniSirGlobal pMac, tANI_U32 statsMask,
                             tCsrStatsCallback callback, tANI_U8 staId, void *pContext);
void csrRoamSaveStatsFromTl(tpAniSirGlobal pMac, WLANTL_TRANSFER_STA_TYPE *pTlStats);
void csrRoamTlStatsTimerHandler(void *pv);
void csrRoamPeStatsTimerHandler(void *pv);
tListElem * csrRoamCheckClientReqList(tpAniSirGlobal pMac, tANI_U32 statsMask);
void csrRoamRemoveEntryFromPeStatsReqList(tpAniSirGlobal pMac, tCsrPeStatsReqInfo *pPeStaEntry);
tListElem * csrRoamFindInPeStatsReqList(tpAniSirGlobal pMac, tANI_U32  statsMask);
eHalStatus csrRoamDeregStatisticsReq(tpAniSirGlobal pMac);
static tANI_U32 csrFindIbssSession( tpAniSirGlobal pMac );
static uint32_t csr_find_sap_session(tpAniSirGlobal pMac);
static uint32_t csr_find_p2pgo_session(tpAniSirGlobal pMac);
static bool csr_is_conn_allow_2g_band(tpAniSirGlobal pMac, uint32_t chnl);
static bool csr_is_conn_allow_5g_band(tpAniSirGlobal pMac, uint32_t chnl);
static eHalStatus csrRoamStartWds( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tSirBssDescription *pBssDesc );
static void csrInitSession( tpAniSirGlobal pMac, tANI_U32 sessionId );
static eHalStatus csrRoamIssueSetKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                             tCsrRoamSetKey *pSetKey, tANI_U32 roamId );
static eHalStatus csrRoamGetQosInfoFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBssDesc);
void csrRoamReissueRoamCommand(tpAniSirGlobal pMac);
extern void SysProcessMmhMsg(tpAniSirGlobal pMac, tSirMsgQ* pMsg);
static void csrSerDesUnpackDiassocRsp(tANI_U8 *pBuf, tSirSmeDisassocRsp *pRsp);
void csrReinitPreauthCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand);
void csrInitOperatingClasses(tHalHandle hHal);

//Initialize global variables
static void csrRoamInitGlobals(tpAniSirGlobal pMac)
{
    if(pMac)
    {
        vos_mem_zero(&csrRoamRoamSession, sizeof(csrRoamRoamSession));
        pMac->roam.roamSession = csrRoamRoamSession;
    }
    return;
}

static void csrRoamDeInitGlobals(tpAniSirGlobal pMac)
{
    if(pMac)
    {
        pMac->roam.roamSession = NULL;
    }
    return;
}
eHalStatus csrOpen(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 i;

    do
    {
        /* Initialize CSR Roam Globals */
        csrRoamInitGlobals(pMac);
        for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
           csrRoamStateChange( pMac, eCSR_ROAMING_STATE_STOP, i);

        initConfigParam(pMac);
        if(!HAL_STATUS_SUCCESS((status = csrScanOpen(pMac))))
            break;
        if(!HAL_STATUS_SUCCESS((status = csrRoamOpen(pMac))))
            break;
        pMac->roam.nextRoamId = 1;  //Must not be 0
        if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &pMac->roam.statsClientReqList)))
           break;
        if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &pMac->roam.peStatsReqList)))
           break;
        if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &pMac->roam.roamCmdPendingList)))
           break;
    }while(0);

    return (status);
}

eHalStatus csr_init_chan_list(tpAniSirGlobal mac, v_U8_t *alpha2)
{
    eHalStatus status;
    v_REGDOMAIN_t reg_id;
    v_CountryInfoSource_t source = COUNTRY_INIT;

    mac->scan.countryCodeDefault[0] = alpha2[0];
    mac->scan.countryCodeDefault[1] = alpha2[1];
    mac->scan.countryCodeDefault[2] = alpha2[2];

    smsLog(mac, LOGE, FL("init time country code %.2s"),
           mac->scan.countryCodeDefault);

    status = csrGetRegulatoryDomainForCountry(mac,
                                              mac->scan.countryCodeDefault,
                                              &reg_id, source);
    if (status != eHAL_STATUS_SUCCESS)
    {
       smsLog(mac, LOGE, FL("csrGetRegulatoryDomainForCountry failed"));
       return status;
    }

    if (vos_nv_setRegDomain(mac, reg_id, FALSE) != VOS_STATUS_SUCCESS)
    {
       smsLog(mac, LOGE, FL("vos_nv_setRegDomain failed"));
       return eHAL_STATUS_FAILURE;
    }
    mac->scan.domainIdDefault = reg_id;
    mac->scan.domainIdCurrent = mac->scan.domainIdDefault;
    vos_mem_copy(mac->scan.countryCodeCurrent,
                 mac->scan.countryCodeDefault,
                 WNI_CFG_COUNTRY_CODE_LEN);
    vos_mem_copy(mac->scan.countryCodeElected,
                 mac->scan.countryCodeDefault,
                 WNI_CFG_COUNTRY_CODE_LEN);
    status = csrInitGetChannels(mac);
    csrClearVotesForCountryInfo(mac);
    return status;
}

eHalStatus csrSetRegInfo(tHalHandle hHal,  tANI_U8 *apCntryCode)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_REGDOMAIN_t regId;
    v_U8_t        cntryCodeLength;
    if(NULL == apCntryCode)
    {
       smsLog( pMac, LOGE, FL(" Invalid country Code Pointer") );
       return eHAL_STATUS_FAILURE;
    }
    smsLog( pMac, LOG1, FL(" country Code %.2s"), apCntryCode );
    /*
     * To get correct Regulatory domain from NV table
     * 2 character Country code should be used
     * 3rd character is optional for indoor/outdoor setting
     */
    cntryCodeLength = WNI_CFG_COUNTRY_CODE_LEN;
    status = csrGetRegulatoryDomainForCountry(pMac, apCntryCode, &regId,
                                              COUNTRY_USER);
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog( pMac, LOGE, FL("  fail to get regId for country Code %.2s"), apCntryCode );
        return status;
    }
    status = WDA_SetRegDomain(hHal, regId, eSIR_TRUE);
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog( pMac, LOGE, FL("  fail to get regId for country Code %.2s"), apCntryCode );
        return status;
    }
    pMac->scan.domainIdDefault = regId;
    pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;
    /* Clear CC field */
    vos_mem_set(pMac->scan.countryCodeDefault, WNI_CFG_COUNTRY_CODE_LEN, 0);

    /* Copy 2 or 3 bytes country code */
    vos_mem_copy(pMac->scan.countryCodeDefault, apCntryCode, cntryCodeLength);

    /* If 2 bytes country code, 3rd byte must be filled with space */
    if((WNI_CFG_COUNTRY_CODE_LEN - 1) == cntryCodeLength)
    {
        vos_mem_set(pMac->scan.countryCodeDefault + 2, 1, 0x20);
    }
    vos_mem_copy(pMac->scan.countryCodeCurrent, pMac->scan.countryCodeDefault,
                 WNI_CFG_COUNTRY_CODE_LEN);
    status = csrInitGetChannels( pMac );
    return status;
}
eHalStatus csrSetChannels(tHalHandle hHal,  tCsrConfigParam *pParam  )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U8   index = 0;
    vos_mem_copy(pParam->Csr11dinfo.countryCode, pMac->scan.countryCodeCurrent,
                 WNI_CFG_COUNTRY_CODE_LEN);
    for ( index = 0; index < pMac->scan.base20MHzChannels.numChannels ; index++)
    {
        pParam->Csr11dinfo.Channels.channelList[index] = pMac->scan.base20MHzChannels.channelList[ index ];
        pParam->Csr11dinfo.ChnPower[index].firstChannel = pMac->scan.base20MHzChannels.channelList[ index ];
        pParam->Csr11dinfo.ChnPower[index].numChannels = 1;
        pParam->Csr11dinfo.ChnPower[index].maxtxPower = pMac->scan.defaultPowerTable[index].pwr;
    }
    pParam->Csr11dinfo.Channels.numChannels = pMac->scan.base20MHzChannels.numChannels;

    return status;
}
eHalStatus csrClose(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    csrRoamClose(pMac);
    csrScanClose(pMac);
    csrLLClose(&pMac->roam.statsClientReqList);
    csrLLClose(&pMac->roam.peStatsReqList);
    csrLLClose(&pMac->roam.roamCmdPendingList);
    /* DeInit Globals */
    csrRoamDeInitGlobals(pMac);
    return (status);
}

static tChannelPwrLimit csrFindChannelPwr(tChannelListWithPower * pdefaultPowerTable,
                                 tANI_U8 ChannelNum)
{
    tANI_U8 i;
    // TODO: if defaultPowerTable is guaranteed to be in ascending
    // order of channel numbers, we can employ binary search
    for (i = 0; i < WNI_CFG_VALID_CHANNEL_LIST_LEN; i++)
    {
       if (pdefaultPowerTable[i].chanId == ChannelNum)
           return pdefaultPowerTable[i].pwr;
    }
    /* Could not find the channel list in default list
       this should not have occurred */
    VOS_ASSERT(0);
    return 0;
}

eHalStatus csrUpdateChannelList(tpAniSirGlobal pMac)
{
    tSirUpdateChanList *pChanList;
    tCsrScanStruct *pScan = &pMac->scan;
    tANI_U8 numChan = pScan->base20MHzChannels.numChannels;
    tANI_U8 num_channel = 0;
    tANI_U32 bufLen;
    vos_msg_t msg;
    tANI_U8 i, j, social_channel[MAX_SOCIAL_CHANNELS] = {1,6,11};
    tANI_U8 channel_state;

    if (CSR_IS_5G_BAND_ONLY(pMac))
    {
        for (i = 0; i < MAX_SOCIAL_CHANNELS; i++)
        {
            /* Scan is not performed on DSRC channels*/
            if (pScan->baseChannels.channelList[i] >= MIN_11P_CHANNEL)
                continue;
            if (vos_nv_getChannelEnabledState(social_channel[i])
                == NV_CHANNEL_ENABLE)
                numChan++;
        }
    }

    bufLen = sizeof(tSirUpdateChanList) +
        (sizeof(tSirUpdateChanParam) * (numChan));

    csrInitOperatingClasses((tHalHandle)pMac);
    pChanList = (tSirUpdateChanList *) vos_mem_malloc(bufLen);
    if (!pChanList)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "Failed to allocate memory for tSirUpdateChanList");
        return eHAL_STATUS_FAILED_ALLOC;
    }
    vos_mem_zero(pChanList, bufLen);

    for (i = 0; i < pScan->base20MHzChannels.numChannels; i++)
    {
        channel_state =
            vos_nv_getChannelEnabledState(
                pScan->base20MHzChannels.channelList[i]);
        if ((NV_CHANNEL_ENABLE == channel_state) ||
            pMac->scan.fEnableDFSChnlScan)
        {
            pChanList->chanParam[num_channel].chanId =
                pScan->base20MHzChannels.channelList[i];
            pChanList->chanParam[num_channel].pwr =
                csrFindChannelPwr(pScan->defaultPowerTable,
                                  pChanList->chanParam[num_channel].chanId);

            if (pScan->fcc_constraint) {
                if (pChanList->chanParam[num_channel].chanId == 12) {
                    pChanList->chanParam[num_channel].pwr =
                                        MAX_PWR_FCC_CHAN_12;
                    smsLog(pMac, LOG1,
                     "fcc_constraint is set, txpower for channel 12 is 8db");
                }
                if (pChanList->chanParam[num_channel].chanId == 13) {
                    pChanList->chanParam[num_channel].pwr =
                                        MAX_PWR_FCC_CHAN_13;
                    smsLog(pMac, LOG1,
                     "fcc_constraint is set, txpower for channel 13 is 2db");
                }
            }

            if (NV_CHANNEL_ENABLE == channel_state)
                pChanList->chanParam[num_channel].dfsSet = VOS_FALSE;
            else
                pChanList->chanParam[num_channel].dfsSet = VOS_TRUE;
            num_channel++;
        }
    }


    if (CSR_IS_5G_BAND_ONLY(pMac))
    {
        for (j = 0; j < MAX_SOCIAL_CHANNELS; j++)
        {
            if (vos_nv_getChannelEnabledState(social_channel[j])
                == NV_CHANNEL_ENABLE)
            {
                pChanList->chanParam[num_channel].chanId = social_channel[j];
                pChanList->chanParam[num_channel].pwr =
                    csrFindChannelPwr(pScan->defaultPowerTable,
                                      social_channel[j]);
                pChanList->chanParam[num_channel].dfsSet = VOS_FALSE;
                num_channel++;
            }
        }
    }

    if ((pMac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
                    (pMac->roam.configParam.uCfgDot11Mode ==
                     eCSR_CFG_DOT11_MODE_11AC) ||
                    (pMac->roam.configParam.uCfgDot11Mode ==
                     eCSR_CFG_DOT11_MODE_11AC_ONLY)) {
            pChanList->vht_en = true;
            if (pMac->roam.configParam.enableVhtFor24GHz)
                pChanList->vht_24_en = true;
    }
    if ((pMac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
                    (pMac->roam.configParam.uCfgDot11Mode ==
                     eCSR_CFG_DOT11_MODE_11N) ||
                    (pMac->roam.configParam.uCfgDot11Mode ==
                     eCSR_CFG_DOT11_MODE_11N_ONLY)) {
            pChanList->ht_en = true;
    }
    msg.type = WDA_UPDATE_CHAN_LIST_REQ;
    msg.reserved = 0;
    msg.bodyptr = pChanList;
    pChanList->numChan = num_channel;

    if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to post msg to WDA", __func__);
        vos_mem_free(pChanList);
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

eHalStatus csrStart(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 i;

    do
    {
       //save the global vos context
        pMac->roam.gVosContext = vos_get_global_context(VOS_MODULE_ID_SME, pMac);
        for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
           csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, i );

        status = csrRoamStart(pMac);
        if(!HAL_STATUS_SUCCESS(status)) break;
        pMac->scan.f11dInfoApplied = eANI_BOOLEAN_FALSE;

        if(!pMac->psOffloadEnabled)
        {
            status = pmcRegisterPowerSaveCheck(pMac, csrCheckPSReady, pMac);
            if(!HAL_STATUS_SUCCESS(status)) break;
        }
        else
        {
            for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
            {
                status = pmcOffloadRegisterPowerSaveCheck(pMac, i,
                                    csrCheckPSOffloadReady, pMac);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGE,
                    "csrStart: Register Power Check Failed Session Id %x", i);
                    return status;
                }
            }
        }
        pMac->roam.sPendingCommands = 0;
        csrScanEnable(pMac);
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
        for (i = 0; i < CSR_ROAM_SESSION_MAX; i++)
            status = csrNeighborRoamInit(pMac, i);
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */
        pMac->roam.tlStatsReqInfo.numClient = 0;
        pMac->roam.tlStatsReqInfo.periodicity = 0;
        pMac->roam.tlStatsReqInfo.timerRunning = FALSE;
        //init the link quality indication also
        pMac->roam.vccLinkQuality = eCSR_ROAM_LINK_QUAL_MIN_IND;
        if(!HAL_STATUS_SUCCESS(status))
        {
           smsLog(pMac, LOGW, " csrStart: Couldn't Init HO control blk ");
           break;
        }

        if (pMac->fScanOffload)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                    "Scan offload is enabled, update default chan list");
            status = csrUpdateChannelList(pMac);
        }

    }while(0);
#if defined(ANI_LOGDUMP)
    csrDumpInit(pMac);
#endif //#if defined(ANI_LOGDUMP)
    return (status);
}

eHalStatus csrStop(tpAniSirGlobal pMac, tHalStopType stopType)
{
    tANI_U32 sessionId;

    for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
    {
        csrRoamCloseSession(pMac, sessionId, TRUE, NULL, NULL);
    }
    csrScanDisable(pMac);
    pMac->scan.fCancelIdleScan = eANI_BOOLEAN_FALSE;
    pMac->scan.fRestartIdleScan = eANI_BOOLEAN_FALSE;
    csrLLPurge( &pMac->roam.roamCmdPendingList, eANI_BOOLEAN_TRUE );

#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
        csrNeighborRoamClose(pMac, sessionId);
#endif
    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
        if (CSR_IS_SESSION_VALID(pMac, sessionId))
            csrScanFlushResult(pMac, sessionId);

    // deregister from PMC since we register during csrStart()
    // (ignore status since there is nothing we can do if it fails)
    if(!pMac->psOffloadEnabled)
    {
        (void) pmcDeregisterPowerSaveCheck(pMac, csrCheckPSReady);
    }
    else
    {
        for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
        {
            pmcOffloadDeregisterPowerSaveCheck(pMac, sessionId,
                                               csrCheckPSOffloadReady);
        }
    }

    /* Reset the domain back to the default */
    pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;

    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++) {
       csrRoamStateChange(pMac, eCSR_ROAMING_STATE_STOP, sessionId);
       pMac->roam.curSubState[sessionId] = eCSR_ROAM_SUBSTATE_NONE;
    }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    /* When HAL resets all the context information
     * in HAL is lost, so we might need to send the
     * scan offload request again when it comes
     * out of reset for scan offload to be functional
     */
    if (HAL_STOP_TYPE_SYS_RESET == stopType)
    {
       bRoamScanOffloadStarted = VOS_FALSE;
    }
#endif

    return (eHAL_STATUS_SUCCESS);
}

eHalStatus csrReady(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    csrScanGetSupportedChannels( pMac );
    //WNI_CFG_VALID_CHANNEL_LIST should be set by this time
    //use it to init the background scan list
    csrInitBGScanChannelList(pMac);
    status = csrInitChannelList( pMac );
    if ( ! HAL_STATUS_SUCCESS( status ) )
    {
       smsLog( pMac, LOGE, "csrInitChannelList failed during csrReady with status=%d",
               status );
    }
    return (status);
}
void csrSetDefaultDot11Mode( tpAniSirGlobal pMac )
{
    v_U32_t wniDot11mode = 0;
    wniDot11mode = csrTranslateToWNICfgDot11Mode(pMac,pMac->roam.configParam.uCfgDot11Mode);
    ccmCfgSetInt(pMac, WNI_CFG_DOT11_MODE, wniDot11mode, NULL, eANI_BOOLEAN_FALSE);
}
void csrSetGlobalCfgs( tpAniSirGlobal pMac )
{

    ccmCfgSetInt(pMac, WNI_CFG_FRAGMENTATION_THRESHOLD, csrGetFragThresh(pMac), NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_RTS_THRESHOLD, csrGetRTSThresh(pMac), NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_11D_ENABLED,
                        ((pMac->roam.configParam.Is11hSupportEnabled) ? pMac->roam.configParam.Is11dSupportEnabled : pMac->roam.configParam.Is11dSupportEnabled),
                        NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_11H_ENABLED, pMac->roam.configParam.Is11hSupportEnabled, NULL, eANI_BOOLEAN_FALSE);
    /* For now we will just use the 5GHz CB mode ini parameter to decide whether CB supported or not in Probes when there is no session
     * Once session is established we will use the session related params stored in PE session for CB mode
     */
    ccmCfgSetInt(pMac, WNI_CFG_CHANNEL_BONDING_MODE, !!(pMac->roam.configParam.channelBondingMode5GHz), NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, pMac->roam.configParam.HeartbeatThresh24, NULL, eANI_BOOLEAN_FALSE);

    //Update the operating mode to configured value during initialization,
    //So that client can advertise full capabilities in Probe request frame.
    csrSetDefaultDot11Mode( pMac );
}

eHalStatus csrRoamOpen(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 i;
    tCsrRoamSession *pSession;
    do
    {
        for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
        {
            pSession = CSR_GET_SESSION( pMac, i );
            pSession->roamingTimerInfo.pMac = pMac;
            pSession->roamingTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
        }
        pMac->roam.WaitForKeyTimerInfo.pMac = pMac;
        pMac->roam.WaitForKeyTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
        status = vos_timer_init(&pMac->roam.hTimerWaitForKey, VOS_TIMER_TYPE_SW,
                                csrRoamWaitForKeyTimeOutHandler,
                                &pMac->roam.WaitForKeyTimerInfo);
      if (!HAL_STATUS_SUCCESS(status))
      {
        smsLog(pMac, LOGE, FL("cannot allocate memory for WaitForKey time out timer"));
        break;
      }
      status = vos_timer_init(&pMac->roam.tlStatsReqInfo.hTlStatsTimer,
                              VOS_TIMER_TYPE_SW, csrRoamTlStatsTimerHandler, pMac);
      if (!HAL_STATUS_SUCCESS(status))
      {
         smsLog(pMac, LOGE, FL("cannot allocate memory for summary Statistics timer"));
         return eHAL_STATUS_FAILURE;
      }
    }while (0);
    return (status);
}

eHalStatus csrRoamClose(tpAniSirGlobal pMac)
{
    tANI_U32 sessionId;
    for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
    {
        csrRoamCloseSession(pMac, sessionId, TRUE, NULL, NULL);
    }
    vos_timer_stop(&pMac->roam.hTimerWaitForKey);
    vos_timer_destroy(&pMac->roam.hTimerWaitForKey);
    vos_timer_stop(&pMac->roam.tlStatsReqInfo.hTlStatsTimer);
    vos_timer_destroy(&pMac->roam.tlStatsReqInfo.hTlStatsTimer);
    return (eHAL_STATUS_SUCCESS);
}

eHalStatus csrRoamStart(tpAniSirGlobal pMac)
{
    (void)pMac;
    return (eHAL_STATUS_SUCCESS);
}

void csrRoamStop(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
   csrRoamStopRoamingTimer(pMac, sessionId);
   /* deregister the clients requesting stats from PE/TL & also stop the corresponding timers*/
   csrRoamDeregStatisticsReq(pMac);
}
eHalStatus csrRoamGetConnectState(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrConnectState *pState)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    if ( CSR_IS_SESSION_VALID(pMac, sessionId) && (NULL != pState) )
    {
        status = eHAL_STATUS_SUCCESS;
        *pState = pMac->roam.roamSession[sessionId].connectState;
    }
    return (status);
}

eHalStatus csrRoamCopyConnectProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamConnectedProfile *pProfile)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tANI_U32 size = 0;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pProfile)
    {
        if(pSession->pConnectBssDesc)
        {
            do
            {
                size = pSession->pConnectBssDesc->length + sizeof(pSession->pConnectBssDesc->length);
                if(size)
                {
                    pProfile->pBssDesc = vos_mem_malloc(size);
                    if ( NULL != pProfile->pBssDesc )
                    {
                        vos_mem_copy(pProfile->pBssDesc,
                                     pSession->pConnectBssDesc, size);
                        status = eHAL_STATUS_SUCCESS;
                    }
                    else
                        break;
                }
                else
                {
                    pProfile->pBssDesc = NULL;
                }
                pProfile->AuthType = pSession->connectedProfile.AuthType;
                pProfile->EncryptionType = pSession->connectedProfile.EncryptionType;
                pProfile->mcEncryptionType = pSession->connectedProfile.mcEncryptionType;
                pProfile->BSSType = pSession->connectedProfile.BSSType;
                pProfile->operationChannel = pSession->connectedProfile.operationChannel;
                pProfile->CBMode = pSession->connectedProfile.CBMode;
                vos_mem_copy(&pProfile->bssid, &pSession->connectedProfile.bssid,
                             sizeof(tCsrBssid));
                vos_mem_copy(&pProfile->SSID, &pSession->connectedProfile.SSID,
                             sizeof(tSirMacSSid));
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (pSession->connectedProfile.MDID.mdiePresent)
                {
                    pProfile->MDID.mdiePresent = 1;
                    pProfile->MDID.mobilityDomain = pSession->connectedProfile.MDID.mobilityDomain;
                }
                else
                {
                    pProfile->MDID.mdiePresent = 0;
                    pProfile->MDID.mobilityDomain = 0;
                }
#endif
#ifdef FEATURE_WLAN_ESE
                pProfile->isESEAssoc = pSession->connectedProfile.isESEAssoc;
                if (csrIsAuthTypeESE(pSession->connectedProfile.AuthType))
                {
                    vos_mem_copy (pProfile->eseCckmInfo.krk,
                                  pSession->connectedProfile.eseCckmInfo.krk,
                                  SIR_KRK_KEY_LEN);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
                    vos_mem_copy (pProfile->eseCckmInfo.btk,
                                  pSession->connectedProfile.eseCckmInfo.btk,
                                  SIR_BTK_KEY_LEN);
#endif
                    pProfile->eseCckmInfo.reassoc_req_num=
                        pSession->connectedProfile.eseCckmInfo.reassoc_req_num;
                    pProfile->eseCckmInfo.krk_plumbed =
                        pSession->connectedProfile.eseCckmInfo.krk_plumbed;
                }
#endif
            }while(0);
        }
    }

    return (status);
}

eHalStatus csrRoamGetConnectProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamConnectedProfile *pProfile)
{
    eHalStatus status = eHAL_STATUS_FAILURE;

    if((csrIsConnStateConnected(pMac, sessionId)) ||
       (csrIsConnStateIbss(pMac, sessionId)))
    {
        if(pProfile)
        {
            status = csrRoamCopyConnectProfile(pMac, sessionId, pProfile);
        }
    }
    return (status);
}

eHalStatus csrRoamFreeConnectProfile(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pProfile)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    if (pProfile->pBssDesc)
    {
        vos_mem_free(pProfile->pBssDesc);
    }
    if (pProfile->pAddIEAssoc)
    {
        vos_mem_free(pProfile->pAddIEAssoc);
    }
    vos_mem_set(pProfile, sizeof(tCsrRoamConnectedProfile), 0);

    pProfile->AuthType = eCSR_AUTH_TYPE_UNKNOWN;
    return (status);
}

static eHalStatus csrRoamFreeConnectedInfo( tpAniSirGlobal pMac, tCsrRoamConnectedInfo *pConnectedInfo )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    if( pConnectedInfo->pbFrames )
    {
        vos_mem_free(pConnectedInfo->pbFrames);
        pConnectedInfo->pbFrames = NULL;
    }
    pConnectedInfo->nBeaconLength = 0;
    pConnectedInfo->nAssocReqLength = 0;
    pConnectedInfo->nAssocRspLength = 0;
    pConnectedInfo->staId = 0;
#ifdef WLAN_FEATURE_VOWIFI_11R
    pConnectedInfo->nRICRspLength = 0;
#endif
#ifdef FEATURE_WLAN_ESE
    pConnectedInfo->nTspecIeLength = 0;
#endif
    return ( status );
}




void csrReleaseCommandPreauth(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitPreauthCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}

void csrReleaseCommandRoam(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitRoamCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}

void csrReleaseCommandScan(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitScanCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}

void csrReleaseCommandWmStatusChange(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitWmStatusChangeCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}

void csrReinitSetKeyCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    vos_mem_set(&pCommand->u.setKeyCmd, sizeof(tSetKeyCmd), 0);
}

void csrReinitRemoveKeyCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    vos_mem_set(&pCommand->u.removeKeyCmd, sizeof(tRemoveKeyCmd), 0);
}

void csrReleaseCommandSetKey(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitSetKeyCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}
void csrReleaseCommandRemoveKey(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    csrReinitRemoveKeyCmd(pMac, pCommand);
    csrReleaseCommand( pMac, pCommand );
}
void csrAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping )
{

    if( eSmeCsrCommandMask & pCommand->command )
    {
        switch (pCommand->command)
        {
        case eSmeCommandScan:
            // We need to inform the requester before dropping the scan command
            smsLog( pMac, LOGW, "%s: Drop scan reason %d callback %p",
                    __func__, pCommand->u.scanCmd.reason,
                    pCommand->u.scanCmd.callback);
            if (NULL != pCommand->u.scanCmd.callback)
            {
                smsLog( pMac, LOGW, "%s callback scan requester", __func__);
                csrScanCallCallback(pMac, pCommand, eCSR_SCAN_ABORT);
            }
            csrReleaseCommandScan( pMac, pCommand );
            break;
        case eSmeCommandRoam:
            csrReleaseCommandRoam( pMac, pCommand );
            break;

        case eSmeCommandWmStatusChange:
            csrReleaseCommandWmStatusChange( pMac, pCommand );
            break;

        case eSmeCommandSetKey:
            csrReleaseCommandSetKey( pMac, pCommand );
            break;

        case eSmeCommandRemoveKey:
            csrReleaseCommandRemoveKey( pMac, pCommand );
            break;

    default:
            smsLog( pMac, LOGW, " CSR abort standard command %d", pCommand->command );
            csrReleaseCommand( pMac, pCommand );
            break;
        }
    }
}

void csrRoamSubstateChange( tpAniSirGlobal pMac, eCsrRoamSubState NewSubstate, tANI_U32 sessionId)
{
     smsLog(pMac, LOG1, FL("CSR RoamSubstate: [ %s <== %s ]"),
                           macTraceGetcsrRoamSubState(NewSubstate),
                           macTraceGetcsrRoamSubState(pMac->roam.curSubState[sessionId]));
    if(pMac->roam.curSubState[sessionId] == NewSubstate)
    {
       return;
    }
    pMac->roam.curSubState[sessionId] = NewSubstate;
}

eCsrRoamState csrRoamStateChange( tpAniSirGlobal pMac, eCsrRoamState NewRoamState, tANI_U8 sessionId)
{
    eCsrRoamState PreviousState;

    smsLog(pMac, LOG1, FL("CSR RoamState[%hu]: [ %s <== %s ]"), sessionId,
                          macTraceGetcsrRoamState(NewRoamState),
                          macTraceGetcsrRoamState(pMac->roam.curState[sessionId]));
    PreviousState = pMac->roam.curState[sessionId];

    if ( NewRoamState != pMac->roam.curState[sessionId] )
    {
        // Whenever we transition OUT of the Roaming state, clear the Roaming substate...
        if ( CSR_IS_ROAM_JOINING(pMac, sessionId) )
        {
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
        }

        pMac->roam.curState[sessionId] = NewRoamState;
    }
    return( PreviousState );
}

void csrAssignRssiForCategory(tpAniSirGlobal pMac, tANI_S8 bestApRssi, tANI_U8 catOffset)
{
    int i;

    smsLog(pMac, LOG2, FL("best AP RSSI:%d, cat offset:%d"), bestApRssi,
           catOffset);
    if(catOffset)
    {
        pMac->roam.configParam.bCatRssiOffset = catOffset;
        for(i = 0; i < CSR_NUM_RSSI_CAT; i++)
        {
            pMac->roam.configParam.RSSICat[CSR_NUM_RSSI_CAT - i - 1] = (int)bestApRssi - pMac->roam.configParam.nSelect5GHzMargin - (int)(i * catOffset);
        }
    }
}

static void initConfigParam(tpAniSirGlobal pMac)
{
    int i;
    pMac->roam.configParam.agingCount = CSR_AGING_COUNT;
    pMac->roam.configParam.channelBondingMode24GHz = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
    pMac->roam.configParam.channelBondingMode5GHz = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

    pMac->roam.configParam.phyMode = eCSR_DOT11_MODE_AUTO;
    pMac->roam.configParam.eBand = eCSR_BAND_ALL;
    pMac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
    pMac->roam.configParam.FragmentationThreshold = eCSR_DOT11_FRAG_THRESH_DEFAULT;
    pMac->roam.configParam.HeartbeatThresh24 = 40;
    pMac->roam.configParam.HeartbeatThresh50 = 40;
    pMac->roam.configParam.Is11dSupportEnabled = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.Is11dSupportEnabledOriginal = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.Is11eSupportEnabled = eANI_BOOLEAN_TRUE;
    pMac->roam.configParam.Is11hSupportEnabled = eANI_BOOLEAN_TRUE;
    pMac->roam.configParam.RTSThreshold = 2346;
    pMac->roam.configParam.shortSlotTime = eANI_BOOLEAN_TRUE;
    pMac->roam.configParam.WMMSupportMode = eCsrRoamWmmAuto;
    pMac->roam.configParam.ProprietaryRatesEnabled = eANI_BOOLEAN_TRUE;
    pMac->roam.configParam.TxRate = eCSR_TX_RATE_AUTO;
    pMac->roam.configParam.impsSleepTime = CSR_IDLE_SCAN_NO_PS_INTERVAL;
    pMac->roam.configParam.scanAgeTimeNCNPS = CSR_SCAN_AGING_TIME_NOT_CONNECT_NO_PS;
    pMac->roam.configParam.scanAgeTimeNCPS = CSR_SCAN_AGING_TIME_NOT_CONNECT_W_PS;
    pMac->roam.configParam.scanAgeTimeCNPS = CSR_SCAN_AGING_TIME_CONNECT_NO_PS;
    pMac->roam.configParam.scanAgeTimeCPS = CSR_SCAN_AGING_TIME_CONNECT_W_PS;
    for(i = 0; i < CSR_NUM_RSSI_CAT; i++)
    {
        pMac->roam.configParam.BssPreferValue[i] = i;
    }
    csrAssignRssiForCategory(pMac, CSR_BEST_RSSI_VALUE, CSR_DEFAULT_RSSI_DB_GAP);
    pMac->roam.configParam.nRoamingTime = CSR_DEFAULT_ROAMING_TIME;
    pMac->roam.configParam.fEnforce11dChannels = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fSupplicantCountryCodeHasPriority = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fEnforceCountryCodeMatch = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fEnforceDefaultDomain = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.nActiveMaxChnTime = CSR_ACTIVE_MAX_CHANNEL_TIME;
    pMac->roam.configParam.nActiveMinChnTime = CSR_ACTIVE_MIN_CHANNEL_TIME;
    pMac->roam.configParam.nPassiveMaxChnTime = CSR_PASSIVE_MAX_CHANNEL_TIME;
    pMac->roam.configParam.nPassiveMinChnTime = CSR_PASSIVE_MIN_CHANNEL_TIME;
    pMac->roam.configParam.nActiveMaxChnTimeBtc = CSR_ACTIVE_MAX_CHANNEL_TIME_BTC;
    pMac->roam.configParam.nActiveMinChnTimeBtc = CSR_ACTIVE_MIN_CHANNEL_TIME_BTC;
    pMac->roam.configParam.disableAggWithBtc = eANI_BOOLEAN_TRUE;
#ifdef WLAN_AP_STA_CONCURRENCY
    pMac->roam.configParam.nActiveMaxChnTimeConc = CSR_ACTIVE_MAX_CHANNEL_TIME_CONC;
    pMac->roam.configParam.nActiveMinChnTimeConc = CSR_ACTIVE_MIN_CHANNEL_TIME_CONC;
    pMac->roam.configParam.nPassiveMaxChnTimeConc = CSR_PASSIVE_MAX_CHANNEL_TIME_CONC;
    pMac->roam.configParam.nPassiveMinChnTimeConc = CSR_PASSIVE_MIN_CHANNEL_TIME_CONC;
    pMac->roam.configParam.nRestTimeConc = CSR_REST_TIME_CONC;
    pMac->roam.configParam.min_rest_time_conc =  CSR_MIN_REST_TIME_CONC;
    pMac->roam.configParam.idle_time_conc = CSR_IDLE_TIME_CONC;
    pMac->roam.configParam.nNumStaChanCombinedConc = CSR_NUM_STA_CHAN_COMBINED_CONC;
    pMac->roam.configParam.nNumP2PChanCombinedConc = CSR_NUM_P2P_CHAN_COMBINED_CONC;
#endif
    pMac->roam.configParam.IsIdleScanEnabled = TRUE; //enable the idle scan by default
    pMac->roam.configParam.nTxPowerCap = CSR_MAX_TX_POWER;
    pMac->roam.configParam.statsReqPeriodicity = CSR_MIN_GLOBAL_STAT_QUERY_PERIOD;
    pMac->roam.configParam.statsReqPeriodicityInPS = CSR_MIN_GLOBAL_STAT_QUERY_PERIOD_IN_BMPS;
#ifdef WLAN_FEATURE_VOWIFI_11R
    pMac->roam.configParam.csr11rConfig.IsFTResourceReqSupported = 0;
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    pMac->roam.configParam.neighborRoamConfig.nMaxNeighborRetries = 3;
    pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold = 120;
    pMac->roam.configParam.neighborRoamConfig.nOpportunisticThresholdDiff = 30;
    pMac->roam.configParam.neighborRoamConfig.nRoamRescanRssiDiff = 5;
    pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold = 125;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime = 20;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime = 40;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod = 200;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels = 3;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[0] = 1;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[1] = 6;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[2] = 11;
    pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod = 20000; //20 seconds
    pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod = 0;
    pMac->roam.configParam.neighborRoamConfig.nRoamBmissFirstBcnt = 10;
    pMac->roam.configParam.neighborRoamConfig.nRoamBmissFinalBcnt = 10;
    pMac->roam.configParam.neighborRoamConfig.nRoamBeaconRssiWeight = 14;
#endif
#ifdef WLAN_FEATURE_11AC
     pMac->roam.configParam.nVhtChannelWidth = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;
#endif

    pMac->roam.configParam.addTSWhenACMIsOff = 0;
    pMac->roam.configParam.fScanTwice = eANI_BOOLEAN_FALSE;

    //Remove this code once SLM_Sessionization is supported
    //BMPS_WORKAROUND_NOT_NEEDED
    pMac->roam.configParam.doBMPSWorkaround = 0;

    pMac->roam.configParam.nInitialDwellTime = 0;
    pMac->roam.configParam.initial_scan_no_dfs_chnl = 0;
}
eCsrBand csrGetCurrentBand(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.bandCapability;
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/*
 This function flushes the roam scan cache
*/
eHalStatus csrFlushRoamScanRoamChannelList(tpAniSirGlobal pMac,
                                           tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo
      = &pMac->roam.neighborRoamInfo[sessionId];
    /* Free up the memory first (if required) */
    if (NULL !=
      pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
    {
        vos_mem_free(
          pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList
          );
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList
          = NULL;
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels
          = 0;
    }
    return status;
}
#endif  /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
/*
 This function flushes the roam scan cache
*/
eHalStatus csrFlushCfgBgScanRoamChannelList(tpAniSirGlobal pMac,
                                            tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
                                        &pMac->roam.neighborRoamInfo[sessionId];

    /* Free up the memory first (if required) */
    if (NULL != pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
    {
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
        pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
        pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels = 0;
    }
    return status;
}



/*
 This function flushes the roam scan cache and creates fresh cache
 based on the input channel list
*/
eHalStatus csrCreateBgScanRoamChannelList(tpAniSirGlobal pMac,
                                          tANI_U8 sessionId,
                                          const tANI_U8 *pChannelList,
                                          const tANI_U8 numChannels)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
                                        &pMac->roam.neighborRoamInfo[sessionId];

    pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels = numChannels;

    pNeighborRoamInfo->cfgParams.channelInfo.ChannelList =
            vos_mem_malloc(pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels);

    if (NULL == pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
    {
        smsLog(pMac, LOGE, FL("Memory Allocation for CFG Channel List failed"));
        pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels = 0;
        return eHAL_STATUS_RESOURCES;
    }

    /* Update the roam global structure */
    vos_mem_copy(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList,
                 pChannelList,
                 pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels);
    return status;
}

/* This function modifies the bgscan channel list set via config ini or
   runtime, whenever the band changes.
   if the band is auto, then no operation is performed on the channel list
   if the band is 2.4G, then make sure channel list contains only 2.4G valid channels
   if the band is 5G, then make sure channel list contains only 5G valid channels
*/
eHalStatus csrUpdateBgScanConfigIniChannelList(tpAniSirGlobal pMac,
                                               tANI_U8 sessionId,
                                               eCsrBand eBand)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
                                        &pMac->roam.neighborRoamInfo[sessionId];
    tANI_U8 outNumChannels = 0;
    tANI_U8 inNumChannels = 0;
    tANI_U8 *inPtr = NULL;
    tANI_U8 i = 0;
    tANI_U8 ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};

    if (NULL == pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)

    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
            "No update required for channel list "
            "either cfg.ini channel list is not set up or "
            "auto band (Band %d)", eBand);
        return status;
    }

    inNumChannels = pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels;
    inPtr = pNeighborRoamInfo->cfgParams.channelInfo.ChannelList;
    if (eCSR_BAND_24 == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            if (CSR_IS_CHANNEL_24GHZ(inPtr[i]) && csrRoamIsChannelValid(pMac, inPtr[i]))
            {
                ChannelList[outNumChannels++] = inPtr[i];
            }
        }
        csrFlushCfgBgScanRoamChannelList(pMac, sessionId);
        csrCreateBgScanRoamChannelList(pMac, sessionId, ChannelList,
                                       outNumChannels);
    }
    else if (eCSR_BAND_5G == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            /* Add 5G Non-DFS channel */
            if (CSR_IS_CHANNEL_5GHZ(inPtr[i]) &&
               csrRoamIsChannelValid(pMac, inPtr[i]) &&
               !CSR_IS_CHANNEL_DFS(inPtr[i]))
            {
               ChannelList[outNumChannels++] = inPtr[i];
            }
        }
        csrFlushCfgBgScanRoamChannelList(pMac, sessionId);
        csrCreateBgScanRoamChannelList(pMac, sessionId, ChannelList,
                                       outNumChannels);
    }
    else if (eCSR_BAND_ALL == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            if (csrRoamIsChannelValid(pMac, inPtr[i]) &&
               !CSR_IS_CHANNEL_DFS(inPtr[i]))
            {
               ChannelList[outNumChannels++] = inPtr[i];
            }
        }
        csrFlushCfgBgScanRoamChannelList(pMac, sessionId);
        csrCreateBgScanRoamChannelList(pMac, sessionId, ChannelList,
                                       outNumChannels);
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
            "Invalid band, No operation carried out (Band %d)", eBand);
        status = eHAL_STATUS_INVALID_PARAMETER;
    }

    return status;
}
#endif

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/*
 * This function modifies the roam scan channel list as per AP neighbor
 * report; AP neighbor report may be empty or may include only other AP
 * channels; in any case, we merge the channel list with the learned occupied
 * channels list.
 * if the band is 2.4G, then make sure channel list contains only 2.4G
 * valid channels if the band is 5G, then make sure channel list contains
 * only 5G valid channels
 */
eHalStatus csrCreateRoamScanChannelList(tpAniSirGlobal pMac,
                                        tANI_U8 sessionId,
                                        tANI_U8 *pChannelList,
                                        tANI_U8 numChannels,
                                        const eCsrBand eBand)
{
    eHalStatus   status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo
         = &pMac->roam.neighborRoamInfo[sessionId];
    tANI_U8      outNumChannels = 0;
    tANI_U8      inNumChannels = numChannels;
    tANI_U8      *inPtr = pChannelList;
    tANI_U8      i = 0;
    tANI_U8      ChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    tANI_U8      tmpChannelList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    tANI_U8      mergedOutputNumOfChannels = 0;
    tpCsrChannelInfo  currChannelListInfo
         = &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;
    /*
     * Create a Union of occupied channel list learnt by the DUT along
     * with the Neighbor report Channels. This increases the chances of
     * the DUT to get a candidate AP while roaming even if the Neighbor
     * Report is not able to provide sufficient information.
     */
    if (pMac->scan.occupiedChannels[sessionId].numChannels)
    {
       csrNeighborRoamMergeChannelLists(pMac,
                  &pMac->scan.occupiedChannels[sessionId].channelList[0],
                  pMac->scan.occupiedChannels[sessionId].numChannels,
                  inPtr,
                  inNumChannels,
                  &mergedOutputNumOfChannels);
       inNumChannels =  mergedOutputNumOfChannels;
    }
    if (eCSR_BAND_24 == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            if (CSR_IS_CHANNEL_24GHZ(inPtr[i])
                && csrRoamIsChannelValid(pMac, inPtr[i]))
            {
                ChannelList[outNumChannels++] = inPtr[i];
            }
        }
    }
    else if (eCSR_BAND_5G == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            /* Add 5G Non-DFS channel */
            if (CSR_IS_CHANNEL_5GHZ(inPtr[i]) &&
               csrRoamIsChannelValid(pMac, inPtr[i]) &&
               !CSR_IS_CHANNEL_DFS(inPtr[i]))
            {
               ChannelList[outNumChannels++] = inPtr[i];
            }
        }
    }
    else if (eCSR_BAND_ALL == eBand)
    {
        for (i = 0; i < inNumChannels; i++)
        {
            if (csrRoamIsChannelValid(pMac, inPtr[i]) &&
               !CSR_IS_CHANNEL_DFS(inPtr[i]))
            {
               ChannelList[outNumChannels++] = inPtr[i];
            }
        }
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
            "Invalid band, No operation carried out (Band %d)", eBand);
        return eHAL_STATUS_INVALID_PARAMETER;
    }
    /*
     * if roaming within band is enabled, then select only the
     * in band channels .
     * This is required only if the band capability is set to ALL,
     * E.g., if band capability is only 2.4G then all the channels in the
     * list are already filtered for 2.4G channels, hence ignore this check
     */
    if ((eCSR_BAND_ALL == eBand) && CSR_IS_ROAM_INTRA_BAND_ENABLED(pMac))
    {
        csrNeighborRoamChannelsFilterByCurrentBand(
                             pMac,
                             sessionId,
                             ChannelList,
                             outNumChannels,
                             tmpChannelList,
                             &outNumChannels);
        vos_mem_copy(ChannelList,
                     tmpChannelList, outNumChannels);
    }
    /* Prepare final roam scan channel list */
    if(outNumChannels)
    {
        /* Clear the channel list first */
        if (NULL != currChannelListInfo->ChannelList)
        {
            vos_mem_free(currChannelListInfo->ChannelList);
            currChannelListInfo->ChannelList = NULL;
            currChannelListInfo->numOfChannels = 0;
        }
        currChannelListInfo->ChannelList
          = vos_mem_malloc(outNumChannels * sizeof(tANI_U8));
        if (NULL == currChannelListInfo->ChannelList)
        {
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
                "Failed to allocate memory for roam scan channel list");
            currChannelListInfo->numOfChannels = 0;
            return VOS_STATUS_E_RESOURCES;
        }
        vos_mem_copy(currChannelListInfo->ChannelList,
                     ChannelList, outNumChannels);
    }
    return status;
}
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

eHalStatus csrSetBand(tHalHandle hHal, tANI_U8 sessionId, eCsrBand eBand)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS;
    if (CSR_IS_PHY_MODE_A_ONLY(pMac) &&
            (eBand == eCSR_BAND_24))
    {
        /* DOT11 mode configured to 11a only and received
           request to change the band to 2.4 GHz */
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "failed to set band cfg80211 = %u, band = %u",
            pMac->roam.configParam.uCfgDot11Mode, eBand);
        return eHAL_STATUS_INVALID_PARAMETER;
    }
    if ((CSR_IS_PHY_MODE_B_ONLY(pMac) ||
                CSR_IS_PHY_MODE_G_ONLY(pMac)) &&
            (eBand == eCSR_BAND_5G))
    {
        /* DOT11 mode configured to 11b/11g only and received
           request to change the band to 5 GHz */
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "failed to set band dot11mode = %u, band = %u",
            pMac->roam.configParam.uCfgDot11Mode, eBand);
        return eHAL_STATUS_INVALID_PARAMETER;
    }
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
            "Band changed to %u (0 - ALL, 1 - 2.4 GHZ, 2 - 5GHZ)", eBand);
    pMac->roam.configParam.eBand = eBand;
    pMac->roam.configParam.bandCapability = eBand;
    csrScanGetSupportedChannels( pMac );
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (!csrRoamIsRoamOffloadScanEnabled(pMac))
#endif
       csrUpdateBgScanConfigIniChannelList(pMac, sessionId, eBand);
#endif
    status = csrInitGetChannels( pMac );
    if (eHAL_STATUS_SUCCESS == status)
        csrInitChannelList( hHal );
    return status;
}


/* The function csrConvertCBIniValueToPhyCBState and
 * csrConvertPhyCBStateToIniValue have been
 * introduced to convert the ini value to the ENUM used in csr and MAC for CB state
 * Ideally we should have kept the ini value and enum value same and representing the same
 * cb values as in 11n standard i.e.
 * Set to 1 (SCA) if the secondary channel is above the primary channel
 * Set to 3 (SCB) if the secondary channel is below the primary channel
 * Set to 0 (SCN) if no secondary channel is present
 * However, since our driver is already distributed we will keep the ini definition as it is which is:
 * 0 - secondary none
 * 1 - secondary LOW
 * 2 - secondary HIGH
 * and convert to enum value used within the driver in
 * csrChangeDefaultConfigParam using this function
 * The enum values are as follows:
 * PHY_SINGLE_CHANNEL_CENTERED          = 0
 * PHY_DOUBLE_CHANNEL_LOW_PRIMARY   = 1
 * PHY_DOUBLE_CHANNEL_HIGH_PRIMARY  = 3
 */
ePhyChanBondState csrConvertCBIniValueToPhyCBState(v_U32_t cbIniValue)
{

   ePhyChanBondState phyCbState;
   switch (cbIniValue) {
      // secondary none
      case eCSR_INI_SINGLE_CHANNEL_CENTERED:
        phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
        break;
      // secondary LOW
      case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
        phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
        break;
      // secondary HIGH
      case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
        phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
        break;
#ifdef WLAN_FEATURE_11AC
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
        break;
      case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
        break;
#endif
      default:
        // If an invalid value is passed, disable CHANNEL BONDING
        phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
        break;
   }
   return phyCbState;
}

v_U32_t csrConvertPhyCBStateToIniValue(ePhyChanBondState phyCbState)
{

   v_U32_t cbIniValue;
   switch (phyCbState) {
      // secondary none
      case PHY_SINGLE_CHANNEL_CENTERED:
        cbIniValue = eCSR_INI_SINGLE_CHANNEL_CENTERED;
        break;
      // secondary LOW
      case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
        cbIniValue = eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
        break;
      // secondary HIGH
      case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
        cbIniValue = eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
        break;
#ifdef WLAN_FEATURE_11AC
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
        cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
        break;
#endif
      default:
        // return some invalid value
        cbIniValue = eCSR_INI_CHANNEL_BONDING_STATE_MAX;
        break;
   }
   return cbIniValue;
}

eHalStatus csrChangeDefaultConfigParam(tpAniSirGlobal pMac, tCsrConfigParam *pParam)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    if(pParam)
    {
        pMac->roam.configParam.pkt_err_disconn_th = pParam->pkt_err_disconn_th;
        pMac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
        cfgSetInt(pMac, WNI_CFG_WME_ENABLED,
                        (pParam->WMMSupportMode == eCsrRoamWmmNoQos)? 0:1);
        pMac->roam.configParam.Is11eSupportEnabled = pParam->Is11eSupportEnabled;
        pMac->roam.configParam.FragmentationThreshold = pParam->FragmentationThreshold;
        pMac->roam.configParam.Is11dSupportEnabled = pParam->Is11dSupportEnabled;
        pMac->roam.configParam.Is11dSupportEnabledOriginal = pParam->Is11dSupportEnabled;
        pMac->roam.configParam.Is11hSupportEnabled = pParam->Is11hSupportEnabled;

        pMac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
        pMac->roam.configParam.mcc_rts_cts_prot_enable =
                                          pParam->mcc_rts_cts_prot_enable;
        pMac->roam.configParam.mcc_bcast_prob_resp_enable =
                                          pParam->mcc_bcast_prob_resp_enable;

        pMac->roam.configParam.fAllowMCCGODiffBI = pParam->fAllowMCCGODiffBI;

        /* channelBondingMode5GHz plays a dual role right now
         * INFRA STA will use this non zero value as CB enabled and SOFTAP will use this non-zero value to determine the secondary channel offset
         * This is how channelBondingMode5GHz works now and this is kept intact to avoid any cfg.ini change
         */
        if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
        {
            smsLog( pMac, LOGW, "Invalid CB value from ini in 2.4GHz band %d, CB DISABLED", pParam->channelBondingMode24GHz);
        }
        pMac->roam.configParam.channelBondingMode24GHz = csrConvertCBIniValueToPhyCBState(pParam->channelBondingMode24GHz);
        if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
        {
            smsLog( pMac, LOGW, "Invalid CB value from ini in 5GHz band %d, CB DISABLED", pParam->channelBondingMode5GHz);
        }
        pMac->roam.configParam.stacbmode = pParam->stacbmode;
        pMac->roam.configParam.channelBondingMode5GHz = csrConvertCBIniValueToPhyCBState(pParam->channelBondingMode5GHz);
        pMac->roam.configParam.RTSThreshold = pParam->RTSThreshold;
        pMac->roam.configParam.phyMode = pParam->phyMode;
        pMac->roam.configParam.shortSlotTime = pParam->shortSlotTime;
        pMac->roam.configParam.HeartbeatThresh24 = pParam->HeartbeatThresh24;
        pMac->roam.configParam.HeartbeatThresh50 = pParam->HeartbeatThresh50;
        pMac->roam.configParam.ProprietaryRatesEnabled = pParam->ProprietaryRatesEnabled;
        pMac->roam.configParam.TxRate = pParam->TxRate;
        pMac->roam.configParam.AdHocChannel24 = pParam->AdHocChannel24;
        pMac->roam.configParam.AdHocChannel5G = pParam->AdHocChannel5G;
        pMac->roam.configParam.bandCapability = pParam->bandCapability;
        pMac->roam.configParam.cbChoice = pParam->cbChoice;
        pMac->roam.configParam.bgScanInterval = pParam->bgScanInterval;
        pMac->roam.configParam.disableAggWithBtc = pParam->disableAggWithBtc;

        pMac->roam.configParam.neighborRoamConfig.delay_before_vdev_stop =
            pParam->neighborRoamConfig.delay_before_vdev_stop;

        //if HDD passed down non zero values then only update,
        //otherwise keep using the defaults
        if (pParam->initial_scan_no_dfs_chnl) {
            pMac->roam.configParam.initial_scan_no_dfs_chnl =
                                        pParam->initial_scan_no_dfs_chnl;
        }
        if (pParam->nInitialDwellTime)
        {
            pMac->roam.configParam.nInitialDwellTime =
                                        pParam->nInitialDwellTime;
        }
        if (pParam->nActiveMaxChnTime)
        {
            pMac->roam.configParam.nActiveMaxChnTime = pParam->nActiveMaxChnTime;
            cfgSetInt(pMac, WNI_CFG_ACTIVE_MAXIMUM_CHANNEL_TIME,
                      pParam->nActiveMaxChnTime);
        }
        if (pParam->nActiveMinChnTime)
        {
            pMac->roam.configParam.nActiveMinChnTime = pParam->nActiveMinChnTime;
            cfgSetInt(pMac, WNI_CFG_ACTIVE_MINIMUM_CHANNEL_TIME,
                      pParam->nActiveMinChnTime);
        }
        if (pParam->nPassiveMaxChnTime)
        {
            pMac->roam.configParam.nPassiveMaxChnTime = pParam->nPassiveMaxChnTime;
            cfgSetInt(pMac, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME,
                      pParam->nPassiveMaxChnTime);
        }
        if (pParam->nPassiveMinChnTime)
        {
            pMac->roam.configParam.nPassiveMinChnTime = pParam->nPassiveMinChnTime;
            cfgSetInt(pMac, WNI_CFG_PASSIVE_MINIMUM_CHANNEL_TIME,
                      pParam->nPassiveMinChnTime);
        }
        if (pParam->nActiveMaxChnTimeBtc)
        {
            pMac->roam.configParam.nActiveMaxChnTimeBtc = pParam->nActiveMaxChnTimeBtc;
        }
        if (pParam->nActiveMinChnTimeBtc)
        {
            pMac->roam.configParam.nActiveMinChnTimeBtc = pParam->nActiveMinChnTimeBtc;
        }
#ifdef WLAN_AP_STA_CONCURRENCY
        if (pParam->nActiveMaxChnTimeConc)
        {
            pMac->roam.configParam.nActiveMaxChnTimeConc = pParam->nActiveMaxChnTimeConc;
        }
        if (pParam->nActiveMinChnTimeConc)
        {
            pMac->roam.configParam.nActiveMinChnTimeConc = pParam->nActiveMinChnTimeConc;
        }
        if (pParam->nPassiveMaxChnTimeConc)
        {
            pMac->roam.configParam.nPassiveMaxChnTimeConc = pParam->nPassiveMaxChnTimeConc;
        }
        if (pParam->nPassiveMinChnTimeConc)
        {
            pMac->roam.configParam.nPassiveMinChnTimeConc = pParam->nPassiveMinChnTimeConc;
        }
        if (pParam->nRestTimeConc)
        {
            pMac->roam.configParam.nRestTimeConc = pParam->nRestTimeConc;
        }
        if (pParam->min_rest_time_conc)
        {
            pMac->roam.configParam.min_rest_time_conc = pParam->min_rest_time_conc;
        }
        if (pParam->idle_time_conc)
        {
            pMac->roam.configParam.idle_time_conc = pParam->idle_time_conc;
        }
        if (pParam->nNumStaChanCombinedConc)
        {
            pMac->roam.configParam.nNumStaChanCombinedConc = pParam->nNumStaChanCombinedConc;
        }
        if (pParam->nNumP2PChanCombinedConc)
        {
            pMac->roam.configParam.nNumP2PChanCombinedConc = pParam->nNumP2PChanCombinedConc;
        }
#endif
        //if upper layer wants to disable idle scan altogether set it to 0
        if (pParam->impsSleepTime)
        {
            //Change the unit from second to microsecond
            tANI_U32 impsSleepTime =
                                  pParam->impsSleepTime * VOS_TIMER_TO_SEC_UNIT;

            if(CSR_IDLE_SCAN_NO_PS_INTERVAL_MIN <= impsSleepTime)
            {
                pMac->roam.configParam.impsSleepTime = impsSleepTime;
            }
        else
        {
            pMac->roam.configParam.impsSleepTime = CSR_IDLE_SCAN_NO_PS_INTERVAL;
        }
        }
        else
        {
            pMac->roam.configParam.impsSleepTime = 0;
        }
        pMac->roam.configParam.eBand = pParam->eBand;
        pMac->roam.configParam.uCfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(NULL, pMac->roam.configParam.phyMode,
                                                    pMac->roam.configParam.ProprietaryRatesEnabled);
        //if HDD passed down non zero values for age params, then only update,
        //otherwise keep using the defaults
        if (pParam->nScanResultAgeCount)
        {
            pMac->roam.configParam.agingCount = pParam->nScanResultAgeCount;
        }
        if(pParam->scanAgeTimeNCNPS)
        {
            pMac->roam.configParam.scanAgeTimeNCNPS = pParam->scanAgeTimeNCNPS;
        }
        if(pParam->scanAgeTimeNCPS)
        {
            pMac->roam.configParam.scanAgeTimeNCPS = pParam->scanAgeTimeNCPS;
        }
        if(pParam->scanAgeTimeCNPS)
        {
            pMac->roam.configParam.scanAgeTimeCNPS = pParam->scanAgeTimeCNPS;
        }
        if(pParam->scanAgeTimeCPS)
        {
            pMac->roam.configParam.scanAgeTimeCPS = pParam->scanAgeTimeCPS;
        }

        pMac->first_scan_bucket_threshold =
                                pParam->first_scan_bucket_threshold;
        csrAssignRssiForCategory(pMac, pMac->first_scan_bucket_threshold,
                                pParam->bCatRssiOffset);
        pMac->roam.configParam.nRoamingTime = pParam->nRoamingTime;
        pMac->roam.configParam.fEnforce11dChannels = pParam->fEnforce11dChannels;
        pMac->roam.configParam.fSupplicantCountryCodeHasPriority = pParam->fSupplicantCountryCodeHasPriority;
        pMac->roam.configParam.fEnforceCountryCodeMatch = pParam->fEnforceCountryCodeMatch;
        pMac->roam.configParam.fEnforceDefaultDomain = pParam->fEnforceDefaultDomain;
        pMac->roam.configParam.vccRssiThreshold = pParam->vccRssiThreshold;
        pMac->roam.configParam.vccUlMacLossThreshold = pParam->vccUlMacLossThreshold;
        pMac->roam.configParam.IsIdleScanEnabled = pParam->IsIdleScanEnabled;
        pMac->roam.configParam.statsReqPeriodicity = pParam->statsReqPeriodicity;
        pMac->roam.configParam.statsReqPeriodicityInPS = pParam->statsReqPeriodicityInPS;
        //Assign this before calling CsrInit11dInfo
        pMac->roam.configParam.nTxPowerCap = pParam->nTxPowerCap;
        if( csrIs11dSupported( pMac ) )
        {
            status = CsrInit11dInfo(pMac, &pParam->Csr11dinfo);
        }
        else
        {
            pMac->scan.curScanType = eSIR_ACTIVE_SCAN;
        }

        /* Initialize the power + channel information if 11h is enabled.
        If 11d is enabled this information has already been initialized */
        if( csrIs11hSupported( pMac ) && !csrIs11dSupported( pMac ) )
        {
            csrInitChannelPowerList(pMac, &pParam->Csr11dinfo);
        }


#ifdef WLAN_FEATURE_VOWIFI_11R
        vos_mem_copy(&pMac->roam.configParam.csr11rConfig,
                     &pParam->csr11rConfig, sizeof(tCsr11rConfigParams));
        smsLog( pMac, LOG1, "IsFTResourceReqSupp = %d", pMac->roam.configParam.csr11rConfig.IsFTResourceReqSupported);
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        pMac->roam.configParam.isFastTransitionEnabled = pParam->isFastTransitionEnabled;
        pMac->roam.configParam.RoamRssiDiff = pParam->RoamRssiDiff;
        pMac->roam.configParam.nImmediateRoamRssiDiff = pParam->nImmediateRoamRssiDiff;
        smsLog( pMac, LOG1, "nImmediateRoamRssiDiff = %d",
                pMac->roam.configParam.nImmediateRoamRssiDiff );
        pMac->roam.configParam.nRoamPrefer5GHz = pParam->nRoamPrefer5GHz;
        pMac->roam.configParam.nRoamIntraBand = pParam->nRoamIntraBand;
        pMac->roam.configParam.isWESModeEnabled = pParam->isWESModeEnabled;
        pMac->roam.configParam.nProbes = pParam->nProbes;
        pMac->roam.configParam.nRoamScanHomeAwayTime = pParam->nRoamScanHomeAwayTime;
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        pMac->roam.configParam.isRoamOffloadScanEnabled = pParam->isRoamOffloadScanEnabled;
        pMac->roam.configParam.bFastRoamInConIniFeatureEnabled = pParam->bFastRoamInConIniFeatureEnabled;
#endif
#ifdef FEATURE_WLAN_LFR
        pMac->roam.configParam.isFastRoamIniFeatureEnabled = pParam->isFastRoamIniFeatureEnabled;
        pMac->roam.configParam.MAWCEnabled = pParam->MAWCEnabled;
#endif

#ifdef FEATURE_WLAN_ESE
        pMac->roam.configParam.isEseIniFeatureEnabled = pParam->isEseIniFeatureEnabled;
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        vos_mem_copy(&pMac->roam.configParam.neighborRoamConfig,
                     &pParam->neighborRoamConfig, sizeof(tCsrNeighborRoamConfigParams));
        smsLog( pMac, LOG1, "nNeighborScanTimerPerioid = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod);
        smsLog( pMac, LOG1, "nNeighborReassocRssiThreshold = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold);
        smsLog( pMac, LOG1, "nNeighborLookupRssiThreshold = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold);
        smsLog( pMac, LOG1, "nOpportunisticThresholdDiff = %d", pMac->roam.configParam.neighborRoamConfig.nOpportunisticThresholdDiff);
        smsLog( pMac, LOG1, "nRoamRescanRssiDiff = %d", pMac->roam.configParam.neighborRoamConfig.nRoamRescanRssiDiff);
        smsLog( pMac, LOG1, "nNeighborScanMinChanTime = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime);
        smsLog( pMac, LOG1, "nNeighborScanMaxChanTime = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime);
        smsLog( pMac, LOG1, "nMaxNeighborRetries = %d", pMac->roam.configParam.neighborRoamConfig.nMaxNeighborRetries);
        smsLog( pMac, LOG1, "nNeighborResultsRefreshPeriod = %d", pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod);
        smsLog( pMac, LOG1, "nEmptyScanRefreshPeriod = %d", pMac->roam.configParam.neighborRoamConfig.nEmptyScanRefreshPeriod);
        {
           int i;
           smsLog( pMac, LOG1, FL("Num of Channels in CFG Channel List: %d"), pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels);
           for( i=0; i< pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels; i++)
           {
              smsLog( pMac, LOG1, "%d ", pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[i] );
           }
        }
        smsLog( pMac, LOG1, "nRoamBmissFirstBcnt = %d", pMac->roam.configParam.neighborRoamConfig.nRoamBmissFirstBcnt);
        smsLog( pMac, LOG1, "nRoamBmissFinalBcnt = %d", pMac->roam.configParam.neighborRoamConfig.nRoamBmissFinalBcnt);
        smsLog( pMac, LOG1, "nRoamBeaconRssiWeight = %d", pMac->roam.configParam.neighborRoamConfig.nRoamBeaconRssiWeight);
#endif
        pMac->roam.configParam.addTSWhenACMIsOff = pParam->addTSWhenACMIsOff;
        pMac->scan.fValidateList = pParam->fValidateList;
        pMac->scan.fEnableBypass11d = pParam->fEnableBypass11d;
        pMac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
        pMac->scan.scanResultCfgAgingTime = pParam->scanCfgAgingTime;
        pMac->roam.configParam.fScanTwice = pParam->fScanTwice;
        pMac->scan.fFirstScanOnly2GChnl = pParam->fFirstScanOnly2GChnl;
        pMac->scan.scanBandPreference = pParam->scanBandPreference;
        /*
         * This parameter is not available in cfg and not passed from upper
         * layers. Instead it is initialized here. This parameter is used in
         * concurrency to determine if there are concurrent active sessions.
         * Is used as a temporary fix to disconnect all active sessions when
         * BMPS enabled so the active session if Infra STA
         * will automatically connect back and resume BMPS since resume BMPS is
         * not working when moving from concurrent to single session
         */
        //Remove this code once SLM_Sessionization is supported
        //BMPS_WORKAROUND_NOT_NEEDED
        pMac->roam.configParam.doBMPSWorkaround = 0;

#ifdef WLAN_FEATURE_11AC
        pMac->roam.configParam.nVhtChannelWidth = pParam->nVhtChannelWidth;
        pMac->roam.configParam.txBFEnable= pParam->enableTxBF;
        pMac->roam.configParam.txBFCsnValue = pParam->txBFCsnValue;
        pMac->roam.configParam.enable2x2= pParam->enable2x2;
        pMac->roam.configParam.enableVhtFor24GHz = pParam->enableVhtFor24GHz;
        pMac->roam.configParam.txMuBformee= pParam->enableMuBformee;
        pMac->roam.configParam.enableVhtpAid = pParam->enableVhtpAid;
        pMac->roam.configParam.enableVhtGid = pParam->enableVhtGid;
#endif
        pMac->roam.configParam.enableAmpduPs = pParam->enableAmpduPs;
        pMac->roam.configParam.enableHtSmps = pParam->enableHtSmps;
        pMac->roam.configParam.htSmps= pParam->htSmps;
        pMac->roam.configParam.txLdpcEnable = pParam->enableTxLdpc;

        pMac->roam.configParam.max_amsdu_num = pParam->max_amsdu_num;
        pMac->roam.configParam.nSelect5GHzMargin = pParam->nSelect5GHzMargin;
        pMac->roam.configParam.ignorePeerErpInfo = pParam->ignorePeerErpInfo;
        pMac->roam.configParam.isCoalesingInIBSSAllowed =
                               pParam->isCoalesingInIBSSAllowed;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
        pMac->roam.configParam.cc_switch_mode = pParam->cc_switch_mode;
#endif
        pMac->roam.configParam.allowDFSChannelRoam = pParam->allowDFSChannelRoam;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        pMac->roam.configParam.isRoamOffloadEnabled =
                               pParam->isRoamOffloadEnabled;
#endif
        pMac->roam.configParam.obssEnabled = pParam->obssEnabled;
        pMac->roam.configParam.conc_custom_rule1 =
                               pParam->conc_custom_rule1;
        pMac->roam.configParam.conc_custom_rule2 =
                               pParam->conc_custom_rule2;
        pMac->roam.configParam.is_sta_connection_in_5gz_enabled =
                               pParam->is_sta_connection_in_5gz_enabled;

        pMac->enable_dot11p = pParam->enable_dot11p;
        pMac->roam.configParam.sendDeauthBeforeCon =
                               pParam->sendDeauthBeforeCon;
    }

    return status;
}

eHalStatus csrGetConfigParam(tpAniSirGlobal pMac, tCsrConfigParam *pParam)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    if(pParam)
    {
        pParam->WMMSupportMode = pMac->roam.configParam.WMMSupportMode;
        pParam->Is11eSupportEnabled = pMac->roam.configParam.Is11eSupportEnabled;
        pParam->FragmentationThreshold = pMac->roam.configParam.FragmentationThreshold;
        pParam->Is11dSupportEnabled = pMac->roam.configParam.Is11dSupportEnabled;
        pParam->Is11dSupportEnabledOriginal = pMac->roam.configParam.Is11dSupportEnabledOriginal;
        pParam->Is11hSupportEnabled = pMac->roam.configParam.Is11hSupportEnabled;
        pParam->channelBondingMode24GHz = csrConvertPhyCBStateToIniValue(pMac->roam.configParam.channelBondingMode24GHz);
        pParam->channelBondingMode5GHz = csrConvertPhyCBStateToIniValue(pMac->roam.configParam.channelBondingMode5GHz);
        pParam->stacbmode = pMac->roam.configParam.stacbmode;
        pParam->RTSThreshold = pMac->roam.configParam.RTSThreshold;
        pParam->phyMode = pMac->roam.configParam.phyMode;
        pParam->shortSlotTime = pMac->roam.configParam.shortSlotTime;
        pParam->HeartbeatThresh24 = pMac->roam.configParam.HeartbeatThresh24;
        pParam->HeartbeatThresh50 = pMac->roam.configParam.HeartbeatThresh50;
        pParam->ProprietaryRatesEnabled = pMac->roam.configParam.ProprietaryRatesEnabled;
        pParam->TxRate = pMac->roam.configParam.TxRate;
        pParam->AdHocChannel24 = pMac->roam.configParam.AdHocChannel24;
        pParam->AdHocChannel5G = pMac->roam.configParam.AdHocChannel5G;
        pParam->bandCapability = pMac->roam.configParam.bandCapability;
        pParam->cbChoice = pMac->roam.configParam.cbChoice;
        pParam->bgScanInterval = pMac->roam.configParam.bgScanInterval;
        pParam->nActiveMaxChnTime = pMac->roam.configParam.nActiveMaxChnTime;
        pParam->nActiveMinChnTime = pMac->roam.configParam.nActiveMinChnTime;
        pParam->nPassiveMaxChnTime = pMac->roam.configParam.nPassiveMaxChnTime;
        pParam->nPassiveMinChnTime = pMac->roam.configParam.nPassiveMinChnTime;
        pParam->nActiveMaxChnTimeBtc = pMac->roam.configParam.nActiveMaxChnTimeBtc;
        pParam->nActiveMinChnTimeBtc = pMac->roam.configParam.nActiveMinChnTimeBtc;
        pParam->disableAggWithBtc = pMac->roam.configParam.disableAggWithBtc;
#ifdef WLAN_AP_STA_CONCURRENCY
        pParam->nActiveMaxChnTimeConc = pMac->roam.configParam.nActiveMaxChnTimeConc;
        pParam->nActiveMinChnTimeConc = pMac->roam.configParam.nActiveMinChnTimeConc;
        pParam->nPassiveMaxChnTimeConc = pMac->roam.configParam.nPassiveMaxChnTimeConc;
        pParam->nPassiveMinChnTimeConc = pMac->roam.configParam.nPassiveMinChnTimeConc;
        pParam->nRestTimeConc = pMac->roam.configParam.nRestTimeConc;
        pParam->min_rest_time_conc = pMac->roam.configParam.min_rest_time_conc;
        pParam->idle_time_conc = pMac->roam.configParam.idle_time_conc;

        pParam->nNumStaChanCombinedConc = pMac->roam.configParam.nNumStaChanCombinedConc;
        pParam->nNumP2PChanCombinedConc = pMac->roam.configParam.nNumP2PChanCombinedConc;
#endif
        //Change the unit from microsecond to second
        pParam->impsSleepTime =
                   pMac->roam.configParam.impsSleepTime / VOS_TIMER_TO_SEC_UNIT;
        pParam->eBand = pMac->roam.configParam.eBand;
        pParam->nScanResultAgeCount = pMac->roam.configParam.agingCount;
        pParam->scanAgeTimeNCNPS = pMac->roam.configParam.scanAgeTimeNCNPS;
        pParam->scanAgeTimeNCPS = pMac->roam.configParam.scanAgeTimeNCPS;
        pParam->scanAgeTimeCNPS = pMac->roam.configParam.scanAgeTimeCNPS;
        pParam->scanAgeTimeCPS = pMac->roam.configParam.scanAgeTimeCPS;
        pParam->bCatRssiOffset = pMac->roam.configParam.bCatRssiOffset;
        pParam->nRoamingTime = pMac->roam.configParam.nRoamingTime;
        pParam->fEnforce11dChannels = pMac->roam.configParam.fEnforce11dChannels;
        pParam->fSupplicantCountryCodeHasPriority = pMac->roam.configParam.fSupplicantCountryCodeHasPriority;
        pParam->fEnforceCountryCodeMatch = pMac->roam.configParam.fEnforceCountryCodeMatch;
        pParam->fEnforceDefaultDomain = pMac->roam.configParam.fEnforceDefaultDomain;
        pParam->vccRssiThreshold = pMac->roam.configParam.vccRssiThreshold;
        pParam->vccUlMacLossThreshold = pMac->roam.configParam.vccUlMacLossThreshold;
        pParam->IsIdleScanEnabled = pMac->roam.configParam.IsIdleScanEnabled;
        pParam->nTxPowerCap = pMac->roam.configParam.nTxPowerCap;
        pParam->statsReqPeriodicity = pMac->roam.configParam.statsReqPeriodicity;
        pParam->statsReqPeriodicityInPS = pMac->roam.configParam.statsReqPeriodicityInPS;
        pParam->addTSWhenACMIsOff = pMac->roam.configParam.addTSWhenACMIsOff;
        pParam->fValidateList = pMac->roam.configParam.fValidateList;
        pParam->fEnableBypass11d = pMac->scan.fEnableBypass11d;
        pParam->fEnableDFSChnlScan = pMac->scan.fEnableDFSChnlScan;
        pParam->fScanTwice = pMac->roam.configParam.fScanTwice;
        pParam->fFirstScanOnly2GChnl = pMac->scan.fFirstScanOnly2GChnl;
        pParam->fEnableMCCMode = pMac->roam.configParam.fenableMCCMode;
        pParam->fAllowMCCGODiffBI = pMac->roam.configParam.fAllowMCCGODiffBI;
        pParam->scanCfgAgingTime = pMac->scan.scanResultCfgAgingTime;
        pParam->scanBandPreference = pMac->scan.scanBandPreference;
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        vos_mem_copy(&pParam->neighborRoamConfig,
                     &pMac->roam.configParam.neighborRoamConfig,
                     sizeof(tCsrNeighborRoamConfigParams));
#endif
#ifdef WLAN_FEATURE_11AC
        pParam->nVhtChannelWidth = pMac->roam.configParam.nVhtChannelWidth;
        pParam->enableTxBF = pMac->roam.configParam.txBFEnable;
        pParam->txBFCsnValue = pMac->roam.configParam.txBFCsnValue;
        pParam->enableMuBformee = pMac->roam.configParam.txMuBformee;
        pParam->enableVhtFor24GHz = pMac->roam.configParam.enableVhtFor24GHz;
        pParam->enable2x2 = pMac->roam.configParam.enable2x2;
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
        vos_mem_copy(&pMac->roam.configParam.csr11rConfig,
                     &pParam->csr11rConfig, sizeof(tCsr11rConfigParams));
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
        pParam->isFastTransitionEnabled = pMac->roam.configParam.isFastTransitionEnabled;
        pParam->RoamRssiDiff = pMac->roam.configParam.RoamRssiDiff;
        pParam->nImmediateRoamRssiDiff = pMac->roam.configParam.nImmediateRoamRssiDiff;
        pParam->nRoamPrefer5GHz = pMac->roam.configParam.nRoamPrefer5GHz;
        pParam->nRoamIntraBand = pMac->roam.configParam.nRoamIntraBand;
        pParam->isWESModeEnabled = pMac->roam.configParam.isWESModeEnabled;
        pParam->nProbes = pMac->roam.configParam.nProbes;
        pParam->nRoamScanHomeAwayTime = pMac->roam.configParam.nRoamScanHomeAwayTime;
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        pParam->isRoamOffloadScanEnabled = pMac->roam.configParam.isRoamOffloadScanEnabled;
        pParam->bFastRoamInConIniFeatureEnabled = pMac->roam.configParam.bFastRoamInConIniFeatureEnabled;
#endif
#ifdef FEATURE_WLAN_LFR
        pParam->isFastRoamIniFeatureEnabled = pMac->roam.configParam.isFastRoamIniFeatureEnabled;
#endif

#ifdef FEATURE_WLAN_ESE
        pParam->isEseIniFeatureEnabled = pMac->roam.configParam.isEseIniFeatureEnabled;
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        vos_mem_copy(&pParam->neighborRoamConfig,
                     &pMac->roam.configParam.neighborRoamConfig,
                     sizeof(tCsrNeighborRoamConfigParams));
        {
           int i;
           smsLog( pMac, LOG1, FL("Num of Channels in CFG Channel List: %d"), pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels);
           for( i=0; i< pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels; i++)
           {
              smsLog( pMac, LOG1, "%d ", pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[i] );
           }
        }
#endif

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
        pParam->cc_switch_mode = pMac->roam.configParam.cc_switch_mode;
#endif
        pParam->enableTxLdpc = pMac->roam.configParam.txLdpcEnable;

        pParam->max_amsdu_num = pMac->roam.configParam.max_amsdu_num;
        pParam->nSelect5GHzMargin = pMac->roam.configParam.nSelect5GHzMargin;
        pParam->ignorePeerErpInfo = pMac->roam.configParam.ignorePeerErpInfo;

        pParam->isCoalesingInIBSSAllowed =
                                pMac->roam.configParam.isCoalesingInIBSSAllowed;
        pParam->allowDFSChannelRoam =
                                pMac->roam.configParam.allowDFSChannelRoam;
        pParam->nInitialDwellTime =
                                pMac->roam.configParam.nInitialDwellTime;
        pParam->initial_scan_no_dfs_chnl =
                                pMac->roam.configParam.initial_scan_no_dfs_chnl;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        pParam->isRoamOffloadEnabled =
                                pMac->roam.configParam.isRoamOffloadEnabled;
#endif

        pParam->enable_dot11p = pMac->enable_dot11p;

        csrSetChannels(pMac, pParam);

        pParam->obssEnabled = pMac->roam.configParam.obssEnabled;

        pParam->conc_custom_rule1 =
                     pMac->roam.configParam.conc_custom_rule1;
        pParam->conc_custom_rule2 =
                     pMac->roam.configParam.conc_custom_rule2;
        pParam->is_sta_connection_in_5gz_enabled =
                     pMac->roam.configParam.is_sta_connection_in_5gz_enabled;
        pParam->sendDeauthBeforeCon =
                     pMac->roam.configParam.sendDeauthBeforeCon;
        pParam->first_scan_bucket_threshold =
                     pMac->first_scan_bucket_threshold;
        status = eHAL_STATUS_SUCCESS;
    }
    return (status);
}

eHalStatus csrSetPhyMode(tHalHandle hHal, tANI_U32 phyMode, eCsrBand eBand, tANI_BOOLEAN *pfRestartNeeded)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fRestartNeeded = eANI_BOOLEAN_FALSE;
    eCsrPhyMode newPhyMode = eCSR_DOT11_MODE_AUTO;
    do
    {
        if(eCSR_BAND_24 == eBand)
        {
            if(CSR_IS_RADIO_A_ONLY(pMac)) break;
            if(eCSR_DOT11_MODE_11a & phyMode) break;
        }
        if(eCSR_BAND_5G == eBand)
        {
            if(CSR_IS_RADIO_BG_ONLY(pMac)) break;
            if((eCSR_DOT11_MODE_11b & phyMode) || (eCSR_DOT11_MODE_11b_ONLY & phyMode) ||
                (eCSR_DOT11_MODE_11g & phyMode) || (eCSR_DOT11_MODE_11g_ONLY & phyMode)
                )
            {
                break;
            }
        }
        if (eCSR_DOT11_MODE_AUTO & phyMode) {
            newPhyMode = eCSR_DOT11_MODE_AUTO;
        } else {
            //Check for dual band and higher capability first
            if(eCSR_DOT11_MODE_11n_ONLY & phyMode)
            {
                if(eCSR_DOT11_MODE_11n_ONLY != phyMode) break;
                newPhyMode = eCSR_DOT11_MODE_11n_ONLY;
            }
            else if(eCSR_DOT11_MODE_11g_ONLY & phyMode)
            {
                if(eCSR_DOT11_MODE_11g_ONLY != phyMode) break;
                if(eCSR_BAND_5G == eBand) break;
                newPhyMode = eCSR_DOT11_MODE_11g_ONLY;
                eBand = eCSR_BAND_24;
            }
            else if(eCSR_DOT11_MODE_11b_ONLY & phyMode)
            {
                if(eCSR_DOT11_MODE_11b_ONLY != phyMode) break;
                if(eCSR_BAND_5G == eBand) break;
                newPhyMode = eCSR_DOT11_MODE_11b_ONLY;
                eBand = eCSR_BAND_24;
            }
            else if(eCSR_DOT11_MODE_11n & phyMode)
            {
                newPhyMode = eCSR_DOT11_MODE_11n;
            }
            else if(eCSR_DOT11_MODE_abg & phyMode)
            {
                newPhyMode = eCSR_DOT11_MODE_abg;
            }
            else if(eCSR_DOT11_MODE_11a & phyMode)
            {
                if((eCSR_DOT11_MODE_11g & phyMode) || (eCSR_DOT11_MODE_11b & phyMode))
                {
                    if(eCSR_BAND_ALL == eBand)
                    {
                        newPhyMode = eCSR_DOT11_MODE_abg;
                    }
                    else
                    {
                        //bad setting
                        break;
                    }
                }
                else
                {
                    newPhyMode = eCSR_DOT11_MODE_11a;
                    eBand = eCSR_BAND_5G;
                }
            }
            else if(eCSR_DOT11_MODE_11g & phyMode)
            {
                newPhyMode = eCSR_DOT11_MODE_11g;
                eBand = eCSR_BAND_24;
            }
            else if(eCSR_DOT11_MODE_11b & phyMode)
            {
                newPhyMode = eCSR_DOT11_MODE_11b;
                eBand = eCSR_BAND_24;
            }
            else
            {
                //We will never be here
                smsLog( pMac, LOGE, FL(" cannot recognize the phy mode 0x%08X"), phyMode );
                newPhyMode = eCSR_DOT11_MODE_AUTO;
            }
        }
        //Done validating
        status = eHAL_STATUS_SUCCESS;
        //Now we need to check whether a restart is needed.
        if(eBand != pMac->roam.configParam.eBand)
        {
            fRestartNeeded = eANI_BOOLEAN_TRUE;
            break;
        }
        if(newPhyMode != pMac->roam.configParam.phyMode)
        {
            fRestartNeeded = eANI_BOOLEAN_TRUE;
            break;
        }
    }while(0);
    if(HAL_STATUS_SUCCESS(status))
    {
        pMac->roam.configParam.eBand = eBand;
        pMac->roam.configParam.phyMode = newPhyMode;
        if(pfRestartNeeded)
        {
            *pfRestartNeeded = fRestartNeeded;
        }
    }
    return (status);
}

void csrPruneChannelListForMode( tpAniSirGlobal pMac, tCsrChannel *pChannelList )
{
    tANI_U8 Index;
    tANI_U8 cChannels;
    // for dual band NICs, don't need to trim the channel list....
    if ( !CSR_IS_OPEARTING_DUAL_BAND( pMac ) )
    {
        // 2.4 GHz band operation requires the channel list to be trimmed to
        // the 2.4 GHz channels only...
        if ( CSR_IS_24_BAND_ONLY( pMac ) )
        {
            for( Index = 0, cChannels = 0; Index < pChannelList->numChannels;
                 Index++ )
            {
                if ( CSR_IS_CHANNEL_24GHZ(pChannelList->channelList[ Index ]) )
                {
                    pChannelList->channelList[ cChannels ] = pChannelList->channelList[ Index ];
                    cChannels++;
                }
            }
            /*
             * Cleanup the rest of channels. Note we only need to clean up the
             * channels if we had to trim the list.
             * The amount of memory to clear is the number of channels that we
             * trimmed (pChannelList->numChannels - cChannels) times the size
             * of a channel in the structure.
             */

            if ( pChannelList->numChannels > cChannels )
            {
                vos_mem_set(&pChannelList->channelList[ cChannels ],
                             sizeof( pChannelList->channelList[ 0 ] ) *
                                 ( pChannelList->numChannels - cChannels ), 0);
            }

            pChannelList->numChannels = cChannels;
        }
        else if ( CSR_IS_5G_BAND_ONLY( pMac ) )
        {
            for ( Index = 0, cChannels = 0; Index < pChannelList->numChannels; Index++ )
            {
                if ( CSR_IS_CHANNEL_5GHZ(pChannelList->channelList[ Index ]) )
                {
                    pChannelList->channelList[ cChannels ] = pChannelList->channelList[ Index ];
                    cChannels++;
                }
            }
            /*
             * Cleanup the rest of channels. Note we only need to clean up the
             * channels if we had to trim the list.
             * The amount of memory to clear is the number of channels that we
             * trimmed (pChannelList->numChannels - cChannels) times the size
             * of a channel in the structure.
             */
            if ( pChannelList->numChannels > cChannels )
            {
                vos_mem_set(&pChannelList->channelList[ cChannels ],
                            sizeof( pChannelList->channelList[ 0 ] ) *
                            ( pChannelList->numChannels - cChannels ), 0);
            }

            pChannelList->numChannels = cChannels;
        }
    }
}
#define INFRA_AP_DEFAULT_CHANNEL 6
VOS_STATUS csrIsValidChannel(tpAniSirGlobal pMac, tANI_U8 chnNum)
{
    tANI_U8 index= 0;
    VOS_STATUS status = VOS_STATUS_E_NOSUPPORT;

    /* regulatory check */
    for (index=0; index < pMac->scan.base20MHzChannels.numChannels ;index++)
    {
        if(pMac->scan.base20MHzChannels.channelList[ index ] == chnNum){
            status = VOS_STATUS_SUCCESS;
            break;
        }
    }

    if (status == VOS_STATUS_SUCCESS)
    {
       /* dfs nol */
       for (index = 0;
             index < pMac->sap.SapDfsInfo.numCurrentRegDomainDfsChannels;
             index++)
       {
          tSapDfsNolInfo* dfsChan =
             &pMac->sap.SapDfsInfo.sapDfsChannelNolList[index];
          if ((dfsChan->dfs_channel_number == chnNum) &&
                (dfsChan->radar_status_flag == eSAP_DFS_CHANNEL_UNAVAILABLE))
          {
             VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   FL("channel %d is in dfs nol"),
                   chnNum);
             status = VOS_STATUS_E_FAILURE;
             break;
          }
       }
    }

    if (VOS_STATUS_SUCCESS != status)
    {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
             FL("channel %d is not available"),
             chnNum);
    }

    return status;
}


eHalStatus csrInitGetChannels(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U8 num20MHzChannelsFound = 0;
    VOS_STATUS vosStatus;
    tANI_U8 Index = 0;
    tANI_U8 num40MHzChannelsFound = 0;


    //TODO: this interface changed to include the 40MHz channel list
    // this needs to be tied into the adapter structure somehow and referenced appropriately for CB operation
    // Read the scan channel list (including the power limit) from EEPROM
    vosStatus = vos_nv_getChannelListWithPower( pMac->scan.defaultPowerTable, &num20MHzChannelsFound,
                        pMac->scan.defaultPowerTable40MHz, &num40MHzChannelsFound);
    if ( (VOS_STATUS_SUCCESS != vosStatus) || (num20MHzChannelsFound == 0) )
    {
        smsLog( pMac, LOGE, FL("failed to get channels "));
        status = eHAL_STATUS_FAILURE;
    }
    else
    {
        if ( num20MHzChannelsFound > WNI_CFG_VALID_CHANNEL_LIST_LEN )
        {
            num20MHzChannelsFound = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        }
        pMac->scan.numChannelsDefault = num20MHzChannelsFound;
        // Move the channel list to the global data
        // structure -- this will be used as the scan list
        for ( Index = 0; Index < num20MHzChannelsFound; Index++)
        {
            pMac->scan.base20MHzChannels.channelList[ Index ] = pMac->scan.defaultPowerTable[ Index ].chanId;
        }
        pMac->scan.base20MHzChannels.numChannels = num20MHzChannelsFound;
        if(num40MHzChannelsFound > WNI_CFG_VALID_CHANNEL_LIST_LEN)
        {
            num40MHzChannelsFound = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        }
        for ( Index = 0; Index < num40MHzChannelsFound; Index++)
        {
            pMac->scan.base40MHzChannels.channelList[ Index ] = pMac->scan.defaultPowerTable40MHz[ Index ].chanId;
        }
        pMac->scan.base40MHzChannels.numChannels = num40MHzChannelsFound;
    }
    return (status);
}
eHalStatus csrInitChannelList( tHalHandle hHal )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS;
    csrPruneChannelListForMode(pMac, &pMac->scan.baseChannels);
    csrPruneChannelListForMode(pMac, &pMac->scan.base20MHzChannels);
    csrSaveChannelPowerForBand(pMac, eANI_BOOLEAN_FALSE);
    csrSaveChannelPowerForBand(pMac, eANI_BOOLEAN_TRUE);
    // Apply the base channel list, power info, and set the Country code...
    csrApplyChannelPowerCountryInfo( pMac, &pMac->scan.base20MHzChannels, pMac->scan.countryCodeCurrent, eANI_BOOLEAN_TRUE );
    csrInitOperatingClasses(hHal);
    return (status);
}
eHalStatus csrChangeConfigParams(tpAniSirGlobal pMac,
                                 tCsrUpdateConfigParam *pUpdateConfigParam)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tCsr11dinfo *ps11dinfo = NULL;
   ps11dinfo = &pUpdateConfigParam->Csr11dinfo;
   status = CsrInit11dInfo(pMac, ps11dinfo);
   return status;
}

static eHalStatus CsrInit11dInfo(tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo)
{
  eHalStatus status = eHAL_STATUS_FAILURE;
  tANI_U8  index;
  tANI_U32 count=0;
  tSirMacChanInfo *pChanInfo;
  tSirMacChanInfo *pChanInfoStart;
  tANI_BOOLEAN applyConfig = TRUE;

  pMac->scan.currentCountryRSSI = -128;
  if(!ps11dinfo)
  {
     return (status);
  }
  if ( ps11dinfo->Channels.numChannels && ( WNI_CFG_VALID_CHANNEL_LIST_LEN >= ps11dinfo->Channels.numChannels ) )
  {
     pMac->scan.base20MHzChannels.numChannels = ps11dinfo->Channels.numChannels;
     vos_mem_copy(pMac->scan.base20MHzChannels.channelList,
                 ps11dinfo->Channels.channelList,
                 ps11dinfo->Channels.numChannels);
  }
  else
  {
     //No change
     return (eHAL_STATUS_SUCCESS);
  }
  //legacy maintenance

  vos_mem_copy(pMac->scan.countryCodeDefault, ps11dinfo->countryCode,
               WNI_CFG_COUNTRY_CODE_LEN);


  /* Tush: at csropen get this initialized with default, during csr reset if
     this already set with some value no need initialize with default again */
  if(0 == pMac->scan.countryCodeCurrent[0])
  {
      vos_mem_copy(pMac->scan.countryCodeCurrent, ps11dinfo->countryCode,
                  WNI_CFG_COUNTRY_CODE_LEN);
  }
  // need to add the max power channel list
  pChanInfo = vos_mem_malloc(sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN);
  if (pChanInfo != NULL)
  {
      vos_mem_set(pChanInfo,
                  sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN ,
                  0);

      pChanInfoStart = pChanInfo;
      for(index = 0; index < ps11dinfo->Channels.numChannels; index++)
      {
        pChanInfo->firstChanNum = ps11dinfo->ChnPower[index].firstChannel;
        pChanInfo->numChannels  = ps11dinfo->ChnPower[index].numChannels;
        pChanInfo->maxTxPower   = CSR_ROAM_MIN( ps11dinfo->ChnPower[index].maxtxPower, pMac->roam.configParam.nTxPowerCap );
        pChanInfo++;
        count++;
      }
      if(count)
      {
          csrSaveToChannelPower2G_5G( pMac, count * sizeof(tSirMacChanInfo), pChanInfoStart );
      }
      vos_mem_free(pChanInfoStart);
  }
  //Only apply them to CFG when not in STOP state. Otherwise they will be applied later
  if( HAL_STATUS_SUCCESS(status) )
  {
      for( index = 0; index < CSR_ROAM_SESSION_MAX; index++ )
      {
          if((CSR_IS_SESSION_VALID(pMac, index)) && CSR_IS_ROAM_STOP(pMac, index))
          {
              applyConfig = FALSE;
          }
    }

    if(TRUE == applyConfig)
    {
        // Apply the base channel list, power info, and set the Country code...
        csrApplyChannelPowerCountryInfo( pMac, &pMac->scan.base20MHzChannels, pMac->scan.countryCodeCurrent, eANI_BOOLEAN_TRUE );
    }

  }
  return (status);
}
/* Initialize the Channel + Power List in the local cache and in the CFG */
eHalStatus csrInitChannelPowerList( tpAniSirGlobal pMac, tCsr11dinfo *ps11dinfo)
{
  tANI_U8  index;
  tANI_U32 count=0;
  tSirMacChanInfo *pChanInfo;
  tSirMacChanInfo *pChanInfoStart;

  if(!ps11dinfo || !pMac)
  {
     return eHAL_STATUS_FAILURE;
  }

  pChanInfo = vos_mem_malloc(sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN);
  if (pChanInfo != NULL)
  {
      vos_mem_set(pChanInfo,
                  sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN,
                  0);
      pChanInfoStart = pChanInfo;

      for(index = 0; index < ps11dinfo->Channels.numChannels; index++)
      {
        pChanInfo->firstChanNum = ps11dinfo->ChnPower[index].firstChannel;
        pChanInfo->numChannels  = ps11dinfo->ChnPower[index].numChannels;
        pChanInfo->maxTxPower   = CSR_ROAM_MIN( ps11dinfo->ChnPower[index].maxtxPower, pMac->roam.configParam.nTxPowerCap );
        pChanInfo++;
        count++;
      }
      if(count)
      {
          csrSaveToChannelPower2G_5G( pMac, count * sizeof(tSirMacChanInfo), pChanInfoStart );
      }
      vos_mem_free(pChanInfoStart);
  }

  return eHAL_STATUS_SUCCESS;
}

/**
 * csr_roam_remove_duplicate_cmd_from_list()- Remove duplicate roam cmd from
 * list
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @list: pending list from which cmd needs to be removed
 * @command: cmd to be removed, can be NULL
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the pending list.
 *
 * Return: void
 */
static void csr_roam_remove_duplicate_cmd_from_list(tpAniSirGlobal mac_ctx,
			tANI_U32 session_id, tDblLinkList *list,
			tSmeCmd *command, eCsrRoamReason roam_reason)
{
	tListElem *entry, *next_entry;
	tSmeCmd *dup_cmd;
	tDblLinkList local_list;

	vos_mem_zero(&local_list, sizeof(tDblLinkList));
	if (!HAL_STATUS_SUCCESS(csrLLOpen(mac_ctx->hHdd, &local_list))) {
		smsLog(mac_ctx, LOGE, FL(" failed to open list"));
		return;
	}
	csrLLLock(list);
	entry = csrLLPeekHead(list, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csrLLNext(list, entry, LL_ACCESS_NOLOCK);
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/*
		 * Remove the previous command if..
		 * - the new roam command is for the same RoamReason...
		 * - the new roam command is a NewProfileList.
		 * - the new roam command is a Forced Dissoc
		 * - the new roam command is from an 802.11 OID
		 *   (OID_SSID or OID_BSSID).
		 */
		if ((command && (command->sessionId == dup_cmd->sessionId) &&
			((command->command == dup_cmd->command) &&
			/*
			 * This peermac check is requried for Softap/GO
			 * scenarios. For STA scenario below OR check will
			 * suffice as command will always be NULL for STA
			 * scenarios
			 */
			(vos_mem_compare(dup_cmd->u.roamCmd.peerMac,
				command->u.roamCmd.peerMac,
					sizeof(v_MACADDR_t))) &&
				((command->u.roamCmd.roamReason ==
					dup_cmd->u.roamCmd.roamReason) ||
				(eCsrForcedDisassoc ==
					command->u.roamCmd.roamReason) ||
				(eCsrHddIssued ==
					command->u.roamCmd.roamReason)))) ||
			/* OR if pCommand is NULL */
			((session_id == dup_cmd->sessionId) &&
			(eSmeCommandRoam == dup_cmd->command) &&
			((eCsrForcedDisassoc == roam_reason) ||
			(eCsrHddIssued == roam_reason)))) {
			smsLog(mac_ctx, LOGW, FL("RoamReason = %d"),
					dup_cmd->u.roamCmd.roamReason);
			/* Remove the roam command from the pending list */
			if (csrLLRemoveEntry(list, entry, LL_ACCESS_NOLOCK))
				csrLLInsertTail(&local_list,
					entry, LL_ACCESS_NOLOCK);
		}
		entry = next_entry;
	}
	csrLLUnlock(list);

	while ((entry = csrLLRemoveHead(&local_list, LL_ACCESS_NOLOCK))) {
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/* Tell caller that the command is cancelled */
		csrRoamCallCallback(mac_ctx, dup_cmd->sessionId, NULL,
				dup_cmd->u.roamCmd.roamId,
				eCSR_ROAM_CANCELLED, eCSR_ROAM_RESULT_NONE);
		csrReleaseCommandRoam(mac_ctx, dup_cmd);
	}
	csrLLClose(&local_list);
}

/**
 * csrRoamRemoveDuplicateCommand()- Remove duplicate roam cmd
 * from pending lists.
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @command: cmd to be removed, can be null
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the sme and roam pending list.
 *
 * Return: void
 */
void csrRoamRemoveDuplicateCommand(tpAniSirGlobal mac_ctx,
			tANI_U32 session_id, tSmeCmd *command,
			eCsrRoamReason roam_reason)
{
	/* Always lock active list before locking pending lists */
	csrLLLock(&mac_ctx->sme.smeCmdActiveList);
	csr_roam_remove_duplicate_cmd_from_list(mac_ctx,
		session_id, &mac_ctx->sme.smeCmdPendingList,
		command, roam_reason);
	csr_roam_remove_duplicate_cmd_from_list(mac_ctx,
		session_id, &mac_ctx->roam.roamCmdPendingList,
		command, roam_reason);
	csrLLUnlock(&mac_ctx->sme.smeCmdActiveList);
}
eHalStatus csrRoamCallCallback(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo,
                               tANI_U32 roamId, eRoamCmdStatus u1, eCsrRoamResult u2)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    tANI_U32 rssi = 0;
    WLAN_VOS_DIAG_EVENT_DEF(connectionStatus, vos_event_wlan_status_payload_type);
#endif
    tCsrRoamSession *pSession;
    tDot11fBeaconIEs *beacon_ies = NULL;
    tANI_U8 chan1, chan2;
    ePhyChanBondState phy_state;

    if( CSR_IS_SESSION_VALID( pMac, sessionId) )
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );
    }
    else
    {
       smsLog(pMac, LOGE, "Session ID:%d is not valid", sessionId);
       VOS_ASSERT(0);
       return eHAL_STATUS_FAILURE;
    }

    if (eANI_BOOLEAN_FALSE == pSession->sessionActive)
    {
        smsLog(pMac, LOG1, "%s Session is not Active", __func__);
        return eHAL_STATUS_FAILURE;
    }

    smsLog(pMac, LOG4, "Received RoamCmdStatus %d with Roam Result %d", u1, u2);

    if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1 &&
        eCSR_ROAM_RESULT_ASSOCIATED == u2 && pRoamInfo)
    {
        smsLog(pMac, LOGW, " Assoc complete result = %d statusCode = %d reasonCode = %d", u2, pRoamInfo->statusCode, pRoamInfo->reasonCode);

        beacon_ies = vos_mem_malloc(sizeof(tDot11fBeaconIEs));

        if ((NULL != beacon_ies) && (NULL != pRoamInfo->pBssDesc)) {
            status = csrParseBssDescriptionIEs((tHalHandle)pMac,
                                               pRoamInfo->pBssDesc,
                                               beacon_ies);

            /* now extract the phymode and center frequencies */

            /* get the VHT OPERATION IE */
            if (beacon_ies->VHTOperation.present) {

                chan1 = beacon_ies->VHTOperation.chanCenterFreqSeg1;
                chan2 = beacon_ies->VHTOperation.chanCenterFreqSeg2;
                pRoamInfo->chan_info.info = MODE_11AC_VHT80;

            } else if (beacon_ies->HTInfo.present) {

                if (beacon_ies->HTInfo.recommendedTxWidthSet == eHT_CHANNEL_WIDTH_40MHZ) {
                    phy_state = beacon_ies->HTInfo.secondaryChannelOffset;
                    if (phy_state == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)

                        chan1 = beacon_ies->HTInfo.primaryChannel +
                            CSR_CB_CENTER_CHANNEL_OFFSET;
                    else if (phy_state == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
                        chan1 = beacon_ies->HTInfo.primaryChannel -
                            CSR_CB_CENTER_CHANNEL_OFFSET;
                    else
                        chan1 = beacon_ies->HTInfo.primaryChannel;
                    pRoamInfo->chan_info.info = MODE_11NA_HT40;
                } else {
                    chan1 = beacon_ies->HTInfo.primaryChannel;
                    pRoamInfo->chan_info.info = MODE_11NA_HT20;
                }
                chan2 = 0;
            } else {
                chan1 = 0;
                chan2 = 0;
                pRoamInfo->chan_info.info = MODE_11A;
            }

            if (0 != chan1)
                pRoamInfo->chan_info.band_center_freq1 =
                                        vos_chan_to_freq(chan1);
            else
                pRoamInfo->chan_info.band_center_freq1 = 0;

            if (0 != chan2)
                pRoamInfo->chan_info.band_center_freq2 =
                                        vos_chan_to_freq(chan2);
            else
                pRoamInfo->chan_info.band_center_freq2 = 0;
        }
        else {
            pRoamInfo->chan_info.band_center_freq1 = 0;
            pRoamInfo->chan_info.band_center_freq2 = 0;
            pRoamInfo->chan_info.info = 0;
        }
        pRoamInfo->chan_info.chan_id = pRoamInfo->u.pConnectedProfile->operationChannel;
        pRoamInfo->chan_info.mhz = vos_chan_to_freq(pRoamInfo->chan_info.chan_id);
        pRoamInfo->chan_info.reg_info_1 =
            (csrGetCfgMaxTxPower(pMac, pRoamInfo->chan_info.chan_id) << 16);
        pRoamInfo->chan_info.reg_info_2 =
            (csrGetCfgMaxTxPower(pMac, pRoamInfo->chan_info.chan_id) << 8);
        vos_mem_free(beacon_ies);
    } else if ((u1 == eCSR_ROAM_FT_REASSOC_FAILED) &&
                                            (pSession->bRefAssocStartCnt)) {
        /*
         * Decrement bRefAssocStartCnt for FT reassoc failure.
         * Reason: For FT reassoc failures, we first call
         * csrRoamCallCallback before notifying a failed roam
         * completion through csrRoamComplete. The latter in
         * turn calls csrRoamProcessResults which tries to
         * once again call csrRoamCallCallback if bRefAssocStartCnt
         * is non-zero. Since this is redundant for FT reassoc
         * failure, decrement bRefAssocStartCnt.
         */
        pSession->bRefAssocStartCnt--;
    } else if (u1 == eCSR_ROAM_SET_CHANNEL_RSP && u2 ==
                                    eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS) {
        pSession->connectedProfile.operationChannel =
                          pRoamInfo->channelChangeRespEvent->newChannelNumber;
    }

    if(NULL != pSession->callback)
    {
        if( pRoamInfo )
        {
            pRoamInfo->sessionId = (tANI_U8)sessionId;
            /*
             * the reasonCode will be passed to supplicant by
             * cfg80211_disconnected. Based on the document, the reason code
             * passed to supplicant needs to set to 0 if unknown.
             * eSIR_BEACON_MISSED reason code is not recognizable so that
             * we set to 0 instead.
             */
            pRoamInfo->reasonCode =
                    (pRoamInfo->reasonCode == eSIR_BEACON_MISSED) ?
                    0 : pRoamInfo->reasonCode;
        }
        /* avoid holding the global lock when making the roaming callback, original change came
        from a raised CR (CR304874).  Since this callback is in HDD a potential deadlock
        is possible on other OS ports where the callback may need to take locks to protect
        HDD state
         UPDATE : revert this change but keep the comments here. Need to revisit as there are callbacks
         that may actually depend on the lock being held */
        // TODO: revisit: sme_ReleaseGlobalLock( &pMac->sme );
        status = pSession->callback(pSession->pContext, pRoamInfo, roamId, u1, u2);
        // TODO: revisit: sme_AcquireGlobalLock( &pMac->sme );
    }
    //EVENT_WLAN_STATUS: eCSR_ROAM_ASSOCIATION_COMPLETION,
    //                   eCSR_ROAM_LOSTLINK, eCSR_ROAM_DISASSOCIATED,
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    vos_mem_set(&connectionStatus,
                sizeof(vos_event_wlan_status_payload_type), 0);

    if((eCSR_ROAM_ASSOCIATION_COMPLETION == u1) && (eCSR_ROAM_RESULT_ASSOCIATED == u2) && pRoamInfo)
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_CONNECT;
       connectionStatus.bssType = pRoamInfo->u.pConnectedProfile->BSSType;

       if(NULL != pRoamInfo->pBssDesc)
       {
          connectionStatus.rssi = pRoamInfo->pBssDesc->rssi * (-1);
          connectionStatus.channel = pRoamInfo->pBssDesc->channelId;
       }
       if (ccmCfgSetInt(pMac, WNI_CFG_CURRENT_RSSI, connectionStatus.rssi, NULL,
                        eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE) {
          smsLog(pMac, LOGE, "Could not pass WNI_CFG_CURRENT_RSSI to Cfg");
       }

       connectionStatus.qosCapability = pRoamInfo->u.pConnectedProfile->qosConnection;
       connectionStatus.authType = (v_U8_t)diagAuthTypeFromCSRType(pRoamInfo->u.pConnectedProfile->AuthType);
       connectionStatus.encryptionType = (v_U8_t)diagEncTypeFromCSRType(pRoamInfo->u.pConnectedProfile->EncryptionType);
       vos_mem_copy(connectionStatus.ssid,
                    pRoamInfo->u.pConnectedProfile->SSID.ssId, 6);

       connectionStatus.reason = eCSR_REASON_UNSPECIFIED;
       vos_mem_copy(&pMac->sme.eventPayload, &connectionStatus,
                    sizeof(vos_event_wlan_status_payload_type));
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if((eCSR_ROAM_MIC_ERROR_IND == u1) || (eCSR_ROAM_RESULT_MIC_FAILURE == u2))
    {
       vos_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
                    sizeof(vos_event_wlan_status_payload_type));
       if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_CURRENT_RSSI, &rssi)))
           connectionStatus.rssi = rssi;

       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_MIC_ERROR;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_FORCED == u2)
    {
       vos_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
                    sizeof(vos_event_wlan_status_payload_type));
       if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_CURRENT_RSSI, &rssi)))
           connectionStatus.rssi = rssi;

       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_USER_REQUESTED;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_DISASSOC_IND == u2)
    {
       vos_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
                    sizeof(vos_event_wlan_status_payload_type));
       if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_CURRENT_RSSI, &rssi)))
           connectionStatus.rssi = rssi;

       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_DISASSOC;
       if(pRoamInfo)
           connectionStatus.reasonDisconnect = pRoamInfo->reasonCode;

       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_DEAUTH_IND == u2)
    {
       vos_mem_copy(&connectionStatus, &pMac->sme.eventPayload,
                    sizeof(vos_event_wlan_status_payload_type));
       if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_CURRENT_RSSI, &rssi)))
           connectionStatus.rssi = rssi;

       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_DEAUTH;
       if(pRoamInfo)
           connectionStatus.reasonDisconnect = pRoamInfo->reasonCode;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR

    return (status);
}

// Returns whether handoff is currently in progress or not
tANI_BOOLEAN csrRoamIsHandoffInProgress(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIsHandoffInProgress(pMac, sessionId);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}

eHalStatus csrRoamIssueDisassociate( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                     eCsrRoamSubState NewSubstate, tANI_BOOLEAN fMICFailure )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrBssid bssId = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tANI_U16 reasonCode;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if ( fMICFailure )
    {
        reasonCode = eSIR_MAC_MIC_FAILURE_REASON;
    }
    else if (NewSubstate == eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF)
    {
        reasonCode = eSIR_MAC_DISASSOC_DUE_TO_FTHANDOFF_REASON;
    }
    else if (eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT == NewSubstate)
    {
        reasonCode = eSIR_MAC_DISASSOC_LEAVING_BSS_REASON;
        NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
            FL("set to reason code eSIR_MAC_DISASSOC_LEAVING_BSS_REASON"
            " and set back NewSubstate"));
    }
    else
    {
        reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;
    }
#ifdef WLAN_FEATURE_VOWIFI_11R
    if ( (csrRoamIsHandoffInProgress(pMac, sessionId)) &&
         (NewSubstate != eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF))
    {
        tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
                                        &pMac->roam.neighborRoamInfo[sessionId];
        vos_mem_copy(&bssId,
                     pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.bssid,
                     sizeof(tSirMacAddr));
    }
    else
#endif
    if(pSession->pConnectBssDesc)
    {
        vos_mem_copy(&bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }

        smsLog(pMac, LOG2, FL("CSR Attempting to Disassociate Bssid="MAC_ADDRESS_STR
                              " subState = %s reason=%d"),
                              MAC_ADDR_ARRAY(bssId), macTraceGetcsrRoamSubState(NewSubstate),
                              reasonCode);

    csrRoamSubstateChange( pMac, NewSubstate, sessionId);

    status = csrSendMBDisassocReqMsg( pMac, sessionId, bssId, reasonCode );

    if(HAL_STATUS_SUCCESS(status))
    {
        csrRoamLinkDown(pMac, sessionId);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
        //no need to tell QoS that we are disassociating, it will be taken care off in assoc req for HO
        if(eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF != NewSubstate)
        {
            //notify QoS module that disassoc happening
            sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_DISCONNECT_REQ, NULL);
        }
#endif
    }
    else
    {
        smsLog(pMac, LOGW, FL("csrSendMBDisassocReqMsg failed with status %d"), status);
    }

    return (status);
}

/* ---------------------------------------------------------------------------
    \fn csrRoamIssueDisassociateStaCmd
    \brief csr function that HDD calls to disassociate a associated station
    \param sessionId    - session Id for Soft AP
    \param pPeerMacAddr - MAC of associated station to delete
    \param reason - reason code, be one of the tSirMacReasonCodes
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueDisassociateStaCmd( tpAniSirGlobal pMac,
                                           tANI_U32 sessionId,
                                           struct tagCsrDelStaParams
                                           *pDelStaParams)

{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    do
    {
        pCommand = csrGetCommandBuffer( pMac );
        if ( !pCommand )
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.roamReason = eCsrForcedDisassocSta;
        vos_mem_copy(pCommand->u.roamCmd.peerMac, pDelStaParams->peerMacAddr,
                     sizeof(tSirMacAddr));
        pCommand->u.roamCmd.reason =
                    (tSirMacReasonCodes)pDelStaParams->reason_code;
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_FALSE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }while(0);

    return status;
}


/* ---------------------------------------------------------------------------
    \fn csrRoamIssueDeauthSta
    \brief csr function that HDD calls to delete a associated station
    \param sessionId    - session Id for Soft AP
    \param pDelStaParams- Pointer to parameters of the station to deauthenticate
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueDeauthStaCmd( tpAniSirGlobal pMac,
                                     tANI_U32 sessionId,
                                     struct tagCsrDelStaParams *pDelStaParams)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    do
    {
        pCommand = csrGetCommandBuffer( pMac );
        if ( !pCommand )
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.roamReason = eCsrForcedDeauthSta;
        vos_mem_copy(pCommand->u.roamCmd.peerMac, pDelStaParams->peerMacAddr,
                     sizeof(tSirMacAddr));
        pCommand->u.roamCmd.reason =
                    (tSirMacReasonCodes)pDelStaParams->reason_code;
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_FALSE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }while(0);

    return status;
}
eHalStatus
csrRoamIssueTkipCounterMeasures( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    tANI_BOOLEAN bEnable )
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tCsrBssid bssId = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (!pSession)
    {
        smsLog( pMac, LOGE, "csrRoamIssueTkipCounterMeasures:CSR Session not found");
        return (status);
    }
    if (pSession->pConnectBssDesc)
    {
        vos_mem_copy(&bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamIssueTkipCounterMeasures:Connected BSS Description in CSR Session not found");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR issuing tkip counter measures for Bssid = "MAC_ADDRESS_STR", Enable = %d",
                  MAC_ADDR_ARRAY(bssId), bEnable);
    status = csrSendMBTkipCounterMeasuresReqMsg( pMac, sessionId, bEnable, bssId );
    return (status);
}
eHalStatus
csrRoamGetAssociatedStas( tpAniSirGlobal pMac, tANI_U32 sessionId,
                            VOS_MODULE_ID modId,  void *pUsrContext,
                            void *pfnSapEventCallback, v_U8_t *pAssocStasBuf )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrBssid bssId = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (!pSession)
    {
        smsLog( pMac, LOGE, "csrRoamGetAssociatedStas:CSR Session not found");
        return (status);
    }
    if(pSession->pConnectBssDesc)
    {
        vos_mem_copy(&bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamGetAssociatedStas:Connected BSS Description in CSR Session not found");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR getting associated stations for Bssid = "MAC_ADDRESS_STR,
                  MAC_ADDR_ARRAY(bssId));
    status = csrSendMBGetAssociatedStasReqMsg( pMac, sessionId, modId, bssId, pUsrContext, pfnSapEventCallback, pAssocStasBuf );
    return (status);
}
eHalStatus
csrRoamGetWpsSessionOverlap( tpAniSirGlobal pMac, tANI_U32 sessionId,
                             void *pUsrContext, void *pfnSapEventCallback, v_MACADDR_t pRemoveMac )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrBssid bssId = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if (!pSession)
    {
        smsLog( pMac, LOGE, "csrRoamGetWpsSessionOverlap:CSR Session not found");
        return (status);
    }
    if(pSession->pConnectBssDesc)
    {
        vos_mem_copy(&bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamGetWpsSessionOverlap:Connected BSS Description in CSR Session not found");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR getting WPS Session Overlap for Bssid = "MAC_ADDRESS_STR,
                  MAC_ADDR_ARRAY(bssId));

    status = csrSendMBGetWPSPBCSessions( pMac, sessionId, bssId, pUsrContext, pfnSapEventCallback, pRemoveMac);

    return (status);
}
eHalStatus csrRoamIssueDeauth( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamSubState NewSubstate )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrBssid bssId = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if (!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pSession->pConnectBssDesc)
    {
        vos_mem_copy(&bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    smsLog( pMac, LOG2, "CSR Attempting to Deauth Bssid= "MAC_ADDRESS_STR,
                  MAC_ADDR_ARRAY(bssId));
    csrRoamSubstateChange( pMac, NewSubstate, sessionId);

    status = csrSendMBDeauthReqMsg( pMac, sessionId, bssId, eSIR_MAC_DEAUTH_LEAVING_BSS_REASON );
    if (HAL_STATUS_SUCCESS(status))
        csrRoamLinkDown(pMac, sessionId);
    else
    {
        smsLog(pMac, LOGE, FL("csrSendMBDeauthReqMsg failed with status %d Session ID: %d"
                                MAC_ADDRESS_STR ), status, sessionId, MAC_ADDR_ARRAY(bssId));
    }

    return (status);
}

eHalStatus csrRoamSaveConnectedBssDesc( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDesc )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U32 size;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    // If no BSS description was found in this connection (happens with start IBSS), then
    // nix the BSS description that we keep around for the connected BSS) and get out...
    if(NULL == pBssDesc)
    {
        csrFreeConnectBssDesc(pMac, sessionId);
    }
    else
    {
        size = pBssDesc->length + sizeof( pBssDesc->length );
        if(NULL != pSession->pConnectBssDesc)
        {
            if(((pSession->pConnectBssDesc->length) + sizeof(pSession->pConnectBssDesc->length)) < size)
            {
                //not enough room for the new BSS, pMac->roam.pConnectBssDesc is freed inside
                csrFreeConnectBssDesc(pMac, sessionId);
            }
        }
        if(NULL == pSession->pConnectBssDesc)
        {
            pSession->pConnectBssDesc = vos_mem_malloc(size);
        }
        if (NULL == pSession->pConnectBssDesc)
            status = eHAL_STATUS_FAILURE;
        else
            vos_mem_copy(pSession->pConnectBssDesc, pBssDesc, size);
     }
    return (status);
}

eHalStatus csrRoamPrepareBssConfig(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,
                                    tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig,
                                    tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    eCsrCfgDot11Mode cfgDot11Mode;
    VOS_ASSERT( pIes != NULL );
    if (pIes == NULL)
        return eHAL_STATUS_FAILURE;

    do
    {
        vos_mem_copy(&pBssConfig->BssCap, &pBssDesc->capabilityInfo,
                     sizeof(tSirMacCapabilityInfo));
        //get qos
        pBssConfig->qosType = csrGetQoSFromBssDesc(pMac, pBssDesc, pIes);
        //get SSID
        if(pIes->SSID.present)
        {
            vos_mem_copy(&pBssConfig->SSID.ssId, pIes->SSID.ssid, pIes->SSID.num_ssid);
            pBssConfig->SSID.length = pIes->SSID.num_ssid;
        }
        else
            pBssConfig->SSID.length = 0;
        if(csrIsNULLSSID(pBssConfig->SSID.ssId, pBssConfig->SSID.length))
        {
            smsLog(pMac, LOGW, "BSS desc SSID is a wild card");
            //Return failed if profile doesn't have an SSID either.
            if(pProfile->SSIDs.numOfSSIDs == 0)
            {
                smsLog(pMac, LOGW, "  Both BSS desc and profile doesn't have SSID");
                status = eHAL_STATUS_FAILURE;
                break;
            }
        }
        if(CSR_IS_CHANNEL_5GHZ(pBssDesc->channelId))
        {
            pBssConfig->eBand = eCSR_BAND_5G;
        }
        else
        {
            pBssConfig->eBand = eCSR_BAND_24;
        }
        //phymode
        if(csrIsPhyModeMatch( pMac, pProfile->phyMode, pBssDesc, pProfile, &cfgDot11Mode, pIes ))
        {
            pBssConfig->uCfgDot11Mode = cfgDot11Mode;
        }
        else
        {
            smsLog(pMac, LOGW, "   Can not find match phy mode");
            //force it
            if(eCSR_BAND_24 == pBssConfig->eBand)
            {
                pBssConfig->uCfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
            }
            else
            {
                pBssConfig->uCfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
            }
        }
        //Qos
        if ((pBssConfig->uCfgDot11Mode != eCSR_CFG_DOT11_MODE_11N) &&
                (pMac->roam.configParam.WMMSupportMode == eCsrRoamWmmNoQos))
        {
            //Joining BSS is not 11n capable and WMM is disabled on client.
            //Disable QoS and WMM
            pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
        }

        if (((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N)  ||
                         (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC)) &&
                         ((pBssConfig->qosType != eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP) ||
                          (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_HCF) ||
                          (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_eDCF) ))
        {
            //Joining BSS is 11n capable and WMM is disabled on AP.
            //Assume all HT AP's are QOS AP's and enable WMM
            pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
        }

        //auth type
        switch( pProfile->negotiatedAuthType )
        {
            default:
            case eCSR_AUTH_TYPE_WPA:
            case eCSR_AUTH_TYPE_WPA_PSK:
            case eCSR_AUTH_TYPE_WPA_NONE:
            case eCSR_AUTH_TYPE_OPEN_SYSTEM:
                pBssConfig->authType = eSIR_OPEN_SYSTEM;
                break;
            case eCSR_AUTH_TYPE_SHARED_KEY:
                pBssConfig->authType = eSIR_SHARED_KEY;
                break;
            case eCSR_AUTH_TYPE_AUTOSWITCH:
                pBssConfig->authType = eSIR_AUTO_SWITCH;
                break;
        }
        //short slot time
        if( eCSR_CFG_DOT11_MODE_11B != cfgDot11Mode )
        {
            pBssConfig->uShortSlotTime = pMac->roam.configParam.shortSlotTime;
        }
        else
        {
            pBssConfig->uShortSlotTime = 0;
        }
        if(pBssConfig->BssCap.ibss)
        {
            //We don't support 11h on IBSS
            pBssConfig->f11hSupport = eANI_BOOLEAN_FALSE;
        }
        else
        {
            pBssConfig->f11hSupport = pMac->roam.configParam.Is11hSupportEnabled;
        }
        //power constraint
        pBssConfig->uPowerLimit = csrGet11hPowerConstraint(pMac, &pIes->PowerConstraints);
        //heartbeat
        if ( CSR_IS_11A_BSS( pBssDesc ) )
        {
             pBssConfig->uHeartBeatThresh = pMac->roam.configParam.HeartbeatThresh50;
        }
        else
        {
             pBssConfig->uHeartBeatThresh = pMac->roam.configParam.HeartbeatThresh24;
        }
        //Join timeout
        // if we find a BeaconInterval in the BssDescription, then set the Join Timeout to
        // be 10 x the BeaconInterval.
        if ( pBssDesc->beaconInterval )
        {
            //Make sure it is bigger than the minimal
            pBssConfig->uJoinTimeOut = CSR_ROAM_MAX(10 * pBssDesc->beaconInterval, CSR_JOIN_FAILURE_TIMEOUT_MIN);
        }
        else
        {
            pBssConfig->uJoinTimeOut = CSR_JOIN_FAILURE_TIMEOUT_DEFAULT;
        }
        //validate CB
        pBssConfig->cbMode = csrGetCBModeFromIes(pMac, pBssDesc->channelId, pIes);
    }while(0);
    return (status);
}

static eHalStatus csrRoamPrepareBssConfigFromProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,
                                                     tBssConfigParam *pBssConfig, tSirBssDescription *pBssDesc)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U8 operationChannel = 0;
    tANI_U8 qAPisEnabled = FALSE;
    //SSID
    pBssConfig->SSID.length = 0;
    if(pProfile->SSIDs.numOfSSIDs)
    {
        //only use the first one
        vos_mem_copy(&pBssConfig->SSID, &pProfile->SSIDs.SSIDList[0].SSID,
                     sizeof(tSirMacSSid));
    }
    else
    {
        //SSID must present
        return eHAL_STATUS_FAILURE;
    }
    /* Setting up the capabilities */
    if( csrIsBssTypeIBSS(pProfile->BSSType) )
    {
        pBssConfig->BssCap.ibss = 1;
    }
    else
    {
        pBssConfig->BssCap.ess = 1;
    }
    if( eCSR_ENCRYPT_TYPE_NONE != pProfile->EncryptionType.encryptionType[0] )
    {
        pBssConfig->BssCap.privacy = 1;
    }
    pBssConfig->eBand = pMac->roam.configParam.eBand;
    //phymode
    if(pProfile->ChannelInfo.ChannelList)
    {
       operationChannel = pProfile->ChannelInfo.ChannelList[0];
    }
    pBssConfig->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, pProfile, operationChannel,
                                        &pBssConfig->eBand);
    //QOS
    //Is this correct to always set to this //***
    if ( pBssConfig->BssCap.ess == 1 )
    {
        /*For Softap case enable WMM*/
        if(CSR_IS_INFRA_AP(pProfile) && (eCsrRoamWmmNoQos != pMac->roam.configParam.WMMSupportMode )){
             qAPisEnabled = TRUE;
        }
        else
        if (csrRoamGetQosInfoFromBss(pMac, pBssDesc) == eHAL_STATUS_SUCCESS) {
             qAPisEnabled = TRUE;
        } else {
             qAPisEnabled = FALSE;
        }
    } else {
             qAPisEnabled = TRUE;
    }
    if ((eCsrRoamWmmNoQos != pMac->roam.configParam.WMMSupportMode &&
         qAPisEnabled) ||
        ((eCSR_CFG_DOT11_MODE_11N == pBssConfig->uCfgDot11Mode &&
         qAPisEnabled))) {
        pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
    } else {
        pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
    }

    //auth type
    switch( pProfile->AuthType.authType[0] ) //Take the preferred Auth type.
    {
        default:
        case eCSR_AUTH_TYPE_WPA:
        case eCSR_AUTH_TYPE_WPA_PSK:
        case eCSR_AUTH_TYPE_WPA_NONE:
        case eCSR_AUTH_TYPE_OPEN_SYSTEM:
            pBssConfig->authType = eSIR_OPEN_SYSTEM;
            break;
        case eCSR_AUTH_TYPE_SHARED_KEY:
            pBssConfig->authType = eSIR_SHARED_KEY;
            break;
        case eCSR_AUTH_TYPE_AUTOSWITCH:
            pBssConfig->authType = eSIR_AUTO_SWITCH;
            break;
    }
    //short slot time
    if( WNI_CFG_PHY_MODE_11B != pBssConfig->uCfgDot11Mode )
    {
        pBssConfig->uShortSlotTime = pMac->roam.configParam.shortSlotTime;
    }
    else
    {
        pBssConfig->uShortSlotTime = 0;
    }
    //power constraint. We don't support 11h on IBSS
    pBssConfig->f11hSupport = eANI_BOOLEAN_FALSE;
    pBssConfig->uPowerLimit = 0;
    //heartbeat
    if ( eCSR_BAND_5G == pBssConfig->eBand )
    {
        pBssConfig->uHeartBeatThresh = pMac->roam.configParam.HeartbeatThresh50;
    }
    else
    {
        pBssConfig->uHeartBeatThresh = pMac->roam.configParam.HeartbeatThresh24;
    }
    //Join timeout
    pBssConfig->uJoinTimeOut = CSR_JOIN_FAILURE_TIMEOUT_DEFAULT;

    return (status);
}
static eHalStatus csrRoamGetQosInfoFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBssDesc)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tDot11fBeaconIEs *pIes = NULL;

  do
   {
      if(!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIes)))
      {
         //err msg
         VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "csrRoamGetQosInfoFromBss() failed");
         break;
      }
      //check if the AP is QAP & it supports APSD
      if( CSR_IS_QOS_BSS(pIes) )
      {
         status = eHAL_STATUS_SUCCESS;
      }
   } while (0);

   if (NULL != pIes)
   {
       vos_mem_free(pIes);
   }

   return status;
}

void csrSetCfgPrivacy( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile, tANI_BOOLEAN fPrivacy )
{
    // !! Note:  the only difference between this function and the csrSetCfgPrivacyFromProfile() is the
    // setting of the privacy CFG based on the advertised privacy setting from the AP for WPA associations.
    // See !!Note: below in this function...
    tANI_U32 PrivacyEnabled = 0;
    tANI_U32 RsnEnabled = 0;
    tANI_U32 WepDefaultKeyId = 0;
    tANI_U32 WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_5;   /* default 40 bits */
    tANI_U32 Key0Length = 0;
    tANI_U32 Key1Length = 0;
    tANI_U32 Key2Length = 0;
    tANI_U32 Key3Length = 0;

    // Reserve for the biggest key
    tANI_U8 Key0[ WNI_CFG_WEP_DEFAULT_KEY_1_LEN ];
    tANI_U8 Key1[ WNI_CFG_WEP_DEFAULT_KEY_2_LEN ];
    tANI_U8 Key2[ WNI_CFG_WEP_DEFAULT_KEY_3_LEN ];
    tANI_U8 Key3[ WNI_CFG_WEP_DEFAULT_KEY_4_LEN ];

    switch ( pProfile->negotiatedUCEncryptionType )
    {
        case eCSR_ENCRYPT_TYPE_NONE:

            // for NO encryption, turn off Privacy and Rsn.
            PrivacyEnabled = 0;
            RsnEnabled = 0;

            // WEP key length and Wep Default Key ID don't matter in this case....

            // clear out the WEP keys that may be hanging around.
            Key0Length = 0;
            Key1Length = 0;
            Key2Length = 0;
            Key3Length = 0;

            break;

        case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP40:

            // Privacy is ON.  NO RSN for Wep40 static key.
            PrivacyEnabled = 1;
            RsnEnabled = 0;

            // Set the Wep default key ID.
            WepDefaultKeyId = pProfile->Keys.defaultIndex;
            // Wep key size if 5 bytes (40 bits).
            WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_5;

            // set encryption keys in the CFG database or clear those that are not present in this profile.
            if ( pProfile->Keys.KeyLength[0] )
            {
                vos_mem_copy(Key0, pProfile->Keys.KeyMaterial[0],
                             WNI_CFG_WEP_KEY_LENGTH_5);
                Key0Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key0Length = 0;
            }

            if ( pProfile->Keys.KeyLength[1] )
            {
               vos_mem_copy(Key1, pProfile->Keys.KeyMaterial[1],
                            WNI_CFG_WEP_KEY_LENGTH_5);
               Key1Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key1Length = 0;
            }

            if ( pProfile->Keys.KeyLength[2] )
            {
                vos_mem_copy(Key2, pProfile->Keys.KeyMaterial[2],
                             WNI_CFG_WEP_KEY_LENGTH_5);
                Key2Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key2Length = 0;
            }

            if ( pProfile->Keys.KeyLength[3] )
            {
                vos_mem_copy(Key3, pProfile->Keys.KeyMaterial[3],
                             WNI_CFG_WEP_KEY_LENGTH_5);
                Key3Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key3Length = 0;
            }
            break;

        case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP104:

            // Privacy is ON.  NO RSN for Wep40 static key.
            PrivacyEnabled = 1;
            RsnEnabled = 0;

            // Set the Wep default key ID.
            WepDefaultKeyId = pProfile->Keys.defaultIndex;

            // Wep key size if 13 bytes (104 bits).
            WepKeyLength = WNI_CFG_WEP_KEY_LENGTH_13;

            // set encryption keys in the CFG database or clear those that are not present in this profile.
            if ( pProfile->Keys.KeyLength[0] )
            {
                vos_mem_copy(Key0, pProfile->Keys.KeyMaterial[ 0 ],
                             WNI_CFG_WEP_KEY_LENGTH_13);
                Key0Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key0Length = 0;
            }

            if ( pProfile->Keys.KeyLength[1] )
            {
                vos_mem_copy(Key1, pProfile->Keys.KeyMaterial[ 1 ],
                             WNI_CFG_WEP_KEY_LENGTH_13);
                Key1Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key1Length = 0;
            }

            if ( pProfile->Keys.KeyLength[2] )
            {
                vos_mem_copy(Key2, pProfile->Keys.KeyMaterial[ 2 ],
                             WNI_CFG_WEP_KEY_LENGTH_13);
                Key2Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key2Length = 0;
            }

            if ( pProfile->Keys.KeyLength[3] )
            {
                vos_mem_copy(Key3, pProfile->Keys.KeyMaterial[ 3 ],
                             WNI_CFG_WEP_KEY_LENGTH_13);
                Key3Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key3Length = 0;
            }

            break;

        case eCSR_ENCRYPT_TYPE_TKIP:
        case eCSR_ENCRYPT_TYPE_AES:
#ifdef FEATURE_WLAN_WAPI
        case eCSR_ENCRYPT_TYPE_WPI:
#endif /* FEATURE_WLAN_WAPI */
            // !! Note:  this is the only difference between this function and the csrSetCfgPrivacyFromProfile()
            // (setting of the privacy CFG based on the advertised privacy setting from the AP for WPA/WAPI associations ).
            PrivacyEnabled = (0 != fPrivacy);

            // turn on RSN enabled for WPA associations
            RsnEnabled = 1;

            // WEP key length and Wep Default Key ID don't matter in this case....

            // clear out the static WEP keys that may be hanging around.
            Key0Length = 0;
            Key1Length = 0;
            Key2Length = 0;
            Key3Length = 0;

            break;
        default:
            PrivacyEnabled = 0;
            RsnEnabled = 0;
            break;
    }

    ccmCfgSetInt(pMac, WNI_CFG_PRIVACY_ENABLED, PrivacyEnabled, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_RSN_ENABLED, RsnEnabled, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_WEP_DEFAULT_KEY_1, Key0, Key0Length, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_WEP_DEFAULT_KEY_2, Key1, Key1Length, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_WEP_DEFAULT_KEY_3, Key2, Key2Length, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_WEP_DEFAULT_KEY_4, Key3, Key3Length, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_WEP_KEY_LENGTH, WepKeyLength, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_WEP_DEFAULT_KEYID, WepDefaultKeyId, NULL, eANI_BOOLEAN_FALSE);
}

static void csrSetCfgSsid( tpAniSirGlobal pMac, tSirMacSSid *pSSID )
{
    tANI_U32 len = 0;
    if(pSSID->length <= WNI_CFG_SSID_LEN)
    {
        len = pSSID->length;
    }
    ccmCfgSetStr(pMac, WNI_CFG_SSID, (tANI_U8 *)pSSID->ssId, len, NULL, eANI_BOOLEAN_FALSE);
}

eHalStatus csrSetQosToCfg( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrMediaAccessType qosType )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 QoSEnabled;
    tANI_U32 WmeEnabled;
    // set the CFG enable/disable variables based on the qosType being configured...
    switch( qosType )
    {
        case eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p:
            QoSEnabled = FALSE;
            WmeEnabled = TRUE;
            break;
        case eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP:
            QoSEnabled = FALSE;
            WmeEnabled = TRUE;
            break;
        case eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify:
            QoSEnabled = FALSE;
            WmeEnabled = TRUE;
            break;
        case eCSR_MEDIUM_ACCESS_11e_eDCF:
            QoSEnabled = TRUE;
            WmeEnabled = FALSE;
            break;
        case eCSR_MEDIUM_ACCESS_11e_HCF:
            QoSEnabled = TRUE;
            WmeEnabled = FALSE;
            break;
        default:
        case eCSR_MEDIUM_ACCESS_DCF:
            QoSEnabled = FALSE;
            WmeEnabled = FALSE;
            break;
    }
    //save the WMM setting for later use
    pMac->roam.roamSession[sessionId].fWMMConnection = (tANI_BOOLEAN)WmeEnabled;
    pMac->roam.roamSession[sessionId].fQOSConnection = (tANI_BOOLEAN)QoSEnabled;
    return (status);
}
static eHalStatus csrGetRateSet( tpAniSirGlobal pMac,  tCsrRoamProfile *pProfile, eCsrPhyMode phyMode, tSirBssDescription *pBssDesc,
                           tDot11fBeaconIEs *pIes, tSirMacRateSet *pOpRateSet, tSirMacRateSet *pExRateSet)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    int i;
    eCsrCfgDot11Mode cfgDot11Mode;
    tANI_U8 *pDstRate;
    tANI_U16 rateBitmap = 0;
    vos_mem_set(pOpRateSet, sizeof(tSirMacRateSet), 0);
    vos_mem_set(pExRateSet, sizeof(tSirMacRateSet), 0);
    VOS_ASSERT( pIes != NULL );

    if( NULL != pIes )
    {
        csrIsPhyModeMatch( pMac, phyMode, pBssDesc, pProfile, &cfgDot11Mode, pIes );
        // Originally, we thought that for 11a networks, the 11a rates are always
        // in the Operational Rate set & for 11b and 11g networks, the 11b rates
        // appear in the Operational Rate set.  Consequently, in either case, we
        // would blindly put the rates we support into our Operational Rate set
        // (including the basic rates, which we have already verified are
        // supported earlier in the roaming decision).
        // However, it turns out that this is not always the case.  Some AP's
        // (e.g. D-Link DI-784) ram 11g rates into the Operational Rate set,
        // too.  Now, we're a little more careful:
        pDstRate = pOpRateSet->rate;
        if(pIes->SuppRates.present)
        {
            for ( i = 0; i < pIes->SuppRates.num_rates; i++ )
            {
                if (csrRatesIsDot11RateSupported(pMac,
                                                 pIes->SuppRates.rates[i])) {
                    if (!csrCheckRateBitmap(pIes->SuppRates.rates[i], rateBitmap)) {
                        csrAddRateBitmap(pIes->SuppRates.rates[i], &rateBitmap);
                        *pDstRate++ = pIes->SuppRates.rates[i];
                        pOpRateSet->numRates++;
                    }
                }
            }
        }
        /* If there are Extended Rates in the beacon, we will reflect those
         * extended rates that we support in out Extended Operational Rate
         * set:
         */
        pDstRate = pExRateSet->rate;
        if (pIes->ExtSuppRates.present) {
            for (i=0; i<pIes->ExtSuppRates.num_rates; i++) {
                if (csrRatesIsDot11RateSupported(pMac,
                        pIes->ExtSuppRates.rates[i])) {
                    if (!csrCheckRateBitmap(pIes->ExtSuppRates.rates[i],
                                                             rateBitmap)) {
                        *pDstRate++ = pIes->ExtSuppRates.rates[i];
                        pExRateSet->numRates++;
                    }
                }
            }
        }
    }//Parsing BSSDesc
    else
    {
        smsLog(pMac, LOGE, FL("failed to parse BssDesc"));
    }
    if (pOpRateSet->numRates > 0 || pExRateSet->numRates > 0) status = eHAL_STATUS_SUCCESS;
    return status;
}

static void csrSetCfgRateSet( tpAniSirGlobal pMac, eCsrPhyMode phyMode, tCsrRoamProfile *pProfile,
                              tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIes)
{
    int i;
    tANI_U8 *pDstRate;
    eCsrCfgDot11Mode cfgDot11Mode;
    tANI_U8 OperationalRates[ CSR_DOT11_SUPPORTED_RATES_MAX ];    // leave enough room for the max number of rates
    tANI_U32 OperationalRatesLength = 0;
    tANI_U8 ExtendedOperationalRates[ CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX ];    // leave enough room for the max number of rates
    tANI_U32 ExtendedOperationalRatesLength = 0;
    tANI_U8 MCSRateIdxSet[ SIZE_OF_SUPPORTED_MCS_SET ];
    tANI_U32 MCSRateLength = 0;
    VOS_ASSERT( pIes != NULL );
    if( NULL != pIes )
    {
        csrIsPhyModeMatch( pMac, phyMode, pBssDesc, pProfile, &cfgDot11Mode, pIes );
        // Originally, we thought that for 11a networks, the 11a rates are always
        // in the Operational Rate set & for 11b and 11g networks, the 11b rates
        // appear in the Operational Rate set.  Consequently, in either case, we
        // would blindly put the rates we support into our Operational Rate set
        // (including the basic rates, which we have already verified are
        // supported earlier in the roaming decision).
        // However, it turns out that this is not always the case.  Some AP's
        // (e.g. D-Link DI-784) ram 11g rates into the Operational Rate set,
        // too.  Now, we're a little more careful:
        pDstRate = OperationalRates;
        if(pIes->SuppRates.present)
        {
            for ( i = 0; i < pIes->SuppRates.num_rates; i++ )
            {
                if ( csrRatesIsDot11RateSupported( pMac, pIes->SuppRates.rates[ i ] ) &&
                     ( OperationalRatesLength < CSR_DOT11_SUPPORTED_RATES_MAX ))
                {
                    *pDstRate++ = pIes->SuppRates.rates[ i ];
                    OperationalRatesLength++;
                }
            }
        }
        if (eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode ||
            eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
            eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) {
            // If there are Extended Rates in the beacon, we will reflect those
            // extended rates that we support in out Extended Operational Rate
            // set:
            pDstRate = ExtendedOperationalRates;
            if(pIes->ExtSuppRates.present)
            {
                for ( i = 0; i < pIes->ExtSuppRates.num_rates; i++ )
                {
                    if ( csrRatesIsDot11RateSupported( pMac, pIes->ExtSuppRates.rates[ i ] ) &&
                     ( ExtendedOperationalRatesLength < CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX ))
                    {
                        *pDstRate++ = pIes->ExtSuppRates.rates[ i ];
                        ExtendedOperationalRatesLength++;
                    }
                }
            }
        }
        /* Get MCS Rate */
        pDstRate = MCSRateIdxSet;
        if ( pIes->HTCaps.present )
        {
           for ( i = 0; i < VALID_MAX_MCS_INDEX; i++ )
           {
              if ( (unsigned int)pIes->HTCaps.supportedMCSSet[0] & (1 << i) )
              {
                 MCSRateLength++;
                 *pDstRate++ = i;
              }
           }
        }
        // Set the operational rate set CFG variables...
        ccmCfgSetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET, OperationalRates,
                        OperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
        ccmCfgSetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET, ExtendedOperationalRates,
                            ExtendedOperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
        ccmCfgSetStr(pMac, WNI_CFG_CURRENT_MCS_SET, MCSRateIdxSet,
                        MCSRateLength, NULL, eANI_BOOLEAN_FALSE);
    }//Parsing BSSDesc
    else
    {
        smsLog(pMac, LOGE, FL("failed to parse BssDesc"));
    }
}

static void csrSetCfgRateSetFromProfile( tpAniSirGlobal pMac,
                                         tCsrRoamProfile *pProfile  )
{
    tSirMacRateSetIE DefaultSupportedRates11a = {  SIR_MAC_RATESET_EID,
                                                   { 8,
                                                     { SIR_MAC_RATE_6,
                                                   SIR_MAC_RATE_9,
                                                   SIR_MAC_RATE_12,
                                                   SIR_MAC_RATE_18,
                                                   SIR_MAC_RATE_24,
                                                   SIR_MAC_RATE_36,
                                                   SIR_MAC_RATE_48,
                                                       SIR_MAC_RATE_54  } } };
    tSirMacRateSetIE DefaultSupportedRates11b = {  SIR_MAC_RATESET_EID,
                                                   { 4,
                                                     { SIR_MAC_RATE_1,
                                                   SIR_MAC_RATE_2,
                                                   SIR_MAC_RATE_5_5,
                                                       SIR_MAC_RATE_11  } } };


    tSirMacPropRateSet DefaultSupportedPropRates = { 3,
                                                     { SIR_MAC_RATE_72,
                                                     SIR_MAC_RATE_96,
                                                       SIR_MAC_RATE_108 } };
    eCsrCfgDot11Mode cfgDot11Mode;
    eCsrBand eBand;
    tANI_U8 OperationalRates[ CSR_DOT11_SUPPORTED_RATES_MAX ];    // leave enough room for the max number of rates
    tANI_U32 OperationalRatesLength = 0;
    tANI_U8 ExtendedOperationalRates[ CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX ];    // leave enough room for the max number of rates
    tANI_U32 ExtendedOperationalRatesLength = 0;
    tANI_U8 ProprietaryOperationalRates[ 4 ];    // leave enough room for the max number of proprietary rates
    tANI_U32 ProprietaryOperationalRatesLength = 0;
    tANI_U32 PropRatesEnable = 0;
    tANI_U8 operationChannel = 0;
    if(pProfile->ChannelInfo.ChannelList)
    {
       operationChannel = pProfile->ChannelInfo.ChannelList[0];
    }
    cfgDot11Mode = csrRoamGetPhyModeBandForBss( pMac, pProfile, operationChannel, &eBand );
    // For 11a networks, the 11a rates go into the Operational Rate set.  For 11b and 11g
    // networks, the 11b rates appear in the Operational Rate set.  In either case,
    // we can blindly put the rates we support into our Operational Rate set
    // (including the basic rates, which we have already verified are supported
    // earlier in the roaming decision).
    if ( eCSR_BAND_5G == eBand )
    {
        // 11a rates into the Operational Rate Set.
        OperationalRatesLength = DefaultSupportedRates11a.supportedRateSet.numRates *
                                            sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
        vos_mem_copy(OperationalRates,
                     DefaultSupportedRates11a.supportedRateSet.rate,
                     OperationalRatesLength);

        // Nothing in the Extended rate set.
        ExtendedOperationalRatesLength = 0;
        // populate proprietary rates if user allows them
        if ( pMac->roam.configParam.ProprietaryRatesEnabled )
        {
            ProprietaryOperationalRatesLength = DefaultSupportedPropRates.numPropRates *
                                                            sizeof(*DefaultSupportedPropRates.propRate);
            vos_mem_copy(ProprietaryOperationalRates,
                         DefaultSupportedPropRates.propRate,
                         ProprietaryOperationalRatesLength);
        }
        else
        {
            // No proprietary modes
            ProprietaryOperationalRatesLength = 0;
        }
    }
    else if ( eCSR_CFG_DOT11_MODE_11B == cfgDot11Mode )
    {
        // 11b rates into the Operational Rate Set.
        OperationalRatesLength = DefaultSupportedRates11b.supportedRateSet.numRates *
                                              sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
        vos_mem_copy(OperationalRates,
                     DefaultSupportedRates11b.supportedRateSet.rate,
                     OperationalRatesLength);
        // Nothing in the Extended rate set.
        ExtendedOperationalRatesLength = 0;
        // No proprietary modes
        ProprietaryOperationalRatesLength = 0;
    }
    else
    {
        // 11G

        // 11b rates into the Operational Rate Set.
        OperationalRatesLength = DefaultSupportedRates11b.supportedRateSet.numRates *
                                            sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
        vos_mem_copy(OperationalRates,
                     DefaultSupportedRates11b.supportedRateSet.rate,
                     OperationalRatesLength);

        // 11a rates go in the Extended rate set.
        ExtendedOperationalRatesLength = DefaultSupportedRates11a.supportedRateSet.numRates *
                                                    sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
        vos_mem_copy(ExtendedOperationalRates,
                     DefaultSupportedRates11a.supportedRateSet.rate,
                     ExtendedOperationalRatesLength);

        // populate proprietary rates if user allows them
        if ( pMac->roam.configParam.ProprietaryRatesEnabled )
        {
            ProprietaryOperationalRatesLength = DefaultSupportedPropRates.numPropRates *
                                                            sizeof(*DefaultSupportedPropRates.propRate);
            vos_mem_copy(ProprietaryOperationalRates,
                         DefaultSupportedPropRates.propRate,
                         ProprietaryOperationalRatesLength);
        }
        else
        {
           // No proprietary modes
            ProprietaryOperationalRatesLength = 0;
        }
    }
    // set this to 1 if prop. rates need to be advertised in to the IBSS beacon and user wants to use them
    if ( ProprietaryOperationalRatesLength && pMac->roam.configParam.ProprietaryRatesEnabled )
    {
        PropRatesEnable = 1;
    }
    else
    {
        PropRatesEnable = 0;
    }

    // Set the operational rate set CFG variables...
    ccmCfgSetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET, OperationalRates,
                    OperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET, ExtendedOperationalRates,
                        ExtendedOperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetStr(pMac, WNI_CFG_PROPRIETARY_OPERATIONAL_RATE_SET,
                    ProprietaryOperationalRates,
                    ProprietaryOperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
}
void csrRoamCcmCfgSetCallback(tHalHandle hHal, tANI_S32 result)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    tListElem *pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    tANI_U32 sessionId;
    tSmeCmd *pCommand = NULL;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    tCsrRoamSession *pSession = NULL;
#endif
    if(NULL == pEntry)
    {
        smsLog(pMac, LOGW, "   CFG_CNF with active list empty");
        return;
    }
    pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
    sessionId = pCommand->sessionId;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    pSession = &pMac->roam.roamSession[sessionId];
    if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                "LFR3:csrRoamCcmCfgSetCallback");
    }
#endif

    if(CSR_IS_ROAM_JOINING(pMac, sessionId) && CSR_IS_ROAM_SUBSTATE_CONFIG(pMac, sessionId))
    {
        csrRoamingStateConfigCnfProcessor(pMac, (tANI_U32)result);
    }
}

//This function is very dump. It is here because PE still need WNI_CFG_PHY_MODE
tANI_U32 csrRoamGetPhyModeFromDot11Mode(eCsrCfgDot11Mode dot11Mode, eCsrBand band)
{
    if(eCSR_CFG_DOT11_MODE_11B == dot11Mode)
    {
        return (WNI_CFG_PHY_MODE_11B);
    }
    else
    {
        if(eCSR_BAND_24 == band)
            return (WNI_CFG_PHY_MODE_11G);
    }
    return (WNI_CFG_PHY_MODE_11A);
}


#ifdef WLAN_FEATURE_11AC
ePhyChanBondState csrGetHTCBStateFromVHTCBState(ePhyChanBondState aniCBMode)
{
    switch ( aniCBMode )
    {
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
            return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
            return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
        case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
        default :
            return PHY_SINGLE_CHANNEL_CENTERED;
    }
}
#endif

//pIes may be NULL
eHalStatus csrRoamSetBssConfigCfg(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                          tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig,
                          tDot11fBeaconIEs *pIes, tANI_BOOLEAN resetCountry)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32   cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
    tANI_U8    channel = 0;
    //Make sure we have the domain info for the BSS we try to connect to.
    //Do we need to worry about sequence for OSs that are not Windows??
    if (pBssDesc)
    {
        if (csrLearnCountryInformation(pMac, pBssDesc, pIes, eANI_BOOLEAN_TRUE))
        {
            //Make sure the 11d info from this BSSDesc can be applied
            pMac->scan.fAmbiguous11dInfoFound = eANI_BOOLEAN_FALSE;
            if (VOS_TRUE == resetCountry)
            {
                csrApplyCountryInformation(pMac, FALSE);
            }
            else
            {
                csrApplyCountryInformation(pMac, TRUE);
            }
        }
        if ((csrIs11dSupported (pMac)) && pIes)
        {
            if (!pIes->Country.present)
            {
                csrResetCountryInformation(pMac, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE );
            }
            else
            {
                //Let's also update the below to make sure we don't update CC while
                //connected to an AP which is advertising some CC
                vos_mem_copy(pMac->scan.currentCountryBssid,
                             pBssDesc->bssId, sizeof(tSirMacAddr));
            }
        }
    }
    //Qos
    csrSetQosToCfg( pMac, sessionId, pBssConfig->qosType );
    //SSID
    csrSetCfgSsid(pMac, &pBssConfig->SSID );

    //Auth type
    ccmCfgSetInt(pMac, WNI_CFG_AUTHENTICATION_TYPE, pBssConfig->authType, NULL, eANI_BOOLEAN_FALSE);
    //encryption type
    csrSetCfgPrivacy(pMac, pProfile, (tANI_BOOLEAN)pBssConfig->BssCap.privacy );
    //short slot time
    ccmCfgSetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, pBssConfig->uShortSlotTime, NULL, eANI_BOOLEAN_FALSE);
    //11d
    ccmCfgSetInt(pMac, WNI_CFG_11D_ENABLED,
                        ((pBssConfig->f11hSupport) ? pBssConfig->f11hSupport : pProfile->ieee80211d),
                        NULL, eANI_BOOLEAN_FALSE);
    ccmCfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, pBssConfig->uPowerLimit, NULL, eANI_BOOLEAN_FALSE);
    //CB

    if (CSR_IS_INFRA_AP(pProfile) || CSR_IS_WDS_AP(pProfile) ||
                    CSR_IS_IBSS(pProfile)) {
        channel = pProfile->operationChannel;
    }
    else
    {
        if(pBssDesc)
        {
            channel = pBssDesc->channelId;
        }
    }
    if(0 != channel)
    {
        if(CSR_IS_CHANNEL_24GHZ(channel))
        {//for now if we are on 2.4 Ghz, CB will be always disabled
            cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
        }
        else
        {
           cfgCb = pBssConfig->cbMode;
        }
    }

    //Rate
    //Fixed Rate
    if(pBssDesc)
    {
        csrSetCfgRateSet(pMac, (eCsrPhyMode)pProfile->phyMode, pProfile, pBssDesc, pIes);
    }
    else
    {
        csrSetCfgRateSetFromProfile(pMac, pProfile);
    }
    //Make this the last CFG to set. The callback will trigger a join_req
    //Join time out
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_CONFIG, sessionId );

    ccmCfgSetInt(pMac, WNI_CFG_JOIN_FAILURE_TIMEOUT, pBssConfig->uJoinTimeOut, (tCcmCfgSetCallback)csrRoamCcmCfgSetCallback, eANI_BOOLEAN_FALSE);
    return (status);
}

eHalStatus csrRoamStopNetwork( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                               tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIes)
{
    eHalStatus status;
    tBssConfigParam *pBssConfig;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    pBssConfig = vos_mem_malloc(sizeof(tBssConfigParam));
    if ( NULL == pBssConfig )
        status = eHAL_STATUS_FAILURE;
    else
    {
        vos_mem_set(pBssConfig, sizeof(tBssConfigParam), 0);
        status = csrRoamPrepareBssConfig(pMac, pProfile, pBssDesc, pBssConfig, pIes);
        if(HAL_STATUS_SUCCESS(status))
        {
            pSession->bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
            /* This will allow to pass cbMode during join req */
            pSession->bssParams.cbMode= pBssConfig->cbMode;
            //For IBSS, we need to prepare some more information
            if( csrIsBssTypeIBSS(pProfile->BSSType) || CSR_IS_WDS( pProfile )
              || CSR_IS_INFRA_AP(pProfile)
            )
            {
                csrRoamPrepareBssParams(pMac, sessionId, pProfile, pBssDesc, pBssConfig, pIes);
            }
            // If we are in an IBSS, then stop the IBSS...
            ////Not worry about WDS connection for now
            if ( csrIsConnStateIbss( pMac, sessionId ) )
            {
                status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING );
            }
            else
            {
                // if we are in an Infrastructure association....
                if ( csrIsConnStateInfra( pMac, sessionId ) )
                {
                    // and the new Bss is an Ibss OR we are roaming from Infra to Infra
                    // across SSIDs (roaming to a new SSID)...            //
                    //Not worry about WDS connection for now
                    if ( pBssDesc && ( ( csrIsIbssBssDesc( pBssDesc ) ) ||
                          !csrIsSsidEqual( pMac, pSession->pConnectBssDesc, pBssDesc, pIes ) ) )
                    {
                        // then we need to disassociate from the Infrastructure network...
                        status = csrRoamIssueDisassociate( pMac, sessionId,
                                      eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE );
                    }
                    else
                    {
                        /*
                         * In an Infrastructure and going to an Infrastructure
                         * network with the same SSID.  This calls for a
                         * Reassociation sequence.  So issue the CFG
                         * sets for this new AP.
                         */
                        if ( pBssDesc )
                        {
                            // Set parameters for this Bss.
                            status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile,
                                                                 pBssDesc, pBssConfig,
                                                             pIes, eANI_BOOLEAN_FALSE);
                        }
                    }
                }
                else
                {
                    /*
                     * Neither in IBSS nor in Infra. We can go ahead
                     * and set the CFG for the new network. Nothing to stop
                     */
                    if ( pBssDesc || CSR_IS_WDS_AP( pProfile )
                     || CSR_IS_INFRA_AP(pProfile)
                    )
                    {
                        tANI_BOOLEAN  is11rRoamingFlag = eANI_BOOLEAN_FALSE;
                        is11rRoamingFlag = csrRoamIs11rAssoc(pMac, sessionId);
                        // Set parameters for this Bss.
                        status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile,
                                                              pBssDesc, pBssConfig,
                                                              pIes, is11rRoamingFlag);
                    }
                }
            }
        }//Success getting BSS config info
        vos_mem_free(pBssConfig);
    }//Allocate memory
    return (status);
}

eCsrJoinState csrRoamJoin( tpAniSirGlobal pMac, tANI_U32 sessionId,
                           tCsrScanResultInfo *pScanResult, tCsrRoamProfile *pProfile )
{
    eCsrJoinState eRoamState = eCsrContinueRoaming;
    eHalStatus status;
    tSirBssDescription *pBssDesc = &pScanResult->BssDescriptor;
    tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *)( pScanResult->pvIes ); //This may be NULL
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return (eCsrStopRoaming);
    }

    if( CSR_IS_WDS_STA( pProfile ) )
    {
        status = csrRoamStartWds( pMac, sessionId, pProfile, pBssDesc );
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            eRoamState = eCsrStopRoaming;
        }
    }
    else
    {
        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIesLocal))) )
        {
            smsLog(pMac, LOGE, FL(" fail to parse IEs"));
            return (eCsrStopRoaming);
        }
        if ( csrIsInfraBssDesc( pBssDesc ) )
    {
        // If we are connected in infrastructure mode and the Join Bss description is for the same BssID, then we are
        // attempting to join the AP we are already connected with.  In that case, see if the Bss or Sta capabilities
        // have changed and handle the changes (without disturbing the current association).

        if ( csrIsConnStateConnectedInfra(pMac, sessionId) &&
             csrIsBssIdEqual( pMac, pBssDesc, pSession->pConnectBssDesc ) &&
                 csrIsSsidEqual( pMac, pSession->pConnectBssDesc, pBssDesc, pIesLocal )
           )
        {
            /*
             * Check to see if the Auth type has changed in the Profile.
             * If so, we don't want to Reassociate with Authenticating first.
             * To force this, stop the current association (Disassociate) and
             * then re 'Join' the AP, which will force an Authentication
             * (with the new Auth type) followed by a new Association.
             */
            if(csrIsSameProfile(pMac, &pSession->connectedProfile, pProfile))
            {
                smsLog(pMac, LOGW, FL("  detect same profile"));
                if(csrRoamIsSameProfileKeys(pMac, &pSession->connectedProfile, pProfile))
                {
                    eRoamState = eCsrReassocToSelfNoCapChange;
                }
                else
                {
                    tBssConfigParam bssConfig;
                    //The key changes
                    vos_mem_set(&bssConfig, sizeof(bssConfig), 0);
                    status = csrRoamPrepareBssConfig(pMac, pProfile, pBssDesc, &bssConfig, pIesLocal);
                    if(HAL_STATUS_SUCCESS(status))
                    {
                        pSession->bssParams.uCfgDot11Mode = bssConfig.uCfgDot11Mode;
                        pSession->bssParams.cbMode = bssConfig.cbMode;
                        //Reapply the config including Keys so reassoc is happening.
                        status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile,
                                                        pBssDesc, &bssConfig,
                                                        pIesLocal, eANI_BOOLEAN_FALSE);
                        if(!HAL_STATUS_SUCCESS(status))
                        {
                            eRoamState = eCsrStopRoaming;
                        }
                    }
                    else
                    {
                        eRoamState = eCsrStopRoaming;
                    }
                }//same profile
            }
            else
            {
                if(!HAL_STATUS_SUCCESS(csrRoamIssueDisassociate( pMac, sessionId,
                                                        eCSR_ROAM_SUBSTATE_DISASSOC_REQ, FALSE )))
                {
                    smsLog(pMac, LOGE, FL("  fail to issue disassociate with Session ID %d"),
                                              sessionId);
                    eRoamState = eCsrStopRoaming;
                }
            }
        }
        else
        {
            // note:  we used to pre-auth here with open authentication networks but that was not working so well.
            //
            // stop the existing network before attempting to join the new network...
                if(!HAL_STATUS_SUCCESS(csrRoamStopNetwork(pMac, sessionId, pProfile, pBssDesc, pIesLocal)))
            {
                eRoamState = eCsrStopRoaming;
            }
        }
        }//Infra
    else
    {
            if(!HAL_STATUS_SUCCESS(csrRoamStopNetwork(pMac, sessionId, pProfile, pBssDesc, pIesLocal)))
        {
            eRoamState = eCsrStopRoaming;
        }
    }
        if( pIesLocal && !pScanResult->pvIes )
        {
            vos_mem_free(pIesLocal);
        }
    }
    return( eRoamState );
}

eHalStatus csrRoamShouldRoam(tpAniSirGlobal pMac, tANI_U32 sessionId,
                             tSirBssDescription *pBssDesc, tANI_U32 roamId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo roamInfo;
    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
    roamInfo.pBssDesc = pBssDesc;
    status = csrRoamCallCallback(pMac, sessionId, &roamInfo, roamId, eCSR_ROAM_SHOULD_ROAM, eCSR_ROAM_RESULT_NONE);
    return (status);
}
//In case no matching BSS is found, use whatever default we can find
static void csrRoamAssignDefaultParam( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    //Need to get all negotiated types in place first
    //auth type
    switch( pCommand->u.roamCmd.roamProfile.AuthType.authType[0] ) //Take the preferred Auth type.
    {
        default:
        case eCSR_AUTH_TYPE_WPA:
        case eCSR_AUTH_TYPE_WPA_PSK:
        case eCSR_AUTH_TYPE_WPA_NONE:
        case eCSR_AUTH_TYPE_OPEN_SYSTEM:
             pCommand->u.roamCmd.roamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
             break;

        case eCSR_AUTH_TYPE_SHARED_KEY:
             pCommand->u.roamCmd.roamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_SHARED_KEY;
             break;

        case eCSR_AUTH_TYPE_AUTOSWITCH:
             pCommand->u.roamCmd.roamProfile.negotiatedAuthType = eCSR_AUTH_TYPE_AUTOSWITCH;
             break;
    }
    pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
    pCommand->u.roamCmd.roamProfile.EncryptionType.encryptionType[0];
    /* In this case, the multicast encryption needs to follow
       the unicast ones */
    pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
    pCommand->u.roamCmd.roamProfile.EncryptionType.encryptionType[0];
}


static void csrSetAbortRoamingCommand(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
   switch(pCommand->u.roamCmd.roamReason)
   {
   case eCsrLostLink1:
      pCommand->u.roamCmd.roamReason = eCsrLostLink1Abort;
      break;
   case eCsrLostLink2:
      pCommand->u.roamCmd.roamReason = eCsrLostLink2Abort;
      break;
   case eCsrLostLink3:
      pCommand->u.roamCmd.roamReason = eCsrLostLink3Abort;
      break;
   default:
      smsLog(pMac, LOGE, FL(" aborting roaming reason %d not recognized"),
         pCommand->u.roamCmd.roamReason);
      break;
   }
}

static eCsrJoinState csrRoamJoinNextBss( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fUseSameBss )
{
    eHalStatus status;
    tCsrScanResult *pScanResult = NULL;
    eCsrJoinState eRoamState = eCsrStopRoaming;
    tScanResultList *pBSSList = (tScanResultList *)pCommand->u.roamCmd.hBSSList;
    tANI_BOOLEAN fDone = eANI_BOOLEAN_FALSE;
    tCsrRoamInfo roamInfo, *pRoamInfo = NULL;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    v_U8_t acm_mask = 0;
#endif
    tANI_U32 sessionId = pCommand->sessionId;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tCsrRoamProfile *pProfile = &pCommand->u.roamCmd.roamProfile;
    tANI_U8  concurrentChannel = 0;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return (eCsrStopRoaming);
    }

    do
    {
        // Check for Cardbus eject condition, before trying to Roam to any BSS
        //***if( !balIsCardPresent(pAdapter) ) break;

        vos_mem_set(&roamInfo, sizeof(roamInfo), 0);
        vos_mem_copy(&roamInfo.bssid, &pSession->joinFailStatusCode.bssId,
                     sizeof(tSirMacAddr));
        if(NULL != pBSSList)
        {
            // When handling AP's capability change, continue to associate to
            // same BSS and make sure pRoamBssEntry is not Null.
            if((eANI_BOOLEAN_FALSE == fUseSameBss) || (pCommand->u.roamCmd.pRoamBssEntry == NULL))
            {
                if(pCommand->u.roamCmd.pRoamBssEntry == NULL)
                {
                    //Try the first BSS
                    pCommand->u.roamCmd.pLastRoamBss = NULL;
                    pCommand->u.roamCmd.pRoamBssEntry = csrLLPeekHead(&pBSSList->List, LL_ACCESS_LOCK);
                }
                else
                {
                    pCommand->u.roamCmd.pRoamBssEntry = csrLLNext(&pBSSList->List, pCommand->u.roamCmd.pRoamBssEntry, LL_ACCESS_LOCK);
                    if(NULL == pCommand->u.roamCmd.pRoamBssEntry)
                    {
                        //Done with all the BSSs
                        //In this case,   will tell HDD the completion
                        break;
                    }
                    else
                    {
                        //We need to indicate to HDD that we are done with this one.
                        //vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                        roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;     //this shall not be NULL
                        roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                        roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                        pRoamInfo = &roamInfo;
                    }
                }
                while(pCommand->u.roamCmd.pRoamBssEntry)
                {
                    pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
                    /*If concurrency enabled take the concurrent connected channel first. */
                    /* Valid multichannel concurrent sessions exempted */
                    if (vos_concurrent_open_sessions_running() &&
                        !csrIsValidMcConcurrentSession(pMac, sessionId,
                                           &pScanResult->Result.BssDescriptor))
                    {
                        concurrentChannel =
                            csrGetConcurrentOperationChannel(pMac);
                        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, "%s: "
                                " csr Concurrent Channel = %d", __func__, concurrentChannel);
                        if ((concurrentChannel) &&
                                (concurrentChannel ==
                                 pScanResult->Result.BssDescriptor.channelId))
                        {
                            //make this 0 because we do not want the
                            //below check to pass as we don't want to
                            //connect on other channel
                            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                                    FL("Concurrent channel match =%d"),
                                    concurrentChannel);
                            concurrentChannel = 0;
                        }
                    }

                    if (!concurrentChannel)
                    {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
                        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                        FL("csrRoamShouldRoam"));
#endif

                        if(HAL_STATUS_SUCCESS(csrRoamShouldRoam(pMac,
                            sessionId, &pScanResult->Result.BssDescriptor,
                            pCommand->u.roamCmd.roamId)))
                        {
                            //Ok to roam this
                            break;
                        }
                     }
                     else
                     {
                         eRoamState = eCsrStopRoamingDueToConcurrency;
                     }
                    pCommand->u.roamCmd.pRoamBssEntry = csrLLNext(&pBSSList->List, pCommand->u.roamCmd.pRoamBssEntry, LL_ACCESS_LOCK);
                    if(NULL == pCommand->u.roamCmd.pRoamBssEntry)
                    {
                        //Done with all the BSSs
                        fDone = eANI_BOOLEAN_TRUE;
                        break;
                    }
                }
                if(fDone)
                {
                    break;
                }
            }
        }

        if (!pRoamInfo)
            pRoamInfo = &roamInfo;

        pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

        //We have something to roam, tell HDD when it is infra.
        //For IBSS, the indication goes back to HDD via eCSR_ROAM_IBSS_IND
        //For WDS, the indication is eCSR_ROAM_WDS_IND
        if( CSR_IS_INFRASTRUCTURE( pProfile ) )
        {
            if(pSession->bRefAssocStartCnt)
            {
                pSession->bRefAssocStartCnt--;
                pRoamInfo->pProfile = pProfile;
                /* Complete the last association attempt because a new one
                   is about to be tried */
                csrRoamCallCallback(pMac, sessionId, pRoamInfo,
                                    pCommand->u.roamCmd.roamId,
                                    eCSR_ROAM_ASSOCIATION_COMPLETION,
                                    eCSR_ROAM_RESULT_NOT_ASSOCIATED);
            }
            /* If the roaming has stopped, not to continue the roaming command*/
            if ( !CSR_IS_ROAMING(pSession) && CSR_IS_ROAMING_COMMAND(pCommand) )
            {
                //No need to complete roaming here as it already completes
                smsLog(pMac, LOGW, FL("  Roam command (reason %d) aborted due to roaming completed"),
                        pCommand->u.roamCmd.roamReason);
                eRoamState = eCsrStopRoaming;
                csrSetAbortRoamingCommand(pMac, pCommand);
                break;
            }
            vos_mem_set(&roamInfo, sizeof(roamInfo), 0);
            if(pScanResult)
            {
                tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *)pScanResult->Result.pvIes;
                if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, &pScanResult->Result.BssDescriptor, &pIesLocal))) )
                {
                    smsLog(pMac, LOGE, FL(" cannot parse IEs"));
                    fDone = eANI_BOOLEAN_TRUE;
                    eRoamState = eCsrStopRoaming;
                    break;
                }
                roamInfo.pBssDesc = &pScanResult->Result.BssDescriptor;
                pCommand->u.roamCmd.pLastRoamBss = roamInfo.pBssDesc;
                //No need to put uapsd_mask in if the BSS doesn't support uAPSD
                if( pCommand->u.roamCmd.roamProfile.uapsd_mask &&
                    CSR_IS_QOS_BSS(pIesLocal) &&
                    CSR_IS_UAPSD_BSS(pIesLocal) )
                {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    acm_mask = sme_QosGetACMMask(pMac, &pScanResult->Result.BssDescriptor,
                         pIesLocal);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                }
                else
                {
                    pCommand->u.roamCmd.roamProfile.uapsd_mask = 0;
                }
                if( pIesLocal && !pScanResult->Result.pvIes)
                {
                    vos_mem_free(pIesLocal);
                }
            }
            else
            {
                pCommand->u.roamCmd.roamProfile.uapsd_mask = 0;
            }
            roamInfo.pProfile = pProfile;
            pSession->bRefAssocStartCnt++;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                          FL("csrGetParsedBssDescriptionIEs"));
#endif
            csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                 eCSR_ROAM_ASSOCIATION_START, eCSR_ROAM_RESULT_NONE );
        }
        if ( NULL == pCommand->u.roamCmd.pRoamBssEntry )
        {
            // If this is a start IBSS profile, then we need to start the IBSS.
            if ( CSR_IS_START_IBSS(pProfile) )
            {
                tANI_BOOLEAN fSameIbss = eANI_BOOLEAN_FALSE;
                // Attempt to start this IBSS...
                csrRoamAssignDefaultParam( pMac, pCommand );
                status = csrRoamStartIbss( pMac, sessionId, pProfile, &fSameIbss );
                if(HAL_STATUS_SUCCESS(status))
                {
                    if ( fSameIbss )
                    {
                        eRoamState = eCsrStartIbssSameIbss;
                    }
                    else
                    {
                        eRoamState = eCsrContinueRoaming;
                    }
                }
                else
                {
                    //it somehow fail need to stop
                    eRoamState = eCsrStopRoaming;
                }
                break;
            }
            else if ( (CSR_IS_WDS_AP(pProfile))
             || (CSR_IS_INFRA_AP(pProfile))
            )
            {
                // Attempt to start this WDS...
                csrRoamAssignDefaultParam( pMac, pCommand );
                /* For AP WDS, we dont have any BSSDescription */
                status = csrRoamStartWds( pMac, sessionId, pProfile, NULL );
                if(HAL_STATUS_SUCCESS(status))
                {
                    eRoamState = eCsrContinueRoaming;
                }
                else
                {
                    //it somehow fail need to stop
                    eRoamState = eCsrStopRoaming;
                }
            }
            else
            {
                //Nothing we can do
                smsLog(pMac, LOGW, FL("cannot continue without BSS list"));
                eRoamState = eCsrStopRoaming;
                break;
            }
        }
        else //We have BSS
        {
            //Need to assign these value because they are used in csrIsSameProfile
            pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
           /* The OSEN IE doesn't provide the cipher suite.
            * Therefore set to constant value of AES */
            if(pCommand->u.roamCmd.roamProfile.bOSENAssociation)
            {
                pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
                                                         eCSR_ENCRYPT_TYPE_AES;
                pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
                                                         eCSR_ENCRYPT_TYPE_AES;
            }
            else
            {
                  //Negotiated while building scan result.
                pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
                                                 pScanResult->ucEncryptionType;
                pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
                                                 pScanResult->mcEncryptionType;
            }
            pCommand->u.roamCmd.roamProfile.negotiatedAuthType = pScanResult->authType;
            if ( CSR_IS_START_IBSS(&pCommand->u.roamCmd.roamProfile) )
            {
                if(csrIsSameProfile(pMac, &pSession->connectedProfile, pProfile))
                {
                    eRoamState = eCsrStartIbssSameIbss;
                    break;
                }
            }
            if( pCommand->u.roamCmd.fReassocToSelfNoCapChange )
            {
                //trying to connect to the one already connected
                pCommand->u.roamCmd.fReassocToSelfNoCapChange = eANI_BOOLEAN_FALSE;
                eRoamState = eCsrReassocToSelfNoCapChange;
                break;
            }
            // Attempt to Join this Bss...
            eRoamState = csrRoamJoin( pMac, sessionId, &pScanResult->Result, pProfile );
            break;
        }

    } while( 0 );
    if( (eCsrStopRoaming == eRoamState) && (CSR_IS_INFRASTRUCTURE( pProfile )) )
    {
        //Need to indicate association_completion if association_start has been done
        if(pSession->bRefAssocStartCnt > 0)
        {
            pSession->bRefAssocStartCnt--;
            /* Complete the last association attempt because a new one is
               about to be tried */
            pRoamInfo = &roamInfo;
            pRoamInfo->pProfile = pProfile;
            csrRoamCallCallback(pMac, sessionId, pRoamInfo, pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_ASSOCIATION_COMPLETION,
                                        eCSR_ROAM_RESULT_NOT_ASSOCIATED);
        }
    }

    return( eRoamState );
}

static eHalStatus csrRoam( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    eCsrJoinState RoamState;
    tANI_U32 sessionId = pCommand->sessionId;

    //***if( hddIsRadioStateOn( pAdapter ) )
    {
        /* Attempt to join a Bss... */
        RoamState = csrRoamJoinNextBss( pMac, pCommand, eANI_BOOLEAN_FALSE );

        // if nothing to join..
        if (( eCsrStopRoaming == RoamState ) || ( eCsrStopRoamingDueToConcurrency == RoamState))
        {
            tANI_BOOLEAN fComplete = eANI_BOOLEAN_FALSE;
            // and if connected in Infrastructure mode...
            if ( csrIsConnStateInfra(pMac, sessionId) )
            {
                //... then we need to issue a disassociation
                status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN, FALSE );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGW, FL("  failed to issue disassociate, status = %d"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
            else if( csrIsConnStateIbss(pMac, sessionId) )
            {
                status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGW, FL("  failed to issue stop bss, status = %d"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
            else if (csrIsConnStateConnectedInfraAp(pMac, sessionId))
            {
                status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGW, FL("  failed to issue stop bss, status = %d"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
            else
            {
                fComplete = eANI_BOOLEAN_TRUE;
            }
            if(fComplete)
            {
               // ... otherwise, we can complete the Roam command here.
               if(eCsrStopRoamingDueToConcurrency == RoamState)
               {
                   csrRoamComplete( pMac, eCsrJoinFailureDueToConcurrency, NULL );
               }
               else
               {
                   csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
               }
            }
       }
       else if ( eCsrReassocToSelfNoCapChange == RoamState )
       {
           csrRoamComplete( pMac, eCsrSilentlyStopRoamingSaveState, NULL );
       }
       else if ( eCsrStartIbssSameIbss == RoamState )
       {
           csrRoamComplete( pMac, eCsrSilentlyStopRoaming, NULL );
       }
    }//hddIsRadioStateOn

    return status;
}
eHalStatus csrProcessFTReassocRoamCommand ( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    tANI_U32 sessionId;
    tCsrRoamSession *pSession;
    tCsrScanResult *pScanResult = NULL;
    tSirBssDescription *pBssDesc = NULL;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    sessionId = pCommand->sessionId;
    pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(CSR_IS_ROAMING(pSession) && pSession->fCancelRoaming)
    {
        /* The roaming is canceled. Simply complete the command */
        smsLog(pMac, LOG1, FL("Roam command canceled"));
        csrRoamComplete(pMac, eCsrNothingToJoin, NULL);
        return eHAL_STATUS_FAILURE;
    }
    if (pCommand->u.roamCmd.pRoamBssEntry)
    {
        pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
        pBssDesc = &pScanResult->Result.BssDescriptor;
    }
    else
    {
        /* The roaming is canceled. Simply complete the command */
        smsLog(pMac, LOG1, FL("Roam command canceled"));
        csrRoamComplete(pMac, eCsrNothingToJoin, NULL);
        return eHAL_STATUS_FAILURE;
    }
    status = csrRoamIssueReassociate(pMac, sessionId, pBssDesc,
        (tDot11fBeaconIEs *)( pScanResult->Result.pvIes ), &pCommand->u.roamCmd.roamProfile);
    return status;
}

eHalStatus csrRoamProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo roamInfo;
    tANI_U32 sessionId = pCommand->sessionId;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    smsLog(pMac, LOG1, FL("Roam Reason : %d, sessionId: %d"),
                         pCommand->u.roamCmd.roamReason, sessionId);

    switch ( pCommand->u.roamCmd.roamReason )
    {
    case eCsrForcedDisassoc:
        if (eCSR_ROAMING_STATE_IDLE == pMac->roam.curState[sessionId]) {
            smsLog(pMac, LOGE, FL("Ignore eCsrForcedDisassoc cmd on roam state"
                                  " %d"), eCSR_ROAMING_STATE_IDLE);
            return eHAL_STATUS_FAILURE;
        }

        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, FALSE );
        csrFreeRoamProfile(pMac, sessionId);
        break;
     case eCsrSmeIssuedDisassocForHandoff:
        //Not to free pMac->roam.pCurRoamProfile (via csrFreeRoamProfile) because it is needed after disconnect
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, FALSE );

        break;
    case eCsrForcedDisassocMICFailure:
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, TRUE );
        csrFreeRoamProfile(pMac, sessionId);
        break;
    case eCsrForcedDeauth:
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, FALSE, FALSE );
        csrFreeRoamProfile(pMac, sessionId);
        break;
    case eCsrHddIssuedReassocToSameAP:
    case eCsrSmeIssuedReassocToSameAP:
    {
        tDot11fBeaconIEs *pIes = NULL;

        if( pSession->pConnectBssDesc )
        {
            status = csrGetParsedBssDescriptionIEs(pMac, pSession->pConnectBssDesc, &pIes);
            if(!HAL_STATUS_SUCCESS(status) )
            {
                smsLog(pMac, LOGE, FL("  fail to parse IEs"));
            }
            else
            {
                roamInfo.reasonCode = eCsrRoamReasonStaCapabilityChanged;
                csrRoamCallCallback(pMac, pSession->sessionId, &roamInfo, 0, eCSR_ROAM_ROAMING_START, eCSR_ROAM_RESULT_NONE);
                pSession->roamingReason = eCsrReassocRoaming;
                roamInfo.pBssDesc = pSession->pConnectBssDesc;
                roamInfo.pProfile = &pCommand->u.roamCmd.roamProfile;
                pSession->bRefAssocStartCnt++;
                csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                     eCSR_ROAM_ASSOCIATION_START, eCSR_ROAM_RESULT_NONE );

                smsLog(pMac, LOG1, FL("  calling csrRoamIssueReassociate"));
                status = csrRoamIssueReassociate( pMac, sessionId, pSession->pConnectBssDesc, pIes,
                                                  &pCommand->u.roamCmd.roamProfile );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGE, FL("csrRoamIssueReassociate failed with status %d"), status);
                    csrReleaseCommandRoam( pMac, pCommand );
                }

                vos_mem_free(pIes);
                pIes = NULL;
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL
                    ("Reassoc To Same AP failed since Connected BSS is NULL"));
            return eHAL_STATUS_FAILURE;
        }
        break;
    }
    case eCsrCapsChange:
        smsLog(pMac, LOGE, FL("received eCsrCapsChange "));
        csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId );
        status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE);
        break;
    case eCsrSmeIssuedFTReassoc:
        smsLog(pMac, LOG1, FL("received FT Reassoc Req "));
        status = csrProcessFTReassocRoamCommand(pMac, pCommand);
        break;

    case eCsrStopBss:
       csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId);
       status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
       break;

    case eCsrForcedDisassocSta:
       csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId);
       csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_DISASSOC_REQ, sessionId);
       status = csrSendMBDisassocReqMsg( pMac, sessionId, pCommand->u.roamCmd.peerMac,
                     pCommand->u.roamCmd.reason);
       break;

    case eCsrForcedDeauthSta:
       csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId);
       csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_DEAUTH_REQ, sessionId);
       status = csrSendMBDeauthReqMsg( pMac, sessionId, pCommand->u.roamCmd.peerMac,
                     pCommand->u.roamCmd.reason);
       break;

    case eCsrPerformPreauth:
        smsLog(pMac, LOG1, FL("Attempting FT PreAuth Req"));
        status = csrRoamIssueFTPreauthReq(pMac, sessionId,
                pCommand->u.roamCmd.pLastRoamBss);
        break;
    default:
        csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId );

        if( pCommand->u.roamCmd.fUpdateCurRoamProfile )
        {
            //Remember the roaming profile
            csrFreeRoamProfile(pMac, sessionId);
            pSession->pCurRoamProfile = vos_mem_malloc(sizeof(tCsrRoamProfile));
            if ( NULL != pSession->pCurRoamProfile )
            {
                vos_mem_set(pSession->pCurRoamProfile, sizeof(tCsrRoamProfile), 0);
                csrRoamCopyProfile(pMac, pSession->pCurRoamProfile, &pCommand->u.roamCmd.roamProfile);
            }
        }

        //At this point, original uapsd_mask is saved in pCurRoamProfile
        //uapsd_mask in the pCommand may change from this point on.

        // Attempt to roam with the new scan results (if we need to..)
        status = csrRoam( pMac, pCommand );
        if(!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGW, FL("csrRoam() failed with status = 0x%08X"), status);
        }
        break;
    }
    return (status);
}

void csrReinitPreauthCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    pCommand->u.roamCmd.pLastRoamBss = NULL;
    pCommand->u.roamCmd.pRoamBssEntry = NULL;
    //Because u.roamCmd is union and share with scanCmd and StatusChange
    vos_mem_set(&pCommand->u.roamCmd, sizeof(tRoamCmd), 0);
}

void csrReinitRoamCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    if(pCommand->u.roamCmd.fReleaseBssList)
    {
        csrScanResultPurge(pMac, pCommand->u.roamCmd.hBSSList);
        pCommand->u.roamCmd.fReleaseBssList = eANI_BOOLEAN_FALSE;
        pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
    }
    if(pCommand->u.roamCmd.fReleaseProfile)
    {
        csrReleaseProfile(pMac, &pCommand->u.roamCmd.roamProfile);
        pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_FALSE;
    }
    pCommand->u.roamCmd.pRoamBssEntry = NULL;
    //Because u.roamCmd is union and share with scanCmd and StatusChange
    vos_mem_set(&pCommand->u.roamCmd, sizeof(tRoamCmd), 0);
}

void csrReinitWmStatusChangeCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    vos_mem_set(&pCommand->u.wmStatusChangeCmd, sizeof(tWmStatusChangeCmd), 0);
}
void csrRoamComplete( tpAniSirGlobal pMac, eCsrRoamCompleteResult Result, void *Context )
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    tANI_BOOLEAN fReleaseCommand = eANI_BOOLEAN_TRUE;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
             "%s: Roam Completion ...", __func__);
    pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
    if ( pEntry )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        // If the head of the queue is Active and it is a ROAM command, remove
        // and put this on the Free queue.
        if ( eSmeCommandRoam == pCommand->command )
        {
            //we need to process the result first before removing it from active list because state changes
            //still happening insides roamQProcessRoamResults so no other roam command should be issued
            fReleaseCommand = csrRoamProcessResults( pMac, pCommand, Result, Context );
            if( fReleaseCommand )
            {
                if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                {
                    csrReleaseCommandRoam( pMac, pCommand );
                }
                else
                {
                    smsLog( pMac, LOGE, " **********csrRoamComplete fail to release command reason %d",
                           pCommand->u.roamCmd.roamReason );
                }
            }
            else
            {
                smsLog( pMac, LOGE, " **********csrRoamComplete fail to release command reason %d",
                       pCommand->u.roamCmd.roamReason );
            }
        }
        else
        {
            smsLog( pMac, LOGW, "CSR: Roam Completion called but ROAM command is not ACTIVE ..." );
        }
    }
    else
    {
        smsLog( pMac, LOGW, "CSR: Roam Completion called but NO commands are ACTIVE ..." );
    }
    if( fReleaseCommand )
    {
        smeProcessPendingQueue( pMac );
    }
}

void csrResetPMKIDCandidateList( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }
    vos_mem_set(&(pSession->PmkidCandidateInfo[0]),
                sizeof(tPmkidCandidateInfo) * CSR_MAX_PMKID_ALLOWED, 0);
    pSession->NumPmkidCandidate = 0;
}
#ifdef FEATURE_WLAN_WAPI
void csrResetBKIDCandidateList( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }
    vos_mem_set(&(pSession->BkidCandidateInfo[0]),
                sizeof(tBkidCandidateInfo) * CSR_MAX_BKID_ALLOWED, 0);
    pSession->NumBkidCandidate = 0;
}
#endif /* FEATURE_WLAN_WAPI */
extern tANI_U8 csrWpaOui[][ CSR_WPA_OUI_SIZE ];

static eHalStatus csrRoamSaveSecurityRspIE(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrAuthType authType,
                                         tSirBssDescription *pSirBssDesc,
                                         tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tDot11fBeaconIEs *pIesLocal = pIes;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
        FL("LFR3:csrRoamSaveSecurityRspIE"));
    }
#endif

    if((eCSR_AUTH_TYPE_WPA == authType) ||
        (eCSR_AUTH_TYPE_WPA_PSK == authType) ||
        (eCSR_AUTH_TYPE_RSN == authType) ||
        (eCSR_AUTH_TYPE_RSN_PSK == authType)
#if defined WLAN_FEATURE_VOWIFI_11R
      ||
       (eCSR_AUTH_TYPE_FT_RSN == authType) ||
       (eCSR_AUTH_TYPE_FT_RSN_PSK == authType)
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_WAPI
      ||
       (eCSR_AUTH_TYPE_WAPI_WAI_PSK == authType) ||
       (eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE == authType)
#endif /* FEATURE_WLAN_WAPI */
#ifdef WLAN_FEATURE_11W
      ||
       (eCSR_AUTH_TYPE_RSN_PSK_SHA256 == authType) ||
       (eCSR_AUTH_TYPE_RSN_8021X_SHA256 == authType)
#endif /* FEATURE_WLAN_WAPI */
        )
    {
        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal))) )
        {
            smsLog(pMac, LOGE, FL(" cannot parse IEs"));
        }
        if( pIesLocal )
        {
            tANI_U32 nIeLen;
            tANI_U8 *pIeBuf;
            if((eCSR_AUTH_TYPE_RSN == authType) ||
#if defined WLAN_FEATURE_VOWIFI_11R
                (eCSR_AUTH_TYPE_FT_RSN == authType) ||
                (eCSR_AUTH_TYPE_FT_RSN_PSK == authType) ||
#endif /* WLAN_FEATURE_VOWIFI_11R */
#if defined WLAN_FEATURE_11W
                (eCSR_AUTH_TYPE_RSN_PSK_SHA256 == authType) ||
                (eCSR_AUTH_TYPE_RSN_8021X_SHA256 == authType) ||
#endif
                (eCSR_AUTH_TYPE_RSN_PSK == authType))
            {
                if(pIesLocal->RSN.present)
                {
                    //Calculate the actual length
                    nIeLen = 8 //version + gp_cipher_suite + pwise_cipher_suite_count
                        + pIesLocal->RSN.pwise_cipher_suite_count * 4    //pwise_cipher_suites
                        + 2 //akm_suite_count
                        + pIesLocal->RSN.akm_suite_count * 4 //akm_suites
                        + 2; //reserved
                    if( pIesLocal->RSN.pmkid_count )
                    {
                        nIeLen += 2 + pIesLocal->RSN.pmkid_count * 4;  //pmkid
                    }
                    //nIeLen doesn't count EID and length fields
                    pSession->pWpaRsnRspIE = vos_mem_malloc(nIeLen + 2);
                    if (NULL == pSession->pWpaRsnRspIE)
                        status = eHAL_STATUS_FAILURE;
                    else
                    {
                        vos_mem_set(pSession->pWpaRsnRspIE, nIeLen + 2, 0);
                        pSession->pWpaRsnRspIE[0] = DOT11F_EID_RSN;
                        pSession->pWpaRsnRspIE[1] = (tANI_U8)nIeLen;
                        /* Copy up to akm_suites */
                        pIeBuf = pSession->pWpaRsnRspIE + 2;
                        vos_mem_copy(pIeBuf, &pIesLocal->RSN.version,
                                     sizeof(pIesLocal->RSN.version));
                        pIeBuf += sizeof(pIesLocal->RSN.version);
                        vos_mem_copy(pIeBuf, &pIesLocal->RSN.gp_cipher_suite,
                                     sizeof(pIesLocal->RSN.gp_cipher_suite));
                        pIeBuf += sizeof(pIesLocal->RSN.gp_cipher_suite);
                        vos_mem_copy(pIeBuf, &pIesLocal->RSN.pwise_cipher_suite_count,
                                     sizeof(pIesLocal->RSN.pwise_cipher_suite_count));
                        pIeBuf += sizeof(pIesLocal->RSN.pwise_cipher_suite_count );
                        if( pIesLocal->RSN.pwise_cipher_suite_count )
                        {
                            //copy pwise_cipher_suites
                            vos_mem_copy(pIeBuf,
                                         pIesLocal->RSN.pwise_cipher_suites,
                                         pIesLocal->RSN.pwise_cipher_suite_count * 4);
                            pIeBuf += pIesLocal->RSN.pwise_cipher_suite_count * 4;
                        }
                        vos_mem_copy(pIeBuf, &pIesLocal->RSN.akm_suite_count, 2);
                        pIeBuf += 2;
                        if( pIesLocal->RSN.akm_suite_count )
                        {
                            //copy akm_suites
                            vos_mem_copy(pIeBuf,
                                         pIesLocal->RSN.akm_suites,
                                         pIesLocal->RSN.akm_suite_count * 4);
                            pIeBuf += pIesLocal->RSN.akm_suite_count * 4;
                        }
                        //copy the rest
                        vos_mem_copy(pIeBuf,
                                     pIesLocal->RSN.akm_suites + pIesLocal->RSN.akm_suite_count * 4,
                                     2 + pIesLocal->RSN.pmkid_count * 4);
                        pSession->nWpaRsnRspIeLength = nIeLen + 2;
                    }
                }
            }
            else if((eCSR_AUTH_TYPE_WPA == authType) ||
                (eCSR_AUTH_TYPE_WPA_PSK == authType))
            {
                if(pIesLocal->WPA.present)
                {
                    //Calculate the actual length
                    nIeLen = 12 //OUI + version + multicast_cipher + unicast_cipher_count
                        + pIesLocal->WPA.unicast_cipher_count * 4    //unicast_ciphers
                        + 2 //auth_suite_count
                        + pIesLocal->WPA.auth_suite_count * 4; //auth_suites

                    /*
                     * The WPA capabilities follows the Auth Suite (two octets)
                     * this field is optional, and we always "send" zero, so
                     * just remove it.  This is consistent with our assumptions
                     * in the frames compiler; c.f. bug 15234:
                     * nIeLen doesn't count EID and length fields
                     */

                    pSession->pWpaRsnRspIE = vos_mem_malloc(nIeLen + 2);
                    if ( NULL == pSession->pWpaRsnRspIE )
                        status = eHAL_STATUS_FAILURE;
                    else
                    {
                        pSession->pWpaRsnRspIE[0] = DOT11F_EID_WPA;
                        pSession->pWpaRsnRspIE[1] = (tANI_U8)nIeLen;
                        pIeBuf = pSession->pWpaRsnRspIE + 2;
                        //Copy WPA OUI
                        vos_mem_copy(pIeBuf, &csrWpaOui[1], 4);
                        pIeBuf += 4;
                        vos_mem_copy(pIeBuf, &pIesLocal->WPA.version,
                                     8 + pIesLocal->WPA.unicast_cipher_count * 4);
                        pIeBuf += 8 + pIesLocal->WPA.unicast_cipher_count * 4;
                        vos_mem_copy(pIeBuf, &pIesLocal->WPA.auth_suite_count,
                                     2 + pIesLocal->WPA.auth_suite_count * 4);
                        pIeBuf += pIesLocal->WPA.auth_suite_count * 4;
                        pSession->nWpaRsnRspIeLength = nIeLen + 2;
                    }
                }
            }
#ifdef FEATURE_WLAN_WAPI
          else if((eCSR_AUTH_TYPE_WAPI_WAI_PSK == authType) ||
                  (eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE == authType))
          {
                if(pIesLocal->WAPI.present)
                {
                   //Calculate the actual length
                   nIeLen = 4 //version + akm_suite_count
                      + pIesLocal->WAPI.akm_suite_count * 4 // akm_suites
                      + 2 //pwise_cipher_suite_count
                      + pIesLocal->WAPI.unicast_cipher_suite_count * 4    //pwise_cipher_suites
                      + 6; //gp_cipher_suite + preauth + reserved
                      if( pIesLocal->WAPI.bkid_count )
                      {
                           nIeLen += 2 + pIesLocal->WAPI.bkid_count * 4;  //bkid
        }

                   //nIeLen doesn't count EID and length fields
                   pSession->pWapiRspIE = vos_mem_malloc(nIeLen + 2);
                   if ( NULL == pSession->pWapiRspIE )
                        status = eHAL_STATUS_FAILURE;
                   else
                   {
                      pSession->pWapiRspIE[0] = DOT11F_EID_WAPI;
                      pSession->pWapiRspIE[1] = (tANI_U8)nIeLen;
                      pIeBuf = pSession->pWapiRspIE + 2;
                      /* Copy up to akm_suite_count */
                      vos_mem_copy(pIeBuf, &pIesLocal->WAPI.version, 2);
                      pIeBuf += 4;
                      if( pIesLocal->WAPI.akm_suite_count )
                      {
                         //copy akm_suites
                         vos_mem_copy(pIeBuf, pIesLocal->WAPI.akm_suites,
                                      pIesLocal->WAPI.akm_suite_count * 4);
                         pIeBuf += pIesLocal->WAPI.akm_suite_count * 4;
                      }
                      vos_mem_copy(pIeBuf,
                                   &pIesLocal->WAPI.unicast_cipher_suite_count,
                                   2);
                      pIeBuf += 2;
                      if( pIesLocal->WAPI.unicast_cipher_suite_count )
                      {
                         //copy pwise_cipher_suites
                         vos_mem_copy( pIeBuf,
                                       pIesLocal->WAPI.unicast_cipher_suites,
                                       pIesLocal->WAPI.unicast_cipher_suite_count * 4);
                         pIeBuf += pIesLocal->WAPI.unicast_cipher_suite_count * 4;
                      }
                      //gp_cipher_suite
                      vos_mem_copy(pIeBuf,
                                   pIesLocal->WAPI.multicast_cipher_suite,
                                   4);
                      pIeBuf += 4;
                      //preauth + reserved
                      vos_mem_copy(pIeBuf,
                                   pIesLocal->WAPI.multicast_cipher_suite + 4,
                                   2);
                      pIeBuf += 2;
                      if (pIesLocal->WAPI.bkid_count) {
                         /* bkid_count */
                         vos_mem_copy(pIeBuf, &pIesLocal->WAPI.bkid_count, 2);
                         pIeBuf += 2;
                         //copy akm_suites
                         vos_mem_copy(pIeBuf, pIesLocal->WAPI.bkid,
                                      pIesLocal->WAPI.bkid_count * 4);
                         pIeBuf += pIesLocal->WAPI.bkid_count * 4;
                      }
                      pSession->nWapiRspIeLength = nIeLen + 2;
                   }
                }
          }
#endif /* FEATURE_WLAN_WAPI */
            if( !pIes )
            {
                //locally allocated
                vos_mem_free(pIesLocal);
        }
    }
    }
    return (status);
}

#ifdef WLAN_FEATURE_VOWIFI_11R
//Returns whether the current association is a 11r assoc or not
tANI_BOOLEAN csrRoamIs11rAssoc(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIs11rAssoc(pMac, sessionId);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}
#endif
#ifdef FEATURE_WLAN_ESE
//Returns whether the current association is a ESE assoc or not
tANI_BOOLEAN csrRoamIsESEAssoc(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIsESEAssoc(pMac, sessionId);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}
#endif
#ifdef FEATURE_WLAN_LFR
//Returns whether "Legacy Fast Roaming" is currently enabled...or not
tANI_BOOLEAN csrRoamIsFastRoamEnabled(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tCsrRoamSession *pSession = NULL;

    if (CSR_IS_SESSION_VALID( pMac, sessionId ) )
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );
        if (NULL != pSession->pCurRoamProfile)
        {
            if (pSession->pCurRoamProfile->csrPersona != VOS_STA_MODE)
            {
                return eANI_BOOLEAN_FALSE;
            }
        }
    }

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
    if (eANI_BOOLEAN_TRUE == CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(pMac))
    {
        return (pMac->roam.configParam.isFastRoamIniFeatureEnabled);
    }
    else
#endif
    {
        return (pMac->roam.configParam.isFastRoamIniFeatureEnabled &&
            (!csrIsConcurrentSessionRunning(pMac)));
    }
}

#ifdef FEATURE_WLAN_ESE
/* ---------------------------------------------------------------------------
    \fn csrNeighborRoamIsESEAssoc

    \brief  This function returns whether the current association
            is a ESE assoc or not

    \param  pMac - The handle returned by macOpen.
    \param  sessionId - Session Id

    \return eANI_BOOLEAN_TRUE if current assoc is ESE, eANI_BOOLEAN_FALSE
     otherwise
---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamIsESEAssoc(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    return pMac->roam.neighborRoamInfo[sessionId].isESEAssoc;
}
#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
//Returns whether "FW based BG scan" is currently enabled...or not
tANI_BOOLEAN csrRoamIsRoamOffloadScanEnabled(tpAniSirGlobal pMac)
{
    return (pMac->roam.configParam.isRoamOffloadScanEnabled);
}
#endif
#endif

#if defined(FEATURE_WLAN_ESE)
tANI_BOOLEAN csrRoamIsEseIniFeatureEnabled(tpAniSirGlobal pMac)
{
    return pMac->roam.configParam.isEseIniFeatureEnabled;
}
#endif /*FEATURE_WLAN_ESE*/

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
eCsrPhyMode csrRoamdot11modeToPhymode(tANI_U8 dot11mode)
{
    eCsrPhyMode phymode = eCSR_DOT11_MODE_abg;

    switch (dot11mode)
    {
    case WNI_CFG_DOT11_MODE_ALL:
        phymode = eCSR_DOT11_MODE_abg;
        break;
    case WNI_CFG_DOT11_MODE_11A:
        phymode = eCSR_DOT11_MODE_11a;
        break;
    case WNI_CFG_DOT11_MODE_11B:
        phymode = eCSR_DOT11_MODE_11b;
        break;
    case WNI_CFG_DOT11_MODE_11G:
        phymode = eCSR_DOT11_MODE_11g;
        break;
    case WNI_CFG_DOT11_MODE_11N:
        phymode = eCSR_DOT11_MODE_11n;
        break;
    case WNI_CFG_DOT11_MODE_11G_ONLY:
        phymode = eCSR_DOT11_MODE_11g_ONLY;
        break;
    case WNI_CFG_DOT11_MODE_11N_ONLY:
        phymode = eCSR_DOT11_MODE_11n_ONLY;
        break;
    case WNI_CFG_DOT11_MODE_11AC:
        phymode = eCSR_DOT11_MODE_11ac;
        break;
    case WNI_CFG_DOT11_MODE_11AC_ONLY:
        phymode = eCSR_DOT11_MODE_11ac_ONLY;
        break;
    default:
        break;
    }

    return phymode;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
eHalStatus csrRoamOffloadSendSynchCnf(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tpSirSmeRoamOffloadSynchCnf pRoamOffloadSynchCnf;
    vos_msg_t msg;
    tCsrRoamSession *pSession = &pMac->roam.roamSession[sessionId];
    pRoamOffloadSynchCnf =
            vos_mem_malloc(sizeof(tSirSmeRoamOffloadSynchCnf));
    if (NULL == pRoamOffloadSynchCnf)
    {
        VOS_TRACE(VOS_MODULE_ID_SME,
          VOS_TRACE_LEVEL_ERROR,
          "%s: not able to allocate memory for roam"
          "offload synch confirmation data", __func__);
        pSession->roamOffloadSynchParams.bRoamSynchInProgress = VOS_FALSE;
        return eHAL_STATUS_FAILURE;
    }
    pRoamOffloadSynchCnf->sessionId = sessionId;
    msg.type     = WDA_ROAM_OFFLOAD_SYNCH_CNF;
    msg.reserved = 0;
    msg.bodyptr  = pRoamOffloadSynchCnf;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "LFR3: Posting WDA_ROAM_OFFLOAD_SYNCH_CNF");
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(
                                    VOS_MODULE_ID_WDA, &msg)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME,VOS_TRACE_LEVEL_DEBUG,
        "%s: Not able to post WDA_ROAM_OFFLOAD_SYNCH_CNF message to WDA",
        __func__);
        vos_mem_free(pRoamOffloadSynchCnf);
        pSession->roamOffloadSynchParams.bRoamSynchInProgress = VOS_FALSE;
        return eHAL_STATUS_FAILURE;
    }
    pSession->roamOffloadSynchParams.bRoamSynchInProgress = VOS_FALSE;
    return eHAL_STATUS_SUCCESS;
}
void csrRoamSynchCleanUp (tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    vos_msg_t msg;
    tpSirRoamOffloadSynchFail pRoamOffloadFailed = NULL;
    tCsrRoamSession *pSession = &pMac->roam.roamSession[sessionId];

    /*Clean up the roam synch in progress for LFR3 */
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
    "%s: Roam Synch Failed, Clean Up", __func__);
    pSession->roamOffloadSynchParams.bRoamSynchInProgress = VOS_FALSE;

    pRoamOffloadFailed =
            vos_mem_malloc(sizeof(tSirRoamOffloadSynchFail));
    if (NULL == pRoamOffloadFailed)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
          "%s: unable to allocate memory for roam synch fail" , __func__);
        return;
    }
    pRoamOffloadFailed->sessionId = sessionId;
    msg.type     = WDA_ROAM_OFFLOAD_SYNCH_FAIL;
    msg.reserved = 0;
    msg.bodyptr  = pRoamOffloadFailed;
    if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg))) {
          VOS_TRACE(VOS_MODULE_ID_SME,VOS_TRACE_LEVEL_DEBUG,
          "%s:Unable to post WDA_ROAM_OFFLOAD_SYNCH_FAIL msg to WDA",__func__);
           vos_mem_free(pRoamOffloadFailed);
    }
}
#endif

//Return true means the command can be release, else not
static tANI_BOOLEAN csrRoamProcessResults( tpAniSirGlobal pMac, tSmeCmd *pCommand,
                                       eCsrRoamCompleteResult Result, void *Context )
{
    tANI_BOOLEAN fReleaseCommand = eANI_BOOLEAN_TRUE;
    tSirBssDescription *pSirBssDesc = NULL;
    tSirMacAddr BroadcastMac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    tCsrScanResult *pScanResult = NULL;
    tCsrRoamInfo roamInfo;
    sme_QosAssocInfo assocInfo;
    sme_QosCsrEventIndType ind_qos;//indication for QoS module in SME
    tANI_U8 acm_mask = 0; //HDD needs the ACM mask in the assoc rsp callback
    tDot11fBeaconIEs *pIes = NULL;
    tANI_U32 sessionId = pCommand->sessionId;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tCsrRoamProfile *pProfile = &pCommand->u.roamCmd.roamProfile;
    eRoamCmdStatus roamStatus;
    eCsrRoamResult roamResult;
    eHalStatus status;
    tANI_U32 key_timeout_interval = 0;
    tSirSmeStartBssRsp  *pSmeStartBssRsp = NULL;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eANI_BOOLEAN_FALSE;
    }
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
              FL("Processing ROAM results..."));
    switch( Result )
    {
        case eCsrJoinSuccess:
            // reset the IDLE timer
            // !!
            // !! fall through to the next CASE statement here is intentional !!
            // !!
        case eCsrReassocSuccess:
            if(eCsrReassocSuccess == Result)
            {
                ind_qos = SME_QOS_CSR_REASSOC_COMPLETE;
            }
            else
            {
                ind_qos = SME_QOS_CSR_ASSOC_COMPLETE;
            }
            // Success Join Response from LIM.  Tell NDIS we are connected and save the
            // Connected state...
            smsLog(pMac, LOGW, FL("receives association indication"));
            vos_mem_set(&roamInfo, sizeof(roamInfo), 0);
            //always free the memory here
            if(pSession->pWpaRsnRspIE)
            {
                pSession->nWpaRsnRspIeLength = 0;
                vos_mem_free(pSession->pWpaRsnRspIE);
                pSession->pWpaRsnRspIE = NULL;
            }
#ifdef FEATURE_WLAN_WAPI
            if(pSession->pWapiRspIE)
            {
                pSession->nWapiRspIeLength = 0;
                vos_mem_free(pSession->pWapiRspIE);
                pSession->pWapiRspIE = NULL;
            }
#endif /* FEATURE_WLAN_WAPI */
            /* This creates problem since we have not saved the connected profile.
            So moving this after saving the profile
            */
            //csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED );

            /* Reset remainInPowerActiveTillDHCP as it might have been set
             * by last failed secured connection.
             * It should be set only for secured connection.
             */
            pMac->pmc.remainInPowerActiveTillDHCP = FALSE;
            if( CSR_IS_INFRASTRUCTURE( pProfile ) )
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
            }
            else
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED;
            }
            //Use the last connected bssdesc for reassoc-ing to the same AP.
            //NOTE: What to do when reassoc to a different AP???
            if( (eCsrHddIssuedReassocToSameAP == pCommand->u.roamCmd.roamReason) ||
                (eCsrSmeIssuedReassocToSameAP == pCommand->u.roamCmd.roamReason) )
            {
                pSirBssDesc = pSession->pConnectBssDesc;
                if(pSirBssDesc)
                {
                    vos_mem_copy(&roamInfo.bssid, &pSirBssDesc->bssId,
                                 sizeof(tCsrBssid));
                }
            }
            else
            {

                if(pCommand->u.roamCmd.pRoamBssEntry)
                {
                    pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
                    if(pScanResult != NULL)
                    {
                        pSirBssDesc = &pScanResult->Result.BssDescriptor;
                        //this can be NULL
                        pIes = (tDot11fBeaconIEs *)( pScanResult->Result.pvIes );
                        vos_mem_copy(&roamInfo.bssid, &pSirBssDesc->bssId,
                                     sizeof(tCsrBssid));
                    }
                }
            }
            if( pSirBssDesc )
            {
                roamInfo.staId = HAL_STA_INVALID_IDX;
                csrRoamSaveConnectedInfomation(pMac, sessionId, pProfile, pSirBssDesc, pIes);
                    //Save WPA/RSN IE
                csrRoamSaveSecurityRspIE(pMac, sessionId, pProfile->negotiatedAuthType, pSirBssDesc, pIes);
#ifdef FEATURE_WLAN_ESE
                roamInfo.isESEAssoc = pSession->connectedProfile.isESEAssoc;
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (pSirBssDesc->mdiePresent)
                {
                    if(csrIsAuthType11r(pProfile->negotiatedAuthType, VOS_TRUE)
#ifdef FEATURE_WLAN_ESE
                       && !((pProfile->negotiatedAuthType ==
                                 eCSR_AUTH_TYPE_OPEN_SYSTEM) &&
                                 (pIes && pIes->ESEVersion.present) &&
                                 (pMac->roam.configParam.isEseIniFeatureEnabled))
#endif
                       )
                    {
                        // is11Rconnection
                        roamInfo.is11rAssoc = VOS_TRUE;
                    }
                    else
                    {
                        // is11Rconnection
                        roamInfo.is11rAssoc = VOS_FALSE;
                    }
                }
#endif

                /* csrRoamStateChange also affects sub-state. Hence,
                 * csrRoamStateChange happens first and then sub state change.
                 * Moving even save profile above so that below mentioned
                 * condition is also met. */
                csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
                // Make sure the Set Context is issued before link indication to NDIS.  After link indication is
                // made to NDIS, frames could start flowing.  If we have not set context with LIM, the frames
                // will be dropped for the security context may not be set properly.
                //
                // this was causing issues in the 2c_wlan_wep WHQL test when the SetContext was issued after the link
                // indication.  (Link Indication happens in the profFSMSetConnectedInfra call).
                //

                if( CSR_IS_ENC_TYPE_STATIC( pProfile->negotiatedUCEncryptionType ) &&
                                        !pProfile->bWPSAssociation)
                {
                    // Issue the set Context request to LIM to establish the Unicast STA context
                    if( !HAL_STATUS_SUCCESS( csrRoamIssueSetContextReq( pMac, sessionId,
                                                pProfile->negotiatedUCEncryptionType,
                                                pSirBssDesc, &(pSirBssDesc->bssId),
                                                FALSE, TRUE, eSIR_TX_RX, 0, 0, NULL, 0 ) ) ) // NO keys... these key parameters don't matter.
                    {
                        smsLog( pMac, LOGE, FL("  Set context for unicast fail") );
                        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
                    }
                    // Issue the set Context request to LIM to establish the Broadcast STA context
                    csrRoamIssueSetContextReq( pMac, sessionId, pProfile->negotiatedMCEncryptionType,
                                               pSirBssDesc, &BroadcastMac,
                                               FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                }
                else
                {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        if (pSession->roamOffloadSynchParams.bRoamSynchInProgress &&
           (pSession->roamOffloadSynchParams.authStatus ==
                                     CSR_ROAM_AUTH_STATUS_AUTHENTICATED))
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
           FL("LFR3:Do not start the wait for key timer"));
           csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
        } else {
#endif
                    /* Need to wait for supplicant authentication */
                    roamInfo.fAuthRequired = eANI_BOOLEAN_TRUE;
                    /* Set the sub-state to WaitForKey in case
                       authentication is needed */
                    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY, sessionId );

                    if(pProfile->bWPSAssociation)
                    {
                        key_timeout_interval = CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD;
                    }
                    else
                    {
                        key_timeout_interval = CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD;
                    }

                    //Save sessionId in case of timeout
                    pMac->roam.WaitForKeyTimerInfo.sessionId = (tANI_U8)sessionId;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        if (pSession->roamOffloadSynchParams.bRoamSynchInProgress &&
           (pSession->roamOffloadSynchParams.authStatus ==
                                     CSR_ROAM_AUTH_STATUS_CONNECTED))
        {
             VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
             FL("LFR3:Send Synch Cnf for Auth status connected"));
             csrRoamOffloadSendSynchCnf( pMac, sessionId);

         }
#endif
                    //This time should be long enough for the rest of the process plus setting key
                    if(!HAL_STATUS_SUCCESS( csrRoamStartWaitForKeyTimer( pMac, key_timeout_interval ) ) )
                    {
                        /* Reset our state so nothing is blocked. */
                        smsLog( pMac, LOGE, FL("   Failed to start pre-auth timer") );
                        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
                    }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        }
#endif
                }

                assocInfo.pBssDesc = pSirBssDesc; //could be NULL
                assocInfo.pProfile = pProfile;
                if(Context)
                {
                    tSirSmeJoinRsp *pJoinRsp = (tSirSmeJoinRsp *)Context;
                    tANI_U32 len;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
        {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
            FL("LFR3:csrRoamFreeConnectedInfo"));
        }
#endif
                    csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
                    len = pJoinRsp->assocReqLength + pJoinRsp->assocRspLength + pJoinRsp->beaconLength;
#ifdef WLAN_FEATURE_VOWIFI_11R
                    len += pJoinRsp->parsedRicRspLen;
#endif /* WLAN_FEATURE_VOWIFI_11R */
#ifdef FEATURE_WLAN_ESE
                    len += pJoinRsp->tspecIeLen;
#endif
                    if(len)
                    {
                        pSession->connectedInfo.pbFrames = vos_mem_malloc(len);
                        if ( pSession->connectedInfo.pbFrames != NULL )
                        {
                            vos_mem_copy(pSession->connectedInfo.pbFrames,
                                         pJoinRsp->frames, len);
                            pSession->connectedInfo.nAssocReqLength = pJoinRsp->assocReqLength;
                            pSession->connectedInfo.nAssocRspLength = pJoinRsp->assocRspLength;
                            pSession->connectedInfo.nBeaconLength = pJoinRsp->beaconLength;
#ifdef WLAN_FEATURE_VOWIFI_11R
                            pSession->connectedInfo.nRICRspLength = pJoinRsp->parsedRicRspLen;
#endif /* WLAN_FEATURE_VOWIFI_11R */
#ifdef FEATURE_WLAN_ESE
                            pSession->connectedInfo.nTspecIeLength = pJoinRsp->tspecIeLen;
#endif
                            roamInfo.nAssocReqLength = pJoinRsp->assocReqLength;
                            roamInfo.nAssocRspLength = pJoinRsp->assocRspLength;
                            roamInfo.nBeaconLength = pJoinRsp->beaconLength;
                            roamInfo.pbFrames = pSession->connectedInfo.pbFrames;
                        }
                    }
                    if(pCommand->u.roamCmd.fReassoc)
                    {
                        roamInfo.fReassocReq = roamInfo.fReassocRsp = eANI_BOOLEAN_TRUE;
                    }
                    pSession->connectedProfile.vht_channel_width =
                                                   pJoinRsp->vht_channel_width;
                    pSession->connectedInfo.staId = ( tANI_U8 )pJoinRsp->staId;
                    roamInfo.staId = ( tANI_U8 )pJoinRsp->staId;
                    roamInfo.ucastSig = ( tANI_U8 )pJoinRsp->ucastSig;
                    roamInfo.bcastSig = ( tANI_U8 )pJoinRsp->bcastSig;
                    roamInfo.timingMeasCap = pJoinRsp->timingMeasCap;
                    roamInfo.chan_info.nss = pJoinRsp->nss;
                    roamInfo.chan_info.rate_flags = pJoinRsp->max_rate_flags;
#ifdef FEATURE_WLAN_TDLS
                    roamInfo.tdls_prohibited = pJoinRsp->tdls_prohibited;
                    roamInfo.tdls_chan_swit_prohibited =
                                      pJoinRsp->tdls_chan_swit_prohibited;
                    smsLog(pMac, LOG1,
                       FL("tdls_prohibited: %d, tdls_chan_swit_prohibited: %d"),
                       roamInfo.tdls_prohibited,
                       roamInfo.tdls_chan_swit_prohibited);
#endif
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
                    if (pMac->roam.configParam.cc_switch_mode
                                            != VOS_MCC_TO_SCC_SWITCH_DISABLE) {
                        pSession->connectedProfile.HTProfile.phymode =
                            csrRoamdot11modeToPhymode(pJoinRsp->HTProfile.dot11mode);
                        pSession->connectedProfile.HTProfile.htCapability =
                                    pJoinRsp->HTProfile.htCapability;
                        pSession->connectedProfile.HTProfile.htSupportedChannelWidthSet =
                                    pJoinRsp->HTProfile.htSupportedChannelWidthSet;
                        pSession->connectedProfile.HTProfile.htRecommendedTxWidthSet =
                                    pJoinRsp->HTProfile.htRecommendedTxWidthSet;
                        pSession->connectedProfile.HTProfile.htSecondaryChannelOffset =
                                    pJoinRsp->HTProfile.htSecondaryChannelOffset;
#ifdef WLAN_FEATURE_11AC
                        pSession->connectedProfile.HTProfile.vhtCapability =
                                    pJoinRsp->HTProfile.vhtCapability;
                        pSession->connectedProfile.HTProfile.vhtTxChannelWidthSet =
                                    pJoinRsp->HTProfile.vhtTxChannelWidthSet;
                        pSession->connectedProfile.HTProfile.apCenterChan =
                                    pJoinRsp->HTProfile.apCenterChan;
                        pSession->connectedProfile.HTProfile.apChanWidth =
                                    pJoinRsp->HTProfile.apChanWidth;
#endif
                    }
#endif
                }
                else
                {
                   if(pCommand->u.roamCmd.fReassoc)
                   {
                       roamInfo.fReassocReq = roamInfo.fReassocRsp = eANI_BOOLEAN_TRUE;
                       roamInfo.nAssocReqLength = pSession->connectedInfo.nAssocReqLength;
                       roamInfo.nAssocRspLength = pSession->connectedInfo.nAssocRspLength;
                       roamInfo.nBeaconLength = pSession->connectedInfo.nBeaconLength;
                       roamInfo.pbFrames = pSession->connectedInfo.pbFrames;
                   }
                }

                /* Update the staId from the previous connected profile info
                   as the reassociation is triggred at SME/HDD */
                if ((eCsrHddIssuedReassocToSameAP ==
                                    pCommand->u.roamCmd.roamReason) ||
                    (eCsrSmeIssuedReassocToSameAP ==
                                    pCommand->u.roamCmd.roamReason))
                    roamInfo.staId = pSession->connectedInfo.staId;

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                // Indicate SME-QOS with reassoc success event, only after
                // copying the frames
                sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, ind_qos, &assocInfo);
#endif
                roamInfo.pBssDesc = pSirBssDesc;
                roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                acm_mask = sme_QosGetACMMask(pMac, pSirBssDesc, NULL);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                pSession->connectedProfile.acm_mask = acm_mask;
                //start UAPSD if uapsd_mask is not 0 because HDD will configure for trigger frame
                //It may be better to let QoS do this????
                if( pSession->connectedProfile.modifyProfileFields.uapsd_mask )
                {
                    smsLog(pMac, LOGE, " uapsd_mask (0x%X) set, request UAPSD now",
                        pSession->connectedProfile.modifyProfileFields.uapsd_mask);
                    if(!pMac->psOffloadEnabled)
                    {
                        pmcStartUapsd( pMac, NULL, NULL );
                    }
                    else
                    {
                        pmcOffloadStartUapsd(pMac, sessionId, NULL, NULL);
                    }
                }
                pSession->connectedProfile.dot11Mode = pSession->bssParams.uCfgDot11Mode;
                roamInfo.u.pConnectedProfile = &pSession->connectedProfile;

                if( pSession->bRefAssocStartCnt > 0 )
                {
                    pSession->bRefAssocStartCnt--;
                    //Remove this code once SLM_Sessionization is supported
                    //BMPS_WORKAROUND_NOT_NEEDED
                    if(!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) && ( csrIsConcurrentSessionRunning( pMac )))
                    {
                       pMac->roam.configParam.doBMPSWorkaround = 1;
                    }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        if (pSession->roamOffloadSynchParams.bRoamSynchInProgress) {
           roamInfo.roamSynchInProgress = 1;
           roamInfo.synchAuthStatus =
                 pSession->roamOffloadSynchParams.authStatus;
           vos_mem_copy(roamInfo.kck, pSession->roamOffloadSynchParams.kck,
                        SIR_KCK_KEY_LEN);
           vos_mem_copy(roamInfo.kek, pSession->roamOffloadSynchParams.kek,
                        SIR_KEK_KEY_LEN);
           vos_mem_copy(roamInfo.replay_ctr,
                        pSession->roamOffloadSynchParams.replay_ctr,
                        SIR_REPLAY_CTR_LEN);
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
            FL("LFR3:csrRoamCallCallback:eCSR_ROAM_RESULT_ASSOCIATED"));
        }
#endif
                    csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);
                }

                csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_NONE, eANI_BOOLEAN_TRUE);
                // reset the PMKID candidate list
                csrResetPMKIDCandidateList( pMac, sessionId );
#ifdef FEATURE_WLAN_WAPI
                // reset the BKID candidate list
                csrResetBKIDCandidateList( pMac, sessionId );
#endif /* FEATURE_WLAN_WAPI */
            }
            else
            {
                smsLog(pMac, LOGW, "  Roam command doesn't have a BSS desc");
            }
            csrScanCancelIdleScan(pMac);
            //Not to signal link up because keys are yet to be set.
            //The linkup function will overwrite the sub-state that we need to keep at this point.
            if( !CSR_IS_WAIT_FOR_KEY(pMac, sessionId) )
            {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
            {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                        FL("NO CSR_IS_WAIT_FOR_KEY -> csrRoamLinkUp"));
            }
#endif
                csrRoamLinkUp(pMac, pSession->connectedProfile.bssid);
            }
            //Check if BMPS is required and start the BMPS retry timer.  Timer period is large
            //enough to let security and DHCP handshake succeed before entry into BMPS
            if (!pMac->psOffloadEnabled && pmcShouldBmpsTimerRun(pMac))
            {
                /* Set remainInPowerActiveTillDHCP to make sure we wait for
                 * until keys are set before going into BMPS.
                 */
                if(eANI_BOOLEAN_TRUE == roamInfo.fAuthRequired)
                {
                     pMac->pmc.remainInPowerActiveTillDHCP = TRUE;
                     smsLog(pMac, LOG1, FL("Set remainInPowerActiveTillDHCP "
                            "to make sure we wait until keys are set before"
                            " going to BMPS"));
                }
                if (pmcStartTrafficTimer(pMac, BMPS_TRAFFIC_TIMER_ALLOW_SECURITY_DHCP)
                    != eHAL_STATUS_SUCCESS)
                {
                    smsLog(pMac, LOGP, FL("Cannot start BMPS Retry timer"));
                }
                smsLog(pMac, LOG2, FL("BMPS Retry Timer already running or started"));
            }
            break;

        case eCsrStartBssSuccess:
            // on the StartBss Response, LIM is returning the Bss Description that we
            // are beaconing.  Add this Bss Description to our scan results and
            // chain the Profile to this Bss Description.  On a Start BSS, there was no
            // detected Bss description (no partner) so we issued the Start Bss to
            // start the Ibss without any Bss description.  Lim was kind enough to return
            // the Bss Description that we start beaconing for the newly started Ibss.
            smsLog(pMac, LOG2, FL("receives start BSS ok indication"));
            status = eHAL_STATUS_FAILURE;
            pSmeStartBssRsp = (tSirSmeStartBssRsp *)Context;
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            if( CSR_IS_IBSS( pProfile ) )
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
            }
            else if (CSR_IS_INFRA_AP(pProfile))
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
            }
            else
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;
            }
            if( !CSR_IS_WDS_STA( pProfile ) )
            {
                csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
                pSirBssDesc = &pSmeStartBssRsp->bssDescription;
                if( !HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs( pMac, pSirBssDesc, &pIes )) )
                {
                    smsLog(pMac, LOGW, FL("cannot parse IBSS IEs"));
                    roamInfo.pBssDesc = pSirBssDesc;
                    /*
                     * We need to associate_complete it first, because
                     * Associate_start already indicated.
                     */
                    csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                            eCSR_ROAM_IBSS_IND, eCSR_ROAM_RESULT_IBSS_START_FAILED );
                    break;
                }
                if (!CSR_IS_INFRA_AP(pProfile))
                {
                    pScanResult = csrScanAppendBssDescription(pMac,
                                                              pSirBssDesc,
                                                              pIes, FALSE,
                                                              sessionId);
                }
                csrRoamSaveConnectedBssDesc(pMac, sessionId, pSirBssDesc);
                csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
                csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
                if(pSirBssDesc)
                {
                    csrRoamSaveConnectedInfomation(pMac, sessionId, pProfile, pSirBssDesc, pIes);
                    vos_mem_copy(&roamInfo.bssid, &pSirBssDesc->bssId,
                                 sizeof(tCsrBssid));
                }
                /* We are done with the IEs so free it */
                vos_mem_free(pIes);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                {
                    vos_log_ibss_pkt_type *pIbssLog;
                    tANI_U32 bi;

                    WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
                    if(pIbssLog)
                    {
                        if(CSR_INVALID_SCANRESULT_HANDLE == pCommand->u.roamCmd.hBSSList)
                        {
                            //We start the IBSS (didn't find any matched IBSS out there)
                            pIbssLog->eventId = WLAN_IBSS_EVENT_START_IBSS_RSP;
                        }
                        else
                        {
                            pIbssLog->eventId = WLAN_IBSS_EVENT_JOIN_IBSS_RSP;
                        }
                        if(pSirBssDesc)
                        {
                            vos_mem_copy(pIbssLog->bssid, pSirBssDesc->bssId, 6);
                            pIbssLog->operatingChannel = pSirBssDesc->channelId;
                        }
                        if(HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL, &bi)))
                        {
                            //***U8 is not enough for beacon interval
                            pIbssLog->beaconInterval = (v_U8_t)bi;
                        }
                        WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                    }
                }
#endif //#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                //Only set context for non-WDS_STA. We don't even need it for WDS_AP. But since the encryption
                //is WPA2-PSK so it won't matter.
                if(CSR_IS_ENC_TYPE_STATIC(pProfile->negotiatedUCEncryptionType)
                    && pSession->pCurRoamProfile
                    && !CSR_IS_INFRA_AP(pSession->pCurRoamProfile))
                {
                    // Issue the set Context request to LIM to establish the Broadcast STA context for the Ibss.
                    // In Rome IBSS case, dummy key installation will break
                    // proper BSS key installation, so skip it.
                    if (!CSR_IS_IBSS( pSession->pCurRoamProfile ))
                    {
                        csrRoamIssueSetContextReq( pMac, sessionId,
                                        pProfile->negotiatedMCEncryptionType,
                                        pSirBssDesc, &BroadcastMac,
                                        FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                    }

                }
            }
            else
            {
                //Keep the state to eCSR_ROAMING_STATE_JOINING
                //Need to send join_req.
                if(pCommand->u.roamCmd.pRoamBssEntry)
                {
                    if((pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link)))
                    {
                        pSirBssDesc = &pScanResult->Result.BssDescriptor;
                        pIes = (tDot11fBeaconIEs *)( pScanResult->Result.pvIes );
                        // Set the roaming substate to 'join attempt'...
                        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
                        status = csrSendJoinReqMsg( pMac, sessionId, pSirBssDesc, pProfile, pIes, eWNI_SME_JOIN_REQ );
                    }
                }
                else
                {
                    smsLog( pMac, LOGE, " StartBSS for WDS station with no BssDesc" );
                    VOS_ASSERT( 0 );
                }
            }
            //Only tell upper layer is we start the BSS because Vista doesn't like multiple connection
            //indications. If we don't start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS will
            //trigger the connection start indication in Vista
            if( !CSR_IS_JOIN_TO_IBSS( pProfile ) )
            {
                roamStatus = eCSR_ROAM_IBSS_IND;
                roamResult = eCSR_ROAM_RESULT_IBSS_STARTED;
                if( CSR_IS_WDS( pProfile ) )
                {
                    roamStatus = eCSR_ROAM_WDS_IND;
                    roamResult = eCSR_ROAM_RESULT_WDS_STARTED;
                }
                if( CSR_IS_INFRA_AP( pProfile ) )
                {
                    roamStatus = eCSR_ROAM_INFRA_IND;
                    roamResult = eCSR_ROAM_RESULT_INFRA_STARTED;
                }

                //Only tell upper layer is we start the BSS because Vista doesn't like multiple connection
                //indications. If we don't start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS will
                //trigger the connection start indication in Vista
                vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                //We start the IBSS (didn't find any matched IBSS out there)
                roamInfo.pBssDesc = pSirBssDesc;
                roamInfo.staId = (tANI_U8)pSmeStartBssRsp->staId;
                vos_mem_copy(roamInfo.bssid, pSirBssDesc->bssId,
                             sizeof(tCsrBssid));
                 //Remove this code once SLM_Sessionization is supported
                 //BMPS_WORKAROUND_NOT_NEEDED
                if(!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
                   ( csrIsConcurrentSessionRunning( pMac )))
                {
                   pMac->roam.configParam.doBMPSWorkaround = 1;
                }

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
                if (pMac->roam.configParam.cc_switch_mode
                                        != VOS_MCC_TO_SCC_SWITCH_DISABLE) {
                    pSession->connectedProfile.HTProfile.phymode =
                            csrRoamdot11modeToPhymode(pSmeStartBssRsp->HTProfile.dot11mode);
                    pSession->connectedProfile.HTProfile.htCapability =
                            pSmeStartBssRsp->HTProfile.htCapability;
                    pSession->connectedProfile.HTProfile.htSupportedChannelWidthSet =
                            pSmeStartBssRsp->HTProfile.htSupportedChannelWidthSet;
                    pSession->connectedProfile.HTProfile.htRecommendedTxWidthSet =
                            pSmeStartBssRsp->HTProfile.htRecommendedTxWidthSet;
                    pSession->connectedProfile.HTProfile.htSecondaryChannelOffset =
                            pSmeStartBssRsp->HTProfile.htSecondaryChannelOffset;
#ifdef WLAN_FEATURE_11AC
                    pSession->connectedProfile.HTProfile.vhtCapability =
                                pSmeStartBssRsp->HTProfile.vhtCapability;
                    pSession->connectedProfile.HTProfile.vhtTxChannelWidthSet =
                                pSmeStartBssRsp->HTProfile.vhtTxChannelWidthSet;
                    pSession->connectedProfile.HTProfile.apCenterChan =
                                pSmeStartBssRsp->HTProfile.apCenterChan;
                    pSession->connectedProfile.HTProfile.apChanWidth =
                                pSmeStartBssRsp->HTProfile.apChanWidth;
#endif
                }
#endif
                csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, roamStatus, roamResult );
            }

            csrScanCancelIdleScan(pMac);

            if( CSR_IS_WDS_STA( pProfile ) )
            {
                //need to send stop BSS because we fail to send join_req
                csrRoamIssueDisassociateCmd( pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED );
                csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_WDS_IND, eCSR_ROAM_RESULT_WDS_STOPPED );
            }
            break;
        case eCsrStartBssFailure:
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
            {
                vos_log_ibss_pkt_type *pIbssLog;
                WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
                if(pIbssLog)
                {
                    pIbssLog->status = WLAN_IBSS_STATUS_FAILURE;
                    WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                }
            }
#endif //#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
            roamStatus = eCSR_ROAM_IBSS_IND;
            roamResult = eCSR_ROAM_RESULT_IBSS_STARTED;
            if( CSR_IS_WDS( pProfile ) )
            {
                roamStatus = eCSR_ROAM_WDS_IND;
                roamResult = eCSR_ROAM_RESULT_WDS_STARTED;
            }
            if( CSR_IS_INFRA_AP( pProfile ) )
            {
                roamStatus = eCSR_ROAM_INFRA_IND;
                roamResult = eCSR_ROAM_RESULT_INFRA_START_FAILED;
            }
            if(Context)
            {
                pSirBssDesc = (tSirBssDescription *)Context;
            }
            else
            {
                pSirBssDesc = NULL;
            }
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            roamInfo.pBssDesc = pSirBssDesc;
            /* We need to associate_complete it first, because
               Associate_start already indicated. */
            csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, roamStatus, roamResult );
            csrSetDefaultDot11Mode( pMac );
            break;
        case eCsrSilentlyStopRoaming:
            // We are here because we try to start the same IBSS
            //No message to PE
            // return the roaming state to Joined.
            smsLog(pMac, LOGW, FL("receives silently roaming indication"));
            csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            roamInfo.pBssDesc = pSession->pConnectBssDesc;
            if( roamInfo.pBssDesc )
            {
                vos_mem_copy(&roamInfo.bssid, &roamInfo.pBssDesc->bssId,
                             sizeof(tCsrBssid));
            }
            //Since there is no change in the current state, simply pass back no result otherwise
            //HDD may be mistakenly mark to disconnected state.
            csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_IBSS_IND, eCSR_ROAM_RESULT_NONE );
            break;
        case eCsrSilentlyStopRoamingSaveState:
            //We are here because we try to connect to the same AP
            //No message to PE
            smsLog(pMac, LOGW, FL("receives silently stop roaming indication"));
            vos_mem_set(&roamInfo, sizeof(roamInfo), 0);

            /* To avoid resetting the substate to NONE */
            pMac->roam.curState[sessionId] = eCSR_ROAMING_STATE_JOINED;
            //No need to change substate to wai_for_key because there is no state change
            roamInfo.pBssDesc = pSession->pConnectBssDesc;
            if( roamInfo.pBssDesc )
            {
                vos_mem_copy(&roamInfo.bssid, &roamInfo.pBssDesc->bssId,
                             sizeof(tCsrBssid));
            }
            roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
            roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
            roamInfo.nBeaconLength = pSession->connectedInfo.nBeaconLength;
            roamInfo.nAssocReqLength = pSession->connectedInfo.nAssocReqLength;
            roamInfo.nAssocRspLength = pSession->connectedInfo.nAssocRspLength;
            roamInfo.pbFrames = pSession->connectedInfo.pbFrames;
            roamInfo.staId = pSession->connectedInfo.staId;
            roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
            if (0 == roamInfo.staId)
               VOS_ASSERT(0);
            pSession->bRefAssocStartCnt--;
            csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);
            csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_ASSOCIATED, eANI_BOOLEAN_TRUE);
            break;
        case eCsrReassocFailure:
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
            sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_REASSOC_FAILURE, NULL);
#endif
        case eCsrJoinWdsFailure:
            smsLog(pMac, LOGW, FL("failed to join WDS"));
            csrFreeConnectBssDesc(pMac, sessionId);
            csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
            csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
            roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
            roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
            csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                    eCSR_ROAM_WDS_IND,
                                    eCSR_ROAM_RESULT_WDS_NOT_ASSOCIATED);
            //Need to issue stop_bss
            break;
        case eCsrJoinFailure:
        case eCsrNothingToJoin:
        case eCsrJoinFailureDueToConcurrency:
        default:
        {
            smsLog(pMac, LOGW, FL("receives no association indication"));
            smsLog(pMac, LOG1, FL("Assoc ref count %d"),
                   pSession->bRefAssocStartCnt);
            if( CSR_IS_INFRASTRUCTURE( &pSession->connectedProfile ) ||
                CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ( pMac, sessionId ) )
            {
                //do not free for the other profiles as we need to send down stop BSS later
                csrFreeConnectBssDesc(pMac, sessionId);
                csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
                csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
                csrSetDefaultDot11Mode( pMac );
            }

            switch( pCommand->u.roamCmd.roamReason )
            {
                // If this transition is because of an 802.11 OID, then we transition
                // back to INIT state so we sit waiting for more OIDs to be issued and
                // we don't start the IDLE timer.
                case eCsrSmeIssuedFTReassoc:
                case eCsrSmeIssuedAssocToSimilarAP:
                case eCsrHddIssued:
                case eCsrSmeIssuedDisassocForHandoff:
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId );
                    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                    roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
                    roamInfo.pProfile = &pCommand->u.roamCmd.roamProfile;
                    roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                    roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                    vos_mem_copy(&roamInfo.bssid,
                                 &pSession->joinFailStatusCode.bssId,
                                 sizeof(tCsrBssid));

                    /* Defeaturize this later if needed */
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
                    /* If Join fails while Handoff is in progress, indicate disassociated event to supplicant to reconnect */
                    if (csrRoamIsHandoffInProgress(pMac, sessionId))
                    {
                        /* Should indicate neighbor roam algorithm about the connect failure here */
                        csrNeighborRoamIndicateConnect(pMac, (tANI_U8)sessionId, VOS_STATUS_E_FAILURE);
                    }
#endif
                        if(pSession->bRefAssocStartCnt > 0)
                        {
                            pSession->bRefAssocStartCnt--;
                            if(eCsrJoinFailureDueToConcurrency == Result)
                            {
                                csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                                eCSR_ROAM_ASSOCIATION_COMPLETION,
                                                eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL);
                            }
                            else
                            {
                                csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                                eCSR_ROAM_ASSOCIATION_COMPLETION,
                                                eCSR_ROAM_RESULT_FAILURE);
                            }
                        }
                        else
                        {
                            /* bRefAssocStartCnt is not incremented when
                             * eRoamState == eCsrStopRoamingDueToConcurrency
                             * in csrRoamJoinNextBss API. so handle this in
                             * else case by sending assoc failure
                             */
                            csrRoamCallCallback(pMac, sessionId, &roamInfo,
                                    pCommand->u.scanCmd.roamId,
                                    eCSR_ROAM_ASSOCIATION_FAILURE,
                                    eCSR_ROAM_RESULT_FAILURE);
                        }
                    smsLog(pMac, LOG1, FL("  roam(reason %d) failed"), pCommand->u.roamCmd.roamReason);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    sme_QosUpdateHandOff((tANI_U8)sessionId, VOS_FALSE);
                    sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                    csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_FAILURE, eANI_BOOLEAN_FALSE);
                    csrScanStartIdleScan(pMac);
                    break;
                case eCsrHddIssuedReassocToSameAP:
                case eCsrSmeIssuedReassocToSameAP:
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId);

                    csrRoamCallCallback(pMac, sessionId, NULL, pCommand->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED, eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                    csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_FAILURE, eANI_BOOLEAN_FALSE);
                    csrScanStartIdleScan(pMac);
                    break;
                case eCsrForcedDisassoc:
                case eCsrForcedDeauth:
                case eCsrSmeIssuedIbssJoinFailure:
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId);

                    if(eCsrSmeIssuedIbssJoinFailure == pCommand->u.roamCmd.roamReason)
                    {
                        // Notify HDD that IBSS join failed
                        csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_IBSS_IND, eCSR_ROAM_RESULT_IBSS_JOIN_FAILED);
                    }
                    else
                    {
                        csrRoamCallCallback(pMac, sessionId, NULL,
                                            pCommand->u.roamCmd.roamId,
                                            eCSR_ROAM_DISASSOCIATED, eCSR_ROAM_RESULT_FORCED);
                    }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                    csrRoamLinkDown(pMac, sessionId);

                    /*
                     * DelSta not done FW still in connected state so dont
                     * issue IMPS req
                     */

                    if (pMac->roam.deauthRspStatus == eSIR_SME_DEAUTH_STATUS)
                    {
                        smsLog(pMac, LOGW, FL("FW still in connected state "));
                        break;
                    }
                    csrScanStartIdleScan(pMac);
                    break;
                case eCsrForcedIbssLeave:
                     csrRoamCallCallback(pMac, sessionId, NULL,
                                        pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_IBSS_LEAVE,
                                        eCSR_ROAM_RESULT_IBSS_STOP);
                    break;
                case eCsrForcedDisassocMICFailure:
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId );

                    csrRoamCallCallback(pMac, sessionId, NULL, pCommand->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED, eCSR_ROAM_RESULT_MIC_FAILURE);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_DISCONNECT_REQ, NULL);
#endif
                    csrScanStartIdleScan(pMac);
                    break;
                case eCsrStopBss:
                    csrRoamCallCallback(pMac, sessionId, NULL,
                                        pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_INFRA_IND,
                                        eCSR_ROAM_RESULT_INFRA_STOPPED);
                    break;
                case eCsrForcedDisassocSta:
                case eCsrForcedDeauthSta:
                   csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId);
                   if( CSR_IS_SESSION_VALID(pMac, sessionId) )
                   {
                       pSession = CSR_GET_SESSION(pMac, sessionId);

                       if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                       {
                           roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                           vos_mem_copy(roamInfo.peerMac,
                                        pCommand->u.roamCmd.peerMac,
                                        sizeof(tSirMacAddr));
                           roamInfo.reasonCode = eCSR_ROAM_RESULT_FORCED;
                           roamInfo.statusCode = eSIR_SME_SUCCESS;
                           status = csrRoamCallCallback(pMac, sessionId,
                                       &roamInfo, pCommand->u.roamCmd.roamId,
                                       eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                       }
                   }
                   break;
                case eCsrLostLink1:
                    // if lost link roam1 failed, then issue lost link Scan2 ...
                    csrScanRequestLostLink2(pMac, sessionId);
                    break;
                case eCsrLostLink2:
                    // if lost link roam2 failed, then issue lost link scan3 ...
                    csrScanRequestLostLink3(pMac, sessionId);
                    break;
                case eCsrLostLink3:
                default:
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId );

                    /* We are done with one round of lost link roaming here */
                    csrScanHandleFailedLostlink3(pMac, sessionId);
                    break;
            }
            break;
        }
    }
    return ( fReleaseCommand );
}

eHalStatus csrRoamCopyProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pDstProfile, tCsrRoamProfile *pSrcProfile)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 size = 0;

    do
    {
        vos_mem_set(pDstProfile, sizeof(tCsrRoamProfile), 0);
        if(pSrcProfile->BSSIDs.numOfBSSIDs)
        {
            size = sizeof(tCsrBssid) * pSrcProfile->BSSIDs.numOfBSSIDs;
            pDstProfile->BSSIDs.bssid = vos_mem_malloc(size);
            if ( NULL == pDstProfile->BSSIDs.bssid )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->BSSIDs.numOfBSSIDs = pSrcProfile->BSSIDs.numOfBSSIDs;
            vos_mem_copy(pDstProfile->BSSIDs.bssid,
                         pSrcProfile->BSSIDs.bssid, size);
        }
        if(pSrcProfile->SSIDs.numOfSSIDs)
        {
            size = sizeof(tCsrSSIDInfo) * pSrcProfile->SSIDs.numOfSSIDs;
            pDstProfile->SSIDs.SSIDList = vos_mem_malloc(size);
            if ( NULL == pDstProfile->SSIDs.SSIDList )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if (!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->SSIDs.numOfSSIDs = pSrcProfile->SSIDs.numOfSSIDs;
            vos_mem_copy(pDstProfile->SSIDs.SSIDList,
                         pSrcProfile->SSIDs.SSIDList, size);
        }
        if(pSrcProfile->nWPAReqIELength)
        {
            pDstProfile->pWPAReqIE = vos_mem_malloc(pSrcProfile->nWPAReqIELength);
            if ( NULL == pDstProfile->pWPAReqIE )
               status = eHAL_STATUS_FAILURE;
            else
               status = eHAL_STATUS_SUCCESS;

            if (!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nWPAReqIELength = pSrcProfile->nWPAReqIELength;
            vos_mem_copy(pDstProfile->pWPAReqIE, pSrcProfile->pWPAReqIE,
                         pSrcProfile->nWPAReqIELength);
        }
        if(pSrcProfile->nRSNReqIELength)
        {
            pDstProfile->pRSNReqIE = vos_mem_malloc(pSrcProfile->nRSNReqIELength);
            if ( NULL == pDstProfile->pRSNReqIE )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;

            if (!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nRSNReqIELength = pSrcProfile->nRSNReqIELength;
            vos_mem_copy(pDstProfile->pRSNReqIE, pSrcProfile->pRSNReqIE,
                         pSrcProfile->nRSNReqIELength);
        }
#ifdef FEATURE_WLAN_WAPI
        if(pSrcProfile->nWAPIReqIELength)
        {
            pDstProfile->pWAPIReqIE = vos_mem_malloc(pSrcProfile->nWAPIReqIELength);
            if ( NULL == pDstProfile->pWAPIReqIE )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nWAPIReqIELength = pSrcProfile->nWAPIReqIELength;
            vos_mem_copy(pDstProfile->pWAPIReqIE, pSrcProfile->pWAPIReqIE,
                         pSrcProfile->nWAPIReqIELength);
        }
#endif /* FEATURE_WLAN_WAPI */
        if(pSrcProfile->nAddIEScanLength)
        {
            pDstProfile->pAddIEScan = vos_mem_malloc(pSrcProfile->nAddIEScanLength);
            if ( NULL == pDstProfile->pAddIEScan )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;

            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nAddIEScanLength = pSrcProfile->nAddIEScanLength;
            vos_mem_copy(pDstProfile->pAddIEScan, pSrcProfile->pAddIEScan,
                         pSrcProfile->nAddIEScanLength);
        }
        if(pSrcProfile->nAddIEAssocLength)
        {
            pDstProfile->pAddIEAssoc = vos_mem_malloc(pSrcProfile->nAddIEAssocLength);
            if ( NULL == pDstProfile->pAddIEAssoc )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;

            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nAddIEAssocLength = pSrcProfile->nAddIEAssocLength;
            vos_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
                         pSrcProfile->nAddIEAssocLength);
        }
        if(pSrcProfile->ChannelInfo.ChannelList)
        {
            pDstProfile->ChannelInfo.ChannelList = vos_mem_malloc(
                                    pSrcProfile->ChannelInfo.numOfChannels);
            if ( NULL == pDstProfile->ChannelInfo.ChannelList )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->ChannelInfo.numOfChannels = pSrcProfile->ChannelInfo.numOfChannels;
            vos_mem_copy(pDstProfile->ChannelInfo.ChannelList,
                         pSrcProfile->ChannelInfo.ChannelList,
                         pSrcProfile->ChannelInfo.numOfChannels);
        }
        pDstProfile->AuthType = pSrcProfile->AuthType;
        pDstProfile->EncryptionType = pSrcProfile->EncryptionType;
        pDstProfile->mcEncryptionType = pSrcProfile->mcEncryptionType;
        pDstProfile->negotiatedUCEncryptionType = pSrcProfile->negotiatedUCEncryptionType;
        pDstProfile->negotiatedMCEncryptionType = pSrcProfile->negotiatedMCEncryptionType;
        pDstProfile->negotiatedAuthType = pSrcProfile->negotiatedAuthType;
#ifdef WLAN_FEATURE_11W
        pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
        pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
        pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
        pDstProfile->BSSType = pSrcProfile->BSSType;
        pDstProfile->phyMode = pSrcProfile->phyMode;
        pDstProfile->csrPersona = pSrcProfile->csrPersona;

#ifdef FEATURE_WLAN_WAPI
        if(csrIsProfileWapi(pSrcProfile))
        {
             if(pDstProfile->phyMode & eCSR_DOT11_MODE_11n)
             {
                pDstProfile->phyMode &= ~eCSR_DOT11_MODE_11n;
             }
        }
#endif /* FEATURE_WLAN_WAPI */
        pDstProfile->CBMode = pSrcProfile->CBMode;
        pDstProfile->vht_channel_width = pSrcProfile->vht_channel_width;
        /*Save the WPS info*/
        pDstProfile->bWPSAssociation = pSrcProfile->bWPSAssociation;
        pDstProfile->bOSENAssociation = pSrcProfile->bOSENAssociation;
        pDstProfile->uapsd_mask = pSrcProfile->uapsd_mask;
        pDstProfile->beaconInterval = pSrcProfile->beaconInterval;
        pDstProfile->privacy           = pSrcProfile->privacy;
        pDstProfile->fwdWPSPBCProbeReq = pSrcProfile->fwdWPSPBCProbeReq;
        pDstProfile->csr80211AuthType  = pSrcProfile->csr80211AuthType;
        pDstProfile->dtimPeriod        = pSrcProfile->dtimPeriod;
        pDstProfile->ApUapsdEnable     = pSrcProfile->ApUapsdEnable;
        pDstProfile->SSIDs.SSIDList[0].ssidHidden = pSrcProfile->SSIDs.SSIDList[0].ssidHidden;
        pDstProfile->protEnabled       = pSrcProfile->protEnabled;
        pDstProfile->obssProtEnabled   = pSrcProfile->obssProtEnabled;
        pDstProfile->cfg_protection    = pSrcProfile->cfg_protection;
        pDstProfile->wps_state         = pSrcProfile->wps_state;
        pDstProfile->ieee80211d        = pSrcProfile->ieee80211d;
        pDstProfile->sap_dot11mc        = pSrcProfile->sap_dot11mc;
        vos_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
                     sizeof(pDstProfile->Keys));
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pSrcProfile->MDID.mdiePresent)
        {
            pDstProfile->MDID.mdiePresent = 1;
            pDstProfile->MDID.mobilityDomain = pSrcProfile->MDID.mobilityDomain;
        }
#endif
        vos_mem_copy(&pDstProfile->addIeParams,
                     &pSrcProfile->addIeParams,
                     sizeof(tSirAddIeParams));
    }while(0);

    if(!HAL_STATUS_SUCCESS(status))
    {
        csrReleaseProfile(pMac, pDstProfile);
        pDstProfile = NULL;
    }

    return (status);
}

eHalStatus csrRoamCopyConnectedProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pDstProfile )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamConnectedProfile *pSrcProfile = &pMac->roam.roamSession[sessionId].connectedProfile;
    do
    {
        vos_mem_set(pDstProfile, sizeof(tCsrRoamProfile), 0);
        if(pSrcProfile->bssid)
        {
            pDstProfile->BSSIDs.bssid = vos_mem_malloc(sizeof(tCsrBssid));
            if ( NULL == pDstProfile->BSSIDs.bssid )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE,
                    FL("failed to allocate memory for BSSID"
                    "%02x:%02x:%02x:%02x:%02x:%02x"),
                    pSrcProfile->bssid[0], pSrcProfile->bssid[1],
                    pSrcProfile->bssid[2], pSrcProfile->bssid[3],
                    pSrcProfile->bssid[4], pSrcProfile->bssid[5]);
                break;
            }
            pDstProfile->BSSIDs.numOfBSSIDs = 1;
            vos_mem_copy(pDstProfile->BSSIDs.bssid, pSrcProfile->bssid,
                         sizeof(tCsrBssid));
        }
        if(pSrcProfile->SSID.ssId)
        {
            pDstProfile->SSIDs.SSIDList = vos_mem_malloc(sizeof(tCsrSSIDInfo));
            if ( NULL == pDstProfile->SSIDs.SSIDList )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE,
                 FL("failed to allocate memory for SSIDList"
                    "%02x:%02x:%02x:%02x:%02x:%02x"),
                    pSrcProfile->bssid[0], pSrcProfile->bssid[1],
                    pSrcProfile->bssid[2], pSrcProfile->bssid[3],
                    pSrcProfile->bssid[4], pSrcProfile->bssid[5]);
                break;
            }
            pDstProfile->SSIDs.numOfSSIDs = 1;
            pDstProfile->SSIDs.SSIDList[0].handoffPermitted = pSrcProfile->handoffPermitted;
            pDstProfile->SSIDs.SSIDList[0].ssidHidden = pSrcProfile->ssidHidden;
            vos_mem_copy(&pDstProfile->SSIDs.SSIDList[0].SSID,
                         &pSrcProfile->SSID, sizeof(tSirMacSSid));
        }
        if(pSrcProfile->nAddIEAssocLength)
        {
            pDstProfile->pAddIEAssoc = vos_mem_malloc(pSrcProfile->nAddIEAssocLength);
            if ( NULL == pDstProfile->pAddIEAssoc)
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog( pMac, LOGE, FL(" failed to allocate memory for additional IEs ") );
                break;
            }
            pDstProfile->nAddIEAssocLength = pSrcProfile->nAddIEAssocLength;
            vos_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
                         pSrcProfile->nAddIEAssocLength);
        }
        pDstProfile->ChannelInfo.ChannelList = vos_mem_malloc(1);
        if ( NULL == pDstProfile->ChannelInfo.ChannelList )
                status = eHAL_STATUS_FAILURE;
        else
                status = eHAL_STATUS_SUCCESS;

        if(!HAL_STATUS_SUCCESS(status))
        {
           break;
        }
        pDstProfile->ChannelInfo.numOfChannels = 1;
        pDstProfile->ChannelInfo.ChannelList[0] = pSrcProfile->operationChannel;
        pDstProfile->AuthType.numEntries = 1;
        pDstProfile->AuthType.authType[0] = pSrcProfile->AuthType;
        pDstProfile->negotiatedAuthType = pSrcProfile->AuthType;
        pDstProfile->EncryptionType.numEntries = 1;
        pDstProfile->EncryptionType.encryptionType[0] = pSrcProfile->EncryptionType;
        pDstProfile->negotiatedUCEncryptionType = pSrcProfile->EncryptionType;
        pDstProfile->mcEncryptionType.numEntries = 1;
        pDstProfile->mcEncryptionType.encryptionType[0] = pSrcProfile->mcEncryptionType;
        pDstProfile->negotiatedMCEncryptionType = pSrcProfile->mcEncryptionType;
        pDstProfile->BSSType = pSrcProfile->BSSType;
        pDstProfile->CBMode = pSrcProfile->CBMode;
        vos_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
                     sizeof(pDstProfile->Keys));
#ifdef WLAN_FEATURE_11W
        pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
        pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
        pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pSrcProfile->MDID.mdiePresent)
        {
            pDstProfile->MDID.mdiePresent = 1;
            pDstProfile->MDID.mobilityDomain = pSrcProfile->MDID.mobilityDomain;
        }
#endif

    }while(0);

    if(!HAL_STATUS_SUCCESS(status))
    {
        csrReleaseProfile(pMac, pDstProfile);
        pDstProfile = NULL;
    }

    return (status);
}

eHalStatus csrRoamIssueConnect(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                tScanResultHandle hBSSList,
                                eCsrRoamReason reason, tANI_U32 roamId, tANI_BOOLEAN fImediate,
                                tANI_BOOLEAN fClearScan)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    pCommand = csrGetCommandBuffer(pMac);
    if(NULL == pCommand)
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer") );
        status = eHAL_STATUS_RESOURCES;
    }
    else
    {
        if( fClearScan )
        {
            csrScanCancelIdleScan(pMac);
            csrScanAbortMacScanNotForConnect(pMac, sessionId);
        }
        pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_FALSE;
        if(NULL == pProfile)
        {
            //We can roam now
            //Since pProfile is NULL, we need to build our own profile, set everything to default
            //We can only support open and no encryption
            pCommand->u.roamCmd.roamProfile.AuthType.numEntries = 1;
            pCommand->u.roamCmd.roamProfile.AuthType.authType[0] = eCSR_AUTH_TYPE_OPEN_SYSTEM;
            pCommand->u.roamCmd.roamProfile.EncryptionType.numEntries = 1;
            pCommand->u.roamCmd.roamProfile.EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
            pCommand->u.roamCmd.roamProfile.csrPersona = VOS_STA_MODE;
        }
        else
        {
            //make a copy of the profile
            status = csrRoamCopyProfile(pMac, &pCommand->u.roamCmd.roamProfile, pProfile);
            if(HAL_STATUS_SUCCESS(status))
            {
                pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_TRUE;
            }
        }

        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.hBSSList = hBSSList;
        pCommand->u.roamCmd.roamId = roamId;
        pCommand->u.roamCmd.roamReason = reason;
        //We need to free the BssList when the command is done
        pCommand->u.roamCmd.fReleaseBssList = eANI_BOOLEAN_TRUE;
        pCommand->u.roamCmd.fUpdateCurRoamProfile = eANI_BOOLEAN_TRUE;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  FL("CSR PERSONA=%d"),
                  pCommand->u.roamCmd.roamProfile.csrPersona);
        status = csrQueueSmeCommand(pMac, pCommand, fImediate);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }

    return (status);
}
eHalStatus csrRoamIssueReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                               tCsrRoamModifyProfileFields *pMmodProfileFields,
                               eCsrRoamReason reason, tANI_U32 roamId, tANI_BOOLEAN fImediate)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    pCommand = csrGetCommandBuffer(pMac);
    if(NULL == pCommand)
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer") );
        status = eHAL_STATUS_RESOURCES;
    }
    else
    {
        csrScanCancelIdleScan(pMac);
        csrScanAbortMacScanNotForConnect(pMac, sessionId);
        if(pProfile)
        {
           //This is likely trying to reassoc to different profile
           pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_FALSE;
           //make a copy of the profile
           status = csrRoamCopyProfile(pMac, &pCommand->u.roamCmd.roamProfile, pProfile);
           pCommand->u.roamCmd.fUpdateCurRoamProfile = eANI_BOOLEAN_TRUE;
        }
        else
        {
            status = csrRoamCopyConnectedProfile(pMac, sessionId, &pCommand->u.roamCmd.roamProfile);
            //how to update WPA/WPA2 info in roamProfile??
            pCommand->u.roamCmd.roamProfile.uapsd_mask = pMmodProfileFields->uapsd_mask;
        }
        if(HAL_STATUS_SUCCESS(status))
        {
           pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_TRUE;
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.roamId = roamId;
        pCommand->u.roamCmd.roamReason = reason;
        //We need to free the BssList when the command is done
        //For reassoc there is no BSS list, so the boolean set to false
        pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
        pCommand->u.roamCmd.fReleaseBssList = eANI_BOOLEAN_FALSE;
        pCommand->u.roamCmd.fReassoc = eANI_BOOLEAN_TRUE;
        csrRoamRemoveDuplicateCommand(pMac, sessionId, pCommand, reason);
        status = csrQueueSmeCommand(pMac, pCommand, fImediate);
        if( !HAL_STATUS_SUCCESS( status ) )
    {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_FAILURE, eANI_BOOLEAN_FALSE);
            csrReleaseCommandRoam( pMac, pCommand );
    }
    }
    return (status);
}

eHalStatus csrRoamEnqueuePreauth(tpAniSirGlobal pMac, tANI_U32 sessionId, tpSirBssDescription pBssDescription,
                                eCsrRoamReason reason, tANI_BOOLEAN fImmediate)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    pCommand = csrGetCommandBuffer(pMac);
    if(NULL == pCommand)
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer") );
        status = eHAL_STATUS_RESOURCES;
    }
    else
    {
        if(pBssDescription)
        {
            //copy over the parameters we need later
            pCommand->command = eSmeCommandRoam;
            pCommand->sessionId = (tANI_U8)sessionId;
            pCommand->u.roamCmd.roamReason = reason;
            //this is the important parameter
            //in this case we are using this field for the "next" BSS
            pCommand->u.roamCmd.pLastRoamBss = pBssDescription;
            status = csrQueueSmeCommand(pMac, pCommand, fImmediate);
            if( !HAL_STATUS_SUCCESS( status ) )
            {
                smsLog( pMac, LOGE, FL(" fail to enqueue preauth command, status = %d"), status );
                csrReleaseCommandPreauth( pMac, pCommand );
            }
        }
        else
        {
           //Return failure
           status = eHAL_STATUS_RESOURCES;
        }
    }
    return (status);
}

eHalStatus csrDequeueRoamCommand(tpAniSirGlobal pMac, eCsrRoamReason reason)
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
    if ( pEntry )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        if ( (eSmeCommandRoam == pCommand->command) &&
                (eCsrPerformPreauth == reason))
        {
            smsLog( pMac, LOG1, FL("DQ-Command = %d, Reason = %d"),
                    pCommand->command, pCommand->u.roamCmd.roamReason);
            if (csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK )) {
                csrReleaseCommandPreauth( pMac, pCommand );
            }
        }
        else if ((eSmeCommandRoam == pCommand->command) &&
                (eCsrSmeIssuedFTReassoc == reason))
        {
            smsLog( pMac, LOG1, FL("DQ-Command = %d, Reason = %d"),
                    pCommand->command, pCommand->u.roamCmd.roamReason);
            if (csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK )) {
                csrReleaseCommandRoam( pMac, pCommand );
            }
        }
        else  {
            smsLog( pMac, LOGE, FL("Command = %d, Reason = %d "),
                    pCommand->command, pCommand->u.roamCmd.roamReason);
        }
    }
    else {
        smsLog( pMac, LOGE, FL("pEntry NULL for eWNI_SME_FT_PRE_AUTH_RSP"));
    }
    smeProcessPendingQueue( pMac );
    return eHAL_STATUS_SUCCESS;
}

eHalStatus csrRoamConnectWithBSSList(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                     tScanResultHandle hBssListIn, tANI_U32 *pRoamId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tScanResultHandle hBSSList;
    tANI_U32 roamId = 0;
    status = csrScanCopyResultList(pMac, hBssListIn, &hBSSList);
    if(HAL_STATUS_SUCCESS(status))
    {
        roamId = GET_NEXT_ROAM_ID(&pMac->roam);
        if(pRoamId)
        {
            *pRoamId = roamId;
        }
        status = csrRoamIssueConnect(pMac, sessionId, pProfile, hBSSList, eCsrHddIssued,
                                        roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
        if(!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGE, FL("failed to start a join process"));
            csrScanResultPurge(pMac, hBSSList);
        }
    }
    return (status);
}

eHalStatus csrRoamConnect(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                          tScanResultHandle hBssListIn, tANI_U32 *pRoamId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tScanResultHandle hBSSList;
    tCsrScanResultFilter *pScanFilter;
    tANI_U32 roamId = 0;
    tANI_BOOLEAN fCallCallback = eANI_BOOLEAN_FALSE;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if (NULL == pSession) {
        smsLog(pMac, LOGE,
               FL("session does not exist for given sessionId:%d"), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if (NULL == pProfile) {
        smsLog(pMac, LOGP, FL("No profile specified"));
        return eHAL_STATUS_FAILURE;
    }
    /* Initialize the bssid count before proceeding with the Join requests */
    pSession->join_bssid_count = 0;
    smsLog(pMac, LOG1, FL("called  BSSType = %s (%d) authtype = %d "
                                                    "encryType = %d"),
                lim_BssTypetoString(pProfile->BSSType),
                pProfile->BSSType,
                pProfile->AuthType.authType[0],
                pProfile->EncryptionType.encryptionType[0]);

    if( CSR_IS_WDS( pProfile ) &&
        !HAL_STATUS_SUCCESS( status = csrIsBTAMPAllowed( pMac, pProfile->operationChannel ) ) )
    {
        smsLog(pMac, LOGE, FL("Request for BT AMP connection failed, channel requested is different than infra = %d"),
               pProfile->operationChannel);
        return status;
    }
    csrRoamCancelRoaming(pMac, sessionId);
    csrScanRemoveFreshScanCommand(pMac, sessionId);
    csrScanCancelIdleScan(pMac);
    //Only abort the scan if it is not used for other roam/connect purpose
    csrScanAbortMacScan(pMac, sessionId, eCSR_SCAN_ABORT_DEFAULT);
    csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrHddIssued);
    //Check whether ssid changes
    if(csrIsConnStateConnected(pMac, sessionId))
    {
        if(pProfile->SSIDs.numOfSSIDs && !csrIsSsidInList(pMac, &pSession->connectedProfile.SSID, &pProfile->SSIDs))
        {
            csrRoamIssueDisassociateCmd(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
        }
    }
    /*
     * If roamSession.connectState is disconnecting that mean
     * disconnect was received with scan for ssid in progress
     * and dropped. This state will ensure that connect will
     * not be issued from scan for ssid completion. Thus
     * if this fresh connect also issue scan for ssid the connect
     * command will be dropped assuming disconnect is in progress.
     * Thus reset connectState here
     */
    if (eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING ==
         pMac->roam.roamSession[sessionId].connectState)
        pMac->roam.roamSession[sessionId].connectState =
                       eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
    if(CSR_INVALID_SCANRESULT_HANDLE != hBssListIn)
    {
        smsLog(pMac, LOG1, FL("is called with BSSList"));
        status = csrRoamConnectWithBSSList(pMac, sessionId, pProfile, hBssListIn, pRoamId);
        if(pRoamId)
        {
            roamId = *pRoamId;
        }
        if(!HAL_STATUS_SUCCESS(status))
        {
            fCallCallback = eANI_BOOLEAN_TRUE;
        }
    }
    else
    {
        pScanFilter = vos_mem_malloc(sizeof(tCsrScanResultFilter));
        if ( NULL == pScanFilter )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if(HAL_STATUS_SUCCESS(status))
        {
            vos_mem_set(pScanFilter, sizeof(tCsrScanResultFilter), 0);
            //Try to connect to any BSS
            if(NULL == pProfile)
            {
                //No encryption
                pScanFilter->EncryptionType.numEntries = 1;
                pScanFilter->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
            }//we don't have a profile
            else
            {
                //Here is the profile we need to connect to
                status = csrRoamPrepareFilterFromProfile(pMac, pProfile, pScanFilter);
            }//We have a profile
            roamId = GET_NEXT_ROAM_ID(&pMac->roam);
            if(pRoamId)
            {
                *pRoamId = roamId;
            }

            if(HAL_STATUS_SUCCESS(status))
            {
                /*Save the WPS info*/
                if(NULL != pProfile)
                {
                    pScanFilter->bWPSAssociation = pProfile->bWPSAssociation;
                    pScanFilter->bOSENAssociation = pProfile->bOSENAssociation;
                }
                else
                {
                    pScanFilter->bWPSAssociation = 0;
                    pScanFilter->bOSENAssociation = 0;
                }
                do
                {
                    if( (pProfile && CSR_IS_WDS_AP( pProfile ) )
                     || (pProfile && CSR_IS_INFRA_AP ( pProfile ))
                    )
                    {
                        //This can be started right away
                        status = csrRoamIssueConnect(pMac, sessionId, pProfile, NULL, eCsrHddIssued,
                                                    roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                        if(!HAL_STATUS_SUCCESS(status))
                        {
                            smsLog(pMac, LOGE, FL("   CSR failed to issue start BSS command with status = 0x%08X"), status);
                            fCallCallback = eANI_BOOLEAN_TRUE;
                        }
                        else
                        {
                            smsLog(pMac, LOG1, FL("Connect request to proceed for AMP/SoftAP mode"));
                        }
                        break;
                    }
                    status = csrScanGetResult(pMac, pScanFilter, &hBSSList);
                    smsLog(pMac, LOG1, "************ csrScanGetResult Status ********* %d", status);
                    if(HAL_STATUS_SUCCESS(status))
                    {
                        status = csrRoamIssueConnect(pMac, sessionId, pProfile, hBSSList, eCsrHddIssued,
                                                    roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                        if(!HAL_STATUS_SUCCESS(status))
                        {
                            smsLog(pMac, LOGE, FL("   CSR failed to issue connect command with status = 0x%08X"), status);
                            csrScanResultPurge(pMac, hBSSList);
                            fCallCallback = eANI_BOOLEAN_TRUE;
                        }
                    }//Have scan result
                    else if(NULL != pProfile)
                    {
                        //Check whether it is for start ibss
                        if(CSR_IS_START_IBSS(pProfile))
                        {
                            status = csrRoamIssueConnect(pMac, sessionId, pProfile, NULL, eCsrHddIssued,
                                                        roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                smsLog(pMac, LOGE, "   CSR failed to issue startIBSS command with status = 0x%08X", status);
                                fCallCallback = eANI_BOOLEAN_TRUE;
                            }
                        }
                        else
                        {
                            //scan for this SSID
                            status = csrScanForSSID(pMac, sessionId, pProfile, roamId, TRUE);
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                smsLog(pMac, LOGE, FL("   CSR failed to issue SSID scan command with status = 0x%08X"), status);
                                fCallCallback = eANI_BOOLEAN_TRUE;
                            }
                            else
                            {
                                smsLog(pMac, LOG1, FL("SSID scan requested for Infra connect req"));
                            }
                        }
                    }
                    else
                    {
                        fCallCallback = eANI_BOOLEAN_TRUE;
                    }
                } while (0);
                if(NULL != pProfile)
                {
                    //we need to free memory for filter if profile exists
                    csrFreeScanFilter(pMac, pScanFilter);
                }
            }//Got the scan filter from profile

            vos_mem_free(pScanFilter);
        }//allocated memory for pScanFilter
    }//No Bsslist coming in
    //tell the caller if we fail to trigger a join request
    if( fCallCallback )
    {
        csrRoamCallCallback(pMac, sessionId, NULL, roamId, eCSR_ROAM_FAILED, eCSR_ROAM_RESULT_FAILURE);
    }

    return (status);
}
eHalStatus csrRoamReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                          tCsrRoamModifyProfileFields modProfileFields,
                          tANI_U32 *pRoamId)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tANI_BOOLEAN fCallCallback = eANI_BOOLEAN_TRUE;
   tANI_U32 roamId = 0;
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   if (NULL == pProfile)
   {
      smsLog(pMac, LOGP, FL("No profile specified"));
      return eHAL_STATUS_FAILURE;
   }
   smsLog(pMac, LOG1, FL("called  BSSType = %s (%d) authtype = %d "
                                                  "encryType = %d"),
            lim_BssTypetoString(pProfile->BSSType),
            pProfile->BSSType,
            pProfile->AuthType.authType[0],
            pProfile->EncryptionType.encryptionType[0]);

   csrScanRemoveFreshScanCommand(pMac, sessionId);
   csrScanCancelIdleScan(pMac);
   csrScanAbortMacScanNotForConnect(pMac, sessionId);
   csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrHddIssuedReassocToSameAP);
   if(csrIsConnStateConnected(pMac, sessionId))
   {
      if(pProfile)
      {
         if(pProfile->SSIDs.numOfSSIDs &&
            csrIsSsidInList(pMac, &pSession->connectedProfile.SSID, &pProfile->SSIDs))
         {
            fCallCallback = eANI_BOOLEAN_FALSE;
         }
         else
         {
            smsLog(pMac, LOG1, FL("Not connected to the same SSID asked in the profile"));
         }
      }
      else if (!vos_mem_compare(&modProfileFields,
                                &pSession->connectedProfile.modifyProfileFields,
                                sizeof(tCsrRoamModifyProfileFields)))
      {
         fCallCallback = eANI_BOOLEAN_FALSE;
      }
      else
      {
         smsLog(pMac, LOG1, FL("Either the profile is NULL or none of the fields "
                               "in tCsrRoamModifyProfileFields got modified"));
      }
   }
   else
   {
      smsLog(pMac, LOG1, FL("Not connected! No need to reassoc"));
   }
   if(!fCallCallback)
   {
      roamId = GET_NEXT_ROAM_ID(&pMac->roam);
      if(pRoamId)
      {
         *pRoamId = roamId;
      }

      status = csrRoamIssueReassoc(pMac, sessionId, pProfile, &modProfileFields,
                                   eCsrHddIssuedReassocToSameAP, roamId, eANI_BOOLEAN_FALSE);
   }
   else
   {
      status = csrRoamCallCallback(pMac, sessionId, NULL, roamId,
                                   eCSR_ROAM_FAILED, eCSR_ROAM_RESULT_FAILURE);
   }
   return status;
}
eHalStatus csrRoamJoinLastProfile(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tScanResultHandle hBSSList = NULL;
    tCsrScanResultFilter *pScanFilter = NULL;
    tANI_U32 roamId;
    tCsrRoamProfile *pProfile = NULL;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    do
    {
        if(pSession->pCurRoamProfile)
        {
            csrScanCancelIdleScan(pMac);
            csrScanAbortMacScanNotForConnect(pMac, sessionId);
            //We have to make a copy of pCurRoamProfile because it will be free inside csrRoamIssueConnect
            pProfile = vos_mem_malloc(sizeof(tCsrRoamProfile));
            if ( NULL == pProfile )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
                break;
            vos_mem_set(pProfile, sizeof(tCsrRoamProfile), 0);
            status = csrRoamCopyProfile(pMac, pProfile, pSession->pCurRoamProfile);
            if (!HAL_STATUS_SUCCESS(status))
                break;
            pScanFilter = vos_mem_malloc(sizeof(tCsrScanResultFilter));
            if ( NULL  == pScanFilter )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;

            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            vos_mem_set(pScanFilter, sizeof(tCsrScanResultFilter), 0);
            status = csrRoamPrepareFilterFromProfile(pMac, pProfile, pScanFilter);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            roamId = GET_NEXT_ROAM_ID(&pMac->roam);
            status = csrScanGetResult(pMac, pScanFilter, &hBSSList);
            if(HAL_STATUS_SUCCESS(status))
            {
                //we want to put the last connected BSS to the very beginning, if possible
                csrMoveBssToHeadFromBSSID(pMac, &pSession->connectedProfile.bssid, hBSSList);
                status = csrRoamIssueConnect(pMac, sessionId, pProfile, hBSSList, eCsrHddIssued,
                                                roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    csrScanResultPurge(pMac, hBSSList);
                    break;
                }
            }
            else
            {
                //Do a scan on this profile
                //scan for this SSID only in case the AP suppresses SSID
                status = csrScanForSSID(pMac, sessionId, pProfile, roamId, TRUE);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    break;
                }
            }
        }//We have a profile
        else
        {
            smsLog(pMac, LOGW, FL("cannot find a roaming profile"));
            break;
        }
    }while(0);
    if(pScanFilter)
    {
        csrFreeScanFilter(pMac, pScanFilter);
        vos_mem_free(pScanFilter);
    }
    if(NULL != pProfile)
    {
        csrReleaseProfile(pMac, pProfile);
        vos_mem_free(pProfile);
    }
    return (status);
}
eHalStatus csrRoamReconnect(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    if(csrIsConnStateConnected(pMac, sessionId))
    {
        status = csrRoamIssueDisassociateCmd(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
        if(HAL_STATUS_SUCCESS(status))
        {
            status = csrRoamJoinLastProfile(pMac, sessionId);
        }
    }
    return (status);
}

eHalStatus csrRoamConnectToLastProfile(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    smsLog(pMac, LOGW, FL("is called"));
    csrRoamCancelRoaming(pMac, sessionId);
    csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrHddIssued);
    if(csrIsConnStateDisconnected(pMac, sessionId))
    {
        status = csrRoamJoinLastProfile(pMac, sessionId);
    }
    return (status);
}

eHalStatus csrRoamProcessDisassocDeauth( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fDisassoc, tANI_BOOLEAN fMICFailure )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fComplete = eANI_BOOLEAN_FALSE;
    eCsrRoamSubState NewSubstate;
    tANI_U32 sessionId = pCommand->sessionId;

    if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
    {
        smsLog(pMac, LOG1, FL(" Stop Wait for key timer and change substate to"
                              " eCSR_ROAM_SUBSTATE_NONE"));
        csrRoamStopWaitForKeyTimer( pMac );
        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
    }
    // change state to 'Roaming'...
    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId );

    if ( csrIsConnStateIbss( pMac, sessionId ) )
    {
        // If we are in an IBSS, then stop the IBSS...
        status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
        fComplete = (!HAL_STATUS_SUCCESS(status));
    }
    else if ( csrIsConnStateInfra( pMac, sessionId ) )
    {
        /* In Infrastructure, we need to disassociate from the
           Infrastructure network... */
        NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
        if(eCsrSmeIssuedDisassocForHandoff == pCommand->u.roamCmd.roamReason)
        {
            NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF;
        }
        else if ((eCsrForcedDisassoc == pCommand->u.roamCmd.roamReason)
            && (eSIR_MAC_DISASSOC_LEAVING_BSS_REASON ==
            pCommand->u.roamCmd.reason))
        {
            NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("set to substate eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT"));
        }
        if (eCsrSmeIssuedDisassocForHandoff != pCommand->u.roamCmd.roamReason) {
            // If we are in neighbor preauth done state then on receiving
            // disassoc or deauth we dont roam instead we just disassoc
            // from current ap and then go to disconnected state
            // This happens for ESE and 11r FT connections ONLY.
#ifdef WLAN_FEATURE_VOWIFI_11R
            if (csrRoamIs11rAssoc(pMac, sessionId) &&
                                     (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac, sessionId);
            }
#endif
#ifdef FEATURE_WLAN_ESE
            if (csrRoamIsESEAssoc(pMac, sessionId) &&
                                     (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac, sessionId);
            }
#endif
#ifdef FEATURE_WLAN_LFR
            if (csrRoamIsFastRoamEnabled(pMac, sessionId) &&
                                     (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac, sessionId);
            }
#endif
        }
        if( fDisassoc )
        {
            status = csrRoamIssueDisassociate( pMac, sessionId, NewSubstate, fMICFailure );
        }
        else
        {
            status = csrRoamIssueDeauth( pMac, sessionId, eCSR_ROAM_SUBSTATE_DEAUTH_REQ );
        }
        fComplete = (!HAL_STATUS_SUCCESS(status));
    }
    else if ( csrIsConnStateWds( pMac, sessionId ) )
    {
        if( CSR_IS_WDS_AP( &pMac->roam.roamSession[sessionId].connectedProfile ) )
        {
            status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
            fComplete = (!HAL_STATUS_SUCCESS(status));
        }
        //This has to be WDS station
        else  if( csrIsConnStateConnectedWds( pMac, sessionId ) ) //This has to be WDS station
        {

            pCommand->u.roamCmd.fStopWds = eANI_BOOLEAN_TRUE;
            if( fDisassoc )
            {
                status = csrRoamIssueDisassociate( pMac, sessionId,
                                eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, fMICFailure );
                fComplete = (!HAL_STATUS_SUCCESS(status));
            }
        }
    } else {
        // we got a dis-assoc request while not connected to any peer
        // just complete the command
           fComplete = eANI_BOOLEAN_TRUE;
           status = eHAL_STATUS_FAILURE;
    }
    if(fComplete)
    {
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }

    if(HAL_STATUS_SUCCESS(status))
    {
        if ( csrIsConnStateInfra( pMac, sessionId ) )
        {
            //Set the state to disconnect here
            pMac->roam.roamSession[sessionId].connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
        }
    }
    else
    {
        smsLog(pMac, LOGW, FL(" failed with status %d"), status);
    }
    return (status);
}

/**
 * csr_prepare_disconnect_command() - function to prepare disconnect command
 * @mac: pointer to global mac structure
 * @session_id: sme session index
 * @sme_cmd: pointer to sme command being prepared
 *
 * Function to prepare internal sme disconnect command
 * Return: eHAL_STATUS_SUCCESS on success else eHAL_STATUS_RESOURCES on failure
 */

eHalStatus csr_prepare_disconnect_command(tpAniSirGlobal mac,
                                        tANI_U32 session_id, tSmeCmd **sme_cmd)
{
	tSmeCmd *command;

	command = csrGetCommandBuffer(mac);
	if (!command) {
		smsLog(mac, LOGE, FL("fail to get command buffer"));
		return eHAL_STATUS_RESOURCES;
	}

	command->command = eSmeCommandRoam;
	command->sessionId = (tANI_U8)session_id;
	command->u.roamCmd.roamReason = eCsrForcedDisassoc;

	*sme_cmd = command;
	return eHAL_STATUS_SUCCESS;
}

eHalStatus csrRoamIssueDisassociateCmd( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;

    do
    {
        pCommand = csrGetCommandBuffer( pMac );
        if ( !pCommand )
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        //Change the substate in case it is wait-for-key
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
        {
            csrRoamStopWaitForKeyTimer( pMac );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        smsLog( pMac, LOG1, FL("Disassociate reason: %d, sessionId: %d"),
                                reason,sessionId);
        switch ( reason )
        {
        case eCSR_DISCONNECT_REASON_MIC_ERROR:
            pCommand->u.roamCmd.roamReason = eCsrForcedDisassocMICFailure;
            break;
        case eCSR_DISCONNECT_REASON_DEAUTH:
            pCommand->u.roamCmd.roamReason = eCsrForcedDeauth;
            break;
        case eCSR_DISCONNECT_REASON_HANDOFF:
            pCommand->u.roamCmd.roamReason = eCsrSmeIssuedDisassocForHandoff;
            break;
        case eCSR_DISCONNECT_REASON_UNSPECIFIED:
        case eCSR_DISCONNECT_REASON_DISASSOC:
            pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
            break;
        case eCSR_DISCONNECT_REASON_IBSS_JOIN_FAILURE:
            pCommand->u.roamCmd.roamReason = eCsrSmeIssuedIbssJoinFailure;
            break;
        case eCSR_DISCONNECT_REASON_IBSS_LEAVE:
            pCommand->u.roamCmd.roamReason = eCsrForcedIbssLeave;
            break;
        case eCSR_DISCONNECT_REASON_STA_HAS_LEFT:
            pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
            pCommand->u.roamCmd.reason = eSIR_MAC_DISASSOC_LEAVING_BSS_REASON;
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                FL("SME convert to internal reason code eCsrStaHasLeft"));
            break;
        default:
            break;
        }
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    } while( 0 );
    return( status );
}

eHalStatus csrRoamIssueStopBssCmd( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_BOOLEAN fHighPriority )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;
    pCommand = csrGetCommandBuffer( pMac );
    if ( NULL != pCommand )
    {
        //Change the substate in case it is wait-for-key
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId) )
        {
            csrRoamStopWaitForKeyTimer( pMac );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.roamReason = eCsrStopBss;
        status = csrQueueSmeCommand(pMac, pCommand, fHighPriority);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer") );
        status = eHAL_STATUS_RESOURCES;
    }
    return ( status );
}

eHalStatus csrRoamDisconnectInternal(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    //Not to call cancel roaming here
    //Only issue disconnect when necessary
    if(csrIsConnStateConnected(pMac, sessionId) || csrIsBssTypeIBSS(pSession->connectedProfile.BSSType)
                || csrIsBssTypeWDS(pSession->connectedProfile.BSSType)
                || csrIsRoamCommandWaitingForSession(pMac, sessionId) )

    {
        smsLog(pMac, LOG2, FL("called"));
        status = csrRoamIssueDisassociateCmd(pMac, sessionId, reason);
    }
    else
    {
        pMac->roam.roamSession[sessionId].connectState =
            eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING;
        csrScanAbortScanForSSID(pMac, sessionId);
        status = eHAL_STATUS_CMD_NOT_QUEUED;
        smsLog( pMac, LOGE,
            FL("Disconnect not queued, Abort Scan for SSID"));
    }
    return (status);
}

eHalStatus csrRoamDisconnect(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason)
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    csrRoamCancelRoaming(pMac, sessionId);
    csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrForcedDisassoc);

    return (csrRoamDisconnectInternal(pMac, sessionId, reason));
}

eHalStatus csrRoamSaveConnectedInfomation(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                          tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tDot11fBeaconIEs *pIesTemp = pIes;
    tANI_U8 index;
    tCsrRoamSession *pSession = NULL;
    tCsrRoamConnectedProfile *pConnectProfile = NULL;

    pSession = CSR_GET_SESSION(pMac, sessionId);
    if (NULL == pSession) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            FL("session %d not found"), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    pConnectProfile = &pSession->connectedProfile;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (pSession->roamOffloadSynchParams.bRoamSynchInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
            FL("csrRoamSaveConnectedInfomation"));
    }
#endif
    if(pConnectProfile->pAddIEAssoc)
    {
        vos_mem_free(pConnectProfile->pAddIEAssoc);
        pConnectProfile->pAddIEAssoc = NULL;
    }
    vos_mem_set(&pSession->connectedProfile, sizeof(tCsrRoamConnectedProfile), 0);
    pConnectProfile->AuthType = pProfile->negotiatedAuthType;
        pConnectProfile->AuthInfo = pProfile->AuthType;
    pConnectProfile->CBMode = pProfile->CBMode;  //*** this may not be valid
    pConnectProfile->EncryptionType = pProfile->negotiatedUCEncryptionType;
        pConnectProfile->EncryptionInfo = pProfile->EncryptionType;
    pConnectProfile->mcEncryptionType = pProfile->negotiatedMCEncryptionType;
        pConnectProfile->mcEncryptionInfo = pProfile->mcEncryptionType;
    pConnectProfile->BSSType = pProfile->BSSType;
    pConnectProfile->modifyProfileFields.uapsd_mask = pProfile->uapsd_mask;
    pConnectProfile->operationChannel = pSirBssDesc->channelId;
    pConnectProfile->beaconInterval = pSirBssDesc->beaconInterval;
    if (!pConnectProfile->beaconInterval)
    {
        smsLog(pMac, LOGW, FL("ERROR: Beacon interval is ZERO"));
    }
    vos_mem_copy(&pConnectProfile->Keys, &pProfile->Keys, sizeof(tCsrKeys));
    /* Saving the additional IE`s like Hot spot indication element and
       extended capabilities */
    if(pProfile->nAddIEAssocLength)
    {
        pConnectProfile->pAddIEAssoc = vos_mem_malloc(pProfile->nAddIEAssocLength);
        if ( NULL ==  pConnectProfile->pAddIEAssoc )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if (!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGE, FL("Failed to allocate memory for additional IEs")) ;
            return eHAL_STATUS_FAILURE;
        }
        pConnectProfile->nAddIEAssocLength = pProfile->nAddIEAssocLength;
        vos_mem_copy(pConnectProfile->pAddIEAssoc, pProfile->pAddIEAssoc,
                     pProfile->nAddIEAssocLength);
    }

#ifdef WLAN_FEATURE_11W
    pConnectProfile->MFPEnabled = pProfile->MFPEnabled;
    pConnectProfile->MFPRequired = pProfile->MFPRequired;
    pConnectProfile->MFPCapable = pProfile->MFPCapable;
#endif
    //Save bssid
    csrGetBssIdBssDesc(pMac, pSirBssDesc, &pConnectProfile->bssid);
#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pSirBssDesc->mdiePresent)
    {
        pConnectProfile->MDID.mdiePresent = 1;
        pConnectProfile->MDID.mobilityDomain = (pSirBssDesc->mdie[1] << 8) | (pSirBssDesc->mdie[0]);
    }
#endif
    if( NULL == pIesTemp )
    {
        status = csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesTemp);
    }
#ifdef FEATURE_WLAN_ESE
    if ((csrIsProfileESE(pProfile) ||
         (HAL_STATUS_SUCCESS(status) && (pIesTemp->ESEVersion.present)
          && (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM)))
        && (pMac->roam.configParam.isEseIniFeatureEnabled))
    {
        pConnectProfile->isESEAssoc = 1;
    }
#endif
    //save ssid
    if(HAL_STATUS_SUCCESS(status))
    {
        if(pIesTemp->SSID.present)
        {
            pConnectProfile->SSID.length = pIesTemp->SSID.num_ssid;
            vos_mem_copy(pConnectProfile->SSID.ssId, pIesTemp->SSID.ssid,
                         pIesTemp->SSID.num_ssid);
        }

        //Save the bss desc
        status = csrRoamSaveConnectedBssDesc(pMac, sessionId, pSirBssDesc);

        if( CSR_IS_QOS_BSS(pIesTemp) || pIesTemp->HTCaps.present)
        {
           //Some HT AP's dont send WMM IE so in that case we assume all HT Ap's are Qos Enabled AP's
           pConnectProfile->qap = TRUE;
        }
        else
        {
           pConnectProfile->qap = FALSE;
        }

        if (pIesTemp->ExtCap.present)
        {
            struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)
                                          pIesTemp->ExtCap.bytes;
            pConnectProfile->proxyARPService = p_ext_cap->proxyARPService;
        }

        if ( NULL == pIes )
        {
            //Free memory if it allocated locally
            vos_mem_free(pIesTemp);
        }
    }
    //Save Qos connection
    pConnectProfile->qosConnection = pMac->roam.roamSession[sessionId].fWMMConnection;

    if(!HAL_STATUS_SUCCESS(status))
    {
        csrFreeConnectBssDesc(pMac, sessionId);
    }
    for(index = 0; index < pProfile->SSIDs.numOfSSIDs; index++)
    {
        if ((pProfile->SSIDs.SSIDList[index].SSID.length == pConnectProfile->SSID.length) &&
            vos_mem_compare(pProfile->SSIDs.SSIDList[index].SSID.ssId,
                            pConnectProfile->SSID.ssId,
                            pConnectProfile->SSID.length))
       {
          pConnectProfile->handoffPermitted = pProfile->SSIDs.SSIDList[index].handoffPermitted;
          break;
       }
       pConnectProfile->handoffPermitted = FALSE;
    }

    return (status);
}


boolean is_disconnect_pending(tpAniSirGlobal pmac,
				uint8_t sessionid)
{
	tListElem *entry = NULL;
	tListElem *next_entry = NULL;
	tSmeCmd *command = NULL;
	bool disconnect_cmd_exist = false;

	csrLLLock(&pmac->sme.smeCmdPendingList);
	entry = csrLLPeekHead(&pmac->sme.smeCmdPendingList, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csrLLNext(&pmac->sme.smeCmdPendingList,
					entry, LL_ACCESS_NOLOCK);

		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if (command && CSR_IS_DISCONNECT_COMMAND(command) &&
				command->sessionId == sessionid){
			disconnect_cmd_exist = true;
			break;
		}
		entry = next_entry;
	}
	csrLLUnlock(&pmac->sme.smeCmdPendingList);
	return disconnect_cmd_exist;
}

static void csrRoamJoinRspProcessor( tpAniSirGlobal pMac, tSirSmeJoinRsp *pSmeJoinRsp )
{
   tListElem *pEntry = NULL;
   tSmeCmd *pCommand = NULL;
   tCsrRoamSession *pSession;

   if (pSmeJoinRsp)
     pSession = CSR_GET_SESSION(pMac, pSmeJoinRsp->sessionId);
   else {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                 FL("Sme Join Response is NULL"));
       return;
   }
   if (!pSession) {
       smsLog(pMac, LOGE, FL("session %d not found"), pSmeJoinRsp->sessionId);
       return;
   }
   //The head of the active list is the request we sent
   pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
   if(pEntry)
   {
       pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
   }
   if ( eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode )
   {
            if(pCommand && eCsrSmeIssuedAssocToSimilarAP == pCommand->u.roamCmd.roamReason)
            {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
               sme_QosCsrEventInd(pMac, pSmeJoinRsp->sessionId, SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
#endif
            }

            pSession->supported_nss_1x1 = pSmeJoinRsp->supported_nss_1x1;
            smsLog(pMac, LOG1, FL("SME session supported nss: %d"),
                   pSession->supported_nss_1x1);

            /* The join bssid count can be reset as soon as
             * we are done with the join requests and returning
             * the response to upper layers
             */
            pSession->join_bssid_count = 0;
            csrRoamComplete( pMac, eCsrJoinSuccess, (void *)pSmeJoinRsp );
   }
   else
   {
        tANI_U32 roamId = 0;
        bool is_dis_pending;
        //The head of the active list is the request we sent
        //Try to get back the same profile and roam again
        if(pCommand)
        {
            roamId = pCommand->u.roamCmd.roamId;
        }
        pSession->joinFailStatusCode.statusCode = pSmeJoinRsp->statusCode;
        pSession->joinFailStatusCode.reasonCode = pSmeJoinRsp->protStatusCode;
        smsLog( pMac, LOGW, "SmeJoinReq failed with statusCode= 0x%08X [%d]", pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode );
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
        /* If Join fails while Handoff is in progress, indicate disassociated event to supplicant to reconnect */
        if (csrRoamIsHandoffInProgress(pMac, pSmeJoinRsp->sessionId))
        {
            csrRoamCallCallback(pMac, pSmeJoinRsp->sessionId, NULL, roamId, eCSR_ROAM_DISASSOCIATED, eCSR_ROAM_RESULT_FORCED);
            /* Should indicate neighbor roam algorithm about the connect failure here */
            csrNeighborRoamIndicateConnect(pMac, pSmeJoinRsp->sessionId, VOS_STATUS_E_FAILURE);
        }
#endif
        /*
         * if userspace has issued disconnection,
         * driver should not continue connecting
         */
        is_dis_pending = is_disconnect_pending(pMac, pSession->sessionId);

        if (pCommand && (pSession->join_bssid_count < CSR_MAX_BSSID_COUNT) &&
            !is_dis_pending)
        {
            if(CSR_IS_WDS_STA( &pCommand->u.roamCmd.roamProfile ))
            {
              pCommand->u.roamCmd.fStopWds = eANI_BOOLEAN_TRUE;
              pSession->connectedProfile.BSSType = eCSR_BSS_TYPE_WDS_STA;
              csrRoamReissueRoamCommand(pMac);
            }
            else if( CSR_IS_WDS( &pCommand->u.roamCmd.roamProfile ) )
            {
                pSession->join_bssid_count = 0;
                csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
            }
            else
            {
                csrRoam(pMac, pCommand);
            }
        }
        else {
          /* When the upper layers issue a connect command, there is a
           * roam command with reason eCsrHddIssued that gets enqueued
           * and an associated timer for the SME command timeout is
           * started which is currently 120 seconds. This command would
           * be dequeued only upon succesfull connections. In case of join
           * failures, if there are too many BSS in the cache, and if we
           * fail Join requests with all of them, there is a chance of
           * timing out the above timer.
           */
          if (pSession->join_bssid_count >= CSR_MAX_BSSID_COUNT)
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               FL("Excessive Join Request Failures"));

          if (is_dis_pending)
            VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
               FL("disconnect is pending, complete roam"));
          pSession->join_bssid_count = 0;
          csrRoamComplete(pMac, eCsrNothingToJoin, NULL);
        }
    } /*else: ( eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode ) */
}

eHalStatus csrRoamIssueJoin( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pSirBssDesc,
                             tDot11fBeaconIEs *pIes,
                             tCsrRoamProfile *pProfile, tANI_U32 roamId )
{
    eHalStatus status;
    smsLog( pMac, LOG1, "Attempting to Join Bssid= "MAC_ADDRESS_STR,
                  MAC_ADDR_ARRAY(pSirBssDesc->bssId));

    // Set the roaming substate to 'join attempt'...
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
    // attempt to Join this BSS...
    status = csrSendJoinReqMsg( pMac, sessionId, pSirBssDesc, pProfile, pIes, eWNI_SME_JOIN_REQ );
    return (status);
}

static eHalStatus csrRoamIssueReassociate( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pSirBssDesc,
                              tDot11fBeaconIEs *pIes, tCsrRoamProfile *pProfile)
{
    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId);
    // Set the roaming substate to 'join attempt'...
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_REASSOC_REQ, sessionId );
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
               FL(" calling csrSendJoinReqMsg (eWNI_SME_REASSOC_REQ)"));
    // attempt to Join this BSS...
    return csrSendJoinReqMsg( pMac, sessionId, pSirBssDesc, pProfile, pIes, eWNI_SME_REASSOC_REQ);
}

void csrRoamReissueRoamCommand(tpAniSirGlobal pMac)
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    tCsrRoamInfo roamInfo;
    tANI_U32 sessionId;
    tCsrRoamSession *pSession;

    pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    if(pEntry)
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        if ( eSmeCommandRoam == pCommand->command )
        {
            sessionId = pCommand->sessionId;
            pSession = CSR_GET_SESSION( pMac, sessionId );

            if(!pSession)
            {
                smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                return;
            }
            /*
             * While switching between two AP, csr will reissue roam command
             * again to the nextbss if it was interrupted by the dissconnect
             * req for the previous bss. During this csr is incrementing
             * bRefAssocStartCnt twice. So reset the bRefAssocStartCnt.
             */
            if (pSession->bRefAssocStartCnt > 0) {
                pSession->bRefAssocStartCnt--;
            }
            if( pCommand->u.roamCmd.fStopWds )
            {
                vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
                roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                if (CSR_IS_WDS(&pSession->connectedProfile)){
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;
                csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                        eCSR_ROAM_WDS_IND,
                                        eCSR_ROAM_RESULT_WDS_DISASSOCIATED);
                                }else if (CSR_IS_INFRA_AP(&pSession->connectedProfile)){
                                        pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
                                        csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                                                                eCSR_ROAM_INFRA_IND,
                                                                                eCSR_ROAM_RESULT_INFRA_DISASSOCIATED);
                                }


                if( !HAL_STATUS_SUCCESS( csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ ) ) )
                {
                    smsLog(pMac, LOGE, " Failed to reissue stop_bss command for WDS after disassociated");
                    csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
                }
            }
            else
            {
                if (pSession->bRefAssocStartCnt > 0)
                {
                    /* bRefAssocStartCnt was incremented in csrRoamJoinNextBss
                     * when the roam command issued previously. As part of reissuing
                     * the roam command again csrRoamJoinNextBss is going increment
                     * RefAssocStartCnt. So make sure to decrement the bRefAssocStartCnt
                     */
                    pSession->bRefAssocStartCnt--;
                }

                if(eCsrStopRoaming == csrRoamJoinNextBss(pMac, pCommand, eANI_BOOLEAN_TRUE))
                {
                    smsLog(pMac, LOGW, " Failed to reissue join command after disassociated");
                    csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
                }
            }
        }
        else
        {
            smsLog(pMac, LOGW, "  Command is not roaming after disassociated");
        }
    }
    else
    {
        smsLog(pMac, LOGE, "   Disassoc rsp cannot continue because no command is available");
    }
}

tANI_BOOLEAN csrIsRoamCommandWaitingForSession(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;
    tListElem *pEntry;
    tSmeCmd *pCommand = NULL;
    /* Always lock active list before locking pending list */
    csrLLLock( &pMac->sme.smeCmdActiveList );
    pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK);
    if(pEntry)
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        if( ( eSmeCommandRoam == pCommand->command ) && ( sessionId == pCommand->sessionId ) )
        {
            fRet = eANI_BOOLEAN_TRUE;
        }
    }
    if(eANI_BOOLEAN_FALSE == fRet)
    {
        csrLLLock(&pMac->sme.smeCmdPendingList);
        pEntry = csrLLPeekHead(&pMac->sme.smeCmdPendingList, LL_ACCESS_NOLOCK);
        while(pEntry)
        {
            pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
            if( ( eSmeCommandRoam == pCommand->command ) && ( sessionId == pCommand->sessionId ) )
            {
                fRet = eANI_BOOLEAN_TRUE;
                break;
            }
            pEntry = csrLLNext(&pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_NOLOCK);
        }
        csrLLUnlock(&pMac->sme.smeCmdPendingList);
    }
    if (eANI_BOOLEAN_FALSE == fRet)
    {
        csrLLLock(&pMac->roam.roamCmdPendingList);
        pEntry = csrLLPeekHead(&pMac->roam.roamCmdPendingList, LL_ACCESS_NOLOCK);
        while (pEntry)
        {
            pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
            if (( eSmeCommandRoam == pCommand->command ) && ( sessionId == pCommand->sessionId ) )
            {
                fRet = eANI_BOOLEAN_TRUE;
                break;
            }
            pEntry = csrLLNext(&pMac->roam.roamCmdPendingList, pEntry, LL_ACCESS_NOLOCK);
        }
        csrLLUnlock(&pMac->roam.roamCmdPendingList);
    }
    csrLLUnlock( &pMac->sme.smeCmdActiveList );
    return (fRet);
}

tANI_BOOLEAN csrIsRoamCommandWaiting(tpAniSirGlobal pMac)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;
    tANI_U32 i;
    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && ( fRet = csrIsRoamCommandWaitingForSession( pMac, i ) ) )
        {
            break;
        }
    }
    return ( fRet );
}

tANI_BOOLEAN csrIsCommandWaiting(tpAniSirGlobal pMac)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;
    /* Always lock active list before locking pending list */
    csrLLLock( &pMac->sme.smeCmdActiveList );
    fRet = csrLLIsListEmpty(&pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK);
    if(eANI_BOOLEAN_FALSE == fRet)
    {
        fRet = csrLLIsListEmpty(&pMac->sme.smeCmdPendingList, LL_ACCESS_LOCK);
    }
    csrLLUnlock( &pMac->sme.smeCmdActiveList );
    return (fRet);
}

tANI_BOOLEAN csrIsScanForRoamCommandActive( tpAniSirGlobal pMac )
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;
    tListElem *pEntry;
    tCsrCmd *pCommand;
    /* Always lock active list before locking pending list */
    csrLLLock( &pMac->sme.smeCmdActiveList );
    pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_NOLOCK);
    if( pEntry )
    {
        pCommand = GET_BASE_ADDR(pEntry, tCsrCmd, Link);
        if( ( eCsrRoamCommandScan == pCommand->command ) &&
            ( ( eCsrScanForSsid == pCommand->u.scanCmd.reason ) ||
              ( eCsrScanForCapsChange == pCommand->u.scanCmd.reason ) ||
              ( eCsrScanP2PFindPeer == pCommand->u.scanCmd.reason ) ) )
        {
            fRet = eANI_BOOLEAN_TRUE;
        }
    }
    csrLLUnlock( &pMac->sme.smeCmdActiveList );
    return (fRet);
}
eHalStatus csrRoamIssueReassociateCmd( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand = NULL;
    tANI_BOOLEAN fHighPriority = eANI_BOOLEAN_TRUE;
    tANI_BOOLEAN fRemoveCmd = FALSE;
    tListElem *pEntry;
    // Delete the old assoc command. All is setup for reassoc to be serialized
    pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
    if ( pEntry )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        if ( !pCommand )
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            return eHAL_STATUS_RESOURCES;
        }
        if ( eSmeCommandRoam == pCommand->command )
        {
            if (pCommand->u.roamCmd.roamReason == eCsrSmeIssuedAssocToSimilarAP)
            {
                fRemoveCmd = csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK );
            }
            else
            {
                smsLog( pMac, LOGE, FL(" Unexpected active roam command present ") );
            }
            if (fRemoveCmd == FALSE)
            {
                // Implies we did not get the serialized assoc command we
                // were expecting
                pCommand = NULL;
            }
        }
    }
    if(NULL == pCommand)
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer as expected based on previous connect roam command") );
        return eHAL_STATUS_RESOURCES;
    }
    do
    {
        //Change the substate in case it is wait-for-key
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
        {
            csrRoamStopWaitForKeyTimer( pMac );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
        }
        pCommand->command = eSmeCommandRoam;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.roamCmd.roamReason = eCsrSmeIssuedFTReassoc;
        status = csrQueueSmeCommand(pMac, pCommand, fHighPriority);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    } while( 0 );

    return( status );
}
static void csrRoamingStateConfigCnfProcessor( tpAniSirGlobal pMac, tANI_U32 result )
{
    tListElem *pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    tCsrScanResult *pScanResult = NULL;
    tSirBssDescription *pBssDesc = NULL;
    tSmeCmd *pCommand = NULL;
    tANI_U32 sessionId;
    tCsrRoamSession *pSession;
    if(NULL == pEntry)
    {
        smsLog(pMac, LOGE, "   CFG_CNF with active list empty");
        return;
    }
    pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
    sessionId = pCommand->sessionId;
    pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if(CSR_IS_ROAMING(pSession) && pSession->fCancelRoaming)
    {
        /* The roaming is canceled. Simply complete the command */
        smsLog(pMac, LOGW, FL("  Roam command canceled"));
        csrRoamComplete(pMac, eCsrNothingToJoin, NULL);
    }
    /* If the roaming has stopped, not to continue the roaming command*/
    else if ( !CSR_IS_ROAMING(pSession) && CSR_IS_ROAMING_COMMAND(pCommand) )
    {
        //No need to complete roaming here as it already completes
        smsLog(pMac, LOGW, FL("  Roam command (reason %d) aborted due to roaming completed\n"),
           pCommand->u.roamCmd.roamReason);
        csrSetAbortRoamingCommand( pMac, pCommand );
        csrRoamComplete(pMac, eCsrNothingToJoin, NULL);
    }
    else
    {
        if ( CCM_IS_RESULT_SUCCESS(result) )
        {
            smsLog(pMac, LOG1, "Cfg sequence complete");
            // Successfully set the configuration parameters for the new Bss.  Attempt to
            // join the roaming Bss.
            if(pCommand->u.roamCmd.pRoamBssEntry)
            {
                pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
                pBssDesc = &pScanResult->Result.BssDescriptor;
            }
            smsLog(pMac, LOG1, "BSSType = %d",
                          pCommand->u.roamCmd.roamProfile.BSSType);
            if ( csrIsBssTypeIBSS( pCommand->u.roamCmd.roamProfile.BSSType ) ||
                 CSR_IS_WDS( &pCommand->u.roamCmd.roamProfile )
                  || CSR_IS_INFRA_AP(&pCommand->u.roamCmd.roamProfile)
            )
            {
                if(!HAL_STATUS_SUCCESS(csrRoamIssueStartBss( pMac, sessionId,
                                        &pSession->bssParams, &pCommand->u.roamCmd.roamProfile,
                                        pBssDesc, pCommand->u.roamCmd.roamId )))
                {
                    smsLog(pMac, LOGE, " CSR start BSS failed");
                    //We need to complete the command
                    csrRoamComplete(pMac, eCsrStartBssFailure, NULL);
                }
            }
            else
            {
                if (!pCommand->u.roamCmd.pRoamBssEntry)
                {
                    smsLog(pMac, LOGE, " pRoamBssEntry is NULL");
                    //We need to complete the command
                    csrRoamComplete(pMac, eCsrJoinFailure, NULL);
                    return;
                }
                if ( NULL == pScanResult)
                {
                   // If we are roaming TO an Infrastructure BSS...
                   VOS_ASSERT(pScanResult != NULL);
                   return;
                }
                if ( csrIsInfraBssDesc( pBssDesc ) )
                {
                    tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *)pScanResult->Result.pvIes;
                    if(pIesLocal || (HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIesLocal))) )
                    {
                    // ..and currently in an Infrastructure connection....
                    if( csrIsConnStateConnectedInfra( pMac, sessionId ) )
                    {
                        // ...and the SSIDs are equal, then we Reassoc.
                        if (  csrIsSsidEqual( pMac, pSession->pConnectBssDesc, pBssDesc,
                                                    pIesLocal ) )
                        // ..and currently in an infrastructure connection
                        {
                            // then issue a Reassoc.
                            pCommand->u.roamCmd.fReassoc = eANI_BOOLEAN_TRUE;
                                csrRoamIssueReassociate( pMac, sessionId, pBssDesc, pIesLocal,
                                                        &pCommand->u.roamCmd.roamProfile );
                        }
                        else
                        {

                            // otherwise, we have to issue a new Join request to LIM because we disassociated from the
                            // previously associated AP.
                            if(!HAL_STATUS_SUCCESS(csrRoamIssueJoin( pMac, sessionId, pBssDesc,
                                                                                                            pIesLocal,
                                                    &pCommand->u.roamCmd.roamProfile, pCommand->u.roamCmd.roamId )))
                            {
                                //try something else
                                csrRoam( pMac, pCommand );
                            }
                        }
                    }
                    else
                    {
                        eHalStatus  status = eHAL_STATUS_SUCCESS;

                        /*
                         * We need to come with other way to figure out that
                         * this is because of HO in BMP. The below API will be
                         * only available for Android as it uses a different
                         * HO algorithm.
                         * Reassoc request will be used only for ESE and 11r
                         * handoff whereas other legacy roaming should
                         * use join request */
#ifdef WLAN_FEATURE_VOWIFI_11R
                        if (csrRoamIsHandoffInProgress(pMac, sessionId) &&
                                           csrRoamIs11rAssoc(pMac, sessionId))
                        {
                            status = csrRoamIssueReassociate(pMac, sessionId, pBssDesc,
                                    (tDot11fBeaconIEs *)( pScanResult->Result.pvIes ), &pCommand->u.roamCmd.roamProfile);
                        }
                        else
#endif
#ifdef FEATURE_WLAN_ESE
                        if (csrRoamIsHandoffInProgress(pMac, sessionId) &&
                                             csrRoamIsESEAssoc(pMac, sessionId))
                        {
                            // Now serialize the reassoc command.
                            status = csrRoamIssueReassociateCmd(pMac, sessionId);
                        }
                        else
#endif
#ifdef FEATURE_WLAN_LFR
                        if (csrRoamIsHandoffInProgress(pMac, sessionId) &&
                                      csrRoamIsFastRoamEnabled(pMac, sessionId))
                        {
                            // Now serialize the reassoc command.
                            status = csrRoamIssueReassociateCmd(pMac, sessionId);
                        }
                        else
#endif
                        // else we are not connected and attempting to Join.  Issue the
                        // Join request.
                        {
                            status = csrRoamIssueJoin( pMac, sessionId, pBssDesc,
                                                (tDot11fBeaconIEs *)( pScanResult->Result.pvIes ),
                                                &pCommand->u.roamCmd.roamProfile, pCommand->u.roamCmd.roamId );
                        }
                        if(!HAL_STATUS_SUCCESS(status))
                        {
                            //try something else
                            csrRoam( pMac, pCommand );
                        }
                    }
                        if( !pScanResult->Result.pvIes )
                        {
                            //Locally allocated
                           vos_mem_free(pIesLocal);
                        }
                    }
                }//if ( csrIsInfraBssDesc( pBssDesc ) )
                else
                {
                    smsLog(pMac, LOGW, FL("  found BSSType mismatching the one in BSS description"));
                }
            }//else
        }//if ( WNI_CFG_SUCCESS == result )
        else
        {
            smsLog(pMac, LOG1,
                          FL("!CCM_IS_RESULT_SUCCESS  result = %d"), result);
            // In the event the configuration failed,  for infra let the roam processor
            //attempt to join something else...
            if( pCommand->u.roamCmd.pRoamBssEntry && CSR_IS_INFRASTRUCTURE( &pCommand->u.roamCmd.roamProfile ) )
            {
            csrRoam(pMac, pCommand);
            }
            else
            {
                //We need to complete the command
                if ( csrIsBssTypeIBSS( pCommand->u.roamCmd.roamProfile.BSSType ) )
                {
                    csrRoamComplete(pMac, eCsrStartBssFailure, NULL);
                }
                else
                {
                    csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
                }
            }
        }
    }//we have active entry
}

static void csrRoamRoamingStateReassocRspProcessor( tpAniSirGlobal pMac, tpSirSmeJoinRsp pSmeJoinRsp )
{
    eCsrRoamCompleteResult result;
    tpCsrNeighborRoamControlInfo  pNeighborRoamInfo =
                           &pMac->roam.neighborRoamInfo[pSmeJoinRsp->sessionId];
    tCsrRoamInfo roamInfo;
    tANI_U32 roamId = 0;

    if ( eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
        FL("CSR SmeReassocReq Successful"));
        result = eCsrReassocSuccess;
        /* Defeaturize this part later if needed */
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        /*
         * Since the neighbor roam algorithm uses reassoc req for
         * handoff instead of join, we need the response contents while
         * processing the result in csrRoamProcessResults()
         */
        if (csrRoamIsHandoffInProgress(pMac, pSmeJoinRsp->sessionId))
        {
            /* Need to dig more on indicating events to SME QoS module */
            sme_QosCsrEventInd(pMac, pSmeJoinRsp->sessionId, SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
            csrRoamComplete( pMac, result, pSmeJoinRsp);
        }
        else
#endif
        {
            csrRoamComplete( pMac, result, NULL );
        }
    }
    /* Should we handle this similar to handling the join failure? Is it ok
     * to call csrRoamComplete() with state as CsrJoinFailure */
    else
    {
        smsLog( pMac, LOGW, "CSR SmeReassocReq failed with statusCode= 0x%08X [%d]", pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode );
        result = eCsrReassocFailure;
#if defined(WLAN_FEATURE_VOWIFI_11R) || defined(FEATURE_WLAN_ESE) || \
    defined(FEATURE_WLAN_LFR)
        if ((eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE == pSmeJoinRsp->statusCode) ||
            (eSIR_SME_FT_REASSOC_FAILURE == pSmeJoinRsp->statusCode) ||
            (eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA == pSmeJoinRsp->statusCode) ||
            (eSIR_SME_INVALID_PARAMETERS == pSmeJoinRsp->statusCode)) {
                /* Inform HDD to turn off FT flag in HDD */
                if (pNeighborRoamInfo) {
                    vos_mem_zero(&roamInfo, sizeof(tCsrRoamInfo));
                    csrRoamCallCallback(pMac, pSmeJoinRsp->sessionId, &roamInfo,
                                        roamId, eCSR_ROAM_FT_REASSOC_FAILED,
                                        eSIR_SME_SUCCESS);
                    /*
                     * Since the above callback sends a disconnect
                     * to HDD, we should clean-up our state
                     * machine as well to be in sync with the upper
                     * layers. There is no need to send a disassoc
                     * since: 1) we will never reassoc to the current
                     * AP in LFR, and 2) there is no need to issue a
                     * disassoc to the AP with which we were trying
                     * to reassoc.
                     */
                    csrRoamComplete(pMac, eCsrJoinFailure, NULL);
                    return;
                }
        }
#endif
        // In the event that the Reassociation fails, then we need to Disassociate the current association and keep
        // roaming.  Note that we will attempt to Join the AP instead of a Reassoc since we may have attempted a
        // 'Reassoc to self', which AP's that don't support Reassoc will force a Disassoc.
        //The disassoc rsp message will remove the command from active list
        if(!HAL_STATUS_SUCCESS(csrRoamIssueDisassociate( pMac, pSmeJoinRsp->sessionId,
                        eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, FALSE )))
        {
            csrRoamComplete( pMac, eCsrJoinFailure, NULL );
        }
    }
}

static void csrRoamRoamingStateStopBssRspProcessor(tpAniSirGlobal pMac, tSirSmeRsp *pSmeRsp)
{
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    {
        vos_log_ibss_pkt_type *pIbssLog;
        WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
        if(pIbssLog)
        {
            pIbssLog->eventId = WLAN_IBSS_EVENT_STOP_RSP;
            if(eSIR_SME_SUCCESS != pSmeRsp->statusCode)
            {
                pIbssLog->status = WLAN_IBSS_STATUS_FAILURE;
            }
            WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
        }
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    pMac->roam.roamSession[pSmeRsp->sessionId].connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
    if(CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ( pMac, pSmeRsp->sessionId))
    {
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else if(CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE( pMac, pSmeRsp->sessionId))
    {
        csrRoamReissueRoamCommand(pMac);
    }
}

void csrRoamRoamingStateDisassocRspProcessor( tpAniSirGlobal pMac, tSirSmeDisassocRsp *pSmeRsp )
{
    tSirResultCodes statusCode;
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
    tScanResultHandle hBSSList;
    tANI_BOOLEAN fCallCallback, fRemoveCmd;
    eHalStatus status;
    tCsrRoamInfo roamInfo;
    tCsrScanResultFilter *pScanFilter = NULL;
    tANI_U32 roamId = 0;
    tCsrRoamProfile *pCurRoamProfile = NULL;
    tListElem *pEntry = NULL;
    tSmeCmd *pCommand = NULL;
#endif
    tANI_U32 sessionId;
    tCsrRoamSession *pSession;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;

    tSirSmeDisassocRsp SmeDisassocRsp;

    csrSerDesUnpackDiassocRsp((tANI_U8 *)pSmeRsp, &SmeDisassocRsp);
    sessionId = SmeDisassocRsp.sessionId;
    statusCode = SmeDisassocRsp.statusCode;
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
    FL("csrRoamRoamingStateDisassocRspProcessor sessionId %d"), sessionId);

    if ( csrIsConnStateInfra( pMac, sessionId ) )
    {
        pMac->roam.roamSession[sessionId].connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
    }
    pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN( pMac, sessionId ) )
    {
        smsLog( pMac, LOG2, "***eCsrNothingToJoin***");
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED( pMac, sessionId ) ||
              CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ( pMac, sessionId ) )
    {
        if ( eSIR_SME_SUCCESS == statusCode )
        {
            smsLog( pMac, LOG2, "CSR SmeDisassocReq force disassociated Successfully" );
            //A callback to HDD will be issued from csrRoamComplete so no need to do anything here
        }
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( pMac, sessionId ) )
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                     "CSR SmeDisassocReq due to HO on session %d", sessionId );
        pNeighborRoamInfo = &pMac->roam.neighborRoamInfo[sessionId];
#if   defined (WLAN_FEATURE_NEIGHBOR_ROAMING)
      /*
        * First ensure if the roam profile is in the scan cache.
        * If not, post a reassoc failure and disconnect.
        */
       pScanFilter = vos_mem_malloc(sizeof(tCsrScanResultFilter));
       if ( NULL == pScanFilter )
           status = eHAL_STATUS_FAILURE;
       else
           status = eHAL_STATUS_SUCCESS;
       if(HAL_STATUS_SUCCESS(status))
       {
           vos_mem_set(pScanFilter, sizeof(tCsrScanResultFilter), 0);
	   pScanFilter->scan_filter_for_roam = 1;
           status = csrRoamPrepareFilterFromProfile(pMac,
                                     &pNeighborRoamInfo->csrNeighborRoamProfile,
                                     pScanFilter);
           if(!HAL_STATUS_SUCCESS(status))
           {
               smsLog(pMac, LOGE, "%s: failed to prepare scan filter with status %d",
                       __func__, status);
               goto POST_ROAM_FAILURE;
           }
           else
           {
               status = csrScanGetResult(pMac, pScanFilter, &hBSSList);
               if (!HAL_STATUS_SUCCESS(status))
               {
                   smsLog( pMac, LOGE,"%s: csrScanGetResult failed with status %d",
                           __func__, status);
                   goto POST_ROAM_FAILURE;
               }
           }
       }
       else
       {
           smsLog( pMac, LOGE,"%s: alloc for pScanFilter failed with status %d",
                   __func__, status);
           goto POST_ROAM_FAILURE;
       }

       /*
        * After ensuring that the roam profile is in the scan result list,
        * dequeue the command from the active list.
        */
        pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
        if ( pEntry )
        {
            pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
            /* If the head of the queue is Active and it is a ROAM command, remove
             * and put this on the Free queue.
             */
            if ( eSmeCommandRoam == pCommand->command )
            {

                /*
                 * we need to process the result first before removing it from active list
                 * because state changes still happening insides roamQProcessRoamResults so
                 * no other roam command should be issued.
                 */
                fRemoveCmd = csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK );
                if(pCommand->u.roamCmd.fReleaseProfile)
                {
                    csrReleaseProfile(pMac, &pCommand->u.roamCmd.roamProfile);
                    pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_FALSE;
                }
                if( fRemoveCmd )
                    csrReleaseCommandRoam( pMac, pCommand );
                else
                {
                    smsLog( pMac, LOGE, "%s: fail to remove cmd reason %d",
                            __func__, pCommand->u.roamCmd.roamReason );
                }
            }
            else
            {
                smsLog( pMac, LOGE, "%s: roam command not active", __func__ );
            }
        }
        else
        {
            smsLog( pMac, LOGE, "%s: NO commands are active", __func__ );
        }

        /* Notify HDD about handoff and provide the BSSID too */
        roamInfo.reasonCode = eCsrRoamReasonBetterAP;

        vos_mem_copy(roamInfo.bssid,
                     pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.bssid,
                     sizeof(tSirMacAddr));

        csrRoamCallCallback(pMac,sessionId, &roamInfo, 0,
            eCSR_ROAM_ROAMING_START, eCSR_ROAM_RESULT_NONE);

        /* Copy the connected profile to apply the same for this connection as well */
        pCurRoamProfile = vos_mem_malloc(sizeof(tCsrRoamProfile));
        if ( pCurRoamProfile != NULL )
        {
            vos_mem_set(pCurRoamProfile, sizeof(tCsrRoamProfile), 0);
            csrRoamCopyProfile(pMac, pCurRoamProfile, pSession->pCurRoamProfile);
            //make sure to put it at the head of the cmd queue
            status = csrRoamIssueConnect(pMac, sessionId, pCurRoamProfile,
                    hBSSList, eCsrSmeIssuedAssocToSimilarAP,
                    roamId, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_FALSE);

            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog( pMac, LOGE,"%s: csrRoamIssueConnect failed with status %d",
                        __func__, status);
                fCallCallback = eANI_BOOLEAN_TRUE;
            }

            /* Notify sub-modules like QoS etc. that handoff happening */
            sme_QosCsrEventInd(pMac, sessionId, SME_QOS_CSR_HANDOFF_ASSOC_REQ, NULL);
            csrReleaseProfile(pMac, pCurRoamProfile);
            vos_mem_free(pCurRoamProfile);
            csrFreeScanFilter(pMac, pScanFilter);
            vos_mem_free(pScanFilter);
            return;
        }

POST_ROAM_FAILURE:
        if (pScanFilter)
        {
            csrFreeScanFilter(pMac, pScanFilter);
            vos_mem_free(pScanFilter);
        }
        if (pCurRoamProfile)
            vos_mem_free(pCurRoamProfile);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        csrRoamSynchCleanUp(pMac, sessionId);
#endif
        /* Inform the upper layers that the reassoc failed */
        vos_mem_zero(&roamInfo, sizeof(tCsrRoamInfo));
        csrRoamCallCallback(pMac, sessionId,
                &roamInfo, 0, eCSR_ROAM_FT_REASSOC_FAILED, eSIR_SME_SUCCESS);

        /*
         * Issue a disassoc request so that PE/LIM uses this to clean-up the FT session.
         * Upon success, we would re-enter this routine after receiving the disassoc
         * response and will fall into the reassoc fail sub-state. And, eventually
         * call csrRoamComplete which would remove the roam command from SME active
         * queue.
         */
        if (!HAL_STATUS_SUCCESS(csrRoamIssueDisassociate(pMac, sessionId,
            eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, FALSE)))
        {
            smsLog( pMac, LOGE,"%s: csrRoamIssueDisassociate failed with status %d",
                    __func__, status);
            csrRoamComplete( pMac, eCsrJoinFailure, NULL );
        }
#endif

    } //else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( pMac ) )
    else if ( CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL( pMac, sessionId ) )
    {
        // Disassoc due to Reassoc failure falls into this codepath....
        csrRoamComplete( pMac, eCsrJoinFailure, NULL );
    }
    else
    {
        if ( eSIR_SME_SUCCESS == statusCode )
        {
            // Successfully disassociated from the 'old' Bss...
            //
            /*
             * We get Disassociate response in three conditions.
             * - First is the case where we are disassociating from an
             *   Infra Bss to start an IBSS.
             * - Second is the when we are disassociating from an Infra Bss
             *   to join an IBSS or a new Infrastructure network.
             * - Third is where we are doing an Infra to Infra roam between
             *   networks with different SSIDs. In all cases, we set the new
             *   Bss configuration here and attempt to join
             */

            smsLog( pMac, LOG2, "CSR SmeDisassocReq disassociated Successfully" );
        }
        else
        {
            smsLog( pMac, LOGE, "SmeDisassocReq failed with statusCode= 0x%08X", statusCode );
        }
        //We are not done yet. Get the data and continue roaming
        csrRoamReissueRoamCommand(pMac);
    }
}

static void csrRoamRoamingStateDeauthRspProcessor( tpAniSirGlobal pMac, tSirSmeDeauthRsp *pSmeRsp )
{
    tSirResultCodes statusCode;
    //No one is sending eWNI_SME_DEAUTH_REQ to PE.
    smsLog(pMac, LOGW, FL("is no-op"));
    statusCode = csrGetDeAuthRspStatusCode( pSmeRsp );
    pMac->roam.deauthRspStatus = statusCode;
    if ( CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ( pMac, pSmeRsp->sessionId) )
    {
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else
    {
        if ( eSIR_SME_SUCCESS == statusCode )
        {
            // Successfully deauth from the 'old' Bss...
            //
            smsLog( pMac, LOG2, "CSR SmeDeauthReq disassociated Successfully" );
        }
        else
        {
            smsLog( pMac, LOGW, "SmeDeauthReq failed with statusCode= 0x%08X", statusCode );
        }
        //We are not done yet. Get the data and continue roaming
        csrRoamReissueRoamCommand(pMac);
    }
}

static void csrRoamRoamingStateStartBssRspProcessor( tpAniSirGlobal pMac, tSirSmeStartBssRsp *pSmeStartBssRsp )
{
    eCsrRoamCompleteResult result;

    if ( eSIR_SME_SUCCESS == pSmeStartBssRsp->statusCode )
    {
        smsLog( pMac, LOGW, "SmeStartBssReq Successful" );
        result = eCsrStartBssSuccess;
    }
    else
    {
        smsLog( pMac, LOGW, "SmeStartBssReq failed with statusCode= 0x%08X", pSmeStartBssRsp->statusCode );
        //Let csrRoamComplete decide what to do
        result = eCsrStartBssFailure;
    }
    csrRoamComplete( pMac, result, pSmeStartBssRsp);
}

/*
 * We need to be careful on whether to cast pMsgBuf (pSmeRsp) to other type of
 * structures. It depends on how the message is constructed. If the message is
 * sent by limSendSmeRsp, the pMsgBuf is only a generic response and can only be
 * used as pointer to tSirSmeRsp. For the messages where sender allocates memory
 * for specific structures, then it can be cast accordingly.
 */
void csrRoamingStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf )
{
    tSirSmeRsp *pSmeRsp;
    tSmeIbssPeerInd *pIbssPeerInd;
    tCsrRoamInfo roamInfo;
        // TODO Session Id need to be acquired in this function
        tANI_U32 sessionId = 0;
    pSmeRsp = (tSirSmeRsp *)pMsgBuf;
    smsLog(pMac, LOG2, FL("Message %d[0x%04X] received in substate %s"),
                          pSmeRsp->messageType, pSmeRsp->messageType,
                          macTraceGetcsrRoamSubState(
                          pMac->roam.curSubState[pSmeRsp->sessionId]));
    pSmeRsp->messageType = (pSmeRsp->messageType);
    pSmeRsp->length = (pSmeRsp->length);
    pSmeRsp->statusCode = (pSmeRsp->statusCode);
    switch (pSmeRsp->messageType)
    {

        case eWNI_SME_JOIN_RSP:      // in Roaming state, process the Join response message...
            if (CSR_IS_ROAM_SUBSTATE_JOIN_REQ(pMac, pSmeRsp->sessionId))
            {
                //We sent a JOIN_REQ
                csrRoamJoinRspProcessor( pMac, (tSirSmeJoinRsp *)pSmeRsp );
            }
            break;

        case eWNI_SME_REASSOC_RSP:     // or the Reassociation response message...
            if (CSR_IS_ROAM_SUBSTATE_REASSOC_REQ( pMac, pSmeRsp->sessionId) )
            {
                csrRoamRoamingStateReassocRspProcessor( pMac, (tpSirSmeJoinRsp )pSmeRsp );
            }
            break;

        case eWNI_SME_STOP_BSS_RSP:    // or the Stop Bss response message...
            {
                csrRoamRoamingStateStopBssRspProcessor(pMac, pSmeRsp);
            }
            break;

        case eWNI_SME_DISASSOC_RSP:    // or the Disassociate response message...
            if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ( pMac, pSmeRsp->sessionId )      ||
                 CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN( pMac, pSmeRsp->sessionId )  ||
                 CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL( pMac, pSmeRsp->sessionId )      ||
                 CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED( pMac, pSmeRsp->sessionId )   ||
                 CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE( pMac, pSmeRsp->sessionId ) ||
//HO
                 CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( pMac, pSmeRsp->sessionId )         )
            {
                 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                 FL("eWNI_SME_DISASSOC_RSP subState = %s"),
                     macTraceGetcsrRoamSubState(
                     pMac->roam.curSubState[pSmeRsp->sessionId]));
                csrRoamRoamingStateDisassocRspProcessor( pMac, (tSirSmeDisassocRsp *)pSmeRsp );
            }
            break;

        case eWNI_SME_DEAUTH_RSP:    // or the Deauthentication response message...
            if ( CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ( pMac, pSmeRsp->sessionId ) )
            {
                csrRoamRoamingStateDeauthRspProcessor( pMac, (tSirSmeDeauthRsp *)pSmeRsp );
            }
            break;

        case eWNI_SME_START_BSS_RSP:      // or the Start BSS response message...
            if (CSR_IS_ROAM_SUBSTATE_START_BSS_REQ( pMac, pSmeRsp->sessionId ) )
            {
                csrRoamRoamingStateStartBssRspProcessor( pMac, (tSirSmeStartBssRsp *)pSmeRsp );
            }
            break;

        case WNI_CFG_SET_CNF:    // process the Config Confirm messages when we are in 'Config' substate...
            if ( CSR_IS_ROAM_SUBSTATE_CONFIG( pMac, pSmeRsp->sessionId ) )
            {
                csrRoamingStateConfigCnfProcessor( pMac, ((tCsrCfgSetRsp *)pSmeRsp)->respStatus );
            }
            break;
        /* In case CSR issues STOP_BSS, we need to tell HDD about peer departed
           because PE is removing them */
        case eWNI_SME_IBSS_PEER_DEPARTED_IND:
            pIbssPeerInd = (tSmeIbssPeerInd*)pSmeRsp;
            smsLog(pMac, LOGE, "CSR: Peer departed notification from LIM in joining state");
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
            roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
            roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
            vos_mem_copy(&roamInfo.peerMac, pIbssPeerInd->peerAddr,
                         sizeof(tCsrBssid));
            csrRoamCallCallback(pMac, sessionId, &roamInfo, 0,
                                eCSR_ROAM_CONNECT_STATUS_UPDATE,
                                eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED);
            break;
        case eWNI_SME_GET_RSSI_REQ:
            {
                tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq*)pMsgBuf;
                if (NULL != pGetRssiReq->rssiCallback)
                {
                    ((tCsrRssiCallback)(pGetRssiReq->rssiCallback))( pGetRssiReq->lastRSSI,
                                                                     pGetRssiReq->staId,
                                                                     pGetRssiReq->pDevContext);
                }
                else
                {
                    smsLog(pMac, LOGE, FL("pGetRssiReq->rssiCallback is NULL"));
                }
            }
            break;

        default:
            smsLog(pMac, LOG1,
                   FL("Unexpected message type = %d[0x%X] received in substate %s"),
                   pSmeRsp->messageType, pSmeRsp->messageType,
                   macTraceGetcsrRoamSubState(
                   pMac->roam.curSubState[pSmeRsp->sessionId]));
            //If we are connected, check the link status change
                        if(!csrIsConnStateDisconnected(pMac, sessionId))
                        {
                                csrRoamCheckForLinkStatusChange( pMac, pSmeRsp );
                        }
            break;
    }
}

void csrRoamJoinedStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf )
{
    tSirSmeRsp *pSirMsg = (tSirSmeRsp *)pMsgBuf;
    switch (pSirMsg->messageType)
    {
       case eWNI_SME_GET_STATISTICS_RSP:
          smsLog( pMac, LOG2, FL("Stats rsp from PE"));
          csrRoamStatsRspProcessor( pMac, pSirMsg );
          break;
        case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
        {
            tCsrRoamSession  *pSession;
            tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
            tCsrRoamInfo roamInfo;
            tCsrRoamInfo *pRoamInfo = NULL;
            tANI_U32 sessionId;
            eHalStatus status;
            smsLog( pMac, LOG1, FL("ASSOCIATION confirmation can be given to upper layer "));
            vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
            pRoamInfo = &roamInfo;
            pUpperLayerAssocCnf = (tSirSmeAssocIndToUpperLayerCnf *)pMsgBuf;
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pUpperLayerAssocCnf->bssId, &sessionId );
            pSession = CSR_GET_SESSION(pMac, sessionId);

            if(!pSession)
            {
                smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                return;
            }

            pRoamInfo->statusCode = eSIR_SME_SUCCESS; //send the status code as Success
            pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
            pRoamInfo->staId = (tANI_U8)pUpperLayerAssocCnf->aid;
            pRoamInfo->rsnIELen = (tANI_U8)pUpperLayerAssocCnf->rsnIE.length;
            pRoamInfo->prsnIE = pUpperLayerAssocCnf->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
            pRoamInfo->wapiIELen = (tANI_U8)pUpperLayerAssocCnf->wapiIE.length;
            pRoamInfo->pwapiIE = pUpperLayerAssocCnf->wapiIE.wapiIEdata;
#endif
            pRoamInfo->addIELen = (tANI_U8)pUpperLayerAssocCnf->addIE.length;
            pRoamInfo->paddIE = pUpperLayerAssocCnf->addIE.addIEdata;
            vos_mem_copy(pRoamInfo->peerMac, pUpperLayerAssocCnf->peerMacAddr,
                         sizeof(tSirMacAddr));
            vos_mem_copy(&pRoamInfo->bssid, pUpperLayerAssocCnf->bssId,
                         sizeof(tCsrBssid));
            pRoamInfo->wmmEnabledSta = pUpperLayerAssocCnf->wmmEnabledSta;
            pRoamInfo->timingMeasCap = pUpperLayerAssocCnf->timingMeasCap;
            vos_mem_copy(&pRoamInfo->chan_info, &pUpperLayerAssocCnf->chan_info,
                                                       sizeof(tSirSmeChanInfo));
            if(CSR_IS_INFRA_AP(pRoamInfo->u.pConnectedProfile) )
            {
                pMac->roam.roamSession[sessionId].connectState = eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED;
                pRoamInfo->fReassocReq = pUpperLayerAssocCnf->reassocReq;
                status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
            }
            if(CSR_IS_WDS_AP( pRoamInfo->u.pConnectedProfile))
            {
                vos_sleep( 100 );
                pMac->roam.roamSession[sessionId].connectState = eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED;//Sta
                status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_WDS_IND, eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND);//Sta
            }

        }
        break;
       default:
          csrRoamCheckForLinkStatusChange( pMac, pSirMsg );
          break;
    }
}

eHalStatus csrRoamIssueSetContextReq( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrEncryptionType EncryptType,
                                     tSirBssDescription *pBssDescription,
                                tSirMacAddr *bssId, tANI_BOOLEAN addKey,
                                 tANI_BOOLEAN fUnicast, tAniKeyDirection aniKeyDirection,
                                 tANI_U8 keyId, tANI_U16 keyLength,
                                 tANI_U8 *pKey, tANI_U8 paeRole )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tAniEdType edType;

    if(eCSR_ENCRYPT_TYPE_UNKNOWN == EncryptType)
    {
        EncryptType = eCSR_ENCRYPT_TYPE_NONE; //***
    }

    edType = csrTranslateEncryptTypeToEdType( EncryptType );

    /*
     * Allow 0 keys to be set for the non-WPA encrypt types.
     * For WPA encrypt types, the num keys must be non-zero
     * or LIM will reject the set context (assumes the SET_CONTEXT does not
     * occur until the keys are distributed).
     */
    if ( CSR_IS_ENC_TYPE_STATIC( EncryptType ) ||
           addKey )
    {
        tCsrRoamSetKey setKey;
        setKey.encType = EncryptType;
        setKey.keyDirection = aniKeyDirection;    //Tx, Rx or Tx-and-Rx
        vos_mem_copy(&setKey.peerMac, bssId, sizeof(tCsrBssid));
        setKey.paeRole = paeRole;      //0 for supplicant
        setKey.keyId = keyId;  /* Key index */
        setKey.keyLength = keyLength;
        if( keyLength )
        {
            vos_mem_copy(setKey.Key, pKey, keyLength);
        }
        status = csrRoamIssueSetKeyCommand( pMac, sessionId, &setKey, 0 );
    }
    return (status);
}

static eHalStatus csrRoamIssueSetKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                             tCsrRoamSetKey *pSetKey, tANI_U32 roamId )
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tSmeCmd *pCommand = NULL;
#if defined(FEATURE_WLAN_ESE) || defined (FEATURE_WLAN_WAPI)
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (NULL == pSession) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            FL("session %d not found"), sessionId);
        return eHAL_STATUS_FAILURE;
    }
#endif /* FEATURE_WLAN_ESE */

    do
    {
        pCommand = csrGetCommandBuffer(pMac);
        if(NULL == pCommand)
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        vos_mem_zero(pCommand, sizeof(tSmeCmd));
        pCommand->command = eSmeCommandSetKey;
        pCommand->sessionId = (tANI_U8)sessionId;
        // validate the key length,  Adjust if too long...
        // for static WEP the keys are not set thru' SetContextReq
        if ( ( eCSR_ENCRYPT_TYPE_WEP40 == pSetKey->encType ) ||
             ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pSetKey->encType ) )
        {
            //KeyLength maybe 0 for static WEP
            if( pSetKey->keyLength )
            {
                if ( pSetKey->keyLength < CSR_WEP40_KEY_LEN )
                {
                    smsLog( pMac, LOGW, "Invalid WEP40 keylength [= %d] in SetContext call", pSetKey->keyLength );
                    break;
                }

                pCommand->u.setKeyCmd.keyLength = CSR_WEP40_KEY_LEN;
                vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key,
                             CSR_WEP40_KEY_LEN);
            }
        }
        else if ( ( eCSR_ENCRYPT_TYPE_WEP104 == pSetKey->encType ) ||
             ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pSetKey->encType ) )
        {
            //KeyLength maybe 0 for static WEP
            if( pSetKey->keyLength )
            {
                if ( pSetKey->keyLength < CSR_WEP104_KEY_LEN )
                {
                    smsLog( pMac, LOGW, "Invalid WEP104 keylength [= %d] in SetContext call", pSetKey->keyLength );
                    break;
                }

                pCommand->u.setKeyCmd.keyLength = CSR_WEP104_KEY_LEN;
                vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key,
                             CSR_WEP104_KEY_LEN);
            }
        }
        else if ( eCSR_ENCRYPT_TYPE_TKIP == pSetKey->encType )
        {
            if ( pSetKey->keyLength < CSR_TKIP_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid TKIP keylength [= %d] in SetContext call", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_TKIP_KEY_LEN;
            vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key,
                         CSR_TKIP_KEY_LEN);
        }
        else if ( eCSR_ENCRYPT_TYPE_AES == pSetKey->encType )
        {
            if ( pSetKey->keyLength < CSR_AES_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid AES/CCMP keylength [= %d] in SetContext call", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_AES_KEY_LEN;
            vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key,
                         CSR_AES_KEY_LEN);
        }
#ifdef FEATURE_WLAN_WAPI
        else if ( eCSR_ENCRYPT_TYPE_WPI == pSetKey->encType )
        {
            if ( pSetKey->keyLength < CSR_WAPI_KEY_LEN )
            {
                smsLog( pMac, LOGW,
                        "Invalid WAPI keylength [= %d] in SetContext call",
                        pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_WAPI_KEY_LEN;
            vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key,
                         CSR_WAPI_KEY_LEN);
            if (pSession->pCurRoamProfile)
                pSession->pCurRoamProfile->negotiatedUCEncryptionType =
                                                    eCSR_ENCRYPT_TYPE_WPI;
        }
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
        else if ( eCSR_ENCRYPT_TYPE_KRK == pSetKey->encType )
        {
            if ( pSetKey->keyLength < CSR_KRK_KEY_LEN )
            {
                smsLog( pMac, LOGW,
                        "Invalid KRK keylength [= %d] in SetContext call",
                        pSetKey->keyLength );
                break;
            }
            vos_mem_copy(pSession->eseCckmInfo.krk, pSetKey->Key,
                         CSR_KRK_KEY_LEN);
            pSession->eseCckmInfo.reassoc_req_num=1;
            pSession->eseCckmInfo.krk_plumbed = eANI_BOOLEAN_TRUE;
            status = eHAL_STATUS_SUCCESS;
            break;
        }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        else if (eCSR_ENCRYPT_TYPE_BTK == pSetKey->encType) {
            if (pSetKey->keyLength < SIR_BTK_KEY_LEN) {
                smsLog(pMac, LOGW,
                "LFR3:Invalid BTK keylength [= %d] in SetContext call",
                                              pSetKey->keyLength);
                break;
            }
            vos_mem_copy(pSession->eseCckmInfo.btk, pSetKey->Key,
                         SIR_BTK_KEY_LEN);
            status = eHAL_STATUS_SUCCESS;
            break;
        }
#endif
#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_11W
        //Check for 11w BIP
        else if (eCSR_ENCRYPT_TYPE_AES_CMAC == pSetKey->encType)
        {
            if (pSetKey->keyLength < CSR_AES_KEY_LEN)
            {
                smsLog(pMac, LOGW, "Invalid AES/CCMP keylength [= %d] in SetContext call", pSetKey->keyLength);
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_AES_KEY_LEN;
            vos_mem_copy(pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_AES_KEY_LEN);
        }
#endif
        status = eHAL_STATUS_SUCCESS;
        pCommand->u.setKeyCmd.roamId = roamId;
        pCommand->u.setKeyCmd.encType = pSetKey->encType;
        pCommand->u.setKeyCmd.keyDirection = pSetKey->keyDirection;    //Tx, Rx or Tx-and-Rx
        vos_mem_copy(&pCommand->u.setKeyCmd.peerMac, &pSetKey->peerMac,
                     sizeof(tCsrBssid));
        pCommand->u.setKeyCmd.paeRole = pSetKey->paeRole;      //0 for supplicant
        pCommand->u.setKeyCmd.keyId = pSetKey->keyId;
        vos_mem_copy(pCommand->u.setKeyCmd.keyRsc, pSetKey->keyRsc, CSR_MAX_RSC_LEN);
        //Always put set key to the head of the Q because it is the only thing to get executed in case of WT_KEY state

        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
        }
    } while (0);
    // Free the command if there has been a failure, or it is a
    // "local" operation like the set ESE CCKM KRK key.
    if ( ( NULL != pCommand ) &&
         ( (!HAL_STATUS_SUCCESS( status ) )
#ifdef FEATURE_WLAN_ESE
            || ( eCSR_ENCRYPT_TYPE_KRK == pSetKey->encType )
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            || ( eCSR_ENCRYPT_TYPE_BTK == pSetKey->encType )
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
#endif /* FEATURE_WLAN_ESE */
           ) )
    {
        csrReleaseCommandSetKey( pMac, pCommand );
    }
    return( status );
}

eHalStatus csrRoamIssueRemoveKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                         tCsrRoamRemoveKey *pRemoveKey, tANI_U32 roamId )
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tSmeCmd *pCommand = NULL;
    tANI_BOOLEAN fImediate = eANI_BOOLEAN_TRUE;
    do
    {
        if( !csrIsSetKeyAllowed(pMac, sessionId) )
        {
            smsLog( pMac, LOGW, FL(" wrong state not allowed to set key") );
            status = eHAL_STATUS_CSR_WRONG_STATE;
            break;
        }
        pCommand = csrGetCommandBuffer(pMac);
        if(NULL == pCommand)
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        pCommand->command = eSmeCommandRemoveKey;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.removeKeyCmd.roamId = roamId;
        pCommand->u.removeKeyCmd.encType = pRemoveKey->encType;
        vos_mem_copy(&pCommand->u.removeKeyCmd.peerMac, &pRemoveKey->peerMac,
                     sizeof(tSirMacAddr));
        pCommand->u.removeKeyCmd.keyId = pRemoveKey->keyId;
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
        {
            /* In this case, put it to the end of the Q in-case there
               is a set key pending. */
            fImediate = eANI_BOOLEAN_FALSE;
        }
        smsLog( pMac, LOGE, FL("keyType=%d, keyId=%d, PeerMac="MAC_ADDRESS_STR),
            pRemoveKey->encType, pRemoveKey->keyId,
            MAC_ADDR_ARRAY(pCommand->u.removeKeyCmd.peerMac));
        status = csrQueueSmeCommand(pMac, pCommand, fImediate);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
            break;
        }
    } while (0);
    if( !HAL_STATUS_SUCCESS( status ) && ( NULL != pCommand ) )
    {
        csrReleaseCommandRemoveKey( pMac, pCommand );
    }
    return (status );
}

eHalStatus csrRoamProcessSetKeyCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status;
    tANI_U8 numKeys = ( pCommand->u.setKeyCmd.keyLength ) ? 1 : 0;
    tAniEdType edType = csrTranslateEncryptTypeToEdType( pCommand->u.setKeyCmd.encType );
    tANI_BOOLEAN fUnicast = ( pCommand->u.setKeyCmd.peerMac[0] == 0xFF ) ? eANI_BOOLEAN_FALSE : eANI_BOOLEAN_TRUE;
    tANI_U32 sessionId = pCommand->sessionId;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    WLAN_VOS_DIAG_EVENT_DEF(setKeyEvent, vos_event_wlan_security_payload_type);

    if(NULL == pSession){
       smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
       return eHAL_STATUS_FAILURE;
    }

    if(eSIR_ED_NONE != edType)
    {
        vos_mem_set(&setKeyEvent,
                    sizeof(vos_event_wlan_security_payload_type), 0);
        if( *(( tANI_U8 *)&pCommand->u.setKeyCmd.peerMac) & 0x01 )
        {
            setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_GTK_REQ;
            setKeyEvent.encryptionModeMulticast = (v_U8_t)diagEncTypeFromCSRType(pCommand->u.setKeyCmd.encType);
            setKeyEvent.encryptionModeUnicast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
        }
        else
        {
            setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_PTK_REQ;
            setKeyEvent.encryptionModeUnicast = (v_U8_t)diagEncTypeFromCSRType(pCommand->u.setKeyCmd.encType);
            setKeyEvent.encryptionModeMulticast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
        }
        vos_mem_copy(setKeyEvent.bssid, pSession->connectedProfile.bssid, 6);
        if(CSR_IS_ENC_TYPE_STATIC(pCommand->u.setKeyCmd.encType))
        {
            tANI_U32 defKeyId;
            //It has to be static WEP here
            if(HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_WEP_DEFAULT_KEYID, &defKeyId)))
            {
                setKeyEvent.keyId = (v_U8_t)defKeyId;
            }
        }
        else
        {
            setKeyEvent.keyId = pCommand->u.setKeyCmd.keyId;
        }
        setKeyEvent.authMode = (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
        WLAN_VOS_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    if( csrIsSetKeyAllowed(pMac, sessionId) )
    {
        status = csrSendMBSetContextReqMsg( pMac, sessionId,
                    ( tANI_U8 *)&pCommand->u.setKeyCmd.peerMac,
                                            numKeys, edType, fUnicast, pCommand->u.setKeyCmd.keyDirection,
                                            pCommand->u.setKeyCmd.keyId, pCommand->u.setKeyCmd.keyLength,
                    pCommand->u.setKeyCmd.Key, pCommand->u.setKeyCmd.paeRole,
                    pCommand->u.setKeyCmd.keyRsc);
    }
    else
    {
        smsLog( pMac, LOGW, FL(" cannot process not connected") );
        //Set this status so the error handling take care of the case.
        status = eHAL_STATUS_CSR_WRONG_STATE;
    }
    if( !HAL_STATUS_SUCCESS(status) )
    {
        smsLog( pMac, LOGE, FL("  error status %d"), status );
        csrRoamCallCallback( pMac, sessionId, NULL, pCommand->u.setKeyCmd.roamId, eCSR_ROAM_SET_KEY_COMPLETE, eCSR_ROAM_RESULT_FAILURE);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        if(eSIR_ED_NONE != edType)
        {
            if( *(( tANI_U8 *)&pCommand->u.setKeyCmd.peerMac) & 0x01 )
            {
                setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_GTK_RSP;
            }
            else
            {
                setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_PTK_RSP;
            }
            setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
            WLAN_VOS_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    }
    return ( status );
}

eHalStatus csrRoamProcessRemoveKeyCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status;
    tpSirSmeRemoveKeyReq pMsg = NULL;
    tANI_U16 wMsgLen = sizeof(tSirSmeRemoveKeyReq);
    tANI_U8 *p;
    tANI_U32 sessionId = pCommand->sessionId;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    WLAN_VOS_DIAG_EVENT_DEF(removeKeyEvent, vos_event_wlan_security_payload_type);

    if(NULL == pSession){
       smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
       return eHAL_STATUS_FAILURE;
    }

    vos_mem_set(&removeKeyEvent,
                sizeof(vos_event_wlan_security_payload_type),0);
    removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_REQ;
    removeKeyEvent.encryptionModeMulticast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
    removeKeyEvent.encryptionModeUnicast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
    vos_mem_copy(removeKeyEvent.bssid, pSession->connectedProfile.bssid, 6);
    removeKeyEvent.keyId = pCommand->u.removeKeyCmd.keyId;
    removeKeyEvent.authMode = (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
    WLAN_VOS_DIAG_EVENT_REPORT(&removeKeyEvent, EVENT_WLAN_SECURITY);
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    if( csrIsSetKeyAllowed(pMac, sessionId) )
    {
        pMsg = vos_mem_malloc(wMsgLen);
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
    }
    else
    {
        smsLog( pMac, LOGW, FL(" wrong state not allowed to set key") );
        //Set the error status so error handling kicks in below
        status = eHAL_STATUS_CSR_WRONG_STATE;
    }
    if( HAL_STATUS_SUCCESS( status ) )
    {
        vos_mem_set(pMsg, wMsgLen ,0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_REMOVEKEY_REQ);
                pMsg->length = pal_cpu_to_be16(wMsgLen);
        pMsg->sessionId = (tANI_U8)sessionId;
        pMsg->transactionId = 0;
        p = (tANI_U8 *)pMsg + sizeof(pMsg->messageType) + sizeof(pMsg->length) +
            sizeof(pMsg->sessionId) + sizeof(pMsg->transactionId);
        // bssId - copy from session Info
        vos_mem_copy(p,
                     &pMac->roam.roamSession[sessionId].connectedProfile.bssid,
                     sizeof(tSirMacAddr));
        p += sizeof(tSirMacAddr);
        // peerMacAddr
        vos_mem_copy(p, pCommand->u.removeKeyCmd.peerMac, sizeof(tSirMacAddr));
        p += sizeof(tSirMacAddr);
        // edType
        *p = (tANI_U8)csrTranslateEncryptTypeToEdType( pCommand->u.removeKeyCmd.encType );
        p++;
        // weptype
        if( ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pCommand->u.removeKeyCmd.encType ) ||
            ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pCommand->u.removeKeyCmd.encType ) )
        {
            *p = (tANI_U8)eSIR_WEP_STATIC;
        }
        else
        {
            *p = (tANI_U8)eSIR_WEP_DYNAMIC;
        }
        p++;
        //keyid
        *p = pCommand->u.removeKeyCmd.keyId;
        p++;
        *p = (pCommand->u.removeKeyCmd.peerMac[0] == 0xFF ) ? 0 : 1;
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }
    if( !HAL_STATUS_SUCCESS( status ) )
    {
        smsLog( pMac, LOGE, FL(" error status %d"), status );
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_RSP;
        removeKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
        WLAN_VOS_DIAG_EVENT_REPORT(&removeKeyEvent, EVENT_WLAN_SECURITY);
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
        csrRoamCallCallback( pMac, sessionId, NULL, pCommand->u.removeKeyCmd.roamId, eCSR_ROAM_REMOVE_KEY_COMPLETE, eCSR_ROAM_RESULT_FAILURE);
    }
    return ( status );
}

eHalStatus csrRoamSetKey( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamSetKey *pSetKey, tANI_U32 roamId )
{
    eHalStatus status;

    if( !csrIsSetKeyAllowed(pMac, sessionId) )
    {
        status = eHAL_STATUS_CSR_WRONG_STATE;
    }
    else
    {
        status = csrRoamIssueSetKeyCommand( pMac, sessionId, pSetKey, roamId );
    }
    return ( status );
}

/*
   Prepare a filter base on a profile for parsing the scan results.
   Upon successful return, caller MUST call csrFreeScanFilter on
   pScanFilter when it is done with the filter.
*/
eHalStatus csrRoamPrepareFilterFromProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,
                                           tCsrScanResultFilter *pScanFilter)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    uint32_t size = 0;
    tANI_U8  index = 0;
    struct roam_ext_params *roam_params;
    uint8_t i;

    roam_params = &pMac->roam.configParam.roam_params;

    do
    {
        if(pProfile->BSSIDs.numOfBSSIDs)
        {
            size = sizeof(tCsrBssid) * pProfile->BSSIDs.numOfBSSIDs;
            pScanFilter->BSSIDs.bssid = vos_mem_malloc(size);
            if ( NULL == pScanFilter->BSSIDs.bssid )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pScanFilter->BSSIDs.numOfBSSIDs = pProfile->BSSIDs.numOfBSSIDs;
            vos_mem_copy(pScanFilter->BSSIDs.bssid, pProfile->BSSIDs.bssid, size);
        }
        if(pProfile->SSIDs.numOfSSIDs)
        {
            if( !CSR_IS_WDS_STA( pProfile ) )
            {
                pScanFilter->SSIDs.numOfSSIDs = pProfile->SSIDs.numOfSSIDs;
            }
            else
            {
                //For WDS station
                //We always use index 1 for self SSID. Index 0 for peer's SSID that we want to join
                pScanFilter->SSIDs.numOfSSIDs = 1;
            }
          VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
              FL("No of Allowed List:%d"), roam_params->num_ssid_allowed_list);
          if (pScanFilter->scan_filter_for_roam
                && roam_params->num_ssid_allowed_list) {
             pScanFilter->SSIDs.numOfSSIDs =
                    roam_params->num_ssid_allowed_list;
             size = sizeof(tCsrSSIDInfo) * pScanFilter->SSIDs.numOfSSIDs;
             pScanFilter->SSIDs.SSIDList = vos_mem_malloc(size);
          if ( NULL == pScanFilter->SSIDs.SSIDList)
             status = eHAL_STATUS_FAILURE;
          else
             status = eHAL_STATUS_SUCCESS;
          if(!HAL_STATUS_SUCCESS(status))
             break;
          for (i=0; i<roam_params->num_ssid_allowed_list; i++) {
             vos_mem_copy((void *)pScanFilter->SSIDs.SSIDList[i].SSID.ssId,
               roam_params->ssid_allowed_list[i].ssId,
               roam_params->ssid_allowed_list[i].length);
             pScanFilter->SSIDs.SSIDList[i].SSID.length =
               roam_params->ssid_allowed_list[i].length;
             pScanFilter->SSIDs.SSIDList[i].handoffPermitted = 1;
             pScanFilter->SSIDs.SSIDList[i].ssidHidden = 0;
          }
          } else {
            size = sizeof(tCsrSSIDInfo) * pProfile->SSIDs.numOfSSIDs;
            pScanFilter->SSIDs.SSIDList = vos_mem_malloc(size);
            if ( NULL == pScanFilter->SSIDs.SSIDList )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            vos_mem_copy(pScanFilter->SSIDs.SSIDList, pProfile->SSIDs.SSIDList,
                         size);
          }
        }
        if(!pProfile->ChannelInfo.ChannelList || (pProfile->ChannelInfo.ChannelList[0] == 0) )
        {
            pScanFilter->ChannelInfo.numOfChannels = 0;
            pScanFilter->ChannelInfo.ChannelList = NULL;
        }
        else if(pProfile->ChannelInfo.numOfChannels)
        {
           pScanFilter->ChannelInfo.ChannelList = vos_mem_malloc(
                                     sizeof(*pScanFilter->ChannelInfo.ChannelList) *
                                     pProfile->ChannelInfo.numOfChannels);
           if ( NULL == pScanFilter->ChannelInfo.ChannelList )
               status = eHAL_STATUS_FAILURE;
           else
               status = eHAL_STATUS_SUCCESS;

           pScanFilter->ChannelInfo.numOfChannels = 0;
            if(HAL_STATUS_SUCCESS(status))
            {
              for(index = 0; index < pProfile->ChannelInfo.numOfChannels; index++)
              {
                 if(csrRoamIsChannelValid(pMac, pProfile->ChannelInfo.ChannelList[index]))
                 {
                    pScanFilter->ChannelInfo.ChannelList[pScanFilter->ChannelInfo.numOfChannels]
                       = pProfile->ChannelInfo.ChannelList[index];
                    pScanFilter->ChannelInfo.numOfChannels++;
                 }
                 else
                 {
                         smsLog(pMac, LOG1, FL("process a channel (%d) that is invalid"), pProfile->ChannelInfo.ChannelList[index]);
                 }
            }
            }
            else
            {
                break;
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL("Channel list empty"));
            status = eHAL_STATUS_FAILURE;
            break;
        }
        pScanFilter->uapsd_mask = pProfile->uapsd_mask;
        pScanFilter->authType = pProfile->AuthType;
        pScanFilter->EncryptionType = pProfile->EncryptionType;
        pScanFilter->mcEncryptionType = pProfile->mcEncryptionType;
        pScanFilter->BSSType = pProfile->BSSType;
        pScanFilter->phyMode = pProfile->phyMode;
#ifdef FEATURE_WLAN_WAPI
        //check if user asked for WAPI with 11n or auto mode, in that case modify
        //the phymode to 11g
        if(csrIsProfileWapi(pProfile))
        {
             if(pScanFilter->phyMode & eCSR_DOT11_MODE_11n)
             {
                pScanFilter->phyMode &= ~eCSR_DOT11_MODE_11n;
             }
             if(pScanFilter->phyMode & eCSR_DOT11_MODE_AUTO)
             {
                pScanFilter->phyMode &= ~eCSR_DOT11_MODE_AUTO;
             }
             if(!pScanFilter->phyMode)
             {
                 pScanFilter->phyMode = eCSR_DOT11_MODE_11g;
             }
        }
#endif /* FEATURE_WLAN_WAPI */
        /*Save the WPS info*/
        pScanFilter->bWPSAssociation = pProfile->bWPSAssociation;
        pScanFilter->bOSENAssociation = pProfile->bOSENAssociation;
        if( pProfile->countryCode[0] )
        {
            //This causes the matching function to use countryCode as one of the criteria.
            vos_mem_copy(pScanFilter->countryCode, pProfile->countryCode,
                         WNI_CFG_COUNTRY_CODE_LEN);
        }
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pProfile->MDID.mdiePresent)
        {
            pScanFilter->MDID.mdiePresent = 1;
            pScanFilter->MDID.mobilityDomain = pProfile->MDID.mobilityDomain;
        }
#endif

#ifdef WLAN_FEATURE_11W
        // Management Frame Protection
        pScanFilter->MFPEnabled = pProfile->MFPEnabled;
        pScanFilter->MFPRequired = pProfile->MFPRequired;
        pScanFilter->MFPCapable = pProfile->MFPCapable;
#endif

    }while(0);

    if(!HAL_STATUS_SUCCESS(status))
    {
        csrFreeScanFilter(pMac, pScanFilter);
    }

    return(status);
}

tANI_BOOLEAN csrRoamIssueWmStatusChange( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                         eCsrRoamWmStatusChangeTypes Type, tSirSmeRsp *pSmeRsp )
{
    tANI_BOOLEAN fCommandQueued = eANI_BOOLEAN_FALSE;
    tSmeCmd *pCommand;
    do
    {
        // Validate the type is ok...
        if ( ( eCsrDisassociated != Type ) && ( eCsrDeauthenticated != Type ) ) break;
        pCommand = csrGetCommandBuffer( pMac );
        if ( !pCommand )
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer") );
            break;
        }
        //Change the substate in case it is waiting for key
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
        {
            csrRoamStopWaitForKeyTimer( pMac );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
        }
        pCommand->command = eSmeCommandWmStatusChange;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.wmStatusChangeCmd.Type = Type;
        if ( eCsrDisassociated ==  Type )
        {
            vos_mem_copy(&pCommand->u.wmStatusChangeCmd.u.DisassocIndMsg,
                         pSmeRsp,
                         sizeof( pCommand->u.wmStatusChangeCmd.u.DisassocIndMsg ));
        }
        else
        {
            vos_mem_copy(&pCommand->u.wmStatusChangeCmd.u.DeauthIndMsg,
                         pSmeRsp,
                         sizeof( pCommand->u.wmStatusChangeCmd.u.DeauthIndMsg ));
        }
        if( HAL_STATUS_SUCCESS( csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_TRUE) ) )
        {
            fCommandQueued = eANI_BOOLEAN_TRUE;
        }
        else
        {
            smsLog( pMac, LOGE, FL(" fail to send message ") );
            csrReleaseCommandWmStatusChange( pMac, pCommand );
        }

        /* AP has issued Dissac/Deauth, Set the operating mode value to configured value */
        csrSetDefaultDot11Mode( pMac );
    } while( 0 );
    return( fCommandQueued );
}

static void csrUpdateRssi(tpAniSirGlobal pMac, void* pMsg)
{
    v_S7_t  rssi = 0;
    tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq*)pMsg;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    if(pGetRssiReq)
    {
        if(NULL != pGetRssiReq->pVosContext)
        {
            vosStatus = WLANTL_GetRssi(pGetRssiReq->pVosContext, pGetRssiReq->staId, &rssi,pGetRssiReq);
        }
        else
        {
            smsLog( pMac, LOGE, FL("pGetRssiReq->pVosContext is NULL"));
            return;
        }

        if(NULL != pGetRssiReq->rssiCallback)
        {
            if(vosStatus!=VOS_STATUS_E_BUSY)
               ((tCsrRssiCallback)(pGetRssiReq->rssiCallback))(rssi, pGetRssiReq->staId, pGetRssiReq->pDevContext);
            else smsLog( pMac, LOG1, FL("rssi request is posted. waiting for reply"));
        }
        else
        {
            smsLog( pMac, LOGE, FL("pGetRssiReq->rssiCallback is NULL"));
            return;
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL("pGetRssiReq is NULL"));
    }
    return;

}

static void csrUpdateSnr(tpAniSirGlobal pMac, void* pMsg)
{
    tAniGetSnrReq *pGetSnrReq = (tAniGetSnrReq*)pMsg;

    if (pGetSnrReq)
    {
        if (VOS_STATUS_SUCCESS !=
            WDA_GetSnr(pGetSnrReq))
        {
            smsLog(pMac, LOGE, FL("Error in WDA_GetSnr"));
            return;
        }
    }
    else
    {
        smsLog(pMac, LOGE, FL("pGetSnrReq is NULL"));
    }

    return;
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
void csrTsmStatsRspProcessor(tpAniSirGlobal pMac, void* pMsg)
{
  tAniGetTsmStatsRsp* pTsmStatsRsp = (tAniGetTsmStatsRsp*)pMsg;

  if (NULL != pTsmStatsRsp)
  {
    /* Get roam Rssi request is backed up and passed back to the response,
              Extract the request message to fetch callback */
    tpAniGetTsmStatsReq reqBkp
       = (tAniGetTsmStatsReq*)pTsmStatsRsp->tsmStatsReq;

    if (NULL != reqBkp)
    {
      if (NULL != reqBkp->tsmStatsCallback)
      {
        ((tCsrTsmStatsCallback)(reqBkp->tsmStatsCallback))(
                                pTsmStatsRsp->tsmMetrics,
                                pTsmStatsRsp->staId,
                                reqBkp->pDevContext
                                );
        reqBkp->tsmStatsCallback = NULL;
      }
      vos_mem_free(reqBkp);
      pTsmStatsRsp->tsmStatsReq = NULL;
    }
    else
    {
        smsLog( pMac, LOGE, FL("reqBkp is NULL"));
        if (NULL != reqBkp)
        {
            vos_mem_free(reqBkp);
            pTsmStatsRsp->tsmStatsReq = NULL;
        }
    }
  }
  else
  {
      smsLog( pMac, LOGE, FL("pTsmStatsRsp is NULL"));
  }
  return;
}

void csrSendEseAdjacentApRepInd(tpAniSirGlobal pMac, tCsrRoamSession *pSession)
{
    tANI_U32 roamTS2 = 0;
    tCsrRoamInfo roamInfo;
    tpPESession pSessionEntry = NULL;
    tANI_U8 sessionId = CSR_SESSION_ID_INVALID;

    if (NULL == pSession)
    {
        smsLog(pMac, LOGE, FL("pSession is NULL"));
        return;
    }

    roamTS2 = vos_timer_get_system_time();
    roamInfo.tsmRoamDelay = roamTS2 - pSession->roamTS1;
    smsLog(pMac, LOG1, "Bssid("MAC_ADDRESS_STR") Roaming Delay(%u ms)",
           MAC_ADDR_ARRAY(pSession->connectedProfile.bssid),
           roamInfo.tsmRoamDelay);

    pSessionEntry = peFindSessionByBssid(pMac,
                                         pSession->connectedProfile.bssid,
                                         &sessionId);
    if (NULL == pSessionEntry)
    {
        smsLog(pMac, LOGE, FL("session %d not found"), sessionId);
        return;
    }

    pSessionEntry->eseContext.tsm.tsmMetrics.RoamingDly
       = roamInfo.tsmRoamDelay;

    csrRoamCallCallback(pMac, pSession->sessionId, &roamInfo,
                        0, eCSR_ROAM_ESE_ADJ_AP_REPORT_IND, 0);
}
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

static void csrRoamRssiIndHdlr(tpAniSirGlobal pMac, void* pMsg)
{
    WLANTL_TlIndicationReq *pTlRssiInd = (WLANTL_TlIndicationReq*)pMsg;
    if(pTlRssiInd)
    {
        if(NULL != pTlRssiInd->tlCallback)
        {
            ((WLANTL_RSSICrossThresholdCBType)(pTlRssiInd->tlCallback))
            (pTlRssiInd->pAdapter, pTlRssiInd->rssiNotification, pTlRssiInd->pUserCtxt, pTlRssiInd->avgRssi);
        }
        else
        {
            smsLog( pMac, LOGE, FL("pTlRssiInd->tlCallback is NULL"));
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL("pTlRssiInd is NULL"));
    }
    return;
}

eHalStatus csrSendResetApCapsChanged(tpAniSirGlobal pMac, tSirMacAddr *bssId)
{
    tpSirResetAPCapsChange pMsg;
    tANI_U16 len;
    eHalStatus status   = eHAL_STATUS_SUCCESS;

    /* Create the message and send to lim */
    len = sizeof(tSirResetAPCapsChange);
    pMsg = vos_mem_malloc(len);
    if ( NULL == pMsg )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (HAL_STATUS_SUCCESS(status))
    {
        vos_mem_set(pMsg, sizeof(tSirResetAPCapsChange), 0);
        pMsg->messageType     = eWNI_SME_RESET_AP_CAPS_CHANGED;
        pMsg->length          = len;
        vos_mem_copy(pMsg->bssId, bssId, sizeof(tSirMacAddr));
        smsLog( pMac, LOG1, FL("CSR reset caps change for Bssid= "MAC_ADDRESS_STR),
                MAC_ADDR_ARRAY(pMsg->bssId));
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }
    else
    {
        smsLog( pMac, LOGE, FL("Memory allocation failed\n"));
    }
    return status;
}

void csrRoamCheckForLinkStatusChange( tpAniSirGlobal pMac, tSirSmeRsp *pSirMsg )
{
    tSirSmeAssocInd *pAssocInd;
    tSirSmeDisassocInd *pDisassocInd;
    tSirSmeDeauthInd *pDeauthInd;
    tSirSmeWmStatusChangeNtf *pStatusChangeMsg;
    tSirSmeNewBssInfo *pNewBss;
    tSmeIbssPeerInd *pIbssPeerInd;
    tSirMacAddr Broadcastaddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    tSirSmeApNewCaps *pApNewCaps;
    eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
    eRoamCmdStatus roamStatus = eCSR_ROAM_FAILED;
    tCsrRoamInfo *pRoamInfo = NULL;
    tCsrRoamInfo roamInfo;
    eHalStatus status;
    tANI_U32 sessionId = CSR_SESSION_ID_INVALID;
    tCsrRoamSession *pSession = NULL;
    tpSirSmeSwitchChannelInd pSwitchChnInd;
    tSmeMaxAssocInd *pSmeMaxAssocInd;
    tSmeCmd pCommand;
    vos_mem_set(&roamInfo, sizeof(roamInfo), 0);

    if (NULL == pSirMsg)
    {   smsLog(pMac, LOGE, FL("pSirMsg is NULL"));
        return;
    }
    switch( pSirMsg->messageType )
    {
        case eWNI_SME_ASSOC_IND:
            {
                tCsrRoamSession  *pSession;
                smsLog( pMac, LOG1, FL("Receive WNI_SME_ASSOC_IND from SME"));
                pAssocInd = (tSirSmeAssocInd *)pSirMsg;
                status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pAssocInd->bssId, &sessionId );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    pSession = CSR_GET_SESSION(pMac, sessionId);

                    if(!pSession)
                    {
                        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                        return;
                    }

                pRoamInfo = &roamInfo;

                // Required for indicating the frames to upper layer
                pRoamInfo->assocReqLength = pAssocInd->assocReqLength;
                pRoamInfo->assocReqPtr = pAssocInd->assocReqPtr;

                pRoamInfo->beaconPtr = pAssocInd->beaconPtr;
                pRoamInfo->beaconLength = pAssocInd->beaconLength;
                pRoamInfo->statusCode = eSIR_SME_SUCCESS; //send the status code as Success
                pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

                pRoamInfo->staId = (tANI_U8)pAssocInd->staId;
                pRoamInfo->rsnIELen = (tANI_U8)pAssocInd->rsnIE.length;
                pRoamInfo->prsnIE = pAssocInd->rsnIE.rsnIEdata;

#ifdef FEATURE_WLAN_WAPI
                pRoamInfo->wapiIELen = (tANI_U8)pAssocInd->wapiIE.length;
                pRoamInfo->pwapiIE = pAssocInd->wapiIE.wapiIEdata;
#endif
                pRoamInfo->addIELen = (tANI_U8)pAssocInd->addIE.length;
                pRoamInfo->paddIE =  pAssocInd->addIE.addIEdata;
                    vos_mem_copy(pRoamInfo->peerMac, pAssocInd->peerMacAddr,
                                 sizeof(tSirMacAddr));
                    vos_mem_copy(&pRoamInfo->bssid, pAssocInd->bssId,
                                 sizeof(tCsrBssid));
                    pRoamInfo->wmmEnabledSta = pAssocInd->wmmEnabledSta;
                    pRoamInfo->timingMeasCap = pAssocInd->timingMeasCap;
                    vos_mem_copy(&pRoamInfo->chan_info, &pAssocInd->chan_info,
                                 sizeof(tSirSmeChanInfo));
                    if(CSR_IS_WDS_AP( pRoamInfo->u.pConnectedProfile))
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_WDS_IND, eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND);//Sta
                    if(CSR_IS_INFRA_AP(pRoamInfo->u.pConnectedProfile))
                    {
                        if(pSession->pCurRoamProfile &&
                            CSR_IS_ENC_TYPE_STATIC(pSession->pCurRoamProfile->negotiatedUCEncryptionType))
                        {
                            csrRoamIssueSetContextReq( pMac, sessionId, pSession->pCurRoamProfile->negotiatedUCEncryptionType,
                                    pSession->pConnectBssDesc,
                                    &(pRoamInfo->peerMac),
                                    FALSE, TRUE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                            pRoamInfo->fAuthRequired = FALSE;
                        }
                        else
                        {
                            pRoamInfo->fAuthRequired = TRUE;
                        }
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND);
                        if (!HAL_STATUS_SUCCESS(status))
                            pRoamInfo->statusCode = eSIR_SME_ASSOC_REFUSED;// Refused due to Mac filtering
                    }
                    /* Send Association completion message to PE */
                    status = csrSendAssocCnfMsg( pMac, pAssocInd, status );//Sta

                    /* send a message to CSR itself just to avoid the EAPOL frames going
                     * OTA before association response */
                    if(CSR_IS_WDS_AP( pRoamInfo->u.pConnectedProfile))
                {
                    status = csrSendAssocIndToUpperLayerCnfMsg(pMac, pAssocInd, status, sessionId);
                }
                else if(CSR_IS_INFRA_AP(pRoamInfo->u.pConnectedProfile) && (pRoamInfo->statusCode != eSIR_SME_ASSOC_REFUSED))
                {
                    pRoamInfo->fReassocReq = pAssocInd->reassocReq;
                    //status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
                    status = csrSendAssocIndToUpperLayerCnfMsg(pMac, pAssocInd, status, sessionId);
                }
                }
            }
            break;
        case eWNI_SME_DISASSOC_IND:
            // Check if AP dis-associated us because of MIC failure. If so,
            // then we need to take action immediately and not wait till the
            // the WmStatusChange requests is pushed and processed
            pDisassocInd = (tSirSmeDisassocInd *)pSirMsg;
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pDisassocInd->bssId, &sessionId );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                smsLog( pMac, LOGE, FL("DISASSOCIATION Indication from MAC for session %d "), sessionId);
                smsLog( pMac, LOGE, FL("DISASSOCIATION from peer =" MAC_ADDRESS_STR " "
                                       " reason = %d status = %d "),
                                       MAC_ADDR_ARRAY(pDisassocInd->peerMacAddr),
                                       pDisassocInd->reasonCode, pDisassocInd->statusCode);
                // If we are in neighbor preauth done state then on receiving
                // disassoc or deauth we dont roam instead we just disassoc
                // from current ap and then go to disconnected state
                // This happens for ESE and 11r FT connections ONLY.
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (csrRoamIs11rAssoc(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
#ifdef FEATURE_WLAN_ESE
                if (csrRoamIsESEAssoc(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
#ifdef FEATURE_WLAN_LFR
                if (csrRoamIsFastRoamEnabled(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
                pSession = CSR_GET_SESSION( pMac, sessionId );

                if(!pSession)
                {
                    smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                    return;
                }

                if ( csrIsConnStateInfra( pMac, sessionId ) )
                {
                    pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
                }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                csrRoamLinkDown(pMac, sessionId);
                csrRoamIssueWmStatusChange( pMac, sessionId, eCsrDisassociated, pSirMsg );
                if(CSR_IS_INFRA_AP(&pSession->connectedProfile))
                {

                    pRoamInfo = &roamInfo;

                    pRoamInfo->statusCode = pDisassocInd->statusCode;
                    pRoamInfo->reasonCode = pDisassocInd->reasonCode;
                    pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

                    pRoamInfo->staId = (tANI_U8)pDisassocInd->staId;

                    vos_mem_copy(pRoamInfo->peerMac, pDisassocInd->peerMacAddr,
                                 sizeof(tSirMacAddr));
                    vos_mem_copy(&pRoamInfo->bssid, pDisassocInd->bssId,
                                 sizeof(tCsrBssid));

                    status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_DISASSOC_IND);

                    /*
                    *  STA/P2P client got  disassociated so remove any pending deauth
                    *  commands in sme pending list
                    */
                    pCommand.command = eSmeCommandRoam;
                    pCommand.sessionId = (tANI_U8)sessionId;
                    pCommand.u.roamCmd.roamReason = eCsrForcedDeauthSta;
                    vos_mem_copy(pCommand.u.roamCmd.peerMac,
                                 pDisassocInd->peerMacAddr,
                                 sizeof(tSirMacAddr));
                    csrRoamRemoveDuplicateCommand(pMac, sessionId, &pCommand, eCsrForcedDeauthSta);
                }
            }
            else
            {
                smsLog(pMac, LOGE, FL(" Session Id not found for BSSID " MAC_ADDRESS_STR),
                                        MAC_ADDR_ARRAY(pDisassocInd->bssId));
            }
            break;
        case eWNI_SME_DEAUTH_IND:
            smsLog( pMac, LOG1, FL("DEAUTHENTICATION Indication from MAC"));
            pDeauthInd = (tpSirSmeDeauthInd)pSirMsg;
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pDeauthInd->bssId, &sessionId );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                // If we are in neighbor preauth done state then on receiving
                // disassoc or deauth we dont roam instead we just disassoc
                // from current ap and then go to disconnected state
                // This happens for ESE and 11r FT connections ONLY.
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (csrRoamIs11rAssoc(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
#ifdef FEATURE_WLAN_ESE
                if (csrRoamIsESEAssoc(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
#ifdef FEATURE_WLAN_LFR
                if (csrRoamIsFastRoamEnabled(pMac, sessionId) &&
                   (csrNeighborRoamStatePreauthDone(pMac, sessionId))) {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac,
                                                                     sessionId);
                }
#endif
                pSession = CSR_GET_SESSION( pMac, sessionId );

                if(!pSession)
                {
                    smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                    return;
                }

                if ( csrIsConnStateInfra( pMac, sessionId ) )
                {
                    pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
                }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                csrRoamLinkDown(pMac, sessionId);
                csrRoamIssueWmStatusChange( pMac, sessionId, eCsrDeauthenticated, pSirMsg );
                if(CSR_IS_INFRA_AP(&pSession->connectedProfile))
                {

                    pRoamInfo = &roamInfo;

                    pRoamInfo->statusCode = pDeauthInd->statusCode;
                    pRoamInfo->reasonCode = pDeauthInd->reasonCode;
                    pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

                    pRoamInfo->staId = (tANI_U8)pDeauthInd->staId;

                    vos_mem_copy(pRoamInfo->peerMac, pDeauthInd->peerMacAddr,
                                 sizeof(tSirMacAddr));
                    vos_mem_copy(&pRoamInfo->bssid, pDeauthInd->bssId,
                                 sizeof(tCsrBssid));

                    status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_DEAUTH_IND);
                }
            }
            break;

        case eWNI_SME_SWITCH_CHL_REQ:        // in case of STA, the SWITCH_CHANNEL originates from its AP
            smsLog( pMac, LOGW, FL("eWNI_SME_SWITCH_CHL_REQ from SME"));
            pSwitchChnInd = (tpSirSmeSwitchChannelInd)pSirMsg;
            //Update with the new channel id.
            //The channel id is hidden in the statusCode.
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pSwitchChnInd->bssId, &sessionId );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                pSession = CSR_GET_SESSION( pMac, sessionId );
                if(!pSession)
                {
                    smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                    return;
                }
                pSession->connectedProfile.operationChannel = (tANI_U8)pSwitchChnInd->newChannelId;
                if(pSession->pConnectBssDesc)
                {
                    pSession->pConnectBssDesc->channelId = (tANI_U8)pSwitchChnInd->newChannelId;
                }
            }
            break;

        case eWNI_SME_DEAUTH_RSP:
            smsLog( pMac, LOGW, FL("eWNI_SME_DEAUTH_RSP from SME"));
            {
                tSirSmeDeauthRsp* pDeauthRsp = (tSirSmeDeauthRsp *)pSirMsg;
                sessionId = pDeauthRsp->sessionId;
                if( CSR_IS_SESSION_VALID(pMac, sessionId) )
                {
                    pSession = CSR_GET_SESSION(pMac, sessionId);
                    if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                    {
                        pRoamInfo = &roamInfo;
                        pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
                        vos_mem_copy(pRoamInfo->peerMac, pDeauthRsp->peerMacAddr,
                                     sizeof(tSirMacAddr));
                        pRoamInfo->reasonCode = eCSR_ROAM_RESULT_FORCED;
                        pRoamInfo->statusCode = pDeauthRsp->statusCode;
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                    }
                }
            }
            break;

        case eWNI_SME_DISASSOC_RSP:
            /* session id is invalid here so cant use it to access the array curSubstate as index */
            smsLog( pMac, LOGW, FL("eWNI_SME_DISASSOC_RSP from SME "));
            {
                tSirSmeDisassocRsp *pDisassocRsp = (tSirSmeDisassocRsp *)pSirMsg;
                sessionId = pDisassocRsp->sessionId;
                if( CSR_IS_SESSION_VALID(pMac, sessionId) )
                {
                    pSession = CSR_GET_SESSION(pMac, sessionId);
                    if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                    {
                        pRoamInfo = &roamInfo;
                        pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
                        vos_mem_copy(pRoamInfo->peerMac, pDisassocRsp->peerMacAddr,
                                     sizeof(tSirMacAddr));
                        pRoamInfo->reasonCode = eCSR_ROAM_RESULT_FORCED;
                        pRoamInfo->statusCode = pDisassocRsp->statusCode;
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                    }
                }
            }
            break;
        case eWNI_SME_MIC_FAILURE_IND:
            {
                tpSirSmeMicFailureInd pMicInd = (tpSirSmeMicFailureInd)pSirMsg;
                eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;

                status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pMicInd->bssId, &sessionId );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                    roamInfo.u.pMICFailureInfo = &pMicInd->info;
                    pRoamInfo = &roamInfo;
                    if(pMicInd->info.multicast)
                    {
                        result = eCSR_ROAM_RESULT_MIC_ERROR_GROUP;
                    }
                    else
                    {
                        result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
                    }
                    csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_MIC_ERROR_IND, result);
                }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                {
                    WLAN_VOS_DIAG_EVENT_DEF(secEvent, vos_event_wlan_security_payload_type);
                    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
                    if(!pSession)
                    {
                        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                        return;
                    }

                    vos_mem_set(&secEvent, sizeof(vos_event_wlan_security_payload_type), 0);
                    secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
                    secEvent.encryptionModeMulticast =
                        (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
                    secEvent.encryptionModeUnicast =
                        (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
                    secEvent.authMode =
                        (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                    vos_mem_copy(secEvent.bssid,
                                 pSession->connectedProfile.bssid, 6);
                    WLAN_VOS_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
                }
#endif//FEATURE_WLAN_DIAG_SUPPORT_CSR
            }
            break;
        case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
            {
                tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd)pSirMsg;
                smsLog( pMac, LOG1, FL("WPS PBC Probe request Indication from SME"));

                status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pProbeReqInd->bssId, &sessionId );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                    roamInfo.u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
                    csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
                        eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
                }
            }
            break;

        case eWNI_SME_WM_STATUS_CHANGE_NTF:
            pStatusChangeMsg = (tSirSmeWmStatusChangeNtf *)pSirMsg;
            switch( pStatusChangeMsg->statusChangeCode )
            {
                case eSIR_SME_IBSS_ACTIVE:
                    sessionId = csrFindIbssSession( pMac );
                    if( CSR_SESSION_ID_INVALID != sessionId )
                    {
                        pSession = CSR_GET_SESSION( pMac, sessionId );
                        if(!pSession)
                        {
                            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                            return;
                        }
                        pSession->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_CONNECTED;
                        if(pSession->pConnectBssDesc)
                        {
                            vos_mem_copy(&roamInfo.bssid,
                                         pSession->pConnectBssDesc->bssId,
                                         sizeof(tCsrBssid));
                            roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                            pRoamInfo = &roamInfo;
                        }
                        else
                        {
                            smsLog(pMac, LOGE, "  CSR eSIR_SME_IBSS_NEW_PEER connected BSS is empty");
                        }
                        result = eCSR_ROAM_RESULT_IBSS_CONNECT;
                        roamStatus = eCSR_ROAM_CONNECT_STATUS_UPDATE;
                    }
                    break;
                case eSIR_SME_IBSS_INACTIVE:
                    sessionId = csrFindIbssSession( pMac );
                    if( CSR_SESSION_ID_INVALID != sessionId )
                    {
                        pSession = CSR_GET_SESSION( pMac, sessionId );
                        if(!pSession)
                        {
                            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                            return;
                        }
                        pSession->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
                        result = eCSR_ROAM_RESULT_IBSS_INACTIVE;
                        roamStatus = eCSR_ROAM_CONNECT_STATUS_UPDATE;
                    }
                    break;
                case eSIR_SME_JOINED_NEW_BSS:    // IBSS coalescing.
                    sessionId = csrFindIbssSession( pMac );
                    if( CSR_SESSION_ID_INVALID != sessionId )
                    {
                        pSession = CSR_GET_SESSION( pMac, sessionId );
                        if(!pSession)
                        {
                            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                            return;
                        }
                        // update the connection state information
                        pNewBss = &pStatusChangeMsg->statusChangeInfo.newBssInfo;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                        {
                            vos_log_ibss_pkt_type *pIbssLog;
                            tANI_U32 bi;
                            WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
                            if(pIbssLog)
                            {
                                pIbssLog->eventId = WLAN_IBSS_EVENT_COALESCING;
                                if(pNewBss)
                                {
                                    vos_mem_copy(pIbssLog->bssid, pNewBss->bssId, 6);
                                    if(pNewBss->ssId.length)
                                    {
                                        vos_mem_copy(pIbssLog->ssid, pNewBss->ssId.ssId,
                                                     pNewBss->ssId.length);
                                    }
                                    pIbssLog->operatingChannel = pNewBss->channelNumber;
                                }
                                if(HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac, WNI_CFG_BEACON_INTERVAL, &bi)))
                                {
                                    //***U8 is not enough for beacon interval
                                    pIbssLog->beaconInterval = (v_U8_t)bi;
                                }
                                WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                            }
                        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                        csrRoamUpdateConnectedProfileFromNewBss( pMac, sessionId, pNewBss );

                        if ((eCSR_ENCRYPT_TYPE_NONE ==
                                    pSession->connectedProfile.EncryptionType ))
                        {
                            csrRoamIssueSetContextReq( pMac, sessionId,
                                     pSession->connectedProfile.EncryptionType,
                                     pSession->pConnectBssDesc,
                                     &Broadcastaddr,
                                     FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 );
                        }
                        result = eCSR_ROAM_RESULT_IBSS_COALESCED;
                        roamStatus = eCSR_ROAM_IBSS_IND;
                        vos_mem_copy(&roamInfo.bssid, &pNewBss->bssId,
                                     sizeof(tCsrBssid));
                        pRoamInfo = &roamInfo;
                        /* This BSSID is the real BSSID, let's save it */
                        if(pSession->pConnectBssDesc)
                        {
                            vos_mem_copy(pSession->pConnectBssDesc->bssId,
                                         &pNewBss->bssId, sizeof(tCsrBssid));
                        }
                    }
                    smsLog(pMac, LOGW, "CSR:  eSIR_SME_JOINED_NEW_BSS received from PE");
                    break;
                // detection by LIM that the capabilities of the associated AP have changed.
                case eSIR_SME_AP_CAPS_CHANGED:
                    pApNewCaps = &pStatusChangeMsg->statusChangeInfo.apNewCaps;
                    smsLog(pMac, LOGW, "CSR handling eSIR_SME_AP_CAPS_CHANGED");
                    status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pApNewCaps->bssId, &sessionId );
                    if( HAL_STATUS_SUCCESS( status ) )
                    {
                        if ((eCSR_ROAMING_STATE_JOINED == pMac->roam.curState[sessionId]) &&
                                ((eCSR_ROAM_SUBSTATE_JOINED_REALTIME_TRAFFIC == pMac->roam.curSubState[sessionId]) ||
                                 (eCSR_ROAM_SUBSTATE_NONE == pMac->roam.curSubState[sessionId]) ||
                                 (eCSR_ROAM_SUBSTATE_JOINED_NON_REALTIME_TRAFFIC == pMac->roam.curSubState[sessionId]) ||
                                 (eCSR_ROAM_SUBSTATE_JOINED_NO_TRAFFIC == pMac->roam.curSubState[sessionId]))
                           )
                        {
                            smsLog(pMac, LOGW, "Calling csrRoamDisconnectInternal");
                            csrRoamDisconnectInternal(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
                        }
                        else
                        {
                            smsLog(pMac, LOGW,
                                   FL("Skipping csrScanForCapabilityChange as "
                                   "CSR is in state %s and sub-state %s"),
                                   macTraceGetcsrRoamState(
                                   pMac->roam.curState[sessionId]),
                                   macTraceGetcsrRoamSubState(
                                   pMac->roam.curSubState[sessionId]));
                            /* We ignore the caps change event if CSR is not in full connected state.
                             * Send one event to PE to reset limSentCapsChangeNtf
                             * Once limSentCapsChangeNtf set 0, lim can send sub sequent CAPS change event
                             * otherwise lim cannot send any CAPS change events to SME */
                            csrSendResetApCapsChanged(pMac, &pApNewCaps->bssId);
                        }
                    }
                    break;

                default:
                    roamStatus = eCSR_ROAM_FAILED;
                    result = eCSR_ROAM_RESULT_NONE;
                    break;
            }  // end switch on statusChangeCode
            if(eCSR_ROAM_RESULT_NONE != result)
            {
                csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, roamStatus, result);
            }
            break;
        case eWNI_SME_IBSS_NEW_PEER_IND:
            pIbssPeerInd = (tSmeIbssPeerInd *)pSirMsg;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
            {
                vos_log_ibss_pkt_type *pIbssLog;
                WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
                if(pIbssLog)
                {
                    pIbssLog->eventId = WLAN_IBSS_EVENT_PEER_JOIN;
                    vos_mem_copy(pIbssLog->peerMacAddr, &pIbssPeerInd->peerAddr, 6);
                    WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                }
            }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
            sessionId = csrFindIbssSession( pMac );
            if( CSR_SESSION_ID_INVALID != sessionId )
            {
                pSession = CSR_GET_SESSION( pMac, sessionId );

                if(!pSession)
                {
                    smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                    return;
                }
            // Issue the set Context request to LIM to establish the Unicast STA context for the new peer...
                if(pSession->pConnectBssDesc)
                {
                    vos_mem_copy(&roamInfo.peerMac, pIbssPeerInd->peerAddr,
                                 sizeof(tCsrBssid));
                    vos_mem_copy(&roamInfo.bssid, pSession->pConnectBssDesc->bssId,
                                 sizeof(tCsrBssid));
                    if(pIbssPeerInd->mesgLen > sizeof(tSmeIbssPeerInd))
                    {
                        roamInfo.pbFrames = vos_mem_malloc((pIbssPeerInd->mesgLen
                                                    - sizeof(tSmeIbssPeerInd)));
                        if ( NULL == roamInfo.pbFrames )
                            status = eHAL_STATUS_FAILURE;
                        else
                            status = eHAL_STATUS_SUCCESS;
                        if (HAL_STATUS_SUCCESS(status))
                        {
                            roamInfo.nBeaconLength = (pIbssPeerInd->mesgLen - sizeof(tSmeIbssPeerInd));
                            vos_mem_copy(roamInfo.pbFrames,
                                        ((tANI_U8 *)pIbssPeerInd) + sizeof(tSmeIbssPeerInd),
                                         roamInfo.nBeaconLength);
                        }
                        roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
                        roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
                        roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
                        roamInfo.pBssDesc = vos_mem_malloc(pSession->pConnectBssDesc->length);
                        if ( NULL == roamInfo.pBssDesc )
                            status = eHAL_STATUS_FAILURE;
                        else
                            status = eHAL_STATUS_SUCCESS;
                        if (HAL_STATUS_SUCCESS(status))
                        {
                            vos_mem_copy(roamInfo.pBssDesc,
                                         pSession->pConnectBssDesc,
                                         pSession->pConnectBssDesc->length);
                        }
                        if(HAL_STATUS_SUCCESS(status))
                        {
                            pRoamInfo = &roamInfo;
                        }
                        else
                        {
                            if(roamInfo.pbFrames)
                            {
                                vos_mem_free(roamInfo.pbFrames);
                            }
                            if(roamInfo.pBssDesc)
                            {
                                vos_mem_free(roamInfo.pBssDesc);
                            }
                        }
                    }
                    else
                    {
                        pRoamInfo = &roamInfo;
                    }
                    if ((eCSR_ENCRYPT_TYPE_NONE ==
                              pSession->connectedProfile.EncryptionType ))
                    {
                       csrRoamIssueSetContextReq( pMac, sessionId,
                                     pSession->connectedProfile.EncryptionType,
                                     pSession->pConnectBssDesc,
                                     &(pIbssPeerInd->peerAddr),
                                     FALSE, TRUE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                    }
                }
                else
                {
                    smsLog(pMac, LOGW, "  CSR eSIR_SME_IBSS_NEW_PEER connected BSS is empty");
                }
                //send up the sec type for the new peer
                if (pRoamInfo)
                {
                    pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
                }
                csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0,
                            eCSR_ROAM_CONNECT_STATUS_UPDATE, eCSR_ROAM_RESULT_IBSS_NEW_PEER);
                if(pRoamInfo)
                {
                    if(roamInfo.pbFrames)
                    {
                        vos_mem_free(roamInfo.pbFrames);
                    }
                    if(roamInfo.pBssDesc)
                    {
                        vos_mem_free(roamInfo.pBssDesc);
                    }
                }
            }
            break;
        case eWNI_SME_IBSS_PEER_DEPARTED_IND:
            pIbssPeerInd = (tSmeIbssPeerInd*)pSirMsg;
            sessionId = csrFindIbssSession( pMac );
            if( CSR_SESSION_ID_INVALID != sessionId )
            {
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                {
                    vos_log_ibss_pkt_type *pIbssLog;

                    WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
                    if(pIbssLog)
                    {
                        pIbssLog->eventId = WLAN_IBSS_EVENT_PEER_LEAVE;
                        if(pIbssPeerInd)
                        {
                           vos_mem_copy(pIbssLog->peerMacAddr,
                                        &pIbssPeerInd->peerAddr, 6);
                        }
                        WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                    }
                }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                smsLog(pMac, LOGW, "CSR: Peer departed notification from LIM");
                roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
                roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
                roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
                vos_mem_copy(&roamInfo.peerMac, pIbssPeerInd->peerAddr,
                             sizeof(tCsrBssid));
                csrRoamCallCallback(pMac, sessionId, &roamInfo, 0,
                        eCSR_ROAM_CONNECT_STATUS_UPDATE, eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED);
            }
            break;
        case eWNI_SME_SETCONTEXT_RSP:
            {
                tSirSmeSetContextRsp *pRsp = (tSirSmeSetContextRsp *)pSirMsg;
                tListElem *pEntry;
                tSmeCmd *pCommand;

                pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
                if ( pEntry )
                {
                    pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
                    if ( eSmeCommandSetKey == pCommand->command )
                    {
                        sessionId = pCommand->sessionId;
                        pSession = CSR_GET_SESSION( pMac, sessionId );

                        if(!pSession)
                        {
                            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                            return;
                        }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                        if(eCSR_ENCRYPT_TYPE_NONE != pSession->connectedProfile.EncryptionType)
                        {
                            WLAN_VOS_DIAG_EVENT_DEF(setKeyEvent, vos_event_wlan_security_payload_type);
                            vos_mem_set(&setKeyEvent,
                                        sizeof(vos_event_wlan_security_payload_type), 0);
                            if( pRsp->peerMacAddr[0] & 0x01 )
                            {
                                setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_GTK_RSP;
                            }
                            else
                            {
                                setKeyEvent.eventId = WLAN_SECURITY_EVENT_SET_PTK_RSP;
                            }
                            setKeyEvent.encryptionModeMulticast =
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
                            setKeyEvent.encryptionModeUnicast =
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
                            vos_mem_copy(setKeyEvent.bssid,
                                         pSession->connectedProfile.bssid, 6);
                            setKeyEvent.authMode =
                                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                            if( eSIR_SME_SUCCESS != pRsp->statusCode )
                            {
                                setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
                            }
                            WLAN_VOS_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
                        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId) )
                        {
                            csrRoamStopWaitForKeyTimer( pMac );

                            /* We are done with authentication,
                               whether succeed or not */
                            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
                            //We do it here because this linkup function is not called after association
                            //when a key needs to be set.
                            if( csrIsConnStateConnectedInfra(pMac, sessionId) )
                            {
                                csrRoamLinkUp(pMac, pSession->connectedProfile.bssid);
                            }
                        }
                        if( eSIR_SME_SUCCESS == pRsp->statusCode )
                        {
                            vos_mem_copy(&roamInfo.peerMac,
                                         &pRsp->peerMacAddr, sizeof(tCsrBssid));
                                //Make sure we install the GTK before indicating to HDD as authenticated
                                //This is to prevent broadcast packets go out after PTK and before GTK.
                                if ( vos_mem_compare( &Broadcastaddr, pRsp->peerMacAddr,
                                                     sizeof(tSirMacAddr) ) )
                                {
#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
                                    if(IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE)
                                    {
                                       tpSirSetActiveModeSetBncFilterReq pMsg;
                                       pMsg = vos_mem_malloc(sizeof(tSirSetActiveModeSetBncFilterReq));
                                       pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_SET_BCN_FILTER_REQ);
                                       pMsg->length = pal_cpu_to_be16(sizeof( tANI_U8));
                                       pMsg->seesionId = sessionId;
                                       status = palSendMBMessage(pMac->hHdd, pMsg );
                                    }
#endif
                                       result = eCSR_ROAM_RESULT_AUTHENTICATED;
                                }
                                else
                                {
                                    result = eCSR_ROAM_RESULT_NONE;
                                }
                            pRoamInfo = &roamInfo;
                        }
                        else
                        {
                            result = eCSR_ROAM_RESULT_FAILURE;
                            smsLog(pMac, LOGE, "CSR: Roam Completion setkey "
                                   "command failed(%d) PeerMac "MAC_ADDRESS_STR,
                                   pRsp->statusCode, MAC_ADDR_ARRAY(pRsp->peerMacAddr));
                        }
                        csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.setKeyCmd.roamId,
                                            eCSR_ROAM_SET_KEY_COMPLETE, result);
                        // Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
                        // can go ahead and initiate the TSPEC if any are pending
                        sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_ESE
                        /* Send Adjacent AP report to new AP. */
                        if (result == eCSR_ROAM_RESULT_AUTHENTICATED &&
                            pSession->isPrevApInfoValid &&
                            pSession->connectedProfile.isESEAssoc)
                        {
#ifdef FEATURE_WLAN_ESE_UPLOAD
                            csrSendEseAdjacentApRepInd(pMac, pSession);
#else
                            csrEseSendAdjacentApRepMsg(pMac, pSession);
#endif
                            pSession->isPrevApInfoValid = FALSE;
                        }
#endif
                        if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                        {
                            csrReleaseCommandSetKey( pMac, pCommand );
                        }
                    }
                    else
                    {
                        smsLog( pMac, LOGE, "CSR: Roam Completion called but setkey command is not ACTIVE ..." );
                    }
                }
                else
                {
                    smsLog( pMac, LOGE, "CSR: SetKey Completion called but NO commands are ACTIVE ..." );
                }
                smeProcessPendingQueue( pMac );
            }
            break;
        case eWNI_SME_REMOVEKEY_RSP:
            {
                tSirSmeRemoveKeyRsp *pRsp = (tSirSmeRemoveKeyRsp *)pSirMsg;
                tListElem *pEntry;
                tSmeCmd *pCommand;

                pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
                if ( pEntry )
                {
                    pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
                    if ( eSmeCommandRemoveKey == pCommand->command )
                    {
                        sessionId = pCommand->sessionId;
                        pSession = CSR_GET_SESSION( pMac, sessionId );

                        if(!pSession)
                        {
                            smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                            return;
                        }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                        {
                            WLAN_VOS_DIAG_EVENT_DEF(removeKeyEvent, vos_event_wlan_security_payload_type);
                            vos_mem_set(&removeKeyEvent,
                                        sizeof(vos_event_wlan_security_payload_type), 0);
                            removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_RSP;
                            removeKeyEvent.encryptionModeMulticast =
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
                            removeKeyEvent.encryptionModeUnicast =
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
                            vos_mem_copy( removeKeyEvent.bssid,
                                          pSession->connectedProfile.bssid, 6);
                            removeKeyEvent.authMode =
                                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                            if( eSIR_SME_SUCCESS != pRsp->statusCode )
                            {
                                removeKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
                            }
                            WLAN_VOS_DIAG_EVENT_REPORT(&removeKeyEvent, EVENT_WLAN_SECURITY);
                        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                        if( eSIR_SME_SUCCESS == pRsp->statusCode )
                        {
                            vos_mem_copy(&roamInfo.peerMac, &pRsp->peerMacAddr,
                                         sizeof(tCsrBssid));
                            result = eCSR_ROAM_RESULT_NONE;
                            pRoamInfo = &roamInfo;
                        }
                        else
                        {
                            result = eCSR_ROAM_RESULT_FAILURE;
                        }
                        csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.setKeyCmd.roamId,
                                            eCSR_ROAM_REMOVE_KEY_COMPLETE, result);
                        if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                        {
                            csrReleaseCommandRemoveKey( pMac, pCommand );
                        }
                    }
                    else
                    {
                        smsLog( pMac, LOGW, "CSR: Roam Completion called but setkey command is not ACTIVE ..." );
                    }
                }
                else
                {
                    smsLog( pMac, LOGW, "CSR: SetKey Completion called but NO commands are ACTIVE ..." );
                }
                smeProcessPendingQueue( pMac );
            }
            break;
        case eWNI_SME_GET_STATISTICS_RSP:
            smsLog( pMac, LOG2, FL("Stats rsp from PE"));
            csrRoamStatsRspProcessor( pMac, pSirMsg );
            break;
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
        case eWNI_SME_GET_TSM_STATS_RSP:
            smsLog( pMac, LOG2, FL("TSM Stats rsp from PE"));
            csrTsmStatsRspProcessor( pMac, pSirMsg );
            break;
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
        case eWNI_SME_GET_RSSI_REQ:
            smsLog( pMac, LOG2, FL("GetRssiReq from self"));
            csrUpdateRssi( pMac, pSirMsg );
            break;

        case eWNI_SME_GET_SNR_REQ:
            smsLog( pMac, LOG2, FL("GetSnrReq from self"));
            csrUpdateSnr(pMac, pSirMsg);
            break;

#ifdef WLAN_FEATURE_VOWIFI_11R
        case eWNI_SME_FT_PRE_AUTH_RSP:
            csrRoamFTPreAuthRspProcessor( pMac, (tpSirFTPreAuthRsp)pSirMsg );
            break;
#endif
        case eWNI_SME_MAX_ASSOC_EXCEEDED:
            pSmeMaxAssocInd = (tSmeMaxAssocInd*)pSirMsg;
            smsLog( pMac, LOG1, FL("send indication that max assoc have been reached and the new peer cannot be accepted"));
            sessionId = pSmeMaxAssocInd->sessionId;
            roamInfo.sessionId = sessionId;
            vos_mem_copy(&roamInfo.peerMac, pSmeMaxAssocInd->peerMac,
                         sizeof(tCsrBssid));
            csrRoamCallCallback(pMac, sessionId, &roamInfo, 0,
                    eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
            break;

        case eWNI_SME_BTAMP_LOG_LINK_IND:
            smsLog( pMac, LOG1, FL("Establish logical link req from HCI serialized through MC thread"));
            break;
        case eWNI_SME_RSSI_IND:
            smsLog( pMac, LOG1, FL("RSSI indication from TL serialized through MC thread"));
            csrRoamRssiIndHdlr( pMac, pSirMsg );
        break;
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        case eWNI_SME_CANDIDATE_FOUND_IND:
            smsLog(pMac, LOG2, FL("Candidate found indication from PE"));
            csrNeighborRoamCandidateFoundIndHdlr(pMac, pSirMsg);
        break;
        case eWNI_SME_HANDOFF_REQ:
            smsLog( pMac, LOG2, FL("Handoff Req from self"));
            csrNeighborRoamHandoffReqHdlr( pMac, pSirMsg );
            break;
#endif

        default:
            break;
    }  // end switch on message type
}

void csrCallRoamingCompletionCallback(tpAniSirGlobal pMac, tCsrRoamSession *pSession,
                                      tCsrRoamInfo *pRoamInfo, tANI_U32 roamId, eCsrRoamResult roamResult)
{
   if(pSession)
   {
      if(pSession->bRefAssocStartCnt)
      {
         pSession->bRefAssocStartCnt--;

         if (0 != pSession->bRefAssocStartCnt)
         {
             VOS_ASSERT( pSession->bRefAssocStartCnt == 0);
             return;
         }
         //Need to call association_completion because there is an assoc_start pending.
         csrRoamCallCallback(pMac, pSession->sessionId, NULL, roamId,
                                               eCSR_ROAM_ASSOCIATION_COMPLETION,
                                               eCSR_ROAM_RESULT_FAILURE);
      }
      csrRoamCallCallback(pMac, pSession->sessionId, pRoamInfo, roamId, eCSR_ROAM_ROAMING_COMPLETION, roamResult);
   }
   else
   {
      smsLog(pMac, LOGW, FL("  pSession is NULL"));
   }
}


eHalStatus csrRoamStartRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamingReason roamingReason)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    if(CSR_IS_LOSTLINK_ROAMING(roamingReason) &&
        (eANI_BOOLEAN_FALSE == pMac->roam.roamSession[sessionId].fCancelRoaming))
    {
        status = csrScanRequestLostLink1( pMac, sessionId );
    }
    return(status);
}

//return a boolean to indicate whether roaming completed or continue.
tANI_BOOLEAN csrRoamCompleteRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    tANI_BOOLEAN fForce, eCsrRoamResult roamResult)
{
    tANI_BOOLEAN fCompleted = eANI_BOOLEAN_TRUE;
    tANI_TIMESTAMP roamTime = (tANI_TIMESTAMP)(pMac->roam.configParam.nRoamingTime * PAL_TICKS_PER_SECOND);
    tANI_TIMESTAMP curTime = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eANI_BOOLEAN_FALSE;
    }
    //Check whether time is up
    if(pSession->fCancelRoaming || fForce ||
       ((curTime - pSession->roamingStartTime) > roamTime) ||
       eCsrReassocRoaming == pSession->roamingReason ||
       eCsrDynamicRoaming == pSession->roamingReason)
    {
        smsLog(pMac, LOGW, FL("  indicates roaming completion"));
        if(pSession->fCancelRoaming && CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason))
        {
            /*
             * Roaming is canceled, tell HDD to indicate disconnect
             * Because LIM overload deauth_ind for both deauth frame and
             * missed beacon we need to use this logic to distinguish it.
             * For missed beacon, LIM set reason to be eSIR_BEACON_MISSED
             */
            if(eSIR_BEACON_MISSED == pSession->roamingStatusCode)
            {
                roamResult = eCSR_ROAM_RESULT_LOSTLINK;
            }
            else if(eCsrLostlinkRoamingDisassoc == pSession->roamingReason)
            {
                roamResult = eCSR_ROAM_RESULT_DISASSOC_IND;
            }
            else if(eCsrLostlinkRoamingDeauth == pSession->roamingReason)
            {
                roamResult = eCSR_ROAM_RESULT_DEAUTH_IND;
            }
            else
            {
                roamResult = eCSR_ROAM_RESULT_LOSTLINK;
            }
        }
        csrCallRoamingCompletionCallback(pMac, pSession, NULL, 0, roamResult);
        pSession->roamingReason = eCsrNotRoaming;
    }
    else
    {
        pSession->roamResult = roamResult;
        if(!HAL_STATUS_SUCCESS(csrRoamStartRoamingTimer(pMac, sessionId,
                                                        VOS_TIMER_TO_SEC_UNIT)))
        {
            csrCallRoamingCompletionCallback(pMac, pSession, NULL, 0, roamResult);
            pSession->roamingReason = eCsrNotRoaming;
        }
        else
        {
            fCompleted = eANI_BOOLEAN_FALSE;
        }
    }
    return(fCompleted);
}

void csrRoamCancelRoaming(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if(CSR_IS_ROAMING(pSession))
    {
        smsLog(pMac, LOGW, "Cancel roaming");
        pSession->fCancelRoaming = eANI_BOOLEAN_TRUE;
        if(CSR_IS_ROAM_JOINING(pMac, sessionId) && CSR_IS_ROAM_SUBSTATE_CONFIG(pMac, sessionId))
        {
            //No need to do anything in here because the handler takes care of it
        }
        else
        {
            eCsrRoamResult roamResult = CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason) ?
                                                    eCSR_ROAM_RESULT_LOSTLINK : eCSR_ROAM_RESULT_NONE;
            //Roaming is stopped after here
            csrRoamCompleteRoaming(pMac, sessionId, eANI_BOOLEAN_TRUE, roamResult);
            /* Since CSR may be in lost link roaming situation, abort all
               roaming related activities */
            csrScanAbortMacScan(pMac, sessionId, eCSR_SCAN_ABORT_DEFAULT);
            csrRoamStopRoamingTimer(pMac, sessionId);
        }
    }
}

void csrRoamRoamingTimerHandler(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    tANI_U32 sessionId = pInfo->sessionId;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if(eANI_BOOLEAN_FALSE == pSession->fCancelRoaming)
    {
        if(!HAL_STATUS_SUCCESS(csrRoamStartRoaming(pMac, sessionId, pSession->roamingReason)))
        {
            csrCallRoamingCompletionCallback(pMac, pSession, NULL, 0, pSession->roamResult);
            pSession->roamingReason = eCsrNotRoaming;
        }
    }
}

eHalStatus csrRoamStartRoamingTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval)
{
    eHalStatus status;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found"), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    smsLog(pMac, LOG1, " csrScanStartRoamingTimer");
    pSession->roamingTimerInfo.sessionId = (tANI_U8)sessionId;
    status = vos_timer_start(&pSession->hTimerRoaming,
                             interval/VOS_TIMER_TO_MS_UNIT);

    return (status);
}

eHalStatus csrRoamStopRoamingTimer(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (vos_timer_stop(&pMac->roam.roamSession[sessionId].hTimerRoaming));
}

void csrRoamWaitForKeyTimeOutHandler(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, pInfo->sessionId );
    eHalStatus status = eHAL_STATUS_FAILURE;

    if(pSession == NULL) {
        smsLog(pMac, LOGE, "%s: session not found", __func__);
        return;
    }

    smsLog(pMac, LOGW, FL("WaitForKey timer expired in state=%s sub-state=%s"),
           macTraceGetNeighbourRoamState(
           pMac->roam.neighborRoamInfo[pInfo->sessionId].neighborRoamState),
           macTraceGetcsrRoamSubState(
           pMac->roam.curSubState[pInfo->sessionId]));

    if( CSR_IS_WAIT_FOR_KEY( pMac, pInfo->sessionId ) )
    {
#ifdef FEATURE_WLAN_LFR
        if (csrNeighborRoamIsHandoffInProgress(pMac, pInfo->sessionId))
        {
            /*
             * Enable heartbeat timer when hand-off is in progress
             * and Key Wait timer expired.
             */
            smsLog(pMac, LOG2, "Enabling HB timer after WaitKey expiry"
                    " (nHBCount=%d)",
                    pMac->roam.configParam.HeartbeatThresh24);
            ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD,
                    pMac->roam.configParam.HeartbeatThresh24,
                    NULL, eANI_BOOLEAN_FALSE);
        }
#endif
        smsLog(pMac, LOGE, " SME pre-auth state timeout. ");

        //Change the substate so command queue is unblocked.
        if (CSR_ROAM_SESSION_MAX > pInfo->sessionId)
        {
            csrRoamSubstateChange(pMac, eCSR_ROAM_SUBSTATE_NONE,
                                  pInfo->sessionId);
        }

        if( csrIsConnStateConnectedInfra(pMac, pInfo->sessionId) )
        {
            csrRoamLinkUp(pMac, pSession->connectedProfile.bssid);
            smeProcessPendingQueue(pMac);
            status = sme_AcquireGlobalLock(&pMac->sme);
            if (HAL_STATUS_SUCCESS(status ))
            {
                csrRoamDisconnect(pMac, pInfo->sessionId,
                                  eCSR_DISCONNECT_REASON_UNSPECIFIED);
                sme_ReleaseGlobalLock(&pMac->sme);
            }
        }
        else
        {
            smsLog(pMac, LOGE, "%s: Session id %d is disconnected",
                   __func__, pInfo->sessionId);
        }
    }
}

eHalStatus csrRoamStartWaitForKeyTimer(tpAniSirGlobal pMac, tANI_U32 interval)
{
    eHalStatus status;
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
         &pMac->roam.neighborRoamInfo[pMac->roam.WaitForKeyTimerInfo.sessionId];
#ifdef FEATURE_WLAN_LFR
    if (csrNeighborRoamIsHandoffInProgress(pMac,
           pMac->roam.WaitForKeyTimerInfo.sessionId))
    {
        /* Disable heartbeat timer when hand-off is in progress */
        smsLog(pMac, LOG2, FL("disabling HB timer in state=%s sub-state=%s"),
               macTraceGetNeighbourRoamState(
               pNeighborRoamInfo->neighborRoamState),
               macTraceGetcsrRoamSubState(
               pMac->roam.curSubState[pMac->roam.WaitForKeyTimerInfo.sessionId]
               ));
        ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, 0, NULL, eANI_BOOLEAN_FALSE);
    }
#endif
    smsLog(pMac, LOG1, " csrScanStartWaitForKeyTimer");
    status = vos_timer_start(&pMac->roam.hTimerWaitForKey,
                             interval/VOS_TIMER_TO_MS_UNIT);

    return (status);
}

eHalStatus csrRoamStopWaitForKeyTimer(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
         &pMac->roam.neighborRoamInfo[pMac->roam.WaitForKeyTimerInfo.sessionId];

    smsLog(pMac, LOG2, FL("WaitForKey timer stopped in state=%s sub-state=%s"),
           macTraceGetNeighbourRoamState(
           pNeighborRoamInfo->neighborRoamState),
           macTraceGetcsrRoamSubState(
           pMac->roam.curSubState[pMac->roam.WaitForKeyTimerInfo.sessionId]));
#ifdef FEATURE_WLAN_LFR
    if (csrNeighborRoamIsHandoffInProgress(pMac,
            pMac->roam.WaitForKeyTimerInfo.sessionId))
    {
        /*
         * Enable heartbeat timer when hand-off is in progress
         * and Key Wait timer got stopped for some reason
         */
        smsLog(pMac, LOG2, "Enabling HB timer after WaitKey stop"
                " (nHBCount=%d)",
                pMac->roam.configParam.HeartbeatThresh24);
        ccmCfgSetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD,
            pMac->roam.configParam.HeartbeatThresh24,
            NULL, eANI_BOOLEAN_FALSE);
    }
#endif
    return (vos_timer_stop(&pMac->roam.hTimerWaitForKey));
}

void csrRoamCompletion(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo, tSmeCmd *pCommand,
                        eCsrRoamResult roamResult, tANI_BOOLEAN fSuccess)
{
    eRoamCmdStatus roamStatus = csrGetRoamCompleteStatus(pMac, sessionId);
    tANI_U32 roamId = 0;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if(pCommand)
    {
        roamId = pCommand->u.roamCmd.roamId;
        if (sessionId != pCommand->sessionId)
        {
           VOS_ASSERT( sessionId == pCommand->sessionId );
           return;
        }
    }
    if(eCSR_ROAM_ROAMING_COMPLETION == roamStatus)
    {
        //if success, force roaming completion
        csrRoamCompleteRoaming(pMac, sessionId, fSuccess, roamResult);
    }
    else
    {
        if (pSession->bRefAssocStartCnt != 0)
        {
           VOS_ASSERT(pSession->bRefAssocStartCnt == 0);
           return;
        }
        smsLog(pMac, LOGW, FL("  indicates association completion. roamResult = %d"), roamResult);
        csrRoamCallCallback(pMac, sessionId, pRoamInfo, roamId, roamStatus, roamResult);
    }
}

eHalStatus csrRoamLostLink( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 type, tSirSmeRsp *pSirMsg)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDeauthInd *pDeauthIndMsg = NULL;
    tSirSmeDisassocInd *pDisassocIndMsg = NULL;
    eCsrRoamResult result = eCSR_ROAM_RESULT_LOSTLINK;
    tCsrRoamInfo *pRoamInfo = NULL;
    tCsrRoamInfo roamInfo;
    tANI_BOOLEAN fToRoam;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    //Only need to roam for infra station. In this case P2P client will roam as well
    fToRoam = CSR_IS_INFRASTRUCTURE(&pSession->connectedProfile);
    pSession->fCancelRoaming = eANI_BOOLEAN_FALSE;
    if ( eWNI_SME_DISASSOC_IND == type )
    {
        result = eCSR_ROAM_RESULT_DISASSOC_IND;
        pDisassocIndMsg = (tSirSmeDisassocInd *)pSirMsg;
        pSession->roamingStatusCode = pDisassocIndMsg->statusCode;
        pSession->joinFailStatusCode.reasonCode = pDisassocIndMsg->reasonCode;
    }
    else if ( eWNI_SME_DEAUTH_IND == type )
    {
        result = eCSR_ROAM_RESULT_DEAUTH_IND;
        pDeauthIndMsg = (tSirSmeDeauthInd *)pSirMsg;
        pSession->roamingStatusCode = pDeauthIndMsg->statusCode;
        /* Convert into proper reason code */
        if ((pDeauthIndMsg->reasonCode == eSIR_BEACON_MISSED) ||
            (pDeauthIndMsg->reasonCode ==
                 eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON))
            pSession->joinFailStatusCode.reasonCode = 0;
        else
            pSession->joinFailStatusCode.reasonCode = pDeauthIndMsg->reasonCode;
        /*
         * cfg layer expects 0 as reason code if the driver doesn't know the
         * reason code eSIR_BEACON_MISSED is defined as locally
         */
    }
    else
    {
        smsLog(pMac, LOGW, FL("gets an unknown type (%d)"), type);
        result = eCSR_ROAM_RESULT_NONE;
        pSession->joinFailStatusCode.reasonCode = 1;
    }

    // call profile lost link routine here
    if(!CSR_IS_INFRA_AP(&pSession->connectedProfile))
    {
        csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_LOSTLINK_DETECTED, result);
        /*Move the state to Idle after disconnection*/
        csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId );

    }

    if ( eWNI_SME_DISASSOC_IND == type )
    {
        status = csrSendMBDisassocCnfMsg(pMac, pDisassocIndMsg);
    }
    else if ( eWNI_SME_DEAUTH_IND == type )
    {
        status = csrSendMBDeauthCnfMsg(pMac, pDeauthIndMsg);
    }
    if(!HAL_STATUS_SUCCESS(status))
    {
        //If fail to send confirmation to PE, not to trigger roaming
        fToRoam = eANI_BOOLEAN_FALSE;
    }

    //prepare to tell HDD to disconnect
    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
    roamInfo.statusCode = (tSirResultCodes)pSession->roamingStatusCode;
    roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
    if( eWNI_SME_DISASSOC_IND == type)
    {
        //staMacAddr
        vos_mem_copy(roamInfo.peerMac, pDisassocIndMsg->peerMacAddr,
                     sizeof(tSirMacAddr));
        roamInfo.staId = (tANI_U8)pDisassocIndMsg->staId;
        roamInfo.reasonCode = pDisassocIndMsg->reasonCode;
    }
    else if( eWNI_SME_DEAUTH_IND == type )
    {
        //staMacAddr
        vos_mem_copy(roamInfo.peerMac, pDeauthIndMsg->peerMacAddr,
                     sizeof(tSirMacAddr));
        roamInfo.staId = (tANI_U8)pDeauthIndMsg->staId;
        roamInfo.reasonCode = pDeauthIndMsg->reasonCode;
        roamInfo.rxRssi = pDeauthIndMsg->rssi;
    }
    smsLog(pMac, LOGW, FL("roamInfo.staId (%d)"), roamInfo.staId);

    /* See if we can possibly roam.  If so, start the roaming process and notify HDD
       that we are roaming.  But if we cannot possibly roam, or if we are unable to
       currently roam, then notify HDD of the lost link */
    if(fToRoam)
    {
        //Only remove the connected BSS in infrastructure mode
        csrRoamRemoveConnectedBssFromScanCache(pMac, &pSession->connectedProfile);
        /* Not to do anything for lost link with WDS */
        if( pMac->roam.configParam.nRoamingTime )
        {
            if(HAL_STATUS_SUCCESS(status = csrRoamStartRoaming(pMac, sessionId,
                        ( eWNI_SME_DEAUTH_IND == type ) ?
                        eCsrLostlinkRoamingDeauth : eCsrLostlinkRoamingDisassoc)))
            {
                vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
                //For IBSS, we need to give some more info to HDD
                if(csrIsBssTypeIBSS(pSession->connectedProfile.BSSType))
                {
                    roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                    roamInfo.statusCode = (tSirResultCodes)pSession->roamingStatusCode;
                    roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                }
                else
                {
                   roamInfo.reasonCode = eCsrRoamReasonSmeIssuedForLostLink;
                }
                    pRoamInfo = &roamInfo;
                pSession->roamingReason = ( eWNI_SME_DEAUTH_IND == type ) ?
                        eCsrLostlinkRoamingDeauth : eCsrLostlinkRoamingDisassoc;
                pSession->roamingStartTime = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);
                csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_ROAMING_START, eCSR_ROAM_RESULT_LOSTLINK);
            }
            else
            {
                smsLog(pMac, LOGW, " %s Fail to start roaming, status = %d", __func__, status);
                fToRoam = eANI_BOOLEAN_FALSE;
            }
        }
        else
        {
            /* We are told not to roam, indicate lost link */
            fToRoam = eANI_BOOLEAN_FALSE;
        }
    }
    if(!fToRoam)
    {
        //Tell HDD about the lost link
        if(!CSR_IS_INFRA_AP(&pSession->connectedProfile))
        {
            /* Don't call csrRoamCallCallback for GO/SoftAp case as this indication
             * was already given as part of eWNI_SME_DISASSOC_IND msg handling in
             * csrRoamCheckForLinkStatusChange API.
             */
            csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_LOSTLINK, result);
        }

       /*No need to start idle scan in case of IBSS/SAP
         Still enable idle scan for polling in case concurrent sessions are running */
        if(CSR_IS_INFRASTRUCTURE(&pSession->connectedProfile))
        {
            csrScanStartIdleScan(pMac);
        }
    }

    return (status);
}

eHalStatus csrRoamLostLinkAfterhandoffFailure( tpAniSirGlobal pMac,tANI_U32 sessionId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tListElem *pEntry = NULL;
    tSmeCmd *pCommand = NULL;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    pSession->fCancelRoaming =  eANI_BOOLEAN_FALSE;
    //Only remove the connected BSS in infrastructure mode
    csrRoamRemoveConnectedBssFromScanCache(pMac, &pSession->connectedProfile);
    if(pMac->roam.configParam.nRoamingTime)
    {
       if(HAL_STATUS_SUCCESS(status = csrRoamStartRoaming(pMac,sessionId, pSession->roamingReason)))
       {
          //before starting the lost link logic release the roam command for handoff
          pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
          if(pEntry)
          {
              pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
          }
          if(pCommand)
          {
             if (( eSmeCommandRoam == pCommand->command ) &&
                 ( eCsrSmeIssuedAssocToSimilarAP == pCommand->u.roamCmd.roamReason))
             {
                 if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
                 {
                    csrReleaseCommandRoam( pMac, pCommand );
                 }
             }
          }
          smsLog( pMac, LOGW, "Lost link roaming started ...");
       }
    }
    else
    {
       /* We are told not to roam, indicate lost link */
       status = eHAL_STATUS_FAILURE;
    }

    return (status);
}
void csrRoamWmStatusChangeComplete( tpAniSirGlobal pMac )
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
    if ( pEntry )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        if ( eSmeCommandWmStatusChange == pCommand->command )
        {
            // Nothing to process in a Lost Link completion....  It just kicks off a
            // roaming sequence.
            if( csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK ) )
            {
                csrReleaseCommandWmStatusChange( pMac, pCommand );
            }
            else
            {
                smsLog( pMac, LOGE, " ******csrRoamWmStatusChangeComplete fail to release command");
            }

        }
        else
        {
            smsLog( pMac, LOGW, "CSR: WmStatusChange Completion called but LOST LINK command is not ACTIVE ..." );
        }
    }
    else
    {
        smsLog( pMac, LOGW, "CSR: WmStatusChange Completion called but NO commands are ACTIVE ..." );
    }
    smeProcessPendingQueue( pMac );
}

void csrRoamProcessWmStatusChangeCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirSmeRsp *pSirSmeMsg;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, pCommand->sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), pCommand->sessionId);
        return;
    }
    smsLog(pMac, LOG1, FL("session:%d, CmdType : %d"),
                       pCommand->sessionId,
                       pCommand->u.wmStatusChangeCmd.Type);
    switch ( pCommand->u.wmStatusChangeCmd.Type )
    {
        case eCsrDisassociated:
            pSirSmeMsg = (tSirSmeRsp *)&pCommand->u.wmStatusChangeCmd.u.DisassocIndMsg;
            status = csrRoamLostLink(pMac, pCommand->sessionId, eWNI_SME_DISASSOC_IND, pSirSmeMsg);
            break;
        case eCsrDeauthenticated:
            pSirSmeMsg = (tSirSmeRsp *)&pCommand->u.wmStatusChangeCmd.u.DeauthIndMsg;
            status = csrRoamLostLink(pMac, pCommand->sessionId, eWNI_SME_DEAUTH_IND, pSirSmeMsg);
            break;
        default:
            smsLog(pMac, LOGW, FL("gets an unknown command %d"), pCommand->u.wmStatusChangeCmd.Type);
            break;
    }
    //For WDS, we want to stop BSS as well when it is indicated that it is disconnected.
    if( CSR_IS_CONN_WDS(&pSession->connectedProfile) )
    {
        if( !HAL_STATUS_SUCCESS(csrRoamIssueStopBssCmd( pMac, pCommand->sessionId, eANI_BOOLEAN_TRUE )) )
        {
            //This is not good
            smsLog(pMac, LOGE, FL("  failed to issue stopBSS command"));
        }
    }
    /*
     * Lost Link just triggers a roaming sequence. We can complete the Lost Link
     * command here since there is nothing else to do.
     */
    csrRoamWmStatusChangeComplete( pMac );
}

//This function returns band and mode information.
//The only tricky part is that if phyMode is set to 11abg, this function may return eCSR_CFG_DOT11_MODE_11B
//instead of eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,
                                                     tANI_U8 operationChn, eCsrBand *pBand )
{
    eCsrPhyMode phyModeIn = (eCsrPhyMode)pProfile->phyMode;
    eCsrCfgDot11Mode cfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(pProfile, phyModeIn,
                                            pMac->roam.configParam.ProprietaryRatesEnabled);
    eCsrBand eBand;
    //If the global setting for dot11Mode is set to auto/abg, we overwrite the setting in the profile.
    if( ((!CSR_IS_INFRA_AP(pProfile )&& !CSR_IS_WDS(pProfile )) &&
         ((eCSR_CFG_DOT11_MODE_AUTO == pMac->roam.configParam.uCfgDot11Mode) ||
         (eCSR_CFG_DOT11_MODE_ABG == pMac->roam.configParam.uCfgDot11Mode))) ||
        (eCSR_CFG_DOT11_MODE_AUTO == cfgDot11Mode) || (eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) )
    {
        switch( pMac->roam.configParam.uCfgDot11Mode )
        {
            case eCSR_CFG_DOT11_MODE_11A:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                eBand = eCSR_BAND_5G;
                break;
            case eCSR_CFG_DOT11_MODE_11B:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                eBand = eCSR_BAND_24;
                break;
            case eCSR_CFG_DOT11_MODE_11G:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
                eBand = eCSR_BAND_24;
                break;
            case eCSR_CFG_DOT11_MODE_11N:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ? eCSR_BAND_24 : eCSR_BAND_5G;
                break;
#ifdef WLAN_FEATURE_11AC
            case eCSR_CFG_DOT11_MODE_11AC:
                if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
                {
                    /* If the operating channel is in 2.4 GHz band, check for
                     * INI item to disable VHT operation in 2.4 GHz band
                     */
                    if (CSR_IS_CHANNEL_24GHZ(operationChn) &&
                        !pMac->roam.configParam.enableVhtFor24GHz)
                    {
                       /* Disable 11AC operation */
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                    }
                    else
                    {
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
                    }
                    eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ? eCSR_BAND_24 : eCSR_BAND_5G;
                }
                else
                {
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                    eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ? eCSR_BAND_24 : eCSR_BAND_5G;
                }
                break;
            case eCSR_CFG_DOT11_MODE_11AC_ONLY:
                if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
                {
                    /* If the operating channel is in 2.4 GHz band, check for
                     * INI item to disable VHT operation in 2.4 GHz band
                     */
                    if (CSR_IS_CHANNEL_24GHZ(operationChn) &&
                        !pMac->roam.configParam.enableVhtFor24GHz)
                    {
                       /* Disable 11AC operation */
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                    }
                    else
                    {
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
                    }
                    eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ? eCSR_BAND_24 : eCSR_BAND_5G;
                }
                else
                {
                    eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ? eCSR_BAND_24 : eCSR_BAND_5G;
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                }
                break;
#endif
            case eCSR_CFG_DOT11_MODE_AUTO:
#ifdef WLAN_FEATURE_11AC
                if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
                {
                    /* If the operating channel is in 2.4 GHz band, check for
                     * INI item to disable VHT operation in 2.4 GHz band
                     */
                    if (CSR_IS_CHANNEL_24GHZ(operationChn) &&
                         !pMac->roam.configParam.enableVhtFor24GHz)
                    {
                       /* Disable 11AC operation */
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                    }
                    else
                    {
                       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
                    }
                    eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ?
                                           eCSR_BAND_24 : eCSR_BAND_5G;
                }
                else
                {
                        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                        eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ?
                                           eCSR_BAND_24 : eCSR_BAND_5G;
                }
#else
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                eBand = CSR_IS_CHANNEL_24GHZ(operationChn) ?
                                           eCSR_BAND_24 : eCSR_BAND_5G;
#endif
                break;
            default:
                // Global dot11 Mode setting is 11a/b/g.
                // use the channel number to determine the Mode setting.
                if ( eCSR_OPERATING_CHANNEL_AUTO == operationChn )
                {
                    eBand = pMac->roam.configParam.eBand;
                    if(eCSR_BAND_24 == eBand)
                    {
                        //See reason in else if ( CSR_IS_CHANNEL_24GHZ(operationChn) ) to pick 11B
                        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                    }
                    else
                    {
                        //prefer 5GHz
                        eBand = eCSR_BAND_5G;
                        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                    }
                }
                else if ( CSR_IS_CHANNEL_24GHZ(operationChn) )
                {
                    // WiFi tests require IBSS networks to start in 11b mode
                    // without any change to the default parameter settings
                    // on the adapter.  We use ACU to start an IBSS through
                    // creation of a startIBSS profile. This startIBSS profile
                    // has Auto MACProtocol and the adapter property setting
                    // for dot11Mode is also AUTO.   So in this case, let's
                    // start the IBSS network in 11b mode instead of 11g mode.
                    // So this is for Auto=profile->MacProtocol && Auto=Global.
                    // dot11Mode && profile->channel is < 14, then start the IBSS
                    // in b mode.
                    //
                    // Note:  we used to have this start as an 11g IBSS for best
                    // performance... now to specify that the user will have to
                    // set the do11Mode in the property page to 11g to force it.
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                    eBand = eCSR_BAND_24;
                }
                else
                {
                    // else, it's a 5.0GHz channel.  Set mode to 11a.
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                    eBand = eCSR_BAND_5G;
                }
                break;
        }//switch
    }//if( eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode )
    else
    {
            //dot11 mode is set, lets pick the band
            if ( eCSR_OPERATING_CHANNEL_AUTO == operationChn )
            {
                // channel is Auto also.
                eBand = pMac->roam.configParam.eBand;
                if(eCSR_BAND_ALL == eBand)
                {
                    //prefer 5GHz
                    eBand = eCSR_BAND_5G;
                }
            }
            else if ( CSR_IS_CHANNEL_24GHZ(operationChn) )
            {
                eBand = eCSR_BAND_24;
            }
            else
            {
                eBand = eCSR_BAND_5G;
            }
    }
    if(pBand)
    {
        *pBand = eBand;
    }

   if (operationChn == 14){
     smsLog(pMac, LOGE, FL("  Switching to Dot11B mode "));
     cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
   }

    /* Incase of WEP Security encryption type is coming as part of add key.
       So while Start BSS dont have information */
    if ((!CSR_IS_11n_ALLOWED(pProfile->EncryptionType.encryptionType[0]) ||
        ((pProfile->privacy == 1) &&
        (pProfile->EncryptionType.encryptionType[0] ==
                                       eCSR_ENCRYPT_TYPE_NONE))) &&
        ((eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode) ||
#ifdef WLAN_FEATURE_11AC
        (eCSR_CFG_DOT11_MODE_11AC == cfgDot11Mode)
#endif
        )) {
        /* We cannot do 11n here */
        if (CSR_IS_CHANNEL_24GHZ(operationChn)) {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
        } else {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
        }
    }
    return( cfgDot11Mode );
}

eHalStatus csrRoamIssueStopBss( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamSubState NewSubstate )
{
    eHalStatus status;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    {
        vos_log_ibss_pkt_type *pIbssLog;
        WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
        if(pIbssLog)
        {
            pIbssLog->eventId = WLAN_IBSS_EVENT_STOP_REQ;
            WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
        }
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    // Set the roaming substate to 'stop Bss request'...
    csrRoamSubstateChange( pMac, NewSubstate, sessionId );

    // attempt to stop the Bss (reason code is ignored...)
    status = csrSendMBStopBssReqMsg( pMac, sessionId );
    if(!HAL_STATUS_SUCCESS(status))
    {
        smsLog(pMac, LOGW, FL("csrSendMBStopBssReqMsg failed with status %d"), status);
    }
    return (status);
}

//pNumChan is a caller allocated space with the sizeof pChannels
eHalStatus csrGetCfgValidChannels(tpAniSirGlobal pMac, tANI_U8 *pChannels, tANI_U32 *pNumChan)
{

    return (ccmCfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                  (tANI_U8 *)pChannels,
                  pNumChan));
}

tPowerdBm csrGetCfgMaxTxPower (tpAniSirGlobal pMac, tANI_U8 channel)
{
    tANI_U32    cfgLength = 0;
    tANI_U16    cfgId = 0;
    tPowerdBm   maxTxPwr = 0;
    tANI_U8     *pCountryInfo = NULL;
    eHalStatus  status;
    tANI_U8     count = 0;
    tANI_U8     firstChannel;
    tANI_U8     maxChannels;

    if (CSR_IS_CHANNEL_5GHZ(channel))
    {
        cfgId = WNI_CFG_MAX_TX_POWER_5;
        cfgLength = WNI_CFG_MAX_TX_POWER_5_LEN;
    }
    else if (CSR_IS_CHANNEL_24GHZ(channel))
    {
        cfgId = WNI_CFG_MAX_TX_POWER_2_4;
        cfgLength = WNI_CFG_MAX_TX_POWER_2_4_LEN;
    }
    else
        return  maxTxPwr;

    pCountryInfo = vos_mem_malloc(cfgLength);
    if ( NULL == pCountryInfo )
        status = eHAL_STATUS_FAILURE;
    else
        status = eHAL_STATUS_SUCCESS;
    if (status != eHAL_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                 FL("%s: failed to allocate memory, status = %d"),
              __FUNCTION__, status);
        goto error;
    }
    status = ccmCfgGetStr(pMac, cfgId, (tANI_U8 *)pCountryInfo, &cfgLength);
    if (status != eHAL_STATUS_SUCCESS)
    {
        goto error;
    }
    /* Identify the channel and max txpower */
    while (count <= (cfgLength - (sizeof(tSirMacChanInfo))))
    {
        firstChannel = pCountryInfo[count++];
        maxChannels = pCountryInfo[count++];
        maxTxPwr = pCountryInfo[count++];

        if ((channel >= firstChannel) &&
            (channel < (firstChannel + maxChannels)))
        {
            break;
        }
    }

error:
    if (NULL != pCountryInfo)
        vos_mem_free(pCountryInfo);

    return maxTxPwr;
}


tANI_BOOLEAN csrRoamIsChannelValid( tpAniSirGlobal pMac, tANI_U8 channel )
{
    tANI_BOOLEAN fValid = FALSE;
    tANI_U32 idxValidChannels;
    tANI_U32 len = sizeof(pMac->roam.validChannelList);

    if (HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, pMac->roam.validChannelList, &len)))
    {
        for ( idxValidChannels = 0; ( idxValidChannels < len ); idxValidChannels++ )
        {
            if ( channel == pMac->roam.validChannelList[ idxValidChannels ] )
            {
                fValid = TRUE;
                break;
            }
        }
    }
    pMac->roam.numValidChannels = len;
    return fValid;
}

tANI_BOOLEAN csrRoamIsValid40MhzChannel(tpAniSirGlobal pMac, tANI_U8 channel)
{
    tANI_BOOLEAN fValid = eANI_BOOLEAN_FALSE;
    tANI_U8 i;
    for(i = 0; i < pMac->scan.base40MHzChannels.numChannels; i++)
    {
        if(channel == pMac->scan.base40MHzChannels.channelList[i])
        {
            fValid = eANI_BOOLEAN_TRUE;
            break;
        }
    }
    return (fValid);
}

//This function check and validate whether the NIC can do CB (40MHz)
 static ePhyChanBondState csrGetCBModeFromIes(tpAniSirGlobal pMac, tANI_U8 primaryChn, tDot11fBeaconIEs *pIes)
{
    ePhyChanBondState eRet = PHY_SINGLE_CHANNEL_CENTERED;
    tANI_U8 centerChn;
    tANI_U32 ChannelBondingMode;
    if(CSR_IS_CHANNEL_24GHZ(primaryChn))
    {
         /* 'gChannelBondingMode24GHz' configuration item is common for
          * SAP and STA mode and currently MDM does not support
          * HT40 in 2.4Ghz STA mode.
          * So disabling the HT40 in 2.4GHz station mode */
#ifdef QCA_HT_20_24G_STA_ONLY
        ChannelBondingMode = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
#else
        ChannelBondingMode = pMac->roam.configParam.channelBondingMode24GHz;
#endif
    }
    else
    {
        ChannelBondingMode = pMac->roam.configParam.stacbmode;
    }
    //Figure what the other side's CB mode
    if(WNI_CFG_CHANNEL_BONDING_MODE_DISABLE != ChannelBondingMode)
    {
        if(pIes->HTCaps.present && (eHT_CHANNEL_WIDTH_40MHZ ==
                              pIes->HTCaps.supportedChannelWidthSet))
        {
           /* In Case WPA2 and TKIP is the only one cipher suite in Pairwise */
            if ((pIes->RSN.present &&
                 (pIes->RSN.pwise_cipher_suite_count == 1) &&
                 !memcmp(&(pIes->RSN.pwise_cipher_suites[0][0]),
                                        "\x00\x0f\xac\x02" ,4))
                /* In Case only WPA1 is supported and TKIP is the
                 * only one cipher suite in Unicast.
                 */
               ||(!pIes->RSN.present &&
                  (pIes->WPA.present && (pIes->WPA.unicast_cipher_count == 1)
                     && !memcmp(&(pIes->WPA.unicast_ciphers[0][0]),
                                  "\x00\x50\xf2\x02", 4)))) {
                smsLog(pMac, LOGW, " No channel bonding in TKIP mode ");
                eRet = PHY_SINGLE_CHANNEL_CENTERED;
            }

            else if(pIes->HTInfo.present)
            {
                /* This is called during INFRA STA/CLIENT and should use the merged value of
                 * supported channel width and recommended tx width as per standard
                 */
                smsLog(pMac, LOG1, "scws %u rtws %u sco %u",
                    pIes->HTCaps.supportedChannelWidthSet,
                    pIes->HTInfo.recommendedTxWidthSet,
                    pIes->HTInfo.secondaryChannelOffset);

                if (pIes->HTInfo.recommendedTxWidthSet == eHT_CHANNEL_WIDTH_40MHZ)
                    eRet = (ePhyChanBondState)pIes->HTInfo.secondaryChannelOffset;
                else
                    eRet = PHY_SINGLE_CHANNEL_CENTERED;
                switch (eRet) {
                    case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
                        centerChn = primaryChn + CSR_CB_CENTER_CHANNEL_OFFSET;
                        break;
                    case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
                        centerChn = primaryChn - CSR_CB_CENTER_CHANNEL_OFFSET;
                        break;
                    case PHY_SINGLE_CHANNEL_CENTERED:
                    default:
                        centerChn = primaryChn;
                        break;
                }
                if((PHY_SINGLE_CHANNEL_CENTERED != eRet) && !csrRoamIsValid40MhzChannel(pMac, centerChn))
                {
                    smsLog(pMac, LOGE, "  Invalid center channel (%d), disable 40MHz mode", centerChn);
                    eRet = PHY_SINGLE_CHANNEL_CENTERED;
                }
            }
        }
    }
    return eRet;
}
tANI_BOOLEAN csrIsEncryptionInList( tpAniSirGlobal pMac, tCsrEncryptionList *pCipherList, eCsrEncryptionType encryptionType )
{
    tANI_BOOLEAN fFound = FALSE;
    tANI_U32 idx;
    for( idx = 0; idx < pCipherList->numEntries; idx++ )
    {
        if( pCipherList->encryptionType[idx] == encryptionType )
        {
            fFound = TRUE;
            break;
        }
    }
    return fFound;
}
tANI_BOOLEAN csrIsAuthInList( tpAniSirGlobal pMac, tCsrAuthList *pAuthList, eCsrAuthType authType )
{
    tANI_BOOLEAN fFound = FALSE;
    tANI_U32 idx;
    for( idx = 0; idx < pAuthList->numEntries; idx++ )
    {
        if( pAuthList->authType[idx] == authType )
        {
            fFound = TRUE;
            break;
        }
    }
    return fFound;
}
tANI_BOOLEAN csrIsSameProfile(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pProfile1, tCsrRoamProfile *pProfile2)
{
    tANI_BOOLEAN fCheck = eANI_BOOLEAN_FALSE;
    tCsrScanResultFilter *pScanFilter = NULL;
    eHalStatus status = eHAL_STATUS_SUCCESS;

    if(pProfile1 && pProfile2)
    {
        pScanFilter = vos_mem_malloc(sizeof(tCsrScanResultFilter));
        if ( NULL == pScanFilter )
           status = eHAL_STATUS_FAILURE;
        else
           status = eHAL_STATUS_SUCCESS;
        if(HAL_STATUS_SUCCESS(status))
        {
            vos_mem_set(pScanFilter, sizeof(tCsrScanResultFilter), 0);
            status = csrRoamPrepareFilterFromProfile(pMac, pProfile2, pScanFilter);
            if(HAL_STATUS_SUCCESS(status))
            {
                fCheck = eANI_BOOLEAN_FALSE;
                do
                {
                    tANI_U32 i;
                    for(i = 0; i < pScanFilter->SSIDs.numOfSSIDs; i++)
                    {
                        fCheck = csrIsSsidMatch( pMac, pScanFilter->SSIDs.SSIDList[i].SSID.ssId,
                                                pScanFilter->SSIDs.SSIDList[i].SSID.length,
                                                pProfile1->SSID.ssId, pProfile1->SSID.length, eANI_BOOLEAN_FALSE );
                        if ( fCheck ) break;
                    }
                    if(!fCheck)
                    {
                        break;
                    }
                    if( !csrIsAuthInList( pMac, &pProfile2->AuthType, pProfile1->AuthType)
                        || pProfile2->BSSType != pProfile1->BSSType
                        || !csrIsEncryptionInList( pMac, &pProfile2->EncryptionType, pProfile1->EncryptionType )
                        )
                    {
                        fCheck = eANI_BOOLEAN_FALSE;
                        break;
                    }
#ifdef WLAN_FEATURE_VOWIFI_11R
                    if (pProfile1->MDID.mdiePresent || pProfile2->MDID.mdiePresent)
                    {
                        if (pProfile1->MDID.mobilityDomain != pProfile2->MDID.mobilityDomain)
                        {
                            fCheck = eANI_BOOLEAN_FALSE;
                            break;
                        }
                    }
#endif
                    //Match found
                    fCheck = eANI_BOOLEAN_TRUE;
                }while(0);
                csrFreeScanFilter(pMac, pScanFilter);
            }
           vos_mem_free(pScanFilter);
        }
    }

    return (fCheck);
}

tANI_BOOLEAN csrRoamIsSameProfileKeys(tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pConnProfile, tCsrRoamProfile *pProfile2)
{
    tANI_BOOLEAN fCheck = eANI_BOOLEAN_FALSE;
    int i;
    do
    {
        //Only check for static WEP
        if(!csrIsEncryptionInList(pMac, &pProfile2->EncryptionType, eCSR_ENCRYPT_TYPE_WEP40_STATICKEY) &&
            !csrIsEncryptionInList(pMac, &pProfile2->EncryptionType, eCSR_ENCRYPT_TYPE_WEP104_STATICKEY))
        {
            fCheck = eANI_BOOLEAN_TRUE;
            break;
        }
        if(!csrIsEncryptionInList(pMac, &pProfile2->EncryptionType, pConnProfile->EncryptionType)) break;
        if(pConnProfile->Keys.defaultIndex != pProfile2->Keys.defaultIndex) break;
        for(i = 0; i < CSR_MAX_NUM_KEY; i++)
        {
            if(pConnProfile->Keys.KeyLength[i] != pProfile2->Keys.KeyLength[i]) break;
            if (!vos_mem_compare(&pConnProfile->Keys.KeyMaterial[i],
                                &pProfile2->Keys.KeyMaterial[i], pProfile2->Keys.KeyLength[i]))
            {
                break;
            }
        }
        if( i == CSR_MAX_NUM_KEY)
        {
            fCheck = eANI_BOOLEAN_TRUE;
        }
    }while(0);
    return (fCheck);
}

//IBSS

tANI_U8 csrRoamGetIbssStartChannelNumber50( tpAniSirGlobal pMac )
{
    tANI_U8 channel = 0;
    tANI_U32 idx;
    tANI_U32 idxValidChannels;
    tANI_BOOLEAN fFound = FALSE;
    tANI_U32 len = sizeof(pMac->roam.validChannelList);

    if(eCSR_OPERATING_CHANNEL_ANY != pMac->roam.configParam.AdHocChannel5G)
    {
        channel = pMac->roam.configParam.AdHocChannel5G;
        if(!csrRoamIsChannelValid(pMac, channel))
        {
            channel = 0;
        }
    }
    if (0 == channel && HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, (tANI_U8 *)pMac->roam.validChannelList, &len)))
    {
        for ( idx = 0; ( idx < CSR_NUM_IBSS_START_CHANNELS_50 ) && !fFound; idx++ )
        {
            for ( idxValidChannels = 0; ( idxValidChannels < len ) && !fFound; idxValidChannels++ )
            {
                if ( csrStartIbssChannels50[ idx ] == pMac->roam.validChannelList[ idxValidChannels ] )
                {
                    fFound = TRUE;
                    channel = csrStartIbssChannels50[ idx ];
                }
            }
        }
        // this is rare, but if it does happen, we find anyone in 11a bandwidth and return the first 11a channel found!
        if (!fFound)
        {
            for ( idxValidChannels = 0; idxValidChannels < len ; idxValidChannels++ )
            {
                if ( CSR_IS_CHANNEL_5GHZ(pMac->roam.validChannelList[ idxValidChannels ]) )   // the max channel# in 11g is 14
                {
                    if (idxValidChannels < CSR_NUM_IBSS_START_CHANNELS_50)
                    {
                        channel = pMac->roam.validChannelList[idxValidChannels];
                    }
                    break;
                }
            }
        }
    }//if

    return( channel );
}

tANI_U8 csrRoamGetIbssStartChannelNumber24( tpAniSirGlobal pMac )
{
    tANI_U8 channel = 1;
    tANI_U32 idx;
    tANI_U32 idxValidChannels;
    tANI_BOOLEAN fFound = FALSE;
    tANI_U32 len = sizeof(pMac->roam.validChannelList);

    if(eCSR_OPERATING_CHANNEL_ANY != pMac->roam.configParam.AdHocChannel24)
    {
        channel = pMac->roam.configParam.AdHocChannel24;
        if(!csrRoamIsChannelValid(pMac, channel))
        {
            channel = 0;
        }
    }

    if (0 == channel && HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, (tANI_U8 *)pMac->roam.validChannelList, &len)))
    {
        for ( idx = 0; ( idx < CSR_NUM_IBSS_START_CHANNELS_24 ) && !fFound; idx++ )
        {
            for ( idxValidChannels = 0; ( idxValidChannels < len ) && !fFound; idxValidChannels++ )
            {
                if ( csrStartIbssChannels24[ idx ] == pMac->roam.validChannelList[ idxValidChannels ] )
                {
                    fFound = TRUE;
                    channel = csrStartIbssChannels24[ idx ];
                }
            }
        }
    }

    return( channel );
}

static void csrRoamGetBssStartParms( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,
                                      tCsrRoamStartBssParams *pParam )
{
    eCsrBand eBand;
    tANI_U8 channel = 0;
    tSirNwType nwType;
    tANI_U8 operationChannel = 0;

    if(pProfile->ChannelInfo.numOfChannels && pProfile->ChannelInfo.ChannelList)
    {
       operationChannel = pProfile->ChannelInfo.ChannelList[0];
    }

    pParam->uCfgDot11Mode =
        csrRoamGetPhyModeBandForBss( pMac, pProfile, operationChannel, &eBand );

    if( ( (pProfile->csrPersona == VOS_P2P_CLIENT_MODE) ||
          (pProfile->csrPersona == VOS_P2P_GO_MODE) )
     && ( pParam->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11B)
      )
    {
        /* This should never happen */
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL,
              FL("For P2PClient/P2P-GO (persona %d) cfgDot11Mode is 11B"),
              pProfile->csrPersona);
        VOS_ASSERT(0);
    }

    switch( pParam->uCfgDot11Mode )
    {
        case eCSR_CFG_DOT11_MODE_11G:
            nwType = eSIR_11G_NW_TYPE;
            break;
        case eCSR_CFG_DOT11_MODE_11B:
            nwType = eSIR_11B_NW_TYPE;
            break;
        case eCSR_CFG_DOT11_MODE_11A:
            nwType = eSIR_11A_NW_TYPE;
            break;
        default:
        case eCSR_CFG_DOT11_MODE_11N:
            /* Because LIM only verifies it against 11a, 11b or 11g,
               set only 11g or 11a here */
            if (eCSR_BAND_24 == eBand) {
                nwType = eSIR_11G_NW_TYPE;
            } else {
                nwType = eSIR_11A_NW_TYPE;
            }
            break;
    }

    pParam->extendedRateSet.numRates = 0;

    switch ( nwType )
    {
        default:
            smsLog(pMac, LOGE, FL("sees an unknown pSirNwType (%d)"), nwType);
        case eSIR_11A_NW_TYPE:

            pParam->operationalRateSet.numRates = 8;

            pParam->operationalRateSet.rate[0] = SIR_MAC_RATE_6 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[1] = SIR_MAC_RATE_9;
            pParam->operationalRateSet.rate[2] = SIR_MAC_RATE_12 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[3] = SIR_MAC_RATE_18;
            pParam->operationalRateSet.rate[4] = SIR_MAC_RATE_24 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[5] = SIR_MAC_RATE_36;
            pParam->operationalRateSet.rate[6] = SIR_MAC_RATE_48;
            pParam->operationalRateSet.rate[7] = SIR_MAC_RATE_54;

            if ( eCSR_OPERATING_CHANNEL_ANY == operationChannel )
            {
                channel = csrRoamGetIbssStartChannelNumber50( pMac );
                if( 0 == channel &&
                    CSR_IS_PHY_MODE_DUAL_BAND(pProfile->phyMode) &&
                    CSR_IS_PHY_MODE_DUAL_BAND(pMac->roam.configParam.phyMode)
                    )
                {
                    //We could not find a 5G channel by auto pick, let's try 2.4G channels
                    //We only do this here because csrRoamGetPhyModeBandForBss always picks 11a for AUTO
                    nwType = eSIR_11B_NW_TYPE;
                    channel = csrRoamGetIbssStartChannelNumber24( pMac );
                   pParam->operationalRateSet.numRates = 4;
                   pParam->operationalRateSet.rate[0] = SIR_MAC_RATE_1 | CSR_DOT11_BASIC_RATE_MASK;
                   pParam->operationalRateSet.rate[1] = SIR_MAC_RATE_2 | CSR_DOT11_BASIC_RATE_MASK;
                   pParam->operationalRateSet.rate[2] = SIR_MAC_RATE_5_5 | CSR_DOT11_BASIC_RATE_MASK;
                   pParam->operationalRateSet.rate[3] = SIR_MAC_RATE_11 | CSR_DOT11_BASIC_RATE_MASK;
                }
            }
            else
            {
                channel = operationChannel;
            }
            break;

        case eSIR_11B_NW_TYPE:
            pParam->operationalRateSet.numRates = 4;
            pParam->operationalRateSet.rate[0] = SIR_MAC_RATE_1 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[1] = SIR_MAC_RATE_2 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[2] = SIR_MAC_RATE_5_5 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[3] = SIR_MAC_RATE_11 | CSR_DOT11_BASIC_RATE_MASK;
            if ( eCSR_OPERATING_CHANNEL_ANY == operationChannel )
            {
                channel = csrRoamGetIbssStartChannelNumber24( pMac );
            }
            else
            {
                channel = operationChannel;
            }

            break;
        case eSIR_11G_NW_TYPE:
            /* For P2P Client and P2P GO, disable 11b rates */
            if( (pProfile->csrPersona == VOS_P2P_CLIENT_MODE) ||
                (pProfile->csrPersona == VOS_P2P_GO_MODE) ||
                (eCSR_CFG_DOT11_MODE_11G_ONLY == pParam->uCfgDot11Mode)
              )
            {
                pParam->operationalRateSet.numRates = 8;

                pParam->operationalRateSet.rate[0] = SIR_MAC_RATE_6 | CSR_DOT11_BASIC_RATE_MASK;
                pParam->operationalRateSet.rate[1] = SIR_MAC_RATE_9;
                pParam->operationalRateSet.rate[2] = SIR_MAC_RATE_12 | CSR_DOT11_BASIC_RATE_MASK;
                pParam->operationalRateSet.rate[3] = SIR_MAC_RATE_18;
                pParam->operationalRateSet.rate[4] = SIR_MAC_RATE_24 | CSR_DOT11_BASIC_RATE_MASK;
                pParam->operationalRateSet.rate[5] = SIR_MAC_RATE_36;
                pParam->operationalRateSet.rate[6] = SIR_MAC_RATE_48;
                pParam->operationalRateSet.rate[7] = SIR_MAC_RATE_54;
            }
            else
            {
            pParam->operationalRateSet.numRates = 4;
            pParam->operationalRateSet.rate[0] = SIR_MAC_RATE_1 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[1] = SIR_MAC_RATE_2 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[2] = SIR_MAC_RATE_5_5 | CSR_DOT11_BASIC_RATE_MASK;
            pParam->operationalRateSet.rate[3] = SIR_MAC_RATE_11 | CSR_DOT11_BASIC_RATE_MASK;

            pParam->extendedRateSet.numRates = 8;
                        pParam->extendedRateSet.rate[0] = SIR_MAC_RATE_6;
            pParam->extendedRateSet.rate[1] = SIR_MAC_RATE_9;
            pParam->extendedRateSet.rate[2] = SIR_MAC_RATE_12;
            pParam->extendedRateSet.rate[3] = SIR_MAC_RATE_18;
            pParam->extendedRateSet.rate[4] = SIR_MAC_RATE_24;
            pParam->extendedRateSet.rate[5] = SIR_MAC_RATE_36;
            pParam->extendedRateSet.rate[6] = SIR_MAC_RATE_48;
            pParam->extendedRateSet.rate[7] = SIR_MAC_RATE_54;
            }

            if ( eCSR_OPERATING_CHANNEL_ANY == operationChannel )
            {
                channel = csrRoamGetIbssStartChannelNumber24( pMac );
            }
            else
            {
                channel = operationChannel;
            }

            break;
    }
    pParam->operationChn = channel;
    pParam->sirNwType = nwType;
    pParam->vht_channel_width = pProfile->vht_channel_width;
}

static void csrRoamGetBssStartParmsFromBssDesc( tpAniSirGlobal pMac, tSirBssDescription *pBssDesc,
                                                 tDot11fBeaconIEs *pIes, tCsrRoamStartBssParams *pParam )
{

    if( pParam )
    {
        pParam->sirNwType = pBssDesc->nwType;
        pParam->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
        pParam->operationChn = pBssDesc->channelId;
        vos_mem_copy(&pParam->bssid, pBssDesc->bssId, sizeof(tCsrBssid));

        if( pIes )
        {
            if(pIes->SuppRates.present)
            {
                pParam->operationalRateSet.numRates = pIes->SuppRates.num_rates;
                if(pIes->SuppRates.num_rates > SIR_MAC_RATESET_EID_MAX)
                {
                    smsLog(pMac, LOGE, FL("num_rates :%d is more than SIR_MAC_RATESET_EID_MAX, resetting to SIR_MAC_RATESET_EID_MAX"),
                      pIes->SuppRates.num_rates);
                    pIes->SuppRates.num_rates = SIR_MAC_RATESET_EID_MAX;
                }
                vos_mem_copy(pParam->operationalRateSet.rate, pIes->SuppRates.rates,
                             sizeof(*pIes->SuppRates.rates) * pIes->SuppRates.num_rates);
            }
            if (pIes->ExtSuppRates.present)
            {
                pParam->extendedRateSet.numRates = pIes->ExtSuppRates.num_rates;
                if(pIes->ExtSuppRates.num_rates > SIR_MAC_RATESET_EID_MAX)
                {
                   smsLog(pMac, LOGE,
                          FL("num_rates :%d is more than SIR_MAC_RATESET_EID_MAX, resetting to SIR_MAC_RATESET_EID_MAX"),
                                         pIes->ExtSuppRates.num_rates);
                   pIes->ExtSuppRates.num_rates = SIR_MAC_RATESET_EID_MAX;
                }
                vos_mem_copy(pParam->extendedRateSet.rate,
                             pIes->ExtSuppRates.rates,
                             sizeof(*pIes->ExtSuppRates.rates) * pIes->ExtSuppRates.num_rates);
            }
            if( pIes->SSID.present )
            {
                pParam->ssId.length = pIes->SSID.num_ssid;
                vos_mem_copy(pParam->ssId.ssId, pIes->SSID.ssid,
                             pParam->ssId.length);
            }
            pParam->cbMode = csrGetCBModeFromIes(pMac, pParam->operationChn, pIes);
        }
        else
        {
            pParam->ssId.length = 0;
           pParam->operationalRateSet.numRates = 0;
        }
    }
}

static void csrRoamDetermineMaxRateForAdHoc( tpAniSirGlobal pMac, tSirMacRateSet *pSirRateSet )
{
    tANI_U8 MaxRate = 0;
    tANI_U32 i;
    tANI_U8 *pRate;

    pRate = pSirRateSet->rate;
    for ( i = 0; i < pSirRateSet->numRates; i++ )
    {
        MaxRate = CSR_MAX( MaxRate, ( pRate[ i ] & (~CSR_DOT11_BASIC_RATE_MASK) ) );
    }

    // Save the max rate in the connected state information...

    // modify LastRates variable as well

    return;
}

eHalStatus csrRoamIssueStartBss( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamStartBssParams *pParam,
                                 tCsrRoamProfile *pProfile, tSirBssDescription *pBssDesc, tANI_U32 roamId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    eCsrBand eBand;
    // Set the roaming substate to 'Start BSS attempt'...
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_START_BSS_REQ, sessionId );
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    //Need to figure out whether we need to log WDS???
    if( CSR_IS_IBSS( pProfile ) )
    {
        vos_log_ibss_pkt_type *pIbssLog;
        WLAN_VOS_DIAG_LOG_ALLOC(pIbssLog, vos_log_ibss_pkt_type, LOG_WLAN_IBSS_C);
        if(pIbssLog)
        {
            if(pBssDesc)
            {
                pIbssLog->eventId = WLAN_IBSS_EVENT_JOIN_IBSS_REQ;
                vos_mem_copy(pIbssLog->bssid, pBssDesc->bssId, 6);
            }
            else
            {
                pIbssLog->eventId = WLAN_IBSS_EVENT_START_IBSS_REQ;
            }
            vos_mem_copy(pIbssLog->ssid, pParam->ssId.ssId, pParam->ssId.length);
            if(pProfile->ChannelInfo.numOfChannels == 0)
            {
                pIbssLog->channelSetting = AUTO_PICK;
            }
            else
            {
                pIbssLog->channelSetting = SPECIFIED;
            }
            pIbssLog->operatingChannel = pParam->operationChn;
            WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
        }
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    //Put RSN information in for Starting BSS
    pParam->nRSNIELength = (tANI_U16)pProfile->nRSNReqIELength;
    pParam->pRSNIE = pProfile->pRSNReqIE;

    pParam->privacy           = pProfile->privacy;
    pParam->fwdWPSPBCProbeReq = pProfile->fwdWPSPBCProbeReq;
    pParam->authType          = pProfile->csr80211AuthType;
    pParam->beaconInterval    = pProfile->beaconInterval;
    pParam->dtimPeriod        = pProfile->dtimPeriod;
    pParam->ApUapsdEnable     = pProfile->ApUapsdEnable;
    pParam->ssidHidden        = pProfile->SSIDs.SSIDList[0].ssidHidden;
    if (CSR_IS_INFRA_AP(pProfile)&& (pParam->operationChn != 0))
    {
        if (csrIsValidChannel(pMac, pParam->operationChn) != VOS_STATUS_SUCCESS)
        {
            pParam->operationChn = INFRA_AP_DEFAULT_CHANNEL;
        }
    }
    pParam->protEnabled     = pProfile->protEnabled;
    pParam->obssProtEnabled = pProfile->obssProtEnabled;
    pParam->ht_protection   = pProfile->cfg_protection;
    pParam->wps_state       = pProfile->wps_state;

    pParam->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, pProfile, pParam->operationChn /* pProfile->operationChannel*/,
                                        &eBand);
    pParam->bssPersona = pProfile->csrPersona;

#ifdef WLAN_FEATURE_11W
    pParam->mfpCapable = (0 != pProfile->MFPCapable);
    pParam->mfpRequired = (0 != pProfile->MFPRequired);
#endif

    pParam->addIeParams.probeRespDataLen =
        pProfile->addIeParams.probeRespDataLen;
    pParam->addIeParams.probeRespData_buff =
        pProfile->addIeParams.probeRespData_buff;

    pParam->addIeParams.assocRespDataLen =
        pProfile->addIeParams.assocRespDataLen;
    pParam->addIeParams.assocRespData_buff =
        pProfile->addIeParams.assocRespData_buff;

    if (CSR_IS_IBSS( pProfile ))
    {
        pParam->addIeParams.probeRespBCNDataLen =
            pProfile->nWPAReqIELength;
        pParam->addIeParams.probeRespBCNData_buff =
            pProfile->pWPAReqIE;
    }
    else
    {
        pParam->addIeParams.probeRespBCNDataLen =
            pProfile->addIeParams.probeRespBCNDataLen;
        pParam->addIeParams.probeRespBCNData_buff =
            pProfile->addIeParams.probeRespBCNData_buff;
    }
    pParam->sap_dot11mc = pProfile->sap_dot11mc;
    // When starting an IBSS, start on the channel from the Profile.
    status = csrSendMBStartBssReqMsg( pMac, sessionId, pProfile->BSSType, pParam, pBssDesc );
    return (status);
}

static void csrRoamPrepareBssParams(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                     tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig, tDot11fBeaconIEs *pIes)
{
    tANI_U8 Channel;
    ePhyChanBondState cbMode = PHY_SINGLE_CHANNEL_CENTERED;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if( pBssDesc )
    {
        csrRoamGetBssStartParmsFromBssDesc( pMac, pBssDesc, pIes, &pSession->bssParams );
        //Since csrRoamGetBssStartParmsFromBssDesc fills in the bssid for pSession->bssParams
        //The following code has to be do after that.
        //For WDS station, use selfMac as the self BSSID
        if( CSR_IS_WDS_STA( pProfile ) )
        {
            vos_mem_copy(&pSession->bssParams.bssid, &pSession->selfMacAddr,
                         sizeof(tCsrBssid));
        }
    }
    else
    {
        csrRoamGetBssStartParms(pMac, pProfile, &pSession->bssParams);
        //Use the first SSID
        if(pProfile->SSIDs.numOfSSIDs)
        {
            vos_mem_copy(&pSession->bssParams.ssId, pProfile->SSIDs.SSIDList,
                         sizeof(tSirMacSSid));
        }
        //For WDS station, use selfMac as the self BSSID
        if( CSR_IS_WDS_STA( pProfile ) )
        {
           vos_mem_copy(&pSession->bssParams.bssid, &pSession->selfMacAddr,
                        sizeof(tCsrBssid));
        }
        //Use the first BSSID
        else if( pProfile->BSSIDs.numOfBSSIDs )
        {
           vos_mem_copy(&pSession->bssParams.bssid, pProfile->BSSIDs.bssid,
                        sizeof(tCsrBssid));
        }
        else
        {
            vos_mem_set(&pSession->bssParams.bssid, sizeof(tCsrBssid), 0);
        }
    }
    Channel = pSession->bssParams.operationChn;
    //Set operating channel in pProfile which will be used
    //in csrRoamSetBssConfigCfg() to determine channel bonding
    //mode and will be configured in CFG later
    pProfile->operationChannel = Channel;

    if(Channel == 0)
    {
        smsLog(pMac, LOGE, "   CSR cannot find a channel to start IBSS");
    }
    else
    {

        csrRoamDetermineMaxRateForAdHoc( pMac, &pSession->bssParams.operationalRateSet );
        if (CSR_IS_INFRA_AP(pProfile) || CSR_IS_START_IBSS( pProfile ) )
        {
            if(CSR_IS_CHANNEL_24GHZ(Channel) )
            {
                cbMode = pMac->roam.configParam.channelBondingMode24GHz;
            }
            else
            {
                cbMode = pMac->roam.configParam.channelBondingMode5GHz;
            }
            smsLog(pMac, LOG1, "## cbMode %d", cbMode);
            pBssConfig->cbMode = cbMode;
            pSession->bssParams.cbMode = cbMode;
        }
    }
}

static eHalStatus csrRoamStartIbss( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                                    tANI_BOOLEAN *pfSameIbss )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fSameIbss = FALSE;

    if ( csrIsConnStateIbss( pMac, sessionId ) )
    {
        // Check if any profile parameter has changed ? If any profile parameter
        // has changed then stop old BSS and start a new one with new parameters
        if ( csrIsSameProfile( pMac, &pMac->roam.roamSession[sessionId].connectedProfile, pProfile ) )
        {
            fSameIbss = TRUE;
        }
        else
        {
            status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING );
        }
    }
    else if ( csrIsConnStateConnectedInfra( pMac, sessionId ) )
    {
        // Disassociate from the connected Infrastructure network...
        status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE );
    }
    else
    {
        tBssConfigParam *pBssConfig;

        pBssConfig = vos_mem_malloc(sizeof(tBssConfigParam));
        if ( NULL == pBssConfig )
           status = eHAL_STATUS_FAILURE;
        else
           status = eHAL_STATUS_SUCCESS;
        if(HAL_STATUS_SUCCESS(status))
        {
            vos_mem_set(pBssConfig, sizeof(tBssConfigParam), 0);
            // there is no Bss description before we start an IBSS so we need to adopt
            // all Bss configuration parameters from the Profile.
            status = csrRoamPrepareBssConfigFromProfile(pMac, pProfile, pBssConfig, NULL);
            if(HAL_STATUS_SUCCESS(status))
            {
                //save dotMode
                pMac->roam.roamSession[sessionId].bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
                //Prepare some more parameters for this IBSS
                csrRoamPrepareBssParams(pMac, sessionId, pProfile, NULL, pBssConfig, NULL);
                status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile,
                                                NULL, pBssConfig,
                                                NULL, eANI_BOOLEAN_FALSE);
            }

            vos_mem_free(pBssConfig);
        }//Allocate memory
    }

    if(pfSameIbss)
    {
        *pfSameIbss = fSameIbss;
    }
    return( status );
}

static void csrRoamUpdateConnectedProfileFromNewBss( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                                     tSirSmeNewBssInfo *pNewBss )
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    if( pNewBss )
    {
        // Set the operating channel.
        pSession->connectedProfile.operationChannel = pNewBss->channelNumber;
        // move the BSSId from the BSS description into the connected state information.
        vos_mem_copy(&pSession->connectedProfile.bssid, &(pNewBss->bssId),
                     sizeof( tCsrBssid ));
    }
    return;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
eHalStatus csrRoamSetPSK_PMK(tpAniSirGlobal pMac, tANI_U32 sessionId,
                             tANI_U8 *pPSK_PMK, size_t pmk_len)
{
    tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);
    if (!pSession) {
        smsLog(pMac, LOGE, FL("session %d not found"), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    vos_mem_copy(pSession->psk_pmk, pPSK_PMK, sizeof(pSession->psk_pmk));
    pSession->pmk_len = pmk_len;
    return eHAL_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
eHalStatus csrRoamSetPMKIDCache( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                 tPmkidCacheInfo *pPMKIDCache,
                                 tANI_U32 numItems,
                                 tANI_BOOLEAN update_entire_cache )
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if (!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    smsLog(pMac, LOGW, "csrRoamSetPMKIDCache called, numItems = %d", numItems);

    if (numItems <= CSR_MAX_PMKID_ALLOWED)
    {
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        {
            WLAN_VOS_DIAG_EVENT_DEF(secEvent, vos_event_wlan_security_payload_type);
            vos_mem_set(&secEvent,
                        sizeof(vos_event_wlan_security_payload_type), 0);
            secEvent.eventId = WLAN_SECURITY_EVENT_PMKID_UPDATE;
            secEvent.encryptionModeMulticast =
                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
            secEvent.encryptionModeUnicast =
                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
            vos_mem_copy(secEvent.bssid, pSession->connectedProfile.bssid, 6);
            secEvent.authMode =
                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
            WLAN_VOS_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
        }
#endif//FEATURE_WLAN_DIAG_SUPPORT_CSR

        status = eHAL_STATUS_SUCCESS;
        if (update_entire_cache) {
            if (numItems && pPMKIDCache)
            {
                pSession->NumPmkidCache = (uint16_t)numItems;
                vos_mem_copy(pSession->PmkidCacheInfo, pPMKIDCache,
                             sizeof(tPmkidCacheInfo) * numItems);
                pSession->curr_cache_idx = (uint16_t)numItems;
            }
        } else {
            tANI_U32 i = 0;
            tPmkidCacheInfo *pmksa;

            for (i = 0; i < numItems; i++) {
                pmksa = &pPMKIDCache[i];

                /* Delete the entry if present */
                csrRoamDelPMKIDfromCache(pMac,sessionId,pmksa->BSSID,FALSE);

                /* Add entry to the cache */
                vos_mem_copy(
                   pSession->PmkidCacheInfo[pSession->curr_cache_idx].BSSID,
                   pmksa->BSSID, ETHER_ADDR_LEN);
                vos_mem_copy(
                   pSession->PmkidCacheInfo[pSession->curr_cache_idx].PMKID,
                   pmksa->PMKID, CSR_RSN_PMKID_SIZE);

                /* Increment the CSR local cache index */
                if (pSession->curr_cache_idx < (CSR_MAX_PMKID_ALLOWED - 1))
                    pSession->curr_cache_idx++;
                else
                    pSession->curr_cache_idx = 0;

                pSession->NumPmkidCache++;
                if (pSession->NumPmkidCache > CSR_MAX_PMKID_ALLOWED)
                    pSession->NumPmkidCache = CSR_MAX_PMKID_ALLOWED;
            }
        }
    }
    return (status);
}

eHalStatus csrRoamDelPMKIDfromCache( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                     const tANI_U8 *pBSSId,
                                     tANI_BOOLEAN flush_cache )
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_BOOLEAN fMatchFound = FALSE;
    tANI_U32 Index;
    uint32_t curr_idx;
    uint32_t i;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    /* Check if there are no entries to delete */
    if (0 == pSession->NumPmkidCache) {
       smsLog(pMac, LOG1, FL("No entries to delete/Flush"));
       return eHAL_STATUS_SUCCESS;
    }

    if (!flush_cache) {
        for (Index = 0; Index < CSR_MAX_PMKID_ALLOWED; Index++) {
            if (vos_mem_compare(pSession->PmkidCacheInfo[Index].BSSID,
                      pBSSId, VOS_MAC_ADDR_SIZE)) {
                fMatchFound = 1;

                /* Clear this - the matched entry */
                vos_mem_zero(&pSession->PmkidCacheInfo[Index],
                             sizeof(tPmkidCacheInfo));

                break;
            }
        }

        if (Index == CSR_MAX_PMKID_ALLOWED && !fMatchFound) {
           smsLog(pMac, LOG1, FL("No such PMKSA entry exists "MAC_ADDRESS_STR),
                  MAC_ADDR_ARRAY(pBSSId));
        }
        else {
            /* Match Found */
            curr_idx = pSession->curr_cache_idx;
            if (Index < curr_idx) {
                for (i = Index; i < (curr_idx - 1); i++) {
                    vos_mem_copy(&pSession->PmkidCacheInfo[i],
                                 &pSession->PmkidCacheInfo[i+1],
                                 sizeof(tPmkidCacheInfo));
                }

                pSession->curr_cache_idx--;
                vos_mem_zero(
                    &pSession->PmkidCacheInfo[pSession->curr_cache_idx],
                    sizeof(tPmkidCacheInfo));
            } else if(Index > curr_idx) {
                for (i = Index; i > (curr_idx); i--) {
                    vos_mem_copy(&pSession->PmkidCacheInfo[i],
                                 &pSession->PmkidCacheInfo[i-1],
                                 sizeof(tPmkidCacheInfo));
                }
                vos_mem_zero(
                    &pSession->PmkidCacheInfo[pSession->curr_cache_idx],
                    sizeof(tPmkidCacheInfo));
            }
            pSession->NumPmkidCache--;
        }
    } else {
        /* Flush the entire cache */
        vos_mem_zero(pSession->PmkidCacheInfo,
                     sizeof(tPmkidCacheInfo) * CSR_MAX_PMKID_ALLOWED);
        pSession->NumPmkidCache = 0;
        pSession->curr_cache_idx = 0;
    }

    return eHAL_STATUS_SUCCESS;
}

tANI_U32 csrRoamGetNumPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (pMac->roam.roamSession[sessionId].NumPmkidCache);
}

eHalStatus csrRoamGetPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pNum, tPmkidCacheInfo *pPmkidCache)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tPmkidCacheInfo *pmksa;
    uint16_t i, j;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pNum && pPmkidCache)
    {
        if(pSession->NumPmkidCache == 0)
        {
            *pNum = 0;
            status = eHAL_STATUS_SUCCESS;
        }
        else if(*pNum >= pSession->NumPmkidCache)
        {
            if(pSession->NumPmkidCache > CSR_MAX_PMKID_ALLOWED)
            {
                smsLog(pMac, LOGE,
                       FL("NumPmkidCache :%d is more than CSR_MAX_PMKID_ALLOWED, resetting to CSR_MAX_PMKID_ALLOWED"),
                       pSession->NumPmkidCache);
                pSession->NumPmkidCache = CSR_MAX_PMKID_ALLOWED;
            }

            for (i = 0,j = 0;
                 (j < pSession->NumPmkidCache) && (i < CSR_MAX_PMKID_ALLOWED);
                 i++) {
                 /* Fill the valid entries */
                 pmksa = &pSession->PmkidCacheInfo[i];
                 if (!csrIsMacAddressZero(pMac, &pmksa->BSSID)) {
                     vos_mem_copy(pPmkidCache, pmksa,
                                  sizeof(tPmkidCacheInfo));
                     pPmkidCache++;
                     j++;
                 }
            }

            *pNum = pSession->NumPmkidCache;
            status = eHAL_STATUS_SUCCESS;
        }
    }
    return (status);
}

eHalStatus csrRoamGetWpaRsnReqIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 len;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pLen)
    {
        len = *pLen;
        *pLen = pSession->nWpaRsnReqIeLength;
        if(pBuf)
        {
            if(len >= pSession->nWpaRsnReqIeLength)
            {
                vos_mem_copy(pBuf, pSession->pWpaRsnReqIE,
                             pSession->nWpaRsnReqIeLength);
                status = eHAL_STATUS_SUCCESS;
            }
        }
    }
    return (status);
}

eHalStatus csrRoamGetWpaRsnRspIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 len;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pLen)
    {
        len = *pLen;
        *pLen = pSession->nWpaRsnRspIeLength;
        if(pBuf)
        {
            if(len >= pSession->nWpaRsnRspIeLength)
            {
                vos_mem_copy(pBuf, pSession->pWpaRsnRspIE,
                             pSession->nWpaRsnRspIeLength);
                status = eHAL_STATUS_SUCCESS;
            }
        }
    }
    return (status);
}
#ifdef FEATURE_WLAN_WAPI
eHalStatus csrRoamGetWapiReqIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 len;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pLen)
    {
        len = *pLen;
        *pLen = pSession->nWapiReqIeLength;
        if(pBuf)
        {
            if(len >= pSession->nWapiReqIeLength)
            {
                vos_mem_copy(pBuf, pSession->pWapiReqIE,
                             pSession->nWapiReqIeLength);
                status = eHAL_STATUS_SUCCESS;
            }
        }
    }
    return (status);
}
eHalStatus csrRoamGetWapiRspIE(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pLen, tANI_U8 *pBuf)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 len;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if(pLen)
    {
        len = *pLen;
        *pLen = pSession->nWapiRspIeLength;
        if(pBuf)
        {
            if(len >= pSession->nWapiRspIeLength)
            {
                vos_mem_copy(pBuf, pSession->pWapiRspIE,
                             pSession->nWapiRspIeLength);
                status = eHAL_STATUS_SUCCESS;
            }
        }
    }
    return (status);
}
#endif /* FEATURE_WLAN_WAPI */
eRoamCmdStatus csrGetRoamCompleteStatus(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    eRoamCmdStatus retStatus = eCSR_ROAM_CONNECT_COMPLETION;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return (retStatus);
    }

    if(CSR_IS_ROAMING(pSession))
    {
        retStatus = eCSR_ROAM_ROAMING_COMPLETION;
        pSession->fRoaming = eANI_BOOLEAN_FALSE;
    }
    return (retStatus);
}

/* This function remove the connected BSS from the cached scan result */
eHalStatus csrRoamRemoveConnectedBssFromScanCache(tpAniSirGlobal pMac,
                                                  tCsrRoamConnectedProfile *pConnProfile)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tCsrScanResultFilter *pScanFilter = NULL;
    tListElem *pEntry;
    tCsrScanResult *pResult;
        tDot11fBeaconIEs *pIes;
    tANI_BOOLEAN fMatch;
    if(!(csrIsMacAddressZero(pMac, &pConnProfile->bssid) ||
            csrIsMacAddressBroadcast(pMac, &pConnProfile->bssid)))
    {
        do
        {
            //Prepare the filter. Only fill in the necessary fields. Not all fields are needed
            pScanFilter = vos_mem_malloc(sizeof(tCsrScanResultFilter));
            if ( NULL == pScanFilter )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status)) break;
            vos_mem_set(pScanFilter, sizeof(tCsrScanResultFilter), 0);
            pScanFilter->BSSIDs.bssid = vos_mem_malloc(sizeof(tCsrBssid));
            if ( NULL == pScanFilter->BSSIDs.bssid )
                status = eHAL_STATUS_FAILURE;
            else
                status = eHAL_STATUS_SUCCESS;
            if(!HAL_STATUS_SUCCESS(status)) break;
            vos_mem_copy(pScanFilter->BSSIDs.bssid, &pConnProfile->bssid,
                         sizeof(tCsrBssid));
            pScanFilter->BSSIDs.numOfBSSIDs = 1;
            if(!csrIsNULLSSID(pConnProfile->SSID.ssId, pConnProfile->SSID.length))
            {
                pScanFilter->SSIDs.SSIDList = vos_mem_malloc(sizeof(tCsrSSIDInfo));
                if ( NULL  == pScanFilter->SSIDs.SSIDList )
                    status = eHAL_STATUS_FAILURE;
                else
                    status = eHAL_STATUS_SUCCESS;
                if (!HAL_STATUS_SUCCESS(status)) break;
                vos_mem_copy(&pScanFilter->SSIDs.SSIDList[0].SSID,
                             &pConnProfile->SSID, sizeof(tSirMacSSid));
            }
            pScanFilter->authType.numEntries = 1;
            pScanFilter->authType.authType[0] = pConnProfile->AuthType;
            pScanFilter->BSSType = pConnProfile->BSSType;
            pScanFilter->EncryptionType.numEntries = 1;
            pScanFilter->EncryptionType.encryptionType[0] = pConnProfile->EncryptionType;
            pScanFilter->mcEncryptionType.numEntries = 1;
            pScanFilter->mcEncryptionType.encryptionType[0] = pConnProfile->mcEncryptionType;
            //We ignore the channel for now, BSSID should be enough
            pScanFilter->ChannelInfo.numOfChannels = 0;
            //Also ignore the following fields
            pScanFilter->uapsd_mask = 0;
            pScanFilter->bWPSAssociation = eANI_BOOLEAN_FALSE;
            pScanFilter->bOSENAssociation = eANI_BOOLEAN_FALSE;
            pScanFilter->countryCode[0] = 0;
            pScanFilter->phyMode = eCSR_DOT11_MODE_AUTO;
            csrLLLock(&pMac->scan.scanResultList);
            pEntry = csrLLPeekHead( &pMac->scan.scanResultList, LL_ACCESS_NOLOCK );
            while( pEntry )
            {
                pResult = GET_BASE_ADDR( pEntry, tCsrScanResult, Link );
                                pIes = (tDot11fBeaconIEs *)( pResult->Result.pvIes );
                fMatch = csrMatchBSS(pMac, &pResult->Result.BssDescriptor,
                               pScanFilter, NULL, NULL, NULL, &pIes);
                //Release the IEs allocated by csrMatchBSS is needed
                if( !pResult->Result.pvIes )
                {
                    //need to free the IEs since it is allocated by csrMatchBSS
                    vos_mem_free(pIes);
                }
                if(fMatch)
                {
                    //We found the one
                    if( csrLLRemoveEntry(&pMac->scan.scanResultList, pEntry, LL_ACCESS_NOLOCK) )
                    {
                        //Free the memory
                        csrFreeScanResultEntry( pMac, pResult );
                    }
                    break;
                }
                pEntry = csrLLNext(&pMac->scan.scanResultList, pEntry, LL_ACCESS_NOLOCK);
            }//while
            csrLLUnlock(&pMac->scan.scanResultList);
        }while(0);
        if(pScanFilter)
        {
            csrFreeScanFilter(pMac, pScanFilter);
            vos_mem_free(pScanFilter);
        }
    }
    return (status);
}

//BT-AMP
eHalStatus csrIsBTAMPAllowed( tpAniSirGlobal pMac, tANI_U32 chnId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 sessionId;
    for( sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
            if( csrIsConnStateIbss( pMac, sessionId ) || csrIsBTAMP( pMac, sessionId ) )
            {
                //co-exist with IBSS or BT-AMP is not supported
                smsLog( pMac, LOGW, " BTAMP is not allowed due to IBSS/BT-AMP exist in session %d", sessionId );
                status = eHAL_STATUS_CSR_WRONG_STATE;
                break;
            }
            if( csrIsConnStateInfra( pMac, sessionId ) )
            {
                if( chnId &&
                    ( (tANI_U8)chnId != pMac->roam.roamSession[sessionId].connectedProfile.operationChannel ) )
                {
                    smsLog( pMac, LOGW, " BTAMP is not allowed due to channel (%d) diff than infr channel (%d)",
                        chnId, pMac->roam.roamSession[sessionId].connectedProfile.operationChannel );
                    status = eHAL_STATUS_CSR_WRONG_STATE;
                    break;
                }
            }
        }
    }
    return ( status );
}

static eHalStatus csrRoamStartWds( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tSirBssDescription *pBssDesc )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tBssConfigParam bssConfig;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if ( csrIsConnStateIbss( pMac, sessionId ) )
    {
        status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING );
    }
    else if ( csrIsConnStateConnectedInfra( pMac, sessionId ) )
    {
        // Disassociate from the connected Infrastructure network...
        status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE );
    }
    else
    {
        //We don't expect Bt-AMP HDD not to disconnect the last connection first at this time.
        //Otherwise we need to add code to handle the
        //situation just like IBSS. Though for WDS station, we need to send disassoc to PE first then
        //send stop_bss to PE, before we can continue.

        if (csrIsConnStateWds( pMac, sessionId ))
        {
           VOS_ASSERT(0);
           return eHAL_STATUS_FAILURE;
        }
        vos_mem_set(&bssConfig, sizeof(tBssConfigParam), 0);
        /* Assume HDD provide bssid in profile */
        vos_mem_copy(&pSession->bssParams.bssid, pProfile->BSSIDs.bssid[0],
                     sizeof(tCsrBssid));
        // there is no Bss description before we start an WDS so we need
        // to adopt all Bss configuration parameters from the Profile.
        status = csrRoamPrepareBssConfigFromProfile(pMac, pProfile, &bssConfig, pBssDesc);
        if(HAL_STATUS_SUCCESS(status))
        {
            //Save profile for late use
            csrFreeRoamProfile( pMac, sessionId );
            pSession->pCurRoamProfile = vos_mem_malloc(sizeof(tCsrRoamProfile));
            if (pSession->pCurRoamProfile != NULL )
            {
                vos_mem_set(pSession->pCurRoamProfile,
                            sizeof(tCsrRoamProfile), 0);
                csrRoamCopyProfile(pMac, pSession->pCurRoamProfile, pProfile);
            }
            //Prepare some more parameters for this WDS
            csrRoamPrepareBssParams(pMac, sessionId, pProfile, NULL, &bssConfig, NULL);
            status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile,
                                            NULL, &bssConfig,
                                            NULL, eANI_BOOLEAN_FALSE);
        }
    }

    return( status );
}

////////////////////Mail box

//pBuf is caller allocated memory point to &(tSirSmeJoinReq->rsnIE.rsnIEdata[ 0 ]) + pMsg->rsnIE.length;
//or &(tSirSmeReassocReq->rsnIE.rsnIEdata[ 0 ]) + pMsg->rsnIE.length;
static void csrPrepareJoinReassocReqBuffer( tpAniSirGlobal pMac,
                                            tSirBssDescription *pBssDescription,
                                            tANI_U8 *pBuf, tANI_U8 uapsdMask)
{
    tCsrChannelSet channelGroup;
    tSirMacCapabilityInfo *pAP_capabilityInfo;
    tAniBool fTmp;
    tANI_BOOLEAN found = FALSE;
    tANI_U32 size = 0;
    tANI_S8 pwrLimit = 0;
    tANI_U16 i;
    // 802.11h
    //We can do this because it is in HOST CPU order for now.
    pAP_capabilityInfo = (tSirMacCapabilityInfo *)&pBssDescription->capabilityInfo;
    //tell the target AP my 11H capability only if both AP and STA support 11H and the channel being used is 11a
    if ( csrIs11hSupported( pMac ) && pAP_capabilityInfo->spectrumMgt && eSIR_11A_NW_TYPE == pBssDescription->nwType )
    {
        fTmp = (tAniBool)pal_cpu_to_be32(1);
    }
    else
        fTmp = (tAniBool)0;

    // corresponds to --- pMsg->spectrumMgtIndicator = ON;
    vos_mem_copy(pBuf, (tANI_U8 *)&fTmp, sizeof(tAniBool));
    pBuf += sizeof(tAniBool);
    *pBuf++ = MIN_TX_PWR_CAP; // it is for pMsg->powerCap.minTxPower = 0;
    found = csrSearchChannelListForTxPower(pMac, pBssDescription, &channelGroup);
    // This is required for 11k test VoWiFi Ent: Test 2.
    // We need the power capabilities for Assoc Req.
    // This macro is provided by the halPhyCfg.h. We pick our
    // max and min capability by the halPhy provided macros
    pwrLimit = csrGetCfgMaxTxPower (pMac, pBssDescription->channelId);
    if (0 != pwrLimit)
    {
        *pBuf++ = pwrLimit;
    }
    else
    {
        *pBuf++ = MAX_TX_PWR_CAP;
    }
    size = sizeof(pMac->roam.validChannelList);
    if(HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, (tANI_U8 *)pMac->roam.validChannelList, &size)))
    {
        tANI_U8 *actualSize = pBuf++;
        *actualSize = 0;

        for ( i = 0; i < size; i++)
        {
            /* Only add 5ghz channels*/
            if (CSR_IS_CHANNEL_5GHZ(pMac->roam.validChannelList[ i ]))
            {
                  *actualSize +=1;
                  *pBuf++ = pMac->roam.validChannelList[ i ];
            }

        }
    }
    else
    {
        smsLog(pMac, LOGE, FL("can not find any valid channel"));
        *pBuf++ = 0;  //tSirSupChnl->numChnl
    }
    //Check whether it is ok to enter UAPSD
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    if( btcIsReadyForUapsd(pMac) )
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
    {
       *pBuf++ = uapsdMask;
    }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
    else
    {
        smsLog(pMac, LOGE, FL(" BTC doesn't allow UAPSD for uapsd_mask(0x%X)"), uapsdMask);
        *pBuf++ = 0;
    }
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/

    // move the entire BssDescription into the join request.
    vos_mem_copy(pBuf, pBssDescription,
                 pBssDescription->length + sizeof( pBssDescription->length ));
    pBuf += pBssDescription->length + sizeof( pBssDescription->length );   // update to new location
}

/*
  * The communication between HDD and LIM is thru mailbox (MB).
  * Both sides will access the data structure "tSirSmeJoinReq".
  *  The rule is, while the components of "tSirSmeJoinReq" can be accessed
  *  in the regular way like tSirSmeJoinReq.assocType, this guideline
  *  stops at component tSirRSNie; any access to the components after tSirRSNie
  *  is forbidden because the space from tSirRSNie is squeezed
  *  with the component "tSirBssDescription". And since the size of actual
  *  'tSirBssDescription' varies, the receiving side (which is the routine
  *  limJoinReqSerDes() of limSerDesUtils.cc) should keep in mind not to access
  *  the components DIRECTLY after tSirRSNie.
  */
eHalStatus csrSendJoinReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDescription,
                              tCsrRoamProfile *pProfile, tDot11fBeaconIEs *pIes, tANI_U16 messageType )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeJoinReq *pMsg;
    tANI_U8 *pBuf;
    v_U8_t acm_mask = 0, uapsd_mask;
    tANI_U16 msgLen, wTmp, ieLen;
    tSirMacRateSet OpRateSet;
    tSirMacRateSet ExRateSet;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U32 dwTmp;
    tANI_U8 wpaRsnIE[DOT11F_IE_RSN_MAX_LEN];    //RSN MAX is bigger than WPA MAX
    tANI_U32 ucDot11Mode = 0;
    tANI_U8 txBFCsnValue = 0;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    if (NULL == pBssDescription)
    {
        smsLog(pMac, LOGE, FL(" pBssDescription is NULL"));
        return eHAL_STATUS_FAILURE;
    }

    do {
        pSession->joinFailStatusCode.statusCode = eSIR_SME_SUCCESS;
        pSession->joinFailStatusCode.reasonCode = 0;
        vos_mem_copy(&pSession->joinFailStatusCode.bssId,
                     &pBssDescription->bssId, sizeof(tSirMacAddr));
        // There are a number of variable length fields to consider.  First, the tSirSmeJoinReq
        // includes a single bssDescription.   bssDescription includes a single tANI_U32 for the
        // IE fields, but the length field in the bssDescription needs to be interpreted to
        // determine length of the IE fields.
        //
        // So, take the size of the JoinReq, subtract the size of the bssDescription and
        // add in the length from the bssDescription (then add the size of the 'length' field
        // itself because that is NOT included in the length field).
        msgLen = sizeof( tSirSmeJoinReq ) - sizeof( *pBssDescription ) +
            pBssDescription->length + sizeof( pBssDescription->length ) +
            sizeof( tCsrWpaIe ) + sizeof( tCsrWpaAuthIe ) + sizeof( tANI_U16 ); // add in the size of the WPA IE that we may build.
        pMsg = vos_mem_malloc(msgLen);
        if (NULL == pMsg)
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, msgLen , 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)messageType);
        pMsg->length = pal_cpu_to_be16(msgLen);
        pBuf = &pMsg->sessionId;
        // sessionId
        *pBuf = (tANI_U8)sessionId;
        pBuf++;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
        // ssId
        if( pIes->SSID.present && pIes->SSID.num_ssid )
        {
            // ssId len
            *pBuf = pIes->SSID.num_ssid;
            pBuf++;
            vos_mem_copy(pBuf, pIes->SSID.ssid, pIes->SSID.num_ssid);
            pBuf += pIes->SSID.num_ssid;
        }
        else
        {
            *pBuf = 0;
            pBuf++;
        }
        smsLog(pMac, LOGE,
               "Connecting to ssid:%.*s bssid: "
               MAC_ADDRESS_STR" rssi: %d channel: %d country_code: %c%c",
               pIes->SSID.num_ssid, pIes->SSID.ssid,
               MAC_ADDR_ARRAY(pBssDescription->bssId),
               pBssDescription->rssi, pBssDescription->channelId,
               pMac->scan.countryCodeCurrent[0],
               pMac->scan.countryCodeCurrent[1]);
        // selfMacAddr
        vos_mem_copy((tSirMacAddr *)pBuf, &pSession->selfMacAddr,
                     sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // bsstype
        dwTmp = pal_cpu_to_be32( csrTranslateBsstypeToMacType( pProfile->BSSType ) );
        if (dwTmp == eSIR_BTAMP_STA_MODE) dwTmp = eSIR_BTAMP_AP_MODE; // Override BssType for BTAMP
        vos_mem_copy(pBuf, &dwTmp, sizeof(tSirBssType));
        pBuf += sizeof(tSirBssType);
        // dot11mode
        ucDot11Mode = csrTranslateToWNICfgDot11Mode( pMac, pSession->bssParams.uCfgDot11Mode );
        if (pBssDescription->channelId <= 14 &&
            FALSE == pMac->roam.configParam.enableVhtFor24GHz &&
            WNI_CFG_DOT11_MODE_11AC == ucDot11Mode)
        {
            //Need to disable VHT operation in 2.4 GHz band
            ucDot11Mode = WNI_CFG_DOT11_MODE_11N;
        }
        *pBuf = (tANI_U8)ucDot11Mode;
        pBuf++;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
        *pBuf = pMac->roam.configParam.cc_switch_mode;
        pBuf += 1;
#endif
        //Persona
        *pBuf = (tANI_U8)pProfile->csrPersona;
        pBuf++;
        //CBMode
        *pBuf = (tANI_U8)pSession->bssParams.cbMode;
        pBuf += sizeof(ePhyChanBondState);

        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  FL("CSR PERSONA=%d CSR CbMode %d"), pProfile->csrPersona, pSession->bssParams.cbMode);

        // uapsdPerAcBitmask
        *pBuf = pProfile->uapsd_mask;
        pBuf++;



        status = csrGetRateSet(pMac, pProfile, (eCsrPhyMode)pProfile->phyMode, pBssDescription, pIes, &OpRateSet, &ExRateSet);
        if (HAL_STATUS_SUCCESS(status) )
        {
            // OperationalRateSet
            if (OpRateSet.numRates) {
                *pBuf++ = OpRateSet.numRates;
                vos_mem_copy(pBuf, OpRateSet.rate, OpRateSet.numRates);
                pBuf += OpRateSet.numRates;
            } else *pBuf++ = 0;
            // ExtendedRateSet
            if (ExRateSet.numRates) {
                *pBuf++ = ExRateSet.numRates;
                vos_mem_copy(pBuf, ExRateSet.rate, ExRateSet.numRates);
                pBuf += ExRateSet.numRates;
            } else *pBuf++ = 0;
        }
        else
        {
            *pBuf++ = 0;
            *pBuf++ = 0;
        }
        // rsnIE
        if ( csrIsProfileWpa( pProfile ) )
        {
            // Insert the Wpa IE into the join request
            ieLen = csrRetrieveWpaIe( pMac, pProfile, pBssDescription, pIes,
                    (tCsrWpaIe *)( wpaRsnIE ) );
        }
        else if( csrIsProfileRSN( pProfile ) )
        {
            // Insert the RSN IE into the join request
            ieLen = csrRetrieveRsnIe( pMac, sessionId, pProfile, pBssDescription, pIes,
                    (tCsrRSNIe *)( wpaRsnIE ) );
        }
#ifdef FEATURE_WLAN_WAPI
        else if( csrIsProfileWapi( pProfile ) )
        {
            // Insert the WAPI IE into the join request
            ieLen = csrRetrieveWapiIe( pMac, sessionId, pProfile, pBssDescription, pIes,
                    (tCsrWapiIe *)( wpaRsnIE ) );
        }
#endif /* FEATURE_WLAN_WAPI */
        else
        {
            ieLen = 0;
        }
        //remember the IE for future use
        if( ieLen )
        {
            if(ieLen > DOT11F_IE_RSN_MAX_LEN)
            {
                smsLog(pMac, LOGE, FL(" WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d"), ieLen, DOT11F_IE_RSN_MAX_LEN);
                ieLen = DOT11F_IE_RSN_MAX_LEN;
            }
#ifdef FEATURE_WLAN_WAPI
            if( csrIsProfileWapi( pProfile ) )
            {
                //Check whether we need to allocate more memory
                if(ieLen > pSession->nWapiReqIeLength)
                {
                    if(pSession->pWapiReqIE && pSession->nWapiReqIeLength)
                    {
                        vos_mem_free(pSession->pWapiReqIE);
                    }
                    pSession->pWapiReqIE = vos_mem_malloc(ieLen);
                    if (NULL == pSession->pWapiReqIE)
                        status = eHAL_STATUS_FAILURE;
                    else
                        status = eHAL_STATUS_SUCCESS;
                    if(!HAL_STATUS_SUCCESS(status)) break;
                }
                pSession->nWapiReqIeLength = ieLen;
                vos_mem_copy(pSession->pWapiReqIE, wpaRsnIE, ieLen);
                wTmp = pal_cpu_to_be16( ieLen );
                vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
                pBuf += sizeof(tANI_U16);
                vos_mem_copy(pBuf, wpaRsnIE, ieLen);
                pBuf += ieLen;
            }
            else//should be WPA/WPA2 otherwise
#endif /* FEATURE_WLAN_WAPI */
            {
                //Check whether we need to allocate more memory
                if(ieLen > pSession->nWpaRsnReqIeLength)
                {
                    if(pSession->pWpaRsnReqIE && pSession->nWpaRsnReqIeLength)
                    {
                        vos_mem_free(pSession->pWpaRsnReqIE);
                    }
                    pSession->pWpaRsnReqIE = vos_mem_malloc(ieLen);
                    if (NULL == pSession->pWpaRsnReqIE)
                        status = eHAL_STATUS_FAILURE;
                    else
                        status = eHAL_STATUS_SUCCESS;
                    if(!HAL_STATUS_SUCCESS(status)) break;
                }
                pSession->nWpaRsnReqIeLength = ieLen;
                vos_mem_copy(pSession->pWpaRsnReqIE, wpaRsnIE, ieLen);
                wTmp = pal_cpu_to_be16( ieLen );
                vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
                pBuf += sizeof(tANI_U16);
                vos_mem_copy(pBuf, wpaRsnIE, ieLen);
                pBuf += ieLen;
            }
        }
        else
        {
            //free whatever old info
            pSession->nWpaRsnReqIeLength = 0;
            if(pSession->pWpaRsnReqIE)
            {
                vos_mem_free(pSession->pWpaRsnReqIE);
                pSession->pWpaRsnReqIE = NULL;
            }
#ifdef FEATURE_WLAN_WAPI
            pSession->nWapiReqIeLength = 0;
            if(pSession->pWapiReqIE)
            {
                vos_mem_free(pSession->pWapiReqIE);
                pSession->pWapiReqIE = NULL;
            }
#endif /* FEATURE_WLAN_WAPI */
            //length is two bytes
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
#ifdef FEATURE_WLAN_ESE
        if( eWNI_SME_JOIN_REQ == messageType )
        {
            // Never include the cckmIE in an Join Request
            //length is two bytes
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
        else if(eWNI_SME_REASSOC_REQ == messageType )
        {
            // cckmIE
            if( csrIsProfileESE( pProfile ) )
            {
                // Insert the CCKM IE into the join request
#ifdef FEATURE_WLAN_ESE_UPLOAD
                ieLen = pSession->suppCckmIeInfo.cckmIeLen;
					 vos_mem_copy((void *) (wpaRsnIE),
							 pSession->suppCckmIeInfo.cckmIe, ieLen);
#else
                ieLen = csrConstructEseCckmIe( pMac,
                                          pSession,
                                          pProfile,
                                          pBssDescription,
                                          pSession->pWpaRsnReqIE,
                                          pSession->nWpaRsnReqIeLength,
                                          (void *)( wpaRsnIE ) );
#endif /* FEATURE_WLAN_ESE_UPLOAD */
            }
            else
            {
                ieLen = 0;
            }
            //If present, copy the IE into the eWNI_SME_REASSOC_REQ message buffer
            if( ieLen )
            {
                //Copy the CCKM IE over from the temp buffer (wpaRsnIE)
                wTmp = pal_cpu_to_be16( ieLen );
                vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
                pBuf += sizeof(tANI_U16);
                vos_mem_copy(pBuf, wpaRsnIE, ieLen);
                pBuf += ieLen;
            }
            else
            {
                //Indicate you have no CCKM IE
                //length is two bytes
                *pBuf = 0;
                *(pBuf + 1) = 0;
                pBuf += 2;
            }
        }
#endif /* FEATURE_WLAN_ESE */
        // addIEScan
        if(pProfile->nAddIEScanLength && pProfile->pAddIEScan)
        {
            ieLen = pProfile->nAddIEScanLength;
            if(ieLen > pSession->nAddIEScanLength)
            {
                if(pSession->pAddIEScan && pSession->nAddIEScanLength)
                {
                    vos_mem_free(pSession->pAddIEScan);
                }
                pSession->pAddIEScan = vos_mem_malloc(ieLen);
                if (NULL == pSession->pAddIEScan)
                    status = eHAL_STATUS_FAILURE;
                else
                    status = eHAL_STATUS_SUCCESS;
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nAddIEScanLength = ieLen;
            vos_mem_copy(pSession->pAddIEScan, pProfile->pAddIEScan, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
            pBuf += sizeof(tANI_U16);
            vos_mem_copy(pBuf, pProfile->pAddIEScan, ieLen);
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEScanLength = 0;
            if(pSession->pAddIEScan)
            {
                vos_mem_free(pSession->pAddIEScan);
                pSession->pAddIEScan = NULL;
            }
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
        // addIEAssoc
        if(pProfile->nAddIEAssocLength && pProfile->pAddIEAssoc)
        {
            ieLen = pProfile->nAddIEAssocLength;
            if(ieLen > pSession->nAddIEAssocLength)
            {
                if(pSession->pAddIEAssoc && pSession->nAddIEAssocLength)
                {
                    vos_mem_free(pSession->pAddIEAssoc);
                }
                pSession->pAddIEAssoc = vos_mem_malloc(ieLen);
                if (NULL == pSession->pAddIEAssoc)
                    status = eHAL_STATUS_FAILURE;
                else
                    status = eHAL_STATUS_SUCCESS;
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nAddIEAssocLength = ieLen;
            vos_mem_copy(pSession->pAddIEAssoc, pProfile->pAddIEAssoc, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
            pBuf += sizeof(tANI_U16);
            vos_mem_copy(pBuf, pProfile->pAddIEAssoc, ieLen);
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEAssocLength = 0;
            if(pSession->pAddIEAssoc)
            {
                vos_mem_free(pSession->pAddIEAssoc);
                pSession->pAddIEAssoc = NULL;
            }
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }

        if(eWNI_SME_REASSOC_REQ == messageType )
        {
            //Unmask any AC in reassoc that is ACM-set
            uapsd_mask = (v_U8_t)pProfile->uapsd_mask;
            if( uapsd_mask && ( NULL != pBssDescription ) )
            {
                if( CSR_IS_QOS_BSS(pIes) && CSR_IS_UAPSD_BSS(pIes) )
                {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    acm_mask = sme_QosGetACMMask(pMac, pBssDescription, pIes);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                }
                else
                {
                    uapsd_mask = 0;
                }
            }
        }

        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedUCEncryptionType) );
        vos_mem_copy(pBuf, &dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);

        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedMCEncryptionType) );
        vos_mem_copy(pBuf, &dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
#ifdef WLAN_FEATURE_11W
        //MgmtEncryption
        if (pProfile->MFPEnabled)
        {
            dwTmp = pal_cpu_to_be32(eSIR_ED_AES_128_CMAC);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(eSIR_ED_NONE);
        }
        vos_mem_copy(pBuf, &dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
        pProfile->MDID.mdiePresent = pBssDescription->mdiePresent;
        if (csrIsProfile11r( pProfile )
#ifdef FEATURE_WLAN_ESE
           && !((pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM) &&
                (pIes->ESEVersion.present) && (pMac->roam.configParam.isEseIniFeatureEnabled))
#endif
        )
        {
            // is11Rconnection;
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool)) ;
            pBuf += sizeof(tAniBool);
        }
        else
        {
            // is11Rconnection;
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
#endif
#ifdef FEATURE_WLAN_ESE

        // isESEFeatureIniEnabled
        if (TRUE == pMac->roam.configParam.isEseIniFeatureEnabled)
        {
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }

        /* A profile can not be both ESE and 11R. But an 802.11R AP
         * may be advertising support for ESE as well. So if we are
         * associating Open or explicitly ESE then we will get ESE.
         * If we are associating explicitly 11R only then we will get
         * 11R.
         */
        if ((csrIsProfileESE(pProfile) ||
                  ((pIes->ESEVersion.present)
                   && (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM)))
                 && (pMac->roam.configParam.isEseIniFeatureEnabled))
        {
            // isESEconnection;
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            //isESEconnection;
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }

        if (eWNI_SME_JOIN_REQ == messageType)
        {
            tESETspecInfo eseTspec;
            // ESE-Tspec IEs in the ASSOC request is presently not supported
            // so nullify the TSPEC parameters
            vos_mem_set(&eseTspec, sizeof(tESETspecInfo), 0);
            vos_mem_copy(pBuf, &eseTspec, sizeof(tESETspecInfo));
            pBuf += sizeof(tESETspecInfo);
        }
        else if (eWNI_SME_REASSOC_REQ == messageType)
        {
        if ((csrIsProfileESE(pProfile) ||
             ((pIes->ESEVersion.present)
              && (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM)))
            && (pMac->roam.configParam.isEseIniFeatureEnabled))
        {
           tESETspecInfo eseTspec;
           // ESE Tspec information
           vos_mem_set(&eseTspec, sizeof(tESETspecInfo), 0);
           eseTspec.numTspecs = sme_QosEseRetrieveTspecInfo(pMac, sessionId, (tTspecInfo *) &eseTspec.tspec[0]);
           *pBuf = eseTspec.numTspecs;
           pBuf += sizeof(tANI_U8);
           // Copy the TSPEC information only if present
           if (eseTspec.numTspecs) {
               vos_mem_copy(pBuf, (void*)&eseTspec.tspec[0],
                           (eseTspec.numTspecs*sizeof(tTspecInfo)));
           }
           pBuf += sizeof(eseTspec.tspec);
        }
        else
        {
                tESETspecInfo eseTspec;
                // ESE-Tspec IEs in the ASSOC request is presently not supported
                // so nullify the TSPEC parameters
                vos_mem_set(&eseTspec, sizeof(tESETspecInfo), 0);
                vos_mem_copy(pBuf, &eseTspec, sizeof(tESETspecInfo));
                pBuf += sizeof(tESETspecInfo);
            }
        }
#endif // FEATURE_WLAN_ESE
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
        // Fill in isFastTransitionEnabled
        if (pMac->roam.configParam.isFastTransitionEnabled
#ifdef FEATURE_WLAN_LFR
         || csrRoamIsFastRoamEnabled(pMac, sessionId)
#endif
         )
        {
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
#endif
#ifdef FEATURE_WLAN_LFR
        if(csrRoamIsFastRoamEnabled(pMac, sessionId))
        {
            //legacy fast roaming enabled
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
#endif

        // txLdpcIniFeatureEnabled
        *pBuf = (tANI_U8)pMac->roam.configParam.txLdpcEnable;
        pBuf++;

        if ((csrIs11hSupported (pMac)) && (CSR_IS_CHANNEL_5GHZ(pBssDescription->channelId)) &&
            (pIes->Country.present) && (!pMac->roam.configParam.fSupplicantCountryCodeHasPriority))
        {
            csrSaveToChannelPower2G_5G( pMac, pIes->Country.num_triplets * sizeof(tSirMacChanInfo),
                                        (tSirMacChanInfo *)(&pIes->Country.triplets[0]) );
            csrApplyPower2Current(pMac);
        }

        //HT Config
		  vos_mem_copy(pBuf, &pSession->htConfig,
				  sizeof(tSirHTConfig));
		  pBuf += sizeof(tSirHTConfig);
#ifdef WLAN_FEATURE_11AC
        // txBFIniFeatureEnabled
        *pBuf = (tANI_U8)pMac->roam.configParam.txBFEnable;
        pBuf++;

        // txBFCsnValue
        if (IS_BSS_VHT_CAPABLE(pIes->VHTCaps) &&
			pMac->roam.configParam.txBFEnable) {
		txBFCsnValue = (tANI_U8)pMac->roam.configParam.txBFCsnValue;
		if (pIes->VHTCaps.numSoundingDim)
			txBFCsnValue = MIN(txBFCsnValue,
					pIes->VHTCaps.numSoundingDim);
        }
        *pBuf = txBFCsnValue;
        pBuf++;

        // txMuBformee
        *pBuf = (tANI_U8)pMac->roam.configParam.txMuBformee;
        pBuf++;

        // enableVhtpAid
        *pBuf = (tANI_U8)pMac->roam.configParam.enableVhtpAid;
        pBuf++;

        // enableVhtGid
        *pBuf = (tANI_U8)pMac->roam.configParam.enableVhtGid;
        pBuf++;

#endif
        // enableAmpduPs
        *pBuf = (tANI_U8)pMac->roam.configParam.enableAmpduPs;
        pBuf++;

        // enableHtSmps
        *pBuf = (tANI_U8)pMac->roam.configParam.enableHtSmps;
        pBuf++;

        // htSmps
        *pBuf = (tANI_U8)pMac->roam.configParam.htSmps;
        pBuf++;

        *pBuf = (tANI_U8)pMac->roam.configParam.max_amsdu_num;
        pBuf++;

        // WME
        if(pMac->roam.roamSession[sessionId].fWMMConnection)
        {
           //WME  enabled
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }

        // QOS
        if(pMac->roam.roamSession[sessionId].fQOSConnection)
        {
            //QOS  enabled
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        // OSEN
        if(pProfile->bOSENAssociation)
        {
            //OSEN connection
            dwTmp = pal_cpu_to_be32(TRUE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE);
            vos_mem_copy(pBuf, &dwTmp, sizeof(tAniBool));
            pBuf += sizeof(tAniBool);
        }
        /* Fill rrm config parameters */
        vos_mem_copy(pBuf, &pMac->rrm.rrmSmeContext.rrmConfig,
                     sizeof(struct rrm_config_param));
        pBuf += sizeof(struct rrm_config_param);

        //BssDesc
        csrPrepareJoinReassocReqBuffer( pMac, pBssDescription, pBuf,
                (tANI_U8)pProfile->uapsd_mask);

        /*
         * conc_custom_rule1:
         * If SAP comes up first and STA comes up later then SAP
         * need to follow STA's channel in 2.4Ghz. In following if condition
         * we are adding sanity check, just to make sure that if this rule
         * is enabled then don't allow STA to connect on 5gz channel and also
         * by this time SAP's channel should be the same as STA's channel.
         */
        if (pMac->roam.configParam.conc_custom_rule1) {
            if ((0 == pMac->roam.configParam.is_sta_connection_in_5gz_enabled)
                 && CSR_IS_CHANNEL_5GHZ(pBssDescription->channelId)) {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                          FL("STA connection on 5G band is not allowed"));
                status = eHAL_STATUS_FAILURE;
                break;
            }
            if (!CSR_IS_CHANNEL_5GHZ(pBssDescription->channelId) &&
                (false == csr_is_conn_allow_2g_band(pMac,
                                    pBssDescription->channelId))) {
                status = eHAL_STATUS_FAILURE;
                break;
            }
        }

        /*
         * conc_custom_rule2:
         * If P2PGO comes up first and STA comes up later then P2PGO
         * need to follow STA's channel in 5Ghz. In following if condition
         * we are just adding sanity check to make sure that by this time
         * P2PGO's channel is same as STA's channel.
         */
        if (pMac->roam.configParam.conc_custom_rule2) {
            if (!CSR_IS_CHANNEL_24GHZ(pBssDescription->channelId) &&
                (false == csr_is_conn_allow_5g_band(pMac,
                                    pBssDescription->channelId))) {
                status = eHAL_STATUS_FAILURE;
                break;
            }
        }

        status = palSendMBMessage(pMac->hHdd, pMsg );
        if(!HAL_STATUS_SUCCESS(status))
        {
            /*
             * palSendMBMessage would've released the memory allocated by pMsg.
             * Let's make it defensive by assigning NULL to the pointer.
             */
            pMsg = NULL;
            break;
        }
        else
        {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
            if (eWNI_SME_JOIN_REQ == messageType)
            {
                //Tush-QoS: notify QoS module that join happening
                pSession->join_bssid_count++;
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                          "BSSID Count = %d", pSession->join_bssid_count);
                sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_JOIN_REQ, NULL);
            }
            else if (eWNI_SME_REASSOC_REQ == messageType)
            {
                //Tush-QoS: notify QoS module that reassoc happening
                sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_REASSOC_REQ, NULL);
            }
#endif
        }
    } while( 0 );

    /* Clean up the memory in case of any failure */
    if (!HAL_STATUS_SUCCESS(status) && (NULL != pMsg)) {
        vos_mem_free(pMsg);
    }
    return( status );
}

//
eHalStatus csrSendMBDisassocReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr bssId, tANI_U16 reasonCode )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDisassocReq *pMsg;
    tANI_U8 *pBuf;
    tANI_U16 wTmp;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
        return eHAL_STATUS_FAILURE;
    do {
        pMsg = vos_mem_malloc(sizeof(tSirSmeDisassocReq));
        if (NULL == pMsg)
              status = eHAL_STATUS_FAILURE;
        else
              status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeDisassocReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DISASSOC_REQ);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDisassocReq ));
        pBuf = &pMsg->sessionId;
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);

        if ( (pSession->pCurRoamProfile != NULL) &&
             ((CSR_IS_INFRA_AP(pSession->pCurRoamProfile)) ||
              (CSR_IS_WDS_AP(pSession->pCurRoamProfile))) )
        {
            // Set the bssid address before sending the message to LIM
            vos_mem_copy((tSirMacAddr *)pBuf, pSession->selfMacAddr,
                         sizeof( tSirMacAddr ));
            status = eHAL_STATUS_SUCCESS;
            pBuf = pBuf + sizeof ( tSirMacAddr );
            // Set the peer MAC address before sending the message to LIM
            vos_mem_copy((tSirMacAddr *)pBuf, bssId, sizeof( tSirMacAddr ));
            //perMacAddr is passed as bssId for softAP
            status = eHAL_STATUS_SUCCESS;
            pBuf = pBuf + sizeof ( tSirMacAddr );
        }
        else
        {
            // Set the peer MAC address before sending the message to LIM
            vos_mem_copy((tSirMacAddr *)pBuf, bssId, sizeof( tSirMacAddr ));
            status = eHAL_STATUS_SUCCESS;
            pBuf = pBuf + sizeof ( tSirMacAddr );
            vos_mem_copy((tSirMacAddr *)pBuf, bssId, sizeof( pMsg->bssId ));
            status = eHAL_STATUS_SUCCESS;
            pBuf = pBuf + sizeof ( tSirMacAddr );
        }
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        // reasonCode
        wTmp = pal_cpu_to_be16(reasonCode);
        vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        pBuf += sizeof(tANI_U16);
        /* The state will be DISASSOC_HANDOFF only when we are doing handoff.
                    Here we should not send the disassoc over the air to the AP */
        if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)
#ifdef WLAN_FEATURE_VOWIFI_11R
                && csrRoamIs11rAssoc(pMac, sessionId)
#endif
           )
        {
            *pBuf = CSR_DONT_SEND_DISASSOC_OVER_THE_AIR;  /* Set DoNotSendOverTheAir flag to 1 only for handoff case */
        }
        pBuf += sizeof(tANI_U8);
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}
eHalStatus csrSendMBTkipCounterMeasuresReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_BOOLEAN bEnable, tSirMacAddr bssId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeTkipCntrMeasReq *pMsg;
    tANI_U8 *pBuf;
    do
    {
        pMsg = vos_mem_malloc(sizeof( tSirSmeTkipCntrMeasReq ));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeTkipCntrMeasReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_TKIP_CNTR_MEAS_REQ);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeTkipCntrMeasReq ));
        pBuf = &pMsg->sessionId;
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
        // bssid
        vos_mem_copy(pMsg->bssId, bssId, sizeof( tSirMacAddr ));
        status = eHAL_STATUS_SUCCESS;
        pBuf = pBuf + sizeof ( tSirMacAddr );
        // bEnable
        *pBuf = (tANI_BOOLEAN)bEnable;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}
eHalStatus
csrSendMBGetAssociatedStasReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                    VOS_MODULE_ID modId, tSirMacAddr bssId,
                                    void *pUsrContext, void *pfnSapEventCallback,
                                    tANI_U8 *pAssocStasBuf )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeGetAssocSTAsReq *pMsg;
    tANI_U8 *pBuf = NULL, *wTmpBuf = NULL;
    tANI_U32 dwTmp;
    do
    {
        pMsg = vos_mem_malloc(sizeof( tSirSmeGetAssocSTAsReq ));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if (!HAL_STATUS_SUCCESS(status)) break;
        vos_mem_set(pMsg, sizeof( tSirSmeGetAssocSTAsReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_ASSOC_STAS_REQ);
        pBuf = (tANI_U8 *)&pMsg->bssId;
        wTmpBuf = pBuf;
        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, bssId, sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // modId
        dwTmp = pal_cpu_to_be16((tANI_U16)modId);
        vos_mem_copy(pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U16));
        pBuf += sizeof(tANI_U16);
        // pUsrContext
        vos_mem_copy(pBuf, (tANI_U8 *)pUsrContext, sizeof(void *));
        pBuf += sizeof(void*);
        // pfnSapEventCallback
        vos_mem_copy(pBuf, (tANI_U8 *)pfnSapEventCallback, sizeof(void*));
        pBuf += sizeof(void*);
        // pAssocStasBuf
        vos_mem_copy(pBuf, pAssocStasBuf, sizeof(void*));
        pBuf += sizeof(void*);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf)));//msg_header + msg
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
        }
eHalStatus
csrSendMBGetWPSPBCSessions( tpAniSirGlobal pMac, tANI_U32 sessionId,
                            tSirMacAddr bssId, void *pUsrContext, void *pfnSapEventCallback,v_MACADDR_t pRemoveMac)
        {
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeGetWPSPBCSessionsReq *pMsg;
    tANI_U8 *pBuf = NULL, *wTmpBuf = NULL;

    do
        {
        pMsg = vos_mem_malloc(sizeof(tSirSmeGetWPSPBCSessionsReq));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if (!HAL_STATUS_SUCCESS(status)) break;
        vos_mem_set(pMsg, sizeof( tSirSmeGetWPSPBCSessionsReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_WPSPBC_SESSION_REQ);
        pBuf = (tANI_U8 *)&pMsg->pUsrContext;

        if( NULL == pBuf)
        {
           VOS_ASSERT(pBuf);
           return eHAL_STATUS_FAILURE;
        }
        wTmpBuf = pBuf;
        // pUsrContext
        vos_mem_copy(pBuf, (tANI_U8 *)pUsrContext, sizeof(void*));
        pBuf += sizeof(void *);
        // pSapEventCallback
        vos_mem_copy(pBuf, (tANI_U8 *)pfnSapEventCallback, sizeof(void *));
        pBuf += sizeof(void *);
        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, bssId, sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // MAC Address of STA in WPS session
        vos_mem_copy((tSirMacAddr *)pBuf, pRemoveMac.bytes, sizeof(v_MACADDR_t));
        pBuf += sizeof(v_MACADDR_t);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf)));//msg_header + msg
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

eHalStatus
csrSendChngMCCBeaconInterval(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tpSirChangeBIParams pMsg;
    tANI_U16 len = 0;
    eHalStatus status   = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    //NO need to update the Beacon Params if update beacon parameter flag is not set
    if(!pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval )
        return eHAL_STATUS_SUCCESS;

    pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval =  eANI_BOOLEAN_FALSE;

     /* Create the message and send to lim */
     len = sizeof(tSirChangeBIParams);
     pMsg = vos_mem_malloc(len);
     if ( NULL == pMsg )
        status = eHAL_STATUS_FAILURE;
     else
        status = eHAL_STATUS_SUCCESS;
     if(HAL_STATUS_SUCCESS(status))
     {
         vos_mem_set(pMsg, sizeof(tSirChangeBIParams), 0);
         pMsg->messageType     = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
         pMsg->length          = len;

        // bssId
        vos_mem_copy((tSirMacAddr *)pMsg->bssId, &pSession->selfMacAddr,
                     sizeof(tSirMacAddr));
        smsLog( pMac, LOG1, FL("CSR Attempting to change BI for Bssid= "MAC_ADDRESS_STR),
               MAC_ADDR_ARRAY(pMsg->bssId));
        pMsg->sessionId       = sessionId;
        smsLog(pMac, LOG1, FL("  session %d BeaconInterval %d"), sessionId, pMac->roam.roamSession[sessionId].bssParams.beaconInterval);
        pMsg->beaconInterval = pMac->roam.roamSession[sessionId].bssParams.beaconInterval;
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }
     return status;
}

#ifdef QCA_HT_2040_COEX
eHalStatus csrSetHT2040Mode(tpAniSirGlobal pMac, tANI_U32 sessionId,
                           ePhyChanBondState cbMode, tANI_BOOLEAN obssEnabled)
{
    tpSirSetHT2040Mode pMsg;
    tANI_U16 len = 0;
    eHalStatus status   = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

     /* Create the message and send to lim */
     len = sizeof(tSirSetHT2040Mode);
     pMsg = vos_mem_malloc(len);
     if ( NULL == pMsg )
        status = eHAL_STATUS_FAILURE;
     else
        status = eHAL_STATUS_SUCCESS;
     if(HAL_STATUS_SUCCESS(status))
     {
         vos_mem_set(pMsg, sizeof(tSirSetHT2040Mode), 0);
         pMsg->messageType     = eWNI_SME_SET_HT_2040_MODE;
         pMsg->length          = len;

        // bssId
        vos_mem_copy((tSirMacAddr *)pMsg->bssId, &pSession->selfMacAddr,
                     sizeof(tSirMacAddr));
        smsLog( pMac, LOG1, FL("CSR Attempting to set HT20/40 mode for Bssid= "MAC_ADDRESS_STR),
               MAC_ADDR_ARRAY(pMsg->bssId));
        pMsg->sessionId       = sessionId;
        smsLog(pMac, LOG1, FL("  session %d HT20/40 mode %d"), sessionId, cbMode);
        pMsg->cbMode = cbMode;
        pMsg->obssEnabled = obssEnabled;
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }
     return status;
}
#endif

eHalStatus csrSendMBDeauthReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr bssId, tANI_U16 reasonCode )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDeauthReq *pMsg;
    tANI_U8 *pBuf;
    tANI_U16 wTmp;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
        return eHAL_STATUS_FAILURE;
    do {
        pMsg = vos_mem_malloc(sizeof( tSirSmeDeauthReq ));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeDeauthReq ), 0);
                pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DEAUTH_REQ);
                pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDeauthReq ));
        //sessionId
        pBuf = &pMsg->sessionId;
        *pBuf++ = (tANI_U8)sessionId;

        //tansactionId
        *pBuf = 0;
        *(pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
        if ((pSession->pCurRoamProfile != NULL)  && (
             (CSR_IS_INFRA_AP(pSession->pCurRoamProfile)) ||
             (CSR_IS_WDS_AP(pSession->pCurRoamProfile)))){
            // Set the BSSID before sending the message to LIM
            vos_mem_copy( (tSirMacAddr *)pBuf, pSession->selfMacAddr,
                           sizeof( pMsg->peerMacAddr ) );
            status = eHAL_STATUS_SUCCESS;
            pBuf =  pBuf + sizeof(tSirMacAddr);
        }
        else
        {
            // Set the BSSID before sending the message to LIM
            vos_mem_copy( (tSirMacAddr *)pBuf, bssId, sizeof( pMsg->peerMacAddr ) );
            status = eHAL_STATUS_SUCCESS;
            pBuf =  pBuf + sizeof(tSirMacAddr);
        }
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
                // Set the peer MAC address before sending the message to LIM
        vos_mem_copy( (tSirMacAddr *) pBuf, bssId, sizeof( pMsg->peerMacAddr ) );
        status = eHAL_STATUS_SUCCESS;
        pBuf =  pBuf + sizeof(tSirMacAddr);
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        wTmp = pal_cpu_to_be16(reasonCode);
        vos_mem_copy( pBuf, &wTmp,sizeof( tANI_U16 ) );
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

eHalStatus csrSendMBDisassocCnfMsg( tpAniSirGlobal pMac, tpSirSmeDisassocInd pDisassocInd )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDisassocCnf *pMsg;
    do {
        pMsg = vos_mem_malloc(sizeof( tSirSmeDisassocCnf ));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeDisassocCnf), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DISASSOC_CNF);
        pMsg->statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDisassocCnf ));
        vos_mem_copy(pMsg->peerMacAddr, pDisassocInd->peerMacAddr,
                     sizeof(pMsg->peerMacAddr));
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }

        vos_mem_copy(pMsg->bssId, pDisassocInd->bssId, sizeof(pMsg->peerMacAddr));
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }

        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

eHalStatus csrSendMBDeauthCnfMsg( tpAniSirGlobal pMac, tpSirSmeDeauthInd pDeauthInd )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDeauthCnf *pMsg;
    do {
        pMsg = vos_mem_malloc(sizeof( tSirSmeDeauthCnf ));
        if ( NULL == pMsg )
                status = eHAL_STATUS_FAILURE;
        else
                status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeDeauthCnf ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DEAUTH_CNF);
        pMsg->statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDeauthCnf ));
        vos_mem_copy(pMsg->bssId, pDeauthInd->bssId, sizeof(pMsg->bssId));
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        vos_mem_copy(pMsg->peerMacAddr, pDeauthInd->peerMacAddr,
                     sizeof(pMsg->peerMacAddr));
        status = eHAL_STATUS_SUCCESS;
        if(!HAL_STATUS_SUCCESS(status))
        {
            vos_mem_free(pMsg);
            break;
        }
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}
eHalStatus csrSendAssocCnfMsg( tpAniSirGlobal pMac, tpSirSmeAssocInd pAssocInd, eHalStatus Halstatus )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeAssocCnf *pMsg;
    tANI_U8 *pBuf;
    tSirResultCodes statusCode;
    tANI_U16 wTmp;
    smsLog( pMac, LOG1, FL("Posting eWNI_SME_ASSOC_CNF to LIM. "
                           "HalStatus : %d"),
                            Halstatus);
    do {
        pMsg = vos_mem_malloc(sizeof( tSirSmeAssocCnf ));
        if ( NULL == pMsg )
            status = eHAL_STATUS_FAILURE;
        else
            status = eHAL_STATUS_SUCCESS;
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        vos_mem_set(pMsg, sizeof( tSirSmeAssocCnf ), 0);
                pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_ASSOC_CNF);
                pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeAssocCnf ));
        pBuf = (tANI_U8 *)&pMsg->statusCode;
        if(HAL_STATUS_SUCCESS(Halstatus))
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        else
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_ASSOC_REFUSED);
        vos_mem_copy(pBuf, &statusCode, sizeof(tSirResultCodes));
        pBuf += sizeof(tSirResultCodes);
        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        status = eHAL_STATUS_SUCCESS;
        pBuf += sizeof (tSirMacAddr);
        // peerMacAddr
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->peerMacAddr,
                      sizeof(tSirMacAddr));
        status = eHAL_STATUS_SUCCESS;
        pBuf += sizeof (tSirMacAddr);
        // aid
        wTmp = pal_cpu_to_be16(pAssocInd->aid);
        vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
        pBuf += sizeof (tANI_U16);
        // alternateBssId
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        status = eHAL_STATUS_SUCCESS;
        pBuf += sizeof (tSirMacAddr);
        // alternateChannelId
        *pBuf = 11;
        status = palSendMBMessage( pMac->hHdd, pMsg );
        if(!HAL_STATUS_SUCCESS(status))
        {
            //pMsg is freed by palSendMBMessage
            break;
        }
    } while( 0 );
    return( status );
}
eHalStatus csrSendAssocIndToUpperLayerCnfMsg(   tpAniSirGlobal pMac,
                                                tpSirSmeAssocInd pAssocInd,
                                                eHalStatus Halstatus,
                                                tANI_U8 sessionId)
{
    tSirMsgQ            msgQ;
    tSirSmeAssocIndToUpperLayerCnf *pMsg;
    tANI_U8 *pBuf;
    tSirResultCodes statusCode;
    tANI_U16 wTmp;
    do {
        pMsg = vos_mem_malloc(sizeof( tSirSmeAssocIndToUpperLayerCnf ));
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, sizeof( tSirSmeAssocIndToUpperLayerCnf ), 0);

        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_UPPER_LAYER_ASSOC_CNF);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeAssocIndToUpperLayerCnf ));

        pMsg->sessionId = sessionId;

        pBuf = (tANI_U8 *)&pMsg->statusCode;
        if(HAL_STATUS_SUCCESS(Halstatus))
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        else
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_ASSOC_REFUSED);
        vos_mem_copy(pBuf, &statusCode, sizeof(tSirResultCodes)) ;
        pBuf += sizeof(tSirResultCodes);
        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // peerMacAddr
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->peerMacAddr,
                      sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // StaId
        wTmp = pal_cpu_to_be16(pAssocInd->staId);
        vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
        pBuf += sizeof (tANI_U16);
        // alternateBssId
        vos_mem_copy((tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // alternateChannelId
        *pBuf = 11;
        pBuf += sizeof (tANI_U8);
        // Instead of copying roam Info, we just copy only WmmEnabled, RsnIE information
        //Wmm
        *pBuf = pAssocInd->wmmEnabledSta;
        pBuf += sizeof (tANI_U8);
        //RSN IE
        vos_mem_copy((tSirRSNie *)pBuf, &pAssocInd->rsnIE, sizeof(tSirRSNie));
        pBuf += sizeof (tSirRSNie);
#ifdef FEATURE_WLAN_WAPI
        //WAPI IE
        vos_mem_copy((tSirWAPIie *)pBuf, &pAssocInd->wapiIE,
                        sizeof(tSirWAPIie));
        pBuf += sizeof (tSirWAPIie);
#endif
        //Additional IE
        vos_mem_copy((void *)pBuf, &pAssocInd->addIE, sizeof(tSirAddie));
        pBuf += sizeof (tSirAddie);
        //reassocReq
        *pBuf = pAssocInd->reassocReq;
        pBuf += sizeof (tANI_U8);
        //timingMeasCap
        *pBuf = pAssocInd->timingMeasCap;
        pBuf += sizeof (tANI_U8);
        vos_mem_copy((void *)pBuf, &pAssocInd->chan_info,
                        sizeof(tSirSmeChanInfo));
        msgQ.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
        msgQ.bodyptr = pMsg;
        msgQ.bodyval = 0;
        SysProcessMmhMsg(pMac, &msgQ);
    } while( 0 );
    return( eHAL_STATUS_SUCCESS );
}

eHalStatus csrSendMBSetContextReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId,
            tSirMacAddr peerMacAddr, tANI_U8 numKeys, tAniEdType edType,
            tANI_BOOLEAN fUnicast, tAniKeyDirection aniKeyDirection,
            tANI_U8 keyId, tANI_U8 keyLength, tANI_U8 *pKey, tANI_U8 paeRole,
            tANI_U8 *pKeyRsc )
{
    tSirSmeSetContextReq *pMsg;
    tANI_U16 msgLen;
    eHalStatus status = eHAL_STATUS_FAILURE;
    tAniEdType tmpEdType;
    tAniKeyDirection tmpDirection;
    tANI_U8 *pBuf = NULL;
    tANI_U8 *p = NULL;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    smsLog( pMac, LOG1, FL("keylength is %d, Encry type is : %d"),
                            keyLength, edType);
    do {
        if( ( 1 != numKeys ) && ( 0 != numKeys ) ) break;
        /*
         * All of these fields appear in every SET_CONTEXT message.
         * Below we'll add in the size for each key set. Since we only support
         * up to one key, we always allocate memory for 1 key.
         */
        msgLen  = sizeof( tANI_U16) + sizeof( tANI_U16 ) + sizeof( tSirMacAddr ) +
                  sizeof( tSirMacAddr ) + 1 + sizeof(tANI_U16) +
                  sizeof( pMsg->keyMaterial.length ) + sizeof( pMsg->keyMaterial.edType ) + sizeof( pMsg->keyMaterial.numKeys ) +
                  ( sizeof( pMsg->keyMaterial.key ) );

        pMsg = vos_mem_malloc(msgLen);
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, msgLen, 0);
                pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_SETCONTEXT_REQ);
                pMsg->length = pal_cpu_to_be16(msgLen);
        //sessionId
        pBuf = &pMsg->sessionId;
        *pBuf = (tANI_U8)sessionId;
        pBuf++;
        // transactionId
        *pBuf = 0;
        *(pBuf + 1) = 0;
        pBuf += sizeof(tANI_U16);
        // peerMacAddr
        vos_mem_copy(pBuf, (tANI_U8 *)peerMacAddr, sizeof(tSirMacAddr));

        pBuf += sizeof(tSirMacAddr);

        // bssId
        vos_mem_copy(pBuf, (tANI_U8 *)&pSession->connectedProfile.bssid,
                     sizeof(tSirMacAddr));

        pBuf += sizeof(tSirMacAddr);

        p = pBuf;
                // Set the pMsg->keyMaterial.length field (this length is defined as all data that follows the edType field
                // in the tSirKeyMaterial keyMaterial; field).
                //
                // !!NOTE:  This keyMaterial.length contains the length of a MAX size key, though the keyLength can be
                // shorter than this max size.  Is LIM interpreting this ok ?
                p = pal_set_U16( p, pal_cpu_to_be16((tANI_U16)( sizeof( pMsg->keyMaterial.numKeys ) + ( numKeys * sizeof( pMsg->keyMaterial.key ) ) )) );
                // set pMsg->keyMaterial.edType
        tmpEdType = (tAniEdType)pal_cpu_to_be32(edType);
        vos_mem_copy(p, (tANI_U8 *)&tmpEdType, sizeof(tAniEdType));
        p += sizeof( pMsg->keyMaterial.edType );
        // set the pMsg->keyMaterial.numKeys field
        *p = numKeys;
        p += sizeof( pMsg->keyMaterial.numKeys );
        // set pSirKey->keyId = keyId;
        *p = keyId;
        p += sizeof( pMsg->keyMaterial.key[ 0 ].keyId );
        // set pSirKey->unicast = (tANI_U8)fUnicast;
        *p = (tANI_U8)fUnicast;
        p += sizeof( pMsg->keyMaterial.key[ 0 ].unicast );
                // set pSirKey->keyDirection = aniKeyDirection;
        tmpDirection = (tAniKeyDirection)pal_cpu_to_be32(aniKeyDirection);
        vos_mem_copy(p, (tANI_U8 *)&tmpDirection, sizeof(tAniKeyDirection));
        p += sizeof(tAniKeyDirection);
        //    pSirKey->keyRsc = ;;
        vos_mem_copy(p, pKeyRsc, CSR_MAX_RSC_LEN);
        p += sizeof( pMsg->keyMaterial.key[ 0 ].keyRsc );
                // set pSirKey->paeRole
                *p = paeRole;   // 0 is Supplicant
                p++;
                // set pSirKey->keyLength = keyLength;
                p = pal_set_U16( p, pal_cpu_to_be16(keyLength) );
        if (keyLength && pKey)
        {
            vos_mem_copy(p, pKey, keyLength);
            smsLog(pMac, LOG1, FL("SME set keyIndx (%d) encType (%d) key"),
                                                keyId, edType);
            sirDumpBuf(pMac, SIR_SMS_MODULE_ID, LOG1, pKey, keyLength);
        }
        status = palSendMBMessage(pMac->hHdd, pMsg);
    } while( 0 );
    return( status );
}

eHalStatus csrSendMBStartBssReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamBssType bssType,
                                    tCsrRoamStartBssParams *pParam, tSirBssDescription *pBssDesc )
{
    eHalStatus status;
    tSirSmeStartBssReq *pMsg;
    tANI_U8 *pBuf = NULL;
    tANI_U8  *wTmpBuf  = NULL;
    tANI_U16 msgLen, wTmp;
    tANI_U32 dwTmp;
    tSirNwType nwType;
    ePhyChanBondState cbMode;
    tANI_U32 authType;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    do {
        pSession->joinFailStatusCode.statusCode = eSIR_SME_SUCCESS;
        pSession->joinFailStatusCode.reasonCode = 0;
        msgLen = sizeof(tSirSmeStartBssReq);
        pMsg = vos_mem_malloc(msgLen);
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, msgLen, 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_START_BSS_REQ);
        pBuf = &pMsg->sessionId;
        wTmpBuf = pBuf;
        //sessionId
        *pBuf = (tANI_U8)sessionId;
        pBuf++;
        // transactionId
        *pBuf = 0;
        *(pBuf + 1) = 0;
        pBuf += sizeof(tANI_U16);
        // bssid
        vos_mem_copy(pBuf, pParam->bssid, sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // selfMacAddr
        vos_mem_copy(pBuf, pSession->selfMacAddr, sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // beaconInterval
        if( pBssDesc && pBssDesc->beaconInterval )
        {
            wTmp = pal_cpu_to_be16( pBssDesc->beaconInterval );
        }
        else if(pParam->beaconInterval)
        {
            wTmp = pal_cpu_to_be16( pParam->beaconInterval );
        }
        else
        {
            wTmp = pal_cpu_to_be16( WNI_CFG_BEACON_INTERVAL_STADEF );
        }
        if(csrIsconcurrentsessionValid (pMac, sessionId,
                                   pParam->bssPersona)
                                   == eHAL_STATUS_SUCCESS )
        {
           csrValidateMCCBeaconInterval(pMac, pParam->operationChn, &wTmp, sessionId,
                                      pParam->bssPersona);
           //Update the beacon Interval
           pParam->beaconInterval = wTmp;
        }
        else
        {
            smsLog( pMac,LOGE, FL("****Start BSS failed persona already exists***"));
            status = eHAL_STATUS_FAILURE;
            vos_mem_free(pMsg);
            return status;
        }

        vos_mem_copy(pBuf, &wTmp, sizeof( tANI_U16 ));
        pBuf += sizeof(tANI_U16);
        // dot11mode
        *pBuf = (tANI_U8)csrTranslateToWNICfgDot11Mode( pMac, pParam->uCfgDot11Mode );
        pBuf += 1;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
        *pBuf = pMac->roam.configParam.cc_switch_mode;
        pBuf += 1;
#endif

        // bssType
        dwTmp = pal_cpu_to_be32( csrTranslateBsstypeToMacType( bssType ) );
        vos_mem_copy(pBuf, &dwTmp, sizeof(tSirBssType));
        pBuf += sizeof(tSirBssType);
        // ssId
        if( pParam->ssId.length )
        {
            // ssId len
            *pBuf = pParam->ssId.length;
            pBuf++;
            vos_mem_copy(pBuf, pParam->ssId.ssId, pParam->ssId.length);
            pBuf += pParam->ssId.length;
        }
        else
        {
            *pBuf = 0;
            pBuf++;
        }
        // set the channel Id
        *pBuf = pParam->operationChn;
        pBuf++;
        //What should we really do for the cbmode.
        cbMode = (ePhyChanBondState)pal_cpu_to_be32(pParam->cbMode);
        vos_mem_copy(pBuf, (tANI_U8 *)&cbMode, sizeof(ePhyChanBondState));
        pBuf += sizeof(ePhyChanBondState);
        /*set vht channel width*/
        *pBuf = pParam->vht_channel_width;
        pBuf++;

        // Set privacy
        *pBuf = pParam->privacy;
        pBuf++;

        //Set Uapsd
        *pBuf = pParam->ApUapsdEnable;
        pBuf++;
        //Set SSID hidden
        *pBuf = pParam->ssidHidden;
        pBuf++;
        *pBuf = (tANI_U8)pParam->fwdWPSPBCProbeReq;
        pBuf++;

        //Ht protection Enable/Disable
        *pBuf = (tANI_U8)pParam->protEnabled;
        pBuf++;
        //Enable Beacons to Receive for OBSS protection Enable/Disable
        *pBuf = (tANI_U8)pParam->obssProtEnabled;
        pBuf++;
        //set cfg related to protection
        wTmp = pal_cpu_to_be16( pParam->ht_protection );
        vos_mem_copy(pBuf, &wTmp, sizeof( tANI_U16 ));
        pBuf += sizeof(tANI_U16);
        // Set Auth type
        authType = pal_cpu_to_be32(pParam->authType);
        vos_mem_copy(pBuf, (tANI_U8 *)&authType, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // Set DTIM
        dwTmp = pal_cpu_to_be32(pParam->dtimPeriod);
        vos_mem_copy(pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // Set wps_state
        *pBuf = pParam->wps_state;
        pBuf++;
        // set isCoalesingInIBSSAllowed
        *pBuf = pMac->isCoalesingInIBSSAllowed;
        pBuf++;
        //Persona
        *pBuf = (tANI_U8)pParam->bssPersona;
        pBuf++;

        //txLdpcIniFeatureEnabled
        *pBuf = (tANI_U8)(tANI_U8)pMac->roam.configParam.txLdpcEnable;
        pBuf++;

#ifdef WLAN_FEATURE_11W
        // Set MFP capable/required
        *pBuf = (tANI_U8)pParam->mfpCapable;
        pBuf++;
        *pBuf = (tANI_U8)pParam->mfpRequired;
        pBuf++;
#endif

        // set RSN IE
        if( pParam->nRSNIELength > sizeof(pMsg->rsnIE.rsnIEdata) )
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
            vos_mem_free(pMsg);
            break;
        }
        wTmp = pal_cpu_to_be16( pParam->nRSNIELength );
        vos_mem_copy(pBuf, &wTmp, sizeof(tANI_U16));
        pBuf += sizeof(tANI_U16);
        if( wTmp )
        {
            wTmp = pParam->nRSNIELength;
            vos_mem_copy(pBuf, pParam->pRSNIE, wTmp);
            pBuf += wTmp;
        }
        nwType = (tSirNwType)pal_cpu_to_be32(pParam->sirNwType);
        vos_mem_copy(pBuf, (tANI_U8 *)&nwType, sizeof(tSirNwType));
        pBuf += sizeof(tSirNwType);
        *pBuf = pParam->operationalRateSet.numRates; //tSirMacRateSet->numRates
        pBuf++;
        vos_mem_copy(pBuf, pParam->operationalRateSet.rate,
                     pParam->operationalRateSet.numRates );
        pBuf += pParam->operationalRateSet.numRates ;
        *pBuf++ = pParam->extendedRateSet.numRates;
        if(0 != pParam->extendedRateSet.numRates)
        {
            vos_mem_copy(pBuf, pParam->extendedRateSet.rate,
                         pParam->extendedRateSet.numRates);
            pBuf += pParam->extendedRateSet.numRates;
        }

        //HT Config
        vos_mem_copy(pBuf, &pSession->htConfig,
          sizeof(tSirHTConfig));
          pBuf += sizeof(tSirHTConfig);

        vos_mem_copy(pBuf, &pParam->addIeParams, sizeof( pParam->addIeParams ));
        pBuf += sizeof(pParam->addIeParams);

        *pBuf++ = (tANI_U8)pMac->roam.configParam.obssEnabled;
        *pBuf++ = (tANI_U8)pParam->sap_dot11mc;

        msgLen = (tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf)); //msg_header + msg
        pMsg->length = pal_cpu_to_be16(msgLen);

        status = palSendMBMessage(pMac->hHdd, pMsg);
    } while( 0 );
  return( status );
}

eHalStatus csrSendMBStopBssReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirSmeStopBssReq *pMsg;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U8 *pBuf;
    tANI_U16 msgLen;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    do {
        pMsg = vos_mem_malloc(sizeof(tSirSmeStopBssReq));
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, sizeof( tSirSmeStopBssReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_STOP_BSS_REQ);
        pBuf = &pMsg->sessionId;
        //sessionId
        *pBuf = (tANI_U8)sessionId;
        pBuf++;
        // transactionId
        *pBuf = 0;
        pBuf += sizeof(tANI_U16);
        //reason code
        *pBuf  = 0;
        pBuf += sizeof(tSirResultCodes);
        // bssid
        // if BSSType is WDS sta, use selfmacAddr as bssid, else use bssid in connectedProfile
        if( CSR_IS_CONN_WDS_STA(&pSession->connectedProfile) )
        {
            vos_mem_copy(pBuf, (tANI_U8 *)&pSession->selfMacAddr,
                         sizeof(tSirMacAddr));
        }
        else
        {
            vos_mem_copy(pBuf, (tANI_U8 *)&pSession->connectedProfile.bssid,
                         sizeof(tSirMacAddr));
        }
       pBuf += sizeof(tSirMacAddr);
       msgLen = sizeof(tANI_U16) + sizeof(tANI_U16) + 1 + sizeof(tANI_U16) + sizeof(tSirResultCodes) + sizeof(tSirMacAddr);
       pMsg->length =  pal_cpu_to_be16(msgLen);
       status =  palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

eHalStatus csrReassoc(tpAniSirGlobal pMac, tANI_U32 sessionId,
                      tCsrRoamModifyProfileFields *pModProfileFields,
                      tANI_U32 *pRoamId, v_BOOL_t fForce)
{
   eHalStatus status = eHAL_STATUS_FAILURE;
   tANI_U32 roamId = 0;
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
   if((csrIsConnStateConnected(pMac, sessionId)) &&
      (fForce || (!vos_mem_compare( &pModProfileFields,
                  &pSession->connectedProfile.modifyProfileFields,
                  sizeof(tCsrRoamModifyProfileFields)))) )
   {
      roamId = GET_NEXT_ROAM_ID(&pMac->roam);
      if(pRoamId)
      {
         *pRoamId = roamId;
      }

      status = csrRoamIssueReassoc(pMac, sessionId, NULL, pModProfileFields,
                                   eCsrSmeIssuedReassocToSameAP, roamId,
                                   eANI_BOOLEAN_FALSE);
   }
   return status;
}
static eHalStatus csrRoamSessionOpened(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo roamInfo;
    vos_mem_set(&roamInfo, sizeof(tCsrRoamInfo), 0);
    status = csrRoamCallCallback(pMac, sessionId, &roamInfo, 0,
                            eCSR_ROAM_SESSION_OPENED, eCSR_ROAM_RESULT_NONE);
    return (status);
}
eHalStatus csrProcessAddStaSessionRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg)
{
   eHalStatus                         status = eHAL_STATUS_SUCCESS;
   tListElem                          *pEntry = NULL;
   tSmeCmd                            *pCommand = NULL;
   tSirSmeAddStaSelfRsp               *pRsp;
   do
   {
      if(pMsg == NULL)
      {
         smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
      pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
      if(pEntry)
      {
         pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
         if(eSmeCommandAddStaSession == pCommand->command)
         {
            pRsp = (tSirSmeAddStaSelfRsp*)pMsg;
            smsLog( pMac, LOG1, "Add Sta rsp status = %d", pRsp->status );
            //Nothing to be done. May be indicate the self sta addition success by calling session callback (TODO).
            csrRoamSessionOpened(pMac, pCommand->sessionId);
            //Remove this command out of the active list
            if(csrLLRemoveEntry(&pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK))
            {
               /* Now put this command back on the available command list */
               csrReleaseCommand(pMac, pCommand);
            }
            smeProcessPendingQueue( pMac );
         }
         else
         {
            smsLog(pMac, LOGE, "in %s eWNI_SME_ADD_STA_SELF_RSP Received but NO Add sta session command are ACTIVE ...",
                  __func__);
            status = eHAL_STATUS_FAILURE;
            break;
         }
      }
      else
      {
         smsLog(pMac, LOGE, "in %s eWNI_SME_ADD_STA_SELF_RSP Received but NO commands are ACTIVE ...",
               __func__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
   } while(0);
   return status;
}
/**
 * csr_get_vdev_type_nss() - gets the nss value based on vdev type
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @dev_mode: current device operating mode.
 * @nss2g: Pointer to the 2G Nss parameter.
 * @nss5g: Pointer to the 5G Nss parameter.
 *
 * Fills the 2G and 5G Nss values based on device mode.
 *
 * Return: None
 */
void csr_get_vdev_type_nss(tpAniSirGlobal mac_ctx, tVOS_CON_MODE dev_mode,
		uint8_t *nss_2g, uint8_t *nss_5g)
{
	switch (dev_mode) {
	case VOS_STA_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sta;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sta;
		break;
	case VOS_STA_SAP_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sap;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sap;
		break;
	case VOS_P2P_CLIENT_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_cli;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_cli;
		break;
	case VOS_P2P_GO_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_go;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_go;
		break;
	case VOS_P2P_DEVICE_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_dev;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_dev;
		break;
	case VOS_IBSS_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ibss;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ibss;
		break;
	case VOS_OCB_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ocb;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ocb;
		break;
	default:
		*nss_2g = 2;
		*nss_5g = 2;
		break;
	}
	smsLog(mac_ctx, LOG1, FL("mode - %d: nss_2g - %d, 5g - %d"),
			dev_mode, *nss_2g, *nss_5g);
}
eHalStatus csrSendMBAddSelfStaReqMsg( tpAniSirGlobal pMac,
                                      tAddStaForSessionCmd *pAddStaReq,
                                      tANI_U8 sessionId)
{
   tSirSmeAddStaSelfReq *pMsg;
   tANI_U16 msgLen;
   eHalStatus status = eHAL_STATUS_FAILURE;
   uint8_t nss_2g;
   uint8_t nss_5g;
   do {
      msgLen  = sizeof(tSirSmeAddStaSelfReq);
      pMsg = vos_mem_malloc(msgLen);
      if ( NULL == pMsg ) break;
      vos_mem_set(pMsg, msgLen, 0);
      csr_get_vdev_type_nss(pMac, pAddStaReq->currDeviceMode, &nss_2g, &nss_5g);
      pMsg->mesgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_ADD_STA_SELF_REQ);
      pMsg->mesgLen = pal_cpu_to_be16(msgLen);
      // self station address
      vos_mem_copy((tANI_U8 *)pMsg->selfMacAddr,
                      (tANI_U8 *)&pAddStaReq->selfMacAddr, sizeof(tSirMacAddr));

      pMsg->currDeviceMode = pAddStaReq->currDeviceMode;
      pMsg->type = pAddStaReq->type;
      pMsg->subType = pAddStaReq->subType;
      pMsg->sessionId = sessionId;
      pMsg->pkt_err_disconn_th = pMac->roam.configParam.pkt_err_disconn_th;
      pMsg->nss_2g = nss_2g;
      pMsg->nss_5g = nss_5g;
      smsLog( pMac, LOG1, FL("selfMac="MAC_ADDRESS_STR),
              MAC_ADDR_ARRAY(pMsg->selfMacAddr));
      status = palSendMBMessage(pMac->hHdd, pMsg);
   } while( 0 );
   return( status );
}

eHalStatus csrIssueAddStaForSessionReq(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                       tSirMacAddr sessionMacAddr,
                                       tANI_U32 type, tANI_U32 subType)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tSmeCmd *pCommand;
   pCommand = csrGetCommandBuffer(pMac);
   if(NULL == pCommand)
   {
      status = eHAL_STATUS_RESOURCES;
   }
   else
   {
      pCommand->command = eSmeCommandAddStaSession;
      pCommand->sessionId = (tANI_U8)sessionId;
      vos_mem_copy(pCommand->u.addStaSessionCmd.selfMacAddr, sessionMacAddr,
                   sizeof( tSirMacAddr ) );
      pCommand->u.addStaSessionCmd.currDeviceMode = pMac->sme.currDeviceMode;
      pCommand->u.addStaSessionCmd.type = type;
      pCommand->u.addStaSessionCmd.subType = subType;
      status = csrQueueSmeCommand(pMac, pCommand, TRUE);
      if( !HAL_STATUS_SUCCESS( status ) )
      {
         //Should be panic??
         smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
      }
   }
   return (status);
}
eHalStatus csrProcessAddStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
   return csrSendMBAddSelfStaReqMsg(pMac,
         &pCommand->u.addStaSessionCmd,
         pCommand->sessionId);
}
eHalStatus csrRoamOpenSession(tpAniSirGlobal pMac,
                              csrRoamCompleteCallback callback,
                              void *pContext,
                              tANI_U8 *pSelfMacAddr, tANI_U8 *pbSessionId,
                              tANI_U32 type, tANI_U32 subType )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 i, value = 0;
    union {
       tANI_U16                        nCfgValue16;
       tSirMacHTCapabilityInfo         htCapInfo;
    }uHTCapabilityInfo;
    tCsrRoamSession *pSession;
    *pbSessionId = CSR_SESSION_ID_INVALID;

    for( i = 0; i < pMac->sme.max_intf_count; i++ )
    {
        if( !CSR_IS_SESSION_VALID( pMac, i ) )
        {
            pSession = CSR_GET_SESSION( pMac, i );
            if (!pSession)
            {
                smsLog(pMac, LOGE,
                       FL("Session does not exist for interface %d"), i);
                break;
            }
            status = eHAL_STATUS_SUCCESS;
            pSession->sessionActive = eANI_BOOLEAN_TRUE;
            pSession->sessionId = (tANI_U8)i;

#ifdef WLAN_FEATURE_VOWIFI_11R
            /* Initialize FT related data structures only in STA mode */
            sme_FTOpen(pMac, pSession->sessionId);
#endif

            pSession->callback = callback;
            pSession->pContext = pContext;
            vos_mem_copy(&pSession->selfMacAddr, pSelfMacAddr,
                         sizeof(tCsrBssid));
            *pbSessionId = (tANI_U8)i;
            status = vos_timer_init(&pSession->hTimerRoaming, VOS_TIMER_TYPE_SW,
                                    csrRoamRoamingTimerHandler,
                                    &pSession->roamingTimerInfo);
            if (!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE,
                       FL("cannot allocate memory for Roaming timer"));
                break;
            }
            /* get the HT capability info*/
            status = ccmCfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &value);
            if (!HAL_STATUS_SUCCESS(status)) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                          "%s: could not get HT capability info",
                          __func__);
                break;
            }

            uHTCapabilityInfo.nCfgValue16 = 0xFFFF & value;
            pSession->htConfig.ht_rx_ldpc =
                                       uHTCapabilityInfo.htCapInfo.advCodingCap;
            pSession->htConfig.ht_tx_stbc = uHTCapabilityInfo.htCapInfo.txSTBC;
            pSession->htConfig.ht_rx_stbc = uHTCapabilityInfo.htCapInfo.rxSTBC;
            pSession->htConfig.ht_sgi = VOS_TRUE;
            status = csrIssueAddStaForSessionReq ( pMac, i, pSelfMacAddr, type,
                                                   subType );
            break;
        }
    }
    if( pMac->sme.max_intf_count == i )
    {
        //No session is available
        smsLog(pMac, LOGE,
               "%s: Reached max interfaces: %d! Session creation will fail",
               __func__, pMac->sme.max_intf_count);
        status = eHAL_STATUS_RESOURCES;
    }
    return ( status );
}

eHalStatus csrProcessDelStaSessionRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg)
{
   eHalStatus                         status = eHAL_STATUS_SUCCESS;
   tListElem                          *pEntry = NULL;
   tSmeCmd                            *pCommand = NULL;
   tSirSmeDelStaSelfRsp               *pRsp;
   do
   {
      if(pMsg == NULL)
      {
         smsLog(pMac, LOGE, "in %s msg ptr is NULL", __func__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
      pEntry = csrLLPeekHead( &pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK );
      if(pEntry)
      {
         pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
         if(eSmeCommandDelStaSession == pCommand->command)
         {
            tANI_U8 sessionId = pCommand->sessionId;
            pRsp = (tSirSmeDelStaSelfRsp*)pMsg;
            smsLog( pMac, LOG1, "Del Sta rsp status = %d", pRsp->status );
            //This session is done.
            csrCleanupSession(pMac, sessionId);
            if(pCommand->u.delStaSessionCmd.callback)
            {

                status = sme_ReleaseGlobalLock( &pMac->sme );
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    pCommand->u.delStaSessionCmd.callback(
                                      pCommand->u.delStaSessionCmd.pContext);
                    status = sme_AcquireGlobalLock( &pMac->sme );
                    if (! HAL_STATUS_SUCCESS( status ) )
                    {
                        smsLog(pMac, LOGP, "%s: Failed to Acquire Lock", __func__);
                        return status;
                    }
                }
                else {
                    smsLog(pMac, LOGE, "%s: Failed to Release Lock", __func__);
                }
            }

            //Remove this command out of the active list
            if(csrLLRemoveEntry(&pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK))
            {
               /* Now put this command back on the available command list */
               csrReleaseCommand(pMac, pCommand);
            }
            smeProcessPendingQueue( pMac );
         }
         else
         {
            smsLog(pMac, LOGE, "in %s eWNI_SME_DEL_STA_SELF_RSP Received but NO Del sta session command are ACTIVE ...",
                  __func__);
            status = eHAL_STATUS_FAILURE;
            break;
         }
      }
      else
      {
         smsLog(pMac, LOGE, "in %s eWNI_SME_DEL_STA_SELF_RSP Received but NO commands are ACTIVE ...",
               __func__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
   } while(0);
   return status;
}
eHalStatus csrSendMBDelSelfStaReqMsg( tpAniSirGlobal pMac, tSirMacAddr macAddr,
                                      tANI_U8 sessionId)
{
   tSirSmeDelStaSelfReq *pMsg;
   tANI_U16 msgLen;
   eHalStatus status = eHAL_STATUS_FAILURE;
   do {
      msgLen  = sizeof(tSirSmeDelStaSelfReq);
      pMsg = vos_mem_malloc(msgLen);
      if (NULL == pMsg)
			return eHAL_STATUS_FAILURE;
      vos_mem_set(pMsg, msgLen, 0);

      pMsg->mesgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DEL_STA_SELF_REQ);
      pMsg->mesgLen = pal_cpu_to_be16(msgLen);
      pMsg->sessionId = sessionId;
      // self station address
      vos_mem_copy((tANI_U8 *)pMsg->selfMacAddr, (tANI_U8 *)macAddr,
                   sizeof(tSirMacAddr));
      status = palSendMBMessage(pMac->hHdd, pMsg);
   } while( 0 );
   return( status );
}
eHalStatus csrIssueDelStaForSessionReq(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                       tSirMacAddr sessionMacAddr,
                                       csrRoamSessionCloseCallback callback,
                                       void *pContext)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
   tSmeCmd *pCommand;
   pCommand = csrGetCommandBuffer(pMac);
   if(NULL == pCommand)
   {
      status = eHAL_STATUS_RESOURCES;
   }
   else
   {
      pCommand->command = eSmeCommandDelStaSession;
      pCommand->sessionId = (tANI_U8)sessionId;
      pCommand->u.delStaSessionCmd.callback = callback;
      pCommand->u.delStaSessionCmd.pContext = pContext;
      vos_mem_copy(pCommand->u.delStaSessionCmd.selfMacAddr, sessionMacAddr,
                   sizeof( tSirMacAddr ));
      status = csrQueueSmeCommand(pMac, pCommand, TRUE);
      if( !HAL_STATUS_SUCCESS( status ) )
      {
         //Should be panic??
         smsLog( pMac, LOGE, FL(" fail to send message status = %d"), status );
      }
   }
   return (status);
}
eHalStatus csrProcessDelStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
   return csrSendMBDelSelfStaReqMsg( pMac,
         pCommand->u.delStaSessionCmd.selfMacAddr,
         (tANI_U8)pCommand->sessionId);
}
static void purgeCsrSessionCmdList(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tDblLinkList *pList = &pMac->roam.roamCmdPendingList;
    tListElem *pEntry, *pNext;
    tSmeCmd *pCommand;
    tDblLinkList localList;

    vos_mem_zero(&localList, sizeof(tDblLinkList));
    if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &localList)))
    {
        smsLog(pMac, LOGE, FL(" failed to open list"));
        return;
    }
    csrLLLock(pList);
    pEntry = csrLLPeekHead(pList, LL_ACCESS_NOLOCK);
    while(pEntry != NULL)
    {
        pNext = csrLLNext(pList, pEntry, LL_ACCESS_NOLOCK);
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        if(pCommand->sessionId == sessionId)
        {
            if(csrLLRemoveEntry(pList, pEntry, LL_ACCESS_NOLOCK))
            {
                csrLLInsertTail(&localList, pEntry, LL_ACCESS_NOLOCK);
            }
        }
        pEntry = pNext;
    }
    csrLLUnlock(pList);

    while( (pEntry = csrLLRemoveHead(&localList, LL_ACCESS_NOLOCK)) )
    {
        pCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        csrAbortCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
    }
    csrLLClose(&localList);
}

void csrCleanupSession(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
    {
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

        csrRoamStop(pMac, sessionId);

        /* Clean up FT related data structures */
#if defined WLAN_FEATURE_VOWIFI_11R
        sme_FTClose(pMac, sessionId);
#endif
        csrFreeConnectBssDesc(pMac, sessionId);
        csrRoamFreeConnectProfile( pMac, &pSession->connectedProfile );
        csrRoamFreeConnectedInfo ( pMac, &pSession->connectedInfo);
        vos_timer_destroy(&pSession->hTimerRoaming);
        purgeSmeSessionCmdList(pMac, sessionId, &pMac->sme.smeCmdPendingList);
        if (pMac->fScanOffload)
        {
            purgeSmeSessionCmdList(pMac, sessionId,
                    &pMac->sme.smeScanCmdPendingList);
        }

        purgeCsrSessionCmdList(pMac, sessionId);
        csrInitSession(pMac, sessionId);
    }
}

eHalStatus csrRoamCloseSession( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                tANI_BOOLEAN fSync,
                                csrRoamSessionCloseCallback callback,
                                void *pContext )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
    {
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
        if(fSync)
        {
            csrCleanupSession(pMac, sessionId);
        }
        else
        {
            purgeSmeSessionCmdList(pMac, sessionId,
                    &pMac->sme.smeCmdPendingList);
            if (pMac->fScanOffload)
            {
                purgeSmeSessionCmdList(pMac, sessionId,
                        &pMac->sme.smeScanCmdPendingList);
            }
            purgeCsrSessionCmdList(pMac, sessionId);
            status = csrIssueDelStaForSessionReq( pMac, sessionId,
                                        pSession->selfMacAddr, callback, pContext);
        }
    }
    else
    {
        status = eHAL_STATUS_INVALID_PARAMETER;
    }
    return ( status );
}

static void csrInitSession( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }

    pSession->sessionActive = eANI_BOOLEAN_FALSE;
    pSession->sessionId = CSR_SESSION_ID_INVALID;
    pSession->callback = NULL;
    pSession->pContext = NULL;
    pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
    csrFreeRoamProfile( pMac, sessionId );
    csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
    csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
    csrFreeConnectBssDesc(pMac, sessionId);
    csrScanEnable(pMac);
    vos_mem_set(&pSession->selfMacAddr, sizeof(tCsrBssid), 0);
    if (pSession->pWpaRsnReqIE)
    {
        vos_mem_free(pSession->pWpaRsnReqIE);
        pSession->pWpaRsnReqIE = NULL;
    }
    pSession->nWpaRsnReqIeLength = 0;
    if (pSession->pWpaRsnRspIE)
    {
        vos_mem_free(pSession->pWpaRsnRspIE);
        pSession->pWpaRsnRspIE = NULL;
    }
    pSession->nWpaRsnRspIeLength = 0;
#ifdef FEATURE_WLAN_WAPI
    if (pSession->pWapiReqIE)
    {
        vos_mem_free(pSession->pWapiReqIE);
        pSession->pWapiReqIE = NULL;
    }
    pSession->nWapiReqIeLength = 0;
    if (pSession->pWapiRspIE)
    {
        vos_mem_free(pSession->pWapiRspIE);
        pSession->pWapiRspIE = NULL;
    }
    pSession->nWapiRspIeLength = 0;
#endif /* FEATURE_WLAN_WAPI */
    if (pSession->pAddIEScan)
    {
        vos_mem_free(pSession->pAddIEScan);
        pSession->pAddIEScan = NULL;
    }
    pSession->nAddIEScanLength = 0;
    if (pSession->pAddIEAssoc)
    {
        vos_mem_free(pSession->pAddIEAssoc);
        pSession->pAddIEAssoc = NULL;
    }
    pSession->nAddIEAssocLength = 0;
}

eHalStatus csrRoamGetSessionIdFromBSSID( tpAniSirGlobal pMac, tCsrBssid *bssid, tANI_U32 *pSessionId )
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tANI_U32 i;
    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) )
        {
            if( csrIsMacAddressEqual( pMac, bssid, &pMac->roam.roamSession[i].connectedProfile.bssid ) )
            {
                //Found it
                status = eHAL_STATUS_SUCCESS;
                *pSessionId = i;
                break;
            }
        }
    }
    return( status );
}

//This function assumes that we only support one IBSS session. We cannot use BSSID to identify
//session because for IBSS, the bssid changes.
static tANI_U32 csrFindIbssSession( tpAniSirGlobal pMac )
{
    tANI_U32 i, nRet = CSR_SESSION_ID_INVALID;
    tCsrRoamSession *pSession;
    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) )
        {
            pSession = CSR_GET_SESSION( pMac, i );
            if( pSession->pCurRoamProfile && ( csrIsBssTypeIBSS( pSession->connectedProfile.BSSType ) ) )
            {
                //Found it
                nRet = i;
                break;
            }
        }
    }
    return (nRet);
}
static void csrRoamLinkUp(tpAniSirGlobal pMac, tCsrBssid bssid)
{
   /* Update the current BSS info in ho control block based on connected
      profile info from pmac global structure                              */

   smsLog(pMac, LOGW, " csrRoamLinkUp: WLAN link UP with AP= "MAC_ADDRESS_STR,
          MAC_ADDR_ARRAY(bssid));
   /* Check for user misconfig of RSSI trigger threshold                  */
   pMac->roam.configParam.vccRssiThreshold =
      ( 0 == pMac->roam.configParam.vccRssiThreshold ) ?
      CSR_VCC_RSSI_THRESHOLD : pMac->roam.configParam.vccRssiThreshold;
   pMac->roam.vccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
    /* Check for user misconfig of UL MAC Loss trigger threshold           */
   pMac->roam.configParam.vccUlMacLossThreshold =
      ( 0 == pMac->roam.configParam.vccUlMacLossThreshold ) ?
      CSR_VCC_UL_MAC_LOSS_THRESHOLD : pMac->roam.configParam.vccUlMacLossThreshold;
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
    {
        tANI_U32 sessionId = 0;
        /* Indicate the neighbor roam algorithm about the connect indication */
        csrRoamGetSessionIdFromBSSID(pMac, (tCsrBssid *)bssid, &sessionId);
        csrNeighborRoamIndicateConnect(pMac, sessionId, VOS_STATUS_SUCCESS);
    }
#endif
}

static void csrRoamLinkDown(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }
   //Only to handle the case for Handover on infra link
   if( eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType )
   {
      return;
   }
   /*
    * In-case of station mode, immediately stop data transmission whenever
    * link down is detected.
    */
   if (csrRoamIsStaMode(pMac, sessionId)
       && !CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)
#ifdef WLAN_FEATURE_VOWIFI_11R
       && !csrRoamIs11rAssoc(pMac, sessionId)
#endif
       ) {
        smsLog(pMac, LOG1, FL("Inform Link lost for session %d"), sessionId);
        csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_LOSTLINK,
                            eCSR_ROAM_RESULT_LOSTLINK);
   }
   /* deregister the clients requesting stats from PE/TL & also stop the corresponding timers*/
   csrRoamDeregStatisticsReq(pMac);
   pMac->roam.vccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
   /* Indicate the neighbor roam algorithm about the disconnect indication */
   csrNeighborRoamIndicateDisconnect(pMac, sessionId);
#endif

    //Remove this code once SLM_Sessionization is supported
    //BMPS_WORKAROUND_NOT_NEEDED
    if(!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
        csrIsInfraApStarted( pMac ) &&
        pMac->roam.configParam.doBMPSWorkaround)
   {
       pMac->roam.configParam.doBMPSWorkaround = 0;
   }

   if(pMac->psOffloadEnabled)
       pmcOffloadCleanup(pMac, sessionId);

}

void csrRoamTlStatsTimerHandler(void *pv)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( pv );
   eHalStatus status;
   pMac->roam.tlStatsReqInfo.timerRunning = FALSE;

   smsLog(pMac, LOG1, FL(" TL stat timer is no-op. It needs to support multiple stations"));

   if(!pMac->roam.tlStatsReqInfo.timerRunning)
   {
      if(pMac->roam.tlStatsReqInfo.periodicity)
      {
         //start timer
         status = vos_timer_start(&pMac->roam.tlStatsReqInfo.hTlStatsTimer,
                                pMac->roam.tlStatsReqInfo.periodicity);
         if (!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamTlStatsTimerHandler:cannot start TlStatsTimer timer"));
            return;
         }
         pMac->roam.tlStatsReqInfo.timerRunning = TRUE;
      }
   }
}
void csrRoamPeStatsTimerHandler(void *pv)
{
   tCsrPeStatsReqInfo *pPeStatsReqListEntry = (tCsrPeStatsReqInfo *)pv;
   eHalStatus status;
   tpAniSirGlobal pMac = pPeStatsReqListEntry->pMac;
   VOS_STATUS vosStatus;
   tPmcPowerState powerState;
   pPeStatsReqListEntry->timerRunning = FALSE;
   if( pPeStatsReqListEntry->timerStopFailed == TRUE )
   {
      // If we entered here, meaning the timer could not be successfully
      // stopped in csrRoamRemoveEntryFromPeStatsReqList(). So do it here.

      /* Destroy the timer */
      vosStatus = vos_timer_destroy( &pPeStatsReqListEntry->hPeStatsTimer );
      if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
      {
         smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:failed to destroy hPeStatsTimer timer"));
      }

      // Free the entry
      vos_mem_free(pPeStatsReqListEntry);
      pPeStatsReqListEntry = NULL;
   }
   else
   {
      if(!pPeStatsReqListEntry->rspPending)
      {
         status = csrSendMBStatsReqMsg(pMac, pPeStatsReqListEntry->statsMask & ~(1 << eCsrGlobalClassDStats),
                                       pPeStatsReqListEntry->staId,
                                       pPeStatsReqListEntry->sessionId);
         if(!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:failed to send down stats req to PE"));
         }
         else
         {
            pPeStatsReqListEntry->rspPending = TRUE;
         }
      }

      //send down a req
      if(pPeStatsReqListEntry->periodicity &&
         (VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&pPeStatsReqListEntry->hPeStatsTimer)))
      {
         pmcQueryPowerState(pMac, &powerState, NULL, NULL);
         if(ePMC_FULL_POWER == powerState)
         {
            if(pPeStatsReqListEntry->periodicity < pMac->roam.configParam.statsReqPeriodicity)
            {
               pPeStatsReqListEntry->periodicity = pMac->roam.configParam.statsReqPeriodicity;
            }
         }
         else
         {
            if(pPeStatsReqListEntry->periodicity < pMac->roam.configParam.statsReqPeriodicityInPS)
            {
               pPeStatsReqListEntry->periodicity = pMac->roam.configParam.statsReqPeriodicityInPS;
            }
         }
         //start timer
         vosStatus = vos_timer_start( &pPeStatsReqListEntry->hPeStatsTimer, pPeStatsReqListEntry->periodicity );
         if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
         {
            smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:cannot start hPeStatsTimer timer"));
            return;
         }
         pPeStatsReqListEntry->timerRunning = TRUE;

      }

   }
}
void csrRoamStatsClientTimerHandler(void *pv)
{
   tCsrStatsClientReqInfo *pStaEntry = (tCsrStatsClientReqInfo *)pv;
   if (VOS_TIMER_STATE_STOPPED ==
                      vos_timer_getCurrentState(&pStaEntry->timer)) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                 FL("roam stats client timer is stopped"));
   }
}



eHalStatus csrSendMBStatsReqMsg( tpAniSirGlobal pMac, tANI_U32 statsMask,
                                 tANI_U8 staId, tANI_U8 sessionId)
{
   tAniGetPEStatsReq *pMsg;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   pMsg = vos_mem_malloc(sizeof(tAniGetPEStatsReq));
   if ( NULL == pMsg )
   {
      smsLog(pMac, LOGE, FL( "Failed to allocate mem for stats req "));
      return eHAL_STATUS_FAILURE;
   }
   // need to initiate a stats request to PE
   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_STATISTICS_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetPEStatsReq);
   pMsg->staId = staId;
   pMsg->statsMask = statsMask;
   pMsg->sessionId = sessionId;
   status = palSendMBMessage(pMac->hHdd, pMsg );
   if(!HAL_STATUS_SUCCESS(status))
   {
      smsLog(pMac, LOG1, FL("Failed to send down the stats req "));
   }
   return status;
}
void csrRoamStatsRspProcessor(tpAniSirGlobal pMac, tSirSmeRsp *pSirMsg)
{
   tAniGetPEStatsRsp *pSmeStatsRsp;
   eHalStatus status = eHAL_STATUS_FAILURE;
   tListElem *pEntry = NULL;
   tCsrStatsClientReqInfo *pTempStaEntry = NULL;
   tCsrPeStatsReqInfo *pPeStaEntry = NULL;
   tANI_U32  tempMask = 0;
   tANI_U8 counter = 0;
   tANI_U8 *pStats = NULL;
   tANI_U32   length = 0;
   v_PVOID_t  pvosGCtx;
   v_S7_t     rssi = 0, snr = 0;
   tANI_U32   *pRssi = NULL, *pSnr = NULL;
   tANI_U32   linkCapacity;
   pSmeStatsRsp = (tAniGetPEStatsRsp *)pSirMsg;
   if(pSmeStatsRsp->rc)
   {
      smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:stats rsp from PE shows failure"));
      goto post_update;
   }
   tempMask = pSmeStatsRsp->statsMask;
   pStats = ((tANI_U8 *)&pSmeStatsRsp->statsMask) + sizeof(pSmeStatsRsp->statsMask);
   /* subtract all statistics from this length, and after processing the entire
    * 'stat' part of the message, if the length is not zero, then rssi is piggy packed
    * in this 'stats' message.
    */
   length = pSmeStatsRsp->msgLen - sizeof(tAniGetPEStatsRsp);
   /* New stats info from PE, fill up the stats structures in PMAC */
   while(tempMask)
   {
      if(tempMask & 1)
      {
         switch(counter)
         {
         case eCsrSummaryStats:
            smsLog( pMac, LOG2, FL("csrRoamStatsRspProcessor:summary stats"));
            vos_mem_copy((tANI_U8 *)&pMac->roam.summaryStatsInfo,
                         pStats, sizeof(tCsrSummaryStatsInfo));
            pStats += sizeof(tCsrSummaryStatsInfo);
            length -= sizeof(tCsrSummaryStatsInfo);
            break;
         case eCsrGlobalClassAStats:
            smsLog( pMac, LOG2, FL("csrRoamStatsRspProcessor:ClassA stats"));
            vos_mem_copy((tANI_U8 *)&pMac->roam.classAStatsInfo,
                         pStats, sizeof(tCsrGlobalClassAStatsInfo));
            pStats += sizeof(tCsrGlobalClassAStatsInfo);
            length -= sizeof(tCsrGlobalClassAStatsInfo);
            break;
         case eCsrGlobalClassBStats:
            smsLog( pMac, LOG2, FL("csrRoamStatsRspProcessor:ClassB stats"));
            vos_mem_copy((tANI_U8 *)&pMac->roam.classBStatsInfo,
                         pStats, sizeof(tCsrGlobalClassBStatsInfo));
            pStats += sizeof(tCsrGlobalClassBStatsInfo);
            length -= sizeof(tCsrGlobalClassBStatsInfo);
            break;
         case eCsrGlobalClassCStats:
            smsLog( pMac, LOG2, FL("csrRoamStatsRspProcessor:ClassC stats"));
            vos_mem_copy((tANI_U8 *)&pMac->roam.classCStatsInfo,
                        pStats, sizeof(tCsrGlobalClassCStatsInfo));
            pStats += sizeof(tCsrGlobalClassCStatsInfo);
            length -= sizeof(tCsrGlobalClassCStatsInfo);
            break;
         case eCsrPerStaStats:
            smsLog( pMac, LOG2, FL("csrRoamStatsRspProcessor:PerSta stats"));
            if( CSR_MAX_STA > pSmeStatsRsp->staId )
            {
               vos_mem_copy((tANI_U8 *)&pMac->roam.perStaStatsInfo[pSmeStatsRsp->staId],
                            pStats, sizeof(tCsrPerStaStatsInfo));
            }
            else
            {
               status = eHAL_STATUS_FAILURE;
               smsLog( pMac, LOGE, FL("csrRoamStatsRspProcessor:out bound staId:%d"), pSmeStatsRsp->staId);
               VOS_ASSERT( 0 );
            }
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy PerSta stats"));
            }
            pStats += sizeof(tCsrPerStaStatsInfo);
            length -= sizeof(tCsrPerStaStatsInfo);
            break;
         default:
            smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:unknown stats type"));
            break;
         }
      }
      tempMask >>=1;
      counter++;
   }
   pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SME, pMac);
   if (length != 0)
   {
       pRssi = (tANI_U32*)pStats;
       rssi = (v_S7_t)*pRssi;
       pStats += sizeof(tANI_U32);
       length -= sizeof(tANI_U32);
   }
   else
   {
       /* If riva is not sending rssi, continue to use the hack */
       rssi = RSSI_HACK_BMPS;
   }

   WDA_UpdateRssiBmps(pvosGCtx, pSmeStatsRsp->staId, rssi);

   if (length != 0)
   {
       linkCapacity = *(tANI_U32*)pStats;
       pStats += sizeof(tANI_U32);
       length -= sizeof(tANI_U32);
   }
   else
   {
       linkCapacity = 0;
   }

   WDA_UpdateLinkCapacity(pvosGCtx, pSmeStatsRsp->staId, linkCapacity);

   if (length != 0)
   {
       pSnr = (tANI_U32*)pStats;
       snr = (v_S7_t)*pSnr;
   }
   else
   {
       snr = SNR_HACK_BMPS;
   }

   WDA_UpdateSnrBmps(pvosGCtx, pSmeStatsRsp->staId, snr);
post_update:
   //make sure to update the pe stats req list
   pEntry = csrRoamFindInPeStatsReqList(pMac, pSmeStatsRsp->statsMask);
   if(pEntry)
      {
      pPeStaEntry = GET_BASE_ADDR( pEntry, tCsrPeStatsReqInfo, link );
      pPeStaEntry->rspPending = FALSE;

   }
   //check the one timer cases
   pEntry = csrRoamCheckClientReqList(pMac, pSmeStatsRsp->statsMask);
   if(pEntry)
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if(pTempStaEntry->timerExpired)
      {
         //send up the stats report
         csrRoamReportStatistics(pMac, pTempStaEntry->statsMask, pTempStaEntry->callback,
                                 pTempStaEntry->staId, pTempStaEntry->pContext);
         //also remove from the client list
         csrRoamRemoveStatListEntry(pMac, pEntry);
         pTempStaEntry = NULL;
      }
   }
}
tListElem * csrRoamFindInPeStatsReqList(tpAniSirGlobal pMac, tANI_U32  statsMask)
{
   tListElem *pEntry = NULL;
   tCsrPeStatsReqInfo *pTempStaEntry = NULL;
   pEntry = csrLLPeekHead( &pMac->roam.peStatsReqList, LL_ACCESS_LOCK );
   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOG2, "csrRoamFindInPeStatsReqList: List empty, no request to PE");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrPeStatsReqInfo, link );
      if(pTempStaEntry->statsMask == statsMask)
      {
         smsLog(pMac, LOG3, "csrRoamFindInPeStatsReqList: match found");
         break;
      }
      pEntry = csrLLNext( &pMac->roam.peStatsReqList, pEntry, LL_ACCESS_NOLOCK );
   }
   return pEntry;
}

tListElem * csrRoamChecknUpdateClientReqList(tpAniSirGlobal pMac, tCsrStatsClientReqInfo *pStaEntry,
                                             tANI_BOOLEAN update)
{
   tListElem *pEntry;
   tCsrStatsClientReqInfo *pTempStaEntry;
   pEntry = csrLLPeekHead( &pMac->roam.statsClientReqList, LL_ACCESS_LOCK );
   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOG2, "csrRoamChecknUpdateClientReqList: List empty, no request from "
             "upper layer client(s)");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if((pTempStaEntry->requesterId == pStaEntry->requesterId) &&
         (pTempStaEntry->statsMask == pStaEntry->statsMask))
      {
         smsLog(pMac, LOG3, "csrRoamChecknUpdateClientReqList: match found");
         if(update)
         {
            pTempStaEntry->periodicity = pStaEntry->periodicity;
            pTempStaEntry->callback = pStaEntry->callback;
            pTempStaEntry->pContext = pStaEntry->pContext;
         }
         break;
      }
      pEntry = csrLLNext( &pMac->roam.statsClientReqList, pEntry, LL_ACCESS_NOLOCK );
   }
   return pEntry;
}
tListElem * csrRoamCheckClientReqList(tpAniSirGlobal pMac, tANI_U32 statsMask)
{
   tListElem *pEntry;
   tCsrStatsClientReqInfo *pTempStaEntry;
   pEntry = csrLLPeekHead( &pMac->roam.statsClientReqList, LL_ACCESS_LOCK );
   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOG2, "csrRoamCheckClientReqList: List empty, no request from "
             "upper layer client(s)");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if((pTempStaEntry->statsMask & ~(1 << eCsrGlobalClassDStats))  == statsMask)
      {
         smsLog(pMac, LOG3, "csrRoamCheckClientReqList: match found");
         break;
      }
      pEntry = csrLLNext( &pMac->roam.statsClientReqList, pEntry, LL_ACCESS_NOLOCK );
   }
   return pEntry;
}
eHalStatus csrRoamRegisterLinkQualityIndCallback(tpAniSirGlobal pMac,
                                                 csrRoamLinkQualityIndCallback   callback,
                                                 void                           *pContext)
{
   pMac->roam.linkQualityIndInfo.callback = callback;
   pMac->roam.linkQualityIndInfo.context = pContext;
   if( NULL == callback )
   {
     smsLog(pMac, LOGW, "csrRoamRegisterLinkQualityIndCallback: indication callback being deregistered");
   }
   else
   {
     smsLog(pMac, LOGW, "csrRoamRegisterLinkQualityIndCallback: indication callback being registered");
     /* do we need to invoke the callback to notify client of initial value ??  */
   }
   return eHAL_STATUS_SUCCESS;
}
void csrRoamVccTrigger(tpAniSirGlobal pMac)
{
   eCsrRoamLinkQualityInd newVccLinkQuality;
   tANI_U32 ul_mac_loss = 0;
   tANI_U32 ul_mac_loss_trigger_threshold;
 /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
   /*-------------------------------------------------------------------------
     Link quality is currently binary based on OBIWAN recommended triggers
     Check for a change in link quality and notify client if necessary
   -------------------------------------------------------------------------*/
   ul_mac_loss_trigger_threshold =
      pMac->roam.configParam.vccUlMacLossThreshold;
   if (0 == ul_mac_loss_trigger_threshold)
   {
      VOS_ASSERT( ul_mac_loss_trigger_threshold != 0 );
      return;
   }
   smsLog(pMac, LOGW, "csrRoamVccTrigger: UL_MAC_LOSS_THRESHOLD is %d",
          ul_mac_loss_trigger_threshold );
   if(ul_mac_loss_trigger_threshold < ul_mac_loss)
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality is POOR ");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
   }
   else
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality is GOOD");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_GOOD_IND;
   }
   smsLog(pMac, LOGW, "csrRoamVccTrigger: link qual : *** UL_MAC_LOSS %d *** ",
          ul_mac_loss);
   if(newVccLinkQuality != pMac->roam.vccLinkQuality)
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality changed: trigger necessary");
      if(NULL != pMac->roam.linkQualityIndInfo.callback)
      {
         smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality indication %d",
                newVccLinkQuality );

         /* we now invoke the callback once to notify client of initial value   */
         pMac->roam.linkQualityIndInfo.callback( newVccLinkQuality,
                                                 pMac->roam.linkQualityIndInfo.context );
         //event: EVENT_WLAN_VCC
      }
   }
   pMac->roam.vccLinkQuality = newVccLinkQuality;

}
VOS_STATUS csrRoamVccTriggerRssiIndCallback(tHalHandle hHal,
                                            v_U8_t  rssiNotification,
                                            void * context)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( context );
   eCsrRoamLinkQualityInd newVccLinkQuality;
   tANI_U32 sessionId = 0;
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   /*-------------------------------------------------------------------------
     Link quality is currently binary based on OBIWAN recommended triggers
     Check for a change in link quality and notify client if necessary
   -------------------------------------------------------------------------*/
   smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: RSSI trigger threshold is %d",
          pMac->roam.configParam.vccRssiThreshold);
   if(!csrIsConnStateConnectedInfra(pMac, sessionId))
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: ignoring the indication as we are not connected");
      return VOS_STATUS_SUCCESS;
   }
   if(WLANTL_HO_THRESHOLD_DOWN == rssiNotification)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality is POOR");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
   }
   else if(WLANTL_HO_THRESHOLD_UP == rssiNotification)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality is GOOD ");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_GOOD_IND;
   }
   else
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: unknown rssi notification %d", rssiNotification);
      //Set to this so the code below won't do anything
      newVccLinkQuality = pMac->roam.vccLinkQuality;
      VOS_ASSERT(0);
   }

   if(newVccLinkQuality != pMac->roam.vccLinkQuality)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality changed: trigger necessary");
      if(NULL != pMac->roam.linkQualityIndInfo.callback)
      {
         smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality indication %d",
                newVccLinkQuality);
        /* we now invoke the callback once to notify client of initial value   */
        pMac->roam.linkQualityIndInfo.callback( newVccLinkQuality,
                                                pMac->roam.linkQualityIndInfo.context );
         //event: EVENT_WLAN_VCC
      }
   }
   pMac->roam.vccLinkQuality = newVccLinkQuality;
   return status;
}
tCsrStatsClientReqInfo * csrRoamInsertEntryIntoList( tpAniSirGlobal pMac,
                                                     tDblLinkList *pStaList,
                                                     tCsrStatsClientReqInfo *pStaEntry)
{
   tCsrStatsClientReqInfo *pNewStaEntry = NULL;
   //if same entity requested for same set of stats with different periodicity &
   // callback update it
   if(NULL == csrRoamChecknUpdateClientReqList(pMac, pStaEntry, TRUE))
   {

      pNewStaEntry = vos_mem_malloc(sizeof(tCsrStatsClientReqInfo));
      if (NULL == pNewStaEntry)
      {
         smsLog(pMac, LOGW, "csrRoamInsertEntryIntoList: couldn't allocate memory for the "
                "entry");
         return NULL;
      }

      pNewStaEntry->callback = pStaEntry->callback;
      pNewStaEntry->pContext = pStaEntry->pContext;
      pNewStaEntry->periodicity = pStaEntry->periodicity;
      pNewStaEntry->requesterId = pStaEntry->requesterId;
      pNewStaEntry->statsMask = pStaEntry->statsMask;
      pNewStaEntry->pPeStaEntry = pStaEntry->pPeStaEntry;
      pNewStaEntry->pMac = pStaEntry->pMac;
      pNewStaEntry->staId = pStaEntry->staId;
      pNewStaEntry->timerExpired = pStaEntry->timerExpired;

      csrLLInsertTail( pStaList, &pNewStaEntry->link, LL_ACCESS_LOCK  );
   }
   return pNewStaEntry;
}

tCsrPeStatsReqInfo * csrRoamInsertEntryIntoPeStatsReqList( tpAniSirGlobal pMac,
                                                           tDblLinkList *pStaList,
                                                           tCsrPeStatsReqInfo *pStaEntry)
{
   tCsrPeStatsReqInfo *pNewStaEntry = NULL;
   pNewStaEntry = vos_mem_malloc(sizeof(tCsrPeStatsReqInfo));
   if (NULL == pNewStaEntry)
   {
      smsLog(pMac, LOGW, "csrRoamInsertEntryIntoPeStatsReqList: couldn't allocate memory for the "
                  "entry");
      return NULL;
   }

   pNewStaEntry->hPeStatsTimer = pStaEntry->hPeStatsTimer;
   pNewStaEntry->numClient = pStaEntry->numClient;
   pNewStaEntry->periodicity = pStaEntry->periodicity;
   pNewStaEntry->statsMask = pStaEntry->statsMask;
   pNewStaEntry->pMac = pStaEntry->pMac;
   pNewStaEntry->staId = pStaEntry->staId;
   pNewStaEntry->timerRunning = pStaEntry->timerRunning;
   pNewStaEntry->rspPending = pStaEntry->rspPending;

   csrLLInsertTail( pStaList, &pNewStaEntry->link, LL_ACCESS_LOCK  );
   return pNewStaEntry;
}
eHalStatus csrGetRssi(tpAniSirGlobal pMac,
                      tCsrRssiCallback callback,
                      tANI_U8 staId,
                      tCsrBssid bssId,
                      tANI_S8 lastRSSI,
                      void *pContext,
                      void* pVosContext)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   vos_msg_t  msg;
   tANI_U32 sessionId;

   tAniGetRssiReq *pMsg;
   smsLog(pMac, LOG2, FL("called"));

   status = csrRoamGetSessionIdFromBSSID(pMac, (tCsrBssid *)bssId, &sessionId);
   if (!HAL_STATUS_SUCCESS(status))
   {
      callback(lastRSSI, staId, pContext);
      smsLog(pMac, LOGE, FL("Failed to get SessionId"));
      return eHAL_STATUS_FAILURE;
   }

   pMsg = vos_mem_malloc(sizeof(tAniGetRssiReq));
   if ( NULL == pMsg )
   {
      smsLog(pMac, LOGE, " csrGetRssi: failed to allocate mem for req ");
      return eHAL_STATUS_FAILURE;
   }

   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_RSSI_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetRssiReq);
   pMsg->sessionId = sessionId;
   pMsg->staId = staId;
   pMsg->rssiCallback = callback;
   pMsg->pDevContext = pContext;
   pMsg->pVosContext = pVosContext;
   /*
    * store RSSI at time of calling, so that if RSSI request cannot
    * be sent to firmware, this value can be used to return immediately
    */
   pMsg->lastRSSI = lastRSSI;
   msg.type = eWNI_SME_GET_RSSI_REQ;
   msg.bodyptr = pMsg;
   msg.reserved = 0;
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
   {
       smsLog(pMac, LOGE, " csrGetRssi failed to post msg to self ");
       vos_mem_free((void *)pMsg);
       status = eHAL_STATUS_FAILURE;
   }
   smsLog(pMac, LOG2, FL("returned"));
   return status;
}

eHalStatus csrGetSnr(tpAniSirGlobal pMac,
                     tCsrSnrCallback callback,
                     tANI_U8 staId, tCsrBssid bssId,
                     void *pContext)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   vos_msg_t  msg;
   tANI_U32 sessionId;

   tAniGetSnrReq *pMsg;

   smsLog(pMac, LOG2, FL("called"));

   pMsg =(tAniGetSnrReq *)vos_mem_malloc(sizeof(tAniGetSnrReq));
   if (NULL == pMsg )
   {
      smsLog(pMac, LOGE, "%s: failed to allocate mem for req",__func__);
      return status;
   }

   csrRoamGetSessionIdFromBSSID(pMac, (tCsrBssid *)bssId, &sessionId);

   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_SNR_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetSnrReq);
   pMsg->sessionId = sessionId;
   pMsg->staId = staId;
   pMsg->snrCallback = callback;
   pMsg->pDevContext = pContext;
   msg.type = eWNI_SME_GET_SNR_REQ;
   msg.bodyptr = pMsg;
   msg.reserved = 0;

   if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
   {
       smsLog(pMac, LOGE, "%s failed to post msg to self", __func__);
       vos_mem_free((v_VOID_t *)pMsg);
       status = eHAL_STATUS_FAILURE;
   }

   smsLog(pMac, LOG2, FL("returned"));
   return status;
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
eHalStatus csrGetTsmStats(tpAniSirGlobal pMac,
                          tCsrTsmStatsCallback callback,
                          tANI_U8 staId,
                          tCsrBssid bssId,
                          void *pContext,
                          void* pVosContext,
                          tANI_U8 tid)
{
   eHalStatus          status = eHAL_STATUS_SUCCESS;
   tAniGetTsmStatsReq *pMsg = NULL;
   pMsg = vos_mem_malloc(sizeof(tAniGetTsmStatsReq));
   if (!pMsg)
   {
      smsLog(pMac, LOGE, "csrGetTsmStats: failed to allocate mem for req");
      return status;
   }
   // need to initiate a stats request to PE
   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_TSM_STATS_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetTsmStatsReq);
   pMsg->staId = staId;
   pMsg->tid = tid;
   vos_mem_copy(pMsg->bssId, bssId, sizeof(tSirMacAddr));
   pMsg->tsmStatsCallback = callback;
   pMsg->pDevContext = pContext;
   pMsg->pVosContext = pVosContext;
   status = palSendMBMessage(pMac->hHdd, pMsg );
   if(!HAL_STATUS_SUCCESS(status))
   {
      smsLog(pMac, LOG1, " csrGetTsmStats: failed to send down the rssi req");
      //pMsg is freed by palSendMBMessage
      status = eHAL_STATUS_FAILURE;
   }
   return status;
}
#endif  /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/* ---------------------------------------------------------------------------
    \fn csrGetTLSTAState
    \helper function to get the TL STA State whenever the function is called.

    \param staId - The staID to be passed to the TL
            to get the relevant TL STA State
    \return the state as tANI_U16
  ---------------------------------------------------------------------------*/
tANI_U16 csrGetTLSTAState(tpAniSirGlobal pMac, tANI_U8 staId)
{
   WLANTL_STAStateType tlSTAState;
   tlSTAState = WLANTL_STA_INIT;

   //request TL for STA State
   if ( !VOS_IS_STATUS_SUCCESS(WLANTL_GetSTAState(pMac->roam.gVosContext, staId, &tlSTAState)) )
   {
      smsLog(pMac, LOGE, FL("csrGetTLSTAState:couldn't get the STA state from TL"));
   }

   return tlSTAState;
}

eHalStatus csrGetStatistics(tpAniSirGlobal pMac, eCsrStatsRequesterType requesterId,
                            tANI_U32 statsMask,
                            tCsrStatsCallback callback,
                            tANI_U32 periodicity, tANI_BOOLEAN cache,
                            tANI_U8 staId, void *pContext,
                            tANI_U8 sessionId)
{
   tCsrStatsClientReqInfo staEntry;
   tCsrStatsClientReqInfo *pStaEntry = NULL;
   tCsrPeStatsReqInfo *pPeStaEntry = NULL;
   tListElem *pEntry = NULL;
   tANI_BOOLEAN found = FALSE;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tANI_BOOLEAN insertInClientList = FALSE;
   VOS_STATUS vosStatus;
   WLANTL_TRANSFER_STA_TYPE *pTlStats;

   if( csrIsAllSessionDisconnected(pMac) )
   {
      //smsLog(pMac, LOGW, "csrGetStatistics: wrong state curState(%d) not connected", pMac->roam.curState);
      return eHAL_STATUS_FAILURE;
   }

   if (csrNeighborMiddleOfRoaming((tHalHandle)pMac, sessionId))
   {
       smsLog(pMac, LOG1, FL("in the middle of roaming states"));
       return eHAL_STATUS_FAILURE;
   }

   if((!statsMask) && (!callback))
   {
      //msg
      smsLog(pMac, LOGW, "csrGetStatistics: statsMask & callback empty in the request");
      return eHAL_STATUS_FAILURE;
   }
   //for the search list method for deregister
   staEntry.requesterId = requesterId;
   staEntry.statsMask = statsMask;
   //requester wants to deregister or just an error
   if((statsMask) && (!callback))
   {
      pEntry = csrRoamChecknUpdateClientReqList(pMac, &staEntry, FALSE);
      if(!pEntry)
      {
         //msg
         smsLog(pMac, LOGW, "csrGetStatistics: callback is empty in the request & couldn't "
                "find any existing request in statsClientReqList");
         return eHAL_STATUS_FAILURE;
      }
      else
      {
         //clean up & return
         pStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
         if(NULL != pStaEntry->pPeStaEntry)
         {
            pStaEntry->pPeStaEntry->numClient--;
            //check if we need to delete the entry from peStatsReqList too
            if(!pStaEntry->pPeStaEntry->numClient)
            {
               csrRoamRemoveEntryFromPeStatsReqList(pMac, pStaEntry->pPeStaEntry);
            }
         }

         //check if we need to stop the tl stats timer too
         pMac->roam.tlStatsReqInfo.numClient--;
         if(!pMac->roam.tlStatsReqInfo.numClient)
         {
            if(pMac->roam.tlStatsReqInfo.timerRunning)
            {
               status = vos_timer_stop(&pMac->roam.tlStatsReqInfo.hTlStatsTimer);
               if (!HAL_STATUS_SUCCESS(status))
               {
                  smsLog(pMac, LOGE, FL("csrGetStatistics:cannot stop TlStatsTimer timer"));
                  return eHAL_STATUS_FAILURE;
               }
            }
            pMac->roam.tlStatsReqInfo.periodicity = 0;
            pMac->roam.tlStatsReqInfo.timerRunning = FALSE;
         }
         vos_timer_stop( &pStaEntry->timer );
         // Destroy the vos timer...
         vosStatus = vos_timer_destroy( &pStaEntry->timer );
         if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
         {
            smsLog(pMac, LOGE, FL("csrGetStatistics:failed to destroy Client req timer"));
         }
         csrRoamRemoveStatListEntry(pMac, pEntry);
         pStaEntry = NULL;
         return eHAL_STATUS_SUCCESS;
      }
   }

   if(cache && !periodicity)
   {
      //return the cached stats
      csrRoamReportStatistics(pMac, statsMask, callback, staId, pContext);
   }
   else
   {
      //add the request in the client req list
      staEntry.callback = callback;
      staEntry.pContext = pContext;
      staEntry.periodicity = periodicity;
      staEntry.pPeStaEntry = NULL;
      staEntry.staId = staId;
      staEntry.pMac = pMac;
      staEntry.timerExpired = FALSE;
      staEntry.sessionId = sessionId;


      //if periodic report requested with non cached result from PE/TL
      if(periodicity)
      {

         //if looking for stats from PE
         if(statsMask & ~(1 << eCsrGlobalClassDStats))
         {

            //check if same request made already & waiting for rsp
            pPeStaEntry = csrRoamCheckPeStatsReqList(pMac, statsMask & ~(1 << eCsrGlobalClassDStats),
                                               periodicity, &found, staId,
                                               sessionId);
            if(!pPeStaEntry)
            {
               //bail out, maxed out on number of req for PE
               return eHAL_STATUS_FAILURE;
            }
            else
            {
               staEntry.pPeStaEntry = pPeStaEntry;
            }

         }
         /* request stats from TL right away if requested by client,
            update tlStatsReqInfo if needed */
         if(statsMask & (1 << eCsrGlobalClassDStats))
         {
            if(cache && pMac->roam.tlStatsReqInfo.numClient)
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:Looking for cached stats from TL"));
            }
            else
            {

               //update periodicity
               if(pMac->roam.tlStatsReqInfo.periodicity)
               {
                  pMac->roam.tlStatsReqInfo.periodicity =
                     CSR_ROAM_MIN(periodicity, pMac->roam.tlStatsReqInfo.periodicity);
               }
               else
               {
                  pMac->roam.tlStatsReqInfo.periodicity = periodicity;
               }
               if(pMac->roam.tlStatsReqInfo.periodicity < CSR_MIN_TL_STAT_QUERY_PERIOD)
               {
                  pMac->roam.tlStatsReqInfo.periodicity = CSR_MIN_TL_STAT_QUERY_PERIOD;
               }

               if(!pMac->roam.tlStatsReqInfo.timerRunning)
               {
                  pTlStats = (WLANTL_TRANSFER_STA_TYPE *)vos_mem_malloc(sizeof(WLANTL_TRANSFER_STA_TYPE));
                  if (NULL != pTlStats)
                  {
                     //req TL for class D stats
                     if(WLANTL_GetStatistics(pMac->roam.gVosContext, pTlStats, staId))
                     {
                        smsLog(pMac, LOGE, FL("csrGetStatistics:couldn't get the stats from TL"));
                     }
                     else
                     {
                        //save in SME
                        csrRoamSaveStatsFromTl(pMac, pTlStats);
                     }
                     vos_mem_free(pTlStats);
                     pTlStats = NULL;
                  }
                  else
                  {
                     smsLog(pMac, LOGE, FL("cannot allocate memory for TL stat"));
                  }

                  if(pMac->roam.tlStatsReqInfo.periodicity)
                  {
                     //start timer
                     status = vos_timer_start(&pMac->roam.tlStatsReqInfo.hTlStatsTimer,
                                            pMac->roam.tlStatsReqInfo.periodicity);
                     if (!HAL_STATUS_SUCCESS(status))
                     {
                        smsLog(pMac, LOGE, FL("csrGetStatistics:cannot start TlStatsTimer timer"));
                        return eHAL_STATUS_FAILURE;
                     }
                     pMac->roam.tlStatsReqInfo.timerRunning = TRUE;
                  }
               }
            }
            pMac->roam.tlStatsReqInfo.numClient++;
         }

         insertInClientList = TRUE;
      }
      //if one time report requested with non cached result from PE/TL
      else if(!cache && !periodicity)
      {
         if(statsMask & ~(1 << eCsrGlobalClassDStats))
         {
            //send down a req
            status = csrSendMBStatsReqMsg(pMac,
                                     statsMask & ~(1 << eCsrGlobalClassDStats),
                                     staId,
                                     sessionId);
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:failed to send down stats req to PE"));
            }
            //so that when the stats rsp comes back from PE we respond to upper layer
            //right away
            staEntry.timerExpired = TRUE;
            insertInClientList = TRUE;
         }
         if(statsMask & (1 << eCsrGlobalClassDStats))
         {
            pTlStats = (WLANTL_TRANSFER_STA_TYPE *)vos_mem_malloc(sizeof(WLANTL_TRANSFER_STA_TYPE));
            if (NULL != pTlStats)
            {
               //req TL for class D stats
               if(!VOS_IS_STATUS_SUCCESS(WLANTL_GetStatistics(pMac->roam.gVosContext, pTlStats, staId)))
               {
                  smsLog(pMac, LOGE, FL("csrGetStatistics:couldn't get the stats from TL"));
               }
               else
               {
                  //save in SME
                  csrRoamSaveStatsFromTl(pMac, pTlStats);
               }
               vos_mem_free(pTlStats);
               pTlStats = NULL;
            }
            else
            {
               smsLog(pMac, LOGE, FL("cannot allocate memory for TL stat"));
            }

         }
         //if looking for stats from TL only
         if(!insertInClientList)
         {
            //return the stats
            csrRoamReportStatistics(pMac, statsMask, callback, staId, pContext);
         }
      }
      if(insertInClientList)
      {
         pStaEntry = csrRoamInsertEntryIntoList(pMac, &pMac->roam.statsClientReqList, &staEntry);
         if(!pStaEntry)
         {
            //msg
            smsLog(pMac, LOGW, "csrGetStatistics: Failed to insert req in statsClientReqList");
            return eHAL_STATUS_FAILURE;
         }
         pStaEntry->periodicity = periodicity;
         //Init & start timer if needed
         if(periodicity)
         {
            vosStatus = vos_timer_init( &pStaEntry->timer, VOS_TIMER_TYPE_SW,
                                        csrRoamStatsClientTimerHandler, pStaEntry );
            if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:cannot init StatsClient timer"));
               return eHAL_STATUS_FAILURE;
            }
            vosStatus = vos_timer_start( &pStaEntry->timer, periodicity );
            if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:cannot start StatsClient timer"));
               return eHAL_STATUS_FAILURE;
            }
         }
      }
   }
   return eHAL_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD

static tSirRetStatus
csrRoamScanOffloadPopulateMacHeader(tpAniSirGlobal pMac,
                                    tANI_U8* pBD,
                                    tANI_U8 type,
                                    tANI_U8 subType,
                                    tSirMacAddr peerAddr,
                                    tSirMacAddr selfMacAddr)
{
        tSirRetStatus   statusCode = eSIR_SUCCESS;
        tpSirMacMgmtHdr pMacHdr;

        /* Prepare MAC management header */
        pMacHdr = (tpSirMacMgmtHdr) (pBD);

        /* Prepare FC */
        pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
        pMacHdr->fc.type    = type;
        pMacHdr->fc.subType = subType;

        /* Prepare Address 1 */
        vos_mem_copy((tANI_U8 *) pMacHdr->da, (tANI_U8 *) peerAddr,
                      sizeof( tSirMacAddr ));

        sirCopyMacAddr(pMacHdr->sa,selfMacAddr);

        /* Prepare Address 3 */
        vos_mem_copy((tANI_U8 *) pMacHdr->bssId, (tANI_U8 *) peerAddr,
                     sizeof( tSirMacAddr ));
        return statusCode;
} /*** csrRoamScanOffloadPopulateMacHeader() ***/

static tSirRetStatus
csrRoamScanOffloadPrepareProbeReqTemplate(tpAniSirGlobal pMac,
                                          tANI_U8 nChannelNum,
                                          tANI_U32 dot11mode,
                                          tSirMacAddr selfMacAddr,
                                          tANI_U8 *pFrame,
                                          tANI_U16 *pusLen,
                                          tCsrRoamSession *psession)
{
        tDot11fProbeRequest pr;
        tANI_U32            nStatus, nBytes, nPayload;
        tSirRetStatus       nSirStatus;
        /*Bcast tx*/
        tSirMacAddr         bssId = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


        vos_mem_set(( tANI_U8* )&pr, sizeof( pr ), 0);

        PopulateDot11fSuppRates( pMac, nChannelNum, &pr.SuppRates,NULL);

        if ( WNI_CFG_DOT11_MODE_11B != dot11mode )
        {
                PopulateDot11fExtSuppRates1( pMac, nChannelNum, &pr.ExtSuppRates );
        }


        if (IS_DOT11_MODE_HT(dot11mode))
        {
                PopulateDot11fHTCaps( pMac, NULL, &pr.HTCaps );
                pr.HTCaps.advCodingCap = psession->htConfig.ht_rx_ldpc;
                pr.HTCaps.txSTBC = psession->htConfig.ht_tx_stbc;
                pr.HTCaps.rxSTBC = psession->htConfig.ht_rx_stbc;
                if (!psession->htConfig.ht_sgi) {
                    pr.HTCaps.shortGI20MHz = pr.HTCaps.shortGI40MHz = 0;
                }
        }


        nStatus = dot11fGetPackedProbeRequestSize( pMac, &pr, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                                "Failed to calculate the packed size f"
                                "or a Probe Request (0x%08x).\n", nStatus );


                nPayload = sizeof( tDot11fProbeRequest );
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                                "There were warnings while calculating"
                                "the packed size for a Probe Request ("
                                "0x%08x).\n", nStatus );
        }

        nBytes = nPayload + sizeof( tSirMacMgmtHdr );

        /* Prepare outgoing frame*/
        vos_mem_set(pFrame, nBytes , 0);


        nSirStatus = csrRoamScanOffloadPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
                        SIR_MAC_MGMT_PROBE_REQ, bssId,selfMacAddr);

        if ( eSIR_SUCCESS != nSirStatus )
        {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                                "Failed to populate the buffer descriptor for a Probe Request (%d).\n",
                                nSirStatus );
                return nSirStatus;
        }


        nStatus = dot11fPackProbeRequest( pMac, &pr, pFrame +
                        sizeof( tSirMacMgmtHdr ),
                        nPayload, &nPayload );
        if ( DOT11F_FAILED( nStatus ) )
        {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                                "Failed to pack a Probe Request (0x%08x).\n", nStatus );
                return eSIR_FAILURE;
        }
        else if ( DOT11F_WARNED( nStatus ) )
        {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                          "There were warnings while packing a Probe Request (0x%08x).\n",
                          nStatus );
        }

        *pusLen = nPayload + sizeof(tSirMacMgmtHdr);
        return eSIR_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
eHalStatus csrRoamSetKeyMgmtOffload(tpAniSirGlobal pMac,
                                    tANI_U32 sessionId,
                                    v_BOOL_t nRoamKeyMgmtOffloadEnabled)
{
    tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);
    if (!pSession) {
        smsLog(pMac, LOGE, FL("session %d not found"), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    pSession->RoamKeyMgmtOffloadEnabled = nRoamKeyMgmtOffloadEnabled;
    return eHAL_STATUS_SUCCESS;
}

void csrRoamOffload(tpAniSirGlobal pMac, tSirRoamOffloadScanReq *pRequestBuf,
                                                   tCsrRoamSession *pSession)
{
        vos_mem_copy(pRequestBuf->PSK_PMK, pSession->psk_pmk,
                     sizeof(pRequestBuf->PSK_PMK));
        pRequestBuf->pmk_len = pSession->pmk_len;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                  "LFR3: PMK Length = %d", pRequestBuf->pmk_len);
        pRequestBuf->R0KH_ID_Length = pSession->ftSmeContext.r0kh_id_len;
        vos_mem_copy(pRequestBuf->R0KH_ID, pSession->ftSmeContext.r0kh_id,
                     pRequestBuf->R0KH_ID_Length);
        pRequestBuf->Prefer5GHz = pMac->roam.configParam.nRoamPrefer5GHz;
        pRequestBuf->RoamRssiCatGap = pMac->roam.configParam.bCatRssiOffset;
        pRequestBuf->Select5GHzMargin = pMac->roam.configParam.nSelect5GHzMargin;
        if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                                (tANI_U32 *)&pRequestBuf->ReassocFailureTimeout)
                        != eSIR_SUCCESS)
        {
                /**
                 * Could not get ReassocFailureTimeout value
                 * from CFG. Log error and set some default value
                 */
                smsLog(pMac, LOGE, FL("could not retrieve ReassocFailureTimeout value"));
                pRequestBuf->ReassocFailureTimeout = DEFAULT_REASSOC_FAILURE_TIMEOUT;
        }
#ifdef FEATURE_WLAN_ESE
        if (csrIsAuthTypeESE(pRequestBuf->ConnectedNetwork.authentication)) {
                vos_mem_copy(pRequestBuf->KRK,pSession->eseCckmInfo.krk, SIR_KRK_KEY_LEN);
                vos_mem_copy(pRequestBuf->BTK,pSession->eseCckmInfo.btk, SIR_BTK_KEY_LEN);
        }
#endif
        pRequestBuf->AcUapsd.acbe_uapsd =
                SIR_UAPSD_GET(ACBE, pMac->lim.gUapsdPerAcBitmask);
        pRequestBuf->AcUapsd.acbk_uapsd =
                SIR_UAPSD_GET(ACBK, pMac->lim.gUapsdPerAcBitmask);
        pRequestBuf->AcUapsd.acvi_uapsd =
                SIR_UAPSD_GET(ACVI, pMac->lim.gUapsdPerAcBitmask);
        pRequestBuf->AcUapsd.acvo_uapsd =
                SIR_UAPSD_GET(ACVO, pMac->lim.gUapsdPerAcBitmask);
}
#endif

/**
 * check_allowed_ssid_list() - Check the WhiteList
 * @req_buffer:      Buffer which contains the connected profile SSID.
 * @roam_params:     Buffer which contains the whitelist SSID's.
 *
 * Check if the connected profile SSID exists in the whitelist.
 * It is assumed that the framework provides this also in the whitelist.
 * If it exists there is no issue. Otherwise add it to the list.
 *
 * Return: None
 */
static void check_allowed_ssid_list(tSirRoamOffloadScanReq *req_buffer,
		struct roam_ext_params *roam_params)
{
	int i = 0;
	bool match = false;
	for (i = 0; i < roam_params->num_ssid_allowed_list; i++) {
		if ((roam_params->ssid_allowed_list[i].length ==
			req_buffer->ConnectedNetwork.ssId.length) &&
			vos_mem_compare(roam_params->ssid_allowed_list[i].ssId,
			req_buffer->ConnectedNetwork.ssId.ssId,
			roam_params->ssid_allowed_list[i].length)) {
			VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
				"Whitelist contains connected profile SSID");
			match = true;
			break;
		}
	}
	if (!match) {
		if (roam_params->num_ssid_allowed_list >=
			MAX_SSID_ALLOWED_LIST) {
			VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
				"Whitelist is FULL. Cannot Add another entry");
			return;
		}
		VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
			"Adding Connected profile SSID to whitelist");
		/* i is the next available index to add the entry.*/
		i = roam_params->num_ssid_allowed_list;
		vos_mem_copy(roam_params->ssid_allowed_list[i].ssId,
				req_buffer->ConnectedNetwork.ssId.ssId,
				req_buffer->ConnectedNetwork.ssId.length);
		roam_params->ssid_allowed_list[i].length =
			req_buffer->ConnectedNetwork.ssId.length;
		roam_params->num_ssid_allowed_list++;
	}
}

/*
 * Below Table describe whether RSO command can be send down to fimrware or not.
 * Host check it on the basis of previous RSO command sent down to firmware.
 *||==========================================================================||
 *|| New cmd        |            LAST SENT COMMAND --->                       ||
 *||====|=====================================================================||
 *||    V           | START | STOP | RESTART | UPDATE_CFG| ABORT_SCAN         ||
 *|| -------------------------------------------------------------------------||
 *|| RSO_START      | NO    | YES  |  NO     | NO        | NO                 ||
 *|| RSO_STOP       | YES   | YES  |  YES    | YES       | YES                ||
 *|| RSO_RESTART    | YES   | YES  |  NO     | YES       | YES                ||
 *|| RSO_UPDATE_CFG | YES   | NO   |  YES    | YES       | YES                ||
 *|| RSO_ABORT_SCAN | YES   | NO   |  YES    | YES       | YES                ||
 *||==========================================================================||
 **/
#define RSO_START_BIT       (1<<ROAM_SCAN_OFFLOAD_START)
#define RSO_STOP_BIT        (1<<ROAM_SCAN_OFFLOAD_STOP)
#define RSO_RESTART_BIT     (1<<ROAM_SCAN_OFFLOAD_RESTART)
#define RSO_UPDATE_CFG_BIT  (1<<ROAM_SCAN_OFFLOAD_UPDATE_CFG)
#define RSO_ABORT_SCAN_BIT  (1<<ROAM_SCAN_OFFLOAD_ABORT_SCAN)
#define RSO_START_ALLOW_MASK   (RSO_STOP_BIT)
#define RSO_STOP_ALLOW_MASK    (RSO_UPDATE_CFG_BIT | RSO_RESTART_BIT | \
		RSO_STOP_BIT | RSO_START_BIT | RSO_ABORT_SCAN_BIT)
#define RSO_RESTART_ALLOW_MASK (RSO_UPDATE_CFG_BIT | RSO_START_BIT | \
		RSO_ABORT_SCAN_BIT | RSO_RESTART_BIT)
#define RSO_UPDATE_CFG_ALLOW_MASK  (RSO_UPDATE_CFG_BIT | RSO_STOP_BIT | \
		RSO_START_BIT | RSO_ABORT_SCAN_BIT)
#define RSO_ABORT_SCAN_ALLOW_MASK (RSO_START_BIT | RSO_RESTART_BIT | \
		RSO_UPDATE_CFG_BIT | RSO_ABORT_SCAN_BIT)

bool csr_is_RSO_cmd_allowed(tpAniSirGlobal mac_ctx, uint8_t command,
		uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo neigh_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	tANI_U8 desiredMask = 0;
	bool ret_val;

	switch(command) {
	case ROAM_SCAN_OFFLOAD_START:
		desiredMask = RSO_START_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_STOP:
		desiredMask = RSO_STOP_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_RESTART:
		desiredMask = RSO_RESTART_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_UPDATE_CFG:
		desiredMask = RSO_UPDATE_CFG_ALLOW_MASK;
		break;
	case ROAM_SCAN_OFFLOAD_ABORT_SCAN:
		desiredMask = RSO_ABORT_SCAN_ALLOW_MASK;
		break;
	default:
		VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
			FL("Wrong RSO command %d, not allowed"), command);
		return 0;/*Cmd Not allowed*/
	}
	ret_val = desiredMask & ( 1 << neigh_roam_info->lastSentCmd);
	return ret_val;
}

void csr_roam_send_restart_cmd(tpAniSirGlobal pMac, tANI_U8 session_id,
		tANI_U8 command, tANI_U8 reason)
{
	struct sir_sme_roam_restart_req *msg;
	eHalStatus status;

	msg = vos_mem_malloc(sizeof(struct sir_sme_roam_restart_req));
	if (msg == NULL) {
		VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
			FL("Memory allocation failed"));
		VOS_ASSERT(msg);
		return;
	}
	vos_mem_set(msg, sizeof(struct sir_sme_roam_restart_req), 0);
	msg->message_type = eWNI_SME_ROAM_RESTART_REQ;
	msg->length = sizeof(struct sir_sme_roam_restart_req);
	msg->sme_session_id = session_id;
	msg->command = command;
	msg->reason = reason;
	status = palSendMBMessage(pMac->hHdd, msg);
	if (eHAL_STATUS_FAILURE == status) {
		VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
			FL("Sending msg eWNI_SME_ROAM_RESTART_REQ failed"));
		vos_mem_free(msg);
	}
}
eHalStatus csrRoamOffloadScan(tpAniSirGlobal pMac, tANI_U8 sessionId,
                              tANI_U8 command, tANI_U8 reason)
{
   vos_msg_t msg;
   tSirRoamOffloadScanReq *pRequestBuf;
   tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
                                     &pMac->roam.neighborRoamInfo[sessionId];
   tCsrRoamSession *pSession;
   tANI_U8 i,j,num_channels = 0, ucDot11Mode;
   tANI_U8 *ChannelList = NULL;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tpCsrChannelInfo    currChannelListInfo;
   tANI_U32 host_channels = 0;
   eCsrBand eBand;
   tANI_U8 ChannelCacheStr[128] = {0};
   struct roam_ext_params *roam_params_dst;
   struct roam_ext_params *roam_params_src;
   uint8_t op_channel;
   currChannelListInfo = &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;

   pSession = CSR_GET_SESSION( pMac, sessionId );

   if (NULL == pSession)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                 "%s:pSession is null", __func__);
       return eHAL_STATUS_FAILURE;
   }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
   if (pSession->roamOffloadSynchParams.bRoamSynchInProgress
       && (ROAM_SCAN_OFFLOAD_STOP == command))
   {
        /* When roam synch is in progress for propagation, there is no
         * need to send down the STOP command since the firmware is not
         * expecting any WMI commands when the roam synch is in progress.*/
         bRoamScanOffloadStarted = VOS_FALSE;
         return eHAL_STATUS_SUCCESS;
   }
#endif
   if (0 == csrRoamIsRoamOffloadScanEnabled(pMac))
   {
      smsLog( pMac, LOGE,"isRoamOffloadScanEnabled not set");
      return eHAL_STATUS_FAILURE;
   }

   if (!csr_is_RSO_cmd_allowed(pMac, command, sessionId) &&
          reason != REASON_ROAM_SET_BLACKLIST_BSSID) {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
        FL("RSO out-of-sync command %d lastSentCmd %d"),
        command, pNeighborRoamInfo->lastSentCmd);
      return eHAL_STATUS_FAILURE;
   }
   if (ROAM_SCAN_OFFLOAD_RESTART == command) {
       csr_roam_send_restart_cmd(pMac, sessionId, command, reason);
       goto cmd_sent;
   }
   if ((VOS_TRUE == bRoamScanOffloadStarted) && (ROAM_SCAN_OFFLOAD_START == command))
   {
     smsLog( pMac, LOGE,"Roam Scan Offload is already started");
     return eHAL_STATUS_FAILURE;
   }
   /*The Dynamic Config Items Update may happen even if the state is in INIT.
    * It is important to ensure that the command is passed down to the FW only
    * if the Infra Station is in a connected state.A connected station could also be
    * in a PREAUTH or REASSOC states.So, consider not sending the command down in INIT state.
    * We also have to ensure that if there is a STOP command we always have to inform Riva,
    * irrespective of whichever state we are in.*/

   if ((pMac->roam.neighborRoamInfo[sessionId].neighborRoamState ==
        eCSR_NEIGHBOR_ROAM_STATE_INIT) &&
       (command != ROAM_SCAN_OFFLOAD_STOP) &&
       (reason != REASON_ROAM_SET_BLACKLIST_BSSID))
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
            FL("Scan Command not sent to FW with state = %s and cmd=%d"),
            macTraceGetNeighbourRoamState(
            pMac->roam.neighborRoamInfo[sessionId].neighborRoamState), command);
      return eHAL_STATUS_FAILURE;
   }

   pRequestBuf = vos_mem_malloc(sizeof(tSirRoamOffloadScanReq));
   if (NULL == pRequestBuf)
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
              FL("Not able to allocate memory for Roam Offload scan request)"));
       return eHAL_STATUS_FAILED_ALLOC;
   }

    vos_mem_zero(pRequestBuf, sizeof(tSirRoamOffloadScanReq));
    pRequestBuf->Command = command;
    /* If command is STOP, then pass down ScanOffloadEnabled as Zero.This will handle the case of
     * host driver reloads, but Riva still up and running*/
    if(command == ROAM_SCAN_OFFLOAD_STOP) {
      /* clear the roaming parameters that are per connection.
       * For a new connection, they have to be programmed again.
       */
      if (csrNeighborMiddleOfRoaming((tHalHandle)pMac, sessionId)) {
          pRequestBuf->middle_of_roaming = 1;
      } else {
          csr_roam_reset_roam_params(pMac);
      }
      pRequestBuf->RoamScanOffloadEnabled = 0;
    }
    else
       pRequestBuf->RoamScanOffloadEnabled = pMac->roam.configParam.isRoamOffloadScanEnabled;
    vos_mem_copy(pRequestBuf->ConnectedNetwork.currAPbssid,
                 pNeighborRoamInfo->currAPbssid,
                 sizeof(tCsrBssid));
    pRequestBuf->ConnectedNetwork.ssId.length =
            pMac->roam.roamSession[sessionId].connectedProfile.SSID.length;
    vos_mem_copy(pRequestBuf->ConnectedNetwork.ssId.ssId,
                 pMac->roam.roamSession[sessionId].connectedProfile.SSID.ssId,
                 pRequestBuf->ConnectedNetwork.ssId.length);
    pRequestBuf->ConnectedNetwork.authentication =
            pMac->roam.roamSession[sessionId].connectedProfile.AuthType;
    pRequestBuf->ConnectedNetwork.encryption =
            pMac->roam.roamSession[sessionId].connectedProfile.EncryptionType;
    pRequestBuf->ConnectedNetwork.mcencryption =
            pMac->roam.roamSession[sessionId].connectedProfile.mcEncryptionType;
#ifdef WLAN_FEATURE_11W
    pRequestBuf->ConnectedNetwork.MFPEnabled =
           pMac->roam.roamSession[sessionId].connectedProfile.MFPEnabled;
#endif
    pRequestBuf->delay_before_vdev_stop =
            pNeighborRoamInfo->cfgParams.delay_before_vdev_stop;
    pRequestBuf->OpportunisticScanThresholdDiff =
            pNeighborRoamInfo->cfgParams.nOpportunisticThresholdDiff;
    pRequestBuf->RoamRescanRssiDiff =
            pNeighborRoamInfo->cfgParams.nRoamRescanRssiDiff;
    pRequestBuf->reason = reason;
    pRequestBuf->NeighborScanTimerPeriod =
            pNeighborRoamInfo->cfgParams.neighborScanPeriod;
    pRequestBuf->NeighborRoamScanRefreshPeriod =
            pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod;
    pRequestBuf->NeighborScanChannelMinTime =
            pNeighborRoamInfo->cfgParams.minChannelScanTime;
    pRequestBuf->NeighborScanChannelMaxTime =
            pNeighborRoamInfo->cfgParams.maxChannelScanTime;
    pRequestBuf->EmptyRefreshScanPeriod =
            pNeighborRoamInfo->cfgParams.emptyScanRefreshPeriod;
    pRequestBuf->RoamBmissFirstBcnt =
            pNeighborRoamInfo->cfgParams.nRoamBmissFirstBcnt;
    pRequestBuf->RoamBmissFinalBcnt =
            pNeighborRoamInfo->cfgParams.nRoamBmissFinalBcnt;
    pRequestBuf->RoamBeaconRssiWeight =
            pNeighborRoamInfo->cfgParams.nRoamBeaconRssiWeight;
    /* MAWC feature */
    pRequestBuf->MAWCEnabled =
            pMac->roam.configParam.MAWCEnabled;
#ifdef FEATURE_WLAN_ESE
    pRequestBuf->IsESEAssoc = csrNeighborRoamIsESEAssoc(pMac, sessionId) &&
    ((pRequestBuf->ConnectedNetwork.authentication ==
                                            eCSR_AUTH_TYPE_OPEN_SYSTEM)  ||
    (csrIsAuthTypeESE(pRequestBuf->ConnectedNetwork.authentication)));
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
             "LFR3:%s:IsEseAssoc=%d\n", __func__, pRequestBuf->IsESEAssoc);
#endif
    if (
#ifdef FEATURE_WLAN_ESE
       ((pNeighborRoamInfo->isESEAssoc) &&
        (pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived ==
         eANI_BOOLEAN_FALSE)) ||
        (pNeighborRoamInfo->isESEAssoc == eANI_BOOLEAN_FALSE) ||
#endif // ESE
        currChannelListInfo->numOfChannels == 0) {
           /* Retrieve the Channel Cache either from ini or from the Occupied
            * Channels list. Give Preference to INI Channels.*/
        if (pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels) {
            ChannelList = pNeighborRoamInfo->cfgParams.channelInfo.ChannelList;
            /* The INI channels need to be filtered with respect to the current
             * band that is supported. */
            eBand = pMac->roam.configParam.bandCapability;
            if ((eCSR_BAND_24 != eBand) && (eCSR_BAND_5G != eBand) &&
                (eCSR_BAND_ALL != eBand)) {
                VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                   "Invalid band, No operation carried out (Band %d)", eBand);
                vos_mem_free(pRequestBuf);
                return eHAL_STATUS_FAILURE;
            }

            for (i = 0;
                 i < pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels;
                 i++) {
                if (((eCSR_BAND_24 == eBand) &&
                     CSR_IS_CHANNEL_24GHZ(*ChannelList)) ||
                    ((eCSR_BAND_5G == eBand) &&
                     CSR_IS_CHANNEL_5GHZ(*ChannelList)) ||
                    (eCSR_BAND_ALL == eBand)) {
                    /* Allow DFS channels only if the DFS channel
                     * roam flag is enabled */
                    if (((pMac->roam.configParam.allowDFSChannelRoam
                                != CSR_ROAMING_DFS_CHANNEL_DISABLED) ||
                        (!CSR_IS_CHANNEL_DFS(*ChannelList))) &&
                        csrRoamIsChannelValid(pMac, *ChannelList) &&
                                              *ChannelList &&
                                       (num_channels < SIR_ROAM_MAX_CHANNELS)) {
                        pRequestBuf->ConnectedNetwork.ChannelCache[
                                             num_channels++] = *ChannelList;
                    }
                }
                ChannelList++;
            }
            pRequestBuf->ConnectedNetwork.ChannelCount = num_channels;
            pRequestBuf->ChannelCacheType = CHANNEL_LIST_STATIC;
    } else {
        ChannelList = pMac->scan.occupiedChannels[sessionId].channelList;
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
               "Num of channels before filtering=%d",
                pMac->scan.occupiedChannels[sessionId].numChannels);
        for (i = 0;
             i < pMac->scan.occupiedChannels[sessionId].numChannels;
             i++) {
            if(((pMac->roam.configParam.allowDFSChannelRoam
                                != CSR_ROAMING_DFS_CHANNEL_DISABLED) ||
               (!CSR_IS_CHANNEL_DFS(*ChannelList))) && *ChannelList &&
               (num_channels < SIR_ROAM_MAX_CHANNELS)) {
                pRequestBuf->ConnectedNetwork.ChannelCache[num_channels++] =
                                                                   *ChannelList;
              }
              if (*ChannelList)
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                       "DFSRoam=%d, ChnlState=%d, Chnl=%d, num_ch=%d",
                       pMac->roam.configParam.allowDFSChannelRoam,
                       vos_nv_getChannelEnabledState(*ChannelList),
                       *ChannelList,
                       num_channels);
              ChannelList++;
        }
        pRequestBuf->ConnectedNetwork.ChannelCount = num_channels;
        /* If the profile changes as to what it was earlier, inform the
         * FW through FLUSH as ChannelCacheType in which case,
         * the FW will flush the occupied channels for the earlier profile and
         * try to learn them afresh.*/
        if (reason == REASON_FLUSH_CHANNEL_LIST)
            pRequestBuf->ChannelCacheType = CHANNEL_LIST_DYNAMIC_FLUSH;
        else {
            if (csrNeighborRoamIsNewConnectedProfile(pMac, sessionId))
                pRequestBuf->ChannelCacheType = CHANNEL_LIST_DYNAMIC_INIT;
            else
                pRequestBuf->ChannelCacheType = CHANNEL_LIST_DYNAMIC_UPDATE;
        }
    }
    }
#ifdef FEATURE_WLAN_ESE
    else
    {
      /* If ESE is enabled, and a neighbor Report is received,then
       * Ignore the INI Channels or the Occupied Channel List. Consider
       * the channels in the neighbor list sent by the ESE AP.*/
       if (currChannelListInfo->numOfChannels != 0)
       {
          ChannelList = currChannelListInfo->ChannelList;
          for (i=0;i<currChannelListInfo->numOfChannels;i++)
          {
           if(((pMac->roam.configParam.allowDFSChannelRoam
                                != CSR_ROAMING_DFS_CHANNEL_DISABLED) ||
                (!CSR_IS_CHANNEL_DFS(*ChannelList))) && *ChannelList)
           {
            pRequestBuf->ConnectedNetwork.ChannelCache[num_channels++] = *ChannelList;
           }
           ChannelList++;
          }
          pRequestBuf->ConnectedNetwork.ChannelCount = num_channels;
          pRequestBuf->ChannelCacheType = CHANNEL_LIST_DYNAMIC_UPDATE;
       }
     }
#endif
    for (i = 0, j = 0; i < pRequestBuf->ConnectedNetwork.ChannelCount; i++)
    {
        if (j < sizeof(ChannelCacheStr))
        {
            j += snprintf(ChannelCacheStr + j, sizeof(ChannelCacheStr) - j," %d",
                          pRequestBuf->ConnectedNetwork.ChannelCache[i]);
        }
        else
        {
            break;
        }
    }
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
              "ChnlCacheType:%d, No of Chnls:%d,Channels: %s",
              pRequestBuf->ChannelCacheType,
              pRequestBuf->ConnectedNetwork.ChannelCount,
              ChannelCacheStr);
    num_channels = 0;
    ChannelList = NULL;

    /* Maintain the Valid Channels List*/
    host_channels = sizeof(pMac->roam.validChannelList);
    if (HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, pMac->roam.validChannelList, &host_channels)))
    {
        ChannelList = pMac->roam.validChannelList;
        pMac->roam.numValidChannels = host_channels;
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
              "%s:Failed to get the valid channel list", __func__);
        vos_mem_free(pRequestBuf);
        return eHAL_STATUS_FAILURE;
    }
    for(i=0; i<pMac->roam.numValidChannels; i++)
    {
        if(((pMac->roam.configParam.allowDFSChannelRoam
                                != CSR_ROAMING_DFS_CHANNEL_DISABLED) ||
            (!CSR_IS_CHANNEL_DFS(*ChannelList))) && *ChannelList)
        {
            pRequestBuf->ValidChannelList[num_channels++] = *ChannelList;
        }
        ChannelList++;
    }
    pRequestBuf->ValidChannelCount = num_channels;

    pRequestBuf->MDID.mdiePresent =
            pMac->roam.roamSession[sessionId].connectedProfile.MDID.mdiePresent;
    pRequestBuf->MDID.mobilityDomain =
            pMac->roam.roamSession[sessionId].connectedProfile.MDID.mobilityDomain;
    pRequestBuf->sessionId = sessionId;
    pRequestBuf->nProbes = pMac->roam.configParam.nProbes;

    pRequestBuf->HomeAwayTime = pMac->roam.configParam.nRoamScanHomeAwayTime;

   /* Home Away Time should be at least equal to (MaxDwell time + (2*RFS)),
    * where RFS is the RF Switching time. It is twice RFS to consider the
    * time to go off channel and return to the home channel. */
    if (pRequestBuf->HomeAwayTime < (pRequestBuf->NeighborScanChannelMaxTime + (2 * CSR_ROAM_SCAN_CHANNEL_SWITCH_TIME)))
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_WARN,
                  "%s: Invalid config, Home away time(%d) is less than (twice RF switching time + channel max time)(%d)"
                  " Hence enforcing home away time to disable (0)",
                  __func__, pRequestBuf->HomeAwayTime,
                  (pRequestBuf->NeighborScanChannelMaxTime + (2 * CSR_ROAM_SCAN_CHANNEL_SWITCH_TIME)
                   ));
        pRequestBuf->HomeAwayTime = 0;
    }
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,"HomeAwayTime:%d",pRequestBuf->HomeAwayTime);

   /*Prepare a probe request for 2.4GHz band and one for 5GHz band*/
    ucDot11Mode = (tANI_U8) csrTranslateToWNICfgDot11Mode(pMac,
                                                           csrFindBestPhyMode( pMac, pMac->roam.configParam.phyMode ));
   csrRoamScanOffloadPrepareProbeReqTemplate(pMac,SIR_ROAM_SCAN_24G_DEFAULT_CH, ucDot11Mode, pSession->selfMacAddr,
                                             pRequestBuf->p24GProbeTemplate,
                                             &pRequestBuf->us24GProbeTemplateLen,
                                             pSession);

   csrRoamScanOffloadPrepareProbeReqTemplate(pMac,SIR_ROAM_SCAN_5G_DEFAULT_CH, ucDot11Mode, pSession->selfMacAddr,
                                             pRequestBuf->p5GProbeTemplate,
                                             &pRequestBuf->us5GProbeTemplateLen,
                                             pSession);
   pRequestBuf->allowDFSChannelRoam = pMac->roam.configParam.allowDFSChannelRoam;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
   pRequestBuf->RoamOffloadEnabled = csrRoamIsRoamOffloadEnabled(pMac);
   pRequestBuf->RoamKeyMgmtOffloadEnabled = pSession->RoamKeyMgmtOffloadEnabled;
   /* Roam Offload piggybacks upon the Roam Scan offload command.*/
   if (pRequestBuf->RoamOffloadEnabled){
       csrRoamOffload(pMac, pRequestBuf, pSession);
   }
#endif
   roam_params_dst = &pRequestBuf->roam_params;
   roam_params_src = &pMac->roam.configParam.roam_params;
   if (reason == REASON_ROAM_SET_SSID_ALLOWED)
      check_allowed_ssid_list(pRequestBuf, roam_params_src);
   /* Configure the lookup threshold either from INI or from framework.
    * If both are present, give higher priority to the one from framework.
    */
   if (roam_params_src->alert_rssi_threshold)
       pRequestBuf->LookupThreshold = roam_params_src->alert_rssi_threshold;
   else
       pRequestBuf->LookupThreshold =
         (v_S7_t)pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1);
   vos_mem_copy(roam_params_dst, roam_params_src,
               sizeof(struct roam_ext_params));
   pRequestBuf->hi_rssi_scan_max_count =
           pNeighborRoamInfo->cfgParams.hi_rssi_scan_max_count;
   pRequestBuf->hi_rssi_scan_delay =
           pNeighborRoamInfo->cfgParams.hi_rssi_scan_delay;
   pRequestBuf->hi_rssi_scan_rssi_ub =
           pNeighborRoamInfo->cfgParams.hi_rssi_scan_rssi_ub;
   /* rssi_diff which is updated via framework is equivalent to the
    * INI RoamRssiDiff parameter and hence should be updated.*/
   if (roam_params_src->rssi_diff)
        pMac->roam.configParam.RoamRssiDiff = roam_params_src->rssi_diff;
   pRequestBuf->RoamRssiDiff =
        pMac->roam.configParam.RoamRssiDiff;
   op_channel = pSession->connectedProfile.operationChannel;
   /* If the current operation channel is 5G frequency band, then
    * there is no need to enable the HI_RSSI feature. This feature
    * is useful only if we are connected to a 2.4 GHz AP and we wish
    * to connect to a better 5GHz AP is available.*/
   if(CSR_IS_CHANNEL_5GHZ(op_channel)) {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
        "Disabling HI_RSSI feature since the connected AP is 5GHz");
      pRequestBuf->hi_rssi_scan_rssi_delta = 0;
   }
   else {
      pRequestBuf->hi_rssi_scan_rssi_delta =
           pNeighborRoamInfo->cfgParams.hi_rssi_scan_rssi_delta;
   }
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
      "hi_rssi_delta=%d, hi_rssi_max_count=%d,"
      "hi_rssi_delay=%d, hi_rssi_ub=%d",
      pRequestBuf->hi_rssi_scan_rssi_delta,
      pRequestBuf->hi_rssi_scan_max_count,
      pRequestBuf->hi_rssi_scan_delay,
      pRequestBuf->hi_rssi_scan_rssi_ub);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
     "num_bssid_avoid_list: %d, num_ssid_allowed_list:%d, num_bssid_favored:%d,"
     "raise_rssi_thresh_5g: %d, drop_rssi_thresh_5g:%d, raise_rssi_type_5g:%d,"
     "raise_factor_5g:%d, drop_rssi_type_5g:%d, drop_factor_5g:%d,"
     "max_raise_rssi_5g=%d, max_drop_rssi_5g:%d, alert_rssi_threshold:%d",
     roam_params_dst->num_bssid_avoid_list,
     roam_params_dst->num_ssid_allowed_list, roam_params_dst->num_bssid_favored,
     roam_params_dst->raise_rssi_thresh_5g,
     roam_params_dst->drop_rssi_thresh_5g, roam_params_dst->raise_rssi_type_5g,
     roam_params_dst->raise_factor_5g, roam_params_dst->drop_rssi_type_5g,
     roam_params_dst->drop_factor_5g, roam_params_dst->max_raise_rssi_5g,
     roam_params_dst->max_drop_rssi_5g, roam_params_dst->alert_rssi_threshold);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
     "dense_rssi_thresh_offset: %d, dense_min_aps_cnt:%d, initial_dense_status:%d, traffic_threshold:%d",
     roam_params_dst->dense_rssi_thresh_offset,
     roam_params_dst->dense_min_aps_cnt,
     roam_params_dst->initial_dense_status,
     roam_params_dst->traffic_threshold);

    for (i = 0; i < roam_params_dst->num_bssid_avoid_list; i++) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
          "Blacklist Bssid("MAC_ADDRESS_STR")",
           MAC_ADDR_ARRAY(roam_params_dst->bssid_avoid_list[i]));
    }
    for (i = 0; i < roam_params_dst->num_ssid_allowed_list; i++) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG, "Whitelist: %.*s",
            roam_params_dst->ssid_allowed_list[i].length,
            roam_params_dst->ssid_allowed_list[i].ssId);
    }
    for (i = 0; i < roam_params_dst->num_bssid_favored; i++) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
          "Preferred Bssid("MAC_ADDRESS_STR") score=%d",
           MAC_ADDR_ARRAY(roam_params_dst->bssid_favored[i]),
           roam_params_dst->bssid_favored_factor[i]);
    }
   msg.type     = WDA_ROAM_SCAN_OFFLOAD_REQ;
   msg.reserved = 0;
   msg.bodyptr  = pRequestBuf;
   if (!VOS_IS_STATUS_SUCCESS(vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)))
   {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, "%s: Not able to post WDA_ROAM_SCAN_OFFLOAD_REQ message to WDA", __func__);
       vos_mem_free(pRequestBuf);
       return eHAL_STATUS_FAILURE;
   }
   else
   {
        if (ROAM_SCAN_OFFLOAD_START == command)
            bRoamScanOffloadStarted = VOS_TRUE;
        else if (ROAM_SCAN_OFFLOAD_STOP == command)
            bRoamScanOffloadStarted = VOS_FALSE;
   }
cmd_sent:
   /* update the last sent cmd */
   pNeighborRoamInfo->lastSentCmd = command;

   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG, "Roam Scan Offload Command %d, Reason %d", command, reason);
   return status;
}

eHalStatus csrRoamOffloadScanRspHdlr(tpAniSirGlobal pMac,
                                     tpSirRoamOffloadScanRsp scanOffloadRsp)
{
    switch (scanOffloadRsp->reason) {
    case 0:
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_DEBUG,
                  "Rsp for Roam Scan Offload with failure status");
        break;
    case REASON_OS_REQUESTED_ROAMING_NOW:
        csrNeighborRoamProceedWithHandoffReq(pMac,
                                            scanOffloadRsp->sessionId);
        break;

    default:
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  "Rsp for Roam Scan Offload with reason %d",
                  scanOffloadRsp->reason);
    }
    return eHAL_STATUS_SUCCESS;
}
#endif

tCsrPeStatsReqInfo * csrRoamCheckPeStatsReqList(tpAniSirGlobal pMac,
                                                tANI_U32  statsMask,
                                                tANI_U32 periodicity,
                                                tANI_BOOLEAN *pFound,
                                                tANI_U8 staId,
                                                tANI_U8 sessionId)
{
   tANI_BOOLEAN found = FALSE;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tCsrPeStatsReqInfo staEntry;
   tCsrPeStatsReqInfo *pTempStaEntry = NULL;
   tListElem *pStaEntry = NULL;
   VOS_STATUS vosStatus;
   tPmcPowerState powerState;
   *pFound = FALSE;

   pStaEntry = csrRoamFindInPeStatsReqList(pMac, statsMask);
   if(pStaEntry)
   {
      pTempStaEntry = GET_BASE_ADDR( pStaEntry, tCsrPeStatsReqInfo, link );
      if(pTempStaEntry->periodicity)
      {
         pTempStaEntry->periodicity =
            CSR_ROAM_MIN(periodicity, pTempStaEntry->periodicity);
      }
      else
      {
         pTempStaEntry->periodicity = periodicity;
      }
      pTempStaEntry->numClient++;
         found = TRUE;
   }
   else
   {
      vos_mem_set(&staEntry, sizeof(tCsrPeStatsReqInfo), 0);
      staEntry.numClient = 1;
      staEntry.periodicity = periodicity;
      staEntry.pMac = pMac;
      staEntry.rspPending = FALSE;
      staEntry.staId = staId;
      staEntry.statsMask = statsMask;
      staEntry.timerRunning = FALSE;
      staEntry.sessionId = sessionId;
      pTempStaEntry = csrRoamInsertEntryIntoPeStatsReqList(pMac, &pMac->roam.peStatsReqList, &staEntry);
      if(!pTempStaEntry)
      {
         //msg
         smsLog(pMac, LOGW, "csrRoamCheckPeStatsReqList: Failed to insert req in peStatsReqList");
         return NULL;
      }
   }
   pmcQueryPowerState(pMac, &powerState, NULL, NULL);
   if(ePMC_FULL_POWER == powerState)
   {
      if(pTempStaEntry->periodicity < pMac->roam.configParam.statsReqPeriodicity)
      {
         pTempStaEntry->periodicity = pMac->roam.configParam.statsReqPeriodicity;
      }
   }
   else
   {
      if(pTempStaEntry->periodicity < pMac->roam.configParam.statsReqPeriodicityInPS)
      {
         pTempStaEntry->periodicity = pMac->roam.configParam.statsReqPeriodicityInPS;
      }
   }
   if(!pTempStaEntry->timerRunning)
   {
      //send down a req in case of one time req, for periodic ones wait for timer to expire
      if(!pTempStaEntry->rspPending &&
         !pTempStaEntry->periodicity)
      {
         status = csrSendMBStatsReqMsg(pMac,
                                     statsMask & ~(1 << eCsrGlobalClassDStats),
                                     staId,
                                     sessionId);
         if(!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:failed to send down stats req to PE"));
         }
         else
         {
            pTempStaEntry->rspPending = TRUE;
         }
      }
      if(pTempStaEntry->periodicity)
      {
         if(!found)
         {

            vosStatus = vos_timer_init( &pTempStaEntry->hPeStatsTimer, VOS_TIMER_TYPE_SW,
                                        csrRoamPeStatsTimerHandler, pTempStaEntry );
            if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
            {
               smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:cannot init hPeStatsTimer timer"));
               return NULL;
            }
         }
         //start timer
         smsLog(pMac, LOG1, "csrRoamCheckPeStatsReqList:peStatsTimer period %d", pTempStaEntry->periodicity);
         vosStatus = vos_timer_start( &pTempStaEntry->hPeStatsTimer, pTempStaEntry->periodicity );
         if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
         {
            smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:cannot start hPeStatsTimer timer"));
            return NULL;
         }
         pTempStaEntry->timerRunning = TRUE;
      }
   }
   *pFound = found;
   return pTempStaEntry;
}

/*
    pStaEntry is no longer invalid upon the return of this function.
*/
static void csrRoamRemoveStatListEntry(tpAniSirGlobal pMac, tListElem *pEntry)
{
    if(pEntry)
    {
        if(csrLLRemoveEntry(&pMac->roam.statsClientReqList, pEntry, LL_ACCESS_LOCK))
        {
            vos_mem_free(GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link ));
            }
        }
    }

void csrRoamRemoveEntryFromPeStatsReqList(tpAniSirGlobal pMac, tCsrPeStatsReqInfo *pPeStaEntry)
{
   tListElem *pEntry;
   tCsrPeStatsReqInfo *pTempStaEntry;
   VOS_STATUS vosStatus;
   pEntry = csrLLPeekHead( &pMac->roam.peStatsReqList, LL_ACCESS_LOCK );
   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOGE, FL(" List empty, no stats req for PE"));
      return;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrPeStatsReqInfo, link );
      if( pTempStaEntry && pTempStaEntry->statsMask == pPeStaEntry->statsMask)
      {
         smsLog(pMac, LOGW, FL("Match found"));
         if(pTempStaEntry->timerRunning)
         {
            vosStatus = vos_timer_stop( &pTempStaEntry->hPeStatsTimer );
            /* If we are not able to stop the timer here, just remove
             * the entry from the linked list. Destroy the timer object
             * and free the memory in the timer CB
             */
            if ( vosStatus == VOS_STATUS_SUCCESS )
            {
               /* the timer is successfully stopped */
               pTempStaEntry->timerRunning = FALSE;

               /* Destroy the timer */
               vosStatus = vos_timer_destroy( &pTempStaEntry->hPeStatsTimer );
               if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
               {
                  smsLog(pMac, LOGE, FL("csrRoamRemoveEntryFromPeStatsReqList:failed to destroy hPeStatsTimer timer"));
               }
            }
            else
            {
               // the timer could not be stopped. Hence destroy and free the
               // memory for the PE stat entry in the timer CB.
               pTempStaEntry->timerStopFailed = TRUE;
            }
         }

         if(csrLLRemoveEntry(&pMac->roam.peStatsReqList, pEntry, LL_ACCESS_LOCK))
         {
            // Only free the memory if we could stop the timer successfully
            if(!pTempStaEntry->timerStopFailed)
            {
                vos_mem_free(pTempStaEntry);
               pTempStaEntry = NULL;
            }
            break;
         }

         pEntry = csrLLNext( &pMac->roam.peStatsReqList, pEntry, LL_ACCESS_NOLOCK );
      }
   }
   return;
}


void csrRoamSaveStatsFromTl(tpAniSirGlobal pMac, WLANTL_TRANSFER_STA_TYPE *pTlStats)
{

   pMac->roam.classDStatsInfo.num_rx_bytes_crc_ok = pTlStats->rxBcntCRCok;
   pMac->roam.classDStatsInfo.rx_bc_byte_cnt = pTlStats->rxBCBcnt;
   pMac->roam.classDStatsInfo.rx_bc_frm_cnt = pTlStats->rxBCFcnt;
   pMac->roam.classDStatsInfo.rx_byte_cnt = pTlStats->rxBcnt;
   pMac->roam.classDStatsInfo.rx_mc_byte_cnt = pTlStats->rxMCBcnt;
   pMac->roam.classDStatsInfo.rx_mc_frm_cnt = pTlStats->rxMCFcnt;
   pMac->roam.classDStatsInfo.rx_rate = pTlStats->rxRate;
   //?? need per AC
   pMac->roam.classDStatsInfo.rx_uc_byte_cnt[0] = pTlStats->rxUCBcnt;
   pMac->roam.classDStatsInfo.rx_uc_frm_cnt = pTlStats->rxUCFcnt;
   pMac->roam.classDStatsInfo.tx_bc_byte_cnt = pTlStats->txBCBcnt;
   pMac->roam.classDStatsInfo.tx_bc_frm_cnt = pTlStats->txBCFcnt;
   pMac->roam.classDStatsInfo.tx_mc_byte_cnt = pTlStats->txMCBcnt;
   pMac->roam.classDStatsInfo.tx_mc_frm_cnt = pTlStats->txMCFcnt;
   //?? need per AC
   pMac->roam.classDStatsInfo.tx_uc_byte_cnt[0] = pTlStats->txUCBcnt;
   pMac->roam.classDStatsInfo.tx_uc_frm_cnt = pTlStats->txUCFcnt;

}

void csrRoamReportStatistics(tpAniSirGlobal pMac, tANI_U32 statsMask,
                             tCsrStatsCallback callback, tANI_U8 staId, void *pContext)
{
   tANI_U8 stats[500];
   tANI_U8 *pStats = NULL;
   tANI_U32 tempMask = 0;
   tANI_U8 counter = 0;
   if(!callback)
   {
      smsLog(pMac, LOGE, FL("Cannot report callback NULL"));
      return;
   }
   if(!statsMask)
   {
      smsLog(pMac, LOGE, FL("Cannot report statsMask is 0"));
      return;
   }
   pStats = stats;
   tempMask = statsMask;
   while(tempMask)
   {
      if(tempMask & 1)
      {
         /* new stats info from PE, fill up the stats structures in PMAC */
         switch(counter)
         {
         case eCsrSummaryStats:
            smsLog( pMac, LOG2, FL("Summary stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.summaryStatsInfo,
                         sizeof(tCsrSummaryStatsInfo));
            pStats += sizeof(tCsrSummaryStatsInfo);
            break;
         case eCsrGlobalClassAStats:
            smsLog( pMac, LOG2, FL("ClassA stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.classAStatsInfo,
                         sizeof(tCsrGlobalClassAStatsInfo));
            pStats += sizeof(tCsrGlobalClassAStatsInfo);
            break;
         case eCsrGlobalClassBStats:
            smsLog( pMac, LOG2, FL("ClassB stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.classBStatsInfo,
                         sizeof(tCsrGlobalClassBStatsInfo));
            pStats += sizeof(tCsrGlobalClassBStatsInfo);
            break;
         case eCsrGlobalClassCStats:
            smsLog( pMac, LOG2, FL("ClassC stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.classCStatsInfo,
                         sizeof(tCsrGlobalClassCStatsInfo));
            pStats += sizeof(tCsrGlobalClassCStatsInfo);
            break;
         case eCsrGlobalClassDStats:
            smsLog( pMac, LOG2, FL("ClassD stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.classDStatsInfo,
                          sizeof(tCsrGlobalClassDStatsInfo));
            pStats += sizeof(tCsrGlobalClassDStatsInfo);
            break;
         case eCsrPerStaStats:
            smsLog( pMac, LOG2, FL("PerSta stats"));
            vos_mem_copy( pStats, (tANI_U8 *)&pMac->roam.perStaStatsInfo[staId],
                         sizeof(tCsrPerStaStatsInfo));
            pStats += sizeof(tCsrPerStaStatsInfo);
            break;
         default:
            smsLog( pMac, LOGE, FL("Unknown stats type and counter %d"), counter);
            break;
         }
      }
      tempMask >>=1;
      counter++;
   }
   callback(stats, pContext );
}

eHalStatus csrRoamDeregStatisticsReq(tpAniSirGlobal pMac)
{
   tListElem *pEntry = NULL;
   tListElem *pPrevEntry = NULL;
   tCsrStatsClientReqInfo *pTempStaEntry = NULL;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   VOS_STATUS vosStatus;
   pEntry = csrLLPeekHead( &pMac->roam.statsClientReqList, LL_ACCESS_LOCK );
   if(!pEntry)
   {
      //list empty
      smsLog(pMac, LOGW, "csrRoamDeregStatisticsReq: List empty, no request from "
             "upper layer client(s)");
      return status;
   }
   while( pEntry )
   {
      if(pPrevEntry)
      {
         pTempStaEntry = GET_BASE_ADDR( pPrevEntry, tCsrStatsClientReqInfo, link );
         //send up the stats report
         csrRoamReportStatistics(pMac, pTempStaEntry->statsMask, pTempStaEntry->callback,
                                 pTempStaEntry->staId, pTempStaEntry->pContext);
         csrRoamRemoveStatListEntry(pMac, pPrevEntry);
      }
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if (pTempStaEntry->pPeStaEntry)  //pPeStaEntry can be NULL
      {
         pTempStaEntry->pPeStaEntry->numClient--;
         //check if we need to delete the entry from peStatsReqList too
         if(!pTempStaEntry->pPeStaEntry->numClient)
         {
            csrRoamRemoveEntryFromPeStatsReqList(pMac, pTempStaEntry->pPeStaEntry);
         }
      }
      //check if we need to stop the tl stats timer too
      pMac->roam.tlStatsReqInfo.numClient--;
      if(!pMac->roam.tlStatsReqInfo.numClient)
      {
         if(pMac->roam.tlStatsReqInfo.timerRunning)
         {
            status = vos_timer_stop(&pMac->roam.tlStatsReqInfo.hTlStatsTimer);
            if (!HAL_STATUS_SUCCESS(status))
            {
               smsLog(pMac, LOGE, FL("csrRoamDeregStatisticsReq:cannot stop TlStatsTimer timer"));
               //we will continue
            }
         }
         pMac->roam.tlStatsReqInfo.periodicity = 0;
         pMac->roam.tlStatsReqInfo.timerRunning = FALSE;
      }
      if (pTempStaEntry->periodicity)
      {
          //While creating StaEntry in csrGetStatistics,
          //Initializing and starting timer only when periodicity is set.
          //So Stop and Destroy timer only when periodicity is set.

          vos_timer_stop( &pTempStaEntry->timer );
          // Destroy the vos timer...
          vosStatus = vos_timer_destroy( &pTempStaEntry->timer );
          if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
          {
              smsLog(pMac, LOGE, FL("csrRoamDeregStatisticsReq:failed to destroy Client req timer"));
          }
      }


      pPrevEntry = pEntry;
      pEntry = csrLLNext( &pMac->roam.statsClientReqList, pEntry, LL_ACCESS_NOLOCK );
   }
   //the last one
   if(pPrevEntry)
   {
      pTempStaEntry = GET_BASE_ADDR( pPrevEntry, tCsrStatsClientReqInfo, link );
      //send up the stats report
      csrRoamReportStatistics(pMac, pTempStaEntry->statsMask, pTempStaEntry->callback,
                                 pTempStaEntry->staId, pTempStaEntry->pContext);
      csrRoamRemoveStatListEntry(pMac, pPrevEntry);
   }
   return status;

}

eHalStatus csrIsFullPowerNeeded( tpAniSirGlobal pMac, tSmeCmd *pCommand,
                                   tRequestFullPowerReason *pReason,
                                   tANI_BOOLEAN *pfNeedPower )
{
    tANI_BOOLEAN fNeedFullPower = eANI_BOOLEAN_FALSE;
    tRequestFullPowerReason reason = eSME_REASON_OTHER;
    tPmcState pmcState;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 sessionId = 0;
    if( pfNeedPower )
    {
        *pfNeedPower = eANI_BOOLEAN_FALSE;
    }
        //We only handle CSR commands
        if( !(eSmeCsrCommandMask & pCommand->command) )
        {
                return eHAL_STATUS_SUCCESS;
        }
    //Check PMC state first
    pmcState = pmcGetPmcState( pMac );
    switch( pmcState )
    {
    case REQUEST_IMPS:
    case IMPS:
        if( eSmeCommandScan == pCommand->command )
        {
            switch( pCommand->u.scanCmd.reason )
            {
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
            case eCsrScanGetLfrResult:
#endif
            case eCsrScanGetResult:
            case eCsrScanBGScanAbort:
            case eCsrScanBGScanEnable:
            case eCsrScanGetScanChnInfo:
                //Internal process, no need for full power
                fNeedFullPower = eANI_BOOLEAN_FALSE;
                break;
            default:
                //Other scans are real scan, ask for power
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                break;
            } //switch
        }
        else
        {
            //ask for power for roam and status change
            fNeedFullPower = eANI_BOOLEAN_TRUE;
        }
        break;
    case REQUEST_BMPS:
    case BMPS:
    case REQUEST_START_UAPSD:
    case UAPSD:
    //We treat WOWL same as BMPS
    case REQUEST_ENTER_WOWL:
    case WOWL:
        if( eSmeCommandRoam == pCommand->command )
        {
            tScanResultList *pBSSList = (tScanResultList *)pCommand->u.roamCmd.hBSSList;
            tCsrScanResult *pScanResult;
            tListElem *pEntry;
            switch ( pCommand->u.roamCmd.roamReason )
            {
            case eCsrForcedDisassoc:
            case eCsrForcedDisassocMICFailure:
                reason = eSME_LINK_DISCONNECTED_BY_HDD;
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                break;
                case eCsrSmeIssuedDisassocForHandoff:
            case eCsrForcedDeauth:
            case eCsrHddIssuedReassocToSameAP:
            case eCsrSmeIssuedReassocToSameAP:
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                break;
            case eCsrCapsChange:
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                break;
            default:
                /*
                 * Check whether the profile is already connected. If so,
                 * no need for full power. Note: IBSS is ignored for now
                 * because we don't support power save in IBSS
                 */
                if ( csrIsConnStateConnectedInfra(pMac, sessionId) && pBSSList )
                {
                    //Only need to check the first one
                    pEntry = csrLLPeekHead(&pBSSList->List, LL_ACCESS_LOCK);
                    if( pEntry )
                    {
                        pScanResult = GET_BASE_ADDR(pEntry, tCsrScanResult, Link);
                    }
                }
                //If we are here, full power is needed
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                break;
            }
        }
        else if( eSmeCommandWmStatusChange == pCommand->command )
        {
            //need full power for all
            fNeedFullPower = eANI_BOOLEAN_TRUE;
            reason = eSME_LINK_DISCONNECTED_BY_OTHER;
        }
#ifdef FEATURE_WLAN_TDLS
        else if( eSmeCommandTdlsAddPeer == pCommand->command )
        {
            //TDLS link is getting established. need full power
            fNeedFullPower = eANI_BOOLEAN_TRUE;
            reason = eSME_FULL_PWR_NEEDED_BY_TDLS_PEER_SETUP;
        }
#endif
        break;
    case REQUEST_STOP_UAPSD:
    case REQUEST_EXIT_WOWL:
        if( eSmeCommandRoam == pCommand->command )
        {
            fNeedFullPower = eANI_BOOLEAN_TRUE;
            switch ( pCommand->u.roamCmd.roamReason )
            {
                case eCsrForcedDisassoc:
                case eCsrForcedDisassocMICFailure:
                    reason = eSME_LINK_DISCONNECTED_BY_HDD;
                    break;
                default:
                    break;
            }
                }
        break;
    case STOPPED:
    case REQUEST_STANDBY:
    case STANDBY:
    case LOW_POWER:
        //We are not supposed to do anything
        smsLog( pMac, LOGE, FL( "cannot process because PMC is in"
                                " stopped/standby state %s (%d)" ),
                sme_PmcStatetoString(pmcState), pmcState );
        status = eHAL_STATUS_FAILURE;
        break;
    case FULL_POWER:
    case REQUEST_FULL_POWER:
    default:
        //No need to ask for full power. This has to be FULL_POWER state
        break;
    } //switch
    if( pReason )
    {
        *pReason = reason;
    }
    if( pfNeedPower )
    {
        *pfNeedPower = fNeedFullPower;
    }
    return ( status );
}

eHalStatus csrPsOffloadIsFullPowerNeeded(tpAniSirGlobal pMac,
                                         tSmeCmd *pCommand,
                                         tRequestFullPowerReason *pReason,
                                         tANI_BOOLEAN *pfNeedPower)
{
    tANI_BOOLEAN fNeedFullPower = eANI_BOOLEAN_FALSE;
    tRequestFullPowerReason reason = eSME_REASON_OTHER;
    tPmcState pmcState;
    eHalStatus status = eHAL_STATUS_SUCCESS;

    if(pfNeedPower)
    {
        *pfNeedPower = eANI_BOOLEAN_FALSE;
    }

    /* We only handle CSR commands */
    if(!(eSmeCsrCommandMask & pCommand->command))
    {
        return eHAL_STATUS_SUCCESS;
    }

    /*
     * No need to request for full power for the following commands
     * Scan Related Command
     * IMPS is handled in Fw so no need
     */
    if(eSmeCommandScan == pCommand->command)
    {
        return eHAL_STATUS_SUCCESS;
    }

    /*
     * Check PMC state first
     * Commands which require Full Power
     * 1) eSmeCommandRoam
     *       -----eCsrForcedDisassoc
     *       -----eCsrForcedDisassocMICFailure
     *       -----eCsrSmeIssuedDisassocForHandoff
     *       -----eCsrForcedDeauth
     *       -----eCsrHddIssuedReassocToSameAP
     *       -----eCsrSmeIssuedReassocToSameAP
     *       -----eCsrCapsChange
     *       -----AddTs
     *       -----DelTs
     *       ----- etc
     * 2) eSmeCommandWmStatusChange
     * 3) eSmeCommandTdlsAddPeer
     */
    pmcState = pmcOffloadGetPmcState(pMac, pCommand->sessionId);
    switch(pmcState)
    {
        case REQUEST_BMPS:
        case BMPS:
        case REQUEST_START_UAPSD:
        case REQUEST_STOP_UAPSD:
        case UAPSD:
            if(eSmeCommandRoam == pCommand->command)
            {
                switch (pCommand->u.roamCmd.roamReason)
                {
                    case eCsrForcedDisassoc:
                    case eCsrForcedDisassocMICFailure:
                        reason = eSME_LINK_DISCONNECTED_BY_HDD;
                    case eCsrSmeIssuedDisassocForHandoff:
                    case eCsrForcedDeauth:
                    case eCsrHddIssuedReassocToSameAP:
                    case eCsrSmeIssuedReassocToSameAP:
                    case eCsrCapsChange:
                    default:
                        fNeedFullPower = eANI_BOOLEAN_TRUE;
                        break;
                }
            }
            else if(eSmeCommandWmStatusChange == pCommand->command)
            {
                /* need full power for all */
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                reason = eSME_LINK_DISCONNECTED_BY_OTHER;
            }
#ifdef FEATURE_WLAN_TDLS
            else if(eSmeCommandTdlsAddPeer == pCommand->command)
            {
                /* TDLS link is getting established. need full power  */
                fNeedFullPower = eANI_BOOLEAN_TRUE;
                reason = eSME_FULL_PWR_NEEDED_BY_TDLS_PEER_SETUP;
            }
#endif
            break;
       case STOPPED:
            /* We are not supposed to do anything */
            smsLog( pMac, LOGE,
            FL("cannot process because PMC is in stopped/standby state %d"),
                pmcState );
            status = eHAL_STATUS_FAILURE;
            break;
        default:
            /* No need to ask for full power. This has to be FULL_POWER state */
            break;
    }

    if(pReason)
    {
        *pReason = reason;
    }
    if(pfNeedPower)
    {
        *pfNeedPower = fNeedFullPower;
    }
    return status;
}

static eHalStatus csrRequestFullPower( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fNeedFullPower = eANI_BOOLEAN_FALSE;
    tRequestFullPowerReason reason = eSME_REASON_OTHER;

    if(pMac->psOffloadEnabled)
        status = csrPsOffloadIsFullPowerNeeded(pMac, pCommand,
                                     &reason, &fNeedFullPower);
    else
        status = csrIsFullPowerNeeded(pMac, pCommand, &reason,
                                      &fNeedFullPower);

    if( fNeedFullPower && HAL_STATUS_SUCCESS( status ) )
    {
        if(!pMac->psOffloadEnabled)
        {
            status = pmcRequestFullPower(pMac, csrFullPowerCallback,
                                         pMac, reason);
        }
        else
        {
            status = pmcOffloadRequestFullPower(pMac, pCommand->sessionId,
                                                csrFullPowerOffloadCallback,
                                                pMac, reason);
        }

    }
    return ( status );
}

tSmeCmd *csrGetCommandBuffer( tpAniSirGlobal pMac )
{
    tSmeCmd *pCmd = smeGetCommandBuffer( pMac );
    if( pCmd )
    {
        pMac->roam.sPendingCommands++;
    }
    return ( pCmd );
}

void csrReleaseCommand(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
   if (pMac->roam.sPendingCommands > 0)
   {
       //All command allocated through csrGetCommandBuffer need to
       //decrement the pending count when releasing.
       pMac->roam.sPendingCommands--;
       smeReleaseCommand( pMac, pCommand );
   }
   else
   {
       smsLog(pMac, LOGE, FL( "no pending commands"));
       VOS_ASSERT(0);
   }
}

//Return SUCCESS is the command is queued, failed
eHalStatus csrQueueSmeCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fHighPriority )
{
    eHalStatus status;

    if (!SME_IS_START(pMac)) {
        smsLog(pMac, LOGE, FL("Sme in stop state"));
        return eHAL_STATUS_FAILURE;
    }

    if( (eSmeCommandScan == pCommand->command) && pMac->scan.fDropScanCmd )
    {
        smsLog(pMac, LOGW, FL(" drop scan (scan reason %d) command"),
           pCommand->u.scanCmd.reason);
        return eHAL_STATUS_CSR_WRONG_STATE;
    }

    if (((pMac->fScanOffload) && (pCommand->command == eSmeCommandScan)) ||
            ((pMac->fP2pListenOffload) &&
             (pCommand->command == eSmeCommandRemainOnChannel)))
    {
        csrLLInsertTail(&pMac->sme.smeScanCmdPendingList,
                        &pCommand->Link, LL_ACCESS_LOCK);
        // process the command queue...
        smeProcessPendingQueue(pMac);
        status = eHAL_STATUS_SUCCESS;
        goto end;
    }

    //We can call request full power first before putting the command into pending Q
    //because we are holding SME lock at this point.
    status = csrRequestFullPower( pMac, pCommand );
    if( HAL_STATUS_SUCCESS( status ) )
    {
        tANI_BOOLEAN fNoCmdPending;
        //make sure roamCmdPendingList is not empty first
        fNoCmdPending = csrLLIsListEmpty( &pMac->roam.roamCmdPendingList, eANI_BOOLEAN_FALSE );
        if( fNoCmdPending )
        {
            smePushCommand( pMac, pCommand, fHighPriority );
        }
        else
        {
            //Other commands are waiting for PMC callback, queue the new command to the pending Q
            //no list lock is needed since SME lock is held
            if( !fHighPriority )
            {
                csrLLInsertTail( &pMac->roam.roamCmdPendingList, &pCommand->Link, eANI_BOOLEAN_FALSE );
            }
            else {
                csrLLInsertHead( &pMac->roam.roamCmdPendingList, &pCommand->Link, eANI_BOOLEAN_FALSE );
            }
        }
    }
    else if( eHAL_STATUS_PMC_PENDING == status )
    {
        //no list lock is needed since SME lock is held
        if( !fHighPriority )
        {
            csrLLInsertTail( &pMac->roam.roamCmdPendingList, &pCommand->Link, eANI_BOOLEAN_FALSE );
        }
        else {
            csrLLInsertHead( &pMac->roam.roamCmdPendingList, &pCommand->Link, eANI_BOOLEAN_FALSE );
        }
        //Let caller know the command is queue
        status = eHAL_STATUS_SUCCESS;
    }
    else
    {
        //Not to decrease pMac->roam.sPendingCommands here. Caller will decrease it when it
        //release the command.
        smsLog( pMac, LOGE, FL( "  cannot queue command %d" ), pCommand->command );
    }
end:
    return ( status );
}
eHalStatus csrRoamUpdateAPWPSIE( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirAPWPSIEs* pAPWPSIES )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirUpdateAPWPSIEsReq *pMsg;
    tANI_U8 *pBuf = NULL, *wTmpBuf = NULL;

    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL( "  Session does not exist for session id %d" ), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    do
    {
        pMsg = vos_mem_malloc(sizeof(tSirUpdateAPWPSIEsReq));
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, sizeof(tSirUpdateAPWPSIEsReq), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_UPDATE_APWPSIE_REQ);

        pBuf = (tANI_U8 *)&pMsg->transactionId;

        if (NULL == pBuf)
        {
           VOS_ASSERT(pBuf);
           return eHAL_STATUS_FAILURE;
        }

        wTmpBuf = pBuf;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, &pSession->selfMacAddr,
                     sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        //sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // APWPSIEs
        vos_mem_copy((tSirAPWPSIEs *)pBuf, pAPWPSIES, sizeof(tSirAPWPSIEs));
        pBuf += sizeof(tSirAPWPSIEs);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32) + (pBuf - wTmpBuf))); //msg_header + msg
        status = palSendMBMessage(pMac->hHdd, pMsg);
    } while( 0 );
    return ( status );
}
eHalStatus csrRoamUpdateWPARSNIEs( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirRSNie * pAPSirRSNie)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirUpdateAPWPARSNIEsReq *pMsg;
    tANI_U8 *pBuf = NULL, *wTmpBuf = NULL;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (NULL == pSession)
    {
        smsLog( pMac, LOGE, FL( "  Session does not exist for session id %d" ), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    do
    {
        pMsg = vos_mem_malloc(sizeof(tSirUpdateAPWPARSNIEsReq));
        if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
        vos_mem_set(pMsg, sizeof( tSirUpdateAPWPARSNIEsReq ), 0);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_SET_APWPARSNIEs_REQ);
        pBuf = (tANI_U8 *)&pMsg->transactionId;
        wTmpBuf = pBuf;

        if (NULL == pBuf)
        {
           VOS_ASSERT(pBuf);
           return eHAL_STATUS_FAILURE;
        }
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);

        // bssId
        vos_mem_copy((tSirMacAddr *)pBuf, &pSession->selfMacAddr,
                     sizeof(tSirMacAddr));
        pBuf += sizeof(tSirMacAddr);
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;

        // APWPARSNIEs
        vos_mem_copy((tSirRSNie *)pBuf, pAPSirRSNie, sizeof(tSirRSNie));
        pBuf += sizeof(tSirRSNie);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf))); //msg_header + msg
    status = palSendMBMessage(pMac->hHdd, pMsg);
    } while( 0 );
    return ( status );
}

#ifdef WLAN_FEATURE_VOWIFI_11R
eHalStatus
csrRoamIssueFTPreauthReq(tHalHandle hHal, tANI_U32 sessionId,
                         tpSirBssDescription pBssDescription)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tpSirFTPreAuthReq pftPreAuthReq;
    tANI_U16 auth_req_len = 0;
    tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);

    if (NULL == pSession) {
        smsLog(pMac, LOGE,
               FL("Session does not exist for session id(%d)"), sessionId);
        return eHAL_STATUS_FAILURE;
    }

    auth_req_len = sizeof(tSirFTPreAuthReq);
    pftPreAuthReq = (tpSirFTPreAuthReq)vos_mem_malloc(auth_req_len);
    if (NULL == pftPreAuthReq)
    {
        smsLog(pMac, LOGE,
               FL("Memory allocation for FT Preauth request failed"));
        return eHAL_STATUS_RESOURCES;
    }
    // Save the SME Session ID here. We need it while processing the preauth response
    pSession->ftSmeContext.smeSessionId = sessionId;
    vos_mem_zero(pftPreAuthReq, auth_req_len);

    pftPreAuthReq->pbssDescription = (tpSirBssDescription)vos_mem_malloc(
            sizeof(pBssDescription->length) + pBssDescription->length);
    if (NULL == pftPreAuthReq->pbssDescription)
    {
        smsLog(pMac, LOGE,
               FL("Memory allocation for FT Preauth request failed"));
        vos_mem_free(pftPreAuthReq);
        return eHAL_STATUS_RESOURCES;
    }

    pftPreAuthReq->messageType = pal_cpu_to_be16(eWNI_SME_FT_PRE_AUTH_REQ);

    pftPreAuthReq->preAuthchannelNum = pBssDescription->channelId;

    vos_mem_copy((void *)&pftPreAuthReq->currbssId,
                 (void *)pSession->connectedProfile.bssid, sizeof(tSirMacAddr));
    vos_mem_copy((void *)&pftPreAuthReq->preAuthbssId,
                 (void *)pBssDescription->bssId, sizeof(tSirMacAddr));

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (csrRoamIs11rAssoc(pMac, sessionId) &&
          (pMac->roam.roamSession[sessionId].connectedProfile.AuthType != eCSR_AUTH_TYPE_OPEN_SYSTEM))
    {
        pftPreAuthReq->ft_ies_length =
            (tANI_U16)pSession->ftSmeContext.auth_ft_ies_length;
        vos_mem_copy(pftPreAuthReq->ft_ies, pSession->ftSmeContext.auth_ft_ies,
                     pSession->ftSmeContext.auth_ft_ies_length);
    }
    else
#endif
    {
        pftPreAuthReq->ft_ies_length = 0;
    }
    vos_mem_copy(pftPreAuthReq->pbssDescription, pBssDescription,
                 sizeof(pBssDescription->length) + pBssDescription->length);
    pftPreAuthReq->length = pal_cpu_to_be16(auth_req_len);
    return palSendMBMessage(pMac->hHdd, pftPreAuthReq);
}
/*--------------------------------------------------------------------------
 * This will receive and process the FT Pre Auth Rsp from the current
 * associated ap.
 *
 * This will invoke the hdd call back. This is so that hdd can now
 * send the FTIEs from the Auth Rsp (Auth Seq 2) to the supplicant.
  ------------------------------------------------------------------------*/
void csrRoamFTPreAuthRspProcessor( tHalHandle hHal, tpSirFTPreAuthRsp pFTPreAuthRsp )
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   eHalStatus  status = eHAL_STATUS_SUCCESS;
#if defined(FEATURE_WLAN_LFR) || defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)
   tCsrRoamInfo roamInfo;
#endif
   eCsrAuthType conn_Auth_type;
   tANI_U32     sessionId = pFTPreAuthRsp->smeSessionId;
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

   if (NULL == pSession)
   {
      smsLog(pMac, LOGE, FL("pSession is NULL"));
      return;
   }

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   status = csrNeighborRoamPreauthRspHandler(pMac, pFTPreAuthRsp->smeSessionId,
                                             pFTPreAuthRsp->status);
   if (status != eHAL_STATUS_SUCCESS) {
      /*
       * Bail out if pre-auth was not even processed.
       */
      smsLog(pMac, LOGE,FL("Preauth was not processed: %d SessionID: %d"),
            status, sessionId);
      return;
   }
#endif

   /* The below function calls/timers should be invoked only if the pre-auth is successful */
   if (VOS_STATUS_SUCCESS != (VOS_STATUS)pFTPreAuthRsp->status)
      return;
   // Implies a success
   pSession->ftSmeContext.FTState = eFT_AUTH_COMPLETE;
   // Indicate SME QoS module the completion of Preauth success. This will trigger the creation of RIC IEs
   pSession->ftSmeContext.psavedFTPreAuthRsp = pFTPreAuthRsp;
   /* No need to notify qos module if this is a non 11r & ESE roam*/
   if (csrRoamIs11rAssoc(pMac, pFTPreAuthRsp->smeSessionId)
#if defined(FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_ESE_UPLOAD)
       || csrRoamIsESEAssoc(pMac, pFTPreAuthRsp->smeSessionId)
#endif
      )
   {
      sme_QosCsrEventInd(pMac,
            pSession->ftSmeContext.smeSessionId,
            SME_QOS_CSR_PREAUTH_SUCCESS_IND, NULL);
   }
   /* Start the pre-auth reassoc interval timer with a period of 400ms. When this expires,
    * actual transition from the current to handoff AP is triggered */
   status = vos_timer_start(&pSession->ftSmeContext.preAuthReassocIntvlTimer,
         60);
   if (eHAL_STATUS_SUCCESS != status)
   {
      smsLog(pMac, LOGE, FL("Preauth reassoc interval timer start failed to start with status %d"), status);
      return;
   }
   // Save the received response
   vos_mem_copy((void *)&pSession->ftSmeContext.preAuthbssId,
         (void *)pFTPreAuthRsp->preAuthbssId, sizeof(tCsrBssid));
   if (csrRoamIs11rAssoc(pMac, pFTPreAuthRsp->smeSessionId))
      csrRoamCallCallback(pMac, pFTPreAuthRsp->smeSessionId, NULL, 0,
            eCSR_ROAM_FT_RESPONSE, eCSR_ROAM_RESULT_NONE);

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
   if (csrRoamIsESEAssoc(pMac, pFTPreAuthRsp->smeSessionId))
   {
      /* read TSF */
      csrRoamReadTSF(pMac, (tANI_U8 *)roamInfo.timestamp,
                     pFTPreAuthRsp->smeSessionId);
      // Save the bssid from the received response
      vos_mem_copy((void *)&roamInfo.bssid,
            (void *)pFTPreAuthRsp->preAuthbssId, sizeof(tCsrBssid));
      csrRoamCallCallback(pMac, pFTPreAuthRsp->smeSessionId, &roamInfo,
            0, eCSR_ROAM_CCKM_PREAUTH_NOTIFY, 0);
   }
#endif /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

#ifdef FEATURE_WLAN_LFR
   // If Legacy Fast Roaming is enabled, signal the supplicant
   // So he can send us a PMK-ID for this candidate AP.
   if (csrRoamIsFastRoamEnabled(pMac, pFTPreAuthRsp->smeSessionId))
   {
      // Save the bssid from the received response
      vos_mem_copy((void *)&roamInfo.bssid,
            (void *)pFTPreAuthRsp->preAuthbssId, sizeof(tCsrBssid));
      csrRoamCallCallback(pMac, pFTPreAuthRsp->smeSessionId, &roamInfo, 0, eCSR_ROAM_PMK_NOTIFY, 0);
   }

#endif

   // If its an Open Auth, FT IEs are not provided by supplicant
   // Hence populate them here
   conn_Auth_type =
      pMac->roam.roamSession[sessionId].connectedProfile.AuthType;

   pSession->ftSmeContext.addMDIE = FALSE;
   // Done with it, init it.
   pSession->ftSmeContext.psavedFTPreAuthRsp = NULL;

   if (csrRoamIs11rAssoc(pMac, pFTPreAuthRsp->smeSessionId) &&
      (conn_Auth_type == eCSR_AUTH_TYPE_OPEN_SYSTEM))
   {
      tANI_U16 ft_ies_length;
      ft_ies_length = pFTPreAuthRsp->ric_ies_length;

      if ( (pSession->ftSmeContext.reassoc_ft_ies) &&
            (pSession->ftSmeContext.reassoc_ft_ies_length))
      {
         vos_mem_free(pSession->ftSmeContext.reassoc_ft_ies);
         pSession->ftSmeContext.reassoc_ft_ies_length = 0;
         pSession->ftSmeContext.reassoc_ft_ies = NULL;
      }

      if (!ft_ies_length)
         return;

      pSession->ftSmeContext.reassoc_ft_ies = vos_mem_malloc(ft_ies_length);
      if ( NULL == pSession->ftSmeContext.reassoc_ft_ies )
      {
         smsLog( pMac, LOGE, FL("Memory allocation failed for ft_ies"));
         return;
      }
      else
      {
         // Copy the RIC IEs to reassoc IEs
         vos_mem_copy(((tANI_U8 *)pSession->ftSmeContext.reassoc_ft_ies),
               (tANI_U8 *)pFTPreAuthRsp->ric_ies,
               pFTPreAuthRsp->ric_ies_length);
         pSession->ftSmeContext.reassoc_ft_ies_length = ft_ies_length;
         pSession->ftSmeContext.addMDIE = TRUE;
      }
   }
}
#endif



/*
  pBuf points to the beginning of the message
  LIM packs disassoc rsp as below,
      messageType - 2 bytes
      messageLength - 2 bytes
      sessionId - 1 byte
      transactionId - 2 bytes (tANI_U16)
      reasonCode - 4 bytes (sizeof(tSirResultCodes))
      peerMacAddr - 6 bytes
*/
static void csrSerDesUnpackDiassocRsp(tANI_U8 *pBuf, tSirSmeDisassocRsp *pRsp)
{
   if(pBuf && pRsp)
   {
      pBuf += 4; //skip type and length
      pRsp->sessionId  = *pBuf++;
      pal_get_U16( pBuf, (tANI_U16 *)&pRsp->transactionId );
      pBuf += 2;
      pal_get_U32( pBuf, (tANI_U32 *)&pRsp->statusCode );
      pBuf += 4;
      vos_mem_copy(pRsp->peerMacAddr, pBuf, 6);
   }
}

eHalStatus csrGetDefaultCountryCodeFrmNv(tpAniSirGlobal pMac, tANI_U8 *pCountry)
{
   static uNvTables nvTables;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   VOS_STATUS vosStatus = vos_nv_readDefaultCountryTable( &nvTables );

   /* read the country code from NV and use it */
   if ( VOS_IS_STATUS_SUCCESS(vosStatus) )
   {
      vos_mem_copy(pCountry, nvTables.defaultCountryTable.countryCode,
                   WNI_CFG_COUNTRY_CODE_LEN);
      return status;
   }
   else
   {
      vos_mem_copy(pCountry, "XXX", WNI_CFG_COUNTRY_CODE_LEN);
      status = eHAL_STATUS_FAILURE;
      return status;
   }
}

eHalStatus csrGetCurrentCountryCode(tpAniSirGlobal pMac, tANI_U8 *pCountry)
{
   vos_mem_copy(pCountry, pMac->scan.countryCode11d, WNI_CFG_COUNTRY_CODE_LEN);
   return eHAL_STATUS_SUCCESS;
}

eHalStatus csrSetTxPower(tpAniSirGlobal pMac, v_U8_t sessionId, v_U8_t mW)
{
   tSirSetTxPowerReq *pMsg = NULL;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);

   if (!pSession)
   {
       smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
       return eHAL_STATUS_FAILURE;
   }

   pMsg = vos_mem_malloc(sizeof(tSirSetTxPowerReq));
   if ( NULL == pMsg ) return eHAL_STATUS_FAILURE;
   vos_mem_set((void *)pMsg, sizeof(tSirSetTxPowerReq), 0);
   pMsg->messageType     = eWNI_SME_SET_TX_POWER_REQ;
   pMsg->length          = sizeof(tSirSetTxPowerReq);
   pMsg->mwPower         = mW;
   vos_mem_copy((tSirMacAddr *)pMsg->bssId, &pSession->selfMacAddr,
                sizeof(tSirMacAddr));
   status = palSendMBMessage(pMac->hHdd, pMsg);
   if (!HAL_STATUS_SUCCESS(status))
   {
       smsLog(pMac, LOGE, FL(" csr set TX Power Post MSG Fail %d "), status);
       //pMsg is freed by palSendMBMessage
   }
   return status;
}

/* Returns whether a session is in VOS_STA_MODE...or not */
tANI_BOOLEAN csrRoamIsStaMode(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
  tCsrRoamSession *pSession = NULL;
  pSession = CSR_GET_SESSION ( pMac, sessionId );
  if(!pSession)
  {
    smsLog(pMac, LOGE, FL(" %s: session %d not found "), __func__, sessionId);
    return eANI_BOOLEAN_FALSE;
  }
  if ( !CSR_IS_SESSION_VALID ( pMac, sessionId ) )
  {
    smsLog(pMac, LOGE, FL(" %s: Inactive session"), __func__);
    return eANI_BOOLEAN_FALSE;
  }
  if ( eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType )
  {
     return eANI_BOOLEAN_FALSE;
  }
  /* There is a possibility that the above check may fail,because
   * P2P CLI also uses the same BSSType (eCSR_BSS_TYPE_INFRASTRUCTURE)
   * when it is connected.So,we may sneak through the above check even
   * if we are not a STA mode INFRA station. So, if we sneak through
   * the above condition, we can use the following check if we are
   * really in STA Mode.*/

  if ( NULL != pSession->pCurRoamProfile )
  {
    if ( pSession->pCurRoamProfile->csrPersona == VOS_STA_MODE )
    {
      return eANI_BOOLEAN_TRUE;
    } else {
      return eANI_BOOLEAN_FALSE;
    }
  }

  return eANI_BOOLEAN_FALSE;
}

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
eHalStatus csrHandoffRequest(tpAniSirGlobal pMac,
                             tANI_U8 sessionId,
                             tCsrHandoffRequest *pHandoffInfo)
{
   eHalStatus status = eHAL_STATUS_SUCCESS;
   vos_msg_t  msg;

   tAniHandoffReq *pMsg;
   pMsg = vos_mem_malloc(sizeof(tAniHandoffReq));
   if ( NULL == pMsg )
   {
      smsLog(pMac, LOGE, " csrHandoffRequest: failed to allocate mem for req ");
      return eHAL_STATUS_FAILURE;
   }
   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_HANDOFF_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniHandoffReq);
   pMsg->sessionId = sessionId;
   pMsg->channel = pHandoffInfo->channel;
   pMsg->handoff_src = pHandoffInfo->src;
   vos_mem_copy(pMsg->bssid,
                       pHandoffInfo->bssid,
                       6);
   msg.type = eWNI_SME_HANDOFF_REQ;
   msg.bodyptr = pMsg;
   msg.reserved = 0;
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
   {
       smsLog(pMac, LOGE, " csrHandoffRequest failed to post msg to self ");
       vos_mem_free((void *)pMsg);
       status = eHAL_STATUS_FAILURE;
   }
   return status;
}
#endif /* WLAN_FEATURE_ROAM_SCAN_OFFLOAD */

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
/* ---------------------------------------------------------------------------
    \fn csrSetCCKMIe
    \brief  This function stores the CCKM IE passed by the supplicant
    in a place holder data structure and this IE will be packed inside
    reassociation request
    \param  pMac - pMac global structure
    \param  sessionId - Current session id
    \param  pCckmIe - pointer to CCKM IE data
    \param  ccKmIeLen - length of the CCKM IE
    \- return Success or failure
    -------------------------------------------------------------------------*/
VOS_STATUS csrSetCCKMIe(tpAniSirGlobal pMac, const tANI_U8 sessionId,
                            const tANI_U8 *pCckmIe,
                            const tANI_U8 ccKmIeLen)
{
    eHalStatus       status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *pSession = CSR_GET_SESSION(pMac, sessionId);
    if (!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    vos_mem_copy(pSession->suppCckmIeInfo.cckmIe, pCckmIe,
                 ccKmIeLen);
    pSession->suppCckmIeInfo.cckmIeLen = ccKmIeLen;
    return status;
}

/* ---------------------------------------------------------------------------
    \fn csrRoamReadTSF
    \brief  This function reads the TSF; and also add the time elapsed since
    last beacon or probe response reception from the hand off AP to arrive at
    the latest TSF value.
    \param  pMac - pMac global structure
    \param  pTimestamp - output TSF time stamp
    \- return Success or failure
    -------------------------------------------------------------------------*/
VOS_STATUS csrRoamReadTSF(tpAniSirGlobal pMac, tANI_U8 *pTimestamp,
                          tANI_U8 sessionId)
{
    eHalStatus              status     = eHAL_STATUS_SUCCESS;
    tCsrNeighborRoamBSSInfo handoffNode;
    tANI_U32                timer_diff = 0;
    tANI_U32                timeStamp[2];
    tpSirBssDescription     pBssDescription = NULL;
    csrNeighborRoamGetHandoffAPInfo(pMac, &handoffNode, sessionId);
    pBssDescription = handoffNode.pBssDescription;
    // Get the time diff in milli seconds
    timer_diff = vos_timer_get_system_time() - pBssDescription->scanSysTimeMsec;
    // Convert msec to micro sec timer
    timer_diff = (tANI_U32)(timer_diff * SYSTEM_TIME_MSEC_TO_USEC);
    timeStamp[0] = pBssDescription->timeStamp[0];
    timeStamp[1] = pBssDescription->timeStamp[1];
    UpdateCCKMTSF(&(timeStamp[0]), &(timeStamp[1]), &timer_diff);
    vos_mem_copy(pTimestamp, (void *) &timeStamp[0],
                 sizeof (tANI_U32) * 2);
    return status;
}
#endif /*FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */

/*
 * Post Channel Change Request to LIM
 * This API is primarily used to post
 * Channel Change Req for SAP
 */
eHalStatus
csrRoamChannelChangeReq(tpAniSirGlobal pMac, tCsrBssid bssid,
                        tANI_U8 cbMode, tCsrRoamProfile *pprofile)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirChanChangeRequest *pMsg;
    tCsrRoamStartBssParams param;

    csrRoamGetBssStartParms(pMac, pprofile, &param);
    pMsg = vos_mem_malloc( sizeof(tSirChanChangeRequest) );
    if (!pMsg)
    {
        return ( eHAL_STATUS_FAILURE );
    }

    vos_mem_set((void *)pMsg, sizeof( tSirChanChangeRequest ), 0);

    pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_CHANNEL_CHANGE_REQ);
    pMsg->messageLen = sizeof(tSirChanChangeRequest);
    pMsg->targetChannel = pprofile->ChannelInfo.ChannelList[0];
    pMsg->cbMode = cbMode;
    pMsg->vht_channel_width = pprofile->vht_channel_width;
    pMsg->dot11mode =
       csrTranslateToWNICfgDot11Mode(pMac,pMac->roam.configParam.uCfgDot11Mode);

    vos_mem_copy(pMsg->bssid, bssid, VOS_MAC_ADDR_SIZE);
    vos_mem_copy((void*)&pMsg->operational_rateset,
                 (void*)&param.operationalRateSet, sizeof(tSirMacRateSet));
    vos_mem_copy((void*)&pMsg->extended_rateset, (void*)&param.extendedRateSet,
                 sizeof(tSirMacRateSet));

    status = palSendMBMessage(pMac->hHdd, pMsg);

    return ( status );
}

/*
 * Post Beacon Tx Start request to LIM
 * immediately after SAP CAC WAIT is
 * completed without any RADAR indications.
 */
eHalStatus csrRoamStartBeaconReq( tpAniSirGlobal pMac, tCsrBssid bssid,
                             tANI_U8 dfsCacWaitStatus)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirStartBeaconIndication *pMsg;

    pMsg = vos_mem_malloc(sizeof(tSirStartBeaconIndication));

    if (!pMsg)
    {
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_set((void *)pMsg, sizeof( tSirStartBeaconIndication ), 0);
    pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_START_BEACON_REQ);
    pMsg->messageLen = sizeof(tSirStartBeaconIndication);
    pMsg->beaconStartStatus  = dfsCacWaitStatus;
    vos_mem_copy(pMsg->bssid, bssid, VOS_MAC_ADDR_SIZE);

    status = palSendMBMessage(pMac->hHdd, pMsg);

    return ( status );
}


/*----------------------------------------------------------------------------
 \fn csrRoamModifyAddIEs
 \brief  This function sends msg to modify the additional IE buffers in PE
 \param  pMac - pMac global structure
 \param  pModifyIE - pointer to tSirModifyIE structure
 \param  updateType - Type of buffer
 \- return Success or failure
-----------------------------------------------------------------------------*/
eHalStatus
csrRoamModifyAddIEs(tpAniSirGlobal pMac,
                    tSirModifyIE *pModifyIE,
                    eUpdateIEsType updateType)
{
    tpSirModifyIEsInd pModifyAddIEInd = NULL;
    tANI_U8 *pLocalBuffer = NULL;
    eHalStatus status;

    /* following buffer will be freed by consumer (PE) */
    pLocalBuffer = vos_mem_malloc(pModifyIE->ieBufferlength);

    if (NULL == pLocalBuffer)
    {
       smsLog(pMac, LOGE, FL("Memory Allocation Failure!!!"));
       return eHAL_STATUS_FAILED_ALLOC;
    }

    pModifyAddIEInd = vos_mem_malloc(sizeof(tSirModifyIEsInd));
    if (NULL == pModifyAddIEInd)
    {
       smsLog(pMac, LOGE, FL("Memory Allocation Failure!!!"));
       vos_mem_free(pLocalBuffer);
       return eHAL_STATUS_FAILED_ALLOC;
    }

    /*copy the IE buffer */
    vos_mem_copy(pLocalBuffer, pModifyIE->pIEBuffer, pModifyIE->ieBufferlength);
    vos_mem_zero(pModifyAddIEInd, sizeof(tSirModifyIEsInd));

    pModifyAddIEInd->msgType =
        pal_cpu_to_be16((tANI_U16)eWNI_SME_MODIFY_ADDITIONAL_IES);
    pModifyAddIEInd->msgLen = sizeof(tSirModifyIEsInd);

    vos_mem_copy(pModifyAddIEInd->modifyIE.bssid, pModifyIE->bssid,
        sizeof(tSirMacAddr));

    pModifyAddIEInd->modifyIE.smeSessionId = pModifyIE->smeSessionId;
    pModifyAddIEInd->modifyIE.notify = pModifyIE->notify;
    pModifyAddIEInd->modifyIE.ieID = pModifyIE->ieID;
    pModifyAddIEInd->modifyIE.ieIDLen = pModifyIE->ieIDLen;
    pModifyAddIEInd->modifyIE.pIEBuffer = pLocalBuffer;
    pModifyAddIEInd->modifyIE.ieBufferlength = pModifyIE->ieBufferlength;
    pModifyAddIEInd->modifyIE.oui_length = pModifyIE->oui_length;

    pModifyAddIEInd->updateType = updateType;

    status = palSendMBMessage(pMac->hHdd, pModifyAddIEInd);
    if (!HAL_STATUS_SUCCESS(status))
    {
       smsLog(pMac, LOGE,
           FL("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg"
           "!!! status %d"), status);
       vos_mem_free(pLocalBuffer);
    }
    return status;
}


/*----------------------------------------------------------------------------
 \fn csrRoamUpdateAddIEs
 \brief  This function sends msg to updates the additional IE buffers in PE
 \param  pMac - pMac global structure
 \param  sessionId - SME session id
 \param  bssid - BSSID
 \param  additionIEBuffer - buffer containing addition IE from hostapd
 \param  length - length of buffer
 \param  updateType - Type of buffer
 \param  append - append or replace completely
 \- return Success or failure
-----------------------------------------------------------------------------*/
eHalStatus
csrRoamUpdateAddIEs(tpAniSirGlobal pMac,
                    tSirUpdateIE *pUpdateIE,
                    eUpdateIEsType updateType)
{
    tpSirUpdateIEsInd pUpdateAddIEs = NULL;
    tANI_U8 *pLocalBuffer = NULL;
    eHalStatus status;

    if (pUpdateIE->ieBufferlength != 0)
    {
        /* Following buffer will be freed by consumer (PE) */
        pLocalBuffer = vos_mem_malloc(pUpdateIE->ieBufferlength);
        if (NULL == pLocalBuffer)
        {
           smsLog(pMac, LOGE, FL("Memory Allocation Failure!!!"));
           return eHAL_STATUS_FAILED_ALLOC;
        }
        vos_mem_copy(pLocalBuffer, pUpdateIE->pAdditionIEBuffer,
                pUpdateIE->ieBufferlength);
    }

    pUpdateAddIEs = vos_mem_malloc( sizeof(tSirUpdateIEsInd) );
    if (NULL == pUpdateAddIEs)
    {
        smsLog(pMac, LOGE, FL("Memory Allocation Failure!!!"));
        if (pLocalBuffer != NULL)
        {
            vos_mem_free(pLocalBuffer);
        }
       return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_zero(pUpdateAddIEs, sizeof(tSirUpdateIEsInd));

    pUpdateAddIEs->msgType =
        pal_cpu_to_be16((tANI_U16)eWNI_SME_UPDATE_ADDITIONAL_IES);
    pUpdateAddIEs->msgLen = sizeof(tSirUpdateIEsInd);

    vos_mem_copy(pUpdateAddIEs->updateIE.bssid, pUpdateIE->bssid, sizeof(tSirMacAddr));

    pUpdateAddIEs->updateIE.smeSessionId = pUpdateIE->smeSessionId;
    pUpdateAddIEs->updateIE.append = pUpdateIE->append;
    pUpdateAddIEs->updateIE.notify = pUpdateIE->notify;
    pUpdateAddIEs->updateIE.ieBufferlength = pUpdateIE->ieBufferlength;
    pUpdateAddIEs->updateIE.pAdditionIEBuffer = pLocalBuffer;

    pUpdateAddIEs->updateType = updateType;

    status = palSendMBMessage(pMac->hHdd, pUpdateAddIEs);
    if (!HAL_STATUS_SUCCESS(status))
    {
       smsLog(pMac, LOGE,
           FL("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg"
           "!!! status %d"), status);
       vos_mem_free(pLocalBuffer);
    }
    return status;
}

/**
 * csr_send_ext_change_channel()- function to post send ECSA
 * action frame to lim.
 * @mac_ctx: pointer to global mac structure
 * @channel: new channel to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to lim.
 *
 * Return: success if msg posted to LIM else return failure
 */
eHalStatus csr_send_ext_change_channel(tpAniSirGlobal mac_ctx, uint32_t channel,
					uint8_t session_id)
{
	eHalStatus status = eHAL_STATUS_SUCCESS;
	struct sir_sme_ext_cng_chan_req *msg;

	msg = vos_mem_malloc(sizeof(*msg));
	if (NULL == msg)
		return eHAL_STATUS_FAILURE;

	vos_mem_zero(msg, sizeof(*msg));
	msg->message_type =
		 pal_cpu_to_be16((uint16_t)eWNI_SME_EXT_CHANGE_CHANNEL);
	msg->length =
		 pal_cpu_to_be16((uint16_t)sizeof(*msg));
	msg->new_channel = channel;
	msg->session_id = session_id;
	status = palSendMBMessage(mac_ctx->hHdd, msg);
	return status;
}


/*----------------------------------------------------------------------------
 \fn csrRoamSendChanSwIERequest
 \brief  This function sends request to transmit channel switch announcement
         IE to lower layers
 \param  pMac - pMac global structure
 \param  sessionId - SME session id
 \param  pDfsCacInd - CAC indication data to PE/LIM
 \param  ch_bandwidth - Channel offset
 \- return Success or failure
-----------------------------------------------------------------------------*/
eHalStatus
csrRoamSendChanSwIERequest(tpAniSirGlobal pMac, tCsrBssid bssid,
                       tANI_U8 targetChannel, tANI_U8 csaIeReqd,
                       u_int8_t ch_bandwidth)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirDfsCsaIeRequest *pMsg;

    pMsg = vos_mem_malloc(sizeof(tSirDfsCsaIeRequest));
    if (!pMsg)
    {
        return eHAL_STATUS_FAILURE;
    }

    vos_mem_set((void *)pMsg, sizeof(tSirDfsCsaIeRequest), 0);
    pMsg->msgType =
        pal_cpu_to_be16((tANI_U16)eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ);
    pMsg->msgLen = sizeof(tSirDfsCsaIeRequest);

    pMsg->targetChannel = targetChannel;
    pMsg->csaIeRequired = csaIeReqd;
    vos_mem_copy(pMsg->bssid, bssid, VOS_MAC_ADDR_SIZE);
    pMsg->ch_bandwidth = ch_bandwidth;

    status = palSendMBMessage(pMac->hHdd, pMsg);

    return status;
}
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*----------------------------------------------------------------------------
 * fn csrProcessRoamOffloadSynchInd
 * brief  This will receive and process the Roam Offload Synch Ind
 * param  hHal - pMac global structure
 * param  pSmeRoamOffloadSynchInd - Roam Synch info is retrieved
 * --------------------------------------------------------------------------*/
void csrProcessRoamOffloadSynchInd(
    tHalHandle hHal, tpSirRoamOffloadSynchInd pSmeRoamOffloadSynchInd)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCsrRoamSession *pSession = NULL;
    tANI_U8 sessionId = pSmeRoamOffloadSynchInd->roamedVdevId;
    pSession =  CSR_GET_SESSION(pMac, pSmeRoamOffloadSynchInd->roamedVdevId);
    if (!pSession) {
        smsLog(pMac, LOGE, FL("LFR3: session %d not found "),
                   pSmeRoamOffloadSynchInd->roamedVdevId);
        goto err_synch_rsp;
    }
    if (!HAL_STATUS_SUCCESS(csrScanSaveRoamOffloadApToScanCache(pMac,
                            pSmeRoamOffloadSynchInd))) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                "fail to save roam offload AP to scan cache");
        goto err_synch_rsp;
    }
    pSession->roamOffloadSynchParams.rssi = pSmeRoamOffloadSynchInd->rssi;
    pSession->roamOffloadSynchParams.roamReason =
                             pSmeRoamOffloadSynchInd->roamReason;
    pSession->roamOffloadSynchParams.roamedVdevId =
                             pSmeRoamOffloadSynchInd->roamedVdevId;
    vos_mem_copy(pSession->roamOffloadSynchParams.bssid,
                 pSmeRoamOffloadSynchInd->bssId, sizeof(tSirMacAddr));
    pSession->roamOffloadSynchParams.txMgmtPower =
                                 pSmeRoamOffloadSynchInd->txMgmtPower;
    pSession->roamOffloadSynchParams.authStatus =
                                 pSmeRoamOffloadSynchInd->authStatus;
    pSession->roamOffloadSynchParams.bRoamSynchInProgress = eANI_BOOLEAN_TRUE;
    /*Save the BSS descriptor for later use*/
    pSession->roamOffloadSynchParams.pbssDescription =
                                  pSmeRoamOffloadSynchInd->pbssDescription;
    pMac->roam.reassocRespLen = pSmeRoamOffloadSynchInd->reassocRespLength;
    pMac->roam.pReassocResp =
    vos_mem_malloc(pMac->roam.reassocRespLen);
    if (NULL == pMac->roam.pReassocResp) {
      VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
           "Memory allocation for reassoc response failed");
        goto err_synch_rsp;
    }
    vos_mem_copy(pMac->roam.pReassocResp,
                (tANI_U8 *)pSmeRoamOffloadSynchInd +
                pSmeRoamOffloadSynchInd->reassocRespOffset,
                pMac->roam.reassocRespLen);

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
              "LFR3:%s: the reassoc resp frame data:", __func__);
    VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
              pMac->roam.pReassocResp, pMac->roam.reassocRespLen);

    vos_mem_copy(pSession->roamOffloadSynchParams.kck,
        pSmeRoamOffloadSynchInd->kck, SIR_KCK_KEY_LEN);
    vos_mem_copy(pSession->roamOffloadSynchParams.kek,
        pSmeRoamOffloadSynchInd->kek, SIR_KEK_KEY_LEN);
    vos_mem_copy(pSession->roamOffloadSynchParams.replay_ctr,
        pSmeRoamOffloadSynchInd->replay_ctr, SIR_REPLAY_CTR_LEN);

    if (eHAL_STATUS_SUCCESS != csrNeighborRoamOffloadUpdatePreauthList(pMac,
                                        pSmeRoamOffloadSynchInd, sessionId)) {
        /*
         * Bail out if Roam Offload Synch Response was not even handled.
         */
        smsLog(pMac, LOGE, FL("Roam Offload Synch Response "
                              "was not processed"));
        goto err_synch_rsp;
    }

    csrNeighborRoamRequestHandoff(pMac, sessionId);

err_synch_rsp:
    vos_mem_free(pSmeRoamOffloadSynchInd->pbssDescription);
    pSmeRoamOffloadSynchInd->pbssDescription = NULL;
}

/*----------------------------------------------------------------------------
 * fn csrProcessHOFailInd
 * brief  This function will process the Hand Off Failure indication
 *        received from the firmware. It will trigger a disconnect on
 *        the session which the firmware reported a hand off failure
 * param  pMac global structure
 * param  pMsgBuf - Contains the session ID for which the handler should apply
 * --------------------------------------------------------------------------*/
void csrProcessHOFailInd(tpAniSirGlobal pMac, void *pMsgBuf)
{
   tSirSmeHOFailureInd *pSmeHOFailInd = (tSirSmeHOFailureInd *)pMsgBuf;
   tANI_U32 sessionId;

   if (pSmeHOFailInd)
       sessionId = pSmeHOFailInd->sessionId;
   else {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "LFR3: Hand-Off Failure Ind is NULL");
       return;
   }
   /* Roaming is supported only on Infra STA Mode. */
   if (!csrRoamIsStaMode(pMac, sessionId)) {
       VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "LFR3:HO Fail cannot be handled for session %d",sessionId);
       return;
   }

   csrRoamSynchCleanUp(pMac, sessionId);
   VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
            "LFR3:Issue Disconnect on session %d", sessionId);
   csrRoamDisconnect(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
}
#endif

void csrInitOperatingClasses(tHalHandle hHal)
{
    tANI_U8 Index = 0;
    tANI_U8 class = 0;
    tANI_U8 i = 0;
    tANI_U8 j = 0;
    tANI_U8 swap = 0;
    tANI_U8 numChannels = 0;
    tANI_U8 numClasses = 0;
    tANI_BOOLEAN found;
    tANI_U8 opClasses[SIR_MAC_MAX_SUPP_OPER_CLASSES];
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    u_int8_t ch_bandwidth;

    smsLog(pMac, LOG1, FL("Current Country = %c%c"),
                          pMac->scan.countryCodeCurrent[0],
                          pMac->scan.countryCodeCurrent[1]);

    for (j = 0; j < SIR_MAC_MAX_SUPP_OPER_CLASSES; j++) {
        opClasses[j] = 0;
    }

    numChannels = pMac->scan.baseChannels.numChannels;

    smsLog(pMac, LOG1, FL("Num of base channels %d"), numChannels);

    for (Index = 0;
         Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
         Index++) {
        for (ch_bandwidth = BW20; ch_bandwidth < BWALL; ch_bandwidth++) {
             class = regdm_get_opclass_from_channel(pMac->scan.countryCodeCurrent,
                                 pMac->scan.baseChannels.channelList[Index],
                                 ch_bandwidth);
             smsLog(pMac, LOG4, FL("for chan %d, op class: %d"),
                    pMac->scan.baseChannels.channelList[Index],
                    class);

             found = FALSE;
             for (j = 0 ; j < SIR_MAC_MAX_SUPP_OPER_CLASSES - 1; j++) {
                if (opClasses[j] == class) {
                   found = TRUE;
                   break;
                }
             }

             if (!found) {
                 opClasses[i]= class;
                 i++;
             }
        }
    }

    numChannels = pMac->scan.base20MHzChannels.numChannels;

    smsLog(pMac, LOG1, FL("Num of 20MHz channels %d"), numChannels);

    for (Index = 0;
         Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
         Index++) {
        class = regdm_get_opclass_from_channel(pMac->scan.countryCodeCurrent,
                            pMac->scan.base20MHzChannels.channelList[Index],
                            BWALL);
        smsLog(pMac, LOG4, FL("for chan %d, op class: %d"),
               pMac->scan.base20MHzChannels.channelList[ Index ],
               class);

        found = FALSE;
        for (j = 0 ; j < SIR_MAC_MAX_SUPP_OPER_CLASSES - 1; j++) {
           if (opClasses[j] == class) {
              found = TRUE;
              break;
           }
        }
        if (!found) {
            opClasses[i]= class;
            i++;
        }
    }

    numChannels = pMac->scan.base40MHzChannels.numChannels;

    smsLog(pMac, LOG1, FL("Num of 40MHz channels %d"), numChannels);

    for (Index = 0;
         Index < numChannels && i < (SIR_MAC_MAX_SUPP_OPER_CLASSES - 1);
         Index++) {
        class = regdm_get_opclass_from_channel(pMac->scan.countryCodeCurrent,
                            pMac->scan.base40MHzChannels.channelList[Index],
                            BWALL);
        smsLog(pMac, LOG4, FL("for chan %d, op class: %d"),
               pMac->scan.base40MHzChannels.channelList[ Index ],
               class);

        found = FALSE;
        for (j = 0 ; j < SIR_MAC_MAX_SUPP_OPER_CLASSES - 1; j++) {
           if (opClasses[j] == class) {
              found = TRUE;
              break;
           }
        }
        if (!found) {
            opClasses[i]= class;
            i++;
        }
    }

    numClasses = i;

    /* As per spec the operating classes should be in ascending order.
     * Bubble sort is fine since we don't have many classes
     */
    for (i = 0 ; i < (numClasses - 1); i++) {
        for (j = 0 ; j < (numClasses - i - 1); j++) {
            /* For decreasing order use < */
            if (opClasses[j] > opClasses[j+1]) {
                swap = opClasses[j];
                opClasses[j] = opClasses[j+1];
                opClasses[j+1] = swap;
            }
        }
    }

    smsLog(pMac, LOG1, FL("Total number of unique supported op classes %d"),
           numClasses);
    for (i = 0; i < numClasses; i++) {
        smsLog(pMac, LOG1, FL("supported opClasses[%d] = %d"), i,
               opClasses[i]);
    }

    /* Set the ordered list of op classes in regdomain
     * for use by other modules
     */
    regdm_set_curr_opclasses(numClasses, &opClasses[0]);
}

/**
 * csr_find_sap_session() - This function will find the AP sessions from all
 *                          sessions.
 * @mac_ctx: pointer to mac context.
 *
 * This function is written to find the sap session id.
 *
 * Return: sap session id.
 */
static uint32_t csr_find_sap_session(tpAniSirGlobal mac_ctx)
{
    uint32_t i, session_id = CSR_SESSION_ID_INVALID;
    tCsrRoamSession *session_ptr;
    for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
         if (CSR_IS_SESSION_VALID( mac_ctx, i)){
             session_ptr = CSR_GET_SESSION(mac_ctx, i);
             if (VOS_STA_SAP_MODE == session_ptr->bssParams.bssPersona) {
                 /* Found it */
                 session_id = i;
                 break;
             }
         }
    }
    return session_id;
}

/**
 * csr_find_p2pgo_session() - This function will find the p2pgo session from all
 *                            sessions.
 * @mac_ctx: pointer to mac context.
 *
 * This function is written to find the p2pgo session id.
 *
 * Return: p2pgo session id.
 */
static uint32_t csr_find_p2pgo_session(tpAniSirGlobal mac_ctx)
{
    uint32_t i, session_id = CSR_SESSION_ID_INVALID;
    tCsrRoamSession *session_ptr;
    for (i = 0; i < CSR_ROAM_SESSION_MAX; i++) {
         if (CSR_IS_SESSION_VALID( mac_ctx, i)){
             session_ptr = CSR_GET_SESSION(mac_ctx, i);
             if (VOS_P2P_GO_MODE == session_ptr->bssParams.bssPersona) {
                 /* Found it */
                 session_id = i;
                 break;
             }
         }
    }
    return session_id;
}

/**
 * csr_is_conn_allow_2g_band() - This function will check if station's conn
 *                               is allowed in 2.4Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with SAP's operating channel. If SAP's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 */
static bool csr_is_conn_allow_2g_band(tpAniSirGlobal mac_ctx, uint32_t chnl)
{
    uint32_t sap_session_id;
    tCsrRoamSession *sap_session;

    if (0 == chnl) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  FL("channel is zero, connection not allowed"));

        return false;
    }

    sap_session_id = csr_find_sap_session(mac_ctx);
    if (CSR_SESSION_ID_INVALID != sap_session_id) {
        sap_session = CSR_GET_SESSION(mac_ctx, sap_session_id);
        if ((0 != sap_session->bssParams.operationChn) &&
            (sap_session->bssParams.operationChn != chnl)) {

             VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  FL("Can't allow STA to connect, channels not same"));
             return false;
        }
    }
    return true;
}

/**
 * csr_is_conn_allow_5g_band() - This function will check if station's conn
 *                               is allowed in 5Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with P2PGO's operating channel. If P2PGO's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 */
static bool csr_is_conn_allow_5g_band(tpAniSirGlobal mac_ctx, uint32_t chnl)
{
    uint32_t p2pgo_session_id;
    tCsrRoamSession *p2pgo_session;

    if (0 == chnl) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  FL("channel is zero, connection not allowed"));
        return false;
    }

    p2pgo_session_id = csr_find_p2pgo_session(mac_ctx);
    if (CSR_SESSION_ID_INVALID != p2pgo_session_id) {
         p2pgo_session = CSR_GET_SESSION(mac_ctx, p2pgo_session_id);
         if ((0 != p2pgo_session->bssParams.operationChn) &&
             (eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED !=
                  p2pgo_session->connectState) &&
             (p2pgo_session->bssParams.operationChn != chnl)) {

              VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  FL("Can't allow STA to connect, channels not same"));
              return false;
         }
    }
    return true;
}

/**
 * csr_clear_joinreq_param() - This function will clear station's params
 *                             for stored join request to csr.
 * @hal_handle: pointer to hal context.
 * @session_id: station's session id.
 *
 * This function will clear station's allocated memory for cached join
 * request.
 *
 * Return: true or false based on function's overall success.
 */
bool csr_clear_joinreq_param(tpAniSirGlobal mac_ctx,
                             uint32_t session_id)
{
    tCsrRoamSession *sta_session;
    tScanResultList *bss_list;

    if (NULL == mac_ctx) {
        return false;
    }

    sta_session = CSR_GET_SESSION(mac_ctx, session_id);
    if (NULL == sta_session) {
        return false;
    }

    /* Release the memory allocated by previous join request */
    bss_list =
           (tScanResultList *)&sta_session->stored_roam_profile.bsslist_handle;
    if (NULL != bss_list) {
        csrScanResultPurge(mac_ctx,
                           sta_session->stored_roam_profile.bsslist_handle);
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  FL("bss list is released for session %d"), session_id);
        sta_session->stored_roam_profile.bsslist_handle = NULL;
    }
    sta_session->stored_roam_profile.bsslist_handle = NULL;
    csrReleaseProfile(mac_ctx, &sta_session->stored_roam_profile.profile);
    sta_session->stored_roam_profile.reason = 0;
    sta_session->stored_roam_profile.roam_id = 0;
    sta_session->stored_roam_profile.imediate_flag = false;
    sta_session->stored_roam_profile.clear_flag = false;
    return true;
}

/**
 * csr_store_joinreq_param() - This function will store station's join
 *                             request to that station's session.
 * @mac_ctx: pointer to mac context.
 * @profile: pointer to station's roam profile.
 * @scan_cache: pointer to station's scan cache.
 * @roam_id: reference to roam_id variable being passed.
 * @session_id: station's session id.
 *
 * This function will store station's join request to one of the
 * csr structure and add it to station's session.
 *
 * Return: true or false based on function's overall success.
 */
bool csr_store_joinreq_param(tpAniSirGlobal mac_ctx,
                             tCsrRoamProfile *profile,
                             tScanResultHandle scan_cache,
                             uint32_t *roam_id,
                             uint32_t session_id)
{
    tCsrRoamSession *sta_session;

    if (NULL == mac_ctx) {
        return false;
    }

    sta_session = CSR_GET_SESSION(mac_ctx, session_id);
    if (NULL == sta_session) {
        return false;
    }

    sta_session->stored_roam_profile.session_id = session_id;
    csrRoamCopyProfile(mac_ctx, &sta_session->stored_roam_profile.profile,
                       profile);
    /* new bsslist_handle's memory will be relased later */
    sta_session->stored_roam_profile.bsslist_handle = scan_cache;
    sta_session->stored_roam_profile.reason = eCsrHddIssued;
    sta_session->stored_roam_profile.roam_id = *roam_id;
    sta_session->stored_roam_profile.imediate_flag = false;
    sta_session->stored_roam_profile.clear_flag = false;

    return true;
}

/**
 * csr_issue_stored_joinreq() - This function will issues station's stored
 *                              the join request.
 * @mac_ctx: pointer to mac context.
 * @roam_id: reference to roam_id variable being passed.
 * @session_id: station's session id.
 *
 * This function will issue station's stored join request, from this point
 * onwards the flow will be just like normal connect request.
 *
 * Return: eHAL_STATUS_SUCCESS or eHAL_STATUS_FAILURE.
 */
eHalStatus csr_issue_stored_joinreq(tpAniSirGlobal mac_ctx,
                                    uint32_t *roam_id,
                                    uint32_t session_id)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamSession *sta_session;
    uint32_t new_roam_id;

    sta_session = CSR_GET_SESSION(mac_ctx, session_id);
    if (NULL == sta_session) {
        return eHAL_STATUS_FAILURE;
    }
    new_roam_id = GET_NEXT_ROAM_ID(&mac_ctx->roam);
    *roam_id = new_roam_id;
    status = csrRoamIssueConnect(mac_ctx,
                            sta_session->stored_roam_profile.session_id,
                            &sta_session->stored_roam_profile.profile,
                            sta_session->stored_roam_profile.bsslist_handle,
                            sta_session->stored_roam_profile.reason,
                            new_roam_id,
                            sta_session->stored_roam_profile.imediate_flag,
                            sta_session->stored_roam_profile.clear_flag);
    if (!HAL_STATUS_SUCCESS(status)) {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
                  FL("CSR failed to issue connect cmd with status = 0x%08X"),
                  status);
        csr_clear_joinreq_param(mac_ctx, session_id);
    }
    return status;
}
