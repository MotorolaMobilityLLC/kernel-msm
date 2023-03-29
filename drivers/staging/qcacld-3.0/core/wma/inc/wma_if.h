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

#ifndef _HALMSGAPI_H_
#define _HALMSGAPI_H_

#include "qdf_types.h"
#include "sir_api.h"
#include "sir_params.h"

/*
 * Validate the OS Type being built
 */

#if defined(ANI_OS_TYPE_ANDROID)        /* ANDROID */

#if defined(ANI_OS_TYPE_QNX)
#error "more than one ANI_OS_TYPE_xxx is defined for this build"
#endif

#elif defined(ANI_OS_TYPE_QNX)        /* QNX */

#if defined(ANI_OS_TYPE_ANDROID)
#error "more than one ANI_OS_TYPE_xxx is defined for this build"
#endif

#elif !defined(ANI_OS_TYPE_ANDROID) && !defined(ANI_OS_TYPE_QNX) /* NONE */
#error "NONE of the ANI_OS_TYPE_xxx are defined for this build"
#endif

/* operMode in ADD BSS message */
#define BSS_OPERATIONAL_MODE_AP     0
#define BSS_OPERATIONAL_MODE_STA    1
#define BSS_OPERATIONAL_MODE_IBSS   2
#define BSS_OPERATIONAL_MODE_NDI    3

/* STA entry type in add sta message */
#define STA_ENTRY_SELF              0
#define STA_ENTRY_OTHER             1
#define STA_ENTRY_BSSID             2
/* Special station id for transmitting broadcast frames. */
#define STA_ENTRY_BCAST             3
#define STA_ENTRY_PEER              STA_ENTRY_OTHER
#ifdef FEATURE_WLAN_TDLS
#define STA_ENTRY_TDLS_PEER         4
#endif /* FEATURE_WLAN_TDLS */
#define STA_ENTRY_NDI_PEER          5

#define STA_INVALID_IDX 0xFF

/* invalid channel id. */
#define INVALID_CHANNEL_ID 0

/**
 * enum eFrameType - frame types
 * @TXRX_FRM_RAW: raw frame
 * @TXRX_FRM_ETH2: ethernet frame
 * @TXRX_FRM_802_3: 802.3 frame
 * @TXRX_FRM_802_11_MGMT: 802.11 mgmt frame
 * @TXRX_FRM_802_11_CTRL: 802.11 control frame
 * @TXRX_FRM_802_11_DATA: 802.11 data frame
 */
typedef enum {
	TXRX_FRM_RAW,
	TXRX_FRM_ETH2,
	TXRX_FRM_802_3,
	TXRX_FRM_802_11_MGMT,
	TXRX_FRM_802_11_CTRL,
	TXRX_FRM_802_11_DATA,
	TXRX_FRM_IGNORED,   /* This frame will be dropped */
	TXRX_FRM_MAX
} eFrameType;

/**
 * enum eFrameTxDir - frame tx direction
 * @ANI_TXDIR_IBSS: IBSS frame
 * @ANI_TXDIR_TODS: frame to DS
 * @ANI_TXDIR_FROMDS: Frame from DS
 * @ANI_TXDIR_WDS: WDS frame
 */
typedef enum {
	ANI_TXDIR_IBSS = 0,
	ANI_TXDIR_TODS,
	ANI_TXDIR_FROMDS,
	ANI_TXDIR_WDS
} eFrameTxDir;

/**
 *struct sAniBeaconStruct - Beacon structure
 * @beaconLength: beacon length
 * @macHdr: mac header for beacon
 */
typedef struct sAniBeaconStruct {
	uint32_t beaconLength;
	tSirMacMgmtHdr macHdr;
} qdf_packed tAniBeaconStruct, *tpAniBeaconStruct;

/**
 * struct sAniProbeRspStruct - probeRsp template structure
 * @macHdr: mac header for probe response
 */
struct sAniProbeRspStruct {
	tSirMacMgmtHdr macHdr;
	/* probeRsp body follows here */
} qdf_packed;

