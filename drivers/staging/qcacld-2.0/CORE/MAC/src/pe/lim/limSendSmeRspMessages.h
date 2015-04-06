/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * This file limSendSmeRspMessages.h contains the definitions for
 * sending SME response/notification messages to applications above
 * MAC software.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_SEND_SME_RSP_H
#define __LIM_SEND_SME_RSP_H

#include "sirCommon.h"
#include "sirApi.h"
#include "sirMacProtDef.h"


// Functions for sending responses to Host
void limSendSmeRsp(tpAniSirGlobal, tANI_U16, tSirResultCodes, tANI_U8 , tANI_U16);
void limSendSmeStartBssRsp(tpAniSirGlobal, tANI_U16, tSirResultCodes,tpPESession,tANI_U8,tANI_U16);
void limSendSmeScanRsp(tpAniSirGlobal, tANI_U16, tSirResultCodes,tANI_U8, tANI_U16);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
void limSendSmeLfrScanRsp(tpAniSirGlobal, tANI_U16, tSirResultCodes,tANI_U8, tANI_U16);
#endif
void limPostSmeScanRspMessage(tpAniSirGlobal, tANI_U16, tSirResultCodes,tANI_U8,tANI_U16);

void limSendSmeJoinReassocRsp(tpAniSirGlobal, tANI_U16, tSirResultCodes, tANI_U16,tpPESession,tANI_U8,tANI_U16);
void limSendSmeDisassocNtf(tpAniSirGlobal, tSirMacAddr, tSirResultCodes, tANI_U16, tANI_U16,tANI_U8,tANI_U16,tpPESession);
void limSendSmeDeauthNtf(tpAniSirGlobal, tSirMacAddr, tSirResultCodes, tANI_U16, tANI_U16, tANI_U8, tANI_U16);
void limSendSmeDisassocInd(tpAniSirGlobal, tpDphHashNode,tpPESession);
void limSendSmeDeauthInd(tpAniSirGlobal, tpDphHashNode, tpPESession psessionEntry);
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
void limSendSmeCandidateFoundInd(tpAniSirGlobal, tANI_U8);
#endif

void limSendSmeWmStatusChangeNtf(tpAniSirGlobal, tSirSmeStatusChangeCode, tANI_U32 *, tANI_U16, tANI_U8);
void limSendSmeSetContextRsp(tpAniSirGlobal,
                             tSirMacAddr, tANI_U16, tSirResultCodes,tpPESession,tANI_U8,tANI_U16);
void limSendSmeNeighborBssInd(tpAniSirGlobal,
                              tLimScanResultNode *);
void limHandleDeleteBssRsp(tpAniSirGlobal pMac,tpSirMsgQ MsgQ);
void limHandleCSAoffloadMsg(tpAniSirGlobal pMac,tpSirMsgQ MsgQ);

#ifdef WLAN_FEATURE_VOWIFI_11R
void
limSendSmeAggrQosRsp(tpAniSirGlobal pMac, tpSirAggrQosRsp aggrQosRsp,
                     tANI_U8 smesessionId);
#endif /*WLAN_FEATURE_VOWIFI_11R*/


void limSendSmeAddtsRsp(tpAniSirGlobal pMac, tANI_U8 rspReqd, tANI_U32 status, tpPESession psessionEntry, tSirMacTspecIE tspec, tANI_U8 smesessionId, tANI_U16 smetransactionId);
void limSendSmeAddtsInd(tpAniSirGlobal pMac, tpSirAddtsReqInfo addts);
void limSendSmeDeltsRsp(tpAniSirGlobal pMac, tpSirDeltsReq delts, tANI_U32 status,tpPESession psessionEntry,tANI_U8 smessionId,tANI_U16 smetransactionId);
void limSendSmeDeltsInd(tpAniSirGlobal pMac, tpSirDeltsReqInfo delts, tANI_U16 aid,tpPESession);
void limSendSmeStatsRsp(tpAniSirGlobal pMac, tANI_U16 msgtype, void * stats);

void limSendSmePEStatisticsRsp(tpAniSirGlobal pMac, tANI_U16 msgtype, void * stats);
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
void limSendSmePEGetRoamRssiRsp(tpAniSirGlobal pMac, tANI_U16 msgtype, void * stats);
#endif
#ifdef FEATURE_WLAN_ESE_UPLOAD
void limSendSmePEEseTsmRsp(tpAniSirGlobal pMac, tAniGetTsmStatsRsp *pStats);
#endif
void limSendSmeRemoveKeyRsp(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tSirResultCodes resultCode,tpPESession,tANI_U8,tANI_U16);


void limSendSmeGetTxPowerRsp(tpAniSirGlobal pMac, tANI_U32 power, tANI_U32 status);
void limSendSmeGetNoiseRsp(tpAniSirGlobal pMac, tSirMacNoise noise);
void limSendSmeIBSSPeerInd(tpAniSirGlobal pMac,tSirMacAddr peerMacAddr,tANI_U16 staIndex,tANI_U8 ucastIdx,tANI_U8 bcastIdx,
               tANI_U8 *beacon,tANI_U16 beaconLen, tANI_U16 msgType, tANI_U8 sessionId);
void limSendExitBmpsInd(tpAniSirGlobal pMac, tExitBmpsReason reasonCode,
                        tpPESession psessionEntry);

#ifdef FEATURE_OEM_DATA_SUPPORT
void limSendSmeOemDataRsp(tpAniSirGlobal pMac, tANI_U32* pMsgBuf, tSirResultCodes resultCode);
#endif
void limSendSmePreChannelSwitchInd(tpAniSirGlobal pMac,
                            tpPESession psessionEntry);

void limSendSmePostChannelSwitchInd(tpAniSirGlobal pMac);
void limSendSmeMaxAssocExceededNtf(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tANI_U8 smesessionId);
#ifdef FEATURE_WLAN_TDLS
void limSendSmeTdlsDisRsp(tpAniSirGlobal pMac, tSirResultCodes statusCode, tANI_U16 msgType);
void limSendSmeTdlsLinkStartRsp(tpAniSirGlobal pMac, tSirResultCodes statusCode, tSirMacAddr peerMac, tANI_U16 msgType);
void limSendSmeTdlsTeardownRsp(tpAniSirGlobal pMac, tSirResultCodes statusCode, tSirMacAddr peerMac, tANI_U16 msgType);
void limSendSmeTdlsLinkEstablishReqRsp(tpAniSirGlobal pMac,
                                       tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                                       tANI_U8 status);
void limSendSmeTdlsEventNotify(tpAniSirGlobal pMac, tANI_U16 msgType,
                               void *events);
#endif

void limSendSmeDfsEventNotify(tpAniSirGlobal pMac, tANI_U16 msgType,
                                       void *event);
void limSendSmeAPChannelSwitchResp(tpAniSirGlobal pMac,
                                   tpPESession psessionEntry,
                                   tpSwitchChannelParams pChnlParams);
void
limProcessBeaconTxSuccessInd(tpAniSirGlobal pMac, tANI_U16 msgType,
                                       void *event);

typedef enum {
    lim_csa_ie_present = 0x00000001,
    lim_xcsa_ie_present = 0x00000002,
    lim_wbw_ie_present = 0x00000004,
    lim_cswarp_ie_present = 0x00000008,
}lim_csa_event_ies_present_flag;

#endif /* __LIM_SEND_SME_RSP_H */
