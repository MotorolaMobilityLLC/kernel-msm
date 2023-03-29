/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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
#include <ol_txrx.h>            /* ol_txrx_get_vdev_from_vdev_id */

/* internal header files relevant only for HL systems */
#include <ol_tx_queue.h>        /* ol_tx_enqueue */

/* internal header files relevant only for specific systems (Pronto) */
#include <ol_txrx_encap.h>      /* OL_TX_ENCAP, etc */
#include <ol_tx.h>
#include <ol_cfg.h>
#include <cdp_txrx_handle.h>
#define INVALID_FLOW_ID 0xFF
#define MAX_INVALID_BIN 3

#ifdef QCA_LL_TX_FLOW_GLOBAL_MGMT_POOL
#define TX_FLOW_MGMT_POOL_ID	0xEF
#define TX_FLOW_MGMT_POOL_SIZE  32

/**
 * ol_tx_register_global_mgmt_pool() - register global pool for mgmt packets
 * @pdev: pdev handler
 *
 * Return: none
 */
static void
ol_tx_register_global_mgmt_pool(struct ol_txrx_pdev_t *pdev)
{
	pdev->mgmt_pool = ol_tx_create_flow_pool(TX_FLOW_MGMT_POOL_ID,
						 TX_FLOW_MGMT_POOL_SIZE);
	if (!pdev->mgmt_pool)
		ol_txrx_err("Management pool creation failed\n");
}

/**
 * ol_tx_deregister_global_mgmt_pool() - Deregister global pool for mgmt packets
 * @pdev: pdev handler
 *
 * Return: none
 */
static void
ol_tx_deregister_global_mgmt_pool(struct ol_txrx_pdev_t *pdev)
{
	ol_tx_dec_pool_ref(pdev->mgmt_pool, false);
}
#else
static inline void
ol_tx_register_global_mgmt_pool(struct ol_txrx_pdev_t *pdev)
{
}
static inline void
ol_tx_deregister_global_mgmt_pool(struct ol_txrx_pdev_t *pdev)
{
}
#endif

bool ol_txrx_fwd_desc_thresh_check(struct ol_txrx_vdev_t *txrx_vdev)
{
	struct ol_tx_flow_pool_t *pool;
	bool enough_desc_flag;

	if (!txrx_vdev)
		return false;

	pool = txrx_vdev->pool;

	if (!pool)
		return false;

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	enough_desc_flag = (pool->avail_desc < (pool->stop_th +
				OL_TX_NON_FWD_RESERVE))
		? false : true;
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
	return enough_desc_flag;
}

/**
 * ol_tx_set_desc_global_pool_size() - set global pool size
 * @num_msdu_desc: total number of descriptors
 *
 * Return: none
 */
void ol_tx_set_desc_global_pool_size(uint32_t num_msdu_desc)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		qdf_print("pdev is NULL");
		return;
	}
	pdev->num_msdu_desc = num_msdu_desc;
	if (!ol_tx_get_is_mgmt_over_wmi_enabled())
		pdev->num_msdu_desc += TX_FLOW_MGMT_POOL_SIZE;
	ol_txrx_info_high("Global pool size: %d\n", pdev->num_msdu_desc);
}

/**
 * ol_tx_get_total_free_desc() - get total free descriptors
 * @pdev: pdev handle
 *
 * Return: total free descriptors
 */
