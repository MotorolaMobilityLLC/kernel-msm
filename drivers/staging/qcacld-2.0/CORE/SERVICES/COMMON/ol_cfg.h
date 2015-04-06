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

#ifndef _OL_CFG__H_
#define _OL_CFG__H_

#include <adf_os_types.h> /* u_int32_t */
#include <ol_ctrl_api.h>  /* ol_pdev_handle */
#include "ieee80211_common.h"    /* ieee80211_qosframe_htc_addr4 */
#include <enet.h>         /* LLC_SNAP_HDR_LEN */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

/**
 * @brief format of data frames delivered to/from the WLAN driver by/to the OS
 */
enum wlan_frm_fmt {
    wlan_frm_fmt_unknown,
    wlan_frm_fmt_raw,
    wlan_frm_fmt_native_wifi,
    wlan_frm_fmt_802_3,
};

#ifdef IPA_UC_OFFLOAD
struct wlan_ipa_uc_rsc_t {
   u8  uc_offload_enabled;
   u32 tx_max_buf_cnt;
   u32 tx_buf_size;
   u32 rx_ind_ring_size;
   u32 tx_partition_base;
};
#endif /* IPA_UC_OFFLOAD */

/* Config parameters for txrx_pdev */
struct txrx_pdev_cfg_t {
	u8 is_high_latency;
	u8 defrag_timeout_check;
	u8 rx_pn_check;
	u8 pn_rx_fwd_check;
	u8 host_addba;
	u8 tx_free_at_download;
	u8 rx_fwd_inter_bss;
	u32 max_thruput_mbps;
	u32 target_tx_credit;
	u32 vow_config;
	u32 tx_download_size;
	u32 max_peer_id;
	u32 max_vdev;
	u32 max_nbuf_frags;
	u32 throttle_period_ms;
	enum wlan_frm_fmt frame_type;
	u8 rx_fwd_disabled;
	u8 is_packet_log_enabled;
	u8 is_full_reorder_offload;
#ifdef IPA_UC_OFFLOAD
	struct wlan_ipa_uc_rsc_t ipa_uc_rsc;
#endif /* IPA_UC_OFFLOAD */
};

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
int ol_cfg_is_high_latency(ol_pdev_handle pdev);

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
int ol_cfg_max_peer_id(ol_pdev_handle pdev);

/**
 * @brief Specify the max number of virtual devices within a physical device.
 * @details
 *  Specify how many virtual devices may exist within a physical device.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of virtual devices
 */
int ol_cfg_max_vdevs(ol_pdev_handle pdev);

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
int ol_cfg_rx_pn_check(ol_pdev_handle pdev);


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
int ol_cfg_rx_fwd_check(ol_pdev_handle pdev);

/**
 * @brief set rx fwd disable/enable.
 * @details
 *  Choose whether to forward rx frames to tx (where applicable) within the
 *  WLAN driver, or to leave all forwarding up to the operating system.
 *  currently only intra-bss fwd is supported.
 *
 * @param pdev - handle to the physical device
 * @param disable_rx_fwd 1 -> no rx->tx forward -> rx->tx forward
 */
void ol_set_cfg_rx_fwd_disabled(ol_pdev_handle pdev, u_int8_t disalbe_rx_fwd);

/**
 * @brief Check whether rx forwarding is enabled or disabled.
 * @details
 *  Choose whether to forward rx frames to tx (where applicable) within the
 *  WLAN driver, or to leave all forwarding up to the operating system.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> no rx->tx forward -OR- 0 -> rx->tx forward (in host or target)
 */
int ol_cfg_rx_fwd_disabled(ol_pdev_handle pdev);

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
int ol_cfg_rx_fwd_inter_bss(ol_pdev_handle pdev);

/**
 * @brief Specify data frame format used by the OS.
 * @details
 *  Specify what type of frame (802.3 or native WiFi) the host data SW
 *  should expect from and provide to the OS shim.
 *
 * @param pdev - handle to the physical device
 * @return enumerated data frame format
 */
enum wlan_frm_fmt ol_cfg_frame_type(ol_pdev_handle pdev);

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
int ol_cfg_max_thruput_mbps(ol_pdev_handle pdev);


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
int ol_cfg_netbuf_frags_max(ol_pdev_handle pdev);


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
int ol_cfg_tx_free_at_download(ol_pdev_handle pdev);


