/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * This file parser_api.h contains the definitions used
 * for parsing received 802.11 frames
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __PARSE_H__
#define __PARSE_H__

#include <stdarg.h>
#include "sir_mac_prop_exts.h"
#include "dot11f.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "wlan_mlme_main.h"

#define COUNTRY_STRING_LENGTH    (3)
#define COUNTRY_INFO_MAX_CHANNEL (84)
#define MAX_SIZE_OF_TRIPLETS_IN_COUNTRY_IE (COUNTRY_STRING_LENGTH * \
						COUNTRY_INFO_MAX_CHANNEL)
#define HIGHEST_24GHZ_CHANNEL_NUM  (14)

#define IS_24G_CH(__chNum) ((__chNum > 0) && (__chNum < 15))
#define IS_5G_CH(__chNum) ((__chNum >= 36) && (__chNum <= 165))
#define IS_2X2_CHAIN(__chain) ((__chain & 0x3) == 0x3)
#define DISABLE_NSS2_MCS 0xC
#define VHT_1x1_MCS9_MAP 0x2
#define VHT_2x2_MCS9_MAP 0xA
#define VHT_1x1_MCS8_VAL 0xFFFD
#define VHT_2x2_MCS8_VAL 0xFFF5
#define VHT_1x1_MCS_MASK 0x3
#define VHT_2x2_MCS_MASK 0xF
#define DISABLE_VHT_MCS_9(mcs, nss) \
	(mcs = (nss > 1) ? VHT_2x2_MCS8_VAL : VHT_1x1_MCS8_VAL)

#define NSS_1x1_MODE 1
#define NSS_2x2_MODE 2
#define NSS_3x3_MODE 3
#define NSS_4x4_MODE 4
#define MBO_IE_ASSOC_DISALLOWED_SUBATTR_ID 0x04

/* QCN IE definitions */
#define QCN_IE_HDR_LEN     6

#define QCN_IE_VERSION_SUBATTR_ID        1
#define QCN_IE_VERSION_SUBATTR_DATA_LEN  2
#define QCN_IE_VERSION_SUBATTR_LEN       4
#define QCN_IE_VERSION_SUPPORTED    1
#define QCN_IE_SUBVERSION_SUPPORTED 0

#define QCN_IE_ATTR_ID_VERSION 1
#define QCN_IE_ATTR_ID_VHT_MCS11 2
#define QCN_IE_ATTR_ID_ALL 0xFF

#define SIZE_OF_FIXED_PARAM 12
#define SIZE_OF_TAG_PARAM_NUM 1
#define SIZE_OF_TAG_PARAM_LEN 1
#define RSNIEID 0x30
#define RSNIE_CAPABILITY_LEN 2
#define DEFAULT_RSNIE_CAP_VAL 0x00

#define SIZE_MASK 0x7FFF
#define FIXED_MASK 0x8000

#define MAX_TPE_IES 8

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
#define QCOM_VENDOR_IE_MCC_AVOID_CH 0x01

struct sAvoidChannelIE {
	/* following must be 0xDD (221) */
	uint8_t tag_number;
	uint8_t length;
	/* following must be 00-A0-C6 */
	uint8_t oui[3];
	/* following must be 0x01 */
	uint8_t type;
	uint8_t channel;
};
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

typedef struct sSirCountryInformation {
	uint8_t countryString[COUNTRY_STRING_LENGTH];
	uint8_t numIntervals;   /* number of channel intervals */
	struct channelPowerLim {
		uint8_t channelNumber;
		uint8_t numChannel;
		uint8_t maxTransmitPower;
	} channelTransmitPower[COUNTRY_INFO_MAX_CHANNEL];
} tSirCountryInformation, *tpSirCountryInformation;

#ifdef WLAN_FEATURE_FILS_SK
#define SIR_MAX_IDENTIFIER_CNT 7
#define SIR_CACHE_IDENTIFIER_LEN 2
#define SIR_HESSID_LEN 6
#define SIR_MAX_KEY_CNT 7
#define SIR_MAX_KEY_LEN 48
#define SIR_FILS_IND_ELEM_OFFSET 2
/*
 * struct public_key_identifier: structure for public key identifier
 * present in fils indication element
 * @is_present: if Key info is present
 * @key_cnt:  number of keys present
 * @key_type: type of key used
 * @length: length of key
 * @key: key data
 */
struct public_key_identifier {
	bool is_present;
	uint8_t key_cnt;
	uint8_t key_type;
	uint8_t length;
	uint8_t key[SIR_MAX_KEY_CNT][SIR_MAX_KEY_LEN];
};

/*
 * struct fils_cache_identifier: structure for fils cache identifier
 * present in fils indication element
 * @is_present: if cache identifier is present
 * @identifier: cache identifier
 */
struct fils_cache_identifier {
	bool is_present;
	uint8_t identifier[SIR_CACHE_IDENTIFIER_LEN];
};

/*
 * struct fils_hessid: structure for fils hessid
 * present in fils indication element
 * @is_present: if hessid info is present
 * @hessid: hessid data
 */
struct fils_hessid {
	bool is_present;
	uint8_t hessid[SIR_HESSID_LEN];
};

/*
 * struct fils_realm_identifier: structure for fils_realm_identifier
 * present in fils indication element
 * @is_present: if realm info is present
 * @realm_cnt: realm count
 * @realm: realm data
 */
struct fils_realm_identifier {
	bool is_present;
	uint8_t realm_cnt;
	uint8_t realm[SIR_MAX_REALM_COUNT][SIR_REALM_LEN];
};

/*
 * struct sir_fils_indication: structure for fils indication element
 * @is_present: if indication element is present
 * @is_ip_config_supported: if IP config is supported
 * @is_fils_sk_auth_supported: if fils sk suppprted
 * @is_fils_sk_auth_pfs_supported: if fils sk with pfs supported
 * @is_pk_auth_supported: if fils public key supported
 * @cache_identifier: fils cache idenfier info
 * @hessid: fils hessid info
 * @realm_identifier: fils realm info
 * @key_identifier: fils key identifier info
 */
