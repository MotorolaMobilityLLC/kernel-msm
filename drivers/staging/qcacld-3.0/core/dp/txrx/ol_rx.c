/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

#include <qdf_nbuf.h>               /* qdf_nbuf_t, etc. */
#include <qdf_util.h>               /* qdf_cpu_to_le64 */
#include <qdf_types.h>              /* bool */
#include <cds_ieee80211_common.h>   /* ieee80211_frame */

/* external API header files */
#include <ol_ctrl_txrx_api.h>   /* ol_rx_notify */
#include <ol_txrx_api.h>        /* ol_txrx_pdev_handle */
#include <ol_txrx_htt_api.h>    /* ol_rx_indication_handler */
#include <ol_htt_rx_api.h>      /* htt_rx_peer_id, etc. */

/* internal API header files */
#include <ol_txrx_peer_find.h>  /* ol_txrx_peer_find_by_id */
#include <ol_rx_reorder.h>      /* ol_rx_reorder_store, etc. */
#include <ol_rx_reorder_timeout.h>      /* OL_RX_REORDER_TIMEOUT_UPDATE */
#include <ol_rx_defrag.h>       /* ol_rx_defrag_waitlist_flush */
#include <ol_txrx_internal.h>
#include <ol_txrx.h>
#include <wdi_event.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>      /* ol_rx_decap_info_t, etc */
#endif
#include <ol_rx.h>

/* FIX THIS: txrx should not include private header files of other modules */
#include <htt_types.h>
#include <ol_if_athvar.h>
#include <enet.h>               /* ethernet + SNAP/LLC header defs and
				 * ethertype values
				 */
#include <ip_prot.h>            /* IP protocol values */
#include <ipv4.h>               /* IPv4 header defs */
#include <ipv6_defs.h>          /* IPv6 header defs */
#include <ol_vowext_dbg_defs.h>
#include <wma.h>
#include <wlan_policy_mgr_api.h>
#include "pktlog_ac_fmt.h"
#include <cdp_txrx_handle.h>
#include <pld_common.h>
#include <htt_internal.h>
#include <wlan_pkt_capture_ucfg_api.h>
#include <wlan_cfr_ucfg_api.h>

#ifndef OL_RX_INDICATION_MAX_RECORDS
#define OL_RX_INDICATION_MAX_RECORDS 2048
#endif

/**
 * enum ol_rx_ind_record_type - OL rx indication events
 * @OL_RX_INDICATION_POP_START: event recorded before netbuf pop
 * @OL_RX_INDICATION_POP_END: event recorded after netbuf pop
 * @OL_RX_INDICATION_BUF_REPLENISH: event recorded after buffer replenishment
 */
enum ol_rx_ind_record_type {
	OL_RX_INDICATION_POP_START,
	OL_RX_INDICATION_POP_END,
	OL_RX_INDICATION_BUF_REPLENISH,
};

/**
 * struct ol_rx_ind_record - structure for detailing ol txrx rx ind. event
 * @value: info corresponding to rx indication event
 * @type: what the event was
 * @time: when it happened
 */
struct ol_rx_ind_record {
	uint16_t value;
	enum ol_rx_ind_record_type type;
	uint64_t time;
};

#ifdef OL_RX_INDICATION_RECORD
static uint32_t ol_rx_ind_record_index;
struct ol_rx_ind_record
	      ol_rx_indication_record_history[OL_RX_INDICATION_MAX_RECORDS];

/**
 * ol_rx_ind_record_event() - record ol rx indication events
 * @value: contains rx ind. event related info
 * @type: ol rx indication message type
 *
 * This API record the ol rx indiation event in a rx indication
 * record buffer.
 *
 * Return: None
 */
static void ol_rx_ind_record_event(uint32_t value,
				    enum ol_rx_ind_record_type type)
{
	ol_rx_indication_record_history[ol_rx_ind_record_index].value = value;
	ol_rx_indication_record_history[ol_rx_ind_record_index].type = type;
	ol_rx_indication_record_history[ol_rx_ind_record_index].time =
							qdf_get_log_timestamp();

	ol_rx_ind_record_index++;
	if (ol_rx_ind_record_index >= OL_RX_INDICATION_MAX_RECORDS)
		ol_rx_ind_record_index = 0;
}
#else
static inline
void ol_rx_ind_record_event(uint32_t value, enum ol_rx_ind_record_type type)
{
}

#endif /* OL_RX_INDICATION_RECORD */

void ol_rx_data_process(struct ol_txrx_peer_t *peer,
			qdf_nbuf_t rx_buf_list);

#ifdef WDI_EVENT_ENABLE
/**
 * ol_rx_send_pktlog_event() - send rx packetlog event
 * @pdev: pdev handle
 * @peer: peer handle
 * @msdu: skb list
 * @pktlog_bit: packetlog bit from firmware
 *
 * Return: none
 */
#ifdef HELIUMPLUS
void ol_rx_send_pktlog_event(struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer, qdf_nbuf_t msdu, uint8_t pktlog_bit)
{
	struct ol_rx_remote_data data;

	/**
	 * pktlog is meant to log rx_desc information which is
	 * already overwritten by radio header when monitor mode is ON.
	 * Therefore, Do not log pktlog event when monitor mode is ON.
	 */
	if (!pktlog_bit || (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE))
		return;

	data.msdu = msdu;
	if (peer)
		data.mac_id = peer->vdev->mac_id;
	else
		data.mac_id = 0;

	wdi_event_handler(WDI_EVENT_RX_DESC_REMOTE, pdev->id,
			  &data);
}
#else
void ol_rx_send_pktlog_event(struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer, qdf_nbuf_t msdu, uint8_t pktlog_bit)
{
	struct ol_rx_remote_data data;

	/**
	 * pktlog is meant to log rx_desc information which is
	 * already overwritten by radio header when monitor mode is ON.
	 * Therefore, Do not log pktlog event when monitor mode is ON.
	 */
	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		return;

	data.msdu = msdu;
	if (peer)
		data.mac_id = peer->vdev->mac_id;
	else
		data.mac_id = 0;

	wdi_event_handler(WDI_EVENT_RX_DESC_REMOTE, pdev->id,
			  &data);
}
#endif
#endif /* WDI_EVENT_ENABLE */

#ifdef HTT_RX_RESTORE

static void ol_rx_restore_handler(struct work_struct *htt_rx)
{
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
		  "Enter: %s", __func__);
	pld_device_self_recovery(qdf_ctx->dev);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
		  "Exit: %s", __func__);
}

static DECLARE_WORK(ol_rx_restore_work, ol_rx_restore_handler);

void ol_rx_trigger_restore(htt_pdev_handle htt_pdev, qdf_nbuf_t head_msdu,
			   qdf_nbuf_t tail_msdu)
{
	qdf_nbuf_t next;

	while (head_msdu) {
		next = qdf_nbuf_next(head_msdu);
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
			  "freeing %pK\n", head_msdu);
		qdf_nbuf_free(head_msdu);
		head_msdu = next;
	}

	if (!htt_pdev->rx_ring.htt_rx_restore) {
		cds_set_recovery_in_progress(true);
		htt_pdev->rx_ring.htt_rx_restore = 1;
		schedule_work(&ol_rx_restore_work);
	}
}
#endif

/**
 * ol_rx_update_histogram_stats() - update rx histogram statistics
 * @msdu_count: msdu count
 * @frag_ind: fragment indication set
 * @offload_ind: offload indication set
 *
 * Return: none
 */
void ol_rx_update_histogram_stats(uint32_t msdu_count, uint8_t frag_ind,
		 uint8_t offload_ind)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	if (msdu_count > 60) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_61_plus, 1);
	} else if (msdu_count > 50) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_51_60, 1);
	} else if (msdu_count > 40) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_41_50, 1);
	} else if (msdu_count > 30) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_31_40, 1);
	} else if (msdu_count > 20) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_21_30, 1);
	} else if (msdu_count > 10) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_11_20, 1);
	} else if (msdu_count > 1) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_2_10, 1);
	} else if (msdu_count == 1) {
		TXRX_STATS_ADD(pdev, pub.rx.rx_ind_histogram.pkts_1, 1);
	}

	if (frag_ind)
		TXRX_STATS_ADD(pdev, pub.rx.msdus_with_frag_ind, msdu_count);

	if (offload_ind)
		TXRX_STATS_ADD(pdev, pub.rx.msdus_with_offload_ind, msdu_count);

}

#ifdef WLAN_PARTIAL_REORDER_OFFLOAD

