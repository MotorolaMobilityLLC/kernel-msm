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
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_HE_CAPS_H
#define __CFG_MLME_HE_CAPS_H

#define CFG_HE_CONTROL CFG_BOOL( \
				"he_control", \
				0, \
				"HE Control")

#define CFG_HE_FRAGMENTATION CFG_UINT( \
				"he_fragmentation", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Fragmentation")

#define CFG_HE_MAX_FRAG_MSDU CFG_UINT( \
				"he_max_frag_msdu", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Max Frag Msdu")

#define CFG_HE_MIN_FRAG_SIZE CFG_UINT( \
				"he_min_frag_size", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Min Frag Size")

#define CFG_HE_TRIG_PAD CFG_UINT( \
				"he_trig_pad", \
				0, \
				2, \
				2, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Trig Pad")

#define CFG_HE_MTID_AGGR_RX CFG_UINT( \
				"he_mtid_aggr_rx", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Mtid Aggr")

#define CFG_HE_LINK_ADAPTATION CFG_UINT( \
				"he_link_adaptation", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Link Adaptation")

#define CFG_HE_ALL_ACK CFG_BOOL( \
				"he_all_ack", \
				0, \
				"HE All Ack")

#define CFG_HE_TRIGD_RSP_SCHEDULING CFG_BOOL( \
				"he_trigd_rsp_scheduling", \
				0, \
				"HE Trigd Rsp Scheduling")

#define CFG_HE_BUFFER_STATUS_RPT CFG_BOOL( \
				"he_buffer_status_rpt", \
				0, \
				"HE Buffer Status Rpt")

#define CFG_HE_BA_32BIT CFG_BOOL( \
				"he_ba_32bit", \
				0, \
				"HE BA 32Bit")

#define CFG_HE_MU_CASCADING CFG_BOOL( \
				"he_mu_cascading", \
				0, \
				"HE Mu Cascading")

#define CFG_HE_MULTI_TID CFG_BOOL( \
				"he_multi_tid", \
				0, \
				"HE Multi Tid")

#define CFG_HE_OMI CFG_BOOL( \
				"he_omi", \
				0, \
				"HE Omi")

#define CFG_HE_OFDMA_RA CFG_BOOL( \
				"he_ofdma_ra", \
				0, \
				"HE Ofdma Ra")

#define CFG_HE_MAX_AMPDU_LEN CFG_UINT( \
				"he_max_ampdu_len", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Max Ampdu Len")

#define CFG_HE_AMSDU_FRAG CFG_BOOL( \
				"he_amspdu_frag", \
				0, \
				"HE Amsdu Frag")

#define CFG_HE_FLEX_TWT_SCHED CFG_BOOL( \
				"he_flex_twt_sched", \
				0, \
				"HE Flex Twt Sched")

#define CFG_HE_RX_CTRL CFG_BOOL( \
				"he_rx_ctrl", \
				0, \
				"HE Rx Ctrl")

#define CFG_HE_BSRP_AMPDU_AGGR CFG_BOOL( \
				"he_bsrp_ampdu_aggr", \
				0, \
				"He Bspr Ampdu Aggr")

#define CFG_HE_QTP CFG_BOOL( \
				"he_qtp", \
				0, \
				"He Qtp")

#define CFG_HE_A_BQR CFG_BOOL( \
				"he_a_bqr", \
				0, \
				"He A Bqr")

#define CFG_HE_SR_RESPONDER CFG_BOOL( \
				"he_sr_responder", \
				0, \
				"He Sr Responder")

#define CFG_HE_NDP_FEEDBACK_SUPP CFG_BOOL( \
				"he_ndp_feedback_supp", \
				0, \
				"He Ndp Feedback Supp")

#define CFG_HE_OPS_SUPP CFG_BOOL( \
				"he_ops_supp", \
				0, \
				"He Ops Supp")

#define CFG_HE_AMSDU_IN_AMPDU CFG_BOOL( \
				"he_amsdu_in_ampdu", \
				0, \
				"He Amsdu In Ampdu")

#define CFG_HE_MTID_AGGR_TX CFG_UINT( \
				"he_mtid_aggr_tx", \
				0, \
				0x7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He MTid Aggr Tx")

