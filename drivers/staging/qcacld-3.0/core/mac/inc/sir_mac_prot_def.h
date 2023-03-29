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
 * This file sir_mac_prot_def.h contains the MAC/PHY protocol
 * definitions used across various projects.
 */

#ifndef __MAC_PROT_DEFS_H
#define __MAC_PROT_DEFS_H

#include <linux/if_ether.h>

#include "cds_api.h"
#include "sir_types.h"
#include "wni_cfg.h"
#include <lim_fils_defs.h>

/* /Capability information related */
#define CAPABILITY_INFO_DELAYED_BA_BIT 14
#define CAPABILITY_INFO_IMMEDIATE_BA_BIT 15

/* / 11h MAC defaults */
#define SIR_11A_CHANNEL_BEGIN           34
#define SIR_11A_CHANNEL_END             165
#define SIR_11B_CHANNEL_BEGIN           1
#define SIR_11B_CHANNEL_END             14
#define SIR_11A_FREQUENCY_OFFSET        4
#define SIR_11B_FREQUENCY_OFFSET        1
#define SIR_11P_CHANNEL_BEGIN           170
#define SIR_11P_CHANNEL_END             184

/* / Current version of 802.11 */
#define SIR_MAC_PROTOCOL_VERSION 0

/* Frame Type definitions */

#define SIR_MAC_MGMT_FRAME    0x0
#define SIR_MAC_CTRL_FRAME    0x1
#define SIR_MAC_DATA_FRAME    0x2

/* Data frame subtype definitions */
#define SIR_MAC_DATA_DATA                 0
#define SIR_MAC_DATA_DATA_ACK             1
#define SIR_MAC_DATA_DATA_POLL            2
#define SIR_MAC_DATA_DATA_ACK_POLL        3
#define SIR_MAC_DATA_NULL                 4
#define SIR_MAC_DATA_NULL_ACK             5
#define SIR_MAC_DATA_NULL_POLL            6
#define SIR_MAC_DATA_NULL_ACK_POLL        7
#define SIR_MAC_DATA_QOS_DATA             8
#define SIR_MAC_DATA_QOS_DATA_ACK         9
#define SIR_MAC_DATA_QOS_DATA_POLL        10
#define SIR_MAC_DATA_QOS_DATA_ACK_POLL    11
#define SIR_MAC_DATA_QOS_NULL             12
#define SIR_MAC_DATA_QOS_NULL_ACK         13
#define SIR_MAC_DATA_QOS_NULL_POLL        14
#define SIR_MAC_DATA_QOS_NULL_ACK_POLL    15

#define SIR_MAC_DATA_QOS_MASK             8
#define SIR_MAC_DATA_NULL_MASK            4
#define SIR_MAC_DATA_POLL_MASK            2
#define SIR_MAC_DATA_ACK_MASK             1

/* Management frame subtype definitions */

#define SIR_MAC_MGMT_ASSOC_REQ    0x0
#define SIR_MAC_MGMT_ASSOC_RSP    0x1
#define SIR_MAC_MGMT_REASSOC_REQ  0x2
#define SIR_MAC_MGMT_REASSOC_RSP  0x3
#define SIR_MAC_MGMT_PROBE_REQ    0x4
#define SIR_MAC_MGMT_PROBE_RSP    0x5
#define SIR_MAC_MGMT_TIME_ADVERT  0x6
#define SIR_MAC_MGMT_BEACON       0x8
#define SIR_MAC_MGMT_ATIM         0x9
#define SIR_MAC_MGMT_DISASSOC     0xA
#define SIR_MAC_MGMT_AUTH         0xB
#define SIR_MAC_MGMT_DEAUTH       0xC
#define SIR_MAC_MGMT_ACTION       0xD
#define SIR_MAC_MGMT_RESERVED15   0xF

#define SIR_MAC_ACTION_TX             1
#define SIR_MAC_ACTION_RX             2

#define SIR_MAC_BA_POLICY_IMMEDIATE     1
#define SIR_MAC_BA_DEFAULT_BUFF_SIZE    64

#define MAX_BA_BUFF_SIZE    256

#ifdef ANI_SUPPORT_11H
#define SIR_MAC_BASIC_MEASUREMENT_TYPE         0
#define SIR_MAC_CCA_MEASUREMENT_TYPE           1
#define SIR_MAC_RPI_MEASUREMENT_TYPE           2
#endif /* ANI_SUPPORT_11H */

/* RRM related. */
/* Refer IEEE Std 802.11k-2008, Section 7.3.2.21, table 7.29 */

#define SIR_MAC_RRM_CHANNEL_LOAD_TYPE          3
#define SIR_MAC_RRM_NOISE_HISTOGRAM_BEACON     4
#define SIR_MAC_RRM_BEACON_TYPE                5
#define SIR_MAC_RRM_FRAME_TYPE                 6
#define SIR_MAC_RRM_STA_STATISTICS_TYPE        7
#define SIR_MAC_RRM_LCI_TYPE                   8
#define SIR_MAC_RRM_TSM_TYPE                   9
#define SIR_MAC_RRM_LOCATION_CIVIC_TYPE        11
#define SIR_MAC_RRM_FINE_TIME_MEAS_TYPE        16

/* VHT Action Field */
#define SIR_MAC_VHT_GID_NOTIFICATION           1
#define SIR_MAC_VHT_OPMODE_NOTIFICATION        2

#define SIR_MAC_VHT_OPMODE_SIZE                3

#define NUM_OF_SOUNDING_DIMENSIONS	1 /*Nss - 1, (Nss = 2 for 2x2)*/
/* HT Action Field Codes */
#define SIR_MAC_SM_POWER_SAVE       1

/* block acknowledgment action frame types */
#define SIR_MAC_ACTION_VENDOR_SPECIFIC 9
#define SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY     0x7F
#define SIR_MAC_ACTION_P2P_SUBTYPE_PRESENCE_RSP     2

/* Public Action for 20/40 BSS Coexistence */
#define SIR_MAC_ACTION_2040_BSS_COEXISTENCE     0
#define SIR_MAC_ACTION_EXT_CHANNEL_SWITCH_ID    4

/* Public Action frames for GAS */
#define SIR_MAC_ACTION_GAS_INITIAL_REQUEST      0x0A
#define SIR_MAC_ACTION_GAS_INITIAL_RESPONSE     0x0B
#define SIR_MAC_ACTION_GAS_COMEBACK_REQUEST     0x0C
#define SIR_MAC_ACTION_GAS_COMEBACK_RESPONSE    0x0D

/* Protected Dual of Public Action(PDPA) frames Action field */
#define SIR_MAC_PDPA_GAS_INIT_REQ      10
#define SIR_MAC_PDPA_GAS_INIT_RSP      11
#define SIR_MAC_PDPA_GAS_COMEBACK_REQ  12
#define SIR_MAC_PDPA_GAS_COMEBACK_RSP  13

/* ----------------------------------------------------------------------------- */
/* EID (Element ID) definitions */
/* and their min/max lengths */
/* ----------------------------------------------------------------------------- */

#define SIR_MAC_TIM_EID_MIN                3

#define SIR_MAC_WPA_EID                221

#define SIR_MAC_MAX_SUPPORTED_MCS_SET    16

#define VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1       390
#define VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1       390
#define VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2       780
#define VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2       780

#define VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80 433
#define VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1_SGI80 433
#define VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80 866
#define VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_2_2_SGI80 866

#define VHT_CAP_NO_160M_SUPP 0
#define VHT_CAP_160_SUPP 1
#define VHT_CAP_160_AND_80P80_SUPP 2

#define VHT_NO_EXTD_NSS_BW_SUPP			0
#define VHT_EXTD_NSS_80_HALF_NSS_160		1
#define VHT_EXTD_NSS_80_HALF_NSS_80P80		2
#define VHT_EXTD_NSS_80_3QUART_NSS_80P80	3
#define VHT_EXTD_NSS_160_HALF_NSS_80P80		1
#define VHT_EXTD_NSS_160_3QUART_NSS_80P80	2
#define VHT_EXTD_NSS_2X_NSS_160_1X_NSS_80P80	3
#define VHT_EXTD_NSS_2X_NSS_80_1X_NSS_80P80	3

#define VHT_MAX_NSS 8

#define VHT_MCS_1x1 0xFFFC
#define VHT_MCS_2x2 0xFFF3

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
#define SIR_MAC_QCOM_VENDOR_EID      200
#define SIR_MAC_QCOM_VENDOR_OUI      "\x00\xA0\xC6"
#define SIR_MAC_QCOM_VENDOR_SIZE     3
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

#define SIR_MAC_MAX_ADD_IE_LENGTH       2048

/* / Minimum length of each IE */
#define SIR_MAC_RSN_IE_MIN_LENGTH   2
#define SIR_MAC_WPA_IE_MIN_LENGTH   6

#ifdef FEATURE_WLAN_ESE
#define ESE_VERSION_4               4
#define ESE_VERSION_SUPPORTED       ESE_VERSION_4

/* When station sends Radio Management Cap. */
/* State should be normal=1 */
/* Mbssid Mask should be 0 */
#define RM_STATE_NORMAL             1
#endif

#define SIR_MAC_OUI_VERSION_1         1

/* OWE DH Parameter element https://tools.ietf.org/html/rfc8110 */
#define SIR_DH_PARAMETER_ELEMENT_EXT_EID 32

#define SIR_MSCS_ELEMENT_EXT_EID 88

/* OUI and type definition for WPA IE in network byte order */
#define SIR_MAC_WPA_OUI             0x01F25000
#define SIR_MAC_WSC_OUI             "\x00\x50\xf2\x04"
#define SIR_MAC_WSC_OUI_SIZE        4
#define SIR_MAC_P2P_OUI             "\x50\x6f\x9a\x09"
#define SIR_MAC_P2P_OUI_SIZE        4
#define SIR_P2P_NOA_ATTR            12
#define SIR_MAX_NOA_ATTR_LEN        31
#define SIR_P2P_IE_HEADER_LEN       6

