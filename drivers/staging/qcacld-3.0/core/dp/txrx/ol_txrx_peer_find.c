/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>              /* uint32_t, etc. */
#include <qdf_mem.h>         /* qdf_mem_malloc, etc. */
#include <qdf_types.h>          /* qdf_device_t, qdf_print */
/* header files for utilities */
#include "queue.h"         /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>             /* ol_cfg_max_peer_id */

/* header files for our internal definitions */
#include <ol_txrx_api.h>        /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_dbg.h>        /* TXRX_DEBUG_LEVEL */
#include <ol_txrx_internal.h>   /* ol_txrx_pdev_t, etc. */
#include <ol_txrx.h>            /* ol_txrx_peer_release_ref */
#include <ol_txrx_peer_find.h>  /* ol_txrx_peer_find_attach, etc. */
#include <ol_tx_queue.h>
#include "wlan_roam_debug.h"

/*=== misc. / utility function definitions ==================================*/

static int ol_txrx_log2_ceil(unsigned int value)
{
	/* need to switch to unsigned math so that negative values
	 * will right-shift towards 0 instead of -1
	 */
	unsigned int tmp = value;
	int log2 = -1;

	if (value == 0) {
		TXRX_ASSERT2(0);
		return 0;
	}

	while (tmp) {
		log2++;
		tmp >>= 1;
	}
	if (1U << log2 != value)
		log2++;

	return log2;
}

int ol_txrx_peer_get_ref(struct ol_txrx_peer_t *peer,
			  enum peer_debug_id_type dbg_id)
{
	int refs_dbg_id;

	if (!peer) {
		ol_txrx_err("peer is null for ID %d", dbg_id);
		return -EINVAL;
	}

	if (dbg_id >= PEER_DEBUG_ID_MAX || dbg_id < 0) {
		ol_txrx_err("incorrect debug_id %d ", dbg_id);
		return -EINVAL;
	}

	qdf_atomic_inc(&peer->ref_cnt);
	qdf_atomic_inc(&peer->access_list[dbg_id]);
	refs_dbg_id = qdf_atomic_read(&peer->access_list[dbg_id]);

	return refs_dbg_id;
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

static int ol_txrx_peer_find_hash_attach(struct ol_txrx_pdev_t *pdev)
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
	pdev->peer_hash.bins =
		qdf_mem_malloc(hash_elems *
			       sizeof(TAILQ_HEAD(anonymous_tail_q,
						 ol_txrx_peer_t)));
	if (!pdev->peer_hash.bins)
		return 1;       /* failure */

	for (i = 0; i < hash_elems; i++)
		TAILQ_INIT(&pdev->peer_hash.bins[i]);

	return 0;               /* success */
}

static void ol_txrx_peer_find_hash_detach(struct ol_txrx_pdev_t *pdev)
{
	qdf_mem_free(pdev->peer_hash.bins);
}

static inline unsigned int
ol_txrx_peer_find_hash_index(struct ol_txrx_pdev_t *pdev,
			     union ol_txrx_align_mac_addr_t *mac_addr)
{
	unsigned int index;

	index =
		mac_addr->align2.bytes_ab ^
		mac_addr->align2.bytes_cd ^ mac_addr->align2.bytes_ef;
	index ^= index >> pdev->peer_hash.idx_bits;
	index &= pdev->peer_hash.mask;
	return index;
}

void
ol_txrx_peer_find_hash_add(struct ol_txrx_pdev_t *pdev,
			   struct ol_txrx_peer_t *peer)
{
	unsigned int index;

	index = ol_txrx_peer_find_hash_index(pdev, &peer->mac_addr);
	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	/*
	 * It is important to add the new peer at the tail of the peer list
	 * with the bin index.  Together with having the hash_find function
	 * search from head to tail, this ensures that if two entries with
	 * the same MAC address are stored, the one added first will be
	 * found first.
	 */
	TAILQ_INSERT_TAIL(&pdev->peer_hash.bins[index], peer, hash_list_elem);
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
}

