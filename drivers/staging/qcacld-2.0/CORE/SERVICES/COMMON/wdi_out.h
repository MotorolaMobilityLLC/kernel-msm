/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

/* AUTO-GENERATED FILE - DO NOT EDIT DIRECTLY */
/**
 *   @addtogroup WDIAPI
 *@{
 */
/**
 * @file wdi_out.h
 * @brief Functions outside the WDI boundary called by datapath functions
 */
#ifndef _WDI_OUT__H_
#define _WDI_OUT__H_

#include <wdi_types.h>

#ifdef WDI_API_AS_FUNCS

#include <adf_os_types.h> /* u_int32_t */
#include <ieee80211.h>    /* ieee80211_qosframe_htc_addr4 */
#include <enet.h>         /* LLC_SNAP_HDR_LEN */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

/**
 * @brief Specify whether the system is high-latency or low-latency.
 * @details
 *  Indicate whether the system is operating in high-latency (message
 *  based, e.g. USB) mode or low-latency (memory-mapped, e.g. PCIe) mode.
 *  Some chips support just one type of host / target interface.
 *  Other chips support both LL and HL interfaces (e.g. PCIe and USB),
 *  so the selection will be made based on which bus HW is present, or
 *  which is preferred if both are present.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> high-latency -OR- 0 -> low-latency
 */
int wdi_out_cfg_is_high_latency(ol_pdev_handle pdev);

/**
 * @brief Specify the range of peer IDs.
 * @details
 *  Specify the maximum peer ID.  This is the maximum number of peers,
 *  minus one.
 *  This is used by the host to determine the size of arrays indexed by
 *  peer ID.
 *
 * @param pdev - handle to the physical device
 * @return maximum peer ID
 */
int wdi_out_cfg_max_peer_id(ol_pdev_handle pdev);

/**
 * @brief Specify the max number of virtual devices within a physical device.
 * @details
 *  Specify how many virtual devices may exist within a physical device.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of virtual devices
 */
int wdi_out_cfg_max_vdevs(ol_pdev_handle pdev);

/**
 * @brief Check whether host-side rx PN check is enabled or disabled.
 * @details
 *  Choose whether to allocate rx PN state information and perform
 *  rx PN checks (if applicable, based on security type) on the host.
 *  If the rx PN check is specified to be done on the host, the host SW
 *  will determine which peers are using a security type (e.g. CCMP) that
 *  requires a PN check.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> host performs rx PN check -OR- 0 -> no host-side rx PN check
 */
int wdi_out_cfg_rx_pn_check(ol_pdev_handle pdev);

/**
 * @brief Check whether host-side rx forwarding is enabled or disabled.
 * @details
 *  Choose whether to check whether to forward rx frames to tx on the host.
 *  For LL systems, this rx -> tx host-side forwarding check is typically
 *  enabled.
 *  For HL systems, the rx -> tx forwarding check is typically done on the
 *  target.  However, even in HL systems, the host-side rx -> tx forwarding
 *  will typically be enabled, as a second-tier safety net in case the
 *  target doesn't have enough memory to store all rx -> tx forwarded frames.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> host does rx->tx forward -OR- 0 -> no host-side rx->tx forward
 */
int wdi_out_cfg_rx_fwd_check(ol_pdev_handle pdev);

/**
 * @brief Check whether to perform inter-BSS or intra-BSS rx->tx forwarding.
 * @details
 *  Check whether data received by an AP on one virtual device destined
 *  to a STA associated with a different virtual device within the same
 *  physical device should be forwarded within the driver, or whether
 *  forwarding should only be done within a virtual device.
 *
 * @param pdev - handle to the physical device
 * @return
 *      1 -> forward both within and between vdevs
 *      -OR-
 *      0 -> forward only within a vdev
 */
int wdi_out_cfg_rx_fwd_inter_bss(ol_pdev_handle pdev);

/**
 * @brief format of data frames delivered to/from the WLAN driver by/to the OS
 */
enum wlan_frm_fmt {
    wlan_frm_fmt_unknown,
    wlan_frm_fmt_raw,
    wlan_frm_fmt_native_wifi,
    wlan_frm_fmt_802_3,
};

