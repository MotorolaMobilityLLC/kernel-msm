/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Implement various notification handlers which are accessed
 * internally in pkt_capture component only.
 */

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
#include <dp_types.h>
#include "htt_ppdu_stats.h"
#endif
#include "wlan_pkt_capture_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_pkt_capture_mon_thread.h"
#include "wlan_pkt_capture_mgmt_txrx.h"
#include "target_if_pkt_capture.h"
#include "cdp_txrx_ctrl.h"
#include "wlan_pkt_capture_tgt_api.h"
#include <cds_ieee80211_common.h>
#include "wlan_vdev_mgr_utils_api.h"

static struct wlan_objmgr_vdev *gp_pkt_capture_vdev;

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
wdi_event_subscribe PKT_CAPTURE_TX_SUBSCRIBER;
wdi_event_subscribe PKT_CAPTURE_RX_SUBSCRIBER;
wdi_event_subscribe PKT_CAPTURE_RX_NO_PEER_SUBSCRIBER;
wdi_event_subscribe PKT_CAPTURE_OFFLOAD_TX_SUBSCRIBER;
wdi_event_subscribe PKT_CAPTURE_PPDU_STATS_SUBSCRIBER;

/**
 * pkt_capture_wdi_event_subscribe() - Subscribe pkt capture callbacks
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
static void pkt_capture_wdi_event_subscribe(struct wlan_objmgr_psoc *psoc)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id = WMI_PDEV_ID_SOC;

	/* subscribing for tx data packets */
	PKT_CAPTURE_TX_SUBSCRIBER.callback =
				pkt_capture_callback;

	PKT_CAPTURE_TX_SUBSCRIBER.context = wlan_psoc_get_dp_handle(psoc);

	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_TX_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_TX_DATA);

	/* subscribing for rx data packets */
	PKT_CAPTURE_RX_SUBSCRIBER.callback =
				pkt_capture_callback;

	PKT_CAPTURE_RX_SUBSCRIBER.context = wlan_psoc_get_dp_handle(psoc);

	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_RX_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_RX_DATA);

	/* subscribe for rx data packets when no peer is there*/
	PKT_CAPTURE_RX_NO_PEER_SUBSCRIBER.callback =
				pkt_capture_callback;

	PKT_CAPTURE_RX_NO_PEER_SUBSCRIBER.context =
					wlan_psoc_get_dp_handle(psoc);

	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_RX_NO_PEER_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_RX_DATA_NO_PEER);

	/* subscribing for offload tx data packets */
	PKT_CAPTURE_OFFLOAD_TX_SUBSCRIBER.callback =
				pkt_capture_callback;

	PKT_CAPTURE_OFFLOAD_TX_SUBSCRIBER.context =
						wlan_psoc_get_dp_handle(psoc);

	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_OFFLOAD_TX_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_OFFLOAD_TX_DATA);

	/* subscribe for packet capture mode related ppdu stats */
	PKT_CAPTURE_PPDU_STATS_SUBSCRIBER.callback =
				pkt_capture_callback;

	PKT_CAPTURE_PPDU_STATS_SUBSCRIBER.context =
						wlan_psoc_get_dp_handle(psoc);

	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_PPDU_STATS_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_PPDU_STATS);
}

/**
 * pkt_capture_wdi_event_unsubscribe() - Unsubscribe pkt capture callbacks
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
static void pkt_capture_wdi_event_unsubscribe(struct wlan_objmgr_psoc *psoc)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id = WMI_PDEV_ID_SOC;

	/* unsubscribe ppdu smu stats */
	cdp_wdi_event_unsub(soc, pdev_id, &PKT_CAPTURE_PPDU_STATS_SUBSCRIBER,
			    WDI_EVENT_PKT_CAPTURE_PPDU_STATS);

	/* unsubscribing for offload tx data packets */
	cdp_wdi_event_unsub(soc, pdev_id, &PKT_CAPTURE_OFFLOAD_TX_SUBSCRIBER,
			    WDI_EVENT_PKT_CAPTURE_OFFLOAD_TX_DATA);

	/* unsubscribe for rx data no peer packets */
	cdp_wdi_event_sub(soc, pdev_id, &PKT_CAPTURE_RX_NO_PEER_SUBSCRIBER,
			  WDI_EVENT_PKT_CAPTURE_RX_DATA_NO_PEER);

	/* unsubscribing for rx data packets */
	cdp_wdi_event_unsub(soc, pdev_id, &PKT_CAPTURE_RX_SUBSCRIBER,
			    WDI_EVENT_PKT_CAPTURE_RX_DATA);

	/* unsubscribing for tx data packets */
	cdp_wdi_event_unsub(soc, pdev_id, &PKT_CAPTURE_TX_SUBSCRIBER,
			    WDI_EVENT_PKT_CAPTURE_TX_DATA);
}

enum pkt_capture_mode
pkt_capture_get_pktcap_mode_v2()
{
	enum pkt_capture_mode mode = PACKET_CAPTURE_MODE_DISABLE;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return PACKET_CAPTURE_MODE_DISABLE;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv)
		pkt_capture_err("vdev_priv is NULL");
	else
		mode = vdev_priv->cfg_params.pkt_capture_mode;

	pkt_capture_vdev_put_ref(vdev);
	return mode;
}

