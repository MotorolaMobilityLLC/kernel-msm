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
#include <htt_common.h>

/*
 * Unless explicitly specified to use 64 bits to represent physical addresses
 * (or more precisely, bus addresses), default to 32 bits.
 */
#ifndef HTT_PADDR64
    #define HTT_PADDR64 0
#endif

#ifndef offsetof
#define offsetof(type, field)   ((unsigned int)(&((type *)0)->field))
#endif

/*
 * HTT version history:
 * 1.0  initial numbered version
 * 1.1  modifications to STATS messages.
 *      These modifications are not backwards compatible, but since the
 *      STATS messages themselves are non-essential (they are for debugging),
 *      the 1.1 version of the HTT message library as a whole is compatible
 *      with the 1.0 version.
 * 1.2  reset mask IE added to STATS_REQ message
 * 1.3  stat config IE added to STATS_REQ message
 *----
 * 2.0  FW rx PPDU desc added to RX_IND message
 * 2.1  Enable msdu_ext/frag_desc banking change for WIFI2.0
 *----
 * 3.0  Remove HTT_H2T_MSG_TYPE_MGMT_TX message
 * 3.1  Added HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND message
 * 3.2  Added HTT_H2T_MSG_TYPE_WDI_IPA_CFG,
 *            HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQUEST messages
 * 3.3  Added HTT_H2T_MSG_TYPE_AGGR_CFG_EX message
 * 3.4  Added tx_compl_req flag in HTT tx descriptor
 * 3.5  Added flush and fail stats in rx_reorder stats structure
 * 3.6  Added frag flag in HTT RX INORDER PADDR IND header
 * 3.7  Made changes to support EOS Mac_core 3.0
 * 3.8  Added txq_group information element definition;
 *      added optional txq_group suffix to TX_CREDIT_UPDATE_IND message
 * 3.9  Added HTT_T2H CHAN_CHANGE message;
 *      Allow buffer addresses in bus-address format to be stored as
 *      either 32 bits or 64 bits.
 * 3.10 Add optional TLV extensions to the VERSION_REQ and VERSION_CONF
 *      messages to specify which HTT options to use.
 *      Initial TLV options cover:
 *        - whether to use 32 or 64 bits to represent LL bus addresses
 *        - whether to use TX_COMPL_IND or TX_CREDIT_UPDATE_IND in HL systems
 *        - how many tx queue groups to use
 * 3.11 Expand rx debug stats:
 *        - Expand the rx_reorder_stats struct with stats about successful and
 *          failed rx buffer allcoations.
 *        - Add a new rx_remote_buffer_mgmt_stats struct with stats about
 *          the supply, allocation, use, and recycling of rx buffers for the
 *          "remote ring" of rx buffers in host member in LL systems.
 *          Add RX_REMOTE_RING_BUFFER_INFO stats type for uploading these stats.
 * 3.12 Add "rx offload packet error" message with initial "MIC error" subtype
 * 3.13 Add constants + macros to support 64-bit address format for the
 *      tx fragments descriptor, the rx ring buffer, and the rx ring
 *      index shadow register.
 * 3.14 Add a method for the host to provide detailed per-frame tx specs:
 *        - Add htt_tx_msdu_desc_ext_t struct def.
 *        - Add TLV to specify whether the target supports the HTT tx MSDU
 *          extension descriptor.
 *        - Change a reserved bit in the HTT tx MSDU descriptor to an
 *          "extension" bit, to specify whether a HTT tx MSDU extension
 *          descriptor is present.
 * 3.15 Add HW rx desc info to per-MSDU info elems in RX_IN_ORD_PADDR_IND msg.
 *      (This allows the host to obtain key information about the MSDU
 *      from a memory location already in the cache, rather than taking a
 *      cache miss for each MSDU by reading the HW rx descs.)
 * 3.16 Add htt_pkt_type_eth2 and define pkt_subtype flags to indicate
 *      whether a copy-engine classification result is appended to TX_FRM.
 * 3.17 Add a version of the WDI_IPA_CFG message; add RX_RING2 to WDI_IPA_CFG
 * 3.18 Add a PEER_DEL tx completion indication status, for HL cleanup of
 *      tx frames in the target after the peer has already been deleted.
 * 3.19 Add HTT_DBG_STATS_RX_RATE_INFO_V2 and HTT_DBG_STATS_TX_RATE_INFO_V2
 * 3.20 Expand rx_reorder_stats.
 * 3.21 Add optional rx channel spec to HL RX_IND.
 * 3.22 Expand rx_reorder_stats
 *      (distinguish duplicates within vs. outside block ack window)
 * 3.23 Add HTT_T2H_MSG_TYPE_RATE_REPORT to report peer justified rate.
 *      The justified rate is calculated by two steps. The first is to
 *      multiply user-rate by (1 - PER) and the other is to smooth the
 *      step 1's result by a low pass filter.
 *      This change allows HL download scheduling to consider the WLAN
 *      rate that will be used for transmitting the downloaded frames.
 * 3.24 Expand rx_reorder_stats
 *      (add counter for decrypt / MIC errors)
 * 3.25 Expand rx_reorder_stats
 *      (add counter of frames received into both local + remote rings)
 * 3.26 Add stats struct for counting rx of tx BF, MU, SU, and NDPA frames
 *      (HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT, rx_txbf_musu_ndpa_pkts_stats)
 * 3.27 Add a new interface for flow-control. The following t2h messages have
 *      been included: HTT_T2H_MSG_TYPE_FLOW_POOL_MAP and
 *      HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP
 */
#define HTT_CURRENT_VERSION_MAJOR 3
#define HTT_CURRENT_VERSION_MINOR 27

#define HTT_NUM_TX_FRAG_DESC  1024

#define HTT_WIFI_IP_VERSION(x,y) ((x) == (y))

