/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains mlme structure definations
 */

#ifndef _WLAN_MLME_STRUCT_H_
#define _WLAN_MLME_STRUCT_H_

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <wmi_unified_param.h>
#include <sir_api.h>
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlme_twt_public_struct.h"
#include "cfg_mlme_generic.h"

#define OWE_TRANSITION_OUI_TYPE "\x50\x6f\x9a\x1c"
#define OWE_TRANSITION_OUI_SIZE 4

#define CFG_VALID_CHANNEL_LIST_LEN              100

#define CFG_PMKID_MODES_OKC                        (0x1)
#define CFG_PMKID_MODES_PMKSA_CACHING              (0x2)

#define CFG_VHT_BASIC_MCS_SET_STADEF    0xFFFE

#define CFG_VHT_RX_MCS_MAP_STAMIN    0
#define CFG_VHT_RX_MCS_MAP_STAMAX    0xFFFF
#define CFG_VHT_RX_MCS_MAP_STADEF    0xFFFE

#define CFG_VHT_TX_MCS_MAP_STAMIN    0
#define CFG_VHT_TX_MCS_MAP_STAMAX    0xFFFF
#define CFG_VHT_TX_MCS_MAP_STADEF    0xFFFE

#define STA_DOT11_MODE_INDX       0
#define P2P_DEV_DOT11_MODE_INDX   4
#define NAN_DISC_DOT11_MODE_INDX  8
#define OCB_DOT11_MODE_INDX       12
#define TDLS_DOT11_MODE_INDX      16
#define NDI_DOT11_MODE_INDX       20

/* Roam debugging related macro defines */
#define MAX_ROAM_DEBUG_BUF_SIZE    250
#define MAX_ROAM_EVENTS_SUPPORTED  5
#define ROAM_FAILURE_BUF_SIZE      60
#define TIME_STRING_LEN            24

#define ROAM_CHANNEL_BUF_SIZE      300
#define LINE_STR "=============================================================="
/*
 * MLME_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF + 1 is
 * assumed to be the default fw supported BF antennas, if fw
 * says it supports 8 antennas in rx ready event and if
 * gTxBFCsnValue INI value is configured above 3, set
 * the same to MLME_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED.
 * Otherwise, fall back and set fw default value[3].
 */
#define MLME_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF 3

#define CFG_STR_DATA_LEN     17
#define CFG_EDCA_DATA_LEN    17
#define CFG_MAX_TX_POWER_2_4_LEN    128
#define CFG_MAX_TX_POWER_5_LEN      256
#define CFG_POWER_USAGE_MAX_LEN      4
#define CFG_MAX_STR_LEN       256
#define MAX_VENDOR_IES_LEN 1532

#define CFG_MAX_PMK_LEN       64

#define CFG_VALID_CHANNEL_LIST_STRING_LEN (CFG_VALID_CHANNEL_LIST_LEN * 4)

#define DEFAULT_ROAM_TRIGGER_BITMAP 0xFFFFFFFF

/**
 * detect AP off based FW reported last RSSI > roaming Low rssi
 * and not less than 20db of host cached RSSI
 */
#define AP_OFF_RSSI_OFFSET 20

/* Default beacon interval of 100 ms */
#define CUSTOM_CONC_GO_BI 100

/**
 * struct mlme_cfg_str - generic structure for all mlme CFG string items
 *
 * @max_len: maximum data length allowed
 * @len: valid no. of elements of the data
 * @data: uint8_t array to store values
 */
struct mlme_cfg_str {
	qdf_size_t max_len;
	qdf_size_t len;
	uint8_t data[CFG_STR_DATA_LEN];
};

/**
 * enum e_edca_type - to index edca params for edca profile
 *			 EDCA profile   AC   unicast/bcast
 * @edca_ani_acbe_local:    ani          BE      unicast
 * @edca_ani_acbk_local:    ani          BK      unicast
 * @edca_ani_acvi_local:    ani          VI      unicast
 * @edca_ani_acvo_local:    ani          VO      unicast
 * @edca_ani_acbe_bcast:    ani          BE      bcast
 * @edca_ani_acbk_bcast:    ani          BK      bcast
 * @edca_ani_acvi_bcast:    ani          VI      bcast
 * @edca_ani_acvo_bcast:    ani          VO      bcast
 * @edca_wme_acbe_local:    wme          BE      unicast
 * @edca_wme_acbk_local:    wme          BK      unicast
 * @edca_wme_acvi_local:    wme          VI      unicast
 * @edca_wme_acvo_local:    wme          VO      unicast
 * @edca_wme_acbe_bcast:    wme          BE      bcast
 * @edca_wme_acbk_bcast:    wme          BK      bcast
 * @edca_wme_acvi_bcast:    wme          VI      bcast
 * @edca_wme_acvo_bcast:    wme          VO      bcast
 * @edca_etsi_acbe_local:   etsi         BE      unicast
 * @edca_etsi_acbk_local:   etsi         BK      unicast
 * @edca_etsi_acvi_local:   etsi         VI      unicast
 * @edca_etsi_acvo_local:   etsi         VO      unicast
 * @edca_etsi_acbe_bcast:   etsi         BE      bcast
 * @edca_etsi_acbk_bcast:   etsi         BK      bcast
 * @edca_etsi_acvi_bcast:   etsi         VI      bcast
 * @edca_etsi_acvo_bcast:   etsi         VO      bcast
 */
enum e_edca_type {
	edca_ani_acbe_local,
	edca_ani_acbk_local,
	edca_ani_acvi_local,
	edca_ani_acvo_local,
	edca_ani_acbe_bcast,
	edca_ani_acbk_bcast,
	edca_ani_acvi_bcast,
	edca_ani_acvo_bcast,
	edca_wme_acbe_local,
	edca_wme_acbk_local,
	edca_wme_acvi_local,
	edca_wme_acvo_local,
	edca_wme_acbe_bcast,
	edca_wme_acbk_bcast,
	edca_wme_acvi_bcast,
	edca_wme_acvo_bcast,
	edca_etsi_acbe_local,
	edca_etsi_acbk_local,
	edca_etsi_acvi_local,
	edca_etsi_acvo_local,
	edca_etsi_acbe_bcast,
	edca_etsi_acbk_bcast,
	edca_etsi_acvi_bcast,
	edca_etsi_acvo_bcast
};

#define CFG_EDCA_PROFILE_ACM_IDX      0
#define CFG_EDCA_PROFILE_AIFSN_IDX    1
#define CFG_EDCA_PROFILE_CWMINA_IDX   2
#define CFG_EDCA_PROFILE_CWMAXA_IDX   4
#define CFG_EDCA_PROFILE_TXOPA_IDX    6
#define CFG_EDCA_PROFILE_CWMINB_IDX   7
#define CFG_EDCA_PROFILE_CWMAXB_IDX   9
#define CFG_EDCA_PROFILE_TXOPB_IDX    11
#define CFG_EDCA_PROFILE_CWMING_IDX   12
#define CFG_EDCA_PROFILE_CWMAXG_IDX   14
#define CFG_EDCA_PROFILE_TXOPG_IDX    16

/**
 * struct mlme_edca_ac_vo - cwmin, cwmax and  aifs value for edca_ac_vo
 *
 * @vo_cwmin: cwmin value for voice
 * @vo_cwmax: cwmax value for voice
 * @vo_aifs: aifs value for voice
 */
struct mlme_edca_ac_vo {
	uint32_t vo_cwmin;
	uint32_t vo_cwmax;
	uint32_t vo_aifs;
};

/**
 * enum dot11_mode - Dot11 mode of the vdev
 * MLME_DOT11_MODE_ALL: vdev supports all dot11 modes
 * MLME_DOT11_MODE_11A: vdev just supports 11A mode
 * MLME_DOT11_MODE_11B: vdev supports 11B mode, and modes above it
 * MLME_DOT11_MODE_11G: vdev supports 11G mode, and modes above it
 * MLME_DOT11_MODE_11N: vdev supports 11N mode, and modes above it
 * MLME_DOT11_MODE_11G_ONLY: vdev just supports 11G mode
 * MLME_DOT11_MODE_1N_ONLYA: vdev just supports 11N mode
 * MLME_DOT11_MODE_11AC: vdev supports 11AC mode, and modes above it
 * MLME_DOT11_MODE_11AC_ONLY: vdev just supports 11AC mode
 * MLME_DOT11_MODE_11AX: vdev supports 11AX mode, and modes above it
 * MLME_DOT11_MODE_11AX_ONLY: vdev just supports 11AX mode
 */
enum mlme_dot11_mode {
	MLME_DOT11_MODE_ALL,
	MLME_DOT11_MODE_11A,
	MLME_DOT11_MODE_11B,
	MLME_DOT11_MODE_11G,
	MLME_DOT11_MODE_11N,
	MLME_DOT11_MODE_11G_ONLY,
	MLME_DOT11_MODE_11N_ONLY,
	MLME_DOT11_MODE_11AC,
	MLME_DOT11_MODE_11AC_ONLY,
	MLME_DOT11_MODE_11AX,
	MLME_DOT11_MODE_11AX_ONLY
};

/**
 * enum mlme_vdev_dot11_mode - Dot11 mode of the vdev
 * MLME_VDEV_DOT11_MODE_AUTO: vdev uses mlme_dot11_mode
 * MLME_VDEV_DOT11_MODE_11N: vdev supports 11N mode
 * MLME_VDEV_DOT11_MODE_11AC: vdev supports 11AC mode
 * MLME_VDEV_DOT11_MODE_11AX: vdev supports 11AX mode
 */
enum mlme_vdev_dot11_mode {
	MLME_VDEV_DOT11_MODE_AUTO,
	MLME_VDEV_DOT11_MODE_11N,
	MLME_VDEV_DOT11_MODE_11AC,
	MLME_VDEV_DOT11_MODE_11AX,
};

/**
 * struct wlan_mlme_dot11_mode - dot11 mode
 *
 * @dot11_mode: dot11 mode supported
 * @vdev_type_dot11_mode: dot11 mode supported by different vdev types
 */
struct wlan_mlme_dot11_mode {
	enum mlme_dot11_mode dot11_mode;
	uint32_t vdev_type_dot11_mode;
};

/**
 * enum roam_invoke_source_entity - Source of invoking roam invoke command.
 * @USERSPACE_INITIATED: Userspace (supplicant)
 * @CONNECTION_MGR_INITIATED: connection mgr initiated.
 */
enum roam_invoke_source_entity {
	USERSPACE_INITIATED,
	CONNECTION_MGR_INITIATED,
};

/**
 * struct mlme_roam_after_data_stall - roam invoke entity params
 * @roam_invoke_in_progress: is roaming already in progress.
 * @source: source of the roam invoke command.
 */
struct mlme_roam_after_data_stall {
	bool roam_invoke_in_progress;
	enum roam_invoke_source_entity source;
};

/**
 * struct mlme_edca_ac_vi - cwmin, cwmax and  aifs value for edca_ac_vi
 *
 * @vi_cwmin: cwmin value for video
 * @vi_cwmax: cwmax value for video
 * @vi_aifs: aifs value for video
 */
struct mlme_edca_ac_vi {
	uint32_t vi_cwmin;
	uint32_t vi_cwmax;
	uint32_t vi_aifs;
};

/**
 * struct mlme_edca_ac_bk - cwmin, cwmax and  aifs value for edca_ac_bk
 *
 * @bk_cwmin: cwmin value for background
 * @bk_cwmax: cwmax value for background
 * @bk_aifs: aifs value for background
 */
struct mlme_edca_ac_bk {
	uint32_t bk_cwmin;
	uint32_t bk_cwmax;
	uint32_t bk_aifs;
};

/**
 * struct mlme_edca_ac_be - cwmin, cwmax and  aifs value for edca_ac_be
 *
 * @be_cwmin: cwmin value for best effort
 * @be_cwmax: cwmax value for best effort
 * @be_aifs: aifs value for best effort
 */
struct mlme_edca_ac_be {
	uint32_t be_cwmin;
	uint32_t be_cwmax;
	uint32_t be_aifs;
};

/**
 * enum mlme_ts_info_ack_policy - TS Info Ack Policy
 * @TS_INFO_ACK_POLICY_NORMAL_ACK:normal ack
 * @TS_INFO_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK: HT immediate block ack
 */
enum mlme_ts_info_ack_policy {
	TS_INFO_ACK_POLICY_NORMAL_ACK = 0,
	TS_INFO_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK = 1,
};

/**
 * struct mlme_edca_params - EDCA pramaters related config items
 *
 * @ani_acbk_l:  EDCA parameters for ANI local access category background
 * @ani_acbe_l:  EDCA parameters for ANI local access category best effort
 * @ani_acvi_l:  EDCA parameters for ANI local access category video
 * @ani_acvo_l:  EDCA parameters for ANI local access category voice
 * @ani_acbk_b:  EDCA parameters for ANI bcast access category background
 * @ani_acbe_b:  EDCA parameters for ANI bcast access category best effort
 * @ani_acvi_b:  EDCA parameters for ANI bcast access category video
 * @ani_acvo_b:  EDCA parameters for ANI bcast access category voice
 * @wme_acbk_l:  EDCA parameters for WME local access category background
 * @wme_acbe_l:  EDCA parameters for WME local access category best effort
 * @wme_acvi_l:  EDCA parameters for WME local access category video
 * @wme_acvo_l:  EDCA parameters for WME local access category voice
 * @wme_acbk_b:  EDCA parameters for WME bcast access category background
 * @wme_acbe_b:  EDCA parameters for WME bcast access category best effort
 * @wme_acvi_b:  EDCA parameters for WME bcast access category video
 * @wme_acvo_b:  EDCA parameters for WME bcast access category voice
 * @etsi_acbk_l: EDCA parameters for ETSI local access category background
 * @etsi_acbe_l: EDCA parameters for ETSI local access category best effort
 * @etsi_acvi_l: EDCA parameters for ETSI local access category video
 * @etsi_acvo_l: EDCA parameters for ETSI local access category voice
 * @etsi_acbk_b: EDCA parameters for ETSI bcast access category background
 * @etsi_acbe_b: EDCA parameters for ETSI bcast access category best effort
 * @etsi_acvi_b: EDCA parameters for ETSI bcast access category video
 * @etsi_acvo_b: EDCA parameters for ETSI bcast access category voice
 * @enable_edca_params: Enable edca parameter
 * @mlme_edca_ac_vo: value for edca_ac_vo
 * @mlme_edca_ac_vi: value for edca_ac_vi
 * @mlme_edca_ac_bk: value for edca_ac_bk
 * @mlme_edca_ac_be: value for edca_ac_be
 */