#ifdef WDI_EVENT_ENABLE
static void ol_rx_process_inv_peer(ol_txrx_pdev_handle pdev,
				   void *rx_mpdu_desc, qdf_nbuf_t msdu)
{
	uint8_t a1[QDF_MAC_ADDR_SIZE];
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	struct ol_txrx_vdev_t *vdev = NULL;
	struct ieee80211_frame *wh;
	struct wdi_event_rx_peer_invalid_msg msg;

	wh = (struct ieee80211_frame *)
	     htt_rx_mpdu_wifi_hdr_retrieve(htt_pdev, rx_mpdu_desc);
	/*
	 * Klocwork issue #6152
	 *  All targets that send a "INVALID_PEER" rx status provide a
	 *  802.11 header for each rx MPDU, so it is certain that
	 *  htt_rx_mpdu_wifi_hdr_retrieve will succeed.
	 *  However, both for robustness, e.g. if this function is given a
	 *  MSDU descriptor rather than a MPDU descriptor, and to make it
	 *  clear to static analysis that this code is safe, add an explicit
	 *  check that htt_rx_mpdu_wifi_hdr_retrieve provides a non-NULL value.
	 */
	if (!wh || !IEEE80211_IS_DATA(wh))
		return;

	/* ignore frames for non-existent bssids */
	qdf_mem_copy(a1, wh->i_addr1, QDF_MAC_ADDR_SIZE);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		if (qdf_mem_cmp(a1, vdev->mac_addr.raw, QDF_MAC_ADDR_SIZE))
			break;
	}
	if (!vdev)
		return;

	msg.wh = wh;
	msg.msdu = msdu;
	msg.vdev_id = vdev->vdev_id;
	wdi_event_handler(WDI_EVENT_RX_PEER_INVALID, pdev->id,
			  &msg);
}
#else
static inline
void ol_rx_process_inv_peer(ol_txrx_pdev_handle pdev,
			    void *rx_mpdu_desc, qdf_nbuf_t msdu)
{
}
#endif

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
static inline int16_t
ol_rx_rssi_avg(struct ol_txrx_pdev_t *pdev, int16_t rssi_old, int16_t rssi_new)
{
	int rssi_old_weight;

	if (rssi_new == HTT_RSSI_INVALID)
		return rssi_old;
	if (rssi_old == HTT_RSSI_INVALID)
		return rssi_new;

	rssi_old_weight =
		(1 << pdev->rssi_update_shift) - pdev->rssi_new_weight;
	return (rssi_new * pdev->rssi_new_weight +
		rssi_old * rssi_old_weight) >> pdev->rssi_update_shift;
}

static void
ol_rx_ind_rssi_update(struct ol_txrx_peer_t *peer, qdf_nbuf_t rx_ind_msg)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	peer->rssi_dbm = ol_rx_rssi_avg(pdev, peer->rssi_dbm,
					htt_rx_ind_rssi_dbm(pdev->htt_pdev,
							    rx_ind_msg));
}

static void
ol_rx_mpdu_rssi_update(struct ol_txrx_peer_t *peer, void *rx_mpdu_desc)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	if (!peer)
		return;
	peer->rssi_dbm = ol_rx_rssi_avg(pdev, peer->rssi_dbm,
					htt_rx_mpdu_desc_rssi_dbm(
						pdev->htt_pdev,
						rx_mpdu_desc));
}

#else
#define ol_rx_ind_rssi_update(peer, rx_ind_msg) /* no-op */
#define ol_rx_mpdu_rssi_update(peer, rx_mpdu_desc)      /* no-op */
#endif /* QCA_SUPPORT_PEER_DATA_RX_RSSI */

static void discard_msdus(htt_pdev_handle htt_pdev,
			  qdf_nbuf_t head_msdu,
			  qdf_nbuf_t tail_msdu)
{
	while (1) {
		qdf_nbuf_t next;

		next = qdf_nbuf_next(
			head_msdu);
		htt_rx_desc_frame_free
			(htt_pdev,
			 head_msdu);
		if (head_msdu ==
		    tail_msdu) {
			break;
		}
		head_msdu = next;
	}
}

static void chain_msdus(htt_pdev_handle htt_pdev,
			qdf_nbuf_t head_msdu,
			qdf_nbuf_t tail_msdu)
{
	while (1) {
		qdf_nbuf_t next;

		next = qdf_nbuf_next(head_msdu);
		htt_rx_desc_frame_free(
			htt_pdev,
			head_msdu);
		if (head_msdu == tail_msdu)
			break;
		head_msdu = next;
	}
}

static void process_reorder(ol_txrx_pdev_handle pdev,
			    void *rx_mpdu_desc,
			    uint8_t tid,
			    struct ol_txrx_peer_t *peer,
			    qdf_nbuf_t head_msdu,
			    qdf_nbuf_t tail_msdu,
			    int num_mpdu_ranges,
			    int num_mpdus,
			    bool rx_ind_release)
{
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	enum htt_rx_status mpdu_status;
	int reorder_idx;

	reorder_idx = htt_rx_mpdu_desc_reorder_idx(htt_pdev, rx_mpdu_desc,
						   true);
	OL_RX_REORDER_TRACE_ADD(pdev, tid,
				reorder_idx,
				htt_rx_mpdu_desc_seq_num(htt_pdev,
							 rx_mpdu_desc, false),
				1);
	ol_rx_mpdu_rssi_update(peer, rx_mpdu_desc);
	/*
	 * In most cases, out-of-bounds and duplicate sequence number detection
	 * is performed by the target, but in some cases it is done by the host.
	 * Specifically, the host does rx out-of-bounds sequence number
	 * detection for:
	 * 1.  Peregrine or Rome target
	 *     for peer-TIDs that do not have aggregation enabled, if the
	 *     RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK flag
	 *     is set during the driver build.
	 * 2.  Riva-family targets, which have rx reorder timeouts handled by
	 *     the host rather than the target.
	 *     (The target already does duplicate detection, but the host
	 *     may have given up waiting for a particular sequence number before
	 *     it arrives.  In this case, the out-of-bounds sequence number
	 *     of the late frame allows the host to discard it, rather than
	 *     sending it out of order.
	 */
	mpdu_status = OL_RX_SEQ_NUM_CHECK(pdev,
						  peer,
						  tid,
						  rx_mpdu_desc);
	if (mpdu_status != htt_rx_status_ok) {
		/*
		 * If the sequence number was out of bounds, the MPDU needs
		 * to be discarded.
		 */
		discard_msdus(htt_pdev, head_msdu, tail_msdu);
		/*
		 * For Peregrine and Rome,
		 * OL_RX_REORDER_SEQ_NUM_CHECK should only fail for the case
		 * of (duplicate) non-aggregates.
		 *
		 * For Riva, Pronto and Northstar,
		 * there should be only one MPDU delivered at a time.
		 * Thus, there are no further MPDUs that need to be
		 * processed here.
		 * Just to be sure this is true, check the assumption
		 * that this was the only MPDU referenced by the rx
		 * indication.
		 */
		TXRX_ASSERT2((num_mpdu_ranges == 1) && num_mpdus == 1);

		/*
		 * The MPDU was not stored in the rx reorder array, so
		 * there's nothing to release.
		 */
		rx_ind_release = false;
	} else {
		ol_rx_reorder_store(pdev, peer, tid,
				    reorder_idx, head_msdu, tail_msdu);
		if (peer->tids_rx_reorder[tid].win_sz_mask == 0) {
			peer->tids_last_seq[tid] = htt_rx_mpdu_desc_seq_num(
				htt_pdev,
				rx_mpdu_desc, false);
		}
	}
} /* process_reorder */

#ifdef WLAN_FEATURE_DSRC
static void
ol_rx_ocb_update_peer(ol_txrx_pdev_handle pdev, qdf_nbuf_t rx_ind_msg,
		      struct ol_txrx_peer_t *peer)
{
	int i;

	htt_rx_ind_legacy_rate(pdev->htt_pdev, rx_ind_msg,
			       &peer->last_pkt_legacy_rate,
			       &peer->last_pkt_legacy_rate_sel);
	peer->last_pkt_rssi_cmb = htt_rx_ind_rssi_dbm(
				pdev->htt_pdev, rx_ind_msg);
	for (i = 0; i < 4; i++)
		peer->last_pkt_rssi[i] =
		    htt_rx_ind_rssi_dbm_chain(pdev->htt_pdev, rx_ind_msg, i);

	htt_rx_ind_timestamp(pdev->htt_pdev, rx_ind_msg,
			     &peer->last_pkt_timestamp_microsec,
			     &peer->last_pkt_timestamp_submicrosec);
	peer->last_pkt_tsf = htt_rx_ind_tsf32(pdev->htt_pdev, rx_ind_msg);
	peer->last_pkt_tid = htt_rx_ind_ext_tid(pdev->htt_pdev, rx_ind_msg);
}
#else
static void
ol_rx_ocb_update_peer(ol_txrx_pdev_handle pdev, qdf_nbuf_t rx_ind_msg,
		      struct ol_txrx_peer_t *peer)
{
}
#endif