#define RX_OFFLOAD_PKT 1
#define PPDU_STATS_Q_MAX_SIZE 500

static void
pkt_capture_process_rx_data_no_peer(void *soc, uint16_t vdev_id, uint8_t *bssid,
				    uint32_t status, qdf_nbuf_t nbuf)
{
	uint32_t pkt_len, l3_hdr_pad, nbuf_len;
	struct dp_soc *psoc = soc;
	qdf_nbuf_t msdu;
	uint8_t *rx_tlv_hdr;

	nbuf_len = QDF_NBUF_CB_RX_PKT_LEN(nbuf);
	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	l3_hdr_pad = hal_rx_msdu_end_l3_hdr_padding_get(psoc->hal_soc,
							rx_tlv_hdr);
	pkt_len = nbuf_len + l3_hdr_pad + RX_PKT_TLVS_LEN;
	qdf_nbuf_set_pktlen(nbuf, pkt_len);

	/*
	 * Offload rx packets are delivered only to pkt capture component, so
	 * can modify the received nbuf, in other cases create a private copy
	 * of the received nbuf so that pkt capture component can modify it
	 * without altering the original nbuf
	 */
	if (status == RX_OFFLOAD_PKT)
		msdu = nbuf;
	else
		msdu = qdf_nbuf_copy(nbuf);

	if (!msdu)
		return;

	QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(msdu) = l3_hdr_pad;

	qdf_nbuf_pull_head(msdu, l3_hdr_pad + RX_PKT_TLVS_LEN);
	pkt_capture_datapkt_process(
			vdev_id, msdu,
			TXRX_PROCESS_TYPE_DATA_RX, 0, 0,
			TXRX_PKTCAPTURE_PKT_FORMAT_8023,
			bssid, psoc, 0);
}

static void
pkt_capture_process_ppdu_stats(void *log_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_ppdu_stats_q_node *q_node;
	htt_ppdu_stats_for_smu_tlv *smu;
	uint32_t stats_len;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (qdf_unlikely(!vdev_priv)) {
		pkt_capture_vdev_put_ref(vdev);
		return;
	}

	smu = (htt_ppdu_stats_for_smu_tlv *)log_data;
	vdev_priv->tx_nss = smu->nss;

	qdf_spin_lock(&vdev_priv->lock_q);
	if (qdf_list_size(&vdev_priv->ppdu_stats_q) <
					PPDU_STATS_Q_MAX_SIZE) {
		/*
		 * win size indicates the size of block ack bitmap, currently
		 * we support only 256 bit ba bitmap.
		 */
		if (smu->win_size > 8) {
			qdf_spin_unlock(&vdev_priv->lock_q);
			pkt_capture_vdev_put_ref(vdev);
			pkt_capture_err("win size %d > 8 not supported\n",
					smu->win_size);
			return;
		}

		stats_len = sizeof(htt_ppdu_stats_for_smu_tlv) +
				smu->win_size * sizeof(uint32_t);

		q_node = qdf_mem_malloc(sizeof(*q_node) + stats_len);
		if (q_node == NULL) {
			qdf_spin_unlock(&vdev_priv->lock_q);
			pkt_capture_vdev_put_ref(vdev);
			pkt_capture_err("stats node and buf allocation fail\n");
			return;
		}

		qdf_mem_copy(q_node->buf, log_data, stats_len);
		/* Insert received ppdu stats in queue */
		qdf_list_insert_back(&vdev_priv->ppdu_stats_q,
				     &q_node->node);
	}
	qdf_spin_unlock(&vdev_priv->lock_q);
	pkt_capture_vdev_put_ref(vdev);
}

