/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_cp_stats_mc_defs.h
 *
 * This file provide definition for structure/enums/defines related to control
 * path stats componenet
 */

#ifndef __WLAN_CP_STATS_MC_DEFS_H__
#define __WLAN_CP_STATS_MC_DEFS_H__

#include "wlan_cmn.h"
#include "qdf_event.h"
/* For WMI_MAX_CHAINS */
#include "wmi_unified.h"

#ifdef QCA_SUPPORT_MC_CP_STATS
#include "wlan_cp_stats_public_structs.h"
#endif

#ifdef WLAN_SUPPORT_TWT

#include <wmi_unified_twt_param.h>
/* Max TWT sessions per peer (supported by fw) */
#define TWT_PEER_MAX_SESSIONS 1

#endif /* WLAN_SUPPORT_TWT */

#define MAX_NUM_CHAINS              2

#define MAX_MIB_STATS               1

#define IS_MSB_SET(__num) ((__num) & BIT(31))
#define IS_LSB_SET(__num) ((__num) & BIT(0))

#define VDEV_ALL                    0xFF

/**
 * enum stats_req_type - enum indicating bit position of various stats type in
 * request map
 * @TYPE_CONNECTION_TX_POWER: tx power was requested
 * @TYPE_STATION_STATS: station stats was requested
 * @TYPE_PEER_STATS: peer stats was requested
 * @TYPE_MIB_STATS: MIB stats was requested
 * @TYPE_PEER_STATS_INFO_EXT: peer stats info ext was requested
 * @TYPE_BIG_DATA_STATS: big data stats was requested
 */
enum stats_req_type {
	TYPE_CONNECTION_TX_POWER = 0,
	TYPE_STATION_STATS,
	TYPE_PEER_STATS,
	TYPE_MIB_STATS,
	TYPE_PEER_STATS_INFO_EXT,
	TYPE_BIG_DATA_STATS,
	TYPE_MAX,
};

/**
 * enum tx_rate_info - tx rate flags
 * @TX_RATE_LEGACY: Legacy rates
 * @TX_RATE_HT20: HT20 rates
 * @TX_RATE_HT40: HT40 rates
 * @TX_RATE_SGI: Rate with Short guard interval
 * @TX_RATE_LGI: Rate with Long guard interval
 * @TX_RATE_VHT20: VHT 20 rates
 * @TX_RATE_VHT40: VHT 40 rates
 * @TX_RATE_VHT80: VHT 80 rates
 * @TX_RATE_HE20: HE 20 rates
 * @TX_RATE_HE40: HE 40 rates
 * @TX_RATE_HE80: HE 80 rates
 * @TX_RATE_HE160: HE 160 rates
 * @TX_RATE_VHT160: VHT 160 rates
 */
enum tx_rate_info {
	TX_RATE_LEGACY = 0x1,
	TX_RATE_HT20 = 0x2,
	TX_RATE_HT40 = 0x4,
	TX_RATE_SGI = 0x8,
	TX_RATE_LGI = 0x10,
	TX_RATE_VHT20 = 0x20,
	TX_RATE_VHT40 = 0x40,
	TX_RATE_VHT80 = 0x80,
	TX_RATE_HE20 = 0x100,
	TX_RATE_HE40 = 0x200,
	TX_RATE_HE80 = 0x400,
	TX_RATE_HE160 = 0x800,
	TX_RATE_VHT160 = 0x1000,
};

/**
 * enum - txrate_gi
 * @txrate_gi_0_8_US: guard interval 0.8 us
 * @txrate_gi_0_4_US: guard interval 0.4 us for legacy
 * @txrate_gi_1_6_US: guard interval 1.6 us
 * @txrate_gi_3_2_US: guard interval 3.2 us
 */
enum txrate_gi {
	TXRATE_GI_0_8_US = 0,
	TXRATE_GI_0_4_US,
	TXRATE_GI_1_6_US,
	TXRATE_GI_3_2_US,
};