struct wlan_mlme_edca_params {
	struct mlme_cfg_str ani_acbk_l;
	struct mlme_cfg_str ani_acbe_l;
	struct mlme_cfg_str ani_acvi_l;
	struct mlme_cfg_str ani_acvo_l;
	struct mlme_cfg_str ani_acbk_b;
	struct mlme_cfg_str ani_acbe_b;
	struct mlme_cfg_str ani_acvi_b;
	struct mlme_cfg_str ani_acvo_b;

	struct mlme_cfg_str wme_acbk_l;
	struct mlme_cfg_str wme_acbe_l;
	struct mlme_cfg_str wme_acvi_l;
	struct mlme_cfg_str wme_acvo_l;
	struct mlme_cfg_str wme_acbk_b;
	struct mlme_cfg_str wme_acbe_b;
	struct mlme_cfg_str wme_acvi_b;
	struct mlme_cfg_str wme_acvo_b;

	struct mlme_cfg_str etsi_acbk_l;
	struct mlme_cfg_str etsi_acbe_l;
	struct mlme_cfg_str etsi_acvi_l;
	struct mlme_cfg_str etsi_acvo_l;
	struct mlme_cfg_str etsi_acbk_b;
	struct mlme_cfg_str etsi_acbe_b;
	struct mlme_cfg_str etsi_acvi_b;
	struct mlme_cfg_str etsi_acvo_b;

	bool enable_edca_params;
	struct mlme_edca_ac_vo edca_ac_vo;
	struct mlme_edca_ac_vi edca_ac_vi;
	struct mlme_edca_ac_bk edca_ac_bk;
	struct mlme_edca_ac_be edca_ac_be;
};

#define WLAN_CFG_MFR_NAME_LEN (63)
#define WLAN_CFG_MODEL_NUMBER_LEN (31)
#define WLAN_CFG_MODEL_NAME_LEN (31)
#define WLAN_CFG_MFR_PRODUCT_NAME_LEN (31)
#define WLAN_CFG_MFR_PRODUCT_VERSION_LEN (31)

#define MLME_NUM_WLM_LATENCY_LEVEL 4
#define MLME_RMENABLEDCAP_MAX_LEN  5

/**
 * struct mlme_ht_capabilities_info - HT Capabilities Info
 * @l_sig_tx_op_protection: L-SIG TXOP Protection Mechanism support
 * @stbc_control_frame: STBC Control frame support
 * @psmp: PSMP Support
 * @dsss_cck_mode_40_mhz: To indicate use of DSSS/CCK in 40Mhz
 * @maximal_amsdu_size: Maximum AMSDU Size - 0:3839 octes, 1:7935 octets
 * @delayed_ba: Support of Delayed Block Ack
 * @rx_stbc: Rx STBC Support - 0:Not Supported, 1: 1SS, 2: 1,2SS, 3: 1,2,3SS
 * @tx_stbc: Tx STBC Support
 * @short_gi_40_mhz: Short GI Support for HT40
 * @short_gi_20_mhz: Short GI support for HT20
 * @green_field: Support for HT Greenfield PPDUs
 * @mimo_power_save: SM Power Save Mode - 0:Static, 1:Dynamic, 3:Disabled, 2:Res
 * @supported_channel_width_set: Supported Chan Width - 0:20Mhz, 1:20Mhz & 40Mhz
 * @adv_coding_cap: Rx LDPC support
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_capabilities_info {
	uint16_t l_sig_tx_op_protection:1;
	uint16_t stbc_control_frame:1;
	uint16_t psmp:1;
	uint16_t dsss_cck_mode_40_mhz:1;
	uint16_t maximal_amsdu_size:1;
	uint16_t delayed_ba:1;
	uint16_t rx_stbc:2;
	uint16_t tx_stbc:1;
	uint16_t short_gi_40_mhz:1;
	uint16_t short_gi_20_mhz:1;
	uint16_t green_field:1;
	uint16_t mimo_power_save:2;
	uint16_t supported_channel_width_set:1;
	uint16_t adv_coding_cap:1;
} qdf_packed;
#else
struct mlme_ht_capabilities_info {
	uint16_t adv_coding_cap:1;
	uint16_t supported_channel_width_set:1;
	uint16_t mimo_power_save:2;
	uint16_t green_field:1;
	uint16_t short_gi_20_mhz:1;
	uint16_t short_gi_40_mhz:1;
	uint16_t tx_stbc:1;
	uint16_t rx_stbc:2;
	uint16_t delayed_ba:1;
	uint16_t maximal_amsdu_size:1;
	uint16_t dsss_cck_mode_40_mhz:1;
	uint16_t psmp:1;
	uint16_t stbc_control_frame:1;
	uint16_t l_sig_tx_op_protection:1;
} qdf_packed;
#endif

/**
 * struct mlme_ht_param_info - HT AMPDU Parameters Info
 * @reserved: reserved bits
 * @mpdu_density: MPDU Density
 * @max_rx_ampdu_factor: Max Rx AMPDU Factor
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_param_info {
	uint8_t reserved:3;
	uint8_t mpdu_density:3;
	uint8_t max_rx_ampdu_factor:2;
} qdf_packed;
#else
struct mlme_ht_param_info {
	uint8_t max_rx_ampdu_factor:2;
	uint8_t mpdu_density:3;
	uint8_t reserved:3;
#endif
} qdf_packed;

/**
 * struct mlme_ht_ext_cap_info - Extended HT Capabilities Info
 * reserved_2: Reserved Bits
 * mcs_feedback: MCS Feedback Capability
 * reserved_1: Reserved Bits
 * transition_time: Time needed for transition between 20Mhz and 40 Mhz
 * pco: PCO (Phased Coexistence Operation) Support
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_ext_cap_info {
	uint16_t reserved_2:6;
	uint16_t mcs_feedback:2;
	uint16_t reserved_1:5;
	uint16_t transition_time:2;
	uint16_t pco:1;
} qdf_packed;
#else
struct mlme_ht_ext_cap_info {
	uint16_t pco:1;
	uint16_t transition_time:2;
	uint16_t reserved1:5;
	uint16_t mcs_feedback:2;
	uint16_t reserved2:6;
} qdf_packed;
#endif

/**
 * struct mlme_ht_info_field_1 - Additional HT IE Field1
 * @service_interval_granularity: Shortest Service Interval
 * @controlled_access_only: Access Control for assoc requests
 * @rifs_mode: Reduced Interframe Spacing mode
 * @recommended_tx_width_set: Recommended Tx Channel Width
 * @secondary_channel_offset: Secondary Channel Offset
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_info_field_1 {
	uint8_t service_interval_granularity:3;
	uint8_t controlled_access_only:1;
	uint8_t rifs_mode:1;
	uint8_t recommended_tx_width_set:1;
	uint8_t secondary_channel_offset:2;
} qdf_packed;
#else
struct mlme_ht_info_field_1 {
	uint8_t secondary_channel_offset:2;
	uint8_t recommended_tx_width_set:1;
	uint8_t rifs_mode:1;
	uint8_t controlled_access_only:1;
	uint8_t service_interval_granularity:3;
} qdf_packed;
#endif

/* struct mlme_ht_info_field_2 - Additional HT IE Field2
 * @reserved: reserved bits
 * @obss_non_ht_sta_present: Protection for non-HT STAs by Overlapping BSS
 * @transmit_burst_limit: Transmit Burst Limit
 * @non_gf_devices_present: Non Greenfield devices present
 * @op_mode: Operation Mode
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_info_field_2 {
	uint16_t reserved:11;
	uint16_t obss_non_ht_sta_present:1;
	uint16_t transmit_burst_limit:1;
	uint16_t non_gf_devices_present:1;
	uint16_t op_mode:2;
} qdf_packed;
#else
struct mlme_ht_info_field_2 {
	uint16_t op_mode:2;
	uint16_t non_gf_devices_present:1;
	uint16_t transmit_burst_limit:1;
	uint16_t obss_non_ht_sta_present:1;
	uint16_t reserved:11;
} qdf_packed;
#endif

#ifdef WLAN_FEATURE_FILS_SK

/**
 * struct wlan_fils_connection_info  - Fils connection parameters
 * @is_fils_connection: flag to indicate if the connection is done using
 * authentication algorithm as 4
 * @keyname_nai: key name network access identifier
 * @key_nai_length:  key name network access identifier length
 * @erp_sequence_number: FILS ERP sequence number
 * @r_rk: re-authentication Root Key length
 * @r_rk_length: reauthentication root keys length
 * @rik: Re-authentication integrity key
 * @rik_length: Re-Authentication integrity key length
 * @realm: Realm name
 * @realm_len: Realm length
 * @akm_type: FILS connection akm
 * @auth_type: FILS Authentication Algorithm
 * @pmk: Pairwise master key
 * @pmk_len: Pairwise master key length
 * @pmkid: Pairwise master key ID
 * @fils_ft: FILS FT key
 * @fils_ft_len: Length of FILS FT
 */
struct wlan_fils_connection_info {
	bool is_fils_connection;
	uint8_t keyname_nai[FILS_MAX_KEYNAME_NAI_LENGTH];
	uint32_t key_nai_length;
	uint32_t erp_sequence_number;
	uint8_t r_rk[WLAN_FILS_MAX_RRK_LENGTH];
	uint32_t r_rk_length;
	uint8_t rik[WLAN_FILS_MAX_RIK_LENGTH];
	uint32_t rik_length;
	uint8_t realm[WLAN_FILS_MAX_REALM_LEN];
	uint32_t realm_len;
	uint8_t akm_type;
	uint8_t auth_type;
	uint8_t pmk[MAX_PMK_LEN];
	uint8_t pmk_len;
	uint8_t pmkid[PMKID_LEN];
	uint8_t fils_ft[WLAN_FILS_FT_MAX_LEN];
	uint8_t fils_ft_len;
};
#endif

/**
 * struct mlme_ht_info_field_3 - Additional HT IE Field3
 * @reserved: reserved bits
 * @pco_phase: PCO Phase
 * @pco_active: PCO state
 * @lsig_txop_protection_full_support: L-Sig TXOP Protection Full Support
 * @secondary_beacon: Beacon ID
 * @dual_cts_protection: Dual CTS protection Required
 * @basic_stbc_mcs: Basic STBC MCS
 */
#ifndef ANI_LITTLE_BIT_ENDIAN
struct mlme_ht_info_field_3 {
	uint16_t reserved:4;
	uint16_t pco_phase:1;
	uint16_t pco_active:1;
	uint16_t lsig_txop_protection_full_support:1;
	uint16_t secondary_beacon:1;
	uint16_t dual_cts_protection:1;
	uint16_t basic_stbc_mcs:7;
} qdf_packed;
#else
struct mlme_ht_info_field_3 {
	uint16_t basic_stbc_mcs:7;
	uint16_t dual_cts_protection:1;
	uint16_t secondary_beacon:1;
	uint16_t lsig_txop_protection_full_support:1;
	uint16_t pco_active:1;
	uint16_t pco_phase:1;
	uint16_t reserved:4;
} qdf_packed;
#endif

/**
 * struct wlan_mlme_ht_caps - HT Capabilities related config items
 * @ht_cap_info: HT capabilities Info Structure
 * @ampdu_params: AMPDU parameters
 * @ext_cap_info: HT EXT capabilities info
 * @info_field_1: HT Information Subset 1
 * @info_field_2: HT Information Subset 2
 * @info_field_3: HT Information Subset 3
 * @short_preamble: Short Preamble support
 * @enable_ampdu_ps: Enable AMPDU Power Save
 * @enable_smps: Enabled SM Power Save
 * @smps : SM Power Save mode
 * @max_num_amsdu: Max number of AMSDU
 * @tx_ldpc_enable: Enable Tx LDPC
 * @short_slot_time_enabled: Enabled/disable short slot time
 */
struct wlan_mlme_ht_caps {
	struct mlme_ht_capabilities_info ht_cap_info;
	struct mlme_ht_param_info ampdu_params;
	struct mlme_ht_ext_cap_info ext_cap_info;
	struct mlme_ht_info_field_1 info_field_1;
	struct mlme_ht_info_field_2 info_field_2;
	struct mlme_ht_info_field_3 info_field_3;
	bool short_preamble;
	bool enable_ampdu_ps;
	bool enable_smps;
	uint8_t smps;
	uint8_t max_num_amsdu;
	uint8_t tx_ldpc_enable;
	bool short_slot_time_enabled;
};

#define MLME_CFG_WPS_UUID_MAX_LEN    16
/*
 * struct wlan_mlme_wps_params - All wps based related cfg items
 *
 * @enable_wps - to enable wps
 * @wps_state - current wps state
 * @wps_version - wps version
 * @wps_cfg_method - wps config method
 * @wps_primary_device_category - wps primary device category
 * @wps_primary_device_oui - primary device OUI
 * @wps_device_sub_category - device sub category
 * @wps_device_password_id - password id of device
 * @wps_uuid - wps uuid to be sent in probe
 */
struct wlan_mlme_wps_params {
	uint8_t enable_wps;
	uint8_t wps_state;
	uint8_t wps_version;
	uint32_t wps_cfg_method;
	uint32_t wps_primary_device_category;
	uint32_t wps_primary_device_oui;
	uint16_t wps_device_sub_category;
	uint32_t wps_device_password_id;
	struct mlme_cfg_str wps_uuid;
};

#define MLME_CFG_LISTEN_INTERVAL        1
#define MLME_CFG_BEACON_INTERVAL_DEF    100
#define MLME_CFG_TX_MGMT_RATE_DEF       0xFF
#define MLME_CFG_TX_MGMT_2G_RATE_DEF    0xFF
#define MLME_CFG_TX_MGMT_5G_RATE_DEF    0xFF