struct ol_txrx_peer_t *ol_txrx_peer_vdev_find_hash(struct ol_txrx_pdev_t *pdev,
						   struct ol_txrx_vdev_t *vdev,
						   uint8_t *peer_mac_addr,
						   int mac_addr_is_aligned,
						   uint8_t check_valid)
{
	union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
	unsigned int index;
	struct ol_txrx_peer_t *peer;

	if (mac_addr_is_aligned) {
		mac_addr = (union ol_txrx_align_mac_addr_t *)peer_mac_addr;
	} else {
		qdf_mem_copy(&local_mac_addr_aligned.raw[0],
			     peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}
	index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	TAILQ_FOREACH(peer, &pdev->peer_hash.bins[index], hash_list_elem) {
		if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) ==
		    0 && (check_valid == 0 || peer->valid)
		    && peer->vdev == vdev) {
			/* found it */
			ol_txrx_peer_get_ref(peer, PEER_DEBUG_ID_OL_INTERNAL);
			qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
			return peer;
		}
	}
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
	return NULL;            /* failure */
}

struct ol_txrx_peer_t *
	ol_txrx_peer_find_hash_find_get_ref
				(struct ol_txrx_pdev_t *pdev,
				uint8_t *peer_mac_addr,
				int mac_addr_is_aligned,
				u8 check_valid,
				enum peer_debug_id_type dbg_id)
{
	union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;
	unsigned int index;
	struct ol_txrx_peer_t *peer;

	if (mac_addr_is_aligned) {
		mac_addr = (union ol_txrx_align_mac_addr_t *)peer_mac_addr;
	} else {
		qdf_mem_copy(&local_mac_addr_aligned.raw[0],
			     peer_mac_addr, QDF_MAC_ADDR_SIZE);
		mac_addr = &local_mac_addr_aligned;
	}
	index = ol_txrx_peer_find_hash_index(pdev, mac_addr);
	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	TAILQ_FOREACH(peer, &pdev->peer_hash.bins[index], hash_list_elem) {
		if (ol_txrx_peer_find_mac_addr_cmp(mac_addr, &peer->mac_addr) ==
		    0 && (check_valid == 0 || peer->valid)) {
			/* found it */
			ol_txrx_peer_get_ref(peer, dbg_id);
			qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
			return peer;
		}
	}
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
	return NULL;            /* failure */
}

void
ol_txrx_peer_find_hash_remove(struct ol_txrx_pdev_t *pdev,
			      struct ol_txrx_peer_t *peer)
{
	unsigned int index;

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
	/* qdf_spin_lock_bh(&pdev->peer_ref_mutex); */
	TAILQ_REMOVE(&pdev->peer_hash.bins[index], peer, hash_list_elem);
	/* qdf_spin_unlock_bh(&pdev->peer_ref_mutex); */
}

void ol_txrx_peer_find_hash_erase(struct ol_txrx_pdev_t *pdev)
{
	unsigned int i;
	/*
	 * Not really necessary to take peer_ref_mutex lock - by this point,
	 * it's known that the pdev is no longer in use.
	 */

	for (i = 0; i <= pdev->peer_hash.mask; i++) {
		if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
			struct ol_txrx_peer_t *peer, *peer_next;

			/*
			 * TAILQ_FOREACH_SAFE must be used here to avoid any
			 * memory access violation after peer is freed
			 */
			TAILQ_FOREACH_SAFE(peer, &pdev->peer_hash.bins[i],
					   hash_list_elem, peer_next) {
				/*
				 * Don't remove the peer from the hash table -
				 * that would modify the list we are currently
				 * traversing,
				 * and it's not necessary anyway.
				 */
				/*
				 * Artificially adjust the peer's ref count to
				 * 1, so it will get deleted by
				 * ol_txrx_peer_release_ref.
				 */
				qdf_atomic_init(&peer->ref_cnt); /* set to 0 */
				ol_txrx_peer_get_ref(peer,
						     PEER_DEBUG_ID_OL_HASH_ERS);
				ol_txrx_peer_release_ref(peer,
						     PEER_DEBUG_ID_OL_HASH_ERS);
			}
		}
	}
}

