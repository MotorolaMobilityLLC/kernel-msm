/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * @file ol_tx_queue.h
 * @brief API definitions for the tx frame queue module within the data SW.
 */
#ifndef _OL_TX_QUEUE__H_
#define _OL_TX_QUEUE__H_

#include <adf_nbuf.h>      /* adf_nbuf_t */
#include <ol_txrx_types.h> /* ol_txrx_vdev_t, etc. */
#include <adf_os_types.h>  /* a_bool_t */

#if defined(CONFIG_HL_SUPPORT)

/**
 * @brief Queue a tx frame to the tid queue.
 *
 * @param pdev - the data virtual device sending the data
 *      (for storing the tx desc in the virtual dev's tx_target_list,
 *      and for accessing the phy dev)
 * @param txq - which queue the tx frame gets stored in
 * @param tx_desc - tx meta-data, including prev and next ptrs
 * @param tx_msdu_info - characteristics of the tx frame
 */
void
ol_tx_enqueue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    struct ol_tx_desc_t *tx_desc,
    struct ol_txrx_msdu_info_t *tx_msdu_info);

/**
 * @brief - remove the specified number of frames from the head of a tx queue
 * @details
 *  This function removes frames from the head of a tx queue,
 *  and returns them as a NULL-terminated linked list.
 *  The function will remove frames until one of the following happens:
 *  1.  The tx queue is empty
 *  2.  The specified number of frames have been removed
 *  3.  Removal of more frames would exceed the specified credit limit
 *
 * @param pdev - the physical device object
 * @param txq - which tx queue to remove frames from
 * @param head - which contains return linked-list of tx frames (descriptors)
 * @param num_frames - maximum number of frames to remove
 * @param[in/out] credit -
 *     input:  max credit the dequeued frames can consume
 *     output: how much credit the dequeued frames consume
 * @param[out] bytes - the sum of the sizes of the dequeued frames
 * @return number of frames dequeued
*/
u_int16_t
ol_tx_dequeue(
	struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	ol_tx_desc_list *head,
	u_int16_t num_frames,
	u_int32_t *credit,
	int *bytes);

/**
 * @brief - free all of frames from the tx queue while deletion
 * @details
 *  This function frees all of frames from the tx queue.
 *  This function is called during peer or vdev deletion.
 *  This function notifies the scheduler, so the scheduler can update
 *  its state to account for the absence of the queue.
 *
 * @param pdev - the physical device object, which stores the txqs
 * @param txq - which tx queue to free frames from
 * @param tid - the extended TID that the queue belongs to
 */
void
ol_tx_queue_free(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int tid);

/**
 * @brief - discard pending tx frames from the tx queue
 * @details
 *  This function is called if there are too many queues in tx scheduler.
 *  This function is called if we wants to flush all pending tx
 *  queues in tx scheduler.
 *
 * @param pdev - the physical device object, which stores the txqs
 * @param flush_all - flush all pending tx queues if set to true
 * @param tx_descs - List Of tx_descs to be discarded will be returned by this function
 */

void
ol_tx_queue_discard(
    struct ol_txrx_pdev_t *pdev,
    a_bool_t flush_all,
    ol_tx_desc_list *tx_descs);

#else

#define ol_tx_enqueue(pdev, txq, tx_desc, tx_msdu_info) /* no-op */
#define ol_tx_dequeue(pdev, ext_tid, txq, head, num_frames, credit, bytes) 0
#define ol_tx_queue_free(pdev, txq, tid) /* no-op */
#define ol_tx_queue_discard(pdev, flush, tx_descs) /* no-op */

#endif /* defined(CONFIG_HL_SUPPORT) */

#if defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL)

void
ol_txrx_peer_bal_add_limit_peer(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id,
    u_int16_t peer_limit);

void
ol_txrx_peer_bal_remove_limit_peer(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id);

void
ol_txrx_peer_pause_but_no_mgmt_q(ol_txrx_peer_handle peer);

void
ol_txrx_peer_unpause_but_no_mgmt_q(ol_txrx_peer_handle peer);

u_int16_t
ol_tx_bad_peer_dequeue_check(struct ol_tx_frms_queue_t *txq,
	u_int16_t max_frames,
	u_int16_t *tx_limit_flag);

void
ol_tx_bad_peer_update_tx_limit(struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	u_int16_t frames,
	u_int16_t tx_limit_flag);

void
ol_txrx_set_txq_peer(
	struct ol_tx_frms_queue_t *txq,
	struct ol_txrx_peer_t *peer);

/**
 * @brief - initialize the peer balance context
 * @param pdev - the physical device object, which stores the txqs
 */
void ol_tx_badpeer_flow_cl_init(struct ol_txrx_pdev_t *pdev);

/**
 * @brief - deinitialize the peer balance context
 * @param pdev - the physical device object, which stores the txqs
 */
