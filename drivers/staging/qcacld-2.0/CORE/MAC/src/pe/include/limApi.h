/*
 * Copyright (c) 2011-2016 The Linux Foundation. All rights reserved.
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
 *
 * This file limApi.h contains the definitions exported by
 * LIM module.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_API_H
#define __LIM_API_H
#include "wniApi.h"
#include "sirApi.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"
#include "sirCommon.h"
#include "sirDebug.h"
#include "schGlobal.h"
#include "utilsApi.h"
#include "limGlobal.h"
#include "halMsgApi.h"
#include "wlan_qct_wda.h"

/* Macro to count heartbeat */
#define limResetHBPktCount(psessionEntry)   (psessionEntry->LimRxedBeaconCntDuringHB = 0)

/* Useful macros for fetching various states in pMac->lim */
/* gLimSystemRole */
#define GET_LIM_SYSTEM_ROLE(psessionEntry)      (psessionEntry->limSystemRole)
#define LIM_IS_UNKNOWN_ROLE(psessionEntry)           \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_UNKNOWN_ROLE)
#define LIM_IS_AP_ROLE(psessionEntry)           \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_AP_ROLE)
#define LIM_IS_BT_AMP_AP_ROLE(psessionEntry)    \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_BT_AMP_AP_ROLE)
#define LIM_IS_BT_AMP_STA_ROLE(psessionEntry)    \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_BT_AMP_STA_ROLE)
#define LIM_IS_STA_ROLE(psessionEntry)          \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_STA_ROLE)
#define LIM_IS_IBSS_ROLE(psessionEntry)         \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_STA_IN_IBSS_ROLE)
#define LIM_IS_P2P_DEVICE_ROLE(psessionEntry)   \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_P2P_DEVICE_ROLE)
#define LIM_IS_P2P_DEVICE_GO(psessionEntry)   \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_P2P_DEVICE_GO)
#define LIM_IS_NDI_ROLE(psessionEntry)   \
                   (GET_LIM_SYSTEM_ROLE(psessionEntry) == eLIM_NDI_ROLE)
/* gLimSmeState */
#define GET_LIM_SME_STATE(pMac)                 (pMac->lim.gLimSmeState)
#define SET_LIM_SME_STATE(pMac, state)          (pMac->lim.gLimSmeState = state)
/* gLimMlmState */
#define GET_LIM_MLM_STATE(pMac)                 (pMac->lim.gLimMlmState)
#define SET_LIM_MLM_STATE(pMac, state)          (pMac->lim.gLimMlmState = state)
/*tpdphHashNode mlmStaContext*/
#define GET_LIM_STA_CONTEXT_MLM_STATE(pStaDs)   (pStaDs->mlmStaContext.mlmState)
#define SET_LIM_STA_CONTEXT_MLM_STATE(pStaDs, state)  (pStaDs->mlmStaContext.mlmState = state)
/* gLimQuietState */
#define GET_LIM_QUIET_STATE(pMac)               (pMac->lim.gLimSpecMgmt.quietState)
#define SET_LIM_QUIET_STATE(pMac, state)        (pMac->lim.gLimSpecMgmt.quietState = state)
#define LIM_IS_CONNECTION_ACTIVE(psessionEntry)  (psessionEntry->LimRxedBeaconCntDuringHB)
/*pMac->lim.gLimProcessDefdMsgs*/
#define GET_LIM_PROCESS_DEFD_MESGS(pMac) (pMac->lim.gLimProcessDefdMsgs)
#define SET_LIM_PROCESS_DEFD_MESGS(pMac, val) (pMac->lim.gLimProcessDefdMsgs = val)
// LIM exported function templates
#define LIM_IS_RADAR_DETECTED(pMac)         (pMac->lim.gLimSpecMgmt.fRadarDetCurOperChan)
#define LIM_SET_RADAR_DETECTED(pMac, val)   (pMac->lim.gLimSpecMgmt.fRadarDetCurOperChan = val)
#define LIM_MIN_BCN_PR_LENGTH  12
#define LIM_BCN_PR_CAPABILITY_OFFSET 10
typedef enum eMgmtFrmDropReason
{
    eMGMT_DROP_NO_DROP,
    eMGMT_DROP_NOT_LAST_IBSS_BCN,
    eMGMT_DROP_INFRA_BCN_IN_IBSS,
    eMGMT_DROP_SCAN_MODE_FRAME,
    eMGMT_DROP_NON_SCAN_MODE_FRAME,
    eMGMT_DROP_INVALID_SIZE,
    eMGMT_DROP_SPURIOUS_FRAME,
}tMgmtFrmDropReason;


