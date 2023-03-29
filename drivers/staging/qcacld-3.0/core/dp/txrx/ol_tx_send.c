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
#include <wlan_pkt_capture_ucfg_api.h>
#ifdef TX_CREDIT_RECLAIM_SUPPORT

#define OL_TX_CREDIT_RECLAIM(pdev)					\
	do {								\
		if (qdf_atomic_read(&pdev->target_tx_credit)  <		\
		    ol_cfg_tx_credit_lwm(pdev->ctrl_pdev)) {		\
			ol_osif_ath_tasklet(pdev->osdev);		\
		}							\
	} while (0)

#else

#define OL_TX_CREDIT_RECLAIM(pdev)

#endif /* TX_CREDIT_RECLAIM_SUPPORT */

#if defined(CONFIG_HL_SUPPORT) || defined(TX_CREDIT_RECLAIM_SUPPORT)

/*
 * HL needs to keep track of the amount of credit available to download
 * tx frames to the target - the download scheduler decides when to
 * download frames, and which frames to download, based on the credit
 * availability.
 * LL systems that use TX_CREDIT_RECLAIM_SUPPORT also need to keep track
 * of the target_tx_credit, to determine when to poll for tx completion
 * messages.
 */
static inline void
ol_tx_target_credit_decr_int(struct ol_txrx_pdev_t *pdev, int delta)
{
	qdf_atomic_add(-1 * delta, &pdev->target_tx_credit);
}

static inline void
ol_tx_target_credit_incr_int(struct ol_txrx_pdev_t *pdev, int delta)
{
	qdf_atomic_add(delta, &pdev->target_tx_credit);
}
#else

static inline void
ol_tx_target_credit_decr_int(struct ol_txrx_pdev_t *pdev, int delta)
{
}

static inline void
ol_tx_target_credit_incr_int(struct ol_txrx_pdev_t *pdev, int delta)
{
}
#endif

#ifdef DESC_TIMESTAMP_DEBUG_INFO
static inline void ol_tx_desc_update_comp_ts(struct ol_tx_desc_t *tx_desc)
{
	tx_desc->desc_debug_info.last_comp_ts = qdf_get_log_timestamp();
}
#else
static inline void ol_tx_desc_update_comp_ts(struct ol_tx_desc_t *tx_desc)
{
}
#endif

#if defined(CONFIG_HL_SUPPORT) && defined(QCA_HL_NETDEV_FLOW_CONTROL)
void ol_tx_flow_ct_unpause_os_q(ol_txrx_pdev_handle pdev)
{
	struct ol_txrx_vdev_t *vdev;
	bool trigger_unpause = false;

	qdf_spin_lock_bh(&pdev->tx_mutex);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		if (vdev->tx_desc_limit == 0)
			continue;

		/* un-pause high priority queue */
		if (vdev->prio_q_paused &&
		    (qdf_atomic_read(&vdev->tx_desc_count)
		     < vdev->tx_desc_limit)) {
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_NETIF_PRIORITY_QUEUE_ON,
				       WLAN_DATA_FLOW_CONTROL_PRIORITY);
			vdev->prio_q_paused = 0;
		}
		/* un-pause non priority queues */
		if (qdf_atomic_read(&vdev->os_q_paused) &&
		    (qdf_atomic_read(&vdev->tx_desc_count)
		    <= vdev->queue_restart_th)) {
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_WAKE_NON_PRIORITY_QUEUE,
				       WLAN_DATA_FLOW_CONTROL);
			qdf_atomic_set(&vdev->os_q_paused, 0);
			trigger_unpause = true;
		}
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);
	if (trigger_unpause)
		ol_tx_hl_pdev_queue_send_all(pdev);
}
#endif

static inline uint16_t
ol_tx_send_base(struct ol_txrx_pdev_t *pdev,
		struct ol_tx_desc_t *tx_desc, qdf_nbuf_t msdu)
{
	int msdu_credit_consumed;

	TX_CREDIT_DEBUG_PRINT("TX %d bytes\n", qdf_nbuf_len(msdu));
	TX_CREDIT_DEBUG_PRINT(" <HTT> Decrease credit %d - 1 = %d, len:%d.\n",
			      qdf_atomic_read(&pdev->target_tx_credit),
			      qdf_atomic_read(&pdev->target_tx_credit) - 1,
			      qdf_nbuf_len(msdu));

	msdu_credit_consumed = htt_tx_msdu_credit(msdu);
	ol_tx_target_credit_decr_int(pdev, msdu_credit_consumed);
	OL_TX_CREDIT_RECLAIM(pdev);

	/*
	 * When the tx frame is downloaded to the target, there are two
	 * outstanding references:
	 * 1.  The host download SW (HTT, HTC, HIF)
	 *     This reference is cleared by the ol_tx_send_done callback
	 *     functions.
	 * 2.  The target FW
	 *     This reference is cleared by the ol_tx_completion_handler
	 *     function.
	 * It is extremely probable that the download completion is processed
	 * before the tx completion message.  However, under exceptional
	 * conditions the tx completion may be processed first.  Thus, rather
	 * that assuming that reference (1) is done before reference (2),
	 * explicit reference tracking is needed.
	 * Double-increment the ref count to account for both references
	 * described above.
	 */

	OL_TX_DESC_REF_INIT(tx_desc);
	OL_TX_DESC_REF_INC(tx_desc);
	OL_TX_DESC_REF_INC(tx_desc);

	return msdu_credit_consumed;
}

void
ol_tx_send(struct ol_txrx_pdev_t *pdev,
	   struct ol_tx_desc_t *tx_desc, qdf_nbuf_t msdu, uint8_t vdev_id)
{
	int msdu_credit_consumed;
	uint16_t id;
	int failed;

	msdu_credit_consumed = ol_tx_send_base(pdev, tx_desc, msdu);
	id = ol_tx_desc_id(pdev, tx_desc);
	QDF_NBUF_UPDATE_TX_PKT_COUNT(msdu, QDF_NBUF_TX_PKT_TXRX);
	DPTRACE(qdf_dp_trace_ptr(msdu, QDF_DP_TRACE_TXRX_PACKET_PTR_RECORD,
				QDF_TRACE_DEFAULT_PDEV_ID,
				qdf_nbuf_data_addr(msdu),
				sizeof(qdf_nbuf_data(msdu)), tx_desc->id,
				vdev_id));
	failed = htt_tx_send_std(pdev->htt_pdev, msdu, id);
	if (qdf_unlikely(failed)) {
		ol_tx_target_credit_incr_int(pdev, msdu_credit_consumed);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);
	}
}

void
ol_tx_send_batch(struct ol_txrx_pdev_t *pdev,
		 qdf_nbuf_t head_msdu, int num_msdus)
{
	qdf_nbuf_t rejected;

	OL_TX_CREDIT_RECLAIM(pdev);

	rejected = htt_tx_send_batch(pdev->htt_pdev, head_msdu, num_msdus);
	while (qdf_unlikely(rejected)) {
		struct ol_tx_desc_t *tx_desc;
		uint16_t *msdu_id_storage;
		qdf_nbuf_t next;

		next = qdf_nbuf_next(rejected);
		msdu_id_storage = ol_tx_msdu_id_storage(rejected);
		tx_desc = ol_tx_desc_find(pdev, *msdu_id_storage);

		ol_tx_target_credit_incr(pdev, rejected);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);

		rejected = next;
	}
}

void
ol_tx_send_nonstd(struct ol_txrx_pdev_t *pdev,
		  struct ol_tx_desc_t *tx_desc,
		  qdf_nbuf_t msdu, enum htt_pkt_type pkt_type)
{
	int msdu_credit_consumed;
	uint16_t id;
	int failed;

	msdu_credit_consumed = ol_tx_send_base(pdev, tx_desc, msdu);
	id = ol_tx_desc_id(pdev, tx_desc);
	QDF_NBUF_UPDATE_TX_PKT_COUNT(msdu, QDF_NBUF_TX_PKT_TXRX);
	failed = htt_tx_send_nonstd(pdev->htt_pdev, msdu, id, pkt_type);
	if (failed) {
		ol_txrx_err(
			   "Error: freeing tx frame after htt_tx failed");
		ol_tx_target_credit_incr_int(pdev, msdu_credit_consumed);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1 /* had error */);
	}
}