void
ol_rx_indication_handler(ol_txrx_pdev_handle pdev,
			 qdf_nbuf_t rx_ind_msg,
			 uint16_t peer_id, uint8_t tid, int num_mpdu_ranges)
{
	int mpdu_range;
	unsigned int seq_num_start = 0, seq_num_end = 0;
	bool rx_ind_release = false;
	struct ol_txrx_vdev_t *vdev = NULL;
	struct ol_txrx_peer_t *peer;
	htt_pdev_handle htt_pdev;
	uint16_t center_freq;
	uint16_t chan1;
	uint16_t chan2;
	uint8_t phymode;
	bool ret;
	uint32_t msdu_count = 0;

	htt_pdev = pdev->htt_pdev;
	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (!peer) {
		/*
		 * If we can't find a peer send this packet to OCB interface
		 * using OCB self peer
		 */
		if (!ol_txrx_get_ocb_peer(pdev, &peer))
			peer = NULL;
	}

	if (peer) {
		vdev = peer->vdev;
		ol_rx_ind_rssi_update(peer, rx_ind_msg);

		if (vdev->opmode == wlan_op_mode_ocb)
			ol_rx_ocb_update_peer(pdev, rx_ind_msg, peer);
	}

	TXRX_STATS_INCR(pdev, priv.rx.normal.ppdus);

	OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev);

	if (htt_rx_ind_flush(pdev->htt_pdev, rx_ind_msg) && peer) {
		htt_rx_ind_flush_seq_num_range(pdev->htt_pdev, rx_ind_msg,
					       &seq_num_start, &seq_num_end);
		if (tid == HTT_INVALID_TID) {
			/*
			 * host/FW reorder state went out-of sync
			 * for a while because FW ran out of Rx indication
			 * buffer. We have to discard all the buffers in
			 * reorder queue.
			 */
			ol_rx_reorder_peer_cleanup(vdev, peer);
		} else {
			if (tid >= OL_TXRX_NUM_EXT_TIDS) {
				ol_txrx_err("invalid tid, %u", tid);
				WARN_ON(1);
				return;
			}
			ol_rx_reorder_flush(vdev, peer, tid, seq_num_start,
					    seq_num_end, htt_rx_flush_release);
		}
	}

	if (htt_rx_ind_release(pdev->htt_pdev, rx_ind_msg)) {
		/*
		 * The ind info of release is saved here and do release at the
		 * end. This is for the reason of in HL case, the qdf_nbuf_t
		 * for msg and payload are the same buf. And the buf will be
		 * changed during processing
		 */
		rx_ind_release = true;
		htt_rx_ind_release_seq_num_range(pdev->htt_pdev, rx_ind_msg,
						 &seq_num_start, &seq_num_end);
	}
#ifdef DEBUG_DMA_DONE
	pdev->htt_pdev->rx_ring.dbg_initial_msdu_payld =
		pdev->htt_pdev->rx_ring.sw_rd_idx.msdu_payld;
#endif

	for (mpdu_range = 0; mpdu_range < num_mpdu_ranges; mpdu_range++) {
		enum htt_rx_status status;
		int i, num_mpdus;
		qdf_nbuf_t head_msdu, tail_msdu, msdu;
		void *rx_mpdu_desc;

#ifdef DEBUG_DMA_DONE
		pdev->htt_pdev->rx_ring.dbg_mpdu_range = mpdu_range;
#endif

		htt_rx_ind_mpdu_range_info(pdev->htt_pdev, rx_ind_msg,
					   mpdu_range, &status, &num_mpdus);
		if ((status == htt_rx_status_ok) && peer) {
			TXRX_STATS_ADD(pdev, priv.rx.normal.mpdus, num_mpdus);
			/* valid frame - deposit it into rx reordering buffer */
			for (i = 0; i < num_mpdus; i++) {
				int msdu_chaining;
				/*
				 * Get a linked list of the MSDUs that comprise
				 * this MPDU.
				 * This also attaches each rx MSDU descriptor to
				 * the corresponding rx MSDU network buffer.
				 * (In some systems, the rx MSDU desc is already
				 * in the same buffer as the MSDU payload; in
				 * other systems they are separate, so a pointer
				 * needs to be set in the netbuf to locate the
				 * corresponding rx descriptor.)
				 *
				 * It is necessary to call htt_rx_amsdu_pop
				 * before htt_rx_mpdu_desc_list_next, because
				 * the (MPDU) rx descriptor has DMA unmapping
				 * done during the htt_rx_amsdu_pop call.
				 * The rx desc should not be accessed until this
				 * DMA unmapping has been done, since the DMA
				 * unmapping involves making sure the cache area
				 * for the mapped buffer is flushed, so the data
				 * written by the MAC DMA into memory will be
				 * fetched, rather than garbage from the cache.
				 */

#ifdef DEBUG_DMA_DONE
				pdev->htt_pdev->rx_ring.dbg_mpdu_count = i;
#endif

				msdu_chaining =
					htt_rx_amsdu_pop(htt_pdev,
							 rx_ind_msg,
							 &head_msdu,
							 &tail_msdu,
							 &msdu_count);
#ifdef HTT_RX_RESTORE
				if (htt_pdev->rx_ring.rx_reset) {
					ol_rx_trigger_restore(htt_pdev,
							      head_msdu,
							      tail_msdu);
					OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(
									pdev);
					return;
				}
#endif
				rx_mpdu_desc =
					htt_rx_mpdu_desc_list_next(htt_pdev,
								   rx_ind_msg);
				ret = htt_rx_msdu_center_freq(htt_pdev, peer,
					rx_mpdu_desc, &center_freq, &chan1,
					&chan2, &phymode);
				if (ret == true) {
					peer->last_pkt_center_freq =
						center_freq;
				} else {
					peer->last_pkt_center_freq = 0;
				}

				/* Pktlog */
				ol_rx_send_pktlog_event(pdev, peer,
							head_msdu, 1);

				if (msdu_chaining) {
					/*
					 * TBDXXX - to deliver SDU with
					 * chaining, we need to stitch those
					 * scattered buffers into one single
					 * buffer.
					 * Just discard it now.
					 */
					chain_msdus(htt_pdev,
						    head_msdu,
						    tail_msdu);
				} else {
					process_reorder(pdev, rx_mpdu_desc,
							tid, peer,
							head_msdu, tail_msdu,
							num_mpdu_ranges,
							num_mpdus,
							rx_ind_release);
				}

			}
		} else {
			/* invalid frames - discard them */
			OL_RX_REORDER_TRACE_ADD(pdev, tid,
						TXRX_SEQ_NUM_ERR(status),
						TXRX_SEQ_NUM_ERR(status),
						num_mpdus);
			TXRX_STATS_ADD(pdev, priv.rx.err.mpdu_bad, num_mpdus);
			for (i = 0; i < num_mpdus; i++) {
				/* pull the MPDU's MSDUs off the buffer queue */
				htt_rx_amsdu_pop(htt_pdev, rx_ind_msg, &msdu,
						 &tail_msdu, &msdu_count);
#ifdef HTT_RX_RESTORE
				if (htt_pdev->rx_ring.rx_reset) {
					ol_rx_trigger_restore(htt_pdev, msdu,
							      tail_msdu);
					OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(
									pdev);
					return;
				}
#endif
				/* pull the MPDU desc off the desc queue */
				rx_mpdu_desc =
					htt_rx_mpdu_desc_list_next(htt_pdev,
								   rx_ind_msg);
				OL_RX_ERR_STATISTICS_2(pdev, vdev, peer,
						       rx_mpdu_desc, msdu,
						       status);

				if (status == htt_rx_status_tkip_mic_err &&
				    vdev && peer) {
					union htt_rx_pn_t pn;
					uint8_t key_id;

					htt_rx_mpdu_desc_pn(
						pdev->htt_pdev,
						htt_rx_msdu_desc_retrieve(
							pdev->htt_pdev,
							msdu), &pn, 48);
					if (htt_rx_msdu_desc_key_id(
						    pdev->htt_pdev,
						    htt_rx_msdu_desc_retrieve(
							    pdev->htt_pdev,
							    msdu),
						    &key_id) == true) {
						ol_rx_send_mic_err_ind(
							vdev->pdev,
							vdev->vdev_id,
							peer->mac_addr.raw,
							tid, 0,
							OL_RX_ERR_TKIP_MIC,
							msdu, &pn.pn48,
							key_id);
					}
				}

				if (status != htt_rx_status_ctrl_mgmt_null) {
					/* Pktlog */
					ol_rx_send_pktlog_event(pdev,
						 peer, msdu, 1);
				}

				if (status == htt_rx_status_err_inv_peer) {
					/* once per mpdu */
					ol_rx_process_inv_peer(pdev,
							       rx_mpdu_desc,
							       msdu);
				}

				while (1) {
					/* Free the nbuf */
					qdf_nbuf_t next;

					next = qdf_nbuf_next(msdu);
					htt_rx_desc_frame_free(htt_pdev, msdu);
					if (msdu == tail_msdu)
						break;
					msdu = next;
				}
			}
		}
	}
	/*
	 * Now that a whole batch of MSDUs have been pulled out of HTT
	 * and put into the rx reorder array, it is an appropriate time
	 * to request HTT to provide new rx MSDU buffers for the target
	 * to fill.
	 * This could be done after the end of this function, but it's
	 * better to do it now, rather than waiting until after the driver
	 * and OS finish processing the batch of rx MSDUs.
	 */
	htt_rx_msdu_buff_replenish(htt_pdev);

	if ((true == rx_ind_release) && peer && vdev) {
		ol_rx_reorder_release(vdev, peer, tid, seq_num_start,
				      seq_num_end);
	}
	OL_RX_REORDER_TIMEOUT_UPDATE(peer, tid);
	OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);

	if (pdev->rx.flags.defrag_timeout_check)
		ol_rx_defrag_waitlist_flush(pdev);
}
#endif

