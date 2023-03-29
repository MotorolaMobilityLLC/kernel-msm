/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

#include <qdf_atomic.h>         /* qdf_atomic_inc, etc. */
#include <qdf_lock.h>           /* qdf_os_spinlock */
#include <qdf_time.h>           /* qdf_system_ticks, etc. */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <qdf_net_types.h>      /* QDF_NBUF_TX_EXT_TID_INVALID */

#include "queue.h"          /* TAILQ */
#ifdef QCA_COMPUTE_TX_DELAY
#include <enet.h>               /* ethernet_hdr_t, etc. */
#include <ipv6_defs.h>          /* ipv6_traffic_class */
#endif

#include <ol_txrx_api.h>        /* ol_txrx_vdev_handle, etc. */
#include <ol_htt_tx_api.h>      /* htt_tx_compl_desc_id */
#include <ol_txrx_htt_api.h>    /* htt_tx_status */

#include <ol_ctrl_txrx_api.h>
#include <cdp_txrx_tx_delay.h>
#include <ol_txrx_types.h>      /* ol_txrx_vdev_t, etc */
#include <ol_tx_desc.h>         /* ol_tx_desc_find, ol_tx_desc_frame_free */
#ifdef QCA_COMPUTE_TX_DELAY
#include <ol_tx_classify.h>     /* ol_tx_dest_addr_find */
#endif
#include <ol_txrx_internal.h>   /* OL_TX_DESC_NO_REFS, etc. */
#include <ol_osif_txrx_api.h>
#include <ol_tx.h>              /* ol_tx_reinject */
#include <ol_tx_send.h>

#include <ol_cfg.h>             /* ol_cfg_is_high_latency */
#include <ol_tx_sched.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>      /* OL_TX_RESTORE_HDR, etc */
#endif
#include <ol_tx_queue.h>
#include <ol_txrx.h>
#include <pktlog_ac_fmt.h>
#include <cdp_txrx_handle.h>

void ol_tx_init_pdev(ol_txrx_pdev_handle pdev)
{
	qdf_atomic_add(ol_cfg_target_tx_credit(pdev->ctrl_pdev),
		       &pdev->target_tx_credit);
}

qdf_nbuf_t ol_tx_reinject(struct ol_txrx_vdev_t *vdev,
			  qdf_nbuf_t msdu, uint16_t peer_id)
{
	struct ol_tx_desc_t *tx_desc = NULL;
	struct ol_txrx_msdu_info_t msdu_info;

	msdu_info.htt.info.l2_hdr_type = vdev->pdev->htt_pkt_type;
	msdu_info.htt.info.ext_tid = HTT_TX_EXT_TID_INVALID;
	msdu_info.peer = NULL;
	msdu_info.htt.action.tx_comp_req = 0;
	msdu_info.tso_info.is_tso = 0;

	tx_desc = ol_tx_prepare_ll(vdev, msdu, &msdu_info);
	if (!tx_desc)
		return msdu;

	HTT_TX_DESC_POSTPONED_SET(*((uint32_t *)(tx_desc->htt_tx_desc)), true);

	htt_tx_desc_set_peer_id(tx_desc->htt_tx_desc, peer_id);

	ol_tx_send(vdev->pdev, tx_desc, msdu, vdev->vdev_id);

	return NULL;
}

/*
 * The TXRX module doesn't accept tx frames unless the target has
 * enough descriptors for them.
 * For LL, the TXRX descriptor pool is sized to match the target's
 * descriptor pool.  Hence, if the descriptor allocation in TXRX
 * succeeds, that guarantees that the target has room to accept
 * the new tx frame.
 */