/**
 * @brief Specify data frame format used by the OS.
 * @details
 *  Specify what type of frame (802.3 or native WiFi) the host data SW
 *  should expect from and provide to the OS shim.
 *
 * @param pdev - handle to the physical device
 * @return enumerated data frame format
 */
enum wlan_frm_fmt wdi_out_cfg_frame_type(ol_pdev_handle pdev);

/**
 * @brief Specify the peak throughput.
 * @details
 *  Specify the peak throughput that a system is expected to support.
 *  The data SW uses this configuration to help choose the size for its
 *  tx descriptor pool and rx buffer ring.
 *  The data SW assumes that the peak throughput applies to either rx or tx,
 *  rather than having separate specs of the rx max throughput vs. the tx
 *  max throughput.
 *
 * @param pdev - handle to the physical device
 * @return maximum supported throughput in Mbps (not MBps)
 */
int wdi_out_cfg_max_thruput_mbps(ol_pdev_handle pdev);

/**
 * @brief Specify the maximum number of fragments per tx network buffer.
 * @details
 *  Specify the maximum number of fragments that a tx frame provided to
 *  the WLAN driver by the OS may contain.
 *  In LL systems, the host data SW uses this maximum fragment count to
 *  determine how many elements to allocate in the fragmentation descriptor
 *  it creates to specify to the tx MAC DMA where to locate the tx frame's
 *  data.
 *  This maximum fragments count is only for regular frames, not TSO frames,
 *  since TSO frames are sent in segments with a limited number of fragments
 *  per segment.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of fragments that can occur in a regular tx frame
 */
int wdi_out_cfg_netbuf_frags_max(ol_pdev_handle pdev);

/**
 * @brief For HL systems, specify when to free tx frames.
 * @details
 *  In LL systems, the host's tx frame is referenced by the MAC DMA, and
 *  thus cannot be freed until the target indicates that it is finished
 *  transmitting the frame.
 *  In HL systems, the entire tx frame is downloaded to the target.
 *  Consequently, the target has its own copy of the tx frame, and the
 *  host can free the tx frame as soon as the download completes.
 *  Alternatively, the HL host can keep the frame allocated until the
 *  target explicitly tells the HL host it is done transmitting the frame.
 *  This gives the target the option of discarding its copy of the tx
 *  frame, and then later getting a new copy from the host.
 *  This function tells the host whether it should retain its copy of the
 *  transmit frames until the target explicitly indicates it is finished
 *  transmitting them, or if it should free its copy as soon as the
 *  tx frame is downloaded to the target.
 *
 * @param pdev - handle to the physical device
 * @return
 *      0 -> retain the tx frame until the target indicates it is done
 *          transmitting the frame
 *      -OR-
 *      1 -> free the tx frame as soon as the download completes
 */
int wdi_out_cfg_tx_free_at_download(ol_pdev_handle pdev);

/**
 * @brief Low water mark for target tx credit.
 * Tx completion handler is invoked to reap the buffers when the target tx
 * credit goes below Low Water Mark.
 */
#define OL_CFG_NUM_MSDU_REAP 512
#define wdi_out_cfg_tx_credit_lwm(pdev)                                             \
        ((CFG_TGT_NUM_MSDU_DESC >  OL_CFG_NUM_MSDU_REAP) ?                     \
                        (CFG_TGT_NUM_MSDU_DESC -  OL_CFG_NUM_MSDU_REAP) : 0)

/**
 * @brief In a HL system, specify the target initial credit count.
 * @details
 *  The HL host tx data SW includes a module for determining which tx frames
 *  to download to the target at a given time.
 *  To make this judgement, the HL tx download scheduler has to know
 *  how many buffers the HL target has available to hold tx frames.
 *  Due to the possibility that a single target buffer pool can be shared
 *  between rx and tx frames, the host may not be able to obtain a precise
 *  specification of the tx buffer space available in the target, but it
 *  uses the best estimate, as provided by this configuration function,
 *  to determine how best to schedule the tx frame downloads.
 *
 * @param pdev - handle to the physical device
 * @return the number of tx buffers available in a HL target
 */