uint32_t ol_tx_get_total_free_desc(struct ol_txrx_pdev_t *pdev)
{
	struct ol_tx_flow_pool_t *pool = NULL;
	uint32_t free_desc;

	free_desc = pdev->tx_desc.num_free;
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(pool, &pdev->tx_desc.flow_pool_list,
		      flow_pool_list_elem) {
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		free_desc += pool->avail_desc;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	return free_desc;
}

/**
 * ol_tx_register_flow_control() - Register fw based tx flow control
 * @pdev: pdev handle
 *
 * Return: none
 */
void ol_tx_register_flow_control(struct ol_txrx_pdev_t *pdev)
{
	qdf_spinlock_create(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_INIT(&pdev->tx_desc.flow_pool_list);

	if (!ol_tx_get_is_mgmt_over_wmi_enabled())
		ol_tx_register_global_mgmt_pool(pdev);
}

/**
 * ol_tx_deregister_flow_control() - Deregister fw based tx flow control
 * @pdev: pdev handle
 *
 * Return: none
 */
void ol_tx_deregister_flow_control(struct ol_txrx_pdev_t *pdev)
{
	int i = 0;
	struct ol_tx_flow_pool_t *pool = NULL;
	struct cdp_soc_t *soc;

	if (!ol_tx_get_is_mgmt_over_wmi_enabled())
		ol_tx_deregister_global_mgmt_pool(pdev);

	soc = cds_get_context(QDF_MODULE_ID_SOC);

	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	while (!TAILQ_EMPTY(&pdev->tx_desc.flow_pool_list)) {
		pool = TAILQ_FIRST(&pdev->tx_desc.flow_pool_list);
		if (!pool)
			break;
		qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
		ol_txrx_info("flow pool list is not empty %d!!!\n", i++);

		if (i == 1)
			ol_tx_dump_flow_pool_info(soc);

		ol_tx_dec_pool_ref(pool, true);
		qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
	qdf_spinlock_destroy(&pdev->tx_desc.flow_pool_list_lock);
}

/**
 * ol_tx_delete_flow_pool() - delete flow pool
 * @pool: flow pool pointer
 * @force: free pool forcefully
 *
 * Delete flow_pool if all tx descriptors are available.
 * Otherwise put it in FLOW_POOL_INVALID state.
 * If force is set then pull all available descriptors to
 * global pool.
 *
 * Return: 0 for success or error
 */
static int ol_tx_delete_flow_pool(struct ol_tx_flow_pool_t *pool, bool force)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	uint16_t i, size;
	union ol_tx_desc_list_elem_t *temp_list = NULL;
	struct ol_tx_desc_t *tx_desc = NULL;

	if (!pool) {
		ol_txrx_err("pool is NULL");
		QDF_ASSERT(0);
		return -ENOMEM;
	}

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		QDF_ASSERT(0);
		return -ENOMEM;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		QDF_ASSERT(0);
		return -ENOMEM;
	}

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->avail_desc == pool->flow_pool_size || force == true)
		pool->status = FLOW_POOL_INACTIVE;
	else
		pool->status = FLOW_POOL_INVALID;

	/* Take all free descriptors and put it in temp_list */
	temp_list = pool->freelist;
	size = pool->avail_desc;
	pool->freelist = NULL;
	pool->avail_desc = 0;

	if (pool->status == FLOW_POOL_INACTIVE) {
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		/* Free flow_pool */
		qdf_spinlock_destroy(&pool->flow_pool_lock);
		qdf_mem_free(pool);
	} else { /* FLOW_POOL_INVALID case*/
		pool->flow_pool_size -= size;
		pool->flow_pool_id = INVALID_FLOW_ID;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		ol_tx_inc_pool_ref(pool);

		pdev->tx_desc.num_invalid_bin++;
		ol_txrx_info("invalid pool created %d",
			     pdev->tx_desc.num_invalid_bin);
		if (pdev->tx_desc.num_invalid_bin > MAX_INVALID_BIN)
			ASSERT(0);

		qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
		TAILQ_INSERT_TAIL(&pdev->tx_desc.flow_pool_list, pool,
				 flow_pool_list_elem);
		qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
	}

	/* put free descriptors to global pool */
	qdf_spin_lock_bh(&pdev->tx_mutex);
	for (i = 0; i < size; i++) {
		tx_desc = &temp_list->tx_desc;
		temp_list = temp_list->next;

		ol_tx_put_desc_global_pool(pdev, tx_desc);
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);

	ol_tx_distribute_descs_to_deficient_pools_from_global_pool();

	return 0;
}

