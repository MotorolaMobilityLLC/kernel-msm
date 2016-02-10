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

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>        /* u_int32_t, etc. */
#include <adf_os_mem.h>   /* adf_os_mem_alloc, etc. */
#include <adf_os_types.h> /* adf_os_device_t, adf_os_print */
/* header files for utilities */
#include <queue.h>        /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>       /* ol_cfg_max_peer_id */

/* header files for our internal definitions */
#include <ol_txrx_api.h>       /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_dbg.h>       /* TXRX_DEBUG_LEVEL */
#include <ol_txrx_internal.h>  /* ol_txrx_pdev_t, etc. */
#include <ol_txrx.h>           /* ol_txrx_peer_unref_delete */
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_attach, etc. */
#include <ol_tx_queue.h>

/*=== misc. / utility function definitions ==================================*/

static int
ol_txrx_log2_ceil(unsigned value)
{
    /* need to switch to unsigned math so that negative values
     * will right-shift towards 0 instead of -1
     */
    unsigned tmp = value;
    int log2 = -1;

    if (value == 0)
    {
        TXRX_ASSERT2(0);
        return 0;
    }

    while (tmp) {
        log2++;
        tmp >>= 1;
    }
    if (1U << log2 != value) {
        log2++;
    }

    return log2;
}

static int
ol_txrx_peer_find_add_id_to_obj(
    struct ol_txrx_peer_t *peer,
    u_int16_t peer_id)
{
    int i;

    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        if (peer->peer_ids[i] == HTT_INVALID_PEER) {
            peer->peer_ids[i] = peer_id;
            return 0; /* success */
        }
    }
    return 1; /* failure */
}

/*=== function definitions for peer MAC addr --> peer object hash table =====*/

/*
 * TXRX_PEER_HASH_LOAD_FACTOR:
 * Multiply by 2 and divide by 2^0 (shift by 0), then round up to a
 * power of two.
 * This provides at least twice as many bins in the peer hash table
 * as there will be entries.
 * Having substantially more bins than spaces minimizes the probability of
 * having to compare MAC addresses.
 * Because the MAC address comparison is fairly efficient, it is okay if the
 * hash table is sparsely loaded, but it's generally better to use extra mem
 * to keep the table sparse, to keep the lookups as fast as possible.
 * An optimization would be to apply a more conservative loading factor for
 * high latency, where the lookup happens during the tx classification of
 * every tx frame, than for low-latency, where the lookup only happens
 * during association, when the PEER_MAP message is received.
 */
#define TXRX_PEER_HASH_LOAD_MULT  2
#define TXRX_PEER_HASH_LOAD_SHIFT 0

static int
ol_txrx_peer_find_hash_attach(struct ol_txrx_pdev_t *pdev)
{
    int i, hash_elems, log2;

    /* allocate the peer MAC address -> peer object hash table */
    hash_elems = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
    hash_elems *= TXRX_PEER_HASH_LOAD_MULT;
    hash_elems >>= TXRX_PEER_HASH_LOAD_SHIFT;
    log2 = ol_txrx_log2_ceil(hash_elems);
    hash_elems = 1 << log2;

    pdev->peer_hash.mask = hash_elems - 1;
    pdev->peer_hash.idx_bits = log2;
    /* allocate an array of TAILQ peer object lists */
    pdev->peer_hash.bins = adf_os_mem_alloc(
        pdev->osdev,
        hash_elems * sizeof(TAILQ_HEAD(anonymous_tail_q, ol_txrx_peer_t)));
    if (!pdev->peer_hash.bins) {
        return 1; /* failure */
    }
    for (i = 0; i < hash_elems; i++) {
        TAILQ_INIT(&pdev->peer_hash.bins[i]);
    }
    return 0; /* success */
}

static void
ol_txrx_peer_find_hash_detach(struct ol_txrx_pdev_t *pdev)
{
    adf_os_mem_free(pdev->peer_hash.bins);
}

static inline unsigned
ol_txrx_peer_find_hash_index(
    struct ol_txrx_pdev_t *pdev,
    union ol_txrx_align_mac_addr_t *mac_addr)
{
    unsigned index;

