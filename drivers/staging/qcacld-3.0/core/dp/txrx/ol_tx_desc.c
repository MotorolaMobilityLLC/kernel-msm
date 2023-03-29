/*
 * Copyright (c) 2011, 2014-2019 The Linux Foundation. All rights reserved.
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

#include <qdf_net_types.h>      /* QDF_NBUF_EXEMPT_NO_EXEMPTION, etc. */
#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_util.h>           /* qdf_assert */
#include <qdf_lock.h>           /* qdf_spinlock */
#include <qdf_trace.h>          /* qdf_tso_seg_dbg stuff */
#ifdef QCA_COMPUTE_TX_DELAY
#include <qdf_time.h>           /* qdf_system_ticks */
#endif

#include <ol_htt_tx_api.h>      /* htt_tx_desc_id */

#include <ol_tx_desc.h>
#include <ol_txrx_internal.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>      /* OL_TX_RESTORE_HDR, etc */
#endif
#include <ol_txrx.h>

#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
static inline void ol_tx_desc_sanity_checks(struct ol_txrx_pdev_t *pdev,
					struct ol_tx_desc_t *tx_desc)
{
	if (tx_desc->pkt_type != ol_tx_frm_freed) {
		ol_txrx_err("Potential tx_desc corruption pkt_type:0x%x pdev:0x%pK",
			    tx_desc->pkt_type, pdev);
		qdf_assert(0);
	}
}
static inline void ol_tx_desc_reset_pkt_type(struct ol_tx_desc_t *tx_desc)
{
	tx_desc->pkt_type = ol_tx_frm_freed;
}
#ifdef QCA_COMPUTE_TX_DELAY
static inline void ol_tx_desc_compute_delay(struct ol_tx_desc_t *tx_desc)
{
	if (tx_desc->entry_timestamp_ticks != 0xffffffff) {
		ol_txrx_err("Timestamp:0x%x", tx_desc->entry_timestamp_ticks);
		qdf_assert(0);
	}
	tx_desc->entry_timestamp_ticks = qdf_system_ticks();
}
static inline void ol_tx_desc_reset_timestamp(struct ol_tx_desc_t *tx_desc)
{
	tx_desc->entry_timestamp_ticks = 0xffffffff;
}
#endif
#else
static inline void ol_tx_desc_sanity_checks(struct ol_txrx_pdev_t *pdev,
						struct ol_tx_desc_t *tx_desc)
{
}
static inline void ol_tx_desc_reset_pkt_type(struct ol_tx_desc_t *tx_desc)
{
}
static inline void ol_tx_desc_compute_delay(struct ol_tx_desc_t *tx_desc)
{
}
static inline void ol_tx_desc_reset_timestamp(struct ol_tx_desc_t *tx_desc)
{
}
#endif

#ifdef DESC_TIMESTAMP_DEBUG_INFO
static inline void ol_tx_desc_update_tx_ts(struct ol_tx_desc_t *tx_desc)
{
	tx_desc->desc_debug_info.prev_tx_ts = tx_desc
						->desc_debug_info.curr_tx_ts;
	tx_desc->desc_debug_info.curr_tx_ts = qdf_get_log_timestamp();
}
#else
static inline void ol_tx_desc_update_tx_ts(struct ol_tx_desc_t *tx_desc)
{
}
#endif

/**
 * ol_tx_desc_vdev_update() - vedv assign.
 * @tx_desc: tx descriptor pointer
 * @vdev: vdev handle
 *
 * Return: None
 */
static inline void
ol_tx_desc_vdev_update(struct ol_tx_desc_t *tx_desc,
		       struct ol_txrx_vdev_t *vdev)
{
	tx_desc->vdev = vdev;
	tx_desc->vdev_id = vdev->vdev_id;
}

#ifdef QCA_HL_NETDEV_FLOW_CONTROL

/**
 * ol_tx_desc_count_inc() - tx desc count increment for desc allocation.
 * @vdev: vdev handle
 *
 * Return: None
 */
static inline void
ol_tx_desc_count_inc(struct ol_txrx_vdev_t *vdev)
{
	qdf_atomic_inc(&vdev->tx_desc_count);
}
#else

static inline void
ol_tx_desc_count_inc(struct ol_txrx_vdev_t *vdev)
{
}

#endif

#ifndef QCA_LL_TX_FLOW_CONTROL_V2
#ifdef QCA_LL_PDEV_TX_FLOW_CONTROL
/**
 * ol_tx_do_pdev_flow_control_pause - pause queues when stop_th reached.
 * @pdev: pdev handle
 *
 * Return: void
 */
static void ol_tx_do_pdev_flow_control_pause(struct ol_txrx_pdev_t *pdev)
{
	struct ol_txrx_vdev_t *vdev;

	if (qdf_unlikely(pdev->tx_desc.num_free <
				pdev->tx_desc.stop_th &&
			pdev->tx_desc.num_free >=
			 pdev->tx_desc.stop_priority_th &&
			pdev->tx_desc.status ==
			 FLOW_POOL_ACTIVE_UNPAUSED)) {
		pdev->tx_desc.status = FLOW_POOL_NON_PRIO_PAUSED;
		/* pause network NON PRIORITY queues */
		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_STOP_NON_PRIORITY_QUEUE,
				       WLAN_DATA_FLOW_CONTROL);
		}
	} else if (qdf_unlikely((pdev->tx_desc.num_free <
				 pdev->tx_desc.stop_priority_th) &&
			pdev->tx_desc.status ==
			FLOW_POOL_NON_PRIO_PAUSED)) {
		pdev->tx_desc.status = FLOW_POOL_ACTIVE_PAUSED;
		/* pause priority queue */
		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_NETIF_PRIORITY_QUEUE_OFF,
				       WLAN_DATA_FLOW_CONTROL_PRIORITY);
		}
	}
}