/**
 * struct wake_lock_stats - wake lock stats structure
 * @ucast_wake_up_count:        Unicast wakeup count
 * @bcast_wake_up_count:        Broadcast wakeup count
 * @ipv4_mcast_wake_up_count:   ipv4 multicast wakeup count
 * @ipv6_mcast_wake_up_count:   ipv6 multicast wakeup count
 * @ipv6_mcast_ra_stats:        ipv6 multicast ra stats
 * @ipv6_mcast_ns_stats:        ipv6 multicast ns stats
 * @ipv6_mcast_na_stats:        ipv6 multicast na stats
 * @icmpv4_count:               ipv4 icmp packet count
 * @icmpv6_count:               ipv6 icmp packet count
 * @rssi_breach_wake_up_count:  rssi breach wakeup count
 * @low_rssi_wake_up_count:     low rssi wakeup count
 * @gscan_wake_up_count:        gscan wakeup count
 * @pno_complete_wake_up_count: pno complete wakeup count
 * @pno_match_wake_up_count:    pno match wakeup count
 * @oem_response_wake_up_count: oem response wakeup count
 * @uc_drop_wake_up_count:      local data uc drop wakeup count
 * @fatal_event_wake_up_count:  fatal event wakeup count
 * @pwr_save_fail_detected:     pwr save fail detected wakeup count
 * @scan_11d                    11d scan wakeup count
 * @mgmt_assoc: association request management frame
 * @mgmt_disassoc: disassociation management frame
 * @mgmt_assoc_resp: association response management frame
 * @mgmt_reassoc: reassociate request management frame
 * @mgmt_reassoc_resp: reassociate response management frame
 * @mgmt_auth: authentication managament frame
 * @mgmt_deauth: deauthentication management frame
 * @mgmt_action: action managament frame
 */
struct wake_lock_stats {
	uint32_t ucast_wake_up_count;
	uint32_t bcast_wake_up_count;
	uint32_t ipv4_mcast_wake_up_count;
	uint32_t ipv6_mcast_wake_up_count;
	uint32_t ipv6_mcast_ra_stats;
	uint32_t ipv6_mcast_ns_stats;
	uint32_t ipv6_mcast_na_stats;
	uint32_t icmpv4_count;
	uint32_t icmpv6_count;
	uint32_t rssi_breach_wake_up_count;
	uint32_t low_rssi_wake_up_count;
	uint32_t gscan_wake_up_count;
	uint32_t pno_complete_wake_up_count;
	uint32_t pno_match_wake_up_count;
	uint32_t oem_response_wake_up_count;
	uint32_t uc_drop_wake_up_count;
	uint32_t fatal_event_wake_up_count;
	uint32_t pwr_save_fail_detected;
	uint32_t scan_11d;
	uint32_t mgmt_assoc;
	uint32_t mgmt_disassoc;
	uint32_t mgmt_assoc_resp;
	uint32_t mgmt_reassoc;
	uint32_t mgmt_reassoc_resp;
	uint32_t mgmt_auth;
	uint32_t mgmt_deauth;
	uint32_t mgmt_action;
};

struct stats_event;

/**
 * struct big_data_stats_event - big data stats event param
 * @vdev_id:               vdev id
 * @tsf_out_of_sync:       tsf out of sync
 * @ani_level:             ani level
 * @last_data_tx_pwr:  tx pwr last data frm
 * @target_power_dsss:  tx power dsss
 * @target_power_ofdm:  target power ofdm
 * @last_tx_data_rix:     rx lateset data frame
 * @last_tx_data_rate_kbps: tx latest data frame
 */
struct big_data_stats_event {
	uint32_t vdev_id;
	uint32_t tsf_out_of_sync;
	int32_t ani_level;
	uint32_t last_data_tx_pwr;
	uint32_t target_power_dsss;
	uint32_t target_power_ofdm;
	uint32_t last_tx_data_rix;
	uint32_t last_tx_data_rate_kbps;
};

