/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

#ifndef _OL_CFG__H_
#define _OL_CFG__H_

#include <qdf_types.h>          /* uint32_t */
#include <cdp_txrx_cmn.h>       /* ol_pdev_handle */
#include <cds_ieee80211_common.h>   /* ieee80211_qosframe_htc_addr4 */
#include <enet.h>               /* LLC_SNAP_HDR_LEN */
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include "ol_txrx_ctrl_api.h"   /* txrx_pdev_cfg_param_t */
#include <cdp_txrx_handle.h>
#include "qca_vendor.h"

/**
 * @brief format of data frames delivered to/from the WLAN driver by/to the OS
 */
enum wlan_frm_fmt {
	wlan_frm_fmt_unknown,
	wlan_frm_fmt_raw,
	wlan_frm_fmt_native_wifi,
	wlan_frm_fmt_802_3,
};

/* Max throughput */
#ifdef SLUB_MEM_OPTIMIZE
#define MAX_THROUGHPUT 400
#else
#define MAX_THROUGHPUT 800
#endif

/* Throttle period Different level Duty Cycle values*/
#define THROTTLE_DUTY_CYCLE_LEVEL0 (0)
#define THROTTLE_DUTY_CYCLE_LEVEL1 (50)
#define THROTTLE_DUTY_CYCLE_LEVEL2 (75)
#define THROTTLE_DUTY_CYCLE_LEVEL3 (94)

struct wlan_ipa_uc_rsc_t {
	u8 uc_offload_enabled;
	u32 tx_max_buf_cnt;
	u32 tx_buf_size;
	u32 rx_ind_ring_size;
	u32 tx_partition_base;
};

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
	u8 dutycycle_level[4];
	enum wlan_frm_fmt frame_type;
	u8 rx_fwd_disabled;
	u8 is_packet_log_enabled;
	u8 is_full_reorder_offload;
#ifdef WLAN_FEATURE_TSF_PLUS
	u8 is_ptp_rx_opt_enabled;
#endif
	struct wlan_ipa_uc_rsc_t ipa_uc_rsc;
	bool ip_tcp_udp_checksum_offload;
	bool p2p_ip_tcp_udp_checksum_offload;
	/* IP, TCP and UDP checksum offload for NAN Mode*/
	bool nan_tcp_udp_checksumoffload;
	bool enable_rxthread;
	bool ce_classify_enabled;
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
	uint32_t tx_flow_stop_queue_th;
	uint32_t tx_flow_start_queue_offset;
#endif
	bool flow_steering_enabled;
	/*
	 * To track if credit reporting through
	 * HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND is enabled/disabled.
	 * In Genoa(QCN7605) credits are reported through
	 * HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND only.
	 */
	u8 credit_update_enabled;
	struct ol_tx_sched_wrr_ac_specs_t ac_specs[QCA_WLAN_AC_ALL];
	bool gro_enable;
	bool tso_enable;
	bool lro_enable;
	bool enable_data_stall_detection;
	bool enable_flow_steering;
	bool disable_intra_bss_fwd;
	/* IPA Micro controller data path offload TX buffer size */
	uint32_t uc_tx_buffer_size;
	/* IPA Micro controller data path offload RX indication ring count */
	uint32_t uc_rx_indication_ring_count;
	/* IPA Micro controller data path offload TX partition base */
	uint32_t uc_tx_partition_base;
	/* Flag to indicate whether new htt format is supported */
	bool new_htt_format_enabled;

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
	/* enable the tcp delay ack feature in the driver */
	bool  del_ack_enable;
	/* timeout if no more tcp ack frames, unit is ms */
	uint16_t del_ack_timer_value;
	/* the maximum number of replaced tcp ack frames */
	uint16_t del_ack_pkt_count;
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
	uint16_t bundle_timer_value;
	uint16_t bundle_size;
#endif
	uint8_t pktlog_buffer_size;
};

/**
 * ol_tx_set_flow_control_parameters() - set flow control parameters
 * @cfg_ctx: cfg context
 * @cfg_param: cfg parameters
 *
 * Return: none
 */
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
void ol_tx_set_flow_control_parameters(struct cdp_cfg *cfg_ctx,
				       struct txrx_pdev_cfg_param_t *cfg_param);