/**
 * struct tAddStaParams - add sta related parameters
 * @bssId: bssid of sta
 * @assocId: associd
 * @staType: 0 - Self, 1 other/remote, 2 - bssid
 * @staMac: MAC Address of STA
 * @listenInterval: Listen interval
 * @wmmEnabled: Support for 11e/WMM
 * @uAPSD: U-APSD Flags: 1b per AC
 * @maxSPLen: Max SP Length
 * @htCapable: 11n HT capable STA
 * @txChannelWidthSet: TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz
 * @mimoPS: MIMO Power Save
 * @maxAmpduDensity: 3 : 0~7 : 2^(11nAMPDUdensity -4)
 * @maxAmsduSize: 1 : 3839 bytes, 0 : 7935 bytes
 * @fShortGI40Mhz: short GI support for 40Mhz packets
 * @fShortGI20Mhz: short GI support for 20Mhz packets
 * @supportedRates: legacy supported rates
 * @status: QDF status
 * @staIdx: station index
 * @updateSta: pdate the existing STA entry, if this flag is set
 * @rmfEnabled: Robust Management Frame (RMF) enabled/disabled
 * @encryptType: The unicast encryption type in the association
 * @sessionId: PE session id
 * @p2pCapableSta: if this is a P2P Capable Sta
 * @csaOffloadEnable: CSA offload enable flag
 * @vhtCapable: is VHT capabale or not
 * @vhtTxChannelWidthSet: VHT channel width
 * @vhtSupportedRxNss: VHT supported RX NSS
 * @vhtTxBFCapable: txbf capable or not
 * @vhtTxMUBformeeCapable: Bformee capable or not
 * @enableVhtpAid: enable VHT AID
 * @enableAmpduPs: AMPDU power save
 * @enableHtSmps: enable HT SMPS
 * @htSmpsconfig: HT SMPS config
 * @htLdpcCapable: HT LDPC capable
 * @vhtLdpcCapable: VHT LDPC capable
 * @vht_mcs_10_11_supp: VHT MCS 10 & 11 support
 * @smesessionId: sme session id
 * @wpa_rsn: RSN capable
 * @capab_info: capabality info
 * @ht_caps: HT capabalities
 * @vht_caps: VHT vapabalities
 * @nwType: NW Type
 * @maxTxPower: max tx power
 * @nss: Return the number of spatial streams supported
 * @stbc_capable: stbc capable
 * @no_ptk_4_way: Do not need 4-way handshake
 *
 * This structure contains parameter required for
 * add sta request of upper layer.
 */
typedef struct {
	tSirMacAddr bssId;
	uint16_t assocId;
	/* Field to indicate if this is sta entry for itself STA adding entry
	 * for itself or remote (AP adding STA after successful association.
	 * This may or may not be required in production driver.
	 */
	uint8_t staType;
	tSirMacAddr staMac;
	uint16_t listenInterval;
	uint8_t wmmEnabled;
	uint8_t uAPSD;
	uint8_t maxSPLen;
	uint8_t htCapable;
	enum phy_ch_width ch_width;
	tSirMacHTMIMOPowerSaveState mimoPS;
	uint8_t maxAmpduSize;
	uint8_t maxAmpduDensity;
	/* 11n Parameters */
	/* HT STA should set it to 1 if it is enabled in BSS
	 * HT STA should set it to 0 if AP does not support it.
	 * This indication is sent to HAL and HAL uses this flag
	 * to pickup up appropriate 40Mhz rates.
	 */
	uint8_t fShortGI40Mhz;
	uint8_t fShortGI20Mhz;
	struct supported_rates supportedRates;
	/*
	 * Following parameters are for returning status and station index from
	 * HAL to PE via response message. HAL does not read them.
	 */
	/* The return status of SIR_HAL_ADD_STA_REQ is reported here */
	QDF_STATUS status;
	uint8_t updateSta;
	uint8_t rmfEnabled;
	uint32_t encryptType;
	uint8_t sessionId;
	uint8_t p2pCapableSta;
	uint8_t csaOffloadEnable;
	uint8_t vhtCapable;
	uint8_t vhtSupportedRxNss;
	uint8_t vht_160mhz_nss;
	uint8_t vht_80p80mhz_nss;
	uint8_t vht_extended_nss_bw_cap;
	uint8_t vhtTxBFCapable;
	uint8_t enable_su_tx_bformer;
	uint8_t vhtTxMUBformeeCapable;
	uint8_t enableVhtpAid;
	uint8_t enableAmpduPs;
	uint8_t enableHtSmps;
	uint8_t htSmpsconfig;
	bool send_smps_action;
	uint8_t htLdpcCapable;
	uint8_t vhtLdpcCapable;
	uint8_t vht_mcs_10_11_supp;
	uint8_t smesessionId;
	uint8_t wpa_rsn;
	uint16_t capab_info;
	uint16_t ht_caps;
	uint32_t vht_caps;
	tSirNwType nwType;
	int8_t maxTxPower;
	uint8_t nonRoamReassoc;
	uint32_t nss;
#ifdef WLAN_FEATURE_11AX
	bool he_capable;
	tDot11fIEhe_cap he_config;
	tDot11fIEhe_op he_op;
	tDot11fIEhe_6ghz_band_cap he_6ghz_band_caps;
	uint16_t he_mcs_12_13_map;
#endif
	uint8_t stbc_capable;
#ifdef WLAN_SUPPORT_TWT
	uint8_t twt_requestor;
	uint8_t twt_responder;
#endif
	bool no_ptk_4_way;
} tAddStaParams, *tpAddStaParams;