/**
 * struct request_info: details of each request
 * @cookie: identifier for os_if request
 * @u: unified data type for callback to process tx power/peer rssi/
 *     station stats/mib stats request when response comes.
 * @vdev_id: vdev_id of request
 * @pdev_id: pdev_id of request
 * @peer_mac_addr: peer mac address
 */
struct request_info {
	void *cookie;
	union {
		void (*get_tx_power_cb)(int tx_power, void *cookie);
		void (*get_peer_rssi_cb)(struct stats_event *ev, void *cookie);
		void (*get_station_stats_cb)(struct stats_event *ev,
					     void *cookie);
		void (*get_mib_stats_cb)(struct stats_event *ev,
					 void *cookie);
		void (*get_peer_stats_cb)(struct stats_event *ev,
					  void *cookie);
#ifdef WLAN_FEATURE_BIG_DATA_STATS
		void (*get_big_data_stats_cb)(struct big_data_stats_event *ev,
					      void *cookie);
#endif
	} u;
	uint32_t vdev_id;
	uint32_t pdev_id;
	uint8_t peer_mac_addr[QDF_MAC_ADDR_SIZE];
};

/**
 * struct pending_stats_requests: details of pending requests
 * @type_map: map indicating type of outstanding requests
 * @req: array of info for outstanding request of each type
 */
struct pending_stats_requests {
	uint32_t type_map;
	struct request_info req[TYPE_MAX];
};

/**
 * struct cca_stats - cca stats
 * @congestion: the congestion percentage = (busy_time/total_time)*100
 *    for the interval from when the vdev was started to the current time
 *    (or the time at which the vdev was stopped).
 */
struct cca_stats {
	uint32_t congestion;
};

/**
 * struct psoc_mc_cp_stats: psoc specific stats
 * @is_cp_stats_suspended: is cp stats suspended or not
 * @pending: details of pending requests
 * @wow_unspecified_wake_up_count: number of non-wow related wake ups
 * @wow_stats: wake_lock stats for vdev
 */
struct psoc_mc_cp_stats {
	bool is_cp_stats_suspended;
	struct pending_stats_requests pending;
	uint32_t wow_unspecified_wake_up_count;
	struct wake_lock_stats wow_stats;
};

/**
 * struct pdev_mc_cp_stats: pdev specific stats
 * @max_pwr: max tx power for vdev
 */
struct pdev_mc_cp_stats {
	int32_t max_pwr;
};

/**
 * struct summary_stats - summary stats
 * @snr: snr of vdev
 * @rssi: rssi of vdev
 * @retry_cnt: retry count
 * @multiple_retry_cnt: multiple_retry_cnt
 * @tx_frm_cnt: num of tx frames
 * @rx_frm_cnt: num of rx frames
 * @frm_dup_cnt: duplicate frame count
 * @fail_cnt: fail count
 * @rts_fail_cnt: rts fail count
 * @ack_fail_cnt: ack fail count
 * @rts_succ_cnt: rts success count
 * @rx_discard_cnt: rx frames discarded
 * @rx_error_cnt: rx frames with error
 */
struct summary_stats {
	uint32_t snr;
	int8_t rssi;
	uint32_t retry_cnt[4];
	uint32_t multiple_retry_cnt[4];
	uint32_t tx_frm_cnt[4];
	uint32_t rx_frm_cnt;
	uint32_t frm_dup_cnt;
	uint32_t fail_cnt[4];
	uint32_t rts_fail_cnt;
	uint32_t ack_fail_cnt;
	uint32_t rts_succ_cnt;
	uint32_t rx_discard_cnt;
	uint32_t rx_error_cnt;
};

/**
 * struct pmf_bcn_protect_stats - pmf bcn protect stats param
 * @pmf_bcn_stats_valid: bcn protect stats received from fw are valid or not
 * @igtk_mic_fail_cnt: MIC failure count of management packets using IGTK
 * @igtk_replay_cnt: Replay detection count of management packets using IGTK
 * @bcn_mic_fail_cnt: MIC failure count of beacon packets using BIGTK
 * @bcn_replay_cnt: Replay detection count of beacon packets using BIGTK
 */
