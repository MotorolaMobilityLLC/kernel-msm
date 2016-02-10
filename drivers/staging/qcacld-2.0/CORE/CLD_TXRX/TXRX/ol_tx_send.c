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

#include <adf_os_atomic.h>    /* adf_os_atomic_inc, etc. */
#include <adf_os_lock.h>      /* adf_os_spinlock */
#include <adf_os_time.h>      /* adf_os_ticks, etc. */
#include <adf_nbuf.h>         /* adf_nbuf_t */
#include <adf_net_types.h>    /* ADF_NBUF_TX_EXT_TID_INVALID */

#include <queue.h>            /* TAILQ */
#ifdef QCA_COMPUTE_TX_DELAY
#include <ieee80211.h>        /* ieee80211_frame, etc. */
#include <enet.h>             /* ethernet_hdr_t, etc. */
#include <ipv6_defs.h>        /* IPV6_TRAFFIC_CLASS */
#endif

#include <ol_txrx_api.h>      /* ol_txrx_vdev_handle, etc. */
#include <ol_htt_tx_api.h>    /* htt_tx_compl_desc_id */
#include <ol_txrx_htt_api.h>  /* htt_tx_status */

#include <ol_ctrl_txrx_api.h>
#include <ol_txrx_types.h>    /* ol_txrx_vdev_t, etc */
#include <ol_tx_desc.h>       /* ol_tx_desc_find, ol_tx_desc_frame_free */
#ifdef QCA_COMPUTE_TX_DELAY
#include <ol_tx_classify.h>   /* ol_tx_dest_addr_find */
#endif
#include <ol_txrx_internal.h> /* OL_TX_DESC_NO_REFS, etc. */
#include <ol_osif_txrx_api.h>
#include <ol_tx.h>            /* ol_tx_reinject */

#include <ol_cfg.h>           /* ol_cfg_is_high_latency */
#include <ol_tx_sched.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>    /* OL_TX_RESTORE_HDR, etc*/
#endif
#include <ol_tx_queue.h>
#include <ol_txrx.h>

#ifdef TX_CREDIT_RECLAIM_SUPPORT

#define OL_TX_CREDIT_RECLAIM(pdev)                                  \
    do {                                                            \
        if (adf_os_atomic_read(&pdev->target_tx_credit)  <          \
                        ol_cfg_tx_credit_lwm(pdev->ctrl_pdev)) {    \
            ol_osif_ath_tasklet(pdev->osdev);                       \
        }                                                           \
    } while (0)

#else

#define OL_TX_CREDIT_RECLAIM(pdev)

#endif /* TX_CREDIT_RECLAIM_SUPPORT */

#if defined(CONFIG_HL_SUPPORT) || defined(TX_CREDIT_RECLAIM_SUPPORT)
/*
 * HL needs to keep track of the amount of credit available to download
 * tx frames to the target - the download scheduler decides when to
 * download frames, and which frames to download, based on the credit
 * availability.
 * LL systems that use TX_CREDIT_RECLAIM_SUPPORT also need to keep track
 * of the target_tx_credit, to determine when to poll for tx completion
 * messages.
 */
#define OL_TX_TARGET_CREDIT_ADJUST(factor, pdev, msdu) \
    adf_os_atomic_add( \
        factor * htt_tx_msdu_credit(msdu), &pdev->target_tx_credit)
#define OL_TX_TARGET_CREDIT_DECR(pdev, msdu) \
    OL_TX_TARGET_CREDIT_ADJUST(-1, pdev, msdu)
#define OL_TX_TARGET_CREDIT_INCR(pdev, msdu) \
    OL_TX_TARGET_CREDIT_ADJUST(1, pdev, msdu)
#define OL_TX_TARGET_CREDIT_DECR_INT(pdev, delta) \
    adf_os_atomic_add(-1 * delta, &pdev->target_tx_credit)
#define OL_TX_TARGET_CREDIT_INCR_INT(pdev, delta) \
    adf_os_atomic_add(delta, &pdev->target_tx_credit)
#else
/*
 * LL does not need to keep track of target credit.
 * Since the host tx descriptor pool size matches the target's,
 * we know the target has space for the new tx frame if the host's
 * tx descriptor allocation succeeded.
 */
#define OL_TX_TARGET_CREDIT_ADJUST(factor, pdev, msdu)  /* no-op */
#define OL_TX_TARGET_CREDIT_DECR(pdev, msdu)  /* no-op */
#define OL_TX_TARGET_CREDIT_INCR(pdev, msdu)  /* no-op */
#define OL_TX_TARGET_CREDIT_DECR_INT(pdev, delta)  /* no-op */
#define OL_TX_TARGET_CREDIT_INCR_INT(pdev, delta)  /* no-op */
#endif

