/*
 * Copyright (c) 2012, 2014 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_osif_api.h
 * @brief Define the host data API functions called by the host OS shim SW.
 */
#ifndef _OL_TXRX_OSIF_API__H_
#define _OL_TXRX_OSIF_API__H_

#include <adf_nbuf.h>     /* adf_nbuf_t */

#include <ol_osif_api.h> /* ol_osif_vdev_handle */
#include <ol_txrx_api.h> /* ol_txrx_pdev_handle, etc. */

/**
 * @typedef ol_txrx_tx_fp
 * @brief top-level transmit function
 */
typedef adf_nbuf_t (*ol_txrx_tx_fp)(
    ol_txrx_vdev_handle data_vdev, adf_nbuf_t msdu_list);

/**
 * @typedef ol_txrx_tx_non_std_fp
 * @brief top-level transmit function for non-standard tx frames
 * @details
 *  This function pointer provides an alternative to the ol_txrx_tx_fp
 *  to support non-standard transmits.  In particular, this function
 *  supports transmission of:
 *  1. "Raw" frames
 *     These raw frames already have an 802.11 header; the usual
 *     802.11 header encapsulation by the driver does not apply.
 *  2. TSO segments
 *     During tx completion, the txrx layer needs to reclaim the buffer
 *     that holds the ethernet/IP/TCP header created for the TSO segment.
 *     Thus, these tx frames need to be marked as TSO, to show that they
 *     need this special handling during tx completion.
 *
 * @param data_vdev - which virtual device should transmit the frame
 * @param tx_spec - what non-standard operations to apply to the tx frame
 * @param msdu_list - tx frame(s), in a null-terminated list
 */
typedef adf_nbuf_t (*ol_txrx_tx_non_std_fp)(
    ol_txrx_vdev_handle data_vdev,
    enum ol_tx_spec tx_spec,
    adf_nbuf_t msdu_list);

struct txrx_rx_metainfo;

#ifdef OSIF_NEED_RX_PEER_ID
/**
 * @typedef ol_txrx_rx_fp
 * @brief receive function to hand batches of data frames from txrx to OS shim
 * @details this version of rx callback is for the OS shim, for example CLD, which
 * depends on peer id information in data rx.
 */
typedef void (*ol_txrx_rx_fp)(void *osif_dev, u_int16_t peer_id,
			      adf_nbuf_t msdus);
#else

/**
 * @typedef ol_txrx_rx_fp
 * @brief receive function to hand batches of data frames from txrx to OS shim
 */
typedef void (*ol_txrx_rx_fp)(void *osif_dev, adf_nbuf_t msdus);
#endif /* OSIF_NEED_RX_PEER_ID */

/**
 * @typedef ol_txrx_tx_fc_fp
 * @brief tx flow control notification function from txrx to OS shim
 * @param osif_dev - the virtual device's OS shim object
 * @param vdev_id - virtual device id
 * @param tx_resume - tx os q should be resumed or not
 */
typedef void (*ol_txrx_tx_flow_control_fp)(void *osif_dev,
                                           u_int8_t vdev_id, a_bool_t tx_resume);

/**
 * @typedef ol_txrx_rx_fp
 * @brief receive function to hand batches of data frames from txrx to OS shim
 */

struct ol_txrx_osif_ops {
    /* tx function pointers - specified by txrx, stored by OS shim */
    struct {
        ol_txrx_tx_fp              std;
        ol_txrx_tx_non_std_fp      non_std;
        ol_txrx_tx_flow_control_fp flow_control_cb;
    } tx;

    /* rx function pointers - specified by OS shim, stored by txrx */
    struct {
        ol_txrx_rx_fp         std;
    } rx;
};

