/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _ANIGLOBAL_H
#define _ANIGLOBAL_H

#include "qdf_types.h"
#include "sir_common.h"
#include "ani_system_defs.h"
#include "sys_def.h"
#include "dph_global.h"
#include "lim_global.h"
#include "sch_global.h"
#include "sys_global.h"
#include "sir_api.h"

#include "csr_api.h"
#include "sme_ft_api.h"
#include "csr_support.h"
#include "sme_internal.h"
#include "sap_api.h"
#include "csr_internal.h"

#include "sme_rrm_internal.h"
#include "rrm_global.h"

#include <lim_ft_defs.h>
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_mlme_public_struct.h"

/**
 * MAC_CONTEXT() - Convert an opaque mac handle into a mac context
 * @handle: MAC handle to be converted
 *
 * Given an opaque mac handle this function will return the mac
 * context that is associated with that handle.
 *
 * This is the inverse function of MAC_HANDLE()
 *
 * Return: mac context for @handle
 */
static inline struct mac_context *MAC_CONTEXT(mac_handle_t handle)
{
	return (struct mac_context *)handle;
}

/**
 * MAC_HANDLE() - Convert a mac context into an opaque mac handle
 * @mac: MAC context to be converted
 *
 * Given a mac context this function will return the opaque mac handle
 * that is associated with that handle.
 *
 * This is the inverse function of MAC_CONTEXT()
 *
 * Return: opaque handle for @mac
 */
static inline mac_handle_t MAC_HANDLE(struct mac_context *mac)
{
	return (mac_handle_t)mac;
}

#define ANI_DRIVER_TYPE(mac)     (((struct mac_context *)(mac))->gDriverType)

/* ------------------------------------------------------------------- */
/* Bss Qos Caps bit map definition */
#define LIM_BSS_CAPS_OFFSET_HCF 0
#define LIM_BSS_CAPS_OFFSET_WME 1
#define LIM_BSS_CAPS_OFFSET_WSM 2

#define LIM_BSS_CAPS_HCF (1 << LIM_BSS_CAPS_OFFSET_HCF)
#define LIM_BSS_CAPS_WME (1 << LIM_BSS_CAPS_OFFSET_WME)
#define LIM_BSS_CAPS_WSM (1 << LIM_BSS_CAPS_OFFSET_WSM)

/* cap should be one of HCF/WME/WSM */
#define LIM_BSS_CAPS_GET(cap, val) (((val) & (LIM_BSS_CAPS_ ## cap)) >> LIM_BSS_CAPS_OFFSET_ ## cap)
#define LIM_BSS_CAPS_SET(cap, val) ((val) |= (LIM_BSS_CAPS_ ## cap))
#define LIM_BSS_CAPS_CLR(cap, val) ((val) &= (~(LIM_BSS_CAPS_ ## cap)))

/* 40 beacons per heart beat interval is the default + 1 to count the rest */
#define MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL 41

#define SPACE_ASCII_VALUE  32

#define WLAN_HOST_SEQ_NUM_MIN                           2048
#define WLAN_HOST_SEQ_NUM_MAX                           4095
#define LOW_SEQ_NUM_MASK                                0x000F
#define HIGH_SEQ_NUM_MASK                               0x0FF0
#define HIGH_SEQ_NUM_OFFSET                             4
#define DEF_HE_AUTO_SGI_LTF                             0x0F07

#define PMF_WEP_DISABLE 2
#define PMF_INCORRECT_KEY 1
#define PMF_CORRECT_KEY 0

/**
 * enum log_event_type - Type of event initiating bug report
 * @WLAN_LOG_TYPE_NON_FATAL: Non fatal event
 * @WLAN_LOG_TYPE_FATAL: Fatal event
 *
 * Enum indicating the type of event that is initiating the bug report
 */
enum log_event_type {
	WLAN_LOG_TYPE_NON_FATAL,
	WLAN_LOG_TYPE_FATAL,
};

