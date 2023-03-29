/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_global.h contains the definitions exported by
 * LIM module.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_GLOBAL_H
#define __LIM_GLOBAL_H

#include "wni_api.h"
#include "sir_api.h"
#include "sir_mac_prot_def.h"
#include "sir_mac_prop_exts.h"
#include "sir_common.h"
#include "sir_debug.h"
#include "wni_cfg.h"
#include "csr_api.h"
#include "sap_api.h"
#include "dot11f.h"
#include "wma_if.h"

/* Deferred Message Queue Length */
#define MAX_DEFERRED_QUEUE_LEN                  80

#ifdef CHANNEL_HOPPING_ALL_BANDS
#define CHAN_HOP_ALL_BANDS_ENABLE        1
#else
#define CHAN_HOP_ALL_BANDS_ENABLE        0
#endif

/* enums exported by LIM are as follows */

/*System role definition */
typedef enum eLimSystemRole {
	eLIM_UNKNOWN_ROLE,
	eLIM_AP_ROLE,
	eLIM_STA_ROLE,
	eLIM_P2P_DEVICE_ROLE,
	eLIM_P2P_DEVICE_GO,
	eLIM_P2P_DEVICE_CLIENT,
	eLIM_NDI_ROLE
} tLimSystemRole;

/*
 * SME state definition accessible across all Sirius modules.
 * AP only states are LIM_SME_CHANNEL_SCAN_STATE &
 * LIM_SME_NORMAL_CHANNEL_SCAN_STATE.
 * Note that these states may also be present in STA
 * side too when DFS support is present for a STA in IBSS mode.
 */
typedef enum eLimSmeStates {
	eLIM_SME_OFFLINE_STATE,
	eLIM_SME_IDLE_STATE,
	eLIM_SME_SUSPEND_STATE,
	eLIM_SME_WT_JOIN_STATE,
	eLIM_SME_WT_AUTH_STATE,
	eLIM_SME_WT_ASSOC_STATE,
	eLIM_SME_WT_REASSOC_STATE,
	eLIM_SME_JOIN_FAILURE_STATE,
	eLIM_SME_ASSOCIATED_STATE,
	eLIM_SME_REASSOCIATED_STATE,
	eLIM_SME_LINK_EST_STATE,
	eLIM_SME_WT_PRE_AUTH_STATE,
	eLIM_SME_WT_DISASSOC_STATE,
	eLIM_SME_WT_DEAUTH_STATE,
	eLIM_SME_WT_START_BSS_STATE,
	eLIM_SME_WT_STOP_BSS_STATE,
	eLIM_SME_NORMAL_STATE,
} tLimSmeStates;

/*
 * MLM state definition.
 * While these states are present on AP too when it is
 * STA mode, per-STA MLM state exclusive to AP is:
 * eLIM_MLM_WT_AUTH_FRAME3.
 */
typedef enum eLimMlmStates {
	eLIM_MLM_OFFLINE_STATE,
	eLIM_MLM_IDLE_STATE,
	eLIM_MLM_WT_JOIN_BEACON_STATE,
	eLIM_MLM_JOINED_STATE,
	eLIM_MLM_BSS_STARTED_STATE,
	eLIM_MLM_WT_AUTH_FRAME2_STATE,
	eLIM_MLM_WT_AUTH_FRAME3_STATE,
	eLIM_MLM_WT_AUTH_FRAME4_STATE,
	eLIM_MLM_AUTH_RSP_TIMEOUT_STATE,
	eLIM_MLM_AUTHENTICATED_STATE,
	eLIM_MLM_WT_ASSOC_RSP_STATE,
	eLIM_MLM_WT_REASSOC_RSP_STATE,
	eLIM_MLM_ASSOCIATED_STATE,
	eLIM_MLM_REASSOCIATED_STATE,
	eLIM_MLM_LINK_ESTABLISHED_STATE,
	eLIM_MLM_WT_ASSOC_CNF_STATE,
	eLIM_MLM_WT_ADD_BSS_RSP_STATE,
	eLIM_MLM_WT_DEL_BSS_RSP_STATE,
	eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE,
	eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE,
	eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE,
	eLIM_MLM_WT_ADD_STA_RSP_STATE,
	eLIM_MLM_WT_DEL_STA_RSP_STATE,
	/*
	 * MLM goes to this state when LIM initiates DELETE_STA
	 * as processing of Assoc req because the entry already exists.
	 * LIM comes out of this state when DELETE_STA response from
	 * HAL is received. LIM needs to maintain this state so that ADD_STA
	 * can be issued while processing DELETE_STA response from HAL.
	 */
	eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE,
	eLIM_MLM_WT_SET_BSS_KEY_STATE,
	eLIM_MLM_WT_SET_STA_KEY_STATE,
	eLIM_MLM_WT_SET_STA_BCASTKEY_STATE,
	eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE,
	eLIM_MLM_WT_FT_REASSOC_RSP_STATE,
	eLIM_MLM_WT_SAE_AUTH_STATE,
} tLimMlmStates;

