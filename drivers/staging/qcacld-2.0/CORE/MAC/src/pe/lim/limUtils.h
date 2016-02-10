/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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
 * This file limUtils.h contains the utility definitions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_UTILS_H
#define __LIM_UTILS_H

#include "sirApi.h"
#include "sirDebug.h"
#include "cfgApi.h"

#include "limTypes.h"
#include "limScanResultUtils.h"
#include "limTimerUtils.h"
#include "limTrace.h"
typedef enum
{
    ONE_BYTE   = 1,
    TWO_BYTE   = 2
} eSizeOfLenField;

#define LIM_STA_ID_MASK                        0x00FF
#define LIM_AID_MASK                              0xC000
#define LIM_SPECTRUM_MANAGEMENT_BIT_MASK          0x0100
#define LIM_RRM_BIT_MASK                          0x1000
#define LIM_SHORT_PREAMBLE_BIT_MASK               0x0020
#define LIM_IMMEDIATE_BLOCK_ACK_MASK              0x8000
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
#define LIM_MAX_REASSOC_RETRY_LIMIT            2
#endif

// classifier ID is coded as 0-3: tsid, 4-5:direction
#define LIM_MAKE_CLSID(tsid, dir) (((tsid) & 0x0F) | (((dir) & 0x03) << 4))

#define LIM_SET_STA_BA_STATE(pSta, tid, newVal) \
{\
    pSta->baState = ((pSta->baState | (0x3 << tid*2)) & ((newVal << tid*2) | ~(0x3 << tid*2)));\
}

#define LIM_GET_STA_BA_STATE(pSta, tid, pCurVal)\
{\
    *pCurVal = (tLimBAState)(((pSta->baState >> tid*2) & 0x3));\
}

#define VHT_MCS_3x3_MASK    0x30
#define VHT_MCS_2x2_MASK    0x0C

typedef struct sAddBaInfo
{
    tANI_U16 fBaEnable : 1;
    tANI_U16 startingSeqNum: 12;
    tANI_U16 reserved : 3;
}tAddBaInfo, *tpAddBaInfo;

typedef struct sAddBaCandidate
{
    tSirMacAddr staAddr;
    tAddBaInfo baInfo[STACFG_MAX_TC];
}tAddBaCandidate, *tpAddBaCandidate;

#ifdef WLAN_FEATURE_11W
typedef union uPmfSaQueryTimerId
{
    struct
    {
        tANI_U8 sessionId;
        tANI_U16 peerIdx;
    } fields;
    tANI_U32 value;
} tPmfSaQueryTimerId, *tpPmfSaQueryTimerId;
#endif

// LIM utility functions
void limGetBssidFromPkt(tpAniSirGlobal, tANI_U8 *, tANI_U8 *, tANI_U32 *);
char * limDot11ReasonStr(tANI_U16 reasonCode);
char * limMlmStateStr(tLimMlmStates state);
char * limResultCodeStr(tSirResultCodes resultCode);
void limPrintMlmState(tpAniSirGlobal pMac, tANI_U16 logLevel, tLimMlmStates state);
char* limBssTypeStr(tSirBssType bssType);

#if defined FEATURE_WLAN_ESE || defined WLAN_FEATURE_VOWIFI
extern tSirRetStatus limSendSetMaxTxPowerReq ( tpAniSirGlobal pMac,
                                  tPowerdBm txPower,
                                  tpPESession pSessionEntry );
extern tANI_U8 limGetMaxTxPower(tPowerdBm regMax, tPowerdBm apTxPower, tANI_U8 iniTxPower);
#endif

tANI_U32            limPostMsgApiNoWait(tpAniSirGlobal, tSirMsgQ *);
tANI_U8           limIsAddrBC(tSirMacAddr);
tANI_U8           limIsGroupAddr(tSirMacAddr);

// check for type of scan allowed
tANI_U8 limActiveScanAllowed(tpAniSirGlobal, tANI_U8);

// AID pool management functions
void    limInitPeerIdxpool(tpAniSirGlobal,tpPESession);
tANI_U16     limAssignPeerIdx(tpAniSirGlobal,tpPESession);

