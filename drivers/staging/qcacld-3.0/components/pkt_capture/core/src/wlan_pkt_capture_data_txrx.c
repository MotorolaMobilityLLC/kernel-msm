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

#include <wlan_pkt_capture_data_txrx.h>
#include <wlan_pkt_capture_main.h>
#include <enet.h>
#include <wlan_reg_services_api.h>
#include <cds_ieee80211_common.h>
#include <ol_txrx_htt_api.h>
#include "wlan_policy_mgr_ucfg.h"
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
#include "dp_internal.h"
#include "cds_utils.h"
#include "htt_ppdu_stats.h"
#endif

#define RESERVE_BYTES (100)

/**
 * pkt_capture_txrx_status_map() - map Tx status for data packets
 * with packet capture Tx status
 * @status: Tx status
 *
 * Return: pkt_capture_tx_status enum
 */
static enum pkt_capture_tx_status
pkt_capture_txrx_status_map(uint8_t status)
{
	enum pkt_capture_tx_status tx_status;

	switch (status) {
	case htt_tx_status_ok:
		tx_status = pkt_capture_tx_status_ok;
		break;
	case htt_tx_status_no_ack:
		tx_status = pkt_capture_tx_status_no_ack;
		break;
	default:
		tx_status = pkt_capture_tx_status_discard;
		break;
	}

	return tx_status;
}

/**
 * pkt_capture_get_tx_rate() - get tx rate for tx packet
 * @preamble_type: preamble type
 * @rate: rate code
 * @preamble: preamble
 *
 * Return: rate
 */
static unsigned char pkt_capture_get_tx_rate(
					uint8_t preamble_type,
					uint8_t rate,
					uint8_t *preamble)
{
	char ret = 0x0;
	*preamble = LONG_PREAMBLE;

	if (preamble_type == 0) {
		switch (rate) {
		case 0x0:
			ret = 0x60;
			break;
		case 0x1:
			ret = 0x30;
			break;
		case 0x2:
			ret = 0x18;
			break;
		case 0x3:
			ret = 0x0c;
			break;
		case 0x4:
			ret = 0x6c;
			break;
		case 0x5:
			ret = 0x48;
			break;
		case 0x6:
			ret = 0x24;
			break;
		case 0x7:
			ret = 0x12;
			break;
		default:
			break;
		}
	} else if (preamble_type == 1) {
		switch (rate) {
		case 0x0:
			ret = 0x16;
			*preamble = LONG_PREAMBLE;
		case 0x1:
			ret = 0xB;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x2:
			ret = 0x4;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x3:
			ret = 0x2;
			*preamble = LONG_PREAMBLE;
			break;
		case 0x4:
			ret = 0x16;
			*preamble = SHORT_PREAMBLE;
			break;
		case 0x5:
			ret = 0xB;
			*preamble = SHORT_PREAMBLE;
			break;
		case 0x6:
			ret = 0x4;
			*preamble = SHORT_PREAMBLE;
			break;
		default:
			break;
		}
	} else {
		qdf_print("Invalid rate info\n");
	}
	return ret;
}

/**
 * pkt_capture_tx_get_phy_info() - get phy info for tx packets for pkt
 * capture mode(normal tx + offloaded tx) to prepare radiotap header
 * @pktcapture_hdr: tx data header
 * @tx_status: tx status to be updated with phy info
 *
 * Return: none
 */
static void pkt_capture_tx_get_phy_info(
		struct pkt_capture_tx_hdr_elem_t *pktcapture_hdr,
		struct mon_rx_status *tx_status)
{
	uint8_t preamble = 0;
	uint8_t preamble_type = pktcapture_hdr->preamble;
	uint8_t mcs = 0;

	switch (preamble_type) {
	case 0x0:
	case 0x1:
	/* legacy */
		tx_status->rate = pkt_capture_get_tx_rate(
						preamble_type,
						pktcapture_hdr->rate,
						&preamble);
		break;
	case 0x2:
		tx_status->ht_flags = 1;
		if (pktcapture_hdr->nss == 2)
			mcs = 8 + pktcapture_hdr->mcs;
		else
			mcs = pktcapture_hdr->mcs;
		break;
	case 0x3:
		tx_status->vht_flags = 1;
		mcs = pktcapture_hdr->mcs;
		tx_status->vht_flag_values3[0] =
			mcs << 0x4 | (pktcapture_hdr->nss + 1);
		tx_status->vht_flag_values2 = pktcapture_hdr->bw;
		break;
	case 0x4:
		tx_status->he_flags = 1;
		tx_status->he_data1 |=
			IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA1_STBC_KNOWN;
		tx_status->he_data2 |= IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN;
		tx_status->he_data3 |= (pktcapture_hdr->mcs << 0x8) |
					(pktcapture_hdr->ldpc << 0xd) |
					(pktcapture_hdr->stbc << 0xf);
		tx_status->he_data5 |=
			(pktcapture_hdr->bw | (pktcapture_hdr->sgi << 0x4));
		tx_status->he_data6 |= pktcapture_hdr->nss;
	default:
		break;
	}

	if (preamble == 0)
		tx_status->ofdm_flag = 1;
	else if (preamble == 1)
		tx_status->cck_flag = 1;

	tx_status->mcs = mcs;
	tx_status->bw = pktcapture_hdr->bw;
	tx_status->nr_ant = pktcapture_hdr->nss;
	tx_status->nss = pktcapture_hdr->nss;
	tx_status->is_stbc = pktcapture_hdr->stbc;
	tx_status->sgi = pktcapture_hdr->sgi;
	tx_status->ldpc = pktcapture_hdr->ldpc;
	tx_status->beamformed = pktcapture_hdr->beamformed;
	tx_status->rtap_flags |= ((preamble == 1) ? BIT(1) : 0);
}

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_update_tx_status() - tx status for tx packets, for
 * pkt capture mode(normal tx + offloaded tx) to prepare radiotap header
 * @pdev: device handler
 * @tx_status: tx status to be updated
 * @mon_hdr: tx data header
 *
 * Return: none
 */
static void
pkt_capture_update_tx_status(
			htt_pdev_handle pdev,
			struct mon_rx_status *tx_status,
			struct pkt_capture_tx_hdr_elem_t *pktcapture_hdr)
{
	struct mon_channel *ch_info = &pdev->mon_ch_info;

	tx_status->tsft = (u_int64_t)(pktcapture_hdr->timestamp);
	tx_status->chan_freq = ch_info->ch_freq;
	tx_status->chan_num = ch_info->ch_num;

	pkt_capture_tx_get_phy_info(pktcapture_hdr, tx_status);

	if (pktcapture_hdr->preamble == 0)
		tx_status->ofdm_flag = 1;
	else if (pktcapture_hdr->preamble == 1)
		tx_status->cck_flag = 1;