struct sir_fils_indication {
	bool is_present;
	uint8_t is_ip_config_supported;
	uint8_t is_fils_sk_auth_supported;
	uint8_t is_fils_sk_auth_pfs_supported;
	uint8_t is_pk_auth_supported;
	struct fils_cache_identifier cache_identifier;
	struct fils_hessid hessid;
	struct fils_realm_identifier realm_identifier;
	struct public_key_identifier key_identifier;
};
#endif

enum operating_class_num {
	OP_CLASS_131 = 131,
	OP_CLASS_132,
	OP_CLASS_133,
	OP_CLASS_134,
	OP_CLASS_135,
	OP_CLASS_136,
};

enum operating_extension_identifier {
	OP_CLASS_ID_200 = 200,
	OP_CLASS_ID_201,
};

/* Structure common to Beacons & Probe Responses */
typedef struct sSirProbeRespBeacon {
	tSirMacTimeStamp timeStamp;
	uint16_t beaconInterval;
	tSirMacCapabilityInfo capabilityInfo;

	tSirMacSSid ssId;
	tSirMacRateSet supportedRates;
	tSirMacRateSet extendedRates;
	uint32_t chan_freq;
	tSirMacCfParamSet cfParamSet;
	tSirMacTim tim;
	tSirMacEdcaParamSetIE edcaParams;
	tSirMacQosCapabilityIE qosCapability;

	tSirCountryInformation countryInfoParam;
	tSirMacWpaInfo wpa;
	tSirMacRsnInfo rsn;

	tSirMacErpInfo erpIEInfo;

	tDot11fIEPowerConstraints localPowerConstraint;
	tDot11fIETPCReport tpcReport;
	tDot11fIEChanSwitchAnn channelSwitchIE;
	tDot11fIEsec_chan_offset_ele sec_chan_offset;
	tDot11fIEext_chan_switch_ann ext_chan_switch;
	tDot11fIESuppOperatingClasses supp_operating_classes;
	tSirMacAddr bssid;
	tDot11fIEQuiet quietIE;
	tDot11fIEHTCaps HTCaps;
	tDot11fIEHTInfo HTInfo;
	tDot11fIEP2PProbeRes P2PProbeRes;
	uint8_t mdie[SIR_MDIE_SIZE];
#ifdef FEATURE_WLAN_ESE
	tDot11fIEESETxmitPower eseTxPwr;
	tDot11fIEQBSSLoad QBSSLoad;
#endif
	uint8_t ssidPresent;
	uint8_t suppRatesPresent;
	uint8_t extendedRatesPresent;
	uint8_t supp_operating_class_present;
	uint8_t cfPresent;
	uint8_t dsParamsPresent;
	uint8_t timPresent;

	uint8_t edcaPresent;
	uint8_t qosCapabilityPresent;
	uint8_t wmeEdcaPresent;
	uint8_t wmeInfoPresent;
	uint8_t wsmCapablePresent;

	uint8_t countryInfoPresent;
	uint8_t wpaPresent;
	uint8_t rsnPresent;
	uint8_t erpPresent;
	uint8_t channelSwitchPresent;
	uint8_t sec_chan_offset_present;
	uint8_t ext_chan_switch_present;
	uint8_t quietIEPresent;
	uint8_t tpcReportPresent;
	uint8_t powerConstraintPresent;

	uint8_t mdiePresent;

	tDot11fIEVHTCaps VHTCaps;
	tDot11fIEVHTOperation VHTOperation;
	tDot11fIEVHTExtBssLoad VHTExtBssLoad;
	tDot11fIEExtCap ext_cap;
	tDot11fIEOperatingMode OperatingMode;
	uint8_t WiderBWChanSwitchAnnPresent;
	tDot11fIEWiderBWChanSwitchAnn WiderBWChanSwitchAnn;
	uint8_t Vendor1IEPresent;
	tDot11fIEvendor_vht_ie vendor_vht_ie;
	uint8_t Vendor3IEPresent;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	tDot11fIEQComVendorIE   AvoidChannelIE;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
#ifdef FEATURE_WLAN_ESE
	uint8_t    is_ese_ver_ie_present;
#endif
	tDot11fIEOBSSScanParameters obss_scanparams;
	bool MBO_IE_present;
	uint8_t MBO_capability;
	bool assoc_disallowed;
	uint8_t assoc_disallowed_reason;
	tDot11fIEqcn_ie qcn_ie;
	tDot11fIEhe_cap he_cap;
	tDot11fIEhe_op he_op;
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
	tDot11fIEbss_color_change vendor_he_bss_color_change;
#endif
#ifdef WLAN_FEATURE_FILS_SK
	struct sir_fils_indication fils_ind;
#endif
	uint8_t num_transmit_power_env;
	tDot11fIEtransmit_power_env transmit_power_env[MAX_TPE_IES];
	uint8_t ap_power_type;
} tSirProbeRespBeacon, *tpSirProbeRespBeacon;

/* probe Request structure */
typedef struct sSirProbeReq {
	tSirMacSSid ssId;
	tSirMacRateSet supportedRates;
	tSirMacRateSet extendedRates;
	tDot11fIEWscProbeReq probeReqWscIeInfo;
	tDot11fIEHTCaps HTCaps;
	uint8_t ssidPresent;
	uint8_t suppRatesPresent;
	uint8_t extendedRatesPresent;
	uint8_t wscIePresent;
	uint8_t p2pIePresent;
	tDot11fIEVHTCaps VHTCaps;
	tDot11fIEhe_cap he_cap;
} tSirProbeReq, *tpSirProbeReq;

