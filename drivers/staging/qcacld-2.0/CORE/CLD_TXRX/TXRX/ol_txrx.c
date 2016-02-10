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

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>         /* u_int32_t, etc. */
#include <adf_os_mem.h>    /* adf_os_mem_alloc,free */
#include <adf_os_types.h>  /* adf_os_device_t, adf_os_print */
#include <adf_os_lock.h>   /* adf_os_spinlock */
#include <adf_os_atomic.h> /* adf_os_atomic_read */

/* header files for utilities */
#include <queue.h>         /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>        /* ol_cfg_is_high_latency */
#include <ol_if_athvar.h>

/* header files for HTT API */
#include <ol_htt_api.h>
#include <ol_htt_tx_api.h>

/* header files for OS shim API */
#include <ol_osif_api.h>

/* header files for our own APIs */
#include <ol_txrx_api.h>
#include <ol_txrx_dbg.h>
#include <ol_txrx_ctrl_api.h>
#include <ol_txrx_osif_api.h>
/* header files for our internal definitions */
#include <ol_txrx_internal.h>      /* TXRX_ASSERT, etc. */
#include <wdi_event.h>             /* WDI events */
#include <ol_txrx_types.h>         /* ol_txrx_pdev_t, etc. */
#include <ol_ctrl_txrx_api.h>
#include <ol_tx.h>                 /* ol_tx_hl, ol_tx_ll */
#include <ol_rx.h>                 /* ol_rx_deliver */
#include <ol_txrx_peer_find.h>     /* ol_txrx_peer_find_attach, etc. */
#include <ol_rx_pn.h>              /* ol_rx_pn_check, etc. */
#include <ol_rx_fwd.h>             /* ol_rx_fwd_check, etc. */
#include <ol_rx_reorder_timeout.h> /* OL_RX_REORDER_TIMEOUT_INIT, etc. */
#include <ol_rx_reorder.h>
#include <ol_tx_send.h>            /* ol_tx_discard_target_frms */
#include <ol_tx_desc.h>            /* ol_tx_desc_frame_free */
#include <ol_tx_queue.h>
#include <ol_tx_sched.h>           /* ol_tx_sched_attach, etc. */
#include <ol_txrx.h>

/*=== function definitions ===*/

u_int16_t
ol_tx_desc_pool_size_hl(ol_pdev_handle ctrl_pdev)
{
    u_int16_t desc_pool_size;
    u_int16_t steady_state_tx_lifetime_ms;
    u_int16_t safety_factor;

    /*
     * Steady-state tx latency:
     *     roughly 1-2 ms flight time
     *   + roughly 1-2 ms prep time,
     *   + roughly 1-2 ms target->host notification time.
     * = roughly 6 ms total
     * Thus, steady state number of frames =
     * steady state max throughput / frame size * tx latency, e.g.
     * 1 Gbps / 1500 bytes * 6 ms = 500
     *
     */
    steady_state_tx_lifetime_ms = 6;

    safety_factor = 8;

    desc_pool_size =
        ol_cfg_max_thruput_mbps(ctrl_pdev) *
        1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */ /
        (8 * OL_TX_AVG_FRM_BYTES) *
        steady_state_tx_lifetime_ms *
        safety_factor;

    /* minimum */
    if (desc_pool_size < OL_TX_DESC_POOL_SIZE_MIN_HL) {
        desc_pool_size = OL_TX_DESC_POOL_SIZE_MIN_HL;
    }
    /* maximum */
    if (desc_pool_size > OL_TX_DESC_POOL_SIZE_MAX_HL) {
        desc_pool_size = OL_TX_DESC_POOL_SIZE_MAX_HL;
    }
    return desc_pool_size;
}
#ifdef QCA_SUPPORT_TXRX_LOCAL_PEER_ID
ol_txrx_peer_handle
ol_txrx_find_peer_by_addr_and_vdev(ol_txrx_pdev_handle pdev,
                                   ol_txrx_vdev_handle vdev,
                                   u_int8_t *peer_addr,
                                   u_int8_t *peer_id)
{
    struct ol_txrx_peer_t *peer;

    peer = ol_txrx_peer_vdev_find_hash(pdev, vdev, peer_addr, 0, 1);
    if (!peer)
        return NULL;
    *peer_id = peer->local_id;
    adf_os_atomic_dec(&peer->ref_cnt);
    return peer;
}

ol_txrx_peer_handle ol_txrx_find_peer_by_addr(ol_txrx_pdev_handle pdev,
					 u_int8_t *peer_addr,
					 u_int8_t *peer_id)
{
	struct ol_txrx_peer_t *peer;

	peer = ol_txrx_peer_find_hash_find(pdev, peer_addr, 0, 1);
	if (!peer)
		return NULL;
	*peer_id = peer->local_id;
	adf_os_atomic_dec(&peer->ref_cnt);
	return peer;
}

u_int16_t
ol_txrx_local_peer_id(ol_txrx_peer_handle peer)
{
    return peer->local_id;
}

ol_txrx_peer_handle
ol_txrx_peer_find_by_local_id(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t local_peer_id)
{
    struct ol_txrx_peer_t *peer;
    if ((local_peer_id == OL_TXRX_INVALID_LOCAL_PEER_ID) ||
        (local_peer_id >= OL_TXRX_NUM_LOCAL_PEER_IDS)) {
        return NULL;
    }

    adf_os_spin_lock_bh(&pdev->local_peer_ids.lock);
    peer = pdev->local_peer_ids.map[local_peer_id];
    adf_os_spin_unlock_bh(&pdev->local_peer_ids.lock);
    return peer;
}

static void
OL_TXRX_LOCAL_PEER_ID_POOL_INIT(struct ol_txrx_pdev_t *pdev)
{
    int i;

    /* point the freelist to the first ID */
    pdev->local_peer_ids.freelist = 0;

    /* link each ID to the next one */
    for (i = 0; i < OL_TXRX_NUM_LOCAL_PEER_IDS; i++) {
        pdev->local_peer_ids.pool[i] = i + 1;
        pdev->local_peer_ids.map[i] = NULL;
    }

    /* link the last ID to itself, to mark the end of the list */
    i = OL_TXRX_NUM_LOCAL_PEER_IDS;
    pdev->local_peer_ids.pool[i] = i;

    adf_os_spinlock_init(&pdev->local_peer_ids.lock);
}

static void
OL_TXRX_LOCAL_PEER_ID_ALLOC(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    int i;

    adf_os_spin_lock_bh(&pdev->local_peer_ids.lock);
    i = pdev->local_peer_ids.freelist;
    if (pdev->local_peer_ids.pool[i] == i) {
        /* the list is empty, except for the list-end marker */
        peer->local_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
    } else {
        /* take the head ID and advance the freelist */
        peer->local_id = i;
        pdev->local_peer_ids.freelist = pdev->local_peer_ids.pool[i];
        pdev->local_peer_ids.map[i] = peer;
    }
    adf_os_spin_unlock_bh(&pdev->local_peer_ids.lock);
}

static void
OL_TXRX_LOCAL_PEER_ID_FREE(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    int i = peer->local_id;
    if ((i == OL_TXRX_INVALID_LOCAL_PEER_ID) ||
        (i >= OL_TXRX_NUM_LOCAL_PEER_IDS)) {
        return;
    }
    /* put this ID on the head of the freelist */
    adf_os_spin_lock_bh(&pdev->local_peer_ids.lock);
    pdev->local_peer_ids.pool[i] = pdev->local_peer_ids.freelist;
    pdev->local_peer_ids.freelist = i;
    pdev->local_peer_ids.map[i] = NULL;
    adf_os_spin_unlock_bh(&pdev->local_peer_ids.lock);
}

static void
OL_TXRX_LOCAL_PEER_ID_CLEANUP(struct ol_txrx_pdev_t *pdev)
{
    adf_os_spinlock_destroy(&pdev->local_peer_ids.lock);
}

#else
#define OL_TXRX_LOCAL_PEER_ID_POOL_INIT(pdev)   /* no-op */
#define OL_TXRX_LOCAL_PEER_ID_ALLOC(pdev, peer) /* no-op */
#define OL_TXRX_LOCAL_PEER_ID_FREE(pdev, peer)  /* no-op */
#define OL_TXRX_LOCAL_PEER_ID_CLEANUP(pdev)     /* no-op */
#endif

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
void
ol_txrx_update_group_credit(
    struct ol_tx_queue_group_t *group,
    int32_t credit,
    u_int8_t absolute)
{
    if (absolute) {
        adf_os_atomic_set(&group->credit, credit);
    } else {
        adf_os_atomic_add(credit, &group->credit);
    }
}

void
ol_txrx_update_tx_queue_groups(
    ol_txrx_pdev_handle pdev,
    u_int8_t group_id,
    int32_t credit,
    u_int8_t absolute,
    u_int32_t vdev_id_mask,
    u_int32_t ac_mask
)
{
    struct ol_tx_queue_group_t *group;
    u_int32_t group_vdev_bit_mask, vdev_bit_mask, group_vdev_id_mask;
    u_int32_t membership;
    struct ol_txrx_vdev_t *vdev;
    group = &pdev->txq_grps[group_id];

    membership = OL_TXQ_GROUP_MEMBERSHIP_GET(vdev_id_mask,ac_mask);

    adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
    /*
     * if the membership (vdev id mask and ac mask)
     * matches then no need to update tx qeue groups.
     */
    if (group->membership == membership) {
       /* Update Credit Only */
       goto credit_update;
    }

    /*
     * membership (vdev id mask and ac mask) is not matching
     * TODO: ignoring ac mask for now
     */
    group_vdev_id_mask =
        OL_TXQ_GROUP_VDEV_ID_MASK_GET(group->membership);

    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        group_vdev_bit_mask =
           OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(group_vdev_id_mask,vdev->vdev_id);
        vdev_bit_mask =
           OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(vdev_id_mask,vdev->vdev_id);

        if (group_vdev_bit_mask != vdev_bit_mask) {
            /*
             * Change in vdev tx queue group
             */
            if (!vdev_bit_mask) {
                /* Set Group Pointer (vdev and peer) to NULL */
                ol_tx_set_vdev_group_ptr(pdev, vdev->vdev_id, NULL);
            } else {
                /* Set Group Pointer (vdev and peer) */
                ol_tx_set_vdev_group_ptr(pdev, vdev->vdev_id, group);
            }
        }
    }
    /* Update membership */
    group->membership = membership;
credit_update:
    /* Update Credit */
    ol_txrx_update_group_credit(group, credit, absolute);
    adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
}
#endif

