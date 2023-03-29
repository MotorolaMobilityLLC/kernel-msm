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
 * This file sir_api.h contains definitions exported by
 * Sirius software.
 * Author:        Chandra Modumudi
 * Date:          04/16/2002
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SIR_API_H
#define __SIR_API_H

/* legacy definition */
typedef void *tpAniSirGlobal;

struct mac_context;
#include "qdf_types.h"
#include "cds_regdomain.h"
#include "sir_types.h"
#include "sir_mac_prot_def.h"
#include "ani_system_defs.h"
#include "sir_params.h"
#include "cds_regdomain.h"
#include "wmi_unified.h"
#include "wmi_unified_param.h"
#include "ol_txrx_htt_api.h"
#include "wlan_reg_services_api.h"
#include <dot11f.h>
#include "wlan_policy_mgr_api.h"
#include "wlan_tdls_public_structs.h"
#include "qca_vendor.h"
#include "wlan_cp_stats_mc_defs.h"

#define OFFSET_OF(structType, fldName)   (&((structType *)0)->fldName)

/* / Max supported channel list */
#define SIR_MAX_SUPPORTED_CHANNEL_LIST      96
#define CFG_VALID_CHANNEL_LIST_LEN              100

#define SIR_MDIE_SIZE               3   /* MD ID(2 bytes), Capability(1 byte) */

#define SIR_MAX_ELEMENT_ID         255

#define SIR_BCN_REPORT_MAX_BSS_DESC       8

#define SIR_NUM_11B_RATES 4     /* 1,2,5.5,11 */
#define SIR_NUM_11A_RATES 8     /* 6,9,12,18,24,36,48,54 */

typedef uint8_t tSirIpv4Addr[QDF_IPV4_ADDR_SIZE];

#define SIR_VERSION_STRING_LEN 64
typedef uint8_t tSirVersionString[SIR_VERSION_STRING_LEN];

/* Periodic Tx pattern offload feature */
#define PERIODIC_TX_PTRN_MAX_SIZE 1536
#ifndef MAXNUM_PERIODIC_TX_PTRNS
#define MAXNUM_PERIODIC_TX_PTRNS 6
#endif

/* FW response timeout values in milli seconds */
#define SIR_PEER_ASSOC_TIMEOUT           (4000) /* 4 seconds */

#ifdef FEATURE_RUNTIME_PM
/* Add extra PMO_RESUME_TIMEOUT for runtime PM resume timeout */
#define SIR_PEER_CREATE_RESPONSE_TIMEOUT (4000 + PMO_RESUME_TIMEOUT)
#define SIR_DELETE_STA_TIMEOUT           (4000 + PMO_RESUME_TIMEOUT)
#define SIR_VDEV_PLCY_MGR_TIMEOUT        (2000 + PMO_RESUME_TIMEOUT)
#else
#define SIR_PEER_CREATE_RESPONSE_TIMEOUT (4000)
#define SIR_DELETE_STA_TIMEOUT           (4000) /* 4 seconds */
#define SIR_VDEV_PLCY_MGR_TIMEOUT        (2000)
#endif

/* This should not be greater than MAX_NUMBER_OF_CONC_CONNECTIONS */
#define MAX_VDEV_SUPPORTED                        4

#define MAX_POWER_DBG_ARGS_SUPPORTED 8
#define QOS_MAP_MAX_EX  21
#define QOS_MAP_RANGE_NUM 8
#define QOS_MAP_LEN_MIN (QOS_MAP_RANGE_NUM * 2)
#define QOS_MAP_LEN_MAX \
	(QOS_MAP_LEN_MIN + 2 * QOS_MAP_MAX_EX)
#define NUM_CHAINS_MAX  2

/* Maximum number of realms present in fils indication element */
#define SIR_MAX_REALM_COUNT 7
/* Realm length */
#define SIR_REALM_LEN 2
/* Cache ID length */
#define CACHE_ID_LEN 2

/* Maximum peer station number query one time */
#define MAX_PEER_STA 12

/* Maximum number of peers for SAP */
#ifndef SIR_SAP_MAX_NUM_PEERS
#define SIR_SAP_MAX_NUM_PEERS 32
#endif

#define SIR_KRK_KEY_LEN 16
#define SIR_BTK_KEY_LEN 32
#define SIR_KCK_KEY_LEN 16
#define KCK_192BIT_KEY_LEN 24
#define KCK_256BIT_KEY_LEN 32

#define SIR_KEK_KEY_LEN 16
#define SIR_KEK_KEY_LEN_FILS 64
#define KEK_256BIT_KEY_LEN 32

#define SIR_REPLAY_CTR_LEN 8
#define SIR_PMK_LEN  48
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define SIR_UAPSD_BITOFFSET_ACVO     0
#define SIR_UAPSD_BITOFFSET_ACVI     1
#define SIR_UAPSD_BITOFFSET_ACBK     2
#define SIR_UAPSD_BITOFFSET_ACBE     3

#define SIR_UAPSD_FLAG_ACVO     (1 << SIR_UAPSD_BITOFFSET_ACVO)
#define SIR_UAPSD_FLAG_ACVI     (1 << SIR_UAPSD_BITOFFSET_ACVI)
#define SIR_UAPSD_FLAG_ACBK     (1 << SIR_UAPSD_BITOFFSET_ACBK)
#define SIR_UAPSD_FLAG_ACBE     (1 << SIR_UAPSD_BITOFFSET_ACBE)
#define SIR_UAPSD_GET(ac, mask)      (((mask) & (SIR_UAPSD_FLAG_ ## ac)) >> SIR_UAPSD_BITOFFSET_ ## ac)

#endif

/* Maximum management packet data unit length */
#define MAX_MGMT_MPDU_LEN 2304

struct scheduler_msg;

/**
 * enum sir_roam_op_code - Operation to be done by the callback.
 * @SIR_ROAM_SYNCH_PROPAGATION: Propagate the new BSS info after roaming.
 * @SIR_ROAMING_DEREGISTER_STA: Deregister the old STA after roaming.
 * @SIR_ROAMING_START: Firmware started roaming operation
 * @SIR_ROAMING_ABORT: Firmware aborted roaming operation, still connected.
 * @SIR_ROAM_SYNCH_COMPLETE: Roam sync propagation is complete.
 * @SIR_ROAMING_INVOKE_FAIL: Firmware roaming failed.
 * @SIR_ROAMING_DEAUTH: Firmware indicates deauth.
 */
enum sir_roam_op_code {
	SIR_ROAM_SYNCH_PROPAGATION = 1,
	SIR_ROAMING_DEREGISTER_STA,
	SIR_ROAMING_START,
	SIR_ROAMING_ABORT,
	SIR_ROAM_SYNCH_COMPLETE,
	SIR_ROAM_SYNCH_NAPI_OFF,
	SIR_ROAMING_INVOKE_FAIL,
	SIR_ROAMING_DEAUTH,
};

/* Type declarations used by Firmware and Host software */

/* Scan type enum used in scan request */
typedef enum eSirScanType {
	eSIR_PASSIVE_SCAN,
	eSIR_ACTIVE_SCAN,
	eSIR_BEACON_TABLE,
} tSirScanType;

/* Rsn Capabilities structure */
struct rsn_caps {
	uint16_t PreAuthSupported:1;
	uint16_t NoPairwise:1;
	uint16_t PTKSAReplayCounter:2;
	uint16_t GTKSAReplayCounter:2;
	uint16_t MFPRequired:1;
	uint16_t MFPCapable:1;
	uint16_t Reserved:8;
};

/**
 * struct roam_scan_ch_resp - roam scan chan list response to userspace
 * @vdev_id: vdev id
 * @num_channels: number of roam scan channels
 * @command_resp: command response or async event
 * @chan_list: list of roam scan channels
 */
struct roam_scan_ch_resp {
	uint16_t vdev_id;
	uint16_t num_channels;
	uint32_t command_resp;
	uint32_t *chan_list;
};

/**
 * struct wlan_beacon_report - Beacon info to be send to userspace
 * @vdev_id: vdev id
 * @ssid: ssid present in beacon
 * @bssid: bssid present in beacon
 * @frequency: channel frequency in MHz
 * @beacon_interval: Interval between two consecutive beacons
 * @time_stamp: time stamp at which beacon received from AP
 * @boot_time: Boot time when beacon received
 */
struct wlan_beacon_report {
	uint8_t vdev_id;
	struct wlan_ssid ssid;
	struct qdf_mac_addr bssid;
	uint32_t frequency;
	uint16_t beacon_interval;
	qdf_time_t time_stamp;
	qdf_time_t boot_time;
};


/* / Result codes Firmware return to Host SW */
typedef enum eSirResultCodes {
	eSIR_SME_SUCCESS,
	eSIR_LOGE_EXCEPTION,
	eSIR_SME_INVALID_PARAMETERS = 500,
	eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,
	eSIR_SME_RESOURCES_UNAVAILABLE,
	/* Unable to find a BssDescription */
	eSIR_SME_SCAN_FAILED,
	/* matching requested scan criteria */
	eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED,
	eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE,
	eSIR_SME_REFUSED,
	eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA,
	eSIR_SME_JOIN_TIMEOUT_RESULT_CODE,
	eSIR_SME_AUTH_TIMEOUT_RESULT_CODE,
	eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE,
	eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE,
	eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED,
	eSIR_SME_AUTH_REFUSED,
	eSIR_SME_INVALID_WEP_DEFAULT_KEY,
	eSIR_SME_NO_KEY_MAPPING_KEY_FOR_PEER,
	eSIR_SME_ASSOC_REFUSED,
	eSIR_SME_REASSOC_REFUSED,
	/* Recvd Deauth while join/pre-auth */
	eSIR_SME_DEAUTH_WHILE_JOIN,
	eSIR_SME_STA_NOT_AUTHENTICATED,
	eSIR_SME_STA_NOT_ASSOCIATED,
	eSIR_SME_ALREADY_JOINED_A_BSS,
	/* Given in SME_SCAN_RSP msg */
	eSIR_SME_MORE_SCAN_RESULTS_FOLLOW,
	/* that more SME_SCAN_RSP */
	/* messages are following. */
	/* SME_SCAN_RSP message with */
	/* eSIR_SME_SUCCESS status */
	/* code is the last one. */
	/* Sent in SME_JOIN/REASSOC_RSP */
	eSIR_SME_INVALID_ASSOC_RSP_RXED,
	/* messages upon receiving */
	/* invalid Re/Assoc Rsp frame. */
	/* STOP BSS triggered by MIC failures: MAC software to
	 * disassoc all stations
	 */
	eSIR_SME_MIC_COUNTER_MEASURES,
	/* with MIC_FAILURE reason code and perform the stop bss operation */
	/* didn't get rsp from peer within timeout interval */
	eSIR_SME_ADDTS_RSP_TIMEOUT,
	/* didn't get success rsp from HAL */
	eSIR_SME_ADDTS_RSP_FAILED,
	/* failed to send ch switch act frm */
	eSIR_SME_CHANNEL_SWITCH_FAIL,
	eSIR_SME_INVALID_STATE,
	/* SIR_HAL_SIR_HAL_INIT_SCAN_RSP returned failed status */
	eSIR_SME_HAL_SCAN_INIT_FAILED,
	/* SIR_HAL_END_SCAN_RSP returned failed status */
	eSIR_SME_HAL_SCAN_END_FAILED,
	/* SIR_HAL_FINISH_SCAN_RSP returned failed status */
	eSIR_SME_HAL_SCAN_FINISH_FAILED,
	/* Failed to send a message to HAL */
	eSIR_SME_HAL_SEND_MESSAGE_FAIL,
	/* Failed to stop the bss */
	eSIR_SME_STOP_BSS_FAILURE,
	eSIR_SME_WOWL_ENTER_REQ_FAILED,
	eSIR_SME_WOWL_EXIT_REQ_FAILED,
	eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE,
	eSIR_SME_FT_REASSOC_FAILURE,
	eSIR_SME_SEND_ACTION_FAIL,
	eSIR_SME_DEAUTH_STATUS,
	eSIR_PNO_SCAN_SUCCESS,
	eSIR_SME_INVALID_SESSION,
	eSIR_SME_PEER_CREATE_FAILED,
	eSIR_DONOT_USE_RESULT_CODE = SIR_MAX_ENUM_SIZE
} tSirResultCodes;

#ifdef WLAN_FEATURE_FILS_SK
struct fils_join_rsp_params {
	uint8_t *fils_pmk;
	uint8_t fils_pmk_len;
	uint8_t fils_pmkid[PMKID_LEN];
	uint8_t kek[MAX_KEK_LEN];
	uint8_t kek_len;
	uint8_t tk[MAX_TK_LEN];
	uint8_t tk_len;
	uint8_t gtk_len;
	uint8_t gtk[MAX_GTK_LEN];
	struct qdf_mac_addr dst_mac;
	struct qdf_mac_addr src_mac;
	uint16_t hlp_data_len;
	uint8_t hlp_data[FILS_MAX_HLP_DATA_LEN];
};
#endif

#define RMENABLEDCAP_MAX_LEN 5

struct rrm_config_param {
	uint8_t rrm_enabled;
	bool sap_rrm_enabled;
	uint8_t max_randn_interval;
	uint8_t rm_capability[RMENABLEDCAP_MAX_LEN];
};

const char *lim_bss_type_to_string(const uint16_t bss_type);
/**
 * struct supported_rates - stores rates/MCS supported
 * @llbRates: 11b rates in unit of 500kbps
 * @llaRates: 11a rates in unit of 500kbps
 * @supportedMCSSet: supported basic MCS, 0-76 bits used, remaining reserved
 *                    bits 0-15 and 32 should be set.
 * @rxHighestDataRate: RX Highest Supported Data Rate defines the highest data
 *                      rate that the STA is able to receive, in unites of 1Mbps
 *                      This value is derived from "Supported MCS Set field"
 *                      inside the HT capability element.
 * @vhtRxMCSMap: Indicates the Maximum MCS(VHT) that can be received for each
 *                number of spacial streams
 * @vhtRxHighestDataRate: Indicate the highest VHT data rate that the STA is
 *                         able to receive
 * @vhtTxMCSMap: Indicates the Maximum MCS(VHT) that can be transmitted for
 *                each number of spacial streams
 * @vhtTxHighestDataRate: Indicate the highest VHT data rate that the STA is
 *                         able to transmit
 * @he_rx_mcs: Indicates the Maximum MCS(HE) that can be received for each
 *              number of spacial streams
 * @he_tx_mcs: Indicates the Maximum MCS(HE) that can be transmitted for each
 *              number of spacial streams
 */
struct supported_rates {
	uint16_t llbRates[SIR_NUM_11B_RATES];
	uint16_t llaRates[SIR_NUM_11A_RATES];
	uint8_t supportedMCSSet[SIR_MAC_MAX_SUPPORTED_MCS_SET];
	uint16_t rxHighestDataRate;
	uint16_t vhtRxMCSMap;
	uint16_t vhtRxHighestDataRate;
	uint16_t vhtTxMCSMap;
	uint16_t vhtTxHighestDataRate;
#ifdef WLAN_FEATURE_11AX
	uint16_t rx_he_mcs_map_lt_80;
	uint16_t tx_he_mcs_map_lt_80;
	uint16_t rx_he_mcs_map_160;
	uint16_t tx_he_mcs_map_160;
	uint16_t rx_he_mcs_map_80_80;
	uint16_t tx_he_mcs_map_80_80;
#endif
};

struct register_mgmt_frame {
	uint16_t messageType;
	uint16_t length;
	uint8_t sessionId;
	bool registerFrame;
	uint16_t frameType;
	uint16_t matchLen;
	uint8_t matchData[1];
};

/* / Generic type for sending a response message */
/* / with result code to host software */
typedef struct sSirSmeRsp {
	uint16_t messageType;   /* eWNI_SME_*_RSP */
	uint16_t length;
	uint8_t vdev_id;
	tSirResultCodes status_code;
	struct wlan_objmgr_psoc *psoc;
} tSirSmeRsp, *tpSirSmeRsp;

struct bss_description;
struct roam_offload_synch_ind;
struct roam_pmkid_req_event;

/**
 * typedef csr_roam_synch_fn_t - CSR roam synch callback routine pointer
 * @mac: Global MAC context
 * @roam_synch_data: Structure with roam synch parameters
 * @bss_desc_ptr: BSS descriptor pointer
 * @reason: Reason for calling the callback
 *
 * This type is for callbacks registered with WMA and used after roaming
 * in firmware. The call to this routine completes the roam synch
 * propagation at both CSR and HDD levels. The HDD level propagation
 * is achieved through the already defined callback for assoc completion
 * handler.
 *
 * Return: Success or Failure.
 */
typedef QDF_STATUS
(*csr_roam_synch_fn_t)(struct mac_context *mac,
		       struct roam_offload_synch_ind *roam_synch_data,
		       struct bss_description *bss_desc_ptr,
		       enum sir_roam_op_code reason);

/**
 * typedef pe_roam_synch_fn_t - PE roam synch callback routine pointer
 * @mac_ctx: Global MAC context
 * @roam_sync_ind_ptr: Structure with roam synch parameters
 * @bss_desc_ptr: bss_description pointer for new bss to which the firmware has
 * started roaming
 * @reason: Reason for calling the callback
 *
 * This type is for callbacks registered with WMA to complete the roam synch
 * propagation at PE level. It also fills the BSS descriptor, which will be
 * helpful to complete the roam synch propagation.
 *
 * Return: Success or Failure.
 */
typedef QDF_STATUS
(*pe_roam_synch_fn_t)(struct mac_context *mac_ctx,
		      struct roam_offload_synch_ind *roam_sync_ind_ptr,
		      struct bss_description *bss_desc_ptr,
		      enum sir_roam_op_code reason);

/**
 * typedef stop_roaming_fn_t - Stop roaming routine pointer
 * @mac_handle: Pointer to opaque mac handle
 * @session_id: Session Identifier
 * @reason: Reason for calling the callback
 * @requestor: Requestor for disabling roaming in driver
 *
 * This type is for callbacks registered with WMA to stop roaming on the given
 * session ID
 *
 * Return: Success or Failure.
 */
typedef QDF_STATUS
(*stop_roaming_fn_t)(mac_handle_t mac_handle,
		     uint8_t session_id, uint8_t reason,
		     enum wlan_cm_rso_control_requestor requestor);

/**
 * typedef csr_roam_pmkid_req_fn_t - pmkid generation fallback event pointer
 * @vdev_id: Vdev id
 * @bss_list: candidate AP bssid list
 *
 * This type is for callbacks registered with CSR to handle roam event from
 * firmware for pmkid generation fallback
 *
 * Return: Success or Failure.
 */
typedef QDF_STATUS
(*csr_roam_pmkid_req_fn_t)(uint8_t vdev_id,
			   struct roam_pmkid_req_event *bss_list);

/* / Definition for indicating all modules ready on STA */
struct sme_ready_req {
	uint16_t messageType;   /* eWNI_SME_SYS_READY_IND */
	uint16_t length;
	csr_roam_synch_fn_t csr_roam_synch_cb;
	QDF_STATUS (*csr_roam_auth_event_handle_cb)(struct mac_context *mac,
						    uint8_t vdev_id,
						    struct qdf_mac_addr bssid);
	pe_roam_synch_fn_t pe_roam_synch_cb;
	stop_roaming_fn_t stop_roaming_cb;
	QDF_STATUS (*sme_msg_cb)(struct mac_context *mac,
				 struct scheduler_msg *msg);
	QDF_STATUS (*pe_disconnect_cb) (struct mac_context *mac,
					uint8_t vdev_id,
					uint8_t *deauth_disassoc_frame,
					uint16_t deauth_disassoc_frame_len,
					uint16_t reason_code);
	csr_roam_pmkid_req_fn_t csr_roam_pmkid_req_cb;
};

/**
 * struct s_sir_set_hw_mode - Set HW mode request
 * @messageType: Message type
 * @length: Length of the message
 * @set_hw: Params containing the HW mode index and callback
 */
struct s_sir_set_hw_mode {
	uint16_t messageType;
	uint16_t length;
	struct policy_mgr_hw_mode set_hw;
};

/**
 * struct sir_set_dual_mac_cfg - Set Dual mac config request
 * @message_type: Message type
 * @length: Length of the message
 * @set_dual_mac: Params containing the dual mac config and callback
 */
struct sir_set_dual_mac_cfg {
	uint16_t message_type;
	uint16_t length;
	struct policy_mgr_dual_mac_config set_dual_mac;
};

/**
 * struct sir_antenna_mode_param - antenna mode param
 * @num_tx_chains: Number of TX chains
 * @num_rx_chains: Number of RX chains
 * @set_antenna_mode_resp: callback to set antenna mode command
 * @set_antenna_mode_ctx: callback context to set antenna mode command
 */
struct sir_antenna_mode_param {
	uint32_t num_tx_chains;
	uint32_t num_rx_chains;
	void *set_antenna_mode_resp;
	void *set_antenna_mode_ctx;
};

/**
 * struct sir_set_antenna_mode - Set antenna mode request
 * @message_type: Message type
 * @length: Length of the message
 * @set_antenna_mode: Params containing antenna mode params
 */
struct sir_set_antenna_mode {
	uint16_t message_type;
	uint16_t length;
	struct sir_antenna_mode_param set_antenna_mode;
};

/**
 * enum bss_type - Enum for BSS type used in scanning/joining etc.
 *
 * @eSIR_INFRASTRUCTURE_MODE: Infrastructure station
 * @eSIR_INFRA_AP_MODE: softAP mode
 * @eSIR_AUTO_MODE: Auto role
 * @eSIR_MONITOR_MODE: Monitor mode
 * @eSIR_NDI_MODE: NAN datapath mode
 */
enum bss_type {
	eSIR_INFRASTRUCTURE_MODE,
	eSIR_INFRA_AP_MODE,
	eSIR_AUTO_MODE,
	eSIR_MONITOR_MODE,
	eSIR_NDI_MODE,
	eSIR_DONOT_USE_BSS_TYPE = SIR_MAX_ENUM_SIZE
};

/* / Power Capability info used in 11H */
struct power_cap_info {
	uint8_t minTxPower;
	uint8_t maxTxPower;
};

/* / Supported Channel info used in 11H */
struct supported_channels {
	uint8_t numChnl;
	uint8_t channelList[SIR_MAX_SUPPORTED_CHANNEL_LIST];
};

typedef enum eSirNwType {
	eSIR_11A_NW_TYPE,
	eSIR_11B_NW_TYPE,
	eSIR_11G_NW_TYPE,
	eSIR_11N_NW_TYPE,
	eSIR_11AC_NW_TYPE,
	eSIR_11AX_NW_TYPE,
	eSIR_DONOT_USE_NW_TYPE = SIR_MAX_ENUM_SIZE
} tSirNwType;

/* HT configuration values */
struct ht_config {
	/* Enable/Disable receiving LDPC coded packets */
	uint32_t ht_rx_ldpc:1;
	/* Enable/Disable TX STBC */
	uint32_t ht_tx_stbc:1;
	/* Enable/Disable RX STBC */
	uint32_t ht_rx_stbc:2;
	/* Enable/Disable SGI */
	uint32_t ht_sgi20:1;
	uint32_t ht_sgi40:1;
	uint32_t unused:27;
};

/**
 * struct sir_vht_config - VHT capabilities
 * @max_mpdu_len: MPDU length
 * @supported_channel_widthset: channel width set
 * @ldpc_coding: LDPC coding capability
 * @shortgi80: short GI 80 support
 * @shortgi160and80plus80: short Gi 160 & 80+80 support
 * @tx_stbc; Tx STBC cap
 * @tx_stbc: Rx STBC cap
 * @su_beam_former: SU beam former cap
 * @su_beam_formee: SU beam formee cap
 * @csnof_beamformer_antSup: Antenna support for beamforming
 * @num_soundingdim: Sound dimensions
 * @mu_beam_former: MU beam former cap
 * @mu_beam_formee: MU beam formee cap
 * @vht_txops: TXOP power save
 * @htc_vhtcap: HTC VHT capability
 * @max_ampdu_lenexp: AMPDU length
 * @vht_link_adapt: VHT link adapatation capable
 * @rx_antpattern: Rx Antenna pattern
 * @tx_antpattern: Tx Antenna pattern
 */
struct sir_vht_config {
	uint32_t           max_mpdu_len:2;
	uint32_t supported_channel_widthset:2;
	uint32_t        ldpc_coding:1;
	uint32_t         shortgi80:1;
	uint32_t shortgi160and80plus80:1;
	uint32_t               tx_stbc:1;
	uint32_t               rx_stbc:3;
	uint32_t      su_beam_former:1;
	uint32_t      su_beam_formee:1;
	uint32_t csnof_beamformer_antSup:3;
	uint32_t       num_soundingdim:3;
	uint32_t      mu_beam_former:1;
	uint32_t      mu_beam_formee:1;
	uint32_t            vht_txops:1;
	uint32_t            htc_vhtcap:1;
	uint32_t       max_ampdu_lenexp:3;
	uint32_t        vht_link_adapt:2;
	uint32_t         rx_antpattern:1;
	uint32_t         tx_antpattern:1;
	uint32_t  extended_nss_bw_supp:2;
	uint8_t  max_nsts_total:2;
	uint8_t  vht_extended_nss_bw_cap:1;
};


struct add_ie_params {
	uint16_t probeRespDataLen;
	uint8_t *probeRespData_buff;
	uint16_t assocRespDataLen;
	uint8_t *assocRespData_buff;
	uint16_t probeRespBCNDataLen;
	uint8_t *probeRespBCNData_buff;
};

/* / Definition for kick starting BSS */
/* / ---> MAC */
/**
 * Usage of ssId, numSSID & ssIdList:
 * ---------------------------------
 * 1. ssId.length of zero indicates that Broadcast/Suppress SSID
 *    feature is enabled.
 * 2. If ssId.length is zero, MAC SW will advertise NULL SSID
 *    and interpret the SSID list from numSSID & ssIdList.
 * 3. If ssId.length is non-zero, MAC SW will advertise the SSID
 *    specified in the ssId field and it is expected that
 *    application will set numSSID to one (only one SSID present
 *    in the list) and SSID in the list is same as ssId field.
 * 4. Application will always set numSSID >= 1.
 */
/* ***** NOTE: Please make sure all codes are updated if inserting field into
 * this structure..********** */