struct ol_tx_desc_t *
ol_tx_prepare_ll(ol_txrx_vdev_handle vdev,
		 qdf_nbuf_t msdu,
		 struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	(msdu_info)->htt.info.frame_type = pdev->htt_pkt_type;
	tx_desc = ol_tx_desc_ll(pdev, vdev, msdu, msdu_info);
	if (qdf_unlikely(!tx_desc)) {
		/*
		 * If TSO packet, free associated
		 * remaining TSO segment descriptors
		 */
		if (qdf_nbuf_is_tso(msdu))
			ol_free_remaining_tso_segs(
					vdev, msdu_info, true);
		TXRX_STATS_MSDU_LIST_INCR(
				pdev, tx.dropped.host_reject, msdu);
		return NULL;
	}

	return tx_desc;
}

qdf_nbuf_t
ol_tx_non_std_ll(struct ol_txrx_vdev_t *vdev,
		 enum ol_tx_spec tx_spec,
		 qdf_nbuf_t msdu_list)
{
	qdf_nbuf_t msdu = msdu_list;
	htt_pdev_handle htt_pdev = vdev->pdev->htt_pdev;
	struct ol_txrx_msdu_info_t msdu_info;

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

		msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
		msdu_info.peer = NULL;
		msdu_info.tso_info.is_tso = 0;

		tx_desc = ol_tx_prepare_ll(vdev, msdu, &msdu_info);
		if (!tx_desc)
			return msdu;

		/*
		 * The netbuf may get linked into a different list inside the
		 * ol_tx_send function, so store the next pointer before the
		 * tx_send call.
		 */
		next = qdf_nbuf_next(msdu);

		if (tx_spec != OL_TX_SPEC_STD) {
			if (tx_spec & OL_TX_SPEC_NO_FREE) {
				tx_desc->pkt_type = OL_TX_FRM_NO_FREE;
			} else if (tx_spec & OL_TX_SPEC_TSO) {
				tx_desc->pkt_type = OL_TX_FRM_TSO;
			} else if (tx_spec & OL_TX_SPEC_NWIFI_NO_ENCRYPT) {
				uint8_t sub_type =
					ol_txrx_tx_raw_subtype(tx_spec);
				htt_tx_desc_type(htt_pdev, tx_desc->htt_tx_desc,
						 htt_pkt_type_native_wifi,
						 sub_type);
			} else if (ol_txrx_tx_is_raw(tx_spec)) {
				/* different types of raw frames */
				uint8_t sub_type =
					ol_txrx_tx_raw_subtype(tx_spec);
				htt_tx_desc_type(htt_pdev, tx_desc->htt_tx_desc,
						 htt_pkt_type_raw, sub_type);
			}
		}
		/*
		 * If debug display is enabled, show the meta-data being
		 * downloaded to the target via the HTT tx descriptor.
		 */
		htt_tx_desc_display(tx_desc->htt_tx_desc);
		ol_tx_send(vdev->pdev, tx_desc, msdu, vdev->vdev_id);
		msdu = next;
	}
	return NULL;            /* all MSDUs were accepted */
}

#if defined(HELIUMPLUS)
void ol_txrx_dump_frag_desc(char *msg, struct ol_tx_desc_t *tx_desc)
{
	uint32_t                *frag_ptr_i_p;
	int                     i;

	ol_txrx_err("OL TX Descriptor 0x%pK msdu_id %d\n",
		    tx_desc, tx_desc->id);
	ol_txrx_err("HTT TX Descriptor vaddr: 0x%pK paddr: %pad",
		    tx_desc->htt_tx_desc, &tx_desc->htt_tx_desc_paddr);
	ol_txrx_err("Fragment Descriptor 0x%pK (paddr=%pad)",
		    tx_desc->htt_frag_desc, &tx_desc->htt_frag_desc_paddr);

	/*
	 * it looks from htt_tx_desc_frag() that tx_desc->htt_frag_desc
	 * is already de-referrable (=> in virtual address space)
	 */
	frag_ptr_i_p = tx_desc->htt_frag_desc;

	/* Dump 6 words of TSO flags */
	print_hex_dump(KERN_DEBUG, "MLE Desc:TSO Flags:  ",
		       DUMP_PREFIX_NONE, 8, 4,
		       frag_ptr_i_p, 24, true);

	frag_ptr_i_p += 6; /* Skip 6 words of TSO flags */

	i = 0;
	while (*frag_ptr_i_p) {
		print_hex_dump(KERN_DEBUG, "MLE Desc:Frag Ptr:  ",
			       DUMP_PREFIX_NONE, 8, 4,
			       frag_ptr_i_p, 8, true);
		i++;
		if (i > 5) /* max 6 times: frag_ptr0 to frag_ptr5 */
			break;
		/* jump to next  pointer - skip length */
		frag_ptr_i_p += 2;
	}
}
#endif /* HELIUMPLUS */