/* / Association Request structure (one day to be replaced by */
/* / tDot11fAssocRequest) */
typedef struct sSirAssocReq {

	tSirMacCapabilityInfo capabilityInfo;
	uint16_t listenInterval;
	tSirMacAddr currentApAddr;      /* only in reassoc frames */
	tSirMacSSid ssId;
	tSirMacRateSet supportedRates;
	tSirMacRateSet extendedRates;

	tSirAddtsReqInfo addtsReq;
	tSirMacQosCapabilityStaIE qosCapability;

	tSirMacWapiInfo wapi;
	tSirMacWpaInfo wpa;
	tSirMacRsnInfo rsn;
	tSirAddie addIE;

	tSirMacPowerCapabilityIE powerCapability;
	tSirMacSupportedChannelIE supportedChannels;
	tDot11fIEHTCaps HTCaps;
	tDot11fIEWMMInfoStation WMMInfoStation;
	tDot11fIESuppOperatingClasses supp_operating_classes;
	/* / This is set if the frame is a reassoc request: */
	uint8_t reassocRequest;
	uint8_t ssidPresent;
	uint8_t suppRatesPresent;
	uint8_t extendedRatesPresent;

	uint8_t wmeInfoPresent;
	uint8_t qosCapabilityPresent;
	uint8_t addtsPresent;
	uint8_t wsmCapablePresent;

	uint8_t wapiPresent;
	uint8_t wpaPresent;
	uint8_t rsnPresent;
	uint8_t addIEPresent;

	uint8_t powerCapabilityPresent;
	uint8_t supportedChannelsPresent;
	/* keeping copy of association request received, this is
	   required for indicating the frame to upper layers */
	uint32_t assocReqFrameLength;
	uint8_t *assocReqFrame;
	tDot11fIEVHTCaps VHTCaps;
	tDot11fIEOperatingMode operMode;
	tDot11fIEExtCap ExtCap;
	tDot11fIEvendor_vht_ie vendor_vht_ie;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
	tDot11fIEhe_cap he_cap;
	tDot11fIEhe_6ghz_band_cap he_6ghz_band_cap;
	tDot11fIEqcn_ie qcn_ie;
	bool is_sae_authenticated;
} tSirAssocReq, *tpSirAssocReq;

#define FTIE_SUBELEM_R1KH_ID 1
#define FTIE_SUBELEM_GTK     2
#define FTIE_SUBELEM_R0KH_ID 3
#define FTIE_SUBELEM_IGTK    4
#define FTIE_SUBELEM_OCI     5

#define FTIE_R1KH_LEN 6
#define FTIE_R0KH_MAX_LEN 48

/**
 * struct wlan_sha384_ftinfo_subelem - subelements of FTIE
 * @r1kh_id: FT R1 Key holder ID
 * @gtk: Ft group temporal key
 * @gtk_len: GTK length
 * @r0kh_id: FT R0 Key Holder ID
 * @igtk: FT IGTK used for 11w
 * @igtk_len: IGTK length
 */
struct wlan_sha384_ftinfo_subelem {
	tDot11fIER1KH_ID r1kh_id;
	uint8_t *gtk;
	uint8_t gtk_len;
	tDot11fIER0KH_ID r0kh_id;
	uint8_t *igtk;
	uint8_t igtk_len;
};

#define MIC_CONTROL_BYTES 2
#define MIC_SHA384_BYTES  24
#define NONCE_BYTES       32

/**
 * struct wlan_sha384_ftinfo - FTE for sha384 based AKMs
 * @mic_control: FTIE mic control field of 2 bytes
 * @mic: MIC present in the FTIE assoc Response
 * @anonce: Anonce sent by the AP
 * @snonce: Snonce field in the FTIE
 */
struct wlan_sha384_ftinfo {
	uint8_t mic_control[MIC_CONTROL_BYTES];
	uint8_t mic[MIC_SHA384_BYTES];
	uint8_t anonce[NONCE_BYTES];
	uint8_t snonce[NONCE_BYTES];
};

/* / Association Response structure (one day to be replaced by */
/* / tDot11fAssocRequest) */
typedef struct sSirAssocRsp {

	tSirMacCapabilityInfo capabilityInfo;
	uint16_t aid;
	uint16_t status_code;
	tSirMacRateSet supportedRates;
	tSirMacRateSet extendedRates;
	tSirMacEdcaParamSetIE edca;
	tSirAddtsRspInfo addtsRsp;
	tDot11fIEHTCaps HTCaps;
	tDot11fIEHTInfo HTInfo;
	tDot11fIEFTInfo FTInfo;
	struct wlan_sha384_ftinfo sha384_ft_info;
	struct wlan_sha384_ftinfo_subelem sha384_ft_subelem;
	uint8_t mdie[SIR_MDIE_SIZE];
	uint8_t num_RICData;
	tDot11fIERICDataDesc RICData[2];

#ifdef FEATURE_WLAN_ESE
	uint8_t num_tspecs;
	tDot11fIEWMMTSPEC TSPECInfo[SIR_ESE_MAX_TSPEC_IES];
	struct ese_tsm_ie tsmIE;
#endif

	uint8_t suppRatesPresent;
	uint8_t extendedRatesPresent;

	uint8_t edcaPresent;
	uint8_t wmeEdcaPresent;
	uint8_t addtsPresent;
	uint8_t wsmCapablePresent;
	uint8_t ftinfoPresent;
	uint8_t mdiePresent;
	uint8_t ricPresent;
#ifdef FEATURE_WLAN_ESE
	uint8_t tspecPresent;
	uint8_t tsmPresent;
#endif
	tDot11fIEVHTCaps VHTCaps;
	tDot11fIEVHTOperation VHTOperation;
	tDot11fIEExtCap ExtCap;
	struct qos_map_set QosMapSet;
#ifdef WLAN_FEATURE_11W
	tDot11fIETimeoutInterval TimeoutInterval;
#endif
	tDot11fIERRMEnabledCap rrm_caps;
	tDot11fIEvendor_vht_ie vendor_vht_ie;
	tDot11fIEOBSSScanParameters obss_scanparams;
	tDot11fTLVrssi_assoc_rej rssi_assoc_rej;
	tDot11fIEqcn_ie qcn_ie;
	tDot11fIEhe_cap he_cap;
	tDot11fIEhe_op he_op;
	tDot11fIEhe_6ghz_band_cap he_6ghz_band_cap;
	bool mu_edca_present;
	tSirMacEdcaParamSetIE mu_edca;
#ifdef WLAN_FEATURE_FILS_SK
	tDot11fIEfils_session fils_session;
	tDot11fIEfils_key_confirmation fils_key_auth;
	tDot11fIEfils_kde fils_kde;
	struct qdf_mac_addr dst_mac;
	struct qdf_mac_addr src_mac;
	uint16_t hlp_data_len;
	uint8_t hlp_data[FILS_MAX_HLP_DATA_LEN];
#endif
} tSirAssocRsp, *tpSirAssocRsp;