#ifdef QCA_LL_TX_FLOW_CT
#ifdef CONFIG_PER_VDEV_TX_DESC_POOL
#define OL_TX_FLOW_CT_UNPAUSE_OS_Q(pdev)                                          \
do {                                                                              \
    struct ol_txrx_vdev_t *vdev;                                                  \
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {                       \
        if (adf_os_atomic_read(&vdev->os_q_paused) &&                             \
                          (vdev->tx_fl_hwm != 0)) {                               \
            adf_os_spin_lock(&pdev->tx_mutex);                                    \
            if (((ol_tx_desc_pool_size_hl(vdev->pdev->ctrl_pdev) >> 1)            \
               - TXRX_HL_TX_FLOW_CTRL_MGMT_RESERVED)                              \
               - adf_os_atomic_read(&vdev->tx_desc_count)                         \
               > vdev->tx_fl_hwm)                                                 \
            {                                                                     \
               adf_os_atomic_set(&vdev->os_q_paused, 0);                          \
               adf_os_spin_unlock(&pdev->tx_mutex);                               \
               vdev->osif_flow_control_cb(vdev->osif_dev,                         \
                                          vdev->vdev_id, A_TRUE);                 \
            }                                                                     \
            else {                                                                \
               adf_os_spin_unlock(&pdev->tx_mutex);                               \
            }                                                                     \
        }                                                                         \
    }                                                                             \
} while(0)
#else
#define OL_TX_FLOW_CT_UNPAUSE_OS_Q(pdev)                                          \
do {                                                                              \
    struct ol_txrx_vdev_t *vdev;                                                  \
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {                       \
        if (adf_os_atomic_read(&vdev->os_q_paused) &&                            \
                          (vdev->tx_fl_hwm != 0)) {                               \
            adf_os_spin_lock(&pdev->tx_mutex);                                    \
            if (pdev->tx_desc.num_free > vdev->tx_fl_hwm) {                       \
               adf_os_atomic_set(&vdev->os_q_paused, 0);                          \
               adf_os_spin_unlock(&pdev->tx_mutex);                               \
               vdev->osif_flow_control_cb(vdev->osif_dev,                         \
                                          vdev->vdev_id, A_TRUE);                 \
            }                                                                     \
            else {                                                                \
               adf_os_spin_unlock(&pdev->tx_mutex);                               \
            }                                                                     \
        }                                                                         \
    }                                                                             \
} while(0)
#endif
#else
#define OL_TX_FLOW_CT_UNPAUSE_OS_Q(pdev)
#endif /* QCA_LL_TX_FLOW_CT */


static inline u_int16_t
ol_tx_send_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu)
{
    int msdu_credit_consumed;

    TX_CREDIT_DEBUG_PRINT("TX %d bytes\n", adf_nbuf_len(msdu));
    TX_CREDIT_DEBUG_PRINT(" <HTT> Decrease credit %d - 1 = %d, len:%d.\n",
        adf_os_atomic_read(&pdev->target_tx_credit),
        adf_os_atomic_read(&pdev->target_tx_credit) - 1,
        adf_nbuf_len(msdu));

    msdu_credit_consumed = htt_tx_msdu_credit(msdu);
    OL_TX_TARGET_CREDIT_DECR_INT(pdev, msdu_credit_consumed);
    OL_TX_CREDIT_RECLAIM(pdev);

    /*
     * When the tx frame is downloaded to the target, there are two
     * outstanding references:
     * 1.  The host download SW (HTT, HTC, HIF)
     *     This reference is cleared by the ol_tx_send_done callback
     *     functions.
     * 2.  The target FW
     *     This reference is cleared by the ol_tx_completion_handler
     *     function.
     * It is extremely probable that the download completion is processed
     * before the tx completion message.  However, under exceptional
     * conditions the tx completion may be processed first.  Thus, rather
     * that assuming that reference (1) is done before reference (2),
     * explicit reference tracking is needed.
     * Double-increment the ref count to account for both references
     * described above.
     */

    OL_TX_DESC_REF_INIT(tx_desc);
    OL_TX_DESC_REF_INC(tx_desc);
    OL_TX_DESC_REF_INC(tx_desc);

    return msdu_credit_consumed;
}

void
ol_tx_send(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu)
{
    int msdu_credit_consumed;
    u_int16_t id;
    int failed;

    msdu_credit_consumed = ol_tx_send_base(pdev, tx_desc, msdu);
    id = ol_tx_desc_id(pdev, tx_desc);
    failed = htt_tx_send_std(pdev->htt_pdev, msdu, id);
    if (adf_os_unlikely(failed)) {
        OL_TX_TARGET_CREDIT_INCR_INT(pdev, msdu_credit_consumed);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);
    }
}

void
ol_tx_send_batch(
    struct ol_txrx_pdev_t *pdev,
    adf_nbuf_t head_msdu, int num_msdus)
{
    adf_nbuf_t rejected;
    OL_TX_CREDIT_RECLAIM(pdev);

    rejected = htt_tx_send_batch(pdev->htt_pdev, head_msdu, num_msdus);
    while (adf_os_unlikely(rejected)) {
        struct ol_tx_desc_t *tx_desc;
        u_int16_t *msdu_id_storage;
        adf_nbuf_t next;

        next = adf_nbuf_next(rejected);
        msdu_id_storage = ol_tx_msdu_id_storage(rejected);
        tx_desc = ol_tx_desc_find(pdev, *msdu_id_storage);

        OL_TX_TARGET_CREDIT_INCR(pdev, rejected);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);

        rejected = next;
    }
}

void
ol_tx_send_nonstd(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    enum htt_pkt_type pkt_type)
{
    int msdu_credit_consumed;
    u_int16_t id;
    int failed;

    msdu_credit_consumed = ol_tx_send_base(pdev, tx_desc, msdu);
    id = ol_tx_desc_id(pdev, tx_desc);
    failed = htt_tx_send_nonstd(
        pdev->htt_pdev, msdu, id, pkt_type);
    if (failed) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                "Error: freeing tx frame after htt_tx failed");
        OL_TX_TARGET_CREDIT_INCR_INT(pdev, msdu_credit_consumed);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);
    }
}