struct ol_tx_desc_t *
ol_txrx_mgmt_tx_desc_alloc(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t tx_mgmt_frm,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	struct ol_tx_desc_t *tx_desc;

	/* For LL tx_comp_req is not used so initialized to 0 */
	tx_msdu_info->htt.action.tx_comp_req = 0;
	tx_desc = ol_tx_desc_ll(pdev, vdev, tx_mgmt_frm, tx_msdu_info);
	/* FIX THIS -
	 * The FW currently has trouble using the host's fragments table
	 * for management frames.  Until this is fixed, rather than
	 * specifying the fragment table to the FW, specify just the
	 * address of the initial fragment.
	 */
#if defined(HELIUMPLUS)
	/* ol_txrx_dump_frag_desc("ol_txrx_mgmt_send(): after ol_tx_desc_ll",
	 *			  tx_desc);
	 */
#endif /* defined(HELIUMPLUS) */
	if (tx_desc) {
		/*
		 * Following the call to ol_tx_desc_ll, frag 0 is the
		 * HTT tx HW descriptor, and the frame payload is in
		 * frag 1.
		 */
		htt_tx_desc_frags_table_set(
				pdev->htt_pdev,
				tx_desc->htt_tx_desc,
				qdf_nbuf_get_frag_paddr(tx_mgmt_frm, 1),
				0, 0);
#if defined(HELIUMPLUS) && defined(HELIUMPLUS_DEBUG)
		ol_txrx_dump_frag_desc(
				"after htt_tx_desc_frags_table_set",
				tx_desc);
#endif /* defined(HELIUMPLUS) */
	}

	return tx_desc;
}

int ol_txrx_mgmt_send_frame(
	struct ol_txrx_vdev_t *vdev,
	struct ol_tx_desc_t *tx_desc,
	qdf_nbuf_t tx_mgmt_frm,
	struct ol_txrx_msdu_info_t *tx_msdu_info,
	uint16_t chanfreq)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	htt_tx_desc_set_chanfreq(tx_desc->htt_tx_desc, chanfreq);
	QDF_NBUF_CB_TX_PACKET_TRACK(tx_desc->netbuf) =
					QDF_NBUF_TX_PKT_MGMT_TRACK;
	ol_tx_send_nonstd(pdev, tx_desc, tx_mgmt_frm,
			  htt_pkt_type_mgmt);

	return 0;
}