#ifdef FEATURE_WLAN_ESE
/* Structure to hold ESE Beacon report mandatory IEs */
typedef struct sSirEseBcnReportMandatoryIe {
	tSirMacSSid ssId;
	tSirMacRateSet supportedRates;
	tSirMacFHParamSet fhParamSet;
	tSirMacDsParamSetIE dsParamSet;
	tSirMacCfParamSet cfParamSet;
	tSirMacTim tim;
	tSirMacRRMEnabledCap rmEnabledCapabilities;

	uint8_t ssidPresent;
	uint8_t suppRatesPresent;
	uint8_t fhParamPresent;
	uint8_t dsParamsPresent;
	uint8_t cfPresent;
	uint8_t timPresent;
	uint8_t rrmPresent;
} tSirEseBcnReportMandatoryIe, *tpSirEseBcnReportMandatoryIe;
#endif /* FEATURE_WLAN_ESE */

/**
 * struct s_ext_cap - holds bitfields of extended capability IE
 *
 * s_ext_cap holds bitfields of extended capability IE. In dot11f files
 * extended capability IE information is stored as an array of bytes.
 * This structure is used to encode/decode the byte array present in
 * dot11f IE structure.
 */

struct s_ext_cap {
	uint8_t bss_coexist_mgmt_support:1;
	uint8_t reserved1:1;
	uint8_t ext_chan_switch:1;
	uint8_t reserved2:1;
	uint8_t psmp_cap:1;
	uint8_t reserved3:1;
	uint8_t spsmp_cap:1;
	uint8_t event:1;
	uint8_t diagnostics:1;
	uint8_t multi_diagnostics:1;
	uint8_t loc_tracking:1;
	uint8_t fms:1;
	uint8_t proxy_arp_service:1;
	uint8_t co_loc_intf_reporting:1;
	uint8_t civic_loc:1;
	uint8_t geospatial_loc:1;
	uint8_t tfs:1;
	uint8_t wnm_sleep_mode:1;
	uint8_t tim_broadcast:1;
	uint8_t bss_transition:1;
	uint8_t qos_traffic_cap:1;
	uint8_t ac_sta_cnt:1;
	uint8_t multi_bssid:1;
	uint8_t timing_meas:1;
	uint8_t chan_usage:1;
	uint8_t ssid_list:1;
	uint8_t dms:1;
	uint8_t utctsf_offset:1;
	uint8_t tdls_peer_uapsd_buffer_sta:1;
	uint8_t tdls_peer_psm_supp:1;
	uint8_t tdls_channel_switching:1;
	uint8_t interworking_service:1;
	uint8_t qos_map:1;
	uint8_t ebr:1;
	uint8_t sspn_interface:1;
	uint8_t reserved4:1;
	uint8_t msg_cf_cap:1;
	uint8_t tdls_support:1;
	uint8_t tdls_prohibited:1;
	uint8_t tdls_chan_swit_prohibited:1;
	uint8_t reject_unadmitted_traffic:1;
	uint8_t service_interval_granularity:3;
	uint8_t identifier_loc:1;
	uint8_t uapsd_coexistence:1;
	uint8_t wnm_notification:1;
	uint8_t qa_bcapbility:1;
	uint8_t utf8_ssid:1;
	uint8_t qmf_activated:1;
	uint8_t qm_frecon_act:1;
	uint8_t robust_av_streaming:1;
	uint8_t advanced_gcr:1;
	uint8_t mesh_gcr:1;
	uint8_t scs:1;
	uint8_t q_load_report:1;
	uint8_t alternate_edca:1;
	uint8_t unprot_txo_pneg:1;
	uint8_t prot_txo_pneg:1;
	uint8_t reserved6:1;
	uint8_t prot_q_load_report:1;
	uint8_t tdls_wider_bw:1;
	uint8_t oper_mode_notification:1;
	uint8_t max_num_of_msdu_bit1:1;
	uint8_t max_num_of_msdu_bit2:1;
	uint8_t chan_sch_mgmt:1;
	uint8_t geo_db_inband_en_signal:1;
	uint8_t nw_chan_control:1;
	uint8_t white_space_map:1;
	uint8_t chan_avail_query:1;
	uint8_t fine_time_meas_responder:1;
	uint8_t fine_time_meas_initiator:1;
	uint8_t fils_capability:1;
	uint8_t ext_spectrum_management:1;
	uint8_t future_channel_guidance:1;
	uint8_t reserved7:2;
	uint8_t twt_requestor_support:1;
	uint8_t twt_responder_support:1;
	uint8_t reserved8: 1;
	uint8_t reserved9: 4;
	uint8_t beacon_protection_enable: 1;
};

void swap_bit_field16(uint16_t in, uint16_t *out);

/* Currently implemented as "shims" between callers & the new framesc- */
/* generated code: */

QDF_STATUS
sir_convert_probe_req_frame2_struct(struct mac_context *mac,
				uint8_t *frame, uint32_t len,
				tpSirProbeReq probe);

QDF_STATUS
sir_convert_probe_frame2_struct(struct mac_context *mac, uint8_t *frame,
				uint32_t len, tpSirProbeRespBeacon probe);

QDF_STATUS
sir_convert_assoc_req_frame2_struct(struct mac_context *mac,
				    uint8_t *frame, uint32_t len,
				    tpSirAssocReq assoc);
