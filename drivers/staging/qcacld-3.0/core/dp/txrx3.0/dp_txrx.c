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

#include <wlan_objmgr_pdev_obj.h>
#include <dp_txrx.h>
#include <dp_types.h>
#include <dp_internal.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_misc.h>
#include <dp_tx_desc.h>
#include <dp_rx.h>
#include <ce_api.h>
#include <ce_internal.h>
#include <wlan_cfg.h>

/**
 * dp_rx_refill_thread_schedule() - Schedule rx refill thread
 * @soc: ol_txrx_soc_handle object
 *
 */
#ifdef WLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
static void dp_rx_refill_thread_schedule(ol_txrx_soc_handle soc)
{
	struct dp_rx_refill_thread *rx_thread;
	struct dp_txrx_handle *dp_ext_hdl;

	if (!soc)
		return;

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl)
		return;

	rx_thread = &dp_ext_hdl->refill_thread;
	qdf_set_bit(RX_REFILL_POST_EVENT, &rx_thread->event_flag);
	qdf_wake_up_interruptible(&rx_thread->wait_q);
}
#else
static void dp_rx_refill_thread_schedule(ol_txrx_soc_handle soc)
{
}
#endif

QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint8_t num_dp_rx_threads;
	struct dp_pdev *pdev;
	struct dp_soc *dp_soc;

	if (qdf_unlikely(!soc)) {
		dp_err("soc is NULL");
		return 0;
	}

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(cdp_soc_t_to_dp_soc(soc),
						  pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return 0;
	}

	dp_ext_hdl = qdf_mem_malloc(sizeof(*dp_ext_hdl));
	if (!dp_ext_hdl) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}

	dp_info("dp_txrx_handle allocated");
	dp_ext_hdl->soc = soc;
	dp_ext_hdl->pdev = dp_pdev_to_cdp_pdev(pdev);
	cdp_soc_set_dp_txrx_handle(soc, dp_ext_hdl);
	qdf_mem_copy(&dp_ext_hdl->config, config, sizeof(*config));
	dp_ext_hdl->rx_tm_hdl.txrx_handle_cmn =
				dp_txrx_get_cmn_hdl_frm_ext_hdl(dp_ext_hdl);

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (wlan_cfg_is_rx_refill_buffer_pool_enabled(dp_soc->wlan_cfg_ctx)) {
		dp_ext_hdl->refill_thread.soc = soc;
		dp_ext_hdl->refill_thread.enabled = true;
		qdf_status =
			dp_rx_refill_thread_init(&dp_ext_hdl->refill_thread);
		if (qdf_status != QDF_STATUS_SUCCESS) {
			dp_err("Failed to initialize RX refill thread status:%d",
			       qdf_status);
			return qdf_status;
		}
		cdp_register_rx_refill_thread_sched_handler(soc,
						dp_rx_refill_thread_schedule);
	}

	num_dp_rx_threads = cdp_get_num_rx_contexts(soc);

	if (dp_ext_hdl->config.enable_rx_threads) {
		qdf_status = dp_rx_tm_init(&dp_ext_hdl->rx_tm_hdl,
					   num_dp_rx_threads);
	}

	return qdf_status;
}

QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;
	struct dp_soc *dp_soc;

	if (!soc)
		return QDF_STATUS_E_INVAL;

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl)
		return QDF_STATUS_E_FAULT;

	dp_soc = cdp_soc_t_to_dp_soc(soc);
	if (wlan_cfg_is_rx_refill_buffer_pool_enabled(dp_soc->wlan_cfg_ctx)) {
		dp_rx_refill_thread_deinit(&dp_ext_hdl->refill_thread);
		dp_ext_hdl->refill_thread.soc = NULL;
		dp_ext_hdl->refill_thread.enabled = false;
	}

	if (dp_ext_hdl->config.enable_rx_threads)
		dp_rx_tm_deinit(&dp_ext_hdl->rx_tm_hdl);

	qdf_mem_free(dp_ext_hdl);
	dp_info("dp_txrx_handle_t de-allocated");

	cdp_soc_set_dp_txrx_handle(soc, NULL);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_tm_get_pending() - get number of frame in thread
 * nbuf queue pending
 * @soc: ol_txrx_soc_handle object
 *
 * Return: number of frames
 */
