/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __SMERRMAPI_H )
#define __SMERRMAPI_H


/**=========================================================================
  
  \file  sme_RrmApi.h
  
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
#include "aniGlobal.h"
#include "sirApi.h"
#include "smeInternal.h"
#include "smeRrmInternal.h"


//APIs
eHalStatus sme_RrmMsgProcessor( tpAniSirGlobal pMac,  v_U16_t msg_type, 
                                void *pMsgBuf);

VOS_STATUS rrmClose (tpAniSirGlobal pMac);
VOS_STATUS rrmReady (tpAniSirGlobal pMac);
VOS_STATUS rrmOpen (tpAniSirGlobal pMac);
VOS_STATUS rrmChangeDefaultConfigParam(tpAniSirGlobal pMac, tpRrmConfigParam pRrmConfig);
VOS_STATUS sme_RrmNeighborReportRequest(tpAniSirGlobal pMac, tANI_U8 sessionId, tpRrmNeighborReq pNeighborReq, tpRrmNeighborRspCallbackInfo callbackInfo);

tRrmNeighborReportDesc* smeRrmGetFirstBssEntryFromNeighborCache( tpAniSirGlobal pMac);
tRrmNeighborReportDesc* smeRrmGetNextBssEntryFromNeighborCache( tpAniSirGlobal pMac, tpRrmNeighborReportDesc pBssEntry);
void sme_RrmProcessBeaconReportReqInd(tpAniSirGlobal pMac, void *pMsgBuf);


#endif