    index =
        mac_addr->align2.bytes_ab ^
        mac_addr->align2.bytes_cd ^
        mac_addr->align2.bytes_ef;
    index ^= index >> pdev->peer_hash.idx_bits;
    index &= pdev->peer_hash.mask;
    return index;
}


void
ol_txrx_peer_find_hash_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    unsigned index;

    index = ol_txrx_peer_find_hash_index(pdev, &peer->mac_addr);
    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    /*
     * It is important to add the new peer at the tail of the peer list
     * with the bin index.  Together with having the hash_find function
     * search from head to tail, this ensures that if two entries with
     * the same MAC address are stored, the one added first will be
     * found first.
     */
    TAILQ_INSERT_TAIL(&pdev->peer_hash.bins[index], peer, hash_list_elem);
    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
}

struct ol_txrx_peer_t *
ol_txrx_peer_vdev_find_hash(struct ol_txrx_pdev_t *pdev,
                            struct ol_txrx_vdev_t *vdev,
                            u_int8_t *peer_mac_addr,
                            int mac_addr_is_aligned,
                            u_int8_t check_valid)
{
    union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
    unsigned index;
    struct ol_txrx_peer_t *peer;

    if (mac_addr_is_aligned) {
        mac_addr = (union ol_txrx_align_mac_addr_t *) peer_mac_addr;
    } else {
        adf_os_mem_copy(
            &local_mac_addr_aligned.raw[0],
            peer_mac_addr, OL_TXRX_MAC_ADDR_LEN);
        mac_addr = &local_mac_addr_aligned;
    }
    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    TAILQ_FOREACH(peer, &pdev->peer_hash.bins[index], hash_list_elem) {
        if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0
            && (check_valid == 0 || peer->valid) && peer->vdev == vdev )  {
            /* found it - increment the ref count before releasing the lock */
            adf_os_atomic_inc(&peer->ref_cnt);
            adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
            return peer;
        }
    }
    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
    return NULL; /* failure */
}

struct ol_txrx_peer_t *
ol_txrx_peer_find_hash_find(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    int mac_addr_is_aligned,
    u_int8_t check_valid)
{
    union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
    unsigned index;
    struct ol_txrx_peer_t *peer;

    if (mac_addr_is_aligned) {
        mac_addr = (union ol_txrx_align_mac_addr_t *) peer_mac_addr;
    } else {
        adf_os_mem_copy(
            &local_mac_addr_aligned.raw[0],
            peer_mac_addr, OL_TXRX_MAC_ADDR_LEN);
        mac_addr = &local_mac_addr_aligned;
    }
    index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
    adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    TAILQ_FOREACH(peer, &pdev->peer_hash.bins[index], hash_list_elem) {
        if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) == 0
            && (check_valid == 0 || peer->valid)) {
            /* found it - increment the ref count before releasing the lock */
            adf_os_atomic_inc(&peer->ref_cnt);
            adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
            return peer;
        }
    }
    adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
    return NULL; /* failure */
}

void
ol_txrx_peer_find_hash_remove(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    unsigned index;

    index = ol_txrx_peer_find_hash_index(pdev, &peer->mac_addr);
    /*
     * DO NOT take the peer_ref_mutex lock here - it needs to be taken
     * by the caller.
     * The caller needs to hold the lock from the time the peer object's
     * reference count is decremented and tested up through the time the
     * reference to the peer object is removed from the hash table, by
     * this function.
     * Holding the lock only while removing the peer object reference
     * from the hash table keeps the hash table consistent, but does not
     * protect against a new HL tx context starting to use the peer object
     * if it looks up the peer object from its MAC address just after the
     * peer ref count is decremented to zero, but just before the peer
     * object reference is removed from the hash table.
     */
    //adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
    TAILQ_REMOVE(&pdev->peer_hash.bins[index], peer, hash_list_elem);
    //adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
}