static inline bool
ol_tx_download_done_base(struct ol_txrx_pdev_t *pdev,
			 A_STATUS status, qdf_nbuf_t msdu, uint16_t msdu_id)
{
	struct ol_tx_desc_t *tx_desc;
	bool is_frame_freed = false;

	tx_desc = ol_tx_desc_find(pdev, msdu_id);
	qdf_assert(tx_desc);

	/*
	 * If the download is done for
	 * the Management frame then
	 * call the download callback if registered
	 */
	if (tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE) {
		ol_txrx_mgmt_tx_cb download_cb =
			pdev->tx_mgmt_cb.download_cb;
		if (download_cb) {
			download_cb(pdev->tx_mgmt_cb.ctxt,
				    tx_desc->netbuf, status != A_OK);
		}
	}

	if (status != A_OK) {
		ol_tx_target_credit_incr(pdev, msdu);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc,
					     1 /* download err */);
		is_frame_freed = true;
	} else {
		if (OL_TX_DESC_NO_REFS(tx_desc)) {
			/*
			 * The decremented value was zero - free the frame.
			 * Use the tx status recorded previously during
			 * tx completion handling.
			 */
			ol_tx_desc_frame_free_nonstd(pdev, tx_desc,
						     tx_desc->status !=
						     htt_tx_status_ok);
			is_frame_freed = true;
		}
	}
	return is_frame_freed;
}

void
ol_tx_download_done_ll(void *pdev,
		       A_STATUS status, qdf_nbuf_t msdu, uint16_t msdu_id)
{
	ol_tx_download_done_base((struct ol_txrx_pdev_t *)pdev, status, msdu,
				 msdu_id);
}

void
ol_tx_download_done_hl_retain(void *txrx_pdev,
			      A_STATUS status,
			      qdf_nbuf_t msdu, uint16_t msdu_id)
{
	struct ol_txrx_pdev_t *pdev = txrx_pdev;

	ol_tx_download_done_base(pdev, status, msdu, msdu_id);
}

void
ol_tx_download_done_hl_free(void *txrx_pdev,
			    A_STATUS status, qdf_nbuf_t msdu, uint16_t msdu_id)
{
	struct ol_txrx_pdev_t *pdev = txrx_pdev;
	struct ol_tx_desc_t *tx_desc;
	bool is_frame_freed;
	uint8_t dp_status;

	tx_desc = ol_tx_desc_find(pdev, msdu_id);
	qdf_assert(tx_desc);
	dp_status = qdf_dp_get_status_from_a_status(status);
	DPTRACE(qdf_dp_trace_ptr(msdu,
				 QDF_DP_TRACE_FREE_PACKET_PTR_RECORD,
				 QDF_TRACE_DEFAULT_PDEV_ID,
				 qdf_nbuf_data_addr(msdu),
				 sizeof(qdf_nbuf_data(msdu)), tx_desc->id,
				 dp_status));

	is_frame_freed = ol_tx_download_done_base(pdev, status, msdu, msdu_id);

	/*
	 * if frame is freed in ol_tx_download_done_base then return.
	 */
	if (is_frame_freed) {
		qdf_atomic_add(1, &pdev->tx_queue.rsrc_cnt);
		return;
	}

	if ((tx_desc->pkt_type != OL_TX_FRM_NO_FREE) &&
	    (tx_desc->pkt_type < OL_TXRX_MGMT_TYPE_BASE)) {
		qdf_atomic_add(1, &pdev->tx_queue.rsrc_cnt);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, status != A_OK);
	}
}

void ol_tx_target_credit_init(struct ol_txrx_pdev_t *pdev, int credit_delta)
{
	qdf_atomic_add(credit_delta, &pdev->orig_target_tx_credit);
}

void ol_tx_target_credit_update(struct ol_txrx_pdev_t *pdev, int credit_delta)
{
	TX_CREDIT_DEBUG_PRINT(" <HTT> Increase credit %d + %d = %d\n",
			      qdf_atomic_read(&pdev->target_tx_credit),
			      credit_delta,
			      qdf_atomic_read(&pdev->target_tx_credit) +
			      credit_delta);
	qdf_atomic_add(credit_delta, &pdev->target_tx_credit);
}

#ifdef QCA_COMPUTE_TX_DELAY

static void
ol_tx_delay_compute(struct ol_txrx_pdev_t *pdev,
		    enum htt_tx_status status,
		    uint16_t *desc_ids, int num_msdus);

#else
static inline void
ol_tx_delay_compute(struct ol_txrx_pdev_t *pdev,
		    enum htt_tx_status status,
		    uint16_t *desc_ids, int num_msdus)
{
}
#endif /* QCA_COMPUTE_TX_DELAY */

#if defined(CONFIG_HL_SUPPORT)
int ol_tx_deduct_one_credit(struct ol_txrx_pdev_t *pdev)
{
	/* TODO: Check if enough credits */

	if (!pdev->cfg.default_tx_comp_req) {
		ol_tx_target_credit_update(pdev, -1);
		ol_tx_deduct_one_any_group_credit(pdev);

		DPTRACE(qdf_dp_trace_credit_record(QDF_TX_HTT_MSG,
			QDF_CREDIT_DEC, 1,
			qdf_atomic_read(&pdev->target_tx_credit),
			qdf_atomic_read(&pdev->txq_grps[0].credit),
			qdf_atomic_read(&pdev->txq_grps[1].credit)));
	}

	return 0;
}
#endif /* CONFIG_HL_SUPPORT */

#ifndef OL_TX_RESTORE_HDR
#define OL_TX_RESTORE_HDR(__tx_desc, __msdu)
#endif
/*
 * The following macros could have been inline functions too.
 * The only rationale for choosing macros, is to force the compiler to inline
 * the implementation, which cannot be controlled for actual "inline" functions,
 * since "inline" is only a hint to the compiler.
 * In the performance path, we choose to force the inlining, in preference to
 * type-checking offered by the actual inlined functions.
 */
#define ol_tx_msdu_complete_batch(_pdev, _tx_desc, _tx_descs, _status) \
	TAILQ_INSERT_TAIL(&(_tx_descs), (_tx_desc), tx_desc_list_elem)
#ifndef ATH_11AC_TXCOMPACT
#define ol_tx_msdu_complete_single(_pdev, _tx_desc, _netbuf,\
				   _lcl_freelist, _tx_desc_last)	\
	do {								\
		qdf_atomic_init(&(_tx_desc)->ref_cnt);			\
		/* restore orginal hdr offset */			\
		OL_TX_RESTORE_HDR((_tx_desc), (_netbuf));		\
		qdf_nbuf_unmap((_pdev)->osdev, (_netbuf), QDF_DMA_TO_DEVICE); \
		qdf_nbuf_free((_netbuf));				\
		((union ol_tx_desc_list_elem_t *)(_tx_desc))->next =	\
			(_lcl_freelist);				\
		if (qdf_unlikely(!lcl_freelist)) {			\
			(_tx_desc_last) = (union ol_tx_desc_list_elem_t *)\
				(_tx_desc);				\
		}							\
		(_lcl_freelist) = (union ol_tx_desc_list_elem_t *)(_tx_desc); \
	} while (0)
#else    /*!ATH_11AC_TXCOMPACT */
#define ol_tx_msdu_complete_single(_pdev, _tx_desc, _netbuf,\
				   _lcl_freelist, _tx_desc_last)	\
	do {								\
		/* restore orginal hdr offset */			\
		OL_TX_RESTORE_HDR((_tx_desc), (_netbuf));		\
		qdf_nbuf_unmap((_pdev)->osdev, (_netbuf), QDF_DMA_TO_DEVICE); \
		qdf_nbuf_free((_netbuf));				\
		((union ol_tx_desc_list_elem_t *)(_tx_desc))->next =	\
			(_lcl_freelist);				\
		if (qdf_unlikely(!lcl_freelist)) {			\
			(_tx_desc_last) = (union ol_tx_desc_list_elem_t *)\
				(_tx_desc);				\
		}							\
		(_lcl_freelist) = (union ol_tx_desc_list_elem_t *)(_tx_desc); \
	} while (0)