static void
pkt_capture_process_tx_data(void *soc, void *log_data, u_int16_t vdev_id,
			    uint32_t status)
{
	struct dp_soc *psoc = soc;
	uint8_t tid = 0;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	struct pkt_capture_tx_hdr_elem_t *ptr_pktcapture_hdr;
	struct pkt_capture_tx_hdr_elem_t pktcapture_hdr = {0};
	struct hal_tx_completion_status tx_comp_status = {0};
	struct qdf_tso_seg_elem_t *tso_seg = NULL;
	uint32_t txcap_hdr_size =
			sizeof(struct pkt_capture_tx_hdr_elem_t);

	struct dp_tx_desc_s *desc = log_data;
	qdf_nbuf_t netbuf;
	int nbuf_len;

	hal_tx_comp_get_status(&desc->comp, &tx_comp_status,
			       psoc->hal_soc);

	if (tx_comp_status.valid)
		pktcapture_hdr.ppdu_id = tx_comp_status.ppdu_id;
	pktcapture_hdr.timestamp = tx_comp_status.tsf;
	pktcapture_hdr.preamble = tx_comp_status.pkt_type;
	pktcapture_hdr.mcs = tx_comp_status.mcs;
	pktcapture_hdr.bw = tx_comp_status.bw;
	/* nss not available */
	pktcapture_hdr.nss = 0;
	pktcapture_hdr.rssi_comb = tx_comp_status.ack_frame_rssi;
	/* rate not available */
	pktcapture_hdr.rate = 0;
	pktcapture_hdr.stbc = tx_comp_status.stbc;
	pktcapture_hdr.sgi = tx_comp_status.sgi;
	pktcapture_hdr.ldpc = tx_comp_status.ldpc;
	/* Beamformed not available */
	pktcapture_hdr.beamformed = 0;
	pktcapture_hdr.framectrl = IEEE80211_FC0_TYPE_DATA |
				   (IEEE80211_FC1_DIR_TODS << 8);
	pktcapture_hdr.tx_retry_cnt = tx_comp_status.transmit_cnt - 1;
	/* seqno not available */
	pktcapture_hdr.seqno = 0;
	tid = tx_comp_status.tid;
	status = tx_comp_status.status;

	if (desc->frm_type == dp_tx_frm_tso) {
		if (!desc->tso_desc)
			return;
		tso_seg = desc->tso_desc;
		nbuf_len = tso_seg->seg.total_len;
	} else {
		nbuf_len = qdf_nbuf_len(desc->nbuf);
	}

	netbuf = qdf_nbuf_alloc(NULL,
				roundup(nbuf_len + RESERVE_BYTES, 4),
				RESERVE_BYTES, 4, false);

	if (!netbuf)
		return;

	qdf_nbuf_put_tail(netbuf, nbuf_len);

	if (desc->frm_type == dp_tx_frm_tso) {
		uint8_t frag_cnt, num_frags = 0;
		int frag_len = 0;
		uint32_t tcp_seq_num;
		uint16_t ip_len;

		if (tso_seg->seg.num_frags > 0)
			num_frags = tso_seg->seg.num_frags - 1;

		/*Num of frags in a tso seg cannot be less than 2 */
		if (num_frags < 1) {
			pkt_capture_err("num of frags in tso segment is %d\n",
					(num_frags + 1));
			qdf_nbuf_free(netbuf);
			return;
		}

		tcp_seq_num = tso_seg->seg.tso_flags.tcp_seq_num;
		tcp_seq_num = qdf_cpu_to_be32(tcp_seq_num);

		ip_len = tso_seg->seg.tso_flags.ip_len;
		ip_len = qdf_cpu_to_be16(ip_len);

		for (frag_cnt = 0; frag_cnt <= num_frags; frag_cnt++) {
			qdf_mem_copy(
			qdf_nbuf_data(netbuf) + frag_len,
			tso_seg->seg.tso_frags[frag_cnt].vaddr,
			tso_seg->seg.tso_frags[frag_cnt].length);
			frag_len +=
				tso_seg->seg.tso_frags[frag_cnt].length;
		}

		qdf_mem_copy((qdf_nbuf_data(netbuf) +
			     IPV4_PKT_LEN_OFFSET),
			     &ip_len, sizeof(ip_len));
		qdf_mem_copy((qdf_nbuf_data(netbuf) +
			     IPV4_TCP_SEQ_NUM_OFFSET),
			     &tcp_seq_num, sizeof(tcp_seq_num));
	} else {
		qdf_mem_copy(qdf_nbuf_data(netbuf),
			     qdf_nbuf_data(desc->nbuf), nbuf_len);
	}

	if (qdf_unlikely(qdf_nbuf_headroom(netbuf) < txcap_hdr_size)) {
		netbuf = qdf_nbuf_realloc_headroom(netbuf,
						   txcap_hdr_size);
		if (!netbuf) {
			QDF_TRACE(QDF_MODULE_ID_PKT_CAPTURE,
				  QDF_TRACE_LEVEL_ERROR,
				  FL("No headroom"));
			return;
		}
	}

	if (!qdf_nbuf_push_head(netbuf, txcap_hdr_size)) {
		QDF_TRACE(QDF_MODULE_ID_PKT_CAPTURE,
			  QDF_TRACE_LEVEL_ERROR, FL("No headroom"));
		qdf_nbuf_free(netbuf);
		return;
	}

	ptr_pktcapture_hdr =
	(struct pkt_capture_tx_hdr_elem_t *)qdf_nbuf_data(netbuf);
	qdf_mem_copy(ptr_pktcapture_hdr, &pktcapture_hdr,
		     txcap_hdr_size);

	pkt_capture_datapkt_process(
		vdev_id, netbuf, TXRX_PROCESS_TYPE_DATA_TX_COMPL,
		tid, status, TXRX_PKTCAPTURE_PKT_FORMAT_8023,
		bssid, NULL, pktcapture_hdr.tx_retry_cnt);
}

/**
 * pkt_capture_is_frame_filter_set() - Check frame filter is set
 * @nbuf: buffer address
 * @frame_filter: frame filter address
 * @direction: frame direction
 *
 * Return: true, if filter bit is set
 *         false, if filter bit is not set
 */