/**
 * struct wlan_mlme_cfg_sap - SAP related config items
 * @cfg_ssid: SSID to be configured
 * @beacon_interval: beacon interval
 * @dtim_interval:   dtim interval
 * @listen_interval: listen interval
 * @sap_11g_policy:  Check if 11g support is enabled
 * @assoc_sta_limit: Limit on number of STA associated to SAP
 * @enable_lte_coex: Flag for LTE coexistence
 * @rate_tx_mgmt: mgmt frame tx rate
 * @rate_tx_mgmt_2g: mgmt frame tx rate for 2G band
 * @rate_tx_mgmt_5g: mgmt frame tx rate for 5G band
 * @tele_bcn_wakeup_en: beacon wakeup enable/disable
 * @tele_bcn_max_li: max listen interval
 * @sap_get_peer_info: get peer info
 * @sap_allow_all_chan_param_name: allow all channels
 * @sap_max_no_peers: Maximum number of peers
 * @sap_max_offload_peers: Maximum number of peer offloads
 * @sap_max_offload_reorder_buffs: Maximum offload reorder buffs
 * @sap_ch_switch_beacon_cnt: Number of beacons to be sent out during CSA
 * @sap_internal_restart: flag to check if sap restart is in progress
 * @sap_ch_switch_mode: Channel switch test mode enable/disable
 * @chan_switch_hostapd_rate_enabled_name: enable/disable skip hostapd rate
 * @reduced_beacon_interval: reduced beacon interval value
 * @max_li_modulated_dtim_time: Max modulated DTIM time.
 * @country_code_priority: Country code priority.
 * @sap_pref_chan_location: SAP Preferred channel location.
 * @sap_mcc_chnl_avoid: SAP MCC channel avoidance flag
 * @sap_11ac_override: Overrirde SAP bandwidth to 11ac
 * @go_11ac_override: Override GO bandwidth to 11ac
 * @sap_sae_enabled: enable sae in sap mode
 * @is_sap_bcast_deauth_enabled: enable bcast deauth for sap
 * @is_6g_sap_fd_enabled: enable fils discovery on sap
 */
struct wlan_mlme_cfg_sap {
	uint8_t cfg_ssid[WLAN_SSID_MAX_LEN];
	uint8_t cfg_ssid_len;
	uint16_t beacon_interval;
	uint16_t dtim_interval;
	uint16_t listen_interval;
	bool sap_11g_policy;
	uint8_t assoc_sta_limit;
	bool enable_lte_coex;
	uint8_t rate_tx_mgmt;
	uint8_t rate_tx_mgmt_2g;
	uint8_t rate_tx_mgmt_5g;
	bool tele_bcn_wakeup_en;
	uint8_t tele_bcn_max_li;
	bool sap_get_peer_info;
	bool sap_allow_all_chan_param_name;
	uint8_t sap_max_no_peers;
	uint8_t sap_max_offload_peers;
	uint8_t sap_max_offload_reorder_buffs;
	uint8_t sap_ch_switch_beacon_cnt;
	bool sap_internal_restart;
	bool sap_ch_switch_mode;
	bool chan_switch_hostapd_rate_enabled_name;
	uint8_t reduced_beacon_interval;
	uint8_t max_li_modulated_dtim_time;
	bool country_code_priority;
	uint8_t sap_pref_chan_location;
	bool sap_force_11n_for_11ac;
	bool go_force_11n_for_11ac;
	bool ap_random_bssid_enable;
	uint8_t sap_mcc_chnl_avoid;
	bool sap_11ac_override;
	bool go_11ac_override;
	bool sap_sae_enabled;
	bool is_sap_bcast_deauth_enabled;
	bool is_6g_sap_fd_enabled;
};

/**
 * struct wlan_mlme_dfs_cfg - DFS Capabilities related config items
 * @dfs_master_capable: Is DFS master mode support enabled
 * @dfs_disable_channel_switch: disable channel switch on radar detection
 * @dfs_ignore_cac: Disable cac
 * @dfs_filter_offload: dfs filter offloaad
 * @dfs_beacon_tx_enhanced: enhance dfs beacon tx
 * @dfs_prefer_non_dfs: perefer non dfs channel after radar
 * @dfs_disable_japan_w53: Disable W53 channels
 * @sap_tx_leakage_threshold: sap tx leakage threshold
 * @dfs_pri_multiplier: dfs_pri_multiplier for handle missing pulses
 */
struct wlan_mlme_dfs_cfg {
	bool dfs_master_capable;
	bool dfs_disable_channel_switch;
	bool dfs_ignore_cac;
	bool dfs_filter_offload;
	bool dfs_beacon_tx_enhanced;
	bool dfs_prefer_non_dfs;
	bool dfs_disable_japan_w53;
	uint32_t sap_tx_leakage_threshold;
	uint32_t dfs_pri_multiplier;
};

/**
 * struct wlan_mlme_mbo - Multiband Operation related ini configs
 * @mbo_candidate_rssi_thres: candidate AP's min rssi to accept it
 * @mbo_current_rssi_thres: Connected AP's rssi threshold below which
 * transition is considered
 * @mbo_current_rssi_mcc_thres: connected AP's RSSI threshold value to prefer
 * against MCC
 * @mbo_candidate_rssi_btc_thres: Candidate AP's minimum RSSI threshold to
 * prefer it even in BT coex.
 */
struct wlan_mlme_mbo {
	int8_t mbo_candidate_rssi_thres;
	int8_t mbo_current_rssi_thres;
	int8_t mbo_current_rssi_mcc_thres;
	int8_t mbo_candidate_rssi_btc_thres;
};

/**
 * struct wlan_mlme_powersave - Powersave related ini configs
 * @is_imps_enabled: flag to enable/disable IMPS
 * @is_bmps_enabled: flag to enable/disable BMPS
 * @auto_bmps_timer: auto BMPS timer value
 * @bmps_min_listen_interval: BMPS listen inteval minimum value
 * @bmps_max_listen_interval: BMPS listen interval maximum value
 * @dtim_selection_diversity: dtim selection diversity value to be sent to fw
 */
struct wlan_mlme_powersave {
	bool is_imps_enabled;
	bool is_bmps_enabled;
	uint32_t auto_bmps_timer_val;
	uint32_t bmps_min_listen_interval;
	uint32_t bmps_max_listen_interval;
	uint32_t dtim_selection_diversity;
};

/**
 * struct wlan_mlme_vht_caps - MLME VHT config items
 * @supp_chan_width: Supported Channel Width
 * @ldpc_coding_cap: LDPC Coding Capability
 * @short_gi_80mhz: 80MHz Short Guard Interval
 * @short_gi_160mhz: 160MHz Short Guard Interval
 * @tx_stbc: Tx STBC cap
 * @rx_stbc: Rx STBC cap
 * @su_bformer: SU Beamformer cap
 * @su_bformee: SU Beamformee cap
 * @tx_bfee_ant_supp: Tx beamformee anti supp
 * @num_soundingdim: Number of sounding dimensions
 * @mu_bformer: MU Beamformer cap
 * @txop_ps: Tx OPs in power save
 * @htc_vhtc: htc_vht capability
 * @link_adap_cap: Link adaptation capability
 * @rx_antpattern: Rx Antenna Pattern cap
 * @tx_antpattern: Tx Antenna Pattern cap
 * @rx_mcs_map: Rx MCS Map
 * @tx_mcs_map: Tx MCS Map
 * @rx_supp_data_rate: Rx highest supported data rate
 * @tx_supp_data_rate: Tx highest supported data rate
 * @basic_mcs_set: Basic MCS set
 * @enable_txbf_20mhz: enable tx bf for 20mhz
 * @channel_width: Channel width capability for 11ac
 * @rx_mcs: VHT Rx MCS capability for 1x1 mode
 * @tx_mcs: VHT Tx MCS capability for 1x1 mode
 * @rx_mcs2x2: VHT Rx MCS capability for 2x2 mode
 * @tx_mcs2x2: VHT Tx MCS capability for 2x2 mode
 * @enable_vht20_mcs9: Enables VHT MCS9 in 20M BW operation
 * @enable2x2: Enables/disables VHT Tx/Rx MCS values for 2x2
 * @enable_mu_bformee: Enables/disables multi-user (MU)
 * beam formee capability
 * @enable_paid: Enables/disables paid
 * @enable_gid: Enables/disables gid
 * @b24ghz_band: To control VHT support in 2.4 GHz band
 * @vendor_24ghz_band: to control VHT support based on vendor
 * ie in 2.4 GHz band
 * @ampdu_len_exponent: To handle maximum receive AMPDU ampdu len exponent
 * @ampdu_len: To handle maximum receive AMPDU ampdu len
 * @tx_bfee_sap: enable tx bfee SAp
 * @subfee_vendor_vhtie: enable subfee vendor vht ie
 * @tx_bf_cap: Transmit bf capability
 * @as_cap: Antenna sharing capability info
 * @disable_ldpc_with_txbf_ap: Disable ldpc capability
 * @vht_mcs_10_11_supp: VHT MCS 10 & 11 support
 */
struct mlme_vht_capabilities_info {
	uint8_t supp_chan_width;
	bool ldpc_coding_cap;
	bool short_gi_80mhz;
	bool short_gi_160mhz;
	bool tx_stbc;
	bool rx_stbc;
	bool su_bformer;
	bool su_bformee;
	uint8_t tx_bfee_ant_supp;
	uint8_t num_soundingdim;
	bool mu_bformer;
	bool txop_ps;
	bool htc_vhtc;
	uint8_t link_adap_cap;
	bool rx_antpattern;
	bool tx_antpattern;
	uint32_t rx_mcs_map;
	uint32_t tx_mcs_map;
	uint32_t rx_supp_data_rate;
	uint32_t tx_supp_data_rate;
	uint32_t basic_mcs_set;
	bool enable_txbf_20mhz;
	uint8_t channel_width;
	uint32_t rx_mcs;
	uint32_t tx_mcs;
	uint8_t rx_mcs2x2;
	uint8_t tx_mcs2x2;
	bool enable_vht20_mcs9;
	bool enable2x2;
	bool enable_mu_bformee;
	bool enable_paid;
	bool enable_gid;
	bool b24ghz_band;
	bool vendor_24ghz_band;
	uint8_t ampdu_len_exponent;
	uint8_t ampdu_len;
	bool tx_bfee_sap;
	bool vendor_vhtie;
	uint8_t tx_bf_cap;
	uint8_t as_cap;
	bool disable_ldpc_with_txbf_ap;
	bool vht_mcs_10_11_supp;
	uint8_t extended_nss_bw_supp;
	uint8_t vht_extended_nss_bw_cap;
	uint8_t max_nsts_total;
	bool restricted_80p80_bw_supp;
};

/**
 * struct wlan_mlme_vht_caps - VHT Capabilities related config items
 * @vht_cap_info: VHT capabilities Info Structure
 */
struct wlan_mlme_vht_caps {
	struct mlme_vht_capabilities_info vht_cap_info;
};

/**
 * struct wlan_mlme_qos - QOS TX/RX aggregation related CFG items
 * @tx_aggregation_size: TX aggr size in number of MPDUs
 * @tx_aggregation_size_be: No. of MPDUs for BE queue for TX aggr
 * @tx_aggregation_size_bk: No. of MPDUs for BK queue for TX aggr
 * @tx_aggregation_size_vi: No. of MPDUs for VI queue for TX aggr
 * @tx_aggregation_size_vo: No. of MPDUs for VO queue for TX aggr
 * @rx_aggregation_size: No. of MPDUs for RX aggr
 * @tx_aggr_sw_retry_threshold_be: aggr sw retry threshold for BE
 * @tx_aggr_sw_retry_threshold_bk: aggr sw retry threshold for BK
 * @tx_aggr_sw_retry_threshold_vi: aggr sw retry threshold for VI
 * @tx_aggr_sw_retry_threshold_vo: aggr sw retry threshold for VO
 * @tx_aggr_sw_retry_threshold: aggr sw retry threshold
 * @tx_non_aggr_sw_retry_threshold_be: non aggr sw retry threshold for BE
 * @tx_non_aggr_sw_retry_threshold_bk: non aggr sw retry threshold for BK
 * @tx_non_aggr_sw_retry_threshold_vi: non aggr sw retry threshold for VI
 * @tx_non_aggr_sw_retry_threshold_vo: non aggr sw retry threshold for VO
 * @tx_non_aggr_sw_retry_threshold: non aggr sw retry threshold
 * @sap_max_inactivity_override: Override updating ap_sta_inactivity from
 * hostapd.conf
 * @sap_uapsd_enabled: Flag to enable/disable UAPSD for SAP
 */
struct wlan_mlme_qos {
	uint32_t tx_aggregation_size;
	uint32_t tx_aggregation_size_be;
	uint32_t tx_aggregation_size_bk;
	uint32_t tx_aggregation_size_vi;
	uint32_t tx_aggregation_size_vo;
	uint32_t rx_aggregation_size;
	uint32_t tx_aggr_sw_retry_threshold_be;
	uint32_t tx_aggr_sw_retry_threshold_bk;
	uint32_t tx_aggr_sw_retry_threshold_vi;
	uint32_t tx_aggr_sw_retry_threshold_vo;
	uint32_t tx_aggr_sw_retry_threshold;
	uint32_t tx_non_aggr_sw_retry_threshold_be;
	uint32_t tx_non_aggr_sw_retry_threshold_bk;
	uint32_t tx_non_aggr_sw_retry_threshold_vi;
	uint32_t tx_non_aggr_sw_retry_threshold_vo;
	uint32_t tx_non_aggr_sw_retry_threshold;
	bool sap_max_inactivity_override;
	bool sap_uapsd_enabled;
};

#ifdef WLAN_FEATURE_11AX
#define MLME_HE_PPET_LEN 25
#define WNI_CFG_HE_OPS_BSS_COLOR_MAX 0x3F

/**
 * struct wlan_mlme_he_caps - HE Capabilities related config items
 */
struct wlan_mlme_he_caps {
	tDot11fIEhe_cap dot11_he_cap;
	tDot11fIEhe_cap he_cap_orig;
	uint8_t he_ppet_2g[MLME_HE_PPET_LEN];
	uint8_t he_ppet_5g[MLME_HE_PPET_LEN];
	uint32_t he_ops_basic_mcs_nss;
	uint8_t he_dynamic_fragmentation;
	uint8_t enable_ul_mimo;
	uint8_t enable_ul_ofdm;
	uint32_t he_sta_obsspd;
	uint16_t he_mcs_12_13_supp_2g;
	uint16_t he_mcs_12_13_supp_5g;
	bool enable_6g_sec_check;
};
#endif

/**
 * struct wlan_mlme_chain_cfg - Chain info related structure
 * @max_tx_chains_2g: max tx chains supported in 2.4ghz band
 * @max_rx_chains_2g: max rx chains supported in 2.4ghz band
 * @max_tx_chains_5g: max tx chains supported in 5ghz band
 * @max_rx_chains_5g: max rx chains supported in 5ghz band
 */
struct wlan_mlme_chain_cfg {
	uint8_t max_tx_chains_2g;
	uint8_t max_rx_chains_2g;
	uint8_t max_tx_chains_5g;
	uint8_t max_rx_chains_5g;
};

/**
 * struct mlme_tgt_caps - mlme related capability coming from target (FW)
 * @data_stall_recovery_fw_support: does target supports data stall recovery.
 * @bigtk_support: does the target support bigtk capability or not.
 * @stop_all_host_scan_support: Target capability that indicates if the target
 * supports stop all host scan request type.
 * @peer_create_conf_support: Peer create confirmation command support
 * @dual_sta_roam_fw_support: Firmware support for dual sta roaming feature
 * @ocv_support: FW supports OCV
 *
 * Add all the mlme-tgt related capablities here, and the public API would fill
 * the related capability in the required mlme cfg structure.
 */
