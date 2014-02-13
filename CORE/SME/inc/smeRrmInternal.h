/*
 * Copyright (c) 2012-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * */
#if !defined( __SMERRMINTERNAL_H )
#define __SMERRMINTERNAL_H


/**=========================================================================
  
  \file  smeRrmInternal.h
  
  \brief prototype for SME RRM APIs

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_lock.h"
#include "vos_trace.h"
#include "vos_memory.h"
#include "vos_types.h"
#include "palTimer.h"
#include "rrmGlobal.h"

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct sRrmConfigParam
{
   tANI_U8 rrmEnabled;
   tANI_U8 maxRandnInterval;
}tRrmConfigParam, *tpRrmConfigParam;

typedef struct sRrmNeighborReportDesc
{
   tListElem    List;
   tSirNeighborBssDescription   *pNeighborBssDescription;
   tANI_U32                     roamScore;
} tRrmNeighborReportDesc, *tpRrmNeighborReportDesc;


typedef void (*NeighborReportRspCallback) (void *context, VOS_STATUS vosStatus);

typedef struct sRrmNeighborRspCallbackInfo
{
    tANI_U32                  timeout;  //in ms.. min value is 10 (10ms)
    NeighborReportRspCallback neighborRspCallback;
    void                      *neighborRspCallbackContext;
} tRrmNeighborRspCallbackInfo, *tpRrmNeighborRspCallbackInfo;

typedef struct sRrmNeighborRequestControlInfo
{
    tANI_BOOLEAN    isNeighborRspPending;   //To check whether a neighbor req is already sent and response pending
    vos_timer_t     neighborRspWaitTimer;
    tRrmNeighborRspCallbackInfo neighborRspCallbackInfo;
} tRrmNeighborRequestControlInfo, *tpRrmNeighborRequestControlInfo;

typedef struct sRrmSMEContext
{
   tANI_U16 token;
   tCsrBssid sessionBssId;
   tANI_U8 regClass;
   tCsrChannelInfo channelList; //list of all channels to be measured.
   tANI_U8 currentIndex;
   tAniSSID ssId;  //SSID used in the measuring beacon report.
   tSirMacAddr bssId; //bssid used for beacon report measurement.
   tANI_U16 randnIntvl; //Randomization interval to be used in subsequent measurements.
   tANI_U16 duration[SIR_CCX_MAX_MEAS_IE_REQS];
   tANI_U8 measMode[SIR_CCX_MAX_MEAS_IE_REQS];
   tRrmConfigParam rrmConfig;
   vos_timer_t IterMeasTimer;
   tDblLinkList neighborReportCache;
   tRrmNeighborRequestControlInfo neighborReqControlInfo;

#if defined(FEATURE_WLAN_CCX) && defined(FEATURE_WLAN_CCX_UPLOAD)
   tCsrCcxBeaconReq  ccxBcnReqInfo;
#endif /* FEATURE_WLAN_CCX && FEATURE_WLAN_CCX_UPLOAD */
   tRrmMsgReqSource msgSource;
}tRrmSMEContext, *tpRrmSMEContext; 

typedef struct sRrmNeighborReq
{
   tANI_U8 no_ssid;
   tSirMacSSid ssid;
}tRrmNeighborReq, *tpRrmNeighborReq;

#endif //#if !defined( __SMERRMINTERNAL_H )