/**
 * enum log_event_indicator - Module triggering bug report
 * @WLAN_LOG_INDICATOR_UNUSED: Unused
 * @WLAN_LOG_INDICATOR_FRAMEWORK: Framework triggers bug report
 * @WLAN_LOG_INDICATOR_HOST_DRIVER: Host driver triggers bug report
 * @WLAN_LOG_INDICATOR_FIRMWARE: FW initiates bug report
 * @WLAN_LOG_INDICATOR_HOST_ONLY: Host triggers fatal event bug report
 *
 * Enum indicating the module that triggered the bug report
 */
enum log_event_indicator {
	WLAN_LOG_INDICATOR_UNUSED,
	WLAN_LOG_INDICATOR_FRAMEWORK,
	WLAN_LOG_INDICATOR_HOST_DRIVER,
	WLAN_LOG_INDICATOR_FIRMWARE,
	WLAN_LOG_INDICATOR_HOST_ONLY,
};

/**
 * enum log_event_host_reason_code - Reason code for bug report
 * @WLAN_LOG_REASON_CODE_UNUSED: Unused
 * @WLAN_LOG_REASON_ROAM_FAIL: Driver initiated roam has failed
 * @WLAN_LOG_REASON_DATA_STALL: Unable to send/receive data due to low resource
 * scenario for a prolonged period
 * @WLAN_LOG_REASON_SME_COMMAND_STUCK: SME command is stuck in SME active queue
 * @WLAN_LOG_REASON_QUEUE_FULL: Defer queue becomes full for a prolonged period
 * @WLAN_LOG_REASON_POWER_COLLAPSE_FAIL: Unable to allow apps power collapse
 * for a prolonged period
 * @WLAN_LOG_REASON_MALLOC_FAIL: Memory allocation Fails
 * @WLAN_LOG_REASON_VOS_MSG_UNDER_RUN: VOS Core runs out of message wrapper
 * @WLAN_LOG_REASON_HDD_TIME_OUT: Wait for event Timeout in HDD layer
   @WLAN_LOG_REASON_SME_OUT_OF_CMD_BUFL sme out of cmd buffer
 * @WLAN_LOG_REASON_NO_SCAN_RESULTS: no scan results to report from HDD
 * This enum contains the different reason codes for bug report
 * @WLAN_LOG_REASON_SCAN_NOT_ALLOWED: scan not allowed due to connection states
 * @WLAN_LOG_REASON_HB_FAILURE: station triggered heart beat failure with AP
 * @WLAN_LOG_REASON_ROAM_HO_FAILURE: Handover failed during LFR3 roaming
 * @WLAN_LOG_REASON_DISCONNECT: Disconnect because of some failure
 */
enum log_event_host_reason_code {
	WLAN_LOG_REASON_CODE_UNUSED,
	WLAN_LOG_REASON_ROAM_FAIL,
	WLAN_LOG_REASON_DATA_STALL,
	WLAN_LOG_REASON_SME_COMMAND_STUCK,
	WLAN_LOG_REASON_QUEUE_FULL,
	WLAN_LOG_REASON_POWER_COLLAPSE_FAIL,
	WLAN_LOG_REASON_MALLOC_FAIL,
	WLAN_LOG_REASON_VOS_MSG_UNDER_RUN,
	WLAN_LOG_REASON_HDD_TIME_OUT,
	WLAN_LOG_REASON_SME_OUT_OF_CMD_BUF,
	WLAN_LOG_REASON_NO_SCAN_RESULTS,
	WLAN_LOG_REASON_SCAN_NOT_ALLOWED,
	WLAN_LOG_REASON_HB_FAILURE,
	WLAN_LOG_REASON_ROAM_HO_FAILURE,
	WLAN_LOG_REASON_DISCONNECT
};


/**
 * enum userspace_log_level - Log level at userspace
 * @LOG_LEVEL_NO_COLLECTION: verbose_level 0 corresponds to no collection
 * @LOG_LEVEL_NORMAL_COLLECT: verbose_level 1 correspond to normal log level,
 * with minimal user impact. this is the default value
 * @LOG_LEVEL_ISSUE_REPRO: verbose_level 2 are enabled when user is lazily
 * trying to reproduce a problem, wifi performances and power can be impacted
 * but device should not otherwise be significantly impacted
 * @LOG_LEVEL_ACTIVE: verbose_level 3+ are used when trying to
 * actively debug a problem
 *
 * Various log levels defined in the userspace for logging applications
 */