#ifdef FEATURE_WLAN_DP_RX_THREADS
int dp_rx_tm_get_pending(ol_txrx_soc_handle soc)
{
	int i;
	int num_pending = 0;
	struct dp_rx_thread *rx_thread;
	struct dp_txrx_handle *dp_ext_hdl;
	struct dp_rx_tm_handle *rx_tm_hdl;

	if (!soc)
		return 0;

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl)
		return 0;

	rx_tm_hdl = &dp_ext_hdl->rx_tm_hdl;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		rx_thread = rx_tm_hdl->rx_thread[i];
		if (!rx_thread)
			continue;
		num_pending += qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue);
	}

	if (num_pending)
		dp_debug("pending frames in thread queue %d", num_pending);

	return num_pending;
}
#else
int dp_rx_tm_get_pending(ol_txrx_soc_handle soc)
{
	return 0;
}
#endif

#ifdef DP_MEM_PRE_ALLOC

/* Max entries in FISA Flow table */
#define FISA_RX_FT_SIZE 128

/* Num elements in REO ring */
#define REO_DST_RING_SIZE 1024

/* Num elements in TCL Data ring */
#define TCL_DATA_RING_SIZE 5120

/* Num elements in WBM2SW ring */
#define WBM2SW_RELEASE_RING_SIZE 8192

/* Num elements in WBM Idle Link */
#define WBM_IDLE_LINK_RING_SIZE (32 * 1024)

/* Num TX desc in TX desc pool */
#define DP_TX_DESC_POOL_SIZE 6144

/**
 * struct dp_consistent_prealloc - element representing DP pre-alloc memory
 * @ring_type: HAL ring type
 * @size: size of pre-alloc memory
 * @in_use: whether this element is in use (occupied)
 * @va_unaligned: Unaligned virtual address
 * @va_aligned: aligned virtual address.
 * @pa_unaligned: Unaligned physical address.
 * @pa_aligned: Aligned physical address.
 */

struct dp_consistent_prealloc {
	enum hal_ring_type ring_type;
	uint32_t size;
	uint8_t in_use;
	void *va_unaligned;
	void *va_aligned;
	qdf_dma_addr_t pa_unaligned;
	qdf_dma_addr_t pa_aligned;
};

/**
 * struct dp_multi_page_prealloc -  element representing DP pre-alloc multiple
				    pages memory
 * @desc_type: source descriptor type for memory allocation
 * @element_size: single element size
 * @element_num: total number of elements should be allocated
 * @in_use: whether this element is in use (occupied)
 * @cacheable: coherent memory or cacheable memory
 * @pages: multi page information storage
 */
struct dp_multi_page_prealloc {
	enum dp_desc_type desc_type;
	size_t element_size;
	uint16_t element_num;
	bool in_use;
	bool cacheable;
	struct qdf_mem_multi_page_t pages;
};

/**
 * struct dp_consistent_prealloc_unaligned - element representing DP pre-alloc
					     unaligned memory
 * @ring_type: HAL ring type
 * @size: size of pre-alloc memory
 * @in_use: whether this element is in use (occupied)
 * @va_unaligned: unaligned virtual address
 * @pa_unaligned: unaligned physical address
 */
struct dp_consistent_prealloc_unaligned {
	enum hal_ring_type ring_type;
	uint32_t size;
	bool in_use;
	void *va_unaligned;
	qdf_dma_addr_t pa_unaligned;
};

/**
 * struct dp_prealloc_context - element representing DP prealloc context memory
 * @ctxt_type: DP context type
 * @size: size of pre-alloc memory
 * @in_use: check if element is being used
 * @is_critical: critical prealloc failure would cause prealloc_init to fail
 * @addr: address of memory allocated
 */