ol_txrx_pdev_handle
ol_txrx_pdev_attach(
    ol_pdev_handle ctrl_pdev,
    HTC_HANDLE htc_pdev,
    adf_os_device_t osdev)
{
    int i, tid;
    struct ol_txrx_pdev_t *pdev;
#ifdef WDI_EVENT_ENABLE
    A_STATUS ret;
#endif
    uint16_t desc_pool_size;
    uint32_t page_size;
    void **desc_pages = NULL;
    unsigned int pages_idx;
    unsigned int descs_idx;

    pdev = adf_os_mem_alloc(osdev, sizeof(*pdev));
    if (!pdev) {
        goto fail0;
    }
    adf_os_mem_zero(pdev, sizeof(*pdev));

    /* init LL/HL cfg here */
    pdev->cfg.is_high_latency = ol_cfg_is_high_latency(ctrl_pdev);
    pdev->cfg.default_tx_comp_req = !ol_cfg_tx_free_at_download(ctrl_pdev);

    /* store provided params */
    pdev->ctrl_pdev = ctrl_pdev;
    pdev->osdev = osdev;

    for (i = 0; i < htt_num_sec_types; i++) {
        pdev->sec_types[i] = (enum ol_sec_type)i;
    }

    TXRX_STATS_INIT(pdev);

    TAILQ_INIT(&pdev->vdev_list);

    /* do initial set up of the peer ID -> peer object lookup map */
    if (ol_txrx_peer_find_attach(pdev)) {
        goto fail1;
    }

    if (ol_cfg_is_high_latency(ctrl_pdev)) {
        desc_pool_size = ol_tx_desc_pool_size_hl(ctrl_pdev);
        adf_os_atomic_init(&pdev->tx_queue.rsrc_cnt);
        adf_os_atomic_add(desc_pool_size, &pdev->tx_queue.rsrc_cnt);
#if defined(CONFIG_PER_VDEV_TX_DESC_POOL)
        /*
         * 5% margin of unallocated desc is too much for per vdev mechanism.
         * Define the value seperately.
         */
        pdev->tx_queue.rsrc_threshold_lo = TXRX_HL_TX_FLOW_CTRL_MGMT_RESERVED;

        /* when freeing up descriptors, keep going until there's a 7.5% margin */
        pdev->tx_queue.rsrc_threshold_hi = ((15 * desc_pool_size)/100)/2;
#else
        /* always maintain a 5% margin of unallocated descriptors */
        pdev->tx_queue.rsrc_threshold_lo = (5 * desc_pool_size)/100;

        /* when freeing up descriptors, keep going until there's a 15% margin */
        pdev->tx_queue.rsrc_threshold_hi = (15 * desc_pool_size)/100;
#endif
        for (i = 0 ; i < OL_TX_MAX_TXQ_GROUPS; i++) {
            adf_os_atomic_init(&pdev->txq_grps[i].credit);
        }

    } else {
        /*
         * For LL, limit the number of host's tx descriptors to match the
         * number of target FW tx descriptors.
         * This simplifies the FW, by ensuring the host will never download
         * more tx descriptors than the target has space for.
         * The FW will drop/free low-priority tx descriptors when it starts
         * to run low, so that in theory the host should never run out of
         * tx descriptors.
         */
        desc_pool_size = ol_cfg_target_tx_credit(ctrl_pdev);
    }

    /* initialize the counter of the target's tx buffer availability */
    adf_os_atomic_init(&pdev->target_tx_credit);
    adf_os_atomic_init(&pdev->orig_target_tx_credit);
    /*
     * LL - initialize the target credit outselves.
     * HL - wait for a HTT target credit initialization during htt_attach.
     */
    if (!ol_cfg_is_high_latency(ctrl_pdev)) {
        adf_os_atomic_add(
            ol_cfg_target_tx_credit(pdev->ctrl_pdev), &pdev->target_tx_credit);
    }

    if (ol_cfg_is_high_latency(ctrl_pdev)) {
        ol_tx_target_credit_init(pdev, desc_pool_size);
    }

    pdev->htt_pdev = htt_attach(
        pdev, ctrl_pdev, htc_pdev, osdev, desc_pool_size);
    if (!pdev->htt_pdev) {
        goto fail2;
    }

#ifdef IPA_UC_OFFLOAD
    /* Attach micro controller data path offload resource */
    if (ol_cfg_ipa_uc_offload_enabled(ctrl_pdev)) {
       if (htt_ipa_uc_attach(pdev->htt_pdev)) {
           goto fail3;
       }
    }
#endif /* IPA_UC_OFFLOAD */

    pdev->tx_desc.array = adf_os_mem_alloc(
        osdev, desc_pool_size * sizeof(struct ol_tx_desc_list_elem_t));
    if (!pdev->tx_desc.array) {
        goto fail3;
    }
    adf_os_mem_set(
        pdev->tx_desc.array, 0,
        desc_pool_size * sizeof(struct ol_tx_desc_list_elem_t));

    pdev->desc_mem_size = desc_pool_size * sizeof(struct ol_tx_desc_t);
    page_size = adf_os_mem_get_page_size();
    pdev->num_descs_per_page = page_size / sizeof(struct ol_tx_desc_t);
    pdev->num_desc_pages = desc_pool_size / pdev->num_descs_per_page;
    if (desc_pool_size % pdev->num_descs_per_page)
        pdev->num_desc_pages++;

    /* Allocate host descriptor resources */
    desc_pages = adf_os_mem_alloc(
        pdev->osdev, pdev->num_desc_pages * sizeof(char *));
    if (!desc_pages)
        goto fail3;

    for (pages_idx = 0; pages_idx < pdev->num_desc_pages; pages_idx++) {
        desc_pages[pages_idx] = adf_os_mem_alloc(pdev->osdev, page_size);
        if (!desc_pages[pages_idx]) {
            for (i = 0; i < pages_idx; i++)
                adf_os_mem_free(desc_pages[i]);
            adf_os_mem_free(desc_pages);
            goto fail3;
        }
    }
    pdev->desc_pages = desc_pages;

    /*
     * Each SW tx desc (used only within the tx datapath SW) has a
     * matching HTT tx desc (used for downloading tx meta-data to FW/HW).
     * Go ahead and allocate the HTT tx desc and link it with the SW tx
     * desc now, to avoid doing it during time-critical transmit.
     */
    pdev->tx_desc.pool_size = desc_pool_size;

    pages_idx = 0;
    descs_idx = 0;
    for (i = 0; i < desc_pool_size; i++) {
        void *htt_tx_desc;
        u_int32_t paddr_lo;

        pdev->tx_desc.array[i].tx_desc =
            (struct ol_tx_desc_t *)(desc_pages[pages_idx] +
            descs_idx * sizeof(struct ol_tx_desc_t));
        descs_idx++;
        if (pdev->num_descs_per_page == descs_idx) {
            /* Next page */
            pages_idx++;
            descs_idx = 0;
        }

        htt_tx_desc = htt_tx_desc_alloc(pdev->htt_pdev, &paddr_lo);
        if (! htt_tx_desc) {
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
                "%s: failed to alloc HTT tx desc (%d of %d)\n",
                __func__, i, desc_pool_size);
            while (--i >= 0) {
                htt_tx_desc_free(
                    pdev->htt_pdev,
                    pdev->tx_desc.array[i].tx_desc->htt_tx_desc);
            }
            goto fail4;
        }
        pdev->tx_desc.array[i].tx_desc->htt_tx_desc = htt_tx_desc;
	pdev->tx_desc.array[i].tx_desc->htt_tx_desc_paddr = paddr_lo;
#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
        pdev->tx_desc.array[i].tx_desc->pkt_type = 0xff;
#ifdef QCA_COMPUTE_TX_DELAY
        pdev->tx_desc.array[i].tx_desc->entry_timestamp_ticks = 0xffffffff;
#endif
#endif
        pdev->tx_desc.array[i].tx_desc->p_link = (void *)&pdev->tx_desc.array[i];
        pdev->tx_desc.array[i].tx_desc->id = i;
    }

    /* link SW tx descs into a freelist */
    pdev->tx_desc.num_free = desc_pool_size;
    pdev->tx_desc.freelist = &pdev->tx_desc.array[0];
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
               "%s first tx_desc:0x%p Last tx desc:0x%p\n", __func__,
               (u_int32_t *) pdev->tx_desc.freelist,
               (u_int32_t *) (pdev->tx_desc.freelist + desc_pool_size));
    for (i = 0; i < desc_pool_size-1; i++) {
        pdev->tx_desc.array[i].next = &pdev->tx_desc.array[i+1];
    }
    pdev->tx_desc.array[i].next = NULL;

    /* check what format of frames are expected to be delivered by the OS */
    pdev->frame_format = ol_cfg_frame_type(pdev->ctrl_pdev);
    if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        pdev->htt_pkt_type = htt_pkt_type_native_wifi;
    } else if (pdev->frame_format == wlan_frm_fmt_802_3) {
        pdev->htt_pkt_type = htt_pkt_type_ethernet;
    } else {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
            "%s Invalid standard frame type: %d\n",
            __func__, pdev->frame_format);
        goto fail5;
    }

    /* setup the global rx defrag waitlist */
    TAILQ_INIT(&pdev->rx.defrag.waitlist);

    /* configure where defrag timeout and duplicate detection is handled */
    pdev->rx.flags.defrag_timeout_check =
        pdev->rx.flags.dup_check =
            ol_cfg_rx_host_defrag_timeout_duplicate_check(ctrl_pdev);

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
    /* Need to revisit this part. Currently,hardcode to riva's caps */
    pdev->target_tx_tran_caps = wlan_frm_tran_cap_raw;
    pdev->target_rx_tran_caps = wlan_frm_tran_cap_raw;
    /*
     * The Riva HW de-aggregate doesn't have capability to generate 802.11
     * header for non-first subframe of A-MSDU.
     */
    pdev->sw_subfrm_hdr_recovery_enable = 1;
    /*
     * The Riva HW doesn't have the capability to set Protected Frame bit
     * in the MAC header for encrypted data frame.
     */
    pdev->sw_pf_proc_enable = 1;

    if (pdev->frame_format == wlan_frm_fmt_802_3) {
        /* sw llc process is only needed in 802.3 to 802.11 transform case */
        pdev->sw_tx_llc_proc_enable = 1;
        pdev->sw_rx_llc_proc_enable = 1;
    } else {
        pdev->sw_tx_llc_proc_enable = 0;
        pdev->sw_rx_llc_proc_enable = 0;
    }

    switch(pdev->frame_format) {
    case wlan_frm_fmt_raw:
        pdev->sw_tx_encap =
            pdev->target_tx_tran_caps & wlan_frm_tran_cap_raw ? 0 : 1;
        pdev->sw_rx_decap =
            pdev->target_rx_tran_caps & wlan_frm_tran_cap_raw ? 0 : 1;
        break;
    case wlan_frm_fmt_native_wifi:
        pdev->sw_tx_encap =
            pdev->target_tx_tran_caps & wlan_frm_tran_cap_native_wifi ? 0: 1;
        pdev->sw_rx_decap =
            pdev->target_rx_tran_caps & wlan_frm_tran_cap_native_wifi ? 0: 1;
        break;
    case wlan_frm_fmt_802_3:
        pdev->sw_tx_encap =
            pdev->target_tx_tran_caps & wlan_frm_tran_cap_8023 ? 0: 1;
        pdev->sw_rx_decap =
            pdev->target_rx_tran_caps & wlan_frm_tran_cap_8023 ? 0: 1;
        break;
    default:
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
            "Invalid standard frame type for encap/decap: fmt %x tx %x rx %x\n",
            pdev->frame_format,
            pdev->target_tx_tran_caps,
            pdev->target_rx_tran_caps);
        goto fail5;
    }