static inline void
ol_tx_download_done_base(
    struct ol_txrx_pdev_t *pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    struct ol_tx_desc_t *tx_desc;

    tx_desc = ol_tx_desc_find(pdev, msdu_id);
    adf_os_assert(tx_desc);

    /*
     * If the download is done for
     * the Management frame then
     * call the download callback if registered
     */
    if (tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE) {
        int tx_mgmt_index = tx_desc->pkt_type - OL_TXRX_MGMT_TYPE_BASE;
        ol_txrx_mgmt_tx_cb download_cb =
             pdev->tx_mgmt.callbacks[tx_mgmt_index].download_cb;

        if (download_cb) {
            download_cb(pdev->tx_mgmt.callbacks[tx_mgmt_index].ctxt,
                tx_desc->netbuf, status != A_OK);
        }
    }

    if (status != A_OK) {
        OL_TX_TARGET_CREDIT_INCR(pdev, msdu);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* download err */);
    } else {
        if (OL_TX_DESC_NO_REFS(tx_desc)) {
            /*
             * The decremented value was zero - free the frame.
             * Use the tx status recorded previously during
             * tx completion handling.
             */
            ol_tx_desc_frame_free_nonstd(
                pdev, tx_desc, tx_desc->status != htt_tx_status_ok);
        }
    }
}

void
ol_tx_download_done_ll(
    void *pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    ol_tx_download_done_base(
        (struct ol_txrx_pdev_t *) pdev, status, msdu, msdu_id);
}

void
ol_tx_download_done_hl_retain(
    void *txrx_pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    struct ol_txrx_pdev_t *pdev = txrx_pdev;
    ol_tx_download_done_base(pdev, status, msdu, msdu_id);
#if 0 /* TODO: Advanced feature */
    //ol_tx_dwl_sched(pdev, OL_TX_HL_SCHED_DOWNLOAD_DONE);
adf_os_assert(0);
#endif
}

void
ol_tx_download_done_hl_free(
    void *txrx_pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    struct ol_txrx_pdev_t *pdev = txrx_pdev;
    struct ol_tx_desc_t *tx_desc;

    tx_desc = ol_tx_desc_find(pdev, msdu_id);
    adf_os_assert(tx_desc);

    ol_tx_download_done_base(pdev, status, msdu, msdu_id);

    if ((tx_desc->pkt_type != ol_tx_frm_no_free) &&
        (tx_desc->pkt_type < OL_TXRX_MGMT_TYPE_BASE)) {
        adf_os_atomic_add(1, &pdev->tx_queue.rsrc_cnt);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, status != A_OK);
    }
#if 0 /* TODO: Advanced feature */
    //ol_tx_dwl_sched(pdev, OL_TX_HL_SCHED_DOWNLOAD_DONE);
adf_os_assert(0);
#endif
}

void
ol_tx_target_credit_init(struct ol_txrx_pdev_t *pdev, int credit_delta)
{
    adf_os_atomic_add(credit_delta, &pdev->orig_target_tx_credit);
}

void
ol_tx_target_credit_update(struct ol_txrx_pdev_t *pdev, int credit_delta)
{
    TX_CREDIT_DEBUG_PRINT(" <HTT> Increase credit %d + %d = %d\n",
            adf_os_atomic_read(&pdev->target_tx_credit),
            credit_delta,
            adf_os_atomic_read(&pdev->target_tx_credit) + credit_delta);
    adf_os_atomic_add(credit_delta, &pdev->target_tx_credit);
}

#ifdef QCA_COMPUTE_TX_DELAY

static void
ol_tx_delay_compute(
    struct ol_txrx_pdev_t *pdev,
    enum htt_tx_status status,
    u_int16_t *desc_ids,
    int num_msdus);
#define OL_TX_DELAY_COMPUTE ol_tx_delay_compute
#else
#define OL_TX_DELAY_COMPUTE(pdev, status, desc_ids, num_msdus) /* no-op */
#endif /* QCA_COMPUTE_TX_DELAY */

#ifndef OL_TX_RESTORE_HDR
#define OL_TX_RESTORE_HDR(__tx_desc, __msdu)
#endif
/*
 * The following macros could have been inline functions too.
 * The only rationale for choosing macros, is to force the compiler to inline
 * the implementation, which cannot be controlled for actual "inline" functions,
 * since "inline" is only a hint to the compiler.
 * In the performance path, we choose to force the inlining, in preference to
 * type-checking offered by the actual inlined functions.
 */
#define ol_tx_msdu_complete_batch(_pdev, _tx_desc, _tx_descs, _status)                          \
        do {                                                                                    \
                TAILQ_INSERT_TAIL(&(_tx_descs), (_tx_desc), tx_desc_list_elem);                 \
        } while (0)
#ifndef ATH_11AC_TXCOMPACT
#define ol_tx_msdu_complete_single(_pdev, _tx_desc, _netbuf, _lcl_freelist, _tx_desc_last)      \
        do {                                                                                    \
                adf_os_atomic_init(&(_tx_desc)->ref_cnt); /* clear the ref cnt */               \
                OL_TX_RESTORE_HDR((_tx_desc), (_netbuf)); /* restore orginal hdr offset */      \
                adf_nbuf_unmap((_pdev)->osdev, (_netbuf), ADF_OS_DMA_TO_DEVICE);                \
                adf_nbuf_free((_netbuf));                                                       \
                ((struct ol_tx_desc_list_elem_t *)(_tx_desc))->next = (_lcl_freelist);          \
                if (adf_os_unlikely(!lcl_freelist)) {                                           \
                    (_tx_desc_last) = (struct ol_tx_desc_list_elem_t *)(_tx_desc);              \
                }                                                                               \
                (_lcl_freelist) = (struct ol_tx_desc_list_elem_t *)(_tx_desc);                  \
        } while (0)
#else  /*!ATH_11AC_TXCOMPACT*/

#define ol_tx_msdu_complete_single(_pdev, _tx_desc, _netbuf, _lcl_freelist, _tx_desc_last)      \
        do {                                                                                    \
                OL_TX_RESTORE_HDR((_tx_desc), (_netbuf)); /* restore orginal hdr offset */      \
                adf_nbuf_unmap((_pdev)->osdev, (_netbuf), ADF_OS_DMA_TO_DEVICE);                \
                adf_nbuf_free((_netbuf));                                                       \
                ((struct ol_tx_desc_list_elem_t *)(_tx_desc))->next = (_lcl_freelist);          \
                if (adf_os_unlikely(!lcl_freelist)) {                                           \
                    (_tx_desc_last) = (struct ol_tx_desc_list_elem_t *)(_tx_desc);              \
                }                                                                               \
                (_lcl_freelist) = (struct ol_tx_desc_list_elem_t *)(_tx_desc);                  \
        } while (0)


