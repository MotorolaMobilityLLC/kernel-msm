/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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

/**
 * ol_tx_ll_wrapper() wrapper to ol_tx_ll
 *
 */
qdf_nbuf_t
ol_tx_ll_wrapper(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	return ol_tx_ll(vdev, msdu_list);
}

#if defined(FEATURE_TSO)
qdf_nbuf_t ol_tx_ll(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu = msdu_list;
	struct ol_txrx_msdu_info_t msdu_info;
	uint32_t tso_msdu_stats_idx = 0;

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
		struct ol_tx_desc_t *tx_desc = NULL;
		int segments = 1;

		msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
		msdu_info.peer = NULL;

		if (qdf_unlikely(ol_tx_prepare_tso(vdev, msdu, &msdu_info))) {
			qdf_print("ol_tx_prepare_tso failed\n");
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
		 * The netbuf may get linked into a different list inside the
		 * ol_tx_send function, so store the next pointer before the
		 * tx_send call.
		 */
		next = qdf_nbuf_next(msdu);
		/* init the current segment to the 1st segment in the list */
		while (segments) {
			if (msdu_info.tso_info.curr_seg)
				QDF_NBUF_CB_PADDR(msdu) =
					msdu_info.tso_info.curr_seg->
					seg.tso_frags[0].paddr;

			segments--;

			tx_desc = ol_tx_prepare_ll(vdev, msdu, &msdu_info);
			if (!tx_desc)
				return msdu;

			/*
			 * If this is a jumbo nbuf, then increment the number
			 * of nbuf users for each additional segment of the msdu
			 * This will ensure that the skb is freed only after
			 * receiving tx completion for all segments of an nbuf.
			 */
			if (segments)
				qdf_nbuf_inc_users(msdu);

			TXRX_STATS_MSDU_INCR(vdev->pdev, tx.from_stack, msdu);

			/*
			 * If debug display is enabled, show the meta-data being
			 * downloaded to the target via the HTT tx descriptor.
			 */
			htt_tx_desc_display(tx_desc->htt_tx_desc);

			ol_tx_send(vdev->pdev, tx_desc, msdu, vdev->vdev_id);

			if (msdu_info.tso_info.curr_seg) {
				msdu_info.tso_info.curr_seg =
					 msdu_info.tso_info.curr_seg->next;
			}

			if (msdu_info.tso_info.is_tso) {
				TXRX_STATS_TSO_INC_SEG(vdev->pdev,
						       tso_msdu_stats_idx);
				TXRX_STATS_TSO_INC_SEG_IDX(vdev->pdev,
							   tso_msdu_stats_idx);
			}
		} /* while segments */

		msdu = next;
	} /* while msdus */
	return NULL;            /* all MSDUs were accepted */
}
#else /* TSO */

qdf_nbuf_t ol_tx_ll(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu = msdu_list;
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
		struct ol_tx_desc_t *tx_desc = NULL;

		msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
		msdu_info.peer = NULL;
		tx_desc = ol_tx_prepare_ll(vdev, msdu, &msdu_info);
		if (!tx_desc)
			return msdu;

		TXRX_STATS_MSDU_INCR(vdev->pdev, tx.from_stack, msdu);

		/*
		 * If debug display is enabled, show the meta-data being
		 * downloaded to the target via the HTT tx descriptor.
		 */
		htt_tx_desc_display(tx_desc->htt_tx_desc);
		/*
		 * The netbuf may get linked into a different list inside the
		 * ol_tx_send function, so store the next pointer before the
		 * tx_send call.
		 */
		next = qdf_nbuf_next(msdu);
		ol_tx_send(vdev->pdev, tx_desc, msdu, vdev->vdev_id);
		msdu = next;
	}
	return NULL;            /* all MSDUs were accepted */
}
#endif /* TSO */
