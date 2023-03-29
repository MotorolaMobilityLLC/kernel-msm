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

/* OS abstraction libraries */
#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_atomic.h>         /* qdf_atomic_read, etc. */
#include <qdf_util.h>           /* qdf_unlikely */

/* APIs for other modules */
#include <htt.h>                /* HTT_TX_EXT_TID_MGMT */
#include <ol_htt_tx_api.h>      /* htt_tx_desc_tid */

/* internal header files relevant for all systems */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT1 */
#include <ol_tx_desc.h>         /* ol_tx_desc */
#include <ol_tx_send.h>         /* ol_tx_send */
#include <ol_txrx.h>

/* internal header files relevant only for HL systems */
#include <ol_tx_classify.h>   /* ol_tx_classify, ol_tx_classify_mgmt */
#include <ol_tx_queue.h>        /* ol_tx_enqueue */
#include <ol_tx_sched.h>      /* ol_tx_sched */

/* internal header files relevant only for specific systems (Pronto) */
#include <ol_txrx_encap.h>      /* OL_TX_ENCAP, etc */
#include <ol_tx.h>
#include <cdp_txrx_ipa.h>

#include <hif.h>              /* HIF_DEVICE */
#include <htc_api.h>    /* Layering violation, but required for fast path */
#include <htt_internal.h>
#include <htt_types.h>        /* htc_endpoint */
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_handle.h>

#if defined(HIF_PCI) || defined(HIF_SNOC) || defined(HIF_AHB) || \
    defined(HIF_IPCI)
#include <ce_api.h>
#endif

/**
 * ol_tx_setup_fastpath_ce_handles() Update ce_handle for fastpath use.
 *
 * @osc: pointer to HIF context
 * @pdev: pointer to ol pdev
 *
 * Return: void
 */
void ol_tx_setup_fastpath_ce_handles(struct hif_opaque_softc *osc,
				     struct ol_txrx_pdev_t *pdev)
{
	/*
	 * Before the HTT attach, set up the CE handles
	 * CE handles are (struct CE_state *)
	 * This is only required in the fast path
	 */
	pdev->ce_tx_hdl = hif_get_ce_handle(osc, CE_HTT_H2T_MSG);
}

qdf_nbuf_t
ol_tx_ll_wrapper(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	struct hif_opaque_softc *hif_device =
		(struct hif_opaque_softc *)cds_get_context(QDF_MODULE_ID_HIF);

	if (qdf_likely(hif_device &&
		       hif_is_fastpath_mode_enabled(hif_device))) {
		msdu_list = ol_tx_ll_fast(vdev, msdu_list);
	} else {
		qdf_print("Fast path is disabled\n");
		QDF_BUG(0);
	}
	return msdu_list;
}

/**
 * ol_tx_trace_pkt() - Trace TX packet at OL layer
 *
 * @skb: skb to be traced
 * @msdu_id: msdu_id of the packet
 * @vdev_id: vdev_id of the packet
 *
 * Return: None
 */
static inline void ol_tx_trace_pkt(qdf_nbuf_t skb, uint16_t msdu_id,
				   uint8_t vdev_id)
{
	DPTRACE(qdf_dp_trace_ptr(skb,
				 QDF_DP_TRACE_TXRX_FAST_PACKET_PTR_RECORD,
				 QDF_TRACE_DEFAULT_PDEV_ID,
				 qdf_nbuf_data_addr(skb),
				 sizeof(qdf_nbuf_data(skb)),
				 msdu_id, vdev_id));

	qdf_dp_trace_log_pkt(vdev_id, skb, QDF_TX, QDF_TRACE_DEFAULT_PDEV_ID);

	DPTRACE(qdf_dp_trace_data_pkt(skb, QDF_TRACE_DEFAULT_PDEV_ID,
				      QDF_DP_TRACE_TX_PACKET_RECORD,
				      msdu_id, QDF_TX));
}