#if defined(FEATURE_TSO)
void ol_free_remaining_tso_segs(ol_txrx_vdev_handle vdev,
				struct ol_txrx_msdu_info_t *msdu_info,
				bool is_tso_seg_mapping_done)
{
	struct qdf_tso_seg_elem_t *next_seg;
	struct qdf_tso_seg_elem_t *free_seg = msdu_info->tso_info.curr_seg;
	struct ol_txrx_pdev_t *pdev;
	bool is_last_seg = false;

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is null");
		return;
	}

	pdev = vdev->pdev;
	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is null");
		return;
	}

	/*
	 * TSO segment are mapped already, therefore,
	 * 1. unmap the tso segments,
	 * 2. free tso num segment if it is a last segment, and
	 * 3. free the tso segments.
	 */

	if (is_tso_seg_mapping_done) {
		struct qdf_tso_num_seg_elem_t *tso_num_desc =
				msdu_info->tso_info.tso_num_seg_list;

		if (qdf_unlikely(!tso_num_desc)) {
			ol_txrx_err("TSO common info is NULL!");
			return;
		}

		while (free_seg) {
			qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);
			tso_num_desc->num_seg.tso_cmn_num_seg--;

			is_last_seg = (tso_num_desc->num_seg.tso_cmn_num_seg ==
				       0) ? true : false;
			qdf_nbuf_unmap_tso_segment(pdev->osdev, free_seg,
						   is_last_seg);
			qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);

			if (is_last_seg) {
				ol_tso_num_seg_free(pdev,
						    msdu_info->tso_info.
						    tso_num_seg_list);
				msdu_info->tso_info.tso_num_seg_list = NULL;
			}

			next_seg = free_seg->next;
			free_seg->force_free = 1;
			ol_tso_free_segment(pdev, free_seg);
			free_seg = next_seg;
		}
	} else {
		/*
		 * TSO segment are not mapped therefore,
		 * free the tso segments only.
		 */
		while (free_seg) {
			next_seg = free_seg->next;
			free_seg->force_free = 1;
			ol_tso_free_segment(pdev, free_seg);
			free_seg = next_seg;
		}
	}
}

/**
 * ol_tx_prepare_tso() - Given a jumbo msdu, prepare the TSO
 * related information in the msdu_info meta data
 * @vdev: virtual device handle
 * @msdu: network buffer
 * @msdu_info: meta data associated with the msdu
 *
 * Return: 0 - success, >0 - error
 */
uint8_t ol_tx_prepare_tso(ol_txrx_vdev_handle vdev,
			  qdf_nbuf_t msdu,
			  struct ol_txrx_msdu_info_t *msdu_info)
{
	msdu_info->tso_info.curr_seg = NULL;
	if (qdf_nbuf_is_tso(msdu)) {
		int num_seg = qdf_nbuf_get_tso_num_seg(msdu);
		struct qdf_tso_num_seg_elem_t *tso_num_seg;

		msdu_info->tso_info.tso_num_seg_list = NULL;
		msdu_info->tso_info.tso_seg_list = NULL;
		msdu_info->tso_info.num_segs = num_seg;
		while (num_seg) {
			struct qdf_tso_seg_elem_t *tso_seg =
				ol_tso_alloc_segment(vdev->pdev);
			if (tso_seg) {
				qdf_tso_seg_dbg_record(tso_seg,
						       TSOSEG_LOC_PREPARETSO);
				tso_seg->next =
					msdu_info->tso_info.tso_seg_list;
				msdu_info->tso_info.tso_seg_list
					= tso_seg;
				num_seg--;
			} else {
				/* Free above alocated TSO segements till now */
				msdu_info->tso_info.curr_seg =
					msdu_info->tso_info.tso_seg_list;
				ol_free_remaining_tso_segs(vdev, msdu_info,
							   false);
				return 1;
			}
		}
		tso_num_seg = ol_tso_num_seg_alloc(vdev->pdev);
		if (tso_num_seg) {
			tso_num_seg->next = msdu_info->tso_info.
						tso_num_seg_list;
			msdu_info->tso_info.tso_num_seg_list = tso_num_seg;
		} else {
			/* Free the already allocated num of segments */
			msdu_info->tso_info.curr_seg =
				msdu_info->tso_info.tso_seg_list;
			ol_free_remaining_tso_segs(vdev, msdu_info, false);
			return 1;
		}

		if (qdf_unlikely(!qdf_nbuf_get_tso_info(vdev->pdev->osdev,
						msdu, &msdu_info->tso_info))) {
			/* Free the already allocated num of segments */
			msdu_info->tso_info.curr_seg =
				msdu_info->tso_info.tso_seg_list;
			ol_free_remaining_tso_segs(vdev, msdu_info, false);
			return 1;
		}

		msdu_info->tso_info.curr_seg =
			msdu_info->tso_info.tso_seg_list;
		num_seg = msdu_info->tso_info.num_segs;
	} else {
		msdu_info->tso_info.is_tso = 0;
		msdu_info->tso_info.num_segs = 1;
	}
	return 0;
}

