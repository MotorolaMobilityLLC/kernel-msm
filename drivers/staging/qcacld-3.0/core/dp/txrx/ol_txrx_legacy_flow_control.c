/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <ol_txrx.h>            /* ol_txrx_get_vdev_from_vdev_id */

/* internal header files relevant only for HL systems */
#include <ol_tx_queue.h>        /* ol_tx_enqueue */

/* internal header files relevant only for specific systems (Pronto) */
#include <ol_txrx_encap.h>      /* OL_TX_ENCAP, etc */
#include <ol_tx.h>
#include <ol_cfg.h>
#include <cdp_txrx_handle.h>

void ol_txrx_vdev_pause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	/* TO DO: log the queue pause */
	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");

	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	vdev->ll_pause.paused_reason |= reason;
	vdev->ll_pause.q_pause_cnt++;
	vdev->ll_pause.is_q_paused = true;
	qdf_spin_unlock_bh(&vdev->ll_pause.mutex);

	TX_SCHED_DEBUG_PRINT("Leave");
}

void ol_txrx_vdev_unpause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			  uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	/* TO DO: log the queue unpause */
	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");

	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	if (vdev->ll_pause.paused_reason & reason) {
		vdev->ll_pause.paused_reason &= ~reason;
		if (!vdev->ll_pause.paused_reason) {
			vdev->ll_pause.is_q_paused = false;
			vdev->ll_pause.q_unpause_cnt++;
			qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
			ol_tx_vdev_ll_pause_queue_send((void *)vdev);
		} else {
			qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
		}
	} else {
		qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
	}
	TX_SCHED_DEBUG_PRINT("Leave");
}

void ol_txrx_vdev_flush(struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	qdf_timer_stop(&vdev->ll_pause.timer);
	vdev->ll_pause.is_q_timer_on = false;
	while (vdev->ll_pause.txq.head) {
		qdf_nbuf_t next =
			qdf_nbuf_next(vdev->ll_pause.txq.head);
		qdf_nbuf_set_next(vdev->ll_pause.txq.head, NULL);
		if (QDF_NBUF_CB_PADDR(vdev->ll_pause.txq.head)) {
			if (!qdf_nbuf_ipa_owned_get(vdev->ll_pause.txq.head))
				qdf_nbuf_unmap(vdev->pdev->osdev,
					       vdev->ll_pause.txq.head,
					       QDF_DMA_TO_DEVICE);
		}
		qdf_nbuf_tx_free(vdev->ll_pause.txq.head,
				 QDF_NBUF_PKT_ERROR);
		vdev->ll_pause.txq.head = next;
	}
	vdev->ll_pause.txq.tail = NULL;
	vdev->ll_pause.txq.depth = 0;
	qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
}

#define OL_TX_VDEV_PAUSE_QUEUE_SEND_MARGIN 400
#define OL_TX_VDEV_PAUSE_QUEUE_SEND_PERIOD_MS 5

static void ol_tx_vdev_ll_pause_queue_send_base(struct ol_txrx_vdev_t *vdev)
{
	int max_to_accept;

	if (!vdev)
		return;

	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	if (vdev->ll_pause.paused_reason) {
		qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
		return;
	}

	/*
	 * Send as much of the backlog as possible, but leave some margin
	 * of unallocated tx descriptors that can be used for new frames
	 * being transmitted by other vdevs.
	 * Ideally there would be a scheduler, which would not only leave
	 * some margin for new frames for other vdevs, but also would
	 * fairly apportion the tx descriptors between multiple vdevs that
	 * have backlogs in their pause queues.
	 * However, the fairness benefit of having a scheduler for frames
	 * from multiple vdev's pause queues is not sufficient to outweigh
	 * the extra complexity.
	 */
	max_to_accept = vdev->pdev->tx_desc.num_free -
		OL_TX_VDEV_PAUSE_QUEUE_SEND_MARGIN;
	while (max_to_accept > 0 && vdev->ll_pause.txq.depth) {
		qdf_nbuf_t tx_msdu;

		max_to_accept--;
		vdev->ll_pause.txq.depth--;
		tx_msdu = vdev->ll_pause.txq.head;
		if (tx_msdu) {
			vdev->ll_pause.txq.head = qdf_nbuf_next(tx_msdu);
			if (!vdev->ll_pause.txq.head)
				vdev->ll_pause.txq.tail = NULL;
			qdf_nbuf_set_next(tx_msdu, NULL);
			QDF_NBUF_UPDATE_TX_PKT_COUNT(tx_msdu,
						QDF_NBUF_TX_PKT_TXRX_DEQUEUE);
			tx_msdu = ol_tx_ll_wrapper(vdev, tx_msdu);
			/*
			 * It is unexpected that ol_tx_ll would reject the frame
			 * since we checked that there's room for it, though
			 * there's an infinitesimal possibility that between the
			 * time we checked the room available and now, a
			 * concurrent batch of tx frames used up all the room.
			 * For simplicity, just drop the frame.
			 */
			if (tx_msdu) {
				qdf_nbuf_unmap(vdev->pdev->osdev, tx_msdu,
					       QDF_DMA_TO_DEVICE);
				qdf_nbuf_tx_free(tx_msdu, QDF_NBUF_PKT_ERROR);
			}
		}
	}
	if (vdev->ll_pause.txq.depth) {
		qdf_timer_stop(&vdev->ll_pause.timer);
		if (!qdf_atomic_read(&vdev->delete.detaching)) {
			qdf_timer_start(&vdev->ll_pause.timer,
					OL_TX_VDEV_PAUSE_QUEUE_SEND_PERIOD_MS);
			vdev->ll_pause.is_q_timer_on = true;
		}
		if (vdev->ll_pause.txq.depth >= vdev->ll_pause.max_q_depth)
			vdev->ll_pause.q_overflow_cnt++;
	}

	qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
}

