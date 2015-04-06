/*
 * Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
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

#ifndef _RX_DESC_H_
#define _RX_DESC_H_

/*
 * REMIND: Copy one of rx_desc related structures here for export,
 *         hopes they are always the same between Peregrine and Rome in future...
 */
struct rx_attention {
    volatile uint32_t first_mpdu                      :  1, //[0]
                      last_mpdu                       :  1, //[1]
                      mcast_bcast                     :  1, //[2]
                      peer_idx_invalid                :  1, //[3]
                      peer_idx_timeout                :  1, //[4]
                      power_mgmt                      :  1, //[5]
                      non_qos                         :  1, //[6]
                      null_data                       :  1, //[7]
                      mgmt_type                       :  1, //[8]
                      ctrl_type                       :  1, //[9]
                      more_data                       :  1, //[10]
                      eosp                            :  1, //[11]
                      u_apsd_trigger                  :  1, //[12]
                      fragment                        :  1, //[13]
                      order                           :  1, //[14]
                      classification                  :  1, //[15]
                      overflow_err                    :  1, //[16]
                      msdu_length_err                 :  1, //[17]
                      tcp_udp_chksum_fail             :  1, //[18]
                      ip_chksum_fail                  :  1, //[19]
                      sa_idx_invalid                  :  1, //[20]
                      da_idx_invalid                  :  1, //[21]
                      sa_idx_timeout                  :  1, //[22]
                      da_idx_timeout                  :  1, //[23]
                      encrypt_required                :  1, //[24]
                      directed                        :  1, //[25]
                      buffer_fragment                 :  1, //[26]
                      mpdu_length_err                 :  1, //[27]
                      tkip_mic_err                    :  1, //[28]
                      decrypt_err                     :  1, //[29]
                      fcs_err                         :  1, //[30]
                      msdu_done                       :  1; //[31]
};

struct rx_frag_info {
    volatile uint32_t ring0_more_count                :  8, //[7:0]
                      ring1_more_count                :  8, //[15:8]
                      ring2_more_count                :  8, //[23:16]
                      ring3_more_count                :  8; //[31:24]
};

struct rx_mpdu_start {
    volatile uint32_t peer_idx                        : 11, //[10:0]
                      fr_ds                           :  1, //[11]
                      to_ds                           :  1, //[12]
                      encrypted                       :  1, //[13]
                      retry                           :  1, //[14]
                      txbf_h_info                     :  1, //[15]
                      seq_num                         : 12, //[27:16]
                      encrypt_type                    :  4; //[31:28]
    volatile uint32_t pn_31_0                         : 32; //[31:0]
    volatile uint32_t pn_47_32                        : 16, //[15:0]
                      directed                        :  1, //[16]
                      reserved_2                      : 11, //[27:17]
                      tid                             :  4; //[31:28]
};

struct rx_msdu_start {
    volatile uint32_t msdu_length                     : 14, //[13:0]
                      ip_offset                       :  6, //[19:14]
                      ring_mask                       :  4, //[23:20]
                      tcp_udp_offset                  :  7, //[30:24]
                      reserved_0c                     :  1; //[31]
    volatile uint32_t flow_id_crc                     : 32; //[31:0]
    volatile uint32_t msdu_number                     :  8, //[7:0]
                      decap_format                    :  2, //[9:8]
                      ipv4_proto                      :  1, //[10]
                      ipv6_proto                      :  1, //[11]
                      tcp_proto                       :  1, //[12]
                      udp_proto                       :  1, //[13]
                      ip_frag                         :  1, //[14]
                      tcp_only_ack                    :  1, //[15]
                      sa_idx                          : 11, //[26:16]
                      reserved_2b                     :  5; //[31:27]
};

struct rx_msdu_end {
    volatile uint32_t ip_hdr_chksum                   : 16, //[15:0]
                      tcp_udp_chksum                  : 16; //[31:16]
    volatile uint32_t key_id_octet                    :  8, //[7:0]
                      classification_filter           :  8, //[15:8]
                      ext_wapi_pn_63_48               : 16; //[31:16]
    volatile uint32_t ext_wapi_pn_95_64               : 32; //[31:0]
    volatile uint32_t ext_wapi_pn_127_96              : 32; //[31:0]
    volatile uint32_t reported_mpdu_length            : 14, //[13:0]
                      first_msdu                      :  1, //[14]
                      last_msdu                       :  1, //[15]
                      reserved_3a                     : 14, //[29:16]
                      pre_delim_err                   :  1, //[30]
                      reserved_3b                     :  1; //[31]
};