/**
 * wlan_parse_ftie_sha384() - Parse the FT IE if akm uses sha384 KDF
 * @frame: Pointer to the association response frame
 * @frame_len: Length of the assoc response frame
 * @assoc_rsp: Destination assoc response structure in PE to which the FTIE
 * needs to be parsed and copied
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_parse_ftie_sha384(uint8_t *frame, uint32_t frame_len,
		       struct sSirAssocRsp *assoc_rsp);

QDF_STATUS
sir_convert_assoc_resp_frame2_struct(struct mac_context *mac,
				struct pe_session *session_entry,
				uint8_t *frame, uint32_t len,
				tpSirAssocRsp assoc);

QDF_STATUS
sir_convert_reassoc_req_frame2_struct(struct mac_context *mac,
				uint8_t *frame, uint32_t len,
				tpSirAssocReq assoc);

QDF_STATUS
sir_parse_beacon_ie(struct mac_context *mac,
		tpSirProbeRespBeacon pBeaconStruct,
		uint8_t *pPayload, uint32_t payloadLength);

QDF_STATUS
sir_convert_beacon_frame2_struct(struct mac_context *mac,
				uint8_t *pBeaconFrame,
				tpSirProbeRespBeacon pBeaconStruct);

QDF_STATUS
sir_convert_auth_frame2_struct(struct mac_context *mac,
			uint8_t *frame, uint32_t len,
			tpSirMacAuthFrameBody auth);

QDF_STATUS
sir_convert_addts_rsp2_struct(struct mac_context *mac,
			uint8_t *frame, uint32_t len,
			tSirAddtsRspInfo *addts);

QDF_STATUS
sir_convert_delts_req2_struct(struct mac_context *mac,
			uint8_t *frame, uint32_t len,
			struct delts_req_info *delTs);
QDF_STATUS
sir_convert_qos_map_configure_frame2_struct(struct mac_context *mac,
					uint8_t *pFrame, uint32_t nFrame,
					struct qos_map_set *pQosMapSet);

#ifdef ANI_SUPPORT_11H
QDF_STATUS
sir_convert_tpc_req_frame2_struct(struct mac_context *, uint8_t *,
				tpSirMacTpcReqActionFrame, uint32_t);

QDF_STATUS
sir_convert_meas_req_frame2_struct(struct mac_context *, uint8_t *,
				tpSirMacMeasReqActionFrame, uint32_t);
#endif

/**
 * \brief Populated a tDot11fFfCapabilities
 *
 * \param mac Pointer to the global MAC data structure
 *
 * \param pDot11f Address of a tDot11fFfCapabilities to be filled in
 *
 *
 * \note If SIR_MAC_PROP_CAPABILITY_11EQOS is enabled, we'll clear the QOS
 * bit in pDot11f
 *
 *
 */

QDF_STATUS
populate_dot11f_capabilities(struct mac_context *mac,
			tDot11fFfCapabilities *pDot11f,
			struct pe_session *pe_session);

/* / Populate a tDot11fIEChanSwitchAnn */
void
populate_dot11f_chan_switch_ann(struct mac_context *mac,
				tDot11fIEChanSwitchAnn *pDot11f,
				struct pe_session *pe_session);

void
populate_dot_11_f_ext_chann_switch_ann(struct mac_context *mac_ptr,
				tDot11fIEext_chan_switch_ann *dot_11_ptr,
				struct pe_session *session_entry);

void
populate_dot11f_tx_power_env(struct mac_context *mac,
			     tDot11fIEtransmit_power_env *pDot11f,
			     enum phy_ch_width ch_width, uint32_t chan_freq,
			     uint16_t *num_tpe, bool is_ch_switch);

/* / Populate a tDot11fIEChannelSwitchWrapper */
void
populate_dot11f_chan_switch_wrapper(struct mac_context *mac,
				tDot11fIEChannelSwitchWrapper *pDot11f,
				struct pe_session *pe_session);

/* / Populate a tDot11fIECountry */
QDF_STATUS
populate_dot11f_country(struct mac_context *mac,
			tDot11fIECountry *pDot11f, struct pe_session *pe_session);

/* Populated a populate_dot11f_ds_params */
QDF_STATUS
populate_dot11f_ds_params(struct mac_context *mac,
			tDot11fIEDSParams *pDot11f, qdf_freq_t freq);

/* / Populated a tDot11fIEEDCAParamSet */
void
populate_dot11f_edca_param_set(struct mac_context *mac,
			tDot11fIEEDCAParamSet *pDot11f,
			struct pe_session *pe_session);

QDF_STATUS
populate_dot11f_erp_info(struct mac_context *mac,
			tDot11fIEERPInfo *pDot11f, struct pe_session *pe_session);

QDF_STATUS
populate_dot11f_ext_supp_rates(struct mac_context *mac,
			uint8_t nChannelNum, tDot11fIEExtSuppRates *pDot11f,
			struct pe_session *pe_session);

/**
 * populate_dot11f_beacon_report() - Populate the Beacon Report IE
 * @mac: Pointer to the global MAC context
 * @pDot11f: Pointer to the measurement report structure
 * @pBeaconReport: Pointer to the Beacon Report structure
 * @is_last_frame: is the current report last or more reports to follow
 *
 * Return: Ret Status
 */
QDF_STATUS
populate_dot11f_beacon_report(struct mac_context *mac,
			tDot11fIEMeasurementReport *pDot11f,
			tSirMacBeaconReport *pBeaconReport,
			bool is_last_frame);

/**
 * \brief Populate a tDot11fIEExtSuppRates
 *
 *
 * \param mac Pointer to the global MAC data structure
 *
 * \param nChannelNum Channel on which the enclosing frame will be going out
 *
 * \param pDot11f Address of a tDot11fIEExtSuppRates struct to be filled in.
 *
 *
 * This method is a NOP if the channel is greater than 14.
 *
 *
 */

QDF_STATUS
populate_dot11f_ext_supp_rates1(struct mac_context *mac,
				uint8_t nChannelNum,
				tDot11fIEExtSuppRates *pDot11f);

QDF_STATUS
populate_dot11f_ht_caps(struct mac_context *mac,
			struct pe_session *pe_session, tDot11fIEHTCaps *pDot11f);

QDF_STATUS
populate_dot11f_ht_info(struct mac_context *mac,
			tDot11fIEHTInfo *pDot11f, struct pe_session *pe_session);


#ifdef ANI_SUPPORT_11H
QDF_STATUS
populate_dot11f_measurement_report0(struct mac_context *mac,
				tpSirMacMeasReqActionFrame pReq,
				tDot11fIEMeasurementReport *pDot11f);

/* / Populate a tDot11fIEMeasurementReport when the report type is CCA */
QDF_STATUS
populate_dot11f_measurement_report1(struct mac_context *mac,
				tpSirMacMeasReqActionFrame pReq,
				tDot11fIEMeasurementReport *pDot11f);

