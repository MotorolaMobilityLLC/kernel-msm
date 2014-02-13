/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limScanResultUtils.h contains the utility definitions
 * LIM uses for maintaining and accessing scan results on STA.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SCAN_UTILS_H
#define __LIM_SCAN_UTILS_H

#include "parserApi.h"
#include "limTypes.h"

// Scan result hash related functions
tANI_U8 limScanHashFunction(tSirMacAddr);
void    limInitHashTable(tpAniSirGlobal);
eHalStatus    
   limLookupNaddHashEntry(tpAniSirGlobal, tLimScanResultNode *, tANI_U8, tANI_U8);
void    limDeleteHashEntry(tLimScanResultNode *);
void    limDeleteCachedScanResults(tpAniSirGlobal);
void    limRestorePreScanState(tpAniSirGlobal);
void    limCopyScanResult(tpAniSirGlobal, tANI_U8 *);
void    limReInitScanResults(tpAniSirGlobal);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
void    limInitLfrHashTable(tpAniSirGlobal);
eHalStatus
   limLookupNaddLfrHashEntry(tpAniSirGlobal, tLimScanResultNode *, tANI_U8, tANI_U8);
void    limDeleteLfrHashEntry(tLimScanResultNode *);
void    limDeleteCachedLfrScanResults(tpAniSirGlobal);
void    limReInitLfrScanResults(tpAniSirGlobal);
#endif
tANI_U32 limDeactivateMinChannelTimerDuringScan(tpAniSirGlobal);
void    limCheckAndAddBssDescription(tpAniSirGlobal, tpSirProbeRespBeacon, tANI_U8 *, tANI_BOOLEAN, tANI_U8);
#if defined WLAN_FEATURE_VOWIFI
eHalStatus    limCollectBssDescription(tpAniSirGlobal,
                                 tSirBssDescription *,
                                 tpSirProbeRespBeacon,
                                 tANI_U8 *,
                                 tANI_U8);
#else
eHalStatus    limCollectBssDescription(tpAniSirGlobal,
                                 tSirBssDescription *,
                                 tpSirProbeRespBeacon,
                                 tANI_U8 *);
#endif

#endif /* __LIM_SCAN_UTILS_H */