/**
 * ol_tx_tso_adjust_pkt_dnld_len() Update download len for TSO pkt
 *
 * @msdu: tso mdsu for which download length is updated
 * @msdu_info: tso msdu_info for the msdu
 * @download_len: packet download length
 *
 * Return: Updated download length
 */
#if defined(FEATURE_TSO)
static uint32_t
ol_tx_tso_adjust_pkt_dnld_len(qdf_nbuf_t msdu,
			      struct ol_txrx_msdu_info_t *msdu_info,
			      uint32_t download_len)
{
	uint32_t frag0_len = 0, delta = 0, eit_hdr_len = 0;
	uint32_t loc_download_len = download_len;

	frag0_len = qdf_nbuf_get_frag_len(msdu, 0);
	loc_download_len -= frag0_len;
	eit_hdr_len = msdu_info->tso_info.curr_seg->seg.tso_frags[0].length;

	if (eit_hdr_len < loc_download_len) {
		delta = loc_download_len - eit_hdr_len;
		download_len -= delta;
	}

	return download_len;
}
#else
static uint32_t
ol_tx_tso_adjust_pkt_dnld_len(qdf_nbuf_t msdu,
			      struct ol_txrx_msdu_info_t *msdu_info,
			      uint32_t download_len)
{
	return download_len;
}
#endif

/**
 * ol_tx_prepare_ll_fast() Alloc and prepare Tx descriptor
 *
 * Allocate and prepare Tx descriptor with msdu and fragment descritor
 * inforamtion.
 *
 * @pdev: pointer to ol pdev handle
 * @vdev: pointer to ol vdev handle
 * @msdu: linked list of msdu packets
 * @pkt_download_len: packet download length
 * @ep_id: endpoint ID
 * @msdu_info: Handle to msdu_info
 *
 * Return: Pointer to Tx descriptor
 */