/**
 * struct tDeleteStaParams - parameters required for del sta request
 * @assocId: association index
 * @status: status
 * @respReqd: is response required
 * @sessionId: PE session id
 * @smesessionId: SME session id
 * @staType: station type
 * @staMac: station mac
 */
typedef struct {
	uint16_t assocId;
	QDF_STATUS status;
	uint8_t respReqd;
	uint8_t sessionId;
	uint8_t smesessionId;
	uint8_t staType;
	tSirMacAddr staMac;
} tDeleteStaParams, *tpDeleteStaParams;

/**
 * struct tSetStaKeyParams - set key params
 * @staIdx: station id
 * @encType: encryption type
 * @defWEPIdx: Default WEP key, valid only for static WEP, must between 0 and 3
 * @key: valid only for non-static WEP encyrptions
 * @singleTidRc: 1=Single TID based Replay Count, 0=Per TID based RC
 * @smesessionId: sme session id
 * @peerMacAddr: peer mac address
 * @status: status
 * @sendRsp: send response
 * @macaddr: MAC address of the peer
 *
 * This is used by PE to configure the key information on a given station.
 * When the secType is WEP40 or WEP104, the defWEPIdx is used to locate
 * a preconfigured key from a BSS the station assoicated with; otherwise
 * a new key descriptor is created based on the key field.
 */
typedef struct {
	tAniEdType encType;
	uint8_t defWEPIdx;
	tSirKeys key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
	uint8_t singleTidRc;
	uint8_t vdev_id;
	struct qdf_mac_addr peer_macaddr;
	QDF_STATUS status;
	uint8_t sendRsp;
	struct qdf_mac_addr macaddr;
} tSetStaKeyParams, *tpSetStaKeyParams;

/**
 * struct sLimMlmSetKeysReq - set key request parameters
 * @peerMacAddr: peer mac address
 * @sessionId: PE session id
 * @vdev_id: vdev id
 * @aid: association id
 * @edType: Encryption/Decryption type
 * @numKeys: number of keys
 * @key: key data
 */
typedef struct sLimMlmSetKeysReq {
	struct qdf_mac_addr peer_macaddr;
	uint8_t sessionId;      /* Added For BT-AMP Support */
	uint8_t vdev_id;   /* Added for drivers based on wmi interface */
	uint16_t aid;
	tAniEdType edType;      /* Encryption/Decryption type */
	uint8_t numKeys;
	tSirKeys key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
} tLimMlmSetKeysReq, *tpLimMlmSetKeysReq;

