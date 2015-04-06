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
 * @file ol_txrx_status.h
 * @brief Functions provided for visibility and debugging.
 * NOTE: This file is used by both kernel driver SW and userspace SW.
 * Thus, do not reference use any kernel header files or defs in this file!
 */
#ifndef _OL_TXRX_STATS__H_
#define _OL_TXRX_STATS__H_

#include <athdefs.h>       /* u_int64_t */

#define TXRX_STATS_LEVEL_OFF   0
#define TXRX_STATS_LEVEL_BASIC 1
#define TXRX_STATS_LEVEL_FULL  2

#ifndef TXRX_STATS_LEVEL
#define TXRX_STATS_LEVEL TXRX_STATS_LEVEL_BASIC
#endif

typedef struct {
   u_int64_t pkts;
   u_int64_t bytes;
} ol_txrx_stats_elem;

/**
 * @brief data stats published by the host txrx layer
 */
struct ol_txrx_stats {
    struct {
        /* MSDUs given to the txrx layer by the management stack */
        ol_txrx_stats_elem mgmt;
        /* MSDUs successfully sent across the WLAN */
        ol_txrx_stats_elem delivered;
        struct {
            /* MSDUs that the host did not accept */
            ol_txrx_stats_elem host_reject;
            /* MSDUs which could not be downloaded to the target */
            ol_txrx_stats_elem download_fail;
            /* MSDUs which the target discarded (lack of mem or old age) */
            ol_txrx_stats_elem target_discard;
            /* MSDUs which the target sent but couldn't get an ack for */
            ol_txrx_stats_elem no_ack;
        } dropped;
    } tx;
    struct {
        /* MSDUs given to the OS shim */
        ol_txrx_stats_elem delivered;
        /* MSDUs forwarded from the rx path to the tx path */
        ol_txrx_stats_elem forwarded;
    } rx;
};

/*
 * Structure to consolidate host stats
 */
struct ieee80211req_ol_ath_host_stats {
    struct ol_txrx_stats txrx_stats;
    struct {
        int pkt_q_fail_count;
        int pkt_q_empty_count;
        int send_q_empty_count;
    } htc;
    struct {
        int pipe_no_resrc_count;
        int ce_ring_delta_fail_count;
    } hif;
};

#endif /* _OL_TXRX_STATS__H_ */
