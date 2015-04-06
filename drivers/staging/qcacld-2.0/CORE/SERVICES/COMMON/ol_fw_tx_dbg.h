/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
 * @file ol_fw_tx_dbg.h
 *
 * @details data structs used for uploading summary info about the FW's tx
 */

#ifndef _OL_FW_TX_DBG__H_
#define _OL_FW_TX_DBG__H_

/*
 * Undef ATH_SUPPORT_FW_TX_DBG to remove the FW tx debug feature.
 * Removing the FW tx debug feature saves a modest amount of program memory.
 * The data memory allocation for the FW tx debug feature is controlled
 * by the host --> target resource configuration parameters; even if
 * ATH_SUPPORT_FW_TX_DBG is defined, no data memory will be allocated for
 * the FW tx debug log unless the host --> target resource configuration
 * specifies it.
 */
#define ATH_SUPPORT_FW_TX_DBG 1 /* enabled */
//#undef ATH_SUPPORT_FW_TX_DBG /* disabled */


#if defined(ATH_TARGET)
#include <osapi.h>      /* A_UINT32 */
#else
#include <a_types.h>    /* A_UINT32 */
#include <osdep.h>    /* PREPACK, POSTPACK */
#endif

enum ol_fw_tx_dbg_log_mode {
   ol_fw_tx_dbg_log_mode_wraparound, /* overwrite old data with new */
   ol_fw_tx_dbg_log_mode_single,     /* fill log once, then stop */
};

/*
 * tx PPDU stats upload message header
 */
struct ol_fw_tx_dbg_ppdu_msg_hdr {
    /* word 0 */
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_BYTES_WORD  0
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_BYTES_S     0
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_BYTES_M     0x000000ff
    A_UINT8  mpdu_bytes_array_len; /* length of array of per-MPDU byte counts */

    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MSDU_BYTES_WORD  0
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MSDU_BYTES_S     8
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MSDU_BYTES_M     0x0000ff00
    A_UINT8  msdu_bytes_array_len; /* length of array of per-MSDU byte counts */

    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_MSDUS_WORD  0
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_MSDUS_S     16
    #define OL_FW_TX_DBG_PPDU_HDR_NUM_MPDU_MSDUS_M     0x00ff0000
    A_UINT8  mpdu_msdus_array_len; /* length of array of per-MPDU MSDU counts */

    A_UINT8 reserved;

    /* word 1 */
    #define OL_FW_TX_DBG_PPDU_HDR_MICROSEC_PER_TICK_WORD  1
    #define OL_FW_TX_DBG_PPDU_HDR_MICROSEC_PER_TICK_S     0
    #define OL_FW_TX_DBG_PPDU_HDR_MICROSEC_PER_TICK_M     0xffffffff
    A_UINT32 microsec_per_tick; /* conversion for timestamp entries */
};

/*
 * tx PPDU log element / stats upload message element
 */
struct ol_fw_tx_dbg_ppdu_base {
    /* word 0 - filled in during tx enqueue */
    #define OL_FW_TX_DBG_PPDU_START_SEQ_NUM_WORD  0
    #define OL_FW_TX_DBG_PPDU_START_SEQ_NUM_S     0
    #define OL_FW_TX_DBG_PPDU_START_SEQ_NUM_M     0x0000ffff
    A_UINT16 start_seq_num;
    #define OL_FW_TX_DBG_PPDU_START_PN_LSBS_WORD  0
    #define OL_FW_TX_DBG_PPDU_START_PN_LSBS_S     16
    #define OL_FW_TX_DBG_PPDU_START_PN_LSBS_M     0xffff0000
    A_UINT16 start_pn_lsbs;

    /* word 1 - filled in during tx enqueue */
    #define OL_FW_TX_DBG_PPDU_NUM_BYTES_WORD      1
    #define OL_FW_TX_DBG_PPDU_NUM_BYTES_S         0
    #define OL_FW_TX_DBG_PPDU_NUM_BYTES_M         0xffffffff
    A_UINT32 num_bytes;