/*=== function definitions for peer id --> peer object map ==================*/

static int ol_txrx_peer_find_map_attach(struct ol_txrx_pdev_t *pdev)
{
	int max_peers, peer_map_size;

	/* allocate the peer ID -> peer object map */
	max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
	peer_map_size = max_peers * sizeof(pdev->peer_id_to_obj_map[0]);
	pdev->peer_id_to_obj_map = qdf_mem_malloc(peer_map_size);
	if (!pdev->peer_id_to_obj_map)
		return 1;       /* failure */

	return 0;               /* success */
}

static void ol_txrx_peer_find_map_detach(struct ol_txrx_pdev_t *pdev)
{
	qdf_mem_free(pdev->peer_id_to_obj_map);
}

/**
 * ol_txrx_peer_clear_map_peer() - Remove map entries that refer to a peer.
 * @pdev: pdev handle
 * @peer: peer for removing obj map entries
 *
 * Run through the entire peer_id_to_obj map and nullify all the entries
 * that map to a particular peer. Called before deleting the peer object.
 *
 * Return: None
 */
void ol_txrx_peer_clear_map_peer(ol_txrx_pdev_handle pdev,
				 struct ol_txrx_peer_t *peer)
{
	int max_peers;
	int i;

	max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;

	qdf_spin_lock_bh(&pdev->peer_map_unmap_lock);
	for (i = 0; i < max_peers; i++) {
		if (pdev->peer_id_to_obj_map[i].peer == peer) {
			/* Found a map entry for this peer, clear it. */
			pdev->peer_id_to_obj_map[i].peer = NULL;
		}
	}
	qdf_spin_unlock_bh(&pdev->peer_map_unmap_lock);
}

/*
 * ol_txrx_peer_find_add_id() - Add peer_id entry to peer
 *
 * @pdev: Handle to pdev object
 * @peer_mac_addr: MAC address of peer provided by firmware
 * @peer_id: peer_id provided by firmware
 *
 * Search for peer object for the MAC address, add the peer_id to
 * its array of peer_id's and update the peer_id_to_obj map entry
 * for that peer_id. Increment corresponding reference counts.
 *
 * Riva/Pronto has one peer id for each peer.
 * Peregrine/Rome has two peer id for each peer.
 * iHelium has upto three peer id for each peer.
 *
 * Return: None
 */