struct start_bss_req {
	uint16_t messageType;   /* eWNI_SME_START_BSS_REQ */
	uint16_t length;
	uint8_t vdev_id;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr self_macaddr;
	uint16_t beaconInterval;
	uint8_t dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif
	enum bss_type bssType;
	tSirMacSSid ssId;
	uint32_t oper_ch_freq;
	ePhyChanBondState cbMode;
	uint8_t vht_channel_width;
	uint8_t center_freq_seg0;
	uint8_t center_freq_seg1;
	uint8_t sec_ch_offset;

	uint8_t privacy;
	uint8_t apUapsdEnable;
	uint8_t ssidHidden;
	bool fwdWPSPBCProbeReq;
	bool protEnabled;
	bool obssProtEnabled;
	uint16_t ht_capab;
	tAniAuthType authType;
	uint32_t dtimPeriod;
	uint8_t wps_state;
	enum QDF_OPMODE bssPersona;

	uint8_t txLdpcIniFeatureEnabled;

	tSirRSNie rsnIE;        /* RSN IE to be sent in */
	/* Beacon and Probe */
	/* Response frames */
	tSirNwType nwType;      /* Indicates 11a/b/g */
	tSirMacRateSet operationalRateSet;      /* Has 11a or 11b rates */
	tSirMacRateSet extendedRateSet; /* Has 11g rates */
	struct ht_config ht_config;
	struct sir_vht_config vht_config;
#ifdef WLAN_FEATURE_11W
	bool pmfCapable;
	bool pmfRequired;
#endif

	struct add_ie_params add_ie_params;

	bool obssEnabled;
	uint8_t sap_dot11mc;
	uint16_t beacon_tx_rate;
	bool vendor_vht_sap;
	uint32_t cac_duration_ms;
	uint32_t dfs_regdomain;

};

#define GET_IE_LEN_IN_BSS(lenInBss) (lenInBss + sizeof(lenInBss) - \
			    ((uintptr_t)OFFSET_OF(struct bss_description,\
						  ieFields)))

#define WSCIE_PROBE_RSP_LEN (317 + 2)

#ifdef WLAN_FEATURE_FILS_SK
/* struct fils_ind_elements: elements parsed from fils indication present
 * in beacon/probe resp
 * @realm_cnt: number of realm present
 * @realm: realms
 * @is_fils_sk_supported: if FILS SK supported
 * @is_cache_id_present: if cache id present
 * @cache_id: cache id
 */
struct fils_ind_elements {
	uint16_t realm_cnt;
	uint8_t realm[SIR_MAX_REALM_COUNT][SIR_REALM_LEN];
	bool is_fils_sk_supported;
	bool is_cache_id_present;
	uint8_t cache_id[CACHE_ID_LEN];
};
#endif

struct bss_description {
	/* offset of the ieFields from bssId. */
	uint16_t length;
	tSirMacAddr bssId;
	unsigned long scansystimensec;
	uint32_t timeStamp[2];
	uint16_t beaconInterval;
	uint16_t capabilityInfo;
	tSirNwType nwType;      /* Indicates 11a/b/g */
	int8_t rssi;
	int8_t rssi_raw;
	int8_t sinr;
	/* channel frequency what peer sent in beacon/probersp. */
	uint32_t chan_freq;
	/* Based on system time, not a relative time. */
	uint64_t received_time;
	uint32_t parentTSF;
	uint32_t startTSF[2];
	uint8_t mdiePresent;
	/* MDIE for 11r, picked from the beacons */
	uint8_t mdie[SIR_MDIE_SIZE];
#ifdef FEATURE_WLAN_ESE
	uint16_t QBSSLoad_present;
	uint16_t QBSSLoad_avail;
#endif
	/* whether it is from a probe rsp */
	uint8_t fProbeRsp;
	tSirMacSeqCtl seq_ctrl;
	uint32_t tsf_delta;
	struct scan_mbssid_info mbssid_info;
#ifdef WLAN_FEATURE_FILS_SK
	struct fils_ind_elements fils_info_element;
#endif
	uint32_t assoc_disallowed;
	uint32_t adaptive_11r_ap;
	uint32_t mbo_oce_enabled_ap;
#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
	uint32_t is_single_pmk;
#endif
	/* Please keep the structure 4 bytes aligned above the ieFields */
	uint32_t ieFields[1];
};

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
struct ht_profile {
	uint8_t dot11mode;
	uint8_t htCapability;
	uint8_t htSupportedChannelWidthSet;
	uint8_t htRecommendedTxWidthSet;
	ePhyChanBondState htSecondaryChannelOffset;
	uint8_t vhtCapability;
	uint8_t apCenterChan;
	uint8_t apChanWidth;
};
#endif
/* / Definition for response message to previously */
/* / issued start BSS request */
/* / MAC ---> */
struct start_bss_rsp {
	uint16_t messageType;   /* eWNI_SME_START_BSS_RSP */
	uint16_t length;
	uint8_t sessionId;
	tSirResultCodes status_code;
	enum bss_type bssType;    /* Add new type for WDS mode */
	uint16_t beaconInterval;        /* Beacon Interval for both type */
	uint32_t staId;         /* Station ID for Self */
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile ht_profile;
#endif
	struct bss_description bssDescription;      /* Peer BSS description */
};

struct report_channel_list {
	uint8_t num_channels;
	uint32_t chan_freq_lst[SIR_ESE_MAX_MEAS_IE_REQS];
};

#ifdef FEATURE_OEM_DATA_SUPPORT
struct oem_data_req {
	uint32_t data_len;
	uint8_t *data;
};

struct oem_data_rsp {
	uint32_t rsp_len;
	uint8_t *data;
};
#endif /* FEATURE_OEM_DATA_SUPPORT */

#ifdef FEATURE_WLAN_ESE
typedef struct ese_wmm_tspec_ie {
	uint16_t         traffic_type:1;
	uint16_t                 tsid:4;
	uint16_t            direction:2;
	uint16_t        access_policy:2;
	uint16_t          aggregation:1;
	uint16_t                  psb:1;
	uint16_t        user_priority:3;
	uint16_t       tsinfo_ack_pol:2;
	uint8_t          tsinfo_rsvd:7;
	uint8_t      burst_size_defn:1;
	uint16_t                 size:15;
	uint16_t                fixed:1;
	uint16_t            max_msdu_size;
	uint32_t            min_service_int;
	uint32_t            max_service_int;
	uint32_t            inactivity_int;
	uint32_t            suspension_int;
	uint32_t            service_start_time;
	uint32_t            min_data_rate;
	uint32_t            mean_data_rate;
	uint32_t            peak_data_rate;
	uint32_t            burst_size;
	uint32_t            delay_bound;
	uint32_t            min_phy_rate;
	uint16_t            surplus_bw_allowance;
	uint16_t            medium_time;
} qdf_packed ese_wmm_tspec_ie;

typedef struct sTspecInfo {
	uint8_t valid;
	struct mac_tspec_ie tspec;
} tTspecInfo;

#define SIR_ESE_MAX_TSPEC_IES   4
typedef struct sESETspecTspecInfo {
	uint8_t numTspecs;
	tTspecInfo tspec[SIR_ESE_MAX_TSPEC_IES];
} tESETspecInfo;

struct tsm_ie {
	uint8_t tsid;
	uint8_t state;
	uint16_t msmt_interval;
};

struct tsm_ie_ind {
	struct tsm_ie tsm_ie;
	uint8_t sessionId;
};

typedef struct sAniTrafStrmMetrics {
	uint16_t UplinkPktQueueDly;
	uint16_t UplinkPktQueueDlyHist[4];
	uint32_t UplinkPktTxDly;
	uint16_t UplinkPktLoss;
	uint16_t UplinkPktCount;
	uint8_t RoamingCount;
	uint16_t RoamingDly;
} tAniTrafStrmMetrics, *tpAniTrafStrmMetrics;

typedef struct sAniGetTsmStatsReq {
	/* Common for all types are requests */
	uint16_t msgType;       /* message type is same as the request type */
	uint16_t msgLen;        /* length of the entire request */
	uint8_t tid;            /* traffic id */
	struct qdf_mac_addr bssId;
	void *tsmStatsCallback;
	void *pDevContext;      /* device context */
} tAniGetTsmStatsReq, *tpAniGetTsmStatsReq;

typedef struct sAniGetTsmStatsRsp {
	/* Common for all types are responses */
	uint16_t msgType;       /*
				 * message type is same as
				 * the request type
				 */
	uint16_t msgLen;        /*
				 * length of the entire request,
				 * includes the pStatsBuf length too
				 */
	uint8_t sessionId;
	uint32_t rc;            /* success/failure */
	struct qdf_mac_addr bssid; /* bssid to get the tsm stats for */
	tAniTrafStrmMetrics tsmMetrics;
	void *tsmStatsReq;      /* tsm stats request backup */
} tAniGetTsmStatsRsp, *tpAniGetTsmStatsRsp;

struct ese_bcn_report_bss_info {
	tBcnReportFields bcnReportFields;
	uint8_t ieLen;
	uint8_t *pBuf;
};

struct ese_bcn_report_rsp {
	uint16_t measurementToken;
	uint8_t flag;        /* Flag to report measurement done and more data */
	uint8_t numBss;
	struct ese_bcn_report_bss_info
				bcnRepBssInfo[SIR_BCN_REPORT_MAX_BSS_DESC];
};

#define TSRS_11AG_RATE_6MBPS   0xC
#define TSRS_11B_RATE_5_5MBPS  0xB

struct ese_tsrs_ie {
	uint8_t tsid;
	uint8_t rates[8];
};

struct ese_tsm_ie {
	uint8_t tsid;
	uint8_t state;
	uint16_t msmt_interval;
};

typedef struct sTSMStats {
	uint8_t tid;
	struct qdf_mac_addr bssid;
	tTrafStrmMetrics tsmMetrics;
} tTSMStats, *tpTSMStats;
typedef struct sEseTSMContext {
	uint8_t tid;
	struct ese_tsm_ie tsmInfo;
	tTrafStrmMetrics tsmMetrics;
} tEseTSMContext, *tpEseTSMContext;
typedef struct sEsePEContext {
	tEseTSMContext tsm;
} tEsePEContext, *tpEsePEContext;

#endif /* FEATURE_WLAN_ESE */

/* / Definition for join request */
/* / ---> MAC */
struct join_req {
	uint16_t messageType;   /* eWNI_SME_JOIN_REQ */
	uint16_t length;
	uint8_t vdev_id;
	tSirMacSSid ssId;
	tSirMacAddr self_mac_addr;        /* self Mac address */
	enum bss_type bsstype;    /* add new type for BT-AMP STA and AP Modules */
	uint8_t dot11mode;      /* to support BT-AMP */
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif
	enum QDF_OPMODE staPersona;       /* Persona */
	bool wps_registration;
	ePhyChanBondState cbMode;       /* Pass CB mode value in Join. */

	/*This contains the UAPSD Flag for all 4 AC
	 * B0: AC_VO UAPSD FLAG
	 * B1: AC_VI UAPSD FLAG
	 * B2: AC_BK UAPSD FLAG
	 * B3: AC_BE UASPD FLAG
	 */
	uint8_t uapsdPerAcBitmask;

	tSirMacRateSet operationalRateSet;      /* Has 11a or 11b rates */
	tSirMacRateSet extendedRateSet; /* Has 11g rates */
	tSirRSNie rsnIE;        /* RSN IE to be sent in */
	/* (Re) Association Request */
#ifdef FEATURE_WLAN_ESE
	/* CCMK IE to be included as handler for join and reassoc is */
	tSirCCKMie cckmIE;
	/* the same. The join will never carry cckm, but will be set to */
	/* 0. */
#endif

	tSirAddie addIEScan;    /* Additional IE to be sent in */
	/* (unicast) Probe Request at the time of join */

	tSirAddie addIEAssoc;   /* Additional IE to be sent in */
	/* (Re) Association Request */

	tAniEdType UCEncryptionType;
	enum ani_akm_type akm;

	bool is11Rconnection;
	bool is_adaptive_11r_connection;
#ifdef FEATURE_WLAN_ESE
	bool isESEFeatureIniEnabled;
	bool isESEconnection;
	tESETspecInfo eseTspecInfo;
#endif

	bool isFastTransitionEnabled;
	bool isFastRoamIniFeatureEnabled;

	uint8_t txLdpcIniFeatureEnabled;
	struct ht_config ht_config;
	struct sir_vht_config vht_config;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_cap he_config;
#endif
	uint8_t enableVhtpAid;
	uint8_t enableVhtGid;
	uint8_t enableAmpduPs;
	uint8_t enableHtSmps;
	uint8_t htSmps;
	bool send_smps_action;
	bool he_with_wep_tkip;
	uint8_t max_amsdu_num;
	bool isWMEenabled;
	bool isQosEnabled;
	bool isOSENConnection;
	struct rrm_config_param rrm_config;
	bool spectrumMgtIndicator;
	struct power_cap_info powerCap;
	struct supported_channels supportedChannels;
	bool enable_bcast_probe_rsp;
	bool sae_pmk_cached;
	bool same_ctry_code;  /* If AP Country IE has same country code as */
	/* STA programmed country */
	uint8_t ap_power_type_6g;  /* AP power type for 6G (LPI, SP, or VLP) */
	/* Pls make this as last variable in struct */
	bool force_24ghz_in_ht20;
	bool force_rsne_override;
	bool supported_nss_1x1;
	uint8_t vdev_nss;
	uint8_t nss;
	bool nss_forced_1x1;
	bool enable_session_twt_support;
	struct bss_description bssDescription;
	/*
	 * WARNING: Pls make bssDescription as last variable in struct
	 * join_req as it has ieFields followed after this bss
	 * description. Adding a variable after this corrupts the ieFields
	 */
};

/* / Definition for response message to previously issued join request */
/* / MAC ---> */
struct join_rsp {
	uint16_t messageType;   /* eWNI_SME_JOIN_RSP */
	uint16_t length;
	uint8_t vdev_id;      /* Session ID */
	tSirResultCodes status_code;
	tAniAuthType authType;
	uint32_t vht_channel_width;
	/* It holds reasonCode when join fails due to deauth/disassoc frame.
	 * Otherwise it holds status code.
	 */
	uint16_t protStatusCode;
	uint16_t aid;
	uint32_t beaconLength;
	uint32_t assocReqLength;
	uint32_t assocRspLength;
	uint32_t parsedRicRspLen;
	uint8_t uapsd_mask;
#ifdef FEATURE_WLAN_ESE
	uint32_t tspecIeLen;
#endif
	uint32_t staId;         /* Station ID for peer */

	/*Timing measurement capability */
	uint8_t timingMeasCap;

#ifdef FEATURE_WLAN_TDLS
	/* TDLS prohibited and TDLS channel switch prohibited are set as
	 * per ExtCap IE in received assoc/re-assoc response from AP
	 */
	bool tdls_prohibited;
	bool tdls_chan_swit_prohibited;
#endif
	uint8_t nss;
	uint32_t max_rate_flags;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile ht_profile;
#endif
	bool supported_nss_1x1;
	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	tDot11fIEHTInfo ht_operation;
	tDot11fIEVHTOperation vht_operation;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_op he_operation;
#endif
	tDot11fIEhs20vendor_ie hs20vendor_ie;
	bool is_fils_connection;
	uint16_t fils_seq_num;
#ifdef WLAN_FEATURE_FILS_SK
	struct fils_join_rsp_params *fils_join_rsp;
#endif
	uint8_t frames[1];
};

struct oem_channel_info {
	uint32_t mhz;
	uint32_t band_center_freq1;
	uint32_t band_center_freq2;
	uint32_t info;
	uint32_t reg_info_1;
	uint32_t reg_info_2;
	uint8_t nss;
	uint32_t rate_flags;
	uint8_t sec_ch_offset;
	enum phy_ch_width ch_width;
};

enum sir_sme_phy_mode {
	SIR_SME_PHY_MODE_LEGACY = 0,
	SIR_SME_PHY_MODE_HT = 1,
	SIR_SME_PHY_MODE_VHT = 2
};

/* / Definition for Association indication from peer */
/* / MAC ---> */
struct assoc_ind {
	uint16_t messageType;   /* eWNI_SME_ASSOC_IND */
	uint16_t length;
	uint8_t sessionId;
	tSirMacAddr peerMacAddr;
	uint16_t aid;
	tSirMacAddr bssId;      /* Self BSSID */
	uint16_t staId;         /* Station ID for peer */
	tAniAuthType authType;
	enum ani_akm_type akm_type;
	tAniSSID ssId;          /* SSID used by STA to associate */
	tSirWAPIie wapiIE;      /* WAPI IE received from peer */
	tSirRSNie rsnIE;        /* RSN IE received from peer */
	/* Additional IE received from peer, which possibly include
	 * WSC IE and/or P2P IE
	 */
	tSirAddie addIE;

	/* powerCap & supportedChannels are present only when */
	/* spectrumMgtIndicator flag is set */
	bool spectrumMgtIndicator;
	struct power_cap_info powerCap;
	struct supported_channels supportedChannels;
	bool wmmEnabledSta; /* if present - STA is WMM enabled */
	bool reassocReq;
	/* Required for indicating the frames to upper layer */
	uint32_t beaconLength;
	uint8_t *beaconPtr;
	uint32_t assocReqLength;
	uint8_t *assocReqPtr;

	/* Timing measurement capability */
	uint8_t timingMeasCap;
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
	/* Extended CSA capability of station */
	uint8_t ecsa_capable;
	tDot11fIEHTCaps HTCaps;
	tDot11fIEVHTCaps VHTCaps;
	bool he_caps_present;
	tSirMacCapabilityInfo capability_info;
	bool is_sae_authenticated;
	const uint8_t *owe_ie;
	uint32_t owe_ie_len;
	uint16_t owe_status;
	bool need_assoc_rsp_tx_cb;
};

/**
 * struct owe_assoc_ind - owe association indication
 * @node : List entry element
 * @assoc_ind: pointer to assoc ind
 */
struct owe_assoc_ind {
	qdf_list_node_t node;
	struct assoc_ind *assoc_ind;
};

/* / Definition for Association confirm */
/* / ---> MAC */
struct assoc_cnf {
	uint16_t messageType;   /* eWNI_SME_ASSOC_CNF */
	uint16_t length;
	tSirResultCodes status_code;
	struct qdf_mac_addr bssid;      /* Self BSSID */
	struct qdf_mac_addr peer_macaddr;
	uint16_t aid;
	enum wlan_status_code mac_status_code;
	uint8_t *owe_ie;
	uint32_t owe_ie_len;
	bool need_assoc_rsp_tx_cb;
};

/* / Enum definition for  Wireless medium status change codes */
typedef enum eSirSmeStatusChangeCode {
	eSIR_SME_DEAUTH_FROM_PEER,
	eSIR_SME_DISASSOC_FROM_PEER,
	eSIR_SME_LOST_LINK_WITH_PEER,
	eSIR_SME_CHANNEL_SWITCH,
	eSIR_SME_RADAR_DETECTED,
	eSIR_SME_AP_CAPS_CHANGED,
} tSirSmeStatusChangeCode;

struct new_bss_info {
	struct qdf_mac_addr bssId;
	uint32_t freq;
	uint8_t reserved;
	tSirMacSSid ssId;
};

struct ap_new_caps {
	uint16_t capabilityInfo;
	struct qdf_mac_addr bssId;
	tSirMacSSid ssId;
};

/**
 * Table below indicates what information is passed for each of
 * the Wireless Media status change notifications:
 *
 * Status Change code               Status change info
 * ----------------------------------------------------------------------
 * eSIR_SME_DEAUTH_FROM_PEER        Reason code received in DEAUTH frame
 * eSIR_SME_DISASSOC_FROM_PEER      Reason code received in DISASSOC frame
 * eSIR_SME_LOST_LINK_WITH_PEER     None
 * eSIR_SME_CHANNEL_SWITCH          New channel number
 * eSIR_SME_RADAR_DETECTED          Indicates that radar is detected
 * eSIR_SME_AP_CAPS_CHANGED         Indicates that capabilities of the AP
 *                                  that STA is currently associated with
 *                                  have changed.
 */

/* / Definition for Wireless medium status change notification */
struct wm_status_change_ntf {
	uint16_t messageType;   /* eWNI_SME_WM_STATUS_CHANGE_NTF */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	tSirSmeStatusChangeCode statusChangeCode;
	struct qdf_mac_addr bssid;      /* Self BSSID */
	union {
		/* eSIR_SME_DEAUTH_FROM_PEER */
		uint16_t deAuthReasonCode;
		/* eSIR_SME_DISASSOC_FROM_PEER */
		uint16_t disassocReasonCode;
		/* none for eSIR_SME_LOST_LINK_WITH_PEER */
		/* eSIR_SME_CHANNEL_SWITCH */
		uint32_t new_freq;
		/* none for eSIR_SME_RADAR_DETECTED */
		/* eSIR_SME_AP_CAPS_CHANGED */
		struct ap_new_caps apNewCaps;
	} statusChangeInfo;
};

/* Definition for Disassociation request */
struct disassoc_req {
	uint16_t messageType;   /* eWNI_SME_DISASSOC_REQ */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	struct qdf_mac_addr bssid;      /* Peer BSSID */
	struct qdf_mac_addr peer_macaddr;
	uint16_t reasonCode;
	/* This flag tells LIM whether to send the disassoc OTA or not */
	/* This will be set in while handing off from one AP to other */
	uint8_t doNotSendOverTheAir;
	bool process_ho_fail;
};

/* / Definition for Disassociation response */
struct disassoc_rsp {
	uint16_t messageType;   /* eWNI_SME_DISASSOC_RSP */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	tSirResultCodes status_code;
	struct qdf_mac_addr peer_macaddr;
	uint16_t staId;
};

/* / Definition for Disassociation indication from peer */
struct disassoc_ind {
	uint16_t messageType;   /* eWNI_SME_DISASSOC_IND */
	uint16_t length;
	uint8_t vdev_id;
	tSirResultCodes status_code;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peer_macaddr;
	uint16_t staId;
	uint32_t reasonCode;
	bool from_ap;
};

/* / Definition for Disassociation confirm */
/* / MAC ---> */
struct disassoc_cnf {
	uint16_t messageType;   /* eWNI_SME_DISASSOC_CNF */
	uint16_t length;
	uint8_t vdev_id;
	tSirResultCodes status_code;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peer_macaddr;
};

/**
 * struct sir_sme_discon_done_ind  - disconnect done indiaction
 * @message_type: msg type
 * @length: length of msg
 * @session_id: session id of the indication
 * @reason_code: reason for disconnect indication
 * @peer_mac: peer mac
 */
struct sir_sme_discon_done_ind {
	uint16_t           message_type;
	uint16_t           length;
	uint8_t            session_id;
	tSirResultCodes    reason_code;
	tSirMacAddr        peer_mac;
};

/* / Definition for Deauthetication request */
struct deauth_req {
	uint16_t messageType;   /* eWNI_SME_DEAUTH_REQ */
	uint16_t length;
	uint8_t vdev_id;      /* Session ID */
	struct qdf_mac_addr bssid;      /* AP BSSID */
	struct qdf_mac_addr peer_macaddr;
	uint16_t reasonCode;
};

/**
 * struct deauth_retry_params - deauth retry params
 * @peer_mac: peer mac
 * @reason_code: reason for disconnect indication
 * @retry_cnt: retry count
 */
struct deauth_retry_params {
	struct qdf_mac_addr peer_macaddr;
	uint16_t reason_code;
	uint8_t retry_cnt;
};

/* / Definition for Deauthetication response */
struct deauth_rsp {
	uint16_t messageType;   /* eWNI_SME_DEAUTH_RSP */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	tSirResultCodes status_code;
	struct qdf_mac_addr peer_macaddr;
};

/* / Definition for Deauthetication indication from peer */
struct deauth_ind {
	uint16_t messageType;   /* eWNI_SME_DEAUTH_IND */
	uint16_t length;
	uint8_t vdev_id;
	tSirResultCodes status_code;
	struct qdf_mac_addr bssid;      /* AP BSSID */
	struct qdf_mac_addr peer_macaddr;

	uint16_t staId;
	uint32_t reasonCode;
	int8_t rssi;
	bool from_ap;
};

/* / Definition for Deauthetication confirm */
struct deauth_cnf {
	uint16_t messageType;   /* eWNI_SME_DEAUTH_CNF */
	uint16_t length;
	uint8_t vdev_id;
	tSirResultCodes status_code;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peer_macaddr;
};

/* / Definition for stop BSS request message */
struct stop_bss_req {
	uint16_t messageType;   /* eWNI_SME_STOP_BSS_REQ */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	tSirResultCodes reasonCode;
	struct qdf_mac_addr bssid;      /* Self BSSID */
};

/* / Definition for Channel Switch indication for station */
/* / MAC ---> */
struct switch_channel_ind {
	uint16_t messageType;   /* eWNI_SME_SWITCH_CHL_IND */
	uint16_t length;
	uint8_t sessionId;
	uint32_t freq;
	struct ch_params chan_params;
	struct qdf_mac_addr bssid;      /* BSSID */
	QDF_STATUS status;
};

/* / Definition for MIC failure indication */
/* / MAC ---> */
/* / MAC reports this each time a MIC failure occures on Rx TKIP packet */
struct mic_failure_ind {
	uint16_t messageType;   /* eWNI_SME_MIC_FAILURE_IND */
	uint16_t length;
	uint8_t sessionId;
	struct qdf_mac_addr bssId;
	tSirMicFailureInfo info;
};

struct missed_beacon_ind {
	uint16_t messageType;   /* eWNI_SME_MISSED_BEACON_IND */
	uint16_t length;
	uint8_t bss_idx;
};

/* / Definition for Set Context request */
/* / ---> MAC */
struct set_context_req {
	uint16_t messageType;   /* eWNI_SME_SET_CONTEXT_REQ */
	uint16_t length;
	uint8_t vdev_id;      /* vdev ID */
	struct qdf_mac_addr peer_macaddr;
	struct qdf_mac_addr bssid;      /* BSSID */
	tSirKeyMaterial keyMaterial;
};

/* / Definition for Set Context response */
/* / MAC ---> */
struct set_context_rsp {
	uint16_t messageType;   /* eWNI_SME_SET_CONTEXT_RSP */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	tSirResultCodes status_code;
	struct qdf_mac_addr peer_macaddr;
};

typedef struct sAniGetSnrReq {
	/* Common for all types are requests */
	uint16_t msgType;       /* message type is same as the request type */
	uint16_t msgLen;        /* length of the entire request */
	uint8_t sessionId;
	void *snrCallback;
	void *pDevContext;      /* device context */
	int8_t snr;
} tAniGetSnrReq, *tpAniGetSnrReq;

