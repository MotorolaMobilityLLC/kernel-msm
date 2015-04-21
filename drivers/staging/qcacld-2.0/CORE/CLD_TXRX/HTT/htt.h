/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/**
 * @file htt.h
 *
 * @details the public header file of HTT layer
 */

#ifndef _HTT_H_
#define _HTT_H_

#include <a_types.h>    /* A_UINT32 */
#include <a_osapi.h>    /* PREPACK, POSTPACK */
#ifdef ATHR_WIN_NWF
#pragma warning( disable:4214 ) //bit field types other than int
#endif
#include "wlan_defs.h"
#include "htt_common.h"

#ifndef offsetof
#define offsetof(type, field)   ((unsigned int)(&((type *)0)->field))
#endif

/*
 * HTT version history:
 * 1.0 initial numbered version
 * 1.1 modifications to STATS messages.
 *     These modifications are not backwards compatible, but since the
 *     STATS messages themselves are non-essential (they are for debugging),
 *     the 1.1 version of the HTT message library as a whole is compatible
 *     with the 1.0 version.
 * 1.2 reset mask IE added to STATS_REQ message
 * 1.3 stat config IE added to STATS_REQ message
 *----
 * 2.0 FW rx PPDU desc added to RX_IND message
 * 2.1 Enable msdu_ext/frag_desc banking change for WIFI2.0
 *----
 * 3.0 Remove HTT_H2T_MSG_TYPE_MGMT_TX message
 * 3.1 Added HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND message
 * 3.2 Added HTT_H2T_MSG_TYPE_WDI_IPA_CFG,
 *           HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQUEST messages
 * 3.3 Added HTT_H2T_MSG_TYPE_AGGR_CFG_EX message
 * 3.4 Added tx_compl_req flag in HTT tx descriptor
 */
#define HTT_CURRENT_VERSION_MAJOR 3
#define HTT_CURRENT_VERSION_MINOR 4

#define HTT_NUM_TX_FRAG_DESC  1024

#define HTT_WIFI_IP_VERSION(x,y) ((x) == (y))

#define HTT_CHECK_SET_VAL(field, val) \
    A_ASSERT(!((val) & ~((field ## _M) >> (field ## _S))))

/*
 * htt_dbg_stats_type -
 * bit positions for each stats type within a stats type bitmask
 * The bitmask contains 24 bits.
 */
enum htt_dbg_stats_type {
    HTT_DBG_STATS_WAL_PDEV_TXRX      = 0, /* bit 0 -> 0x1 */
    HTT_DBG_STATS_RX_REORDER         = 1, /* bit 1 -> 0x2 */
    HTT_DBG_STATS_RX_RATE_INFO       = 2, /* bit 2 -> 0x4 */
    HTT_DBG_STATS_TX_PPDU_LOG        = 3, /* bit 3 -> 0x8 */
    HTT_DBG_STATS_TX_RATE_INFO       = 4, /* bit 4 -> 0x10 */
    /* bits 5-23 currently reserved */

    /* keep this last */
    HTT_DBG_NUM_STATS
};

/*=== host -> target messages ===============================================*/

enum htt_h2t_msg_type {
    HTT_H2T_MSG_TYPE_VERSION_REQ        = 0x0,
    HTT_H2T_MSG_TYPE_TX_FRM             = 0x1,
    HTT_H2T_MSG_TYPE_RX_RING_CFG        = 0x2,
    HTT_H2T_MSG_TYPE_STATS_REQ          = 0x3,
    HTT_H2T_MSG_TYPE_SYNC               = 0x4,
    HTT_H2T_MSG_TYPE_AGGR_CFG           = 0x5,
    HTT_H2T_MSG_TYPE_FRAG_DESC_BANK_CFG = 0x6,
    DEPRECATED_HTT_H2T_MSG_TYPE_MGMT_TX = 0x7, /* no longer used */
    HTT_H2T_MSG_TYPE_WDI_IPA_CFG        = 0x8,
    HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ     = 0x9,
    HTT_H2T_MSG_TYPE_AGGR_CFG_EX        = 0xa, /* per vdev amsdu subfrm limit */
    /* keep this last */
    HTT_H2T_NUM_MSGS
};

/*
 * HTT host to target message type -
 * stored in bits 7:0 of the first word of the message
 */
#define HTT_H2T_MSG_TYPE_M      0xff
#define HTT_H2T_MSG_TYPE_S      0

#define HTT_H2T_MSG_TYPE_SET(word, msg_type)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_MSG_TYPE, msg_type); \
        (word) |= ((msg_type) << HTT_H2T_MSG_TYPE_S);  \
    } while (0)
#define HTT_H2T_MSG_TYPE_GET(word) \
    (((word) & HTT_H2T_MSG_TYPE_M) >> HTT_H2T_MSG_TYPE_S)

/**
 * @brief target -> host version number request message definition
 *
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |                     reserved                     |    msg type    |
 *     |-------------------------------------------------------------------|
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a version number request message
 *     Value: 0x0
 */

#define HTT_VER_REQ_BYTES 4

/* TBDXXX: figure out a reasonable number */
#define HTT_HL_DATA_SVC_PIPE_DEPTH         24
#define HTT_LL_DATA_SVC_PIPE_DEPTH         64

/**
 * @brief HTT tx MSDU descriptor
 *
 * @details
 *  The HTT tx MSDU descriptor is created by the host HTT SW for each
 *  tx MSDU.  The HTT tx MSDU descriptor contains the information that
 *  the target firmware needs for the FW's tx processing, particularly
 *  for creating the HW msdu descriptor.
 *  The same HTT tx descriptor is used for HL and LL systems, though
 *  a few fields within the tx descriptor are used only by LL or
 *  only by HL.
 *  The HTT tx descriptor is defined in two manners: by a struct with
 *  bitfields, and by a series of [dword offset, bit mask, bit shift]
 *  definitions.
 *  The target should use the struct def, for simplicitly and clarity,
 *  but the host shall use the bit-mast + bit-shift defs, to be endian-
 *  neutral.  Specifically, the host shall use the get/set macros built
 *  around the mask + shift defs.
 */
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_80211_HDR_S   0
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_80211_HDR_M   0x1
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_AGGR_S     1
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_AGGR_M     0x2
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_ENCRYPT_S  2
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_ENCRYPT_M  0x4
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_CLASSIFY_S 3
#define HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_CLASSIFY_M 0x8
PREPACK struct htt_tx_msdu_desc_t
{
    /* DWORD 0: flags and meta-data */
    A_UINT32
        msg_type: 8, /* HTT_H2T_MSG_TYPE_TX_FRM */

        /* pkt_subtype -
         * Detailed specification of the tx frame contents, extending the
         * general specification provided by pkt_type.
         * FIX THIS: ADD COMPLETE SPECS FOR THIS FIELDS VALUE, e.g.
         *     pkt_type    | pkt_subtype
         *     ==============================================================
         *     802.3       | n/a
         *     ------------+-------------------------------------------------
         *     native WiFi | n/a
         *     ------------+-------------------------------------------------
         *     mgmt        | 0x0 - 802.11 MAC header absent
         *                 | 0x1 - 802.11 MAC header present
         *     ------------+-------------------------------------------------
         *     raw         | bit 0: 0x0 - 802.11 MAC header absent
         *                 |        0x1 - 802.11 MAC header present
         *                 | bit 1: 0x0 - allow aggregation
         *                 |        0x1 - don't allow aggregation
         *                 | bit 2: 0x0 - perform encryption
         *                 |        0x1 - don't perform encryption
         *                 | bit 3: 0x0 - perform tx classification / queuing
         *                 |        0x1 - don't perform tx classification;
         *                 |              insert the frame into the "misc"
         *                 |              tx queue
         *                 | bit 4: reserved
         */
        pkt_subtype: 5,

        /* pkt_type -
         * General specification of the tx frame contents.
         * The htt_pkt_type enum should be used to specify and check the
         * value of this field.
         */
        pkt_type: 3,

        /* vdev_id -
         * ID for the vdev that is sending this tx frame.
         * For certain non-standard packet types, e.g. pkt_type == raw
         * and (pkt_subtype >> 3) == 1, this field is not relevant/valid.
         * This field is used primarily for determining where to queue
         * broadcast and multicast frames.
         */
#define HTT_TX_VDEV_ID_WORD 0
#define HTT_TX_VDEV_ID_MASK 0x3f
#define HTT_TX_VDEV_ID_SHIFT 16
        vdev_id: 6,

        /* ext_tid -
         * The extended traffic ID.
         * If the TID is unknown, the extended TID is set to
         * HTT_TX_EXT_TID_INVALID.
         * If the tx frame is QoS data, then the extended TID has the 0-15
         * value of the QoS TID.
         * If the tx frame is non-QoS data, then the extended TID is set to
         * HTT_TX_EXT_TID_NON_QOS.
         * If the tx frame is multicast or broadcast, then the extended TID
         * is set to HTT_TX_EXT_TID_MCAST_BCAST.
         */
        ext_tid: 5,

        /* postponed -
         * This flag indicates whether the tx frame has been downloaded to
         * the target before but discarded by the target, and now is being
         * downloaded again; or if this is a new frame that is being
         * downloaded for the first time.
         * This flag allows the target to determine the correct order for
         * transmitting new vs. old frames.
         * value: 0 -> new frame, 1 -> re-send of a previously sent frame
         * This flag only applies to HL systems, since in LL systems,
         * the tx flow control is handled entirely within the target.
         */
        postponed: 1,

        reserved_dword0_bits28: 1, /* unused */

        /* cksum_offload -
         * This flag indicates whether checksum offload is enabled or not
         * for this frame. Target FW use this flag to turn on HW checksumming
         *  0x0 - No checksum offload
         *  0x1 - L3 header checksum only
         *  0x2 - L4 checksum only
         *  0x3 - L3 header checksum + L4 checksum
         */
        cksum_offload: 2,

        #define HTT_TX_L3_CKSUM_OFFLOAD      1
        #define HTT_TX_L4_CKSUM_OFFLOAD      2

        /* tx_comp_req -
         * This flag indicates whether Tx Completion
         * from fw is required or not.
         * This  flag is only relevant if tx completion is not
         * universally enabled.
         * For all LL systems, tx completion is mandatory,
         * so this flag will be irrelevant.
         * For HL systems tx completion is optional, but HL systems in which
         * the bus throughput exceeds the WLAN throughput will
         * probably want to always use tx completion, and thus
         * would not check this flag.
         * This flag is required when tx completions are not used universally,
         * but are still required for certain tx frames for which
         * an OTA delivery acknowledgment is needed by the host.
         * In practice, this would be for HL systems in which the
         * bus throughput is less than the WLAN throughput.
         *
         * 0x0 - Tx Completion Indication from Fw not required
         * 0x1 - Tx Completion Indication from Fw is required
         */
        tx_compl_req: 1;


        /* DWORD 1: MSDU length and ID */
#define HTT_TX_MSDU_LEN_DWORD 1
#define HTT_TX_MSDU_LEN_MASK 0xffff;
        A_UINT32
            len: 16, /* MSDU length, in bytes */
            id:  16; /* MSDU ID used to identify the MSDU to the host, and this id is used to calculate
                      * fragmentation descriptor pointer inside the target based on the base address,
                      * configured inside the target.
                      */

        /* DWORD 2: fragmentation descriptor bus address */
        /* frags_desc_ptr -
         * The fragmentation descriptor pointer tells the HW's MAC DMA
         * where the tx frame's fragments reside in memory.
         * This field only applies to LL systems, since in HL systems the
         * (degenerate single-fragment) fragmentation descriptor is created
         * within the target.
         */
        A_UINT32 frags_desc_ptr;

        /* DWORD 3: peerid, chanfreq */
        /*
         * Peer ID : Target can use this value to know which peer-id packet
         *           destined to.
         *           It's intended to be specified by host in case of NAWDS.
         */
        A_UINT16 peerid;

        /*
         * Channel frequency: This identifies the desired channel
         * frequency (in mhz) for tx frames. This is used by FW to help determine
         * when it is safe to transmit or drop frames for off-channel
         * operation. The default value of zero indicates to FW that the
         * corresponding VDEV's home channel (if there is one) is the
         * desired channel frequency.
         */
        A_UINT16 chanfreq;

        /* Reason reserved is commented is increasing the htt structure size
         * leads to some wierd issues. Contact Raj/Kyeyoon for more info
         * A_UINT32 reserved_dword3_bits0_31;
         */
} POSTPACK;

PREPACK struct htt_mgmt_tx_compl_ind {
    A_UINT32    desc_id;
    A_UINT32    status;
} POSTPACK;

/*
 * This SDU header size comes from the summation of the following:
 *  1. Max of:
 *     a.  Native WiFi header, for native WiFi frames: 24 bytes
 *         (frame control, duration / ID, addr1, addr2, addr3, seq ctrl, addr4)
 *     b.  802.11 header, for raw frames: 36 bytes
 *         (frame control, duration / ID, addr1, addr2, addr3, seq ctrl, addr4,
 *         QoS header, HT header)
 *     c.  802.3 header, for ethernet frames: 14 bytes
 *         (destination address, source address, ethertype / length)
 *  2. Max of:
 *     a.  IPv4 header, up through the DiffServ Code Point: 2 bytes
 *     b.  IPv6 header, up through the Traffic Class: 2 bytes
 *  3. 802.1Q VLAN header: 4 bytes
 *  4. LLC/SNAP header: 8 bytes
 */
#define HTT_TX_HDR_SIZE_NATIVE_WIFI 30
#define HTT_TX_HDR_SIZE_802_11_RAW 36
#define HTT_TX_HDR_SIZE_ETHERNET 14

#define HTT_TX_HDR_SIZE_OUTER_HDR_MAX HTT_TX_HDR_SIZE_802_11_RAW
A_COMPILE_TIME_ASSERT(
    htt_encap_hdr_size_max_check_nwifi,
    HTT_TX_HDR_SIZE_OUTER_HDR_MAX >= HTT_TX_HDR_SIZE_NATIVE_WIFI);
A_COMPILE_TIME_ASSERT(
    htt_encap_hdr_size_max_check_enet,
    HTT_TX_HDR_SIZE_OUTER_HDR_MAX >= HTT_TX_HDR_SIZE_ETHERNET);

#define HTT_HL_TX_HDR_SIZE_IP 1600    /* also include payload */
#define HTT_LL_TX_HDR_SIZE_IP 16      /* up to the end of UDP header for IPv4 case */

#define HTT_TX_HDR_SIZE_802_1Q 4
#define HTT_TX_HDR_SIZE_LLC_SNAP 8


#define HTT_COMMON_TX_FRM_HDR_LEN \
     (HTT_TX_HDR_SIZE_OUTER_HDR_MAX + \
     HTT_TX_HDR_SIZE_802_1Q + \
     HTT_TX_HDR_SIZE_LLC_SNAP)

#define HTT_HL_TX_FRM_HDR_LEN \
     (HTT_COMMON_TX_FRM_HDR_LEN + HTT_HL_TX_HDR_SIZE_IP)

#define HTT_LL_TX_FRM_HDR_LEN \
     (HTT_COMMON_TX_FRM_HDR_LEN + HTT_LL_TX_HDR_SIZE_IP)

#define HTT_TX_DESC_LEN  sizeof(struct htt_tx_msdu_desc_t)

/* dword 0 */
#define HTT_TX_DESC_PKT_SUBTYPE_OFFSET_BYTES 0
#define HTT_TX_DESC_PKT_SUBTYPE_OFFSET_DWORD 0
#define HTT_TX_DESC_PKT_SUBTYPE_M      0x00001f00
#define HTT_TX_DESC_PKT_SUBTYPE_S      8

#define HTT_TX_DESC_NO_ENCRYPT_OFFSET_BYTES 0
#define HTT_TX_DESC_NO_ENCRYPT_OFFSET_DWORD 0
#define HTT_TX_DESC_NO_ENCRYPT_M      0x00000400
#define HTT_TX_DESC_NO_ENCRYPT_S      10

#define HTT_TX_DESC_PKT_TYPE_OFFSET_BYTES 0
#define HTT_TX_DESC_PKT_TYPE_OFFSET_DWORD 0
#define HTT_TX_DESC_PKT_TYPE_M         0x0000e000
#define HTT_TX_DESC_PKT_TYPE_S         13

#define HTT_TX_DESC_VDEV_ID_OFFSET_BYTES 0
#define HTT_TX_DESC_VDEV_ID_OFFSET_DWORD 0
#define HTT_TX_DESC_VDEV_ID_M          0x003f0000
#define HTT_TX_DESC_VDEV_ID_S          16

#define HTT_TX_DESC_EXT_TID_OFFSET_BYTES 0
#define HTT_TX_DESC_EXT_TID_OFFSET_DWORD 0
#define HTT_TX_DESC_EXT_TID_M          0x07c00000
#define HTT_TX_DESC_EXT_TID_S          22

#define HTT_TX_DESC_POSTPONED_OFFSET_BYTES 0
#define HTT_TX_DESC_POSTPONED_OFFSET_DWORD 0
#define HTT_TX_DESC_POSTPONED_M        0x08000000
#define HTT_TX_DESC_POSTPONED_S        27

#define HTT_TX_DESC_CKSUM_OFFLOAD_OFFSET_BYTES 0
#define HTT_TX_DESC_CKSUM_OFFLOAD_OFFSET_DWORD 0
#define HTT_TX_DESC_CKSUM_OFFLOAD_M 0x60000000
#define HTT_TX_DESC_CKSUM_OFFLOAD_S 29

#define HTT_TX_DESC_TX_COMP_OFFSET_BYTES 0
#define HTT_TX_DESC_TX_COMP_OFFSET_DWORD 0
#define HTT_TX_DESC_TX_COMP_M 0x80000000
#define HTT_TX_DESC_TX_COMP_S 31

/* dword 1 */
#define HTT_TX_DESC_FRM_LEN_OFFSET_BYTES 4
#define HTT_TX_DESC_FRM_LEN_OFFSET_DWORD 1
#define HTT_TX_DESC_FRM_LEN_M          0x0000ffff
#define HTT_TX_DESC_FRM_LEN_S          0

#define HTT_TX_DESC_FRM_ID_OFFSET_BYTES 4
#define HTT_TX_DESC_FRM_ID_OFFSET_DWORD 1
#define HTT_TX_DESC_FRM_ID_M           0xffff0000
#define HTT_TX_DESC_FRM_ID_S           16

/* dword 2 */
#define HTT_TX_DESC_FRAGS_DESC_PADDR_OFFSET_BYTES 8
#define HTT_TX_DESC_FRAGS_DESC_PADDR_OFFSET_DWORD 2
#define HTT_TX_DESC_FRAGS_DESC_PADDR_M 0xffffffff
#define HTT_TX_DESC_FRAGS_DESC_PADDR_S 0

/* dword 3 */
/* peer_id */
#define HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_BYTES 12
#define HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_DWORD 3
#define HTT_TX_DESC_PEERID_DESC_PADDR_M 0x0000ffff
#define HTT_TX_DESC_PEERID_DESC_PADDR_S 0

/* channel frequency tag */
#define HTT_TX_DESC_CHANFREQ_DESC_PADDR_OFFSET_BYTES 14
#define HTT_TX_DESC_CHANFREQ_DESC_PADDR_OFFSET_DWORD 3
#define HTT_TX_DESC_CHANFREQ_DESC_PADDR_M 0xffff0000
#define HTT_TX_DESC_CHANFREQ_DESC_PADDR_S 16

#define HTT_TX_DESC_PKT_SUBTYPE_GET(_var) \
    (((_var) & HTT_TX_DESC_PKT_SUBTYPE_M) >> HTT_TX_DESC_PKT_SUBTYPE_S)
#define HTT_TX_DESC_PKT_SUBTYPE_SET(_var, _val)            \
    do {                                                   \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_PKT_SUBTYPE, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_PKT_SUBTYPE_S)); \
    } while (0)

#define HTT_TX_DESC_NO_ENCRYPT_GET(_var) \
    (((_var) & HTT_TX_DESC_NO_ENCRYPT_M) >> HTT_TX_DESC_NO_ENCRYPT_S)
#define HTT_TX_DESC_NO_ENCRYPT_SET(_var, _val)            \
    do {                                                   \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_NO_ENCRYPT, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_NO_ENCRYPT_S)); \
    } while (0)

#define HTT_TX_DESC_PKT_TYPE_GET(_var) \
    (((_var) & HTT_TX_DESC_PKT_TYPE_M) >> HTT_TX_DESC_PKT_TYPE_S)
#define HTT_TX_DESC_PKT_TYPE_SET(_var, _val)            \
    do {                                                \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_PKT_TYPE, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_PKT_TYPE_S)); \
    } while (0)

#define HTT_TX_DESC_VDEV_ID_GET(_var) \
    (((_var) & HTT_TX_DESC_VDEV_ID_M) >> HTT_TX_DESC_VDEV_ID_S)
#define HTT_TX_DESC_VDEV_ID_SET(_var, _val)            \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_VDEV_ID, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_VDEV_ID_S)); \
    } while (0)

#define HTT_TX_DESC_EXT_TID_GET(_var) \
    (((_var) & HTT_TX_DESC_EXT_TID_M) >> HTT_TX_DESC_EXT_TID_S)