struct dp_prealloc_context {
	enum dp_ctxt_type ctxt_type;
	uint32_t size;
	bool in_use;
	bool is_critical;
	void *addr;
};

static struct dp_prealloc_context g_dp_context_allocs[] = {
	{DP_PDEV_TYPE, (sizeof(struct dp_pdev)), false,  true, NULL},
#ifdef WLAN_FEATURE_DP_RX_RING_HISTORY
	/* 4 Rx ring history */
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	{DP_RX_RING_HIST_TYPE, sizeof(struct dp_rx_history), false, false,
	 NULL},
	/* 1 Rx error ring history */
	{DP_RX_ERR_RING_HIST_TYPE, sizeof(struct dp_rx_err_history),
	 false, false, NULL},
#ifndef RX_DEFRAG_DO_NOT_REINJECT
	/* 1 Rx reinject ring history */
	{DP_RX_REINJECT_RING_HIST_TYPE, sizeof(struct dp_rx_reinject_history),
	 false, false, NULL},
#endif	/* RX_DEFRAG_DO_NOT_REINJECT */
	/* 1 Rx refill ring history */
	{DP_RX_REFILL_RING_HIST_TYPE, sizeof(struct dp_rx_refill_history),
	false, false, NULL},
#endif	/* WLAN_FEATURE_DP_RX_RING_HISTORY */
#ifdef DP_TX_HW_DESC_HISTORY
	{DP_TX_HW_DESC_HIST_TYPE, sizeof(struct dp_tx_hw_desc_history),
	false, false, NULL},
#endif
#ifdef WLAN_FEATURE_DP_TX_DESC_HISTORY
	{DP_TX_TCL_HIST_TYPE, sizeof(struct dp_tx_tcl_history),
	 false, false, NULL},
	{DP_TX_COMP_HIST_TYPE, sizeof(struct dp_tx_comp_history),
	 false, false, NULL},
#endif	/* WLAN_FEATURE_DP_TX_DESC_HISTORY */
#ifdef WLAN_SUPPORT_RX_FISA
	{DP_FISA_RX_FT_TYPE, sizeof(struct dp_fisa_rx_sw_ft) * FISA_RX_FT_SIZE,
	 false, true, NULL},
#endif
};

static struct  dp_consistent_prealloc g_dp_consistent_allocs[] = {
	/* 5 REO DST rings */
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 3 TCL data rings */
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	{TCL_DATA, 0, 0, NULL, NULL, 0, 0},
	/* 4 WBM2SW rings */
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, 0, 0, NULL, 0, 0},
	/* SW2WBM link descriptor return ring */
	{SW2WBM_RELEASE, 0, 0, NULL, 0, 0},
	/* 1 WBM idle link desc ring */
	{WBM_IDLE_LINK, (sizeof(struct wbm_link_descriptor_ring)) * WBM_IDLE_LINK_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 2 RXDMA DST ERR rings */
	{RXDMA_DST, 0, 0, NULL, NULL, 0, 0},
	{RXDMA_DST, 0, 0, NULL, NULL, 0, 0},
	/* REFILL ring 0 */
	{RXDMA_BUF, (sizeof(struct wbm_buffer_ring)) * WLAN_CFG_RXDMA_REFILL_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* REO Exception ring */
	{REO_EXCEPTION, 0, 0, NULL, NULL, 0, 0},
};

/* Number of HW link descriptors needed (rounded to power of 2) */
#define NUM_HW_LINK_DESCS (32 * 1024)

/* Size in bytes of HW LINK DESC */
#define HW_LINK_DESC_SIZE 128

/* Size in bytes of TX Desc (rounded to power of 2) */
#define TX_DESC_SIZE 128

/* Size in bytes of TX TSO Desc (rounded to power of 2) */
#define TX_TSO_DESC_SIZE 256

/* Size in bytes of TX TSO Num Seg Desc (rounded to power of 2) */
#define TX_TSO_NUM_SEG_DESC_SIZE 16

#define NON_CACHEABLE 0
#define CACHEABLE 1

static struct  dp_multi_page_prealloc g_dp_multi_page_allocs[] = {
	/* 4 TX DESC pools */
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },

	/* 4 Tx EXT DESC NON Cacheable pools */
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, 0, 0,
	 NON_CACHEABLE, { 0 } },

	/* 4 Tx EXT DESC Link Cacheable pools */
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), 0, 0,
	 CACHEABLE, { 0 } },

	/* 4 TX TSO DESC pools */
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, 0, 0, CACHEABLE, { 0 } },

	/* 4 TX TSO NUM SEG DESC pools */
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, 0, 0,
	 CACHEABLE, { 0 } },

	/* DP RX DESCs BUF pools */
	{DP_RX_DESC_BUF_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE * WLAN_CFG_RXDMA_REFILL_RING_SIZE, 0, CACHEABLE, { 0 } },

