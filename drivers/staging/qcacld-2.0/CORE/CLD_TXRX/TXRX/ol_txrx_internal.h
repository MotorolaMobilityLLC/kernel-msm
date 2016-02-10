/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

#ifndef _OL_TXRX_INTERNAL__H_
#define _OL_TXRX_INTERNAL__H_

#include <adf_os_util.h>   /* adf_os_assert */
#include <adf_nbuf.h>      /* adf_nbuf_t */
#include <adf_os_mem.h>    /* adf_os_mem_set */
#include <ieee80211_common.h>     /* ieee80211_frame */
#include <ol_htt_rx_api.h> /* htt_rx_msdu_desc_completes_mpdu, etc. */

#include <ol_txrx_types.h>

#include <ol_txrx_dbg.h>
#include <enet.h>          /* ETHERNET_HDR_LEN, etc. */
#include <ipv4.h>          /* IPV4_HDR_LEN, etc. */
#include <ipv6.h>          /* IPV6_HDR_LEN, etc. */
#include <ip_prot.h>       /* IP_PROTOCOL_TCP, etc. */

#ifdef ATH_11AC_TXCOMPACT
#define OL_TX_DESC_NO_REFS(tx_desc) 1
#define OL_TX_DESC_REF_INIT(tx_desc) /* no-op */
#define OL_TX_DESC_REF_INC(tx_desc) /* no-op */
#else
#define OL_TX_DESC_NO_REFS(tx_desc) \
    adf_os_atomic_dec_and_test(&tx_desc->ref_cnt)
#define OL_TX_DESC_REF_INIT(tx_desc) adf_os_atomic_init(&tx_desc->ref_cnt)
#define OL_TX_DESC_REF_INC(tx_desc) adf_os_atomic_inc(&tx_desc->ref_cnt)
#endif

#ifndef ARRAY_LEN
#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))
#endif


#ifndef TXRX_ASSERT_LEVEL
#define TXRX_ASSERT_LEVEL 3
#endif

#ifdef __KLOCWORK__
# define TXRX_ASSERT1(x) do { if (!(x)) abort(); } while (0)
# define TXRX_ASSERT2(x) do { if (!(x)) abort(); } while (0)
#else // #ifdef __KLOCWORK__

#if TXRX_ASSERT_LEVEL > 0
#define TXRX_ASSERT1(condition) adf_os_assert((condition))
#else
#define TXRX_ASSERT1(condition)
#endif

#if TXRX_ASSERT_LEVEL > 1
#define TXRX_ASSERT2(condition) adf_os_assert((condition))
#else
#define TXRX_ASSERT2(condition)
#endif
#endif // #ifdef __KLOCWORK__
enum {
    /* FATAL_ERR - print only irrecoverable error messages */
    TXRX_PRINT_LEVEL_FATAL_ERR,

    /* ERR - include non-fatal err messages */
    TXRX_PRINT_LEVEL_ERR,

    /* WARN - include warnings */
    TXRX_PRINT_LEVEL_WARN,

    /* INFO1 - include fundamental, infrequent events */
    TXRX_PRINT_LEVEL_INFO1,

    /* INFO2 - include non-fundamental but infrequent events */
    TXRX_PRINT_LEVEL_INFO2,

    /* INFO3 - include frequent events */
    /* to avoid performance impact, don't use INFO3 unless explicitly enabled */
    #ifdef TXRX_PRINT_VERBOSE_ENABLE
    TXRX_PRINT_LEVEL_INFO3,
    #endif /* TXRX_PRINT_VERBOSE_ENABLE */
};

extern unsigned g_txrx_print_level;

#ifdef TXRX_PRINT_ENABLE

#include <stdarg.h>       /* va_list */
#include <adf_os_types.h> /* adf_os_vprint */

/* Supress 4296 - expression is always true
 * It will fire if level is TXRX_PRINT_LEVEL_FATAL_ERR (0)
 * because g_txrx_print_level is unsigned */