/* / Populate a tDot11fIEMeasurementReport when the report type is RPI Hist */
QDF_STATUS
populate_dot11f_measurement_report2(struct mac_context *mac,
				tpSirMacMeasReqActionFrame pReq,
				tDot11fIEMeasurementReport *pDot11f);
#endif /* ANI_SUPPORT_11H */

/* / Populate a tDot11fIEPowerCaps */
void
populate_dot11f_power_caps(struct mac_context *mac,
			tDot11fIEPowerCaps *pCaps,
			uint8_t nAssocType, struct pe_session *pe_session);

/* / Populate a tDot11fIEPowerConstraints */
QDF_STATUS
populate_dot11f_power_constraints(struct mac_context *mac,
				tDot11fIEPowerConstraints *pDot11f);

void
populate_dot11f_qos_caps_station(struct mac_context *mac, struct pe_session *session,
				tDot11fIEQOSCapsStation *pDot11f);

QDF_STATUS
populate_dot11f_rsn(struct mac_context *mac,
		tpSirRSNie pRsnIe, tDot11fIERSN *pDot11f);

QDF_STATUS
populate_dot11f_rsn_opaque(struct mac_context *mac,
		tpSirRSNie pRsnIe, tDot11fIERSNOpaque *pDot11f);

#if defined(FEATURE_WLAN_WAPI)

QDF_STATUS
populate_dot11f_wapi(struct mac_context *mac,
		tpSirRSNie pRsnIe, tDot11fIEWAPI *pDot11f);

QDF_STATUS populate_dot11f_wapi_opaque(struct mac_context *mac,
					tpSirRSNie pRsnIe,
					tDot11fIEWAPIOpaque *pDot11f);

#endif /* defined(FEATURE_WLAN_WAPI) */

/* / Populate a tDot11fIESSID given a tSirMacSSid */
void
populate_dot11f_ssid(struct mac_context *mac,
		tSirMacSSid *pInternal, tDot11fIESSID *pDot11f);

/* / Populate a tDot11fIESSID from CFG */
QDF_STATUS populate_dot11f_ssid2(struct mac_context *mac,
				tDot11fIESSID *pDot11f);

/**
 * \brief Populate a tDot11fIESchedule
 *
 * \sa populate_dot11f_wmm_schedule
 *
 *
 * \param pSchedule Address of a tSirMacScheduleIE struct
 *
 * \param pDot11f Address of a tDot11fIESchedule to be filled in
 *
 *
 */

void
populate_dot11f_schedule(tSirMacScheduleIE *pSchedule,
			tDot11fIESchedule *pDot11f);

void
populate_dot11f_supp_channels(struct mac_context *mac,
			tDot11fIESuppChannels *pDot11f,
			uint8_t nAssocType, struct pe_session *pe_session);

/**
 * \brief Populated a tDot11fIESuppRates
 *
 *
 * \param mac Pointer to the global MAC data structure
 *
 * \param nChannelNum Channel the enclosing frame will be going out on; see
 * below
 *
 * \param pDot11f Address of a tDot11fIESuppRates struct to be filled in.
 *
 *
 * If nChannelNum is greater than 13, the supported rates will be
 * WNI_CFG_SUPPORTED_RATES_11B.  If it is less than or equal to 13, the
 * supported rates will be WNI_CFG_SUPPORTED_RATES_11A.  If nChannelNum is
 * set to the sentinel value POPULATE_DOT11F_RATES_OPERATIONAL, the struct
 * will be populated with WNI_CFG_OPERATIONAL_RATE_SET.
 *
 *
 */

#define POPULATE_DOT11F_RATES_OPERATIONAL (0xff)

QDF_STATUS
populate_dot11f_supp_rates(struct mac_context *mac,
			uint8_t nChannelNum,
			tDot11fIESuppRates *pDot11f, struct pe_session *);

QDF_STATUS
populate_dot11f_rates_tdls(struct mac_context *p_mac,
			tDot11fIESuppRates *p_supp_rates,
			tDot11fIEExtSuppRates *p_ext_supp_rates,
			uint8_t curr_oper_channel);

QDF_STATUS populate_dot11f_tpc_report(struct mac_context *mac,
					tDot11fIETPCReport *pDot11f,
					struct pe_session *pe_session);

/* / Populate a tDot11FfTSInfo */
void populate_dot11f_ts_info(struct mac_ts_info *pInfo,
			     tDot11fFfTSInfo *pDot11f);

void populate_dot11f_wmm(struct mac_context *mac,
			tDot11fIEWMMInfoAp *pInfo,
			tDot11fIEWMMParams *pParams,
			tDot11fIEWMMCaps *pCaps, struct pe_session *pe_session);

void populate_dot11f_wmm_caps(tDot11fIEWMMCaps *pCaps);

#if defined(FEATURE_WLAN_ESE)
/* Fill the ESE version IE */
void populate_dot11f_ese_version(tDot11fIEESEVersion *pESEVersion);
/* Fill the Radio Management Capability */
void populate_dot11f_ese_rad_mgmt_cap(tDot11fIEESERadMgmtCap *pESERadMgmtCap);
/* Fill the CCKM IE */
QDF_STATUS populate_dot11f_ese_cckm_opaque(struct mac_context *mac,
					tpSirCCKMie pCCKMie,
					tDot11fIEESECckmOpaque *pDot11f);

void populate_dot11_tsrsie(struct mac_context *mac,
			struct ese_tsrs_ie *pOld,
			tDot11fIEESETrafStrmRateSet *pDot11f,
			uint8_t rate_length);
void populate_dot11f_re_assoc_tspec(struct mac_context *mac,
				tDot11fReAssocRequest *pReassoc,
				struct pe_session *pe_session);
QDF_STATUS
sir_beacon_ie_ese_bcn_report(struct mac_context *mac,
		uint8_t *pPayload, const uint32_t payloadLength,
		uint8_t **outIeBuf, uint32_t *pOutIeLen);

