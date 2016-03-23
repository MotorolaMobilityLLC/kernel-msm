/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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
 * DOC: nan_datapath.h
 *
 * MAC NAN Data path API specification
 */

#ifndef __MAC_NAN_DATAPATH_H
#define __MAC_NAN_DATAPATH_H

#include "sirCommon.h"
#include "aniGlobal.h"
#include "sirParams.h"

#ifdef WLAN_FEATURE_NAN_DATAPATH

/**
 * struct ndp_peer_node - structure for holding per-peer context
 * @next: pointer to the next peer
 * @peer_mac_addr: peer mac address
 * @ext_rates_present: extended rates supported
 * @edca_present: edca supported
 * @wme_edca_present: WME EDCA supported
 * @wme_info_present: WME info supported
 * @ht_capable: HT capable
 * @vht_capable: VHT capabale
 * @ht_sec_chan_offset: HT secondary channel offset
 * @capability_info: Generic capability info
 * @supported_rates: Supported rates
 * @extended_rates: Supported extended rates
 * @supported_mcs_rate: Supported MCS rates
 * @edca_params: EDCA parameters
 * @erp_ie_present: ERP IE supported
 * @ht_green_field: HT green field supported
 * @ht_shortGI_40Mhz; 40 MHZ short GI support
 * @ht_shortGI_20Mhz; 20 MHZ short GI support
 * @ht_mimo_ps_state: MIMO power state
 * @ht_ampdu_density: AMPDU density
 * @ht_max_rxampdu_factor: receieve AMPDU factor
 * @ht_max_amsdu_len: Max AMSDU lengh supported
 * @ht_supp_chan_widthset: Supported channel widthset
 * @ht_ldpc_capable: LDPC capable
 * @heartbeat_failure: heart beat failure indication flag
 * @vht_caps: VHT capability
 * @vht_supp_chanwidth_set: VHT supported channel width
 * @vht_beamformer_capable: Beam former capable
 */
struct ndp_peer_node {
	struct ndp_peer_node *next;
	v_MACADDR_t peer_mac_addr;
	uint8_t ext_rates_present;
	uint8_t edca_present;
	uint8_t wme_edca_present;
	uint8_t wme_info_present;
	uint8_t ht_capable;
	uint8_t vht_capable;
	uint8_t ht_sec_chan_offset;
	tSirMacCapabilityInfo    capability_info;
	tSirMacRateSet           supported_rates;
	tSirMacRateSet           extended_rates;
	uint8_t supported_mcs_rate[SIZE_OF_SUPPORTED_MCS_SET];
	tSirMacEdcaParamSetIE    edca_params;
	uint8_t erp_ie_present;
	uint8_t ht_green_field;
	uint8_t ht_shortGI_40Mhz;
	uint8_t ht_shortGI_20Mhz;
	/* MIMO Power Save */
	tSirMacHTMIMOPowerSaveState ht_mimo_ps_state;
	uint8_t ht_ampdu_density;
	/* Maximum Rx A-MPDU factor */
	uint8_t ht_max_rxampdu_factor;
	uint8_t ht_max_amsdu_len;
	uint8_t ht_supp_chan_widthset;
	uint8_t ht_ldpc_capable;
	uint8_t heartbeat_failure;

#ifdef WLAN_FEATURE_11AC
	tDot11fIEVHTCaps vht_caps;
	uint8_t vht_supp_chanwidth_set;
	uint8_t vht_beamformer_capable;
#endif
};

/* Function to process NDP requests */
VOS_STATUS lim_handle_ndp_request_message(tpAniSirGlobal mac_ctx,
					tpSirMsgQ msg);
/* Function to process NDP events */
VOS_STATUS lim_handle_ndp_event_message(tpAniSirGlobal mac_ctx,
					tpSirMsgQ msg);

void lim_process_ndi_mlm_add_bss_rsp(tpAniSirGlobal mac_ctx,
				     tpSirMsgQ lim_msg_q,
				     tpPESession session_entry);

/* Handler DEL BSS resp for NDI interface */
void lim_ndi_del_bss_rsp(tpAniSirGlobal  mac_ctx,
			void *msg, tpPESession session_entry);
#else
/* Function to process NDP requests */
static inline VOS_STATUS lim_handle_ndp_request_message(tpAniSirGlobal mac_ctx,
					tpSirMsgQ msg)
{
	return VOS_STATUS_SUCCESS;
}
/* Function to process NDP events */
static inline VOS_STATUS lim_handle_ndp_event_message(tpAniSirGlobal mac_ctx,
							tpSirMsgQ msg)
{
	return VOS_STATUS_SUCCESS;
}
/* Function to process NDP events */
static inline void lim_process_ndi_mlm_add_bss_rsp(tpAniSirGlobal mac_ctx,
						   tpSirMsgQ lim_msg_q,
						   tpPESession session_entry)
{
}
static inline void lim_ndi_del_bss_rsp(tpAniSirGlobal mac_ctx,
					void *msg, tpPESession session_entry)
{
}
#endif /* WLAN_FEATURE_NAN_DATAPATH */

#endif /* __MAC_NAN_DATAPATH_H */

