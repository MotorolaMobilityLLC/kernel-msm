/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_types.h contains the definitions used by all
 * all LIM modules.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_TYPES_H
#define __LIM_TYPES_H

#include "wni_api.h"
#include "sir_api.h"
#include "sir_common.h"
#include "sir_mac_prot_def.h"
#include "utils_api.h"

#include "lim_api.h"
#include "lim_trace.h"
#include "lim_send_sme_rsp_messages.h"
#include "sys_global.h"
#include "dph_global.h"
#include "parser_api.h"
#include "wma_if.h"

#define LINK_TEST_DEFER 1

#define TRACE_EVENT_CNF_TIMER_DEACT        0x6600
#define TRACE_EVENT_CNF_TIMER_ACT          0x6601
#define TRACE_EVENT_AUTH_RSP_TIMER_DEACT   0x6602
#define TRACE_EVENT_AUTH_RSP_TIMER_ACT     0x6603

/* MLM message types */
enum mlmmsgtype {
	LIM_MLM_MSG_START = 1000,
	LIM_MLM_SCAN_REQ = LIM_MLM_MSG_START,
	LIM_MLM_SCAN_CNF = (LIM_MLM_MSG_START + 1),
	LIM_MLM_START_CNF  = (LIM_MLM_MSG_START + 3),
	LIM_MLM_JOIN_REQ = (LIM_MLM_MSG_START + 4),
	LIM_MLM_JOIN_CNF = (LIM_MLM_MSG_START + 5),
	LIM_MLM_AUTH_REQ = (LIM_MLM_MSG_START + 6),
	LIM_MLM_AUTH_CNF = (LIM_MLM_MSG_START + 7),
	LIM_MLM_AUTH_IND = (LIM_MLM_MSG_START + 8),
	LIM_MLM_ASSOC_REQ = (LIM_MLM_MSG_START + 9),
	LIM_MLM_ASSOC_CNF = (LIM_MLM_MSG_START + 10),
	LIM_MLM_ASSOC_IND = (LIM_MLM_MSG_START + 11),
	LIM_MLM_DISASSOC_REQ = (LIM_MLM_MSG_START + 12),
	LIM_MLM_DISASSOC_CNF = (LIM_MLM_MSG_START + 13),
	LIM_MLM_DISASSOC_IND = (LIM_MLM_MSG_START + 14),
	LIM_MLM_REASSOC_CNF = (LIM_MLM_MSG_START + 15),
	LIM_MLM_REASSOC_IND = (LIM_MLM_MSG_START + 16),
	LIM_MLM_DEAUTH_REQ = (LIM_MLM_MSG_START + 17),
	LIM_MLM_DEAUTH_CNF = (LIM_MLM_MSG_START + 18),
	LIM_MLM_DEAUTH_IND = (LIM_MLM_MSG_START + 19),
	LIM_MLM_TSPEC_REQ = (LIM_MLM_MSG_START + 20),
	LIM_MLM_TSPEC_CNF = (LIM_MLM_MSG_START + 21),
	LIM_MLM_TSPEC_IND = (LIM_MLM_MSG_START + 22),
	LIM_MLM_SETKEYS_CNF  =  LIM_MLM_MSG_START + 24,
	LIM_MLM_LINK_TEST_STOP_REQ  =  LIM_MLM_MSG_START + 30,
	LIM_MLM_PURGE_STA_IND = (LIM_MLM_MSG_START + 31),
	/*
	 * Values (LIM_MLM_MSG_START + 32) through
	 * (LIM_MLM_MSG_START + 40) are unused.
	 */
};

#define LIM_WEP_IN_FC           1
#define LIM_NO_WEP_IN_FC        0

#define LIM_DECRYPT_ICV_FAIL    1

/* / Definitions to distinquish between Association/Reassociaton */
#define LIM_ASSOC    0
#define LIM_REASSOC  1

/* / Verifies whether given mac addr matches the CURRENT Bssid */
#define IS_CURRENT_BSSID(mac, addr, pe_session)  (!qdf_mem_cmp(addr, \
								      pe_session->bssId, \
								      sizeof(pe_session->bssId)))
/* / Verifies whether given addr matches the REASSOC Bssid */
#define IS_REASSOC_BSSID(mac, addr, pe_session)  (!qdf_mem_cmp(addr, \
								      pe_session->limReAssocbssId, \
								      sizeof(pe_session->limReAssocbssId)))

#define REQ_TYPE_REGISTRAR                   (0x2)
#define REQ_TYPE_WLAN_MANAGER_REGISTRAR      (0x3)

#define RESP_TYPE_ENROLLEE_INFO_ONLY         (0x0)
#define RESP_TYPE_ENROLLEE_OPEN_8021X        (0x1)
#define RESP_TYPE_AP                         (0x3)


#define HAL_USE_SELF_STA_REQUESTED_MASK     0x2 /* bit 1 for STA overwrite with selfSta Requested. */

#define HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40      /* Bit 6 will be used to control BD rate for Management frames */
#define HAL_USE_PEER_STA_REQUESTED_MASK   0x80  /* bit 7 will be used to control frames for p2p interface */
#define HAL_USE_PMF   0x20
#define HAL_USE_INCORRECT_KEY_PMF   0x10

#define MGMT_TX_USE_INCORRECT_KEY   BIT(0)

#define LIM_DOS_PROTECTION_TIME 1000 //1000ms
#define LIM_MIN_RSSI 0 /* 0dbm */
/* enums used by LIM are as follows */

enum eLimDisassocTrigger {
	eLIM_HOST_DISASSOC,
	eLIM_PEER_ENTITY_DISASSOC,
	eLIM_LINK_MONITORING_DISASSOC,
	eLIM_PROMISCUOUS_MODE_DISASSOC,
	eLIM_HOST_DEAUTH,
	eLIM_PEER_ENTITY_DEAUTH,
	eLIM_LINK_MONITORING_DEAUTH,
	eLIM_JOIN_FAILURE,
	eLIM_REASSOC_REJECT,
	eLIM_DUPLICATE_ENTRY
};

/**
 * enum eChannelChangeReasonCodes - Reason code to determine the channel change
 * reason
 * @LIM_SWITCH_CHANNEL_REASSOC: channel switch to reassoc
 * @LIM_SWITCH_CHANNEL_JOIN: switch for connect req
 * @LIM_SWITCH_CHANNEL_OPERATION: Generic change channel for STA
 * @LIM_SWITCH_CHANNEL_SAP_DFS: SAP channel change req
 * @LIM_SWITCH_CHANNEL_HT_WIDTH: HT channel width change reg
 * @LIM_SWITCH_CHANNEL_MONITOR: Monitor mode channel change req
 */