/**
 * ol_tx_tso_update_stats() - update TSO stats
 * @pdev: pointer to ol_txrx_pdev_t structure
 * @msdu_info: tso msdu_info for the msdu
 * @msdu: tso mdsu for which stats are updated
 * @tso_msdu_idx: stats index in the global TSO stats array where stats will be
 *                updated
 *
 * Return: None
 */
void ol_tx_tso_update_stats(struct ol_txrx_pdev_t *pdev,
			    struct qdf_tso_info_t  *tso_info, qdf_nbuf_t msdu,
			    uint32_t tso_msdu_idx)
{
	TXRX_STATS_TSO_HISTOGRAM(pdev, tso_info->num_segs);
	TXRX_STATS_TSO_GSO_SIZE_UPDATE(pdev, tso_msdu_idx,
				       qdf_nbuf_tcp_tso_size(msdu));
	TXRX_STATS_TSO_TOTAL_LEN_UPDATE(pdev,
					tso_msdu_idx, qdf_nbuf_len(msdu));
	TXRX_STATS_TSO_NUM_FRAGS_UPDATE(pdev, tso_msdu_idx,
					qdf_nbuf_get_nr_frags(msdu));
}

/**
 * ol_tx_tso_get_stats_idx() - retrieve global TSO stats index and increment it
 * @pdev: pointer to ol_txrx_pdev_t structure
 *
 * Retrieve  the current value of the global variable and increment it. This is
 * done in a spinlock as the global TSO stats may be accessed in parallel by
 * multiple TX streams.
 *
 * Return: The current value of TSO stats index.
 */
uint32_t ol_tx_tso_get_stats_idx(struct ol_txrx_pdev_t *pdev)
{
	uint32_t msdu_stats_idx = 0;

	qdf_spin_lock_bh(&pdev->stats.pub.tx.tso.tso_stats_lock);
	msdu_stats_idx = pdev->stats.pub.tx.tso.tso_info.tso_msdu_idx;
	pdev->stats.pub.tx.tso.tso_info.tso_msdu_idx++;
	pdev->stats.pub.tx.tso.tso_info.tso_msdu_idx &=
					NUM_MAX_TSO_MSDUS_MASK;
	qdf_spin_unlock_bh(&pdev->stats.pub.tx.tso.tso_stats_lock);

	TXRX_STATS_TSO_RESET_MSDU(pdev, msdu_stats_idx);

	return msdu_stats_idx;
}

/**
 * ol_tso_seg_list_init() - function to initialise the tso seg freelist
 * @pdev: the data physical device sending the data
 * @num_seg: number of segments needs to be intialised
 *
 * Return: none
 */
