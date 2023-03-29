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

/**
 *   \file csr_api.h
 *
 *   Exports and types for the Common Scan and Roaming Module interfaces.
 */

#ifndef CSRAPI_H__
#define CSRAPI_H__

#include "sir_api.h"
#include "sir_mac_prot_def.h"
#include "csr_link_list.h"
#include "wlan_scan_public_structs.h"
#include "wlan_mlme_public_struct.h"

#define CSR_INVALID_SCANRESULT_HANDLE       (NULL)

enum csr_akm_type {
	/* never used */
	eCSR_AUTH_TYPE_NONE,
	/* MAC layer authentication types */
	eCSR_AUTH_TYPE_OPEN_SYSTEM,
	eCSR_AUTH_TYPE_SHARED_KEY,
	eCSR_AUTH_TYPE_SAE,
	eCSR_AUTH_TYPE_AUTOSWITCH,

	/* Upper layer authentication types */
	eCSR_AUTH_TYPE_WPA,
	eCSR_AUTH_TYPE_WPA_PSK,
	eCSR_AUTH_TYPE_WPA_NONE,

	eCSR_AUTH_TYPE_RSN,
	eCSR_AUTH_TYPE_RSN_PSK,
	eCSR_AUTH_TYPE_FT_RSN,
	eCSR_AUTH_TYPE_FT_RSN_PSK,
#ifdef FEATURE_WLAN_WAPI
	eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE,
	eCSR_AUTH_TYPE_WAPI_WAI_PSK,
#endif /* FEATURE_WLAN_WAPI */
	eCSR_AUTH_TYPE_CCKM_WPA,
	eCSR_AUTH_TYPE_CCKM_RSN,
	eCSR_AUTH_TYPE_RSN_PSK_SHA256,
	eCSR_AUTH_TYPE_RSN_8021X_SHA256,
	eCSR_AUTH_TYPE_FILS_SHA256,
	eCSR_AUTH_TYPE_FILS_SHA384,
	eCSR_AUTH_TYPE_FT_FILS_SHA256,
	eCSR_AUTH_TYPE_FT_FILS_SHA384,
	eCSR_AUTH_TYPE_DPP_RSN,
	eCSR_AUTH_TYPE_OWE,
	eCSR_AUTH_TYPE_SUITEB_EAP_SHA256,
	eCSR_AUTH_TYPE_SUITEB_EAP_SHA384,
	eCSR_AUTH_TYPE_OSEN,
	eCSR_AUTH_TYPE_FT_SAE,
	eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384,
	eCSR_NUM_OF_SUPPORT_AUTH_TYPE,
	eCSR_AUTH_TYPE_FAILED = 0xff,
	eCSR_AUTH_TYPE_UNKNOWN = eCSR_AUTH_TYPE_FAILED,

};

typedef enum {
	eCSR_ENCRYPT_TYPE_NONE,
	eCSR_ENCRYPT_TYPE_WEP40_STATICKEY,
	eCSR_ENCRYPT_TYPE_WEP104_STATICKEY,
	eCSR_ENCRYPT_TYPE_WEP40,
	eCSR_ENCRYPT_TYPE_WEP104,
	eCSR_ENCRYPT_TYPE_TKIP,
	eCSR_ENCRYPT_TYPE_AES,/* CCMP */
#ifdef FEATURE_WLAN_WAPI
	/* WAPI */
	eCSR_ENCRYPT_TYPE_WPI,
#endif  /* FEATURE_WLAN_WAPI */
	eCSR_ENCRYPT_TYPE_KRK,
	eCSR_ENCRYPT_TYPE_BTK,
	eCSR_ENCRYPT_TYPE_AES_CMAC,
	eCSR_ENCRYPT_TYPE_AES_GMAC_128,
	eCSR_ENCRYPT_TYPE_AES_GMAC_256,
	eCSR_ENCRYPT_TYPE_AES_GCMP,
	eCSR_ENCRYPT_TYPE_AES_GCMP_256,
	eCSR_ENCRYPT_TYPE_ANY,
	eCSR_NUM_OF_ENCRYPT_TYPE = eCSR_ENCRYPT_TYPE_ANY,

	eCSR_ENCRYPT_TYPE_FAILED = 0xff,
	eCSR_ENCRYPT_TYPE_UNKNOWN = eCSR_ENCRYPT_TYPE_FAILED,

} eCsrEncryptionType;

typedef enum {
	/* 11a/b/g only, no HT, no proprietary */
	eCSR_DOT11_MODE_abg = 0x0001,
	eCSR_DOT11_MODE_11a = 0x0002,
	eCSR_DOT11_MODE_11b = 0x0004,
	eCSR_DOT11_MODE_11g = 0x0008,
	eCSR_DOT11_MODE_11n = 0x0010,
	eCSR_DOT11_MODE_11g_ONLY = 0x0020,
	eCSR_DOT11_MODE_11n_ONLY = 0x0040,
	eCSR_DOT11_MODE_11b_ONLY = 0x0080,
	eCSR_DOT11_MODE_11ac = 0x0100,
	eCSR_DOT11_MODE_11ac_ONLY = 0x0200,
	/*
	 * This is for WIFI test. It is same as eWNIAPI_MAC_PROTOCOL_ALL
	 * It is for CSR internal use
	 */
	eCSR_DOT11_MODE_AUTO = 0x0400,
	eCSR_DOT11_MODE_11ax = 0x0800,
	eCSR_DOT11_MODE_11ax_ONLY = 0x1000,

	/* specify the number of maximum bits for phyMode */
	eCSR_NUM_PHY_MODE = 16,
} eCsrPhyMode;

/**
 * enum eCsrRoamBssType - BSS type in CSR operations
 * @eCSR_BSS_TYPE_INFRASTRUCTURE: Infrastructure station
 * @eCSR_BSS_TYPE_INFRA_AP: SoftAP
 * @eCSR_BSS_TYPE_NDI: NAN datapath interface
 * @eCSR_BSS_TYPE_ANY: any BSS type
 */
typedef enum {
	eCSR_BSS_TYPE_INFRASTRUCTURE,
	eCSR_BSS_TYPE_INFRA_AP,
	eCSR_BSS_TYPE_NDI,
	eCSR_BSS_TYPE_ANY,
} eCsrRoamBssType;

typedef enum {
	eCSR_SCAN_SUCCESS,
	eCSR_SCAN_FAILURE,
	eCSR_SCAN_ABORT,
	eCSR_SCAN_FOUND_PEER,
} eCsrScanStatus;

typedef enum {
	eCSR_BW_20MHz_VAL = 20,
	eCSR_BW_40MHz_VAL = 40,
	eCSR_BW_80MHz_VAL = 80,
	eCSR_BW_160MHz_VAL = 160
} eCSR_BW_Val;

typedef enum {
	eCSR_INI_SINGLE_CHANNEL_CENTERED = 0,
	eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY = 1,
	eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY = 3,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9,
	eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,
	eCSR_INI_CHANNEL_BONDING_STATE_MAX = 11
} eIniChanBondState;

#define CSR_RSN_MAX_PMK_LEN         48
#define CSR_MAX_PMKID_ALLOWED       32
#define CSR_TKIP_KEY_LEN            32
#define CSR_AES_KEY_LEN             16
#define CSR_AES_GCMP_KEY_LEN        16
#define CSR_AES_GCMP_256_KEY_LEN    32
#define CSR_AES_GMAC_128_KEY_LEN    16
#define CSR_AES_GMAC_256_KEY_LEN    32
#define CSR_MAX_TX_POWER        (WNI_CFG_CURRENT_TX_POWER_LEVEL_STAMAX)
#ifdef FEATURE_WLAN_WAPI
#define CSR_WAPI_BKID_SIZE          16
#define CSR_MAX_BKID_ALLOWED        16
#define CSR_WAPI_KEY_LEN            32
#define CSR_MAX_KEY_LEN         (CSR_WAPI_KEY_LEN) /* longest one is for WAPI */
#else
#define CSR_MAX_KEY_LEN         (CSR_TKIP_KEY_LEN) /* longest one is for TKIP */
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
#define CSR_KRK_KEY_LEN             16
#endif

