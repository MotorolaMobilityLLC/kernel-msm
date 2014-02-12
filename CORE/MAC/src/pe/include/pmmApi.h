/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __PMM_API_H__
#define __PMM_API_H__

#include "sirCommon.h"
#include "schApi.h"
#include "halMsgApi.h"


/// Initialize PMM
extern tSirRetStatus pmmInitialize(tpAniSirGlobal pMac);

/// Post a message to PMM module
extern tSirRetStatus pmmPostMessage(tpAniSirGlobal pMac, tpSirMsgQ pMsg);

/// Reset PMM beacon mode power save statistics
extern void pmmResetStats(void *pvMac);

/// Process the next PM message
extern void pmmProcessMessage(tpAniSirGlobal, tpSirMsgQ);

extern void pmmProcessPSPoll(tpAniSirGlobal, tANI_U8 *);
extern void pmmUpdatePSPollState(tpAniSirGlobal);
extern void pmmProcessRxActivity(tpAniSirGlobal, tANI_U16, tANI_U8);

extern void pmmGenerateTIM(tpAniSirGlobal, tANI_U8 **, tANI_U16 *, tANI_U8);


void pmmUpdateTIM(tpAniSirGlobal pMac, tpBeaconGenParams pBeaconGenParams);

/// Update the PM mode
extern void pmmUpdatePMMode(tpAniSirGlobal pMac, tANI_U16 staId, tANI_U8 pmMode);

/// Update Power Mode
extern void pmmUpdatePollablePMMode(tpAniSirGlobal, tANI_U16, tANI_U8, tANI_U16);
extern void pmmUpdateNonPollablePMMode(tpAniSirGlobal, tANI_U16, tANI_U8, tANI_U16);

/** Monitor the STA in PS*/
void pmmHandleTimBasedDisassociation(tpAniSirGlobal pMac, tpPESession psessionEntry);

//go into sleep state
void pmmInitBmpsPwrSave(tpAniSirGlobal pMac);
tSirRetStatus  pmmSendInitPowerSaveMsg(tpAniSirGlobal pMac,tpPESession);
void pmmInitBmpsResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirRetStatus  pmmSendChangePowerSaveMsg(tpAniSirGlobal pMac);
tSirRetStatus pmmSendSleepIndicationToHal(tpAniSirGlobal pMac);

//go into wakeup state
void pmmExitBmpsRequestHandler(tpAniSirGlobal pMac, tpExitBmpsInfo pExitBmpsInfo);
void pmmExitBmpsResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg) ;
void pmmMissedBeaconHandler(tpAniSirGlobal pMac);

//handlling all UAPSD messages
void pmmEnterUapsdRequestHandler (tpAniSirGlobal pMac);
void pmmEnterUapsdResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
void pmmExitUapsdRequestHandler (tpAniSirGlobal pMac);
void pmmExitUapsdResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirRetStatus pmmUapsdSendChangePwrSaveMsg (tpAniSirGlobal pMac, tANI_U8 mode);

// handling of all idle mode power save messages
void pmmEnterImpsRequestHandler(tpAniSirGlobal pMac);
void pmmEnterImpsResponseHandler(tpAniSirGlobal pMac, eHalStatus rspStatus);
void pmmExitImpsRequestHandler(tpAniSirGlobal pMac);
void pmmExitImpsResponseHandler(tpAniSirGlobal pMac, eHalStatus rspStatus);

// handling WOWLAN messages
void pmmSendWowlAddBcastPtrn(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
void pmmSendWowlDelBcastPtrn(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
void pmmEnterWowlRequestHandler(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
tSirRetStatus pmmSendWowlEnterRequest(tpAniSirGlobal pMac, tpSirHalWowlEnterParams pHalWowlParams);
void pmmEnterWowlanResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirRetStatus  pmmSendExitWowlReq(tpAniSirGlobal pMac, tpSirHalWowlExitParams pHalWowlParams);
void pmmExitWowlanRequestHandler(tpAniSirGlobal pMac);
void pmmExitWowlanResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);

// update idle mode statistics
void pmmImpsUpdatePwrSaveStats(tpAniSirGlobal pMac);
void pmmImpsUpdateWakeupStats(tpAniSirGlobal pMac);
tSirRetStatus pmmImpsSendChangePwrSaveMsg(tpAniSirGlobal pMac, tANI_U8 mode);
void pmmImpsUpdateSleepErrStats(tpAniSirGlobal pMac, tSirRetStatus retStatus);
void pmmImpsUpdateWakeupErrStats(tpAniSirGlobal pMac, tSirRetStatus retStatus);
void pmmImpsUpdateErrStateStats(tpAniSirGlobal pMac);
void pmmImpsUpdatePktDropStats(tpAniSirGlobal pMac);

void pmmUpdatePwrSaveStats(tpAniSirGlobal pMac);
void pmmUpdateWakeupStats(tpAniSirGlobal pMac);
void pmmBmpsUpdatePktDropStats(tpAniSirGlobal pMac);
void pmmBmpsUpdateHalReqFailureCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdateInitFailureCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdateInvalidStateCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdatePktDropStats(tpAniSirGlobal pMac);
void pmmBmpsUpdateReqInInvalidRoleCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdateSleepReqFailureCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdateWakeupIndCnt(tpAniSirGlobal pMac);
void pmmBmpsUpdateWakeupReqFailureCnt(tpAniSirGlobal pMac);
void pmmResetPmmState(tpAniSirGlobal pMac);
void pmmSendMessageToLim(tpAniSirGlobal pMac, tANI_U32 msgId);

//Power Save CFG
tSirRetStatus  pmmSendPowerSaveCfg(tpAniSirGlobal pMac, tpSirPowerSaveCfg pUpdatedPwrSaveCfg);

//Handle Low RSSI Indication
void pmmLowRssiHandler(tpAniSirGlobal pMac);

#ifdef WLAN_FEATURE_PACKET_FILTERING
void pmmFilterMatchCountResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
#endif // WLAN_FEATURE_PACKET_FILTERING

#ifdef WLAN_FEATURE_GTK_OFFLOAD
void pmmGTKOffloadGetInfoResponseHandler(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
#endif // WLAN_FEATURE_GTK_OFFLOAD

#endif