enum eChannelChangeReasonCodes {
	LIM_SWITCH_CHANNEL_REASSOC,
	LIM_SWITCH_CHANNEL_JOIN,
	LIM_SWITCH_CHANNEL_OPERATION,
	LIM_SWITCH_CHANNEL_SAP_DFS,
	LIM_SWITCH_CHANNEL_HT_WIDTH,
	LIM_SWITCH_CHANNEL_MONITOR,
};

typedef struct sLimMlmStartReq {
	tSirMacSSid ssId;
	enum bss_type bssType;
	tSirMacAddr bssId;
	tSirMacBeaconInterval beaconPeriod;
	uint8_t dtimPeriod;
	tSirMacCfParamSet cfParamSet;
	uint32_t oper_ch_freq;
	ePhyChanBondState cbMode;
	tSirMacRateSet rateSet;
	uint8_t sessionId;      /* Added For BT-AMP Support */

	/* Parameters reqd for new HAL (message) interface */
	tSirNwType nwType;
	uint8_t htCapable;
	tSirMacHTOperatingMode htOperMode;
	uint8_t dualCTSProtection;
	uint8_t txChannelWidthSet;
	uint8_t ssidHidden;
	uint8_t wps_state;
	uint8_t obssProtEnabled;
	uint16_t beacon_tx_rate;
	uint32_t cac_duration_ms;
	uint32_t dfs_regdomain;
} tLimMlmStartReq, *tpLimMlmStartReq;

typedef struct sLimMlmStartCnf {
	tSirResultCodes resultCode;
	uint8_t sessionId;
} tLimMlmStartCnf, *tpLimMlmStartCnf;

typedef struct sLimMlmJoinCnf {
	tSirResultCodes resultCode;
	uint16_t protStatusCode;
	uint8_t sessionId;
} tLimMlmJoinCnf, *tpLimMlmJoinCnf;

typedef struct sLimMlmAssocReq {
	tSirMacAddr peerMacAddr;
	uint16_t capabilityInfo;
	tSirMacListenInterval listenInterval;
	uint8_t sessionId;
} tLimMlmAssocReq, *tpLimMlmAssocReq;

typedef struct sLimMlmAssocCnf {
	tSirResultCodes resultCode;     /* Internal status code. */
	uint16_t protStatusCode;        /* Protocol Status code. */
	uint8_t sessionId;
} tLimMlmAssocCnf, *tpLimMlmAssocCnf;

typedef struct sLimMlmAssocInd {
	tSirMacAddr peerMacAddr;
	uint16_t aid;
	tAniAuthType authType;
	enum ani_akm_type akm_type;
	tAniSSID ssId;
	tSirRSNie rsnIE;
	tSirWAPIie wapiIE;
	tSirAddie addIE;        /* additional IE received from the peer, which possibly includes WSC IE and/or P2P IE. */
	tSirMacCapabilityInfo capabilityInfo;
	bool spectrumMgtIndicator;
	struct power_cap_info powerCap;
	struct supported_channels supportedChannels;
	uint8_t sessionId;

	bool WmmStaInfoPresent;

	/* Required for indicating the frames to upper layer */
	uint32_t beaconLength;
	uint8_t *beaconPtr;
	uint32_t assocReqLength;
	uint8_t *assocReqPtr;
	struct oem_channel_info chan_info;
	bool ampdu;
	bool sgi_enable;
	bool tx_stbc;
	bool rx_stbc;
	tSirMacHTChannelWidth ch_width;
	enum sir_sme_phy_mode mode;
	uint8_t max_supp_idx;
	uint8_t max_ext_idx;
	uint8_t max_mcs_idx;
	uint8_t rx_mcs_map;
	uint8_t tx_mcs_map;
	uint8_t ecsa_capable;

	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	bool he_caps_present;
	bool is_sae_authenticated;
} tLimMlmAssocInd, *tpLimMlmAssocInd;

typedef struct sLimMlmReassocReq {
	tSirMacAddr peerMacAddr;
	uint16_t capabilityInfo;
	tSirMacListenInterval listenInterval;
	uint8_t sessionId;
} tLimMlmReassocReq, *tpLimMlmReassocReq;

typedef struct sLimMlmReassocCnf {
	tSirResultCodes resultCode;
	uint16_t protStatusCode;        /* Protocol Status code. */
	uint8_t sessionId;
} tLimMlmReassocCnf, *tpLimMlmReassocCnf;

typedef struct sLimMlmAuthCnf {
	tSirMacAddr peerMacAddr;
	tAniAuthType authType;
	tSirResultCodes resultCode;
	uint16_t protStatusCode;
	uint8_t sessionId;
} tLimMlmAuthCnf, *tpLimMlmAuthCnf;

typedef struct sLimMlmDeauthReq {
	struct qdf_mac_addr peer_macaddr;
	uint16_t reasonCode;
	uint16_t deauthTrigger;
	uint16_t aid;
	uint8_t sessionId;      /* Added for BT-AMP SUPPORT */

} tLimMlmDeauthReq, *tpLimMlmDeauthReq;

typedef struct sLimMlmDeauthCnf {
	struct qdf_mac_addr peer_macaddr;
	tSirResultCodes resultCode;
	uint16_t deauthTrigger;
	uint16_t aid;
	uint8_t sessionId;
} tLimMlmDeauthCnf, *tpLimMLmDeauthCnf;

typedef struct sLimMlmDeauthInd {
	tSirMacAddr peerMacAddr;
	uint16_t reasonCode;
	uint16_t deauthTrigger;
	uint16_t aid;
} tLimMlmDeauthInd, *tpLimMlmDeauthInd;

typedef struct sLimMlmDisassocReq {
	struct qdf_mac_addr peer_macaddr;
	uint16_t reasonCode;
	uint16_t disassocTrigger;
	uint16_t aid;
	uint8_t sessionId;
} tLimMlmDisassocReq, *tpLimMlmDisassocReq;

typedef struct sLimMlmDisassocCnf {
	tSirMacAddr peerMacAddr;
	tSirResultCodes resultCode;
	uint16_t disassocTrigger;
	uint16_t aid;
	uint8_t sessionId;
} tLimMlmDisassocCnf, *tpLimMlmDisassocCnf;

typedef struct sLimMlmDisassocInd {
	tSirMacAddr peerMacAddr;
	uint16_t reasonCode;
	uint16_t disassocTrigger;
	uint16_t aid;
	uint8_t sessionId;
} tLimMlmDisassocInd, *tpLimMlmDisassocInd;

typedef struct sLimMlmPurgeStaInd {
	tSirMacAddr peerMacAddr;
	uint16_t reasonCode;
	uint16_t purgeTrigger;
	uint16_t aid;
	uint8_t sessionId;
} tLimMlmPurgeStaInd, *tpLimMlmPurgeStaInd;

