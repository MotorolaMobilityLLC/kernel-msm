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

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limPropExtsUtils.h contains the definitions
 * used by all LIM modules to support proprietary features.
 * Author:        Chandra Modumudi
 * Date:          12/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 
 */

#ifndef __LIM_PROP_EXTS_UTILS_H
#define __LIM_PROP_EXTS_UTILS_H


// Function templates
void limQuietBss(tpAniSirGlobal, tANI_U32);
void limCleanupMeasData(tpAniSirGlobal);
void limDeleteMeasTimers(tpAniSirGlobal);
void limStopMeasTimers(tpAniSirGlobal pMac);
void limCleanupMeasResources(tpAniSirGlobal);
void limRestorePreLearnState(tpAniSirGlobal);
void limCollectMeasurementData(tpAniSirGlobal,
                               tANI_U32 *, tpSchBeaconStruct);
void limCollectRSSI(tpAniSirGlobal);
void limDeleteCurrentBssWdsNode(tpAniSirGlobal);
tANI_U32  limComputeAvg(tpAniSirGlobal, tANI_U32, tANI_U32);

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
// Timer Handlers
void limMeasurementTimerHandler(VOID*, tANI_U32);
#endif

/// Function to extract AP's HCF capability from IE fields
void limExtractApCapability(tpAniSirGlobal, tANI_U8 *, tANI_U16, tANI_U8 *, tANI_U16 *, tANI_U8 *, tPowerdBm*);

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/// Function to extract current Learn channel
tANI_U8 limGetCurrentLearnChannel(tpAniSirGlobal);
// Determine if a newly discovered BSS is TITAN-compatible
void handleNonTitanBss( tpAniSirGlobal, tSirNeighborBssWdsInfo );
#endif
ePhyChanBondState limGetPhyCBState( tpAniSirGlobal );
tStaRateMode limGetStaPeerType( tpAniSirGlobal, tpDphHashNode ,tpPESession);
void setupCBState( tpAniSirGlobal, tAniCBSecondaryMode );

tANI_U8 limGetCurrentCBSecChannel( tpAniSirGlobal,tpPESession );

#endif /* __LIM_PROP_EXTS_UTILS_H */