/* generic country code change request MSG structure */
typedef struct sAniGenericChangeCountryCodeReq {
	uint16_t msgType;       /* message type is same as the request type */
	uint16_t msgLen;        /* length of the entire request */
	uint8_t countryCode[REG_ALPHA2_LEN + 1];  /* 3 char country code */
} tAniGenericChangeCountryCodeReq, *tpAniGenericChangeCountryCodeReq;

/**
 * struct sAniDHCPStopInd - DHCP Stop indication message
 * @msgType: message type is same as the request type
 * @msgLen: length of the entire request
 * @device_mode: Mode of the device(ex:STA, AP)
 * @adapterMacAddr: MAC address of the adapter
 * @peerMacAddr: MAC address of the connected peer
 */
typedef struct sAniDHCPStopInd {
	uint16_t msgType;
	uint16_t msgLen;
	uint8_t device_mode;
	struct qdf_mac_addr adapterMacAddr;
	struct qdf_mac_addr peerMacAddr;
} tAniDHCPInd, *tpAniDHCPInd;

/**********************PE Statistics end*************************/

typedef struct sSirP2PNoaAttr {
#ifdef ANI_BIG_BYTE_ENDIAN
	uint32_t index:8;
	uint32_t oppPsFlag:1;
	uint32_t ctWin:7;
	uint32_t rsvd1:16;
#else
	uint32_t rsvd1:16;
	uint32_t ctWin:7;
	uint32_t oppPsFlag:1;
	uint32_t index:8;
#endif

#ifdef ANI_BIG_BYTE_ENDIAN
	uint32_t uNoa1IntervalCnt:8;
	uint32_t rsvd2:24;
#else
	uint32_t rsvd2:24;
	uint32_t uNoa1IntervalCnt:8;
#endif
	uint32_t uNoa1Duration;
	uint32_t uNoa1Interval;
	uint32_t uNoa1StartTime;

#ifdef ANI_BIG_BYTE_ENDIAN
	uint32_t uNoa2IntervalCnt:8;
	uint32_t rsvd3:24;
#else
	uint32_t rsvd3:24;
	uint32_t uNoa2IntervalCnt:8;
#endif
	uint32_t uNoa2Duration;
	uint32_t uNoa2Interval;
	uint32_t uNoa2StartTime;
} tSirP2PNoaAttr, *tpSirP2PNoaAttr;

typedef struct sSirTclasInfo {
	tSirMacTclasIE tclas;
	uint8_t version;        /* applies only for classifier type ip */
	union {
		tSirMacTclasParamEthernet eth;
		tSirMacTclasParamIPv4 ipv4;
		tSirMacTclasParamIPv6 ipv6;
		tSirMacTclasParam8021dq t8021dq;
	} qdf_packed tclasParams;
} qdf_packed tSirTclasInfo;

typedef struct sSirAddtsReqInfo {
	uint8_t dialogToken;
	struct mac_tspec_ie tspec;

	uint8_t numTclas;       /* number of Tclas elements */
	tSirTclasInfo tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
	uint8_t tclasProc;
#if defined(FEATURE_WLAN_ESE)
	struct ese_tsrs_ie tsrsIE;
	uint8_t tsrsPresent:1;
#endif
	uint8_t wmeTspecPresent:1;
	uint8_t wsmTspecPresent:1;
	uint8_t lleTspecPresent:1;
	uint8_t tclasProcPresent:1;
} tSirAddtsReqInfo, *tpSirAddtsReqInfo;

typedef struct sSirAddtsRspInfo {
	uint8_t dialogToken;
	enum wlan_status_code status;
	tSirMacTsDelayIE delay;

	struct mac_tspec_ie tspec;
	uint8_t numTclas;       /* number of Tclas elements */
	tSirTclasInfo tclasInfo[SIR_MAC_TCLASIE_MAXNUM];
	uint8_t tclasProc;
	tSirMacScheduleIE schedule;
#ifdef FEATURE_WLAN_ESE
	struct ese_tsm_ie tsmIE;
	uint8_t tsmPresent:1;
#endif
	uint8_t wmeTspecPresent:1;
	uint8_t wsmTspecPresent:1;
	uint8_t lleTspecPresent:1;
	uint8_t tclasProcPresent:1;
	uint8_t schedulePresent:1;
} tSirAddtsRspInfo, *tpSirAddtsRspInfo;

/* / Add a tspec as defined */
typedef struct sSirAddtsReq {
	uint16_t messageType;   /* eWNI_SME_ADDTS_REQ */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	struct qdf_mac_addr bssid;      /* BSSID */
	uint32_t timeout;       /* in ms */
	uint8_t rspReqd;
	tSirAddtsReqInfo req;
} tSirAddtsReq, *tpSirAddtsReq;

typedef struct sSirAddtsRsp {
	uint16_t messageType;   /* eWNI_SME_ADDTS_RSP */
	uint16_t length;
	uint8_t sessionId;      /* sme sessionId  Added for BT-AMP support */
	uint32_t rc;            /* return code */
	tSirAddtsRspInfo rsp;
} tSirAddtsRsp, *tpSirAddtsRsp;

typedef struct sSirDeltsReq {
	uint16_t messageType;   /* eWNI_SME_DELTS_REQ */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	struct qdf_mac_addr bssid;      /* BSSID */
	uint16_t aid;           /* use 0 if macAddr is being specified */
	struct qdf_mac_addr macaddr;    /* only on AP to specify the STA */
	uint8_t rspReqd;
	struct delts_req_info req;
} tSirDeltsReq, *tpSirDeltsReq;

typedef struct sSirDeltsRsp {
	uint16_t messageType;   /* eWNI_SME_DELTS_RSP */
	uint16_t length;
	uint8_t sessionId;
	uint32_t rc;
	uint16_t aid;           /* use 0 if macAddr is being specified */
	struct qdf_mac_addr macaddr;    /* only on AP to specify the STA */
	struct delts_req_info rsp;
} tSirDeltsRsp, *tpSirDeltsRsp;

typedef struct sSirAggrQosReqInfo {
	uint16_t tspecIdx;
	tSirAddtsReqInfo aggrAddTsInfo[QCA_WLAN_AC_ALL];
} tSirAggrQosReqInfo, *tpSirAggrQosReqInfo;

typedef struct sSirAggrQosReq {
	uint16_t messageType;   /* eWNI_SME_ADDTS_REQ */
	uint16_t length;
	uint8_t sessionId;      /* Session ID */
	struct qdf_mac_addr bssid;      /* BSSID */
	uint32_t timeout;       /* in ms */
	uint8_t rspReqd;
	tSirAggrQosReqInfo aggrInfo;
} tSirAggrQosReq;

typedef struct sSirAggrQosRspInfo {
	uint16_t tspecIdx;
	tSirAddtsRspInfo aggrRsp[QCA_WLAN_AC_ALL];
} tSirAggrQosRspInfo, *tpSirAggrQosRspInfo;

typedef struct sSirAggrQosRsp {
	uint16_t messageType;
	uint16_t length;
	uint8_t sessionId;
	tSirAggrQosRspInfo aggrInfo;
} tSirAggrQosRsp, *tpSirAggrQosRsp;


struct qos_map_set {
	uint8_t present;
	uint8_t num_dscp_exceptions;
	uint8_t dscp_exceptions[QOS_MAP_MAX_EX][2];
	uint8_t dscp_range[QOS_MAP_RANGE_NUM][2];
};

typedef struct sSmeIbssPeerInd {
	uint16_t mesgType;
	uint16_t mesgLen;
	uint8_t sessionId;

	struct qdf_mac_addr peer_addr;

	/* Beacon will be appended for new Peer indication. */
} tSmeIbssPeerInd, *tpSmeIbssPeerInd;

/**
 * struct lim_channel_status
 * @channelfreq: Channel freq
 * @noise_floor: Noise Floor value
 * @rx_clear_count: rx clear count
 * @cycle_count: cycle count
 * @chan_tx_pwr_range: channel tx power per range in 0.5dBm steps
 * @chan_tx_pwr_throughput: channel tx power per throughput
 * @rx_frame_count: rx frame count (cumulative)
 * @bss_rx_cycle_count: BSS rx cycle count
 * @rx_11b_mode_data_duration: b-mode data rx time (units are microseconds)
 * @tx_frame_count: BSS tx cycle count
 * @mac_clk_mhz: sample frequency
 * @channel_id: channel index
 * @cmd_flags: indicate which stat event is this status coming from
 */
struct lim_channel_status {
	uint32_t    channelfreq;
	uint32_t    noise_floor;
	uint32_t    rx_clear_count;
	uint32_t    cycle_count;
	uint32_t    chan_tx_pwr_range;
	uint32_t    chan_tx_pwr_throughput;
	uint32_t    rx_frame_count;
	uint32_t    bss_rx_cycle_count;
	uint32_t    rx_11b_mode_data_duration;
	uint32_t    tx_frame_count;
	uint32_t    mac_clk_mhz;
	uint32_t    channel_id;
	uint32_t    cmd_flags;
};

/**
 * struct lim_scan_channel_status
 * @total_channel: total number of be scanned channel
 * @channel_status_list: channel status info store in this array
 */
struct lim_scan_channel_status {
	uint8_t total_channel;
	struct lim_channel_status
		 channel_status_list[SIR_MAX_SUPPORTED_CHANNEL_LIST];
};

typedef struct sSmeMaxAssocInd {
	uint16_t mesgType;      /* eWNI_SME_MAX_ASSOC_EXCEEDED */
	uint16_t mesgLen;
	uint8_t sessionId;
	/* the new peer that got rejected max assoc limit reached */
	struct qdf_mac_addr peer_mac;
} tSmeMaxAssocInd, *tpSmeMaxAssocInd;

#define SIR_MAX_NAME_SIZE 64
#define SIR_MAX_TEXT_SIZE 32

typedef struct sSirName {
	uint8_t num_name;
	uint8_t name[SIR_MAX_NAME_SIZE];
} tSirName;

typedef struct sSirText {
	uint8_t num_text;
	uint8_t text[SIR_MAX_TEXT_SIZE];
} tSirText;

#define SIR_WPS_PROBRSP_VER_PRESENT    0x00000001
#define SIR_WPS_PROBRSP_STATE_PRESENT    0x00000002
#define SIR_WPS_PROBRSP_APSETUPLOCK_PRESENT    0x00000004
#define SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT    0x00000008
#define SIR_WPS_PROBRSP_DEVICEPASSWORDID_PRESENT    0x00000010
#define SIR_WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT    0x00000020
#define SIR_WPS_PROBRSP_RESPONSETYPE_PRESENT    0x00000040
#define SIR_WPS_PROBRSP_UUIDE_PRESENT    0x00000080
#define SIR_WPS_PROBRSP_MANUFACTURE_PRESENT    0x00000100
#define SIR_WPS_PROBRSP_MODELNAME_PRESENT    0x00000200
#define SIR_WPS_PROBRSP_MODELNUMBER_PRESENT    0x00000400
#define SIR_WPS_PROBRSP_SERIALNUMBER_PRESENT    0x00000800
#define SIR_WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT    0x00001000
#define SIR_WPS_PROBRSP_DEVICENAME_PRESENT    0x00002000
#define SIR_WPS_PROBRSP_CONFIGMETHODS_PRESENT    0x00004000
#define SIR_WPS_PROBRSP_RF_BANDS_PRESENT    0x00008000

typedef struct sSirWPSProbeRspIE {
	uint32_t FieldPresent;
	uint32_t Version;       /* Version. 0x10 = version 1.0, 0x11 = etc. */
	uint32_t wpsState;      /* 1 = unconfigured, 2 = configured. */
	bool APSetupLocked;     /* Must be included if value is true */
	/*
	 * BOOL:  indicates if the user has recently activated a Registrar to
	 * add an Enrollee.
	 */
	bool SelectedRegistra;
	uint16_t DevicePasswordID;      /* Device Password ID */
	/* Selected Registrar config method */
	uint16_t SelectedRegistraCfgMethod;
	uint8_t ResponseType;   /* Response type */
	uint8_t UUID_E[16];     /* Unique identifier of the AP. */
	tSirName Manufacture;
	tSirText ModelName;
	tSirText ModelNumber;
	tSirText SerialNumber;
	/* Device Category ID: 1Computer, 2Input Device, ... */
	uint32_t PrimaryDeviceCategory;
	/* Vendor specific OUI for Device Sub Category */
	uint8_t PrimaryDeviceOUI[4];
	/*
	   Device Sub Category ID: 1-PC, 2-Server if Device Category ID
	 * is computer
	 */
	uint32_t DeviceSubCategory;
	tSirText DeviceName;
	uint16_t ConfigMethod;  /* Configuaration method */
	uint8_t RFBand;         /* RF bands available on the AP */
} tSirWPSProbeRspIE;

#define SIR_WPS_BEACON_VER_PRESENT    0x00000001
#define SIR_WPS_BEACON_STATE_PRESENT    0x00000002
#define SIR_WPS_BEACON_APSETUPLOCK_PRESENT    0x00000004
#define SIR_WPS_BEACON_SELECTEDREGISTRA_PRESENT    0x00000008
#define SIR_WPS_BEACON_DEVICEPASSWORDID_PRESENT    0x00000010
#define SIR_WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT    0x00000020
#define SIR_WPS_BEACON_UUIDE_PRESENT    0x00000080
#define SIR_WPS_BEACON_RF_BANDS_PRESENT    0x00000100
#define SIR_WPS_UUID_LEN 16

typedef struct sSirWPSBeaconIE {
	uint32_t FieldPresent;
	uint32_t Version;       /* Version. 0x10 = version 1.0, 0x11 = etc. */
	uint32_t wpsState;      /* 1 = unconfigured, 2 = configured. */
	bool APSetupLocked;     /* Must be included if value is true */
	/*
	 * BOOL:  indicates if the user has recently activated a Registrar to
	 * add an Enrollee.
	 */
	bool SelectedRegistra;
	uint16_t DevicePasswordID;      /* Device Password ID */
	/* Selected Registrar config method */
	uint16_t SelectedRegistraCfgMethod;
	uint8_t UUID_E[SIR_WPS_UUID_LEN];     /* Unique identifier of the AP. */
	uint8_t RFBand;         /* RF bands available on the AP */
} tSirWPSBeaconIE;

#define SIR_WPS_ASSOCRSP_VER_PRESENT    0x00000001
#define SIR_WPS_ASSOCRSP_RESPONSETYPE_PRESENT    0x00000002

typedef struct sSirWPSAssocRspIE {
	uint32_t FieldPresent;
	uint32_t Version;
	uint8_t ResposeType;
} tSirWPSAssocRspIE;

typedef struct sSirAPWPSIEs {
	tSirWPSProbeRspIE SirWPSProbeRspIE;     /*WPS Set Probe Respose IE */
	tSirWPSBeaconIE SirWPSBeaconIE; /*WPS Set Beacon IE */
	tSirWPSAssocRspIE SirWPSAssocRspIE;     /*WPS Set Assoc Response IE */
} tSirAPWPSIEs, *tpSiriAPWPSIEs;

struct update_config {
	uint16_t messageType;   /* eWNI_SME_UPDATE_CONFIG */
	uint16_t length;
	uint8_t vdev_id;
	uint16_t capab;
	uint32_t value;
};

/*
 * enum sir_update_session_param_type - session param type
 * @SIR_PARAM_SSID_HIDDEN: ssidHidden parameter
 */
enum sir_update_session_param_type {
	SIR_PARAM_SSID_HIDDEN,
};

/*
 * struct sir_update_session_param
 * @message_type: SME message type
 * @length: size of struct sir_update_session_param
 * @vdev_id: vdev ID
 * @param_type: parameter to be updated
 * @param_val: Parameter value to update
 */
struct sir_update_session_param {
	uint16_t message_type;
	uint16_t length;
	uint8_t vdev_id;
	uint32_t param_type;
	uint32_t param_val;
};

/**
 * struct sir_set_he_bss_color
 * @message_type: SME message type
 * @length: size of struct sir_set_he_bss_color
 * @vdev_id: vdev ID
 * @bss_color: bss color value
 */
struct sir_set_he_bss_color {
	uint16_t message_type;
	uint16_t length;
	uint8_t vdev_id;
	uint8_t bss_color;
};

/**
 * struct sir_create_session - Used for creating session in monitor mode
 * @type: SME host message type.
 * @msg_len: Length of the message.
 * @bss_id: bss_id for creating the session.
 */
struct sir_create_session {
	uint16_t type;
	uint16_t msg_len;
	uint8_t vdev_id;
	struct qdf_mac_addr bss_id;
};

/**
 * struct sir_delete_session - Used for deleting session in monitor mode
 * @type: SME host message type.
 * @msg_len: Length of the message.
 * @vdev_id: vdev id.
 */
struct sir_delete_session {
	uint16_t type;
	uint16_t msg_len;
	uint8_t vdev_id;
};

#ifdef QCA_HT_2040_COEX
struct set_ht2040_mode {
	uint16_t messageType;
	uint16_t length;
	uint8_t cbMode;
	bool obssEnabled;
	struct qdf_mac_addr bssid;
	uint8_t sessionId;      /* Session ID */
};
#endif

#define SIR_WPS_PBC_WALK_TIME   120     /* 120 Second */

typedef struct sSirWPSPBCSession {
	struct sSirWPSPBCSession *next;
	struct qdf_mac_addr addr;
	uint8_t uuid_e[SIR_WPS_UUID_LEN];
	uint32_t timestamp;
} tSirWPSPBCSession;

typedef struct sSirWPSPBCProbeReq {
	struct qdf_mac_addr peer_macaddr;
	uint16_t probeReqIELen;
	uint8_t probeReqIE[512];
} tSirWPSPBCProbeReq, *tpSirWPSPBCProbeReq;

/* probereq from peer, when wsc is enabled */
typedef struct sSirSmeProbeReqInd {
	uint16_t messageType;   /*  eWNI_SME_WPS_PBC_PROBE_REQ_IND */
	uint16_t length;
	uint8_t sessionId;
	struct qdf_mac_addr bssid;
	tSirWPSPBCProbeReq WPSPBCProbeReq;
} tSirSmeProbeReqInd, *tpSirSmeProbeReqInd;

#define SIR_ROAM_MAX_CHANNELS            80
#define SIR_ROAM_SCAN_MAX_PB_REQ_SIZE    450
/* Occupied channel list remains static */
#define CHANNEL_LIST_STATIC                   1
/* Occupied channel list can be dynamic */
#define CHANNEL_LIST_DYNAMIC                  2

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define SIR_ROAM_SCAN_PSK_SIZE    48
#define SIR_ROAM_R0KH_ID_MAX_LEN  48
#endif
/* SME -> HAL - This is the host offload request. */
#define SIR_IPV6_NS_OFFLOAD                         2
#define SIR_OFFLOAD_DISABLE                         0
#define SIR_OFFLOAD_ENABLE                          1

struct sir_host_offload_req {
	uint8_t offloadType;
	uint8_t enableOrDisable;
	uint32_t num_ns_offload_count;
	union {
		uint8_t hostIpv4Addr[QDF_IPV4_ADDR_SIZE];
		uint8_t hostIpv6Addr[QDF_IPV6_ADDR_SIZE];
	} params;
	struct qdf_mac_addr bssid;
};

/* Packet Types. */
#define SIR_KEEP_ALIVE_NULL_PKT              1
#define SIR_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2

/* Keep Alive request. */
struct keep_alive_req {
	uint8_t packetType;
	uint32_t timePeriod;
	tSirIpv4Addr hostIpv4Addr;
	tSirIpv4Addr destIpv4Addr;
	struct qdf_mac_addr dest_macaddr;
	struct qdf_mac_addr bssid;
	uint8_t sessionId;
};

/**
 * enum rxmgmt_flags - flags for received management frame.
 * @RXMGMT_FLAG_NONE: Default value to indicate no flags are set.
 * @RXMGMT_FLAG_EXTERNAL_AUTH: frame can be used for external authentication
 *			       by upper layers.
 */
enum rxmgmt_flags {
	RXMGMT_FLAG_NONE,
	RXMGMT_FLAG_EXTERNAL_AUTH = 1 << 1,
};

typedef struct sSirSmeMgmtFrameInd {
	uint16_t frame_len;
	uint32_t rx_freq;
	uint8_t sessionId;
	uint8_t frameType;
	int8_t rxRssi;
	enum rxmgmt_flags rx_flags;
	uint8_t frameBuf[1];    /* variable */
} tSirSmeMgmtFrameInd, *tpSirSmeMgmtFrameInd;

typedef void (*sir_mgmt_frame_ind_callback)(tSirSmeMgmtFrameInd *frame_ind);
/**
 * struct sir_sme_mgmt_frame_cb_req - Register a
 * management frame callback req
 *
 * @message_type: message id
 * @length: msg length
 * @callback: callback for management frame indication
 */
struct sir_sme_mgmt_frame_cb_req {
	uint16_t message_type;
	uint16_t length;
	sir_mgmt_frame_ind_callback callback;
};

#ifdef WLAN_FEATURE_11W
typedef struct sSirSmeUnprotMgmtFrameInd {
	uint8_t sessionId;
	uint8_t frameType;
	uint8_t frameLen;
	uint8_t frameBuf[1];    /* variable */
} tSirSmeUnprotMgmtFrameInd, *tpSirSmeUnprotMgmtFrameInd;
#endif

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT

typedef enum ext_wow_type {
	EXT_WOW_TYPE_APP_TYPE1, /* wow type: only enable wakeup for app type1 */
	EXT_WOW_TYPE_APP_TYPE2, /* wow type: only enable wakeup for app type2 */
	EXT_WOW_TYPE_APP_TYPE1_2,  /* wow type: enable wakeup for app type1&2 */
} EXT_WOW_TYPE;

typedef struct {
	uint8_t vdev_id;
	EXT_WOW_TYPE type;
	uint32_t wakeup_pin_num;
} tSirExtWoWParams, *tpSirExtWoWParams;

typedef struct {
	uint8_t vdev_id;
	struct qdf_mac_addr wakee_mac_addr;
	uint8_t identification_id[8];
	uint8_t password[16];
	uint32_t id_length;
	uint32_t pass_length;
} tSirAppType1Params, *tpSirAppType1Params;

typedef struct {
	uint8_t vdev_id;

	uint8_t rc4_key[16];
	uint32_t rc4_key_len;

	/** ip header parameter */
	uint32_t ip_id;         /* NC id */
	uint32_t ip_device_ip;  /* NC IP address */
	uint32_t ip_server_ip;  /* Push server IP address */

	/** tcp header parameter */
	uint16_t tcp_src_port;  /* NC TCP port */
	uint16_t tcp_dst_port;  /* Push server TCP port */
	uint32_t tcp_seq;
	uint32_t tcp_ack_seq;

	uint32_t keepalive_init;        /* Initial ping interval */
	uint32_t keepalive_min; /* Minimum ping interval */
	uint32_t keepalive_max; /* Maximum ping interval */
	uint32_t keepalive_inc; /* Increment of ping interval */

	struct qdf_mac_addr gateway_mac;
	uint32_t tcp_tx_timeout_val;
	uint32_t tcp_rx_timeout_val;
} tSirAppType2Params, *tpSirAppType2Params;
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
typedef struct {
	uint8_t acvo_uapsd:1;
	uint8_t acvi_uapsd:1;
	uint8_t acbk_uapsd:1;
	uint8_t acbe_uapsd:1;
	uint8_t reserved:4;
} tSirAcUapsd, *tpSirAcUapsd;
#endif

typedef struct {
	tSirMacSSid ssId;
	uint8_t currAPbssid[QDF_MAC_ADDR_SIZE];
	uint32_t authentication;
	uint8_t encryption;
	uint8_t mcencryption;
	tAniEdType gp_mgmt_cipher_suite;
	uint8_t ChannelCount;
	uint32_t chan_freq_cache[SIR_ROAM_MAX_CHANNELS];
#ifdef WLAN_FEATURE_11W
	bool mfp_enabled;
#endif
} tSirRoamNetworkType;

typedef enum {
	SIR_ROAMING_DFS_CHANNEL_DISABLED = 0,
	SIR_ROAMING_DFS_CHANNEL_ENABLED_NORMAL = 1,
	SIR_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE = 2
} eSirDFSRoamScanMode;

/**
 * struct roam_ext_params - Structure holding roaming parameters
 * @num_bssid_avoid_list:       The number of BSSID's that we should
 *                              avoid connecting to. It is like a
 *                              blacklist of BSSID's.
 * @num_ssid_allowed_list:      The number of SSID profiles that are
 *                              in the Whitelist. When roaming, we
 *                              consider the BSSID's with this SSID
 *                              also for roaming apart from the connected one's
 * @num_bssid_favored:          Number of BSSID's which have a preference over
 *                              others
 * @ssid_allowed_list:          Whitelist SSID's
 * @bssid_avoid_list:           Blacklist SSID's
 * @bssid_favored:              Favorable BSSID's
 * @bssid_favored_factor:       RSSI to be added to this BSSID to prefer it
 * @raise_rssi_thresh_5g:       The RSSI threshold below which the
 *                              raise_factor_5g (boost factor) should be
 *                              applied.
 * @drop_rssi_thresh_5g:        The RSSI threshold beyond which the
 *                              drop_factor_5g (penalty factor) should be
 *                              applied
 * @raise_rssi_type_5g:         Algorithm to apply the boost factor
 * @raise_factor_5g:            Boost factor
 * @drop_rssi_type_5g:          Algorithm to apply the penalty factor
 * @drop_factor_5g:             Penalty factor
 * @max_raise_rssi_5g:          Maximum amount of Boost that can added
 * @max_drop_rssi_5g:           Maximum amount of penalty that can be subtracted
 * @good_rssi_threshold:        The Lookup UP threshold beyond which roaming
 *                              scan should be performed.
 * @rssi_diff:                  RSSI difference for the AP to be better over the
 *                              current AP to avoid ping pong effects
 * @good_rssi_roam:             Lazy Roam
 * @rssi_reject_list:           RSSI reject list (APs rejected by OCE, BTM)
 * @bg_scan_bad_rssi_thresh:    Bad RSSI threshold to perform bg scan.
 * @bad_rssi_thresh_offset_2g:  Offset from Bad RSSI threshold for 2G to 5G Roam
 * @bg_scan_client_bitmap:      Bitmap to identify the client scans to snoop.
 * @roam_data_rssi_threshold_triggers:    Bad data RSSI threshold to roam
 * @roam_data_rssi_threshold:    Bad data RSSI threshold to roam
 * @rx_data_inactivity_time:    rx duration to check data RSSI
 *
 * This structure holds all the key parameters related to
 * initial connection and also roaming connections.
 * */
