/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_htt_api.h
 * @brief Define the host data API functions called by the host HTT SW.
 */
#ifndef _OL_TXRX_HTT_API__H_
#define _OL_TXRX_HTT_API__H_

#include <htt.h>         /* HTT_TX_COMPL_IND_STAT */
#include <athdefs.h>     /* A_STATUS */
#include <adf_nbuf.h>    /* adf_nbuf_t */

#include <ol_txrx_api.h> /* ol_txrx_pdev_handle */

#ifdef CONFIG_HL_SUPPORT
static inline u_int16_t *
ol_tx_msdu_id_storage(adf_nbuf_t msdu)
{
    return NBUF_CB_ID(msdu);
}
#else
static inline u_int16_t *
ol_tx_msdu_id_storage(adf_nbuf_t msdu)
{
    adf_os_assert(adf_nbuf_headroom(msdu) >= (sizeof(u_int16_t) * 2 - 1));
    return (u_int16_t *) (((adf_os_size_t) (adf_nbuf_head(msdu) + 1)) & ~0x1);
}
#endif

/**
 * @brief Tx MSDU download completion for a LL system
 * @details
 *  Release the reference to the downloaded tx descriptor.
 *  In the unlikely event that the reference count is zero, free
 *  the tx descriptor and tx frame.
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_ll(
    void *pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id);

/**
 * @brief Tx MSDU download completion for HL system without tx completion msgs
 * @details
 *  Free the tx descriptor and tx frame.
 *  Invoke the HL tx download scheduler.
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_hl_free(
    void *pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id);

/**
 * @brief Tx MSDU download completion for HL system with tx completion msgs
 * @details
 *  Release the reference to the downloaded tx descriptor.
 *  In the unlikely event that the reference count is zero, free
 *  the tx descriptor and tx frame.
 *  Optionally, invoke the HL tx download scheduler.  (It is probable that
 *  the HL tx download scheduler would operate in response to tx completion
 *  messages rather than download completion events.)
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_hl_retain(
    void *pdev,
    A_STATUS status,
    adf_nbuf_t msdu,
    u_int16_t msdu_id);

/*
 * For now, make the host HTT -> host txrx tx completion status
 * match the target HTT -> host HTT tx completion status, so no
 * translation is needed.
 */
/*
 * host-only statuses use a different part of the number space
 * than host-target statuses
 */
#define HTT_HOST_ONLY_STATUS_CODE_START 128
enum htt_tx_status {
    /* ok - successfully sent + acked */
    htt_tx_status_ok = HTT_TX_COMPL_IND_STAT_OK,

    /* discard - not sent (congestion control) */
    htt_tx_status_discard = HTT_TX_COMPL_IND_STAT_DISCARD,

    /* no_ack - sent, but no ack */
    htt_tx_status_no_ack = HTT_TX_COMPL_IND_STAT_NO_ACK,

    /* download_fail - the host could not deliver the tx frame to the target */
    htt_tx_status_download_fail = HTT_HOST_ONLY_STATUS_CODE_START,

    /* peer_del - tx completion for alreay deleted peer used for HL case */
    htt_tx_status_peer_del = HTT_TX_COMPL_IND_STAT_PEER_DEL,
};

/**
 * @brief Process a tx completion message sent by the target.
 * @details
 *  When the target is done transmitting a tx frame (either because
 *  the frame was sent + acknowledged, or because the target gave up)
 *  it sends a tx completion message to the host.
 *  This notification function is used regardless of whether the
 *  transmission succeeded or not; the status argument indicates whether
 *  the transmission succeeded.
 *  This tx completion message indicates via the descriptor ID which
 *  tx frames were completed, and indicates via the status whether the
 *  frames were transmitted successfully.
 *  The host frees the completed descriptors / frames (updating stats
 *  in the process).
 *
 * @param pdev - the data physical device that sent the tx frames
 *      (registered with HTT as a context pointer during attach time)
 * @param num_msdus - how many MSDUs are referenced by the tx completion
 *      message
 * @param status - whether transmission was successful
 * @param tx_msdu_id_iterator - abstract method of finding the IDs for the
 *      individual MSDUs referenced by the tx completion message, via the
 *      htt_tx_compl_desc_id API function
 */
void
ol_tx_completion_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    enum htt_tx_status status,
    void *tx_msdu_id_iterator);

void
ol_tx_credit_completion_handler(ol_txrx_pdev_handle pdev, int credits);



struct rate_report_t{
    u_int16_t id;
    u_int16_t phy : 4;
    u_int32_t rate;
};