typedef struct tagCsrChannelInfo {
	uint8_t numOfChannels;
	uint32_t *freq_list;
} tCsrChannelInfo, *tpCsrChannelInfo;

typedef enum {
	eHIDDEN_SSID_NOT_IN_USE,
	eHIDDEN_SSID_ZERO_LEN,
	eHIDDEN_SSID_ZERO_CONTENTS
} tHiddenssId;

typedef struct tagCsrSSIDInfo {
	tSirMacSSid SSID;
	bool handoffPermitted;
	tHiddenssId ssidHidden;
} tCsrSSIDInfo;

typedef struct tagCsrSSIDs {
	uint32_t numOfSSIDs;
	tCsrSSIDInfo *SSIDList; /* To be allocated for array of SSIDs */
} tCsrSSIDs;

typedef struct tagCsrBSSIDs {
	uint32_t numOfBSSIDs;
	struct qdf_mac_addr *bssid;
} tCsrBSSIDs;

typedef struct tagCsrScanResultInfo {
	/*
	 * Carry the IEs for the current BSSDescription.
	 * A pointer to tDot11fBeaconIEs. Maybe NULL for start BSS.
	 */
	void *pvIes;
	tAniSSID ssId;
	unsigned long timer;           /* timer is variable for hidden SSID timer */
	/*
	 * This member must be the last in the structure because the
	 * end of struct bss_description is an
	 * array with nonknown size at this time */
	struct bss_description BssDescriptor;
} tCsrScanResultInfo;

typedef struct tagCsrEncryptionList {

	uint32_t numEntries;
	eCsrEncryptionType encryptionType[eCSR_NUM_OF_ENCRYPT_TYPE];

} tCsrEncryptionList, *tpCsrEncryptionList;

typedef struct tagCsrAuthList {
	uint32_t numEntries;
	enum csr_akm_type authType[eCSR_NUM_OF_SUPPORT_AUTH_TYPE];
} tCsrAuthList, *tpCsrAuthList;

#ifdef FEATURE_WLAN_ESE
typedef struct tagCsrEseCckmInfo {
	uint32_t reassoc_req_num;
	bool krk_plumbed;
	uint8_t krk[SIR_KRK_KEY_LEN];
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint8_t btk[SIR_BTK_KEY_LEN];
#endif
} tCsrEseCckmInfo;

typedef struct tagCsrEseCckmIe {
	uint8_t cckmIe[DOT11F_IE_RSN_MAX_LEN];
	uint8_t cckmIeLen;
} tCsrEseCckmIe;
#endif /* FEATURE_WLAN_ESE */

typedef struct sCsrChannel_ {
	uint8_t numChannels;
	uint32_t channel_freq_list[CFG_VALID_CHANNEL_LIST_LEN];
} sCsrChannel;

typedef struct sCsrChnPower_ {
	uint32_t first_chan_freq;
	uint8_t numChannels;
	uint8_t maxtxPower;
} sCsrChnPower;

typedef struct tagCsr11dinfo {
	sCsrChannel Channels;
	uint8_t countryCode[REG_ALPHA2_LEN + 1];
	/* max power channel list */
	sCsrChnPower ChnPower[CFG_VALID_CHANNEL_LIST_LEN];
} tCsr11dinfo;

typedef enum {
	eCSR_ROAM_CANCELLED = 1,
	/* it means error happens before assoc_start/roaming_start is called. */
	eCSR_ROAM_FAILED,
	/*
	 * a CSR trigger roaming operation starts,
	 * callback may get a pointer to tCsrConnectedProfile
	 */
	eCSR_ROAM_ROAMING_START,
	/* a CSR trigger roaming operation is completed */
	eCSR_ROAM_ROAMING_COMPLETION,
	/* Connection completed status. */
	eCSR_ROAM_CONNECT_COMPLETION,
	/*
	 * an association or start_IBSS operation starts,
	 * callback may get a pointer to struct csr_roam_profile and
	 * a pointer to struct bss_description
	 */
	eCSR_ROAM_ASSOCIATION_START,
	/*
	 * a roaming operation is finish, see eCsrRoamResult for
	 * possible data passed back
	 */
	eCSR_ROAM_ASSOCIATION_COMPLETION,
	eCSR_ROAM_DISASSOCIATED,
	eCSR_ROAM_ASSOCIATION_FAILURE,
	/* when callback with this flag. it gets a pointer to the BSS desc. */
	eCSR_ROAM_SHOULD_ROAM,
	/* A new candidate for PMKID is found */
	eCSR_ROAM_SCAN_FOUND_NEW_BSS,
	/* CSR is done lostlink roaming and still cannot reconnect */
	eCSR_ROAM_LOSTLINK,
	/* a link lost is detected. CSR starts roaming. */
	eCSR_ROAM_LOSTLINK_DETECTED,
	/*
	 * TKIP MIC error detected, callback gets a pointer
	 * to struct mic_failure_ind
	 */
	eCSR_ROAM_MIC_ERROR_IND,
	/*
	 * Update the connection status network is active etc.
	 */
	eCSR_ROAM_CONNECT_STATUS_UPDATE,
	eCSR_ROAM_GEN_INFO,
	eCSR_ROAM_SET_KEY_COMPLETE,
	/* BSS in SoftAP mode status indication */
	eCSR_ROAM_INFRA_IND,
	eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
	eCSR_ROAM_FT_RESPONSE,
	eCSR_ROAM_FT_START,
	/* this mean error happens before assoc_start/roam_start is called. */
	eCSR_ROAM_SESSION_OPENED,
	eCSR_ROAM_FT_REASSOC_FAILED,
	eCSR_ROAM_PMK_NOTIFY,
	/*
	 * Following 4 enums are used by FEATURE_WLAN_LFR_METRICS
	 * but they are needed for compilation even when
	 * FEATURE_WLAN_LFR_METRICS is not defined.
	 */
	eCSR_ROAM_PREAUTH_INIT_NOTIFY,
	eCSR_ROAM_PREAUTH_STATUS_SUCCESS,
	eCSR_ROAM_PREAUTH_STATUS_FAILURE,
	eCSR_ROAM_HANDOVER_SUCCESS,
	/*
	 * TDLS callback events
	 */
	eCSR_ROAM_TDLS_STATUS_UPDATE,
	eCSR_ROAM_RESULT_MGMT_TX_COMPLETE_IND,

	/* Disaconnect all the clients */
	eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS,
	/* Stopbss triggered from SME due to different */
	eCSR_ROAM_SEND_P2P_STOP_BSS,
	/* beacon interval */
#ifdef WLAN_FEATURE_11W
	eCSR_ROAM_UNPROT_MGMT_FRAME_IND,
#endif

#ifdef FEATURE_WLAN_ESE
	eCSR_ROAM_TSM_IE_IND,
	eCSR_ROAM_CCKM_PREAUTH_NOTIFY,
	eCSR_ROAM_ESE_ADJ_AP_REPORT_IND,
	eCSR_ROAM_ESE_BCN_REPORT_IND,
#endif /* FEATURE_WLAN_ESE */

	/* Radar indication from lower layers */
	eCSR_ROAM_DFS_RADAR_IND,
	eCSR_ROAM_SET_CHANNEL_RSP,

	/* Channel sw update notification */
	eCSR_ROAM_DFS_CHAN_SW_NOTIFY,
	eCSR_ROAM_EXT_CHG_CHNL_IND,
	eCSR_ROAM_STA_CHANNEL_SWITCH,
	eCSR_ROAM_NDP_STATUS_UPDATE,
	eCSR_ROAM_UPDATE_SCAN_RESULT,
	eCSR_ROAM_START,
	eCSR_ROAM_ABORT,
	eCSR_ROAM_NAPI_OFF,
	eCSR_ROAM_CHANNEL_COMPLETE_IND,
	eCSR_ROAM_CAC_COMPLETE_IND,
	eCSR_ROAM_SAE_COMPUTE,
	eCSR_ROAM_FIPS_PMK_REQUEST,
} eRoamCmdStatus;