#define SIR_MAC_CISCO_OUI "\x00\x40\x96"
#define SIR_MAC_CISCO_OUI_SIZE 3

#define SIR_MAC_QCN_OUI_TYPE   "\x8c\xfd\xf0\x01"
#define SIR_MAC_QCN_OUI_TYPE_SIZE  4

/* MBO OUI definitions */
#define SIR_MAC_MBO_OUI "\x50\x6f\x9a\x16"
#define SIR_MAC_MBO_OUI_SIZE 4
#define SIR_MBO_ELEM_OFFSET  (2 + SIR_MAC_MBO_OUI_SIZE)

/* min size of wme oui header: oui(3) + type + subtype + version */
#define SIR_MAC_OUI_WME_HDR_MIN       6

/* ----------------------------------------------------------------------------- */

/* OFFSET definitions for fixed fields in Management frames */

/* Beacon/Probe Response offsets */
#define SIR_MAC_B_PR_CAPAB_OFFSET            10
#define SIR_MAC_B_PR_SSID_OFFSET             12

/* Association/Reassociation offsets */
#define SIR_MAC_REASSOC_REQ_SSID_OFFSET      10
#define SIR_MAC_ASSOC_RSP_STATUS_CODE_OFFSET 2

/* Association Request offsets */
#define SIR_MAC_ASSOC_REQ_SSID_OFFSET        4

/* / Transaction sequence number definitions (used in Authentication frames) */
#define    SIR_MAC_AUTH_FRAME_1        1
#define    SIR_MAC_AUTH_FRAME_2        2
#define    SIR_MAC_AUTH_FRAME_3        3
#define    SIR_MAC_AUTH_FRAME_4        4

/* / Protocol defined MAX definitions */
#define SIR_MAC_MAX_NUMBER_OF_RATES          12
#define SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS      4
#define SIR_MAC_KEY_LENGTH                   13 /* WEP Maximum key length size */
#define SIR_MAC_AUTH_CHALLENGE_LENGTH        253
#define SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH    128
#define SIR_MAC_WEP_IV_LENGTH                4
#define SIR_MAC_WEP_ICV_LENGTH               4
#define SIR_MAC_CHALLENGE_ID_LEN             2

/* 2 bytes each for auth algo number, transaction number and status code */
#define SIR_MAC_AUTH_FRAME_INFO_LEN          6
/* 2 bytes for ID and length + SIR_MAC_AUTH_CHALLENGE_LENGTH */
#define SIR_MAC_AUTH_CHALLENGE_BODY_LEN    (SIR_MAC_CHALLENGE_ID_LEN + \
					    SIR_MAC_AUTH_CHALLENGE_LENGTH)

/* / MAX key length when ULA is used */
#define SIR_MAC_MAX_KEY_LENGTH               32

/* / Macro definitions for get/set on FC fields */
#define SIR_MAC_GET_PROT_VERSION(x)      ((((uint16_t) x) & 0x0300) >> 8)
#define SIR_MAC_GET_FRAME_TYPE(x)        ((((uint16_t) x) & 0x0C00) >> 8)
#define SIR_MAC_GET_FRAME_SUB_TYPE(x)    ((((uint16_t) x) & 0xF000) >> 12)
#define SIR_MAC_GET_WEP_BIT_IN_FC(x)     (((uint16_t) x) & 0x0040)
#define SIR_MAC_SET_PROT_VERSION(x)      ((uint16_t) x)
#define SIR_MAC_SET_FRAME_TYPE(x)        (((uint16_t) x) << 2)
#define SIR_MAC_SET_FRAME_SUB_TYPE(x)    (((uint16_t) x) << 4)
#define SIR_MAC_SET_WEP_BIT_IN_FC(x)     (((uint16_t) x) << 14)

/* / Macro definitions for get/set on capabilityInfo bits */
#define SIR_MAC_GET_ESS(x)               (((uint16_t) x) & 0x0001)
#define SIR_MAC_GET_IBSS(x)              ((((uint16_t) x) & 0x0002) >> 1)
#define SIR_MAC_GET_CF_POLLABLE(x)       ((((uint16_t) x) & 0x0004) >> 2)
#define SIR_MAC_GET_CF_POLL_REQ(x)       ((((uint16_t) x) & 0x0008) >> 3)
#define SIR_MAC_GET_PRIVACY(x)           ((((uint16_t) x) & 0x0010) >> 4)
#define SIR_MAC_GET_SHORT_PREAMBLE(x)    ((((uint16_t) x) & 0x0020) >> 5)
#define SIR_MAC_GET_SPECTRUM_MGMT(x)     ((((uint16_t) x) & 0x0100) >> 8)
#define SIR_MAC_GET_QOS(x)               ((((uint16_t) x) & 0x0200) >> 9)
#define SIR_MAC_GET_SHORT_SLOT_TIME(x)   ((((uint16_t) x) & 0x0400) >> 10)
#define SIR_MAC_GET_APSD(x)              ((((uint16_t) x) & 0x0800) >> 11)
#define SIR_MAC_GET_RRM(x)               ((((uint16_t) x) & 0x1000) >> 12)
#define SIR_MAC_GET_BLOCK_ACK(x)         ((((uint16_t) x) & 0xc000) >> CAPABILITY_INFO_DELAYED_BA_BIT)
#define SIR_MAC_SET_ESS(x)               (((uint16_t) x) | 0x0001)
#define SIR_MAC_SET_IBSS(x)              (((uint16_t) x) | 0x0002)
#define SIR_MAC_SET_CF_POLLABLE(x)       (((uint16_t) x) | 0x0004)
#define SIR_MAC_SET_CF_POLL_REQ(x)       (((uint16_t) x) | 0x0008)
#define SIR_MAC_SET_PRIVACY(x)           (((uint16_t) x) | 0x0010)
#define SIR_MAC_SET_SHORT_PREAMBLE(x)    (((uint16_t) x) | 0x0020)
#define SIR_MAC_SET_SPECTRUM_MGMT(x)     (((uint16_t) x) | 0x0100)
#define SIR_MAC_SET_QOS(x)               (((uint16_t) x) | 0x0200)
#define SIR_MAC_SET_SHORT_SLOT_TIME(x)   (((uint16_t) x) | 0x0400)
#define SIR_MAC_SET_APSD(x)              (((uint16_t) x) | 0x0800)
#define SIR_MAC_SET_RRM(x)               (((uint16_t) x) | 0x1000)
#define SIR_MAC_SET_GROUP_ACK(x)         (((uint16_t) x) | 0x4000)

#define SIR_MAC_GET_VHT_MAX_AMPDU_EXPO(x) ((((uint32_t) x) & 0x03800000) >> 23)