/**
 * ol_tx_do_pdev_flow_control_unpause - unpause queues when start_th restored.
 * @pdev: pdev handle
 *
 * Return: void
 */
static void ol_tx_do_pdev_flow_control_unpause(struct ol_txrx_pdev_t *pdev)
{
	struct ol_txrx_vdev_t *vdev;

	switch (pdev->tx_desc.status) {
	case FLOW_POOL_ACTIVE_PAUSED:
		if (pdev->tx_desc.num_free >
		    pdev->tx_desc.start_priority_th) {
			/* unpause priority queue */
			TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
				pdev->pause_cb(vdev->vdev_id,
				       WLAN_NETIF_PRIORITY_QUEUE_ON,
				       WLAN_DATA_FLOW_CONTROL_PRIORITY);
			}
			pdev->tx_desc.status = FLOW_POOL_NON_PRIO_PAUSED;
		}
		break;
	case FLOW_POOL_NON_PRIO_PAUSED:
		if (pdev->tx_desc.num_free > pdev->tx_desc.start_th) {
			TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
				pdev->pause_cb(vdev->vdev_id,
					       WLAN_WAKE_NON_PRIORITY_QUEUE,
					       WLAN_DATA_FLOW_CONTROL);
			}
			pdev->tx_desc.status = FLOW_POOL_ACTIVE_UNPAUSED;
		}
		break;
	case FLOW_POOL_INVALID:
		if (pdev->tx_desc.num_free == pdev->tx_desc.pool_size)
			ol_txrx_err("pool is INVALID State!!");
		break;
	case FLOW_POOL_ACTIVE_UNPAUSED:
		break;
	default:
		ol_txrx_err("pool is INACTIVE State!!\n");
		break;
	};
}
#else
static inline void
ol_tx_do_pdev_flow_control_pause(struct ol_txrx_pdev_t *pdev)
{
}

static inline void
ol_tx_do_pdev_flow_control_unpause(struct ol_txrx_pdev_t *pdev)
{
}
#endif
/**
 * ol_tx_desc_alloc() - allocate descriptor from freelist
 * @pdev: pdev handle
 * @vdev: vdev handle
 *
 * Return: tx descriptor pointer/ NULL in case of error
 */
static
struct ol_tx_desc_t *ol_tx_desc_alloc(struct ol_txrx_pdev_t *pdev,
					     struct ol_txrx_vdev_t *vdev)
{
	struct ol_tx_desc_t *tx_desc = NULL;

	qdf_spin_lock_bh(&pdev->tx_mutex);
	if (pdev->tx_desc.freelist) {
		tx_desc = ol_tx_get_desc_global_pool(pdev);
		if (!tx_desc) {
			qdf_spin_unlock_bh(&pdev->tx_mutex);
			return NULL;
		}
		ol_tx_desc_dup_detect_set(pdev, tx_desc);
		ol_tx_do_pdev_flow_control_pause(pdev);
		ol_tx_desc_sanity_checks(pdev, tx_desc);
		ol_tx_desc_compute_delay(tx_desc);
		ol_tx_desc_vdev_update(tx_desc, vdev);
		ol_tx_desc_count_inc(vdev);
		ol_tx_desc_update_tx_ts(tx_desc);
		qdf_atomic_inc(&tx_desc->ref_cnt);
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);
	return tx_desc;
}

/**
 * ol_tx_desc_alloc_wrapper() -allocate tx descriptor
 * @pdev: pdev handler
 * @vdev: vdev handler
 * @msdu_info: msdu handler
 *
 * Return: tx descriptor or NULL
 */
struct ol_tx_desc_t *
ol_tx_desc_alloc_wrapper(struct ol_txrx_pdev_t *pdev,
			 struct ol_txrx_vdev_t *vdev,
			 struct ol_txrx_msdu_info_t *msdu_info)
{
	return ol_tx_desc_alloc(pdev, vdev);
}

#else
/**
 * ol_tx_desc_alloc() -allocate tx descriptor
 * @pdev: pdev handler
 * @vdev: vdev handler
 * @pool: flow pool
 *
 * Return: tx descriptor or NULL
 */
static
struct ol_tx_desc_t *ol_tx_desc_alloc(struct ol_txrx_pdev_t *pdev,
				      struct ol_txrx_vdev_t *vdev,
				      struct ol_tx_flow_pool_t *pool)
{
	struct ol_tx_desc_t *tx_desc = NULL;