#endif

    /*
     * Determine what rx processing steps are done within the host.
     * Possibilities:
     * 1.  Nothing - rx->tx forwarding and rx PN entirely within target.
     *     (This is unlikely; even if the target is doing rx->tx forwarding,
     *     the host should be doing rx->tx forwarding too, as a back up for
     *     the target's rx->tx forwarding, in case the target runs short on
     *     memory, and can't store rx->tx frames that are waiting for missing
     *     prior rx frames to arrive.)
     * 2.  Just rx -> tx forwarding.
     *     This is the typical configuration for HL, and a likely
     *     configuration for LL STA or small APs (e.g. retail APs).
     * 3.  Both PN check and rx -> tx forwarding.
     *     This is the typical configuration for large LL APs.
     * Host-side PN check without rx->tx forwarding is not a valid
     * configuration, since the PN check needs to be done prior to
     * the rx->tx forwarding.
     */
    if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev)) {
        /* PN check, rx-tx forwarding and rx reorder is done by the target */
        if (ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev)) {
            pdev->rx_opt_proc = ol_rx_in_order_deliver;
        } else {
            pdev->rx_opt_proc = ol_rx_fwd_check;
        }
    } else {
        if (ol_cfg_rx_pn_check(pdev->ctrl_pdev)) {
            if (ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev)) {
                /*
                 * PN check done on host, rx->tx forwarding not done at all.
                 */
                pdev->rx_opt_proc = ol_rx_pn_check_only;
            } else if (ol_cfg_rx_fwd_check(pdev->ctrl_pdev)) {
                /*
                 * Both PN check and rx->tx forwarding done on host.
                 */
                pdev->rx_opt_proc = ol_rx_pn_check;
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                    "%s: invalid config: if rx PN check is on the host,"
                    "rx->tx forwarding check needs to also be on the host.\n",
                    __func__);
                goto fail5;
            }
        } else {
            /* PN check done on target */
            if ((!ol_cfg_rx_fwd_disabled(pdev->ctrl_pdev)) &&
                ol_cfg_rx_fwd_check(pdev->ctrl_pdev))
            {
                /*
                 * rx->tx forwarding done on host (possibly as
                 * back-up for target-side primary rx->tx forwarding)
                 */
                pdev->rx_opt_proc = ol_rx_fwd_check;
            } else {
                /* rx->tx forwarding either done in target, or not done at all */
                pdev->rx_opt_proc = ol_rx_deliver;
            }
        }
    }

    /* initialize mutexes for tx desc alloc and peer lookup */
    adf_os_spinlock_init(&pdev->tx_mutex);
    adf_os_spinlock_init(&pdev->peer_ref_mutex);
    adf_os_spinlock_init(&pdev->rx.mutex);
    adf_os_spinlock_init(&pdev->last_real_peer_mutex);
    OL_TXRX_PEER_STATS_MUTEX_INIT(pdev);

    if (ol_cfg_is_high_latency(ctrl_pdev)) {
        adf_os_spinlock_init(&pdev->tx_queue_spinlock);
        pdev->tx_sched.scheduler = ol_tx_sched_attach(pdev);
        if (pdev->tx_sched.scheduler == NULL) {
            goto fail6;
        }
    }

    if (OL_RX_REORDER_TRACE_ATTACH(pdev) != A_OK) {
        goto fail7;
    }

    if (OL_RX_PN_TRACE_ATTACH(pdev) != A_OK) {
        goto fail8;
    }

#ifdef PERE_IP_HDR_ALIGNMENT_WAR
    pdev->host_80211_enable = ol_scn_host_80211_enable_get(pdev->ctrl_pdev);
#endif

    /*
     * WDI event attach
     */
#ifdef WDI_EVENT_ENABLE
    if ((ret = wdi_event_attach(pdev)) == A_ERROR) {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_WARN,
            "WDI event attach unsuccessful\n");
    }
#endif

    /*
     * Initialize rx PN check characteristics for different security types.
     */
    adf_os_mem_set(&pdev->rx_pn[0], 0, sizeof(pdev->rx_pn));

    /* TKIP: 48-bit TSC, CCMP: 48-bit PN */
    pdev->rx_pn[htt_sec_type_tkip].len =
        pdev->rx_pn[htt_sec_type_tkip_nomic].len =
        pdev->rx_pn[htt_sec_type_aes_ccmp].len = 48;
    pdev->rx_pn[htt_sec_type_tkip].cmp =
        pdev->rx_pn[htt_sec_type_tkip_nomic].cmp =
        pdev->rx_pn[htt_sec_type_aes_ccmp].cmp = ol_rx_pn_cmp48;

    /* WAPI: 128-bit PN */
    pdev->rx_pn[htt_sec_type_wapi].len = 128;
    pdev->rx_pn[htt_sec_type_wapi].cmp = ol_rx_pn_wapi_cmp;

    OL_RX_REORDER_TIMEOUT_INIT(pdev);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Created pdev %p\n", pdev);

    #if defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING)
    adf_os_spinlock_init(&pdev->txq_log_spinlock);
    pdev->txq_log.size = OL_TXQ_LOG_SIZE;
    pdev->txq_log.oldest_record_offset = 0;
    pdev->txq_log.offset = 0;
    pdev->txq_log.allow_wrap = 1;
    pdev->txq_log.wrapped = 0;
    #endif /* defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING) */

#ifdef DEBUG_HL_LOGGING
    adf_os_spinlock_init(&pdev->grp_stat_spinlock);
    pdev->grp_stats.last_valid_index = -1;
    pdev->grp_stats.wrap_around= 0;
#endif
    pdev->cfg.host_addba = ol_cfg_host_addba(ctrl_pdev);

    #ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
    #define OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT 3
    #if 1
    #define OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT \
        /* avg = 100% * new + 0% * old */ \
        (1 << OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT)
    #else
    #define OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT \
        /* avg = 25% * new + 25% * old */ \
        (1 << (OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT-2))
    #endif
    pdev->rssi_update_shift = OL_TXRX_RSSI_UPDATE_SHIFT_DEFAULT;
    pdev->rssi_new_weight =  OL_TXRX_RSSI_NEW_WEIGHT_DEFAULT;
    #endif

    OL_TXRX_LOCAL_PEER_ID_POOL_INIT(pdev);

    pdev->cfg.ll_pause_txq_limit = ol_tx_cfg_max_tx_queue_depth_ll(ctrl_pdev);

    /* TX flow control for peer who is in very bad link status */
    ol_tx_badpeer_flow_cl_init(pdev);

#ifdef QCA_COMPUTE_TX_DELAY
    adf_os_mem_zero(&pdev->tx_delay, sizeof(pdev->tx_delay));
    adf_os_spinlock_init(&pdev->tx_delay.mutex);

    /* initialize compute interval with 5 seconds (ESE default) */
    pdev->tx_delay.avg_period_ticks = adf_os_msecs_to_ticks(5000);
    {
        u_int32_t bin_width_1000ticks;
        bin_width_1000ticks = adf_os_msecs_to_ticks(
            QCA_TX_DELAY_HIST_INTERNAL_BIN_WIDTH_MS * 1000);
        /*
         * Compute a factor and shift that together are equal to the
         * inverse of the bin_width time, so that rather than dividing
         * by the bin width time, approximately the same result can be
         * obtained much more efficiently by a multiply + shift.
         * multiply_factor >> shift = 1 / bin_width_time, so
         * multiply_factor = (1 << shift) / bin_width_time.
         *
         * Pick the shift semi-arbitrarily.
         * If we knew statically what the bin_width would be, we could
         * choose a shift that minimizes the error.
         * Since the bin_width is determined dynamically, simply use a
         * shift that is about half of the u_int32_t size.  This should
         * result in a relatively large multiplier value, which minimizes
         * the error from rounding the multiplier to an integer.
         * The rounding error only becomes significant if the tick units
         * are on the order of 1 microsecond.  In most systems, it is
         * expected that the tick units will be relatively low-resolution,
         * on the order of 1 millisecond.  In such systems the rounding
         * error is negligible.
         * It would be more accurate to dynamically try out different
         * shifts and choose the one that results in the smallest rounding
         * error, but that extra level of fidelity is not needed.
         */
        pdev->tx_delay.hist_internal_bin_width_shift = 16;
        pdev->tx_delay.hist_internal_bin_width_mult =
            ((1 << pdev->tx_delay.hist_internal_bin_width_shift) *
            1000 + (bin_width_1000ticks >> 1)) / bin_width_1000ticks;
    }
#endif /* QCA_COMPUTE_TX_DELAY */

#ifdef QCA_SUPPORT_TX_THROTTLE
    /* Thermal Mitigation */
    ol_tx_throttle_init(pdev);
#endif

    /*
     * Init the tid --> category table.
     * Regular tids (0-15) map to their AC.
     * Extension tids get their own categories.
     */
    for (tid = 0; tid < OL_TX_NUM_QOS_TIDS; tid++) {
        int ac = TXRX_TID_TO_WMM_AC(tid);
        pdev->tid_to_ac[tid] = ac;
    }
    pdev->tid_to_ac[OL_TX_NON_QOS_TID] =
        OL_TX_SCHED_WRR_ADV_CAT_NON_QOS_DATA;
    pdev->tid_to_ac[OL_TX_MGMT_TID] =
        OL_TX_SCHED_WRR_ADV_CAT_UCAST_MGMT;
    pdev->tid_to_ac[OL_TX_NUM_TIDS + OL_TX_VDEV_MCAST_BCAST] =
        OL_TX_SCHED_WRR_ADV_CAT_MCAST_DATA;
    pdev->tid_to_ac[OL_TX_NUM_TIDS + OL_TX_VDEV_DEFAULT_MGMT] =
        OL_TX_SCHED_WRR_ADV_CAT_MCAST_MGMT;

    return pdev; /* success */

fail8:
    OL_RX_REORDER_TRACE_DETACH(pdev);

fail7:
    adf_os_spinlock_destroy(&pdev->tx_mutex);
    adf_os_spinlock_destroy(&pdev->peer_ref_mutex);
    adf_os_spinlock_destroy(&pdev->rx.mutex);
    adf_os_spinlock_destroy(&pdev->last_real_peer_mutex);
    OL_TXRX_PEER_STATS_MUTEX_DESTROY(pdev);

    ol_tx_sched_detach(pdev);

fail6:
    if (ol_cfg_is_high_latency(ctrl_pdev)) {
        adf_os_spinlock_destroy(&pdev->tx_queue_spinlock);
    }

fail5:
    for (i = 0; i < desc_pool_size; i++) {
        htt_tx_desc_free(
            pdev->htt_pdev, pdev->tx_desc.array[i].tx_desc->htt_tx_desc);
    }

fail4:
    for (i = 0; i < pages_idx; i++)
        adf_os_mem_free(desc_pages[i]);
    adf_os_mem_free(desc_pages);

    adf_os_mem_free(pdev->tx_desc.array);
#ifdef IPA_UC_OFFLOAD
    if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev)) {
       htt_ipa_uc_detach(pdev->htt_pdev);
    }
#endif /* IPA_UC_OFFLOAD */

fail3:
    htt_detach(pdev->htt_pdev);

fail2:
    ol_txrx_peer_find_detach(pdev);

fail1:
    adf_os_mem_free(pdev);

fail0:
    return NULL; /* fail */
}

A_STATUS ol_txrx_pdev_attach_target(ol_txrx_pdev_handle pdev)
{
    return htt_attach_target(pdev->htt_pdev);
}

void
ol_txrx_pdev_detach(ol_txrx_pdev_handle pdev, int force)
{
    int i;
    unsigned int page_idx;

    /*checking to ensure txrx pdev structure is not NULL */
    if (!pdev) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "NULL pdev passed to %s\n", __func__);
        return;
    }
    /* preconditions */
    TXRX_ASSERT2(pdev);

    /* check that the pdev has no vdevs allocated */
    TXRX_ASSERT1(TAILQ_EMPTY(&pdev->vdev_list));

    OL_RX_REORDER_TIMEOUT_CLEANUP(pdev);

    if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
        ol_tx_sched_detach(pdev);
    }