/* bitname must be one of the above, eg ESS, CF_POLLABLE, etc. */
#define SIR_MAC_CLEAR_CAPABILITY(u16value, bitname) \
	((u16value) &= (~(SIR_MAC_SET_ ## bitname(0))))

#define IS_WES_MODE_ENABLED(x) \
	((x)->mlme_cfg->lfr.wes_mode_enabled)

#define SIR_MAC_VENDOR_AP_1_OUI             "\x00\x0C\x43"
#define SIR_MAC_VENDOR_AP_1_OUI_LEN         3

#define SIR_MAC_VENDOR_AP_3_OUI             "\x00\x03\x7F"
#define SIR_MAC_VENDOR_AP_3_OUI_LEN         3

#define SIR_MAC_VENDOR_AP_4_OUI             "\x8C\xFD\xF0"
#define SIR_MAC_VENDOR_AP_4_OUI_LEN         3

#define SIR_MAC_BA_2K_JUMP_AP_VENDOR_OUI             "\x00\x14\x6C"
#define SIR_MAC_BA_2K_JUMP_AP_VENDOR_OUI_LEN         3

#define SIR_MAC_BAD_HTC_HE_VENDOR_OUI1             "\x00\x50\xF2\x11"
#define SIR_MAC_BAD_HTC_HE_VENDOR_OUI2             "\x00\x50\xF2\x12"
#define SIR_MAC_BAD_HTC_HE_VENDOR_OUI_LEN         4

/* Maximum allowable size of a beacon and probe rsp frame */
#define SIR_MAX_BEACON_SIZE    512
#define SIR_MAX_PROBE_RESP_SIZE 512

/* / Frame control field format (2 bytes) */
typedef struct sSirMacFrameCtl {

#ifndef ANI_LITTLE_BIT_ENDIAN

	uint8_t subType:4;
	uint8_t type:2;
	uint8_t protVer:2;

	uint8_t order:1;
	uint8_t wep:1;
	uint8_t moreData:1;
	uint8_t powerMgmt:1;
	uint8_t retry:1;
	uint8_t moreFrag:1;
	uint8_t fromDS:1;
	uint8_t toDS:1;

#else

	uint8_t protVer:2;
	uint8_t type:2;
	uint8_t subType:4;

	uint8_t toDS:1;
	uint8_t fromDS:1;
	uint8_t moreFrag:1;
	uint8_t retry:1;
	uint8_t powerMgmt:1;
	uint8_t moreData:1;
	uint8_t wep:1;
	uint8_t order:1;

#endif

} qdf_packed tSirMacFrameCtl, *tpSirMacFrameCtl;

/* / Sequence control field */
typedef struct sSirMacSeqCtl {

#ifndef ANI_LITTLE_BIT_ENDIAN

	uint8_t seqNumLo:4;
	uint8_t fragNum:4;

	uint8_t seqNumHi:8;

#else

	uint8_t fragNum:4;
	uint8_t seqNumLo:4;
	uint8_t seqNumHi:8;

#endif
} qdf_packed tSirMacSeqCtl, *tpSirMacSeqCtl;

/* / QoS control field */
typedef struct sSirMacQosCtl {

#ifndef ANI_LITTLE_BIT_ENDIAN

	uint8_t rsvd:1;
	uint8_t ackPolicy:2;
	uint8_t esop_txopUnit:1;
	uint8_t tid:4;

	uint8_t txop:8;

#else

	uint8_t tid:4;
	uint8_t esop_txopUnit:1;
	uint8_t ackPolicy:2;
	uint8_t rsvd:1;

	uint8_t txop:8;

#endif
} qdf_packed tSirMacQosCtl, *tpSirMacQosCtl;

/* / Length (in bytes) of MAC header in 3 address format */
#define SIR_MAC_HDR_LEN_3A    24

typedef uint8_t tSirMacAddr[ETH_ALEN];

/* / 3 address MAC data header format (24/26 bytes) */
typedef struct sSirMacDot3Hdr {
	tSirMacAddr da;
	tSirMacAddr sa;
	uint16_t length;
} qdf_packed tSirMacDot3Hdr, *tpSirMacDot3Hdr;

/* / 3 address MAC data header format (24/26 bytes) */
typedef struct sSirMacDataHdr3a {
	tSirMacFrameCtl fc;
	uint8_t durationLo;
	uint8_t durationHi;
	tSirMacAddr addr1;
	tSirMacAddr addr2;
	tSirMacAddr addr3;
	tSirMacSeqCtl seqControl;
	tSirMacQosCtl qosControl;
} qdf_packed tSirMacDataHdr3a, *tpSirMacDataHdr3a;

/* / Management header format */
typedef struct sSirMacMgmtHdr {
	tSirMacFrameCtl fc;
	uint8_t durationLo;
	uint8_t durationHi;
	tSirMacAddr da;
	tSirMacAddr sa;
	tSirMacAddr bssId;
	tSirMacSeqCtl seqControl;
} qdf_packed tSirMacMgmtHdr, *tpSirMacMgmtHdr;

/* / ERP information field */
typedef struct sSirMacErpInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t reserved:5;
	uint8_t barkerPreambleMode:1;
	uint8_t useProtection:1;
	uint8_t nonErpPresent:1;
#else
	uint8_t nonErpPresent:1;
	uint8_t useProtection:1;
	uint8_t barkerPreambleMode:1;
	uint8_t reserved:5;
#endif
} qdf_packed tSirMacErpInfo, *tpSirMacErpInfo;

/* / Capability information field */
typedef struct sSirMacCapabilityInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t immediateBA:1;
	uint16_t delayedBA:1;
	uint16_t dsssOfdm:1;
	uint16_t rrm:1;
	uint16_t apsd:1;
	uint16_t shortSlotTime:1;
	uint16_t qos:1;
	uint16_t spectrumMgt:1;
	uint16_t channelAgility:1;
	uint16_t pbcc:1;
	uint16_t shortPreamble:1;
	uint16_t privacy:1;
	uint16_t cfPollReq:1;
	uint16_t cfPollable:1;
	uint16_t ibss:1;
	uint16_t ess:1;
#else
	uint16_t ess:1;
	uint16_t ibss:1;
	uint16_t cfPollable:1;
	uint16_t cfPollReq:1;
	uint16_t privacy:1;
	uint16_t shortPreamble:1;
	uint16_t pbcc:1;
	uint16_t channelAgility:1;
	uint16_t spectrumMgt:1;
	uint16_t qos:1;
	uint16_t shortSlotTime:1;
	uint16_t apsd:1;
	uint16_t rrm:1;
	uint16_t dsssOfdm:1;
	uint16_t delayedBA:1;
	uint16_t immediateBA:1;
#endif
} qdf_packed tSirMacCapabilityInfo, *tpSirMacCapabilityInfo;

typedef struct sSirMacCfParamSet {
	uint8_t cfpCount;
	uint8_t cfpPeriod;
	uint16_t cfpMaxDuration;
	uint16_t cfpDurRemaining;
} qdf_packed tSirMacCfParamSet;

typedef struct sSirMacTim {
	uint8_t dtimCount;
	uint8_t dtimPeriod;
	uint8_t bitmapControl;
	uint8_t bitmapLength;
	uint8_t bitmap[251];
} qdf_packed tSirMacTim;

/* 12 Bytes long because this structure can be used to represent rate */
/* and extended rate set IEs */
/* The parser assume this to be at least 12 */
typedef struct sSirMacRateSet {
	uint8_t numRates;
	uint8_t rate[WLAN_SUPPORTED_RATES_IE_MAX_LEN];
} qdf_packed tSirMacRateSet;

/** struct merged_mac_rate_set - merged mac rate set
 * @num_rates: num of rates
 * @rate: rate list
 */
struct merged_mac_rate_set {
	uint8_t num_rates;
	uint8_t rate[2 * WLAN_SUPPORTED_RATES_IE_MAX_LEN];
};

/* Reserve 1 byte for NULL character in the SSID name field to print in %s */
typedef struct sSirMacSSid {
	uint8_t length;
	uint8_t ssId[WLAN_SSID_MAX_LEN +1];
} qdf_packed tSirMacSSid;

typedef struct sSirMacWpaInfo {
	uint8_t length;
	uint8_t info[WLAN_MAX_IE_LEN];
} qdf_packed tSirMacWpaInfo, *tpSirMacWpaInfo,
tSirMacRsnInfo, *tpSirMacRsnInfo;

typedef struct sSirMacWapiInfo {
	uint8_t length;
	uint8_t info[WLAN_MAX_IE_LEN];
} qdf_packed tSirMacWapiInfo, *tpSirMacWapiInfo;

typedef struct sSirMacFHParamSet {
	uint16_t dwellTime;
	uint8_t hopSet;
	uint8_t hopPattern;
	uint8_t hopIndex;
} tSirMacFHParamSet, *tpSirMacFHParamSet;

typedef struct sSirMacIBSSParams {
	uint16_t atim;
} tSirMacIBSSParams, *tpSirMacIBSSParams;

typedef struct sSirMacRRMEnabledCap {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t reserved:6;
	uint8_t AntennaInformation:1;
	uint8_t BSSAvailAdmission:1;
	uint8_t BssAvgAccessDelay:1;
	uint8_t RSNIMeasurement:1;
	uint8_t RCPIMeasurement:1;
	uint8_t NeighborTSFOffset:1;
	uint8_t MeasurementPilotEnabled:1;
	uint8_t MeasurementPilot:3;
	uint8_t nonOperatinChanMax:3;
	uint8_t operatingChanMax:3;
	uint8_t RRMMIBEnabled:1;
	uint8_t APChanReport:1;
	uint8_t triggeredTCM:1;
	uint8_t TCMCapability:1;
	uint8_t LCIAzimuth:1;
	uint8_t LCIMeasurement:1;
	uint8_t statistics:1;
	uint8_t NoiseHistogram:1;
	uint8_t ChannelLoad:1;
	uint8_t FrameMeasurement:1;
	uint8_t BeaconRepCond:1;
	uint8_t BeaconTable:1;
	uint8_t BeaconActive:1;
	uint8_t BeaconPassive:1;
	uint8_t repeated:1;
	uint8_t parallel:1;
	uint8_t NeighborRpt:1;
	uint8_t LinkMeasurement:1;
	uint8_t present;
#else
	uint8_t present;
	uint8_t LinkMeasurement:1;
	uint8_t NeighborRpt:1;
	uint8_t parallel:1;
	uint8_t repeated:1;
	uint8_t BeaconPassive:1;
	uint8_t BeaconActive:1;
	uint8_t BeaconTable:1;
	uint8_t BeaconRepCond:1;
	uint8_t FrameMeasurement:1;
	uint8_t ChannelLoad:1;
	uint8_t NoiseHistogram:1;
	uint8_t statistics:1;
	uint8_t LCIMeasurement:1;
	uint8_t LCIAzimuth:1;
	uint8_t TCMCapability:1;
	uint8_t triggeredTCM:1;
	uint8_t APChanReport:1;
	uint8_t RRMMIBEnabled:1;
	uint8_t operatingChanMax:3;
	uint8_t nonOperatinChanMax:3;
	uint8_t MeasurementPilot:3;
	uint8_t MeasurementPilotEnabled:1;
	uint8_t NeighborTSFOffset:1;
	uint8_t RCPIMeasurement:1;
	uint8_t RSNIMeasurement:1;
	uint8_t BssAvgAccessDelay:1;
	uint8_t BSSAvailAdmission:1;
	uint8_t AntennaInformation:1;
	uint8_t reserved:6;
#endif
} tSirMacRRMEnabledCap, *tpSirMacRRMEnabledCap;

#define MU_EDCA_DEF_AIFSN     0
#define MU_EDCA_DEF_CW_MAX    15
#define MU_EDCA_DEF_CW_MIN    15
#define MU_EDCA_DEF_TIMER     255
/* access category record */
typedef struct sSirMacAciAifsn {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t rsvd:1;
	uint8_t aci:2;
	uint8_t acm:1;
	uint8_t aifsn:4;
#else
	uint8_t aifsn:4;
	uint8_t acm:1;
	uint8_t aci:2;
	uint8_t rsvd:1;
#endif
} qdf_packed tSirMacAciAifsn;

/* contention window size */
typedef struct sSirMacCW {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t max:4;
	uint8_t min:4;
#else
	uint8_t min:4;
	uint8_t max:4;
#endif
} qdf_packed tSirMacCW;

typedef struct sSirMacEdcaParamRecord {
	tSirMacAciAifsn aci;
	tSirMacCW cw;
	union {
		uint16_t txoplimit;
		uint16_t mu_edca_timer;
	};
	uint8_t no_ack;
} qdf_packed tSirMacEdcaParamRecord;

typedef struct sSirMacQosInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t uapsd:1;
	uint8_t txopreq:1;
	uint8_t qreq:1;
	uint8_t qack:1;
	uint8_t count:4;
#else
	uint8_t count:4;
	uint8_t qack:1;
	uint8_t qreq:1;
	uint8_t txopreq:1;
	uint8_t uapsd:1;
#endif
} qdf_packed tSirMacQosInfo;