static bool
pkt_capture_is_frame_filter_set(qdf_nbuf_t buf,
				struct pkt_capture_frame_filter *frame_filter,
				bool direction)
{
	enum pkt_capture_data_frame_type data_frame_type =
		PKT_CAPTURE_DATA_FRAME_TYPE_ALL;

	if (qdf_nbuf_is_ipv4_arp_pkt(buf)) {
		data_frame_type = PKT_CAPTURE_DATA_FRAME_TYPE_ARP;
	} else if (qdf_nbuf_is_ipv4_eapol_pkt(buf)) {
		data_frame_type = PKT_CAPTURE_DATA_FRAME_TYPE_EAPOL;
	} else if (qdf_nbuf_data_is_tcp_syn(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_SYN;
	} else if (qdf_nbuf_data_is_tcp_syn_ack(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_SYNACK;
	} else if (qdf_nbuf_data_is_tcp_syn(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_FIN;
	} else if (qdf_nbuf_data_is_tcp_syn_ack(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_FINACK;
	} else if (qdf_nbuf_data_is_tcp_ack(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_ACK;
	} else if (qdf_nbuf_data_is_tcp_rst(buf)) {
		data_frame_type =
			PKT_CAPTURE_DATA_FRAME_TYPE_TCP_RST;
	} else if (qdf_nbuf_is_ipv4_pkt(buf)) {
		if (qdf_nbuf_is_ipv4_dhcp_pkt(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_DHCPV4;
		else if (qdf_nbuf_is_icmp_pkt(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_ICMPV4;
		else if (qdf_nbuf_data_is_dns_query(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_DNSV4;
		else if (qdf_nbuf_data_is_dns_response(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_DNSV4;
	} else if (qdf_nbuf_is_ipv6_pkt(buf)) {
		if (qdf_nbuf_is_ipv6_dhcp_pkt(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_DHCPV6;
		else if (qdf_nbuf_is_icmpv6_pkt(buf))
			data_frame_type =
				PKT_CAPTURE_DATA_FRAME_TYPE_ICMPV6;
		/* need to add code for
		 * PKT_CAPTURE_DATA_FRAME_TYPE_DNSV6
		 */
	}
	/* Add code for
	 * PKT_CAPTURE_DATA_FRAME_TYPE_RTP
	 * PKT_CAPTURE_DATA_FRAME_TYPE_SIP
	 * PKT_CAPTURE_DATA_FRAME_QOS_NULL
	 */

	if (direction == IEEE80211_FC1_DIR_TODS) {
		if (data_frame_type & frame_filter->data_tx_frame_filter)
			return true;
		else
			return false;
	} else {
		if (data_frame_type & frame_filter->data_rx_frame_filter)
			return true;
		else
			return false;
	}
}

void pkt_capture_callback(void *soc, enum WDI_EVENT event, void *log_data,
			  u_int16_t peer_id, uint32_t status)
{
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_frame_filter *frame_filter;
	uint16_t vdev_id = 0;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	ret = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(ret))
		return;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		pkt_capture_vdev_put_ref(vdev);
		return;
	}

	frame_filter = &vdev_priv->frame_filter;

	switch (event) {
	case WDI_EVENT_PKT_CAPTURE_TX_DATA:
	{
		struct dp_tx_desc_s *desc = log_data;

		if (!frame_filter->data_tx_frame_filter) {
			pkt_capture_vdev_put_ref(vdev);
			return;
		}

		if (frame_filter->data_tx_frame_filter &
		    PKT_CAPTURE_DATA_FRAME_TYPE_ALL) {
			pkt_capture_process_tx_data(soc, log_data,
						    vdev_id, status);
		} else if (pkt_capture_is_frame_filter_set(
			   desc->nbuf, frame_filter, IEEE80211_FC1_DIR_TODS)) {
			pkt_capture_process_tx_data(soc, log_data,
						    vdev_id, status);
		}
		break;
	}

	case WDI_EVENT_PKT_CAPTURE_RX_DATA:
	{
		qdf_nbuf_t nbuf = (qdf_nbuf_t)log_data;

		if (!frame_filter->data_rx_frame_filter) {
			/*
			 * Rx offload packets are delivered only to pkt capture
			 * component and not to stack so free them.
			 */
			if (status == RX_OFFLOAD_PKT)
				qdf_nbuf_free(nbuf);

			pkt_capture_vdev_put_ref(vdev);
			return;
		}

		if (frame_filter->data_rx_frame_filter &
		    PKT_CAPTURE_DATA_FRAME_TYPE_ALL) {
			pkt_capture_msdu_process_pkts(bssid, log_data,
						      vdev_id, soc, status);
		} else if (pkt_capture_is_frame_filter_set(
			   nbuf, frame_filter, IEEE80211_FC1_DIR_FROMDS)) {
			pkt_capture_msdu_process_pkts(bssid, log_data,
						      vdev_id, soc, status);
		} else {
			if (status == RX_OFFLOAD_PKT)
				qdf_nbuf_free(nbuf);
		}

		break;
	}

	case WDI_EVENT_PKT_CAPTURE_RX_DATA_NO_PEER:
	{
		qdf_nbuf_t nbuf = (qdf_nbuf_t)log_data;

		if (!frame_filter->data_rx_frame_filter) {
			/*
			 * Rx offload packets are delivered only to pkt capture
			 * component and not to stack so free them
			 */
			if (status == RX_OFFLOAD_PKT)
				qdf_nbuf_free(nbuf);

			pkt_capture_vdev_put_ref(vdev);
			return;
		}

		if (frame_filter->data_rx_frame_filter &
		    PKT_CAPTURE_DATA_FRAME_TYPE_ALL) {
			pkt_capture_process_rx_data_no_peer(soc, vdev_id, bssid,
							    status, nbuf);
		} else if (pkt_capture_is_frame_filter_set(
			   nbuf, frame_filter, IEEE80211_FC1_DIR_FROMDS)) {
			pkt_capture_process_rx_data_no_peer(soc, vdev_id, bssid,
							    status, nbuf);
		} else {
			if (status == RX_OFFLOAD_PKT)
				qdf_nbuf_free(nbuf);
		}

		break;
	}

	case WDI_EVENT_PKT_CAPTURE_OFFLOAD_TX_DATA:
	{
		struct htt_tx_offload_deliver_ind_hdr_t *offload_deliver_msg;
		bool is_pkt_during_roam = false;
		uint32_t freq = 0;
		qdf_nbuf_t buf = log_data +
				sizeof(struct htt_tx_offload_deliver_ind_hdr_t);

		if (!frame_filter->data_tx_frame_filter) {
			pkt_capture_vdev_put_ref(vdev);
			return;
		}

		offload_deliver_msg =
		(struct htt_tx_offload_deliver_ind_hdr_t *)log_data;
		is_pkt_during_roam =
		(offload_deliver_msg->reserved_2 ? true : false);

		if (is_pkt_during_roam) {
			vdev_id = HTT_INVALID_VDEV;
			freq =
			(uint32_t)offload_deliver_msg->reserved_3;
		} else {
			vdev_id = offload_deliver_msg->vdev_id;
		}

		if (frame_filter->data_tx_frame_filter &
		    PKT_CAPTURE_DATA_FRAME_TYPE_ALL) {
			pkt_capture_offload_deliver_indication_handler(
							log_data,
							vdev_id, bssid, soc);
		} else if (pkt_capture_is_frame_filter_set(
			   buf, frame_filter, IEEE80211_FC1_DIR_TODS)) {
			pkt_capture_offload_deliver_indication_handler(
							log_data,
							vdev_id, bssid, soc);
		}
		break;
	}

	case WDI_EVENT_PKT_CAPTURE_PPDU_STATS:
		pkt_capture_process_ppdu_stats(log_data);
		break;

	default:
		break;
	}
	pkt_capture_vdev_put_ref(vdev);
}

#else
static void pkt_capture_wdi_event_subscribe(struct wlan_objmgr_psoc *psoc)
{
}

static void pkt_capture_wdi_event_unsubscribe(struct wlan_objmgr_psoc *psoc)
{
}
#endif

struct wlan_objmgr_vdev *pkt_capture_get_vdev()
{
	return gp_pkt_capture_vdev;
}

enum pkt_capture_mode pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pkt_psoc_priv *psoc_priv;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return PACKET_CAPTURE_MODE_DISABLE;
	}

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc_priv is NULL");
		return PACKET_CAPTURE_MODE_DISABLE;
	}

	return psoc_priv->cfg_param.pkt_capture_mode;
}

bool pkt_capture_is_tx_mgmt_enable(struct wlan_objmgr_pdev *pdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum pkt_capture_config config;

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("failed to get vdev ref");
		return false;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev_priv is NULL");
		pkt_capture_vdev_put_ref(vdev);
		return false;
	}

	config = pkt_capture_get_pktcap_config(vdev);

	if (!(vdev_priv->frame_filter.mgmt_tx_frame_filter &
	    PKT_CAPTURE_MGMT_FRAME_TYPE_ALL)) {
		if (!(config & PACKET_CAPTURE_CONFIG_QOS_ENABLE)) {
			pkt_capture_vdev_put_ref(vdev);
			return false;
		}
	}

	pkt_capture_vdev_put_ref(vdev);
	return true;
}

QDF_STATUS
pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
			       QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
			       void *context)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_psoc_priv *psoc_priv;
	struct wlan_objmgr_psoc *psoc;
	enum pkt_capture_mode mode;
	QDF_STATUS status;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv->cb_ctx->mon_cb = mon_cb;
	vdev_priv->cb_ctx->mon_ctx = context;

	status = pkt_capture_mgmt_rx_ops(psoc, true);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to register pkt capture mgmt rx ops");
		goto mgmt_rx_ops_fail;
	}

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc_priv is NULL");
		return QDF_STATUS_E_INVAL;
	}

	target_if_pkt_capture_register_tx_ops(&psoc_priv->tx_ops);
	target_if_pkt_capture_register_rx_ops(&psoc_priv->rx_ops);
	pkt_capture_wdi_event_subscribe(psoc);
	pkt_capture_record_channel(vdev);

	status = tgt_pkt_capture_register_ev_handler(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto register_ev_handlers_fail;

	/*
	 * set register event bit so that mon thread will start
	 * processing packets in queue.
	 */
	set_bit(PKT_CAPTURE_REGISTER_EVENT,
		&vdev_priv->mon_ctx->mon_event_flag);

	mode = pkt_capture_get_mode(psoc);
	status = tgt_pkt_capture_send_mode(vdev, mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Unable to send packet capture mode to fw");
		goto send_mode_fail;
	}

	return QDF_STATUS_SUCCESS;

send_mode_fail:
	tgt_pkt_capture_unregister_ev_handler(vdev);
register_ev_handlers_fail:
	pkt_capture_mgmt_rx_ops(psoc, false);
mgmt_rx_ops_fail:
	vdev_priv->cb_ctx->mon_cb = NULL;
	vdev_priv->cb_ctx->mon_ctx = NULL;

	return status;
}

QDF_STATUS pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = tgt_pkt_capture_send_mode(vdev, PACKET_CAPTURE_MODE_DISABLE);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Unable to send packet capture mode to fw");

	/*
	 * Clear packet capture register event so that mon thread will
	 * stop processing packets in queue.
	 */
	clear_bit(PKT_CAPTURE_REGISTER_EVENT,
		  &vdev_priv->mon_ctx->mon_event_flag);
	set_bit(PKT_CAPTURE_RX_POST_EVENT,
		&vdev_priv->mon_ctx->mon_event_flag);
	wake_up_interruptible(&vdev_priv->mon_ctx->mon_wait_queue);

	/*
	 * Wait till current packet process completes in mon thread and
	 * flush the remaining packet in queue.
	 */
	wait_for_completion(&vdev_priv->mon_ctx->mon_register_event);
	pkt_capture_drop_monpkt(vdev_priv->mon_ctx);

	pkt_capture_wdi_event_unsubscribe(psoc);
	status = tgt_pkt_capture_unregister_ev_handler(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Unable to unregister event handlers");

	status = pkt_capture_mgmt_rx_ops(psoc, false);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to unregister pkt capture mgmt rx ops");

	vdev_priv->cb_ctx->mon_cb = NULL;
	vdev_priv->cb_ctx->mon_ctx = NULL;

	return QDF_STATUS_SUCCESS;
}

void pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				 enum pkt_capture_mode mode)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (vdev_priv)
		vdev_priv->cfg_params.pkt_capture_mode = mode;
	else
		pkt_capture_err("vdev_priv is NULL");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
}

enum pkt_capture_mode
pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc)
{
	enum pkt_capture_mode mode = PACKET_CAPTURE_MODE_DISABLE;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return 0;
	}

	if (!pkt_capture_get_mode(psoc))
		return 0;

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev)
		return 0;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv)
		pkt_capture_err("vdev_priv is NULL");
	else
		mode = vdev_priv->cfg_params.pkt_capture_mode;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
	return mode;
}