/* comment inside indicates what roaming callback gets */
typedef enum {
	eCSR_ROAM_RESULT_NONE,
	eCSR_ROAM_RESULT_SUCCESS = eCSR_ROAM_RESULT_NONE,
	/*
	 * If roamStatus is eCSR_ROAM_ASSOCIATION_COMPLETION,
	 * struct csr_roam_info's bss_desc may pass back
	 */
	eCSR_ROAM_RESULT_FAILURE,
	/* Pass back pointer to struct csr_roam_info */
	eCSR_ROAM_RESULT_ASSOCIATED,
	eCSR_ROAM_RESULT_NOT_ASSOCIATED,
	eCSR_ROAM_RESULT_MIC_FAILURE,
	eCSR_ROAM_RESULT_FORCED,
	eCSR_ROAM_RESULT_DISASSOC_IND,
	eCSR_ROAM_RESULT_DEAUTH_IND,
	eCSR_ROAM_RESULT_CAP_CHANGED,
	eCSR_ROAM_RESULT_LOSTLINK,
	eCSR_ROAM_RESULT_MIC_ERROR_UNICAST,
	eCSR_ROAM_RESULT_MIC_ERROR_GROUP,
	eCSR_ROAM_RESULT_AUTHENTICATED,
	eCSR_ROAM_RESULT_NEW_RSN_BSS,
#ifdef FEATURE_WLAN_WAPI
	eCSR_ROAM_RESULT_NEW_WAPI_BSS,
#endif /* FEATURE_WLAN_WAPI */
	/* INFRA started successfully */
	eCSR_ROAM_RESULT_INFRA_STARTED,
	/* INFRA start failed */
	eCSR_ROAM_RESULT_INFRA_START_FAILED,
	/* INFRA stopped */
	eCSR_ROAM_RESULT_INFRA_STOPPED,
	/* A station joining INFRA AP */
	eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND,
	/* A station joined INFRA AP */
	eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF,
	/* INFRA disassociated */
	eCSR_ROAM_RESULT_INFRA_DISASSOCIATED,
	eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND,
	eCSR_ROAM_RESULT_SEND_ACTION_FAIL,
	/* peer rejected assoc because max assoc limit reached */
	eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED,
	/* Assoc rejected due to concurrent session running on a diff channel */
	eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL,
	/* TDLS events */
	eCSR_ROAM_RESULT_ADD_TDLS_PEER,
	eCSR_ROAM_RESULT_UPDATE_TDLS_PEER,
	eCSR_ROAM_RESULT_DELETE_TDLS_PEER,
	eCSR_ROAM_RESULT_TEARDOWN_TDLS_PEER_IND,
	eCSR_ROAM_RESULT_DELETE_ALL_TDLS_PEER_IND,
	eCSR_ROAM_RESULT_LINK_ESTABLISH_REQ_RSP,
	eCSR_ROAM_RESULT_TDLS_SHOULD_DISCOVER,
	eCSR_ROAM_RESULT_TDLS_SHOULD_TEARDOWN,
	eCSR_ROAM_RESULT_TDLS_SHOULD_PEER_DISCONNECTED,
	eCSR_ROAM_RESULT_TDLS_CONNECTION_TRACKER_NOTIFICATION,
	eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND,
	eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS,
	eCSR_ROAM_RESULT_CHANNEL_CHANGE_FAILURE,
	eCSR_ROAM_RESULT_CSA_RESTART_RSP,
	eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_SUCCESS,
	eCSR_ROAM_EXT_CHG_CHNL_UPDATE_IND,

	eCSR_ROAM_RESULT_NDI_CREATE_RSP,
	eCSR_ROAM_RESULT_NDI_DELETE_RSP,
	eCSR_ROAM_RESULT_NDP_INITIATOR_RSP,
	eCSR_ROAM_RESULT_NDP_NEW_PEER_IND,
	eCSR_ROAM_RESULT_NDP_CONFIRM_IND,
	eCSR_ROAM_RESULT_NDP_INDICATION,
	eCSR_ROAM_RESULT_NDP_SCHED_UPDATE_RSP,
	eCSR_ROAM_RESULT_NDP_RESPONDER_RSP,
	eCSR_ROAM_RESULT_NDP_END_RSP,
	eCSR_ROAM_RESULT_NDP_PEER_DEPARTED_IND,
	eCSR_ROAM_RESULT_NDP_END_IND,
	eCSR_ROAM_RESULT_CAC_END_IND,
	/* If Scan for SSID failed to found proper BSS */
	eCSR_ROAM_RESULT_SCAN_FOR_SSID_FAILURE,
	eCSR_ROAM_RESULT_INVOKE_FAILED,
} eCsrRoamResult;

typedef enum {
	eCSR_DISCONNECT_REASON_UNSPECIFIED = 0,
	eCSR_DISCONNECT_REASON_MIC_ERROR,
	eCSR_DISCONNECT_REASON_DISASSOC,
	eCSR_DISCONNECT_REASON_DEAUTH,
	eCSR_DISCONNECT_REASON_HANDOFF,
	eCSR_DISCONNECT_REASON_STA_HAS_LEFT,
	eCSR_DISCONNECT_REASON_NDI_DELETE,
	eCSR_DISCONNECT_REASON_ROAM_HO_FAIL,
} eCsrRoamDisconnectReason;

typedef enum {
	/* Not associated in Infra or participating in an Ad-hoc */
	eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED,
	/* Associated in an Infrastructure network. */
	eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED,
	/* Participating in WDS network in AP/STA mode but not connected yet */
	eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED,
	/* Participating in a WDS network and connected peer to peer */
	eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED,
	/* Participating in a Infra network in AP not yet in connected state */
	eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED,
	/* Participating in a Infra network and connected to a peer */
	eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED,
	/* Disconnecting with AP or stop connecting process */
	eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING,
	/* NAN Data interface not started */
	eCSR_CONNECT_STATE_TYPE_NDI_NOT_STARTED,
	/* NAN Data interface started */
	eCSR_CONNECT_STATE_TYPE_NDI_STARTED,

} eCsrConnectState;

/*
 * This parameter is no longer supported in the Profile.
 * Need to set this in the global properties for the adapter.
 */
typedef enum eCSR_MEDIUM_ACCESS {
	eCSR_MEDIUM_ACCESS_AUTO = 0,
	eCSR_MEDIUM_ACCESS_DCF,
	eCSR_MEDIUM_ACCESS_eDCF,
	eCSR_MEDIUM_ACCESS_HCF,

	eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p,
	eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP,
	eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify,
	eCSR_MEDIUM_ACCESS_11e_eDCF = eCSR_MEDIUM_ACCESS_eDCF,
	eCSR_MEDIUM_ACCESS_11e_HCF = eCSR_MEDIUM_ACCESS_HCF,
} eCsrMediaAccessType;

