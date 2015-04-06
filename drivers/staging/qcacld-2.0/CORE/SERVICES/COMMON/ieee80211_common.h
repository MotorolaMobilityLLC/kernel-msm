/*
 * Copyright (c) 2011,2014 The Linux Foundation. All rights reserved.
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



#ifndef EXTERNAL_USE_ONLY
#include "osdep.h"
#endif /* EXTERNAL_USE_ONLY */
#include "_ieee80211_common.h"

#ifndef _COMMON_IEEE80211_H_
#define _COMMON_IEEE80211_H_

/*
 * 802.11 protocol definitions.
 */

/* is 802.11 address multicast/broadcast? */
#define IEEE80211_IS_MULTICAST(_a)  (*(_a) & 0x01)

#define IEEE80211_IS_IPV4_MULTICAST(_a)  (*(_a) == 0x01)

#define IEEE80211_IS_IPV6_MULTICAST(_a)         \
    ((_a)[0] == 0x33 &&                         \
     (_a)[1] == 0x33)


#define IEEE80211_IS_BROADCAST(_a)              \
    ((_a)[0] == 0xff &&                         \
     (_a)[1] == 0xff &&                         \
     (_a)[2] == 0xff &&                         \
     (_a)[3] == 0xff &&                         \
     (_a)[4] == 0xff &&                         \
     (_a)[5] == 0xff)

/* IEEE 802.11 PLCP header */
struct ieee80211_plcp_hdr {
    u_int16_t   i_sfd;
    u_int8_t    i_signal;
    u_int8_t    i_service;
    u_int16_t   i_length;
    u_int16_t   i_crc;
} __packed;

#define IEEE80211_PLCP_SFD      0xF3A0
#define IEEE80211_PLCP_SERVICE  0x00

/*
 * generic definitions for IEEE 802.11 frames
 */
struct ieee80211_frame {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    union {
        struct {
            u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
            u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
            u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
        };
        u_int8_t    i_addr_all[3 * IEEE80211_ADDR_LEN];
    };
    u_int8_t    i_seq[2];
    /* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
    /* see below */
} __packed;

struct ieee80211_qosframe {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
    u_int8_t    i_seq[2];
    u_int8_t    i_qos[2];
    /* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
    /* see below */
} __packed;

struct ieee80211_qoscntl {
    u_int8_t    i_qos[2];
};

struct ieee80211_frame_addr4 {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
    u_int8_t    i_seq[2];
    u_int8_t    i_addr4[IEEE80211_ADDR_LEN];
} __packed;

struct ieee80211_qosframe_addr4 {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
    u_int8_t    i_seq[2];
    u_int8_t    i_addr4[IEEE80211_ADDR_LEN];
    u_int8_t    i_qos[2];
} __packed;

/* HTC frame for TxBF*/
// for TxBF RC
struct ieee80211_frame_min_one {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];

} __packed;// For TxBF RC

struct ieee80211_qosframe_htc {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
    u_int8_t    i_seq[2];
    u_int8_t    i_qos[2];
    u_int8_t    i_htc[4];
    /* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
    /* see below */
} __packed;
struct ieee80211_qosframe_htc_addr4 {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr3[IEEE80211_ADDR_LEN];
    u_int8_t    i_seq[2];
    u_int8_t    i_addr4[IEEE80211_ADDR_LEN];
    u_int8_t    i_qos[2];
    u_int8_t    i_htc[4];
} __packed;
struct ieee80211_htc {
    u_int8_t    i_htc[4];
};
/*HTC frame for TxBF*/

struct ieee80211_ctlframe_addr2 {
    u_int8_t    i_fc[2];
    u_int8_t    i_aidordur[2]; /* AID or duration */
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
} __packed;

#define	IEEE80211_WHQ(wh)		((struct ieee80211_qosframe *)(wh))
#define	IEEE80211_WH4(wh)		((struct ieee80211_frame_addr4 *)(wh))
#define	IEEE80211_WHQ4(wh)		((struct ieee80211_qosframe_addr4 *)(wh))

#define IEEE80211_FC0_VERSION_MASK          0x03
#define IEEE80211_FC0_VERSION_SHIFT         0
#define IEEE80211_FC0_VERSION_0             0x00
#define IEEE80211_FC0_TYPE_MASK             0x0c
#define IEEE80211_FC0_TYPE_SHIFT            2
#define IEEE80211_FC0_TYPE_MGT              0x00
#define IEEE80211_FC0_TYPE_CTL              0x04
#define IEEE80211_FC0_TYPE_DATA             0x08

#define IEEE80211_FC0_SUBTYPE_MASK          0xf0
#define IEEE80211_FC0_SUBTYPE_SHIFT         4
/* for TYPE_MGT */
#define IEEE80211_FC0_SUBTYPE_ASSOC_REQ     0x00
#define IEEE80211_FC0_SUBTYPE_ASSOC_RESP    0x10
#define IEEE80211_FC0_SUBTYPE_REASSOC_REQ   0x20
#define IEEE80211_FC0_SUBTYPE_REASSOC_RESP  0x30
#define IEEE80211_FC0_SUBTYPE_PROBE_REQ     0x40
#define IEEE80211_FC0_SUBTYPE_PROBE_RESP    0x50
#define IEEE80211_FC0_SUBTYPE_BEACON        0x80
#define IEEE80211_FC0_SUBTYPE_ATIM          0x90
#define IEEE80211_FC0_SUBTYPE_DISASSOC      0xa0
#define IEEE80211_FC0_SUBTYPE_AUTH          0xb0
#define IEEE80211_FC0_SUBTYPE_DEAUTH        0xc0
#define IEEE80211_FC0_SUBTYPE_ACTION        0xd0
#define IEEE80211_FCO_SUBTYPE_ACTION_NO_ACK 0xe0
/* for TYPE_CTL */
#define IEEE80211_FCO_SUBTYPE_Control_Wrapper   0x70    // For TxBF RC
#define IEEE80211_FC0_SUBTYPE_BAR           0x80
#define IEEE80211_FC0_SUBTYPE_PS_POLL       0xa0
#define IEEE80211_FC0_SUBTYPE_RTS           0xb0
#define IEEE80211_FC0_SUBTYPE_CTS           0xc0
#define IEEE80211_FC0_SUBTYPE_ACK           0xd0
#define IEEE80211_FC0_SUBTYPE_CF_END        0xe0
#define IEEE80211_FC0_SUBTYPE_CF_END_ACK    0xf0
/* for TYPE_DATA (bit combination) */
#define IEEE80211_FC0_SUBTYPE_DATA          0x00
#define IEEE80211_FC0_SUBTYPE_CF_ACK        0x10
#define IEEE80211_FC0_SUBTYPE_CF_POLL       0x20
#define IEEE80211_FC0_SUBTYPE_CF_ACPL       0x30
#define IEEE80211_FC0_SUBTYPE_NODATA        0x40
#define IEEE80211_FC0_SUBTYPE_CFACK         0x50
#define IEEE80211_FC0_SUBTYPE_CFPOLL        0x60
#define IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK 0x70
#define IEEE80211_FC0_SUBTYPE_QOS           0x80
#define IEEE80211_FC0_SUBTYPE_QOS_NULL      0xc0

#define IEEE80211_FC1_DIR_MASK              0x03
#define IEEE80211_FC1_DIR_NODS              0x00    /* STA->STA */
#define IEEE80211_FC1_DIR_TODS              0x01    /* STA->AP  */
#define IEEE80211_FC1_DIR_FROMDS            0x02    /* AP ->STA */
#define IEEE80211_FC1_DIR_DSTODS            0x03    /* AP ->AP  */

#define IEEE80211_FC1_MORE_FRAG             0x04
#define IEEE80211_FC1_RETRY                 0x08
#define IEEE80211_FC1_PWR_MGT               0x10
#define IEEE80211_FC1_MORE_DATA             0x20
#define IEEE80211_FC1_WEP                   0x40
#define IEEE80211_FC1_ORDER                 0x80

#define IEEE80211_SEQ_FRAG_MASK             0x000f
#define IEEE80211_SEQ_FRAG_SHIFT            0
#define IEEE80211_SEQ_SEQ_MASK              0xfff0
#define IEEE80211_SEQ_SEQ_SHIFT             4
#define IEEE80211_SEQ_MAX                   4096

#define IEEE80211_SEQ_LEQ(a,b)  ((int)((a)-(b)) <= 0)


#define IEEE80211_QOS_TXOP                  0x00ff

#define IEEE80211_QOS_AMSDU                 0x80
#define IEEE80211_QOS_AMSDU_S               7
#define IEEE80211_QOS_ACKPOLICY             0x60
#define IEEE80211_QOS_ACKPOLICY_S           5
#define IEEE80211_QOS_EOSP                  0x10
#define IEEE80211_QOS_EOSP_S                4
#define IEEE80211_QOS_TID                   0x0f
#define IEEE80211_MFP_TID                   0xff

#define IEEE80211_HTC0_TRQ                  0x02
#define	IEEE80211_HTC2_CalPos               0x03
#define	IEEE80211_HTC2_CalSeq               0x0C
#define	IEEE80211_HTC2_CSI_NONCOMP_BF       0x80
#define	IEEE80211_HTC2_CSI_COMP_BF          0xc0

/* Set bits 14 and 15 to 1 when duration field carries Association ID */
#define IEEE80211_FIELD_TYPE_AID            0xC000

#define IEEE80211_IS_BEACON(_frame)    ((((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) && \
                                        (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BEACON))
#define IEEE80211_IS_DATA(_frame)      (((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)