#if defined(CONFIG_HL_SUPPORT) && defined(QCA_BAD_PEER_TX_FLOW_CL)
/**
 * @brief Process a link status report for all peers.
 * @details
 *  The ol_txrx_peer_link_status_handler function performs basic peer link
 *  status analysis
 *
 *  According to the design, there are 3 kinds of peers which will be
 *  treated differently:
 *  1) normal: not do any flow control for the peer
 *  2) limited: will apply flow control for the peer, but frames are allowed to send
 *  3) paused: will apply flow control for the peer, no frame is allowed to send
 *
 * @param pdev - the data physical device that sent the tx frames
 * @param status - the number of peers need to be handled
 * @param peer_link_report - the link status dedail message
 */
void
ol_txrx_peer_link_status_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_num,
    struct rate_report_t* peer_link_status);


#else
static inline void ol_txrx_peer_link_status_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_num,
    struct rate_report_t* peer_link_status)
{
    /* no-op */
}
#endif


#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
void
ol_txrx_update_tx_queue_groups(
    ol_txrx_pdev_handle pdev,
    u_int8_t group_id,
    int32_t credit,
    u_int8_t absolute,
    u_int32_t vdev_id_mask,
    u_int32_t ac_mask
);

void
ol_tx_desc_update_group_credit(
    ol_txrx_pdev_handle pdev,
    u_int16_t tx_desc_id,
    int credit, u_int8_t absolute, enum htt_tx_status status);
#define OL_TX_DESC_UPDATE_GROUP_CREDIT ol_tx_desc_update_group_credit

#ifdef DEBUG_HL_LOGGING
void
ol_tx_update_group_credit_stats(ol_txrx_pdev_handle pdev);

void
ol_tx_dump_group_credit_stats(ol_txrx_pdev_handle pdev);

void
ol_tx_clear_group_credit_stats(ol_txrx_pdev_handle pdev);

#define OL_TX_UPDATE_GROUP_CREDIT_STATS ol_tx_update_group_credit_stats
#define OL_TX_DUMP_GROUP_CREDIT_STATS ol_tx_dump_group_credit_stats
#define OL_TX_CLEAR_GROUP_CREDIT_STATS ol_tx_clear_group_credit_stats
#else
#define OL_TX_UPDATE_GROUP_CREDIT_STATS(pdev) /* no -op*/
#define OL_TX_DUMP_GROUP_CREDIT_STATS(pdev) /* no -op*/
#define OL_TX_CLEAR_GROUP_CREDIT_STATS(pdev) /* no -op*/
#endif

#else
#define OL_TX_DESC_UPDATE_GROUP_CREDIT(pdev, tx_desc_id, credit, absolute, status) /* no-op */
#endif

/**
 * @brief Init the total amount of target credit.
 * @details
 *
 * @param pdev - the data physical device that sent the tx frames
 * @param credit_delta - how much to increment the target's tx credit by
 */
void
ol_tx_target_credit_init(struct ol_txrx_pdev_t *pdev, int credit_delta);

/**
 * @brief Process a tx completion message for a single MSDU.
 * @details
 *  The ol_tx_single_completion_handler function performs the same tx
 *  completion processing as the ol_tx_completion_handler, but for a
 *  single frame.
 *  ol_tx_completion_handler is optimized to handle batch completions
 *  as efficiently as possible; in contrast ol_tx_single_completion_handler
 *  handles single frames as simply and generally as possible.
 *  Thus, this ol_tx_single_completion_handler function is suitable for
 *  intermittent usage, such as for tx mgmt frames.
 *
 * @param pdev - the data physical device that sent the tx frames
 * @param status - whether transmission was successful
 * @param tx_msdu_id - ID of the frame which completed transmission
 */
void
ol_tx_single_completion_handler(
    ol_txrx_pdev_handle pdev,
    enum htt_tx_status status,
    u_int16_t tx_desc_id);

/**
 * @brief Update the amount of target credit.
 * @details
 *  When the target finishes with an old transmit frame, it can use the
 *  space that was occupied by the old tx frame to store a new tx frame.
 *  This function is used to inform the txrx layer, where the HL tx download
 *  scheduler resides, about such updates to the target's tx credit.
 *  This credit update is done explicitly, rather than having the txrx layer
 *  update the credit count itself inside the ol_tx_completion handler
 *  function.  This provides HTT with the flexibility to limit the rate of
 *  downloads from the TXRX layer's download scheduler, by controlling how
 *  much credit the download scheduler gets, and also provides the flexibility
 *  to account for a change in the tx memory pool size within the target.
 *  This function is only used for HL systems; in LL systems, each tx frame
 *  is assumed to use exactly one credit (for its target-side tx descriptor),
 *  and any rate limiting is managed within the target.
 *
 * @param pdev - the data physical device that sent the tx frames
 * @param credit_delta - how much to increment the target's tx credit by
 */