typedef struct sSirMacQosInfoStation {
#ifdef ANI_LITTLE_BIT_ENDIAN
	uint8_t acvo_uapsd:1;
	uint8_t acvi_uapsd:1;
	uint8_t acbk_uapsd:1;
	uint8_t acbe_uapsd:1;
	uint8_t qack:1;
	uint8_t maxSpLen:2;
	uint8_t moreDataAck:1;
#else
	uint8_t moreDataAck:1;
	uint8_t maxSpLen:2;
	uint8_t qack:1;
	uint8_t acbe_uapsd:1;
	uint8_t acbk_uapsd:1;
	uint8_t acvi_uapsd:1;
	uint8_t acvo_uapsd:1;
#endif
} qdf_packed tSirMacQosInfoStation, *tpSirMacQosInfoStation;

typedef struct sSirMacEdcaParamSetIE {
	uint8_t type;
	uint8_t length;
	tSirMacQosInfo qosInfo;
	uint8_t rsvd;
	tSirMacEdcaParamRecord acbe;    /* best effort */
	tSirMacEdcaParamRecord acbk;    /* background */
	tSirMacEdcaParamRecord acvi;    /* video */
	tSirMacEdcaParamRecord acvo;    /* voice */
} qdf_packed tSirMacEdcaParamSetIE;

/* ts info direction field can take any of these values */
#define SIR_MAC_DIRECTION_UPLINK    0
#define SIR_MAC_DIRECTION_DNLINK    1
#define SIR_MAC_DIRECTION_DIRECT    2
#define SIR_MAC_DIRECTION_BIDIR     3

/* access policy */
/* reserved                         0 */
#define SIR_MAC_ACCESSPOLICY_EDCA   1
#define SIR_MAC_ACCESSPOLICY_HCCA   2
#define SIR_MAC_ACCESSPOLICY_BOTH   3

/* frame classifier types */
#define SIR_MAC_TCLASTYPE_ETHERNET 0
#define SIR_MAC_TCLASTYPE_TCPUDPIP 1
#define SIR_MAC_TCLASTYPE_8021DQ   2
/* reserved                        3-255 */

typedef struct sSirMacTclasParamEthernet {
	tSirMacAddr srcAddr;
	tSirMacAddr dstAddr;
	uint16_t type;
} qdf_packed tSirMacTclasParamEthernet;

typedef struct sSirMacTclasParamIPv4 {
	uint8_t version;
	uint8_t srcIpAddr[4];
	uint8_t dstIpAddr[4];
	uint16_t srcPort;
	uint16_t dstPort;
	uint8_t dscp;
	uint8_t protocol;
	uint8_t rsvd;
} qdf_packed tSirMacTclasParamIPv4;

#define SIR_MAC_TCLAS_IPV4  4

typedef struct sSirMacTclasParamIPv6 {
	uint8_t version;
	uint8_t srcIpAddr[16];
	uint8_t dstIpAddr[16];
	uint16_t srcPort;
	uint16_t dstPort;
	uint8_t flowLabel[3];
} qdf_packed tSirMacTclasParamIPv6;

typedef struct sSirMacTclasParam8021dq {
	uint16_t tag;
} qdf_packed tSirMacTclasParam8021dq;

typedef struct sSirMacTclasIE {
	uint8_t type;
	uint8_t length;
	uint8_t userPrio;
	uint8_t classifierType;
	uint8_t classifierMask;
} qdf_packed tSirMacTclasIE;

typedef struct sSirMacTsDelayIE {
	uint8_t type;
	uint8_t length;
	uint32_t delay;
} qdf_packed tSirMacTsDelayIE;

typedef struct sSirMacScheduleInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t rsvd:9;
	uint16_t direction:2;
	uint16_t tsid:4;
	uint16_t aggregation:1;
#else
	uint16_t aggregation:1;
	uint16_t tsid:4;
	uint16_t direction:2;
	uint16_t rsvd:9;
#endif
} qdf_packed tSirMacScheduleInfo;

typedef struct sSirMacScheduleIE {
	uint8_t type;
	uint8_t length;
	tSirMacScheduleInfo info;
	uint32_t svcStartTime;
	uint32_t svcInterval;
	uint16_t maxSvcDuration;
	uint16_t specInterval;
} qdf_packed tSirMacScheduleIE;

typedef struct sSirMacQosCapabilityIE {
	uint8_t type;
	uint8_t length;
	tSirMacQosInfo qosInfo;
} qdf_packed tSirMacQosCapabilityIE;

typedef struct sSirMacQosCapabilityStaIE {
	uint8_t type;
	uint8_t length;
	tSirMacQosInfoStation qosInfo;
} qdf_packed tSirMacQosCapabilityStaIE;

typedef uint32_t tSirMacTimeStamp[2];

typedef uint16_t tSirMacBeaconInterval;

typedef uint16_t tSirMacListenInterval;

typedef uint8_t tSirMacChanNum;

/* IE definitions */
typedef struct sSirMacSSidIE {
	uint8_t type;
	tSirMacSSid ssId;
} qdf_packed tSirMacSSidIE;

typedef struct sSirMacRateSetIE {
	uint8_t type;
	tSirMacRateSet supportedRateSet;
} qdf_packed tSirMacRateSetIE;

typedef struct sSirMacDsParamSetIE {
	uint8_t type;
	uint8_t length;
	tSirMacChanNum channelNumber;
} qdf_packed tSirMacDsParamSetIE;

typedef struct sSirMacCfParamSetIE {
	uint8_t type;
	uint8_t length;
	tSirMacCfParamSet cfParams;
} qdf_packed tSirMacCfParamSetIE;

typedef struct sSirMacChanInfo {
	uint32_t first_freq;
	uint8_t numChannels;
	int8_t maxTxPower;
} qdf_packed tSirMacChanInfo;

typedef struct sSirMacNonErpPresentIE {
	uint8_t type;
	uint8_t length;
	uint8_t erp;
} qdf_packed tSirMacNonErpPresentIE;

typedef struct sSirMacPowerCapabilityIE {
	uint8_t type;
	uint8_t length;
	uint8_t minTxPower;
	uint8_t maxTxPower;
} tSirMacPowerCapabilityIE;

typedef struct sSirMacSupportedChannelIE {
	uint8_t type;
	uint8_t length;
	uint8_t supportedChannels[96];
} tSirMacSupportedChannelIE;

typedef struct sSirMacMeasReqField {
	uint8_t channelNumber;
	uint8_t measStartTime[8];
	uint16_t measDuration;
} tSirMacMeasReqField, *tpSirMacMeasReqField;

typedef struct sSirMacMeasReqIE {
	uint8_t type;
	uint8_t length;
	uint8_t measToken;
	uint8_t measReqMode;
	uint8_t measType;
	tSirMacMeasReqField measReqField;
} tSirMacMeasReqIE, *tpSirMacMeasReqIE;

/* VHT Capabilities Info */
typedef struct sSirMacVHTCapabilityInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint32_t extended_nss_bw_supp:2;
	uint32_t txAntPattern:1;
	uint32_t rxAntPattern:1;
	uint32_t vhtLinkAdaptCap:2;
	uint32_t maxAMPDULenExp:3;
	uint32_t htcVHTCap:1;
	uint32_t vhtTXOPPS:1;
	uint32_t muBeamformeeCap:1;
	uint32_t muBeamformerCap:1;
	uint32_t numSoundingDim:3;
	uint32_t csnofBeamformerAntSup:3;
	uint32_t suBeamformeeCap:1;
	uint32_t suBeamFormerCap:1;
	uint32_t rxSTBC:3;
	uint32_t txSTBC:1;
	uint32_t shortGI160and80plus80MHz:1;
	uint32_t shortGI80MHz:1;
	uint32_t ldpcCodingCap:1;
	uint32_t supportedChannelWidthSet:2;
	uint32_t maxMPDULen:2;
#else
	uint32_t maxMPDULen:2;
	uint32_t supportedChannelWidthSet:2;
	uint32_t ldpcCodingCap:1;
	uint32_t shortGI80MHz:1;
	uint32_t shortGI160and80plus80MHz:1;
	uint32_t txSTBC:1;
	uint32_t rxSTBC:3;
	uint32_t suBeamFormerCap:1;
	uint32_t suBeamformeeCap:1;
	uint32_t csnofBeamformerAntSup:3;
	uint32_t numSoundingDim:3;
	uint32_t muBeamformerCap:1;
	uint32_t muBeamformeeCap:1;
	uint32_t vhtTXOPPS:1;
	uint32_t htcVHTCap:1;
	uint32_t maxAMPDULenExp:3;
	uint32_t vhtLinkAdaptCap:2;
	uint32_t rxAntPattern:1;
	uint32_t txAntPattern:1;
	uint32_t extended_nss_bw_supp:2;
#endif
} qdf_packed tSirMacVHTCapabilityInfo;

typedef struct sSirMacVHTTxSupDataRateInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t reserved:2;
	uint16_t vht_extended_nss_bw_cap:1;
	uint16_t txSupDataRate:13;
#else
	uint16_t txSupDataRate:13;
	uint16_t vht_extended_nss_bw_cap:1;
	uint16_t reserved:2;
#endif
} qdf_packed tSirMacVHTTxSupDataRateInfo;

typedef struct sSirMacVHTRxSupDataRateInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t max_nsts_total:3;
	uint16_t rxSupDataRate:13;
#else
	uint16_t rxSupDataRate:13;
	uint16_t max_nsts_total:3;
#endif
} qdf_packed tSirMacVHTRxSupDataRateInfo;

/**
 * struct sSirVhtMcsInfo - VHT MCS information
 * @rx_mcs_map: RX MCS map 2 bits for each stream, total 8 streams
 * @rx_highest: Indicates highest long GI VHT PPDU data rate
 *      STA can receive. Rate expressed in units of 1 Mbps.
 *      If this field is 0 this value should not be used to
 *      consider the highest RX data rate supported.
 * @tx_mcs_map: TX MCS map 2 bits for each stream, total 8 streams
 * @tx_highest: Indicates highest long GI VHT PPDU data rate
 *      STA can transmit. Rate expressed in units of 1 Mbps.
 *      If this field is 0 this value should not be used to
 *      consider the highest TX data rate supported.
 */