/**
 * @brief Low water mark for target tx credit.
 * Tx completion handler is invoked to reap the buffers when the target tx
 * credit goes below Low Water Mark.
 */
#define OL_CFG_NUM_MSDU_REAP 512
#define ol_cfg_tx_credit_lwm(pdev)                                             \
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
u_int16_t ol_cfg_target_tx_credit(ol_pdev_handle pdev);


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
 *  frame header format specified by the ol_cfg_frame_type configuration
 *  function.
 *
 * @param pdev - handle to the physical device
 * @return the number of bytes beyond the 802.3 or native WiFi header to
 *      download to the target for tx classification
 */
int ol_cfg_tx_download_size(ol_pdev_handle pdev);

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
int ol_cfg_rx_host_defrag_timeout_duplicate_check(ol_pdev_handle pdev);

/**
 * brief Query for the period in ms used for throttling for
 * thermal mitigation
 * @details
 *   In LL systems, transmit data throttling is used for thermal
 *   mitigation where data is paused and resumed during the
 *   throttle period i.e. the throttle period consists of an
 *   "on" phase when transmit is allowed and an "off" phase when
 *   transmit is suspended. This function returns the total
 *   period used for throttling.
 *
 * @param pdev - handle to the physical device
 * @return the total throttle period in ms
 */
int ol_cfg_throttle_period_ms(ol_pdev_handle pdev);

/**
 * brief Check whether full reorder offload is
 * enabled/disable by the host
 * @details
 *   If the host does not support receive reorder (i.e. the
 *   target performs full receive re-ordering) this will return
 *   "enabled"
 *
 * @param pdev - handle to the physical device
 * @return 1 - enable, 0 - disable
 */
int ol_cfg_is_full_reorder_offload(ol_pdev_handle pdev);

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
ol_cfg_sw_encap_hdr_max_size(ol_pdev_handle pdev)
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
ol_cfg_tx_encap(ol_pdev_handle pdev)
{
    /* tx encap done in HW */
    return 0;
}

static inline int
ol_cfg_host_addba(ol_pdev_handle pdev)
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
ol_cfg_addba_retry(ol_pdev_handle pdev)
{
    return 0; /* disabled for now */
}

/**
 * @brief How many frames to hold in a paused vdev's tx queue in LL systems
 */
static inline int
ol_tx_cfg_max_tx_queue_depth_ll(ol_pdev_handle pdev)
{
    /*
     * Store up to 1500 frames for a paused vdev.
     * For example, if the vdev is sending 300 Mbps of traffic, and the
     * PHY is capable of 600 Mbps, then it will take 56 ms for the PHY to
     * drain both the 700 frames that are queued initially, plus the next
     * 700 frames that come in while the PHY is catching up.
     * So in this example scenario, the PHY will remain fully utilized
     * in a MCC system that has a channel-switching period of 56 ms or less.
     * 700 frames calculation was correct when FW drain packet without
     * any overhead. Actual situation drain overhead will slowdown drain
     * speed. And channel period is less than 56 msec
     * Worst scenario, 1500 frames should be stored in host.
     */
    return 1500;
}

/**
 * @brief Set packet log config in HTT config based on CFG ini configuration
 */
void ol_set_cfg_packet_log_enabled(ol_pdev_handle pdev, u_int8_t val);

/**
 * @brief Get packet log config from HTT config
 */
u_int8_t ol_cfg_is_packet_log_enabled(ol_pdev_handle pdev);

#ifdef IPA_UC_OFFLOAD
/**
 * @brief IPA micro controller data path offload enable or not
 * @detail
 *  This function returns IPA micro controller data path offload
 *  feature enabled or not
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_offload_enabled(ol_pdev_handle pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  TX buffer size which should be pre-allocated by driver.
 *  Default buffer size is 2K
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_buf_size(ol_pdev_handle pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  TX buffer count which should be pre-allocated by driver.
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_max_buf_cnt(ol_pdev_handle pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  RX indication ring size which will notified by WLAN FW to IPA
 *  micro controller
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_rx_ind_ring_size(ol_pdev_handle pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_partition_base(ol_pdev_handle pdev);
#endif /* IPA_UC_OFFLOAD */
#endif /* _OL_CFG__H_ */