static qdf_nbuf_t
ol_tx_vdev_pause_queue_append(struct ol_txrx_vdev_t *vdev,
			      qdf_nbuf_t msdu_list, uint8_t start_timer)
{
	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	while (msdu_list &&
	       vdev->ll_pause.txq.depth < vdev->ll_pause.max_q_depth) {
		qdf_nbuf_t next = qdf_nbuf_next(msdu_list);

		QDF_NBUF_UPDATE_TX_PKT_COUNT(msdu_list,
					     QDF_NBUF_TX_PKT_TXRX_ENQUEUE);
		DPTRACE(qdf_dp_trace(msdu_list,
			QDF_DP_TRACE_TXRX_QUEUE_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID,
			qdf_nbuf_data_addr(msdu_list),
			sizeof(qdf_nbuf_data(msdu_list)), QDF_TX));

		vdev->ll_pause.txq.depth++;
		if (!vdev->ll_pause.txq.head) {
			vdev->ll_pause.txq.head = msdu_list;
			vdev->ll_pause.txq.tail = msdu_list;
		} else {
			qdf_nbuf_set_next(vdev->ll_pause.txq.tail, msdu_list);
		}
		vdev->ll_pause.txq.tail = msdu_list;

		msdu_list = next;
	}
	if (vdev->ll_pause.txq.tail)
		qdf_nbuf_set_next(vdev->ll_pause.txq.tail, NULL);

	if (start_timer) {
		qdf_timer_stop(&vdev->ll_pause.timer);
		if (!qdf_atomic_read(&vdev->delete.detaching)) {
			qdf_timer_start(&vdev->ll_pause.timer,
					OL_TX_VDEV_PAUSE_QUEUE_SEND_PERIOD_MS);
			vdev->ll_pause.is_q_timer_on = true;
		}
	}
	qdf_spin_unlock_bh(&vdev->ll_pause.mutex);

	return msdu_list;
}

/*
 * Store up the tx frame in the vdev's tx queue if the vdev is paused.
 * If there are too many frames in the tx queue, reject it.
 */
qdf_nbuf_t ol_tx_ll_queue(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	uint16_t eth_type;
	uint32_t paused_reason;

	if (!msdu_list)
		return NULL;

	paused_reason = vdev->ll_pause.paused_reason;
	if (paused_reason) {
		if (qdf_unlikely((paused_reason &
				  OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED) ==
				 paused_reason)) {
			eth_type = (((struct ethernet_hdr_t *)
				     qdf_nbuf_data(msdu_list))->
				    ethertype[0] << 8) |
				   (((struct ethernet_hdr_t *)
				     qdf_nbuf_data(msdu_list))->ethertype[1]);
			if (ETHERTYPE_IS_EAPOL_WAPI(eth_type)) {
				msdu_list = ol_tx_ll_wrapper(vdev, msdu_list);
				return msdu_list;
			}
		}
		msdu_list = ol_tx_vdev_pause_queue_append(vdev, msdu_list, 1);
	} else {
		if (vdev->ll_pause.txq.depth > 0 ||
		    vdev->pdev->tx_throttle.current_throttle_level !=
		    THROTTLE_LEVEL_0) {
			/*
			 * not paused, but there is a backlog of frms
			 * from a prior pause or throttle off phase
			 */
			msdu_list = ol_tx_vdev_pause_queue_append(
				vdev, msdu_list, 0);
			/*
			 * if throttle is disabled or phase is "on",
			 * send the frame
			 */
			if (vdev->pdev->tx_throttle.current_throttle_level ==
			    THROTTLE_LEVEL_0 ||
			    vdev->pdev->tx_throttle.current_throttle_phase ==
			    THROTTLE_PHASE_ON) {
				/*
				 * send as many frames as possible
				 * from the vdevs backlog
				 */
				ol_tx_vdev_ll_pause_queue_send_base(vdev);
			}
		} else {
			/*
			 * not paused, no throttle and no backlog -
			 * send the new frames
			 */
			msdu_list = ol_tx_ll_wrapper(vdev, msdu_list);
		}
	}
	return msdu_list;
}

