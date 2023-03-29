/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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

#ifndef _DP_TXRX_H
#define _DP_TXRX_H

#include <wlan_objmgr_psoc_obj.h>
#include <dp_rx_thread.h>
#include <qdf_trace.h>
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

/**
 * struct dp_txrx_config - dp txrx configuration passed to dp txrx modules
 * @enable_dp_rx_threads: enable DP rx threads or not
 */
struct dp_txrx_config {
	bool enable_rx_threads;
};

struct dp_txrx_handle_cmn;
/**
 * struct dp_txrx_handle - main dp txrx container handle
 * @soc: ol_txrx_soc_handle soc handle
 * @refill_thread: rx refill thread infra handle
 * @rx_tm_hdl: rx thread infrastructure handle
 */
struct dp_txrx_handle {
	ol_txrx_soc_handle soc;
	struct cdp_pdev *pdev;
	struct dp_rx_tm_handle rx_tm_hdl;
	struct dp_rx_refill_thread refill_thread;
	struct dp_txrx_config config;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
/**
 * dp_rx_napi_gro_flush() - do gro flush
 * @napi: napi used to do gro flush
 * @flush_code: flush_code differentiating low_tput_flush and normal_flush
 *
 * if there is RX GRO_NORMAL packets pending in napi
 * rx_list, flush them manually right after napi_gro_flush.
 *
 * return: none
 */
static inline void dp_rx_napi_gro_flush(struct napi_struct *napi,
					enum dp_rx_gro_flush_code flush_code)
{
	if (napi->poll) {
		/* Skipping GRO flush in low TPUT */
		if (flush_code != DP_RX_GRO_LOW_TPUT_FLUSH)
			napi_gro_flush(napi, false);

		if (napi->rx_count) {
			netif_receive_skb_list(&napi->rx_list);
			qdf_init_list_head(&napi->rx_list);
			napi->rx_count = 0;
		}
	}
}
#else
static inline void dp_rx_napi_gro_flush(struct napi_struct *napi,
					enum dp_rx_gro_flush_code flush_code)
{
	if (napi->poll) {
		/* Skipping GRO flush in low TPUT */
		if (flush_code != DP_RX_GRO_LOW_TPUT_FLUSH)
			napi_gro_flush(napi, false);
	}
}
#endif

#ifdef FEATURE_WLAN_DP_RX_THREADS
/**
 * dp_txrx_get_cmn_hdl_frm_ext_hdl() - conversion func ext_hdl->txrx_handle_cmn
 * @dp_ext_hdl: pointer to dp_txrx_handle structure
 *
 * Return: typecasted pointer of type - struct dp_txrx_handle_cmn
 */
static inline struct dp_txrx_handle_cmn *
dp_txrx_get_cmn_hdl_frm_ext_hdl(struct dp_txrx_handle *dp_ext_hdl)
{
	return (struct dp_txrx_handle_cmn *)dp_ext_hdl;
}

/**
 * dp_txrx_get_ext_hdl_frm_cmn_hdl() - conversion func txrx_handle_cmn->ext_hdl
 * @txrx_cmn_hdl: pointer to dp_txrx_handle_cmn structure
 *
 * Return: typecasted pointer of type - struct dp_txrx_handle
 */
static inline struct dp_txrx_handle *
dp_txrx_get_ext_hdl_frm_cmn_hdl(struct dp_txrx_handle_cmn *txrx_cmn_hdl)
{
	return (struct dp_txrx_handle *)txrx_cmn_hdl;
}

static inline ol_txrx_soc_handle
dp_txrx_get_soc_from_ext_handle(struct dp_txrx_handle_cmn *txrx_cmn_hdl)
{
	struct dp_txrx_handle *dp_ext_hdl;

	dp_ext_hdl = dp_txrx_get_ext_hdl_frm_cmn_hdl(txrx_cmn_hdl);

	return dp_ext_hdl->soc;
}

static inline struct cdp_pdev*
dp_txrx_get_pdev_from_ext_handle(struct dp_txrx_handle_cmn *txrx_cmn_hdl)
{
	struct dp_txrx_handle *dp_ext_hdl;

	dp_ext_hdl = dp_txrx_get_ext_hdl_frm_cmn_hdl(txrx_cmn_hdl);

	return dp_ext_hdl->pdev;
}

/**
 * dp_txrx_init() - initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 * @pdev_id: id of dp pdev handle
 * @config: configuration for DP TXRX modules
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config);

/**
 * dp_txrx_deinit() - de-initialize DP TXRX module
 * @soc: ol_txrx_soc_handle
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc);

/**
 * dp_txrx_flush_pkts_by_vdev_id() - flush rx packets for a vdev_id
 * @soc: ol_txrx_soc_handle object
 * @vdev_id: vdev_id for which rx packets are to be flushed
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_flush_pkts_by_vdev_id(ol_txrx_soc_handle soc,
						       uint8_t vdev_id)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_flush_by_vdev_id(&dp_ext_hdl->rx_tm_hdl, vdev_id);
ret:
	return qdf_status;
}

/**
 * dp_txrx_resume() - resume all threads
 * @soc: ol_txrx_soc_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_resume(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_rx_refill_thread *refill_thread;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	refill_thread = &dp_ext_hdl->refill_thread;
	if (refill_thread->enabled) {
		qdf_status = dp_rx_refill_thread_resume(refill_thread);
		if (qdf_status != QDF_STATUS_SUCCESS)
			return qdf_status;
	}

	qdf_status = dp_rx_tm_resume(&dp_ext_hdl->rx_tm_hdl);
ret:
	return qdf_status;
}

/**
 * dp_txrx_suspend() - suspend all threads
 * @soc: ol_txrx_soc_handle object
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_suspend(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_rx_refill_thread *refill_thread;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	refill_thread = &dp_ext_hdl->refill_thread;
	if (refill_thread->enabled) {
		qdf_status = dp_rx_refill_thread_suspend(refill_thread);
		if (qdf_status != QDF_STATUS_SUCCESS)
			return qdf_status;
	}

	qdf_status = dp_rx_tm_suspend(&dp_ext_hdl->rx_tm_hdl);
	if (QDF_IS_STATUS_ERROR(qdf_status) && refill_thread->enabled)
		dp_rx_refill_thread_resume(refill_thread);

ret:
	return qdf_status;
}

/**
 * dp_rx_enqueue_pkt() - enqueue packet(s) into the thread
 * @soc: ol_txrx_soc_handle object
 * @nbuf_list: list of packets to be queued into the rx_thread
 *
 * The function accepts a list of skbs connected by the skb->next pointer and
 * queues them into a RX thread to be sent to the stack.
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_rx_enqueue_pkt(ol_txrx_soc_handle soc, qdf_nbuf_t nbuf_list)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc || !nbuf_list) {
		qdf_status = QDF_STATUS_E_INVAL;
		dp_err("invalid input params soc %pK nbuf %pK"
		       , soc, nbuf_list);
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_enqueue_pkt(&dp_ext_hdl->rx_tm_hdl, nbuf_list);
ret:
	return qdf_status;
}

/**
 * dp_rx_gro_flush_ind() - Flush GRO packets for a given RX CTX Id
 * @soc: ol_txrx_soc_handle object
 * @rx_ctx_id: Context Id (Thread for which GRO packets need to be flushed)
 * @flush_code: flush_code differentiating normal_flush from low_tput_flush
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_rx_gro_flush_ind(ol_txrx_soc_handle soc, int rx_ctx_id,
			       enum dp_rx_gro_flush_code flush_code)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		dp_err("invalid input param soc %pK", soc);
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_gro_flush_ind(&dp_ext_hdl->rx_tm_hdl, rx_ctx_id,
					    flush_code);
ret:
	return qdf_status;
}

/**
 * dp_txrx_ext_dump_stats() - dump txrx external module stats
 * @soc: ol_txrx_soc_handle object
 * @stats_id: id  for the module whose stats are needed
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline QDF_STATUS dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
						uint8_t stats_id)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status;

	if (!soc) {
		dp_err("invalid input params soc %pK", soc);
		return QDF_STATUS_E_INVAL;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		return QDF_STATUS_E_FAULT;
	}

	if (stats_id == CDP_DP_RX_THREAD_STATS)
		qdf_status = dp_rx_tm_dump_stats(&dp_ext_hdl->rx_tm_hdl);
	else
		qdf_status = QDF_STATUS_E_INVAL;

	return qdf_status;
}

/**
 * dp_rx_get_napi_context() - get NAPI context for a RX CTX ID
 * @soc: ol_txrx_soc_handle object
 * @rx_ctx_id: RX context ID (RX thread ID) corresponding to which NAPI is
 *             needed
 *
 * Return: NULL on failure, else pointer to NAPI corresponding to rx_ctx_id
 */
static inline
struct napi_struct *dp_rx_get_napi_context(ol_txrx_soc_handle soc,
					   uint8_t rx_ctx_id)
{
	struct dp_txrx_handle *dp_ext_hdl;

	if (!soc) {
		dp_err("soc in NULL!");
		return NULL;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		dp_err("dp_ext_hdl in NULL!");
		return NULL;
	}

	return dp_rx_tm_get_napi_context(&dp_ext_hdl->rx_tm_hdl, rx_ctx_id);
}

/**
 * dp_txrx_set_cpu_mask() - set CPU mask for RX threads
 * @soc: ol_txrx_soc_handle object
 * @new_mask: New CPU mask pointer
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
static inline
QDF_STATUS dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc, qdf_cpu_mask *new_mask)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (!soc) {
		qdf_status = QDF_STATUS_E_INVAL;
		goto ret;
	}

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl) {
		qdf_status = QDF_STATUS_E_FAULT;
		goto ret;
	}

	qdf_status = dp_rx_tm_set_cpu_mask(&dp_ext_hdl->rx_tm_hdl, new_mask);

ret:
	return qdf_status;
}

#else

static inline
QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_flush_pkts_by_vdev_id(ol_txrx_soc_handle soc,
						       uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_resume(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_suspend(ol_txrx_soc_handle soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_enqueue_pkt(ol_txrx_soc_handle soc, qdf_nbuf_t nbuf_list)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS dp_rx_gro_flush_ind(ol_txrx_soc_handle soc, int rx_ctx_id,
			       enum dp_rx_gro_flush_code flush_code)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS dp_txrx_ext_dump_stats(ol_txrx_soc_handle soc,
						uint8_t stats_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
struct napi_struct *dp_rx_get_napi_context(ol_txrx_soc_handle soc,
					   uint8_t rx_ctx_id)
{
	return NULL;
}

static inline
QDF_STATUS dp_txrx_set_cpu_mask(ol_txrx_soc_handle soc, qdf_cpu_mask *new_mask)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* FEATURE_WLAN_DP_RX_THREADS */

/**
 * dp_rx_tm_get_pending() - get number of frame in thread
 * nbuf queue pending
 * @soc: ol_txrx_soc_handle object
 *
 * Return: number of frames
 */
int dp_rx_tm_get_pending(ol_txrx_soc_handle soc);

#ifdef DP_MEM_PRE_ALLOC
/**
 * dp_prealloc_init() - Pre-allocate DP memory
 * @ctrl_psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS on success, error qdf status on failure
 */
QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc);