int wdi_out_cfg_target_tx_credit(ol_pdev_handle pdev);

/**
 * @brief Specify the LL tx MSDU header download size.
 * @details
 *  In LL systems, determine how many bytes from a tx frame to download,
 *  in order to provide the target FW's Descriptor Engine with enough of
 *  the packet's payload to interpret what kind of traffic this is,
 *  and who it is for.
 *  This download size specification does not include the 802.3 / 802.11
 *  frame encapsulation headers; it starts with the encapsulated IP packet
 *  (or whatever ethertype is carried within the ethernet-ish frame).
 *  The LL host data SW will determine how many bytes of the MSDU header to
 *  download by adding this download size specification to the size of the
 *  frame header format specified by the wdi_out_cfg_frame_type configuration
 *  function.
 *
 * @param pdev - handle to the physical device
 * @return the number of bytes beyond the 802.3 or native WiFi header to
 *      download to the target for tx classification
 */
int wdi_out_cfg_tx_download_size(ol_pdev_handle pdev);

/**
 * brief Specify where defrag timeout and duplicate detection is handled
 * @details
 *   non-aggregate duplicate detection and timing out stale fragments
 *   requires additional target memory. To reach max client
 *   configurations (128+), non-aggregate duplicate detection and the
 *   logic to time out stale fragments is moved to the host.
 *
 * @param pdev - handle to the physical device
 * @return
 *  0 -> target is responsible non-aggregate duplicate detection and
 *          timing out stale fragments.
 *
 *  1 -> host is responsible non-aggregate duplicate detection and
 *          timing out stale fragments.
 */
int wdi_out_cfg_rx_host_defrag_timeout_duplicate_check(ol_pdev_handle pdev);

typedef enum {
   wlan_frm_tran_cap_raw = 0x01,
   wlan_frm_tran_cap_native_wifi = 0x02,
   wlan_frm_tran_cap_8023 = 0x04,
} wlan_target_fmt_translation_caps;

/**
 * @brief Specify the maximum header size added by SW tx encapsulation
 * @details
 *  This function returns the maximum size of the new L2 header, not the
 *  difference between the new and old L2 headers.
 *  Thus, this function returns the maximum 802.11 header size that the
 *  tx SW may need to add to tx data frames.
 *
 * @param pdev - handle to the physical device
 */
static inline int
wdi_out_cfg_sw_encap_hdr_max_size(ol_pdev_handle pdev)
{
    /*
     *  24 byte basic 802.11 header
     * + 6 byte 4th addr
     * + 2 byte QoS control
     * + 4 byte HT control
     * + 8 byte LLC/SNAP
     */
    return sizeof(struct ieee80211_qosframe_htc_addr4) + LLC_SNAP_HDR_LEN;
}

static inline u_int8_t
wdi_out_cfg_tx_encap(ol_pdev_handle pdev)
{
    /* tx encap done in HW */
    return 0;
}

static inline int
wdi_out_cfg_host_addba(ol_pdev_handle pdev)
{
    /*
     * ADDBA negotiation is handled by the target FW for Peregrine + Rome.
     */
    return 0;
}

/**
 * @brief If the host SW's ADDBA negotiation fails, should it be retried?
 *
 * @param pdev - handle to the physical device
 */
static inline int
wdi_out_cfg_addba_retry(ol_pdev_handle pdev)
{
    return 0; /* disabled for now */
}

/**
 * @brief How many frames to hold in a paused vdev's tx queue in LL systems
 */
static inline int
wdi_out_tx_cfg_max_tx_queue_depth_ll(ol_pdev_handle pdev)
{
    /*
     * Store up to 700 frames for a paused vdev.
     * For example, if the vdev is sending 300 Mbps of traffic, and the
     * PHY is capable of 600 Mbps, then it will take 56 ms for the PHY to
     * drain both the 700 frames that are queued initially, plus the next
     * 700 frames that come in while the PHY is catching up.
     * So in this example scenario, the PHY will remain fully utilized
     * in a MCC system that has a channel-switching period of 56 ms or less.
     */
    return 700;
}

//#include <osapi_linux.h>      /* u_int8_t */
#include <osdep.h>        /* u_int8_t */
#include <adf_nbuf.h>     /* adf_nbuf_t */