void
ol_rx_sec_ind_handler(ol_txrx_pdev_handle pdev,
		      uint16_t peer_id,
		      enum htt_sec_type sec_type,
		      int is_unicast, uint32_t *michael_key, uint32_t *rx_pn)
{
	struct ol_txrx_peer_t *peer;
	int sec_index, i;

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (!peer) {
		ol_txrx_err(
			"Couldn't find peer from ID %d - skipping security inits\n",
			peer_id);
		return;
	}
	ol_txrx_dbg(
		"sec spec for peer %pK ("QDF_MAC_ADDR_FMT"): %s key of type %d\n",
		peer,
		QDF_MAC_ADDR_REF(peer->mac_addr.raw),
		is_unicast ? "ucast" : "mcast", sec_type);
	sec_index = is_unicast ? txrx_sec_ucast : txrx_sec_mcast;
	peer->security[sec_index].sec_type = sec_type;
	/*
	 * michael key only valid for TKIP
	 * but for simplicity, copy it anyway
	 */
	qdf_mem_copy(&peer->security[sec_index].michael_key[0],
		     michael_key,
		     sizeof(peer->security[sec_index].michael_key));

	if (sec_type != htt_sec_type_wapi) {
		qdf_mem_zero(peer->tids_last_pn_valid,
			    OL_TXRX_NUM_EXT_TIDS);
	} else if (sec_index == txrx_sec_mcast || peer->tids_last_pn_valid[0]) {
		for (i = 0; i < OL_TXRX_NUM_EXT_TIDS; i++) {
			/*
			 * Setting PN valid bit for WAPI sec_type,
			 * since WAPI PN has to be started with predefined value
			 */
			peer->tids_last_pn_valid[i] = 1;
			qdf_mem_copy((uint8_t *) &peer->tids_last_pn[i],
				     (uint8_t *) rx_pn,
				     sizeof(union htt_rx_pn_t));
			peer->tids_last_pn[i].pn128[1] =
				qdf_cpu_to_le64(
					peer->tids_last_pn[i].pn128[1]);
			peer->tids_last_pn[i].pn128[0] =
				qdf_cpu_to_le64(
					peer->tids_last_pn[i].pn128[0]);
			if (sec_index == txrx_sec_ucast)
				peer->tids_rekey_flag[i] = 1;
		}
	}
}

void ol_rx_notify(struct cdp_cfg *cfg_pdev,
		  uint8_t vdev_id,
		  uint8_t *peer_mac_addr,
		  int tid,
		  uint32_t tsf32,
		  enum ol_rx_notify_type notify_type, qdf_nbuf_t rx_frame)
{
	/*
	 * NOTE: This is used in qca_main for AP mode to handle IGMP
	 * packets specially. Umac has a corresponding handler for this
	 * not sure if we need to have this for CLD as well.
	 */
}

#ifdef WLAN_PARTIAL_REORDER_OFFLOAD
/**
 * @brief Look into a rx MSDU to see what kind of special handling it requires
 * @details
 *      This function is called when the host rx SW sees that the target
 *      rx FW has marked a rx MSDU as needing inspection.
 *      Based on the results of the inspection, the host rx SW will infer
 *      what special handling to perform on the rx frame.
 *      Currently, the only type of frames that require special handling
 *      are IGMP frames.  The rx data-path SW checks if the frame is IGMP
 *      (it should be, since the target would not have set the inspect flag
 *      otherwise), and then calls the ol_rx_notify function so the
 *      control-path SW can perform multicast group membership learning
 *      by sniffing the IGMP frame.
 */
#define SIZEOF_80211_HDR (sizeof(struct ieee80211_frame))
static void
ol_rx_inspect(struct ol_txrx_vdev_t *vdev,
	      struct ol_txrx_peer_t *peer,
	      unsigned int tid, qdf_nbuf_t msdu, void *rx_desc)
{
	ol_txrx_pdev_handle pdev = vdev->pdev;
	uint8_t *data, *l3_hdr;
	uint16_t ethertype;
	int offset;

	data = qdf_nbuf_data(msdu);
	if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
		offset = SIZEOF_80211_HDR + LLC_SNAP_HDR_OFFSET_ETHERTYPE;
		l3_hdr = data + SIZEOF_80211_HDR + LLC_SNAP_HDR_LEN;
	} else {
		offset = QDF_MAC_ADDR_SIZE * 2;
		l3_hdr = data + ETHERNET_HDR_LEN;
	}
	ethertype = (data[offset] << 8) | data[offset + 1];
	if (ethertype == ETHERTYPE_IPV4) {
		offset = IPV4_HDR_OFFSET_PROTOCOL;
		if (l3_hdr[offset] == IP_PROTOCOL_IGMP) {
			ol_rx_notify(pdev->ctrl_pdev,
				     vdev->vdev_id,
				     peer->mac_addr.raw,
				     tid,
				     htt_rx_mpdu_desc_tsf32(pdev->htt_pdev,
							    rx_desc),
				     OL_RX_NOTIFY_IPV4_IGMP, msdu);
		}
	}
}
#endif

void
ol_rx_offload_deliver_ind_handler(ol_txrx_pdev_handle pdev,
				  qdf_nbuf_t msg, uint16_t msdu_cnt)
{
	int vdev_id, peer_id, tid;
	qdf_nbuf_t head_buf, tail_buf, buf;
	struct ol_txrx_peer_t *peer;
	uint8_t fw_desc;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;

	if (msdu_cnt > htt_rx_offload_msdu_cnt(htt_pdev)) {
		ol_txrx_err("invalid msdu_cnt=%u", msdu_cnt);

		if (pdev->cfg.is_high_latency)
			htt_rx_desc_frame_free(htt_pdev, msg);

		return;
	}

	while (msdu_cnt) {
		if (!htt_rx_offload_msdu_pop(htt_pdev, msg, &vdev_id, &peer_id,
					&tid, &fw_desc, &head_buf, &tail_buf)) {
			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
			if (peer) {
				ol_rx_data_process(peer, head_buf);
			} else {
				buf = head_buf;
				while (1) {
					qdf_nbuf_t next;

					next = qdf_nbuf_next(buf);
					htt_rx_desc_frame_free(htt_pdev, buf);
					if (buf == tail_buf)
						break;
					buf = next;
				}
			}
		}
		msdu_cnt--;
	}
	htt_rx_msdu_buff_replenish(htt_pdev);
}

void
ol_rx_send_mic_err_ind(struct ol_txrx_pdev_t *pdev, uint8_t vdev_id,
		       uint8_t *peer_mac_addr, int tid, uint32_t tsf32,
		       enum ol_rx_err_type err_type, qdf_nbuf_t rx_frame,
		       uint64_t *pn, uint8_t key_id)
{
	struct cdp_rx_mic_err_info mic_failure_info;
	qdf_ether_header_t *eth_hdr;
	struct ol_if_ops *tops = NULL;
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_soc_handle ol_txrx_soc = &soc->cdp_soc;

	if (err_type != OL_RX_ERR_TKIP_MIC)
		return;

	if (qdf_nbuf_len(rx_frame) < sizeof(*eth_hdr))
		return;

	eth_hdr = (qdf_ether_header_t *)qdf_nbuf_data(rx_frame);

	qdf_copy_macaddr((struct qdf_mac_addr *)&mic_failure_info.ta_mac_addr,
			 (struct qdf_mac_addr *)peer_mac_addr);
	qdf_copy_macaddr((struct qdf_mac_addr *)&mic_failure_info.da_mac_addr,
			 (struct qdf_mac_addr *)eth_hdr->ether_dhost);
	mic_failure_info.key_id = key_id;
	mic_failure_info.multicast =
		IEEE80211_IS_MULTICAST(eth_hdr->ether_dhost);
	qdf_mem_copy(mic_failure_info.tsc, pn, SIR_CIPHER_SEQ_CTR_SIZE);
	mic_failure_info.frame_type = cdp_rx_frame_type_802_3;
	mic_failure_info.data = NULL;
	mic_failure_info.vdev_id = vdev_id;

	tops = ol_txrx_soc->ol_ops;
	if (tops->rx_mic_error)
		tops->rx_mic_error(soc->psoc, pdev->id, &mic_failure_info);
}

void
ol_rx_mic_error_handler(
	ol_txrx_pdev_handle pdev,
	u_int8_t tid,
	u_int16_t peer_id,
	void *msdu_desc,
	qdf_nbuf_t msdu)
{
	union htt_rx_pn_t pn = {0};
	u_int8_t key_id = 0;

	struct ol_txrx_peer_t *peer = NULL;
	struct ol_txrx_vdev_t *vdev = NULL;

	if (pdev) {
		TXRX_STATS_MSDU_INCR(pdev, rx.dropped_mic_err, msdu);
		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (peer) {
			vdev = peer->vdev;
			if (vdev) {
				htt_rx_mpdu_desc_pn(vdev->pdev->htt_pdev,
						    msdu_desc, &pn, 48);

				if (htt_rx_msdu_desc_key_id(
					vdev->pdev->htt_pdev, msdu_desc,
					&key_id) == true) {
					ol_rx_send_mic_err_ind(vdev->pdev,
						vdev->vdev_id,
						peer->mac_addr.raw, tid, 0,
						OL_RX_ERR_TKIP_MIC, msdu,
						&pn.pn48, key_id);
				}
			}
		}
		/* Pktlog */
		ol_rx_send_pktlog_event(pdev, peer, msdu, 1);
	}
}

#ifdef WLAN_PARTIAL_REORDER_OFFLOAD
/**
 * @brief Check the first msdu to decide whether the a-msdu should be accepted.
 */