#define HTT_CHECK_SET_VAL(field, val) \
    A_ASSERT(!((val) & ~((field ## _M) >> (field ## _S))))

/* macros to assist in sign-extending fields from HTT messages */
#define HTT_SIGN_BIT_MASK(field) \
    ((field ## _M + (1 << field ## _S)) >> 1)
#define HTT_SIGN_BIT(_val, field) \
    (_val & HTT_SIGN_BIT_MASK(field))
#define HTT_SIGN_BIT_UNSHIFTED(_val, field) \
    (HTT_SIGN_BIT(_val, field) >> field ## _S)
#define HTT_SIGN_BIT_UNSHIFTED_MINUS_ONE(_val, field) \
    (HTT_SIGN_BIT_UNSHIFTED(_val, field) - 1)
#define HTT_SIGN_BIT_EXTENSION(_val, field) \
    (~(HTT_SIGN_BIT_UNSHIFTED(_val, field) | \
    HTT_SIGN_BIT_UNSHIFTED_MINUS_ONE(_val, field)))
#define HTT_SIGN_BIT_EXTENSION_MASK(_val, field) \
    (HTT_SIGN_BIT_EXTENSION(_val, field) & ~(field ## _M >> field ## _S))


/*
 * TEMPORARY:
 * Provide HTT_H2T_MSG_TYPE_MGMT_TX as an alias for
 * DEPRECATED_HTT_H2T_MSG_TYPE_MGMT_TX until all code
 * that refers to HTT_H2T_MSG_TYPE_MGMT_TX has been
 * updated.
 */
#define HTT_H2T_MSG_TYPE_MGMT_TX DEPRECATED_HTT_H2T_MSG_TYPE_MGMT_TX

/*
 * TEMPORARY:
 * Provide HTT_T2H_MSG_TYPE_RC_UPDATE_IND as an alias for
 * DEPRECATED_HTT_T2H_MSG_TYPE_RC_UPDATE_IND until all code
 * that refers to HTT_T2H_MSG_TYPE_RC_UPDATE_IND has been
 * updated.
 */
#define HTT_T2H_MSG_TYPE_RC_UPDATE_IND DEPRECATED_HTT_T2H_MSG_TYPE_RC_UPDATE_IND

/* HTT Access Category values */
enum HTT_AC_WMM {
    /* WMM Access Categories */
    HTT_AC_WMM_BE         = 0x0,
    HTT_AC_WMM_BK         = 0x1,
    HTT_AC_WMM_VI         = 0x2,
    HTT_AC_WMM_VO         = 0x3,
    /* extension Access Categories */
    HTT_AC_EXT_NON_QOS    = 0x4,
    HTT_AC_EXT_UCAST_MGMT = 0x5,
    HTT_AC_EXT_MCAST_DATA = 0x6,
    HTT_AC_EXT_MCAST_MGMT = 0x7,
};
enum HTT_AC_WMM_MASK {
    /* WMM Access Categories */
    HTT_AC_WMM_BE_MASK = (1 << HTT_AC_WMM_BE),
    HTT_AC_WMM_BK_MASK = (1 << HTT_AC_WMM_BK),
    HTT_AC_WMM_VI_MASK = (1 << HTT_AC_WMM_VI),
    HTT_AC_WMM_VO_MASK = (1 << HTT_AC_WMM_VO),
    /* extension Access Categories */
    HTT_AC_EXT_NON_QOS_MASK    = (1 << HTT_AC_EXT_NON_QOS),
    HTT_AC_EXT_UCAST_MGMT_MASK = (1 << HTT_AC_EXT_UCAST_MGMT),
    HTT_AC_EXT_MCAST_DATA_MASK = (1 << HTT_AC_EXT_MCAST_DATA),
    HTT_AC_EXT_MCAST_MGMT_MASK = (1 << HTT_AC_EXT_MCAST_MGMT),
};
#define HTT_AC_MASK_WMM \
    (HTT_AC_WMM_BE_MASK | HTT_AC_WMM_BK_MASK | \
     HTT_AC_WMM_VI_MASK | HTT_AC_WMM_VO_MASK)
#define HTT_AC_MASK_EXT \
    (HTT_AC_EXT_NON_QOS_MASK | HTT_AC_EXT_UCAST_MGMT_MASK | \
    HTT_AC_EXT_MCAST_DATA_MASK | HTT_AC_EXT_MCAST_MGMT_MASK)
#define HTT_AC_MASK_ALL (HTT_AC_MASK_WMM | HTT_AC_MASK_EXT)

/*
 * htt_dbg_stats_type -
 * bit positions for each stats type within a stats type bitmask
 * The bitmask contains 24 bits.
 */
enum htt_dbg_stats_type {
    HTT_DBG_STATS_WAL_PDEV_TXRX              =  0, /* bit 0  ->    0x1 */
    HTT_DBG_STATS_RX_REORDER                 =  1, /* bit 1  ->    0x2 */
    HTT_DBG_STATS_RX_RATE_INFO               =  2, /* bit 2  ->    0x4 */
    HTT_DBG_STATS_TX_PPDU_LOG                =  3, /* bit 3  ->    0x8 */
    HTT_DBG_STATS_TX_RATE_INFO               =  4, /* bit 4  ->   0x10 */
    HTT_DBG_STATS_TIDQ                       =  5, /* bit 5  ->   0x20 */
    HTT_DBG_STATS_TXBF_INFO                  =  6, /* bit 6  ->   0x40 */
    HTT_DBG_STATS_SND_INFO                   =  7, /* bit 7  ->   0x80 */
    HTT_DBG_STATS_ERROR_INFO                 =  8, /* bit 8  ->  0x100 */
    HTT_DBG_STATS_TX_SELFGEN_INFO            =  9, /* bit 9  ->  0x200 */
    HTT_DBG_STATS_TX_MU_INFO                 = 10, /* bit 10 ->  0x400 */
    HTT_DBG_STATS_SIFS_RESP_INFO             = 11, /* bit 11 ->  0x800 */
    HTT_DBG_STATS_RX_REMOTE_RING_BUFFER_INFO = 12, /* bit 12 -> 0x1000*/
    HTT_DBG_STATS_RX_RATE_INFO_V2            = 13, /* bit 13 -> 0x2000 */
    HTT_DBG_STATS_TX_RATE_INFO_V2            = 14, /* bit 14 -> 0x4000 */
    HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT         = 15, /* bit 15 -> 0x8000 */
    /* bits 16-23 currently reserved */

    /* keep this last */
    HTT_DBG_NUM_STATS
};

/*=== HTT option selection TLVs ===
 * Certain HTT messages have alternatives or options.
 * For such cases, the host and target need to agree on which option to use.
 * Option specification TLVs can be appended to the VERSION_REQ and
 * VERSION_CONF messages to select options other than the default.
 * These TLVs are entirely optional - if they are not provided, there is a
 * well-defined default for each option.  If they are provided, they can be
 * provided in any order.  Each TLV can be present or absent independent of
 * the presence / absence of other TLVs.
 *
 * The HTT option selection TLVs use the following format:
 *     |31                             16|15             8|7              0|
 *     |---------------------------------+----------------+----------------|
 *     |        value (payload)          |     length     |       tag      |
 *     |-------------------------------------------------------------------|
 * The value portion need not be only 2 bytes; it can be extended by any
 * integer number of 4-byte units.  The total length of the TLV, including
 * the tag and length fields, must be a multiple of 4 bytes.  The length
 * field specifies the total TLV size in 4-byte units.  Thus, the typical
 * TLV, with a 1-byte tag field, a 1-byte length field, and a 2-byte value
 * field, would store 0x1 in its length field, to show that the TLV occupies
 * a single 4-byte unit.
 */

/*--- TLV header format - applies to all HTT option TLVs ---*/

enum HTT_OPTION_TLV_TAGS {
    HTT_OPTION_TLV_TAG_RESERVED0                = 0x0,
    HTT_OPTION_TLV_TAG_LL_BUS_ADDR_SIZE         = 0x1,
    HTT_OPTION_TLV_TAG_HL_SUPPRESS_TX_COMPL_IND = 0x2,
    HTT_OPTION_TLV_TAG_MAX_TX_QUEUE_GROUPS      = 0x3,
    HTT_OPTION_TLV_TAG_SUPPORT_TX_MSDU_DESC_EXT = 0x4,
};

PREPACK struct htt_option_tlv_header_t {
    A_UINT8 tag;
    A_UINT8 length;
} POSTPACK;

#define HTT_OPTION_TLV_TAG_M      0x000000ff
#define HTT_OPTION_TLV_TAG_S      0
#define HTT_OPTION_TLV_LENGTH_M   0x0000ff00
#define HTT_OPTION_TLV_LENGTH_S   8
/*
 * value0 - 16 bit value field stored in word0
 * The TLV's value field may be longer than 2 bytes, in which case
 * the remainder of the value is stored in word1, word2, etc.
 */
#define HTT_OPTION_TLV_VALUE0_M   0xffff0000
#define HTT_OPTION_TLV_VALUE0_S   16

#define HTT_OPTION_TLV_TAG_SET(word, tag)           \
    do {                                            \
        HTT_CHECK_SET_VAL(HTT_OPTION_TLV_TAG, tag); \
        (word) |= ((tag) << HTT_OPTION_TLV_TAG_S);  \
    } while (0)
#define HTT_OPTION_TLV_TAG_GET(word) \
    (((word) & HTT_OPTION_TLV_TAG_M) >> HTT_OPTION_TLV_TAG_S)

#define HTT_OPTION_TLV_LENGTH_SET(word, tag)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_OPTION_TLV_LENGTH, tag); \
        (word) |= ((tag) << HTT_OPTION_TLV_LENGTH_S);  \
    } while (0)
#define HTT_OPTION_TLV_LENGTH_GET(word) \
    (((word) & HTT_OPTION_TLV_LENGTH_M) >> HTT_OPTION_TLV_LENGTH_S)

#define HTT_OPTION_TLV_VALUE0_SET(word, tag)           \
    do {                                               \
        HTT_CHECK_SET_VAL(HTT_OPTION_TLV_VALUE0, tag); \
        (word) |= ((tag) << HTT_OPTION_TLV_VALUE0_S);  \
    } while (0)
#define HTT_OPTION_TLV_VALUE0_GET(word) \
    (((word) & HTT_OPTION_TLV_VALUE0_M) >> HTT_OPTION_TLV_VALUE0_S)

/*--- format of specific HTT option TLVs ---*/

/*
 * HTT option TLV for specifying LL bus address size
 * Some chips require bus addresses used by the target to access buffers
 * within the host's memory to be 32 bits; others require bus addresses
 * used by the target to access buffers within the host's memory to be
 * 64 bits.
 * The LL_BUS_ADDR_SIZE TLV can be sent from the target to the host as
 * a suffix to the VERSION_CONF message to specify which bus address format
 * the target requires.
 * If this LL_BUS_ADDR_SIZE TLV is not sent by the target, the host should
 * default to providing bus addresses to the target in 32-bit format.
 */
enum HTT_OPTION_TLV_LL_BUS_ADDR_SIZE_VALUES {
    HTT_OPTION_TLV_LL_BUS_ADDR_SIZE32 = 0x0,
    HTT_OPTION_TLV_LL_BUS_ADDR_SIZE64 = 0x1,
};
PREPACK struct htt_option_tlv_ll_bus_addr_size_t {
    struct htt_option_tlv_header_t hdr;
    A_UINT16 ll_bus_addr_size; /* LL_BUS_ADDR_SIZE_VALUES enum */
} POSTPACK;

/*
 * HTT option TLV for specifying whether HL systems should indicate
 * over-the-air tx completion for individual frames, or should instead
 * send a bulk TX_CREDIT_UPDATE_IND except when the host explicitly
 * requests an OTA tx completion for a particular tx frame.
 * This option does not apply to LL systems, where the TX_COMPL_IND
 * is mandatory.
 * This option is primarily intended for HL systems in which the tx frame
 * downloads over the host --> target bus are as slow as or slower than
 * the transmissions over the WLAN PHY.  For cases where the bus is faster
 * than the WLAN PHY, the target will transmit relatively large A-MPDUs,
 * and consquently will send one TX_COMPL_IND message that covers several
 * tx frames.  For cases where the WLAN PHY is faster than the bus,
 * the target will end up transmitting very short A-MPDUs, and consequently
 * sending many TX_COMPL_IND messages, which each cover a very small number
 * of tx frames.
 * The HL_SUPPRESS_TX_COMPL_IND TLV can be sent by the host to the target as
 * a suffix to the VERSION_REQ message to request whether the host desires to
 * use TX_CREDIT_UPDATE_IND rather than TX_COMPL_IND.  The target can then
 * send a HTT_SUPPRESS_TX_COMPL_IND TLV to the host as a suffix to the
 * VERSION_CONF message to confirm whether TX_CREDIT_UPDATE_IND will be used
 * rather than TX_COMPL_IND.  TX_CREDIT_UPDATE_IND shall only be used if the
 * host sends a HL_SUPPRESS_TX_COMPL_IND TLV requesting use of
 * TX_CREDIT_UPDATE_IND, and the target sends a HL_SUPPRESS_TX_COMPLE_IND TLV
 * back to the host confirming use of TX_CREDIT_UPDATE_IND.
 * Lack of a HL_SUPPRESS_TX_COMPL_IND TLV from either host --> target or
 * target --> host is equivalent to a HL_SUPPRESS_TX_COMPL_IND that
 * explicitly specifies HL_ALLOW_TX_COMPL_IND in the value payload of the
 * TLV.
 */
enum HTT_OPTION_TLV_HL_SUPPRESS_TX_COMPL_IND_VALUES {
    HTT_OPTION_TLV_HL_ALLOW_TX_COMPL_IND = 0x0,
    HTT_OPTION_TLV_HL_SUPPRESS_TX_COMPL_IND = 0x1,
};
PREPACK struct htt_option_tlv_hl_suppress_tx_compl_ind_t {
    struct htt_option_tlv_header_t hdr;
    A_UINT16 hl_suppress_tx_compl_ind; /* HL_SUPPRESS_TX_COMPL_IND enum */
} POSTPACK;

/*
 * HTT option TLV for specifying how many tx queue groups the target
 * may establish.
 * This TLV specifies the maximum value the target may send in the
 * txq_group_id field of any TXQ_GROUP information elements sent by
 * the target to the host.  This allows the host to pre-allocate an
 * appropriate number of tx queue group structs.
 *
 * The MAX_TX_QUEUE_GROUPS_TLV can be sent from the host to the target as
 * a suffix to the VERSION_REQ message to specify whether the host supports
 * tx queue groups at all, and if so if there is any limit on the number of
 * tx queue groups that the host supports.
 * The MAX_TX_QUEUE_GROUPS TLV can be sent from the target to the host as
 * a suffix to the VERSION_CONF message.  If the host has specified in the
 * VER_REQ message a limit on the number of tx queue groups the host can
 * supprt, the target shall limit its specification of the maximum tx groups
 * to be no larger than this host-specified limit.
 *
 * If the target does not provide a MAX_TX_QUEUE_GROUPS TLV, then the host
 * shall preallocate 4 tx queue group structs, and the target shall not
 * specify a txq_group_id larger than 3.
 */
enum HTT_OPTION_TLV_MAX_TX_QUEUE_GROUPS_VALUES {
    HTT_OPTION_TLV_TX_QUEUE_GROUPS_UNSUPPORTED = 0,
    /*
     * values 1 through N specify the max number of tx queue groups
     * the sender supports
     */
    HTT_OPTION_TLV_TX_QUEUE_GROUPS_UNLIMITED = 0xffff,
};
/* TEMPORARY backwards-compatibility alias for a typo fix -
 * The htt_option_tlv_mac_tx_queue_groups_t typo has been corrected
 * to  htt_option_tlv_max_tx_queue_groups_t, but an alias is provided
 * to support the old name (with the typo) until all references to the
 * old name are replaced with the new name.
 */
#define htt_option_tlv_mac_tx_queue_groups_t htt_option_tlv_max_tx_queue_groups_t
PREPACK struct htt_option_tlv_max_tx_queue_groups_t {
    struct htt_option_tlv_header_t hdr;
    A_UINT16 max_tx_queue_groups; /* max txq_group_id + 1 */
} POSTPACK;

/*
 * HTT option TLV for specifying whether the target supports an extended
 * version of the HTT tx descriptor.  If the target provides this TLV
 * and specifies in the TLV that the target supports an extended version
 * of the HTT tx descriptor, the target must check the "extension" bit in
 * the HTT tx descriptor, and if the extension bit is set, to expect a
 * HTT tx MSDU extension descriptor immediately following the HTT tx MSDU
 * descriptor.  Furthermore, the target must provide room for the HTT
 * tx MSDU extension descriptor in the target's TX_FRM buffer.
 * This option is intended for systems where the host needs to explicitly
 * control the transmission parameters such as tx power for individual
 * tx frames.
 * The SUPPORT_TX_MSDU_DESC_EXT TLB can be sent by the target to the host
 * as a suffix to the VERSION_CONF message to explicitly specify whether
 * the target supports the HTT tx MSDU extension descriptor.
 * Lack of a SUPPORT_TX_MSDU_DESC_EXT from the target shall be interpreted
 * by the host as lack of target support for the HTT tx MSDU extension
 * descriptor; the host shall provide HTT tx MSDU extension descriptors in
 * the HTT_H2T TX_FRM messages only if the target indicates it supports
 * the HTT tx MSDU extension descriptor.
 * The host is not required to provide the HTT tx MSDU extension descriptor
 * just because the target supports it; the target must check the
 * "extension" bit in the HTT tx MSDU descriptor to determine whether an
 * extension descriptor is present.
 */
enum HTT_OPTION_TLV_SUPPORT_TX_MSDU_DESC_EXT_VALUES {
    HTT_OPTION_TLV_TX_MSDU_DESC_EXT_NO_SUPPORT = 0x0,
    HTT_OPTION_TLV_TX_MSDU_DESC_EXT_SUPPORT = 0x1,
};
PREPACK struct htt_option_tlv_support_tx_msdu_desc_ext_t {
    struct htt_option_tlv_header_t hdr;
    A_UINT16 tx_msdu_desc_ext_support; /* SUPPORT_TX_MSDU_DESC_EXT enum */
} POSTPACK;


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
 *     :                    option request TLV (optional)                  |
 *     :...................................................................:
 *
 * The VER_REQ message may consist of a single 4-byte word, or may be
 * extended with TLVs that specify which HTT options the host is requesting
 * from the target.
 * The following option TLVs may be appended to the VER_REQ message:
 *   - HL_SUPPRESS_TX_COMPL_IND
 *   - HL_MAX_TX_QUEUE_GROUPS
 * These TLVs may appear in an arbitrary order.  Any number of these TLVs
 * may be appended to the VER_REQ message (but only one TLV of each type).
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

#define HTT_TX_VDEV_ID_WORD 0
#define HTT_TX_VDEV_ID_MASK 0x3f
#define HTT_TX_VDEV_ID_SHIFT 16

#define HTT_TX_L3_CKSUM_OFFLOAD      1
#define HTT_TX_L4_CKSUM_OFFLOAD      2

#define HTT_TX_MSDU_LEN_DWORD 1
#define HTT_TX_MSDU_LEN_MASK 0xffff;

/*
 * HTT_VAR_PADDR macros
 * Allow physical / bus addresses to be either a single 32-bit value,
 * or a 64-bit value, stored as a little-endian lo,hi pair of 32-bit parts
 */
#define HTT_VAR_PADDR32(var_name) \
    A_UINT32 var_name
#define HTT_VAR_PADDR64_LE(var_name)        \
    struct {                                \
        /* little-endian: lo precedes hi */ \
        A_UINT32 lo;                        \
        A_UINT32 hi;                        \
    } var_name

/*
 * TEMPLATE_HTT_TX_MSDU_DESC_T:
 * This macro defines a htt_tx_msdu_descXXX_t in which any physical
 * addresses are stored in a XXX-bit field.
 * This macro is used to define both htt_tx_msdu_desc32_t and
 * htt_tx_msdu_desc64_t structs.
 */
#define TEMPLATE_HTT_TX_MSDU_DESC_T(_paddr_bits_, _paddr__frags_desc_ptr_)     \
PREPACK struct htt_tx_msdu_desc ## _paddr_bits_ ## _t                          \
{                                                                              \
    /* DWORD 0: flags and meta-data */                                         \
    A_UINT32                                                                   \
        msg_type: 8, /* HTT_H2T_MSG_TYPE_TX_FRM */                             \
                                                                               \
        /* pkt_subtype -                                                       \
         * Detailed specification of the tx frame contents, extending the      \
         * general specification provided by pkt_type.                         \
         * FIX THIS: ADD COMPLETE SPECS FOR THIS FIELDS VALUE, e.g.            \
         *     pkt_type    | pkt_subtype                                       \
         *     ==============================================================  \
         *     802.3       | bit 0:3    - Reserved                             \
         *                 | bit 4: 0x0 - Copy-Engine Classification Results   \
         *                 |              not appended to the HTT message      \
         *                 |        0x1 - Copy-Engine Classification Results   \
         *                 |              appended to the HTT message in the   \
         *                 |              format:                              \
         *                 |              [HTT tx desc, frame header,          \
         *                 |              CE classification results]           \
         *                 |              The CE classification results begin  \
         *                 |              at the next 4-byte boundary after    \
         *                 |              the frame header.                    \
         *     ------------+-------------------------------------------------  \
         *     Eth2        | bit 0:3    - Reserved                             \
         *                 | bit 4: 0x0 - Copy-Engine Classification Results   \
         *                 |              not appended to the HTT message      \
         *                 |        0x1 - Copy-Engine Classification Results   \
         *                 |              appended to the HTT message.         \
         *                 |              See the above specification of the   \
         *                 |              CE classification results location.  \
         *     ------------+-------------------------------------------------  \
         *     native WiFi | bit 0:3    - Reserved                             \
         *                 | bit 4: 0x0 - Copy-Engine Classification Results   \
         *                 |              not appended to the HTT message      \
         *                 |        0x1 - Copy-Engine Classification Results   \
         *                 |              appended to the HTT message.         \
         *                 |              See the above specification of the   \
         *                 |              CE classification results location.  \
         *     ------------+-------------------------------------------------  \
         *     mgmt        | 0x0 - 802.11 MAC header absent                    \
         *                 | 0x1 - 802.11 MAC header present                   \
         *     ------------+-------------------------------------------------  \
         *     raw         | bit 0: 0x0 - 802.11 MAC header absent             \
         *                 |        0x1 - 802.11 MAC header present            \
         *                 | bit 1: 0x0 - allow aggregation                    \
         *                 |        0x1 - don't allow aggregation              \
         *                 | bit 2: 0x0 - perform encryption                   \
         *                 |        0x1 - don't perform encryption             \
         *                 | bit 3: 0x0 - perform tx classification / queuing  \
         *                 |        0x1 - don't perform tx classification;     \
         *                 |              insert the frame into the "misc"     \
         *                 |              tx queue                             \
         *                 | bit 4: 0x0 - Copy-Engine Classification Results   \
         *                 |              not appended to the HTT message      \
         *                 |        0x1 - Copy-Engine Classification Results   \
         *                 |              appended to the HTT message.         \
         *                 |              See the above specification of the   \
         *                 |              CE classification results location.  \
         */                                                                    \
        pkt_subtype: 5,                                                        \
                                                                               \
        /* pkt_type -                                                          \
         * General specification of the tx frame contents.                     \
         * The htt_pkt_type enum should be used to specify and check the       \
         * value of this field.                                                \
         */                                                                    \
        pkt_type: 3,                                                           \
                                                                               \
        /* vdev_id -                                                           \
         * ID for the vdev that is sending this tx frame.                      \
         * For certain non-standard packet types, e.g. pkt_type == raw         \
         * and (pkt_subtype >> 3) == 1, this field is not relevant/valid.      \
         * This field is used primarily for determining where to queue         \
         * broadcast and multicast frames.                                     \
         */                                                                    \
        vdev_id: 6,                                                            \
        /* ext_tid -                                                           \
         * The extended traffic ID.                                            \
         * If the TID is unknown, the extended TID is set to                   \
         * HTT_TX_EXT_TID_INVALID.                                             \
         * If the tx frame is QoS data, then the extended TID has the 0-15     \
         * value of the QoS TID.                                               \
         * If the tx frame is non-QoS data, then the extended TID is set to    \
         * HTT_TX_EXT_TID_NON_QOS.                                             \
         * If the tx frame is multicast or broadcast, then the extended TID    \
         * is set to HTT_TX_EXT_TID_MCAST_BCAST.                               \
         */                                                                    \
        ext_tid: 5,                                                            \
                                                                               \
        /* postponed -                                                         \
         * This flag indicates whether the tx frame has been downloaded to     \
         * the target before but discarded by the target, and now is being     \
         * downloaded again; or if this is a new frame that is being           \
         * downloaded for the first time.                                      \
         * This flag allows the target to determine the correct order for      \
         * transmitting new vs. old frames.                                    \
         * value: 0 -> new frame, 1 -> re-send of a previously sent frame      \
         * This flag only applies to HL systems, since in LL systems,          \
         * the tx flow control is handled entirely within the target.          \
         */                                                                    \
        postponed: 1,                                                          \
                                                                               \
        /* extension -                                                         \
         * This flag indicates whether a HTT tx MSDU extension descriptor      \
         * (htt_tx_msdu_desc_ext_t) follows this HTT tx MSDU descriptor.       \
         *                                                                     \
         * 0x0 - no extension MSDU descriptor is present                       \
         * 0x1 - an extension MSDU descriptor immediately follows the          \
         *       regular MSDU descriptor                                       \
         */                                                                    \
        extension: 1,                                                          \
                                                                               \
        /* cksum_offload -                                                     \
         * This flag indicates whether checksum offload is enabled or not      \
         * for this frame. Target FW use this flag to turn on HW checksumming  \
         *  0x0 - No checksum offload                                          \
         *  0x1 - L3 header checksum only                                      \
         *  0x2 - L4 checksum only                                             \
         *  0x3 - L3 header checksum + L4 checksum                             \
         */                                                                    \
        cksum_offload: 2,                                                      \
                                                                               \
        /* tx_comp_req -                                                       \
         * This flag indicates whether Tx Completion                           \
         * from fw is required or not.                                         \
         * This  flag is only relevant if tx completion is not                 \
         * universally enabled.                                                \
         * For all LL systems, tx completion is mandatory,                     \
         * so this flag will be irrelevant.                                    \
         * For HL systems tx completion is optional, but HL systems in which   \
         * the bus throughput exceeds the WLAN throughput will                 \
         * probably want to always use tx completion, and thus                 \
         * would not check this flag.                                          \
         * This flag is required when tx completions are not used universally, \
         * but are still required for certain tx frames for which              \
         * an OTA delivery acknowledgment is needed by the host.               \
         * In practice, this would be for HL systems in which the              \
         * bus throughput is less than the WLAN throughput.                    \
         *                                                                     \
         * 0x0 - Tx Completion Indication from Fw not required                 \
         * 0x1 - Tx Completion Indication from Fw is required                  \
         */                                                                    \
        tx_compl_req: 1;                                                       \
                                                                               \
                                                                               \
        /* DWORD 1: MSDU length and ID */                                      \
        A_UINT32                                                               \
            len: 16, /* MSDU length, in bytes */                               \
            id:  16; /* MSDU ID used to identify the MSDU to the host,         \
                      * and this id is used to calculate fragmentation         \
                      * descriptor pointer inside the target based on          \
                      * the base address, configured inside the target.        \
                      */                                                       \
                                                                               \
        /* DWORD 2 (or 2-3): fragmentation descriptor bus address */           \
        /* frags_desc_ptr -                                                    \
         * The fragmentation descriptor pointer tells the HW's MAC DMA         \
         * where the tx frame's fragments reside in memory.                    \
         * This field only applies to LL systems, since in HL systems the      \
         * (degenerate single-fragment) fragmentation descriptor is created    \
         * within the target.                                                  \
         */                                                                    \
        _paddr__frags_desc_ptr_;                                               \
                                                                               \
        /* DWORD 3 (or 4): peerid, chanfreq */                                 \
        /*                                                                     \
         * Peer ID : Target can use this value to know which peer-id packet    \
         *           destined to.                                              \
         *           It's intended to be specified by host in case of NAWDS.   \
         */                                                                    \
        A_UINT16 peerid;                                                       \
                                                                               \
        /*                                                                     \
         * Channel frequency: This identifies the desired channel              \
         * frequency (in mhz) for tx frames. This is used by FW to help        \
         * determine when it is safe to transmit or drop frames for            \
         * off-channel operation.                                              \
         * The default value of zero indicates to FW that the corresponding    \
         * VDEV's home channel (if there is one) is the desired channel        \
         * frequency.                                                          \
         */                                                                    \
        A_UINT16 chanfreq;                                                     \
                                                                               \
        /* Reason reserved is commented is increasing the htt structure size   \
         * leads to some wierd issues. Contact Raj/Kyeyoon for more info       \
         * A_UINT32 reserved_dword3_bits0_31;                                  \
         */                                                                    \
} POSTPACK
/* define a htt_tx_msdu_desc32_t type */
TEMPLATE_HTT_TX_MSDU_DESC_T(32, HTT_VAR_PADDR32(frags_desc_ptr));
/* define a htt_tx_msdu_desc64_t type */
TEMPLATE_HTT_TX_MSDU_DESC_T(64, HTT_VAR_PADDR64_LE(frags_desc_ptr));
/*
 * Make htt_tx_msdu_desc_t be an alias for either
 * htt_tx_msdu_desc32_t or htt_tx_msdu_desc64_t
 */
#if HTT_PADDR64
    #define htt_tx_msdu_desc_t htt_tx_msdu_desc64_t
#else
    #define htt_tx_msdu_desc_t htt_tx_msdu_desc32_t
#endif

/* decriptor information for Management frame*/
/*
 * THIS htt_mgmt_tx_desc_t STRUCT IS DEPRECATED - DON'T USE IT.
 * BOTH MANAGEMENT AND DATA FRAMES SHOULD USE htt_tx_msdu_desc_t.
 */
#define HTT_MGMT_FRM_HDR_DOWNLOAD_LEN    32
extern A_UINT32 mgmt_hdr_len;
PREPACK struct htt_mgmt_tx_desc_t {
    A_UINT32    msg_type;
#if HTT_PADDR64
    A_UINT64    frag_paddr; /* DMAble address of the data */
#else
    A_UINT32    frag_paddr; /* DMAble address of the data */
#endif
    A_UINT32    desc_id;    /* returned to host during completion
                             * to free the meory*/
    A_UINT32    len;    /* Fragment length */
    A_UINT32    vdev_id; /* virtual device ID*/
    A_UINT8     hdr[HTT_MGMT_FRM_HDR_DOWNLOAD_LEN]; /* frm header */
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
/* for systems using 64-bit format for bus addresses */
#define HTT_TX_DESC_FRAGS_DESC_PADDR_HI_M 0xffffffff
#define HTT_TX_DESC_FRAGS_DESC_PADDR_HI_S 0
#define HTT_TX_DESC_FRAGS_DESC_PADDR_LO_M 0xffffffff
#define HTT_TX_DESC_FRAGS_DESC_PADDR_LO_S 0
/* for systems using 32-bit format for bus addresses */
#define HTT_TX_DESC_FRAGS_DESC_PADDR_M    0xffffffff
#define HTT_TX_DESC_FRAGS_DESC_PADDR_S    0

/* dword 3 */
#define HTT_TX_DESC_PEER_ID_OFFSET_BYTES_64 16
#define HTT_TX_DESC_PEER_ID_OFFSET_BYTES_32 12
#define HTT_TX_DESC_PEER_ID_OFFSET_DWORD_64 \
        (HTT_TX_DESC_PEER_ID_OFFSET_BYTES_64 >> 2)
#define HTT_TX_DESC_PEER_ID_OFFSET_DWORD_32 \
        (HTT_TX_DESC_PEER_ID_OFFSET_BYTES_32 >> 2)

#if HTT_PADDR64
#define HTT_TX_DESC_PEER_ID_OFFSET_BYTES HTT_TX_DESC_PEER_ID_OFFSET_BYTES_64
#define HTT_TX_DESC_PEER_ID_OFFSET_DWORD HTT_TX_DESC_PEER_ID_OFFSET_DWORD_64
#else
#define HTT_TX_DESC_PEER_ID_OFFSET_BYTES HTT_TX_DESC_PEER_ID_OFFSET_BYTES_32
#define HTT_TX_DESC_PEER_ID_OFFSET_DWORD HTT_TX_DESC_PEER_ID_OFFSET_DWORD_32
#endif

#define HTT_TX_DESC_PEER_ID_M 0x0000ffff
#define HTT_TX_DESC_PEER_ID_S 0
    /*
     * TEMPORARY:
     * The original definitions for the PEER_ID fields contained typos
     * (with _DESC_PADDR appended to this PEER_ID field name).
     * Retain deprecated original names for PEER_ID fields until all code that
     * refers to them has been updated.
     */
    #define HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_BYTES \
        HTT_TX_DESC_PEER_ID_OFFSET_BYTES
    #define HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_DWORD \
        HTT_TX_DESC_PEER_ID_OFFSET_DWORD
    #define HTT_TX_DESC_PEERID_DESC_PADDR_M \
        HTT_TX_DESC_PEER_ID_M
    #define HTT_TX_DESC_PEERID_DESC_PADDR_S \
        HTT_TX_DESC_PEER_ID_S

#define HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_64 16 // to dword with chan freq
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_32 12 // to dword with chan freq
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD_64 \
        (HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_64 >> 2)
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD_32 \
        (HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_32 >> 2)

#if HTT_PADDR64
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_64
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD_64
#else
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES HTT_TX_DESC_CHAN_FREQ_OFFSET_BYTES_32
#define HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD HTT_TX_DESC_CHAN_FREQ_OFFSET_DWORD_32
#endif

#define HTT_TX_DESC_CHAN_FREQ_M 0xffff0000
#define HTT_TX_DESC_CHAN_FREQ_S 16

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

#define HTT_TX_DESC_PEER_ID_GET(_var) \
    (((_var) & HTT_TX_DESC_PEER_ID_M) >> HTT_TX_DESC_PEER_ID_S)
#define HTT_TX_DESC_PEER_ID_SET(_var, _val)             \
     do {                                               \
         HTT_CHECK_SET_VAL(HTT_TX_DESC_PEER_ID, _val);  \
         ((_var) |= ((_val) << HTT_TX_DESC_PEER_ID_S)); \
     } while (0)

#define HTT_TX_DESC_CHAN_FREQ_GET(_var) \
    (((_var) & HTT_TX_DESC_CHAN_FREQ_M) >> HTT_TX_DESC_CHAN_FREQ_S)
#define HTT_TX_DESC_CHAN_FREQ_SET(_var, _val)             \
     do {                                               \
         HTT_CHECK_SET_VAL(HTT_TX_DESC_CHAN_FREQ, _val);  \
         ((_var) |= ((_val) << HTT_TX_DESC_CHAN_FREQ_S)); \
     } while (0)


/* enums used in the HTT tx MSDU extension descriptor */
enum {
    htt_tx_guard_interval_regular = 0,
    htt_tx_guard_interval_short   = 1,
};

enum {
    htt_tx_preamble_type_ofdm = 0,
    htt_tx_preamble_type_cck  = 1,
    htt_tx_preamble_type_ht   = 2,
    htt_tx_preamble_type_vht  = 3,
};

enum {
    htt_tx_bandwidth_5MHz   = 0,
    htt_tx_bandwidth_10MHz  = 1,
    htt_tx_bandwidth_20MHz  = 2,
    htt_tx_bandwidth_40MHz  = 3,
    htt_tx_bandwidth_80MHz  = 4,
    htt_tx_bandwidth_160MHz = 5, /* includes 80+80 */
};

/**
 * @brief HTT tx MSDU extension descriptor
 * @details
 *  If the target supports HTT tx MSDU extension descriptors, the host has
 *  the option of appending the following struct following the regular
 *  HTT tx MSDU descriptor (and setting the "extension" flag in the regular
 *  HTT tx MSDU descriptor, to show that the extension descriptor is present).
 *  The HTT tx MSDU extension descriptors allows the host to provide detailed
 *  tx specs for each frame.
 */
PREPACK struct htt_tx_msdu_desc_ext_t {
    /* DWORD 0: flags */
    A_UINT32
        valid_pwr:            1, /* bit 0: if set, tx pwr spec is valid */
        valid_mcs_mask:       1, /* bit 1: if set, tx MCS mask spec is valid */
        valid_nss_mask:       1, /* bit 2: if set, tx Nss mask spec is valid */
        valid_guard_interval: 1, /* bit 3: if set, tx guard intv spec is valid*/
        valid_preamble_type_mask: 1, /* 4: if set, tx preamble mask is valid */
        valid_chainmask:      1, /* bit 5: if set, tx chainmask spec is valid */
        valid_retries:        1, /* bit 6: if set, tx retries spec is valid */
        valid_bandwidth:      1, /* bit 7: if set, tx bandwidth spec is valid */
        valid_expire_tsf:     1, /* bit 8: if set, tx expire TSF spec is valid*/
        is_dsrc:              1, /* bit 9: if set, MSDU is a DSRC frame */
        reserved0_31_7:      22; /* bits 31:10 - unused, set to 0x0 */

    /* DWORD 1: tx power, tx rate, tx BW */
    A_UINT32
        /* pwr -
         * Specify what power the tx frame needs to be transmitted at.
         * The power a signed (two's complement) value is in units of 0.5 dBm.
         * The value needs to be appropriately sign-extended when extracting
         * the value from the message and storing it in a variable that is
         * larger than A_INT8.  (The HTT_TX_MSDU_EXT_DESC_FLAG_PWR_GET macro
         * automatically handles this sign-extension.)
         * If the transmission uses multiple tx chains, this power spec is
         * the total transmit power, assuming incoherent combination of
         * per-chain power to produce the total power.
         */
        pwr: 8,

        /* mcs_mask -
         * Specify the allowable values for MCS index (modulation and coding)
         * to use for transmitting the frame.
         *
         * For HT / VHT preamble types, this mask directly corresponds to
         * the HT or VHT MCS indices that are allowed.  For each bit N set
         * within the mask, MCS index N is allowed for transmitting the frame.
         * For legacy CCK and OFDM rates, separate bits are provided for CCK
         * rates versus OFDM rates, so the host has the option of specifying
         * that the target must transmit the frame with CCK or OFDM rates
         * (not HT or VHT), but leaving the decision to the target whether
         * to use CCK or OFDM.
         *
         * For CCK and OFDM, the bits within this mask are interpreted as
         * follows:
         *     bit  0 -> CCK 1 Mbps rate is allowed
         *     bit  1 -> CCK 2 Mbps rate is allowed
         *     bit  2 -> CCK 5.5 Mbps rate is allowed
         *     bit  3 -> CCK 11 Mbps rate is allowed
         *     bit  4 -> OFDM BPSK modulation, 1/2 coding rate is allowed
         *     bit  5 -> OFDM BPSK modulation, 3/4 coding rate is allowed
         *     bit  6 -> OFDM QPSK modulation, 1/2 coding rate is allowed
         *     bit  7 -> OFDM QPSK modulation, 3/4 coding rate is allowed
         *     bit  8 -> OFDM 16-QAM modulation, 1/2 coding rate is allowed
         *     bit  9 -> OFDM 16-QAM modulation, 3/4 coding rate is allowed
         *     bit 10 -> OFDM 64-QAM modulation, 2/3 coding rate is allowed
         *     bit 11 -> OFDM 64-QAM modulation, 3/4 coding rate is allowed
         *
         * The MCS index specification needs to be compatible with the
         * bandwidth mask specification.  For example, a MCS index == 9
         * specification is inconsistent with a preamble type == VHT,
         * Nss == 1, and channel bandwidth == 20 MHz.
         *
         * Furthermore, the host has only a limited ability to specify to
         * the target to select from HT + legacy rates, or VHT + legacy rates,
         * since this mcs_mask can specify either HT/VHT rates or legacy rates.
         */
        mcs_mask: 12,

        /* nss_mask -
         * Specify which numbers of spatial streams (MIMO factor) are permitted.
         * Each bit in this mask corresponds to a Nss value:
         *     bit 0: if set, Nss = 1 (non-MIMO) is permitted
         *     bit 1: if set, Nss = 2 (2x2 MIMO) is permitted
         *     bit 2: if set, Nss = 3 (3x3 MIMO) is permitted
         *     bit 3: if set, Nss = 4 (4x4 MIMO) is permitted
         * The values in the Nss mask must be suitable for the recipient, e.g.
         * a value of 0x4 (Nss = 3) cannot be specified for a tx frame to a
         * recipient which only supports 2x2 MIMO.
         */
        nss_mask: 4,

        /* guard_interval -
         * Specify a htt_tx_guard_interval enum value to indicate whether
         * the transmission should use a regular guard interval or a
         * short guard interval.
         */
        guard_interval: 1,

        /* preamble_type_mask -
         * Specify which preamble types (CCK, OFDM, HT, VHT) the target
         * may choose from for transmitting this frame.
         * The bits in this mask correspond to the values in the
         * htt_tx_preamble_type enum.  For example, to allow the target
         * to transmit the frame as either CCK or OFDM, this field would
         * be set to
         *     (1 << htt_tx_preamble_type_ofdm) |
         *     (1 << htt_tx_preamble_type_cck)
         */
        preamble_type_mask: 4,

        reserved1_31_29: 3; /* unused, set to 0x0 */

    /* DWORD 2: tx chain mask, tx retries */
    A_UINT32
        /* chain_mask - specify which chains to transmit from */
        chain_mask: 4,

        /* retry_limit -
         * Specify the maximum number of transmissions, including the
         * initial transmission, to attempt before giving up if no ack
         * is received.
         * If the tx rate is specified, then all retries shall use the
         * same rate as the initial transmission.
         * If no tx rate is specified, the target can choose whether to
         * retain the original rate during the retransmissions, or to
         * fall back to a more robust rate.
         */
        retry_limit: 4,

        /* bandwidth_mask -
         * Specify what channel widths may be used for the transmission.
         * A value of zero indicates "don't care" - the target may choose
         * the transmission bandwidth.
         * The bits within this mask correspond to the htt_tx_bandwidth
         * enum values - bit 0 is for 5 MHz, bit 1 is for 10 MHz, etc.
         * The bandwidth_mask must be consistent with the preamble_type_mask
         * and mcs_mask specs, if they are provided.  For example, 80 MHz and
         * 160 MHz can only be enabled in the mask if preamble_type == VHT.
         */
        bandwidth_mask: 6,

        reserved2_31_14: 18; /* unused, set to 0x0 */

    /* DWORD 3: tx expiry time (TSF) LSBs */
    A_UINT32 expire_tsf_lo;

    /* DWORD 4: tx expiry time (TSF) MSBs */
    A_UINT32 expire_tsf_hi;

    A_UINT32 reserved_for_future_expansion_set_to_zero[3];
} POSTPACK;

/* DWORD 0 */
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_M                0x00000001
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_S                0
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_M           0x00000002
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_S           1
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_NSS_MASK_M           0x00000004
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_NSS_MASK_S           2
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_M     0x00000008
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_S     3
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_M 0x00000010
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_S 4
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_M         0x00000020
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_S         5
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_M            0x00000040
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_S            6
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_M          0x00000080
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_S          7
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_M        0x00000100
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_S        8
#define HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_M                  0x00000200
#define HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_S                  9

/* DWORD 1 */
#define HTT_TX_MSDU_EXT_DESC_PWR_M                           0x000000ff
#define HTT_TX_MSDU_EXT_DESC_PWR_S                           0
#define HTT_TX_MSDU_EXT_DESC_MCS_MASK_M                      0x000fff00
#define HTT_TX_MSDU_EXT_DESC_MCS_MASK_S                      8
#define HTT_TX_MSDU_EXT_DESC_NSS_MASK_M                      0x00f00000
#define HTT_TX_MSDU_EXT_DESC_NSS_MASK_S                      20
#define HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_M                0x01000000
#define HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_S                24
#define HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_M            0x1c000000
#define HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_S            25

/* DWORD 2 */
#define HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_M                    0x0000000f
#define HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_S                    0
#define HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_M                   0x000000f0
#define HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_S                   4
#define HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_M                0x00003f00
#define HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_S                8


/* DWORD 0 */
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PWR_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_MCS_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL( \
             HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL, _val); \
         ((_var) |= ((_val) \
             << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_GUARD_INTERVAL_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL( \
             HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK, _val); \
         ((_var) |= ((_val) \
             << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_PREAMBLE_TYPE_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_CHAIN_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_RETRIES_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_BANDWIDTH_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_VALID_EXPIRE_TIME_S));\
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_M) >> \
    HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_S)
#define HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_FLAG_IS_DSRC_S)); \
     } while (0)


/* DWORD 1 */
#define HTT_TX_MSDU_EXT_DESC_PWR_GET_BASE(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_PWR_M) >> \
    HTT_TX_MSDU_EXT_DESC_PWR_S)
#define HTT_TX_MSDU_EXT_DESC_PWR_GET(_var) \
    (HTT_TX_MSDU_EXT_DESC_PWR_GET_BASE(_var)  | \
    HTT_SIGN_BIT_EXTENSION_MASK(_var, HTT_TX_MSDU_EXT_DESC_PWR))
#define HTT_TX_MSDU_EXT_DESC_PWR_SET(_var, _val) \
    ((_var) |= (((_val) << HTT_TX_MSDU_EXT_DESC_PWR_S)) & \
    HTT_TX_MSDU_EXT_DESC_PWR_M)

#define HTT_TX_MSDU_EXT_DESC_MCS_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_MCS_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_MCS_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_MCS_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_MCS_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_MCS_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_NSS_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_NSS_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_NSS_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_NSS_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_NSS_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_NSS_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_M) >> \
    HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_S)
#define HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_GUARD_INTERVAL_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_PREAMBLE_TYPE_MASK_S)); \
     } while (0)


/* DWORD 2 */
#define HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_CHAIN_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_CHAIN_MASK_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_M) >> \
    HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_S)
#define HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_RETRY_LIMIT_S)); \
     } while (0)

#define HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_GET(_var) \
    (((_var) & HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_M) >> \
    HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_S)
#define HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_SET(_var, _val) \
     do { \
         HTT_CHECK_SET_VAL(HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK, _val); \
         ((_var) |= ((_val) << HTT_TX_MSDU_EXT_DESC_BANDWIDTH_MASK_S)); \
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
 * payload 1: |       FW_IDX shadow register physical address (bits 31:0)     |
#if HTT_PADDR64
 *            |       FW_IDX shadow register physical address (bits 63:32)    |
#endif
 *            |---------------------------------------------------------------|
 *            |                 rx ring base physical address (bits 31:0)     |
#if HTT_PADDR64
 *            |                 rx ring base physical address (bits 63:32)    |
#endif
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
 *     for systems using 64-bit format for bus addresses:
 *       - IDX_SHADOW_REG_PADDR_LO
 *         Bits 31:0
 *         Value: lower 4 bytes of physical address of the host's
 *                FW_IDX shadow register
 *       - IDX_SHADOW_REG_PADDR_HI
 *         Bits 31:0
 *         Value: upper 4 bytes of physical address of the host's
 *                FW_IDX shadow register
 *       - RING_BASE_PADDR_LO
 *         Bits 31:0
 *         Value: lower 4 bytes of physical address of the host's rx ring
 *       - RING_BASE_PADDR_HI
 *         Bits 31:0
 *         Value: uppper 4 bytes of physical address of the host's rx ring
 *     for systems using 32-bit format for bus addresses:
 *       - IDX_SHADOW_REG_PADDR
 *         Bits 31:0
 *         Value: physical address of the host's FW_IDX shadow register
 *       - RING_BASE_PADDR
 *         Bits 31:0
 *         Value: physical address of the host's rx ring
 *   - RING_LEN
 *     Bits 15:0
 *     Value: number of elements in the rx ring
 *   - RING_BUF_SZ
 *     Bits 31:16
 *     Value: size of the buffers referenced by the rx ring, in byte units
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
/* for systems using a 64-bit format for bus addresses */
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_HI_M 0xffffffff
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_HI_S 0
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_LO_M 0xffffffff
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_LO_S 0
#define HTT_RX_RING_CFG_BASE_PADDR_HI_M           0xffffffff
#define HTT_RX_RING_CFG_BASE_PADDR_HI_S           0
#define HTT_RX_RING_CFG_BASE_PADDR_LO_M           0xffffffff
#define HTT_RX_RING_CFG_BASE_PADDR_LO_S           0

/* for systems using a 32-bit format for bus addresses */
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_M    0xffffffff
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_S    0
#define HTT_RX_RING_CFG_BASE_PADDR_M              0xffffffff
#define HTT_RX_RING_CFG_BASE_PADDR_S              0

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
#define HTT_RX_RING_CFG_PAYLD_BYTES_64 44
#define HTT_RX_RING_CFG_PAYLD_BYTES_32 36
#if HTT_PADDR64
    #define HTT_RX_RING_CFG_PAYLD_BYTES HTT_RX_RING_CFG_PAYLD_BYTES_64
#else
    #define HTT_RX_RING_CFG_PAYLD_BYTES HTT_RX_RING_CFG_PAYLD_BYTES_32
#endif
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
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_HI_GET(_var) (_var)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_HI_SET(_var, _val) (_var) = (_val)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_LO_GET(_var) (_var)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_LO_SET(_var, _val) (_var) = (_val)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_GET(_var) (_var)
#define HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(_var, _val) (_var) = (_val)

/* degenerate case for 32-bit fields */
#define HTT_RX_RING_CFG_BASE_PADDR_HI_GET(_var) (_var)
#define HTT_RX_RING_CFG_BASE_PADDR_HI_SET(_var, _val) (_var) = (_val)
#define HTT_RX_RING_CFG_BASE_PADDR_LO_GET(_var) (_var)
#define HTT_RX_RING_CFG_BASE_PADDR_LO_SET(_var, _val) (_var) = (_val)
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
 *  TX CE ring is used for pushing packet metadata from IPA uC
 *  to WLAN FW
 *  TX Completion ring is used for generating TX completions from
 *  WLAN FW to IPA uC
 *  RX Indication ring is used for indicating RX packets from FW
 *  to IPA uC
 *  RX Ring2 is used as either completion ring or as second
 *  indication ring. when Ring2 is used as completion ring, IPA uC
 *  puts completed RX packet meta data to Ring2. when Ring2 is used
 *  as second indication ring, RX packets for LTE-WLAN aggregation are
 *  indicated in Ring2, other RX packets (e.g. hotspot related) are
 *  indicated in RX Indication ring. Please see WDI_IPA specification
 *  for more details.
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |        tx pkt pool size         |      Rsvd      |     msg_type   |
 *     |-------------------------------------------------------------------|
 *     |                 tx comp ring base (bits 31:0)                     |
#if HTT_PADDR64
 *     |                 tx comp ring base (bits 63:32)                    |
#endif
 *     |-------------------------------------------------------------------|
 *     |                         tx comp ring size                         |
 *     |-------------------------------------------------------------------|
 *     |            tx comp WR_IDX physical address (bits 31:0)            |
#if HTT_PADDR64
 *     |            tx comp WR_IDX physical address (bits 63:32)           |
#endif
 *     |-------------------------------------------------------------------|
 *     |            tx CE WR_IDX physical address (bits 31:0)              |
#if HTT_PADDR64
 *     |            tx CE WR_IDX physical address (bits 63:32)             |
#endif
 *     |-------------------------------------------------------------------|
 *     |             rx indication ring base (bits 31:0)                   |
#if HTT_PADDR64
 *     |             rx indication ring base (bits 63:32)                  |
#endif
 *     |-------------------------------------------------------------------|
 *     |                      rx indication ring size                      |
 *     |-------------------------------------------------------------------|
 *     |             rx ind RD_IDX physical address (bits 31:0)            |
#if HTT_PADDR64
 *     |             rx ind RD_IDX physical address (bits 63:32)           |
#endif
 *     |-------------------------------------------------------------------|
 *     |             rx ind WR_IDX physical address (bits 31:0)            |
#if HTT_PADDR64
 *     |             rx ind WR_IDX physical address (bits 63:32)           |
#endif
 *     |-------------------------------------------------------------------|
 *     |-------------------------------------------------------------------|
 *     |                    rx ring2 base (bits 31:0)                      |
#if HTT_PADDR64
 *     |                    rx ring2 base (bits 63:32)                     |
#endif
 *     |-------------------------------------------------------------------|
 *     |                        rx ring2 size                              |
 *     |-------------------------------------------------------------------|
 *     |             rx ring2 RD_IDX physical address (bits 31:0)          |
#if HTT_PADDR64
 *     |             rx ring2 RD_IDX physical address (bits 63:32)         |
#endif
 *     |-------------------------------------------------------------------|
 *     |             rx ring2 WR_IDX physical address (bits 31:0)          |
#if HTT_PADDR64
 *     |             rx ring2 WR_IDX physical address (bits 63:32)         |
#endif
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
 *   For systems using 32-bit format for bus addresses:
 *     - TX_COMP_RING_BASE_ADDR
 *       Bits 31:0
 *       Purpose: TX Completion Ring base address in DDR
 *     - TX_COMP_RING_SIZE
 *       Bits 31:0
 *       Purpose: TX Completion Ring size (must be power of 2)
 *     - TX_COMP_WR_IDX_ADDR
 *       Bits 31:0
 *       Purpose: IPA doorbell register address OR DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA TX completion ring
 *     - TX_CE_WR_IDX_ADDR
 *       Bits 31:0
 *       Purpose: DDR address where IPA uC
 *                updates the WR Index for TX CE ring
 *                (needed for fusion platforms)
 *     - RX_IND_RING_BASE_ADDR
 *       Bits 31:0
 *       Purpose: RX Indication Ring base address in DDR
 *     - RX_IND_RING_SIZE
 *       Bits 31:0
 *       Purpose: RX Indication Ring size
 *     - RX_IND_RD_IDX_ADDR
 *       Bits 31:0
 *       Purpose: DDR address where IPA uC updates the Read Index for WDI_IPA
 *                RX indication ring
 *     - RX_IND_WR_IDX_ADDR
 *       Bits 31:0
 *       Purpose: IPA doorbell register address OR DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA RX indication ring
 *     - RX_RING2_BASE_ADDR
 *       Bits 31:0
 *       Purpose: Second RX Ring(Indication or completion)base address in DDR
 *     - RX_RING2_SIZE
 *       Bits 31:0
 *       Purpose: Second RX  Ring size (must be >= RX_IND_RING_SIZE)
 *     - RX_RING2_RD_IDX_ADDR
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring, DDR address where
 *                IPA uC updates the Read Index for Ring2.
 *                If Second RX ring is completion ring, this is NOT used
 *     - RX_RING2_WR_IDX_ADDR
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring,  DDR address where
 *                WIFI FW updates the Write Index for WDI_IPA RX ring2
 *                If second RX ring is completion ring, DDR address where
 *                IPA uC updates the Write Index for Ring 2.
 *   For systems using 64-bit format for bus addresses:
 *     - TX_COMP_RING_BASE_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of TX Completion Ring base physical address in DDR
 *     - TX_COMP_RING_BASE_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of TX Completion Ring base physical address in DDR
 *     - TX_COMP_RING_SIZE
 *       Bits 31:0
 *       Purpose: TX Completion Ring size (must be power of 2)
 *     - TX_COMP_WR_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of IPA doorbell register address OR
 *                Lower 4 bytes of DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA TX completion ring
 *     - TX_COMP_WR_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of IPA doorbell register address OR
 *                Higher 4 bytes of DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA TX completion ring
 *     - TX_CE_WR_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of DDR address where IPA uC
 *                updates the WR Index for TX CE ring
 *                (needed for fusion platforms)
 *     - TX_CE_WR_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of DDR address where IPA uC
 *                updates the WR Index for TX CE ring
 *                (needed for fusion platforms)
 *     - RX_IND_RING_BASE_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of RX Indication Ring base address in DDR
 *     - RX_IND_RING_BASE_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of RX Indication Ring base address in DDR
 *     - RX_IND_RING_SIZE
 *       Bits 31:0
 *       Purpose: RX Indication Ring size
 *     - RX_IND_RD_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of DDR address where IPA uC updates the Read Index
 *                for WDI_IPA RX indication ring
 *     - RX_IND_RD_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of DDR address where IPA uC updates the Read Index
 *                for WDI_IPA RX indication ring
 *     - RX_IND_WR_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of IPA doorbell register address OR
 *                Lower 4 bytes of DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA RX indication ring
 *     - RX_IND_WR_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of IPA doorbell register address OR
 *                Higher 4 bytes of DDR address where WIFI FW
 *                updates the Write Index for WDI_IPA RX indication ring
 *     - RX_RING2_BASE_ADDR_LO
 *       Bits 31:0
 *       Purpose: Lower 4 bytes of Second RX Ring(Indication OR completion)base address in DDR
 *     - RX_RING2_BASE_ADDR_HI
 *       Bits 31:0
 *       Purpose: Higher 4 bytes of Second RX Ring(Indication OR completion)base address in DDR
 *     - RX_RING2_SIZE
 *       Bits 31:0
 *       Purpose: Second RX  Ring size (must be >= RX_IND_RING_SIZE)
 *     - RX_RING2_RD_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring, lower 4 bytes of
 *                DDR address where IPA uC updates the Read Index for Ring2.
 *                If Second RX ring is completion ring, this is NOT used
 *     - RX_RING2_RD_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring, higher 4 bytes of
 *                DDR address where IPA uC updates the Read Index for Ring2.
 *                If Second RX ring is completion ring, this is NOT used
 *     - RX_RING2_WR_IDX_ADDR_LO
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring, lower 4 bytes of
 *                DDR address where WIFI FW updates the Write Index
 *                for WDI_IPA RX ring2
 *                If second RX ring is completion ring, lower 4 bytes of
 *                DDR address where IPA uC updates the Write Index for Ring 2.
 *     - RX_RING2_WR_IDX_ADDR_HI
 *       Bits 31:0
 *       Purpose: If Second RX ring is Indication ring, higher 4 bytes of
 *                DDR address where WIFI FW updates the Write Index
 *                for WDI_IPA RX ring2
 *                If second RX ring is completion ring, higher 4 bytes of
 *                DDR address where IPA uC updates the Write Index for Ring 2.
 */

#if HTT_PADDR64
#define HTT_WDI_IPA_CFG_SZ                           88 /* bytes */
#else
#define HTT_WDI_IPA_CFG_SZ                           52 /* bytes */
#endif

#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_M           0xffff0000
#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S           16

#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_M     0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S     0

#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_M  0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_S  0

#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_M  0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_S  0

#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_M          0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S          0

#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_M        0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S        0

#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_M     0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_S     0

#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_M     0xffffffff
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_S     0

#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_M          0xffffffff
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S          0

#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_M       0xffffffff
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_S       0

#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_M       0xffffffff
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_S       0

#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S      0

#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_M   0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_S   0

#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_M   0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_S   0

#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_M           0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S           0

#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_M         0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S         0

#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_S      0

#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_S      0

#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_M         0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S         0

#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_S      0

#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_S      0

#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_M         0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_S         0

#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_S      0

#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_M      0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_S      0

#define HTT_WDI_IPA_CFG_RX_RING2_SIZE_M              0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_SIZE_S              0

#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_M       0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_S       0

#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_M    0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_S    0

#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_M    0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_S    0

#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_M       0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_S       0

#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_M    0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_S    0

#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_M    0xffffffff
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_S    0

#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_M) >> HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S)
#define HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_M) >> HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S)
#define HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_S)); \
    } while (0)


/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_M) >> HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S)
#define HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RING_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_M) >> HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_S)); \
    } while (0)