#define ol_txrx_print(level, fmt, ...) \
    if (level <= g_txrx_print_level) adf_os_print(fmt, ## __VA_ARGS__)
#define TXRX_PRINT(level, fmt, ...) \
    ol_txrx_print(level, "TXRX: " fmt, ## __VA_ARGS__)

#ifdef TXRX_PRINT_VERBOSE_ENABLE

#define ol_txrx_print_verbose(fmt, ...) \
    if (TXRX_PRINT_LEVEL_INFO3 <= g_txrx_print_level) \
        adf_os_print(fmt, ## __VA_ARGS__)
#define TXRX_PRINT_VERBOSE(fmt, ...) \
    ol_txrx_print_verbose("TXRX: " fmt, ## __VA_ARGS__)
#else
#define TXRX_PRINT_VERBOSE(fmt, ...)
#endif /* TXRX_PRINT_VERBOSE_ENABLE */

/* define PN check failure message print rate
   as 1 second */
#define TXRX_PN_CHECK_FAILURE_PRINT_PERIOD_MS  1000

#else
#define TXRX_PRINT(level, fmt, ...)
#define TXRX_PRINT_VERBOSE(fmt, ...)
#endif /* TXRX_PRINT_ENABLE */

/*--- tx credit debug printouts ---*/

#ifndef DEBUG_CREDIT
#define DEBUG_CREDIT 0
#endif

#if DEBUG_CREDIT
#define TX_CREDIT_DEBUG_PRINT(fmt, ...) adf_os_print(fmt, ## __VA_ARGS__)
#else
#define TX_CREDIT_DEBUG_PRINT(fmt, ...)
#endif

/*--- tx scheduler debug printouts ---*/

#ifdef HOST_TX_SCHED_DEBUG
#define TX_SCHED_DEBUG_PRINT(fmt, ...) adf_os_print(fmt, ## __VA_ARGS__)
#else
#define TX_SCHED_DEBUG_PRINT(fmt, ...)
#endif
#define TX_SCHED_DEBUG_PRINT_ALWAYS(fmt, ...) adf_os_print(fmt, ## __VA_ARGS__)

#define OL_TXRX_LIST_APPEND(head, tail, elem) \
do {                                            \
    if (!(head)) {                              \
        (head) = (elem);                        \
    } else {                                    \
        adf_nbuf_set_next((tail), (elem));      \
    }                                           \
    (tail) = (elem);                            \
} while (0)

static inline void
ol_rx_mpdu_list_next(
    struct ol_txrx_pdev_t *pdev,
    void *mpdu_list,
    adf_nbuf_t *mpdu_tail,
    adf_nbuf_t *next_mpdu)
{
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    adf_nbuf_t msdu;

    /*
     * For now, we use a simply flat list of MSDUs.
     * So, traverse the list until we reach the last MSDU within the MPDU.
     */
    TXRX_ASSERT2(mpdu_list);
    msdu = mpdu_list;
    while (!htt_rx_msdu_desc_completes_mpdu(
                htt_pdev, htt_rx_msdu_desc_retrieve(htt_pdev, msdu)))
    {
        msdu = adf_nbuf_next(msdu);
        TXRX_ASSERT2(msdu);
    }
    /* msdu now points to the last MSDU within the first MPDU */
    *mpdu_tail = msdu;
    *next_mpdu = adf_nbuf_next(msdu);
}


/*--- txrx stats macros ---*/


/* unconditional defs */
#define TXRX_STATS_INCR(pdev, field) TXRX_STATS_ADD(pdev, field, 1)

/* default conditional defs (may be undefed below) */

#define TXRX_STATS_INIT(_pdev) \
    adf_os_mem_set(&((_pdev)->stats), 0x0, sizeof((_pdev)->stats))
#define TXRX_STATS_ADD(_pdev, _field, _delta) \
    _pdev->stats._field += _delta
#define TXRX_STATS_MSDU_INCR(pdev, field, netbuf) \
    do { \
        TXRX_STATS_INCR((pdev), pub.field.pkts); \
        TXRX_STATS_ADD((pdev), pub.field.bytes, adf_nbuf_len(netbuf)); \
    } while (0)

/* conditional defs based on verbosity level */

#if /*---*/ TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_FULL

#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list) \
    do { \
        adf_nbuf_t tmp_list = netbuf_list; \
        while (tmp_list) { \
            TXRX_STATS_MSDU_INCR(pdev, field, tmp_list); \
            tmp_list = adf_nbuf_next(tmp_list); \
        } \
    } while (0)

#define TXRX_STATS_MSDU_INCR_TX_STATUS(status, pdev, netbuf) \
    switch (status) { \
    case htt_tx_status_ok: \
        TXRX_STATS_MSDU_INCR(pdev, tx.delivered, netbuf); \
        break; \
    case htt_tx_status_discard: \
        TXRX_STATS_MSDU_INCR(pdev, tx.dropped.target_discard, netbuf); \
        break; \
    case htt_tx_status_no_ack: \
        TXRX_STATS_MSDU_INCR(pdev, tx.dropped.no_ack, netbuf); \
        break; \
    case htt_tx_status_download_fail: \
        TXRX_STATS_MSDU_INCR(pdev, tx.dropped.download_fail, netbuf); \
        break; \
    default: \
        break; \
    }

#define TXRX_STATS_UPDATE_TX_COMP_HISTOGRAM(_pdev, _p_cntrs)                   \
do {                                                                           \
    if (_p_cntrs > 60)                                                         \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_61_plus,1);            \
    else if (_p_cntrs > 50)                                                    \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_51_60,1);              \
    else if (_p_cntrs > 40)                                                    \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_41_50,1);              \
    else if (_p_cntrs > 30)                                                    \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_31_40,1);              \
    else if (_p_cntrs > 20)                                                    \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_21_30,1);              \
    else if (_p_cntrs > 10)                                                    \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_11_20,1);              \
    else if (_p_cntrs > 2)                                                     \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_2_10,1);               \
    else                                                                       \
        TXRX_STATS_ADD(_pdev,pub.tx.comp_histogram.pkts_1,1);                  \
                                                                               \
} while (0)