void limEnableOverlap11gProtection(tpAniSirGlobal pMac, tpUpdateBeaconParams pBeaconParams, tpSirMacMgmtHdr pMh,tpPESession psessionEntry);
void limUpdateOverlapStaParam(tpAniSirGlobal pMac, tSirMacAddr bssId, tpLimProtStaParams pStaParams);
void limUpdateShortPreamble(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
void limUpdateShortSlotTime(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);

/*
 * The below 'product' check to be removed if 'Association' is
 * allowed in IBSS.
 */
void    limReleasePeerIdx(tpAniSirGlobal, tANI_U16, tpPESession);


void limDecideApProtection(tpAniSirGlobal pMac, tSirMacAddr peerMacAddr,  tpUpdateBeaconParams pBeaconParams,tpPESession);
void
limDecideApProtectionOnDelete(tpAniSirGlobal pMac,
                              tpDphHashNode pStaDs, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);

extern tSirRetStatus limEnable11aProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnable11gProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHtProtectionFrom11g(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHT20Protection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession sessionEntry);
extern tSirRetStatus limEnableHTNonGfProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnableHtRifsProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession psessionEntry);
extern tSirRetStatus limEnableHTLsigTxopProtection(tpAniSirGlobal pMac, tANI_U8 enable, tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams,tpPESession);
extern tSirRetStatus limEnableShortPreamble(tpAniSirGlobal pMac, tANI_U8 enable, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
extern tSirRetStatus limEnableHtOBSSProtection (tpAniSirGlobal pMac, tANI_U8 enable,  tANI_U8 overlap, tpUpdateBeaconParams pBeaconParams, tpPESession);
void limDecideStaProtection(tpAniSirGlobal pMac, tpSchBeaconStruct pBeaconStruct, tpUpdateBeaconParams pBeaconParams, tpPESession psessionEntry);
void limDecideStaProtectionOnAssoc(tpAniSirGlobal pMac, tpSchBeaconStruct pBeaconStruct, tpPESession psessionEntry);
void limUpdateStaRunTimeHTSwitchChnlParams(tpAniSirGlobal pMac, tDot11fIEHTInfo * pHTInfo, tANI_U8 bssIdx, tpPESession psessionEntry);
// Print MAC address utility function
void    limPrintMacAddr(tpAniSirGlobal, tSirMacAddr, tANI_U8);



// Deferred Message Queue read/write
tANI_U8 limWriteDeferredMsgQ(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirMsgQ* limReadDeferredMsgQ(tpAniSirGlobal pMac);
void limHandleDeferMsgError(tpAniSirGlobal pMac, tpSirMsgQ pLimMsg);

// Deferred Message Queue Reset
void limResetDeferredMsgQ(tpAniSirGlobal pMac);

tSirRetStatus limSysProcessMmhMsgApi(tpAniSirGlobal, tSirMsgQ*, tANI_U8);

void limHandleUpdateOlbcCache(tpAniSirGlobal pMac);

tANI_U8 limIsNullSsid( tSirMacSSid *pSsid );

void limProcessAddtsRspTimeout(tpAniSirGlobal pMac, tANI_U32 param);

// 11h Support
void limStopTxAndSwitchChannel(tpAniSirGlobal pMac, tANI_U8 sessionId);
void limProcessChannelSwitchTimeout(tpAniSirGlobal);
tSirRetStatus limStartChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limUpdateChannelSwitch(tpAniSirGlobal, tpSirProbeRespBeacon, tpPESession psessionEntry);
void limProcessQuietTimeout(tpAniSirGlobal);
void limProcessQuietBssTimeout(tpAniSirGlobal);

void limStartQuietTimer(tpAniSirGlobal pMac, tANI_U8 sessionId);
void limSwitchPrimaryChannel(tpAniSirGlobal, tANI_U8,tpPESession);
void limSwitchPrimarySecondaryChannel(tpAniSirGlobal, tpPESession, tANI_U8, ePhyChanBondState);
tAniBool limTriggerBackgroundScanDuringQuietBss(tpAniSirGlobal);
void limUpdateStaRunTimeHTSwtichChnlParams(tpAniSirGlobal pMac, tDot11fIEHTInfo *pRcvdHTInfo, tANI_U8 bssIdx);
void limUpdateStaRunTimeHTCapability(tpAniSirGlobal pMac, tDot11fIEHTCaps *pHTCaps);
void limUpdateStaRunTimeHTInfo(struct sAniSirGlobal *pMac, tDot11fIEHTInfo *pRcvdHTInfo, tpPESession psessionEntry);
void limCancelDot11hChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limCancelDot11hQuiet(tpAniSirGlobal pMac, tpPESession psessionEntry);
tAniBool limIsChannelValidForChannelSwitch(tpAniSirGlobal pMac, tANI_U8 channel);
void limFrameTransmissionControl(tpAniSirGlobal pMac, tLimQuietTxMode type, tLimControlTx mode);
tSirRetStatus limRestorePreChannelSwitchState(tpAniSirGlobal pMac, tpPESession psessionEntry);
tSirRetStatus limRestorePreQuietState(tpAniSirGlobal pMac, tpPESession psessionEntry);

void limPrepareFor11hChannelSwitch(tpAniSirGlobal pMac, tpPESession psessionEntry);
void limSwitchChannelCback(tpAniSirGlobal pMac, eHalStatus status,
                           tANI_U32 *data, tpPESession psessionEntry);

static inline tSirRFBand limGetRFBand(tANI_U8 channel)
{
    if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
        (channel <= SIR_11A_CHANNEL_END))
        return SIR_BAND_5_GHZ;

    if ((channel >= SIR_11B_CHANNEL_BEGIN) &&
        (channel <= SIR_11B_CHANNEL_END))
        return SIR_BAND_2_4_GHZ;

    return SIR_BAND_UNKNOWN;
}


static inline tSirRetStatus
limGetMgmtStaid(tpAniSirGlobal pMac, tANI_U16 *staid, tpPESession psessionEntry)
{
    if (LIM_IS_AP_ROLE(psessionEntry))
        *staid = 1;
    else if (LIM_IS_STA_ROLE(psessionEntry))
        *staid = 0;
    else
        return eSIR_FAILURE;

    return eSIR_SUCCESS;
}

static inline tANI_U8
limIsSystemInSetMimopsState(tpAniSirGlobal pMac)
{
    if (pMac->lim.gLimMlmState == eLIM_MLM_WT_SET_MIMOPS_STATE)
        return true;
    return false;
}

static inline tANI_U8
 isEnteringMimoPS(tSirMacHTMIMOPowerSaveState curState, tSirMacHTMIMOPowerSaveState newState)
 {
    if (curState == eSIR_HT_MIMO_PS_NO_LIMIT &&
        (newState == eSIR_HT_MIMO_PS_DYNAMIC ||newState == eSIR_HT_MIMO_PS_STATIC))
        return TRUE;
    return FALSE;
}

static inline int limSelectCBMode(tDphHashNode *pStaDs, tpPESession psessionEntry,
                                  tANI_U8 channel, tANI_U8 chan_bw)
{
    if ( pStaDs->mlmStaContext.vhtCapability && chan_bw)
    {
        if ( channel== 36 || channel == 52 || channel == 100 ||
             channel == 116 || channel == 149 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW - 1;
        }
        else if ( channel == 40 || channel == 56 || channel == 104 ||
             channel == 120 || channel == 153 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW - 1;
        }
        else if ( channel == 44 || channel == 60 || channel == 108 ||
                  channel == 124 || channel == 157 )
        {
           return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH -1;
        }
        else if ( channel == 48 || channel == 64 || channel == 112 ||
             channel == 128 || channel == 161 )
        {
            return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH - 1;
        }
        else if ( channel == 165 )
        {
            return PHY_SINGLE_CHANNEL_CENTERED;
        }
    }
    else if ( pStaDs->mlmStaContext.htCapability )
    {
        if ( channel== 40 || channel == 48 || channel == 56 ||
             channel == 64 || channel == 104 || channel == 112 ||
             channel == 120 || channel == 128 || channel == 136 ||
             channel == 144 || channel == 153 || channel == 161 )
        {
           return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
        }
        else if ( channel== 36 || channel == 44 || channel == 52 ||
             channel == 60 || channel == 100 || channel == 108 ||
             channel == 116 || channel == 124 || channel == 132 ||
             channel == 140 || channel == 149 || channel == 157 )
        {
           return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
        }
        else if ( channel == 165 )
        {
           return PHY_SINGLE_CHANNEL_CENTERED;
        }
    }
    return PHY_SINGLE_CHANNEL_CENTERED;
}

/// ANI peer station count management and associated actions
void limUtilCountStaAdd(tpAniSirGlobal pMac, tpDphHashNode pSta, tpPESession psessionEntry);
void limUtilCountStaDel(tpAniSirGlobal pMac, tpDphHashNode pSta, tpPESession psessionEntry);

tANI_U8 limGetHTCapability( tpAniSirGlobal, tANI_U32, tpPESession);
void limTxComplete( tHalHandle hHal, void *pData, v_BOOL_t free );

/**********Admit Control***************************************/

//callback function for HAL to issue DelTS request to PE.
//This function will be registered with HAL for callback when TSPEC inactivity timer fires.

void limProcessDelTsInd(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
tSirRetStatus limProcessHalIndMessages(tpAniSirGlobal pMac, tANI_U32 mesgId, void *mesgParam );
tSirRetStatus limValidateDeltsReq(tpAniSirGlobal pMac, tpSirDeltsReq pDeltsReq, tSirMacAddr peerMacAddr,tpPESession psessionEntry);
/**********************************************************/

//callback function registration to HAL for any indication.
void limRegisterHalIndCallBack(tpAniSirGlobal pMac);
void limPktFree (
    tpAniSirGlobal  pMac,
    eFrameType      frmType,
    tANI_U8         *pBD,
    void            *body);



void limGetBDfromRxPacket(tpAniSirGlobal pMac, void *body, tANI_U32 **pBD);

/**
 * \brief Given a base(X) and power(Y), this API will return
 * the result of base raised to power - (X ^ Y)
 *
 * \sa utilsPowerXY
 *
 * \param base Base value
 *
 * \param power Base raised to this Power value
 *
 * \return Result of X^Y
 *
 */
static inline tANI_U32 utilsPowerXY( tANI_U16 base, tANI_U16 power )
{
tANI_U32 result = 1, i;

  for( i = 0; i < power; i++ )
    result *= base;

  return result;
}



tSirRetStatus limPostMlmAddBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pStaDs,
    tANI_U8 tid, tANI_U16 startingSeqNum,tpPESession psessionEntry);
tSirRetStatus limPostMlmAddBARsp( tpAniSirGlobal pMac,
    tSirMacAddr peerMacAddr,
    tSirMacStatusCodes baStatusCode,
    tANI_U8 baDialogToken,
    tANI_U8 baTID,
    tANI_U8 baPolicy,
    tANI_U16 baBufferSize,
    tANI_U16 baTimeout,
    tpPESession psessionEntry);
tSirRetStatus limPostMlmDelBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baDirection,
    tANI_U8 baTID,
    tSirMacReasonCodes baReasonCode ,
    tpPESession psessionEntry);