void
ol_txrx_peer_find_hash_erase(struct ol_txrx_pdev_t *pdev)
{
    unsigned i;
    /*
     * Not really necessary to take peer_ref_mutex lock - by this point,
     * it's known that the pdev is no longer in use.
     */

    for (i = 0; i <= pdev->peer_hash.mask; i++) {
        if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
            struct ol_txrx_peer_t *peer, *peer_next;

			/*
			 * TAILQ_FOREACH_SAFE must be used here to avoid any memory access
			 * violation after peer is freed
			 */
			TAILQ_FOREACH_SAFE(
                peer, &pdev->peer_hash.bins[i], hash_list_elem, peer_next)
			{
				/*
                 * Don't remove the peer from the hash table -
                 * that would modify the list we are currently traversing,
                 * and it's not necessary anyway.
                 */
                /*
                 * Artificially adjust the peer's ref count to 1, so it
                 * will get deleted by ol_txrx_peer_unref_delete.
                 */
                adf_os_atomic_init(&peer->ref_cnt); /* set to zero */
                adf_os_atomic_inc(&peer->ref_cnt);  /* incr to one */
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "%s: Delete Peer %p\n", __func__, peer);
                ol_txrx_peer_unref_delete(peer);
            }
        }
    }
}

/*=== function definitions for peer id --> peer object map ==================*/

static int
ol_txrx_peer_find_map_attach(struct ol_txrx_pdev_t *pdev)
{
    int max_peers, peer_map_size;

    /* allocate the peer ID -> peer object map */
    max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
    peer_map_size = max_peers * sizeof(pdev->peer_id_to_obj_map[0]);
    pdev->peer_id_to_obj_map = adf_os_mem_alloc(pdev->osdev, peer_map_size);
    if (!pdev->peer_id_to_obj_map) {
        return 1; /* failure */
    }

    /*
     * The peer_id_to_obj_map doesn't really need to be initialized,
     * since elements are only used after they have been individually
     * initialized.
     * However, it is convenient for debugging to have all elements
     * that are not in use set to 0.
     */
    adf_os_mem_set(pdev->peer_id_to_obj_map, 0, peer_map_size);

    return 0; /* success */
}

static void
ol_txrx_peer_find_map_detach(struct ol_txrx_pdev_t *pdev)
{
    adf_os_mem_free(pdev->peer_id_to_obj_map);
}

static inline void
ol_txrx_peer_find_add_id(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t *peer_mac_addr,
    u_int16_t peer_id)
{
    struct ol_txrx_peer_t *peer;

    /* check if there's already a peer object with this MAC address */
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 1 /* is aligned */, 0);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "%s: peer %p ID %d\n", __func__, peer, peer_id);
    if (peer) {
        /* peer's ref count was already incremented by peer_find_hash_find */
        pdev->peer_id_to_obj_map[peer_id] = peer;
        /*
         * remove the reference added in ol_txrx_peer_find_hash_find.
         * the reference for the first peer id is already added in ol_txrx_peer_attach.
         * Riva/Pronto has one peer id for each peer.
         * Peregrine/Rome has two peer id for each peer.
         */
        if (peer->peer_ids[0] == HTT_INVALID_PEER) {
            ol_txrx_peer_unref_delete(peer);
        }
        if (ol_txrx_peer_find_add_id_to_obj(peer, peer_id)) {
            /* TBDXXX: assert for now */
            adf_os_assert(0);
        }
        return;
    }
    /*
     * Currently peer IDs are assigned for vdevs as well as peers.
     * If the peer ID is for a vdev, then we will fail to find a peer
     * with a matching MAC address.
     */
    //TXRX_ASSERT2(0);
}

/*=== allocation / deallocation function definitions ========================*/

int
ol_txrx_peer_find_attach(struct ol_txrx_pdev_t *pdev)
{
    if (ol_txrx_peer_find_map_attach(pdev)) {
        return 1;
    }
    if (ol_txrx_peer_find_hash_attach(pdev)) {
        ol_txrx_peer_find_map_detach(pdev);
        return 1;
    }
    return 0; /* success */
}

void
ol_txrx_peer_find_detach(struct ol_txrx_pdev_t *pdev)
{
    ol_txrx_peer_find_map_detach(pdev);
    ol_txrx_peer_find_hash_detach(pdev);
}

/*=== function definitions for message handling =============================*/