	if (!pool) {
		pdev->pool_stats.pkt_drop_no_pool++;
		goto end;
	}

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->avail_desc) {
		tx_desc = ol_tx_get_desc_flow_pool(pool);
		ol_tx_desc_dup_detect_set(pdev, tx_desc);
		if (qdf_unlikely(pool->avail_desc < pool->stop_th &&
				(pool->avail_desc >= pool->stop_priority_th) &&
				(pool->status == FLOW_POOL_ACTIVE_UNPAUSED))) {
			pool->status = FLOW_POOL_NON_PRIO_PAUSED;
			/* pause network NON PRIORITY queues */
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_STOP_NON_PRIORITY_QUEUE,
				       WLAN_DATA_FLOW_CONTROL);
		} else if (qdf_unlikely((pool->avail_desc <
						pool->stop_priority_th) &&
				pool->status == FLOW_POOL_NON_PRIO_PAUSED)) {
			pool->status = FLOW_POOL_ACTIVE_PAUSED;
			/* pause priority queue */
			pdev->pause_cb(vdev->vdev_id,
				       WLAN_NETIF_PRIORITY_QUEUE_OFF,
				       WLAN_DATA_FLOW_CONTROL_PRIORITY);
		}

		qdf_spin_unlock_bh(&pool->flow_pool_lock);

		ol_tx_desc_sanity_checks(pdev, tx_desc);
		ol_tx_desc_compute_delay(tx_desc);
		ol_tx_desc_update_tx_ts(tx_desc);
		ol_tx_desc_vdev_update(tx_desc, vdev);
		qdf_atomic_inc(&tx_desc->ref_cnt);
	} else {
		pool->pkt_drop_no_desc++;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	}

end:
	return tx_desc;
}

/**
 * ol_tx_desc_alloc_wrapper() -allocate tx descriptor
 * @pdev: pdev handler
 * @vdev: vdev handler
 * @msdu_info: msdu handler
 *
 * Return: tx descriptor or NULL
 */
#ifdef QCA_LL_TX_FLOW_GLOBAL_MGMT_POOL
struct ol_tx_desc_t *
ol_tx_desc_alloc_wrapper(struct ol_txrx_pdev_t *pdev,
			 struct ol_txrx_vdev_t *vdev,
			 struct ol_txrx_msdu_info_t *msdu_info)
{
	if (qdf_unlikely(msdu_info->htt.info.frame_type == htt_pkt_type_mgmt))
		return ol_tx_desc_alloc(pdev, vdev, pdev->mgmt_pool);
	else
		return ol_tx_desc_alloc(pdev, vdev, vdev->pool);
}
#else
struct ol_tx_desc_t *
ol_tx_desc_alloc_wrapper(struct ol_txrx_pdev_t *pdev,
			 struct ol_txrx_vdev_t *vdev,
			 struct ol_txrx_msdu_info_t *msdu_info)
{
	return ol_tx_desc_alloc(pdev, vdev, vdev->pool);
}
#endif
#endif

/**
 * ol_tx_desc_alloc_hl() - allocate tx descriptor
 * @pdev: pdev handle
 * @vdev: vdev handle
 * @msdu_info: tx msdu info
 *
 * Return: tx descriptor pointer/ NULL in case of error
 */
static struct ol_tx_desc_t *
ol_tx_desc_alloc_hl(struct ol_txrx_pdev_t *pdev,
		    struct ol_txrx_vdev_t *vdev,
		    struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc;

	tx_desc = ol_tx_desc_alloc_wrapper(pdev, vdev, msdu_info);
	if (!tx_desc)
		return NULL;

	qdf_atomic_dec(&pdev->tx_queue.rsrc_cnt);

	return tx_desc;
}

#ifdef QCA_HL_NETDEV_FLOW_CONTROL

/**
 * ol_tx_desc_vdev_rm() - decrement the tx desc count for vdev.
 * @tx_desc: tx desc
 *
 * Return: None
 */
static inline void
ol_tx_desc_vdev_rm(struct ol_tx_desc_t *tx_desc)
{
	/*
	 * In module exit context, vdev handle could be destroyed but still
	 * we need to free pending completion tx_desc.
	 */
	if (!tx_desc || !tx_desc->vdev)
		return;

	qdf_atomic_dec(&tx_desc->vdev->tx_desc_count);
	tx_desc->vdev = NULL;
}
#else

static inline void
ol_tx_desc_vdev_rm(struct ol_tx_desc_t *tx_desc)
{
}
#endif

#ifdef FEATURE_TSO
/**
 * ol_tso_unmap_tso_segment() - Unmap TSO segment
 * @pdev: pointer to ol_txrx_pdev_t structure
 * @tx_desc: pointer to ol_tx_desc_t containing the TSO segment
 *
 * Unmap TSO segment (frag[1]). If it is the last TSO segment corresponding the
 * nbuf, also unmap the EIT header(frag[0]).
 *
 * Return: None
 */
static void ol_tso_unmap_tso_segment(struct ol_txrx_pdev_t *pdev,
						struct ol_tx_desc_t *tx_desc)
{
	bool is_last_seg = false;
	struct qdf_tso_num_seg_elem_t *tso_num_desc = NULL;

