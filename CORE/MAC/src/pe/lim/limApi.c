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
 * This file limApi.cc contains the functions that are
 * exported by LIM to other modules.
 *
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#ifdef ANI_PRODUCT_TYPE_AP
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#include "wniApi.h"
#endif
#include "sirCommon.h"
#include "sirDebug.h"
#include "aniParam.h"
#include "cfgApi.h"

#include "schApi.h"
#include "utilsApi.h"
#include "limApi.h"
#include "limGlobal.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"
#include "limIbssPeerMgmt.h"
#include "limAdmitControl.h"
#include "pmmApi.h"
#include "logDump.h"
#include "limSendSmeRspMessages.h"
#include "wmmApsd.h"
#include "limTrace.h"
#include "limSession.h"
#include "wlan_qct_wda.h"

#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif

#include <limFT.h>

#ifdef VOSS_ENABLED
#include "vos_types.h"
#include "vos_packet.h"
#include "wlan_qct_tl.h"
#include "sysStartup.h"
#endif


static void __limInitScanVars(tpAniSirGlobal pMac)
{
    pMac->lim.gLimUseScanModeForLearnMode = 1;

    pMac->lim.gLimSystemInScanLearnMode = 0;

    // Scan related globals on STA
    pMac->lim.gLimReturnAfterFirstMatch = 0;
    pMac->lim.gLim24Band11dScanDone = 0;
    pMac->lim.gLim50Band11dScanDone = 0;
    pMac->lim.gLimReturnUniqueResults = 0;

    // Background Scan related globals on STA
    pMac->lim.gLimNumOfBackgroundScanSuccess = 0;
    pMac->lim.gLimNumOfConsecutiveBkgndScanFailure = 0;
    pMac->lim.gLimNumOfForcedBkgndScan = 0;
    pMac->lim.gLimBackgroundScanDisable = false;      //based on BG timer
    pMac->lim.gLimForceBackgroundScanDisable = false; //debug control flag
    pMac->lim.gLimBackgroundScanTerminate = TRUE;    //controlled by SME
    pMac->lim.gLimReportBackgroundScanResults = FALSE;    //controlled by SME    

    pMac->lim.gLimCurrentScanChannelId = 0;
    pMac->lim.gpLimMlmScanReq = NULL;
    pMac->lim.gLimMlmScanResultLength = 0;
    pMac->lim.gLimSmeScanResultLength = 0;

    palZeroMemory(pMac->hHdd, pMac->lim.gLimCachedScanHashTable,
                    sizeof(pMac->lim.gLimCachedScanHashTable));

#if defined(ANI_PRODUCT_TYPE_CLIENT) || defined(ANI_AP_CLIENT_SDK)
    pMac->lim.gLimBackgroundScanChannelId = 0;
    pMac->lim.gLimBackgroundScanStarted = 0;
    pMac->lim.gLimRestoreCBNumScanInterval = LIM_RESTORE_CB_NUM_SCAN_INTERVAL_DEFAULT;
    pMac->lim.gLimRestoreCBCount = 0;
    palZeroMemory(pMac->hHdd, pMac->lim.gLimLegacyBssidList, sizeof(pMac->lim.gLimLegacyBssidList));
#endif

    /* Fill in default values */
    pMac->lim.gLimTriggerBackgroundScanDuringQuietBss = 0;

#ifdef ANI_AP_SDK
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimScanDurationConvert, sizeof(tLimScanDurationConvert)); /* Used to store converted scan duration values in TU and TICKS */
#endif /* ANI_AP_SDK */

    // abort scan is used to abort an on-going scan
    pMac->lim.abortScan = 0;
    palZeroMemory(pMac->hHdd, &pMac->lim.scanChnInfo, sizeof(tLimScanChnInfo));

//WLAN_SUSPEND_LINK Related
    pMac->lim.gpLimSuspendCallback = NULL;
    pMac->lim.gpLimResumeCallback = NULL;
//end WLAN_SUSPEND_LINK Related
}


static void __limInitBssVars(tpAniSirGlobal pMac)
{

    palZeroMemory(pMac->hHdd,  (void*)pMac->lim.gpSession,   sizeof(*pMac->lim.gpSession)*pMac->lim.maxBssId);


    //pMac->lim.gpLimStartBssReq = NULL;

#if defined(ANI_PRODUCT_TYPE_AP)
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimNeighborBssList, sizeof(tSirMultipleNeighborBssInfo));
#endif



/* These global variables are moved to session table and intialization is done during session creation Oct 9th Review */
#if 0

   // Place holder for BSS description that we're
   // currently joined with
   palZeroMemory(pMac->hHdd, &pMac->lim.gLimCurrentBssId, sizeof(tSirMacAddr));
   pMac->lim.gLimCurrentChannelId = HAL_INVALID_CHANNEL_ID;
   palZeroMemory(pMac->hHdd, &pMac->lim.gLimCurrentSSID, sizeof(tSirMacSSid));
   pMac->lim.gLimCurrentBssCaps = 0;
   QosCaps is a bit map of various qos capabilities - see defn above
   pMac->lim.gLimCurrentBssQosCaps = 0;
   pMac->lim.gLimCurrentBssPropCap = 0;
   pMac->lim.gLimSentCapsChangeNtf = 0;

   // Place holder for BSS description that
   // we're currently Reassociating
   palZeroMemory(pMac->hHdd, &pMac->lim.gLimReassocBssId, sizeof(tSirMacAddr));
   pMac->lim.gLimReassocChannelId = 0;
   palZeroMemory(pMac->hHdd, &pMac->lim.gLimReassocSSID, sizeof(tSirMacSSid));
   pMac->lim.gLimReassocBssCaps = 0;
   pMac->lim.gLimReassocBssQosCaps = 0;
   pMac->lim.gLimReassocBssPropCap = 0;
 #endif

    /* This is for testing purposes only, be default should always be off */
    pMac->lim.gLimForceNoPropIE = 0;

   // pMac->lim.gLimBssIdx = 0;

    pMac->lim.gpLimMlmSetKeysReq = NULL;
    pMac->lim.gpLimMlmRemoveKeyReq = NULL;
  //  pMac->lim.gLimStaid = 0; //TO SUPPORT BT-AMP

}


static void __limInitStatsVars(tpAniSirGlobal pMac)
{
    pMac->lim.gLimNumBeaconsRcvd = 0;
    pMac->lim.gLimNumBeaconsIgnored = 0;

    pMac->lim.gLimNumDeferredMsgs = 0;

    /// Variable to keep track of number of currently associated STAs
    pMac->lim.gLimNumOfCurrentSTAs = 0;
    pMac->lim.gLimNumOfAniSTAs = 0;      // count of ANI peers

    /// This indicates number of RXed Beacons during HB period
    //pMac->lim.gLimRxedBeaconCntDuringHB = 0;

    // Heart-Beat interval value
    pMac->lim.gLimHeartBeatCount = 0;

    // Statistics to keep track of no. beacons rcvd in heart beat interval
    palZeroMemory(pMac->hHdd, pMac->lim.gLimHeartBeatBeaconStats, sizeof(pMac->lim.gLimHeartBeatBeaconStats));

#ifdef WLAN_DEBUG    
    // Debug counters
    pMac->lim.numTot = 0;
    pMac->lim.numBbt = 0;
    pMac->lim.numProtErr = 0;
    pMac->lim.numLearn = 0;
    pMac->lim.numLearnIgnore = 0;
    pMac->lim.numSme = 0;
    palZeroMemory(pMac->hHdd, pMac->lim.numMAC, sizeof(pMac->lim.numMAC));
    pMac->lim.gLimNumAssocReqDropInvldState = 0;
    pMac->lim.gLimNumAssocReqDropACRejectTS = 0;
    pMac->lim.gLimNumAssocReqDropACRejectSta = 0;
    pMac->lim.gLimNumReassocReqDropInvldState = 0;
    pMac->lim.gLimNumHashMissIgnored = 0;
    pMac->lim.gLimUnexpBcnCnt = 0;
    pMac->lim.gLimBcnSSIDMismatchCnt = 0;
    pMac->lim.gLimNumLinkEsts = 0;
    pMac->lim.gLimNumRxCleanup = 0;
    pMac->lim.gLim11bStaAssocRejectCount = 0;
#endif    
}



static void __limInitStates(tpAniSirGlobal pMac)
{
    // Counts Heartbeat failures
    pMac->lim.gLimHBfailureCntInLinkEstState = 0;
    pMac->lim.gLimProbeFailureAfterHBfailedCnt = 0;
    pMac->lim.gLimHBfailureCntInOtherStates = 0;
    pMac->lim.gLimRspReqd = 0;
    pMac->lim.gLimPrevSmeState = eLIM_SME_OFFLINE_STATE;

    /// MLM State visible across all Sirius modules
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, NO_SESSION, eLIM_MLM_IDLE_STATE));
    pMac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;

    /// Previous MLM State
    pMac->lim.gLimPrevMlmState = eLIM_MLM_OFFLINE_STATE;

#ifdef GEN4_SCAN
    // LIM to HAL SCAN Management Message Interface states
    pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;
#endif // GEN4_SCAN

#ifdef FEATURE_WLAN_INTEGRATED_SOC
    /**
     * Initialize state to eLIM_SME_OFFLINE_STATE
     */
    pMac->lim.gLimSmeState     = eLIM_SME_OFFLINE_STATE;
#else
    /**
     * Initialize state to suspended state and wait for
     * HAL to send LIM_RESUME_ACTIVITY_NTF message.
     */
    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, NO_SESSION, pMac->lim.gLimSmeState));
    pMac->lim.gLimSmeState     = eLIM_SME_SUSPEND_STATE;
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

    /**
     * By default assume 'unknown' role. This will be updated
     * when SME_START_BSS_REQ is received.
     */

    palZeroMemory(pMac->hHdd, &pMac->lim.gLimOverlap11gParams, sizeof(tLimProtStaParams));
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimOverlap11aParams, sizeof(tLimProtStaParams));
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimOverlapHt20Params, sizeof(tLimProtStaParams));
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimOverlapNonGfParams, sizeof(tLimProtStaParams));
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimNoShortParams, sizeof(tLimNoShortParams));
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimNoShortSlotParams, sizeof(tLimNoShortSlotParams));

    pMac->lim.gLimPhyMode = 0; 
    pMac->lim.scanStartTime = 0;    // used to measure scan time

    palZeroMemory(pMac->hHdd, pMac->lim.gLimMyMacAddr, sizeof(pMac->lim.gLimMyMacAddr));
    pMac->lim.ackPolicy = 0;

#if 0 /* Moving all these to session specific elements */
    pMac->lim.gLimQosEnabled = 0; //11E
    pMac->lim.gLimWmeEnabled = 0; //WME
    pMac->lim.gLimWsmEnabled = 0; //WSM
    pMac->lim.gLimHcfEnabled = 0;
    pMac->lim.gLim11dEnabled = 0;
#endif

    pMac->lim.gLimProbeRespDisableFlag = 0; // control over probe response
}

static void __limInitVars(tpAniSirGlobal pMac)
{

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimAlternateRadioList, sizeof(tSirMultipleAlternateRadioInfo));
#endif

    // Place holder for Measurement Req/Rsp/Ind related info
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    pMac->lim.gpLimMeasReq = NULL;
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimMeasParams, sizeof(tLimMeasParams));
    pMac->lim.gpLimMeasData = NULL;
