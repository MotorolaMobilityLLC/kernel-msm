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

/**
 * @file ol_tx_desc.h
 * @brief API definitions for the tx descriptor module within the data SW.
 */
#ifndef _OL_TX_DESC__H_
#define _OL_TX_DESC__H_

#include "queue.h"          /* TAILQ_HEAD */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */
#include <ol_txrx_internal.h>   /*TXRX_ASSERT2 */
#include <ol_htt_tx_api.h>

#define DIV_BY_8	3
#define DIV_BY_32	5
#define MOD_BY_8	0x7
#define MOD_BY_32	0x1F

struct ol_tx_desc_t *
ol_tx_desc_alloc_wrapper(struct ol_txrx_pdev_t *pdev,
			 struct ol_txrx_vdev_t *vdev,
			 struct ol_txrx_msdu_info_t *msdu_info);


/**
 * @brief Allocate and initialize a tx descriptor for a LL system.
 * @details
 *  Allocate a tx descriptor pair for a new tx frame - a SW tx descriptor
 *  for private use within the host data SW, and a HTT tx descriptor for
 *  downloading tx meta-data to the target FW/HW.
 *  Fill in the fields of this pair of tx descriptors based on the
 *  information in the netbuf.
 *  For LL, this includes filling in a fragmentation descriptor to
 *  specify to the MAC HW where to find the tx frame's fragments.
 *
 * @param pdev - the data physical device sending the data
 *      (for accessing the tx desc pool)
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param netbuf - the tx frame
 * @param msdu_info - tx meta-data
 */
struct ol_tx_desc_t *ol_tx_desc_ll(struct ol_txrx_pdev_t *pdev,
				   struct ol_txrx_vdev_t *vdev,
				   qdf_nbuf_t netbuf,
				   struct ol_txrx_msdu_info_t *msdu_info);


/**
 * @brief Allocate and initialize a tx descriptor for a HL system.
 * @details
 *  Allocate a tx descriptor pair for a new tx frame - a SW tx descriptor
 *  for private use within the host data SW, and a HTT tx descriptor for
 *  downloading tx meta-data to the target FW/HW.
 *  Fill in the fields of this pair of tx descriptors based on the
 *  information in the netbuf.
 *
 * @param pdev - the data physical device sending the data
 *      (for accessing the tx desc pool)
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param netbuf - the tx frame
 * @param msdu_info - tx meta-data
 */
struct ol_tx_desc_t *
ol_tx_desc_hl(
		struct ol_txrx_pdev_t *pdev,
		struct ol_txrx_vdev_t *vdev,
		qdf_nbuf_t netbuf,
		struct ol_txrx_msdu_info_t *msdu_info);


/**
 * @brief Use a tx descriptor ID to find the corresponding descriptor object.
 *
 * @param pdev - the data physical device sending the data
 * @param tx_desc_id - the ID of the descriptor in question
 * @return the descriptor object that has the specified ID
 */
static inline struct ol_tx_desc_t *ol_tx_desc_find(
			struct ol_txrx_pdev_t *pdev, uint16_t tx_desc_id)
{
	void **td_base = (void **)pdev->tx_desc.desc_pages.cacheable_pages;

	return &((union ol_tx_desc_list_elem_t *)
		(td_base[tx_desc_id >> pdev->tx_desc.page_divider] +
		(pdev->tx_desc.desc_reserved_size *
		(tx_desc_id & pdev->tx_desc.offset_filter))))->tx_desc;
}

/**
 * @brief Use a tx descriptor ID to find the corresponding descriptor object
 *    and add sanity check.
 *
 * @param pdev - the data physical device sending the data
 * @param tx_desc_id - the ID of the descriptor in question
 * @return the descriptor object that has the specified ID,
 *    if failure, will return NULL.
 */

#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
static inline struct ol_tx_desc_t *
ol_tx_desc_find_check(struct ol_txrx_pdev_t *pdev, u_int16_t tx_desc_id)
{
	struct ol_tx_desc_t *tx_desc;

	if (tx_desc_id >= pdev->tx_desc.pool_size)
		return NULL;

	tx_desc = ol_tx_desc_find(pdev, tx_desc_id);

	if (tx_desc->pkt_type == ol_tx_frm_freed)
		return NULL;

	return tx_desc;
}

