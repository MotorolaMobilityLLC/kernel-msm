/*
 * Copyright (c) 2011-2015, 2017 The Linux Foundation. All rights reserved.
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

#ifndef _RX_DESC_H_
#define _RX_DESC_H_

/*
 * REMIND: Copy one of rx_desc related structures here for export,
 *         hopes they are always the same between Peregrine and Rome in future
 */
struct rx_attention {
	volatile
	uint32_t first_mpdu:1, /* [0] */
		last_mpdu:1, /* [1] */
		mcast_bcast:1, /* [2] */
		peer_idx_invalid:1, /* [3] */
		peer_idx_timeout:1, /* [4] */
		power_mgmt:1, /* [5] */
		non_qos:1, /* [6] */
		null_data:1, /* [7] */
		mgmt_type:1, /* [8] */
		ctrl_type:1, /* [9] */
		more_data:1, /* [10] */
		eosp:1, /* [11] */
		u_apsd_trigger:1, /* [12] */
		fragment:1, /* [13] */
		order:1, /* [14] */
		classification:1, /* [15] */
		overflow_err:1, /* [16] */
		msdu_length_err:1, /* [17] */
		tcp_udp_chksum_fail:1, /* [18] */
		ip_chksum_fail:1, /* [19] */
		sa_idx_invalid:1, /* [20] */
		da_idx_invalid:1, /* [21] */
		sa_idx_timeout:1, /* [22] */
		da_idx_timeout:1, /* [23] */
		encrypt_required:1, /* [24] */
		directed:1, /* [25] */
		buffer_fragment:1, /* [26] */
		mpdu_length_err:1, /* [27] */
		tkip_mic_err:1, /* [28] */
		decrypt_err:1, /* [29] */
		fcs_err:1, /* [30] */
		msdu_done:1; /* [31] */
};

struct rx_frag_info {
	volatile
	uint32_t ring0_more_count:8,   /* [7:0] */
		ring1_more_count:8, /* [15:8] */
		ring2_more_count:8, /* [23:16] */
		ring3_more_count:8; /* [31:24] */
	volatile
	uint32_t ring4_more_count:8, /* [7:0] */
		ring5_more_count:8, /* [15:8] */
		ring6_more_count:8, /* [23:16] */
		ring7_more_count:8; /* [31:24] */
};

struct rx_msdu_start {
	volatile
	uint32_t msdu_length:14, /* [13:0] */
#if defined(HELIUMPLUS)
		l3_offset:7, /* [20:14] */
		ipsec_ah:1, /* [21] */
		reserved_0a:2, /* [23:22] */
		l4_offset:7, /* [30:24] */
		ipsec_esp:1; /* [31] */
#else
		ip_offset:6, /* [19:14] */
		ring_mask:4, /* [23:20] */
		tcp_udp_offset:7, /* [30:24] */
		reserved_0c:1; /* [31] */
#endif /* defined(HELIUMPLUS) */
#if defined(HELIUMPLUS)
	volatile uint32_t flow_id_toeplitz:32; /* [31:0] */
#else
	volatile uint32_t flow_id_crc:32; /* [31:0] */
#endif /* defined(HELIUMPLUS) */
	volatile
	uint32_t msdu_number:8, /* [7:0] */
		decap_format:2, /* [9:8] */
		ipv4_proto:1, /* [10] */
		ipv6_proto:1, /* [11] */
		tcp_proto:1, /* [12] */
		udp_proto:1, /* [13] */
		ip_frag:1, /* [14] */
		tcp_only_ack:1, /* [15] */
		sa_idx:11, /* [26:16] */
		reserved_2b:5; /* [31:27] */
#if defined(HELIUMPLUS)
	volatile
	uint32_t da_idx:11, /* [10:0] */
		da_is_bcast_mcast:1, /* [11] */
		reserved_3a:4, /* [15:12] */
		ip4_protocol_ip6_next_header:8, /* [23:16] */
		ring_mask:8; /* [31:24] */
	volatile uint32_t toeplitz_hash_2_or_4:32; /* [31:0] */
#endif /* defined(HELIUMPLUS) */
};