#define HTT_WDI_IPA_CFG_RX_RING2_SIZE_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_SIZE_M) >> HTT_WDI_IPA_CFG_RX_RING2_SIZE_S)
#define HTT_WDI_IPA_CFG_RX_RING2_SIZE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_SIZE, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_SIZE_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_S)); \
    } while (0)

/* for systems using 32-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_M) >> HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_S)
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_M) >> HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_S)
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_S)); \
    } while (0)

/* for systems using 64-bit format for bus addr */
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_GET(_var) \
    (((_var) & HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_M) >> HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_S)
#define HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO, _val);  \
        ((_var) |= ((_val) << HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_S)); \
    } while (0)

/*
 * TEMPLATE_HTT_WDI_IPA_CONFIG_T:
 * This macro defines a htt_wdi_ipa_configXXX_t in which any physical
 * addresses are stored in a XXX-bit field.
 * This macro is used to define both htt_wdi_ipa_config32_t and
 * htt_wdi_ipa_config64_t structs.
 */
#define TEMPLATE_HTT_WDI_IPA_CONFIG_T(_paddr_bits_, \
                                      _paddr__tx_comp_ring_base_addr_, \
                                      _paddr__tx_comp_wr_idx_addr_, \
                                      _paddr__tx_ce_wr_idx_addr_, \
                                      _paddr__rx_ind_ring_base_addr_, \
                                      _paddr__rx_ind_rd_idx_addr_, \
                                      _paddr__rx_ind_wr_idx_addr_, \
                                      _paddr__rx_ring2_base_addr_,\
                                      _paddr__rx_ring2_rd_idx_addr_,\
                                      _paddr__rx_ring2_wr_idx_addr_)      \