#ifdef QCA_SUPPORT_TX_THROTTLE
    /* Thermal Mitigation */
    adf_os_timer_cancel(&pdev->tx_throttle.phase_timer);
    adf_os_timer_free(&pdev->tx_throttle.phase_timer);
#ifdef QCA_SUPPORT_TXRX_VDEV_LL_TXQ
    adf_os_timer_cancel(&pdev->tx_throttle.tx_timer);
    adf_os_timer_free(&pdev->tx_throttle.tx_timer);
#endif
#endif

    if (force) {
        /*
         * The assertion above confirms that all vdevs within this pdev
         * were detached.  However, they may not have actually been deleted.
         * If the vdev had peers which never received a PEER_UNMAP message
         * from the target, then there are still zombie peer objects, and
         * the vdev parents of the zombie peers are also zombies, hanging
         * around until their final peer gets deleted.
         * Go through the peer hash table and delete any peers left in it.
         * As a side effect, this will complete the deletion of any vdevs
         * that are waiting for their peers to finish deletion.
         */
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Force delete for pdev %p\n", pdev);
        ol_txrx_peer_find_hash_erase(pdev);
    }

    /* Stop the communication between HTT and target at first */
    htt_detach_target(pdev->htt_pdev);

    for (i = 0; i < pdev->tx_desc.pool_size; i++) {
        void *htt_tx_desc;

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
        htt_tx_desc = pdev->tx_desc.array[i].tx_desc->htt_tx_desc;
        htt_tx_desc_free(pdev->htt_pdev, htt_tx_desc);
    }


    for (page_idx = 0; page_idx < pdev->num_desc_pages; page_idx++) {
        adf_os_mem_free(pdev->desc_pages[page_idx]);
    }
    adf_os_mem_free(pdev->desc_pages);

    adf_os_mem_free(pdev->tx_desc.array);

#ifdef IPA_UC_OFFLOAD
    /* Detach micro controller data path offload resource */
    if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev)) {
       htt_ipa_uc_detach(pdev->htt_pdev);
    }
#endif /* IPA_UC_OFFLOAD */

    htt_detach(pdev->htt_pdev);

    ol_txrx_peer_find_detach(pdev);

    adf_os_spinlock_destroy(&pdev->tx_mutex);
    adf_os_spinlock_destroy(&pdev->peer_ref_mutex);
    adf_os_spinlock_destroy(&pdev->last_real_peer_mutex);
    adf_os_spinlock_destroy(&pdev->rx.mutex);
#ifdef QCA_SUPPORT_TX_THROTTLE
    /* Thermal Mitigation */
    adf_os_spinlock_destroy(&pdev->tx_throttle.mutex);
#endif

    /* TX flow control for peer who is in very bad link status */
    ol_tx_badpeer_flow_cl_deinit(pdev);

    OL_TXRX_PEER_STATS_MUTEX_DESTROY(pdev);

    OL_RX_REORDER_TRACE_DETACH(pdev);
    OL_RX_PN_TRACE_DETACH(pdev);

#if defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING)
    adf_os_spinlock_destroy(&pdev->txq_log_spinlock);
#endif

#ifdef DEBUG_HL_LOGGING
    adf_os_spinlock_destroy(&pdev->grp_stat_spinlock);
#endif

    /*
     * WDI event detach
     */
#ifdef WDI_EVENT_ENABLE
    if (wdi_event_detach(pdev) == A_ERROR) {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_WARN,
            "WDI detach unsuccessful\n");
    }
#endif
    OL_TXRX_LOCAL_PEER_ID_CLEANUP(pdev);

#ifdef QCA_COMPUTE_TX_DELAY
    adf_os_spinlock_destroy(&pdev->tx_delay.mutex);
#endif

    adf_os_mem_free(pdev);
}

ol_txrx_vdev_handle
ol_txrx_vdev_attach(
    ol_txrx_pdev_handle pdev,
    u_int8_t *vdev_mac_addr,
    u_int8_t vdev_id,
    enum wlan_op_mode op_mode)
{
    struct ol_txrx_vdev_t *vdev;

    /* preconditions */
    TXRX_ASSERT2(pdev);
    TXRX_ASSERT2(vdev_mac_addr);

    vdev = adf_os_mem_alloc(pdev->osdev, sizeof(*vdev));
    if (!vdev) {
        return NULL; /* failure */
    }

    /* store provided params */
    vdev->pdev = pdev;
    vdev->vdev_id = vdev_id;
    vdev->opmode = op_mode;

    vdev->osif_rx =     NULL;

    vdev->delete.pending = 0;
    vdev->safemode = 0;
    vdev->drop_unenc = 1;
    vdev->num_filters = 0;
#if defined(CONFIG_PER_VDEV_TX_DESC_POOL)
    adf_os_atomic_init(&vdev->tx_desc_count);
#endif

    adf_os_mem_copy(
        &vdev->mac_addr.raw[0], vdev_mac_addr, OL_TXRX_MAC_ADDR_LEN);

    TAILQ_INIT(&vdev->peer_list);
    vdev->last_real_peer = NULL;

    #if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
    vdev->hlTdlsFlag = false;
    #endif

    #ifdef  QCA_IBSS_SUPPORT
    vdev->ibss_peer_num = 0;
    vdev->ibss_peer_heart_beat_timer = 0;
    #endif

    #if defined(CONFIG_HL_SUPPORT)
    if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
        u_int8_t i;
        for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
            TAILQ_INIT(&vdev->txqs[i].head);
            vdev->txqs[i].paused_count.total = 0;
            vdev->txqs[i].frms = 0;
            vdev->txqs[i].bytes = 0;
            vdev->txqs[i].ext_tid = OL_TX_NUM_TIDS + i;
            vdev->txqs[i].flag = ol_tx_queue_empty;
            /* aggregation is not applicable for vdev tx queues */
            vdev->txqs[i].aggr_state = ol_tx_aggr_disabled;
            OL_TX_TXQ_SET_GROUP_PTR(&vdev->txqs[i], NULL);
            ol_txrx_set_txq_peer(&vdev->txqs[i], NULL);
        }
    }
    #endif /* defined(CONFIG_HL_SUPPORT) */

    adf_os_spinlock_init(&vdev->ll_pause.mutex);
    vdev->ll_pause.paused_reason = 0;
    vdev->ll_pause.txq.head = vdev->ll_pause.txq.tail = NULL;
    vdev->ll_pause.txq.depth = 0;
    adf_os_timer_init(
            pdev->osdev,
            &vdev->ll_pause.timer,
            ol_tx_vdev_ll_pause_queue_send,
            vdev, ADF_DEFERRABLE_TIMER);
    adf_os_atomic_init(&vdev->os_q_paused);
    adf_os_atomic_set(&vdev->os_q_paused, 0);
    vdev->tx_fl_lwm = 0;
    vdev->tx_fl_hwm = 0;
    vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
    vdev->osif_flow_control_cb = NULL;
    /* Default MAX Q depth for every VDEV */
    vdev->ll_pause.max_q_depth =
        ol_tx_cfg_max_tx_queue_depth_ll(vdev->pdev->ctrl_pdev);
    /* add this vdev into the pdev's list */
    TAILQ_INSERT_TAIL(&pdev->vdev_list, vdev, vdev_list_elem);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "Created vdev %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
        vdev,
        vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
        vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

    /*
     * We've verified that htt_op_mode == wlan_op_mode,
     * so no translation is needed.
     */
    htt_vdev_attach(pdev->htt_pdev, vdev_id, op_mode);

    return vdev;
}

void ol_txrx_osif_vdev_register(ol_txrx_vdev_handle vdev,
                                void *osif_vdev,
                                struct ol_txrx_osif_ops *txrx_ops)
{
    vdev->osif_dev = osif_vdev;
    vdev->osif_rx = txrx_ops->rx.std;

    if (ol_cfg_is_high_latency(vdev->pdev->ctrl_pdev)) {
        txrx_ops->tx.std = vdev->tx = ol_tx_hl;
        txrx_ops->tx.non_std = ol_tx_non_std_hl;
    } else {
        txrx_ops->tx.std = vdev->tx = OL_TX_LL;
        txrx_ops->tx.non_std = ol_tx_non_std_ll;
    }
    #ifdef QCA_LL_TX_FLOW_CT
    vdev->osif_flow_control_cb = txrx_ops->tx.flow_control_cb;
    #endif /* QCA_LL_TX_FLOW_CT */
}

void
ol_txrx_set_curchan(
    ol_txrx_pdev_handle pdev,
    u_int32_t chan_mhz)
{
    return;
}

void
ol_txrx_set_safemode(
    ol_txrx_vdev_handle vdev,
    u_int32_t val)
{
    vdev->safemode = val;
}

void
ol_txrx_set_privacy_filters(
    ol_txrx_vdev_handle vdev,
    void *filters,
    u_int32_t num)
{
     adf_os_mem_copy(
         vdev->privacy_filters, filters, num*sizeof(privacy_exemption));
     vdev->num_filters = num;
}

void
ol_txrx_set_drop_unenc(
    ol_txrx_vdev_handle vdev,
    u_int32_t val)
{
    vdev->drop_unenc = val;
}

void
ol_txrx_vdev_detach(
    ol_txrx_vdev_handle vdev,
    ol_txrx_vdev_delete_cb callback,
    void *context)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    /* preconditions */
    TXRX_ASSERT2(vdev);

#if defined(CONFIG_HL_SUPPORT)
    if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
        struct ol_tx_frms_queue_t *txq;
        int i;

        for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
            txq = &vdev->txqs[i];
            ol_tx_queue_free(pdev, txq, (i + OL_TX_NUM_TIDS));
        }
    }
    #endif /* defined(CONFIG_HL_SUPPORT) */

    adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
    adf_os_timer_cancel(&vdev->ll_pause.timer);
    vdev->ll_pause.is_q_timer_on = FALSE;
    adf_os_timer_free(&vdev->ll_pause.timer);
    while (vdev->ll_pause.txq.head) {
        adf_nbuf_t next = adf_nbuf_next(vdev->ll_pause.txq.head);
        adf_nbuf_set_next(vdev->ll_pause.txq.head, NULL);
        adf_nbuf_unmap(pdev->osdev, vdev->ll_pause.txq.head,
                       ADF_OS_DMA_TO_DEVICE);
        adf_nbuf_tx_free(vdev->ll_pause.txq.head, 1 /* error */);
        vdev->ll_pause.txq.head = next;
    }
    adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
    adf_os_spinlock_destroy(&vdev->ll_pause.mutex);

    /* remove the vdev from its parent pdev's list */
    TAILQ_REMOVE(&pdev->vdev_list, vdev, vdev_list_elem);

    /*
     * Use peer_ref_mutex while accessing peer_list, in case
     * a peer is in the process of being removed from the list.
     */
    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    /* check that the vdev has no peers allocated */
    if (!TAILQ_EMPTY(&vdev->peer_list)) {
        /* debug print - will be removed later */
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
            "%s: not deleting vdev object %p (%02x:%02x:%02x:%02x:%02x:%02x)"
            "until deletion finishes for all its peers\n",
            __func__, vdev,
            vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
            vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
            vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
        /* indicate that the vdev needs to be deleted */
        vdev->delete.pending = 1;
        vdev->delete.callback = callback;
        vdev->delete.context = context;
        adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
        return;
    }
    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "%s: deleting vdev object %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
        __func__, vdev,
        vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
        vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

    htt_vdev_detach(pdev->htt_pdev, vdev->vdev_id);

    /*
     * Doesn't matter if there are outstanding tx frames -
     * they will be freed once the target sends a tx completion
     * message for them.
     */
    adf_os_mem_free(vdev);
    if (callback) {
        callback(context);
    }
}