/**
 * struct sLimMlmSetKeysCnf - set key confirmation parameters
 * @peer_macaddr: peer mac address
 * @resultCode: Result of set key operation
 * @aid: association id
 * @sessionId: PE session id
 * @key_len_nonzero: Keys are non-zero length
 */
typedef struct sLimMlmSetKeysCnf {
	struct qdf_mac_addr peer_macaddr;
	uint16_t resultCode;
	uint16_t aid;
	uint8_t sessionId;
	bool key_len_nonzero;
} tLimMlmSetKeysCnf, *tpLimMlmSetKeysCnf;

/* Function templates */

bool lim_process_sme_req_messages(struct mac_context *, struct scheduler_msg *);
void lim_process_mlm_req_messages(struct mac_context *, struct scheduler_msg *);
void lim_process_mlm_rsp_messages(struct mac_context *, uint32_t, uint32_t *);
void lim_process_sme_del_bss_rsp(struct mac_context *mac,
				 struct pe_session *pe_session);

/**
 * lim_process_mlm_start_cnf(): called to processes MLM_START_CNF message from
 * MLM State machine.
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: A pointer to the MLM message buffer
 *
 * Return: None
 */
void lim_process_mlm_start_cnf(struct mac_context *mac_ctx, uint32_t *msg_buf);

void lim_get_random_bssid(struct mac_context *mac, uint8_t *data);

/* Function to handle HT and HT IE CFG parameter intializations */
void handle_ht_capabilityand_ht_info(struct mac_context *mac,
				     struct pe_session *pe_session);

void lim_handle_param_update(struct mac_context *mac, eUpdateIEsType cfgId);

/* Function to apply CFG parameters before join/reassoc/start BSS */
void lim_apply_configuration(struct mac_context *, struct pe_session *);

/**
 * lim_set_cfg_protection() - sets lim global cfg cache from the config
 * @mac: global mac context
 * @pesessionEntry: PE session
 *
 * Return none
 */
void lim_set_cfg_protection(struct mac_context *mac, struct pe_session *pesessionEntry);

/* Function to Initialize MLM state machine on STA */
QDF_STATUS lim_init_mlm(struct mac_context *);

/* Function to cleanup MLM state machine */
void lim_cleanup_mlm(struct mac_context *);

/* Management frame handling functions */
void lim_process_beacon_frame(struct mac_context *, uint8_t *, struct pe_session *);
void lim_process_probe_req_frame(struct mac_context *, uint8_t *, struct pe_session *);
void lim_process_probe_rsp_frame(struct mac_context *, uint8_t *, struct pe_session *);
void lim_process_probe_req_frame_multiple_bss(struct mac_context *, uint8_t *,
					      struct pe_session *);

/* Process Auth frame when we have a session in progress. */
void lim_process_auth_frame(struct mac_context *, uint8_t *, struct pe_session *);

/**
 * lim_process_auth_frame_no_session() - Process auth frame received from AP to
 * which we are not connected currently.
 * @mac: Pointer to global mac context
 * @bd: Pointer to rx auth frame
 * @body: Pointer to lim_msg->body_ptr
 *
 * This is possibly the pre-auth from the neighbor AP, in the same mobility
 * domain or pre-authentication reply for WPA3 SAE roaming.
 * This will be used in case of 11r FT.
 */
QDF_STATUS lim_process_auth_frame_no_session(struct mac_context *mac,
					     uint8_t *bd, void *body);

void lim_process_assoc_req_frame(struct mac_context *, uint8_t *, uint8_t, struct pe_session *);

/**
 * lim_fill_lim_assoc_ind_params() - Initialize lim association indication
 * @assoc_ind: PE association indication structure
 * @mac_ctx: Pointer to Global MAC structure
 * @sta_ds: station dph entry
 * @session_entry: PE session entry
 *
 * Return: true if lim assoc ind filled successfully
 */
bool lim_fill_lim_assoc_ind_params(
		tpLimMlmAssocInd assoc_ind,
		struct mac_context *mac_ctx,
		tpDphHashNode sta_ds,
		struct pe_session *session_entry);

/**
 * lim_sae_auth_cleanup_retry() - API to cleanup sae auth frmae stored
 * and deactivate the timer
 * @mac_ctx: Pointer to mac context
 * @vdev_id: vdev id
 *
 * Return: none
 */
void lim_sae_auth_cleanup_retry(struct mac_context *mac_ctx,
				uint8_t vdev_id);

/**
 * lim_fill_sme_assoc_ind_params() - Initialize association indication
 * @mac_ctx: Pointer to Global MAC structure
 * @assoc_ind: PE association indication structure
 * @sme_assoc_ind: SME association indication
 * @session_entry: PE session entry
 * @assoc_req_alloc: malloc memory for assoc_req or not
 *
 * Return: None
 */
void
lim_fill_sme_assoc_ind_params(
	struct mac_context *mac_ctx,
	tpLimMlmAssocInd assoc_ind, struct assoc_ind *sme_assoc_ind,
	struct pe_session *session_entry, bool assoc_req_alloc);

/**
 * lim_send_mlm_assoc_ind() - Sends assoc indication to SME
 * @mac_ctx: Global Mac context
 * @sta_ds: Station DPH hash entry
 * @session_entry: PE session entry
 *
 * This function sends either LIM_MLM_ASSOC_IND
 * or LIM_MLM_REASSOC_IND to SME.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_mlm_assoc_ind(struct mac_context *mac,
				  tpDphHashNode sta,
				  struct pe_session *pe_session);

#define ASSOC_FRAME_LEN 0
/**
 * lim_process_assoc_rsp_frame() - Processes assoc response
 * @mac_ctx:              Pointer to Global MAC structure
 * @rx_packet_info:       A pointer to Rx packet info structure
 * @reassoc_frame_length: Valid frame length if its a reassoc response frame
 * else 0
 * @sub_type: Indicates whether it is Association Response (=0) or
 *             Reassociation Response (=1) frame
 *
 * This function is called by lim_handle80211_frames() or
 * pe_roam_synch_callback() upon Re/Association Response frame reception or
 * roam synch indication with reassociation response frame is received.
 *
 * Return: None
 */
void lim_process_assoc_rsp_frame(struct mac_context *mac, uint8_t *rx_pkt_info,
				 uint32_t reassoc_frame_len, uint8_t subtype,
				 struct pe_session *pe_session);

void lim_process_disassoc_frame(struct mac_context *, uint8_t *, struct pe_session *);

/**
 * lim_get_nss_supported_by_ap() - finds out nss from AP's beacons
 * @vht_caps: VHT capabilities
 * @ht_caps: HT capabilities
 *
 * Return: nss advertised by AP in beacon
 */
uint8_t lim_get_nss_supported_by_ap(tDot11fIEVHTCaps *vht_caps,
				    tDot11fIEHTCaps *ht_caps,
				    tDot11fIEhe_cap *he_caps);