PREPACK struct htt_wdi_ipa_cfg ## _paddr_bits_ ## _t \
{ \
  /* DWORD 0: flags and meta-data */ \
    A_UINT32 \
        msg_type: 8, /* HTT_H2T_MSG_TYPE_WDI_IPA_CFG */ \
        reserved: 8, \
        tx_pkt_pool_size: 16;\
    /* DWORD 1  */\
    _paddr__tx_comp_ring_base_addr_;\
    /* DWORD 2 (or 3)*/\
    A_UINT32 tx_comp_ring_size;\
    /* DWORD 3 (or 4)*/\
    _paddr__tx_comp_wr_idx_addr_;\
    /* DWORD 4 (or 6)*/\
    _paddr__tx_ce_wr_idx_addr_;\
    /* DWORD 5 (or 8)*/\
    _paddr__rx_ind_ring_base_addr_;\
    /* DWORD 6 (or 10)*/\
    A_UINT32 rx_ind_ring_size;\
    /* DWORD 7 (or 11)*/\
    _paddr__rx_ind_rd_idx_addr_;\
    /* DWORD 8 (or 13)*/\
    _paddr__rx_ind_wr_idx_addr_;\
    /* DWORD 9 (or 15)*/\
    _paddr__rx_ring2_base_addr_;\
    /* DWORD 10 (or 17) */\
    A_UINT32 rx_ring2_size;\
    /* DWORD 11 (or 18) */\
    _paddr__rx_ring2_rd_idx_addr_;\
    /* DWORD 12 (or 20) */\
    _paddr__rx_ring2_wr_idx_addr_;\
} POSTPACK