#define HTT_TX_DESC_EXT_TID_SET(_var, _val)            \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_EXT_TID, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_EXT_TID_S)); \
    } while (0)

#define HTT_TX_DESC_POSTPONED_GET(_var) \
    (((_var) & HTT_TX_DESC_POSTPONED_M) >> HTT_TX_DESC_POSTPONED_S)
#define HTT_TX_DESC_POSTPONED_SET(_var, _val)            \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_POSTPONED, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_POSTPONED_S)); \
    } while (0)

#define HTT_TX_DESC_FRM_LEN_GET(_var) \
    (((_var) & HTT_TX_DESC_FRM_LEN_M) >> HTT_TX_DESC_FRM_LEN_S)
#define HTT_TX_DESC_FRM_LEN_SET(_var, _val)            \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_FRM_LEN, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_FRM_LEN_S)); \
    } while (0)

#define HTT_TX_DESC_FRM_ID_GET(_var) \
    (((_var) & HTT_TX_DESC_FRM_ID_M) >> HTT_TX_DESC_FRM_ID_S)
#define HTT_TX_DESC_FRM_ID_SET(_var, _val)            \
    do {                                              \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_FRM_ID, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_FRM_ID_S)); \
    } while (0)

#define HTT_TX_DESC_CKSUM_OFFLOAD_GET(_var) \
    (((_var) & HTT_TX_DESC_CKSUM_OFFLOAD_M) >> HTT_TX_DESC_CKSUM_OFFLOAD_S)
#define HTT_TX_DESC_CKSUM_OFFLOAD_SET(_var, _val)            \
    do {                                              \
        HTT_CHECK_SET_VAL(HTT_TX_DESC_CKSUM_OFFLOAD, _val);  \
        ((_var) |= ((_val) << HTT_TX_DESC_CKSUM_OFFLOAD_S)); \
    } while (0)

#define HTT_TX_DESC_TX_COMP_GET(_var) \
    (((_var) & HTT_TX_DESC_TX_COMP_M) >> HTT_TX_DESC_TX_COMP_S)
#define HTT_TX_DESC_TX_COMP_SET(_var, _val)             \
     do {                                               \
         HTT_CHECK_SET_VAL(HTT_TX_DESC_TX_COMP, _val);  \
         ((_var) |= ((_val) << HTT_TX_DESC_TX_COMP_S)); \
     } while (0)

/**
 * @brief MAC DMA rx ring setup specification
 * @details
 *  To allow for dynamic rx ring reconfiguration and to avoid race
 *  conditions, the host SW never directly programs the MAC DMA rx ring(s)
 *  it uses.  Instead, it sends this message to the target, indicating how
 *  the rx ring used by the host should be set up and maintained.
 *  The message consists of a 4-octet header followed by 1 or 2 rx ring setup
 *  specifications.
 *
 *            |31                           16|15            8|7             0|
 *            |---------------------------------------------------------------|
 * header:    |            reserved           |   num rings   |    msg type   |
 *            |---------------------------------------------------------------|
 * payload 1: |             FW_IDX shadow register physical address           |
 *            |---------------------------------------------------------------|
 *            |                   rx ring base physical address               |
 *            |---------------------------------------------------------------|
 *            |      rx ring buffer size      |        rx ring length         |
 *            |---------------------------------------------------------------|
 *            |      FW_IDX initial value     |         enabled flags         |
 *            |---------------------------------------------------------------|
 *            |      MSDU payload offset      |     802.11 header offset      |
 *            |---------------------------------------------------------------|
 *            |        PPDU end offset        |       PPDU start offset       |
 *            |---------------------------------------------------------------|
 *            |        MPDU end offset        |       MPDU start offset       |
 *            |---------------------------------------------------------------|
 *            |        MSDU end offset        |       MSDU start offset       |
 *            |---------------------------------------------------------------|
 *            |        frag info offset       |      rx attention offset      |
 *            |---------------------------------------------------------------|
 * payload 2, if present, has the same format as payload 1
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx ring configuration message
 *     Value: 0x2
 *   - NUM_RINGS
 *     Bits 15:8
 *     Purpose: indicates whether the host is setting up one rx ring or two
 *     Value: 1 or 2
 * Payload:
 *   - IDX_SHADOW_REG_PADDR
 *     Bits 31:0
 *     Value: physical address of the host's FW_IDX shadow register
 *   - RING_BASE_PADDR
 *     Bits 31:0
 *     Value: physical address of the host's rx ring
 *   - RING_LEN
 *     Bits 15:0
 *     Value: number of elements in the rx ring
 *   - RING_BUF_SZ
 *     Bits 31:16
 *     Value: size of the buffers reference by the rx ring, in byte units
 *   - ENABLED_FLAGS
 *     Bits 15:0
 *     Value: 1-bit flags to show whether different rx fields are enabled
 *         bit  0: 802.11 header enabled (1) or disabled (0)
 *         bit  1: MSDU payload  enabled (1) or disabled (0)
 *         bit  2: PPDU start    enabled (1) or disabled (0)
 *         bit  3: PPDU end      enabled (1) or disabled (0)
 *         bit  4: MPDU start    enabled (1) or disabled (0)
 *         bit  5: MPDU end      enabled (1) or disabled (0)
 *         bit  6: MSDU start    enabled (1) or disabled (0)
 *         bit  7: MSDU end      enabled (1) or disabled (0)
 *         bit  8: rx attention  enabled (1) or disabled (0)
 *         bit  9: frag info     enabled (1) or disabled (0)
 *         bit 10: unicast rx    enabled (1) or disabled (0)
 *         bit 11: multicast rx  enabled (1) or disabled (0)
 *         bit 12: ctrl rx       enabled (1) or disabled (0)
 *         bit 13: mgmt rx       enabled (1) or disabled (0)
 *         bit 14: null rx       enabled (1) or disabled (0)
 *         bit 15: phy data rx   enabled (1) or disabled (0)
 *   - IDX_INIT_VAL
 *     Bits 31:16
 *     Purpose: Specify the initial value for the FW_IDX.
 *     Value: the number of buffers initially present in the host's rx ring
 *   - OFFSET_802_11_HDR
 *     Bits 15:0
 *     Value: offset in QUAD-bytes of 802.11 header from the buffer start
 *   - OFFSET_MSDU_PAYLOAD
 *     Bits 31:16
 *     Value: offset in QUAD-bytes of MSDU payload from the buffer start
 *   - OFFSET_PPDU_START
 *     Bits 15:0
 *     Value: offset in QUAD-bytes of PPDU start rx desc from the buffer start
 *   - OFFSET_PPDU_END
 *     Bits 31:16
 *     Value: offset in QUAD-bytes of PPDU end rx desc from the buffer start
 *   - OFFSET_MPDU_START
 *     Bits 15:0
 *     Value: offset in QUAD-bytes of MPDU start rx desc from the buffer start
 *   - OFFSET_MPDU_END
 *     Bits 31:16
 *     Value: offset in QUAD-bytes of MPDU end rx desc from the buffer start
 *   - OFFSET_MSDU_START
 *     Bits 15:0
 *     Value: offset in QUAD-bytes of MSDU start rx desc from the buffer start
 *   - OFFSET_MSDU_END
 *     Bits 31:16
 *     Value: offset in QUAD-bytes of MSDU end rx desc from the buffer start
 *   - OFFSET_RX_ATTN
 *     Bits 15:0
 *     Value: offset in QUAD-bytes of rx attention word from the buffer start
 *   - OFFSET_FRAG_INFO
 *     Bits 31:16
 *     Value: offset in QUAD-bytes of frag info table
 */
/* header fields */
#define HTT_RX_RING_CFG_NUM_RINGS_M      0xff00
#define HTT_RX_RING_CFG_NUM_RINGS_S      8
/* payload fields */
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_M 0xffffffff
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_S 0
#define HTT_RX_RING_CFG_BASE_PADDR_M      0xffffffff
#define HTT_RX_RING_CFG_BASE_PADDR_S      0
#define HTT_RX_RING_CFG_LEN_M             0xffff
#define HTT_RX_RING_CFG_LEN_S             0
#define HTT_RX_RING_CFG_BUF_SZ_M          0xffff0000
#define HTT_RX_RING_CFG_BUF_SZ_S          16
#define HTT_RX_RING_CFG_ENABLED_802_11_HDR_M   0x1
#define HTT_RX_RING_CFG_ENABLED_802_11_HDR_S   0
#define HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_M   0x2
#define HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_S   1
#define HTT_RX_RING_CFG_ENABLED_PPDU_START_M   0x4
#define HTT_RX_RING_CFG_ENABLED_PPDU_START_S   2
#define HTT_RX_RING_CFG_ENABLED_PPDU_END_M     0x8
#define HTT_RX_RING_CFG_ENABLED_PPDU_END_S     3
#define HTT_RX_RING_CFG_ENABLED_MPDU_START_M   0x10
#define HTT_RX_RING_CFG_ENABLED_MPDU_START_S   4
#define HTT_RX_RING_CFG_ENABLED_MPDU_END_M     0x20
#define HTT_RX_RING_CFG_ENABLED_MPDU_END_S     5
#define HTT_RX_RING_CFG_ENABLED_MSDU_START_M   0x40
#define HTT_RX_RING_CFG_ENABLED_MSDU_START_S   6
#define HTT_RX_RING_CFG_ENABLED_MSDU_END_M     0x80
#define HTT_RX_RING_CFG_ENABLED_MSDU_END_S     7
#define HTT_RX_RING_CFG_ENABLED_RX_ATTN_M      0x100
#define HTT_RX_RING_CFG_ENABLED_RX_ATTN_S      8
#define HTT_RX_RING_CFG_ENABLED_FRAG_INFO_M    0x200
#define HTT_RX_RING_CFG_ENABLED_FRAG_INFO_S    9
#define HTT_RX_RING_CFG_ENABLED_UCAST_M        0x400
#define HTT_RX_RING_CFG_ENABLED_UCAST_S        10
#define HTT_RX_RING_CFG_ENABLED_MCAST_M        0x800
#define HTT_RX_RING_CFG_ENABLED_MCAST_S        11
#define HTT_RX_RING_CFG_ENABLED_CTRL_M         0x1000
#define HTT_RX_RING_CFG_ENABLED_CTRL_S         12
#define HTT_RX_RING_CFG_ENABLED_MGMT_M         0x2000
#define HTT_RX_RING_CFG_ENABLED_MGMT_S         13
#define HTT_RX_RING_CFG_ENABLED_NULL_M         0x4000
#define HTT_RX_RING_CFG_ENABLED_NULL_S         14
#define HTT_RX_RING_CFG_ENABLED_PHY_M          0x8000
#define HTT_RX_RING_CFG_ENABLED_PHY_S          15
#define HTT_RX_RING_CFG_IDX_INIT_VAL_M         0xffff0000
#define HTT_RX_RING_CFG_IDX_INIT_VAL_S         16
#define HTT_RX_RING_CFG_OFFSET_802_11_HDR_M    0xffff
#define HTT_RX_RING_CFG_OFFSET_802_11_HDR_S    0
#define HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_M    0xffff0000
#define HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_S    16
#define HTT_RX_RING_CFG_OFFSET_PPDU_START_M    0xffff
#define HTT_RX_RING_CFG_OFFSET_PPDU_START_S    0
#define HTT_RX_RING_CFG_OFFSET_PPDU_END_M      0xffff0000
#define HTT_RX_RING_CFG_OFFSET_PPDU_END_S      16
#define HTT_RX_RING_CFG_OFFSET_MPDU_START_M    0xffff
#define HTT_RX_RING_CFG_OFFSET_MPDU_START_S    0
#define HTT_RX_RING_CFG_OFFSET_MPDU_END_M      0xffff0000
#define HTT_RX_RING_CFG_OFFSET_MPDU_END_S      16
#define HTT_RX_RING_CFG_OFFSET_MSDU_START_M    0xffff
#define HTT_RX_RING_CFG_OFFSET_MSDU_START_S    0
#define HTT_RX_RING_CFG_OFFSET_MSDU_END_M      0xffff0000
#define HTT_RX_RING_CFG_OFFSET_MSDU_END_S      16
#define HTT_RX_RING_CFG_OFFSET_RX_ATTN_M       0xffff
#define HTT_RX_RING_CFG_OFFSET_RX_ATTN_S       0
#define HTT_RX_RING_CFG_OFFSET_FRAG_INFO_M     0xffff0000
#define HTT_RX_RING_CFG_OFFSET_FRAG_INFO_S     16

#define HTT_RX_RING_CFG_HDR_BYTES 4
#define HTT_RX_RING_CFG_PAYLD_BYTES 36
#define HTT_RX_RING_CFG_BYTES(num_rings) \
    (HTT_RX_RING_CFG_HDR_BYTES + (num_rings) * HTT_RX_RING_CFG_PAYLD_BYTES)


#define HTT_RX_RING_CFG_NUM_RINGS_GET(_var) \
    (((_var) & HTT_RX_RING_CFG_NUM_RINGS_M) >> HTT_RX_RING_CFG_NUM_RINGS_S)
#define HTT_RX_RING_CFG_NUM_RINGS_SET(_var, _val)            \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_NUM_RINGS, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_NUM_RINGS_S)); \
    } while (0)

/* degenerate case for 32-bit fields */
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_GET(_var) (_var)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(_var, _val) (_var) = (_val)

/* degenerate case for 32-bit fields */
#define HTT_RX_RING_CFG_BASE_PADDR_GET(_var) (_var)
#define HTT_RX_RING_CFG_BASE_PADDR_SET(_var, _val) (_var) = (_val)

#define HTT_RX_RING_CFG_LEN_GET(_var) \
    (((_var) & HTT_RX_RING_CFG_LEN_M) >> HTT_RX_RING_CFG_LEN_S)
#define HTT_RX_RING_CFG_LEN_SET(_var, _val)            \
    do {                                                    \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_LEN, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_LEN_S)); \
    } while (0)

#define HTT_RX_RING_CFG_BUF_SZ_GET(_var) \
    (((_var) & HTT_RX_RING_CFG_BUF_SZ_M) >> HTT_RX_RING_CFG_BUF_SZ_S)
#define HTT_RX_RING_CFG_BUF_SZ_SET(_var, _val)            \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_BUF_SZ, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_BUF_SZ_S)); \
    } while (0)

#define HTT_RX_RING_CFG_IDX_INIT_VAL_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_IDX_INIT_VAL_M) >> \
    HTT_RX_RING_CFG_IDX_INIT_VAL_S)
#define HTT_RX_RING_CFG_IDX_INIT_VAL_SET(_var, _val)            \
    do {                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_IDX_INIT_VAL, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_IDX_INIT_VAL_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_802_11_HDR_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_802_11_HDR_M) >> \
    HTT_RX_RING_CFG_ENABLED_802_11_HDR_S)
#define HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(_var, _val)            \
    do {                                                              \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_802_11_HDR, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_802_11_HDR_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_M) >> \
    HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_S)
#define HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(_var, _val)            \
    do {                                                              \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_PPDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_PPDU_START_M) >> \
    HTT_RX_RING_CFG_ENABLED_PPDU_START_S)
#define HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(_var, _val)            \
    do {                                                              \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_PPDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_PPDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_PPDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_PPDU_END_M) >> \
    HTT_RX_RING_CFG_ENABLED_PPDU_END_S)
#define HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_PPDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_PPDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MPDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MPDU_START_M) >> \
    HTT_RX_RING_CFG_ENABLED_MPDU_START_S)
#define HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(_var, _val)            \
    do {                                                              \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MPDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MPDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MPDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MPDU_END_M) >> \
    HTT_RX_RING_CFG_ENABLED_MPDU_END_S)
#define HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MPDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MPDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MSDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MSDU_START_M) >> \
    HTT_RX_RING_CFG_ENABLED_MSDU_START_S)
#define HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(_var, _val)            \
    do {                                                              \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MSDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MSDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MSDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MSDU_END_M) >> \
    HTT_RX_RING_CFG_ENABLED_MSDU_END_S)
#define HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MSDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MSDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_RX_ATTN_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_RX_ATTN_M) >> \
    HTT_RX_RING_CFG_ENABLED_RX_ATTN_S)
#define HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(_var, _val)            \
    do {                                                           \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_RX_ATTN, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_RX_ATTN_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_FRAG_INFO_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_FRAG_INFO_M) >> \
    HTT_RX_RING_CFG_ENABLED_FRAG_INFO_S)
#define HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_FRAG_INFO, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_FRAG_INFO_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_UCAST_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_UCAST_M) >> \
    HTT_RX_RING_CFG_ENABLED_UCAST_S)
#define HTT_RX_RING_CFG_ENABLED_UCAST_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_UCAST, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_UCAST_S)); \
    } while (0)

#define HTT_RX_RING_CFG_ENABLED_MCAST_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MCAST_M) >> \
    HTT_RX_RING_CFG_ENABLED_MCAST_S)
#define HTT_RX_RING_CFG_ENABLED_MCAST_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MCAST, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MCAST_S)); \
    } while (0)
#define HTT_RX_RING_CFG_ENABLED_CTRL_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_CTRL_M) >> \
    HTT_RX_RING_CFG_ENABLED_CTRL_S)
#define HTT_RX_RING_CFG_ENABLED_CTRL_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_CTRL, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_CTRL_S)); \
    } while (0)
#define HTT_RX_RING_CFG_ENABLED_MGMT_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_MGMT_M) >> \
    HTT_RX_RING_CFG_ENABLED_MGMT_S)
#define HTT_RX_RING_CFG_ENABLED_MGMT_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_MGMT, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_MGMT_S)); \
    } while (0)
#define HTT_RX_RING_CFG_ENABLED_NULL_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_NULL_M) >> \
    HTT_RX_RING_CFG_ENABLED_NULL_S)
#define HTT_RX_RING_CFG_ENABLED_NULL_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_NULL, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_NULL_S)); \
    } while (0)
#define HTT_RX_RING_CFG_ENABLED_PHY_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_ENABLED_PHY_M) >> \
    HTT_RX_RING_CFG_ENABLED_PHY_S)
#define HTT_RX_RING_CFG_ENABLED_PHY_SET(_var, _val)            \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_ENABLED_PHY, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_ENABLED_PHY_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_802_11_HDR_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_802_11_HDR_M) >> \
    HTT_RX_RING_CFG_OFFSET_802_11_HDR_S)
#define HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(_var, _val)            \
    do {                                                                  \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_802_11_HDR, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_802_11_HDR_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_M) >> \
    HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_S)
#define HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(_var, _val)            \
    do {                                                                  \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_PPDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_PPDU_START_M) >> \
    HTT_RX_RING_CFG_OFFSET_PPDU_START_S)
#define HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(_var, _val)            \
    do {                                                                  \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_PPDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_PPDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_PPDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_PPDU_END_M) >> \
    HTT_RX_RING_CFG_OFFSET_PPDU_END_S)
#define HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(_var, _val)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_PPDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_PPDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_MPDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_MPDU_START_M) >> \
    HTT_RX_RING_CFG_OFFSET_MPDU_START_S)
#define HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(_var, _val)            \
    do {                                                                  \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_MPDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_MPDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_MPDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_MPDU_END_M) >> \
    HTT_RX_RING_CFG_OFFSET_MPDU_END_S)
#define HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(_var, _val)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_MPDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_MPDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_MSDU_START_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_MSDU_START_M) >> \
    HTT_RX_RING_CFG_OFFSET_MSDU_START_S)
#define HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(_var, _val)            \
    do {                                                                  \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_MSDU_START, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_MSDU_START_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_MSDU_END_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_MSDU_END_M) >> \
    HTT_RX_RING_CFG_OFFSET_MSDU_END_S)
#define HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(_var, _val)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_MSDU_END, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_MSDU_END_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_RX_ATTN_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_RX_ATTN_M) >> \
    HTT_RX_RING_CFG_OFFSET_RX_ATTN_S)
#define HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(_var, _val)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_RX_ATTN, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_RX_ATTN_S)); \
    } while (0)

#define HTT_RX_RING_CFG_OFFSET_FRAG_INFO_GET(_var)    \
    (((_var) & HTT_RX_RING_CFG_OFFSET_FRAG_INFO_M) >> \
    HTT_RX_RING_CFG_OFFSET_FRAG_INFO_S)
#define HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(_var, _val)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_RING_CFG_OFFSET_FRAG_INFO, _val);  \
        ((_var) |= ((_val) << HTT_RX_RING_CFG_OFFSET_FRAG_INFO_S)); \
    } while (0)