void pkt_capture_set_pktcap_config(struct wlan_objmgr_vdev *vdev,
				   enum pkt_capture_config config)
{
	struct pkt_capture_vdev_priv *vdev_priv;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (vdev_priv)
		vdev_priv->cfg_params.pkt_capture_config = config;
	else
		pkt_capture_err("vdev_priv is NULL");
}

enum pkt_capture_config
pkt_capture_get_pktcap_config(struct wlan_objmgr_vdev *vdev)
{
	enum pkt_capture_config config = 0;
	struct pkt_capture_vdev_priv *vdev_priv;

	if (!vdev)
		return 0;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv)
		pkt_capture_err("vdev_priv is NULL");
	else
		config = vdev_priv->cfg_params.pkt_capture_config;

	return config;
}

/**
 * pkt_capture_callback_ctx_create() - Create packet capture callback context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
pkt_capture_callback_ctx_create(struct pkt_capture_vdev_priv *vdev_priv)
{
	struct pkt_capture_cb_context *cb_ctx;

	cb_ctx = qdf_mem_malloc(sizeof(*cb_ctx));
	if (!cb_ctx)
		return QDF_STATUS_E_NOMEM;

	vdev_priv->cb_ctx = cb_ctx;

	return QDF_STATUS_SUCCESS;
}

/**
 * pkt_capture_callback_ctx_destroy() - Destroy packet capture callback context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Return: None
 */