/* define a htt_wdi_ipa_config32_t type */
TEMPLATE_HTT_WDI_IPA_CONFIG_T(32, HTT_VAR_PADDR32(tx_comp_ring_base_addr), HTT_VAR_PADDR32(tx_comp_wr_idx_addr), HTT_VAR_PADDR32(tx_ce_wr_idx_addr), HTT_VAR_PADDR32(rx_ind_ring_base_addr), HTT_VAR_PADDR32(rx_ind_rd_idx_addr),HTT_VAR_PADDR32(rx_ind_wr_idx_addr), HTT_VAR_PADDR32(rx_ring2_base_addr), HTT_VAR_PADDR32(rx_ring2_rd_idx_addr), HTT_VAR_PADDR32(rx_ring2_wr_idx_addr));

/* define a htt_wdi_ipa_config64_t type */
TEMPLATE_HTT_WDI_IPA_CONFIG_T(64, HTT_VAR_PADDR64_LE(tx_comp_ring_base_addr), HTT_VAR_PADDR64_LE(tx_comp_wr_idx_addr), HTT_VAR_PADDR64_LE(tx_ce_wr_idx_addr), HTT_VAR_PADDR64_LE(rx_ind_ring_base_addr), HTT_VAR_PADDR64_LE(rx_ind_rd_idx_addr), HTT_VAR_PADDR64_LE(rx_ind_wr_idx_addr), HTT_VAR_PADDR64_LE(rx_ring2_base_addr), HTT_VAR_PADDR64_LE(rx_ring2_rd_idx_addr), HTT_VAR_PADDR64_LE(rx_ring2_wr_idx_addr));

#if HTT_PADDR64
    #define htt_wdi_ipa_cfg_t htt_wdi_ipa_cfg64_t
#else
    #define htt_wdi_ipa_cfg_t htt_wdi_ipa_cfg32_t
#endif

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
    DEPRECATED_HTT_T2H_MSG_TYPE_RC_UPDATE_IND = 0xc, /* no longer used */
    HTT_T2H_MSG_TYPE_TX_INSPECT_IND           = 0xd,
    HTT_T2H_MSG_TYPE_MGMT_TX_COMPL_IND        = 0xe,
    /* only used for HL, add HTT MSG for HTT CREDIT update */
    HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND     = 0xf,
    HTT_T2H_MSG_TYPE_RX_PN_IND                = 0x10,
    HTT_T2H_MSG_TYPE_RX_OFFLOAD_DELIVER_IND   = 0x11,
    HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND      = 0x12,
    /* 0x13 is reserved for RX_RING_LOW_IND (RX Full reordering related) */
    HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE      = 0x14,
    HTT_T2H_MSG_TYPE_CHAN_CHANGE              = 0x15,
    HTT_T2H_MSG_TYPE_RX_OFLD_PKT_ERR          = 0x16,
    HTT_T2H_MSG_TYPE_RATE_REPORT              = 0x17,
    HTT_T2H_MSG_TYPE_FLOW_POOL_MAP            = 0x18,
    HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP          = 0x19,

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
 *     :                    option request TLV (optional)                  |
 *     :...................................................................:
 *
 * The VER_CONF message may consist of a single 4-byte word, or may be
 * extended with TLVs that specify HTT options selected by the target.
 * The following option TLVs may be appended to the VER_CONF message:
 *   - LL_BUS_ADDR_SIZE
 *   - HL_SUPPRESS_TX_COMPL_IND
 *   - MAX_TX_QUEUE_GROUPS
 * These TLVs may appear in an arbitrary order.  Any number of these TLVs
 * may be appended to the VER_CONF message (but only one TLV of each type).
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
 * |                  peer ID           |  | F| O| ext TID    |   msg type    |
 * |--------------------------------------------------------------------------|
 * |                  MSDU count        |        Reserved     |   vdev id     |
 * |--------------------------------------------------------------------------|
 * |                        MSDU 0 bus address (bits 31:0)                    |
#if HTT_PADDR64
 * |                        MSDU 0 bus address (bits 63:32)                   |
#endif
 * |--------------------------------------------------------------------------|
 * |    MSDU info   | MSDU 0 FW Desc    |         MSDU 0 Length               |
 * |--------------------------------------------------------------------------|
 * |                        MSDU 1 bus address (bits 31:0)                    |
#if HTT_PADDR64
 * |                        MSDU 1 bus address (bits 63:32)                   |
#endif
 * |--------------------------------------------------------------------------|
 * |    MSDU info   | MSDU 1 FW Desc    |         MSDU 1 Length               |
 * |--------------------------------------------------------------------------|
 */


/** @brief - MSDU info byte for TCP_CHECKSUM_OFFLOAD use
 *
 * @details
 *                            bits
 * |  7  | 6  |   5   |    4   |   3    |    2    |    1    |     0     |
 * |-----+----+-------+--------+--------+---------+---------+-----------|
 * | reserved | is IP | is UDP | is TCP | is IPv6 |IP chksum|  TCP/UDP  |
 * |          | frag  |        |        |         | fail    |chksum fail|
 * |-----+----+-------+--------+--------+---------+---------+-----------|
 * (see fw_rx_msdu_info def in wal_rx_desc.h)
 */

struct htt_rx_in_ord_paddr_ind_hdr_t
{
    A_UINT32 /* word 0 */
        msg_type:   8,
        ext_tid:    5,
        offload:    1,
        frag:       1,
        reserved_0: 1,
        peer_id:    16;

    A_UINT32 /* word 1 */
        vap_id:     8,
        reserved_1: 8,
        msdu_cnt:   16;
};

struct htt_rx_in_ord_paddr_ind_msdu32_t
{
    A_UINT32 dma_addr;
    A_UINT32
        length: 16,
        fw_desc: 8,
        msdu_info:8;
};
struct htt_rx_in_ord_paddr_ind_msdu64_t
{
    A_UINT32 dma_addr_lo;
    A_UINT32 dma_addr_hi;
    A_UINT32
        length: 16,
        fw_desc: 8,
        msdu_info:8;
};
#if HTT_PADDR64
    #define htt_rx_in_ord_paddr_ind_msdu_t htt_rx_in_ord_paddr_ind_msdu64_t
#else
    #define htt_rx_in_ord_paddr_ind_msdu_t htt_rx_in_ord_paddr_ind_msdu32_t
#endif


#define HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES (sizeof(struct htt_rx_in_ord_paddr_ind_hdr_t))
#define HTT_RX_IN_ORD_PADDR_IND_HDR_DWORDS (HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES >> 2)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTE_OFFSET  HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORD_OFFSET HTT_RX_IN_ORD_PADDR_IND_HDR_DWORDS
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES_64 (sizeof(struct htt_rx_in_ord_paddr_ind_msdu64_t))
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS_64 (HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES_64 >> 2)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES_32 (sizeof(struct htt_rx_in_ord_paddr_ind_msdu32_t))
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS_32 (HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES_32 >> 2)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES (sizeof(struct htt_rx_in_ord_paddr_ind_msdu_t))
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS (HTT_RX_IN_ORD_PADDR_IND_MSDU_BYTES >> 2)

#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_M      0x00001f00
#define HTT_RX_IN_ORD_PADDR_IND_EXT_TID_S      8
#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_M      0x00002000
#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_S      13
#define HTT_RX_IN_ORD_PADDR_IND_FRAG_M         0x00004000
#define HTT_RX_IN_ORD_PADDR_IND_FRAG_S         14
#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_M      0xffff0000
#define HTT_RX_IN_ORD_PADDR_IND_PEER_ID_S      16
#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_M       0x000000ff
#define HTT_RX_IN_ORD_PADDR_IND_VAP_ID_S       0
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_M     0xffff0000
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_S     16
/* for systems using 64-bit format for bus addresses */
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_M     0xffffffff
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_S     0
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_M     0xffffffff
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_S     0
/* for systems using 32-bit format for bus addresses */
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_M        0xffffffff
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_S        0
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_M     0x0000ffff
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S     0
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_M      0x00ff0000
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S      16
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_M    0xff000000
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_S    24


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

/* for systems using 64-bit format for bus addresses */
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_SET(word, value)                     \
    do {                                                                      \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_PADDR_HI, value);           \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_S;             \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_M) >> HTT_RX_IN_ORD_PADDR_IND_PADDR_HI_S)
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_SET(word, value)                     \
        do {                                                                  \
            HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_PADDR_LO, value);       \
            (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_S;         \
        } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_GET(word) \
        (((word) & HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_M) >> HTT_RX_IN_ORD_PADDR_IND_PADDR_LO_S)

/* for systems using 32-bit format for bus addresses */
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_SET(word, value)                        \
    do {                                                                      \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_PADDR, value);              \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_PADDR_S;                \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_PADDR_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_PADDR_M) >> HTT_RX_IN_ORD_PADDR_IND_PADDR_S)

#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_SET(word, value)                              \
    do {                                                                         \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_M) >> HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_S)

#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_SET(word, value)                              \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_FW_DESC, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_FW_DESC_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_FW_DESC_M) >> HTT_RX_IN_ORD_PADDR_IND_FW_DESC_S)

#define HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_SET(word, value)                              \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_M) >> HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_S)

#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_IND_OFFLOAD, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_IND_OFFLOAD_S;                      \
    } while (0)
#define HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_GET(word) \
    (((word) & HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_M) >> HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_S)

#define HTT_RX_IN_ORD_PADDR_IND_FRAG_SET(word, value)                              \
    do {                                                                        \
        HTT_CHECK_SET_VAL(HTT_RX_IN_ORD_IND_FRAG, value);                    \
        (word) |= (value)  << HTT_RX_IN_ORD_IND_FRAG_S;                      \
    } while (0)
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
        chan_info_present:1,
        resv0:2,
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


/*
 * Channel information can optionally be appended after hl_htt_rx_desc_base.
 * If so, the len field in htt_rx_ind_hl_rx_desc_t will be updated accordingly,
 * and the chan_info_present flag in hl_htt_rx_desc_base will be set.
 * Please see htt_chan_change_t for description of the fields.
 */
PREPACK struct htt_chan_info_t
{
    A_UINT32    primary_chan_center_freq_mhz: 16,
                contig_chan1_center_freq_mhz: 16;
    A_UINT32    contig_chan2_center_freq_mhz: 16,
                phy_mode: 8,
                reserved: 8;
} POSTPACK;

#define HTT_CHAN_INFO_SIZE      sizeof(struct htt_chan_info_t)

#define HL_RX_DESC_SIZE         (sizeof(struct hl_htt_rx_desc_base))
#define HL_RX_DESC_SIZE_DWORD   (HL_RX_STD_DESC_SIZE >> 2)

#define HTT_HL_RX_DESC_MPDU_SEQ_NUM_M       0xfff
#define HTT_HL_RX_DESC_MPDU_SEQ_NUM_S       0
#define HTT_HL_RX_DESC_MPDU_ENC_M           0x1000
#define HTT_HL_RX_DESC_MPDU_ENC_S           12
#define HTT_HL_RX_DESC_CHAN_INFO_PRESENT_M  0x2000
#define HTT_HL_RX_DESC_CHAN_INFO_PRESENT_S  13
#define HTT_HL_RX_DESC_MCAST_BCAST_M        0x10000
#define HTT_HL_RX_DESC_MCAST_BCAST_S        16
#define HTT_HL_RX_DESC_FRAGMENT_M           0x20000
#define HTT_HL_RX_DESC_FRAGMENT_S           17
#define HTT_HL_RX_DESC_KEY_ID_OCT_M         0x3fc0000
#define HTT_HL_RX_DESC_KEY_ID_OCT_S         18

#define HTT_HL_RX_DESC_PN_OFFSET            offsetof(struct hl_htt_rx_desc_base, pn_31_0)
#define HTT_HL_RX_DESC_PN_WORD_OFFSET       (HTT_HL_RX_DESC_PN_OFFSET >> 2)


/* Channel information */
#define HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_M   0x0000ffff
#define HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_S   0
#define HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_M   0xffff0000
#define HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_S   16
#define HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_M   0x0000ffff
#define HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_S   0
#define HTT_CHAN_INFO_PHY_MODE_M                   0x00ff0000
#define HTT_CHAN_INFO_PHY_MODE_S                   16


#define HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_SET(word, value)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ, value);  \
        (word) |= (value)  << HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_S;    \
    } while (0)
#define HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_GET(word)                   \
    (((word) & HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_M) >> HTT_CHAN_INFO_PRIMARY_CHAN_CENTER_FREQ_S)


#define HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_SET(word, value)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ, value);  \
        (word) |= (value)  << HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_S;    \
    } while (0)
#define HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_GET(word)                   \
    (((word) & HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_M) >> HTT_CHAN_INFO_CONTIG_CHAN1_CENTER_FREQ_S)


#define HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_SET(word, value)            \
    do {                                                                \
        HTT_CHECK_SET_VAL(HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ, value);  \
        (word) |= (value)  << HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_S;    \
    } while (0)
#define HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_GET(word)                   \
    (((word) & HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_M) >> HTT_CHAN_INFO_CONTIG_CHAN2_CENTER_FREQ_S)


#define HTT_CHAN_INFO_PHY_MODE_SET(word, value)            \
    do {                                                \
        HTT_CHECK_SET_VAL(HTT_CHAN_INFO_PHY_MODE, value);  \
        (word) |= (value)  << HTT_CHAN_INFO_PHY_MODE_S;    \
    } while (0)
#define HTT_CHAN_INFO_PHY_MODE_GET(word)                   \
    (((word) & HTT_CHAN_INFO_PHY_MODE_M) >> HTT_CHAN_INFO_PHY_MODE_S)


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
 * @brief target -> host rx peer map/unmap message definition
 *
 * @details
 * The following diagram shows the format of the rx peer map message sent
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
 * The following diagram shows the format of the rx peer unmap message sent
 * from the target to the host.
 *
 * |31             24|23             16|15              8|7               0|
 * |-----------------------------------------------------------------------|
 * |              peer ID              |     VDEV ID     |     msg type    |
 * |-----------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the rx peer map
 * and peer unmap messages sent from the target to the host.
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx peer map or peer unmap message
 *     Value: peer map -> 0x3, peer unmap -> 0x4
 *   - VDEV_ID
 *     Bits 15:8
 *     Purpose: Indicates which virtual device the peer is associated
 *         with.
 *     Value: vdev ID (used in the host to look up the vdev object)
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: The peer ID (index) that WAL is allocating (map) or
 *         freeing (unmap)
 *     Value: (rx) peer ID
 *   - MAC_ADDR_L32 (peer map only)
 *     Bits 31:0
 *     Purpose: Identifies which peer node the peer ID is for.
 *     Value: lower 4 bytes of peer node's MAC address
 *   - MAC_ADDR_U16 (peer map only)
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
 * @brief tx queue group information element definition
 *
 * @details
 * The following diagram shows the format of the tx queue group
 * information element, which can be included in target --> host
 * messages to specify the number of tx "credits" (tx descriptors
 * for LL, or tx buffers for HL) available to a particular group
 * of host-side tx queues, and which host-side tx queues belong to
 * the group.
 *
 * |31|30          24|23             16|15|14|13                           0|
 * |------------------------------------------------------------------------|
 * | X|   reserved   | tx queue grp ID | A| S|     credit count             |
 * |------------------------------------------------------------------------|
 * |            vdev ID mask           |               AC mask              |
 * |------------------------------------------------------------------------|
 *
 * The following definitions describe the fields within the tx queue group
 * information element:
 * - credit_count
 *   Bits 13:1
 *   Purpose: specify how many tx credits are available to the tx queue group
 *   Value: An absolute or relative, positive or negative credit value
 *       The 'A' bit specifies whether the value is absolute or relative.
 *       The 'S' bit specifies whether the value is positive or negative.
 *       A negative value can only be relative, not absolute.
 *       An absolute value replaces any prior credit value the host has for
 *       the tx queue group in question.
 *       A relative value is added to the prior credit value the host has for
 *       the tx queue group in question.
 * - sign
 *   Bit 14
 *   Purpose: specify whether the credit count is positive or negative
 *   Value: 0 -> positive, 1 -> negative
 * - absolute
 *   Bit 15
 *   Purpose: specify whether the credit count is absolute or relative
 *   Value: 0 -> relative, 1 -> absolute
 * - txq_group_id
 *   Bits 23:16
 *   Purpose: indicate which tx queue group's credit and/or membership are
 *       being specified
 *   Value: 0 to max_tx_queue_groups-1
 * - reserved
 *   Bits 30:16
 *   Value: 0x0
 * - eXtension
 *   Bit 31
 *   Purpose: specify whether another tx queue group info element follows
 *   Value: 0 -> no more tx queue group information elements
 *          1 -> another tx queue group information element immediately follows
 * - ac_mask
 *   Bits 15:0
 *   Purpose: specify which Access Categories belong to the tx queue group
 *   Value: bit-OR of masks for the ACs (WMM and extension) that belong to
 *       the tx queue group.
 *       The AC bit-mask values are obtained by left-shifting by the
 *       corresponding HTT_AC_WMM enum values, e.g. (1 << HTT_AC_WMM_BE) == 0x1
 * - vdev_id_mask
 *   Bits 31:16
 *   Purpose: specify which vdev's tx queues belong to the tx queue group
 *   Value: bit-OR of masks based on the IDs of the vdevs whose tx queues
 *       belong to the tx queue group.
 *       For example, if vdev IDs 1 and 4 belong to a tx queue group, the
 *       vdev_id_mask would be (1 << 1) | (1 << 4) = 0x12
 */