/**
 * @brief Link a vdev's data object with the matching OS shim vdev object.
 * @details
 *  The data object for a virtual device is created by the function
 *  ol_txrx_vdev_attach.  However, rather than fully linking the
 *  data vdev object with the vdev objects from the other subsystems
 *  that the data vdev object interacts with, the txrx_vdev_attach
 *  function focuses primarily on creating the data vdev object.
 *  After the creation of both the data vdev object and the OS shim
 *  vdev object, this txrx_osif_vdev_attach function is used to connect
 *  the two vdev objects, so the data SW can use the OS shim vdev handle
 *  when passing rx data received by a vdev up to the OS shim.
 *
 * @param txrx_vdev - the virtual device's data object
 * @param osif_vdev - the virtual device's OS shim object
 * @param txrx_ops - (pointers to) the functions used for tx and rx data xfer
 *      There are two portions of these txrx operations.
 *      The rx portion is filled in by OSIF SW before calling
 *      ol_txrx_osif_vdev_register; inside the ol_txrx_osif_vdev_register
 *      the txrx SW stores a copy of these rx function pointers, to use
 *      as it delivers rx data frames to the OSIF SW.
 *      The tx portion is filled in by the txrx SW inside
 *      ol_txrx_osif_vdev_register; when the function call returns,
 *      the OSIF SW stores a copy of these tx functions to use as it
 *      delivers tx data frames to the txrx SW.
 *      The rx function pointer inputs consist of the following:
 *      rx: the OS shim rx function to deliver rx data frames to.
 *          This can have different values for different virtual devices,
 *          e.g. so one virtual device's OS shim directly hands rx frames to
 *          the OS, but another virtual device's OS shim filters out P2P
 *          messages before sending the rx frames to the OS.
 *          The netbufs delivered to the osif_rx function are in the format
 *          specified by the OS to use for tx and rx frames (either 802.3 or
 *          native WiFi).
 *      rx_mon: the OS shim rx monitor function to deliver monitor data to
 *          Though in practice, it is probable that the same function will
 *          be used for delivering rx monitor data for all virtual devices,
 *          in theory each different virtual device can have a different
 *          OS shim function for accepting rx monitor data.
 *          The netbufs delivered to the osif_rx_mon function are in 802.11
 *          format.  Each netbuf holds a 802.11 MPDU, not an 802.11 MSDU.
 *          Depending on compile-time configuration, each netbuf may also
 *          have a monitor-mode encapsulation header such as a radiotap
 *          header added before the MPDU contents.
 *      The tx function pointer outputs consist of the following:
 *      tx: the tx function pointer for standard data frames
 *          This function pointer is set by the txrx SW to perform
 *          host-side transmit operations based on whether a HL or LL
 *          host/target interface is in use.
 *      tx_non_std: the tx function pointer for non-standard data frames,
 *          such as TSO frames, explicitly-prioritized frames, or "raw"
 *          frames which skip some of the tx operations, such as 802.11
 *          MAC header encapsulation.
 */
void
ol_txrx_osif_vdev_register(
    ol_txrx_vdev_handle txrx_vdev,
    void *osif_vdev,
    struct ol_txrx_osif_ops *txrx_ops);

/**
 * @brief Divide a jumbo TCP frame into smaller segments.
 * @details
 *  For efficiency, the protocol stack above the WLAN driver may operate
 *  on jumbo tx frames, which are larger than the 802.11 MTU.
 *  The OSIF SW uses this txrx API function to divide the jumbo tx TCP frame
 *  into a series of segment frames.
 *  The segments are created as clones of the input jumbo frame.
 *  The txrx SW generates a new encapsulation header (ethernet + IP + TCP)
 *  for each of the output segment frames.  The exact format of this header,
 *  e.g. 802.3 vs. Ethernet II, and IPv4 vs. IPv6, is chosen to match the
 *  header format of the input jumbo frame.
 *  The input jumbo frame is not modified.
 *  After the ol_txrx_osif_tso_segment returns, the OSIF SW needs to perform
 *  DMA mapping on each of the segment network buffers, and also needs to
 *
 * @param txrx_vdev - which virtual device will transmit the TSO segments
 * @param max_seg_payload_bytes - the maximum size for the TCP payload of
 *      each segment frame.
 *      This does not include the ethernet + IP + TCP header sizes.
 * @param jumbo_tcp_frame - jumbo frame which needs to be cloned+segmented
 * @return
 *      NULL if the segmentation fails, - OR -
 *      a NULL-terminated list of segment network buffers
 */
adf_nbuf_t ol_txrx_osif_tso_segment(
    ol_txrx_vdev_handle txrx_vdev,
    int max_seg_payload_bytes,
    adf_nbuf_t jumbo_tcp_frame);

#endif /* _OL_TXRX_OSIF_API__H_ */
