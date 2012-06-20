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
 * This file limTimerUtils.h contains the utility definitions
 * LIM uses for timer handling.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_TIMER_UTILS_H
#define __LIM_TIMER_UTILS_H

#include "limTypes.h"


// Timer related functions
enum
{
    eLIM_MIN_CHANNEL_TIMER,
    eLIM_MAX_CHANNEL_TIMER,
    eLIM_JOIN_FAIL_TIMER,
    eLIM_AUTH_FAIL_TIMER,
    eLIM_AUTH_RESP_TIMER,
    eLIM_ASSOC_FAIL_TIMER,
    eLIM_REASSOC_FAIL_TIMER,
    eLIM_PRE_AUTH_CLEANUP_TIMER,
    eLIM_HEART_BEAT_TIMER,    
    eLIM_BACKGROUND_SCAN_TIMER,
#ifdef ANI_PRODUCT_TYPE_AP
    eLIM_LEARN_INTERVAL_TIMER,
#endif
    eLIM_KEEPALIVE_TIMER,
    eLIM_CNF_WAIT_TIMER,
    eLIM_AUTH_RSP_TIMER,
    eLIM_UPDATE_OLBC_CACHE_TIMER,
    eLIM_PROBE_AFTER_HB_TIMER,
    eLIM_ADDTS_RSP_TIMER,
    eLIM_CHANNEL_SWITCH_TIMER,
    eLIM_LEARN_DURATION_TIMER,
    eLIM_QUIET_TIMER,
    eLIM_QUIET_BSS_TIMER,
#ifdef WLAN_SOFTAP_FEATURE
    eLIM_WPS_OVERLAP_TIMER, 
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
    eLIM_FT_PREAUTH_RSP_TIMER,
#endif
#ifdef WLAN_FEATURE_P2P
    eLIM_REMAIN_CHN_TIMER,
#endif
    eLIM_PERIODIC_PROBE_REQ_TIMER,
#ifdef FEATURE_WLAN_CCX
    eLIM_TSM_TIMER,
#endif
};

// Timer Handler functions
void limCreateTimers(tpAniSirGlobal);
void limTimerHandler(void *, tANI_U32);
void limAuthResponseTimerHandler(void *, tANI_U32);
void limAssocFailureTimerHandler(void *, tANI_U32);
void limReassocFailureTimerHandler(void *, tANI_U32);

void limDeactivateAndChangeTimer(tpAniSirGlobal, tANI_U32);
void limHeartBeatDeactivateAndChangeTimer(tpAniSirGlobal, tpPESession);
void limReactivateHeartBeatTimer(tpAniSirGlobal, tpPESession);
void limDummyPktExpTimerHandler(void *, tANI_U32);
void limSendDisassocFrameThresholdHandler(void *, tANI_U32);
void limCnfWaitTmerHandler(void *, tANI_U32);
void limKeepaliveTmerHandler(void *, tANI_U32);
void limDeactivateAndChangePerStaIdTimer(tpAniSirGlobal, tANI_U32, tANI_U16);
void limActivateCnfTimer(tpAniSirGlobal, tANI_U16, tpPESession);
void limActivateAuthRspTimer(tpAniSirGlobal, tLimPreAuthNode *);
#ifdef WLAN_SOFTAP_FEATURE
void limUpdateOlbcCacheTimerHandler(void *, tANI_U32);
#endif
void limAddtsResponseTimerHandler(void *, tANI_U32);
void limChannelSwitchTimerHandler(void *, tANI_U32);
void limQuietTimerHandler(void *, tANI_U32);
void limQuietBssTimerHandler(void *, tANI_U32);
void limCBScanIntervalTimerHandler(void *, tANI_U32);
void limCBScanDurationTimerHandler(void *, tANI_U32);
/**
 * limActivateHearBeatTimer()
 *
 *
 * @brief: This function is called to activate heartbeat timer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 * @note   staId for eLIM_AUTH_RSP_TIMER is auth Node Index.
 *
 * @param  pMac    - Pointer to Global MAC structure
 *
 * @return TX_SUCCESS - timer is activated
 *         errors - fail to start the timer
 */
v_UINT_t limActivateHearBeatTimer(tpAniSirGlobal pMac);

#ifdef WLAN_SOFTAP_FEATURE
#if 0
void limWPSOverlapTimerHandler(void *pMacGlobal, tANI_U32 param);
#endif
#endif
#endif /* __LIM_TIMER_UTILS_H */