#define CFG_HE_SUB_CH_SEL_TX CFG_BOOL( \
				"he_sub_ch_sel_tx", \
				0, \
				"He Sub cg sel tx")

#define CFG_HE_UL_2X996_RU CFG_BOOL( \
				"he_ul_2x996_ru", \
				0, \
				"He Ul 2x996 Ru")

#define CFG_HE_OM_CTRL_UL_MU_DIS_RX CFG_BOOL( \
				"he_om_ctrl_ul_mu_dis_rx", \
				0, \
				"He Om Ctrl Ul My Dis Rx")

#define CFG_HE_DYNAMIC_SMPS CFG_BOOL( \
				"he_dynamic_smps", \
				0, \
				"He Dyanmic SMPS")

#define CFG_HE_PUNCTURED_SOUNDING CFG_BOOL( \
				"he_punctured_sounding", \
				0, \
				"He Punctured Sounding")

#define CFG_HE_HT_VHT_TRG_FRM_RX CFG_BOOL( \
				"ht_vht_trg_frm_rx", \
				0, \
				"HT VHT Trigger frame Rx")

#define CFG_HE_CHAN_WIDTH CFG_UINT( \
				"he_chan_width", \
				0, \
				0x3F, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Chan Width")

#define CFG_HE_RX_PREAM_PUNC CFG_UINT( \
				"he_rx_pream_punc", \
				0, \
				0xF, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Rx Pream Punc")

#define CFG_HE_CLASS_OF_DEVICE CFG_BOOL( \
				"he_class_of_device", \
				0, \
				"He Class Of Device")

#define CFG_HE_LDPC CFG_BOOL( \
				"he_ldpc", \
				0, \
				"He Ldpc")

#define CFG_HE_LTF_PPDU CFG_UINT( \
				"he_ltf_ppdu", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Ltf Ppdu")

#define CFG_HE_MIDAMBLE_RX_MAX_NSTS CFG_UINT( \
				"he_midamble_rx_max_nsts", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Midamble Rx Max Nsts")

#define CFG_HE_LTF_NDP CFG_UINT( \
				"he_ltf_ndp", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Ltf Ndp")

#define CFG_HE_TX_STBC_LT80 CFG_BOOL( \
				"he_tx_stbc_lt80_sta", \
				0, \
				"He Tx Stbc Lt80")

#define CFG_HE_RX_STBC_LT80 CFG_BOOL( \
				"he_rx_stbc_lt80", \
				0, \
				"He Rx Stbc Lt80")

#define CFG_HE_DOPPLER CFG_UINT( \
				"he_doppler", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Doppler")

#define CFG_HE_DCM_TX CFG_UINT( \
				"he_dcm_tx", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Dcm Tx")

#define CFG_HE_DCM_RX CFG_UINT( \
				"he_dcm_rx", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Dcm Rx")

#define CFG_HE_MU_PPDU CFG_BOOL( \
				"he_mu_ppdu", \
				0, \
				"He Mu Ppdu")

#define CFG_HE_SU_BEAMFORMER CFG_BOOL( \
				"he_su_beamformer", \
				0, \
				"He Su Beamformer")

#define CFG_HE_SU_BEAMFORMEE CFG_BOOL( \
				"he_su_beamformee", \
				0, \
				"He Su Beamformee")

#define CFG_HE_MU_BEAMFORMER CFG_BOOL( \
				"he_mu_beamformer", \
				0, \
				"He Mu Beamformer")

#define CFG_HE_BFEE_STS_LT80 CFG_UINT( \
				"he_bfee_sts_lt80", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Mu Bfee Sts Lt80")

#define CFG_HE_BFEE_STS_GT80 CFG_UINT( \
				"he_bfee_sts_lt80", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Mu Bfee Sts Gt80")

#define CFG_HE_NUM_SOUND_LT80 CFG_UINT( \
				"he_num_sound_lt80", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Num Sound Lt80")

#define CFG_HE_NUM_SOUND_GT80 CFG_UINT( \
				"he_num_sound_gt80", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Num Sound Gt80")