#define TXRX_STATS_UPDATE_TX_STATS(_pdev, _status, _p_cntrs, _b_cntrs)         \
do {                                                                           \
    switch (status) {                                                          \
    case htt_tx_status_ok:                                                     \
       TXRX_STATS_ADD(_pdev, pub.tx.delivered.pkts, _p_cntrs);                 \
       TXRX_STATS_ADD(_pdev, pub.tx.delivered.bytes, _b_cntrs);                \
        break;                                                                 \
    case htt_tx_status_discard:                                                \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.target_discard.pkts, _p_cntrs);    \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.target_discard.bytes, _b_cntrs);   \
        break;                                                                 \
    case htt_tx_status_no_ack:                                                 \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.no_ack.pkts, _p_cntrs);            \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.no_ack.bytes, _b_cntrs);           \
        break;                                                                 \
    case htt_tx_status_download_fail:                                          \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.download_fail.pkts, _p_cntrs);     \
       TXRX_STATS_ADD(_pdev, pub.tx.dropped.download_fail.bytes, _b_cntrs);    \
        break;                                                                 \
    default:                                                                   \
        break;                                                                 \
    }                                                                          \
    TXRX_STATS_UPDATE_TX_COMP_HISTOGRAM(_pdev, _p_cntrs);                      \
} while (0)

#elif /*---*/ TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_BASIC

#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list) \
    do { \
        adf_nbuf_t tmp_list = netbuf_list; \
        while (tmp_list) { \
            TXRX_STATS_MSDU_INCR(pdev, field, tmp_list); \
            tmp_list = adf_nbuf_next(tmp_list); \
        } \
    } while (0)

#define TXRX_STATS_MSDU_INCR_TX_STATUS(status, pdev, netbuf) \
    do { \
        if (status == htt_tx_status_ok) { \
            TXRX_STATS_MSDU_INCR(pdev, tx.delivered, netbuf); \
        } \
    } while (0)

#define TXRX_STATS_INIT(_pdev) \
    adf_os_mem_set(&((_pdev)->stats), 0x0, sizeof((_pdev)->stats))

#define TXRX_STATS_UPDATE_TX_STATS(_pdev, _status, _p_cntrs, _b_cntrs)          \
do {                                                                            \
    if (adf_os_likely(_status == htt_tx_status_ok)) {                           \
       TXRX_STATS_ADD(_pdev, pub.tx.delivered.pkts, _p_cntrs);                  \
       TXRX_STATS_ADD(_pdev, pub.tx.delivered.bytes, _b_cntrs);                 \
    }                                                                           \
} while (0)

#else /*---*/ /* stats off */

#undef  TXRX_STATS_INIT
#define TXRX_STATS_INIT(_pdev)