struct mlme_tgt_caps {
	bool data_stall_recovery_fw_support;
	bool bigtk_support;
	bool stop_all_host_scan_support;
	bool peer_create_conf_support;
	bool dual_sta_roam_fw_support;
	bool ocv_support;
};

/**
 * struct wlan_mlme_rates - RATES related config items
 * @cfpPeriod: cfp period info
 * @cfpMaxDuration: cfp Max duration info
 * @max_htmcs_txdata: max HT mcs info for Tx
 * @disable_abg_rate_txdata: disable abg rate info for tx data
 * @sap_max_mcs_txdata: sap max mcs info
 * @disable_high_ht_mcs_2x2: disable high mcs for 2x2 info
 * @supported_11b: supported 11B rates
 * @supported_11a: supported 11A rates
 * @supported_mcs_set: supported MCS set
 * @basic_mcs_set: basic MCS set
 * @current_mcs_set: current MCS set
 */
struct wlan_mlme_rates {
	uint8_t cfp_period;
	uint16_t cfp_max_duration;
	uint16_t max_htmcs_txdata;
	bool disable_abg_rate_txdata;
	uint16_t sap_max_mcs_txdata;
	uint8_t disable_high_ht_mcs_2x2;
	struct mlme_cfg_str supported_11b;
	struct mlme_cfg_str supported_11a;
	struct mlme_cfg_str supported_mcs_set;
	struct mlme_cfg_str basic_mcs_set;
	struct mlme_cfg_str current_mcs_set;
};


/* Flags for gLimProtectionControl that is updated in pe session*/
#define MLME_FORCE_POLICY_PROTECTION_DISABLE        0
#define MLME_FORCE_POLICY_PROTECTION_CTS            1
#define MLME_FORCE_POLICY_PROTECTION_RTS            2
#define MLME_FORCE_POLICY_PROTECTION_DUAL_CTS       3
#define MLME_FORCE_POLICY_PROTECTION_RTS_ALWAYS     4
#define MLME_FORCE_POLICY_PROTECTION_AUTO           5

/* protection_enabled bits*/
#define MLME_PROTECTION_ENABLED_FROM_llA            0
#define MLME_PROTECTION_ENABLED_FROM_llB            1
#define MLME_PROTECTION_ENABLED_FROM_llG            2
#define MLME_PROTECTION_ENABLED_HT_20               3
#define MLME_PROTECTION_ENABLED_NON_GF              4
#define MLME_PROTECTION_ENABLED_LSIG_TXOP           5
#define MLME_PROTECTION_ENABLED_RIFS                6
#define MLME_PROTECTION_ENABLED_OBSS                7
#define MLME_PROTECTION_ENABLED_OLBC_FROM_llA       8
#define MLME_PROTECTION_ENABLED_OLBC_FROM_llB       9
#define MLME_PROTECTION_ENABLED_OLBC_FROM_llG       10
#define MLME_PROTECTION_ENABLED_OLBC_HT20           11
#define MLME_PROTECTION_ENABLED_OLBC_NON_GF         12
#define MLME_PROTECTION_ENABLED_OLBC_LSIG_TXOP      13
#define MLME_PROTECTION_ENABLED_OLBC_RIFS           14
#define MLME_PROTECTION_ENABLED_OLBC_OBSS           15

/**
 * struct wlan_mlme_feature_flag - feature related information
 * @accept_short_slot_assoc: enable short sloc feature
 * @enable_hcf: enable HCF feature
 * @enable_rsn: enable RSN for connection
 * @enable_short_preamble_11g: enable short preamble for 11g
 * @enable_short_slot_time_11g
 * @enable_ampdu: enable AMPDU feature
 * @enable_mcc: enable MCC feature
 * @mcc_rts_cts_prot: RTS-CTS protection in MCC
 * @mcc_bcast_prob_rsp: broadcast Probe Response in MCC
 * @channel_bonding_mode: channel bonding mode
 * @enable_block_ack: enable block ack feature
 * @channel_bonding_mode_24ghz: configures Channel Bonding in 24 GHz
 * @channel_bonding_mode_5ghz:  configures Channel Bonding in 5 GHz
 */
struct wlan_mlme_feature_flag {
	bool accept_short_slot_assoc;
	bool enable_hcf;
	bool enable_rsn;
	bool enable_short_preamble_11g;
	bool enable_short_slot_time_11g;
	bool enable_ampdu;
	bool enable_mcc;
	uint8_t mcc_rts_cts_prot;
	uint8_t mcc_bcast_prob_rsp;
	uint32_t channel_bonding_mode;
	uint32_t enable_block_ack;
	uint32_t channel_bonding_mode_24ghz;
	uint32_t channel_bonding_mode_5ghz;
};

/*
 * struct wlan_mlme_sap_protection_cfg - SAP erp protection config items
 * @ignore_peer_ht_opmode:     Ignore the ht opmode of the peer. Dynamic via INI
 * @enable_ap_obss_protection: enable/disable AP OBSS protection
 * @protection_force_policy:   Protection force policy. Static via cfg
 * @is_ap_prot_enabled:        Enable/disable SAP protection
 * @ap_protection_mode:        AP protection bitmap
 * @protection_enabled:        Force enable protection. static via cfg
 */
struct wlan_mlme_sap_protection {
	bool ignore_peer_ht_opmode;
	bool enable_ap_obss_protection;
	uint8_t protection_force_policy;
	bool is_ap_prot_enabled;
	uint16_t ap_protection_mode;
	uint32_t protection_enabled;
};

/*
 * struct wlan_mlme_chainmask - All chainmask related cfg items
 * @txchainmask1x1:     To set transmit chainmask
 * @rxchainmask1x1:     To set rx chainmask
 * @tx_chain_mask_cck:  Used to enable/disable Cck ChainMask
 * @tx_chain_mask_1ss:  Enables/disables tx chain Mask1ss
 * @num_11b_tx_chains:  Number of Tx Chains in 11b mode
 * @num_11ag_tx_chains: Number of Tx Chains in 11ag mode
 * @tx_chain_mask_2g:   Tx chain mask for 2g
 * @rx_chain_mask_2g:   Tx chain mask for 2g
 * @tx_chain_mask_5g:   Tx chain mask for 5g
 * @rx_chain_mask_5g:   Rx chain mask for 5g
 * @enable_bt_chain_separation: Enable/Disable BT/WLAN Host chain seperation
 */
struct wlan_mlme_chainmask {
	uint8_t txchainmask1x1;
	uint8_t rxchainmask1x1;
	bool tx_chain_mask_cck;
	uint8_t tx_chain_mask_1ss;
	uint16_t num_11b_tx_chains;
	uint16_t num_11ag_tx_chains;
	uint8_t tx_chain_mask_2g;
	uint8_t rx_chain_mask_2g;
	uint8_t tx_chain_mask_5g;
	uint8_t rx_chain_mask_5g;
	bool enable_bt_chain_separation;
};

/**
 * enum wlan_mlme_ratemask_type: Type of PHY for ratemask
 * @WLAN_MLME_RATEMASK_TYPE_NO_MASK: no ratemask set
 * @WLAN_MLME_RATEMASK_TYPE_CCK: CCK/OFDM rate
 * @WLAN_MLEM_RATEMASK_TYPE_HT: HT rate
 * @WLAN_MLME_RATEMASK_TYPE_VHT: VHT rate
 * @WLAN_MLME_RATEMASK_TYPE_HE: HE rate
 *
 * This is used for 'type' values in wlan_mlme_ratemask
 */
enum wlan_mlme_ratemask_type {
	WLAN_MLME_RATEMASK_TYPE_NO_MASK  =  0,
	WLAN_MLME_RATEMASK_TYPE_CCK      =  1,
	WLAN_MLME_RATEMASK_TYPE_HT       =  2,
	WLAN_MLME_RATEMASK_TYPE_VHT      =  3,
	WLAN_MLME_RATEMASK_TYPE_HE       =  4,
	/* keep this last */
	WLAN_MLME_RATEMASK_TYPE_MAX,
};

/**
 * struct wlan_mlme_ratemask - ratemask config parameters
 * @type:       Type of PHY the mask to be applied
 * @lower32:    Lower 32 bits in the 1st 64-bit value
 * @higher32:   Higher 32 bits in the 1st 64-bit value
 * @lower32_2:  Lower 32 bits in the 2nd 64-bit value
 * @higher32_2: Higher 32 bits in the 2nd 64-bit value
 */
struct wlan_mlme_ratemask {
	enum wlan_mlme_ratemask_type type;
	uint32_t lower32;
	uint32_t higher32;
	uint32_t lower32_2;
	uint32_t higher32_2;
};

/**
 * enum mlme_cfg_frame_type  - frame type to configure mgmt hw tx retry count
 * @CFG_GO_NEGOTIATION_REQ_FRAME_TYPE: p2p go negotiation request fame
 * @CFG_P2P_INVITATION_REQ_FRAME_TYPE: p2p invitation request frame
 * @CFG_PROVISION_DISCOVERY_REQ_FRAME_TYPE: p2p provision discovery request
 */
enum mlme_cfg_frame_type {
	CFG_GO_NEGOTIATION_REQ_FRAME_TYPE = 0,
	CFG_P2P_INVITATION_REQ_FRAME_TYPE = 1,
	CFG_PROVISION_DISCOVERY_REQ_FRAME_TYPE = 2,
	CFG_FRAME_TYPE_MAX,
};

#define MAX_MGMT_HW_TX_RETRY_COUNT 127

/* struct wlan_mlme_generic - Generic CFG config items
 *
 * @band_capability: HW Band Capability - Both or 2.4G only or 5G only
 * @band: Current Band - Internal variable, initialized to INI and updated later
 * @select_5ghz_margin: RSSI margin to select 5Ghz over 2.4 Ghz
 * @sub_20_chan_width: Sub 20Mhz Channel Width
 * @ito_repeat_count: ITO Repeat Count
 * @pmf_sa_query_max_retries: PMF query max retries for SAP
 * @pmf_sa_query_retry_interval: PMF query retry interval for SAP
 * @dropped_pkt_disconnect_thresh: Threshold for dropped pkts before disconnect
 * @rtt_mac_randomization: Enable/Disable RTT MAC randomization
 * @rtt3_enabled: RTT3 enable or disable info
 * @prevent_link_down: Enable/Disable prevention of link down
 * @memory_deep_sleep: Enable/Disable memory deep sleep
 * @cck_tx_fir_override: Enable/Disable CCK Tx FIR Override
 * @crash_inject: Enable/Disable Crash Inject
 * @lpass_support: Enable/Disable LPASS Support
 * @self_recovery: Enable/Disable Self Recovery
 * @sap_dot11mc: Enable/Disable SAP 802.11mc support
 * @fatal_event_trigger: Enable/Disable Fatal Events Trigger
 * @optimize_ca_event: Enable/Disable Optimization of CA events
 * @fw_timeout_crash: Enable/Disable FW Timeout Crash *
 * @debug_packet_log: Debug packet log flags
 * @enabled_11h: enable 11h flag
 * @enabled_11d: enable 11d flag
 * @enable_beacon_reception_stats: enable beacon reception stats
 * @enable_remove_time_stamp_sync_cmd: Enable remove time stamp sync cmd
 * @data_stall_recovery_fw_support: whether FW supports Data stall recovery.
 * @enable_change_channel_bandwidth: enable/disable change channel bw in mission
 * mode
 * @disable_4way_hs_offload: enable/disable 4 way handshake offload to firmware
 * @as_enabled: antenna sharing enabled or not (FW capability)
 * @mgmt_retry_max: maximum retries for management frame
 * @bmiss_skip_full_scan: Decide if full scan can be skipped in firmware if no
 * candidate is found in partial scan based on channel map
 * @enable_ring_buffer: Decide to enable/disable ring buffer for bug report
 * @enable_peer_unmap_conf_support: Indicate whether to send conf for peer unmap
 * @dfs_chan_ageout_time: Set DFS Channel ageout time
 * @bigtk_support: Whether BIGTK is supported or not
 * @stop_all_host_scan_support: Target capability that indicates if the target
 * supports stop all host scan request type.
 * @dual_sta_roam_fw_support: Firmware support for dual sta roaming feature
 * @sae_connect_retries: sae connect retry bitmask
 * @wls_6ghz_capable: wifi location service(WLS) is 6ghz capable
 * @enabled_rf_test_mode: Enable/disable the RF test mode config
 * @monitor_mode_concurrency: Monitor mode concurrency supported
 * @ocv_support: FW supports OCV or not
 * @tx_retry_multiplier: TX xretry extension parameter
 * @mgmt_hw_tx_retry_count: MGMT HW tx retry count for frames
 */
struct wlan_mlme_generic {
	uint32_t band_capability;
	uint32_t band;
	uint8_t select_5ghz_margin;
	uint8_t sub_20_chan_width;
	uint8_t ito_repeat_count;
	uint8_t pmf_sa_query_max_retries;
	uint16_t pmf_sa_query_retry_interval;
	uint16_t dropped_pkt_disconnect_thresh;
	bool rtt_mac_randomization;
	bool rtt3_enabled;
	bool prevent_link_down;
	bool memory_deep_sleep;
	bool cck_tx_fir_override;
	bool crash_inject;
	bool lpass_support;
	bool self_recovery;
	bool sap_dot11mc;
	bool fatal_event_trigger;
	bool optimize_ca_event;
	bool fw_timeout_crash;
	uint8_t debug_packet_log;
	bool enabled_11h;
	bool enabled_11d;
	bool enable_deauth_to_disassoc_map;
	bool enable_beacon_reception_stats;
	bool enable_remove_time_stamp_sync_cmd;
	bool data_stall_recovery_fw_support;
	uint32_t disable_4way_hs_offload;
	bool as_enabled;
	uint8_t mgmt_retry_max;
	bool bmiss_skip_full_scan;
	bool enable_ring_buffer;
	bool enable_peer_unmap_conf_support;
	uint8_t dfs_chan_ageout_time;
	bool bigtk_support;
	bool stop_all_host_scan_support;
	bool dual_sta_roam_fw_support;
	uint32_t sae_connect_retries;
	bool wls_6ghz_capable;
	bool enabled_rf_test_mode;
	enum monitor_mode_concurrency monitor_mode_concurrency;
	bool ocv_support;
	uint32_t tx_retry_multiplier;
	uint8_t mgmt_hw_tx_retry_count[CFG_FRAME_TYPE_MAX];
};