struct pmf_bcn_protect_stats {
	bool pmf_bcn_stats_valid;
	uint32_t igtk_mic_fail_cnt;
	uint32_t igtk_replay_cnt;
	uint32_t bcn_mic_fail_cnt;
	uint32_t bcn_replay_cnt;
};

/**
 * struct vdev_mc_cp_stats - vdev specific stats
 * @cca: cca stats
 * @tx_rate_flags: tx rate flags (enum tx_rate_info)
 * @chain_rssi: chain rssi
 * @vdev_summary_stats: vdev's summary stats
 * @pmf_bcn_stats: pmf beacon protect stats
 */
struct vdev_mc_cp_stats {
	struct cca_stats cca;
	uint32_t tx_rate_flags;
	int8_t chain_rssi[MAX_NUM_CHAINS];
	struct summary_stats vdev_summary_stats;
	struct pmf_bcn_protect_stats pmf_bcn_stats;
};

/**
 * struct peer_extd_stats - Peer extension statistics
 * @peer_macaddr: peer MAC address
 * @rx_duration: lower 32 bits of rx duration in microseconds
 * @peer_tx_bytes: Total TX bytes (including dot11 header) sent to peer
 * @peer_rx_bytes: Total RX bytes (including dot11 header) received from peer
 * @last_tx_rate_code: last TX ratecode
 * @last_tx_power: TX power used by peer - units are 0.5 dBm
 * @rx_mc_bc_cnt: Total number of received multicast & broadcast data frames
 * corresponding to this peer, 1 in the MSB of rx_mc_bc_cnt represents a
 * valid data
 */
struct peer_extd_stats {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint32_t rx_duration;
	uint32_t peer_tx_bytes;
	uint32_t peer_rx_bytes;
	uint32_t last_tx_rate_code;
	int32_t last_tx_power;
	uint32_t rx_mc_bc_cnt;
};

/**
 * struct peer_mc_cp_stats - peer specific stats
 * @tx_rate: tx rate
 * @rx_rate: rx rate
 * @peer_rssi: rssi
 * @peer_macaddr: mac address
 * @extd_stats: Pointer to peer extended stats
 * @adv_stats: Pointer to peer adv (extd2) stats
 * @twt_param: Pointer to peer twt session parameters
 */
struct peer_mc_cp_stats {
	uint32_t tx_rate;
	uint32_t rx_rate;
	int8_t peer_rssi;
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	struct peer_extd_stats *extd_stats;
	struct peer_adv_mc_cp_stats *adv_stats;
#ifdef WLAN_SUPPORT_TWT
	struct wmi_host_twt_session_stats_info twt_param[TWT_PEER_MAX_SESSIONS];
#endif
};

/**
 * struct peer_adv_mc_cp_stats - peer specific adv stats
 * @peer_macaddr: mac address
 * @fcs_count: fcs count
 * @rx_bytes: rx bytes
 * @rx_count: rx count
 */
struct peer_adv_mc_cp_stats {
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint32_t fcs_count;
	uint32_t rx_count;
	uint64_t rx_bytes;
};

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * struct dot11_counters - mib group containing attributes that are MAC counters
 * @tx_frags: successfully transmitted fragments
 * @group_tx_frames: transmitted group addressed frames
 * @failed_cnt: MSDUs not transmitted successfully
 * @rx_frags: fragments successfully received
 * @group_rx_frames: group addressed frames received
 * @fcs_error_cnt: FCS errors detected
 * @tx_frames: frames successfully transmitted
 */
struct dot11_counters {
	uint32_t tx_frags;
	uint32_t group_tx_frames;
	uint32_t failed_cnt;
	uint32_t rx_frags;
	uint32_t group_rx_frames;
	uint32_t fcs_error_cnt;
	uint32_t tx_frames;
};

