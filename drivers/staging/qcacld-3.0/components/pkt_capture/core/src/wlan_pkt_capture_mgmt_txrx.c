/*
 * Copyright (c) 2020, 2021 The Linux Foundation. All rights reserved.
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
 * DOC: Implement mgmt txrx APIs which shall be used internally only
 * in pkt_capture component.
 * Note: These APIs should be never accessed out of pkt_capture component.
 */

#include "wlan_pkt_capture_main.h"
#include "wlan_pkt_capture_priv.h"
#include "wlan_pkt_capture_mgmt_txrx.h"
#include "wlan_mlme_main.h"
#include "wlan_lmac_if_api.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "wlan_utility.h"
#include "cds_ieee80211_common.h"

enum pkt_capture_tx_status
pkt_capture_mgmt_status_map(uint8_t status)
{
	enum pkt_capture_tx_status tx_status;

	switch (status) {
	case WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK:
		tx_status = pkt_capture_tx_status_ok;
		break;
	case WMI_MGMT_TX_COMP_TYPE_COMPLETE_NO_ACK:
		tx_status = pkt_capture_tx_status_no_ack;
		break;
	default:
		tx_status = pkt_capture_tx_status_discard;
	break;
	}

	return tx_status;
}

/**
 * pkt_capture_mgmtpkt_cb() - callback to process management packets
 * for pkt capture mode
 * @context: vdev handler
 * @nbuf_list: netbuf list
 * @vdev_id: vdev id for which packet is captured
 * @tid:  tid number
 * @ch_freq: channel frequency
 * @pkt_format: Frame format
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
static void
pkt_capture_mgmtpkt_cb(void *context, void *ppdev, void *nbuf_list,
		       uint8_t vdev_id, uint8_t tid, uint16_t ch_freq,
		       bool pkt_format, uint8_t *bssid, uint8_t tx_retry_cnt)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_psoc *psoc = context;
	struct pkt_capture_cb_context *cb_ctx;
	struct wlan_objmgr_vdev *vdev;
	qdf_nbuf_t msdu, next_buf;
	uint32_t drop_count;

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		goto free_buf;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		goto release_vdev_ref;
	}

	cb_ctx = vdev_priv->cb_ctx;
	if (!cb_ctx || !cb_ctx->mon_cb || !cb_ctx->mon_ctx) {
		pkt_capture_err("mon cb params are NULL");
		goto release_vdev_ref;
	}

	msdu = nbuf_list;
	while (msdu) {
		next_buf = qdf_nbuf_queue_next(msdu);
		qdf_nbuf_set_next(msdu, NULL);   /* Add NULL terminator */
		pkt_capture_mon(cb_ctx, msdu, vdev, ch_freq);
		msdu = next_buf;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
	return;

release_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
free_buf:
	drop_count = pkt_capture_drop_nbuf_list(nbuf_list);
	pkt_capture_debug("Dropped frames %u", drop_count);
}

/**
 * pkt_capture_mgmtpkt_process() - process management packets
 * for pkt capture mode
 * @psoc: pointer to psoc object
 * @txrx_status: mon_rx_status to update radiotap header
 * @nbuf: netbuf
 * @status: Tx status
 *
 * Return: QDF_STATUS Enumeration
 */
static QDF_STATUS
pkt_capture_mgmtpkt_process(struct wlan_objmgr_psoc *psoc,
			    struct mon_rx_status *txrx_status,
			    qdf_nbuf_t nbuf, uint8_t status)
{
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_mon_pkt *pkt;
	uint32_t headroom;

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc,
							QDF_STA_MODE,
							WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Calculate the headroom and adjust head to prepare radiotap header
	 */
	headroom = qdf_nbuf_headroom(nbuf);
	qdf_nbuf_update_radiotap(txrx_status, nbuf, headroom);

	pkt = pkt_capture_alloc_mon_pkt(vdev);
	if (!pkt) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
		return QDF_STATUS_E_FAILURE;
	}

	pkt->callback = pkt_capture_mgmtpkt_cb;
	pkt->context = psoc;
	pkt->monpkt = nbuf;
	pkt->vdev_id = WLAN_INVALID_VDEV_ID;
	pkt->tid = WLAN_INVALID_TID;
	pkt->status = txrx_status->chan_freq;
	pkt->pkt_format = PKTCAPTURE_PKT_FORMAT_80211;
	pkt_capture_indicate_monpkt(vdev, pkt);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
	return QDF_STATUS_SUCCESS;
}