#endif /*!ATH_11AC_TXCOMPACT */

#ifdef QCA_TX_SINGLE_COMPLETIONS
#ifdef QCA_TX_STD_PATH_ONLY
#define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs,			\
			    _netbuf, _lcl_freelist,			\
			    _tx_desc_last, _status, is_tx_desc_freed)	\
	{								\
		is_tx_desc_freed = 0;					\
		ol_tx_msdu_complete_single((_pdev), (_tx_desc),		\
					   (_netbuf), (_lcl_freelist),	\
					   _tx_desc_last)		\
	}
#else                           /* !QCA_TX_STD_PATH_ONLY */
#define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs,			\
			    _netbuf, _lcl_freelist,			\
			    _tx_desc_last, _status, is_tx_desc_freed)	\
	do {								\
		if (qdf_likely((_tx_desc)->pkt_type == OL_TX_FRM_STD)) { \
			is_tx_desc_freed = 0;				\
			ol_tx_msdu_complete_single((_pdev), (_tx_desc),\
						   (_netbuf), (_lcl_freelist), \
						   (_tx_desc_last));	\
		} else {						\
			is_tx_desc_freed = 1;				\
			ol_tx_desc_frame_free_nonstd(			\
				(_pdev), (_tx_desc),			\
				(_status) != htt_tx_status_ok);		\
		}							\
	} while (0)
#endif /* !QCA_TX_STD_PATH_ONLY */
#else                           /* !QCA_TX_SINGLE_COMPLETIONS */
#ifdef QCA_TX_STD_PATH_ONLY
#define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs,			\
			    _netbuf, _lcl_freelist,			\
			    _tx_desc_last, _status, is_tx_desc_freed)	\
	{								\
		is_tx_desc_freed = 0;					\
		ol_tx_msdu_complete_batch((_pdev), (_tx_desc),		\
					(_tx_descs), (_status))		\
	}
#else                           /* !QCA_TX_STD_PATH_ONLY */
#define ol_tx_msdu_complete(_pdev, _tx_desc, _tx_descs,			\
			    _netbuf, _lcl_freelist,			\
			    _tx_desc_last, _status, is_tx_desc_freed)	\
	do {								\
		if (qdf_likely((_tx_desc)->pkt_type == OL_TX_FRM_STD)) { \
			is_tx_desc_freed = 0;				\
			ol_tx_msdu_complete_batch((_pdev), (_tx_desc),	\
						  (_tx_descs), (_status)); \
		} else {						\
			is_tx_desc_freed = 1;				\
			ol_tx_desc_frame_free_nonstd((_pdev), (_tx_desc), \
						     (_status) !=	\
						     htt_tx_status_ok); \
		}							\
	} while (0)
#endif /* !QCA_TX_STD_PATH_ONLY */
#endif /* QCA_TX_SINGLE_COMPLETIONS */

#if !defined(CONFIG_HL_SUPPORT)
void ol_tx_discard_target_frms(ol_txrx_pdev_handle pdev)
{
	int i = 0;
	struct ol_tx_desc_t *tx_desc;
	int num_disarded = 0;

	for (i = 0; i < pdev->tx_desc.pool_size; i++) {
		tx_desc = ol_tx_desc_find(pdev, i);
		/*
		 * Confirm that each tx descriptor is "empty", i.e. it has
		 * no tx frame attached.
		 * In particular, check that there are no frames that have
		 * been given to the target to transmit, for which the
		 * target has never provided a response.
		 */
		if (qdf_atomic_read(&tx_desc->ref_cnt)) {
			ol_txrx_dbg(
				   "Warning: freeing tx desc %d", tx_desc->id);
			ol_tx_desc_frame_free_nonstd(pdev,
						     tx_desc, 1);
			num_disarded++;
		}
	}

	if (num_disarded)
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
			"Warning: freed %d tx descs for which no tx completion rcvd from the target",
			num_disarded);
}
#endif

void ol_tx_credit_completion_handler(ol_txrx_pdev_handle pdev, int credits)
{
	ol_tx_target_credit_update(pdev, credits);

	if (pdev->cfg.is_high_latency)
		ol_tx_sched(pdev);

	/* UNPAUSE OS Q */
	ol_tx_flow_ct_unpause_os_q(pdev);
}

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * ol_tx_flow_pool_lock() - take flow pool lock
 * @tx_desc: tx desc
 *
 * Return: None
 */
static inline
void ol_tx_flow_pool_lock(struct ol_tx_desc_t *tx_desc)
{
	struct ol_tx_flow_pool_t *pool;

	pool = tx_desc->pool;
	qdf_spin_lock_bh(&pool->flow_pool_lock);
}

/**
 * ol_tx_flow_pool_unlock() - release flow pool lock
 * @tx_desc: tx desc
 *
 * Return: None
 */
static inline
void ol_tx_flow_pool_unlock(struct ol_tx_desc_t *tx_desc)
{
	struct ol_tx_flow_pool_t *pool;

	pool = tx_desc->pool;
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
}
#else
static inline
void ol_tx_flow_pool_lock(struct ol_tx_desc_t *tx_desc)
{
}

static inline
void ol_tx_flow_pool_unlock(struct ol_tx_desc_t *tx_desc)
{
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE
#define RESERVE_BYTES 100
/**
 * ol_tx_pkt_capture_tx_completion_process(): process tx packets
 * for pkt capture mode
 * @pdev: device handler
 * @tx_desc: tx desc
 * @payload: tx data header
 * @tid:  tid number
 * @status: Tx status
 *
 * Return: none
 */
static void
ol_tx_pkt_capture_tx_completion_process(
			ol_txrx_pdev_handle pdev,
			struct ol_tx_desc_t *tx_desc,
			struct htt_tx_data_hdr_information *payload_hdr,
			uint8_t tid, uint8_t status)
{
	qdf_nbuf_t netbuf;
	int nbuf_len;
	struct qdf_tso_seg_elem_t *tso_seg = NULL;
	struct ol_txrx_peer_t *peer;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint8_t pkt_type = 0;

	qdf_assert(tx_desc);

	ol_tx_flow_pool_lock(tx_desc);
	/*
	 * In cases when vdev has gone down and tx completion
	 * are received, leads to NULL vdev access.
	 * So, check for NULL before dereferencing it.
	 */
	if (!tx_desc->vdev) {
		ol_tx_flow_pool_unlock(tx_desc);
		return;
	}

	ol_tx_flow_pool_unlock(tx_desc);

	if (tx_desc->pkt_type == OL_TX_FRM_TSO) {
		if (!tx_desc->tso_desc)
			return;

		tso_seg = tx_desc->tso_desc;
		nbuf_len = tso_seg->seg.total_len;
	} else {
		int i, extra_frag_len = 0;

		i = QDF_NBUF_CB_TX_NUM_EXTRA_FRAGS(tx_desc->netbuf);
		if (i > 0)
			extra_frag_len =
			QDF_NBUF_CB_TX_EXTRA_FRAG_LEN(tx_desc->netbuf);
		nbuf_len = qdf_nbuf_len(tx_desc->netbuf) - extra_frag_len;
	}

	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	peer = TAILQ_FIRST(&tx_desc->vdev->peer_list);
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
	if (!peer)
		return;

	qdf_spin_lock_bh(&peer->peer_info_lock);
	qdf_mem_copy(bssid, &peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
	qdf_spin_unlock_bh(&peer->peer_info_lock);

	netbuf = qdf_nbuf_alloc(NULL,
				roundup(nbuf_len + RESERVE_BYTES, 4),
				RESERVE_BYTES, 4, false);
	if (!netbuf)
		return;

	qdf_nbuf_put_tail(netbuf, nbuf_len);

	if (tx_desc->pkt_type == OL_TX_FRM_TSO) {
		uint8_t frag_cnt, num_frags = 0;
		int frag_len = 0;
		uint32_t tcp_seq_num;
		uint16_t ip_len;

		qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);

		if (tso_seg->seg.num_frags > 0)
			num_frags = tso_seg->seg.num_frags - 1;

		/*Num of frags in a tso seg cannot be less than 2 */
		if (num_frags < 1) {
			qdf_print("ERROR: num of frags in tso segment is %d\n",
				  (num_frags + 1));
			qdf_nbuf_free(netbuf);
			qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
			return;
		}

		tcp_seq_num = tso_seg->seg.tso_flags.tcp_seq_num;
		tcp_seq_num = qdf_cpu_to_be32(tcp_seq_num);

		ip_len = tso_seg->seg.tso_flags.ip_len;
		ip_len = qdf_cpu_to_be16(ip_len);

		for (frag_cnt = 0; frag_cnt < num_frags; frag_cnt++) {
			qdf_mem_copy(qdf_nbuf_data(netbuf) + frag_len,
				     tso_seg->seg.tso_frags[frag_cnt].vaddr,
				     tso_seg->seg.tso_frags[frag_cnt].length);
			frag_len += tso_seg->seg.tso_frags[frag_cnt].length;
		}

		qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);

		qdf_mem_copy((qdf_nbuf_data(netbuf) + IPV4_PKT_LEN_OFFSET),
			     &ip_len, sizeof(ip_len));
		qdf_mem_copy((qdf_nbuf_data(netbuf) + IPV4_TCP_SEQ_NUM_OFFSET),
			     &tcp_seq_num, sizeof(tcp_seq_num));
	} else {
		qdf_mem_copy(qdf_nbuf_data(netbuf),
			     qdf_nbuf_data(tx_desc->netbuf),
			     nbuf_len);
	}

	qdf_nbuf_push_head(
			netbuf,
			sizeof(struct htt_tx_data_hdr_information));
	qdf_mem_copy(qdf_nbuf_data(netbuf), payload_hdr,
		     sizeof(struct htt_tx_data_hdr_information));

	ucfg_pkt_capture_tx_completion_process(
				tx_desc->vdev_id,
				netbuf, pkt_type,
				tid, status,
				TXRX_PKTCAPTURE_PKT_FORMAT_8023, bssid,
				pdev->htt_pdev, payload_hdr->tx_retry_cnt);
}
#else
static void
ol_tx_pkt_capture_tx_completion_process(
			ol_txrx_pdev_handle pdev,
			struct ol_tx_desc_t *tx_desc,
			struct htt_tx_data_hdr_information *payload_hdr,
			uint8_t tid, uint8_t status)
{
}
#endif /* WLAN_FEATURE_PKT_CAPTURE */