struct rx_msdu_end {
	volatile
	uint32_t ip_hdr_chksum:16, /* [15:0] */
		tcp_udp_chksum:16; /* [31:16] */
	volatile
	uint32_t key_id_octet:8, /* [7:0] */
#if defined(HELIUMPLUS)
		classification_rule:6, /* [13:8] */
		classify_not_done_truncate:1, /* [14] */
		classify_not_done_cce_dis:1, /* [15] */
#else
		classification_filter:8, /* [15:8] */
#endif /* defined(HELIUMPLUS) */
	ext_wapi_pn_63_48:16; /* [31:16] */
	volatile uint32_t ext_wapi_pn_95_64:32; /* [31:0] */
	volatile uint32_t ext_wapi_pn_127_96:32; /* [31:0] */
	volatile
	uint32_t reported_mpdu_length:14, /* [13:0] */
		first_msdu:1, /* [14] */
		last_msdu:1, /* [15] */
#if defined(HELIUMPLUS)
		sa_idx_timeout:1, /* [16] */
		da_idx_timeout:1, /* [17] */
		msdu_limit_error:1, /* [18] */
		classify_ring_mask:8, /* [26:19] */
#endif /* defined(HELIUMPLUS) */
		reserved_3a:3, /* [29:27] */
		pre_delim_err:1, /* [30] */
		reserved_3b:1; /* [31] */
#if defined(HELIUMPLUS)
	volatile uint32_t ipv6_options_crc:32;
	volatile uint32_t tcp_seq_number:32;
	volatile uint32_t tcp_ack_number:32;
	volatile
	uint32_t tcp_flag:9, /* [8:0] */
		lro_eligible:1, /* [9] */
		l3_header_padding:3, /* [12:10] */
		reserved_8a:3, /* [15:13] */
		window_size:16; /* [31:16] */
	volatile
	uint32_t da_offset:6, /* [5:0] */
		sa_offset:6, /* [11:6] */
		da_offset_valid:1, /* [12] */
		sa_offset_valid:1, /* [13] */
		type_offset:7, /* [20:14] */
		reserved_9a:11; /* [31:21] */
	volatile uint32_t rule_indication_31_0:32;
	volatile uint32_t rule_indication_63_32:32;
	volatile uint32_t rule_indication_95_64:32;
	volatile uint32_t rule_indication_127_96:32;
#endif /* defined(HELIUMPLUS) */
};

struct rx_mpdu_end {
	volatile
	uint32_t reserved_0:13, /* [12:0] */
		overflow_err:1, /* [13] */
		last_mpdu:1, /* [14] */
		post_delim_err:1, /* [15] */
		post_delim_cnt:12, /* [27:16] */
		mpdu_length_err:1, /* [28] */
		tkip_mic_err:1, /* [29] */
		decrypt_err:1, /* [30] */
		fcs_err:1; /* [31] */
};


#if defined(HELIUMPLUS)

struct rx_mpdu_start {
	volatile
	uint32_t peer_idx:11, /* [10:0] */
		fr_ds:1, /* [11] */
		to_ds:1, /* [12] */
		encrypted:1, /* [13] */
		retry:1, /* [14] */
		reserved:1, /* [15] */
		seq_num:12, /* [27:16] */
		encrypt_type:4; /* [31:28] */
	volatile uint32_t pn_31_0:32; /* [31:0] */
	volatile
	uint32_t pn_47_32:16, /* [15:0] */
		toeplitz_hash:2, /* [17:16] */
		reserved_2:10, /* [27:18] */
		tid:4; /* [31:28] */
};