static void
pkt_capture_callback_ctx_destroy(struct pkt_capture_vdev_priv *vdev_priv)
{
	qdf_mem_free(vdev_priv->cb_ctx);
}

/**
 * pkt_capture_mon_context_create() - Create packet capture mon context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * This function allocates memory for packet capture mon context
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
pkt_capture_mon_context_create(struct pkt_capture_vdev_priv *vdev_priv)
{
	struct pkt_capture_mon_context *mon_context;

	mon_context = qdf_mem_malloc(sizeof(*mon_context));
	if (!mon_context)
		return QDF_STATUS_E_NOMEM;

	vdev_priv->mon_ctx = mon_context;

	return QDF_STATUS_SUCCESS;
}

/**
 * pkt_capture_mon_context_destroy() - Destroy packet capture mon context
 * @vdev_priv: pointer to packet capture vdev priv obj
 *
 * Free packet capture mon context
 *
 * Return: None
 */
static void
pkt_capture_mon_context_destroy(struct pkt_capture_vdev_priv *vdev_priv)
{
	qdf_mem_free(vdev_priv->mon_ctx);
}

uint32_t pkt_capture_drop_nbuf_list(qdf_nbuf_t buf_list)
{
	qdf_nbuf_t buf, next_buf;
	uint32_t num_dropped = 0;

	buf = buf_list;
	while (buf) {
		QDF_NBUF_CB_RX_PEER_CACHED_FRM(buf) = 1;
		next_buf = qdf_nbuf_queue_next(buf);
		qdf_nbuf_free(buf);
		buf = next_buf;
		num_dropped++;
	}
	return num_dropped;
}