struct roam_ext_params {
	uint8_t num_bssid_avoid_list;
	uint8_t num_ssid_allowed_list;
	uint8_t num_bssid_favored;
	tSirMacSSid ssid_allowed_list[MAX_SSID_ALLOWED_LIST];
	struct qdf_mac_addr bssid_avoid_list[MAX_BSSID_AVOID_LIST];
	struct qdf_mac_addr bssid_favored[MAX_BSSID_FAVORED];
	uint8_t bssid_favored_factor[MAX_BSSID_FAVORED];
	int raise_rssi_thresh_5g;
	int drop_rssi_thresh_5g;
	uint8_t raise_rssi_type_5g;
	uint8_t raise_factor_5g;
	uint8_t drop_rssi_type_5g;
	uint8_t drop_factor_5g;
	int max_raise_rssi_5g;
	int max_drop_rssi_5g;
	int alert_rssi_threshold;
	int rssi_diff;
	int good_rssi_roam;
	int dense_rssi_thresh_offset;
	int dense_min_aps_cnt;
	int initial_dense_status;
	int traffic_threshold;
	uint8_t num_rssi_rejection_ap;
	struct reject_ap_config_params
			rssi_reject_bssid_list[MAX_RSSI_AVOID_BSSID_LIST];
	int8_t bg_scan_bad_rssi_thresh;
	uint8_t roam_bad_rssi_thresh_offset_2g;
	uint32_t bg_scan_client_bitmap;
	uint32_t roam_data_rssi_threshold_triggers;
	int32_t roam_data_rssi_threshold;
	uint32_t rx_data_inactivity_time;
};

/**
 * struct pmkid_mode_bits - Bit flags for PMKID usage in RSN IE
 * @fw_okc: Opportunistic key caching enable in firmware
 * @fw_pmksa_cache: PMKSA caching enable in firmware, remember previously
 *                  visited BSSID/PMK pairs
 */
struct pmkid_mode_bits {
	uint32_t fw_okc:1;
	uint32_t fw_pmksa_cache:1;
	uint32_t unused:30;
};

/**
 * struct lca_disallow_config_params - LCA[Last Connected AP]
 *                                     disallow config params
 * @disallow_duration: LCA AP disallowed duration
 * @rssi_channel_penalization: RSSI channel Penalization
 * @num_disallowed_aps: Maximum number of AP's in LCA list
 *
 */
struct lca_disallow_config_params {
    uint32_t disallow_duration;
    uint32_t rssi_channel_penalization;
    uint32_t num_disallowed_aps;
};

/**
 * struct mawc_params - Motion Aided Wireless Connectivity configuration
 * @mawc_enabled: Global configuration for MAWC (Roaming/PNO/ExtScan)
 * @mawc_roam_enabled: MAWC roaming enable/disable
 * @mawc_roam_traffic_threshold: Traffic threshold in kBps for MAWC roaming
 * @mawc_roam_ap_rssi_threshold: AP RSSI threshold for MAWC roaming
 * @mawc_roam_rssi_high_adjust: High Adjustment value for suppressing scan
 * @mawc_roam_rssi_low_adjust: Low Adjustment value for suppressing scan
 */
struct mawc_params {
	bool mawc_enabled;
	bool mawc_roam_enabled;
	uint32_t mawc_roam_traffic_threshold;
	int8_t mawc_roam_ap_rssi_threshold;
	uint8_t mawc_roam_rssi_high_adjust;
	uint8_t mawc_roam_rssi_low_adjust;
};

/**
 * struct roam_init_params - Firmware roam module initialization parameters
 * @vdev_id: vdev for which the roaming has to be enabled/disabled
 * @enable:  flag to init/deinit roam module
 */
struct roam_init_params {
	uint8_t vdev_id;
	uint8_t enable;
};

/**
 * struct roam_sync_timeout_timer_info - Info related to roam sync timer
 * @vdev_id: Vdev id for which host waiting roam sync ind from fw
 */
struct roam_sync_timeout_timer_info {
	uint8_t vdev_id;
};

struct roam_offload_scan_req {
	uint16_t message_type;
	uint16_t length;
	bool RoamScanOffloadEnabled;
	struct mawc_params mawc_roam_params;
	int8_t LookupThreshold;
	int8_t rssi_thresh_offset_5g;
	uint8_t delay_before_vdev_stop;
	uint8_t OpportunisticScanThresholdDiff;
	uint8_t RoamRescanRssiDiff;
	uint8_t RoamRssiDiff;
	uint8_t bg_rssi_threshold;
	struct rsn_caps rsn_caps;
	int32_t rssi_abs_thresh;
	uint8_t ChannelCacheType;
	uint8_t Command;
	uint8_t reason;
	uint16_t NeighborScanTimerPeriod;
	uint16_t neighbor_scan_min_timer_period;
	uint16_t NeighborScanChannelMinTime;
	uint16_t NeighborScanChannelMaxTime;
	uint16_t EmptyRefreshScanPeriod;
	bool IsESEAssoc;
	bool is_11r_assoc;
	uint8_t nProbes;
	uint16_t HomeAwayTime;
	tSirRoamNetworkType ConnectedNetwork;
	struct mobility_domain_info mdid;
	uint8_t sessionId;
	uint8_t RoamBmissFirstBcnt;
	uint8_t RoamBmissFinalBcnt;
	eSirDFSRoamScanMode allowDFSChannelRoam;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint8_t roam_offload_enabled;
	bool enable_self_bss_roam;
	uint8_t PSK_PMK[SIR_ROAM_SCAN_PSK_SIZE];
	uint32_t pmk_len;
	uint8_t Prefer5GHz;
	uint8_t RoamRssiCatGap;
	uint8_t Select5GHzMargin;
	uint8_t KRK[SIR_KRK_KEY_LEN];
	uint8_t BTK[SIR_BTK_KEY_LEN];
	uint32_t ReassocFailureTimeout;
	tSirAcUapsd AcUapsd;
	uint8_t R0KH_ID[SIR_ROAM_R0KH_ID_MAX_LEN];
	uint32_t R0KH_ID_Length;
	uint8_t RoamKeyMgmtOffloadEnabled;
	struct pmkid_mode_bits pmkid_modes;
	bool is_adaptive_11r_connection;
	bool is_sae_single_pmk;
	bool enable_ft_im_roaming;
	/* Idle/Disconnect roam parameters */
	struct wlan_roam_idle_params idle_roam_params;
	struct wlan_roam_disconnect_params disconnect_roam_params;
#endif
	struct roam_ext_params roam_params;
	struct wlan_roam_triggers roam_triggers;
	uint8_t  middle_of_roaming;
	uint32_t hi_rssi_scan_max_count;
	uint32_t hi_rssi_scan_rssi_delta;
	uint32_t hi_rssi_scan_delay;
	int32_t hi_rssi_scan_rssi_ub;
	uint8_t early_stop_scan_enable;
	int8_t early_stop_scan_min_threshold;
	int8_t early_stop_scan_max_threshold;
	enum scan_dwelltime_adaptive_mode roamscan_adaptive_dwell_mode;
	tSirAddie assoc_ie;
	struct lca_disallow_config_params lca_config_params;
	struct scoring_param score_params;
#ifdef WLAN_FEATURE_FILS_SK
	bool is_fils_connection;
#endif
	uint32_t btm_offload_config;
	uint32_t btm_solicited_timeout;
	uint32_t btm_max_attempt_cnt;
	uint32_t btm_sticky_time;
	uint32_t rct_validity_timer;
	uint32_t disassoc_timer_threshold;
	uint32_t btm_trig_min_candidate_score;
	struct wlan_roam_11k_offload_params offload_11k_params;
	uint32_t ho_delay_for_rx;
	uint32_t roam_preauth_retry_count;
	uint32_t roam_preauth_no_ack_timeout;
	uint32_t min_delay_btw_roam_scans;
	uint32_t roam_trigger_reason_bitmask;
	bool roam_force_rssi_trigger;
	/* bss load triggered roam related params */
	bool bss_load_trig_enabled;
	struct wlan_roam_bss_load_config bss_load_config;
	bool roaming_scan_policy;
	uint32_t roam_scan_inactivity_time;
	uint32_t roam_inactive_data_packet_count;
	uint32_t roam_scan_period_after_inactivity;
	uint32_t btm_query_bitmask;
	struct roam_trigger_min_rssi min_rssi_params[NUM_OF_ROAM_MIN_RSSI];
	struct roam_trigger_score_delta score_delta_param[NUM_OF_ROAM_TRIGGERS];
	uint32_t full_roam_scan_period;
};

struct roam_offload_scan_rsp {
	uint8_t sessionId;
	uint32_t reason;
};

/*---------------------------------------------------------------------------
   Packet Filtering Parameters
   ---------------------------------------------------------------------------*/
#define    SIR_MAX_FILTER_TEST_DATA_LEN       8
#define    SIR_MAX_FILTER_TEST_DATA_OFFSET  200
#define    SIR_MAX_NUM_MULTICAST_ADDRESS    240

/* */
/* Multicast Address List Parameters */
/* */
typedef struct sSirRcvFltMcAddrList {
	uint32_t ulMulticastAddrCnt;
	struct qdf_mac_addr multicastAddr[SIR_MAX_NUM_MULTICAST_ADDRESS];
	struct qdf_mac_addr self_macaddr;
	struct qdf_mac_addr bssid;
	uint8_t action;
} tSirRcvFltMcAddrList, *tpSirRcvFltMcAddrList;

/**
 * struct sir_wifi_start_log - Structure to store the params sent to start/
 * stop logging
 * @name:          Attribute which indicates the type of logging like per packet
 *                 statistics, connectivity etc.
 * @verbose_level: Verbose level which can be 0,1,2,3
 * @is_iwpriv_command: Set 1 for iwpriv command
 * @ini_triggered: triggered using ini
 * @user_triggered: triggered by user
 * @size: pktlog buffer size
 * @is_pktlog_buff_clear: clear the pktlog buffer
 */
struct sir_wifi_start_log {
	uint32_t ring_id;
	uint32_t verbose_level;
	uint32_t is_iwpriv_command;
	bool ini_triggered;
	uint8_t user_triggered;
	int size;
	bool is_pktlog_buff_clear;
};


/**
 * struct sir_pcl_list - Format of PCL
 * @pcl_list: List of preferred channels
 * @weight_list: Weights of the PCL
 * @pcl_len: Number of channels in the PCL
 */
struct sir_pcl_list {
	uint32_t pcl_len;
	uint8_t pcl_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
};

/**
 * struct sir_pcl_chan_weights - Params to get the valid weighed list
 * @pcl_list: Preferred channel list already sorted in the order of preference
 * @pcl_len: Length of the PCL
 * @saved_chan_list: Valid channel list updated as part of
 * WMA_UPDATE_CHAN_LIST_REQ
 * @saved_num_chan: Length of the valid channel list
 * @weighed_valid_list: Weights of the valid channel list. This will have one
 * to one mapping with valid_chan_list. FW expects channel order and size to be
 * as per the list provided in WMI_SCAN_CHAN_LIST_CMDID.
 * @weight_list: Weights assigned by policy manager
 */
struct sir_pcl_chan_weights {
	uint8_t pcl_list[NUM_CHANNELS];
	uint32_t pcl_len;
	uint8_t saved_chan_list[NUM_CHANNELS];
	uint32_t saved_num_chan;
	uint8_t weighed_valid_list[NUM_CHANNELS];
	uint8_t weight_list[NUM_CHANNELS];
};

/**
 * struct sir_hw_mode_params - HW mode params
 * @mac0_tx_ss: MAC0 Tx spatial stream
 * @mac0_rx_ss: MAC0 Rx spatial stream
 * @mac1_tx_ss: MAC1 Tx spatial stream
 * @mac1_rx_ss: MAC1 Rx spatial stream
 * @mac0_bw: MAC0 bandwidth
 * @mac1_bw: MAC1 bandwidth
 * @dbs_cap: DBS capabality
 * @agile_dfs_cap: Agile DFS capabality
 */
struct sir_hw_mode_params {
	uint8_t mac0_tx_ss;
	uint8_t mac0_rx_ss;
	uint8_t mac1_tx_ss;
	uint8_t mac1_rx_ss;
	uint8_t mac0_bw;
	uint8_t mac1_bw;
	uint8_t dbs_cap;
	uint8_t agile_dfs_cap;
	uint8_t sbs_cap;
};

/**
 * struct sir_set_hw_mode_resp - HW mode response
 * @status: Status
 * @cfgd_hw_mode_index: Configured HW mode index
 * @num_vdev_mac_entries: Number of vdev-mac id entries
 * @vdev_mac_map: vdev id-mac id map
 */
struct sir_set_hw_mode_resp {
	uint32_t status;
	uint32_t cfgd_hw_mode_index;
	uint32_t num_vdev_mac_entries;
	struct policy_mgr_vdev_mac_map vdev_mac_map[MAX_VDEV_SUPPORTED];
};

/**
 * struct sir_hw_mode_trans_ind - HW mode transition indication
 * @old_hw_mode_index: Index of old HW mode
 * @new_hw_mode_index: Index of new HW mode
 * @num_vdev_mac_entries: Number of vdev-mac id entries
 * @vdev_mac_map: vdev id-mac id map
 */
struct sir_hw_mode_trans_ind {
	uint32_t old_hw_mode_index;
	uint32_t new_hw_mode_index;
	uint32_t num_vdev_mac_entries;
	struct policy_mgr_vdev_mac_map vdev_mac_map[MAX_VDEV_SUPPORTED];
};

/**
 * struct sir_dual_mac_config_resp - Dual MAC config response
 * @status: Status of setting the dual mac configuration
 */
struct sir_dual_mac_config_resp {
	uint32_t status;
};

/**
 * enum set_antenna_mode_status - Status of set antenna mode
 * command
 * @SET_ANTENNA_MODE_STATUS_OK: command successful
 * @SET_ANTENNA_MODE_STATUS_EINVAL: invalid antenna mode
 * @SET_ANTENNA_MODE_STATUS_ECANCELED: mode change cancelled
 * @SET_ANTENNA_MODE_STATUS_ENOTSUP: mode not supported
 */
enum set_antenna_mode_status {
	SET_ANTENNA_MODE_STATUS_OK,
	SET_ANTENNA_MODE_STATUS_EINVAL,
	SET_ANTENNA_MODE_STATUS_ECANCELED,
	SET_ANTENNA_MODE_STATUS_ENOTSUP,
};

/**
 * struct sir_antenna_mode_resp - set antenna mode response
 * @status: Status of setting the antenna mode
 */
struct sir_antenna_mode_resp {
	enum set_antenna_mode_status status;
};

/* Reset AP Caps Changed */
typedef struct sSirResetAPCapsChange {
	uint16_t messageType;
	uint16_t length;
	struct qdf_mac_addr bssId;
} tSirResetAPCapsChange, *tpSirResetAPCapsChange;

/* / Definition for Candidate found indication from FW */
typedef struct sSirSmeCandidateFoundInd {
	uint16_t messageType;   /* eWNI_SME_CANDIDATE_FOUND_IND */
	uint16_t length;
	uint8_t sessionId;      /* Session Identifier */
} tSirSmeCandidateFoundInd, *tpSirSmeCandidateFoundInd;

#ifdef WLAN_FEATURE_11W
typedef struct sSirWlanExcludeUnencryptParam {
	bool excludeUnencrypt;
	struct qdf_mac_addr bssid;
} tSirWlanExcludeUnencryptParam, *tpSirWlanExcludeUnencryptParam;
#endif

typedef enum {
	P2P_SCAN_TYPE_SEARCH = 1,       /* P2P Search */
	P2P_SCAN_TYPE_LISTEN    /* P2P Listen */
} tSirP2pScanType;

typedef struct sAniHandoffReq {
	/* Common for all types are requests */
	uint16_t msgType;       /* message type is same as the request type */
	uint16_t msgLen;        /* length of the entire request */
	uint8_t sessionId;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint32_t ch_freq;
	uint8_t handoff_src;
} tAniHandoffReq, *tpAniHandoffReq;

/**
 * sir_scan_event_type - scan event types used in LIM
 * @SIR_SCAN_EVENT_STARTED - scan command accepted by FW
 * @SIR_SCAN_EVENT_COMPLETED - scan has been completed by FW
 * @SIR_SCAN_EVENT_BSS_CHANNEL - FW is going to move to HOME channel
 * @SIR_SCAN_EVENT_FOREIGN_CHANNEL - FW is going to move to FORIEGN channel
 * @SIR_SCAN_EVENT_DEQUEUED - scan request got dequeued
 * @SIR_SCAN_EVENT_PREEMPTED - preempted by other high priority scan
 * @SIR_SCAN_EVENT_START_FAILED - scan start failed
 * @SIR_SCAN_EVENT_RESTARTED - scan restarted
 * @SIR_SCAN_EVENT_MAX - max value for event type
*/
enum sir_scan_event_type {
	SIR_SCAN_EVENT_STARTED = 0x1,
	SIR_SCAN_EVENT_COMPLETED = 0x2,
	SIR_SCAN_EVENT_BSS_CHANNEL = 0x4,
	SIR_SCAN_EVENT_FOREIGN_CHANNEL = 0x8,
	SIR_SCAN_EVENT_DEQUEUED = 0x10,
	SIR_SCAN_EVENT_PREEMPTED = 0x20,
	SIR_SCAN_EVENT_START_FAILED = 0x40,
	SIR_SCAN_EVENT_RESTARTED = 0x80,
	SIR_SCAN_EVENT_MAX = 0x8000
};

typedef struct sSirScanOffloadEvent {
	enum sir_scan_event_type event;
	tSirResultCodes reasonCode;
	uint32_t chanFreq;
	uint32_t requestor;
	uint32_t scanId;
	tSirP2pScanType p2pScanType;
	uint8_t sessionId;
} tSirScanOffloadEvent, *tpSirScanOffloadEvent;

/**
 * struct sSirUpdateChanParam - channel parameters
 * @freq: Frequency of the channel
 * @pwr: power level
 * @dfsSet: is dfs supported or not
 * @half_rate: is the channel operating at 10MHz
 * @quarter_rate: is the channel operating at 5MHz
 * @nan_disabled: is NAN disabled on @freq
 */
typedef struct sSirUpdateChanParam {
	uint32_t freq;
	uint8_t pwr;
	bool dfsSet;
	bool half_rate;
	bool quarter_rate;
	bool nan_disabled;
} tSirUpdateChanParam, *tpSirUpdateChanParam;

typedef struct sSirUpdateChan {
	uint8_t numChan;
	uint8_t ht_en;
	uint8_t vht_en;
	uint8_t vht_24_en;
	bool he_en;
	tSirUpdateChanParam chanParam[1];
} tSirUpdateChanList, *tpSirUpdateChanList;

typedef enum eSirAddonPsReq {
	eSIR_ADDON_NOTHING,
	eSIR_ADDON_ENABLE_UAPSD,
	eSIR_ADDON_DISABLE_UAPSD
} tSirAddonPsReq;

#ifdef FEATURE_WLAN_CH_AVOID
typedef struct sSirChAvoidUpdateReq {
	uint32_t reserved_param;
} tSirChAvoidUpdateReq;
#endif /* FEATURE_WLAN_CH_AVOID */

struct link_speed_info {
	/* MAC Address for the peer */
	struct qdf_mac_addr peer_macaddr;
	uint32_t estLinkSpeed;  /* Linkspeed from firmware */
};

/**
 * struct sir_isolation_resp - isolation info related structure
 * @isolation_chain0: isolation value for chain 0
 * @isolation_chain1: isolation value for chain 1
 * @isolation_chain2: isolation value for chain 2
 * @isolation_chain3: isolation value for chain 3
 */
struct sir_isolation_resp {
	uint32_t isolation_chain0:8,
		 isolation_chain1:8,
		 isolation_chain2:8,
		 isolation_chain3:8;
};

typedef struct sSirAddPeriodicTxPtrn {
	/* MAC Address for the adapter */
	struct qdf_mac_addr mac_address;
	uint8_t ucPtrnId;       /* Pattern ID */
	uint16_t ucPtrnSize;    /* Pattern size */
	uint32_t usPtrnIntervalMs;      /* In msec */
	uint8_t ucPattern[PERIODIC_TX_PTRN_MAX_SIZE];   /* Pattern buffer */
} tSirAddPeriodicTxPtrn, *tpSirAddPeriodicTxPtrn;

typedef struct sSirDelPeriodicTxPtrn {
	/* MAC Address for the adapter */
	struct qdf_mac_addr mac_address;
	uint8_t ucPtrnId;       /* Pattern ID */
} tSirDelPeriodicTxPtrn, *tpSirDelPeriodicTxPtrn;

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
typedef struct {
	uint16_t mesgType;
	uint16_t mesgLen;
	bool status;
} tSirReadyToExtWoWInd, *tpSirReadyToExtWoWInd;
#endif
typedef struct sSirRateUpdateInd {
	uint8_t nss;            /* 0: 1x1, 1: 2x2 */
	struct qdf_mac_addr bssid;
	enum QDF_OPMODE dev_mode;
	int32_t bcastDataRate;  /* bcast rate unit Mbpsx10, -1:not used */

	/*
	 * 0 implies MCAST RA, positive value implies fixed rate,
	 * -1 implies ignore this param
	 */
	int32_t reliableMcastDataRate;  /* unit Mbpsx10 */

	/* TX flag to differentiate between HT20, HT40 etc */
	enum tx_rate_info reliableMcastDataRateTxFlag;

	/*
	 * MCAST(or BCAST) fixed data rate in 2.4 GHz, unit Mbpsx10,
	 * 0 implies ignore
	 */
	uint32_t mcastDataRate24GHz;

	/* TX flag to differentiate between HT20, HT40 etc */
	enum tx_rate_info mcastDataRate24GHzTxFlag;

	/*
	 * MCAST(or BCAST) fixed data rate in 5 GHz,
	 * unit Mbpsx10, 0 implies ignore
	 */
	uint32_t mcastDataRate5GHz;

	/* TX flag to differentiate between HT20, HT40 etc */
	enum tx_rate_info mcastDataRate5GHzTxFlag;

} tSirRateUpdateInd, *tpSirRateUpdateInd;

#define SIR_DFS_MAX_20M_SUB_CH 8
#define SIR_80MHZ_START_CENTER_CH_DIFF 6

typedef struct sSirSmeDfsChannelList {
	uint32_t nchannels;
	/* Ch num including bonded channels on which the RADAR is present */
	uint8_t channels[SIR_DFS_MAX_20M_SUB_CH];
} tSirSmeDfsChannelList, *tpSirSmeDfsChannelList;

typedef struct sSirSmeDfsEventInd {
	uint32_t sessionId;
	tSirSmeDfsChannelList chan_list;
	uint32_t dfs_radar_status;
	int use_nol;
} tSirSmeDfsEventInd, *tpSirSmeDfsEventInd;

typedef struct sSirChanChangeRequest {
	uint16_t messageType;
	uint16_t messageLen;
	uint32_t target_chan_freq;
	uint8_t sec_ch_offset;
	enum phy_ch_width ch_width;
	uint8_t center_freq_seg_0;
	uint8_t center_freq_seg_1;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint32_t dot11mode;
	tSirNwType nw_type;
	tSirMacRateSet operational_rateset;
	tSirMacRateSet extended_rateset;
	uint32_t cac_duration_ms;
	uint32_t dfs_regdomain;
} tSirChanChangeRequest, *tpSirChanChangeRequest;

typedef struct sSirChanChangeResponse {
	uint8_t sessionId;
	uint32_t new_op_freq;
	uint8_t channelChangeStatus;
} tSirChanChangeResponse, *tpSirChanChangeResponse;

typedef struct sSirStartBeaconIndication {
	uint16_t messageType;
	uint16_t messageLen;
	uint8_t beaconStartStatus;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
} tSirStartBeaconIndication, *tpSirStartBeaconIndication;

/* additional IE type */
typedef enum tUpdateIEsType {
	eUPDATE_IE_NONE,
	eUPDATE_IE_PROBE_BCN,
	eUPDATE_IE_PROBE_RESP,
	eUPDATE_IE_ASSOC_RESP,

	/* Add type above this line */
	/* this is used to reset all buffer */
	eUPDATE_IE_ALL,
	eUPDATE_IE_MAX
} eUpdateIEsType;

/* Modify particular IE in addition IE for prob resp Bcn */
typedef struct sSirModifyIE {
	struct qdf_mac_addr bssid;
	uint16_t vdev_id;
	bool notify;
	uint8_t ieID;
	uint8_t ieIDLen;        /*ie length as per spec */
	uint16_t ieBufferlength;
	uint8_t *pIEBuffer;
	int32_t oui_length;

} tSirModifyIE, *tpSirModifyIE;

struct send_add_ba_req {
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	struct addba_send_params param;
};

/* Message format for Update IE message sent to PE  */
typedef struct sSirModifyIEsInd {
	uint16_t msgType;
	uint16_t msgLen;
	tSirModifyIE modifyIE;
	eUpdateIEsType updateType;
} tSirModifyIEsInd, *tpSirModifyIEsInd;

/* Message format for Update IE message sent to PE  */
typedef struct sSirUpdateIE {
	struct qdf_mac_addr bssid;
	uint16_t vdev_id;
	bool append;
	bool notify;
	uint16_t ieBufferlength;
	uint8_t *pAdditionIEBuffer;
} tSirUpdateIE, *tpSirUpdateIE;

/* Message format for Update IE message sent to PE  */
typedef struct sSirUpdateIEsInd {
	uint16_t msgType;
	uint16_t msgLen;
	tSirUpdateIE updateIE;
	eUpdateIEsType updateType;
} tSirUpdateIEsInd, *tpSirUpdateIEsInd;

/* Message format for requesting channel switch announcement to lower layers */
typedef struct sSirDfsCsaIeRequest {
	uint16_t msgType;
	uint16_t msgLen;
	uint32_t target_chan_freq;
	uint8_t csaIeRequired;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	struct ch_params ch_params;
	uint8_t  ch_switch_beacon_cnt;
	uint8_t  ch_switch_mode;
	uint8_t  dfs_ch_switch_disable;
} tSirDfsCsaIeRequest, *tpSirDfsCsaIeRequest;

/* Indication from lower layer indicating the completion of first beacon send
 * after the beacon template update
 */