#endif

    // WDS info
    pMac->lim.gLimNumWdsInfoInd = 0;
    pMac->lim.gLimNumWdsInfoSet = 0;
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimWdsInfo, sizeof(tSirWdsInfo));
    /* initialize some parameters */
    limInitWdsInfoParams(pMac);

    // Deferred Queue Paramters
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimDeferredMsgQ, sizeof(tSirAddtsReq));

    // addts request if any - only one can be outstanding at any time
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimAddtsReq, sizeof(tSirAddtsReq));
    pMac->lim.gLimAddtsSent = 0;
    pMac->lim.gLimAddtsRspTimerCount = 0;

    //protection related config cache
    palZeroMemory(pMac->hHdd, &pMac->lim.cfgProtection, sizeof(tCfgProtection));
    pMac->lim.gLimProtectionControl = 0;
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimAlternateRadio, sizeof(tSirAlternateRadioInfo));
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

#if 0
    // 11h Spectrum Management Related Flag
    LIM_SET_RADAR_DETECTED(pMac, eANI_BOOLEAN_FALSE);
    pMac->sys.gSysEnableLearnMode = eANI_BOOLEAN_TRUE;
#endif
    // WMM Related Flag
    pMac->lim.gUapsdEnable = 0;
    pMac->lim.gUapsdPerAcBitmask = 0;
    pMac->lim.gUapsdPerAcTriggerEnableMask = 0;
    pMac->lim.gUapsdPerAcDeliveryEnableMask = 0;

    // QoS-AC Downgrade: Initially, no AC is admitted
    pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] = 0;
    pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] = 0;

    //dialogue token List head/tail for Action frames request sent.
    pMac->lim.pDialogueTokenHead = NULL;
    pMac->lim.pDialogueTokenTail = NULL;

    palZeroMemory(pMac->hHdd, &pMac->lim.tspecInfo, sizeof(tLimTspecInfo) * LIM_NUM_TSPEC_MAX);

    // admission control policy information
    palZeroMemory(pMac->hHdd, &pMac->lim.admitPolicyInfo, sizeof(tLimAdmitPolicyInfo));

    pMac->lim.gLastBeaconDtimCount = 0;
    pMac->lim.gLastBeaconDtimPeriod = 0;

    //Scan in Power Save Flag
    pMac->lim.gScanInPowersave = 0;
}

static void __limInitAssocVars(tpAniSirGlobal pMac)
{
    palZeroMemory(pMac->hHdd, pMac->lim.gpLimAIDpool,
                  sizeof(*pMac->lim.gpLimAIDpool) * (WNI_CFG_ASSOC_STA_LIMIT_STAMAX+1));
    pMac->lim.freeAidHead = 0;
    pMac->lim.freeAidTail = 0;
    pMac->lim.gLimAssocStaLimit = WNI_CFG_ASSOC_STA_LIMIT_STADEF;

    // Place holder for current authentication request
    // being handled
    pMac->lim.gpLimMlmAuthReq = NULL;
    //pMac->lim.gpLimMlmJoinReq = NULL;

    /// MAC level Pre-authentication related globals
    pMac->lim.gLimPreAuthChannelNumber = 0;
    pMac->lim.gLimPreAuthType = eSIR_OPEN_SYSTEM;
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimPreAuthPeerAddr, sizeof(tSirMacAddr));
    pMac->lim.gLimNumPreAuthContexts = 0;
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimPreAuthTimerTable, sizeof(tLimPreAuthTable));

    // Placed holder to deauth reason
    pMac->lim.gLimDeauthReasonCode = 0;

    // Place holder for Pre-authentication node list
    pMac->lim.pLimPreAuthList = NULL;

    // Send Disassociate frame threshold parameters
    pMac->lim.gLimDisassocFrameThreshold = LIM_SEND_DISASSOC_FRAME_THRESHOLD;
    pMac->lim.gLimDisassocFrameCredit = 0;

    //One cache for each overlap and associated case.
    palZeroMemory(pMac->hHdd, pMac->lim.protStaOverlapCache, sizeof(tCacheParams) * LIM_PROT_STA_OVERLAP_CACHE_SIZE);
    palZeroMemory(pMac->hHdd, pMac->lim.protStaCache, sizeof(tCacheParams) * LIM_PROT_STA_CACHE_SIZE);

}


static void __limInitTitanVars(tpAniSirGlobal pMac)
{
#if 0
    palZeroMemory(pMac->hHdd, &pMac->lim.gLimChannelSwitch, sizeof(tLimChannelSwitchInfo));
    pMac->lim.gLimChannelSwitch.state               = eLIM_CHANNEL_SWITCH_IDLE;
    pMac->lim.gLimChannelSwitch.secondarySubBand    = PHY_SINGLE_CHANNEL_CENTERED;
#endif
    // Debug workaround for BEACON's
    // State change triggered by "dump 222"
    pMac->lim.gLimScanOverride = 1;
    pMac->lim.gLimScanOverrideSaved = eSIR_ACTIVE_SCAN;
    pMac->lim.gLimTitanStaCount = 0;
    pMac->lim.gLimBlockNonTitanSta = 0;
}

static void __limInitHTVars(tpAniSirGlobal pMac)
{
    pMac->lim.htCapabilityPresentInBeacon = 0;
    pMac->lim.gHTGreenfield = 0;
    pMac->lim.gHTShortGI40Mhz = 0;
    pMac->lim.gHTShortGI20Mhz = 0;
    pMac->lim.gHTMaxAmsduLength = 0;
    pMac->lim.gHTDsssCckRate40MHzSupport = 0;
    pMac->lim.gHTPSMPSupport = 0;
    pMac->lim.gHTLsigTXOPProtection = 0;
    pMac->lim.gHTMIMOPSState = eSIR_HT_MIMO_PS_STATIC;
    pMac->lim.gHTAMpduDensity = 0;

    pMac->lim.gMaxAmsduSizeEnabled = false;
    pMac->lim.gHTMaxRxAMpduFactor = 0;
    pMac->lim.gHTServiceIntervalGranularity = 0;
    pMac->lim.gHTControlledAccessOnly = 0;
    pMac->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
    pMac->lim.gHTPCOActive = 0;

    pMac->lim.gHTPCOPhase = 0;
    pMac->lim.gHTSecondaryBeacon = 0;
    pMac->lim.gHTDualCTSProtection = 0;
    pMac->lim.gHTSTBCBasicMCS = 0;
    pMac->lim.gAddBA_Declined = 0;               // Flag to Decline the BAR if the particular bit (0-7) is being set   
}

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
static tSirRetStatus __limInitConfig( tpAniSirGlobal pMac )
{
   tANI_U32 val1, val2, val3;
   tANI_U16 val16;
   tANI_U8 val8;
   tSirMacHTCapabilityInfo   *pHTCapabilityInfo;
   tSirMacHTInfoField1       *pHTInfoField1;
   tpSirPowerSaveCfg          pPowerSaveConfig;
   tSirMacHTParametersInfo   *pAmpduParamInfo;

   /* Read all the CFGs here that were updated before peStart is called */
   /* All these CFG READS/WRITES are only allowed in init, at start when there is no session 
    * and they will be used throughout when there is no session
    */

   if(wlan_cfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &val1) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve HT Cap CFG\n"));)
      return eSIR_FAILURE;
   }

   if(wlan_cfgGetInt(pMac, WNI_CFG_CHANNEL_BONDING_MODE, &val2) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve Channel Bonding CFG\n"));)
      return eSIR_FAILURE;
   }
   val16 = ( tANI_U16 ) val1;
   pHTCapabilityInfo = ( tSirMacHTCapabilityInfo* ) &val16;
  
   //channel bonding mode could be set to anything from 0 to 4(Titan had these 
   // modes But for Taurus we have only two modes: enable(>0) or disable(=0)
   pHTCapabilityInfo->supportedChannelWidthSet = val2 ? 
     WNI_CFG_CHANNEL_BONDING_MODE_ENABLE : WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
   if(cfgSetInt(pMac, WNI_CFG_HT_CAP_INFO, *(tANI_U16*)pHTCapabilityInfo) 
      != eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("could not update HT Cap Info CFG\n"));)
      return eSIR_FAILURE;
   }

   if(wlan_cfgGetInt(pMac, WNI_CFG_HT_INFO_FIELD1, &val1) != eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve HT INFO Field1 CFG\n"));)
      return eSIR_FAILURE;
   }

   val8 = ( tANI_U8 ) val1;
   pHTInfoField1 = ( tSirMacHTInfoField1* ) &val8;
   pHTInfoField1->recommendedTxWidthSet = 
     (tANI_U8)pHTCapabilityInfo->supportedChannelWidthSet;
   if(cfgSetInt(pMac, WNI_CFG_HT_INFO_FIELD1, *(tANI_U8*)pHTInfoField1) 
      != eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("could not update HT Info Field\n"));)
      return eSIR_FAILURE;
   }

   /* WNI_CFG_HEART_BEAT_THRESHOLD */
  
   if( wlan_cfgGetInt(pMac, WNI_CFG_HEART_BEAT_THRESHOLD, &val1) != 
       eSIR_SUCCESS ) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve WNI_CFG_HEART_BEAT_THRESHOLD CFG\n"));) 
      return eSIR_FAILURE;
   }
   if(!val1) 
   {
      limDeactivateAndChangeTimer(pMac, eLIM_HEART_BEAT_TIMER);
      pMac->sys.gSysEnableLinkMonitorMode = 0;
   } 
   else 
   {
      //No need to activate the timer during init time.
      pMac->sys.gSysEnableLinkMonitorMode = 1;
   }

   /* WNI_CFG_SHORT_GI_20MHZ */

   if (wlan_cfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &val1) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve HT Cap CFG\n"));)
      return eSIR_FAILURE;
   }
   if (wlan_cfgGetInt(pMac, WNI_CFG_SHORT_GI_20MHZ, &val2) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve shortGI 20Mhz CFG\n"));)
      return eSIR_FAILURE;
   }
   if (wlan_cfgGetInt(pMac, WNI_CFG_SHORT_GI_40MHZ, &val3) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve shortGI 40Mhz CFG\n"));)
      return eSIR_FAILURE;
   }

   val16 = ( tANI_U16 ) val1;
   pHTCapabilityInfo = ( tSirMacHTCapabilityInfo* ) &val16;
   pHTCapabilityInfo->shortGI20MHz = (tANI_U16)val2;
   pHTCapabilityInfo->shortGI40MHz = (tANI_U16)val3;

   if(cfgSetInt(pMac,  WNI_CFG_HT_CAP_INFO, *(tANI_U16*)pHTCapabilityInfo) != 
      eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("could not update HT Cap Info CFG\n"));)
      return eSIR_FAILURE;
   }

   /* WNI_CFG_MAX_RX_AMPDU_FACTOR */

   if (wlan_cfgGetInt(pMac, WNI_CFG_HT_AMPDU_PARAMS, &val1) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve HT AMPDU Param CFG\n"));)
      return eSIR_FAILURE;
   }
   if (wlan_cfgGetInt(pMac, WNI_CFG_MAX_RX_AMPDU_FACTOR, &val2) != eSIR_SUCCESS) 
   {
      PELOGE(limLog(pMac, LOGE, FL("could not retrieve AMPDU Factor CFG\n"));)
      return eSIR_FAILURE;
   }
   val16 = ( tANI_U16 ) val1;
   pAmpduParamInfo = ( tSirMacHTParametersInfo* ) &val16;
   pAmpduParamInfo->maxRxAMPDUFactor = (tANI_U8)val2;
   if(cfgSetInt(pMac,  WNI_CFG_HT_AMPDU_PARAMS, *(tANI_U8*)pAmpduParamInfo) != 
      eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("could not update HT AMPDU Param CFG\n"));)
      return eSIR_FAILURE;
   }

   /* WNI_CFG_SHORT_PREAMBLE - this one is not updated in 
      limHandleCFGparamUpdate do we want to update this? */
   if(wlan_cfgGetInt(pMac, WNI_CFG_SHORT_PREAMBLE, &val1) != eSIR_SUCCESS)
   {
      limLog(pMac, LOGP, FL("cfg get short preamble failed\n"));
      return eSIR_FAILURE;
   }

   /* WNI_CFG_MAX_PS_POLL */

   /* Allocate and fill in power save configuration. */
   if (palAllocateMemory(pMac->hHdd, (void **)&pPowerSaveConfig,
        sizeof(tSirPowerSaveCfg)) != eHAL_STATUS_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("LIM: Cannot allocate memory for power save "
                                  "configuration\n"));) 
      return eSIR_FAILURE;
   }
       
   /* This context should be valid if power-save configuration message has been
    * already dispatched during initialization process. Re-using the present
    * configuration mask
    */
   palCopyMemory(pMac->hHdd, pPowerSaveConfig, (tANI_U8 *)&pMac->pmm.gPmmCfg, 
                 sizeof(tSirPowerSaveCfg));

   /* Note: it is okay to do this since DAL/HAL is alrady started */
   if ( (pmmSendPowerSaveCfg(pMac, pPowerSaveConfig)) != eSIR_SUCCESS)
   {
      PELOGE(limLog(pMac, LOGE, FL("LIM: pmmSendPowerSaveCfg() failed \n"));)
      return eSIR_FAILURE;
   }

   /* WNI_CFG_BG_SCAN_CHANNEL_LIST_CHANNEL_LIST */

