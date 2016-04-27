/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
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


#ifndef __WLAN_TGT_DEF_CONFIG_H__
#define __WLAN_TGT_DEF_CONFIG_H__

/*
 * TODO: please help to consider if we need a seperate config file from LL case.
 */

/*
 * set of default target config , that can be over written by platform
 */

#ifdef QCA_SUPPORT_INTEGRATED_SOC
#define CFG_TGT_NUM_VDEV                3 /*STA, P2P device, P2P GO/Cli*/
#else
/*
 * default limit of VAPs per device.
 */
#define CFG_TGT_NUM_VDEV                3
#endif
/*
 * We would need 1 AST entry per peer. Scale it by a factor of 2 to minimize
 * hash collisions.
 * TODO: This scaling factor would be taken care inside the WAL in the future.
 */
#define CFG_TGT_NUM_PEER_AST            2

/* # of WDS entries to support.
 */
#define CFG_TGT_WDS_ENTRIES             0

/* MAC DMA burst size. 0: 128B - default, 1: 256B, 2: 64B
 */
#define CFG_TGT_DEFAULT_DMA_BURST_SIZE   0

/* Fixed delimiters to be inserted after every MPDU
 */
#define CFG_TGT_DEFAULT_MAC_AGGR_DELIM   0

/*
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_AST_SKID_LIMIT          6
/*
 * total number of peers per device.
 * currently set to 8 to bring up IP3.9 for memory size problem
 */
#define CFG_TGT_NUM_PEERS               8
/*
 *  max number of peers per device.
 */
#define CFG_TGT_NUM_PEERS_MAX           8
/*
 * In offload mode target supports features like WOW, chatter and other
 * protocol offloads. In order to support them some functionalities like
 * reorder buffering, PN checking need to be done in target. This determines
 * maximum number of peers suported by target in offload mode
 */
#define CFG_TGT_NUM_OFFLOAD_PEERS       0
/*
 * Number of reorder buffers used in offload mode
 */
#define CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS   0
/*
 * keys per peer node
 */
#define CFG_TGT_NUM_PEER_KEYS           2
/*
 * total number of TX/RX data TIDs
 */
#define CFG_TGT_NUM_TIDS                (2 * (CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV))
/*
 * max number of Tx TIDS
 */
#define CFG_TGT_NUM_TIDS_MAX            (2 * (CFG_TGT_NUM_PEERS_MAX + CFG_TGT_NUM_VDEV))
/*
 * number of multicast keys.
 */
#define CFG_TGT_NUM_MCAST_KEYS          8
/*
 * A value of 3 would probably suffice - one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_PDEV_HANDLERS       8
/*
 * A value of 3 would probably suffice - one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_VDEV_HANDLERS       4
/*
 * set this to 8:
 *     one for WAL interals (connection pause)
 *     one for the control stack,
 *     one for the data stack
 *     and one for debugging
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_HANDLERS            14
/*
 * set this to 3: one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_PEER_HANDLERS       32
/*
 * set this to 0x7 (Peregrine = 3 chains).
 * need to be set dynamically based on the HW capability.
 * this is rome
 */
#define CFG_TGT_DEFAULT_TX_CHAIN_MASK   0x3
/*
 * set this to 0x7 (Peregrine = 3 chains).
 * need to be set dynamically based on the HW capability.
 * this is rome
 */
#define CFG_TGT_DEFAULT_RX_CHAIN_MASK   0x3
/* 100 ms for video, best-effort, and background */
#define CFG_TGT_RX_TIMEOUT_LO_PRI       100
/* 40 ms for voice*/
#define CFG_TGT_RX_TIMEOUT_HI_PRI       40

/* AR9888 unified is default in ethernet mode */
#define CFG_TGT_RX_DECAP_MODE (0x2)
/* Decap to native Wifi header */
#define CFG_TGT_RX_DECAP_MODE_NWIFI (0x1)
/* Decap to raw mode header */
#define CFG_TGT_RX_DECAP_MODE_RAW   (0x0)

/* maximum number of pending scan requests */
#define CFG_TGT_DEFAULT_SCAN_MAX_REQS   0x4

/* maximum number of scan event handlers */
#define CFG_TGT_DEFAULT_SCAN_MAX_HANDLERS   0x4