static inline void ol_txrx_peer_find_add_id(struct ol_txrx_pdev_t *pdev,
				uint8_t *peer_mac_addr, uint16_t peer_id)
{
	struct ol_txrx_peer_t *peer;
	int status;
	int i;
	uint32_t peer_id_ref_cnt;
	uint32_t peer_ref_cnt;
	u8 check_valid = 0;

	if (pdev->enable_peer_unmap_conf_support)
		check_valid = 1;

	/* check if there's already a peer object with this MAC address */
	peer =
		ol_txrx_peer_find_hash_find_get_ref(pdev, peer_mac_addr,
						    1 /* is aligned */,
						    check_valid,
						    PEER_DEBUG_ID_OL_PEER_MAP);

	if (!peer || peer_id == HTT_INVALID_PEER) {
		/*
		 * Currently peer IDs are assigned for vdevs as well as peers.
		 * If the peer ID is for a vdev, then we will fail to find a
		 * peer with a matching MAC address.
		 */
		ol_txrx_err("peer not found or peer ID is %d invalid",
			    peer_id);
		wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
				    DEBUG_PEER_MAP_EVENT,
				    peer_id, peer_mac_addr,
				    peer, 0, 0);

		return;
	}

	qdf_spin_lock(&pdev->peer_map_unmap_lock);

	/* peer's ref count was already incremented by
	 * peer_find_hash_find
	 */
	if (!pdev->peer_id_to_obj_map[peer_id].peer) {
		pdev->peer_id_to_obj_map[peer_id].peer = peer;
		qdf_atomic_init
		  (&pdev->peer_id_to_obj_map[peer_id].peer_id_ref_cnt);
	}
	qdf_atomic_inc
		(&pdev->peer_id_to_obj_map[peer_id].peer_id_ref_cnt);

	status = 1;

	/* find a place in peer_id array and insert peer_id */
	for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
		if (peer->peer_ids[i] == HTT_INVALID_PEER) {
			peer->peer_ids[i] = peer_id;
			status = 0;
			break;
		}
	}

	if (qdf_atomic_read(&peer->fw_create_pending) == 1) {
		qdf_atomic_set(&peer->fw_create_pending, 0);
	}

	qdf_spin_unlock(&pdev->peer_map_unmap_lock);

	peer_id_ref_cnt = qdf_atomic_read(&pdev->
				peer_id_to_obj_map[peer_id].peer_id_ref_cnt);
	peer_ref_cnt = qdf_atomic_read(&peer->ref_cnt);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_HIGH,
	   "%s: peer %pK ID %d peer_id[%d] peer_id_ref_cnt %d peer->ref_cnt %d",
	   __func__, peer, peer_id, i, peer_id_ref_cnt, peer_ref_cnt);
	wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
			    DEBUG_PEER_MAP_EVENT,
			    peer_id, &peer->mac_addr.raw, peer,
			    peer_id_ref_cnt,
			    peer_ref_cnt);


	if (status) {
		/* TBDXXX: assert for now */
		qdf_assert(0);
	}
}

/*=== allocation / deallocation function definitions ========================*/

int ol_txrx_peer_find_attach(struct ol_txrx_pdev_t *pdev)
{
	if (ol_txrx_peer_find_map_attach(pdev))
		return 1;
	if (ol_txrx_peer_find_hash_attach(pdev)) {
		ol_txrx_peer_find_map_detach(pdev);
		return 1;
	}
	return 0;               /* success */
}

void ol_txrx_peer_find_detach(struct ol_txrx_pdev_t *pdev)
{
	ol_txrx_peer_find_map_detach(pdev);
	ol_txrx_peer_find_hash_detach(pdev);
}

/**
 * ol_txrx_peer_unmap_conf_handler() - send peer unmap conf cmd to FW
 * @pdev: pdev_handle
 * @peer_id: peer_id
 *
 * Return: None
 */
static inline void
ol_txrx_peer_unmap_conf_handler(ol_txrx_pdev_handle pdev,
				uint16_t peer_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (peer_id == HTT_INVALID_PEER) {
		ol_txrx_err(
		   "invalid peer ID %d\n", peer_id);
		return;
	}

	qdf_atomic_inc(&pdev->peer_id_to_obj_map[peer_id].peer_id_unmap_cnt);

	if (qdf_atomic_read(
		&pdev->peer_id_to_obj_map[peer_id].peer_id_unmap_cnt) ==
		pdev->peer_id_unmap_ref_cnt) {
		ol_txrx_dbg("send unmap conf cmd: peer_id[%d] unmap_cnt[%d]",
			    peer_id, pdev->peer_id_unmap_ref_cnt);
		status = pdev->peer_unmap_sync_cb(
				DEBUG_INVALID_VDEV_ID,
				1, &peer_id);

		if (status == QDF_STATUS_SUCCESS ||
		    status == QDF_STATUS_E_BUSY) {
			qdf_atomic_init(
			&pdev->peer_id_to_obj_map[peer_id].peer_id_unmap_cnt);
		} else {
			qdf_atomic_set(
			&pdev->peer_id_to_obj_map[peer_id].peer_id_unmap_cnt,
			OL_TXRX_INVALID_PEER_UNMAP_COUNT);
			ol_txrx_err("unable to send unmap conf cmd [%d]",
				    peer_id);
		}

	}
}

/*=== function definitions for message handling =============================*/