	tx_status->ant_signal_db = pktcapture_hdr->rssi_comb;
	tx_status->rssi_comb = pktcapture_hdr->rssi_comb;
	tx_status->tx_status = pktcapture_hdr->status;
	tx_status->tx_retry_cnt = pktcapture_hdr->tx_retry_cnt;
	tx_status->add_rtap_ext = true;
}
#else
static void
pkt_capture_update_tx_status(
			void *context,
			struct mon_rx_status *tx_status,
			struct pkt_capture_tx_hdr_elem_t *pktcapture_hdr)
{
	struct connection_info info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev = context;
	htt_ppdu_stats_for_smu_tlv *smu;
	struct wlan_objmgr_psoc *psoc;
	struct pkt_capture_ppdu_stats_q_node *q_node;
	qdf_list_node_t *node;
	uint32_t conn_count;
	uint8_t vdev_id;
	int i;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pkt_capture_err("Failed to get psoc");
		return;
	}

	vdev_id = wlan_vdev_get_id(vdev);

	/* Update the connected channel info from policy manager */
	conn_count = policy_mgr_get_connection_info(psoc, info);
	for (i = 0; i < conn_count; i++) {
		if (info[i].vdev_id == vdev_id) {
			tx_status->chan_freq = info[0].ch_freq;
			tx_status->chan_num = info[0].channel;
			break;
		}
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (qdf_unlikely(!vdev_priv))
		goto skip_ppdu_stats;

	/* Fill the nss received from ppdu_stats */
	pktcapture_hdr->nss = vdev_priv->tx_nss;

	/* Remove the ppdu stats from front of list and fill it in tx_status */
	qdf_spin_lock(&vdev_priv->lock_q);
	if (QDF_STATUS_SUCCESS ==
	    qdf_list_remove_front(&vdev_priv->ppdu_stats_q, &node)) {
		q_node = qdf_container_of(
			node, struct pkt_capture_ppdu_stats_q_node, node);
		smu = (htt_ppdu_stats_for_smu_tlv *)(q_node->buf);
		tx_status->prev_ppdu_id = smu->ppdu_id;
		tx_status->start_seq = smu->start_seq;
		tx_status->tid = smu->tid_num;

		if (smu->win_size == 8)
			qdf_mem_copy(tx_status->ba_bitmap, smu->ba_bitmap,
				     8 * sizeof(uint32_t));
		else if (smu->win_size == 2)
			qdf_mem_copy(tx_status->ba_bitmap, smu->ba_bitmap,
				     2 * sizeof(uint32_t));

		qdf_mem_free(q_node);
	}
	qdf_spin_unlock(&vdev_priv->lock_q);

skip_ppdu_stats:
	pkt_capture_tx_get_phy_info(pktcapture_hdr, tx_status);

	tx_status->tsft = (u_int64_t)(pktcapture_hdr->timestamp);
	tx_status->ant_signal_db = pktcapture_hdr->rssi_comb;
	tx_status->rssi_comb = pktcapture_hdr->rssi_comb;
	tx_status->tx_status = pktcapture_hdr->status;
	tx_status->tx_retry_cnt = pktcapture_hdr->tx_retry_cnt;
	tx_status->ppdu_id = pktcapture_hdr->ppdu_id;
	tx_status->add_rtap_ext = true;
	tx_status->add_rtap_ext2 = true;
}
#endif

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_rx_convert8023to80211() - convert 802.3 packet to 802.11
 * format from rx desc
 * @bssid: bssid
 * @msdu: netbuf
 * @desc: rx desc
 *
 * Return: none
 */
