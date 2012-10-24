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

  
    \file csrNeighborRoam.h
  
    Exports and types for the neighbor roaming algorithm which is sepcifically 
    designed for Android.
  
   Copyright (C) 2006 Airgo Networks, Incorporated
   
========================================================================== */
#ifndef CSR_NEIGHBOR_ROAM_H
#define CSR_NEIGHBOR_ROAM_H

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING

/* Enumeration of various states in neighbor roam algorithm */
typedef enum
{
    eCSR_NEIGHBOR_ROAM_STATE_CLOSED,
    eCSR_NEIGHBOR_ROAM_STATE_INIT,
    eCSR_NEIGHBOR_ROAM_STATE_CONNECTED,
    eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN,
    eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING,
#ifdef WLAN_FEATURE_VOWIFI_11R
    eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY,
    eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN,
    eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING,
    eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE,
#endif /* WLAN_FEATURE_VOWIFI_11R */    
    eNEIGHBOR_STATE_MAX
} eCsrNeighborRoamState;

/* Parameters that are obtained from CFG */
typedef struct sCsrNeighborRoamCfgParams
{
    tANI_U8         maxNeighborRetries;
    tANI_U32        neighborScanPeriod;
    tCsrChannelInfo channelInfo;
    tANI_U8         neighborLookupThreshold;
    tANI_U8         neighborReassocThreshold;
    tANI_U32        minChannelScanTime;
    tANI_U32        maxChannelScanTime;
    tANI_U16        neighborResultsRefreshPeriod;
} tCsrNeighborRoamCfgParams, *tpCsrNeighborRoamCfgParams;

#define CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX    255
typedef struct sCsrNeighborRoamChannelInfo
{
    tANI_BOOLEAN    IAPPNeighborListReceived; // Flag to mark reception of IAPP Neighbor list
    tANI_BOOLEAN    chanListScanInProgress;
    tANI_U8         currentChanIndex;       //Current channel index that is being scanned
    tCsrChannelInfo currentChannelListInfo; //Max number of channels in channel list and the list of channels
} tCsrNeighborRoamChannelInfo, *tpCsrNeighborRoamChannelInfo;

typedef struct sCsrNeighborRoamBSSInfo
{
    tListElem           List;
    tANI_U8             apPreferenceVal;
//    tCsrScanResultInfo  *scanResultInfo;
    tpSirBssDescription pBssDescription;
} tCsrNeighborRoamBSSInfo, *tpCsrNeighborRoamBSSInfo;

#ifdef WLAN_FEATURE_VOWIFI_11R
#define CSR_NEIGHBOR_ROAM_REPORT_QUERY_TIMEOUT  1000    //in milliseconds
#define CSR_NEIGHBOR_ROAM_PREAUTH_RSP_WAIT_MULTIPLIER   10     //in milliseconds
#define MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS       10 //Max number of MAC addresses with which the pre-auth was failed
#define MAX_BSS_IN_NEIGHBOR_RPT                 15
#define CSR_NEIGHBOR_ROAM_MAX_NUM_PREAUTH_RETRIES 3

/* Black listed APs. List of MAC Addresses with which the Preauthentication was failed. */
typedef struct sCsrPreauthFailListInfo
{
    tANI_U8     numMACAddress;
    tSirMacAddr macAddress[MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS];
} tCsrPreauthFailListInfo, *tpCsrPreauthFailListInfo;

typedef struct sCsrNeighborReportBssInfo
{
    tANI_U8 channelNum;
    tANI_U8 neighborScore;
    tSirMacAddr neighborBssId;
} tCsrNeighborReportBssInfo, *tpCsrNeighborReportBssInfo;

typedef struct sCsr11rAssocNeighborInfo
{
    tANI_BOOLEAN                preauthRspPending;
    tANI_BOOLEAN                neighborRptPending;
    tANI_U8                     currentNeighborRptRetryNum;
    tPalTimerHandle             preAuthRspWaitTimer; //This timer is used for preauth response
    tCsrTimerInfo               preAuthRspWaitTimerInfo;
    tCsrPreauthFailListInfo     preAuthFailList;
    tANI_U32                    neighborReportTimeout;
    tANI_U32                    PEPreauthRespTimeout;
    tANI_U8                     numPreAuthRetries;
    tDblLinkList                preAuthDoneList;    /* Linked list which consists or preauthenticated nodes */
    tANI_U8                     numBssFromNeighborReport;
    tCsrNeighborReportBssInfo   neighboReportBssInfo[MAX_BSS_IN_NEIGHBOR_RPT];  //Contains info needed during REPORT_SCAN State
} tCsr11rAssocNeighborInfo, *tpCsr11rAssocNeighborInfo;
#endif /* WLAN_FEATURE_VOWIFI_11R */