ol_txrx_peer_handle
ol_txrx_peer_attach(
    ol_txrx_pdev_handle pdev,
    ol_txrx_vdev_handle vdev,
    u_int8_t *peer_mac_addr)
{
    struct ol_txrx_peer_t *peer;
    struct ol_txrx_peer_t *temp_peer;
    u_int8_t i;
    int differs;
    bool wait_on_deletion = false;
    unsigned long rc;

    /* preconditions */
    TXRX_ASSERT2(pdev);
    TXRX_ASSERT2(vdev);
    TXRX_ASSERT2(peer_mac_addr);

    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    /* check for duplicate exsisting peer */
    TAILQ_FOREACH(temp_peer, &vdev->peer_list, peer_list_elem) {
        if (!ol_txrx_peer_find_mac_addr_cmp(&temp_peer->mac_addr,
                   (union ol_txrx_align_mac_addr_t *)peer_mac_addr)) {
            TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                "vdev_id %d (%02x:%02x:%02x:%02x:%02x:%02x) already exsist.\n",
                vdev->vdev_id,
                peer_mac_addr[0], peer_mac_addr[1],
                peer_mac_addr[2], peer_mac_addr[3],
                peer_mac_addr[4], peer_mac_addr[5]);
            if (adf_os_atomic_read(&temp_peer->delete_in_progress)) {
                vdev->wait_on_peer_id = temp_peer->local_id;
                adf_os_init_completion(&vdev->wait_delete_comp);
                wait_on_deletion = true;
            } else {
                adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
                return NULL;
            }
        }
    }

    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);

    if (wait_on_deletion) {
        /* wait for peer deletion */
        rc = adf_os_wait_for_completion_timeout(
                            &vdev->wait_delete_comp,
                            adf_os_msecs_to_ticks(PEER_DELETION_TIMEOUT));
        if (!rc) {
             TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                 "timedout waiting for peer(%d) deletion\n",
                 vdev->wait_on_peer_id);
             vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
             return NULL;
        }
    }

    peer = adf_os_mem_alloc(pdev->osdev, sizeof(*peer));
    if (!peer) {
        return NULL; /* failure */
    }
    adf_os_mem_zero(peer, sizeof(*peer));

    /* store provided params */
    peer->vdev = vdev;
    adf_os_mem_copy(
        &peer->mac_addr.raw[0], peer_mac_addr, OL_TXRX_MAC_ADDR_LEN);

    #if defined(CONFIG_HL_SUPPORT)
    if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
        adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
        for (i = 0; i < OL_TX_NUM_TIDS; i++) {
            TAILQ_INIT(&peer->txqs[i].head);
            peer->txqs[i].paused_count.total = 0;
            peer->txqs[i].frms = 0;
            peer->txqs[i].bytes = 0;
            peer->txqs[i].ext_tid = i;
            peer->txqs[i].flag = ol_tx_queue_empty;
            peer->txqs[i].aggr_state = ol_tx_aggr_untried;
            OL_TX_SET_PEER_GROUP_PTR(pdev, peer, vdev->vdev_id, i);
            ol_txrx_set_txq_peer(&peer->txqs[i], peer);
        }
        adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);

        /* aggregation is not applicable for mgmt and non-QoS tx queues */
        for (i = OL_TX_NUM_QOS_TIDS; i < OL_TX_NUM_TIDS; i++) {
            peer->txqs[i].aggr_state = ol_tx_aggr_disabled;
        }
    }
    ol_txrx_peer_pause(peer);
    #endif /* defined(CONFIG_HL_SUPPORT) */

    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    /* add this peer into the vdev's list */
    TAILQ_INSERT_TAIL(&vdev->peer_list, peer, peer_list_elem);
    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
    /* check whether this is a real peer (peer mac addr != vdev mac addr) */
    if (ol_txrx_peer_find_mac_addr_cmp(&vdev->mac_addr, &peer->mac_addr)) {
        vdev->last_real_peer = peer;
    }

    peer->rx_opt_proc = pdev->rx_opt_proc;

    ol_rx_peer_init(pdev, peer);

    /* initialize the peer_id */
    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        peer->peer_ids[i] = HTT_INVALID_PEER;
    }

    adf_os_atomic_init(&peer->delete_in_progress);

    adf_os_atomic_init(&peer->ref_cnt);

    /* keep one reference for attach */
    adf_os_atomic_inc(&peer->ref_cnt);

    /* keep one reference for ol_rx_peer_map_handler */
    adf_os_atomic_inc(&peer->ref_cnt);

    peer->valid = 1;

    ol_txrx_peer_find_hash_add(pdev, peer);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
        "vdev %p created peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
        vdev, peer,
        peer->mac_addr.raw[0], peer->mac_addr.raw[1], peer->mac_addr.raw[2],
        peer->mac_addr.raw[3], peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
    /*
     * For every peer MAp message search and set if bss_peer
     */
    differs = adf_os_mem_cmp(
        peer->mac_addr.raw, vdev->mac_addr.raw, OL_TXRX_MAC_ADDR_LEN);
    if (!differs) {
        peer->bss_peer = 1;
    }

    /*
     * The peer starts in the "disc" state while association is in progress.
     * Once association completes, the peer will get updated to "auth" state
     * by a call to ol_txrx_peer_state_update if the peer is in open mode, or
     * else to the "conn" state. For non-open mode, the peer will progress to
     * "auth" state once the authentication completes.
     */
    peer->state = ol_txrx_peer_state_invalid;
    ol_txrx_peer_state_update(pdev, peer->mac_addr.raw, ol_txrx_peer_state_disc);

    #ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
    peer->rssi_dbm = HTT_RSSI_INVALID;
    #endif

    OL_TXRX_LOCAL_PEER_ID_ALLOC(pdev, peer);

    return peer;
}

/*
 * Discarding tx filter - removes all data frames (disconnected state)
 */
static A_STATUS
ol_tx_filter_discard(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
    return A_ERROR;
}
/*
 * Non-autentication tx filter - filters out data frames that are not
 * related to authentication, but allows EAPOL (PAE) or WAPI (WAI)
 * data frames (connected state)
 */
static A_STATUS
ol_tx_filter_non_auth(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
    return
        (tx_msdu_info->htt.info.ethertype == ETHERTYPE_PAE ||
         tx_msdu_info->htt.info.ethertype == ETHERTYPE_WAI) ? A_OK : A_ERROR;
}

/*
 * Pass-through tx filter - lets all data frames through (authenticated state)
 */
static A_STATUS
ol_tx_filter_pass_thru(struct ol_txrx_msdu_info_t *tx_msdu_info)
{
    return A_OK;
}

void
ol_txrx_peer_state_update(ol_txrx_pdev_handle pdev, u_int8_t *peer_mac,
			  enum ol_txrx_peer_state state)
{
	struct ol_txrx_peer_t *peer;

	peer =  ol_txrx_peer_find_hash_find(pdev, peer_mac, 0, 1);

        if (NULL == peer)
        {
           TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: peer is null for peer_mac"
             " 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", __FUNCTION__,
             peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3],
             peer_mac[4], peer_mac[5]);
             return;
        }


	/* TODO: Should we send WMI command of the connection state? */
    /* avoid multiple auth state change. */
    if (peer->state == state) {
#ifdef TXRX_PRINT_VERBOSE_ENABLE
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO3,
            "%s: no state change, returns directly\n", __FUNCTION__);
#endif
	adf_os_atomic_dec(&peer->ref_cnt);
        return;
    }

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: change from %d to %d\n",
        __FUNCTION__, peer->state, state);

    peer->tx_filter = (state == ol_txrx_peer_state_auth) ?
        ol_tx_filter_pass_thru : (state == ol_txrx_peer_state_conn) ?
            ol_tx_filter_non_auth : ol_tx_filter_discard;

    if (peer->vdev->pdev->cfg.host_addba) {
        if (state == ol_txrx_peer_state_auth) {
            int tid;
            /*
             * Pause all regular (non-extended) TID tx queues until data
             * arrives and ADDBA negotiation has completed.
             */
            TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
                "%s: pause peer and unpause mgmt, non-qos\n", __func__);
            ol_txrx_peer_pause(peer); /* pause all tx queues */
            /* unpause mgmt and non-QoS tx queues */
            for (tid = OL_TX_NUM_QOS_TIDS; tid < OL_TX_NUM_TIDS; tid++) {
                ol_txrx_peer_tid_unpause(peer, tid);
            }
        }
    }
    adf_os_atomic_dec(&peer->ref_cnt);

    /* Set the state after the Pause to avoid the race condiction with ADDBA check in tx path */
    peer->state = state;
}

void
ol_txrx_peer_keyinstalled_state_update(
    struct ol_txrx_peer_t *peer,
    u_int8_t val)
{
    peer->keyinstalled = val;
}

void
ol_txrx_peer_update(ol_txrx_vdev_handle vdev,
                        u_int8_t *peer_mac,
                        ol_txrx_peer_update_param_t *param,
                        ol_txrx_peer_update_select_t select)
{
    struct ol_txrx_peer_t *peer;

    peer =  ol_txrx_peer_find_hash_find(vdev->pdev, peer_mac, 0, 1);
    if (!peer)
    {
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "%s: peer is null", __FUNCTION__);
        return;
    }

    switch (select) {
    case ol_txrx_peer_update_qos_capable:
        {
            /* save qos_capable here txrx peer,
                      * when HTT_ISOC_T2H_MSG_TYPE_PEER_INFO comes then save.
                      */
            peer->qos_capable = param->qos_capable;
            /*
                       * The following function call assumes that the peer has a single
                       * ID.  This is currently true, and is expected to remain true.
                       */
            htt_peer_qos_update(
                peer->vdev->pdev->htt_pdev,
                peer->peer_ids[0],
                peer->qos_capable);
            break;
        }
    case ol_txrx_peer_update_uapsdMask:
        {
            peer->uapsd_mask = param->uapsd_mask;
            htt_peer_uapsdmask_update(
                peer->vdev->pdev->htt_pdev,
                peer->peer_ids[0],
                peer->uapsd_mask);
            break;
        }
    case ol_txrx_peer_update_peer_security:
        {
            enum ol_sec_type              sec_type = param->sec_type;
            enum htt_sec_type             peer_sec_type = htt_sec_type_none;

            switch(sec_type) {
            case ol_sec_type_none:
                peer_sec_type = htt_sec_type_none;
                break;
            case ol_sec_type_wep128:
                peer_sec_type = htt_sec_type_wep128;
                break;
            case ol_sec_type_wep104:
                peer_sec_type = htt_sec_type_wep104;
                break;
            case ol_sec_type_wep40:
                peer_sec_type = htt_sec_type_wep40;
                break;
            case ol_sec_type_tkip:
                peer_sec_type = htt_sec_type_tkip;
                break;
            case ol_sec_type_tkip_nomic:
                peer_sec_type = htt_sec_type_tkip_nomic;
                break;
            case ol_sec_type_aes_ccmp:
                peer_sec_type = htt_sec_type_aes_ccmp;
                break;
            case ol_sec_type_wapi:
                peer_sec_type = htt_sec_type_wapi;
                break;
            default:
                peer_sec_type = htt_sec_type_none;
                break;
            }

            peer->security[txrx_sec_ucast].sec_type =
            peer->security[txrx_sec_mcast].sec_type = peer_sec_type;

            break;
        }
    default:
        {
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                "ERROR: unknown param %d in %s\n", select, __func__);
            break;
        }
    }
    adf_os_atomic_dec(&peer->ref_cnt);
}