typedef enum {
	eCSR_OPERATING_CHANNEL_ALL = 0,
	eCSR_OPERATING_CHANNEL_AUTO = eCSR_OPERATING_CHANNEL_ALL,
	eCSR_OPERATING_CHANNEL_ANY = eCSR_OPERATING_CHANNEL_ALL,
} eOperationChannel;

/*
 * For channel bonding, the channel number gap is 4, either up or down.
 * For both 11a and 11g mode.
 */
#define CSR_CB_CHANNEL_GAP 4
/* Considering 5 MHz Channel BW */
#define CSR_CB_CENTER_CHANNEL_OFFSET    10
#define CSR_SEC_CHANNEL_OFFSET    20


/* WEP keysize (in bits) */
typedef enum {
	/* 40 bit key + 24bit IV = 64bit WEP */
	eCSR_SECURITY_WEP_KEYSIZE_40 = 40,
	/* 104bit key + 24bit IV = 128bit WEP */
	eCSR_SECURITY_WEP_KEYSIZE_104 = 104,
	eCSR_SECURITY_WEP_KEYSIZE_MIN = eCSR_SECURITY_WEP_KEYSIZE_40,
	eCSR_SECURITY_WEP_KEYSIZE_MAX = eCSR_SECURITY_WEP_KEYSIZE_104,
	eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES =
		(eCSR_SECURITY_WEP_KEYSIZE_MAX / 8),
} eCsrWEPKeySize;

/* Possible values for the WEP static key ID */
typedef enum {

	eCSR_SECURITY_WEP_STATIC_KEY_ID_MIN = 0,
	eCSR_SECURITY_WEP_STATIC_KEY_ID_MAX = 3,
	eCSR_SECURITY_WEP_STATIC_KEY_ID_DEFAULT = 0,

	eCSR_SECURITY_WEP_STATIC_KEY_ID_INVALID = -1,

} eCsrWEPStaticKeyID;

/* Two extra key indicies are used for the IGTK, two for BIGTK */
#define CSR_MAX_NUM_KEY     (eCSR_SECURITY_WEP_STATIC_KEY_ID_MAX + 2 + 1 + 2)

typedef enum {
	/*
	 * Roaming because HDD requested for reassoc by changing one of the
	 * fields in tCsrRoamModifyProfileFields. OR Roaming because SME
	 * requested for reassoc by changing one of the fields in
	 * tCsrRoamModifyProfileFields.
	 */
	eCsrRoamReasonStaCapabilityChanged,
	/*
	 * Roaming because SME requested for reassoc to a different AP,
	 * as part of inter AP handoff.
	 */
	eCsrRoamReasonBetterAP,
	/*
	 * Roaming because SME requested it as the link is lost - placeholder,
	 * will clean it up once handoff code gets in
	 */
	eCsrRoamReasonSmeIssuedForLostLink,

} eCsrRoamReasonCodes;

typedef enum {
	eCsrRoamWmmAuto = 0,
	eCsrRoamWmmQbssOnly = 1,
	eCsrRoamWmmNoQos = 2,

} eCsrRoamWmmUserModeType;

typedef struct tagPmkidCandidateInfo {
	struct qdf_mac_addr BSSID;
	bool preAuthSupported;
} tPmkidCandidateInfo;

typedef struct tagPmkidCacheInfo {
	struct qdf_mac_addr BSSID;
	uint8_t PMKID[PMKID_LEN];
	uint8_t pmk[CSR_RSN_MAX_PMK_LEN];
	uint8_t pmk_len;
	uint8_t ssid_len;
	uint8_t ssid[WLAN_SSID_MAX_LEN];
	uint8_t cache_id[CACHE_ID_LEN];
	uint32_t   pmk_lifetime;
	uint8_t    pmk_lifetime_threshold;
	qdf_time_t pmk_ts;
} tPmkidCacheInfo;

#ifdef FEATURE_WLAN_WAPI
typedef struct tagBkidCandidateInfo {
	struct qdf_mac_addr BSSID;
	bool preAuthSupported;
} tBkidCandidateInfo;

typedef struct tagBkidCacheInfo {
	struct qdf_mac_addr BSSID;
	uint8_t BKID[CSR_WAPI_BKID_SIZE];
} tBkidCacheInfo;
#endif /* FEATURE_WLAN_WAPI */

typedef struct tagCsrKeys {
	/* Also use to indicate whether the key index is set */
	uint8_t KeyLength[CSR_MAX_NUM_KEY];
	uint8_t KeyMaterial[CSR_MAX_NUM_KEY][CSR_MAX_KEY_LEN];
	uint8_t defaultIndex;
} tCsrKeys;

/*
 * Following fields which're part of tCsrRoamConnectedProfile might need
 * modification dynamically once STA is up & running & this'd trigger reassoc
 */
typedef struct tagCsrRoamModifyProfileFields {
	/*
	 * during connect this specifies ACs U-APSD is to be setup
	 * for (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored).
	 * During assoc response this COULD carry confirmation of what
	 * ACs U-APSD got setup for. Later if an APP looking for APSD,
	 * SME-QoS might need to modify this field
	 */
	uint8_t uapsd_mask;
	/* HDD might ask to modify this field */
	uint16_t listen_interval;
} tCsrRoamModifyProfileFields;

struct csr_roam_profile {
	tCsrSSIDs SSIDs;
	tCsrBSSIDs BSSIDs;
	/* this is bit mask of all the needed phy mode defined in eCsrPhyMode */
	uint32_t phyMode;
	eCsrRoamBssType BSSType;
	tCsrAuthList AuthType;
	enum csr_akm_type negotiatedAuthType;
	tCsrAuthList akm_list;
	tCsrEncryptionList EncryptionType;
	/* This field is for output only, not for input */
	eCsrEncryptionType negotiatedUCEncryptionType;
	/*
	 * eCSR_ENCRYPT_TYPE_ANY cannot be set in multicast encryption type.
	 * If caller doesn't case, put all supported encryption types in here
	 */
	tCsrEncryptionList mcEncryptionType;
	/* This field is for output only, not for input */
	eCsrEncryptionType negotiatedMCEncryptionType;
#ifdef WLAN_FEATURE_11W
	/* Management Frame Protection */
	bool MFPEnabled;
	uint8_t MFPRequired;
	uint8_t MFPCapable;
#endif
	tCsrKeys Keys;
	tCsrChannelInfo ChannelInfo;
	uint32_t op_freq;
	struct ch_params ch_params;
	/* If this is 0, SME will fill in for caller. */
	uint16_t beaconInterval;
	/*
	 * during connect this specifies ACs U-APSD is to be setup
	 * for (Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE all other bits are ignored).
	 * During assoc resp this'd carry cnf of what ACs U-APSD got setup for
	 */
	uint8_t uapsd_mask;
	uint32_t nWPAReqIELength; /* The byte count in the pWPAReqIE */
	uint8_t *pWPAReqIE;       /* If not null,it's IE byte stream for WPA */
	uint32_t nRSNReqIELength; /* The byte count in the pRSNReqIE */
	uint8_t *pRSNReqIE;       /* If not null,it's IE byte stream for RSN */
#ifdef FEATURE_WLAN_WAPI
	uint32_t nWAPIReqIELength;/* The byte count in the pWAPIReqIE */
	uint8_t *pWAPIReqIE;      /* If not null,it's IE byte stream for WAPI */
#endif /* FEATURE_WLAN_WAPI */