/*
 * struct wlan_mlme_product_details_cfg - product details config items
 * @manufacturer_name: manufacture name
 * @model_number: model number
 * @model_name: model name
 * @manufacture_product_name: manufacture product name
 * @manufacture_product_version: manufacture product version
 */
struct wlan_mlme_product_details_cfg {
	char manufacturer_name[WLAN_CFG_MFR_NAME_LEN + 1];
	char model_number[WLAN_CFG_MODEL_NUMBER_LEN + 1];
	char model_name[WLAN_CFG_MODEL_NAME_LEN + 1];
	char manufacture_product_name[WLAN_CFG_MFR_PRODUCT_NAME_LEN + 1];
	char manufacture_product_version[WLAN_CFG_MFR_PRODUCT_VERSION_LEN + 1];
};

/*
 * struct acs_weight - Normalize ACS weight for mentioned channels
 * @chan_freq: frequency of the channel
 * @normalize_weight: Normalization factor of the frequency
 */
struct acs_weight {
	uint32_t chan_freq;
	uint8_t normalize_weight;
};

/*
 * struct acs_weight_range - Normalize ACS weight for mentioned channel range
 * @start_freq: frequency of the start channel
 * @end_freq: frequency of the end channel
 * @normalize_weight: Normalization factor for freq range
 */
struct acs_weight_range {
	uint32_t start_freq;
	uint32_t end_freq;
	uint8_t normalize_weight;
};

#define MAX_ACS_WEIGHT_RANGE              10
#define MLME_GET_DFS_CHAN_WEIGHT(np_chan_weight) (np_chan_weight & 0x000000FF)

/*
 * struct wlan_mlme_acs - All acs related cfg items
 * @is_acs_with_more_param - to enable acs with more param
 * @auto_channel_select_weight - to set acs channel weight
 * @is_vendor_acs_support - enable application based channel selection
 * @is_acs_support_for_dfs_ltecoex - enable channel for dfs and lte coex
 * @is_external_acs_policy - control external policy
 * @normalize_weight_chan: Weight factor to be considered in ACS
 * @normalize_weight_num_chan: Number of freq items for normalization.
 * @normalize_weight_range: Frequency range for weight normalization
 * @num_weight_range: num of ranges provided by user
 * @force_sap_start: Force SAP start when no channel is found suitable
 * by ACS
 * @np_chan_weightage: Weightage to be given to non preferred channels.
 */
struct wlan_mlme_acs {
	bool is_acs_with_more_param;
	uint32_t auto_channel_select_weight;
	bool is_vendor_acs_support;
	bool is_acs_support_for_dfs_ltecoex;
	bool is_external_acs_policy;
	struct acs_weight normalize_weight_chan[NUM_CHANNELS];
	uint16_t normalize_weight_num_chan;
	struct acs_weight_range normalize_weight_range[MAX_ACS_WEIGHT_RANGE];
	uint16_t num_weight_range;
	bool force_sap_start;
	uint32_t np_chan_weightage;
};

/*
 * struct wlan_mlme_cfg_twt - All twt related cfg items
 * @is_twt_enabled: global twt configuration
 * @is_bcast_responder_enabled: bcast responder enable/disable
 * @is_bcast_requestor_enabled: bcast requestor enable/disable
 * @bcast_requestor_tgt_cap: Broadcast requestor target capability
 * @bcast_responder_tgt_cap: Broadcast responder target capability
 * @bcast_legacy_tgt_cap: Broadcast Target capability. This is the legacy
 * capability.
 * @is_twt_nudge_tgt_cap_enabled: support for nudge request enable/disable
 * @is_all_twt_tgt_cap_enabled: support for all twt enable/disable
 * @is_twt_statistics_tgt_cap_enabled: support for twt statistics
 * @twt_congestion_timeout: congestion timeout value
 * @enable_twt_24ghz: Enable/disable host TWT when STA is connected in
 * 2.4Ghz
 * @req_flag: requestor flag enable/disable
 * @res_flag: responder flag enable/disable
 * @twt_res_svc_cap: responder service capability
 */
struct wlan_mlme_cfg_twt {
	bool is_twt_enabled;
	bool is_bcast_responder_enabled;
	bool is_bcast_requestor_enabled;
	bool bcast_requestor_tgt_cap;
	bool bcast_responder_tgt_cap;
	bool bcast_legacy_tgt_cap;
	bool is_twt_nudge_tgt_cap_enabled;
	bool is_all_twt_tgt_cap_enabled;
	bool is_twt_statistics_tgt_cap_enabled;
	uint32_t twt_congestion_timeout;
	bool enable_twt_24ghz;
	bool req_flag;
	bool res_flag;
	bool twt_res_svc_cap;
};

/**
 * struct wlan_mlme_obss_ht40 - OBSS HT40 config items
 * @active_dwelltime:        obss active dwelltime
 * @passive_dwelltime:       obss passive dwelltime
 * @width_trigger_interval:  obss trigger interval
 * @passive_per_channel:     obss scan passive total duration per channel
 * @active_per_channel:      obss scan active total duration per channel
 * @width_trans_delay:       obss width transition delay
 * @scan_activity_threshold: obss scan activity threshold
 * @is_override_ht20_40_24g: use channel bonding in 2.4 GHz
 * @obss_detection_offload_enabled:       Enable OBSS detection offload
 * @obss_color_collision_offload_enabled: Enable obss color collision
 * @bss_color_collision_det_sta: STA BSS color collision detection offload
 */
struct wlan_mlme_obss_ht40 {
	uint32_t active_dwelltime;
	uint32_t passive_dwelltime;
	uint32_t width_trigger_interval;
	uint32_t passive_per_channel;
	uint32_t active_per_channel;
	uint32_t width_trans_delay;
	uint32_t scan_activity_threshold;
	bool is_override_ht20_40_24g;
	bool obss_detection_offload_enabled;
	bool obss_color_collision_offload_enabled;
	bool bss_color_collision_det_sta;
};

/**
 * enum dot11p_mode - The 802.11p mode of operation
 * @WLAN_HDD_11P_DISABLED:   802.11p mode is disabled
 * @WLAN_HDD_11P_STANDALONE: 802.11p-only operation
 * @WLAN_HDD_11P_CONCURRENT: 802.11p and WLAN operate concurrently
 */
enum dot11p_mode {
	CFG_11P_DISABLED = 0,
	CFG_11P_STANDALONE,
	CFG_11P_CONCURRENT,
};

#define MAX_VDEV_NSS                2
#define MAX_VDEV_CHAINS             2

/**
 * struct wlan_mlme_nss_chains -     MLME vdev config of nss, and chains
 * @num_tx_chains:                   tx chains of vdev config
 * @num_rx_chains:                   rx chains of vdev config
 * @tx_nss:                          tx nss of vdev config
 * @rx_nss:                          rx nss of vdev config
 * @num_tx_chains_11b:               number of tx chains in 11b mode
 * @num_tx_chains_11g:               number of tx chains in 11g mode
 * @num_tx_chains_11a:               number of tx chains in 11a mode
 * @disable_rx_mrc:                  disable 2 rx chains, in rx nss 1 mode
 * @disable_tx_mrc:                  disable 2 tx chains, in tx nss 1 mode
 * @enable_dynamic_nss_chains_cfg:   enable the dynamic nss chain config to FW
 */
struct wlan_mlme_nss_chains {
	uint32_t num_tx_chains[NSS_CHAINS_BAND_MAX];
	uint32_t num_rx_chains[NSS_CHAINS_BAND_MAX];
	uint32_t tx_nss[NSS_CHAINS_BAND_MAX];
	uint32_t rx_nss[NSS_CHAINS_BAND_MAX];
	uint32_t num_tx_chains_11b;
	uint32_t num_tx_chains_11g;
	uint32_t num_tx_chains_11a;
	bool disable_rx_mrc[NSS_CHAINS_BAND_MAX];
	bool disable_tx_mrc[NSS_CHAINS_BAND_MAX];
	bool enable_dynamic_nss_chains_cfg;
};

/**
 * enum station_keepalive_method - available keepalive methods for stations
 * @MLME_STA_KEEPALIVE_NULL_DATA: null data packet
 * @MLME_STA_KEEPALIVE_GRAT_ARP: gratuitous ARP packet
 * @MLME_STA_KEEPALIVE_COUNT: number of method options available
 */
enum station_keepalive_method {
	MLME_STA_KEEPALIVE_NULL_DATA,
	MLME_STA_KEEPALIVE_GRAT_ARP,
	/* keep at the end */
	MLME_STA_KEEPALIVE_COUNT
};

/**
 * struct wlan_mlme_sta_cfg - MLME STA configuration items
 * @sta_keep_alive_period:          Sends NULL frame to AP period
 * @tgt_gtx_usr_cfg:                Target gtx user config
 * @pmkid_modes:                    Enable PMKID modes
 * @wait_cnf_timeout:               Wait assoc cnf timeout
 * @sta_miracast_mcc_rest_time:     STA+MIRACAST(P2P) MCC rest time
 * @dot11p_mode:                    Set 802.11p mode
 * @fils_max_chan_guard_time:       Set maximum channel guard time
 * @current_rssi:                   Current rssi
 * @deauth_retry_cnt:               Deauth retry count
 * @ignore_peer_erp_info:           Ignore peer infrormation
 * @sta_prefer_80mhz_over_160mhz:   Set Sta preference to connect in 80HZ/160HZ
 * @enable_5g_ebt:                  Set default 5G early beacon termination
 * @deauth_before_connection:       Send deauth before connection or not
 * @enable_go_cts2self_for_sta:     Stop NOA and start using cts2self
 * @qcn_ie_support:                 QCN IE support
 * @force_rsne_override:            Force rsnie override from user
 * @single_tid:                     Set replay counter for all TID
 * @allow_tpc_from_ap:              Support for AP power constraint
 * @usr_disabled_roaming:           User config for roaming disable
 */
struct wlan_mlme_sta_cfg {
	uint32_t sta_keep_alive_period;
	uint32_t tgt_gtx_usr_cfg;
	uint32_t pmkid_modes;
	uint32_t wait_cnf_timeout;
	uint32_t sta_miracast_mcc_rest_time;
	enum dot11p_mode dot11p_mode;
	uint8_t fils_max_chan_guard_time;
	uint8_t current_rssi;
	uint8_t deauth_retry_cnt;
	bool ignore_peer_erp_info;
	bool sta_prefer_80mhz_over_160mhz;
	bool enable_5g_ebt;
	bool deauth_before_connection;
	bool enable_go_cts2self_for_sta;
	bool qcn_ie_support;
	bool single_tid;
	bool allow_tpc_from_ap;
	enum station_keepalive_method sta_keepalive_method;
	bool usr_disabled_roaming;
};

/**
 * struct wlan_mlme_stats_cfg - MLME stats configuration items
 * @stats_periodic_display_time: time after which stats will be printed
 * @stats_link_speed_rssi_high: rssi link speed, high
 * @stats_link_speed_rssi_med: medium rssi link speed
 * @stats_link_speed_rssi_low: rssi link speed, low
 * @stats_report_max_link_speed_rssi: report speed limit
 */
struct wlan_mlme_stats_cfg {
	uint32_t stats_periodic_display_time;
	int stats_link_speed_rssi_high;
	int stats_link_speed_rssi_med;
	int stats_link_speed_rssi_low;
	uint32_t stats_report_max_link_speed_rssi;
};

/**
 * enum roaming_dfs_channel_type - Allow dfs channel in roam
 * @CFG_ROAMING_DFS_CHANNEL_DISABLED:   Disallow dfs channel in roam
 * @CFG_ROAMING_DFS_CHANNEL_ENABLED_NORMAL: Allow dfs channel
 * @CFG_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE: Allow dfs channel with active scan
 */
enum roaming_dfs_channel_type {
	ROAMING_DFS_CHANNEL_DISABLED,
	ROAMING_DFS_CHANNEL_ENABLED_NORMAL,
	ROAMING_DFS_CHANNEL_ENABLED_ACTIVE,
};

/*
 * struct bss_load_trigger - parameters related to bss load triggered roam
 * @enabled: flag to check if this trigger is enabled/disabled
 * @threshold: Bss load threshold value above which roaming should start
 * @sample_time: Time duration in milliseconds for which the bss load value
 * should be monitored
 * @rssi_threshold_5ghz: RSSI threshold of the current connected AP below which
 * roam should be triggered if bss load threshold exceeds the configured value.
 * This value is applicable only when we are connected in 5GHz band.
 * @rssi_threshold_24ghz: RSSI threshold of the current connected AP below which
 * roam should be triggered if bss load threshold exceeds the configured value.
 * This value is applicable only when we are connected in 2.4 GHz band.
 */
struct bss_load_trigger {
	bool enabled;
	uint32_t threshold;
	uint32_t sample_time;
	int32_t rssi_threshold_5ghz;
	int32_t rssi_threshold_24ghz;
};

/*
 * AKM suites supported by firmware for roaming
 */
#define AKM_FT_SAE           0
#define AKM_FT_SUITEB_SHA384 1
#define AKM_FT_FILS          2
#define AKM_SAE              3
#define AKM_OWE              4
#define AKM_SUITEB           5

#define LFR3_STA_ROAM_DISABLE_BY_P2P BIT(0)
#define LFR3_STA_ROAM_DISABLE_BY_NAN BIT(1)

/**
 * struct fw_scan_channels  - Channel details part of VDEV set PCL command
 * @num_channels: Number of channels
 * @ch_freq_list: Channel Frequency list
 */
struct fw_scan_channels {
	uint8_t num_channels;
	uint32_t freq[NUM_CHANNELS];
};