#if (WNI_POLARIS_FW_PRODUCT == WLAN_STA) || defined(ANI_AP_CLIENT_SDK)
   PELOG1(limLog(pMac, LOG1,
      FL("VALID_CHANNEL_LIST has changed, reset next bg scan channel\n"));)
   pMac->lim.gLimBackgroundScanChannelId = 0;
#endif

   /* WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA - not needed */

   /* This was initially done after resume notification from HAL. Now, DAL is
      started before PE so this can be done here */
   handleHTCapabilityandHTInfo(pMac, NULL);

   return eSIR_SUCCESS;
}
#endif  /* FEATURE_WLAN_INTEGRATED_SOC */

/*
   limStart
   This function is to replace the __limProcessSmeStartReq since there is no 
   eWNI_SME_START_REQ post to PE.
*/
tSirRetStatus limStart(tpAniSirGlobal pMac)
{
   tSirResultCodes retCode = eSIR_SUCCESS;

   PELOG1(limLog(pMac, LOG1, FL(" enter\n"));)

   if (pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE)
   {
      pMac->lim.gLimSmeState = eLIM_SME_IDLE_STATE;

      MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, NO_SESSION, pMac->lim.gLimSmeState));

      // By default do not return after first scan match
      pMac->lim.gLimReturnAfterFirstMatch = 0;

      // Initialize MLM state machine
      limInitMlm(pMac);

      // By default return unique scan results
      pMac->lim.gLimReturnUniqueResults = true;
      pMac->lim.gLimSmeScanResultLength = 0;
   }
   else
   {
      /**
      * Should not have received eWNI_SME_START_REQ in states
      * other than OFFLINE. Return response to host and
      * log error
      */
      limLog(pMac, LOGE, FL("Invalid SME state %X\n"),pMac->lim.gLimSmeState );
      retCode = eSIR_FAILURE;
   }
   
   return retCode;
}

/**
 * limInitialize()
 *
 *FUNCTION:
 * This function is called from LIM thread entry function.
 * LIM related global data structures are initialized in this function.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to global MAC structure
 * @return None
 */

tSirRetStatus
limInitialize(tpAniSirGlobal pMac)
{
    tSirRetStatus status = eSIR_SUCCESS;

    __limInitAssocVars(pMac);
    __limInitVars(pMac);
    __limInitStates(pMac);
    __limInitStatsVars(pMac);
    __limInitBssVars(pMac);
    __limInitScanVars(pMac);
    __limInitHTVars(pMac);
    __limInitTitanVars(pMac);

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
    status = limStart(pMac);
    if(eSIR_SUCCESS != status)
    {
        return status;
    }
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

    /*
     * MLM will be intitalized when 'START' request comes from SME.
     * limInitMlm calls limCreateTimers, which actually relies on
     * CFG to be downloaded. So it should not be called as part of
     * peStart, as CFG download is happening after peStart.
     */
    //limInitMlm(pMac);
    // Initializations for maintaining peers in IBSS
    limIbssInit(pMac);

    pmmInitialize(pMac);

    
#if defined WLAN_FEATURE_VOWIFI
    rrmInitialize(pMac);
#endif
#if defined WLAN_FEATURE_VOWIFI_11R
    limFTOpen(pMac);
#endif

#ifdef WLAN_FEATURE_P2P
    vos_list_init(&pMac->lim.gLimMgmtFrameRegistratinQueue);
#endif    

#if 0

    vos_trace_setLevel(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR);
    vos_trace_setLevel(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_WARN);
    vos_trace_setLevel(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_FATAL);

    vos_trace_setLevel(VOS_MODULE_ID_HAL, VOS_TRACE_LEVEL_WARN);
    vos_trace_setLevel(VOS_MODULE_ID_HAL, VOS_TRACE_LEVEL_ERROR);

    vos_trace_setLevel(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_WARN);
    vos_trace_setLevel(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR);
    vos_trace_setLevel(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR);
    
    vos_trace_setLevel(VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_ERROR);
    
    vos_trace_setLevel(VOS_MODULE_ID_SSC, VOS_TRACE_LEVEL_ERROR);
    
    vos_trace_setLevel(VOS_MODULE_ID_SAL, VOS_TRACE_LEVEL_ERROR);
    vos_trace_setLevel(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR);

    vos_trace_setLevel(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR);

    
    vos_trace_setLevel(VOS_MODULE_ID_BAL, VOS_TRACE_LEVEL_ERROR);
    
    vos_trace_setLevel(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR);
#endif
    MTRACE(limTraceInit(pMac));

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
    //Initialize the configurations needed by PE
    if( eSIR_FAILURE == __limInitConfig(pMac))
    {
       //We need to undo everything in limStart
       limCleanupMlm(pMac);
       return eSIR_FAILURE;
    }

   //initialize the TSPEC admission control table.
   //Note that this was initially done after resume notification from HAL.
   //Now, DAL is started before PE so this can be done here
   limAdmitControlInit(pMac);
   limRegisterHalIndCallBack(pMac);
#endif /*FEATURE_WLAN_INTEGRATED_SOC*/

   return status;
            
} /*** end limInitialize() ***/



/**
 * limCleanup()
 *
 *FUNCTION:
 * This function is called upon reset or persona change
 * to cleanup LIM state
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limCleanup(tpAniSirGlobal pMac)
{
#ifdef VOSS_ENABLED
    v_PVOID_t pvosGCTx;
    VOS_STATUS retStatus;
#endif

#ifdef WLAN_FEATURE_P2P
//Before destroying the list making sure all the nodes have been deleted.
//Which should be the normal case, but a memory leak has been reported.

    tpLimMgmtFrameRegistration pLimMgmtRegistration = NULL;
    
    while(vos_list_remove_front(&pMac->lim.gLimMgmtFrameRegistratinQueue,
            (vos_list_node_t**)&pLimMgmtRegistration) == VOS_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                FL("Fixing leak! Deallocating pLimMgmtRegistration node"));

        palFreeMemory(pMac, pLimMgmtRegistration);
    }

    vos_list_destroy(&pMac->lim.gLimMgmtFrameRegistratinQueue);
#endif

    limCleanupMlm(pMac);
    limCleanupLmm(pMac);

    // free up preAuth table
    if (pMac->lim.gLimPreAuthTimerTable.pTable != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gLimPreAuthTimerTable.pTable);
        pMac->lim.gLimPreAuthTimerTable.pTable = NULL;
        pMac->lim.gLimPreAuthTimerTable.numEntry = 0;
    }

    if(NULL != pMac->lim.pDialogueTokenHead)
    {
        limDeleteDialogueTokenList(pMac);
    }

    if(NULL != pMac->lim.pDialogueTokenTail)
    {
        palFreeMemory(pMac->hHdd, (void *) pMac->lim.pDialogueTokenTail);
        pMac->lim.pDialogueTokenTail = NULL;
    }

    # if 0
    if (pMac->lim.gpLimStartBssReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimStartBssReq);
        pMac->lim.gpLimStartBssReq = NULL;
    }
    #endif

    if (pMac->lim.gpLimMlmSetKeysReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimMlmSetKeysReq);
        pMac->lim.gpLimMlmSetKeysReq = NULL;
    }

    #if 0
    if (pMac->lim.gpLimJoinReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimJoinReq);
        pMac->lim.gpLimJoinReq = NULL;
    }
    #endif

    if (pMac->lim.gpLimMlmAuthReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimMlmAuthReq);
        pMac->lim.gpLimMlmAuthReq = NULL;
    }

#if 0
    if (pMac->lim.gpLimMlmJoinReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimMlmJoinReq);
        pMac->lim.gpLimMlmJoinReq = NULL;
    }
#endif

    #if 0
    if (pMac->lim.gpLimReassocReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimReassocReq);
        pMac->lim.gpLimReassocReq = NULL;
    }
    #endif

    if (pMac->lim.gpLimMlmRemoveKeyReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimMlmRemoveKeyReq);
        pMac->lim.gpLimMlmRemoveKeyReq = NULL;
    }

    if (pMac->lim.gpLimMlmScanReq != NULL)
    {
        palFreeMemory(pMac->hHdd, pMac->lim.gpLimMlmScanReq);
        pMac->lim.gpLimMlmScanReq = NULL;
    }

#if 0
    if(NULL != pMac->lim.beacon)
    {
        palFreeMemory(pMac->hHdd, (void*) pMac->lim.beacon);
        pMac->lim.beacon = NULL;
     }
#endif
    #if 0
    if(NULL != pMac->lim.assocReq)
    {
        palFreeMemory(pMac->hHdd, (void*) pMac->lim.assocReq);
        pMac->lim.assocReq= NULL;
     }
    #endif

#if 0
    if(NULL != pMac->lim.assocRsp)
    {
        palFreeMemory(pMac->hHdd, (void*) pMac->lim.assocRsp);
        pMac->lim.assocRsp= NULL;
     }
#endif
    // Now, finally reset the deferred message queue pointers
    limResetDeferredMsgQ(pMac);

#ifdef VOSS_ENABLED

    pvosGCTx = vos_get_global_context(VOS_MODULE_ID_PE, (v_VOID_t *) pMac);
    retStatus = WLANTL_DeRegisterMgmtFrmClient(pvosGCTx);

    if ( retStatus != VOS_STATUS_SUCCESS )
        PELOGE(limLog(pMac, LOGE, FL("DeRegistering the PE Handle with TL has failed bailing out...\n"));)
#endif

#if defined WLAN_FEATURE_VOWIFI
    rrmCleanup(pMac);
#endif
#if defined WLAN_FEATURE_VOWIFI_11R
    limFTCleanup(pMac);
#endif

} /*** end limCleanup() ***/