tSirRetStatus limPostMsgAddBAReq( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baDialogToken,
    tANI_U8 baTID,
    tANI_U8 baPolicy,
    tANI_U16 baBufferSize,
    tANI_U16 baTimeout,
    tANI_U16 baSSN,
    tANI_U8 baDirection,
    tpPESession psessionEntry);
tSirRetStatus limPostMsgDelBAInd( tpAniSirGlobal pMac,
    tpDphHashNode pSta,
    tANI_U8 baTID,
    tANI_U8 baDirection,
    tpPESession psessionEntry);

tSirRetStatus limPostSMStateUpdate(tpAniSirGlobal pMac,
    tANI_U16 StaIdx,
    tSirMacHTMIMOPowerSaveState MIMOPSState,
    tANI_U8 *pPeerStaMac, tANI_U8 sessionId);

void limDeleteStaContext(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
void limProcessAddBaInd(tpAniSirGlobal pMac, tpSirMsgQ limMsg);
void limDeleteBASessions(tpAniSirGlobal pMac, tpPESession pSessionEntry, tANI_U32 baDirection);
void limDelPerBssBASessionsBtc(tpAniSirGlobal pMac);
void limDelAllBASessions(tpAniSirGlobal pMac);
void limDeleteDialogueTokenList(tpAniSirGlobal pMac);
tSirRetStatus limSearchAndDeleteDialogueToken(tpAniSirGlobal pMac, tANI_U8 token, tANI_U16 assocId, tANI_U16 tid);
void limRessetScanChannelInfo(tpAniSirGlobal pMac);
void limAddScanChannelInfo(tpAniSirGlobal pMac, tANI_U8 channelId);

tANI_U8 limGetChannelFromBeacon(tpAniSirGlobal pMac, tpSchBeaconStruct pBeacon);
tSirNwType limGetNwType(tpAniSirGlobal pMac, tANI_U8 channelNum, tANI_U32 type, tpSchBeaconStruct pBeacon);
void limSetTspecUapsdMask(tpAniSirGlobal pMac, tSirMacTSInfo *pTsInfo, tANI_U32 action);

void limSetTspecUapsdMaskPerSession(tpAniSirGlobal pMac,
                                    tpPESession psessionEntry,
                                    tSirMacTSInfo *pTsInfo,
                                    tANI_U32 action);

void limHandleHeartBeatTimeout(tpAniSirGlobal pMac);
void limHandleHeartBeatTimeoutForSession(tpAniSirGlobal pMac, tpPESession psessionEntry);

//void limProcessBtampAddBssRsp(tpAniSirGlobal pMac,tpSirMsgQ pMsgQ,tpPESession peSession);
void limProcessAddStaRsp(tpAniSirGlobal pMac,tpSirMsgQ pMsgQ);

void limUpdateBeacon(tpAniSirGlobal pMac);

void limProcessBtAmpApMlmAddStaRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ, tpPESession psessionEntry);
void limProcessBtAmpApMlmDelBssRsp( tpAniSirGlobal pMac, tpSirMsgQ limMsgQ,tpPESession psessionEntry);

void limProcessBtAmpApMlmDelStaRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ,tpPESession psessionEntry);
tpPESession  limIsIBSSSessionActive(tpAniSirGlobal pMac);
tpPESession limIsApSessionActive(tpAniSirGlobal pMac);
void limHandleHeartBeatFailureTimeout(tpAniSirGlobal pMac);

void limProcessDelStaSelfRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ);
void limProcessAddStaSelfRsp(tpAniSirGlobal pMac,tpSirMsgQ limMsgQ);
v_U8_t* limGetIEPtr(tpAniSirGlobal pMac, v_U8_t *pIes, int length, v_U8_t eid,eSizeOfLenField size_of_len_field);

tANI_U8 limUnmapChannel(tANI_U8 mapChannel);

#define limGetWscIEPtr(pMac, ie, ie_len) \
    cfg_get_vendor_ie_ptr_from_oui(pMac, SIR_MAC_WSC_OUI, SIR_MAC_WSC_OUI_SIZE, ie, ie_len)

#define limGetP2pIEPtr(pMac, ie, ie_len) \
    cfg_get_vendor_ie_ptr_from_oui(pMac, SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE, ie, ie_len)

v_U8_t limGetNoaAttrStreamInMultP2pIes(tpAniSirGlobal pMac,v_U8_t* noaStream,v_U8_t noaLen,v_U8_t overFlowLen);
v_U8_t limGetNoaAttrStream(tpAniSirGlobal pMac, v_U8_t*pNoaStream,tpPESession psessionEntry);