void
ol_tx_target_credit_update(struct ol_txrx_pdev_t *pdev, int credit_delta);


/**
 * @brief Process an rx indication message sent by the target.
 * @details
 *  The target sends a rx indication message to the host as a
 *  notification that there are new rx frames available for the
 *  host to process.
 *  The HTT host layer locates the rx descriptors and rx frames
 *  associated with the indication, and calls this function to
 *  invoke the rx data processing on the new frames.
 *  (For LL, the rx descriptors and frames are delivered directly
 *  to the host via MAC DMA, while for HL the rx descriptor and
 *  frame for individual frames are combined with the rx indication
 *  message.)
 *  All MPDUs referenced by a rx indication message belong to the
 *  same peer-TID.
 *
 * @param pdev - the data physical device that received the frames
 *      (registered with HTT as a context pointer during attach time)
 * @param rx_ind_msg - the network buffer holding the rx indication message
 *      (For HL, this netbuf also holds the rx desc and rx payload, but
 *      the data SW is agnostic to whether the desc and payload are
 *      piggybacked with the rx indication message.)
 * @param peer_id - which peer sent this rx data
 * @param tid - what (extended) traffic type the rx data is
 * @param num_mpdu_ranges - how many ranges of MPDUs does the message describe.
 *      Each MPDU within the range has the same rx status.
 */
void
ol_rx_indication_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid,
    int num_mpdu_ranges);

/**
 * @brief Process an rx fragment indication message sent by the target.
 * @details
 *  The target sends a rx fragment indication message to the host as a
 *  notification that there are new rx fragment available for the
 *  host to process.
 *  The HTT host layer locates the rx descriptors and rx fragment
 *  associated with the indication, and calls this function to
 *  invoke the rx fragment data processing on the new fragment.
 *
 * @param pdev - the data physical device that received the frames
 *      (registered with HTT as a context pointer during attach time)
 * @param rx_frag_ind_msg - the network buffer holding the rx fragment indication message
 * @param peer_id - which peer sent this rx data
 * @param tid - what (extended) traffic type the rx data is
 */
void ol_rx_frag_indication_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t rx_frag_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid);

/**
 * @brief Process rx offload deliver indication message sent by the target.
 * @details
 *  When the target exits offload mode, target delivers packets that it has held in its
 *  memory to the host using this message.
 *  Low latency case:
 *  The message contains the number of MSDUs that are being delivered by the target to the
 *  host. The packet itself resides in host ring along with some metadata describing the peer
 *  id, vdev id, tid, FW desc and length of the packet being delivered.
 *  Hight letency case:
 *  The message itself contains the payload of the MSDU being delivered by the target to the
 *  host. The message also contains meta data describing the packet such as peer id, vdev id,
 *  tid, FW desc and length of the packet being delivered. Refer to htt.h for the exact structure
 *  of the message.
 *  @param pdev - the data physical device that received the frame.
 *  @param msg - offload deliver indication message
 *  @param msdu_cnt - number of MSDUs being delivred.
 */
void
ol_rx_offload_deliver_ind_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t msg,
    int msdu_cnt);

/**
 * @brief Process a peer map message sent by the target.
 * @details
 *  Each time the target allocates a new peer ID, it will inform the
 *  host via the "peer map" message.  This function processes that
 *  message.  The host data SW looks for a peer object whose MAC address
 *  matches the MAC address specified in the peer map message, and then
 *  sets up a mapping between the peer ID specified in the message and
 *  the peer object that was found.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - ID generated by the target to refer to the peer in question
 *      The target may create multiple IDs for a single peer.
 * @param vdev_id - Reference to the virtual device the peer is associated with
 * @param peer_mac_addr - MAC address of the peer in question
 * @param tx_ready - whether transmits to this peer can be done already, or
 *      need to wait for a call to peer_tx_ready (only applies to HL systems)
 */
void
ol_rx_peer_map_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tx_ready);

/**
 * @brief Notify the host that the target is ready to transmit to a new peer.
 * @details
 *  Some targets can immediately accept tx frames for a new peer, as soon as
 *  the peer's association completes.  Other target need a short setup time
 *  before they are ready to accept tx frames for the new peer.
 *  If the target needs time for setup, it will provide a peer_tx_ready
 *  message when it is done with the setup.  This function forwards this
 *  notification from the target to the host's tx queue manager.
 *  This function only applies for HL systems, in which the host determines
 *  which peer a given tx frame is for, and stores the tx frames in queues.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - ID for the new peer which can now accept tx frames
 */