#undef  TXRX_STATS_ADD
#define TXRX_STATS_ADD(_pdev, _field, _delta)

#undef  TXRX_STATS_MSDU_INCR
#define TXRX_STATS_MSDU_INCR(pdev, field, netbuf)

#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list)

#define TXRX_STATS_MSDU_INCR_TX_STATUS(status, pdev, netbuf)

#define TXRX_STATS_UPDATE_TX_STATS(_pdev, _status, _p_cntrs, _b_cntrs)

#endif /*---*/ /* TXRX_STATS_LEVEL */


/*--- txrx sequence number trace macros ---*/


#define TXRX_SEQ_NUM_ERR(_status) (0xffff - _status)

#if defined(ENABLE_RX_REORDER_TRACE)

A_STATUS ol_rx_reorder_trace_attach(ol_txrx_pdev_handle pdev);
void ol_rx_reorder_trace_detach(ol_txrx_pdev_handle pdev);
void ol_rx_reorder_trace_add(
    ol_txrx_pdev_handle pdev,
    u_int8_t tid,
    u_int16_t reorder_idx,
    u_int16_t seq_num,
    int num_mpdus);

#define OL_RX_REORDER_TRACE_ATTACH ol_rx_reorder_trace_attach
#define OL_RX_REORDER_TRACE_DETACH ol_rx_reorder_trace_detach
#define OL_RX_REORDER_TRACE_ADD    ol_rx_reorder_trace_add

#else

#define OL_RX_REORDER_TRACE_ATTACH(_pdev) A_OK
#define OL_RX_REORDER_TRACE_DETACH(_pdev)
#define OL_RX_REORDER_TRACE_ADD(pdev, tid, reorder_idx, seq_num, num_mpdus)

#endif /* ENABLE_RX_REORDER_TRACE */


/*--- txrx packet number trace macros ---*/


#if defined(ENABLE_RX_PN_TRACE)

A_STATUS ol_rx_pn_trace_attach(ol_txrx_pdev_handle pdev);
void ol_rx_pn_trace_detach(ol_txrx_pdev_handle pdev);
void ol_rx_pn_trace_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    u_int16_t tid,
    void *rx_desc);

#define OL_RX_PN_TRACE_ATTACH ol_rx_pn_trace_attach
#define OL_RX_PN_TRACE_DETACH ol_rx_pn_trace_detach
#define OL_RX_PN_TRACE_ADD    ol_rx_pn_trace_add

#else

#define OL_RX_PN_TRACE_ATTACH(_pdev) A_OK
#define OL_RX_PN_TRACE_DETACH(_pdev)
#define OL_RX_PN_TRACE_ADD(pdev, peer, tid, rx_desc)

#endif /* ENABLE_RX_PN_TRACE */

static inline int
ol_txrx_ieee80211_hdrsize(const void *data)
{
    const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;
    int size = sizeof(struct ieee80211_frame);

    /* NB: we don't handle control frames */
    TXRX_ASSERT1(
        (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL);
    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
        size += IEEE80211_ADDR_LEN;
    }
    if (IEEE80211_QOS_HAS_SEQ(wh)) {
        size += sizeof(u_int16_t);
        /* Qos frame with Order bit set indicates an HTC frame */
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            size += sizeof(struct ieee80211_htc);
        }
    }
    return size;
}

/*--- frame display utility ---*/

enum ol_txrx_frm_dump_options {
    ol_txrx_frm_dump_contents = 0x1,
    ol_txrx_frm_dump_tcp_seq  = 0x2,
};