/**
 * Function to initialize LIM state machines.
 * This called upon LIM thread creation.
 */
extern tSirRetStatus limInitialize(tpAniSirGlobal);
tSirRetStatus peOpen(tpAniSirGlobal pMac, tMacOpenParameters *pMacOpenParam);
tSirRetStatus peClose(tpAniSirGlobal pMac);
tSirRetStatus limStart(tpAniSirGlobal pMac);
tSirRetStatus peStart(tpAniSirGlobal pMac);
void peStop(tpAniSirGlobal pMac);
tSirRetStatus pePostMsgApi(tpAniSirGlobal pMac, tSirMsgQ* pMsg);
tSirRetStatus peProcessMsg(tpAniSirGlobal pMac, tSirMsgQ* limMsg);
void limDumpInit(tpAniSirGlobal pMac);
/**
 * Function to clean up LIM state.
 * This called upon reset/persona change etc
 */
extern void limCleanup(tpAniSirGlobal);
/// Function to post messages to LIM thread
extern tANI_U32  limPostMsgApi(tpAniSirGlobal, tSirMsgQ *);
uint32_t
lim_post_msg_high_pri(tpAniSirGlobal mac, tSirMsgQ *msg);

/**
 * Function to process messages posted to LIM thread
 * and dispatch to various sub modules within LIM module.
 */
extern void limMessageProcessor(tpAniSirGlobal, tpSirMsgQ);
extern void limProcessMessages(tpAniSirGlobal, tpSirMsgQ); // DT test alt deferred 2
/**
 * Function to check the LIM state if system is in Scan/Learn state.
 */
extern tANI_U8 limIsSystemInScanState(tpAniSirGlobal);
/**
 * Function to handle IBSS coalescing.
 * Beacon Processing module to call this.
 */
extern tSirRetStatus limHandleIBSScoalescing(tpAniSirGlobal,
                                              tpSchBeaconStruct,
                                              tANI_U8 *,tpPESession);
/// Function used by other Sirius modules to read global SME state
 static inline tLimSmeStates
limGetSmeState(tpAniSirGlobal pMac) { return pMac->lim.gLimSmeState; }
extern void limReceivedHBHandler(tpAniSirGlobal, tANI_U8, tpPESession);
extern void limCheckAndQuietBSS(tpAniSirGlobal);
/// Function that triggers STA context deletion
extern void limTriggerSTAdeletion(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry);

#ifdef FEATURE_WLAN_TDLS
// Function that sends TDLS Del Sta indication to SME
extern void limSendSmeTDLSDelStaInd(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry, tANI_U16 reasonCode);
#endif

/// Function that checks for change in AP's capabilties on STA
extern void limDetectChangeInApCapabilities(tpAniSirGlobal,
                                             tpSirProbeRespBeacon,tpPESession);
tSirRetStatus limUpdateShortSlot(tpAniSirGlobal pMac,
                                                            tpSirProbeRespBeacon pBeacon,
                                                            tpUpdateBeaconParams pBeaconParams,tpPESession);

/// creates an addts request action frame and sends it out to staid
extern void limSendAddtsReq (tpAniSirGlobal pMac, tANI_U16 staid, tANI_U8 tsid, tANI_U8 userPrio, tANI_U8 wme);
/// creates a delts request action frame and sends it out to staid
extern void limSendDeltsReq (tpAniSirGlobal pMac, tANI_U16 staid, tANI_U8 tsid, tANI_U8 userPrio, tANI_U8 wme);
#ifdef WLAN_FEATURE_11AC
extern ePhyChanBondState limGet11ACPhyCBState(tpAniSirGlobal pMac, tANI_U8 channel, tANI_U8 htSecondaryChannelOffset, tANI_U8 CenterChan,tpPESession );
#endif
tANI_U8 limIsSystemInActiveState(tpAniSirGlobal pMac);

