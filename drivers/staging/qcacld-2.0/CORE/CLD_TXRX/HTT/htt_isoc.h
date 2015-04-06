/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
 * @file htt_isoc.h
 *
 * @details
 *  This file defines the target --> host messages that configure the
 *  host data-path SW with the information required for data transfers
 *  to and from the target.
 */

#ifndef _HTT_ISOC_H_
#define _HTT_ISOC_H_

#include <a_types.h>    /* A_UINT32, A_UINT8 */

#ifdef ATHR_WIN_NWF
#pragma warning( disable:4214 ) //bit field types other than int
#endif

#include "htt_common.h"

/*=== definitions that apply to all messages ================================*/

typedef enum htt_isoc_t2h_msg_type {
    /* 0x0 reserved for VERSION message (probably not needed) */

    /* PEER_INFO - specify ID and parameters of a new peer */
    HTT_ISOC_T2H_MSG_TYPE_PEER_INFO     = 0x1,

    /* PEER_UNMAP - deallocate the ID that refers to a peer */
    HTT_ISOC_T2H_MSG_TYPE_PEER_UNMAP    = 0x2,

    /* ADDBA - start rx aggregation for the specified peer-TID */
    HTT_ISOC_T2H_MSG_TYPE_RX_ADDBA      = 0x3,

    /* DELBA - stop rx aggregation for the specified peer-TID */
    HTT_ISOC_T2H_MSG_TYPE_RX_DELBA      = 0x4,

    /* TX_COMPL_IND - over-the-air tx completion notification for a tx frame */
    HTT_ISOC_T2H_MSG_TYPE_TX_COMPL_IND  = 0x5,

    /* SEC_IND - notification of the type of security used for a new peer */
    HTT_ISOC_T2H_MSG_TYPE_SEC_IND       = 0x6,

    /* PEER_TX_READY - the target is ready to transmit to a new peer */
    HTT_ISOC_T2H_MSG_TYPE_PEER_TX_READY = 0x7,

    /* RX_ERR - notification that an rx frame was discarded due to errors */
    HTT_ISOC_T2H_MSG_TYPE_RX_ERR        = 0x8,


    /* keep this last */
    HTT_ISOC_T2H_NUM_MSGS
} htt_isoc_t2h_msg_type;

/*
 * HTT ISOC target to host message type -
 * stored in bits 7:0 of the first word of the message
 */
#define HTT_ISOC_T2H_MSG_TYPE_M      0xff
#define HTT_ISOC_T2H_MSG_TYPE_S      0

#define HTT_ISOC_T2H_MSG_TYPE_SET(msg_addr, msg_type) \
    (*((A_UINT8 *) msg_addr) = (msg_type))
#define HTT_ISOC_T2H_MSG_TYPE_GET(msg_addr) \
    (*((A_UINT8 *) msg_addr))

#ifndef INLINE
/* target FW */
#define INLINE __inline
#define HTT_ISOC_INLINE_DEF
#endif /* INLINE */

static INLINE void
htt_isoc_t2h_field_set(
    A_UINT32 *msg_addr32,
    unsigned offset32,
    unsigned mask,
    unsigned shift,
    unsigned value)
{
    /* sanity check: make sure the value fits within the field */
    //adf_os_assert(value << shift == (value << shift) | mask);

    msg_addr32 += offset32;
    /* clear the field */
    *msg_addr32 &= ~mask;
    /* write the new value */
    *msg_addr32 |= (value << shift);
}

#ifdef HTT_ISOC_INLINE_DEF
#undef HTT_ISOC_INLINE_DEF
#undef INLINE
#endif

#define HTT_ISOC_T2H_FIELD_GET(msg_addr32, offset32, mask, shift) \
    (((*(msg_addr32 + offset32)) & mask) >> shift)

typedef enum {
    /* ASSOC - "real" peer from STA-AP association */
    HTT_ISOC_T2H_PEER_TYPE_ASSOC = 0x0,

    /* SELF - self-peer for unicast tx to unassociated peer */
    HTT_ISOC_T2H_PEER_TYPE_SELF  = 0x1,

    /* BSSID - reserved for FW use for BT-AMP+IBSS */
    HTT_ISOC_T2H_PEER_TYPE_BSSID = 0x2,

    /* BCAST - self-peer for multicast / broadcast tx */
    HTT_ISOC_T2H_PEER_TYPE_BCAST = 0x3
} HTT_ISOC_T2H_PEER_TYPE_ENUM;

enum {
    HTT_ISOC_NON_QOS = 0,
    HTT_ISOC_QOS     = 1
};

enum {
    HTT_ISOC_RMF_DISABLED = 0,
    HTT_ISOC_RMF_ENABLED  = 1
};

enum {
    HTT_ISOC_TID_MGMT = 7
};


/*=== definitions for specific messages =====================================*/

/*=== PEER_INFO message ===*/