#else

static inline struct ol_tx_desc_t *
ol_tx_desc_find_check(struct ol_txrx_pdev_t *pdev, u_int16_t tx_desc_id)
{
	struct ol_tx_desc_t *tx_desc;

	if (tx_desc_id >= pdev->tx_desc.pool_size)
		return NULL;

	tx_desc = ol_tx_desc_find(pdev, tx_desc_id);

	/* check against invalid tx_desc_id */
	if (ol_cfg_is_high_latency(pdev->ctrl_pdev) && !tx_desc->vdev)
		return NULL;

	return tx_desc;
}
#endif

/**
 * @brief Free a list of tx descriptors and the tx frames they refer to.
 * @details
 *  Free a batch of "standard" tx descriptors and their tx frames.
 *  Free each tx descriptor, by returning it to the freelist.
 *  Unmap each netbuf, and free the netbufs as a batch.
 *  Irregular tx frames like TSO or management frames that require
 *  special handling are processed by the ol_tx_desc_frame_free_nonstd
 *  function rather than this function.
 *
 * @param pdev - the data physical device that sent the data
 * @param tx_descs - a list of SW tx descriptors for the tx frames
 * @param had_error - bool indication of whether the transmission failed.
 *            This is provided to callback functions that get notified of
 *            the tx frame completion.
 */
void ol_tx_desc_frame_list_free(struct ol_txrx_pdev_t *pdev,
				ol_tx_desc_list *tx_descs, int had_error);

/**
 * @brief Free a non-standard tx frame and its tx descriptor.
 * @details
 *  Check the tx frame type (e.g. TSO vs. management) to determine what
 *  special steps, if any, need to be performed prior to freeing the
 *  tx frame and its tx descriptor.
 *  This function can also be used to free single standard tx frames.
 *  After performing any special steps based on tx frame type, free the
 *  tx descriptor, i.e. return it to the freelist, and unmap and
 *  free the netbuf referenced by the tx descriptor.
 *
 * @param pdev - the data physical device that sent the data
 * @param tx_desc - the SW tx descriptor for the tx frame that was sent
 * @param had_error - bool indication of whether the transmission failed.
 *            This is provided to callback functions that get notified of
 *            the tx frame completion.
 */
void ol_tx_desc_frame_free_nonstd(struct ol_txrx_pdev_t *pdev,
				  struct ol_tx_desc_t *tx_desc, int had_error);

/*
 * @brief Determine the ID of a tx descriptor.
 *
 * @param pdev - the physical device that is sending the data
 * @param tx_desc - the descriptor whose ID is being determined
 * @return numeric ID that uniquely identifies the tx descriptor
 */
static inline uint16_t
ol_tx_desc_id(struct ol_txrx_pdev_t *pdev, struct ol_tx_desc_t *tx_desc)
{
	TXRX_ASSERT2(tx_desc->id < pdev->tx_desc.pool_size);
	return tx_desc->id;
}

/*
 * @brief Retrieves the beacon headr for the vdev
 * @param pdev - opaque pointe to scn
 * @param vdevid - vdev id
 * @return void pointer to the beacon header for the given vdev
 */

void *ol_ath_get_bcn_header(struct cdp_cfg *cfg_pdev, A_UINT32 vdev_id);

/*
 * @brief Free a tx descriptor, without freeing the matching frame.
 * @details
 *  This function is using during the function call that submits tx frames
 *  into the txrx layer, for cases where a tx descriptor is successfully
 *  allocated, but for other reasons the frame could not be accepted.
 *
 * @param pdev - the data physical device that is sending the data
 * @param tx_desc - the descriptor being freed
 */
void ol_tx_desc_free(struct ol_txrx_pdev_t *pdev, struct ol_tx_desc_t *tx_desc);

#if defined(FEATURE_TSO)
struct qdf_tso_seg_elem_t *ol_tso_alloc_segment(struct ol_txrx_pdev_t *pdev);

void ol_tso_free_segment(struct ol_txrx_pdev_t *pdev,
	 struct qdf_tso_seg_elem_t *tso_seg);