/**
 * dp_prealloc_deinit() - Free pre-alloced DP memory
 *
 * Return: None
 */
void dp_prealloc_deinit(void);

/**
 * dp_prealloc_get_context_memory() - gets pre-alloc DP context memory from
 *				      global pool
 * @ctxt_type: type of DP context
 *
 * This is done only as part of init happening in a single context. Hence
 * no lock is used for protection
 *
 * Return: Address of context
 */
void *dp_prealloc_get_context_memory(uint32_t ctxt_type);

/**
 * dp_prealloc_put_context_memory() - puts back pre-alloc DP context memory to
 *				      global pool
 * @ctxt_type: type of DP context
 * @vaddr: address of DP context
 *
 * This is done only as part of de-init happening in a single context. Hence
 * no lock is used for protection
 *
 * Return: Failure if address not found
 */
QDF_STATUS dp_prealloc_put_context_memory(uint32_t ctxt_type, void *vaddr);

/**
 * dp_prealloc_get_coherent() - gets pre-alloc DP memory
 * @size: size of memory needed
 * @base_vaddr_unaligned: Unaligned virtual address.
 * @paddr_unaligned: Unaligned physical address.
 * @paddr_aligned: Aligned physical address.
 * @align: Base address alignment.
 * @align: alignment needed
 * @ring_type: HAL ring type
 *
 * The function does not handle concurrent access to pre-alloc memory.
 * All ring memory allocation from pre-alloc memory should happen from single
 * context to avoid race conditions.
 *
 * Return: unaligned virtual address if success or null if memory alloc fails.
 */