/*
 * lim_perform_disassoc() - Actual action taken after receiving disassoc
 * @mac_ctx: Global MAC context
 * @frame_rssi: RSSI of the frame
 * @rc: Reason code of the deauth
 * @pe_session: PE session entry pointer
 * @addr: BSSID from which the disassoc is received
 *
 * Return: None
 */
void lim_perform_disassoc(struct mac_context *mac_ctx, int32_t frame_rssi,
			  uint16_t rc, struct pe_session *pe_session,
			  tSirMacAddr addr);
/*
 * lim_disassoc_tdls_peers() - Disassoc action for tdls peers
 * @mac_ctx: Global MAC context
 * @pe_session: PE session entry pointer
 * @addr: BSSID from which the disassoc is received
 *
 * Return: None
 */
#ifdef FEATURE_WLAN_TDLS
void lim_disassoc_tdls_peers(struct mac_context *mac_ctx,
				    struct pe_session *pe_session, tSirMacAddr addr);
#else
static inline void lim_disassoc_tdls_peers(struct mac_context *mac_ctx,
				    struct pe_session *pe_session, tSirMacAddr addr)
{
}
#endif
void lim_process_deauth_frame(struct mac_context *, uint8_t *, struct pe_session *);
/*
 * lim_perform_deauth() - Actual action taken after receiving deauth
 * @mac_ctx: Global MAC context
 * @pe_session: PE session entry pointer
 * @rc: Reason code of the deauth
 * @addr: BSSID from which the deauth is received
 * @frame_rssi: RSSI of the frame
 *
 * Return: None
 */
void lim_perform_deauth(struct mac_context *mac_ctx, struct pe_session *pe_session,
			uint16_t rc, tSirMacAddr addr, int32_t frame_rssi);
void lim_process_action_frame(struct mac_context *, uint8_t *, struct pe_session *);
void lim_process_action_frame_no_session(struct mac_context *mac, uint8_t *pRxMetaInfo);

void lim_populate_mac_header(struct mac_context *, uint8_t *, uint8_t, uint8_t,
				      tSirMacAddr, tSirMacAddr);
QDF_STATUS lim_send_probe_req_mgmt_frame(struct mac_context *, tSirMacSSid *,
					 tSirMacAddr, qdf_freq_t, tSirMacAddr,
					 uint32_t, uint16_t *, uint8_t *);

/**
 * lim_send_probe_rsp_mgmt_frame() - Send probe response
 * @mac_ctx: Handle for mac context
 * @peer_macaddr: Mac address of requesting peer
 * @ssid: SSID for response
 * @pe_session: PE session id
 * @preq_p2pie: P2P IE in incoming probe request
 *
 * Builds and sends probe response frame to the requesting peer
 *
 * Return: void
 */
void
lim_send_probe_rsp_mgmt_frame(struct mac_context *mac_ctx,
			      tSirMacAddr peer_macaddr,
			      tpAniSSID ssid,
			      struct pe_session *pe_session,
			      uint8_t preq_p2pie);

void lim_send_auth_mgmt_frame(struct mac_context *, tSirMacAuthFrameBody *, tSirMacAddr,
			      uint8_t, struct pe_session *);
void lim_send_assoc_req_mgmt_frame(struct mac_context *, tLimMlmAssocReq *, struct pe_session *);
#ifdef WLAN_FEATURE_HOST_ROAM
void lim_send_reassoc_req_with_ft_ies_mgmt_frame(struct mac_context *mac,
		tLimMlmReassocReq *pMlmReassocReq, struct pe_session *pe_session);
void lim_send_reassoc_req_mgmt_frame(struct mac_context *, tLimMlmReassocReq *,
				     struct pe_session *);
/**
 * lim_process_rx_scan_handler() -
 *	process the event for scan which is issued by LIM
 * @vdev: wlan objmgr vdev pointer
 * @event: scan event
 * @arg: global mac context pointer
 *
 * Return: void
 */
void lim_process_rx_scan_handler(struct wlan_objmgr_vdev *vdev,
				 struct scan_event *event, void *arg);
#else
static inline void lim_send_reassoc_req_with_ft_ies_mgmt_frame(
		struct mac_context *mac, tLimMlmReassocReq *pMlmReassocReq,
		struct pe_session *pe_session)
{}
static inline void lim_send_reassoc_req_mgmt_frame(struct mac_context *mac_ctx,
		tLimMlmReassocReq *reassoc_req, struct pe_session *pe_session)
{}
static inline void lim_process_rx_scan_handler(struct wlan_objmgr_vdev *vdev,
				 struct scan_event *event, void *arg)
{}
#endif
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 * lim_process_set_he_bss_color() - process the set he bss color request
 *
 * @mac_ctx: global mac context pointer
 * @msg_buf: message buffer pointer
 *
 * Return: void
 */
void lim_process_set_he_bss_color(struct mac_context *mac_ctx, uint32_t *msg_buf);

/**
 * lim_process_obss_color_collision_info() - Process the obss color collision
 *  request.
 * @mac_ctx: global mac context pointer
 * @msg_buf: message buffer pointer
 *
 * Return: void
 */
void lim_process_obss_color_collision_info(struct mac_context *mac_ctx,
					   uint32_t *msg_buf);

/**
 * lim_send_obss_color_collision_cfg() - Send obss color collision cfg.
 * @mac_ctx: global mac context pointer
 * @session: Pointer to session
 * @event_type: obss color collision detection type
 *
 * Return: void
 */
void lim_send_obss_color_collision_cfg(struct mac_context *mac_ctx,
				       struct pe_session *session,
				       enum wmi_obss_color_collision_evt_type
				       event_type);
#else
static inline void lim_process_set_he_bss_color(struct mac_context *mac_ctx,
		uint32_t *msg_buf)
{}
static inline void lim_process_obss_color_collision_info(struct mac_context *mac_ctx,
							 uint32_t *msg_buf)
{}
static inline void lim_send_obss_color_collision_cfg(struct mac_context *mac_ctx,
			struct pe_session *session,
			enum wmi_obss_color_collision_evt_type event_type)
{}
#endif
void lim_send_delts_req_action_frame(struct mac_context *mac, tSirMacAddr peer,
				     uint8_t wmmTspecPresent,
				     struct mac_ts_info * pTsinfo,
				     struct mac_tspec_ie * pTspecIe,
				     struct pe_session *pe_session);
void lim_send_addts_req_action_frame(struct mac_context *mac, tSirMacAddr peerMacAddr,
				     tSirAddtsReqInfo *addts, struct pe_session *);