/**
 * pkt_capture_cfg_init() - Initialize packet capture cfg ini params
 * @psoc_priv: psoc private object
 *
 * Return: None
 */
static void
pkt_capture_cfg_init(struct pkt_psoc_priv *psoc_priv)
{
	struct pkt_capture_cfg *cfg_param;

	cfg_param = &psoc_priv->cfg_param;

	cfg_param->pkt_capture_mode = cfg_get(psoc_priv->psoc,
					      CFG_PKT_CAPTURE_MODE);
}

QDF_STATUS
pkt_capture_vdev_create_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pkt_capture_mon_context *mon_ctx;
	struct pkt_capture_vdev_priv *vdev_priv;
	QDF_STATUS status;

	if ((wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE) ||
	    !pkt_capture_get_mode(wlan_vdev_get_psoc(vdev)))
		return QDF_STATUS_SUCCESS;

	vdev_priv = qdf_mem_malloc(sizeof(*vdev_priv));
	if (!vdev_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_component_obj_attach(
					vdev,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					vdev_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to attach vdev component obj");
		goto free_vdev_priv;
	}

	vdev_priv->vdev = vdev;
	gp_pkt_capture_vdev = vdev;

	status = pkt_capture_callback_ctx_create(vdev_priv);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pkt_capture_err("Failed to create callback context");
		goto detach_vdev_priv;
	}

	status = pkt_capture_mon_context_create(vdev_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to create mon context");
		goto destroy_pkt_capture_cb_context;
	}

	mon_ctx = vdev_priv->mon_ctx;

	status = pkt_capture_alloc_mon_thread(mon_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to alloc mon thread");
		goto destroy_mon_context;
	}

	status = pkt_capture_open_mon_thread(mon_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to open mon thread");
		goto open_mon_thread_fail;
	}
	qdf_spinlock_create(&vdev_priv->lock_q);
	qdf_list_create(&vdev_priv->ppdu_stats_q, PPDU_STATS_Q_MAX_SIZE);

	return status;

open_mon_thread_fail:
	pkt_capture_free_mon_pkt_freeq(mon_ctx);
destroy_mon_context:
	pkt_capture_mon_context_destroy(vdev_priv);
destroy_pkt_capture_cb_context:
	pkt_capture_callback_ctx_destroy(vdev_priv);
detach_vdev_priv:
	wlan_objmgr_vdev_component_obj_detach(vdev,
					      WLAN_UMAC_COMP_PKT_CAPTURE,
					      vdev_priv);
free_vdev_priv:
	qdf_mem_free(vdev_priv);
	return status;
}

QDF_STATUS
pkt_capture_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_ppdu_stats_q_node *stats_node;
	qdf_list_node_t *node;
	QDF_STATUS status;

	if ((wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE) ||
	    !pkt_capture_get_mode(wlan_vdev_get_psoc(vdev)))
		return QDF_STATUS_SUCCESS;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	while (qdf_list_remove_front(&vdev_priv->ppdu_stats_q, &node)
	       == QDF_STATUS_SUCCESS) {
		stats_node = qdf_container_of(
			node, struct pkt_capture_ppdu_stats_q_node, node);
		qdf_mem_free(stats_node);
	}
	qdf_list_destroy(&vdev_priv->ppdu_stats_q);
	qdf_spinlock_destroy(&vdev_priv->lock_q);

	status = wlan_objmgr_vdev_component_obj_detach(
					vdev,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					vdev_priv);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to detach vdev component obj");

	pkt_capture_close_mon_thread(vdev_priv->mon_ctx);
	pkt_capture_mon_context_destroy(vdev_priv);
	pkt_capture_callback_ctx_destroy(vdev_priv);

	qdf_mem_free(vdev_priv);
	gp_pkt_capture_vdev = NULL;
	return status;
}

QDF_STATUS
pkt_capture_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pkt_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
				WLAN_UMAC_COMP_PKT_CAPTURE,
				psoc_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to attach psoc component obj");
		goto free_psoc_priv;
	}

	psoc_priv->psoc = psoc;
	pkt_capture_cfg_init(psoc_priv);

	return status;

free_psoc_priv:
	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS
pkt_capture_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pkt_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_PKT_CAPTURE,
					psoc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to detach psoc component obj");
		return status;
	}

	qdf_mem_free(psoc_priv);
	return status;
}

void pkt_capture_record_channel(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_channel *des_chan;
	cdp_config_param_type val;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_vdev_get_psoc(vdev);

	if (!pkt_capture_get_mode(psoc))
		return;
	/*
	 * Record packet capture channel here
	 */
	des_chan = vdev->vdev_mlme.des_chan;
	val.cdp_pdev_param_monitor_chan = des_chan->ch_ieee;
	cdp_txrx_set_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(pdev),
				CDP_MONITOR_CHANNEL, val);
	val.cdp_pdev_param_mon_freq = des_chan->ch_freq;
	cdp_txrx_set_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(pdev),
				CDP_MONITOR_FREQUENCY, val);
}