	if (qdf_unlikely(!tx_desc->tso_desc)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s %d TSO desc is NULL!",
			  __func__, __LINE__);
		qdf_assert(0);
		return;
	} else if (qdf_unlikely(!tx_desc->tso_num_desc)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s %d TSO common info is NULL!",
			  __func__, __LINE__);
		qdf_assert(0);
		return;
	}

	tso_num_desc = tx_desc->tso_num_desc;

	qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);

	tso_num_desc->num_seg.tso_cmn_num_seg--;
	is_last_seg = (tso_num_desc->num_seg.tso_cmn_num_seg == 0) ?
								true : false;
	qdf_nbuf_unmap_tso_segment(pdev->osdev, tx_desc->tso_desc, is_last_seg);

	qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);

}

/**
 * ol_tx_tso_desc_free() - Add TSO TX descs back to the freelist
 * @pdev: pointer to ol_txrx_pdev_t structure
 * @tx_desc: pointer to ol_tx_desc_t containing the TSO segment
 *
 * Add qdf_tso_seg_elem_t corresponding to the TSO seg back to freelist.
 * If it is the last segment of the jumbo skb, also add the
 * qdf_tso_num_seg_elem_t to the free list.
 *
 * Return: None
 */
static void ol_tx_tso_desc_free(struct ol_txrx_pdev_t *pdev,
				struct ol_tx_desc_t *tx_desc)
{
	bool is_last_seg;
	struct qdf_tso_num_seg_elem_t *tso_num_desc = tx_desc->tso_num_desc;

	is_last_seg = (tso_num_desc->num_seg.tso_cmn_num_seg == 0) ?
								true : false;
	if (is_last_seg) {
		ol_tso_num_seg_free(pdev, tx_desc->tso_num_desc);
		tx_desc->tso_num_desc = NULL;
	}

	ol_tso_free_segment(pdev, tx_desc->tso_desc);
	tx_desc->tso_desc = NULL;
}

#else
static inline void ol_tx_tso_desc_free(struct ol_txrx_pdev_t *pdev,
				       struct ol_tx_desc_t *tx_desc)
{
}

static inline void ol_tso_unmap_tso_segment(
					struct ol_txrx_pdev_t *pdev,
					struct ol_tx_desc_t *tx_desc)
{
}
#endif

/**
 * ol_tx_desc_free_common() - common funcs to free tx_desc for all flow ctl vers
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Set of common functions needed for QCA_LL_TX_FLOW_CONTROL_V2 and older
 * versions of flow control. Needs to be called from within a spinlock.
 *
 * Return: None
 */
static void ol_tx_desc_free_common(struct ol_txrx_pdev_t *pdev,
						struct ol_tx_desc_t *tx_desc)
{
	ol_tx_desc_dup_detect_reset(pdev, tx_desc);

	if (tx_desc->pkt_type == OL_TX_FRM_TSO)
		ol_tx_tso_desc_free(pdev, tx_desc);

	ol_tx_desc_reset_pkt_type(tx_desc);
	ol_tx_desc_reset_timestamp(tx_desc);
	/* clear the ref cnt */
	qdf_atomic_init(&tx_desc->ref_cnt);
	tx_desc->vdev_id = OL_TXRX_INVALID_VDEV_ID;
}

#ifndef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * ol_tx_desc_free() - put descriptor to freelist
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Return: None
 */
void ol_tx_desc_free(struct ol_txrx_pdev_t *pdev, struct ol_tx_desc_t *tx_desc)
{
	qdf_spin_lock_bh(&pdev->tx_mutex);

	ol_tx_desc_free_common(pdev, tx_desc);

	ol_tx_put_desc_global_pool(pdev, tx_desc);
	ol_tx_desc_vdev_rm(tx_desc);
	ol_tx_do_pdev_flow_control_unpause(pdev);

	qdf_spin_unlock_bh(&pdev->tx_mutex);
}

#else

/**
 * ol_tx_update_free_desc_to_pool() - update free desc to pool
 * @pdev: pdev handle
 * @tx_desc: descriptor
 *
 * Return : 1 desc distribution required / 0 don't need distribution
 */
#ifdef QCA_LL_TX_FLOW_CONTROL_RESIZE
static inline bool ol_tx_update_free_desc_to_pool(struct ol_txrx_pdev_t *pdev,
						  struct ol_tx_desc_t *tx_desc)
{
	struct ol_tx_flow_pool_t *pool = tx_desc->pool;
	bool distribute_desc = false;

	if (unlikely(pool->overflow_desc)) {
		ol_tx_put_desc_global_pool(pdev, tx_desc);
		--pool->overflow_desc;
		distribute_desc = true;
	} else {
		ol_tx_put_desc_flow_pool(pool, tx_desc);
	}

	return distribute_desc;
}
#else
static inline bool ol_tx_update_free_desc_to_pool(struct ol_txrx_pdev_t *pdev,
						  struct ol_tx_desc_t *tx_desc)
{
	ol_tx_put_desc_flow_pool(tx_desc->pool, tx_desc);
	return false;
}
#endif

/**
 * ol_tx_desc_free() - put descriptor to pool freelist
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Return: None
 */