/** -------------------------------------------------------------
\fn peOpen
\brief will be called in Open sequence from macOpen
\param   tpAniSirGlobal pMac
\param   tHalOpenParameters *pHalOpenParam
\return  tSirRetStatus
  -------------------------------------------------------------*/

tSirRetStatus peOpen(tpAniSirGlobal pMac, tMacOpenParameters *pMacOpenParam)
{
    pMac->lim.maxBssId = pMacOpenParam->maxBssId;
    pMac->lim.maxStation = pMacOpenParam->maxStation;

    if ((pMac->lim.maxBssId == 0) || (pMac->lim.maxStation == 0))
    {
         PELOGE(limLog(pMac, LOGE, FL("max number of Bssid or Stations cannot be zero!\n"));)
         return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->lim.limTimers.gpLimCnfWaitTimer, sizeof(TX_TIMER)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->lim.gpLimAIDpool, 
              sizeof(*pMac->lim.gpLimAIDpool) * (WNI_CFG_ASSOC_STA_LIMIT_STAMAX+1)))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
        (void **) &pMac->lim.gpSession, sizeof(tPESession)* pMac->lim.maxBssId))
    {
        limLog(pMac, LOGE, FL("memory allocate failed!\n"));
        return eSIR_FAILURE;
    }
 
    palZeroMemory(pMac->hHdd, pMac->lim.gpSession, sizeof(tPESession)*pMac->lim.maxBssId);


 /*
    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->dph.dphHashTable.pHashTable, sizeof(tpDphHashNode)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->dph.dphHashTable.pDphNodeArray, sizeof(tDphHashNode)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }
    */

#ifdef WLAN_SOFTAP_FEATURE
    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->pmm.gPmmTim.pTim, sizeof(tANI_U8)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed for pTim!\n"));)
        return eSIR_FAILURE;
    }
    palZeroMemory(pMac->hHdd, pMac->pmm.gPmmTim.pTim, sizeof(tANI_U8)*pMac->lim.maxStation);
#endif

#ifdef ANI_PRODUCT_TYPE_AP

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
                  (void **) &pMac->pmm.gPmmTim.pStaInfo, sizeof(*pMac->pmm.gPmmTim.pStaInfo) * pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed for pStaInfo!\n"));)
        return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->pmm.gpPmmStaState, sizeof(tPmmStaState)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }

    if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
              (void **) &pMac->pmm.gpPmmPSState, sizeof(tANI_U8)*pMac->lim.maxStation))
    {
        PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!\n"));)
        return eSIR_FAILURE;
    }
#endif

#ifdef WLAN_FEATURE_P2P
    pMac->lim.actionFrameSessionId = 0xff;
#endif

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn peClose
\brief will be called in close sequence from macClose
\param   tpAniSirGlobal pMac
\return  tSirRetStatus
  -------------------------------------------------------------*/

tSirRetStatus peClose(tpAniSirGlobal pMac)
{
    tANI_U8 i;

    if (ANI_DRIVER_TYPE(pMac) == eDRIVER_TYPE_MFG)
        return eSIR_SUCCESS;
    
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            peDeleteSession(pMac,&pMac->lim.gpSession[i]);
        }
    }
    palFreeMemory(pMac->hHdd, pMac->lim.limTimers.gpLimCnfWaitTimer);
    pMac->lim.limTimers.gpLimCnfWaitTimer = NULL;
    palFreeMemory(pMac->hHdd, pMac->lim.gpLimAIDpool);
    pMac->lim.gpLimAIDpool = NULL;
    
    palFreeMemory(pMac->hHdd, pMac->lim.gpSession);
    pMac->lim.gpSession = NULL;
    /*
    palFreeMemory(pMac->hHdd, pMac->dph.dphHashTable.pHashTable);
    pMac->dph.dphHashTable.pHashTable = NULL;
    palFreeMemory(pMac->hHdd, pMac->dph.dphHashTable.pDphNodeArray);
    pMac->dph.dphHashTable.pDphNodeArray = NULL;
    */
#ifdef WLAN_SOFTAP_FEATURE
    palFreeMemory(pMac->hHdd, pMac->pmm.gPmmTim.pTim);
    pMac->pmm.gPmmTim.pTim = NULL;
#endif
#ifdef ANI_PRODUCT_TYPE_AP
    palFreeMemory(pMac->hHdd, pMac->pmm.gPmmTim.pStaInfo);
    pMac->pmm.gPmmTim.pStaInfo = NULL;
    palFreeMemory(pMac->hHdd, pMac->pmm.gpPmmStaState);
    pMac->pmm.gpPmmStaState = NULL;
    palFreeMemory(pMac->hHdd, pMac->pmm.gpPmmPSState);
    pMac->pmm.gpPmmPSState = NULL;
#endif

    return eSIR_SUCCESS;
}

/** -------------------------------------------------------------
\fn peStart
\brief will be called in start sequence from macStart
\param   tpAniSirGlobal pMac
\return none
  -------------------------------------------------------------*/

tSirRetStatus peStart(tpAniSirGlobal pMac)
{
    tSirRetStatus status = eSIR_SUCCESS;

    status = limInitialize(pMac);
#if defined(ANI_LOGDUMP)
    limDumpInit(pMac);
#endif //#if defined(ANI_LOGDUMP)

    return status;
}

/** -------------------------------------------------------------
\fn peStop
\brief will be called in stop sequence from macStop
\param   tpAniSirGlobal pMac
\return none
  -------------------------------------------------------------*/

void peStop(tpAniSirGlobal pMac)
{
    limCleanup(pMac);
    SET_LIM_MLM_STATE(pMac, eLIM_MLM_OFFLINE_STATE);
    return;
}

/** -------------------------------------------------------------
\fn peFreeMsg
\brief Called by VOS scheduler (function vos_sched_flush_mc_mqs)
\      to free a given PE message on the TX and MC thread.
\      This happens when there are messages pending in the PE 
\      queue when system is being stopped and reset. 
\param   tpAniSirGlobal pMac
\param   tSirMsgQ       pMsg
\return none
-----------------------------------------------------------------*/
v_VOID_t peFreeMsg( tpAniSirGlobal pMac, tSirMsgQ* pMsg)
{  
    if (pMsg != NULL)
    {
        if (NULL != pMsg->bodyptr)
        {
            if (SIR_BB_XPORT_MGMT_MSG == pMsg->type)
            {
                vos_pkt_return_packet((vos_pkt_t *)pMsg->bodyptr);
            }
            else
            {
                vos_mem_free((v_VOID_t*)pMsg->bodyptr);
            }
        }
        pMsg->bodyptr = 0;
        pMsg->bodyval = 0;
        pMsg->type = 0;
    }
    return;
}


/**
 * The function checks if a particular timer should be allowed
 * into LIM while device is sleeping
 */
tANI_U8 limIsTimerAllowedInPowerSaveState(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
    tANI_U8 retStatus = TRUE;

    if(!limIsSystemInActiveState(pMac))
    {
        switch(pMsg->type)
        {
            /* Don't allow following timer messages if in sleep */
            case SIR_LIM_MIN_CHANNEL_TIMEOUT:
            case SIR_LIM_MAX_CHANNEL_TIMEOUT:
            case SIR_LIM_PERIODIC_PROBE_REQ_TIMEOUT:
                retStatus = FALSE;
                break;
            /* May allow following timer messages in sleep mode */
            case SIR_LIM_HASH_MISS_THRES_TIMEOUT:

            /* Safe to allow as of today, this triggers background scan
             * which will not be started if the device is in power-save mode
             * might need to block in the future if we decide to implement
             * spectrum management
             */
            case SIR_LIM_QUIET_TIMEOUT:

            /* Safe to allow as of today, this triggers background scan
             * which will not be started if the device is in power-save mode
             * might need to block in the future if we decide to implement
             * spectrum management
             */
            case SIR_LIM_QUIET_BSS_TIMEOUT:

            /* Safe to allow this timermessage, triggers background scan
             * which is blocked in sleep mode
             */
            case SIR_LIM_CHANNEL_SCAN_TIMEOUT:

            /* Safe to allow this timer, since, while in IMPS this timer will not
             * be started. In case of BMPS sleep, SoftMAC handles the heart-beat
             * when heart-beat control is handled back to PE, device would have
             * already woken-up due to EXIT_BMPS_IND mesage from SoftMAC
             */
            case SIR_LIM_HEART_BEAT_TIMEOUT:
            case SIR_LIM_PROBE_HB_FAILURE_TIMEOUT:

            /* Safe to allow, PE is not handling this message as of now. May need
             * to block it, basically, free the buffer and restart the timer
             */
            case SIR_LIM_REASSOC_FAIL_TIMEOUT:
            case SIR_LIM_JOIN_FAIL_TIMEOUT:
            case SIR_LIM_ASSOC_FAIL_TIMEOUT:
            case SIR_LIM_AUTH_FAIL_TIMEOUT:
            case SIR_LIM_ADDTS_RSP_TIMEOUT:
                retStatus = TRUE;
                break;

            /* by default allow rest of messages */
            default:
                retStatus = TRUE;
                break;


        }
    }

    return retStatus;

}



/**
 * limPostMsgApi()
 *
 *FUNCTION:
 * This function is called from other thread while posting a
 * message to LIM message Queue gSirLimMsgQ.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMsg - Pointer to the message structure
 * @return None
 */

tANI_U32
limPostMsgApi(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
#ifdef VOSS_ENABLED
    return  vos_mq_post_message(VOS_MQ_ID_PE, (vos_msg_t *) pMsg);


#elif defined(ANI_OS_TYPE_LINUX) || defined(ANI_OS_TYPE_OSX)
    return tx_queue_send(&pMac->sys.gSirLimMsgQ, pMsg, TX_WAIT_FOREVER);

#else
    /* Check if this is a timeout message from a timer
     * and if the timeout message is allowed if the device is in power-save state
     */
    if(!limIsTimerAllowedInPowerSaveState(pMac, pMsg))
    {
        limLog(pMac, LOGW,
                FL("Timeout message %d is not allowed while device is in Power-Save mode\n"),
                pMsg->type);

        return TX_SUCCESS;
    }
    if(pMac->gDriverType != eDRIVER_TYPE_MFG)
    {
        limMessageProcessor(pMac, pMsg);
    }

    return TX_SUCCESS;

#endif
} /*** end limPostMsgApi() ***/


/*--------------------------------------------------------------------------

  \brief pePostMsgApi() - A wrapper function to post message to Voss msg queues

  This function can be called by legacy code to post message to voss queues OR
  legacy code may keep on invoking 'limPostMsgApi' to post the message to voss queue
  for dispatching it later.

  \param pMac - Pointer to Global MAC structure
  \param pMsg - Pointer to the message structure

  \return  tANI_U32 - TX_SUCCESS for success.

  --------------------------------------------------------------------------*/

tSirRetStatus pePostMsgApi(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
   return (tSirRetStatus)limPostMsgApi(pMac, pMsg);
}

/*--------------------------------------------------------------------------

  \brief peProcessMessages() - Message Processor for PE

  Voss calls this function to dispatch the message to PE

  \param pMac - Pointer to Global MAC structure
  \param pMsg - Pointer to the message structure

  \return  tANI_U32 - TX_SUCCESS for success.

  --------------------------------------------------------------------------*/

