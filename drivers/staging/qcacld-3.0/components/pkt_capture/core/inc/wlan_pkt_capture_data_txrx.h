/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare private API which shall be used internally only
 * in pkt_capture component. This file shall include prototypes of
 * pkt_capture data tx and data rx.
 *
 * Note: This API should be never accessed out of pkt_capture component.
 */

#ifndef _WLAN_PKT_CAPTURE_DATA_TXRX_H_
#define _WLAN_PKT_CAPTURE_DATA_TXRX_H_

#include "cdp_txrx_cmn_struct.h"
#include <qdf_nbuf.h>
#include <qdf_list.h>
#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
#include <htt_internal.h>
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
#define IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN 0x0020
#define IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN 0x4000
#define IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN 0x0002
#define IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN 0x0080
#define IEEE80211_RADIOTAP_HE_DATA1_STBC_KNOWN 0x0200
#endif

/**
 * pkt_capture_data_process_type - data pkt types to process
 * for packet capture mode
 * @TXRX_PROCESS_TYPE_DATA_RX: process RX packets (normal rx + offloaded rx)
 * @TXRX_PROCESS_TYPE_DATA_TX: process TX packets (ofloaded tx)
 * @TXRX_PROCESS_TYPE_DATA_TX_COMPL: process TX compl packets (normal tx)
 */
enum pkt_capture_data_process_type {
	TXRX_PROCESS_TYPE_DATA_RX,
	TXRX_PROCESS_TYPE_DATA_TX,
	TXRX_PROCESS_TYPE_DATA_TX_COMPL,
};

#define TXRX_PKTCAPTURE_PKT_FORMAT_8023    0
#define TXRX_PKTCAPTURE_PKT_FORMAT_80211   1

#define SHORT_PREAMBLE 1
#define LONG_PREAMBLE  0

/**
 * pkt_capture_datapkt_process() - process data tx and rx packets
 * for pkt capture mode. (normal tx/rx + offloaded tx/rx)
 * @vdev_id: vdev id for which packet is captured
 * @mon_buf_list: netbuf list
 * @type: data process type
 * @tid:  tid number
 * @status: Tx status
 * @pkt_format: Frame format
 * @bssid: bssid
 * @pdev: pdev handle
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
void pkt_capture_datapkt_process(
			uint8_t vdev_id,
			qdf_nbuf_t mon_buf_list,
			enum pkt_capture_data_process_type type,
			uint8_t tid, uint8_t status, bool pktformat,
			uint8_t *bssid, void *pdev,
			uint8_t tx_retry_cnt);

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_msdu_process_pkts() - process data rx pkts
 * @bssid: bssid
 * @head_msdu: pointer to head msdu
 * @vdev_id: vdev_id
 * @pdev: pdev handle
 *
 * Return: none
 */
void pkt_capture_msdu_process_pkts(
			uint8_t *bssid,
			qdf_nbuf_t head_msdu,
			uint8_t vdev_id,
			htt_pdev_handle pdev, uint16_t status);
#else
void pkt_capture_msdu_process_pkts(
			uint8_t *bssid,
			qdf_nbuf_t head_msdu,
			uint8_t vdev_id,
			void *psoc, uint16_t status);
#endif

/**
 * pkt_capture_rx_in_order_drop_offload_pkt() - drop offload packets
 * @head_msdu: pointer to head msdu
 *
 * Return: none
 */
void pkt_capture_rx_in_order_drop_offload_pkt(qdf_nbuf_t head_msdu);

/**
 * pkt_capture_rx_in_order_offloaded_pkt() - check offloaded data pkt or not
 * @rx_ind_msg: rx_ind_msg
 *
 * Return: false, if it is not offload pkt
 *         true, if it is offload pkt
 */
bool pkt_capture_rx_in_order_offloaded_pkt(qdf_nbuf_t rx_ind_msg);

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_offload_deliver_indication_handler() - Handle offload data pkts
 * @msg: offload netbuf msg
 * @vdev_id: vdev id
 * @bssid: bssid
 * @pdev: pdev handle
 *
 * Return: none
 */
void pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, htt_pdev_handle pdev);
#else
/**
 * pkt_capture_offload_deliver_indication_handler() - Handle offload data pkts
 * @msg: offload netbuf msg
 * @vdev_id: vdev id
 * @bssid: bssid
 * @soc: dp_soc handle
 *
 * Return: none
 */
void pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, void *soc);
#endif

/**
 * pkt_capture_tx_hdr_elem_t - tx packets header struture to
 * be used to update radiotap header for packet capture mode
 * @timestamp: timestamp
 * @preamble: preamble
 * @mcs: MCS
 * @rate: rate
 * @rssi_comb: rssi in dBm
 * @nss: if nss 1 means 1ss and 2 means 2ss
 * @bw: BW (0=>20MHz, 1=>40MHz, 2=>80MHz, 3=>160MHz)
 * @stbc: STBC
 * @sgi: SGI
 * @ldpc: LDPC
 * @beamformed: beamformed
 * @dir: direction rx: 0 and tx: 1
 * @status: tx status
 * @tx_retry_cnt: tx retry count
 * @ppdu_id: ppdu_id of msdu
 */
struct pkt_capture_tx_hdr_elem_t {
	uint32_t timestamp;
	uint8_t preamble;
	uint8_t mcs;
	uint8_t rate;
	uint8_t rssi_comb;
	uint8_t nss;
	uint8_t bw;
	bool stbc;
	bool sgi;
	bool ldpc;
	bool beamformed;
	bool dir; /* rx:0 , tx:1 */
	uint8_t status; /* tx status */
	uint8_t tx_retry_cnt;
	uint16_t framectrl;
	uint16_t seqno;
	uint32_t ppdu_id;
};

/**
 * pkt_capture_ppdu_stats_q_node - node structure to be enqueued
 * in ppdu_stats_q
 * @node: list node
 * @buf: buffer data received from ppdu_stats
 */
struct pkt_capture_ppdu_stats_q_node {
	qdf_list_node_t node;
	uint32_t buf[];
};

/**
 * pkt_capture_tx_get_txcomplete_data_hdr() - extract Tx data hdr from Tx
 * completion for pkt capture mode
 * @msg_word: Tx completion htt msg
 * @num_msdus: number of msdus
 *
 * Return: tx data hdr information
 */
struct htt_tx_data_hdr_information *pkt_capture_tx_get_txcomplete_data_hdr(
						uint32_t *msg_word,
						int num_msdus);

#endif /* End  of _WLAN_PKT_CAPTURE_DATA_TXRX_H_ */