/**
 * @brief host -> target FW statistics retrieve
 *
 * @details
 * The following field definitions describe the format of the HTT host
 * to target FW stats retrieve message. The message specifies the type of
 * stats host wants to retrieve.
 *
 * |31          24|23          16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |        stats types request bitmask         |   msg type   |
 * |-----------------------------------------------------------|
 * |         stats types reset bitmask          |   reserved   |
 * |-----------------------------------------------------------|
 * |  stats type  |               config value                 |
 * |-----------------------------------------------------------|
 * |                        cookie LSBs                        |
 * |-----------------------------------------------------------|
 * |                        cookie MSBs                        |
 * |-----------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this is a stats upload request message
 *    Value: 0x3
 *  - UPLOAD_TYPES
 *    Bits 31:8
 *    Purpose: identifies which types of FW statistics to upload
 *    Value: mask with bits set in positions defined by htt_dbg_stats_type
 *  - RESET_TYPES
 *    Bits 31:8
 *    Purpose: identifies which types of FW statistics to reset
 *    Value: mask with bits set in positions defined by htt_dbg_stats_type
 *  - CFG_VAL
 *    Bits 23:0
 *    Purpose: give an opaque configuration value to the specified stats type
 *    Value: stats-type specific configuration value
 *        if stats type == tx PPDU log, then CONFIG_VAL has the format:
 *            bits  7:0  - how many per-MPDU byte counts to include in a record
 *            bits 15:8  - how many per-MPDU MSDU counts to include in a record
 *            bits 23:16 - how many per-MSDU byte counts to include in a record
 *  - CFG_STAT_TYPE
 *    Bits 31:24
 *    Purpose: specify which stats type (if any) the config value applies to
 *    Value: htt_dbg_stats_type value, or 0xff if the message doesn't have
 *        a valid configuration specification
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: LSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 */

#define HTT_H2T_STATS_REQ_MSG_SZ                    20 /* bytes */

#define HTT_H2T_STATS_REQ_CFG_STAT_TYPE_INVALID     0xff

#define HTT_H2T_STATS_REQ_UPLOAD_TYPES_M            0xffffff00
#define HTT_H2T_STATS_REQ_UPLOAD_TYPES_S            8

#define HTT_H2T_STATS_REQ_RESET_TYPES_M             0xffffff00
#define HTT_H2T_STATS_REQ_RESET_TYPES_S             8

#define HTT_H2T_STATS_REQ_CFG_VAL_M                 0x00ffffff
#define HTT_H2T_STATS_REQ_CFG_VAL_S                 0

#define HTT_H2T_STATS_REQ_CFG_STAT_TYPE_M           0xff000000
#define HTT_H2T_STATS_REQ_CFG_STAT_TYPE_S           24

#define HTT_H2T_STATS_REQ_UPLOAD_TYPES_GET(_var)     \
    (((_var) & HTT_H2T_STATS_REQ_UPLOAD_TYPES_M) >>  \
     HTT_H2T_STATS_REQ_UPLOAD_TYPES_S)
#define HTT_H2T_STATS_REQ_UPLOAD_TYPES_SET(_var, _val)            \
    do {                                                          \
        HTT_CHECK_SET_VAL(HTT_H2T_STATS_REQ_UPLOAD_TYPES, _val);  \
        ((_var) |= ((_val) << HTT_H2T_STATS_REQ_UPLOAD_TYPES_S)); \
    } while (0)

#define HTT_H2T_STATS_REQ_RESET_TYPES_GET(_var)     \
    (((_var) & HTT_H2T_STATS_REQ_RESET_TYPES_M) >>  \
     HTT_H2T_STATS_REQ_RESET_TYPES_S)
#define HTT_H2T_STATS_REQ_RESET_TYPES_SET(_var, _val)            \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_H2T_STATS_REQ_RESET_TYPES, _val);  \
        ((_var) |= ((_val) << HTT_H2T_STATS_REQ_RESET_TYPES_S)); \
    } while (0)

#define HTT_H2T_STATS_REQ_CFG_VAL_GET(_var)     \
    (((_var) & HTT_H2T_STATS_REQ_CFG_VAL_M) >>  \
     HTT_H2T_STATS_REQ_CFG_VAL_S)
#define HTT_H2T_STATS_REQ_CFG_VAL_SET(_var, _val)            \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_H2T_STATS_REQ_CFG_VAL, _val);  \
        ((_var) |= ((_val) << HTT_H2T_STATS_REQ_CFG_VAL_S)); \
    } while (0)

#define HTT_H2T_STATS_REQ_CFG_STAT_TYPE_GET(_var)     \
    (((_var) & HTT_H2T_STATS_REQ_CFG_STAT_TYPE_M) >>  \
     HTT_H2T_STATS_REQ_CFG_STAT_TYPE_S)
#define HTT_H2T_STATS_REQ_CFG_STAT_TYPE_SET(_var, _val)            \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_H2T_STATS_REQ_CFG_STAT_TYPE, _val);  \
        ((_var) |= ((_val) << HTT_H2T_STATS_REQ_CFG_STAT_TYPE_S)); \
    } while (0)

/**
 * @brief host -> target HTT out-of-band sync request
 *
 * @details
 *  The HTT SYNC tells the target to suspend processing of subsequent
 *  HTT host-to-target messages until some other target agent locally
 *  informs the target HTT FW that the current sync counter is equal to
 *  or greater than (in a modulo sense) the sync counter specified in
 *  the SYNC message.
 *  This allows other host-target components to synchronize their operation
 *  with HTT, e.g. to ensure that tx frames don't get transmitted until a
 *  security key has been downloaded to and activated by the target.
 *  In the absence of any explicit synchronization counter value
 *  specification, the target HTT FW will use zero as the default current
 *  sync value.
 *
 * |31          24|23          16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |          reserved           |  sync count  |   msg type   |
 * |-----------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this as a sync message
 *    Value: 0x4
 *  - SYNC_COUNT
 *    Bits 15:8
 *    Purpose: specifies what sync value the HTT FW will wait for from
 *        an out-of-band specification to resume its operation
 *    Value: in-band sync counter value to compare against the out-of-band
 *        counter spec.
 *        The HTT target FW will suspend its host->target message processing
 *        as long as
 *        0 < (in-band sync counter - out-of-band sync counter) & 0xff < 128
 */

#define HTT_H2T_SYNC_MSG_SZ                 4

#define HTT_H2T_SYNC_COUNT_M                0x0000ff00
#define HTT_H2T_SYNC_COUNT_S                8

#define HTT_H2T_SYNC_COUNT_GET(_var)        \
    (((_var) & HTT_H2T_SYNC_COUNT_M) >>     \
     HTT_H2T_SYNC_COUNT_S)
#define HTT_H2T_SYNC_COUNT_SET(_var, _val)            \
    do {                                              \
        HTT_CHECK_SET_VAL(HTT_H2T_SYNC_COUNT, _val);  \
        ((_var) |= ((_val) << HTT_H2T_SYNC_COUNT_S)); \
    } while (0)


/**
 * @brief HTT aggregation configuration
 */
#define HTT_AGGR_CFG_MSG_SZ                     4

#define HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_M     0xff00
#define HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_S     8
#define HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_M     0x1f0000
#define HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_S     16

#define HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_GET(_var) \
    (((_var) & HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_M) >> \
     HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_S)
#define HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM, _val);  \
        ((_var) |= ((_val) << HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_S)); \
    } while (0)

#define HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_GET(_var) \
    (((_var) & HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_M) >> \
     HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_S)
#define HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM, _val);  \
        ((_var) |= ((_val) << HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_S)); \
    } while (0)


/**
 * @brief host -> target HTT configure max amsdu info per vdev
 *
 * @details
 *  The HTT AGGR CFG EX tells the target to configure max_amsdu info per vdev
 *
 * |31             21|20       16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |       reserved  | vdev id   |   max amsdu  |   msg type   |
 * |-----------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this as a aggr cfg ex message
 *    Value: 0xa
 *  - MAX_NUM_AMSDU_SUBFRM
 *    Bits 15:8
 *    Purpose: max MSDUs per A-MSDU
 *  - VDEV_ID
 *    Bits 20:16
 *    Purpose: ID of the vdev to which this limit is applied
 */
#define HTT_AGGR_CFG_EX_MSG_SZ                     4

#define HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_M     0xff00
#define HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_S     8
#define HTT_AGGR_CFG_EX_VDEV_ID_M                  0x1f0000
#define HTT_AGGR_CFG_EX_VDEV_ID_S                  16

#define HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_GET(_var) \
            (((_var) & HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_M) >> \
             HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_S)
#define HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_SET(_var, _val) \
            do {                                                     \
                HTT_CHECK_SET_VAL(HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM, _val);  \
                ((_var) |= ((_val) << HTT_AGGR_CFG_EX_MAX_NUM_AMSDU_SUBFRM_S)); \
            } while (0)

#define HTT_AGGR_CFG_EX_VDEV_ID_GET(_var) \
            (((_var) & HTT_AGGR_CFG_EX_VDEV_ID_M) >> \
             HTT_AGGR_CFG_EX_VDEV_ID_S)
#define HTT_AGGR_CFG_EX_VDEV_ID_SET(_var, _val) \
            do {                                                     \
                HTT_CHECK_SET_VAL(HTT_AGGR_CFG_EX_VDEV_ID, _val);  \
                ((_var) |= ((_val) << HTT_AGGR_CFG_EX_VDEV_ID_S)); \
            } while (0)

/**
 * @brief HTT WDI_IPA Config Message
 *
 * @details
 *  The HTT WDI_IPA config message is created/sent by host at driver
 *  init time. It contains information about data structures used on
 *  WDI_IPA TX and RX path.
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |        tx pkt pool size         |      Rsvd      |     msg_type   |
 *     |-------------------------------------------------------------------|
 *     |                         tx comp ring base                         |
 *     |-------------------------------------------------------------------|
 *     |                         tx comp ring size                         |
 *     |-------------------------------------------------------------------|
 *     |                   tx comp WR_IDX physical address                 |
 *     |-------------------------------------------------------------------|
 *     |                   tx CE WR_IDX physical address                   |
 *     |-------------------------------------------------------------------|
 *     |                      rx indication ring base                      |
 *     |-------------------------------------------------------------------|
 *     |                      rx indication ring size                      |
 *     |-------------------------------------------------------------------|
 *     |                    rx ind RD_IDX physical address                 |
 *     |-------------------------------------------------------------------|
 *     |                    rx ind WR_IDX physical address                 |
 *     |-------------------------------------------------------------------|
 *
 * Header fields:
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: Identifies this as WDI_IPA config message
 *     value: = 0x8
 *   - TX_PKT_POOL_SIZE
 *     Bits 15:0
 *     Purpose: Total number of TX packet buffer pool allocated by Host for
 *              WDI_IPA TX path
 *   - TX_COMP_RING_BASE_ADDR
 *     Bits 31:0
 *     Purpose: TX Completion Ring base address in DDR
 *   - TX_COMP_RING_SIZE
 *     Bits 31:0
 *     Purpose: TX Completion Ring size (must be power of 2)
 *   - TX_COMP_WR_IDX_ADDR
 *     Bits 31:0
 *     Purpose: IPA doorbell register address OR DDR address where WIFI FW
 *              updates the Write Index for WDI_IPA TX completion ring
 *   - TX_CE_WR_IDX_ADDR
 *     Bits 31:0
 *     Purpose: DDR address where IPA uC
 *              updates the WR Index for TX CE ring
 *              (needed for fusion platforms)
 *   - RX_IND_RING_BASE_ADDR
 *     Bits 31:0
 *     Purpose: RX Indication Ring base address in DDR
 *   - RX_IND_RING_SIZE
 *     Bits 31:0
 *     Purpose: RX Indication Ring size
 *   - RX_IND_RD_IDX_ADDR
 *     Bits 31:0
 *     Purpose: DDR address where IPA uC updates the Read Index for WDI_IPA
 *              RX indication ring
 *   - RX_IND_WR_IDX_ADDR
 *     Bits 31:0
 *     Purpose: IPA doorbell register address OR DDR address where WIFI FW
 *              updates the Write Index for WDI_IPA RX indication ring
 */

#define HTT_WDI_IPA_CFG_SZ                           36 /* bytes */

#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_M           0xffff0000
#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S           16

#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_M     0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S     0

#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_M          0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S          0

#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_M        0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S        0

#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_M          0xffffffff
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S          0

#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S      0

#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_M           0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S           0

#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_M         0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S         0

#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_M         0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S         0

#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_M) >> HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S)
#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S)); \
    } while (0)

PREPACK struct htt_wdi_ipa_cfg_t
{
    /* DWORD 0: flags and meta-data */
    A_UINT32
        msg_type: 8, /* HTT_H2T_MSG_TYPE_WDI_IPA_CFG */
        reserved: 8,
        tx_pkt_pool_size: 16;
    /* DWORD 1 */
    A_UINT32 tx_comp_ring_base_addr;
    /* DWORD 2 */
    A_UINT32 tx_comp_ring_size;
    /* DWORD 3 */
    A_UINT32 tx_comp_wr_idx_addr;
    /* DWORD 4*/
    A_UINT32 tx_ce_wr_idx_addr;
    /* DWORD 5 */
    A_UINT32 rx_ind_ring_base_addr;
    /* DWORD 6 */
    A_UINT32 rx_ind_ring_size;
    /* DWORD 7 */
    A_UINT32 rx_ind_rd_idx_addr;
    /* DWORD 8 */
    A_UINT32 rx_ind_wr_idx_addr;
} POSTPACK;

enum htt_wdi_ipa_op_code {
    HTT_WDI_IPA_OPCODE_TX_SUSPEND           = 0,
    HTT_WDI_IPA_OPCODE_TX_RESUME            = 1,
    HTT_WDI_IPA_OPCODE_RX_SUSPEND           = 2,
    HTT_WDI_IPA_OPCODE_RX_RESUME            = 3,
    HTT_WDI_IPA_OPCODE_DBG_STATS            = 4,
    /* keep this last */
    HTT_WDI_IPA_OPCODE_MAX
};

/**
 * @brief HTT WDI_IPA Operation Request Message
 *
 * @details
 *  HTT WDI_IPA Operation Request message is sent by host
 *  to either suspend or resume WDI_IPA TX or RX path.
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |             op_code             |      Rsvd      |     msg_type   |
 *     |-------------------------------------------------------------------|
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: Identifies this as WDI_IPA Operation Request message
 *     value: = 0x9
 *   - OP_CODE
 *     Bits 31:16
 *     Purpose: Identifies operation host is requesting (e.g. TX suspend)
 *     value: = enum htt_wdi_ipa_op_code
 */

PREPACK struct htt_wdi_ipa_op_request_t
{
    /* DWORD 0: flags and meta-data */
    A_UINT32
        msg_type: 8, /* HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQUEST */
        reserved: 8,
        op_code: 16;
} POSTPACK;

#define HTT_WDI_IPA_OP_REQUEST_SZ                    4 /* bytes */

#define HTT_WDI_IPA_OP_REQUEST_OP_CODE_M             0xffff0000
#define HTT_WDI_IPA_OP_REQUEST_OP_CODE_S             16

#define HTT_WDI_IPA_OP_REQUEST_OP_CODE_GET(_var) \
    (((_var) & HTT_WDI_IPA_OP_REQUEST_OP_CODE_M) >> HTT_WDI_IPA_OP_REQUEST_OP_CODE_S)
#define HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_OP_REQUEST_OP_CODE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_OP_REQUEST_OP_CODE_S)); \
    } while (0)




/*=== target -> host messages ===============================================*/


enum htt_t2h_msg_type {
    HTT_T2H_MSG_TYPE_VERSION_CONF             = 0x0,
    HTT_T2H_MSG_TYPE_RX_IND                   = 0x1,
    HTT_T2H_MSG_TYPE_RX_FLUSH                 = 0x2,
    HTT_T2H_MSG_TYPE_PEER_MAP                 = 0x3,
    HTT_T2H_MSG_TYPE_PEER_UNMAP               = 0x4,
    HTT_T2H_MSG_TYPE_RX_ADDBA                 = 0x5,
    HTT_T2H_MSG_TYPE_RX_DELBA                 = 0x6,
    HTT_T2H_MSG_TYPE_TX_COMPL_IND             = 0x7,
    HTT_T2H_MSG_TYPE_PKTLOG                   = 0x8,
    HTT_T2H_MSG_TYPE_STATS_CONF               = 0x9,
    HTT_T2H_MSG_TYPE_RX_FRAG_IND              = 0xa,
    HTT_T2H_MSG_TYPE_SEC_IND                  = 0xb,
    DEPRECATED_HTT_T2H_MSG_TYPE_RC_UPDATE_IND = 0xc,
    HTT_T2H_MSG_TYPE_TX_INSPECT_IND           = 0xd,
    HTT_T2H_MSG_TYPE_MGMT_TX_COMPL_IND        = 0xe,
    /* only used for HL, add HTT MSG for HTT CREDIT update */
    HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND     = 0xf,
    HTT_T2H_MSG_TYPE_RX_PN_IND                = 0x10,
    HTT_T2H_MSG_TYPE_RX_OFFLOAD_DELIVER_IND   = 0x11,
    HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND      = 0x12,
    /* 0x13 is reserved for RX_RING_LOW_IND (RX Full reordering related) */
    HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE      = 0x14,
    HTT_T2H_MSG_TYPE_TEST,
    /* keep this last */
    HTT_T2H_NUM_MSGS
};

/*
 * HTT target to host message type -
 * stored in bits 7:0 of the first word of the message
 */
#define HTT_T2H_MSG_TYPE_M      0xff
#define HTT_T2H_MSG_TYPE_S      0

#define HTT_T2H_MSG_TYPE_SET(word, msg_type)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_T2H_MSG_TYPE, msg_type); \
        (word) |= ((msg_type) << HTT_T2H_MSG_TYPE_S);  \
    } while (0)
#define HTT_T2H_MSG_TYPE_GET(word) \
    (((word) & HTT_T2H_MSG_TYPE_M) >> HTT_T2H_MSG_TYPE_S)

/**
 * @brief target -> host version number confirmation message definition
 *
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |    reserved    |  major number  |  minor number  |    msg type    |
 *     |-------------------------------------------------------------------|
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a version number confirmation message
 *     Value: 0x0
 *   - VER_MINOR
 *     Bits 15:8
 *     Purpose: Specify the minor number of the HTT message library version
 *         in use by the target firmware.
 *         The minor number specifies the specific revision within a range
 *         of fundamentally compatible HTT message definition revisions.
 *         Compatible revisions involve adding new messages or perhaps
 *         adding new fields to existing messages, in a backwards-compatible
 *         manner.
 *         Incompatible revisions involve changing the message type values,
 *         or redefining existing messages.
 *     Value: minor number
 *   - VER_MAJOR
 *     Bits 15:8
 *     Purpose: Specify the major number of the HTT message library version
 *         in use by the target firmware.
 *         The major number specifies the family of minor revisions that are
 *         fundamentally compatible with each other, but not with prior or
 *         later families.
 *     Value: major number
 */

#define HTT_VER_CONF_MINOR_M      0x0000ff00
#define HTT_VER_CONF_MINOR_S      8
#define HTT_VER_CONF_MAJOR_M      0x00ff0000
#define HTT_VER_CONF_MAJOR_S      16


#define HTT_VER_CONF_MINOR_SET(word, value)                              \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_VER_CONF_MINOR, value);                    \
        (word) |= (value)  << HTT_VER_CONF_MINOR_S;                      \
    } while (0)
#define HTT_VER_CONF_MINOR_GET(word) \
    (((word) & HTT_VER_CONF_MINOR_M) >> HTT_VER_CONF_MINOR_S)

#define HTT_VER_CONF_MAJOR_SET(word, value)                              \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_VER_CONF_MAJOR, value);                    \
        (word) |= (value)  << HTT_VER_CONF_MAJOR_S;                      \
    } while (0)
#define HTT_VER_CONF_MAJOR_GET(word) \
    (((word) & HTT_VER_CONF_MAJOR_M) >> HTT_VER_CONF_MAJOR_S)


#define HTT_VER_CONF_BYTES 4


/**
 * @brief - target -> host HTT Rx In order indication message
 *
 * @details
 *
 * |31            24|23                 |15|14|13|12|11|10|9|8|7|6|5|4       0|
 * |----------------+-------------------+---------------------+---------------|
 * |                  peer ID           |     | O| ext TID    |   msg type    |
 * |--------------------------------------------------------------------------|
 * |                  MSDU count        |        Reserved     |   vdev id     |
 * |--------------------------------------------------------------------------|
 * |                        MSDU 0 Bus address                                |
 * |--------------------------------------------------------------------------|
 * |     Reserved   | MSDU 0 FW Desc    |         MSDU 0 Length               |
 * |--------------------------------------------------------------------------|
 * |                        MSDU 1 Bus address                                |
 * |--------------------------------------------------------------------------|
 * |     Reserved   | MSDU 1 FW Desc    |         MSDU 1 Length               |
 * |--------------------------------------------------------------------------|
 */

struct htt_rx_in_ord_paddr_ind_hdr_t
{
    A_UINT32 /* word 0 */
        msg_type:   8,
        ext_tid:    5,
        offload:    1,
        reserved_0: 2,
        peer_id:    16;

    A_UINT32 /* word 1 */
        vap_id:     8,
        reserved_1: 8,
        msdu_cnt:   16;
};

struct htt_rx_in_ord_paddr_ind_msdu_t
{
    A_UINT32 dma_addr;
    A_UINT32
        length: 16,
        fw_desc: 8,
        reserved_1:8;
};


#define HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES (sizeof(struct htt_rx_in_ord_paddr_ind_hdr_t))
#define HTT_RX_IN_ORD_PADDR_IND_HDR_DWORDS (HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES >> 2)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTE_OFFSET  HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORD_OFFSET HTT_RX_IN_ORD_PADDR_IND_HDR_DWORDS
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES (sizeof(struct htt_rx_in_ord_paddr_ind_msdu_t))
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS (HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES >> 2)