/**
 * struct dot11_mac_statistics - mib stats information on the operation of MAC
 * @retry_cnt: retries done by mac for successful transmition
 * @multi_retry_cnt: multiple retries done before successful transmition
 * @frame_dup_cnt: duplicate no of frames
 * @rts_success_cnt: number of CTS received (in response to RTS)
 * @rts_fail_cnt: number of CTS not received (in response to RTS)
 * @tx_ack_fail_cnt: number of ACK not received
 */
struct dot11_mac_statistics {
	uint32_t retry_cnt;
	uint32_t multi_retry_cnt;
	uint32_t frame_dup_cnt;
	uint32_t rts_success_cnt;
	uint32_t rts_fail_cnt;
	uint32_t tx_ack_fail_cnt;
};

/**
 * dot11_qos_counters - qos mac counters
 * @qos_tx_frag_cnt: transmitted QoS fragments
 * @qos_failed_cnt: failed Qos fragments
 * @qos_retry_cnt: Qos frames transmitted after retransmissions
 * @qos_multi_retry_cnt: Qos frames transmitted after more than
 *                       one retransmissions
 * @qos_frame_dup_cnt: duplicate frames
 * @qos_rts_success_cnt: number of CTS received (in response to RTS)
 * @qos_rts_fail_cnt: number of CTS not received (in response to RTS)
 * @tx_qos_ack_fail_cnt_up: number of ACK not received
 *                          (in response to Qos frame)
 * @qos_rx_frag_cnt: number of received MPDU of type Data
 * @qos_tx_frame_cnt: number of transmitted MPDU of type Data
 * @qos_discarded_frame_cnt: total Discarded MSDUs
 * @qos_mpdu_rx_cnt: total received MPDU
 * @qos_retries_rx_cnt: received MPDU with retry bit equal to 1
 */
struct dot11_qos_counters {
	uint32_t qos_tx_frag_cnt;
	uint32_t qos_failed_cnt;
	uint32_t qos_retry_cnt;
	uint32_t qos_multi_retry_cnt;
	uint32_t qos_frame_dup_cnt;
	uint32_t qos_rts_success_cnt;
	uint32_t qos_rts_fail_cnt;
	uint32_t tx_qos_ack_fail_cnt_up;
	uint32_t qos_rx_frag_cnt;
	uint32_t qos_tx_frame_cnt;
	uint32_t qos_discarded_frame_cnt;
	uint32_t qos_mpdu_rx_cnt;
	uint32_t qos_retries_rx_cnt;
};

/**
 * dot11_rsna_stats - mib rsn stats
 * @rm_ccmp_replays: received robust management CCMP MPDUs discarded
 *                   by the replay mechanism
 * @tkip_icv_err: TKIP ICV errors encountered
 * @tkip_replays: TKIP replay errors detected
 * @ccmp_decrypt_err: MPDUs discarded by the CCMP decryption algorithm
 * @ccmp_replays: received CCMP MPDUs discarded by the replay mechanism
 * @cmac_icv_err: MPDUs discarded by the CMAC integrity check algorithm
 * @cmac_replays: MPDUs discarded by the CMAC replay errors
 */
struct dot11_rsna_stats {
	uint32_t rm_ccmp_replays;
	uint32_t tkip_icv_err;
	uint32_t tkip_replays;
	uint32_t ccmp_decrypt_err;
	uint32_t ccmp_replays;
	uint32_t cmac_icv_err;
	uint32_t cmac_replays;
};

/**
 * dot11_counters_group3 - dot11 group3 stats
 * @tx_ampdu_cnt: transmitted AMPDUs
 * @tx_mpdus_in_ampdu_cnt: number of MPDUs in the A-MPDU in transmitted AMPDUs
 * @tx_octets_in_ampdu_cnt: octets in the transmitted A-MPDUs
 * @ampdu_rx_cnt: received A-MPDU
 * @mpdu_in_rx_ampdu_cnt: MPDUs received in the A-MPDU
 * @rx_octets_in_ampdu_cnt: octets in the received A-MPDU
 * @rx_ampdu_deli_crc_err_cnt: number of MPDUs delimiter with CRC error
 */