static bool
ol_rx_filter(struct ol_txrx_vdev_t *vdev,
	     struct ol_txrx_peer_t *peer, qdf_nbuf_t msdu, void *rx_desc)
{
#define FILTER_STATUS_REJECT 1
#define FILTER_STATUS_ACCEPT 0
	uint8_t *wh;
	uint32_t offset = 0;
	uint16_t ether_type = 0;
	bool is_encrypted = false, is_mcast = false;
	uint8_t i;
	enum privacy_filter_packet_type packet_type =
		PRIVACY_FILTER_PACKET_UNICAST;
	ol_txrx_pdev_handle pdev = vdev->pdev;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	int sec_idx;

	/*
	 * Safemode must avoid the PrivacyExemptionList and
	 * ExcludeUnencrypted checking
	 */
	if (vdev->safemode)
		return FILTER_STATUS_ACCEPT;

	is_mcast = htt_rx_msdu_is_wlan_mcast(htt_pdev, rx_desc);
	if (vdev->num_filters > 0) {
		if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
			offset = SIZEOF_80211_HDR +
				LLC_SNAP_HDR_OFFSET_ETHERTYPE;
		} else {
			offset = QDF_MAC_ADDR_SIZE * 2;
		}
		/* get header info from msdu */
		wh = qdf_nbuf_data(msdu);

		/* get ether type */
		ether_type = (wh[offset] << 8) | wh[offset + 1];
		/* get packet type */
		if (true == is_mcast)
			packet_type = PRIVACY_FILTER_PACKET_MULTICAST;
		else
			packet_type = PRIVACY_FILTER_PACKET_UNICAST;
	}
	/* get encrypt info */
	is_encrypted = htt_rx_mpdu_is_encrypted(htt_pdev, rx_desc);
#ifdef ATH_SUPPORT_WAPI
	if ((true == is_encrypted) && (ETHERTYPE_WAI == ether_type)) {
		/*
		 * We expect the WAI frames to be always unencrypted when
		 * the UMAC gets it
		 */
		return FILTER_STATUS_REJECT;
	}
#endif /* ATH_SUPPORT_WAPI */

	for (i = 0; i < vdev->num_filters; i++) {
		enum privacy_filter filter_type;
		enum privacy_filter_packet_type filter_packet_type;

		/* skip if the ether type does not match */
		if (vdev->privacy_filters[i].ether_type != ether_type)
			continue;

		/* skip if the packet type does not match */
		filter_packet_type = vdev->privacy_filters[i].packet_type;
		if (filter_packet_type != packet_type &&
		    filter_packet_type != PRIVACY_FILTER_PACKET_BOTH) {
			continue;
		}

		filter_type = vdev->privacy_filters[i].filter_type;
		if (filter_type == PRIVACY_FILTER_ALWAYS) {
			/*
			 * In this case, we accept the frame if and only if
			 * it was originally NOT encrypted.
			 */
			if (true == is_encrypted)
				return FILTER_STATUS_REJECT;
			else
				return FILTER_STATUS_ACCEPT;

		} else if (filter_type == PRIVACY_FILTER_KEY_UNAVAILABLE) {
			/*
			 * In this case, we reject the frame if it was
			 * originally NOT encrypted but we have the key mapping
			 * key for this frame.
			 */
			if (!is_encrypted &&
			    !is_mcast &&
			    (peer->security[txrx_sec_ucast].sec_type !=
			     htt_sec_type_none) &&
			    (peer->keyinstalled || !ETHERTYPE_IS_EAPOL_WAPI(
				    ether_type))) {
				return FILTER_STATUS_REJECT;
			} else {
				return FILTER_STATUS_ACCEPT;
			}
		} else {
			/*
			 * The privacy exemption does not apply to this frame.
			 */
			break;
		}
	}

	/*
	 * If the privacy exemption list does not apply to the frame,
	 * check ExcludeUnencrypted.
	 * If ExcludeUnencrypted is not set, or if this was oringially
	 * an encrypted frame, it will be accepted.
	 */
	if (!vdev->drop_unenc || (true == is_encrypted))
		return FILTER_STATUS_ACCEPT;

	/*
	 *  If this is a open connection, it will be accepted.
	 */
	sec_idx = (true == is_mcast) ? txrx_sec_mcast : txrx_sec_ucast;
	if (peer->security[sec_idx].sec_type == htt_sec_type_none)
		return FILTER_STATUS_ACCEPT;

	if ((false == is_encrypted) && vdev->drop_unenc) {
		OL_RX_ERR_STATISTICS(pdev, vdev, OL_RX_ERR_PRIVACY,
				     pdev->sec_types[htt_sec_type_none],
				     is_mcast);
	}
	return FILTER_STATUS_REJECT;
}
#endif

#ifdef WLAN_FEATURE_TSF_PLUS
#ifdef CONFIG_HL_SUPPORT
void ol_rx_timestamp(struct cdp_cfg *cfg_pdev,
		     void *rx_desc, qdf_nbuf_t msdu)
{
	struct htt_rx_ppdu_desc_t *rx_ppdu_desc;

	if (!ol_cfg_is_ptp_rx_opt_enabled(cfg_pdev))
		return;

	if (!rx_desc || !msdu)
		return;

	rx_ppdu_desc = (struct htt_rx_ppdu_desc_t *)((uint8_t *)(rx_desc) -
			HTT_RX_IND_HL_BYTES + HTT_RX_IND_HDR_PREFIX_BYTES);
	msdu->tstamp = ns_to_ktime((u_int64_t)rx_ppdu_desc->tsf32 *
				   NSEC_PER_USEC);
}

static inline void ol_rx_timestamp_update(ol_txrx_pdev_handle pdev,
					  qdf_nbuf_t head_msdu,
					  qdf_nbuf_t tail_msdu)
{
	qdf_nbuf_t loop_msdu;
	struct htt_host_rx_desc_base *rx_desc;

	loop_msdu = head_msdu;
	while (loop_msdu) {
		rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, loop_msdu);
		ol_rx_timestamp(pdev->ctrl_pdev, rx_desc, loop_msdu);
		loop_msdu = qdf_nbuf_next(loop_msdu);
	}
}
#else
void ol_rx_timestamp(struct cdp_cfg *cfg_pdev,
		     void *rx_desc, qdf_nbuf_t msdu)
{
	struct htt_host_rx_desc_base *rx_mpdu_desc = rx_desc;
	uint32_t tsf64_low32, tsf64_high32;
	uint64_t tsf64, tsf64_ns;

	if (!ol_cfg_is_ptp_rx_opt_enabled(cfg_pdev))
		return;

	if (!rx_mpdu_desc || !msdu)
		return;

	tsf64_low32 = rx_mpdu_desc->ppdu_end.wb_timestamp_lower_32;
	tsf64_high32 = rx_mpdu_desc->ppdu_end.wb_timestamp_upper_32;

	tsf64 = (uint64_t)tsf64_high32 << 32 | tsf64_low32;
	if (tsf64 * NSEC_PER_USEC < tsf64)
		tsf64_ns = 0;
	else
		tsf64_ns = tsf64 * NSEC_PER_USEC;

	msdu->tstamp = ns_to_ktime(tsf64_ns);
}

/**
 * ol_rx_timestamp_update() - update msdu tsf64 timestamp
 * @pdev: pointer to txrx handle
 * @head_msdu: pointer to head msdu
 * @tail_msdu: pointer to tail msdu
 *
 * Return: none
 */
static inline void ol_rx_timestamp_update(ol_txrx_pdev_handle pdev,
					  qdf_nbuf_t head_msdu,
					  qdf_nbuf_t tail_msdu)
{
	qdf_nbuf_t loop_msdu;
	uint64_t hostime, detlahostime, tsf64_time;
	struct htt_host_rx_desc_base *rx_desc;

	if (!ol_cfg_is_ptp_rx_opt_enabled(pdev->ctrl_pdev))
		return;

	if (!tail_msdu)
		return;

	hostime = ktime_get_ns();
	rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, tail_msdu);
	if (rx_desc->ppdu_end.wb_timestamp_lower_32 == 0 &&
	    rx_desc->ppdu_end.wb_timestamp_upper_32 == 0) {
		detlahostime = hostime - pdev->last_host_time;
		do_div(detlahostime, NSEC_PER_USEC);
		tsf64_time = pdev->last_tsf64_time + detlahostime;

		rx_desc->ppdu_end.wb_timestamp_lower_32 =
						tsf64_time & 0xFFFFFFFF;
		rx_desc->ppdu_end.wb_timestamp_upper_32 = tsf64_time >> 32;
	} else {
		pdev->last_host_time = hostime;
		pdev->last_tsf64_time =
		  (uint64_t)rx_desc->ppdu_end.wb_timestamp_upper_32 << 32 |
		  rx_desc->ppdu_end.wb_timestamp_lower_32;
	}

	loop_msdu = head_msdu;
	while (loop_msdu) {
		ol_rx_timestamp(pdev->ctrl_pdev, rx_desc, loop_msdu);
		loop_msdu = qdf_nbuf_next(loop_msdu);
	}
}
#endif
#else
void ol_rx_timestamp(struct cdp_cfg *cfg_pdev,
		     void *rx_desc, qdf_nbuf_t msdu)
{
}

static inline void ol_rx_timestamp_update(ol_txrx_pdev_handle pdev,
					  qdf_nbuf_t head_msdu,
					  qdf_nbuf_t tail_msdu)
{
}
#endif

