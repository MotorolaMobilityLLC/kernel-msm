/*
 * Copyright (c) 2012, 2014, 2016-2017 The Linux Foundation. All rights reserved.
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

#ifndef _OL_RX_REORDER_TIMEOUT__H_
#define _OL_RX_REORDER_TIMEOUT__H_

#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */
#ifdef QCA_SUPPORT_OL_RX_REORDER_TIMEOUT

void ol_rx_reorder_timeout_init(struct ol_txrx_pdev_t *pdev);
void ol_rx_reorder_timeout_cleanup(struct ol_txrx_pdev_t *pdev);
void ol_rx_reorder_timeout_remove(struct ol_txrx_peer_t *peer,
				  unsigned int tid);
void ol_rx_reorder_timeout_update(struct ol_txrx_peer_t *peer, uint8_t tid);
void ol_rx_reorder_timeout_peer_cleanup(struct ol_txrx_peer_t *peer);

#define OL_RX_REORDER_TIMEOUT_INIT    ol_rx_reorder_timeout_init
#define OL_RX_REORDER_TIMEOUT_PEER_CLEANUP ol_rx_reorder_timeout_peer_cleanup
#define OL_RX_REORDER_TIMEOUT_CLEANUP ol_rx_reorder_timeout_cleanup
#define OL_RX_REORDER_TIMEOUT_REMOVE  ol_rx_reorder_timeout_remove
#define OL_RX_REORDER_TIMEOUT_UPDATE  ol_rx_reorder_timeout_update
#define OL_RX_REORDER_TIMEOUT_PEER_TID_INIT(peer, tid) \
	(peer)->tids_rx_reorder[(tid)].timeout.active = 0
#define OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev) \
	qdf_spin_lock(&(pdev)->rx.mutex)
#define OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev) \
	qdf_spin_unlock(&(pdev)->rx.mutex)

#else

#define OL_RX_REORDER_TIMEOUT_INIT(pdev)        /* no-op */
#define OL_RX_REORDER_TIMEOUT_PEER_CLEANUP(peer)        /* no-op */
#define OL_RX_REORDER_TIMEOUT_CLEANUP(pdev)     /* no-op */
#define OL_RX_REORDER_TIMEOUT_REMOVE(peer, tid) /* no-op */
#define OL_RX_REORDER_TIMEOUT_UPDATE(peer, tid) /* no-op */
#define OL_RX_REORDER_TIMEOUT_PEER_TID_INIT(peer, tid)  /* no-op */
#define OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev)  /* no-op */
#define OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev)        /* no-op */

#endif /* QCA_SUPPORT_OL_RX_REORDER_TIMEOUT */

#endif /* _OL_RX_REORDER_TIMEOUT__H_ */