typedef struct sSirFirstBeaconTxCompleteInd {
	uint16_t messageType;   /* eWNI_SME_MISSED_BEACON_IND */
	uint16_t length;
	uint8_t bss_idx;
} tSirFirstBeaconTxCompleteInd, *tpSirFirstBeaconTxCompleteInd;

typedef struct sSirSmeCSAIeTxCompleteRsp {
	uint8_t sessionId;
	uint8_t chanSwIeTxStatus;
} tSirSmeCSAIeTxCompleteRsp, *tpSirSmeCSAIeTxCompleteRsp;

/* Thermal Mitigation*/

typedef struct {
	uint16_t minTempThreshold;
	uint16_t maxTempThreshold;
} t_thermal_level_info, *tp_thermal_level_info;

typedef enum {
	WLAN_WMA_THERMAL_LEVEL_0,
	WLAN_WMA_THERMAL_LEVEL_1,
	WLAN_WMA_THERMAL_LEVEL_2,
	WLAN_WMA_THERMAL_LEVEL_3,
	WLAN_WMA_MAX_THERMAL_LEVELS
} t_thermal_level;

#define WLAN_THROTTLE_DUTY_CYCLE_LEVEL_MAX (4)

typedef struct {
	/* Array of thermal levels */
	t_thermal_level_info thermalLevels[WLAN_WMA_MAX_THERMAL_LEVELS];
	uint8_t thermalCurrLevel;
	uint8_t thermalMgmtEnabled;
	uint32_t throttlePeriod;
	uint8_t throttle_duty_cycle_tbl[WLAN_THROTTLE_DUTY_CYCLE_LEVEL_MAX];
	enum thermal_mgmt_action_code thermal_action;
} t_thermal_mgmt, *tp_thermal_mgmt;

struct tx_power_limit {
	/* Thermal limits for 2g and 5g */
	uint32_t txPower2g;
	uint32_t txPower5g;
};

enum bad_peer_thresh_levels {
	WLAN_WMA_IEEE80211_B_LEVEL = 0,
	WLAN_WMA_IEEE80211_AG_LEVEL,
	WLAN_WMA_IEEE80211_N_LEVEL,
	WLAN_WMA_IEEE80211_AC_LEVEL,
	WLAN_WMA_IEEE80211_AX_LEVEL,
	WLAN_WMA_IEEE80211_MAX_LEVEL,
};

#define NUM_OF_RATE_THRESH_MAX    (4)
struct t_bad_peer_info {
	uint32_t cond;
	uint32_t delta;
	uint32_t percentage;
	uint32_t thresh[NUM_OF_RATE_THRESH_MAX];
	uint32_t txlimit;
};

struct t_bad_peer_txtcl_config {
	/* Array of thermal levels */
	struct t_bad_peer_info threshold[WLAN_WMA_IEEE80211_MAX_LEVEL];
	uint32_t enable;
	uint32_t period;
	uint32_t txq_limit;
	uint32_t tgt_backoff;
	uint32_t tgt_report_prd;
};

/* notify MODEM power state to FW */
typedef struct {
	uint32_t param;
} tSirModemPowerStateInd, *tpSirModemPowerStateInd;

#ifdef WLAN_FEATURE_STATS_EXT
typedef struct {
	uint32_t vdev_id;
	uint32_t event_data_len;
	uint8_t event_data[];
} tSirStatsExtEvent, *tpSirStatsExtEvent;
#endif

struct roam_offload_synch_ind {
	uint16_t messageType;   /*eWNI_SME_ROAM_OFFLOAD_SYNCH_IND */
	uint16_t length;
	uint16_t beaconProbeRespOffset;
	uint16_t beaconProbeRespLength;
	uint16_t reassocRespOffset;
	uint16_t reassocRespLength;
	uint16_t reassoc_req_offset;
	uint16_t reassoc_req_length;
	uint8_t isBeacon;
	uint8_t roamed_vdev_id;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr self_mac;
	int8_t txMgmtPower;
	uint32_t authStatus;
	uint8_t rssi;
	uint8_t roamReason;
	uint32_t chan_freq;
	uint8_t kck[KCK_256BIT_KEY_LEN];
	uint8_t kck_len;
	uint32_t kek_len;
	uint8_t kek[SIR_KEK_KEY_LEN_FILS];
	uint32_t   pmk_len;
	uint8_t    pmk[SIR_PMK_LEN];
	uint8_t    pmkid[PMKID_LEN];
	bool update_erp_next_seq_num;
	uint16_t next_erp_seq_num;
	uint8_t replay_ctr[SIR_REPLAY_CTR_LEN];
	void *add_bss_params;
	struct join_rsp *join_rsp;
	uint16_t aid;
	struct sir_hw_mode_trans_ind hw_mode_trans_ind;
	uint8_t nss;
	struct qdf_mac_addr dst_mac;
	struct qdf_mac_addr src_mac;
	uint16_t hlp_data_len;
	uint8_t hlp_data[FILS_MAX_HLP_DATA_LEN];
	bool is_ft_im_roam;
	enum wlan_phymode phy_mode; /*phy mode sent by fw */
};

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
struct handoff_failure_ind {
	uint8_t vdev_id;
	struct qdf_mac_addr bssid;
};

struct roam_offload_synch_fail {
	uint8_t session_id;
};

#endif

/**
 * struct sir_wisa_params - WISA Mode Parameters
 * @mode: WISA mode
 * @session_id: Session ID of vdev
 */
struct sir_wisa_params {
	bool mode;
	uint8_t vdev_id;
};

/**
 * typedef enum wifi_scan_flags - wifi scan flags
 * @WIFI_SCAN_FLAG_INTERRUPTED: Indicates that scan results are not complete
 *				because probes were not sent on some channels
 */
typedef enum {
	WIFI_SCAN_FLAG_INTERRUPTED = 1,
} wifi_scan_flags;

typedef enum {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,       /* 2.4 GHz */
	WIFI_BAND_A = 2,        /* 5 GHz without DFS */
	WIFI_BAND_ABG = 3,      /* 2.4 GHz + 5 GHz; no DFS */
	WIFI_BAND_A_DFS_ONLY = 4,       /* 5 GHz DFS only */
	/* 5 is reserved */
	WIFI_BAND_A_WITH_DFS = 6,       /* 5 GHz with DFS */
	WIFI_BAND_ABG_WITH_DFS = 7,     /* 2.4 GHz + 5 GHz with DFS */

	/* Keep it last */
	WIFI_BAND_MAX
} tWifiBand;

#ifdef FEATURE_WLAN_EXTSCAN

#define WLAN_EXTSCAN_MAX_CHANNELS                 36
#define WLAN_EXTSCAN_MAX_BUCKETS                  16
#define WLAN_EXTSCAN_MAX_SIGNIFICANT_CHANGE_APS   64

typedef enum {
	eSIR_EXTSCAN_INVALID,
	eSIR_EXTSCAN_START_RSP,
	eSIR_EXTSCAN_STOP_RSP,
	eSIR_EXTSCAN_CACHED_RESULTS_RSP,
	eSIR_EXTSCAN_SET_BSSID_HOTLIST_RSP,
	eSIR_EXTSCAN_RESET_BSSID_HOTLIST_RSP,
	eSIR_EXTSCAN_SET_SIGNIFICANT_WIFI_CHANGE_RSP,
	eSIR_EXTSCAN_RESET_SIGNIFICANT_WIFI_CHANGE_RSP,

	eSIR_EXTSCAN_GET_CAPABILITIES_IND,
	eSIR_EXTSCAN_HOTLIST_MATCH_IND,
	eSIR_EXTSCAN_SIGNIFICANT_WIFI_CHANGE_RESULTS_IND,
	eSIR_EXTSCAN_CACHED_RESULTS_IND,
	eSIR_EXTSCAN_SCAN_RES_AVAILABLE_IND,
	eSIR_EXTSCAN_SCAN_PROGRESS_EVENT_IND,
	eSIR_EXTSCAN_FULL_SCAN_RESULT_IND,
	eSIR_EPNO_NETWORK_FOUND_IND,
	eSIR_PASSPOINT_NETWORK_FOUND_IND,
	eSIR_EXTSCAN_SET_SSID_HOTLIST_RSP,
	eSIR_EXTSCAN_RESET_SSID_HOTLIST_RSP,

	/* Keep this last */
	eSIR_EXTSCAN_CALLBACK_TYPE_MAX,
} tSirExtScanCallbackType;

/**
 * enum wifi_extscan_event_type - extscan event type
 * @WIFI_EXTSCAN_RESULTS_AVAILABLE: reported when REPORT_EVENTS_EACH_SCAN is set
 *		and a scan cycle completes. WIFI_SCAN_THRESHOLD_NUM_SCANS or
 *		WIFI_SCAN_THRESHOLD_PERCENT can be reported instead if the
 *		reason for the event is available; however, at most one of
 *		these events should be reported per scan.
 * @WIFI_EXTSCAN_THRESHOLD_NUM_SCANS: can be reported when
 *		REPORT_EVENTS_EACH_SCAN is not set and
 *		report_threshold_num_scans is reached.
 * @WIFI_EXTSCAN_THRESHOLD_PERCENT: can be reported when REPORT_EVENTS_EACH_SCAN
 *		is not set and report_threshold_percent is reached.
 * @WIFI_SCAN_DISABLED: reported when currently executing gscans are disabled
 *		start_gscan will need to be called again in order to continue
 *		scanning.
 * @WIFI_EXTSCAN_BUCKET_STARTED_EVENT: Bucket started event
 *		This event is consumed in driver only.
 * @WIFI_EXTSCAN_CYCLE_STARTED_EVENT: Cycle started event.
 *		This event is consumed in driver only.
 * @WIFI_EXTSCAN_CYCLE_COMPLETED_EVENT: Cycle complete event. This event
 *		triggers @WIFI_EXTSCAN_RESULTS_AVAILABLE to the user space.
 */
enum wifi_extscan_event_type {
	WIFI_EXTSCAN_RESULTS_AVAILABLE,
	WIFI_EXTSCAN_THRESHOLD_NUM_SCANS,
	WIFI_EXTSCAN_THRESHOLD_PERCENT,
	WIFI_SCAN_DISABLED,

	WIFI_EXTSCAN_BUCKET_STARTED_EVENT = 0x10,
	WIFI_EXTSCAN_CYCLE_STARTED_EVENT,
	WIFI_EXTSCAN_CYCLE_COMPLETED_EVENT,
};

/**
 * enum extscan_configuration_flags - extscan config flags
 * @EXTSCAN_LP_EXTENDED_BATCHING: extended batching
 */
enum extscan_configuration_flags {
	EXTSCAN_LP_EXTENDED_BATCHING = 0x00000001,
};

/**
 * struct ext_scan_capabilities_response - extscan capabilities response data
 * @requestId: request identifier
 * @status:    status
 * @max_scan_cache_size: total space allocated for scan (in bytes)
 * @max_scan_buckets: maximum number of channel buckets
 * @max_ap_cache_per_scan: maximum number of APs that can be stored per scan
 * @max_rssi_sample_size: number of RSSI samples used for averaging RSSI
 * @ax_scan_reporting_threshold: max possible report_threshold
 * @max_hotlist_bssids: maximum number of entries for hotlist APs
 * @max_significant_wifi_change_aps: maximum number of entries for
 *				significant wifi change APs
 * @max_bssid_history_entries: number of BSSID/RSSI entries that device can hold
 * @max_hotlist_ssids: maximum number of entries for hotlist SSIDs
 * @max_number_epno_networks: max number of epno entries
 * @max_number_epno_networks_by_ssid: max number of epno entries
 *			if ssid is specified, that is, epno entries for
 *			which an exact match is required,
 *			or entries corresponding to hidden ssids
 * @max_number_of_white_listed_ssid: max number of white listed SSIDs
 * @max_number_of_black_listed_bssid: max number of black listed BSSIDs
 */
struct ext_scan_capabilities_response {
	uint32_t requestId;
	uint32_t status;

	uint32_t max_scan_cache_size;
	uint32_t max_scan_buckets;
	uint32_t max_ap_cache_per_scan;
	uint32_t max_rssi_sample_size;
	uint32_t max_scan_reporting_threshold;

	uint32_t max_hotlist_bssids;
	uint32_t max_significant_wifi_change_aps;

	uint32_t max_bssid_history_entries;
	uint32_t max_hotlist_ssids;
	uint32_t max_number_epno_networks;
	uint32_t max_number_epno_networks_by_ssid;
	uint32_t max_number_of_white_listed_ssid;
	uint32_t max_number_of_black_listed_bssid;
};

typedef struct {
	/* Time of discovery */
	uint64_t ts;

	/* Null terminated SSID */
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1];

	struct qdf_mac_addr bssid;

	/* Frequency in MHz */
	uint32_t channel;

	/* RSSI in dBm */
	int32_t rssi;

	/* RTT in nanoseconds */
	uint32_t rtt;

	/* Standard deviation in rtt */
	uint32_t rtt_sd;

	/* Period advertised in the beacon */
	uint16_t beaconPeriod;

	/* Capabilities advertised in the beacon */
	uint16_t capability;

	uint16_t ieLength;

	uint8_t ieData[];
} tSirWifiScanResult, *tpSirWifiScanResult;

/**
 * struct extscan_hotlist_match - extscan hotlist match
 * @requestId: request identifier
 * @numOfAps: number of bssids retrieved by the scan
 * @moreData: 0 - for last fragment
 *	      1 - still more fragment(s) coming
 * @ap: wifi scan result
 */
struct extscan_hotlist_match {
	uint32_t    requestId;
	bool        moreData;
	bool        ap_found;
	uint32_t    numOfAps;
	tSirWifiScanResult   ap[];
};

/**
 * struct extscan_cached_scan_result - extscan cached scan result
 * @scan_id: a unique identifier for the scan unit
 * @flags: a bitmask with additional information about scan
 * @num_results: number of bssids retrieved by the scan
 * @buckets_scanned: bitmask of buckets scanned in current extscan cycle
 * @ap: wifi scan bssid results info
 */
struct extscan_cached_scan_result {
	uint32_t    scan_id;
	uint32_t    flags;
	uint32_t    num_results;
	uint32_t    buckets_scanned;
	tSirWifiScanResult *ap;
};

/**
 * struct extscan_cached_scan_results - extscan cached scan results
 * @request_id: request identifier
 * @more_data: 0 - for last fragment
 *	       1 - still more fragment(s) coming
 * @num_scan_ids: number of scan ids
 * @result: wifi scan result
 */
struct extscan_cached_scan_results {
	uint32_t    request_id;
	bool        more_data;
	uint32_t    num_scan_ids;
	struct extscan_cached_scan_result  *result;
};


/**
 * struct tSirWifiFullScanResultEvent - extscan full scan event
 * @request_id: request identifier
 * @moreData: 0 - for last fragment
 *             1 - still more fragment(s) coming
 * @ap: bssid info
 *
 * Reported when each probe response is received, if report_events
 * enabled in struct wifi_scan_cmd_req_params
 */
typedef struct {
	uint32_t            requestId;
	bool                moreData;
	tSirWifiScanResult  ap;
} tSirWifiFullScanResultEvent, *tpSirWifiFullScanResultEvent;

/**
 * struct pno_match_found - epno match found
 * @request_id: request identifier
 * @moreData: 0 - for last fragment
     * 1 - still more fragment(s) coming
 * @num_results: number of bssids, driver sends this event to upper layer
 *		 for every beacon, hence %num_results is always set to 1.
 * @ap: bssid info
 *
 * Reported when each beacon probe response is received with
 * epno match found tag.
     */
struct pno_match_found {
	uint32_t            request_id;
	bool                more_data;
	uint32_t            num_results;
	tSirWifiScanResult  ap[];
};

/**
 * struct sir_extscan_generic_response -
 *	Generic ExtScan Response structure
 * @request_id: ID of the request
 * @status: operation status returned by firmware
 */
struct sir_extscan_generic_response {
	uint32_t request_id;
	uint32_t status;
};

typedef struct {
	struct qdf_mac_addr bssid;
	uint32_t channel;
	uint32_t numOfRssi;

	/* Rssi history in db */
	int32_t rssi[];
} tSirWifiSignificantChange, *tpSirWifiSignificantChange;

typedef struct {
	uint32_t requestId;

	bool moreData;
	uint32_t numResults;
	tSirWifiSignificantChange ap[];
} tSirWifiSignificantChangeEvent, *tpSirWifiSignificantChangeEvent;

typedef struct {
	uint32_t requestId;
	uint32_t numResultsAvailable;
} tSirExtScanResultsAvailableIndParams, *tpSirExtScanResultsAvailableIndParams;

typedef struct {
	uint32_t requestId;
	uint32_t status;
	uint8_t scanEventType;
	uint32_t   buckets_scanned;
} tSirExtScanOnScanEventIndParams, *tpSirExtScanOnScanEventIndParams;

#define MAX_EPNO_NETWORKS 64

#define SIR_PASSPOINT_LIST_MAX_NETWORKS 8

/**
 * struct wifi_passpoint_match - wifi passpoint network match
 * @id: network block identifier for the matched network
 * @anqp_len: length of ANQP blob
 * @ap: scan result, with channel and beacon information
 * @anqp: ANQP data, in the information_element format
 */
struct wifi_passpoint_match {
	uint32_t  request_id;
	uint32_t  id;
	uint32_t  anqp_len;
	tSirWifiScanResult ap;
	uint8_t   anqp[];
};
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
struct auto_shutdown_cmd {
	uint32_t timer_val;
};
#endif

#ifdef WLAN_POWER_DEBUG
/**
 * struct power_stats_response - Power stats response
 * @cumulative_sleep_time_ms: cumulative sleep time in ms
 * @cumulative_total_on_time_ms: total awake time in ms
 * @deep_sleep_enter_counter: deep sleep enter counter
 * @last_deep_sleep_enter_tstamp_ms: last deep sleep enter timestamp
 * @debug_register_fmt: debug registers format
 * @num_debug_register: number of debug registers
 * @debug_registers: Pointer to the debug registers buffer
 */
struct power_stats_response {
	uint32_t cumulative_sleep_time_ms;
	uint32_t cumulative_total_on_time_ms;
	uint32_t deep_sleep_enter_counter;
	uint32_t last_deep_sleep_enter_tstamp_ms;
	uint32_t debug_register_fmt;
	uint32_t num_debug_register;
	uint32_t *debug_registers;
};
#endif

#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
#define MAX_BCNMISS_BITMAP 8
/**
 * struct bcn_reception_stats_rsp - beacon stats response
 * @total_bcn_cnt: total beacon count (tbtt instances)
 * @total_bmiss_cnt: Total beacon miss count in last 255 beacons, max 255
 * @bmiss_bitmap: This bitmap indicates the status of the last 255 beacons.
 * If a bit is set, that means the corresponding beacon was missed.
 * Bit 0 of bmiss_bitmap[0] represents the most recent beacon.
 * The total_bcn_cnt field indicates how many bits within bmiss_bitmap
 * are valid.
 */
struct bcn_reception_stats_rsp {
	uint32_t vdev_id;
	uint32_t total_bcn_cnt;
	uint32_t total_bmiss_cnt;
	uint32_t bmiss_bitmap[MAX_BCNMISS_BITMAP];
};
#endif

/**
 * struct lfr_firmware_status - LFR status in firmware
 * @is_disabled: Is LFR disabled in FW
 * @disable_lfr_event: Disable attempt done in FW
 */
struct lfr_firmware_status {
	uint32_t is_disabled;
	struct completion disable_lfr_event;
};

/**
 * struct rso_cmd_status - RSO Command status
 * @vdev_id: Vdev ID for which RSO command sent
 * @status: Status of RSO command sent to FW
 */
struct rso_cmd_status {
	uint32_t vdev_id;
	bool status;
};

enum {
	SIR_AP_RX_DATA_OFFLOAD             = 0x00,
	SIR_STA_RX_DATA_OFFLOAD            = 0x01,
};

/**
 * struct sir_set_vdev_ies_per_band
 * @msg_type: message type
 * @len: message length
 * @vdev_id: vdev id
 *
 * Message wrapper structure for eWNI_SME_SET_VDEV_IES_PER_BAND.
 */
struct sir_set_vdev_ies_per_band {
	uint16_t msg_type;
	uint16_t len;
	uint32_t vdev_id;
	uint16_t dot11_mode;
	enum QDF_OPMODE device_mode;
};

/**
 * struct sir_set_ht_vht_cfg - ht, vht IE config
 * @msg_type: message type
 * @len: message length
 * @pdev_id: pdev id
 * @nss: Nss value
 * @dot11mode: Dot11 mode.
 *
 * Message wrapper structure for set HT/VHT IE req.
 */
struct sir_set_ht_vht_cfg {
	uint16_t msg_type;
	uint16_t len;
	uint32_t pdev_id;
	uint32_t nss;
	uint32_t dot11mode;
};

#define WIFI_INVALID_PEER_ID            (-1)
#define WIFI_INVALID_VDEV_ID            (-1)
#define WIFI_MAX_AC                     (4)
#define RATE_STAT_MCS_MASK              (0xFF00)
#define RATE_STAT_GET_MCS_INDEX(x)      (((x) & RATE_STAT_MCS_MASK) >> 8)

typedef struct {
	uint32_t paramId;
	uint8_t ifaceId;
	uint32_t rspId;
	uint32_t moreResultToFollow;
	uint32_t nr_received;
	union {
		uint32_t num_peers;
		uint32_t num_radio;
	};

	uint32_t peer_event_number;
	/* Variable  length field - Do not add anything after this */
	uint8_t results[0];
} tSirLLStatsResults, *tpSirLLStatsResults;

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*---------------------------------------------------------------------------
   WLAN_HAL_LL_NOTIFY_STATS
   ---------------------------------------------------------------------------*/

/******************************LINK LAYER Statistics**********************/

typedef struct {
	uint32_t reqId;
	uint8_t staId;
	uint32_t mpduSizeThreshold;
	uint32_t aggressiveStatisticsGathering;
} tSirLLStatsSetReq, *tpSirLLStatsSetReq;

typedef struct {
	uint32_t reqId;
	uint8_t staId;
	uint32_t paramIdMask;
} tSirLLStatsGetReq, *tpSirLLStatsGetReq;

typedef struct {
	uint32_t reqId;
	uint8_t staId;
	uint32_t statsClearReqMask;
	uint8_t stopReq;
} tSirLLStatsClearReq, *tpSirLLStatsClearReq;

typedef enum {
	WIFI_DISCONNECTED = 0,
	WIFI_AUTHENTICATING = 1,
	WIFI_ASSOCIATING = 2,
	WIFI_ASSOCIATED = 3,
	WIFI_EAPOL_STARTED = 4, /* if done by firmware/driver */
	WIFI_EAPOL_COMPLETED = 5,       /* if done by firmware/driver */
} tSirWifiConnectionState;

typedef enum {
	WIFI_ROAMING_IDLE = 0,
	WIFI_ROAMING_ACTIVE = 1,
} tSirWifiRoamState;

typedef enum {
	WIFI_INTERFACE_STA = 0,
	WIFI_INTERFACE_SOFTAP = 1,
	WIFI_INTERFACE_IBSS = 2,
	WIFI_INTERFACE_P2P_CLIENT = 3,
	WIFI_INTERFACE_P2P_GO = 4,
	WIFI_INTERFACE_NAN = 5,
	WIFI_INTERFACE_MESH = 6,
	WIFI_INTERFACE_NDI = 7,
} tSirWifiInterfaceMode;

/* set for QOS association */
#define WIFI_CAPABILITY_QOS          0x00000001
/* set for protected assoc (802.11 beacon frame control protected bit set) */
#define WIFI_CAPABILITY_PROTECTED    0x00000002
/* set if 802.11 Extended Capabilities element interworking bit is set */
#define WIFI_CAPABILITY_INTERWORKING 0x00000004
/* set for HS20 association */
#define WIFI_CAPABILITY_HS20         0x00000008
/* set is 802.11 Extended Capabilities element UTF-8 SSID bit is set */
#define WIFI_CAPABILITY_SSID_UTF8    0x00000010
/* set is 802.11 Country Element is present */
#define WIFI_CAPABILITY_COUNTRY      0x00000020

struct wifi_interface_info {
	/* tSirWifiInterfaceMode */
	/* interface mode */
	uint8_t mode;
	/* interface mac address (self) */
	struct qdf_mac_addr macAddr;
	/* tSirWifiConnectionState */
	/* connection state (valid for STA, CLI only) */
	uint8_t state;
	/* tSirWifiRoamState */
	/* roaming state */
	uint32_t roaming;
	/* WIFI_CAPABILITY_XXX (self) */
	uint32_t capabilities;
	/* null terminated SSID */
	uint8_t ssid[33];
	/* bssid */
	struct qdf_mac_addr bssid;
	/* country string advertised by AP */
	uint8_t apCountryStr[REG_ALPHA2_LEN + 1];
	/* country string for this association */
	uint8_t countryStr[REG_ALPHA2_LEN + 1];
};

/**
 * struct wifi_channel_info - channel information
 * @width: channel width (20, 40, 80, 80+80, 160)
 * @center_freq: primary 20 MHz channel
 * @center_freq0: center frequency (MHz) first segment
 * @center_freq1: center frequency (MHz) second segment
 */
struct wifi_channel_info {
	enum phy_ch_width width;
	uint32_t center_freq;
	uint32_t center_freq0;
	uint32_t center_freq1;
};

/**
 * struct wifi_rate_info - wifi rate information
 * @preamble: 0:OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved
 * @nss: 0:1x1, 1:2x2, 3:3x3, 4:4x4
 * @bw: 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz
 * @rate_or_mcs_index:
 * * OFDM/CCK: rate code per ieee std in units of 0.5mbps
 * * HT/VHT: mcs index
 * @reserved: reserved
 * @bitrate: bitrate units of 100 Kbps
 */
struct wifi_rate_info {
	uint32_t preamble:3;
	uint32_t nss:2;
	uint32_t bw:3;
	uint32_t rate_or_mcs_index:8;
	uint32_t reserved:16;
	uint32_t bitrate;
};

/**
 * struct wifi_channel_stats - channel statistics
 * @channel: channel for which the stats are applicable
 * @on_time: msecs the radio is awake
 * @cca_busy_time: secs the CCA register is busy excluding own tx_time
 * @tx_time: msecs the radio is transmitting on this channel
 * @rx_time: msecs the radio is in active receive on this channel
 */
struct wifi_channel_stats {
	struct wifi_channel_info channel;
	uint32_t on_time;
	uint32_t cca_busy_time;
	uint32_t tx_time;
	uint32_t rx_time;

};