#ifdef WLAN_FEATURE_MSCS
/**
 * lim_send_mscs_req_action_frame() - Send mscs req
 * @mac_ctx: Handle for mac context
 * @peer_mac: Mac address of requesting peer
 * @mscs_req: mscs request buffer
 * @pe_session: PE session id.
 *
 * Builds and sends mscs action frame to the peer.
 *
 * Return: void
 */
void lim_send_mscs_req_action_frame(struct mac_context *mac,
				    struct qdf_mac_addr peer_mac,
				    struct mscs_req_info *mscs_req,
				    struct pe_session *pe_session);
#endif

/**
 * lim_send_assoc_rsp_mgmt_frame() - Send assoc response
 * @mac_ctx: Handle for mac context
 * @status_code: Status code for assoc response frame
 * @aid: Association ID
 * @peer_addr: Mac address of requesting peer
 * @subtype: Assoc/Reassoc
 * @sta: Pointer to station node
 * @pe_session: PE session id.
 * @tx_complete: Need tx complete callback or not
 *
 * Builds and sends association response frame to the requesting peer.
 *
 * Return: void
 */
void
lim_send_assoc_rsp_mgmt_frame(
	struct mac_context *mac_ctx,
	uint16_t status_code, uint16_t aid, tSirMacAddr peer_addr,
	uint8_t subtype, tpDphHashNode sta, struct pe_session *pe_session,
	bool tx_complete);

void lim_send_disassoc_mgmt_frame(struct mac_context *, uint16_t, tSirMacAddr,
				  struct pe_session *, bool waitForAck);
void lim_send_deauth_mgmt_frame(struct mac_context *, uint16_t, tSirMacAddr, struct pe_session *,
				bool waitForAck);

/**
 * lim_process_mlm_update_hidden_ssid_rsp() - process hidden ssid response
 * @mac_ctx: global mac context
 * @vdev_id: vdev id
 *
 * Return: None
 */
void lim_process_mlm_update_hidden_ssid_rsp(struct mac_context *mac_ctx,
					    uint8_t vdev_id);

tSirResultCodes lim_mlm_add_bss(struct mac_context *, tLimMlmStartReq *,
				struct pe_session *pe_session);

QDF_STATUS lim_send_channel_switch_mgmt_frame(struct mac_context *, tSirMacAddr,
						 uint8_t, uint8_t, uint8_t,
						 struct pe_session *);

QDF_STATUS lim_send_extended_chan_switch_action_frame(struct mac_context *mac_ctx,
	tSirMacAddr peer, uint8_t mode, uint8_t new_op_class,
	uint8_t new_channel, uint8_t count, struct pe_session *session_entry);
QDF_STATUS lim_p2p_oper_chan_change_confirm_action_frame(
	struct mac_context *mac_ctx, tSirMacAddr peer,
	struct pe_session *session_entry);

QDF_STATUS lim_send_neighbor_report_request_frame(struct mac_context *,
						     tpSirMacNeighborReportReq,
						     tSirMacAddr, struct pe_session *);

/**
 * lim_send_link_report_action_frame() - Send link measurement report action
 * frame in response for a link measurement request received.
 * @mac: Pointer to Mac context
 * @link_report: Pointer to the sSirMacLinkReport struct
 * @peer: BSSID of the peer
 * @pe_session: Pointer to the pe_session
 *
 * Return: QDF_STATUS
 *
 */
QDF_STATUS
lim_send_link_report_action_frame(struct mac_context *mac,
				  tpSirMacLinkReport link_report,
				  tSirMacAddr peer,
				  struct pe_session *pe_session);

/**
 * lim_send_radio_measure_report_action_frame - Send RRM report action frame
 * @mac: pointer to global MAC context
 * @dialog_token: Dialog token to be used in the action frame
 * @num_report: number of reports in pRRMReport
 * @is_last_frame: is the current report last or more reports to follow
 * @pRRMReport: Pointer to the RRM report structure
 * @peer: MAC address of the peer
 * @pe_session: Pointer to the PE session entry
 *
 * Return: Ret Status
 */
QDF_STATUS
lim_send_radio_measure_report_action_frame(struct mac_context *mac,
				uint8_t dialog_token,
				uint8_t num_report,
				bool is_last_frame,
				tpSirMacRadioMeasureReport pRRMReport,
				tSirMacAddr peer,
				struct pe_session *pe_session);

#ifdef FEATURE_WLAN_TDLS
void lim_init_tdls_data(struct mac_context *, struct pe_session *);

/**
 * lim_process_sme_tdls_mgmt_send_req() - send out tdls management frames
 * @mac_ctx: global mac context
 * @msg: message buffer received from SME.
 *
 * Process Send Mgmt Request from SME and transmit to AP.
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise
 */
QDF_STATUS lim_process_sme_tdls_mgmt_send_req(struct mac_context *mac_ctx,
					      void *msg);

/**
 * lim_process_sme_tdls_add_sta_req() - process TDLS Add STA
 * @mac_ctx: global mac context
 * @msg: message buffer received from SME.
 *
 * Process TDLS Add Station request
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise
 */
QDF_STATUS lim_process_sme_tdls_add_sta_req(struct mac_context *mac,
					    void *msg);

/**
 * lim_process_sme_tdls_del_sta_req() - process TDLS Del STA
 * @mac_ctx: global mac context
 * @msg: message buffer received from SME.
 *
 * Process TDLS Delete Station request
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise
 */
QDF_STATUS lim_process_sme_tdls_del_sta_req(struct mac_context *mac,
					    void *msg);

void lim_send_sme_mgmt_tx_completion(struct mac_context *mac, uint32_t vdev_id,
				     uint32_t txCompleteStatus);
QDF_STATUS lim_delete_tdls_peers(struct mac_context *mac_ctx,
				    struct pe_session *session_entry);
QDF_STATUS lim_process_tdls_add_sta_rsp(struct mac_context *mac, void *msg, struct pe_session *);
void lim_process_tdls_del_sta_rsp(struct mac_context *mac_ctx,
				  struct scheduler_msg *lim_msg,
				  struct pe_session *session_entry);

/**
 * lim_update_tdls_state_in_fw() - Update TDLS state in FW
 *
 * @session_entry - PE sessions
 * @value  -value to be updated
 *
 *
 * Return: void
 */
void lim_update_tdls_set_state_for_fw(struct pe_session *session_entry,
				      bool value);
#else
static inline QDF_STATUS lim_delete_tdls_peers(struct mac_context *mac_ctx,
						struct pe_session *session_entry)
{
	return QDF_STATUS_SUCCESS;
}
static inline void lim_init_tdls_data(struct mac_context *mac,
					struct pe_session *pe_session)
{

}

static inline void lim_update_tdls_set_state_for_fw(struct pe_session
						    *session_entry, bool value)
{
}
#endif

/* Algorithms & Link Monitoring related functions */
/* / Function that handles heartbeat failure */
void lim_handle_heart_beat_failure(struct mac_context *, struct pe_session *);