enum userspace_log_level {
	LOG_LEVEL_NO_COLLECTION,
	LOG_LEVEL_NORMAL_COLLECT,
	LOG_LEVEL_ISSUE_REPRO,
	LOG_LEVEL_ACTIVE,
};

/**
 * enum wifi_driver_log_level - Log level defined in the driver for logging
 * @WLAN_LOG_LEVEL_OFF: No logging
 * @WLAN_LOG_LEVEL_NORMAL: Default logging
 * @WLAN_LOG_LEVEL_REPRO: Normal debug level
 * @WLAN_LOG_LEVEL_ACTIVE: Active debug level
 *
 * Log levels defined for logging by the wifi driver
 */
enum wifi_driver_log_level {
	WLAN_LOG_LEVEL_OFF,
	WLAN_LOG_LEVEL_NORMAL,
	WLAN_LOG_LEVEL_REPRO,
	WLAN_LOG_LEVEL_ACTIVE,
};

/**
 * enum wifi_logging_ring_id - Ring id of logging entities
 * @RING_ID_WAKELOCK:         Power events ring id
 * @RING_ID_CONNECTIVITY:     Connectivity event ring id
 * @RING_ID_PER_PACKET_STATS: Per packet statistic ring id
 * @RING_ID_DRIVER_DEBUG:     Driver debug messages ring id
 * @RING_ID_FIRMWARE_DEBUG:   Firmware debug messages ring id
 *
 * This enum has the ring id values of logging rings
 */
enum wifi_logging_ring_id {
	RING_ID_WAKELOCK,
	RING_ID_CONNECTIVITY,
	RING_ID_PER_PACKET_STATS,
	RING_ID_DRIVER_DEBUG,
	RING_ID_FIRMWARE_DEBUG,
};

/* ------------------------------------------------------------------- */
/* Change channel generic scheme */
typedef void (*CHANGE_CHANNEL_CALLBACK)(struct mac_context *mac, QDF_STATUS status,
					uint32_t *data,
					struct pe_session *pe_session);

typedef struct sDialogueToken {
	/* bytes 0-3 */
	uint16_t assocId;
	uint8_t token;
	uint8_t rsvd1;
	/* Bytes 4-7 */
	uint16_t tid;
	uint8_t rsvd2[2];

	struct sDialogueToken *next;
} tDialogueToken, *tpDialogueToken;

typedef struct sLimTimers {
	/* TIMERS IN LIM ARE NOT SUPPOSED TO BE ZEROED OUT DURING RESET. */
	/* DURING lim_initialize DONOT ZERO THEM OUT. */

/* STA SPECIFIC TIMERS */

	TX_TIMER gLimPreAuthClnupTimer;

	/* Association related timers */
	TX_TIMER gLimAssocFailureTimer;
	TX_TIMER gLimReassocFailureTimer;

	/* / Wait for Probe after Heartbeat failure timer on STA */
	TX_TIMER gLimProbeAfterHBTimer;

	/* Authentication related timers */
	TX_TIMER gLimAuthFailureTimer;

	/* Join Failure timeout on STA */
	TX_TIMER gLimJoinFailureTimer;

	/* CNF_WAIT timer */
	TX_TIMER *gpLimCnfWaitTimer;

	TX_TIMER gLimAddtsRspTimer;     /* max wait for a response */

	/* Update OLBC Cache Timer */
	TX_TIMER gLimUpdateOlbcCacheTimer;

	TX_TIMER gLimFTPreAuthRspTimer;

	TX_TIMER gLimPeriodicJoinProbeReqTimer;
	TX_TIMER gLimDisassocAckTimer;
	TX_TIMER gLimDeauthAckTimer;
	TX_TIMER g_lim_periodic_auth_retry_timer;

	/* SAE authentication related timer */
	TX_TIMER sae_auth_timer;

/* ********************TIMER SECTION ENDS************************************************** */
/* ALL THE FIELDS BELOW THIS CAN BE ZEROED OUT in lim_initialize */
/* **************************************************************************************** */

} tLimTimers;