/**
 * struct wifi_radio_stats - per-radio statistics
 * @radio: wifi radio for which the stats are applicable
 * @on_time: msecs the radio is awake
 * @tx_time: msecs the radio is transmitting
 * @rx_time: msecs the radio is in active receive
 * @on_time_scan: msecs the radio is awake due to all scan
 * @on_time_nbd: msecs the radio is awake due to NAN
 * @on_time_gscan: msecs the radio is awake due to Gscan
 * @on_time_roam_scan: msecs the radio is awake due to roam scan
 * @on_time_pno_scan: msecs the radio is awake due to PNO scan
 * @on_time_hs20: msecs the radio is awake due to HS2.0 scans and GAS exchange
 * @on_time_host_scan: msecs the radio is awake due to Host initiated scan
 * @on_time_lpi_scan: msecs the radio is awake due to LPI scan
 * @total_num_tx_power_levels: @tx_time_per_power_level record count
 * @tx_time_per_power_level:  tx time (in milliseconds) per TPC level (0.5 dBm)
 * @more_channels: If more channels are there and will come in next event
 * @num_channels: @channels record count
 * @channels: per-channel statistics
 */
struct wifi_radio_stats {
	uint32_t radio;
	uint32_t on_time;
	uint32_t tx_time;
	uint32_t rx_time;
	uint32_t on_time_scan;
	uint32_t on_time_nbd;
	uint32_t on_time_gscan;
	uint32_t on_time_roam_scan;
	uint32_t on_time_pno_scan;
	uint32_t on_time_hs20;
	uint32_t on_time_host_scan;
	uint32_t on_time_lpi_scan;
	uint32_t total_num_tx_power_levels;
	uint32_t *tx_time_per_power_level;
	uint32_t more_channels;
	uint32_t num_channels;
	struct wifi_channel_stats *channels;
};

/**
 * struct wifi_rate_stat - per rate statistics
 * @rate: rate information
 * @tx_mpdu: number of successfully transmitted data pkts (ACK rcvd)
 * @rx_mpdu: number of received data pkts
 * @mpdu_lost: number of data packet losses (no ACK)
 * @retries: total number of data pkt retries *
 * @retries_short: number of short data pkt retries
 * @retries_long: number of long data pkt retries
 */
struct wifi_rate_stat {
	struct wifi_rate_info rate;
	uint32_t tx_mpdu;
	uint32_t rx_mpdu;
	uint32_t mpdu_lost;
	uint32_t retries;
	uint32_t retries_short;
	uint32_t retries_long;
};

/* wifi peer type */
typedef enum {
	WIFI_PEER_STA,
	WIFI_PEER_AP,
	WIFI_PEER_P2P_GO,
	WIFI_PEER_P2P_CLIENT,
	WIFI_PEER_NAN,
	WIFI_PEER_TDLS,
	WIFI_PEER_INVALID,
} tSirWifiPeerType;

/**
 * struct wifi_peer_info - per peer information
 * @type: peer type (AP, TDLS, GO etc.)
 * @peer_macaddr: mac address
 * @capabilities: peer WIFI_CAPABILITY_XXX
 * @power_saving: peer power saving mode
 * @num_rate: number of rates
 * @rate_stats: per rate statistics, number of entries  = @num_rate
 */
struct wifi_peer_info {
	enum wmi_peer_type type;
	struct qdf_mac_addr peer_macaddr;
	uint32_t capabilities;
	union {
		uint32_t power_saving;
		uint32_t num_rate;
	};
	struct wifi_rate_stat rate_stats[0];
};

/**
 * struct wifi_interface_stats - Interface statistics
 * @info: struct containing the current state of the interface
 * @rts_succ_cnt: number of RTS/CTS sequence success
 * @rts_fail_cnt: number of RTS/CTS sequence failures
 * @ppdu_succ_cnt: number of PPDUs transmitted
 * @ppdu_fail_cnt: number of PPDUs that failed to transmit
 * @link_stats: link-level statistics
 * @ac_stats: per-Access Category statistics
 * @num_offload_stats: @offload_stats record count
 * @offload_stats: per-offload statistics
 *
 * Statistics corresponding to 2nd most LSB in wifi statistics bitmap
 * for getting statistics
 */
struct wifi_interface_stats {
	struct wifi_interface_info info;
	uint32_t rts_succ_cnt;
	uint32_t rts_fail_cnt;
	uint32_t ppdu_succ_cnt;
	uint32_t ppdu_fail_cnt;
	wmi_iface_link_stats link_stats;
	wmi_wmm_ac_stats ac_stats[WIFI_AC_MAX];
	uint32_t num_offload_stats;
	wmi_iface_offload_stats offload_stats[WMI_OFFLOAD_STATS_TYPE_MAX];
};

/**
 * struct wifi_peer_stat - peer statistics
 * @num_peers: number of peers
 * @peer_info: per peer statistics
 *
 * Peer statistics - corresponding to 3rd most LSB in
 * wifi statistics bitmap for getting statistics
 */
struct wifi_peer_stat {
	uint32_t num_peers;
	struct wifi_peer_info peer_info[0];
};

/* wifi statistics bitmap  for getting statistics */
#define WMI_LINK_STATS_RADIO          0x00000001
#define WMI_LINK_STATS_IFACE          0x00000002
#define WMI_LINK_STATS_ALL_PEER       0x00000004
#define WMI_LINK_STATS_PER_PEER       0x00000008

/* wifi statistics bitmap  for clearing statistics */
/* all radio statistics */
#define WIFI_STATS_RADIO              0x00000001
/* cca_busy_time (within radio statistics) */
#define WIFI_STATS_RADIO_CCA          0x00000002
/* all channel statistics (within radio statistics) */
#define WIFI_STATS_RADIO_CHANNELS     0x00000004
/* all scan statistics (within radio statistics) */
#define WIFI_STATS_RADIO_SCAN         0x00000008
/* all interface statistics */
#define WIFI_STATS_IFACE              0x00000010
/* all tx rate statistics (within interface statistics) */
#define WIFI_STATS_IFACE_TXRATE       0x00000020
/* all ac statistics (within interface statistics) */
#define WIFI_STATS_IFACE_AC           0x00000040
/* all contention (min, max, avg) statistics (within ac statistics) */
#define WIFI_STATS_IFACE_CONTENTION   0x00000080
/* All peer stats on this interface */
#define WIFI_STATS_IFACE_ALL_PEER      0x00000100
/* Clear particular peer stats depending on the peer_mac */
#define WIFI_STATS_IFACE_PER_PEER      0x00000200

/**
 * struct sir_wifi_iface_tx_fail - TX failure event
 * @tid: TX TID
 * @msdu_num: TX MSDU failed counter
 * @status: TX status from HTT message.
 *          Only failure status will be involved.
 */
struct sir_wifi_iface_tx_fail {
	uint8_t  tid;
	uint16_t msdu_num;
	enum htt_tx_status status;
};

/**
 * struct sir_wifi_chan_cca_stats - channal CCA stats
 * @vdev_id: vdev ID
 * @idle_time: percentage of idle time, no TX, no RX, no interference
 * @tx_time: percentage of time transmitting packets
 * @rx_in_bss_time: percentage of time receiving packets in current BSS
 * @rx_out_bss_time: percentage of time receiving packets not in current BSS
 * @rx_busy_time: percentage of time interference detected
 * @rx_in_bad_cond_time: percentage of time receiving packets with errors
 *	or packets flagged as retransmission or seqnum discontinued.
 * @tx_in_bad_cond_time: percentage of time the device transmitted packets
 *	that haven't been ACKed.
 * @wlan_not_avail_time: percentage of time the chip is unable to
 *	work in normal conditions.
 */
struct sir_wifi_chan_cca_stats {
	uint32_t vdev_id;
	uint32_t idle_time;
	uint32_t tx_time;
	uint32_t rx_in_bss_time;
	uint32_t rx_out_bss_time;
	uint32_t rx_busy_time;
	uint32_t rx_in_bad_cond_time;
	uint32_t tx_in_bad_cond_time;
	uint32_t wlan_not_avail_time;
};

#define WIFI_MAX_CHAINS                 8

/**
 * struct sir_wifi_peer_signal_stats - peer signal stats
 * @vdev_id: vdev ID
 * @peer_id: peer ID
 * @per_ant_snr: per antenna SNR
 * @nf: peer background noise
 * @per_ant_rx_mpdus: MPDUs received per antenna
 * @per_ant_tx_mpdus: MPDUs transferred per antenna
 * @num_chain: valid chain count
 */
struct sir_wifi_peer_signal_stats {
	uint32_t vdev_id;
	uint32_t peer_id;

	/* per antenna SNR in current bss */
	int32_t per_ant_snr[WIFI_MAX_CHAINS];

	/* Background noise */
	int32_t nf[WIFI_MAX_CHAINS];

	uint32_t per_ant_rx_mpdus[WIFI_MAX_CHAINS];
	uint32_t per_ant_tx_mpdus[WIFI_MAX_CHAINS];
	uint32_t num_chain;
};

#define WIFI_VDEV_NUM           4
#define WFIF_MCS_NUM            10
#define WIFI_AGGR_NUM           8
#define WIFI_DELAY_SIZE         11

/**
 * struct sir_wifi_tx - per AC tx stats
 * @msdus: number of totoal MSDUs on MAC layer in the period
 * @mpdus: number of totoal MPDUs on MAC layer in the period
 * @ppdus: number of totoal PPDUs on PHY layer in the period
 * @bytes: bytes of tx data on MAC layer in the period
 * @drops: number of TX packets cancelled due to any reason in the period,
 *	such as WMM limitation/bandwidth limitation/radio congestion
 * @drop_bytes: bytes of dropped TX packets in the period
 * @retries: number of unacked transmissions of MPDUs
 * @failed: number of packets have not been ACKed despite retried
 * @aggr_len: length of the MPDU aggregation size buffer
 * @mpdu_aggr_size: histogram of MPDU aggregation size
 * @success_mcs_len: length of success mcs buffer
 * @success_mcs: histogram of successed received MPDUs encoding rate
 * @fail_mcs_len: length of failed mcs buffer
 * @fail_mcs: histogram of failed received MPDUs encoding rate
 * @delay_len: length of the delay histofram buffer
 * @delay: histogram of delays on MAC layer
 */
struct sir_wifi_tx {
	uint32_t msdus;
	uint32_t mpdus;
	uint32_t ppdus;
	uint32_t bytes;
	uint32_t drops;
	uint32_t drop_bytes;
	uint32_t retries;
	uint32_t failed;
	uint32_t aggr_len;
	uint32_t *mpdu_aggr_size;
	uint32_t success_mcs_len;
	uint32_t *success_mcs;
	uint32_t fail_mcs_len;
	uint32_t *fail_mcs;
	uint32_t delay_len;
	uint32_t *delay;
};

/**
 * struct sir_wifi_rx - per AC rx stats
 * @mpdus: number of RX packets on MAC layer
 * @bytes: bytes of RX packets on MAC layer
 * @ppdus: number of RX packets on PHY layer
 * @ppdu_bytes: bytes of RX packets on PHY layer
 * @mpdu_lost: number of discontinuity in seqnum
 * @mpdu_retry: number of RX packets flagged as retransmissions
 * @mpdu_dup: number of RX packets identified as duplicates
 * @mpdu_discard: number of RX packets discarded
 * @aggr_len: length of MPDU aggregation histogram buffer
 * @mpdu_aggr: histogram of MPDU aggregation size
 * @mcs_len: length of mcs histogram buffer
 * @mcs: histogram of encoding rate.
 */
struct sir_wifi_rx {
	uint32_t mpdus;
	uint32_t bytes;
	uint32_t ppdus;
	uint32_t ppdu_bytes;
	uint32_t mpdu_lost;
	uint32_t mpdu_retry;
	uint32_t mpdu_dup;
	uint32_t mpdu_discard;
	uint32_t aggr_len;
	uint32_t *mpdu_aggr;
	uint32_t mcs_len;
	uint32_t *mcs;
};

/**
 * struct sir_wifi_ll_ext_wmm_ac_stats - stats for WMM AC
 * @type: WMM AC type
 * @tx_stats: pointer to TX stats
 * @rx_stats: pointer to RX stats
 */
struct sir_wifi_ll_ext_wmm_ac_stats {
	uint32_t type;
	struct sir_wifi_tx *tx_stats;
	struct sir_wifi_rx *rx_stats;
};

/**
 * struct sir_wifi_ll_ext_peer_stats - per peer stats
 * @peer_id: peer ID
 * @vdev_id: VDEV ID
 * @mac_address: MAC address
 * @sta_ps_inds: how many times STAs go to sleep
 * @sta_ps_durs: total sleep time of STAs (units in ms)
 * @rx_probe_reqs: number of probe requests received
 * @rx_oth_mgmts: number of other management frames received,
 *		  not including probe requests
 * @peer_signal_stat: signal stats
 * @ac_stats: WMM BE/BK/VI/VO stats
 */
struct sir_wifi_ll_ext_peer_stats {
	uint32_t peer_id;
	uint32_t vdev_id;
	tSirMacAddr mac_address;
	uint32_t sta_ps_inds;
	uint32_t sta_ps_durs;
	uint32_t rx_probe_reqs;
	uint32_t rx_oth_mgmts;
	struct sir_wifi_peer_signal_stats peer_signal_stats;
	struct sir_wifi_ll_ext_wmm_ac_stats ac_stats[WIFI_MAX_AC];
};

/**
 * struct sir_wifi_ll_ext_stats - link layer stats report
 * @trigger_cond_id:  Indicate what triggered this event.
 *	1: timeout. 2: threshold
 * @cca_chgd_bitmap: Bitmap to indicate changed channel CCA stats
 *	which exceeded the thresholds
 * @sig_chgd_bitmap: Bitmap to indicate changed peer signal stats
 *	which exceeded the thresholds
 * @tx_chgd_bitmap: Bitmap to indicate changed TX counters
 *	which exceeded the thresholds
 * @rx_chgd_bitmap: Bitmap to indicate changed RX counters
 *	which exceeded the thresholds
 * @chan_cca_stats: channel CCA stats
 * @peer_signal_stats: peer signal stats
 * @tx_mpdu_aggr_array_len: length of TX MPDU aggregation buffer
 * @tx_succ_mcs_array_len: length of mcs buffer for ACKed MPDUs
 * @tx_fail_mcs_array_len: length of mcs buffer for no-ACKed MPDUs
 * @tx_delay_array_len: length of delay stats buffer
 * @rx_mpdu_aggr_array_len: length of RX MPDU aggregation buffer
 * @rx_mcs_array_len: length of RX mcs stats buffer
 * @peer_stats: peer stats
 * @cca: physical channel CCA stats
 * @stats: pointer to stats data buffer.
 *
 * Structure of the whole statictics is like this:
 *     ---------------------------------
 *     |      trigger_cond_i           |
 *     +-------------------------------+
 *     |      cca_chgd_bitmap          |
 *     +-------------------------------+
 *     |      sig_chgd_bitmap          |
 *     +-------------------------------+
 *     |      tx_chgd_bitmap           |
 *     +-------------------------------+
 *     |      rx_chgd_bitmap           |
 *     +-------------------------------+
 *     |      peer_num                 |
 *     +-------------------------------+
 *     |      channel_num              |
 *     +-------------------------------+
 *     |      tx_mpdu_aggr_array_len   |
 *     +-------------------------------+
 *     |      tx_succ_mcs_array_len    |
 *     +-------------------------------+
 *     |      tx_fail_mcs_array_len    |
 *     +-------------------------------+
 *     |      tx_delay_array_len       |
 *     +-------------------------------+
 *     |      rx_mpdu_aggr_array_len   |
 *     +-------------------------------+
 *     |      rx_mcs_array_len         |
 *     +-------------------------------+
 *     |      pointer to CCA stats     |
 *     +-------------------------------+
 *     |      pointer to peer stats    |
 *     +-------------------------------+
 *     |      CCA stats                |
 *     +-------------------------------+
 *     |      peer_stats               |----+
 *     +-------------------------------+    |
 *     |      per peer signals stats   |<---+
 *     |        peer0 ~ peern          |    |
 *     +-------------------------------+    |
 *     | TX aggr/mcs parameters array  |    |
 *     | Length of this buffer is      |    |
 *     | configurable for user layer.  |<-+ |
 *     +-------------------------------+  | |
 *     |      per peer tx stats        |--+ |
 *     |         BE                    | <--+
 *     |         BK                    |    |
 *     |         VI                    |    |
 *     |         VO                    |    |
 *     +-------------------------------+    |
 *     | TX aggr/mcs parameters array  |    |
 *     | Length of this buffer is      |    |
 *     | configurable for user layer.  |<-+ |
 *     +-------------------------------+  | |
 *     |      peer peer rx stats       |--+ |
 *     |         BE                    | <--+
 *     |         BE                    |
 *     |         BK                    |
 *     |         VI                    |
 *     |         VO                    |
 *     ---------------------------------
 */
struct sir_wifi_ll_ext_stats {
	uint32_t trigger_cond_id;
	uint32_t cca_chgd_bitmap;
	uint32_t sig_chgd_bitmap;
	uint32_t tx_chgd_bitmap;
	uint32_t rx_chgd_bitmap;
	uint8_t peer_num;
	uint8_t channel_num;
	uint32_t tx_mpdu_aggr_array_len;
	uint32_t tx_succ_mcs_array_len;
	uint32_t tx_fail_mcs_array_len;
	uint32_t tx_delay_array_len;
	uint32_t rx_mpdu_aggr_array_len;
	uint32_t rx_mcs_array_len;
	struct sir_wifi_ll_ext_peer_stats *peer_stats;
	struct sir_wifi_chan_cca_stats *cca;
	uint8_t stats[];
};

/**
 * struct sir_channel_cca_threshold - threshold for channel CCA
 * @idle_time: idle time, no TX, no RX, no interference
 * @tx_time: time transmitting packets
 * @rx_in_bss_time: time receiving packets in current BSSs
 * @rx_out_bss_time: time receiving packets not in current BSSs
 * @rx_busy_time: time interference detected
 * @rx_in_bad_cond_time: receiving packets with errors
 * @tx_in_bad_cond_time: time transmitted packets not been ACKed
 * @wlan_not_avail_time: wlan card cannot work
 */
struct sir_channel_cca_threshold {
	uint32_t idle_time;
	uint32_t tx_time;
	uint32_t rx_in_bss_time;
	uint32_t rx_out_bss_time;
	uint32_t rx_busy_time;
	uint32_t rx_in_bad_cond_time;
	uint32_t tx_in_bad_cond_time;
	uint32_t wlan_not_avail_time;
};

/**
 * struct sir_signal_threshold - threshold for per peer sigbal
 * @snr: signal to noise rate
 * @nf: noise floor
 */
struct sir_signal_threshold {
	uint32_t snr;
	uint32_t nf;
};

/**
 * struct sir_tx_threshold - threshold for TX
 * @msdu: TX MSDUs on MAC layer
 * @mpdu: TX MPDUs on MAC layer
 * @ppdu: TX PPDUs on MAC layer
 * @bytes: TX bytes on MAC layer
 * @msdu_drop: drooped MSDUs
 * @byte_drop: dropped Bytes
 * @mpdu_retry: MPDU not acked
 * @ppdu_fail: PPDUs which received no block ack
 * @aggregation: aggregation size
 * @succ_mcs: histogram of encoding rate for acked PPDUs
 * @fail_mcs: histogram of encoding rate for no-acked PPDUs
 */
struct sir_tx_threshold {
	uint32_t msdu;
	uint32_t mpdu;
	uint32_t ppdu;
	uint32_t bytes;
	uint32_t msdu_drop;
	uint32_t byte_drop;
	uint32_t mpdu_retry;
	uint32_t mpdu_fail;
	uint32_t ppdu_fail;
	uint32_t aggregation;
	uint32_t succ_mcs;
	uint32_t fail_mcs;
	uint32_t delay;
};

/**
 * struct sir_rx_threshold - threshold for RX
 * @mpdu: RX MPDUs on MAC layer
 * @bytes: RX bytes on MAC layer
 * @ppdu: RX PPDU on PHY layer
 * @ppdu_bytes: RX bytes on PHY layer
 * @disorder: discontinuity in seqnum
 * @mpdu_retry: MPDUs flagged as retry
 * @mpdu_dup: MPDUs identified as duplicated
 * @aggregation: aggregation size
 * @mcs: histogram of encoding rate for PPDUs
 * @ps_inds: power save indication
 * @ps_durs: total time in power save
 * @probe_reqs: probe request received
 * @other_mgmt: other MGMT frames received
 */
struct sir_rx_threshold {
	uint32_t mpdu;
	uint32_t bytes;
	uint32_t ppdu;
	uint32_t ppdu_bytes;
	uint32_t disorder;
	uint32_t mpdu_lost;
	uint32_t mpdu_retry;
	uint32_t mpdu_dup;
	uint32_t mpdu_discard;
	uint32_t aggregation;
	uint32_t mcs;
	uint32_t ps_inds;
	uint32_t ps_durs;
	uint32_t probe_reqs;
	uint32_t other_mgmt;
};

/**
 * struct sir_wifi_ll_ext_stats_threshold - Threshold for stats update
 * @period: MAC counter indication period (unit in ms)
 * @enable: if threshold mechnism is enabled or disabled
 * @enable_bitmap: whether dedicated threshold is enabed.
 *     Every MAC counter has a dedicated threshold. If the dedicated
 *     threshold is not set in the bitmap, global threshold will take
 *     effect.
 * @global: whether clobal threshold is enabled.
 *     When both global and dedicated threshold are disabled, MAC counter
 *     will indicate stats periodically.
 * @global_threshold: global threshold value
 * @cca_bitmap: bitmap for CCA.
 *     Bit0: idle time
 *     Bit1: tx time
 *     Bit2: RX in BSS
 *     Bit3: RX out of BSS
 *     Bit4: medium busy
 *     Bit5: RX bad
 *     Bit6: TX bad
 *     Bit7: WLAN card not available
 * @signal_bitmap:
 *     Bit0: Per channel SNR counter
 *     Bit1: Per channel noise floor counter
 * @tx_bitmap:  bitmap for TX counters
 *     Bit0: TX counter unit in MSDU
 *     Bit1: TX counter unit in MPDU
 *     Bit2: TX counter unit in PPDU
 *     Bit3: TX counter unit in byte
 *     Bit4: Dropped MSDUs
 *     Bit5: Dropped Bytes
 *     Bit6: MPDU retry counter
 *     Bit7: MPDU failure counter
 *     Bit8: PPDU failure counter
 *     Bit9: MPDU aggregation counter
 *     Bit10: MCS counter for ACKed MPDUs
 *     Bit11: MCS counter for Failed MPDUs
 *     Bit12: TX Delay counter
 * @rx_bitmap:bitmap for RX counters
 *     Bit0: MAC RX counter unit in MPDU
 *     Bit1: MAC RX counter unit in byte
 *     Bit2: PHY RX counter unit in PPDU
 *     Bit3: PHY RX counter unit in byte
 *     Bit4: Disorder counter
 *     Bit5: Retry counter
 *     Bit6: Duplication counter
 *     Bit7: Discard counter
 *     Bit8: MPDU aggregation size counter
 *     Bit9: MCS counter
 *     Bit10: Peer STA power state change (wake to sleep) counter
 *     Bit11: Peer STA power save counter, total time in PS mode
 *     Bit12: Probe request counter
 *     Bit13: Other management frames counter
 * @cca_thresh: CCA threshold
 * @signal_thresh: signal threshold
 * @tx_thresh: TX threshold
 * @rx_thresh: RX threshold
 *
 * Generally, Link layer statistics is reported periodically. But if the
 * variation of one stats of compared to the pervious notification exceeds
 * a threshold, FW will report the new stats immediately.
 * This structure contains threshold for different counters.
 */
struct sir_ll_ext_stats_threshold {
	uint32_t period;
	uint32_t enable;
	uint32_t enable_bitmap;
	uint32_t global;
	uint32_t global_threshold;
	uint32_t cca_bitmap;
	uint32_t signal_bitmap;
	uint32_t tx_bitmap;
	uint32_t rx_bitmap;
	struct sir_channel_cca_threshold cca;
	struct sir_signal_threshold signal;
	struct sir_tx_threshold tx;
	struct sir_rx_threshold rx;
};

#define LL_STATS_MIN_PERIOD          10
#define LL_STATS_INVALID_PERIOD      0xFFFFFFFF

/* Result ID for LL stats extension */
#define WMI_LL_STATS_EXT_PS_CHG             0x00000100
#define WMI_LL_STATS_EXT_TX_FAIL            0x00000200
#define WMI_LL_STATS_EXT_MAC_COUNTER        0x00000400
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

typedef struct sAniGetLinkStatus {
	uint16_t msgType;       /* message type is same as the request type */
	uint16_t msgLen;        /* length of the entire request */
	uint8_t linkStatus;
	uint8_t sessionId;
} tAniGetLinkStatus, *tpAniGetLinkStatus;

/**
 * struct sir_lost_link_info - lost link information structure.
 *
 * @vdev_id: vdev_id from WMA. some modules call sessionId.
 * @rssi: rssi at disconnection time.
 *
 * driver uses this structure to communicate information collected at
 * disconnection time.
 */
struct sir_lost_link_info {
	uint32_t vdev_id;
	int32_t rssi;
};

/* find the size of given member within a structure */
#ifndef member_size
#define member_size(type, member) (sizeof(((type *)0)->member))
#endif

#define RTT_INVALID                     0x00
#define RTT_TIMING_MEAS_CAPABILITY      0x01
#define RTT_FINE_TIME_MEAS_INITIATOR_CAPABILITY 0x02
#define RTT_FINE_TIME_MEAS_RESPONDER_CAPABILITY 0x03

/* number of neighbor reports that we can handle in Neighbor Report Response */
#define MAX_SUPPORTED_NEIGHBOR_RPT 15

/**
 * struct sir_stats_avg_factor
 * @vdev_id: session id
 * @stats_avg_factor: average factor
 */
struct sir_stats_avg_factor {
	uint8_t vdev_id;
	uint16_t stats_avg_factor;
};

/**
 * struct sir_guard_time_request
 * @vdev_id: session id
 * @guard_time: guard time
 */
struct sir_guard_time_request {
	uint8_t vdev_id;
	uint32_t guard_time;
};

/* Max number of rates allowed in Supported Rates IE */
#define MAX_NUM_SUPPORTED_RATES (8)