struct rx_mpdu_end {
    volatile uint32_t reserved_0                      : 13, //[12:0]
                      overflow_err                    :  1, //[13]
                      last_mpdu                       :  1, //[14]
                      post_delim_err                  :  1, //[15]
                      post_delim_cnt                  : 12, //[27:16]
                      mpdu_length_err                 :  1, //[28]
                      tkip_mic_err                    :  1, //[29]
                      decrypt_err                     :  1, //[30]
                      fcs_err                         :  1; //[31]
};

struct rx_ppdu_start {
    volatile uint32_t rssi_chain0_pri20               :  8, //[7:0]
                      rssi_chain0_sec20               :  8, //[15:8]
                      rssi_chain0_sec40               :  8, //[23:16]
                      rssi_chain0_sec80               :  8; //[31:24]
    volatile uint32_t rssi_chain1_pri20               :  8, //[7:0]
                      rssi_chain1_sec20               :  8, //[15:8]
                      rssi_chain1_sec40               :  8, //[23:16]
                      rssi_chain1_sec80               :  8; //[31:24]
    volatile uint32_t rssi_chain2_pri20               :  8, //[7:0]
                      rssi_chain2_sec20               :  8, //[15:8]
                      rssi_chain2_sec40               :  8, //[23:16]
                      rssi_chain2_sec80               :  8; //[31:24]
    volatile uint32_t rssi_chain3_pri20               :  8, //[7:0]
                      rssi_chain3_sec20               :  8, //[15:8]
                      rssi_chain3_sec40               :  8, //[23:16]
                      rssi_chain3_sec80               :  8; //[31:24]
    volatile uint32_t rssi_comb                       :  8, //[7:0]
                      reserved_4a                     : 16, //[23:8]
                      is_greenfield                   :  1, //[24]
                      reserved_4b                     :  7; //[31:25]
    volatile uint32_t l_sig_rate                      :  4, //[3:0]
                      l_sig_rate_select               :  1, //[4]
                      l_sig_length                    : 12, //[16:5]
                      l_sig_parity                    :  1, //[17]
                      l_sig_tail                      :  6, //[23:18]
                      preamble_type                   :  8; //[31:24]
    volatile uint32_t ht_sig_vht_sig_a_1              : 24, //[23:0]
                      reserved_6                      :  8; //[31:24]
    volatile uint32_t ht_sig_vht_sig_a_2              : 24, //[23:0]
                      txbf_h_info                     :  1, //[24]
                      reserved_7                      :  7; //[31:25]
    volatile uint32_t vht_sig_b                       : 29, //[28:0]
                      reserved_8                      :  3; //[31:29]
    volatile uint32_t service                         : 16, //[15:0]
                      reserved_9                      : 16; //[31:16]
};

struct rx_ppdu_end {
    volatile uint32_t evm_p0                          : 32; //[31:0]
    volatile uint32_t evm_p1                          : 32; //[31:0]
    volatile uint32_t evm_p2                          : 32; //[31:0]
    volatile uint32_t evm_p3                          : 32; //[31:0]
    volatile uint32_t evm_p4                          : 32; //[31:0]
    volatile uint32_t evm_p5                          : 32; //[31:0]
    volatile uint32_t evm_p6                          : 32; //[31:0]
    volatile uint32_t evm_p7                          : 32; //[31:0]
    volatile uint32_t evm_p8                          : 32; //[31:0]
    volatile uint32_t evm_p9                          : 32; //[31:0]
    volatile uint32_t evm_p10                         : 32; //[31:0]
    volatile uint32_t evm_p11                         : 32; //[31:0]
    volatile uint32_t evm_p12                         : 32; //[31:0]
    volatile uint32_t evm_p13                         : 32; //[31:0]
    volatile uint32_t evm_p14                         : 32; //[31:0]
    volatile uint32_t evm_p15                         : 32; //[31:0]
    volatile uint32_t tsf_timestamp                   : 32; //[31:0]
    volatile uint32_t wb_timestamp                    : 32; //[31:0]
    volatile uint32_t locationing_timestamp           :  8, //[7:0]
                      phy_err_code                    :  8, //[15:8]
                      phy_err                         :  1, //[16]
                      rx_location                     :  1, //[17]
                      txbf_h_info                     :  1, //[18]
                      reserved_18                     : 13; //[31:19]
    volatile uint32_t rx_antenna                      : 24, //[23:0]
                      tx_ht_vht_ack                   :  1, //[24]
                      bb_captured_channel             :  1, //[25]
                      reserved_19                     :  6; //[31:26]
    volatile uint32_t rtt_correction_value            : 24, //[23:0]
                      reserved_20                     :  7, //[30:24]
                      rtt_normal_mode                 :  1; //[31]
    volatile uint32_t bb_length                       : 16, //[15:0]
                      reserved_21                     : 15, //[30:16]
                      ppdu_done                       :  1; //[31]
};

#endif /*_RX_DESC_H_*/