u_int8_t
ol_txrx_peer_uapsdmask_get(struct ol_txrx_pdev_t *txrx_pdev, u_int16_t peer_id)
{

    struct ol_txrx_peer_t *peer;
    peer = ol_txrx_peer_find_by_id(txrx_pdev, peer_id);
    if (peer) {
        return peer->uapsd_mask;
    }

    return 0;
}

u_int8_t
ol_txrx_peer_qoscapable_get (struct ol_txrx_pdev_t * txrx_pdev, u_int16_t peer_id)
{

    struct ol_txrx_peer_t *peer_t  = ol_txrx_peer_find_by_id(txrx_pdev, peer_id);
    if (peer_t != NULL)
    {
        return peer_t->qos_capable;
    }

    return 0;
}

void
ol_txrx_peer_unref_delete(ol_txrx_peer_handle peer)
{
    struct ol_txrx_vdev_t *vdev;
    struct ol_txrx_pdev_t *pdev;
    int i;

    /* preconditions */
    TXRX_ASSERT2(peer);

    vdev = peer->vdev;
    if (NULL == vdev) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "The vdev is not present anymore\n");
        return;
    }

    pdev = vdev->pdev;
    if (NULL == pdev) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "The pdev is not present anymore\n");
        return;
    }

    /*
     * Check for the reference count before deleting the peer
     * as we noticed that sometimes we are re-entering this
     * function again which is leading to dead-lock.
     * (A double-free should never happen, so throw an assertion if it does.)
     */

    if (0 == adf_os_atomic_read(&(peer->ref_cnt)) ) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "The Peer is not present anymore\n");
        adf_os_assert(0);
        return;
    }

    /*
     * Hold the lock all the way from checking if the peer ref count
     * is zero until the peer references are removed from the hash
     * table and vdev list (if the peer ref count is zero).
     * This protects against a new HL tx operation starting to use the
     * peer object just after this function concludes it's done being used.
     * Furthermore, the lock needs to be held while checking whether the
     * vdev's list of peers is empty, to make sure that list is not modified
     * concurrently with the empty check.
     */
    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    if (adf_os_atomic_dec_and_test(&peer->ref_cnt)) {
        u_int16_t peer_id;

        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
            "Deleting peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
            peer,
            peer->mac_addr.raw[0], peer->mac_addr.raw[1],
            peer->mac_addr.raw[2], peer->mac_addr.raw[3],
            peer->mac_addr.raw[4], peer->mac_addr.raw[5]);

        peer_id = peer->local_id;
        /* remove the reference to the peer from the hash table */
        ol_txrx_peer_find_hash_remove(pdev, peer);

        /* remove the peer from its parent vdev's list */
        TAILQ_REMOVE(&peer->vdev->peer_list, peer, peer_list_elem);

        /* cleanup the Rx reorder queues for this peer */
        ol_rx_peer_cleanup(vdev, peer);

        /* peer is removed from peer_list */
        adf_os_atomic_set(&peer->delete_in_progress, 0);

        /* Set wait_delete_comp event if the current peer id matches
         * with registered peer id.
         */
        if (peer_id == vdev->wait_on_peer_id) {
            adf_os_complete(&vdev->wait_delete_comp);
            vdev->wait_on_peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
        }

        /* check whether the parent vdev has no peers left */
        if (TAILQ_EMPTY(&vdev->peer_list)) {
            /*
             * Check if the parent vdev was waiting for its peers to be
             * deleted, in order for it to be deleted too.
             */
            if (vdev->delete.pending == 1) {
                ol_txrx_vdev_delete_cb vdev_delete_cb = vdev->delete.callback;
                void *vdev_delete_context = vdev->delete.context;

                /*
                 * Now that there are no references to the peer, we can
                 * release the peer reference lock.
                 */
                adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);

                TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
                    "%s: deleting vdev object %p "
                    "(%02x:%02x:%02x:%02x:%02x:%02x)"
                    " - its last peer is done\n",
                    __func__, vdev,
                    vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
                    vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
                    vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
                /* all peers are gone, go ahead and delete it */
                adf_os_mem_free(vdev);
                if (vdev_delete_cb) {
                    vdev_delete_cb(vdev_delete_context);
                }
            } else {
                adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
            }
        } else {
            adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
        }

        #if defined(CONFIG_HL_SUPPORT)
        if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
            struct ol_tx_frms_queue_t *txq;

            for (i = 0; i < OL_TX_NUM_TIDS; i++) {
                txq = &peer->txqs[i];
                ol_tx_queue_free(pdev, txq, i);
            }
        }
        #endif /* defined(CONFIG_HL_SUPPORT) */
        /*
         * 'array' is allocated in addba handler and is supposed to be freed
         * in delba handler. There is the case (for example, in SSR) where
         * delba handler is not called. Because array points to address of
         * 'base' by default and is reallocated in addba handler later, only
         * free the memory when the array does not point to base.
         */
        for (i = 0; i < OL_TXRX_NUM_EXT_TIDS; i++) {
            if (peer->tids_rx_reorder[i].array !=
                &peer->tids_rx_reorder[i].base) {
                    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
                               "%s, delete reorder array, tid:%d\n",
                               __func__, i);
                    adf_os_mem_free(peer->tids_rx_reorder[i].array);
                    ol_rx_reorder_init(&peer->tids_rx_reorder[i], (u_int8_t)i);
            }
        }

        adf_os_mem_free(peer);
    } else {
        adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
}

void
ol_txrx_peer_detach(ol_txrx_peer_handle peer)
{
    struct ol_txrx_vdev_t *vdev = peer->vdev;

    /* redirect the peer's rx delivery function to point to a discard func */
    peer->rx_opt_proc = ol_rx_discard;

    peer->valid = 0;

    OL_TXRX_LOCAL_PEER_ID_FREE(peer->vdev->pdev, peer);

    /* debug print to dump rx reorder state */
    //htt_rx_reorder_log_print(vdev->pdev->htt_pdev);

    TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
        "%s:peer %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
          __func__, peer,
          peer->mac_addr.raw[0], peer->mac_addr.raw[1],
          peer->mac_addr.raw[2], peer->mac_addr.raw[3],
          peer->mac_addr.raw[4], peer->mac_addr.raw[5]);

    if (peer->vdev->last_real_peer == peer) {
        peer->vdev->last_real_peer = NULL;
    }

    adf_os_spin_lock_bh(&vdev->pdev->last_real_peer_mutex);
    if (vdev->last_real_peer == peer) {
	    vdev->last_real_peer = NULL;
    }
    adf_os_spin_unlock_bh(&vdev->pdev->last_real_peer_mutex);
    htt_rx_reorder_log_print(peer->vdev->pdev->htt_pdev);

    /* set delete_in_progress to identify that wma
     * is waiting for unmap massage for this peer */
    adf_os_atomic_set(&peer->delete_in_progress, 1);
    /*
     * Remove the reference added during peer_attach.
     * The peer will still be left allocated until the
     * PEER_UNMAP message arrives to remove the other
     * reference, added by the PEER_MAP message.
     */
    ol_txrx_peer_unref_delete(peer);
}

ol_txrx_peer_handle
ol_txrx_peer_find_by_addr(struct ol_txrx_pdev_t *pdev, u_int8_t *peer_mac_addr)
{
    struct ol_txrx_peer_t *peer;
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 0, 0);
    if (peer) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                  "%s: Delete extra reference %p\n", __func__, peer);
        /* release the extra reference */
        ol_txrx_peer_unref_delete(peer);
    }
    return peer;
}

/**
 * ol_txrx_dump_tx_desc() - dump tx desc info
 * @pdev_handle: Pointer to pdev handle
 *
 * Return: none
 */
void ol_txrx_dump_tx_desc(ol_txrx_pdev_handle pdev_handle)
{
	struct ol_txrx_pdev_t *pdev = pdev_handle;
	int total;

	if (ol_cfg_is_high_latency(pdev->ctrl_pdev))
		total = adf_os_atomic_read(&pdev->orig_target_tx_credit);
	else
		total = ol_cfg_target_tx_credit(pdev->ctrl_pdev);

	TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
		"Total tx credits %d free_credits %d",
		total, pdev->tx_desc.num_free);

	return;
}

int
ol_txrx_get_tx_pending(ol_txrx_pdev_handle pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (ol_txrx_pdev_handle)pdev_handle;
    int total;

    if (ol_cfg_is_high_latency(pdev->ctrl_pdev)) {
        total = adf_os_atomic_read(&pdev->orig_target_tx_credit);
    } else {
        total = ol_cfg_target_tx_credit(pdev->ctrl_pdev);
    }

    return (total - pdev->tx_desc.num_free);
}

void
ol_txrx_discard_tx_pending(ol_txrx_pdev_handle pdev_handle)
{
    ol_tx_desc_list tx_descs;
	/* First let hif do the adf_os_atomic_dec_and_test(&tx_desc->ref_cnt)
      * then let htt do the adf_os_atomic_dec_and_test(&tx_desc->ref_cnt)
      * which is tha same with normal data send complete path*/
    htt_tx_pending_discard(pdev_handle->htt_pdev);

    TAILQ_INIT(&tx_descs);
    ol_tx_queue_discard(pdev_handle, A_TRUE, &tx_descs);
    //Discard Frames in Discard List
    ol_tx_desc_frame_list_free(pdev_handle, &tx_descs, 1 /* error */);

    ol_tx_discard_target_frms(pdev_handle);
}

/*--- debug features --------------------------------------------------------*/

unsigned g_txrx_print_level = TXRX_PRINT_LEVEL_ERR; /* default */

void ol_txrx_print_level_set(unsigned level)
{
#ifndef TXRX_PRINT_ENABLE
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
        "The driver is compiled without TXRX prints enabled.\n"
        "To enable them, recompile with TXRX_PRINT_ENABLE defined.\n");
#else
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
        "TXRX printout level changed from %d to %d\n",
        g_txrx_print_level, level);
    g_txrx_print_level = level;
#endif
}

struct ol_txrx_stats_req_internal {
    struct ol_txrx_stats_req base;
    int serviced; /* state of this request */
    int offset;
};

static inline
u_int64_t OL_TXRX_STATS_PTR_TO_U64(struct ol_txrx_stats_req_internal *req)
{
    return (u_int64_t) ((size_t) req);
}

static inline
struct ol_txrx_stats_req_internal * OL_TXRX_U64_TO_STATS_PTR(u_int64_t cookie)
{
    return (struct ol_txrx_stats_req_internal *) ((size_t) cookie);
}

#ifdef ATH_PERF_PWR_OFFLOAD
void
ol_txrx_fw_stats_cfg(
    ol_txrx_vdev_handle vdev,
    u_int8_t cfg_stats_type,
    u_int32_t cfg_val)
{
    u_int64_t dummy_cookie = 0;
    htt_h2t_dbg_stats_get(
        vdev->pdev->htt_pdev,
        0 /* upload mask */,
        0 /* reset mask */,
        cfg_stats_type,
        cfg_val,
        dummy_cookie);
}