#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_M      0x00001f00
#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_S      8
#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_M      0x00002000
#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_S      13
#define HTT_RX_IN_ORD_PADDR_IND_FRAG_M         0x4000
#define HTT_RX_IN_ORD_PADDR_IND_FRAG_S         14
#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_M      0xffff0000
#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_S      16
#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_M       0x000000ff
#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_S       0
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_M     0xffff0000
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_S     16
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_M        0xffffffff
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_S        0
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_M      0x00ff0000
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S      16
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_M     0x0000ffff
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S     0


#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_EXT_TID, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_EXT_TID_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_EXT_TID_M) >> HTT_RX_IN_ORD_PADDR_IND_EXT_TID_S)

#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_PEER_ID, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_PEER_ID_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_PEER_ID_M) >> HTT_RX_IN_ORD_PADDR_IND_PEER_ID_S)

#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_SET(word, value)                              \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_VAP_ID, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_VAP_ID_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_VAP_ID_M) >> HTT_RX_IN_ORD_PADDR_IND_VAP_ID_S)

#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_M) >> HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_S)

#define HTT_RX_IN_ORD_PADDR_IND_PADDR_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_PADDR, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_PADDR_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_PADDR_M) >> HTT_RX_IN_ORD_PADDR_IND_PADDR_S)

#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_SET(word, value)                              \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_FW_DESC, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_FW_DESC_M) >> HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S)

#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_SET(word, value)                              \
    do {                                                                         \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_M) >> HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S)

#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_IND_OFFLOAD, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_IND_OFFLOAD_S;                      \
    } while (0)

#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_M) >> HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_S)

#define HTT_RX_IN_ORD_PADDR_IND_FRAG_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_FRAG_M) >> HTT_RX_IN_ORD_PADDR_IND_FRAG_S)

/* definitions used within target -> host rx indication message */

PREPACK struct htt_rx_ind_hdr_prefix_t
{
    A_UINT32 /* word 0 */
        msg_type:      8,
        ext_tid:       5,
        release_valid: 1,
        flush_valid:   1,
        reserved0:     1,
        peer_id:       16;

    A_UINT32 /* word 1 */
        flush_start_seq_num:   6,
        flush_end_seq_num:     6,
        release_start_seq_num: 6,
        release_end_seq_num:   6,
        num_mpdu_ranges:       8;
} POSTPACK;

#define HTT_RX_IND_HDR_PREFIX_BYTES (sizeof(struct htt_rx_ind_hdr_prefix_t))
#define HTT_RX_IND_HDR_PREFIX_SIZE32 (HTT_RX_IND_HDR_PREFIX_BYTES >> 2)

#define HTT_TGT_RSSI_INVALID 0x80

PREPACK struct htt_rx_ppdu_desc_t
{
    #define HTT_RX_IND_PPDU_OFFSET_WORD_RSSI_CMB              0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_TIMESTAMP_SUBMICROSEC 0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_PHY_ERR_CODE          0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_PHY_ERR               0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_LEGACY_RATE           0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_LEGACY_RATE_SEL       0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_END_VALID             0
    #define HTT_RX_IND_PPDU_OFFSET_WORD_START_VALID           0
    A_UINT32 /* word 0 */
        rssi_cmb: 8,
        timestamp_submicrosec: 8,
        phy_err_code: 8,
        phy_err: 1,
        legacy_rate: 4,
        legacy_rate_sel: 1,
        end_valid: 1,
        start_valid: 1;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_RSSI0 1
    union {
        A_UINT32 /* word 1 */
            rssi0_pri20: 8,
            rssi0_ext20: 8,
            rssi0_ext40: 8,
            rssi0_ext80: 8;
       A_UINT32 rssi0; /* access all 20/40/80 per-bandwidth RSSIs together */
    } u0;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_RSSI1 2
    union {
        A_UINT32 /* word 2 */
            rssi1_pri20: 8,
            rssi1_ext20: 8,
            rssi1_ext40: 8,
            rssi1_ext80: 8;
       A_UINT32 rssi1; /* access all 20/40/80 per-bandwidth RSSIs together */
    } u1;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_RSSI2 3
    union {
        A_UINT32 /* word 3 */
            rssi2_pri20: 8,
            rssi2_ext20: 8,
            rssi2_ext40: 8,
            rssi2_ext80: 8;
       A_UINT32 rssi2; /* access all 20/40/80 per-bandwidth RSSIs together */
    } u2;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_RSSI3 4
    union {
        A_UINT32 /* word 4 */
            rssi3_pri20: 8,
            rssi3_ext20: 8,
            rssi3_ext40: 8,
            rssi3_ext80: 8;
       A_UINT32 rssi3; /* access all 20/40/80 per-bandwidth RSSIs together */
    } u3;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_TSF32 5
    A_UINT32 tsf32; /* word 5 */

    #define HTT_RX_IND_PPDU_OFFSET_WORD_TIMESTAMP_MICROSEC 6
    A_UINT32 timestamp_microsec; /* word 6 */

    #define HTT_RX_IND_PPDU_OFFSET_WORD_PREAMBLE_TYPE 7
    #define HTT_RX_IND_PPDU_OFFSET_WORD_VHT_SIG_A1    7
    A_UINT32 /* word 7 */
        vht_sig_a1: 24,
        preamble_type: 8;

    #define HTT_RX_IND_PPDU_OFFSET_WORD_VHT_SIG_A2    8
    A_UINT32 /* word 8 */
        vht_sig_a2: 24,
        reserved0: 8;
} POSTPACK;

#define HTT_RX_PPDU_DESC_BYTES (sizeof(struct htt_rx_ppdu_desc_t))
#define HTT_RX_PPDU_DESC_SIZE32 (HTT_RX_PPDU_DESC_BYTES >> 2)

PREPACK struct htt_rx_ind_hdr_suffix_t
{
    A_UINT32 /* word 0 */
        fw_rx_desc_bytes: 16,
        reserved0: 16;
} POSTPACK;

#define HTT_RX_IND_HDR_SUFFIX_BYTES (sizeof(struct htt_rx_ind_hdr_suffix_t))
#define HTT_RX_IND_HDR_SUFFIX_SIZE32 (HTT_RX_IND_HDR_SUFFIX_BYTES >> 2)

PREPACK struct htt_rx_ind_hdr_t
{
    struct htt_rx_ind_hdr_prefix_t prefix;
    struct htt_rx_ppdu_desc_t      rx_ppdu_desc;
    struct htt_rx_ind_hdr_suffix_t suffix;
} POSTPACK;

#define HTT_RX_IND_HDR_BYTES (sizeof(struct htt_rx_ind_hdr_t))
#define HTT_RX_IND_HDR_SIZE32 (HTT_RX_IND_HDR_BYTES >> 2)

/* confirm that HTT_RX_IND_HDR_BYTES is a multiple of 4 */
A_COMPILE_TIME_ASSERT(HTT_RX_IND_hdr_size_quantum,
    (HTT_RX_IND_HDR_BYTES & 0x3) == 0);

/*
 * HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET:
 * the offset into the HTT rx indication message at which the
 * FW rx PPDU descriptor resides
 */
#define HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET HTT_RX_IND_HDR_PREFIX_BYTES

/*
 * HTT_RX_IND_HDR_SUFFIX_BYTE_OFFSET:
 * the offset into the HTT rx indication message at which the
 * header suffix (FW rx MSDU byte count) resides
 */
#define HTT_RX_IND_HDR_SUFFIX_BYTE_OFFSET \
    (HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET + HTT_RX_PPDU_DESC_BYTES)

/*
 * HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET:
 * the offset into the HTT rx indication message at which the per-MSDU
 * information starts
 * Bytes 0-7 are the message header; bytes 8-11 contain the length of the
 * per-MSDU information portion of the message.  The per-MSDU info itself
 * starts at byte 12.
 */
#define HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET HTT_RX_IND_HDR_BYTES


/**
 * @brief target -> host rx indication message definition
 *
 * @details
 * The following field definitions describe the format of the rx indication
 * message sent from the target to the host.
 * The message consists of three major sections:
 * 1.  a fixed-length header
 * 2.  a variable-length list of firmware rx MSDU descriptors
 * 3.  one or more 4-octet MPDU range information elements
 * The fixed length header itself has two sub-sections
 * 1.  the message meta-information, including identification of the
 *     sender and type of the received data, and a 4-octet flush/release IE
 * 2.  the firmware rx PPDU descriptor
 *
 * The format of the message is depicted below.
 * in this depiction, the following abbreviations are used for information
 * elements within the message:
 *   - SV - start valid: this flag is set if the FW rx PPDU descriptor
 *          elements associated with the PPDU start are valid.
 *          Specifically, the following fields are valid only if SV is set:
 *              RSSI (all variants), L, legacy rate, preamble type, service,
 *              VHT-SIG-A
 *   - EV - end valid: this flag is set if the FW rx PPDU descriptor
 *          elements associated with the PPDU end are valid.
 *          Specifically, the following fields are valid only if EV is set:
 *              P, PHY err code, TSF, microsec / sub-microsec timestamp
 *   - L  - Legacy rate selector - if legacy rates are used, this flag
 *          indicates whether the rate is from a CCK (L == 1) or OFDM
 *          (L == 0) PHY.
 *   - P  - PHY error flag - boolean indication of whether the rx frame had
 *          a PHY error
 *
 * |31            24|23         18|17|16|15|14|13|12|11|10|9|8|7|6|5|4       0|
 * |----------------+-------------------+---------------------+---------------|
 * |                  peer ID           |  |RV|FV| ext TID    |   msg type    |
 * |--------------------------------------------------------------------------|
 * |      num       |   release   |     release     |    flush    |   flush   |
 * |      MPDU      |     end     |      start      |     end     |   start   |
 * |     ranges     |   seq num   |     seq num     |   seq num   |  seq num  |
 * |==========================================================================|
 * |S|E|L| legacy |P|   PHY err code    |     sub-microsec    |    combined   |
 * |V|V| |  rate  | |                   |       timestamp     |       RSSI    |
 * |--------------------------------------------------------------------------|
 * | RSSI rx0 ext80 |  RSSI rx0 ext40   |    RSSI rx0  ext20  | RSSI rx0 pri20|
 * |--------------------------------------------------------------------------|
 * | RSSI rx1 ext80 |  RSSI rx1 ext40   |    RSSI rx1  ext20  | RSSI rx1 pri20|
 * |--------------------------------------------------------------------------|
 * | RSSI rx2 ext80 |  RSSI rx2 ext40   |    RSSI rx2  ext20  | RSSI rx2 pri20|
 * |--------------------------------------------------------------------------|
 * | RSSI rx3 ext80 |  RSSI rx3 ext40   |    RSSI rx3  ext20  | RSSI rx3 pri20|
 * |--------------------------------------------------------------------------|
 * |                                  TSF LSBs                                |
 * |--------------------------------------------------------------------------|
 * |                             microsec timestamp                           |
 * |--------------------------------------------------------------------------|
 * | preamble type  |                    HT-SIG / VHT-SIG-A1                  |
 * |--------------------------------------------------------------------------|
 * |    service     |                    HT-SIG / VHT-SIG-A2                  |
 * |==========================================================================|
 * |             reserved               |          FW rx desc bytes           |
 * |--------------------------------------------------------------------------|
 * |     MSDU Rx    |      MSDU Rx      |        MSDU Rx      |    MSDU Rx    |
 * |     desc B3    |      desc B2      |        desc B1      |    desc B0    |
 * |--------------------------------------------------------------------------|
 * :                                    :                                     :
 * |--------------------------------------------------------------------------|
 * |                          alignment                       |    MSDU Rx    |
 * |                           padding                        |    desc Bn    |
 * |--------------------------------------------------------------------------|
 * |              reserved              |  MPDU range status  |   MPDU count  |
 * |--------------------------------------------------------------------------|
 * :              reserved              :  MPDU range status  :   MPDU count  :
 * :- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - :
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx indication message
 *     Value: 0x1
 *   - EXT_TID
 *     Bits 12:8
 *     Purpose: identify the traffic ID of the rx data, including
 *         special "extended" TID values for multicast, broadcast, and
 *         non-QoS data frames
 *     Value: 0-15 for regular TIDs, or >= 16 for bcast/mcast/non-QoS
 *   - FLUSH_VALID (FV)
 *     Bit 13
 *     Purpose: indicate whether the flush IE (start/end sequence numbers)
 *         is valid
 *     Value:
 *         1 -> flush IE is valid and needs to be processed
 *         0 -> flush IE is not valid and should be ignored
 *   - REL_VALID (RV)
 *     Bit 13
 *     Purpose: indicate whether the release IE (start/end sequence numbers)
 *         is valid
 *     Value:
 *         1 -> release IE is valid and needs to be processed
 *         0 -> release IE is not valid and should be ignored
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: Identify, by ID, which peer sent the rx data
 *     Value: ID of the peer who sent the rx data
 *   - FLUSH_SEQ_NUM_START
 *     Bits 5:0
 *     Purpose: Indicate the start of a series of MPDUs to flush
 *         Not all MPDUs within this series are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 *         This field is only valid if the FV bit is set.
 *     Value:
 *         The sequence number for the first MPDUs to check to flush.
 *         The sequence number is masked by 0x3f.
 *   - FLUSH_SEQ_NUM_END
 *     Bits 11:6
 *     Purpose: Indicate the end of a series of MPDUs to flush
 *     Value:
 *         The sequence number one larger than the sequence number of the
 *         last MPDU to check to flush.
 *         The sequence number is masked by 0x3f.
 *         Not all MPDUs within this series are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 *         This field is only valid if the FV bit is set.
 *   - REL_SEQ_NUM_START
 *     Bits 17:12
 *     Purpose: Indicate the start of a series of MPDUs to release.
 *         All MPDUs within this series are present and valid - the host
 *         need not check each sequence number within this range to see if
 *         the corresponding MPDU is actually present.
 *         This field is only valid if the RV bit is set.
 *     Value:
 *         The sequence number for the first MPDUs to check to release.
 *         The sequence number is masked by 0x3f.
 *   - REL_SEQ_NUM_END
 *     Bits 23:18
 *     Purpose: Indicate the end of a series of MPDUs to release.
 *     Value:
 *         The sequence number one larger than the sequence number of the
 *         last MPDU to check to release.
 *         The sequence number is masked by 0x3f.
 *         All MPDUs within this series are present and valid - the host
 *         need not check each sequence number within this range to see if
 *         the corresponding MPDU is actually present.
 *         This field is only valid if the RV bit is set.
 *   - NUM_MPDU_RANGES
 *     Bits 31:24
 *     Purpose: Indicate how many ranges of MPDUs are present.
 *         Each MPDU range consists of a series of contiguous MPDUs within the
 *         rx frame sequence which all have the same MPDU status.
 *     Value: 1-63 (typically a small number, like 1-3)
 *
 * Rx PPDU descriptor fields:
 *   - RSSI_CMB
 *     Bits 7:0
 *     Purpose: Combined RSSI from all active rx chains, across the active
 *         bandwidth.
 *     Value: RSSI dB units w.r.t. noise floor
 *   - TIMESTAMP_SUBMICROSEC
 *     Bits 15:8
 *     Purpose: high-resolution timestamp
 *     Value:
 *         Sub-microsecond time of PPDU reception.
 *         This timestamp ranges from [0,MAC clock MHz).
 *         This timestamp can be used in conjunction with TIMESTAMP_MICROSEC
 *         to form a high-resolution, large range rx timestamp.
 *   - PHY_ERR_CODE
 *     Bits 23:16
 *     Purpose:
 *         If the rx frame processing resulted in a PHY error, indicate what
 *         type of rx PHY error occurred.
 *     Value:
 *         This field is valid if the "P" (PHY_ERR) flag is set.
 *         TBD: document/specify the values for this field
 *   - PHY_ERR
 *     Bit 24
 *     Purpose: indicate whether the rx PPDU had a PHY error
 *     Value: 0 -> no rx PHY error, 1 -> rx PHY error encountered
 *   - LEGACY_RATE
 *     Bits 28:25
 *     Purpose:
 *         If the rx frame used a legacy rate rather than a HT or VHT rate,
 *         specify which rate was used.
 *     Value:
 *         The LEGACY_RATE field's value depends on the "L" (LEGACY_RATE_SEL)
 *         flag.
 *         If LEGACY_RATE_SEL is 0:
 *             0x8: OFDM 48 Mbps
 *             0x9: OFDM 24 Mbps
 *             0xA: OFDM 12 Mbps
 *             0xB: OFDM 6 Mbps
 *             0xC: OFDM 54 Mbps
 *             0xD: OFDM 36 Mbps
 *             0xE: OFDM 18 Mbps
 *             0xF: OFDM 9 Mbps
 *         If LEGACY_RATE_SEL is 1:
 *             0x8: CCK 11 Mbps long preamble
 *             0x9: CCK 5.5 Mbps long preamble
 *             0xA: CCK 2 Mbps long preamble
 *             0xB: CCK 1 Mbps long preamble
 *             0xC: CCK 11 Mbps short preamble
 *             0xD: CCK 5.5 Mbps short preamble
 *             0xE: CCK 2 Mbps short preamble
 *   - LEGACY_RATE_SEL
 *     Bit 29
 *     Purpose: if rx used a legacy rate, specify whether it was OFDM or CCK
 *     Value:
 *         This field is valid if the PREAMBLE_TYPE field indicates the rx
 *         used a legacy rate.
 *         0 -> OFDM, 1 -> CCK
 *   - END_VALID
 *     Bit 30
 *     Purpose: Indicate whether the FW rx PPDU desc fields associated with
 *         the start of the PPDU are valid.  Specifically, the following
 *         fields are only valid if END_VALID is set:
 *         PHY_ERR, PHY_ERR_CODE, TSF32, TIMESTAMP_MICROSEC,
 *         TIMESTAMP_SUBMICROSEC
 *     Value:
 *         0 -> rx PPDU desc end fields are not valid
 *         1 -> rx PPDU desc end fields are valid
 *   - START_VALID
 *     Bit 31
 *     Purpose: Indicate whether the FW rx PPDU desc fields associated with
 *         the end of the PPDU are valid.  Specifically, the following
 *         fields are only valid if START_VALID is set:
 *         RSSI, LEGACY_RATE_SEL, LEGACY_RATE, PREAMBLE_TYPE, SERVICE,
 *         VHT-SIG-A
 *     Value:
 *         0 -> rx PPDU desc start fields are not valid
 *         1 -> rx PPDU desc start fields are valid
 *   - RSSI0_PRI20
 *     Bits 7:0
 *     Purpose: RSSI from chain 0 on the primary 20 MHz channel
 *     Value: RSSI dB units w.r.t. noise floor
 *
 *   - RSSI0_EXT20
 *     Bits 7:0
 *     Purpose: RSSI from chain 0 on the bonded extension 20 MHz channel
 *         (if the rx bandwidth was >= 40 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI0_EXT40
 *     Bits 7:0
 *     Purpose: RSSI from chain 0 on the bonded extension 40 MHz channel
 *         (if the rx bandwidth was >= 80 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI0_EXT80
 *     Bits 7:0
 *     Purpose: RSSI from chain 0 on the bonded extension 80 MHz channel
 *         (if the rx bandwidth was >= 160 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *
 *   - RSSI1_PRI20
 *     Bits 7:0
 *     Purpose: RSSI from chain 1 on the primary 20 MHz channel
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI1_EXT20
 *     Bits 7:0
 *     Purpose: RSSI from chain 1 on the bonded extension 20 MHz channel
 *         (if the rx bandwidth was >= 40 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI1_EXT40
 *     Bits 7:0
 *     Purpose: RSSI from chain 1 on the bonded extension 40 MHz channel
 *         (if the rx bandwidth was >= 80 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI1_EXT80
 *     Bits 7:0
 *     Purpose: RSSI from chain 1 on the bonded extension 80 MHz channel
 *         (if the rx bandwidth was >= 160 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *
 *   - RSSI2_PRI20
 *     Bits 7:0
 *     Purpose: RSSI from chain 2 on the primary 20 MHz channel
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI2_EXT20
 *     Bits 7:0
 *     Purpose: RSSI from chain 2 on the bonded extension 20 MHz channel
 *         (if the rx bandwidth was >= 40 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI2_EXT40
 *     Bits 7:0
 *     Purpose: RSSI from chain 2 on the bonded extension 40 MHz channel
 *         (if the rx bandwidth was >= 80 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI2_EXT80
 *     Bits 7:0
 *     Purpose: RSSI from chain 2 on the bonded extension 80 MHz channel
 *         (if the rx bandwidth was >= 160 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *
 *   - RSSI3_PRI20
 *     Bits 7:0
 *     Purpose: RSSI from chain 3 on the primary 20 MHz channel
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI3_EXT20
 *     Bits 7:0
 *     Purpose: RSSI from chain 3 on the bonded extension 20 MHz channel
 *         (if the rx bandwidth was >= 40 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI3_EXT40
 *     Bits 7:0
 *     Purpose: RSSI from chain 3 on the bonded extension 40 MHz channel
 *         (if the rx bandwidth was >= 80 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *   - RSSI3_EXT80
 *     Bits 7:0
 *     Purpose: RSSI from chain 3 on the bonded extension 80 MHz channel
 *         (if the rx bandwidth was >= 160 MHz)
 *     Value: RSSI dB units w.r.t. noise floor
 *
 *   - TSF32
 *     Bits 31:0
 *     Purpose: specify the time the rx PPDU was received, in TSF units
 *     Value: 32 LSBs of the TSF
 *   - TIMESTAMP_MICROSEC
 *     Bits 31:0
 *     Purpose: specify the time the rx PPDU was received, in microsecond units
 *     Value: PPDU rx time, in microseconds
 *   - VHT_SIG_A1
 *     Bits 23:0
 *     Purpose: Provide the HT-SIG (initial 24 bits) or VHT-SIG-A1 field
 *         from the rx PPDU
 *     Value:
 *         If PREAMBLE_TYPE specifies VHT, then this field contains the
 *         VHT-SIG-A1 data.
 *         If PREAMBLE_TYPE specifies HT, then this field contains the
 *         first 24 bits of the HT-SIG data.
 *         Otherwise, this field is invalid.
 *         Refer to the the 802.11 protocol for the definition of the
 *         HT-SIG and VHT-SIG-A1 fields
 *   - VHT_SIG_A2
 *     Bits 23:0
 *     Purpose: Provide the HT-SIG (final 24 bits) or VHT-SIG-A2 field
 *         from the rx PPDU
 *     Value:
 *         If PREAMBLE_TYPE specifies VHT, then this field contains the
 *         VHT-SIG-A2 data.
 *         If PREAMBLE_TYPE specifies HT, then this field contains the
 *         last 24 bits of the HT-SIG data.
 *         Otherwise, this field is invalid.
 *         Refer to the the 802.11 protocol for the definition of the
 *         HT-SIG and VHT-SIG-A2 fields
 *   - PREAMBLE_TYPE
 *     Bits 31:24
 *     Purpose: indicate the PHY format of the received burst
 *     Value:
 *         0x4: Legacy (OFDM/CCK)
 *         0x8: HT
 *         0x9: HT with TxBF
 *         0xC: VHT
 *         0xD: VHT with TxBF
 *   - SERVICE
 *     Bits 31:24
 *     Purpose: TBD
 *     Value: TBD
 *
 * Rx MSDU descriptor fields:
 *   - FW_RX_DESC_BYTES
 *     Bits 15:0
 *     Purpose: Indicate how many bytes in the Rx indication are used for
 *         FW Rx descriptors
 *
 * Payload fields:
 *   - MPDU_COUNT
 *     Bits 7:0
 *     Purpose: Indicate how many sequential MPDUs share the same status.
 *         All MPDUs within the indicated list are from the same RA-TA-TID.
 *   - MPDU_STATUS
 *     Bits 15:8
 *     Purpose: Indicate whether the (group of sequential) MPDU(s) were
 *         received successfully.
 *     Value:
 *         0x1: success
 *         0x2: FCS error
 *         0x3: duplicate error
 *         0x4: replay error
 *         0x5: invalid peer
 */
