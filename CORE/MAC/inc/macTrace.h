/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**=========================================================================

  \file  macTrace.h

  \brief definition for trace related APIs

  \author Sunit Bhatia

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/



#ifndef __MAC_TRACE_H
#define __MAC_TRACE_H

#include "aniGlobal.h"


#ifdef TRACE_RECORD

#define CASE_RETURN_STRING( str )           \
    case ( ( str ) ): return( (tANI_U8*)(#str) ); break \

#define MAC_TRACE_GET_MODULE_ID(data) ((data >> 8) & 0xff)
#define MAC_TRACE_GET_MSG_ID(data)       (data & 0xffff)


#define eLOG_NODROP_MISSED_BEACON_SCENARIO 0
#define eLOG_PROC_DEAUTH_FRAME_SCENARIO 1

void macTraceReset(tpAniSirGlobal pMac);
void macTrace(tpAniSirGlobal pMac,  tANI_U8 code, tANI_U8 session, tANI_U32 data);
void macTraceNew(tpAniSirGlobal pMac,  tANI_U8 module, tANI_U8 code, tANI_U8 session, tANI_U32 data);
tANI_U8* macTraceGetCfgMsgString( tANI_U16 cfgMsg );
tANI_U8* macTraceGetLimMsgString( tANI_U16 limMsg );
tANI_U8* macTraceGetWdaMsgString( tANI_U16 wdaMsg );
tANI_U8* macTraceGetSmeMsgString( tANI_U16 smeMsg );
tANI_U8* macTraceGetModuleString( tANI_U8 moduleId);
tANI_U8* macTraceGetInfoLogString( tANI_U16 infoLog );
eHalStatus pe_AcquireGlobalLock( tAniSirLim *psPe);
eHalStatus pe_ReleaseGlobalLock( tAniSirLim *psPe);

tANI_U8* macTraceGetHDDWlanConnState(tANI_U16 connState);

#ifdef WLAN_FEATURE_P2P_DEBUG
tANI_U8* macTraceGetP2PConnState(tANI_U16 connState);
#endif

tANI_U8* macTraceGetNeighbourRoamState(tANI_U16 neighbourRoamState);
tANI_U8* macTraceGetcsrRoamState(tANI_U16 csrRoamState);
tANI_U8* macTraceGetcsrRoamSubState(tANI_U16 csrRoamSubState);
tANI_U8* macTraceGetLimSmeState(tANI_U16 limState);
tANI_U8* macTraceGetLimMlmState(tANI_U16 mlmState);
tANI_U8* macTraceGetTLState(tANI_U16 tlState);

#endif

#endif