QDF_STATUS ol_tx_inc_pool_ref(struct ol_tx_flow_pool_t *pool)
{
	if (!pool) {
		ol_txrx_err("flow pool is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	qdf_atomic_inc(&pool->ref_cnt);
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
	ol_txrx_dbg("pool %pK, ref_cnt %x",
		    pool, qdf_atomic_read(&pool->ref_cnt));

	return  QDF_STATUS_SUCCESS;
}

QDF_STATUS ol_tx_dec_pool_ref(struct ol_tx_flow_pool_t *pool, bool force)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	if (!pool) {
		ol_txrx_err("flow pool is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (qdf_atomic_dec_and_test(&pool->ref_cnt)) {
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		TAILQ_REMOVE(&pdev->tx_desc.flow_pool_list, pool,
			     flow_pool_list_elem);
		qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
		ol_txrx_dbg("Deleting pool %pK", pool);
		ol_tx_delete_flow_pool(pool, force);
	} else {
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
		ol_txrx_dbg("pool %pK, ref_cnt %x",
			    pool, qdf_atomic_read(&pool->ref_cnt));
	}

	return  QDF_STATUS_SUCCESS;
}

/**
 * ol_tx_flow_pool_status_to_str() - convert flow pool status to string
 * @status - flow pool status
 *
 * Returns: String corresponding to flow pool status
 */
static const char *ol_tx_flow_pool_status_to_str
					(enum flow_pool_status status)
{
	switch (status) {
	CASE_RETURN_STRING(FLOW_POOL_ACTIVE_UNPAUSED);
	CASE_RETURN_STRING(FLOW_POOL_ACTIVE_PAUSED);
	CASE_RETURN_STRING(FLOW_POOL_NON_PRIO_PAUSED);
	CASE_RETURN_STRING(FLOW_POOL_INVALID);
	CASE_RETURN_STRING(FLOW_POOL_INACTIVE);
	default:
		return "unknown";
	}
}

void ol_tx_dump_flow_pool_info_compact(struct ol_txrx_pdev_t *pdev)
{
	char *comb_log_str;
	int bytes_written = 0;
	uint32_t free_size;
	struct ol_tx_flow_pool_t *pool = NULL;

	free_size = WLAN_MAX_VDEVS * 100 + 100;
	comb_log_str = qdf_mem_malloc(free_size);
	if (!comb_log_str)
		return;

	bytes_written = snprintf(&comb_log_str[bytes_written], free_size,
				 "G:(%d,%d) ",
				 pdev->tx_desc.pool_size,
				 pdev->tx_desc.num_free);

	free_size -= bytes_written;

	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(pool, &pdev->tx_desc.flow_pool_list,
		      flow_pool_list_elem) {
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		bytes_written += snprintf(&comb_log_str[bytes_written],
					  free_size, "| %d (%d,%d)",
					  pool->flow_pool_id,
					  pool->flow_pool_size,
					  pool->avail_desc);
		free_size -= bytes_written;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);
	qdf_nofl_debug("STATS | FC: %s", comb_log_str);
	qdf_mem_free(comb_log_str);
}

/**
 * ol_tx_dump_flow_pool_info() - dump global_pool and flow_pool info
 * @soc_hdl: cdp_soc context, required only in lithium_dp flow control.
 *
 * Return: none
 */
void ol_tx_dump_flow_pool_info(struct cdp_soc_t *soc_hdl)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool = NULL, *pool_prev = NULL;
	struct ol_tx_flow_pool_t tmp_pool;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		QDF_ASSERT(0);
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("ERROR: pdev NULL");
		QDF_ASSERT(0); /* traceback */
		return;
	}

	txrx_nofl_info("Global total %d :: avail %d invalid flow_pool %d ",
		       pdev->tx_desc.pool_size,
		       pdev->tx_desc.num_free,
		       pdev->tx_desc.num_invalid_bin);

	txrx_nofl_info("maps %d pool unmaps %d pool resize %d pkt drops %d",
		       pdev->pool_stats.pool_map_count,
		       pdev->pool_stats.pool_unmap_count,
		       pdev->pool_stats.pool_resize_count,
		       pdev->pool_stats.pkt_drop_no_pool);
	/*
	 * Nested spin lock.
	 * Always take in below order.
	 * flow_pool_list_lock -> flow_pool_lock
	 */
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(pool, &pdev->tx_desc.flow_pool_list,
					 flow_pool_list_elem) {
		ol_tx_inc_pool_ref(pool);
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		qdf_mem_copy(&tmp_pool, pool, sizeof(tmp_pool));
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

		if (pool_prev)
			ol_tx_dec_pool_ref(pool_prev, false);

		txrx_nofl_info("flow_pool_id %d ::", tmp_pool.flow_pool_id);
		txrx_nofl_info("status %s flow_id %d flow_type %d",
			       ol_tx_flow_pool_status_to_str
					(tmp_pool.status),
			       tmp_pool.member_flow_id, tmp_pool.flow_type);
		txrx_nofl_info("total %d :: available %d :: deficient %d :: overflow %d :: pkt dropped (no desc) %d",
			       tmp_pool.flow_pool_size, tmp_pool.avail_desc,
			       tmp_pool.deficient_desc,
			       tmp_pool.overflow_desc,
			       tmp_pool.pkt_drop_no_desc);
		txrx_nofl_info("thresh: start %d stop %d prio start %d prio stop %d",
			       tmp_pool.start_th, tmp_pool.stop_th,
			       tmp_pool.start_priority_th,
			       tmp_pool.stop_priority_th);
		pool_prev = pool;
		qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	/* decrement ref count for last pool in list */
	if (pool_prev)
		ol_tx_dec_pool_ref(pool_prev, false);

}

/**
 * ol_tx_clear_flow_pool_stats() - clear flow pool statistics
 *
 * Return: none
 */
void ol_tx_clear_flow_pool_stats(void)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is null");
		return;
	}
	qdf_mem_zero(&pdev->pool_stats, sizeof(pdev->pool_stats));
}

/**
 * ol_tx_move_desc_n() - Move n descriptors from src_pool to dst_pool.
 * @src_pool: source pool
 * @dst_pool: destination pool
 * @desc_move_count: descriptor move count
 *
 * Return: actual descriptors moved
 */
static int ol_tx_move_desc_n(struct ol_tx_flow_pool_t *src_pool,
		      struct ol_tx_flow_pool_t *dst_pool,
		      int desc_move_count)
{
	uint16_t count = 0, i;
	struct ol_tx_desc_t *tx_desc;
	union ol_tx_desc_list_elem_t *temp_list = NULL;

	/* Take descriptors from source pool and put it in temp_list */
	qdf_spin_lock_bh(&src_pool->flow_pool_lock);
	for (i = 0; i < desc_move_count; i++) {
		tx_desc = ol_tx_get_desc_flow_pool(src_pool);
		((union ol_tx_desc_list_elem_t *)tx_desc)->next = temp_list;
		temp_list = (union ol_tx_desc_list_elem_t *)tx_desc;

	}
	qdf_spin_unlock_bh(&src_pool->flow_pool_lock);

	/* Take descriptors from temp_list and put it in destination pool */
	qdf_spin_lock_bh(&dst_pool->flow_pool_lock);
	for (i = 0; i < desc_move_count; i++) {
		if (dst_pool->deficient_desc)
			dst_pool->deficient_desc--;
		else
			break;
		tx_desc = &temp_list->tx_desc;
		temp_list = temp_list->next;
		ol_tx_put_desc_flow_pool(dst_pool, tx_desc);
		count++;
	}
	qdf_spin_unlock_bh(&dst_pool->flow_pool_lock);

	/* If anything is there in temp_list put it back to source pool */
	qdf_spin_lock_bh(&src_pool->flow_pool_lock);
	while (temp_list) {
		tx_desc = &temp_list->tx_desc;
		temp_list = temp_list->next;
		ol_tx_put_desc_flow_pool(src_pool, tx_desc);
	}
	qdf_spin_unlock_bh(&src_pool->flow_pool_lock);

	return count;
}


/**
 * ol_tx_distribute_descs_to_deficient_pools() - Distribute descriptors
 * @src_pool: source pool
 *
 * Distribute all descriptors of source pool to all
 * deficient pools as per flow_pool_list.
 *
 * Return: 0 for success
 */