typedef struct sSirVhtMcsInfo {
	uint16_t rxMcsMap;
	uint16_t rxHighest;
	uint16_t txMcsMap;
	uint16_t txHighest;
} tSirVhtMcsInfo;

/**
 * struct sSirVHtCap - VHT capabilities
 *
 * This structure is the "VHT capabilities element" as
 * described in 802.11ac D3.0 8.4.2.160
 * @vht_cap_info: VHT capability info
 * @supp_mcs: VHT MCS supported rates
 */
typedef struct sSirVHtCap {
	uint32_t vhtCapInfo;
	tSirVhtMcsInfo suppMcs;
} tSirVHTCap;

/* */
/* Determines the current operating mode of the 802.11n STA */
/* */

typedef enum eSirMacHTOperatingMode {
	eSIR_HT_OP_MODE_PURE,   /* No Protection */
	eSIR_HT_OP_MODE_OVERLAP_LEGACY, /* Overlap Legacy device present, protection is optional */
	eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT,     /* No legacy device, but 20 MHz HT present */
	eSIR_HT_OP_MODE_MIXED   /* Protetion is required */
} tSirMacHTOperatingMode;

/* Spatial Multiplexing(SM) Power Save mode */
typedef enum eSirMacHTMIMOPowerSaveState {
	eSIR_HT_MIMO_PS_STATIC = 0,     /* Static SM Power Save mode */
	eSIR_HT_MIMO_PS_DYNAMIC = 1,    /* Dynamic SM Power Save mode */
	eSIR_HT_MIMO_PS_NA = 2, /* reserved */
	eSIR_HT_MIMO_PS_NO_LIMIT = 3    /* SM Power Save disabled */
} tSirMacHTMIMOPowerSaveState;

typedef enum eSirMacHTChannelWidth {
	eHT_CHANNEL_WIDTH_20MHZ = 0,
	eHT_CHANNEL_WIDTH_40MHZ = 1,
	eHT_CHANNEL_WIDTH_80MHZ = 2,
	eHT_CHANNEL_WIDTH_160MHZ = 3,
	eHT_CHANNEL_WIDTH_80P80MHZ = 4,
	eHT_MAX_CHANNEL_WIDTH
} tSirMacHTChannelWidth;

typedef enum eSirMacHTChannelType {
	eHT_CHAN_NO_HT = 0,
	eHT_CHAN_HT20 = 1,
	eHT_CHAN_HT40MINUS = 2,
	eHT_CHAN_HT40PLUS = 3
} tSirMacHTChannelType;

/* Packet struct for HT capability */
typedef struct sHtCaps {
	uint16_t advCodingCap:1;
	uint16_t supportedChannelWidthSet:1;
	uint16_t mimoPowerSave:2;
	uint16_t greenField:1;
	uint16_t shortGI20MHz:1;
	uint16_t shortGI40MHz:1;
	uint16_t txSTBC:1;
	uint16_t rxSTBC:2;
	uint16_t delayedBA:1;
	uint16_t maximalAMSDUsize:1;
	uint16_t dsssCckMode40MHz:1;
	uint16_t psmp:1;
	uint16_t stbcControlFrame:1;
	uint16_t lsigTXOPProtection:1;
	uint8_t maxRxAMPDUFactor:2;
	uint8_t mpduDensity:3;
	uint8_t reserved1:3;
	uint8_t supportedMCSSet[16];
	uint16_t pco:1;
	uint16_t transitionTime:2;
	uint16_t reserved2:5;
	uint16_t mcsFeedback:2;
	uint16_t reserved3:6;
	uint32_t txBF:1;
	uint32_t rxStaggeredSounding:1;
	uint32_t txStaggeredSounding:1;
	uint32_t rxZLF:1;
	uint32_t txZLF:1;
	uint32_t implicitTxBF:1;
	uint32_t calibration:2;
	uint32_t explicitCSITxBF:1;
	uint32_t explicitUncompressedSteeringMatrix:1;
	uint32_t explicitBFCSIFeedback:3;
	uint32_t explicitUncompressedSteeringMatrixFeedback:3;
	uint32_t explicitCompressedSteeringMatrixFeedback:3;
	uint32_t csiNumBFAntennae:2;
	uint32_t uncompressedSteeringMatrixBFAntennae:2;
	uint32_t compressedSteeringMatrixBFAntennae:2;
	uint32_t reserved4:7;
	uint8_t antennaSelection:1;
	uint8_t explicitCSIFeedbackTx:1;
	uint8_t antennaIndicesFeedbackTx:1;
	uint8_t explicitCSIFeedback:1;
	uint8_t antennaIndicesFeedback:1;
	uint8_t rxAS:1;
	uint8_t txSoundingPPDUs:1;
	uint8_t reserved5:1;

} qdf_packed tHtCaps;

/* Supported MCS set */
#define SIZE_OF_SUPPORTED_MCS_SET                          16
#define SIZE_OF_BASIC_MCS_SET                              16
#define VALID_MCS_SIZE                                     77   /* 0-76 */
#define MCS_RX_HIGHEST_SUPPORTED_RATE_BYTE_OFFSET          10
#define VALID_MAX_MCS_INDEX                                8

/* */
/* The following enums will be used to get the "current" HT Capabilities of */
/* the local STA in a generic fashion. In other words, the following enums */
/* identify the HT capabilities that can be queried or set. */
/* */
typedef enum eHTCapability {
	eHT_LSIG_TXOP_PROTECTION,
	eHT_STBC_CONTROL_FRAME,
	eHT_PSMP,
	eHT_DSSS_CCK_MODE_40MHZ,
	eHT_MAX_AMSDU_LENGTH,
	eHT_MAX_AMSDU_NUM,
	eHT_RX_STBC,
	eHT_TX_STBC,
	eHT_SHORT_GI_40MHZ,
	eHT_SHORT_GI_20MHZ,
	eHT_GREENFIELD,
	eHT_MIMO_POWER_SAVE,
	eHT_SUPPORTED_CHANNEL_WIDTH_SET,
	eHT_ADVANCED_CODING,
	eHT_MAX_RX_AMPDU_FACTOR,
	eHT_MPDU_DENSITY,
	eHT_PCO,
	eHT_TRANSITION_TIME,
	eHT_MCS_FEEDBACK,
	eHT_TX_BEAMFORMING,
	eHT_ANTENNA_SELECTION,
	/* The following come under Additional HT Capabilities */
	eHT_SI_GRANULARITY,
	eHT_CONTROLLED_ACCESS,
	eHT_RIFS_MODE,
	eHT_RECOMMENDED_TX_WIDTH_SET,
	eHT_EXTENSION_CHANNEL_OFFSET,
	eHT_OP_MODE,
	eHT_BASIC_STBC_MCS,
	eHT_DUAL_CTS_PROTECTION,
	eHT_LSIG_TXOP_PROTECTION_FULL_SUPPORT,
	eHT_PCO_ACTIVE,
	eHT_PCO_PHASE
} tHTCapability;

/* HT Parameters Info */
typedef struct sSirMacHTParametersInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t reserved:3;
	uint8_t mpduDensity:3;  /* Dynamic state */
	uint8_t maxRxAMPDUFactor:2;     /* Dynamic state */
#else
	uint8_t maxRxAMPDUFactor:2;
	uint8_t mpduDensity:3;
	uint8_t reserved:3;
#endif
} qdf_packed tSirMacHTParametersInfo;

/* Extended HT Capabilities Info */
typedef struct sSirMacExtendedHTCapabilityInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t reserved2:6;
	uint16_t mcsFeedback:2; /* Static via CFG */
	uint16_t reserved1:5;
	uint16_t transitionTime:2;      /* Static via CFG */
	uint16_t pco:1;         /* Static via CFG */
#else
	uint16_t pco:1;
	uint16_t transitionTime:2;
	uint16_t reserved1:5;
	uint16_t mcsFeedback:2;
	uint16_t reserved2:6;
#endif
} qdf_packed tSirMacExtendedHTCapabilityInfo;

/* IEEE 802.11n/D7.0 - 7.3.2.57.4 */
/* Part of the "supported MCS set field" */
typedef struct sSirMacRxHighestSupportRate {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint16_t reserved:6;
	uint16_t rate:10;
#else
	uint16_t rate:10;
	uint16_t reserved:6;
#endif
} qdf_packed tSirMacRxHighestSupportRate, *tpSirMacRxHighestSupportRate;

/* Transmit Beam Forming Capabilities Info */
typedef struct sSirMacTxBFCapabilityInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint32_t reserved:7;
	uint32_t compressedSteeringMatrixBFAntennae:2;  /* Static via CFG */
	/* Static via CFG */
	uint32_t uncompressedSteeringMatrixBFAntennae:2;
	uint32_t csiNumBFAntennae:2;    /* Static via CFG */
	/* Static via CFG */
	uint32_t explicitCompressedSteeringMatrixFeedback:3;
	/* Static via CFG */
	uint32_t explicitUncompressedSteeringMatrixFeedback:3;
	uint32_t explicitBFCSIFeedback:3;       /* Static via CFG */
	uint32_t explicitUncompressedSteeringMatrix:1;  /* Static via CFG */
	uint32_t explicitCSITxBF:1;     /* Static via CFG */
	uint32_t calibration:2; /* Static via CFG */
	uint32_t implicitTxBF:1;        /* Static via CFG */
	uint32_t txZLF:1;       /* Static via CFG */
	uint32_t rxZLF:1;       /* Static via CFG */
	uint32_t txStaggeredSounding:1; /* Static via CFG */
	uint32_t rxStaggeredSounding:1; /* Static via CFG */
	uint32_t txBF:1;        /* Static via CFG */
#else
	uint32_t txBF:1;
	uint32_t rxStaggeredSounding:1;
	uint32_t txStaggeredSounding:1;
	uint32_t rxZLF:1;
	uint32_t txZLF:1;
	uint32_t implicitTxBF:1;
	uint32_t calibration:2;
	uint32_t explicitCSITxBF:1;
	uint32_t explicitUncompressedSteeringMatrix:1;
	uint32_t explicitBFCSIFeedback:3;
	uint32_t explicitUncompressedSteeringMatrixFeedback:3;
	uint32_t explicitCompressedSteeringMatrixFeedback:3;
	uint32_t csiNumBFAntennae:2;
	uint32_t uncompressedSteeringMatrixBFAntennae:2;
	uint32_t compressedSteeringMatrixBFAntennae:2;
	uint32_t reserved:7;