struct qdf_tso_num_seg_elem_t *ol_tso_num_seg_alloc(
				struct ol_txrx_pdev_t *pdev);
void ol_tso_num_seg_free(struct ol_txrx_pdev_t *pdev,
	 struct qdf_tso_num_seg_elem_t *tso_num_seg);
void ol_free_remaining_tso_segs(ol_txrx_vdev_handle vdev,
				struct ol_txrx_msdu_info_t *msdu_info,
				bool is_tso_seg_mapping_done);

#else
#define ol_tso_alloc_segment(pdev) /*no-op*/
#define ol_tso_free_segment(pdev, tso_seg) /*no-op*/
#define ol_tso_num_seg_alloc(pdev) /*no-op*/
#define ol_tso_num_seg_free(pdev, tso_num_seg) /*no-op*/
/*no-op*/
#define ol_free_remaining_tso_segs(vdev, msdu_info, is_tso_seg_mapping_done)
#endif

/**
 * ol_tx_get_desc_global_pool() - get descriptor from global pool
 * @pdev: pdev handler
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: tx descriptor
 */
static inline
struct ol_tx_desc_t *ol_tx_get_desc_global_pool(struct ol_txrx_pdev_t *pdev)
{
	struct ol_tx_desc_t *tx_desc = &pdev->tx_desc.freelist->tx_desc;

	pdev->tx_desc.freelist = pdev->tx_desc.freelist->next;
	pdev->tx_desc.num_free--;
	return tx_desc;
}

/**
 * ol_tx_put_desc_global_pool() - put descriptor to global pool freelist
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: none
 */
static inline
void ol_tx_put_desc_global_pool(struct ol_txrx_pdev_t *pdev,
			struct ol_tx_desc_t *tx_desc)
{
	((union ol_tx_desc_list_elem_t *)tx_desc)->next =
					pdev->tx_desc.freelist;
	pdev->tx_desc.freelist =
			 (union ol_tx_desc_list_elem_t *)tx_desc;
	pdev->tx_desc.num_free++;
}


#ifdef QCA_LL_TX_FLOW_CONTROL_V2

#ifdef QCA_LL_TX_FLOW_CONTROL_RESIZE
int ol_tx_distribute_descs_to_deficient_pools_from_global_pool(void);
#else
static inline
int ol_tx_distribute_descs_to_deficient_pools_from_global_pool(void)
{
	return 0;
}
#endif

int ol_tx_free_invalid_flow_pool(struct ol_tx_flow_pool_t *pool);
/**
 * ol_tx_get_desc_flow_pool() - get descriptor from flow pool
 * @pool: flow pool
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: tx descriptor
 */
static inline
struct ol_tx_desc_t *ol_tx_get_desc_flow_pool(struct ol_tx_flow_pool_t *pool)
{
	struct ol_tx_desc_t *tx_desc = &pool->freelist->tx_desc;

	pool->freelist = pool->freelist->next;
	pool->avail_desc--;
	return tx_desc;
}

/**
 * ol_tx_put_desc_flow_pool() - put descriptor to flow pool freelist
 * @pool: flow pool
 * @tx_desc: tx descriptor
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: none
 */
static inline
void ol_tx_put_desc_flow_pool(struct ol_tx_flow_pool_t *pool,
			struct ol_tx_desc_t *tx_desc)
{
	tx_desc->pool = pool;
	((union ol_tx_desc_list_elem_t *)tx_desc)->next = pool->freelist;
	pool->freelist = (union ol_tx_desc_list_elem_t *)tx_desc;
	pool->avail_desc++;
}

#else
static inline int ol_tx_free_invalid_flow_pool(void *pool)
{
	return 0;
}
#endif

#ifdef DESC_DUP_DETECT_DEBUG
/**
 * ol_tx_desc_dup_detect_init() - initialize descriptor duplication logic
 * @pdev: pdev handle
 * @pool_size: global pool size
 *
 * Return: none
 */