#define IEEE80211_IS_MFP_FRAME(_frame) ((((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) && \
                                        ((_frame)->i_fc[1] & IEEE80211_FC1_WEP) && \
                                        ((((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_DEAUTH) || \
                                         (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_DISASSOC) || \
                                         (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_ACTION)))
#define IEEE80211_IS_AUTH(_frame)      ((((_frame)->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) && \
                                        (((_frame)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_AUTH))

/* MCS Set */
#define IEEE80211_RX_MCS_1_STREAM_BYTE_OFFSET 0
#define IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET 1
#define IEEE80211_RX_MCS_3_STREAM_BYTE_OFFSET 2
#define IEEE80211_RX_MCS_ALL_NSTREAM_RATES 0xff
#define IEEE80211_TX_MCS_OFFSET 12

#define IEEE80211_TX_MCS_SET_DEFINED 0x80
#define IEEE80211_TX_RX_MCS_SET_NOT_EQUAL 0x40
#define IEEE80211_TX_1_SPATIAL_STREAMS 0x0
#define IEEE80211_TX_2_SPATIAL_STREAMS 0x10
#define IEEE80211_TX_3_SPATIAL_STREAMS 0x20
#define IEEE80211_TX_4_SPATIAL_STREAMS 0x30

#define IEEE80211_TX_MCS_SET 0xf8

/*
 * Subtype data: If bit 6 is set then the data frame contains no actual data.
 */
#define IEEE80211_FC0_SUBTYPE_NO_DATA_MASK  0x40
#define IEEE80211_CONTAIN_DATA(_subtype) \
    (! ((_subtype) & IEEE80211_FC0_SUBTYPE_NO_DATA_MASK))

#define IEEE8023_MAX_LEN 0x600 /* 1536 - larger is Ethernet II */
#define RFC1042_SNAP_ORGCODE_0 0x00
#define RFC1042_SNAP_ORGCODE_1 0x00
#define RFC1042_SNAP_ORGCODE_2 0x00

#define BTEP_SNAP_ORGCODE_0 0x00
#define BTEP_SNAP_ORGCODE_1 0x00
#define BTEP_SNAP_ORGCODE_2 0xf8

/* BT 3.0 */
#define BTAMP_SNAP_ORGCODE_0 0x00
#define BTAMP_SNAP_ORGCODE_1 0x19
#define BTAMP_SNAP_ORGCODE_2 0x58

/* Aironet OUI Codes */
#define AIRONET_SNAP_CODE_0  0x00
#define AIRONET_SNAP_CODE_1  0x40
#define AIRONET_SNAP_CODE_2  0x96

#define IEEE80211_LSIG_LEN  3
#define IEEE80211_HTSIG_LEN 6
#define IEEE80211_SB_LEN    2

/*
 * Information element header format
 */
struct ieee80211_ie_header {
    u_int8_t    element_id;     /* Element Id */
    u_int8_t    length;         /* IE Length */
} __packed;

/*
 * Country information element.
 */
#define IEEE80211_COUNTRY_MAX_TRIPLETS (83)
struct ieee80211_ie_country {
    u_int8_t    country_id;
    u_int8_t    country_len;
    u_int8_t    country_str[3];
    u_int8_t    country_triplet[IEEE80211_COUNTRY_MAX_TRIPLETS*3];
} __packed;

/* does frame have QoS sequence control data */
#define IEEE80211_QOS_HAS_SEQ(wh) \
    (((wh)->i_fc[0] & \
      (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_QOS)) == \
      (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS))

#define WME_QOSINFO_UAPSD   0x80  /* Mask for U-APSD field */
#define WME_QOSINFO_COUNT   0x0f  /* Mask for Param Set Count field */
/*
 * WME/802.11e information element.
 */
struct ieee80211_ie_wme {
    u_int8_t    wme_id;         /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    wme_len;        /* length in bytes */
    u_int8_t    wme_oui[3];     /* 0x00, 0x50, 0xf2 */
    u_int8_t    wme_type;       /* OUI type */
    u_int8_t    wme_subtype;    /* OUI subtype */
    u_int8_t    wme_version;    /* spec revision */
    u_int8_t    wme_info;       /* QoS info */
} __packed;

/*
 * TS INFO part of the tspec element is a collection of bit flags
 */
#if _BYTE_ORDER == _BIG_ENDIAN
struct ieee80211_tsinfo_bitmap {
    u_int8_t    one       : 1,
                direction : 2,
                tid       : 4,
                reserved1 : 1;
    u_int8_t    reserved2 : 2,
                dot1Dtag  : 3,
                psb       : 1,
                reserved3 : 1,
                zero      : 1;
    u_int8_t    reserved5 : 7,
                reserved4 : 1;
} __packed;
#else
struct ieee80211_tsinfo_bitmap {
    u_int8_t    reserved1 : 1,
                tid       : 4,
                direction : 2,
                one       : 1;
    u_int8_t    zero      : 1,
                reserved3 : 1,
                psb       : 1,
                dot1Dtag  : 3,
                reserved2 : 2;
    u_int8_t    reserved4 : 1,
                reserved5 : 7;
}  __packed;
#endif

/*
 * WME/802.11e Tspec Element
 */
struct ieee80211_wme_tspec {
    u_int8_t    ts_id;
    u_int8_t    ts_len;
    u_int8_t    ts_oui[3];
    u_int8_t    ts_oui_type;
    u_int8_t    ts_oui_subtype;
    u_int8_t    ts_version;
    u_int8_t    ts_tsinfo[3];
    u_int8_t    ts_nom_msdu[2];
    u_int8_t    ts_max_msdu[2];
    u_int8_t    ts_min_svc[4];
    u_int8_t    ts_max_svc[4];
    u_int8_t    ts_inactv_intv[4];
    u_int8_t    ts_susp_intv[4];
    u_int8_t    ts_start_svc[4];
    u_int8_t    ts_min_rate[4];
    u_int8_t    ts_mean_rate[4];
    u_int8_t    ts_peak_rate[4];
    u_int8_t    ts_max_burst[4];
    u_int8_t    ts_delay[4];
    u_int8_t    ts_min_phy[4];
    u_int8_t    ts_surplus[2];
    u_int8_t    ts_medium_time[2];
} __packed;

/*
 * WME AC parameter field
 */
struct ieee80211_wme_acparams {
    u_int8_t    acp_aci_aifsn;
    u_int8_t    acp_logcwminmax;
    u_int16_t   acp_txop;
} __packed;

#define IEEE80211_WME_PARAM_LEN 24
#define WME_NUM_AC              4       /* 4 AC categories */

#define WME_PARAM_ACI           0x60    /* Mask for ACI field */
#define WME_PARAM_ACI_S         5       /* Shift for ACI field */
#define WME_PARAM_ACM           0x10    /* Mask for ACM bit */
#define WME_PARAM_ACM_S         4       /* Shift for ACM bit */
#define WME_PARAM_AIFSN         0x0f    /* Mask for aifsn field */
#define WME_PARAM_AIFSN_S       0       /* Shift for aifsn field */
#define WME_PARAM_LOGCWMIN      0x0f    /* Mask for CwMin field (in log) */
#define WME_PARAM_LOGCWMIN_S    0       /* Shift for CwMin field */
#define WME_PARAM_LOGCWMAX      0xf0    /* Mask for CwMax field (in log) */
#define WME_PARAM_LOGCWMAX_S    4       /* Shift for CwMax field */

#define WME_AC_TO_TID(_ac) (       \
    ((_ac) == WME_AC_VO) ? 6 : \
    ((_ac) == WME_AC_VI) ? 5 : \
    ((_ac) == WME_AC_BK) ? 1 : \
    0)

#define TID_TO_WME_AC(_tid) (      \
    (((_tid) == 0) || ((_tid) == 3)) ? WME_AC_BE : \
    (((_tid) == 1) || ((_tid) == 2)) ? WME_AC_BK : \
    (((_tid) == 4) || ((_tid) == 5)) ? WME_AC_VI : \
    WME_AC_VO)

/*
 * WME Parameter Element
 */
struct ieee80211_wme_param {
    u_int8_t                        param_id;
    u_int8_t                        param_len;
    u_int8_t                        param_oui[3];
    u_int8_t                        param_oui_type;
    u_int8_t                        param_oui_sybtype;
    u_int8_t                        param_version;
    u_int8_t                        param_qosInfo;
    u_int8_t                        param_reserved;
    struct ieee80211_wme_acparams   params_acParams[WME_NUM_AC];
} __packed;

/*
 * WME U-APSD qos info field defines
 */
#define WME_CAPINFO_UAPSD_EN                    0x00000080
#define WME_CAPINFO_UAPSD_VO                    0x00000001
#define WME_CAPINFO_UAPSD_VI                    0x00000002
#define WME_CAPINFO_UAPSD_BK                    0x00000004
#define WME_CAPINFO_UAPSD_BE                    0x00000008
#define WME_CAPINFO_UAPSD_ACFLAGS_SHIFT         0
#define WME_CAPINFO_UAPSD_ACFLAGS_MASK          0xF
#define WME_CAPINFO_UAPSD_MAXSP_SHIFT           5
#define WME_CAPINFO_UAPSD_MAXSP_MASK            0x3
#define WME_CAPINFO_IE_OFFSET                   8
#define WME_UAPSD_MAXSP(_qosinfo) (((_qosinfo) >> WME_CAPINFO_UAPSD_MAXSP_SHIFT) & WME_CAPINFO_UAPSD_MAXSP_MASK)
#define WME_UAPSD_AC_ENABLED(_ac, _qosinfo) ( (1<<(3 - (_ac))) &   \
        (((_qosinfo) >> WME_CAPINFO_UAPSD_ACFLAGS_SHIFT) & WME_CAPINFO_UAPSD_ACFLAGS_MASK) )

/* Mask used to determined whether all queues are UAPSD-enabled */
#define WME_CAPINFO_UAPSD_ALL                   (WME_CAPINFO_UAPSD_VO | \
                                                 WME_CAPINFO_UAPSD_VI | \
                                                 WME_CAPINFO_UAPSD_BK | \
                                                 WME_CAPINFO_UAPSD_BE)
#define WME_CAPINFO_UAPSD_NONE                  0

#define WME_UAPSD_AC_MAX_VAL		1
#define WME_UAPSD_AC_INVAL			WME_UAPSD_AC_MAX_VAL+1

/*
 * Atheros Advanced Capability information element.
 */
struct ieee80211_ie_athAdvCap {
    u_int8_t    athAdvCap_id;           /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    athAdvCap_len;          /* length in bytes */
    u_int8_t    athAdvCap_oui[3];       /* 0x00, 0x03, 0x7f */
    u_int8_t    athAdvCap_type;         /* OUI type */
    u_int16_t   athAdvCap_version;      /* spec revision */
    u_int8_t    athAdvCap_capability;   /* Capability info */
    u_int16_t   athAdvCap_defKeyIndex;
} __packed;

/*
 * Atheros Extended Capability information element.
 */
struct ieee80211_ie_ath_extcap {
    u_int8_t    ath_extcap_id;          /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    ath_extcap_len;         /* length in bytes */
    u_int8_t    ath_extcap_oui[3];      /* 0x00, 0x03, 0x7f */
    u_int8_t    ath_extcap_type;        /* OUI type */
    u_int8_t    ath_extcap_subtype;     /* OUI subtype */
    u_int8_t    ath_extcap_version;     /* spec revision */
    u_int32_t   ath_extcap_extcap              : 16,  /* B0-15  extended capabilities */
                ath_extcap_weptkipaggr_rxdelim : 8,   /* B16-23 num delimiters for receiving WEP/TKIP aggregates */
                ath_extcap_reserved            : 8;   /* B24-31 reserved */
} __packed;

/*
 * Atheros XR information element.
 */
struct ieee80211_xr_param {
    u_int8_t    param_id;
    u_int8_t    param_len;
    u_int8_t    param_oui[3];
    u_int8_t    param_oui_type;
    u_int8_t    param_oui_sybtype;
    u_int8_t    param_version;
    u_int8_t    param_Info;
    u_int8_t    param_base_bssid[IEEE80211_ADDR_LEN];
    u_int8_t    param_xr_bssid[IEEE80211_ADDR_LEN];
    u_int16_t   param_xr_beacon_interval;
    u_int8_t    param_base_ath_capability;
    u_int8_t    param_xr_ath_capability;
} __packed;

/*
 * SFA information element.
 */
struct ieee80211_ie_sfa {
    u_int8_t    sfa_id;         /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    sfa_len;        /* length in bytes */
    u_int8_t    sfa_oui[3];     /* 0x00, 0x40, 0x96 */
    u_int8_t    sfa_type;       /* OUI type */
    u_int8_t    sfa_caps;       /* Capabilities */
} __packed;

/* Atheros capabilities */
#define IEEE80211_ATHC_TURBOP   0x0001      /* Turbo Prime */
#define IEEE80211_ATHC_COMP     0x0002      /* Compression */
#define IEEE80211_ATHC_FF       0x0004      /* Fast Frames */
#define IEEE80211_ATHC_XR       0x0008      /* Xtended Range support */
#define IEEE80211_ATHC_AR       0x0010      /* Advanced Radar support */
#define IEEE80211_ATHC_BURST    0x0020      /* Bursting - not negotiated */
#define IEEE80211_ATHC_WME      0x0040      /* CWMin tuning */
#define IEEE80211_ATHC_BOOST    0x0080      /* Boost */
#define IEEE80211_ATHC_TDLS     0x0100      /* TDLS */

/* Atheros extended capabilities */
/* OWL device capable of WDS workaround */
#define IEEE80211_ATHEC_OWLWDSWAR        0x0001
#define IEEE80211_ATHEC_WEPTKIPAGGR	     0x0002
#define IEEE80211_ATHEC_EXTRADELIMWAR    0x0004
/*
 * Management Frames
 */

/*
 * *** Platform-specific code?? ***
 * In Vista one must use bit fields of type (unsigned short = u_int16_t) to
 * ensure data structure is of the correct size. ANSI C used to specify only
 * "int" bit fields, which led to a larger structure size in Windows (32 bits).
 *
 * We must make sure the following construction is valid in all OS's.
 */
union ieee80211_capability {
    struct {
        u_int16_t    ess                 : 1;
        u_int16_t    ibss                : 1;
        u_int16_t    cf_pollable         : 1;
        u_int16_t    cf_poll_request     : 1;
        u_int16_t    privacy             : 1;
        u_int16_t    short_preamble      : 1;
        u_int16_t    pbcc                : 1;
        u_int16_t    channel_agility     : 1;
        u_int16_t    spectrum_management : 1;
        u_int16_t    qos                 : 1;
        u_int16_t    short_slot_time     : 1;
        u_int16_t    apsd                : 1;
        u_int16_t    reserved2           : 1;
        u_int16_t    dsss_ofdm           : 1;
        u_int16_t    del_block_ack       : 1;
        u_int16_t    immed_block_ack     : 1;
    };

    u_int16_t   value;
} __packed;