v_U8_t limBuildP2pIe(tpAniSirGlobal pMac, tANI_U8 *ie, tANI_U8 *data, tANI_U8 ie_len);
tANI_BOOLEAN limIsNOAInsertReqd(tpAniSirGlobal pMac);
tANI_BOOLEAN limIsconnectedOnDFSChannel(tANI_U8 currentChannel);
tANI_U8 limGetCurrentOperatingChannel(tpAniSirGlobal pMac);

uint32_t lim_get_max_rate_flags(tpAniSirGlobal mac_ctx, tpDphHashNode sta_ds);

#ifdef WLAN_FEATURE_11AC
tANI_BOOLEAN limCheckVHTOpModeChange( tpAniSirGlobal pMac, tpPESession psessionEntry,
                                      tANI_U8 chanWidth, tANI_U8 staId, tANI_U8 *peerMac);
tANI_BOOLEAN limSetNssChange( tpAniSirGlobal pMac, tpPESession psessionEntry,
                              tANI_U8 rxNss, tANI_U8 staId, tANI_U8 *peerMac);
tANI_BOOLEAN limCheckMembershipUserPosition( tpAniSirGlobal pMac, tpPESession psessionEntry,
                                             tANI_U32 membership, tANI_U32 userPosition,
                                             tANI_U8 staId);
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT

typedef enum
{
    WLAN_PE_DIAG_SCAN_REQ_EVENT = 0,
    WLAN_PE_DIAG_SCAN_ABORT_IND_EVENT,
    WLAN_PE_DIAG_SCAN_RSP_EVENT,
    WLAN_PE_DIAG_JOIN_REQ_EVENT,
    WLAN_PE_DIAG_JOIN_RSP_EVENT,
    WLAN_PE_DIAG_SETCONTEXT_REQ_EVENT,
    WLAN_PE_DIAG_SETCONTEXT_RSP_EVENT,
    WLAN_PE_DIAG_REASSOC_REQ_EVENT,
    WLAN_PE_DIAG_REASSOC_RSP_EVENT,
    WLAN_PE_DIAG_AUTH_REQ_EVENT,
    WLAN_PE_DIAG_AUTH_RSP_EVENT = 10,
    WLAN_PE_DIAG_DISASSOC_REQ_EVENT,
    WLAN_PE_DIAG_DISASSOC_RSP_EVENT,
    WLAN_PE_DIAG_DISASSOC_IND_EVENT,
    WLAN_PE_DIAG_DISASSOC_CNF_EVENT,
    WLAN_PE_DIAG_DEAUTH_REQ_EVENT,
    WLAN_PE_DIAG_DEAUTH_RSP_EVENT,
    WLAN_PE_DIAG_DEAUTH_IND_EVENT,
    WLAN_PE_DIAG_START_BSS_REQ_EVENT,
    WLAN_PE_DIAG_START_BSS_RSP_EVENT,
    WLAN_PE_DIAG_AUTH_IND_EVENT = 20,
    WLAN_PE_DIAG_ASSOC_IND_EVENT,
    WLAN_PE_DIAG_ASSOC_CNF_EVENT,
    WLAN_PE_DIAG_REASSOC_IND_EVENT,
    WLAN_PE_DIAG_SWITCH_CHL_REQ_EVENT,
    WLAN_PE_DIAG_SWITCH_CHL_RSP_EVENT,
    WLAN_PE_DIAG_STOP_BSS_REQ_EVENT,
    WLAN_PE_DIAG_STOP_BSS_RSP_EVENT,
    WLAN_PE_DIAG_DEAUTH_CNF_EVENT,
    WLAN_PE_DIAG_ADDTS_REQ_EVENT,
    WLAN_PE_DIAG_ADDTS_RSP_EVENT = 30,
    WLAN_PE_DIAG_DELTS_REQ_EVENT,
    WLAN_PE_DIAG_DELTS_RSP_EVENT,
    WLAN_PE_DIAG_DELTS_IND_EVENT,
    WLAN_PE_DIAG_ENTER_BMPS_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_BMPS_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_BMPS_IND_EVENT,
    WLAN_PE_DIAG_ENTER_IMPS_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_IMPS_RSP_EVENT = 40,
    WLAN_PE_DIAG_EXIT_IMPS_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_IMPS_RSP_EVENT,
    WLAN_PE_DIAG_ENTER_UAPSD_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_UAPSD_RSP_EVENT,
    WLAN_PE_DIAG_EXIT_UAPSD_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_UAPSD_RSP_EVENT,
    WLAN_PE_DIAG_WOWL_ADD_BCAST_PTRN_EVENT,
    WLAN_PE_DIAG_WOWL_DEL_BCAST_PTRN_EVENT,
    WLAN_PE_DIAG_ENTER_WOWL_REQ_EVENT,
    WLAN_PE_DIAG_ENTER_WOWL_RSP_EVENT = 50,
    WLAN_PE_DIAG_EXIT_WOWL_REQ_EVENT,
    WLAN_PE_DIAG_EXIT_WOWL_RSP_EVENT,
    WLAN_PE_DIAG_HAL_ADDBA_REQ_EVENT,
    WLAN_PE_DIAG_HAL_ADDBA_RSP_EVENT,
    WLAN_PE_DIAG_HAL_DELBA_IND_EVENT,
    WLAN_PE_DIAG_HB_FAILURE_TIMEOUT,
    WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT,
    WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT,
    WLAN_PE_DIAG_PREAUTH_DONE,
    WLAN_PE_DIAG_REASSOCIATING = 60,
    WLAN_PE_DIAG_CONNECTED,
    WLAN_PE_DIAG_ASSOC_REQ_EVENT,
    WLAN_PE_DIAG_AUTH_COMP_EVENT,
    WLAN_PE_DIAG_ASSOC_COMP_EVENT,
    WLAN_PE_DIAG_AUTH_START_EVENT,
    WLAN_PE_DIAG_ASSOC_START_EVENT,
    WLAN_PE_DIAG_REASSOC_START_EVENT,
    WLAN_PE_DIAG_ROAM_AUTH_START_EVENT,
    WLAN_PE_DIAG_ROAM_AUTH_COMP_EVENT,
    WLAN_PE_DIAG_ROAM_ASSOC_START_EVENT = 70,
    WLAN_PE_DIAG_ROAM_ASSOC_COMP_EVENT,
    RESERVED1, /* = 72 for SCAN_COMPLETE */
    RESERVED2, /*  = 73 for SCAN_RES_FOUND */
} WLAN_PE_DIAG_EVENT_TYPE;