/**
 * struct bss_params - parameters required for add bss params
 * @bssId: MAC Address/BSSID
 * @nwType: network type
 * @shortSlotTimeSupported: is short slot time supported or not
 * @llbCoexist: 11b coexist supported or not
 * @beaconInterval: beacon interval
 * @dtimPeriod: DTIM period
 * @htCapable: Enable/Disable HT capabilities
 * @rmfEnabled: RMF enabled/disabled
 * @staContext: sta context
 * @updateBss: update the existing BSS entry, if this flag is set
 * @maxTxPower: max power to be used after applying the power constraint
 * @bSpectrumMgtEnabled: Spectrum Management Capability, 1:Enabled, 0:Disabled.
 * @vhtCapable: VHT capablity
 * @ch_width: VHT tx channel width
 * @he_capable: HE Capability
 * @no_ptk_4_way: Do not need 4-way handshake
 */
struct bss_params {
	tSirMacAddr bssId;
	tSirNwType nwType;
	uint8_t shortSlotTimeSupported;
	uint8_t llbCoexist;
	tSirMacBeaconInterval beaconInterval;
	uint8_t dtimPeriod;
	uint8_t htCapable;
	uint8_t rmfEnabled;
	tAddStaParams staContext;
	/* HAL should update the existing BSS entry, if this flag is set.
	 * PE will set this flag in case of reassoc, where we want to resue the
	 * the old bssID and still return success.
	 */
	uint8_t updateBss;
	int8_t maxTxPower;
	uint8_t vhtCapable;
	enum phy_ch_width ch_width;
	uint8_t nonRoamReassoc;
#ifdef WLAN_FEATURE_11AX
	bool he_capable;
	uint32_t he_sta_obsspd;
#endif
	bool no_ptk_4_way;
};

/**
 * struct add_bss_rsp - params required for add bss response
 * @vdev_id: vdev_id
 * @status: QDF status
 * @chain_mask: chain mask vdev start resp
 * @smps_mode: smps mode in vdev start resp
 */
struct add_bss_rsp {
	uint8_t vdev_id;
	QDF_STATUS status;
	uint32_t chain_mask;
	uint8_t smps_mode;
};

/**
 * struct del_bss_resp - params required for del bss response
 * @status: QDF status
 * @vdev_id: vdev_id
 */
struct del_bss_resp {
	QDF_STATUS status;
	uint8_t vdev_id;
};

typedef enum eDelStaReasonCode {
	HAL_DEL_STA_REASON_CODE_KEEP_ALIVE = 0x1,
	HAL_DEL_STA_REASON_CODE_TIM_BASED = 0x2,
	HAL_DEL_STA_REASON_CODE_RA_BASED = 0x3,
	HAL_DEL_STA_REASON_CODE_UNKNOWN_A2 = 0x4,
	HAL_DEL_STA_REASON_CODE_BTM_DISASSOC_IMMINENT = 0x5,
	HAL_DEL_STA_REASON_CODE_SA_QUERY_TIMEOUT = 0x6,
	HAL_DEL_STA_REASON_CODE_XRETRY = 0x7,
} tDelStaReasonCode;

typedef enum eSmpsModeValue {
	STATIC_SMPS_MODE = 0x0,
	DYNAMIC_SMPS_MODE = 0x1,
	SMPS_MODE_RESERVED = 0x2,
	SMPS_MODE_DISABLED = 0x3
} tSmpsModeValue;

/**
 * struct tDeleteStaContext - params required for delete sta request
 * @assocId: association id
 * @bssId: mac address
 * @addr2: mac address
 * @reasonCode: reason code
 * @rssi: rssi value during disconnection
 */
typedef struct {
	bool is_tdls;
	uint8_t vdev_id;
	uint16_t assocId;
	tSirMacAddr bssId;
	tSirMacAddr addr2;
	uint16_t reasonCode;
	int8_t rssi;
} tDeleteStaContext, *tpDeleteStaContext;

#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1724
#endif

/**
 * struct tStartOemDataRsp - start OEM Data response
 * @target_rsp: Indicates if the rsp is from Target or WMA generated.
 * @rsp_len: oem data response length
 * @oem_data_rsp: pointer to OEM Data response
 */
