/*
 * Copyright (c) 2011, 2015-2019 The Linux Foundation. All rights reserved.
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

/**
 * @file ol_txrx_peer_find.h
 * @brief Define the API for the rx peer lookup datapath module.
 */
#ifndef _OL_TXRX_PEER_FIND__H_
#define _OL_TXRX_PEER_FIND__H_

#include <htt.h>                /* HTT_INVALID_PEER */
#include <cdp_txrx_cmn.h>       /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT */

/**
 * ol_txrx_peer_get_ref() - get peer reference
 * @peer: peer for removing obj map entries
 * @dbg_id: debug id to keep track of peer references
 *
 * The function increments the peer ref count. The ref count can be reduced by
 * caling ol_txrx_peer_release_ref function. Callers are responsible for
 * acquiring the peer_ref_mutex lock when needed.
 *
 * Return: peer debug id ref count or error
 */
int
ol_txrx_peer_get_ref(struct ol_txrx_peer_t *peer,
		     enum peer_debug_id_type dbg_id);

int ol_txrx_peer_find_attach(struct ol_txrx_pdev_t *pdev);

void ol_txrx_peer_find_detach(struct ol_txrx_pdev_t *pdev);

static inline
int
ol_txrx_peer_find_mac_addr_cmp(union ol_txrx_align_mac_addr_t *mac_addr1,
			       union ol_txrx_align_mac_addr_t *mac_addr2)
{
	return !((mac_addr1->align4.bytes_abcd == mac_addr2->align4.bytes_abcd)
		 /*
		  * Intentionally use & rather than &&.
		  * because the operands are binary rather than generic bool,
		  * the functionality is equivalent.
		  * Using && has the advantage of short-circuited evaluation,
		  * but using & has the advantage of no conditional branching,
		  * which is a more significant benefit.
		  */
		 & (mac_addr1->align4.bytes_ef == mac_addr2->align4.bytes_ef));
}

static inline
struct ol_txrx_peer_t *ol_txrx_peer_find_by_id(struct ol_txrx_pdev_t *pdev,
					       uint16_t peer_id)
{
	struct ol_txrx_peer_t *peer;

	peer = (peer_id > ol_cfg_max_peer_id(pdev->ctrl_pdev)) ? NULL :
	       pdev->peer_id_to_obj_map[peer_id].peer;
	/*
	 * Currently, peer IDs are assigned to vdevs as well as peers.
	 * If the peer ID is for a vdev, the peer_id_to_obj_map entry
	 * will hold NULL rather than a valid peer pointer.
	 */
	/* TXRX_ASSERT2(peer); */
	/*
	 * Only return the peer object if it is valid,
	 * i.e. it has not already been detached.
	 * If it has already been detached, then returning the
	 * peer object could result in unpausing the peer's tx queues
	 * in HL systems, which is an invalid operation following peer_detach.
	 */
	if (peer && peer->valid)
		return peer;

	return NULL;
}

void
ol_txrx_peer_find_hash_add(struct ol_txrx_pdev_t *pdev,
			   struct ol_txrx_peer_t *peer);

struct ol_txrx_peer_t *
	ol_txrx_peer_find_hash_find_get_ref
				(struct ol_txrx_pdev_t *pdev,
				uint8_t *peer_mac_addr,
				int mac_addr_is_aligned,
				u8 check_valid,
				enum peer_debug_id_type dbg_id);

struct
ol_txrx_peer_t *ol_txrx_peer_vdev_find_hash(struct ol_txrx_pdev_t *pdev,
					    struct ol_txrx_vdev_t *vdev,
					    uint8_t *peer_mac_addr,
					    int mac_addr_is_aligned,
					    uint8_t check_valid);

void
ol_txrx_peer_find_hash_remove(struct ol_txrx_pdev_t *pdev,
			      struct ol_txrx_peer_t *peer);

void ol_txrx_peer_find_hash_erase(struct ol_txrx_pdev_t *pdev);

struct ol_txrx_peer_t *ol_txrx_assoc_peer_find(struct ol_txrx_vdev_t *vdev);
void ol_txrx_peer_remove_obj_map_entries(ol_txrx_pdev_handle pdev,
					struct ol_txrx_peer_t *peer);
void ol_txrx_peer_clear_map_peer(ol_txrx_pdev_handle pdev,
				 struct ol_txrx_peer_t *peer);
#if defined(TXRX_DEBUG_LEVEL) && TXRX_DEBUG_LEVEL > 5
void ol_txrx_peer_find_display(ol_txrx_pdev_handle pdev, int indent);
#else
#define ol_txrx_peer_find_display(pdev, indent)
#endif /* TXRX_DEBUG_LEVEL */

#endif /* _OL_TXRX_PEER_FIND__H_ */