/*
 * Run through the transmit queues for all the vdevs and
 * send the pending frames
 */
void ol_tx_pdev_ll_pause_queue_send_all(struct ol_txrx_pdev_t *pdev)
{
	int max_to_send;        /* tracks how many frames have been sent */
	qdf_nbuf_t tx_msdu;
	struct ol_txrx_vdev_t *vdev = NULL;
	uint8_t more;

	if (!pdev)
		return;

	if (pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_OFF)
		return;

	/* ensure that we send no more than tx_threshold frames at once */
	max_to_send = pdev->tx_throttle.tx_threshold;

	/* round robin through the vdev queues for the given pdev */

	/*
	 * Potential improvement: download several frames from the same vdev
	 * at a time, since it is more likely that those frames could be
	 * aggregated together, remember which vdev was serviced last,
	 * so the next call this function can resume the round-robin
	 * traversing where the current invocation left off
	 */
	do {
		more = 0;
		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			qdf_spin_lock_bh(&vdev->ll_pause.mutex);
			if (vdev->ll_pause.txq.depth) {
				if (vdev->ll_pause.paused_reason) {
					qdf_spin_unlock_bh(&vdev->ll_pause.
							   mutex);
					continue;
				}

				tx_msdu = vdev->ll_pause.txq.head;
				if (!tx_msdu) {
					qdf_spin_unlock_bh(&vdev->ll_pause.
							   mutex);
					continue;
				}

				max_to_send--;
				vdev->ll_pause.txq.depth--;

				vdev->ll_pause.txq.head =
					qdf_nbuf_next(tx_msdu);

				if (!vdev->ll_pause.txq.head)
					vdev->ll_pause.txq.tail = NULL;

				qdf_nbuf_set_next(tx_msdu, NULL);
				tx_msdu = ol_tx_ll_wrapper(vdev, tx_msdu);
				/*
				 * It is unexpected that ol_tx_ll would reject
				 * the frame, since we checked that there's
				 * room for it, though there's an infinitesimal
				 * possibility that between the time we checked
				 * the room available and now, a concurrent
				 * batch of tx frames used up all the room.
				 * For simplicity, just drop the frame.
				 */
				if (tx_msdu) {
					qdf_nbuf_unmap(pdev->osdev, tx_msdu,
						       QDF_DMA_TO_DEVICE);
					qdf_nbuf_tx_free(tx_msdu,
							 QDF_NBUF_PKT_ERROR);
				}
			}
			/*check if there are more msdus to transmit */
			if (vdev->ll_pause.txq.depth)
				more = 1;
			qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
		}
	} while (more && max_to_send);

	vdev = NULL;
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		qdf_spin_lock_bh(&vdev->ll_pause.mutex);
		if (vdev->ll_pause.txq.depth) {
			qdf_timer_stop(&pdev->tx_throttle.tx_timer);
			qdf_timer_start(
				&pdev->tx_throttle.tx_timer,
				OL_TX_VDEV_PAUSE_QUEUE_SEND_PERIOD_MS);
			qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
			return;
		}
		qdf_spin_unlock_bh(&vdev->ll_pause.mutex);
	}
}

void ol_tx_vdev_ll_pause_queue_send(void *context)
{
	struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)context;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	if (pdev &&
	    pdev->tx_throttle.current_throttle_level != THROTTLE_LEVEL_0 &&
	    pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_OFF)
		return;
	ol_tx_vdev_ll_pause_queue_send_base(vdev);
}

int ol_txrx_register_tx_flow_control(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id,
				     ol_txrx_tx_flow_control_fp flowControl,
				     void *osif_fc_ctx,
				     ol_txrx_tx_flow_control_is_pause_fp
				     flow_control_is_pause)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	qdf_spin_lock_bh(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = flowControl;
	vdev->osif_flow_control_is_pause = flow_control_is_pause;
	vdev->osif_fc_ctx = osif_fc_ctx;
	qdf_spin_unlock_bh(&vdev->flow_control_lock);
	return 0;
}