struct rx_ppdu_start {
	volatile
	uint32_t rssi_pri_chain0:8, /* [7:0] */
		rssi_sec20_chain0:8, /* [15:8] */
		rssi_sec40_chain0:8, /* [23:16] */
		rssi_sec80_chain0:8; /* [31:24] */
	volatile
	uint32_t rssi_pri_chain1:8, /* [7:0] */
		rssi_sec20_chain1:8, /* [15:8] */
		rssi_sec40_chain1:8, /* [23:16] */
		rssi_sec80_chain1:8; /* [31:24] */
	volatile
	uint32_t rssi_pri_chain2:8, /* [7:0] */
		rssi_sec20_chain2:8, /* [15:8] */
		rssi_sec40_chain2:8, /* [23:16] */
		rssi_sec80_chain2:8; /* [31:24] */
	volatile
	uint32_t rssi_pri_chain3:8, /* [7:0] */
		rssi_sec20_chain3:8, /* [15:8] */
		rssi_sec40_chain3:8, /* [23:16] */
		rssi_sec80_chain3:8; /* [31:24] */
	volatile
	uint32_t rssi_comb:8, /* [7:0] */
		bandwidth:3, /* [10:8] */
		reserved_4a:5, /* [15:11] */
		rssi_comb_ht:8, /* [23:16] */
		reserved_4b:8; /* [31:24] */
	volatile
	uint32_t l_sig_rate:4, /*[3:0] */
		l_sig_rate_select:1, /* [4] */
		l_sig_length:12, /* [16:5] */
		l_sig_parity:1, /* [17] */
		l_sig_tail:6, /* [23:18] */
		preamble_type:8; /* [31:24] */
	volatile
	uint32_t ht_sig_vht_sig_ah_sig_a_1:24, /* [23:0] */
		captured_implicit_sounding:1, /* [24] */
		reserved_6:7; /* [31:25] */
	volatile
	uint32_t ht_sig_vht_sig_ah_sig_a_2:24, /* [23:0] */
		reserved_7:8; /* [31:24] */
	volatile uint32_t vht_sig_b:32; /* [31:0] */
	volatile
	uint32_t service:16, /* [15:0] */
		reserved_9:16; /* [31:16] */
};

#define VHT_SIG_A_1(rx_desc) ((rx_desc)->ppdu_start.ht_sig_vht_sig_ah_sig_a_1)
#define VHT_SIG_A_2(rx_desc) ((rx_desc)->ppdu_start.ht_sig_vht_sig_ah_sig_a_2)
#define TSF_TIMESTAMP(rx_desc) \
((rx_desc)->ppdu_end.rx_pkt_end.phy_timestamp_1_lower_32)

struct rx_location_info {
	volatile
	uint32_t rtt_fac_legacy:14, /* [13:0] */
		rtt_fac_legacy_status:1, /* [14] */
		rtt_fac_vht:14, /* [28:15] */
		rtt_fac_vht_status:1, /* [29] */
		rtt_cfr_status:1, /* [30] */
		rtt_cir_status:1; /* [31] */
	volatile
	uint32_t rtt_fac_sifs:10, /* [9:0] */
		rtt_fac_sifs_status:2, /* [11:10] */
		rtt_channel_dump_size:11, /* [22:12] */
		rtt_mac_phy_phase:2, /* [24:23] */
		rtt_hw_ifft_mode:1, /* [25] */
		rtt_btcf_status:1, /* [26] */
		rtt_preamble_type:2, /* [28:27] */
		rtt_pkt_bw:2, /* [30:29] */
		rtt_gi_type:1; /* [31] */
	volatile
	uint32_t rtt_mcs_rate:4, /* [3:0] */
		rtt_strongest_chain:2, /* [5:4] */
		rtt_phase_jump:7, /* [12:6] */
		rtt_rx_chain_mask:4, /* [16:13] */
		rtt_tx_data_start_x_phase:1, /* [17] */
		reserved_2:13, /* [30:18] */
		rx_location_info_valid:1; /* [31] */
};