static int
ol_tx_distribute_descs_to_deficient_pools(struct ol_tx_flow_pool_t *src_pool)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *dst_pool = NULL;
	uint16_t desc_count = src_pool->avail_desc;
	uint16_t desc_move_count = 0;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return -EINVAL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return -EINVAL;
	}
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(dst_pool, &pdev->tx_desc.flow_pool_list,
					 flow_pool_list_elem) {
		qdf_spin_lock_bh(&dst_pool->flow_pool_lock);
		if (dst_pool->deficient_desc) {
			desc_move_count =
				(dst_pool->deficient_desc > desc_count) ?
					desc_count : dst_pool->deficient_desc;
			qdf_spin_unlock_bh(&dst_pool->flow_pool_lock);
			desc_move_count = ol_tx_move_desc_n(src_pool,
						dst_pool, desc_move_count);
			desc_count -= desc_move_count;

			qdf_spin_lock_bh(&dst_pool->flow_pool_lock);
			if (dst_pool->status == FLOW_POOL_ACTIVE_PAUSED) {
				if (dst_pool->avail_desc > dst_pool->start_th) {
					pdev->pause_cb(dst_pool->member_flow_id,
					      WLAN_NETIF_PRIORITY_QUEUE_ON,
					      WLAN_DATA_FLOW_CONTROL_PRIORITY);

					pdev->pause_cb(dst_pool->member_flow_id,
						      WLAN_WAKE_ALL_NETIF_QUEUE,
						      WLAN_DATA_FLOW_CONTROL);

					dst_pool->status =
						FLOW_POOL_ACTIVE_UNPAUSED;
				}
			}
		}
		qdf_spin_unlock_bh(&dst_pool->flow_pool_lock);
		if (desc_count == 0)
			break;
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	return 0;
}

/**
 * ol_tx_create_flow_pool() - create flow pool
 * @flow_pool_id: flow pool id
 * @flow_pool_size: flow pool size
 *
 * Return: flow_pool pointer / NULL for error
 */
struct ol_tx_flow_pool_t *ol_tx_create_flow_pool(uint8_t flow_pool_id,
						 uint16_t flow_pool_size)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool;
	uint16_t size = 0, i;
	struct ol_tx_desc_t *tx_desc;
	union ol_tx_desc_list_elem_t *temp_list = NULL;
	uint32_t stop_threshold;
	uint32_t start_threshold;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return NULL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return NULL;
	}
	stop_threshold = ol_cfg_get_tx_flow_stop_queue_th(pdev->ctrl_pdev);
	start_threshold = stop_threshold +
		ol_cfg_get_tx_flow_start_queue_offset(pdev->ctrl_pdev);
	pool = qdf_mem_malloc(sizeof(*pool));
	if (!pool)
		return NULL;

	pool->flow_pool_id = flow_pool_id;
	pool->flow_pool_size = flow_pool_size;
	pool->status = FLOW_POOL_ACTIVE_UNPAUSED;
	pool->start_th = (start_threshold * flow_pool_size)/100;
	pool->stop_th = (stop_threshold * flow_pool_size)/100;
	pool->stop_priority_th = (TX_PRIORITY_TH * pool->stop_th)/100;
	if (pool->stop_priority_th >= MAX_TSO_SEGMENT_DESC)
		pool->stop_priority_th -= MAX_TSO_SEGMENT_DESC;

	pool->start_priority_th = (TX_PRIORITY_TH * pool->start_th)/100;
	if (pool->start_priority_th >= MAX_TSO_SEGMENT_DESC)
			pool->start_priority_th -= MAX_TSO_SEGMENT_DESC;

	qdf_spinlock_create(&pool->flow_pool_lock);
	qdf_atomic_init(&pool->ref_cnt);
	ol_tx_inc_pool_ref(pool);

	/* Take TX descriptor from global_pool and put it in temp_list*/
	qdf_spin_lock_bh(&pdev->tx_mutex);
	if (pdev->tx_desc.num_free >= pool->flow_pool_size)
		size = pool->flow_pool_size;
	else
		size = pdev->tx_desc.num_free;

	for (i = 0; i < size; i++) {
		tx_desc = ol_tx_get_desc_global_pool(pdev);
		tx_desc->pool = pool;
		((union ol_tx_desc_list_elem_t *)tx_desc)->next = temp_list;
		temp_list = (union ol_tx_desc_list_elem_t *)tx_desc;

	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);

	/* put temp_list to flow_pool */
	pool->freelist = temp_list;
	pool->avail_desc = size;
	pool->deficient_desc = pool->flow_pool_size - pool->avail_desc;
	/* used for resize pool*/
	pool->overflow_desc = 0;

	/* Add flow_pool to flow_pool_list */
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_INSERT_TAIL(&pdev->tx_desc.flow_pool_list, pool,
			 flow_pool_list_elem);
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	return pool;
}

/**
 * ol_tx_free_invalid_flow_pool() - free invalid pool
 * @pool: pool
 *
 * Return: 0 for success or failure
 */
