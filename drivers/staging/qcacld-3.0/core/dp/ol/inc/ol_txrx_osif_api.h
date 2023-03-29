/*
 * Copyright (c) 2012, 2014-2019 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_osif_api.h
 * @brief Define the host data API functions called by the host OS shim SW.
 */
#ifndef _OL_TXRX_OSIF_API__H_
#define _OL_TXRX_OSIF_API__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include "cds_sched.h"
#include "ol_txrx_ctrl_api.h"
#include <cdp_txrx_peer_ops.h>

/**
 * struct ol_rx_cached_buf - rx cached buffer
 * @list: linked list
 * @buf: skb buffer
 */
struct ol_rx_cached_buf {
	struct list_head list;
	qdf_nbuf_t buf;
};

struct txrx_rx_metainfo;

/**
 * @brief Divide a jumbo TCP frame into smaller segments.
 * @details
 *  For efficiency, the protocol stack above the WLAN driver may operate
 *  on jumbo tx frames, which are larger than the 802.11 MTU.
 *  The OSIF SW uses this txrx API function to divide the jumbo tx TCP frame
 *  into a series of segment frames.
 *  The segments are created as clones of the input jumbo frame.
 *  The txrx SW generates a new encapsulation header (ethernet + IP + TCP)
 *  for each of the output segment frames.  The exact format of this header,
 *  e.g. 802.3 vs. Ethernet II, and IPv4 vs. IPv6, is chosen to match the
 *  header format of the input jumbo frame.
 *  The input jumbo frame is not modified.
 *  After the ol_txrx_osif_tso_segment returns, the OSIF SW needs to perform
 *  DMA mapping on each of the segment network buffers, and also needs to
 *
 * @param txrx_vdev - which virtual device will transmit the TSO segments
 * @param max_seg_payload_bytes - the maximum size for the TCP payload of
 *      each segment frame.
 *      This does not include the ethernet + IP + TCP header sizes.
 * @param jumbo_tcp_frame - jumbo frame which needs to be cloned+segmented
 * @return
 *      NULL if the segmentation fails, - OR -
 *      a NULL-terminated list of segment network buffers
 */
qdf_nbuf_t ol_txrx_osif_tso_segment(ol_txrx_vdev_handle txrx_vdev,
				    int max_seg_payload_bytes,
				    qdf_nbuf_t jumbo_tcp_frame);

qdf_nbuf_t ol_tx_data(struct cdp_soc_t *soc, uint8_t vdev_id, qdf_nbuf_t skb);

void ol_rx_data_process(struct ol_txrx_peer_t *peer,
			qdf_nbuf_t rx_buf_list);

void ol_txrx_flush_rx_frames(struct ol_txrx_peer_t *peer,
			     bool drop);
#endif /* _OL_TXRX_OSIF_API__H_ */