#endif /*!ATH_11AC_TXCOMPACT*/

#ifdef QCA_TX_SINGLE_COMPLETIONS
    #ifdef QCA_TX_STD_PATH_ONLY
        #define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs, _netbuf, _lcl_freelist,         \
                                        _tx_desc_last, _status)                                 \
            ol_tx_msdu_complete_single((_pdev), (_tx_desc), (_netbuf), (_lcl_freelist),         \
                                             _tx_desc_last)
    #else   /* !QCA_TX_STD_PATH_ONLY */
        #define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs, _netbuf, _lcl_freelist,         \
                                        _tx_desc_last, _status)                                 \
        do {                                                                                    \
            if (adf_os_likely((_tx_desc)->pkt_type == ol_tx_frm_std)) {                         \
                ol_tx_msdu_complete_single((_pdev), (_tx_desc), (_netbuf), (_lcl_freelist),     \
                                             (_tx_desc_last));                                  \
            } else {                                                                            \
                ol_tx_desc_frame_free_nonstd(                                                   \
                    (_pdev), (_tx_desc), (_status) != htt_tx_status_ok);                        \
            }                                                                                   \
        } while (0)
    #endif  /* !QCA_TX_STD_PATH_ONLY */
#else  /* !QCA_TX_SINGLE_COMPLETIONS */
    #ifdef QCA_TX_STD_PATH_ONLY
        #define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs, _netbuf, _lcl_freelist,         \
                                        _tx_desc_last, _status)                                 \
            ol_tx_msdus_complete_batch((_pdev), (_tx_desc), (_tx_descs), (_status))
    #else   /* !QCA_TX_STD_PATH_ONLY */
        #define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs, _netbuf, _lcl_freelist,         \
                                        _tx_desc_last, _status)                                 \
        do {                                                                                    \
            if (adf_os_likely((_tx_desc)->pkt_type == ol_tx_frm_std)) {                         \
                ol_tx_msdu_complete_batch((_pdev), (_tx_desc), (_tx_descs), (_status));         \
            } else {                                                                            \
                ol_tx_desc_frame_free_nonstd(                                                   \
                    (_pdev), (_tx_desc), (_status) != htt_tx_status_ok);                        \
            }                                                                                   \
        } while (0)
    #endif  /* !QCA_TX_STD_PATH_ONLY */
#endif /* QCA_TX_SINGLE_COMPLETIONS */

void
ol_tx_discard_target_frms(ol_txrx_pdev_handle pdev)
{
    int i = 0;
    for (i = 0; i < pdev->tx_desc.pool_size; i++) {

        /*
         * Confirm that each tx descriptor is "empty", i.e. it has
         * no tx frame attached.
         * In particular, check that there are no frames that have
         * been given to the target to transmit, for which the
         * target has never provided a response.
         */
        if (adf_os_atomic_read(&pdev->tx_desc.array[i].tx_desc->ref_cnt)) {
            TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                "Warning: freeing tx frame "
                "(no tx completion from the target)\n");
            ol_tx_desc_frame_free_nonstd(
                pdev, pdev->tx_desc.array[i].tx_desc, 1);
        }
    }
}

void
ol_tx_credit_completion_handler(ol_txrx_pdev_handle pdev, int credits)
{
    ol_tx_target_credit_update(pdev, credits);
    if (pdev->cfg.is_high_latency) {
        ol_tx_sched(pdev);
    }
    /* UNPAUSE OS Q */
    OL_TX_FLOW_CT_UNPAUSE_OS_Q(pdev);
}

/* WARNING: ol_tx_inspect_handler()'s bahavior is similar to that of ol_tx_completion_handler().
 * any change in ol_tx_completion_handler() must be mirrored in ol_tx_inspect_handler().
 */