int ol_tx_free_invalid_flow_pool(struct ol_tx_flow_pool_t *pool)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return -EINVAL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if ((!pdev) || (!pool) || (pool->status != FLOW_POOL_INVALID)) {
		ol_txrx_err("Invalid pool/pdev");
		return -EINVAL;
	}

	/* direclty distribute to other deficient pools */
	ol_tx_distribute_descs_to_deficient_pools(pool);

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	pool->flow_pool_size = pool->avail_desc;
	qdf_spin_unlock_bh(&pool->flow_pool_lock);

	pdev->tx_desc.num_invalid_bin--;
	ol_txrx_info("invalid pool deleted %d",
		     pdev->tx_desc.num_invalid_bin);

	return ol_tx_dec_pool_ref(pool, false);
}

/**
 * ol_tx_get_flow_pool() - get flow_pool from flow_pool_id
 * @flow_pool_id: flow pool id
 *
 * Return: flow_pool ptr / NULL if not found
 */
static struct ol_tx_flow_pool_t *ol_tx_get_flow_pool(uint8_t flow_pool_id)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool = NULL;
	bool is_found = false;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		QDF_ASSERT(0);
		return NULL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("ERROR: pdev NULL");
		QDF_ASSERT(0); /* traceback */
		return NULL;
	}

	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(pool, &pdev->tx_desc.flow_pool_list,
					 flow_pool_list_elem) {
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		if (pool->flow_pool_id == flow_pool_id) {
			qdf_spin_unlock_bh(&pool->flow_pool_lock);
			is_found = true;
			break;
		}
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	if (is_found == false)
		pool = NULL;

	return pool;
}

/**
 * ol_tx_flow_pool_vdev_map() - Map flow_pool with vdev
 * @pool: flow_pool
 * @vdev_id: flow_id /vdev_id
 *
 * Return: none
 */
static void ol_tx_flow_pool_vdev_map(struct ol_tx_flow_pool_t *pool,
				     uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev;

	vdev = (struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (!vdev) {
		ol_txrx_err("invalid vdev_id %d", vdev_id);
		return;
	}

	vdev->pool = pool;
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	pool->member_flow_id = vdev_id;
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
}

/**
 * ol_tx_flow_pool_vdev_unmap() - Unmap flow_pool from vdev
 * @pool: flow_pool
 * @vdev_id: flow_id /vdev_id
 *
 * Return: none
 */
static void ol_tx_flow_pool_vdev_unmap(struct ol_tx_flow_pool_t *pool,
				       uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev;

	vdev = (struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (!vdev) {
		ol_txrx_dbg("invalid vdev_id %d", vdev_id);
		return;
	}

	vdev->pool = NULL;
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	pool->member_flow_id = INVALID_FLOW_ID;
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
}

/**
 * ol_tx_flow_pool_map_handler() - Map flow_id with pool of descriptors
 * @flow_id: flow id
 * @flow_type: flow type
 * @flow_pool_id: pool id
 * @flow_pool_size: pool size
 *
 * Process below target to host message
 * HTT_T2H_MSG_TYPE_FLOW_POOL_MAP
 *
 * Return: none
 */
void ol_tx_flow_pool_map_handler(uint8_t flow_id, uint8_t flow_type,
				 uint8_t flow_pool_id, uint16_t flow_pool_size)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool;
	uint8_t pool_create = 0;
	enum htt_flow_type type = flow_type;

	ol_txrx_dbg("flow_id %d flow_type %d flow_pool_id %d flow_pool_size %d",
		    flow_id, flow_type, flow_pool_id, flow_pool_size);

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return;
	}
	pdev->pool_stats.pool_map_count++;

	pool = ol_tx_get_flow_pool(flow_pool_id);
	if (!pool) {
		pool = ol_tx_create_flow_pool(flow_pool_id, flow_pool_size);
		if (!pool) {
			ol_txrx_err("creation of flow_pool %d size %d failed",
				    flow_pool_id, flow_pool_size);
			return;
		}
		pool_create = 1;
	}

	switch (type) {

	case FLOW_TYPE_VDEV:
		ol_tx_flow_pool_vdev_map(pool, flow_id);
		pdev->pause_cb(flow_id,
			       WLAN_NETIF_PRIORITY_QUEUE_ON,
			       WLAN_DATA_FLOW_CONTROL_PRIORITY);
		pdev->pause_cb(flow_id,
			       WLAN_WAKE_ALL_NETIF_QUEUE,
			       WLAN_DATA_FLOW_CONTROL);
		break;
	default:
		if (pool_create)
			ol_tx_dec_pool_ref(pool, false);
		ol_txrx_err("flow type %d not supported", type);
		break;
	}
}

/**
 * ol_tx_flow_pool_unmap_handler() - Unmap flow_id from pool of descriptors
 * @flow_id: flow id
 * @flow_type: flow type
 * @flow_pool_id: pool id
 *
 * Process below target to host message
 * HTT_T2H_MSG_TYPE_FLOW_POOL_UNMAP
 *
 * Return: none
 */
void ol_tx_flow_pool_unmap_handler(uint8_t flow_id, uint8_t flow_type,
							  uint8_t flow_pool_id)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool;
	enum htt_flow_type type = flow_type;

	ol_txrx_dbg("flow_id %d flow_type %d flow_pool_id %d",
		    flow_id, flow_type, flow_pool_id);

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return;
	}
	pdev->pool_stats.pool_unmap_count++;

	pool = ol_tx_get_flow_pool(flow_pool_id);
	if (!pool) {
		ol_txrx_info("flow_pool not available flow_pool_id %d", type);
		return;
	}

	switch (type) {

	case FLOW_TYPE_VDEV:
		ol_tx_flow_pool_vdev_unmap(pool, flow_id);
		break;
	default:
		ol_txrx_info("flow type %d not supported", type);
		return;
	}

	/*
	 * only delete if all descriptors are available
	 * and pool ref count becomes 0
	 */
	ol_tx_dec_pool_ref(pool, false);
}