#if defined(CONFIG_HL_SUPPORT)

void
ol_rx_peer_map_handler(ol_txrx_pdev_handle pdev,
		       uint16_t peer_id,
		       uint8_t vdev_id, uint8_t *peer_mac_addr, int tx_ready)
{
	ol_txrx_peer_find_add_id(pdev, peer_mac_addr, peer_id);
	if (!tx_ready) {
		struct ol_txrx_peer_t *peer;

		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (!peer) {
			/* ol_txrx_peer_detach called before peer map arrived*/
			return;
		} else {
			if (tx_ready) {
				int i;

				/* unpause all tx queues now, since the
				 * target is ready
				 */
				for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs);
									i++)
					ol_txrx_peer_tid_unpause(peer, i);

			} else {
				/* walk through paused mgmt queue,
				 * update tx descriptors
				 */
				ol_tx_queue_decs_reinit(peer, peer_id);

				/* keep non-mgmt tx queues paused until assoc
				 * is finished tx queues were paused in
				 * ol_txrx_peer_attach
				 */
				/* unpause tx mgmt queue */
				ol_txrx_peer_tid_unpause(peer,
							 HTT_TX_EXT_TID_MGMT);
			}
		}
	}
}

void ol_txrx_peer_tx_ready_handler(ol_txrx_pdev_handle pdev, uint16_t peer_id)
{
	struct ol_txrx_peer_t *peer;

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (peer) {
		int i;
		/*
		 * Unpause all data tx queues now that the target is ready.
		 * The mgmt tx queue was not paused, so skip it.
		 */
		for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs); i++) {
			if (i == HTT_TX_EXT_TID_MGMT)
				continue; /* mgmt tx queue was not paused */

			ol_txrx_peer_tid_unpause(peer, i);
		}
	}
}
#else

void
ol_rx_peer_map_handler(ol_txrx_pdev_handle pdev,
		       uint16_t peer_id,
		       uint8_t vdev_id,
		       uint8_t *peer_mac_addr,
		       int tx_ready)
{
	ol_txrx_peer_find_add_id(pdev, peer_mac_addr, peer_id);
}

void ol_txrx_peer_tx_ready_handler(ol_txrx_pdev_handle pdev, uint16_t peer_id)
{
}

#endif

/*
 * ol_rx_peer_unmap_handler() - Handle peer unmap event from firmware
 *
 * @pdev: Handle to pdev pbject
 * @peer_id: peer_id unmapped by firmware
 *
 * Decrement reference count for the peer_id in peer_id_to_obj_map,
 * decrement reference count in corresponding peer object and clear the entry
 * in peer's peer_ids array.
 * In case of unmap events for a peer that is already deleted, just decrement
 * del_peer_id_ref_cnt.
 *
 * Return: None
 */