typedef struct {
	void *pMlmDisassocReq;
	void *pMlmDeauthReq;
} tLimDisassocDeauthCnfReq;

typedef struct sAniSirLim {
	/* ////////////////////////////////////     TIMER RELATED START /////////////////////////////////////////// */

	tLimTimers lim_timers;
	/* / Flag to track if LIM timers are created or not */
	uint32_t gLimTimersCreated;

	/* ////////////////////////////////////     TIMER RELATED END /////////////////////////////////////////// */

	struct lim_scan_channel_status scan_channel_status;

	uint8_t gLimCurrentBssUapsd;

	/* */
	/* Store the BSS Index returned by HAL during */
	/* WMA_ADD_BSS_RSP here. */
	/* */

	/* For now: */
	/* This will be used during WMA_SET_BSSKEY_REQ in */
	/* order to set the GTK */
	/* Later: */
	/* There could be other interfaces needing this info */
	/* */

	/* */
	/* Due to the asynchronous nature of the interface */
	/* between PE <-> HAL, some transient information */
	/* like this needs to be cached. */
	/* This is cached upon receipt of eWNI_SME_SETCONTEXT_REQ. */
	/* This is released while posting LIM_MLM_SETKEYS_CNF */
	/* */
	void *gpLimMlmSetKeysReq;

	/* ////////////////////////////////////////     BSS RELATED END /////////////////////////////////////////// */

	/* ////////////////////////////////////////     STATS/COUNTER RELATED START /////////////////////////////////////////// */

	uint16_t maxStation;
	uint16_t maxBssId;

	uint32_t gLimNumBeaconsRcvd;
	uint32_t gLimNumBeaconsIgnored;

	uint32_t gLimNumDeferredMsgs;

	/* / Variable to keep track of number of currently associated STAs */
	uint16_t gLimNumOfAniSTAs;      /* count of ANI peers */

	tSirMacAddr gLimHeartBeatApMac[2];
	uint8_t gLimHeartBeatApMacIndex;

	/* Statistics to keep track of no. beacons rcvd in heart beat interval */
	uint16_t
		gLimHeartBeatBeaconStats[MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL];

#ifdef WLAN_DEBUG
	/* Debug counters */
	uint32_t numTot, numBbt, numProtErr, numLearn, numLearnIgnore;
	uint32_t numSme, numMAC[4][16];

	/* Debug counter to track number of Assoc Req frame drops */
	/* when received in sta->mlmState other than LINK_ESTABLISED */
	uint32_t gLimNumAssocReqDropInvldState;
	/* counters to track rejection of Assoc Req due to Admission Control */
	uint32_t gLimNumAssocReqDropACRejectTS;
	uint32_t gLimNumAssocReqDropACRejectSta;
	/* Debug counter to track number of Reassoc Req frame drops */
	/* when received in sta->mlmState other than LINK_ESTABLISED */
	uint32_t gLimNumReassocReqDropInvldState;
	/* Debug counter to track number of Hash Miss event that */
	/* will not cause a sending of de-auth/de-associate frame */
	uint32_t gLimNumHashMissIgnored;

	/* Debug counter to track number of Beacon frames */
	/* received in unexpected state */
	uint32_t gLimUnexpBcnCnt;

	/* Debug counter to track number of Beacon frames */
	/* received in wt-join-state that do have SSID mismatch */
	uint32_t gLimBcnSSIDMismatchCnt;

	/* Debug counter to track number of Link establishments on STA/BP */
	uint32_t gLimNumLinkEsts;

	/* Debug counter to track number of Rx cleanup */
	uint32_t gLimNumRxCleanup;

	/* Debug counter to track different parse problem */
	uint32_t gLim11bStaAssocRejectCount;

#endif

	/* ////////////////////////////////////////     STATS/COUNTER RELATED END /////////////////////////////////////////// */

	/* ////////////////////////////////////////     STATES RELATED START /////////////////////////////////////////// */
	/* Counts Heartbeat failures */
	uint8_t gLimHBfailureCntInLinkEstState;
	uint8_t gLimProbeFailureAfterHBfailedCnt;
	uint8_t gLimHBfailureCntInOtherStates;

	/**
	 * This variable indicates whether LIM module need to
	 * send response to host. Used to identify whether a request
	 * is generated internally within LIM module or by host
	 */
	uint8_t gLimRspReqd;

	/* / Previous SME State */
	tLimSmeStates gLimPrevSmeState;

	/* / MLM State visible across all Sirius modules */
	tLimMlmStates gLimMlmState;

	/* / Previous MLM State */
	tLimMlmStates gLimPrevMlmState;

	/* Can be set to invalid channel. If it is invalid, HAL */
	/* should move to previous valid channel or stay in the */
	/* current channel. CB state goes along with channel to resume to */
	uint16_t gResumeChannel;
	ePhyChanBondState gResumePhyCbState;

	/* Change channel generic scheme */
	CHANGE_CHANNEL_CALLBACK gpchangeChannelCallback;
	uint32_t *gpchangeChannelData;

	/* / SME State visible across all Sirius modules */
	tLimSmeStates gLimSmeState;
	/* / This indicates whether we're an AP, STA in BSS/IBSS */
	tLimSystemRole gLimSystemRole;

	/* Number of STAs that do not support short preamble */
	tLimNoShortParams gLimNoShortParams;

	/* Number of STAs that do not support short slot time */
	tLimNoShortSlotParams gLimNoShortSlotParams;

	/* */
	/* ---------------- DPH ----------------------- */
	uint32_t gLimPhyMode;

	/* ---------------- DPH ----------------------- */

	/* ////////////////////////////////////////     STATES RELATED END /////////////////////////////////////////// */

	/* ////////////////////////////////////////     MISC RELATED START /////////////////////////////////////////// */

	/* Deferred Queue Parameters */
	tLimDeferredMsgQParams gLimDeferredMsgQ;

	/* addts request if any - only one can be outstanding at any time */
	tSirAddtsReq gLimAddtsReq;
	uint8_t gLimAddtsSent;
	uint8_t gLimAddtsRspTimerCount;

	/* protection related config cache */
	tCfgProtection cfgProtection;

	uint8_t gLimProtectionControl;
	/* This flag will remain to be set except while LIM is waiting for specific response messages */
	/* from HAL. e.g when LIM issues ADD_STA req it will clear this flag and when it will receive */
	/* the response the flag will be set. */
	uint8_t gLimProcessDefdMsgs;

	/* UAPSD flag used on AP */
	uint8_t gUapsdEnable;

	/* Used on STA for AC downgrade. This is a dynamic mask
	 * setting which keep tracks of ACs being admitted.
	 * If bit is set to 0: That partiular AC is not admitted
	 * If bit is set to 1: That particular AC is admitted
	 */
	uint8_t gAcAdmitMask[SIR_MAC_DIRECTION_DIRECT];

	/* dialogue token List head/tail for Action frames request sent. */
	tpDialogueToken pDialogueTokenHead;
	tpDialogueToken pDialogueTokenTail;

	tLimTspecInfo tspecInfo[LIM_NUM_TSPEC_MAX];

	/* admission control policy information */
	tLimAdmitPolicyInfo admitPolicyInfo;
#ifdef FEATURE_WLAN_TDLS
	uint8_t gLimTDLSBufStaEnabled;
	uint8_t gLimTDLSUapsdMask;
	uint8_t gLimTDLSOffChannelEnabled;
	uint8_t gLimTDLSWmmMode;
#endif
	/* ////////////////////////////////////////     MISC RELATED END /////////////////////////////////////////// */

	/* ASSOC RELATED START */

	/* Place holder for current authentication request */
	/* being handled */
	tLimMlmAuthReq *gpLimMlmAuthReq;

	/* Reason code to determine the channel change context while sending */
	/* WMA_CHNL_SWITCH_REQ message to HAL */
	uint32_t channelChangeReasonCode;

	/* / MAC level Pre-authentication related globals */
	tSirMacChanNum gLimPreAuthChannelNumber;
	tAniAuthType gLimPreAuthType;
	tSirMacAddr gLimPreAuthPeerAddr;
	uint32_t gLimNumPreAuthContexts;
	tLimPreAuthTable gLimPreAuthTimerTable;

	/* Place holder for Pre-authentication node list */
	struct tLimPreAuthNode *pLimPreAuthList;

	/* Assoc or ReAssoc Response Data/Frame */
	void *gLimAssocResponseData;

	/* One cache for each overlap and associated case. */
	tCacheParams protStaOverlapCache[LIM_PROT_STA_OVERLAP_CACHE_SIZE];
	tCacheParams protStaCache[LIM_PROT_STA_CACHE_SIZE];

	/* Peer RSSI value */
	int8_t bss_rssi;

	/* ASSOC RELATED END */

	/* //////////////////////////////  HT RELATED           ////////////////////////////////////////// */
	/* */
	/* The following global LIM variables maintain/manage */
	/* the runtime configurations related to 802.11n */

	/* 802.11n Station detected HT capability in Beacon Frame */
	uint8_t htCapabilityPresentInBeacon;

	/* 802.11 HT capability: Enabled or Disabled */
	uint8_t htCapability;

	uint8_t gHTGreenfield;

	uint8_t gHTShortGI40Mhz;
	uint8_t gHTShortGI20Mhz;

	/* Set to 0 for 3839 octets */
	/* Set to 1 for 7935 octets */
	uint8_t gHTMaxAmsduLength;

	/* DSSS/CCK at 40 MHz: Enabled 1 or Disabled */
	uint8_t gHTDsssCckRate40MHzSupport;

	/* PSMP Support: Enabled 1 or Disabled 0 */
	uint8_t gHTPSMPSupport;

	/* L-SIG TXOP Protection used only if peer support available */
	uint8_t gHTLsigTXOPProtection;

	/* MIMO Power Save */
	tSirMacHTMIMOPowerSaveState gHTMIMOPSState;

	/* */
	/* A-MPDU Density */
	/* 000 - No restriction */
	/* 001 - 1/8 usec */
	/* 010 - 1/4 usec */
	/* 011 - 1/2 usec */
	/* 100 - 1 usec */
	/* 101 - 2 usec */
	/* 110 - 4 usec */
	/* 111 - 8 usec */
	/* */
	uint8_t gHTAMpduDensity;

	bool gMaxAmsduSizeEnabled;
	/* Maximum Tx/Rx A-MPDU factor */
	uint8_t gHTMaxRxAMpduFactor;

	/* */
	/* Scheduled PSMP related - Service Interval Granularity */
	/* 000 - 5 ms */
	/* 001 - 10 ms */
	/* 010 - 15 ms */
	/* 011 - 20 ms */
	/* 100 - 25 ms */
	/* 101 - 30 ms */
	/* 110 - 35 ms */
	/* 111 - 40 ms */
	/* */
	uint8_t gHTServiceIntervalGranularity;

	/* Indicates whether an AP wants to associate PSMP enabled Stations */
	uint8_t gHTControlledAccessOnly;

	/* OBss Mode . set when we have Non HT STA is associated or with in overlap bss */
	uint8_t gHTObssMode;

	/* Identifies the current Operating Mode */
	tSirMacHTOperatingMode gHTOperMode;

	/* Indicates if PCO is activated in the BSS */
	uint8_t gHTPCOActive;

	/* */
	/* If PCO is active, indicates which PCO phase to use */
	/* 0 - switch to 20 MHz phase */
	/* 1 - switch to 40 MHz phase */
	/* */
	uint8_t gHTPCOPhase;

	/* */
	/* Used only in beacons. For PR, this is set to 0 */
	/* 0 - Primary beacon */
	/* 1 - Secondary beacon */
	/* */
	uint8_t gHTSecondaryBeacon;

	/* */
	/* Dual CTS Protection */
	/* 0 - Use RTS/CTS */
	/* 1 - Dual CTS Protection is used */
	/* */
	uint8_t gHTDualCTSProtection;

	/* */
	/* Identifies a single STBC MCS that shall ne used for */
	/* STBC control frames and STBC beacons */
	/* */
	uint8_t gHTSTBCBasicMCS;

	uint8_t gHTNonGFDevicesPresent;

	/* HT RELATED END */

	/* wsc info required to form the wsc IE */
	tLimWscIeInfo wscIeInfo;
	struct pe_session *gpSession;  /* Pointer to  session table */
	uint8_t max_sta_of_pe_session;

	qdf_mutex_t lim_frame_register_lock;
	qdf_list_t gLimMgmtFrameRegistratinQueue;
	uint32_t tdls_frm_session_id;

	struct pe_session *pe_session;
	uint8_t reAssocRetryAttempt;
	tLimDisassocDeauthCnfReq limDisassocDeauthCnfReq;
	uint8_t deferredMsgCnt;
	uint8_t deauthMsgCnt;
	uint8_t disassocMsgCnt;
	uint8_t gLimIbssStaLimit;

	QDF_STATUS(*sme_msg_callback)
		(struct mac_context *mac, struct scheduler_msg *msg);
	stop_roaming_fn_t stop_roaming_callback;
	uint8_t retry_packet_cnt;
	uint8_t beacon_probe_rsp_cnt_per_scan;
	wlan_scan_requester req_id;
	QDF_STATUS (*sme_bcn_rcv_callback)(hdd_handle_t hdd_handle,
				struct wlan_beacon_report *beacon_report);
} tAniSirLim, *tpAniSirLim;