/*
 * @mawc_roam_enabled:              Enable/Disable MAWC during roaming
 * @enable_fast_roam_in_concurrency:Enable LFR roaming on STA during concurrency
 * @vendor_btm_param:               Vendor WTC roam trigger parameters
 * @roam_rt_stats:                  Roam event stats vendor command parameters
 * @lfr3_roaming_offload:           Enable/disable roam offload feature
 * @lfr3_dual_sta_roaming_enabled:  Enable/Disable dual sta roaming offload
 * feature
 * @enable_self_bss_roam:               enable roaming to connected BSSID
 * @enable_disconnect_roam_offload: enable disassoc/deauth roam scan.
 * @enable_idle_roam: flag to enable/disable idle roam in fw
 * @idle_roam_rssi_delta: rssi delta of connected ap which is used to
 * identify if the AP is idle or in motion
 * @idle_roam_inactive_time: Timeout value in seconds, above which the
 * connection is idle
 * @idle_data_packet_count: data packet count measured during inactive time,
 * below which the connection is idle.
 * @idle_roam_min_rssi: Minimum rssi of connected AP to be considered for
 * idle roam trigger.
 * @enable_roam_reason_vsie:        Enable/disable incluison of roam reason
 * vsie in Re(assoc) frame
 * @roam_trigger_bitmap:            Bitmap of roaming triggers.
 * @sta_roam_disable                STA roaming disabled by interfaces
 * @early_stop_scan_enable:         Set early stop scan
 * @enable_5g_band_pref:            Enable preference for 5G from INI
 * @ese_enabled:                    Enable ESE feature
 * @lfr_enabled:                    Enable fast roaming
 * @mawc_enabled:                   Enable MAWC
 * @fast_transition_enabled:        Enable fast transition
 * @wes_mode_enabled:               Enable WES mode
 * @mawc_roam_traffic_threshold:    Configure traffic threshold
 * @mawc_roam_ap_rssi_threshold:    Best AP RSSI threshold
 * @mawc_roam_rssi_high_adjust:     Adjust MAWC roam high RSSI
 * @mawc_roam_rssi_low_adjust:      Adjust MAWC roam low RSSI
 * @roam_rssi_abs_threshold:        The min RSSI of the candidate AP
 * @rssi_threshold_offset_5g:       Lookup threshold offset for 5G band
 * @early_stop_scan_min_threshold:  Set early stop scan min
 * @early_stop_scan_max_threshold:  Set early stop scan max
 * @roam_dense_traffic_threshold:   Dense traffic threshold
 * @roam_dense_rssi_thre_offset:    Sets dense roam RSSI threshold diff
 * @roam_dense_min_aps:             Sets minimum number of AP for dense roam
 * @roam_bg_scan_bad_rssi_threshold:RSSI threshold for background roam
 * @roam_bg_scan_client_bitmap:     Bitmap used to identify the scan clients
 * @roam_bg_scan_bad_rssi_offset_2g:RSSI threshold offset for 2G to 5G roam
 * @roam_data_rssi_threshold_triggers: triggers of bad data RSSI threshold to
 *                                  roam
 * @roam_data_rssi_threshold: Bad data RSSI threshold to roam
 * @rx_data_inactivity_time: Rx duration to check data RSSI
 * @adaptive_roamscan_dwell_mode:   Sets dwell time adaptive mode
 * @per_roam_enable:                To enabled/disable PER based roaming in FW
 * @per_roam_config_high_rate_th:   Rate at which PER based roam will stop
 * @per_roam_config_low_rate_th:    Rate at which PER based roam will start
 * @per_roam_config_rate_th_percent:Percentage at which FW will issue roam scan
 * @per_roam_rest_time:             FW will wait once it issues a roam scan.
 * @per_roam_monitor_time:          Min time to be considered as valid scenario
 * @per_roam_min_candidate_rssi:    Min roamable AP RSSI for candidate selection
 * @lfr3_disallow_duration:         Disallow duration before roaming
 * @lfr3_rssi_channel_penalization: RSSI penalization
 * @lfr3_num_disallowed_aps:        Max number of AP's to maintain in LCA list
 * @rssi_boost_threshold_5g:        Boost threshold above which 5 GHz is favored
 * @rssi_boost_factor_5g:           Factor by which 5GHz RSSI is boosted
 * @max_rssi_boost_5g:              Maximum boost that can be applied to 5G RSSI
 * @rssi_penalize_threshold_5g:     Penalize thres above which 5G isn't favored
 * @rssi_penalize_factor_5g:        Factor by which 5GHz RSSI is penalizeed
 * @max_rssi_penalize_5g:           Max penalty that can be applied to 5G RSSI
 * @max_num_pre_auth:               Configure max number of pre-auth
 * @roam_preauth_retry_count:       Configure the max number of preauth retry
 * @roam_preauth_no_ack_timeout:    Configure the no ack timeout period
 * @roam_rssi_diff:                 Enable roam based on rssi
 * @roam_scan_offload_enabled:      Enable Roam Scan Offload
 * @neighbor_scan_timer_period:     Neighbor scan timer period
 * @neighbor_scan_min_timer_period: Min neighbor scan timer period
 * @neighbor_lookup_rssi_threshold: Neighbor lookup rssi threshold
 * @opportunistic_scan_threshold_diff: Set oppurtunistic threshold diff
 * @roam_rescan_rssi_diff:          Sets RSSI for Scan trigger in firmware
 * @neighbor_scan_min_chan_time:    Neighbor scan channel min time
 * @neighbor_scan_max_chan_time:    Neighbor scan channel max time
 * @neighbor_scan_results_refresh_period: Neighbor scan refresh period
 * @empty_scan_refresh_period:      Empty scan refresh period
 * @roam_bmiss_first_bcnt:          First beacon miss count
 * @roam_bmiss_final_bcnt:          Final beacon miss count
 * @roam_beacon_rssi_weight:        Beacon miss weight
 * @roaming_dfs_channel:            Allow dfs channel in roam
 * @roam_scan_hi_rssi_maxcount:     5GHz maximum scan count
 * @roam_scan_hi_rssi_delta:        RSSI Delta for scan trigger
 * @roam_scan_hi_rssi_delay:        Minimum delay between 5GHz scans
 * @roam_scan_hi_rssi_ub:           Upper bound after which 5GHz scan
 * @roam_prefer_5ghz:               Prefer roaming to 5GHz Bss
 * @roam_intra_band:                Prefer roaming within Band
 * @enable_adaptive_11r             Flag to check if adaptive 11r ini is enabled
 * @tgt_adaptive_11r_cap:           Flag to check if target supports adaptive
 * 11r
 * @enable_ft_im_roaming:           Flag to enable/disable FT-IM roaming
 * @roam_scan_home_away_time:       The home away time to firmware
 * @roam_scan_n_probes:    The number of probes to be sent for firmware roaming
 * @delay_before_vdev_stop:Wait time for tx complete before vdev stop
 * @neighbor_scan_channel_list:     Neighbor scan channel list
 * @neighbor_scan_channel_list_num: Neighbor scan channel list number
 * @enable_lfr_subnet_detection:    Enable LFR3 subnet detection
 * @ho_delay_for_rx:                Delay hand-off by this duration to receive
 * @min_delay_btw_roam_scans:       Min duration
 * @roam_trigger_reason_bitmask:    Contains roam_trigger_reasons
 * @enable_ftopen:                  Enable/disable FT open feature
 * @roam_force_rssi_trigger:        Force RSSI trigger or not
 * @roaming_scan_policy:            Config roaming scan policy in fw
 * @roam_scan_inactivity_time:         Device inactivity monitoring time in
 * milliseconds for which the device is considered to be inactive.
 * @roam_inactive_data_packet_count:   Maximum allowed data packets count
 * during roam_scan_inactivity_time.
 * @roam_scan_period_after_inactivity: Roam scan period after device was in
 * inactive state
 * @fw_akm_bitmap:                  Supported Akm suites of firmware
 * @roam_full_scan_period: Idle period in seconds between two successive
 * full channel roam scans
 * @saved_freq_list: Valid channel list
 * @sae_single_pmk_feature_enabled: Contains value of ini
 * sae_single_pmk_feature_enabled
 */
struct wlan_mlme_lfr_cfg {
	bool mawc_roam_enabled;
	bool enable_fast_roam_in_concurrency;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	struct wlan_cm_roam_vendor_btm_params vendor_btm_param;
	struct wlan_cm_roam_rt_stats roam_rt_stats;
	bool lfr3_roaming_offload;
	bool lfr3_dual_sta_roaming_enabled;
	bool enable_self_bss_roam;
	bool enable_disconnect_roam_offload;
	bool enable_idle_roam;
	uint32_t idle_roam_rssi_delta;
	uint32_t idle_roam_inactive_time;
	uint32_t idle_data_packet_count;
	uint32_t idle_roam_band;
	int32_t idle_roam_min_rssi;
	bool enable_roam_reason_vsie;
	uint32_t roam_trigger_bitmap;
	uint32_t sta_roam_disable;
#endif
	bool early_stop_scan_enable;
	bool enable_5g_band_pref;
#ifdef FEATURE_WLAN_ESE
	bool ese_enabled;
#endif
	bool lfr_enabled;
	bool mawc_enabled;
	bool fast_transition_enabled;
	bool wes_mode_enabled;
	uint32_t mawc_roam_traffic_threshold;
	uint32_t mawc_roam_ap_rssi_threshold;
	uint8_t mawc_roam_rssi_high_adjust;
	uint8_t mawc_roam_rssi_low_adjust;
	uint32_t roam_rssi_abs_threshold;
	uint8_t rssi_threshold_offset_5g;
	uint8_t early_stop_scan_min_threshold;
	uint8_t early_stop_scan_max_threshold;
	uint32_t roam_dense_traffic_threshold;
	uint32_t roam_dense_rssi_thre_offset;
	uint32_t roam_dense_min_aps;
	uint32_t roam_bg_scan_bad_rssi_threshold;
	uint32_t roam_bg_scan_client_bitmap;
	uint32_t roam_bg_scan_bad_rssi_offset_2g;
	uint32_t roam_data_rssi_threshold_triggers;
	int32_t roam_data_rssi_threshold;
	uint32_t rx_data_inactivity_time;
	uint32_t adaptive_roamscan_dwell_mode;
	uint32_t per_roam_enable;
	uint32_t per_roam_config_high_rate_th;
	uint32_t per_roam_config_low_rate_th;
	uint32_t per_roam_config_rate_th_percent;
	uint32_t per_roam_rest_time;
	uint32_t per_roam_monitor_time;
	uint32_t per_roam_min_candidate_rssi;
	uint32_t lfr3_disallow_duration;
	uint32_t lfr3_rssi_channel_penalization;
	uint32_t lfr3_num_disallowed_aps;
	uint32_t rssi_boost_threshold_5g;
	uint32_t rssi_boost_factor_5g;
	uint32_t max_rssi_boost_5g;
	uint32_t rssi_penalize_threshold_5g;
	uint32_t rssi_penalize_factor_5g;
	uint32_t max_rssi_penalize_5g;
	uint32_t max_num_pre_auth;
	uint32_t roam_preauth_retry_count;
	uint32_t roam_preauth_no_ack_timeout;
	uint8_t roam_rssi_diff;
	uint8_t bg_rssi_threshold;
	bool roam_scan_offload_enabled;
	uint32_t neighbor_scan_timer_period;
	uint32_t neighbor_scan_min_timer_period;
	uint32_t neighbor_lookup_rssi_threshold;
	uint32_t opportunistic_scan_threshold_diff;
	uint32_t roam_rescan_rssi_diff;
	uint16_t neighbor_scan_min_chan_time;
	uint16_t neighbor_scan_max_chan_time;
	uint32_t neighbor_scan_results_refresh_period;
	uint32_t empty_scan_refresh_period;
	uint8_t roam_bmiss_first_bcnt;
	uint8_t roam_bmiss_final_bcnt;
	enum roaming_dfs_channel_type roaming_dfs_channel;
	uint32_t roam_scan_hi_rssi_maxcount;
	uint32_t roam_scan_hi_rssi_delta;
	uint32_t roam_scan_hi_rssi_delay;
	uint32_t roam_scan_hi_rssi_ub;
	bool roam_prefer_5ghz;
	bool roam_intra_band;
#ifdef WLAN_ADAPTIVE_11R
	bool enable_adaptive_11r;
	bool tgt_adaptive_11r_cap;
#endif
	bool enable_ft_im_roaming;
	uint16_t roam_scan_home_away_time;
	uint32_t roam_scan_n_probes;
	uint8_t delay_before_vdev_stop;
	uint8_t neighbor_scan_channel_list[CFG_VALID_CHANNEL_LIST_LEN];
	uint8_t neighbor_scan_channel_list_num;
#ifdef FEATURE_LFR_SUBNET_DETECTION
	bool enable_lfr_subnet_detection;
#endif
	uint8_t ho_delay_for_rx;
	uint8_t min_delay_btw_roam_scans;
	uint32_t roam_trigger_reason_bitmask;
	bool enable_ftopen;
	bool roam_force_rssi_trigger;
	struct bss_load_trigger bss_load_trig;
	bool roaming_scan_policy;
	uint32_t roam_scan_inactivity_time;
	uint32_t roam_inactive_data_packet_count;
	uint32_t roam_scan_period_after_inactivity;
	uint32_t fw_akm_bitmap;
	uint32_t roam_full_scan_period;
	struct fw_scan_channels saved_freq_list;
#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
	bool sae_single_pmk_feature_enabled;
#endif
};

/**
 * struct wlan_mlme_wmm_config - WMM configuration
 * @wmm_mode: Enable WMM feature
 * @b80211e_is_enabled: Enable 802.11e feature
 * @uapsd_mask: what ACs to setup U-APSD for at assoc
 * @bimplicit_qos_enabled: Enable implicit QOS
 */
struct wlan_mlme_wmm_config {
	uint8_t wmm_mode;
	bool b80211e_is_enabled;
	uint8_t uapsd_mask;
	bool bimplicit_qos_enabled;
};

/**
 * struct wlan_mlme_wmm_tspec_element - Default TSPEC parameters
 * from the wmm spec
 * @inactivity_interval: inactivity_interval as per wmm spec
 * @burst_size_def: TS burst size
 * @ts_ack_policy: TS Info ACK policy
 * @ts_acm_is_off: ACM is off for AC
 */
struct wlan_mlme_wmm_tspec_element {
#ifdef FEATURE_WLAN_ESE
	uint32_t inactivity_intv;
#endif
	bool burst_size_def;
	enum mlme_ts_info_ack_policy ts_ack_policy;
	bool ts_acm_is_off;
};

/**
 * struct wlan_mlme_wmm_ac_vo - Default TSPEC parameters
 * for AC_VO
 * @dir_ac_vo: TSPEC direction for VO
 * @nom_msdu_size_ac_vo: normal MSDU size for VO
 * @mean_data_rate_ac_vo: mean data rate for VO
 * @min_phy_rate_ac_vo: min PHY rate for VO
 * @sba_ac_vo: surplus bandwidth allowance for VO
 * @uapsd_vo_srv_intv: Uapsd service interval for voice
 * @uapsd_vo_sus_intv: Uapsd suspension interval for voice
 */
struct wlan_mlme_wmm_ac_vo {
	uint8_t dir_ac_vo;
	uint16_t nom_msdu_size_ac_vo;
	uint32_t mean_data_rate_ac_vo;
	uint32_t min_phy_rate_ac_vo;
	uint16_t sba_ac_vo;
	uint32_t uapsd_vo_srv_intv;
	uint32_t uapsd_vo_sus_intv;
};