/**
 * struct rssi_breach_event - rssi breached event structure
 * @request_id: request id
 * @session_id: session id
 * @curr_rssi: current rssi
 * @curr_bssid: current bssid
 */
struct rssi_breach_event {
	uint32_t     request_id;
	uint32_t     session_id;
	int8_t       curr_rssi;
	struct qdf_mac_addr  curr_bssid;
};

/**
 * struct chip_pwr_save_fail_detected_params - chip power save failure detected
 * event params
 * @failure_reason_code:failure reason code
 * @wake_lock_bitmap:bitmap for modules voting against sleep for long duration.
 */
struct chip_pwr_save_fail_detected_params {
	uint32_t     failure_reason_code;
	uint32_t     wake_lock_bitmap[4];
};

#define MAX_NUM_FW_SEGMENTS 4

/**
 * DEFAULT_SCAN_IE_ID - Identifier for the collection of IE's added
 * by default to the probe request
 */
#define DEFAULT_SCAN_IE_ID 256

 /* MAX_DEFAULT_SCAN_IE_LEN - Maxmimum length of Default Scan IE's */
#define MAX_DEFAULT_SCAN_IE_LEN 2048

 /* Extended Capabilities IE header(IE Id + IE Length) length */
#define EXT_CAP_IE_HDR_LEN 2

/**
 * struct hdd_default_scan_ie - HDD default scan IE structure
 * @message_type: message type to be set with eWNI_SME_DEFAULT_SCAN_IE
 * @length: length of the struct hdd_default_scan_ie
 * @vdev_id: vdev_id
 * @ie_len: Default scan IE length
 * @ie_data: Pointer to default scan IE data
 */
struct hdd_default_scan_ie {
	uint16_t message_type;
	uint16_t length;
	uint16_t vdev_id;
	uint16_t ie_len;
	uint8_t ie_data[MAX_DEFAULT_SCAN_IE_LEN];
};

/**
 * struct vdev_ie_info - IE info
 * @vdev_id - vdev for which the IE is being sent
 * @ie_id - ID of the IE
 * @length - length of the IE data
 * @band - indicates IE is intended for which band
 * @data - IE data
 *
 * This structure is used to store the IE information.
 */
struct vdev_ie_info {
	uint32_t vdev_id;
	uint32_t ie_id;
	uint32_t length;
	uint32_t band;
	uint8_t *data;
};

/**
 * struct send_extcap_ie - used to pass send_extcap_ie msg from SME to PE
 * @type - MSG type
 * @length - length of the message
 * @seesion_id - session_id for which the message is intended for
 *
 * This structure is used to pass send_extcap_ie msg from SME to PE
 */
struct send_extcap_ie {
	uint16_t msg_type; /* eWNI_SME_SET_IE_REQ */
	uint16_t length;
	uint8_t session_id;
};

typedef void (*antenna_mode_cb)(uint32_t status, void *context);

/**
 * struct cfg_action_frm_tb_ppdu - action frame TB PPDU cfg
 * @vdev_id - vdev id
 * @cfg - enable/disable
 * @frm_len - frame length
 * @data - frame data
 *
 * This structure is used to cfg action frame tb ppdu.
 */
struct cfg_action_frm_tb_ppdu {
	uint8_t vdev_id;
	uint8_t cfg;
	uint8_t frm_len;
	uint8_t *data;
};

/**
 * struct sir_nss_update_request
 * @msgType: nss update msg type
 * @msgLen: length of the msg
 * @new_nss: new spatial stream value
 * @ch_width: channel width - optional
 * @vdev_id: session id
 */
struct sir_nss_update_request {
	uint16_t msgType;
	uint16_t msgLen;
	uint8_t  new_nss;
	uint8_t ch_width;
	uint32_t vdev_id;
};

/**
 * enum sir_bcn_update_reason: bcn update reason
 * @REASON_DEFAULT: reason default
 * @REASON_NSS_UPDATE: If NSS is updated
 * @REASON_CONFIG_UPDATE: Config update
 * @REASON_SET_HT2040: HT2040 update
 * @REASON_COLOR_CHANGE: Color change
 * @REASON_CHANNEL_SWITCH: channel switch
 */
enum sir_bcn_update_reason {
	REASON_DEFAULT = 0,
	REASON_NSS_UPDATE = 1,
	REASON_CONFIG_UPDATE = 2,
	REASON_SET_HT2040 = 3,
	REASON_COLOR_CHANGE = 4,
	REASON_CHANNEL_SWITCH = 5,
};

/**
 * struct sir_bcn_update_rsp
 *
 * @vdev_id: session for which bcn was updated
 * @reason: bcn update reason
 * @status: status of the beacon sent to FW
 */
struct sir_bcn_update_rsp {
	uint8_t vdev_id;
	enum sir_bcn_update_reason reason;
	QDF_STATUS status;
};

struct sir_qos_params {
	uint8_t aifsn;
	uint8_t cwmin;
	uint8_t cwmax;
};

/**
 * struct sir_sme_ext_change_chan_req - channel change request
 * @message_type: message id
 * @length: msg length
 * @new_channel: new channel
 * @vdev_id: vdev id
 */
struct sir_sme_ext_cng_chan_req {
	uint16_t  message_type; /* eWNI_SME_EXT_CHANGE_CHANNEL */
	uint16_t  length;
	uint32_t  new_channel;
	uint8_t   vdev_id;
};

#define IGNORE_NUD_FAIL                      0
#define DISCONNECT_AFTER_NUD_FAIL            1
#define ROAM_AFTER_NUD_FAIL                  2
#define DISCONNECT_AFTER_ROAM_FAIL           3

/**
 * struct sir_sme_ext_change_chan_ind.
 * @session_id: session id
 * @new_chan_freq: new channel frequency to change to
 */
struct sir_sme_ext_cng_chan_ind {
	uint8_t  session_id;
	uint32_t  new_chan_freq;
};

/**
 * struct stsf - the basic stsf structure
 *
 * @vdev_id: vdev id
 * @tsf_low: low 32bits of tsf
 * @tsf_high: high 32bits of tsf
 * @soc_timer_low: low 32bits of synced SOC timer value
 * @soc_timer_high: high 32bits of synced SOC timer value
 * @global_tsf_low: low 32bits of tsf64
 * @global_tsf_high: high 32bits of tsf64
 *
 * driver use this struct to store the tsf info
 */
struct stsf {
	uint32_t vdev_id;
	uint32_t tsf_low;
	uint32_t tsf_high;
	uint32_t soc_timer_low;
	uint32_t soc_timer_high;
	uint32_t global_tsf_low;
	uint32_t global_tsf_high;
};

#define SIR_BCN_FLT_MAX_ELEMS_IE_LIST 8
/**
 * struct beacon_filter_param - parameters for beacon filtering
 * @vdev_id: vdev id
 * @ie_map: bitwise map of IEs that needs to be filtered
 *
 */
struct beacon_filter_param {
	uint32_t   vdev_id;
	uint32_t   ie_map[SIR_BCN_FLT_MAX_ELEMS_IE_LIST];
};

/**
 * struct adaptive_dwelltime_params - the adaptive dwelltime params
 * @vdev_id: vdev id
 * @is_enabled: Adaptive dwell time is enabled/disabled
 * @dwelltime_mode: global default adaptive dwell mode
 * @lpf_weight: weight to calculate the average low pass
 * filter for channel congestion
 * @passive_mon_intval: intval to monitor wifi activity in passive scan in msec
 * @wifi_act_threshold: % of wifi activity used in passive scan 0-100
 *
 */
struct adaptive_dwelltime_params {
	uint32_t  vdev_id;
	bool      is_enabled;
	uint8_t   dwelltime_mode;
	uint8_t   lpf_weight;
	uint8_t   passive_mon_intval;
	uint8_t   wifi_act_threshold;
};

/**
 * struct csa_offload_params - CSA offload request parameters
 * @channel: channel
 * @switch_mode: switch mode
 * @sec_chan_offset: second channel offset
 * @new_ch_width: new channel width
 * @new_ch_freq_seg1: channel center freq 1
 * @new_ch_freq_seg2: channel center freq 2
 * @ies_present_flag: IE present flag
 */
struct csa_offload_params {
	uint8_t channel;
	uint32_t csa_chan_freq;
	uint8_t switch_mode;
	uint8_t sec_chan_offset;
	uint8_t new_ch_width;
	uint8_t new_op_class;
	uint8_t new_ch_freq_seg1;
	uint8_t new_ch_freq_seg2;
	uint32_t ies_present_flag;
	tSirMacAddr bssId;
};

/**
 * enum obss_ht40_scancmd_type - obss scan command type
 * @HT40_OBSS_SCAN_PARAM_START: OBSS scan start
 * @HT40_OBSS_SCAN_PARAM_UPDATE: OBSS scan param update
 */
enum obss_ht40_scancmd_type {
	HT40_OBSS_SCAN_PARAM_START,
	HT40_OBSS_SCAN_PARAM_UPDATE
};

/**
 * struct sme_obss_ht40_scanind_msg - sme obss scan params
 * @msg_type: message type
 * @length: message length
 * @mac_addr: mac address
 */
struct sme_obss_ht40_scanind_msg {
	uint16_t               msg_type;
	uint16_t               length;
	struct qdf_mac_addr    mac_addr;
};

/**
 * struct obss_ht40_scanind - ht40 obss scan request
 * @cmd: message type
 * @scan_type: message length
 * @obss_passive_dwelltime: obss passive dwelltime
 * @obss_active_dwelltime: obss active dwelltime
 * @obss_width_trigger_interval: scan interval
 * @obss_passive_total_per_channel: total passive scan time per channel
 * @obss_active_total_per_channel: total active scan time per channel
 * @bsswidth_ch_trans_delay: OBSS transition delay time
 * @obss_activity_threshold: OBSS activity threshold
 * @self_sta_idx: self sta identification
 * @bss_id: BSS index
 * @fortymhz_intolerent: Ht40mhz intolerance
 * @channel_count: channel count
 * @chan_freq_list: List of channel frequencies in MHz
 * @current_operatingclass: operating class
 * @iefield_len: ie's length
 * @iefiled: ie's information
 */
struct obss_ht40_scanind {
	enum obss_ht40_scancmd_type cmd;
	enum eSirScanType scan_type;
	/* In TUs */
	uint16_t obss_passive_dwelltime;
	uint16_t obss_active_dwelltime;
	/* In seconds */
	uint16_t obss_width_trigger_interval;
	/* In TU's */
	uint16_t obss_passive_total_per_channel;
	uint16_t obss_active_total_per_channel;
	uint16_t bsswidth_ch_trans_delay;
	uint16_t obss_activity_threshold;
	uint8_t  self_sta_idx;
	uint8_t bss_id;
	uint8_t fortymhz_intolerent;
	uint8_t channel_count;
	uint32_t chan_freq_list[SIR_ROAM_MAX_CHANNELS];
	uint8_t current_operatingclass;
	uint16_t iefield_len;
	uint8_t  iefield[SIR_ROAM_SCAN_MAX_PB_REQ_SIZE];
};

/**
 * struct obss_scanparam - OBSS scan parameters
 * @obss_passive_dwelltime: message type
 * @obss_active_dwelltime: message length
 * @obss_width_trigger_interval: obss passive dwelltime
 * @obss_passive_total_per_channel: obss passive total scan time
 * @obss_active_total_per_channel: obss active total scan time
 * @bsswidth_ch_trans_delay: OBSS transition delay time
 * @obss_activity_threshold: OBSS activity threshold
 */
struct obss_scanparam {
	uint16_t obss_passive_dwelltime;
	uint16_t obss_active_dwelltime;
	uint16_t obss_width_trigger_interval;
	uint16_t obss_passive_total_per_channel;
	uint16_t obss_active_total_per_channel;
	uint16_t bsswidth_ch_trans_delay;
	uint16_t obss_activity_threshold;
};

/**
 * struct sir_apf_set_offload - set apf filter instructions
 * @session_id: session identifier
 * @version: host apf version
 * @filter_id: Filter ID for APF filter
 * @total_length: The total length of the full instruction
 *                total_length equal to 0 means reset
 * @current_offset: current offset, 0 means start a new setting
 * @current_length: Length of current @program
 * @program: APF instructions
 */
struct sir_apf_set_offload {
	uint8_t  session_id;
	uint32_t version;
	uint32_t filter_id;
	uint32_t total_length;
	uint32_t current_offset;
	uint32_t current_length;
	uint8_t  *program;
};

/**
 * struct sir_apf_offload_capabilities - get apf Capabilities
 * @apf_version: fw's implement version
 * @max_apf_filters: max filters that fw supports
 * @max_bytes_for_apf_inst: the max bytes that can be used as apf instructions
 */
struct sir_apf_get_offload {
	uint32_t apf_version;
	uint32_t max_apf_filters;
	uint32_t max_bytes_for_apf_inst;
};

#ifdef WLAN_FEATURE_NAN
#define IFACE_NAME_SIZE 64

/**
 * enum ndp_accept_policy - nan data path accept policy
 * @NDP_ACCEPT_POLICY_NONE: the framework will decide the policy
 * @NDP_ACCEPT_POLICY_ALL: accept policy offloaded to fw
 *
 */
enum ndp_accept_policy {
	NDP_ACCEPT_POLICY_NONE = 0,
	NDP_ACCEPT_POLICY_ALL = 1,
};

/**
 * enum ndp_self_role - nan data path role
 * @NDP_ROLE_INITIATOR: initiator of nan data path request
 * @NDP_ROLE_RESPONDER: responder to nan data path request
 *
 */
enum ndp_self_role {
	NDP_ROLE_INITIATOR = 0,
	NDP_ROLE_RESPONDER = 1,
};

/**
 * enum ndp_response_code - responder's response code to nan data path request
 * @NDP_RESPONSE_ACCEPT: ndp request accepted
 * @NDP_RESPONSE_REJECT: ndp request rejected
 * @NDP_RESPONSE_DEFER: ndp request deferred until later (response to follow
 * any time later)
 *
 */
enum ndp_response_code {
	NDP_RESPONSE_ACCEPT = 0,
	NDP_RESPONSE_REJECT = 1,
	NDP_RESPONSE_DEFER = 2,
};

/**
 * enum ndp_end_type - NDP end type
 * @NDP_END_TYPE_UNSPECIFIED: type is unspecified
 * @NDP_END_TYPE_PEER_UNAVAILABLE: type is peer unavailable
 * @NDP_END_TYPE_OTA_FRAME: NDP end frame received from peer
 *
 */
enum ndp_end_type {
	NDP_END_TYPE_UNSPECIFIED = 0x00,
	NDP_END_TYPE_PEER_UNAVAILABLE = 0x01,
	NDP_END_TYPE_OTA_FRAME = 0x02,
};

/**
 * enum ndp_end_reason_code - NDP end reason code
 * @NDP_END_REASON_UNSPECIFIED: reason is unspecified
 * @NDP_END_REASON_INACTIVITY: reason is peer inactivity
 * @NDP_END_REASON_PEER_DATA_END: data end indication received from peer
 *
 */
enum ndp_end_reason_code {
	NDP_END_REASON_UNSPECIFIED = 0x00,
	NDP_END_REASON_INACTIVITY = 0x01,
	NDP_END_REASON_PEER_DATA_END = 0x02,
};

/**
 * enum nan_status_type - NDP status type
 * @NDP_RSP_STATUS_SUCCESS: request was successful
 * @NDP_RSP_STATUS_ERROR: request failed
 */
enum nan_status_type {
	NDP_RSP_STATUS_SUCCESS = 0x00,
	NDP_RSP_STATUS_ERROR = 0x01,
};

/**
 * enum nan_reason_code - NDP command rsp reason code value
 * @NDP_UNSUPPORTED_CONCURRENCY: Will be used in unsupported concurrency cases
 * @NDP_NAN_DATA_IFACE_CREATE_FAILED: ndi create failed
 * @NDP_NAN_DATA_IFACE_DELETE_FAILED: ndi delete failed
 * @NDP_DATA_INITIATOR_REQ_FAILED: data initiator request failed
 * @NDP_DATA_RESPONDER_REQ_FAILED: data responder request failed
 * @NDP_INVALID_SERVICE_INSTANCE_ID: invalid service instance id
 * @NDP_INVALID_NDP_INSTANCE_ID: invalid ndp instance id
 * @NDP_INVALID_RSP_CODE: invalid response code in ndp responder request
 * @NDP_INVALID_APP_INFO_LEN: invalid app info length
 * @NDP_NMF_REQ_FAIL: OTA nan mgmt frame failure for data request
 * @NDP_NMF_RSP_FAIL: OTA nan mgmt frame failure for data response
 * @NDP_NMF_CNF_FAIL: OTA nan mgmt frame failure for confirm
 * @NDP_END_FAILED: ndp end failed
 * @NDP_NMF_END_REQ_FAIL: OTA nan mgmt frame failure for data end
 * @NDP_VENDOR_SPECIFIC_ERROR: other vendor specific failures
 */
enum nan_reason_code {
	NDP_UNSUPPORTED_CONCURRENCY = 9000,
	NDP_NAN_DATA_IFACE_CREATE_FAILED = 9001,
	NDP_NAN_DATA_IFACE_DELETE_FAILED = 9002,
	NDP_DATA_INITIATOR_REQ_FAILED = 9003,
	NDP_DATA_RESPONDER_REQ_FAILED = 9004,
	NDP_INVALID_SERVICE_INSTANCE_ID = 9005,
	NDP_INVALID_NDP_INSTANCE_ID = 9006,
	NDP_INVALID_RSP_CODE = 9007,
	NDP_INVALID_APP_INFO_LEN = 9008,
	NDP_NMF_REQ_FAIL = 9009,
	NDP_NMF_RSP_FAIL = 9010,
	NDP_NMF_CNF_FAIL = 9011,
	NDP_END_FAILED = 9012,
	NDP_NMF_END_REQ_FAIL = 9013,
	/* 9500 onwards vendor specific error codes */
	NDP_VENDOR_SPECIFIC_ERROR = 9500,
};

/**
 * struct ndp_cfg - ndp configuration
 * @tag: unique identifier
 * @ndp_cfg_len: ndp configuration length
 * @ndp_cfg: variable length ndp configuration
 *
 */
struct ndp_cfg {
	uint32_t tag;
	uint32_t ndp_cfg_len;
	uint8_t *ndp_cfg;
};

/**
 * struct ndp_qos_cfg - ndp qos configuration
 * @tag: unique identifier
 * @ndp_qos_cfg_len: ndp qos configuration length
 * @ndp_qos_cfg: variable length ndp qos configuration
 *
 */
struct ndp_qos_cfg {
	uint32_t tag;
	uint32_t ndp_qos_cfg_len;
	uint8_t ndp_qos_cfg[];
};

/**
 * struct ndp_app_info - application info shared during ndp setup
 * @tag: unique identifier
 * @ndp_app_info_len: ndp app info length
 * @ndp_app_info: variable length application information
 *
 */
struct ndp_app_info {
	uint32_t tag;
	uint32_t ndp_app_info_len;
	uint8_t *ndp_app_info;
};

/**
 * struct ndp_scid - structure to hold sceurity context identifier
 * @scid_len: length of scid
 * @scid: scid
 *
 */
struct ndp_scid {
	uint32_t scid_len;
	uint8_t *scid;
};

/**
 * struct ndp_pmk - structure to hold pairwise master key
 * @pmk_len: length of pairwise master key
 * @pmk: buffer containing pairwise master key
 *
 */
struct ndp_pmk {
	uint32_t pmk_len;
	uint8_t *pmk;
};

/**
 * struct ndi_create_rsp - ndi create response params
 * @status: request status
 * @reason: reason if any
 *
 */
struct ndi_create_rsp {
	uint32_t status;
	uint32_t reason;
	uint8_t sta_id;
};

/**
 * struct ndi_delete_rsp - ndi delete response params
 * @status: request status
 * @reason: reason if any
 *
 */
struct ndi_delete_rsp {
	uint32_t status;
	uint32_t reason;
};

/**
 * struct ndp_initiator_req - ndp initiator request params
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @channel: suggested channel for ndp creation
 * @channel_cfg: channel config, 0=no channel, 1=optional, 2=mandatory
 * @service_instance_id: Service identifier
 * @peer_discovery_mac_addr: Peer's discovery mac address
 * @self_ndi_mac_addr: self NDI mac address
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 * @pmk: pairwise master key
 *
 */
struct ndp_initiator_req {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t channel;
	uint32_t channel_cfg;
	uint32_t service_instance_id;
	struct qdf_mac_addr peer_discovery_mac_addr;
	struct qdf_mac_addr self_ndi_mac_addr;
	struct ndp_cfg ndp_config;
	struct ndp_app_info ndp_info;
	uint32_t ncs_sk_type;
	struct ndp_pmk pmk;
};

/**
 * struct ndp_initiator_rsp - response event from FW
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @ndp_instance_id: locally created NDP instance ID
 * @status: status of the ndp request
 * @reason: reason for failure if any
 *
 */
struct ndp_initiator_rsp {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t ndp_instance_id;
	uint32_t status;
	uint32_t reason;
};

/**
 * struct ndp_indication_event - create ndp indication on the responder
 * @vdev_id: session id of the interface over which ndp is being created
 * @service_instance_id: Service identifier
 * @peer_discovery_mac_addr: Peer's discovery mac address
 * @peer_mac_addr: Peer's NDI mac address
 * @ndp_initiator_mac_addr: NDI mac address of the peer initiating NDP
 * @ndp_instance_id: locally created NDP instance ID
 * @role: self role for NDP
 * @ndp_accept_policy: accept policy configured by the upper layer
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 * @scid: security context identifier
 *
 */
struct ndp_indication_event {
	uint32_t vdev_id;
	uint32_t service_instance_id;
	struct qdf_mac_addr peer_discovery_mac_addr;
	struct qdf_mac_addr peer_mac_addr;
	uint32_t ndp_instance_id;
	enum ndp_self_role role;
	enum ndp_accept_policy policy;
	struct ndp_cfg ndp_config;
	struct ndp_app_info ndp_info;
	uint32_t ncs_sk_type;
	struct ndp_scid scid;
};

/**
 * struct ndp_responder_req - responder's response to ndp create request
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @ndp_instance_id: locally created NDP instance ID
 * @ndp_rsp: response to the ndp create request
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @pmk: pairwise master key
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 *
 */
struct ndp_responder_req {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t ndp_instance_id;
	enum ndp_response_code ndp_rsp;
	struct ndp_cfg ndp_config;
	struct ndp_app_info ndp_info;
	struct ndp_pmk pmk;
	uint32_t ncs_sk_type;
};

/**
 * struct ndp_responder_rsp_event - response to responder's request
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @status: command status
 * @reason: reason for failure if any
 * @peer_mac_addr: Peer's mac address
 * @create_peer: Flag to indicate to create peer
 */
struct ndp_responder_rsp_event {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t status;
	uint32_t reason;
	struct qdf_mac_addr peer_mac_addr;
	bool create_peer;
};

/**
 * struct ndp_confirm_event - ndp confirmation event from FW
 * @vdev_id: session id of the interface over which ndp is being created
 * @ndp_instance_id: ndp instance id for which confirm is being generated
 * @reason_code : reason code(opaque to driver)
 * @num_active_ndps_on_peer: number of ndp instances on peer
 * @peer_ndi_mac_addr: peer NDI mac address
 * @rsp_code: ndp response code
 * @ndp_info: ndp application info
 *
 */
struct ndp_confirm_event {
	uint32_t vdev_id;
	uint32_t ndp_instance_id;
	uint32_t reason_code;
	uint32_t num_active_ndps_on_peer;
	struct qdf_mac_addr peer_ndi_mac_addr;
	enum ndp_response_code rsp_code;
	struct ndp_app_info ndp_info;
};

/**
 * struct ndp_end_req - ndp end request
 * @transaction_id: unique transaction identifier
 * @num_ndp_instances: number of ndp instances to be terminated
 * @ndp_ids: pointer to array of ndp_instance_id to be terminated
 *
 */
struct ndp_end_req {
	uint32_t transaction_id;
	uint32_t num_ndp_instances;
	uint32_t *ndp_ids;
};

/**
 * struct peer_ndp_map  - mapping of NDP instances to peer to VDEV
 * @vdev_id: session id of the interface over which ndp is being created
 * @peer_ndi_mac_addr: peer NDI mac address
 * @num_active_ndp_sessions: number of active NDP sessions on the peer
 * @type: NDP end indication type
 * @reason_code: NDP end indication reason code
 * @ndp_instance_id: NDP instance ID
 *
 */
struct peer_ndp_map {
	uint32_t vdev_id;
	struct qdf_mac_addr peer_ndi_mac_addr;
	uint32_t num_active_ndp_sessions;
	enum ndp_end_type type;
	enum ndp_end_reason_code reason_code;
	uint32_t ndp_instance_id;
};

/**
 * struct ndp_end_rsp_event  - firmware response to ndp end request
 * @transaction_id: unique identifier for the request
 * @status: status of operation
 * @reason: reason(opaque to host driver)
 *
 */
struct ndp_end_rsp_event {
	uint32_t transaction_id;
	uint32_t status;
	uint32_t reason;
};

/**
 * struct ndp_end_indication_event - ndp termination notification from FW
 * @num_ndp_ids: number of NDP ids
 * @ndp_map: mapping of NDP instances to peer and vdev
 *
 */
struct ndp_end_indication_event {
	uint32_t num_ndp_ids;
	struct peer_ndp_map ndp_map[];
};

/**
 * struct ndp_schedule_update_req - ndp schedule update request
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @ndp_instance_id: ndp instance id for which schedule update is requested
 * @ndp_qos: new set of qos parameters
 *
 */
struct ndp_schedule_update_req {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t ndp_instance_id;
	struct ndp_qos_cfg ndp_qos;
};

/**
 * struct ndp_schedule_update_rsp - ndp schedule update response
 * @transaction_id: unique identifier
 * @vdev_id: session id of the interface over which ndp is being created
 * @status: status of the request
 * @reason: reason code for failure if any
 *
 */
struct ndp_schedule_update_rsp {
	uint32_t transaction_id;
	uint32_t vdev_id;
	uint32_t status;
	uint32_t reason;
};

/**
 * struct sme_ndp_peer_ind - ndp peer indication
 * @msg_type: message id
 * @msg_len: message length
 * @session_id: session id
 * @peer_mac_addr: peer mac address
 * @sta_id: station id
 *
 */