struct mgmt_frm_reg_info {
	qdf_list_node_t node;   /* MUST be first element */
	uint16_t frameType;
	uint16_t matchLen;
	uint16_t sessionId;
	uint8_t matchData[1];
};

typedef struct sRrmContext {
	struct rrm_config_param rrmConfig;
	tRrmSMEContext rrmSmeContext[MAX_MEASUREMENT_REQUEST];
	tRrmPEContext rrmPEContext;
} tRrmContext, *tpRrmContext;

/**
 * enum tx_ack_status - Indicate TX status
 * @LIM_ACK_NOT_RCD: Default status while waiting for ack status.
 * @LIM_ACK_RCD_SUCCESS: Ack is received.
 * @LIM_ACK_RCD_FAILURE: No Ack received.
 * @LIM_TX_FAILED: Failed to TX
 *
 * Indicate if driver is waiting for ACK status of auth or ACK received for AUTH
 * OR NO ACK is received for the auth sent.
 */
enum tx_ack_status {
	LIM_ACK_NOT_RCD,
	LIM_ACK_RCD_SUCCESS,
	LIM_ACK_RCD_FAILURE,
	LIM_TX_FAILED,
};

/**
 * struct vdev_type_nss - vdev type nss structure
 * @sta: STA Nss value.
 * @sap: SAP Nss value.
 * @p2p_go: P2P GO Nss value.
 * @p2p_cli: P2P CLI Nss value.
 * @p2p_dev: P2P device Nss value.
 * @ibss: IBSS Nss value.
 * @tdls: TDLS Nss value.
 * @ocb: OCB Nss value.
 * @nan: NAN Nss value.
 *
 * Holds the Nss values of different vdev types.
 */