#endif
} qdf_packed tSirMacTxBFCapabilityInfo;

/* Antenna Selection Capability Info */
typedef struct sSirMacASCapabilityInfo {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint8_t reserved2:1;
	uint8_t txSoundingPPDUs:1;      /* Static via CFG */
	uint8_t rxAS:1;         /* Static via CFG */
	uint8_t antennaIndicesFeedback:1;       /* Static via CFG */
	uint8_t explicitCSIFeedback:1;  /* Static via CFG */
	uint8_t antennaIndicesFeedbackTx:1;     /* Static via CFG */
	uint8_t explicitCSIFeedbackTx:1;        /* Static via CFG */
	uint8_t antennaSelection:1;     /* Static via CFG */
#else
	uint8_t antennaSelection:1;
	uint8_t explicitCSIFeedbackTx:1;
	uint8_t antennaIndicesFeedbackTx:1;
	uint8_t explicitCSIFeedback:1;
	uint8_t antennaIndicesFeedback:1;
	uint8_t rxAS:1;
	uint8_t txSoundingPPDUs:1;
	uint8_t reserved2:1;
#endif
} qdf_packed tSirMacASCapabilityInfo;

typedef struct sSirMacAuthFrameBody {
	uint16_t authAlgoNumber;
	uint16_t authTransactionSeqNumber;
	uint16_t authStatusCode;
	uint8_t type;           /* = WLAN_ELEMID_CHALLENGE */
	uint8_t length;         /* = SIR_MAC_AUTH_CHALLENGE_LENGTH */
	uint8_t challengeText[SIR_MAC_AUTH_CHALLENGE_LENGTH];
#ifdef WLAN_FEATURE_FILS_SK
	tSirMacRsnInfo rsn_ie;
	struct mac_ft_ie ft_ie;
	uint8_t assoc_delay_info;
	uint8_t session[SIR_FILS_SESSION_LENGTH];
	uint8_t wrapped_data_len;
	uint8_t wrapped_data[SIR_FILS_WRAPPED_DATA_MAX_SIZE];
	uint8_t nonce[SIR_FILS_NONCE_LENGTH];
#endif
} qdf_packed tSirMacAuthFrameBody, *tpSirMacAuthFrameBody;

/* / Common header for all action frames */
typedef struct sSirMacActionFrameHdr {
	uint8_t category;
	uint8_t actionID;
} qdf_packed tSirMacActionFrameHdr, *tpSirMacActionFrameHdr;

typedef struct sSirMacVendorSpecificFrameHdr {
	uint8_t category;
	uint8_t Oui[4];
} qdf_packed tSirMacVendorSpecificFrameHdr,
*tpSirMacVendorSpecificFrameHdr;

typedef struct sSirMacVendorSpecificPublicActionFrameHdr {
	uint8_t category;
	uint8_t actionID;
	uint8_t Oui[4];
	uint8_t OuiSubType;
	uint8_t dialogToken;
} qdf_packed tSirMacVendorSpecificPublicActionFrameHdr,
*tpSirMacVendorSpecificPublicActionFrameHdr;

typedef struct sSirMacMeasActionFrameHdr {
	uint8_t category;
	uint8_t actionID;
	uint8_t dialogToken;
} tSirMacMeasActionFrameHdr, *tpSirMacMeasActionFrameHdr;

#ifdef ANI_SUPPORT_11H
typedef struct sSirMacTpcReqActionFrame {
	tSirMacMeasActionFrameHdr actionHeader;
	uint8_t type;
	uint8_t length;
} tSirMacTpcReqActionFrame, *tpSirMacTpcReqActionFrame;
typedef struct sSirMacMeasReqActionFrame {
	tSirMacMeasActionFrameHdr actionHeader;
	tSirMacMeasReqIE measReqIE;
} tSirMacMeasReqActionFrame, *tpSirMacMeasReqActionFrame;
#endif

typedef struct sSirMacNeighborReportReq {
	uint8_t dialogToken;
	uint8_t ssid_present;
	tSirMacSSid ssid;
} tSirMacNeighborReportReq, *tpSirMacNeighborReportReq;

typedef struct sSirMacLinkReport {
	uint8_t dialogToken;
	uint8_t txPower;
	uint8_t rxAntenna;
	uint8_t txAntenna;
	uint8_t rcpi;
	uint8_t rsni;
} tSirMacLinkReport, *tpSirMacLinkReport;

#define BEACON_REPORT_MAX_IES 215
/* Max number of beacon reports per channel supported in the driver */
#define MAX_BEACON_REPORTS 32
/* Offset of IEs after Fixed Fields in Beacon Frame */
#define BEACON_FRAME_IES_OFFSET 12

/**
 * struct bcn_report_frame_body_frag_id - beacon report reported frame body
 *					  fragment ID sub element params
 * @id: report ID
 * @frag_id: fragment ID
 * @more_frags: more frags present or not present
 */
struct bcn_report_frame_body_frag_id {
	uint8_t id;
	uint8_t frag_id;
	bool more_frags;
};

/**
 * struct sSirMacBeaconReport - Beacon Report Structure
 * @regClass: Regulatory Class
 * @channel: Channel for which the current report is being sent
 * @measStartTime: RRM scan start time for this report
 * @measDuration: Scan duration for the current channel
 * @phyType: Condensed Phy Type
 * @bcnProbeRsp: Beacon or probe response being reported
 * @rsni: Received signal-to-noise indication
 * @rcpi: Received Channel Power indication
 * @bssid: BSSID of the AP requesting the beacon report
 * @antennaId: Number of Antennas used for measurement
 * @parentTSF: measuring STA's TSF timer value
 * @numIes: Number of IEs included in the beacon frames
 * @last_bcn_report_ind_support: Support for Last beacon report indication
 * @is_last_bcn_report: Is the current report last or more reports present
 * @frame_body_frag_id: Reported Frame Body Frag Id sub-element params
 * @Ies: IEs included in the beacon report
 */
typedef struct sSirMacBeaconReport {
	uint8_t regClass;
	uint8_t channel;
	uint8_t measStartTime[8];
	uint8_t measDuration;
	uint8_t phyType;
	uint8_t bcnProbeRsp;
	uint8_t rsni;
	uint8_t rcpi;
	tSirMacAddr bssid;
	uint8_t antennaId;
	uint32_t parentTSF;
	uint8_t numIes;
	uint8_t last_bcn_report_ind_support;
	uint8_t is_last_bcn_report;
	struct bcn_report_frame_body_frag_id frame_body_frag_id;
	uint8_t Ies[BEACON_REPORT_MAX_IES];

} tSirMacBeaconReport, *tpSirMacBeaconReport;

typedef struct sSirMacRadioMeasureReport {
	uint8_t token;
	uint8_t refused;
	uint8_t incapable;
	uint8_t type;
	union {
		tSirMacBeaconReport beaconReport;
	} report;

} tSirMacRadioMeasureReport, *tpSirMacRadioMeasureReport;

#ifdef WLAN_FEATURE_11AX
struct he_cap_network_endian {
	uint32_t htc_he:1;
	uint32_t twt_request:1;
	uint32_t twt_responder:1;
	uint32_t fragmentation:2;
	uint32_t max_num_frag_msdu_amsdu_exp:3;
	uint32_t min_frag_size:2;
	uint32_t trigger_frm_mac_pad:2;
	uint32_t multi_tid_aggr_rx_supp:3;
	uint32_t he_link_adaptation:2;
	uint32_t all_ack:1;
	uint32_t trigd_rsp_sched:1;
	uint32_t a_bsr:1;
	uint32_t broadcast_twt:1;
	uint32_t ba_32bit_bitmap:1;
	uint32_t mu_cascade:1;
	uint32_t ack_enabled_multitid:1;
	uint32_t reserved:1;
	uint32_t omi_a_ctrl:1;
	uint32_t ofdma_ra:1;
	uint32_t max_ampdu_len_exp_ext:2;
	uint32_t amsdu_frag:1;
	uint32_t flex_twt_sched:1;
	uint32_t rx_ctrl_frame:1;

	uint16_t bsrp_ampdu_aggr:1;
	uint16_t qtp:1;
	uint16_t a_bqr:1;
	uint16_t spatial_reuse_param_rspder:1;
	uint16_t ndp_feedback_supp:1;
	uint16_t ops_supp:1;
	uint16_t amsdu_in_ampdu:1;
	uint16_t multi_tid_aggr_tx_supp:3;
	uint16_t he_sub_ch_sel_tx_supp:1;
	uint16_t ul_2x996_tone_ru_supp:1;
	uint16_t om_ctrl_ul_mu_data_dis_rx:1;
	uint16_t he_dynamic_smps:1;
	uint16_t punctured_sounding_supp:1;
	uint16_t ht_vht_trg_frm_rx_supp:1;

	uint32_t reserved2:1;
	uint32_t chan_width:7;
	uint32_t rx_pream_puncturing:4;
	uint32_t device_class:1;
	uint32_t ldpc_coding:1;
	uint32_t he_1x_ltf_800_gi_ppdu:1;
	uint32_t midamble_tx_rx_max_nsts:2;
	uint32_t he_4x_ltf_3200_gi_ndp:1;
	uint32_t tb_ppdu_tx_stbc_lt_80mhz:1;
	uint32_t rx_stbc_lt_80mhz:1;
	uint32_t doppler:2;
	uint32_t ul_mu:2;
	uint32_t dcm_enc_tx:3;
	uint32_t dcm_enc_rx:3;
	uint32_t ul_he_mu:1;
	uint32_t su_beamformer:1;