PREPACK struct htt_txq_group {
    A_UINT32
        credit_count:      14,
        sign:               1,
        absolute:           1,
        tx_queue_group_id:  8,
        reserved0:          7,
        extension:          1;
    A_UINT32
        ac_mask:           16,
        vdev_id_mask:      16;
} POSTPACK;

/* first word */
#define HTT_TXQ_GROUP_CREDIT_COUNT_S 0
#define HTT_TXQ_GROUP_CREDIT_COUNT_M 0x00003fff
#define HTT_TXQ_GROUP_SIGN_S         14
#define HTT_TXQ_GROUP_SIGN_M         0x00004000
#define HTT_TXQ_GROUP_ABS_S          15
#define HTT_TXQ_GROUP_ABS_M          0x00008000
#define HTT_TXQ_GROUP_ID_S           16
#define HTT_TXQ_GROUP_ID_M           0x00ff0000
#define HTT_TXQ_GROUP_EXT_S          31
#define HTT_TXQ_GROUP_EXT_M          0x80000000
/* second word */
#define HTT_TXQ_GROUP_AC_MASK_S      0
#define HTT_TXQ_GROUP_AC_MASK_M      0x0000ffff
#define HTT_TXQ_GROUP_VDEV_ID_MASK_S 16
#define HTT_TXQ_GROUP_VDEV_ID_MASK_M 0xffff0000

#define HTT_TXQ_GROUP_CREDIT_COUNT_SET(_info, _val)            \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_CREDIT_COUNT, _val);   \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_CREDIT_COUNT_S)); \
    } while (0)
#define HTT_TXQ_GROUP_CREDIT_COUNT_GET(_info)                  \
    (((_info) & HTT_TXQ_GROUP_CREDIT_COUNT_M) >> HTT_TXQ_GROUP_CREDIT_COUNT_S)

#define HTT_TXQ_GROUP_SIGN_SET(_info, _val)                    \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_SIGN, _val);           \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_SIGN_S));         \
    } while (0)
#define HTT_TXQ_GROUP_SIGN_GET(_info)                          \
    (((_info) & HTT_TXQ_GROUP_SIGN_M) >> HTT_TXQ_GROUP_SIGN_S)

#define HTT_TXQ_GROUP_ABS_SET(_info, _val)                     \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_ABS, _val);            \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_ABS_S));          \
    } while (0)
#define HTT_TXQ_GROUP_ABS_GET(_info)                           \
    (((_info) & HTT_TXQ_GROUP_ABS_M) >> HTT_TXQ_GROUP_ABS_S)

#define HTT_TXQ_GROUP_ID_SET(_info, _val)                      \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_ID, _val);             \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_ID_S));           \
    } while (0)
#define HTT_TXQ_GROUP_ID_GET(_info)                            \
    (((_info) & HTT_TXQ_GROUP_ID_M) >> HTT_TXQ_GROUP_ID_S)

#define HTT_TXQ_GROUP_EXT_SET(_info, _val)                     \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_EXT, _val);            \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_EXT_S));          \
    } while (0)
#define HTT_TXQ_GROUP_EXT_GET(_info)                           \
    (((_info) & HTT_TXQ_GROUP_EXT_M) >> HTT_TXQ_GROUP_EXT_S)

#define HTT_TXQ_GROUP_AC_MASK_SET(_info, _val)                 \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_AC_MASK, _val);        \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_AC_MASK_S));      \
    } while (0)
#define HTT_TXQ_GROUP_AC_MASK_GET(_info)                       \
    (((_info) & HTT_TXQ_GROUP_AC_MASK_M) >> HTT_TXQ_GROUP_AC_MASK_S)

#define HTT_TXQ_GROUP_VDEV_ID_MASK_SET(_info, _val)            \
    do {                                                       \
        HTT_CHECK_SET_VAL(HTT_TXQ_GROUP_VDEV_ID_MASK, _val);   \
        ((_info) |= ((_val) << HTT_TXQ_GROUP_VDEV_ID_MASK_S)); \
    } while (0)
#define HTT_TXQ_GROUP_VDEV_ID_MASK_GET(_info)                  \
    (((_info) & HTT_TXQ_GROUP_VDEV_ID_MASK_M) >> HTT_TXQ_GROUP_VDEV_ID_MASK_S)

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
/*
 * The PEER_DEL tx completion status is used for HL cases
 * where the peer the frame is for has been deleted.
 * The host has already discarded its copy of the frame, but
 * it still needs the tx completion to restore its credit.
 */
#define HTT_TX_COMPL_IND_STAT_PEER_DEL    4


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
 * @brief target -> host rate-control update indication message
 *
 * @details
 * The following diagram shows the format of the RC Update message
 * sent from the target to the host, while processing the tx-completion
 * of a transmitted PPDU.
 *
 *          |31          24|23           16|15            8|7            0|
 *          |-------------------------------------------------------------|
 *          |            peer ID           |    vdev ID    |    msg_type  |
 *          |-------------------------------------------------------------|
 *          |  MAC addr 3  |  MAC addr 2   |   MAC addr 1  |  MAC addr 0  |
 *          |-------------------------------------------------------------|
 *          |   reserved   |   num elems   |   MAC addr 5  |  MAC addr 4  |
 *          |-------------------------------------------------------------|
 *          |                              :                              |
 *          :         HTT_RC_TX_DONE_PARAMS (DWORD-aligned)               :
 *          |                              :                              |
 *          |-------------------------------------------------------------|
 *          |                              :                              |
 *          :         HTT_RC_TX_DONE_PARAMS (DWORD-aligned)               :
 *          |                              :                              |
 *          |-------------------------------------------------------------|
 *          :                                                             :
 *          - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 */

typedef struct {
    A_UINT32 rate_code; /* rate code, bw, chain mask sgi */
    A_UINT32 rate_code_flags;
    A_UINT32 flags;       /* Encodes information such as excessive
                                                  retransmission, aggregate, some info
                                                  from .11 frame control,
                                                  STBC, LDPC, (SGI and Tx Chain Mask
                                                  are encoded in ptx_rc->flags field),
                                                  AMPDU truncation (BT/time based etc.),
                                                  RTS/CTS attempt  */

    A_UINT32 num_enqued;  /* # of MPDUs (for non-AMPDU 1) for this rate */
    A_UINT32 num_retries; /* Total # of transmission attempt for this rate */
    A_UINT32 num_failed;  /* # of failed MPDUs in A-MPDU, 0 otherwise */
    A_UINT32 ack_rssi;    /* ACK RSSI: b'7..b'0 avg RSSI across all chain */
    A_UINT32 time_stamp ; /* ACK timestamp (helps determine age) */
    A_UINT32 is_probe;   /* Valid if probing. Else, 0 */
} HTT_RC_TX_DONE_PARAMS;

#define HTT_RC_UPDATE_CTXT_SZ     (sizeof(HTT_RC_TX_DONE_PARAMS)) /* bytes */
#define HTT_RC_UPDATE_HDR_SZ      (12) /* bytes */

#define HTT_RC_UPDATE_MAC_ADDR_OFFSET   (4) /* bytes */
#define HTT_RC_UPDATE_MAC_ADDR_LENGTH   IEEE80211_ADDR_LEN /* bytes */

#define HTT_RC_UPDATE_VDEVID_S    8
#define HTT_RC_UPDATE_VDEVID_M    0xff00
#define HTT_RC_UPDATE_PEERID_S    16
#define HTT_RC_UPDATE_PEERID_M    0xffff0000

#define HTT_RC_UPDATE_NUM_ELEMS_S   16
#define HTT_RC_UPDATE_NUM_ELEMS_M   0x00ff0000

#define HTT_RC_UPDATE_VDEVID_SET(_info, _val)              \
    do {                                                   \
        HTT_CHECK_SET_VAL(HTT_RC_UPDATE_VDEVID, _val);     \
        ((_info) |= ((_val) << HTT_RC_UPDATE_VDEVID_S));   \
    } while (0)

#define HTT_RC_UPDATE_VDEVID_GET(_info)                    \
    (((_info) & HTT_RC_UPDATE_VDEVID_M) >> HTT_RC_UPDATE_VDEVID_S)

#define HTT_RC_UPDATE_PEERID_SET(_info, _val)              \
    do {                                                   \
        HTT_CHECK_SET_VAL(HTT_RC_UPDATE_PEERID, _val);     \
        ((_info) |= ((_val) << HTT_RC_UPDATE_PEERID_S));   \
    } while (0)

#define HTT_RC_UPDATE_PEERID_GET(_info)                    \
    (((_info) & HTT_RC_UPDATE_PEERID_M) >> HTT_RC_UPDATE_PEERID_S)

#define HTT_RC_UPDATE_NUM_ELEMS_SET(_info, _val)            \
    do {                                                    \
        HTT_CHECK_SET_VAL(HTT_RC_UPDATE_NUM_ELEMS, _val);   \
        ((_info) |= ((_val) << HTT_RC_UPDATE_NUM_ELEMS_S)); \
    } while (0)

#define HTT_RC_UPDATE_NUM_ELEMS_GET(_info)                  \
    (((_info) & HTT_RC_UPDATE_NUM_ELEMS_M) >> HTT_RC_UPDATE_NUM_ELEMS_S)

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
    /* Unicast-data MPDUs dropped due to invalid peer */
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
    /* Flush due to delete peer */
    A_UINT32 deliver_flush_delpeer;
    /* Flush due to offload*/
    A_UINT32 deliver_flush_offload;
    /* Flush due to out of buffer*/
    A_UINT32 deliver_flush_oob;
    /* MPDUs dropped due to PN check fail */
    A_UINT32 pn_fail;
    /* MPDUs dropped due to unable to allocate memory  */
    A_UINT32 store_fail;
    /* Number of times the tid pool alloc succeeded */
    A_UINT32 tid_pool_alloc_succ;
    /* Number of times the MPDU pool alloc succeeded */
    A_UINT32 mpdu_pool_alloc_succ;
    /* Number of times the MSDU pool alloc succeeded */
    A_UINT32 msdu_pool_alloc_succ;
    /* Number of times the tid pool alloc failed */
    A_UINT32 tid_pool_alloc_fail;
    /* Number of times the MPDU pool alloc failed */
    A_UINT32 mpdu_pool_alloc_fail;
    /* Number of times the MSDU pool alloc failed */
    A_UINT32 msdu_pool_alloc_fail;
    /* Number of times the tid pool freed */
    A_UINT32 tid_pool_free;
    /* Number of times the MPDU pool freed */
    A_UINT32 mpdu_pool_free;
    /* Number of times the MSDU pool freed */
    A_UINT32 msdu_pool_free;
    /* number of MSDUs undelivered to HTT and queued to Data Rx MSDU free list*/
    A_UINT32 msdu_queued;
    /* Number of MSDUs released from Data Rx MSDU list to MAC ring */
    A_UINT32 msdu_recycled;
    /* Number of MPDUs with invalid peer but A2 found in AST */
    A_UINT32 invalid_peer_a2_in_ast;
    /* Number of MPDUs with invalid peer but A3 found in AST */
    A_UINT32 invalid_peer_a3_in_ast;
    /* Number of MPDUs with invalid peer, Broadcast or Multicast frame */
    A_UINT32 invalid_peer_bmc_mpdus;
    /* Number of MSDUs with err attention word */
    A_UINT32 rxdesc_err_att;
    /* Number of MSDUs with flag of peer_idx_invalid */
    A_UINT32 rxdesc_err_peer_idx_inv;
    /* Number of MSDUs with flag of peer_idx_timeout */
    A_UINT32 rxdesc_err_peer_idx_to;
    /* Number of MSDUs with flag of overflow */
    A_UINT32 rxdesc_err_ov;
    /* Number of MSDUs with flag of msdu_length_err */
    A_UINT32 rxdesc_err_msdu_len;
    /* Number of MSDUs with flag of mpdu_length_err */
    A_UINT32 rxdesc_err_mpdu_len;
    /* Number of MSDUs with flag of tkip_mic_err */
    A_UINT32 rxdesc_err_tkip_mic;
    /* Number of MSDUs with flag of decrypt_err */
    A_UINT32 rxdesc_err_decrypt;
    /* Number of MSDUs with flag of fcs_err */
    A_UINT32 rxdesc_err_fcs;
    /* Number of Unicast (bc_mc bit is not set in attention word)
     * frames with invalid peer handler
     */
    A_UINT32 rxdesc_uc_msdus_inv_peer;
    /* Number of unicast frame directly (direct bit is set in attention word)
     * to DUT with invalid peer handler
     */
    A_UINT32 rxdesc_direct_msdus_inv_peer;
    /* Number of Broadcast/Multicast (bc_mc bit set in attention word)
     * frames with invalid peer handler
     */
    A_UINT32 rxdesc_bmc_msdus_inv_peer;
    /* Number of MSDUs dropped due to no first MSDU flag */
    A_UINT32 rxdesc_no_1st_msdu;
    /* Number of MSDUs droped due to ring overflow */
    A_UINT32 msdu_drop_ring_ov;
    /* Number of MSDUs dropped due to FC mismatch */
    A_UINT32 msdu_drop_fc_mismatch;
    /* Number of MSDUs dropped due to mgt frame in Remote ring */
    A_UINT32 msdu_drop_mgmt_remote_ring;
    /* Number of MSDUs dropped due to errors not reported in attention word */
    A_UINT32 msdu_drop_misc;
    /* Number of MSDUs go to offload before reorder */
    A_UINT32 offload_msdu_wal;
    /* Number of data frame dropped by offload after reorder */
    A_UINT32 offload_msdu_reorder;
    /* Number of MPDUs with sequence number in the past and within
       the BA window */
    A_UINT32 dup_past_within_window;
    /* Number of MPDUs with sequence number in the past and
     * outside the BA window */
    A_UINT32 dup_past_outside_window;
    /* Number of MSDUs with decrypt/MIC error */
    A_UINT32 rxdesc_err_decrypt_mic;
    /* Number of data MSDUs received on both local and remote rings */
    A_UINT32 data_msdus_on_both_rings;
};


/*
 * Rx Remote buffer statistics
 * NB: all the fields must be defined in 4 octets size.
 */
struct rx_remote_buffer_mgmt_stats {
    /* Total number of MSDUs reaped for Rx processing */
    A_UINT32 remote_reaped;
    /* MSDUs recycled within firmware */
    A_UINT32 remote_recycled;
    /* MSDUs stored by Data Rx */
    A_UINT32 data_rx_msdus_stored;
    /* Number of HTT indications from WAL Rx MSDU */
    A_UINT32 wal_rx_ind;
    /* Number of unconsumed HTT indications from WAL Rx MSDU */
    A_UINT32 wal_rx_ind_unconsumed;
    /* Number of HTT indications from Data Rx MSDU */
    A_UINT32 data_rx_ind;
    /* Number of unconsumed HTT indications from Data Rx MSDU */
    A_UINT32 data_rx_ind_unconsumed;
    /* Number of HTT indications from ATHBUF */
    A_UINT32 athbuf_rx_ind;
    /* Number of remote buffers requested for refill */
    A_UINT32 refill_buf_req;
    /* Number of remote buffers filled by the host */
    A_UINT32 refill_buf_rsp;
    /* Number of times MAC hw_index = f/w write_index */
    A_INT32 mac_no_bufs;
    /* Number of times f/w write_index = f/w read_index for MAC Rx ring */
    A_INT32 fw_indices_equal;
    /* Number of times f/w finds no buffers to post */
    A_INT32 host_no_bufs;
};