	uint32_t nAddIEScanLength;/* pAddIE for scan (at the time of join) */
	/*
	 * If not null,it's the IE byte stream for additional IE,
	 * which can be WSC IE and/or P2P IE
	 */
	uint8_t *pAddIEScan;
	uint32_t nAddIEAssocLength; /* The byte count in the pAddIE for assoc */
	/*
	 * If not null, it has the IE byte stream for additional IE,
	 * which can be WSC IE and/or P2P IE
	 */
	uint8_t *pAddIEAssoc;
	/* it is ignored if [0] is 0. */
	uint8_t countryCode[REG_ALPHA2_LEN + 1];
	/* WPS Association if true => auth and ecryption should be ignored */
	bool bWPSAssociation;
	bool bOSENAssociation;
	uint8_t ieee80211d;
	uint8_t privacy;
	bool fwdWPSPBCProbeReq;
	tAniAuthType csr80211AuthType;
	uint32_t dtimPeriod;
	bool ApUapsdEnable;
	bool protEnabled;
	bool obssProtEnabled;
	bool chan_switch_hostapd_rate_enabled;
	uint16_t cfg_protection;
	uint8_t wps_state;
	struct mobility_domain_info mdid;
	enum QDF_OPMODE csrPersona;
	/* addIe params */
	struct add_ie_params add_ie_params;
	uint16_t beacon_tx_rate;
	tSirMacRateSet  supported_rates;
	tSirMacRateSet  extended_rates;
	struct qdf_mac_addr bssid_hint;
	bool force_24ghz_in_ht20;
	bool require_h2e;
	uint32_t cac_duration_ms;
	uint32_t dfs_regdomain;
#ifdef WLAN_FEATURE_FILS_SK
	uint8_t *hlp_ie;
	uint32_t hlp_ie_len;
	struct wlan_fils_connection_info *fils_con_info;
#endif
	bool force_rsne_override;
};

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
typedef struct tagCsrRoamHTProfile {
	uint8_t phymode;
	uint8_t htCapability;
	uint8_t htSupportedChannelWidthSet;
	uint8_t htRecommendedTxWidthSet;
	ePhyChanBondState htSecondaryChannelOffset;
	uint8_t vhtCapability;
	uint8_t apCenterChan;
	uint8_t apChanWidth;
} tCsrRoamHTProfile;
#endif

typedef struct tagCsrRoamConnectedProfile {
	tSirMacSSid SSID;
	bool handoffPermitted;
	bool ssidHidden;
	uint32_t op_freq;
	struct qdf_mac_addr bssid;
	uint16_t beaconInterval;
	eCsrRoamBssType BSSType;
	enum csr_akm_type AuthType;
	tCsrAuthList AuthInfo;
	tCsrAuthList akm_list;
	eCsrEncryptionType EncryptionType;
	tCsrEncryptionList EncryptionInfo;
	eCsrEncryptionType mcEncryptionType;
	tCsrEncryptionList mcEncryptionInfo;
	uint8_t country_code[WNI_CFG_COUNTRY_CODE_LEN];
	uint32_t vht_channel_width;
	tCsrKeys Keys;
	/*
	 * meaningless on connect. It's an OUT param from CSR's point of view
	 * During assoc response carries the ACM bit-mask i.e. what
	 * ACs have ACM=1 (if any),(Bit0:VO; Bit1:VI; Bit2:BK; Bit3:BE
	 * all other bits are ignored)
	 */
	uint8_t acm_mask;
	tCsrRoamModifyProfileFields modifyProfileFields;
	bool qosConnection;     /* A connection is QoS enabled */
	uint32_t nAddIEAssocLength;
	/*
	 * If not null,it's IE byte stream for additional IE,
	 * which can be WSC IE and/or P2P IE
	 */
	uint8_t *pAddIEAssoc;
	struct bss_description *bss_desc;
	bool qap;               /* AP supports QoS */
	struct mobility_domain_info mdid;
#ifdef FEATURE_WLAN_ESE
	tCsrEseCckmInfo eseCckmInfo;
	bool isESEAssoc;
#endif
	uint32_t dot11Mode;
	uint8_t proxy_arp_service;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	tCsrRoamHTProfile ht_profile;
#endif
#ifdef WLAN_FEATURE_11W
	/* Management Frame Protection */
	bool MFPEnabled;
	uint8_t MFPRequired;
	uint8_t MFPCapable;
#endif
} tCsrRoamConnectedProfile;

/**
 * enum sta_roam_policy_dfs_mode - state of DFS mode for STA ROME policy
 * @CSR_STA_ROAM_POLICY_NONE: DFS mode attribute is not valid
 * @CSR_STA_ROAM_POLICY_DFS_ENABLED:  DFS mode is enabled
 * @CSR_STA_ROAM_POLICY_DFS_DISABLED: DFS mode is disabled
 * @CSR_STA_ROAM_POLICY_DFS_DEPRIORITIZE: Deprioritize DFS channels in scanning
 */
enum sta_roam_policy_dfs_mode {
	CSR_STA_ROAM_POLICY_NONE,
	CSR_STA_ROAM_POLICY_DFS_ENABLED,
	CSR_STA_ROAM_POLICY_DFS_DISABLED,
	CSR_STA_ROAM_POLICY_DFS_DEPRIORITIZE
};

/**
 * struct csr_sta_roam_policy_params - sta roam policy params for station
 * @dfs_mode: tell is DFS channels needs to be skipped while scanning
 * @skip_unsafe_channels: tells if unsafe channels needs to be skip in scanning
 * @sap_operating_band: Opearting band for SAP
 */
struct csr_sta_roam_policy_params {
	enum sta_roam_policy_dfs_mode dfs_mode;
	bool skip_unsafe_channels;
	uint8_t sap_operating_band;
};

/**
 * struct csr_neighbor_report_offload_params - neighbor report offload params
 * @params_bitmask: bitmask to specify which of the below are enabled
 * @time_offset: time offset after 11k offload command to trigger a neighbor
 *		report request (in seconds)
 * @low_rssi_offset: Offset from rssi threshold to trigger neighbor
 *	report request (in dBm)
 * @bmiss_count_trigger: Number of beacon miss events to trigger neighbor
 *		report request
 * @per_threshold_offset: offset from PER threshold to trigger neighbor
 *		report request (in %)
 * @neighbor_report_cache_timeout: timeout after which new trigger can enable
 *		sending of a neighbor report request (in seconds)
 * @max_neighbor_report_req_cap: max number of neighbor report requests that
 *		can be sent to the peer in the current session
 */
struct csr_neighbor_report_offload_params {
	uint8_t params_bitmask;
	uint32_t time_offset;
	uint32_t low_rssi_offset;
	uint32_t bmiss_count_trigger;
	uint32_t per_threshold_offset;
	uint32_t neighbor_report_cache_timeout;
	uint32_t max_neighbor_report_req_cap;
};

struct csr_config_params {
	/* keep this uint32_t. This gets converted to ePhyChannelBondState */
	uint32_t channelBondingMode24GHz;
	uint8_t nud_fail_behaviour;
	uint32_t channelBondingMode5GHz;
	eCsrPhyMode phyMode;
	uint32_t HeartbeatThresh50;
	eCsrRoamWmmUserModeType WMMSupportMode;
	bool Is11eSupportEnabled;
	bool ProprietaryRatesEnabled;
	/*
	 * this number minus one is the number of times a scan doesn't find it
	 * before it is removed
	 */
	/* to set the RSSI difference for each category */
	uint8_t bCatRssiOffset;
	/* to set MCC Enable/Disable mode */
	uint8_t fEnableMCCMode;
	bool mcc_rts_cts_prot_enable;
	bool mcc_bcast_prob_resp_enable;
	/*
	 * To allow MCC GO different B.I than STA's.
	 * NOTE: make sure if RIVA firmware can handle this combination before
	 * enabling this at the moment, this flag is provided only to pass
	 * Wi-Fi Cert. 5.1.12
	 */
	uint8_t fAllowMCCGODiffBI;
	tCsr11dinfo Csr11dinfo;
#ifdef FEATURE_WLAN_ESE
	uint8_t isEseIniFeatureEnabled;
#endif
	uint8_t isFastRoamIniFeatureEnabled;
	struct mawc_params csr_mawc_config;
	/*
	 * Customer wants to optimize the scan time. Avoiding scans(passive)
	 * on DFS channels while swipping through both bands can save some time
	 * (apprx 1.3 sec)
	 */
	uint8_t fEnableDFSChnlScan;
	bool send_smps_action;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif
	uint8_t allowDFSChannelRoam;
	bool obssEnabled;
	uint8_t conc_custom_rule1;
	uint8_t conc_custom_rule2;
	uint8_t is_sta_connection_in_5gz_enabled;