struct vdev_type_nss {
	uint8_t sta;
	uint8_t sap;
	uint8_t p2p_go;
	uint8_t p2p_cli;
	uint8_t p2p_dev;
	uint8_t ibss;
	uint8_t tdls;
	uint8_t ocb;
	uint8_t nan;
	uint8_t ndi;
};

/**
 * struct mgmt_beacon_probe_filter
 * @num_sta_sessions: Number of active PE STA sessions
 * @sta_bssid: Array of PE STA session's peer BSSIDs
 * @num_sap_session: Number of active PE SAP sessions
 * @sap_channel: Array of PE SAP session's channels
 *
 * Used to filter the STA/IBSS/SAP beacons/probes required in PE and
 * drop other unwanted beacon/probe response frames
 */
struct mgmt_beacon_probe_filter {
	uint8_t num_sta_sessions;
	tSirMacAddr sta_bssid[WLAN_MAX_VDEVS];
	uint8_t num_sap_sessions;
	uint8_t sap_channel[WLAN_MAX_VDEVS];
};

#ifdef FEATURE_ANI_LEVEL_REQUEST
struct ani_level_params {
	void (*ani_level_cb)(struct wmi_host_ani_level_event *ani, uint8_t num,
			     void *context);
	void *context;
};
#endif

/**
 * struct mac_context - Global MAC context
 */
