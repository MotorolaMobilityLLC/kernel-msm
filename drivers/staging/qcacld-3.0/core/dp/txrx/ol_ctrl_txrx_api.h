/*
 * Copyright (c) 2011-2017, 2019 The Linux Foundation. All rights reserved.
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
 * @file ol_ctrl_txrx_api.h
 * @brief Define the host control API functions called by the host data SW.
 */
#ifndef _OL_CTRL_TXRX_API__H_
#define _OL_CTRL_TXRX_API__H_

#include <osdep.h>              /* uint8_t */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */

#include <cdp_txrx_cmn.h>           /* ol_txrx_pdev_handle */
#include <ol_txrx_types.h>
#include <cds_ieee80211_common.h>   /* ieee80211_frame */
#include <cdp_txrx_handle.h>
#ifdef SUPPORT_HOST_STATISTICS
/** * @brief Update tx statistics
 * @details
 *  Update tx statistics after tx complete.
 *
 * @param pdev - ol_pdev_handle instance
 * @param vdev_id - ID of the virtual device that tx frame
 * @param had_error - whether there is error when tx
 */
void ol_tx_statistics(struct cdp_cfg *cfg_pdev,
		     uint16_t vdev_id, int had_error);
#else
#define ol_tx_statistics(pdev, vdev_id, had_error)
#endif

/** * @brief Count on received packets for invalid peer case
 *
 * @param pdev - txrx pdev handle
 * @param wh - received frame
 * @param err_type - what kind of error occurred
 */
void ol_rx_err_inv_peer_statistics(struct cdp_cfg *cfg_pdev,
				   struct ieee80211_frame *wh,
				   enum ol_rx_err_type err_type);

/**
 * @brief Count on received packets, both success and failed
 *
 * @param pdev - ol_pdev_handle handle
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param err_type - what kind of error occurred
 * @param sec_type - The cipher type the peer is using
 * @param is_mcast - whether this is one multi cast frame
 */
void ol_rx_err_statistics(struct cdp_cfg *cfg_pdev,
			  uint8_t vdev_id,
			  enum ol_rx_err_type err_type,
			  enum ol_sec_type sec_type, int is_mcast);

/**
 * @brief Provide notification of failure during host rx processing
 * @details
 *  Indicate an error during host rx data processing, including what
 *  kind of error happened, when it happened, which peer and TID the
 *  erroneous rx frame is from, and what the erroneous rx frame itself
 *  is.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param peer_mac_addr - MAC address of the peer that sent the erroneous
 *      rx frame
 * @param tid - which TID within the peer sent the erroneous rx frame
 * @param tsf32  - the timstamp in TSF units of the erroneous rx frame, or
 *      one of the fragments that when reassembled, constitute the rx frame
 * @param err_type - what kind of error occurred
 * @param rx_frame - the rx frame that had an error
 * @pn - Packet sequence number
 * @key_id - Key index octet received in IV of the frame
 */
void
ol_rx_err(struct cdp_cfg *cfg_pdev,
	  uint8_t vdev_id,
	  uint8_t *peer_mac_addr,
	  int tid,
	  uint32_t tsf32,
	  enum ol_rx_err_type err_type,
	  qdf_nbuf_t rx_frame, uint64_t *pn, uint8_t key_id);

#ifdef HL_RX_AGGREGATION_HOLE_DETECTION
/**
 * ol_rx_aggregation_hole - ol rx aggregation hole report
 * @hole_info: hole_info
 *
 * Return: void
 */
void ol_rx_aggregation_hole(uint32_t hole_info);
#endif

enum ol_rx_notify_type {
	OL_RX_NOTIFY_IPV4_IGMP,
};

/**
 * @brief Provide notification of reception of data of special interest.
 * @details
 *  Indicate when "special" data has been received.  The nature of the
 *  data that results in it being considered special is specified in the
 *  notify_type argument.
 *  This function is currently used by the data-path SW to notify the
 *  control path SW when the following types of rx data are received:
 *    + IPv4 IGMP frames
 *      The control SW can use these to learn about multicast group
 *      membership, if it so chooses.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the special data
 * @param peer_mac_addr - MAC address of the peer that sent the special data
 * @param tid - which TID within the peer sent the special data
 * @param tsf32  - the timstamp in TSF units of the special data
 * @param notify_type - what kind of special data was received
 * @param rx_frame - the rx frame containing the special data
 */
void
ol_rx_notify(struct cdp_cfg *cfg_pdev,
	     uint8_t vdev_id,
	     uint8_t *peer_mac_addr,
	     int tid,
	     uint32_t tsf32,
	     enum ol_rx_notify_type notify_type, qdf_nbuf_t rx_frame);

#define ol_ctrl_addba_req(pdev, peer_mac_addr, tid) ol_addba_req_reject
#define ol_ctrl_rx_addba_complete(pdev, peer_mac_addr, tid, failed) /* no-op */

#endif /* _OL_CTRL_TXRX_API__H_ */