/**
 * ese_populate_wmm_tspec() - Populates TSPEC info for
 * reassoc
 * @source: source structure
 * @dest: destination structure
 *
 * This function copies TSPEC parameters from source
 * structure to destination structure.
 *
 * Return: None
 */
void ese_populate_wmm_tspec(struct mac_tspec_ie *source,
			    ese_wmm_tspec_ie *dest);

#endif

void populate_dot11f_wmm_info_ap(struct mac_context *mac,
				tDot11fIEWMMInfoAp *pInfo,
				struct pe_session *pe_session);

void populate_dot11f_wmm_info_station_per_session(struct mac_context *mac,
					struct pe_session *pe_session,
					tDot11fIEWMMInfoStation *pInfo);

void populate_dot11f_wmm_params(struct mac_context *mac,
				tDot11fIEWMMParams *pParams,
				struct pe_session *pe_session);

QDF_STATUS
populate_dot11f_wpa(struct mac_context *mac,
		tpSirRSNie pRsnIe, tDot11fIEWPA *pDot11f);

QDF_STATUS
populate_dot11f_wpa_opaque(struct mac_context *mac,
			tpSirRSNie pRsnIe, tDot11fIEWPAOpaque *pDot11f);

void populate_dot11f_tspec(struct mac_tspec_ie *pOld, tDot11fIETSPEC *pDot11f);

void populate_dot11f_wmmtspec(struct mac_tspec_ie *pOld,
			      tDot11fIEWMMTSPEC *pDot11f);

#ifdef WLAN_FEATURE_MSCS
void
populate_dot11f_mscs_dec_element(struct mscs_req_info *mscs_req,
				 tDot11fmscs_request_action_frame *dot11f);
#endif

QDF_STATUS
populate_dot11f_tclas(struct mac_context *mac,
		tSirTclasInfo *pOld, tDot11fIETCLAS *pDot11f);

QDF_STATUS
populate_dot11f_wmmtclas(struct mac_context *mac,
			tSirTclasInfo *pOld, tDot11fIEWMMTCLAS *pDot11f);

QDF_STATUS populate_dot11f_wsc(struct mac_context *mac,
			tDot11fIEWscBeacon *pDot11f);

QDF_STATUS populate_dot11f_wsc_registrar_info(struct mac_context *mac,
						tDot11fIEWscBeacon *pDot11f);

QDF_STATUS de_populate_dot11f_wsc_registrar_info(struct mac_context *mac,
						tDot11fIEWscBeacon *pDot11f);

QDF_STATUS populate_dot11f_probe_res_wpsi_es(struct mac_context *mac,
						tDot11fIEWscProbeRes *pDot11f,
						struct pe_session *pe_session);
QDF_STATUS populate_dot11f_beacon_wpsi_es(struct mac_context *mac,
					tDot11fIEWscBeacon *pDot11f,
					struct pe_session *pe_session);

QDF_STATUS populate_dot11f_wsc_in_probe_res(struct mac_context *mac,
					tDot11fIEWscProbeRes *pDot11f);

QDF_STATUS
populate_dot11f_wsc_registrar_info_in_probe_res(struct mac_context *mac,
					tDot11fIEWscProbeRes *pDot11f);

QDF_STATUS
de_populate_dot11f_wsc_registrar_info_in_probe_res(struct mac_context *mac,
						tDot11fIEWscProbeRes *pDot11f);

QDF_STATUS populate_dot11_assoc_res_p2p_ie(struct mac_context *mac,
					tDot11fIEP2PAssocRes *pDot11f,
					tpSirAssocReq pRcvdAssocReq);

QDF_STATUS populate_dot11f_wfatpc(struct mac_context *mac,
				tDot11fIEWFATPC *pDot11f, uint8_t txPower,
				uint8_t linkMargin);

QDF_STATUS populate_dot11f_rrm_ie(struct mac_context *mac,
				tDot11fIERRMEnabledCap *pDot11f,
				struct pe_session *pe_session);

void populate_mdie(struct mac_context *mac, tDot11fIEMobilityDomain *pDot11f,
		   uint8_t mdie[]);

#ifdef WLAN_FEATURE_FILS_SK
/**
 * populate_fils_ft_info() - Populate FTIE into assoc request frame
 * @mac: Global mac context
 * @ft_info: pointer to assoc request frame FT IE buffer
 * @pe_session: pointer to PE session
 *
 * Return: None
 */
void populate_fils_ft_info(struct mac_context *mac, tDot11fIEFTInfo *ft_info,
			   struct pe_session *pe_session);
#else
static inline
void populate_fils_ft_info(struct mac_context *mac, tDot11fIEFTInfo *ft_info,
			   struct pe_session *pe_session)
{}
#endif

void populate_dot11f_assoc_rsp_rates(struct mac_context *mac,
				tDot11fIESuppRates *pSupp,
				tDot11fIEExtSuppRates *pExt,
				uint16_t *_11bRates, uint16_t *_11aRates);

int find_ie_location(struct mac_context *mac, tpSirRSNie pRsnIe, uint8_t EID);

void lim_log_vht_cap(struct mac_context *mac, tDot11fIEVHTCaps *pDot11f);

QDF_STATUS
populate_dot11f_vht_caps(struct mac_context *mac, struct pe_session *pe_session,
			tDot11fIEVHTCaps *pDot11f);

QDF_STATUS
populate_dot11f_vht_operation(struct mac_context *mac,
			struct pe_session *pe_session,
			tDot11fIEVHTOperation *pDot11f);

QDF_STATUS
populate_dot11f_ext_cap(struct mac_context *mac, bool isVHTEnabled,
			tDot11fIEExtCap *pDot11f, struct pe_session *pe_session);

void populate_dot11f_qcn_ie(struct mac_context *mac,
			    struct pe_session *pe_session,
			    tDot11fIEqcn_ie *qcn_ie,
			    uint8_t attr_id);

#ifdef WLAN_FEATURE_FILS_SK
/**
 * populate_dot11f_fils_params() - Populate FILS IE to frame
 * @mac_ctx: global mac context
 * @frm: Assoc request frame
 * @pe_session: PE session
 *
 * This API is used to populate FILS IE to Association request
 *
 * Return: None
 */