QDF_STATUS pkt_capture_set_filter(struct pkt_capture_frame_filter frame_filter,
				  struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_psoc *psoc;
	enum pkt_capture_mode mode = PACKET_CAPTURE_MODE_DISABLE;
	ol_txrx_soc_handle soc;
	QDF_STATUS status;
	enum pkt_capture_config config = 0;
	bool check_enable_beacon = 0, send_bcn = 0;
	struct vdev_mlme_obj *vdev_mlme;
	uint32_t bcn_interval, nth_beacon_value;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("vdev_priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc) {
		pkt_capture_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE))
		vdev_priv->frame_filter.data_tx_frame_filter =
			frame_filter.data_tx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE))
		vdev_priv->frame_filter.data_rx_frame_filter =
			frame_filter.data_rx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE))
		vdev_priv->frame_filter.mgmt_tx_frame_filter =
			frame_filter.mgmt_tx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE))
		vdev_priv->frame_filter.mgmt_rx_frame_filter =
			frame_filter.mgmt_rx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE))
		vdev_priv->frame_filter.ctrl_tx_frame_filter =
			frame_filter.ctrl_tx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE))
		vdev_priv->frame_filter.ctrl_rx_frame_filter =
			frame_filter.ctrl_rx_frame_filter;

	if (frame_filter.vendor_attr_to_set &
	    BIT(PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL)) {
		if (frame_filter.connected_beacon_interval !=
		    vdev_priv->frame_filter.connected_beacon_interval) {
			vdev_priv->frame_filter.connected_beacon_interval =
				frame_filter.connected_beacon_interval;
			send_bcn = 1;
		}
	}

	if (vdev_priv->frame_filter.mgmt_tx_frame_filter)
		mode |= PACKET_CAPTURE_MODE_MGMT_ONLY;

	if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
	    PKT_CAPTURE_MGMT_FRAME_TYPE_ALL) {
		mode |= PACKET_CAPTURE_MODE_MGMT_ONLY;
		config |= PACKET_CAPTURE_CONFIG_BEACON_ENABLE;
		config |= PACKET_CAPTURE_CONFIG_OFF_CHANNEL_BEACON_ENABLE;
	} else {
		if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
		    PKT_CAPTURE_MGMT_CONNECT_NO_BEACON) {
			mode |= PACKET_CAPTURE_MODE_MGMT_ONLY;
			config |= PACKET_CAPTURE_CONFIG_NO_BEACON_ENABLE;
		} else {
			check_enable_beacon = 1;
		}
	}

	if (check_enable_beacon) {
		if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
		    PKT_CAPTURE_MGMT_CONNECT_BEACON)
			config |= PACKET_CAPTURE_CONFIG_BEACON_ENABLE;

		if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
		    PKT_CAPTURE_MGMT_CONNECT_SCAN_BEACON)
			config |=
				PACKET_CAPTURE_CONFIG_OFF_CHANNEL_BEACON_ENABLE;
	}

	if (vdev_priv->frame_filter.data_tx_frame_filter ||
	    vdev_priv->frame_filter.data_rx_frame_filter)
		mode |= PACKET_CAPTURE_MODE_DATA_ONLY;

	if (mode != pkt_capture_get_pktcap_mode(psoc)) {
		status = tgt_pkt_capture_send_mode(vdev, mode);
		if (QDF_IS_STATUS_ERROR(status)) {
			pkt_capture_err("Unable to send packet capture mode");
			return status;
		}

		if (mode & PACKET_CAPTURE_MODE_DATA_ONLY)
			cdp_set_pkt_capture_mode(soc, true);
	}

	if (vdev_priv->frame_filter.ctrl_tx_frame_filter ||
	    vdev_priv->frame_filter.ctrl_rx_frame_filter)
		config |= PACKET_CAPTURE_CONFIG_TRIGGER_ENABLE;

	if ((vdev_priv->frame_filter.data_tx_frame_filter &
	    PKT_CAPTURE_DATA_FRAME_TYPE_ALL) ||
	    (vdev_priv->frame_filter.data_tx_frame_filter &
	    PKT_CAPTURE_DATA_FRAME_QOS_NULL))
		config |= PACKET_CAPTURE_CONFIG_QOS_ENABLE;

	if (config != pkt_capture_get_pktcap_config(vdev)) {
		status = tgt_pkt_capture_send_config(vdev, config);
		if (QDF_IS_STATUS_ERROR(status)) {
			pkt_capture_err("packet capture send config failed");
			return status;
		}
	}

	if (send_bcn) {
		vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
							vdev,
							WLAN_UMAC_COMP_MLME);

		if (!vdev_mlme)
			return QDF_STATUS_E_FAILURE;

		wlan_util_vdev_mlme_get_param(vdev_mlme,
					      WLAN_MLME_CFG_BEACON_INTERVAL,
					      &bcn_interval);

		if (bcn_interval) {
			nth_beacon_value =
				vdev_priv->
				frame_filter.connected_beacon_interval /
				bcn_interval;

			status = tgt_pkt_capture_send_beacon_interval(
							vdev,
							nth_beacon_value);

			if (QDF_IS_STATUS_ERROR(status)) {
				pkt_capture_err("send beacon interval fail");
				return status;
			}
		}
	}
	return QDF_STATUS_SUCCESS;
}