void ol_tx_badpeer_flow_cl_deinit(struct ol_txrx_pdev_t *pdev);

#else

static inline void ol_txrx_peer_bal_add_limit_peer(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id,
    u_int16_t peer_limit)
{
    /* no-op */
}

static inline void ol_txrx_peer_bal_remove_limit_peer(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id)
{
    /* no-op */
}

static inline void ol_txrx_peer_pause_but_no_mgmt_q(ol_txrx_peer_handle peer)
{
    /* no-op */
}

static inline void ol_txrx_peer_unpause_but_no_mgmt_q(ol_txrx_peer_handle peer)
{
    /* no-op */
}

static inline u_int16_t
ol_tx_bad_peer_dequeue_check(struct ol_tx_frms_queue_t *txq,
	u_int16_t max_frames,
	u_int16_t *tx_limit_flag)
{
    /* just return max_frames */
    return max_frames;
}

static inline void
ol_tx_bad_peer_update_tx_limit(struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	u_int16_t frames,
	u_int16_t tx_limit_flag)
{
    /* no-op */
}

static inline void
ol_txrx_set_txq_peer(
	struct ol_tx_frms_queue_t *txq,
	struct ol_txrx_peer_t *peer)
{
    /* no-op */
}

static inline void ol_tx_badpeer_flow_cl_init(struct ol_txrx_pdev_t *pdev)
{
    /* no-op */
}

static inline void ol_tx_badpeer_flow_cl_deinit(struct ol_txrx_pdev_t *pdev)
{
    /* no-op */
}

#endif /* defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL) */


#if defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING)

void
ol_tx_queue_log_sched(
    struct ol_txrx_pdev_t *pdev,
    int credit,
    int *num_active_tids,
    u_int32_t **active_bitmap,
    u_int8_t **data);

#define OL_TX_QUEUE_LOG_SCHED ol_tx_queue_log_sched

#else

#define OL_TX_QUEUE_LOG_SCHED(\
    pdev, credit, num_active_tids, active_bitmap, data)

#endif /* defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING) */

#if defined(CONFIG_HL_SUPPORT) && TXRX_DEBUG_LEVEL > 5
/**
 * @brief - show current state of all tx queues
 * @param pdev - the physical device object, which stores the txqs
 */
void
ol_tx_queues_display(struct ol_txrx_pdev_t *pdev);
#else
#define ol_tx_queues_display(pdev) /* no-op */
#endif

#define ol_tx_queue_decs_reinit(peer, peer_id) /* no-op */

#ifdef QCA_SUPPORT_TX_THROTTLE
/**
 * @brief - initialize the throttle context
 * @param pdev - the physical device object, which stores the txqs
 */
void ol_tx_throttle_init(struct ol_txrx_pdev_t *pdev);
#else
#define ol_tx_throttle_init(pdev) /*no op*/
#endif

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
#define OL_TX_IS_TXQ_LAST_SERVICED_QUEUE(pdev, txq) \
    txq == pdev->tx_sched.last_used_txq

u_int32_t ol_tx_txq_group_credit_limit(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    u_int32_t credit);

void ol_tx_txq_group_credit_update(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int32_t credit,
    u_int8_t absolute);

void
ol_tx_set_vdev_group_ptr(
    ol_txrx_pdev_handle pdev,
    u_int8_t vdev_id,
    struct ol_tx_queue_group_t *grp_ptr);

void
ol_tx_txq_set_group_ptr(
    struct ol_tx_frms_queue_t *txq,
    struct ol_tx_queue_group_t *grp_ptr);

void
ol_tx_set_peer_group_ptr(
    ol_txrx_pdev_handle pdev,
    struct ol_txrx_peer_t *peer,
    u_int8_t vdev_id,
    u_int8_t tid);

#define OL_TX_TXQ_GROUP_CREDIT_LIMIT ol_tx_txq_group_credit_limit
#define OL_TX_TXQ_GROUP_CREDIT_UPDATE ol_tx_txq_group_credit_update
#define OL_TX_TXQ_SET_GROUP_PTR ol_tx_txq_set_group_ptr
#define OL_TX_SET_PEER_GROUP_PTR ol_tx_set_peer_group_ptr
#else
#define OL_TX_IS_TXQ_LAST_SERVICED_QUEUE(pdev, txq) 0
#define OL_TX_TXQ_GROUP_CREDIT_LIMIT(pdev, txq, credit) credit
#define OL_TX_TXQ_GROUP_CREDIT_UPDATE(pdev, txq, credit, absolute) /* no-op */
#define OL_TX_TXQ_SET_GROUP_PTR(txq,grp_ptr) /* no-op */
#define OL_TX_SET_PEER_GROUP_PTR(pdev, peer, vdev_id, tid) /* no-op */
#endif

#endif /* _OL_TX_QUEUE__H_ */