void populate_dot11f_fils_params(struct mac_context *mac_ctx,
				 tDot11fAssocRequest * frm,
				 struct pe_session *pe_session);
#else
static inline void populate_dot11f_fils_params(struct mac_context *mac_ctx,
				 tDot11fAssocRequest *frm,
				 struct pe_session *pe_session)
{ }
#endif

QDF_STATUS
populate_dot11f_operating_mode(struct mac_context *mac,
			tDot11fIEOperatingMode *pDot11f,
			struct pe_session *pe_session);

void populate_dot11f_timeout_interval(struct mac_context *mac,
				tDot11fIETimeoutInterval *pDot11f,
				uint8_t type, uint32_t value);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/* Populate a tDot11fIEQComVendorIE */
void
populate_dot11f_avoid_channel_ie(struct mac_context *mac_ctx,
				tDot11fIEQComVendorIE *dot11f,
				struct pe_session *session_entry);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

QDF_STATUS populate_dot11f_timing_advert_frame(struct mac_context *mac,
	tDot11fTimingAdvertisementFrame *frame);
void populate_dot11_supp_operating_classes(struct mac_context *mac_ptr,
	tDot11fIESuppOperatingClasses *dot_11_ptr, struct pe_session *session_entry);

QDF_STATUS
sir_validate_and_rectify_ies(struct mac_context *mac_ctx,
				uint8_t *mgmt_frame,
				uint32_t frame_bytes,
				uint32_t *missing_rsn_bytes);
/**
 * sir_copy_caps_info() - Copy Caps info from tDot11fFfCapabilities to
 *                        beacon/probe response structure.
 * @mac_ctx: MAC Context
 * @caps: tDot11fFfCapabilities structure
 * @pProbeResp: beacon/probe response structure
 *
 * Copy the caps info to beacon/probe response structure
 *
 * Return: None
 */
void sir_copy_caps_info(struct mac_context *mac_ctx, tDot11fFfCapabilities caps,
			tpSirProbeRespBeacon pProbeResp);

#ifdef WLAN_FEATURE_FILS_SK
/**
 * update_fils_data: update fils params from beacon/probe response
 * @fils_ind: pointer to sir_fils_indication
 * @fils_indication: pointer to tDot11fIEfils_indication
 *
 * Return: None
 */
void update_fils_data(struct sir_fils_indication *fils_ind,
				 tDot11fIEfils_indication * fils_indication);
#endif
#ifdef WLAN_FEATURE_11AX
QDF_STATUS populate_dot11f_he_caps(struct mac_context *, struct pe_session *,
				   tDot11fIEhe_cap *);
QDF_STATUS populate_dot11f_he_operation(struct mac_context *, struct pe_session *,
					tDot11fIEhe_op *);
/**
 * populate_dot11f_he_6ghz_cap() - pouldate HE 6GHz caps IE
 * @mac_ctx: Global MAC context
 * @session: PE session
 * @he_6g_cap: pointer to HE 6GHz IE
 *
 * Populdate the HE 6GHz IE based on the session.
 */
QDF_STATUS
populate_dot11f_he_6ghz_cap(struct mac_context *mac_ctx,
			    struct pe_session *session,
			    tDot11fIEhe_6ghz_band_cap *he_6g_cap);
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
QDF_STATUS populate_dot11f_he_bss_color_change(struct mac_context *mac_ctx,
				struct pe_session *session,
				tDot11fIEbss_color_change *bss_color);
#else
static inline QDF_STATUS populate_dot11f_he_bss_color_change(
				struct mac_context *mac_ctx,
				struct pe_session *session,
				tDot11fIEbss_color_change *bss_color)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#else
static inline QDF_STATUS populate_dot11f_he_caps(struct mac_context *mac_ctx,
			struct pe_session *session, tDot11fIEhe_cap *he_cap)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS populate_dot11f_he_operation(struct mac_context *mac_ctx,
			struct pe_session *session, tDot11fIEhe_op *he_op)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
populate_dot11f_he_6ghz_cap(struct mac_context *mac_ctx,
			    struct pe_session *session,
			    tDot11fIEhe_6ghz_band_cap *he_6g_cap)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS populate_dot11f_he_bss_color_change(
				struct mac_context *mac_ctx,
				struct pe_session *session,
				tDot11fIEbss_color_change *bss_color)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if defined(WLAN_FEATURE_11AX) && defined(WLAN_SUPPORT_TWT)
/**
 * populate_dot11f_twt_extended_caps() - populate TWT extended capabilities
 * @mac_ctx: Global MAC context.
 * @pe_session: Pointer to the PE session.
 * @dot11f: Pointer to the extended capabilities of the session.
 *
 * Populate the TWT extended capabilities based on the target and INI support.
 *
 * Return: QDF_STATUS Success or Failure
 */
QDF_STATUS populate_dot11f_twt_extended_caps(struct mac_context *mac_ctx,
					     struct pe_session *pe_session,
					     tDot11fIEExtCap *dot11f);
#else
static inline
QDF_STATUS populate_dot11f_twt_extended_caps(struct mac_context *mac_ctx,
					     struct pe_session *pe_session,
					     tDot11fIEExtCap *dot11f)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * populate_dot11f_btm_extended_caps() - populate btm extended capabilities
 * @mac_ctx: Global MAC context.
 * @pe_session: Pointer to the PE session.
 * @dot11f: Pointer to the extended capabilities of the session.
 *
 * Disable btm for SAE types for Helium firmware limit
 *
 * Return: QDF_STATUS Success or Failure
 */
QDF_STATUS populate_dot11f_btm_extended_caps(struct mac_context *mac_ctx,
					     struct pe_session *pe_session,
					     struct sDot11fIEExtCap *dot11f);

/**
 * lim_truncate_ppet: truncates ppet of trailling zeros
 * @ppet: ppet to truncate
 * max_len: max length of ppet
 *
 * Return: new length after truncation
 */
static inline uint32_t lim_truncate_ppet(uint8_t *ppet, uint32_t max_len)
{
	while (max_len) {
		if (ppet[max_len - 1])
			break;
		max_len--;
	}
	return max_len;
}
#endif /* __PARSE_H__ */