void *dp_prealloc_get_coherent(uint32_t *size, void **base_vaddr_unaligned,
			       qdf_dma_addr_t *paddr_unaligned,
			       qdf_dma_addr_t *paddr_aligned,
			       uint32_t align,
			       uint32_t ring_type);

/**
 * dp_prealloc_put_coherent() - puts back pre-alloc DP memory
 * @size: size of memory to be returned
 * @base_vaddr_unaligned: Unaligned virtual address.
 * @paddr_unaligned: Unaligned physical address.
 *
 * Return: None
 */
void dp_prealloc_put_coherent(qdf_size_t size, void *vaddr_unligned,
			      qdf_dma_addr_t paddr);

/**
 * dp_prealloc_get_multi_page() - gets pre-alloc DP multi-pages memory
 * @src_type: the source that do memory allocation
 * @element_size: single element size
 * @element_num: total number of elements should be allocated
 * @pages: multi page information storage
 * @cacheable: coherent memory or cacheable memory
 *
 * Return: None.
 */
void dp_prealloc_get_multi_pages(uint32_t src_type,
				 size_t element_size,
				 uint16_t element_num,
				 struct qdf_mem_multi_page_t *pages,
				 bool cacheable);

/**
 * dp_prealloc_put_multi_pages() - puts back pre-alloc DP multi-pages memory
 * @src_type: the source that do memory freement
 * @pages: multi page information storage
 *
 * Return: None
 */
void dp_prealloc_put_multi_pages(uint32_t src_type,
				 struct qdf_mem_multi_page_t *pages);

/**
 * dp_prealloc_get_consistent_mem_unaligned() - gets pre-alloc unaligned
						consistent memory
 * @size: total memory size
 * @base_addr: pointer to dma address
 * @ring_type: HAL ring type that requires memory
 *
 * Return: memory virtual address pointer, NULL if fail
 */
void *dp_prealloc_get_consistent_mem_unaligned(size_t size,
					       qdf_dma_addr_t *base_addr,
					       uint32_t ring_type);

/**
 * dp_prealloc_put_consistent_mem_unaligned() - puts back pre-alloc unaligned
						consistent memory
 * @va_unaligned: memory virtual address pointer
 *
 * Return: None
 */
void dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned);

#else
static inline
QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_prealloc_deinit(void) { }

#endif

#endif /* _DP_TXRX_H */