void
ol_rx_peer_map_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tx_ready)
{
    ol_txrx_peer_find_add_id(pdev, peer_mac_addr, peer_id);
    if (pdev->cfg.is_high_latency && !tx_ready) {
        struct ol_txrx_peer_t *peer;
        peer = ol_txrx_peer_find_by_id(pdev, peer_id);
        if (!peer) {
            /* ol_txrx_peer_detach called before peer map arrived */
            return;
        }else {
            if (tx_ready) {
#if defined(CONFIG_HL_SUPPORT)
                int i;
                /* unpause all tx queues now, since the target is ready */
                for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
                    ol_txrx_peer_tid_unpause(peer, i);
                }
#endif
            } else {
                /* walk through paused mgmt queue, update tx descriptors */
                ol_tx_queue_decs_reinit(peer, peer_id);

                /* keep non-mgmt tx queues paused until assoc is finished */
                /* tx queues were paused in ol_txrx_peer_attach*/
                /* unpause tx mgmt queue */
                ol_txrx_peer_tid_unpause(peer, HTT_TX_EXT_TID_MGMT);
            }
        }
    }
}

void
ol_txrx_peer_tx_ready_handler(ol_txrx_pdev_handle pdev, u_int16_t peer_id)
{
#if defined(CONFIG_HL_SUPPORT)
    struct ol_txrx_peer_t *peer;
    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer) {
        int i;
        /*
         * Unpause all data tx queues now that the target is ready.
         * The mgmt tx queue was not paused, so skip it.
         */
        for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
            if (i == HTT_TX_EXT_TID_MGMT) {
                continue; /* mgmt tx queue was not paused */
            }
            ol_txrx_peer_tid_unpause(peer, i);
        }
    }
#endif
}

void
ol_rx_peer_unmap_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id)
{
    struct ol_txrx_peer_t *peer;
    peer = (peer_id == HTT_INVALID_PEER) ? NULL :
        pdev->peer_id_to_obj_map[peer_id];
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "%s: peer %p with ID %d to be unmapped.\n", __func__, peer, peer_id);
    pdev->peer_id_to_obj_map[peer_id] = NULL;
    /*
     * Currently peer IDs are assigned for vdevs as well as peers.
     * If the peer ID is for a vdev, then the peer pointer stored
     * in peer_id_to_obj_map will be NULL.
     */
    if (!peer) return;
    /*
     * Remove a reference to the peer.
     * If there are no more references, delete the peer object.
     */
    TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
        "%s: Remove the ID %d reference to peer %p\n",
        __func__, peer_id, peer);
    ol_txrx_peer_unref_delete(peer);
}

struct ol_txrx_peer_t *
ol_txrx_assoc_peer_find(struct ol_txrx_vdev_t *vdev)
{
	struct ol_txrx_peer_t *peer;

	adf_os_spin_lock_bh(&vdev->pdev->last_real_peer_mutex);
	/*
	 * Check the TXRX Peer is itself valid And also
	 * if HTT Peer ID has been setup for this peer
	 */
	if (vdev->last_real_peer && vdev->last_real_peer->peer_ids[0] != HTT_INVALID_PEER_ID)
	{
		adf_os_atomic_inc(&vdev->last_real_peer->ref_cnt);
		peer = vdev->last_real_peer;
	} else {
		peer = NULL;
	}
	adf_os_spin_unlock_bh(&vdev->pdev->last_real_peer_mutex);
	return peer;
}

/*=== function definitions for debug ========================================*/

#if defined (TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
void
ol_txrx_peer_find_display(ol_txrx_pdev_handle pdev, int indent)
{
    int i, max_peers;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*speer map:\n", indent, " ");
    max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
    for (i = 0; i < max_peers; i++) {
        if (pdev->peer_id_to_obj_map[i]) {
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
                "%*sid %d -> %p\n",
                indent+4, " ", i, pdev->peer_id_to_obj_map[i]);
        }
    }
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*speer hash table:\n", indent, " ");
    for (i = 0; i <= pdev->peer_hash.mask; i++) {
        if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
            struct ol_txrx_peer_t *peer;
            TAILQ_FOREACH(peer, &pdev->peer_hash.bins[i], hash_list_elem) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
                    "%*shash idx %d -> %p (%02x:%02x:%02x:%02x:%02x:%02x)\n",
                    indent+4, " ", i, peer,
                    peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                    peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                    peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
            }
        }
    }
}
#endif /* if TXRX_DEBUG_LEVEL */