/**
 * lim_tear_down_link_with_ap() - Tear down link with AP
 * @mac: mac context
 * @session_id: PE session id
 * @reason_code: Disconnect reason code as per emun wlan_reason_code
 * @trigger: Disconnect trigger as per enum eLimDisassocTrigger
 *
 * Function that triggers link tear down with AP upon HB failure
 *
 * Return: None
 */
void lim_tear_down_link_with_ap(struct mac_context *mac,
				uint8_t session_id,
				enum wlan_reason_code reason_code,
				enum eLimDisassocTrigger trigger);

/* / Function that defers the messages received */
uint32_t lim_defer_msg(struct mac_context *, struct scheduler_msg *);

#ifdef ANI_SUPPORT_11H
/* / Function that sends Measurement Report action frame */
QDF_STATUS lim_send_meas_report_frame(struct mac_context *, tpSirMacMeasReqActionFrame,
					 tSirMacAddr, struct pe_session *pe_session);

/* / Function that sends TPC Report action frame */
QDF_STATUS lim_send_tpc_report_frame(struct mac_context *, tpSirMacTpcReqActionFrame,
					tSirMacAddr, struct pe_session *pe_session);
#endif

/**
 * lim_handle_add_bss_rsp() - Handle add bss response
 * @mac_ctx: mac context
 * @add_bss_rsp: add bss rsp
 *
 * This function is called to handle all types of add bss rsp
 * It will free memory of add_bss_rsp in the end after rsp is handled.
 *
 * Return: None
 */
void lim_handle_add_bss_rsp(struct mac_context *mac_ctx,
			    struct add_bss_rsp *add_bss_rsp);

void lim_process_mlm_add_sta_rsp(struct mac_context *mac,
				struct scheduler_msg *limMsgQt,
				 struct pe_session *pe_session);
void lim_process_mlm_del_sta_rsp(struct mac_context *mac,
				 struct scheduler_msg *limMsgQ);

QDF_STATUS
lim_process_mlm_del_all_sta_rsp(struct vdev_mlme_obj *vdev_mlme,
				struct peer_delete_all_response *rsp);
/**
 * lim_process_mlm_del_bss_rsp () - API to process delete bss response
 * @mac: Pointer to Global MAC structure
 * @vdev_stop_rsp: pointer to vdev stop response
 * @pe_session: pointer to pe_session
 *
 * Return: None
 */
void lim_process_mlm_del_bss_rsp(struct mac_context *mac,
				 struct del_bss_resp *vdev_stop_rsp,
				 struct pe_session *pe_session);

void lim_process_sta_mlm_add_sta_rsp(struct mac_context *mac,
				     struct scheduler_msg *limMsgQ,
				     struct pe_session *pe_session);
void lim_process_sta_mlm_del_sta_rsp(struct mac_context *mac,
				     struct scheduler_msg *limMsgQ,
				     struct pe_session *pe_session);

/**
 * lim_process_sta_mlm_del_bss_rsp() - handle del bss response of STA
 * @mac: Pointer to Global MAC structure
 * @vdev_stop_rsp: pointer to vdev stop response
 * @pe_session: pointer to pe_session
 *
 * Return: none
 */
void lim_process_sta_mlm_del_bss_rsp(struct mac_context *mac,
				     struct del_bss_resp *vdev_stop_rsp,
				     struct pe_session *pe_session);

void lim_process_mlm_set_sta_key_rsp(struct mac_context *mac,
				     struct scheduler_msg *limMsgQ);
void lim_process_mlm_set_bss_key_rsp(struct mac_context *mac,
				     struct scheduler_msg *limMsgQ);

/* Function to process WMA_SWITCH_CHANNEL_RSP message */
void lim_process_switch_channel_rsp(struct mac_context *mac,
				    struct vdev_start_response *rsp);

/**
 * lim_sta_handle_connect_fail() - handle connect failure of STA
 * @param - join params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_sta_handle_connect_fail(join_params *param);

/**
 * lim_join_result_callback() - Callback to handle join rsp
 * @mac: Pointer to Global MAC structure
 * @vdev_id: vdev id
 *
 * This callback function is used to delete PE session
 * entry and send join response to sme.
 *
 * Return: None
 */
void lim_join_result_callback(struct mac_context *mac,
			      uint8_t vdev_id);

#ifdef WLAN_FEATURE_HOST_ROAM
QDF_STATUS lim_sta_reassoc_error_handler(struct reassoc_params *param);
#else
static inline
QDF_STATUS lim_sta_reassoc_error_handler(struct reassoc_params *param)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_11W
/* 11w send SA query request action frame */
QDF_STATUS lim_send_sa_query_request_frame(struct mac_context *mac, uint8_t *transId,
					      tSirMacAddr peer,
					      struct pe_session *pe_session);
/* 11w SA query request action frame handler */
QDF_STATUS lim_send_sa_query_response_frame(struct mac_context *mac,
					       uint8_t *transId, tSirMacAddr peer,
					       struct pe_session *pe_session);
#endif

/* Inline functions */

/**
 * lim_post_sme_message()
 *
 ***FUNCTION:
 * This function is called by limProcessMlmMessages(). In this
 * function MLM sub-module invokes MLM ind/cnf primitives.
 *
 ***LOGIC:
 * Initially MLM makes an SME function call to invoke MLM ind/cnf
 * primitive. In future this can be enhanced to 'post' messages to SME.
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param mac      Pointer to Global MAC structure
 * @param msgType   Indicates the MLM primitive message type
 * @param *msg_buf  A pointer to the MLM message buffer
 *
 * @return None
 */
static inline void
lim_post_sme_message(struct mac_context *mac, uint32_t msgType,
		     uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}

	msg.type = (uint16_t) msgType;
	msg.bodyptr = msg_buf;
	msg.bodyval = 0;
	if (msgType > eWNI_SME_MSG_TYPES_BEGIN) {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG, NO_SESSION,
				 msg.type));
		lim_process_sme_req_messages(mac, &msg);
	} else {
		lim_process_mlm_rsp_messages(mac, msgType, msg_buf);
	}
} /*** end lim_post_sme_message() ***/

/**
 * lim_post_mlm_message()
 *
 ***FUNCTION:
 * This function is called by limProcessSmeMessages(). In this
 * function SME invokes MLME primitives.
 *
 ***PARAMS:
 *
 ***LOGIC:
 * Initially SME makes an MLM function call to invoke MLM primitive.
 * In future this can be enhanced to 'post' messages to MLM.
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param mac      Pointer to Global MAC structure
 * @param msgType   Indicates the MLM primitive message type
 * @param *msg_buf  A pointer to the MLM message buffer
 *
 * @return None
 */