#ifdef WLAN_FEATURE_TSF_PLUS
static inline struct htt_tx_compl_ind_append_tx_tstamp *ol_tx_get_txtstamps(
		u_int32_t *msg_word_header, u_int32_t **msg_word_payload,
		int num_msdus)
{
	u_int32_t has_tx_tsf;
	u_int32_t has_retry;

	struct htt_tx_compl_ind_append_tx_tstamp *txtstamp_list = NULL;
	struct htt_tx_compl_ind_append_retries *retry_list = NULL;
	int offset_dwords;

	if (num_msdus <= 0)
		return NULL;

	has_tx_tsf = HTT_TX_COMPL_IND_APPEND1_GET(*msg_word_header);

	/* skip header and MSDUx ID part*/
	offset_dwords = ((num_msdus + 1) >> 1);
	*msg_word_payload += offset_dwords;

	if (!has_tx_tsf)
		return NULL;

	has_retry = HTT_TX_COMPL_IND_APPEND_GET(*msg_word_header);
	if (has_retry) {
		int retry_index = 0;
		int width_for_each_retry =
			(sizeof(struct htt_tx_compl_ind_append_retries) +
			3) >> 2;

		retry_list = (struct htt_tx_compl_ind_append_retries *)
			(*msg_word_payload + offset_dwords);
		while (retry_list) {
			if (retry_list[retry_index++].flag == 0)
				break;
		}
		offset_dwords = retry_index * width_for_each_retry;
	}

	*msg_word_payload +=  offset_dwords;
	txtstamp_list = (struct htt_tx_compl_ind_append_tx_tstamp *)
		(*msg_word_payload);
	return txtstamp_list;
}

static inline
struct htt_tx_compl_ind_append_tx_tsf64 *ol_tx_get_txtstamp64s(
		u_int32_t *msg_word_header, u_int32_t **msg_word_payload,
		int num_msdus)
{
	u_int32_t has_tx_tstamp64;
	u_int32_t has_rssi;
	struct htt_tx_compl_ind_append_tx_tsf64 *txtstamp64_list = NULL;

	int offset_dwords = 0;

	if (num_msdus <= 0)
		return NULL;

	has_tx_tstamp64 = HTT_TX_COMPL_IND_APPEND3_GET(*msg_word_header);
	if (!has_tx_tstamp64)
		return NULL;

	/*skip MSDUx ACK RSSI part*/
	has_rssi = HTT_TX_COMPL_IND_APPEND2_GET(*msg_word_header);
	if (has_rssi)
		offset_dwords = ((num_msdus + 1) >> 1);

	*msg_word_payload = *msg_word_payload + offset_dwords;
	txtstamp64_list =
		(struct htt_tx_compl_ind_append_tx_tsf64 *)
		(*msg_word_payload);

	return txtstamp64_list;
}

static inline void ol_tx_timestamp(ol_txrx_pdev_handle pdev,
				   qdf_nbuf_t netbuf, u_int64_t ts)
{
	if (!netbuf)
		return;

	if (pdev->ol_tx_timestamp_cb)
		pdev->ol_tx_timestamp_cb(netbuf, ts);
}
#else
static inline struct htt_tx_compl_ind_append_tx_tstamp *ol_tx_get_txtstamps(
		u_int32_t *msg_word_header, u_int32_t **msg_word_payload,
		int num_msdus)
{
	return NULL;
}

static inline
struct htt_tx_compl_ind_append_tx_tsf64 *ol_tx_get_txtstamp64s(
		u_int32_t *msg_word_header, u_int32_t **msg_word_payload,
		int num_msdus)
{
	return NULL;
}

static inline void ol_tx_timestamp(ol_txrx_pdev_handle pdev,
				   qdf_nbuf_t netbuf, u_int64_t ts)
{
}
#endif

static void ol_tx_update_ack_count(struct ol_tx_desc_t *tx_desc,
				   enum htt_tx_status status)
{
	if (!tx_desc->vdev)
		return;

	if (status == htt_tx_status_ok)
		++tx_desc->vdev->txrx_stats.txack_success;
	else
		++tx_desc->vdev->txrx_stats.txack_failed;
}

/**
 * ol_tx_notify_completion() - Notify tx completion for this desc
 * @tx_desc: tx desc
 * @netbuf:  buffer
 * @status: tx status
 *
 * Return: none
 */
static void ol_tx_notify_completion(struct ol_tx_desc_t *tx_desc,
				    qdf_nbuf_t netbuf, uint8_t status)
{
	void *osif_dev;
	ol_txrx_completion_fp tx_compl_cbk = NULL;
	uint16_t flag = 0;

	qdf_assert(tx_desc);

	ol_tx_flow_pool_lock(tx_desc);

	if (!tx_desc->vdev ||
	    !tx_desc->vdev->osif_dev) {
		ol_tx_flow_pool_unlock(tx_desc);
		return;
	}
	osif_dev = tx_desc->vdev->osif_dev;
	tx_compl_cbk = tx_desc->vdev->tx_comp;
	ol_tx_flow_pool_unlock(tx_desc);

	if (status == htt_tx_status_ok)
		flag = (BIT(QDF_TX_RX_STATUS_OK) |
			BIT(QDF_TX_RX_STATUS_DOWNLOAD_SUCC));
	else if (status != htt_tx_status_download_fail)
		flag = BIT(QDF_TX_RX_STATUS_DOWNLOAD_SUCC);

	if (tx_compl_cbk)
		tx_compl_cbk(netbuf, osif_dev, flag);
}

/**
 * ol_tx_update_connectivity_stats() - update connectivity stats
 * @tx_desc: tx desc
 * @netbuf:  buffer
 * @status: htt status
 *
 *
 * Return: none
 */
static void ol_tx_update_connectivity_stats(struct ol_tx_desc_t *tx_desc,
					    qdf_nbuf_t netbuf,
					    enum htt_tx_status status)
{
	void *osif_dev;
	uint32_t pkt_type_bitmap;
	ol_txrx_stats_rx_fp stats_rx = NULL;
	uint8_t pkt_type = 0;