struct mac_context {
	enum qdf_driver_type gDriverType;
	struct wlan_mlme_chain_cfg fw_chain_cfg;
	struct wlan_mlme_cfg *mlme_cfg;
	tAniSirLim lim;
	uint8_t nud_fail_behaviour;
	struct sch_context sch;
	tAniSirSys sys;

	/* PAL/HDD handle */
	hdd_handle_t hdd_handle;

	struct sme_context sme;
	tSapStruct sap;
	struct csr_scanstruct scan;
	struct csr_roamstruct roam;
	tRrmContext rrm;
	uint8_t beacon_offload;
	bool pmf_offload;
	bool is_fils_roaming_supported;
	uint32_t f_sta_miracast_mcc_rest_time_val;
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	csr_readyToExtWoWCallback readyToExtWoWCallback;
	void *readyToExtWoWContext;
#endif
	struct vdev_type_nss vdev_type_nss_2g;
	struct vdev_type_nss vdev_type_nss_5g;

	uint16_t mgmtSeqNum;
	sir_mgmt_frame_ind_callback mgmt_frame_ind_cb;
	qdf_atomic_t global_cmd_id;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	void (*chan_info_cb)(struct scan_chan_info *chan_info);
	void (*del_peers_ind_cb)(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id);
	enum  country_src reg_hint_src;
	uint32_t rx_packet_drop_counter;
	enum tx_ack_status auth_ack_status;
	enum tx_ack_status assoc_ack_status;
	uint8_t user_configured_nss;
	uint32_t peer_rssi;
	uint32_t peer_txrate;
	uint32_t peer_rxrate;
	uint32_t rx_retry_cnt;
	uint32_t rx_mc_bc_cnt;
	/* 11k Offload Support */
	bool is_11k_offload_supported;
	uint8_t reject_addba_req;
	uint16_t usr_cfg_ba_buff_size;
	bool is_usr_cfg_amsdu_enabled;
	uint8_t no_ack_policy_cfg[QCA_WLAN_AC_ALL];
	uint32_t he_sgi_ltf_cfg_bit_mask;
	uint8_t usr_cfg_tx_bfee_nsts;
	struct mgmt_beacon_probe_filter bcn_filter;
	tSirMacEdcaParamRecord usr_mu_edca_params[QCA_WLAN_AC_ALL];
	bool usr_cfg_mu_edca_params;
	bool he_om_ctrl_cfg_bw_set;
	uint8_t he_om_ctrl_cfg_bw;
	bool he_om_ctrl_cfg_nss_set;
	uint8_t he_om_ctrl_cfg_nss;
	bool he_om_ctrl_cfg_ul_mu_dis;
	bool he_om_ctrl_cfg_tx_nsts_set;
	uint8_t he_om_ctrl_cfg_tx_nsts;
	bool he_om_ctrl_ul_mu_data_dis;
	uint8_t is_usr_cfg_pmf_wep;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_cap he_cap_2g;
	tDot11fIEhe_cap he_cap_5g;
#endif
	bool obss_scan_offload;
	bool bcn_reception_stats;
	csr_session_close_cb session_close_cb;
	csr_roam_complete_cb session_roam_complete_cb;
#ifdef FEATURE_ANI_LEVEL_REQUEST
	struct ani_level_params ani_params;
#endif

#ifdef WLAN_FEATURE_CAL_FAILURE_TRIGGER
	void (*cal_failure_event_cb)(uint8_t cal_type, uint8_t reason);
#endif
};

#ifdef FEATURE_WLAN_TDLS

#define RFC1042_HDR_LENGTH      (6)
#define GET_BE16(x)             ((uint16_t) (((x)[0] << 8) | (x)[1]))
#define ETH_TYPE_89_0d          (0x890d)
#define ETH_TYPE_LEN            (2)
#define PAYLOAD_TYPE_TDLS_SIZE  (1)
#define PAYLOAD_TYPE_TDLS       (2)

#endif

#endif /* _ANIGLOBAL_H */