/* header fields */
#define HTT_RX_IND_EXT_TID_M      0x1f00
#define HTT_RX_IND_EXT_TID_S      8
#define HTT_RX_IND_FLUSH_VALID_M  0x2000
#define HTT_RX_IND_FLUSH_VALID_S  13
#define HTT_RX_IND_REL_VALID_M    0x4000
#define HTT_RX_IND_REL_VALID_S    14
#define HTT_RX_IND_PEER_ID_M      0xffff0000
#define HTT_RX_IND_PEER_ID_S      16

#define HTT_RX_IND_FLUSH_SEQ_NUM_START_M 0x3f
#define HTT_RX_IND_FLUSH_SEQ_NUM_START_S 0
#define HTT_RX_IND_FLUSH_SEQ_NUM_END_M   0xfc0
#define HTT_RX_IND_FLUSH_SEQ_NUM_END_S   6
#define HTT_RX_IND_REL_SEQ_NUM_START_M   0x3f000
#define HTT_RX_IND_REL_SEQ_NUM_START_S   12
#define HTT_RX_IND_REL_SEQ_NUM_END_M     0xfc0000
#define HTT_RX_IND_REL_SEQ_NUM_END_S     18
#define HTT_RX_IND_NUM_MPDU_RANGES_M     0xff000000
#define HTT_RX_IND_NUM_MPDU_RANGES_S     24

/* rx PPDU descriptor fields */
#define HTT_RX_IND_RSSI_CMB_M              0x000000ff
#define HTT_RX_IND_RSSI_CMB_S              0
#define HTT_RX_IND_TIMESTAMP_SUBMICROSEC_M 0x0000ff00
#define HTT_RX_IND_TIMESTAMP_SUBMICROSEC_S 8
#define HTT_RX_IND_PHY_ERR_CODE_M          0x00ff0000
#define HTT_RX_IND_PHY_ERR_CODE_S          16
#define HTT_RX_IND_PHY_ERR_M               0x01000000
#define HTT_RX_IND_PHY_ERR_S               24
#define HTT_RX_IND_LEGACY_RATE_M           0x1e000000
#define HTT_RX_IND_LEGACY_RATE_S           25
#define HTT_RX_IND_LEGACY_RATE_SEL_M       0x20000000
#define HTT_RX_IND_LEGACY_RATE_SEL_S       29
#define HTT_RX_IND_END_VALID_M             0x40000000
#define HTT_RX_IND_END_VALID_S             30
#define HTT_RX_IND_START_VALID_M           0x80000000
#define HTT_RX_IND_START_VALID_S           31

#define HTT_RX_IND_RSSI_PRI20_M            0x000000ff
#define HTT_RX_IND_RSSI_PRI20_S            0
#define HTT_RX_IND_RSSI_EXT20_M            0x0000ff00
#define HTT_RX_IND_RSSI_EXT20_S            8
#define HTT_RX_IND_RSSI_EXT40_M            0x00ff0000
#define HTT_RX_IND_RSSI_EXT40_S            16
#define HTT_RX_IND_RSSI_EXT80_M            0xff000000
#define HTT_RX_IND_RSSI_EXT80_S            24

#define HTT_RX_IND_VHT_SIG_A1_M            0x00ffffff
#define HTT_RX_IND_VHT_SIG_A1_S            0
#define HTT_RX_IND_VHT_SIG_A2_M            0x00ffffff
#define HTT_RX_IND_VHT_SIG_A2_S            0
#define HTT_RX_IND_PREAMBLE_TYPE_M         0xff000000
#define HTT_RX_IND_PREAMBLE_TYPE_S         24
#define HTT_RX_IND_SERVICE_M               0xff000000
#define HTT_RX_IND_SERVICE_S               24

/* rx MSDU descriptor fields */
#define HTT_RX_IND_FW_RX_DESC_BYTES_M   0xffff
#define HTT_RX_IND_FW_RX_DESC_BYTES_S   0

/* payload fields */
#define HTT_RX_IND_MPDU_COUNT_M    0xff
#define HTT_RX_IND_MPDU_COUNT_S    0
#define HTT_RX_IND_MPDU_STATUS_M   0xff00
#define HTT_RX_IND_MPDU_STATUS_S   8


#define HTT_RX_IND_EXT_TID_SET(word, value)                              \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_EXT_TID, value);                    \
        (word) |= (value)  << HTT_RX_IND_EXT_TID_S;                      \
    } while (0)
#define HTT_RX_IND_EXT_TID_GET(word) \
    (((word) & HTT_RX_IND_EXT_TID_M) >> HTT_RX_IND_EXT_TID_S)

#define HTT_RX_IND_FLUSH_VALID_SET(word, value)                          \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_FLUSH_VALID, value);                \
        (word) |= (value)  << HTT_RX_IND_FLUSH_VALID_S;                  \
    } while (0)
#define HTT_RX_IND_FLUSH_VALID_GET(word) \
    (((word) & HTT_RX_IND_FLUSH_VALID_M) >> HTT_RX_IND_FLUSH_VALID_S)

#define HTT_RX_IND_REL_VALID_SET(word, value)                            \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_REL_VALID, value);                  \
        (word) |= (value)  << HTT_RX_IND_REL_VALID_S;                    \
    } while (0)
#define HTT_RX_IND_REL_VALID_GET(word) \
    (((word) & HTT_RX_IND_REL_VALID_M) >> HTT_RX_IND_REL_VALID_S)

#define HTT_RX_IND_PEER_ID_SET(word, value)                              \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_PEER_ID, value);                    \
        (word) |= (value)  << HTT_RX_IND_PEER_ID_S;                      \
    } while (0)
#define HTT_RX_IND_PEER_ID_GET(word) \
    (((word) & HTT_RX_IND_PEER_ID_M) >> HTT_RX_IND_PEER_ID_S)


#define HTT_RX_IND_FW_RX_DESC_BYTES_SET(word, value)                     \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_FW_RX_DESC_BYTES, value);           \
        (word) |= (value)  << HTT_RX_IND_FW_RX_DESC_BYTES_S;             \
    } while (0)
#define HTT_RX_IND_FW_RX_DESC_BYTES_GET(word) \
    (((word) & HTT_RX_IND_FW_RX_DESC_BYTES_M) >> HTT_RX_IND_FW_RX_DESC_BYTES_S)


#define HTT_RX_IND_FLUSH_SEQ_NUM_START_SET(word, value)              \
    do {                                                             \
        HTT_CHECK_SET_VAL(HTT_RX_IND_FLUSH_SEQ_NUM_START, value);    \
        (word) |= (value)  << HTT_RX_IND_FLUSH_SEQ_NUM_START_S;      \
    } while (0)
#define HTT_RX_IND_FLUSH_SEQ_NUM_START_GET(word)                     \
     (((word) & HTT_RX_IND_FLUSH_SEQ_NUM_START_M) >>                 \
      HTT_RX_IND_FLUSH_SEQ_NUM_START_S)

#define HTT_RX_IND_FLUSH_SEQ_NUM_END_SET(word, value)                \
    do {                                                             \
        HTT_CHECK_SET_VAL(HTT_RX_IND_FLUSH_SEQ_NUM_END, value);      \
        (word) |= (value)  << HTT_RX_IND_FLUSH_SEQ_NUM_END_S;        \
    } while (0)
#define HTT_RX_IND_FLUSH_SEQ_NUM_END_GET(word)                       \
    (((word) & HTT_RX_IND_FLUSH_SEQ_NUM_END_M) >>                    \
    HTT_RX_IND_FLUSH_SEQ_NUM_END_S)

#define HTT_RX_IND_REL_SEQ_NUM_START_SET(word, value)                \
    do {                                                             \
        HTT_CHECK_SET_VAL(HTT_RX_IND_REL_SEQ_NUM_START, value);      \
        (word) |= (value)  << HTT_RX_IND_REL_SEQ_NUM_START_S;        \
    } while (0)
#define HTT_RX_IND_REL_SEQ_NUM_START_GET(word)                       \
     (((word) & HTT_RX_IND_REL_SEQ_NUM_START_M) >>                   \
      HTT_RX_IND_REL_SEQ_NUM_START_S)

#define HTT_RX_IND_REL_SEQ_NUM_END_SET(word, value)                  \
    do {                                                             \
        HTT_CHECK_SET_VAL(HTT_RX_IND_REL_SEQ_NUM_END, value);        \
        (word) |= (value)  << HTT_RX_IND_REL_SEQ_NUM_END_S;          \
    } while (0)
#define HTT_RX_IND_REL_SEQ_NUM_END_GET(word)                         \
    (((word) & HTT_RX_IND_REL_SEQ_NUM_END_M) >>                      \
    HTT_RX_IND_REL_SEQ_NUM_END_S)

#define HTT_RX_IND_NUM_MPDU_RANGES_SET(word, value)                  \
    do {                                                             \
        HTT_CHECK_SET_VAL(HTT_RX_IND_NUM_MPDU_RANGES, value);        \
        (word) |= (value)  << HTT_RX_IND_NUM_MPDU_RANGES_S;          \
    } while (0)
#define HTT_RX_IND_NUM_MPDU_RANGES_GET(word)                         \
    (((word) & HTT_RX_IND_NUM_MPDU_RANGES_M) >>                      \
    HTT_RX_IND_NUM_MPDU_RANGES_S)

/* FW rx PPDU descriptor fields */
#define HTT_RX_IND_RSSI_CMB_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_RX_IND_RSSI_CMB, value); \
        (word) |= (value)  << HTT_RX_IND_RSSI_CMB_S;   \
    } while (0)
#define HTT_RX_IND_RSSI_CMB_GET(word)    \
    (((word) & HTT_RX_IND_RSSI_CMB_M) >> \
    HTT_RX_IND_RSSI_CMB_S)

#define HTT_RX_IND_TIMESTAMP_SUBMICROSEC_SET(word, value)           \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_IND_TIMESTAMP_SUBMICROSEC, value); \
        (word) |= (value)  << HTT_RX_IND_TIMESTAMP_SUBMICROSEC_S;   \
    } while (0)
#define HTT_RX_IND_TIMESTAMP_SUBMICROSEC_GET(word)    \
    (((word) & HTT_RX_IND_TIMESTAMP_SUBMICROSEC_M) >> \
    HTT_RX_IND_TIMESTAMP_SUBMICROSEC_S)

#define HTT_RX_IND_PHY_ERR_CODE_SET(word, value)           \
    do {                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IND_PHY_ERR_CODE, value); \
        (word) |= (value)  << HTT_RX_IND_PHY_ERR_CODE_S;   \
    } while (0)
#define HTT_RX_IND_PHY_ERR_CODE_GET(word)    \
    (((word) & HTT_RX_IND_PHY_ERR_CODE_M) >> \
    HTT_RX_IND_PHY_ERR_CODE_S)

#define HTT_RX_IND_PHY_ERR_SET(word, value)           \
    do {                                                   \
        HTT_CHECK_SET_VAL(HTT_RX_IND_PHY_ERR, value); \
        (word) |= (value)  << HTT_RX_IND_PHY_ERR_S;   \
    } while (0)
#define HTT_RX_IND_PHY_ERR_GET(word)    \
    (((word) & HTT_RX_IND_PHY_ERR_M) >> \
    HTT_RX_IND_PHY_ERR_S)

#define HTT_RX_IND_LEGACY_RATE_SET(word, value)           \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IND_LEGACY_RATE, value); \
        (word) |= (value)  << HTT_RX_IND_LEGACY_RATE_S;   \
    } while (0)
#define HTT_RX_IND_LEGACY_RATE_GET(word)    \
    (((word) & HTT_RX_IND_LEGACY_RATE_M) >> \
    HTT_RX_IND_LEGACY_RATE_S)

#define HTT_RX_IND_LEGACY_RATE_SEL_SET(word, value)           \
    do {                                                           \
        HTT_CHECK_SET_VAL(HTT_RX_IND_LEGACY_RATE_SEL, value); \
        (word) |= (value)  << HTT_RX_IND_LEGACY_RATE_SEL_S;   \
    } while (0)
#define HTT_RX_IND_LEGACY_RATE_SEL_GET(word)    \
    (((word) & HTT_RX_IND_LEGACY_RATE_SEL_M) >> \
    HTT_RX_IND_LEGACY_RATE_SEL_S)

#define HTT_RX_IND_END_VALID_SET(word, value)           \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_IND_END_VALID, value); \
        (word) |= (value)  << HTT_RX_IND_END_VALID_S;   \
    } while (0)
#define HTT_RX_IND_END_VALID_GET(word)    \
    (((word) & HTT_RX_IND_END_VALID_M) >> \
    HTT_RX_IND_END_VALID_S)

#define HTT_RX_IND_START_VALID_SET(word, value)           \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IND_START_VALID, value); \
        (word) |= (value)  << HTT_RX_IND_START_VALID_S;   \
    } while (0)
#define HTT_RX_IND_START_VALID_GET(word)    \
    (((word) & HTT_RX_IND_START_VALID_M) >> \
    HTT_RX_IND_START_VALID_S)

#define HTT_RX_IND_RSSI_PRI20_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_RSSI_PRI20, value); \
        (word) |= (value)  << HTT_RX_IND_RSSI_PRI20_S;   \
    } while (0)
#define HTT_RX_IND_RSSI_PRI20_GET(word)    \
    (((word) & HTT_RX_IND_RSSI_PRI20_M) >> \
    HTT_RX_IND_RSSI_PRI20_S)

#define HTT_RX_IND_RSSI_EXT20_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_RSSI_EXT20, value); \
        (word) |= (value)  << HTT_RX_IND_RSSI_EXT20_S;   \
    } while (0)
#define HTT_RX_IND_RSSI_EXT20_GET(word)    \
    (((word) & HTT_RX_IND_RSSI_EXT20_M) >> \
    HTT_RX_IND_RSSI_EXT20_S)

#define HTT_RX_IND_RSSI_EXT40_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_RSSI_EXT40, value); \
        (word) |= (value)  << HTT_RX_IND_RSSI_EXT40_S;   \
    } while (0)
#define HTT_RX_IND_RSSI_EXT40_GET(word)    \
    (((word) & HTT_RX_IND_RSSI_EXT40_M) >> \
    HTT_RX_IND_RSSI_EXT40_S)

#define HTT_RX_IND_RSSI_EXT80_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_RSSI_EXT80, value); \
        (word) |= (value)  << HTT_RX_IND_RSSI_EXT80_S;   \
    } while (0)
#define HTT_RX_IND_RSSI_EXT80_GET(word)    \
    (((word) & HTT_RX_IND_RSSI_EXT80_M) >> \
    HTT_RX_IND_RSSI_EXT80_S)

#define HTT_RX_IND_VHT_SIG_A1_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_VHT_SIG_A1, value); \
        (word) |= (value)  << HTT_RX_IND_VHT_SIG_A1_S;   \
    } while (0)
#define HTT_RX_IND_VHT_SIG_A1_GET(word)    \
    (((word) & HTT_RX_IND_VHT_SIG_A1_M) >> \
    HTT_RX_IND_VHT_SIG_A1_S)

#define HTT_RX_IND_VHT_SIG_A2_SET(word, value)           \
    do {                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_IND_VHT_SIG_A2, value); \
        (word) |= (value)  << HTT_RX_IND_VHT_SIG_A2_S;   \
    } while (0)
#define HTT_RX_IND_VHT_SIG_A2_GET(word)    \
    (((word) & HTT_RX_IND_VHT_SIG_A2_M) >> \
    HTT_RX_IND_VHT_SIG_A2_S)

#define HTT_RX_IND_PREAMBLE_TYPE_SET(word, value)           \
    do {                                                    \
        HTT_CHECK_SET_VAL(HTT_RX_IND_PREAMBLE_TYPE, value); \
        (word) |= (value)  << HTT_RX_IND_PREAMBLE_TYPE_S;   \
    } while (0)
#define HTT_RX_IND_PREAMBLE_TYPE_GET(word)    \
    (((word) & HTT_RX_IND_PREAMBLE_TYPE_M) >> \
    HTT_RX_IND_PREAMBLE_TYPE_S)

#define HTT_RX_IND_SERVICE_SET(word, value)           \
    do {                                              \
        HTT_CHECK_SET_VAL(HTT_RX_IND_SERVICE, value); \
        (word) |= (value)  << HTT_RX_IND_SERVICE_S;   \
    } while (0)
#define HTT_RX_IND_SERVICE_GET(word)    \
    (((word) & HTT_RX_IND_SERVICE_M) >> \
    HTT_RX_IND_SERVICE_S)


#define HTT_RX_IND_MPDU_COUNT_SET(word, value)                          \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_IND_MPDU_COUNT, value);                \
        (word) |= (value)  << HTT_RX_IND_MPDU_COUNT_S;                  \
    } while (0)
#define HTT_RX_IND_MPDU_COUNT_GET(word) \
    (((word) & HTT_RX_IND_MPDU_COUNT_M) >> HTT_RX_IND_MPDU_COUNT_S)

#define HTT_RX_IND_MPDU_STATUS_SET(word, value)                         \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_IND_MPDU_STATUS, value);               \
        (word) |= (value)  << HTT_RX_IND_MPDU_STATUS_S;                 \
    } while (0)
#define HTT_RX_IND_MPDU_STATUS_GET(word) \
    (((word) & HTT_RX_IND_MPDU_STATUS_M) >> HTT_RX_IND_MPDU_STATUS_S)


#define HTT_RX_IND_HL_BYTES                               \
    (HTT_RX_IND_HDR_BYTES +                               \
     4 /* single FW rx MSDU descriptor, plus padding */ + \
     4 /* single MPDU range information element */)
#define HTT_RX_IND_HL_SIZE32 (HTT_RX_IND_HL_BYTES >> 2)