	qdf_assert(tx_desc);

	ol_tx_flow_pool_lock(tx_desc);
	/*
	 * In cases when vdev has gone down and tx completion
	 * are received, leads to NULL vdev access.
	 * So, check for NULL before dereferencing it.
	 */
	if (!tx_desc->vdev ||
	    !tx_desc->vdev->osif_dev ||
	    !tx_desc->vdev->stats_rx) {
		ol_tx_flow_pool_unlock(tx_desc);
		return;
	}
	osif_dev = tx_desc->vdev->osif_dev;
	stats_rx = tx_desc->vdev->stats_rx;
	ol_tx_flow_pool_unlock(tx_desc);

	pkt_type_bitmap = cds_get_connectivity_stats_pkt_bitmap(osif_dev);

	if (pkt_type_bitmap) {
		if (status != htt_tx_status_download_fail)
			stats_rx(netbuf, osif_dev,
				 PKT_TYPE_TX_HOST_FW_SENT, &pkt_type);
		if (status == htt_tx_status_ok)
			stats_rx(netbuf, osif_dev,
				 PKT_TYPE_TX_ACK_CNT, &pkt_type);
	}
}

/**
 * WARNING: ol_tx_inspect_handler()'s behavior is similar to that of
 * ol_tx_completion_handler().
 * any change in ol_tx_completion_handler() must be mirrored in
 * ol_tx_inspect_handler().
 */
void
ol_tx_completion_handler(ol_txrx_pdev_handle pdev,
			 int num_msdus,
			 enum htt_tx_status status, void *msg)
{
	int i;
	uint16_t tx_desc_id;
	struct ol_tx_desc_t *tx_desc;
	uint32_t byte_cnt = 0;
	qdf_nbuf_t netbuf;
#if !defined(REMOVE_PKT_LOG)
	ol_txrx_pktdump_cb packetdump_cb;
#endif
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t is_tx_desc_freed = 0;
	struct htt_tx_compl_ind_append_tx_tstamp *txtstamp_list = NULL;
	struct htt_tx_compl_ind_append_tx_tsf64 *txtstamp64_list = NULL;
	struct htt_tx_data_hdr_information *pkt_capture_txcomp_hdr_list = NULL;
	u_int32_t *msg_word_header = (u_int32_t *)msg;
	/*msg_word skip header*/
	u_int32_t *msg_word_payload = msg_word_header + 1;
	u_int32_t *msg_word = (u_int32_t *)msg;
	u_int16_t *desc_ids = (u_int16_t *)(msg_word + 1);
	union ol_tx_desc_list_elem_t *lcl_freelist = NULL;
	union ol_tx_desc_list_elem_t *tx_desc_last = NULL;
	ol_tx_desc_list tx_descs;
	uint64_t tx_tsf64;
	uint8_t tid;
	uint8_t dp_status;

	TAILQ_INIT(&tx_descs);

	tid = HTT_TX_COMPL_IND_TID_GET(*msg_word);

	ol_tx_delay_compute(pdev, status, desc_ids, num_msdus);
	if (status == htt_tx_status_ok) {
		txtstamp_list = ol_tx_get_txtstamps(
			msg_word_header, &msg_word_payload, num_msdus);
		if (pdev->enable_tx_compl_tsf64)
			txtstamp64_list = ol_tx_get_txtstamp64s(
				msg_word_header, &msg_word_payload, num_msdus);
	}

	if ((ucfg_pkt_capture_get_mode((void *)soc->psoc) ==
						PACKET_CAPTURE_MODE_DATA_ONLY))
		pkt_capture_txcomp_hdr_list =
				ucfg_pkt_capture_tx_get_txcomplete_data_hdr(
								msg_word,
								num_msdus);

	for (i = 0; i < num_msdus; i++) {
		tx_desc_id = desc_ids[i];
		if (tx_desc_id >= pdev->tx_desc.pool_size) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			"%s: drop due to invalid msdu id = %x\n",
			__func__, tx_desc_id);
			continue;
		}
		tx_desc = ol_tx_desc_find(pdev, tx_desc_id);
		qdf_assert(tx_desc);
		ol_tx_desc_update_comp_ts(tx_desc);
		tx_desc->status = status;
		netbuf = tx_desc->netbuf;

		if (txtstamp64_list) {
			tx_tsf64 =
			(u_int64_t)txtstamp64_list[i].tx_tsf64_high << 32 |
			txtstamp64_list[i].tx_tsf64_low;

			ol_tx_timestamp(pdev, netbuf, tx_tsf64);
		} else if (txtstamp_list)
			ol_tx_timestamp(pdev, netbuf,
					(u_int64_t)txtstamp_list->timestamp[i]
					);

		if (pkt_capture_txcomp_hdr_list) {
			ol_tx_pkt_capture_tx_completion_process(
						pdev,
						tx_desc,
						&pkt_capture_txcomp_hdr_list[i],
						tid, status);
		}

		QDF_NBUF_UPDATE_TX_PKT_COUNT(netbuf, QDF_NBUF_TX_PKT_FREE);

		/* check tx completion notification */
		if (QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(netbuf))
			ol_tx_notify_completion(tx_desc, netbuf, status);

		/* track connectivity stats */
		ol_tx_update_connectivity_stats(tx_desc, netbuf,
						status);
		ol_tx_update_ack_count(tx_desc, status);

#if !defined(REMOVE_PKT_LOG)
		if (tx_desc->pkt_type != OL_TX_FRM_TSO) {
			packetdump_cb = pdev->ol_tx_packetdump_cb;
			if (packetdump_cb)
				packetdump_cb((void *)soc, pdev->id,
					      tx_desc->vdev_id,
					      netbuf, status, TX_DATA_PKT);
		}
#endif
		dp_status = qdf_dp_get_status_from_htt(status);

		DPTRACE(qdf_dp_trace_ptr(netbuf,
			QDF_DP_TRACE_FREE_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID,
			qdf_nbuf_data_addr(netbuf),
			sizeof(qdf_nbuf_data(netbuf)), tx_desc->id, dp_status));
		htc_pm_runtime_put(pdev->htt_pdev->htc_pdev);
		/*
		 * If credits are reported through credit_update_ind then do not
		 * update group credits on tx_complete_ind.
		 */
		if (!pdev->cfg.credit_update_enabled)
			ol_tx_desc_update_group_credit(pdev,
						       tx_desc_id,
						       1, 0, status);
		/* Per SDU update of byte count */
		byte_cnt += qdf_nbuf_len(netbuf);
		if (OL_TX_DESC_NO_REFS(tx_desc)) {
			ol_tx_statistics(
				pdev->ctrl_pdev,
				HTT_TX_DESC_VDEV_ID_GET(*((uint32_t *)
							  (tx_desc->
							   htt_tx_desc))),
				status != htt_tx_status_ok);
			ol_tx_msdu_complete(pdev, tx_desc, tx_descs, netbuf,
					    lcl_freelist, tx_desc_last, status,
					    is_tx_desc_freed);

#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
			if (!is_tx_desc_freed) {
				tx_desc->pkt_type = ol_tx_frm_freed;
#ifdef QCA_COMPUTE_TX_DELAY
				tx_desc->entry_timestamp_ticks = 0xffffffff;
#endif
			}
#endif
		}
	}

	/* One shot protected access to pdev freelist, when setup */
	if (lcl_freelist) {
		qdf_spin_lock(&pdev->tx_mutex);
		tx_desc_last->next = pdev->tx_desc.freelist;
		pdev->tx_desc.freelist = lcl_freelist;
		pdev->tx_desc.num_free += (uint16_t) num_msdus;
		qdf_spin_unlock(&pdev->tx_mutex);
	} else {
		ol_tx_desc_frame_list_free(pdev, &tx_descs,
					   status != htt_tx_status_ok);
	}

	if (pdev->cfg.is_high_latency) {
		/*
		 * Credit was already explicitly updated by HTT,
		 * but update the number of available tx descriptors,
		 * then invoke the scheduler, since new credit is probably
		 * available now.
		 */
		qdf_atomic_add(num_msdus, &pdev->tx_queue.rsrc_cnt);
		ol_tx_sched(pdev);
	} else {
		ol_tx_target_credit_adjust(num_msdus, pdev, NULL);
	}

	/* UNPAUSE OS Q */
	ol_tx_flow_ct_unpause_os_q(pdev);
	/* Do one shot statistics */
	TXRX_STATS_UPDATE_TX_STATS(pdev, status, num_msdus, byte_cnt);
}

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL

void ol_tx_desc_update_group_credit(ol_txrx_pdev_handle pdev,
		u_int16_t tx_desc_id, int credit, u_int8_t absolute,
		enum htt_tx_status status)
{
	uint8_t i, is_member;
	uint16_t vdev_id_mask;
	struct ol_tx_desc_t *tx_desc;

	if (tx_desc_id >= pdev->tx_desc.pool_size) {
		qdf_print("Invalid desc id");
		return;
	}

	tx_desc = ol_tx_desc_find(pdev, tx_desc_id);
	for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
		vdev_id_mask =
			OL_TXQ_GROUP_VDEV_ID_MASK_GET(
					pdev->txq_grps[i].membership);
		is_member = OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(vdev_id_mask,
				tx_desc->vdev_id);
		if (is_member) {
			ol_txrx_update_group_credit(&pdev->txq_grps[i],
						    credit, absolute);
			break;
		}
	}
	ol_tx_update_group_credit_stats(pdev);
}

void ol_tx_deduct_one_any_group_credit(ol_txrx_pdev_handle pdev)
{
	int credits_group_0, credits_group_1;

	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
	credits_group_0 = qdf_atomic_read(&pdev->txq_grps[0].credit);
	credits_group_1 = qdf_atomic_read(&pdev->txq_grps[1].credit);

	if (credits_group_0 > credits_group_1)
		ol_txrx_update_group_credit(&pdev->txq_grps[0], -1, 0);
	else if (credits_group_1 != 0)
		ol_txrx_update_group_credit(&pdev->txq_grps[1], -1, 0);

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
}

#ifdef DEBUG_HL_LOGGING

void ol_tx_update_group_credit_stats(ol_txrx_pdev_handle pdev)
{
	uint16_t curr_index;
	uint8_t i;

	qdf_spin_lock_bh(&pdev->grp_stat_spinlock);
	pdev->grp_stats.last_valid_index++;
	if (pdev->grp_stats.last_valid_index > (OL_TX_GROUP_STATS_LOG_SIZE
				- 1)) {
		pdev->grp_stats.last_valid_index -= OL_TX_GROUP_STATS_LOG_SIZE;
		pdev->grp_stats.wrap_around = 1;
	}
	curr_index = pdev->grp_stats.last_valid_index;

	for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
		pdev->grp_stats.stats[curr_index].grp[i].member_vdevs =
			OL_TXQ_GROUP_VDEV_ID_MASK_GET(
					pdev->txq_grps[i].membership);
		pdev->grp_stats.stats[curr_index].grp[i].credit =
			qdf_atomic_read(&pdev->txq_grps[i].credit);
	}

	qdf_spin_unlock_bh(&pdev->grp_stat_spinlock);
}

void ol_tx_dump_group_credit_stats(ol_txrx_pdev_handle pdev)
{
	uint16_t i, j, is_break = 0;
	int16_t curr_index, old_index, wrap_around;
	uint16_t curr_credit, mem_vdevs;
	uint16_t old_credit = 0;

	txrx_nofl_info("Group credit stats:");
	txrx_nofl_info("  No: GrpID: Credit: Change: vdev_map");

	qdf_spin_lock_bh(&pdev->grp_stat_spinlock);
	curr_index = pdev->grp_stats.last_valid_index;
	wrap_around = pdev->grp_stats.wrap_around;
	qdf_spin_unlock_bh(&pdev->grp_stat_spinlock);

	if (curr_index < 0) {
		txrx_nofl_info("Not initialized");
		return;
	}

	for (i = 0; i < OL_TX_GROUP_STATS_LOG_SIZE; i++) {
		old_index = curr_index - 1;
		if (old_index < 0) {
			if (wrap_around == 0)
				is_break = 1;
			else
				old_index = OL_TX_GROUP_STATS_LOG_SIZE - 1;
		}

		for (j = 0; j < OL_TX_MAX_TXQ_GROUPS; j++) {
			qdf_spin_lock_bh(&pdev->grp_stat_spinlock);
			curr_credit =
				pdev->grp_stats.stats[curr_index].
								grp[j].credit;
			if (!is_break)
				old_credit =
					pdev->grp_stats.stats[old_index].
								grp[j].credit;

			mem_vdevs =
				pdev->grp_stats.stats[curr_index].grp[j].
								member_vdevs;
			qdf_spin_unlock_bh(&pdev->grp_stat_spinlock);

			if (!is_break)
				txrx_nofl_info("%4d: %5d: %6d %6d %8x",
					       curr_index, j,
					       curr_credit,
					       (curr_credit - old_credit),
					       mem_vdevs);
			else
				txrx_nofl_info("%4d: %5d: %6d %6s %8x",
					       curr_index, j,
					       curr_credit, "NA", mem_vdevs);
		}

		if (is_break)
			break;

		curr_index = old_index;
	}
}

void ol_tx_clear_group_credit_stats(ol_txrx_pdev_handle pdev)
{
	qdf_spin_lock_bh(&pdev->grp_stat_spinlock);
	qdf_mem_zero(&pdev->grp_stats, sizeof(pdev->grp_stats));
	pdev->grp_stats.last_valid_index = -1;
	pdev->grp_stats.wrap_around = 0;
	qdf_spin_unlock_bh(&pdev->grp_stat_spinlock);
}
#endif
#endif

/*
 * ol_tx_single_completion_handler performs the same tx completion
 * processing as ol_tx_completion_handler, but for a single frame.
 * ol_tx_completion_handler is optimized to handle batch completions
 * as efficiently as possible; in contrast ol_tx_single_completion_handler
 * handles single frames as simply and generally as possible.
 * Thus, this ol_tx_single_completion_handler function is suitable for
 * intermittent usage, such as for tx mgmt frames.
 */
void
ol_tx_single_completion_handler(ol_txrx_pdev_handle pdev,
				enum htt_tx_status status, uint16_t tx_desc_id)
{
	struct ol_tx_desc_t *tx_desc;
	qdf_nbuf_t netbuf;
#if !defined(REMOVE_PKT_LOG)
	ol_txrx_pktdump_cb packetdump_cb;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
#endif

	tx_desc = ol_tx_desc_find_check(pdev, tx_desc_id);
	if (!tx_desc) {
		ol_txrx_err("invalid desc_id(%u), ignore it", tx_desc_id);
		return;
	}

	tx_desc->status = status;
	netbuf = tx_desc->netbuf;

	QDF_NBUF_UPDATE_TX_PKT_COUNT(netbuf, QDF_NBUF_TX_PKT_FREE);
	/* Do one shot statistics */
	TXRX_STATS_UPDATE_TX_STATS(pdev, status, 1, qdf_nbuf_len(netbuf));

#if !defined(REMOVE_PKT_LOG)
	packetdump_cb = pdev->ol_tx_packetdump_cb;
	if (packetdump_cb)
		packetdump_cb(soc, pdev->id, tx_desc->vdev_id,
			      netbuf, status, TX_MGMT_PKT);
#endif

	if (OL_TX_DESC_NO_REFS(tx_desc)) {
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc,
					     status != htt_tx_status_ok);
	}

	TX_CREDIT_DEBUG_PRINT(" <HTT> Increase credit %d + %d = %d\n",
			      qdf_atomic_read(&pdev->target_tx_credit),
			      1, qdf_atomic_read(&pdev->target_tx_credit) + 1);

	if (pdev->cfg.is_high_latency) {
		/*
		 * Credit was already explicitly updated by HTT,
		 * but update the number of available tx descriptors,
		 * then invoke the scheduler, since new credit is probably
		 * available now.
		 */
		qdf_atomic_add(1, &pdev->tx_queue.rsrc_cnt);
		ol_tx_sched(pdev);
	} else {
		qdf_atomic_add(1, &pdev->target_tx_credit);
	}
}