static void
pkt_capture_rx_convert8023to80211(uint8_t *bssid, qdf_nbuf_t msdu, void *desc)
{
	struct ethernet_hdr_t *eth_hdr;
	struct llc_snap_hdr_t *llc_hdr;
	struct ieee80211_frame *wh;
	uint8_t hdsize, new_hdsize;
	struct ieee80211_qoscntl *qos_cntl;
	uint16_t seq_no;
	uint8_t localbuf[sizeof(struct ieee80211_qosframe_htc_addr4) +
			sizeof(struct llc_snap_hdr_t)];
	const uint8_t ethernet_II_llc_snap_header_prefix[] = {
					0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	uint16_t ether_type;

	struct htt_host_rx_desc_base *rx_desc = desc;

	eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
	hdsize = sizeof(struct ethernet_hdr_t);
	wh = (struct ieee80211_frame *)localbuf;

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(uint16_t *)wh->i_dur = 0;

	new_hdsize = 0;

	/* DA , BSSID , SA */
	qdf_mem_copy(wh->i_addr1, eth_hdr->dest_addr,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(wh->i_addr2, bssid,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(wh->i_addr3, eth_hdr->src_addr,
		     QDF_MAC_ADDR_SIZE);

	wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;

	if (rx_desc->attention.more_data)
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;

	if (rx_desc->attention.power_mgmt)
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;

	if (rx_desc->attention.fragment)
		wh->i_fc[1] |= IEEE80211_FC1_MORE_FRAG;

	if (rx_desc->attention.order)
		wh->i_fc[1] |= IEEE80211_FC1_ORDER;

	if (rx_desc->mpdu_start.retry)
		wh->i_fc[1] |= IEEE80211_FC1_RETRY;

	seq_no = rx_desc->mpdu_start.seq_num;
	seq_no = (seq_no << IEEE80211_SEQ_SEQ_SHIFT) & IEEE80211_SEQ_SEQ_MASK;
	qdf_mem_copy(wh->i_seq, &seq_no, sizeof(seq_no));

	new_hdsize = sizeof(struct ieee80211_frame);

	if (rx_desc->attention.non_qos == 0) {
		qos_cntl =
		(struct ieee80211_qoscntl *)(localbuf + new_hdsize);
		qos_cntl->i_qos[0] =
		(rx_desc->mpdu_start.tid & IEEE80211_QOS_TID);
		wh->i_fc[0] |= QDF_IEEE80211_FC0_SUBTYPE_QOS;

		qos_cntl->i_qos[1] = 0;
		new_hdsize += sizeof(struct ieee80211_qoscntl);
	}

	/*
	 * Prepare llc Header
	 */
	llc_hdr = (struct llc_snap_hdr_t *)(localbuf + new_hdsize);
	ether_type = (eth_hdr->ethertype[0] << 8) |
			(eth_hdr->ethertype[1]);
	if (ether_type >= ETH_P_802_3_MIN) {
		qdf_mem_copy(llc_hdr,
			     ethernet_II_llc_snap_header_prefix,
			     sizeof
			     (ethernet_II_llc_snap_header_prefix));
		if (ether_type == ETHERTYPE_AARP ||
		    ether_type == ETHERTYPE_IPX) {
			llc_hdr->org_code[2] =
				BTEP_SNAP_ORGCODE_2;
			/* 0xf8; bridge tunnel header */
		}
		llc_hdr->ethertype[0] = eth_hdr->ethertype[0];
		llc_hdr->ethertype[1] = eth_hdr->ethertype[1];
		new_hdsize += sizeof(struct llc_snap_hdr_t);
	}

	/*
	 * Remove 802.3 Header by adjusting the head
	 */
	qdf_nbuf_pull_head(msdu, hdsize);

	/*
	 * Adjust the head and prepare 802.11 Header
	 */
	qdf_nbuf_push_head(msdu, new_hdsize);
	qdf_mem_copy(qdf_nbuf_data(msdu), localbuf, new_hdsize);
}
#else
static void
pkt_capture_rx_convert8023to80211(hal_soc_handle_t hal_soc_hdl,
				  qdf_nbuf_t msdu, void *desc)
{
	struct ethernet_hdr_t *eth_hdr;
	struct llc_snap_hdr_t *llc_hdr;
	struct ieee80211_frame *wh;
	uint8_t *pwh;
	uint8_t hdsize, new_hdsize;
	struct ieee80211_qoscntl *qos_cntl;
	static uint8_t first_msdu_hdr[sizeof(struct ieee80211_frame)];
	uint8_t localbuf[sizeof(struct ieee80211_qosframe_htc_addr4) +
			sizeof(struct llc_snap_hdr_t)];
	const uint8_t ethernet_II_llc_snap_header_prefix[] = {
					0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	uint16_t ether_type;

	uint32_t tid = 0;

	eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
	hdsize = sizeof(struct ethernet_hdr_t);

	wh = (struct ieee80211_frame *)localbuf;

	new_hdsize = sizeof(struct ieee80211_frame);

	/*
	 * Only first msdu in mpdu has rx_tlv_hdr(802.11 hdr) filled by HW, so
	 * copy the 802.11 hdr to all other msdu's which are received in
	 * single mpdu from first msdu.
	 */
	if (qdf_nbuf_is_rx_chfrag_start(msdu)) {
		pwh = HAL_RX_DESC_GET_80211_HDR(desc);
		qdf_mem_copy(first_msdu_hdr, pwh,
			     sizeof(struct ieee80211_frame));
	}

	qdf_mem_copy(localbuf, first_msdu_hdr, new_hdsize);

	/* Flush the cached 802.11 hdr once last msdu in mpdu is received */
	if (qdf_nbuf_is_rx_chfrag_end(msdu))
		qdf_mem_zero(first_msdu_hdr, sizeof(struct ieee80211_frame));

	wh->i_fc[0] |= IEEE80211_FC0_TYPE_DATA;
	wh->i_fc[1] &= ~IEEE80211_FC1_WEP;

	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		qos_cntl =
		(struct ieee80211_qoscntl *)(localbuf + new_hdsize);
		tid = hal_rx_mpdu_start_tid_get(hal_soc_hdl, desc);
		qos_cntl->i_qos[0] = (tid & IEEE80211_QOS_TID);
		wh->i_fc[0] |= QDF_IEEE80211_FC0_SUBTYPE_QOS;

		qos_cntl->i_qos[1] = 0;
		new_hdsize += sizeof(struct ieee80211_qoscntl);
	}

	/*
	 * Prepare llc Header
	 */

	llc_hdr = (struct llc_snap_hdr_t *)(localbuf + new_hdsize);
	ether_type = (eth_hdr->ethertype[0] << 8) |
			(eth_hdr->ethertype[1]);
	if (ether_type >= ETH_P_802_3_MIN) {
		qdf_mem_copy(llc_hdr,
			     ethernet_II_llc_snap_header_prefix,
			     sizeof
			     (ethernet_II_llc_snap_header_prefix));
		if (ether_type == ETHERTYPE_AARP ||
		    ether_type == ETHERTYPE_IPX) {
			llc_hdr->org_code[2] =
				BTEP_SNAP_ORGCODE_2;
			/* 0xf8; bridge tunnel header */
		}
		llc_hdr->ethertype[0] = eth_hdr->ethertype[0];
		llc_hdr->ethertype[1] = eth_hdr->ethertype[1];
		new_hdsize += sizeof(struct llc_snap_hdr_t);
	}

	/*
	 * Remove 802.3 Header by adjusting the head
	 */
	qdf_nbuf_pull_head(msdu, hdsize);

	/*
	 * Adjust the head and prepare 802.11 Header
	 */
	qdf_nbuf_push_head(msdu, new_hdsize);
	qdf_mem_copy(qdf_nbuf_data(msdu), localbuf, new_hdsize);
}
#endif

void pkt_capture_rx_in_order_drop_offload_pkt(qdf_nbuf_t head_msdu)
{
	while (head_msdu) {
		qdf_nbuf_t msdu = head_msdu;

		head_msdu = qdf_nbuf_next(head_msdu);
		qdf_nbuf_free(msdu);
	}
}

bool pkt_capture_rx_in_order_offloaded_pkt(qdf_nbuf_t rx_ind_msg)
{
	uint32_t *msg_word;

	msg_word = (uint32_t *)qdf_nbuf_data(rx_ind_msg);

	/* check if it is for offloaded data pkt */
	return HTT_RX_IN_ORD_PADDR_IND_PKT_CAPTURE_MODE_IS_MONITOR_SET
					(*(msg_word + 1));
}

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
void pkt_capture_msdu_process_pkts(
				uint8_t *bssid,
				qdf_nbuf_t head_msdu, uint8_t vdev_id,
				htt_pdev_handle pdev, uint16_t status)
{
	qdf_nbuf_t loop_msdu, pktcapture_msdu;
	qdf_nbuf_t msdu, prev = NULL;

	pktcapture_msdu = NULL;
	loop_msdu = head_msdu;
	while (loop_msdu) {
		msdu = qdf_nbuf_copy(loop_msdu);

		if (msdu) {
			qdf_nbuf_push_head(msdu,
					   HTT_RX_STD_DESC_RESERVATION);
			qdf_nbuf_set_next(msdu, NULL);

			if (!(pktcapture_msdu)) {
				pktcapture_msdu = msdu;
				prev = msdu;
			} else {
				qdf_nbuf_set_next(prev, msdu);
				prev = msdu;
			}
		}
		loop_msdu = qdf_nbuf_next(loop_msdu);
	}

	if (!pktcapture_msdu)
		return;

	pkt_capture_datapkt_process(
			vdev_id, pktcapture_msdu,
			TXRX_PROCESS_TYPE_DATA_RX, 0, 0,
			TXRX_PKTCAPTURE_PKT_FORMAT_8023,
			bssid, pdev, 0);
}
#else
#define RX_OFFLOAD_PKT 1
void pkt_capture_msdu_process_pkts(
				uint8_t *bssid, qdf_nbuf_t head_msdu,
				uint8_t vdev_id, void *psoc, uint16_t status)
{
	qdf_nbuf_t loop_msdu, pktcapture_msdu, offload_msdu = NULL;
	qdf_nbuf_t msdu, prev = NULL;

	pktcapture_msdu = NULL;
	loop_msdu = head_msdu;
	while (loop_msdu) {
		msdu = qdf_nbuf_copy(loop_msdu);

		if (msdu) {
			qdf_nbuf_set_next(msdu, NULL);

			if (!(pktcapture_msdu)) {
				pktcapture_msdu = msdu;
				prev = msdu;
			} else {
				qdf_nbuf_set_next(prev, msdu);
				prev = msdu;
			}
		}
		if (status == RX_OFFLOAD_PKT)
			offload_msdu = loop_msdu;
		loop_msdu = qdf_nbuf_next(loop_msdu);

		/* Free offload msdu as it is delivered only to pkt capture */
		if (offload_msdu) {
			qdf_nbuf_free(offload_msdu);
			offload_msdu = NULL;
		}
	}

	if (!pktcapture_msdu)
		return;

	pkt_capture_datapkt_process(
			vdev_id, pktcapture_msdu,
			TXRX_PROCESS_TYPE_DATA_RX, 0, 0,
			TXRX_PKTCAPTURE_PKT_FORMAT_8023,
			bssid, psoc, 0);
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_dp_rx_skip_tlvs() - Skip TLVs len + L2 hdr_offset, save in nbuf
 * @nbuf: nbuf to be updated
 * @l3_padding: l3_padding
 *
 * Return: None
 */
static void pkt_capture_dp_rx_skip_tlvs(qdf_nbuf_t nbuf, uint32_t l3_padding)
{
	QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(nbuf) = l3_padding;
	qdf_nbuf_pull_head(nbuf, l3_padding + RX_PKT_TLVS_LEN);
}

/**
 * pkt_capture_rx_get_phy_info() - Get phy info
 * @context: objmgr vdev
 * @psoc: dp_soc handle
 * @rx_tlv_hdr: Pointer to struct rx_pkt_tlvs
 * @rx_status: Pointer to struct mon_rx_status
 *
 * Return: none
 */
static void pkt_capture_rx_get_phy_info(void *context, void *psoc,
					uint8_t *rx_tlv_hdr,
					struct mon_rx_status *rx_status)
{
	uint8_t preamble = 0;
	uint8_t preamble_type;
	uint16_t mcs = 0, nss = 0, sgi = 0, bw = 0;
	uint8_t beamformed = 0;
	bool is_stbc = 0, ldpc = 0;
	struct dp_soc *soc = psoc;
	hal_soc_handle_t hal_soc;
	struct wlan_objmgr_vdev *vdev = context;

	hal_soc = soc->hal_soc;
	preamble_type = hal_rx_msdu_start_get_pkt_type(rx_tlv_hdr);
	nss = hal_rx_msdu_start_nss_get(hal_soc, rx_tlv_hdr); /* NSS */
	bw = hal_rx_msdu_start_bw_get(rx_tlv_hdr);
	mcs = hal_rx_msdu_start_rate_mcs_get(rx_tlv_hdr);
	sgi = hal_rx_msdu_start_sgi_get(rx_tlv_hdr);

	switch (preamble_type) {
	case HAL_RX_PKT_TYPE_11A:
	case HAL_RX_PKT_TYPE_11B:
	/* legacy */
		rx_status->rate = pkt_capture_get_tx_rate(
						preamble_type,
						mcs,
						&preamble);
		rx_status->mcs = mcs;
		break;
	case HAL_RX_PKT_TYPE_11N:
		rx_status->ht_flags = 1;
		if (nss == 2)
			mcs = 8 + mcs;
		rx_status->ht_mcs = mcs;
		break;
	case HAL_RX_PKT_TYPE_11AC:
		rx_status->vht_flags = 1;
		rx_status->vht_flag_values3[0] = mcs << 0x4 | nss;
		bw = vdev->vdev_mlme.des_chan->ch_width;
		rx_status->vht_flag_values2 = bw;
		break;
	case HAL_RX_PKT_TYPE_11AX:
		rx_status->he_flags = 1;
		rx_status->he_data1 |=
			IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN |
			IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN;
		rx_status->he_data2 |= IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN;
		rx_status->he_data3 |= mcs << 0x8;
		rx_status->he_data5 |= (bw | (sgi << 0x4));
		rx_status->he_data6 |= nss;
	default:
		break;
	}

	if (preamble == 0)
		rx_status->ofdm_flag = 1;
	else if (preamble == 1)
		rx_status->cck_flag = 1;

	rx_status->bw = bw;
	rx_status->nr_ant = nss;
	rx_status->nss = nss;
	/* is_stbc not available */
	rx_status->is_stbc = is_stbc;
	rx_status->sgi = sgi;
	/* ldpc not available */
	rx_status->ldpc = ldpc;
	/* beamformed not available */
	rx_status->beamformed = beamformed;
	rx_status->rtap_flags |= ((preamble == SHORT_PREAMBLE) ? BIT(1) : 0);
}

/**
 * pkt_capture_get_rx_rtap_flags() - Get radiotap flags
 * @ptr_rx_tlv_hdr: Pointer to struct rx_pkt_tlvs
 *
 * Return: Bitmapped radiotap flags.
 */
static uint8_t pkt_capture_get_rx_rtap_flags(void *ptr_rx_tlv_hdr)
{
	struct rx_pkt_tlvs *rx_tlv_hdr = (struct rx_pkt_tlvs *)ptr_rx_tlv_hdr;
	struct rx_attention *rx_attn = &rx_tlv_hdr->attn_tlv.rx_attn;
	struct rx_mpdu_start *mpdu_start =
				&rx_tlv_hdr->mpdu_start_tlv.rx_mpdu_start;
	struct rx_mpdu_end *mpdu_end = &rx_tlv_hdr->mpdu_end_tlv.rx_mpdu_end;
	uint8_t rtap_flags = 0;

	/* WEP40 || WEP104 || WEP128 */
	if (mpdu_start->rx_mpdu_info_details.encrypt_type == 0 ||
	    mpdu_start->rx_mpdu_info_details.encrypt_type == 1 ||
	    mpdu_start->rx_mpdu_info_details.encrypt_type == 3)
		rtap_flags |= BIT(2);

	/* IEEE80211_RADIOTAP_F_FRAG */
	if (rx_attn->fragment_flag)
		rtap_flags |= BIT(3);

	/* IEEE80211_RADIOTAP_F_FCS */
	rtap_flags |= BIT(4);

	/* IEEE80211_RADIOTAP_F_BADFCS */
	if (mpdu_end->fcs_err)
		rtap_flags |= BIT(6);

	return rtap_flags;
}

/**
 * pkt_capture_rx_mon_get_rx_status() - Get rx status
 * @context: objmgr vdev
 * @psoc: dp_soc handle
 * @desc: Pointer to struct rx_pkt_tlvs
 * @rx_status: Pointer to struct mon_rx_status
 *
 * Return: none
 */
static void pkt_capture_rx_mon_get_rx_status(void *context, void *dp_soc,
					     void *desc,
					     struct mon_rx_status *rx_status)
{
	uint8_t *rx_tlv_hdr = desc;
	struct rx_pkt_tlvs *pkt_tlvs = (struct rx_pkt_tlvs *)desc;
	struct rx_msdu_start *msdu_start =
					&pkt_tlvs->msdu_start_tlv.rx_msdu_start;
	struct wlan_objmgr_vdev *vdev = context;
	uint8_t primary_chan_num;
	uint32_t center_chan_freq;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	enum reg_wifi_band band;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pkt_capture_err("Failed to get psoc");
		return;
	}

	rx_status->rtap_flags |= pkt_capture_get_rx_rtap_flags(rx_tlv_hdr);
	rx_status->ant_signal_db = hal_rx_msdu_start_get_rssi(rx_tlv_hdr);
	rx_status->rssi_comb = hal_rx_msdu_start_get_rssi(rx_tlv_hdr);
	rx_status->tsft = msdu_start->ppdu_start_timestamp;
	primary_chan_num = hal_rx_msdu_start_get_freq(rx_tlv_hdr);
	center_chan_freq = hal_rx_msdu_start_get_freq(rx_tlv_hdr) >> 16;
	rx_status->chan_num = primary_chan_num;
	band = wlan_reg_freq_to_band(center_chan_freq);

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0, WLAN_PKT_CAPTURE_ID);
	if (!pdev) {
		pkt_capture_err("Failed to get pdev");
		return;
	}

	rx_status->chan_freq =
		wlan_reg_chan_band_to_freq(pdev, primary_chan_num, BIT(band));
	wlan_objmgr_pdev_release_ref(pdev, WLAN_PKT_CAPTURE_ID);

	pkt_capture_rx_get_phy_info(context, dp_soc, desc, rx_status);
}
#endif

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_rx_data_cb(): callback to process data rx packets
 * for pkt capture mode. (normal rx + offloaded rx)
 * @context: objmgr vdev
 * @ppdev: device handler
 * @nbuf_list: netbuf list
 * @vdev_id: vdev id for which packet is captured
 * @tid:  tid number
 * @status: Tx status
 * @pkt_format: Frame format
 * @bssid: bssid
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
static void
pkt_capture_rx_data_cb(
		void *context, void *ppdev, void *nbuf_list,
		uint8_t vdev_id, uint8_t tid,
		uint16_t status, bool pkt_format,
		uint8_t *bssid, uint8_t tx_retry_cnt)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	qdf_nbuf_t buf_list = (qdf_nbuf_t)nbuf_list;
	struct wlan_objmgr_vdev *vdev;
	htt_pdev_handle pdev = ppdev;
	struct pkt_capture_cb_context *cb_ctx;
	qdf_nbuf_t msdu, next_buf;
	uint8_t drop_count;
	struct htt_host_rx_desc_base *rx_desc;
	struct mon_rx_status rx_status = {0};
	uint32_t headroom;
	static uint8_t preamble_type;
	static uint32_t vht_sig_a_1;
	static uint32_t vht_sig_a_2;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_buf;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	cb_ctx = vdev_priv->cb_ctx;
	if (!cb_ctx || !cb_ctx->mon_cb || !cb_ctx->mon_ctx) {
		pkt_capture_vdev_put_ref(vdev);
		goto free_buf;
	}

	msdu = buf_list;
	while (msdu) {
		struct ethernet_hdr_t *eth_hdr;

		next_buf = qdf_nbuf_queue_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);   /* Add NULL terminator */

		rx_desc = htt_rx_desc(msdu);

		/*
		 * Only the first mpdu has valid preamble type, so use it
		 * till the last mpdu is reached
		 */
		if (rx_desc->attention.first_mpdu) {
			preamble_type = rx_desc->ppdu_start.preamble_type;
			if (preamble_type == 8 || preamble_type == 9 ||
			    preamble_type == 0x0c || preamble_type == 0x0d) {
				vht_sig_a_1 = VHT_SIG_A_1(rx_desc);
				vht_sig_a_2 = VHT_SIG_A_2(rx_desc);
			}
		} else {
			rx_desc->ppdu_start.preamble_type = preamble_type;
			if (preamble_type == 8 || preamble_type == 9 ||
			    preamble_type == 0x0c || preamble_type == 0x0d) {
				VHT_SIG_A_1(rx_desc) = vht_sig_a_1;
				VHT_SIG_A_2(rx_desc) = vht_sig_a_2;
			}
		}

		if (rx_desc->attention.last_mpdu) {
			preamble_type = 0;
			vht_sig_a_1 = 0;
			vht_sig_a_2 = 0;
		}

		qdf_nbuf_pull_head(msdu, HTT_RX_STD_DESC_RESERVATION);

		/*
		 * Get the channel info and update the rx status
		 */

		/* need to update this to fill rx_status*/
		htt_rx_mon_get_rx_status(pdev, rx_desc, &rx_status);
		rx_status.chan_noise_floor = NORMALIZED_TO_NOISE_FLOOR;
		rx_status.tx_status = status;
		rx_status.tx_retry_cnt = tx_retry_cnt;
		rx_status.add_rtap_ext = true;

		/* clear IEEE80211_RADIOTAP_F_FCS flag*/
		rx_status.rtap_flags &= ~(BIT(4));
		rx_status.rtap_flags &= ~(BIT(2));

		/*
		 * convert 802.3 header format into 802.11 format
		 */
		if (vdev_id == HTT_INVALID_VDEV) {
			eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
			qdf_mem_copy(bssid, eth_hdr->src_addr,
				     QDF_MAC_ADDR_SIZE);
		}

		pkt_capture_rx_convert8023to80211(bssid, msdu, rx_desc);

		/*
		 * Calculate the headroom and adjust head
		 * to prepare radiotap header.
		 */
		headroom = qdf_nbuf_headroom(msdu);
		qdf_nbuf_update_radiotap(&rx_status, msdu, headroom);
		pkt_capture_mon(cb_ctx, msdu, vdev, 0);
		msdu = next_buf;
	}

	pkt_capture_vdev_put_ref(vdev);
	return;

free_buf:
	drop_count = pkt_capture_drop_nbuf_list(buf_list);
}
#else
/**
 * pkt_capture_rx_data_cb(): callback to process data rx packets
 * for pkt capture mode. (normal rx + offloaded rx)
 * @context: objmgr vdev
 * @psoc: dp_soc handle
 * @nbuf_list: netbuf list
 * @vdev_id: vdev id for which packet is captured
 * @tid:  tid number
 * @status: Tx status
 * @pkt_format: Frame format
 * @bssid: bssid
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
static void
pkt_capture_rx_data_cb(
		void *context, void *psoc, void *nbuf_list,
		uint8_t vdev_id, uint8_t tid,
		uint16_t status, bool pkt_format,
		uint8_t *bssid, uint8_t tx_retry_cnt)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	qdf_nbuf_t buf_list = (qdf_nbuf_t)nbuf_list;
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_cb_context *cb_ctx;
	qdf_nbuf_t msdu, next_buf;
	uint8_t drop_count;
	struct mon_rx_status rx_status = {0};
	uint32_t headroom;
	uint8_t *rx_tlv_hdr = NULL;
	struct dp_soc *soc = psoc;
	hal_soc_handle_t hal_soc;
	struct hal_rx_msdu_metadata msdu_metadata;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	ret = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(ret))
		goto free_buf;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	cb_ctx = vdev_priv->cb_ctx;
	if (!cb_ctx || !cb_ctx->mon_cb || !cb_ctx->mon_ctx) {
		pkt_capture_vdev_put_ref(vdev);
		goto free_buf;
	}

	hal_soc = soc->hal_soc;
	msdu = buf_list;
	while (msdu) {
		struct ethernet_hdr_t *eth_hdr;

		/* push the tlvs to get rx_tlv_hdr pointer */
		qdf_nbuf_push_head(msdu, RX_PKT_TLVS_LEN +
				QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(msdu));

		rx_tlv_hdr = qdf_nbuf_data(msdu);
		hal_rx_msdu_metadata_get(hal_soc, rx_tlv_hdr, &msdu_metadata);
		/* Pull rx_tlv_hdr */
		pkt_capture_dp_rx_skip_tlvs(msdu, msdu_metadata.l3_hdr_pad);

		next_buf = qdf_nbuf_queue_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);   /* Add NULL terminator */

		/*
		 * Get the channel info and update the rx status
		 */

		/* need to update this to fill rx_status*/
		pkt_capture_rx_mon_get_rx_status(vdev, psoc,
						 rx_tlv_hdr, &rx_status);
		rx_status.tx_status = status;
		rx_status.tx_retry_cnt = tx_retry_cnt;
		rx_status.add_rtap_ext = true;

		/* clear IEEE80211_RADIOTAP_F_FCS flag*/
		rx_status.rtap_flags &= ~(BIT(4));
		rx_status.rtap_flags &= ~(BIT(2));

		/*
		 * convert 802.3 header format into 802.11 format
		 */
		if (vdev_id == HTT_INVALID_VDEV) {
			eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
			qdf_mem_copy(bssid, eth_hdr->src_addr,
				     QDF_MAC_ADDR_SIZE);
		}

		pkt_capture_rx_convert8023to80211(hal_soc, msdu, rx_tlv_hdr);

		/*
		 * Calculate the headroom and adjust head
		 * to prepare radiotap header.
		 */
		headroom = qdf_nbuf_headroom(msdu);
		qdf_nbuf_update_radiotap(&rx_status, msdu, headroom);
		pkt_capture_mon(cb_ctx, msdu, vdev, 0);
		msdu = next_buf;
	}

	pkt_capture_vdev_put_ref(vdev);
	return;