A_STATUS
ol_txrx_fw_stats_get(
    ol_txrx_vdev_handle vdev,
    struct ol_txrx_stats_req *req)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    u_int64_t cookie;
    struct ol_txrx_stats_req_internal *non_volatile_req;

    if (!pdev ||
        req->stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
        req->stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS )
    {
        return A_ERROR;
    }

    /*
     * Allocate a non-transient stats request object.
     * (The one provided as an argument is likely allocated on the stack.)
     */
    non_volatile_req = adf_os_mem_alloc(pdev->osdev, sizeof(*non_volatile_req));
    if (! non_volatile_req) {
        return A_NO_MEMORY;
    }
    /* copy the caller's specifications */
    non_volatile_req->base = *req;
    non_volatile_req->serviced = 0;
    non_volatile_req->offset = 0;

    /* use the non-volatile request object's address as the cookie */
    cookie = OL_TXRX_STATS_PTR_TO_U64(non_volatile_req);

    if (htt_h2t_dbg_stats_get(
            pdev->htt_pdev,
            req->stats_type_upload_mask,
            req->stats_type_reset_mask,
            HTT_H2T_STATS_REQ_CFG_STAT_TYPE_INVALID, 0,
            cookie))
    {
        adf_os_mem_free(non_volatile_req);
        return A_ERROR;
    }

    if (req->wait.blocking) {
        while (adf_os_mutex_acquire(pdev->osdev, req->wait.sem_ptr)) {}
    }

    return A_OK;
}
#endif
void
ol_txrx_fw_stats_handler(
    ol_txrx_pdev_handle pdev,
    u_int64_t cookie,
    u_int8_t *stats_info_list)
{
    enum htt_dbg_stats_type type;
    enum htt_dbg_stats_status status;
    int length;
    u_int8_t *stats_data;
    struct ol_txrx_stats_req_internal *req;
    int more = 0;

    req = OL_TXRX_U64_TO_STATS_PTR(cookie);

    do {
        htt_t2h_dbg_stats_hdr_parse(
            stats_info_list, &type, &status, &length, &stats_data);
        if (status == HTT_DBG_STATS_STATUS_SERIES_DONE) {
            break;
        }
        if (status == HTT_DBG_STATS_STATUS_PRESENT ||
            status == HTT_DBG_STATS_STATUS_PARTIAL)
        {
            u_int8_t *buf;
            int bytes = 0;

            if (status == HTT_DBG_STATS_STATUS_PARTIAL) {
                more = 1;
            }
            if (req->base.print.verbose || req->base.print.concise) {
                /* provide the header along with the data */
                htt_t2h_stats_print(stats_info_list, req->base.print.concise);
            }

            switch (type) {
            case HTT_DBG_STATS_WAL_PDEV_TXRX:
                bytes = sizeof(struct wlan_dbg_stats);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(struct wlan_dbg_stats);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;
            case HTT_DBG_STATS_RX_REORDER:
                bytes = sizeof(struct rx_reorder_stats);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(struct rx_reorder_stats);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;
            case HTT_DBG_STATS_RX_RATE_INFO:
                bytes = sizeof(wlan_dbg_rx_rate_info_t);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(wlan_dbg_rx_rate_info_t);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;

            case HTT_DBG_STATS_TX_RATE_INFO:
                bytes = sizeof(wlan_dbg_tx_rate_info_t);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(wlan_dbg_tx_rate_info_t);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;

            case HTT_DBG_STATS_TX_PPDU_LOG:
                bytes = 0; /* TO DO: specify how many bytes are present */
                /* TO DO: add copying to the requestor's buffer */
                break;

            case HTT_DBG_STATS_RX_REMOTE_RING_BUFFER_INFO:

                bytes = sizeof(struct rx_remote_buffer_mgmt_stats);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(struct rx_remote_buffer_mgmt_stats);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;

            case HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT:

                bytes = sizeof(struct rx_txbf_musu_ndpa_pkts_stats);
                if (req->base.copy.buf) {
                    int limit;

                    limit = sizeof(struct rx_txbf_musu_ndpa_pkts_stats);
                    if (req->base.copy.byte_limit < limit) {
                        limit = req->base.copy.byte_limit;
                    }
                    buf = req->base.copy.buf + req->offset;
                    adf_os_mem_copy(buf, stats_data, limit);
                }
                break;

            default:
                break;
            }
            buf = req->base.copy.buf ? req->base.copy.buf : stats_data;
            if (req->base.callback.fp) {
                req->base.callback.fp(
                    req->base.callback.ctxt, type, buf, bytes);
            }
        }
        stats_info_list += length;
    } while (1);

    if (! more) {
        if (req->base.wait.blocking) {
            adf_os_mutex_release(pdev->osdev, req->base.wait.sem_ptr);
        }
        adf_os_mem_free(req);
    }
}

#ifndef ATH_PERF_PWR_OFFLOAD /*---------------------------------------------*/
int ol_txrx_debug(ol_txrx_vdev_handle vdev, int debug_specs)
{
    if (debug_specs & TXRX_DBG_MASK_OBJS) {
        #if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
            ol_txrx_pdev_display(vdev->pdev, 0);
        #else
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
                "The pdev,vdev,peer display functions are disabled.\n"
                "To enable them, recompile with TXRX_DEBUG_LEVEL > 5.\n");
        #endif
    }
    if (debug_specs & TXRX_DBG_MASK_STATS) {
        #if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
            ol_txrx_stats_display(vdev->pdev);
        #else
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
                "txrx stats collection is disabled.\n"
                "To enable it, recompile with TXRX_STATS_LEVEL on.\n");
        #endif
    }
    if (debug_specs & TXRX_DBG_MASK_PROT_ANALYZE) {
        #if defined(ENABLE_TXRX_PROT_ANALYZE)
            ol_txrx_prot_ans_display(vdev->pdev);
        #else
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
                "txrx protocol analysis is disabled.\n"
                "To enable it, recompile with "
                "ENABLE_TXRX_PROT_ANALYZE defined.\n");
        #endif
    }
    if (debug_specs & TXRX_DBG_MASK_RX_REORDER_TRACE) {
        #if defined(ENABLE_RX_REORDER_TRACE)
            ol_rx_reorder_trace_display(vdev->pdev, 0, 0);
        #else
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_FATAL,
                "rx reorder seq num trace is disabled.\n"
                "To enable it, recompile with "
                "ENABLE_RX_REORDER_TRACE defined.\n");
        #endif

    }
    return 0;
}
#endif

int ol_txrx_aggr_cfg(ol_txrx_vdev_handle vdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu)
{
    return htt_h2t_aggr_cfg_msg(vdev->pdev->htt_pdev,
                                max_subfrms_ampdu,
                                max_subfrms_amsdu);
}

#if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
void
ol_txrx_pdev_display(ol_txrx_pdev_handle pdev, int indent)
{
    struct ol_txrx_vdev_t *vdev;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*s%s:\n", indent, " ", "txrx pdev");
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*spdev object: %p\n", indent+4, " ", pdev);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*svdev list:\n", indent+4, " ");
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        ol_txrx_vdev_display(vdev, indent+8);
    }
    ol_txrx_peer_find_display(pdev, indent+4);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*stx desc pool: %d elems @ %p\n", indent+4, " ",
        pdev->tx_desc.pool_size, pdev->tx_desc.array);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW, "\n");
    htt_display(pdev->htt_pdev, indent);
}

void
ol_txrx_vdev_display(ol_txrx_vdev_handle vdev, int indent)
{
    struct ol_txrx_peer_t *peer;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*stxrx vdev: %p\n", indent, " ", vdev);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*sID: %d\n", indent+4, " ", vdev->vdev_id);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*sMAC addr: %d:%d:%d:%d:%d:%d\n",
        indent+4, " ",
        vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
        vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*speer list:\n", indent+4, " ");
    TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
        ol_txrx_peer_display(peer, indent+8);
    }
}

void
ol_txrx_peer_display(ol_txrx_peer_handle peer, int indent)
{
    int i;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
    "%*stxrx peer: %p\n", indent, " ", peer);
    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        if (peer->peer_ids[i] != HTT_INVALID_PEER) {
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
                "%*sID: %d\n", indent+4, " ", peer->peer_ids[i]);
        }
    }
}
#endif /* TXRX_DEBUG_LEVEL */
/**
 * ol_txrx_stats() - update ol layter stats
 * @vdev: pointer to vdev adapter
 * @buffer: pointer to buffer
 * @buf_len: length of the buffer
 * *
 * to update the stats
 *
 * Return: None
 */
void
ol_txrx_stats(ol_txrx_vdev_handle vdev, char *buffer, unsigned buf_len)
{
	snprintf(buffer, buf_len,
		"\nTXRX stats:\n"
		"\nllQueue State : %s"
		"\n pause %u unpause %u"
		"\n overflow %u"
		"\nllQueue timer state : %s\n",
		((vdev->ll_pause.is_q_paused == FALSE) ? "UNPAUSED" : "PAUSED"),
		vdev->ll_pause.q_pause_cnt,
		vdev->ll_pause.q_unpause_cnt,
		vdev->ll_pause.q_overflow_cnt,
		((vdev->ll_pause.is_q_timer_on == FALSE)
			? "NOT-RUNNING" : "RUNNING"));
}

#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
void
ol_txrx_stats_display(ol_txrx_pdev_handle pdev)
{

    adf_os_print("TXRX Stats:\n");
    if (TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_BASIC) {
        adf_os_print(
            "  tx: %lld msdus (%lld B) rejected %lld (%lld B) \n",
            pdev->stats.pub.tx.delivered.pkts,
            pdev->stats.pub.tx.delivered.bytes,
            pdev->stats.pub.tx.dropped.host_reject.pkts,
            pdev->stats.pub.tx.dropped.host_reject.bytes);
    } else { /* full */
        adf_os_print(
            "  tx: sent %lld msdus (%lld B), rejected %lld (%lld B)\n"
            "      dropped %lld (%lld B)\n",
            pdev->stats.pub.tx.delivered.pkts,
            pdev->stats.pub.tx.delivered.bytes,
            pdev->stats.pub.tx.dropped.host_reject.pkts,
            pdev->stats.pub.tx.dropped.host_reject.bytes,
            pdev->stats.pub.tx.dropped.download_fail.pkts
              + pdev->stats.pub.tx.dropped.target_discard.pkts
              + pdev->stats.pub.tx.dropped.no_ack.pkts,
            pdev->stats.pub.tx.dropped.download_fail.bytes
              + pdev->stats.pub.tx.dropped.target_discard.bytes
              + pdev->stats.pub.tx.dropped.no_ack.bytes);
        adf_os_print(
            "    download fail: %lld (%lld B), "
            "target discard: %lld (%lld B), "
            "no ack: %lld (%lld B)\n",
            pdev->stats.pub.tx.dropped.download_fail.pkts,
            pdev->stats.pub.tx.dropped.download_fail.bytes,
            pdev->stats.pub.tx.dropped.target_discard.pkts,
            pdev->stats.pub.tx.dropped.target_discard.bytes,
            pdev->stats.pub.tx.dropped.no_ack.pkts,
            pdev->stats.pub.tx.dropped.no_ack.bytes);
        adf_os_print(
            "Tx completion per interrupt:\n"
            "Single Packet  %d\n"
            " 2-10 Packets  %d\n"
            "11-20 Packets  %d\n"
            "21-30 Packets  %d\n"
            "31-40 Packets  %d\n"
            "41-50 Packets  %d\n"
            "51-60 Packets  %d\n"
            "  60+ Packets  %d\n",
            pdev->stats.pub.tx.comp_histogram.pkts_1,
            pdev->stats.pub.tx.comp_histogram.pkts_2_10,
            pdev->stats.pub.tx.comp_histogram.pkts_11_20,
            pdev->stats.pub.tx.comp_histogram.pkts_21_30,
            pdev->stats.pub.tx.comp_histogram.pkts_31_40,
            pdev->stats.pub.tx.comp_histogram.pkts_41_50,
            pdev->stats.pub.tx.comp_histogram.pkts_51_60,
            pdev->stats.pub.tx.comp_histogram.pkts_61_plus);
    }
    adf_os_print(
        "  rx: %lld ppdus, %lld mpdus, %lld msdus, %lld bytes, %lld errs\n",
        pdev->stats.priv.rx.normal.ppdus,
        pdev->stats.priv.rx.normal.mpdus,
        pdev->stats.pub.rx.delivered.pkts,
        pdev->stats.pub.rx.delivered.bytes,
        pdev->stats.priv.rx.err.mpdu_bad);

    adf_os_print(
        "  fwd to stack %d, fwd to fw %d, fwd to stack & fw  %d\n",
        pdev->stats.pub.rx.intra_bss_fwd.packets_stack,
        pdev->stats.pub.rx.intra_bss_fwd.packets_fwd,
        pdev->stats.pub.rx.intra_bss_fwd.packets_stack_n_fwd);
}