#define CFG_HE_SU_FEED_TONE16 CFG_BOOL( \
				"he_su_feed_tone16", \
				0, \
				"He Su Feed Tone16")

#define CFG_HE_MU_FEED_TONE16 CFG_BOOL( \
				"he_mu_feed_tone16", \
				0, \
				"He Mu Feed Tone16")

#define CFG_HE_CODEBOOK_SU CFG_BOOL( \
				"he_codebook_su", \
				0, \
				"He Codebook Su")

#define CFG_HE_CODEBOOK_MU CFG_BOOL( \
				"he_codebook_mu", \
				0, \
				"He Codebook Mu")

#define CFG_HE_BFRM_FEED CFG_UINT( \
				"he_bfrm_feed", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Bfrm Feed")

#define CFG_HE_ER_SU_PPDU CFG_BOOL( \
				"he_bfrm_feed", \
				0, \
				"He Er Su Ppdu")

#define CFG_HE_DL_PART_BW CFG_BOOL( \
				"he_dl_part_bw", \
				0, \
				"He Dl Part Bw")

#define CFG_HE_PPET_PRESENT CFG_BOOL( \
				"he_ppet_present", \
				0, \
				"He Pper Present")

#define CFG_HE_SRP CFG_BOOL( \
				"he_srp", \
				0, \
				"He Srp")

#define CFG_HE_POWER_BOOST CFG_BOOL( \
				"he_power_boost", \
				0, \
				"He Power Boost")

#define CFG_HE_4x_LTF_GI CFG_BOOL( \
				"he_4x_ltf_gi", \
				0, \
				"He 4x Ltf Gi")

#define CFG_HE_MAX_NC CFG_UINT( \
				"he_max_nc", \
				0, \
				7, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Max Nc")

#define CFG_HE_RX_STBC_GT80 CFG_BOOL( \
				"he_rx_stbc_gt80", \
				0, \
				"He Rx Stbc Gt80")

#define CFG_HE_TX_STBC_GT80 CFG_BOOL( \
				"he_Tx_stbc_gt80", \
				0, \
				"He Tx Stbc Gt80")

#define CFG_HE_ER_4x_LTF_GI CFG_BOOL( \
				"he_er_4x_ltf_gi", \
				0, \
				"He Er 4x Ltf Gi")

#define CFG_HE_PPDU_20_IN_40MHZ_2G CFG_BOOL( \
				"he_ppdu_20_in_40mhz_2g", \
				0, \
				"He Ppdu 20 In 40Mhz 2g")

#define CFG_HE_PPDU_20_IN_160_80P80MHZ CFG_BOOL( \
				"he_ppdu_20_in_160_80p80mhz", \
				0, \
				"He Ppdu 20 In 160 80p80mhz")

#define CFG_HE_PPDU_80_IN_160_80P80MHZ CFG_BOOL( \
				"he_ppdu_80_in_160_80p80mhz", \
				0, \
				"He Ppdu 80 In 160 80p80mhz")

#define CFG_HE_ER_1X_HE_LTF_GI CFG_BOOL( \
				"he_er_1x_he_ltf_gi", \
				0, \
				"He Er 1x He Ltf Gi")

#define CFG_HE_MIDAMBLE_TXRX_1X_HE_LTF CFG_BOOL( \
				"he_midamble_txrx_1x_he_ltf", \
				0, \
				"He Midamble Tx Rx 1x He Ltf")

#define CFG_HE_DCM_MAX_BW CFG_UINT( \
				"he_dcm_max_bw", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Dcm Max Bw")

#define CFG_HE_LONGER_16_SIGB_OFDM_SYM CFG_BOOL( \
				"he_longer_16_sigb_ofdm_sys", \
				0, \
				"He Longer 16 Sigb Ofdm Sys")

#define CFG_HE_NON_TRIG_CQI_FEEDBACK CFG_BOOL( \
				"he_rx_mcs_map_lt_80", \
				0, \
				"He Non Trig Cqi Feedback")

#define CFG_HE_TX_1024_QAM_LT_242_RU CFG_BOOL( \
				"he_tx_1024_qam_lt_242_ru", \
				0, \
				"He Tx 1024 Qam Lt 242 Ru")