/**
 * struct wlan_mlme_wmm_ac_vi - Default TSPEC parameters
 * for AC_VI
 * @dir_ac_vi: TSPEC direction for VI
 * @nom_msdu_size_ac_vi: normal MSDU size for VI
 * @mean_data_rate_ac_vi: mean data rate for VI
 * @min_phy_rate_ac_vi: min PHY rate for VI
 * @sba_ac_vi: surplus bandwidth allowance for VI
 * @uapsd_vo_srv_intv: Uapsd service interval for VI
 * @uapsd_vo_sus_intv: Uapsd suspension interval for VI
 */
struct wlan_mlme_wmm_ac_vi {
	uint8_t dir_ac_vi;
	uint16_t nom_msdu_size_ac_vi;
	uint32_t mean_data_rate_ac_vi;
	uint32_t min_phy_rate_ac_vi;
	uint16_t sba_ac_vi;
	uint32_t uapsd_vi_srv_intv;
	uint32_t uapsd_vi_sus_intv;
};

/**
 * struct wlan_mlme_wmm_ac_be - Default TSPEC parameters
 * for AC_BE
 * @dir_ac_be: TSPEC direction for BE
 * @nom_msdu_size_ac_be: normal MSDU size for BE
 * @mean_data_rate_ac_be: mean data rate for BE
 * @min_phy_rate_ac_be: min PHY rate for BE
 * @sba_ac_be: surplus bandwidth allowance for BE
 * @uapsd_be_srv_intv: Uapsd service interval for BE
 * @uapsd_be_sus_intv: Uapsd suspension interval for BE

 */
struct wlan_mlme_wmm_ac_be {
	uint8_t dir_ac_be;
	uint16_t nom_msdu_size_ac_be;
	uint32_t mean_data_rate_ac_be;
	uint32_t min_phy_rate_ac_be;
	uint16_t sba_ac_be;
	uint32_t uapsd_be_srv_intv;
	uint32_t uapsd_be_sus_intv;

};

/**
 * struct wlan_mlme_wmm_ac_bk - Default TSPEC parameters
 * for AC_BK
 * @dir_ac_bk: TSPEC direction for BK
 * @nom_msdu_size_ac_bk: normal MSDU size for BK
 * @mean_data_rate_ac_bk: mean data rate for BK
 * @min_phy_rate_ac_bk: min PHY rate for BK
 * @sba_ac_bk: surplus bandwidth allowance for BK
 * @uapsd_bk_srv_intv: Uapsd service interval for BK
 * @uapsd_bk_sus_intv: Uapsd suspension interval for BK
 */
struct wlan_mlme_wmm_ac_bk {
	uint8_t dir_ac_bk;
	uint16_t nom_msdu_size_ac_bk;
	uint32_t mean_data_rate_ac_bk;
	uint32_t min_phy_rate_ac_bk;
	uint16_t sba_ac_bk;
	uint32_t uapsd_bk_srv_intv;
	uint32_t uapsd_bk_sus_intv;
};

/**
 * struct wlan_mlme_wmm_params - WMM CFG Items
 * @qos_enabled: AP is enabled with 11E
 * @wme_enabled: AP is enabled with WMM
 * @max_sp_length: Maximum SP Length
 * @wsm_enabled: AP is enabled with WSM
 * @edca_profile: WMM Edca profile
 * @wmm_config: WMM configuration
 * @wmm_tspec_element: Default TSPEC parameters
 * @ac_vo: Default TSPEC parameters for AC_VO
 * @ac_vi: Default TSPEC parameters for AC_VI
 * @ac_be: Default TSPEC parameters for AC_BE
 * @ac_bk: Default TSPEC parameters for AC_BK
 * @delayed_trigger_frm_int: delay int(in ms) of UAPSD auto trigger
 */
struct wlan_mlme_wmm_params {
	bool qos_enabled;
	bool wme_enabled;
	uint8_t max_sp_length;
	bool wsm_enabled;
	uint32_t edca_profile;
	struct wlan_mlme_wmm_config wmm_config;
	struct wlan_mlme_wmm_tspec_element wmm_tspec_element;
	struct wlan_mlme_wmm_ac_vo ac_vo;
	struct wlan_mlme_wmm_ac_vi ac_vi;
	struct wlan_mlme_wmm_ac_be ac_be;
	struct wlan_mlme_wmm_ac_bk ac_bk;
	uint32_t delayed_trigger_frm_int;
};

/**
 * struct wlan_mlme_weight_config - weight params to
 * calculate best candidate
 * @rssi_weightage: RSSI weightage
 * @ht_caps_weightage: HT caps weightage
 * @vht_caps_weightage: VHT caps weightage
 * @he_caps_weightage: HE caps weightage
 * @chan_width_weightage: Channel width weightage
 * @chan_band_weightage: Channel band weightage
 * @nss_weightage: NSS weightage
 * @beamforming_cap_weightage: Beamforming caps weightage
 * @pcl_weightage: PCL weightage
 * @channel_congestion_weightage: channel congestion weightage
 * @oce_wan_weightage: OCE WAN metrics weightage
 * @oce_ap_tx_pwr_weightage: weightage based on ap tx power
 * @oce_subnet_id_weightage: weightage based on subnet id
 * @sae_pk_ap_weightage:SAE-PK AP weightage
 */
struct  wlan_mlme_weight_config {
	uint8_t rssi_weightage;
	uint8_t ht_caps_weightage;
	uint8_t vht_caps_weightage;
	uint8_t he_caps_weightage;
	uint8_t chan_width_weightage;
	uint8_t chan_band_weightage;
	uint8_t nss_weightage;
	uint8_t beamforming_cap_weightage;
	uint8_t pcl_weightage;
	uint8_t channel_congestion_weightage;
	uint8_t oce_wan_weightage;
	uint8_t oce_ap_tx_pwr_weightage;
	uint8_t oce_subnet_id_weightage;
	uint8_t sae_pk_ap_weightage;
};

/**
 * struct wlan_mlme_rssi_cfg_score - RSSI params to
 * calculate best candidate
 * @best_rssi_threshold: Best RSSI threshold
 * @good_rssi_threshold: Good RSSI threshold
 * @bad_rssi_threshold: Bad RSSI threshold
 * @good_rssi_pcnt: Good RSSI Percentage
 * @bad_rssi_pcnt: Bad RSSI Percentage
 * @good_rssi_bucket_size: Good RSSI Bucket Size
 * @bad_rssi_bucket_size: Bad RSSI Bucket Size
 * @rssi_pref_5g_rssi_thresh: Preffered 5G RSSI threshold
 */
struct wlan_mlme_rssi_cfg_score  {
	uint32_t best_rssi_threshold;
	uint32_t good_rssi_threshold;
	uint32_t bad_rssi_threshold;
	uint32_t good_rssi_pcnt;
	uint32_t bad_rssi_pcnt;
	uint32_t good_rssi_bucket_size;
	uint32_t bad_rssi_bucket_size;
	uint32_t rssi_pref_5g_rssi_thresh;
};

/*
 * struct wlan_mlme_roam_scoring_cfg - MLME roam related scoring config
 * @enable_scoring_for_roam: Enable/disable BSS Scoring for Roaming
 * @roam_trigger_bitmap: bitmap for various roam triggers
 * @roam_score_delta: percentage delta in roam score
 * @apsd_enabled: Enable automatic power save delivery
 * @min_roam_score_delta: Minimum difference between connected AP's and
 *			candidate AP's roam score to start roaming.
 */
struct wlan_mlme_roam_scoring_cfg {
	bool enable_scoring_for_roam;
	uint32_t roam_trigger_bitmap;
	uint32_t roam_score_delta;
	bool apsd_enabled;
	uint32_t min_roam_score_delta;
};

/* struct wlan_mlme_threshold - Threshold related config items
 * @rts_threshold: set rts threshold
 * @frag_threshold: set fragmentation threshold
 */
struct wlan_mlme_threshold {
	uint32_t rts_threshold;
	uint32_t frag_threshold;
};

/* struct mlme_max_tx_power_24 - power related items
 * @max_len: max length of string
 * @len: actual len of string
 * @data: Data in string format
 */
struct mlme_max_tx_power_24 {
	qdf_size_t max_len;
	qdf_size_t len;
	uint8_t data[CFG_MAX_TX_POWER_2_4_LEN];
};

/* struct mlme_max_tx_power_5 - power related items
 * @max_len: max length of string
 * @len: actual len of string
 * @data: Data in string format
 */
struct mlme_max_tx_power_5 {
	qdf_size_t max_len;
	qdf_size_t len;
	uint8_t data[CFG_MAX_TX_POWER_5_LEN];
};

/* struct mlme_power_usage - power related items
 * @max_len: max length of string
 * @len: actual len of string
 * @data: Data in string format
 */
struct mlme_power_usage {
	qdf_size_t max_len;
	qdf_size_t len;
	char data[CFG_POWER_USAGE_MAX_LEN];
};

/*
 * struct wlan_mlme_power - power related config items
 * @max_tx_power_24: max power Tx for 2.4 ghz, this is based on frequencies
 * @max_tx_power_5: max power Tx for 5 ghz, this is based on frequencies
 * @max_tx_power_24_chan: max power Tx for 2.4 ghz, this is based on channel
 * numbers, this is added to parse the ini values to maintain the backward
 * compatibility, these channel numbers are converted to frequencies and copied
 * to max_tx_power_24 structure, once this conversion is done this structure
 * should not be used.
 * @max_tx_power_5_chan: max power Tx for 5 ghz, this is based on channel
 * numbers, this is added to parse the ini values to maintain the backward
 * compatibility, these channel numbers are converted to frequencies and copied
 * to max_tx_power_24 structure, once this conversion is done this structure
 * should not be used.
 * @power_usage: power usage mode, min, max, mod
 * @tx_power_2g: limit tx power in 2.4 ghz
 * @tx_power_5g: limit tx power in 5 ghz
 * @current_tx_power_level: current tx power level
 * @local_power_constraint: local power constraint
 * @use_local_tpe: preference to use local or regulatory TPE
 * @skip_tpe: option to not consider TPE values in 2.4G/5G bands
 */
struct wlan_mlme_power {
	struct mlme_max_tx_power_24 max_tx_power_24;
	struct mlme_max_tx_power_5 max_tx_power_5;
	struct mlme_max_tx_power_24 max_tx_power_24_chan;
	struct mlme_max_tx_power_5 max_tx_power_5_chan;
	struct mlme_power_usage power_usage;
	uint8_t tx_power_2g;
	uint8_t tx_power_5g;
	uint8_t current_tx_power_level;
	uint8_t local_power_constraint;
	bool use_local_tpe;
	bool skip_tpe;
};

/*
 * struct wlan_mlme_timeout - mlme timeout related config items
 * @join_failure_timeout: join failure timeout
 * @auth_failure_timeout: authenticate failure timeout
 * @auth_rsp_timeout: authenticate response timeout
 * @assoc_failure_timeout: assoc failure timeout
 * @reassoc_failure_timeout: re-assoc failure timeout
 * @probe_after_hb_fail_timeout: Probe after HB fail timeout
 * @olbc_detect_timeout: OLBC detect timeout
 * @addts_rsp_timeout: ADDTS rsp timeout value
 * @heart_beat_threshold: Heart beat threshold
 * @ap_keep_alive_timeout: AP keep alive timeout value
 * @ap_link_monitor_timeout: AP link monitor timeout value
 * @ps_data_inactivity_timeout: PS data inactivity timeout
 * @wmi_wq_watchdog_timeout: timeout period for wmi watchdog bite
 * @sae_auth_failure_timeout: SAE authentication failure timeout
 */
struct wlan_mlme_timeout {
	uint32_t join_failure_timeout;
	uint32_t auth_failure_timeout;
	uint32_t auth_rsp_timeout;
	uint32_t assoc_failure_timeout;
	uint32_t reassoc_failure_timeout;
	uint32_t probe_after_hb_fail_timeout;
	uint32_t olbc_detect_timeout;
	uint32_t addts_rsp_timeout;
	uint32_t heart_beat_threshold;
	uint32_t ap_keep_alive_timeout;
	uint32_t ap_link_monitor_timeout;
	uint32_t ps_data_inactivity_timeout;
	uint32_t wmi_wq_watchdog_timeout;
	uint32_t sae_auth_failure_timeout;
};

/**
 * struct wlan_mlme_oce - OCE related config items
 * @enable_bcast_probe_rsp: enable broadcast probe response
 * @oce_sta_enabled: enable/disable oce feature for sta
 * @oce_sap_enabled: enable/disable oce feature for sap
 * @fils_enabled: enable/disable fils support
 * @feature_bitmap: oce feature bitmap
 *
 */
struct wlan_mlme_oce {
	bool enable_bcast_probe_rsp;
	bool oce_sta_enabled;
	bool oce_sap_enabled;
	bool fils_enabled;
	uint8_t feature_bitmap;
};

/**
 * enum wep_key_id  - values passed to get/set wep default keys
 * @MLME_WEP_DEFAULT_KEY_1: wep default key 1
 * @MLME_WEP_DEFAULT_KEY_2: wep default key 2
 * @MLME_WEP_DEFAULT_KEY_3: wep default key 3
 * @MLME_WEP_DEFAULT_KEY_4: wep default key 4
 */
enum wep_key_id {
	MLME_WEP_DEFAULT_KEY_1 = 0,
	MLME_WEP_DEFAULT_KEY_2,
	MLME_WEP_DEFAULT_KEY_3,
	MLME_WEP_DEFAULT_KEY_4
};

/**
 * struct wlan_mlme_wep_cfg - WEP related configs
 * @is_privacy_enabled:     Flag to check if encryption is enabled
 * @is_shared_key_auth:     Flag to check if the auth type is shared key
 * @is_auth_open_system:    Flag to check if the auth type is open
 * @auth_type:              Authentication type value
 * @wep_default_key_id:     Default WEP key id
 * @wep_default_key_1:      WEP encryption key 1
 * @wep_default_key_2:      WEP encryption key 2
 * @wep_default_key_3:      WEP encryption key 3
 * @wep_default_key_4:      WEP encryption key 4
 */
struct wlan_mlme_wep_cfg {
	bool is_privacy_enabled;
	bool is_shared_key_auth;
	bool is_auth_open_system;
	uint8_t auth_type;
	uint8_t wep_default_key_id;
	struct mlme_cfg_str wep_default_key_1;
	struct mlme_cfg_str wep_default_key_2;
	struct mlme_cfg_str wep_default_key_3;
	struct mlme_cfg_str wep_default_key_4;
};

/**
 * struct wlan_mlme_wifi_pos_cfg - WIFI POS configs
 * @fine_time_meas_cap: fine timing measurement capability information
 * @oem_6g_support_disable: oem is 6Ghz disabled if set
 */