static inline struct ol_tx_desc_t *
ol_tx_prepare_ll_fast(struct ol_txrx_pdev_t *pdev,
		      ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu,
		      uint32_t *pkt_download_len, uint32_t ep_id,
		      struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc = NULL;
	uint32_t *htt_tx_desc;
	void *htc_hdr_vaddr;
	u_int32_t num_frags, i;
	enum extension_header_type type;

	tx_desc = ol_tx_desc_alloc_wrapper(pdev, vdev, msdu_info);
	if (qdf_unlikely(!tx_desc))
		return NULL;

	tx_desc->netbuf = msdu;
	if (msdu_info->tso_info.is_tso) {
		tx_desc->tso_desc = msdu_info->tso_info.curr_seg;
		qdf_tso_seg_dbg_setowner(tx_desc->tso_desc, tx_desc);
		qdf_tso_seg_dbg_record(tx_desc->tso_desc,
				       TSOSEG_LOC_TXPREPLLFAST);
		tx_desc->tso_num_desc = msdu_info->tso_info.tso_num_seg_list;
		tx_desc->pkt_type = OL_TX_FRM_TSO;
		TXRX_STATS_MSDU_INCR(pdev, tx.tso.tso_pkts, msdu);
	} else {
		tx_desc->pkt_type = OL_TX_FRM_STD;
	}

	htt_tx_desc = tx_desc->htt_tx_desc;

#if defined(HELIUMPLUS)
	qdf_mem_zero(tx_desc->htt_frag_desc, sizeof(struct msdu_ext_desc_t));
#endif

	/* Make sure frags num is set to 0 */
	/*
	 * Do this here rather than in hardstart, so
	 * that we can hopefully take only one cache-miss while
	 * accessing skb->cb.
	 */

	/* HTT Header */
	/* TODO : Take care of multiple fragments */

	type = ol_tx_get_ext_header_type(vdev, msdu);

	/* TODO: Precompute and store paddr in ol_tx_desc_t */
	/* Virtual address of the HTT/HTC header, added by driver */
	htc_hdr_vaddr = (char *)htt_tx_desc - HTC_HEADER_LEN;
	if (qdf_unlikely(htt_tx_desc_init(pdev->htt_pdev, htt_tx_desc,
					  tx_desc->htt_tx_desc_paddr,
					  tx_desc->id, msdu,
					  &msdu_info->htt,
					  &msdu_info->tso_info,
					  NULL, type))) {
		/*
		 * HTT Tx descriptor initialization failed.
		 * therefore, free the tx desc
		 */
		ol_tx_desc_free(pdev, tx_desc);
		return NULL;
	}

	num_frags = qdf_nbuf_get_num_frags(msdu);
	/* num_frags are expected to be 2 max */
	num_frags = (num_frags > QDF_NBUF_CB_TX_MAX_EXTRA_FRAGS)
		? QDF_NBUF_CB_TX_MAX_EXTRA_FRAGS
		: num_frags;
#if defined(HELIUMPLUS)
	/*
	 * Use num_frags - 1, since 1 frag is used to store
	 * the HTT/HTC descriptor
	 * Refer to htt_tx_desc_init()
	 */
	htt_tx_desc_num_frags(pdev->htt_pdev, tx_desc->htt_frag_desc,
			      num_frags - 1);
#else /* ! defined(HELIUMPLUS) */
	htt_tx_desc_num_frags(pdev->htt_pdev, tx_desc->htt_tx_desc,
			      num_frags - 1);
#endif /* defined(HELIUMPLUS) */
	if (msdu_info->tso_info.is_tso) {
		htt_tx_desc_fill_tso_info(pdev->htt_pdev,
					  tx_desc->htt_frag_desc,
					  &msdu_info->tso_info);
		TXRX_STATS_TSO_SEG_UPDATE(pdev,
					  msdu_info->tso_info.msdu_stats_idx,
					  msdu_info->tso_info.curr_seg->seg);
	} else {
		for (i = 1; i < num_frags; i++) {
			qdf_size_t frag_len;
			qdf_dma_addr_t frag_paddr;

			frag_len = qdf_nbuf_get_frag_len(msdu, i);
			frag_paddr = qdf_nbuf_get_frag_paddr(msdu, i);
			if (type != EXT_HEADER_NOT_PRESENT) {
				frag_paddr +=
				    sizeof(struct htt_tx_msdu_desc_ext_t);
				frag_len -=
				    sizeof(struct htt_tx_msdu_desc_ext_t);
			}
#if defined(HELIUMPLUS)
			htt_tx_desc_frag(pdev->htt_pdev, tx_desc->htt_frag_desc,
					 i - 1, frag_paddr, frag_len);
#if defined(HELIUMPLUS_DEBUG)
			qdf_debug("htt_fdesc=%pK frag=%d frag_paddr=0x%0llx len=%zu",
				  tx_desc->htt_frag_desc,
				  i - 1, frag_paddr, frag_len);
			ol_txrx_dump_pkt(netbuf, frag_paddr, 64);
#endif /* HELIUMPLUS_DEBUG */
#else /* ! defined(HELIUMPLUS) */
			htt_tx_desc_frag(pdev->htt_pdev, tx_desc->htt_tx_desc,
					 i - 1, frag_paddr, frag_len);
#endif /* defined(HELIUMPLUS) */
		}
	}

	/*
	 * Do we want to turn on word_stream bit-map here ? For linux, non-TSO
	 * this is not required. We still have to mark the swap bit correctly,
	 * when posting to the ring
	 */
	/* Check to make sure, data download length is correct */

	/*
	 * TODO : Can we remove this check and always download a fixed length ?
	 */

	if (QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_EXT_HEADER(msdu))
		*pkt_download_len += sizeof(struct htt_tx_msdu_desc_ext_t);

	if (qdf_unlikely(qdf_nbuf_len(msdu) < *pkt_download_len))
		*pkt_download_len = qdf_nbuf_len(msdu);

	if (msdu_info->tso_info.curr_seg)
		*pkt_download_len = ol_tx_tso_adjust_pkt_dnld_len(
							msdu, msdu_info,
							*pkt_download_len);

	/* Fill the HTC header information */
	/*
	 * Passing 0 as the seq_no field, we can probably get away
	 * with it for the time being, since this is not checked in f/w
	 */
	/* TODO : Prefill this, look at multi-fragment case */
	if (ol_txrx_get_new_htt_msg_format(pdev))
		HTC_TX_DESC_FILL(htc_hdr_vaddr,
				 *pkt_download_len - HTC_HEADER_LEN, ep_id, 0);
	else
		HTC_TX_DESC_FILL(htc_hdr_vaddr, *pkt_download_len, ep_id, 0);

	return tx_desc;
}