/**
 * pkt_capture_is_rmf_enabled - API to check if rmf is enabled or not
 * @pdev: pointer to pdev object
 * @psoc: pointer to psoc object
 * @addr: mac address
 */
static bool
pkt_capture_is_rmf_enabled(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_psoc *psoc,
			   uint8_t *addr)
{
	struct pkt_psoc_priv *psoc_priv;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;
	int rmf_enabled;

	psoc_priv = pkt_capture_psoc_get_priv(psoc);
	if (!psoc_priv) {
		pkt_capture_err("psoc priv is NULL");
		return false;
	}

	vdev = wlan_objmgr_get_vdev_by_macaddr_from_pdev(pdev,
							 addr,
							 WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return false;
	}

	vdev_id = wlan_vdev_get_id(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);

	rmf_enabled = psoc_priv->cb_obj.get_rmf_status(vdev_id);
	if (rmf_enabled < 0) {
		pkt_capture_err("unable to get rmf status");
		return false;
	}

	return true;
}

/**
 * pkt_capture_process_rmf_frame - process rmf frame
 * @pdev: pointer to pdev object
 * @psoc: pointer to psoc object
 * @nbuf: netbuf
 */
static QDF_STATUS
pkt_capture_process_rmf_frame(struct wlan_objmgr_pdev *pdev,
			      struct wlan_objmgr_psoc *psoc,
			      qdf_nbuf_t nbuf)
{
	tpSirMacFrameCtl pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(nbuf));
	uint8_t mic_len, hdr_len, pdev_id;
	struct ieee80211_frame *wh;
	uint8_t *orig_hdr;

	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);

	if (!QDF_IS_ADDR_BROADCAST(wh->i_addr1) &&
	    !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		if (pfc->wep) {
			QDF_STATUS status;

			orig_hdr = (uint8_t *)qdf_nbuf_data(nbuf);
			pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
			status = mlme_get_peer_mic_len(psoc, pdev_id,
						       wh->i_addr1,
						       &mic_len,
						       &hdr_len);
			if (QDF_IS_STATUS_ERROR(status)) {
				pkt_capture_err("Failed to get mic hdr");
				return QDF_STATUS_E_FAILURE;
			}

			/* Strip privacy headers (and trailer)
			 * for a received frame
			 */
			qdf_mem_move(orig_hdr + hdr_len, wh, sizeof(*wh));
			qdf_nbuf_pull_head(nbuf, hdr_len);
			qdf_nbuf_trim_tail(nbuf, mic_len);
		}
	} else {
		qdf_nbuf_trim_tail(nbuf, IEEE80211_MMIE_LEN);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
pkt_capture_process_mgmt_tx_data(struct wlan_objmgr_pdev *pdev,
				 struct mgmt_offload_event_params *params,
				 qdf_nbuf_t nbuf,
				 uint8_t status)
{
	struct mon_rx_status txrx_status = {0};
	struct wlan_objmgr_psoc *psoc;
	tpSirMacFrameCtl pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(nbuf));
	struct ieee80211_frame *wh;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);

	if ((pfc->type == IEEE80211_FC0_TYPE_MGT) &&
	    (pfc->subType == SIR_MAC_MGMT_DISASSOC ||
	     pfc->subType == SIR_MAC_MGMT_DEAUTH ||
	     pfc->subType == SIR_MAC_MGMT_ACTION)) {
		if (pkt_capture_is_rmf_enabled(pdev, psoc, wh->i_addr2)) {
			QDF_STATUS status;

			status = pkt_capture_process_rmf_frame(pdev, psoc,
							       nbuf);
			if (QDF_IS_STATUS_ERROR(status))
				return status;
		}
	}

	txrx_status.tsft = (u_int64_t)params->tsf_l32;
	txrx_status.chan_num = wlan_freq_to_chan(params->chan_freq);
	txrx_status.chan_freq = params->chan_freq;
	/* params->rate is in Kbps, convert into Mbps */
	txrx_status.rate = (params->rate_kbps / 1000);
	if (params->rssi == INVALID_RSSI_FOR_TX)
		/* RSSI -128 is invalid rssi for TX, make it 0 here,
		 * will be normalized during radiotap updation
		 */
		txrx_status.ant_signal_db = 0;
	else
		txrx_status.ant_signal_db = params->rssi;

	txrx_status.rssi_comb = txrx_status.ant_signal_db;
	txrx_status.nr_ant = 1;
	txrx_status.rtap_flags |=
		((txrx_status.rate == 6 /* Mbps */) ? BIT(1) : 0);

	if (txrx_status.rate == 6)
		txrx_status.ofdm_flag = 1;
	else
		txrx_status.cck_flag = 1;

	txrx_status.rate = ((txrx_status.rate == 6 /* Mbps */) ? 0x0c : 0x02);
	txrx_status.tx_status = status;
	txrx_status.tx_retry_cnt = params->tx_retry_cnt;
	txrx_status.add_rtap_ext = true;

	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);
	wh->i_fc[1] &= ~IEEE80211_FC1_WEP;

	return pkt_capture_mgmtpkt_process(psoc, &txrx_status,
					   nbuf, status);
}