struct ieee80211_beacon_frame {
    u_int8_t                      timestamp[8];    /* the value of sender's TSFTIMER */
    u_int16_t                     beacon_interval; /* the number of time units between target beacon transmission times */
    union ieee80211_capability    capability;
/* Value of capability for every bit
#define IEEE80211_CAPINFO_ESS               0x0001
#define IEEE80211_CAPINFO_IBSS              0x0002
#define IEEE80211_CAPINFO_CF_POLLABLE       0x0004
#define IEEE80211_CAPINFO_CF_POLLREQ        0x0008
#define IEEE80211_CAPINFO_PRIVACY           0x0010
#define IEEE80211_CAPINFO_SHORT_PREAMBLE    0x0020
#define IEEE80211_CAPINFO_PBCC              0x0040
#define IEEE80211_CAPINFO_CHNL_AGILITY      0x0080
#define IEEE80211_CAPINFO_SPECTRUM_MGMT     0x0100
#define IEEE80211_CAPINFO_QOS               0x0200
#define IEEE80211_CAPINFO_SHORT_SLOTTIME    0x0400
#define IEEE80211_CAPINFO_APSD              0x0800
#define IEEE80211_CAPINFO_RADIOMEAS         0x1000
#define IEEE80211_CAPINFO_DSSSOFDM          0x2000
bits 14-15 are reserved
*/
    struct ieee80211_ie_header    info_elements;
} __packed;

/*
 * Management Action Frames
 */

/* generic frame format */
struct ieee80211_action {
    u_int8_t    ia_category;
    u_int8_t    ia_action;
} __packed;

/* spectrum action frame header */
struct ieee80211_action_measrep_header {
    struct ieee80211_action action_header;
    u_int8_t                dialog_token;
} __packed;

/* categories */
#define IEEE80211_ACTION_CAT_SPECTRUM       0   /* Spectrum management */
#define IEEE80211_ACTION_CAT_QOS            1   /* IEEE QoS  */
#define IEEE80211_ACTION_CAT_DLS            2   /* DLS */
#define IEEE80211_ACTION_CAT_BA             3   /* BA */
#define IEEE80211_ACTION_CAT_PUBLIC         4   /* Public Action Frame */
#define IEEE80211_ACTION_CAT_HT             7   /* HT per IEEE802.11n-D1.06 */
#define IEEE80211_ACTION_CAT_SA_QUERY       8   /* SA Query per IEEE802.11w, PMF */
#define IEEE80211_ACTION_CAT_WMM_QOS       17   /* QoS from WMM specification */
#define IEEE80211_ACTION_CAT_VHT           21   /* VHT Action */

/* Spectrum Management actions */
#define IEEE80211_ACTION_MEAS_REQUEST       0   /* Measure channels */
#define IEEE80211_ACTION_MEAS_REPORT        1
#define IEEE80211_ACTION_TPC_REQUEST        2   /* Transmit Power control */
#define IEEE80211_ACTION_TPC_REPORT         3
#define IEEE80211_ACTION_CHAN_SWITCH        4   /* 802.11h Channel Switch Announcement */

/* HT actions */
#define IEEE80211_ACTION_HT_TXCHWIDTH       0   /* recommended transmission channel width */
#define IEEE80211_ACTION_HT_SMPOWERSAVE     1   /* Spatial Multiplexing (SM) Power Save */
#define IEEE80211_ACTION_HT_CSI             4   /* CSI Frame */
#define IEEE80211_ACTION_HT_NONCOMP_BF      5   /* Non-compressed Beamforming*/
#define IEEE80211_ACTION_HT_COMP_BF         6   /* Compressed Beamforming*/

/* VHT actions */
#define IEEE80211_ACTION_VHT_OPMODE         2  /* Operating  mode notification */

/* Spectrum channel switch action frame after IE*/
/* Public Actions*/
#define IEEE80211_ACTION_TDLS_DISCRESP  14      /* TDLS Discovery Response frame */

/* HT - recommended transmission channel width */
struct ieee80211_action_ht_txchwidth {
    struct ieee80211_action     at_header;
    u_int8_t                    at_chwidth;
} __packed;

#define IEEE80211_A_HT_TXCHWIDTH_20         0
#define IEEE80211_A_HT_TXCHWIDTH_2040       1

/* HT - Spatial Multiplexing (SM) Power Save */
struct ieee80211_action_ht_smpowersave {
    struct ieee80211_action     as_header;
    u_int8_t                    as_control;
} __packed;

/*HT - CSI Frame */     //for TxBF RC
#define MIMO_CONTROL_LEN 6
struct ieee80211_action_ht_CSI {
    struct ieee80211_action     as_header;
    u_int8_t                   mimo_control[MIMO_CONTROL_LEN];
} __packed;

/*HT - V/CV report frame*/
struct ieee80211_action_ht_txbf_rpt {
    struct ieee80211_action     as_header;
    u_int8_t                   mimo_control[MIMO_CONTROL_LEN];
} __packed;

/*
 * 802.11ac Operating Mode  Notification
 */
struct ieee80211_ie_op_mode {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int8_t rx_nss_type        : 1,
                 rx_nss             : 3,
                 reserved           : 2,
                 ch_width           : 2;
#else
        u_int8_t ch_width           : 2,
                 reserved           : 2,
                 rx_nss             : 3,
                 rx_nss_type        : 1;
#endif
} __packed;

struct ieee80211_ie_op_mode_ntfy {
        u_int8_t    elem_id;
        u_int8_t    elem_len;
        struct ieee80211_ie_op_mode opmode;
} __packed;


/* VHT - recommended Channel width and Nss */
struct ieee80211_action_vht_opmode {
    struct ieee80211_action     at_header;
    struct ieee80211_ie_op_mode at_op_mode;
} __packed;

/* values defined for 'as_control' field per 802.11n-D1.06 */
#define IEEE80211_A_HT_SMPOWERSAVE_DISABLED     0x00   /* SM Power Save Disabled, SM packets ok  */
#define IEEE80211_A_HT_SMPOWERSAVE_ENABLED      0x01   /* SM Power Save Enabled bit  */
#define IEEE80211_A_HT_SMPOWERSAVE_MODE         0x02   /* SM Power Save Mode bit */
#define IEEE80211_A_HT_SMPOWERSAVE_RESERVED     0xFC   /* SM Power Save Reserved bits */

/* values defined for SM Power Save Mode bit */
#define IEEE80211_A_HT_SMPOWERSAVE_STATIC       0x00   /* Static, SM packets not ok */
#define IEEE80211_A_HT_SMPOWERSAVE_DYNAMIC      0x02   /* Dynamic, SM packets ok if preceded by RTS */

/* DLS actions */
#define IEEE80211_ACTION_DLS_REQUEST            0
#define IEEE80211_ACTION_DLS_RESPONSE           1
#define IEEE80211_ACTION_DLS_TEARDOWN           2

struct ieee80211_dls_request {
	struct ieee80211_action hdr;
    u_int8_t dst_addr[IEEE80211_ADDR_LEN];
    u_int8_t src_addr[IEEE80211_ADDR_LEN];
    u_int16_t capa_info;
    u_int16_t timeout;
} __packed;

struct ieee80211_dls_response {
	struct ieee80211_action hdr;
    u_int16_t statuscode;
    u_int8_t dst_addr[IEEE80211_ADDR_LEN];
    u_int8_t src_addr[IEEE80211_ADDR_LEN];
} __packed;

/* BA actions */
#define IEEE80211_ACTION_BA_ADDBA_REQUEST       0   /* ADDBA request */
#define IEEE80211_ACTION_BA_ADDBA_RESPONSE      1   /* ADDBA response */
#define IEEE80211_ACTION_BA_DELBA               2   /* DELBA */

struct ieee80211_ba_parameterset {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   buffersize      : 10,   /* B6-15  buffer size */
                    tid             : 4,    /* B2-5   TID */
                    bapolicy        : 1,    /* B1   block ack policy */
                    amsdusupported  : 1;    /* B0   amsdu supported */
#else
        u_int16_t   amsdusupported  : 1,    /* B0   amsdu supported */
                    bapolicy        : 1,    /* B1   block ack policy */
                    tid             : 4,    /* B2-5   TID */
                    buffersize      : 10;   /* B6-15  buffer size */
#endif
} __packed;

#define  IEEE80211_BA_POLICY_DELAYED      0
#define  IEEE80211_BA_POLICY_IMMEDIATE    1
#define  IEEE80211_BA_AMSDU_SUPPORTED     1

struct ieee80211_ba_seqctrl {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   startseqnum     : 12,    /* B4-15  starting sequence number */
                    fragnum         : 4;     /* B0-3  fragment number */
#else
        u_int16_t   fragnum         : 4,     /* B0-3  fragment number */
                    startseqnum     : 12;    /* B4-15  starting sequence number */
#endif
} __packed;

struct ieee80211_delba_parameterset {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int16_t   tid             : 4,     /* B12-15  tid */
                    initiator       : 1,     /* B11     initiator */
                    reserved0       : 11;    /* B0-10   reserved */
#else
        u_int16_t   reserved0       : 11,    /* B0-10   reserved */
                    initiator       : 1,     /* B11     initiator */
                    tid             : 4;     /* B12-15  tid */
#endif
} __packed;

/* BA - ADDBA request */
struct ieee80211_action_ba_addbarequest {
    struct ieee80211_action             rq_header;
    u_int8_t                            rq_dialogtoken;
    struct ieee80211_ba_parameterset    rq_baparamset;
    u_int16_t                           rq_batimeout;   /* in TUs */
    struct ieee80211_ba_seqctrl         rq_basequencectrl;
} __packed;

/* BA - ADDBA response */
struct ieee80211_action_ba_addbaresponse {
    struct ieee80211_action             rs_header;
    u_int8_t                            rs_dialogtoken;
    u_int16_t                           rs_statuscode;
    struct ieee80211_ba_parameterset    rs_baparamset;
    u_int16_t                           rs_batimeout;   /* in TUs */
} __packed;

/* BA - DELBA */
struct ieee80211_action_ba_delba {
    struct ieee80211_action                dl_header;
    struct ieee80211_delba_parameterset    dl_delbaparamset;
    u_int16_t                              dl_reasoncode;
} __packed;

/* MGT Notif actions */
#define IEEE80211_WMM_QOS_ACTION_SETUP_REQ    0
#define IEEE80211_WMM_QOS_ACTION_SETUP_RESP   1
#define IEEE80211_WMM_QOS_ACTION_TEARDOWN     2

#define IEEE80211_WMM_QOS_DIALOG_TEARDOWN     0
#define IEEE80211_WMM_QOS_DIALOG_SETUP        1

#define IEEE80211_WMM_QOS_TSID_DATA_TSPEC     6
#define IEEE80211_WMM_QOS_TSID_SIG_TSPEC      7

struct ieee80211_action_wmm_qos {
    struct ieee80211_action             ts_header;
    u_int8_t                            ts_dialogtoken;
    u_int8_t                            ts_statuscode;
    struct ieee80211_wme_tspec          ts_tspecie;
} __packed;

/*
 * Control frames.
 */