void ol_tx_desc_free(struct ol_txrx_pdev_t *pdev, struct ol_tx_desc_t *tx_desc)
{
	bool distribute_desc = false;
	struct ol_tx_flow_pool_t *pool = tx_desc->pool;

	qdf_spin_lock_bh(&pool->flow_pool_lock);

	ol_tx_desc_free_common(pdev, tx_desc);
	distribute_desc = ol_tx_update_free_desc_to_pool(pdev, tx_desc);

	switch (pool->status) {
	case FLOW_POOL_ACTIVE_PAUSED:
		if (pool->avail_desc > pool->start_priority_th) {
			/* unpause priority queue */
			pdev->pause_cb(pool->member_flow_id,
			       WLAN_NETIF_PRIORITY_QUEUE_ON,
			       WLAN_DATA_FLOW_CONTROL_PRIORITY);
			pool->status = FLOW_POOL_NON_PRIO_PAUSED;
		}
		break;
	case FLOW_POOL_NON_PRIO_PAUSED:
		if (pool->avail_desc > pool->start_th) {
			pdev->pause_cb(pool->member_flow_id,
				       WLAN_WAKE_NON_PRIORITY_QUEUE,
				       WLAN_DATA_FLOW_CONTROL);
			pool->status = FLOW_POOL_ACTIVE_UNPAUSED;
		}
		break;
	case FLOW_POOL_INVALID:
		if (pool->avail_desc == pool->flow_pool_size) {
			qdf_spin_unlock_bh(&pool->flow_pool_lock);
			ol_tx_free_invalid_flow_pool(pool);
			qdf_print("pool is INVALID State!!");
			return;
		}
		break;
	case FLOW_POOL_ACTIVE_UNPAUSED:
		break;
	default:
		qdf_print("pool is INACTIVE State!!");
		break;
	};

	qdf_spin_unlock_bh(&pool->flow_pool_lock);

	if (unlikely(distribute_desc))
		ol_tx_distribute_descs_to_deficient_pools_from_global_pool();

}
#endif

const uint32_t htt_to_ce_pkt_type[] = {
	[htt_pkt_type_raw] = tx_pkt_type_raw,
	[htt_pkt_type_native_wifi] = tx_pkt_type_native_wifi,
	[htt_pkt_type_ethernet] = tx_pkt_type_802_3,
	[htt_pkt_type_mgmt] = tx_pkt_type_mgmt,
	[htt_pkt_type_eth2] = tx_pkt_type_eth2,
	[htt_pkt_num_types] = 0xffffffff
};

#define WISA_DEST_PORT_6MBPS	50000
#define WISA_DEST_PORT_24MBPS	50001

/**
 * ol_tx_get_wisa_ext_hdr_type() - get header type for WiSA mode
 * @netbuf: network buffer
 *
 * Return: extension header type
 */
static enum extension_header_type
ol_tx_get_wisa_ext_hdr_type(qdf_nbuf_t netbuf)
{
	uint8_t *buf = qdf_nbuf_data(netbuf);
	uint16_t dport;

	if (qdf_is_macaddr_group(
		(struct qdf_mac_addr *)(buf + QDF_NBUF_DEST_MAC_OFFSET))) {

		dport = (uint16_t)(*(uint16_t *)(buf +
			QDF_NBUF_TRAC_IPV4_OFFSET +
			QDF_NBUF_TRAC_IPV4_HEADER_SIZE + sizeof(uint16_t)));

		if (dport == QDF_SWAP_U16(WISA_DEST_PORT_6MBPS))
			return WISA_MODE_EXT_HEADER_6MBPS;
		else if (dport == QDF_SWAP_U16(WISA_DEST_PORT_24MBPS))
			return WISA_MODE_EXT_HEADER_24MBPS;
		else
			return EXT_HEADER_NOT_PRESENT;
	} else {
		return EXT_HEADER_NOT_PRESENT;
	}
}

/**
 * ol_tx_get_ext_header_type() - extension header is required or not
 * @vdev: vdev pointer
 * @netbuf: network buffer
 *
 * This function returns header type and if extension header is
 * not required than returns EXT_HEADER_NOT_PRESENT.
 *
 * Return: extension header type
 */
enum extension_header_type
ol_tx_get_ext_header_type(struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t netbuf)
{
	if (vdev->is_wisa_mode_enable == true)
		return ol_tx_get_wisa_ext_hdr_type(netbuf);
	else
		return EXT_HEADER_NOT_PRESENT;
}