	uint8_t max_intf_count;
	uint32_t f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	bool sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	uint32_t roam_dense_rssi_thresh_offset;
	uint32_t roam_dense_min_aps;
	int8_t roam_bg_scan_bad_rssi_thresh;
	uint8_t roam_bad_rssi_thresh_offset_2g;
	uint32_t roam_data_rssi_threshold_triggers;
	int32_t roam_data_rssi_threshold;
	uint32_t rx_data_inactivity_time;
	struct csr_sta_roam_policy_params sta_roam_policy_params;
	enum force_1x1_type is_force_1x1;
	uint32_t offload_11k_enable_bitmask;
	bool wep_tkip_in_he;
	struct csr_neighbor_report_offload_params neighbor_report_offload;
};

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define csr_is_roam_offload_enabled(mac) \
	(mac->mlme_cfg->lfr.lfr3_roaming_offload)
#define DEFAULT_REASSOC_FAILURE_TIMEOUT 1000
#else
#define csr_is_roam_offload_enabled(mac)  false
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/* connected but not authenticated */
#define CSR_ROAM_AUTH_STATUS_CONNECTED      0x1
/* connected and authenticated */
#define CSR_ROAM_AUTH_STATUS_AUTHENTICATED  0x2
#endif

struct csr_roam_info {
	struct csr_roam_profile *pProfile;
	struct bss_description *bss_desc;
	uint32_t nBeaconLength;
	uint32_t nAssocReqLength;
	uint32_t nAssocRspLength;
	uint32_t nFrameLength;
	uint8_t frameType;
	/*
	 * Point to a buffer contain the beacon, assoc req, assoc rsp frame,
	 * in that order user needs to use nBeaconLength, nAssocReqLength,
	 * nAssocRspLength to desice where each frame starts and ends.
	 */
	uint8_t *pbFrames;
	bool fReassocReq;       /* set to true if for re-association */
	bool fReassocRsp;       /* set to true if for re-association */
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peerMac;
	tSirResultCodes status_code;
	/* this'd be our own defined or sent from otherBSS(per 802.11spec) */
	uint32_t reasonCode;

	uint8_t disassoc_reason;

	uint8_t staId;         /* Peer stationId when connected */
	/* false means auth needed from supplicant. true means authenticated */
	bool fAuthRequired;
	uint8_t sessionId;
	uint8_t rsnIELen;
	uint8_t *prsnIE;
	uint8_t wapiIELen;
	uint8_t *pwapiIE;
	uint8_t addIELen;
	uint8_t *paddIE;
	union {
		tSirMicFailureInfo *pMICFailureInfo;
		tCsrRoamConnectedProfile *pConnectedProfile;
		tSirWPSPBCProbeReq *pWPSPBCProbeReq;
	} u;
	bool wmmEnabledSta;  /* set to true if WMM enabled STA */
	uint32_t dtimPeriod;
#ifdef FEATURE_WLAN_ESE
	bool isESEAssoc;
	struct tsm_ie tsm_ie;
	uint32_t timestamp[2];
	uint16_t tsmRoamDelay;
	struct ese_bcn_report_rsp *pEseBcnReportRsp;
#endif
#ifdef FEATURE_WLAN_TDLS
	/*
	 * TDLS parameters to check whether TDLS
	 * and TDLS channel switch is allowed in the
	 * AP network
	 */
	bool tdls_prohibited;           /* per ExtCap in Assoc/Reassoc resp */
	bool tdls_chan_swit_prohibited; /* per ExtCap in Assoc/Reassoc resp */
#endif
	/* Required for indicating the frames to upper layer */
	uint32_t beaconLength;
	uint8_t *beaconPtr;
	uint32_t assocReqLength;
	uint8_t *assocReqPtr;
	int8_t rxRssi;
	tSirSmeDfsEventInd dfs_event;
	tSirChanChangeResponse *channelChangeRespEvent;
	/* Timing and fine Timing measurement capability clubbed together */
	uint8_t timingMeasCap;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint8_t roamSynchInProgress;
	uint8_t synchAuthStatus;
	uint8_t kck[KCK_256BIT_KEY_LEN];
	uint8_t kck_len;
	uint8_t kek[SIR_KEK_KEY_LEN_FILS];
	uint8_t kek_len;
	uint32_t pmk_len;
	uint8_t pmk[SIR_PMK_LEN];
	uint8_t pmkid[PMKID_LEN];
	bool update_erp_next_seq_num;
	uint16_t next_erp_seq_num;
	uint8_t replay_ctr[SIR_REPLAY_CTR_LEN];
	uint8_t subnet_change_status;
#endif
	struct oem_channel_info chan_info;
	uint32_t target_chan_freq;

#ifdef WLAN_FEATURE_NAN
	union {
		struct ndi_create_rsp ndi_create_params;
		struct ndi_delete_rsp ndi_delete_params;
	} ndp;
#endif
	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	bool he_caps_present;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
	tDot11fIEVHTOperation vht_operation;
	tDot11fIEHTInfo ht_operation;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_op he_operation;
#endif
	bool reassoc;
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
	/* Extended capabilities of STA */
	uint8_t ecsa_capable;
	bool is_fils_connection;
#ifdef WLAN_FEATURE_FILS_SK
	uint16_t fils_seq_num;
	struct fils_join_rsp_params *fils_join_rsp;
#endif
	int rssi;
	int tx_rate;
	int rx_rate;
	tSirMacCapabilityInfo capability_info;
	uint32_t rx_mc_bc_cnt;
	uint32_t rx_retry_cnt;
#ifdef WLAN_FEATURE_SAE
	struct sir_sae_info *sae_info;
#endif
	struct assoc_ind *owe_pending_assoc_ind;
	uint16_t roam_reason;
	struct wlan_ies *disconnect_ies;
};

typedef struct sSirSmeAssocIndToUpperLayerCnf {
	uint16_t messageType;   /* eWNI_SME_ASSOC_CNF */
	uint16_t length;
	uint8_t sessionId;
	tSirResultCodes status_code;
	tSirMacAddr bssId;      /* Self BSSID */
	tSirMacAddr peerMacAddr;
	uint16_t aid;
	uint8_t wmmEnabledSta;  /* set to true if WMM enabled STA */
	tSirRSNie rsnIE;        /* RSN IE received from peer */
	tSirWAPIie wapiIE;      /* WAPI IE received from peer */
	tSirAddie addIE;        /* this can be WSC and/or P2P IE */
	uint8_t reassocReq;     /* set to true if reassoc */
	/* Timing and fine Timing measurement capability clubbed together */
	uint8_t timingMeasCap;
	struct oem_channel_info chan_info;
	uint8_t target_channel;
	bool ampdu;
	bool sgi_enable;
	bool tx_stbc;
	tSirMacHTChannelWidth ch_width;
	enum sir_sme_phy_mode mode;
	bool rx_stbc;
	uint8_t max_supp_idx;
	uint8_t max_ext_idx;
	uint8_t max_mcs_idx;
	uint8_t rx_mcs_map;
	uint8_t tx_mcs_map;
	/* Extended capabilities of STA */
	uint8_t              ecsa_capable;

	uint32_t ies_len;
	uint8_t *ies;
	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	tSirMacCapabilityInfo capability_info;
	bool he_caps_present;
} tSirSmeAssocIndToUpperLayerCnf, *tpSirSmeAssocIndToUpperLayerCnf;