#ifdef QCA_LL_TX_FLOW_CONTROL_RESIZE
/**
 * ol_tx_distribute_descs_to_deficient_pools_from_global_pool()
 *
 * Distribute descriptors of global pool to all
 * deficient pools as per need.
 *
 * Return: 0 for success
 */
int ol_tx_distribute_descs_to_deficient_pools_from_global_pool(void)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *dst_pool = NULL;
	struct ol_tx_flow_pool_t *tmp_pool = NULL;
	uint16_t total_desc_req = 0;
	uint16_t desc_move_count = 0;
	uint16_t temp_count = 0, i;
	union ol_tx_desc_list_elem_t *temp_list = NULL;
	struct ol_tx_desc_t *tx_desc;
	uint8_t free_invalid_pool = 0;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return -EINVAL;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return -EINVAL;
	}

	/* Nested locks: maintain flow_pool_list_lock->flow_pool_lock */
	/* find out total deficient desc required */
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(dst_pool, &pdev->tx_desc.flow_pool_list,
		      flow_pool_list_elem) {
		qdf_spin_lock_bh(&dst_pool->flow_pool_lock);
		total_desc_req += dst_pool->deficient_desc;
		qdf_spin_unlock_bh(&dst_pool->flow_pool_lock);
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	qdf_spin_lock_bh(&pdev->tx_mutex);
	desc_move_count = (pdev->tx_desc.num_free >= total_desc_req) ?
				 total_desc_req : pdev->tx_desc.num_free;

	for (i = 0; i < desc_move_count; i++) {
		tx_desc = ol_tx_get_desc_global_pool(pdev);
		((union ol_tx_desc_list_elem_t *)tx_desc)->next = temp_list;
		temp_list = (union ol_tx_desc_list_elem_t *)tx_desc;
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);

	if (!desc_move_count)
		return 0;

	/* destribute desc to deficient pool */
	qdf_spin_lock_bh(&pdev->tx_desc.flow_pool_list_lock);
	TAILQ_FOREACH(dst_pool, &pdev->tx_desc.flow_pool_list,
		      flow_pool_list_elem) {
		qdf_spin_lock_bh(&dst_pool->flow_pool_lock);
		if (dst_pool->deficient_desc) {
			temp_count =
				(dst_pool->deficient_desc > desc_move_count) ?
				desc_move_count : dst_pool->deficient_desc;

			desc_move_count -= temp_count;
			for (i = 0; i < temp_count; i++) {
				tx_desc = &temp_list->tx_desc;
				temp_list = temp_list->next;
				ol_tx_put_desc_flow_pool(dst_pool, tx_desc);
			}

			if (dst_pool->status == FLOW_POOL_ACTIVE_PAUSED) {
				if (dst_pool->avail_desc > dst_pool->start_th) {
					pdev->pause_cb(dst_pool->member_flow_id,
						      WLAN_WAKE_ALL_NETIF_QUEUE,
						      WLAN_DATA_FLOW_CONTROL);
					dst_pool->status =
						FLOW_POOL_ACTIVE_UNPAUSED;
				}
			} else if ((dst_pool->status == FLOW_POOL_INVALID) &&
				   (dst_pool->avail_desc ==
					 dst_pool->flow_pool_size)) {
				free_invalid_pool = 1;
				tmp_pool = dst_pool;
			}
		}
		qdf_spin_unlock_bh(&dst_pool->flow_pool_lock);
		if (desc_move_count == 0)
			break;
	}
	qdf_spin_unlock_bh(&pdev->tx_desc.flow_pool_list_lock);

	if (free_invalid_pool && tmp_pool)
		ol_tx_free_invalid_flow_pool(tmp_pool);

	return 0;
}

/**
 * ol_tx_flow_pool_update_queue_state() - update network queue for pool based on
 *                                        new available count.
 * @pool : pool handle
 *
 * Return : none
 */
static void ol_tx_flow_pool_update_queue_state(struct ol_txrx_pdev_t *pdev,
					       struct ol_tx_flow_pool_t *pool)
{
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->avail_desc > pool->start_th) {
		pool->status = FLOW_POOL_ACTIVE_UNPAUSED;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		pdev->pause_cb(pool->member_flow_id,
			       WLAN_WAKE_ALL_NETIF_QUEUE,
			       WLAN_DATA_FLOW_CONTROL);
	} else if (pool->avail_desc < pool->stop_th &&
		   pool->avail_desc >= pool->stop_priority_th) {
		pool->status = FLOW_POOL_NON_PRIO_PAUSED;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		pdev->pause_cb(pool->member_flow_id,
			       WLAN_STOP_NON_PRIORITY_QUEUE,
			       WLAN_DATA_FLOW_CONTROL);
		pdev->pause_cb(pool->member_flow_id,
			       WLAN_NETIF_PRIORITY_QUEUE_ON,
			       WLAN_DATA_FLOW_CONTROL);
	} else if (pool->avail_desc < pool->stop_priority_th) {
		pool->status = FLOW_POOL_ACTIVE_PAUSED;
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		pdev->pause_cb(pool->member_flow_id,
			       WLAN_STOP_ALL_NETIF_QUEUE,
			       WLAN_DATA_FLOW_CONTROL);
	} else {
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	}
}