void ol_rx_peer_unmap_handler(ol_txrx_pdev_handle pdev, uint16_t peer_id)
{
	struct ol_txrx_peer_t *peer;
	int i = 0;
	int32_t ref_cnt;

	if (peer_id == HTT_INVALID_PEER) {
		ol_txrx_err(
		   "invalid peer ID %d\n", peer_id);
		wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
				    DEBUG_PEER_UNMAP_EVENT,
				    peer_id, NULL, NULL, 0, 0x100);
		return;
	}

	qdf_spin_lock_bh(&pdev->peer_map_unmap_lock);

	/* send peer unmap conf cmd to fw for unmapped peer_ids */
	if (pdev->enable_peer_unmap_conf_support &&
	    pdev->peer_unmap_sync_cb)
		ol_txrx_peer_unmap_conf_handler(pdev, peer_id);

	if (qdf_atomic_read(
		&pdev->peer_id_to_obj_map[peer_id].del_peer_id_ref_cnt)) {
		/* This peer_id belongs to a peer already deleted */
		qdf_atomic_dec(&pdev->peer_id_to_obj_map[peer_id].
					del_peer_id_ref_cnt);
		ref_cnt = qdf_atomic_read(&pdev->peer_id_to_obj_map[peer_id].
							del_peer_id_ref_cnt);
		qdf_spin_unlock_bh(&pdev->peer_map_unmap_lock);
		wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
				    DEBUG_PEER_UNMAP_EVENT,
				    peer_id, NULL, NULL, ref_cnt, 0x101);
		ol_txrx_dbg("peer already deleted, peer_id %d del_peer_id_ref_cnt %d",
			    peer_id, ref_cnt);
		return;
	}
	peer = pdev->peer_id_to_obj_map[peer_id].peer;

	if (!peer) {
		/*
		 * Currently peer IDs are assigned for vdevs as well as peers.
		 * If the peer ID is for a vdev, then the peer pointer stored
		 * in peer_id_to_obj_map will be NULL.
		 */
		qdf_spin_unlock_bh(&pdev->peer_map_unmap_lock);
		ol_txrx_info("peer not found for peer_id %d", peer_id);
		wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
				    DEBUG_PEER_UNMAP_EVENT,
				    peer_id, NULL, NULL, 0, 0x102);
		return;
	}

	if (qdf_atomic_dec_and_test
		(&pdev->peer_id_to_obj_map[peer_id].peer_id_ref_cnt)) {
		pdev->peer_id_to_obj_map[peer_id].peer = NULL;
		for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
			if (peer->peer_ids[i] == peer_id) {
				peer->peer_ids[i] = HTT_INVALID_PEER;
				break;
			}
		}
	}

	ref_cnt = qdf_atomic_read
		(&pdev->peer_id_to_obj_map[peer_id].peer_id_ref_cnt);

	qdf_spin_unlock_bh(&pdev->peer_map_unmap_lock);

	wlan_roam_debug_log(DEBUG_INVALID_VDEV_ID,
			    DEBUG_PEER_UNMAP_EVENT,
			    peer_id, &peer->mac_addr.raw, peer, ref_cnt,
			    qdf_atomic_read(&peer->ref_cnt));

	/*
	 * Remove a reference to the peer.
	 * If there are no more references, delete the peer object.
	 */
	ol_txrx_peer_release_ref(peer, PEER_DEBUG_ID_OL_PEER_MAP);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: peer_id %d peer %pK peer_id_ref_cnt %d",
		  __func__, peer_id, peer, ref_cnt);
}

/**
 * ol_txrx_peer_remove_obj_map_entries() - Remove matching pdev peer map entries
 * @pdev: pdev handle
 * @peer: peer for removing obj map entries
 *
 * Saves peer_id_ref_cnt to a different field and removes the link
 * to peer object. It also decrements the peer reference count by
 * the number of references removed.
 *
 * Return: None
 */
void ol_txrx_peer_remove_obj_map_entries(ol_txrx_pdev_handle pdev,
					struct ol_txrx_peer_t *peer)
{
	int i;
	uint16_t peer_id;
	int32_t peer_id_ref_cnt;
	int32_t num_deleted_maps = 0;
	uint16_t save_peer_ids[MAX_NUM_PEER_ID_PER_PEER];
	uint16_t save_peer_id_ref_cnt[MAX_NUM_PEER_ID_PER_PEER] = {0};