void limDiagEventReport(tpAniSirGlobal pMac, tANI_U16 eventType, tpPESession pSessionEntry, tANI_U16 status, tANI_U16 reasonCode);

#endif /* FEATURE_WLAN_DIAG_SUPPORT */

void peSetResumeChannel(tpAniSirGlobal pMac, tANI_U16 channel, ePhyChanBondState cbState);
/*--------------------------------------------------------------------------

  \brief peGetResumeChannel() - Returns the  channel number for scanning, from a valid session.

  This function returns the channel to resume to during link resume. channel id of 0 means HAL will
  resume to previous channel before link suspend

  \param pMac                   - pointer to global adapter context
  \return                            - channel to scan from valid session else zero.

  \sa

  --------------------------------------------------------------------------*/
void peGetResumeChannel(tpAniSirGlobal pMac, tANI_U8* resumeChannel, ePhyChanBondState* resumePhyCbState);


void limGetShortSlotFromPhyMode(tpAniSirGlobal pMac, tpPESession psessionEntry, tANI_U32 phyMode,
                                   tANI_U8 *pShortSlotEnable);

void limCleanUpDisassocDeauthReq(tpAniSirGlobal pMac, tANI_U8 *staMac, tANI_BOOLEAN cleanRxPath);

tANI_BOOLEAN limCheckDisassocDeauthAckPending(tpAniSirGlobal pMac, tANI_U8 *staMac);

#ifdef WLAN_FEATURE_11W
void limPmfSaQueryTimerHandler(void *pMacGlobal, tANI_U32 param);
#endif