void pkt_capture_mgmt_tx(struct wlan_objmgr_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint16_t chan_freq,
			 uint8_t preamble_type)
{
	struct mgmt_offload_event_params params = {0};
	tpSirMacFrameCtl pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(nbuf));
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	qdf_nbuf_t wbuf;
	int nbuf_len;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pdev) {
		pkt_capture_err("pdev is NULL");
		return;
	}

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("failed to get vdev ref");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		pkt_capture_vdev_put_ref(vdev);
		return;
	}

	if (pfc->type == IEEE80211_FC0_TYPE_MGT &&
	    !(vdev_priv->frame_filter.mgmt_tx_frame_filter &
	    PKT_CAPTURE_MGMT_FRAME_TYPE_ALL))
		goto exit;

	if (pfc->type == IEEE80211_FC0_TYPE_CTL &&
	    !vdev_priv->frame_filter.ctrl_tx_frame_filter)
		goto exit;

	nbuf_len = qdf_nbuf_len(nbuf);
	wbuf = qdf_nbuf_alloc(NULL, roundup(nbuf_len + RESERVE_BYTES, 4),
			      RESERVE_BYTES, 4, false);
	if (!wbuf) {
		pkt_capture_err("Failed to allocate wbuf for mgmt len(%u)",
				nbuf_len);
		goto exit;
	}

	qdf_nbuf_put_tail(wbuf, nbuf_len);
	qdf_mem_copy(qdf_nbuf_data(wbuf), qdf_nbuf_data(nbuf), nbuf_len);

	params.chan_freq = chan_freq;
	/*
	 * Filling Tpc in rssi field.
	 * As Tpc is not available, filling with default value of tpc
	 */
	params.rssi = 0;
	/* Assigning the local timestamp as TSF timestamp is not available*/
	params.tsf_l32 = (uint32_t)jiffies;

	if (preamble_type == (1 << WMI_RATE_PREAMBLE_CCK))
		params.rate_kbps = 1000; /* Rate is 1 Mbps for CCK */
	else
		params.rate_kbps = 6000; /* Rate is 6 Mbps for OFDM */

	/*
	 * The mgmt tx packet is send to mon interface before tx completion.
	 * we do not have status for this packet, using magic number(0xFF)
	 * as status for mgmt tx packet
	 */
	if (QDF_STATUS_SUCCESS !=
		pkt_capture_process_mgmt_tx_data(pdev, &params, wbuf, 0xFF))
		qdf_nbuf_free(wbuf);
exit:
	pkt_capture_vdev_put_ref(vdev);
}