	qdf_spin_lock_bh(&pdev->peer_map_unmap_lock);
	for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
		peer_id = peer->peer_ids[i];
		save_peer_ids[i] = HTT_INVALID_PEER;
		if (peer_id == HTT_INVALID_PEER ||
			!pdev->peer_id_to_obj_map[peer_id].peer) {
			/* unused peer_id, or object is already dereferenced */
			continue;
		}
		if (pdev->peer_id_to_obj_map[peer_id].peer != peer) {
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				QDF_TRACE_LEVEL_ERROR,
				FL("peer pointer mismatch in peer_id_to_obj"));
			continue;
		}
		peer_id_ref_cnt = qdf_atomic_read(
					&pdev->peer_id_to_obj_map[peer_id].
						peer_id_ref_cnt);
		save_peer_ids[i] = peer_id;
		save_peer_id_ref_cnt[i] = peer_id_ref_cnt;

		/*
		 * Transfer peer_id_ref_cnt into del_peer_id_ref_cnt so that
		 * ol_txrx_peer_release_ref will decrement del_peer_id_ref_cnt
		 * and any map events will increment peer_id_ref_cnt. Otherwise
		 * accounting will be messed up.
		 *
		 * Add operation will ensure that back to back roaming in the
		 * middle of unmap/map event sequence will be accounted for.
		 */
		qdf_atomic_add(peer_id_ref_cnt,
			&pdev->peer_id_to_obj_map[peer_id].del_peer_id_ref_cnt);
		qdf_atomic_init(&pdev->peer_id_to_obj_map[peer_id].
				peer_id_ref_cnt);
		num_deleted_maps += peer_id_ref_cnt;
		pdev->peer_id_to_obj_map[peer_id].peer = NULL;
		peer->peer_ids[i] = HTT_INVALID_PEER;
	}
	qdf_spin_unlock_bh(&pdev->peer_map_unmap_lock);

	/* Debug print the information after releasing bh spinlock */
	for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
		if (save_peer_ids[i] == HTT_INVALID_PEER)
			continue;
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
			  FL("peer_id = %d, peer_id_ref_cnt = %d, index = %d"),
			  save_peer_ids[i], save_peer_id_ref_cnt[i], i);
	}

	if (num_deleted_maps > qdf_atomic_read(&peer->ref_cnt)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  FL("num_deleted_maps %d ref_cnt %d"),
			  num_deleted_maps, qdf_atomic_read(&peer->ref_cnt));
		QDF_BUG(0);
		return;
	}

	while (num_deleted_maps-- > 0)
		ol_txrx_peer_release_ref(peer, PEER_DEBUG_ID_OL_PEER_MAP);
}

struct ol_txrx_peer_t *ol_txrx_assoc_peer_find(struct ol_txrx_vdev_t *vdev)
{
	struct ol_txrx_peer_t *peer;

	qdf_spin_lock_bh(&vdev->pdev->last_real_peer_mutex);
	/*
	 * Check the TXRX Peer is itself valid And also
	 * if HTT Peer ID has been setup for this peer
	 */
	if (vdev->last_real_peer
	    && vdev->last_real_peer->peer_ids[0] != HTT_INVALID_PEER_ID) {
		qdf_spin_lock_bh(&vdev->pdev->peer_ref_mutex);
		ol_txrx_peer_get_ref(vdev->last_real_peer,
				     PEER_DEBUG_ID_OL_INTERNAL);
		qdf_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);
		peer = vdev->last_real_peer;
	} else {
		peer = NULL;
	}
	qdf_spin_unlock_bh(&vdev->pdev->last_real_peer_mutex);
	return peer;
}


/*=== function definitions for debug ========================================*/

#if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
void ol_txrx_peer_find_display(ol_txrx_pdev_handle pdev, int indent)
{
	int i, max_peers;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "%*speer map:\n", indent, " ");
	max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
	for (i = 0; i < max_peers; i++) {
		if (pdev->peer_id_to_obj_map[i].peer) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
				  "%*sid %d -> %pK\n",
				  indent + 4, " ", i,
				  pdev->peer_id_to_obj_map[i].peer);
		}
	}
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "%*speer hash table:\n", indent, " ");
	for (i = 0; i <= pdev->peer_hash.mask; i++) {
		if (!TAILQ_EMPTY(&pdev->peer_hash.bins[i])) {
			struct ol_txrx_peer_t *peer;

			TAILQ_FOREACH(peer, &pdev->peer_hash.bins[i],
				      hash_list_elem) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_INFO_LOW,
					  "%*shash idx %d -> %pK ("QDF_MAC_ADDR_FMT")\n",
					indent + 4, " ", i, peer,
					QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			}
		}
	}
}

#endif /* if TXRX_DEBUG_LEVEL */