/**
 * @brief target -> host peer info message definition
 *
 * @details
 * The following diagram shows the format of the peer info message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31          25|24|23       18|17|16|15      11|10|9|8|7|6|            0|
 * |-----------------------------------------------------------------------|
 * |   mgmt DPU idx  |  bcast DPU idx  |     DPU idx     |     msg type    |
 * |-----------------------------------------------------------------------|
 * | mgmt DPU sig |bcast DPU sig |     DPU sig    |       peer ID          |
 * |-----------------------------------------------------------------------|
 * |    MAC addr 1   |    MAC addr 0   |     vdev ID     | |R|  peer type  |
 * |-----------------------------------------------------------------------|
 * |    MAC addr 5   |    MAC addr 4   |    MAC addr 3   |    MAC addr 2   |
 * |-----------------------------------------------------------------------|
 *
 *
 * The following field definitions describe the format of the peer info
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as peer info message
 *     Value: 0x1
 *   - DPU_IDX
 *     Bits 15:8
 *     Purpose: specify the DPU index (a.k.a. security key ID) to use for
 *         unicast data frames sent to this peer
 *     Value: key ID
 *   - BCAST_DPU_IDX
 *     Bits 23:16
 *     Purpose: specify the DPU index (a.k.a. security key ID) to use for
 *         broadcast data frames sent by this (self) peer
 *     Value: key ID
 *   - MGMT_DPU_IDX
 *     Bits 31:24
 *     Purpose: specify the DPU index (a.k.a. security key ID) to use for
 *         unicast management frames sent by this (self) peer
 *     Value: key ID
 * WORD 1:
 *   - PEER_ID
 *     Bits 10:0
 *     Purpose: The ID that the target has allocated to refer to the peer
 *   - DPU_SIG
 *     Bits 17:11
 *     Purpose: specify the DPU signature (a.k.a. security key validity
 *         magic number) to specify for unicast data frames sent to this peer
 *   - BCAST_DPU_SIG
 *     Bits 24:18
 *     Purpose: specify the DPU signature (a.k.a. security key validity
 *         magic number) to specify for broadcast data frames sent by this
 *         (self) peer
 *   - MGMT_DPU_SIG
 *     Bits 31:25
 *     Purpose: specify the DPU signature (a.k.a. security key validity
 *         magic number) to specify for unicast management frames sent by this
 *         (self) peer
 * WORD 2:
 *   - PEER_TYPE
 *     Bits 5:0
 *     Purpose: specify whether the peer in question is a real peer or
 *         one of the types of "self-peer" created for the vdev
 *     Value: HTT_ISOC_T2H_PEER_TYPE enum
 *   - RMF_ENABLED (R)
 *     Bit 6
 *     Purpose: specify whether the peer in question has enable robust
 *         management frames, to encrypt certain managment frames
 *     Value: HTT_ISOC_RMF enum
 *     Value: HTT_ISOC_NON_QOS or HTT_ISOC_QOS
 *   - VDEV_ID
 *     Bits 15:8
 *     Purpose: For a real peer, the vdev ID indicates which virtual device
 *         the peer is associated with.  For a self-peer, the vdev ID shows
 *         which virtual device the self-peer represents.
 *   - MAC_ADDR_L16
 *     Bits 31:16
 *     Purpose: Identifies which peer the peer ID is for.
 *     Value: lower 2 bytes of the peer's MAC address
 *         For a self-peer, the peer's MAC address is the MAC address of the
 *         vdev the self-peer represents.
 * WORD 3:
 *   - MAC_ADDR_U32
 *     Bits 31:0
 *     Purpose: Identifies which peer the peer ID is for.
 *     Value: upper 4 bytes of the peer's MAC address
 *         For a self-peer, the peer's MAC address is the MAC address of the
 *         vdev the self-peer represents.
 */
typedef struct htt_isoc_t2h_peer_info_s {
    /* word 0 */
    A_UINT32
        msg_type:      8, /* HTT_ISOC_T2H_MSG_TYPE_PEER_INFO */
        dpu_idx:       8,
        bcast_dpu_idx: 8,
        mgmt_dpu_idx:  8;
    /* word 1 */
    A_UINT32
        peer_id:      11,
        dpu_sig:       7,
        bcast_dpu_sig: 7,
        mgmt_dpu_sig:  7;
    /* word 2 */
    A_UINT32
        peer_type:     6,
        rmf_enabled:   1,
        reserved0:     1,
        vdev_id:       8,
        mac_addr_l16: 16;
    /* word 3 */
    A_UINT32 mac_addr_u32;
} htt_isoc_t2h_peer_info_t;

/* word 0 */
#define HTT_ISOC_T2H_PEER_INFO_DPU_IDX_OFFSET32        0
#define HTT_ISOC_T2H_PEER_INFO_DPU_IDX_M               0x0000ff00
#define HTT_ISOC_T2H_PEER_INFO_DPU_IDX_S               8

#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_IDX_OFFSET32  0
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_IDX_M         0x00ff0000
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_IDX_S         16

#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_IDX_OFFSET32   0
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_IDX_M          0xff000000
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_IDX_S          24

/* word 1 */
#define HTT_ISOC_T2H_PEER_INFO_PEER_ID_OFFSET32        1
#define HTT_ISOC_T2H_PEER_INFO_PEER_ID_M               0x000007ff
#define HTT_ISOC_T2H_PEER_INFO_PEER_ID_S               0