struct wlan_mlme_wifi_pos_cfg {
	uint32_t fine_time_meas_cap;
	bool oem_6g_support_disable;
};

#define MLME_SET_BIT(value, bit_offset) ((value) |= (1 << (bit_offset)))

/* Mask to check if BTM offload is enabled/disabled*/
#define BTM_OFFLOAD_ENABLED_MASK    0x01

#define BTM_OFFLOAD_CONFIG_BIT_8    8
#define BTM_OFFLOAD_CONFIG_BIT_7    7

/*
 * struct wlan_mlme_btm - BTM related configs
 * @prefer_btm_query: flag to prefer btm query over 11k
 * @abridge_flag: set this flag to enable firmware to sort candidates based on
 * roam score rather than selecting preferred APs.
 * @btm_offload_config: configure btm offload
 * @btm_solicited_timeout: configure timeout value for waiting BTM request
 * @btm_max_attempt_cnt: configure maximum attempt for sending BTM query to ESS
 * @btm_sticky_time: configure Stick time after roaming to new AP by BTM
 * @rct_validity_timer: Timeout values for roam cache table entries
 * @disassoc_timer_threshold: Disassociation timeout till which roam scan need
 * not be triggered
 * @btm_query_bitmask: Bitmask to send BTM query with candidate list on
 * various roam
 * @btm_trig_min_candidate_score: Minimum score to consider the AP as candidate
 * when the roam trigger is BTM.
 */
struct wlan_mlme_btm {
	bool prefer_btm_query;
	bool abridge_flag;
	uint32_t btm_offload_config;
	uint32_t btm_solicited_timeout;
	uint32_t btm_max_attempt_cnt;
	uint32_t btm_sticky_time;
	uint32_t rct_validity_timer;
	uint32_t disassoc_timer_threshold;
	uint32_t btm_query_bitmask;
	uint32_t btm_trig_min_candidate_score;
};

/**
 * struct wlan_mlme_fe_wlm - WLM related configs
 * @latency_enable: Flag to check if latency is enabled
 * @latency_reset: Flag to check if latency reset is enabled
 * @latency_level: WLM latency level
 * @latency_flags: WLM latency flags setting
 * @latency_host_flags: WLM latency host flags setting
 */
struct wlan_mlme_fe_wlm {
	bool latency_enable;
	bool latency_reset;
	uint8_t latency_level;
	uint32_t latency_flags[MLME_NUM_WLM_LATENCY_LEVEL];
	uint32_t latency_host_flags[MLME_NUM_WLM_LATENCY_LEVEL];
};

/**
 * struct wlan_mlme_fe_rrm - RRM related configs
 * @rrm_enabled: Flag to check if RRM is enabled for STA
 * @sap_rrm_enabled: Flag to check if RRM is enabled for SAP
 * @rrm_rand_interval: RRM randomization interval
 * @rm_capability: RM enabled capabilities IE
 */
struct wlan_mlme_fe_rrm {
	bool rrm_enabled;
	bool sap_rrm_enabled;
	uint8_t rrm_rand_interval;
	uint8_t rm_capability[MLME_RMENABLEDCAP_MAX_LEN];
};

#ifdef MWS_COEX
/*
 * struct wlan_mlme_mwc - MWC related configs
 * @mws_coex_4g_quick_tdm:  bitmap to set mws-coex 5g-nr power limit
 * @mws_coex_5g_nr_pwr_limit: bitmap to set mws-coex 5g-nr power limit
 * @mws_coex_pcc_channel_avoid_delay: PCC avoidance delay in seconds
 * @mws_coex_scc_channel_avoid_delay: SCC avoidance delay in seconds
 **/
struct wlan_mlme_mwc {
	uint32_t mws_coex_4g_quick_tdm;
	uint32_t mws_coex_5g_nr_pwr_limit;
	uint32_t mws_coex_pcc_channel_avoid_delay;
	uint32_t mws_coex_scc_channel_avoid_delay;
};
#else
struct wlan_mlme_mwc {
};
#endif

/**
 * enum mlme_reg_srd_master_modes  - Bitmap of SRD master modes supported
 * @MLME_SRD_MASTER_MODE_SAP: SRD master mode for SAP
 * @MLME_SRD_MASTER_MODE_P2P_GO: SRD master mode for P2P-GO
 * @MLME_SRD_MASTER_MODE_NAN: SRD master mode for NAN
 */
enum mlme_reg_srd_master_modes {
	MLME_SRD_MASTER_MODE_SAP = 1,
	MLME_SRD_MASTER_MODE_P2P_GO = 2,
	MLME_SRD_MASTER_MODE_NAN = 4,
};

/**
 * struct wlan_mlme_reg - REG related configs
 * @self_gen_frm_pwr: self-generated frame power in tx chain mask
 * for CCK rates
 * @etsi_srd_chan_in_master_mode: etsi srd chan in master mode
 * @fcc_5dot9_ghz_chan_in_master_mode: fcc 5.9 GHz chan in master mode
 * @restart_beaconing_on_ch_avoid: restart beaconing on ch avoid
 * @indoor_channel_support: indoor channel support
 * @scan_11d_interval: scan 11d interval
 * @valid_channel_freq_list: array for valid channel list
 * @valid_channel_list_num: valid channel list number
 * @enable_11d_in_world_mode: Whether to enable 11d scan in world mode or not
 * @avoid_acs_freq_list: List of the frequencies which need to be avoided
 * during acs
 * @avoid_acs_freq_list_num: Number of the frequencies to be avoided during acs
 * @ignore_fw_reg_offload_ind: Ignore fw regulatory offload indication
 * @enable_pending_chan_list_req: enables/disables scan channel
 * list command to FW till the current scan is complete.
 * @retain_nol_across_regdmn_update: Retain the NOL list across the regdomain.
 * @enable_nan_on_indoor_channels: Enable nan on Indoor channels
 */
struct wlan_mlme_reg {
	uint32_t self_gen_frm_pwr;
	uint8_t etsi_srd_chan_in_master_mode;
	bool fcc_5dot9_ghz_chan_in_master_mode;
	enum restart_beaconing_on_ch_avoid_rule
		restart_beaconing_on_ch_avoid;
	bool indoor_channel_support;
	uint32_t scan_11d_interval;
	uint32_t valid_channel_freq_list[CFG_VALID_CHANNEL_LIST_LEN];
	uint32_t valid_channel_list_num;
	bool enable_11d_in_world_mode;
#ifdef SAP_AVOID_ACS_FREQ_LIST
	uint16_t avoid_acs_freq_list[CFG_VALID_CHANNEL_LIST_LEN];
	uint8_t avoid_acs_freq_list_num;
#endif
	bool ignore_fw_reg_offload_ind;
	bool enable_pending_chan_list_req;
	bool retain_nol_across_regdmn_update;
	bool enable_nan_on_indoor_channels;
};

#define IOT_AGGR_INFO_MAX_NUM 32

/**
 * struct wlan_iot_aggr - IOT related AGGR rule
 *
 * @oui: OUI for the rule
 * @oui_len: length of the OUI
 * @ampdu_sz: max aggregation size in no. of MPDUs
 * @amsdu_sz: max aggregation size in no. of MSDUs
 */
struct wlan_iot_aggr {
	uint8_t oui[OUI_LENGTH];
	uint32_t oui_len;
	uint32_t ampdu_sz;
	uint32_t amsdu_sz;
};

/**
 * struct wlan_mlme_iot - IOT related CFG Items
 *
 * @aggr: aggr rules
 * @aggr_num: number of the configured aggr rules
 */
struct wlan_mlme_iot {
	struct wlan_iot_aggr aggr[IOT_AGGR_INFO_MAX_NUM];
	uint32_t aggr_num;
};

/**
 * struct wlan_mlme_cfg - MLME config items
 * @chainmask_cfg: VHT chainmask related cfg items
 * @edca_params: edca related CFG items
 * @gen: Generic CFG items
 * @ht_caps: HT related CFG Items
 * @he_caps: HE related cfg items
 * @lfr: LFR related CFG Items
 * @ibss: IBSS related CFG items
 * @obss_ht40:obss ht40 CFG Items
 * @mbo_cfg: Multiband Operation related CFG items
 * @vht_caps: VHT related CFG Items
 * @qos_mlme_params: QOS CFG Items
 * @rates: Rates related cfg items
 * @product_details: product details related CFG Items
 * @dfs_cfg: DFS related CFG Items
 * @sap_protection_cfg: SAP erp protection related CFG items
 * @sap_cfg: sap CFG items
 * @nss_chains_ini_cfg: Per vdev nss, chains related CFG items
 * @sta: sta CFG Items
 * @stats: stats CFG Items
 * @scoring: BSS Scoring related CFG Items
 * @oce: OCE related CFG items
 * @threshold: threshold related cfg items
 * @timeouts: mlme timeout related CFG items
 * @twt_cfg: TWT CFG Items
 * @wlan_mlme_power: power related items
 * @acs: ACS related CFG items
 * @feature_flags: Feature flag config items
 * @ps_params: Powersave related ini configs
 * @wep_params:  WEP related config items
 * @wifi_pos_cfg: WIFI POS config
 * @wmm_params: WMM related CFG & INI Items
 * @wps_params: WPS related CFG itmes
 * @btm: BTM related CFG itmes
 * @wlm_config: WLM related CFG items
 * @rrm_config: RRM related CFG items
 * @mwc: MWC related CFG items
 * @dot11_mode: dot11 mode supported
 * @reg: REG related CFG itmes
 * @trig_score_delta: Roam score delta value for various roam triggers
 * @trig_min_rssi: Expected minimum RSSI value of candidate AP for
 * various roam triggers
 * @iot: IOT related CFG items
 */
struct wlan_mlme_cfg {
	struct wlan_mlme_chainmask chainmask_cfg;
	struct wlan_mlme_edca_params edca_params;
	struct wlan_mlme_generic gen;
	struct wlan_mlme_ht_caps ht_caps;
#ifdef WLAN_FEATURE_11AX
	struct wlan_mlme_he_caps he_caps;
#endif
	struct wlan_mlme_lfr_cfg lfr;
	struct wlan_mlme_obss_ht40 obss_ht40;
	struct wlan_mlme_mbo mbo_cfg;
	struct wlan_mlme_vht_caps vht_caps;
	struct wlan_mlme_qos qos_mlme_params;
	struct wlan_mlme_rates rates;
	struct wlan_mlme_product_details_cfg product_details;
	struct wlan_mlme_dfs_cfg dfs_cfg;
	struct wlan_mlme_sap_protection sap_protection_cfg;
	struct wlan_mlme_cfg_sap sap_cfg;
	struct wlan_mlme_nss_chains nss_chains_ini_cfg;
	struct wlan_mlme_sta_cfg sta;
	struct wlan_mlme_stats_cfg stats;
	struct wlan_mlme_roam_scoring_cfg roam_scoring;
	struct wlan_mlme_oce oce;
	struct wlan_mlme_threshold threshold;
	struct wlan_mlme_timeout timeouts;
	struct wlan_mlme_cfg_twt twt_cfg;
	struct wlan_mlme_power power;
	struct wlan_mlme_acs acs;
	struct wlan_mlme_feature_flag feature_flags;
	struct wlan_mlme_powersave ps_params;
	struct wlan_mlme_wep_cfg wep_params;
	struct wlan_mlme_wifi_pos_cfg wifi_pos_cfg;
	struct wlan_mlme_wmm_params wmm_params;
	struct wlan_mlme_wps_params wps_params;
	struct wlan_mlme_btm btm;
	struct wlan_mlme_fe_wlm wlm_config;
	struct wlan_mlme_fe_rrm rrm_config;
	struct wlan_mlme_mwc mwc;
	struct wlan_mlme_dot11_mode dot11_mode;
	struct wlan_mlme_reg reg;
	struct roam_trigger_score_delta trig_score_delta[NUM_OF_ROAM_TRIGGERS];
	struct roam_trigger_min_rssi trig_min_rssi[NUM_OF_ROAM_MIN_RSSI];
	struct wlan_mlme_ratemask ratemask_cfg;
	struct wlan_mlme_iot iot;
};

enum pkt_origin {
	FW,
	HOST
};

/**
 * struct mlme_pmk_info - SAE Roaming using single pmk info
 * @pmk: pmk
 * @pmk_len: pmk length
 * @spmk_timeout_period: Time to generate new SPMK in seconds.
 * @spmk_timestamp: System timestamp at which the Single PMK entry was added.
 */
struct mlme_pmk_info {
	uint8_t pmk[CFG_MAX_PMK_LEN];
	uint8_t pmk_len;
	uint16_t spmk_timeout_period;
	qdf_time_t spmk_timestamp;
};

/**
 * struct wlan_mlme_sae_single_pmk - SAE Roaming using single pmk configuration
 * structure
 * @sae_single_pmk_ap: Current connected AP has VSIE or not
 * @pmk_info: pmk information
 */
struct wlan_mlme_sae_single_pmk {
	bool sae_single_pmk_ap;
	struct mlme_pmk_info pmk_info;
};

/**
 * struct mlme_roam_debug_info - Roam debug information storage structure.
 * @trigger:            Roam trigger related data
 * @scan:               Roam scan related data structure.
 * @result:             Roam result parameters.
 * @data_11kv:          Neighbor report/BTM parameters.
 * @btm_rsp:            BTM response information
 * @roam_init_info:     Roam initial info
 * @roam_msg_info:      roam related message information
 * @roam_event_param:   Roam event notif params
 */
struct mlme_roam_debug_info {
	struct wmi_roam_trigger_info trigger;
	struct wmi_roam_scan_data scan;
	struct wmi_roam_result result;
	struct wmi_neighbor_report_data data_11kv;
	struct roam_btm_response_data btm_rsp;
	struct roam_initial_data roam_init_info;
	struct roam_msg_info roam_msg_info;
	struct roam_event_rt_info roam_event_param;
};

/**
 * struct wlan_ies - Generic WLAN Information Element(s) format
 * @len: Total length of the IEs
 * @data: IE data
 */
struct wlan_ies {
	uint16_t len;
	uint8_t *data;
};

/**
 * struct wlan_change_bi - Message struct to update beacon interval
 * @message_type: type of message
 * @length: length of message
 * @beacon_interval: beacon interval to update to (seconds)
 * @bssid: BSSID of vdev
 * @session_id: session ID of vdev
 */
struct wlan_change_bi {
	uint16_t message_type;
	uint16_t length;
	uint16_t beacon_interval;
	struct qdf_mac_addr bssid;
	uint8_t session_id;
};

#endif