struct dot11_counters_group3 {
	uint32_t tx_ampdu_cnt;
	uint32_t tx_mpdus_in_ampdu_cnt;
	uint64_t tx_octets_in_ampdu_cnt;
	uint32_t ampdu_rx_cnt;
	uint32_t mpdu_in_rx_ampdu_cnt;
	uint64_t rx_octets_in_ampdu_cnt;
	uint32_t rx_ampdu_deli_crc_err_cnt;
};

/**
 * mib_stats_metrics - mib stats counters
 * @mib_counters: dot11Counters group
 * @mib_mac_statistics: dot11MACStatistics group
 * @mib_qos_counters: dot11QoSCounters group
 * @mib_rsna_stats: dot11RSNAStats group
 * @mib_counters_group3: dot11CountersGroup3 group
 */
struct mib_stats_metrics {
	struct dot11_counters mib_counters;
	struct dot11_mac_statistics mib_mac_statistics;
	struct dot11_qos_counters mib_qos_counters;
	struct dot11_rsna_stats mib_rsna_stats;
	struct dot11_counters_group3 mib_counters_group3;
};
#endif

/**
 * struct congestion_stats_event: congestion stats event param
 * @vdev_id: vdev_id of the event
 * @congestion: the congestion percentage
 */
struct congestion_stats_event {
	uint8_t vdev_id;
	uint32_t congestion;
};

/**
 * struct summary_stats_event - summary_stats event param
 * @vdev_id: vdev_id of the event
 * @stats: summary stats
 */
struct summary_stats_event {
	uint8_t vdev_id;
	struct summary_stats stats;
};

/**
 * struct chain_rssi_event - chain_rssi event param
 * @vdev_id: vdev_id of the event
 * @chain_rssi: chain_rssi
 */
struct chain_rssi_event {
	uint8_t vdev_id;
	int8_t chain_rssi[MAX_NUM_CHAINS];
};

/**
 * struct peer_stats_info_ext_event - peer extended stats info
 * @peer_macaddr: MAC address
 * @tx_packets: packets transmitted to this station
 * @tx_bytes: bytes transmitted to this station
 * @rx_packets: packets received from this station
 * @rx_bytes: bytes received from this station
 * @tx_retries: cumulative retry counts
 * @tx_failed: the number of failed frames
 * @tx_succeed: the number of succeed frames
 * @rssi: the signal strength
 * @tx_rate: last used tx bitrate (kbps)
 * @tx_rate_code: last tx rate code (last_tx_rate_code of wmi_peer_stats_info)
 * @rx_rate: last used rx bitrate (kbps)
 * @rx_rate_code: last rx rate code (last_rx_rate_code of wmi_peer_stats_info)
 * @peer_rssi_per_chain: the average value of RSSI (dbm) per chain
 */
struct peer_stats_info_ext_event {
	struct qdf_mac_addr peer_macaddr;
	uint32_t tx_packets;
	uint64_t tx_bytes;
	uint32_t rx_packets;
	uint64_t rx_bytes;
	uint32_t tx_retries;
	uint32_t tx_failed;
	uint32_t tx_succeed;
	int32_t rssi;
	uint32_t tx_rate;
	uint32_t tx_rate_code;
	uint32_t rx_rate;
	uint32_t rx_rate_code;
	int32_t peer_rssi_per_chain[WMI_MAX_CHAINS];
};

