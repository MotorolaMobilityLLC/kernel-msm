/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

/*=== header file includes ===*/
/* generic utilities */
#include <adf_nbuf.h>          /* adf_nbuf_t, etc. */
#include <adf_os_timer.h>
#include <adf_os_time.h>

/* datapath internal interfaces */
#include <ol_txrx_types.h>     /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_internal.h>  /* TXRX_ASSERT, etc. */
#include <ol_rx_reorder.h>     /* ol_rx_reorder_flush, etc. */

#ifdef QCA_SUPPORT_OL_RX_REORDER_TIMEOUT

void
ol_rx_reorder_timeout_remove(struct ol_txrx_peer_t *peer, unsigned tid)
{
    struct ol_txrx_pdev_t *pdev;
    struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac;
    struct ol_rx_reorder_timeout_list_elem_t *list_elem;
    int ac;

    pdev = peer->vdev->pdev;
    ac = TXRX_TID_TO_WMM_AC(tid);
    rx_reorder_timeout_ac = &pdev->rx.reorder_timeout.access_cats[ac];
    list_elem = &peer->tids_rx_reorder[tid].timeout;
    if (!list_elem->active) {
        /* this element has already been removed */
        return;
    }
    list_elem->active = 0;
    TAILQ_REMOVE(
        &rx_reorder_timeout_ac->virtual_timer_list, list_elem,
        reorder_timeout_list_elem);
}

static void
ol_rx_reorder_timeout_start(
    struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac,
    u_int32_t time_now_ms)
{
    u_int32_t duration_ms;
    struct ol_rx_reorder_timeout_list_elem_t *list_elem;

    list_elem = TAILQ_FIRST(&rx_reorder_timeout_ac->virtual_timer_list);

    duration_ms = list_elem->timestamp_ms - time_now_ms;
    adf_os_timer_start(&rx_reorder_timeout_ac->timer, duration_ms);
}

static inline void
ol_rx_reorder_timeout_add(struct ol_txrx_peer_t *peer, u_int8_t tid)
{
    u_int32_t time_now_ms;
    struct ol_txrx_pdev_t *pdev;
    struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac;
    struct ol_rx_reorder_timeout_list_elem_t *list_elem;
    int ac;
    int start;

    pdev = peer->vdev->pdev;
    ac = TXRX_TID_TO_WMM_AC(tid);
    rx_reorder_timeout_ac = &pdev->rx.reorder_timeout.access_cats[ac];
    list_elem = &peer->tids_rx_reorder[tid].timeout;

    list_elem->active = 1;
    list_elem->peer = peer;
    list_elem->tid = tid;

    /* set the expiration timestamp */
    time_now_ms = adf_os_ticks_to_msecs(adf_os_ticks());
    list_elem->timestamp_ms = time_now_ms + rx_reorder_timeout_ac->duration_ms;

    /* add to the queue */
    start = TAILQ_EMPTY(&rx_reorder_timeout_ac->virtual_timer_list);
    TAILQ_INSERT_TAIL(
        &rx_reorder_timeout_ac->virtual_timer_list,
        list_elem, reorder_timeout_list_elem);
    if (start) {
        ol_rx_reorder_timeout_start(rx_reorder_timeout_ac, time_now_ms);
    }
}

void
ol_rx_reorder_timeout_update(struct ol_txrx_peer_t *peer, u_int8_t tid)
{
    if (!peer) return;

    /*
     * If there are no holes, i.e. no queued frames,
     * then timeout doesn't apply.
     */
    if (peer->tids_rx_reorder[tid].num_mpdus == 0) return;

    /*
     * If the virtual timer for this peer-TID is already running,
     * then leave it.
     */
    if (peer->tids_rx_reorder[tid].timeout.active) return;

    ol_rx_reorder_timeout_add(peer, tid);
}