void
pkt_capture_mgmt_tx_completion(struct wlan_objmgr_pdev *pdev,
			       uint32_t desc_id,
			       uint32_t status,
			       struct mgmt_offload_event_params *params)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	tpSirMacFrameCtl pfc;
	qdf_nbuf_t wbuf, nbuf;
	int nbuf_len;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (!pdev) {
		pkt_capture_err("pdev is NULL");
		return;
	}

	vdev = pkt_capture_get_vdev();
	ret = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pkt_capture_err("failed to get vdev ref");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		pkt_capture_vdev_put_ref(vdev);
		return;
	}

	nbuf = mgmt_txrx_get_nbuf(pdev, desc_id);
	if (!nbuf)
		goto exit;

	pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(nbuf));
	if (pfc->type == IEEE80211_FC0_TYPE_MGT &&
	    !(vdev_priv->frame_filter.mgmt_tx_frame_filter &
	    PKT_CAPTURE_MGMT_FRAME_TYPE_ALL))
		goto exit;

	if (pfc->type == IEEE80211_FC0_TYPE_CTL &&
	    !vdev_priv->frame_filter.ctrl_tx_frame_filter)
		goto exit;

	nbuf_len = qdf_nbuf_len(nbuf);
	wbuf = qdf_nbuf_alloc(NULL, roundup(nbuf_len + RESERVE_BYTES, 4),
			      RESERVE_BYTES, 4, false);
	if (!wbuf) {
		pkt_capture_err("Failed to allocate wbuf for mgmt len(%u)",
				nbuf_len);
		goto exit;
	}

	qdf_nbuf_put_tail(wbuf, nbuf_len);
	qdf_mem_copy(qdf_nbuf_data(wbuf), qdf_nbuf_data(nbuf), nbuf_len);

	if (QDF_STATUS_SUCCESS !=
		pkt_capture_process_mgmt_tx_data(
					pdev, params, wbuf,
					pkt_capture_mgmt_status_map(status)))
		qdf_nbuf_free(wbuf);

exit:
	pkt_capture_vdev_put_ref(vdev);
}

/**
 * pkt_capture_is_beacon_forward_enable() - API to check whether particular
 * beacon needs to be forwarded on mon interface based on vendor command
 * @vdev: vdev object
 * @wbuf: netbuf
 *
 * Return: bool
 */
static bool
pkt_capture_is_beacon_forward_enable(struct wlan_objmgr_vdev *vdev,
				     qdf_nbuf_t wbuf)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct qdf_mac_addr connected_bssid = {0};
	tpSirMacMgmtHdr mac_hdr;
	bool my_beacon = false;

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return false;
	}

	if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
	    PKT_CAPTURE_MGMT_CONNECT_NO_BEACON)
		return false;

	mac_hdr = (tpSirMacMgmtHdr)(qdf_nbuf_data(wbuf));
	wlan_vdev_get_bss_peer_mac(vdev, &connected_bssid);

	if (qdf_is_macaddr_equal((struct qdf_mac_addr *)mac_hdr->bssId,
				 &connected_bssid))
		my_beacon = true;

	if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
	    PKT_CAPTURE_MGMT_CONNECT_BEACON && !my_beacon)
		return false;

	if (vdev_priv->frame_filter.mgmt_rx_frame_filter &
	    PKT_CAPTURE_MGMT_CONNECT_SCAN_BEACON && my_beacon)
		return false;

	return true;
}

#ifdef DP_MON_RSSI_IN_DBM
#define PKT_CAPTURE_FILL_RSSI(rx_params) \
((rx_params)->snr + NORMALIZED_TO_NOISE_FLOOR)
#else
#define PKT_CAPTURE_FILL_RSSI(rx_status) \
((rx_params)->snr)
#endif

/**
 * process_pktcapture_mgmt_rx_data_cb() -  process management rx packets
 * @rx_params: mgmt rx event params
 * @wbuf: netbuf
 *
 * Return: none
 */