struct rx_pkt_end {
	volatile
	uint32_t rx_success:1, /* [0] */
		reserved_0a:2, /* [2:1] */
		error_tx_interrupt_rx:1, /* [3] */
		error_ofdm_power_drop:1, /* [4] */
		error_ofdm_restart:1, /* [5] */
		error_cck_power_drop:1, /* [6] */
		error_cck_restart:1, /* [7] */
		reserved_0b:24; /* [31:8] */
	volatile uint32_t phy_timestamp_1_lower_32:32; /* [31:0] */
	volatile uint32_t phy_timestamp_1_upper_32:32; /* [31:0] */
	volatile uint32_t phy_timestamp_2_lower_32:32; /* [31:0] */
	volatile uint32_t phy_timestamp_2_upper_32:32; /* [31:0] */
	struct rx_location_info rx_location_info;
};

struct rx_phy_ppdu_end {
	volatile
	uint32_t reserved_0a:2, /* [1:0] */
		error_radar:1, /* [2] */
		error_rx_abort:1, /* [3] */
		error_rx_nap:1, /* [4] */
		error_ofdm_timing:1, /* [5] */
		error_ofdm_signal_parity:1, /* [6] */
		error_ofdm_rate_illegal:1, /* [7] */
		error_ofdm_length_illegal:1, /* [8] */
		error_ppdu_ofdm_restart:1, /* [9] */
		error_ofdm_service:1, /* [10] */
		error_ppdu_ofdm_power_drop:1, /* [11] */
		error_cck_blocker:1, /* [12] */
		error_cck_timing:1, /* [13] */
		error_cck_header_crc:1, /* [14] */
		error_cck_rate_illegal:1, /* [15] */
		error_cck_length_illegal:1, /* [16] */
		error_ppdu_cck_restart:1, /* [17] */
		error_cck_service:1, /* [18] */
		error_ppdu_cck_power_drop:1, /* [19] */
		error_ht_crc_err:1, /* [20] */
		error_ht_length_illegal:1, /* [21] */
		error_ht_rate_illegal:1, /* [22] */
		error_ht_zlf:1, /* [23] */
		error_false_radar_ext:1, /* [24] */
		error_green_field:1, /* [25] */
		error_spectral_scan:1, /* [26] */
		error_rx_bw_gt_dyn_bw:1, /* [27] */
		error_leg_ht_mismatch:1, /* [28] */
		error_vht_crc_error:1, /* [29] */
		error_vht_siga_unsupported:1, /* [30] */
		error_vht_lsig_len_invalid:1; /* [31] */
	volatile
	uint32_t error_vht_ndp_or_zlf:1, /* [0] */
		error_vht_nsym_lt_zero:1, /* [1] */
		error_vht_rx_extra_symbol_mismatch:1, /* [2] */
		error_vht_rx_skip_group_id0:1, /* [3] */
		error_vht_rx_skip_group_id1to62:1, /* [4] */
		error_vht_rx_skip_group_id63:1, /* [5] */
		error_ofdm_ldpc_decoder_disabled:1, /* [6] */
		error_defer_nap:1, /* [7] */
		error_fdomain_timeout:1, /* [8] */
		error_lsig_rel_check:1, /* [9] */
		error_bt_collision:1, /* [10] */
		error_unsupported_mu_feedback:1, /* [11] */
		error_ppdu_tx_interrupt_rx:1, /* [12] */
		error_rx_unsupported_cbf:1, /* [13] */
		reserved_1:18; /* [31:14] */
};

struct rx_timing_offset {
	volatile
	uint32_t timing_offset:12, /* [11:0] */
		reserved:20; /* [31:12] */
};