typedef struct tagCsrSummaryStatsInfo {
	uint32_t snr;
	int8_t rssi;
	uint32_t retry_cnt[4];
	uint32_t multiple_retry_cnt[4];
	uint32_t tx_frm_cnt[4];
	/* uint32_t num_rx_frm_crc_err; same as rx_error_cnt */
	/* uint32_t num_rx_frm_crc_ok; same as rx_frm_cnt */
	uint32_t rx_frm_cnt;
	uint32_t frm_dup_cnt;
	uint32_t fail_cnt[4];
	uint32_t rts_fail_cnt;
	uint32_t ack_fail_cnt;
	uint32_t rts_succ_cnt;
	uint32_t rx_discard_cnt;
	uint32_t rx_error_cnt;
	uint32_t tx_byte_cnt;

} tCsrSummaryStatsInfo;

typedef struct tagCsrGlobalClassAStatsInfo {
	uint8_t tx_nss;
	uint8_t rx_nss;
	uint32_t max_pwr;
	uint32_t tx_rate;
	uint32_t rx_rate;
	/* mcs index for HT20 and HT40 rates */
	uint32_t tx_mcs_index;
	uint32_t rx_mcs_index;
	enum tx_rate_info tx_mcs_rate_flags;
	enum tx_rate_info rx_mcs_rate_flags;
	uint8_t  tx_dcm;
	uint8_t  rx_dcm;
	enum txrate_gi  tx_gi;
	enum txrate_gi  rx_gi;
	/* to diff between HT20 & HT40 rates;short & long guard interval */
	enum tx_rate_info tx_rx_rate_flags;

} tCsrGlobalClassAStatsInfo;

typedef struct tagCsrGlobalClassDStatsInfo {
	uint32_t tx_uc_frm_cnt;
	uint32_t tx_mc_frm_cnt;
	uint32_t tx_bc_frm_cnt;
	uint32_t rx_uc_frm_cnt;
	uint32_t rx_mc_frm_cnt;
	uint32_t rx_bc_frm_cnt;
	uint32_t tx_uc_byte_cnt[4];
	uint32_t tx_mc_byte_cnt;
	uint32_t tx_bc_byte_cnt;
	uint32_t rx_uc_byte_cnt[4];
	uint32_t rx_mc_byte_cnt;
	uint32_t rx_bc_byte_cnt;
	uint32_t rx_byte_cnt;
	uint32_t num_rx_bytes_crc_ok;
	uint32_t rx_rate;

} tCsrGlobalClassDStatsInfo;

/**
 * struct csr_per_chain_rssi_stats_info - stores chain rssi
 * @rssi: array containing rssi for all chains
 * @peer_mac_addr: peer mac address
 */
struct csr_per_chain_rssi_stats_info {
	int8_t rssi[NUM_CHAINS_MAX];
	tSirMacAddr peer_mac_addr;
};

typedef struct tagCsrRoamSetKey {
	eCsrEncryptionType encType;
	tAniKeyDirection keyDirection;  /* Tx, Rx or Tx-and-Rx */
	struct qdf_mac_addr peerMac;    /* Peer MAC. ALL 1's for group key */
	uint8_t paeRole;        /* 0 for supplicant */
	uint8_t keyId;          /* Key index */
	uint16_t keyLength;     /* Number of bytes containing the key in pKey */
	uint8_t Key[CSR_MAX_KEY_LEN];
	uint8_t keyRsc[WLAN_CRYPTO_RSC_SIZE];
} tCsrRoamSetKey;

typedef void *tScanResultHandle;

typedef enum {
	REASSOC = 0,
	FASTREASSOC = 1,
	CONNECT_CMD_USERSPACE = 2,
} handoff_src;

typedef struct tagCsrHandoffRequest {
	struct qdf_mac_addr bssid;
	uint32_t ch_freq;
	uint8_t src;   /* To check if its a REASSOC or a FASTREASSOC IOCTL */
} tCsrHandoffRequest;

#ifdef FEATURE_WLAN_ESE
typedef struct tagCsrEseBeaconReqParams {
	uint16_t measurementToken;
	uint32_t ch_freq;
	uint8_t scanMode;
	uint16_t measurementDuration;
} tCsrEseBeaconReqParams, *tpCsrEseBeaconReqParams;

typedef struct tagCsrEseBeaconReq {
	uint8_t numBcnReqIe;
	tCsrEseBeaconReqParams bcnReq[SIR_ESE_MAX_MEAS_IE_REQS];
} tCsrEseBeaconReq, *tpCsrEseBeaconReq;
#endif /* FEATURE_WLAN_ESE */

struct csr_del_sta_params {
	struct qdf_mac_addr peerMacAddr;
	uint16_t reason_code;
	uint8_t subtype;
};

typedef QDF_STATUS (*csr_roam_complete_cb)(struct wlan_objmgr_psoc *psoc,
					   uint8_t session_id,
					   struct csr_roam_info *param,
					   uint32_t roam_id,
					   eRoamCmdStatus roam_status,
					   eCsrRoamResult roam_result);
typedef QDF_STATUS (*csr_session_open_cb)(uint8_t session_id,
					  QDF_STATUS qdf_status);
typedef QDF_STATUS (*csr_session_close_cb)(uint8_t session_id);

#define CSR_IS_INFRASTRUCTURE(pProfile) (eCSR_BSS_TYPE_INFRASTRUCTURE == \
					 (pProfile)->BSSType)
#define CSR_IS_ANY_BSS_TYPE(pProfile) (eCSR_BSS_TYPE_ANY == \
				       (pProfile)->BSSType)
#define CSR_IS_INFRA_AP(pProfile) (eCSR_BSS_TYPE_INFRA_AP ==  \
				   (pProfile)->BSSType)
#ifdef WLAN_FEATURE_NAN
#define CSR_IS_NDI(profile)  (eCSR_BSS_TYPE_NDI == (profile)->BSSType)
#else
#define CSR_IS_NDI(profile)  (false)
#endif
#define CSR_IS_CONN_INFRA_AP(pProfile)  (eCSR_BSS_TYPE_INFRA_AP == \
					 (pProfile)->BSSType)
#ifdef WLAN_FEATURE_NAN
#define CSR_IS_CONN_NDI(profile)  (eCSR_BSS_TYPE_NDI == (profile)->BSSType)
#else
#define CSR_IS_CONN_NDI(profile)  (false)
#endif

#ifdef WLAN_FEATURE_SAE
#define CSR_IS_AUTH_TYPE_SAE(auth_type) \
	(eCSR_AUTH_TYPE_SAE == auth_type)

#define CSR_IS_AKM_FT_SAE(auth_type) \
	(eCSR_AUTH_TYPE_FT_SAE == (auth_type))

#define CSR_IS_FW_FT_SAE_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_FT_SAE)) ? true : false)

#define CSR_IS_FW_SAE_ROAM_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_SAE)) ? true : false)
#else
#define CSR_IS_AUTH_TYPE_SAE(auth_type) (false)

#define CSR_IS_AKM_FT_SAE(auth_type) (false)