/*
 * TXBF MU/SU packets and NDPA statistics
 * NB: all the fields must be defined in 4 octets size.
 */
struct rx_txbf_musu_ndpa_pkts_stats {
    /* number of TXBF MU packets received */
    A_UINT32 number_mu_pkts;
    /* number of TXBF SU packets received */
    A_UINT32 number_su_pkts;
    /* number of TXBF directed NDPA */
    A_UINT32 txbf_directed_ndpa_count;
    /* number of TXBF retried NDPA */
    A_UINT32 txbf_ndpa_retry_count;
    /* total number of TXBF NDPA */
    A_UINT32 txbf_total_ndpa_count;
    /* must be set to 0x0 */
    A_UINT32 reserved[3];
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
#define HTT_T2H_STATS_COOKIE_SIZE         8

#define HTT_T2H_STATS_CONF_TAIL_SIZE      4

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
 * |                 BANK0_BASE_ADDRESS (bits 31:0)             |
#if HTT_PADDR64
 * |                 BANK0_BASE_ADDRESS (bits 63:32)            |
#endif
 * |------------------------------------------------------------|
 * |                            ...                             |
 * |------------------------------------------------------------|
 * |                 BANK15_BASE_ADDRESS (bits 31:0)            |
#if HTT_PADDR64
 * |                 BANK15_BASE_ADDRESS (bits 63:32)           |
#endif
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
 *  for systems with 64-bit format for bus addresses:
 *      - BANKx_BASE_ADDRESS_LO
 *        Bits 31:0
 *        Purpose: Provide a mechanism to specify the base address of the
 *             MSDU_EXT bank physical/bus address.
 *        Value: lower 4 bytes of MSDU_EXT bank physical / bus address
 *      - BANKx_BASE_ADDRESS_HI
 *        Bits 31:0
 *        Purpose: Provide a mechanism to specify the base address of the
 *             MSDU_EXT bank physical/bus address.
 *        Value: higher 4 bytes of MSDU_EXT bank physical / bus address
 *  for systems with 32-bit format for bus addresses:
 *      - BANKx_BASE_ADDRESS
 *        Bits 31:0
 *        Purpose: Provide a mechanism to specify the base address of the
 *             MSDU_EXT bank physical/bus address.
 *        Value: MSDU_EXT bank physical / bus address
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


/*
 * TEMPLATE_HTT_TX_FRAG_DESC_BANK_CFG_T:
 * This macro defines a htt_tx_frag_descXXX_bank_cfg_t in which any physical
 * addresses are stored in a XXX-bit field.
 * This macro is used to define both htt_tx_frag_desc32_bank_cfg_t and
 * htt_tx_frag_desc64_bank_cfg_t structs.
 */
#define TEMPLATE_HTT_TX_FRAG_DESC_BANK_CFG_T(                                  \
    _paddr_bits_,                                                              \
    _paddr__bank_base_address_)                                                \
PREPACK struct htt_tx_frag_desc ## _paddr_bits_ ## _bank_cfg_t {               \
      /** word 0                                                               \
       * msg_type:     8,                                                      \
       * pdev_id:      2,                                                      \
       * swap:         1,                                                      \
       * reserved0:    5,                                                      \
       * num_banks:    8,                                                      \
       * desc_size:    8;                                                      \
       */                                                                      \
    A_UINT32 word0;                                                            \
    /*                                                                         \
     * If bank_base_address is 64 bits, the upper / lower halves are stored    \
     * in little-endian order (bytes 0-3 in the first A_UINT32, bytes 4-7 in   \
     * the second A_UINT32).                                                   \
     */                                                                        \
    _paddr__bank_base_address_[HTT_TX_MSDU_EXT_BANK_MAX];                      \
    A_UINT32 bank_info[HTT_TX_MSDU_EXT_BANK_MAX];                              \
} POSTPACK
/* define htt_tx_frag_desc32_bank_cfg_t */
TEMPLATE_HTT_TX_FRAG_DESC_BANK_CFG_T(32, HTT_VAR_PADDR32(bank_base_address));
/* define htt_tx_frag_desc64_bank_cfg_t */
TEMPLATE_HTT_TX_FRAG_DESC_BANK_CFG_T(64, HTT_VAR_PADDR64_LE(bank_base_address));
/*
 * Make htt_tx_frag_desc_bank_cfg_t be an alias for either
 * htt_tx_frag_desc32_bank_cfg_t or htt_tx_frag_desc64_bank_cfg_t
 */
#if HTT_PADDR64
    #define htt_tx_frag_desc_bank_cfg_t htt_tx_frag_desc64_bank_cfg_t
#else
    #define htt_tx_frag_desc_bank_cfg_t htt_tx_frag_desc32_bank_cfg_t
#endif


/**
 * @brief target -> host HTT TX Credit total count update message definition
 *
 *|31                 16|15|14       9|  8    |7       0 |
 *|---------------------+--+----------+-------+----------|
 *|cur htt credit delta | Q| reserved | sign  | msg type |
 *|------------------------------------------------------|
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
 *   - reserved
 *     Bits 14:9
 *     Value: 0x0
 *   - TXQ_GRP
 *     Bit 15
 *     Purpose: indicates whether any tx queue group information elements
 *         are appended to the tx credit update message
 *     Value: 0 -> no tx queue group information element is present
 *            1 -> a tx queue group information element immediately follows
 *   - DELTA_COUNT
 *     Bits 31:16
 *     Purpose: Specify current htt credit delta absolute count
 */

#define HTT_TX_CREDIT_SIGN_BIT_M       0x00000100
#define HTT_TX_CREDIT_SIGN_BIT_S       8
#define HTT_TX_CREDIT_TXQ_GRP_M        0x00008000
#define HTT_TX_CREDIT_TXQ_GRP_S        15
#define HTT_TX_CREDIT_DELTA_ABS_M      0xffff0000
#define HTT_TX_CREDIT_DELTA_ABS_S      16


#define HTT_TX_CREDIT_SIGN_BIT_SET(word, value)                              \
    do {                                                                     \
        HTT_CHECK_SET_VAL(HTT_TX_CREDIT_SIGN_BIT, value);                    \
        (word) |= (value)  << HTT_TX_CREDIT_SIGN_BIT_S;                      \
    } while (0)

#define HTT_TX_CREDIT_SIGN_BIT_GET(word) \
    (((word) & HTT_TX_CREDIT_SIGN_BIT_M) >> HTT_TX_CREDIT_SIGN_BIT_S)

#define HTT_TX_CREDIT_TXQ_GRP_SET(word, value)                              \
    do {                                                                    \
        HTT_CHECK_SET_VAL(HTT_TX_CREDIT_TXQ_GRP, value);                    \
        (word) |= (value)  << HTT_TX_CREDIT_TXQ_GRP_S;                      \
    } while (0)

#define HTT_TX_CREDIT_TXQ_GRP_GET(word) \
    (((word) & HTT_TX_CREDIT_TXQ_GRP_M) >> HTT_TX_CREDIT_TXQ_GRP_S)

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


enum htt_phy_mode {
    htt_phy_mode_11a            = 0,
    htt_phy_mode_11g            = 1,
    htt_phy_mode_11b            = 2,
    htt_phy_mode_11g_only       = 3,
    htt_phy_mode_11na_ht20      = 4,
    htt_phy_mode_11ng_ht20      = 5,
    htt_phy_mode_11na_ht40      = 6,
    htt_phy_mode_11ng_ht40      = 7,
    htt_phy_mode_11ac_vht20     = 8,
    htt_phy_mode_11ac_vht40     = 9,
    htt_phy_mode_11ac_vht80     = 10,
    htt_phy_mode_11ac_vht20_2g  = 11,
    htt_phy_mode_11ac_vht40_2g  = 12,
    htt_phy_mode_11ac_vht80_2g  = 13,
    htt_phy_mode_11ac_vht80_80  = 14, /* 80+80 */
    htt_phy_mode_11ac_vht160    = 15,

    htt_phy_mode_max,
};

/**
 * @brief target -> host HTT channel change indication
 * @details
 *  Specify when a channel change occurs.
 *  This allows the host to precisely determine which rx frames arrived
 *  on the old channel and which rx frames arrived on the new channel.
 *
 *|31                                         |7       0 |
 *|-------------------------------------------+----------|
 *|                  reserved                 | msg type |
 *|------------------------------------------------------|
 *|              primary_chan_center_freq_mhz            |
 *|------------------------------------------------------|
 *|            contiguous_chan1_center_freq_mhz          |
 *|------------------------------------------------------|
 *|            contiguous_chan2_center_freq_mhz          |
 *|------------------------------------------------------|
 *|                        phy_mode                      |
 *|------------------------------------------------------|
 *
 * Header fields:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as a htt channel change indication message
 *     Value: 0x15
 *   - PRIMARY_CHAN_CENTER_FREQ_MHZ
 *     Bits 31:0
 *     Purpose: identify the (center of the) new 20 MHz primary channel
 *     Value: center frequency of the 20 MHz primary channel, in MHz units
 *   - CONTIG_CHAN1_CENTER_FREQ_MHZ
 *     Bits 31:0
 *     Purpose: identify the (center of the) contiguous frequency range
 *         comprising the new channel.
 *         For example, if the new channel is a 80 MHz channel extending
 *         60 MHz beyond the primary channel, this field would be 30 larger
 *         than the primary channel center frequency field.
 *     Value: center frequency of the contiguous frequency range comprising
 *         the full channel in MHz units
 *         (80+80 channels also use the CONTIG_CHAN2 field)
 *   - CONTIG_CHAN2_CENTER_FREQ_MHZ
 *     Bits 31:0
 *     Purpose: Identify the (center of the) 80 MHz extension frequency range
 *         within a VHT 80+80 channel.
 *         This field is only relevant for VHT 80+80 channels.
 *     Value: center frequency of the 80 MHz extension channel in a VHT 80+80
 *         channel (arbitrary value for cases besides VHT 80+80)
 *   - PHY_MODE
 *     Bits 31:0
 *     Purpose: specify the PHY channel's type (legacy vs. HT vs. VHT), width,
 *         and band
 *     Value: htt_phy_mode enum value
 */

PREPACK struct htt_chan_change_t
{
    /* DWORD 0: flags and meta-data */
    A_UINT32
        msg_type:   8, /* HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE */
        reserved1: 24;
    A_UINT32 primary_chan_center_freq_mhz;
    A_UINT32 contig_chan1_center_freq_mhz;
    A_UINT32 contig_chan2_center_freq_mhz;
    A_UINT32 phy_mode;
} POSTPACK;

#define HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_M  0xffffffff
#define HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_S  0
#define HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_M  0xffffffff
#define HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_S  0
#define HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_M  0xffffffff
#define HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_S  0
#define HTT_CHAN_CHANGE_PHY_MODE_M                      0xffffffff
#define HTT_CHAN_CHANGE_PHY_MODE_S                      0


#define HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_SET(word, value)          \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ, value);\
        (word) |= (value)  << HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_S;  \
    } while (0)
#define HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_GET(word) \
    (((word) & HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_M) \
     >> HTT_CHAN_CHANGE_PRIMARY_CHAN_CENTER_FREQ_MHZ_S)

#define HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_SET(word, value)          \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ, value);\
        (word) |= (value)  << HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_S;  \
    } while (0)
#define HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_GET(word) \
    (((word) & HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_M) \
     >> HTT_CHAN_CHANGE_CONTIG_CHAN1_CENTER_FREQ_MHZ_S)

#define HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_SET(word, value)          \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ, value);\
        (word) |= (value)  << HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_S;  \
    } while (0)
#define HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_GET(word) \
    (((word) & HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_M) \
     >> HTT_CHAN_CHANGE_CONTIG_CHAN2_CENTER_FREQ_MHZ_S)

#define HTT_CHAN_CHANGE_PHY_MODE_SET(word, value)          \
    do {                                                                       \
        HTT_CHECK_SET_VAL(HTT_CHAN_CHANGE_PHY_MODE, value);\
        (word) |= (value)  << HTT_CHAN_CHANGE_PHY_MODE_S;  \
    } while (0)
#define HTT_CHAN_CHANGE_PHY_MODE_GET(word) \
    (((word) & HTT_CHAN_CHANGE_PHY_MODE_M) \
     >> HTT_CHAN_CHANGE_PHY_MODE_S)

#define HTT_CHAN_CHANGE_BYTES sizeof(struct htt_chan_change_t)


/**
 * @brief rx offload packet error message
 *
 * @details
 *  HTT_RX_OFLD_PKT_ERR message is sent by target to host to indicate err
 *  of target payload like mic err.
 *
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |      tid       |     vdev_id    |  msg_sub_type  |    msg_type    |
 *     |-------------------------------------------------------------------|
 *     :                    (sub-type dependent content)                   :
 *     :- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -:
 * Header fields:
 *   - msg_type
 *     Bits 7:0
 *     Purpose: Identifies this as HTT_RX_OFLD_PKT_ERR message
 *     value: 0x16 (HTT_T2H_MSG_TYPE_RX_OFLD_PKT_ERR)
 *   - msg_sub_type
 *     Bits 15:8
 *     Purpose: Identifies which type of rx error is reported by this message
 *     value: htt_rx_ofld_pkt_err_type
 *   - vdev_id
 *     Bits 23:16
 *     Purpose: Identifies which vdev received the erroneous rx frame
 *     value:
 *   - tid
 *     Bits 31:24
 *     Purpose: Identifies the traffic type of the rx frame
 *     value:
 *
 *   - The payload fields used if the sub-type == MIC error are shown below.
 *     Note - MIC err is per MSDU, while PN is per MPDU.
 *     The FW will discard the whole MPDU if any MSDU within the MPDU is marked
 *     with MIC err in A-MSDU case, so FW will send only one HTT message
 *     with the PN of this MPDU attached to indicate MIC err for one MPDU
 *     instead of sending separate HTT messages for each wrong MSDU within
 *     the MPDU.
 *
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |     Rsvd       |     key_id     |             peer_id             |
 *     |-------------------------------------------------------------------|
 *     |                        receiver MAC addr 31:0                     |
 *     |-------------------------------------------------------------------|
 *     |              Rsvd               |    receiver MAC addr 47:32      |
 *     |-------------------------------------------------------------------|
 *     |                     transmitter MAC addr 31:0                     |
 *     |-------------------------------------------------------------------|
 *     |              Rsvd               |    transmitter MAC addr 47:32   |
 *     |-------------------------------------------------------------------|
 *     |                              PN 31:0                              |
 *     |-------------------------------------------------------------------|
 *     |              Rsvd               |              PN 47:32           |
 *     |-------------------------------------------------------------------|
 *   - peer_id
 *     Bits 15:0
 *     Purpose: identifies which peer is frame is from
 *     value:
 *   - key_id
 *     Bits 23:16
 *     Purpose: identifies key_id of rx frame
 *     value:
 *   - RA_31_0 (receiver MAC addr 31:0)
 *     Bits 31:0
 *     Purpose: identifies by MAC address which vdev received the frame
 *     value: MAC address lower 4 bytes
 *   - RA_47_32 (receiver MAC addr 47:32)
 *     Bits 15:0
 *     Purpose: identifies by MAC address which vdev received the frame
 *     value: MAC address upper 2 bytes
 *   - TA_31_0 (transmitter MAC addr 31:0)
 *     Bits 31:0
 *     Purpose: identifies by MAC address which peer transmitted the frame
 *     value: MAC address lower 4 bytes
 *   - TA_47_32 (transmitter MAC addr 47:32)
 *     Bits 15:0
 *     Purpose: identifies by MAC address which peer transmitted the frame
 *     value: MAC address upper 2 bytes
 *   - PN_31_0
 *     Bits 31:0
 *     Purpose: Identifies pn of rx frame
 *     value: PN lower 4 bytes
 *   - PN_47_32
 *     Bits 15:0
 *     Purpose: Identifies pn of rx frame
 *     value:
 *         TKIP or CCMP: PN upper 2 bytes
 *         WAPI: PN bytes 6:5 (bytes 15:7 not included in this message)
 */

enum htt_rx_ofld_pkt_err_type {
    HTT_RX_OFLD_PKT_ERR_TYPE_NONE = 0,
    HTT_RX_OFLD_PKT_ERR_TYPE_MIC_ERR,
};

/* definition for HTT_RX_OFLD_PKT_ERR msg hdr */
#define HTT_RX_OFLD_PKT_ERR_HDR_BYTES 4

#define HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_M     0x0000ff00
#define HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_S     8

#define HTT_RX_OFLD_PKT_ERR_VDEV_ID_M          0x00ff0000
#define HTT_RX_OFLD_PKT_ERR_VDEV_ID_S          16

#define HTT_RX_OFLD_PKT_ERR_TID_M              0xff000000
#define HTT_RX_OFLD_PKT_ERR_TID_S              24

#define HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_M) \
    >> HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_S)
#define HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_VDEV_ID_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_VDEV_ID_M) >> HTT_RX_OFLD_PKT_ERR_VDEV_ID_S)
#define HTT_RX_OFLD_PKT_ERR_VDEV_ID_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_VDEV_ID, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_VDEV_ID_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_TID_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_TID_M) >> HTT_RX_OFLD_PKT_ERR_TID_S)
#define HTT_RX_OFLD_PKT_ERR_TID_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_TID, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_TID_S)); \
    } while (0)