typedef struct {
	bool target_rsp;
	uint32_t rsp_len;
	uint8_t *oem_data_rsp;
} tStartOemDataRsp, *tpStartOemDataRsp;
#endif /* FEATURE_OEM_DATA_SUPPORT */

/**
 * struct beacon_gen_params - params required for beacon gen request
 * @bssdd: BSSID for which it is time to generate a beacon
 */
struct beacon_gen_params {
	tSirMacAddr bssid;
};

/**
 * struct tSendbeaconParams - send beacon parameters
 * vdev_id: vdev id
 * @bssId: BSSID mac address
 * @beacon: beacon data
 * @beaconLength: beacon length of template
 * @timIeOffset: TIM IE offset
 * @p2pIeOffset: P2P IE offset
 * @csa_count_offset: Offset of Switch count field in CSA IE
 * @ecsa_count_offset: Offset of Switch count field in ECSA IE
 * @reason: bcn update reason
 * @status: beacon send status
 */
typedef struct {
	uint8_t vdev_id;
	tSirMacAddr bssId;
	uint8_t beacon[SIR_MAX_BEACON_SIZE];
	uint32_t beaconLength;
	uint32_t timIeOffset;
	uint16_t p2pIeOffset;
	uint32_t csa_count_offset;
	uint32_t ecsa_count_offset;
	enum sir_bcn_update_reason reason;
	QDF_STATUS status;
} tSendbeaconParams, *tpSendbeaconParams;

/**
 * struct tSendProbeRespParams - send probe response parameters
 * @bssId: BSSID
 * @probeRespTemplate: probe response template
 * @probeRespTemplateLen: probe response template length
 * @ucProxyProbeReqValidIEBmap: valid IE bitmap
 */
typedef struct sSendProbeRespParams {
	tSirMacAddr bssId;
	uint8_t probeRespTemplate[SIR_MAX_PROBE_RESP_SIZE];
	uint32_t probeRespTemplateLen;
	uint32_t ucProxyProbeReqValidIEBmap[8];
} tSendProbeRespParams, *tpSendProbeRespParams;

/**
 * struct tSetBssKeyParams - BSS key parameters
 * @encType: encryption Type
 * @numKeys: number of keys
 * @key: key data
 * @singleTidRc: 1=Single TID based Replay Count, 0=Per TID based RC
 * @vdev_id: vdev id id
 * @status: return status of command
 * @macaddr: MAC address of the peer
 */
typedef struct {
	tAniEdType encType;
	uint8_t numKeys;
	tSirKeys key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
	uint8_t singleTidRc;
	uint8_t vdev_id;
	QDF_STATUS status;
	struct qdf_mac_addr macaddr;
} tSetBssKeyParams, *tpSetBssKeyParams;

/**
 * struct tUpdateBeaconParams - update beacon request parameters
 * @bss_idx: BSSID index
 * @fShortPreamble: shortPreamble mode
 * @fShortSlotTime: short Slot time
 * @beaconInterval: Beacon Interval
 * @llaCoexist: 11a coexist
 * @llbCoexist: 11b coexist
 * @llgCoexist: 11g coexist
 * @ht20MhzCoexist: HT 20MHz coexist
 * @fLsigTXOPProtectionFullSupport: TXOP protection supported or not
 * @fRIFSMode: RIFS mode
 * @paramChangeBitmap: change bitmap
 * @vdev_id: vdev_id
 */
typedef struct {
	uint8_t bss_idx;
	uint8_t fShortPreamble;
	uint8_t fShortSlotTime;
	uint16_t beaconInterval;
	uint8_t llaCoexist;
	uint8_t llbCoexist;
	uint8_t llgCoexist;
	uint8_t ht20MhzCoexist;
	uint8_t llnNonGFCoexist;
	uint8_t fLsigTXOPProtectionFullSupport;
	uint8_t fRIFSMode;
	uint16_t paramChangeBitmap;
	uint8_t vdev_id;
	uint32_t bss_color;
	bool bss_color_disabled;
} tUpdateBeaconParams, *tpUpdateBeaconParams;