#define CSR_IS_FW_FT_SAE_SUPPORTED(fw_akm_bitmap) (false)

#define CSR_IS_FW_SAE_ROAM_SUPPORTED(fw_akm_bitmap) (false)
#endif

#define CSR_IS_FW_OWE_ROAM_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_OWE)) ? true : false)

#define CSR_IS_AKM_FT_SUITEB_SHA384(auth_type) \
	(eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384 == (auth_type))

#define CSR_IS_AKM_FILS(auth_type) \
	((eCSR_AUTH_TYPE_FILS_SHA256 == auth_type) || \
	 (eCSR_AUTH_TYPE_FILS_SHA384 == auth_type))

#define CSR_IS_AKM_FT_FILS(auth_type) \
	((eCSR_AUTH_TYPE_FT_FILS_SHA256 == (auth_type)) || \
	 (eCSR_AUTH_TYPE_FT_FILS_SHA384 == (auth_type)))

#define CSR_IS_FW_FT_SUITEB_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_FT_SUITEB_SHA384))  ? true : false)

#define CSR_IS_FW_FT_FILS_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_FT_FILS))  ? true : false)

#define CSR_IS_FW_SUITEB_ROAM_SUPPORTED(fw_akm_bitmap) \
	(((fw_akm_bitmap) & (1 << AKM_SUITEB))  ? true : false)

QDF_STATUS csr_set_channels(struct mac_context *mac,
			    struct csr_config_params *pParam);

/* enum to string conversion for debug output */
const char *get_e_roam_cmd_status_str(eRoamCmdStatus val);
const char *get_e_csr_roam_result_str(eCsrRoamResult val);
const char *csr_phy_mode_str(eCsrPhyMode phy_mode);

#ifdef FEATURE_WLAN_ESE
typedef void (*tCsrTsmStatsCallback)(tAniTrafStrmMetrics tsmMetrics,
				     void *pContext);
#endif /* FEATURE_WLAN_ESE */
typedef void (*tCsrSnrCallback)(int8_t snr, void *pContext);

/**
 * csr_roam_issue_ft_preauth_req() - Initiate Preauthentication request
 * @max_ctx: Global MAC context
 * @session_id: SME Session ID
 * @bss_desc: BSS descriptor
 *
 * Return: Success or Failure
 */
#ifdef WLAN_FEATURE_HOST_ROAM
QDF_STATUS csr_roam_issue_ft_preauth_req(struct mac_context *mac_ctx,
					 uint32_t session_id,
					 struct bss_description *bss_desc);

QDF_STATUS csr_continue_lfr2_connect(struct mac_context *mac,
				     uint32_t session_id);
#else
static inline
QDF_STATUS csr_roam_issue_ft_preauth_req(struct mac_context *mac_ctx,
					 uint32_t session_id,
					 struct bss_description *bss_desc)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS csr_continue_lfr2_connect(struct mac_context *mac,
				     uint32_t session_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

typedef void (*csr_readyToSuspendCallback)(void *pContext, bool suspended);
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
typedef void (*csr_readyToExtWoWCallback)(void *pContext, bool status);
#endif
typedef void (*csr_link_status_callback)(uint8_t status, void *context);
#ifdef FEATURE_WLAN_TDLS
void csr_roam_fill_tdls_info(struct mac_context *mac_ctx,
			     struct csr_roam_info *roam_info,
			     struct join_rsp *join_rsp);
#else
static inline void csr_roam_fill_tdls_info(struct mac_context *mac_ctx,
					   struct csr_roam_info *roam_info,
					   struct join_rsp *join_rsp)
{}
#endif

typedef void (*sme_get_raom_scan_ch_callback)(
				hdd_handle_t hdd_handle,
				struct roam_scan_ch_resp *roam_ch,
				void *context);

#if defined(WLAN_LOGGING_SOCK_SVC_ENABLE) && \
	defined(FEATURE_PKTLOG) && !defined(REMOVE_PKT_LOG)
/**
 * csr_packetdump_timer_stop() - stops packet dump timer
 *
 * This function is used to stop packet dump timer
 *
 * Return: None
 *
 */
void csr_packetdump_timer_stop(void);

/**
 * csr_packetdump_timer_start() - start packet dump timer
 *
 * This function is used to start packet dump timer
 *
 * Return: None
 *
 */
void csr_packetdump_timer_start(void);
#else
static inline void csr_packetdump_timer_stop(void) {}
static inline void csr_packetdump_timer_start(void) {}
#endif

/**
 * csr_get_channel_status() - get chan info via channel number
 * @mac: Pointer to Global MAC structure
 * @chan_freq: channel frequency
 *
 * Return: chan status info
 */
struct lim_channel_status *
csr_get_channel_status(struct mac_context *mac, uint32_t chan_freq);

/**
 * csr_clear_channel_status() - clear chan info
 * @mac: Pointer to Global MAC structure
 *
 * Return: none
 */
void csr_clear_channel_status(struct mac_context *mac);

/**
 * csr_update_owe_info() - Update OWE info
 * @mac: mac context
 * @assoc_ind: assoc ind
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_update_owe_info(struct mac_context *mac,
			       struct assoc_ind *assoc_ind);

typedef void (*csr_ani_callback)(int8_t *ani, void *context);

#ifdef WLAN_FEATURE_11W
/**
 * csr_update_pmf_cap_from_connected_profile() - Update pmf cap from profile
 * @profile: connected profile
 * @filter: scan filter
 *
 * Return: None
 */
void
csr_update_pmf_cap_from_connected_profile(tCsrRoamConnectedProfile *profile,
					  struct scan_filter *filter);
#else
void
csr_update_pmf_cap_from_connected_profile(tCsrRoamConnectedProfile *profile,
					  struct scan_filter *filter);
#endif

/*
 * csr_convert_to_reg_phy_mode() - CSR API to convert CSR phymode into
 * regulatory phymode
 * @csr_phy_mode: csr phymode with type eCsrPhyMode
 * @freq: current operating frequency
 *
 * This API is used to convert a phymode from CSR to a phymode from regulatory
 *
 * Return: regulatory phymode that is comparable to input
 */
enum reg_phymode csr_convert_to_reg_phy_mode(eCsrPhyMode csr_phy_mode,
				       qdf_freq_t freq);

/*
 * csr_convert_from_reg_phy_mode() - CSR API to convert regulatory phymode into
 * CSR phymode
 * @reg_phymode: regulatory phymode
 *
 * This API is used to convert a regulatory phymode to a CSR phymode
 *
 * Return: eCSR phymode that is comparable to input
 */
eCsrPhyMode csr_convert_from_reg_phy_mode(enum reg_phymode phymode);

/*
 * csr_update_beacon() - CSR API to update beacon template
 * @mac: mac context
 *
 * This API is used to update beacon template to FW
 *
 * Return: None
 */
void csr_update_beacon(struct mac_context *mac);

/*
 * csr_mlme_vdev_disconnect_all_p2p_client_event() - Callback for MLME module
 *	to send a disconnect all P2P event to the SAP event handler
 * @vdev_id: vdev id of SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_mlme_vdev_disconnect_all_p2p_client_event(uint8_t vdev_id);

/*
 * csr_mlme_vdev_stop_bss() - Callback for MLME module to send a stop BSS event
 *	to the SAP event handler
 * @vdev_id: vdev id of SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_mlme_vdev_stop_bss(uint8_t vdev_id);

/*
 * csr_mlme_get_concurrent_operation_freq() - Callback for MLME module to
 *	get the concurrent operation frequency
 *
 * Return: concurrent frequency
 */
qdf_freq_t csr_mlme_get_concurrent_operation_freq(void);

#endif