/**
 * ol_tx_flow_pool_update() - update pool parameters with new size
 * @pool : pool handle
 * @new_pool_size : new pool size
 * @deficient_count : deficient count
 * @overflow_count : overflow count
 *
 * Return : none
 */
static void ol_tx_flow_pool_update(struct ol_tx_flow_pool_t *pool,
				   uint16_t new_pool_size,
				   uint16_t deficient_count,
				   uint16_t overflow_count)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	uint32_t stop_threshold;
	uint32_t start_threshold;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	stop_threshold = ol_cfg_get_tx_flow_stop_queue_th(pdev->ctrl_pdev);
	start_threshold = stop_threshold +
			ol_cfg_get_tx_flow_start_queue_offset(pdev->ctrl_pdev);
	pool->flow_pool_size = new_pool_size;
	pool->start_th = (start_threshold * new_pool_size) / 100;
	pool->stop_th = (stop_threshold * new_pool_size) / 100;
	pool->stop_priority_th = (TX_PRIORITY_TH * pool->stop_th) / 100;
	if (pool->stop_priority_th >= MAX_TSO_SEGMENT_DESC)
		pool->stop_priority_th -= MAX_TSO_SEGMENT_DESC;

	pool->start_priority_th = (TX_PRIORITY_TH * pool->start_th) / 100;
	if (pool->start_priority_th >= MAX_TSO_SEGMENT_DESC)
		pool->start_priority_th -= MAX_TSO_SEGMENT_DESC;

	if (deficient_count)
		pool->deficient_desc = deficient_count;

	if (overflow_count)
		pool->overflow_desc = overflow_count;
}

/**
 * ol_tx_flow_pool_resize() - resize pool with new size
 * @pool: pool pointer
 * @new_pool_size: new pool size
 *
 * Return: none
 */
static void ol_tx_flow_pool_resize(struct ol_tx_flow_pool_t *pool,
				   uint16_t new_pool_size)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	uint16_t diff = 0, overflow_count = 0, deficient_count = 0;
	uint16_t move_desc_to_global = 0, move_desc_from_global = 0;
	union ol_tx_desc_list_elem_t *temp_list = NULL;
	int i = 0, update_done = 0;
	struct ol_tx_desc_t *tx_desc = NULL;
	uint16_t temp = 0;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->flow_pool_size == new_pool_size) {
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
		ol_txrx_info("pool resize received with same size");
		return;
	}
	qdf_spin_unlock_bh(&pool->flow_pool_lock);

	/* Reduce pool size */
	/* start_priority_th desc should available after reduction */
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->flow_pool_size > new_pool_size) {
		diff = pool->flow_pool_size - new_pool_size;
		diff += pool->overflow_desc;
		pool->overflow_desc = 0;
		temp = QDF_MIN(pool->deficient_desc, diff);
		pool->deficient_desc -= temp;
		diff -= temp;

		if (diff) {
			/* Have enough descriptors */
			if (pool->avail_desc >=
				 (diff + pool->start_priority_th)) {
				move_desc_to_global = diff;
			}
			/* Do not have enough descriptors */
			else if (pool->avail_desc > pool->start_priority_th) {
				move_desc_to_global = pool->avail_desc -
						 pool->start_priority_th;
				overflow_count = diff - move_desc_to_global;
			}

			/* Move desc to temp_list */
			for (i = 0; i < move_desc_to_global; i++) {
				tx_desc = ol_tx_get_desc_flow_pool(pool);
				((union ol_tx_desc_list_elem_t *)tx_desc)->next
								 = temp_list;
				temp_list =
				  (union ol_tx_desc_list_elem_t *)tx_desc;
			}
		}

		/* update pool size and threshold */
		ol_tx_flow_pool_update(pool, new_pool_size, 0, overflow_count);
		update_done = 1;
	}
	qdf_spin_unlock_bh(&pool->flow_pool_lock);

	if (move_desc_to_global && temp_list) {
		/* put free descriptors to global pool */
		qdf_spin_lock_bh(&pdev->tx_mutex);
		for (i = 0; i < move_desc_to_global; i++) {
			tx_desc = &temp_list->tx_desc;
			temp_list = temp_list->next;
			ol_tx_put_desc_global_pool(pdev, tx_desc);
		}
		qdf_spin_unlock_bh(&pdev->tx_mutex);
	}

	if (update_done)
		goto update_done;

	/* Increase pool size */
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (pool->flow_pool_size < new_pool_size) {
		diff = new_pool_size - pool->flow_pool_size;
		diff += pool->deficient_desc;
		pool->deficient_desc = 0;
		temp = QDF_MIN(pool->overflow_desc, diff);
		pool->overflow_desc -= temp;
		diff -= temp;
	}
	qdf_spin_unlock_bh(&pool->flow_pool_lock);

	if (diff) {
		/* take descriptors from global pool */
		qdf_spin_lock_bh(&pdev->tx_mutex);

		if (pdev->tx_desc.num_free >= diff) {
			move_desc_from_global = diff;
		} else {
			move_desc_from_global = pdev->tx_desc.num_free;
			deficient_count = diff - move_desc_from_global;
		}

		for (i = 0; i < move_desc_from_global; i++) {
			tx_desc = ol_tx_get_desc_global_pool(pdev);
			((union ol_tx_desc_list_elem_t *)tx_desc)->next =
								 temp_list;
			temp_list = (union ol_tx_desc_list_elem_t *)tx_desc;
		}
		qdf_spin_unlock_bh(&pdev->tx_mutex);
	}
	/* update desc to pool */
	qdf_spin_lock_bh(&pool->flow_pool_lock);
	if (move_desc_from_global && temp_list) {
		for (i = 0; i < move_desc_from_global; i++) {
			tx_desc = &temp_list->tx_desc;
			temp_list = temp_list->next;
			ol_tx_put_desc_flow_pool(pool, tx_desc);
		}
	}
	/* update pool size and threshold */
	ol_tx_flow_pool_update(pool, new_pool_size, deficient_count, 0);
	qdf_spin_unlock_bh(&pool->flow_pool_lock);