// Could we use one macro entry?
#define HTT_WORD_SET(word, field, value) \
    do { \
        HTT_CHECK_SET_VAL(field, value); \
        (word) |= ((value) << field ## _S); \
    } while (0)
#define HTT_WORD_GET(word, field) \
    (((word) & field ## _M) >> field ## _S)

PREPACK struct hl_htt_rx_ind_base {
    A_UINT32 rx_ind_msg[HTT_RX_IND_HL_SIZE32];    /* algin with LL case rx indication message, but reduced to 5 words */
} POSTPACK;

/*
 * HTT_RX_IND_HL_RX_DESC_BASE_OFFSET
 * Currently, we use a resv field in hl_htt_rx_ind_base to store some
 * HL host needed info. The field is just after the msdu fw rx desc.
 */
#define HTT_RX_IND_HL_RX_DESC_BASE_OFFSET (HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET + 1)
struct htt_rx_ind_hl_rx_desc_t {
    A_UINT8 ver;
    A_UINT8 len;
    struct {
        A_UINT8
            first_msdu: 1,
            last_msdu: 1,
            c3_failed: 1,
            c4_failed: 1,
            ipv6: 1,
            tcp: 1,
            udp: 1,
            reserved: 1;
    } flags;
};

#define HTT_RX_IND_HL_RX_DESC_VER_OFFSET \
    (HTT_RX_IND_HL_RX_DESC_BASE_OFFSET \
     + offsetof(struct htt_rx_ind_hl_rx_desc_t, ver))
#define HTT_RX_IND_HL_RX_DESC_VER 0

#define HTT_RX_IND_HL_RX_DESC_LEN_OFFSET \
    (HTT_RX_IND_HL_RX_DESC_BASE_OFFSET \
     + offsetof(struct htt_rx_ind_hl_rx_desc_t, len))

#define HTT_RX_IND_HL_FLAG_OFFSET \
    (HTT_RX_IND_HL_RX_DESC_BASE_OFFSET \
     + offsetof(struct htt_rx_ind_hl_rx_desc_t, flags))

#define HTT_RX_IND_HL_FLAG_FIRST_MSDU   (0x01 << 0)
#define HTT_RX_IND_HL_FLAG_LAST_MSDU    (0x01 << 1)
#define HTT_RX_IND_HL_FLAG_C3_FAILED    (0x01 << 2) // L3 checksum failed
#define HTT_RX_IND_HL_FLAG_C4_FAILED    (0x01 << 3) // L4 checksum failed
#define HTT_RX_IND_HL_FLAG_IPV6         (0x01 << 4) // is ipv6, or else ipv4
#define HTT_RX_IND_HL_FLAG_TCP          (0x01 << 5) // is tcp
#define HTT_RX_IND_HL_FLAG_UDP          (0x01 << 6) // is udp
/* This structure is used in HL, the basic descriptor information
 * used by host. the structure is translated by FW from HW desc
 * or generated by FW. But in HL monitor mode, the host would use
 * the same structure with LL.
 */
PREPACK struct hl_htt_rx_desc_base {
    A_UINT32
        seq_num:12,
        encrypted:1,
        resv0:3,
        mcast_bcast:1,
        fragment:1,
        key_id_oct:8,
        resv1:6;
    A_UINT32
        pn_31_0;
    union {
        struct {
            A_UINT16 pn_47_32;
            A_UINT16 pn_63_48;
        } pn16;
        A_UINT32 pn_63_32;
    } u0;
    A_UINT32
        pn_95_64;
    A_UINT32
        pn_127_96;
} POSTPACK;

#define HL_RX_DESC_SIZE         (sizeof(struct hl_htt_rx_desc_base))
#define HL_RX_DESC_SIZE_DWORD   (HL_RX_STD_DESC_SIZE >> 2)

#define HTT_HL_RX_DESC_MPDU_SEQ_NUM_M       0xfff
#define HTT_HL_RX_DESC_MPDU_SEQ_NUM_S       0
#define HTT_HL_RX_DESC_MPDU_ENC_M           0x1000
#define HTT_HL_RX_DESC_MPDU_ENC_S           12
#define HTT_HL_RX_DESC_MCAST_BCAST_M        0x10000
#define HTT_HL_RX_DESC_MCAST_BCAST_S        16
#define HTT_HL_RX_DESC_FRAGMENT_M           0x20000
#define HTT_HL_RX_DESC_FRAGMENT_S           17
#define HTT_HL_RX_DESC_KEY_ID_OCT_M         0x3fc0000
#define HTT_HL_RX_DESC_KEY_ID_OCT_S         18

#define HTT_HL_RX_DESC_PN_OFFSET            offsetof(struct hl_htt_rx_desc_base, pn_31_0)
#define HTT_HL_RX_DESC_PN_WORD_OFFSET       (HTT_HL_RX_DESC_PN_OFFSET >> 2)

/*
 * @brief target -> host rx reorder flush message definition
 *
 * @details
 * The following field definitions describe the format of the rx flush
 * message sent from the target to the host.
 * The message consists of a 4-octet header, followed by one or more
 * 4-octet payload information elements.
 *
 *     |31           24|23                            8|7            0|
 *     |--------------------------------------------------------------|
 *     |       TID     |          peer ID              |   msg type   |
 *     |--------------------------------------------------------------|
 *     |  seq num end  | seq num start |  MPDU status  |   reserved   |
 *     |--------------------------------------------------------------|
 * First DWORD:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx flush message
 *     Value: 0x2
 *   - PEER_ID
 *     Bits 23:8 (only bits 18:8 actually used)
 *     Purpose: identify which peer's rx data is being flushed
 *     Value: (rx) peer ID
 *   - TID
 *     Bits 31:24 (only bits 27:24 actually used)
 *     Purpose: Specifies which traffic identifier's rx data is being flushed
 *     Value: traffic identifier
 * Second DWORD:
 *   - MPDU_STATUS
 *     Bits 15:8
 *     Purpose:
 *         Indicate whether the flushed MPDUs should be discarded or processed.
 *     Value:
 *         0x1:   send the MPDUs from the rx reorder buffer to subsequent
 *                stages of rx processing
 *         other: discard the MPDUs
 *         It is anticipated that flush messages will always have
 *         MPDU status == 1, but the status flag is included for
 *         flexibility.
 *   - SEQ_NUM_START
 *     Bits 23:16
 *     Purpose:
 *         Indicate the start of a series of consecutive MPDUs being flushed.
 *         Not all MPDUs within this range are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 *     Value:
 *         The sequence number for the first MPDU in the sequence.
 *         This sequence number is the 6 LSBs of the 802.11 sequence number.
 *   - SEQ_NUM_END
 *     Bits 30:24
 *     Purpose:
 *         Indicate the end of a series of consecutive MPDUs being flushed.
 *     Value:
 *         The sequence number one larger than the sequence number of the
 *         last MPDU being flushed.
 *         This sequence number is the 6 LSBs of the 802.11 sequence number.
 *         The range of MPDUs from [SEQ_NUM_START,SEQ_NUM_END-1] inclusive
 *         are to be released for further rx processing.
 *         Not all MPDUs within this range are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 */
/* first DWORD */
#define HTT_RX_FLUSH_PEER_ID_M  0xffff00
#define HTT_RX_FLUSH_PEER_ID_S  8
#define HTT_RX_FLUSH_TID_M      0xff000000
#define HTT_RX_FLUSH_TID_S      24
/* second DWORD */
#define HTT_RX_FLUSH_MPDU_STATUS_M   0x0000ff00
#define HTT_RX_FLUSH_MPDU_STATUS_S   8
#define HTT_RX_FLUSH_SEQ_NUM_START_M 0x00ff0000
#define HTT_RX_FLUSH_SEQ_NUM_START_S 16
#define HTT_RX_FLUSH_SEQ_NUM_END_M   0xff000000
#define HTT_RX_FLUSH_SEQ_NUM_END_S   24

#define HTT_RX_FLUSH_BYTES 8

#define HTT_RX_FLUSH_PEER_ID_SET(word, value)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_FLUSH_PEER_ID, value);                 \
        (word) |= (value)  << HTT_RX_FLUSH_PEER_ID_S;                   \
    } while (0)
#define HTT_RX_FLUSH_PEER_ID_GET(word) \
    (((word) & HTT_RX_FLUSH_PEER_ID_M) >> HTT_RX_FLUSH_PEER_ID_S)

#define HTT_RX_FLUSH_TID_SET(word, value)                               \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_FLUSH_TID, value);                     \
        (word) |= (value)  << HTT_RX_FLUSH_TID_S;                       \
    } while (0)
#define HTT_RX_FLUSH_TID_GET(word) \
    (((word) & HTT_RX_FLUSH_TID_M) >> HTT_RX_FLUSH_TID_S)

#define HTT_RX_FLUSH_MPDU_STATUS_SET(word, value)                       \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_FLUSH_MPDU_STATUS, value);             \
        (word) |= (value)  << HTT_RX_FLUSH_MPDU_STATUS_S;               \
    } while (0)
#define HTT_RX_FLUSH_MPDU_STATUS_GET(word) \
    (((word) & HTT_RX_FLUSH_MPDU_STATUS_M) >> HTT_RX_FLUSH_MPDU_STATUS_S)

#define HTT_RX_FLUSH_SEQ_NUM_START_SET(word, value)                     \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_FLUSH_SEQ_NUM_START, value);           \
        (word) |= (value)  << HTT_RX_FLUSH_SEQ_NUM_START_S;             \
    } while (0)
#define HTT_RX_FLUSH_SEQ_NUM_START_GET(word) \
    (((word) & HTT_RX_FLUSH_SEQ_NUM_START_M) >> HTT_RX_FLUSH_SEQ_NUM_START_S)

#define HTT_RX_FLUSH_SEQ_NUM_END_SET(word, value)                       \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_FLUSH_SEQ_NUM_END, value);             \
        (word) |= (value)  << HTT_RX_FLUSH_SEQ_NUM_END_S;               \
    } while (0)
#define HTT_RX_FLUSH_SEQ_NUM_END_GET(word) \
    (((word) & HTT_RX_FLUSH_SEQ_NUM_END_M) >> HTT_RX_FLUSH_SEQ_NUM_END_S)

/*
 * @brief target -> host rx pn check indication message
 *
 * @details
 * The following field definitions describe the format of the Rx PN check
 * indication message sent from the target to the host.
 * The message consists of a 4-octet header, followed by the start and
 * end sequence numbers to be released, followed by the PN IEs. Each PN
 * IE is one octet containing the sequence number that failed the PN
 * check.
 *
 *     |31           24|23                            8|7            0|
 *     |--------------------------------------------------------------|
 *     |       TID     |          peer ID              |   msg type   |
 *     |--------------------------------------------------------------|
 *     |  Reserved     | PN IE count   | seq num end   | seq num start|
 *     |--------------------------------------------------------------|
 *     l               :    PN IE 2    |    PN IE 1    |   PN IE 0    |
 *     |--------------------------------------------------------------|

 * First DWORD:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: Identifies this as an rx pn check indication message
 *     Value: 0x2
 *   - PEER_ID
 *     Bits 23:8 (only bits 18:8 actually used)
 *     Purpose: identify which peer
 *     Value: (rx) peer ID
 *   - TID
 *     Bits 31:24 (only bits 27:24 actually used)
 *     Purpose: identify traffic identifier
 *     Value: traffic identifier
 * Second DWORD:
 *   - SEQ_NUM_START
 *     Bits 7:0
 *     Purpose:
 *        Indicates the starting sequence number of the MPDU in this
 *        series of MPDUs that went though PN check.
 *     Value:
 *        The sequence number for the first MPDU in the sequence.
 *        This sequence number is the 6 LSBs of the 802.11 sequence number.
 *   - SEQ_NUM_END
 *     Bits 15:8
 *     Purpose:
 *        Indicates the ending sequence number of the MPDU in this
 *        series of MPDUs that went though PN check.
 *     Value:
 *        The sequence number one larger then the sequence number of the last
 *        MPDU being flushed.
 *        This sequence number is the 6 LSBs of the 802.11 sequence number.
 *        The range of MPDUs from [SEQ_NUM_START,SEQ_NUM_END-1] have been checked
 *        for invalid PN numbers and are ready to be released for further processing.
 *        Not all MPDUs within this range are necessarily valid - the host
 *        must check each sequence number within this range to see if the
 *        corresponding MPDU is actually present.
 *   - PN_IE_COUNT
 *     Bits 23:16
 *     Purpose:
 *        Used to determine the variable number of PN information elements in this
 *        message
 *
 * PN information elements:
 *  - PN_IE_x-
 *      Purpose:
 *          Each PN information element contains the sequence number of the MPDU that
 *          has failed the target PN check.
 *      Value:
 *          Contains the 6 LSBs of the 802.11 sequence number corresponding to the MPDU
 *          that failed the PN check.
 */
/* first DWORD */
#define HTT_RX_PN_IND_PEER_ID_M  0xffff00
#define HTT_RX_PN_IND_PEER_ID_S  8
#define HTT_RX_PN_IND_TID_M      0xff000000
#define HTT_RX_PN_IND_TID_S      24
/* second DWORD */
#define HTT_RX_PN_IND_SEQ_NUM_START_M 0x000000ff
#define HTT_RX_PN_IND_SEQ_NUM_START_S 0
#define HTT_RX_PN_IND_SEQ_NUM_END_M   0x0000ff00
#define HTT_RX_PN_IND_SEQ_NUM_END_S   8
#define HTT_RX_PN_IND_PN_IE_CNT_M     0x00ff0000
#define HTT_RX_PN_IND_PN_IE_CNT_S     16

#define HTT_RX_PN_IND_BYTES 8

#define HTT_RX_PN_IND_PEER_ID_SET(word, value)                           \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_PN_IND_PEER_ID, value);                 \
        (word) |= (value)  << HTT_RX_PN_IND_PEER_ID_S;                   \
    } while (0)
#define HTT_RX_PN_IND_PEER_ID_GET(word) \
    (((word) & HTT_RX_PN_IND_PEER_ID_M) >> HTT_RX_PN_IND_PEER_ID_S)

#define HTT_RX_PN_IND_EXT_TID_SET(word, value)                               \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_PN_IND_TID, value);                     \
        (word) |= (value)  << HTT_RX_PN_IND_TID_S;                       \
    } while (0)
#define HTT_RX_PN_IND_EXT_TID_GET(word) \
    (((word) & HTT_RX_PN_IND_TID_M) >> HTT_RX_PN_IND_TID_S)

#define HTT_RX_PN_IND_SEQ_NUM_START_SET(word, value)                     \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_PN_IND_SEQ_NUM_START, value);           \
        (word) |= (value)  << HTT_RX_PN_IND_SEQ_NUM_START_S;             \
    } while (0)
#define HTT_RX_PN_IND_SEQ_NUM_START_GET(word) \
    (((word) & HTT_RX_PN_IND_SEQ_NUM_START_M) >> HTT_RX_PN_IND_SEQ_NUM_START_S)

#define HTT_RX_PN_IND_SEQ_NUM_END_SET(word, value)                       \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_PN_IND_SEQ_NUM_END, value);             \
        (word) |= (value)  << HTT_RX_PN_IND_SEQ_NUM_END_S;               \
    } while (0)
#define HTT_RX_PN_IND_SEQ_NUM_END_GET(word) \
    (((word) & HTT_RX_PN_IND_SEQ_NUM_END_M) >> HTT_RX_PN_IND_SEQ_NUM_END_S)

#define HTT_RX_PN_IND_PN_IE_CNT_SET(word, value)                         \
    do {                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_PN_IND_PN_IE_CNT, value);               \
        (word) |= (value) << HTT_RX_PN_IND_PN_IE_CNT_S;                  \
    } while(0)
#define HTT_RX_PN_IND_PN_IE_CNT_GET(word)   \
    (((word) & HTT_RX_PN_IND_PN_IE_CNT_M) >> HTT_RX_PN_IND_PN_IE_CNT_S)

/*
 * @brief target -> host rx offload deliver message for LL system
 *
 * @details
 * In a low latency system this message is sent whenever the offload
 * manager flushes out the packets it has coalesced in its coalescing buffer.
 * The DMA of the actual packets into host memory is done before sending out
 * this message. This message indicates only how many MSDUs to reap. The
 * peer ID, vdev ID, tid and MSDU length are copied inline into the header
 * portion of the MSDU while DMA'ing into the host memory. Unlike the packets
 * DMA'd by the MAC directly into host memory these packets do not contain
 * the MAC descriptors in the header portion of the packet. Instead they contain
 * the peer ID, vdev ID, tid and MSDU length. Also when the host receives this
 * message, the packets are delivered directly to the NW stack without going
 * through the regular reorder buffering and PN checking path since it has
 * already been done in target.
 *
 * |31             24|23             16|15              8|7               0|
 * |-----------------------------------------------------------------------|
 * |         Total MSDU count          |     reserved    |     msg type    |
 * |-----------------------------------------------------------------------|
 *
 * @brief target -> host rx offload deliver message for HL system
 *
 * @details
 * In a high latency system this message is sent whenever the offload manager
 * flushes out the packets it has coalesced in its coalescing buffer. The
 * actual packets are also carried along with this message. When the host
 * receives this message, it is expected to deliver these packets to the NW
 * stack directly instead of routing them through the reorder buffering and
 * PN checking path since it has already been done in target.
 *
 * |31             24|23             16|15              8|7               0|
 * |-----------------------------------------------------------------------|
 * |         Total MSDU count          |    reserved     |     msg type    |
 * |-----------------------------------------------------------------------|
 * |            peer ID                |              MSDU length          |
 * |-----------------------------------------------------------------------|
 * |  MSDU payload   |     FW Desc     |       tid       |   vdev ID       |
 * |-----------------------------------------------------------------------|
 * |                           MSDU payload contd.                         |
 * |-----------------------------------------------------------------------|
 * |            peer ID                |              MSDU length          |
 * |-----------------------------------------------------------------------|
 * |  MSDU payload   |    FW Desc      |       tid       |   vdev ID       |
 * |-----------------------------------------------------------------------|
 * |                           MSDU payload contd.                         |
 * |-----------------------------------------------------------------------|
 *
 */
/* first DWORD */
#define HTT_RX_OFFLOAD_DELIVER_IND_HDR_BYTES          4
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_HDR_BYTES     7

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_M         0xffff0000
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_S         16
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_M         0x0000ffff
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_S         0
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_M     0xffff0000
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_S     16
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_M     0x000000ff
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_S     0
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_M         0x0000ff00
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_S         8
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_M        0x00ff0000
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_S        16

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_GET(word)                                             \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_SET(word, value)                                      \
    do {                                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT, value);                            \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_S;                               \
    } while(0)                                                                                  \

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_GET(word)                                                 \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_SET(word, value)                                          \
    do {                                                                                                 \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN, value);                                \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_S;                                   \
    } while(0)                                                                                           \

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_GET(word)                                             \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_SET(word, value)                                      \
    do {                                                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID, value);                            \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_S;                               \
    } while(0)                                                                                      \

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_GET(word)                                             \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_SET(word, value)                                      \
    do {                                                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID, value);                            \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_S;                               \
    } while(0)                                                                                      \

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_GET(word)                                                 \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_SET(word, value)                                          \
    do {                                                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID, value);                                \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_S;                                   \
    } while(0)                                                                                      \

#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_GET(word)                                                 \
    (((word) & HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_M) >> HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_S)
#define HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_SET(word, value)                                          \
    do {                                                                                            \
        HTT_CHECK_SET_VAL(HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC, value);                                \
        (word) |= (value) << HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_S;                                   \
    } while(0)                                                                                      \

/**
 * @brief target -> host rx connection map/unmap message definition
 *
 * @details
 * The following diagram shows the format of the rx conn map message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31             24|23             16|15              8|7               0|
 * |-----------------------------------------------------------------------|
 * |              peer ID              |     VDEV ID     |     msg type    |
 * |-----------------------------------------------------------------------|
 * |    MAC addr 3   |    MAC addr 2   |    MAC addr 1   |    MAC addr 0   |
 * |-----------------------------------------------------------------------|
 * |              reserved             |    MAC addr 5   |    MAC addr 4   |
 * |-----------------------------------------------------------------------|
 *
 *
 * The following diagram shows the format of the rx conn unmap message sent
 * from the target to the host.
 *
 * |31             24|23             16|15              8|7               0|
 * |-----------------------------------------------------------------------|
 * |              peer ID              |     VDEV ID     |     msg type    |
 * |-----------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the rx conn map
 * and conn unmap messages sent from the target to the host.
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx conn map or conn unmap message
 *     Value: conn map -> 0x3, conn unmap -> 0x4
 *   - VDEV_ID
 *     Bits 15:8
 *     Purpose: Indicates which virtual device the connection is associated
 *         with.
 *     Value: vdev ID (used in the host to look up the vdev object)
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: The peer ID (index) that WAL is allocating (map) or
 *         freeing (unmap)
 *     Value: (rx) peer ID
 *   - MAC_ADDR_L32 (conn map only)
 *     Bits 31:0
 *     Purpose: Identifies which peer node the peer ID is for.
 *     Value: lower 4 bytes of peer node's MAC address
 *   - MAC_ADDR_U16 (conn map only)
 *     Bits 15:0
 *     Purpose: Identifies which peer node the peer ID is for.
 *     Value: upper 2 bytes of peer node's MAC address
 */