tSirRetStatus peProcessMessages(tpAniSirGlobal pMac, tSirMsgQ* pMsg)
{
   if(pMac->gDriverType == eDRIVER_TYPE_MFG)
   {
      return eSIR_SUCCESS;
   }
   /**
    *   If the Message to be handled is for CFG Module call the CFG Msg Handler and
    *   for all the other cases post it to LIM
    */
    if ( SIR_CFG_PARAM_UPDATE_IND != pMsg->type && IS_CFG_MSG(pMsg->type))
        cfgProcessMbMsg(pMac, (tSirMbMsg*)pMsg->bodyptr);
    else
        limMessageProcessor(pMac, pMsg);
    return eSIR_SUCCESS;
}


#ifdef VOSS_ENABLED

// ---------------------------------------------------------------------------
/**
 * peHandleMgmtFrame
 *
 * FUNCTION:
 *    Process the Management frames from TL
 *
 * LOGIC:
 *
 * ASSUMPTIONS: TL sends the packet along with the VOS GlobalContext
 *
 * NOTE:
 *
 * @param pvosGCtx  Global Vos Context
 * @param vossBuff  Packet
 * @return None
 */

VOS_STATUS peHandleMgmtFrame( v_PVOID_t pvosGCtx, v_PVOID_t vosBuff)
{
    tpAniSirGlobal  pMac;
    tpSirMacMgmtHdr mHdr;
    tSirMsgQ        msg;
    vos_pkt_t      *pVosPkt;
    VOS_STATUS      vosStatus;
    v_U8_t         *pRxPacketInfo;

    pVosPkt = (vos_pkt_t *)vosBuff;
    if (NULL == pVosPkt)
    {
        return VOS_STATUS_E_FAILURE;
    }

    pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE, pvosGCtx);
    if (NULL == pMac)
    {
        // cannot log a failure without a valid pMac
        vos_pkt_return_packet(pVosPkt);
        return VOS_STATUS_E_FAILURE;
    }

    vosStatus = WDA_DS_PeekRxPacketInfo( pVosPkt, (void *)&pRxPacketInfo, VOS_FALSE );

    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        vos_pkt_return_packet(pVosPkt);
        return VOS_STATUS_E_FAILURE;
    }


    //
    //  The MPDU header is now present at a certain "offset" in
    // the BD and is specified in the BD itself
    //
    mHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    if(mHdr->fc.type == SIR_MAC_MGMT_FRAME) 
    {
    PELOG1(limLog( pMac, LOG1,
       FL ( "RxBd=%p mHdr=%p Type: %d Subtype: %d  Sizes:FC%d Mgmt%d\n"),
       pRxPacketInfo, mHdr, mHdr->fc.type, mHdr->fc.subType, sizeof(tSirMacFrameCtl), sizeof(tSirMacMgmtHdr) );)

    MTRACE(macTrace(pMac, TRACE_CODE_RX_MGMT, NO_SESSION, 
                        LIM_TRACE_MAKE_RXMGMT(mHdr->fc.subType,  
                        (tANI_U16) (((tANI_U16) (mHdr->seqControl.seqNumHi << 4)) | mHdr->seqControl.seqNumLo)));)
    }


    // Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG
    msg.type = SIR_BB_XPORT_MGMT_MSG;
    msg.bodyptr = vosBuff;
    msg.bodyval = 0;

    if( eSIR_SUCCESS != sysBbtProcessMessageCore( pMac,
                                                  &msg,
                                                  mHdr->fc.type,
                                                  mHdr->fc.subType ))
    {
        vos_pkt_return_packet(pVosPkt);
        limLog( pMac, LOGW,
                FL ( "sysBbtProcessMessageCore failed to process SIR_BB_XPORT_MGMT_MSG\n" ));
        return VOS_STATUS_E_FAILURE;
    }

    return  VOS_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
/**
 * peRegisterTLHandle
 *
 * FUNCTION:
 *    Registers the Handler which, process the Management frames from TL
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @return None
 */

void peRegisterTLHandle(tpAniSirGlobal pMac)
{
    v_PVOID_t pvosGCTx;
    VOS_STATUS retStatus;

    pvosGCTx = vos_get_global_context(VOS_MODULE_ID_PE, (v_VOID_t *) pMac);

    retStatus = WLANTL_RegisterMgmtFrmClient(pvosGCTx, peHandleMgmtFrame);

    if (retStatus != VOS_STATUS_SUCCESS)
        limLog( pMac, LOGP, FL("Registering the PE Handle with TL has failed bailing out...\n"));

}
#endif


/**
 * limIsSystemInScanState()
 *
 *FUNCTION:
 * This function is called by various MAC software modules to
 * determine if System is in Scan/Learn state
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @return true  - System is in Scan/Learn state
 *         false - System is NOT in Scan/Learn state
 */

tANI_U8
limIsSystemInScanState(tpAniSirGlobal pMac)
{
    switch (pMac->lim.gLimSmeState)
    {
        case eLIM_SME_CHANNEL_SCAN_STATE:
        case eLIM_SME_NORMAL_CHANNEL_SCAN_STATE:
        case eLIM_SME_LINK_EST_WT_SCAN_STATE:
        case eLIM_SME_WT_SCAN_STATE:
            // System is in Learn mode
            return true;

        default:
            // System is NOT in Learn mode
            return false;
    }
} /*** end limIsSystemInScanState() ***/



/**
 * limIsSystemInActiveState()
 *
 *FUNCTION:
 * This function is called by various MAC software modules to
 * determine if System is in Active/Wakeup state
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @return true  - System is in Active state
 *         false - System is not in Active state
 */

tANI_U8 limIsSystemInActiveState(tpAniSirGlobal pMac)
{
    switch (pMac->pmm.gPmmState)
    {
        case ePMM_STATE_BMPS_WAKEUP:
        case ePMM_STATE_IMPS_WAKEUP:
        case ePMM_STATE_READY:
            // System is in Active mode
            return true;
        default:
            return false;
          // System is NOT in Active mode
    }
}


#if defined(ANI_PRODUCT_TYPE_AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
/**
 * limCheckAndQuietBSS()
 *
 *FUNCTION:
 * This function is called by limSetLearnMode() to check
 * if BSS needs to be quieted and call limQuietBSS() to
 * send data frame to self for that purpose.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limCheckAndQuietBSS(tpAniSirGlobal pMac)
{
    tANI_U32 dur;

    if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
    {
        // LIM is in AP role. Quiet the BSS before
        // switching to channel to be learned
        if (pMac->lim.gpLimMeasReq->measDuration.shortChannelScanDuration >
            LIM_MAX_QUIET_DURATION)
        {
            // May need to quiet BSS multiple times.
            // Quiet for a limit of 32 msecs on Learn
            // duration for now.
            dur = LIM_MAX_QUIET_DURATION;
        }
        else
        {
            dur =
            pMac->lim.gpLimMeasReq->measDuration.shortChannelScanDuration;
        }
       PELOG3(limLog(pMac, LOG3,
               FL("*** Going to quiet BSS for duration=%d msec\n"),
               dur);)

        limQuietBss(pMac, dur);
    }
} /*** end limCheckAndQuietBSS() ***/
#endif

#if (defined(ANI_PRODUCT_TYPE_AP) || defined(ANI_PRODUCT_TYPE_AP_SDK))
/**
 * limSetLearnMode()
 *
 *FUNCTION:
 * This function is called to setup system into Learn mode
 * to collect DFS measurements.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limSetLearnMode(tpAniSirGlobal pMac)
{
    limSendHalInitScanReq(pMac, eLIM_HAL_INIT_LEARN_WAIT_STATE, eSIR_DONT_CHECK_LINK_TRAFFIC_BEFORE_SCAN);
    return;
} /*** end limSetLearnMode() ***/

/**
 * limContinueChannelLearn()
 *
 *FUNCTION:
 * This function is called to do measurement (learn) on current channel.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 *
 * @return None
 */

void
limContinueChannelLearn(tpAniSirGlobal pMac)
{
    tANI_U8        chanNum;
    tSirMacSSid    ssId;
    tSirMacAddr    bssId = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Time to collect measurements
    chanNum = limGetCurrentLearnChannel(pMac);

    // Switch channel
    pMac->lim.gLimSystemInScanLearnMode = 1;

    if (pMac->lim.gpLimMeasReq->measControl.scanType == eSIR_ACTIVE_SCAN)
    {
        /// Prepare and send Probe Request frame
        ssId.length = 0;
        /* for learning channel, we don't include any additional IE */
        limSendProbeReqMgmtFrame(pMac, &ssId, bssId, chanNum,pMac->lim.gSelfMacAddr, 0 , NULL);
    }

    // Activate Learn duration timer during which
    // DFS measurements are made.
    pMac->lim.gLimMeasParams.shortDurationCount++;
    limDeactivateAndChangeTimer(pMac, eLIM_LEARN_DURATION_TIMER);

    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, NO_SESSION, eLIM_LEARN_DURATION_TIMER));
    if (tx_timer_activate(&pMac->lim.gLimMeasParams.learnDurationTimer)
                                           != TX_SUCCESS)
    {
        /// Could not activate learn duration timer.
        // Log error
        limLog(pMac, LOGP, FL("could not activate learn duration timer\n"));

        return;
    }
} /*** end limContinueChannelLearn() ***/


/**
 * limReEnableLearnMode()
 *
 *FUNCTION:
 * This function is called by various MAC software modules to
 * re-enable Learn mode measurements.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limReEnableLearnMode(tpAniSirGlobal pMac)
{
   PELOG4(limLog(pMac, LOG4, FL("quietEnabled = %d\n"),
                 pMac->lim.gLimSpecMgmt.fQuietEnabled);)

    /** Stop measurement temperorily when radar is detected or channel
     * switch is running as part of periodic DFS */
    if (!pMac->lim.gpLimMeasReq || LIM_IS_RADAR_DETECTED(pMac) ||
            (pMac->lim.gLimSpecMgmt.dot11hChanSwState  == eLIM_11H_CHANSW_RUNNING))
    {
        return;
    }

    if (pMac->lim.gLimSpecMgmt.fQuietEnabled)
    {
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, NO_SESSION, eLIM_QUIET_BSS_TIMER));
#ifdef GEN6_TODO
        /* revisit this piece of code to assign the appropriate sessionId below
         * priority - HIGH
         */
        pMac->lim.limTimers.gLimQuietBssTimer.sessionId = sessionId;
#endif
        if (tx_timer_activate(
                 &pMac->lim.limTimers.gLimQuietBssTimer)
                 != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("could not start Quiet Bss timer\n"));
            return;
        }
        pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
    }
    else
    {
        limDeactivateAndChangeTimer(pMac, eLIM_LEARN_INTERVAL_TIMER);
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, NO_SESSION, eLIM_LEARN_INTERVAL_TIMER));
#ifdef GEN6_TODO
        /* revisit this piece of code to assign the appropriate sessionId below
        */
        pMac->lim.gLimMeasParams.learnIntervalTimer.sessionId = sessionId;
#endif
        if (tx_timer_activate(
                     &pMac->lim.gLimMeasParams.learnIntervalTimer)
                     != TX_SUCCESS)
        {
            /// Could not activate Learn Interval timer.
            // Log error
            limLog(pMac, LOGP, FL("could not start Learn Interval timer\n"));
            return;
        }
    }

    PELOG3(limLog(pMac, LOG3, FL("Re-enabled Learn mode Measurements\n"));)
    pMac->lim.gLimMeasParams.disableMeasurements = 0;

    return;
} /*** end limReEnableLearnMode() ***/