#ifdef WLAN_FEATURE_DSRC
static inline
void ol_rx_ocb_prepare_rx_stats_header(struct ol_txrx_vdev_t *vdev,
				       struct ol_txrx_peer_t *peer,
				       qdf_nbuf_t msdu)
{
	int i;
	struct ol_txrx_ocb_chan_info *chan_info = 0;
	int packet_freq = peer->last_pkt_center_freq;

	for (i = 0; i < vdev->ocb_channel_count; i++) {
		if (vdev->ocb_channel_info[i].chan_freq == packet_freq) {
			chan_info = &vdev->ocb_channel_info[i];
			break;
		}
	}

	if (!chan_info || !chan_info->disable_rx_stats_hdr) {
		qdf_ether_header_t eth_header = { {0} };
		struct ocb_rx_stats_hdr_t rx_header = {0};

		/*
		 * Construct the RX stats header and
		 * push that to the frontof the packet.
		 */
		rx_header.version = 1;
		rx_header.length = sizeof(rx_header);
		rx_header.channel_freq = peer->last_pkt_center_freq;
		rx_header.rssi_cmb = peer->last_pkt_rssi_cmb;
		qdf_mem_copy(rx_header.rssi, peer->last_pkt_rssi,
			     sizeof(rx_header.rssi));

		if (peer->last_pkt_legacy_rate_sel)
			rx_header.datarate = 0xFF;
		else if (peer->last_pkt_legacy_rate == 0x8)
			rx_header.datarate = 6;
		else if (peer->last_pkt_legacy_rate == 0x9)
			rx_header.datarate = 4;
		else if (peer->last_pkt_legacy_rate == 0xA)
			rx_header.datarate = 2;
		else if (peer->last_pkt_legacy_rate == 0xB)
			rx_header.datarate = 0;
		else if (peer->last_pkt_legacy_rate == 0xC)
			rx_header.datarate = 7;
		else if (peer->last_pkt_legacy_rate == 0xD)
			rx_header.datarate = 5;
		else if (peer->last_pkt_legacy_rate == 0xE)
			rx_header.datarate = 3;
		else if (peer->last_pkt_legacy_rate == 0xF)
			rx_header.datarate = 1;
		else
			rx_header.datarate = 0xFF;

		rx_header.timestamp_microsec =
			 peer->last_pkt_timestamp_microsec;
		rx_header.timestamp_submicrosec =
			 peer->last_pkt_timestamp_submicrosec;
		rx_header.tsf32 = peer->last_pkt_tsf;
		rx_header.ext_tid = peer->last_pkt_tid;

		qdf_nbuf_push_head(msdu, sizeof(rx_header));
		qdf_mem_copy(qdf_nbuf_data(msdu),
			     &rx_header, sizeof(rx_header));

		/*
		 * Construct the ethernet header with
		 * type 0x8152 and push that to the
		 * front of the packet to indicate the
		 * RX stats header.
		 */
		eth_header.ether_type = QDF_SWAP_U16(ETHERTYPE_OCB_RX);
		qdf_nbuf_push_head(msdu, sizeof(eth_header));
		qdf_mem_copy(qdf_nbuf_data(msdu), &eth_header,
			     sizeof(eth_header));
	}
}
#else
static inline
void ol_rx_ocb_prepare_rx_stats_header(struct ol_txrx_vdev_t *vdev,
				       struct ol_txrx_peer_t *peer,
				       qdf_nbuf_t msdu)
{
}
#endif

#ifdef WLAN_PARTIAL_REORDER_OFFLOAD
void
ol_rx_deliver(struct ol_txrx_vdev_t *vdev,
	      struct ol_txrx_peer_t *peer, unsigned int tid,
	      qdf_nbuf_t msdu_list)
{
	ol_txrx_pdev_handle pdev = vdev->pdev;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	qdf_nbuf_t deliver_list_head = NULL;
	qdf_nbuf_t deliver_list_tail = NULL;
	qdf_nbuf_t msdu;
	bool filter = false;
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
	struct ol_rx_decap_info_t info;

	qdf_mem_zero(&info, sizeof(info));
#endif

	msdu = msdu_list;
	/*
	 * Check each MSDU to see whether it requires special handling,
	 * and free each MSDU's rx descriptor
	 */
	while (msdu) {
		void *rx_desc;
		int discard, inspect, dummy_fwd;
		qdf_nbuf_t next = qdf_nbuf_next(msdu);

		rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, msdu);
		/* for HL, point to payload right now*/
		if (pdev->cfg.is_high_latency) {
			qdf_nbuf_pull_head(msdu,
				htt_rx_msdu_rx_desc_size_hl(htt_pdev, rx_desc));
		}

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
		info.is_msdu_cmpl_mpdu =
			htt_rx_msdu_desc_completes_mpdu(htt_pdev, rx_desc);
		info.is_first_subfrm =
			htt_rx_msdu_first_msdu_flag(htt_pdev, rx_desc);
		if (OL_RX_DECAP(vdev, peer, msdu, &info) != A_OK) {
			discard = 1;
			ol_txrx_dbg(
				"decap error %pK from peer %pK ("QDF_MAC_ADDR_FMT") len %d\n",
				msdu, peer,
				QDF_MAC_ADDR_REF(peer->mac_addr.raw),
				qdf_nbuf_len(msdu));
			goto DONE;
		}
#endif
		htt_rx_msdu_actions(pdev->htt_pdev, rx_desc, &discard,
				    &dummy_fwd, &inspect);
		if (inspect)
			ol_rx_inspect(vdev, peer, tid, msdu, rx_desc);

		/*
		 * Check the first msdu in the mpdu, if it will be filtered out,
		 * then discard the entire mpdu.
		 */
		if (htt_rx_msdu_first_msdu_flag(htt_pdev, rx_desc))
			filter = ol_rx_filter(vdev, peer, msdu, rx_desc);

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
DONE:
#endif
		htt_rx_msdu_desc_free(htt_pdev, msdu);
		if (discard || (true == filter)) {
			ol_txrx_frms_dump("rx discarding:",
					  pdev, deliver_list_head,
					  ol_txrx_frm_dump_tcp_seq |
					  ol_txrx_frm_dump_contents,
					  0 /* don't print contents */);
			qdf_nbuf_free(msdu);
			/*
			 * If discarding packet is last packet of the delivery
			 * list, NULL terminator should be added
			 * for delivery list.
			 */
			if (!next && deliver_list_head) {
				/* add NULL terminator */
				qdf_nbuf_set_next(deliver_list_tail, NULL);
			}
		} else {
			/*
			 *  If this is for OCB,
			 *  then prepend the RX stats header.
			 */
			if (vdev->opmode == wlan_op_mode_ocb)
				ol_rx_ocb_prepare_rx_stats_header(vdev, peer,
								  msdu);

			OL_RX_PEER_STATS_UPDATE(peer, msdu);
			OL_RX_ERR_STATISTICS_1(pdev, vdev, peer, rx_desc,
					       OL_RX_ERR_NONE);
			TXRX_STATS_MSDU_INCR(vdev->pdev, rx.delivered, msdu);

			ol_rx_timestamp(pdev->ctrl_pdev, rx_desc, msdu);
			OL_TXRX_LIST_APPEND(deliver_list_head,
					    deliver_list_tail, msdu);
			QDF_NBUF_CB_DP_TRACE_PRINT(msdu) = false;
			qdf_dp_trace_set_track(msdu, QDF_RX);
		}
		msdu = next;
	}
	/* sanity check - are there any frames left to give to the OS shim? */
	if (!deliver_list_head)
		return;

	ol_txrx_frms_dump("rx delivering:",
			  pdev, deliver_list_head,
			  ol_txrx_frm_dump_tcp_seq | ol_txrx_frm_dump_contents,
			  0 /* don't print contents */);

	ol_rx_data_process(peer, deliver_list_head);
}
#endif

void
ol_rx_discard(struct ol_txrx_vdev_t *vdev,
	      struct ol_txrx_peer_t *peer, unsigned int tid,
	      qdf_nbuf_t msdu_list)
{
	while (msdu_list) {
		qdf_nbuf_t msdu = msdu_list;

		msdu_list = qdf_nbuf_next(msdu_list);
		ol_txrx_dbg("discard rx %pK", msdu);
		qdf_nbuf_free(msdu);
	}
}

void ol_rx_peer_init(struct ol_txrx_pdev_t *pdev, struct ol_txrx_peer_t *peer)
{
	uint8_t tid;

	for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
		ol_rx_reorder_init(&peer->tids_rx_reorder[tid], tid);

		/* invalid sequence number */
		peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX;
		/* invalid reorder index number */
		peer->tids_next_rel_idx[tid] = INVALID_REORDER_INDEX;

	}
	/*
	 * Set security defaults: no PN check, no security.
	 * The target may send a HTT SEC_IND message to overwrite
	 * these defaults.
	 */
	peer->security[txrx_sec_ucast].sec_type =
		peer->security[txrx_sec_mcast].sec_type = htt_sec_type_none;
	peer->keyinstalled = 0;

	peer->last_assoc_rcvd = 0;
	peer->last_disassoc_rcvd = 0;
	peer->last_deauth_rcvd = 0;

	qdf_atomic_init(&peer->fw_pn_check);
}

void
ol_rx_peer_cleanup(struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer)
{
	peer->keyinstalled = 0;
	peer->last_assoc_rcvd = 0;
	peer->last_disassoc_rcvd = 0;
	peer->last_deauth_rcvd = 0;
	ol_rx_reorder_peer_cleanup(vdev, peer);
}