/* Below macros are used to increase the registered neighbor Lookup threshold with TL when 
 * we dont see any AP during back ground scanning. The values are incremented from neighborLookupThreshold 
 * from CFG, incremented by 5,10,15...50(LOOKUP_THRESHOLD_INCREMENT_MULTIPLIER_MAX * 
 * NEIGHBOR_LOOKUP_THRESHOLD_INCREMENT_CONSTANT) */
#define NEIGHBOR_LOOKUP_THRESHOLD_INCREMENT_CONSTANT    5
#define LOOKUP_THRESHOLD_INCREMENT_MULTIPLIER_MAX       4
/* 
 * For every scan that results in no candidates, double the scan periodicity 
 * (initialized to NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN) until we hit 
 * NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX (60s). Subsequently, scan every 
 * 60s if we continue to find no candidates. Once a candidate is found, 
 * the periodicity is reset back to NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN.
 */
#define NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN (1000)
#define NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX (60000)

/* Complete control information for neighbor roam algorithm */
typedef struct sCsrNeighborRoamControlInfo
{
    eCsrNeighborRoamState       neighborRoamState;
    eCsrNeighborRoamState       prevNeighborRoamState;
    tCsrNeighborRoamCfgParams   cfgParams;
    tCsrBssid                   currAPbssid; // current assoc AP
    tANI_U8                     currAPoperationChannel; // current assoc AP
    tPalTimerHandle             neighborScanTimer;
    tPalTimerHandle             neighborResultsRefreshTimer;
    tCsrTimerInfo               neighborScanTimerInfo;
    tCsrNeighborRoamChannelInfo roamChannelInfo;
    tANI_U8                     currentNeighborLookupThreshold;
    tANI_BOOLEAN                scanRspPending;
    tANI_TIMESTAMP              scanRequestTimeStamp;
    tDblLinkList                roamableAPList;    // List of current FT candidates
    tANI_U32                    csrSessionId;
    tCsrRoamProfile             csrNeighborRoamProfile;
#ifdef WLAN_FEATURE_VOWIFI_11R    
    tANI_BOOLEAN                is11rAssoc;
    tCsr11rAssocNeighborInfo    FTRoamInfo;
#endif /* WLAN_FEATURE_VOWIFI_11R */
#ifdef FEATURE_WLAN_CCX    
    tANI_BOOLEAN                isCCXAssoc;
    tANI_BOOLEAN                isVOAdmitted;
    tANI_U32                    MinQBssLoadRequired;
#endif
    tANI_U16                    currentScanResultsRefreshPeriod;
} tCsrNeighborRoamControlInfo, *tpCsrNeighborRoamControlInfo;


/* All the necessary Function declarations are here */
eHalStatus csrNeighborRoamIndicateConnect(tpAniSirGlobal pMac,tANI_U8 sessionId, VOS_STATUS status);
eHalStatus csrNeighborRoamIndicateDisconnect(tpAniSirGlobal pMac,tANI_U8 sessionId);
tANI_BOOLEAN csrNeighborRoamIsHandoffInProgress(tpAniSirGlobal pMac);
void csrNeighborRoamRequestHandoff(tpAniSirGlobal pMac);
eHalStatus csrNeighborRoamInit(tpAniSirGlobal pMac);
void csrNeighborRoamClose(tpAniSirGlobal pMac);
void csrNeighborRoamPurgePreauthFailedList(tpAniSirGlobal pMac);
VOS_STATUS csrNeighborRoamTransitToCFGChanScan(tpAniSirGlobal pMac);
VOS_STATUS csrNeighborRoamTransitionToPreauthDone(tpAniSirGlobal pMac);
eHalStatus csrNeighborRoamPrepareScanProfileFilter(tpAniSirGlobal pMac, tCsrScanResultFilter *pScanFilter);
void csrNeighborRoamGetHandoffAPInfo(tpAniSirGlobal pMac, tpCsrNeighborRoamBSSInfo pHandoffNode);
eHalStatus csrNeighborRoamPreauthRspHandler(tpAniSirGlobal pMac, VOS_STATUS vosStatus);
#ifdef WLAN_FEATURE_VOWIFI_11R
tANI_BOOLEAN csrNeighborRoamIs11rAssoc(tpAniSirGlobal pMac);
#endif
VOS_STATUS csrNeighborRoamCreateChanListFromNeighborReport(tpAniSirGlobal pMac);
void csrNeighborRoamTranistionPreauthDoneToDisconnected(tpAniSirGlobal pMac);
tANI_BOOLEAN csrNeighborRoamStatePreauthDone(tpAniSirGlobal pMac);



#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */

#endif /* CSR_NEIGHBOR_ROAM_H */