free_buf:
	drop_count = pkt_capture_drop_nbuf_list(buf_list);
}

#endif

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_tx_data_cb() - process data tx and rx packets
 * for pkt capture mode. (normal tx/rx + offloaded tx/rx)
 * @vdev_id: vdev id for which packet is captured
 * @mon_buf_list: netbuf list
 * @type: data process type
 * @tid:  tid number
 * @status: Tx status
 * @pktformat: Frame format
 * @bssid: bssid
 * @pdev: pdev handle
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
static void
pkt_capture_tx_data_cb(
		void *context, void *ppdev, void *nbuf_list, uint8_t vdev_id,
		uint8_t tid, uint16_t status, bool pkt_format,
		uint8_t *bssid, uint8_t tx_retry_cnt)
{
	qdf_nbuf_t msdu, next_buf;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	htt_pdev_handle pdev = ppdev;
	struct pkt_capture_cb_context *cb_ctx;
	uint8_t drop_count;
	struct htt_tx_data_hdr_information *cmpl_desc = NULL;
	struct pkt_capture_tx_hdr_elem_t pktcapture_hdr = {0};
	struct ethernet_hdr_t *eth_hdr;
	struct llc_snap_hdr_t *llc_hdr;
	struct ieee80211_frame *wh;
	uint8_t hdsize, new_hdsize;
	struct ieee80211_qoscntl *qos_cntl;
	uint16_t ether_type;
	uint32_t headroom;
	uint16_t seq_no, fc_ctrl;
	struct mon_rx_status tx_status = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t localbuf[sizeof(struct ieee80211_qosframe_htc_addr4) +
			sizeof(struct llc_snap_hdr_t)];
	const uint8_t ethernet_II_llc_snap_header_prefix[] = {
					0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_buf;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	cb_ctx = vdev_priv->cb_ctx;
	if (!cb_ctx || !cb_ctx->mon_cb || !cb_ctx->mon_ctx) {
		pkt_capture_vdev_put_ref(vdev);
		goto free_buf;
	}

	msdu = nbuf_list;
	while (msdu) {
		next_buf = qdf_nbuf_queue_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);   /* Add NULL terminator */

		cmpl_desc = (struct htt_tx_data_hdr_information *)
					(qdf_nbuf_data(msdu));

		pktcapture_hdr.timestamp = cmpl_desc->phy_timestamp_l32;
		pktcapture_hdr.preamble = cmpl_desc->preamble;
		pktcapture_hdr.mcs = cmpl_desc->mcs;
		pktcapture_hdr.bw = cmpl_desc->bw;
		pktcapture_hdr.nss = cmpl_desc->nss;
		pktcapture_hdr.rssi_comb = cmpl_desc->rssi;
		pktcapture_hdr.rate = cmpl_desc->rate;
		pktcapture_hdr.stbc = cmpl_desc->stbc;
		pktcapture_hdr.sgi = cmpl_desc->sgi;
		pktcapture_hdr.ldpc = cmpl_desc->ldpc;
		pktcapture_hdr.beamformed = cmpl_desc->beamformed;
		pktcapture_hdr.status = status;
		pktcapture_hdr.tx_retry_cnt = tx_retry_cnt;

		qdf_nbuf_pull_head(
			msdu,
			sizeof(struct htt_tx_data_hdr_information));

		if (pkt_format == TXRX_PKTCAPTURE_PKT_FORMAT_8023) {
			eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
			hdsize = sizeof(struct ethernet_hdr_t);
			wh = (struct ieee80211_frame *)localbuf;

			*(uint16_t *)wh->i_dur = 0;

			new_hdsize = 0;

			if (vdev_id == HTT_INVALID_VDEV)
				qdf_mem_copy(bssid, eth_hdr->dest_addr,
					     QDF_MAC_ADDR_SIZE);

			/* BSSID , SA , DA */
			qdf_mem_copy(wh->i_addr1, bssid,
				     QDF_MAC_ADDR_SIZE);
			qdf_mem_copy(wh->i_addr2, eth_hdr->src_addr,
				     QDF_MAC_ADDR_SIZE);
			qdf_mem_copy(wh->i_addr3, eth_hdr->dest_addr,
				     QDF_MAC_ADDR_SIZE);

			seq_no = cmpl_desc->seqno;
			seq_no = (seq_no << IEEE80211_SEQ_SEQ_SHIFT) &
					IEEE80211_SEQ_SEQ_MASK;
			fc_ctrl = cmpl_desc->framectrl;
			qdf_mem_copy(wh->i_fc, &fc_ctrl, sizeof(fc_ctrl));
			qdf_mem_copy(wh->i_seq, &seq_no, sizeof(seq_no));

			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;

			new_hdsize = sizeof(struct ieee80211_frame);

			if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
				qos_cntl = (struct ieee80211_qoscntl *)
						(localbuf + new_hdsize);
				qos_cntl->i_qos[0] =
					(tid & IEEE80211_QOS_TID);
				qos_cntl->i_qos[1] = 0;
				new_hdsize += sizeof(struct ieee80211_qoscntl);
			}
			/*
			 * Prepare llc Header
			 */
			llc_hdr = (struct llc_snap_hdr_t *)
					(localbuf + new_hdsize);
			ether_type = (eth_hdr->ethertype[0] << 8) |
					(eth_hdr->ethertype[1]);
			if (ether_type >= ETH_P_802_3_MIN) {
				qdf_mem_copy(
					llc_hdr,
					ethernet_II_llc_snap_header_prefix,
					sizeof
					(ethernet_II_llc_snap_header_prefix));
				if (ether_type == ETHERTYPE_AARP ||
				    ether_type == ETHERTYPE_IPX) {
					llc_hdr->org_code[2] =
						BTEP_SNAP_ORGCODE_2;
					/* 0xf8; bridge tunnel header */
				}
				llc_hdr->ethertype[0] = eth_hdr->ethertype[0];
				llc_hdr->ethertype[1] = eth_hdr->ethertype[1];
				new_hdsize += sizeof(struct llc_snap_hdr_t);
			}

			/*
			 * Remove 802.3 Header by adjusting the head
			 */
			qdf_nbuf_pull_head(msdu, hdsize);

			/*
			 * Adjust the head and prepare 802.11 Header
			 */
			qdf_nbuf_push_head(msdu, new_hdsize);
			qdf_mem_copy(qdf_nbuf_data(msdu), localbuf, new_hdsize);
		}

		pkt_capture_update_tx_status(
				pdev,
				&tx_status,
				&pktcapture_hdr);
		/*
		 * Calculate the headroom and adjust head
		 * to prepare radiotap header.
		 */
		headroom = qdf_nbuf_headroom(msdu);
		qdf_nbuf_update_radiotap(&tx_status, msdu, headroom);
		pkt_capture_mon(cb_ctx, msdu, vdev, 0);
		msdu = next_buf;
	}
	pkt_capture_vdev_put_ref(vdev);
	return;