    /* word 2 - filled in during tx enqueue */
    #define OL_FW_TX_DBG_PPDU_NUM_MSDUS_WORD      2
    #define OL_FW_TX_DBG_PPDU_NUM_MSDUS_S         0
    #define OL_FW_TX_DBG_PPDU_NUM_MSDUS_M         0x000000ff
    A_UINT8  num_msdus;
    #define OL_FW_TX_DBG_PPDU_NUM_MPDUS_WORD      2
    #define OL_FW_TX_DBG_PPDU_NUM_MPDUS_S         8
    #define OL_FW_TX_DBG_PPDU_NUM_MPDUS_M         0x0000ff00
    A_UINT8  num_mpdus;
    A_UINT16
    #define OL_FW_TX_DBG_PPDU_EXT_TID_WORD        2
    #define OL_FW_TX_DBG_PPDU_EXT_TID_S           16
    #define OL_FW_TX_DBG_PPDU_EXT_TID_M           0x001f0000
        ext_tid :  5,
    #define OL_FW_TX_DBG_PPDU_PEER_ID_WORD        2
    #define OL_FW_TX_DBG_PPDU_PEER_ID_S           21
    #define OL_FW_TX_DBG_PPDU_PEER_ID_M           0xffe00000
        peer_id : 11;

    /* word 3 - filled in during tx enqueue */
    #define OL_FW_TX_DBG_PPDU_TIME_ENQUEUE_WORD   3
    #define OL_FW_TX_DBG_PPDU_TIME_ENQUEUE_S      0
    #define OL_FW_TX_DBG_PPDU_TIME_ENQUEUE_M      0xffffffff
    A_UINT32 timestamp_enqueue;

    /* word 4 - filled in during tx completion */
    #define OL_FW_TX_DBG_PPDU_TIME_COMPL_WORD     4
    #define OL_FW_TX_DBG_PPDU_TIME_COMPL_S        0
    #define OL_FW_TX_DBG_PPDU_TIME_COMPL_M        0xffffffff
    A_UINT32 timestamp_completion;

    /* word 5 - filled in during tx completion */
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_LSBS_WORD 5
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_LSBS_S    0
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_LSBS_M    0xffffffff
    A_UINT32 block_ack_bitmap_lsbs;

    /* word 6 - filled in during tx completion */
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_MSBS_WORD 6
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_MSBS_S    0
    #define OL_FW_TX_DBG_PPDU_BLOCK_ACK_MSBS_M    0xffffffff
    A_UINT32 block_ack_bitmap_msbs;

    /* word 7 - filled in during tx completion (enqueue would work too) */
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_LSBS_WORD  7
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_LSBS_S     0
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_LSBS_M     0xffffffff
    A_UINT32 enqueued_bitmap_lsbs;

    /* word 8 - filled in during tx completion (enqueue would work too) */
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_MSBS_WORD  8
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_MSBS_S     0
    #define OL_FW_TX_DBG_PPDU_ENQUEUED_MSBS_M     0xffffffff
    A_UINT32 enqueued_bitmap_msbs;

    /* word 9 - filled in during tx completion */
    #define OL_FW_TX_DBG_PPDU_RATE_CODE_WORD      9
    #define OL_FW_TX_DBG_PPDU_RATE_CODE_S         0
    #define OL_FW_TX_DBG_PPDU_RATE_CODE_M         0x000000ff
    A_UINT8 rate_code;
    #define OL_FW_TX_DBG_PPDU_RATE_FLAGS_WORD     9
    #define OL_FW_TX_DBG_PPDU_RATE_FLAGS_S        8
    #define OL_FW_TX_DBG_PPDU_RATE_FLAGS_M        0x0000ff00
    A_UINT8 rate_flags; /* includes dynamic bandwidth info */
    #define OL_FW_TX_DBG_PPDU_TRIES_WORD          9
    #define OL_FW_TX_DBG_PPDU_TRIES_S             16
    #define OL_FW_TX_DBG_PPDU_TRIES_M             0x00ff0000
    A_UINT8 tries;
    #define OL_FW_TX_DBG_PPDU_COMPLETE_WORD       9
    #define OL_FW_TX_DBG_PPDU_COMPLETE_S          24
    #define OL_FW_TX_DBG_PPDU_COMPLETE_M          0xff000000
    A_UINT8 complete;
};


#endif /* _OL_FW_TX_DBG__H_ */