	uint32_t su_beamformee:1;
	uint32_t mu_beamformer:1;
	uint32_t bfee_sts_lt_80:3;
	uint32_t bfee_sts_gt_80:3;
	uint32_t num_sounding_lt_80:3;
	uint32_t num_sounding_gt_80:3;
	uint32_t su_feedback_tone16:1;
	uint32_t mu_feedback_tone16:1;
	uint32_t codebook_su:1;
	uint32_t codebook_mu:1;
	uint32_t beamforming_feedback:3;
	uint32_t he_er_su_ppdu:1;
	uint32_t dl_mu_mimo_part_bw:1;
	uint32_t ppet_present:1;
	uint32_t srp:1;
	uint32_t power_boost:1;
	uint32_t he_ltf_800_gi_4x:1;
	uint32_t max_nc:3;
	uint32_t tb_ppdu_tx_stbc_gt_80mhz:1;
	uint32_t rx_stbc_gt_80mhz:1;

	uint16_t er_he_ltf_800_gi_4x:1;
	uint16_t he_ppdu_20_in_40Mhz_2G:1;
	uint16_t he_ppdu_20_in_160_80p80Mhz:1;
	uint16_t he_ppdu_80_in_160_80p80Mhz:1;
	uint16_t er_1x_he_ltf_gi:1;
	uint16_t midamble_tx_rx_1x_he_ltf:1;
	uint16_t dcm_max_bw:2;
	uint16_t longer_than_16_he_sigb_ofdm_sym:1;
	uint16_t non_trig_cqi_feedback:1;
	uint16_t tx_1024_qam_lt_242_tone_ru:1;
	uint16_t rx_1024_qam_lt_242_tone_ru:1;
	uint16_t rx_full_bw_su_he_mu_compress_sigb:1;
	uint16_t rx_full_bw_su_he_mu_non_cmpr_sigb:1;
	uint16_t reserved3:2;

	uint8_t  reserved4;

	uint16_t rx_he_mcs_map_lt_80;
	uint16_t tx_he_mcs_map_lt_80;
	uint16_t rx_he_mcs_map_160;
	uint16_t tx_he_mcs_map_160;
	uint16_t rx_he_mcs_map_80_80;
	uint16_t tx_he_mcs_map_80_80;
} qdf_packed;

struct he_ops_network_endian {
	uint16_t default_pe:3;
	uint16_t twt_required:1;
	uint16_t txop_rts_threshold:10;
	uint16_t vht_oper_present:1;
	uint16_t co_located_bss:1;
	uint8_t  er_su_disable:1;
	uint8_t  reserved1:7;
	uint8_t  bss_color:6;
	uint8_t  partial_bss_col:1;
	uint8_t  bss_col_disabled:1;
	uint8_t  basic_mcs_nss[2];
	union {
		struct {
			uint8_t chan_width;
			uint8_t center_freq_seg0;
			uint8_t center_freq_seg1;
		} info; /* vht_oper_present = 1 */
	} vht_oper;
	union {
		struct {
			uint8_t data;
		} info; /* co_located_bss = 1 */
	} maxbssid_ind;
} qdf_packed;

/* HE Capabilities Info */
struct he_capability_info {
#ifndef ANI_LITTLE_BIT_ENDIAN
	uint32_t rx_ctrl_frame:1;
	uint32_t flex_twt_sched:1;
	uint32_t amsdu_frag:1;
	uint32_t max_ampdu_len_exp_ext:2;
	uint32_t ofdma_ra:1;
	uint32_t omi_a_ctrl:1;
	uint32_t reserved:1;
	uint32_t ack_enabled_multitid:1;
	uint32_t mu_cascade:1;
	uint32_t ba_32bit_bitmap:1;
	uint32_t broadcast_twt:1;
	uint32_t a_bsr:1;
	uint32_t trigd_rsp_sched:1;
	uint32_t all_ack:1;
	uint32_t he_link_adaptation:2;
	uint32_t multi_tid_aggr_rx_supp:3;
	uint32_t trigger_frm_mac_pad:2;
	uint32_t min_frag_size:2;
	uint32_t max_num_frag_msdu_amsdu_exp:3;
	uint32_t fragmentation:2;
	uint32_t twt_responder:1;
	uint32_t twt_request:1;
	uint32_t htc_he:1;

	uint16_t ht_vht_trg_frm_rx_supp:1;
	uint16_t punctured_sounding_supp:1;
	uint16_t he_dynamic_smps:1;
	uint16_t om_ctrl_ul_mu_data_dis_rx:1;
	uint16_t ul_2x996_tone_ru_supp:1;
	uint16_t he_sub_ch_sel_tx_supp:1;
	uint16_t multi_tid_aggr_tx_supp:3;
	uint16_t amsdu_in_ampdu:1;
	uint16_t ops_supp:1;
	uint16_t ndp_feedback_supp:1;
	uint16_t spatial_reuse_param_rspder:1;
	uint16_t a_bqr:1;
	uint16_t qtp:1;
	uint16_t bsrp_ampdu_aggr:1;

	uint32_t su_beamformer:1;
	uint32_t ul_he_mu:1;
	uint32_t dcm_enc_rx:3;
	uint32_t dcm_enc_tx:3;
	uint32_t ul_mu:2;
	uint32_t doppler:2;
	uint32_t rx_stbc_lt_80mhz:1;
	uint32_t tb_ppdu_tx_stbc_lt_80mhz:1;
	uint32_t he_4x_ltf_3200_gi_ndp:1;
	uint32_t midamble_tx_rx_max_nsts:2;
	uint32_t he_1x_ltf_800_gi_ppdu:1;
	uint32_t ldpc_coding:1;
	uint32_t device_class:1;
	uint32_t rx_pream_puncturing:4;
	uint32_t chan_width:7;
	uint32_t reserved2:1;

	uint32_t rx_stbc_gt_80mhz:1;
	uint32_t tb_ppdu_tx_stbc_gt_80mhz:1;
	uint32_t max_nc:3;
	uint32_t he_ltf_800_gi_4x:1;
	uint32_t power_boost:1;
	uint32_t srp:1;
	uint32_t ppet_present:1;
	uint32_t dl_mu_mimo_part_bw:1;
	uint32_t he_er_su_ppdu:1;
	uint32_t beamforming_feedback:3;
	uint32_t codebook_mu:1;
	uint32_t codebook_su:1;
	uint32_t mu_feedback_tone16:1;
	uint32_t su_feedback_tone16:1;
	uint32_t num_sounding_gt_80:3;
	uint32_t num_sounding_lt_80:3;
	uint32_t bfee_sts_gt_80:3;
	uint32_t bfee_sts_lt_80:3;
	uint32_t mu_beamformer:1;
	uint32_t su_beamformee:1;

	uint16_t reserved3:2;
	uint16_t rx_full_bw_su_he_mu_non_cmpr_sigb:1;
	uint16_t rx_full_bw_su_he_mu_compress_sigb:1;
	uint16_t rx_1024_qam_lt_242_tone_ru:1;
	uint16_t tx_1024_qam_lt_242_tone_ru:1;
	uint16_t non_trig_cqi_feedback:1;
	uint16_t longer_than_16_he_sigb_ofdm_sym:1;
	uint16_t dcm_max_bw:2;
	uint16_t midamble_tx_rx_1x_he_ltf:1;
	uint16_t er_1x_he_ltf_gi:1;
	uint16_t he_ppdu_80_in_160_80p80Mhz:1;
	uint16_t he_ppdu_20_in_160_80p80Mhz:1;
	uint16_t he_ppdu_20_in_40Mhz_2G:1;
	uint16_t er_he_ltf_800_gi_4x:1;

	uint8_t reserved4;

	uint16_t tx_he_mcs_map_80_80;
	uint16_t rx_he_mcs_map_80_80;
	uint16_t tx_he_mcs_map_160;
	uint16_t rx_he_mcs_map_160;
	uint16_t tx_he_mcs_map_lt_80;
	uint16_t rx_he_mcs_map_lt_80;
#else
	uint32_t htc_he:1;
	uint32_t twt_request:1;
	uint32_t twt_responder:1;
	uint32_t fragmentation:2;
	uint32_t max_num_frag_msdu_amsdu_exp:3;
	uint32_t min_frag_size:2;
	uint32_t trigger_frm_mac_pad:2;
	uint32_t multi_tid_aggr_rx_supp:3;
	uint32_t he_link_adaptation:2;
	uint32_t all_ack:1;
	uint32_t trigd_rsp_sched:1;
	uint32_t a_bsr:1;
	uint32_t broadcast_twt:1;
	uint32_t ba_32bit_bitmap:1;
	uint32_t mu_cascade:1;
	uint32_t ack_enabled_multitid:1;
	uint32_t reserved:1;
	uint32_t omi_a_ctrl:1;
	uint32_t ofdma_ra:1;
	uint32_t max_ampdu_len_exp_ext:2;
	uint32_t amsdu_frag:1;
	uint32_t flex_twt_sched:1;
	uint32_t rx_ctrl_frame:1;

	uint16_t bsrp_ampdu_aggr:1;
	uint16_t qtp:1;
	uint16_t a_bqr:1;
	uint16_t spatial_reuse_param_rspder:1;
	uint16_t ndp_feedback_supp:1;
	uint16_t ops_supp:1;
	uint16_t amsdu_in_ampdu:1;
	uint16_t multi_tid_aggr_tx_supp:3;
	uint16_t he_sub_ch_sel_tx_supp:1;
	uint16_t ul_2x996_tone_ru_supp:1;
	uint16_t om_ctrl_ul_mu_data_dis_rx:1;
	uint16_t he_dynamic_smps:1;
	uint16_t punctured_sounding_supp:1;
	uint16_t ht_vht_trg_frm_rx_supp:1;

	uint32_t reserved2:1;
	uint32_t chan_width:7;
	uint32_t rx_pream_puncturing:4;
	uint32_t device_class:1;
	uint32_t ldpc_coding:1;
	uint32_t he_1x_ltf_800_gi_ppdu:1;
	uint32_t midamble_tx_rx_max_nsts:2;
	uint32_t he_4x_ltf_3200_gi_ndp:1;
	uint32_t tb_ppdu_tx_stbc_lt_80mhz:1;
	uint32_t rx_stbc_lt_80mhz:1;
	uint32_t doppler:2;
	uint32_t ul_mu:2;
	uint32_t dcm_enc_tx:3;
	uint32_t dcm_enc_rx:3;
	uint32_t ul_he_mu:1;
	uint32_t su_beamformer:1;