#ifdef TXRX_DEBUG_DATA
static inline void
OL_TXRX_FRMS_DUMP(
    const char *name,
    struct ol_txrx_pdev_t *pdev,
    adf_nbuf_t frm,
    enum ol_txrx_frm_dump_options display_options,
    int max_len)
{
    #define TXRX_FRM_DUMP_MAX_LEN 128
    u_int8_t local_buf[TXRX_FRM_DUMP_MAX_LEN] = {0};
    u_int8_t *p;

    if (name) {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO, "%s\n", name);
    }
    while (frm) {
        p = adf_nbuf_data(frm);
        if (display_options & ol_txrx_frm_dump_tcp_seq) {
            int tcp_offset;
            int l2_hdr_size;
            u_int16_t ethertype;
            u_int8_t ip_prot;

            if (pdev->frame_format == wlan_frm_fmt_802_3) {
                struct ethernet_hdr_t *enet_hdr = (struct ethernet_hdr_t *) p;
                l2_hdr_size = ETHERNET_HDR_LEN;

                /*
                 * LLC/SNAP present?
                 */
                ethertype =
                    (enet_hdr->ethertype[0] << 8) | enet_hdr->ethertype[1];
                if (!IS_ETHERTYPE(ethertype)) {
                    /* 802.3 format */
                    struct llc_snap_hdr_t *llc_hdr;

                    llc_hdr = (struct llc_snap_hdr_t *) (p + l2_hdr_size);
                    l2_hdr_size += LLC_SNAP_HDR_LEN;
                    ethertype =
                        (llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
                }
            } else {
                struct llc_snap_hdr_t *llc_hdr;
                /* (generic?) 802.11 */
                l2_hdr_size = sizeof(struct ieee80211_frame);
                llc_hdr = (struct llc_snap_hdr_t *) (p + l2_hdr_size);
                l2_hdr_size += LLC_SNAP_HDR_LEN;
                ethertype =
                    (llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
            }
            if (ethertype == ETHERTYPE_IPV4) {
                struct ipv4_hdr_t *ipv4_hdr;
                ipv4_hdr = (struct ipv4_hdr_t *) (p + l2_hdr_size);
                ip_prot = ipv4_hdr->protocol;
                tcp_offset = l2_hdr_size + IPV4_HDR_LEN;
            } else if (ethertype == ETHERTYPE_IPV6) {
                struct ipv6_hdr_t *ipv6_hdr;
                ipv6_hdr = (struct ipv6_hdr_t *) (p + l2_hdr_size);
                ip_prot = ipv6_hdr->next_hdr;
                tcp_offset = l2_hdr_size + IPV6_HDR_LEN;
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "frame %p non-IP ethertype (%x)\n", frm, ethertype);
                goto NOT_IP_TCP;
            }
            if (ip_prot == IP_PROTOCOL_TCP) {
#if 0
                struct tcp_hdr_t *tcp_hdr;
                u_int32_t tcp_seq_num;
                tcp_hdr = (struct tcp_hdr_t *) (p + tcp_offset);
                tcp_seq_num =
                    (tcp_hdr->seq_num[0] << 24) |
                    (tcp_hdr->seq_num[1] << 16) |
                    (tcp_hdr->seq_num[1] <<  8) |
                    (tcp_hdr->seq_num[1] <<  0);
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "frame %p: TCP seq num = %d\n", frm, tcp_seq_num);
#else
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                     "frame %p: TCP seq num = %d\n", frm,
                    ((*(p + tcp_offset + 4)) << 24) |
                    ((*(p + tcp_offset + 5)) << 16) |
                    ((*(p + tcp_offset + 6)) <<  8) |
                     (*(p + tcp_offset + 7)));
#endif
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "frame %p non-TCP IP protocol (%x)\n", frm, ip_prot);
            }
        }
NOT_IP_TCP:
        if (display_options & ol_txrx_frm_dump_contents) {
            int i, frag_num, len_lim;
            len_lim = max_len;
            if (len_lim > adf_nbuf_len(frm)) {
                len_lim = adf_nbuf_len(frm);
            }
            if (len_lim > TXRX_FRM_DUMP_MAX_LEN) {
                len_lim = TXRX_FRM_DUMP_MAX_LEN;
            }

            /*
             * Gather frame contents from netbuf fragments
             * into a contiguous buffer.
             */
            frag_num = 0;
            i = 0;
            while (i < len_lim) {
                int frag_bytes;
                frag_bytes = adf_nbuf_get_frag_len(frm, frag_num);
                if (frag_bytes > len_lim - i) {
                    frag_bytes = len_lim - i;
                }
                if (frag_bytes > 0) {
                    p = adf_nbuf_get_frag_vaddr(frm, frag_num);
                    adf_os_mem_copy(&local_buf[i], p, frag_bytes);
                }
                frag_num++;
                i += frag_bytes;
            }

            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                "frame %p data (%p), hex dump of bytes 0-%d of %d:\n",
                frm, p, len_lim-1, (int) adf_nbuf_len(frm));
            p = local_buf;
            while (len_lim > 16) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  " /* indent */
                    "%02x %02x %02x %02x %02x %02x %02x %02x "
                    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                    *(p +  0), *(p +  1), *(p +  2), *(p +  3),
                    *(p +  4), *(p +  5), *(p +  6), *(p +  7),
                    *(p +  8), *(p +  9), *(p + 10), *(p + 11),
                    *(p + 12), *(p + 13), *(p + 14), *(p + 15));
                p += 16;
                len_lim -= 16;
            }
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                "  " /* indent */);
            while (len_lim > 0) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "%02x ", *p);
                p++;
                len_lim--;
            }
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO, "\n");
        }
        frm = adf_nbuf_next(frm);
    }
}
#else
#define OL_TXRX_FRMS_DUMP(name, pdev, frms, display_options, max_len)
#endif /* TXRX_DEBUG_DATA */