void
ol_tx_completion_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    enum htt_tx_status status,
    void *tx_desc_id_iterator)
{
    int i;
    u_int16_t *desc_ids = (u_int16_t *)tx_desc_id_iterator;
    u_int16_t tx_desc_id;
    struct ol_tx_desc_t *tx_desc;
    char *trace_str;

    uint32_t   byte_cnt = 0;
    struct ol_tx_desc_list_elem_t *td_array = pdev->tx_desc.array;
    adf_nbuf_t  netbuf;

    struct ol_tx_desc_list_elem_t *lcl_freelist = NULL;
    struct ol_tx_desc_list_elem_t *tx_desc_last = NULL;
    ol_tx_desc_list tx_descs;
    TAILQ_INIT(&tx_descs);

    OL_TX_DELAY_COMPUTE(pdev, status, desc_ids, num_msdus);

    trace_str = (status) ? "OT:C:F:" : "OT:C:S:";
    for (i = 0; i < num_msdus; i++) {
        tx_desc_id = desc_ids[i];
        tx_desc = td_array[tx_desc_id].tx_desc;
        tx_desc->status = status;
        netbuf = tx_desc->netbuf;

        if (pdev->cfg.is_high_latency) {
            OL_TX_DESC_UPDATE_GROUP_CREDIT(pdev, tx_desc_id, 1, 0, status);
        }

        htc_pm_runtime_put(pdev->htt_pdev->htc_pdev);
        adf_nbuf_trace_update(netbuf, trace_str);
        /* Per SDU update of byte count */
        byte_cnt += adf_nbuf_len(netbuf);
        if (OL_TX_DESC_NO_REFS(tx_desc)) {
            ol_tx_statistics(pdev->ctrl_pdev,
                HTT_TX_DESC_VDEV_ID_GET(*((u_int32_t *)(tx_desc->htt_tx_desc))),
                status != htt_tx_status_ok);
            ol_tx_msdu_complete(
                pdev, tx_desc, tx_descs, netbuf,
                lcl_freelist, tx_desc_last, status);
        }
#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
        tx_desc->pkt_type = 0xff;
#ifdef QCA_COMPUTE_TX_DELAY
        tx_desc->entry_timestamp_ticks = 0xffffffff;
#endif
#endif
    }

    /* One shot protected access to pdev freelist, when setup */
    if (lcl_freelist) {
        adf_os_spin_lock(&pdev->tx_mutex);
        tx_desc_last->next = pdev->tx_desc.freelist;
        pdev->tx_desc.freelist = lcl_freelist;
        pdev->tx_desc.num_free += (u_int16_t) num_msdus;
        adf_os_spin_unlock(&pdev->tx_mutex);
    } else {
        ol_tx_desc_frame_list_free(pdev, &tx_descs, status != htt_tx_status_ok);
    }
    if (pdev->cfg.is_high_latency) {
        /*
         * Credit was already explicitly updated by HTT,
         * but update the number of available tx descriptors,
         * then invoke the scheduler, since new credit is probably
         * available now.
         */
        adf_os_atomic_add(num_msdus, &pdev->tx_queue.rsrc_cnt);
        ol_tx_sched(pdev);
    } else {
        OL_TX_TARGET_CREDIT_ADJUST(num_msdus, pdev, NULL);
    }

    /* UNPAUSE OS Q */
    OL_TX_FLOW_CT_UNPAUSE_OS_Q(pdev);
    /* Do one shot statistics */
    TXRX_STATS_UPDATE_TX_STATS(pdev, status, num_msdus, byte_cnt);
}

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
void
ol_tx_desc_update_group_credit(ol_txrx_pdev_handle pdev, u_int16_t tx_desc_id,
                       int credit, u_int8_t absolute, enum htt_tx_status status)
{
    uint8_t i, is_member;
    uint16_t vdev_id_mask;
    struct ol_tx_desc_t *tx_desc;
    struct ol_tx_desc_list_elem_t *td_array = pdev->tx_desc.array;

    tx_desc = td_array[tx_desc_id].tx_desc;

    for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
        vdev_id_mask =
               OL_TXQ_GROUP_VDEV_ID_MASK_GET(pdev->txq_grps[i].membership);
        is_member = OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(vdev_id_mask,
                                                      tx_desc->vdev->vdev_id);
        if (is_member) {
            ol_txrx_update_group_credit(&pdev->txq_grps[i],
                                        credit, absolute);
            break;
        }
    }
    OL_TX_UPDATE_GROUP_CREDIT_STATS(pdev);
}

#ifdef DEBUG_HL_LOGGING
void
ol_tx_update_group_credit_stats(ol_txrx_pdev_handle pdev)
{
    uint16 curr_index;
    uint8 i;

    adf_os_spin_lock_bh(&pdev->grp_stat_spinlock);
    pdev->grp_stats.last_valid_index++;
    if (pdev->grp_stats.last_valid_index > (OL_TX_GROUP_STATS_LOG_SIZE - 1)) {
        pdev->grp_stats.last_valid_index -= OL_TX_GROUP_STATS_LOG_SIZE;
        pdev->grp_stats.wrap_around = 1;
    }
    curr_index = pdev->grp_stats.last_valid_index;

    for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
        pdev->grp_stats.stats[curr_index].grp[i].member_vdevs =
        OL_TXQ_GROUP_VDEV_ID_MASK_GET(pdev->txq_grps[i].membership);
        pdev->grp_stats.stats[curr_index].grp[i].credit =
                  adf_os_atomic_read(&pdev->txq_grps[i].credit);
    }

    adf_os_spin_unlock_bh(&pdev->grp_stat_spinlock);
}

void
ol_tx_dump_group_credit_stats(ol_txrx_pdev_handle pdev)
{
    uint16 i,j, is_break = 0;
    int16 curr_index, old_index, wrap_around;
    uint16 curr_credit, old_credit, mem_vdevs;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,"Group credit stats:");
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                                      "  No: GrpID: Credit: Change: vdev_map");

    adf_os_spin_lock_bh(&pdev->grp_stat_spinlock);
    curr_index = pdev->grp_stats.last_valid_index;
    wrap_around = pdev->grp_stats.wrap_around;
    adf_os_spin_unlock_bh(&pdev->grp_stat_spinlock);

    if(curr_index < 0) {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,"Not initialized");
        return;
    }

    for (i = 0; i < OL_TX_GROUP_STATS_LOG_SIZE; i++) {
        old_index = curr_index - 1;
        if (old_index < 0) {
            if (wrap_around == 0)
                is_break = 1;
            else
                old_index = OL_TX_GROUP_STATS_LOG_SIZE - 1;
        }

        for (j = 0; j < OL_TX_MAX_TXQ_GROUPS; j++) {
            adf_os_spin_lock_bh(&pdev->grp_stat_spinlock);
            curr_credit = pdev->grp_stats.stats[curr_index].grp[j].credit;
            if (!is_break)
                old_credit = pdev->grp_stats.stats[old_index].grp[j].credit;
            mem_vdevs = pdev->grp_stats.stats[curr_index].grp[j].member_vdevs;
            adf_os_spin_unlock_bh(&pdev->grp_stat_spinlock);

            if (!is_break)
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                      "%4d: %5d: %6d %6d %8x",curr_index, j,
                      curr_credit, (curr_credit - old_credit),
                      mem_vdevs);
            else
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                      "%4d: %5d: %6d %6s %8x",curr_index, j,
                      curr_credit, "NA", mem_vdevs);
       }

       if (is_break)
           break;

       curr_index = old_index;
    }
}