#define HTT_ISOC_T2H_PEER_INFO_DPU_SIG_OFFSET32        1
#define HTT_ISOC_T2H_PEER_INFO_DPU_SIG_M               0x0003f800
#define HTT_ISOC_T2H_PEER_INFO_DPU_SIG_S               11

#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_SIG_OFFSET32  1
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_SIG_M         0x01fc0000
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_SIG_S         18

#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_SIG_OFFSET32   1
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_SIG_M          0xfe000000
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_SIG_S          25

/* word 2 */
#define HTT_ISOC_T2H_PEER_INFO_PEER_TYPE_OFFSET32      2
#define HTT_ISOC_T2H_PEER_INFO_PEER_TYPE_M             0x0000003f
#define HTT_ISOC_T2H_PEER_INFO_PEER_TYPE_S             0

#define HTT_ISOC_T2H_PEER_INFO_RMF_ENABLED_OFFSET32    2
#define HTT_ISOC_T2H_PEER_INFO_RMF_ENABLED_M           0x00000040
#define HTT_ISOC_T2H_PEER_INFO_RMF_ENABLED_S           6

#define HTT_ISOC_T2H_PEER_INFO_VDEV_ID_OFFSET32        2
#define HTT_ISOC_T2H_PEER_INFO_VDEV_ID_M               0x0000ff00
#define HTT_ISOC_T2H_PEER_INFO_VDEV_ID_S               8

#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_L16_OFFSET32   2
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_L16_M          0xffff0000
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_L16_S          16

/* word 3 */
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_U32_OFFSET32   3
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_U32_M          0xffffffff
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_U32_S          0


/* general field access macros */