void limHandleMissedBeaconInd(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
void limPsOffloadHandleMissedBeaconInd(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
void
limSendHeartBeatTimeoutInd(tpAniSirGlobal pMac, tpPESession psessionEntry);
tMgmtFrmDropReason limIsPktCandidateForDrop(tpAniSirGlobal pMac, tANI_U8 *pRxPacketInfo, tANI_U32 subType);
bool lim_is_deauth_diassoc_for_drop(tpAniSirGlobal mac, uint8_t *rx_pkt_info);
#ifdef WLAN_FEATURE_11W
bool lim_is_assoc_req_for_drop(tpAniSirGlobal mac, uint8_t *rx_pkt_info);
#endif
void limMicFailureInd(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void limRoamOffloadSynchInd(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
#endif
/**
 * lim_mon_init_session() - create PE session for monitor mode operation
 * @mac_ptr: mac pointer
 * @msg: Pointer to struct sir_create_session type.
 *
 * Return: NONE
 */
void lim_mon_init_session(tpAniSirGlobal mac_ptr,
			  struct sir_create_session *msg);
void lim_update_lost_link_info(tpAniSirGlobal mac, tpPESession session,
			       int8_t rssi);
/* ----------------------------------------------------------------------- */
// These used to be in DPH
extern void limGetMyMacAddr(tpAniSirGlobal pMac, tANI_U8 *mac);
#define limGetQosMode(psessionEntry, pVal) (*(pVal) = (psessionEntry)->limQosEnabled)
#define limGetWmeMode(psessionEntry, pVal) (*(pVal) = (psessionEntry)->limWmeEnabled)
#define limGetWsmMode(psessionEntry, pVal) (*(pVal) = (psessionEntry)->limWsmEnabled)
#define limGet11dMode(psessionEntry, pVal) (*(pVal) = (psessionEntry)->lim11dEnabled)
#define limGetAckPolicy(pMac, pVal)         (*(pVal) = pMac->lim.ackPolicy)
/* ----------------------------------------------------------------------- */
static inline void limGetPhyMode(tpAniSirGlobal pMac, tANI_U32 *phyMode, tpPESession psessionEntry)
{
   *phyMode = psessionEntry ? psessionEntry->gLimPhyMode : pMac->lim.gLimPhyMode;
}

/* ----------------------------------------------------------------------- */
static inline void limGetRfBand(tpAniSirGlobal pMac, tSirRFBand *band, tpPESession psessionEntry)
{
   *band = psessionEntry ? psessionEntry->limRFBand : SIR_BAND_UNKNOWN;
}

/*--------------------------------------------------------------------------

  \brief peProcessMessages() - Message Processor for PE

  Voss calls this function to dispatch the message to PE

  \param pMac - Pointer to Global MAC structure
  \param pMsg - Pointer to the message structure

  \return  tANI_U32 - TX_SUCCESS for success.

  --------------------------------------------------------------------------*/
tSirRetStatus peProcessMessages(tpAniSirGlobal pMac, tSirMsgQ* pMsg);
/** -------------------------------------------------------------
\fn peFreeMsg
\brief Called by VOS scheduler (function vos_sched_flush_mc_mqs)
\      to free a given PE message on the TX and MC thread.
\      This happens when there are messages pending in the PE
\      queue when system is being stopped and reset.
\param   tpAniSirGlobal pMac
\param   tSirMsgQ       pMsg
\return none
-----------------------------------------------------------------*/
v_VOID_t peFreeMsg( tpAniSirGlobal pMac, tSirMsgQ* pMsg);

/*--------------------------------------------------------------------------

  \brief limRemainOnChnRsp() - API for sending remain on channel response.

  LIM calls this api to send the remain on channel response to SME.

  \param pMac - Pointer to Global MAC structure
  \param status - status of the response
  \param data - pointer to msg

  \return  void

  --------------------------------------------------------------------------*/
void limRemainOnChnRsp(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data);

/*--------------------------------------------------------------------------

  \brief limProcessAbortScanInd() - function for sending abort scan indication.

  LIM calls this function for sending abort scan indication.

  \param pMac - Pointer to Global MAC structure

  \return  void

  --------------------------------------------------------------------------*/
void limProcessAbortScanInd(tpAniSirGlobal pMac, tANI_U8 sessionId);

void lim_smps_force_mode_ind(tpAniSirGlobal mac_ctx, tpSirMsgQ msg);

typedef void (*tp_pe_packetdump_cb)(adf_nbuf_t netbuf,
			uint8_t status, uint8_t vdev_id, uint8_t type);

void pe_register_packetdump_callback(tp_pe_packetdump_cb pe_packetdump_cb);
void pe_deregister_packetdump_callback(void);

/************************************************************/
#endif /* __LIM_API_H */