#define HTT_RX_PEER_MAP_VDEV_ID_M       0xff00
#define HTT_RX_PEER_MAP_VDEV_ID_S       8
#define HTT_RX_PEER_MAP_PEER_ID_M      0xffff0000
#define HTT_RX_PEER_MAP_PEER_ID_S      16
#define HTT_RX_PEER_MAP_MAC_ADDR_L32_M 0xffffffff
#define HTT_RX_PEER_MAP_MAC_ADDR_L32_S 0
#define HTT_RX_PEER_MAP_MAC_ADDR_U16_M 0xffff
#define HTT_RX_PEER_MAP_MAC_ADDR_U16_S 0

#define HTT_RX_PEER_MAP_VAP_ID_SET HTT_RX_PEER_MAP_VDEV_ID_SET /* deprecated */
#define HTT_RX_PEER_MAP_VDEV_ID_SET(word, value)                         \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_PEER_MAP_VDEV_ID, value);               \
        (word) |= (value)  << HTT_RX_PEER_MAP_VDEV_ID_S;                 \
    } while (0)
#define HTT_RX_PEER_MAP_VAP_ID_GET HTT_RX_PEER_MAP_VDEV_ID_GET /* deprecated */
#define HTT_RX_PEER_MAP_VDEV_ID_GET(word) \
    (((word) & HTT_RX_PEER_MAP_VDEV_ID_M) >> HTT_RX_PEER_MAP_VDEV_ID_S)

#define HTT_RX_PEER_MAP_PEER_ID_SET(word, value)                        \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_PEER_MAP_PEER_ID, value);              \
        (word) |= (value)  << HTT_RX_PEER_MAP_PEER_ID_S;                \
    } while (0)
#define HTT_RX_PEER_MAP_PEER_ID_GET(word) \
    (((word) & HTT_RX_PEER_MAP_PEER_ID_M) >> HTT_RX_PEER_MAP_PEER_ID_S)

#define HTT_RX_PEER_MAP_MAC_ADDR_OFFSET 4 /* bytes */

#define HTT_RX_PEER_MAP_BYTES 12


#define HTT_RX_PEER_UNMAP_PEER_ID_M   HTT_RX_PEER_MAP_PEER_ID_M
#define HTT_RX_PEER_UNMAP_PEER_ID_S   HTT_RX_PEER_MAP_PEER_ID_S

#define HTT_RX_PEER_UNMAP_PEER_ID_SET HTT_RX_PEER_MAP_PEER_ID_SET
#define HTT_RX_PEER_UNMAP_PEER_ID_GET HTT_RX_PEER_MAP_PEER_ID_GET

#define HTT_RX_PEER_UNMAP_VDEV_ID_SET HTT_RX_PEER_MAP_VDEV_ID_SET
#define HTT_RX_PEER_UNMAP_VDEV_ID_GET HTT_RX_PEER_MAP_VDEV_ID_GET

#define HTT_RX_PEER_UNMAP_BYTES 4


/**
 * @brief target -> host message specifying security parameters
 *
 * @details
 *  The following diagram shows the format of the security specification
 *  message sent from the target to the host.
 *  This security specification message tells the host whether a PN check is
 *  necessary on rx data frames, and if so, how large the PN counter is.
 *  This message also tells the host about the security processing to apply
 *  to defragmented rx frames - specifically, whether a Message Integrity
 *  Check is required, and the Michael key to use.
 *
 * |31             24|23          16|15|14              8|7               0|
 * |-----------------------------------------------------------------------|
 * |              peer ID           | U|  security type  |     msg type    |
 * |-----------------------------------------------------------------------|
 * |                           Michael Key K0                              |
 * |-----------------------------------------------------------------------|
 * |                           Michael Key K1                              |
 * |-----------------------------------------------------------------------|
 * |                           WAPI RSC Low0                               |
 * |-----------------------------------------------------------------------|
 * |                           WAPI RSC Low1                               |
 * |-----------------------------------------------------------------------|
 * |                           WAPI RSC Hi0                                |
 * |-----------------------------------------------------------------------|
 * |                           WAPI RSC Hi1                                |
 * |-----------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the security
 * indication message sent from the target to the host.
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a security specification message
 *     Value: 0xb
 *   - SEC_TYPE
 *     Bits 14:8
 *     Purpose: specifies which type of security applies to the peer
 *     Value: htt_sec_type enum value
 *   - UNICAST
 *     Bit 15
 *     Purpose: whether this security is applied to unicast or multicast data
 *     Value: 1 -> unicast, 0 -> multicast
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: The ID number for the peer the security specification is for
 *     Value: peer ID
 *   - MICHAEL_KEY_K0
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 1st half of the TKIP Michael key
 *     Value: Michael Key K0 (if security type is TKIP)
 *   - MICHAEL_KEY_K1
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 2nd half of the TKIP Michael key
 *     Value: Michael Key K1 (if security type is TKIP)
 *   - WAPI_RSC_LOW0
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 1st quarter of the 16 byte WAPI RSC
 *     Value: WAPI RSC Low0 (if security type is WAPI)
 *   - WAPI_RSC_LOW1
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 2nd quarter of the 16 byte WAPI RSC
 *     Value: WAPI RSC Low1 (if security type is WAPI)
 *   - WAPI_RSC_HI0
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 3rd quarter of the 16 byte WAPI RSC
 *     Value: WAPI RSC Hi0 (if security type is WAPI)
 *   - WAPI_RSC_HI1
 *     Bits 31:0
 *     Purpose: 4-byte word that forms the 4th quarter of the 16 byte WAPI RSC
 *     Value: WAPI RSC Hi1 (if security type is WAPI)
 */

#define HTT_SEC_IND_SEC_TYPE_M     0x00007f00
#define HTT_SEC_IND_SEC_TYPE_S     8
#define HTT_SEC_IND_UNICAST_M      0x00008000
#define HTT_SEC_IND_UNICAST_S      15
#define HTT_SEC_IND_PEER_ID_M      0xffff0000
#define HTT_SEC_IND_PEER_ID_S      16

#define HTT_SEC_IND_SEC_TYPE_SET(word, value)                       \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_SEC_IND_SEC_TYPE, value);             \
        (word) |= (value)  << HTT_SEC_IND_SEC_TYPE_S;               \
    } while (0)
#define HTT_SEC_IND_SEC_TYPE_GET(word) \
    (((word) & HTT_SEC_IND_SEC_TYPE_M) >> HTT_SEC_IND_SEC_TYPE_S)

#define HTT_SEC_IND_UNICAST_SET(word, value)                        \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_SEC_IND_UNICAST, value);              \
        (word) |= (value)  << HTT_SEC_IND_UNICAST_S;                \
    } while (0)
#define HTT_SEC_IND_UNICAST_GET(word) \
    (((word) & HTT_SEC_IND_UNICAST_M) >> HTT_SEC_IND_UNICAST_S)

#define HTT_SEC_IND_PEER_ID_SET(word, value)                        \
    do {                                                            \
        HTT_CHECK_SET_VAL(HTT_SEC_IND_PEER_ID, value);              \
        (word) |= (value)  << HTT_SEC_IND_PEER_ID_S;                \
    } while (0)
#define HTT_SEC_IND_PEER_ID_GET(word) \
    (((word) & HTT_SEC_IND_PEER_ID_M) >> HTT_SEC_IND_PEER_ID_S)


#define HTT_SEC_IND_BYTES 28


/**
 * @brief target -> host rx ADDBA / DELBA message definitions
 *
 * @details
 * The following diagram shows the format of the rx ADDBA message sent
 * from the target to the host:
 *
 * |31                      20|19  16|15              8|7               0|
 * |---------------------------------------------------------------------|
 * |          peer ID         |  TID |   window size   |     msg type    |
 * |---------------------------------------------------------------------|
 *
 * The following diagram shows the format of the rx DELBA message sent
 * from the target to the host:
 *
 * |31                      20|19  16|15              8|7               0|
 * |---------------------------------------------------------------------|
 * |          peer ID         |  TID |     reserved    |     msg type    |
 * |---------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the rx ADDBA
 * and DELBA messages sent from the target to the host.
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx ADDBA or DELBA message
 *     Value: ADDBA -> 0x5, DELBA -> 0x6
 *   - WIN_SIZE
 *     Bits 15:8 (ADDBA only)
 *     Purpose: Specifies the length of the block ack window (max = 64).
 *     Value:
 *         block ack window length specified by the received ADDBA
 *         management message.
 *   - TID
 *     Bits 19:16
 *     Purpose: Specifies which traffic identifier the ADDBA / DELBA is for.
 *     Value:
 *         TID specified by the received ADDBA or DELBA management message.
 *   - PEER_ID
 *     Bits 31:20
 *     Purpose: Identifies which peer sent the ADDBA / DELBA.
 *     Value:
 *         ID (hash value) used by the host for fast,  direct lookup of
 *         host SW peer info, including rx reorder states.
 */
#define HTT_RX_ADDBA_WIN_SIZE_M  0xff00
#define HTT_RX_ADDBA_WIN_SIZE_S  8
#define HTT_RX_ADDBA_TID_M       0xf0000
#define HTT_RX_ADDBA_TID_S       16
#define HTT_RX_ADDBA_PEER_ID_M   0xfff00000
#define HTT_RX_ADDBA_PEER_ID_S   20

#define HTT_RX_ADDBA_WIN_SIZE_SET(word, value)                          \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_ADDBA_WIN_SIZE, value);                \
        (word) |= (value)  << HTT_RX_ADDBA_WIN_SIZE_S;                  \
    } while (0)
#define HTT_RX_ADDBA_WIN_SIZE_GET(word) \
    (((word) & HTT_RX_ADDBA_WIN_SIZE_M) >> HTT_RX_ADDBA_WIN_SIZE_S)

#define HTT_RX_ADDBA_TID_SET(word, value)                               \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_ADDBA_TID, value);                     \
        (word) |= (value)  << HTT_RX_ADDBA_TID_S;                       \
    } while (0)
#define HTT_RX_ADDBA_TID_GET(word) \
    (((word) & HTT_RX_ADDBA_TID_M) >> HTT_RX_ADDBA_TID_S)

#define HTT_RX_ADDBA_PEER_ID_SET(word, value)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_ADDBA_PEER_ID, value);                 \
        (word) |= (value)  << HTT_RX_ADDBA_PEER_ID_S;                   \
    } while (0)
#define HTT_RX_ADDBA_PEER_ID_GET(word) \
    (((word) & HTT_RX_ADDBA_PEER_ID_M) >> HTT_RX_ADDBA_PEER_ID_S)

#define HTT_RX_ADDBA_BYTES 4


#define HTT_RX_DELBA_TID_M         HTT_RX_ADDBA_TID_M
#define HTT_RX_DELBA_TID_S         HTT_RX_ADDBA_TID_S
#define HTT_RX_DELBA_PEER_ID_M     HTT_RX_ADDBA_PEER_ID_M
#define HTT_RX_DELBA_PEER_ID_S     HTT_RX_ADDBA_PEER_ID_S

#define HTT_RX_DELBA_TID_SET       HTT_RX_ADDBA_TID_SET
#define HTT_RX_DELBA_TID_GET       HTT_RX_ADDBA_TID_GET
#define HTT_RX_DELBA_PEER_ID_SET   HTT_RX_ADDBA_PEER_ID_SET
#define HTT_RX_DELBA_PEER_ID_GET   HTT_RX_ADDBA_PEER_ID_GET

#define HTT_RX_DELBA_BYTES 4

/**
 * @brief target -> host TX completion indication message definition
 *
 * @details
 * The following diagram shows the format of the TX completion indication sent
 * from the target to the host
 *
 *          |31      25|    24|23        16| 15 |14 11|10   8|7          0|
 *          |-------------------------------------------------------------|
 * header:  | reserved |append|     num    | t_i| tid |status|  msg_type  |
 *          |-------------------------------------------------------------|
 * payload: |            MSDU1 ID          |         MSDU0 ID             |
 *          |-------------------------------------------------------------|
 *          :            MSDU3 ID          :         MSDU2 ID             :
 *          |-------------------------------------------------------------|
 *          |          struct htt_tx_compl_ind_append_retries             |
 *          - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * The following field definitions describe the format of the TX completion
 * indication sent from the target to the host
 * Header fields:
 * - msg_type
 *   Bits 7:0
 *   Purpose: identifies this as HTT TX completion indication
 *   Value: 0x7
 * - status
 *   Bits 10:8
 *   Purpose: the TX completion status of payload fragmentations descriptors
 *   Value: could be HTT_TX_COMPL_IND_STAT_OK or HTT_TX_COMPL_IND_STAT_DISCARD
 * - tid
 *   Bits 14:11
 *   Purpose: the tid associated with those fragmentation descriptors. It is
 *            valid or not, depending on the tid_invalid bit.
 *   Value: 0 to 15
 * - tid_invalid
 *   Bits 15:15
 *   Purpose: this bit indicates whether the tid field is valid or not
 *   Value: 0 indicates valid; 1 indicates invalid
 * - num
 *   Bits 23:16
 *   Purpose: the number of payload in this indication
 *   Value: 1 to 255
 * - append
 *   Bits 24:24
 *   Purpose: append the struct htt_tx_compl_ind_append_retries which contains
 *            the number of tx retries for one MSDU at the end of this message
 *   Value: 0 indicates no appending; 1 indicates appending
 * Payload fields:
 * - hmsdu_id
 *   Bits 15:0
 *   Purpose: this ID is used to track the Tx buffer in host
 *   Value: 0 to "size of host MSDU descriptor pool - 1"
 */

#define HTT_TX_COMPL_IND_STATUS_S      8
#define HTT_TX_COMPL_IND_STATUS_M      0x00000700
#define HTT_TX_COMPL_IND_TID_S         11
#define HTT_TX_COMPL_IND_TID_M         0x00007800
#define HTT_TX_COMPL_IND_TID_INV_S     15
#define HTT_TX_COMPL_IND_TID_INV_M     0x00008000
#define HTT_TX_COMPL_IND_NUM_S         16
#define HTT_TX_COMPL_IND_NUM_M         0x00ff0000
#define HTT_TX_COMPL_IND_APPEND_S      24
#define HTT_TX_COMPL_IND_APPEND_M      0x01000000

#define HTT_TX_COMPL_IND_STATUS_SET(_info, _val)                        \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_TX_COMPL_IND_STATUS, _val);               \
        ((_info) |= ((_val) << HTT_TX_COMPL_IND_STATUS_S));             \
    } while (0)
#define HTT_TX_COMPL_IND_STATUS_GET(_info)                              \
    (((_info) & HTT_TX_COMPL_IND_STATUS_M) >> HTT_TX_COMPL_IND_STATUS_S)
#define HTT_TX_COMPL_IND_NUM_SET(_info, _val)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_TX_COMPL_IND_NUM, _val);                  \
        ((_info) |= ((_val) << HTT_TX_COMPL_IND_NUM_S));                \
    } while (0)
#define HTT_TX_COMPL_IND_NUM_GET(_info)                             \
    (((_info) & HTT_TX_COMPL_IND_NUM_M) >> HTT_TX_COMPL_IND_NUM_S)
#define HTT_TX_COMPL_IND_TID_SET(_info, _val)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_TX_COMPL_IND_TID, _val);                  \
        ((_info) |= ((_val) << HTT_TX_COMPL_IND_TID_S));                \
    } while (0)
#define HTT_TX_COMPL_IND_TID_GET(_info)                             \
    (((_info) & HTT_TX_COMPL_IND_TID_M) >> HTT_TX_COMPL_IND_TID_S)
#define HTT_TX_COMPL_IND_TID_INV_SET(_info, _val)                       \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_TX_COMPL_IND_TID_INV, _val);              \
        ((_info) |= ((_val) << HTT_TX_COMPL_IND_TID_INV_S));            \
    } while (0)
#define HTT_TX_COMPL_IND_TID_INV_GET(_info)                         \
    (((_info) & HTT_TX_COMPL_IND_TID_INV_M) >>                      \
     HTT_TX_COMPL_IND_TID_INV_S)
#define HTT_TX_COMPL_IND_APPEND_SET(_info, _val)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_TX_COMPL_IND_APPEND, _val);                  \
        ((_info) |= ((_val) << HTT_TX_COMPL_IND_APPEND_S));                \
    } while (0)
#define HTT_TX_COMPL_IND_APPEND_GET(_info)                             \
    (((_info) & HTT_TX_COMPL_IND_APPEND_M) >> HTT_TX_COMPL_IND_APPEND_S)

#define HTT_TX_COMPL_CTXT_SZ                sizeof(A_UINT16)
#define HTT_TX_COMPL_CTXT_NUM(_bytes)       ((_bytes) >> 1)

#define HTT_TX_COMPL_INV_MSDU_ID            0xffff

#define HTT_TX_COMPL_IND_STAT_OK          0
#define HTT_TX_COMPL_IND_STAT_DISCARD     1
#define HTT_TX_COMPL_IND_STAT_NO_ACK      2
#define HTT_TX_COMPL_IND_STAT_POSTPONE    3

#define HTT_TX_COMPL_IND_APPEND_SET_MORE_RETRY(f)  ((f) |= 0x1)
#define HTT_TX_COMPL_IND_APPEND_CLR_MORE_RETRY(f)  ((f) &= (~0x1))

PREPACK struct htt_tx_compl_ind_base {
    A_UINT32 hdr;
    A_UINT16 payload[1/*or more*/];
} POSTPACK;

PREPACK struct htt_tx_compl_ind_append_retries {
    A_UINT16 msdu_id;
    A_UINT8  tx_retries;
    A_UINT8  flag; /* Bit 0, 1: another append_retries struct is appended
                             0: this is the last append_retries struct */
} POSTPACK;


/**
 * @brief target -> host rx fragment indication message definition
 *
 * @details
 * The following field definitions describe the format of the rx fragment
 * indication message sent from the target to the host.
 * The rx fragment indication message shares the format of the
 * rx indication message, but not all fields from the rx indication message
 * are relevant to the rx fragment indication message.
 *
 *
 *     |31       24|23         18|17|16|15|14|13|12|11|10|9|8|7|6|5|4     0|
 *     |-----------+-------------------+---------------------+-------------|
 *     |             peer ID           |     |FV| ext TID    |  msg type   |
 *     |-------------------------------------------------------------------|
 *     |                                           |    flush    |  flush  |
 *     |                                           |     end     |  start  |
 *     |                                           |   seq num   | seq num |
 *     |-------------------------------------------------------------------|
 *     |           reserved            |         FW rx desc bytes          |
 *     |-------------------------------------------------------------------|
 *     |                                                     | FW MSDU Rx  |
 *     |                                                     |   desc B0   |
 *     |-------------------------------------------------------------------|
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx fragment indication message
 *     Value: 0xa
 *   - EXT_TID
 *     Bits 12:8
 *     Purpose: identify the traffic ID of the rx data, including
 *         special "extended" TID values for multicast, broadcast, and
 *         non-QoS data frames
 *     Value: 0-15 for regular TIDs, or >= 16 for bcast/mcast/non-QoS
 *   - FLUSH_VALID (FV)
 *     Bit 13
 *     Purpose: indicate whether the flush IE (start/end sequence numbers)
 *         is valid
 *     Value:
 *         1 -> flush IE is valid and needs to be processed
 *         0 -> flush IE is not valid and should be ignored
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: Identify, by ID, which peer sent the rx data
 *     Value: ID of the peer who sent the rx data
 *   - FLUSH_SEQ_NUM_START
 *     Bits 5:0
 *     Purpose: Indicate the start of a series of MPDUs to flush
 *         Not all MPDUs within this series are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 *         This field is only valid if the FV bit is set.
 *     Value:
 *         The sequence number for the first MPDUs to check to flush.
 *         The sequence number is masked by 0x3f.
 *   - FLUSH_SEQ_NUM_END
 *     Bits 11:6
 *     Purpose: Indicate the end of a series of MPDUs to flush
 *     Value:
 *         The sequence number one larger than the sequence number of the
 *         last MPDU to check to flush.
 *         The sequence number is masked by 0x3f.
 *         Not all MPDUs within this series are necessarily valid - the host
 *         must check each sequence number within this range to see if the
 *         corresponding MPDU is actually present.
 *         This field is only valid if the FV bit is set.
 * Rx descriptor fields:
 *   - FW_RX_DESC_BYTES
 *     Bits 15:0
 *     Purpose: Indicate how many bytes in the Rx indication are used for
 *         FW Rx descriptors
 *     Value: 1
 */
#define HTT_RX_FRAG_IND_HDR_PREFIX_SIZE32         2

#define HTT_RX_FRAG_IND_FW_DESC_BYTE_OFFSET       12

#define HTT_RX_FRAG_IND_EXT_TID_SET     HTT_RX_IND_EXT_TID_SET
#define HTT_RX_FRAG_IND_EXT_TID_GET     HTT_RX_IND_EXT_TID_GET

#define HTT_RX_FRAG_IND_PEER_ID_SET     HTT_RX_IND_PEER_ID_SET
#define HTT_RX_FRAG_IND_PEER_ID_GET     HTT_RX_IND_PEER_ID_GET

#define HTT_RX_FRAG_IND_FLUSH_VALID_SET HTT_RX_IND_FLUSH_VALID_SET
#define HTT_RX_FRAG_IND_FLUSH_VALID_GET HTT_RX_IND_FLUSH_VALID_GET