#ifdef SUPPORT_HOST_STATISTICS

#define OL_RX_ERR_STATISTICS(pdev, vdev, err_type, sec_type, is_mcast) \
    ol_rx_err_statistics(pdev->ctrl_pdev, vdev->vdev_id, err_type,     \
			             sec_type, is_mcast);

#define OL_RX_ERR_STATISTICS_1(pdev, vdev, peer, rx_desc, err_type)                  \
do {                                                                                    \
    int is_mcast;                                                                    \
    enum htt_sec_type sec_type;                                                      \
    is_mcast = htt_rx_msdu_is_wlan_mcast(pdev->htt_pdev, rx_desc);                   \
    sec_type = peer->security[is_mcast ? txrx_sec_mcast : txrx_sec_ucast].sec_type;  \
	OL_RX_ERR_STATISTICS(pdev, vdev, err_type, pdev->sec_types[sec_type], is_mcast); \
} while (false)

#define OL_RX_ERR_INV_PEER_STATISTICS(pdev, rx_msdu)                             \
do {                                                                                \
	struct ieee80211_frame *wh = NULL;                                           \
	/*FIX THIS :Here htt_rx_mpdu_wifi_hdr_retrieve should be used. But at */     \
    /*present it seems it does not work.*/                                       \
	/*wh = (struct ieee80211_frame *)htt_rx_mpdu_wifi_hdr_retrieve(pdev->htt_pdev, rx_desc);*/ \
	                                                                             \
	/* this only apply to LL device.*/                                           \
	if (!ol_cfg_is_high_latency(pdev->ctrl_pdev) &&                              \
			ol_cfg_frame_type(pdev->ctrl_pdev) == wlan_frm_fmt_native_wifi) {    \
		/* For windows, it is always native wifi header .*/                      \
		wh = (struct ieee80211_frame*)adf_nbuf_data(rx_msdu);                    \
	}                                                                            \
	ol_rx_err_inv_peer_statistics(pdev->ctrl_pdev, wh, OL_RX_ERR_UNKNOWN_PEER);  \
} while (false)

#define OL_RX_ERR_STATISTICS_2(pdev, vdev, peer, rx_desc, rx_msdu, rx_status)                 \
do {                                                                                    \
	enum ol_rx_err_type err_type = OL_RX_ERR_NONE;                                   \
    switch (rx_status) {                                                             \
		case htt_rx_status_decrypt_err:                                              \
			 err_type = OL_RX_ERR_DECRYPT;                                           \
			 break;                                                                  \
		case htt_rx_status_tkip_mic_err:                                             \
			 err_type = OL_RX_ERR_TKIP_MIC;                                          \
			 break;                                                                  \
		case htt_rx_status_mpdu_length_err:                                          \
		     err_type = OL_RX_ERR_MPDU_LENGTH;                                       \
			 break;                                                                  \
		case htt_rx_status_mpdu_encrypt_required_err:                                \
		     err_type = OL_RX_ERR_ENCRYPT_REQUIRED;                                  \
			 break;                                                                  \
		case htt_rx_status_err_dup:                                                  \
		     err_type = OL_RX_ERR_DUP;                                               \
			 break;                                                                  \
		case htt_rx_status_err_fcs:                                                  \
		     err_type = OL_RX_ERR_FCS;                                               \
			 break;                                                                  \
		default:                                                                     \
			 err_type = OL_RX_ERR_UNKNOWN;                                           \
		     break;                                                                  \
	}                                                                                \
	if (vdev != NULL && peer != NULL) {                                              \
	    OL_RX_ERR_STATISTICS_1(pdev, vdev, peer, rx_mpdu_desc, err_type);            \
	} else {                                                                         \
		OL_RX_ERR_INV_PEER_STATISTICS(pdev, rx_msdu);                                \
	}                                                                                \
} while (false)
#else
#define OL_RX_ERR_STATISTICS(pdev, vdev, err_type, sec_type, is_mcast)
#define OL_RX_ERR_STATISTICS_1(pdev, vdev, peer, rx_desc, err_type)
#define OL_RX_ERR_STATISTICS_2(pdev, vdev, peer, rx_desc, rx_msdu, rx_status)
#endif /* SUPPORT_HOST_STATISTICS */