/* 11h channel switch states */

/*
 * This enum indicates in which state the channel-swith
 * is presently operating.
 * eLIM_11H_CHANSW_INIT - Default state
 * eLIM_11H_CHANSW_RUNNING - When channel switch is running
 * eLIM_11H_CHANSW_END - After channel switch is complete
 */
typedef enum eLimDot11hChanSwStates {
	eLIM_11H_CHANSW_INIT,
	eLIM_11H_CHANSW_RUNNING,
	eLIM_11H_CHANSW_END
} tLimDot11hChanSwStates;

/* MLM Req/Cnf structure definitions */
typedef struct sLimMlmAuthReq {
	tSirMacAddr peerMacAddr;
	tAniAuthType authType;
	uint8_t sessionId;
} tLimMlmAuthReq, *tpLimMlmAuthReq;

typedef struct sLimMlmJoinReq {
	tSirMacRateSet operationalRateSet;
	uint8_t sessionId;
	struct bss_description bssDescription;
	/*
	 * WARNING: Pls make bssDescription as last variable in struct
	 * tLimMlmJoinReq as it has ieFields followed after this bss
	 * description. Adding a variable after this corrupts the ieFields
	 */
} tLimMlmJoinReq, *tpLimMlmJoinReq;

/* Forward declarations */
struct sSirAssocReq;
struct sDphHashNode;

/* struct lim_assoc_data - Assoc data to be cached to defer association
 *			   indication to SME
 * @present: Indicates whether assoc data is present or not
 * @sub_type: Indicates whether it is Association Request(=0) or Reassociation
 *            Request(=1) frame
 * @hdr: MAC header
 * @assoc_req: pointer to parsed ASSOC/REASSOC Request frame
 * @pmf_connection: flag indicating pmf connection
 * @assoc_req_copied: boolean to indicate if assoc req was copied to tmp above
 * @dup_entry: flag indicating if duplicate entry found
 * @sta_ds: station dph entry
 */
struct lim_assoc_data {
	bool present;
	uint8_t sub_type;
	tSirMacMgmtHdr hdr;
	struct sSirAssocReq *assoc_req;
	bool pmf_connection;
	bool assoc_req_copied;
	bool dup_entry;
	struct sDphHashNode *sta_ds;
};

/* Pre-authentication structure definition */
typedef struct tLimPreAuthNode {
	struct tLimPreAuthNode *next;
	tSirMacAddr peerMacAddr;
	tAniAuthType authType;
	tLimMlmStates mlmState;
	uint8_t authNodeIdx;
	uint8_t challengeText[SIR_MAC_AUTH_CHALLENGE_LENGTH];
	uint8_t fTimerStarted:1;
	uint8_t fSeen:1;
	uint8_t fFree:1;
	uint8_t rsvd:5;
	TX_TIMER timer;
	uint16_t seq_num;
	unsigned long timestamp;
	/* keeping copy of association request received, this is
	 * to defer the association request processing
	 */
	struct lim_assoc_data assoc_req;
} tLimPreAuthNode, *tpLimPreAuthNode;