struct ieee80211_frame_min {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_addr1[IEEE80211_ADDR_LEN];
    u_int8_t    i_addr2[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

/*
 * BAR frame format
 */
#define IEEE80211_BAR_CTL_TID_M     0xF000      /* tid mask             */
#define IEEE80211_BAR_CTL_TID_S         12      /* tid shift            */
#define IEEE80211_BAR_CTL_NOACK     0x0001      /* no-ack policy        */
#define IEEE80211_BAR_CTL_COMBA     0x0004      /* compressed block-ack */

/*
 * SA Query Action mgmt Frame
 */
struct ieee80211_action_sa_query {
    struct ieee80211_action     sa_header;
    u_int16_t                   sa_transId;
};

typedef enum ieee80211_action_sa_query_type{
    IEEE80211_ACTION_SA_QUERY_REQUEST,
    IEEE80211_ACTION_SA_QUERY_RESPONSE
}ieee80211_action_sa_query_type_t;

struct ieee80211_frame_bar {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_ra[IEEE80211_ADDR_LEN];
    u_int8_t    i_ta[IEEE80211_ADDR_LEN];
    u_int16_t   i_ctl;
    u_int16_t   i_seq;
    /* FCS */
} __packed;

struct ieee80211_frame_rts {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_ra[IEEE80211_ADDR_LEN];
    u_int8_t    i_ta[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

struct ieee80211_frame_cts {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_ra[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

struct ieee80211_frame_ack {
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];
    u_int8_t    i_ra[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

struct ieee80211_frame_pspoll {
    u_int8_t    i_fc[2];
    u_int8_t    i_aid[2];
    u_int8_t    i_bssid[IEEE80211_ADDR_LEN];
    u_int8_t    i_ta[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

struct ieee80211_frame_cfend {      /* NB: also CF-End+CF-Ack */
    u_int8_t    i_fc[2];
    u_int8_t    i_dur[2];   /* should be zero */
    u_int8_t    i_ra[IEEE80211_ADDR_LEN];
    u_int8_t    i_bssid[IEEE80211_ADDR_LEN];
    /* FCS */
} __packed;

/*
 * BEACON management packets
 *
 *  octet timestamp[8]
 *  octet beacon interval[2]
 *  octet capability information[2]
 *  information element
 *      octet elemid
 *      octet length
 *      octet information[length]
 */

typedef u_int8_t *ieee80211_mgt_beacon_t;

#define IEEE80211_BEACON_INTERVAL(beacon) \
    ((beacon)[8] | ((beacon)[9] << 8))
#define IEEE80211_BEACON_CAPABILITY(beacon) \
    ((beacon)[10] | ((beacon)[11] << 8))

#define IEEE80211_CAPINFO_ESS               0x0001
#define IEEE80211_CAPINFO_IBSS              0x0002
#define IEEE80211_CAPINFO_CF_POLLABLE       0x0004
#define IEEE80211_CAPINFO_CF_POLLREQ        0x0008
#define IEEE80211_CAPINFO_PRIVACY           0x0010
#define IEEE80211_CAPINFO_SHORT_PREAMBLE    0x0020
#define IEEE80211_CAPINFO_PBCC              0x0040
#define IEEE80211_CAPINFO_CHNL_AGILITY      0x0080
#define IEEE80211_CAPINFO_SPECTRUM_MGMT     0x0100
#define IEEE80211_CAPINFO_QOS               0x0200
#define IEEE80211_CAPINFO_SHORT_SLOTTIME    0x0400
#define IEEE80211_CAPINFO_APSD              0x0800
#define IEEE80211_CAPINFO_RADIOMEAS         0x1000
#define IEEE80211_CAPINFO_DSSSOFDM          0x2000
/* bits 14-15 are reserved */

/*
 * 802.11i/WPA information element (maximally sized).
 */
struct ieee80211_ie_wpa {
    u_int8_t    wpa_id;          /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    wpa_len;         /* length in bytes */
    u_int8_t    wpa_oui[3];      /* 0x00, 0x50, 0xf2 */
    u_int8_t    wpa_type;        /* OUI type */
    u_int16_t   wpa_version;     /* spec revision */
    u_int32_t   wpa_mcipher[1];  /* multicast/group key cipher */
    u_int16_t   wpa_uciphercnt;  /* # pairwise key ciphers */
    u_int32_t   wpa_uciphers[8]; /* ciphers */
    u_int16_t   wpa_authselcnt;  /* authentication selector cnt */
    u_int32_t   wpa_authsels[8]; /* selectors */
    u_int16_t   wpa_caps;        /* 802.11i capabilities */
    u_int16_t   wpa_pmkidcnt;    /* 802.11i pmkid count */
    u_int16_t   wpa_pmkids[8];   /* 802.11i pmkids */
} __packed;

#ifndef _BYTE_ORDER
#error "Don't know native byte order"
#endif

#ifndef IEEE80211N_IE
/* Temporary vendor specific IE for 11n pre-standard interoperability */
#define VENDOR_HT_OUI       0x00904c
#define VENDOR_HT_CAP_ID    51
#define VENDOR_HT_INFO_ID   52
#endif

#ifdef ATH_SUPPORT_TxBF
union ieee80211_hc_txbf {
    struct {
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int32_t   reserved              : 3,
                channel_estimation_cap    : 2,
                csi_max_rows_bfer         : 2,
                comp_bfer_antennas        : 2,
                noncomp_bfer_antennas     : 2,
                csi_bfer_antennas         : 2,
                minimal_grouping          : 2,
                explicit_comp_bf          : 2,
                explicit_noncomp_bf       : 2,
                explicit_csi_feedback     : 2,
                explicit_comp_steering    : 1,
                explicit_noncomp_steering : 1,
                explicit_csi_txbf_capable : 1,
                calibration               : 2,
                implicit_txbf_capable     : 1,
                tx_ndp_capable            : 1,
                rx_ndp_capable            : 1,
                tx_staggered_sounding     : 1,
                rx_staggered_sounding     : 1,
                implicit_rx_capable       : 1;
#else
        u_int32_t   implicit_rx_capable   : 1,
                rx_staggered_sounding     : 1,
                tx_staggered_sounding     : 1,
                rx_ndp_capable            : 1,
                tx_ndp_capable            : 1,
                implicit_txbf_capable     : 1,
                calibration               : 2,
                explicit_csi_txbf_capable : 1,
                explicit_noncomp_steering : 1,
                explicit_comp_steering    : 1,
                explicit_csi_feedback     : 2,
                explicit_noncomp_bf       : 2,
                explicit_comp_bf          : 2,
                minimal_grouping          : 2,
                csi_bfer_antennas         : 2,
                noncomp_bfer_antennas     : 2,
                comp_bfer_antennas        : 2,
                csi_max_rows_bfer         : 2,
                channel_estimation_cap    : 2,
                reserved                  : 3;
#endif
    };

    u_int32_t value;
} __packed;
#endif

struct ieee80211_ie_htcap_cmn {
    u_int16_t   hc_cap;         /* HT capabilities */
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t    hc_reserved     : 3,    /* B5-7 reserved */
                hc_mpdudensity  : 3,    /* B2-4 MPDU density (aka Minimum MPDU Start Spacing) */
                hc_maxampdu     : 2;    /* B0-1 maximum rx A-MPDU factor */
#else
    u_int8_t    hc_maxampdu     : 2,    /* B0-1 maximum rx A-MPDU factor */
                hc_mpdudensity  : 3,    /* B2-4 MPDU density (aka Minimum MPDU Start Spacing) */
                hc_reserved     : 3;    /* B5-7 reserved */
#endif
    u_int8_t    hc_mcsset[16];          /* supported MCS set */
    u_int16_t   hc_extcap;              /* extended HT capabilities */
#ifdef ATH_SUPPORT_TxBF
    union ieee80211_hc_txbf hc_txbf;    /* txbf capabilities */
#else
    u_int32_t   hc_txbf;                /* txbf capabilities */
#endif
    u_int8_t    hc_antenna;             /* antenna capabilities */
} __packed;

/*
 * 802.11n HT Capability IE
 */
struct ieee80211_ie_htcap {
    u_int8_t                         hc_id;      /* element ID */
    u_int8_t                         hc_len;     /* length in bytes */
    struct ieee80211_ie_htcap_cmn    hc_ie;
} __packed;

/*
 * Temporary vendor private HT Capability IE
 */
struct vendor_ie_htcap {
    u_int8_t                         hc_id;          /* element ID */
    u_int8_t                         hc_len;         /* length in bytes */
    u_int8_t                         hc_oui[3];
    u_int8_t                         hc_ouitype;
    struct ieee80211_ie_htcap_cmn    hc_ie;
} __packed;

/* HT capability flags */
#define IEEE80211_HTCAP_C_ADVCODING             0x0001
#define IEEE80211_HTCAP_C_CHWIDTH40             0x0002
#define IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC    0x0000 /* Capable of SM Power Save (Static) */
#define IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC   0x0004 /* Capable of SM Power Save (Dynamic) */
#define IEEE80211_HTCAP_C_SM_RESERVED           0x0008 /* Reserved */
#define IEEE80211_HTCAP_C_SM_ENABLED            0x000c /* SM enabled, no SM Power Save */
#define IEEE80211_HTCAP_C_GREENFIELD            0x0010
#define IEEE80211_HTCAP_C_SHORTGI20             0x0020
#define IEEE80211_HTCAP_C_SHORTGI40             0x0040
#define IEEE80211_HTCAP_C_TXSTBC                0x0080
#define IEEE80211_HTCAP_C_TXSTBC_S                   7
#define IEEE80211_HTCAP_C_RXSTBC                0x0300  /* 2 bits */
#define IEEE80211_HTCAP_C_RXSTBC_S                   8
#define IEEE80211_HTCAP_C_DELAYEDBLKACK         0x0400
#define IEEE80211_HTCAP_C_MAXAMSDUSIZE          0x0800  /* 1 = 8K, 0 = 3839B */
#define IEEE80211_HTCAP_C_DSSSCCK40             0x1000
#define IEEE80211_HTCAP_C_PSMP                  0x2000
#define IEEE80211_HTCAP_C_INTOLERANT40          0x4000
#define IEEE80211_HTCAP_C_LSIGTXOPPROT          0x8000

#define IEEE80211_HTCAP_C_SM_MASK               0x000c /* Spatial Multiplexing (SM) capabitlity bitmask */

/* B0-1 maximum rx A-MPDU factor 2^(13+Max Rx A-MPDU Factor) */
enum {
    IEEE80211_HTCAP_MAXRXAMPDU_8192,    /* 2 ^ 13 */
    IEEE80211_HTCAP_MAXRXAMPDU_16384,   /* 2 ^ 14 */
    IEEE80211_HTCAP_MAXRXAMPDU_32768,   /* 2 ^ 15 */
    IEEE80211_HTCAP_MAXRXAMPDU_65536,   /* 2 ^ 16 */
};
#define IEEE80211_HTCAP_MAXRXAMPDU_FACTOR   13

/* B2-4 MPDU density (usec) */
enum {
    IEEE80211_HTCAP_MPDUDENSITY_NA,     /* No time restriction */
    IEEE80211_HTCAP_MPDUDENSITY_0_25,   /* 1/4 usec */
    IEEE80211_HTCAP_MPDUDENSITY_0_5,    /* 1/2 usec */
    IEEE80211_HTCAP_MPDUDENSITY_1,      /* 1 usec */
    IEEE80211_HTCAP_MPDUDENSITY_2,      /* 2 usec */
    IEEE80211_HTCAP_MPDUDENSITY_4,      /* 4 usec */
    IEEE80211_HTCAP_MPDUDENSITY_8,      /* 8 usec */
    IEEE80211_HTCAP_MPDUDENSITY_16,     /* 16 usec */
};

/* HT extended capability flags */
#define IEEE80211_HTCAP_EXTC_PCO                0x0001
#define IEEE80211_HTCAP_EXTC_TRANS_TIME_RSVD    0x0000
#define IEEE80211_HTCAP_EXTC_TRANS_TIME_400     0x0002 /* 20-40 switch time */
#define IEEE80211_HTCAP_EXTC_TRANS_TIME_1500    0x0004 /* in us             */
#define IEEE80211_HTCAP_EXTC_TRANS_TIME_5000    0x0006
#define IEEE80211_HTCAP_EXTC_RSVD_1             0x00f8
#define IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_NONE  0x0000
#define IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_RSVD  0x0100
#define IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_UNSOL 0x0200
#define IEEE80211_HTCAP_EXTC_MCS_FEEDBACK_FULL  0x0300
#define IEEE80211_HTCAP_EXTC_RSVD_2             0xfc00
#ifdef ATH_SUPPORT_TxBF
#define IEEE80211_HTCAP_EXTC_HTC_SUPPORT        0x0400
#endif

struct ieee80211_ie_htinfo_cmn {
    u_int8_t    hi_ctrlchannel;     /* control channel */
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t    hi_serviceinterval    : 3,    /* B5-7 svc interval granularity */
                hi_ctrlaccess         : 1,    /* B4   controlled access only */
                hi_rifsmode           : 1,    /* B3   rifs mode */
                hi_txchwidth          : 1,    /* B2   recommended xmiss width set */
                hi_extchoff           : 2;    /* B0-1 extension channel offset */


/*

 * The following 2 consecutive bytes are defined in word in 80211n spec.

 * Some processors store MSB byte into lower memory address which causes wrong

 * wrong byte sequence in beacon. Thus we break into byte definition which should

 * avoid the problem for all processors

 */

    u_int8_t    hi_reserved3          : 3,    /* B5-7 reserved */

                hi_obssnonhtpresent   : 1,    /* B4   OBSS non-HT STA present */

                hi_txburstlimit       : 1,    /* B3   transmit burst limit */

                hi_nongfpresent       : 1,    /* B2   non greenfield devices present */

                hi_opmode             : 2;    /* B0-1 operating mode */

    u_int8_t    hi_reserved0             ;    /* B0-7 (B8-15 in 11n) reserved */



/* The following 2 consecutive bytes are defined in word in 80211n spec. */

    u_int8_t    hi_dualctsprot        : 1,    /* B7   dual CTS protection */
                hi_dualbeacon         : 1,    /* B6   dual beacon */
                hi_reserved2          : 6;    /* B0-5 reserved */
    u_int8_t    hi_reserved1          : 4,    /* B4-7 (B12-15 in 11n) reserved */
                hi_pcophase           : 1,    /* B3   (B11 in 11n)  pco phase */
                hi_pcoactive          : 1,    /* B2   (B10 in 11n)  pco active */
                hi_lsigtxopprot       : 1,    /* B1   (B9 in 11n)   l-sig txop protection full support */
                hi_stbcbeacon         : 1;    /* B0   (B8 in 11n)   STBC beacon */
#else
    u_int8_t    hi_extchoff           : 2,    /* B0-1 extension channel offset */
                hi_txchwidth          : 1,    /* B2   recommended xmiss width set */
                hi_rifsmode           : 1,    /* B3   rifs mode */
                hi_ctrlaccess         : 1,    /* B4   controlled access only */
                hi_serviceinterval    : 3;    /* B5-7 svc interval granularity */
    u_int16_t   hi_opmode             : 2,    /* B0-1 operating mode */
                hi_nongfpresent       : 1,    /* B2   non greenfield devices present */
                hi_txburstlimit       : 1,    /* B3   transmit burst limit */
                hi_obssnonhtpresent   : 1,    /* B4   OBSS non-HT STA present */
                hi_reserved0          : 11;   /* B5-15 reserved */
    u_int16_t   hi_reserved2          : 6,    /* B0-5 reserved */
                hi_dualbeacon         : 1,    /* B6   dual beacon */
                hi_dualctsprot        : 1,    /* B7   dual CTS protection */
                hi_stbcbeacon         : 1,    /* B8   STBC beacon */
                hi_lsigtxopprot       : 1,    /* B9   l-sig txop protection full support */
                hi_pcoactive          : 1,    /* B10  pco active */
                hi_pcophase           : 1,    /* B11  pco phase */
                hi_reserved1          : 4;    /* B12-15 reserved */
#endif
    u_int8_t    hi_basicmcsset[16];     /* basic MCS set */
} __packed;

/*
 * 802.11n HT Information IE
 */
struct ieee80211_ie_htinfo {
    u_int8_t                        hi_id;          /* element ID */
    u_int8_t                        hi_len;         /* length in bytes */
    struct ieee80211_ie_htinfo_cmn  hi_ie;
} __packed;

/*
 * Temporary vendor private HT Information IE
 */
struct vendor_ie_htinfo {
    u_int8_t                        hi_id;          /* element ID */
    u_int8_t                        hi_len;         /* length in bytes */
    u_int8_t                        hi_oui[3];
    u_int8_t                        hi_ouitype;
    struct ieee80211_ie_htinfo_cmn  hi_ie;
} __packed;

/* extension channel offset (2 bit signed number) */
enum {
    IEEE80211_HTINFO_EXTOFFSET_NA    = 0,   /* 0  no extension channel is present */
    IEEE80211_HTINFO_EXTOFFSET_ABOVE = 1,   /* +1 extension channel above control channel */
    IEEE80211_HTINFO_EXTOFFSET_UNDEF = 2,   /* -2 undefined */
    IEEE80211_HTINFO_EXTOFFSET_BELOW = 3    /* -1 extension channel below control channel*/
};

/* recommended transmission width set */
enum {
    IEEE80211_HTINFO_TXWIDTH_20,
    IEEE80211_HTINFO_TXWIDTH_2040
};

/* operating flags */
#define IEEE80211_HTINFO_OPMODE_PURE                0x00 /* no protection */
#define IEEE80211_HTINFO_OPMODE_MIXED_PROT_OPT      0x01 /* prot optional (legacy device maybe present) */
#define IEEE80211_HTINFO_OPMODE_MIXED_PROT_40       0x02 /* prot required (20 MHz) */
#define IEEE80211_HTINFO_OPMODE_MIXED_PROT_ALL      0x03 /* prot required (legacy devices present) */
#define IEEE80211_HTINFO_OPMODE_NON_GF_PRESENT      0x04 /* non-greenfield devices present */

#define IEEE80211_HTINFO_OPMODE_MASK                0x03 /* For protection 0x00-0x03 */

/* Non-greenfield STAs present */
enum {
    IEEE80211_HTINFO_NON_GF_NOT_PRESENT,    /* Non-greenfield STAs not present */
    IEEE80211_HTINFO_NON_GF_PRESENT,        /* Non-greenfield STAs present */
};

/* Transmit Burst Limit */
enum {
    IEEE80211_HTINFO_TXBURST_UNLIMITED,     /* Transmit Burst is unlimited */
    IEEE80211_HTINFO_TXBURST_LIMITED,       /* Transmit Burst is limited */
};

/* OBSS Non-HT STAs present */
enum {
    IEEE80211_HTINFO_OBSS_NONHT_NOT_PRESENT, /* OBSS Non-HT STAs not present */
    IEEE80211_HTINFO_OBSS_NONHT_PRESENT,     /* OBSS Non-HT STAs present */
};

/* misc flags */
#define IEEE80211_HTINFO_DUALBEACON               0x0040 /* B6   dual beacon */
#define IEEE80211_HTINFO_DUALCTSPROT              0x0080 /* B7   dual stbc protection */
#define IEEE80211_HTINFO_STBCBEACON               0x0100 /* B8   secondary beacon */
#define IEEE80211_HTINFO_LSIGTXOPPROT             0x0200 /* B9   lsig txop prot full support */
#define IEEE80211_HTINFO_PCOACTIVE                0x0400 /* B10  pco active */
#define IEEE80211_HTINFO_PCOPHASE                 0x0800 /* B11  pco phase */

/* Secondary Channel offset for for 40MHz direct link */
#define IEEE80211_SECONDARY_CHANNEL_ABOVE         1
#define IEEE80211_SECONDARY_CHANNEL_BELOW         3

#define IEEE80211_TDLS_CHAN_SX_PROHIBIT         0x00000002 /* bit-2 TDLS Channel Switch Prohibit */

/* RIFS mode */
enum {
    IEEE80211_HTINFO_RIFSMODE_PROHIBITED,   /* use of rifs prohibited */
    IEEE80211_HTINFO_RIFSMODE_ALLOWED,      /* use of rifs permitted */
};

/*
 * Management information element payloads.
 */
enum {
    IEEE80211_ELEMID_SSID             = 0,
    IEEE80211_ELEMID_RATES            = 1,
    IEEE80211_ELEMID_FHPARMS          = 2,
    IEEE80211_ELEMID_DSPARMS          = 3,
    IEEE80211_ELEMID_CFPARMS          = 4,
    IEEE80211_ELEMID_TIM              = 5,
    IEEE80211_ELEMID_IBSSPARMS        = 6,
    IEEE80211_ELEMID_COUNTRY          = 7,
    IEEE80211_ELEMID_REQINFO          = 10,
    IEEE80211_ELEMID_QBSS_LOAD        = 11,
    IEEE80211_ELEMID_TCLAS            = 14,
    IEEE80211_ELEMID_CHALLENGE        = 16,
    /* 17-31 reserved for challenge text extension */
    IEEE80211_ELEMID_PWRCNSTR         = 32,
    IEEE80211_ELEMID_PWRCAP           = 33,
    IEEE80211_ELEMID_TPCREQ           = 34,
    IEEE80211_ELEMID_TPCREP           = 35,
    IEEE80211_ELEMID_SUPPCHAN         = 36,
    IEEE80211_ELEMID_CHANSWITCHANN    = 37,
    IEEE80211_ELEMID_MEASREQ          = 38,
    IEEE80211_ELEMID_MEASREP          = 39,
    IEEE80211_ELEMID_QUIET            = 40,
    IEEE80211_ELEMID_IBSSDFS          = 41,
    IEEE80211_ELEMID_ERP              = 42,
    IEEE80211_ELEMID_TCLAS_PROCESS    = 44,
    IEEE80211_ELEMID_HTCAP_ANA        = 45,
    IEEE80211_ELEMID_RESERVED_47      = 47,
    IEEE80211_ELEMID_RSN              = 48,
    IEEE80211_ELEMID_XRATES           = 50,
    IEEE80211_ELEMID_HTCAP            = 51,
    IEEE80211_ELEMID_HTINFO           = 52,
    IEEE80211_ELEMID_MOBILITY_DOMAIN  = 54,
    IEEE80211_ELEMID_FT               = 55,
    IEEE80211_ELEMID_TIMEOUT_INTERVAL = 56,
    IEEE80211_ELEMID_EXTCHANSWITCHANN = 60,
    IEEE80211_ELEMID_HTINFO_ANA       = 61,
    IEEE80211_ELEMID_SECCHANOFFSET    = 62,
	IEEE80211_ELEMID_WAPI		      = 68,   /*IE for WAPI*/
    IEEE80211_ELEMID_TIME_ADVERTISEMENT = 69,
    IEEE80211_ELEMID_RRM              = 70,   /* Radio resource measurement */
    IEEE80211_ELEMID_2040_COEXT       = 72,
    IEEE80211_ELEMID_2040_INTOL       = 73,
    IEEE80211_ELEMID_OBSS_SCAN        = 74,
    IEEE80211_ELEMID_MMIE             = 76,   /* 802.11w Management MIC IE */
    IEEE80211_ELEMID_FMS_DESCRIPTOR   = 86,   /* 802.11v FMS descriptor IE */
    IEEE80211_ELEMID_FMS_REQUEST      = 87,   /* 802.11v FMS request IE */
    IEEE80211_ELEMID_FMS_RESPONSE     = 88,   /* 802.11v FMS response IE */
    IEEE80211_ELEMID_BSSMAX_IDLE_PERIOD = 90, /* BSS MAX IDLE PERIOD */
    IEEE80211_ELEMID_TFS_REQUEST      = 91,
    IEEE80211_ELEMID_TFS_RESPONSE     = 92,
    IEEE80211_ELEMID_TIM_BCAST_REQUEST  = 94,
    IEEE80211_ELEMID_TIM_BCAST_RESPONSE = 95,
    IEEE80211_ELEMID_INTERWORKING     = 107,
    IEEE80211_ELEMID_XCAPS            = 127,
    IEEE80211_ELEMID_RESERVED_133     = 133,
    IEEE80211_ELEMID_TPC              = 150,
    IEEE80211_ELEMID_CCKM             = 156,
    IEEE80211_ELEMID_VHTCAP           = 191,  /* VHT Capabilities */
    IEEE80211_ELEMID_VHTOP            = 192,  /* VHT Operation */
    IEEE80211_ELEMID_EXT_BSS_LOAD     = 193,  /* Extended BSS Load */
    IEEE80211_ELEMID_WIDE_BAND_CHAN_SWITCH = 194,  /* Wide Band Channel Switch */
    IEEE80211_ELEMID_VHT_TX_PWR_ENVLP = 195,  /* VHT Transmit Power Envelope */
    IEEE80211_ELEMID_CHAN_SWITCH_WRAP = 196,  /* Channel Switch Wrapper */
    IEEE80211_ELEMID_AID              = 197,  /* AID */
    IEEE80211_ELEMID_QUIET_CHANNEL    = 198,  /* Quiet Channel */
    IEEE80211_ELEMID_OP_MODE_NOTIFY   = 199,  /* Operating Mode Notification */
    IEEE80211_ELEMID_VENDOR           = 221,  /* vendor private */
};

#define IEEE80211_MAX_IE_LEN                255
#define IEEE80211_RSN_IE_LEN                22

#define IEEE80211_CHANSWITCHANN_BYTES        5
#define IEEE80211_EXTCHANSWITCHANN_BYTES     6

//TODO -> Need to Check Redefinition Error used in only UMAC
#if 0
struct ieee80211_tim_ie {
    u_int8_t    tim_ie;         /* IEEE80211_ELEMID_TIM */
    u_int8_t    tim_len;
    u_int8_t    tim_count;      /* DTIM count */
    u_int8_t    tim_period;     /* DTIM period */
    u_int8_t    tim_bitctl;     /* bitmap control */
    u_int8_t    tim_bitmap[1];  /* variable-length bitmap */
} __packed;
#endif

/* Country IE channel triplet */
struct country_ie_triplet {
    union{
        u_int8_t schan;             /* starting channel */
        u_int8_t regextid;          /* Regulatory Extension Identifier */
    };
    union{
        u_int8_t nchan;             /* number of channels */
        u_int8_t regclass;          /* Regulatory Class */
    };
    union{
        u_int8_t maxtxpwr;          /* tx power  */
        u_int8_t coverageclass;     /* Coverage Class */
    };
}__packed;

struct ieee80211_country_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_COUNTRY */
    u_int8_t    len;
    u_int8_t    cc[3];              /* ISO CC+(I)ndoor/(O)utdoor */
    struct country_ie_triplet triplet[1];
} __packed;

struct ieee80211_fh_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_FHPARMS */
    u_int8_t    len;
    u_int16_t   dwell_time;    // endianess??
    u_int8_t    hop_set;
    u_int8_t    hop_pattern;
    u_int8_t    hop_index;
} __packed;

struct ieee80211_ds_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_DSPARMS */
    u_int8_t    len;
    u_int8_t    current_channel;
} __packed;

struct ieee80211_erp_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_ERP */
    u_int8_t    len;
    u_int8_t    value;
} __packed;

//TODO -> Need to Check Redefinition Error used in only UMAC
#if 0
struct ieee80211_quiet_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_QUIET */
    u_int8_t    len;
    u_int8_t    tbttcount;          /* quiet start */
    u_int8_t    period;             /* beacon intervals between quiets*/
    u_int16_t   duration;           /* TUs of each quiet*/
    u_int16_t   offset;             /* TUs of from TBTT of quiet start*/
} __packed;
#endif

struct ieee80211_channelswitch_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_CHANSWITCHANN */
    u_int8_t    len;
    u_int8_t    switchmode;
    u_int8_t    newchannel;
    u_int8_t    tbttcount;
} __packed;

/* channel switch action frame format definition */
struct ieee80211_action_spectrum_channel_switch {
    struct ieee80211_action             csa_header;
    struct ieee80211_channelswitch_ie   csa_element;
}__packed;

struct ieee80211_extendedchannelswitch_ie {
    u_int8_t    ie;                 /* IEEE80211_ELEMID_EXTCHANSWITCHANN */
    u_int8_t    len;
    u_int8_t    switchmode;
    u_int8_t    newClass;
    u_int8_t    newchannel;
    u_int8_t    tbttcount;
} __packed;

struct ieee80211_tpc_ie {
    u_int8_t    ie;
    u_int8_t    len;
    u_int8_t    pwrlimit;
} __packed;

/*
 * MHDRIE included in TKIP MFP protected management frames
 */
struct ieee80211_ese_mhdr_ie {
    u_int8_t    mhdr_id;
    u_int8_t    mhdr_len;
    u_int8_t    mhdr_oui[3];
    u_int8_t    mhdr_oui_type;
    u_int8_t    mhdr_fc[2];
    u_int8_t    mhdr_bssid[6];
} __packed;

/*
 * SSID IE
 */
struct ieee80211_ie_ssid {
    u_int8_t    ssid_id;
    u_int8_t    ssid_len;
    u_int8_t    ssid[32];
} __packed;

/*
 * Supported rates
 */
#define IEEE80211_MAX_SUPPORTED_RATES      8

struct ieee80211_ie_rates {
    u_int8_t    rate_id;     /* Element Id */
    u_int8_t    rate_len;    /* IE Length */
    u_int8_t    rate[IEEE80211_MAX_SUPPORTED_RATES];     /* IE Length */
} __packed;

/*
 * Extended rates
 */
#define IEEE80211_MAX_EXTENDED_RATES     256

struct ieee80211_ie_xrates {
    u_int8_t    xrate_id;     /* Element Id */
    u_int8_t    xrate_len;    /* IE Length */
    u_int8_t    xrate[IEEE80211_MAX_EXTENDED_RATES];   /* IE Length */
} __packed;

/*
 * WPS SSID list information element (maximally sized).
 */
struct ieee80211_ie_ssidl {
    u_int8_t    ssidl_id;     /* IEEE80211_ELEMID_VENDOR */
    u_int8_t    ssidl_len;    /* length in bytes */
    u_int8_t    ssidl_oui[3]; /* 0x00, 0x50, 0xf2 */
    u_int8_t    ssidl_type;   /* OUI type */
    u_int8_t    ssidl_prim_cap; /* Primary capabilities */
    u_int8_t    ssidl_count;  /* # of secondary SSIDs */
    u_int16_t   ssidl_value[248];
} __packed;

#if _BYTE_ORDER == _BIG_ENDIAN
struct ieee80211_sec_ssid_cap {
    u_int32_t       reserved0   :1,
                    akmlist     :6,
                    reserved1   :4,
                    reeserved2  :2,
                    ucipher     :15,
                    mcipher     :4;
};
#else
struct ieee80211_sec_ssid_cap {
    u_int32_t       mcipher     :4,
                    ucipher     :15,
                    reserved2   :2,
                    reserved1   :4,
                    akmlist     :6,
                    reserved0   :1;
};
#endif

struct ieee80211_ie_qbssload {
    u_int8_t     elem_id;                /* IEEE80211_ELEMID_QBSS_LOAD */
    u_int8_t     length;                 /* length in bytes */
    u_int16_t    station_count;          /* number of station associated */
    u_int8_t     channel_utilization;    /* channel busy time in 0-255 scale */
    u_int16_t    aac;                    /* available admission capacity */
} __packed;

#define SEC_SSID_HEADER_LEN  6
#define SSIDL_IE_HEADER_LEN  6

struct ieee80211_sec_ssid {
    u_int8_t                        sec_ext_cap;
    struct ieee80211_sec_ssid_cap   sec_cap;
    u_int8_t                        sec_ssid_len;
    u_int8_t                        sec_ssid[32];
} __packed;

/* Definitions of SSIDL IE */
enum {
    CAP_MCIPHER_ENUM_NONE = 0,
    CAP_MCIPHER_ENUM_WEP40,
    CAP_MCIPHER_ENUM_WEP104,
    CAP_MCIPHER_ENUM_TKIP,
    CAP_MCIPHER_ENUM_CCMP,
    CAP_MCIPHER_ENUM_CKIP_CMIC,
    CAP_MCIPHER_ENUM_CKIP,
    CAP_MCIPHER_ENUM_CMIC
};


#define CAP_UCIPHER_BIT_NONE           0x0001
#define CAP_UCIPHER_BIT_WEP40          0x0002
#define CAP_UCIPHER_BIT_WEP104         0x0004
#define CAP_UCIPHER_BIT_TKIP           0x0008
#define CAP_UCIPHER_BIT_CCMP           0x0010
#define CAP_UCIPHER_BIT_CKIP_CMIC      0x0020
#define CAP_UCIPHER_BIT_CKIP           0x0040
#define CAP_UCIPHER_BIT_CMIC           0x0080
#define CAP_UCIPHER_BIT_WPA2_WEP40     0x0100
#define CAP_UCIPHER_BIT_WPA2_WEP104    0x0200
#define CAP_UCIPHER_BIT_WPA2_TKIP      0x0400
#define CAP_UCIPHER_BIT_WPA2_CCMP      0x0800
#define CAP_UCIPHER_BIT_WPA2_CKIP_CMIC 0x1000
#define CAP_UCIPHER_BIT_WPA2_CKIP      0x2000
#define CAP_UCIPHER_BIT_WPA2_CMIC      0x4000

#define CAP_AKM_BIT_WPA1_1X            0x01
#define CAP_AKM_BIT_WPA1_PSK           0x02
#define CAP_AKM_BIT_WPA2_1X            0x04
#define CAP_AKM_BIT_WPA2_PSK           0x08
#define CAP_AKM_BIT_WPA1_CCKM          0x10
#define CAP_AKM_BIT_WPA2_CCKM          0x20

#define IEEE80211_CHALLENGE_LEN         128

#define IEEE80211_SUPPCHAN_LEN          26

#define IEEE80211_RATE_BASIC            0x80
#define IEEE80211_RATE_VAL              0x7f

/* EPR information element flags */
#define IEEE80211_ERP_NON_ERP_PRESENT   0x01
#define IEEE80211_ERP_USE_PROTECTION    0x02
#define IEEE80211_ERP_LONG_PREAMBLE     0x04

/* Atheros private advanced capabilities info */
#define ATHEROS_CAP_TURBO_PRIME         0x01
#define ATHEROS_CAP_COMPRESSION         0x02
#define ATHEROS_CAP_FAST_FRAME          0x04
/* bits 3-6 reserved */
#define ATHEROS_CAP_BOOST               0x80

#define ATH_OUI                     0x7f0300    /* Atheros OUI */
#define ATH_OUI_TYPE                    0x01
#define ATH_OUI_SUBTYPE                 0x01
#define ATH_OUI_VERSION                 0x00
#define ATH_OUI_TYPE_XR                 0x03
#define ATH_OUI_VER_XR                  0x01
#define ATH_OUI_EXTCAP_TYPE             0x04    /* Atheros Extended Cap Type */
#define ATH_OUI_EXTCAP_SUBTYPE          0x01    /* Atheros Extended Cap Sub-type */
#define ATH_OUI_EXTCAP_VERSION          0x00    /* Atheros Extended Cap Version */

#define WPA_OUI                     0xf25000
#define WPA_VERSION                        1    /* current supported version */
#define CSCO_OUI                    0x964000    /* Cisco OUI */
#define AOW_OUI                     0x4a0100    /* AoW OUI, workaround */
#define AOW_OUI_TYPE                    0x01
#define AOW_OUI_VERSION                 0x01

#define WSC_OUI                   0x0050f204

#define WPA_CSE_NULL                    0x00
#define WPA_CSE_WEP40                   0x01
#define WPA_CSE_TKIP                    0x02
#define WPA_CSE_CCMP                    0x04
#define WPA_CSE_WEP104                  0x05

#define WPA_ASE_NONE                    0x00
#define WPA_ASE_8021X_UNSPEC            0x01
#define WPA_ASE_8021X_PSK               0x02
#define WPA_ASE_FT_IEEE8021X            0x20
#define WPA_ASE_FT_PSK                  0x40
#define WPA_ASE_SHA256_IEEE8021X        0x80
#define WPA_ASE_SHA256_PSK              0x100
#define WPA_ASE_WPS                     0x200


#define RSN_OUI                     0xac0f00
#define RSN_VERSION                        1    /* current supported version */

#define RSN_CSE_NULL                    0x00
#define RSN_CSE_WEP40                   0x01
#define RSN_CSE_TKIP                    0x02
#define RSN_CSE_WRAP                    0x03
#define RSN_CSE_CCMP                    0x04
#define RSN_CSE_WEP104                  0x05
#define RSN_CSE_AES_CMAC                0x06

#define RSN_ASE_NONE                    0x00
#define RSN_ASE_8021X_UNSPEC            0x01
#define RSN_ASE_8021X_PSK               0x02
#define RSN_ASE_FT_IEEE8021X            0x20
#define RSN_ASE_FT_PSK                  0x40
#define RSN_ASE_SHA256_IEEE8021X        0x80
#define RSN_ASE_SHA256_PSK              0x100
#define RSN_ASE_WPS                     0x200

#define AKM_SUITE_TYPE_IEEE8021X        0x01
#define AKM_SUITE_TYPE_PSK              0x02
#define AKM_SUITE_TYPE_FT_IEEE8021X     0x03
#define AKM_SUITE_TYPE_FT_PSK           0x04
#define AKM_SUITE_TYPE_SHA256_IEEE8021X 0x05
#define AKM_SUITE_TYPE_SHA256_PSK       0x06

#define RSN_CAP_PREAUTH                 0x01
#define RSN_CAP_PTKSA_REPLAYCOUNTER    0x0c
#define RSN_CAP_GTKSA_REPLAYCOUNTER    0x30
#define RSN_CAP_MFP_REQUIRED            0x40
#define RSN_CAP_MFP_ENABLED             0x80

#define CCKM_OUI                    0x964000
#define CCKM_ASE_UNSPEC                    0
#define WPA_CCKM_AKM              0x00964000
#define RSN_CCKM_AKM              0x00964000

#define WME_OUI                     0xf25000
#define WME_OUI_TYPE                    0x02
#define WME_INFO_OUI_SUBTYPE            0x00
#define WME_PARAM_OUI_SUBTYPE           0x01
#define WME_TSPEC_OUI_SUBTYPE           0x02

#define WME_PARAM_OUI_VERSION              1
#define WME_TSPEC_OUI_VERSION              1
#define WME_VERSION                        1

/* WME stream classes */
#define WME_AC_BE                          0    /* best effort */
#define WME_AC_BK                          1    /* background */
#define WME_AC_VI                          2    /* video */
#define WME_AC_VO                          3    /* voice */

/* WCN IE */
#define WCN_OUI                     0xf25000    /* Microsoft OUI */
#define WCN_OUI_TYPE                    0x04    /* WCN */

/* Atheros htoui  for ht vender ie; use Epigram OUI for compatibility with pre11n devices */
#define ATH_HTOUI                   0x00904c

#define SFA_OUI                     0x964000
#define SFA_OUI_TYPE                    0x14
#define SFA_IE_CAP_MFP                  0x01
#define SFA_IE_CAP_DIAG_CHANNEL         0x02
#define SFA_IE_CAP_LOCATION_SVCS        0x04
#define SFA_IE_CAP_EXP_BANDWIDTH        0x08

#define WPA_OUI_BYTES       0x00, 0x50, 0xf2
#define RSN_OUI_BYTES       0x00, 0x0f, 0xac
#define WME_OUI_BYTES       0x00, 0x50, 0xf2
#define ATH_OUI_BYTES       0x00, 0x03, 0x7f
#define SFA_OUI_BYTES       0x00, 0x40, 0x96
#define CCKM_OUI_BYTES      0x00, 0x40, 0x96
#define WPA_SEL(x)          (((x)<<24)|WPA_OUI)
#define RSN_SEL(x)          (((x)<<24)|RSN_OUI)
#define SFA_SEL(x)          (((x)<<24)|SFA_OUI)
#define CCKM_SEL(x)         (((x)<<24)|CCKM_OUI)

#define IEEE80211_RV(v)     ((v) & IEEE80211_RATE_VAL)
#define IEEE80211_N(a)      (sizeof(a) / sizeof(a[0]))

/*
 * AUTH management packets
 *
 *  octet algo[2]
 *  octet seq[2]
 *  octet status[2]
 *  octet chal.id
 *  octet chal.length
 *  octet chal.text[253]
 */

typedef u_int8_t *ieee80211_mgt_auth_t;

#define IEEE80211_AUTH_ALGORITHM(auth) \
    ((auth)[0] | ((auth)[1] << 8))
#define IEEE80211_AUTH_TRANSACTION(auth) \
    ((auth)[2] | ((auth)[3] << 8))
#define IEEE80211_AUTH_STATUS(auth) \
    ((auth)[4] | ((auth)[5] << 8))

#define IEEE80211_AUTH_ALG_OPEN     0x0000
#define IEEE80211_AUTH_ALG_SHARED   0x0001
#define IEEE80211_AUTH_ALG_FT       0x0002
#define IEEE80211_AUTH_ALG_LEAP     0x0080

enum {
    IEEE80211_AUTH_OPEN_REQUEST     = 1,
    IEEE80211_AUTH_OPEN_RESPONSE    = 2,
};

enum {
    IEEE80211_AUTH_SHARED_REQUEST   = 1,
    IEEE80211_AUTH_SHARED_CHALLENGE = 2,
    IEEE80211_AUTH_SHARED_RESPONSE  = 3,
    IEEE80211_AUTH_SHARED_PASS      = 4,
};

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */

enum {
    IEEE80211_REASON_UNSPECIFIED        = 1,
    IEEE80211_REASON_AUTH_EXPIRE        = 2,
    IEEE80211_REASON_AUTH_LEAVE         = 3,
    IEEE80211_REASON_ASSOC_EXPIRE       = 4,
    IEEE80211_REASON_ASSOC_TOOMANY      = 5,
    IEEE80211_REASON_NOT_AUTHED         = 6,
    IEEE80211_REASON_NOT_ASSOCED        = 7,
    IEEE80211_REASON_ASSOC_LEAVE        = 8,
    IEEE80211_REASON_ASSOC_NOT_AUTHED   = 9,

    IEEE80211_REASON_RSN_REQUIRED       = 11,
    IEEE80211_REASON_RSN_INCONSISTENT   = 12,
    IEEE80211_REASON_IE_INVALID         = 13,
    IEEE80211_REASON_MIC_FAILURE        = 14,

    IEEE80211_REASON_QOS                = 32,
    IEEE80211_REASON_QOS_BANDWITDH      = 33,
    IEEE80211_REASON_QOS_CH_CONDITIONS  = 34,
    IEEE80211_REASON_QOS_TXOP           = 35,
    IEEE80211_REASON_QOS_LEAVE          = 36,
    IEEE80211_REASON_QOS_DECLINED       = 37,
    IEEE80211_REASON_QOS_SETUP_REQUIRED = 38,
    IEEE80211_REASON_QOS_TIMEOUT        = 39,
    IEEE80211_REASON_QOS_CIPHER         = 45,

    IEEE80211_STATUS_SUCCESS            = 0,
    IEEE80211_STATUS_UNSPECIFIED        = 1,
    IEEE80211_STATUS_CAPINFO            = 10,
    IEEE80211_STATUS_NOT_ASSOCED        = 11,
    IEEE80211_STATUS_OTHER              = 12,
    IEEE80211_STATUS_ALG                = 13,
    IEEE80211_STATUS_SEQUENCE           = 14,
    IEEE80211_STATUS_CHALLENGE          = 15,
    IEEE80211_STATUS_TIMEOUT            = 16,
    IEEE80211_STATUS_TOOMANY            = 17,
    IEEE80211_STATUS_BASIC_RATE         = 18,
    IEEE80211_STATUS_SP_REQUIRED        = 19,
    IEEE80211_STATUS_PBCC_REQUIRED      = 20,
    IEEE80211_STATUS_CA_REQUIRED        = 21,
    IEEE80211_STATUS_TOO_MANY_STATIONS  = 22,
    IEEE80211_STATUS_RATES              = 23,
    IEEE80211_STATUS_SHORTSLOT_REQUIRED = 25,
    IEEE80211_STATUS_DSSSOFDM_REQUIRED  = 26,
    IEEE80211_STATUS_NO_HT              = 27,
    IEEE80211_STATUS_REJECT_TEMP        = 30,
    IEEE80211_STATUS_MFP_VIOLATION      = 31,
    IEEE80211_STATUS_REFUSED            = 37,
    IEEE80211_STATUS_INVALID_PARAM      = 38,

    IEEE80211_STATUS_DLS_NOT_ALLOWED    = 48,
};

/* private IEEE80211_STATUS */
#define    IEEE80211_STATUS_CANCEL             -1
#define    IEEE80211_STATUS_INVALID_IE         -2
#define    IEEE80211_STATUS_INVALID_CHANNEL    -3

#define IEEE80211_WEP_KEYLEN        5   /* 40bit */
#define IEEE80211_WEP_IVLEN         3   /* 24bit */
#define IEEE80211_WEP_KIDLEN        1   /* 1 octet */
#define IEEE80211_WEP_CRCLEN        4   /* CRC-32 */
#define IEEE80211_WEP_NKID          4   /* number of key ids */

/*
 * 802.11i defines an extended IV for use with non-WEP ciphers.
 * When the EXTIV bit is set in the key id byte an additional
 * 4 bytes immediately follow the IV for TKIP.  For CCMP the
 * EXTIV bit is likewise set but the 8 bytes represent the
 * CCMP header rather than IV+extended-IV.
 */
#define IEEE80211_WEP_EXTIV      0x20
#define IEEE80211_WEP_EXTIVLEN      4   /* extended IV length */
#define IEEE80211_WEP_MICLEN        8   /* trailing MIC */

#define IEEE80211_CCMP_HEADERLEN    8
#define IEEE80211_CCMP_MICLEN       8

/*
 * 802.11w defines a MMIE chunk to be attached at the end of
 * any outgoing broadcast or multicast robust management frame.
 * MMIE field is total 18 bytes in size. Following the diagram of MMIE
 *
 *        <------------ 18 Bytes MMIE ----------------------->
 *        +--------+---------+---------+-----------+---------+
 *        |Element | Length  | Key id  |   IPN     |  MIC    |
 *        |  id    |         |         |           |         |
 *        +--------+---------+---------+-----------+---------+
 * bytes      1         1         2         6            8
 *
 */
#define IEEE80211_MMIE_LEN          18
#define IEEE80211_MMIE_ELEMENTIDLEN 1
#define IEEE80211_MMIE_LENGTHLEN    1
#define IEEE80211_MMIE_KEYIDLEN     2
#define IEEE80211_MMIE_IPNLEN       6
#define IEEE80211_MMIE_MICLEN       8

#define IEEE80211_CRC_LEN           4

#define IEEE80211_8021Q_HEADER_LEN  4
/*
 * Maximum acceptable MTU is:
 *  IEEE80211_MAX_LEN - WEP overhead - CRC -
 *      QoS overhead - RSN/WPA overhead
 * Min is arbitrarily chosen > IEEE80211_MIN_LEN.  The default
 * mtu is Ethernet-compatible; it's set by ether_ifattach.
 */
#define IEEE80211_MTU_MAX       2290
#define IEEE80211_MTU_MIN       32

/* Rather than using this default value, customer platforms can provide a custom value for this constant.
  Coustomer platform will use the different define value by themself */
#ifndef IEEE80211_MAX_MPDU_LEN
#define IEEE80211_MAX_MPDU_LEN      (3840 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))
#endif
#define IEEE80211_ACK_LEN \
    (sizeof(struct ieee80211_frame_ack) + IEEE80211_CRC_LEN)
#define IEEE80211_MIN_LEN \
    (sizeof(struct ieee80211_frame_min) + IEEE80211_CRC_LEN)

/* An 802.11 data frame can be one of three types:
1. An unaggregated frame: The maximum length of an unaggregated data frame is 2324 bytes + headers.
2. A data frame that is part of an AMPDU: The maximum length of an AMPDU may be upto 65535 bytes, but data frame is limited to 2324 bytes + header.
3. An AMSDU: The maximum length of an AMSDU is eihther 3839 or 7095 bytes.
The maximum frame length supported by hardware is 4095 bytes.
A length of 3839 bytes is chosen here to support unaggregated data frames, any size AMPDUs and 3839 byte AMSDUs.
*/
#define IEEE80211N_MAX_FRAMELEN  3839
#define IEEE80211N_MAX_LEN (IEEE80211N_MAX_FRAMELEN + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

#define IEEE80211_TX_CHAINMASK_MIN  1
#define IEEE80211_TX_CHAINMASK_MAX  7

#define IEEE80211_RX_CHAINMASK_MIN  1
#define IEEE80211_RX_CHAINMASK_MAX  7

/*
 * The 802.11 spec says at most 2007 stations may be
 * associated at once.  For most AP's this is way more
 * than is feasible so we use a default of 128.  This
 * number may be overridden by the driver and/or by
 * user configuration.
 */
#define IEEE80211_AID_MAX       2007
#define IEEE80211_AID_DEF       128

#define IEEE80211_AID(b)    ((b) &~ 0xc000)

/*
 * RTS frame length parameters.  The default is specified in
 * the 802.11 spec.  The max may be wrong for jumbo frames.
 */
#define IEEE80211_RTS_DEFAULT       512
#define IEEE80211_RTS_MIN           0
#define IEEE80211_RTS_MAX           2347

/*
 * Fragmentation limits
 */
#define IEEE80211_FRAGMT_THRESHOLD_MIN        540      /* min frag threshold */
#define IEEE80211_FRAGMT_THRESHOLD_MAX       2346      /* max frag threshold */

/*
 * Regulatory extention identifier for country IE.
 */
#define IEEE80211_REG_EXT_ID        201

/*
 * overlapping BSS
 */
#define IEEE80211_OBSS_SCAN_PASSIVE_DWELL_DEF  20
#define IEEE80211_OBSS_SCAN_ACTIVE_DWELL_DEF   10
#define IEEE80211_OBSS_SCAN_INTERVAL_DEF       300
#define IEEE80211_OBSS_SCAN_PASSIVE_TOTAL_DEF  200
#define IEEE80211_OBSS_SCAN_ACTIVE_TOTAL_DEF   20
#define IEEE80211_OBSS_SCAN_THRESH_DEF   25
#define IEEE80211_OBSS_SCAN_DELAY_DEF   5

/*
 * overlapping BSS scan ie
 */
struct ieee80211_ie_obss_scan {
        u_int8_t elem_id;
        u_int8_t elem_len;
        u_int16_t scan_passive_dwell;
        u_int16_t scan_active_dwell;
        u_int16_t scan_interval;
        u_int16_t scan_passive_total;
        u_int16_t scan_active_total;
        u_int16_t scan_delay;
        u_int16_t scan_thresh;
} __packed;

/*
 * Extended capability ie
 */
struct ieee80211_ie_ext_cap {
        u_int8_t elem_id;
        u_int8_t elem_len;
        u_int32_t ext_capflags;
        u_int32_t ext_capflags2;
} __packed;

/* Extended capability IE flags */
#define IEEE80211_EXTCAPIE_2040COEXTMGMT        0x00000001
#define IEEE80211_EXTCAPIE_TFS                  0x00010000
#define IEEE80211_EXTCAPIE_FMS                  0x00000800
#define IEEE80211_EXTCAPIE_WNMSLEEPMODE         0x00020000
#define IEEE80211_EXTCAPIE_TIMBROADCAST         0x00040000
#define IEEE80211_EXTCAPIE_PROXYARP             0x00001000
#define IEEE80211_EXTCAPIE_BSSTRANSITION        0x00080000
/* Tunneled Direct Link Setup (TDLS) extended capability bits */
#define IEEE80211_EXTCAPIE_PEER_UAPSD_BUF_STA   0x10000000
#define IEEE80211_EXTCAPIE_TDLS_PEER_PSM        0x20000000
#define IEEE80211_EXTCAPIE_TDLS_CHAN_SX         0x40000000
/* 2nd Extended capability IE flags bit32-bit63*/
#define IEEE80211_EXTCAPIE_TDLSSUPPORT      0x00000020 /* bit-37 TDLS Support */
#define IEEE80211_EXTCAPIE_TDLSPROHIBIT     0x00000040 /* bit-38 TDLS Prohibit Support */
#define IEEE80211_EXTCAPIE_TDLSCHANSXPROHIBIT   0x00000080 /* bit-39 TDLS Channel Switch Prohibit */
#define IEEE80211_EXTCAPIE_TDLS_WIDE_BAND   0x20000080 /* bit-61 TDLS Wide Bandwidth support */
#define IEEE80211_EXTCAPIE_OP_MODE_NOTIFY   0x40000000 /* bit-62 Operating Mode notification */

/*
 * These caps are populated when we recieve beacon/probe response
 * This is used to maintain local TDLS cap bit masks
 */

#define IEEE80211_TDLS_PROHIBIT     0x00000001 /* bit-1 TDLS Prohibit Support */

/*
 * 20/40 BSS coexistence ie
 */
struct ieee80211_ie_bss_coex {
        u_int8_t elem_id;
        u_int8_t elem_len;
#if _BYTE_ORDER == _BIG_ENDIAN
        u_int8_t reserved1          : 1,
                 reserved2          : 1,
                 reserved3          : 1,
                 obss_exempt_grant  : 1,
                 obss_exempt_req    : 1,
                 ht20_width_req       : 1,
                 ht40_intolerant      : 1,
                 inf_request        : 1;
#else
        u_int8_t inf_request        : 1,
                 ht40_intolerant      : 1,
                 ht20_width_req       : 1,
                 obss_exempt_req    : 1,
                 obss_exempt_grant  : 1,
                 reserved3          : 1,
                 reserved2          : 1,
                 reserved1          : 1;
#endif
} __packed;

/*
 * 20/40 BSS intolerant channel report ie
 */
struct ieee80211_ie_intolerant_report {
        u_int8_t elem_id;
        u_int8_t elem_len;
        u_int8_t reg_class;
        u_int8_t chan_list[1];          /* variable-length channel list */
} __packed;

/*
 * 20/40 coext management action frame
 */
struct ieee80211_action_bss_coex_frame {
        struct ieee80211_action                ac_header;
        struct ieee80211_ie_bss_coex           coex;
        struct ieee80211_ie_intolerant_report    chan_report;
} __packed;

typedef enum ieee80211_tie_interval_type{
    IEEE80211_TIE_INTERVAL_TYPE_RESERVED                  = 0,
    IEEE80211_TIE_INTERVAL_TYPE_REASSOC_DEADLINE_INTERVAL = 1,
    IEEE80211_TIE_INTERVAL_TYPE_KEY_LIFETIME_INTERVAL     = 2,
    IEEE80211_TIE_INTERVAL_TYPE_ASSOC_COMEBACK_TIME       = 3,
}ieee80211_tie_interval_type_t;

struct ieee80211_ie_timeout_interval {
        u_int8_t elem_id;
        u_int8_t elem_len;
        u_int8_t interval_type;
        u_int32_t value;
} __packed;

//TODO -> Need to Check Redefinition Error used in only UMAC
#if 0
/* Management MIC information element (IEEE 802.11w) */
struct ieee80211_mmie {
    u_int8_t element_id;
    u_int8_t length;
    u_int16_t key_id;
    u_int8_t sequence_number[6];
    u_int8_t mic[8];
} __packed;
#endif

/* VHT capability flags */
/* B0-B1 Maximum MPDU Length */
#define IEEE80211_VHTCAP_MAX_MPDU_LEN_3839     0x00000000 /* A-MSDU Length 3839 octets */
#define IEEE80211_VHTCAP_MAX_MPDU_LEN_7935     0x00000001 /* A-MSDU Length 7991 octets */
#define IEEE80211_VHTCAP_MAX_MPDU_LEN_11454    0x00000002 /* A-MSDU Length 11454 octets */

/* B2-B3 Supported Channel Width */
#define IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80     0x00000000 /* Does not support 160 or 80+80 */
#define IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160    0x00000004 /* Supports 160 */
#define IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160 0x00000008 /* Support both 160 or 80+80 */
#define IEEE80211_VHTCAP_SUP_CHAN_WIDTH_S      2          /* B2-B3 */

#define IEEE80211_VHTCAP_RX_LDPC             0x00000010 /* B4 RX LDPC */
#define IEEE80211_VHTCAP_SHORTGI_80          0x00000020 /* B5 Short GI for 80MHz */
#define IEEE80211_VHTCAP_SHORTGI_160         0x00000040 /* B6 Short GI for 160 and 80+80 MHz */
#define IEEE80211_VHTCAP_TX_STBC             0x00000080 /* B7 Tx STBC */
#define IEEE80211_VHTCAP_TX_STBC_S           7

#define IEEE80211_VHTCAP_RX_STBC             0x00000700 /* B8-B10 Rx STBC */
#define IEEE80211_VHTCAP_RX_STBC_S           8

#define IEEE80211_VHTCAP_SU_BFORMER          0x00000800 /* B11 SU Beam former capable */
#define IEEE80211_VHTCAP_SU_BFORMEE          0x00001000 /* B12 SU Beam formee capable */
#define IEEE80211_VHTCAP_BF_MAX_ANT          0x0000E000 /* B13-B15 Compressed steering number of
                                                         * beacomformer Antennas supported */
#define IEEE80211_VHTCAP_BF_MAX_ANT_S        13

#define IEEE80211_VHTCAP_SOUND_DIMENSIONS    0x00070000 /* B16-B18 Sounding Dimensions */
#define IEEE80211_VHTCAP_SOUND_DIMENSIONS_S  16

#define IEEE80211_VHTCAP_MU_BFORMER          0x00080000 /* B19 MU Beam Former */
#define IEEE80211_VHTCAP_MU_BFORMEE          0x00100000 /* B20 MU Beam Formee */
#define IEEE80211_VHTCAP_TXOP_PS             0x00200000 /* B21 VHT TXOP PS */
#define IEEE80211_VHTCAP_PLUS_HTC_VHT        0x00400000 /* B22 +HTC-VHT capable */

#define IEEE80211_VHTCAP_MAX_AMPDU_LEN_FACTOR  13
#define IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP   0x03800000 /* B23-B25 maximum AMPDU Length Exponent */
#define IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP_S 23

#define IEEE80211_VHTCAP_LINK_ADAPT          0x0C000000 /* B26-B27 VHT Link Adaptation capable */
#define IEEE80211_VHTCAP_RESERVED            0xF0000000 /* B28-B31 Reserved */

/*
 * 802.11ac VHT Capability IE
 */
struct ieee80211_ie_vhtcap {
        u_int8_t    elem_id;
        u_int8_t    elem_len;
        u_int32_t   vht_cap_info;
        u_int16_t   rx_mcs_map;          /* B0-B15 Max Rx MCS for each SS */
        u_int16_t   rx_high_data_rate;   /* B16-B28 Max Rx data rate,
                                            Note:  B29-B31 reserved */
        u_int16_t   tx_mcs_map;          /* B32-B47 Max Tx MCS for each SS */
        u_int16_t   tx_high_data_rate;   /* B48-B60 Max Tx data rate,
                                            Note: B61-B63 reserved */
} __packed;


/* VHT Operation  */
#define IEEE80211_VHTOP_CHWIDTH_2040      0 /* 20/40 MHz Operating Channel */
#define IEEE80211_VHTOP_CHWIDTH_80        1 /* 80 MHz Operating Channel */
#define IEEE80211_VHTOP_CHWIDTH_160       2 /* 160 MHz Operating Channel */
#define IEEE80211_VHTOP_CHWIDTH_80_80     3 /* 80 + 80 MHz Operating Channel */

/*
 * 802.11ac VHT Operation IE
 */
struct ieee80211_ie_vhtop {
        u_int8_t    elem_id;
        u_int8_t    elem_len;
        u_int8_t    vht_op_chwidth;              /* BSS Operational Channel width */
        u_int8_t    vht_op_ch_freq_seg1;         /* Channel Center frequency */
        u_int8_t    vht_op_ch_freq_seg2;         /* Channel Center frequency applicable
                                                  * for 80+80MHz mode of operation */
        u_int16_t   vhtop_basic_mcs_set;         /* Basic MCS set */
} __packed;

/*
 * 802.11n Secondary Channel Offset element
 */
#define IEEE80211_SEC_CHAN_OFFSET_SCN               0 /* no secondary channel */
#define IEEE80211_SEC_CHAN_OFFSET_SCA               1 /* secondary channel above */
#define IEEE80211_SEC_CHAN_OFFSET_SCB               3 /* secondary channel below */

struct ieee80211_ie_sec_chan_offset {
     u_int8_t    elem_id;
     u_int8_t    len;
     u_int8_t    sec_chan_offset;
} __packed;

/*
 * 802.11ac Transmit Power Envelope element
 */
#define IEEE80211_VHT_TXPWR_IS_SUB_ELEMENT          1  /* It checks whether its  sub element */
#define IEEE80211_VHT_TXPWR_MAX_POWER_COUNT         4 /* Max TX power elements valid */
#define IEEE80211_VHT_TXPWR_NUM_POWER_SUPPORTED     3 /* Max TX power elements supported */
#define IEEE80211_VHT_TXPWR_LCL_MAX_PWR_UNITS_SHFT  3 /* B3-B5 Local Max transmit power units */

struct ieee80211_ie_vht_txpwr_env {
        u_int8_t    elem_id;
        u_int8_t    elem_len;
        u_int8_t    txpwr_info;       /* Transmit Power Information */
        u_int8_t    local_max_txpwr[4]; /* Local Max TxPower for 20,40,80,160MHz */
} __packed;

/*
 * 802.11ac Wide Bandwidth Channel Switch Element
 */

#define IEEE80211_VHT_EXTCH_SWITCH             1  /* For extension channel switch */
#define CHWIDTH_VHT20                          20  /* Channel width 20 */
#define CHWIDTH_VHT40                          40  /* Channel width 40 */
#define CHWIDTH_VHT80                          80  /* Channel width 80 */
#define CHWIDTH_VHT160                         160  /* Channel width 160 */

struct ieee80211_ie_wide_bw_switch {
        u_int8_t    elem_id;
        u_int8_t    elem_len;
        u_int8_t    new_ch_width;       /* New channel width */
        u_int8_t    new_ch_freq_seg1;   /* Channel Center frequency 1 */
        u_int8_t    new_ch_freq_seg2;   /* Channel Center frequency 2 */
} __packed;

#define IEEE80211_RSSI_RX       0x00000001
#define IEEE80211_RSSI_TX       0x00000002
#define IEEE80211_RSSI_EXTCHAN  0x00000004
#define IEEE80211_RSSI_BEACON   0x00000008
#define IEEE80211_RSSI_RXDATA   0x00000010

#define IEEE80211_RATE_TX 0
#define IEEE80211_RATE_RX 1
#define IEEE80211_LASTRATE_TX 2
#define IEEE80211_LASTRATE_RX 3
#define IEEE80211_RATECODE_TX 4
#define IEEE80211_RATECODE_RX 5

#define IEEE80211_MAX_RATE_PER_CLIENT 8
/* Define for the P2P Wildcard SSID */
#define IEEE80211_P2P_WILDCARD_SSID         "DIRECT-"

#define IEEE80211_P2P_WILDCARD_SSID_LEN     (sizeof(IEEE80211_P2P_WILDCARD_SSID) - 1)

#endif /* _COMMON_IEEE80211_H_ */