void ol_tso_seg_list_init(struct ol_txrx_pdev_t *pdev, uint32_t num_seg)
{
	int i = 0;
	struct qdf_tso_seg_elem_t *c_element;

	/* Host should not allocate any c_element. */
	if (num_seg <= 0) {
		ol_txrx_err("Pool size passed is 0");
		QDF_BUG(0);
		pdev->tso_seg_pool.pool_size = i;
		qdf_spinlock_create(&pdev->tso_seg_pool.tso_mutex);
		return;
	}

	c_element = qdf_mem_malloc(sizeof(struct qdf_tso_seg_elem_t));
	pdev->tso_seg_pool.freelist = c_element;
	for (i = 0; i < (num_seg - 1); i++) {
		if (qdf_unlikely(!c_element)) {
			ol_txrx_err("c_element NULL for seg %d", i);
			QDF_BUG(0);
			pdev->tso_seg_pool.pool_size = i;
			pdev->tso_seg_pool.num_free = i;
			qdf_spinlock_create(&pdev->tso_seg_pool.tso_mutex);
			return;
		}
		/* set the freelist bit and magic cookie*/
		c_element->on_freelist = 1;
		c_element->cookie = TSO_SEG_MAGIC_COOKIE;
#ifdef TSOSEG_DEBUG
		c_element->dbg.txdesc = NULL;
		qdf_atomic_init(&c_element->dbg.cur); /* history empty */
		qdf_tso_seg_dbg_record(c_element, TSOSEG_LOC_INIT1);
#endif /* TSOSEG_DEBUG */
		c_element->next =
			qdf_mem_malloc(sizeof(struct qdf_tso_seg_elem_t));
		c_element = c_element->next;
	}
	/*
	 * NULL check for the last c_element of the list or
	 * first c_element if num_seg is equal to 1.
	 */
	if (qdf_unlikely(!c_element)) {
		ol_txrx_err("c_element NULL for seg %d", i);
		QDF_BUG(0);
		pdev->tso_seg_pool.pool_size = i;
		pdev->tso_seg_pool.num_free = i;
		qdf_spinlock_create(&pdev->tso_seg_pool.tso_mutex);
		return;
	}
	c_element->on_freelist = 1;
	c_element->cookie = TSO_SEG_MAGIC_COOKIE;
#ifdef TSOSEG_DEBUG
	qdf_tso_seg_dbg_init(c_element);
	qdf_tso_seg_dbg_record(c_element, TSOSEG_LOC_INIT2);
#endif /* TSOSEG_DEBUG */
	c_element->next = NULL;
	pdev->tso_seg_pool.pool_size = num_seg;
	pdev->tso_seg_pool.num_free = num_seg;
	qdf_spinlock_create(&pdev->tso_seg_pool.tso_mutex);
}

/**
 * ol_tso_seg_list_deinit() - function to de-initialise the tso seg freelist
 * @pdev: the data physical device sending the data
 *
 * Return: none
 */
void ol_tso_seg_list_deinit(struct ol_txrx_pdev_t *pdev)
{
	int i;
	struct qdf_tso_seg_elem_t *c_element;
	struct qdf_tso_seg_elem_t *temp;

	/* pool size 0 implies that tso seg list is not initialised*/
	if (!pdev->tso_seg_pool.freelist &&
	    pdev->tso_seg_pool.pool_size == 0)
		return;

	qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);
	c_element = pdev->tso_seg_pool.freelist;
	i = pdev->tso_seg_pool.pool_size;

	pdev->tso_seg_pool.freelist = NULL;
	pdev->tso_seg_pool.num_free = 0;
	pdev->tso_seg_pool.pool_size = 0;

	qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
	qdf_spinlock_destroy(&pdev->tso_seg_pool.tso_mutex);

	while (i-- > 0 && c_element) {
		temp = c_element->next;
		if (c_element->on_freelist != 1) {
			qdf_tso_seg_dbg_bug("seg already freed (double?)");
			return;
		} else if (c_element->cookie != TSO_SEG_MAGIC_COOKIE) {
			qdf_tso_seg_dbg_bug("seg cookie is bad (corruption?)");
			return;
		}
		/* free this seg, so reset the cookie value*/
		c_element->cookie = 0;
		qdf_mem_free(c_element);
		c_element = temp;
	}
}

/**
 * ol_tso_num_seg_list_init() - function to initialise the freelist of elements
 *				use to count the num of tso segments in jumbo
 *				skb packet freelist
 * @pdev: the data physical device sending the data
 * @num_seg: number of elements needs to be intialised
 *
 * Return: none
 */