update_done:

	ol_tx_flow_pool_update_queue_state(pdev, pool);
}

/**
 * ol_tx_flow_pool_resize_handler() - Resize pool with new size
 * @flow_pool_id: pool id
 * @flow_pool_size: pool size
 *
 * Process below target to host message
 * HTT_T2H_MSG_TYPE_FLOW_POOL_RESIZE
 *
 * Return: none
 */
void ol_tx_flow_pool_resize_handler(uint8_t flow_pool_id,
				    uint16_t flow_pool_size)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_flow_pool_t *pool;

	ol_txrx_dbg("flow_pool_id %d flow_pool_size %d",
		    flow_pool_id, flow_pool_size);

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return;
	}
	pdev->pool_stats.pool_resize_count++;

	pool = ol_tx_get_flow_pool(flow_pool_id);
	if (!pool) {
		ol_txrx_err("resize for flow_pool %d size %d failed",
			    flow_pool_id, flow_pool_size);
		return;
	}

	ol_tx_inc_pool_ref(pool);
	ol_tx_flow_pool_resize(pool, flow_pool_size);
	ol_tx_dec_pool_ref(pool, false);
}
#endif

/**
 * ol_txrx_map_to_netif_reason_type() - map to netif_reason_type
 * @reason: network queue pause reason
 *
 * Return: netif_reason_type
 */
static enum netif_reason_type
ol_txrx_map_to_netif_reason_type(uint32_t reason)
{
	switch (reason) {
	case OL_TXQ_PAUSE_REASON_FW:
		return WLAN_FW_PAUSE;
	case OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED:
		return WLAN_PEER_UNAUTHORISED;
	case OL_TXQ_PAUSE_REASON_TX_ABORT:
		return WLAN_TX_ABORT;
	case OL_TXQ_PAUSE_REASON_VDEV_STOP:
		return WLAN_VDEV_STOP;
	case OL_TXQ_PAUSE_REASON_THERMAL_MITIGATION:
		return WLAN_THERMAL_MITIGATION;
	default:
		ol_txrx_err("reason not supported %d", reason);
		return WLAN_REASON_TYPE_MAX;
	}
}

void ol_txrx_vdev_pause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	struct ol_txrx_pdev_t *pdev;
	enum netif_reason_type netif_reason;

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	pdev = vdev->pdev;
	if (qdf_unlikely((!pdev) || (!pdev->pause_cb))) {
		ol_txrx_err("invalid pdev");
		return;
	}

	netif_reason = ol_txrx_map_to_netif_reason_type(reason);
	if (netif_reason == WLAN_REASON_TYPE_MAX)
		return;

	pdev->pause_cb(vdev->vdev_id, WLAN_STOP_ALL_NETIF_QUEUE, netif_reason);
}

/**
 * ol_txrx_vdev_unpause() - unpause vdev network queues
 * @soc_hdl: datapath soc handle
 * @vdev: vdev handle
 * @reason: network queue pause reason
 * @pause_type: type of pause
 *
 * Return: none
 */
void ol_txrx_vdev_unpause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			  uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	struct ol_txrx_pdev_t *pdev;
	enum netif_reason_type netif_reason;

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	pdev = vdev->pdev;
	if (qdf_unlikely((!pdev) || (!pdev->pause_cb))) {
		ol_txrx_err("invalid pdev");
		return;
	}

	netif_reason = ol_txrx_map_to_netif_reason_type(reason);
	if (netif_reason == WLAN_REASON_TYPE_MAX)
		return;

	pdev->pause_cb(vdev->vdev_id, WLAN_WAKE_ALL_NETIF_QUEUE,
			netif_reason);
}

/**
 * ol_txrx_pdev_pause() - pause network queues for each vdev
 * @pdev: pdev handle
 * @reason: network queue pause reason
 *
 * Return: none
 */
void ol_txrx_pdev_pause(struct ol_txrx_pdev_t *pdev, uint32_t reason)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		ol_txrx_vdev_pause(ol_txrx_soc_t_to_cdp_soc_t(soc),
				   vdev->vdev_id, reason, 0);
	}
}

/**
 * ol_txrx_pdev_unpause() - unpause network queues for each vdev
 * @pdev: pdev handle
 * @reason: network queue pause reason
 *
 * Return: none
 */
void ol_txrx_pdev_unpause(struct ol_txrx_pdev_t *pdev, uint32_t reason)
{
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		ol_txrx_vdev_unpause(ol_txrx_soc_t_to_cdp_soc_t(soc),
				     vdev->vdev_id, reason, 0);
	}
}