/**
 * struct tUpdateVHTOpMode - VHT operating mode
 * @opMode: VHT operating mode
 * @staId: station id
 * @smesessionId: SME session id
 * @peer_mac: peer mac address
 */
typedef struct {
	uint16_t opMode;
	uint16_t smesessionId;
	tSirMacAddr peer_mac;
} tUpdateVHTOpMode, *tpUpdateVHTOpMode;

/**
 * struct tUpdateRxNss - update rx nss parameters
 * @rxNss: rx nss value
 * @staId: station id
 * @smesessionId: sme session id
 * @peer_mac: peer mac address
 */
typedef struct {
	uint16_t rxNss;
	uint16_t smesessionId;
	tSirMacAddr peer_mac;
} tUpdateRxNss, *tpUpdateRxNss;

/**
 * struct tUpdateMembership - update membership parmaters
 * @membership: membership value
 * @staId: station id
 * @smesessionId: SME session id
 * @peer_mac: peer mac address
 */
typedef struct {
	uint32_t membership;
	uint16_t smesessionId;
	tSirMacAddr peer_mac;
} tUpdateMembership, *tpUpdateMembership;

/**
 * struct tUpdateUserPos - update user position parmeters
 * @userPos: user position
 * @staId: station id
 * @smesessionId: sme session id
 * @peer_mac: peer mac address
 */
typedef struct {
	uint32_t userPos;
	uint16_t smesessionId;
	tSirMacAddr peer_mac;
} tUpdateUserPos, *tpUpdateUserPos;

/**
 * struct tEdcaParams - EDCA parameters
 * @vdev_id: vdev id
 * @acbe: best effort access category
 * @acbk: Background access category
 * @acvi: video access category
 * @acvo: voice access category
 * @mu_edca_params: flag to indicate MU EDCA
 */
typedef struct {
	uint16_t vdev_id;
	tSirMacEdcaParamRecord acbe;
	tSirMacEdcaParamRecord acbk;
	tSirMacEdcaParamRecord acvi;
	tSirMacEdcaParamRecord acvo;
	bool mu_edca_params;
} tEdcaParams, *tpEdcaParams;

/**
 * struct tSetMIMOPS - MIMO power save related parameters
 * @htMIMOPSState: MIMO Power Save State
 * @status: response status
 * @fsendRsp: send response flag
 * @peerMac: peer mac address
 * @sessionId: session id
 */
typedef struct sSet_MIMOPS {
	tSirMacHTMIMOPowerSaveState htMIMOPSState;
	QDF_STATUS status;
	uint8_t fsendRsp;
	tSirMacAddr peerMac;
	uint8_t sessionId;
} tSetMIMOPS, *tpSetMIMOPS;

/**
 * struct tMaxTxPowerParams - Max Tx Power parameters
 * @bssId: BSSID is needed to identify which session issued this request
 * @selfStaMacAddr: self mac address
 * @power: tx power in dbm
 * @dev_mode: device mode
 * Request Type = SIR_HAL_SET_MAX_TX_POWER_REQ
 */
typedef struct sMaxTxPowerParams {
	struct qdf_mac_addr bssId;
	struct qdf_mac_addr selfStaMacAddr;
	/* In request,
	 * power == MaxTx power to be used.
	 * In response,
	 * power == tx power used for management frames.
	 */
	int8_t power;
	enum QDF_OPMODE dev_mode;
} tMaxTxPowerParams, *tpMaxTxPowerParams;

/**
 * struct tMaxTxPowerPerBandParams - max tx power per band info
 * @bandInfo: band info
 * @power: power in dbm
 */
typedef struct sMaxTxPowerPerBandParams {
	enum band_info bandInfo;
	int8_t power;
} tMaxTxPowerPerBandParams, *tpMaxTxPowerPerBandParams;

/**
 * struct vdev_create_req_param - vdev create request params
 * @vdev_id: vdev_id
 * @status: response status code
 */
struct vdev_create_req_param {
	uint32_t vdev_id;
	QDF_STATUS status;
};

/**
 * struct set_ie_param - set IE params structure
 * @pdev_id: pdev id
 * @ie_type: IE type
 * @nss: Nss value
 * @ie_len: IE length
 * @ie_ptr: Pointer to IE data
 *
 * Holds the set pdev IE req data.
 */