free_buf:
	drop_count = pkt_capture_drop_nbuf_list(nbuf_list);
}
#else
/**
 * pkt_capture_get_vdev_bss_peer_mac_addr() - API to get bss peer mac address
 * @vdev: objmgr vdev
 * @bss_peer_mac_address: bss peer mac address
 *.
 * Helper function to  get bss peer mac address
 *
 * Return: if success pmo vdev ctx else NULL
 */
static QDF_STATUS pkt_capture_get_vdev_bss_peer_mac_addr(
				struct wlan_objmgr_vdev *vdev,
				struct qdf_mac_addr *bss_peer_mac_address)
{
	struct wlan_objmgr_peer *peer;

	if (!vdev) {
		qdf_print("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_PKT_CAPTURE_ID);
	if (!peer) {
		qdf_print("peer is null");
		return QDF_STATUS_E_INVAL;
	}
	wlan_peer_obj_lock(peer);
	qdf_mem_copy(bss_peer_mac_address->bytes, wlan_peer_get_macaddr(peer),
		     QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_PKT_CAPTURE_ID);

	return QDF_STATUS_SUCCESS;
}

static void
pkt_capture_tx_data_cb(
		void *context, void *ppdev, void *nbuf_list, uint8_t vdev_id,
		uint8_t tid, uint16_t status, bool pkt_format,
		uint8_t *bssid, uint8_t tx_retry_cnt)
{
	qdf_nbuf_t msdu, next_buf;
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_cb_context *cb_ctx;
	uint8_t drop_count;
	struct pkt_capture_tx_hdr_elem_t *ptr_pktcapture_hdr = NULL;
	struct pkt_capture_tx_hdr_elem_t pktcapture_hdr = {0};
	uint32_t txcap_hdr_size = sizeof(struct pkt_capture_tx_hdr_elem_t);
	struct ethernet_hdr_t *eth_hdr;
	struct llc_snap_hdr_t *llc_hdr;
	struct ieee80211_frame *wh;
	uint8_t hdsize, new_hdsize;
	struct ieee80211_qoscntl *qos_cntl;
	uint16_t ether_type;
	uint32_t headroom;
	uint16_t seq_no, fc_ctrl;
	struct mon_rx_status tx_status = {0};
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint8_t localbuf[sizeof(struct ieee80211_qosframe_htc_addr4) +
			sizeof(struct llc_snap_hdr_t)];
	const uint8_t ethernet_II_llc_snap_header_prefix[] = {
					0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	struct qdf_mac_addr bss_peer_mac_address;

	vdev = pkt_capture_get_vdev();
	ret = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(ret))
		goto free_buf;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	cb_ctx = vdev_priv->cb_ctx;
	if (!cb_ctx || !cb_ctx->mon_cb || !cb_ctx->mon_ctx) {
		pkt_capture_vdev_put_ref(vdev);
		goto free_buf;
	}

	msdu = nbuf_list;
	while (msdu) {
		next_buf = qdf_nbuf_queue_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);   /* Add NULL terminator */

		ptr_pktcapture_hdr = (struct pkt_capture_tx_hdr_elem_t *)
					(qdf_nbuf_data(msdu));

		qdf_mem_copy(&pktcapture_hdr, ptr_pktcapture_hdr,
			     txcap_hdr_size);
		pktcapture_hdr.status = status;

		qdf_nbuf_pull_head(msdu, txcap_hdr_size);

		if (pkt_format == TXRX_PKTCAPTURE_PKT_FORMAT_8023) {
			eth_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
			hdsize = sizeof(struct ethernet_hdr_t);
			wh = (struct ieee80211_frame *)localbuf;

			*(uint16_t *)wh->i_dur = 0;

			new_hdsize = 0;

			pkt_capture_get_vdev_bss_peer_mac_addr(
							vdev,
							&bss_peer_mac_address);
			qdf_mem_copy(bssid, bss_peer_mac_address.bytes,
				     QDF_MAC_ADDR_SIZE);
			if (vdev_id == HTT_INVALID_VDEV)
				qdf_mem_copy(bssid, eth_hdr->dest_addr,
					     QDF_MAC_ADDR_SIZE);

			/* BSSID , SA , DA */
			qdf_mem_copy(wh->i_addr1, bssid,
				     QDF_MAC_ADDR_SIZE);
			qdf_mem_copy(wh->i_addr2, eth_hdr->src_addr,
				     QDF_MAC_ADDR_SIZE);
			qdf_mem_copy(wh->i_addr3, eth_hdr->dest_addr,
				     QDF_MAC_ADDR_SIZE);

			seq_no = pktcapture_hdr.seqno;
			seq_no = (seq_no << IEEE80211_SEQ_SEQ_SHIFT) &
					IEEE80211_SEQ_SEQ_MASK;
			fc_ctrl = pktcapture_hdr.framectrl;
			qdf_mem_copy(wh->i_fc, &fc_ctrl, sizeof(fc_ctrl));
			qdf_mem_copy(wh->i_seq, &seq_no, sizeof(seq_no));

			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;

			if (tx_retry_cnt)
				wh->i_fc[1] |= IEEE80211_FC1_RETRY;

			new_hdsize = sizeof(struct ieee80211_frame);

			if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
				qos_cntl = (struct ieee80211_qoscntl *)
						(localbuf + new_hdsize);
				qos_cntl->i_qos[0] =
					(tid & IEEE80211_QOS_TID);
				qos_cntl->i_qos[1] = 0;
				new_hdsize += sizeof(struct ieee80211_qoscntl);
			}
			/*
			 * Prepare llc Header
			 */
			llc_hdr = (struct llc_snap_hdr_t *)
					(localbuf + new_hdsize);
			ether_type = (eth_hdr->ethertype[0] << 8) |
					(eth_hdr->ethertype[1]);
			if (ether_type >= ETH_P_802_3_MIN) {
				qdf_mem_copy(
					llc_hdr,
					ethernet_II_llc_snap_header_prefix,
					sizeof
					(ethernet_II_llc_snap_header_prefix));
				if (ether_type == ETHERTYPE_AARP ||
				    ether_type == ETHERTYPE_IPX) {
					llc_hdr->org_code[2] =
						BTEP_SNAP_ORGCODE_2;
					/* 0xf8; bridge tunnel header */
				}
				llc_hdr->ethertype[0] = eth_hdr->ethertype[0];
				llc_hdr->ethertype[1] = eth_hdr->ethertype[1];
				new_hdsize += sizeof(struct llc_snap_hdr_t);
			}

			/*
			 * Remove 802.3 Header by adjusting the head
			 */
			qdf_nbuf_pull_head(msdu, hdsize);

			/*
			 * Adjust the head and prepare 802.11 Header
			 */
			qdf_nbuf_push_head(msdu, new_hdsize);
			qdf_mem_copy(qdf_nbuf_data(msdu), localbuf, new_hdsize);
		}

		pkt_capture_update_tx_status(
				vdev,
				&tx_status,
				&pktcapture_hdr);
		/*
		 * Calculate the headroom and adjust head
		 * to prepare radiotap header.
		 */
		headroom = qdf_nbuf_headroom(msdu);
		qdf_nbuf_update_radiotap(&tx_status, msdu, headroom);
		pkt_capture_mon(cb_ctx, msdu, vdev, 0);
		msdu = next_buf;
	}
	pkt_capture_vdev_put_ref(vdev);
	return;

free_buf:
	drop_count = pkt_capture_drop_nbuf_list(nbuf_list);
}
#endif