#endif //#if (defined(ANI_PRODUCT_TYPE_AP) || defined(ANI_PRODUCT_TYPE_AP_SDK))


/** 
*\brief limReceivedHBHandler() 
* 
* This function is called by schBeaconProcess() upon
* receiving a Beacon on STA. This also gets called upon
* receiving Probe Response after heat beat failure is
* detected.  
*   
* param pMac - global mac structure
* param channel - channel number indicated in Beacon, Probe Response
* return - none
*/


void
limReceivedHBHandler(tpAniSirGlobal pMac, tANI_U8 channelId, tpPESession psessionEntry)
{
    if((channelId == 0 ) || (channelId == psessionEntry->currentOperChannel) )
    psessionEntry->LimRxedBeaconCntDuringHB++;

    pMac->pmm.inMissedBeaconScenario = FALSE;
} /*** end limReceivedHBHandler() ***/



#if 0
void limResetHBPktCount(tpPESession psessionEntry)
{
    psessionEntry->LimRxedBeaconCntDuringHB = 0;
}
#endif


/*
 * limProcessWdsInfo()
 *
 *FUNCTION:
 * This function is called from schBeaconProcess in BP
 *
 *PARAMS:
 * @param pMac     - Pointer to Global MAC structure
 * @param propIEInfo - proprietary IE info
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 *
 *RETURNS:
 *
 */

void limProcessWdsInfo(tpAniSirGlobal pMac,
                       tSirPropIEStruct propIEInfo)
{
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    tpSirSmeWdsInfoInd  pSirSmeWdsInfoInd;
    tANI_U8                  *pTemp;
    tSirMsgQ            mmhMsg;

    if (propIEInfo.wdsLength &&
        propIEInfo.wdsLength <= ANI_WDS_INFO_MAX_LENGTH)
    {
        if (pMac->lim.gLimWdsInfo.wdsLength)
        {
            if ((propIEInfo.wdsLength ==
                 pMac->lim.gLimWdsInfo.wdsLength) &&
                (palEqualMemory( pMac->hHdd,propIEInfo.wdsData,
                    pMac->lim.gLimWdsInfo.wdsBytes,
                    pMac->lim.gLimWdsInfo.wdsLength) ))

                return; // no difference in WDS info
            else
            {
               PELOG2(limLog(pMac, LOG2,
                       FL("Cached WDS Info: length %d bytes is: "),
                       pMac->lim.gLimWdsInfo.wdsLength);
                sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2,
                            pMac->lim.gLimWdsInfo.wdsBytes,
                            pMac->lim.gLimWdsInfo.wdsLength);)

               PELOG2(limLog(pMac, LOG2, FL("New WDS Info: length %d bytes is: "),
                       propIEInfo.wdsLength);
                sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2,
                            propIEInfo.wdsData,
                            propIEInfo.wdsLength);)

                pMac->lim.gLimWdsInfo.wdsLength = propIEInfo.wdsLength;
                palCopyMemory( pMac->hHdd, pMac->lim.gLimWdsInfo.wdsBytes,
                                propIEInfo.wdsData,
                                propIEInfo.wdsLength);

                // send IND to WSM
                if (eHAL_STATUS_SUCCESS !=
                    palAllocateMemory(pMac->hHdd,
                                      (void **) &pSirSmeWdsInfoInd,
                                      sizeof(tSirSmeWdsInfoInd)))
                {
                    // Log error
                    limLog(pMac, LOGP,
                           FL("memory allocate failed for WDS_INFO_IND\n"));

                    return;
                }

                pSirSmeWdsInfoInd->messageType = eWNI_SME_WDS_INFO_IND;
                pSirSmeWdsInfoInd->length = sizeof(tSirSmeWdsInfoInd);

                pSirSmeWdsInfoInd->wdsInfo.wdsLength =
                    pMac->lim.gLimWdsInfo.wdsLength;

                palCopyMemory( pMac->hHdd, pSirSmeWdsInfoInd->wdsInfo.wdsBytes,
                                pMac->lim.gLimWdsInfo.wdsBytes,
                                pMac->lim.gLimWdsInfo.wdsLength);

                pTemp = (tANI_U8 *) pSirSmeWdsInfoInd;

               PELOG2(limLog(pMac, LOG2,
                       FL("eWNI_SME_WDS_INFO_IND length %d bytes is: "),
                       pSirSmeWdsInfoInd->length);
                sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG2, pTemp,
                            pSirSmeWdsInfoInd->length);)

                mmhMsg.type = eWNI_SME_WDS_INFO_IND;
                mmhMsg.bodyptr = pSirSmeWdsInfoInd;
                mmhMsg.bodyval = 0;
                MTRACE(macTraceMsgTx(pMac, NO_SESSION, mmhMsg.type));
                limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
                pMac->lim.gLimNumWdsInfoInd++;
            }
        }
        else
        {
            // first WDS info
            pMac->lim.gLimWdsInfo.wdsLength = propIEInfo.wdsLength;
            palCopyMemory( pMac->hHdd, pMac->lim.gLimWdsInfo.wdsBytes,
                            propIEInfo.wdsData,
                            propIEInfo.wdsLength);

            PELOG1(limLog(pMac, LOG1, FL("First WDS Info: length %d bytes is:\n"),
                            pMac->lim.gLimWdsInfo.wdsLength);
            sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG1,
                            pMac->lim.gLimWdsInfo.wdsBytes,
                            pMac->lim.gLimWdsInfo.wdsLength);)

        }
    }
    else
    {
       PELOG2(limLog(pMac, LOG2,
               FL("Illegal WDS length = %d\n"),
               propIEInfo.wdsLength);)
    }
#endif
}



/**
 * limInitWdsInfoParams()
 *
 *FUNCTION:
 * This function is called while processing
 * START_BSS/JOIN/REASSOC_REQ  to initialize WDS info
 * ind/set related parameters.
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
limInitWdsInfoParams(tpAniSirGlobal pMac)
{
    pMac->lim.gLimWdsInfo.wdsLength = 0;
    pMac->lim.gLimNumWdsInfoInd     = 0;
    pMac->lim.gLimNumWdsInfoSet     = 0;
} /*** limInitWdsInfoParams() ***/


/** -------------------------------------------------------------
\fn limUpdateOverlapStaParam
\brief Updates overlap cache and param data structure
\param      tpAniSirGlobal    pMac
\param      tSirMacAddr bssId
\param      tpLimProtStaParams pStaParams
\return      None
  -------------------------------------------------------------*/
void
limUpdateOverlapStaParam(tpAniSirGlobal pMac, tSirMacAddr bssId, tpLimProtStaParams pStaParams)
{
    int i;
    if (!pStaParams->numSta)
    {
        palCopyMemory( pMac->hHdd, pMac->lim.protStaOverlapCache[0].addr,
                      bssId,
                      sizeof(tSirMacAddr));
        pMac->lim.protStaOverlapCache[0].active = true;

        pStaParams->numSta = 1;

        return;
    }

    for (i=0; i<LIM_PROT_STA_OVERLAP_CACHE_SIZE; i++)
    {
        if (pMac->lim.protStaOverlapCache[i].active)
        {
            if (palEqualMemory( pMac->hHdd,pMac->lim.protStaOverlapCache[i].addr,
                          bssId,
                          sizeof(tSirMacAddr))) {
                return; }
        }
        else
            break;
    }

    if (i == LIM_PROT_STA_OVERLAP_CACHE_SIZE)
    {
        PELOG1(limLog(pMac, LOG1, FL("Overlap cache is full\n"));)
    }
    else
    {
        palCopyMemory( pMac->hHdd, pMac->lim.protStaOverlapCache[i].addr,
                      bssId,
                      sizeof(tSirMacAddr));
        pMac->lim.protStaOverlapCache[i].active = true;

        pStaParams->numSta++;
    }
}


/**
 * limHandleIBSScoalescing()
 *
 *FUNCTION:
 * This function is called upon receiving Beacon/Probe Response
 * while operating in IBSS mode.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  pBeacon - Parsed Beacon Frame structure
 * @param  pRxPacketInfo - Pointer to RX packet info structure
 *
 * @return Status whether to process or ignore received Beacon Frame
 */

tSirRetStatus
limHandleIBSScoalescing(
    tpAniSirGlobal      pMac,
    tpSchBeaconStruct   pBeacon,
    tANI_U8            *pRxPacketInfo,tpPESession psessionEntry)
{
    tpSirMacMgmtHdr pHdr;
    tSirRetStatus   retCode;

    pHdr = WDA_GET_RX_MAC_HEADER(pRxPacketInfo);
    if ( (!pBeacon->capabilityInfo.ibss) || (limCmpSSid(pMac, &pBeacon->ssId,psessionEntry) != true) )
        /* Received SSID does not match => Ignore received Beacon frame. */
        retCode =  eSIR_LIM_IGNORE_BEACON;
    else
    {
        tANI_U32 ieLen;
        tANI_U16 tsfLater;
        tANI_U8 *pIEs;
        ieLen    = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
        tsfLater = WDA_GET_RX_TSF_LATER(pRxPacketInfo);
        pIEs = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);
        PELOG3(limLog(pMac, LOG3, FL("BEFORE Coalescing tsfLater val :%d"), tsfLater);)
        retCode  = limIbssCoalesce(pMac, pHdr, pBeacon, pIEs, ieLen, tsfLater,psessionEntry);
    }
    return retCode;
} /*** end limHandleIBSScoalescing() ***/



/**
 * limDetectChangeInApCapabilities()
 *
 *FUNCTION:
 * This function is called while SCH is processing
 * received Beacon from AP on STA to detect any
 * change in AP's capabilities. If there any change
 * is detected, Roaming is informed of such change
 * so that it can trigger reassociation.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * Notification is enabled for STA product only since
 * it is not a requirement on BP side.
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  pBeacon   Pointer to parsed Beacon structure
 * @return None
 */

void
limDetectChangeInApCapabilities(tpAniSirGlobal pMac,
                                tpSirProbeRespBeacon pBeacon,
                                tpPESession psessionEntry)
{
#if defined(ANI_PRODUCT_TYPE_CLIENT) || defined(ANI_AP_CLIENT_SDK)
    tANI_U8                 len;
    tSirSmeApNewCaps   apNewCaps;
    tANI_U8            newChannel;
    apNewCaps.capabilityInfo = limGetU16((tANI_U8 *) &pBeacon->capabilityInfo);
    newChannel = (tANI_U8) pBeacon->channelNumber;

    if ((psessionEntry->limSentCapsChangeNtf == false) && 
        (((!limIsNullSsid(&pBeacon->ssId)) && (limCmpSSid(pMac, &pBeacon->ssId, psessionEntry) == false)) || 
        ((SIR_MAC_GET_ESS(apNewCaps.capabilityInfo) != SIR_MAC_GET_ESS(psessionEntry->limCurrentBssCaps)) ||
         (SIR_MAC_GET_PRIVACY(apNewCaps.capabilityInfo) !=   SIR_MAC_GET_PRIVACY(psessionEntry->limCurrentBssCaps)) ||
         (SIR_MAC_GET_SHORT_PREAMBLE(apNewCaps.capabilityInfo) !=  SIR_MAC_GET_SHORT_PREAMBLE(psessionEntry->limCurrentBssCaps)) ||
         (SIR_MAC_GET_QOS(apNewCaps.capabilityInfo) !=   SIR_MAC_GET_QOS(psessionEntry->limCurrentBssCaps)) ||
         (newChannel !=  psessionEntry->currentOperChannel) 
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED)
         || (LIM_BSS_CAPS_GET(HCF, psessionEntry->limCurrentBssQosCaps) !=
          pBeacon->propIEinfo.hcfEnabled)
#endif
         )))
    {

        /**
         * BSS capabilities have changed.
         * Inform Roaming.
         */
        len = sizeof(tSirMacCapabilityInfo) +
              sizeof(tSirMacAddr) + sizeof(tANI_U8) +
              3 * sizeof(tANI_U8) + // reserved fields
              pBeacon->ssId.length + 1;

        palCopyMemory( pMac->hHdd, apNewCaps.bssId,
                      psessionEntry->bssId,
                      sizeof(tSirMacAddr));
        if (newChannel != psessionEntry->currentOperChannel)
        {
            PELOGE(limLog(pMac, LOGE, FL("Channel Change from %d --> %d  - "
                                         "Ignoring beacon!\n"), 
                          psessionEntry->currentOperChannel, newChannel);)
            return;
        }
        else
            apNewCaps.channelId = psessionEntry->currentOperChannel;
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &apNewCaps.ssId,
                      (tANI_U8 *) &pBeacon->ssId,
                      pBeacon->ssId.length + 1);

        psessionEntry->limSentCapsChangeNtf = true;
        limSendSmeWmStatusChangeNtf(pMac, eSIR_SME_AP_CAPS_CHANGED,
                                    (tANI_U32 *) &apNewCaps,
                                    len, psessionEntry->smeSessionId);
    }