static QDF_STATUS
pkt_capture_mgmt_rx_data_cb(struct wlan_objmgr_psoc *psoc,
			    struct wlan_objmgr_peer *peer,
			    qdf_nbuf_t wbuf,
			    struct mgmt_rx_event_params *rx_params,
			    enum mgmt_frame_type frm_type)
{
	struct mon_rx_status txrx_status = {0};
	struct pkt_capture_vdev_priv *vdev_priv;
	struct ieee80211_frame *wh;
	tpSirMacFrameCtl pfc;
	qdf_nbuf_t nbuf;
	int buf_len;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = pkt_capture_get_vdev();
	status = pkt_capture_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("failed to get vdev ref");
		qdf_nbuf_free(wbuf);
		return QDF_STATUS_E_FAILURE;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		pkt_capture_vdev_put_ref(vdev);
		qdf_nbuf_free(wbuf);
		return QDF_STATUS_E_FAILURE;
	}

	pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(wbuf));

	if (pfc->type == SIR_MAC_CTRL_FRAME  &&
	    !vdev_priv->frame_filter.ctrl_rx_frame_filter)
		goto exit;

	if (pfc->type == SIR_MAC_MGMT_FRAME  &&
	    !vdev_priv->frame_filter.mgmt_rx_frame_filter)
		goto exit;

	if (pfc->type == SIR_MAC_MGMT_FRAME) {
		if (pfc->subType == SIR_MAC_MGMT_BEACON) {
			if (!pkt_capture_is_beacon_forward_enable(vdev, wbuf))
				goto exit;
		} else {
			if (!((vdev_priv->frame_filter.mgmt_rx_frame_filter &
			    PKT_CAPTURE_MGMT_FRAME_TYPE_ALL) ||
			    (vdev_priv->frame_filter.mgmt_rx_frame_filter &
			    PKT_CAPTURE_MGMT_CONNECT_NO_BEACON)))
				goto exit;
		}
	}

	buf_len = qdf_nbuf_len(wbuf);
	nbuf = qdf_nbuf_alloc(NULL, roundup(
				  buf_len + RESERVE_BYTES, 4),
				  RESERVE_BYTES, 4, false);
	if (!nbuf) {
		qdf_nbuf_free(wbuf);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_nbuf_put_tail(nbuf, buf_len);
	qdf_mem_copy(qdf_nbuf_data(nbuf), qdf_nbuf_data(wbuf), buf_len);

	qdf_nbuf_free(wbuf);

	pfc = (tpSirMacFrameCtl)(qdf_nbuf_data(nbuf));
	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);

	pdev = wlan_vdev_get_pdev(vdev);
	pkt_capture_vdev_put_ref(vdev);

	if ((pfc->type == IEEE80211_FC0_TYPE_MGT) &&
	    (pfc->subType == SIR_MAC_MGMT_DISASSOC ||
	     pfc->subType == SIR_MAC_MGMT_DEAUTH ||
	     pfc->subType == SIR_MAC_MGMT_ACTION)) {
		if (pkt_capture_is_rmf_enabled(pdev, psoc, wh->i_addr1)) {
			QDF_STATUS status;

			status = pkt_capture_process_rmf_frame(pdev, psoc,
							       nbuf);
			if (QDF_IS_STATUS_ERROR(status))
				return status;
		}
	}

	txrx_status.tsft = (u_int64_t)rx_params->tsf_l32;
	txrx_status.chan_num = rx_params->channel;
	txrx_status.chan_freq = rx_params->chan_freq;
	/* rx_params->rate is in Kbps, convert into Mbps */
	txrx_status.rate = (rx_params->rate / 1000);
	txrx_status.ant_signal_db = rx_params->snr;
	txrx_status.chan_noise_floor = NORMALIZED_TO_NOISE_FLOOR;
	txrx_status.rssi_comb = PKT_CAPTURE_FILL_RSSI(rx_params);
	txrx_status.nr_ant = 1;
	txrx_status.rtap_flags |=
		((txrx_status.rate == 6 /* Mbps */) ? BIT(1) : 0);

	if (rx_params->phy_mode != PKTCAPTURE_RATECODE_CCK)
		txrx_status.ofdm_flag = 1;
	else
		txrx_status.cck_flag = 1;

	/* Convert rate from Mbps to 500 Kbps */
	txrx_status.rate = txrx_status.rate * 2;
	txrx_status.add_rtap_ext = true;

	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);
	wh->i_fc[1] &= ~IEEE80211_FC1_WEP;

	if (QDF_STATUS_SUCCESS !=
		pkt_capture_mgmtpkt_process(psoc, &txrx_status, nbuf, 0))
		qdf_nbuf_free(nbuf);

	return QDF_STATUS_SUCCESS;
exit:
	pkt_capture_vdev_put_ref(vdev);
	qdf_nbuf_free(wbuf);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pkt_capture_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
				   bool is_register)
{
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info[2];
	QDF_STATUS status;
	int num_of_entries;

	frm_cb_info[0].frm_type = MGMT_FRAME_TYPE_ALL;
	frm_cb_info[0].mgmt_rx_cb = pkt_capture_mgmt_rx_data_cb;
	frm_cb_info[1].frm_type = MGMT_CTRL_FRAME;
	frm_cb_info[1].mgmt_rx_cb = pkt_capture_mgmt_rx_data_cb;
	num_of_entries = 2;

	if (is_register)
		status = wlan_mgmt_txrx_register_rx_cb(
					psoc, WLAN_UMAC_COMP_PKT_CAPTURE,
					frm_cb_info, num_of_entries);
	else
		status = wlan_mgmt_txrx_deregister_rx_cb(
					psoc, WLAN_UMAC_COMP_PKT_CAPTURE,
					frm_cb_info, num_of_entries);

	return status;
}