void pkt_capture_datapkt_process(
		uint8_t vdev_id,
		qdf_nbuf_t mon_buf_list,
		enum pkt_capture_data_process_type type,
		uint8_t tid, uint8_t status, bool pkt_format,
		uint8_t *bssid, void *pdev,
		uint8_t tx_retry_cnt)
{
	uint8_t drop_count;
	struct pkt_capture_mon_pkt *pkt;
	pkt_capture_mon_thread_cb callback = NULL;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	status = pkt_capture_txrx_status_map(status);
	vdev = pkt_capture_get_vdev();
	ret = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(ret))
		goto drop_rx_buf;

	pkt = pkt_capture_alloc_mon_pkt(vdev);
	if (!pkt)
		goto drop_rx_buf;

	switch (type) {
	case TXRX_PROCESS_TYPE_DATA_RX:
		callback = pkt_capture_rx_data_cb;
		break;
	case TXRX_PROCESS_TYPE_DATA_TX:
	case TXRX_PROCESS_TYPE_DATA_TX_COMPL:
		callback = pkt_capture_tx_data_cb;
		break;
	default:
		return;
	}

	pkt->callback = callback;
	pkt->context = NULL;
	pkt->pdev = (void *)pdev;
	pkt->monpkt = (void *)mon_buf_list;
	pkt->vdev_id = vdev_id;
	pkt->tid = tid;
	pkt->status = status;
	pkt->pkt_format = pkt_format;
	qdf_mem_copy(pkt->bssid, bssid, QDF_MAC_ADDR_SIZE);
	pkt->tx_retry_cnt = tx_retry_cnt;
	pkt_capture_indicate_monpkt(vdev, pkt);
	pkt_capture_vdev_put_ref(vdev);
	return;