void ol_tso_num_seg_list_init(struct ol_txrx_pdev_t *pdev, uint32_t num_seg)
{
	int i = 0;
	struct qdf_tso_num_seg_elem_t *c_element;

	/* Host should not allocate any c_element. */
	if (num_seg <= 0) {
		ol_txrx_err("Pool size passed is 0");
		QDF_BUG(0);
		pdev->tso_num_seg_pool.num_seg_pool_size = i;
		qdf_spinlock_create(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
		return;
	}

	c_element = qdf_mem_malloc(sizeof(struct qdf_tso_num_seg_elem_t));
	pdev->tso_num_seg_pool.freelist = c_element;
	for (i = 0; i < (num_seg - 1); i++) {
		if (qdf_unlikely(!c_element)) {
			ol_txrx_err("c_element NULL for num of seg %d", i);
			QDF_BUG(0);
			pdev->tso_num_seg_pool.num_seg_pool_size = i;
			pdev->tso_num_seg_pool.num_free = i;
			qdf_spinlock_create(&pdev->tso_num_seg_pool.
							tso_num_seg_mutex);
			return;
		}
		c_element->next =
			qdf_mem_malloc(sizeof(struct qdf_tso_num_seg_elem_t));
		c_element = c_element->next;
	}
	/*
	 * NULL check for the last c_element of the list or
	 * first c_element if num_seg is equal to 1.
	 */
	if (qdf_unlikely(!c_element)) {
		ol_txrx_err("c_element NULL for num of seg %d", i);
		QDF_BUG(0);
		pdev->tso_num_seg_pool.num_seg_pool_size = i;
		pdev->tso_num_seg_pool.num_free = i;
		qdf_spinlock_create(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
		return;
	}
	c_element->next = NULL;
	pdev->tso_num_seg_pool.num_seg_pool_size = num_seg;
	pdev->tso_num_seg_pool.num_free = num_seg;
	qdf_spinlock_create(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
}

/**
 * ol_tso_num_seg_list_deinit() - function to de-initialise the freelist of
 *				  elements use to count the num of tso segment
 *				  in a jumbo skb packet freelist
 * @pdev: the data physical device sending the data
 *
 * Return: none
 */
void ol_tso_num_seg_list_deinit(struct ol_txrx_pdev_t *pdev)
{
	int i;
	struct qdf_tso_num_seg_elem_t *c_element;
	struct qdf_tso_num_seg_elem_t *temp;

	/* pool size 0 implies that tso num seg list is not initialised*/
	if (!pdev->tso_num_seg_pool.freelist &&
	    pdev->tso_num_seg_pool.num_seg_pool_size == 0)
		return;

	qdf_spin_lock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
	c_element = pdev->tso_num_seg_pool.freelist;
	i = pdev->tso_num_seg_pool.num_seg_pool_size;

	pdev->tso_num_seg_pool.freelist = NULL;
	pdev->tso_num_seg_pool.num_free = 0;
	pdev->tso_num_seg_pool.num_seg_pool_size = 0;

	qdf_spin_unlock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
	qdf_spinlock_destroy(&pdev->tso_num_seg_pool.tso_num_seg_mutex);

	while (i-- > 0 && c_element) {
		temp = c_element->next;
		qdf_mem_free(c_element);
		c_element = temp;
	}
}
#endif /* FEATURE_TSO */

#if defined(FEATURE_TSO) && defined(FEATURE_TSO_DEBUG)
void ol_txrx_tso_stats_init(ol_txrx_pdev_handle pdev)
{
	qdf_spinlock_create(&pdev->stats.pub.tx.tso.tso_stats_lock);
}

void ol_txrx_tso_stats_deinit(ol_txrx_pdev_handle pdev)
{
	qdf_spinlock_destroy(&pdev->stats.pub.tx.tso.tso_stats_lock);
}

void ol_txrx_stats_display_tso(ol_txrx_pdev_handle pdev)
{
	int msdu_idx;
	int seg_idx;

	txrx_nofl_info("TSO Statistics:");
	txrx_nofl_info("TSO pkts %lld, bytes %lld\n",
		       pdev->stats.pub.tx.tso.tso_pkts.pkts,
		       pdev->stats.pub.tx.tso.tso_pkts.bytes);

	txrx_nofl_info("TSO Histogram for numbers of segments:\n"
		       "Single segment	%d\n"
		       "  2-5 segments	%d\n"
		       " 6-10 segments	%d\n"
		       "11-15 segments	%d\n"
		       "16-20 segments	%d\n"
		       "  20+ segments	%d\n",
		       pdev->stats.pub.tx.tso.tso_hist.pkts_1,
		       pdev->stats.pub.tx.tso.tso_hist.pkts_2_5,
		       pdev->stats.pub.tx.tso.tso_hist.pkts_6_10,
		       pdev->stats.pub.tx.tso.tso_hist.pkts_11_15,
		       pdev->stats.pub.tx.tso.tso_hist.pkts_16_20,
		       pdev->stats.pub.tx.tso.tso_hist.pkts_20_plus);

	txrx_nofl_info("TSO History Buffer: Total size %d, current_index %d",
		       NUM_MAX_TSO_MSDUS,
		       TXRX_STATS_TSO_MSDU_IDX(pdev));

	for (msdu_idx = 0; msdu_idx < NUM_MAX_TSO_MSDUS; msdu_idx++) {
		if (TXRX_STATS_TSO_MSDU_TOTAL_LEN(pdev, msdu_idx) == 0)
			continue;
		txrx_nofl_info("jumbo pkt idx: %d num segs %d gso_len %d total_len %d nr_frags %d",
			       msdu_idx,
			       TXRX_STATS_TSO_MSDU_NUM_SEG(pdev, msdu_idx),
			       TXRX_STATS_TSO_MSDU_GSO_SIZE(pdev, msdu_idx),
			       TXRX_STATS_TSO_MSDU_TOTAL_LEN(pdev, msdu_idx),
			       TXRX_STATS_TSO_MSDU_NR_FRAGS(pdev, msdu_idx));

		for (seg_idx = 0;
			 ((seg_idx < TXRX_STATS_TSO_MSDU_NUM_SEG(pdev,
			   msdu_idx)) && (seg_idx < NUM_MAX_TSO_SEGS));
			 seg_idx++) {
			struct qdf_tso_seg_t tso_seg =
				 TXRX_STATS_TSO_SEG(pdev, msdu_idx, seg_idx);

			txrx_nofl_info("seg idx: %d", seg_idx);
			txrx_nofl_info("tso_enable: %d",
				       tso_seg.tso_flags.tso_enable);
			txrx_nofl_info("fin %d syn %d rst %d psh %d ack %d urg %d ece %d cwr %d ns %d",
				       tso_seg.tso_flags.fin,
				       tso_seg.tso_flags.syn,
				       tso_seg.tso_flags.rst,
				       tso_seg.tso_flags.psh,
				       tso_seg.tso_flags.ack,
				       tso_seg.tso_flags.urg,
				       tso_seg.tso_flags.ece,
				       tso_seg.tso_flags.cwr,
				       tso_seg.tso_flags.ns);
			txrx_nofl_info("tcp_seq_num: 0x%x ip_id: %d",
				       tso_seg.tso_flags.tcp_seq_num,
				       tso_seg.tso_flags.ip_id);
		}
	}
}

void ol_txrx_tso_stats_clear(ol_txrx_pdev_handle pdev)
{
	qdf_mem_zero(&pdev->stats.pub.tx.tso.tso_pkts,
		     sizeof(struct ol_txrx_stats_elem));
#if defined(FEATURE_TSO)
	qdf_mem_zero(&pdev->stats.pub.tx.tso.tso_info,
		     sizeof(struct ol_txrx_stats_tso_info));
	qdf_mem_zero(&pdev->stats.pub.tx.tso.tso_hist,
		     sizeof(struct ol_txrx_tso_histogram));
#endif
}
#endif /* defined(FEATURE_TSO) && defined(FEATURE_TSO_DEBUG) */