void ol_tx_clear_group_credit_stats(ol_txrx_pdev_handle pdev)
{
    adf_os_spin_lock_bh(&pdev->grp_stat_spinlock);
    adf_os_mem_zero(&pdev->grp_stats, sizeof(pdev->grp_stats));
    pdev->grp_stats.last_valid_index = -1;
    pdev->grp_stats.wrap_around= 0;
    adf_os_spin_unlock_bh(&pdev->grp_stat_spinlock);
}
#endif
#endif

/*
 * ol_tx_single_completion_handler performs the same tx completion
 * processing as ol_tx_completion_handler, but for a single frame.
 * ol_tx_completion_handler is optimized to handle batch completions
 * as efficiently as possible; in contrast ol_tx_single_completion_handler
 * handles single frames as simply and generally as possible.
 * Thus, this ol_tx_single_completion_handler function is suitable for
 * intermittent usage, such as for tx mgmt frames.
 */
void
ol_tx_single_completion_handler(
    ol_txrx_pdev_handle pdev,
    enum htt_tx_status status,
    u_int16_t tx_desc_id)
{
    struct ol_tx_desc_t *tx_desc;
    struct ol_tx_desc_list_elem_t *td_array = pdev->tx_desc.array;
    adf_nbuf_t  netbuf;

    tx_desc = td_array[tx_desc_id].tx_desc;
    tx_desc->status = status;
    netbuf = tx_desc->netbuf;

    /* Do one shot statistics */
    TXRX_STATS_UPDATE_TX_STATS(pdev, status, 1, adf_nbuf_len(netbuf));

    if (OL_TX_DESC_NO_REFS(tx_desc)) {
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, status != htt_tx_status_ok);
    }

    TX_CREDIT_DEBUG_PRINT(
        " <HTT> Increase credit %d + %d = %d\n",
        adf_os_atomic_read(&pdev->target_tx_credit),
        1, adf_os_atomic_read(&pdev->target_tx_credit) + 1);

    if (pdev->cfg.is_high_latency) {
        /*
         * Credit was already explicitly updated by HTT,
         * but update the number of available tx descriptors,
         * then invoke the scheduler, since new credit is probably
         * available now.
         */
        adf_os_atomic_add(1, &pdev->tx_queue.rsrc_cnt);
	ol_tx_sched(pdev);
    } else {
        adf_os_atomic_add(1, &pdev->target_tx_credit);
    }
}

/* WARNING: ol_tx_inspect_handler()'s bahavior is similar to that of ol_tx_completion_handler().
 * any change in ol_tx_completion_handler() must be mirrored here.
 */
void
ol_tx_inspect_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    void *tx_desc_id_iterator)
{
    u_int16_t vdev_id, i;
    struct ol_txrx_vdev_t *vdev;
    u_int16_t *desc_ids = (u_int16_t *)tx_desc_id_iterator;
    u_int16_t tx_desc_id;
    struct ol_tx_desc_t *tx_desc;
    struct ol_tx_desc_list_elem_t *td_array = pdev->tx_desc.array;
    struct ol_tx_desc_list_elem_t *lcl_freelist = NULL;
    struct ol_tx_desc_list_elem_t *tx_desc_last = NULL;
    adf_nbuf_t  netbuf;
    ol_tx_desc_list tx_descs;
    TAILQ_INIT(&tx_descs);

    for (i = 0; i < num_msdus; i++) {
        tx_desc_id = desc_ids[i];
        tx_desc = td_array[tx_desc_id].tx_desc;
        netbuf = tx_desc->netbuf;

        /* find the "vdev" this tx_desc belongs to */
        vdev_id = HTT_TX_DESC_VDEV_ID_GET(*((u_int32_t *)(tx_desc->htt_tx_desc)));
        TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
            if (vdev->vdev_id == vdev_id)
                break;
        }

        /* vdev now points to the vdev for this descriptor. */

#ifndef ATH_11AC_TXCOMPACT
        /* save this multicast packet to local free list */
        if (adf_os_atomic_dec_and_test(&tx_desc->ref_cnt))
#endif
        {
            /* for this function only, force htt status to be "htt_tx_status_ok"
             * for graceful freeing of this multicast frame
             */
            ol_tx_msdu_complete(pdev, tx_desc, tx_descs, netbuf, lcl_freelist,
                                    tx_desc_last, htt_tx_status_ok);
        }
    }

    if (lcl_freelist) {
        adf_os_spin_lock(&pdev->tx_mutex);
        tx_desc_last->next = pdev->tx_desc.freelist;
        pdev->tx_desc.freelist = lcl_freelist;
        adf_os_spin_unlock(&pdev->tx_mutex);
    } else {
        ol_tx_desc_frame_list_free(pdev, &tx_descs, htt_tx_status_discard);
    }
    TX_CREDIT_DEBUG_PRINT(" <HTT> Increase HTT credit %d + %d = %d..\n",
            adf_os_atomic_read(&pdev->target_tx_credit),
            num_msdus,
            adf_os_atomic_read(&pdev->target_tx_credit) + num_msdus);

    if (pdev->cfg.is_high_latency) {
        /* credit was already explicitly updated by HTT */
	ol_tx_sched(pdev);
    } else {
        OL_TX_TARGET_CREDIT_ADJUST(num_msdus, pdev, NULL) ;
    }
}

#ifdef QCA_COMPUTE_TX_DELAY

void
ol_tx_set_compute_interval(
    ol_txrx_pdev_handle pdev,
    u_int32_t interval)
{
    pdev->tx_delay.avg_period_ticks = adf_os_msecs_to_ticks(interval);
}

