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

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  
  
  
    \file csrApiRoam.c
  
    Implementation for the Common Roaming interfaces.
  
    Copyright (C) 2008 Qualcomm, Incorporated
  
 
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
#ifdef FEATURE_WLAN_INTEGRATED_SOC
#include "halMsgApi.h" //for HAL_STA_INVALID_IDX.
#endif
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halPhyApi.h"
#include "halInternal.h"
#endif
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
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#include "csrNeighborRoam.h"
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */
#ifdef FEATURE_WLAN_CCX
#include "csrCcx.h"
#endif /* FEATURE_WLAN_CCX */
#define CSR_NUM_IBSS_START_CHANNELS_50      4
#define CSR_NUM_IBSS_START_CHANNELS_24      3
#define CSR_DEF_IBSS_START_CHANNEL_50       36
#define CSR_DEF_IBSS_START_CHANNEL_24       1
#define CSR_IBSS_JOIN_TIMEOUT_PERIOD        ( 1 *  PAL_TIMER_TO_SEC_UNIT )  // 1 second
#define CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD         ( 50 * PAL_TIMER_TO_SEC_UNIT )  // 50 seconds, for WPA, WPA2, CCKM
#define CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD         ( 120 * PAL_TIMER_TO_SEC_UNIT )  // 120 seconds, for WPS
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
#define CSR_DIAG_LOG_STAT_PERIOD           3000 //ms
//We use constatnt 4 here
//This macro returns true when higher AC parameter is bigger than lower AC for a difference
//The bigger the number, the less chance of TX
//It must put lower AC as the first parameter.
#define SME_DETECT_AC_WEIGHT_DIFF(loAC, hiAC)   (v_BOOL_t)(((hiAC) > (loAC)) ? (((hiAC)-(loAC)) > 4) : 0)
//Flag to send/do not send disassoc frame over the air
#define CSR_DONT_SEND_DISASSOC_OVER_THE_AIR 1
#define RSSI_HACK_BMPS (-40)
#define MAX_CB_VALUE_IN_INI (2)

/*-------------------------------------------------------------------------- 
  Static Type declarations
  ------------------------------------------------------------------------*/
static tChannelListWithPower csrRoamPowerTableFromEeprom[WNI_CFG_VALID_CHANNEL_LIST_LEN];
static tChannelListWithPower csrRoamPowerTableFromEeprom40MHz[WNI_CFG_VALID_CHANNEL_LIST_LEN];
static tCsrRoamSession       csrRoamRoamSession[CSR_ROAM_SESSION_MAX];
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
        n = AUTH_WPA2_EAP;
        break;
    case eCSR_AUTH_TYPE_RSN_PSK:
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
eHalStatus csrRoamStartIbssJoinTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval);
eHalStatus csrRoamStopIbssJoinTimer(tpAniSirGlobal pMac, tANI_U32 sessionId);
static void csrRoamIbssJoinTimerHandler(void *pv);
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
eHalStatus csrSendMBStatsReqMsg( tpAniSirGlobal pMac, tANI_U32 statsMask, tANI_U8 staId);
/*
    pStaEntry is no longer invalid upon the return of this function.
*/
static void csrRoamRemoveStatListEntry(tpAniSirGlobal pMac, tListElem *pEntry);
#ifdef WLAN_SOFTAP_FEATURE
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile,tANI_U8 operationChn, eCsrBand *pBand );
#else
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, eCsrPhyMode phyModeIn, tANI_U8 operationChn, eCsrBand *pBand );
#endif
static eHalStatus csrRoamGetQosInfoFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBssDesc);
tCsrStatsClientReqInfo * csrRoamInsertEntryIntoList( tpAniSirGlobal pMac,
                                                     tDblLinkList *pStaList,
                                                     tCsrStatsClientReqInfo *pStaEntry);
void csrRoamStatsClientTimerHandler(void *pv);
tCsrPeStatsReqInfo *  csrRoamCheckPeStatsReqList(tpAniSirGlobal pMac, tANI_U32  statsMask, 
                                                 tANI_U32 periodicity, tANI_BOOLEAN *pFound, tANI_U8 staId);
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
static eHalStatus csrRoamStartWds( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, tSirBssDescription *pBssDesc );
static void csrInitSession( tpAniSirGlobal pMac, tANI_U32 sessionId );
static eHalStatus csrRoamIssueSetKeyCommand( tpAniSirGlobal pMac, tANI_U32 sessionId, 
                                             tCsrRoamSetKey *pSetKey, tANI_U32 roamId );
//static eHalStatus csrRoamProcessStopBss( tpAniSirGlobal pMac, tSmeCmd *pCommand );
static eHalStatus csrRoamGetQosInfoFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBssDesc);
void csrRoamReissueRoamCommand(tpAniSirGlobal pMac);
#ifdef FEATURE_WLAN_BTAMP_UT_RF
void csrRoamJoinRetryTimerHandler(void *pv);
#endif
extern void SysProcessMmhMsg(tpAniSirGlobal pMac, tSirMsgQ* pMsg);
extern void btampEstablishLogLinkHdlr(void* pMsg);
static void csrSerDesUnpackDiassocRsp(tANI_U8 *pBuf, tSirSmeDisassocRsp *pRsp);

//Initialize global variables
static void csrRoamInitGlobals(tpAniSirGlobal pMac)
{
    if(pMac)
    {
        pMac->roam.powerTableFromEeprom      = csrRoamPowerTableFromEeprom;
        pMac->roam.powerTableFromEeprom40MHz = csrRoamPowerTableFromEeprom40MHz;
        pMac->roam.roamSession               = csrRoamRoamSession;
    }
    return;
}

static void csrRoamDeInitGlobals(tpAniSirGlobal pMac)
{
    if(pMac)
    {
        pMac->roam.powerTableFromEeprom      = NULL;
        pMac->roam.powerTableFromEeprom40MHz = NULL;
        pMac->roam.roamSession               = NULL;
    }
    return;
}
eHalStatus csrOpen(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    static uNvTables nvTables;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    v_REGDOMAIN_t regId;
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
        vosStatus = vos_nv_readDefaultCountryTable( &nvTables );
        if ( VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
            palCopyMemory( pMac->hHdd, pMac->scan.countryCodeDefault, 
                    nvTables.defaultCountryTable.countryCode, WNI_CFG_COUNTRY_CODE_LEN );
            status = eHAL_STATUS_SUCCESS;
        }
        else
        {
            smsLog( pMac, LOGE, FL("  fail to get NV_FIELD_IMAGE\n") );
            //hardcoded for now
            pMac->scan.countryCodeDefault[0] = 'U';
            pMac->scan.countryCodeDefault[1] = 'S';
            pMac->scan.countryCodeDefault[2] = 'I';
            //status = eHAL_STATUS_SUCCESS;
        }
        smsLog( pMac, LOGE, FL(" country Code from nvRam %s\n"), pMac->scan.countryCodeDefault );
        csrGetRegulatoryDomainForCountry(pMac, pMac->scan.countryCodeDefault, &regId);
        WDA_SetRegDomain(pMac, regId);
        pMac->scan.domainIdDefault = regId;
        pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;
        status = palCopyMemory(pMac->hHdd, pMac->scan.countryCodeCurrent, 
                         pMac->scan.countryCodeDefault, WNI_CFG_COUNTRY_CODE_LEN);
        status = csrInitGetChannels( pMac );
    }while(0);
    
    return (status);
}

#ifdef WLAN_SOFTAP_FEATURE
eHalStatus csrSetRegInfo(tHalHandle hHal,  tANI_U8 *apCntryCode)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    v_REGDOMAIN_t regId;
    v_U8_t        cntryCodeLength;
    if(NULL == apCntryCode)
    {
       smsLog( pMac, LOGW, FL(" Invalid country Code Pointer\n") );
       return eHAL_STATUS_FAILURE;
    }
    smsLog( pMac, LOGW, FL(" country Code %s\n"), apCntryCode );
    /* To get correct Regulatory domain from NV table 
     * 2 character Country code should be used
     * 3rd charater is optional for indoor/outdoor setting */
    cntryCodeLength = strlen(apCntryCode);
    status = csrGetRegulatoryDomainForCountry(pMac, apCntryCode, &regId);
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog( pMac, LOGE, FL("  fail to get regId for country Code %s\n"), apCntryCode );
        return status;
    }
    status = WDA_SetRegDomain(hHal, regId);
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog( pMac, LOGE, FL("  fail to get regId for country Code %s\n"), apCntryCode );
        return status;
    }
    pMac->scan.domainIdDefault = regId;
    pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;
    /* Clear CC field */
    palFillMemory( pMac->hHdd,
                   pMac->scan.countryCodeDefault,
                   WNI_CFG_COUNTRY_CODE_LEN,
                   0 );
    /* Copy 2 or 3 bytes country code */
    palCopyMemory( pMac->hHdd, pMac->scan.countryCodeDefault, 
                apCntryCode, cntryCodeLength );
    /* If 2 bytes country code, 3rd byte must be filled with space */
    if((WNI_CFG_COUNTRY_CODE_LEN - 1) == cntryCodeLength)
    {
       palFillMemory( pMac->hHdd,
                      pMac->scan.countryCodeDefault + 2,
                      1,
                      0x20 );
    }
    status = palCopyMemory(pMac->hHdd, pMac->scan.countryCodeCurrent, 
                               pMac->scan.countryCodeDefault, WNI_CFG_COUNTRY_CODE_LEN);
    status = csrInitGetChannels( pMac );
    return status;
}
eHalStatus csrSetChannels(tHalHandle hHal,  tCsrConfigParam *pParam  )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U8   index = 0;
    palCopyMemory( pMac->hHdd, pParam->Csr11dinfo.countryCode, 
                   pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN );
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
#endif
eHalStatus csrClose(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    
    csrStop(pMac);
    csrRoamClose(pMac);
    csrScanClose(pMac);
    csrLLClose(&pMac->roam.statsClientReqList);
    csrLLClose(&pMac->roam.peStatsReqList);
    csrLLClose(&pMac->roam.roamCmdPendingList);
    /* DeInit Globals */
    csrRoamDeInitGlobals(pMac);
    return (status);
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
        status = pmcRegisterPowerSaveCheck(pMac, csrCheckPSReady, pMac);
        if(!HAL_STATUS_SUCCESS(status)) break;
        pMac->roam.sPendingCommands = 0;
        csrScanEnable(pMac);
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
        status = csrNeighborRoamInit(pMac);
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */
        pMac->roam.tlStatsReqInfo.numClient = 0;
        pMac->roam.tlStatsReqInfo.periodicity = 0;
        pMac->roam.tlStatsReqInfo.timerRunning = FALSE;
        //init the link quality indication also
        pMac->roam.vccLinkQuality = eCSR_ROAM_LINK_QUAL_MIN_IND;
        if(!HAL_STATUS_SUCCESS(status)) 
        {
           smsLog(pMac, LOGW, " csrStart: Couldn't Init HO control blk \n");
           break;
        }
    }while(0);
#if defined(ANI_LOGDUMP)
    csrDumpInit(pMac);
#endif //#if defined(ANI_LOGDUMP)
    return (status);
}

eHalStatus csrStop(tpAniSirGlobal pMac)
{
    tANI_U32 sessionId;
    tANI_U32 i;

    for(sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++)
    {
        csrRoamCloseSession(pMac, sessionId, TRUE, NULL, NULL);
    }
    csrScanDisable(pMac);
    pMac->scan.fCancelIdleScan = eANI_BOOLEAN_FALSE;
    pMac->scan.fRestartIdleScan = eANI_BOOLEAN_FALSE;
    csrLLPurge( &pMac->roam.roamCmdPendingList, eANI_BOOLEAN_TRUE );
    
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
    csrNeighborRoamClose(pMac);
#endif
    csrScanFlushResult(pMac); //Do we want to do this?
    // deregister from PMC since we register during csrStart()
    // (ignore status since there is nothing we can do if it fails)
    (void) pmcDeregisterPowerSaveCheck(pMac, csrCheckPSReady);
    //Reset the domain back to the deault
    pMac->scan.domainIdCurrent = pMac->scan.domainIdDefault;
    csrResetCountryInformation(pMac, eANI_BOOLEAN_TRUE);

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
       csrRoamStateChange( pMac, eCSR_ROAMING_STATE_STOP ,i );
       pMac->roam.curSubState[i] = eCSR_ROAM_SUBSTATE_NONE;
    }

    return (eHAL_STATUS_SUCCESS);
}

eHalStatus csrReady(tpAniSirGlobal pMac)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    csrScanGetSupportedChannels( pMac );
    //WNI_CFG_VALID_CHANNEL_LIST should be set by this time
    //use it to init the background scan list
    csrInitBGScanChannelList(pMac);
    /* HDD issues the init scan */
    csrScanStartResultAgingTimer(pMac);
    //Store the AC weights in TL for later use
    WLANTL_GetACWeights(pMac->roam.gVosContext, pMac->roam.ucACWeights);
    status = csrInitChannelList( pMac );
    if ( ! HAL_STATUS_SUCCESS( status ) )
    {
       smsLog( pMac, LOGE, "csrInitChannelList failed during csrReady with status=%d\n",
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
        status = palTimerAlloc(pMac->hHdd, &pMac->roam.hTimerWaitForKey, csrRoamWaitForKeyTimeOutHandler, 
                                &pMac->roam.WaitForKeyTimerInfo);
      if(!HAL_STATUS_SUCCESS(status))
      {
        smsLog(pMac, LOGE, FL("cannot allocate memory for WaitForKey time out timer\n"));
        break;
      }
      status = palTimerAlloc(pMac->hHdd, &pMac->roam.tlStatsReqInfo.hTlStatsTimer, csrRoamTlStatsTimerHandler, pMac);
      if(!HAL_STATUS_SUCCESS(status))
      {
         smsLog(pMac, LOGE, FL("cannot allocate memory for summary Statistics timer\n"));
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
    palTimerStop(pMac->hHdd, pMac->roam.hTimerWaitForKey);
    palTimerFree(pMac->hHdd, pMac->roam.hTimerWaitForKey);
    palTimerStop(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer);
    palTimerFree(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer);
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
    if( pState )
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
                    status = palAllocateMemory(pMac->hHdd, (void **)&pProfile->pBssDesc, size);
                    if(HAL_STATUS_SUCCESS(status))
                    {
                        palCopyMemory(pMac->hHdd, pProfile->pBssDesc, pSession->pConnectBssDesc, size);
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
                palCopyMemory(pMac->hHdd, &pProfile->bssid, &pSession->connectedProfile.bssid, sizeof(tCsrBssid));
                palCopyMemory(pMac->hHdd, &pProfile->SSID, &pSession->connectedProfile.SSID, sizeof(tSirMacSSid));
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
#ifdef FEATURE_WLAN_CCX
                pProfile->isCCXAssoc = pSession->connectedProfile.isCCXAssoc;
                if (csrIsAuthTypeCCX(pSession->connectedProfile.AuthType))
                {
                    palCopyMemory( pMac->hHdd, pProfile->ccxCckmInfo.krk, 
                        pSession->connectedProfile.ccxCckmInfo.krk, CSR_KRK_KEY_LEN );
                    pProfile->ccxCckmInfo.reassoc_req_num=
                        pSession->connectedProfile.ccxCckmInfo.reassoc_req_num;
                    pProfile->ccxCckmInfo.krk_plumbed = 
                        pSession->connectedProfile.ccxCckmInfo.krk_plumbed;
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
    
    if(csrIsConnStateConnected(pMac, sessionId))
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
    
    if(pProfile->pBssDesc)
    {
        palFreeMemory(pMac->hHdd, pProfile->pBssDesc);
    }
    palZeroMemory(pMac->hHdd, pProfile, sizeof(tCsrRoamConnectedProfile));
    pProfile->AuthType = eCSR_AUTH_TYPE_UNKNOWN;
    return (status);
}

static eHalStatus csrRoamFreeConnectedInfo( tpAniSirGlobal pMac, tCsrRoamConnectedInfo *pConnectedInfo )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    if( pConnectedInfo->pbFrames )
    {
        palFreeMemory( pMac->hHdd, pConnectedInfo->pbFrames );
        pConnectedInfo->pbFrames = NULL;
    }
    pConnectedInfo->nBeaconLength = 0;
    pConnectedInfo->nAssocReqLength = 0;
    pConnectedInfo->nAssocRspLength = 0;
    pConnectedInfo->staId = 0;
#ifdef WLAN_FEATURE_VOWIFI_11R
    pConnectedInfo->nRICRspLength = 0;
#endif    
#ifdef FEATURE_WLAN_CCX
    pConnectedInfo->nTspecIeLength = 0;
#endif    
    return ( status );
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
    palZeroMemory(pMac->hHdd, &pCommand->u.setKeyCmd, sizeof(tSetKeyCmd));
}

void csrReinitRemoveKeyCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    palZeroMemory(pMac->hHdd, &pCommand->u.removeKeyCmd, sizeof(tRemoveKeyCmd));
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
            // We need to inform the requester before droping the scan command
            smsLog( pMac, LOGW, "%s: Drop scan reason %d callback 0x%X\n", 
                __FUNCTION__, pCommand->u.scanCmd.reason, (unsigned int)pCommand->u.scanCmd.callback);
            if (NULL != pCommand->u.scanCmd.callback)
            {
                smsLog( pMac, LOGW, "%s callback scan requester\n", __FUNCTION__);
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
            smsLog( pMac, LOGW, " CSR abort standard command %d\n", pCommand->command );
            csrReleaseCommand( pMac, pCommand );
            break;
        }
    }
}

void csrRoamSubstateChange( tpAniSirGlobal pMac, eCsrRoamSubState NewSubstate, tANI_U32 sessionId)
{
    smsLog( pMac, LOG1, "   CSR RoamSubstate: [ %d <== %d ]\n", NewSubstate, pMac->roam.curSubState[sessionId]);

    if(pMac->roam.curSubState[sessionId] == NewSubstate)
    {
       return;
    }
    pMac->roam.curSubState[sessionId] = NewSubstate;
}

eCsrRoamState csrRoamStateChange( tpAniSirGlobal pMac, eCsrRoamState NewRoamState, tANI_U8 sessionId)
{
    eCsrRoamState PreviousState;
          
    smsLog( pMac, LOG1, "CSR RoamState: [ %d <== %d ]\n", NewRoamState, pMac->roam.curState[sessionId]);

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

void csrAssignRssiForCategory(tpAniSirGlobal pMac, tANI_U8 catOffset)
{
    int i;
    if(catOffset)
    {
        pMac->roam.configParam.bCatRssiOffset = catOffset;
        for(i = 0; i < CSR_NUM_RSSI_CAT; i++)
        {
            pMac->roam.configParam.RSSICat[CSR_NUM_RSSI_CAT - i - 1] = (int)CSR_BEST_RSSI_VALUE - (int)(i * catOffset);
        }
    }
}

static void initConfigParam(tpAniSirGlobal pMac)
{
    int i;
    pMac->roam.configParam.agingCount = CSR_AGING_COUNT;
    pMac->roam.configParam.channelBondingMode24GHz = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
    pMac->roam.configParam.channelBondingMode5GHz = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

    pMac->roam.configParam.phyMode = eCSR_DOT11_MODE_TAURUS;
    pMac->roam.configParam.eBand = eCSR_BAND_ALL;
    pMac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_TAURUS;
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
    csrAssignRssiForCategory(pMac, CSR_DEFAULT_RSSI_DB_GAP);
    pMac->roam.configParam.nRoamingTime = CSR_DEFAULT_ROAMING_TIME;
    pMac->roam.configParam.fEnforce11dChannels = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fSupplicantCountryCodeHasPriority = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fEnforceCountryCodeMatch = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.fEnforceDefaultDomain = eANI_BOOLEAN_FALSE;
    pMac->roam.configParam.nActiveMaxChnTime = CSR_ACTIVE_MAX_CHANNEL_TIME;
    pMac->roam.configParam.nActiveMinChnTime = CSR_ACTIVE_MIN_CHANNEL_TIME;
    pMac->roam.configParam.nPassiveMaxChnTime = CSR_PASSIVE_MAX_CHANNEL_TIME;
    pMac->roam.configParam.nPassiveMinChnTime = CSR_PASSIVE_MIN_CHANNEL_TIME;
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
    pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold = 125;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime = 20;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime = 40;
    pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod = 200;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels = 3;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[0] = 1;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[1] = 6;
    pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[2] = 11;
    pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod = 20000; //20 seconds
#endif
#ifdef WLAN_FEATURE_11AC
     pMac->roam.configParam.nVhtChannelWidth = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;
#endif

    pMac->roam.configParam.addTSWhenACMIsOff = 0;
    pMac->roam.configParam.fScanTwice = eANI_BOOLEAN_FALSE;
#ifndef BMPS_WORKAROUND_NOT_NEEDED
    pMac->roam.configParam.doBMPSWorkaround = 0;
#endif

}
eCsrBand csrGetCurrentBand(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    return pMac->roam.configParam.bandCapability;
}
eHalStatus csrSetBand(tHalHandle hHal, eCsrBand eBand)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS;
    if (CSR_IS_PHY_MODE_A_ONLY(pMac) &&
            (eBand == eCSR_BAND_24))
    {
        /* DOT11 mode configured to 11a only and received 
           request to change the band to 2.4 GHz */
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
            "failed to set band cfg80211 = %u, band = %u\n",  
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
            "failed to set band dot11mode = %u, band = %u\n",  
            pMac->roam.configParam.uCfgDot11Mode, eBand);
        return eHAL_STATUS_INVALID_PARAMETER;
    }
    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
            "Band changed to %u (0 - ALL, 1 - 2.4 GHZ, 2 - 5GHZ)\n", eBand);
    pMac->roam.configParam.eBand = eBand; 
    pMac->roam.configParam.bandCapability = eBand; 
    csrScanGetSupportedChannels( pMac );
    status = csrInitGetChannels( pMac );
    if (eHAL_STATUS_SUCCESS == status)
        csrInitChannelList( hHal );
    return status;
}
/* The funcns csrConvertCBIniValueToPhyCBState and csrConvertPhyCBStateToIniValue have been
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
 * and convert to enum value used within the driver in csrChangeDefaultConfigParam using this funcn
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
      case 0:
        phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
        break;
      // secondary LOW
      case 1:
        phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
        break;
      // secondary HIGH
      case 2:
        phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
        break;
#ifdef WLAN_FEATURE_11AC
      case 3:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED; 
        break;
      case 4:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
        break;
      case 5:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
        break; 
      case 6:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
        break;
      case 7:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
        break; 
      case 8:
        phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
        break;
      case 9:
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
        cbIniValue = 0;
        break;
      // secondary LOW
      case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
        cbIniValue = 1;
        break;
      // secondary HIGH
      case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
        cbIniValue = 2;
        break;
#ifdef WLAN_FEATURE_11AC
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        cbIniValue = 3;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
        cbIniValue = 4;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        cbIniValue = 5;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        cbIniValue = 6;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        cbIniValue = 7;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
        cbIniValue = 8;
        break;
      case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
        cbIniValue = 9;
        break;
#endif
      default:
        // return some invalid value
        cbIniValue = 10;
        break;
   }
   return cbIniValue;
}

eHalStatus csrChangeDefaultConfigParam(tpAniSirGlobal pMac, tCsrConfigParam *pParam)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;

    if(pParam)
    {
        pMac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
        pMac->roam.configParam.Is11eSupportEnabled = pParam->Is11eSupportEnabled;
        pMac->roam.configParam.FragmentationThreshold = pParam->FragmentationThreshold;
        pMac->roam.configParam.Is11dSupportEnabled = pParam->Is11dSupportEnabled;
        pMac->roam.configParam.Is11dSupportEnabledOriginal = pParam->Is11dSupportEnabled;
        pMac->roam.configParam.Is11hSupportEnabled = pParam->Is11hSupportEnabled;

        pMac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
        /* channelBondingMode5GHz plays a dual role right now
         * INFRA STA will use this non zero value as CB enabled and SOFTAP will use this non-zero value to determine the secondary channel offset
         * This is how channelBondingMode5GHz works now and this is kept intact to avoid any cfg.ini change
         */
        if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
        {
            smsLog( pMac, LOGW, "Invalid CB value from ini in 2.4GHz band %d, CB DISABLED\n", pParam->channelBondingMode24GHz); 
        }
        pMac->roam.configParam.channelBondingMode24GHz = csrConvertCBIniValueToPhyCBState(pParam->channelBondingMode24GHz);
        if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
        {
            smsLog( pMac, LOGW, "Invalid CB value from ini in 5GHz band %d, CB DISABLED\n", pParam->channelBondingMode5GHz); 
        }
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
        //if HDD passed down non zero values then only update,
        //otherwise keep using the defaults
        if(pParam->nActiveMaxChnTime)
        {
            pMac->roam.configParam.nActiveMaxChnTime = pParam->nActiveMaxChnTime;
        }
        if(pParam->nActiveMinChnTime)
        {
            pMac->roam.configParam.nActiveMinChnTime = pParam->nActiveMinChnTime;
        }
        if(pParam->nPassiveMaxChnTime)
        {
            pMac->roam.configParam.nPassiveMaxChnTime = pParam->nPassiveMaxChnTime;
        }
        if(pParam->nPassiveMinChnTime)
        {
            pMac->roam.configParam.nPassiveMinChnTime = pParam->nPassiveMinChnTime;
        }
        //if upper layer wants to disable idle scan altogether set it to 0
        if(pParam->impsSleepTime)
        {
            //Change the unit from second to microsecond
            tANI_U32 impsSleepTime = pParam->impsSleepTime * PAL_TIMER_TO_SEC_UNIT;
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
#ifdef WLAN_SOFTAP_FEATURE
        pMac->roam.configParam.uCfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(NULL, pMac->roam.configParam.phyMode, 
                                                    pMac->roam.configParam.ProprietaryRatesEnabled);
#else
        pMac->roam.configParam.uCfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(pMac->roam.configParam.phyMode, 
                                                    pMac->roam.configParam.ProprietaryRatesEnabled);
#endif
        //if HDD passed down non zero values for age params, then only update,
        //otherwise keep using the defaults
        if(pParam->nScanResultAgeCount)
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
        
        csrAssignRssiForCategory(pMac, pParam->bCatRssiOffset);
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
        palCopyMemory( pMac->hHdd, &pMac->roam.configParam.csr11rConfig, &pParam->csr11rConfig, sizeof(tCsr11rConfigParams) );
        smsLog( pMac, LOG1, "IsFTResourceReqSupp = %d\n", pMac->roam.configParam.csr11rConfig.IsFTResourceReqSupported); 
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
        pMac->roam.configParam.isFastTransitionEnabled = pParam->isFastTransitionEnabled;
        pMac->roam.configParam.RoamRssiDiff = pParam->RoamRssiDiff;
#endif
#ifdef FEATURE_WLAN_LFR 
        pMac->roam.configParam.isFastRoamIniFeatureEnabled = pParam->isFastRoamIniFeatureEnabled;
#endif

#ifdef FEATURE_WLAN_CCX 
        pMac->roam.configParam.isCcxIniFeatureEnabled = pParam->isCcxIniFeatureEnabled;
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        palCopyMemory( pMac->hHdd, &pMac->roam.configParam.neighborRoamConfig, &pParam->neighborRoamConfig, sizeof(tCsrNeighborRoamConfigParams) );
        smsLog( pMac, LOG1, "nNeighborScanTimerPerioid = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod);
        smsLog( pMac, LOG1, "nNeighborReassocRssiThreshold = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold);  
        smsLog( pMac, LOG1, "nNeighborLookupRssiThreshold = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold);  
        smsLog( pMac, LOG1, "nNeighborScanMinChanTime = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime); 
        smsLog( pMac, LOG1, "nNeighborScanMaxChanTime = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime);
        smsLog( pMac, LOG1, "nMaxNeighborRetries = %d\n", pMac->roam.configParam.neighborRoamConfig.nMaxNeighborRetries); 
        smsLog( pMac, LOG1, "nNeighborResultsRefreshPeriod = %d\n", pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod); 
        {
           int i;
           smsLog( pMac, LOG1, FL("Num of Channels in CFG Channel List: %d\n"), pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels); 
           for( i=0; i< pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels; i++)
           {
              smsLog( pMac, LOG1, "%d ", pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList[i] );
           }
           smsLog( pMac, LOG1, "\n");
        }
#endif
        pMac->roam.configParam.addTSWhenACMIsOff = pParam->addTSWhenACMIsOff;
        pMac->scan.fValidateList = pParam->fValidateList;
        pMac->scan.fEnableBypass11d = pParam->fEnableBypass11d;
        pMac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
        pMac->roam.configParam.fScanTwice = pParam->fScanTwice;
        pMac->scan.fFirstScanOnly2GChnl = pParam->fFirstScanOnly2GChnl;
        /* This parameter is not available in cfg and not passed from upper layers. Instead it is initialized here
         * This paramtere is used in concurrency to determine if there are concurrent active sessions.
         * Is used as a temporary fix to disconnect all active sessions when BMPS enabled so the active session if Infra STA
         * will automatically connect back and resume BMPS since resume BMPS is not working when moving from concurrent to
         * single session
         */
#ifndef BMPS_WORKAROUND_NOT_NEEDED
       pMac->roam.configParam.doBMPSWorkaround = 0;
#endif
#ifdef WLAN_FEATURE_11AC
        pMac->roam.configParam.nVhtChannelWidth = pParam->nVhtChannelWidth;
#endif
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
        //Change the unit from microsecond to second
        pParam->impsSleepTime = pMac->roam.configParam.impsSleepTime / PAL_TIMER_TO_SEC_UNIT;
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

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        palCopyMemory( pMac->hHdd, &pParam->neighborRoamConfig, &pMac->roam.configParam.neighborRoamConfig, sizeof(tCsrNeighborRoamConfigParams) );
#endif
#ifdef WLAN_FEATURE_11AC
        pParam->nVhtChannelWidth = pMac->roam.configParam.nVhtChannelWidth;
#endif

        csrSetChannels(pMac, pParam);

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
            if((eCSR_DOT11_MODE_11a & phyMode) || (eCSR_DOT11_MODE_11a_ONLY & phyMode)) break;
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
        if((0 == phyMode) || (eCSR_DOT11_MODE_TAURUS & phyMode))
        {
            newPhyMode = eCSR_DOT11_MODE_TAURUS;
        }
        else if(eCSR_DOT11_MODE_AUTO & phyMode)
        {
            newPhyMode = eCSR_DOT11_MODE_AUTO;
        }
        else
        {
            //Check for dual band and higher capability first
            if(eCSR_DOT11_MODE_11n_ONLY & phyMode)
            {
                if(eCSR_DOT11_MODE_11n_ONLY != phyMode) break;
                newPhyMode = eCSR_DOT11_MODE_11n_ONLY;
            }
            else if(eCSR_DOT11_MODE_11a_ONLY & phyMode)
            {
                if(eCSR_DOT11_MODE_11a_ONLY != phyMode) break;
                if(eCSR_BAND_24 == eBand) break;
                newPhyMode = eCSR_DOT11_MODE_11a_ONLY;
                eBand = eCSR_BAND_5G;
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
                smsLog( pMac, LOGE, FL(" cannot recognize the phy mode 0x%08X\n"), phyMode );
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
            // Cleanup the rest of channels.   Note we only need to clean up the channels if we had
            // to trim the list.  Calling palZeroMemory() with a 0 size is going to throw asserts on 
            // the debug builds so let's be a bit smarter about that.  Zero out the reset of the channels
            // only if we need to.
            //
            // The amount of memory to clear is the number of channesl that we trimmed 
            // (pChannelList->numChannels - cChannels) times the size of a channel in the structure.
           
            if ( pChannelList->numChannels > cChannels )
            {
                palZeroMemory( pMac->hHdd, &pChannelList->channelList[ cChannels ],
                               sizeof( pChannelList->channelList[ 0 ] ) * ( pChannelList->numChannels - cChannels ) );
                
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
            // Cleanup the rest of channels.   Note we only need to clean up the channels if we had
            // to trim the list.  Calling palZeroMemory() with a 0 size is going to throw asserts on 
            // the debug builds so let's be a bit smarter about that.  Zero out the reset of the channels
            // only if we need to.
            //
            // The amount of memory to clear is the number of channesl that we trimmed 
            // (pChannelList->numChannels - cChannels) times the size of a channel in the structure.
            if ( pChannelList->numChannels > cChannels )
            {
                palZeroMemory( pMac->hHdd, &pChannelList->channelList[ cChannels ],
                               sizeof( pChannelList->channelList[ 0 ] ) * ( pChannelList->numChannels - cChannels ) );
            }            
                               
            pChannelList->numChannels = cChannels;
        }
    }
}
#ifdef WLAN_SOFTAP_FEATURE
#define INFRA_AP_DEFAULT_CHANNEL 6
eHalStatus csrIsValidChannel(tpAniSirGlobal pMac, tANI_U8 chnNum)
{
    tANI_U8 index= 0;
    eHalStatus status = eHAL_STATUS_FAILURE;
    for (index=0; index < pMac->scan.base20MHzChannels.numChannels ;index++)
    {
        if(pMac->scan.base20MHzChannels.channelList[ index ] == chnNum){
            status = eHAL_STATUS_SUCCESS;
            break;
        }
    }
    return status;
}
#endif
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
        smsLog( pMac, LOGE, FL("failed to get channels \n"));
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
#ifdef FEATURE_WLAN_INTEGRATED_SOC /* Need to fix this while dealing with NV item */
            pMac->scan.base20MHzChannels.channelList[ Index ] = pMac->scan.defaultPowerTable[ Index ].chanId;
#else
           pMac->scan.base20MHzChannels.channelList[ Index ] = Index + 1;
           pMac->scan.defaultPowerTable[Index].chanId = Index + 1;
           pMac->scan.defaultPowerTable[Index].pwr = 25;
#endif
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
    // Apply the base channel list, power info, and set the Country code...
    csrApplyChannelPowerCountryInfo( pMac, &pMac->scan.base20MHzChannels, pMac->scan.countryCodeCurrent );
 
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

  if(!ps11dinfo)
  {
     return (status);
  }
  if ( ps11dinfo->Channels.numChannels && ( WNI_CFG_VALID_CHANNEL_LIST_LEN >= ps11dinfo->Channels.numChannels ) ) 
  {
    pMac->scan.base20MHzChannels.numChannels = ps11dinfo->Channels.numChannels;
    status = palCopyMemory(pMac->hHdd, pMac->scan.base20MHzChannels.channelList, 
                           ps11dinfo->Channels.channelList, ps11dinfo->Channels.numChannels);
    if(!HAL_STATUS_SUCCESS(status)) return (status);
  }
  else
  {
     //No change
     return (eHAL_STATUS_SUCCESS);
  }
  //legacy maintenance
  status = palCopyMemory(pMac->hHdd, pMac->scan.countryCodeDefault, 
                         ps11dinfo->countryCode, WNI_CFG_COUNTRY_CODE_LEN);
  if(!HAL_STATUS_SUCCESS(status)) return (status);
  //Tush: at csropen get this initialized with default, during csr reset if this 
  // already set with some value no need initilaize with default again
  if(0 == pMac->scan.countryCodeCurrent[0])
  {
     status = palCopyMemory(pMac->hHdd, pMac->scan.countryCodeCurrent, 
                         ps11dinfo->countryCode, WNI_CFG_COUNTRY_CODE_LEN);
     if(!HAL_STATUS_SUCCESS(status)) return (status);
  }
  // need to add the max power channel list
  if(HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, (void **)&pChanInfo, sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN)))
  {
      palZeroMemory(pMac->hHdd, pChanInfo, sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN);
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
      palFreeMemory(pMac->hHdd, pChanInfoStart);
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
        csrApplyChannelPowerCountryInfo( pMac, &pMac->scan.base20MHzChannels, pMac->scan.countryCodeCurrent );
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

  if(HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, (void **)&pChanInfo, sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN)))
  {
      palZeroMemory(pMac->hHdd, pChanInfo, sizeof(tSirMacChanInfo) * WNI_CFG_VALID_CHANNEL_LIST_LEN);
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
      palFreeMemory(pMac->hHdd, pChanInfoStart);
  }

  return eHAL_STATUS_SUCCESS;
}

//pCommand may be NULL
//Pass in sessionId in case pCommand is NULL. sessionId is not used in case pCommand is not NULL.
void csrRoamRemoveDuplicateCommand(tpAniSirGlobal pMac, tANI_U32 sessionId, tSmeCmd *pCommand, eCsrRoamReason eRoamReason)
{
    tListElem *pEntry, *pNextEntry;
    tSmeCmd *pDupCommand;
    tDblLinkList localList;

    vos_mem_zero(&localList, sizeof(tDblLinkList));
    if(!HAL_STATUS_SUCCESS(csrLLOpen(pMac->hHdd, &localList)))
    {
        smsLog(pMac, LOGE, FL(" failed to open list"));
        return;
    }
    csrLLLock( &pMac->sme.smeCmdPendingList );
    pEntry = csrLLPeekHead( &pMac->sme.smeCmdPendingList, LL_ACCESS_NOLOCK );
    while( pEntry )
    {
        pNextEntry = csrLLNext( &pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_NOLOCK );
        pDupCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        // Remove the previous command if..
        // - the new roam command is for the same RoamReason...
        // - the new roam command is a NewProfileList.
        // - the new roam command is a Forced Dissoc
        // - the new roam command is from an 802.11 OID (OID_SSID or OID_BSSID).
        if ( 
            (pCommand && ( pCommand->sessionId == pDupCommand->sessionId ) &&
                ((pCommand->command == pDupCommand->command) &&
                 (pCommand->u.roamCmd.roamReason == pDupCommand->u.roamCmd.roamReason ||
                    eCsrForcedDisassoc == pCommand->u.roamCmd.roamReason ||
                    eCsrHddIssued == pCommand->u.roamCmd.roamReason))) 
                ||
            //below the pCommand is NULL
            ( (sessionId == pDupCommand->sessionId) &&
              (eSmeCommandRoam == pDupCommand->command) &&
                 ((eCsrForcedDisassoc == eRoamReason) ||
                    (eCsrHddIssued == eRoamReason))
               )
           )
        {
            smsLog(pMac, LOGW, FL("   roamReason = %d\n"), pDupCommand->u.roamCmd.roamReason);
            // Remove the 'stale' roam command from the pending list...
            if(csrLLRemoveEntry( &pMac->sme.smeCmdPendingList, pEntry, LL_ACCESS_NOLOCK ))
            {
                csrLLInsertTail(&localList, pEntry, LL_ACCESS_NOLOCK);
            }
        }
        pEntry = pNextEntry;
    }
    csrLLUnlock( &pMac->sme.smeCmdPendingList );

    while( (pEntry = csrLLRemoveHead(&localList, LL_ACCESS_NOLOCK)) )
    {
        pDupCommand = GET_BASE_ADDR( pEntry, tSmeCmd, Link );
        //Tell caller that the command is cancelled
        csrRoamCallCallback(pMac, pDupCommand->sessionId, NULL, pDupCommand->u.roamCmd.roamId,
                                            eCSR_ROAM_CANCELLED, eCSR_ROAM_RESULT_NONE);
        csrReleaseCommandRoam(pMac, pDupCommand);
    }
    csrLLClose(&localList);
}
eHalStatus csrRoamCallCallback(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo, 
                               tANI_U32 roamId, eRoamCmdStatus u1, eCsrRoamResult u2)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
    WLAN_VOS_DIAG_EVENT_DEF(connectionStatus, vos_event_wlan_status_payload_type);
#endif
    tCsrRoamSession *pSession;
    if( CSR_IS_SESSION_VALID( pMac, sessionId) )
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );
    }
    else
    {
       smsLog(pMac, LOGE, "Session ID:%d is not valid\n", sessionId);
       VOS_ASSERT(0);
       return eHAL_STATUS_FAILURE;
    }
    if(eCSR_ROAM_ASSOCIATION_COMPLETION == u1 && pRoamInfo)
    {
        smsLog(pMac, LOGW, " Assoc complete result = %d statusCode = %d reasonCode = %d\n", u2, pRoamInfo->statusCode, pRoamInfo->reasonCode);
    }
    if ( (pSession == NULL) ||
        (eANI_BOOLEAN_FALSE == pSession->sessionActive) )
    {
        smsLog(pMac, LOG1, "Session ID is not valid\n");
        return eHAL_STATUS_FAILURE;
    }

    if(NULL != pSession->callback)
    {
        if( pRoamInfo )
        {
            pRoamInfo->sessionId = (tANI_U8)sessionId;
        }

        /* avoid holding the global lock when making the roaming callback , original change came 
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
    palZeroMemory(pMac->hHdd, &connectionStatus, sizeof(vos_event_wlan_status_payload_type));
    if((eCSR_ROAM_ASSOCIATION_COMPLETION == u1) && (eCSR_ROAM_RESULT_ASSOCIATED == u2))
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_CONNECT;
       connectionStatus.bssType = pRoamInfo->u.pConnectedProfile->BSSType;
       if(NULL != pRoamInfo->pBssDesc)
       {
          connectionStatus.rssi = pRoamInfo->pBssDesc->rssi * (-1);
          connectionStatus.channel = pRoamInfo->pBssDesc->channelId;
       }
       connectionStatus.qosCapability = pRoamInfo->u.pConnectedProfile->qosConnection;
       connectionStatus.authType = (v_U8_t)diagAuthTypeFromCSRType(pRoamInfo->u.pConnectedProfile->AuthType);
       connectionStatus.encryptionType = (v_U8_t)diagEncTypeFromCSRType(pRoamInfo->u.pConnectedProfile->EncryptionType);
       palCopyMemory(pMac->hHdd, connectionStatus.ssid, pRoamInfo->u.pConnectedProfile->SSID.ssId, 6);
       connectionStatus.reason = eCSR_REASON_UNSPECIFIED;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if((eCSR_ROAM_MIC_ERROR_IND == u1) || (eCSR_ROAM_RESULT_MIC_FAILURE == u2))
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_MIC_ERROR;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_FORCED == u2)
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_USER_REQUESTED;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_DISASSOC_IND == u2)
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_DISASSOC;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
    if(eCSR_ROAM_RESULT_DEAUTH_IND == u2)
    {
       connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
       connectionStatus.reason = eCSR_REASON_DEAUTH;
       WLAN_VOS_DIAG_EVENT_REPORT(&connectionStatus, EVENT_WLAN_STATUS);
    }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    
    return (status);
}
// Returns whether handoff is currently in progress or not
tANI_BOOLEAN csrRoamIsHandoffInProgress(tpAniSirGlobal pMac)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIsHandoffInProgress(pMac);                    
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
    
    //Restore AC weight in case we change it 
    if ( csrIsConnStateConnectedInfra( pMac, sessionId ) )
    {
        smsLog(pMac, LOGE, FL(" restore AC weights (%d-%d-%d-%d)\n"), pMac->roam.ucACWeights[0], pMac->roam.ucACWeights[1],
            pMac->roam.ucACWeights[2], pMac->roam.ucACWeights[3]);
        WLANTL_SetACWeights(pMac->roam.gVosContext, pMac->roam.ucACWeights);
    }
    
    if ( fMICFailure )
    {
        reasonCode = eSIR_MAC_MIC_FAILURE_REASON;
    }
    else if (NewSubstate == eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF)
    {
        reasonCode = eSIR_MAC_DISASSOC_DUE_TO_FTHANDOFF_REASON;
    } else 
    {
        reasonCode = eSIR_MAC_UNSPEC_FAILURE_REASON;
    }    
#ifdef WLAN_FEATURE_VOWIFI_11R
    if ( (csrRoamIsHandoffInProgress(pMac)) && 
         (NewSubstate != eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF))
    {
        tpCsrNeighborRoamControlInfo pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
        palCopyMemory(pMac->hHdd, &bssId, pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.bssid, sizeof(tSirMacAddr));
    } else 
#endif
    if(pSession->pConnectBssDesc)
    {
        palCopyMemory(pMac->hHdd, &bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    
    
    smsLog( pMac, LOGE, "CSR Attempting to Disassociate Bssid= %02x-%02x-%02x-%02x-%02x-%02x subState = %d\n", 
                  bssId[ 0 ], bssId[ 1 ], bssId[ 2 ],
                  bssId[ 3 ], bssId[ 4 ], bssId[ 5 ], NewSubstate );    
    csrRoamSubstateChange( pMac, NewSubstate, sessionId);

    status = csrSendMBDisassocReqMsg( pMac, sessionId, bssId, reasonCode );    
    
    if(HAL_STATUS_SUCCESS(status)) 
    {
        csrRoamLinkDown(pMac, sessionId);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
        //no need to tell QoS that we are disassociating, it will be taken care off in assoc req for HO
        if(eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF != NewSubstate)
        {
            //Tush-QoS: notify QoS module that disassoc happening
            sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_DISCONNECT_REQ, NULL);
        }
#endif
     }
    return (status);
}
#ifdef WLAN_SOFTAP_FEATURE

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
                                           tANI_U8 *pPeerMacAddr,
                                           tANI_U32 reason)
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
        vos_mem_copy(pCommand->u.roamCmd.peerMac, pPeerMacAddr, 6);
        pCommand->u.roamCmd.reason = (tSirMacReasonCodes)reason;
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_FALSE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }while(0);

    return status;
}


/* ---------------------------------------------------------------------------
    \fn csrRoamIssueDeauthSta
    \brief csr function that HDD calls to delete a associated station
    \param sessionId    - session Id for Soft AP
    \param pPeerMacAddr - MAC of associated station to delete
    \param reason - reason code, be one of the tSirMacReasonCodes
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus csrRoamIssueDeauthStaCmd( tpAniSirGlobal pMac, 
                                     tANI_U32 sessionId, 
                                     tANI_U8 *pPeerMacAddr,
                                     tANI_U32 reason)
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
        vos_mem_copy(pCommand->u.roamCmd.peerMac, pPeerMacAddr, 6);
        pCommand->u.roamCmd.reason = (tSirMacReasonCodes)reason;
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_FALSE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
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
        smsLog( pMac, LOGE, "csrRoamIssueTkipCounterMeasures:CSR Session not found\n");
        return (status);
    }
    if (pSession->pConnectBssDesc)
    {
        palCopyMemory(pMac->hHdd, &bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamIssueTkipCounterMeasures:Connected BSS Description in CSR Session not found\n");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR issuing tkip counter measures for Bssid = %02x-%02x-%02x-%02x-%02x-%02x, Enable = %d\n", 
                  bssId[ 0 ], bssId[ 1 ], bssId[ 2 ],
                  bssId[ 3 ], bssId[ 4 ], bssId[ 5 ] , bEnable);
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
        smsLog( pMac, LOGE, "csrRoamGetAssociatedStas:CSR Session not found\n");
        return (status);
    }
    if(pSession->pConnectBssDesc)
    {
        palCopyMemory( pMac->hHdd, &bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid) );
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamGetAssociatedStas:Connected BSS Description in CSR Session not found\n");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR getting associated stations for Bssid = %02x-%02x-%02x-%02x-%02x-%02x\n",
                  bssId[ 0 ], bssId[ 1 ], bssId[ 2 ],
                  bssId[ 3 ], bssId[ 4 ], bssId[ 5 ] );
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
        smsLog( pMac, LOGE, "csrRoamGetWpsSessionOverlap:CSR Session not found\n");
        return (status);
    }
    if(pSession->pConnectBssDesc)
    {
        palCopyMemory( pMac->hHdd, &bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid) );
    }
    else
    {
        smsLog( pMac, LOGE, "csrRoamGetWpsSessionOverlap:Connected BSS Description in CSR Session not found\n");
        return (status);
    }
    smsLog( pMac, LOG2, "CSR getting WPS Session Overlap for Bssid = %02x-%02x-%02x-%02x-%02x-%02x\n",
                  bssId[ 0 ], bssId[ 1 ], bssId[ 2 ],
                  bssId[ 3 ], bssId[ 4 ], bssId[ 5 ] );
     
    status = csrSendMBGetWPSPBCSessions( pMac, sessionId, bssId, pUsrContext, pfnSapEventCallback, pRemoveMac);            
            
    return (status);
}
#endif
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
        palCopyMemory(pMac->hHdd, &bssId, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
    }
    smsLog( pMac, LOG2, "CSR Attempting to Deauth Bssid= %02x-%02x-%02x-%02x-%02x-%02x\n", 
                  bssId[ 0 ], bssId[ 1 ], bssId[ 2 ],
                  bssId[ 3 ], bssId[ 4 ], bssId[ 5 ] );    
    csrRoamSubstateChange( pMac, NewSubstate, sessionId);
    
    status = csrSendMBDeauthReqMsg( pMac, sessionId, bssId, eSIR_MAC_DISASSOC_LEAVING_BSS_REASON );    
    
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
            status = palAllocateMemory( pMac->hHdd, (void **)&pSession->pConnectBssDesc, size); 
        }
        if ( HAL_STATUS_SUCCESS(status) && pSession->pConnectBssDesc ) 
        {
            palCopyMemory( pMac->hHdd, pSession->pConnectBssDesc, pBssDesc, size );    
        }
    }
    
    return (status);
}

eHalStatus csrRoamPrepareBssConfig(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile, 
                                    tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig,
                                    tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    eCsrCfgDot11Mode cfgDot11Mode;
#if defined(VOSS_ENABLED)
    VOS_ASSERT( pIes != NULL );
#endif

    do
    {
        palCopyMemory(pMac->hHdd, &pBssConfig->BssCap, &pBssDesc->capabilityInfo, sizeof(tSirMacCapabilityInfo));
        //get qos
        pBssConfig->qosType = csrGetQoSFromBssDesc(pMac, pBssDesc, pIes);
        //get SSID
        if(pIes->SSID.present)
        {
            palCopyMemory(pMac->hHdd, &pBssConfig->SSID.ssId, pIes->SSID.ssid, pIes->SSID.num_ssid);
            pBssConfig->SSID.length = pIes->SSID.num_ssid;
        }
        else
            pBssConfig->SSID.length = 0;
        if(csrIsNULLSSID(pBssConfig->SSID.ssId, pBssConfig->SSID.length))
        {
            smsLog(pMac, LOGW, "  BSS desc SSID is a wildcard\n");
            //Return failed if profile doesn't have an SSID either.
            if(pProfile->SSIDs.numOfSSIDs == 0)
            {
                smsLog(pMac, LOGW, "  Both BSS desc and profile doesn't have SSID\n");
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
            smsLog(pMac, LOGW, "   Can not find match phy mode\n");
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
        palCopyMemory(pMac->hHdd, &pBssConfig->SSID, &pProfile->SSIDs.SSIDList[0].SSID, sizeof(tSirMacSSid));
    }
    else
    {
        //SSID must present
        return eHAL_STATUS_FAILURE;
    }
    //Settomg up the capabilities
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
#ifdef WLAN_SOFTAP_FEATURE
    pBssConfig->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, pProfile, operationChannel, 
                                        &pBssConfig->eBand);
#else
    pBssConfig->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, (eCsrPhyMode)pProfile->phyMode, operationChannel, 
                                        &pBssConfig->eBand);
#endif
    //QOS
    //Is this correct to always set to this //***
    if ( pBssConfig->BssCap.ess == 1 ) 
    {
#ifdef WLAN_SOFTAP_FEATURE
        /*For Softap case enable WMM*/
        if(CSR_IS_INFRA_AP(pProfile) && (eCsrRoamWmmNoQos != pMac->roam.configParam.WMMSupportMode )){
             qAPisEnabled = TRUE;
        }
        else
#endif
        if (csrRoamGetQosInfoFromBss(pMac, pBssDesc) == eHAL_STATUS_SUCCESS) {
             qAPisEnabled = TRUE;
        } else {
             qAPisEnabled = FALSE;
        }
    } else {
             qAPisEnabled = TRUE;
    }
    if (( eCsrRoamWmmNoQos != pMac->roam.configParam.WMMSupportMode && qAPisEnabled) ||
          (( eCSR_CFG_DOT11_MODE_11N == pBssConfig->uCfgDot11Mode && qAPisEnabled) ||
             ( eCSR_CFG_DOT11_MODE_TAURUS == pBssConfig->uCfgDot11Mode ) ) //For 11n, need QoS
      )
    {
        pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
    } else {
        pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
    }
    
    //auth type
    switch( pProfile->AuthType.authType[0] ) //Take the prefered Auth type.
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
                   "csrRoamGetQosInfoFromBss() failed\n");
         break;
      }
      //check if the AP is QAP & it supports APSD
      if( CSR_IS_QOS_BSS(pIes) )
      {
         return eHAL_STATUS_SUCCESS;
      }
   } while (0);
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
                palCopyMemory( pMac->hHdd, Key0, pProfile->Keys.KeyMaterial[0], WNI_CFG_WEP_KEY_LENGTH_5 );
                Key0Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key0Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[1] ) 
            {
                palCopyMemory( pMac->hHdd, Key1, pProfile->Keys.KeyMaterial[1], WNI_CFG_WEP_KEY_LENGTH_5 );
                Key1Length = WNI_CFG_WEP_KEY_LENGTH_5;
            }
            else
            {
                Key1Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[2] ) 
            {
                palCopyMemory( pMac->hHdd, Key2, pProfile->Keys.KeyMaterial[2], WNI_CFG_WEP_KEY_LENGTH_5 );
                Key2Length = WNI_CFG_WEP_KEY_LENGTH_5;                
            }
            else
            {
                Key2Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[3] ) 
            {
                palCopyMemory( pMac->hHdd, Key3, pProfile->Keys.KeyMaterial[3], WNI_CFG_WEP_KEY_LENGTH_5 );
                Key3Length = WNI_CFG_WEP_KEY_LENGTH_5;                
            }
            else
            {
                Key3Length = 0;
            }      
            break;
        
        case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
            
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
                palCopyMemory( pMac->hHdd, Key0, pProfile->Keys.KeyMaterial[ 0 ], WNI_CFG_WEP_KEY_LENGTH_13 );
                Key0Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key0Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[1] ) 
            {
                palCopyMemory( pMac->hHdd, Key1, pProfile->Keys.KeyMaterial[ 1 ], WNI_CFG_WEP_KEY_LENGTH_13 );
                Key1Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key1Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[2] ) 
            {
                palCopyMemory( pMac->hHdd, Key2, pProfile->Keys.KeyMaterial[ 2 ], WNI_CFG_WEP_KEY_LENGTH_13 );
                Key2Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key2Length = 0;
            }
            
            if ( pProfile->Keys.KeyLength[3] ) 
            {
                palCopyMemory( pMac->hHdd, Key3, pProfile->Keys.KeyMaterial[ 3 ], WNI_CFG_WEP_KEY_LENGTH_13 );
                Key3Length = WNI_CFG_WEP_KEY_LENGTH_13;
            }
            else
            {
                Key3Length = 0;
            }
           
            break;
        
        case eCSR_ENCRYPT_TYPE_WEP40:
        case eCSR_ENCRYPT_TYPE_WEP104:
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
    status = ccmCfgSetInt(pMac, WNI_CFG_QOS_ENABLED, QoSEnabled, NULL, eANI_BOOLEAN_FALSE);
    status = ccmCfgSetInt(pMac, WNI_CFG_WME_ENABLED, WmeEnabled, NULL, eANI_BOOLEAN_FALSE);
    return (status);
}
static eHalStatus csrGetRateSet( tpAniSirGlobal pMac,  tCsrRoamProfile *pProfile, eCsrPhyMode phyMode, tSirBssDescription *pBssDesc,
                           tDot11fBeaconIEs *pIes, tSirMacRateSet *pOpRateSet, tSirMacRateSet *pExRateSet)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    int i;
    eCsrCfgDot11Mode cfgDot11Mode;
    tANI_U8 *pDstRate;
    palZeroMemory(pMac->hHdd, pOpRateSet, sizeof(tSirMacRateSet));
    palZeroMemory(pMac->hHdd, pExRateSet, sizeof(tSirMacRateSet));
#if defined(VOSS_ENABLED)
    VOS_ASSERT( pIes != NULL );
#endif
    
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
                if ( csrRatesIsDot11RateSupported( pMac, pIes->SuppRates.rates[ i ] ) ) 
                {
                    *pDstRate++ = pIes->SuppRates.rates[ i ];
                    pOpRateSet->numRates++;;
                }
            }
        }
        if ( eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode || 
             eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
             eCSR_CFG_DOT11_MODE_TAURUS == cfgDot11Mode ||
             eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode )
        {
            // If there are Extended Rates in the beacon, we will reflect those
            // extended rates that we support in out Extended Operational Rate
            // set:
            pDstRate = pExRateSet->rate;
            if(pIes->ExtSuppRates.present)
            {
                for ( i = 0; i < pIes->ExtSuppRates.num_rates; i++ ) 
                {
                    if ( csrRatesIsDot11RateSupported( pMac, pIes->ExtSuppRates.rates[ i ] ) ) 
                    {
                        *pDstRate++ = pIes->ExtSuppRates.rates[ i ];
                        pExRateSet->numRates++;
                    }
                }
            }
        }
    }//Parsing BSSDesc
    else
    {
        smsLog(pMac, LOGE, FL("failed to parse BssDesc\n"));
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
    tANI_U8 ProprietaryOperationalRates[ 4 ];    // leave enough room for the max number of proprietary rates
    tANI_U32 ProprietaryOperationalRatesLength = 0;
    tANI_U32 PropRatesEnable = 0;
    tANI_U8 MCSRateIdxSet[ SIZE_OF_SUPPORTED_MCS_SET ];
    tANI_U32 MCSRateLength = 0;
#if defined(VOSS_ENABLED)
    VOS_ASSERT( pIes != NULL );
#endif
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
        if ( eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode || 
             eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
             eCSR_CFG_DOT11_MODE_TAURUS == cfgDot11Mode ||
             eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode )
        {
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
        // Enable proprietary MAC features if peer node is Airgo node and STA
        // user wants to use them
        if( pIes->Airgo.present && pMac->roam.configParam.ProprietaryRatesEnabled )
        {
            PropRatesEnable = 1;
        }
        else
        {
            PropRatesEnable = 0;
        }
        // For ANI network companions, we need to populate the proprietary rate
        // set with any proprietary rates we found in the beacon, only if user
        // allows them...
        if ( PropRatesEnable && pIes->Airgo.PropSuppRates.present &&
             ( pIes->Airgo.PropSuppRates.num_rates > 0 )) 
        {
            ProprietaryOperationalRatesLength = pIes->Airgo.PropSuppRates.num_rates;
            if ( ProprietaryOperationalRatesLength > sizeof(ProprietaryOperationalRates) )
            {
               ProprietaryOperationalRatesLength = sizeof (ProprietaryOperationalRates);
            }
            palCopyMemory( pMac->hHdd, ProprietaryOperationalRates, pIes->Airgo.PropSuppRates.rates, ProprietaryOperationalRatesLength );
        }
        else {
            // No proprietary modes...
            ProprietaryOperationalRatesLength = 0;
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
        ccmCfgSetStr(pMac, WNI_CFG_PROPRIETARY_OPERATIONAL_RATE_SET, 
                        ProprietaryOperationalRates, 
                        ProprietaryOperationalRatesLength, NULL, eANI_BOOLEAN_FALSE);
        ccmCfgSetInt(pMac, WNI_CFG_PROPRIETARY_ANI_FEATURES_ENABLED, PropRatesEnable, NULL, eANI_BOOLEAN_FALSE);
        ccmCfgSetStr(pMac, WNI_CFG_CURRENT_MCS_SET, MCSRateIdxSet, 
                        MCSRateLength, NULL, eANI_BOOLEAN_FALSE);        
    }//Parsing BSSDesc
    else
    {
        smsLog(pMac, LOGE, FL("failed to parse BssDesc\n"));
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
#ifdef WLAN_SOFTAP_FEATURE
    cfgDot11Mode = csrRoamGetPhyModeBandForBss( pMac, pProfile, operationChannel, &eBand );
#else
    cfgDot11Mode = csrRoamGetPhyModeBandForBss( pMac, (eCsrPhyMode)pProfile->phyMode, operationChannel, &eBand );
#endif
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
        palCopyMemory( pMac->hHdd, OperationalRates,         
                        DefaultSupportedRates11a.supportedRateSet.rate, 
                        OperationalRatesLength );
                         
        // Nothing in the Extended rate set.
        ExtendedOperationalRatesLength = 0;
        // populate proprietary rates if user allows them
        if ( pMac->roam.configParam.ProprietaryRatesEnabled ) 
        {
            ProprietaryOperationalRatesLength = DefaultSupportedPropRates.numPropRates * 
                                                            sizeof(*DefaultSupportedPropRates.propRate);         
            palCopyMemory( pMac->hHdd, ProprietaryOperationalRates,
                            DefaultSupportedPropRates.propRate, 
                            ProprietaryOperationalRatesLength );
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
        palCopyMemory( pMac->hHdd, OperationalRates, 
                        DefaultSupportedRates11b.supportedRateSet.rate, 
                        OperationalRatesLength );
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
        palCopyMemory( pMac->hHdd, OperationalRates, 
                        DefaultSupportedRates11b.supportedRateSet.rate, 
                        OperationalRatesLength );
        
        // 11a rates go in the Extended rate set.
        ExtendedOperationalRatesLength = DefaultSupportedRates11a.supportedRateSet.numRates * 
                                                    sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
        palCopyMemory( pMac->hHdd, ExtendedOperationalRates,         
                        DefaultSupportedRates11a.supportedRateSet.rate, 
                        ExtendedOperationalRatesLength );
        
        // populate proprietary rates if user allows them
        if ( pMac->roam.configParam.ProprietaryRatesEnabled ) 
        {
            ProprietaryOperationalRatesLength = DefaultSupportedPropRates.numPropRates *
                                                            sizeof(*DefaultSupportedPropRates.propRate);         
            palCopyMemory( pMac->hHdd, ProprietaryOperationalRates, 
                            DefaultSupportedPropRates.propRate, 
                            ProprietaryOperationalRatesLength );
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
    ccmCfgSetInt(pMac, WNI_CFG_PROPRIETARY_ANI_FEATURES_ENABLED, PropRatesEnable, NULL, eANI_BOOLEAN_FALSE);
}
void csrRoamCcmCfgSetCallback(tHalHandle hHal, tANI_S32 result)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    tListElem *pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    tANI_U32 sessionId;
    tSmeCmd *pCommand = NULL;

    if(NULL == pEntry)
    {
        smsLog(pMac, LOGW, "   CFG_CNF with active list empty\n");
        return;
    }
    pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
    sessionId = pCommand->sessionId;

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
                          tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32   cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
    tANI_U8    channel = 0;
    //Make sure we have the domain info for the BSS we try to connect to.
    //Do we need to worry about sequence for OSs that are not Windows??
    if(pBssDesc)
    {
        if(csrLearnCountryInformation(pMac, pBssDesc, pIes, eANI_BOOLEAN_TRUE))
        {
            //Make sure the 11d info from this BSSDesc can be applied
            pMac->scan.fAmbiguous11dInfoFound = eANI_BOOLEAN_FALSE;
            csrApplyCountryInformation( pMac, TRUE );
        }
    }
        
    //Qos
    csrSetQosToCfg( pMac, sessionId, pBssConfig->qosType );
    //SSID
    csrSetCfgSsid(pMac, &pBssConfig->SSID );
    //fragment threshold
    //ccmCfgSetInt(pMac, WNI_CFG_FRAGMENTATION_THRESHOLD, csrGetFragThresh(pMac), NULL, eANI_BOOLEAN_FALSE);
    //RTS threshold
    //ccmCfgSetInt(pMac, WNI_CFG_RTS_THRESHOLD, csrGetRTSThresh(pMac), NULL, eANI_BOOLEAN_FALSE);

    //ccmCfgSetInt(pMac, WNI_CFG_DOT11_MODE, csrTranslateToWNICfgDot11Mode(pMac, pBssConfig->uCfgDot11Mode), NULL, eANI_BOOLEAN_FALSE);
        
    //Auth type
    ccmCfgSetInt(pMac, WNI_CFG_AUTHENTICATION_TYPE, pBssConfig->authType, NULL, eANI_BOOLEAN_FALSE);
    //encryption type
    csrSetCfgPrivacy(pMac, pProfile, (tANI_BOOLEAN)pBssConfig->BssCap.privacy );
    //short slot time
    ccmCfgSetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, pBssConfig->uShortSlotTime, NULL, eANI_BOOLEAN_FALSE);
#ifdef WLAN_SOFTAP_FEATURE
    //11d
    ccmCfgSetInt(pMac, WNI_CFG_11D_ENABLED,
                        ((pBssConfig->f11hSupport) ? pBssConfig->f11hSupport : pProfile->ieee80211d),
                        NULL, eANI_BOOLEAN_FALSE);
#endif
    /*//11h
    ccmCfgSetInt(pMac, WNI_CFG_11H_ENABLED, pMac->roam.configParam.Is11hSupportEnabled, NULL, eANI_BOOLEAN_FALSE);
    */
    ccmCfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, pBssConfig->uPowerLimit, NULL, eANI_BOOLEAN_FALSE);
    //CB

    if(CSR_IS_INFRA_AP(pProfile) || CSR_IS_WDS_AP(pProfile))
    {
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
#ifdef WLAN_FEATURE_11AC
    if(cfgCb > 2 )
    {
	if(!WDA_getFwWlanFeatCaps(DOT11AC)) {
            cfgCb = csrGetHTCBStateFromVHTCBState(cfgCb);
	}
	else 
	{
            ccmCfgSetInt(pMac, WNI_CFG_VHT_CHANNEL_WIDTH,  pMac->roam.configParam.nVhtChannelWidth, NULL, eANI_BOOLEAN_FALSE);
	}
    }
    else
#endif
    ccmCfgSetInt(pMac, WNI_CFG_CHANNEL_BONDING_MODE, cfgCb, NULL, eANI_BOOLEAN_FALSE);
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
    
    status = palAllocateMemory(pMac->hHdd, (void **)&pBssConfig, sizeof(tBssConfigParam)); 
    if(HAL_STATUS_SUCCESS(status))
    {
        palZeroMemory(pMac->hHdd, pBssConfig, sizeof(tBssConfigParam));
        status = csrRoamPrepareBssConfig(pMac, pProfile, pBssDesc, pBssConfig, pIes);
        if(HAL_STATUS_SUCCESS(status))
        {
            pSession->bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
            /* This will allow to pass cbMode during join req */
            pSession->bssParams.cbMode= pBssConfig->cbMode;
            //For IBSS, we need to prepare some more information
            if( csrIsBssTypeIBSS(pProfile->BSSType) || CSR_IS_WDS( pProfile )
#ifdef WLAN_SOFTAP_FEATURE
              || CSR_IS_INFRA_AP(pProfile)
#endif
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
                        status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE );  
                    }
                    else 
                    {  
                        // In an Infrastucture and going to an Infrastructure network with the same SSID.  This
                        // calls for a Reassociation sequence.  So issue the CFG sets for this new AP.
                        if ( pBssDesc ) 
                        {
                            // Set parameters for this Bss.    
                            status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile, pBssDesc, pBssConfig, pIes);
                        }
                    }      
                }
                else 
                {
                    // Neiher in IBSS nor in Infra.  We can go ahead and set the CFG for tne new network...
                    // Nothing to stop.
                    if ( pBssDesc || CSR_IS_WDS_AP( pProfile )
#ifdef WLAN_SOFTAP_FEATURE
                     || CSR_IS_INFRA_AP(pProfile)
#endif
                    ) 
                    {      
                        // Set parameters for this Bss.    
                        status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile, pBssDesc, pBssConfig, pIes);
                    }  
                }
            }
        }//Success getting BSS config info
        palFreeMemory(pMac->hHdd, pBssConfig);
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
            // Check to see if the Auth type has changed in the Profile.  If so, we don't want to Reassociate
            // with Authenticating first.  To force this, stop the current association (Disassociate) and 
            // then re 'Join' the AP, wihch will force an Authentication (with the new Auth type) followed by 
            // a new Association.
            if(csrIsSameProfile(pMac, &pSession->connectedProfile, pProfile))
            {
                smsLog(pMac, LOGW, FL("  detect same profile authType = %d encryType = %d\n"), pProfile->AuthType, pProfile->EncryptionType);
                if(csrRoamIsSameProfileKeys(pMac, &pSession->connectedProfile, pProfile))
                {
                    eRoamState = eCsrReassocToSelfNoCapChange;
                }
                else
                {
                    tBssConfigParam bssConfig;
                    //The key changes
                    palZeroMemory(pMac->hHdd, &bssConfig, sizeof(bssConfig));
                    status = csrRoamPrepareBssConfig(pMac, pProfile, pBssDesc, &bssConfig, pIesLocal);
                    if(HAL_STATUS_SUCCESS(status))
                    {
                        pSession->bssParams.uCfgDot11Mode = bssConfig.uCfgDot11Mode;
                        pSession->bssParams.cbMode = bssConfig.cbMode;
                        //Reapply the config including Keys so reassoc is happening.
                        status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile, pBssDesc, &bssConfig, pIesLocal);
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
                    smsLog(pMac, LOGW, FL("  fail to issue disassociate\n"));
                    eRoamState = eCsrStopRoaming;
                }                       
            }
        }            
        else 
        {
            // note:  we used to pre-auth here with open authentication networks but that was not working so well.
            // we had a lot of join timeouts when testing at Samsung.  removing this step helped associations 
            // work much better.
            //
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
            palFreeMemory(pMac->hHdd, pIesLocal);
        }
    }
    return( eRoamState );
}

eHalStatus csrRoamShouldRoam(tpAniSirGlobal pMac, tANI_U32 sessionId, 
                             tSirBssDescription *pBssDesc, tANI_U32 roamId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamInfo roamInfo;
    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
    roamInfo.pBssDesc = pBssDesc;
    status = csrRoamCallCallback(pMac, sessionId, &roamInfo, roamId, eCSR_ROAM_SHOULD_ROAM, eCSR_ROAM_RESULT_NONE);
    return (status);
}
//In case no matching BSS is found, use whatever default we can find
static void csrRoamAssignDefaultParam( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    //Need to get all negotiated types in place first
    //auth type
    switch( pCommand->u.roamCmd.roamProfile.AuthType.authType[0] ) //Take the prefered Auth type.
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
    //In this case, the multicast encryption needs to follow the uncast ones.
    pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType = 
    pCommand->u.roamCmd.roamProfile.EncryptionType.encryptionType[0];
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
                        palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
                    if (vos_concurrent_sessions_running() && 
                        !csrIsValidMcConcurrentSession(pMac, sessionId, &pScanResult->Result.BssDescriptor))
                    {
                        concurrentChannel = 
                            csrGetConcurrentOperationChannel(pMac);
                        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_HIGH, "%s: "
                                " csr Concurrent Channel = %d", __FUNCTION__, concurrentChannel);
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
        //We have something to roam, tell HDD when it is infra.
        //For IBSS, the indication goes back to HDD via eCSR_ROAM_IBSS_IND
        //For WDS, the indication is eCSR_ROAM_WDS_IND
        if( CSR_IS_INFRASTRUCTURE( pProfile ) )
        {
            if(pRoamInfo)
            {
                pSession->bRefAssocStartCnt--;
                //Complete the last association attemp because a new one is about to be tried
                csrRoamCallCallback(pMac, sessionId, pRoamInfo, pCommand->u.roamCmd.roamId, 
                                        eCSR_ROAM_ASSOCIATION_COMPLETION, 
                                        eCSR_ROAM_RESULT_NOT_ASSOCIATED);
            }
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(roamInfo));
            if(pScanResult)
            {
                tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *)pScanResult->Result.pvIes;
                if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, &pScanResult->Result.BssDescriptor, &pIesLocal))) )
                {
                    smsLog(pMac, LOGE, FL(" cannot parse IEs\n"));
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
                    pCommand->u.roamCmd.roamProfile.uapsd_mask &= ~(acm_mask);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                }
                else
                {
                    pCommand->u.roamCmd.roamProfile.uapsd_mask = 0;
                }
                if( pIesLocal && !pScanResult->Result.pvIes)
                {
                    palFreeMemory(pMac->hHdd, pIesLocal);
                }
            }
            else
            {
                pCommand->u.roamCmd.roamProfile.uapsd_mask = 0;
            }
            roamInfo.pProfile = pProfile;
            pSession->bRefAssocStartCnt++;
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
#ifdef WLAN_SOFTAP_FEATURE
             || (CSR_IS_INFRA_AP(pProfile))
#endif
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
                smsLog(pMac, LOGW, FL("cannot continue without BSS list\n"));
                eRoamState = eCsrStopRoaming;
                break;
            }
        } 
        else //We have BSS 
        {
            //Need to assign these value because they are used in csrIsSameProfile
            pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
            pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType = pScanResult->ucEncryptionType; //Negotiated while building scan result.
            pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType = pScanResult->mcEncryptionType;
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
            //Complete the last association attemp because a new one is about to be tried
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
        // Attept to join a Bss...
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
                    smsLog(pMac, LOGW, FL("  failed to issue disassociate, status = %d\n"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
            else if( csrIsConnStateIbss(pMac, sessionId) )
            {
                status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGW, FL("  failed to issue stop bss, status = %d\n"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
#ifdef WLAN_SOFTAP_FEATURE
            else if (csrIsConnStateConnectedInfraAp(pMac, sessionId))
            {
                status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
                if(!HAL_STATUS_SUCCESS(status))
                {
                    smsLog(pMac, LOGW, FL("  failed to issue stop bss, status = %d\n"), status);
                    //roam command is completed by caller in the failed case
                    fComplete = eANI_BOOLEAN_TRUE;
                }
            }
#endif
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
        //the roaming is cancelled. Simply complete the command
        smsLog(pMac, LOG1, FL("  Roam command cancelled\n"));
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
        //the roaming is cancelled. Simply complete the command
        smsLog(pMac, LOG1, FL("  Roam command cancelled\n"));
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
    
    switch ( pCommand->u.roamCmd.roamReason )
    {
    case eCsrForcedDisassoc:
        csrFreeRoamProfile(pMac, sessionId);
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, FALSE );
        break;
     case eCsrSmeIssuedDisassocForHandoff:
        //Not to free pMac->roam.pCurRoamProfile (via csrFreeRoamProfile) because it is needed after disconnect
#if 0 // TODO : Confirm this change
        status = csrRoamProcessDisassociate( pMac, pCommand, FALSE );
#else
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, FALSE );
#endif

        break;
    case eCsrForcedDisassocMICFailure:
        csrFreeRoamProfile(pMac, sessionId);
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, TRUE, TRUE );
        break;
    case eCsrForcedDeauth:
        csrFreeRoamProfile(pMac, sessionId);
        status = csrRoamProcessDisassocDeauth( pMac, pCommand, FALSE, FALSE );
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
                smsLog(pMac, LOGE, FL("  fail to parse IEs\n"));
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
   
                smsLog(pMac, LOG1, FL("  calling csrRoamIssueReassociate\n"));
                csrRoamIssueReassociate( pMac, sessionId, pSession->pConnectBssDesc, pIes,
                                         &pCommand->u.roamCmd.roamProfile );
                palFreeMemory(pMac->hHdd, pIes);
                pIes = NULL;
            }
        }
        break;
    }
    case eCsrCapsChange:
        smsLog(pMac, LOGE, FL("received eCsrCapsChange \n"));
        csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId );
        status = csrRoamIssueDisassociate( pMac, sessionId, eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, FALSE); 
        break;
    case eCsrSmeIssuedFTReassoc:
        smsLog(pMac, LOGE, FL("received FT Reassoc Req \n"));
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

    default:
        csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId );

        if( pCommand->u.roamCmd.fUpdateCurRoamProfile )
        {
            //Remember the roaming profile 
            csrFreeRoamProfile(pMac, sessionId);
            if(HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, (void **)&pSession->pCurRoamProfile, sizeof(tCsrRoamProfile))))
            {
                palZeroMemory(pMac->hHdd, pSession->pCurRoamProfile, sizeof(tCsrRoamProfile));
                csrRoamCopyProfile(pMac, pSession->pCurRoamProfile, &pCommand->u.roamCmd.roamProfile);
            }
        }
 
        //At this point, original uapsd_mask is saved in pCurRoamProfile
        //uapsd_mask in the pCommand may change from this point on.
 
        // Attempt to roam with the new scan results (if we need to..)
        status = csrRoam( pMac, pCommand );
        break;
    }
    return (status);
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
    palZeroMemory(pMac->hHdd, &pCommand->u.roamCmd, sizeof(tRoamCmd));
}

void csrReinitWmStatusChangeCmd(tpAniSirGlobal pMac, tSmeCmd *pCommand)
{
    palZeroMemory(pMac->hHdd, &pCommand->u.wmStatusChangeCmd, sizeof(tWmStatusChangeCmd));
}
void csrRoamComplete( tpAniSirGlobal pMac, eCsrRoamCompleteResult Result, void *Context )
{
    tListElem *pEntry;
    tSmeCmd *pCommand;
    tANI_BOOLEAN fReleaseCommand = eANI_BOOLEAN_TRUE;
    smsLog( pMac, LOG2, "roamQ: Roam Completion ...\n" );
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
                    smsLog( pMac, LOGE, " **********csrRoamComplete fail to release command reason %d\n",
                        pCommand->u.roamCmd.roamReason );
                }
            }
            else
            {
                smsLog( pMac, LOGE, " **********csrRoamComplete fail to release command reason %d\n",
                    pCommand->u.roamCmd.roamReason );
            }
        }
        else
        {
            smsLog( pMac, LOGW, "CSR: Roam Completion called but ROAM command is not ACTIVE ...\n" );
        }
    }
    else
    {
        smsLog( pMac, LOGW, "CSR: Roam Completion called but NO commands are ACTIVE ...\n" );
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
    palZeroMemory( pMac->hHdd, &(pSession->PmkidCandidateInfo[0]), sizeof(tPmkidCandidateInfo) * CSR_MAX_PMKID_ALLOWED );
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
    palZeroMemory( pMac->hHdd, &(pSession->BkidCandidateInfo[0]), sizeof(tBkidCandidateInfo) * CSR_MAX_BKID_ALLOWED );
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
        )
    {
        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal))) )
        {
            smsLog(pMac, LOGE, FL(" cannot parse IEs\n"));
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
                    if(HAL_STATUS_SUCCESS((status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWpaRsnRspIE, nIeLen + 2))))
                    {
                        pSession->pWpaRsnRspIE[0] = DOT11F_EID_RSN;
                        pSession->pWpaRsnRspIE[1] = (tANI_U8)nIeLen;
                        //copy upto akm_suites
                        pIeBuf = pSession->pWpaRsnRspIE + 2;
                        palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->RSN.version, 8);
                        pIeBuf += 8;
                        if( pIesLocal->RSN.pwise_cipher_suite_count )
                        {
                            //copy pwise_cipher_suites
                            palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->RSN.pwise_cipher_suites, 
                                            pIesLocal->RSN.pwise_cipher_suite_count * 4);
                            pIeBuf += pIesLocal->RSN.pwise_cipher_suite_count * 4;
                        }
                        palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->RSN.akm_suite_count, 2);
                        pIeBuf += 2;
                        if( pIesLocal->RSN.akm_suite_count )
                        {
                            //copy akm_suites
                            palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->RSN.akm_suites, 
                                            pIesLocal->RSN.akm_suite_count * 4);
                            pIeBuf += pIesLocal->RSN.akm_suite_count * 4;
                        }
                        //copy the rest
                        palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->RSN.akm_suites + 
                                        pIesLocal->RSN.akm_suite_count * 4, 
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
                    // The WPA capabilities follows the Auth Suite (two octects)--
                    // this field is optional, and we always "send" zero, so just
                    // remove it.  This is consistent with our assumptions in the
                    // frames compiler; c.f. bug 15234:
                    //nIeLen doesn't count EID and length fields
                    if(HAL_STATUS_SUCCESS((status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWpaRsnRspIE, nIeLen + 2))))
                    {
                        pSession->pWpaRsnRspIE[0] = DOT11F_EID_WPA;
                        pSession->pWpaRsnRspIE[1] = (tANI_U8)nIeLen;
                        pIeBuf = pSession->pWpaRsnRspIE + 2;
                        //Copy WPA OUI
                        palCopyMemory(pMac->hHdd, pIeBuf, &csrWpaOui[1], 4);
                        pIeBuf += 4;
                        palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->WPA.version, 8 + 
                                        pIesLocal->WPA.unicast_cipher_count * 4);
                        pIeBuf += 8 + pIesLocal->WPA.unicast_cipher_count * 4;
                        palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->WPA.auth_suite_count, 2 + 
                                        pIesLocal->WPA.auth_suite_count * 4);
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
                   if(HAL_STATUS_SUCCESS((status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWapiRspIE, nIeLen + 2))))
                   {
                      pSession->pWapiRspIE[0] = DOT11F_EID_WAPI;
                      pSession->pWapiRspIE[1] = (tANI_U8)nIeLen;
                      pIeBuf = pSession->pWapiRspIE + 2;
                      //copy upto akm_suite_count
                      palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->WAPI.version, 4);
                      pIeBuf += 4;
                      if( pIesLocal->WAPI.akm_suite_count )
                      {
                         //copy akm_suites
                         palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->WAPI.akm_suites, 
                                        pIesLocal->WAPI.akm_suite_count * 4);
                         pIeBuf += pIesLocal->WAPI.akm_suite_count * 4;
    }
                      palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->WAPI.unicast_cipher_suite_count, 2);
                      pIeBuf += 2;
                      if( pIesLocal->WAPI.unicast_cipher_suite_count )
                      {
                         //copy pwise_cipher_suites
                         palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->WAPI.unicast_cipher_suites, 
                                        pIesLocal->WAPI.unicast_cipher_suite_count * 4);
                         pIeBuf += pIesLocal->WAPI.unicast_cipher_suite_count * 4;
                      }
                      //gp_cipher_suite + preauth + reserved + bkid_count
                      palCopyMemory(pMac->hHdd, pIeBuf, &pIesLocal->WAPI.multicast_cipher_suite, 8);
                      pIeBuf += 8;
                      if( pIesLocal->WAPI.bkid_count )
                      {
                         //copy akm_suites
                         palCopyMemory(pMac->hHdd, pIeBuf, pIesLocal->WAPI.bkid, pIesLocal->WAPI.bkid_count * 4);
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
                palFreeMemory(pMac->hHdd, pIesLocal);
        }
    }
    }
    return (status);
}

static void csrCheckAndUpdateACWeight( tpAniSirGlobal pMac, tDot11fBeaconIEs *pIEs )
{
    v_U8_t bACWeights[WLANTL_MAX_AC];
    v_U8_t paramBk, paramBe, paramVi, paramVo;
    v_BOOL_t fWeightChange = VOS_FALSE;
    //Compare two ACs' EDCA parameters, from low to high (BK, BE, VI, VO)
    //The "formula" is, if lower AC's AIFSN+CWMin is bigger than a fixed amount
    //of the higher AC one, make the higher AC has the same weight as the lower AC.
    //This doesn't address the case where the lower AC needs a real higher weight
    if( pIEs->WMMParams.present )
    {
        //no change to the lowest ones
        bACWeights[WLANTL_AC_BK] = pMac->roam.ucACWeights[WLANTL_AC_BK];
        bACWeights[WLANTL_AC_BE] = pMac->roam.ucACWeights[WLANTL_AC_BE];
        bACWeights[WLANTL_AC_VI] = pMac->roam.ucACWeights[WLANTL_AC_VI];
        bACWeights[WLANTL_AC_VO] = pMac->roam.ucACWeights[WLANTL_AC_VO];
        paramBk = pIEs->WMMParams.acbk_aifsn + pIEs->WMMParams.acbk_acwmin;
        paramBe = pIEs->WMMParams.acbe_aifsn + pIEs->WMMParams.acbe_acwmin;
        paramVi = pIEs->WMMParams.acvi_aifsn + pIEs->WMMParams.acvi_acwmin;
        paramVo = pIEs->WMMParams.acvo_aifsn + pIEs->WMMParams.acvo_acwmin;
        if( SME_DETECT_AC_WEIGHT_DIFF(paramBk, paramBe) )
        {
            bACWeights[WLANTL_AC_BE] = bACWeights[WLANTL_AC_BK];
            fWeightChange = VOS_TRUE;
        }
        if( SME_DETECT_AC_WEIGHT_DIFF(paramBk, paramVi) )
        {
            bACWeights[WLANTL_AC_VI] = bACWeights[WLANTL_AC_BK];
            fWeightChange = VOS_TRUE;
        }
        else if( SME_DETECT_AC_WEIGHT_DIFF(paramBe, paramVi) )
        {
            bACWeights[WLANTL_AC_VI] = bACWeights[WLANTL_AC_BE];
            fWeightChange = VOS_TRUE;
        }
        if( SME_DETECT_AC_WEIGHT_DIFF(paramBk, paramVo) )
        {
            bACWeights[WLANTL_AC_VO] = bACWeights[WLANTL_AC_BK];
            fWeightChange = VOS_TRUE;
        }
        else if( SME_DETECT_AC_WEIGHT_DIFF(paramBe, paramVo) )
        {
            bACWeights[WLANTL_AC_VO] = bACWeights[WLANTL_AC_BE];
            fWeightChange = VOS_TRUE;
        }
        else if( SME_DETECT_AC_WEIGHT_DIFF(paramVi, paramVo) )
        {
            bACWeights[WLANTL_AC_VO] = bACWeights[WLANTL_AC_VI];
            fWeightChange = VOS_TRUE;
        }
        if(fWeightChange)
        {
            smsLog(pMac, LOGE, FL(" change AC weights (%d-%d-%d-%d)\n"), bACWeights[0], bACWeights[1],
                bACWeights[2], bACWeights[3]);
            WLANTL_SetACWeights(pMac->roam.gVosContext, bACWeights);
        }
    }
}
#ifdef WLAN_FEATURE_VOWIFI_11R
//Returns whether the current association is a 11r assoc or not
tANI_BOOLEAN csrRoamIs11rAssoc(tpAniSirGlobal pMac)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIs11rAssoc(pMac);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}
#endif
#ifdef FEATURE_WLAN_CCX
//Returns whether the current association is a CCX assoc or not
tANI_BOOLEAN csrRoamIsCCXAssoc(tpAniSirGlobal pMac)
{
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
    return csrNeighborRoamIsCCXAssoc(pMac);
#else
    return eANI_BOOLEAN_FALSE;
#endif
}
#endif
#ifdef FEATURE_WLAN_LFR
//Returns whether "Legacy Fast Roaming" is currently enabled...or not
tANI_BOOLEAN csrRoamIsFastRoamEnabled(tpAniSirGlobal pMac)
{
    return pMac->roam.configParam.isFastRoamIniFeatureEnabled;
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
#ifdef WLAN_SOFTAP_FEATURE
    tSirSmeStartBssRsp  *pSmeStartBssRsp = NULL;
#endif

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eANI_BOOLEAN_FALSE;
    }

    smsLog( pMac, LOG1, FL("Processing ROAM results...\n"));
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
            smsLog(pMac, LOGW, FL("receives association indication\n"));
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(roamInfo));
            //always free the memory here
            if(pSession->pWpaRsnRspIE)
            {
                pSession->nWpaRsnRspIeLength = 0;
                palFreeMemory(pMac->hHdd, pSession->pWpaRsnRspIE);
                pSession->pWpaRsnRspIE = NULL;
            }
#ifdef FEATURE_WLAN_WAPI
            if(pSession->pWapiRspIE)
            {
                pSession->nWapiRspIeLength = 0;
                palFreeMemory(pMac->hHdd, pSession->pWapiRspIE);
                pSession->pWapiRspIE = NULL;
            }
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_BTAMP_UT_RF
            //Reset counter so no join retry is needed.
            pSession->maxRetryCount = 0;
            csrRoamStopJoinRetryTimer(pMac, sessionId);
#endif
            /* This creates problem since we have not saved the connected profile.
            So moving this after saving the profile
            */
            //csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED );
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
                    palCopyMemory(pMac->hHdd, &roamInfo.bssid, &pSirBssDesc->bssId, sizeof(tCsrBssid));
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
                        palCopyMemory(pMac->hHdd, &roamInfo.bssid, &pSirBssDesc->bssId, sizeof(tCsrBssid));
                    }
                }
            }
            if( pSirBssDesc )
            {
                roamInfo.staId = HAL_STA_INVALID_IDX;
                csrRoamSaveConnectedInfomation(pMac, sessionId, pProfile, pSirBssDesc, pIes);
                    //Save WPA/RSN IE
                csrRoamSaveSecurityRspIE(pMac, sessionId, pProfile->negotiatedAuthType, pSirBssDesc, pIes);
#ifdef FEATURE_WLAN_CCX
                roamInfo.isCCXAssoc = pSession->connectedProfile.isCCXAssoc;
#endif
                
                // csrRoamStateChange also affects sub-state. Hence, csrRoamStateChange happens first and then
                // substate change.
                // Moving even save profile above so that below mentioned conditon is also met.
                // JEZ100225: Moved to after saving the profile. Fix needed in main/latest
                csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
                // Make sure the Set Context is issued before link indication to NDIS.  After link indication is 
                // made to NDIS, frames could start flowing.  If we have not set context with LIM, the frames
                // will be dropped for the security context may not be set properly. 
                //
                // this was causing issues in the 2c_wlan_wep WHQL test when the SetContext was issued after the link
                // indication.  (Link Indication happens in the profFSMSetConnectedInfra call).
                //
                // this reordering was done on titan_prod_usb branch and is being replicated here.
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
                        smsLog( pMac, LOGE, FL("  Set context for unicast fail\n") );
                        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
                    }
                    // Issue the set Context request to LIM to establish the Broadcast STA context
                    csrRoamIssueSetContextReq( pMac, sessionId, pProfile->negotiatedMCEncryptionType,
                                               pSirBssDesc, &BroadcastMac,
                                               FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                }
                else
                {
                    //Need to wait for supplicant authtication
                    roamInfo.fAuthRequired = eANI_BOOLEAN_TRUE;
                    //Set the subestate to WaitForKey in case authentiation is needed
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
                    //This time should be long enough for the rest of the process plus setting key
                    if(!HAL_STATUS_SUCCESS( csrRoamStartWaitForKeyTimer( pMac, key_timeout_interval ) ) )
                    {
                        //Reset our state so nothting is blocked.
                        smsLog( pMac, LOGE, FL("   Failed to start pre-auth timer\n") );
                        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);
                    }
                }
                
                assocInfo.pBssDesc = pSirBssDesc; //could be NULL
                assocInfo.pProfile = pProfile;
                if(Context)
                {
                    tSirSmeJoinRsp *pJoinRsp = (tSirSmeJoinRsp *)Context;
                    tANI_U32 len;
                    csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
                    len = pJoinRsp->assocReqLength + pJoinRsp->assocRspLength + pJoinRsp->beaconLength;
#ifdef WLAN_FEATURE_VOWIFI_11R
                    len += pJoinRsp->parsedRicRspLen;
#endif /* WLAN_FEATURE_VOWIFI_11R */                    
#ifdef FEATURE_WLAN_CCX
                    len += pJoinRsp->tspecIeLen;
#endif
                    if(len)
                    {
                        if(HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, 
                                        (void **)&pSession->connectedInfo.pbFrames, len)))
                        {
                            if(HAL_STATUS_SUCCESS( palCopyMemory(pMac->hHdd, 
                                            pSession->connectedInfo.pbFrames, pJoinRsp->frames, len) ))
                            {
                                pSession->connectedInfo.nAssocReqLength = pJoinRsp->assocReqLength;
                                pSession->connectedInfo.nAssocRspLength = pJoinRsp->assocRspLength;
                                pSession->connectedInfo.nBeaconLength = pJoinRsp->beaconLength;
#ifdef WLAN_FEATURE_VOWIFI_11R
                                pSession->connectedInfo.nRICRspLength = pJoinRsp->parsedRicRspLen;
#endif /* WLAN_FEATURE_VOWIFI_11R */                                
#ifdef FEATURE_WLAN_CCX
                                pSession->connectedInfo.nTspecIeLength = pJoinRsp->tspecIeLen;
#endif
                                roamInfo.nAssocReqLength = pJoinRsp->assocReqLength;
                                roamInfo.nAssocRspLength = pJoinRsp->assocRspLength;
                                roamInfo.nBeaconLength = pJoinRsp->beaconLength;
                                roamInfo.pbFrames = pSession->connectedInfo.pbFrames;
                            }
                            else
                            {
                                smsLog(pMac, LOGE, "%s: Copying of memory failed for %d bytes !!!\n", 
                                        __FUNCTION__, len);
                                palFreeMemory( pMac->hHdd, pSession->connectedInfo.pbFrames );
                                pSession->connectedInfo.pbFrames = NULL;
                            }
                        }
                    }
                    if(pCommand->u.roamCmd.fReassoc)
                    {
                        roamInfo.fReassocReq = roamInfo.fReassocRsp = eANI_BOOLEAN_TRUE;
                    }
                    pSession->connectedInfo.staId = ( tANI_U8 )pJoinRsp->staId;
                    roamInfo.staId = ( tANI_U8 )pJoinRsp->staId;
                    roamInfo.ucastSig = ( tANI_U8 )pJoinRsp->ucastSig;
                    roamInfo.bcastSig = ( tANI_U8 )pJoinRsp->bcastSig;
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
#ifdef FEATURE_WLAN_UAPSD_FW_TRG_FRAMES
                //start UAPSD if uapsd_mask is not 0 because HDD will configure for trigger frame
                //It may be better to let QoS do this????
                if( pSession->connectedProfile.modifyProfileFields.uapsd_mask )
                {
                    smsLog(pMac, LOGE, " uapsd_mask (0x%X) set, request UAPSD now\n",
                        pSession->connectedProfile.modifyProfileFields.uapsd_mask);
                    pmcStartUapsd( pMac, NULL, NULL );
                }
#endif
                roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                if( pSession->bRefAssocStartCnt > 0 )
                {
                    pSession->bRefAssocStartCnt--;
#ifndef BMPS_WORKAROUND_NOT_NEEDED
                    if(  csrIsConcurrentSessionRunning( pMac ) )
                    {
                       pMac->roam.configParam.doBMPSWorkaround = 1;
                    }
#endif
                    csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);
                }
                
                csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_NONE, eANI_BOOLEAN_TRUE);
                // reset the PMKID candidate list
                csrResetPMKIDCandidateList( pMac, sessionId );
                //Update TL's AC weight base on the current EDCA parameters
                //These parameters may change in the course of the connection, that sictuation
                //is not taken care here. This change is mainly to address a WIFI WMM test where
                //BE has a equal or higher TX priority than VI. 
                //We only do this for infra link
                if( csrIsConnStateConnectedInfra(pMac, sessionId ) && pIes )
                {
                    csrCheckAndUpdateACWeight(pMac, pIes);
                }
#ifdef FEATURE_WLAN_WAPI
                // reset the BKID candidate list
                csrResetBKIDCandidateList( pMac, sessionId );
#endif /* FEATURE_WLAN_WAPI */
            }
            else
            {
                smsLog(pMac, LOGW, "  Roam command doesn't have a BSS desc\n");
            }
            csrScanCancelIdleScan(pMac);
            //Not to signal link up because keys are yet to be set.
            //The linkup function will overwrite the sub-state that we need to keep at this point.
            if( !CSR_IS_WAIT_FOR_KEY(pMac, sessionId) )
            {
                csrRoamLinkUp(pMac, pSession->connectedProfile.bssid);
            }
            //Check if BMPS is required and start the BMPS retry timer.  Timer period is large
            //enough to let security and DHCP handshake succeed before entry into BMPS
            if (pmcShouldBmpsTimerRun(pMac))
            {
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
            smsLog(pMac, LOG2, FL("receives start BSS ok indication\n"));
            status = eHAL_STATUS_FAILURE;
#ifdef WLAN_SOFTAP_FEATURE
            pSmeStartBssRsp = (tSirSmeStartBssRsp *)Context;
#endif             
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
            if( CSR_IS_IBSS( pProfile ) )
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED;
            }
#ifdef WLAN_SOFTAP_FEATURE
            else if (CSR_IS_INFRA_AP(pProfile))
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
            }
#endif
            else
            {
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;
            }
            if( !CSR_IS_WDS_STA( pProfile ) )
            {
                csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
#ifdef WLAN_SOFTAP_FEATURE
                pSirBssDesc = &pSmeStartBssRsp->bssDescription;
#else
                pSirBssDesc = (tSirBssDescription *)Context;
#endif
                if( !HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs( pMac, pSirBssDesc, &pIes )) )
                {
                    smsLog(pMac, LOG2, FL("cannot parse IBSS IEs\n"));
                    roamInfo.pBssDesc = pSirBssDesc;
                    //We need to associate_complete it first, becasue Associate_start already indicated.
                    csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, 
                            eCSR_ROAM_IBSS_IND, eCSR_ROAM_RESULT_IBSS_START_FAILED );
                    break;
                }
#ifdef WLAN_SOFTAP_FEATURE
                if (!CSR_IS_INFRA_AP(pProfile))
#endif
                {
                    pScanResult = csrScanAppendBssDescription( pMac, pSirBssDesc, pIes );
                }
                csrRoamSaveConnectedBssDesc(pMac, sessionId, pSirBssDesc);
                csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
                csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
                if(pSirBssDesc)
                {
                    csrRoamSaveConnectedInfomation(pMac, sessionId, pProfile, pSirBssDesc, pIes);
                    palCopyMemory(pMac->hHdd, &roamInfo.bssid, &pSirBssDesc->bssId, sizeof(tCsrBssid));
                }
                //We are doen with the IEs so free it
                palFreeMemory(pMac->hHdd, pIes);
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
                            palCopyMemory(pMac->hHdd, pIbssLog->bssid, pSirBssDesc->bssId, 6);
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
#ifdef WLAN_SOFTAP_FEATURE
                if( CSR_IS_ENC_TYPE_STATIC( pProfile->negotiatedUCEncryptionType ) && !CSR_IS_INFRA_AP( pSession->pCurRoamProfile ))
                {
#else 
                if( CSR_IS_ENC_TYPE_STATIC( pProfile->negotiatedUCEncryptionType ))
                {
#endif
                    // Issue the set Context request to LIM to establish the Broadcast STA context for the Ibss.
                    csrRoamIssueSetContextReq( pMac, sessionId, 
                                        pProfile->negotiatedMCEncryptionType, 
                                        pSirBssDesc, &BroadcastMac,
                                        FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
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
                        status = csrSendJoinReqMsg( pMac, sessionId, pSirBssDesc, pProfile, pIes );
                    }
                }
                else
                {
                    smsLog( pMac, LOGE, " StartBSS for WDS station with no BssDesc\n" );
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
#ifdef WLAN_SOFTAP_FEATURE
                if( CSR_IS_INFRA_AP( pProfile ) )
                {
                    roamStatus = eCSR_ROAM_INFRA_IND;
                    roamResult = eCSR_ROAM_RESULT_INFRA_STARTED;
                }
#endif  
                 
                //Only tell upper layer is we start the BSS because Vista doesn't like multiple connection
                //indications. If we don't start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS will 
                //trigger the connection start indication in Vista
                palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
                roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                if(CSR_INVALID_SCANRESULT_HANDLE == pCommand->u.roamCmd.hBSSList)
                {
                    //We start the IBSS (didn't find any matched IBSS out there)
                    roamInfo.pBssDesc = pSirBssDesc;
                }
#ifdef WLAN_SOFTAP_FEATURE
                roamInfo.staId = (tANI_U8)pSmeStartBssRsp->staId;
#endif
#ifndef BMPS_WORKAROUND_NOT_NEEDED
                if(  csrIsConcurrentSessionRunning( pMac ) )
                {
                   pMac->roam.configParam.doBMPSWorkaround = 1;
                }
#endif
                csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, roamStatus, roamResult );
            }
    
            csrScanCancelIdleScan(pMac);
            //Only use this timer for ibss. BAP has its own timer for WDS
            if( CSR_IS_IBSS( pProfile) && CSR_INVALID_SCANRESULT_HANDLE != pCommand->u.roamCmd.hBSSList)
            {
              //start the join IBSS timer
                csrRoamStartIbssJoinTimer(pMac, sessionId, CSR_IBSS_JOIN_TIMEOUT_PERIOD); //interval
                pSession->ibss_join_pending = TRUE;
            }
            if( HAL_STATUS_SUCCESS( status ) )
            {
                //Already sent join_req for the WDS station
                fReleaseCommand = eANI_BOOLEAN_FALSE;
            }
            else if( CSR_IS_WDS_STA( pProfile ) )
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
#ifdef WLAN_SOFTAP_FEATURE
            if( CSR_IS_INFRA_AP( pProfile ) )
            {
                roamStatus = eCSR_ROAM_INFRA_IND;
                roamResult = eCSR_ROAM_RESULT_INFRA_START_FAILED;
            }
#endif
            if(Context)
            {
                pSirBssDesc = (tSirBssDescription *)Context;
            }
            else
            {
                pSirBssDesc = NULL;
            }
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
            roamInfo.pBssDesc = pSirBssDesc;
            //We need to associate_complete it first, becasue Associate_start already indicated.
            csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, roamStatus, roamResult );
            csrSetDefaultDot11Mode( pMac );
            break;
        case eCsrSilentlyStopRoaming:
            // We are here because we try to start the same IBSS
            //No message to PE
            // return the roaming state to Joined.
            smsLog(pMac, LOGW, FL("receives silently roaming indication\n"));
            csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINED, sessionId );
            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId );
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
            roamInfo.pBssDesc = pSession->pConnectBssDesc;
            if( roamInfo.pBssDesc )
            {
                palCopyMemory(pMac->hHdd, &roamInfo.bssid, &roamInfo.pBssDesc->bssId, sizeof(tCsrBssid));
            }
            //Since there is no change in the current state, simply pass back no result otherwise
            //HDD may be mistakenly mark to disconnected state.
            csrRoamCallCallback( pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, 
                                        eCSR_ROAM_IBSS_IND, eCSR_ROAM_RESULT_NONE );
            break;
        case eCsrSilentlyStopRoamingSaveState:
            //We are here because we try to connect to the same AP
            //No message to PE
            smsLog(pMac, LOGW, FL("receives silently stop roaming indication\n"));
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(roamInfo));
            
            //to aviod resetting the substate to NONE
            pMac->roam.curState[sessionId] = eCSR_ROAMING_STATE_JOINED;
            //No need to change substate to wai_for_key because there is no state change
            roamInfo.pBssDesc = pSession->pConnectBssDesc;
            if( roamInfo.pBssDesc )
            {
                palCopyMemory(pMac->hHdd, &roamInfo.bssid, &roamInfo.pBssDesc->bssId, sizeof(tCsrBssid));
            }
            roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
            roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
            roamInfo.nBeaconLength = pSession->connectedInfo.nBeaconLength;
            roamInfo.nAssocReqLength = pSession->connectedInfo.nAssocReqLength;
            roamInfo.nAssocRspLength = pSession->connectedInfo.nAssocRspLength;
            roamInfo.pbFrames = pSession->connectedInfo.pbFrames;
            roamInfo.staId = pSession->connectedInfo.staId;
            roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
#if defined(VOSS_ENABLED)
            VOS_ASSERT( roamInfo.staId != 0 );
#endif
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
            smsLog(pMac, LOGW, FL("failed to join WDS\n"));
            csrFreeConnectBssDesc(pMac, sessionId);
            csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
            csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
            smsLog(pMac, LOGW, FL("receives no association indication\n"));
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
                    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_IDLE, sessionId );
                    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
                    roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
                    roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                    roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
                    /* Defeaturize this later if needed */
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
                    /* If Join fails while Handoff is in progress, indicate disassociated event to supplicant to reconnect */
                    if (csrRoamIsHandoffInProgress(pMac))
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
                    smsLog(pMac, LOG1, FL("  roam(reason %d) failed\n"), pCommand->u.roamCmd.roamReason);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                    sme_QosCsrEventInd(pMac, (tANI_U8)sessionId, SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
                    csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_FAILURE, eANI_BOOLEAN_FALSE);
                    csrScanStartIdleScan(pMac);
#ifdef FEATURE_WLAN_BTAMP_UT_RF
                    //For WDS STA. To fix the issue where the WDS AP side may be too busy by
                    //BT activity and not able to recevie WLAN traffic. Retry the join
                    if( CSR_IS_WDS_STA(pProfile) )
                    {
                        csrRoamStartJoinRetryTimer(pMac, sessionId, CSR_JOIN_RETRY_TIMEOUT_PERIOD);
                    }
#endif
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
#ifdef WLAN_SOFTAP_FEATURE
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
                       if (!pSession)
                           break;

                       if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                       {
                           roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                           palCopyMemory(pMac->hHdd, roamInfo.peerMac, 
                              pCommand->u.roamCmd.peerMac, sizeof(tSirMacAddr));
                           roamInfo.reasonCode = eCSR_ROAM_RESULT_FORCED;
                           roamInfo.statusCode = eSIR_SME_SUCCESS;
                           status = csrRoamCallCallback(pMac, sessionId, 
                                       &roamInfo, pCommand->u.roamCmd.roamId, 
                                       eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                       }
                   }
                   break;
#endif
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

                    //We are done with one round of lostlink roaming here
                    csrScanHandleFailedLostlink3(pMac, sessionId);
                    break;
            }
            break;
        }
    }
    return ( fReleaseCommand );
}

eHalStatus csrRoamRegisterCallback(tpAniSirGlobal pMac, csrRoamCompleteCallback callback, void *pContext)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    return (status);
}

eHalStatus csrRoamCopyProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pDstProfile, tCsrRoamProfile *pSrcProfile)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 size = 0;
    
    do
    {
        palZeroMemory(pMac->hHdd, pDstProfile, sizeof(tCsrRoamProfile));
        if(pSrcProfile->BSSIDs.numOfBSSIDs)
        {
            size = sizeof(tCsrBssid) * pSrcProfile->BSSIDs.numOfBSSIDs;
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->BSSIDs.bssid, size);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->BSSIDs.numOfBSSIDs = pSrcProfile->BSSIDs.numOfBSSIDs;
            palCopyMemory(pMac->hHdd, pDstProfile->BSSIDs.bssid, pSrcProfile->BSSIDs.bssid, size);
        }
        if(pSrcProfile->SSIDs.numOfSSIDs)
        {
            size = sizeof(tCsrSSIDInfo) * pSrcProfile->SSIDs.numOfSSIDs;
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->SSIDs.SSIDList, size);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->SSIDs.numOfSSIDs = pSrcProfile->SSIDs.numOfSSIDs;
            palCopyMemory(pMac->hHdd, pDstProfile->SSIDs.SSIDList, pSrcProfile->SSIDs.SSIDList, size);
        }
        if(pSrcProfile->nWPAReqIELength)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->pWPAReqIE, pSrcProfile->nWPAReqIELength);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nWPAReqIELength = pSrcProfile->nWPAReqIELength;
            palCopyMemory(pMac->hHdd, pDstProfile->pWPAReqIE, pSrcProfile->pWPAReqIE, pSrcProfile->nWPAReqIELength);
        }
        if(pSrcProfile->nRSNReqIELength)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->pRSNReqIE, pSrcProfile->nRSNReqIELength);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nRSNReqIELength = pSrcProfile->nRSNReqIELength;
            palCopyMemory(pMac->hHdd, pDstProfile->pRSNReqIE, pSrcProfile->pRSNReqIE, pSrcProfile->nRSNReqIELength);
        }
#ifdef FEATURE_WLAN_WAPI
        if(pSrcProfile->nWAPIReqIELength)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->pWAPIReqIE, pSrcProfile->nWAPIReqIELength);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nWAPIReqIELength = pSrcProfile->nWAPIReqIELength;
            palCopyMemory(pMac->hHdd, pDstProfile->pWAPIReqIE, pSrcProfile->pWAPIReqIE, pSrcProfile->nWAPIReqIELength);
        }
#endif /* FEATURE_WLAN_WAPI */
        if(pSrcProfile->nAddIEScanLength)
        {
            status = palAllocateMemory(pMac->hHdd,
                     (void **)&pDstProfile->pAddIEScan, pSrcProfile->nAddIEScanLength);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nAddIEScanLength = pSrcProfile->nAddIEScanLength;
            palCopyMemory(pMac->hHdd, pDstProfile->pAddIEScan, pSrcProfile->pAddIEScan,
                pSrcProfile->nAddIEScanLength);
        }
        if(pSrcProfile->nAddIEAssocLength)
        {
            status = palAllocateMemory(pMac->hHdd,
                     (void **)&pDstProfile->pAddIEAssoc, pSrcProfile->nAddIEAssocLength);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->nAddIEAssocLength = pSrcProfile->nAddIEAssocLength;
            palCopyMemory(pMac->hHdd, pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
                pSrcProfile->nAddIEAssocLength);
        }
        if(pSrcProfile->ChannelInfo.ChannelList)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->ChannelInfo.ChannelList, pSrcProfile->ChannelInfo.numOfChannels);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->ChannelInfo.numOfChannels = pSrcProfile->ChannelInfo.numOfChannels;
            palCopyMemory(pMac->hHdd, pDstProfile->ChannelInfo.ChannelList, pSrcProfile->ChannelInfo.ChannelList, pSrcProfile->ChannelInfo.numOfChannels);
        }
        pDstProfile->AuthType = pSrcProfile->AuthType;
        pDstProfile->EncryptionType = pSrcProfile->EncryptionType;
        pDstProfile->mcEncryptionType = pSrcProfile->mcEncryptionType;
        pDstProfile->negotiatedUCEncryptionType = pSrcProfile->negotiatedUCEncryptionType;
        pDstProfile->negotiatedMCEncryptionType = pSrcProfile->negotiatedMCEncryptionType;
        pDstProfile->negotiatedAuthType = pSrcProfile->negotiatedAuthType;
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
        /*Save the WPS info*/
        pDstProfile->bWPSAssociation = pSrcProfile->bWPSAssociation;
        pDstProfile->uapsd_mask = pSrcProfile->uapsd_mask;
        pDstProfile->beaconInterval = pSrcProfile->beaconInterval;
#ifdef WLAN_SOFTAP_FEATURE
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
#endif
        palCopyMemory(pMac->hHdd, &pDstProfile->Keys, &pSrcProfile->Keys, sizeof(pDstProfile->Keys));
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
eHalStatus csrRoamCopyConnectedProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pDstProfile )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCsrRoamConnectedProfile *pSrcProfile = &pMac->roam.roamSession[sessionId].connectedProfile; 
    do
    {
        palZeroMemory(pMac->hHdd, pDstProfile, sizeof(tCsrRoamProfile));
        if(pSrcProfile->bssid)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->BSSIDs.bssid, sizeof(tCsrBssid));
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->BSSIDs.numOfBSSIDs = 1;
            palCopyMemory(pMac->hHdd, pDstProfile->BSSIDs.bssid, pSrcProfile->bssid, sizeof(tCsrBssid));
        }
        if(pSrcProfile->SSID.ssId)
        {
            status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->SSIDs.SSIDList, sizeof(tCsrSSIDInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pDstProfile->SSIDs.numOfSSIDs = 1;
            pDstProfile->SSIDs.SSIDList[0].handoffPermitted = pSrcProfile->handoffPermitted;
            pDstProfile->SSIDs.SSIDList[0].ssidHidden = pSrcProfile->ssidHidden;
            palCopyMemory(pMac->hHdd, &pDstProfile->SSIDs.SSIDList[0].SSID, &pSrcProfile->SSID, sizeof(tSirMacSSid));
        }
        status = palAllocateMemory(pMac->hHdd, (void **)&pDstProfile->ChannelInfo.ChannelList, 1);
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
        palCopyMemory(pMac->hHdd, &pDstProfile->Keys, &pSrcProfile->Keys, sizeof(pDstProfile->Keys));
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
        smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
        status = eHAL_STATUS_RESOURCES;
    }
    else
    {
        if( fClearScan )
        {
            csrScanCancelIdleScan(pMac);
            csrScanAbortMacScanNotForConnect(pMac);
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
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
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
        smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
        status = eHAL_STATUS_RESOURCES;
    }
    else
    {
        csrScanCancelIdleScan(pMac);
        csrScanAbortMacScanNotForConnect(pMac);
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
        status = csrQueueSmeCommand(pMac, pCommand, fImediate);
        if( !HAL_STATUS_SUCCESS( status ) )
    {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
            csrRoamCompletion(pMac, sessionId, NULL, pCommand, eCSR_ROAM_RESULT_FAILURE, eANI_BOOLEAN_FALSE);
            csrReleaseCommandRoam( pMac, pCommand );
    }
    }
    return (status);
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
            smsLog(pMac, LOGE, FL("failed to start a join process\n"));
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
    if (NULL == pProfile)
    {
        smsLog(pMac, LOGP, FL("No profile specified"));
        return eHAL_STATUS_FAILURE;
    }
    smsLog(pMac, LOG1, FL("called  BSSType = %d authtype = %d  encryType = %d\n"),
                pProfile->BSSType, pProfile->AuthType.authType[0], pProfile->EncryptionType.encryptionType[0]);
    if( CSR_IS_WDS( pProfile ) && 
        !HAL_STATUS_SUCCESS( status = csrIsBTAMPAllowed( pMac, pProfile->operationChannel ) ) )
    {
        return status;
    }
    csrRoamCancelRoaming(pMac, sessionId);
    csrScanRemoveFreshScanCommand(pMac, sessionId);
    csrScanCancelIdleScan(pMac);
    //Only abort the scan if it is not used for other roam/connect purpose
    csrScanAbortMacScan(pMac);
#ifdef WLAN_SOFTAP_FEATURE
    if (!vos_concurrent_sessions_running() && (VOS_STA_SAP_MODE == pProfile->csrPersona))//In case of AP mode we do not want idle mode scan
    {
        csrScanDisable(pMac);
    }
#endif
    csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrHddIssued);
    //Check whether ssid changes
    if(csrIsConnStateConnected(pMac, sessionId))
    {
        if(pProfile->SSIDs.numOfSSIDs && !csrIsSsidInList(pMac, &pSession->connectedProfile.SSID, &pProfile->SSIDs))
        {
            csrRoamIssueDisassociateCmd(pMac, sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);
        }
    }
#ifdef FEATURE_WLAN_BTAMP_UT_RF
    pSession->maxRetryCount = CSR_JOIN_MAX_RETRY_COUNT; 
#endif
    if(CSR_INVALID_SCANRESULT_HANDLE != hBssListIn)
    {
        smsLog(pMac, LOGW, FL("is called with BSSList\n"));
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter, sizeof(tCsrScanResultFilter));
        if(HAL_STATUS_SUCCESS(status))
        {
            palZeroMemory(pMac->hHdd, pScanFilter, sizeof(tCsrScanResultFilter));
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
                }
                else
                {
                    pScanFilter->bWPSAssociation = 0;
                }
                do
                {
                    if( (pProfile && CSR_IS_WDS_AP( pProfile ) )
#ifdef WLAN_SOFTAP_FEATURE
                     || (pProfile && CSR_IS_INFRA_AP ( pProfile ))
#endif
                    )
                    {
                        //This can be start right away
                        status = csrRoamIssueConnect(pMac, sessionId, pProfile, NULL, eCsrHddIssued, 
                                                    roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                        if(!HAL_STATUS_SUCCESS(status))
                        {
                            fCallCallback = eANI_BOOLEAN_TRUE;
                        }
                        break;
                    }
                    status = csrScanGetResult(pMac, pScanFilter, &hBSSList);
                    smsLog(pMac, LOGE, "************ csrScanGetResult Status ********* %d\n", status);
                    if(HAL_STATUS_SUCCESS(status))
                    {
                        status = csrRoamIssueConnect(pMac, sessionId, pProfile, hBSSList, eCsrHddIssued, 
                                                    roamId, eANI_BOOLEAN_FALSE, eANI_BOOLEAN_FALSE);
                        if(!HAL_STATUS_SUCCESS(status))
                        {
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
                                smsLog(pMac, LOGE, "   CSR failed to issue startIBSS command with status = 0x%08X\n", status);
                                fCallCallback = eANI_BOOLEAN_TRUE;
                            }
                        }
                        else
                        {
                            //scan for this SSID
                            status = csrScanForSSID(pMac, sessionId, pProfile, roamId);
                            if(!HAL_STATUS_SUCCESS(status))
                            {
                                fCallCallback = eANI_BOOLEAN_TRUE;
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
            
            palFreeMemory(pMac->hHdd, pScanFilter);
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
   smsLog(pMac, LOG1, FL("called  BSSType = %d authtype = %d  encryType = %d\n"), pProfile->BSSType, pProfile->AuthType.authType[0], pProfile->EncryptionType.encryptionType[0]);
   csrRoamCancelRoaming(pMac, sessionId);
   csrScanRemoveFreshScanCommand(pMac, sessionId);
   csrScanCancelIdleScan(pMac);
   csrScanAbortMacScanNotForConnect(pMac);
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
            smsLog(pMac, LOG1, FL("Not connected to the same SSID asked in the profile\n"));
         }
      }
      else if(!palEqualMemory(pMac->hHdd, &modProfileFields, 
                              &pSession->connectedProfile.modifyProfileFields, 
                              sizeof(tCsrRoamModifyProfileFields)))
      {
         fCallCallback = eANI_BOOLEAN_FALSE;
      }
      else
      {
         smsLog(pMac, LOG1, FL("Either the profile is NULL or none of the fields "
                               "in tCsrRoamModifyProfileFields got modified\n"));
      }
   }
   else
   {
      smsLog(pMac, LOG1, FL("Not connected! No need to reassoc\n"));
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
            csrScanAbortMacScanNotForConnect(pMac);
            //We have to make a copy of pCurRoamProfile because it will be free inside csrRoamIssueConnect
            status = palAllocateMemory(pMac->hHdd, (void **)&pProfile, sizeof(tCsrRoamProfile));
            if(!HAL_STATUS_SUCCESS(status))
                break;
            palZeroMemory(pMac->hHdd, pProfile, sizeof(tCsrRoamProfile));
            status = csrRoamCopyProfile(pMac, pProfile, pSession->pCurRoamProfile);
            if(!HAL_STATUS_SUCCESS(status))
                break;
            status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter, sizeof(tCsrScanResultFilter));
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            palZeroMemory(pMac->hHdd, pScanFilter, sizeof(tCsrScanResultFilter));
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
                status = csrScanForSSID(pMac, sessionId, pProfile, roamId);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    break;
                }
            }
        }//We have a profile
        else
        {
            smsLog(pMac, LOGW, FL("cannot find a roaming profile\n"));
            break;
        }
    }while(0);
    if(pScanFilter)
    {
        csrFreeScanFilter(pMac, pScanFilter);
        palFreeMemory(pMac->hHdd, pScanFilter);
    }
    if(NULL != pProfile)
    {
        csrReleaseProfile(pMac, pProfile);
        palFreeMemory(pMac->hHdd, pProfile);
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
    smsLog(pMac, LOGW, FL("is called\n"));
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
        smsLog(pMac, LOGE, FL(" restore AC weights (%d-%d-%d-%d)\n"), pMac->roam.ucACWeights[0], pMac->roam.ucACWeights[1],
            pMac->roam.ucACWeights[2], pMac->roam.ucACWeights[3]);
        //Restore AC weight in case we change it 
        WLANTL_SetACWeights(pMac->roam.gVosContext, pMac->roam.ucACWeights);
        // in Infrasturcture, we need to disassociate from the Infrastructure network...
        NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
        if(eCsrSmeIssuedDisassocForHandoff == pCommand->u.roamCmd.roamReason)
        {
            NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF;
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
    return (status);
}

/* This is been removed from latest code base */
/*
static eHalStatus csrRoamProcessStopBss( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status;
    tANI_U32 sessionId = pCommand->sessionId;
    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING );
    status = csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ );
    return ( status );
}
*/

eHalStatus csrRoamIssueDisassociateCmd( tpAniSirGlobal pMac, tANI_U32 sessionId, eCsrRoamDisconnectReason reason )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSmeCmd *pCommand;
        tANI_BOOLEAN fHighPriority = eANI_BOOLEAN_FALSE;
    do
    {
        smsLog( pMac, LOGE, FL("  reason = %d\n"), reason );
        pCommand = csrGetCommandBuffer( pMac );
        if ( !pCommand ) 
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
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
        switch ( reason )
        {
        case eCSR_DISCONNECT_REASON_MIC_ERROR:
            pCommand->u.roamCmd.roamReason = eCsrForcedDisassocMICFailure;
            break;
        case eCSR_DISCONNECT_REASON_DEAUTH:
            pCommand->u.roamCmd.roamReason = eCsrForcedDeauth;
            break;
        case eCSR_DISCONNECT_REASON_HANDOFF:
            fHighPriority = eANI_BOOLEAN_TRUE;
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
        default:
            break;
        }
        status = csrQueueSmeCommand(pMac, pCommand, fHighPriority);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
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
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
            csrReleaseCommandRoam( pMac, pCommand );
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
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
    
#ifdef FEATURE_WLAN_BTAMP_UT_RF
    //Stop te retry
    pSession->maxRetryCount = 0;
    csrRoamStopJoinRetryTimer(pMac, sessionId);
#endif
    //Not to call cancel roaming here
    //Only issue disconnect when necessary
    if(csrIsConnStateConnected(pMac, sessionId) || csrIsBssTypeIBSS(pSession->connectedProfile.BSSType) 
                || csrIsBssTypeWDS(pSession->connectedProfile.BSSType) 
                || csrIsRoamCommandWaitingForSession(pMac, sessionId) )
                
    {
        smsLog(pMac, LOG2, FL("called\n"));
        status = csrRoamIssueDisassociateCmd(pMac, sessionId, reason);
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
    pSession->ibss_join_pending = FALSE;
    csrRoamStopIbssJoinTimer(pMac, sessionId);
    csrRoamRemoveDuplicateCommand(pMac, sessionId, NULL, eCsrForcedDisassoc);
    
    return (csrRoamDisconnectInternal(pMac, sessionId, reason));
}

eHalStatus csrRoamSaveConnectedInfomation(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                                          tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tDot11fBeaconIEs *pIesTemp = pIes;
    tANI_U8 index;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tCsrRoamConnectedProfile *pConnectProfile = &pSession->connectedProfile;

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    palZeroMemory(pMac->hHdd, &pSession->connectedProfile, sizeof(tCsrRoamConnectedProfile));
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

    palCopyMemory(pMac->hHdd, &pConnectProfile->Keys, &pProfile->Keys, sizeof(tCsrKeys));
    //Save bssid
    csrGetBssIdBssDesc(pMac, pSirBssDesc, &pConnectProfile->bssid);
#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pSirBssDesc->mdiePresent)
    {
        pConnectProfile->MDID.mdiePresent = 1;
        pConnectProfile->MDID.mobilityDomain = (pSirBssDesc->mdie[1] << 8) | (pSirBssDesc->mdie[0]);
    }
#endif
#ifdef FEATURE_WLAN_CCX
    if ((csrIsProfileCCX(pProfile) || 
         ((pIesTemp->CCXVersion.present) 
          && ((pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM) 
              || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA) 
              || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA_PSK) 
              || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN) 
              || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN_PSK)))) 
        && (!(csrIsProfile11r( pProfile ))) 
        && (pMac->roam.configParam.isCcxIniFeatureEnabled))
    {
        pConnectProfile->isCCXAssoc = 1;
    }
#endif
    //save ssid
    if( NULL == pIesTemp )
    {
        status = csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesTemp);
    }
    if(HAL_STATUS_SUCCESS(status))
    {
        if(pIesTemp->SSID.present)
        {
            pConnectProfile->SSID.length = pIesTemp->SSID.num_ssid;
            palCopyMemory(pMac->hHdd, pConnectProfile->SSID.ssId, 
                            pIesTemp->SSID.ssid, pIesTemp->SSID.num_ssid);
        }
        
        //Save the bss desc
        status = csrRoamSaveConnectedBssDesc(pMac, sessionId, pSirBssDesc);
           if( CSR_IS_QOS_BSS(pIesTemp) )
           {
              pConnectProfile->qap = TRUE;
           }
           else
           {
              pConnectProfile->qap = FALSE;
           }
        if ( NULL == pIes )
        {
            //Free memory if it allocated locally
            palFreeMemory(pMac->hHdd, pIesTemp);
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
       if((pProfile->SSIDs.SSIDList[index].SSID.length == pConnectProfile->SSID.length) &&
          palEqualMemory(pMac->hHdd, pProfile->SSIDs.SSIDList[index].SSID.ssId, 
                         pConnectProfile->SSID.ssId, pConnectProfile->SSID.length))
       {
          pConnectProfile->handoffPermitted = pProfile->SSIDs.SSIDList[index].handoffPermitted;
          break;
       }
       pConnectProfile->handoffPermitted = FALSE;
    }
    
    return (status);
}

static void csrRoamJoinRspProcessor( tpAniSirGlobal pMac, tSirSmeJoinRsp *pSmeJoinRsp )
{
   tListElem *pEntry = NULL;
   tSmeCmd *pCommand = NULL;
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
            csrRoamComplete( pMac, eCsrJoinSuccess, (void *)pSmeJoinRsp );
   }
   else
   {
        tANI_U32 roamId = 0;
        tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, pSmeJoinRsp->sessionId );
        if(!pSession)
        {
            smsLog(pMac, LOGE, FL("  session %d not found "), pSmeJoinRsp->sessionId);
            return;
        }
        
        
        //The head of the active list is the request we sent
        //Try to get back the same profile and roam again
        if(pCommand)
        {
            roamId = pCommand->u.roamCmd.roamId;
        }
        pSession->joinFailStatusCode.statusCode = pSmeJoinRsp->statusCode;
        pSession->joinFailStatusCode.reasonCode = pSmeJoinRsp->protStatusCode;
        smsLog( pMac, LOGW, "SmeJoinReq failed with statusCode= 0x%08lX [%d]\n", pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode );
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
        /* If Join fails while Handoff is in progress, indicate disassociated event to supplicant to reconnect */
        if (csrRoamIsHandoffInProgress(pMac))
        {
            csrRoamCallCallback(pMac, pSmeJoinRsp->sessionId, NULL, roamId, eCSR_ROAM_DISASSOCIATED, eCSR_ROAM_RESULT_FORCED);
            /* Should indicate neighbor roam algorithm about the connect failure here */
            csrNeighborRoamIndicateConnect(pMac, pSmeJoinRsp->sessionId, VOS_STATUS_E_FAILURE);
        }
#endif
        if (pCommand)
        {
            if(CSR_IS_WDS_STA( &pCommand->u.roamCmd.roamProfile ))
            {
              pCommand->u.roamCmd.fStopWds = eANI_BOOLEAN_TRUE;
              pSession->connectedProfile.BSSType = eCSR_BSS_TYPE_WDS_STA;
              csrRoamReissueRoamCommand(pMac);
            }
            else if( CSR_IS_WDS( &pCommand->u.roamCmd.roamProfile ) )
            {
                csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
            }
            else
            {
                csrRoam(pMac, pCommand);
            }    
        }    
        else
        {
           csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
        }
    } /*else: ( eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode ) */
}

eHalStatus csrRoamIssueJoin( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pSirBssDesc, 
                             tDot11fBeaconIEs *pIes,
                             tCsrRoamProfile *pProfile, tANI_U32 roamId )
{
    eHalStatus status;
    smsLog( pMac, LOG1, "Attempting to Join Bssid= %02x-%02x-%02x-%02x-%02x-%02x\n", 
                  pSirBssDesc->bssId[ 0 ],pSirBssDesc->bssId[ 1 ],pSirBssDesc->bssId[ 2 ],
                  pSirBssDesc->bssId[ 3 ],pSirBssDesc->bssId[ 4 ],pSirBssDesc->bssId[ 5 ] );
    
    // Set the roaming substate to 'join attempt'...
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
    // attempt to Join this BSS...
    status = csrSendJoinReqMsg( pMac, sessionId, pSirBssDesc, pProfile, pIes );
    return (status);
}

static eHalStatus csrRoamIssueReassociate( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pSirBssDesc, 
                              tDot11fBeaconIEs *pIes, tCsrRoamProfile *pProfile)
{
    csrRoamStateChange( pMac, eCSR_ROAMING_STATE_JOINING, sessionId);
    // Set the roaming substate to 'join attempt'...
    csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_REASSOC_REQ, sessionId );

    smsLog(pMac, LOGE, FL("  calling csrSendSmeReassocReqMsg\n"));
    
    // attempt to Join this BSS...
    return csrSendSmeReassocReqMsg( pMac, sessionId, pSirBssDesc, pIes, pProfile );
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
            
            if( pCommand->u.roamCmd.fStopWds )
            {
                palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
                roamInfo.pBssDesc = pCommand->u.roamCmd.pLastRoamBss;
                roamInfo.statusCode = pSession->joinFailStatusCode.statusCode;
                roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
#ifdef WLAN_SOFTAP_FEATURE              
                if (CSR_IS_WDS(&pSession->connectedProfile)){
#endif
                pSession->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;
                csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId, 
                                        eCSR_ROAM_WDS_IND, 
                                        eCSR_ROAM_RESULT_WDS_DISASSOCIATED);
#ifdef  WLAN_SOFTAP_FEATURE
                                }else if (CSR_IS_INFRA_AP(&pSession->connectedProfile)){
                                        pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
                                        csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.roamCmd.roamId,
                                                                                eCSR_ROAM_INFRA_IND,
                                                                                eCSR_ROAM_RESULT_INFRA_DISASSOCIATED);
                                }  
 
#endif                  

                if( !HAL_STATUS_SUCCESS( csrRoamIssueStopBss( pMac, sessionId, eCSR_ROAM_SUBSTATE_STOP_BSS_REQ ) ) )
                {
                    smsLog(pMac, LOGE, " Failed to reissue stop_bss command for WDS after disassociated\n");
                    csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
                }
            }
            else if(eCsrStopRoaming == csrRoamJoinNextBss(pMac, pCommand, eANI_BOOLEAN_TRUE))
            {
                smsLog(pMac, LOGW, " Failed to reissue join command after disassociated\n");
                csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
            }
        }
        else
        {
            smsLog(pMac, LOGW, "  Command is not roaming after disassociated\n");
        }
    }
    else 
    {
        smsLog(pMac, LOGE, "   Disassoc rsp cannot continue because no command is available\n");
    }
}

tANI_BOOLEAN csrIsRoamCommandWaitingForSession(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;
    tListElem *pEntry;
    tSmeCmd *pCommand = NULL;
    //alwasy lock active list before locking pending list
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
    //alwasy lock active list before locking pending list
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
    //alwasy lock active list before locking pending list
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
            smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
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
                smsLog( pMac, LOGE, FL(" Unexpected active roam command present \n") );
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
        smsLog( pMac, LOGE, FL(" fail to get command buffer as expected based on previous connect roam command\n") );
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
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
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
        smsLog(pMac, LOGW, "   CFG_CNF with active list empty\n");
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
        //the roaming is cancelled. Simply complete the command
        smsLog(pMac, LOGW, FL("  Roam command cancelled\n"));
        csrRoamComplete(pMac, eCsrNothingToJoin, NULL); 
    }
    else
    {
        if ( CCM_IS_RESULT_SUCCESS(result) )
        {
            smsLog(pMac, LOG2, "Cfg sequence complete\n");
            // Successfully set the configuration parameters for the new Bss.  Attempt to
            // join the roaming Bss.
            if(pCommand->u.roamCmd.pRoamBssEntry)
            {
                pScanResult = GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry, tCsrScanResult, Link);
                pBssDesc = &pScanResult->Result.BssDescriptor;
            }
            if ( csrIsBssTypeIBSS( pCommand->u.roamCmd.roamProfile.BSSType ) ||
                 CSR_IS_WDS( &pCommand->u.roamCmd.roamProfile ) 
#ifdef WLAN_SOFTAP_FEATURE
                  || CSR_IS_INFRA_AP(&pCommand->u.roamCmd.roamProfile) 
#endif
            )
            {
                if(!HAL_STATUS_SUCCESS(csrRoamIssueStartBss( pMac, sessionId,
                                        &pSession->bssParams, &pCommand->u.roamCmd.roamProfile, 
                                        pBssDesc, pCommand->u.roamCmd.roamId )))
                {
                    smsLog(pMac, LOGW, " CSR start BSS failed\n");
                    //We need to complete the command
                    csrRoamComplete(pMac, eCsrStartBssFailure, NULL);
                }
            }
            else
            {
                if (!pCommand->u.roamCmd.pRoamBssEntry)
                {
                    smsLog(pMac, LOGW, " pRoamBssEntry is NULL\n");
                    //We need to complete the command
                    csrRoamComplete(pMac, eCsrJoinFailure, NULL);
                    return;
                } 
                // If we are roaming TO an Infrastructure BSS...
                VOS_ASSERT(pScanResult != NULL); 
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
                         
                        /* We need to come with other way to figure out that this is because of HO in BMP
                           The below API will be only available for Android as it uses a different HO algorithm */
                        /* Reassoc request will be used only for CCX and 11r handoff whereas other legacy roaming should 
                         * use join request */
#ifdef WLAN_FEATURE_VOWIFI_11R
                        if (csrRoamIsHandoffInProgress(pMac) && 
                                                csrRoamIs11rAssoc(pMac))
                        {
                            status = csrRoamIssueReassociate(pMac, sessionId, pBssDesc, 
                                    (tDot11fBeaconIEs *)( pScanResult->Result.pvIes ), &pCommand->u.roamCmd.roamProfile);
                        }
                        else
#endif
#ifdef FEATURE_WLAN_CCX
                        if (csrRoamIsHandoffInProgress(pMac) && 
                                                csrRoamIsCCXAssoc(pMac))
                        {
                            // Now serialize the reassoc command.
                            status = csrRoamIssueReassociateCmd(pMac, sessionId);
                        }
                        else
#endif
#ifdef FEATURE_WLAN_LFR
                        if (csrRoamIsHandoffInProgress(pMac) && 
                                                csrRoamIsFastRoamEnabled(pMac))
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
                            palFreeMemory(pMac->hHdd, pIesLocal);
                        }
                    }
                }//if ( csrIsInfraBssDesc( pBssDesc ) )
                else
                {
                    smsLog(pMac, LOGW, FL("  found BSSType mismatching the one in BSS description\n"));
                }
            }//else
        }//if ( WNI_CFG_SUCCESS == result )
        else
        {
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

static void csrRoamRoamingStateAuthRspProcessor( tpAniSirGlobal pMac, tSirSmeAuthRsp *pSmeAuthRsp )
{
    //No one is sending eWNI_SME_AUTH_REQ to PE.
    smsLog(pMac, LOGW, FL("is no-op\n"));
    if ( eSIR_SME_SUCCESS == pSmeAuthRsp->statusCode ) 
    {
        smsLog( pMac, LOGW, "CSR SmeAuthReq Successful\n" );
        // Successfully authenticated with a new Bss.  Attempt to stop the current Bss and
        // join the new one...
        /***pBssDesc = profGetRoamingBssDesc( pAdapter, &pHddProfile );
        roamStopNetwork( pAdapter, &pBssDesc->SirBssDescription );***/
    }
    else {
        smsLog( pMac, LOGW, "CSR SmeAuthReq failed with statusCode= 0x%08lX [%d]\n", pSmeAuthRsp->statusCode, pSmeAuthRsp->statusCode );
        /***profHandleLostLinkAfterReset(pAdapter);
        // In the event the authenticate fails, let the roam processor attempt to join something else...
        roamRoam( pAdapter );***/
    }
}

static void csrRoamRoamingStateReassocRspProcessor( tpAniSirGlobal pMac, tpSirSmeJoinRsp pSmeJoinRsp )
{
    eCsrRoamCompleteResult result;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tCsrRoamInfo roamInfo;
    tANI_U32 roamId = 0;
    
    if ( eSIR_SME_SUCCESS == pSmeJoinRsp->statusCode ) 
    {
        smsLog( pMac, LOGW, "CSR SmeReassocReq Successful\n" );
        result = eCsrReassocSuccess;
        /* Defeaturize this part later if needed */
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
        /* Since the neighbor roam algorithm uses reassoc req for handoff instead of join, 
         * we need the response contents while processing the result in csrRoamProcessResults() */
        if (csrRoamIsHandoffInProgress(pMac))
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
        smsLog( pMac, LOGW, "CSR SmeReassocReq failed with statusCode= 0x%08lX [%d]\n", pSmeJoinRsp->statusCode, pSmeJoinRsp->statusCode );
        result = eCsrReassocFailure;
#ifdef WLAN_FEATURE_VOWIFI_11R
        if ((eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE == pSmeJoinRsp->statusCode) ||
                        (eSIR_SME_FT_REASSOC_FAILURE == pSmeJoinRsp->statusCode))
        {
                // Inform HDD to turn off FT flag in HDD 
                if (pNeighborRoamInfo)
                {
                        vos_mem_zero(&roamInfo, sizeof(tCsrRoamInfo));
                        csrRoamCallCallback(pMac, pNeighborRoamInfo->csrSessionId,
                                        &roamInfo, roamId, eCSR_ROAM_FT_REASSOC_FAILED, eSIR_SME_SUCCESS);
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

    tSirSmeDisassocRsp SmeDisassocRsp;

    csrSerDesUnpackDiassocRsp((tANI_U8 *)pSmeRsp, &SmeDisassocRsp);
    sessionId = SmeDisassocRsp.sessionId;
    statusCode = SmeDisassocRsp.statusCode;

    smsLog( pMac, LOG2, "csrRoamRoamingStateDisassocRspProcessor sessionId %d\n", sessionId);

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
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED( pMac, sessionId ) ||
              CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ( pMac, sessionId ) )
    {
        if ( eSIR_SME_SUCCESS == statusCode )
        {
            smsLog( pMac, LOG2, "CSR SmeDisassocReq force disassociated Successfully\n" );
            //A callback to HDD will be issued from csrRoamComplete so no need to do anything here
        } 
        csrRoamComplete( pMac, eCsrNothingToJoin, NULL );
    }
    else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( pMac, sessionId ) )
    {
       smsLog( pMac, LOGE, "CSR SmeDisassocReq due to HO\n" );
#if   defined (WLAN_FEATURE_NEIGHBOR_ROAMING)
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
                fRemoveCmd = csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK );
                if(pCommand->u.roamCmd.fReleaseProfile)
                {
                    csrReleaseProfile(pMac, &pCommand->u.roamCmd.roamProfile);
                    pCommand->u.roamCmd.fReleaseProfile = eANI_BOOLEAN_FALSE;
                }
                if( fRemoveCmd )
                {
                    csrReleaseCommandRoam( pMac, pCommand );
                }
                else
                {
                    smsLog( pMac, LOGE, "  ********csrRoamRoamingStateDisassocRspProcessor fail to remove cmd reason %d\n",
                        pCommand->u.roamCmd.roamReason );
                }
            }
            else
            {
                smsLog( pMac, LOGW, "CSR: Roam Completion called but ROAM command is not ACTIVE ...\n" );
            }
        }
        else
        {
            smsLog( pMac, LOGW, "CSR: Roam Completion called but NO commands are ACTIVE ...\n" );
        }
        //notify HDD for handoff, providing the BSSID too
        roamInfo.reasonCode = eCsrRoamReasonBetterAP;

        palCopyMemory(pMac->hHdd, roamInfo.bssid, pMac->roam.neighborRoamInfo.csrNeighborRoamProfile.BSSIDs.bssid, sizeof(tSirMacAddr));

            csrRoamCallCallback(pMac,sessionId, &roamInfo, 0, eCSR_ROAM_ROAMING_START, eCSR_ROAM_RESULT_NONE);

        status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter, sizeof(tCsrScanResultFilter));
        if(HAL_STATUS_SUCCESS(status))
        {
            palZeroMemory(pMac->hHdd, pScanFilter, sizeof(tCsrScanResultFilter));
            status = csrRoamPrepareFilterFromProfile(pMac, &pMac->roam.neighborRoamInfo.csrNeighborRoamProfile, pScanFilter);
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE, FL(" csrRoamPrepareFilterFromProfile fail to create scan filter\n"));
            }
        
            status = csrScanGetResult(pMac, pScanFilter, &hBSSList);
            if(HAL_STATUS_SUCCESS(status))
            {
                //copy over the connected profile to apply the same for this connection as well
                if(HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, (void **)&pCurRoamProfile, sizeof(tCsrRoamProfile))))
                {
                    palZeroMemory(pMac->hHdd, pCurRoamProfile, sizeof(tCsrRoamProfile));
                    csrRoamCopyProfile(pMac, pCurRoamProfile, pSession->pCurRoamProfile);
                }
                //make sure to put it at the head of the cmd queue
                status = csrRoamIssueConnect(pMac, sessionId, pCurRoamProfile, 
                                             hBSSList, eCsrSmeIssuedAssocToSimilarAP, 
                                             roamId, eANI_BOOLEAN_TRUE, eANI_BOOLEAN_FALSE);
                if(!HAL_STATUS_SUCCESS(status))
                {
                    //msg
                    fCallCallback = eANI_BOOLEAN_TRUE;
                }
                /* Notify sub-modules like QoS etc. that handoff happening         */
                sme_QosCsrEventInd(pMac, sessionId, SME_QOS_CSR_HANDOFF_ASSOC_REQ, NULL);
                palFreeMemory(pMac->hHdd, pCurRoamProfile);
            }
            else
            {
                //msg
                smsLog( pMac, LOGE,"csrRoamRoamingStateDisassocRspProcessor: csrScanGetResult failed");
                // should have asserted, sending up roam complete instead. Let upper layer
                // decide what to do next
                csrCallRoamingCompletionCallback(pMac, pSession, &roamInfo, 0, eCSR_ROAM_RESULT_FAILURE);
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL(" fail to allocate memory for scan filter\n"));
            csrCallRoamingCompletionCallback(pMac, pSession, &roamInfo, 0, eCSR_ROAM_RESULT_FAILURE);
        }
        if( pScanFilter )
        {

            csrFreeScanFilter(pMac, pScanFilter);
            palFreeMemory( pMac->hHdd, pScanFilter );
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
            // We get Disassociate response in three conditions.
            // - First is the case where we are disasociating from an Infra Bss to start an IBSS.
            // - Second is the when we are disassociating from an Infra Bss to join an IBSS or a new
            // Infrastructure network.
            // - Third is where we are doing an Infra to Infra roam between networks with different
            // SSIDs.  In all cases, we set the new Bss configuration here and attempt to join
            
            smsLog( pMac, LOG2, "CSR SmeDisassocReq disassociated Successfully\n" );
        }
        else
        {
            smsLog( pMac, LOGE, "SmeDisassocReq failed with statusCode= 0x%08lX\n", statusCode );
        }
        //We are not done yet. Get the data and continue roaming
        csrRoamReissueRoamCommand(pMac);
    }
}

static void csrRoamRoamingStateDeauthRspProcessor( tpAniSirGlobal pMac, tSirSmeDeauthRsp *pSmeRsp )
{
    tSirResultCodes statusCode;
    //No one is sending eWNI_SME_DEAUTH_REQ to PE.
    smsLog(pMac, LOGW, FL("is no-op\n"));
    statusCode = csrGetDeAuthRspStatusCode( pSmeRsp );
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
            smsLog( pMac, LOG2, "CSR SmeDeauthReq disassociated Successfully\n" );
        }
        else
        {
            smsLog( pMac, LOGW, "SmeDeauthReq failed with statusCode= 0x%08lX\n", statusCode );
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
        smsLog( pMac, LOGW, "SmeStartBssReq Successful\n" );
        result = eCsrStartBssSuccess;
    }
    else 
    {
        smsLog( pMac, LOGW, "SmeStartBssReq failed with statusCode= 0x%08lX\n", pSmeStartBssRsp->statusCode );
        //Let csrRoamComplete decide what to do
        result = eCsrStartBssFailure;
    }
#ifdef WLAN_SOFTAP_FEATURE
    csrRoamComplete( pMac, result, pSmeStartBssRsp);
#else
    csrRoamComplete( pMac, result, &pSmeStartBssRsp->bssDescription );
#endif
}

/*
  We need to be careful on whether to cast pMsgBuf (pSmeRsp) to other type of strucutres.
  It depends on how the message is constructed. If the message is sent by limSendSmeRsp,
  the pMsgBuf is only a generic response and can only be used as pointer to tSirSmeRsp.
  For the messages where sender allocates memory for specific structures, then it can be 
  cast accordingly.
*/
void csrRoamingStateMsgProcessor( tpAniSirGlobal pMac, void *pMsgBuf )
{
    tSirSmeRsp *pSmeRsp;
    tSmeIbssPeerInd *pIbssPeerInd;
    tCsrRoamInfo roamInfo;
        // TODO Session Id need to be acquired in this function
        tANI_U32 sessionId = 0;
    pSmeRsp = (tSirSmeRsp *)pMsgBuf;
    smsLog( pMac, LOG2, "Message %d[0x%04X] received in substate %d\n",
                pSmeRsp->messageType, pSmeRsp->messageType,
                pMac->roam.curSubState[pSmeRsp->sessionId] );
#if defined ANI_PRODUCT_TYPE_AP
    pSmeRsp->messageType = pal_be16_to_cpu(pSmeRsp->messageType);
    pSmeRsp->length = pal_be16_to_cpu(pSmeRsp->length);
    pSmeRsp->statusCode = pal_be32_to_cpu(pSmeRsp->statusCode);
#else
    pSmeRsp->messageType = (pSmeRsp->messageType);
    pSmeRsp->length = (pSmeRsp->length);
    pSmeRsp->statusCode = (pSmeRsp->statusCode);
#endif
    switch (pSmeRsp->messageType) 
    {
        
        case eWNI_SME_JOIN_RSP:      // in Roaming state, process the Join response message...
            if (CSR_IS_ROAM_SUBSTATE_JOIN_REQ(pMac, pSmeRsp->sessionId))
            {
                //We sent a JOIN_REQ
                csrRoamJoinRspProcessor( pMac, (tSirSmeJoinRsp *)pSmeRsp );
            }
            break;
                
        case eWNI_SME_AUTH_RSP:       // or the Authenticate response message...
            if (CSR_IS_ROAM_SUBSTATE_AUTH_REQ( pMac, pSmeRsp->sessionId) ) 
            {
                //We sent a AUTH_REQ
                csrRoamRoamingStateAuthRspProcessor( pMac, (tSirSmeAuthRsp *)pSmeRsp );
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
                smsLog(pMac, LOGE, FL("eWNI_SME_DISASSOC_RSP subState = %d\n"), pMac->roam.curSubState[pSmeRsp->sessionId]);
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
        //In case CSR issues STOP_BSS, we need to tell HDD about peer departed becasue PE is removing them
        case eWNI_SME_IBSS_PEER_DEPARTED_IND:
            pIbssPeerInd = (tSmeIbssPeerInd*)pSmeRsp;
            smsLog(pMac, LOGE, "CSR: Peer departed notification from LIM in joining state\n");
            palZeroMemory( pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo) );
                        roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
            roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
            roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
            palCopyMemory(pMac->hHdd, &roamInfo.peerMac, pIbssPeerInd->peerAddr, sizeof(tCsrBssid));
            csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, 
                                eCSR_ROAM_CONNECT_STATUS_UPDATE, 
                                eCSR_ROAM_RESULT_IBSS_PEER_DEPARTED);
            break;
        default:
            smsLog( pMac, LOG1, "Unexpected message type = %d[0x%X] received in substate %d\n",
                      pSmeRsp->messageType, pSmeRsp->messageType,
                      pMac->roam.curSubState[pSmeRsp->sessionId] );

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
          smsLog( pMac, LOGW, FL("Stats rsp from PE\n"));
          csrRoamStatsRspProcessor( pMac, pSirMsg );
          break;
#ifdef WLAN_SOFTAP_FEATURE
        case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
        {
            tCsrRoamSession  *pSession;
            tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
            tCsrRoamInfo roamInfo;
            tCsrRoamInfo *pRoamInfo = NULL;
            tANI_U32 sessionId;
            eHalStatus status;
            smsLog( pMac, LOG1, FL("ASSOCIATION confirmation can be given to upper layer \n"));
            palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
            pRoamInfo->addIELen = (tANI_U8)pUpperLayerAssocCnf->addIE.length;
            pRoamInfo->paddIE = pUpperLayerAssocCnf->addIE.addIEdata;           
            palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pUpperLayerAssocCnf->peerMacAddr, sizeof(tSirMacAddr));
            palCopyMemory(pMac->hHdd, &pRoamInfo->bssid, pUpperLayerAssocCnf->bssId, sizeof(tCsrBssid));
            pRoamInfo->wmmEnabledSta = pUpperLayerAssocCnf->wmmEnabledSta;
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
#endif
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
    
    // Allow 0 keys to be set for the non-WPA encrypt types...  For WPA encrypt types, the num keys must be non-zero
    // or LIM will reject the set context (assumes the SET_CONTEXT does not occur until the keys are distrubuted).
    if ( CSR_IS_ENC_TYPE_STATIC( EncryptType ) ||
           addKey )     
    {
        tCsrRoamSetKey setKey;
        setKey.encType = EncryptType;
        setKey.keyDirection = aniKeyDirection;    //Tx, Rx or Tx-and-Rx
        palCopyMemory( pMac->hHdd, &setKey.peerMac, bssId, sizeof(tCsrBssid) );   
        setKey.paeRole = paeRole;      //0 for supplicant
        setKey.keyId = keyId;  // Kye index
        setKey.keyLength = keyLength;  
        if( keyLength )
        {
            palCopyMemory( pMac->hHdd, setKey.Key, pKey, keyLength );
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
#ifdef FEATURE_WLAN_CCX
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
#endif /* FEATURE_WLAN_CCX */
 
    do
    {
        pCommand = csrGetCommandBuffer(pMac);
        if(NULL == pCommand)
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
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
                    smsLog( pMac, LOGW, "Invalid WEP40 keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                    break;        
                }
                
                pCommand->u.setKeyCmd.keyLength = CSR_WEP40_KEY_LEN;
                palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_WEP40_KEY_LEN );
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
                    smsLog( pMac, LOGW, "Invalid WEP104 keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                    break;        
                }
                
                pCommand->u.setKeyCmd.keyLength = CSR_WEP104_KEY_LEN;
                palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_WEP104_KEY_LEN );
            }
        }
        else if ( eCSR_ENCRYPT_TYPE_TKIP == pSetKey->encType ) 
        {
            if ( pSetKey->keyLength < CSR_TKIP_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid TKIP keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_TKIP_KEY_LEN;
            palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_TKIP_KEY_LEN );
        }
        else if ( eCSR_ENCRYPT_TYPE_AES == pSetKey->encType ) 
        {
            if ( pSetKey->keyLength < CSR_AES_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid AES/CCMP keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_AES_KEY_LEN;
            palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_AES_KEY_LEN );
        }
#ifdef FEATURE_WLAN_WAPI
        else if ( eCSR_ENCRYPT_TYPE_WPI == pSetKey->encType ) 
        {
            if ( pSetKey->keyLength < CSR_WAPI_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid WAPI keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_WAPI_KEY_LEN;
            palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_WAPI_KEY_LEN );
        }
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_CCX
        else if ( eCSR_ENCRYPT_TYPE_KRK == pSetKey->encType ) 
        {
            if ( pSetKey->keyLength < CSR_KRK_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid KRK keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                break;
            }
            palCopyMemory( pMac->hHdd, pSession->ccxCckmInfo.krk, pSetKey->Key, CSR_KRK_KEY_LEN );
            pSession->ccxCckmInfo.reassoc_req_num=1;
            pSession->ccxCckmInfo.krk_plumbed = eANI_BOOLEAN_TRUE;
            status = eHAL_STATUS_SUCCESS;
            break;
        }
#endif /* FEATURE_WLAN_CCX */

#ifdef WLAN_FEATURE_11W
        //Check for 11w BIP
        else if ( eCSR_ENCRYPT_TYPE_AES_CMAC == pSetKey->encType )
        {
            tANI_U16 count = 0;
            if ( pSetKey->keyLength < CSR_AES_KEY_LEN )
            {
                smsLog( pMac, LOGW, "Invalid AES/CCMP keylength [= %d] in SetContext call\n", pSetKey->keyLength );
                break;
            }
            pCommand->u.setKeyCmd.keyLength = CSR_AES_KEY_LEN;
            palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.Key, pSetKey->Key, CSR_AES_KEY_LEN );
        }
#endif
        status = eHAL_STATUS_SUCCESS;
        pCommand->u.setKeyCmd.roamId = roamId;
        pCommand->u.setKeyCmd.encType = pSetKey->encType;
        pCommand->u.setKeyCmd.keyDirection = pSetKey->keyDirection;    //Tx, Rx or Tx-and-Rx
        palCopyMemory( pMac->hHdd, &pCommand->u.setKeyCmd.peerMac, &pSetKey->peerMac, sizeof(tCsrBssid) );   
        pCommand->u.setKeyCmd.paeRole = pSetKey->paeRole;      //0 for supplicant
        pCommand->u.setKeyCmd.keyId = pSetKey->keyId;
        palCopyMemory( pMac->hHdd, pCommand->u.setKeyCmd.keyRsc, pSetKey->keyRsc, CSR_MAX_RSC_LEN );
        //Always put set key to the head of the Q because it is the only thing to get executed in case of WT_KEY state
         
        status = csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_TRUE);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
        }
    } while (0);
    // Free the command if there has been a failure, or it is a 
    // "local" operation like the set CCX CCKM KRK key.
    if( (!HAL_STATUS_SUCCESS( status ) && ( NULL != pCommand )) 
#ifdef FEATURE_WLAN_CCX
            || ( eCSR_ENCRYPT_TYPE_KRK == pSetKey->encType ) 
#endif /* FEATURE_WLAN_CCX */
            )
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
            smsLog( pMac, LOGW, FL(" wrong state not allowed to set key\n") );
            status = eHAL_STATUS_CSR_WRONG_STATE;
            break;
        }
        pCommand = csrGetCommandBuffer(pMac);
        if(NULL == pCommand)
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
            status = eHAL_STATUS_RESOURCES;
            break;
        }
        pCommand->command = eSmeCommandRemoveKey;
        pCommand->sessionId = (tANI_U8)sessionId;
        pCommand->u.removeKeyCmd.roamId = roamId;
        pCommand->u.removeKeyCmd.encType = pRemoveKey->encType;
        palCopyMemory( pMac->hHdd, &pCommand->u.removeKeyCmd.peerMac, &pRemoveKey->peerMac, sizeof(tSirMacAddr) );
        pCommand->u.removeKeyCmd.keyId = pRemoveKey->keyId;
        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId ) )
        {
            //in this case, put it to the end of the Q incase there is a set key pending.
            fImediate = eANI_BOOLEAN_FALSE;
        }
        smsLog( pMac, LOGE, FL("keyType=%d, keyId=%d, PeerMac=%02x, %02x, %02x, %02x, %02x, %02x\n"),       
            pRemoveKey->encType, pRemoveKey->keyId,
            pCommand->u.removeKeyCmd.peerMac[0],
            pCommand->u.removeKeyCmd.peerMac[1],
            pCommand->u.removeKeyCmd.peerMac[2], 
            pCommand->u.removeKeyCmd.peerMac[3], 
            pCommand->u.removeKeyCmd.peerMac[4],
            pCommand->u.removeKeyCmd.peerMac[5]);
        status = csrQueueSmeCommand(pMac, pCommand, fImediate);
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
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
    if(eCSR_ENCRYPT_TYPE_NONE != edType)
    {
        palZeroMemory(pMac->hHdd, &setKeyEvent, sizeof(vos_event_wlan_security_payload_type));
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
        palCopyMemory( pMac->hHdd, setKeyEvent.bssid, pSession->connectedProfile.bssid, 6 );
        if(CSR_IS_ENC_TYPE_STATIC(edType))
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
        smsLog( pMac, LOGW, FL(" cannot process not connected\n") );
        //Set this status so the error handling take care of the case.
        status = eHAL_STATUS_CSR_WRONG_STATE;
    }
    if( !HAL_STATUS_SUCCESS(status) )
    {
        smsLog( pMac, LOGE, FL("  error status %d\n"), status );
        csrRoamCallCallback( pMac, sessionId, NULL, pCommand->u.setKeyCmd.roamId, eCSR_ROAM_SET_KEY_COMPLETE, eCSR_ROAM_RESULT_FAILURE);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        if(eCSR_ENCRYPT_TYPE_NONE != edType)
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
    palZeroMemory(pMac->hHdd, &removeKeyEvent, sizeof(vos_event_wlan_security_payload_type));
    removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_REQ;
    removeKeyEvent.encryptionModeMulticast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
    removeKeyEvent.encryptionModeUnicast = (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
    palCopyMemory( pMac->hHdd, removeKeyEvent.bssid, pSession->connectedProfile.bssid, 6 );
    removeKeyEvent.keyId = pCommand->u.removeKeyCmd.keyId;
    removeKeyEvent.authMode = (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
    WLAN_VOS_DIAG_EVENT_REPORT(&removeKeyEvent, EVENT_WLAN_SECURITY);
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
    if( csrIsSetKeyAllowed(pMac, sessionId) )
    {
        status = palAllocateMemory( pMac->hHdd, (void **)&pMsg, wMsgLen );
    }
    else
    {
        smsLog( pMac, LOGW, FL(" wrong state not allowed to set key\n") );
        //Set the error status so error handling kicks in below
        status = eHAL_STATUS_CSR_WRONG_STATE;
    }
    if( HAL_STATUS_SUCCESS( status ) )
    {
        palZeroMemory(pMac->hHdd, pMsg, wMsgLen);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_REMOVEKEY_REQ);
                pMsg->length = pal_cpu_to_be16(wMsgLen);
        pMsg->sessionId = (tANI_U8)sessionId;
        pMsg->transactionId = 0;
        p = (tANI_U8 *)pMsg + sizeof(pMsg->messageType) + sizeof(pMsg->length) +
            sizeof(pMsg->sessionId) + sizeof(pMsg->transactionId);
        // bssId - copy from session Info
        palCopyMemory( pMac->hHdd, p, &pMac->roam.roamSession[sessionId].connectedProfile.bssid, sizeof(tSirMacAddr) );
        p += sizeof(tSirMacAddr);
        // peerMacAddr
        palCopyMemory( pMac->hHdd, p, pCommand->u.removeKeyCmd.peerMac, sizeof(tSirMacAddr) );
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
        smsLog( pMac, LOGE, FL(" error status \n"), status );
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_RSP;
        removeKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;;
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
    tANI_U32 size = 0;
    tANI_U8  index = 0;
    
    do
    {
        if(pProfile->BSSIDs.numOfBSSIDs)
        {
            size = sizeof(tCsrBssid) * pProfile->BSSIDs.numOfBSSIDs;
            status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter->BSSIDs.bssid, size);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            pScanFilter->BSSIDs.numOfBSSIDs = pProfile->BSSIDs.numOfBSSIDs;
            palCopyMemory(pMac->hHdd, pScanFilter->BSSIDs.bssid, pProfile->BSSIDs.bssid, size);
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
            size = sizeof(tCsrSSIDInfo) * pProfile->SSIDs.numOfSSIDs;
            status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter->SSIDs.SSIDList, size);
            if(!HAL_STATUS_SUCCESS(status))
            {
                break;
            }
            palCopyMemory(pMac->hHdd, pScanFilter->SSIDs.SSIDList, pProfile->SSIDs.SSIDList, size);
        }
        if(!pProfile->ChannelInfo.ChannelList || (pProfile->ChannelInfo.ChannelList[0] == 0) )
        {
            pScanFilter->ChannelInfo.numOfChannels = 0;
            pScanFilter->ChannelInfo.ChannelList = NULL;
        }
        else if(pProfile->ChannelInfo.numOfChannels)
        {
           status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter->ChannelInfo.ChannelList, sizeof(*pScanFilter->ChannelInfo.ChannelList) * pProfile->ChannelInfo.numOfChannels);
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
                         smsLog(pMac, LOG1, FL("process a channel (%d) that is invalid\n"), pProfile->ChannelInfo.ChannelList[index]);
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
            smsLog(pMac, LOGW, FL("Channel list empty\n"));
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
        if( pProfile->countryCode[0] )
        {
            //This causes the matching function to use countryCode as one of the criteria.
            palCopyMemory( pMac->hHdd, pScanFilter->countryCode, pProfile->countryCode, 
                        WNI_CFG_COUNTRY_CODE_LEN );
        }
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pProfile->MDID.mdiePresent)
        {
            pScanFilter->MDID.mdiePresent = 1;
            pScanFilter->MDID.mobilityDomain = pProfile->MDID.mobilityDomain;
        }
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
            smsLog( pMac, LOGE, FL(" fail to get command buffer\n") );
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
            palCopyMemory( pMac->hHdd, &pCommand->u.wmStatusChangeCmd.u.DisassocIndMsg, pSmeRsp, 
                                sizeof( pCommand->u.wmStatusChangeCmd.u.DisassocIndMsg ) );
        }
        else
        {
            palCopyMemory( pMac->hHdd, &pCommand->u.wmStatusChangeCmd.u.DeauthIndMsg, pSmeRsp, 
                            sizeof( pCommand->u.wmStatusChangeCmd.u.DeauthIndMsg ) );
        }
        if( HAL_STATUS_SUCCESS( csrQueueSmeCommand(pMac, pCommand, eANI_BOOLEAN_TRUE) ) )
        {
            fCommandQueued = eANI_BOOLEAN_TRUE;
        }
        else
        {
            smsLog( pMac, LOGE, FL(" fail to send message \n") );
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
    if(pGetRssiReq)
    {
        if(NULL != pGetRssiReq->pVosContext)
        {
            WLANTL_GetRssi(pGetRssiReq->pVosContext, pGetRssiReq->staId, &rssi);
        }
        else
        {
            smsLog( pMac, LOGE, FL("pGetRssiReq->pVosContext is NULL\n"));                
            return;
        }
            
        if(NULL != pGetRssiReq->rssiCallback)
        {
            ((tCsrRssiCallback)(pGetRssiReq->rssiCallback))(rssi, pGetRssiReq->staId, pGetRssiReq->pDevContext);
        }
        else
        {
            smsLog( pMac, LOGE, FL("pGetRssiReq->rssiCallback is NULL\n"));                
            return;
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL("pGetRssiReq is NULL\n"));    
    }
    return;
}
static void csrRoamRssiIndHdlr(tpAniSirGlobal pMac, void* pMsg)
{
    WLANTL_TlIndicationReq *pTlRssiInd = (WLANTL_TlIndicationReq*)pMsg;
    if(pTlRssiInd)
    {
        if(NULL != pTlRssiInd->tlCallback)
        {
            ((WLANTL_RSSICrossThresholdCBType)(pTlRssiInd->tlCallback))
            (pTlRssiInd->pAdapter, pTlRssiInd->rssiNotification, pTlRssiInd->pUserCtxt);
        }
        else
        {
            smsLog( pMac, LOGE, FL("pTlRssiInd->tlCallback is NULL\n"));                
        }
    }
    else
    {
        smsLog( pMac, LOGE, FL("pTlRssiInd is NULL\n"));    
    }
    return;
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
#if defined ANI_PRODUCT_TYPE_AP
    pSirMsg->messageType = pal_be16_to_cpu(pSirMsg->messageType);
    pSirMsg->length = pal_be16_to_cpu(pSirMsg->length);
    pSirMsg->statusCode = pal_be32_to_cpu(pSirMsg->statusCode);
#else
    pSirMsg->messageType = (pSirMsg->messageType);
    pSirMsg->length = (pSirMsg->length);
    pSirMsg->statusCode = (pSirMsg->statusCode);
#endif
    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(roamInfo));
    switch( pSirMsg->messageType ) 
    {
        case eWNI_SME_ASSOC_IND:
            {
                tCsrRoamSession  *pSession;
                smsLog( pMac, LOG1, FL("ASSOCIATION Indication from SME\n"));
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
                
                pRoamInfo->addIELen = (tANI_U8)pAssocInd->addIE.length;
                pRoamInfo->paddIE =  pAssocInd->addIE.addIEdata;
                    palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pAssocInd->peerMacAddr, sizeof(tSirMacAddr));
                    palCopyMemory(pMac->hHdd, &pRoamInfo->bssid, pAssocInd->bssId, sizeof(tCsrBssid));
#ifdef WLAN_SOFTAP_FEATURE
                    pRoamInfo->wmmEnabledSta = pAssocInd->wmmEnabledSta;
                    if(CSR_IS_WDS_AP( pRoamInfo->u.pConnectedProfile))
#endif 
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_WDS_IND, eCSR_ROAM_RESULT_WDS_ASSOCIATION_IND);//Sta
#ifdef WLAN_SOFTAP_FEATURE
                    if(CSR_IS_INFRA_AP(pRoamInfo->u.pConnectedProfile))
                    {
                        if( CSR_IS_ENC_TYPE_STATIC( pSession->pCurRoamProfile->negotiatedUCEncryptionType ))
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
#endif 
                    /* Send Association completion message to PE */
                    status = csrSendAssocCnfMsg( pMac, pAssocInd, status );//Sta
                    
                    /* send a message to CSR itself just to avoid the EAPOL frames going
                     * OTA before association response */
                    if(CSR_IS_WDS_AP( pRoamInfo->u.pConnectedProfile))
                {
                    status = csrSendAssocIndToUpperLayerCnfMsg(pMac, pAssocInd, status, sessionId);
                }
#ifdef WLAN_SOFTAP_FEATURE
                else if(CSR_IS_INFRA_AP(pRoamInfo->u.pConnectedProfile) && (pRoamInfo->statusCode != eSIR_SME_ASSOC_REFUSED))
                {
                    pRoamInfo->fReassocReq = pAssocInd->reassocReq;
                    //status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
                    status = csrSendAssocIndToUpperLayerCnfMsg(pMac, pAssocInd, status, sessionId);
                }
#endif
                }
            }
            break;
        case eWNI_SME_DISASSOC_IND:
            smsLog( pMac, LOGE, FL("DISASSOCIATION Indication from MAC\n"));
            // Check if AP dis-associated us because of MIC failure. If so,
            // then we need to take action immediately and not wait till the
            // the WmStatusChange requests is pushed and processed
            pDisassocInd = (tSirSmeDisassocInd *)pSirMsg;
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pDisassocInd->bssId, &sessionId );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                // If we are in neighbor preauth done state then on receiving
                // disassoc or deauth we dont roam instead we just disassoc
                // from current ap and then go to disconnected state 
                // This happens for CCX and 11r FT connections ONLY.
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (csrRoamIs11rAssoc(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
                }
#endif
#ifdef FEATURE_WLAN_CCX
                if (csrRoamIsCCXAssoc(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
                }
#endif
#ifdef FEATURE_WLAN_LFR
                if (csrRoamIsFastRoamEnabled(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
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
#ifdef WLAN_SOFTAP_FEATURE
                if(CSR_IS_INFRA_AP(&pSession->connectedProfile))
                {

                    pRoamInfo = &roamInfo;

                    pRoamInfo->statusCode = pDisassocInd->statusCode; 
                    pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

                    pRoamInfo->staId = (tANI_U8)pDisassocInd->staId;

                    palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pDisassocInd->peerMacAddr, sizeof(tSirMacAddr));
                    palCopyMemory(pMac->hHdd, &pRoamInfo->bssid, pDisassocInd->bssId, sizeof(tCsrBssid));

                    status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_DISASSOC_IND);
                }
#endif                
            }
            break;
        case eWNI_SME_DEAUTH_IND:
            smsLog( pMac, LOG1, FL("DEAUTHENTICATION Indication from MAC\n"));
            pDeauthInd = (tpSirSmeDeauthInd)pSirMsg;
            status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pDeauthInd->bssId, &sessionId );
            if( HAL_STATUS_SUCCESS( status ) )
            {
                // If we are in neighbor preauth done state then on receiving
                // disassoc or deauth we dont roam instead we just disassoc
                // from current ap and then go to disconnected state 
                // This happens for CCX and 11r FT connections ONLY.
#ifdef WLAN_FEATURE_VOWIFI_11R
                if (csrRoamIs11rAssoc(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
                }
#endif
#ifdef FEATURE_WLAN_CCX
                if (csrRoamIsCCXAssoc(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
                }
#endif
#ifdef FEATURE_WLAN_LFR
                if (csrRoamIsFastRoamEnabled(pMac) && (csrNeighborRoamStatePreauthDone(pMac)))
                {
                    csrNeighborRoamTranistionPreauthDoneToDisconnected(pMac);
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
#ifdef WLAN_SOFTAP_FEATURE
                if(CSR_IS_INFRA_AP(&pSession->connectedProfile))
                {

                    pRoamInfo = &roamInfo;

                    pRoamInfo->statusCode = pDeauthInd->statusCode;
                    pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;

                    pRoamInfo->staId = (tANI_U8)pDeauthInd->staId;

                    palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pDeauthInd->peerMacAddr, sizeof(tSirMacAddr));
                    palCopyMemory(pMac->hHdd, &pRoamInfo->bssid, pDeauthInd->bssId, sizeof(tCsrBssid));

                    status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_DEAUTH_IND);
                }
#endif                
            }
            break;
        
        case eWNI_SME_SWITCH_CHL_REQ:        // in case of STA, the SWITCH_CHANNEL originates from its AP
            smsLog( pMac, LOGW, FL("eWNI_SME_SWITCH_CHL_REQ from SME\n"));
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
            smsLog( pMac, LOGW, FL("eWNI_SME_DEAUTH_RSP from SME\n"));
#ifdef WLAN_SOFTAP_FEATURE
            {
                tSirSmeDeauthRsp* pDeauthRsp = (tSirSmeDeauthRsp *)pSirMsg;
                sessionId = pDeauthRsp->sessionId;
                if( CSR_IS_SESSION_VALID(pMac, sessionId) )
                {                    
                    pSession = CSR_GET_SESSION(pMac, sessionId);
                    if(!pSession)
                    {
                        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                        return;
                    }
                    if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                    {
                        pRoamInfo = &roamInfo;
                        pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
                        palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pDeauthRsp->peerMacAddr, sizeof(tSirMacAddr));
                        pRoamInfo->reasonCode = eCSR_ROAM_RESULT_FORCED;
                        pRoamInfo->statusCode = pDeauthRsp->statusCode;
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                    }
                }
            }
#endif
            break;
            
        case eWNI_SME_DISASSOC_RSP:
            smsLog( pMac, LOGW, FL("eWNI_SME_DISASSOC_RSP from SME subState = %d\n"), pMac->roam.curSubState[sessionId]);
#ifdef WLAN_SOFTAP_FEATURE
            {
                tSirSmeDisassocRsp *pDisassocRsp = (tSirSmeDisassocRsp *)pSirMsg;
                sessionId = pDisassocRsp->sessionId;
                if( CSR_IS_SESSION_VALID(pMac, sessionId) )
                {                    
                    pSession = CSR_GET_SESSION(pMac, sessionId);
                    if(!pSession)
                    {
                        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                        return;
                    }
                    if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
                    {
                        pRoamInfo = &roamInfo;
                        pRoamInfo->u.pConnectedProfile = &pSession->connectedProfile;
                        palCopyMemory(pMac->hHdd, pRoamInfo->peerMac, pDisassocRsp->peerMacAddr, sizeof(tSirMacAddr));
                        pRoamInfo->reasonCode = eCSR_ROAM_RESULT_FORCED;
                        pRoamInfo->statusCode = pDisassocRsp->statusCode;
                        status = csrRoamCallCallback(pMac, sessionId, pRoamInfo, 0, eCSR_ROAM_LOSTLINK, eCSR_ROAM_RESULT_FORCED);
                    }
                }
            }
#endif
            break;
        case eWNI_SME_MIC_FAILURE_IND:
            {
                tpSirSmeMicFailureInd pMicInd = (tpSirSmeMicFailureInd)pSirMsg;
                tCsrRoamInfo roamInfo, *pRoamInfo = NULL;
                eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
                {
                    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
                    if(!pSession)
                    {
                        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
                        return;
                    }
                    WLAN_VOS_DIAG_EVENT_DEF(secEvent, vos_event_wlan_security_payload_type);
                    palZeroMemory(pMac->hHdd, &secEvent, sizeof(vos_event_wlan_security_payload_type));
                    secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
                    secEvent.encryptionModeMulticast = 
                        (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
                    secEvent.encryptionModeUnicast = 
                        (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
                    secEvent.authMode = 
                        (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                    palCopyMemory( pMac->hHdd, secEvent.bssid, pSession->connectedProfile.bssid, 6 );
                    WLAN_VOS_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
                }
#endif//FEATURE_WLAN_DIAG_SUPPORT_CSR
                status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pMicInd->bssId, &sessionId );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
            }
            break;
#ifdef WLAN_SOFTAP_FEATURE
        case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
            {
                tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd)pSirMsg;
                tCsrRoamInfo roamInfo;
                smsLog( pMac, LOG1, FL("WPS PBC Probe request Indication from SME\n"));
           
                status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pProbeReqInd->bssId, &sessionId );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
                    roamInfo.u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
                    csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_WPS_PBC_PROBE_REQ_IND, 
                        eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
                }
            }
            break;        
#endif
            
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
                            palCopyMemory(pMac->hHdd, &roamInfo.bssid, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
                            roamInfo.u.pConnectedProfile = &pSession->connectedProfile;
                            pRoamInfo = &roamInfo;
                        }
                        else
                        {
                            smsLog(pMac, LOGE, "  CSR eSIR_SME_IBSS_NEW_PEER connected BSS is empty\n");
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
                                    palCopyMemory(pMac->hHdd, pIbssLog->bssid, pNewBss->bssId, 6);
                                    if(pNewBss->ssId.length)
                                    {
                                        palCopyMemory(pMac->hHdd, pIbssLog->ssid, pNewBss->ssId.ssId, pNewBss->ssId.length);
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
                        csrRoamIssueSetContextReq( pMac, sessionId, pSession->connectedProfile.EncryptionType, 
                                                    pSession->pConnectBssDesc,
                                                &Broadcastaddr,
                                                FALSE, FALSE, eSIR_TX_RX, 0, 0, NULL, 0 );
                        result = eCSR_ROAM_RESULT_IBSS_COALESCED;
                        roamStatus = eCSR_ROAM_IBSS_IND;
                        palCopyMemory(pMac->hHdd, &roamInfo.bssid, &pNewBss->bssId, sizeof(tCsrBssid));
                        pRoamInfo = &roamInfo;
                        //This BSSID is th ereal BSSID, let's save it
                        if(pSession->pConnectBssDesc)
                        {
                            palCopyMemory(pMac->hHdd, pSession->pConnectBssDesc->bssId, &pNewBss->bssId, sizeof(tCsrBssid));
                        }
                        // Stop the join IBSS timer in case of join, for 
                        // genuine merge do nothing
                        if(pSession->ibss_join_pending)
                        {
                           pSession->ibss_join_pending = FALSE;
                           csrRoamStopIbssJoinTimer(pMac, sessionId);
                           result = eCSR_ROAM_RESULT_IBSS_JOIN_SUCCESS;
                        }
                    }
                    smsLog(pMac, LOGW, "CSR:  eSIR_SME_JOINED_NEW_BSS received from PE\n");
                    break;
                // detection by LIM that the capabilities of the associated AP have changed.
                case eSIR_SME_AP_CAPS_CHANGED:
                    pApNewCaps = &pStatusChangeMsg->statusChangeInfo.apNewCaps;
                    smsLog(pMac, LOGW, "CSR handling eSIR_SME_AP_CAPS_CHANGED\n");
                    status = csrRoamGetSessionIdFromBSSID( pMac, (tCsrBssid *)pApNewCaps->bssId, &sessionId );
                    if( HAL_STATUS_SUCCESS( status ) )
                    {
                    csrScanForCapabilityChange( pMac, pApNewCaps );
                    result = eCSR_ROAM_RESULT_CAP_CHANGED;
                    roamStatus = eCSR_ROAM_GEN_INFO;
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
                    palCopyMemory(pMac->hHdd, pIbssLog->peerMacAddr, &pIbssPeerInd->peerAddr, 6);
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
                    palCopyMemory(pMac->hHdd, &roamInfo.peerMac, pIbssPeerInd->peerAddr, sizeof(tCsrBssid));
                    palCopyMemory(pMac->hHdd, &roamInfo.bssid, pSession->pConnectBssDesc->bssId, sizeof(tCsrBssid));
                    if(pIbssPeerInd->mesgLen > sizeof(tSmeIbssPeerInd))
                    {
                        status = palAllocateMemory(pMac->hHdd, (void **)&roamInfo.pbFrames, 
                                                (pIbssPeerInd->mesgLen - sizeof(tSmeIbssPeerInd)));
                        if(HAL_STATUS_SUCCESS(status))
                        {
                            roamInfo.nBeaconLength = (pIbssPeerInd->mesgLen - sizeof(tSmeIbssPeerInd));
                            palCopyMemory(pMac->hHdd, roamInfo.pbFrames, ((tANI_U8 *)pIbssPeerInd) + sizeof(tSmeIbssPeerInd),
                                                roamInfo.nBeaconLength);
                        }
                        roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
                        roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
                        roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
                        status = palAllocateMemory(pMac->hHdd, (void **)&roamInfo.pBssDesc, 
                                                pSession->pConnectBssDesc->length);
                        if(HAL_STATUS_SUCCESS(status))
                        {
                            palCopyMemory(pMac->hHdd, roamInfo.pBssDesc, pSession->pConnectBssDesc, 
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
                                palFreeMemory(pMac->hHdd, roamInfo.pbFrames);
                            }
                            if(roamInfo.pBssDesc)
                            {
                                palFreeMemory(pMac->hHdd, roamInfo.pBssDesc);
                            }
                        }
                    }
                    else
                    {
                        pRoamInfo = &roamInfo;
                    }
                        csrRoamIssueSetContextReq( pMac, sessionId, pSession->connectedProfile.EncryptionType, 
                                            pSession->pConnectBssDesc,
                                            &(pIbssPeerInd->peerAddr),
                                            FALSE, TRUE, eSIR_TX_RX, 0, 0, NULL, 0 ); // NO keys... these key parameters don't matter.
                }
                else
                {
                    smsLog(pMac, LOGW, "  CSR eSIR_SME_IBSS_NEW_PEER connected BSS is empty\n");
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
                        palFreeMemory(pMac->hHdd, roamInfo.pbFrames);
                    }
                    if(roamInfo.pBssDesc)
                    {
                        palFreeMemory(pMac->hHdd, roamInfo.pBssDesc);
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
                            palCopyMemory(pMac->hHdd, pIbssLog->peerMacAddr, &pIbssPeerInd->peerAddr, 6);
                        }
                        WLAN_VOS_DIAG_LOG_REPORT(pIbssLog);
                    }
                }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                smsLog(pMac, LOGW, "CSR: Peer departed notification from LIM\n");
                roamInfo.staId = (tANI_U8)pIbssPeerInd->staId;
                roamInfo.ucastSig = (tANI_U8)pIbssPeerInd->ucastSig;
                roamInfo.bcastSig = (tANI_U8)pIbssPeerInd->bcastSig;
                palCopyMemory(pMac->hHdd, &roamInfo.peerMac, pIbssPeerInd->peerAddr, sizeof(tCsrBssid));
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
                            palZeroMemory(pMac->hHdd, &setKeyEvent, sizeof(vos_event_wlan_security_payload_type));
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
                            palCopyMemory( pMac->hHdd, setKeyEvent.bssid, pSession->connectedProfile.bssid, 6 );
                            setKeyEvent.authMode = 
                                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                            if( eSIR_SUCCESS != pRsp->statusCode )
                            {
                                setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
                            }
                            WLAN_VOS_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
                        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                        if( CSR_IS_WAIT_FOR_KEY( pMac, sessionId) )
                        {
                            //We are done with authentication, whethere succeed or not
                            csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, sessionId);

                            csrRoamStopWaitForKeyTimer( pMac );
                            //We do it here because this linkup function is not called after association 
                            //when a key needs to be set. 
                            if( csrIsConnStateConnectedInfra(pMac, sessionId) ) 
                            {
                                csrRoamLinkUp(pMac, pSession->connectedProfile.bssid);
                            }
                        }
                        if( eSIR_SME_SUCCESS == pRsp->statusCode )
                        {
                            palCopyMemory( pMac, &roamInfo.peerMac, &pRsp->peerMacAddr, sizeof(tCsrBssid) );
                                //Make sure we install the GTK before indicating to HDD as authenticated
                                //This is to prevent broadcast packets go out after PTK and before GTK.
                                if( palEqualMemory( pMac->hHdd, &Broadcastaddr, pRsp->peerMacAddr, 
                                           sizeof(tSirMacAddr) ) )
                                {
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
                            smsLog( pMac, LOGE, "CSR: Roam Completion setkey command failed(%d) PeerMac %02X-%02X-%02X-%02X-%02X-%02X...\n", 
                                pRsp->statusCode, pRsp->peerMacAddr[0], pRsp->peerMacAddr[1], pRsp->peerMacAddr[2],
                                pRsp->peerMacAddr[3], pRsp->peerMacAddr[4], pRsp->peerMacAddr[5] );
                        }
                        csrRoamCallCallback(pMac, sessionId, &roamInfo, pCommand->u.setKeyCmd.roamId, 
                                            eCSR_ROAM_SET_KEY_COMPLETE, result);
                        // Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
                        // can go ahead and initiate the TSPEC if any are pending
                        sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_CCX
                        //Send Adjacent AP repot to new AP.
                        if (result == eCSR_ROAM_RESULT_AUTHENTICATED &&
                            pSession->isPrevApInfoValid && 
                            pSession->connectedProfile.isCCXAssoc)
                        {
#ifdef WLAN_FEATURE_VOWIFI
                            csrCcxSendAdjacentApRepMsg(pMac, pSession);
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
                        smsLog( pMac, LOGE, "CSR: Roam Completion called but setkey command is not ACTIVE ...\n" );
                    }
                }
                else
                {
                    smsLog( pMac, LOGE, "CSR: SetKey Completion called but NO commands are ACTIVE ...\n" );
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
                            palZeroMemory(pMac->hHdd, &removeKeyEvent, sizeof(vos_event_wlan_security_payload_type));
                            removeKeyEvent.eventId = WLAN_SECURITY_EVENT_REMOVE_KEY_RSP;
                            removeKeyEvent.encryptionModeMulticast = 
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
                            removeKeyEvent.encryptionModeUnicast = 
                                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
                            palCopyMemory( pMac->hHdd, removeKeyEvent.bssid, pSession->connectedProfile.bssid, 6 );
                            removeKeyEvent.authMode = 
                                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
                            if( eSIR_SUCCESS != pRsp->statusCode )
                            {
                                removeKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
                            }
                            WLAN_VOS_DIAG_EVENT_REPORT(&removeKeyEvent, EVENT_WLAN_SECURITY);
                        }
#endif //FEATURE_WLAN_DIAG_SUPPORT_CSR
                        if( eSIR_SME_SUCCESS == pRsp->statusCode )
                        {
                            palCopyMemory( pMac, &roamInfo.peerMac, &pRsp->peerMacAddr, sizeof(tCsrBssid) );
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
                        smsLog( pMac, LOGW, "CSR: Roam Completion called but setkey command is not ACTIVE ...\n" );
                    }
                }
                else
                {
                    smsLog( pMac, LOGW, "CSR: SetKey Completion called but NO commands are ACTIVE ...\n" );
                }
                smeProcessPendingQueue( pMac );
            }
            break;
        case eWNI_SME_GET_STATISTICS_RSP:
            smsLog( pMac, LOGW, FL("Stats rsp from PE\n"));
            csrRoamStatsRspProcessor( pMac, pSirMsg );
            break;
        case eWNI_SME_GET_RSSI_REQ:
            smsLog( pMac, LOGW, FL("GetRssiReq from self\n"));
            csrUpdateRssi( pMac, pSirMsg );
            break;

#ifdef WLAN_FEATURE_VOWIFI_11R
        case eWNI_SME_FT_PRE_AUTH_RSP:
            csrRoamFTPreAuthRspProcessor( pMac, (tpSirFTPreAuthRsp)pSirMsg );
            break;
#endif
        case eWNI_SME_MAX_ASSOC_EXCEEDED:
            pSmeMaxAssocInd = (tSmeMaxAssocInd*)pSirMsg;
            smsLog( pMac, LOG1, FL("send indication that max assoc have been reached and the new peer cannot be accepted\n"));          
            sessionId = pSmeMaxAssocInd->sessionId;
            roamInfo.sessionId = sessionId;
            palCopyMemory(pMac->hHdd, &roamInfo.peerMac, pSmeMaxAssocInd->peerMac, sizeof(tCsrBssid));
            csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, 
                    eCSR_ROAM_INFRA_IND, eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
            break;
            
        case eWNI_SME_BTAMP_LOG_LINK_IND:
            smsLog( pMac, LOG1, FL("Establish logical link req from HCI serialized through MC thread\n"));
            btampEstablishLogLinkHdlr( pSirMsg );
            break;
        case eWNI_SME_RSSI_IND:
            smsLog( pMac, LOG1, FL("RSSI indication from TL serialized through MC thread\n"));
            csrRoamRssiIndHdlr( pMac, pSirMsg );
        break;

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
         VOS_ASSERT( pSession->bRefAssocStartCnt == 0);
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
    //Check whether time is up
    if(pSession->fCancelRoaming || fForce || 
       ((curTime - pSession->roamingStartTime) > roamTime) ||
       eCsrReassocRoaming == pSession->roamingReason ||
       eCsrDynamicRoaming == pSession->roamingReason)
    {
        smsLog(pMac, LOGW, FL("  indicates roaming completion\n"));
        if(pSession->fCancelRoaming && CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason))
        {
            //roaming is cancelled, tell HDD to indicate disconnect
            //Because LIM overload deauth_ind for both deauth frame and missed beacon
            //we need to use this logic to detinguish it. For missed beacon, LIM set reason
            //to be eSIR_BEACON_MISSED
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
        if(!HAL_STATUS_SUCCESS(csrRoamStartRoamingTimer(pMac, sessionId, PAL_TIMER_TO_SEC_UNIT)))
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
        smsLog(pMac, LOGW, "   Cancelling roaming\n");
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
            //Since CSR may be in lostlink roaming situation, abort all roaming related activities
            csrScanAbortMacScan(pMac);
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
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    smsLog(pMac, LOG1, " csrScanStartRoamingTimer \n ");
    pSession->roamingTimerInfo.sessionId = (tANI_U8)sessionId;
    status = palTimerStart(pMac->hHdd, pSession->hTimerRoaming, interval, eANI_BOOLEAN_FALSE);
    
    return (status);
}

eHalStatus csrRoamStopRoamingTimer(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (palTimerStop(pMac->hHdd, pMac->roam.roamSession[sessionId].hTimerRoaming));
}

void csrRoamWaitForKeyTimeOutHandler(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    if( CSR_IS_WAIT_FOR_KEY( pMac, pInfo->sessionId ) )
    {
        smsLog(pMac, LOGW, " SME pre-auth state timeout. \n ");
        //Change the substate so command queue is unblocked.
        csrRoamSubstateChange( pMac, eCSR_ROAM_SUBSTATE_NONE, pInfo->sessionId);
    }
    
}

eHalStatus csrRoamStartWaitForKeyTimer(tpAniSirGlobal pMac, tANI_U32 interval)
{
    eHalStatus status;
    
    smsLog(pMac, LOG1, " csrScanStartWaitForKeyTimer \n ");
    status = palTimerStart(pMac->hHdd, pMac->roam.hTimerWaitForKey, interval, eANI_BOOLEAN_FALSE);
    
    return (status);
}

eHalStatus csrRoamStopWaitForKeyTimer(tpAniSirGlobal pMac)
{
    return (palTimerStop(pMac->hHdd, pMac->roam.hTimerWaitForKey));
}

void csrRoamIbssJoinTimerHandler(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    eCsrRoamDisconnectReason reason = eCSR_DISCONNECT_REASON_IBSS_JOIN_FAILURE;
    tANI_U32 sessionId = pInfo->sessionId;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return;
    }
    
    pSession->ibss_join_pending = FALSE;
    // JEZ100225:  As of main/latest "tip", we are no longer doing this. Check on this.
    //csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_IBS_IND, eCSR_ROAM_RESULT_IBSS_JOIN_FAILED);
    // Send an IBSS stop request to PE
    csrRoamDisconnectInternal(pMac, sessionId, reason);
}
eHalStatus csrRoamStartIbssJoinTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval)
{
    eHalStatus status;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    smsLog(pMac, LOG1, " csrRoamStartIbssJoinTimer \n ");
    pSession->ibssJoinTimerInfo.sessionId = (tANI_U8)sessionId;
    status = palTimerStart(pMac->hHdd, pSession->hTimerIbssJoining, interval, eANI_BOOLEAN_FALSE);
    
    return (status);
}
eHalStatus csrRoamStopIbssJoinTimer(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (palTimerStop(pMac->hHdd, pMac->roam.roamSession[sessionId].hTimerIbssJoining));
}
void csrRoamCompletion(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamInfo *pRoamInfo, tSmeCmd *pCommand, 
                        eCsrRoamResult roamResult, tANI_BOOLEAN fSuccess)
{
    eRoamCmdStatus roamStatus = csrGetRoamCompleteStatus(pMac, sessionId);
    tANI_U32 roamId = 0;
    
    if(pCommand)
    {
        roamId = pCommand->u.roamCmd.roamId;
#if defined(VOSS_ENABLED)
        VOS_ASSERT( sessionId == pCommand->sessionId );
#endif
    }
    if(eCSR_ROAM_ROAMING_COMPLETION == roamStatus)
    {
        //if success, force roaming completion
        csrRoamCompleteRoaming(pMac, sessionId, fSuccess, roamResult);
    }
    else
    {
        VOS_ASSERT((CSR_GET_SESSION( pMac, sessionId ))->bRefAssocStartCnt == 0);
        smsLog(pMac, LOGW, FL("  indicates association completion. roamResult = %d\n"), roamResult);
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
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    //Only need to roam for infra station. In this case P2P client will roam as well
    tANI_BOOLEAN fToRoam = CSR_IS_INFRASTRUCTURE(&pSession->connectedProfile);
    pSession->fCancelRoaming = eANI_BOOLEAN_FALSE;
    if ( eWNI_SME_DISASSOC_IND == type )
    {
        result = eCSR_ROAM_RESULT_DISASSOC_IND;
        pDisassocIndMsg = (tSirSmeDisassocInd *)pSirMsg;
        pSession->roamingStatusCode = pDisassocIndMsg->statusCode;
    }
    else if ( eWNI_SME_DEAUTH_IND == type )
    {
        result = eCSR_ROAM_RESULT_DEAUTH_IND;
        pDeauthIndMsg = (tSirSmeDeauthInd *)pSirMsg;
        pSession->roamingStatusCode = pDeauthIndMsg->statusCode;
    }
    else
    {
        smsLog(pMac, LOGW, FL("gets an unknown type (%d)\n"), type);
        result = eCSR_ROAM_RESULT_NONE;
    }
    
    // call profile lost link routine here
#ifdef WLAN_SOFTAP_FEATURE
    if(!CSR_IS_INFRA_AP(&pSession->connectedProfile))
#endif
    {
        csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_LOSTLINK_DETECTED, result);
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

    //tell HDD to disconnect
    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
    roamInfo.statusCode = (tSirResultCodes)pSession->roamingStatusCode;
    roamInfo.reasonCode = pSession->joinFailStatusCode.reasonCode;
#ifdef WLAN_SOFTAP_FEATURE
    if( eWNI_SME_DISASSOC_IND == type)
    {
            //staMacAddr
            palCopyMemory(pMac->hHdd, roamInfo.peerMac, pDisassocIndMsg->peerMacAddr, sizeof(tSirMacAddr));
            roamInfo.staId = (tANI_U8)pDisassocIndMsg->staId;
        }
    else if( eWNI_SME_DEAUTH_IND == type )
    {
            //staMacAddr
            palCopyMemory(pMac->hHdd, roamInfo.peerMac, pDeauthIndMsg->peerMacAddr, sizeof(tSirMacAddr));
            roamInfo.staId = (tANI_U8)pDeauthIndMsg->staId;
        }
#endif
    smsLog(pMac, LOGW, FL("roamInfo.staId (%d)\n"), roamInfo.staId);
    csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_LOSTLINK, result);
    if(fToRoam)
    {
        //Only remove the connected BSS in infrastructure mode
        csrRoamRemoveConnectedBssFromScanCache(pMac, &pSession->connectedProfile);
        //Not to do anying for lostlink with WDS
        if( pMac->roam.configParam.nRoamingTime )
        {
            if(HAL_STATUS_SUCCESS(status = csrRoamStartRoaming(pMac, sessionId,
                        ( eWNI_SME_DEAUTH_IND == type ) ? 
                        eCsrLostlinkRoamingDeauth : eCsrLostlinkRoamingDisassoc)))
            {
                palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
                smsLog(pMac, LOGW, " %s Fail to start roaming, status = %d", __FUNCTION__, status);
                fToRoam = eANI_BOOLEAN_FALSE;
            }
        }
        else
        {
            //We are told not to roam, indicate lostlink
            fToRoam = eANI_BOOLEAN_FALSE;
        }
    }
    if(!fToRoam)
    {
       if( eWNI_SME_DISASSOC_IND == type)
       {
           //staMacAddr
           palCopyMemory(pMac->hHdd, roamInfo.peerMac, pDisassocIndMsg->peerMacAddr, sizeof(tSirMacAddr));
           roamInfo.staId = (tANI_U8)pDisassocIndMsg->staId;
       }
       else if( eWNI_SME_DEAUTH_IND == type )
       {
           //staMacAddr
            palCopyMemory(pMac->hHdd, roamInfo.peerMac, pDeauthIndMsg->peerMacAddr, sizeof(tSirMacAddr));
            roamInfo.staId = (tANI_U8)pDeauthIndMsg->staId;
        }
        csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_LOSTLINK, result);
       
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
          smsLog( pMac, LOGW, "Lost link roaming started ...\n");
       }
    }
    else
    {
       //We are told not to roam, indicate lostlink
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
                smsLog( pMac, LOGE, " ******csrRoamWmStatusChangeComplete fail to release command\n");
            }
            
        }
        else
        {
            smsLog( pMac, LOGW, "CSR: WmStatusChange Completion called but LOST LINK command is not ACTIVE ...\n" );
        }
    }
    else
    {
        smsLog( pMac, LOGW, "CSR: WmStatusChange Completion called but NO commands are ACTIVE ...\n" );
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
            smsLog(pMac, LOGW, FL("gets an unknown command %d\n"), pCommand->u.wmStatusChangeCmd.Type);
            break;
    }
    //For WDS, we want to stop BSS as well when it is indicated that it is disconnected.
    if( CSR_IS_CONN_WDS(&pSession->connectedProfile) )
    {
        if( !HAL_STATUS_SUCCESS(csrRoamIssueStopBssCmd( pMac, pCommand->sessionId, eANI_BOOLEAN_TRUE )) )
        {
            //This is not good
            smsLog(pMac, LOGE, FL("  failed to issue stopBSS command\n"));
        }
    }
    // Lost Link just triggers a roaming sequence.  We can complte the Lost Link
    // command here since there is nothing else to do.
    csrRoamWmStatusChangeComplete( pMac );
}

//This function returns band and mode information.
//The only tricky part is that if phyMode is set to 11abg, this function may return eCSR_CFG_DOT11_MODE_11B
//instead of eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
#ifdef WLAN_SOFTAP_FEATURE
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, tCsrRoamProfile *pProfile, 
                                                     tANI_U8 operationChn, eCsrBand *pBand )
#else
static eCsrCfgDot11Mode csrRoamGetPhyModeBandForBss( tpAniSirGlobal pMac, eCsrPhyMode phyModeIn, 
                                                     tANI_U8 operationChn, eCsrBand *pBand )
#endif
{
#ifdef WLAN_SOFTAP_FEATURE
    eCsrPhyMode phyModeIn = (eCsrPhyMode)pProfile->phyMode;
    eCsrCfgDot11Mode cfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(pProfile, phyModeIn, 
                                            pMac->roam.configParam.ProprietaryRatesEnabled);
#else
    eCsrCfgDot11Mode cfgDot11Mode = csrGetCfgDot11ModeFromCsrPhyMode(phyModeIn, 
                                            pMac->roam.configParam.ProprietaryRatesEnabled);
#endif
    eCsrBand eBand;
    
    //If the global setting for dot11Mode is set to auto/abg, we overwrite the setting in the profile.
#ifdef WLAN_SOFTAP_FEATURE
    if( ((!CSR_IS_INFRA_AP(pProfile )&& !CSR_IS_WDS(pProfile )) && 
         ((eCSR_CFG_DOT11_MODE_AUTO == pMac->roam.configParam.uCfgDot11Mode) ||
         (eCSR_CFG_DOT11_MODE_ABG == pMac->roam.configParam.uCfgDot11Mode))) ||
        (eCSR_CFG_DOT11_MODE_AUTO == cfgDot11Mode) || (eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) )
#else
    if( (eCSR_CFG_DOT11_MODE_AUTO == pMac->roam.configParam.uCfgDot11Mode) ||
        (eCSR_CFG_DOT11_MODE_ABG == pMac->roam.configParam.uCfgDot11Mode) ||
        (eCSR_CFG_DOT11_MODE_AUTO == cfgDot11Mode) || (eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) )
#endif
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
                eBand = eCSR_BAND_24;
                break;            
            //case eCSR_CFG_DOT11_MODE_BEST:
            //    cfgDot11Mode = eCSR_CFG_DOT11_MODE_BEST;
            //    eBand = eCSR_BAND_24;
            //    break;            
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
                    // channel is a 2.4GHz channel.  Set mode to 11g.
                    //
                    // !!LAC - WiFi tests require IBSS networks to start in 11b mode without any change to the
                    // default parameter settings on the adapter.  We use ACU to start an IBSS through creation
                    // of a startIBSS profile.   this startIBSS profile has Auto MACProtocol and the 
                    // adapter property setting for dot11Mode is also AUTO.   So in this case, let's start 
                    // the IBSS network in 11b mode instead of 11g mode.
                    //
                    // so this is for Auto=profile->MacProtocol && Auto=Global.dot11Mode && profile->channel is < 14, 
                    // then start the IBSS in b mode.
                    // 
                    // Note:  we used to have this start as an 11g IBSS for best performance... now to specify that
                    // the user will have to set the do11Mode in the property page to 11g to force it.
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
     smsLog(pMac, LOGE, FL("  Switching to Dot11B mode \n"));
     cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
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
    pSession->ibss_join_pending = FALSE;
    csrRoamStopIbssJoinTimer(pMac, sessionId );
    // Set the roaming substate to 'stop Bss request'...
    csrRoamSubstateChange( pMac, NewSubstate, sessionId );

    // attempt to stop the Bss (reason code is ignored...)
    status = csrSendMBStopBssReqMsg( pMac, sessionId );
    
    return (status);
}

//pNumChan is a caller allocated space with the sizeof pChannels
eHalStatus csrGetCfgValidChannels(tpAniSirGlobal pMac, tANI_U8 *pChannels, tANI_U32 *pNumChan)
{
   
    return (ccmCfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                  (tANI_U8 *)pChannels,
                  pNumChan));
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
        ChannelBondingMode = pMac->roam.configParam.channelBondingMode24GHz;
    }
    else
    {
        ChannelBondingMode = pMac->roam.configParam.channelBondingMode5GHz;
    }
    //Figure what the other side's CB mode
    if(WNI_CFG_CHANNEL_BONDING_MODE_DISABLE != ChannelBondingMode)
    {
        if(pIes->HTCaps.present && (eHT_CHANNEL_WIDTH_40MHZ == pIes->HTCaps.supportedChannelWidthSet))
        {
            if(pIes->HTInfo.present)
            {
                /* This is called during INFRA STA/CLIENT and should use the merged value of 
                 * supported channel width and recommended tx width as per standard
                 */
                smsLog(pMac, LOG1, "scws %u rtws %u sco %u\n",
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
                    smsLog(pMac, LOGE, "  Invalid center channel (%d), disable 40MHz mode\n", centerChn);
                    //eRet = PHY_SINGLE_CHANNEL_CENTERED;
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter, sizeof(tCsrScanResultFilter));
        if(HAL_STATUS_SUCCESS(status))
        {
            palZeroMemory(pMac->hHdd, pScanFilter, sizeof(tCsrScanResultFilter));
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
            palFreeMemory(pMac->hHdd, pScanFilter);
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
            if(!palEqualMemory(pMac->hHdd, &pConnProfile->Keys.KeyMaterial[i], 
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
                if ( CSR_IS_CHANNEL_5GHZ(pMac->roam.validChannelList[ idx ]) )   // the max channel# in 11g is 14
                {
                    channel = csrStartIbssChannels50[ idx ];
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
    eCsrCfgDot11Mode cfgDot11Mode;
    eCsrBand eBand;
    tANI_U8 channel = 0;
    tSirNwType nwType;
    tANI_U8 operationChannel = 0; 
    
    if(pProfile->ChannelInfo.numOfChannels && pProfile->ChannelInfo.ChannelList)
    {
       operationChannel = pProfile->ChannelInfo.ChannelList[0];
    }
    
#ifdef WLAN_SOFTAP_FEATURE
    cfgDot11Mode = csrRoamGetPhyModeBandForBss( pMac, pProfile, operationChannel, &eBand );
#else
    cfgDot11Mode = csrRoamGetPhyModeBandForBss( pMac, (eCsrPhyMode)pProfile->phyMode, operationChannel, &eBand );
#endif
    
#ifdef WLAN_FEATURE_P2P
    if( ( (pProfile->csrPersona == VOS_P2P_CLIENT_MODE) ||
          (pProfile->csrPersona == VOS_P2P_GO_MODE) )
     && ( cfgDot11Mode == eCSR_CFG_DOT11_MODE_11B)
      )
    {
        /* This should never happen */
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
              FL("For P2PClient/P2P-GO (persona %d) cfgDot11Mode is 11B\n"), 
              pProfile->csrPersona);
        VOS_ASSERT(0);
    }
#endif
    switch( cfgDot11Mode )
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
        case eCSR_CFG_DOT11_MODE_TAURUS:
            //Because LIM only verifies it against 11a, 11b or 11g, set only 11g or 11a here
            if(eCSR_BAND_24 == eBand)
            {
                nwType = eSIR_11G_NW_TYPE;
            }
            else
            {
                nwType = eSIR_11A_NW_TYPE;
            }
            break;
    }   

    pParam->extendedRateSet.numRates = 0;

    switch ( nwType )
    {
        default:
            smsLog(pMac, LOGE, FL("sees an unknown pSirNwType (%d)\n"), nwType);
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
#ifdef WLAN_FEATURE_P2P
            /* For P2P Client and P2P GO, disable 11b rates */ 
            if( (pProfile->csrPersona == VOS_P2P_CLIENT_MODE) ||
                (pProfile->csrPersona == VOS_P2P_GO_MODE)
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
#endif            
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
}

static void csrRoamGetBssStartParmsFromBssDesc( tpAniSirGlobal pMac, tSirBssDescription *pBssDesc, 
                                                 tDot11fBeaconIEs *pIes, tCsrRoamStartBssParams *pParam )
{
    
    if( pParam )
    {
        pParam->sirNwType = pBssDesc->nwType;
        pParam->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
        pParam->operationChn = pBssDesc->channelId;
        palCopyMemory( pMac->hHdd, &pParam->bssid, pBssDesc->bssId, sizeof(tCsrBssid) );
    
        if( pIes )
        {
            if(pIes->SuppRates.present)
            {
                pParam->operationalRateSet.numRates = pIes->SuppRates.num_rates;
                if(pIes->SuppRates.num_rates > SIR_MAC_RATESET_EID_MAX)
                {
                    smsLog(pMac, LOGE, FL("num_rates :%d is more than SIR_MAC_RATESET_EID_MAX, resetting to SIR_MAC_RATESET_EID_MAX\n"),
                      pIes->SuppRates.num_rates);
                    pIes->SuppRates.num_rates = SIR_MAC_RATESET_EID_MAX;
                }
                palCopyMemory(pMac->hHdd, pParam->operationalRateSet.rate, pIes->SuppRates.rates, 
                    sizeof(*pIes->SuppRates.rates) * pIes->SuppRates.num_rates);
            }
            if( pIes->SSID.present )
            {
                pParam->ssId.length = pIes->SSID.num_ssid;
                palCopyMemory(pMac->hHdd, pParam->ssId.ssId, pIes->SSID.ssid, pParam->ssId.length);
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

//this function finds a valid secondary channel for channel bonding with "channel".
//Param: channel -- primary channel, caller must validate it
//       cbChoice -- CB directory
//Return: if 0, no secondary channel is found. Otherwise a valid secondary channel.
static tANI_U8 csrRoamGetSecondaryChannel(tpAniSirGlobal pMac, tANI_U8 channel, eCsrCBChoice cbChoice)
{
    tANI_U8 chnUp = 0, chnDown = 0, chnRet = 0;
    switch (cbChoice)
    {
    case eCSR_CB_OFF:
        chnUp = 0;
        chnDown = 0;
        break;
    case eCSR_CB_DOWN:
        chnUp = 0;
        chnDown = channel - CSR_CB_CHANNEL_GAP;
        break;
    case eCSR_CB_UP:
        chnUp = channel + CSR_CB_CHANNEL_GAP;
        chnDown = 0;
        break;
    case eCSR_CB_AUTO:
    //consider every other value means auto
    default:
        chnUp = channel + CSR_CB_CHANNEL_GAP;
        chnDown = channel - CSR_CB_CHANNEL_GAP;
        break;
    }
    //if CB_UP or auto, try channel up first
    if(chnUp && CSR_IS_SAME_BAND_CHANNELS(chnUp, channel) && csrRoamIsChannelValid(pMac, chnUp))
    {
        //found a valid up channel for channel bonding
        //check whether the center channel is valid
        if(csrRoamIsValid40MhzChannel(pMac, channel + CSR_CB_CENTER_CHANNEL_OFFSET))
        {
            chnRet = chnUp;
        }
    }
    if(chnRet == 0 && chnDown && CSR_IS_SAME_BAND_CHANNELS(chnDown, channel) && csrRoamIsChannelValid(pMac, chnDown))
    {
        //found a valid down channel for channel bonding
        if(csrRoamIsValid40MhzChannel(pMac, channel - CSR_CB_CENTER_CHANNEL_OFFSET))
        {
            chnRet = chnDown;
        }
    }
    return chnRet;
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
                palCopyMemory(pMac->hHdd, pIbssLog->bssid, pBssDesc->bssId, 6);
            }
            else
            {
                pIbssLog->eventId = WLAN_IBSS_EVENT_START_IBSS_REQ;
            }
            palCopyMemory(pMac->hHdd, pIbssLog->ssid, pParam->ssId.ssId,
                pParam->ssId.length);
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

#ifdef WLAN_SOFTAP_FEATURE
    pParam->privacy           = pProfile->privacy;
    pParam->fwdWPSPBCProbeReq = pProfile->fwdWPSPBCProbeReq;   
    pParam->authType          = pProfile->csr80211AuthType;
    pParam->beaconInterval    = pProfile->beaconInterval;
    pParam->dtimPeriod        = pProfile->dtimPeriod;
    pParam->ApUapsdEnable     = pProfile->ApUapsdEnable;
    pParam->ssidHidden        = pProfile->SSIDs.SSIDList[0].ssidHidden;
    if (CSR_IS_INFRA_AP(pProfile)&& (pParam->operationChn != 0))
    {
        if (csrIsValidChannel(pMac, pParam->operationChn) != eHAL_STATUS_SUCCESS)
        {
            pParam->operationChn = INFRA_AP_DEFAULT_CHANNEL;                   
        }  
    }
    pParam->protEnabled     = pProfile->protEnabled;
    pParam->obssProtEnabled = pProfile->obssProtEnabled;
    pParam->ht_protection   = pProfile->cfg_protection;
    pParam->wps_state       = pProfile->wps_state;
#endif
    
#ifdef WLAN_SOFTAP_FEATURE
    pParam->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, pProfile, pParam->operationChn /* pProfile->operationChannel*/, 
                                        &eBand);
#else
    pParam->uCfgDot11Mode = csrRoamGetPhyModeBandForBss(pMac, (eCsrPhyMode)pProfile->phyMode, pProfile->operationChannel, 
                                        &eBand);
#endif
    pParam->bssPersona = pProfile->csrPersona;
    // When starting an IBSS, start on the channel from the Profile.
    status = csrSendMBStartBssReqMsg( pMac, sessionId, pProfile->BSSType, pParam, pBssDesc );
    return (status);
}

static void csrRoamPrepareBssParams(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                                     tSirBssDescription *pBssDesc, tBssConfigParam *pBssConfig, tDot11fBeaconIEs *pIes)
{
    tANI_U8 Channel, SecondChn;
    ePhyChanBondState cbMode = PHY_SINGLE_CHANNEL_CENTERED;
    eCsrCBChoice cbChoice;
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
            palCopyMemory(pMac->hHdd, &pSession->bssParams.bssid, &pSession->selfMacAddr, sizeof(tCsrBssid));
        }
    }
    else
    {
        csrRoamGetBssStartParms(pMac, pProfile, &pSession->bssParams);
        //Use the first SSID
        if(pProfile->SSIDs.numOfSSIDs)
        {
            palCopyMemory(pMac->hHdd, &pSession->bssParams.ssId, pProfile->SSIDs.SSIDList, sizeof(tSirMacSSid));
        }
        //For WDS station, use selfMac as the self BSSID
        if( CSR_IS_WDS_STA( pProfile ) )
        {
            palCopyMemory(pMac->hHdd, &pSession->bssParams.bssid, &pSession->selfMacAddr, sizeof(tCsrBssid));
        }
        //Use the first BSSID
        else if( pProfile->BSSIDs.numOfBSSIDs )
        {
            palCopyMemory(pMac->hHdd, &pSession->bssParams.bssid, pProfile->BSSIDs.bssid, sizeof(tCsrBssid));
        }
        else
        {
            palZeroMemory( pMac->hHdd, &pSession->bssParams.bssid, sizeof(tCsrBssid) );
        }
    }
    Channel = pSession->bssParams.operationChn;
    //Set operating channel in pProfile which will be used 
    //in csrRoamSetBssConfigCfg() to determine channel bonding
    //mode and will be configured in CFG later 
    pProfile->operationChannel = Channel;
    
    if(Channel == 0)
    {
        smsLog(pMac, LOGW, "   CSR cannot find a channel to start IBSS\n");
    }
    else
    {
  
        csrRoamDetermineMaxRateForAdHoc( pMac, &pSession->bssParams.operationalRateSet );
        if (CSR_IS_INFRA_AP(pProfile))
        {
            if(CSR_IS_CHANNEL_24GHZ(Channel))
            {
                cbMode = pMac->roam.configParam.channelBondingMode24GHz;
            }
            else
            {
                cbMode = pMac->roam.configParam.channelBondingMode5GHz;
            }
            smsLog(pMac, LOG1, "##softap cbMode %d\n", cbMode);
            pBssConfig->cbMode = cbMode;
            pSession->bssParams.cbMode = cbMode;
        }

        if( CSR_IS_START_IBSS( pProfile ) )
        {
           //TBH: channel bonding is not supported for Libra
            if( pProfile->ChannelInfo.ChannelList && eCSR_OPERATING_CHANNEL_AUTO != pProfile->ChannelInfo.ChannelList[0] )
            {
                Channel = pProfile->ChannelInfo.ChannelList[0];
                cbChoice = pProfile->CBMode;
            }
            else {
                cbChoice = pMac->roam.configParam.cbChoice;
            }
            pSession->bssParams.operationChn = Channel;
            //make sure channel is valid
            if(!csrRoamIsChannelValid(pMac, Channel))
            {
                //set Channel to 0 to let lim know this is invalid
                //We still send this request down to lim even though we know the channel is wrong because
                //lim will response with error and hdd's eWNI_SME_START_BSS_RSP handler will roam other profile (if any)
                Channel = 0;
                pSession->bssParams.operationChn = 0;
            }
            else {
                tANI_U32 ChannelBondingMode;
                if(CSR_IS_CHANNEL_24GHZ(Channel))
                {
                    ChannelBondingMode = pMac->roam.configParam.channelBondingMode24GHz;
                }
                else
                {
                    ChannelBondingMode = pMac->roam.configParam.channelBondingMode5GHz;
                }
                //now we have a valid channel
                if(WNI_CFG_CHANNEL_BONDING_MODE_DISABLE != ChannelBondingMode)
                {
                    //let's pick a secondard channel
                    SecondChn = csrRoamGetSecondaryChannel(pMac, Channel, cbChoice);
                    if(SecondChn > Channel)
                    {
                        cbMode = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
                    }
                    else if(SecondChn && SecondChn < Channel)
                    {
                        cbMode = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
                    }
                    else
                    {
                        cbMode = PHY_SINGLE_CHANNEL_CENTERED;
                    }
                    pSession->bssParams.cbMode = cbMode;
                }
                else
                {
                    pSession->bssParams.cbMode = PHY_SINGLE_CHANNEL_CENTERED;
                }
            }
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
        
        status = palAllocateMemory(pMac->hHdd, (void **)&pBssConfig, sizeof(tBssConfigParam)); 
        if(HAL_STATUS_SUCCESS(status))
        {
            palZeroMemory(pMac->hHdd, pBssConfig, sizeof(tBssConfigParam));
            // there is no Bss description before we start an IBSS so we need to adopt
            // all Bss configuration parameters from the Profile.
            status = csrRoamPrepareBssConfigFromProfile(pMac, pProfile, pBssConfig, NULL);
            if(HAL_STATUS_SUCCESS(status))
            {
                //save dotMode
                pMac->roam.roamSession[sessionId].bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
                //Prepare some more parameters for this IBSS
                csrRoamPrepareBssParams(pMac, sessionId, pProfile, NULL, pBssConfig, NULL);
                status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile, NULL, pBssConfig, NULL);
            }
            
            palFreeMemory(pMac->hHdd, pBssConfig);
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
        palCopyMemory( pMac->hHdd, &pSession->connectedProfile.bssid, 
                      &(pNewBss->bssId), sizeof( tCsrBssid ) );    
    }
    return;
}

#ifdef FEATURE_WLAN_WAPI
eHalStatus csrRoamSetBKIDCache( tpAniSirGlobal pMac, tANI_U32 sessionId, tBkidCacheInfo *pBKIDCache,
                                 tANI_U32 numItems )
{
   eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
   tCsrRoamSession *pSession;
   if(!CSR_IS_SESSION_VALID( pMac, sessionId ))
   {
       smsLog(pMac, LOGE, FL("  Invalid session ID\n"));
       return status;
   }
   smsLog(pMac, LOGW, "csrRoamSetBKIDCache called, numItems = %d\n", numItems);
   pSession = CSR_GET_SESSION( pMac, sessionId );
   if(numItems <= CSR_MAX_BKID_ALLOWED)
   {
       status = eHAL_STATUS_SUCCESS;
       //numItems may be 0 to clear the cache
       pSession->NumBkidCache = (tANI_U16)numItems;
       if(numItems && pBKIDCache)
       {
           status = palCopyMemory( pMac->hHdd, pSession->BkidCacheInfo, pBKIDCache,
                           sizeof(tBkidCacheInfo) * numItems );
       }
   }
   return (status);
}
eHalStatus csrRoamGetBKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pNum,
                                tBkidCacheInfo *pBkidCache)
{
   eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
   tCsrRoamSession *pSession;
   if(!CSR_IS_SESSION_VALID( pMac, sessionId ))
   {
       smsLog(pMac, LOGE, FL("  Invalid session ID\n"));
       return status;
   }
   pSession = CSR_GET_SESSION( pMac, sessionId );
   if(pNum && pBkidCache)
   {
       if(pSession->NumBkidCache == 0)
       {
           *pNum = 0;
           status = eHAL_STATUS_SUCCESS;
       }
       else if(*pNum >= pSession->NumBkidCache)
       {
           if(pSession->NumBkidCache > CSR_MAX_PMKID_ALLOWED)
           {
               smsLog(pMac, LOGE, FL("NumPmkidCache :%d is more than CSR_MAX_PMKID_ALLOWED, resetting to CSR_MAX_PMKID_ALLOWED\n"),
                 pSession->NumBkidCache);
               pSession->NumBkidCache = CSR_MAX_PMKID_ALLOWED;
           }
           palCopyMemory( pMac->hHdd, pBkidCache, pSession->BkidCacheInfo,
                           sizeof(tBkidCacheInfo) * pSession->NumBkidCache );
           *pNum = pSession->NumBkidCache;
           status = eHAL_STATUS_SUCCESS;
       }
   }
   return (status);
}
tANI_U32 csrRoamGetNumBKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
   return (pMac->roam.roamSession[sessionId].NumBkidCache);
}
#endif /* FEATURE_WLAN_WAPI */
eHalStatus csrRoamSetPMKIDCache( tpAniSirGlobal pMac, tANI_U32 sessionId,
                                 tPmkidCacheInfo *pPMKIDCache, tANI_U32 numItems )
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    
    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    smsLog(pMac, LOGW, "csrRoamSetPMKIDCache called, numItems = %d\n", numItems);
    if(numItems <= CSR_MAX_PMKID_ALLOWED)
    {
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
        {
            WLAN_VOS_DIAG_EVENT_DEF(secEvent, vos_event_wlan_security_payload_type);
            palZeroMemory(pMac->hHdd, &secEvent, sizeof(vos_event_wlan_security_payload_type));
            secEvent.eventId = WLAN_SECURITY_EVENT_PMKID_UPDATE;
            secEvent.encryptionModeMulticast = 
                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.mcEncryptionType);
            secEvent.encryptionModeUnicast = 
                (v_U8_t)diagEncTypeFromCSRType(pSession->connectedProfile.EncryptionType);
            palCopyMemory( pMac->hHdd, secEvent.bssid, pSession->connectedProfile.bssid, 6 );
            secEvent.authMode = 
                (v_U8_t)diagAuthTypeFromCSRType(pSession->connectedProfile.AuthType);
            WLAN_VOS_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
        }
#endif//FEATURE_WLAN_DIAG_SUPPORT_CSR
        status = eHAL_STATUS_SUCCESS;
        //numItems may be 0 to clear the cache
        pSession->NumPmkidCache = (tANI_U16)numItems;
        if(numItems && pPMKIDCache)
        {
            status = palCopyMemory( pMac->hHdd, pSession->PmkidCacheInfo, pPMKIDCache,
                            sizeof(tPmkidCacheInfo) * numItems );
        }
    }
    return (status);
}

tANI_U32 csrRoamGetNumPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (pMac->roam.roamSession[sessionId].NumPmkidCache);
}

eHalStatus csrRoamGetPMKIDCache(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 *pNum, tPmkidCacheInfo *pPmkidCache)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

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
                smsLog(pMac, LOGE, FL("NumPmkidCache :%d is more than CSR_MAX_PMKID_ALLOWED, resetting to CSR_MAX_PMKID_ALLOWED\n"),
                  pSession->NumPmkidCache);
                pSession->NumPmkidCache = CSR_MAX_PMKID_ALLOWED;
            }
            palCopyMemory( pMac->hHdd, pPmkidCache, pSession->PmkidCacheInfo,
                            sizeof(tPmkidCacheInfo) * pSession->NumPmkidCache );
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
                status = palCopyMemory(pMac->hHdd, pBuf, pSession->pWpaRsnReqIE, pSession->nWpaRsnReqIeLength);
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
                status = palCopyMemory(pMac->hHdd, pBuf, pSession->pWpaRsnRspIE, pSession->nWpaRsnRspIeLength);
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
                status = palCopyMemory(pMac->hHdd, pBuf, pSession->pWapiReqIE, pSession->nWapiReqIeLength);
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
                status = palCopyMemory(pMac->hHdd, pBuf, pSession->pWapiRspIE, pSession->nWapiRspIeLength);
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

//This function remove the connected BSS from te cached scan result
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
            status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter, sizeof(tCsrScanResultFilter));
            if(!HAL_STATUS_SUCCESS(status)) break;
            palZeroMemory(pMac->hHdd, pScanFilter, sizeof(tCsrScanResultFilter));
            status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter->BSSIDs.bssid, sizeof(tCsrBssid));
            if(!HAL_STATUS_SUCCESS(status)) break;
            palCopyMemory(pMac->hHdd, pScanFilter->BSSIDs.bssid, &pConnProfile->bssid, sizeof(tCsrBssid));
            pScanFilter->BSSIDs.numOfBSSIDs = 1;
            if(!csrIsNULLSSID(pConnProfile->SSID.ssId, pConnProfile->SSID.length))
            {
                status = palAllocateMemory(pMac->hHdd, (void **)&pScanFilter->SSIDs.SSIDList, sizeof(tCsrSSIDInfo));
                if(!HAL_STATUS_SUCCESS(status)) break;
                palCopyMemory(pMac->hHdd, &pScanFilter->SSIDs.SSIDList[0].SSID, &pConnProfile->SSID, sizeof(tSirMacSSid));
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
            pScanFilter->countryCode[0] = 0;
            pScanFilter->phyMode = eCSR_DOT11_MODE_TAURUS;
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
                    palFreeMemory(pMac->hHdd, pIes);
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
            palFreeMemory(pMac->hHdd, pScanFilter);
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
                smsLog( pMac, LOGW, " BTAMP is not allowed due to IBSS/BT-AMP exist in session %d\n", sessionId );
                status = eHAL_STATUS_CSR_WRONG_STATE;
                break;
            }
            if( csrIsConnStateInfra( pMac, sessionId ) )
            {
                if( chnId && 
                    ( (tANI_U8)chnId != pMac->roam.roamSession[sessionId].connectedProfile.operationChannel ) )
                {
                    smsLog( pMac, LOGW, " BTAMP is not allowed due to channel (%d) diff than infr channel (%d)\n",
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
#if defined(VOSS_ENABLED)
        //We don't expect Bt-AMP HDD not to disconnect the last connection first at this time. 
        //Otherwise we need to add code to handle the
        //situation just like IBSS. Though for WDS station, we need to send disassoc to PE first then 
        //send stop_bss to PE, before we can continue.
        VOS_ASSERT( !csrIsConnStateWds( pMac, sessionId ) );
#endif
        palZeroMemory(pMac->hHdd, &bssConfig, sizeof(tBssConfigParam));
        /* Assume HDD provide bssid in profile */
        palCopyMemory( pMac->hHdd, &pSession->bssParams.bssid, pProfile->BSSIDs.bssid[0], sizeof(tCsrBssid) );
        // there is no Bss description before we start an WDS so we need
        // to adopt all Bss configuration parameters from the Profile.
        status = csrRoamPrepareBssConfigFromProfile(pMac, pProfile, &bssConfig, pBssDesc);
        if(HAL_STATUS_SUCCESS(status))
        {
            //Save profile for late use
            csrFreeRoamProfile( pMac, sessionId );
            if (HAL_STATUS_SUCCESS(palAllocateMemory(pMac->hHdd, 
                                   (void **)&pSession->pCurRoamProfile,
                                   sizeof(tCsrRoamProfile))))
            {
                palZeroMemory(pMac->hHdd, pSession->pCurRoamProfile, sizeof(tCsrRoamProfile));
                csrRoamCopyProfile(pMac, pSession->pCurRoamProfile, pProfile);
            }
            //Prepare some more parameters for this WDS
            csrRoamPrepareBssParams(pMac, sessionId, pProfile, NULL, &bssConfig, NULL);
            status = csrRoamSetBssConfigCfg(pMac, sessionId, pProfile, NULL, &bssConfig, NULL);
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
        tANI_U16 i;
    // plug in neighborhood occupancy info (i.e. BSSes on primary or secondary channels)
    *pBuf++ = (tANI_U8)FALSE;  //tAniTitanCBNeighborInfo->cbBssFoundPri
    *pBuf++ = (tANI_U8)FALSE;  //tAniTitanCBNeighborInfo->cbBssFoundSecDown
    *pBuf++ = (tANI_U8)FALSE;  //tAniTitanCBNeighborInfo->cbBssFoundSecUp
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
    palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&fTmp, sizeof(tAniBool) );
    pBuf += sizeof(tAniBool);
    *pBuf++ = MIN_STA_PWR_CAP_DBM; // it is for pMsg->powerCap.minTxPower = 0;
        found = csrSearchChannelListForTxPower(pMac, pBssDescription, &channelGroup);
    // This is required for 11k test VoWiFi Ent: Test 2.
    // We need the power capabilities for Assoc Req. 
    // This macro is provided by the halPhyCfg.h. We pick our
    // max and min capability by the halPhy provided macros
    *pBuf++ = MAX_STA_PWR_CAP_DBM;
    size = sizeof(pMac->roam.validChannelList);
    if(HAL_STATUS_SUCCESS(csrGetCfgValidChannels(pMac, (tANI_U8 *)pMac->roam.validChannelList, &size)))
    { 
        *pBuf++ = (tANI_U8)size;        //tSirSupChnl->numChnl        
        for ( i = 0; i < size; i++) 
        {
            *pBuf++ = pMac->roam.validChannelList[ i ];   //tSirSupChnl->channelList[ i ]
             
        }
    }
    else
    {
        smsLog(pMac, LOGE, FL("can not find any valid channel\n"));
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
        smsLog(pMac, LOGE, FL(" BTC doesn't allow UAPSD for uapsd_mask(0x%X)\n"), uapsdMask);
        *pBuf++ = 0;
    }
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
  
    // move the entire BssDescription into the join request.
    palCopyMemory( pMac->hHdd, pBuf, pBssDescription, 
                    pBssDescription->length + sizeof( pBssDescription->length ) );
    pBuf += pBssDescription->length + sizeof( pBssDescription->length );   // update to new location
}

/* 
  * The communication between HDD and LIM is thru mailbox (MB).
  * Both sides will access the data structure "tSirSmeJoinReq".
  *  The rule is, while the components of "tSirSmeJoinReq" can be accessed in the regular way like tSirSmeJoinReq.assocType, this guideline
  *  stops at component tSirRSNie; any acces to the components after tSirRSNie is forbidden because the space from tSirRSNie is quueezed
  *  with the component "tSirBssDescription". And since the size of actual 'tSirBssDescription' varies, the receiving side (which is the routine
  *  limJoinReqSerDes() of limSerDesUtils.cc) should keep in mind not to access the components DIRECTLY after tSirRSNie.
  */
eHalStatus csrSendJoinReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDescription, 
                              tCsrRoamProfile *pProfile, tDot11fBeaconIEs *pIes )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeJoinReq *pMsg;
    tANI_U8 *pBuf;
    tANI_U16 msgLen, wTmp, ieLen;
    tSirMacRateSet OpRateSet;
    tSirMacRateSet ExRateSet;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U32 dwTmp;
    tANI_U8 wpaRsnIE[DOT11F_IE_RSN_MAX_LEN];    //RSN MAX is bigger than WPA MAX

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    do {
        pSession->joinFailStatusCode.statusCode = eSIR_SME_SUCCESS;
        pSession->joinFailStatusCode.reasonCode = 0;
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, msgLen);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_JOIN_REQ);
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
            palCopyMemory( pMac->hHdd, pBuf, pIes->SSID.ssid, pIes->SSID.num_ssid );
            pBuf += pIes->SSID.num_ssid;
        }
        else
        {
            *pBuf = 0;
            pBuf++;
        }
        // selfMacAddr
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, &pSession->selfMacAddr, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // bsstype
        dwTmp = pal_cpu_to_be32( csrTranslateBsstypeToMacType( pProfile->BSSType ) );
        if (dwTmp == eSIR_BTAMP_STA_MODE) dwTmp = eSIR_BTAMP_AP_MODE; // Override BssType for BTAMP
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tSirBssType) );
        pBuf += sizeof(tSirBssType);
        // dot11mode
        *pBuf = (tANI_U8)csrTranslateToWNICfgDot11Mode( pMac, pSession->bssParams.uCfgDot11Mode );
        pBuf++;
        //Persona
        *pBuf = (tANI_U8)pProfile->csrPersona;
        pBuf++;
        //CBMode
        *pBuf = (tANI_U8)pSession->bssParams.cbMode;
        pBuf++;

        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  FL("CSR PERSONA=%d CSR CbMode %d"), pProfile->csrPersona, pSession->bssParams.cbMode);

        // uapsdPerAcBitmask
        *pBuf = pProfile->uapsd_mask;
        pBuf++;

    
    
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)        
#endif /*(WNI_POLARIS_FW_PACKAGE == ADVANCED)*/
        status = csrGetRateSet(pMac, pProfile, (eCsrPhyMode)pProfile->phyMode, pBssDescription, pIes, &OpRateSet, &ExRateSet);
        if (HAL_STATUS_SUCCESS(status) ) 
        {
            // OperationalRateSet
            if (OpRateSet.numRates) {
                *pBuf++ = OpRateSet.numRates;
                palCopyMemory(pMac->hHdd, pBuf, OpRateSet.rate, OpRateSet.numRates);
                pBuf += OpRateSet.numRates;
            } else *pBuf++ = 0;
            // ExtendedRateSet
            if (ExRateSet.numRates) {
                *pBuf++ = ExRateSet.numRates;
                palCopyMemory(pMac->hHdd, pBuf, ExRateSet.rate, ExRateSet.numRates);
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
                smsLog(pMac, LOGE, FL(" WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d\n"), ieLen, DOT11F_IE_RSN_MAX_LEN);
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
                        palFreeMemory(pMac->hHdd, pSession->pWapiReqIE);
                    }
                    status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWapiReqIE, ieLen);
                    if(!HAL_STATUS_SUCCESS(status)) break;
                }
                pSession->nWapiReqIeLength = ieLen;
                palCopyMemory(pMac->hHdd, pSession->pWapiReqIE, wpaRsnIE, ieLen);
                wTmp = pal_cpu_to_be16( ieLen );
                palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
                pBuf += sizeof(tANI_U16);
                palCopyMemory( pMac->hHdd, pBuf, wpaRsnIE, ieLen );
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
                        palFreeMemory(pMac->hHdd, pSession->pWpaRsnReqIE);
                    }
                    status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWpaRsnReqIE, ieLen);
                    if(!HAL_STATUS_SUCCESS(status)) break;
                }
                pSession->nWpaRsnReqIeLength = ieLen;
                palCopyMemory(pMac->hHdd, pSession->pWpaRsnReqIE, wpaRsnIE, ieLen);
                wTmp = pal_cpu_to_be16( ieLen );
                palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
                pBuf += sizeof(tANI_U16);
                palCopyMemory( pMac->hHdd, pBuf, wpaRsnIE, ieLen );
                pBuf += ieLen;
            }
        }
        else
        {
            //free whatever old info
            pSession->nWpaRsnReqIeLength = 0;
            if(pSession->pWpaRsnReqIE)
            {
                palFreeMemory(pMac->hHdd, pSession->pWpaRsnReqIE);
                pSession->pWpaRsnReqIE = NULL;
            }
#ifdef FEATURE_WLAN_WAPI
            pSession->nWapiReqIeLength = 0;
            if(pSession->pWapiReqIE)
            {
                palFreeMemory(pMac->hHdd, pSession->pWapiReqIE);
                pSession->pWapiReqIE = NULL;
            }
#endif /* FEATURE_WLAN_WAPI */
            //length is two bytes
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
#ifdef FEATURE_WLAN_CCX
        // Never include the cckmIE in an Join Request
        //length is two bytes
        *pBuf = 0;
        *(pBuf + 1) = 0;
        pBuf += 2;
#endif 
        // addIEScan
        if(pProfile->nAddIEScanLength && pProfile->pAddIEScan)
        {
            ieLen = pProfile->nAddIEScanLength;
            if(ieLen > pSession->nAddIEScanLength)
            {
                if(pSession->pAddIEScan && pSession->nAddIEScanLength)
        {
                    palFreeMemory(pMac->hHdd, pSession->pAddIEScan);
                }
                status = palAllocateMemory(pMac->hHdd, 
                                 (void **)&pSession->pAddIEScan, ieLen);
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nAddIEScanLength = ieLen;
            palCopyMemory(pMac->hHdd, pSession->pAddIEScan, 
                                      pProfile->pAddIEScan, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, pProfile->pAddIEScan, ieLen );
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEScanLength = 0;
            if(pSession->pAddIEScan)
            {
                palFreeMemory(pMac->hHdd, pSession->pAddIEScan);
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
                    palFreeMemory(pMac->hHdd, pSession->pAddIEAssoc);
                }
                status = palAllocateMemory(pMac->hHdd, 
                                 (void **)&pSession->pAddIEAssoc, ieLen);
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nAddIEAssocLength = ieLen;
            palCopyMemory(pMac->hHdd, pSession->pAddIEAssoc, 
                                      pProfile->pAddIEAssoc, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, pProfile->pAddIEAssoc, ieLen );
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEAssocLength = 0;
            if(pSession->pAddIEAssoc)
            {
                palFreeMemory(pMac->hHdd, pSession->pAddIEAssoc);
                pSession->pAddIEAssoc = NULL;
            }
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedUCEncryptionType) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tANI_U32) );
        pBuf += sizeof(tANI_U32);        
        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedMCEncryptionType) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tANI_U32) );
        pBuf += sizeof(tANI_U32);        
#ifdef WLAN_FEATURE_VOWIFI_11R
        if (csrIsProfile11r( pProfile ) )
        {
            // is11Rconnection;
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else
        {
            // is11Rconnection;
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
#endif
#ifdef FEATURE_WLAN_CCX
        /* A profile can not be both CCX and 11R. But an 802.11R AP
         * may be advertising support for CCX as well. So if we are 
         * associating Open or explicitly CCX then we will get CCX.
         * If we are associating explictly 11R only then we will get
         * 11R.
         */
        if ((csrIsProfileCCX(pProfile) || 
             ((pIes->CCXVersion.present) 
              && ((pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM) 
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA) 
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA_PSK) 
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN) 
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN_PSK)))) 
            && (!(csrIsProfile11r( pProfile ))) 
            && (pMac->roam.configParam.isCcxIniFeatureEnabled))
        {
            // isCCXconnection;
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else 
        {
            //isCCXconnection;
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        {
            tCCXTspecInfo ccxTspec;
            // CCX-Tspec IEs in the ASSOC request is presently not supported
            // so nullify the TSPEC parameters
            palZeroMemory(pMac->hHdd, &ccxTspec, sizeof(tCCXTspecInfo));
            palCopyMemory( pMac->hHdd, pBuf, &ccxTspec, sizeof(tCCXTspecInfo));
            pBuf += sizeof(tCCXTspecInfo);
        }
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
        // Fill in isFastTransitionEnabled
        if (pMac->roam.configParam.isFastTransitionEnabled
#ifdef FEATURE_WLAN_LFR
        || csrRoamIsFastRoamEnabled(pMac)
#endif
        )
        {
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
#endif
#ifdef FEATURE_WLAN_LFR
        if(csrRoamIsFastRoamEnabled(pMac))
        {
            //legacy fast roaming enabled
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
#endif
        //BssDesc
        csrPrepareJoinReassocReqBuffer( pMac, pBssDescription, pBuf, 
                (tANI_U8)pProfile->uapsd_mask);
        status = palSendMBMessage(pMac->hHdd, pMsg );    
        if(!HAL_STATUS_SUCCESS(status)) 
        {
            break;
        }
        //Tush-QoS: notify QoS module that join happening
        else
        {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
            sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_JOIN_REQ, NULL);
#endif
        }
    } while( 0 );
    return( status );
}

eHalStatus csrSendSmeReassocReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirBssDescription *pBssDescription, 
                                    tDot11fBeaconIEs *pIes, tCsrRoamProfile *pProfile )
{
    eHalStatus status;
    tSirSmeReassocReq *pMsg;
    tANI_U8 *pBuf;
    v_U8_t acm_mask = 0, uapsd_mask;
    tANI_U16 msgLen, ieLen, wTmp;
    tANI_U32 dwTmp;
    tSirMacRateSet OpRateSet;
    tSirMacRateSet ExRateSet;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    tANI_U8 wpaRsnIE[DOT11F_IE_RSN_MAX_LEN];    //RSN MAX is bigger than WPA MAX

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return eHAL_STATUS_FAILURE;
    }
    
    /* To satisfy klockworks */
    if (pBssDescription == NULL)
    {
        smsLog(pMac, LOGE, FL(" pBssDescription is NULL\n"));
        return eHAL_STATUS_FAILURE;
    }
    do {
        // There are a number of variable length fields to consider.  First, the tSirSmeJoinReq
        // includes a single bssDescription.   bssDescription includes a single tANI_U32 for the 
        // IE fields, but the length field in the bssDescription needs to be interpreted to 
        // determine length of the IE fields.
        //
        // So, take the size of the JoinReq, subtract the size of the bssDescription and 
        // add in the length from the bssDescription (then add the size of the 'length' field
        // itself because that is NOT included in the length field).
        msgLen = sizeof( tSirSmeReassocReq ) - sizeof( *pBssDescription ) + 
                 pBssDescription->length + sizeof( pBssDescription->length ) +
                 sizeof( tCsrWpaIe ) + sizeof( tCsrWpaAuthIe ) + sizeof( tANI_U16 ); // add in the size of the WPA IE that we may build.
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);         
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, msgLen);
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_REASSOC_REQ);
                pMsg->length = pal_cpu_to_be16(msgLen);
        pBuf = &pMsg->sessionId;
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // transactionId
        *pBuf = 0;
        *(pBuf + 1) = 0;
        pBuf += sizeof (tANI_U16);
        // ssId
        if( pIes->SSID.present && pIes->SSID.num_ssid )
        {
            // ssId len
            *pBuf++ = pIes->SSID.num_ssid;
            palCopyMemory( pMac->hHdd, pBuf, pIes->SSID.ssid, pIes->SSID.num_ssid );
            pBuf += pIes->SSID.num_ssid;
        } 
        else
        {
            *pBuf++ = 0;
        }
        // selfMacAddr
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, &pSession->selfMacAddr, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // bsstype
        dwTmp = pal_cpu_to_be32( csrTranslateBsstypeToMacType( pProfile->BSSType ) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tSirBssType) );
        pBuf += sizeof(tSirBssType);
        // dot11mode
        *pBuf = (tANI_U8)csrTranslateToWNICfgDot11Mode( pMac, pSession->bssParams.uCfgDot11Mode );
        pBuf++;
        //Persona
        *pBuf = (tANI_U8)pProfile->csrPersona;
        pBuf++;
        //CBMode
        *pBuf = (tANI_U8)pSession->bssParams.cbMode;
        pBuf++;

        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
            FL("CSR PERSONA=%d CSR CBMode=%u\n"), pProfile->csrPersona, pSession->bssParams.cbMode);

        // uapsdPerAcBitmask
        *pBuf = pProfile->uapsd_mask;
        pBuf++;

        status = csrGetRateSet(pMac, pProfile, (eCsrPhyMode)pProfile->phyMode, 
                        pBssDescription, pIes, &OpRateSet, &ExRateSet);
        if (HAL_STATUS_SUCCESS(status) ) 
        {
            // OperationalRateSet
            if (OpRateSet.numRates) 
            {
                *pBuf++ = OpRateSet.numRates;
                palCopyMemory(pMac->hHdd, pBuf, OpRateSet.rate, OpRateSet.numRates);
                pBuf += OpRateSet.numRates;
            } 
            else *pBuf++ = 0;
            // ExtendedRateSet
            if (ExRateSet.numRates) 
            {
                *pBuf++ = ExRateSet.numRates;
                palCopyMemory(pMac->hHdd, pBuf, ExRateSet.rate, ExRateSet.numRates);
                pBuf += ExRateSet.numRates;
            } 
            else *pBuf++ = 0;
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
                                                (tCsrWapiIe *)( wpaRsnIE) );
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
                 smsLog(pMac, LOGE, FL(" WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d\n"), ieLen, DOT11F_IE_RSN_MAX_LEN);
                 ieLen = DOT11F_IE_RSN_MAX_LEN;
             }
            //Check whether we need to allocate more memory
            if(ieLen > pSession->nWpaRsnReqIeLength)
            {
                if(pSession->pWpaRsnReqIE && pSession->nWpaRsnReqIeLength)
                {
                    palFreeMemory(pMac->hHdd, pSession->pWpaRsnReqIE);
                }
                status = palAllocateMemory(pMac->hHdd, (void **)&pSession->pWpaRsnReqIE, ieLen);
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nWpaRsnReqIeLength = ieLen;
            palCopyMemory(pMac->hHdd, pSession->pWpaRsnReqIE, wpaRsnIE, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, wpaRsnIE, ieLen );
            pBuf += ieLen;
                }
                else
                {
            //free whatever old info
            pSession->nWpaRsnReqIeLength = 0;
            if(pSession->pWpaRsnReqIE)
            {
                palFreeMemory(pMac->hHdd, pSession->pWpaRsnReqIE);
                pSession->pWpaRsnReqIE = NULL;
            }
            //length is two bytes
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
                }
#ifdef FEATURE_WLAN_CCX
        // cckmIE
        if( csrIsProfileCCX( pProfile ) )
        {
            // Insert the CCKM IE into the join request
            ieLen = csrConstructCcxCckmIe( pMac, 
                                          pSession, 
                                          pProfile, 
                                          pBssDescription, 
                                          pSession->pWpaRsnReqIE, 
                                          pSession->nWpaRsnReqIeLength,
                                          (void *)( wpaRsnIE ) );
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
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, wpaRsnIE, ieLen );
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
#endif /* FEATURE_WLAN_CCX */
        // addIEScan
        if(pProfile->nAddIEScanLength && pProfile->pAddIEScan)
        {
            ieLen = pProfile->nAddIEScanLength;
            if(ieLen > pSession->nAddIEScanLength)
            {
                if(pSession->pAddIEScan && pSession->nAddIEScanLength)
                {
                    palFreeMemory(pMac->hHdd, pSession->pAddIEScan);
                }
                status = palAllocateMemory(pMac->hHdd, 
                                 (void **)&pSession->pAddIEScan, ieLen);
                if(!HAL_STATUS_SUCCESS(status)) break;
            }
            pSession->nAddIEScanLength = ieLen;
            palCopyMemory(pMac->hHdd, pSession->pAddIEScan, 
                                      pProfile->pAddIEScan, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, pProfile->pAddIEScan, ieLen );
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEScanLength = 0;
            if(pSession->pAddIEScan)
            {
                palFreeMemory(pMac->hHdd, pSession->pAddIEScan);
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
                    palFreeMemory(pMac->hHdd, pSession->pAddIEAssoc);
        }
                status = palAllocateMemory(pMac->hHdd, 
                                 (void **)&pSession->pAddIEAssoc, ieLen);
                if(!HAL_STATUS_SUCCESS(status)) break;
        }
            pSession->nAddIEAssocLength = ieLen;
            palCopyMemory(pMac->hHdd, pSession->pAddIEAssoc, 
                                      pProfile->pAddIEAssoc, ieLen);
            wTmp = pal_cpu_to_be16( ieLen );
            palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
            pBuf += sizeof(tANI_U16);
            palCopyMemory( pMac->hHdd, pBuf, pProfile->pAddIEAssoc, ieLen );
            pBuf += ieLen;
        }
        else
        {
            pSession->nAddIEAssocLength = 0;
            if(pSession->pAddIEAssoc)
            {
                palFreeMemory(pMac->hHdd, pSession->pAddIEAssoc);
                pSession->pAddIEAssoc = NULL;
            }
            *pBuf = 0;
            *(pBuf + 1) = 0;
            pBuf += 2;
        }
        //Unmask any AC in reassoc that is ACM-set
        uapsd_mask = (v_U8_t)pProfile->uapsd_mask;
        if( uapsd_mask && ( NULL != pBssDescription ) )
        {
            if( CSR_IS_QOS_BSS(pIes) && CSR_IS_UAPSD_BSS(pIes) )
            {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
                acm_mask = sme_QosGetACMMask(pMac, pBssDescription, pIes);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
                uapsd_mask &= ~(acm_mask);
            }
            else
            {
                uapsd_mask = 0;
            }
        }
        
        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedUCEncryptionType) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tANI_U32) );
        pBuf += sizeof(tANI_U32);        
        
        dwTmp = pal_cpu_to_be32( csrTranslateEncryptTypeToEdType( pProfile->negotiatedMCEncryptionType) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tANI_U32) );
        pBuf += sizeof(tANI_U32);        
#ifdef WLAN_FEATURE_VOWIFI_11R
        // is11Rconnection;
        dwTmp = csrIsProfile11r( pProfile )?  pal_cpu_to_be32(TRUE) : 0; 
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
        pBuf += sizeof(tAniBool);        
#ifdef FEATURE_WLAN_CCX
        //isCCXconnection;
        //CCKM profile, ccxversion ie present, not 11r and ini file has CCX enabled
        dwTmp = ( ((csrIsProfileCCX(pProfile) || 
                  ((pIes->CCXVersion.present) 
                   && ((pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM) 
                       || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA)
                       || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA_PSK)
                       || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN)
                       || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN_PSK)))) 
                 && (!(csrIsProfile11r( pProfile ))) 
                 && (pMac->roam.configParam.isCcxIniFeatureEnabled)) 
                ? pal_cpu_to_be32(TRUE) : 0);
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
        pBuf += sizeof(tAniBool);        
#endif // FEATURE_WLAN_CCX
#endif // WLAN_FEATURE_VOWIFI_11R
#ifdef FEATURE_WLAN_CCX
        if ((csrIsProfileCCX(pProfile) || 
             ((pIes->CCXVersion.present) 
              && ((pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM) 
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA)
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_WPA_PSK)
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN)
                  || (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_RSN_PSK)))) 
            && (!(csrIsProfile11r( pProfile ))) 
            && (pMac->roam.configParam.isCcxIniFeatureEnabled))
        {
           tCCXTspecInfo ccxTspec;
           // CCX Tspec information
           palZeroMemory(pMac->hHdd, &ccxTspec, sizeof(tCCXTspecInfo));
           ccxTspec.numTspecs = sme_QosCCxRetrieveTspecInfo(pMac, sessionId, (tTspecInfo *) &ccxTspec.tspec[0]);
           *pBuf = ccxTspec.numTspecs;
           pBuf += sizeof(tANI_U8);
           // Copy the TSPEC information only if present
           if (ccxTspec.numTspecs) {
               palCopyMemory(pMac->hHdd, pBuf, (void*)&ccxTspec.tspec[0], (ccxTspec.numTspecs*sizeof(tTspecInfo)));
           }
           pBuf += sizeof(ccxTspec.tspec);
        }
        else 
        {
            {
                tCCXTspecInfo ccxTspec;
                // CCX-Tspec IEs in the ASSOC request is presently not supported
                // so nullify the TSPEC parameters
                palZeroMemory(pMac->hHdd, &ccxTspec, sizeof(tCCXTspecInfo));
                palCopyMemory( pMac->hHdd, pBuf, &ccxTspec, sizeof(tCCXTspecInfo));
                pBuf += sizeof(tCCXTspecInfo);
            }
        }
#endif // FEATURE_WLAN_CCX
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
        // Fill in isFastTransitionEnabled
        if (pMac->roam.configParam.isFastTransitionEnabled
#ifdef FEATURE_WLAN_LFR
         || csrRoamIsFastRoamEnabled(pMac)
#endif
         )
        {
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
#endif
#ifdef FEATURE_WLAN_LFR
        if(csrRoamIsFastRoamEnabled(pMac))
        {
            //legacy fast roaming enabled
            dwTmp = pal_cpu_to_be32(TRUE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
        else
        {
            dwTmp = pal_cpu_to_be32(FALSE); 
            palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tAniBool) );
            pBuf += sizeof(tAniBool);        
        }
#endif
        csrPrepareJoinReassocReqBuffer( pMac, pBssDescription, pBuf, uapsd_mask);
        
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
        //Tush-QoS: notify QoS module that reassoc happening
        sme_QosCsrEventInd(pMac, (v_U8_t)sessionId, SME_QOS_CSR_REASSOC_REQ, NULL);
#endif
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

//
eHalStatus csrSendMBDisassocReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr bssId, tANI_U16 reasonCode )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDisassocReq *pMsg;
    tANI_U8 *pBuf;
    tANI_U16 wTmp;
#ifdef WLAN_SOFTAP_FEATURE
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
        return eHAL_STATUS_FAILURE;
#endif
    do {
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeDisassocReq ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeDisassocReq ));
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DISASSOC_REQ);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDisassocReq ));
        pBuf = &pMsg->sessionId;
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
     
#ifdef WLAN_SOFTAP_FEATURE        
        if ( (pSession->pCurRoamProfile != NULL ) && 
             ( reasonCode == eSIR_MAC_UNSPEC_FAILURE_REASON ) && 
             ((CSR_IS_INFRA_AP(pSession->pCurRoamProfile)) || (CSR_IS_WDS_AP(pSession->pCurRoamProfile))))
        {
            // Set the bssid address before sending the message to LIM
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, pSession->selfMacAddr, sizeof( tSirMacAddr ) );
            pBuf = pBuf + sizeof ( tSirMacAddr );
            // Set the peer MAC address before sending the message to LIM
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof( tSirMacAddr ) ); //perMacAddr is passed as bssId for softAP
            pBuf = pBuf + sizeof ( tSirMacAddr );
        }
        else
        {
#endif
            // Set the peer MAC address before sending the message to LIM
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof( tSirMacAddr ) );
            pBuf = pBuf + sizeof ( tSirMacAddr );
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof( pMsg->bssId ) );
            pBuf = pBuf + sizeof ( tSirMacAddr );
#ifdef WLAN_SOFTAP_FEATURE
        }
#endif
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }
        // reasonCode
        wTmp = pal_cpu_to_be16(reasonCode);
        status = palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }
        pBuf += sizeof(tANI_U16);
        /* The state will be DISASSOC_HANDOFF only when we are doing handoff. 
                    Here we should not send the disassoc over the air to the AP */
        if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(pMac, sessionId)
#ifdef WLAN_FEATURE_VOWIFI_11R
                && csrRoamIs11rAssoc(pMac)
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
#ifdef WLAN_SOFTAP_FEATURE
eHalStatus csrSendMBTkipCounterMeasuresReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_BOOLEAN bEnable, tSirMacAddr bssId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeTkipCntrMeasReq *pMsg;
    tANI_U8 *pBuf;
    do
    {
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeTkipCntrMeasReq ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeTkipCntrMeasReq ));
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
        status = palCopyMemory( pMac->hHdd, pMsg->bssId, bssId, sizeof( tSirMacAddr ) );
        pBuf = pBuf + sizeof ( tSirMacAddr );
        // bEnable
        *pBuf = (tANI_BOOLEAN)bEnable;
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
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
        status = palAllocateMemory( pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeGetAssocSTAsReq ) );
        if (!HAL_STATUS_SUCCESS(status)) break;
        palZeroMemory( pMac->hHdd, pMsg, sizeof( tSirSmeGetAssocSTAsReq ) );
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_ASSOC_STAS_REQ);
        pBuf = (tANI_U8 *)&pMsg->bssId;
        wTmpBuf = pBuf;
        // bssId
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // modId 
        dwTmp = pal_cpu_to_be16((tANI_U16)modId);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U16));
        pBuf += sizeof(tANI_U16);
        // pUsrContext
        dwTmp = pal_cpu_to_be32((tANI_U32)pUsrContext);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // pfnSapEventCallback
        dwTmp = pal_cpu_to_be32((tANI_U32)pfnSapEventCallback);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // pAssocStasBuf
        dwTmp = pal_cpu_to_be32((tANI_U32)pAssocStasBuf);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
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
    tANI_U32 dwTmp;
    do
        {
        status = palAllocateMemory( pMac->hHdd, (void **)&pMsg, sizeof(tSirSmeGetWPSPBCSessionsReq) );
        if (!HAL_STATUS_SUCCESS(status)) break;
        palZeroMemory( pMac->hHdd, pMsg, sizeof( tSirSmeGetWPSPBCSessionsReq ) );
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_WPSPBC_SESSION_REQ);
        pBuf = (tANI_U8 *)&pMsg->pUsrContext;
        wTmpBuf = pBuf;
        // pUsrContext
        dwTmp = pal_cpu_to_be32((tANI_U32)pUsrContext);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // pSapEventCallback
        dwTmp = pal_cpu_to_be32((tANI_U32)pfnSapEventCallback);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // bssId
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // MAC Address of STA in WPS session
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, pRemoveMac.bytes, sizeof(v_MACADDR_t));
        pBuf += sizeof(v_MACADDR_t);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf)));//msg_header + msg
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeDeauthReq ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeDeauthReq ));
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
#ifdef WLAN_SOFTAP_FEATURE
             (CSR_IS_INFRA_AP(pSession->pCurRoamProfile)) || 
#endif
             (CSR_IS_WDS_AP(pSession->pCurRoamProfile)))){ 
            // Set the BSSID before sending the message to LIM
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, pSession->selfMacAddr, sizeof( pMsg->peerMacAddr ) );
            pBuf =  pBuf + sizeof(tSirMacAddr);
        }
        else
        {
            // Set the BSSID before sending the message to LIM
            status = palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, bssId, sizeof( pMsg->peerMacAddr ) );
            pBuf =  pBuf + sizeof(tSirMacAddr);
        }
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }     
                // Set the peer MAC address before sending the message to LIM
        status = palCopyMemory( pMac->hHdd, (tSirMacAddr *) pBuf, bssId, sizeof( pMsg->peerMacAddr ) );
        pBuf =  pBuf + sizeof(tSirMacAddr);
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }     
        wTmp = pal_cpu_to_be16(reasonCode);
        status = palCopyMemory( pMac->hHdd, pBuf, &wTmp,sizeof( tANI_U16 ) );
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeDisassocCnf ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeDisassocCnf ));
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DISASSOC_CNF);
        pMsg->statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDisassocCnf ));
        status = palCopyMemory(pMac->hHdd, pMsg->peerMacAddr, pDisassocInd->peerMacAddr, sizeof(pMsg->peerMacAddr)); 
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }
//To test reconn        
        status = palCopyMemory(pMac->hHdd, pMsg->bssId, pDisassocInd->bssId, sizeof(pMsg->peerMacAddr)); 
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }
//To test reconn ends
        status = palSendMBMessage( pMac->hHdd, pMsg );
    } while( 0 );
    return( status );
}

eHalStatus csrSendMBDeauthCnfMsg( tpAniSirGlobal pMac, tpSirSmeDeauthInd pDeauthInd )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeDeauthCnf *pMsg;
    do {
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeDeauthCnf ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeDeauthCnf ));
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DEAUTH_CNF);
        pMsg->statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeDeauthCnf ));
        status = palCopyMemory(pMac->hHdd, pMsg->bssId, pDeauthInd->bssId, sizeof(pMsg->bssId)); 
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
            break;
        }
        status = palCopyMemory(pMac->hHdd, pMsg->peerMacAddr, pDeauthInd->peerMacAddr, sizeof(pMsg->peerMacAddr)); 
        if(!HAL_STATUS_SUCCESS(status))
        {
            palFreeMemory(pMac->hHdd, pMsg);
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
    do {
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeAssocCnf ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeAssocCnf ));
                pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_ASSOC_CNF);
                pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeAssocCnf ));
        pBuf = (tANI_U8 *)&pMsg->statusCode;
        if(HAL_STATUS_SUCCESS(Halstatus))
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        else
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_ASSOC_REFUSED);
        palCopyMemory( pMac->hHdd, pBuf, &statusCode, sizeof(tSirResultCodes) );
        pBuf += sizeof(tSirResultCodes);
        // bssId
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // peerMacAddr
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->peerMacAddr, sizeof(tSirMacAddr)); 
        pBuf += sizeof (tSirMacAddr);
        // aid
        wTmp = pal_cpu_to_be16(pAssocInd->aid);
        palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
        pBuf += sizeof (tANI_U16);
        // alternateBssId
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
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
#ifdef WLAN_SOFTAP_FEATURE
eHalStatus csrSendAssocIndToUpperLayerCnfMsg(   tpAniSirGlobal pMac, 
                                                tpSirSmeAssocInd pAssocInd, 
                                                eHalStatus Halstatus, 
                                                tANI_U8 sessionId)
{
    tSirMsgQ            msgQ;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tSirSmeAssocIndToUpperLayerCnf *pMsg;
    tANI_U8 *pBuf;
    tSirResultCodes statusCode;
    tANI_U16 wTmp;
    do {
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof( tSirSmeAssocIndToUpperLayerCnf ));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeAssocIndToUpperLayerCnf ));

        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_UPPER_LAYER_ASSOC_CNF);
        pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeAssocIndToUpperLayerCnf ));

        pMsg->sessionId = sessionId;

        pBuf = (tANI_U8 *)&pMsg->statusCode;
        if(HAL_STATUS_SUCCESS(Halstatus))
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_SUCCESS);
        else
            statusCode = (tSirResultCodes)pal_cpu_to_be32(eSIR_SME_ASSOC_REFUSED);
        palCopyMemory( pMac->hHdd, pBuf, &statusCode, sizeof(tSirResultCodes) );
        pBuf += sizeof(tSirResultCodes);
        // bssId
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // peerMacAddr
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->peerMacAddr, sizeof(tSirMacAddr)); 
        pBuf += sizeof (tSirMacAddr);
        // StaId
        wTmp = pal_cpu_to_be16(pAssocInd->staId);
        palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
        pBuf += sizeof (tANI_U16);
        // alternateBssId
        status = palCopyMemory(pMac->hHdd, (tSirMacAddr *)pBuf, pAssocInd->bssId, sizeof(tSirMacAddr));
        pBuf += sizeof (tSirMacAddr);
        // alternateChannelId
        *pBuf = 11;
        pBuf += sizeof (tANI_U8);
        // Instead of copying roam Info, we just copy only WmmEnabled , RsnIE information
        //Wmm
        *pBuf = pAssocInd->wmmEnabledSta;
        pBuf += sizeof (tANI_U8);
        //RSN IE
        status = palCopyMemory(pMac->hHdd, (tSirRSNie *)pBuf, &pAssocInd->rsnIE, sizeof(tSirRSNie));
        pBuf += sizeof (tSirRSNie);
        //Additional IE
        status = palCopyMemory(pMac->hHdd, (void *)pBuf, &pAssocInd->addIE, sizeof(tSirAddie));
        pBuf += sizeof (tSirAddie);
        //reassocReq
        *pBuf = pAssocInd->reassocReq;
        pBuf += sizeof (tANI_U8);
        msgQ.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
        msgQ.bodyptr = pMsg;
        msgQ.bodyval = 0;
        SysProcessMmhMsg(pMac, &msgQ);
    } while( 0 );
    return( status );
}
#endif

eHalStatus csrSendMBSetContextReqMsg( tpAniSirGlobal pMac, tANI_U32 sessionId ,
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
    tANI_U8 *pBuf;
    tANI_U8 *p;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    do {
        if( ( 1 != numKeys ) && ( 0 != numKeys ) ) break;
        // all of these fields appear in every SET_CONTEXT message.  Below we'll add in the size for each 
        // key set. Since we only support upto one key, we always allocate memory for 1 key
        msgLen  = sizeof( tANI_U16) + sizeof( tANI_U16 ) + sizeof( tSirMacAddr ) +
                  sizeof( tSirMacAddr ) + 1 + sizeof(tANI_U16) +
                  sizeof( pMsg->keyMaterial.length ) + sizeof( pMsg->keyMaterial.edType ) + sizeof( pMsg->keyMaterial.numKeys ) +
                  ( sizeof( pMsg->keyMaterial.key ) );
                     
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, msgLen);
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
        palCopyMemory( pMac->hHdd, pBuf, 
             (tANI_U8 *)peerMacAddr, sizeof(tSirMacAddr) );

        pBuf += sizeof(tSirMacAddr);

        // bssId
        palCopyMemory( pMac->hHdd, pBuf, 
            (tANI_U8 *)&pSession->connectedProfile.bssid, sizeof(tSirMacAddr) );

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
        palCopyMemory( pMac->hHdd, p, (tANI_U8 *)&tmpEdType, sizeof(tAniEdType) );
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
        palCopyMemory( pMac->hHdd, p, (tANI_U8 *)&tmpDirection, sizeof(tAniKeyDirection) );
        p += sizeof(tAniKeyDirection);
        //    pSirKey->keyRsc = ;;
        palCopyMemory( pMac->hHdd, p, pKeyRsc, CSR_MAX_RSC_LEN );
        p += sizeof( pMsg->keyMaterial.key[ 0 ].keyRsc );
                // set pSirKey->paeRole
                *p = paeRole;   // 0 is Supplicant
                p++;
                // set pSirKey->keyLength = keyLength;
                p = pal_set_U16( p, pal_cpu_to_be16(keyLength) );
        if ( keyLength && pKey ) 
        {   
            palCopyMemory( pMac->hHdd, p, pKey, keyLength ); 
            if(keyLength == 16)
            {
                smsLog(pMac, LOGE, "  SME Set keyIdx (%d) encType(%d) key = %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
                keyId, edType, pKey[0], pKey[1], pKey[2], pKey[3], pKey[4],
                pKey[5], pKey[6], pKey[7], pKey[8],
                pKey[9], pKey[10], pKey[11], pKey[12], pKey[13], pKey[14], pKey[15]);
            }
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
#ifdef WLAN_SOFTAP_FEATURE
    tANI_U32 authType;
#endif
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
        status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, msgLen);
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
        palCopyMemory( pMac->hHdd, pBuf, pParam->bssid, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // selfMacAddr
        palCopyMemory( pMac->hHdd, pBuf, pSession->selfMacAddr, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // beaconInterval
        if( pBssDesc && pBssDesc->beaconInterval )
        {
            wTmp = pal_cpu_to_be16( pBssDesc->beaconInterval );
        }
#ifdef WLAN_SOFTAP_FEATURE
        else if(pParam->beaconInterval)
        {
            wTmp = pal_cpu_to_be16( pParam->beaconInterval );
        }
#endif
        else
        {
            wTmp = pal_cpu_to_be16( WNI_CFG_BEACON_INTERVAL_STADEF );
        }
        if(csrIsconcurrentsessionValid (pMac, sessionId, 
                                   pParam->bssPersona) 
                                   == eHAL_STATUS_SUCCESS )
        {    
           csrValidateBeaconInterval(pMac, pParam->operationChn, &wTmp, sessionId,
                                      pParam->bssPersona);
           //Update the beacon Interval 
           pParam->beaconInterval = wTmp;
        }
        else
        {
            smsLog( pMac,LOGE, FL("****Start BSS failed persona already exists***\n"));
            status = eHAL_STATUS_FAILURE;
            return status;
        }

        palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof( tANI_U16 ) ); 
        pBuf += sizeof(tANI_U16);
        // dot11mode
        *pBuf = (tANI_U8)csrTranslateToWNICfgDot11Mode( pMac, pParam->uCfgDot11Mode );
        pBuf += 1;
        // bssType
        dwTmp = pal_cpu_to_be32( csrTranslateBsstypeToMacType( bssType ) );
        palCopyMemory( pMac->hHdd, pBuf, &dwTmp, sizeof(tSirBssType) );
        pBuf += sizeof(tSirBssType);
        // ssId
        if( pParam->ssId.length )
        {
            // ssId len
            *pBuf = pParam->ssId.length;
            pBuf++;
            palCopyMemory( pMac->hHdd, pBuf, pParam->ssId.ssId, pParam->ssId.length );
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
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&cbMode, sizeof(ePhyChanBondState) );
        pBuf += sizeof(ePhyChanBondState);

#ifdef WLAN_SOFTAP_FEATURE
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
        palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof( tANI_U16 ) ); 
        pBuf += sizeof(tANI_U16);
        // Set Auth type
        authType = pal_cpu_to_be32(pParam->authType);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&authType, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // Set DTIM
        dwTmp = pal_cpu_to_be32(pParam->dtimPeriod);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&dwTmp, sizeof(tANI_U32));
        pBuf += sizeof(tANI_U32);
        // Set wps_state
        *pBuf = pParam->wps_state;
        pBuf++;
#endif
        //Persona
        *pBuf = (tANI_U8)pParam->bssPersona;
        pBuf++;
        
        
        
        // set RSN IE 
        if( pParam->nRSNIELength > sizeof(pMsg->rsnIE.rsnIEdata) )
        {
            status = eHAL_STATUS_INVALID_PARAMETER;
            palFreeMemory( pMac->hHdd, pMsg );
            break;
        }
        wTmp = pal_cpu_to_be16( pParam->nRSNIELength );
        palCopyMemory( pMac->hHdd, pBuf, &wTmp, sizeof(tANI_U16) );
        pBuf += sizeof(tANI_U16);
        if( wTmp )
        {
            wTmp = pParam->nRSNIELength;
            palCopyMemory( pMac->hHdd, pBuf, pParam->pRSNIE, wTmp );
            pBuf += wTmp;
        }
        nwType = (tSirNwType)pal_cpu_to_be32(pParam->sirNwType);
        palCopyMemory( pMac->hHdd, pBuf, (tANI_U8 *)&nwType, sizeof(tSirNwType) );
        pBuf += sizeof(tSirNwType);
        *pBuf = pParam->operationalRateSet.numRates; //tSirMacRateSet->numRates
        pBuf++;
        palCopyMemory( pMac->hHdd, pBuf, pParam->operationalRateSet.rate, pParam->operationalRateSet.numRates );
        pBuf += pParam->operationalRateSet.numRates ;
        *pBuf++ = pParam->extendedRateSet.numRates;
        if(0 != pParam->extendedRateSet.numRates)
        {
            palCopyMemory( pMac->hHdd, pBuf, pParam->extendedRateSet.rate, pParam->extendedRateSet.numRates );
            pBuf += pParam->extendedRateSet.numRates;
        }
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
        status = palAllocateMemory(pMac, (void **)&pMsg, sizeof(tSirSmeStopBssReq));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeStopBssReq ));
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
            palCopyMemory( pMac->hHdd, pBuf,(tANI_U8 *)&pSession->selfMacAddr,  sizeof(tSirMacAddr) );                
        }
        else
        {
            palCopyMemory( pMac->hHdd, pBuf,(tANI_U8 *)&pSession->connectedProfile.bssid,  sizeof(tSirMacAddr) );              
        }
       pBuf += sizeof(tSirMacAddr);
       msgLen = sizeof(tANI_U16) + sizeof(tANI_U16) + 1 + sizeof(tANI_U16) + sizeof(tSirResultCodes) + sizeof(tSirMacAddr);
       pMsg->length =  pal_cpu_to_be16(msgLen);
       status =  palSendMBMessage( pMac->hHdd, pMsg );
#if 0            
        status = palAllocateMemory(pMac, (void **)&pMsg, sizeof(tSirSmeStopBssReq));
        if ( !HAL_STATUS_SUCCESS(status) ) break;
        palZeroMemory(pMac->hHdd, pMsg, sizeof( tSirSmeStopBssReq ));
                pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_STOP_BSS_REQ);
                pMsg->reasonCode = 0;
        // bssid
        // if BSSType is WDS sta, use selfmacAddr as bssid, else use bssid in connectedProfile
        if( CSR_IS_CONN_WDS_STA(&pSession->connectedProfile) )
        {
            pbBssid = (tANI_U8 *)&pSession->selfMacAddr;
        }
        else
        {
            pbBssid = (tANI_U8 *)&pSession->connectedProfile.bssid;
        }
        palCopyMemory( pMac->hHdd, &pMsg->bssId, pbBssid, sizeof(tSirMacAddr) );
        pMsg->transactionId = 0;
        pMsg->sessionId = (tANI_U8)sessionId;
                pMsg->length = pal_cpu_to_be16((tANI_U16)sizeof( tSirSmeStopBssReq ));
                status = palSendMBMessage( pMac->hHdd, pMsg );
#endif                
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
      (fForce || (!palEqualMemory(pMac->hHdd, &pModProfileFields, 
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
    palZeroMemory(pMac->hHdd, &roamInfo, sizeof(tCsrRoamInfo));
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
         smsLog(pMac, LOGE, "in %s msg ptr is NULL\n", __FUNCTION__);
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
            smsLog( pMac, LOG1, "Add Sta rsp status = %d\n", pRsp->status );
            //Nothing to be done. May be indicate the self sta addition success by calling session callback (TODO).
            csrRoamSessionOpened(pMac, pCommand->sessionId);
            //Remove this command out of the active list
            if(csrLLRemoveEntry(&pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK))
            {
               //Now put this command back on the avilable command list
               csrReleaseCommand(pMac, pCommand);
            }
            smeProcessPendingQueue( pMac );
         }
         else
         {
            smsLog(pMac, LOGE, "in %s eWNI_SME_ADD_STA_SELF_RSP Received but NO Add sta session command are ACTIVE ...\n",
                  __FUNCTION__);
            status = eHAL_STATUS_FAILURE;
            break;
         }
      }
      else
      {
         smsLog(pMac, LOGE, "in %s eWNI_SME_ADD_STA_SELF_RSP Received but NO commands are ACTIVE ...\n",
               __FUNCTION__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
   } while(0);
   return status;
}
eHalStatus csrSendMBAddSelfStaReqMsg( tpAniSirGlobal pMac, tSirMacAddr macAddr )
{
   tSirSmeAddStaSelfReq *pMsg;
   tANI_U16 msgLen;
   eHalStatus status = eHAL_STATUS_FAILURE;
   do {
      msgLen  = sizeof( tANI_U16 ) + sizeof( tANI_U16 ) + sizeof( tSirMacAddr ) /*+
         sizeof( tSirBssType )*/;
      status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
      if ( !HAL_STATUS_SUCCESS(status) ) break;
      palZeroMemory(pMac->hHdd, pMsg, msgLen);
      pMsg->mesgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_ADD_STA_SELF_REQ);
      pMsg->mesgLen = pal_cpu_to_be16(msgLen);
      // self station address
      palCopyMemory( pMac->hHdd, (tANI_U8 *)pMsg->selfMacAddr, (tANI_U8 *)macAddr, sizeof(tSirMacAddr) );
        smsLog( pMac, LOGE, FL("selfMac=%02x, %02x, %02x, %02x, %02x, %02x\n"),       
            pMsg->selfMacAddr[0],
            pMsg->selfMacAddr[1],
            pMsg->selfMacAddr[2],
            pMsg->selfMacAddr[3],
            pMsg->selfMacAddr[4],
            pMsg->selfMacAddr[5]);
      status = palSendMBMessage(pMac->hHdd, pMsg);
   } while( 0 );
   return( status );
}
eHalStatus csrIssueAddStaForSessionReq(tpAniSirGlobal pMac, tANI_U32 sessionId, tSirMacAddr sessionMacAddr)
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
      palCopyMemory( pMac->hHdd, pCommand->u.addStaSessionCmd.selfMacAddr, sessionMacAddr, sizeof( tSirMacAddr ) );
      status = csrQueueSmeCommand(pMac, pCommand, TRUE);
      if( !HAL_STATUS_SUCCESS( status ) )
      {
         //Should be panic??
         smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
      }
   }
   return (status);
}
eHalStatus csrProcessAddStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
   return csrSendMBAddSelfStaReqMsg( pMac, 
         pCommand->u.addStaSessionCmd.selfMacAddr );
}
eHalStatus csrRoamOpenSession( tpAniSirGlobal pMac, csrRoamCompleteCallback callback, void *pContext,
                          tANI_U8 *pSelfMacAddr, tANI_U8 *pbSessionId )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 i;
    tCsrRoamSession *pSession;
    *pbSessionId = CSR_SESSION_ID_INVALID;
    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( !CSR_IS_SESSION_VALID( pMac, i ) )
        {
            pSession = CSR_GET_SESSION( pMac, i );
            status = eHAL_STATUS_SUCCESS;
            pSession->sessionActive = eANI_BOOLEAN_TRUE;
            pSession->sessionId = (tANI_U8)i;
                pSession->callback = callback;
            pSession->pContext = pContext;
            palCopyMemory( pMac->hHdd, &pSession->selfMacAddr, pSelfMacAddr, sizeof(tCsrBssid) );
            *pbSessionId = (tANI_U8)i;
            status = palTimerAlloc(pMac->hHdd, &pSession->hTimerRoaming, csrRoamRoamingTimerHandler, 
                                    &pSession->roamingTimerInfo);
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE, FL("cannot allocate memory for Roaming timer\n"));
                break;
            }
#ifdef FEATURE_WLAN_BTAMP_UT_RF
            status = palTimerAlloc(pMac->hHdd, &pSession->hTimerJoinRetry, csrRoamJoinRetryTimerHandler, 
                                    &pSession->joinRetryTimerInfo);
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE, FL("cannot allocate memory for joinretry timer\n"));
                break;
            }
#endif
            pSession->ibssJoinTimerInfo.pMac = pMac;
            pSession->ibssJoinTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
            status = palTimerAlloc(pMac->hHdd, &pSession->hTimerIbssJoining, csrRoamIbssJoinTimerHandler, 
                                    &pSession->ibssJoinTimerInfo);
            if(!HAL_STATUS_SUCCESS(status))
            {
                smsLog(pMac, LOGE, FL("cannot allocate memory for IbssJoining timer\n"));
                break;
            }
            status = csrIssueAddStaForSessionReq ( pMac, i, pSelfMacAddr );
            break;
        }
    }
    if( CSR_ROAM_SESSION_MAX == i )
    {
        //No session is available
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
         smsLog(pMac, LOGE, "in %s msg ptr is NULL\n", __FUNCTION__);
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
            smsLog( pMac, LOG1, "Del Sta rsp status = %d\n", pRsp->status );
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
                        smsLog(pMac, LOGP, "%s: Failed to Acquire Lock\n", __FUNCTION__);
                        return status;
                    }
                }
                else {
                    smsLog(pMac, LOGE, "%s: Failed to Release Lock\n", __FUNCTION__);
                }
            } 
   
            //Remove this command out of the active list
            if(csrLLRemoveEntry(&pMac->sme.smeCmdActiveList, pEntry, LL_ACCESS_LOCK))
            {
               //Now put this command back on the avilable command list
               csrReleaseCommand(pMac, pCommand);
            }
            smeProcessPendingQueue( pMac );
         }
         else
         {
            smsLog(pMac, LOGE, "in %s eWNI_SME_DEL_STA_SELF_RSP Received but NO Del sta session command are ACTIVE ...\n",
                  __FUNCTION__);
            status = eHAL_STATUS_FAILURE;
            break;
         }
      }
      else
      {
         smsLog(pMac, LOGE, "in %s eWNI_SME_DEL_STA_SELF_RSP Received but NO commands are ACTIVE ...\n",
               __FUNCTION__);
         status = eHAL_STATUS_FAILURE;
         break;
      }
   } while(0);
   return status;
}
eHalStatus csrSendMBDelSelfStaReqMsg( tpAniSirGlobal pMac, tSirMacAddr macAddr )
{
   tSirSmeDelStaSelfReq *pMsg;
   tANI_U16 msgLen;
   eHalStatus status = eHAL_STATUS_FAILURE;
   do {
      msgLen  = sizeof( tANI_U16 ) + sizeof( tANI_U16 ) + sizeof( tSirMacAddr ) /*+
         sizeof( tSirBssType )*/;
      status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, msgLen);
      if ( !HAL_STATUS_SUCCESS(status) ) break;
   
      palZeroMemory(pMac->hHdd, pMsg, msgLen);
      pMsg->mesgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_DEL_STA_SELF_REQ);
      pMsg->mesgLen = pal_cpu_to_be16(msgLen);
      // self station address
      palCopyMemory( pMac->hHdd, (tANI_U8 *)pMsg->selfMacAddr, (tANI_U8 *)macAddr, sizeof(tSirMacAddr) );
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
      palCopyMemory( pMac->hHdd, pCommand->u.delStaSessionCmd.selfMacAddr, sessionMacAddr, sizeof( tSirMacAddr ) );
      status = csrQueueSmeCommand(pMac, pCommand, TRUE);
      if( !HAL_STATUS_SUCCESS( status ) )
      {
         //Should be panic??
         smsLog( pMac, LOGE, FL(" fail to send message status = %d\n"), status );
      }
   }
   return (status);
}
eHalStatus csrProcessDelStaSessionCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
   return csrSendMBDelSelfStaReqMsg( pMac, 
         pCommand->u.delStaSessionCmd.selfMacAddr );
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
        csrFreeConnectBssDesc(pMac, sessionId);
        csrRoamFreeConnectProfile( pMac, &pSession->connectedProfile );
        csrRoamFreeConnectedInfo ( pMac, &pSession->connectedInfo);
        palTimerFree(pMac->hHdd, pSession->hTimerRoaming);
#ifdef FEATURE_WLAN_BTAMP_UT_RF
        palTimerFree(pMac->hHdd, pSession->hTimerJoinRetry);
#endif
        palTimerFree(pMac->hHdd, pSession->hTimerIbssJoining);
        purgeSmeSessionCmdList(pMac, sessionId);
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
            purgeSmeSessionCmdList(pMac, sessionId);
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
    pSession->ibss_join_pending = FALSE;
    pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
    // TODO : Confirm pMac->roam.fReadyForPowerSave = eANI_BOOLEAN_FALSE;
    csrFreeRoamProfile( pMac, sessionId );
    csrRoamFreeConnectProfile(pMac, &pSession->connectedProfile);
    csrRoamFreeConnectedInfo( pMac, &pSession->connectedInfo );
    csrFreeConnectBssDesc(pMac, sessionId);
    csrScanEnable(pMac);
    palZeroMemory( pMac->hHdd, &pSession->selfMacAddr, sizeof(tCsrBssid) );
    if(pSession->pWpaRsnReqIE)
    {
        palFreeMemory(pMac->hHdd, pSession->pWpaRsnReqIE);
        pSession->pWpaRsnReqIE = NULL;
    }
    pSession->nWpaRsnReqIeLength = 0;
    if(pSession->pWpaRsnRspIE)
    {
        palFreeMemory(pMac->hHdd, pSession->pWpaRsnRspIE);
        pSession->pWpaRsnRspIE = NULL;
    }
    pSession->nWpaRsnRspIeLength = 0;
#ifdef FEATURE_WLAN_WAPI
    if(pSession->pWapiReqIE)
    {
        palFreeMemory(pMac->hHdd, pSession->pWapiReqIE);
        pSession->pWapiReqIE = NULL;
    }
    pSession->nWapiReqIeLength = 0;
    if(pSession->pWapiRspIE)
    {
        palFreeMemory(pMac->hHdd, pSession->pWapiRspIE);
        pSession->pWapiRspIE = NULL;
    }
    pSession->nWapiRspIeLength = 0;
#endif /* FEATURE_WLAN_WAPI */
    if(pSession->pAddIEScan)
    {
        palFreeMemory(pMac->hHdd, pSession->pAddIEScan);
        pSession->pAddIEScan = NULL;
    }
    pSession->nAddIEScanLength = 0;
    if(pSession->pAddIEAssoc)
    {
        palFreeMemory(pMac->hHdd, pSession->pAddIEAssoc);
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
   
   smsLog(pMac, LOGW, " csrRoamLinkUp: WLAN link UP with AP= %02x-%02x-%02x-%02x-%02x-%02x\n", 
          bssid[ 0 ], bssid[ 1 ], bssid[ 2 ],
          bssid[ 3 ], bssid[ 4 ], bssid[ 5 ] );
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
        /* Indicate the neighbor roal algorithm about the connect indication */
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
   /* deregister the clients requesting stats from PE/TL & also stop the corresponding timers*/
   csrRoamDeregStatisticsReq(pMac);
   pMac->roam.vccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
#if   defined WLAN_FEATURE_NEIGHBOR_ROAMING
   /* Indicate the neighbor roal algorithm about the disconnect indication */
   csrNeighborRoamIndicateDisconnect(pMac, sessionId);
#endif
   
#ifndef BMPS_WORKAROUND_NOT_NEEDED
   if(csrIsInfraApStarted( pMac ) &&  pMac->roam.configParam.doBMPSWorkaround)
   {
       pMac->roam.configParam.doBMPSWorkaround = 0;
   }
#endif
   
}

void csrRoamTlStatsTimerHandler(void *pv)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( pv );
   eHalStatus status;
   pMac->roam.tlStatsReqInfo.timerRunning = FALSE;

   smsLog(pMac, LOG1, FL(" TL stat timer is no-op. It needs to support multiple stations"));

#if 0
   // TODO Persession .???
   //req TL for stats
   if(WLANTL_GetStatistics(pMac->roam.gVosContext, &tlStats, pMac->roam.connectedInfo.staId))
   {
      smsLog(pMac, LOGE, FL("csrRoamTlStatsTimerHandler:couldn't get the stats from TL\n"));
   }
   else
   {
      //save in SME
      csrRoamSaveStatsFromTl(pMac, tlStats);
   }
#endif
   if(!pMac->roam.tlStatsReqInfo.timerRunning)
   {
      if(pMac->roam.tlStatsReqInfo.periodicity)
      {
         //start timer
         status = palTimerStart(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer, 
                                pMac->roam.tlStatsReqInfo.periodicity * PAL_TIMER_TO_MS_UNIT, eANI_BOOLEAN_FALSE);
         if(!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamTlStatsTimerHandler:cannot start TlStatsTimer timer\n"));
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
         smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:failed to destroy hPeStatsTimer timer\n"));
      }

      // Free the entry
      palFreeMemory(pMac->hHdd, pPeStatsReqListEntry);
      pPeStatsReqListEntry = NULL;
   }
   else
   {
      if(!pPeStatsReqListEntry->rspPending)
      {
         status = csrSendMBStatsReqMsg(pMac, pPeStatsReqListEntry->statsMask & ~(1 << eCsrGlobalClassDStats), 
                                       pPeStatsReqListEntry->staId);
         if(!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:failed to send down stats req to PE\n"));
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
            smsLog(pMac, LOGE, FL("csrRoamPeStatsTimerHandler:cannot start hPeStatsTimer timer\n"));
            return;
         }
         pPeStatsReqListEntry->timerRunning = TRUE;

      }

   }
}
void csrRoamStatsClientTimerHandler(void *pv)
{
   tCsrStatsClientReqInfo *pStaEntry = (tCsrStatsClientReqInfo *)pv;
   if(VOS_TIMER_STATE_STOPPED == vos_timer_getCurrentState(&pStaEntry->timer))
   {
#if 0
       // TODO Stats fix for multisession
       //start the timer
       vosStatus = vos_timer_start( &pStaEntry->timer, pStaEntry->periodicity );
   
       if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) ) 
       {
          smsLog(pStaEntry->pMac, LOGE, FL("csrGetStatistics:cannot start StatsClient timer\n"));
       }
#endif       
   }
#if 0
   //send up the stats report
   csrRoamReportStatistics(pStaEntry->pMac, pStaEntry->statsMask, pStaEntry->callback, 
                           pStaEntry->staId, pStaEntry->pContext);
#endif
}



eHalStatus csrSendMBStatsReqMsg( tpAniSirGlobal pMac, tANI_U32 statsMask, tANI_U8 staId)
{
   tAniGetPEStatsReq *pMsg;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof(tAniGetPEStatsReq));
   if ( !HAL_STATUS_SUCCESS(status) ) 
   {
      smsLog(pMac, LOG1, " csrSendMBStatsReqMsg: failed to allocate mem for stats req \n");
      return status;
   }
   // need to initiate a stats request to PE
   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_STATISTICS_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetPEStatsReq);
   pMsg->staId = staId;
   pMsg->statsMask = statsMask;
   status = palSendMBMessage(pMac->hHdd, pMsg );    
   if(!HAL_STATUS_SUCCESS(status))
   {
      smsLog(pMac, LOG1, " csrSendMBStatsReqMsg: failed to send down the stats req \n");
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
   v_S7_t     rssi = 0;
   tANI_U32   *pRssi = NULL;
   pSmeStatsRsp = (tAniGetPEStatsRsp *)pSirMsg;
   if(pSmeStatsRsp->rc)
   {
      smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:stats rsp from PE shows failure\n"));
      goto post_update;
   }
   tempMask = pSmeStatsRsp->statsMask;
   pStats = ((tANI_U8 *)&pSmeStatsRsp->statsMask) + sizeof(pSmeStatsRsp->statsMask);
   /* subtract all statistics from this length, and after processing the entire 
    * 'stat' part of the message, if the length is not zero, then rssi is piggy packed 
    * in this 'stats' message.
    */
   length = pSmeStatsRsp->msgLen - sizeof(tAniGetPEStatsRsp);
   //new stats info from PE, fill up the stats strucutres in PMAC
   while(tempMask)
   {
      if(tempMask & 1)
      {
         switch(counter)
         {
         case eCsrSummaryStats:
            smsLog( pMac, LOG1, FL("csrRoamStatsRspProcessor:summary stats\n"));
            status = palCopyMemory(pMac->hHdd, (tANI_U8 *)&pMac->roam.summaryStatsInfo, 
                                   pStats, sizeof(tCsrSummaryStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy summary stats\n"));
            }
            pStats += sizeof(tCsrSummaryStatsInfo);
            length -= sizeof(tCsrSummaryStatsInfo);
            break;
         case eCsrGlobalClassAStats:
            smsLog( pMac, LOG1, FL("csrRoamStatsRspProcessor:ClassA stats\n"));
            status = palCopyMemory(pMac->hHdd, (tANI_U8 *)&pMac->roam.classAStatsInfo, 
                                   pStats, sizeof(tCsrGlobalClassAStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy ClassA stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassAStatsInfo);
            length -= sizeof(tCsrGlobalClassAStatsInfo);
            break;
         case eCsrGlobalClassBStats:
            smsLog( pMac, LOG1, FL("csrRoamStatsRspProcessor:ClassB stats\n"));
            status = palCopyMemory(pMac->hHdd, (tANI_U8 *)&pMac->roam.classBStatsInfo, 
                                   pStats, sizeof(tCsrGlobalClassBStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy ClassB stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassBStatsInfo);
            length -= sizeof(tCsrGlobalClassBStatsInfo);
            break;
         case eCsrGlobalClassCStats:
            smsLog( pMac, LOG1, FL("csrRoamStatsRspProcessor:ClassC stats\n"));
            status = palCopyMemory(pMac->hHdd, (tANI_U8 *)&pMac->roam.classCStatsInfo, 
                                   pStats, sizeof(tCsrGlobalClassCStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy ClassC stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassCStatsInfo);
            length -= sizeof(tCsrGlobalClassCStatsInfo);
            break;
         case eCsrPerStaStats:
            smsLog( pMac, LOG1, FL("csrRoamStatsRspProcessor:PerSta stats\n"));
            if( CSR_MAX_STA > pSmeStatsRsp->staId )
            {
               status = palCopyMemory(pMac->hHdd, (tANI_U8 *)&pMac->roam.perStaStatsInfo[pSmeStatsRsp->staId], 
                                      pStats, sizeof(tCsrPerStaStatsInfo));
            }
            else
            {
               status = eHAL_STATUS_FAILURE;
               smsLog( pMac, LOGE, FL("csrRoamStatsRspProcessor:out bound staId:%d\n"), pSmeStatsRsp->staId);
               VOS_ASSERT( 0 );
            }
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:failed to copy PerSta stats\n"));
            }
            pStats += sizeof(tCsrPerStaStatsInfo);
            length -= sizeof(tCsrPerStaStatsInfo);
            break;
         default:
            smsLog( pMac, LOGW, FL("csrRoamStatsRspProcessor:unknown stats type\n"));
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
   }
   else
   {
       /* If riva is not sending rssi, continue to use the hack */
       rssi = RSSI_HACK_BMPS;
   }
   WDA_UpdateRssiBmps(pvosGCtx, pSmeStatsRsp->staId, rssi);
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
      smsLog(pMac, LOGW, "csrRoamFindInPeStatsReqList: List empty, no request to PE\n");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrPeStatsReqInfo, link );
      if(pTempStaEntry->statsMask == statsMask)
      {
         smsLog(pMac, LOGW, "csrRoamFindInPeStatsReqList: match found\n");
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
      smsLog(pMac, LOGW, "csrRoamChecknUpdateClientReqList: List empty, no request from "
             "upper layer client(s)\n");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if((pTempStaEntry->requesterId == pStaEntry->requesterId) && 
         (pTempStaEntry->statsMask == pStaEntry->statsMask))
      {
         smsLog(pMac, LOGW, "csrRoamChecknUpdateClientReqList: match found\n");
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
      smsLog(pMac, LOGW, "csrRoamCheckClientReqList: List empty, no request from "
             "upper layer client(s)\n");
      return NULL;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link );
      if((pTempStaEntry->statsMask & ~(1 << eCsrGlobalClassDStats))  == statsMask)
      {
         smsLog(pMac, LOGW, "csrRoamCheckClientReqList: match found\n");
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
   VOS_ASSERT( ul_mac_loss_trigger_threshold != 0 );
   smsLog(pMac, LOGW, "csrRoamVccTrigger: UL_MAC_LOSS_THRESHOLD is %d\n", 
          ul_mac_loss_trigger_threshold );
   if(ul_mac_loss_trigger_threshold < ul_mac_loss)
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality is POOR \n");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
   }
   else
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality is GOOD\n");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_GOOD_IND;
   }
   smsLog(pMac, LOGW, "csrRoamVccTrigger: link qual : *** UL_MAC_LOSS %d *** ",
          ul_mac_loss);
   if(newVccLinkQuality != pMac->roam.vccLinkQuality)
   {
      smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality changed: trigger necessary\n");
      if(NULL != pMac->roam.linkQualityIndInfo.callback) 
      {
         smsLog(pMac, LOGW, "csrRoamVccTrigger: link quality indication %d\n",
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
        // TODO : Session info unavailable
        tANI_U32 sessionId = 0;
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   /*-------------------------------------------------------------------------
     Link quality is currently binary based on OBIWAN recommended triggers
     Check for a change in link quality and notify client if necessary
   -------------------------------------------------------------------------*/
   smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: RSSI trigger threshold is %d\n", 
          pMac->roam.configParam.vccRssiThreshold);
   if(!csrIsConnStateConnectedInfra(pMac, sessionId))
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: ignoring the indication as we are not connected\n");
      return VOS_STATUS_SUCCESS;
   }
   if(WLANTL_HO_THRESHOLD_DOWN == rssiNotification)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality is POOR\n");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_POOR_IND;
   }
   else if(WLANTL_HO_THRESHOLD_UP == rssiNotification)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality is GOOD \n");
      newVccLinkQuality = eCSR_ROAM_LINK_QUAL_GOOD_IND;
   }
   else
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: unknown rssi notification %d\n", rssiNotification);
      //Set to this so the code below won't do anything
      newVccLinkQuality = pMac->roam.vccLinkQuality;    
      VOS_ASSERT(0);
   }

   if(newVccLinkQuality != pMac->roam.vccLinkQuality)
   {
      smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality changed: trigger necessary\n");
      if(NULL != pMac->roam.linkQualityIndInfo.callback) 
      {
         smsLog(pMac, LOGW, "csrRoamVccTriggerRssiIndCallback: link quality indication %d\n",
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
   eHalStatus  status;
   //if same entity requested for same set of stats with different periodicity & 
   // callback update it
   if(NULL == csrRoamChecknUpdateClientReqList(pMac, pStaEntry, TRUE))
   {
   
      status = palAllocateMemory(pMac->hHdd, (void **)&pNewStaEntry, sizeof(tCsrStatsClientReqInfo));
      if (!HAL_STATUS_SUCCESS(status))
      {
         smsLog(pMac, LOGW, "csrRoamInsertEntryIntoList: couldn't allocate memory for the "
                "entry\n");
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
   eHalStatus  status;
   status = palAllocateMemory(pMac->hHdd, (void **)&pNewStaEntry, sizeof(tCsrPeStatsReqInfo));
   if (!HAL_STATUS_SUCCESS(status))
   {
      smsLog(pMac, LOGW, "csrRoamInsertEntryIntoPeStatsReqList: couldn't allocate memory for the "
                  "entry\n");
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
                            tANI_U8 staId, tCsrBssid bssId, void *pContext, void* pVosContext)
{  
   eHalStatus status = eHAL_STATUS_SUCCESS;
   vos_msg_t  msg;
   tANI_U32 sessionId;

   tAniGetRssiReq *pMsg;
   smsLog(pMac, LOG2, FL("called"));      
   status = palAllocateMemory(pMac->hHdd, (void **)&pMsg, sizeof(tAniGetRssiReq));
   if ( !HAL_STATUS_SUCCESS(status) ) 
   {
      smsLog(pMac, LOGE, " csrGetRssi: failed to allocate mem for req \n");
      return status;
   }
   csrRoamGetSessionIdFromBSSID(pMac, (tCsrBssid *)bssId, &sessionId);

   pMsg->msgType = pal_cpu_to_be16((tANI_U16)eWNI_SME_GET_RSSI_REQ);
   pMsg->msgLen = (tANI_U16)sizeof(tAniGetRssiReq);
   pMsg->sessionId = sessionId;
   pMsg->staId = staId;
   pMsg->rssiCallback = callback;
   pMsg->pDevContext = pContext;
   pMsg->pVosContext = pVosContext;
   msg.type = eWNI_SME_GET_RSSI_REQ;
   msg.bodyptr = pMsg;
   msg.reserved = 0;
   if(VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME, &msg))
   {
       smsLog(pMac, LOGE, " csrGetRssi failed to post msg to self \n");   
       palFreeMemory(pMac->hHdd, (void *)pMsg);
       status = eHAL_STATUS_FAILURE;
   }
   smsLog(pMac, LOG2, FL("returned"));      
   return status;
}
eHalStatus csrGetStatistics(tpAniSirGlobal pMac, eCsrStatsRequesterType requesterId, 
                            tANI_U32 statsMask, 
                            tCsrStatsCallback callback, 
                            tANI_U32 periodicity, tANI_BOOLEAN cache, 
                            tANI_U8 staId, void *pContext)
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
      //smsLog(pMac, LOGW, "csrGetStatistics: wrong state curState(%d) not connected\n", pMac->roam.curState);
      return eHAL_STATUS_FAILURE;
   }
   if((!statsMask) && (!callback))
   {
      //msg
      smsLog(pMac, LOGW, "csrGetStatistics: statsMask & callback empty in the request\n");
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
                "find any existing request in statsClientReqList\n");
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
               status = palTimerStop(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer);
               if(!HAL_STATUS_SUCCESS(status))
               {
                  smsLog(pMac, LOGE, FL("csrGetStatistics:cannot stop TlStatsTimer timer\n"));
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
            smsLog(pMac, LOGE, FL("csrGetStatistics:failed to destroy Client req timer\n"));
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
   
   
      //if periodic report requested with non cached result from PE/TL
      if(periodicity)
      {
      
         //if looking for stats from PE
         if(statsMask & ~(1 << eCsrGlobalClassDStats))
         {
         
            //check if same request made already & waiting for rsp
            pPeStaEntry = csrRoamCheckPeStatsReqList(pMac, statsMask & ~(1 << eCsrGlobalClassDStats), 
                                               periodicity, &found, staId);
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
         //request stats from TL rightaway if requested by client, update tlStatsReqInfo if needed
         if(statsMask & (1 << eCsrGlobalClassDStats))
         {
            if(cache && pMac->roam.tlStatsReqInfo.numClient)
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:Looking for cached stats from TL\n"));
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
                  if(NULL != pTlStats)
                  {
                     //req TL for class D stats
                     if(WLANTL_GetStatistics(pMac->roam.gVosContext, pTlStats, staId))
                     {
                        smsLog(pMac, LOGE, FL("csrGetStatistics:couldn't get the stats from TL\n"));
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
                     status = palTimerStart(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer, 
                                            pMac->roam.tlStatsReqInfo.periodicity * PAL_TIMER_TO_MS_UNIT, eANI_BOOLEAN_FALSE);
                     if(!HAL_STATUS_SUCCESS(status))
                     {
                        smsLog(pMac, LOGE, FL("csrGetStatistics:cannot start TlStatsTimer timer\n"));
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
            status = csrSendMBStatsReqMsg(pMac, statsMask & ~(1 << eCsrGlobalClassDStats), staId);
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:failed to send down stats req to PE\n"));
            }
            //so that when the stats rsp comes back from PE we respond to upper layer
            //right away
            staEntry.timerExpired = TRUE;
            insertInClientList = TRUE;
         }
         if(statsMask & (1 << eCsrGlobalClassDStats))
         {
            pTlStats = (WLANTL_TRANSFER_STA_TYPE *)vos_mem_malloc(sizeof(WLANTL_TRANSFER_STA_TYPE));
            if(NULL != pTlStats)
            {
               //req TL for class D stats
               if(!VOS_IS_STATUS_SUCCESS(WLANTL_GetStatistics(pMac->roam.gVosContext, pTlStats, staId)))
               {
                  smsLog(pMac, LOGE, FL("csrGetStatistics:couldn't get the stats from TL\n"));
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
            smsLog(pMac, LOGW, "csrGetStatistics: Failed to insert req in statsClientReqList\n");
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
               smsLog(pMac, LOGE, FL("csrGetStatistics:cannot init StatsClient timer\n"));
               return eHAL_STATUS_FAILURE;
            }
            vosStatus = vos_timer_start( &pStaEntry->timer, periodicity );
            if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) ) 
            {
               smsLog(pMac, LOGE, FL("csrGetStatistics:cannot start StatsClient timer\n"));
               return eHAL_STATUS_FAILURE;
            }
         }
      }
   }
   return eHAL_STATUS_SUCCESS;
}

tCsrPeStatsReqInfo * csrRoamCheckPeStatsReqList(tpAniSirGlobal pMac, tANI_U32  statsMask, 
                                                tANI_U32 periodicity, tANI_BOOLEAN *pFound, tANI_U8 staId)
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
      palZeroMemory(pMac->hHdd, &staEntry, sizeof(tCsrPeStatsReqInfo));
      staEntry.numClient = 1;
      staEntry.periodicity = periodicity;
      staEntry.pMac = pMac;
      staEntry.rspPending = FALSE;
      staEntry.staId = staId;
      staEntry.statsMask = statsMask;
      staEntry.timerRunning = FALSE;
      pTempStaEntry = csrRoamInsertEntryIntoPeStatsReqList(pMac, &pMac->roam.peStatsReqList, &staEntry); 
      if(!pTempStaEntry)
      {
         //msg
         smsLog(pMac, LOGW, "csrRoamCheckPeStatsReqList: Failed to insert req in peStatsReqList\n");
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
         status = csrSendMBStatsReqMsg(pMac, statsMask & ~(1 << eCsrGlobalClassDStats), staId);
         if(!HAL_STATUS_SUCCESS(status))
         {
            smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:failed to send down stats req to PE\n"));
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
               smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:cannot init hPeStatsTimer timer\n"));
               return NULL;
            }
         }
         //start timer
         smsLog(pMac, LOG1, "csrRoamCheckPeStatsReqList:peStatsTimer period %d\n", pTempStaEntry->periodicity);
         vosStatus = vos_timer_start( &pTempStaEntry->hPeStatsTimer, pTempStaEntry->periodicity );
         if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) ) 
         {
            smsLog(pMac, LOGE, FL("csrRoamCheckPeStatsReqList:cannot start hPeStatsTimer timer\n"));
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
            palFreeMemory(pMac->hHdd, GET_BASE_ADDR( pEntry, tCsrStatsClientReqInfo, link ));
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
      smsLog(pMac, LOGW, "csrRoamRemoveEntryFromPeStatsReqList: List empty, no stats req for PE\n");
      return;
   }
   while( pEntry )
   {
      pTempStaEntry = GET_BASE_ADDR( pEntry, tCsrPeStatsReqInfo, link );
      if( pTempStaEntry && pTempStaEntry->statsMask == pPeStaEntry->statsMask)
      {
         smsLog(pMac, LOGW, "csrRoamRemoveEntryFromPeStatsReqList: match found\n");
         if(pTempStaEntry->timerRunning)
         {
            vosStatus = vos_timer_stop( &pTempStaEntry->hPeStatsTimer );
            /* If we are not able to stop the timer here, just remove 
             * the entry from the linked list. Destroy the timer object 
             * and free the memory in the timer CB
             */
            if( vosStatus == VOS_STATUS_SUCCESS )
            {
               /* the timer is successfully stopped */
               pTempStaEntry->timerRunning = FALSE;

               /* Destroy the timer */
               vosStatus = vos_timer_destroy( &pTempStaEntry->hPeStatsTimer );
               if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
               {
                  smsLog(pMac, LOGE, FL("csrRoamRemoveEntryFromPeStatsReqList:failed to destroy hPeStatsTimer timer\n"));
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
               palFreeMemory(pMac->hHdd, pTempStaEntry);
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
   eHalStatus status = eHAL_STATUS_FAILURE;
   if(!callback)
   {
      smsLog(pMac, LOGE, FL("csrRoamReportStatistics:cannot report callback NULL\n"));
      return;
   }
   if(!statsMask)
   {
      smsLog(pMac, LOGE, FL("csrRoamReportStatistics:cannot report statsMask is 0\n"));
      return;
   }
   pStats = stats;
   tempMask = statsMask;
   while(tempMask)
   {
      if(tempMask & 1)
      {
         //new stats info from PE, fill up the stats strucutres in PMAC
         switch(counter)
         {
         case eCsrSummaryStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:summary stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.summaryStatsInfo, 
                                   sizeof(tCsrSummaryStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy summary stats\n"));
            }
            pStats += sizeof(tCsrSummaryStatsInfo);
            break;
         case eCsrGlobalClassAStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:ClassA stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.classAStatsInfo, 
                                   sizeof(tCsrGlobalClassAStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy ClassA stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassAStatsInfo);
            break;
         case eCsrGlobalClassBStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:ClassB stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.classBStatsInfo, 
                                   sizeof(tCsrGlobalClassBStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy ClassB stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassBStatsInfo);
            break;
         case eCsrGlobalClassCStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:ClassC stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.classCStatsInfo, 
                                   sizeof(tCsrGlobalClassCStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy ClassC stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassCStatsInfo);
            break;
         case eCsrGlobalClassDStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:ClassD stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.classDStatsInfo, 
                                   sizeof(tCsrGlobalClassDStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy ClassD stats\n"));
            }
            pStats += sizeof(tCsrGlobalClassDStatsInfo);
            break;
         case eCsrPerStaStats:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:PerSta stats\n"));
            status = palCopyMemory(pMac->hHdd, pStats, (tANI_U8 *)&pMac->roam.perStaStatsInfo[staId], 
                                   sizeof(tCsrPerStaStatsInfo));
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog( pMac, LOG1, FL("csrRoamReportStatistics:failed to copy PerSta stats\n"));
            }
            pStats += sizeof(tCsrPerStaStatsInfo);
            break;
         default:
            smsLog( pMac, LOG1, FL("csrRoamReportStatistics:unknown stats type\n"));
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
             "upper layer client(s)\n");
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
            status = palTimerStop(pMac->hHdd, pMac->roam.tlStatsReqInfo.hTlStatsTimer);
            if(!HAL_STATUS_SUCCESS(status))
            {
               smsLog(pMac, LOGE, FL("csrRoamDeregStatisticsReq:cannot stop TlStatsTimer timer\n"));
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
              smsLog(pMac, LOGE, FL("csrRoamDeregStatisticsReq:failed to destroy Client req timer\n"));
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
        // TODO : Session info unavailable
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
                //Check whether the profile is already connected. If so, no need for full power
                //Note: IBSS is ignored for now because we don't support powersave in IBSS
                if ( csrIsConnStateConnectedInfra(pMac, sessionId) && pBSSList )
                {
                    //Only need to check the first one
                    pEntry = csrLLPeekHead(&pBSSList->List, LL_ACCESS_LOCK);
                    if( pEntry )
                    {
                        pScanResult = GET_BASE_ADDR(pEntry, tCsrScanResult, Link);
#if 0
                                                // TODO : Session Specific info pConnectBssDesc
                        if( csrIsBssIdEqual( pMac, &pScanResult->Result.BssDescriptor, pMac->roam.pConnectBssDesc ) &&
                            csrIsSsidEqual( pMac, pMac->roam.pConnectBssDesc, 
                                            &pScanResult->Result.BssDescriptor, (tDot11fBeaconIEs *)( pScanResult->Result.pvIes ) ) )
                        {
                            // Check to see if the Auth type has changed in the Profile.  If so, we don't want to Reassociate
                            // with Authenticating first.  To force this, stop the current association (Disassociate) and 
                            // then re 'Join' the AP, wihch will force an Authentication (with the new Auth type) followed by 
                            // a new Association.
                            if(csrIsSameProfile(pMac, &pMac->roam.connectedProfile, pProfile))
                            {
                                if(csrRoamIsSameProfileKeys(pMac, &pMac->roam.connectedProfile, pProfile))
                                {
                                    //Done, eventually, the command reaches eCsrReassocToSelfNoCapChange;
                                    //No need for full power
                                    //Set the flag so the code later can avoid to do the above
                                    //check again.
                                    pCommand->u.roamCmd.fReassocToSelfNoCapChange = eANI_BOOLEAN_TRUE;
                                    break;
                                }
                            }
                        }
#endif
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
        smsLog( pMac, LOGE, FL( "  cannot process because PMC is in stopped/standby state %d\n" ), pmcState );
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

static eHalStatus csrRequestFullPower( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fNeedFullPower = eANI_BOOLEAN_FALSE;
    tRequestFullPowerReason reason = eSME_REASON_OTHER;
    status = csrIsFullPowerNeeded( pMac, pCommand, &reason, &fNeedFullPower );
    if( fNeedFullPower && HAL_STATUS_SUCCESS( status ) )
    {
        status = pmcRequestFullPower(pMac, csrFullPowerCallback, pMac, reason);
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
    if( (eSmeCommandScan == pCommand->command) && pMac->scan.fDropScanCmd )
    {
        smsLog(pMac, LOGW, FL(" drop scan (scan reason %d) command"),
           pCommand->u.scanCmd.reason);
        return eHAL_STATUS_CSR_WRONG_STATE;
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
        smsLog( pMac, LOGE, FL( "  cannot queue command %d\n" ), pCommand->command );
    }
    return ( status );
}
#ifdef WLAN_SOFTAP_FEATURE
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
        status = palAllocateMemory( pMac->hHdd, (void **)&pMsg, sizeof(tSirUpdateAPWPSIEsReq) );
        if (!HAL_STATUS_SUCCESS(status)) break;
        palZeroMemory( pMac->hHdd, pMsg, sizeof(tSirUpdateAPWPSIEsReq) );
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_UPDATE_APWPSIE_REQ);

        pBuf = (tANI_U8 *)&pMsg->transactionId;
        wTmpBuf = pBuf;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
        // bssId
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, &pSession->selfMacAddr, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        //sessionId
        *pBuf++ = (tANI_U8)sessionId;
        // APWPSIEs
        palCopyMemory( pMac->hHdd, (tSirAPWPSIEs *)pBuf, pAPWPSIES, sizeof(tSirAPWPSIEs));
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
        status = palAllocateMemory( pMac->hHdd, (void **)&pMsg, sizeof(tSirUpdateAPWPARSNIEsReq) );
        if (!HAL_STATUS_SUCCESS(status)) break;
        palZeroMemory( pMac->hHdd, pMsg, sizeof( tSirUpdateAPWPARSNIEsReq ) );
        pMsg->messageType = pal_cpu_to_be16((tANI_U16)eWNI_SME_SET_APWPARSNIEs_REQ);
        pBuf = (tANI_U8 *)&pMsg->transactionId;
        wTmpBuf = pBuf;
        // transactionId
        *pBuf = 0;
        *( pBuf + 1 ) = 0;
        pBuf += sizeof(tANI_U16);
 
        // bssId
        palCopyMemory( pMac->hHdd, (tSirMacAddr *)pBuf, &pSession->selfMacAddr, sizeof(tSirMacAddr) );
        pBuf += sizeof(tSirMacAddr);
        // sessionId
        *pBuf++ = (tANI_U8)sessionId;
    
        // APWPARSNIEs
        palCopyMemory( pMac->hHdd, (tSirRSNie *)pBuf, pAPSirRSNie, sizeof(tSirRSNie));
        pBuf += sizeof(tSirRSNie);
        pMsg->length = pal_cpu_to_be16((tANI_U16)(sizeof(tANI_U32 ) + (pBuf - wTmpBuf))); //msg_header + msg
    status = palSendMBMessage(pMac->hHdd, pMsg);
    } while( 0 );
    return ( status );
}
#endif //#ifdef WLAN_SOFTAP_FEATURE

#ifdef WLAN_FEATURE_VOWIFI_11R
//eHalStatus csrRoamIssueFTPreauthReq(tHalHandle hHal, tANI_U32 sessionId, tCsrBssid preAuthBssid, tANI_U8 channelId)
eHalStatus csrRoamIssueFTPreauthReq(tHalHandle hHal, tANI_U32 sessionId, tpSirBssDescription pBssDescription)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tpSirFTPreAuthReq pftPreAuthReq;
    tANI_U16 auth_req_len = 0;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    auth_req_len = sizeof(tSirFTPreAuthReq);
    pftPreAuthReq = (tpSirFTPreAuthReq)vos_mem_malloc(auth_req_len);
    if (pftPreAuthReq == NULL)
    {
        smsLog(pMac, LOGE, FL("Memory allocation for FT Preauth request failed"));
        return eHAL_STATUS_RESOURCES;
    }
    // Save the SME Session ID here. We need it while processing the preauth response
    pMac->ft.ftSmeContext.smeSessionId = sessionId;
    vos_mem_zero(pftPreAuthReq, auth_req_len);

    pftPreAuthReq->pbssDescription = (tpSirBssDescription)vos_mem_malloc(
            sizeof(pBssDescription->length) + pBssDescription->length);

    pftPreAuthReq->messageType = pal_cpu_to_be16(eWNI_SME_FT_PRE_AUTH_REQ);

    pftPreAuthReq->preAuthchannelNum = pBssDescription->channelId;

    palCopyMemory(pMac->hHdd, (void *)&pftPreAuthReq->currbssId, (void *)pSession->connectedProfile.bssid, sizeof(tSirMacAddr));
    palCopyMemory(pMac->hHdd, (void *)&pftPreAuthReq->preAuthbssId, (void *)pBssDescription->bssId, sizeof(tSirMacAddr));  

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (csrRoamIs11rAssoc(pMac))
    {
        pftPreAuthReq->ft_ies_length = (tANI_U16)pMac->ft.ftSmeContext.auth_ft_ies_length;
        palCopyMemory(pMac->hHdd, pftPreAuthReq->ft_ies, pMac->ft.ftSmeContext.auth_ft_ies, 
                                pMac->ft.ftSmeContext.auth_ft_ies_length);
    }
    else
#endif
    {
        pftPreAuthReq->ft_ies_length = 0; 
    }
    vos_mem_copy(pftPreAuthReq->pbssDescription, pBssDescription, pBssDescription->length);
    pftPreAuthReq->length = pal_cpu_to_be16(sizeof(tSirFTPreAuthReq) + sizeof(pBssDescription->length) +
                                                    pBssDescription->length); 
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
#ifdef FEATURE_WLAN_LFR
    tCsrRoamInfo roamInfo;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R_DEBUG
    smsLog( pMac, LOGE, FL("Preauth response status code %d"), pFTPreAuthRsp->status); 
#endif
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING    
    csrNeighborRoamPreauthRspHandler(pMac, (VOS_STATUS)pFTPreAuthRsp->status);
#endif
    /* The below function calls/timers should be invoked only if the pre-auth is successful */
    if (VOS_STATUS_SUCCESS != (VOS_STATUS)pFTPreAuthRsp->status)
        return;
    // Implies a success
    pMac->ft.ftSmeContext.FTState = eFT_AUTH_COMPLETE;
    // Indicate SME QoS module the completion of Preauth success. This will trigger the creation of RIC IEs
    pMac->ft.ftSmeContext.psavedFTPreAuthRsp = pFTPreAuthRsp;
    sme_QosCsrEventInd(pMac, pMac->ft.ftSmeContext.smeSessionId, SME_QOS_CSR_PREAUTH_SUCCESS_IND, NULL);
    /* Start the pre-auth reassoc interval timer with a period of 400ms. When this expires, 
     * actual transition from the current to handoff AP is triggered */
    status = palTimerStart(pMac->hHdd, pMac->ft.ftSmeContext.preAuthReassocIntvlTimer,
                                                            60 * PAL_TIMER_TO_MS_UNIT,
                                                            eANI_BOOLEAN_FALSE);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Preauth reassoc interval timer start failed to start with status %d\n"), status);
        return;
    }
    // Save the received response
    palCopyMemory(pMac->hHdd, (void *)&pMac->ft.ftSmeContext.preAuthbssId, (void *)pFTPreAuthRsp->preAuthbssId, sizeof(tCsrBssid));
    if (csrRoamIs11rAssoc(pMac))
       csrRoamCallCallback(pMac, pFTPreAuthRsp->smeSessionId, NULL, 0, 
                        eCSR_ROAM_FT_RESPONSE, eCSR_ROAM_RESULT_NONE);

    // Currently we dont do anything special for CCX connection.
#ifdef FEATURE_WLAN_LFR
    // If Legacy Fast Roaming is enabled, signal the supplicant  
    // So he can send us a PMK-ID for this candidate AP.
    if (csrRoamIsFastRoamEnabled(pMac))
    {
        // Save the bssid from the received response 
        palCopyMemory(pMac->hHdd, (void *)&roamInfo.bssid, (void *)pFTPreAuthRsp->preAuthbssId, sizeof(tCsrBssid));
        csrRoamCallCallback(pMac, pFTPreAuthRsp->smeSessionId, &roamInfo, 0, eCSR_ROAM_PMK_NOTIFY, 0);
    }

#endif

    // Done with it, init it.
    pMac->ft.ftSmeContext.psavedFTPreAuthRsp = NULL;
}
#endif
#ifdef FEATURE_WLAN_BTAMP_UT_RF
void csrRoamJoinRetryTimerHandler(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    tANI_U32 sessionId = pInfo->sessionId;
    tCsrRoamSession *pSession;
    
    if( CSR_IS_SESSION_VALID(pMac, sessionId) )
    {
        smsLog( pMac, LOGE, FL( "  retrying the last roam profile on session %d\n" ), sessionId );
        pSession = CSR_GET_SESSION( pMac, sessionId );
        if(pSession->pCurRoamProfile && csrIsConnStateDisconnected(pMac, sessionId))
        {
            if( !HAL_STATUS_SUCCESS(csrRoamJoinLastProfile(pMac, sessionId)) )
            {
               smsLog( pMac, LOGE, FL( "  fail to retry the last roam profile\n" ) );
            }
        }
    }
}
eHalStatus csrRoamStartJoinRetryTimer(tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U32 interval)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );
    
    if(pSession->pCurRoamProfile && pSession->maxRetryCount)
    {
        smsLog(pMac, LOGE, FL(" call sessionId %d retry count %d left\n "), sessionId, pSession->maxRetryCount);
        pSession->maxRetryCount--;
        pSession->joinRetryTimerInfo.pMac = pMac;
        pSession->joinRetryTimerInfo.sessionId = (tANI_U8)sessionId;
        status = palTimerStart(pMac->hHdd, pSession->hTimerJoinRetry, interval, eANI_BOOLEAN_FALSE);
        if(!HAL_STATUS_SUCCESS(status))
        {
            smsLog(pMac, LOGE, FL(" fail to start timer status %s \n "), status);
        }
    }
    else
    {
        smsLog(pMac, LOGE, FL(" not to start timer due to no profile or reach mac ret (%d)\n "), 
               pSession->maxRetryCount);
    }
    
    return (status);
}
eHalStatus csrRoamStopJoinRetryTimer(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    smsLog(pMac, LOGE, " csrRoamStopJoinRetryTimer \n ");
    if( CSR_IS_SESSION_VALID(pMac, sessionId) )
    {
        return (palTimerStop(pMac->hHdd, pMac->roam.roamSession[sessionId].hTimerJoinRetry));
    }
    
    return eHAL_STATUS_SUCCESS;
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
      The rest is conditionally defined of (WNI_POLARIS_FW_PRODUCT == AP) and not used
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
      palCopyMemory( pMac->hHdd, pCountry,
            nvTables.defaultCountryTable.countryCode,
            WNI_CFG_COUNTRY_CODE_LEN );
      return status;
   }
   else
   {
      palCopyMemory( pMac->hHdd, pCountry,
            "XXX",
            WNI_CFG_COUNTRY_CODE_LEN );
      status = eHAL_STATUS_FAILURE;
      return status;
   }
}

eHalStatus csrGetCurrentCountryCode(tpAniSirGlobal pMac, tANI_U8 *pCountry)
{
   palCopyMemory( pMac->hHdd, pCountry,
         pMac->scan.countryCode11d,
         WNI_CFG_COUNTRY_CODE_LEN );
   return eHAL_STATUS_SUCCESS;
}