/**
 * WARNING: ol_tx_inspect_handler()'s behavior is similar to that of
 * ol_tx_completion_handler().
 * any change in ol_tx_completion_handler() must be mirrored here.
 */
void
ol_tx_inspect_handler(ol_txrx_pdev_handle pdev,
		      int num_msdus, void *tx_desc_id_iterator)
{
	uint16_t vdev_id, i;
	struct ol_txrx_vdev_t *vdev;
	uint16_t *desc_ids = (uint16_t *) tx_desc_id_iterator;
	uint16_t tx_desc_id;
	struct ol_tx_desc_t *tx_desc;
	union ol_tx_desc_list_elem_t *lcl_freelist = NULL;
	union ol_tx_desc_list_elem_t *tx_desc_last = NULL;
	qdf_nbuf_t netbuf;
	ol_tx_desc_list tx_descs;
	uint32_t is_tx_desc_freed = 0;

	TAILQ_INIT(&tx_descs);

	for (i = 0; i < num_msdus; i++) {
		tx_desc_id = desc_ids[i];
		if (tx_desc_id >= pdev->tx_desc.pool_size) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			"%s: drop due to invalid msdu id = %x\n",
			__func__, tx_desc_id);
			continue;
		}
		tx_desc = ol_tx_desc_find(pdev, tx_desc_id);
		qdf_assert(tx_desc);
		ol_tx_desc_update_comp_ts(tx_desc);
		netbuf = tx_desc->netbuf;

		/* find the "vdev" this tx_desc belongs to */
		vdev_id = HTT_TX_DESC_VDEV_ID_GET(*((uint32_t *)
						    (tx_desc->htt_tx_desc)));
		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			if (vdev->vdev_id == vdev_id)
				break;
		}

		/* vdev now points to the vdev for this descriptor. */

#ifndef ATH_11AC_TXCOMPACT
		/* save this multicast packet to local free list */
		if (qdf_atomic_dec_and_test(&tx_desc->ref_cnt))
#endif
		{
			/*
			 * For this function only, force htt status to be
			 * "htt_tx_status_ok"
			 * for graceful freeing of this multicast frame
			 */
			ol_tx_msdu_complete(pdev, tx_desc, tx_descs, netbuf,
					    lcl_freelist, tx_desc_last,
					    htt_tx_status_ok,
					    is_tx_desc_freed);
#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
			if (!is_tx_desc_freed) {
				tx_desc->pkt_type = ol_tx_frm_freed;
#ifdef QCA_COMPUTE_TX_DELAY
				tx_desc->entry_timestamp_ticks = 0xffffffff;
#endif
			}
#endif
		}
	}

	if (lcl_freelist) {
		qdf_spin_lock(&pdev->tx_mutex);
		tx_desc_last->next = pdev->tx_desc.freelist;
		pdev->tx_desc.freelist = lcl_freelist;
		qdf_spin_unlock(&pdev->tx_mutex);
	} else {
		ol_tx_desc_frame_list_free(pdev, &tx_descs,
					   htt_tx_status_discard);
	}
	TX_CREDIT_DEBUG_PRINT(" <HTT> Increase HTT credit %d + %d = %d..\n",
			      qdf_atomic_read(&pdev->target_tx_credit),
			      num_msdus,
			      qdf_atomic_read(&pdev->target_tx_credit) +
			      num_msdus);

	if (pdev->cfg.is_high_latency) {
		/* credit was already explicitly updated by HTT */
		ol_tx_sched(pdev);
	} else {
		ol_tx_target_credit_adjust(num_msdus, pdev, NULL);
	}
}

#ifdef QCA_COMPUTE_TX_DELAY
/**
 * ol_tx_set_compute_interval -  updates the compute interval
 *				 period for TSM stats.
 * @soc_hdl: Datapath soc handle
 * @pdev_id: id of data path pdev handle
 * @param interval: interval for stats computation
 *
 * Return: None
 */
void ol_tx_set_compute_interval(struct cdp_soc_t *soc_hdl,
				uint8_t pdev_id, uint32_t interval)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);

	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	pdev->tx_delay.avg_period_ticks = qdf_system_msecs_to_ticks(interval);
}

/**
 * ol_tx_packet_count() -  Return the uplink (transmitted) packet count
			   and loss count.
 * @soc_hdl: soc handle
 * @pdev_id: pdev identifier
 * @out_packet_count - number of packets transmitted
 * @out_packet_loss_count - number of packets lost
 * @category - access category of interest
 *
 *  This function will be called for getting uplink packet count and
 *  loss count for given stream (access category) a regular interval.
 *  This also resets the counters hence, the value returned is packets
 *  counted in last 5(default) second interval. These counter are
 *  incremented per access category in ol_tx_completion_handler()
 */
void
ol_tx_packet_count(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		   uint16_t *out_packet_count,
		   uint16_t *out_packet_loss_count, int category)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);

	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	*out_packet_count = pdev->packet_count[category];
	*out_packet_loss_count = pdev->packet_loss_count[category];
	pdev->packet_count[category] = 0;
	pdev->packet_loss_count[category] = 0;
}

static uint32_t ol_tx_delay_avg(uint64_t sum, uint32_t num)
{
	uint32_t sum32;
	int shift = 0;
	/*
	 * To avoid doing a 64-bit divide, shift the sum down until it is
	 * no more than 32 bits (and shift the denominator to match).
	 */
	while ((sum >> 32) != 0) {
		sum >>= 1;
		shift++;
	}
	sum32 = (uint32_t) sum;
	num >>= shift;
	return (sum32 + (num >> 1)) / num;      /* round to nearest */
}

void
ol_tx_delay(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	    uint32_t *queue_delay_microsec,
	    uint32_t *tx_delay_microsec, int category)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	int index;
	uint32_t avg_delay_ticks;
	struct ol_tx_delay_data *data;

	qdf_assert(category >= 0 && category < QCA_TX_DELAY_NUM_CATEGORIES);

	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&pdev->tx_delay.mutex);
	index = 1 - pdev->tx_delay.cats[category].in_progress_idx;

	data = &pdev->tx_delay.cats[category].copies[index];

	if (data->avgs.transmit_num > 0) {
		avg_delay_ticks =
			ol_tx_delay_avg(data->avgs.transmit_sum_ticks,
					data->avgs.transmit_num);
		*tx_delay_microsec =
			qdf_system_ticks_to_msecs(avg_delay_ticks * 1000);
	} else {
		/*
		 * This case should only happen if there's a query
		 * within 5 sec after the first tx data frame.
		 */
		*tx_delay_microsec = 0;
	}
	if (data->avgs.queue_num > 0) {
		avg_delay_ticks =
			ol_tx_delay_avg(data->avgs.queue_sum_ticks,
					data->avgs.queue_num);
		*queue_delay_microsec =
			qdf_system_ticks_to_msecs(avg_delay_ticks * 1000);
	} else {
		/*
		 * This case should only happen if there's a query
		 * within 5 sec after the first tx data frame.
		 */
		*queue_delay_microsec = 0;
	}

	qdf_spin_unlock_bh(&pdev->tx_delay.mutex);
}

void
ol_tx_delay_hist(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		 uint16_t *report_bin_values, int category)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	int index, i, j;
	struct ol_tx_delay_data *data;

	qdf_assert(category >= 0 && category < QCA_TX_DELAY_NUM_CATEGORIES);

	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&pdev->tx_delay.mutex);
	index = 1 - pdev->tx_delay.cats[category].in_progress_idx;

	data = &pdev->tx_delay.cats[category].copies[index];

	for (i = 0, j = 0; i < QCA_TX_DELAY_HIST_REPORT_BINS - 1; i++) {
		uint16_t internal_bin_sum = 0;

		while (j < (1 << i))
			internal_bin_sum += data->hist_bins_queue[j++];

		report_bin_values[i] = internal_bin_sum;
	}
	report_bin_values[i] = data->hist_bins_queue[j];        /* overflow */

	qdf_spin_unlock_bh(&pdev->tx_delay.mutex);
}

