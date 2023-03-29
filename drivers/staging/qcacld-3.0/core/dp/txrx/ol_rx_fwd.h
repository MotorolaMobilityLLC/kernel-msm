/*
 * Copyright (c) 2011, 2014-2017, 2019 The Linux Foundation. All rights reserved.
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

#ifndef _OL_RX_FWD_H_
#define _OL_RX_FWD_H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */

#include <ol_txrx_api.h>        /* ol_txrx_peer_t, etc. */

qdf_nbuf_t
ol_rx_fwd_mcast_check_sta(struct ol_txrx_vdev_t *vdev,
			  struct ol_txrx_peer_t *peer,
			  qdf_nbuf_t msdu, void *rx_desc, int is_wlan_mcast);

qdf_nbuf_t
ol_rx_fwd_mcast_check_ap(struct ol_txrx_vdev_t *vdev,
			 struct ol_txrx_peer_t *peer,
			 qdf_nbuf_t msdu, void *rx_desc, int is_wlan_mcast);

/**
 * @brief Check if rx frames should be transmitted over WLAN.
 * @details
 *  Check if rx frames should be transmitted back over WLAN, instead of
 *  or in addition to delivering the rx frames to the OS.
 *  Rx frames will be forwarded to the transmit path under the following
 *  conditions:
 *  1.  If the destination is a STA associated to the same virtual device
 *      within this physical device, the rx frame will be forwarded to the
 *      tx path rather than being sent to the OS.  If the destination is a
 *      STA associated to a different virtual device within this physical
 *      device, then the rx frame will optionally be forwarded to the tx path.
 *  2.  If the frame is received by an AP, but the destination is for another
 *      AP that the current AP is associated with for WDS forwarding, the
 *      intermediate AP will forward the rx frame to the tx path to transmit
 *      to send to the destination AP, rather than sending it to the OS.
 *  3.  If the AP receives a multicast frame, it will retransmit the frame
 *      within the BSS, in addition to sending the frame to the OS.
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid  - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform the rx->tx
 *      forwarding check on
 */
void
ol_rx_fwd_check(struct ol_txrx_vdev_t *vdev,
		struct ol_txrx_peer_t *peer,
		unsigned int tid, qdf_nbuf_t msdu_list);

A_STATUS
ol_get_intra_bss_fwd_pkts_count(struct cdp_soc_t *soc_hdl,
				uint8_t vdev_id,
				uint64_t *fwd_tx_packets,
				uint64_t *fwd_rx_packets);

#endif /* _OL_RX_FWD_H_ */