static inline
void ol_tx_desc_dup_detect_init(struct ol_txrx_pdev_t *pdev, uint16_t pool_size)
{
	uint16_t size = (pool_size >> DIV_BY_8) +
		sizeof(*pdev->tx_desc.free_list_bitmap);
	pdev->tx_desc.free_list_bitmap = qdf_mem_malloc(size);
}

/**
 * ol_tx_desc_dup_detect_deinit() - deinit descriptor duplication logic
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void ol_tx_desc_dup_detect_deinit(struct ol_txrx_pdev_t *pdev)
{
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
		  "%s: pool_size %d num_free %d\n", __func__,
		pdev->tx_desc.pool_size, pdev->tx_desc.num_free);
	if (pdev->tx_desc.free_list_bitmap)
		qdf_mem_free(pdev->tx_desc.free_list_bitmap);
}

/**
 * ol_tx_desc_dup_detect_set() - set bit for msdu_id
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Return: none
 */
static inline
void ol_tx_desc_dup_detect_set(struct ol_txrx_pdev_t *pdev,
				struct ol_tx_desc_t *tx_desc)
{
	uint16_t msdu_id = ol_tx_desc_id(pdev, tx_desc);
	bool test;

	if (!pdev->tx_desc.free_list_bitmap)
		return;

	if (qdf_unlikely(msdu_id > pdev->tx_desc.pool_size)) {
		qdf_print("msdu_id %d > pool_size %d",
			  msdu_id, pdev->tx_desc.pool_size);
		QDF_BUG(0);
	}

	test = test_and_set_bit(msdu_id, pdev->tx_desc.free_list_bitmap);
	if (qdf_unlikely(test)) {
		uint16_t size = (pdev->tx_desc.pool_size >> DIV_BY_8) +
			((pdev->tx_desc.pool_size & MOD_BY_8) ? 1 : 0);
		qdf_print("duplicate msdu_id %d detected!!", msdu_id);
		qdf_trace_hex_dump(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
		(void *)pdev->tx_desc.free_list_bitmap, size);
		QDF_BUG(0);
	}
}

/**
 * ol_tx_desc_dup_detect_reset() - reset bit for msdu_id
 * @pdev: pdev handle
 * @tx_desc: tx descriptor
 *
 * Return: none
 */
static inline
void ol_tx_desc_dup_detect_reset(struct ol_txrx_pdev_t *pdev,
				 struct ol_tx_desc_t *tx_desc)
{
	uint16_t msdu_id = ol_tx_desc_id(pdev, tx_desc);
	bool test;

	if (!pdev->tx_desc.free_list_bitmap)
		return;

	if (qdf_unlikely(msdu_id > pdev->tx_desc.pool_size)) {
		qdf_print("msdu_id %d > pool_size %d",
			  msdu_id, pdev->tx_desc.pool_size);
		QDF_BUG(0);
	}

	test = !test_and_clear_bit(msdu_id, pdev->tx_desc.free_list_bitmap);
	if (qdf_unlikely(test)) {
		uint16_t size = (pdev->tx_desc.pool_size >> DIV_BY_8) +
			((pdev->tx_desc.pool_size & MOD_BY_8) ? 1 : 0);
		qdf_print("duplicate free msg received for msdu_id %d!!\n",
								 msdu_id);
		qdf_trace_hex_dump(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
		(void *)pdev->tx_desc.free_list_bitmap, size);
		QDF_BUG(0);
	}
}
#else
static inline
void ol_tx_desc_dup_detect_init(struct ol_txrx_pdev_t *pdev, uint16_t size)
{
}

static inline
void ol_tx_desc_dup_detect_deinit(struct ol_txrx_pdev_t *pdev)
{
}

static inline
void ol_tx_desc_dup_detect_set(struct ol_txrx_pdev_t *pdev,
				struct ol_tx_desc_t *tx_desc)
{
}

static inline
void ol_tx_desc_dup_detect_reset(struct ol_txrx_pdev_t *pdev,
				 struct ol_tx_desc_t *tx_desc)
{
}
#endif

enum extension_header_type
ol_tx_get_ext_header_type(struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t netbuf);
enum extension_header_type
ol_tx_get_wisa_ext_type(qdf_nbuf_t netbuf);


#endif /* _OL_TX_DESC__H_ */