void
ol_txrx_peer_tx_ready_handler(ol_txrx_pdev_handle pdev, u_int16_t peer_id);

/**
 * @brief Process a peer unmap message sent by the target.
 * @details
 *  Each time the target frees a peer ID, it will inform the host via the
 *  "peer unmap" message.  This function processes that message.
 *  The host data SW uses the peer ID from the message to find the peer
 *  object from peer_map[peer_id], then invalidates peer_map[peer_id]
 *  (by setting it to NULL), and checks whether there are any remaining
 *  references to the peer object.  If not, the function deletes the
 *  peer object.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - ID that is being freed.
 *      The target may create multiple IDs for a single peer.
 */
void
ol_rx_peer_unmap_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id);

/**
 * @brief Process a security indication message sent by the target.
 * @details
 *  When a key is assigned to a peer, the target will inform the host
 *  with a security indication message.
 *  The host remembers the security type, and infers whether a rx PN
 *  check is needed.
 *
 * @param pdev - data physical device handle
 * @param peer_id - which peer the security info is for
 * @param sec_type - which type of security / key the peer is using
 * @param is_unicast - whether security spec is for a unicast or multicast key
 * @param michael_key - key used for TKIP MIC (if sec_type == TKIP)
 * @param rx_pn - RSC used for WAPI PN replay check (if sec_type == WAPI)
 */
void
ol_rx_sec_ind_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    enum htt_sec_type sec_type,
    int is_unicast,
    u_int32_t *michael_key,
    u_int32_t *rx_pn);

/**
 * @brief Process an ADDBA message sent by the target.
 * @details
 *  When the target notifies the host of an ADDBA event for a specified
 *  peer-TID, the host will set up the rx reordering state for the peer-TID.
 *  Specifically, the host will create a rx reordering array whose length
 *  is based on the window size specified in the ADDBA.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer the ADDBA event is for
 * @param tid - which traffic ID within the peer the ADDBA event is for
 * @param win_sz - how many sequence numbers are in the ARQ block ack window
 *      set up by the ADDBA event
 * @param start_seq_num - the initial value of the sequence number during the
 *      block ack agreement, as specified by the ADDBA request.
 * @param failed - indicate whether the target's ADDBA setup succeeded:
 *      0 -> success, 1 -> fail
 */
void
ol_rx_addba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t win_sz,
    u_int16_t start_seq_num,
    u_int8_t failed);

/**
 * @brief Process a DELBA message sent by the target.
 * @details
 *  When the target notifies the host of a DELBA event for a specified
 *  peer-TID, the host will clean up the rx reordering state for the peer-TID.
 *  Specifically, the host will remove the rx reordering array, and will
 *  set the reorder window size to be 1 (stop and go ARQ).
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer the ADDBA event is for
 * @param tid - which traffic ID within the peer the ADDBA event is for
 */
void
ol_rx_delba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid);

enum htt_rx_flush_action {
    htt_rx_flush_release,
    htt_rx_flush_discard,
};

/**
 * @brief Process a rx reorder flush message sent by the target.
 * @details
 *  The target's rx reorder logic can send a flush indication to the
 *  host's rx reorder buffering either as a flush IE within a rx
 *  indication message, or as a standalone rx reorder flush message.
 *  This ol_rx_flush_handler function processes the standalone rx
 *  reorder flush message from the target.
 *  The flush message specifies a range of sequence numbers whose
 *  rx frames are flushed.
 *  Some sequence numbers within the specified range may not have
 *  rx frames; the host needs to check for each sequence number in
 *  the specified range whether there are rx frames held for that
 *  sequence number.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer's rx data is being flushed
 * @param tid - which traffic ID within the peer has the rx data being flushed
 * @param seq_num_start - Which sequence number within the rx reordering
 *      buffer the flushing should start with.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      The flush includes this initial sequence number.
 * @param seq_num_end - Which sequence number within the rx reordering
 *      buffer the flushing should stop at.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      The flush excludes this final sequence number.
 * @param action - whether to release or discard the rx frames
 */
void
ol_rx_flush_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int16_t seq_num_start,
    u_int16_t seq_num_end,
    enum htt_rx_flush_action action);