/* definition for HTT_RX_OFLD_PKT_ERR_MIC_ERR msg sub-type payload */
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_BYTES   28

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_M          0x0000ffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_S          0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_M            0x00ff0000
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_S            16

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_M          0xffffffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_S          0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_M         0x0000ffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_S         0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_M          0xffffffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_S          0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_M         0x0000ffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_S         0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_M          0xffffffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_S          0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_M         0x0000ffff
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_S         0

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_31_0_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_RA_47_32_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_31_0_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_TA_47_32_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_31_0_S)); \
    } while (0)

#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_GET(_var) \
    (((_var) & HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_M) >> \
    HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_S)
#define HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32, _val);  \
        ((_var) |= ((_val) << HTT_RX_OFLD_PKT_ERR_MIC_ERR_PN_47_32_S)); \
    } while (0)

/**
 * @brief peer rate report message
 *
 * @details
 *  HTT_T2H_MSG_TYPE_RATE_REPORT message is sent by target to host to
 *  indicate the justified rate of all the peers.
 *
 *     |31         24|23         16|15          8|7           0|
 *     |-------------+-------------+-------------+-------------|
 *     |     peer_count            | reserved    |    msg_type |
 *     |-------------------------------------------------------|
 *     :      Payload (variant number of peer rate report)     :
 *     :- - -- - - - - - - - - - - - - - - - - - - - - - -  - -:
 * Header fields:
 *   - msg_type
 *     Bits 7:0
 *     Purpose: Identifies this as HTT_T2H_MSG_TYPE_RATE_REPORT message.
 *     value: 0x17 (HTT_T2H_MSG_TYPE_RATE_REPORT)
 *   - reserved
 *     Bits 15:8
 *     Purpose:
 *     value:
 *   - peer_count
 *     Bits 31:16
 *     Purpose: Specify how many peer rate report elements are present
 *     in the payload.
 *     value:
 *
 * Payload:
 *     There are variant number of peer rate report follow the first 32
 *     bits. The peer rate report is defined as follows.
 *
 *     |31            20|19    16|15                      0|
 *     |----------------+--------+------------------------|-
 *     |     reserved   |  phy   |        peer_id         | \
 *     |--------------------------------------------------|  -> report #0
 *     |                   rate                           | /
 *     |----------------+--------+------------------------|-
 *     |     reserved   |  phy   |        peer_id         | \
 *     |--------------------------------------------------|  -> report #1
 *     |                   rate                           | /
 *     |----------------+--------+------------------------|-
 *     |     reserved   |  phy   |        peer_id         | \
 *     |--------------------------------------------------|  -> report #2
 *     |                   rate                           | /
 *     |---- ---------------------------------------------|-
 *     :                                                  :
 *     :                                                  :
 *     :                                                  :
 *     :--------------------------------------------------:
 *
 *   - peer_id
 *     Bits 15:0
 *     Purpose: identify the peer
 *     value:
 *   - phy
 *     Bits 19:16
 *     Purpose: identify which phy is in use
 *     value: 0=11b, 1=11a/g, 2=11n, 3=11ac.
 *         Please see enum htt_peer_report_phy_type for detail.
 *   - reserved
 *     Bits 31:20
 *     Purpose:
 *     value:
 *   - rate
 *     Bits 31:0
 *     Purpose: represent the justified rate of the peer specified
 *     by peer_id
 *     value:
 */

enum htt_peer_rate_report_phy_type {
    HTT_PEER_RATE_REPORT_11B = 0,
    HTT_PEER_RATE_REPORT_11A_G,
    HTT_PEER_RATE_REPORT_11N,
    HTT_PEER_RATE_REPORT_11AC,
};

#define HTT_PEER_RATE_REPORT_SIZE                8

#define HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_M    0xffff0000
#define HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_S    16

#define HTT_PEER_RATE_REPORT_MSG_PEER_ID_M       0x0000ffff
#define HTT_PEER_RATE_REPORT_MSG_PEER_ID_S       0

#define HTT_PEER_RATE_REPORT_MSG_PHY_M           0x000f0000
#define HTT_PEER_RATE_REPORT_MSG_PHY_S           16

#define HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_GET(_var) \
    (((_var) & HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_M) \
    >> HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_S)
#define HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_PEER_RATE_REPORT_MSG_PEER_COUNT, _val);  \
        ((_var) |= ((_val) << HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_S)); \
    } while (0)

#define HTT_PEER_RATE_REPORT_MSG_PEER_ID_GET(_var) \
    (((_var) & HTT_PEER_RATE_REPORT_MSG_PEER_ID_M) \
    >> HTT_PEER_RATE_REPORT_MSG_PEER_ID_S)
#define HTT_PEER_RATE_REPORT_MSG_PEER_ID_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_PEER_RATE_REPORT_MSG_PEER_ID, _val);  \
        ((_var) |= ((_val) << HTT_PEER_RATE_REPORT_MSG_PEER_ID_S)); \
    } while (0)

#define HTT_PEER_RATE_REPORT_MSG_PHY_GET(_var) \
    (((_var) & HTT_PEER_RATE_REPORT_MSG_PHY_M) \
    >> HTT_PEER_RATE_REPORT_MSG_PHY_S)
#define HTT_PEER_RATE_REPORT_MSG_PHY_SET(_var, _val) \
    do {                                                     \
        HTT_CHECK_SET_VAL(HTT_PEER_RATE_REPORT_MSG_PHY, _val);  \
        ((_var) |= ((_val) << HTT_PEER_RATE_REPORT_MSG_PHY_S)); \
    } while (0)

/**
 * @brief HTT_T2H_MSG_TYPE_FLOW_POOL_MAP Message
 *
 * @details
 *  HTT_T2H_MSG_TYPE_FLOW_POOL_MAP message is sent by the target when setting up
 *  a flow of descriptors.
 *
 *  This message is in TLV format and indicates the parameters to be setup a
 *  flow in the host. Each entry indicates that a particular flow ID is ready to
 *  receive descriptors from a specified pool.
 *
 *  The message would appear as follows:
 *
 *         |31            24|23            16|15             8|7              0|
 *         |----------------+----------------+----------------+----------------|
 * header  |            reserved             |   num_flows    |     msg_type   |
 *         |-------------------------------------------------------------------|
 *         |                                                                   |
 *         :                              payload                              :
 *         |                                                                   |
 *         |-------------------------------------------------------------------|
 *
 * The header field is one DWORD long and is interpreted as follows:
 * b'0:7   - msg_type:  This will be set to HTT_T2H_MSG_TYPE_FLOW_POOL_MAP
 * b'8-15  - num_flows: This will indicate the number of flows being setup in
 *                      this message
 * b'16-31 - reserved:  These bits are reserved for future use
 *
 * Payload:
 * The payload would contain multiple objects of the following structure. Each
 * object represents a flow.
 *
 *         |31            24|23            16|15             8|7              0|
 *         |----------------+----------------+----------------+----------------|
 * header  |            reserved             |   num_flows    |     msg_type   |
 *         |-------------------------------------------------------------------|
 * payload0|                             flow_type                             |
 *         |-------------------------------------------------------------------|
 *         |                              flow_id                              |
 *         |-------------------------------------------------------------------|
 *         |            reserved0            |          flow_pool_id           |
 *         |-------------------------------------------------------------------|
 *         |            reserved1            |         flow_pool_size          |
 *         |-------------------------------------------------------------------|
 *         |                             reserved2                             |
 *         |-------------------------------------------------------------------|
 * payload1|                             flow_type                             |
 *         |-------------------------------------------------------------------|
 *         |                              flow_id                              |
 *         |-------------------------------------------------------------------|
 *         |            reserved0            |          flow_pool_id           |
 *         |-------------------------------------------------------------------|
 *         |            reserved1            |         flow_pool_size          |
 *         |-------------------------------------------------------------------|
 *         |                             reserved2                             |
 *         |-------------------------------------------------------------------|
 *         |                                 .                                 |
 *         |                                 .                                 |
 *         |                                 .                                 |
 *         |-------------------------------------------------------------------|
 *
 * Each payload is 5 DWORDS long and is interpreted as follows:
 * dword0 - b'0:31  - flow_type: This indicates the type of the entity to which
 *                               this flow is associated. It can be VDEV, peer,
 *                               or tid (AC). Based on enum htt_flow_type.
 *
 * dword1 - b'0:31  - flow_id: Identifier for the flow corresponding to this
 *                             object. For flow_type vdev it is set to the
 *                             vdevid, for peer it is peerid and for tid, it is
 *                             tid_num.
 *
 * dword2 - b'0:15  - flow_pool_id: Identifier of the descriptor-pool being used
 *                                  in the host for this flow
 *          b'16:31 - reserved0: This field in reserved for the future. In case
 *                               we have a hierarchical implementation (HCM) of
 *                               pools, it can be used to indicate the ID of the
 *                               parent-pool.
 *
 * dword3 - b'0:15  - flow_pool_size: Size of the pool in number of descriptors.
 *                                    Descriptors for this flow will be
 *                                    allocated from this pool in the host.
 *          b'16:31 - reserved1: This field in reserved for the future. In case
 *                               we have a hierarchical implementation of pools,
 *                               it can be used to indicate the max number of
 *                               descriptors in the pool. The b'0:15 can be used
 *                               to indicate min number of descriptors in the
 *                               HCM scheme.
 *
 * dword4 - b'0:31  - reserved2: This field in reserved for the future. In case
 *                               we have a hierarchical implementation of pools,
 *                               b'0:15 can be used to indicate the
 *                               priority-based borrowing (PBB) threshold of
 *                               the flow's pool. The b'16:31 are still left
 *                               reserved.
 */

enum htt_flow_type {
	FLOW_TYPE_VDEV = 0,
	/* Insert new flow types above this line */
};

PREPACK struct htt_flow_pool_map_payload_t {
	A_UINT32 flow_type;
	A_UINT32 flow_id;
	A_UINT32 flow_pool_id:16,
		 reserved0:16;
	A_UINT32 flow_pool_size:16,
		 reserved1:16;
	A_UINT32 reserved2;
} POSTPACK;

#define HTT_FLOW_POOL_MAP_HEADER_SZ    (sizeof(A_UINT32))

#define HTT_FLOW_POOL_MAP_PAYLOAD_SZ    \
    (sizeof(struct htt_flow_pool_map_payload_t))

#define HTT_FLOW_POOL_MAP_NUM_FLOWS_M                    0x0000ff00
#define HTT_FLOW_POOL_MAP_NUM_FLOWS_S                    8

#define HTT_FLOW_POOL_MAP_FLOW_TYPE_M                    0xffffffff
#define HTT_FLOW_POOL_MAP_FLOW_TYPE_S                    0

#define HTT_FLOW_POOL_MAP_FLOW_ID_M                      0xffffffff
#define HTT_FLOW_POOL_MAP_FLOW_ID_S                      0

#define HTT_FLOW_POOL_MAP_FLOW_POOL_ID_M                 0x0000ffff
#define HTT_FLOW_POOL_MAP_FLOW_POOL_ID_S                 0

#define HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_M               0x0000ffff
#define HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_S               0

#define HTT_FLOW_POOL_MAP_NUM_FLOWS_GET(_var) \
	(((_var) & HTT_FLOW_POOL_MAP_NUM_FLOWS_M) >> HTT_FLOW_POOL_MAP_NUM_FLOWS_S)

#define HTT_FLOW_POOL_MAP_FLOW_TYPE_GET(_var) \
	(((_var) & HTT_FLOW_POOL_MAP_FLOW_TYPE_M) >> HTT_FLOW_POOL_MAP_FLOW_TYPE_S)

#define HTT_FLOW_POOL_MAP_FLOW_ID_GET(_var) \
	(((_var) & HTT_FLOW_POOL_MAP_FLOW_ID_M) >> HTT_FLOW_POOL_MAP_FLOW_ID_S)

#define HTT_FLOW_POOL_MAP_FLOW_POOL_ID_GET(_var) \
	(((_var) & HTT_FLOW_POOL_MAP_FLOW_POOL_ID_M) >> \
		HTT_FLOW_POOL_MAP_FLOW_POOL_ID_S)

#define HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_GET(_var) \
	(((_var) & HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_M) >> \
		HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_S)

#define HTT_FLOW_POOL_MAP_NUM_FLOWS_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_MAP_NUM_FLOWS, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_MAP_NUM_FLOWS_S)); \
	} while (0)

#define HTT_FLOW_POOL_MAP_FLOW_TYPE_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_MAP_FLOW_TYPE, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_MAP_FLOW_TYPE_S)); \
	} while (0)

#define HTT_FLOW_POOL_MAP_FLOW_ID_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_MAP_FLOW_ID, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_MAP_FLOW_ID_S)); \
	} while (0)

#define HTT_FLOW_POOL_MAP_FLOW_POOL_ID_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_MAP_FLOW_POOL_ID, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_MAP_FLOW_POOL_ID_S)); \
	} while (0)

#define HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_MAP_FLOW_POOL_SIZE_S)); \
	} while (0)

/**
 * @brief HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP Message
 *
 * @details
 *  HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP message is sent by the target when tearing
 *  down a flow of descriptors.
 *  This message indicates that for the flow (whose ID is provided) is wanting
 *  to stop receiving descriptors. This flow ID corresponds to the ID of the
 *  pool of descriptors from where descriptors are being allocated for this
 *  flow. When a flow (and its pool) are unmapped, all the child-pools will also
 *  be unmapped by the host.
 *
 *  The message would appear as follows:
 *
 *     |31            24|23            16|15             8|7              0|
 *     |----------------+----------------+----------------+----------------|
 *     |                     reserved0                    |     msg_type   |
 *     |-------------------------------------------------------------------|
 *     |                             flow_type                             |
 *     |-------------------------------------------------------------------|
 *     |                              flow_id                              |
 *     |-------------------------------------------------------------------|
 *     |             reserved1           |         flow_pool_id            |
 *     |-------------------------------------------------------------------|
 *
 *  The message is interpreted as follows:
 *  dword0 - b'0:7   - msg_type: This will be set to
 *                               HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP
 *           b'8:31  - reserved0: Reserved for future use
 *
 *  dword1 - b'0:31  - flow_type: This indicates the type of the entity to which
 *                                this flow is associated. It can be VDEV, peer,
 *                                or tid (AC). Based on enum htt_flow_type.
 *
 *  dword2 - b'0:31  - flow_id: Identifier for the flow corresponding to this
 *                              object. For flow_type vdev it is set to the
 *                              vdevid, for peer it is peerid and for tid, it is
 *                              tid_num.
 *
 *  dword3 - b'0:15  - flow_pool_id: Identifier of the descriptor-pool being
 *                                   used in the host for this flow
 *           b'16:31 - reserved0: This field in reserved for the future.
 *
 */

PREPACK struct htt_flow_pool_unmap_t {
	A_UINT32 msg_type:8,
		 reserved0:24;
	A_UINT32 flow_type;
	A_UINT32 flow_id;
	A_UINT32 flow_pool_id:16,
		 reserved1:16;
} POSTPACK;

#define HTT_FLOW_POOL_UNMAP_SZ  (sizeof(struct htt_flow_pool_unmap_t))

#define HTT_FLOW_POOL_UNMAP_FLOW_TYPE_M         0xffffffff
#define HTT_FLOW_POOL_UNMAP_FLOW_TYPE_S         0

#define HTT_FLOW_POOL_UNMAP_FLOW_ID_M           0xffffffff
#define HTT_FLOW_POOL_UNMAP_FLOW_ID_S           0

#define HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_M      0x0000ffff
#define HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_S      0

#define HTT_FLOW_POOL_UNMAP_FLOW_TYPE_GET(_var) \
	(((_var) & HTT_FLOW_POOL_UNMAP_FLOW_TYPE_M) >> \
		HTT_FLOW_POOL_UNMAP_FLOW_TYPE_S)

#define HTT_FLOW_POOL_UNMAP_FLOW_ID_GET(_var) \
	(((_var) & HTT_FLOW_POOL_UNMAP_FLOW_ID_M) >> HTT_FLOW_POOL_UNMAP_FLOW_ID_S)

#define HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_GET(_var) \
	(((_var) & HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_M) >> \
		HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_S)

#define HTT_FLOW_POOL_UNMAP_FLOW_TYPE_SET(_var, _val) \
	do {						\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_UNMAP_FLOW_TYPE, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_UNMAP_FLOW_TYPE_S)); \
	} while (0)

#define HTT_FLOW_POOL_UNMAP_FLOW_ID_SET(_var, _val) \
	do {					\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_UNMAP_FLOW_ID, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_UNMAP_FLOW_ID_S)); \
	} while (0)

#define HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_SET(_var, _val) \
	do {							\
		HTT_CHECK_SET_VAL(HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID, _val);  \
		((_var) |= ((_val) << HTT_FLOW_POOL_UNMAP_FLOW_POOL_ID_S)); \
	} while (0)

#endif