#define CFG_HE_RX_1024_QAM_LT_242_RU CFG_BOOL( \
				"he_rx_1024_qam_lt_242_ru", \
				0, \
				"He Rx 1024 Qam Lt 242 Ru")

#define CFG_HE_RX_FULL_BW_MU_CMPR_SIGB CFG_BOOL( \
				"he_rx_full_bw_cmpr_sigb", \
				0, \
				"He Rx Full Bw Mu Cmpr Sigb")

#define CFG_HE_RX_FULL_BW_MU_NON_CMPR_SIGB CFG_BOOL( \
				"he_rx_full_bw_mu_non_cmpr_sigb", \
				0, \
				"He Rx Full Bw Mu Non Cmpr Sigb")

/* 11AX related INI configuration */
/*
 * <ini>
 * he_rx_mcs_map_lt_80 - configure Rx HE-MCS Map for ≤ 80 MHz
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0xFFFA
 *
 * This ini is used to configure Rx HE-MCS Map for ≤ 80 MHz
 * 0:1 Max HE-MCS For 1 SS
 * 2:3 Max HE-MCS For 2 SS
 * 4:5 Max HE-MCS For 3 SS
 * 6:7 Max HE-MCS For 4 SS
 * 8:9 Max HE-MCS For 5 SS
 * 10:11 Max HE-MCS For 6 SS
 * 12:13 Max HE-MCS For 7 SS
 * 14:15 Max HE-MCS For 8 SS
 *
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_RX_MCS_MAP_LT_80 CFG_INI_UINT( \
				"he_rx_mcs_map_lt_80", \
				0, \
				0xFFFF, \
				0xFFFA, \
				CFG_VALUE_OR_DEFAULT, \
				"He Rx Mcs Map Lt 80")

/* 11AX related INI configuration */
/*
 * <ini>
 * he_tx_mcs_map_lt_80 - configure Tx HE-MCS Map for ≤ 80 MHz
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0xFFFA
 *
 * This ini is used to configure Tx HE-MCS Map for ≤ 80 MHz
 * 0:1 Max HE-MCS For 1 SS
 * 2:3 Max HE-MCS For 2 SS
 * 4:5 Max HE-MCS For 3 SS
 * 6:7 Max HE-MCS For 4 SS
 * 8:9 Max HE-MCS For 5 SS
 * 10:11 Max HE-MCS For 6 SS
 * 12:13 Max HE-MCS For 7 SS
 * 14:15 Max HE-MCS For 8 SS
 *
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_TX_MCS_MAP_LT_80 CFG_INI_UINT( \
				"he_tx_mcs_map_lt_80", \
				0, \
				0xFFFF, \
				0xFFFA, \
				CFG_VALUE_OR_DEFAULT, \
				"He Tx Mcs Map Lt 80")
/* 11AX related INI configuration */
/*
 * <ini>
 * he_rx_mcs_map_160 - configure Rx HE-MCS Map for 160 MHz
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0xFFFA
 *
 * This ini is used to configure Rx HE-MCS Map for 160 MHz
 * 0:1 Max HE-MCS For 1 SS
 * 2:3 Max HE-MCS For 2 SS
 * 4:5 Max HE-MCS For 3 SS
 * 6:7 Max HE-MCS For 4 SS
 * 8:9 Max HE-MCS For 5 SS
 * 10:11 Max HE-MCS For 6 SS
 * 12:13 Max HE-MCS For 7 SS
 * 14:15 Max HE-MCS For 8 SS
 *
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_RX_MCS_MAP_160 CFG_INI_UINT( \
				"he_rx_mcs_map_160", \
				0, \
				0xFFFF, \
				0xFFFA, \
				CFG_VALUE_OR_DEFAULT, \
				"He Rx Mcs Map 160")

/* 11AX related INI configuration */
/*
 * <ini>
 * he_tx_mcs_map_160 - configure Tx HE-MCS Map for 160 MHz
 * @Min: 0
 * @Max: 0xFFFF
 * @Default: 0xFFFA
 *
 * This ini is used to configure Tx HE-MCS Map for 160 MHz
 * 0:1 Max HE-MCS For 1 SS
 * 2:3 Max HE-MCS For 2 SS
 * 4:5 Max HE-MCS For 3 SS
 * 6:7 Max HE-MCS For 4 SS
 * 8:9 Max HE-MCS For 5 SS
 * 10:11 Max HE-MCS For 6 SS
 * 12:13 Max HE-MCS For 7 SS
 * 14:15 Max HE-MCS For 8 SS
 *
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_TX_MCS_MAP_160 CFG_INI_UINT( \
				"he_tx_mcs_map_160", \
				0, \
				0xFFFF, \
				0xFFFA, \
				CFG_VALUE_OR_DEFAULT, \
				"He Tx Mcs Map 160")

#define CFG_HE_RX_MCS_MAP_80_80 CFG_UINT( \
				"he_rx_mcs_map_80_80", \
				0, \
				0xFFFF, \
				0xFFF0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Rx Mcs Map 80 80")

#define CFG_HE_TX_MCS_MAP_80_80 CFG_UINT( \
				"he_tx_mcs_map_80_80", \
				0, \
				0xFFFF, \
				0xFFF0, \
				CFG_VALUE_OR_DEFAULT, \
				"He tx Mcs Map 80 80")

#define CFG_HE_OPS_BASIC_MCS_NSS CFG_UINT( \
				"cfg_he_ops_basic_mcs_nss", \
				0x0000, \
				0xFFFF, \
				0xFFFC, \
				CFG_VALUE_OR_DEFAULT, \
				"He Ops Basic Mcs NSS")

/* 11AX related INI configuration */
/*
 * <ini>
 * he_ul_mumimo - configure ul mu capabilities
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini is used to configure capabilities of ul mu-mimo
 * 0-> no support
 * 1-> full bandwidth support
 * 2-> partial bandwidth support
 * 3-> full and partial bandwidth support
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_HE_UL_MUMIMO CFG_INI_UINT( \
				"he_ul_mumimo", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"He Ul Mumimo")

/* 11AX related INI configuration */
/*
 * <ini>
 * he_dynamic_frag_support - configure dynamic fragmentation
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini is used to configure dynamic fragmentation.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_HE_DYNAMIC_FRAGMENTATION CFG_INI_UINT( \
				"he_dynamic_frag_support", \
				0, \
				3, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"HE Dynamic Fragmentation")


/*
 * <ini>
 * enable_ul_mimo- Enable UL MIMO.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable UL MIMO.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_UL_MIMO CFG_INI_BOOL( \
				"enable_ul_mimo", \
				1, \
				"He Enble Ul Mimo Name")

/*
 * <ini>
 * enable_ul_ofdma- Enable UL OFDMA.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable UL OFDMA.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_UL_OFDMA CFG_INI_BOOL( \
				"enable_ul_ofdma", \
				1, \
				"He Enable Ul Ofdma Name")

/*
 * <ini>
 * he_sta_obsspd- 11AX HE OBSS PD bit field
 * @Min: 0
 * @Max: uin32_t max
 * @Default: 0x15b8c2ae
 *
 * 4 Byte value with each byte representing a signed value for following params:
 * Param                   Bit position    Default
 * OBSS_PD min (primary)   7:0             -82 (0xae)
 * OBSS_PD max (primary)   15:8            -62 (0xc2)
 * Secondary channel Ed    23:16           -72 (0xb8)
 * TX_PWR(ref)             31:24           21  (0x15)
 * This bit field value is directly applied to FW
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HE_STA_OBSSPD CFG_INI_UINT( \
				"he_sta_obsspd", \
				0, \
				0xFFFFFFFF, \
				0x15b8c2ae, \
				CFG_VALUE_OR_DEFAULT, \
				"He Mu Bfee Sts Gt80")

/*
 * <ini>
 * he_mcs_12_13_support - Bit mask to enable MCS 12 and 13 support
 * @Min: 0x0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * This ini is used to set MCS 12 and 13 for 2.4Ghz and 5Ghz. first 16
 * bits(0-15) is for 2.4ghz and next 16 bits is for 5Ghz. Of 16 bits the lower
 * 8 bits represent BW less than or equal 80Mhz (<= 80Mhz) and higher 8 bits
 * represent BW greater than 80Mhz (> 80Mhz). nth bit in octet represent support
 * for nth NSS [n=1:8]. Def value is 0xFFFFFFFF which enable MCS 12 and 13 for
 * all NSS and BW.
 *
 * Bits         Band
 * BIT[0:15]    2.4Ghz support for MCS 12 and 13, for NSS n[1:8] and BW <= 80Mhz
 *              first 8 bits should be used (0-7) and for NSS n[1:8] and BW >
 *              80 Mhz, next 8 bits (8-15) should be used.
 *
 * BIT[16:31]   5Ghz support for MCS 12 and 13, for NSS n[1:8] and BW < 80Mhz,
 *              bits 16-23 should be used and for BW > 80Mhz, next 8 bits
 *              (24-31)
 *
 * Some Possible values are as below
 * 0          - MCS 12 and 13 disabled for 2.4Ghz and 5Ghz for all nss and
 *              BW > 80Mz and <= 80Mhz
 * 0x3030303  - MCS 12 and 13 enabled for 2.4Ghz and 5Ghz for NSS 1 and 2 for
 *              BW > 80Mhz and <= 80Mhz
 * 0x0303     - MCS 12 and 13 enabled for 2.4Ghz NSS 1 and 2 for BW > 80Mhz and
 *              <= 80Mhz but disabled for 5Ghz
 * 0x3030000  - MCS 12 and 13 enabled for 5Ghz NSS 1 and 2 for BW > 80Mhz and
 *              <= 80Mhz but disabled for 2.4Ghz
 * 0x30000    - MCS 12 and 13 enabled for 5Ghz NSS 1 and 2 for BW <= 80Mhz and
 *              disabled for BW > 80Mhz. And disabled for 2.4Ghz
 * 0x3          MCS 12 and 13 enabled for 2.4Ghz NSS 1 and 2 for BW <= 80Mhz and
 *              disabled for all
 *
 * Related: None
 *
 * Supported Feature: HE MCS 12 and 13
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_HE_MCS_12_13_SUPPORT CFG_INI_UINT("he_mcs_12_13_support", \
				0, 0xffffffff, 0xffffffff, \
				CFG_VALUE_OR_DEFAULT, \
				"He Configure MCS_12_13 bits")

#define CFG_HE_CAPS_ALL \
	CFG(CFG_HE_CONTROL) \
	CFG(CFG_HE_FRAGMENTATION) \
	CFG(CFG_HE_MAX_FRAG_MSDU) \
	CFG(CFG_HE_MIN_FRAG_SIZE) \
	CFG(CFG_HE_TRIG_PAD) \
	CFG(CFG_HE_MTID_AGGR_RX) \
	CFG(CFG_HE_LINK_ADAPTATION) \
	CFG(CFG_HE_ALL_ACK) \
	CFG(CFG_HE_TRIGD_RSP_SCHEDULING) \
	CFG(CFG_HE_BUFFER_STATUS_RPT) \
	CFG(CFG_HE_BA_32BIT) \
	CFG(CFG_HE_MU_CASCADING) \
	CFG(CFG_HE_MULTI_TID) \
	CFG(CFG_HE_OMI) \
	CFG(CFG_HE_OFDMA_RA) \
	CFG(CFG_HE_MAX_AMPDU_LEN) \
	CFG(CFG_HE_AMSDU_FRAG) \
	CFG(CFG_HE_FLEX_TWT_SCHED) \
	CFG(CFG_HE_RX_CTRL) \
	CFG(CFG_HE_BSRP_AMPDU_AGGR) \
	CFG(CFG_HE_QTP) \
	CFG(CFG_HE_A_BQR) \
	CFG(CFG_HE_SR_RESPONDER) \
	CFG(CFG_HE_NDP_FEEDBACK_SUPP) \
	CFG(CFG_HE_OPS_SUPP) \
	CFG(CFG_HE_AMSDU_IN_AMPDU) \
	CFG(CFG_HE_CHAN_WIDTH) \
	CFG(CFG_HE_MTID_AGGR_TX) \
	CFG(CFG_HE_SUB_CH_SEL_TX) \
	CFG(CFG_HE_UL_2X996_RU) \
	CFG(CFG_HE_OM_CTRL_UL_MU_DIS_RX) \
	CFG(CFG_HE_RX_PREAM_PUNC) \
	CFG(CFG_HE_CLASS_OF_DEVICE) \
	CFG(CFG_HE_LDPC) \
	CFG(CFG_HE_LTF_PPDU) \
	CFG(CFG_HE_MIDAMBLE_RX_MAX_NSTS) \
	CFG(CFG_HE_LTF_NDP) \
	CFG(CFG_HE_TX_STBC_LT80) \
	CFG(CFG_HE_RX_STBC_LT80) \
	CFG(CFG_HE_DOPPLER) \
	CFG(CFG_HE_UL_MUMIMO) \
	CFG(CFG_HE_DCM_TX) \
	CFG(CFG_HE_DCM_RX) \
	CFG(CFG_HE_MU_PPDU) \
	CFG(CFG_HE_SU_BEAMFORMER) \
	CFG(CFG_HE_SU_BEAMFORMEE) \
	CFG(CFG_HE_MU_BEAMFORMER) \
	CFG(CFG_HE_BFEE_STS_LT80) \
	CFG(CFG_HE_BFEE_STS_GT80) \
	CFG(CFG_HE_NUM_SOUND_LT80) \
	CFG(CFG_HE_NUM_SOUND_GT80) \
	CFG(CFG_HE_SU_FEED_TONE16) \
	CFG(CFG_HE_MU_FEED_TONE16) \
	CFG(CFG_HE_CODEBOOK_SU) \
	CFG(CFG_HE_CODEBOOK_MU) \
	CFG(CFG_HE_BFRM_FEED) \
	CFG(CFG_HE_ER_SU_PPDU) \
	CFG(CFG_HE_DL_PART_BW) \
	CFG(CFG_HE_PPET_PRESENT) \
	CFG(CFG_HE_SRP) \
	CFG(CFG_HE_POWER_BOOST) \
	CFG(CFG_HE_4x_LTF_GI) \
	CFG(CFG_HE_MAX_NC) \
	CFG(CFG_HE_RX_STBC_GT80) \
	CFG(CFG_HE_TX_STBC_GT80) \
	CFG(CFG_HE_ER_4x_LTF_GI) \
	CFG(CFG_HE_PPDU_20_IN_40MHZ_2G) \
	CFG(CFG_HE_PPDU_20_IN_160_80P80MHZ) \
	CFG(CFG_HE_PPDU_80_IN_160_80P80MHZ) \
	CFG(CFG_HE_ER_1X_HE_LTF_GI) \
	CFG(CFG_HE_MIDAMBLE_TXRX_1X_HE_LTF) \
	CFG(CFG_HE_DCM_MAX_BW) \
	CFG(CFG_HE_LONGER_16_SIGB_OFDM_SYM) \
	CFG(CFG_HE_NON_TRIG_CQI_FEEDBACK) \
	CFG(CFG_HE_TX_1024_QAM_LT_242_RU) \
	CFG(CFG_HE_RX_1024_QAM_LT_242_RU) \
	CFG(CFG_HE_RX_FULL_BW_MU_CMPR_SIGB) \
	CFG(CFG_HE_RX_FULL_BW_MU_NON_CMPR_SIGB) \
	CFG(CFG_HE_RX_MCS_MAP_LT_80) \
	CFG(CFG_HE_TX_MCS_MAP_LT_80) \
	CFG(CFG_HE_RX_MCS_MAP_160) \
	CFG(CFG_HE_TX_MCS_MAP_160) \
	CFG(CFG_HE_RX_MCS_MAP_80_80) \
	CFG(CFG_HE_TX_MCS_MAP_80_80) \
	CFG(CFG_HE_OPS_BASIC_MCS_NSS) \
	CFG(CFG_HE_DYNAMIC_FRAGMENTATION) \
	CFG(CFG_ENABLE_UL_MIMO) \
	CFG(CFG_ENABLE_UL_OFDMA) \
	CFG(CFG_HE_STA_OBSSPD) \
	CFG(CFG_HE_MCS_12_13_SUPPORT)

#endif /* __CFG_MLME_HE_CAPS_H */