/*
 * Free frames including both rx descriptors and buffers
 */
void ol_rx_frames_free(htt_pdev_handle htt_pdev, qdf_nbuf_t frames)
{
	qdf_nbuf_t next, frag = frames;

	while (frag) {
		next = qdf_nbuf_next(frag);
		htt_rx_desc_frame_free(htt_pdev, frag);
		frag = next;
	}
}

#ifdef WLAN_FULL_REORDER_OFFLOAD
void
ol_rx_in_order_indication_handler(ol_txrx_pdev_handle pdev,
				  qdf_nbuf_t rx_ind_msg,
				  uint16_t peer_id,
				  uint8_t tid, uint8_t is_offload)
{
	struct ol_txrx_vdev_t *vdev = NULL;
	struct ol_txrx_peer_t *peer = NULL;
	struct ol_txrx_peer_t *peer_head = NULL;
	htt_pdev_handle htt_pdev = NULL;
	int status;
	qdf_nbuf_t head_msdu = NULL, tail_msdu = NULL;
	uint8_t *rx_ind_data;
	uint32_t *msg_word;
	uint32_t msdu_count;
	uint8_t pktlog_bit;
	uint32_t filled = 0;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	bool offloaded_pkt;
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("invalid tid, %u", tid);
		WARN_ON(1);
		return;
	}

	if (pdev) {
		if (qdf_unlikely(QDF_GLOBAL_MONITOR_MODE == cds_get_conparam()))
			peer = pdev->self_peer;
		else
			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		htt_pdev = pdev->htt_pdev;
	} else {
		ol_txrx_err("Invalid pdev passed!");
		qdf_assert_always(pdev);
		return;
	}

#if defined(HELIUMPLUS_DEBUG)
	qdf_print("rx_ind_msg 0x%pK peer_id %d tid %d is_offload %d",
		  rx_ind_msg, peer_id, tid, is_offload);
#endif

	pktlog_bit = (htt_rx_amsdu_rx_in_order_get_pktlog(rx_ind_msg) == 0x01);
	rx_ind_data = qdf_nbuf_data(rx_ind_msg);
	msg_word = (uint32_t *)rx_ind_data;
	/* Get the total number of MSDUs */
	msdu_count = HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_GET(*(msg_word + 1));

	ol_rx_ind_record_event(msdu_count, OL_RX_INDICATION_POP_START);

	/*
	 * Get a linked list of the MSDUs in the rx in order indication.
	 * This also attaches each rx MSDU descriptor to the
	 * corresponding rx MSDU network buffer.
	 */
	status = htt_rx_amsdu_pop(htt_pdev, rx_ind_msg, &head_msdu,
				  &tail_msdu, &msdu_count);
	ol_rx_ind_record_event(status, OL_RX_INDICATION_POP_END);

	if (qdf_unlikely(0 == status)) {
		ol_txrx_warn("pop failed");
		return;
	}

	/*
	 * Replenish the rx buffer ring first to provide buffers to the target
	 * rather than waiting for the indeterminate time taken by the OS
	 * to consume the rx frames
	 */
	filled = htt_rx_msdu_buff_in_order_replenish(htt_pdev, msdu_count);
	ol_rx_ind_record_event(filled, OL_RX_INDICATION_BUF_REPLENISH);

	if (!head_msdu) {
		ol_txrx_dbg("No packet to send to HDD");
		return;
	}

	/* Send the chain of MSDUs to the OS */
	/* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
	qdf_nbuf_set_next(tail_msdu, NULL);

	/* Packet Capture Mode */

	if ((ucfg_pkt_capture_get_pktcap_mode((void *)soc->psoc) &
	      PKT_CAPTURE_MODE_DATA_ONLY)) {
		offloaded_pkt = ucfg_pkt_capture_rx_offloaded_pkt(rx_ind_msg);
		if (peer) {
			vdev = peer->vdev;
			if (peer->vdev) {
				qdf_spin_lock_bh(&pdev->peer_ref_mutex);
				peer_head = TAILQ_FIRST(&vdev->peer_list);
				qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
				if (peer_head) {
					qdf_spin_lock_bh(
						&peer_head->peer_info_lock);
					qdf_mem_copy(bssid,
						     &peer_head->mac_addr.raw,
						     QDF_MAC_ADDR_SIZE);
					qdf_spin_unlock_bh(
						&peer_head->peer_info_lock);

					ucfg_pkt_capture_rx_msdu_process(
							bssid, head_msdu,
							peer->vdev->vdev_id,
							htt_pdev);
				}
			}
		} else if (offloaded_pkt) {
			ucfg_pkt_capture_rx_msdu_process(
						bssid, head_msdu,
						HTT_INVALID_VDEV,
						htt_pdev);

			ucfg_pkt_capture_rx_drop_offload_pkt(head_msdu);
			return;
		}
	}

	/* Pktlog */
	ol_rx_send_pktlog_event(pdev, peer, head_msdu, pktlog_bit);

	/*
	 * if this is an offload indication, peer id is carried in the
	 * rx buffer
	 */
	if (peer) {
		vdev = peer->vdev;
	} else {
		ol_txrx_dbg("Couldn't find peer from ID 0x%x", peer_id);
		while (head_msdu) {
			qdf_nbuf_t msdu = head_msdu;

			head_msdu = qdf_nbuf_next(head_msdu);
			TXRX_STATS_MSDU_INCR(pdev,
				 rx.dropped_peer_invalid, msdu);
			htt_rx_desc_frame_free(htt_pdev, msdu);
		}
		return;
	}

	/*Loop msdu to fill tstamp with tsf64 time in ol_rx_timestamp*/
	ol_rx_timestamp_update(pdev, head_msdu, tail_msdu);

	peer->rx_opt_proc(vdev, peer, tid, head_msdu);
}
#endif

#ifndef REMOVE_PKT_LOG
/**
 * ol_rx_pkt_dump_call() - updates status and
 * calls packetdump callback to log rx packet
 *
 * @msdu: rx packet
 * @peer_id: peer id
 * @status: status of rx packet
 *
 * This function is used to update the status of rx packet
 * and then calls packetdump callback to log that packet.
 *
 * Return: None
 *
 */
void ol_rx_pkt_dump_call(
	qdf_nbuf_t msdu,
	uint8_t peer_id,
	uint8_t status)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_soc_handle soc_hdl = ol_txrx_soc_t_to_cdp_soc_t(soc);
	struct ol_txrx_peer_t *peer = NULL;
	ol_txrx_pktdump_cb packetdump_cb;
	ol_txrx_pdev_handle pdev;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (!peer) {
		ol_txrx_dbg("peer with peer id %d is NULL", peer_id);
		return;
	}

	packetdump_cb = pdev->ol_rx_packetdump_cb;
	if (packetdump_cb &&
	    wlan_op_mode_sta == peer->vdev->opmode)
		packetdump_cb(soc_hdl, OL_TXRX_PDEV_ID, peer->vdev->vdev_id,
			      msdu, status, RX_DATA_PKT);
}
#endif

#ifdef WLAN_FULL_REORDER_OFFLOAD
/* the msdu_list passed here must be NULL terminated */
void
ol_rx_in_order_deliver(struct ol_txrx_vdev_t *vdev,
		       struct ol_txrx_peer_t *peer,
		       unsigned int tid, qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu;

	msdu = msdu_list;
	/*
	 * Currently, this does not check each MSDU to see whether it requires
	 * special handling. MSDUs that need special handling (example: IGMP
	 * frames) should be sent via a separate HTT message. Also, this does
	 * not do rx->tx forwarding or filtering.
	 */

	while (msdu) {
		qdf_nbuf_t next = qdf_nbuf_next(msdu);

		DPTRACE(qdf_dp_trace(msdu,
			QDF_DP_TRACE_RX_TXRX_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID,
			qdf_nbuf_data_addr(msdu),
			sizeof(qdf_nbuf_data(msdu)), QDF_RX));

		OL_RX_PEER_STATS_UPDATE(peer, msdu);
		OL_RX_ERR_STATISTICS_1(vdev->pdev, vdev, peer, rx_desc,
				       OL_RX_ERR_NONE);
		TXRX_STATS_MSDU_INCR(vdev->pdev, rx.delivered, msdu);

		msdu = next;
	}

	ol_txrx_frms_dump("rx delivering:",
			  pdev, deliver_list_head,
			  ol_txrx_frm_dump_tcp_seq | ol_txrx_frm_dump_contents,
			  0 /* don't print contents */);

	ol_rx_data_process(peer, msdu_list);
}
#endif