#ifdef QCA_COMPUTE_TX_DELAY_PER_TID
static uint8_t
ol_tx_delay_tid_from_l3_hdr(struct ol_txrx_pdev_t *pdev,
			    qdf_nbuf_t msdu, struct ol_tx_desc_t *tx_desc)
{
	uint16_t ethertype;
	uint8_t *dest_addr, *l3_hdr;
	int is_mgmt, is_mcast;
	int l2_hdr_size;

	dest_addr = ol_tx_dest_addr_find(pdev, msdu);
	if (!dest_addr)
		return QDF_NBUF_TX_EXT_TID_INVALID;

	is_mcast = IEEE80211_IS_MULTICAST(dest_addr);
	is_mgmt = tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE;
	if (is_mgmt) {
		return (is_mcast) ?
		       OL_TX_NUM_TIDS + OL_TX_VDEV_DEFAULT_MGMT :
		       HTT_TX_EXT_TID_MGMT;
	}
	if (is_mcast)
		return OL_TX_NUM_TIDS + OL_TX_VDEV_MCAST_BCAST;

	if (pdev->frame_format == wlan_frm_fmt_802_3) {
		struct ethernet_hdr_t *enet_hdr;

		enet_hdr = (struct ethernet_hdr_t *)qdf_nbuf_data(msdu);
		l2_hdr_size = sizeof(struct ethernet_hdr_t);
		ethertype =
			(enet_hdr->ethertype[0] << 8) | enet_hdr->ethertype[1];
		if (!IS_ETHERTYPE(ethertype)) {
			struct llc_snap_hdr_t *llc_hdr;

			llc_hdr = (struct llc_snap_hdr_t *)
				  (qdf_nbuf_data(msdu) + l2_hdr_size);
			l2_hdr_size += sizeof(struct llc_snap_hdr_t);
			ethertype =
				(llc_hdr->ethertype[0] << 8) | llc_hdr->
				ethertype[1];
		}
	} else {
		struct llc_snap_hdr_t *llc_hdr;

		l2_hdr_size = sizeof(struct ieee80211_frame);
		llc_hdr = (struct llc_snap_hdr_t *)(qdf_nbuf_data(msdu)
						    + l2_hdr_size);
		l2_hdr_size += sizeof(struct llc_snap_hdr_t);
		ethertype =
			(llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
	}
	l3_hdr = qdf_nbuf_data(msdu) + l2_hdr_size;
	if (ETHERTYPE_IPV4 == ethertype) {
		return (((struct ipv4_hdr_t *)l3_hdr)->tos >> 5) & 0x7;
	} else if (ETHERTYPE_IPV6 == ethertype) {
		return (ipv6_traffic_class((struct ipv6_hdr_t *)l3_hdr) >> 5) &
		       0x7;
	} else {
		return QDF_NBUF_TX_EXT_TID_INVALID;
	}
}

static int ol_tx_delay_category(struct ol_txrx_pdev_t *pdev, uint16_t msdu_id)
{
	struct ol_tx_desc_t *tx_desc = ol_tx_desc_find(pdev, msdu_id);
	uint8_t tid;
	qdf_nbuf_t msdu = tx_desc->netbuf;

	tid = qdf_nbuf_get_tid(msdu);
	if (tid == QDF_NBUF_TX_EXT_TID_INVALID) {
		tid = ol_tx_delay_tid_from_l3_hdr(pdev, msdu, tx_desc);
		if (tid == QDF_NBUF_TX_EXT_TID_INVALID) {
			/*
			 * TID could not be determined
			 * (this is not an IP frame?)
			 */
			return -EINVAL;
		}
	}
	return tid;
}
#else
static int ol_tx_delay_category(struct ol_txrx_pdev_t *pdev, uint16_t msdu_id)
{
	return 0;
}
#endif

static inline int
ol_tx_delay_hist_bin(struct ol_txrx_pdev_t *pdev, uint32_t delay_ticks)
{
	int bin;
	/*
	 * For speed, multiply and shift to approximate a divide. This causes
	 * a small error, but the approximation error should be much less
	 * than the other uncertainties in the tx delay computation.
	 */
	bin = (delay_ticks * pdev->tx_delay.hist_internal_bin_width_mult) >>
	      pdev->tx_delay.hist_internal_bin_width_shift;
	if (bin >= QCA_TX_DELAY_HIST_INTERNAL_BINS)
		bin = QCA_TX_DELAY_HIST_INTERNAL_BINS - 1;

	return bin;
}

static void
ol_tx_delay_compute(struct ol_txrx_pdev_t *pdev,
		    enum htt_tx_status status,
		    uint16_t *desc_ids, int num_msdus)
{
	int i, index, cat;
	uint32_t now_ticks = qdf_system_ticks();
	uint32_t tx_delay_transmit_ticks, tx_delay_queue_ticks;
	uint32_t avg_time_ticks;
	struct ol_tx_delay_data *data;

	qdf_assert(num_msdus > 0);

	/*
	 * keep static counters for total packet and lost packets
	 * reset them in ol_tx_delay(), function used to fetch the stats
	 */

	cat = ol_tx_delay_category(pdev, desc_ids[0]);
	if (cat < 0 || cat >= QCA_TX_DELAY_NUM_CATEGORIES)
		return;

	pdev->packet_count[cat] = pdev->packet_count[cat] + num_msdus;
	if (status != htt_tx_status_ok) {
		for (i = 0; i < num_msdus; i++) {
			cat = ol_tx_delay_category(pdev, desc_ids[i]);
			if (cat < 0 || cat >= QCA_TX_DELAY_NUM_CATEGORIES)
				return;
			pdev->packet_loss_count[cat]++;
		}
		return;
	}

	/* since we may switch the ping-pong index, provide mutex w. readers */
	qdf_spin_lock_bh(&pdev->tx_delay.mutex);
	index = pdev->tx_delay.cats[cat].in_progress_idx;

	data = &pdev->tx_delay.cats[cat].copies[index];

	if (pdev->tx_delay.tx_compl_timestamp_ticks != 0) {
		tx_delay_transmit_ticks =
			now_ticks - pdev->tx_delay.tx_compl_timestamp_ticks;
		/*
		 * We'd like to account for the number of MSDUs that were
		 * transmitted together, but we don't know this.  All we know
		 * is the number of MSDUs that were acked together.
		 * Since the frame error rate is small, this is nearly the same
		 * as the number of frames transmitted together.
		 */
		data->avgs.transmit_sum_ticks += tx_delay_transmit_ticks;
		data->avgs.transmit_num += num_msdus;
	}
	pdev->tx_delay.tx_compl_timestamp_ticks = now_ticks;

	for (i = 0; i < num_msdus; i++) {
		int bin;
		uint16_t id = desc_ids[i];
		struct ol_tx_desc_t *tx_desc = ol_tx_desc_find(pdev, id);

		tx_delay_queue_ticks =
			now_ticks - tx_desc->entry_timestamp_ticks;

		data->avgs.queue_sum_ticks += tx_delay_queue_ticks;
		data->avgs.queue_num++;
		bin = ol_tx_delay_hist_bin(pdev, tx_delay_queue_ticks);
		data->hist_bins_queue[bin]++;
	}

	/* check if it's time to start a new average */
	avg_time_ticks =
		now_ticks - pdev->tx_delay.cats[cat].avg_start_time_ticks;
	if (avg_time_ticks > pdev->tx_delay.avg_period_ticks) {
		pdev->tx_delay.cats[cat].avg_start_time_ticks = now_ticks;
		index = 1 - index;
		pdev->tx_delay.cats[cat].in_progress_idx = index;
		qdf_mem_zero(&pdev->tx_delay.cats[cat].copies[index],
			     sizeof(pdev->tx_delay.cats[cat].copies[index]));
	}

	qdf_spin_unlock_bh(&pdev->tx_delay.mutex);
}

#endif /* QCA_COMPUTE_TX_DELAY */

#ifdef WLAN_FEATURE_TSF_PLUS
void ol_register_timestamp_callback(tp_ol_timestamp_cb ol_tx_timestamp_cb)
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
	pdev->ol_tx_timestamp_cb = ol_tx_timestamp_cb;
}

void ol_deregister_timestamp_callback(void)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}
	pdev->ol_tx_timestamp_cb = NULL;
}
#endif
