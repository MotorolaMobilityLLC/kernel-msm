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

/**
 * @file ol_ctrl_txrx_api.h
 * @brief Define the host control API functions called by the host data SW.
 */
#ifndef _OL_CTRL_TXRX_API__H_
#define _OL_CTRL_TXRX_API__H_

//#include <osapi_linux.h>      /* u_int8_t */
#include <osdep.h>        /* u_int8_t */
#include <adf_nbuf.h>     /* adf_nbuf_t */

#include <ol_ctrl_api.h>  /* ol_vdev_handle */
#include <ol_txrx_api.h>  /* ol_txrx_peer_handle, etc. */
#include <ieee80211_common.h>   /*ieee80211_frame */

enum ol_rx_err_type {
    OL_RX_ERR_DEFRAG_MIC,
    OL_RX_ERR_PN,
    OL_RX_ERR_UNKNOWN_PEER,
    OL_RX_ERR_MALFORMED,
    OL_RX_ERR_TKIP_MIC,
	OL_RX_ERR_DECRYPT,
	OL_RX_ERR_MPDU_LENGTH,
	OL_RX_ERR_ENCRYPT_REQUIRED,
	OL_RX_ERR_DUP,
	OL_RX_ERR_UNKNOWN,
	OL_RX_ERR_FCS,
	OL_RX_ERR_PRIVACY,
	OL_RX_ERR_NONE_FRAG,
	OL_RX_ERR_NONE = 0xFF
};

#ifdef SUPPORT_HOST_STATISTICS
/** * @brief Update tx statistics
 * @details
 *  Update tx statistics after tx complete.
 *
 * @param pdev - ol_pdev_handle instance
 * @param vdev_id - ID of the virtual device that tx frame
 * @param had_error - whether there is error when tx
 */
void ol_tx_statistics(ol_pdev_handle pdev,
                      u_int16_t vdev_id,
					  int had_error);
#else
#define ol_tx_statistics(pdev, vdev_id, had_error)
#endif

/** * @brief Count on received packets for invalid peer case
 *
 * @param pdev - txrx pdev handle
 * @param wh - received frame
 * @param err_type - what kind of error occurred
 */
void ol_rx_err_inv_peer_statistics(ol_pdev_handle pdev,
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
void ol_rx_err_statistics(ol_pdev_handle pdev,
						  u_int8_t vdev_id,
						  enum ol_rx_err_type err_type,
						  enum ol_sec_type sec_type,
						  int is_mcast);

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
ol_rx_err(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_err_type err_type,
    adf_nbuf_t rx_frame,
    u_int64_t *pn,
    u_int8_t key_id);


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
ol_rx_notify(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_notify_type notify_type,
    adf_nbuf_t rx_frame);

/**
 * @brief Indicate when a paused STA has tx data available.
 * @details
 *  Indicate to the control SW when a paused peer that previously
 *  has all its peer-TID queues empty gets a MSDU to transmit.
 *  Conversely, indicate when a paused peer that had data in one or more of
 *  its peer-TID queues has all queued data removed (e.g. due to a U-APSD
 *  triggered transmission), but is still paused.
 *  It is up to the control SW to determine whether the peer is paused due to
 *  being in power-save sleep, or some other reason, and thus whether it is
 *  necessary to set the TIM in beacons to notify a sleeping STA that it has
 *  data.
 *  The data SW will also issue this ol_tx_paused_peer_data call when an
 *  unpaused peer that currently has tx data in one or more of its
 *  peer-TID queues becomes paused.
 *  The data SW will not issue this ol_tx_paused_peer_data call when a
 *  peer with data in one or more of its peer-TID queues becomes unpaused.
 *
 * @param peer - the paused peer
 * @param has_tx_data -
 *      1 -> a paused peer that previously had no tx data now does, -OR-
 *      0 -> a paused peer that previously had tx data now doesnt
 */
void
ol_tx_paused_peer_data(ol_peer_handle peer, int has_tx_data);


#define ol_ctrl_addba_req(pdev, peer_mac_addr, tid) ol_addba_req_reject
#define ol_ctrl_rx_addba_complete(pdev, peer_mac_addr, tid, failed) /* no-op */



#endif /* _OL_CTRL_TXRX_API__H_ */