#endif
} /*** limDetectChangeInApCapabilities() ***/




// ---------------------------------------------------------------------
/**
 * limUpdateShortSlot
 *
 * FUNCTION:
 * Enable/Disable short slot
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param enable        Flag to enable/disable short slot
 * @return None
 */

tSirRetStatus limUpdateShortSlot(tpAniSirGlobal pMac, tpSirProbeRespBeacon pBeacon, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry)
{

    tSirSmeApNewCaps   apNewCaps;
    tANI_U32           nShortSlot;
    tANI_U32 val = 0;
    tANI_U32 phyMode;

    // Check Admin mode first. If it is disabled just return
    if (wlan_cfgGetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED, &val)
                   != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP,
               FL("cfg get WNI_CFG_11G_SHORT_SLOT_TIME failed\n"));
        return eSIR_FAILURE;
    }
    if (val == false)
        return eSIR_SUCCESS;

    // Check for 11a mode or 11b mode. In both cases return since slot time is constant and cannot/should not change in beacon
    limGetPhyMode(pMac, &phyMode, psessionEntry);
    if ((phyMode == WNI_CFG_PHY_MODE_11A) || (phyMode == WNI_CFG_PHY_MODE_11B))
        return eSIR_SUCCESS;

    apNewCaps.capabilityInfo = limGetU16((tANI_U8 *) &pBeacon->capabilityInfo);

    //  Earlier implementation: determine the appropriate short slot mode based on AP advertised modes
    // when erp is present, apply short slot always unless, prot=on  && shortSlot=off
    // if no erp present, use short slot based on current ap caps

    // Issue with earlier implementation : Cisco 1231 BG has shortSlot = 0, erpIEPresent and useProtection = 0 (Case4);

    //Resolution : always use the shortSlot setting the capability info to decide slot time. 
    // The difference between the earlier implementation and the new one is only Case4.
    /*
                        ERP IE Present  |   useProtection   |   shortSlot   =   QC STA Short Slot
       Case1        1                                   1                       1                       1           //AP should not advertise this combination. 
       Case2        1                                   1                       0                       0
       Case3        1                                   0                       1                       1
       Case4        1                                   0                       0                       0
       Case5        0                                   1                       1                       1
       Case6        0                                   1                       0                       0
       Case7        0                                   0                       1                       1
       Case8        0                                   0                       0                       0
    */
    nShortSlot = SIR_MAC_GET_SHORT_SLOT_TIME(apNewCaps.capabilityInfo);

    if (nShortSlot != psessionEntry->shortSlotTimeSupported)
    {
        // Short slot time capability of AP has changed. Adopt to it.
        PELOG1(limLog(pMac, LOG1, FL("Shortslot capability of AP changed: %d\n"),  nShortSlot);)
        ((tpSirMacCapabilityInfo)&psessionEntry->limCurrentBssCaps)->shortSlotTime = (tANI_U16)nShortSlot;
        psessionEntry->shortSlotTimeSupported = nShortSlot;
        pBeaconParams->fShortSlotTime = (tANI_U8) nShortSlot;
        pBeaconParams->paramChangeBitmap |= PARAM_SHORT_SLOT_TIME_CHANGED;
    }
    return eSIR_SUCCESS;
}


#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)

/**
 * limUpdateQuietIEInBeacons()
 *
 *FUNCTION:
 * This function is called by specialBeaconProcessing(),
 * when it is time to generate the next beacon.
 * If gLimQuietState is not in the INIT state, then it
 * means that the next beacon may need to include some
 * information about the Quiet BSS IE.
 * This function makes that decision based on the current
 * state of gLimQuietState
 *
 *LOGIC:
 * This routine invokes schSetFixedBeaconFields() only
 * if necessary, as it is expensive to update the fixed
 * beacon fields during each beacon interval.
 *
 *ASSUMPTIONS:
 * This Quiet BSS IE will be sent out as part of
 * Proprietary IE's. If 802.11H is enabled, this IE
 * will be sent out as defined in the 11H spec
 *
 *NOTE:
 *
 * @param  pMac Pointer to Global MAC structure
 * @return true, if the beacon fields need updating
 *         false, if not
 */
tANI_BOOLEAN limUpdateQuietIEInBeacons( tpAniSirGlobal pMac )
{
  tANI_BOOLEAN fUpdateBeaconFields = eANI_BOOLEAN_TRUE;

  limLog( pMac, LOG2, FL("Quiet BSS State = %d\n"),
          pMac->lim.gLimSpecMgmt.quietState );
  switch( pMac->lim.gLimSpecMgmt.quietState )
  {
    case eLIM_QUIET_BEGIN:
      // We need to start broadcasting the Quiet BSS IE
      // Transition to eLIM_QUIET_RUNNING
      pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_RUNNING;
      break;

    case eLIM_QUIET_RUNNING:
      // Start down-counting...
      pMac->lim.gLimSpecMgmt.quietCount--;
      if( pMac->lim.gLimSpecMgmt.quietCount == 0 )
      {
        //
        // We no longer need to broadcast the Quiet BSS IE
        //
        // NOTE - We still need to call schSetFixedBeaconFields()
        // one last time, just to remove the Quiet BSS IE from
        // the list of fixed beacon fields
        //
        // Transition to eLIM_QUIET_END
        pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_END;
        limProcessLearnIntervalTimeout(pMac);
      }
      break;

    case eLIM_QUIET_CHANGED:
      //
      // State possibly changed via setupQuietBss().
      // This means, gLimQuietCount has been changed!!
      //
      // NOTE - We still need to call schSetFixedBeaconFields()
      // one last time, just to remove the Quiet BSS IE from
      // the list of fixed beacon fields
      //

      // Transition to eLIM_QUIET_END
      pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_END;
      break;

    case eLIM_QUIET_INIT:
    case eLIM_QUIET_END:
      // Transition to eLIM_QUIET_INIT
      pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_INIT;
      // Fall thru'...
    default:
      fUpdateBeaconFields = eANI_BOOLEAN_FALSE;
      break;
  }

  return fUpdateBeaconFields;
}

#endif

#if ((defined ANI_PRODUCT_TYPE_AP) && (defined ANI_AP_SDK))
void limConvertScanDuration(tpAniSirGlobal pMac)
{
    tpSirSmeMeasurementReq pMeasReq = pMac->lim.gpLimMeasReq;
    tpLimScanDurationConvert scanDurConv = &pMac->lim.gLimScanDurationConvert;

    /* This is not a good idea to convert {long}shortChannelScanDuration from mS to TICKS *
         * The reason is that {long}shortChannelScanDuration is used all over and a lot of code *
         * that assumes the old mS definition was never changed to accommodate this new change to TICKS. *
         * If optimization is needed, create another set of shadow variables to store the converted *
         * values in Ticks, and TU.  */
    scanDurConv->shortChannelScanDuration_tick =
                     SYS_MS_TO_TICKS(pMeasReq->measDuration.shortChannelScanDuration +SYS_TICK_DUR_MS-1);

    /* convert shortChannelScanDuration to TU also for CB scan, used to set gLimQuietDuration */
    /* (shortChanneScanDuration * 1000) / 2^10 */
    scanDurConv->shortChannelScanDuration_TU = (pMeasReq->measDuration.shortChannelScanDuration * 1000) >> 10;

    scanDurConv->longChannelScanDuration_tick =
                     SYS_MS_TO_TICKS(pMeasReq->measDuration.longChannelScanDuration +SYS_TICK_DUR_MS-1);

    /* convert shortChannelScanDuration to TU also for CB scan, used to set gLimQuietDuration */
    /* (longChanneScanDuration * 1000) / 2^10 */
    scanDurConv->longChannelScanDuration_TU = (pMeasReq->measDuration.longChannelScanDuration * 1000) >> 10;
}
#endif /* ((defined ANI_PRODUCT_TYPE_AP) && (defined ANI_AP_SDK)) */


#ifdef ANI_PRODUCT_TYPE_AP
/**-------------------------------------------------
\fn     limIsRadarEnabled

\brief  Checks if radar is enabled
\param  pMac
\return true - if Both 11h and radar enabled
        false - if either is not enabled.
 --------------------------------------------------*/
tANI_BOOLEAN limIsRadarEnabled(tpAniSirGlobal pMac)
{
    tANI_U32 fEnabled;

    if(wlan_cfgGetInt(pMac, WNI_CFG_11H_ENABLED, &fEnabled) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("HAL: could not retrieve radar config from CFG"));
        return eANI_BOOLEAN_FALSE;
    }

    if (!fEnabled)
        return eANI_BOOLEAN_FALSE;

    if(wlan_cfgGetInt(pMac, WNI_CFG_RDET_FLAG, &fEnabled) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("HAL: could not retrieve radar config from CFG"));
        return eANI_BOOLEAN_FALSE;
    }

    if (fEnabled)
        return eANI_BOOLEAN_TRUE;

    return eANI_BOOLEAN_FALSE;
}

/**---------------------------------------
\fn     limRadarInit
\brief  Initialize Radar Interrupt.

\param  pMac
\return None
 ----------------------------------------*/
void limRadarInit(tpAniSirGlobal pMac)
{
    tANI_U32    status;
    tSirMsgQ    msg;

   PELOG3(limLog(pMac, LOG3, FL("Radar Interrupt Already configured? %s\n"),
                 pMac->lim.gLimSpecMgmt.fRadarIntrConfigured?"Yes":"No");)
    /** To avoid configuring the radar multiple times */
    if (pMac->lim.gLimSpecMgmt.fRadarIntrConfigured)
        return;

    if (!limIsRadarEnabled(pMac))
        return;
    // Prepare and post message to HAL Message Queue
    msg.type = WDA_INIT_RADAR_IND;
    msg.bodyptr = NULL;
    msg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, NO_SESSION, msg.type));
    status = wdaPostCtrlMsg(pMac, &msg);
    if (status != eHAL_STATUS_SUCCESS)
    {
        limLog(pMac, LOGP,
               FL("posting to HAL failed, reason=%d\n"), status);
        return;
    }
    pMac->lim.gLimSpecMgmt.fRadarIntrConfigured = eANI_BOOLEAN_TRUE;
} /****** end limRadarInit() ******/