/* maximum number of VDEV that could use BMISS offload */
#define CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV   0x2

/* maximum number of VDEV offload Roaming to support */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV   0x2

/* maximum number of AP profiles pushed to offload Roaming */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES   0x8

/* maximum number of VDEV offload GTK to support */
#define CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV   0x2
/* default: mcast->ucast disabled */
#if 1
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 0
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 0
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 0 /* disabled */
#else
/* (for testing) small multicast group membership table enabled */
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 4
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 16
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 1
#endif

/*
 * Specify how much memory the target should allocate for a debug log of
 * tx PPDU meta-information (how large the PPDU was, when it was sent,
 * whether it was successful, etc.)
 * The size of the log records is configurable, from a minimum of 28 bytes
 * to a maximum of about 300 bytes.  A typical configuration would result
 * in each log record being about 124 bytes.
 * Thus, 1KB of log space can hold about 30 small records, 3 large records,
 * or about 8 typical-sized records.
 */
#define CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE 1024 /* bytes */

/* target based fragment timeout and MPDU duplicate detection */
#define CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK 0
/*  Default VoW configuration
 */
#define CFG_TGT_DEFAULT_VOW_CONFIG   0

/*
 * total number of descriptors to use in the target
 */
#ifndef HIF_SDIO
#define CFG_TGT_NUM_MSDU_DESC    (32)
#else
#define CFG_TGT_NUM_MSDU_DESC    (0)
#endif
/*
 * Maximum number of frag table entries
 */
#define CFG_TGT_MAX_FRAG_TABLE_ENTRIES 2

/*
 * number of vdevs that can support tdls
 */
#define CFG_TGT_NUM_TDLS_VDEVS    1

/*
 * number of peers that each Tdls vdev can track
 */
#define CFG_TGT_NUM_TDLS_CONN_TABLE_ENTRIES    32
/*
 * number of TDLS concurrent sleep STAs
 */
#define CFG_TGT_NUM_TDLS_CONC_SLEEP_STAS    1

/*
 * number of TDLS concurrent buffer STAs
 */
#define CFG_TGT_NUM_TDLS_CONC_BUFFER_STAS    1

#define CFG_TGT_MAX_MULTICAST_FILTER_ENTRIES 16
/*
 * Maximum number of VDEV that beacon tx offload will support
 */
#ifdef HIF_SDIO
#define CFG_TGT_DEFAULT_BEACON_TX_OFFLOAD_MAX_VDEV 2
#else
#define CFG_TGT_DEFAULT_BEACON_TX_OFFLOAD_MAX_VDEV 1
#endif

/*
 * ht enable highest MCS by default
 */
#define CFG_TGT_DEFAULT_GTX_HT_MASK     0x8080
/*
 * vht enable highest MCS by default
 */
#define CFG_TGT_DEFAULT_GTX_VHT_MASK        0x80200
/*
 * resv for furture use, bit 30 is used for fix tpc, bit0-3 for Power save
 * balance
 */
#define CFG_TGT_DEFAULT_GTX_USR_CFG     0xa
/*
 * threshold to enable GTX
 */
#define CFG_TGT_DEFAULT_GTX_PER_THRESHOLD   3
/*
 * margin to move back when per > margin + threshold
 */
#define CFG_TGT_DEFAULT_GTX_PER_MARGIN      2
/*
 * step for every move
 */
#define CFG_TGT_DEFAULT_GTX_TPC_STEP        1
/*
 * lowest TPC
 */
#define CFG_TGT_DEFAULT_GTX_TPC_MIN     0
/*
 * enable all BW 20/40/80/160
 */
#define CFG_TGT_DEFAULT_GTX_BW_MASK     0xf

/*
 * number of vdevs that can support OCB
 */
#define CFG_TGT_NUM_OCB_VDEVS			1

/*
 * maximum number of channels that can do OCB
 */
#define CFG_TGT_NUM_OCB_CHANNELS		2

/*
 * maximum number of channels in an OCB schedule
 */
#define CFG_TGT_NUM_OCB_SCHEDULES		2

#endif  /*__WLAN_TGT_DEF_CONFIG_H__ */