int ol_txrx_deregister_tx_flow_control_cb(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id", __func__);
		return -EINVAL;
	}

	qdf_spin_lock_bh(&vdev->flow_control_lock);
	vdev->osif_flow_control_cb = NULL;
	vdev->osif_flow_control_is_pause = NULL;
	vdev->osif_fc_ctx = NULL;
	qdf_spin_unlock_bh(&vdev->flow_control_lock);
	return 0;
}

/**
 * ol_txrx_get_tx_resource() - if tx resource less than low_watermark
 * soc_hdl: soc handle
 * @pdev_id: datapath pdev identifier
 * @peer_addr: peer mac address
 * @low_watermark: low watermark
 * @high_watermark_offset: high watermark offset value
 *
 * Return: true/false
 */
bool
ol_txrx_get_tx_resource(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			struct qdf_mac_addr peer_addr,
			unsigned int low_watermark,
			unsigned int high_watermark_offset)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev =
				ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	ol_txrx_vdev_handle vdev;

	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return true;
	}

	vdev = ol_txrx_get_vdev_by_peer_addr(ol_txrx_pdev_t_to_cdp_pdev(pdev),
					     peer_addr);
	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Invalid peer address: " QDF_MAC_ADDR_FMT,
			  __func__, QDF_MAC_ADDR_REF(peer_addr.bytes));
		/* Return true so caller do not understand that resource
		 * is less than low_watermark.
		 * sta_id validation will be done in ol_tx_send_data_frame
		 * and if sta_id is not registered then host will drop
		 * packet.
		 */
		return true;
	}

	qdf_spin_lock_bh(&vdev->pdev->tx_mutex);

	if (vdev->pdev->tx_desc.num_free < (uint16_t)low_watermark) {
		vdev->tx_fl_lwm = (uint16_t)low_watermark;
		vdev->tx_fl_hwm =
			(uint16_t)(low_watermark + high_watermark_offset);
		/* Not enough free resource, stop TX OS Q */
		qdf_atomic_set(&vdev->os_q_paused, 1);
		qdf_spin_unlock_bh(&vdev->pdev->tx_mutex);
		return false;
	}
	qdf_spin_unlock_bh(&vdev->pdev->tx_mutex);
	return true;
}

int ol_txrx_ll_set_tx_pause_q_depth(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				    int pause_q_depth)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	qdf_spin_lock_bh(&vdev->ll_pause.mutex);
	vdev->ll_pause.max_q_depth = pause_q_depth;
	qdf_spin_unlock_bh(&vdev->ll_pause.mutex);

	return 0;
}

void ol_txrx_flow_control_cb(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			     bool tx_resume)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&vdev->flow_control_lock);
	if ((vdev->osif_flow_control_cb) && (vdev->osif_fc_ctx))
		vdev->osif_flow_control_cb(vdev->osif_fc_ctx, tx_resume);
	qdf_spin_unlock_bh(&vdev->flow_control_lock);
}

/**
 * ol_txrx_flow_control_is_pause() - is osif paused by flow control
 * @vdev: vdev handle
 *
 * Return: true if osif is paused by flow control
 */
static bool ol_txrx_flow_control_is_pause(ol_txrx_vdev_handle vdev)
{
	bool is_pause = false;

	if ((vdev->osif_flow_control_is_pause) && (vdev->osif_fc_ctx))
		is_pause = vdev->osif_flow_control_is_pause(vdev->osif_fc_ctx);

	return is_pause;
}

/**
 * ol_tx_flow_ct_unpause_os_q() - Unpause OS Q
 * @pdev: physical device object
 *
 *
 * Return: None
 */
void ol_tx_flow_ct_unpause_os_q(ol_txrx_pdev_handle pdev)
{
	struct ol_txrx_vdev_t *vdev;
	struct cdp_soc_t *soc_hdl = ol_txrx_soc_t_to_cdp_soc_t(pdev->soc);

	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		if ((qdf_atomic_read(&vdev->os_q_paused) &&
		     (vdev->tx_fl_hwm != 0)) ||
		     ol_txrx_flow_control_is_pause(vdev)) {
			qdf_spin_lock(&pdev->tx_mutex);
			if (pdev->tx_desc.num_free > vdev->tx_fl_hwm) {
				qdf_atomic_set(&vdev->os_q_paused, 0);
				qdf_spin_unlock(&pdev->tx_mutex);
				ol_txrx_flow_control_cb(soc_hdl,
							vdev->vdev_id, true);
			} else {
				qdf_spin_unlock(&pdev->tx_mutex);
			}
		}
	}
}