static inline void
lim_post_mlm_message(struct mac_context *mac, uint32_t msgType,
		     uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	msg.type = (uint16_t) msgType;
	msg.bodyptr = msg_buf;
	msg.bodyval = 0;
	MTRACE(mac_trace_msg_rx(mac, NO_SESSION, msg.type));
	lim_process_mlm_req_messages(mac, &msg);
} /*** end lim_post_mlm_message() ***/

/**
 * lim_get_ielen_from_bss_description()
 *
 ***FUNCTION:
 * This function is called in various places to get IE length
 * from struct bss_description structure
 * number being scanned.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param     pBssDescr
 * @return    Total IE length
 */

static inline uint16_t
lim_get_ielen_from_bss_description(struct bss_description *pBssDescr)
{
	uint16_t ielen;

	if (!pBssDescr)
		return 0;

	/*
	 * Length of BSS desription is without length of
	 * length itself and length of pointer
	 * that holds ieFields
	 *
	 * <------------sizeof(struct bss_description)-------------------->
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */

	ielen = (uint16_t)(pBssDescr->length + sizeof(pBssDescr->length) -
			   GET_FIELD_OFFSET(struct bss_description, ieFields));

	return ielen;
} /*** end lim_get_ielen_from_bss_description() ***/

/**
 * lim_send_beacon_ind() - send the beacon indication
 * @mac_ctx: pointer to mac structure
 * @session: pe session
 * @reason: beacon update reason
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
QDF_STATUS lim_send_beacon_ind(struct mac_context *mac_ctx, struct pe_session *session,
			       enum sir_bcn_update_reason reason);

void
lim_send_vdev_restart(struct mac_context *mac, struct pe_session *pe_session,
		      uint8_t sessionId);

void lim_wpspbc_close(struct mac_context *mac, struct pe_session *pe_session);

#define LIM_WPS_OVERLAP_TIMER_MS                 10000

void lim_process_disassoc_ack_timeout(struct mac_context *mac);
void lim_process_deauth_ack_timeout(struct mac_context *mac);
QDF_STATUS lim_send_disassoc_cnf(struct mac_context *mac);
QDF_STATUS lim_send_deauth_cnf(struct mac_context *mac);

/**
 * lim_disassoc_tx_complete_cnf() - callback to indicate Tx completion
 * @context: pointer to mac structure
 * @txCompleteSuccess: indicates tx success/failure
 * @params: tx completion params
 *
 * function will be invoked on receiving tx completion indication
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
QDF_STATUS lim_disassoc_tx_complete_cnf(void *context,
					uint32_t txCompleteSuccess,
					void *params);

/**
 * lim_deauth_tx_complete_cnf() - callback to indicate Tx completion
 * @context: pointer to mac structure
 * @txCompleteSuccess: indicates tx success/failure
 * @params: tx completion params
 *
 * function will be invoked on receiving tx completion indication
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
QDF_STATUS lim_deauth_tx_complete_cnf(void *context,
				      uint32_t txCompleteSuccess,
				      void *params);

void lim_send_sme_disassoc_deauth_ntf(struct mac_context *mac_ctx,
				QDF_STATUS status, uint32_t *ctx);

#ifdef FEATURE_WLAN_TDLS
QDF_STATUS lim_process_sme_del_all_tdls_peers(struct mac_context *p_mac,
						 uint32_t *msg_buf);
#else
static inline
QDF_STATUS lim_process_sme_del_all_tdls_peers(struct mac_context *p_mac,
						 uint32_t *msg_buf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * lim_send_bcn_rsp() - handle beacon send response
 * @mac_ctx Pointer to Global MAC structure
 * @rsp: beacon send response
 *
 * Return: None
 */
void lim_send_bcn_rsp(struct mac_context *mac_ctx, tpSendbeaconParams rsp);

/**
 * lim_add_roam_blacklist_ap() - handle the blacklist bssid list received from
 * firmware
 * @mac_ctx: Pointer to Global MAC structure
 * @list: roam blacklist ap list
 *
 * Return: None
 */
void lim_add_roam_blacklist_ap(struct mac_context *mac_ctx,
			       struct roam_blacklist_event *src_lst);

/**
 * lim_process_rx_channel_status_event() - processes
 * event WDA_RX_CHN_STATUS_EVENT
 * @mac_ctx Pointer to Global MAC structure
 * @buf: Received message info
 *
 * Return: None
 */
void lim_process_rx_channel_status_event(struct mac_context *mac_ctx, void *buf);

/* / Bit value data structure */
typedef enum sHalBitVal         /* For Bit operations */
{
	eHAL_CLEAR,
	eHAL_SET
} tHalBitVal;

/**
 * lim_send_addba_response_frame(): Send ADDBA response action frame to peer
 * @mac_ctx: mac context
 * @peer_mac: Peer MAC address
 * @tid: TID for which addba response is being sent
 * @session: PE session entry
 * @addba_extn_present: ADDBA extension present flag
 * @amsdu_support: amsdu in ampdu support
 * @is_wep: protected bit in fc
 * @calc_buff_size: Calculated buf size from peer and self capabilities
 *
 * This function is called when ADDBA request is successful. ADDBA response is
 * setup by calling addba_response_setup API and frame is then sent out OTA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_addba_response_frame(struct mac_context *mac_ctx,
					 tSirMacAddr peer_mac, uint16_t tid,
					 struct pe_session *session,
					 uint8_t addba_extn_present,
					 uint8_t amsdu_support, uint8_t is_wep,
					 uint16_t calc_buff_size);

/**
 * lim_send_delba_action_frame() - Send delba to peer
 * @mac_ctx: mac context
 * @vdev_id: vdev id
 * @peer_macaddr: Peer mac addr
 * @tid: Tid number
 * @reason_code: reason code
 *
 * Return: 0 for success, non-zero for failure
 */
QDF_STATUS lim_send_delba_action_frame(struct mac_context *mac_ctx,
				       uint8_t vdev_id,
				       uint8_t *peer_macaddr, uint8_t tid,
				       uint8_t reason_code);

/**
 * lim_process_join_failure_timeout() - This function is called to process
 * JoinFailureTimeout
 *
 * @mac_ctx: Pointer to Global MAC structure
 *
 * This function is called to process JoinFailureTimeout
 *
 * @Return None
 */
void lim_process_join_failure_timeout(struct mac_context *mac_ctx);

/**
 * lim_process_auth_failure_timeout() - This function is called to process Min
 * Channel Timeout during channel scan.
 *
 * @mac_ctx: Pointer to Global MAC structure
 *
 * This function is called to process Min Channel Timeout during channel scan.
 *
 * @Return: None
 */
void lim_process_auth_failure_timeout(struct mac_context *mac_ctx);