#define HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_START_SET \
    HTT_RX_IND_FLUSH_SEQ_NUM_START_SET
#define HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_START_GET \
    HTT_RX_IND_FLUSH_SEQ_NUM_START_GET

#define HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_END_SET \
    HTT_RX_IND_FLUSH_SEQ_NUM_END_SET
#define HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_END_GET \
    HTT_RX_IND_FLUSH_SEQ_NUM_END_GET

#define HTT_RX_FRAG_IND_FW_RX_DESC_BYTES_GET  HTT_RX_IND_FW_RX_DESC_BYTES_GET

#define HTT_RX_FRAG_IND_BYTES                 \
    (4 /* msg hdr */ +                        \
     4 /* flush spec */ +                     \
     4 /* (unused) FW rx desc bytes spec */ + \
     4 /* FW rx desc */)

/**
 * @brief target -> host test message definition
 *
 * @details
 * The following field definitions describe the format of the test
 * message sent from the target to the host.
 * The message consists of a 4-octet header, followed by a variable
 * number of 32-bit integer values, followed by a variable number
 * of 8-bit character values.
 *
 * |31                         16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |          num chars          |   num ints   |   msg type   |
 * |-----------------------------------------------------------|
 * |                           int 0                           |
 * |-----------------------------------------------------------|
 * |                           int 1                           |
 * |-----------------------------------------------------------|
 * |                            ...                            |
 * |-----------------------------------------------------------|
 * |    char 3    |    char 2    |    char 1    |    char 0    |
 * |-----------------------------------------------------------|
 * |              |              |      ...     |    char 4    |
 * |-----------------------------------------------------------|
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a test message
 *     Value: HTT_MSG_TYPE_TEST
 *   - NUM_INTS
 *     Bits 15:8
 *     Purpose: indicate how many 32-bit integers follow the message header
 *   - NUM_CHARS
 *     Bits 31:16
 *     Purpose: indicate how many 8-bit charaters follow the series of integers
 */
#define HTT_RX_TEST_NUM_INTS_M   0xff00
#define HTT_RX_TEST_NUM_INTS_S   8
#define HTT_RX_TEST_NUM_CHARS_M  0xffff0000
#define HTT_RX_TEST_NUM_CHARS_S  16

#define HTT_RX_TEST_NUM_INTS_SET(word, value)                           \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_TEST_NUM_INTS, value);                 \
        (word) |= (value)  << HTT_RX_TEST_NUM_INTS_S;                   \
    } while (0)
#define HTT_RX_TEST_NUM_INTS_GET(word) \
    (((word) & HTT_RX_TEST_NUM_INTS_M) >> HTT_RX_TEST_NUM_INTS_S)

#define HTT_RX_TEST_NUM_CHARS_SET(word, value)                          \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_RX_TEST_NUM_CHARS, value);                \
        (word) |= (value)  << HTT_RX_TEST_NUM_CHARS_S;                  \
    } while (0)
#define HTT_RX_TEST_NUM_CHARS_GET(word) \
    (((word) & HTT_RX_TEST_NUM_CHARS_M) >> HTT_RX_TEST_NUM_CHARS_S)

/**
 * @brief target -> host packet log message
 *
 * @details
 * The following field definitions describe the format of the packet log
 * message sent from the target to the host.
 * The message consists of a 4-octet header,followed by a variable number
 * of 32-bit character values.
 *
 * |31          24|23          16|15           8|7            0|
 * |-----------------------------------------------------------|
 * |              |              |              |   msg type   |
 * |-----------------------------------------------------------|
 * |                        payload                            |
 * |-----------------------------------------------------------|
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a test message
 *     Value: HTT_MSG_TYPE_PACKETLOG
 */
PREPACK struct htt_pktlog_msg {
    A_UINT32    header;
    A_UINT32   payload[1/* or more */];
} POSTPACK;


/*
 * Rx reorder statistics
 * NB: all the fields must be defined in 4 octets size.
 */
struct rx_reorder_stats {
    /* Non QoS MPDUs received */
    A_UINT32 deliver_non_qos;
    /* MPDUs received in-order */
    A_UINT32 deliver_in_order;
    /* Flush due to reorder timer expired */
    A_UINT32 deliver_flush_timeout;
    /* Flush due to move out of window */
    A_UINT32 deliver_flush_oow;
    /* Flush due to DELBA */
    A_UINT32 deliver_flush_delba;
    /* MPDUs dropped due to FCS error */
    A_UINT32 fcs_error;
    /* MPDUs dropped due to monitor mode non-data packet */
    A_UINT32 mgmt_ctrl;
    /* MPDUs dropped due to invalid peer */
    A_UINT32 invalid_peer;
    /* MPDUs dropped due to duplication (non aggregation) */
    A_UINT32 dup_non_aggr;
    /* MPDUs dropped due to processed before */
    A_UINT32 dup_past;
    /* MPDUs dropped due to duplicate in reorder queue */
    A_UINT32 dup_in_reorder;
    /* Reorder timeout happened */
    A_UINT32 reorder_timeout;
    /* invalid bar ssn */
    A_UINT32 invalid_bar_ssn;
    /* reorder reset due to bar ssn */
    A_UINT32 ssn_reset;

};

/*
 * htt_dbg_stats_status -
 * present -     The requested stats have been delivered in full.
 *               This indicates that either the stats information was contained
 *               in its entirety within this message, or else this message
 *               completes the delivery of the requested stats info that was
 *               partially delivered through earlier STATS_CONF messages.
 * partial -     The requested stats have been delivered in part.
 *               One or more subsequent STATS_CONF messages with the same
 *               cookie value will be sent to deliver the remainder of the
 *               information.
 * error -       The requested stats could not be delivered, for example due
 *               to a shortage of memory to construct a message holding the
 *               requested stats.
 * invalid -     The requested stat type is either not recognized, or the
 *               target is configured to not gather the stats type in question.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * series_done - This special value indicates that no further stats info
 *               elements are present within a series of stats info elems
 *               (within a stats upload confirmation message).
 */
enum htt_dbg_stats_status {
    HTT_DBG_STATS_STATUS_PRESENT = 0,
    HTT_DBG_STATS_STATUS_PARTIAL = 1,
    HTT_DBG_STATS_STATUS_ERROR   = 2,
    HTT_DBG_STATS_STATUS_INVALID = 3,


    HTT_DBG_STATS_STATUS_SERIES_DONE = 7
};

/**
 * @brief target -> host statistics upload
 *
 * @details
 * The following field definitions describe the format of the HTT target
 * to host stats upload confirmation message.
 * The message contains a cookie echoed from the HTT host->target stats
 * upload request, which identifies which request the confirmation is
 * for, and a series of tag-length-value stats information elements.
 * The tag-length header for each stats info element also includes a
 * status field, to indicate whether the request for the stat type in
 * question was fully met, partially met, unable to be met, or invalid
 * (if the stat type in question is disabled in the target).
 * A special value of all 1's in this status field is used to indicate
 * the end of the series of stats info elements.
 *
 *
 * |31                         16|15           8|7   5|4       0|
 * |------------------------------------------------------------|
 * |                  reserved                  |    msg type   |
 * |------------------------------------------------------------|
 * |                        cookie LSBs                         |
 * |------------------------------------------------------------|
 * |                        cookie MSBs                         |
 * |------------------------------------------------------------|
 * |      stats entry length     |   reserved   |  S  |stat type|
 * |------------------------------------------------------------|
 * |                                                            |
 * |                  type-specific stats info                  |
 * |                                                            |
 * |------------------------------------------------------------|
 * |      stats entry length     |   reserved   |  S  |stat type|
 * |------------------------------------------------------------|
 * |                                                            |
 * |                  type-specific stats info                  |
 * |                                                            |
 * |------------------------------------------------------------|
 * |              n/a            |   reserved   | 111 |   n/a   |
 * |------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Purpose: identifies this is a statistics upload confirmation message
 *    Value: 0x9
 *  - COOKIE_LSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: LSBs of the opaque cookie specified by the host-side requestor
 *  - COOKIE_MSBS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to match a target->host stats confirmation
 *        message with its preceding host->target stats request message.
 *    Value: MSBs of the opaque cookie specified by the host-side requestor
 *
 * Stats Information Element tag-length header fields:
 *  - STAT_TYPE
 *    Bits 4:0
 *    Purpose: identifies the type of statistics info held in the
 *        following information element
 *    Value: htt_dbg_stats_type
 *  - STATUS
 *    Bits 7:5
 *    Purpose: indicate whether the requested stats are present
 *    Value: htt_dbg_stats_status, including a special value (0x7) to mark
 *        the completion of the stats entry series
 *  - LENGTH
 *    Bits 31:16
 *    Purpose: indicate the stats information size
 *    Value: This field specifies the number of bytes of stats information
 *       that follows the element tag-length header.
 *       It is expected but not required that this length is a multiple of
 *       4 bytes.  Even if the length is not an integer multiple of 4, the
 *       subsequent stats entry header will begin on a 4-byte aligned
 *       boundary.
 */

#define HTT_T2H_STATS_CONF_HDR_SIZE       4

#define HTT_T2H_STATS_CONF_TLV_HDR_SIZE   4

#define HTT_T2H_STATS_CONF_TLV_TYPE_M     0x0000001f
#define HTT_T2H_STATS_CONF_TLV_TYPE_S     0
#define HTT_T2H_STATS_CONF_TLV_STATUS_M   0x000000e0
#define HTT_T2H_STATS_CONF_TLV_STATUS_S   5
#define HTT_T2H_STATS_CONF_TLV_LENGTH_M   0xffff0000
#define HTT_T2H_STATS_CONF_TLV_LENGTH_S   16

#define HTT_T2H_STATS_CONF_TLV_TYPE_SET(word, value)             \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_T2H_STATS_CONF_TLV_TYPE, value);   \
        (word) |= (value)  << HTT_T2H_STATS_CONF_TLV_TYPE_S;     \
    } while (0)
#define HTT_T2H_STATS_CONF_TLV_TYPE_GET(word) \
    (((word) & HTT_T2H_STATS_CONF_TLV_TYPE_M) >> \
    HTT_T2H_STATS_CONF_TLV_TYPE_S)

#define HTT_T2H_STATS_CONF_TLV_STATUS_SET(word, value)             \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_T2H_STATS_CONF_TLV_STATUS, value);   \
        (word) |= (value)  << HTT_T2H_STATS_CONF_TLV_STATUS_S;     \
    } while (0)
#define HTT_T2H_STATS_CONF_TLV_STATUS_GET(word) \
    (((word) & HTT_T2H_STATS_CONF_TLV_STATUS_M) >> \
    HTT_T2H_STATS_CONF_TLV_STATUS_S)

#define HTT_T2H_STATS_CONF_TLV_LENGTH_SET(word, value)             \
    do {                                                         \
        HTT_CHECK_SET_VAL(HTT_T2H_STATS_CONF_TLV_LENGTH, value);   \
        (word) |= (value)  << HTT_T2H_STATS_CONF_TLV_LENGTH_S;     \
    } while (0)
#define HTT_T2H_STATS_CONF_TLV_LENGTH_GET(word) \
    (((word) & HTT_T2H_STATS_CONF_TLV_LENGTH_M) >> \
    HTT_T2H_STATS_CONF_TLV_LENGTH_S)

#define HL_HTT_FW_RX_DESC_RSVD_SIZE 18
#define HTT_MAX_AGGR 64
#define HTT_HL_MAX_AGGR 18

/**
 * @brief host -> target FRAG DESCRIPTOR/MSDU_EXT DESC bank
 *
 * @details
 * The following field definitions describe the format of the HTT host
 * to target frag_desc/msdu_ext bank configuration message.
 * The message contains the based address and the min and max id of the
 * MSDU_EXT/FRAG_DESC that will be used by the HTT to map MSDU DESC and
 * MSDU_EXT/FRAG_DESC.
 * HTT will use id in HTT descriptor instead sending the frag_desc_ptr.
 * In peregrine the firmware will use fragment_desc_ptr but in WIFI2.0
 * the hardware does the mapping/translation.
 *
 * Total banks that can be configured is configured to 16.
 *
 * This should be called before any TX has be initiated by the HTT
 *
 * |31                         16|15           8|7   5|4       0|
 * |------------------------------------------------------------|
 * | DESC_SIZE    |  NUM_BANKS   | RES |SWP|pdev|    msg type   |
 * |------------------------------------------------------------|
 * |                     BANK0_BASE_ADDRESS                     |
 * |------------------------------------------------------------|
 * |                            ...                             |
 * |------------------------------------------------------------|
 * |                    BANK15_BASE_ADDRESS                     |
 * |------------------------------------------------------------|
 * |       BANK0_MAX_ID          |       BANK0_MIN_ID           |
 * |------------------------------------------------------------|
 * |                            ...                             |
 * |------------------------------------------------------------|
 * |       BANK15_MAX_ID         |       BANK15_MIN_ID          |
 * |------------------------------------------------------------|
 * Header fields:
 *  - MSG_TYPE
 *    Bits 7:0
 *    Value: 0x6
 *  - BANKx_BASE_ADDRESS
 *    Bits 31:0
 *    Purpose: Provide a mechanism to specify the base address of the MSDU_EXT
 *         bank physical/bus address.
 *  - BANKx_MIN_ID
 *    Bits 15:0
 *    Purpose: Provide a mechanism to specify the min index that needs to
 *          mapped.
 *  - BANKx_MAX_ID
 *    Bits 31:16
 *    Purpose: Provide a mechanism to specify the max index that needs to
 *          mapped.
 *
 */

/** @todo Compress the fields to fit MAX HTT Message size, until then configure to a
 *         safe value.
 *  @note MAX supported banks is 16.
 */
#define HTT_TX_MSDU_EXT_BANK_MAX 4

#define HTT_H2T_FRAG_DESC_BANK_PDEVID_M       0x300
#define HTT_H2T_FRAG_DESC_BANK_PDEVID_S       8

#define HTT_H2T_FRAG_DESC_BANK_SWAP_M         0x400
#define HTT_H2T_FRAG_DESC_BANK_SWAP_S         10

#define HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_M    0xff0000
#define HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_S    16

#define HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_M    0xff000000
#define HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_S    24

#define HTT_H2T_FRAG_DESC_BANK_MIN_IDX_M      0xffff
#define HTT_H2T_FRAG_DESC_BANK_MIN_IDX_S      0

#define HTT_H2T_FRAG_DESC_BANK_MAX_IDX_M      0xffff0000
#define HTT_H2T_FRAG_DESC_BANK_MAX_IDX_S      16

#define HTT_H2T_FRAG_DESC_BANK_PDEVID_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_PDEVID, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_PDEVID_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_PDEVID_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_PDEVID_M) >> HTT_H2T_FRAG_DESC_BANK_PDEVID_S)

#define HTT_H2T_FRAG_DESC_BANK_SWAP_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_SWAP, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_SWAP_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_SWAP_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_SWAP_M) >> HTT_H2T_FRAG_DESC_BANK_SWAP_S)

#define HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_NUM_BANKS, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_M) >> HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_S)

#define HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_DESC_SIZE, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_M) >> HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_S)

#define HTT_H2T_FRAG_DESC_BANK_MIN_IDX_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_MIN_IDX, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_MIN_IDX_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_MIN_IDX_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_MIN_IDX_M) >> HTT_H2T_FRAG_DESC_BANK_MIN_IDX_S)

#define HTT_H2T_FRAG_DESC_BANK_MAX_IDX_SET(word, value)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_H2T_FRAG_DESC_BANK_MAX_IDX, value); \
        (word) |= ((value) << HTT_H2T_FRAG_DESC_BANK_MAX_IDX_S);  \
    } while (0)
#define HTT_H2T_FRAG_DESC_BANK_MAX_IDX_GET(word) \
    (((word) & HTT_H2T_FRAG_DESC_BANK_MAX_IDX_M) >> HTT_H2T_FRAG_DESC_BANK_MAX_IDX_S)


PREPACK struct htt_tx_frag_desc_bank_cfg_t {
      /** word 0
       * msg_type:      8,
       * pdev_id:      2,
       * swap:         1,
       * reserved0:    5,
       * num_banks:    8,
       * desc_size:    8;
       */
    A_UINT32 word0;
    A_UINT32 bank_base_address[HTT_TX_MSDU_EXT_BANK_MAX];
    A_UINT32 bank_info[HTT_TX_MSDU_EXT_BANK_MAX];
} POSTPACK;



/**
 * @brief target -> host HTT TX Credit total count update message definition
 *
 *|31                 16|15       9|  8    |7       0 |
 *|---------------------+----------+-------+----------|
 *|cur htt credit delta | reserved | sign  | msg type |
 *|---------------------------------------------------|
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a htt tx credit delta update message
 *     Value: 0xe
 *   - SIGN
 *     Bits 8
 *      identifies whether credit delta is positive or negative
 *     Value:
 *       - 0x0: credit delta is positive, rebalance in some buffers
 *       - 0x1: credit delta is negative, rebalance out some buffers
 *     Bits 15:9
 *       - reserved
 *   - DELTA_COUNT
 *     Bits 31:16
 *     Purpose: Specify current htt credit delta absolute count
 */

#define HTT_TX_CREDIT_SIGN_BIT_M       0x00000100
#define HTT_TX_CREDIT_SIGN_BIT_S       8
#define HTT_TX_CREDIT_DELTA_ABS_M      0xffff0000
#define HTT_TX_CREDIT_DELTA_ABS_S      16


#define HTT_TX_CREDIT_SIGN_BIT_SET(word, value)                              \
    do {                                                                     \
        HTT_CHECK_SET_VAL(HTT_TX_CREDIT_SIGN_BIT, value);                    \
        (word) |= (value)  << HTT_TX_CREDIT_SIGN_BIT_S;                      \
    } while (0)

#define HTT_TX_CREDIT_SIGN_BIT_GET(word) \
    (((word) & HTT_TX_CREDIT_SIGN_BIT_M) >> HTT_TX_CREDIT_SIGN_BIT_S)

#define HTT_TX_CREDIT_DELTA_ABS_SET(word, value)                              \
    do {                                                                      \
        HTT_CHECK_SET_VAL(HTT_TX_CREDIT_DELTA_ABS, value);                    \
        (word) |= (value)  << HTT_TX_CREDIT_DELTA_ABS_S;                      \
    } while (0)

#define HTT_TX_CREDIT_DELTA_ABS_GET(word) \
    (((word) & HTT_TX_CREDIT_DELTA_ABS_M) >> HTT_TX_CREDIT_DELTA_ABS_S)


#define HTT_TX_CREDIT_MSG_BYTES 4

#define HTT_TX_CREDIT_SIGN_BIT_POSITIVE  0x0
#define HTT_TX_CREDIT_SIGN_BIT_NEGATIVE  0x1


/**
 * @brief HTT WDI_IPA Operation Response Message
 *
 * @details
 *  HTT WDI_IPA Operation Response message is sent by target
 *  to host confirming suspend or resume operation.
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |             op_code             |      Rsvd      |     msg_type   |
 *     |-------------------------------------------------------------------|
 *     |             Rsvd                |          Response len           |
 *     |-------------------------------------------------------------------|
 *     |                                                                   |
 *     |                  Response-type specific info                      |
 *     |                                                                   |
 *     |                                                                   |
 *     |-------------------------------------------------------------------|
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: Identifies this as WDI_IPA Operation Response message
 *     value: = 0x13
 *   - OP_CODE
 *     Bits 31:16
 *     Purpose: Identifies the operation target is responding to (e.g. TX suspend)
 *     value: = enum htt_wdi_ipa_op_code
 *   - RSP_LEN
 *     Bits 16:0
 *     Purpose: length for the response-type specific info
 *     value: = length in bytes for response-type specific info
 *              For example, if OP_CODE == HTT_WDI_IPA_OPCODE_DBG_STATS, the
 *              length value will be sizeof(struct wlan_wdi_ipa_dbg_stats_t).
 */

PREPACK struct htt_wdi_ipa_op_response_t
{
    /* DWORD 0: flags and meta-data */
    A_UINT32
        msg_type:   8, /* HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE */
        reserved1:  8,
        op_code:   16;
    A_UINT32
        rsp_len:   16,
        reserved2: 16;
} POSTPACK;

#define HTT_WDI_IPA_OP_RESPONSE_SZ                    8 /* bytes */

#define HTT_WDI_IPA_OP_RESPONSE_OP_CODE_M             0xffff0000
#define HTT_WDI_IPA_OP_RESPONSE_OP_CODE_S             16

#define HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_M             0x0000ffff
#define HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_S             0

#define HTT_WDI_IPA_OP_RESPONSE_OP_CODE_GET(_var) \
    (((_var) & HTT_WDI_IPA_OP_RESPONSE_OP_CODE_M) >> HTT_WDI_IPA_OP_RESPONSE_OP_CODE_S)
#define HTT_WDI_IPA_OP_RESPONSE_OP_CODE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_OP_RESPONSE_OP_CODE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_OP_RESPONSE_OP_CODE_S)); \
    } while (0)

#define HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_GET(_var) \
    (((_var) & HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_M) >> HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_S)
#define HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_OP_RESPONSE_RSP_LEN, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_S)); \
    } while (0)


#endif