#if defined(FEATURE_TSO)
/**
 * ol_tx_ll_fast() Update metadata information and send msdu to HIF/CE
 *
 * @vdev: handle to ol_txrx_vdev_t
 * @msdu_list: msdu list to be sent out.
 *
 * Return: on success return NULL, pointer to nbuf when it fails to send.
 */
qdf_nbuf_t
ol_tx_ll_fast(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu = msdu_list;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	uint32_t pkt_download_len;
	uint32_t ep_id = HTT_EPID_GET(pdev->htt_pdev);
	struct ol_txrx_msdu_info_t msdu_info;
	uint32_t tso_msdu_stats_idx = 0;

	qdf_mem_zero(&msdu_info, sizeof(msdu_info));
	msdu_info.htt.info.l2_hdr_type = vdev->pdev->htt_pkt_type;
	msdu_info.htt.action.tx_comp_req = 0;
	/*
	 * The msdu_list variable could be used instead of the msdu var,
	 * but just to clarify which operations are done on a single MSDU
	 * vs. a list of MSDUs, use a distinct variable for single MSDUs
	 * within the list.
	 */
	while (msdu) {
		qdf_nbuf_t next;
		struct ol_tx_desc_t *tx_desc;
		int segments = 1;

		msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
		msdu_info.peer = NULL;

		if (qdf_unlikely(ol_tx_prepare_tso(vdev, msdu, &msdu_info))) {
			ol_txrx_err("ol_tx_prepare_tso failed\n");
			TXRX_STATS_MSDU_LIST_INCR(vdev->pdev,
						  tx.dropped.host_reject,
						  msdu);
			return msdu;
		}

		segments = msdu_info.tso_info.num_segs;

		if (msdu_info.tso_info.is_tso) {
			tso_msdu_stats_idx =
					ol_tx_tso_get_stats_idx(vdev->pdev);
			msdu_info.tso_info.msdu_stats_idx = tso_msdu_stats_idx;
			ol_tx_tso_update_stats(vdev->pdev,
					       &(msdu_info.tso_info),
					       msdu, tso_msdu_stats_idx);
		}

		/*
		 * The netbuf may get linked into a different list
		 * inside the ce_send_fast function, so store the next
		 * pointer before the ce_send call.
		 */
		next = qdf_nbuf_next(msdu);
		/*
		 * Increment the skb->users count here, for this SKB, to make
		 * sure it will be freed only after receiving Tx completion
		 * of the last segment.
		 * Decrement skb->users count before sending last segment
		 */
		if (qdf_nbuf_is_tso(msdu) && segments)
			qdf_nbuf_inc_users(msdu);

		/* init the current segment to the 1st segment in the list */
		while (segments) {
			if (msdu_info.tso_info.curr_seg)
				QDF_NBUF_CB_PADDR(msdu) = msdu_info.tso_info.
					curr_seg->seg.tso_frags[0].paddr;

			segments--;

			msdu_info.htt.info.frame_type = pdev->htt_pkt_type;
			msdu_info.htt.info.vdev_id = vdev->vdev_id;
			msdu_info.htt.action.cksum_offload =
				qdf_nbuf_get_tx_cksum(msdu);
			switch (qdf_nbuf_get_exemption_type(msdu)) {
			case QDF_NBUF_EXEMPT_NO_EXEMPTION:
			case QDF_NBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
				/* We want to encrypt this frame */
				msdu_info.htt.action.do_encrypt = 1;
				break;
			case QDF_NBUF_EXEMPT_ALWAYS:
				/* We don't want to encrypt this frame */
				msdu_info.htt.action.do_encrypt = 0;
				break;
			default:
				msdu_info.htt.action.do_encrypt = 1;
				qdf_assert(0);
				break;
			}

			pkt_download_len = ((struct htt_pdev_t *)
					(pdev->htt_pdev))->download_len;
			tx_desc = ol_tx_prepare_ll_fast(pdev, vdev, msdu,
							&pkt_download_len,
							ep_id, &msdu_info);

			TXRX_STATS_MSDU_INCR(pdev, tx.from_stack, msdu);

			if (qdf_likely(tx_desc)) {
				struct qdf_tso_seg_elem_t *next_seg;

				/*
				 * if this is a jumbo nbuf, then increment the
				 * number of nbuf users for each additional
				 * segment of the msdu. This will ensure that
				 * the skb is freed only after receiving tx
				 * completion for all segments of an nbuf.
				 */
				if (segments !=
					(msdu_info.tso_info.num_segs - 1))
					qdf_nbuf_inc_users(msdu);

				ol_tx_trace_pkt(msdu, tx_desc->id,
						vdev->vdev_id);
				/*
				 * If debug display is enabled, show the meta
				 * data being downloaded to the target via the
				 * HTT tx descriptor.
				 */
				htt_tx_desc_display(tx_desc->htt_tx_desc);

				/* mark the relevant tso_seg free-able */
				if (msdu_info.tso_info.curr_seg) {
					msdu_info.tso_info.curr_seg->
						sent_to_target = 1;
					next_seg = msdu_info.tso_info.
						curr_seg->next;
				} else {
					next_seg = NULL;
				}

				/* Decrement the skb-users count if segment
				 * is the last segment or the only segment
				 */
				if (tx_desc->pkt_type == OL_TX_FRM_TSO &&
				    segments == 0)
					qdf_nbuf_tx_free(msdu, 0);

				if ((ce_send_fast(pdev->ce_tx_hdl, msdu,
						  ep_id,
						  pkt_download_len) == 0)) {
					struct qdf_tso_info_t *tso_info =
							&msdu_info.tso_info;
					/*
					 * If TSO packet, free associated
					 * remaining TSO segment descriptors
					 */
					if (tx_desc->pkt_type ==
							OL_TX_FRM_TSO) {
						tso_info->curr_seg = next_seg;
						ol_free_remaining_tso_segs(vdev,
							&msdu_info, true);
						if (segments ==
						    (msdu_info.tso_info.num_segs
						     - 1))
							qdf_nbuf_tx_free(
							msdu,
							QDF_NBUF_PKT_ERROR);
					}

					/*
					 * The packet could not be sent.
					 * Free the descriptor, return the
					 * packet to the caller.
					 */
					ol_tx_desc_frame_free_nonstd(pdev,
						tx_desc,
						htt_tx_status_download_fail);
					return msdu;
				}
				if (msdu_info.tso_info.curr_seg)
					msdu_info.tso_info.curr_seg = next_seg;

				if (msdu_info.tso_info.is_tso) {
					TXRX_STATS_TSO_INC_SEG(vdev->pdev,
						tso_msdu_stats_idx);
					TXRX_STATS_TSO_INC_SEG_IDX(vdev->pdev,
						tso_msdu_stats_idx);
				}
			} else {
				/*
				 * If TSO packet, free associated
				 * remaining TSO segment descriptors
				 */
				if (qdf_nbuf_is_tso(msdu)) {
					ol_free_remaining_tso_segs(vdev,
							&msdu_info, true);
					if (segments ==
					    (msdu_info.tso_info.num_segs - 1))
						qdf_nbuf_tx_free(msdu,
							 QDF_NBUF_PKT_ERROR);
				}
				TXRX_STATS_MSDU_LIST_INCR(
					pdev, tx.dropped.host_reject, msdu);
				/* the list of unaccepted MSDUs */
				return msdu;
			}
		} /* while segments */

		msdu = next;
	} /* while msdus */
	return NULL; /* all MSDUs were accepted */
}
#else
qdf_nbuf_t
ol_tx_ll_fast(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu = msdu_list;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	uint32_t pkt_download_len;
	uint32_t ep_id = HTT_EPID_GET(pdev->htt_pdev);
	struct ol_txrx_msdu_info_t msdu_info;

	msdu_info.htt.info.l2_hdr_type = vdev->pdev->htt_pkt_type;
	msdu_info.htt.action.tx_comp_req = 0;
	msdu_info.tso_info.is_tso = 0;
	/*
	 * The msdu_list variable could be used instead of the msdu var,
	 * but just to clarify which operations are done on a single MSDU
	 * vs. a list of MSDUs, use a distinct variable for single MSDUs
	 * within the list.
	 */
	while (msdu) {
		qdf_nbuf_t next;
		struct ol_tx_desc_t *tx_desc;

		msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
		msdu_info.peer = NULL;

		msdu_info.htt.info.frame_type = pdev->htt_pkt_type;
		msdu_info.htt.info.vdev_id = vdev->vdev_id;
		msdu_info.htt.action.cksum_offload =
			qdf_nbuf_get_tx_cksum(msdu);
		switch (qdf_nbuf_get_exemption_type(msdu)) {
		case QDF_NBUF_EXEMPT_NO_EXEMPTION:
		case QDF_NBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
			/* We want to encrypt this frame */
			msdu_info.htt.action.do_encrypt = 1;
			break;
		case QDF_NBUF_EXEMPT_ALWAYS:
			/* We don't want to encrypt this frame */
			msdu_info.htt.action.do_encrypt = 0;
			break;
		default:
			msdu_info.htt.action.do_encrypt = 1;
			qdf_assert(0);
			break;
		}

		pkt_download_len = ((struct htt_pdev_t *)
				(pdev->htt_pdev))->download_len;
		tx_desc = ol_tx_prepare_ll_fast(pdev, vdev, msdu,
						&pkt_download_len, ep_id,
						&msdu_info);

		TXRX_STATS_MSDU_INCR(pdev, tx.from_stack, msdu);

		if (qdf_likely(tx_desc)) {
			DPTRACE(qdf_dp_trace_ptr(msdu,
				QDF_DP_TRACE_TXRX_FAST_PACKET_PTR_RECORD,
				QDF_TRACE_DEFAULT_PDEV_ID,
				qdf_nbuf_data_addr(msdu),
				sizeof(qdf_nbuf_data(msdu)), tx_desc->id,
				vdev->vdev_id));
			/*
			 * If debug display is enabled, show the meta-data being
			 * downloaded to the target via the HTT tx descriptor.
			 */
			htt_tx_desc_display(tx_desc->htt_tx_desc);
			/*
			 * The netbuf may get linked into a different list
			 * inside the ce_send_fast function, so store the next
			 * pointer before the ce_send call.
			 */
			next = qdf_nbuf_next(msdu);
			if ((ce_send_fast(pdev->ce_tx_hdl, msdu,
					  ep_id, pkt_download_len) == 0)) {
				/*
				 * The packet could not be sent
				 * Free the descriptor, return the packet to the
				 * caller
				 */
				ol_tx_desc_free(pdev, tx_desc);
				return msdu;
			}
			msdu = next;
		} else {
			TXRX_STATS_MSDU_LIST_INCR(
				pdev, tx.dropped.host_reject, msdu);
			return msdu; /* the list of unaccepted MSDUs */
		}
	}

	return NULL; /* all MSDUs were accepted */
}
#endif /* FEATURE_TSO */