void
ol_tx_packet_count(
    ol_txrx_pdev_handle pdev,
    u_int16_t *out_packet_count,
    u_int16_t *out_packet_loss_count,
    int category)
{
    *out_packet_count = pdev->packet_count[category];
    *out_packet_loss_count = pdev->packet_loss_count[category];
    pdev->packet_count[category] =  0;
    pdev->packet_loss_count[category] = 0;
}

u_int32_t
ol_tx_delay_avg(u_int64_t sum, u_int32_t num)
{
    u_int32_t sum32;
    int shift = 0;
    /*
     * To avoid doing a 64-bit divide, shift the sum down until it is
     * no more than 32 bits (and shift the denominator to match).
     */
    while ((sum >> 32) != 0) {
        sum >>= 1;
        shift++;
    }
    sum32 = (u_int32_t) sum;
    num >>= shift;
    return (sum32 + (num >> 1)) / num; /* round to nearest */
}

void
ol_tx_delay(
    ol_txrx_pdev_handle pdev,
    u_int32_t *queue_delay_microsec,
    u_int32_t *tx_delay_microsec,
    int category)
{
    int index;
    u_int32_t avg_delay_ticks;
    struct ol_tx_delay_data *data;

    adf_os_assert(category >= 0 && category < QCA_TX_DELAY_NUM_CATEGORIES);

    adf_os_spin_lock_bh(&pdev->tx_delay.mutex);
    index = 1 - pdev->tx_delay.cats[category].in_progress_idx;

    data = &pdev->tx_delay.cats[category].copies[index];

    if (data->avgs.transmit_num > 0) {
        avg_delay_ticks = ol_tx_delay_avg(
            data->avgs.transmit_sum_ticks, data->avgs.transmit_num);
        *tx_delay_microsec = adf_os_ticks_to_msecs(avg_delay_ticks * 1000);
    } else {
        /*
         * This case should only happen if there's a query
         * within 5 sec after the first tx data frame.
         */
        *tx_delay_microsec = 0;
    }
    if (data->avgs.queue_num > 0) {
        avg_delay_ticks = ol_tx_delay_avg(
            data->avgs.queue_sum_ticks, data->avgs.queue_num);
        *queue_delay_microsec = adf_os_ticks_to_msecs(avg_delay_ticks * 1000);
    } else {
        /*
         * This case should only happen if there's a query
         * within 5 sec after the first tx data frame.
         */
        *queue_delay_microsec = 0;
    }

    adf_os_spin_unlock_bh(&pdev->tx_delay.mutex);
}

void
ol_tx_delay_hist(
    ol_txrx_pdev_handle pdev,
    u_int16_t *report_bin_values,
    int category)
{
    int index, i, j;
    struct ol_tx_delay_data *data;

    adf_os_assert(category >= 0 && category < QCA_TX_DELAY_NUM_CATEGORIES);

    adf_os_spin_lock_bh(&pdev->tx_delay.mutex);
    index = 1 - pdev->tx_delay.cats[category].in_progress_idx;

    data = &pdev->tx_delay.cats[category].copies[index];

    for (i = 0, j = 0; i < QCA_TX_DELAY_HIST_REPORT_BINS-1; i++) {
        u_int16_t internal_bin_sum = 0;
        while (j < (1 << i)) {
            internal_bin_sum += data->hist_bins_queue[j++];
        }
        report_bin_values[i] = internal_bin_sum;
    }
    report_bin_values[i] = data->hist_bins_queue[j]; /* overflow */

    adf_os_spin_unlock_bh(&pdev->tx_delay.mutex);
}

#ifdef QCA_COMPUTE_TX_DELAY_PER_TID
static u_int8_t
ol_tx_delay_tid_from_l3_hdr(
    struct ol_txrx_pdev_t *pdev,
    adf_nbuf_t msdu,
    struct ol_tx_desc_t *tx_desc)
{
    u_int16_t ethertype;
    u_int8_t *dest_addr, *l3_hdr;
    int is_mgmt, is_mcast;
    int l2_hdr_size;