/**
 * struct stats_event - parameters populated by stats event
 * @num_pdev_stats: num pdev stats
 * @pdev_stats: if populated array indicating pdev stats (index = pdev_id)
 * @num_peer_stats: num peer stats
 * @peer_stats: if populated array indicating peer stats
 * @peer_adv_stats: if populated, indicates peer adv (extd2) stats
 * @num_peer_adv_stats: number of peer adv (extd2) stats
 * @num_peer_extd_stats: Num peer extended stats
 * @peer_extended_stats: Peer extended stats
 * @cca_stats: if populated indicates congestion stats
 * @num_summary_stats: number of summary stats
 * @vdev_summary_stats: if populated indicates array of summary stats per vdev
 * @num_mib_stats: number of mib stats
 * @mib_stats: if populated indicates array of mib stats per vdev
 * @num_chain_rssi_stats: number of chain rssi stats
 * @vdev_chain_rssi: if populated indicates array of chain rssi per vdev
 * @tx_rate: tx rate (kbps)
 * @tx_rate_flags: tx rate flags, (enum tx_rate_info)
 * @last_event: The LSB indicates if the event is the last event or not and the
 *              MSB indicates if this feature is supported by FW or not.
 * @num_peer_stats_info_ext: number of peer extended stats info
 * @peer_stats_info_ext: peer extended stats info
 * @pmf_bcn_protect_stats: pmf bcn protect stats
 */
struct stats_event {
	uint32_t num_pdev_stats;
	struct pdev_mc_cp_stats *pdev_stats;
	uint32_t num_peer_stats;
	struct peer_mc_cp_stats *peer_stats;
	uint32_t num_peer_adv_stats;
	struct peer_adv_mc_cp_stats *peer_adv_stats;
	uint32_t num_peer_extd_stats;
	struct peer_extd_stats *peer_extended_stats;
	struct congestion_stats_event *cca_stats;
	uint32_t num_summary_stats;
	struct summary_stats_event *vdev_summary_stats;
#ifdef WLAN_FEATURE_MIB_STATS
	uint32_t num_mib_stats;
	struct mib_stats_metrics *mib_stats;
#endif
	uint32_t num_chain_rssi_stats;
	struct chain_rssi_event *vdev_chain_rssi;
	uint32_t tx_rate;
	uint32_t rx_rate;
	enum tx_rate_info tx_rate_flags;
	uint32_t last_event;
	uint32_t num_peer_stats_info_ext;
	struct peer_stats_info_ext_event *peer_stats_info_ext;
	struct pmf_bcn_protect_stats bcn_protect_stats;
};

/**
 * struct peer_stats_request_params - peer stats request parameter
 * @request_type: request type, one peer or all peers of the vdev
 * @vdev_id: vdev id
 * @peer_mac_addr: peer mac address, omitted if request type is all peers
 * @reset_after_request: whether reset stats after request
 */
struct peer_stats_request_params {
	uint32_t request_type;
	uint32_t vdev_id;
	uint8_t peer_mac_addr[QDF_MAC_ADDR_SIZE];
	uint32_t reset_after_request;
};

/**
 * struct wmi_host_peer_stats_info - WMI peer stats info
 * @peer_macaddr: peer mac address
 * @tx_bytes: tx_bytes
 * @tx_packets: tx packets
 * @rx_bytes: rx_bytes
 * @rx_packets: rx packets
 * @tx_retries: tx retries of MPDU
 * @tx_failed: tx failed MPDU
 * @last_tx_rate_code: rate code of the last tx
 * @last_rx_rate_code: rate code of the last rx
 * @last_tx_bitrate_kbps: bitrate in bps of the last tx
 * @last_rx_bitrate_kbps: bitrate in bps of the last rx
 * @peer_rssi: peer rssi
 * @tx_succeed: tx succeed MPDU
 * @peer_rssi_per_chain: peer rssi per chain
 */
typedef struct {
	struct qdf_mac_addr peer_macaddr;
	uint64_t tx_bytes;
	uint32_t tx_packets;
	uint64_t rx_bytes;
	uint32_t rx_packets;
	uint32_t tx_retries;
	uint32_t tx_failed;
	uint32_t last_tx_rate_code;
	uint32_t last_rx_rate_code;
	uint32_t last_tx_bitrate_kbps;
	uint32_t last_rx_bitrate_kbps;
	int32_t peer_rssi;
	uint32_t tx_succeed;
	int32_t peer_rssi_per_chain[WMI_MAX_CHAINS];
} wmi_host_peer_stats_info;

#endif /* __WLAN_CP_STATS_MC_DEFS_H__ */