/**
 * lim_process_assoc_failure_timeout() - This function is called to process Min
 * Channel Timeout during channel scan.
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_type: Assoc or reassoc
 *
 * This function is called to process Min Channel Timeout during channel scan.
 *
 * @Return: None
 */
void lim_process_assoc_failure_timeout(struct mac_context *mac_ctx,
				       uint32_t msg_type);

/**
 * lim_process_sae_auth_timeout() - This function is called to process sae
 * auth timeout
 * @mac_ctx: Pointer to Global MAC structure
 *
 * @Return: None
 */
void lim_process_sae_auth_timeout(struct mac_context *mac_ctx);

/**
 * lim_send_frame() - API to send frame
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @buf: Pointer to SAE auth retry frame
 * @buf_len: length of frame
 *
 * Return: None
 */
void lim_send_frame(struct mac_context *mac_ctx, uint8_t vdev_id, uint8_t *buf,
		    uint16_t buf_len);

/**
 * lim_send_mgmt_frame_tx() - Sends mgmt frame
 * @mac_ctx Pointer to Global MAC structure
 * @msg: Received message info
 *
 * Return: None
 */
void lim_send_mgmt_frame_tx(struct mac_context *mac_ctx,
		struct scheduler_msg *msg);

/**
 * lim_send_csa_restart_req() - send csa restart req
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 *
 * Return: None
 */
void lim_send_csa_restart_req(struct mac_context *mac_ctx, uint8_t vdev_id);

/**
 * lim_continue_sta_csa_req() - continue with CSA req after HW mode change
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 *
 * Return: None
 */
void lim_continue_sta_csa_req(struct mac_context *mac_ctx, uint8_t vdev_id);

/**
 * lim_process_mlm_start_req() - process MLM_START_REQ message
 *
 * @mac_ctx: global MAC context
 * @mlm_start_req: Pointer to start req
 *
 * This function is called to process MLM_START_REQ message
 * from SME. MLME now waits for HAL to send WMA_ADD_BSS_RSP.
 *
 * Return: None
 */
void lim_process_mlm_start_req(struct mac_context *mac_ctx,
					  tLimMlmStartReq *mlm_start_req);

/**
 * lim_process_mlm_join_req() - process mlm join request.
 *
 * @mac_ctx:    Pointer to Global MAC structure
 * @msg:        Pointer to the MLM message buffer
 *
 * This function is called to process MLM_JOIN_REQ message
 * from SME. It does following:
 * 1) Initialize LIM, HAL, DPH
 * 2) Configure the BSS for which the JOIN REQ was received
 *   a) Send WMA_ADD_BSS_REQ to HAL -
 *   This will identify the BSS that we are interested in
 *   --AND--
 *   Add a STA entry for the AP (in a STA context)
 *   b) Wait for WMA_ADD_BSS_RSP
 *   c) Send WMA_ADD_STA_REQ to HAL
 *   This will add the "local STA" entry to the STA table
 * 3) Continue as before, i.e,
 *   a) Send a PROBE REQ
 *   b) Wait for PROBE RSP/BEACON containing the SSID that
 *   we are interested in
 *   c) Then start an AUTH seq
 *   d) Followed by the ASSOC seq
 *
 * @Return: None
 */
void lim_process_mlm_join_req(struct mac_context *mac_ctx,
			      tLimMlmJoinReq *mlm_join_req);

/**
 * lim_post_join_set_link_state_callback()- registered callback to perform post
 * peer creation operations
 * @mac: pointer to global mac structure
 * @callback_arg: registered callback argument
 * @status: peer creation status
 *
 * This is registered callback function during association to perform
 * post peer creation operation based on the peer creation status
 *
 * Return: none
 */
void
lim_post_join_set_link_state_callback(struct mac_context *mac, uint32_t vdev_id,
				      QDF_STATUS status);

/*
 * lim_process_mlm_deauth_req() - This function is called to process
 * MLM_DEAUTH_REQ message from SME
 *
 * @mac_ctx:      Pointer to Global MAC structure
 * @msg_buf:      A pointer to the MLM message buffer
 *
 * This function is called to process MLM_DEAUTH_REQ message from SME
 *
 * @Return: None
 */
void lim_process_mlm_deauth_req(struct mac_context *mac_ctx, uint32_t *msg_buf);

/**
 * lim_sta_mlme_vdev_disconnect_bss() - Disconnect from BSS
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes BSS disconnection
 *
 * Return: SUCCESS on successful completion of disconnection
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_sta_mlme_vdev_disconnect_bss(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data);

/**
 * lim_process_assoc_cleanup() - frees up resources used in function
 * lim_process_assoc_req_frame()
 * @mac_ctx: pointer to Global MAC structure
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sta_ds: station dph entry
 * @assoc_req_copied: boolean to indicate if assoc req was copied to tmp above
 *
 * Frees up resources used in function lim_process_assoc_req_frame
 *
 * Return: void
 */
void lim_process_assoc_cleanup(struct mac_context *mac_ctx,
			       struct pe_session *session,
			       tpSirAssocReq assoc_req,
			       tpDphHashNode sta_ds,
			       bool assoc_req_copied);

/**
 * lim_send_assoc_ind_to_sme() - Initialize PE data structures and send assoc
 *				 indication to SME.
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pe session entry
 * @sub_type: Indicates whether it is Association Request(=0) or Reassociation
 *            Request(=1) frame
 * @hdr: A pointer to the MAC header
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @akm_type: AKM type
 * @pmf_connection: flag indicating pmf connection
 * @assoc_req_copied: boolean to indicate if assoc req was copied to tmp above
 * @dup_entry: flag indicating if duplicate entry found
 * @force_1x1: flag to indicate if the STA nss needs to be downgraded to 1x1
 *
 * Return: void
 */
bool lim_send_assoc_ind_to_sme(struct mac_context *mac_ctx,
			       struct pe_session *session,
			       uint8_t sub_type,
			       tpSirMacMgmtHdr hdr,
			       tpSirAssocReq assoc_req,
			       enum ani_akm_type akm_type,
			       bool pmf_connection,
			       bool *assoc_req_copied,
			       bool dup_entry, bool force_1x1);

/**
 * lim_process_sta_add_bss_rsp_pre_assoc - Processes handoff request
 * @mac_ctx:  Pointer to mac context
 * @pAddBssParams: Bss params including rsp data
 * @session_entry: PE session handle
 * @status: Qdf status
 *
 * This function is called to process a WMA_ADD_BSS_RSP from HAL.
 * Upon receipt of this message from HAL if the state is pre assoc.
 *
 * Return: Null
 */
void lim_process_sta_add_bss_rsp_pre_assoc(struct mac_context *mac_ctx,
					   struct bss_params *add_bss_params,
					   struct pe_session *session_entry,
					   QDF_STATUS status);
#endif /* __LIM_TYPES_H */