drop_rx_buf:
	drop_count = pkt_capture_drop_nbuf_list(mon_buf_list);
}

struct htt_tx_data_hdr_information *pkt_capture_tx_get_txcomplete_data_hdr(
						uint32_t *msg_word,
						int num_msdus)
{
	int offset_dwords;
	u_int32_t has_tx_tsf;
	u_int32_t has_retry;
	u_int32_t has_ack_rssi;
	u_int32_t has_tx_tsf64;
	u_int32_t has_tx_compl_payload;
	struct htt_tx_compl_ind_append_retries *retry_list = NULL;
	struct htt_tx_data_hdr_information *txcomplete_data_hrd_list = NULL;

	has_tx_compl_payload = HTT_TX_COMPL_IND_APPEND4_GET(*msg_word);
	if (num_msdus <= 0 || !has_tx_compl_payload)
		return NULL;

	offset_dwords = 1 + ((num_msdus + 1) >> 1);

	has_retry = HTT_TX_COMPL_IND_APPEND_GET(*msg_word);
	if (has_retry) {
		int retry_index = 0;
		int width_for_each_retry =
			(sizeof(struct htt_tx_compl_ind_append_retries) +
			3) >> 2;

		retry_list = (struct htt_tx_compl_ind_append_retries *)
			(msg_word + offset_dwords);
		while (retry_list) {
			if (retry_list[retry_index++].flag == 0)
				break;
		}
		offset_dwords += retry_index * width_for_each_retry;
	}
	has_tx_tsf = HTT_TX_COMPL_IND_APPEND1_GET(*msg_word);
	if (has_tx_tsf) {
		int width_for_each_tsf =
			(sizeof(struct htt_tx_compl_ind_append_tx_tstamp)) >> 2;
		offset_dwords += width_for_each_tsf * num_msdus;
	}

	has_ack_rssi = HTT_TX_COMPL_IND_APPEND2_GET(*msg_word);
	if (has_ack_rssi)
		offset_dwords += ((num_msdus + 1) >> 1);

	has_tx_tsf64 = HTT_TX_COMPL_IND_APPEND3_GET(*msg_word);
	if (has_tx_tsf64)
		offset_dwords += (num_msdus << 1);

	txcomplete_data_hrd_list = (struct htt_tx_data_hdr_information *)
					(msg_word + offset_dwords);

	return txcomplete_data_hrd_list;
}