#ifdef DISABLE_MON_CONFIG
	/* no op */
#else
	/* 2 DP RX DESCs Status pools */
	{DP_RX_DESC_STATUS_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE + 1, 0, CACHEABLE, { 0 } },
	{DP_RX_DESC_STATUS_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE + 1, 0, CACHEABLE, { 0 } },
#endif
	/* DP HW Link DESCs pools */
	{DP_HW_LINK_DESC_TYPE, HW_LINK_DESC_SIZE, NUM_HW_LINK_DESCS, 0, NON_CACHEABLE, { 0 } },

};

static struct dp_consistent_prealloc_unaligned
		g_dp_consistent_unaligned_allocs[] = {
	/* CE-0 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 16 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-1 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 512 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 512
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
	/* CE-2 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 32 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 32
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
	/* CE-3 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 32 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-4 */
	{CE_SRC, (sizeof(struct ce_srng_src_desc) * 256 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	/* CE-5 */
	{CE_DST, (sizeof(struct ce_srng_dest_desc) * 512 + CE_DESC_RING_ALIGN),
	 false, NULL, 0},
	{CE_DST_STATUS, (sizeof(struct ce_srng_dest_status_desc) * 512
	 + CE_DESC_RING_ALIGN), false, NULL, 0},
};

void dp_prealloc_deinit(void)
{
	int i;
	struct dp_prealloc_context *cp;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	struct dp_consistent_prealloc_unaligned *up;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		dp_warn("qdf_ctx is NULL");
		return;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];

		if (p->in_use)
			dp_warn("i %d: consistent_mem in use while free", i);

		if (p->va_aligned) {
			dp_debug("i %d: va aligned %pK pa aligned %pK size %d",
				 i, p->va_aligned, (void *)p->pa_aligned,
				 p->size);
			qdf_mem_free_consistent(qdf_ctx, qdf_ctx->dev,
						p->size,
						p->va_unaligned,
						p->pa_unaligned, 0);
			qdf_mem_zero(p, sizeof(*p));
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (mp->in_use)
			dp_warn("i %d: multi-page mem in use while free", i);

		if (mp->pages.num_pages) {
			dp_info("i %d: type %d cacheable_pages %pK dma_pages %pK num_pages %d",
				i, mp->desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			qdf_mem_multi_pages_free(qdf_ctx, &mp->pages,
						 0, mp->cacheable);
			qdf_mem_zero(mp, sizeof(*mp));
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (qdf_unlikely(up->in_use))
			dp_info("i %d: unaligned mem in use while free", i);

		if (up->va_unaligned) {
			dp_info("i %d: va unalign %pK pa unalign %pK size %d",
				i, up->va_unaligned,
				(void *)up->pa_unaligned, up->size);
			qdf_mem_free_consistent(qdf_ctx, qdf_ctx->dev,
						up->size,
						up->va_unaligned,
						up->pa_unaligned, 0);
			qdf_mem_zero(up, sizeof(*up));
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];
		if (qdf_unlikely(cp->in_use))
			dp_warn("i %d: context in use while free", i);

		if (cp->addr) {
			qdf_mem_free(cp->addr);
			cp->addr = NULL;
		}
	}
}

#ifdef CONFIG_BERYLLIUM
/**
 * dp_get_tcl_data_srng_entrysize() - Get the tcl data srng entry
 *  size
 *
 * Return: TCL data srng entry size
 */
static inline uint32_t dp_get_tcl_data_srng_entrysize(void)
{
	return sizeof(struct tcl_data_cmd);
}
#else
static inline uint32_t dp_get_tcl_data_srng_entrysize(void)
{
	return (sizeof(struct tlv_32_hdr) + sizeof(struct tcl_data_cmd));
}
#endif

/**
 * dp_update_mem_size_by_ring_type() - Update srng memory size based
 *  on ring type and the corresponding ini configuration
 * @cfg: prealloc related cfg params
 * @ring_type: srng type
 * @mem_size: memory size to be updated
 *
 * Return: None
 */
static void
dp_update_mem_size_by_ring_type(struct wlan_dp_prealloc_cfg *cfg,
				enum hal_ring_type ring_type,
				uint32_t *mem_size)
{
	switch (ring_type) {
	case TCL_DATA:
		*mem_size = dp_get_tcl_data_srng_entrysize() *
			    cfg->num_tx_ring_entries;
		return;
	case WBM2SW_RELEASE:
		*mem_size = (sizeof(struct wbm_release_ring)) *
			    cfg->num_tx_comp_ring_entries;
		return;
	case SW2WBM_RELEASE:
		*mem_size = (sizeof(struct wbm_release_ring)) *
			    cfg->num_wbm_rel_ring_entries;
		return;
	case RXDMA_DST:
		*mem_size = (sizeof(struct reo_entrance_ring)) *
			    cfg->num_rxdma_err_dst_ring_entries;
		return;
	case REO_EXCEPTION:
		*mem_size = (sizeof(struct reo_destination_ring)) *
			    cfg->num_reo_exception_ring_entries;
		return;
	default:
		return;
	}
}

/**
 * dp_update_num_elements_by_desc_type() - Update num of descriptors based
 *  on type and the corresponding ini configuration
 * @cfg: prealloc related cfg params
 * @desc_type: descriptor type
 * @num_elements: num of descriptor elements
 *
 * Return: None
 */
static void
dp_update_num_elements_by_desc_type(struct wlan_dp_prealloc_cfg *cfg,
				    enum dp_desc_type desc_type,
				    uint16_t *num_elements)
{
	switch (desc_type) {
	case DP_TX_DESC_TYPE:
		*num_elements = cfg->num_tx_desc;
		return;
	case DP_TX_EXT_DESC_TYPE:
	case DP_TX_EXT_DESC_LINK_TYPE:
	case DP_TX_TSO_DESC_TYPE:
	case DP_TX_TSO_NUM_SEG_TYPE:
		*num_elements = cfg->num_tx_ext_desc;
		return;
	default:
		return;
	}
}

QDF_STATUS dp_prealloc_init(struct cdp_ctrl_objmgr_psoc *ctrl_psoc)
{
	int i;
	struct dp_prealloc_context *cp;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	struct dp_consistent_prealloc_unaligned *up;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	struct wlan_dp_prealloc_cfg cfg;

	if (!qdf_ctx || !ctrl_psoc) {
		dp_err("qdf_ctx is NULL");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	wlan_cfg_get_prealloc_cfg(ctrl_psoc, &cfg);

	/*Context pre-alloc*/
	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];
		cp->addr = qdf_mem_malloc(cp->size);

		if (qdf_unlikely(!cp->addr) && cp->is_critical) {
			dp_warn("i %d: unable to preallocate %d bytes memory!",
				i, cp->size);
			break;
		}
	}

	if (i != QDF_ARRAY_SIZE(g_dp_context_allocs)) {
		dp_err("unable to allocate context memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		p->in_use = 0;
		dp_update_mem_size_by_ring_type(&cfg, p->ring_type, &p->size);
		p->va_aligned =
			qdf_aligned_mem_alloc_consistent(qdf_ctx,
							 &p->size,
							 &p->va_unaligned,
							 &p->pa_unaligned,
							 &p->pa_aligned,
							 DP_RING_BASE_ALIGN);
		if (qdf_unlikely(!p->va_unaligned)) {
			dp_warn("i %d: unable to preallocate %d bytes memory!",
				i, p->size);
			break;
		}
		dp_debug("i %d: va aligned %pK pa aligned %pK size %d",
			 i, p->va_aligned, (void *)p->pa_aligned, p->size);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_consistent_allocs)) {
		dp_err("unable to allocate consistent memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];
		mp->in_use = false;
		dp_update_num_elements_by_desc_type(&cfg, mp->desc_type,
						    &mp->element_num);
		qdf_mem_multi_pages_alloc(qdf_ctx, &mp->pages,
					  mp->element_size,
					  mp->element_num,
					  0, mp->cacheable);
		if (qdf_unlikely(!mp->pages.num_pages)) {
			dp_warn("i %d: preallocate %d bytes multi-pages failed!",
				i, (int)(mp->element_size * mp->element_num));
			break;
		}

		mp->pages.is_mem_prealloc = true;
		dp_info("i %d: cacheable_pages %pK dma_pages %pK num_pages %d",
			i, mp->pages.cacheable_pages,
			mp->pages.dma_pages,
			mp->pages.num_pages);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_multi_page_allocs)) {
		dp_err("unable to allocate multi-pages memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];
		up->in_use = 0;
		up->va_unaligned = qdf_mem_alloc_consistent(qdf_ctx,
							    qdf_ctx->dev,
							    up->size,
							    &up->pa_unaligned);
		if (qdf_unlikely(!up->va_unaligned)) {
			dp_warn("i %d: fail to prealloc unaligned %d bytes!",
				i, up->size);
			break;
		}
		dp_info("i %d: va unalign %pK pa unalign %pK size %d",
			i, up->va_unaligned,
			(void *)up->pa_unaligned, up->size);
	}

	if (i != QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs)) {
		dp_info("unable to allocate unaligned memory!");
		/*
		 * Only if unaligned memory prealloc fail, is deinit
		 * necessary for all other DP srng/multi-pages memory?
		 */
		goto deinit;
	}

	return QDF_STATUS_SUCCESS;
deinit:
	dp_prealloc_deinit();
	return QDF_STATUS_E_FAILURE;
}

void *dp_prealloc_get_context_memory(uint32_t ctxt_type)
{
	int i;
	struct dp_prealloc_context *cp;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];

		if ((ctxt_type == cp->ctxt_type) && !cp->in_use &&
		    cp->addr) {
			cp->in_use = true;
			return cp->addr;
		}
	}

	return NULL;
}

