/*
 * Copyright (c) 2011, 2014-2019 The Linux Foundation. All rights reserved.
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
 * @file ol_tx_send.h
 * @brief API definitions for the tx sendriptor module within the data SW.
 */
#ifndef _OL_TX_SEND__H_
#define _OL_TX_SEND__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */

#if defined(CONFIG_HL_SUPPORT)

static inline void ol_tx_discard_target_frms(ol_txrx_pdev_handle pdev)
{
}
#else

/**
 * @flush the ol tx when surprise remove.
 *
 */
void ol_tx_discard_target_frms(ol_txrx_pdev_handle pdev);
#endif

/**
 * @brief Send a tx frame to the target.
 * @details
 *
 * @param pdev -  the phy dev
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param netbuf - the tx frame
 */
void
ol_tx_send(struct ol_txrx_pdev_t *pdev,
	   struct ol_tx_desc_t *tx_desc, qdf_nbuf_t msdu, uint8_t vdev_id);

/**
 * @brief Send a tx batch download to the target.
 * @details
 *     This function is different from above in that
 *     it accepts a list of msdu's to be downloaded as a batch
 *
 * @param pdev -  the phy dev
 * @param msdu_list - the Head pointer to the Tx Batch
 * @param num_msdus - Total msdus chained in msdu_list
 */

void
ol_tx_send_batch(struct ol_txrx_pdev_t *pdev,
		 qdf_nbuf_t msdu_list, int num_msdus);

/**
 * @brief Send a tx frame with a non-std header or payload type to the target.
 * @details
 *
 * @param pdev -  the phy dev
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param netbuf - the tx frame
 * @param pkt_type - what kind of non-std frame is being sent
 */
void
ol_tx_send_nonstd(struct ol_txrx_pdev_t *pdev,
		  struct ol_tx_desc_t *tx_desc,
		  qdf_nbuf_t msdu, enum htt_pkt_type pkt_type);

#ifdef QCA_COMPUTE_TX_DELAY
/**
 * ol_tx_set_compute_interval() - update compute interval period for TSM stats
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @interval: interval for stats computation
 *
 * Return: NONE
 */
void ol_tx_set_compute_interval(struct cdp_soc_t *soc_hdl,
				uint8_t pdev_id, uint32_t interval);

/**
 * ol_tx_packet_count() - Return the uplink (transmitted) packet counts
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @out_packet_count: number of packets transmitted
 * @out_packet_loss_count: number of packets lost
 * @category: access category of interest
 *
 * This function will be called for getting uplink packet count and
 * loss count for given stream (access category) a regular interval.
 * This also resets the counters hence, the value returned is packets
 * counted in last 5(default) second interval. These counter are
 * incremented per access category in ol_tx_completion_handler()
 *
 * Return: NONE
 */
void
ol_tx_packet_count(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		   uint16_t *out_packet_count,
		   uint16_t *out_packet_loss_count, int category);

/**
 * ol_tx_delay() - get tx packet delay
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @queue_delay_microsec: tx packet delay within queue, usec
 * @tx_delay_microsec: tx packet delay, usec
 * @category: packet category
 *
 * Return: NONE
 */
void
ol_tx_delay(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	    uint32_t *queue_delay_microsec,
	    uint32_t *tx_delay_microsec, int category);

/**
 * ol_tx_delay_hist() - get tx packet delay histogram
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @report_bin_values: bin
 * @category: packet category
 *
 * Return: NONE
 */
void
ol_tx_delay_hist(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		 uint16_t *report_bin_values, int category);
#endif /* QCA_COMPUTE_TX_DELAY */

/**
 * ol_txrx_flow_control_cb() - call osif flow control callback
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of vdev
 * @tx_resume: tx resume flag
 *
 * Return: none
 */
void ol_txrx_flow_control_cb(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			     bool tx_resume);

#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || (defined(CONFIG_HL_SUPPORT) && \
	 defined(QCA_HL_NETDEV_FLOW_CONTROL))
void ol_tx_flow_ct_unpause_os_q(ol_txrx_pdev_handle pdev);
#else
static inline void ol_tx_flow_ct_unpause_os_q(ol_txrx_pdev_handle pdev)
{
}
#endif
#endif /* _OL_TX_SEND__H_ */
