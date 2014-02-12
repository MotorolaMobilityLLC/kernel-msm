/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limIbssPeerMgmt.h contains prototypes for
 * the utility functions LIM uses to maintain peers in IBSS.
 * Author:        Chandra Modumudi
 * Date:          03/12/04
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "sirCommon.h"
#include "limUtils.h"

#define IBSS_STATIONS_USED_DURING_INIT 4  //(broadcast + self + p2p + softap)

void limIbssInit(tpAniSirGlobal);
void limIbssDelete(tpAniSirGlobal,tpPESession psessionEntry);
tSirRetStatus limIbssCoalesce(tpAniSirGlobal, tpSirMacMgmtHdr, tpSchBeaconStruct, tANI_U8*,tANI_U32, tANI_U16,tpPESession);
tSirRetStatus limIbssStaAdd(tpAniSirGlobal, void *,tpPESession);
tSirRetStatus limIbssAddStaRsp( tpAniSirGlobal, void *,tpPESession);
void limIbssDelBssRsp( tpAniSirGlobal, void *,tpPESession);
void limIbssDelBssRspWhenCoalescing(tpAniSirGlobal,  void *,tpPESession);
void limIbssAddBssRspWhenCoalescing(tpAniSirGlobal  pMac, void * msg, tpPESession pSessionEntry);
void limIbssDecideProtectionOnDelete(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams,tpPESession pSessionEntry);
void limIbssHeartBeatHandle(tpAniSirGlobal pMac,tpPESession psessionEntry);
void limProcessIbssPeerInactivity(tpAniSirGlobal pMac, void *buf);