#ifdef QCA_ENABLE_OL_TXRX_PEER_STATS
#define OL_TXRX_PEER_STATS_UPDATE_BASE(peer, tx_or_rx, type, msdu) \
    do { \
        adf_os_spin_lock_bh(&peer->vdev->pdev->peer_stat_mutex); \
        peer->stats. tx_or_rx .frms.  type += 1; \
        peer->stats. tx_or_rx .bytes. type += adf_nbuf_len(msdu); \
        adf_os_spin_unlock_bh(&peer->vdev->pdev->peer_stat_mutex); \
    } while (0)
#define OL_TXRX_PEER_STATS_UPDATE(peer, tx_or_rx, msdu) \
    do { \
        struct ol_txrx_vdev_t *vdev = peer->vdev; \
        struct ol_txrx_pdev_t *pdev = vdev->pdev; \
        u_int8_t *dest_addr; \
        if (pdev->frame_format == wlan_frm_fmt_802_3) { \
            dest_addr = adf_nbuf_data(msdu); \
        } else { /* 802.11 format */ \
            struct ieee80211_frame *frm; \
            frm = (struct ieee80211_frame *) adf_nbuf_data(msdu); \
            if (vdev->opmode == wlan_op_mode_ap) { \
                dest_addr = (u_int8_t *) &(frm->i_addr1[0]); \
            } else { \
                dest_addr = (u_int8_t *) &(frm->i_addr3[0]); \
            } \
        } \
        if (adf_os_unlikely(IEEE80211_IS_BROADCAST(dest_addr))) { \
            OL_TXRX_PEER_STATS_UPDATE_BASE(peer, tx_or_rx, bcast, msdu); \
        } else if (adf_os_unlikely(IEEE80211_IS_MULTICAST(dest_addr))) { \
            OL_TXRX_PEER_STATS_UPDATE_BASE(peer, tx_or_rx, mcast, msdu); \
        } else { \
            OL_TXRX_PEER_STATS_UPDATE_BASE(peer, tx_or_rx, ucast, msdu); \
        } \
    } while (0)
#define OL_TX_PEER_STATS_UPDATE(peer, msdu) \
    OL_TXRX_PEER_STATS_UPDATE(peer, tx, msdu)
#define OL_RX_PEER_STATS_UPDATE(peer, msdu) \
    OL_TXRX_PEER_STATS_UPDATE(peer, rx, msdu)
#define OL_TXRX_PEER_STATS_MUTEX_INIT(pdev) \
    adf_os_spinlock_init(&pdev->peer_stat_mutex)
#define OL_TXRX_PEER_STATS_MUTEX_DESTROY(pdev) \
    adf_os_spinlock_destroy(&pdev->peer_stat_mutex)
#else
#define OL_TX_PEER_STATS_UPDATE(peer, msdu) /* no-op */
#define OL_RX_PEER_STATS_UPDATE(peer, msdu) /* no-op */
#define OL_TXRX_PEER_STATS_MUTEX_INIT(peer) /* no-op */
#define OL_TXRX_PEER_STATS_MUTEX_DESTROY(peer) /* no-op */
#endif

#ifndef DEBUG_HTT_CREDIT
#define DEBUG_HTT_CREDIT 0
#endif

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
void
ol_txrx_update_group_credit(
    struct ol_tx_queue_group_t *group,
    int32_t credit,
    u_int8_t absolute);
#endif

#endif /* _OL_TXRX_INTERNAL__H_ */