QDF_STATUS dp_prealloc_put_context_memory(uint32_t ctxt_type, void *vaddr)
{
	int i;
	struct dp_prealloc_context *cp;

	if (!vaddr)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_context_allocs); i++) {
		cp = &g_dp_context_allocs[i];

		if ((ctxt_type == cp->ctxt_type) && vaddr == cp->addr) {
			qdf_mem_zero(cp->addr, cp->size);
			cp->in_use = false;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

void *dp_prealloc_get_coherent(uint32_t *size, void **base_vaddr_unaligned,
			       qdf_dma_addr_t *paddr_unaligned,
			       qdf_dma_addr_t *paddr_aligned,
			       uint32_t align,
			       uint32_t ring_type)
{
	int i;
	struct dp_consistent_prealloc *p;
	void *va_aligned = NULL;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->ring_type == ring_type && !p->in_use &&
		    p->va_unaligned && *size <= p->size) {
			p->in_use = 1;
			*base_vaddr_unaligned = p->va_unaligned;
			*paddr_unaligned = p->pa_unaligned;
			*paddr_aligned = p->pa_aligned;
			va_aligned = p->va_aligned;
			*size = p->size;
			dp_debug("index %i -> ring type %s va-aligned %pK", i,
				dp_srng_get_str_from_hal_ring_type(ring_type),
				va_aligned);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_allocs))
		dp_info("unable to allocate memory for ring type %s (%d) size %d",
			dp_srng_get_str_from_hal_ring_type(ring_type),
			ring_type, *size);
	return va_aligned;
}