void
ol_txrx_stats_clear(ol_txrx_pdev_handle pdev)
{
    if (TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_BASIC) {
       pdev->stats.pub.tx.delivered.pkts = 0;
       pdev->stats.pub.tx.delivered.bytes = 0;
       pdev->stats.pub.tx.dropped.host_reject.pkts = 0;
       pdev->stats.pub.tx.dropped.host_reject.bytes = 0;
       adf_os_mem_zero(&pdev->stats.pub.rx,
                       sizeof(pdev->stats.pub.rx));
       adf_os_mem_zero(&pdev->stats.priv.rx, sizeof(pdev->stats.priv.rx));
    } else { /* Full */
       adf_os_mem_zero(&pdev->stats, sizeof(pdev->stats));
    }
}

int
ol_txrx_stats_publish(ol_txrx_pdev_handle pdev, struct ol_txrx_stats *buf)
{
    adf_os_assert(buf);
    adf_os_assert(pdev);
    adf_os_mem_copy(buf, &pdev->stats.pub, sizeof(pdev->stats.pub));
    return TXRX_STATS_LEVEL;
}
#endif /* TXRX_STATS_LEVEL */

#if defined(ENABLE_TXRX_PROT_ANALYZE)

void
ol_txrx_prot_ans_display(ol_txrx_pdev_handle pdev)
{
    ol_txrx_prot_an_display(pdev->prot_an_tx_sent);
    ol_txrx_prot_an_display(pdev->prot_an_rx_sent);
}

#endif /* ENABLE_TXRX_PROT_ANALYZE */

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
int16_t
ol_txrx_peer_rssi(ol_txrx_peer_handle peer)
{
    return (peer->rssi_dbm == HTT_RSSI_INVALID) ?
        OL_TXRX_RSSI_INVALID : peer->rssi_dbm;
}
#endif /* #ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI */

#ifdef QCA_ENABLE_OL_TXRX_PEER_STATS
A_STATUS
ol_txrx_peer_stats_copy(
    ol_txrx_pdev_handle pdev,
    ol_txrx_peer_handle peer,
    ol_txrx_peer_stats_t *stats)
{
    adf_os_assert(pdev && peer && stats);
    adf_os_spin_lock_bh(&pdev->peer_stat_mutex);
    adf_os_mem_copy(stats, &peer->stats, sizeof(*stats));
    adf_os_spin_unlock_bh(&pdev->peer_stat_mutex);
    return A_OK;
}
#endif /* QCA_ENABLE_OL_TXRX_PEER_STATS */

void
ol_vdev_rx_set_intrabss_fwd(
    ol_txrx_vdev_handle vdev,
    a_bool_t val)
{
    if (NULL == vdev)
        return;

    vdev->disable_intrabss_fwd = val;
}

#ifdef QCA_LL_TX_FLOW_CT
a_bool_t
ol_txrx_get_tx_resource(
    ol_txrx_vdev_handle vdev,
    unsigned int low_watermark,
    unsigned int high_watermark_offset
)
{
   adf_os_spin_lock_bh(&vdev->pdev->tx_mutex);
#ifdef CONFIG_PER_VDEV_TX_DESC_POOL
   if (((ol_tx_desc_pool_size_hl(vdev->pdev->ctrl_pdev) >> 1)
      - TXRX_HL_TX_FLOW_CTRL_MGMT_RESERVED)
      - adf_os_atomic_read(&vdev->tx_desc_count)
      < (u_int16_t)low_watermark)
#else
   if (vdev->pdev->tx_desc.num_free < (u_int16_t)low_watermark)
#endif
   {
      vdev->tx_fl_lwm = (u_int16_t)low_watermark;
      vdev->tx_fl_hwm = (u_int16_t)(low_watermark + high_watermark_offset);
      /* Not enough free resource, stop TX OS Q */
      adf_os_atomic_set(&vdev->os_q_paused, 1);
      adf_os_spin_unlock_bh(&vdev->pdev->tx_mutex);
      return A_FALSE;
   }
   adf_os_spin_unlock_bh(&vdev->pdev->tx_mutex);
   return A_TRUE;
}

void
ol_txrx_ll_set_tx_pause_q_depth(
    ol_txrx_vdev_handle vdev,
    int pause_q_depth
)
{
    adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
    vdev->ll_pause.max_q_depth = pause_q_depth;
    adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);

    return;
}
#endif /* QCA_LL_TX_FLOW_CT */

/**
 * @brief Setter function to store OCB Peer.
 */
void ol_txrx_set_ocb_peer(struct ol_txrx_pdev_t *pdev, struct ol_txrx_peer_t *peer)
{
    if (pdev == NULL) {
        return;
    }

    pdev->ocb_peer = peer;
    pdev->ocb_peer_valid = (NULL != peer);
}

/**
 * @brief Getter function to retrieve OCB peer.
 * @details
 *      Returns A_TRUE if ocb_peer is valid
 *      Otherwise, returns A_FALSE
 */
a_bool_t ol_txrx_get_ocb_peer(struct ol_txrx_pdev_t *pdev, struct ol_txrx_peer_t **peer)
{
    int rc;

    if ((pdev == NULL) || (peer == NULL)) {
        rc = A_FALSE;
        goto exit;
    }

    if (pdev->ocb_peer_valid) {
        *peer = pdev->ocb_peer;
        rc = A_TRUE;
    } else {
        rc = A_FALSE;
    }

exit:
    return rc;
}

#ifdef IPA_UC_OFFLOAD
void
ol_txrx_ipa_uc_get_resource(
   ol_txrx_pdev_handle pdev,
   u_int32_t *ce_sr_base_paddr,
   u_int32_t *ce_sr_ring_size,
   u_int32_t *ce_reg_paddr,
   u_int32_t *tx_comp_ring_base_paddr,
   u_int32_t *tx_comp_ring_size,
   u_int32_t *tx_num_alloc_buffer,
   u_int32_t *rx_rdy_ring_base_paddr,
   u_int32_t *rx_rdy_ring_size,
   u_int32_t *rx_proc_done_idx_paddr
)
{
    htt_ipa_uc_get_resource(pdev->htt_pdev,
                           ce_sr_base_paddr,
                           ce_sr_ring_size,
                           ce_reg_paddr,
                           tx_comp_ring_base_paddr,
                           tx_comp_ring_size,
                           tx_num_alloc_buffer,
                           rx_rdy_ring_base_paddr,
                           rx_rdy_ring_size,
                           rx_proc_done_idx_paddr);
}

void
ol_txrx_ipa_uc_set_doorbell_paddr(
   ol_txrx_pdev_handle pdev,
   u_int32_t ipa_tx_uc_doorbell_paddr,
   u_int32_t ipa_rx_uc_doorbell_paddr
)
{
    htt_ipa_uc_set_doorbell_paddr(pdev->htt_pdev,
                           ipa_tx_uc_doorbell_paddr,
                           ipa_rx_uc_doorbell_paddr);
}

void
ol_txrx_ipa_uc_set_active(
   ol_txrx_pdev_handle pdev,
   a_bool_t uc_active,
   a_bool_t is_tx
)
{
    htt_h2t_ipa_uc_set_active(pdev->htt_pdev,
                          uc_active,
                          is_tx);
}

void
ol_txrx_ipa_uc_op_response(
   ol_txrx_pdev_handle pdev,
   u_int8_t *op_msg
)
{
   if (pdev->ipa_uc_op_cb) {
      pdev->ipa_uc_op_cb(op_msg, pdev->osif_dev);
   }
}

void ol_txrx_ipa_uc_register_op_cb(
   ol_txrx_pdev_handle pdev,
   ipa_uc_op_cb_type op_cb,
   void *osif_dev)
{
   pdev->ipa_uc_op_cb = op_cb;
   pdev->osif_dev = osif_dev;
}

void ol_txrx_ipa_uc_get_stat(ol_txrx_pdev_handle pdev)
{
   htt_h2t_ipa_uc_get_stats(pdev->htt_pdev);
}

#endif /* IPA_UC_OFFLOAD */

void ol_txrx_display_stats(struct ol_txrx_pdev_t *pdev, uint16_t value)
{

    switch(value)
    {
        case WLAN_TXRX_STATS:
            ol_txrx_stats_display(pdev);
            break;
#ifdef CONFIG_HL_SUPPORT
        case WLAN_SCHEDULER_STATS:
            ol_tx_sched_cur_state_display(pdev);
            ol_tx_sched_stats_display(pdev);
            break;
        case WLAN_TX_QUEUE_STATS:
            ol_tx_queue_log_display(pdev);
            break;
#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
        case WLAN_CREDIT_STATS:
            OL_TX_DUMP_GROUP_CREDIT_STATS(pdev);
            break;
#endif

#ifdef DEBUG_HL_LOGGING
        case WLAN_BUNDLE_STATS:
            htt_dump_bundle_stats(pdev->htt_pdev);
            break;
#endif
#endif
        default:
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                     "%s: Unknown value",__func__);
            break;
    }
}

void ol_txrx_clear_stats(struct ol_txrx_pdev_t *pdev, uint16_t value)
{

    switch(value)
    {
        case WLAN_TXRX_STATS:
            ol_txrx_stats_clear(pdev);
            break;
#ifdef CONFIG_HL_SUPPORT
        case WLAN_SCHEDULER_STATS:
            ol_tx_sched_stats_clear(pdev);
            break;
        case WLAN_TX_QUEUE_STATS:
            ol_tx_queue_log_clear(pdev);
            break;
#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
        case WLAN_CREDIT_STATS:
            OL_TX_CLEAR_GROUP_CREDIT_STATS(pdev);
            break;
#endif
        case WLAN_BUNDLE_STATS:
            htt_clear_bundle_stats(pdev->htt_pdev);
            break;
#endif
        default:
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                                          "%s: Unknown value",__func__);
            break;
    }
}