void limUtilsframeshtons(tpAniSirGlobal  pCtx,
                            tANI_U8  *pOut,
                            tANI_U16  pIn,
                            tANI_U8  fMsb);

void limUtilsframeshtonl(tpAniSirGlobal  pCtx,
                            tANI_U8  *pOut,
                            tANI_U32  pIn,
                            tANI_U8  fMsb);

void limSetProtectedBit(tpAniSirGlobal  pMac,
                           tpPESession     psessionEntry,
                           tSirMacAddr     peer,
                           tpSirMacMgmtHdr pMacHdr);

tANI_U8* lim_get_ie_ptr(tANI_U8 *pIes, int length, tANI_U8 eid);

#ifdef WLAN_FEATURE_11W
void limPmfComebackTimerCallback(void *context);
#endif /* WLAN_FEATURE_11W */

void lim_set_ht_caps(tpAniSirGlobal p_mac,
			tpPESession p_session_entry,
			tANI_U8 *p_ie_start,
			tANI_U32 num_bytes);
#ifdef WLAN_FEATURE_11AC
void lim_set_vht_caps(tpAniSirGlobal p_mac,
			tpPESession p_session_entry,
			tANI_U8 *p_ie_start,
			tANI_U32 num_bytes);
#endif /* WLAN_FEATURE_11AC */

#ifdef SAP_AUTH_OFFLOAD
void lim_sap_offload_add_sta(tpAniSirGlobal pmac,
                            tpSirMsgQ lim_msgq);
void lim_sap_offload_del_sta(tpAniSirGlobal pmac,
                            tpSirMsgQ lim_msgq);
void
lim_pop_sap_deferred_msg(tpAniSirGlobal pmac, tpPESession sessionentry);

void
lim_push_sap_deferred_msg(tpAniSirGlobal pmac, tpSirMsgQ lim_msgq);

void
lim_init_sap_deferred_msg_queue(tpAniSirGlobal pmac);

void
lim_cleanup_sap_deferred_msg_queue(tpAniSirGlobal pmac);
#else
static inline void
lim_pop_sap_deferred_msg(tpAniSirGlobal pmac, tpPESession sessionentry)
{
	return;
}
static inline void
lim_push_sap_deferred_msg(tpAniSirGlobal pmac, tpSirMsgQ lim_msgq)
{
	return;
}
static inline void
lim_init_sap_deferred_msg_queue(tpAniSirGlobal pmac)
{
    return;
}
static inline  void
lim_cleanup_sap_deferred_msg_queue(tpAniSirGlobal pmac)
{
	return;
}
#endif /* SAP_AUTH_OFFLOAD */
bool lim_validate_received_frame_a1_addr(tpAniSirGlobal mac_ctx,
		tSirMacAddr a1, tpPESession session);

void lim_set_stads_rtt_cap(tpDphHashNode sta_ds, struct s_ext_cap *ext_cap);
void lim_check_and_reset_protection_params(tpAniSirGlobal mac_ctx);
eHalStatus lim_send_ext_cap_ie(tpAniSirGlobal mac_ctx,
			       uint32_t session_id,
			       tDot11fIEExtCap *extracted_extcap, bool merge);

tSirRetStatus lim_strip_extcap_ie(tpAniSirGlobal mac_ctx, uint8_t *addn_ie,
			uint16_t *addn_ielen, uint8_t *extracted_extcap);
void lim_update_extcap_struct(tpAniSirGlobal mac_ctx, uint8_t *buf,
			       tDot11fIEExtCap *ext_cap);
tSirRetStatus lim_strip_extcap_update_struct(tpAniSirGlobal mac_ctx,
		uint8_t* addn_ie, uint16_t *addn_ielen, tDot11fIEExtCap *dst);
void lim_merge_extcap_struct(tDot11fIEExtCap *dst, tDot11fIEExtCap *src);
uint8_t
lim_get_80Mhz_center_channel(uint8_t primary_channel);
#endif /* __LIM_UTILS_H */