struct ol_tx_desc_t *ol_tx_desc_ll(struct ol_txrx_pdev_t *pdev,
				   struct ol_txrx_vdev_t *vdev,
				   qdf_nbuf_t netbuf,
				   struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc;
	unsigned int i;
	uint32_t num_frags;
	enum extension_header_type type;

	msdu_info->htt.info.vdev_id = vdev->vdev_id;
	msdu_info->htt.action.cksum_offload = qdf_nbuf_get_tx_cksum(netbuf);
	switch (qdf_nbuf_get_exemption_type(netbuf)) {
	case QDF_NBUF_EXEMPT_NO_EXEMPTION:
	case QDF_NBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
		/* We want to encrypt this frame */
		msdu_info->htt.action.do_encrypt = 1;
		break;
	case QDF_NBUF_EXEMPT_ALWAYS:
		/* We don't want to encrypt this frame */
		msdu_info->htt.action.do_encrypt = 0;
		break;
	default:
		qdf_assert(0);
		break;
	}

	/* allocate the descriptor */
	tx_desc = ol_tx_desc_alloc_wrapper(pdev, vdev, msdu_info);
	if (!tx_desc)
		return NULL;

	/* initialize the SW tx descriptor */
	tx_desc->netbuf = netbuf;

	if (msdu_info->tso_info.is_tso) {
		tx_desc->tso_desc = msdu_info->tso_info.curr_seg;
		tx_desc->tso_num_desc = msdu_info->tso_info.tso_num_seg_list;
		tx_desc->pkt_type = OL_TX_FRM_TSO;
		TXRX_STATS_MSDU_INCR(pdev, tx.tso.tso_pkts, netbuf);
	} else {
		tx_desc->pkt_type = OL_TX_FRM_STD;
	}

	type = ol_tx_get_ext_header_type(vdev, netbuf);

	/* initialize the HW tx descriptor */
	if (qdf_unlikely(htt_tx_desc_init(pdev->htt_pdev, tx_desc->htt_tx_desc,
			 tx_desc->htt_tx_desc_paddr,
			 ol_tx_desc_id(pdev, tx_desc), netbuf, &msdu_info->htt,
			 &msdu_info->tso_info, NULL, type))) {
		/*
		 * HTT Tx descriptor initialization failed.
		 * therefore, free the tx desc
		 */
		ol_tx_desc_free(pdev, tx_desc);
		return NULL;
	}

	/*
	 * Initialize the fragmentation descriptor.
	 * Skip the prefix fragment (HTT tx descriptor) that was added
	 * during the call to htt_tx_desc_init above.
	 */
	num_frags = qdf_nbuf_get_num_frags(netbuf);
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
			 tx_desc->htt_frag_desc, &msdu_info->tso_info);
		TXRX_STATS_TSO_SEG_UPDATE(pdev,
			 msdu_info->tso_info.msdu_stats_idx,
			 msdu_info->tso_info.curr_seg->seg);
	} else {
		for (i = 1; i < num_frags; i++) {
			qdf_size_t frag_len;
			qdf_dma_addr_t frag_paddr;
#ifdef HELIUMPLUS_DEBUG
			void *frag_vaddr;

			frag_vaddr = qdf_nbuf_get_frag_vaddr(netbuf, i);
#endif
			frag_len = qdf_nbuf_get_frag_len(netbuf, i);
			frag_paddr = qdf_nbuf_get_frag_paddr(netbuf, i);
#if defined(HELIUMPLUS)
			htt_tx_desc_frag(pdev->htt_pdev, tx_desc->htt_frag_desc,
					 i - 1, frag_paddr, frag_len);
#if defined(HELIUMPLUS_DEBUG)
			qdf_debug("htt_fdesc=%pK frag=%d frag_vaddr=0x%pK frag_paddr=0x%llx len=%zu\n",
				  tx_desc->htt_frag_desc,
				  i-1, frag_vaddr, frag_paddr, frag_len);
			ol_txrx_dump_pkt(netbuf, frag_paddr, 64);
#endif /* HELIUMPLUS_DEBUG */
#else /* ! defined(HELIUMPLUS) */
			htt_tx_desc_frag(pdev->htt_pdev, tx_desc->htt_tx_desc,
					 i - 1, frag_paddr, frag_len);
#endif /* defined(HELIUMPLUS) */
		}
	}

#if defined(HELIUMPLUS_DEBUG)
	ol_txrx_dump_frag_desc("ol_tx_desc_ll()", tx_desc);
#endif
	return tx_desc;
}

struct ol_tx_desc_t *
ol_tx_desc_hl(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t netbuf,
	struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc;

	/* FIX THIS: these inits should probably be done by tx classify */
	msdu_info->htt.info.vdev_id = vdev->vdev_id;
	msdu_info->htt.info.frame_type = pdev->htt_pkt_type;
	msdu_info->htt.action.cksum_offload = qdf_nbuf_get_tx_cksum(netbuf);
	switch (qdf_nbuf_get_exemption_type(netbuf)) {
	case QDF_NBUF_EXEMPT_NO_EXEMPTION:
	case QDF_NBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
		/* We want to encrypt this frame */
		msdu_info->htt.action.do_encrypt = 1;
		break;
	case QDF_NBUF_EXEMPT_ALWAYS:
		/* We don't want to encrypt this frame */
		msdu_info->htt.action.do_encrypt = 0;
		break;
	default:
		qdf_assert(0);
		break;
	}

	/* allocate the descriptor */
	tx_desc = ol_tx_desc_alloc_hl(pdev, vdev, msdu_info);
	if (!tx_desc)
		return NULL;

	/* initialize the SW tx descriptor */
	tx_desc->netbuf = netbuf;
	/* fix this - get pkt_type from msdu_info */
	tx_desc->pkt_type = OL_TX_FRM_STD;

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
	tx_desc->orig_l2_hdr_bytes = 0;
#endif
	/* the HW tx descriptor will be initialized later by the caller */

	return tx_desc;
}