/* Pre-authentication table definition */
typedef struct tLimPreAuthTable {
	uint32_t numEntry;
	tLimPreAuthNode **pTable;
} tLimPreAuthTable, *tpLimPreAuthTable;

/**
 * struct lim_sta_context - LIM per STA structure
 * @mlmState: LIM State
 * @authType: Authentication algorithm
 * @akm_type: AKM of the connection
 * @listenInterval: Listen interval
 * @capabilityInfo: Capabilities
 * @disassocReason: Disassociation reason code
 * @resultCode:     Result code
 * @subType:        Indicates association or reassociation
 * @updateContext:  Update context
 * @schClean:       Scheduler clean
 * @htCapability:   802.11n HT capability
 * @vhtCapability:  802.11ac VHT capability
 * @cleanupTrigger: Cleanup trigger
 * @protStatusCode: Protocol Status code
 * @he_capable:     802.11ax HE capability
 * @owe_ie:         Pointer to OWE IE
 * @owe_ie_len:     Length of OWE IE
 */
struct lim_sta_context {
	tLimMlmStates mlmState;
	tAniAuthType authType;		/* auth algo in auth frame */
	enum ani_akm_type akm_type;	/* akm in rsn/wpa ie */
	uint16_t listenInterval;
	tSirMacCapabilityInfo capabilityInfo;
	enum wlan_reason_code disassocReason;

	tSirResultCodes resultCode;

	uint8_t subType:1;      /* Indicates ASSOC (0) or REASSOC (1) */
	uint8_t updateContext:1;
	uint8_t schClean:1;
	/* 802.11n HT Capability in Station: Enabled 1 or DIsabled 0 */
	uint8_t htCapability:1;
	uint8_t vhtCapability:1;
	uint16_t cleanupTrigger;
	uint16_t protStatusCode;
#ifdef WLAN_FEATURE_11AX
	bool he_capable;
#endif
	bool force_1x1;
	uint8_t *owe_ie;
	uint32_t owe_ie_len;
};

/* Structure definition to hold deferred messages queue parameters */
typedef struct sLimDeferredMsgQParams {
	struct scheduler_msg deferredQueue[MAX_DEFERRED_QUEUE_LEN];
	uint16_t size;
	uint16_t read;
	uint16_t write;
} tLimDeferredMsgQParams, *tpLimDeferredMsgQParams;

typedef struct sCfgProtection {
	uint32_t overlapFromlla:1;
	uint32_t overlapFromllb:1;
	uint32_t overlapFromllg:1;
	uint32_t overlapHt20:1;
	uint32_t overlapNonGf:1;
	uint32_t overlapLsigTxop:1;
	uint32_t overlapRifs:1;
	uint32_t overlapOBSS:1; /* added for obss */
	uint32_t fromlla:1;
	uint32_t fromllb:1;
	uint32_t fromllg:1;
	uint32_t ht20:1;
	uint32_t nonGf:1;
	uint32_t lsigTxop:1;
	uint32_t rifs:1;
	uint32_t obss:1;        /* added for Obss */
} tCfgProtection, *tpCfgProtection;

typedef enum eLimProtStaCacheType {
	eLIM_PROT_STA_CACHE_TYPE_INVALID,
	eLIM_PROT_STA_CACHE_TYPE_llB,
	eLIM_PROT_STA_CACHE_TYPE_llG,
	eLIM_PROT_STA_CACHE_TYPE_HT20
} tLimProtStaCacheType;

typedef struct sCacheParams {
	uint8_t active;
	tSirMacAddr addr;
	tLimProtStaCacheType protStaCacheType;

} tCacheParams, *tpCacheParams;

#define LIM_PROT_STA_OVERLAP_CACHE_SIZE    HAL_NUM_ASSOC_STA
#define LIM_PROT_STA_CACHE_SIZE            HAL_NUM_ASSOC_STA

typedef struct sLimProtStaParams {
	uint8_t numSta;
	uint8_t protectionEnabled;
} tLimProtStaParams, *tpLimProtStaParams;

typedef struct sLimNoShortParams {
	uint8_t numNonShortPreambleSta;
	tCacheParams staNoShortCache[LIM_PROT_STA_CACHE_SIZE];
} tLimNoShortParams, *tpLimNoShortParams;