enum ol_rx_err_type {
    OL_RX_ERR_DEFRAG_MIC,
    OL_RX_ERR_PN,
    OL_RX_ERR_UNKNOWN_PEER,
    OL_RX_ERR_MALFORMED,
    OL_RX_ERR_TKIP_MIC,
	OL_RX_ERR_DECRYPT,
	OL_RX_ERR_MPDU_LENGTH,
	OL_RX_ERR_ENCRYPT_REQUIRED,
	OL_RX_ERR_DUP,
	OL_RX_ERR_UNKNOWN,
	OL_RX_ERR_FCS,
	OL_RX_ERR_PRIVACY,
	OL_RX_ERR_NONE_FRAG,
	OL_RX_ERR_NONE = 0xFF
};

#ifdef SUPPORT_HOST_STATISTICS
/** * @brief Update tx statistics
 * @details
 *  Update tx statistics after tx complete.
 *
 * @param pdev - ol_pdev_handle instance
 * @param vdev_id - ID of the virtual device that tx frame
 * @param had_error - whether there is error when tx
 */
void wdi_out_tx_statistics(ol_pdev_handle pdev,
                      u_int16_t vdev_id,
					  int had_error);
#else
#define wdi_out_tx_statistics(pdev, vdev_id, had_error)
#endif

/** * @brief Count on received packets for invalid peer case
 *
 * @param pdev - txrx pdev handle
 * @param wh - received frame
 * @param err_type - what kind of error occurred
 */
void wdi_out_rx_err_inv_peer_statistics(ol_pdev_handle pdev,
								   struct ieee80211_frame *wh,
								   enum ol_rx_err_type err_type);

/**
 * @brief Count on received packets, both success and failed
 *
 * @param pdev - ol_pdev_handle handle
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param err_type - what kind of error occurred
 * @param sec_type - The cipher type the peer is using
 * @param is_mcast - whether this is one multi cast frame
 */
void wdi_out_rx_err_statistics(ol_pdev_handle pdev,
						  u_int8_t vdev_id,
						  enum ol_rx_err_type err_type,
						  enum ol_sec_type sec_type,
						  int is_mcast);

/**
 * @brief Provide notification of failure during host rx processing
 * @details
 *  Indicate an error during host rx data processing, including what
 *  kind of error happened, when it happened, which peer and TID the
 *  erroneous rx frame is from, and what the erroneous rx frame itself
 *  is.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param peer_mac_addr - MAC address of the peer that sent the erroneous
 *      rx frame
 * @param tid - which TID within the peer sent the erroneous rx frame
 * @param tsf32  - the timstamp in TSF units of the erroneous rx frame, or
 *      one of the fragments that when reassembled, constitute the rx frame
 * @param err_type - what kind of error occurred
 * @param rx_frame - the rx frame that had an error
 * @pn - Packet sequence number
 * @key_id - Key index octet received in IV of the frame
 */
void
wdi_out_rx_err(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_err_type err_type,
    adf_nbuf_t rx_frame,
    u_int64_t *pn,
    u_int8_t key_id);

enum ol_rx_notify_type {
    OL_RX_NOTIFY_IPV4_IGMP,
};

/**
 * @brief Provide notification of reception of data of special interest.
 * @details
 *  Indicate when "special" data has been received.  The nature of the
 *  data that results in it being considered special is specified in the
 *  notify_type argument.
 *  This function is currently used by the data-path SW to notify the
 *  control path SW when the following types of rx data are received:
 *    + IPv4 IGMP frames
 *      The control SW can use these to learn about multicast group
 *      membership, if it so chooses.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the special data
 * @param peer_mac_addr - MAC address of the peer that sent the special data
 * @param tid - which TID within the peer sent the special data
 * @param tsf32  - the timstamp in TSF units of the special data
 * @param notify_type - what kind of special data was received
 * @param rx_frame - the rx frame containing the special data
 */
void
wdi_out_rx_notify(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_notify_type notify_type,
    adf_nbuf_t rx_frame);