#ifndef WLAN_FEATURE_PKT_CAPTURE_V2
void pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, htt_pdev_handle pdev)
{
	int nbuf_len;
	qdf_nbuf_t netbuf;
	uint8_t status;
	uint8_t tid = 0;
	bool pkt_format;
	u_int32_t *msg_word = (u_int32_t *)msg;
	u_int8_t *buf = (u_int8_t *)msg;
	struct htt_tx_data_hdr_information *txhdr;
	struct htt_tx_offload_deliver_ind_hdr_t *offload_deliver_msg;

	offload_deliver_msg = (struct htt_tx_offload_deliver_ind_hdr_t *)msg;

	txhdr = (struct htt_tx_data_hdr_information *)
		(msg_word + 1);

	nbuf_len = offload_deliver_msg->tx_mpdu_bytes;

	netbuf = qdf_nbuf_alloc(NULL,
				roundup(nbuf_len + RESERVE_BYTES, 4),
				RESERVE_BYTES, 4, false);

	if (!netbuf)
		return;

	qdf_nbuf_put_tail(netbuf, nbuf_len);

	qdf_mem_copy(qdf_nbuf_data(netbuf),
		     buf + sizeof(struct htt_tx_offload_deliver_ind_hdr_t),
		     nbuf_len);

	qdf_nbuf_push_head(
			netbuf,
			sizeof(struct htt_tx_data_hdr_information));

	qdf_mem_copy(qdf_nbuf_data(netbuf), txhdr,
		     sizeof(struct htt_tx_data_hdr_information));

	status = offload_deliver_msg->status;
	pkt_format = offload_deliver_msg->format;
	tid = offload_deliver_msg->tid_num;

	pkt_capture_datapkt_process(
			vdev_id,
			netbuf, TXRX_PROCESS_TYPE_DATA_TX,
			tid, status, pkt_format, bssid, pdev,
			offload_deliver_msg->tx_retry_cnt);
}
#else
void pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, void *soc)
{
	int nbuf_len;
	qdf_nbuf_t netbuf;
	uint8_t status;
	uint8_t tid = 0;
	bool pkt_format;
	u_int8_t *buf = (u_int8_t *)msg;
	struct htt_tx_offload_deliver_ind_hdr_t *offload_deliver_msg;

	struct pkt_capture_tx_hdr_elem_t *ptr_pktcapture_hdr;
	struct pkt_capture_tx_hdr_elem_t pktcapture_hdr = {0};
	uint32_t txcap_hdr_size = sizeof(struct pkt_capture_tx_hdr_elem_t);

	offload_deliver_msg = (struct htt_tx_offload_deliver_ind_hdr_t *)msg;

	pktcapture_hdr.timestamp = offload_deliver_msg->phy_timestamp_l32;
	pktcapture_hdr.preamble = offload_deliver_msg->preamble;
	pktcapture_hdr.mcs = offload_deliver_msg->mcs;
	pktcapture_hdr.bw = offload_deliver_msg->bw;
	pktcapture_hdr.nss = offload_deliver_msg->nss;
	pktcapture_hdr.rssi_comb = offload_deliver_msg->rssi;
	pktcapture_hdr.rate = offload_deliver_msg->rate;
	pktcapture_hdr.stbc = offload_deliver_msg->stbc;
	pktcapture_hdr.sgi = offload_deliver_msg->sgi;
	pktcapture_hdr.ldpc = offload_deliver_msg->ldpc;
	/* Beamformed not available */
	pktcapture_hdr.beamformed = 0;
	pktcapture_hdr.framectrl = offload_deliver_msg->framectrl;
	pktcapture_hdr.tx_retry_cnt = offload_deliver_msg->tx_retry_cnt;
	pktcapture_hdr.seqno = offload_deliver_msg->seqno;
	tid = offload_deliver_msg->tid_num;
	status = offload_deliver_msg->status;
	pkt_format = offload_deliver_msg->format;

	nbuf_len = offload_deliver_msg->tx_mpdu_bytes;

	netbuf = qdf_nbuf_alloc(NULL,
				roundup(nbuf_len + RESERVE_BYTES, 4),
				RESERVE_BYTES, 4, false);

	if (!netbuf)
		return;

	qdf_nbuf_put_tail(netbuf, nbuf_len);

	qdf_mem_copy(qdf_nbuf_data(netbuf),
		     buf + sizeof(struct htt_tx_offload_deliver_ind_hdr_t),
		     nbuf_len);

	qdf_nbuf_push_head(netbuf, txcap_hdr_size);

	ptr_pktcapture_hdr =
	(struct pkt_capture_tx_hdr_elem_t *)qdf_nbuf_data(netbuf);
	qdf_mem_copy(ptr_pktcapture_hdr, &pktcapture_hdr, txcap_hdr_size);

	pkt_capture_datapkt_process(
			vdev_id,
			netbuf, TXRX_PROCESS_TYPE_DATA_TX,
			tid, status, pkt_format, bssid, soc,
			offload_deliver_msg->tx_retry_cnt);
}
#endif