#endif


/** -----------------------------------------------------------------
  \brief limHandleLowRssiInd() - handles low rssi indication
 
  This function process the SIR_HAL_LOW_RSSI_IND message from
  HAL, and sends a eWNI_SME_LOW_RSSI_IND to CSR.

  \param pMac - global mac structure

  \return  

  \sa
  ----------------------------------------------------------------- */
void limHandleLowRssiInd(tpAniSirGlobal pMac)
{
#if 0  //RSSI related indications will now go to TL and not PE
    if ( (pMac->pmm.gPmmState == ePMM_STATE_BMPS_SLEEP) ||
         (pMac->pmm.gPmmState == ePMM_STATE_UAPSD_SLEEP)||
         (pMac->pmm.gPmmState == ePMM_STATE_WOWLAN) )
    {
        PELOG1(limLog(pMac, LOG1, FL("Sending LOW_RSSI_IND to SME \n"));)
        limSendSmeRsp(pMac, eWNI_SME_LOW_RSSI_IND, eSIR_SME_SUCCESS, 0, 0);
    }
    else
    {
        limLog(pMac, LOGE,
            FL("Received SIR_HAL_LOW_RSSI_IND while in incorrect state: %d\n"),
            pMac->pmm.gPmmState);
    }
    return;
#endif
}


/** -----------------------------------------------------------------
  \brief limHandleBmpsStatusInd() - handles BMPS status indication
 
  This function process the SIR_HAL_BMPS_STATUS_IND message from HAL, 
  and invokes limSendExitBmpsInd( ) to send an eWNI_PMC_EXIT_BMPS_IND 
  to SME with reason code 'eSME_EXIT_BMPS_IND_RCVD'. 
  
  HAL sends this message when Firmware fails to enter BMPS mode 'AFTER'
  HAL had already send PE a SIR_HAL_ENTER_BMPS_RSP with status 
  code "success".  Hence, HAL needs to notify PE to get out of BMPS mode. 
  This message can also come from FW anytime after we have entered BMPS. 
  This means we should handle it in WoWL and UAPSD states as well
   
  \param pMac - global mac structure
  \return - none
  \sa
  ----------------------------------------------------------------- */
void limHandleBmpsStatusInd(tpAniSirGlobal pMac)
{
    switch(pMac->pmm.gPmmState)
    {
        case ePMM_STATE_BMPS_SLEEP:
        case ePMM_STATE_UAPSD_WT_SLEEP_RSP:
        case ePMM_STATE_UAPSD_SLEEP:
        case ePMM_STATE_UAPSD_WT_WAKEUP_RSP:
        case ePMM_STATE_WOWLAN:
            PELOG1(limLog(pMac, LOG1, FL("Sending EXIT_BMPS_IND to SME \n"));)
            limSendExitBmpsInd(pMac, eSME_BMPS_STATUS_IND_RCVD);
            break;

        default:
            limLog(pMac, LOGE,
                FL("Received SIR_HAL_BMPS_STATUS_IND while in incorrect state: %d\n"),
                pMac->pmm.gPmmState);
            break;
    }
    return;
}


/** -----------------------------------------------------------------
  \brief limHandleMissedBeaconInd() - handles missed beacon indication
 
  This function process the SIR_HAL_MISSED_BEACON_IND message from HAL,
  and invokes limSendExitBmpsInd( ) to send an eWNI_PMC_EXIT_BMPS_IND 
  to SME with reason code 'eSME_MISSED_BEACON_IND_RCVD'.
  
  \param pMac - global mac structure
  \return - none 
  \sa
  ----------------------------------------------------------------- */
void limHandleMissedBeaconInd(tpAniSirGlobal pMac)
{
    if ( (pMac->pmm.gPmmState == ePMM_STATE_BMPS_SLEEP) ||
         (pMac->pmm.gPmmState == ePMM_STATE_UAPSD_SLEEP)||
         (pMac->pmm.gPmmState == ePMM_STATE_WOWLAN) )
    {
        pMac->pmm.inMissedBeaconScenario = TRUE;
        PELOG1(limLog(pMac, LOG1, FL("Sending EXIT_BMPS_IND to SME \n"));)
        limSendExitBmpsInd(pMac, eSME_MISSED_BEACON_IND_RCVD);
    }
    else
    {
        limLog(pMac, LOGE,
            FL("Received SIR_HAL_MISSED_BEACON_IND while in incorrect state: %d\n"),
            pMac->pmm.gPmmState);
    }
    return;
}

/** -----------------------------------------------------------------
  \brief limMicFailureInd() - handles mic failure  indication
 
  This function process the SIR_HAL_MIC_FAILURE_IND message from HAL,

  \param pMac - global mac structure
  \return - none 
  \sa
  ----------------------------------------------------------------- */
void limMicFailureInd(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    tpSirSmeMicFailureInd pSirSmeMicFailureInd;
    tpSirSmeMicFailureInd pSirMicFailureInd = (tpSirSmeMicFailureInd)pMsg->bodyptr;
    tSirMsgQ            mmhMsg;
    tpPESession psessionEntry ;
    tANI_U8     sessionId;

    if((psessionEntry = peFindSessionByBssid(pMac,pSirMicFailureInd->bssId,&sessionId))== NULL)
    {
         limLog(pMac, LOGE,
               FL("session does not exist for given BSSId\n"));
         return;
    }

    if (eHAL_STATUS_SUCCESS !=
                    palAllocateMemory(pMac->hHdd,
                                      (void **) &pSirSmeMicFailureInd,
                                      sizeof(tSirSmeMicFailureInd)))
    {
        // Log error
       limLog(pMac, LOGP,
               FL("memory allocate failed for eWNI_SME_MIC_FAILURE_IND\n"));
       return;
    }

    pSirSmeMicFailureInd->messageType = eWNI_SME_MIC_FAILURE_IND;
    pSirSmeMicFailureInd->length = sizeof(pSirSmeMicFailureInd);
    pSirSmeMicFailureInd->sessionId = psessionEntry->smeSessionId;

    vos_mem_copy(pSirSmeMicFailureInd->bssId,
        pSirMicFailureInd->bssId,
        sizeof(tSirMacAddr));

    vos_mem_copy(pSirSmeMicFailureInd->info.srcMacAddr,
        pSirMicFailureInd->info.srcMacAddr,
        sizeof(tSirMacAddr));

    vos_mem_copy(pSirSmeMicFailureInd->info.taMacAddr,
        pSirMicFailureInd->info.taMacAddr,
        sizeof(tSirMacAddr));

    vos_mem_copy(pSirSmeMicFailureInd->info.dstMacAddr,
        pSirMicFailureInd->info.dstMacAddr,
        sizeof(tSirMacAddr));

    vos_mem_copy(pSirSmeMicFailureInd->info.rxMacAddr,
        pSirMicFailureInd->info.rxMacAddr,
        sizeof(tSirMacAddr));

    pSirSmeMicFailureInd->info.multicast = 
                                   pSirMicFailureInd->info.multicast;

    pSirSmeMicFailureInd->info.keyId= 
                                  pSirMicFailureInd->info.keyId;

    pSirSmeMicFailureInd->info.IV1= 
                                  pSirMicFailureInd->info.IV1;

    vos_mem_copy(pSirSmeMicFailureInd->info.TSC,
         pSirMicFailureInd->info.TSC,SIR_CIPHER_SEQ_CTR_SIZE);

    mmhMsg.type = eWNI_SME_MIC_FAILURE_IND;
    mmhMsg.bodyptr = pSirSmeMicFailureInd;
    mmhMsg.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, 0, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return;
}


/** -----------------------------------------------------------------
  \brief limIsPktCandidateForDrop() - decides whether to drop the frame or not

  This function is called before enqueuing the frame to PE queue for further processing.
  This prevents unnecessary frames getting into PE Queue and drops them right away.
  Frames will be droped in the following scenarios:
  
   - In Scan State, drop the frames which are not marked as scan frames
   - In non-Scan state, drop the frames which are marked as scan frames.
   - Drop INFRA Beacons and Probe Responses in IBSS Mode
   - Drop the Probe Request in IBSS mode, if STA did not send out the last beacon
  
  \param pMac - global mac structure
  \return - none 
  \sa
  ----------------------------------------------------------------- */

tMgmtFrmDropReason limIsPktCandidateForDrop(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tANI_U32 subType)
{
    tANI_U32                     framelen;
    tANI_U8                      *pBody;
    tSirMacCapabilityInfo     capabilityInfo;

    /*
    * 
    * In scan mode, drop only Beacon/Probe Response which are NOT marked as scan-frames.
    * In non-scan mode, drop only Beacon/Probe Response which are marked as scan frames. 
    * Allow other mgmt frames, they must be from our own AP, as we don't allow
    * other than beacons or probe responses in scan state.
    */
    if( (subType == SIR_MAC_MGMT_BEACON) ||
        (subType == SIR_MAC_MGMT_PROBE_RSP))
    {
        if(pMac->pmm.inMissedBeaconScenario)
        {
           PELOGE(limLog(pMac, LOGE, FL("Do not drop beacon and probe response - Missed beacon sceanrio"));)
           return eMGMT_DROP_NO_DROP;
        }
        if (limIsSystemInScanState(pMac))
        {
            return eMGMT_DROP_NO_DROP;
        }
        else if (WDA_IS_RX_IN_SCAN(pRxPacketInfo))
        {
            return eMGMT_DROP_SCAN_MODE_FRAME;
        }
    }

    framelen = WDA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
    pBody    = WDA_GET_RX_MPDU_DATA(pRxPacketInfo);

   /* Note sure if this is sufficient, basically this condition allows all probe responses and 
    *   beacons from an infrastructure network 
    */
        *((tANI_U16*) &capabilityInfo) = sirReadU16(pBody+ LIM_BCN_PR_CAPABILITY_OFFSET);
    if(!capabilityInfo.ibss)
        return eMGMT_DROP_NO_DROP;
#if 0
    //Allow the mgmt frames to be queued if STA not in IBSS mode.
    if (pMac->lim.gLimSystemRole != eLIM_STA_IN_IBSS_ROLE)
        return eMGMT_DROP_NO_DROP;
#endif

    //Drop INFRA Beacons and Probe Responses in IBSS Mode
    if( (subType == SIR_MAC_MGMT_BEACON) ||
        (subType == SIR_MAC_MGMT_PROBE_RSP))
    {
        //drop the frame if length is less than 12
        if(framelen < LIM_MIN_BCN_PR_LENGTH)
            return eMGMT_DROP_INVALID_SIZE;
        
        *((tANI_U16*) &capabilityInfo) = sirReadU16(pBody+ LIM_BCN_PR_CAPABILITY_OFFSET);

        //This can be enhanced to even check the SSID before deciding to enque the frame.
        if(capabilityInfo.ess)
            return eMGMT_DROP_INFRA_BCN_IN_IBSS;
    }
    else if( (subType == SIR_MAC_MGMT_PROBE_REQ) &&
                (!WDA_GET_RX_BEACON_SENT(pRxPacketInfo)))
    {
        //Drop the Probe Request in IBSS mode, if STA did not send out the last beacon
        //In IBSS, the node which sends out the beacon, is supposed to respond to ProbeReq
        return eMGMT_DROP_NOT_LAST_IBSS_BCN;
    }

    return eMGMT_DROP_NO_DROP;
}