/**
 * @brief Indicate when a paused STA has tx data available.
 * @details
 *  Indicate to the control SW when a paused peer that previously
 *  has all its peer-TID queues empty gets a MSDU to transmit.
 *  Conversely, indicate when a paused peer that had data in one or more of
 *  its peer-TID queues has all queued data removed (e.g. due to a U-APSD
 *  triggered transmission), but is still paused.
 *  It is up to the control SW to determine whether the peer is paused due to
 *  being in power-save sleep, or some other reason, and thus whether it is
 *  necessary to set the TIM in beacons to notify a sleeping STA that it has
 *  data.
 *  The data SW will also issue this wdi_out_tx_paused_peer_data call when an
 *  unpaused peer that currently has tx data in one or more of its
 *  peer-TID queues becomes paused.
 *  The data SW will not issue this wdi_out_tx_paused_peer_data call when a
 *  peer with data in one or more of its peer-TID queues becomes unpaused.
 *
 * @param peer - the paused peer
 * @param has_tx_data -
 *      1 -> a paused peer that previously had no tx data now does, -OR-
 *      0 -> a paused peer that previously had tx data now doesnt
 */
void
wdi_out_tx_paused_peer_data(ol_peer_handle peer, int has_tx_data);


#define wdi_out_ctrl_addba_req(pdev, peer_mac_addr, tid) ol_addba_req_reject
#define wdi_out_ctrl_rx_addba_complete(pdev, peer_mac_addr, tid, failed) /* no-op */


#else
/* WDI_API_AS_MACROS */

#include <ol_cfg.h>

#define wdi_out_cfg_is_high_latency ol_cfg_is_high_latency
#define wdi_out_cfg_max_peer_id ol_cfg_max_peer_id
#define wdi_out_cfg_max_vdevs ol_cfg_max_vdevs
#define wdi_out_cfg_rx_pn_check ol_cfg_rx_pn_check
#define wdi_out_cfg_rx_fwd_check ol_cfg_rx_fwd_check
#define wdi_out_cfg_rx_fwd_inter_bss ol_cfg_rx_fwd_inter_bss
#define wdi_out_cfg_frame_type ol_cfg_frame_type
#define wdi_out_cfg_max_thruput_mbps ol_cfg_max_thruput_mbps
#define wdi_out_cfg_netbuf_frags_max ol_cfg_netbuf_frags_max
#define wdi_out_cfg_tx_free_at_download ol_cfg_tx_free_at_download
#define wdi_out_cfg_tx_credit_lwm ol_cfg_tx_credit_lwm
#define wdi_out_cfg_target_tx_credit ol_cfg_target_tx_credit
#define wdi_out_cfg_tx_download_size ol_cfg_tx_download_size
#define wdi_out_cfg_rx_host_defrag_timeout_duplicate_check ol_cfg_rx_host_defrag_timeout_duplicate_check
#define wdi_out_cfg_sw_encap_hdr_max_size ol_cfg_sw_encap_hdr_max_size
#define wdi_out_cfg_tx_encap ol_cfg_tx_encap
#define wdi_out_cfg_host_addba ol_cfg_host_addba
#define wdi_out_cfg_addba_retry ol_cfg_addba_retry
#define wdi_out_tx_cfg_max_tx_queue_depth_ll ol_tx_cfg_max_tx_queue_depth_ll

#include <ol_ctrl_txrx_api.h>

#define wdi_out_tx_statistics ol_tx_statistics
#define wdi_out_tx_statistics ol_tx_statistics
#define wdi_out_rx_err_inv_peer_statistics ol_rx_err_inv_peer_statistics
#define wdi_out_rx_err_statistics ol_rx_err_statistics
#define wdi_out_rx_err ol_rx_err
#define wdi_out_rx_notify ol_rx_notify
#define wdi_out_tx_paused_peer_data ol_tx_paused_peer_data
#define wdi_out_ctrl_addba_req ol_ctrl_addba_req
#define wdi_out_ctrl_rx_addba_complete ol_ctrl_rx_addba_complete
#define wdi_out_cfg_rx_fwd_disabled ol_cfg_rx_fwd_disabled

#endif /* WDI_API_AS_FUNCS / MACROS */

#endif /* _WDI_OUT__H_ */

/**@}*/