struct rx_ppdu_end {
	volatile uint32_t evm_p0:32;
	volatile uint32_t evm_p1:32;
	volatile uint32_t evm_p2:32;
	volatile uint32_t evm_p3:32;
	volatile uint32_t evm_p4:32;
	volatile uint32_t evm_p5:32;
	volatile uint32_t evm_p6:32;
	volatile uint32_t evm_p7:32;
	volatile uint32_t evm_p8:32;
	volatile uint32_t evm_p9:32;
	volatile uint32_t evm_p10:32;
	volatile uint32_t evm_p11:32;
	volatile uint32_t evm_p12:32;
	volatile uint32_t evm_p13:32;
	volatile uint32_t evm_p14:32;
	volatile uint32_t evm_p15:32;
	volatile uint32_t reserved_16:32;
	volatile uint32_t reserved_17:32;
	volatile uint32_t wb_timestamp_lower_32:32;
	volatile uint32_t wb_timestamp_upper_32:32;
	struct rx_pkt_end rx_pkt_end;
	struct rx_phy_ppdu_end rx_phy_ppdu_end;
	struct rx_timing_offset rx_timing_offset;
	volatile
	uint32_t rx_antenna:24, /* [23:0] */
		tx_ht_vht_ack:1, /* [24] */
		rx_pkt_end_valid:1, /* [25] */
		rx_phy_ppdu_end_valid:1, /* [26] */
		rx_timing_offset_valid:1, /* [27] */
		bb_captured_channel:1, /* [28] */
		unsupported_mu_nc:1, /* [29] */
		otp_txbf_disable:1, /* [30] */
		reserved_31:1; /* [31] */
	volatile
	uint32_t coex_bt_tx_from_start_of_rx:1, /* [0] */
		coex_bt_tx_after_start_of_rx:1, /* [1] */
		coex_wan_tx_from_start_of_rx:1, /* [2] */
		coex_wan_tx_after_start_of_rx:1, /* [3] */
		coex_wlan_tx_from_start_of_rx:1, /* [4] */
		coex_wlan_tx_after_start_of_rx:1, /* [5] */
		mpdu_delimiter_errors_seen:1, /* [6] */
		ftm:1, /* [7] */
		ftm_dialog_token:8, /* [15:8] */
		ftm_follow_up_dialog_token:8, /* [23:16] */
		reserved_32:8; /* [31:24] */
	volatile
	uint32_t before_mpdu_cnt_passing_fcs:8, /* [7:0] */
		before_mpdu_cnt_failing_fcs:8, /* [15:8] */
		after_mpdu_cnt_passing_fcs:8, /* [23:16] */
		after_mpdu_cnt_failing_fcs:8; /* [31:24] */
	volatile uint32_t phy_timestamp_tx_lower_32:32; /* [31:0] */
	volatile uint32_t phy_timestamp_tx_upper_32:32; /* [31:0] */
	volatile
	uint32_t bb_length:16, /* [15:0] */
		bb_data:1, /* [16] */
		peer_idx_valid:1, /* [17] */
		peer_idx:11, /* [28:18] */
		reserved_26:2, /* [30:29] */
		ppdu_done:1; /* [31] */
};
#else
struct rx_ppdu_start {
	volatile
	uint32_t rssi_chain0_pri20:8, /* [7:0] */
		rssi_chain0_sec20:8, /* [15:8] */
		rssi_chain0_sec40:8, /* [23:16] */
		rssi_chain0_sec80:8; /* [31:24] */
	volatile
	uint32_t rssi_chain1_pri20:8, /* [7:0] */
		rssi_chain1_sec20:8, /* [15:8] */
		rssi_chain1_sec40:8, /* [23:16] */
		rssi_chain1_sec80:8; /* [31:24] */
	volatile
	uint32_t rssi_chain2_pri20:8, /* [7:0] */
		rssi_chain2_sec20:8, /* [15:8] */
		rssi_chain2_sec40:8, /* [23:16] */
		rssi_chain2_sec80:8; /* [31:24] */
	volatile
	uint32_t rssi_chain3_pri20:8, /* [7:0] */
		rssi_chain3_sec20:8, /* [15:8] */
		rssi_chain3_sec40:8, /* [23:16] */
		rssi_chain3_sec80:8; /* [31:24] */
	volatile
	uint32_t rssi_comb:8,  /* [7:0] */
		reserved_4a:16, /* [23:8] */
		is_greenfield:1, /* [24] */
		reserved_4b:7; /* [31:25] */
	volatile
	uint32_t l_sig_rate:4, /* [3:0] */
		l_sig_rate_select:1, /* [4] */
		l_sig_length:12, /* [16:5] */
		l_sig_parity:1, /* [17] */
		l_sig_tail:6, /* [23:18] */
		preamble_type:8; /* [31:24] */
	volatile
	uint32_t ht_sig_vht_sig_a_1:24, /* [23:0] */
		reserved_6:8; /* [31:24] */
	volatile
	uint32_t ht_sig_vht_sig_a_2:24, /* [23:0] */
		txbf_h_info:1, /* [24] */
		reserved_7:7; /* [31:25] */
	volatile
	uint32_t vht_sig_b:29, /* [28:0] */
		reserved_8:3; /* [31:29] */
	volatile
	uint32_t service:16,   /* [15:0] */
		reserved_9:16; /* [31:16] */
};