#else
static inline
void ol_tx_set_flow_control_parameters(struct cdp_cfg *cfg_ctx,
				       struct txrx_pdev_cfg_param_t *cfg_param)
{
}
#endif

/**
 * ol_pdev_cfg_attach - setup configuration parameters
 * @osdev: OS handle needed as an argument for some OS primitives
 * @cfg_param: configuration parameters
 *
 * Allocation configuration context that will be used across data path
 *
 * Return: the control device object
 */
struct cdp_cfg *ol_pdev_cfg_attach(qdf_device_t osdev, void *pcfg_param);

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
int ol_cfg_is_high_latency(struct cdp_cfg *cfg_pdev);

/**
 * @brief Specify whether credit reporting through
 * HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND is enabled by default.
 * In Genoa credits are reported only through
 * HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND
 * @details
 * @param pdev - handle to the physical device
 * @return 1 -> enabled -OR- 0 -> disabled
 */
int ol_cfg_is_credit_update_enabled(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_max_peer_id(struct cdp_cfg *cfg_pdev);

/**
 * @brief Specify the max number of virtual devices within a physical device.
 * @details
 *  Specify how many virtual devices may exist within a physical device.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of virtual devices
 */
int ol_cfg_max_vdevs(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_rx_pn_check(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_rx_fwd_check(struct cdp_cfg *cfg_pdev);

/**
 * ol_set_cfg_rx_fwd_disabled - set rx fwd disable/enable
 *
 * @pdev - handle to the physical device
 * @disable_rx_fwd 1 -> no rx->tx forward -> rx->tx forward
 *
 * Choose whether to forward rx frames to tx (where applicable) within the
 * WLAN driver, or to leave all forwarding up to the operating system.
 * Currently only intra-bss fwd is supported.
 *
 */
void ol_set_cfg_rx_fwd_disabled(struct cdp_cfg *ppdev, uint8_t disable_rx_fwd);

/**
 * ol_set_cfg_packet_log_enabled - Set packet log config in HTT
 * config based on CFG ini configuration
 *
 * @pdev - handle to the physical device
 * @val - 0 - disable, 1 - enable
 */
void ol_set_cfg_packet_log_enabled(struct cdp_cfg *ppdev, uint8_t val);

/**
 * @brief Check whether rx forwarding is enabled or disabled.
 * @details
 *  Choose whether to forward rx frames to tx (where applicable) within the
 *  WLAN driver, or to leave all forwarding up to the operating system.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> no rx->tx forward -OR- 0 -> rx->tx forward (in host or target)
 */
int ol_cfg_rx_fwd_disabled(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_rx_fwd_inter_bss(struct cdp_cfg *cfg_pdev);

/**
 * @brief Specify data frame format used by the OS.
 * @details
 *  Specify what type of frame (802.3 or native WiFi) the host data SW
 *  should expect from and provide to the OS shim.
 *
 * @param pdev - handle to the physical device
 * @return enumerated data frame format
 */
enum wlan_frm_fmt ol_cfg_frame_type(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_max_thruput_mbps(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_netbuf_frags_max(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_tx_free_at_download(struct cdp_cfg *cfg_pdev);
void ol_cfg_set_tx_free_at_download(struct cdp_cfg *cfg_pdev);

/**
 * @brief Low water mark for target tx credit.
 * Tx completion handler is invoked to reap the buffers when the target tx
 * credit goes below Low Water Mark.
 */
#define OL_CFG_NUM_MSDU_REAP 512
#define ol_cfg_tx_credit_lwm(pdev)					       \
	((CFG_TGT_NUM_MSDU_DESC >  OL_CFG_NUM_MSDU_REAP) ?		       \
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
uint16_t ol_cfg_target_tx_credit(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_tx_download_size(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_rx_host_defrag_timeout_duplicate_check(struct cdp_cfg *cfg_pdev);

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
int ol_cfg_throttle_period_ms(struct cdp_cfg *cfg_pdev);

/**
 * brief Query for the duty cycle in percentage used for throttling for
 * thermal mitigation
 *
 * @param pdev - handle to the physical device
 * @param level - duty cycle level
 * @return the duty cycle level in percentage
 */
int ol_cfg_throttle_duty_cycle_level(struct cdp_cfg *cfg_pdev, int level);

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
int ol_cfg_is_full_reorder_offload(struct cdp_cfg *cfg_pdev);

int ol_cfg_is_rx_thread_enabled(struct cdp_cfg *cfg_pdev);

#ifdef WLAN_FEATURE_TSF_PLUS
void ol_set_cfg_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev, u_int8_t val);
u_int8_t ol_cfg_is_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev);
#else
static inline void
ol_set_cfg_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev, u_int8_t val)
{
}

static inline u_int8_t
ol_cfg_is_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev)
{
	return 0;
}
#endif

/**
 * ol_cfg_is_ip_tcp_udp_checksum_offload_enabled() - return
 *                        ip_tcp_udp_checksum_offload is enable/disable
 * @pdev : handle to the physical device
 *
 * Return: 1 - enable, 0 - disable
 */
static inline
int ol_cfg_is_ip_tcp_udp_checksum_offload_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ip_tcp_udp_checksum_offload;
}


#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
int ol_cfg_get_tx_flow_stop_queue_th(struct cdp_cfg *cfg_pdev);

int ol_cfg_get_tx_flow_start_queue_offset(struct cdp_cfg *cfg_pdev);
#endif

bool ol_cfg_is_ce_classify_enabled(struct cdp_cfg *cfg_pdev);

enum wlan_target_fmt_translation_caps {
	wlan_frm_tran_cap_raw = 0x01,
	wlan_frm_tran_cap_native_wifi = 0x02,
	wlan_frm_tran_cap_8023 = 0x04,
};

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
static inline int ol_cfg_sw_encap_hdr_max_size(struct cdp_cfg *cfg_pdev)
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

static inline uint8_t ol_cfg_tx_encap(struct cdp_cfg *cfg_pdev)
{
	/* tx encap done in HW */
	return 0;
}

static inline int ol_cfg_host_addba(struct cdp_cfg *cfg_pdev)
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
static inline int ol_cfg_addba_retry(struct cdp_cfg *cfg_pdev)
{
	return 0;               /* disabled for now */
}

/**
 * @brief How many frames to hold in a paused vdev's tx queue in LL systems
 */
static inline int ol_tx_cfg_max_tx_queue_depth_ll(struct cdp_cfg *cfg_pdev)
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
 * @brief Get packet log config from HTT config
 */
uint8_t ol_cfg_is_packet_log_enabled(struct cdp_cfg *cfg_pdev);

#ifdef IPA_OFFLOAD
/**
 * @brief IPA micro controller data path offload enable or not
 * @detail
 *  This function returns IPA micro controller data path offload
 *  feature enabled or not
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_offload_enabled(struct cdp_cfg *cfg_pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  TX buffer size which should be pre-allocated by driver.
 *  Default buffer size is 2K
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_buf_size(struct cdp_cfg *cfg_pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  TX buffer count which should be pre-allocated by driver.
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_max_buf_cnt(struct cdp_cfg *cfg_pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @detail
 *  This function returns IPA micro controller data path offload
 *  RX indication ring size which will notified by WLAN FW to IPA
 *  micro controller
 *
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_rx_ind_ring_size(struct cdp_cfg *cfg_pdev);
/**
 * @brief IPA micro controller data path TX buffer size
 * @param pdev - handle to the physical device
 */
unsigned int ol_cfg_ipa_uc_tx_partition_base(struct cdp_cfg *cfg_pdev);
void ol_cfg_set_ipa_uc_tx_partition_base(struct cdp_cfg *cfg_pdev,
					 uint32_t value);
#else
static inline unsigned int ol_cfg_ipa_uc_offload_enabled(
	struct cdp_cfg *cfg_pdev)
{
	return 0;
}

static inline unsigned int ol_cfg_ipa_uc_tx_buf_size(
	struct cdp_cfg *cfg_pdev)
{
	return 0;
}

static inline unsigned int ol_cfg_ipa_uc_tx_max_buf_cnt(
	struct cdp_cfg *cfg_pdev)
{
	return 0;
}

static inline unsigned int ol_cfg_ipa_uc_rx_ind_ring_size(
	struct cdp_cfg *cfg_pdev)
{
	return 0;
}

static inline unsigned int ol_cfg_ipa_uc_tx_partition_base(
	struct cdp_cfg *cfg_pdev)
{
	return 0;
}

static inline void ol_cfg_set_ipa_uc_tx_partition_base(
	void *cfg_pdev, uint32_t value)
{
}
#endif /* IPA_OFFLOAD */

/**
 * ol_set_cfg_flow_steering - Set Rx flow steering config based on CFG ini
 *			      config.
 *
 * @pdev - handle to the physical device
 * @val - 0 - disable, 1 - enable
 *
 * Return: None
 */
static inline void ol_set_cfg_flow_steering(struct cdp_cfg *cfg_pdev,
				uint8_t val)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->flow_steering_enabled = val;
}

/**
 * ol_cfg_is_flow_steering_enabled - Return Rx flow steering config.
 *
 * @pdev - handle to the physical device
 *
 * Return: value of configured flow steering value.
 */
static inline uint8_t ol_cfg_is_flow_steering_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->flow_steering_enabled;
}

/**
 * ol_set_cfg_new_htt_format - Set whether FW supports new htt format
 *
 * @pdev - handle to the physical device
 * @val - true - supported, false - not supported
 *
 * Return: None
 */
static inline void
ol_set_cfg_new_htt_format(struct cdp_cfg *cfg_pdev, bool val)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->new_htt_format_enabled = val;
}

/**
 * ol_cfg_is_htt_new_format_enabled - Return whether FW supports new htt format
 *
 * @pdev - handle to the physical device
 *
 * Return: value of configured htt_new_format
 */
static inline bool
ol_cfg_is_htt_new_format_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->new_htt_format_enabled;
}

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/**
 * ol_cfg_get_del_ack_timer_value() - get delayed ack timer value
 * @cfg_pdev: pdev handle
 *
 * Return: timer value
 */
int ol_cfg_get_del_ack_timer_value(struct cdp_cfg *cfg_pdev);

/**
 * ol_cfg_get_del_ack_enable_value() - get delayed ack enable value
 * @cfg_pdev: pdev handle
 *
 * Return: enable/disable
 */
bool ol_cfg_get_del_ack_enable_value(struct cdp_cfg *cfg_pdev);

/**
 * ol_cfg_get_del_ack_count_value() - get delayed ack count value
 * @cfg_pdev: pdev handle
 *
 * Return: count value
 */
int ol_cfg_get_del_ack_count_value(struct cdp_cfg *cfg_pdev);

/**
 * ol_cfg_update_del_ack_params() - update delayed ack params
 * @cfg_ctx: cfg context
 * @cfg_param: parameters
 *
 * Return: none
 */
void ol_cfg_update_del_ack_params(struct txrx_pdev_cfg_t *cfg_ctx,
				  struct txrx_pdev_cfg_param_t *cfg_param);
#else
/**
 * ol_cfg_update_del_ack_params() - update delayed ack params
 * @cfg_ctx: cfg context
 * @cfg_param: parameters
 *
 * Return: none
 */
static inline
void ol_cfg_update_del_ack_params(struct txrx_pdev_cfg_t *cfg_ctx,
				  struct txrx_pdev_cfg_param_t *cfg_param)
{
}
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
int ol_cfg_get_bundle_timer_value(struct cdp_cfg *cfg_pdev);
int ol_cfg_get_bundle_size(struct cdp_cfg *cfg_pdev);
#else
#endif
/**
 * ol_cfg_get_wrr_skip_weight() - brief Query for the param of wrr_skip_weight
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: wrr_skip_weight for specified ac.
 */
int ol_cfg_get_wrr_skip_weight(struct cdp_cfg *pdev, int ac);

/**
 * ol_cfg_get_credit_threshold() - Query for the param of credit_threshold
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: credit_threshold for specified ac.
 */
uint32_t ol_cfg_get_credit_threshold(struct cdp_cfg *pdev, int ac);

/**
 * ol_cfg_get_send_limit() - Query for the param of send_limit
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: send_limit for specified ac.
 */
uint16_t ol_cfg_get_send_limit(struct cdp_cfg *pdev, int ac);

/**
 * ol_cfg_get_credit_reserve() - Query for the param of credit_reserve
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: credit_reserve for specified ac.
 */
int ol_cfg_get_credit_reserve(struct cdp_cfg *pdev, int ac);

/**
 * ol_cfg_get_discard_weight() - Query for the param of discard_weight
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: discard_weight for specified ac.
 */
int ol_cfg_get_discard_weight(struct cdp_cfg *pdev, int ac);
#endif /* _OL_CFG__H_ */