void ol_tx_desc_frame_list_free(struct ol_txrx_pdev_t *pdev,
				ol_tx_desc_list *tx_descs, int had_error)
{
	struct ol_tx_desc_t *tx_desc, *tmp;
	qdf_nbuf_t msdus = NULL;

	TAILQ_FOREACH_SAFE(tx_desc, tx_descs, tx_desc_list_elem, tmp) {
		qdf_nbuf_t msdu = tx_desc->netbuf;

		qdf_atomic_init(&tx_desc->ref_cnt);   /* clear the ref cnt */
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
		/* restore original hdr offset */
		OL_TX_RESTORE_HDR(tx_desc, msdu);
#endif

		/*
		 * In MCC IPA tx context, IPA driver provides skb with directly
		 * DMA mapped address. In such case, there's no need for WLAN
		 * driver to DMA unmap the skb.
		 */
		if (qdf_nbuf_get_users(msdu) <= 1) {
			if (!qdf_nbuf_ipa_owned_get(msdu))
				qdf_nbuf_unmap(pdev->osdev, msdu,
					       QDF_DMA_TO_DEVICE);
		}

		/* free the tx desc */
		ol_tx_desc_free(pdev, tx_desc);
		/* link the netbuf into a list to free as a batch */
		qdf_nbuf_set_next(msdu, msdus);
		msdus = msdu;
	}
	/* free the netbufs as a batch */
	qdf_nbuf_tx_free(msdus, had_error);
}

void ol_tx_desc_frame_free_nonstd(struct ol_txrx_pdev_t *pdev,
				  struct ol_tx_desc_t *tx_desc, int had_error)
{
	int mgmt_type;
	ol_txrx_mgmt_tx_cb ota_ack_cb;

	qdf_atomic_init(&tx_desc->ref_cnt);     /* clear the ref cnt */
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
	/* restore original hdr offset */
	OL_TX_RESTORE_HDR(tx_desc, (tx_desc->netbuf));
#endif
	if (tx_desc->pkt_type == OL_TX_FRM_NO_FREE) {

		/* free the tx desc but don't unmap or free the frame */
		if (pdev->tx_data_callback.func) {
			qdf_nbuf_set_next(tx_desc->netbuf, NULL);
			pdev->tx_data_callback.func(pdev->tx_data_callback.ctxt,
						    tx_desc->netbuf, had_error);
			goto free_tx_desc;
		}
		/* let the code below unmap and free the frame */
	}
	if (tx_desc->pkt_type == OL_TX_FRM_TSO)
		ol_tso_unmap_tso_segment(pdev, tx_desc);
	else
		qdf_nbuf_unmap(pdev->osdev, tx_desc->netbuf, QDF_DMA_TO_DEVICE);
	/* check the frame type to see what kind of special steps are needed */
	if ((tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE) &&
		   (tx_desc->pkt_type != ol_tx_frm_freed)) {
		qdf_dma_addr_t frag_desc_paddr = 0;

#if defined(HELIUMPLUS)
		frag_desc_paddr = tx_desc->htt_frag_desc_paddr;
		/* FIX THIS -
		 * The FW currently has trouble using the host's fragments
		 * table for management frames.  Until this is fixed,
		 * rather than specifying the fragment table to the FW,
		 * the host SW will specify just the address of the initial
		 * fragment.
		 * Now that the mgmt frame is done, the HTT tx desc's frags
		 * table pointer needs to be reset.
		 */
#if defined(HELIUMPLUS_DEBUG)
		qdf_print("Frag Descriptor Reset [%d] to 0x%x\n",
			  tx_desc->id,
			  frag_desc_paddr);
#endif /* HELIUMPLUS_DEBUG */
#endif /* HELIUMPLUS */
		htt_tx_desc_frags_table_set(pdev->htt_pdev,
					    tx_desc->htt_tx_desc, 0,
					    frag_desc_paddr, 1);

		mgmt_type = tx_desc->pkt_type - OL_TXRX_MGMT_TYPE_BASE;
		/*
		 *  we already checked the value when the mgmt frame was
		 *  provided to the txrx layer.
		 *  no need to check it a 2nd time.
		 */
		ota_ack_cb = pdev->tx_mgmt_cb.ota_ack_cb;
		if (ota_ack_cb) {
			void *ctxt;
			ctxt = pdev->tx_mgmt_cb.ctxt;
			ota_ack_cb(ctxt, tx_desc->netbuf, had_error);
		}
	} else if (had_error == htt_tx_status_download_fail) {
		/* Failed to send to target */
		goto free_tx_desc;
	} else {
		/* single regular frame, called from completion path */
		qdf_nbuf_set_next(tx_desc->netbuf, NULL);
		qdf_nbuf_tx_free(tx_desc->netbuf, had_error);
	}
free_tx_desc:
	/* free the tx desc */
	ol_tx_desc_free(pdev, tx_desc);
}

#if defined(FEATURE_TSO)
#ifdef TSOSEG_DEBUG
static int
ol_tso_seg_dbg_sanitize(struct qdf_tso_seg_elem_t *tsoseg)
{
	int rc = -1;
	struct ol_tx_desc_t *txdesc;

	if (tsoseg) {
		txdesc = tsoseg->dbg.txdesc;
		/* Don't validate if TX desc is NULL*/
		if (!txdesc)
			return 0;
		if (txdesc->tso_desc != tsoseg)
			qdf_tso_seg_dbg_bug("Owner sanity failed");
		else
			rc = 0;
	}
	return rc;

};
#else
static int
ol_tso_seg_dbg_sanitize(struct qdf_tso_seg_elem_t *tsoseg)
{
	return 0;
}
#endif /* TSOSEG_DEBUG */