typedef struct sLimNoShortSlotParams {
	uint8_t numNonShortSlotSta;
	tCacheParams staNoShortSlotCache[LIM_PROT_STA_CACHE_SIZE];
} tLimNoShortSlotParams, *tpLimNoShortSlotParams;

/* Enums used for channel switching. */
typedef enum eLimChannelSwitchState {
	eLIM_CHANNEL_SWITCH_IDLE,
	eLIM_CHANNEL_SWITCH_PRIMARY_ONLY,
	eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY
} tLimChannelSwitchState;

/* Channel Switch Info */
typedef struct sLimChannelSwitchInfo {
	tLimChannelSwitchState state;
	uint32_t sw_target_freq;
	uint8_t primaryChannel;
	uint8_t ch_center_freq_seg0;
	uint8_t ch_center_freq_seg1;
	uint8_t sec_ch_offset;
	enum phy_ch_width ch_width;
	int8_t switchCount;
	uint32_t switchTimeoutValue;
	uint8_t switchMode;
} tLimChannelSwitchInfo, *tpLimChannelSwitchInfo;

typedef struct sLimOperatingModeInfo {
	uint8_t present;
	uint8_t chanWidth:2;
	uint8_t reserved:2;
	uint8_t rxNSS:3;
	uint8_t rxNSSType:1;
} tLimOperatingModeInfo, *tpLimOperatingModeInfo;

typedef struct sLimWiderBWChannelSwitch {
	uint8_t newChanWidth;
	uint8_t newCenterChanFreq0;
	uint8_t newCenterChanFreq1;
} tLimWiderBWChannelSwitchInfo, *tpLimWiderBWChannelSwitchInfo;

typedef struct sLimTspecInfo {
	/* 0==free, else used */
	uint8_t inuse;
	/* index in list */
	uint8_t idx;
	tSirMacAddr staAddr;
	uint16_t assocId;
	struct mac_tspec_ie tspec;
	/* number of Tclas elements */
	uint8_t numTclas;
	tSirTclasInfo tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
	uint8_t tclasProc;
	/* tclassProc is valid only if this is set to 1. */
	uint8_t tclasProcPresent:1;
} qdf_packed tLimTspecInfo, *tpLimTspecInfo;

typedef struct sLimAdmitPolicyInfo {
	/* admit control policy type */
	uint8_t type;
	/* oversubscription factor : 0 means nothing is allowed */
	uint8_t bw_factor;
	/* valid only when 'type' is set BW_FACTOR */
} tLimAdmitPolicyInfo, *tpLimAdmitPolicyInfo;

typedef enum eLimWscEnrollState {
	eLIM_WSC_ENROLL_NOOP,
	eLIM_WSC_ENROLL_BEGIN,
	eLIM_WSC_ENROLL_IN_PROGRESS,
	eLIM_WSC_ENROLL_END
} tLimWscEnrollState;

#define WSC_PASSWD_ID_PUSH_BUTTON         (0x0004)

typedef struct sLimWscIeInfo {
	bool apSetupLocked;
	bool selectedRegistrar;
	uint16_t selectedRegistrarConfigMethods;
	tLimWscEnrollState wscEnrollmentState;
	tLimWscEnrollState probeRespWscEnrollmentState;
	uint8_t reqType;
	uint8_t respType;
} tLimWscIeInfo, *tpLimWscIeInfo;

/* maximum number of tspec's supported */
#define LIM_NUM_TSPEC_MAX      15

/* structure to hold all 11h specific data */
typedef struct sLimSpecMgmtInfo {
	tLimDot11hChanSwStates dot11hChanSwState;
} tLimSpecMgmtInfo, *tpLimSpecMgmtInfo;

/**
 * struct lim_delba_req_info - Delba request struct
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @tid: tid
 * @reason_code: reason code
 */
struct lim_delba_req_info {
	uint8_t vdev_id;
	tSirMacAddr peer_macaddr;
	uint8_t tid;
	uint8_t reason_code;
};
#endif