static void
ol_rx_reorder_timeout(void *arg)
{
    struct ol_txrx_pdev_t *pdev;
    struct ol_rx_reorder_timeout_list_elem_t *list_elem, *tmp;
    u_int32_t time_now_ms;
    struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac;

    rx_reorder_timeout_ac = (struct ol_tx_reorder_cat_timeout_t *) arg;
    time_now_ms = adf_os_ticks_to_msecs(adf_os_ticks());

    pdev = rx_reorder_timeout_ac->pdev;
    adf_os_spin_lock(&pdev->rx.mutex);
// TODO: conditionally take mutex lock during regular rx
    TAILQ_FOREACH_SAFE(
        list_elem, &rx_reorder_timeout_ac->virtual_timer_list,
        reorder_timeout_list_elem, tmp)
    {
        unsigned idx_start, idx_end;
        struct ol_txrx_peer_t *peer;

        if (list_elem->timestamp_ms > time_now_ms) {
            break; /* time has not expired yet for this element */
        }

        list_elem->active = 0;
        /* remove the expired element from the list */
        TAILQ_REMOVE(
            &rx_reorder_timeout_ac->virtual_timer_list, list_elem,
            reorder_timeout_list_elem);

        peer = list_elem->peer;

        idx_start = 0xffff; /* start from next_rel_idx */
        ol_rx_reorder_first_hole(peer, list_elem->tid, &idx_end);
        ol_rx_reorder_flush(
            peer->vdev,
            peer,
            list_elem->tid,
            idx_start,
            idx_end,
            htt_rx_flush_release);
    }
    /* restart the timer if unexpired elements are left in the list */
    if (!TAILQ_EMPTY(&rx_reorder_timeout_ac->virtual_timer_list)) {
        ol_rx_reorder_timeout_start(rx_reorder_timeout_ac, time_now_ms);
    }
    adf_os_spin_unlock(&pdev->rx.mutex);
}

void
ol_rx_reorder_timeout_init(struct ol_txrx_pdev_t *pdev)
{
    int i;

    for (i = 0; i < ARRAY_LEN(pdev->rx.reorder_timeout.access_cats); i++) {
        struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac;
        rx_reorder_timeout_ac = &pdev->rx.reorder_timeout.access_cats[i];
        /* init the per-AC timers */
        adf_os_timer_init(
            pdev->osdev, &rx_reorder_timeout_ac->timer,
            ol_rx_reorder_timeout, rx_reorder_timeout_ac, ADF_DEFERRABLE_TIMER);
        /* init the virtual timer list */
        TAILQ_INIT(&rx_reorder_timeout_ac->virtual_timer_list);
        rx_reorder_timeout_ac->pdev = pdev;
    }
    pdev->rx.reorder_timeout.access_cats[TXRX_WMM_AC_VO].duration_ms = 40;
    pdev->rx.reorder_timeout.access_cats[TXRX_WMM_AC_VI].duration_ms = 100;
    pdev->rx.reorder_timeout.access_cats[TXRX_WMM_AC_BE].duration_ms = 100;
    pdev->rx.reorder_timeout.access_cats[TXRX_WMM_AC_BK].duration_ms = 100;
}

void
ol_rx_reorder_timeout_peer_cleanup(struct ol_txrx_peer_t *peer)
{
    int tid;

    for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
        if (peer->tids_rx_reorder[tid].timeout.active) {
            ol_rx_reorder_timeout_remove(peer, tid);
        }
    }
}

void
ol_rx_reorder_timeout_cleanup(struct ol_txrx_pdev_t *pdev)
{
    int i;

    for (i = 0; i < ARRAY_LEN(pdev->rx.reorder_timeout.access_cats); i++) {
        struct ol_tx_reorder_cat_timeout_t *rx_reorder_timeout_ac;
        rx_reorder_timeout_ac = &pdev->rx.reorder_timeout.access_cats[i];
        adf_os_timer_cancel(&rx_reorder_timeout_ac->timer);
        adf_os_timer_free(&rx_reorder_timeout_ac->timer);
    }
}

#endif /* QCA_SUPPORT_OL_RX_REORDER_TIMEOUT */