/**
 * ol_tso_alloc_segment() - function to allocate a TSO segment
 * element
 * @pdev: the data physical device sending the data
 *
 * Allocates a TSO segment element from the free list held in
 * the pdev
 *
 * Return: tso_seg
 */
struct qdf_tso_seg_elem_t *ol_tso_alloc_segment(struct ol_txrx_pdev_t *pdev)
{
	struct qdf_tso_seg_elem_t *tso_seg = NULL;

	qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);
	if (pdev->tso_seg_pool.freelist) {
		pdev->tso_seg_pool.num_free--;
		tso_seg = pdev->tso_seg_pool.freelist;
		if (tso_seg->on_freelist != 1) {
			qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
			qdf_print("tso seg alloc failed: not in freelist");
			QDF_BUG(0);
			return NULL;
		} else if (tso_seg->cookie != TSO_SEG_MAGIC_COOKIE) {
			qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
			qdf_print("tso seg alloc failed: bad cookie");
			QDF_BUG(0);
			return NULL;
		}
		/*this tso seg is not a part of freelist now.*/
		tso_seg->on_freelist = 0;
		tso_seg->sent_to_target = 0;
		tso_seg->force_free = 0;
		pdev->tso_seg_pool.freelist = pdev->tso_seg_pool.freelist->next;
		qdf_tso_seg_dbg_record(tso_seg, TSOSEG_LOC_ALLOC);
	}
	qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);

	return tso_seg;
}

/**
 * ol_tso_free_segment() - function to free a TSO segment
 * element
 * @pdev: the data physical device sending the data
 * @tso_seg: The TSO segment element to be freed
 *
 * Returns a TSO segment element to the free list held in the
 * pdev
 *
 * Return: none
 */
void ol_tso_free_segment(struct ol_txrx_pdev_t *pdev,
	 struct qdf_tso_seg_elem_t *tso_seg)
{
	qdf_spin_lock_bh(&pdev->tso_seg_pool.tso_mutex);
	if (tso_seg->on_freelist != 0) {
		qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
		qdf_print("Do not free tso seg, already freed");
		QDF_BUG(0);
		return;
	} else if (tso_seg->cookie != TSO_SEG_MAGIC_COOKIE) {
		qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
		qdf_print("Do not free tso seg: cookie is not good.");
		QDF_BUG(0);
		return;
	} else if ((tso_seg->sent_to_target != 1) &&
		   (tso_seg->force_free != 1)) {
		qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
		qdf_print("Do not free tso seg:  yet to be sent to target");
		QDF_BUG(0);
		return;
	}
	/* sanitize before free */
	ol_tso_seg_dbg_sanitize(tso_seg);
	qdf_tso_seg_dbg_setowner(tso_seg, NULL);
	/*this tso seg is now a part of freelist*/
	/* retain segment history, if debug is enabled */
	qdf_tso_seg_dbg_zero(tso_seg);
	tso_seg->next = pdev->tso_seg_pool.freelist;
	tso_seg->on_freelist = 1;
	tso_seg->sent_to_target = 0;
	tso_seg->cookie = TSO_SEG_MAGIC_COOKIE;
	pdev->tso_seg_pool.freelist = tso_seg;
	pdev->tso_seg_pool.num_free++;
	qdf_tso_seg_dbg_record(tso_seg, tso_seg->force_free
			       ? TSOSEG_LOC_FORCE_FREE
			       : TSOSEG_LOC_FREE);
	tso_seg->force_free = 0;
	qdf_spin_unlock_bh(&pdev->tso_seg_pool.tso_mutex);
}

/**
 * ol_tso_num_seg_alloc() - function to allocate a element to count TSO segments
 *			    in a jumbo skb packet.
 * @pdev: the data physical device sending the data
 *
 * Allocates a element to count TSO segments from the free list held in
 * the pdev
 *
 * Return: tso_num_seg
 */
struct qdf_tso_num_seg_elem_t *ol_tso_num_seg_alloc(struct ol_txrx_pdev_t *pdev)
{
	struct qdf_tso_num_seg_elem_t *tso_num_seg = NULL;

	qdf_spin_lock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
	if (pdev->tso_num_seg_pool.freelist) {
		pdev->tso_num_seg_pool.num_free--;
		tso_num_seg = pdev->tso_num_seg_pool.freelist;
		pdev->tso_num_seg_pool.freelist =
				pdev->tso_num_seg_pool.freelist->next;
	}
	qdf_spin_unlock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);

	return tso_num_seg;
}

/**
 * ol_tso_num_seg_free() - function to free a TSO segment
 * element
 * @pdev: the data physical device sending the data
 * @tso_seg: The TSO segment element to be freed
 *
 * Returns a element to the free list held in the pdev
 *
 * Return: none
 */
void ol_tso_num_seg_free(struct ol_txrx_pdev_t *pdev,
	 struct qdf_tso_num_seg_elem_t *tso_num_seg)
{
	qdf_spin_lock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
	tso_num_seg->next = pdev->tso_num_seg_pool.freelist;
	pdev->tso_num_seg_pool.freelist = tso_num_seg;
		pdev->tso_num_seg_pool.num_free++;
	qdf_spin_unlock_bh(&pdev->tso_num_seg_pool.tso_num_seg_mutex);
}
#endif