#define HTT_ISOC_T2H_PEER_INFO_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                      \
        ((A_UINT32 *) msg_addr),                                 \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _M,                  \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_PEER_INFO_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                               \
        ((A_UINT32 *) msg_addr),                          \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _M,           \
        HTT_ISOC_T2H_PEER_INFO_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_PEER_INFO_DPU_IDX_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(DPU_IDX, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_DPU_IDX_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(DPU_IDX, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_IDX_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(BCAST_DPU_IDX, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_IDX_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(BCAST_DPU_IDX, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_IDX_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(MGMT_DPU_IDX, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_IDX_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(MGMT_DPU_IDX, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(PEER_ID, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_DPU_SIG_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(DPU_SIG, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_DPU_SIG_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(DPU_SIG, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_SIG_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(BCAST_DPU_SIG, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_BCAST_DPU_SIG_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(BCAST_DPU_SIG, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_SIG_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(MGMT_DPU_SIG, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_MGMT_DPU_SIG_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(MGMT_DPU_SIG, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_PEER_TYPE_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(PEER_TYPE, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_PEER_TYPE_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(PEER_TYPE, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_QOS_CAPABLE_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(QOS_CAPABLE, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_QOS_CAPABLE_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(QOS_CAPABLE, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_RMF_ENABLED_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(RMF_ENABLED, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_RMF_ENABLED_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(RMF_ENABLED, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_VDEV_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(VDEV_ID, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_VDEV_ID_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(VDEV_ID, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_L16_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(MAC_ADDR_L16, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_L16_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(MAC_ADDR_L16, msg_addr)

#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_U32_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_SET(MAC_ADDR_U32, msg_addr, value)
#define HTT_ISOC_T2H_PEER_INFO_MAC_ADDR_U32_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_INFO_FIELD_GET(MAC_ADDR_U32, msg_addr)

/*=== PEER_UNMAP message ===*/

/**
 * @brief target -> host peer unmap message definition
 *
 * @details
 * The following diagram shows the format of the peer unmap message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31                      19|18                       8|7               0|
 * |-----------------------------------------------------------------------|
 * |         reserved         |          peer ID         |     msg type    |
 * |-----------------------------------------------------------------------|
 *
 *
 * The following field definitions describe the format of the peer info
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as peer unmap message
 *     Value: 0x2
 *   - PEER_ID
 *     Bits 18:8
 *     Purpose: The ID that the target has allocated to refer to the peer
 */
typedef struct htt_isoc_t2h_peer_unmap_s {
    /* word 0 */
    A_UINT32
        msg_type:      8, /* HTT_ISOC_T2H_MSG_TYPE_PEER_UNMAP */
        peer_id:      11,
        reserved0:    13;
} htt_isoc_t2h_peer_unmap_t;

/* word 0 */
#define HTT_ISOC_T2H_PEER_UNMAP_PEER_ID_OFFSET32        0
#define HTT_ISOC_T2H_PEER_UNMAP_PEER_ID_M               0x0007ff00
#define HTT_ISOC_T2H_PEER_UNMAP_PEER_ID_S               8


/* general field access macros */

#define HTT_ISOC_T2H_PEER_UNMAP_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                       \
        ((A_UINT32 *) msg_addr),                                  \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _M,                  \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_PEER_UNMAP_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                                \
        ((A_UINT32 *) msg_addr),                           \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _M,           \
        HTT_ISOC_T2H_PEER_UNMAP_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_PEER_UNMAP_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_UNMAP_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_PEER_UNMAP_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_UNMAP_FIELD_GET(PEER_ID, msg_addr)

/*=== ADDBA message ===*/
enum {
    htt_isoc_addba_success = 0,
    /* TBD: use different failure values to specify failure causes? */
    htt_isoc_addba_fail = 1,
};

/**
 * @brief target -> host ADDBA message definition
 *
 * @details
 * The following diagram shows the format of the rx ADDBA message sent
 * from the target to the host:
 *
 * |31                      20|19  16|15     12|11    8|7               0|
 * |---------------------------------------------------------------------|
 * |          peer ID         |  TID |   window size   |     msg type    |
 * |---------------------------------------------------------------------|
 * |                  reserved                |S|      start seq num     |
 * |---------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the ADDBA
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an ADDBA message
 *     Value: 0x3
 *   - WIN_SIZE
 *     Bits 15:8
 *     Purpose: Specifies the length of the block ack window (max = 64).
 *     Value:
 *         block ack window length specified by the received ADDBA
 *         management message.
 *   - TID
 *     Bits 19:16
 *     Purpose: Specifies which traffic identifier the ADDBA is for.
 *     Value:
 *         TID specified by the received ADDBA management message.
 *   - PEER_ID
 *     Bits 31:20
 *     Purpose: Identifies which peer sent the ADDBA.
 *     Value:
 *         ID (hash value) used by the host for fast, direct lookup of
 *         host SW peer info, including rx reorder states.
 *   - START_SEQ_NUM
 *     Bits 11:0
 *     Purpose: Specifies the initial location of the block ack window
 *     Value: start sequence value specified by the ADDBA-request message
 *   - STATUS
 *     Bit 12
 *     Purpose: status of the WMI ADDBA request
 *     Value: 0 - SUCCESS, 1 - FAILURE
 */
typedef struct htt_isoc_t2h_addba_s {
    /* word 0 */
    A_UINT32 msg_type:       8, /* HTT_ISOC_T2H_MSG_TYPE_ADDBA */
             win_size:       8,
             tid:            4,
             peer_id:       12;
    /* word 1 */
    A_UINT32 start_seq_num: 12,
             status:         1,
             reserved0:     19;
} htt_isoc_t2h_addba_t;

/* word 0 */
#define HTT_ISOC_T2H_ADDBA_WIN_SIZE_OFFSET32       0
#define HTT_ISOC_T2H_ADDBA_WIN_SIZE_M              0x0000ff00
#define HTT_ISOC_T2H_ADDBA_WIN_SIZE_S              8

#define HTT_ISOC_T2H_ADDBA_TID_OFFSET32            0
#define HTT_ISOC_T2H_ADDBA_TID_M                   0x000f0000
#define HTT_ISOC_T2H_ADDBA_TID_S                   16

#define HTT_ISOC_T2H_ADDBA_PEER_ID_OFFSET32        0
#define HTT_ISOC_T2H_ADDBA_PEER_ID_M               0xfff00000
#define HTT_ISOC_T2H_ADDBA_PEER_ID_S               20

/* word 1 */
#define HTT_ISOC_T2H_ADDBA_START_SEQ_NUM_OFFSET32  1
#define HTT_ISOC_T2H_ADDBA_START_SEQ_NUM_M         0x00000fff
#define HTT_ISOC_T2H_ADDBA_START_SEQ_NUM_S         0

#define HTT_ISOC_T2H_ADDBA_STATUS_OFFSET32         1
#define HTT_ISOC_T2H_ADDBA_STATUS_M                0x00001000
#define HTT_ISOC_T2H_ADDBA_STATUS_S                12

/* general field access macros */
#define HTT_ISOC_T2H_ADDBA_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                       \
        ((A_UINT32 *) msg_addr),                                  \
        HTT_ISOC_T2H_ADDBA_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_ADDBA_ ## field ## _M,                  \
        HTT_ISOC_T2H_ADDBA_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_ADDBA_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                                \
        ((A_UINT32 *) msg_addr),                           \
        HTT_ISOC_T2H_ADDBA_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_ADDBA_ ## field ## _M,           \
        HTT_ISOC_T2H_ADDBA_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_ADDBA_WIN_SIZE_SET(msg_addr, value) \
    HTT_ISOC_T2H_ADDBA_FIELD_SET(WIN_SIZE, msg_addr, value)
#define HTT_ISOC_T2H_ADDBA_WIN_SIZE_GET(msg_addr) \
    HTT_ISOC_T2H_ADDBA_FIELD_GET(WIN_SIZE, msg_addr)

#define HTT_ISOC_T2H_ADDBA_TID_SET(msg_addr, value) \
    HTT_ISOC_T2H_ADDBA_FIELD_SET(TID, msg_addr, value)
#define HTT_ISOC_T2H_ADDBA_TID_GET(msg_addr) \
    HTT_ISOC_T2H_ADDBA_FIELD_GET(TID, msg_addr)

#define HTT_ISOC_T2H_ADDBA_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_ADDBA_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_ADDBA_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_ADDBA_FIELD_GET(PEER_ID, msg_addr)

#define HTT_ISOC_T2H_ADDBA_START_SEQ_NUM_SET(msg_addr, value) \
    HTT_ISOC_T2H_ADDBA_FIELD_SET(START_SEQ_NUM, msg_addr, value)
#define HTT_ISOC_T2H_ADDBA_START_SEQ_NUM_GET(msg_addr) \
    HTT_ISOC_T2H_ADDBA_FIELD_GET(START_SEQ_NUM, msg_addr)

#define HTT_ISOC_T2H_ADDBA_STATUS_SET(msg_addr, value) \
    HTT_ISOC_T2H_ADDBA_FIELD_SET(STATUS, msg_addr, value)
#define HTT_ISOC_T2H_ADDBA_STATUS_GET(msg_addr) \
    HTT_ISOC_T2H_ADDBA_FIELD_GET(STATUS, msg_addr)

/*=== DELBA message ===*/

/**
 * @brief target -> host DELBA message definition
 *
 * @details
 * The following diagram shows the format of the rx DELBA message sent
 * from the target to the host:
 *
 * |31                      20|19  16|15     12|11    8|7               0|
 * |---------------------------------------------------------------------|
 * |          peer ID         |  TID |    reserved   |S|     msg type    |
 * |---------------------------------------------------------------------|
 *
 * The following field definitions describe the format of the ADDBA
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an DELBA message
 *     Value: 0x4
 *   - TID
 *     Bits 19:16
 *     Purpose: Specifies which traffic identifier the DELBA is for.
 *     Value:
 *         TID specified by the received DELBA management message.
 *   - PEER_ID
 *     Bits 31:20
 *     Purpose: Identifies which peer sent the DELBA.
 *     Value:
 *         ID (hash value) used by the host for fast, direct lookup of
 *         host SW peer info, including rx reorder states.
 *   - STATUS
 *     Bit 8
 *     Purpose: status of the WMI DELBA request
 *     Value: 0 - SUCCESS, 1 - FAILURE
 */
typedef struct htt_isoc_t2h_delba_s {
    /* word 0 */
    A_UINT32
        msg_type:       8, /* HTT_ISOC_T2H_MSG_TYPE_DELBA */
        status:         1,
        reserved0:      7,
        tid:            4,
        peer_id:       12;
} htt_isoc_t2h_delba_t;

/* word 0 */
#define HTT_ISOC_T2H_DELBA_TID_OFFSET32            0
#define HTT_ISOC_T2H_DELBA_TID_M                   0x000f0000
#define HTT_ISOC_T2H_DELBA_TID_S                   16

#define HTT_ISOC_T2H_DELBA_PEER_ID_OFFSET32        0
#define HTT_ISOC_T2H_DELBA_PEER_ID_M               0xfff00000
#define HTT_ISOC_T2H_DELBA_PEER_ID_S               20

#define HTT_ISOC_T2H_DELBA_STATUS_OFFSET32         0
#define HTT_ISOC_T2H_DELBA_STATUS_M                0x00000100
#define HTT_ISOC_T2H_DELBA_STATUS_S                8

/* general field access macros */

#define HTT_ISOC_T2H_DELBA_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                       \
        ((A_UINT32 *) msg_addr),                                  \
        HTT_ISOC_T2H_DELBA_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_DELBA_ ## field ## _M,                  \
        HTT_ISOC_T2H_DELBA_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_DELBA_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                                \
        ((A_UINT32 *) msg_addr),                           \
        HTT_ISOC_T2H_DELBA_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_DELBA_ ## field ## _M,           \
        HTT_ISOC_T2H_DELBA_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_DELBA_TID_SET(msg_addr, value) \
    HTT_ISOC_T2H_DELBA_FIELD_SET(TID, msg_addr, value)
#define HTT_ISOC_T2H_DELBA_TID_GET(msg_addr) \
    HTT_ISOC_T2H_DELBA_FIELD_GET(TID, msg_addr)

#define HTT_ISOC_T2H_DELBA_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_DELBA_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_DELBA_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_DELBA_FIELD_GET(PEER_ID, msg_addr)

#define HTT_ISOC_T2H_DELBA_STATUS_SET(msg_addr, value) \
    HTT_ISOC_T2H_DELBA_FIELD_SET(STATUS, msg_addr, value)
#define HTT_ISOC_T2H_DELBA_STATUS_GET(msg_addr) \
    HTT_ISOC_T2H_DELBA_FIELD_GET(STATUS, msg_addr)

/*=== SEC_IND message ===*/

/**
 * @brief target -> host Security indication message definition
 *
 * @details
 * The following diagram shows the format of the SEC_IND message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31          25|24|23       18|17|16|15      11|10|9|8|7|6|            0|
 * |-----------------------------------------------------------------------|
 * |   is unicast    |  sec type       |     Peer id     |     msg type    |
 * |-----------------------------------------------------------------------|
 * |                    mic key1                                           |
 * |-----------------------------------------------------------------------|
 * |                    mic key2                                           |
 * |-----------------------------------------------------------------------|
 *
 *
 * The following field definitions describe the format of the peer info
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as SEC_IND message
 *     Value: 0x6
 *   - PEER_ID
 *     Bits 15:8
 *     Purpose: The ID that the target has allocated to refer to the peer
 *     Value: Peer ID
 *   - SEC_TYPE
 *     Bits 23:16
 *     Purpose: specify the security encryption type
 *     Value: htt_sec_type
 *   - is unicast
 *     Bits 31:24
 *     Purpose: specify unicast/bcast
 *     Value: 1-unicast/0-bcast
 * WORD 1:
 *   - MIC1
 *     Bits 31:0
 *     Purpose: Mickey1
 * WORD 2:
 *   - MIC2
 *     Bits 31:0
 *     Purpose: Mickey2
 */
typedef struct htt_isoc_t2h_sec_ind_s {
    /* word 0 */
    A_UINT32
        msg_type:      8, /* HTT_ISOC_T2H_MSG_TYPE_SEC_IND */
        peer_id:       8,
        sec_type:      8,
        is_unicast:    8;
    /* word 1 */
    A_UINT32 mic_key1;
    /* word 2 */
    A_UINT32 mic_key2;
    /* word 3 */
    A_UINT32 status;
} htt_isoc_t2h_sec_ind_t;

/* word 0 */
#define HTT_ISOC_T2H_SEC_IND_PEER_ID_OFFSET32        0
#define HTT_ISOC_T2H_SEC_IND_PEER_ID_M               0x0000ff00
#define HTT_ISOC_T2H_SEC_IND_PEER_ID_S               8

#define HTT_ISOC_T2H_SEC_IND_SEC_TYPE_OFFSET32       0
#define HTT_ISOC_T2H_SEC_IND_SEC_TYPE_M              0x00ff0000
#define HTT_ISOC_T2H_SEC_IND_SEC_TYPE_S              16

#define HTT_ISOC_T2H_SEC_IND_IS_UNICAST_OFFSET32     0
#define HTT_ISOC_T2H_SEC_IND_IS_UNICAST_M            0xff000000
#define HTT_ISOC_T2H_SEC_IND_IS_UNICAST_S            24

/* word 1 */
#define HTT_ISOC_T2H_SEC_IND_MIC1_OFFSET32           1
#define HTT_ISOC_T2H_SEC_IND_MIC1_M                  0xffffffff
#define HTT_ISOC_T2H_SEC_IND_MIC1_S                  0

/* word 2 */
#define HTT_ISOC_T2H_SEC_IND_MIC2_OFFSET32           2
#define HTT_ISOC_T2H_SEC_IND_MIC2_M                  0xffffffff
#define HTT_ISOC_T2H_SEC_IND_MIC2_S                  0


/* general field access macros */
#define HTT_ISOC_T2H_SEC_IND_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                      \
        ((A_UINT32 *) msg_addr),                                 \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _M,                  \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_SEC_IND_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                               \
        ((A_UINT32 *) msg_addr),                          \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _M,           \
        HTT_ISOC_T2H_SEC_IND_ ## field ## _S)

/* access macros for specific fields */
#define HTT_ISOC_T2H_SEC_IND_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_SEC_IND_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_SEC_IND_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_SEC_IND_FIELD_GET(PEER_ID, msg_addr)

#define HTT_ISOC_T2H_SEC_IND_SEC_TYPE_SET(msg_addr, value) \
    HTT_ISOC_T2H_SEC_IND_FIELD_SET(SEC_TYPE, msg_addr, value)
#define HTT_ISOC_T2H_SEC_IND_SEC_TYPE_GET(msg_addr) \
    HTT_ISOC_T2H_SEC_IND_FIELD_GET(SEC_TYPE, msg_addr)

#define HTT_ISOC_T2H_SEC_IND_IS_UNICAST_SET(msg_addr, value) \
    HTT_ISOC_T2H_SEC_IND_FIELD_SET(IS_UNICAST, msg_addr, value)
#define HTT_ISOC_T2H_SEC_IND_IS_UNICAST_GET(msg_addr) \
    HTT_ISOC_T2H_SEC_IND_FIELD_GET(IS_UNICAST, msg_addr)

#define HTT_ISOC_T2H_SEC_IND_MIC1_SET(msg_addr, value) \
    HTT_ISOC_T2H_SEC_IND_FIELD_SET(MIC1, msg_addr, value)
#define HTT_ISOC_T2H_SEC_IND_MIC1_GET(msg_addr) \
    HTT_ISOC_T2H_SEC_IND_FIELD_GET(MIC1, msg_addr)

#define HTT_ISOC_T2H_SEC_IND_MIC2_SET(msg_addr, value) \
    HTT_ISOC_T2H_SEC_IND_FIELD_SET(MIC2, msg_addr, value)
#define HTT_ISOC_T2H_SEC_IND_MIC2_GET(msg_addr) \
    HTT_ISOC_T2H_SEC_IND_FIELD_GET(MIC2, msg_addr)

/*=== PEER_TX_READY message ===*/

/**
 * @brief target -> host peer tx ready message definition
 *
 * @details
 * The following diagram shows the format of the peer tx ready message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31                      19|18                       8|7               0|
 * |-----------------------------------------------------------------------|
 * |         reserved         |          peer ID         |     msg type    |
 * |-----------------------------------------------------------------------|
 *
 *
 * The following field definitions describe the format of the peer info
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as peer tx ready message
 *     Value: 0x7
 *   - PEER_ID
 *     Bits 18:8
 *     Purpose: The ID assigned to the peer by the PEER_INFO message
 */
typedef struct htt_isoc_t2h_peer_tx_ready_s {
    /* word 0 */
    A_UINT32
        msg_type:      8, /* HTT_ISOC_T2H_MSG_TYPE_PEER_TX_READY */
        peer_id:      11,
        reserved0:    13;
} htt_isoc_t2h_peer_tx_ready_t;

/* word 0 */
#define HTT_ISOC_T2H_PEER_TX_READY_PEER_ID_OFFSET32        0
#define HTT_ISOC_T2H_PEER_TX_READY_PEER_ID_M               0x0007ff00
#define HTT_ISOC_T2H_PEER_TX_READY_PEER_ID_S               8


/* general field access macros */

#define HTT_ISOC_T2H_PEER_TX_READY_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                       \
        ((A_UINT32 *) msg_addr),                                  \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _M,                  \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_PEER_TX_READY_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                                \
        ((A_UINT32 *) msg_addr),                           \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _M,           \
        HTT_ISOC_T2H_PEER_TX_READY_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_PEER_TX_READY_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_PEER_TX_READY_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_PEER_TX_READY_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_PEER_TX_READY_FIELD_GET(PEER_ID, msg_addr)


/*=== RX_ERR message ===*/

/**
 * @brief target -> host rx error notification message definition
 *
 * @details
 * The following diagram shows the format of the rx err message sent
 * from the target to the host.  This layout assumes the target operates
 * as little-endian.
 *
 * |31                               16|15              8|7|6|5|4       0|
 * |---------------------------------------------------------------------|
 * |               peer ID             |   rx err type   |    msg type   |
 * |---------------------------------------------------------------------|
 * |               reserved            |   rx err count  |M| r | ext TID |
 * |---------------------------------------------------------------------|
 * M = multicast
 * r = reserved
 *
 * The following field definitions describe the format of the peer info
 * message sent from the target to the host.
 *
 * WORD 0:
 *   - MSG_TYPE
 *     Bits 7:0
 *     Purpose: identifies this as an rx err message
 *     Value: 0x8
 *   - RX_ERR_TYPE
 *     Bits 15:8
 *     Purpose: specifies which type of rx error is being reported
 *     Value: htt_rx_ind_mpdu_status enum
 *   - PEER_ID
 *     Bits 31:16
 *     Purpose: specify which peer sent the frame that resulted in an error
 * WORD 1:
 *   - EXT_TID
 *     Bits 4:0
 *     Purpose: specifies which traffic type had the rx error
 *     Value: 0-15 for a real TID value, 16 for non-QoS data, 31 for unknown
 *   - MCAST
 *     Bit 6
 *     Purpose: specify whether the rx error frame was unicast or multicast
 *     Value: 0 -> unicast, 1 -> multicast
 *   - L2_HDR_IS_80211
 *     Bit 7
 *     Purpose: specifies whether the included L2 header (if present) is in
 *         802.3 or 802.11 format
 *     Value: 0 -> 802.3, 1 -> 802.11
 *   - L2_HDR_BYTES
 *     Bits 15:8
 *     Purpose: Specify the size of the L2 header in this rx error report.
 *     Value:
 *         If no L2 header is included, this field shall be 0.
 *         If a 802.3 + LLC/SNAP header is included, this field shall be
 *         14 (ethernet header) + 8 (LLC/SNAP).
 *         If a 802.11 header is included, this field shall be 24 bytes for
 *         a basic header, or 26 bytes if a QoS control field is included,
 *         or 30 bytes if a 4th address is included, or 32 bytes if a 4th
 *         address and a QoS control field are included, etc.
 *         Though the L2 header included in the message needs to include
 *         padding up to a 4-byte boundary, this L2 header size field need
 *         not account for the padding following the L2 header.
 *   - SEC_HDR_BYTES
 *     Bits 23:16
 *     Purpose: Specify the size of the security encapsulation header in
 *         this rx error report.
 *     Value:
 *         If no security header is included, this field shall be 0.
 *         If a security header is included, this field depends on the
 *         security type, which can be inferred from the rx error type.
 *         For TKIP MIC errors, the security header could be any of:
 *             8  - if IV / KeyID and Extended IV are included
 *             16 - if MIC is also included
 *             20 - if ICV is also included
 *   - RX_ERR_CNT
 *     Bits 31:24
 *     Purpose: specifies how many rx errors are reported in this message
 *     Value:
 *         Rx error reports that include a L2 header and/or security header
 *         will set this field to 1, to indicate that the error notification
 *         is for a single frame.
 *         Rx error reports that don't include a L2 header or security header
 *         can use this field to send a single message to report multiple
 *         erroneous rx frames.
 */
typedef struct htt_isoc_t2h_rx_err_s {
    /* word 0 */
    A_UINT32
        msg_type:        8, /* HTT_ISOC_T2H_MSG_TYPE_RX_ERR */
        rx_err_type:     8,
        peer_id:        16;
    /* word 1 */
    A_UINT32
        ext_tid:         5,
        reserved1:       1,
        mcast:           1,
        l2_hdr_is_80211: 1,
        l2_hdr_bytes:    8,
        sec_hdr_bytes:   8,
        rx_err_cnt:      8;
    /* words 2 - M-1: L2 header */
    /* words M - N: security header */
} htt_isoc_t2h_rx_err_t;

/* word 0 */
#define HTT_ISOC_T2H_RX_ERR_TYPE_OFFSET32             0
#define HTT_ISOC_T2H_RX_ERR_TYPE_M                    0x0000ff00
#define HTT_ISOC_T2H_RX_ERR_TYPE_ID_S                 8

#define HTT_ISOC_T2H_RX_ERR_PEER_ID_OFFSET32          0
#define HTT_ISOC_T2H_RX_ERR_PEER_ID_M                 0xffff0000
#define HTT_ISOC_T2H_RX_ERR_PEER_ID_S                 16

/* word 1 */
#define HTT_ISOC_T2H_RX_ERR_EXT_TID_OFFSET32          1
#define HTT_ISOC_T2H_RX_ERR_EXT_TID_M                 0x0000001f
#define HTT_ISOC_T2H_RX_ERR_EXT_TID_ID_S              0

#define HTT_ISOC_T2H_RX_ERR_MCAST_OFFSET32            1
#define HTT_ISOC_T2H_RX_ERR_MCAST_M                   0x00000040
#define HTT_ISOC_T2H_RX_ERR_MCAST_ID_S                6

#define HTT_ISOC_T2H_RX_ERR_L2_HDR_IS_80211_OFFSET32  1
#define HTT_ISOC_T2H_RX_ERR_L2_HDR_IS_80211_M         0x00000080
#define HTT_ISOC_T2H_RX_ERR_L2_HDR_IS_80211_ID_S      7

#define HTT_ISOC_T2H_RX_L2_HDR_BYTES_OFFSET32         1
#define HTT_ISOC_T2H_RX_L2_HDR_BYTES_M                0x0000ff00
#define HTT_ISOC_T2H_RX_L2_HDR_BYTES_ID_S             8

#define HTT_ISOC_T2H_RX_SEC_HDR_BYTES_OFFSET32        1
#define HTT_ISOC_T2H_RX_SEC_HDR_BYTES_M               0x00ff0000
#define HTT_ISOC_T2H_RX_SEC_HDR_BYTES_ID_S            16

#define HTT_ISOC_T2H_RX_ERR_CNT_OFFSET32              1
#define HTT_ISOC_T2H_RX_ERR_CNT_M                     0xff000000
#define HTT_ISOC_T2H_RX_ERR_CNT_ID_S                  24


/* general field access macros */

#define HTT_ISOC_T2H_RX_ERR_FIELD_SET(field, msg_addr, value) \
    htt_isoc_t2h_field_set(                                       \
        ((A_UINT32 *) msg_addr),                                  \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _OFFSET32,           \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _M,                  \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _S,                  \
        value)

#define HTT_ISOC_T2H_RX_ERR_FIELD_GET(field, msg_addr) \
    HTT_ISOC_T2H_FIELD_GET(                                \
        ((A_UINT32 *) msg_addr),                           \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _OFFSET32,    \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _M,           \
        HTT_ISOC_T2H_RX_ERR_ ## field ## _S)

/* access macros for specific fields */

#define HTT_ISOC_T2H_RX_ERR_TYPE_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(TYPE, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_TYPE_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(TYPE, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_PEER_ID_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(PEER_ID, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_PEER_ID_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(PEER_ID, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_EXT_TID_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(EXT_TID, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_EXT_TID_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(EXT_TID, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_MCAST_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(MCAST, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_MCAST_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(MCAST, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_L2_HDR_IS_80211_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(L2_HDR_IS_80211, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_L2_HDR_IS_80211_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(L2_HDR_IS_80211, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_L2_HDR_BYTES_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(L2_HDR_BYTES, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_L2_HDR_BYTES_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(L2_HDR_BYTES, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_SEC_HDR_BYTES_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(SEC_HDR_BYTES, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_SEC_HDR_BYTES_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(SEC_HDR_BYTES, msg_addr)

#define HTT_ISOC_T2H_RX_ERR_CNT_SET(msg_addr, value) \
    HTT_ISOC_T2H_RX_ERR_FIELD_SET(CNT, msg_addr, value)
#define HTT_ISOC_T2H_RX_ERR_CNT_GET(msg_addr) \
    HTT_ISOC_T2H_RX_ERR_FIELD_GET(CNT, msg_addr)


#endif /* _HTT_ISOC_H_ */