#ifndef CONFIG_HL_SUPPORT
void
ol_rx_offload_paddr_deliver_ind_handler(htt_pdev_handle htt_pdev,
					uint32_t msdu_count,
					uint32_t *msg_word)
{
	int vdev_id, peer_id, tid;
	qdf_nbuf_t head_buf, tail_buf, buf;
	struct ol_txrx_peer_t *peer;
	uint8_t fw_desc;
	int msdu_iter = 0;

	while (msdu_count) {
		if (htt_rx_offload_paddr_msdu_pop_ll(
						htt_pdev, msg_word, msdu_iter,
						 &vdev_id, &peer_id, &tid,
						 &fw_desc, &head_buf,
						 &tail_buf)) {
			msdu_iter++;
			msdu_count--;
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
				  "skip msg_word %pK, msdu #%d, continue next",
				  msg_word, msdu_iter);
			continue;
		}

		peer = ol_txrx_peer_find_by_id(htt_pdev->txrx_pdev, peer_id);
		if (peer) {
			QDF_NBUF_CB_DP_TRACE_PRINT(head_buf) = false;
			qdf_dp_trace_set_track(head_buf, QDF_RX);
			QDF_NBUF_CB_TX_PACKET_TRACK(head_buf) =
						QDF_NBUF_TX_PKT_DATA_TRACK;
			qdf_dp_trace_log_pkt(peer->vdev->vdev_id,
				head_buf, QDF_RX,
				QDF_TRACE_DEFAULT_PDEV_ID);
			DPTRACE(qdf_dp_trace(head_buf,
				QDF_DP_TRACE_RX_OFFLOAD_HTT_PACKET_PTR_RECORD,
				QDF_TRACE_DEFAULT_PDEV_ID,
				qdf_nbuf_data_addr(head_buf),
				sizeof(qdf_nbuf_data(head_buf)), QDF_RX));
			ol_rx_data_process(peer, head_buf);
		} else {
			buf = head_buf;
			while (1) {
				qdf_nbuf_t next;

				next = qdf_nbuf_next(buf);
				htt_rx_desc_frame_free(htt_pdev, buf);
				if (buf == tail_buf)
					break;
				buf = next;
			}
		}
		msdu_iter++;
		msdu_count--;
	}
	htt_rx_msdu_buff_replenish(htt_pdev);
}
#endif

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * ol_htt_mon_note_chan() - Update monitor channel information
 * @pdev:  handle to the physical device
 * @mon_ch: Monitor channel
 *
 * Return: None
 */
void ol_htt_mon_note_chan(struct cdp_pdev *ppdev, int mon_ch)
{
	struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)ppdev;

	htt_rx_mon_note_capture_channel(pdev->htt_pdev, mon_ch);
}
#endif

#ifdef NEVERDEFINED
/**
 * @brief populates vow ext stats in given network buffer.
 * @param msdu - network buffer handle
 * @param pdev - handle to htt dev.
 */
void ol_ath_add_vow_extstats(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
	/* FIX THIS:
	 * txrx should not be directly using data types (scn)
	 * that are internal to other modules.
	 */
	struct ol_ath_softc_net80211 *scn =
		(struct ol_ath_softc_net80211 *)pdev->ctrl_pdev;
	uint8_t *data, *l3_hdr, *bp;
	uint16_t ethertype;
	int offset;
	struct vow_extstats vowstats;

	if (scn->vow_extstats == 0)
		return;

	data = qdf_nbuf_data(msdu);

	offset = QDF_MAC_ADDR_SIZE * 2;
	l3_hdr = data + ETHERNET_HDR_LEN;
	ethertype = (data[offset] << 8) | data[offset + 1];
	if (ethertype == ETHERTYPE_IPV4) {
		offset = IPV4_HDR_OFFSET_PROTOCOL;
		if ((l3_hdr[offset] == IP_PROTOCOL_UDP) &&
				(l3_hdr[0] == IP_VER4_N_NO_EXTRA_HEADERS)) {
			bp = data + EXT_HDR_OFFSET;

			if ((data[RTP_HDR_OFFSET] == UDP_PDU_RTP_EXT) &&
					(bp[0] == 0x12) &&
					(bp[1] == 0x34) &&
					(bp[2] == 0x00) && (bp[3] == 0x08)) {
				/*
				 * Clear UDP checksum so we do not have
				 * to recalculate it
				 * after filling in status fields.
				 */
				data[UDP_CKSUM_OFFSET] = 0;
				data[(UDP_CKSUM_OFFSET + 1)] = 0;

				bp += IPERF3_DATA_OFFSET;

				htt_rx_get_vowext_stats(msdu,
						&vowstats);

				/* control channel RSSI */
				*bp++ = vowstats.rx_rssi_ctl0;
				*bp++ = vowstats.rx_rssi_ctl1;
				*bp++ = vowstats.rx_rssi_ctl2;

				/* rx rate info */
				*bp++ = vowstats.rx_bw;
				*bp++ = vowstats.rx_sgi;
				*bp++ = vowstats.rx_nss;

				*bp++ = vowstats.rx_rssi_comb;
				/* rsflags */
				*bp++ = vowstats.rx_rs_flags;

				/* Time stamp Lo */
				*bp++ = (uint8_t)
					((vowstats.
					  rx_macTs & 0x0000ff00) >> 8);
				*bp++ = (uint8_t)
					(vowstats.rx_macTs & 0x0000ff);
				/* rx phy errors */
				*bp++ = (uint8_t)
					((scn->chan_stats.
					  phy_err_cnt >> 8) & 0xff);
				*bp++ =
					(uint8_t) (scn->chan_stats.
							phy_err_cnt & 0xff);
				/* rx clear count */
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  rx_clear_count >> 24) & 0xff);
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  rx_clear_count >> 16) & 0xff);
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  rx_clear_count >> 8) & 0xff);
				*bp++ = (uint8_t)
					(scn->mib_cycle_cnts.
					 rx_clear_count & 0xff);
				/* rx cycle count */
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  cycle_count >> 24) & 0xff);
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  cycle_count >> 16) & 0xff);
				*bp++ = (uint8_t)
					((scn->mib_cycle_cnts.
					  cycle_count >> 8) & 0xff);
				*bp++ = (uint8_t)
					(scn->mib_cycle_cnts.
					 cycle_count & 0xff);

				*bp++ = vowstats.rx_ratecode;
				*bp++ = vowstats.rx_moreaggr;

				/* sequence number */
				*bp++ = (uint8_t)
					((vowstats.rx_seqno >> 8) &
					 0xff);
				*bp++ = (uint8_t)
					(vowstats.rx_seqno & 0xff);
			}
		}
	}
}

#endif

#ifdef WLAN_CFR_ENABLE
void ol_rx_cfr_capture_msg_handler(qdf_nbuf_t htt_t2h_msg)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	HTT_PEER_CFR_CAPTURE_MSG_TYPE cfr_type;
	struct htt_cfr_dump_compl_ind *cfr_dump;
	struct htt_cfr_dump_ind_type_1 cfr_ind;
	struct csi_cfr_header cfr_hdr = {};
	uint32_t mem_index, req_id, vdev_id;
	uint32_t *msg_word;
	uint8_t *mac_addr;

	msg_word = (uint32_t *)qdf_nbuf_data(htt_t2h_msg);

	/* First payload word */
	msg_word++;
	cfr_dump = (struct htt_cfr_dump_compl_ind *)msg_word;
	cfr_type = cfr_dump->msg_type;
	if (cfr_type != HTT_PEER_CFR_CAPTURE_MSG_TYPE_1) {
		ol_txrx_err("Unsupported cfr msg type 0x%x", cfr_type);
		return;
	}

	/* Second payload word */
	msg_word++;
	req_id = HTT_T2H_CFR_DUMP_TYPE1_MEM_REQ_ID_GET(*msg_word);
	if (req_id != CFR_CAPTURE_HOST_MEM_REQ_ID) {
		ol_txrx_err("Invalid req id in cfr capture msg");
		return;
	}
	cfr_hdr.start_magic_num = 0xDEADBEAF;
	cfr_hdr.u.meta_v1.status = HTT_T2H_CFR_DUMP_TYPE1_STATUS_GET(
					*msg_word);
	cfr_hdr.u.meta_v1.capture_bw = HTT_T2H_CFR_DUMP_TYPE1_CAP_BW_GET(
					*msg_word);
	cfr_hdr.u.meta_v1.capture_mode = HTT_T2H_CFR_DUMP_TYPE1_MODE_GET(
					*msg_word);
	cfr_hdr.u.meta_v1.sts_count = HTT_T2H_CFR_DUMP_TYPE1_STS_GET(
					*msg_word);
	cfr_hdr.u.meta_v1.channel_bw = HTT_T2H_CFR_DUMP_TYPE1_CHAN_BW_GET(
					*msg_word);
	cfr_hdr.u.meta_v1.capture_type = HTT_T2H_CFR_DUMP_TYPE1_CAP_TYPE_GET(
					*msg_word);

	vdev_id = HTT_T2H_CFR_DUMP_TYPE1_VDEV_ID_GET(*msg_word);

	mac_addr = (uint8_t *)(msg_word + 1);
	qdf_mem_copy(cfr_hdr.u.meta_v1.peer_addr, mac_addr, QDF_MAC_ADDR_SIZE);

	cfr_ind = cfr_dump->htt_cfr_dump_compl_ind_type_1;

	cfr_hdr.u.meta_v1.prim20_chan = cfr_ind.chan.chan_mhz;
	cfr_hdr.u.meta_v1.center_freq1 = cfr_ind.chan.band_center_freq1;
	cfr_hdr.u.meta_v1.center_freq2 = cfr_ind.chan.band_center_freq2;
	cfr_hdr.u.meta_v1.phy_mode = cfr_ind.chan.chan_mode;
	cfr_hdr.u.meta_v1.length = cfr_ind.length;
	cfr_hdr.u.meta_v1.timestamp = cfr_ind.timestamp;

	mem_index = cfr_ind.index;

	ucfg_cfr_capture_data((void *)soc->psoc, vdev_id, &cfr_hdr, mem_index);
}
#endif