struct sme_ndp_peer_ind {
	uint16_t msg_type;
	uint16_t msg_len;
	uint8_t session_id;
	struct qdf_mac_addr peer_mac_addr;
	uint16_t sta_id;
};
#endif /* WLAN_FEATURE_NAN */

/**
 * struct sir_p2p_lo_start - p2p listen offload start
 * @vdev_id: vdev identifier
 * @ctl_flags: control flag
 * @freq: p2p listen frequency
 * @period: listen offload period
 * @interval: listen offload interval
 * @count: number listen offload intervals
 * @device_types: device types
 * @dev_types_len: device types length
 * @probe_resp_tmplt: probe response template
 * @probe_resp_len: probe response template length
 */
struct sir_p2p_lo_start {
	uint32_t vdev_id;
	uint32_t ctl_flags;
	uint32_t freq;
	uint32_t period;
	uint32_t interval;
	uint32_t count;
	uint8_t  *device_types;
	uint32_t dev_types_len;
	uint8_t  *probe_resp_tmplt;
	uint32_t probe_resp_len;
};

/**
 * struct sir_p2p_lo_event - P2P listen offload stop event
 * @vdev_id: vdev identifier
 * @reason_code: P2P listen offload stop reason
 */
struct sir_p2p_lo_event {
	uint32_t vdev_id;
	uint32_t reason_code;
};

/**
 * struct sir_hal_pwr_dbg_cmd - unit test command parameters
 * @pdev_id: pdev id
 * @module_id: module id
 * @num_args: number of arguments
 * @args: arguments
 */
struct sir_mac_pwr_dbg_cmd {
	uint32_t pdev_id;
	uint32_t module_id;
	uint32_t num_args;
	uint32_t args[MAX_POWER_DBG_ARGS_SUPPORTED];
};

/**
 * struct sme_send_disassoc_frm_req - send disassoc request frame
 * @msg_type: message type
 * @length: length of message
 * @vdev_id: vdev id
 * @peer_mac: peer mac address
 * @reason: reason for disassoc
 * @wait_for_ack: wait for acknowledgment
 **/
 struct sme_send_disassoc_frm_req {
	uint16_t msg_type;
	uint16_t length;
	uint8_t vdev_id;
	uint8_t peer_mac[6];
	uint16_t reason;
	uint8_t wait_for_ack;
 };

/**
 * struct sme_update_access_policy_vendor_ie - update vendor ie and access
 * policy
 * @msg_type: message id
 * @msg_len: message length
 * @vdev_id: vdev id
 * @ie: vendor ie
 * @access_policy: access policy for vendor ie
 */
struct sme_update_access_policy_vendor_ie {
	uint16_t msg_type;
	uint16_t length;
	uint32_t vdev_id;
	uint8_t ie[WLAN_MAX_IE_LEN + 2];
	uint8_t access_policy;
};

/**
 * struct sme_tx_fail_cnt_threshold - tx failure count for disconnect to fw
 * @session_id: Session id
 * @tx_fail_cnt_threshold: Tx failure count to do disconnect
 */
struct sme_tx_fail_cnt_threshold {
	uint8_t session_id;
	uint32_t tx_fail_cnt_threshold;
};

/**
 * struct sme_short_retry_limit - transmission retry limit for short frames.
 * @session_id: Session id
 * @short_retry_limit: tranmission retry limit for short frame.
 *
 */
struct sme_short_retry_limit {
	uint8_t session_id;
	uint32_t short_retry_limit;
};

/**
 * struct sme_long_retry_limit - tranmission retry limit for long frames
 * @session_id: Session id
 * @short_retry_limit: tranmission retry limit for long frames.
 *
 */
struct sme_long_retry_limit {
	uint8_t session_id;
	uint32_t long_retry_limit;
};

/**
 * struct sme_addba_accept - Allow/reject the addba request frame
 * @session_id: Session id
 * @addba_accept: Allow/reject the addba request frame
 */
struct sme_addba_accept {
	uint8_t session_id;
	uint8_t addba_accept;
};

/**
 * struct sme_sta_inactivity_timeout - set sta_inactivity_timeout
 * @session_id: session Id.
 * @sta_inactivity_timeout: Timeout to disconnect STA after there
 * is no activity.
 */
struct sme_sta_inactivity_timeout {
	uint8_t session_id;
	uint32_t sta_inactivity_timeout;
};

/*
 * struct wow_pulse_mode - WoW Pulse set cmd struct
 * @wow_pulse_enable: enable or disable this feature
 * @wow_pulse_pin: GPIO PIN for Pulse
 * @wow_pulse_interval_low: Pulse interval low
 * @wow_pulse_interval_high: Pulse interval high
 * @wow_pulse_repeat_count: Pulse repeat count
 * @wow_pulse_init_state: Pulse init level
 *
 * SME uses this structure to configure wow pulse info
 * and send it to WMA
 */
struct wow_pulse_mode {
	bool                       wow_pulse_enable;
	uint8_t                    wow_pulse_pin;
	uint16_t                   wow_pulse_interval_high;
	uint16_t                   wow_pulse_interval_low;
	uint32_t                   wow_pulse_repeat_count;
	uint32_t                   wow_pulse_init_state;
};


/**
 * umac_send_mb_message_to_mac(): post message to umac
 * @msg: opaque message pointer
 *
 * Return: QDF status
 */
QDF_STATUS umac_send_mb_message_to_mac(void *msg);

/**
 * struct scan_chan_info - channel info
 * @freq: radio frequence
 * @cmd flag: cmd flag
 * @noise_floor: noise floor
 * @cycle_count: cycle count
 * @rx_clear_count: rx clear count
 * @tx_frame_count: TX frame count
 * @clock_freq: clock frequence MHZ
 */
struct scan_chan_info {
	uint32_t freq;
	uint32_t cmd_flag;
	uint32_t noise_floor;
	uint32_t cycle_count;
	uint32_t rx_clear_count;
	uint32_t tx_frame_count;
	uint32_t clock_freq;
};

/**
 * enum wow_resume_trigger - resume trigger override setting values
 * @WOW_RESUME_TRIGGER_DEFAULT: fw to use platform default resume trigger
 * @WOW_RESUME_TRIGGER_HTC_WAKEUP: force fw to use HTC Wakeup to resume
 * @WOW_RESUME_TRIGGER_GPIO: force fw to use GPIO to resume
 * @WOW_RESUME_TRIGGER_COUNT: number of resume trigger options
 */
enum wow_resume_trigger {
	/* always first */
	WOW_RESUME_TRIGGER_DEFAULT = 0,
	WOW_RESUME_TRIGGER_HTC_WAKEUP,
	WOW_RESUME_TRIGGER_GPIO,
	/* always last */
	WOW_RESUME_TRIGGER_COUNT
};

/**
 * enum wow_interface_pause - interface pause override setting values
 * @WOW_INTERFACE_PAUSE_DEFAULT: use platform default interface pause setting
 * @WOW_INTERFACE_PAUSE_ENABLE: force interface pause setting to enabled
 * @WOW_INTERFACE_PAUSE_DISABLE: force interface pause setting to disabled
 * @WOW_INTERFACE_PAUSE_COUNT: number of interface pause options
 */
enum wow_interface_pause {
	/* always first */
	WOW_INTERFACE_PAUSE_DEFAULT = 0,
	WOW_INTERFACE_PAUSE_ENABLE,
	WOW_INTERFACE_PAUSE_DISABLE,
	/* always last */
	WOW_INTERFACE_PAUSE_COUNT
};

/**
 * struct wow_enable_params - A collection of wow enable override parameters
 * @is_unit_test: true to notify fw this is a unit-test suspend
 * @interface_pause: used to override the interface pause indication sent to fw
 * @resume_trigger: used to force fw to use a particular resume method
 */
struct wow_enable_params {
	bool is_unit_test;
	enum wow_interface_pause interface_pause;
	enum wow_resume_trigger resume_trigger;
};

#define HE_LTF_1X	0
#define HE_LTF_2X	1
#define HE_LTF_4X	2

#define HE_LTF_ALL	0x7
#define HE_SGI_MASK	0xFF00

#define AUTO_RATE_GI_400NS	8
#define AUTO_RATE_GI_800NS	9
#define AUTO_RATE_GI_1600NS	10
#define AUTO_RATE_GI_3200NS	11

#define AUTO_RATE_LDPC_DIS_BIT	16

#define SET_AUTO_RATE_SGI_VAL(set_val, bit_mask) \
	(set_val = (set_val & HE_LTF_ALL) | bit_mask)

#define SET_AUTO_RATE_HE_LTF_VAL(set_val, bit_mask) \
	(set_val = (set_val & HE_SGI_MASK) | bit_mask)

#define MSCS_OUI_TYPE "\x58"
#define MSCS_OUI_SIZE 1

#ifdef WLAN_FEATURE_11AX
#define HE_CAP_OUI_TYPE "\x23"
#define HE_CAP_OUI_SIZE 1
#define HE_OP_OUI_TYPE "\x24"
#define HE_OP_OUI_SIZE 1

#define HE_RU_ALLOC_INDX0_MASK (0x01 << 0)
#define HE_RU_ALLOC_INDX1_MASK (0x01 << 1)
#define HE_RU_ALLOC_INDX2_MASK (0x01 << 2)
#define HE_RU_ALLOC_INDX3_MASK (0x01 << 3)

/* 3 bits for NSS and 4 bits for RU Index */
#define HE_PPET_NSS_LEN 3
#define HE_PEPT_RU_IDX_LEN 4
#define HE_PPET_NSS_RU_LEN (HE_PPET_NSS_LEN + HE_PEPT_RU_IDX_LEN)
#define HE_PPET_SIZE 3
#define HE_BYTE_SIZE 8

struct ppet_hdr {
	uint8_t nss:3;
	uint8_t ru_idx_mask:4;
	uint8_t remaining:1;
};

/* MAX PPET size = 7 bits + (max_nss X max_ru_number X 6) = 25 bytes */
#define HE_MAX_PPET_SIZE WNI_CFG_HE_PPET_LEN

#define HE_MAX_PHY_CAP_SIZE 3

#define HE_CH_WIDTH_GET_BIT(ch_wd, bit)      (((ch_wd) >> (bit)) & 1)
#define HE_CH_WIDTH_COMBINE(b0, b1, b2, b3, b4, b5, b6)             \
	((uint8_t)(b0) | ((b1) << 1) | ((b2) << 2) |  ((b3) << 3) | \
	((b4) << 4) | ((b5) << 5) | ((b6) << 6))
#define HE_CH_WIDTH_CLR_BIT(ch_wd, bit)      (((ch_wd) >> (bit)) & ~1)

/*
 * MCS values are interpreted as in IEEE 11ax-D1.4 spec onwards
 * +-----------------------------------------------------+
 * |  SS8  |  SS7  |  SS6  | SS5 | SS4 | SS3 | SS2 | SS1 |
 * +-----------------------------------------------------+
 * | 15-14 | 13-12 | 11-10 | 9-8 | 7-6 | 5-4 | 3-2 | 1-0 |
 * +-----------------------------------------------------+
 */
#define HE_MCS_NSS_SHIFT(nss)                 (((nss) - 1) << 1)
#define HE_MCS_MSK_4_NSS(nss)                 (3 << HE_MCS_NSS_SHIFT(nss))
#define HE_MCS_INV_MSK_4_NSS(nss)             (~HE_MCS_MSK_4_NSS(nss))
#define HE_GET_MCS_4_NSS(mcs_set, nss)             \
	(((mcs_set) >> HE_MCS_NSS_SHIFT(nss)) & 3)
#define HE_SET_MCS_4_NSS(mcs_set, mcs, nss)        \
	(((mcs_set) & HE_MCS_INV_MSK_4_NSS(nss)) | \
	((mcs) << HE_MCS_NSS_SHIFT(nss)))
#define HE_MCS_IS_NSS_ENABLED(mcs_set, nss)        \
	((HE_MCS_MSK_4_NSS(nss) & (mcs_set)) != HE_MCS_MSK_4_NSS(nss))

#define HE_MCS_ALL_DISABLED                   0xFFFF

#define HE_MCS_0_7     0x0
#define HE_MCS_0_9     0x1
#define HE_MCS_0_11    0x2
#define HE_MCS_DISABLE 0x3

#define HE_6G_MIN_MPDU_START_SAPCE_BIT_POS 0
#define HE_6G_MAX_AMPDU_LEN_EXP_BIT_POS 3
#define HE_6G_MAX_MPDU_LEN_BIT_POS 6
#define HE_6G_SMPS_BIT_POS 9
#define HE_6G_RD_RESP_BIT_POS 11
#define HE_6G_RX_ANT_PATTERN_BIT_POS 12
#define HE_6G_TX_ANT_PATTERN_BIT_POS 13

/*
 * Following formuala has been arrived at using karnaugh map and unit tested
 * with sample code. Take MCS for each NSS as 2 bit value first and solve for
 * 2 bit intersection of NSS. Use following table/Matrix as guide for solving
 * K-Maps
 * MCS 1\MCS 2    00         01         10         11
 *    00          00         00         00         11
 *    01          00         01         01         11
 *    10          00         01         10         11
 *    11          11         11         11         11
 * if output MCS is o1o0, then as per K-map reduction:
 * o0 = m1.m0 | n1.n0 | (~m1).m0.(n1^n0) | (~n1).n0.(m1^m0)
 * o1 = m1.m0 | n1.n0 | m1.(~m0).n1.(~n0)
 *
 * Please note: Calculating MCS intersection is 80211 protocol specific and
 * should be implemented in PE. WMA can use this macro rather than calling any
 * lim API to do the intersection.
 */
#define HE_INTERSECT_MCS_BITS_PER_NSS(m1, m0, n1, n0)                \
	(((m1 & m0) | (n1 & n0) | (((~m1) & m0) & (n1 ^ n0))  |      \
	(((~n1) & n0) & (m1 ^ m0))) | (((m1 & m0) | (n1 & n0) |      \
	(m1 & ~m0 & n1 & ~n0)) << 1))

/* following takes MCS as 2 bits */
#define HE_INTERSECT_MCS_PER_NSS(mcs_1, mcs_2)                       \
	HE_INTERSECT_MCS_BITS_PER_NSS((mcs_1 >> 1), (mcs_1 & 1),     \
				      (mcs_2 >> 1), (mcs_2 & 1))

/* following takes MCS as 16 bits */
#define HE_INTERSECT_MCS(mcs_1, mcs_2)                             ( \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 1),         \
		HE_GET_MCS_4_NSS(mcs_2, 1)) << HE_MCS_NSS_SHIFT(1) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 2),         \
		HE_GET_MCS_4_NSS(mcs_2, 2)) << HE_MCS_NSS_SHIFT(2) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 3),         \
		HE_GET_MCS_4_NSS(mcs_2, 3)) << HE_MCS_NSS_SHIFT(3) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 4),         \
		HE_GET_MCS_4_NSS(mcs_2, 4)) << HE_MCS_NSS_SHIFT(4) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 5),         \
		HE_GET_MCS_4_NSS(mcs_2, 5)) << HE_MCS_NSS_SHIFT(5) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 6),         \
		HE_GET_MCS_4_NSS(mcs_2, 6)) << HE_MCS_NSS_SHIFT(6) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 7),         \
		HE_GET_MCS_4_NSS(mcs_2, 7)) << HE_MCS_NSS_SHIFT(7) | \
	HE_INTERSECT_MCS_PER_NSS(HE_GET_MCS_4_NSS(mcs_1, 8),         \
		HE_GET_MCS_4_NSS(mcs_2, 8)) << HE_MCS_NSS_SHIFT(8))

/**
 * struct he_capability - to store 11ax HE capabilities
 * @phy_cap: HE PHY capabilities
 * @mac_cap: HE MAC capabilities
 * @mcs: HE MCS
 * @ppet: HE PPE threshold
 */
struct he_capability {
	uint32_t phy_cap[HE_MAX_PHY_CAP_SIZE];
	uint32_t mac_cap;
	uint32_t mcs;
	struct wlan_psoc_host_ppe_threshold ppet;
};
#endif

#define HE_GET_NSS(mcs, nss)                                         \
	do {                                                         \
		(nss) = 0;                                           \
		while ((((mcs) >> ((nss)*2)) & 3) != 3 && nss < 8)   \
			(nss)++;                                     \
	} while (0)

/**
 * struct rsp_stats - arp packet stats
 * @arp_req_enqueue: fw tx count
 * @arp_req_tx_success: tx ack count
 * @arp_req_tx_failure: tx ack fail count
 * @arp_rsp_recvd: rx fw count
 * @out_of_order_arp_rsp_drop_cnt: out of order count
 * @dad_detected: dad detected
 * @connect_status: connection status
 * @ba_session_establishment_status: BA session status
 * @connect_stats_present: connectivity stats present or not
 * @tcp_ack_recvd: tcp syn ack's count
 * @icmpv4_rsp_recvd: icmpv4 responses count
 */
struct rsp_stats {
	uint32_t vdev_id;
	uint32_t arp_req_enqueue;
	uint32_t arp_req_tx_success;
	uint32_t arp_req_tx_failure;
	uint32_t arp_rsp_recvd;
	uint32_t out_of_order_arp_rsp_drop_cnt;
	uint32_t dad_detected;
	uint32_t connect_status;
	uint32_t ba_session_establishment_status;
	bool connect_stats_present;
	uint32_t tcp_ack_recvd;
	uint32_t icmpv4_rsp_recvd;
};

/**
 * struct set_arp_stats_params - set/reset arp stats
 * @vdev_id: session id
 * @flag: enable/disable stats
 * @pkt_type: type of packet(1 - arp)
 * @ip_addr: subnet ipv4 address in case of encrypted packets
 * @pkt_type_bitmap: pkt bitmap
 * @tcp_src_port: tcp src port for pkt tracking
 * @tcp_dst_port: tcp dst port for pkt tracking
 * @icmp_ipv4: target ipv4 address to track ping packets
 * @reserved: reserved
 */
struct set_arp_stats_params {
	uint32_t vdev_id;
	uint8_t flag;
	uint8_t pkt_type;
	uint32_t ip_addr;
	uint32_t pkt_type_bitmap;
	uint32_t tcp_src_port;
	uint32_t tcp_dst_port;
	uint32_t icmp_ipv4;
	uint32_t reserved;
};

/**
 * struct get_arp_stats_params - get arp stats from firmware
 * @pkt_type: packet type(1 - ARP)
 * @vdev_id: session id
 */
struct get_arp_stats_params {
	uint8_t pkt_type;
	uint32_t vdev_id;
};

typedef void (*sme_rcpi_callback)(void *context, struct qdf_mac_addr mac_addr,
				  int32_t rcpi, QDF_STATUS status);
/**
 * struct sme_rcpi_req - structure for querying rcpi info
 * @session_id: session for which rcpi is required
 * @measurement_type: type of measurement from enum rcpi_measurement_type
 * @rcpi_callback: callback function to be invoked for rcpi response
 * @rcpi_context: context info for rcpi callback
 * @mac_addr: peer addr for which rcpi is required
 */
struct sme_rcpi_req {
	uint32_t session_id;
	enum rcpi_measurement_type measurement_type;
	sme_rcpi_callback rcpi_callback;
	void *rcpi_context;
	struct qdf_mac_addr mac_addr;
};

/*
 * @SCAN_REJECT_DEFAULT: default value
 * @CONNECTION_IN_PROGRESS: connection is in progress
 * @REASSOC_IN_PROGRESS: reassociation is in progress
 * @EAPOL_IN_PROGRESS: STA/P2P-CLI is in middle of EAPOL/WPS exchange
 * @SAP_EAPOL_IN_PROGRESS: SAP/P2P-GO is in middle of EAPOL/WPS exchange
 * @SAP_CONNECTION_IN_PROGRESS: SAP/P2P-GO is in middle of connection.
 * @NAN_ENABLE_DISABLE_IN_PROGRESS: NAN is in middle of enable/disable discovery
 */
enum scan_reject_states {
	SCAN_REJECT_DEFAULT = 0,
	CONNECTION_IN_PROGRESS,
	REASSOC_IN_PROGRESS,
	EAPOL_IN_PROGRESS,
	SAP_EAPOL_IN_PROGRESS,
	SAP_CONNECTION_IN_PROGRESS,
	NAN_ENABLE_DISABLE_IN_PROGRESS,
};

/**
 * sir_sme_rx_aggr_hole_ind - sme rx aggr hole indication
 * @hole_cnt: num of holes detected
 * @hole_info_array: hole info
 */
struct sir_sme_rx_aggr_hole_ind {
	uint32_t hole_cnt;
	uint32_t hole_info_array[];
};

/**
 * struct sir_set_rx_reorder_timeout_val - rx reorder timeout
 * @rx_timeout_pri: reorder timeout for AC
 *                  rx_timeout_pri[0] : AC_VO
 *                  rx_timeout_pri[1] : AC_VI
 *                  rx_timeout_pri[2] : AC_BE
 *                  rx_timeout_pri[3] : AC_BK
 */
struct sir_set_rx_reorder_timeout_val {
	uint32_t rx_timeout_pri[4];
};

/**
 * struct sir_peer_set_rx_blocksize - set rx blocksize
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @rx_block_ack_win_limit: windows size limitation
 */
struct sir_peer_set_rx_blocksize {
	uint32_t vdev_id;
	struct qdf_mac_addr peer_macaddr;
	uint32_t rx_block_ack_win_limit;
};

/**
 * struct sir_rssi_disallow_lst - Structure holding Rssi based avoid candidate
 * list
 * @node: Node pointer
 * @bssid: BSSID of the AP
 * @retry_delay: Retry delay received during last rejection in ms
 * @ expected_rssi: RSSI at which STA can initate
 * @time_during_rejection: Timestamp during last rejection in millisec
 * @reject_reason: reason to add the BSSID to BLM
 * @source: Source of adding the BSSID to BLM
 * @original_timeout: original timeout sent by the AP
 * @received_time: Timestamp when the AP was added to the Blacklist
 */
struct sir_rssi_disallow_lst {
	qdf_list_node_t node;
	struct qdf_mac_addr bssid;
	uint32_t retry_delay;
	int8_t expected_rssi;
	qdf_time_t time_during_rejection;
	enum blm_reject_ap_reason reject_reason;
	enum blm_reject_ap_source source;
	uint32_t original_timeout;
	qdf_time_t received_time;
};

/**
 * struct chain_rssi_result - chain rssi result
 * num_chains_valid: valid chain num
 * @chain_rssi: chain rssi result as dBm unit
 * @chain_evm: error vector magnitude
 * @ant_id: antenna id
 */
#define CHAIN_MAX_NUM 8
struct chain_rssi_result {
	uint32_t num_chains_valid;
	uint32_t chain_rssi[CHAIN_MAX_NUM];
	int32_t chain_evm[CHAIN_MAX_NUM];
	uint32_t ant_id[CHAIN_MAX_NUM];
};

/**
 * struct get_chain_rssi_req_params - get chain rssi req params
 * @peer_macaddr: specific peer mac address
 * @session_id: session id
 */
struct get_chain_rssi_req_params {
	struct qdf_mac_addr peer_macaddr;
	uint8_t session_id;
};

/*
 * struct sir_limit_off_chan - limit off-channel command parameters
 * @vdev_id: vdev id
 * @is_tos_active: status of the traffic (active/inactive)
 * @max_off_chan_time: max allowed off channel time
 * @rest_time: home channel time
 * @skip_dfs_chans: skip dfs channels during scan
 */
struct sir_limit_off_chan {
	uint8_t vdev_id;
	bool is_tos_active;
	uint32_t max_off_chan_time;
	uint32_t rest_time;
	bool skip_dfs_chans;
};

typedef void (*roam_scan_stats_cb)(void *context,
				   struct wmi_roam_scan_stats_res *res);

/**
 * struct sir_roam_scan_stats - Stores roam scan context
 * @vdev_id: vdev id
 * @cb: callback to be invoked for roam scan stats response
 * @context: context of callback
 */
struct sir_roam_scan_stats {
	uint32_t vdev_id;
	roam_scan_stats_cb cb;
	void *context;
};

/**
 * struct sae_info - SAE info used for commit/confirm messages
 * @msg_type: Message type
 * @msg_len: length of message
 * @vdev_id: vdev id
 * @peer_mac_addr: peer MAC address
 * @ssid: SSID
 */
struct sir_sae_info {
	uint16_t msg_type;
	uint16_t msg_len;
	uint32_t vdev_id;
	struct qdf_mac_addr peer_mac_addr;
	tSirMacSSid ssid;
};

/**
 * struct sir_sae_msg - SAE msg used for message posting
 * @message_type: message type
 * @length: message length
 * @vdev_id: vdev id
 * @sae_status: SAE status, 0: Success, Non-zero: Failure.
 * @peer_mac_addr: peer MAC address
 */
struct sir_sae_msg {
	uint16_t message_type;
	uint16_t length;
	uint16_t vdev_id;
	uint8_t sae_status;
	tSirMacAddr peer_mac_addr;
};

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * struct sir_md_evt - motion detection event status
 * @vdev_id: vdev id
 * @status: md event status
 */
struct sir_md_evt {
	uint8_t vdev_id;
	uint32_t status;
};

/**
 * struct sir_md_bl_evt - motion detection baseline event values
 * @vdev_id: vdev id
 * @bl_baseline_value: baseline correlation value calculated during baselining
 * @bl_max_corr_reserved: max corr value obtained during baselining phase in %
 * @bl_min_corr_reserved: min corr value obtained during baselining phase in %
 */
struct sir_md_bl_evt {
	uint8_t vdev_id;
	uint32_t bl_baseline_value;
	uint32_t bl_max_corr_reserved;
	uint32_t bl_min_corr_reserved;
};
#endif /* WLAN_FEATURE_MOTION_DETECTION */

#ifdef WLAN_MWS_INFO_DEBUGFS
/**
 * struct sir_get_mws_coex_info - Get MWS coex info
 * @vdev_id: vdev id
 * @cmd_id: wmi mws-coex command IDs
 */
struct sir_get_mws_coex_info {
	uint32_t vdev_id;
	uint32_t cmd_id;
};
#endif /* WLAN_MWS_INFO_DEBUGFS */

/*
 * struct sir_update_session_txq_edca_param
 * @message_type: SME message type
 * @length: size of struct sir_update_session_txq_edca_param
 * @vdev_id: vdev ID
 * @txq_edca_params: txq edca parameter to update
 */
struct sir_update_session_txq_edca_param {
	uint16_t message_type;
	uint16_t length;
	uint8_t vdev_id;
	tSirMacEdcaParamRecord txq_edca_params;
};
#endif /* __SIR_API_H */