	uint32_t su_beamformee:1;
	uint32_t mu_beamformer:1;
	uint32_t bfee_sts_lt_80:3;
	uint32_t bfee_sts_gt_80:3;
	uint32_t num_sounding_lt_80:3;
	uint32_t num_sounding_gt_80:3;
	uint32_t su_feedback_tone16:1;
	uint32_t mu_feedback_tone16:1;
	uint32_t codebook_su:1;
	uint32_t codebook_mu:1;
	uint32_t beamforming_feedback:3;
	uint32_t he_er_su_ppdu:1;
	uint32_t dl_mu_mimo_part_bw:1;
	uint32_t ppet_present:1;
	uint32_t srp:1;
	uint32_t power_boost:1;
	uint32_t he_ltf_800_gi_4x:1;
	uint32_t max_nc:3;
	uint32_t tb_ppdu_tx_stbc_gt_80mhz:1;
	uint32_t rx_stbc_gt_80mhz:1;

	uint16_t er_he_ltf_800_gi_4x:1;
	uint16_t he_ppdu_20_in_40Mhz_2G:1;
	uint16_t he_ppdu_20_in_160_80p80Mhz:1;
	uint16_t he_ppdu_80_in_160_80p80Mhz:1;
	uint16_t er_1x_he_ltf_gi:1;
	uint16_t midamble_tx_rx_1x_he_ltf:1;
	uint16_t dcm_max_bw:2;
	uint16_t longer_than_16_he_sigb_ofdm_sym:1;
	uint16_t non_trig_cqi_feedback:1;
	uint16_t tx_1024_qam_lt_242_tone_ru:1;
	uint16_t rx_1024_qam_lt_242_tone_ru:1;
	uint16_t rx_full_bw_su_he_mu_compress_sigb:1;
	uint16_t rx_full_bw_su_he_mu_non_cmpr_sigb:1;
	uint16_t reserved3:2;

	uint8_t  reserved4;

	uint16_t rx_he_mcs_map_lt_80;
	uint16_t tx_he_mcs_map_lt_80;
	uint16_t rx_he_mcs_map_160;
	uint16_t tx_he_mcs_map_160;
	uint16_t rx_he_mcs_map_80_80;
	uint16_t tx_he_mcs_map_80_80;
#endif
} qdf_packed;

struct he_6ghz_capability_info {
	uint16_t min_mpdu_start_spacing:3;
	uint16_t    max_ampdu_len_exp:3;
	uint16_t         max_mpdu_len:2;

	uint16_t             reserved:1;
	uint16_t          sm_pow_save:2;
	uint16_t         rd_responder:1;
	uint16_t rx_ant_pattern_consistency:1;
	uint16_t tx_ant_pattern_consistency:1;
	uint16_t             reserved2:2;
} qdf_packed;
#endif

/*
 * frame parser does not include optional 160 and 80+80 mcs set for MIN IE len
 */
#define SIR_MAC_HE_CAP_MIN_LEN       (DOT11F_IE_HE_CAP_MIN_LEN)
#define HE_CAP_160M_MCS_MAP_LEN      4
#define HE_CAP_80P80_MCS_MAP_LEN     4
#define HE_CAP_OUI_LEN               3

/* QOS action frame definitions */

/* max number of possible tclas elements in any frame */
#define SIR_MAC_TCLASIE_MAXNUM  2

/* 11b rate encoding in MAC format */

#define SIR_MAC_RATE_1   0x02
#define SIR_MAC_RATE_2   0x04
#define SIR_MAC_RATE_5_5 0x0B
#define SIR_MAC_RATE_11  0x16

/* 11a/g rate encoding in MAC format */

#define SIR_MAC_RATE_6   0x0C
#define SIR_MAC_RATE_9   0x12
#define SIR_MAC_RATE_12  0x18
#define SIR_MAC_RATE_18  0x24
#define SIR_MAC_RATE_24  0x30
#define SIR_MAC_RATE_36  0x48
#define SIR_MAC_RATE_48  0x60
#define SIR_MAC_RATE_54  0x6C

/* ANI legacy supported rates */
#define SIR_MAC_RATE_72  0x01
#define SIR_MAC_RATE_96  0x03
#define SIR_MAC_RATE_108 0x05

/* ANI enhanced rates */
#define SIR_MAC_RATE_42  1000
#define SIR_MAC_RATE_84  1001
#define SIR_MAC_RATE_126 1002
#define SIR_MAC_RATE_144 1003
#define SIR_MAC_RATE_168 1004
#define SIR_MAC_RATE_192 1005
#define SIR_MAC_RATE_216 1006
#define SIR_MAC_RATE_240 1007

#define SIR_MAC_RATE_1_BITMAP    (1<<0)
#define SIR_MAC_RATE_2_BITMAP    (1<<1)
#define SIR_MAC_RATE_5_5_BITMAP  (1<<2)
#define SIR_MAC_RATE_11_BITMAP   (1<<3)
#define SIR_MAC_RATE_6_BITMAP    (1<<4)
#define SIR_MAC_RATE_9_BITMAP    (1<<5)
#define SIR_MAC_RATE_12_BITMAP   (1<<6)
#define SIR_MAC_RATE_18_BITMAP   (1<<7)
#define SIR_MAC_RATE_24_BITMAP   (1<<8)
#define SIR_MAC_RATE_36_BITMAP   (1<<9)
#define SIR_MAC_RATE_48_BITMAP   (1<<10)
#define SIR_MAC_RATE_54_BITMAP   (1<<11)

#define sirIsArate(x) ((((uint8_t)x) == SIR_MAC_RATE_6)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_9)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_12) || \
		       (((uint8_t)x) == SIR_MAC_RATE_18) || \
		       (((uint8_t)x) == SIR_MAC_RATE_24) || \
		       (((uint8_t)x) == SIR_MAC_RATE_36) || \
		       (((uint8_t)x) == SIR_MAC_RATE_48) || \
		       (((uint8_t)x) == SIR_MAC_RATE_54))

#define sirIsBrate(x) ((((uint8_t)x) == SIR_MAC_RATE_1)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_2)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_5_5) || \
		       (((uint8_t)x) == SIR_MAC_RATE_11))

#define sirIsGrate(x) ((((uint8_t)x) == SIR_MAC_RATE_1)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_2)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_5_5) || \
		       (((uint8_t)x) == SIR_MAC_RATE_11)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_6)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_9)   || \
		       (((uint8_t)x) == SIR_MAC_RATE_12)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_18)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_24)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_36)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_48)  || \
		       (((uint8_t)x) == SIR_MAC_RATE_54))

#define SIR_MAC_MIN_IE_LEN 2    /* Minimum IE length for IE validation */

#define SIR_MAC_TI_TYPE_ASSOC_COMEBACK          3

#define SIR_MAC_VHT_CAP_MAX_MPDU_LEN              0
#define SIR_MAC_VHT_CAP_SUPP_CH_WIDTH_SET         2
#define SIR_MAC_VHT_CAP_LDPC_CODING_CAP           4
#define SIR_MAC_VHT_CAP_SHORTGI_80MHZ             5
#define SIR_MAC_VHT_CAP_SHORTGI_160_80_80MHZ      6
#define SIR_MAC_VHT_CAP_TXSTBC                    7
#define SIR_MAC_VHT_CAP_RXSTBC                    8
#define SIR_MAC_VHT_CAP_SU_BEAMFORMER_CAP         11
#define SIR_MAC_VHT_CAP_SU_BEAMFORMEE_CAP         12
#define SIR_MAC_VHT_CAP_CSN_BEAMORMER_ANT_SUP     13
#define SIR_MAC_VHT_CAP_NUM_SOUNDING_DIM          16
#define SIR_MAC_VHT_CAP_NUM_BEAM_FORMER_CAP       19
#define SIR_MAC_VHT_CAP_NUM_BEAM_FORMEE_CAP       20
#define SIR_MAC_VHT_CAP_TXOPPS                    21
#define SIR_MAC_VHT_CAP_HTC_CAP                   22
#define SIR_MAC_VHT_CAP_MAX_AMDU_LEN_EXPO         23
#define SIR_MAC_VHT_CAP_LINK_ADAPT_CAP            26
#define SIR_MAC_VHT_CAP_RX_ANTENNA_PATTERN        28
#define SIR_MAC_VHT_CAP_TX_ANTENNA_PATTERN        29
#define SIR_MAC_VHT_CAP_EXTD_NSS_BW               30

#define SIR_MAC_HT_CAP_ADVCODING_S                 0
#define SIR_MAC_HT_CAP_CHWIDTH40_S                 1
#define SIR_MAC_HT_CAP_SMPOWERSAVE_DYNAMIC_S       2
#define SIR_MAC_HT_CAP_SM_RESERVED_S               3
#define SIR_MAC_HT_CAP_GREENFIELD_S                4
#define SIR_MAC_HT_CAP_SHORTGI20MHZ_S              5
#define SIR_MAC_HT_CAP_SHORTGI40MHZ_S              6
#define SIR_MAC_HT_CAP_TXSTBC_S                    7
#define SIR_MAC_HT_CAP_RXSTBC_S                    8
#define SIR_MAC_HT_CAP_DELAYEDBLKACK_S            10
#define SIR_MAC_HT_CAP_MAXAMSDUSIZE_S             11
#define SIR_MAC_HT_CAP_DSSSCCK40_S                12
#define SIR_MAC_HT_CAP_PSMP_S                     13
#define SIR_MAC_HT_CAP_INTOLERANT40_S             14
#define SIR_MAC_HT_CAP_LSIGTXOPPROT_S             15

#define SIR_MAC_TXSTBC                             1
#define SIR_MAC_RXSTBC                             1

#endif /* __MAC_PROT_DEFS_H */