struct set_ie_param {
	uint8_t pdev_id;
	uint8_t ie_type;
	uint8_t nss;
	uint8_t ie_len;
	uint8_t *ie_ptr;
};

/**
 * struct set_dtim_params - dtim params
 * @session_id: SME Session ID
 * @dtim_period: dtim period
 */
struct set_dtim_params {
	uint8_t session_id;
	uint8_t dtim_period;
};

#define DOT11_HT_IE     1
#define DOT11_VHT_IE    2

/**
 * struct del_vdev_params - Del Sta Self params
 * @session_id: SME Session ID
 * @status: response status code
 * @vdev: Object to vdev
 * @sme_ctx: pointer to context provided by SME
 */
struct del_vdev_params {
	tSirMacAddr self_mac_addr;
	uint8_t vdev_id;
	uint32_t status;
	struct wlan_objmgr_vdev *vdev;
	void *sme_ctx;
};

/**
 * struct del_sta_self_rsp_params - Del Sta Self response params
 * @self_sta_param: sta params
 * @generate_rsp: generate response to upper layers
 */
struct del_sta_self_rsp_params {
	struct del_vdev_params *self_sta_param;
};

/**
 * struct send_peer_unmap_conf_params - Send Peer Unmap Conf param
 * @vdev_id: vdev ID
 * @peer_id_cnt: peer_id count
 * @peer_id_list: list of peer IDs
 */
struct send_peer_unmap_conf_params {
	uint8_t vdev_id;
	uint32_t peer_id_cnt;
	uint16_t *peer_id_list;
};

/**
 * struct peer_create_rsp_params  - Peer create response parameters
 * @peer_mac: Peer mac address
 */
struct peer_create_rsp_params {
	struct qdf_mac_addr peer_mac;
};

/**
 * struct tDisableIntraBssFwd - intra bss forward parameters
 * @sessionId: session id
 * @disableintrabssfwd: disable intra bss forward flag
 */
typedef struct sDisableIntraBssFwd {
	uint16_t sessionId;
	bool disableintrabssfwd;
} qdf_packed tDisableIntraBssFwd, *tpDisableIntraBssFwd;

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * struct tStatsExtRequest - ext stats request
 * @vdev_id: vdev id
 * @request_data_len: request data length
 * @request_data: request data
 */
typedef struct sStatsExtRequest {
	uint32_t vdev_id;
	uint32_t request_data_len;
	uint8_t request_data[];
} tStatsExtRequest, *tpStatsExtRequest;
#endif /* WLAN_FEATURE_STATS_EXT */

/*
 * struct roam_blacklist_timeout - BTM blacklist entry
 * @bssid - bssid that is to be blacklisted
 * @timeout - time duration for which the bssid is blacklisted
 * @received_time - boot timestamp at which the firmware event was received
 * @rssi - rssi value for which the bssid is blacklisted
 * @reject_reason: reason to add the BSSID to BLM
 * @original_timeout: original timeout sent by the AP
 * @source: Source of adding the BSSID to BLM
 */
struct roam_blacklist_timeout {
	struct qdf_mac_addr bssid;
	uint32_t timeout;
	qdf_time_t received_time;
	int32_t rssi;
	enum blm_reject_ap_reason reject_reason;
	uint32_t original_timeout;
	enum blm_reject_ap_source source;
};

/*
 * struct roam_blacklist_event - Blacklist event entries destination structure
 * @num_entries: total entries sent over the event
 * @roam_blacklist: blacklist details
 */
struct roam_blacklist_event {
	uint32_t num_entries;
	struct roam_blacklist_timeout roam_blacklist[];
};

/*
 * struct roam_pmkid_req_event - Pmkid event with entries destination structure
 * @num_entries: total entries sent over the event
 * @ap_bssid: bssid list
 */
struct roam_pmkid_req_event {
	uint32_t num_entries;
	struct qdf_mac_addr ap_bssid[];
};

#endif /* _HALMSGAPI_H_ */