#define VHT_SIG_A_1(rx_desc) ((rx_desc)->ppdu_start.ht_sig_vht_sig_a_1)
#define VHT_SIG_A_2(rx_desc) ((rx_desc)->ppdu_start.ht_sig_vht_sig_a_2)

#define TSF_TIMESTAMP(rx_desc) ((rx_desc)->ppdu_end.tsf_timestamp)

struct rx_mpdu_start {
	volatile
	uint32_t peer_idx:11,  /* [10:0] */
		fr_ds:1, /* [11] */
		to_ds:1, /* [12] */
		encrypted:1, /* [13] */
		retry:1, /* [14] */
		txbf_h_info:1, /* [15] */
		seq_num:12, /* [27:16] */
		encrypt_type:4; /* [31:28] */
	volatile uint32_t pn_31_0:32;   /* [31:0] */
	volatile
	uint32_t pn_47_32:16,  /* [15:0] */
		directed:1, /* [16] */
		reserved_2:11, /* [27:17] */
		tid:4; /* [31:28] */
};

struct rx_ppdu_end {
	volatile uint32_t evm_p0:32;    /* [31:0] */
	volatile uint32_t evm_p1:32;    /* [31:0] */
	volatile uint32_t evm_p2:32;    /* [31:0] */
	volatile uint32_t evm_p3:32;    /* [31:0] */
	volatile uint32_t evm_p4:32;    /* [31:0] */
	volatile uint32_t evm_p5:32;    /* [31:0] */
	volatile uint32_t evm_p6:32;    /* [31:0] */
	volatile uint32_t evm_p7:32;    /* [31:0] */
	volatile uint32_t evm_p8:32;    /* [31:0] */
	volatile uint32_t evm_p9:32;    /* [31:0] */
	volatile uint32_t evm_p10:32;   /* [31:0] */
	volatile uint32_t evm_p11:32;   /* [31:0] */
	volatile uint32_t evm_p12:32;   /* [31:0] */
	volatile uint32_t evm_p13:32;   /* [31:0] */
	volatile uint32_t evm_p14:32;   /* [31:0] */
	volatile uint32_t evm_p15:32;   /* [31:0] */
	volatile uint32_t tsf_timestamp:32; /* [31:0] */
	volatile uint32_t wb_timestamp:32; /* [31:0] */
	volatile
	uint32_t locationing_timestamp:8, /* [7:0] */
		phy_err_code:8, /* [15:8] */
		phy_err:1, /* [16] */
		rx_location:1, /* [17] */
		txbf_h_info:1, /* [18] */
		reserved_18:13; /* [31:19] */
	volatile
	uint32_t rx_antenna:24, /* [23:0] */
		tx_ht_vht_ack:1, /* [24] */
		bb_captured_channel:1, /* [25] */
		reserved_19:6; /* [31:26] */
	volatile
	uint32_t rtt_correction_value:24, /* [23:0] */
		reserved_20:7, /* [30:24] */
		rtt_normal_mode:1; /* [31] */
	volatile
	uint32_t bb_length:16, /* [15:0] */
		reserved_21:15, /* [30:16] */
		ppdu_done:1; /* [31] */
};
#endif /* defined(HELIUMPLUS) */

#endif /*_RX_DESC_H_*/