    dest_addr = ol_tx_dest_addr_find(pdev, msdu);
    if (NULL == dest_addr) {
        return ADF_NBUF_TX_EXT_TID_INVALID;
    }
    is_mcast = IEEE80211_IS_MULTICAST(dest_addr);
    is_mgmt = tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE;
    if (is_mgmt) {
        return (is_mcast) ?
            OL_TX_NUM_TIDS + OL_TX_VDEV_DEFAULT_MGMT :
            HTT_TX_EXT_TID_MGMT;
    }
    if (is_mcast) {
        return OL_TX_NUM_TIDS + OL_TX_VDEV_MCAST_BCAST;
    }
    if (pdev->frame_format == wlan_frm_fmt_802_3) {
        struct ethernet_hdr_t *enet_hdr;
        enet_hdr = (struct ethernet_hdr_t *) adf_nbuf_data(msdu);
        l2_hdr_size = sizeof(struct ethernet_hdr_t);
        ethertype = (enet_hdr->ethertype[0] << 8) | enet_hdr->ethertype[1];
        if (!IS_ETHERTYPE(ethertype)) {
            struct llc_snap_hdr_t *llc_hdr;
            llc_hdr = (struct llc_snap_hdr_t *)
                (adf_nbuf_data(msdu) + l2_hdr_size);
            l2_hdr_size += sizeof(struct llc_snap_hdr_t);
            ethertype = (llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
        }
    } else {
        struct llc_snap_hdr_t *llc_hdr;
        l2_hdr_size = sizeof(struct ieee80211_frame);
        llc_hdr = (struct llc_snap_hdr_t *) (adf_nbuf_data(msdu)
            + l2_hdr_size);
        l2_hdr_size += sizeof(struct llc_snap_hdr_t);
        ethertype = (llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
    }
    l3_hdr = adf_nbuf_data(msdu) + l2_hdr_size;
    if (ETHERTYPE_IPV4 == ethertype) {
        return (((struct ipv4_hdr_t *) l3_hdr)->tos >> 5) & 0x7;
    } else if (ETHERTYPE_IPV6 == ethertype) {
        return (IPV6_TRAFFIC_CLASS((struct ipv6_hdr_t *) l3_hdr) >> 5) & 0x7;
    } else {
        return ADF_NBUF_TX_EXT_TID_INVALID;
    }
}
#endif

static int
ol_tx_delay_category(struct ol_txrx_pdev_t *pdev, u_int16_t msdu_id)
{
#ifdef QCA_COMPUTE_TX_DELAY_PER_TID
    struct ol_tx_desc_t *tx_desc = pdev->tx_desc.array[msdu_id].tx_desc;
    u_int8_t tid;

    adf_nbuf_t msdu = tx_desc->netbuf;
    tid = adf_nbuf_get_tid(msdu);
    if (tid == ADF_NBUF_TX_EXT_TID_INVALID) {
        tid = ol_tx_delay_tid_from_l3_hdr(pdev, msdu, tx_desc);
        if (tid == ADF_NBUF_TX_EXT_TID_INVALID) {
            /* TID could not be determined (this is not an IP frame?) */
            return -1;
        }
    }
    return tid;
#else
    return 0;
#endif
}

static inline int
ol_tx_delay_hist_bin(struct ol_txrx_pdev_t *pdev, u_int32_t delay_ticks)
{
    int bin;
    /*
     * For speed, multiply and shift to approximate a divide. This causes
     * a small error, but the approximation error should be much less
     * than the other uncertainties in the tx delay computation.
     */
    bin = (delay_ticks * pdev->tx_delay.hist_internal_bin_width_mult) >>
        pdev->tx_delay.hist_internal_bin_width_shift;
    if (bin >= QCA_TX_DELAY_HIST_INTERNAL_BINS) {
        bin = QCA_TX_DELAY_HIST_INTERNAL_BINS - 1;
    }
    return bin;
}

static void
ol_tx_delay_compute(
    struct ol_txrx_pdev_t *pdev,
    enum htt_tx_status status,
    u_int16_t *desc_ids,
    int num_msdus)
{
    int i, index, cat;
    u_int32_t now_ticks = adf_os_ticks();
    u_int32_t tx_delay_transmit_ticks, tx_delay_queue_ticks;
    u_int32_t avg_time_ticks;
    struct ol_tx_delay_data *data;

    adf_os_assert(num_msdus > 0);

    /*
     * keep static counters for total packet and lost packets
     * reset them in ol_tx_delay(), function used to fetch the stats
     */

    cat = ol_tx_delay_category(pdev, desc_ids[0]);
    if (cat < 0 || cat >= QCA_TX_DELAY_NUM_CATEGORIES)
        return;

    pdev->packet_count[cat] = pdev->packet_count[cat] + num_msdus;
    if (status != htt_tx_status_ok) {
        for (i = 0; i < num_msdus; i++) {
            cat = ol_tx_delay_category(pdev, desc_ids[i]);
            if (cat < 0 || cat >= QCA_TX_DELAY_NUM_CATEGORIES)
                return;
            pdev->packet_loss_count[cat]++;
        }
        return;
    }

    /* since we may switch the ping-pong index, provide mutex w. readers */
    adf_os_spin_lock_bh(&pdev->tx_delay.mutex);
    index = pdev->tx_delay.cats[cat].in_progress_idx;

    data = &pdev->tx_delay.cats[cat].copies[index];

    if (pdev->tx_delay.tx_compl_timestamp_ticks != 0) {
        tx_delay_transmit_ticks =
            now_ticks - pdev->tx_delay.tx_compl_timestamp_ticks;
        /*
         * We'd like to account for the number of MSDUs that were
         * transmitted together, but we don't know this.  All we know
         * is the number of MSDUs that were acked together.
         * Since the frame error rate is small, this is nearly the same as
         * the number of frames transmitted together.
         */
        data->avgs.transmit_sum_ticks += tx_delay_transmit_ticks;
        data->avgs.transmit_num += num_msdus;
    }
    pdev->tx_delay.tx_compl_timestamp_ticks = now_ticks;

    for (i = 0; i < num_msdus; i++) {
        u_int16_t id = desc_ids[i];
        struct ol_tx_desc_t *tx_desc = pdev->tx_desc.array[id].tx_desc;
        int bin;

        tx_delay_queue_ticks = now_ticks - tx_desc->entry_timestamp_ticks;

        data->avgs.queue_sum_ticks += tx_delay_queue_ticks;
        data->avgs.queue_num++;
        bin = ol_tx_delay_hist_bin(pdev, tx_delay_queue_ticks);
        data->hist_bins_queue[bin]++;
    }

    /* check if it's time to start a new average */
    avg_time_ticks =
        now_ticks - pdev->tx_delay.cats[cat].avg_start_time_ticks;
    if (avg_time_ticks > pdev->tx_delay.avg_period_ticks) {
        pdev->tx_delay.cats[cat].avg_start_time_ticks = now_ticks;
        index = 1 - index;
        pdev->tx_delay.cats[cat].in_progress_idx = index;
        adf_os_mem_zero(
            &pdev->tx_delay.cats[cat].copies[index],
            sizeof(pdev->tx_delay.cats[cat].copies[index]));
    }

    adf_os_spin_unlock_bh(&pdev->tx_delay.mutex);
}

#endif /* QCA_COMPUTE_TX_DELAY */