/**
 * @brief Process a rx pn indication message
 * @details
 *      When the peer is configured to get PN checking done in target,
 *      the target instead of sending reorder flush/release messages
 *      sends PN indication messages which contain the start and end
 *      sequence numbers to be flushed/released along with the sequence
 *      numbers of MPDUs that failed the PN check in target.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer's rx data is being flushed
 * @param tid - which traffic ID within the peer
 * @param seq_num_start - Which sequence number within the rx reordering
 *      buffer to start with.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      This is the initial sequence number.
 * @param seq_num_end - Which sequence number within the rx reordering
 *      buffer to stop at.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      The processing stops right before this sequence number
 * @param pn_ie_cnt - Indicates the number of PN information elements.
 * @param pn_ie - Pointer to the array of PN information elements. Each
 *      PN information element contains the LSBs of the 802.11 sequence number
 *      of the MPDU that failed the PN checking in target.
 */
void
ol_rx_pn_ind_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    int seq_num_start,
    int seq_num_end,
    u_int8_t pn_ie_cnt,
    u_int8_t *pn_ie);

/**
 * @brief Process a stats message sent by the target.
 * @details
 *  The host can request target for stats.
 *  The target sends the stats to the host via a confirmation message.
 *  This ol_txrx_fw_stats_handler function processes the confirmation message.
 *  Currently, this processing consists of copying the stats from the message
 *  buffer into the txrx pdev object, and waking the sleeping host context
 *  that requested the stats.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param cookie - Value echoed from the cookie in the stats request
 *      message.  This allows the host SW to find the stats request object.
 *      (Currently, this cookie is unused.)
 * @param stats_info_list - stats confirmation message contents, containing
 *      a list of the stats requested from the target
 */
void
ol_txrx_fw_stats_handler(
    ol_txrx_pdev_handle pdev,
    u_int64_t cookie,
    u_int8_t *stats_info_list);

/**
 * @brief Process a tx inspect message sent by the target.
 * @details:
 *  TODO: update
 *  This tx inspect message indicates via the descriptor ID
 *  which tx frames are to be inspected by host. The host
 *  re-injects the packet back to the host for a number of
 *  cases.
 *
 * @param pdev - the data physical device that sent the tx frames
 *      (registered with HTT as a context pointer during attach time)
 * @param num_msdus - how many MSDUs are referenced by the tx completion
 *      message
 * @param tx_msdu_id_iterator - abstract method of finding the IDs for the
 *      individual MSDUs referenced by the tx completion message, via the
 *      htt_tx_compl_desc_id API function
 */
void
ol_tx_inspect_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    void *tx_desc_id_iterator);

/**
 * @brief Get the UAPSD mask.
 * @details
 *  This function will return the UAPSD TID mask.
 *
 * @param txrx_pdev - pointer to the txrx pdev object
 * @param peer_id - PeerID.
 * @return uapsd mask value
 */
u_int8_t
ol_txrx_peer_uapsdmask_get(struct ol_txrx_pdev_t * txrx_pdev,
    u_int16_t peer_id);

/**
 * @brief Get the Qos Capable.
 * @details
 *  This function will return the txrx_peer qos_capable.
 *
 * @param txrx_pdev - pointer to the txrx pdev object
 * @param peer_id - PeerID.
 * @return qos_capable value
 */
u_int8_t
ol_txrx_peer_qoscapable_get(struct ol_txrx_pdev_t * txrx_pdev,
    u_int16_t peer_id);

/**
 * @brief Process an rx indication message sent by the target.
 * @details
 *  The target sends a rx indication message to the host as a
 *  notification that there are new rx frames available for the
 *  host to process.
 *  The HTT host layer locates the rx descriptors and rx frames
 *  associated with the indication, and calls this function to
 *  invoke the rx data processing on the new frames.
 *  All MPDUs referenced by a rx indication message belong to the
 *  same peer-TID. The frames indicated have been re-ordered by
 *  the target.
 *
 * @param pdev - the data physical device that received the frames
 *      (registered with HTT as a context pointer during attach time)
 * @param rx_ind_msg - the network buffer holding the rx indication message
 * @param peer_id - which peer sent this rx data
 * @param tid - what (extended) traffic type the rx data is
 * @param is_offload - is this an offload indication?
 */
void
ol_rx_in_order_indication_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t is_offload );

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
u_int32_t ol_tx_get_max_tx_groups_supported(struct ol_txrx_pdev_t *pdev);
#define OL_TX_GET_MAX_GROUPS ol_tx_get_max_tx_groups_supported
#else
#define OL_TX_GET_MAX_GROUPS(pdev) 0
#endif

#endif /* _OL_TXRX_HTT_API__H_ */