void dp_prealloc_put_coherent(qdf_size_t size, void *vaddr_unligned,
			      qdf_dma_addr_t paddr)
{
	int i;
	struct dp_consistent_prealloc *p;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->va_unaligned == vaddr_unligned) {
			dp_debug("index %d, returned", i);
			p->in_use = 0;
			qdf_mem_zero(p->va_unaligned, p->size);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_allocs))
		dp_err("unable to find vaddr %pK", vaddr_unligned);
}

void dp_prealloc_get_multi_pages(uint32_t desc_type,
				 size_t element_size,
				 uint16_t element_num,
				 struct qdf_mem_multi_page_t *pages,
				 bool cacheable)
{
	int i;
	struct dp_multi_page_prealloc *mp;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type && !mp->in_use &&
		    mp->pages.num_pages && element_size == mp->element_size &&
		    element_num <= mp->element_num) {
			mp->in_use = true;
			*pages = mp->pages;

			dp_info("i %d: desc_type %d cacheable_pages %pK"
				"dma_pages %pK num_pages %d",
				i, desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			break;
		}
	}
}

void dp_prealloc_put_multi_pages(uint32_t desc_type,
				 struct qdf_mem_multi_page_t *pages)
{
	int i;
	struct dp_multi_page_prealloc *mp;
	bool mp_found = false;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type) {
			/* compare different address by cacheable flag */
			mp_found = mp->cacheable ?
				(mp->pages.cacheable_pages ==
				 pages->cacheable_pages) :
				(mp->pages.dma_pages == pages->dma_pages);
			/* find it, put back to prealloc pool */
			if (mp_found) {
				dp_info("i %d: desc_type %d returned",
					i, desc_type);
				mp->in_use = false;
				qdf_mem_multi_pages_zero(&mp->pages,
							 mp->cacheable);
				break;
			}
		}
	}

	if (qdf_unlikely(!mp_found))
		dp_warn("Not prealloc pages %pK desc_type %d cacheable_pages %pK dma_pages %pK",
			pages,
			desc_type,
			pages->cacheable_pages,
			pages->dma_pages);
}

void *dp_prealloc_get_consistent_mem_unaligned(size_t size,
					       qdf_dma_addr_t *base_addr,
					       uint32_t ring_type)
{
	int i;
	struct dp_consistent_prealloc_unaligned *up;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (ring_type == up->ring_type && size == up->size &&
		    up->va_unaligned && !up->in_use) {
			up->in_use = true;
			*base_addr = up->pa_unaligned;
			dp_info("i %d: va unalign %pK pa unalign %pK size %d",
				i, up->va_unaligned,
				(void *)up->pa_unaligned, up->size);
			return up->va_unaligned;
		}
	}

	return NULL;
}

void dp_prealloc_put_consistent_mem_unaligned(void *va_unaligned)
{
	int i;
	struct dp_consistent_prealloc_unaligned *up;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs); i++) {
		up = &g_dp_consistent_unaligned_allocs[i];

		if (va_unaligned == up->va_unaligned) {
			dp_info("index %d, returned", i);
			up->in_use = false;
			qdf_mem_zero(up->va_unaligned, up->size);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_unaligned_allocs))
		dp_err("unable to find vaddr %pK", va_unaligned);
}
#endif
